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
  --sheets DIR   read the levels of a Bezel Engine game's sounds from the sound
                 sheets in DIR (default: a Data/SheetBin folder near the archive)
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

A game on Bezel Engine, such as WarioWare: Get It Together! or WarioWare: Move It!, gives its
sounds levels in its sound sheets (`Data/SheetBin/SoundVoice_*.elf`), outside the sound archive.
A piece's level there runs from 0.3 to 2.37. Unpack the game's `.bea` archives into folders beside
each other. hacsf2 then finds the sheets by itself: it looks for `Data/SheetBin` in the archive's
folder, in the three folders above it, and in the folders directly below each of those. `--sheets`
names the folder when it lies elsewhere. Without sheets, each sequence plays at the archive's
level.

Some of WarioWare: Get It Together!'s microgames turn tracks of their music up and down as the
microgame goes on: the guitar, the three spotlight dances, the windmill and its success music,
the dance, and Super Wario Co's results. hacsf2 finds the game's `Sound_Common.bfsar` by its
size and SHA-1. Each of those pieces converts in the state that the game starts it in. Each
later state gets a conversion too, named after the piece and the state, such as
`BGM_P_Guitar 60 percent`. `--list` shows them. A track that the state turns down plays at
CC11 0 from the start. Its notes stay in the MIDI file.

Each of the game's microgames also puts a reverb on effect bus B as it starts. The reverb stays
there until the stage ends. Before a stage's first microgame, bus B is empty. The microgames'
sequences that send to bus B convert with their microgame's reverb there. The game's other
sequences that send to bus B, such as the stages' music, convert with bus B empty. Each of them
also gets a conversion with each reverb, such as `BGM_T_Sport_SpeedUp Hall reverb`. WarioWare:
Move It! keeps a reverb on bus B for every sequence. hacsf2 finds its `Sound.bfsar` the same
way. A send to a bus with a reverb goes to the synth's reverb (CC91), beside effect send A.
Move It! keeps a delay on bus A, and there CC91 holds the reverb's send alone. CC11 holds only
the reverb's dry part: a quarter of the send for `I3DL2_Default`, and none for `I3DL2_Hall` or
Move It!'s `I3DL2_Waltz`.

Clubhouse Games: 51 Worldwide Classics puts the sound library's reverb on bus B in two of its
games, the piano and bowling. hacsf2 converts their sounds with that reverb. Their sends to bus
B go to CC91 as well, and none of them reaches the mix dry.

Super Mario Party Jamboree's rhythm mode has 134 pieces that wait for values from the game.
hacsf2 recognizes the mode's sub-archives (`subarc_rc_cmn.fsst`, the rhythm stage's archive and
`subarc_mg1801.fsst` to `subarc_mg1810.fsst`). It converts each piece with the tempo and the
number that the game gives it. `--var` overrides them. In the game, the mode's controller,
`SQ_BGM_RC_MAIN_RHYTHM` in `subarc_rc_cmn.fsst`, plays beside each piece. It plays a kick drum on
each beat under 59 of the pieces. hacsf2 converts each piece together with the controller into one
MIDI file. The two exchange the game's variables as they do in the game. The controller's tracks
come after the piece's, and a track of the controller without a note during the piece is left out.
The controller's samples go into the same SoundFont, with names that start with `c`. A piece keeps
its loop only where the controller repeats with it. When `subarc_rc_cmn.fsst` isn't beside the
archive, or in a folder of its name beside the archive's folder, the pieces convert without the
controller, with a warning.

Super Mario Party's Sound Stage has 129 pieces of the same design. hacsf2 recognizes the
stage's sub-archives (`subarc_bdcn001.fsst`, `subarc_common_a.fsst` and `subarc_mgcn401.fsst`
to `subarc_mgcn410.fsst`). It converts each piece with the tempo and the number that the game
gives it. In the game, the stage's controller, `SQ_BGM_BDCN001_MAIN_RHYTHM` in
`subarc_common_a.fsst`, plays a kick drum on each beat under 67 of the pieces. hacsf2 converts
each piece together with the controller, as for Jamboree, when it finds `subarc_common_a.fsst`.

Jamboree's star melody, `SQ_BGM_STAR_MELODY` in `subarc_bgm_bd01_star_melody.fsst`, follows the
beat and the chords of the board's music. The game plays it over one of 15 streams: the main
streams of the seven boards, with and without the last stretch of the game, and board 5's sale.
Each stream file holds markers of its beats and chords, and the stream's jump setting gives the
order of its parts. hacsf2 converts the melody once for each stream whose file and jump setting
lie beside the archive, under the names that the game gives them, such as
`SM_BGM_BD01_MAIN_JMP.dspadpcm.bfstm` (from the game's `stream` folder) and
`SM_BGM_BD01_MAIN_JMP.msgpack` (from `audio/jump_setting/conv` in `audio.nx.bea`). The
conversion's name is the melody's, a space and the stream's, such as
`SQ_BGM_STAR_MELODY SM_BGM_BD01_MAIN_JMP`. It plays the melody over one pass of the stream's
parts and the loop once more, starting with the stream. The stream itself isn't part of the
conversion. Without the stream files, the melody converts as a sequence that plays nothing.

Jamboree's and Mario Party Superstars' sounds pick one of their variants at random. Such a
sound calls a user procedure: code of the game that draws a number and gives it to the
sequence. The sound then plays the variant with that number. hacsf2 recognizes every sound
archive of the two games, and runs the game's procedures as the game's code does. Each
sequence starts from the state that the game's code gives the procedures as the game starts.
A conversion therefore plays the same variant every time. In the game, the variant changes
from play to play. One of Jamboree's procedures reads a table from the game's files,
`audio/userproc/conv/00.msgpack` in `audio.nx.bea`. hacsf2 finds it beside the archive, or in
the game's folders when the archive is in `audio/sounddata`. Without the table, the sounds
that read it play as if the game had loaded none.

Some sound effects play only once the game has written one of their variables as it starts
them. Others play only on some draws of the game's random numbers. hacsf2 starts such an effect
in a state that the game can give it. For Super Mario 3D World + Bowser's Fury's `SeData.bfsar`
and some of Super Mario Party Jamboree's archives, hacsf2 holds the values that the games' code
writes. It tries each of them until the effect plays a note above volume 0. A `--var` value
replaces the value for its variable. An effect that draws a number and plays nothing from the
default seed converts from a later start. hacsf2 steps the generator once a frame, as the game
does, for 1 to 400 frames, and takes the first start that plays such a note. `--seed` turns
that search off.

Files use the sequence names, with adjustments for Windows: forbidden characters and trailing
dots or spaces become `_`, and reserved device names such as `CON` get an added `_`. If names
differ only in ASCII letter case, later sequences get a sound-index suffix to avoid collisions
on Windows and macOS. The same applies to a sequence named `samples`. That name would otherwise
collide with the SFZ sample folder. The default output folder is `<archive name>_hacsf2`, and
the same rules apply to its name. Use `--output` to choose another location.

Switch games use nn::atk, the successor of the Wii U's sound library, nw::snd. hacsf2 plays
each sequence the way nn::atk does. The Switch's timing, audio renderer and arithmetic come
from the executable of WarioWare: Get It Together!.

## Downloads

The Releases page has hacsf2 for 64-bit Windows. On macOS on Apple Silicon, build it from
source (see [Building](#building)).

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
  that wait forever, on held notes or for a negative number of ticks, do not block this choice.
  A pass that changes nothing, such as one that only polls or counts a variable, qualifies only
  while no other track changes the sound or plays on. Setting a variable that another track
  reads counts as a change. A tie or mono legato on a note below hearing (see below) changes
  nothing that can be heard. A track that has sounded nothing and changed nothing for 10 seconds
  doesn't count as playing on. A sequence whose tracks all reach their end has no main loop.
  Backward jumps to unvisited commands and passes without waits, such as random retries or
  variable polling, are not loops. Inside a subroutine, a jump back isn't a loop when its pass
  played no note that can be heard and ran a conditional jump, call, return or end that could
  leave the loop. A tie or mono legato on a note below hearing (see below) plays nothing that
  can be heard. A held note that sounds until the game stops it keeps such a loop. A loop that a
  track runs inside a pass of another loop, such as a countdown of waits between notes, is an
  inner loop. An inner loop's passes don't qualify until the track has played no pass of another
  loop for 10 seconds. A main loop in which no note starts isn't a loop either once every
  earlier note has ended or is fading out. The sequence then ends with its last sound. The
  markers go on the first pass that a player looping on them repeats as the game plays the next
  one: the same notes at the same times, with the same settings and tempo. The first pass often
  differs. Setup before the loop can land in its first tick. The game can keep a value from pass
  to pass. Later passes may only tie again a note that the first pass strikes. If no pass so far
  repeats that way but the next pass does, the performance plays that pass too. Otherwise the
  markers stay on the first pass. They also stay there in a loop that draws random numbers. At
  the loop's start, the MIDI file sends each controller, pitch, preset and tempo that the game
  gives back there when the loop ends with another value. Without those events, a player that
  loops on the markers would keep the value from the loop's end.

Short loops, such as repeating sound effects, run until the MIDI is at least 10 seconds long.
Indefinitely held sounds are capped at 10 seconds after their last change. A note of 9,600
ticks or more that its track waits out counts as held. Games use such
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
pitch bend at the start of each of those frames. After its track closes, the sweep goes on until
the note's release has decayed 40 dB. Sweeps also go on after the sequence's last tick, at the
last tempo.

nn::atk applies track settings to notes at frame end. A note starts with the settings that its
tick ends with. MIDI controller changes are written before the notes of the same tick.

| Game setting | MIDI output |
|---|---|
| Track volume, up to 127 | CC7 |
| Track volume above 127 × second volume × main volume × sound volume × dry sends, plus legato velocity changes | CC11. Like nn::atk's volumes, CC7 and CC11 both act squared. Values are scaled for sequences exceeding MIDI's range (see [Differences from the game](#differences-from-the-game)). |
| Main send and effect sends B and C, which reach the mix dry | Part of CC11 (see below) |
| Effect send A, and the sends to a bus with a reverb (see above) | CC91 (reverb) |
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
starts that sweeps, starts at a raised velocity or requires a level of its own. A note whose
pitch LFO changes while it sounds plays on a free channel as well. Its preset there has no LFO.
Pitch bends follow the game's LFO frame by frame instead. Each channel gets a MIDI track named
after the sequence track, in the same way as a layer.

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
  within, and 32 after. This preserves playback.
- Continued playback for regions that ignore note-off. Their MIDI notes end when the game
  would release them, with release settings that also preserve decay after a repeated strike.

A SoundFont holds at most 65,535 zones, generators and modulators. Each preset holds every zone
of its bank instrument. A sequence with many presets of large instruments can pass that limit.
Its SoundFont then holds only the zones that the sequence's notes pick, and hacsf2 says so. A
synth picks zones by a note's key and velocity as the note starts. The zones left out never sound.
The SFZ files keep every zone.

hacsf2 reads Switch FSAR archives and their FSEQ, FBNK, FWAR and FWAV files. They're the Wii U's
formats in little-endian byte order. Supported wave encodings are PCM8, PCM16 and DSP-ADPCM.
Banks and wave archives stored only in FGRP groups are also supported. Groups can have wave
archives of their own. Copies of a bank may then reference different files. hacsf2 uses the
first group's copy. The copies play the same waves.

Some games keep groups outside the archive, in files that the game loads itself. hacsf2 looks
for each such group as `<group name>.bfgrp` beside the archive, or in a `userManagementFiles`
folder beside it. A game can also keep these group files in SARC archives under other names.
While a group is still missing, hacsf2 reads the group files in the `.sarc` files beside the
archive too. A folder can hold another archive's group files as well. hacsf2 takes a group file
from a SARC only if its files are of the kinds that the archive gives them. When hacsf2 finds
none of the groups, a warning says so. Archives from other consoles are refused: the 3DS's
`.bcsar`, the Wii's `.brsar` and the Wii U's `.bfsar`. The Wii U's archives are big-endian, and
the Switch's archives are little-endian.

## SFZ files

SoundFont has one two-pole low-pass filter per voice. That filter cannot reproduce the game's
high-pass or band-pass presets, and only approximates the one-pole track low-pass. SFZ supports
all of these filters. hacsf2 also writes these SFZ files:

- `sfz/<sequence>/Track <n>.sfz`: instruments for MIDI `Track <n>` and its layers, for each
  active sequence track.
- `sfz/samples/`: 16-bit WAV samples shared by all sequences in the archive. Stereo channels
  are separate files, panned left and right.

SFZ players ignore MIDI channels, and each file contains one instrument. Load the MIDI in a DAW
and assign an SFZ player, such as sfizz, to each track with its matching file. The player tested
is sfizz 1.2.

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
- **Envelopes.** As in the game, decays and releases fall linearly in dB. The documentation's
  curve falls by a factor of e^-8 over the specified time. That's 69.49 dB. The SFZ files give
  them the game's time for that fall. Attack uses the SoundFont fit described above.
- **LFO.** SFZ uses a sine, the game's default curve. SFZ also supports pan LFO.
- **Pan.** A curve maps CC10 to `(value - 64) / 64`. That's the SoundFont convention that the
  MIDI output uses.
- **Level.** SFZ region `volume` restores any gain lost when MIDI CC11 is scaled. Playback keeps
  the game's level.
- **Controllers.** CC7, CC10 and CC11 act at once, without sfizz's default 10 ms smoothing. A
  note whose track sets the volume or pan in the note's first tick starts at that level and pan.
- **Loops.** sfizz crossfades the loop end with data before the loop start over at least 1 ms.
  Samples include 4 ms of repeated loop data before the start. The crossfade is then inaudible.
- **Key groups.** A note of a key group ends the group's other notes with the game's fade at
  release rate 126 (`off_mode=time` and `off_time`). A SoundFont's exclusive class can't set
  that fade.

Six sfizz 1.2.3 quirks affect playback:

- **Decays and releases.** sfizz ends them sooner than the documentation's curve. Over the
  specified time, sfizz falls by a factor of e^-9: 78.17 dB. Where the game has fallen 40 dB,
  sfizz has fallen 45 dB.
- **Sample rate.** Loading a file configures its filters for 48 kHz. A later change of sample
  rate corrects them. At other rates, load files before setting the rate. `sfizz_render` and
  `sfizz_jack` set the rate first. `sfizz_render` uses 48 kHz unless given another rate.
- **Streaming.** During offline rendering, streamed samples can fall silent after their
  preloaded part. The files load samples in full with `hint_ram_based=1`. sfizz prints
  "Unsupported value for hint_stealing: 1" for this. The message is harmless.
- **Mono samples.** sfizz pans a mono sample by the sin/cos law and then raises both sides by
  3 dB. Every sample of the files is mono. Every region therefore plays 3.0 dB above the game's
  level, at every pan. The balance of the mix stays the same. Lowering sfizz's volume by 3 dB
  gives the game's level.
- **Repeated keys.** A key group in sfizz never cuts a note of the same key as the new note. The
  documentation's `off_by` cuts it. A key of a group struck again therefore leaves the earlier
  note playing in sfizz. The game fades that note out at release rate 126.
- **Short decays and releases.** sfizz makes each decay and release last at least 6 ms. The
  documentation sets no such floor. A shorter time plays over 6 ms.

The opcodes follow their documentation at sfzformat.com, with these exceptions:

- **Long decays and releases.** A decay or release can last longer than the documented limit of
  100 s. The game's envelope falls that slowly. sfizz 1.2 plays the time as written.
- **Loud regions.** A region's `volume` can lie above the documented limit of 6 dB. sfizz 1.2
  takes volumes up to 48 dB.
- **Low resonances.** The game's biquad presets have resonances down to -46 dB. The documented
  range starts at 0 dB. Each biquad track's curve reaches resonances below 0 dB. sfizz 1.2 plays
  those resonances.
- **A wide resonance amount.** Biquad type 5 has steps down to -46 dB. A track of that type
  moves its resonance in units of 50 dB. The documentation gives the amount a range of -40 to
  40 dB.
- **High cutoffs.** A region pitched far up can get a low-pass cutoff above half the output's
  sample rate. The documentation gives half the rate as the limit.

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
  also updates them during playback. When the pitch LFO's settings change during a note, the
  note plays on a free channel with pitch bends that follow the game's LFO (see "MIDI"). Those
  bends follow the LFO's curve and phase too. With a random curve, the bends can differ from
  those in the game. Such a curve is reported. A volume or pan LFO that changes during a note
  is reported. So is a pitch LFO that changes when no channel is free. Depth changes work
  through CC1.
- **Deep tremolo.** In the game, a volume LFO can lower a note as far as silence but can raise
  it by no more than 6 dB. The SoundFont and SFZ LFOs swing as far up as down. A volume LFO
  deeper than 6 dB is kept to a 6 dB swing and reported. Such a tremolo dips less than in the
  game.
- **SoundFont filters.** The two-pole low-pass closely matches the biquad low-pass. For the
  one-pole track filter, the two-pole low-pass passes slightly more just above cutoff and less
  in the top octave. The two-pole filter's slope is twice as steep. Cutoffs are fitted by
  ntdlib's `tools/sequence_export/filter_tables.py`. High-pass and band-pass are omitted. Two
  active low-passes use the lower cutoff. SFZ has all these filters.
- **Effects.** The synth's reverb stands in for the game's effect on bus A. Each game chooses
  its effects and can change them with the scene. A sequence's mix depends on where the game
  plays it. When the game puts no effect on bus B or C, a send there reaches the mix dry. CC11
  gives that. For the reverbs that WarioWare: Get It Together!, WarioWare: Move It! and
  Clubhouse Games: 51 Worldwide Classics put on bus B, the send goes to the synth's reverb, and
  CC11 holds the reverb's dry part. For any other effect there, hacsf2 plays the send dry.
  Surround pan is omitted. Pan LFO is omitted from SoundFont but included in SFZ.
- **Volume limits.** Archive sound volume, track volume, second/main volume and region volume
  can reach 255, and the sends that reach the mix dry add to them. MIDI CC7 holds track volume
  up to 127. CC11 holds the rest, also up to 127. If necessary, all CC11 values are scaled
  equally. That keeps the balance between tracks and lowers the sequence's level. SoundFont also
  cannot amplify a sample: if any used region exceeds volume 127, every zone is attenuated
  equally to preserve region balance. hacsf2 reports the reduction. SFZ restores the MIDI gain
  and keeps each region's original volume.
- **A sound's level.** The game chooses a sound's record in its sheets as it plays. A piece that
  the game plays through records with other names can play at several levels. hacsf2 takes the
  record named after the sound. Without one, it takes the level that most of the records give,
  and the higher one on a tie. A sound without a record plays at the archive's level. The game
  can also add a random change to the level for each play. hacsf2 takes the middle of its range.
- **The renderer's output.** Interpolation and output limiting in synths differ from those of
  the Switch's audio renderer. The renderer resamples each voice to 48 kHz with one of three
  filters. The filter depends on how fast the voice plays its wave. The filters take off up to
  about 3 dB at the top of a wave's range. A voice that reads its wave faster than 48,000
  samples a second loses up to about 2 dB at 12 kHz and 8 dB at 24 kHz. hacsf2 stands in for
  that loss with a low-pass on the zones and regions that play their waves that fast. Zones
  split where a key reads its wave at 48,000 and 64,000 samples a second. The renderer changes
  filters at those rates. The cutoff follows the key. It's a fixed fraction of the rate at
  which the key reads the wave. The fractions are fitted to FluidSynth's low-pass at 44.1 kHz
  and to sfizz's `lpf_1p` at 48 kHz. A note keeps the low-pass of its key. A bend, sweep or
  LFO that takes the note past one of those rates doesn't change it. A SoundFont preset whose
  filter changes during notes (CC102) gets no such low-pass. hacsf2 reports its notes that read
  their waves that fast. In the SFZ files, the low-pass takes a filter that the track's filters
  leave free. A track that uses both its low-pass and its biquad leaves none while the biquad
  is on. Its regions then play without this low-pass. In a synth, slower voices keep the highs
  that the renderer takes off.
- **Pitch limits.** nn::atk plays a voice at 1/16 to 16 times the rate of its wave. A note that
  asks for a pitch past a limit plays at the limit in the game. A note whose key starts past a
  limit and whose pitch holds still gets zones and regions that play at the limit too. Every key
  past a limit then plays at one pitch, with a scale tuning of 0 in the SoundFont and
  `pitch_keytrack=0` in the SFZ files. Those keys take no resampling low-pass. A note that a bend,
  a sweep or an LFO moves plays every key at its pitch. In the game, such a note stays at the
  limit for as long as its pitch would pass it. In the files, it plays past the limit then.
- **Voice limits.** nn::atk has 96 voices unless a game sets another number. Synths usually
  have more. If a track starts over 96 notes in one tick, the game keeps the newest 96. hacsf2
  keeps the first 1,024 and reports any that it leaves out. After 96 legatos in a tick, the MIDI
  note continues with its existing preset and no gain above its starting level. Legatos that
  would otherwise require a new note are reported. When the game runs out of voices, it cuts off
  the notes of lowest priority, usually release tails. The synth plays them out. In busy music,
  and in a game that sets fewer voices, that happens often.

SoundFont output follows the specification, apart from the zones' attenuation and the biquad
low-pass's resonance. Player differences remain:

- **Level.** FluidSynth runs the SoundFont low-pass filter on every note. That includes notes
  whose filter is open. The filter divides the level by the square root of its Q. FluidSynth
  converts the default resonance of 0 dB to a Q of 0.707. Every note therefore plays 1.5 dB
  louder than the SoundFont specifies. Multiplying FluidSynth's gain by 0.84 gives the specified
  level.
- **Pan.** Pans are mapped from nn::atk's square-root law to FluidSynth's sin/cos law.
  TinySoundFont uses a square-root law. That gives a slightly narrower result.
- **Envelopes.** Times follow the specification's 100 dB range. FluidSynth uses 96 dB. Decays
  and releases there fall 4% slower in decibels, and sustain levels are 4% shallower: -11.9 dB
  plays as -11.4 dB. FluidSynth also slows any release faster than 15.6 ms (-7200 timecents) to
  that time.
- **Tremolo.** In FluidSynth before 2.4.0, a volume LFO can't raise a note above full level. On
  a note that sustains at full level, a tremolo only dips. In the game, a tremolo swings as far
  up as down.
- **Attenuation.** FluidSynth, BASSMIDI and the E-mu chips apply 0.4 of a zone's initial
  attenuation. The specification calls for all of it. hacsf2 therefore writes 2.5 times a zone's
  attenuation in the generator, up to the generator's limit of 1440 cB. Those players play that
  limit as 57.6 dB. A modulator without a source holds any attenuation beyond 57.6 dB.
  FluidSynth 2.4.5 and later apply that modulator. BASSMIDI and earlier versions of FluidSynth
  ignore it. A player that follows the specification plays every attenuated zone too quietly.
  TinySoundFont plays a quarter of the attenuation. Before 27 January 2025, TinySoundFont played
  2.5 times the attenuation.
- **Low-pass level.** In FluidSynth, a note through the SoundFont's version of the biquad
  low-pass plays 0.85 dB below an unfiltered one. The game's filter lowers it by 0.5 dB.
- **Resonance.** FluidSynth, BASSMIDI and the E-mu chips play a resonance of r dB as r - 3 dB at
  the cutoff. The specification calls for r dB. hacsf2 writes the biquad low-pass's resonance
  for those players. A player that follows the specification plays 3 dB more at the cutoff.
- **Pitch.** FluidSynth rounds pitch down to whole cents. Fractional bends can be almost a cent
  flat. A long note's loop can then drift by up to 35 ms per minute against the game.
- **Key groups.** The game releases interrupted group notes at rate 126. They fade out within
  40 ms. FluidSynth 2.4.3 and later cut such notes with -2000 timecents, about 0.3 s over the
  full envelope range. Earlier versions use -200 timecents, about 0.9 s. BASSMIDI ends them in
  about 4 ms.
- **Modulators.** BASSMIDI 2.4.16 ignores the CC1 and CC102 modulators and the modulator without
  a source (see "Attenuation"). CC1 then gives the specification's default vibrato in every
  zone: 50 cents at CC1 127. A tremolo becomes that vibrato. Filters that change during a note
  stay open. TinySoundFont doesn't support SoundFont modulators. CC1 LFO depth and CC102 filter
  changes have no effect there.
- **Tempo changes.** FluidSynth rounds its position to the nearest MIDI tick at each change and
  restarts its clock there. This can advance playback by half a tick per change: 1/30 ms at
  120 BPM with 7680 ticks per quarter note. TinySoundFont rounds tempo-change times down to
  milliseconds. Playback then advances by about half a millisecond per change on average.
- **Note timing.** FluidSynth applies MIDI events at the start of each 64-sample render block.
  Notes can start up to 64 samples late, about 1.5 ms at 44.1 kHz.
- **Short notes.** FluidSynth holds every note for at least 10 ms (`synth.min-note-length`).
  Shorter notes therefore sound longer than in the game.

## Building

Requires C++20 and CMake 3.20 or later. hacsf2 supports macOS on Apple Silicon and Windows x64
with clang-cl. CI builds both. It runs the tests on Windows.

- **Windows:** install Visual Studio 2022 with "Desktop development with C++", including CMake.
  Add the "C++ Clang tools for Windows" component. Use Developer PowerShell or Developer Command
  Prompt.
- **macOS:** install Xcode command line tools (`xcode-select --install`) and CMake
  (`brew install cmake` or the installer from cmake.org).

hacsf2 builds on three libraries: seqlib, xsflib and ntdlib. The repository holds them as Git
submodules in `externals/`. Clone it with its submodules:

```sh
git clone --recursive <repository URL>
```

In a clone made without `--recursive`, run `git submodule update --init` first.

From the repository root:

```sh
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release
```

On Windows, the first command is `cmake -S . -B build -A x64 -T ClangCL`. It selects clang-cl
and 64-bit output.

The executable is `build/hacsf2`, or `build/Release/hacsf2.exe` with Visual Studio. Xcode and
other multi-configuration generators also use `build/Release`. The Windows build links the
runtime statically. The executable requires no runtime DLLs. CI uploads the Windows executable as an
artifact. macOS builds also produce `build/Hacsf2.app`. The app holds a copy of the program.
You can move the app elsewhere.

C++ formatting uses clang-format with the repository's `.clang-format`.

## Tests and tools

`ctest` checks that the program reports its version and rejects a variable that the game
doesn't have. The conversion's unit tests are in the seqlib and ntdlib libraries. They run on
synthetic files, without game data.

Other checks and tools:

- `-DHACSF2_TEST_ARCHIVE=path/to/archive.bfsar` adds the conversion of every sequence in that
  archive. seqlib's readers check each MIDI file and SoundFont. ntdlib checks the SFZ files.
- seqlib's `seqlib_check` program checks the structure of SoundFont and MIDI files. Configure
  with `-DSEQLIB_BUILD_TOOLS=ON`, then build the `seqlib_check` target.
- ntdlib's `tools/sequence_export/filter_tables.py switch` derives the Switch's SoundFont and
  SFZ filter tables in ntdlib's `src/sequence_export/filter_tables_switch.cpp` from WarioWare:
  Get It Together!'s tables in ntdlib's `include/ntdlib/nwsnd/tables.h`. Requires Python 3 and
  numpy.

The archive check requires a game's archive, supplied separately. hacsf2 doesn't distribute game
files. Normal conversion requires only the input archive, and any group files beside it.

## Licence

hacsf2 is under the [MIT](LICENSE) licence. The libraries it builds on, seqlib, xsflib and
ntdlib, are under the MIT licence too. `THIRD-PARTY-NOTICES.md` gives their licence texts. Game
data is not covered by this licence.
