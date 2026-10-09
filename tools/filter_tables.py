#!/usr/bin/env python3
# SPDX-License-Identifier: MIT

"""Generate SoundFont and SFZ filter tables for src/filter.cpp.

usage: filter_tables.py <main>

Read a locally supplied WarioWare: Get It Together! executable, the `main` module of its ExeFS, an NSO file; the
executable is not distributed with hacsf2. Extract kLpfFreq (0x1afc90c), the low-pass's one-pole coefficients for a
48 kHz renderer (0x1afca2c), the five biquad preset tables the Wii U's library has (from 0x1af977c) and the five made
for 48 kHz (from 0x1afabae), all in Q14. The addresses are offsets into the loaded module.

The track low-pass (0xd8) is a one-pole filter. nn::atk gives it to the Switch's audio renderer as a biquad from a table
for the renderer's rate: y = b0 x + a1 y1, with b0 = 16384 - a1. The renderer runs that biquad on the voice at 48 kHz.
The biquad presets are Robert Bristow-Johnson cookbook filters: low/high-pass at about -0.5 dB gain and band-pass at
0 dB peak gain. Types 1 to 5 are the Wii U's tables for its 32 kHz renderer. The Switch's renderer runs them at 48 kHz.
Types 6 to 10 are made for 48 kHz. Each coefficient set reduces to a cutoff or centre frequency, Q and gain.

SFZ: sfizz's lpf_1p has pole exp(-2 pi f / rate); lpf_2p, hpf_2p and bpf_2p use cookbook filters with Q expressed in
dB. Match the game's coefficients at 48000 Hz.

SoundFont: FluidSynth supplies one cookbook low-pass (fluid_iir_filter.c), with Q = 10^((resonance / 10 - 3.01) / 20).
It matches the biquad low-pass. Approximate the one-pole by fitting cutoff at 44100 Hz against the game's response at
48000 Hz. Minimize RMS dB error relative to 20 Hz over 400 log-spaced frequencies from 20 Hz to 14 kHz. Floor both
responses at -30 dB.

Requires Python 3 and numpy.
"""
import argparse
import math
import struct
import sys
from pathlib import Path

try:
    import numpy as np
except ImportError:
    sys.exit("filter_tables.py requires numpy (pip install numpy)")

LPF_FREQ = 0x1afc90c  # kLpfFreq: 24 little-endian uint16 cutoffs in Hz. The last (16000) switches the filter off.
LOW_PASS_48K = 0x1afca2c  # the one-pole for each cutoff at 48 kHz: b0, b1, b2, a1, a2
BIQUADS = {  # type: (address, sets, name)
    1: (0x1af977c, 112, "low-pass"),
    2: (0x1af9bdc, 97, "high-pass"),
    3: (0x1af9fa6, 122, "band-pass 512"),
    4: (0x1afa46a, 93, "band-pass 1024"),
    5: (0x1afa80c, 93, "band-pass 2048"),
    6: (0x1afabae, 112, "low-pass made for 48 kHz"),
    7: (0x1afb00e, 97, "high-pass made for 48 kHz"),
    8: (0x1afb3d8, 122, "band-pass 512 made for 48 kHz"),
    9: (0x1afb89c, 93, "band-pass 1024 made for 48 kHz"),
    10: (0x1afbc3e, 93, "band-pass 2048 made for 48 kHz"),
}
GAME_RATE = 48000.0  # Hz, the Switch's audio renderer's rate
OUT_RATE = 44100.0  # Hz, the rate FluidSynth usually plays at
FREQS = np.geomspace(20.0, 14000.0, 400)
FLOOR_DB = -30.0
CUTOFFS = np.arange(1500, 13501, 10)  # the SoundFont cutoffs tried, in absolute cents
SFZ_LOW_PASS_BASE = 20000.0  # Hz: reference cutoff for the SFZ low-pass table (filter open)
SFZ_BIQUAD_BASE = 1000.0  # Hz: reference cutoff for the SFZ biquad table


def lz4_block(data, size):
    """Decompress an LZ4 block of `size` bytes."""
    out = bytearray()
    i = 0
    while i < len(data):
        token = data[i]
        i += 1
        length = token >> 4
        if length == 15:
            while True:
                byte = data[i]
                i += 1
                length += byte
                if byte != 255:
                    break
        out += data[i:i + length]
        i += length
        if i >= len(data):
            break
        offset = data[i] | data[i + 1] << 8
        i += 2
        length = token & 15
        if length == 15:
            while True:
                byte = data[i]
                i += 1
                length += byte
                if byte != 255:
                    break
        length += 4
        for _ in range(length):
            out.append(out[-offset])
    if len(out) != size:
        sys.exit(f"an LZ4 segment gave {len(out)} bytes, not {size}")
    return bytes(out)


def load_nso(path):
    """Like the loader, load an NSO's text, read-only data and data segments at their offsets in the module."""
    d = path.read_bytes()
    if d[:4] != b"NSO0":
        sys.exit(f"{path} isn't a Switch executable (NSO)")
    flags, = struct.unpack_from("<I", d, 0x0C)
    image = bytearray()
    for k, header in enumerate((0x10, 0x20, 0x30)):
        file_offset, memory_offset, size = struct.unpack_from("<III", d, header)
        stored, = struct.unpack_from("<I", d, 0x60 + 4 * k)
        data = d[file_offset:file_offset + stored]
        if flags >> k & 1:
            data = lz4_block(data, size)
        if len(image) < memory_offset + size:
            image += bytes(memory_offset + size - len(image))
        image[memory_offset:memory_offset + size] = data[:size]
    return bytes(image)


def read(image, fmt, address):
    if address + struct.calcsize(fmt) > len(image):
        sys.exit(f"address {address:#x} isn't in the executable")
    return struct.unpack_from(fmt, image, address)


def response_db(b, a, rate, freqs=FREQS):
    """Compute the response of y = b0 x + b1 x1 + b2 x2 - a1 y1 - a2 y2 at `rate`. Return dB relative to the
    first frequency, clamped to the floor."""
    z = np.exp(-2j * np.pi * freqs / rate)
    h = (b[0] + b[1] * z + b[2] * z * z) / (1.0 + a[0] * z + a[1] * z * z)
    db = 20.0 * np.log10(np.maximum(np.abs(h), 1e-12))
    return np.maximum(db - db[0], FLOOR_DB)


def fluidsynth_db(cents, resonance):
    """Compute FluidSynth filter coefficients for cutoff in absolute cents and resonance in cB."""
    fres = min(max(8.176 * 2.0 ** (cents / 1200.0), 5.0), 0.45 * OUT_RATE)
    q = 10.0 ** ((min(max(resonance / 10.0, 0.0), 96.0) - 3.01) / 20.0)
    w = 2.0 * np.pi * fres / OUT_RATE
    alpha = np.sin(w) / (2.0 * q)
    b1 = (1.0 - np.cos(w)) / (1.0 + alpha)
    return response_db((b1 / 2.0, b1, b1 / 2.0), (-2.0 * np.cos(w) / (1.0 + alpha), (1.0 - alpha) / (1.0 + alpha)),
                       OUT_RATE)


def best_cutoff(target):
    """Find the zero-resonance SoundFont cutoff closest to `target`. Return cutoff in absolute cents and RMS
    error in dB."""
    errors = [np.sqrt(np.mean((fluidsynth_db(c, 0) - target) ** 2)) for c in CUTOFFS]
    i = int(np.argmin(errors))
    return int(CUTOFFS[i]), float(errors[i])


def decode_biquad(kind, coefs):
    """Decode a game biquad's cutoff/centre (Hz at the renderer's rate), Q and gain (dB) from Q14 coefficients in y =
    b0 x + b1 x1 + b2 x2 + a1 y1 + a2 y2. Measure gain at cookbook unity: DC for low-pass, Nyquist for high-pass,
    centre for band-pass."""
    b0, b1, b2, a1, a2 = (v / 16384.0 for v in coefs)
    alpha = (1.0 + a2) / (1.0 - a2)  # a2 = -(1 - alpha) / (1 + alpha)
    cos_w0 = a1 * (1.0 + alpha) / 2.0  # a1 = 2 cos w0 / (1 + alpha)
    w0 = math.acos(max(-1.0, min(1.0, cos_w0)))
    q = math.sin(w0) / (2.0 * alpha)
    if kind == 1:
        z = 1.0
    elif kind == 2:
        z = -1.0
    else:
        z = complex(math.cos(w0), -math.sin(w0))
    gain = abs((b0 + b1 * z + b2 * z * z) / (1.0 - a1 * z - a2 * z * z))
    return w0 * GAME_RATE / (2.0 * math.pi), q, 20.0 * math.log10(gain)


def cents(ratio):
    return int(round(1200.0 * math.log2(ratio)))


def table(ctype, name, comment, values, per_line=10):
    """Format a C++ table with `per_line` values per line, followed by a blank line."""
    lines = [f"// {comment}", f"constexpr {ctype} {name}[{len(values)}] = {{"]
    for i in range(0, len(values), per_line):
        lines.append("    " + ", ".join(str(v) for v in values[i:i + per_line]) + ",")
    lines.append("};\n")
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser(description="Generate the filter tables for src/filter.cpp.")
    ap.add_argument("main", type=Path, help="WarioWare: Get It Together!'s exefs/main")
    args = ap.parse_args()
    image = load_nso(args.main)

    lpf_freq = read(image, "<24H", LPF_FREQ)
    if lpf_freq[0] != 80 or lpf_freq[-1] != 16000:
        sys.exit(f"{args.main} isn't WarioWare: Get It Together!'s main (kLpfFreq reads {lpf_freq[:3]}...)")

    # The track low-pass's pole, and the SoundFont cutoff fitted to it.
    low_pass_sf2, low_pass_sfz, sf2_errors = [], [], []
    for i in range(len(lpf_freq) - 1):
        b0, b1, b2, a1, a2 = read(image, "<5h", LOW_PASS_48K + 10 * i)
        if b1 or b2 or a2 or b0 + a1 != 16384:
            sys.exit(f"the low-pass at {lpf_freq[i]} Hz isn't a one-pole: {(b0, b1, b2, a1, a2)}")
        cutoff, error = best_cutoff(response_db((b0 / 16384.0, 0.0, 0.0), (-a1 / 16384.0, 0.0), GAME_RATE))
        low_pass_sf2.append(cutoff)
        sf2_errors.append(error)
        low_pass_sfz.append(cents(-GAME_RATE * math.log(a1 / 16384.0) / (2.0 * math.pi) / SFZ_LOW_PASS_BASE))

    print(table("uint16_t", "kLowPassCutoff", "The SoundFont cutoffs, kLpfFreq[0] to kLpfFreq[22].", low_pass_sf2))
    print(table("int16_t", "kSfzLowPassCutoff", "The SFZ cutoffs, kLpfFreq[0] to kLpfFreq[22].", low_pass_sfz))

    # The biquad presets: cutoff (cents from SFZ_BIQUAD_BASE), Q (0.01 dB) and gain (0.01 dB) of each step.
    for kind, (address, sets, name) in BIQUADS.items():
        shape = (kind - 1) % 5 + 1
        steps = [decode_biquad(shape, read(image, "<5h", address + 10 * i)) for i in range(sets)]
        values = []
        for freq, q, gain in steps:
            cutoff, resonance = cents(freq / SFZ_BIQUAD_BASE), round(2000.0 * math.log10(q))
            values.append(f"{{{cutoff}, {resonance}, {round(100.0 * gain)}}}")
        print(table("SfzBiquad", f"kSfzBiquad{kind}", f"Type {kind}, {name}: steps 0 to {sets - 1}.", values, 5))
        if shape == 1:
            sf2 = [f"{{{min(cents(freq / 8.176), 13500)}, {round(10.0 * (20.0 * math.log10(q) + 3.01))}}}"
                   for freq, q, _ in steps]
            name = "kBiquadLowPass" if kind == 1 else "kBiquadLowPass48k"
            print(table("SoundFontFilter", name, f"The SoundFont filters of type {kind}, steps 0 to 111.", sf2, 6))

    print(f"// RMS difference, SoundFont low-pass: {min(sf2_errors):.2f} to {max(sf2_errors):.2f} dB", file=sys.stderr)


if __name__ == "__main__":
    main()
