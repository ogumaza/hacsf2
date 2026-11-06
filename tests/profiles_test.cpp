// SPDX-License-Identifier: MIT

// The conversions in each state of a sound (src/profiles.h). A sound's sequence plays its first state, and the
// sequences of its later states follow it, with the state's name after the sound's. The other sequences stay as they
// are. Each sequence's index gives back its sound and its state. The state gives the tracks that play and the dry gain
// of the reverb on aux bus B. The renderer's reverb holds its dry gain in its eleventh value, not its last. An archive
// without rules converts as before.

#include "profiles.h"

#include <cstdint>
#include <string>
#include <vector>

#include "ntdlib/sar/game_rules.h"
#include "ntdlib/sequence_export/frame.h"
#include "ntdlib/sequence_export/performance.h"
#include "xsflib/testing/check.h"

using xsflib::testing::Check;

int main()
{
    ntdlib::sar::ArchiveRules rules;
    rules.layered.push_back({"SEQ_B", {{"", 0x0006}, {"later", 0x0001}, {"all", 0}}});
    rules.bus_b_presets.push_back({"I3DL2_Small", {}});
    rules.bus_b_presets.back().parameters[11] = 0.5f;
    rules.bus_b.emplace("SEQ_B", "I3DL2_Small");
    rules.bus_b_later.push_back("SEQ_C");

    const std::vector<ntdlib::sequence_export::Sequence> sequences =
        hacsf2::WithStates({{0, "SEQ_A"}, {7, "SEQ_B"}, {8, "SEQ_C"}, {9, "SEQ_B2"}}, rules);
    Check(sequences.size() == 7 && sequences[0].name == "SEQ_A" && sequences[1].name == "SEQ_B" &&
              sequences[2].name == "SEQ_B later" && sequences[3].name == "SEQ_B all" && sequences[4].name == "SEQ_C" &&
              sequences[5].name == "SEQ_C Small reverb" && sequences[6].name == "SEQ_B2" && sequences[6].index == 9,
          "a sound's states follow it, and the other sequences stay as they are");
    Check(sequences.size() == 7 && hacsf2::SoundOf(sequences[2].index) == 7 &&
              hacsf2::StateOf(sequences[2].index) == 1 && hacsf2::SoundOf(sequences[5].index) == 8 &&
              hacsf2::StateOf(sequences[5].index) == 1 && hacsf2::StateOf(sequences[6].index) == 0,
          "a sequence's index gives its sound and its state");

    const ntdlib::sequence_export::PerformOptions perform;
    const ntdlib::sequence_export::PerformOptions b0 = hacsf2::InState(perform, rules, "SEQ_B", 0);
    const ntdlib::sequence_export::PerformOptions b1 = hacsf2::InState(perform, rules, "SEQ_B", 1);
    const ntdlib::sequence_export::PerformOptions c0 = hacsf2::InState(perform, rules, "SEQ_C", 0);
    const ntdlib::sequence_export::PerformOptions c1 = hacsf2::InState(perform, rules, "SEQ_C", 1);
    const ntdlib::sequence_export::PerformOptions a0 = hacsf2::InState(perform, rules, "SEQ_A", 0);
    Check(b0.tracks == 0xfff9 && b1.tracks == 0xfffe && b0.bus_reverbs[1] == 0.5f && b1.bus_reverbs[1] == 0.5f,
          "a state plays every track that it doesn't turn down, with its sound's reverb on bus B");
    Check(c0.tracks == 0xffff && !c0.bus_reverbs[1] && c1.bus_reverbs[1] == 0.5f,
          "a sound of a stage starts with bus B empty, and plays later with the reverb there");
    Check(a0.tracks == 0xffff && !a0.bus_reverbs[0] && !a0.bus_reverbs[1] && !a0.bus_reverbs[2],
          "another sound plays every track with no reverb on the buses");

    Check(!a0.bus_a_delay, "a game with a reverb on bus A keeps its sends in CC91");
    ntdlib::sar::ArchiveRules delay = rules;
    delay.bus_a_delay = true;
    Check(hacsf2::InState(perform, delay, "SEQ_A", 0).bus_a_delay &&
              hacsf2::InState(perform, delay, "SEQ_B", 1).bus_a_delay,
          "a game with a delay on bus A says so in every state");
    ntdlib::sar::ArchiveRules room;
    room.bus_b_presets.push_back({"EFFECT_ROOM",
                                  {2.0f, 0.4f, 40000.0f, 4.0f, 0.5f, 2000000.0f, 0.7f, 0.1f, 0.6f, 1.0f, 0.25f, 2.0f},
                                  ntdlib::sar::ReverbType::kReverb});
    room.bus_b_always = "EFFECT_ROOM";
    Check(hacsf2::InState(perform, room, "SEQ_A", 0).bus_reverbs[1] == 0.25f,
          "the renderer's reverb on bus B gives its dry gain, its eleventh value");

    const ntdlib::sar::ArchiveRules none;
    Check(hacsf2::WithStates({{0, "SEQ_A"}, {7, "SEQ_B"}}, none).size() == 2 &&
              !hacsf2::InState(perform, none, "SEQ_B", 0).bus_reverbs[1],
          "an archive without rules converts as before");

    return xsflib::testing::Summary("hacsf2 profiles");
}
