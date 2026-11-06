// SPDX-License-Identifier: MIT

// The start of a sound that sounds (src/sounding_start.h). A stand-in for the conversion plays a sample only for one
// random state, or only for one value of a variable. ConvertSounding keeps a conversion that plays a sample at once. It
// tries the rules' values before later starts, and later starts only when the sequence drew a number and the seed is
// nn::atk's first. It gives up on a sound that never plays a sample. A note at a volume of 0 plays none.

#include "sounding_start.h"

#include <cstdint>
#include <string>
#include <vector>

#include "ntdlib/sar/game_rules.h"
#include "ntdlib/sequence_export/convert.h"
#include "ntdlib/sequence_export/performance.h"
#include "xsflib/testing/check.h"

using xsflib::testing::Check;

namespace
{

namespace se = ntdlib::sequence_export;

// A MIDI file of one track: CC7 at `volume`, then a note of a quarter note.
std::vector<uint8_t> OneNote(uint8_t volume)
{
    const std::vector<uint8_t> events = {0x00, 0xB0, 7,  volume, 0x00, 0x90, 60,   100,
                                         0x60, 0x80, 60, 0,      0x00, 0xFF, 0x2F, 0};
    std::vector<uint8_t> midi = {'M', 'T', 'h', 'd', 0,   0,   0,   6, 0, 0, 0,
                                 1,   0,   96,  'M', 'T', 'r', 'k', 0, 0, 0, static_cast<uint8_t>(events.size())};
    midi.insert(midi.end(), events.begin(), events.end());

    return midi;
}

// A conversion that plays a sample when `plays`, from a sequence that drew a number when `draws`.
se::Conversion Converted(bool plays, bool draws)
{
    se::Conversion conversion;
    if (plays)
    {
        conversion.performance.note_commands.resize(1);
        conversion.samples = 1;
        conversion.midi = OneNote(100);
    }
    conversion.performance.draws = draws;

    return conversion;
}

} // namespace

int main()
{
    // nn::atk's generator: x * 0x19660D + 0x3C6EF35F.
    Check(hacsf2::StateAfter(0x12345678, 0) == 0x12345678, "no frames leave the state as it is");
    Check(hacsf2::StateAfter(0x12345678, 2) == (0x12345678u * 0x19660Du + 0x3C6EF35Fu) * 0x19660Du + 0x3C6EF35Fu,
          "the state steps once a frame");

    const se::PerformOptions defaults;
    const uint32_t sounding_seed = hacsf2::StateAfter(defaults.seed, 37);
    int calls = 0;
    const auto drawing = [&calls, sounding_seed](const se::PerformOptions& options)
    {
        calls++;
        return Converted(options.seed == sounding_seed, true);
    };
    se::Conversion found = hacsf2::ConvertSounding(drawing, defaults);
    Check(hacsf2::PlaysSample(found) && calls == 38,
          "a sound that draws sounds from the start 37 frames later, after " + std::to_string(calls) + " conversions");

    se::PerformOptions seeded;
    seeded.seed = 0x1;
    calls = 0;
    Check(!hacsf2::PlaysSample(hacsf2::ConvertSounding(drawing, seeded)) && calls == 1, "--seed keeps its state");

    calls = 0;
    const auto still = [&calls](const se::PerformOptions&)
    {
        calls++;
        return Converted(false, false);
    };
    Check(!hacsf2::PlaysSample(hacsf2::ConvertSounding(still, defaults)) && calls == 1,
          "a sound that draws nothing gets no later start");

    const ntdlib::sar::VariableValues low = {{6, 127}};
    const ntdlib::sar::VariableValues high = {{6, 2 << 7 | 127}};
    const ntdlib::sar::VariableValues states[] = {low, high};
    calls = 0;
    const auto waiting = [&calls](const se::PerformOptions& options)
    {
        calls++;
        const auto found_value = options.variables.find(6);
        return Converted(found_value != options.variables.end() && found_value->second == (2 << 7 | 127), true);
    };
    Check(hacsf2::PlaysSample(hacsf2::ConvertSounding(waiting, defaults, states)) && calls == 3,
          "a sound that waits for a value takes the first of the rules' values with which it plays a sample");

    se::Conversion quiet = Converted(true, false);
    quiet.midi = OneNote(0);
    Check(hacsf2::HasAudibleNote(OneNote(1)) && !hacsf2::HasAudibleNote(OneNote(0)) && !hacsf2::PlaysSample(quiet),
          "a note at CC7 0 plays no sample, and one at CC7 1 does");

    se::PerformOptions given;
    given.variables[6] = 5;
    calls = 0;
    const se::Conversion kept = hacsf2::ConvertSounding(waiting, given, states);
    Check(!hacsf2::PlaysSample(kept) && calls == 3 + 400, "a value from --var stays, and later starts follow");

    return xsflib::testing::Summary("hacsf2 sounding start");
}
