// SPDX-License-Identifier: MIT

// The conversions of a sound archive's sequences in the states that a game's code gives them (ntdlib/sar/game_rules.h).
// A game's code may turn tracks of a piece down as the piece plays, through the sound's handle (nn::atk's
// SetTrackVolume, with volume 0). It may put a reverb on aux bus B while a sound plays. A conversion of such a sound
// plays it in the state that the game starts it in. Each later state gets a conversion too, under the sound's name, a
// space and the state's name.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "ntdlib/sar/game_rules.h"
#include "ntdlib/sequence_export/frame.h"
#include "ntdlib/sequence_export/performance.h"

namespace hacsf2
{

// The sequences to convert from an archive whose sequences are `sequences` (ArchiveSequences) and whose rules are
// `rules`. A sound's sequence plays its first state. The sequences of its later states follow it. Each has the state's
// number above the sound's index (StateOf). The other sequences stay as they are.
std::vector<ntdlib::sequence_export::Sequence> WithStates(std::vector<ntdlib::sequence_export::Sequence> sequences,
                                                          const ntdlib::sar::ArchiveRules& rules);

// The sound's index in the archive, from a sequence's index that WithStates gave.
uint32_t SoundOf(std::size_t index);

// The state's number in its sound's states, from a sequence's index that WithStates gave: 0 for the state that the
// sound starts in.
std::size_t StateOf(std::size_t index);

// `perform` for state `state` of the sound named `name` under `rules`: without the tracks that the state turns down
// (PerformOptions::tracks), and with the dry gain of the state's reverb on aux bus B (PerformOptions::bus_reverbs).
ntdlib::sequence_export::PerformOptions InState(const ntdlib::sequence_export::PerformOptions& perform,
                                                const ntdlib::sar::ArchiveRules& rules, std::string_view name,
                                                std::size_t state);

} // namespace hacsf2
