# hacsf2

hacsf2 converts Nintendo Switch sound archives (`.bfsar`) to MIDI, SoundFont 2 and SFZ.
Each sequence gets a MIDI file and a SoundFont containing its instruments. SFZ output includes
filters that SoundFont cannot reproduce (see [SFZ files](#sfz-files)).

Drop archives onto `hacsf2.exe` on Windows or `Hacsf2.app` on macOS. Output goes in a folder
beside each archive. From a terminal:

```
hacsf2 [options] <archive.bfsar> [<archive.bfsar> ...]

  --output DIR   write to DIR (one archive only)
  --only REGEX   convert matching sequence names (for example --only BGM)
  --loops N      repeat the main loop N times (default 1)
                 MIDI loop markers are included for players that support them
  --seed N       initial game PRNG state (default 0x12345678)
  --var N=V      set variable N (0-15 player, 16-31 global) to V before each
                 sequence starts; repeat for more variables
  --list         list sequences without converting
  --quiet        print errors only
  --version      show the version
```

Some games keep their archives inside other containers or give them other names. Unpack them
first: hacsf2 reads the archive itself, whatever its extension. Some games name their archives
`.fsst` and `.fspj`.

Some sequences play nothing until the game sets one of their variables. `--var` sets one before
each sequence starts. As in the game, player and global variables start at -1. Use `--only` to
choose the sequences that `--var` applies to.

Files use the sequence names, with adjustments for Windows: forbidden characters and trailing
dots or spaces become `_`, and reserved device names such as `CON` get an added `_`. If names
differ only in ASCII letter case, later sequences get a sound-index suffix to avoid collisions
on Windows and macOS. The same applies to a sequence named `samples`. That name would otherwise
collide with the SFZ sample folder. The default output folder is `<archive name>_hacsf2`, and
the same rules apply to its name. Use `--output` to choose another location.

hacsf2 is the Switch counterpart of cafesf2 (Wii U) and citrusf2 (3DS). Switch games use
nn::atk, the successor of the Wii U's sound library, nw::snd. hacsf2 plays sequences as
cafesf2 does. The Switch's timing, audio renderer and arithmetic come from the executable of
WarioWare: Get It Together!.

## Downloads

The Releases page has hacsf2 for 64-bit Windows. On macOS and Linux, build it from source
(see [Building](#building)).

## Drag and drop

- **Windows:** drop one or more `.bfsar` files onto `hacsf2.exe`. The console shows the results
  and waits for Enter before closing.
- **macOS:** drop archives onto `Hacsf2.app`, or open it to choose files. The app shows the
  results and can open the output folders. The app wraps the command-line program because Finder
  cannot drop files onto a bare executable. The [build](#building) includes the app.

hacsf2 reports an unreadable archive and goes on with the others. If two inputs would use the
same output folder, hacsf2 skips the second to protect the first input's files. This can happen
when names differ only in extension, or only in case on a filesystem that ignores case. Windows
and macOS filesystems usually do.

## The conversion

### Sequence playback

FSEQ is bytecode, with variables, conditions, random arguments, calls, jumps, loops and tracks
that start other tracks. hacsf2 executes the bytecode and records the resulting notes and
controllers. The performer follows the Switch's nn::atk player, including:

- All commands and their variable, comparison, random and `if` forms: calls and loops nested
  ten deep, main and second volumes that ramp like track volume, three effect sends, and four
  LFOs per track with curves, phases and periods. In a Switch archive, the sequences' 16-bit
  and 24-bit arguments are little-endian.
- Note lengths, damper, key groups, tie/mono, `notewait off` and waits for sounding notes to
  finish. Note ends are calculated frame by frame. nn::atk works in 5 ms sound frames. The
  Switch's audio renderer plays a voice from the frame its note starts in. A one-shot wave ends
  when the renderer has played all of it, at the pitch that the note's sweep, the track's bend
  and the pitch LFOs give. nn::atk works that pitch out again each frame. The renderer reports a
  wave's end two frames late. nn::atk frees the note's channel the frame after that. Envelopes
  start at -90.4 dB and first advance at frame end. Notes released or stopped before then are
  inaudible and produce no MIDI note. A note whose length runs out before then sounds for that
  frame. The note's release starts in the next frame.
- Tick scheduling in 1/65536 ms, in single precision. The game's rounding occasionally
  places a tick in an adjacent sound frame.
- The game's random generator, including its once-per-frame advance. Random choices therefore
  depend on timing.
- Main-loop detection and MIDI loop markers. The main loop belongs to the lowest-numbered track
  that can still advance. A pass qualifies only if earlier tracks make no contribution. Tracks
  waiting forever on held notes do not block this choice. A pass that changes nothing, such as
  one that only polls or counts a variable, qualifies only while no other track changes the
  sound or plays on. Setting a variable that another track reads counts as a change. A track
  that has sounded nothing and changed nothing for 10 seconds doesn't count as playing on. A
  sequence whose tracks all reach their end has no main loop. Backward jumps to unvisited
  commands and passes without waits, such as random retries or variable polling, are not loops.
  Inside a subroutine, a jump back isn't a loop when its pass played no note that can be heard
  and ran a conditional jump, call, return or end that could leave the loop. A tie or mono
  legato on a note below hearing (see below) plays nothing that can be heard. A held note that
  sounds until the game stops it keeps such a loop. A main loop in which no note starts isn't a
  loop either once every earlier note has ended or is fading out. The sequence then ends with
  its last sound. The markers go on the first pass that a player looping on them repeats as the
  game plays the next one: the same notes at the same times, with the same settings and tempo.
  The first pass often differs. Setup before the loop can land in its first tick. The game can
  keep a value from pass to pass. Later passes may only tie again a note that the first pass
  strikes. If no pass so far repeats that way but the next pass does, the performance plays that
  pass too. Otherwise the markers stay on the first pass. They also stay there in a loop that
  draws random numbers. At the loop's start, the MIDI file sends each controller, pitch, preset
  and tempo that the game gives back there when the loop ends with another value. Without those
  events, a player that loops on the markers would keep the value from the loop's end.

Short loops, such as repeating sound effects, run until the MIDI is at least 10 seconds long.
Indefinitely held sounds are capped at 10 seconds after their last change. As in the WSF
project, a note of 9,600 ticks or more that its track waits out counts as held. Games use such
notes, minutes long at a typical tempo, for sounds that they stop themselves. The cap also
covers a main loop longer than 10 seconds that ends with more than 10 seconds of no change and
sounds only endless notes held with tie or mono. The game's next pass usually continues those
notes. A sequence that finishes naturally ends with its last sound, including release. A note
counts as below hearing once its envelope has decayed 40 dB. A note that plays until its track
releases it, such as a tie, ends where its envelope has decayed 60 dB. If its sustain level
stays above that, the note ends at 40 dB down. The game's channel goes on in silence.
Performances stop after 15 minutes. One that has been silent for more than 10 seconds by then
ends with its last sound too. Tempo or timebase 0 stops sequence timing while notes keep
playing. Those notes finish normally, or take the same 10-second limit if held indefinitely.

### MIDI

Each sequence track gets a MIDI track and channel of its own. Channel 10, reserved for drums in
General MIDI, is allocated last. SoundFont synths use bank 128 for that channel. hacsf2 copies
that channel's presets to bank 128.

MIDI output keeps tempo changes and uses at least 7680 ticks per quarter note, with an integer
number of MIDI ticks per sequence tick. The game processes ticks in 5 ms sound frames. The
renderer plays their changes in the same frame. hacsf2 places events at the start of the frame
that contains each tick, rounded to the nearest MIDI tick. At 120 BPM, the rounding error is at
most 1/30 ms. No moved event crosses a loop marker. A loop keeps its contents. A sweep or a
portamento with a portamento time moves on in every frame, between ticks too. hacsf2 writes a
pitch bend at the start of each of those frames. After its track closes, the sweep goes on while
the note's release sounds. Sweeps also go on after the sequence's last tick, at the last tempo.

nn::atk applies track settings to notes at frame end. A note starts with the settings that its
tick ends with. MIDI controller changes are written before the notes of the same tick.

| Game setting | MIDI output |
|---|---|
| Track volume, up to 127 | CC7 |
| Track volume above 127 × second volume × main volume × sound volume × dry sends, plus legato velocity changes | CC11. Like nn::atk's volumes, CC7 and CC11 both act squared. Values are scaled for sequences exceeding MIDI's range (see [Differences from the game](#differences-from-the-game)). |
| Main send and effect sends B and C, which reach the mix dry | Part of CC11 (see below) |
| Effect send A | CC91 (reverb) |
| Track pan | CC10, mapped to the SoundFont pan law |
| Pitch bend, sweep, portamento and legato key changes | Pitch bend, with RPN 0 range set per channel |
| First LFO's depth | CC1 = depth / 2, mapped to vibrato or tremolo in the SoundFont |
| Immediate track silence | CC120 |
| Filters that change during sounding notes | CC102, mapped to SoundFont cutoff by a modulator |
| Track low-pass (0xd8), for SFZ | CC103 = 64 minus the argument; values <= 6 mean off |
| Biquad filter (0xb4/0xb5), for SFZ | CC104 = filter value; 0 means off |
| Instrument and track envelope, LFO, initial pan and filter settings | Bank select (CC0) and program change |

A voice sends its sound to a main bus and to three effect buses, A to C. The main send and the
effect sends set how much goes to each. On the Switch, every bus reaches the output. A bus that
the game puts no effect on reaches it dry, in phase with the main bus. The main send and effect
sends B and C add up to the note's level, and CC11 holds them. Effect send A goes to the synth's
reverb. Games usually put a reverb on that bus.

A MIDI channel can hold only one note per key. Repeated strikes that overlap, common in drums,
get extra channels called layers, with matching controllers, up to the 16-channel limit.

A MIDI channel has one set of controllers for all its notes. In the game, each note keeps its
settings. While notes that the game plays differently from the track's later notes still sound,
the track moves on to a free channel when there is one. That covers release tails that the track
has detached. Those keep their settings. The same goes for notes still sounding when a new note
starts that sweeps, starts at a raised velocity or requires a level of its own. Each channel
gets a MIDI track named after the sequence track, in the same way as a layer.

### SoundFont

Each distinct combination of bank instrument and track settings becomes a preset. Instrument
regions become zones with:

- Key and velocity ranges resolved by nn::atk's first-match rules.
- The region's sample, root key, tuning, linear volume and pan.
- Envelope parameters after track overrides. nn::atk advances an envelope 5 ms in each 5 ms
  sound frame. nn::atk's attack is exponential in decibels. The SoundFont attack is linear, and
  fits the game's attack best at 0.55 of its time. A note that the game releases during its
  attack gets a longer attack, in a preset of its own. That attack reaches the game's level at
  the release. The release then starts from there. SoundFont decays and releases fall 100 dB in
  101.6 s at the slowest. The game's slowest decays and releases take up to six times as long.
  Such a decay gets a longer hold, up to SoundFont's limit of 18 s: half the time by which the
  game's decay reaches its sustain level later. The note is then as far above the game's level
  at the end of the hold as below it when its decay reaches the sustain level. Such a release
  plays at SoundFont's slowest release time. The SFZ files keep the game's times.
- Key groups mapped to exclusive classes.
- Loops repeated as needed to meet SoundFont padding rules: 8 samples before, at least 32
  within, and 8 after. This preserves playback.
- Continued playback for regions that ignore note-off. Their MIDI notes end when the game
  would release them, with release settings that also preserve decay after a repeated strike.

hacsf2 reads Switch FSAR archives and their FSEQ, FBNK, FWAR and FWAV files. They're the Wii U's
formats in little-endian byte order. Supported wave encodings are PCM8, PCM16 and DSP-ADPCM.
Banks and wave archives stored only in FGRP groups are also supported. Groups can have wave
archives of their own. Copies of a bank may then reference different files. hacsf2 uses the
first group's copy. The copies play the same waves.

Some games keep groups outside the archive, in files that the game loads itself. hacsf2 looks
for each such group as `<group name>.bfgrp` beside the archive, or in a `userManagementFiles`
folder beside it. When hacsf2 finds none of them, a warning says so. Archives from other
consoles are refused: the 3DS's `.bcsar`, the Wii's `.brsar` and the Wii U's `.bfsar`. The
Wii U's archives are big-endian, and the Switch's archives are little-endian.

## SFZ files

SoundFont has one two-pole low-pass filter per voice. That filter cannot reproduce the game's
high-pass or band-pass presets, and only approximates the one-pole track low-pass. SFZ supports
all of these filters. hacsf2 also writes these SFZ files:

- `sfz/<sequence>/Track <n>.sfz`: instruments for MIDI `Track <n>` and its layers, for each
  active sequence track.
- `sfz/samples/`: 16-bit WAV samples shared by all sequences in the archive. Stereo channels
  are separate files, panned left and right.

SFZ players ignore MIDI channels, and each file contains one instrument. Load the MIDI in a DAW
and assign an SFZ player, such as sfizz, to each track with its matching file. Output targets
sfizz 1.2.

Presets use program change, plus bank select (CC0) when a sequence has more than 128 presets.
Regions match the SoundFont, with these differences:

- **Filters.** sfizz's `lpf_1p` matches the track low-pass. `lpf_2p`, `hpf_2p` and `bpf_2p`
  match the biquad presets coefficient for coefficient at the renderer's 48 kHz. The Switch has
  two sets of presets. Types 1 to 5 are the Wii U's presets, made for its 32 kHz output. At
  48 kHz their cutoffs are 1.5 times as high. Types 6 to 10 are the same presets made for
  48 kHz. When both filters are active, they run in series. CC103/CC104 and SFZ curves map track
  values to cutoff, resonance and gain, including changes during notes. An inactive low-pass
  stays at sfizz's highest cutoff, 20 kHz. There it lowers 10 kHz by about 0.5 dB and 16 kHz by
  1 dB. SFZ has no biquad bypass like the game's value 0 or absent preset. Instead, CC104
  selects filtered or unfiltered regions at note-on, and that choice lasts for the note. As in
  the game, the choice is the one that the note's sound frame ends with. Each track uses its
  first biquad type. Later types are omitted and reported.
- **Envelopes.** Decays and releases in sfizz fall linearly in dB, like those in the game, by
  78.17 dB over their given time. The SFZ files give them the game's time for that fall. Attack
  uses the SoundFont fit described above.
- **LFO.** SFZ uses a sine, the game's default curve. SFZ also supports pan LFO.
- **Pan.** A curve maps CC10 to `(value - 64) / 64`. That's the SoundFont convention that the
  MIDI output uses.
- **Level.** SFZ region `volume` restores any gain lost when MIDI CC11 is scaled. Playback keeps
  the game's level.
- **Controllers.** CC7, CC10 and CC11 act at once, without sfizz's default 10 ms smoothing. A
  note whose track sets the volume or pan in the note's first tick starts at that level and pan.
- **Loops.** sfizz crossfades the loop end with data before the loop start over at least 1 ms.
  Samples include 4 ms of repeated loop data before the start. The crossfade is then inaudible.

Two sfizz 1.2.3 quirks affect playback:

- **Sample rate.** Loading a file configures its filters for 48 kHz. A later change of sample
  rate corrects them. At other rates, load files before setting the rate. `sfizz_render` and
  `sfizz_jack` set the rate first. `sfizz_render` uses 48 kHz unless given another rate.
- **Streaming.** During offline rendering, streamed samples can fall silent after their
  preloaded part. The files load samples in full with `hint_ram_based=1`. sfizz prints
  "Unsupported value for hint_stealing: 1" for this. The message is harmless.

## Differences from the game

hacsf2 reports approximations for each sequence. Some behavior cannot be represented exactly
in MIDI, SoundFont or SFZ:

- **Per-note pitch.** Game sweeps, portamento and legato key changes affect individual notes.
  MIDI pitch bend affects the whole channel. A note that sweeps beside another sounding note
  goes on another channel (see "MIDI").
- **Legato.** Tie/mono normally continue the existing note. Pitch bend gives the key changes,
  and CC11 the velocity changes. A note that legatos take above its velocity starts at the
  highest velocity they reach. CC11 lowers it until then. That works as long as the instrument
  plays the same region at that velocity. Past that, CC11 goes above 127 (see "Volume limits").
  Beside another note's release, such a note goes on another channel (see "MIDI"). CC11 would
  lower that release too. When no channel is left, the rising legato requires a new MIDI note. A
  changed envelope/LFO preset requires a new MIDI note too. The new note restarts the sample and
  envelope. The game would continue both. The game keeps the volume of the region that a tied
  note started on. When the new MIDI note plays a region with another volume, CC11 makes up the
  difference.
- **Detached notes.** Toggling tie/mono, muting with release or restarting a track detaches its
  sounding notes. They finish releasing with their old settings. In MIDI, they keep those
  settings on a channel of their own (see "MIDI"). When no channel is left, they stay on the
  track's channel and receive its later changes. Changes affecting a detached note above -40 dB
  are reported.
- **Combined pan.** nn::atk adds region pan, track initial pan and track pan before applying
  its pan law. Synths receive track pan separately through CC10 and combine it with zone pan
  using the synth's pan law. For example, a region at +0.38 on a hard-left track is 6.3 dB
  left in the game but 14 dB left in FluidSynth.
- **Pan curves and modes.** hacsf2 always uses the default curve and dual mode. Other curves
  and balance mode are reported. Balance mode pans stereo channels together.
- **LFOs.** Only a track's first LFO is converted. The converted LFO is a sine that starts at
  the beginning of its cycle. The other three LFOs, and other curves (triangle, saw, square and
  random) and phases, are reported. A note keeps the speed, range, type and delay that the track
  has at the end of the note-on's tick. The game gives the note those settings then. The game
  also updates them during playback. A note that plays its LFO with later settings is reported.
  Depth changes work through CC1.
- **Deep tremolo.** In the game, a volume LFO can lower a note as far as silence but can raise
  it by no more than 6 dB. The SoundFont and SFZ LFOs swing as far up as down. A volume LFO
  deeper than 6 dB is kept to a 6 dB swing and reported. Such a tremolo dips less than in the
  game.
- **SoundFont filters.** The two-pole low-pass closely matches the biquad low-pass. For the
  one-pole track filter, the two-pole low-pass passes slightly more just above cutoff and less
  in the top octave. The two-pole filter's slope is twice as steep. Cutoffs are fitted by
  `tools/filter_tables.py`. High-pass and band-pass are omitted. Two active low-passes use the
  lower cutoff. SFZ has all these filters.
- **Effects.** The synth's reverb stands in for the game's effect on bus A. Each game chooses
  its effects and can change them with the scene. A sequence's mix depends on where the game
  plays it. When the game puts no effect on bus B or C, a send there reaches the mix dry. CC11
  gives that. In a game that puts an effect there, hacsf2 plays the send dry. Surround pan is
  omitted. Pan LFO is omitted from SoundFont but included in SFZ.
- **Volume limits.** Archive sound volume, track volume, second/main volume and region volume
  can reach 255, and the sends that reach the mix dry add to them. MIDI CC7 holds track volume
  up to 127. CC11 holds the rest, also up to 127. If necessary, all CC11 values are scaled
  equally. That keeps the balance between tracks and lowers the sequence's level. SoundFont also
  cannot amplify a sample: if any used region exceeds volume 127, every zone is attenuated
  equally to preserve region balance. hacsf2 reports the reduction. SFZ restores the MIDI gain
  and keeps each region's original volume.
- **The renderer's output.** Interpolation and output limiting in synths differ from those of
  the Switch's audio renderer. The renderer resamples each voice to 48 kHz with one of three
  filters. The filter depends on how fast the voice plays its wave. The filters take off up to
  about 3 dB at the top of a wave's range. A voice that reads its wave faster than 48,000
  samples a second loses up to about 2 dB at 12 kHz and 8 dB at 24 kHz. A synth keeps those
  highs.
- **Voice limits.** nn::atk has 96 voices unless a game sets another number. Synths usually
  have more. If a track starts over 96 notes in one tick, hacsf2 keeps the first 96. The game
  keeps the newest 96. After 96 legatos in a tick, the MIDI note continues with its existing
  preset and no gain above its starting level. Legatos that would otherwise require a new note
  are reported. When the game runs out of voices, it cuts off the notes of lowest priority,
  usually release tails. The synth plays them out. In busy music, and in a game that sets fewer
  voices, that happens often.

SoundFont output follows the specification and targets FluidSynth. Player differences remain:

- **Pan.** Pans are mapped from nn::atk's square-root law to FluidSynth's sin/cos law.
  TinySoundFont uses a square-root law. That gives a slightly narrower result.
- **Envelopes.** Times follow the specification's 100 dB range. FluidSynth uses 96 dB. Decays
  and releases there fall 4% slower in decibels, and sustain levels are 4% shallower: -11.9 dB
  plays as -11.4 dB. FluidSynth also slows any release faster than 15.6 ms (-7200 timecents) to
  that time.
- **Tremolo.** In FluidSynth, a volume LFO can't raise a note above full level. On a note that
  sustains at full level, a tremolo only dips. In the game, a tremolo swings as far up as down.
- **Attenuation.** Like E-mu hardware, FluidSynth applies 0.4 of a zone's initial attenuation.
  The specification applies all of it. A region at volume 64 therefore plays 2.4 dB below one
  at 127 in FluidSynth, rather than 6 dB. Attenuation for regions above 127 is reduced the same
  way.
- **Low-pass level.** In FluidSynth, a note through the SoundFont's version of the biquad
  low-pass plays 0.85 dB below an unfiltered one. The game's filter lowers it by 0.5 dB.
- **Pitch.** FluidSynth rounds pitch down to whole cents. Fractional bends can be almost a cent
  flat. A long note's loop can then drift by up to 35 ms per minute against the game.
- **Key groups.** The game releases interrupted group notes at rate 126. They fade out within
  40 ms. FluidSynth uses -200 timecents for exclusive-class cuts, about 0.9 s over the full
  envelope range.
- **Modulators.** TinySoundFont doesn't support SoundFont modulators. CC1 LFO depth and CC102
  filter changes have no effect there.
- **Tempo changes.** FluidSynth rounds its position to the nearest MIDI tick at each change and
  restarts its clock there. This can advance playback by half a tick per change: 1/30 ms at
  120 BPM with 7680 ticks per quarter note. TinySoundFont rounds tempo-change times down to
  milliseconds. Playback then advances by about half a millisecond per change on average.
- **Note timing.** FluidSynth applies MIDI events at the start of each 64-sample render block.
  Notes can start up to 64 samples late, about 1.5 ms at 44.1 kHz.
- **Short notes.** FluidSynth holds every note for at least 10 ms (`synth.min-note-length`).
  Shorter notes therefore sound longer than in the game.

## Building

Requires C++20 and CMake 3.20 or later, with no other build dependencies. CI builds and tests
Windows/MSVC x64, macOS/Apple Clang on Apple Silicon and Intel, and Linux/GCC and Clang x64.

- **Windows:** install Visual Studio 2022 or later with "Desktop development with C++",
  including CMake. Use Developer PowerShell or Developer Command Prompt.
- **macOS:** install Xcode command line tools (`xcode-select --install`) and CMake
  (`brew install cmake` or the installer from cmake.org).
- **Linux:** install GCC or Clang and CMake. On Debian/Ubuntu:
  `sudo apt install build-essential cmake`.

From the repository root:

```sh
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release
```

The executable is `build/hacsf2`, or `build/Release/hacsf2.exe` with Visual Studio. Xcode and
other multi-configuration generators also use `build/Release`. MSVC links the runtime
statically. The executable requires no runtime DLLs. CI uploads the Windows executable as an
artifact. macOS builds also produce `build/Hacsf2.app`. The app holds a copy of the program.
You can move the app elsewhere.

C++ formatting uses clang-format with the repository's `.clang-format`.

## Tests and tools

`ctest` runs unit tests without game data. Coverage includes:

- DSP-ADPCM and PCM16 decoding, including nn::atk's two-channel limit and files in either
  byte order.
- Truncated files, missing blocks, oversized wave/region counts, invalid bank/wave references,
  names containing control characters, line breaks or invalid UTF-8, and other consoles'
  archives.
- Group file lookup, malformed groups/tables, partial files, fallback to complete copies,
  groups missing from truncated archives, and group files kept outside the archive. Archive
  truncation is distinguished from invalid file offsets in otherwise complete archives.
- Sequence start offsets and bank slots, little-endian arguments, loop detection, held loops,
  the time limit, tie/mono and legato gain scaling, unheard notes, one-shot completion, tick
  rounding, unusual arguments, stopped timing, variable polling, loops without waits, voice
  limits and approximation reports.
- Envelope conversion, pitch ratios, LFOs, pan, SoundFont/SFZ filters and loop unrolling.
- SoundFont/MIDI output checked by independent readers, SFZ/WAV output and file naming.

Other checks and tools:

- `-DHACSF2_TEST_ARCHIVE=path/to/archive.bfsar` adds the conversion of every sequence in that
  archive. Independent readers check each output file.
- `tools/check_sf2.py` checks SoundFont structure using Python alone.
- `tools/filter_tables.py` derives the SoundFont and SFZ filter tables in `src/filter.cpp`
  from a locally supplied WarioWare: Get It Together! executable, the `main` file of its
  ExeFS. Requires Python 3 and numpy.

These development tools require game files supplied separately. hacsf2 doesn't distribute those
files. Normal conversion requires only the input archive, and any group files beside it.

## Licence

[MIT](LICENSE). hacsf2 is built from cafesf2's and citrusf2's MIT-licensed code, and follows the
MIT-licensed 3SF project's model of nw::snd. Game data is not covered by this licence.
