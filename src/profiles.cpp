// SPDX-License-Identifier: MIT

#include "profiles.h"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

namespace hacsf2
{

namespace
{

using ntdlib::sequence_export::Sequence;

// The bits of a sequence's index above the sound's index hold the state's number.
constexpr int kStateShift = 32;
static_assert(sizeof(std::size_t) * 8 > kStateShift, "a sequence's index has room for a state's number");

} // namespace

std::vector<Sequence> WithStates(std::vector<Sequence> sequences, const ntdlib::sar::ArchiveRules& rules)
{
    if (rules.Empty())
    {
        return sequences;
    }

    std::vector<Sequence> out;
    for (Sequence& sequence : sequences)
    {
        const std::vector<ntdlib::sar::SoundState> states = ntdlib::sar::StatesOf(rules, sequence.name);
        const Sequence start = sequence;
        out.push_back(std::move(sequence));
        for (std::size_t state = 1; state < states.size(); ++state)
        {
            Sequence later = start;
            later.index = start.index | state << kStateShift;
            later.name += " ";
            later.name += states[state].name;
            out.push_back(std::move(later));
        }
    }

    return out;
}

uint32_t SoundOf(std::size_t index)
{
    return static_cast<uint32_t>(index & ((std::size_t{1} << kStateShift) - 1));
}

std::size_t StateOf(std::size_t index)
{
    return index >> kStateShift;
}

ntdlib::sequence_export::PerformOptions InState(const ntdlib::sequence_export::PerformOptions& perform,
                                                const ntdlib::sar::ArchiveRules& rules, std::string_view name,
                                                std::size_t state)
{
    ntdlib::sequence_export::PerformOptions in_state = perform;
    if (rules.Empty())
    {
        return in_state;
    }

    in_state.bus_a_delay = rules.bus_a_delay;

    const std::vector<ntdlib::sar::SoundState> states = ntdlib::sar::StatesOf(rules, name);
    if (state >= states.size())
    {
        return in_state;
    }

    in_state.tracks = static_cast<uint16_t>(perform.tracks & ~states[state].muted);
    if (const ntdlib::sar::ReverbPreset* reverb = rules.FindBusBPreset(states[state].bus_b))
    {
        in_state.bus_reverbs[1] = reverb->DryGain();
    }

    return in_state;
}

} // namespace hacsf2
