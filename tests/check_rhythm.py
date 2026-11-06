# SPDX-License-Identifier: MIT

# Checks that hacsf2 converts a rhythm piece of Super Mario Party Jamboree as the game plays it (src/rhythm_pieces.h).
#
#     check_rhythm.py <hacsf2> <folder> <archive> <label> <minimum notes> <controller archive> <controller label>
#
# <folder> holds the piece's archive and the archive of the mode's controller. Beside the controller, the piece's MIDI
# file holds the piece's notes and the controller's kick drum (key 36) on every beat, from the first beat to the file's
# end. An Eden run of the mode's first stage shows the kick under the stage's first piece. Alone, in a folder without
# the controller's archive, the piece converts with the values that the controller gives it. Its MIDI file then has the
# piece's notes and no kick. With --var 28=-1, the value that the variable holds before the controller sets it, the
# piece alone plays nothing.

import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path


def read_varlen(data, pos):
    value = 0
    while True:
        byte = data[pos]
        pos += 1
        value = (value << 7) | (byte & 0x7F)
        if not byte & 0x80:
            return value, pos


def read_midi(path):
    """The division of a standard MIDI file, and each track's name and note-ons with a velocity above 0, as (tick,
    key)."""
    data = path.read_bytes()
    division = struct.unpack(">H", data[12:14])[0]
    tracks = []
    pos = 14
    while pos + 8 <= len(data):
        size = struct.unpack(">I", data[pos + 4:pos + 8])[0]
        if data[pos:pos + 4] == b"MTrk":
            tracks.append(read_track(data[pos + 8:pos + 8 + size]))
        pos += 8 + size
    return division, tracks


def read_track(track):
    name = ""
    notes = []
    tick = 0
    pos = 0
    status = 0
    while pos < len(track):
        delta, pos = read_varlen(track, pos)
        tick += delta
        if track[pos] & 0x80:
            status = track[pos]
            pos += 1
        if status == 0xFF:
            kind = track[pos]
            length, pos = read_varlen(track, pos + 1)
            if kind == 0x03:
                name = track[pos:pos + length].decode("utf-8", "replace")
            pos += length
        elif status in (0xF0, 0xF7):
            length, pos = read_varlen(track, pos)
            pos += length
        else:
            kind = status & 0xF0
            if kind == 0x90 and track[pos + 1] > 0:
                notes.append((tick, track[pos]))
            pos += 1 if kind in (0xC0, 0xD0) else 2
    return name, notes, tick


def convert(hacsf2, archive, label, folder, extra):
    subprocess.run([hacsf2, "--quiet", "--output", str(folder), "--only", "^" + label + "$", *extra, str(archive)],
                   check=True)
    return read_midi(folder / (label + ".mid"))


def piece_notes(tracks, controller):
    return sum(len(notes) for name, notes, _ in tracks if not name.startswith(controller))


def main():
    hacsf2, folder, archive, label = sys.argv[1], Path(sys.argv[2]), sys.argv[3], sys.argv[4]
    minimum, controller_archive, controller = int(sys.argv[5]), sys.argv[6], sys.argv[7]
    failures = []
    with tempfile.TemporaryDirectory() as temp:
        division, tracks = convert(hacsf2, folder / archive, label, Path(temp) / "beside", [])
        notes = piece_notes(tracks, controller)
        kicks = [tick for name, track_notes, _ in tracks if name.startswith(controller) for tick, key in track_notes
                 if key == 36]
        end = max(last for _, _, last in tracks)
        beats = [beat * division for beat in range((end + division - 1) // division)]
        print(f"{label} beside {controller}: {notes} notes of the piece, {len(kicks)} kicks on {len(beats)} beats")
        if notes < minimum:
            failures.append(f"expected {minimum} notes of the piece or more beside the controller")
        if sorted(kicks) != beats:
            failures.append(f"expected a kick on every beat, at {beats[:8]}..., got {sorted(kicks)[:8]}...")

        alone_folder = Path(temp) / "alone"
        alone_folder.mkdir()
        shutil.copy(folder / archive, alone_folder / archive)
        _, tracks = convert(hacsf2, alone_folder / archive, label, Path(temp) / "given", [])
        given = piece_notes(tracks, controller)
        alone = sum(1 for name, _, _ in tracks if name.startswith(controller))
        _, tracks = convert(hacsf2, alone_folder / archive, label, Path(temp) / "waiting", ["--var", "28=-1"])
        waiting = piece_notes(tracks, controller)
        print(f"{label} alone: {given} notes with the game's values, {waiting} without")
        if given < minimum or waiting != 0 or alone != 0:
            failures.append(f"expected {minimum} notes or more alone with the game's values, none without, and no "
                            f"track of {controller}")
        if not (folder / controller_archive).is_file():
            failures.append(f"{controller_archive} is missing from {folder}")

    for failure in failures:
        print(failure)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
