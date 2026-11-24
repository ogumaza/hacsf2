// SPDX-License-Identifier: MIT

// The start of a sound that sounds. A sequence can wait for a value that the game's code writes to its variables. It
// can also draw a number at random and play its notes only on some draws. nn::atk steps its random numbers once a sound
// frame (5 ms). A draw therefore depends on when the game starts the sound. hacsf2 converts a sound as the game starts
// it at once, with its variables at -1. When that conversion plays no sample, hacsf2 tries the values that the rules of
// the archive set for the variables of the sound (ntdlib/sar/game_rules.h). When none of them plays one either and the
// sequence drew a number, hacsf2 converts the sound as the game starts it later. It keeps the first conversion that
// plays a sample.

#pragma once

#include <cstdint>
#include <functional>
#include <span>

#include "ntdlib/sar/game_rules.h"
#include "ntdlib/sequence_export/convert.h"
#include "ntdlib/sequence_export/performance.h"

namespace hacsf2
{

// nn::atk's random state `frames` frames after `seed`: the state steps once a frame (x = x * 0x19660D + 0x3C6EF35F).
uint32_t StateAfter(uint32_t seed, uint32_t frames);

// Whether a MIDI file contains a note that sounds: a note held while CC7 and CC11 of its channel are both above 0. A
// channel starts at the MIDI defaults, CC7 100 and CC11 127.
bool HasAudibleNote(std::span<const uint8_t> midi);

// Whether a conversion plays a sample: it contains a note that sounds (HasAudibleNote), and its SoundFont contains a
// sample. A sequence that plays its notes at a volume of 0 plays none.
bool PlaysSample(const ntdlib::sequence_export::Conversion& conversion);

// Converts a sound with the options given.
using Convert =
    std::function<ntdlib::sequence_export::Conversion(const ntdlib::sequence_export::PerformOptions& perform)>;

// Converts a sound with `convert` and `perform`. When the conversion plays no sample (PlaysSample), converts the sound
// with each of `states` in turn: the values that the game can set in its variables. A value that `perform` contains
// already, from --var, stays. When none plays a sample and the first conversion's sequence drew a number, converts the
// sound with the random states of starts 1 to ntdlib::sar::kStartDelays frames later. `perform` keeps its seed when it
// isn't nn::atk's first state (--seed). Returns the first conversion that plays a sample, or else the first conversion.
ntdlib::sequence_export::Conversion ConvertSounding(const Convert& convert,
                                                    const ntdlib::sequence_export::PerformOptions& perform,
                                                    std::span<const ntdlib::sar::VariableValues> states = {});

} // namespace hacsf2
