// SPDX-License-Identifier: MIT

#include "rhythm_pieces.h"

#include <cstdint>
#include <map>
#include <span>
#include <string_view>

#include "ntdlib/nx/bex/rhythm_modes.h"
#include "ntdlib/sequence_export/performance.h"

namespace hacsf2
{

namespace
{

// The global variables that the game and the controller use to hand over from one piece to the next (RhythmScript).
constexpr uint8_t kHandOver = 26;
constexpr uint8_t kTempo = 27;
constexpr uint8_t kHandedOver = 28;
constexpr uint8_t kCurrent = 29;

} // namespace

ntdlib::sequence_export::CompanionScript RhythmScript(const ntdlib::nx::bex::RhythmPiece& piece)
{
    ntdlib::sequence_export::CompanionScript script;
    script.writes.push_back({.frame = 0, .index = kTempo, .value = piece.bpm});
    const int16_t n = piece.number;
    if (n == 1 || n == 200 || n == 300 || n == 400)
    {
        script.writes.push_back({.frame = 1, .index = kCurrent, .value = n});
    }
    else
    {
        script.writes.push_back({.frame = 1, .index = kCurrent, .value = 1});
        script.writes.push_back({.frame = 5, .index = kCurrent, .value = n});
        script.writes.push_back({.frame = 7, .index = kHandOver, .value = 1});
    }

    script.hand_over = kHandedOver;
    script.hand_over_value = n;

    return script;
}

void AddRhythmVariables(std::span<const ntdlib::nx::bex::RhythmPiece> pieces, std::string_view label,
                        std::map<uint8_t, int16_t>& variables)
{
    const ntdlib::nx::bex::RhythmPiece* piece = ntdlib::nx::bex::RhythmPieceOf(pieces, label);
    if (!piece)
    {
        return;
    }

    variables.try_emplace(kTempo, piece->bpm);
    variables.try_emplace(kHandedOver, piece->number);
}

} // namespace hacsf2
