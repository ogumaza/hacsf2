// SPDX-License-Identifier: MIT

#include "sounding_start.h"

#include <algorithm>
#include <array>

#include "ntdlib/nwsnd/util.h"
#include "seqlib/midi/reader.h"

namespace hacsf2
{

namespace se = ntdlib::sequence_export;

uint32_t StateAfter(uint32_t seed, uint32_t frames)
{
    ntdlib::nwsnd::Random random(seed);
    for (uint32_t i = 0; i < frames; ++i)
    {
        random.Next();
    }

    return random.State();
}

bool HasAudibleNote(std::span<const uint8_t> midi)
{
    const seqlib::midi::File file = seqlib::midi::Read(midi);
    for (const seqlib::midi::Track& track : file.tracks)
    {
        std::array<int, 16> volume;
        std::array<int, 16> expression;
        std::array<int, 16> held = {};
        volume.fill(100);
        expression.fill(127);
        for (const seqlib::midi::Event& event : track.events)
        {
            if (event.bytes.size() < 3)
            {
                continue;
            }

            const int kind = event.bytes[0] & 0xF0;
            const int channel = event.bytes[0] & 0x0F;
            if (kind == 0xB0 && event.bytes[1] == 7)
            {
                volume[channel] = event.bytes[2];
            }
            else if (kind == 0xB0 && event.bytes[1] == 11)
            {
                expression[channel] = event.bytes[2];
            }
            else if (kind == 0x90 && event.bytes[2] > 0)
            {
                held[channel]++;
            }
            else if (kind == 0x80 || kind == 0x90)
            {
                held[channel] = std::max(0, held[channel] - 1);
            }

            if (held[channel] > 0 && volume[channel] > 0 && expression[channel] > 0)
            {
                return true;
            }
        }
    }

    return false;
}

bool PlaysSample(const se::Conversion& conversion)
{
    return !conversion.performance.note_commands.empty() && conversion.samples != 0 && HasAudibleNote(conversion.midi);
}

se::Conversion ConvertSounding(const Convert& convert, const se::PerformOptions& perform,
                               std::span<const ntdlib::sar::VariableValues> states)
{
    se::Conversion first = convert(perform);
    if (PlaysSample(first))
    {
        return first;
    }

    for (const ntdlib::sar::VariableValues& state : states)
    {
        se::PerformOptions in_state = perform;
        for (const auto& [number, value] : state)
        {
            in_state.variables.try_emplace(static_cast<uint8_t>(number), value);
        }

        se::Conversion conversion = convert(in_state);
        if (PlaysSample(conversion))
        {
            return conversion;
        }
    }

    const se::PerformOptions defaults;
    if (!first.performance.draws || perform.seed != defaults.seed)
    {
        return first;
    }

    se::PerformOptions later = perform;
    for (uint32_t delay = 1; delay <= kStartDelays; ++delay)
    {
        later.seed = StateAfter(perform.seed, delay);
        se::Conversion conversion = convert(later);
        if (PlaysSample(conversion))
        {
            return conversion;
        }
    }

    return first;
}

} // namespace hacsf2
