// SPDX-License-Identifier: MIT

// The pieces of Super Mario Party Jamboree's rhythm mode and of Super Mario Party's Sound Stage. Each waits for values
// that the game writes to the global variables of its sequence. hacsf2 converts each piece with those values. ntdlib's
// list of the rhythm modes (ntdlib/nx/bex/rhythm_modes.h) finds the pieces.

#pragma once

#include <cstdint>
#include <map>
#include <span>
#include <string_view>

#include "ntdlib/nx/bex/rhythm_modes.h"
#include "ntdlib/sequence_export/performance.h"

namespace hacsf2
{

// How the game plays `piece` beside the controller of its mode (ntdlib::sequence_export::CompanionScript). The game
// writes the tempo to the global variable 27 and starts the controller. The controller sets the global variables 24 to
// 26, 28 and 29 to -1, sets 26 to 1 and waits until 29 is 1, 200, 300 or 400. The first piece of the mode writes one of
// those numbers to 29. Each later piece writes its number there, and the piece before it then sets 26 to 1. At the end
// of a bar, the controller copies 29 to 28 while 26 is 1. The piece plays from the frame in which 28 is set to its
// number.
//
// The script does the same, at frames counted from the first tick of the controller. The tempo is written to 27 before
// the controller starts. The number of a first piece is written to 29 in frame 1. The game starts the controller and
// the piece in the same frame, in that order, and the first tick of the piece follows that of the controller. For a
// later piece, the 1 of the piece before is written to 29 in frame 1 and the number of the piece in frame 5, and 26 is
// set to 1 in frame 7. The controller of the Sound Stage in Super Mario Party waits for 1 or 200 only. It reads bit 0
// of 26 as the request to copy 29 to 28, and the 1 of the script sets that bit. The same script therefore serves both
// games.
ntdlib::sequence_export::CompanionScript RhythmScript(const ntdlib::nx::bex::RhythmPiece& piece);

// Adds the values that the game sets for the piece labelled `label` among `pieces` to `variables`, by the numbers of
// the variables in the sequence commands. A value that `variables` contains already, from --var, stays. A label that
// `pieces` doesn't contain adds nothing.
void AddRhythmVariables(std::span<const ntdlib::nx::bex::RhythmPiece> pieces, std::string_view label,
                        std::map<uint8_t, int16_t>& variables);

} // namespace hacsf2
