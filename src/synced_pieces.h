// SPDX-License-Identifier: MIT

// The pieces that follow the beat and the chords of the stream under them (ntdlib/nx/bex/music_sync.h). Super Mario
// Party Jamboree's star melody plays over a board's stream, and reads the stream's beat and chord through user
// procedures 2 to 4. hacsf2 converts such a piece once for each stream whose files lie beside the archive.

#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include "ntdlib/sar/formats.h"
#include "ntdlib/sequence_export/frame.h"
#include "ntdlib/sequence_export/performance.h"

namespace hacsf2
{

// A conversion of a piece over a stream: the piece's sound, the conversion's name (the piece's label, a space and the
// stream's label), and the writes of the piece's user procedures, from the frame of the piece's first tick.
struct SyncedSequence
{
    std::size_t sound = 0;
    std::string name;
    std::vector<ntdlib::sequence_export::ProcedureChange> changes;
};

// The conversions of the synced pieces in `archive`, the archive at `input`: one for each stream whose file and jump
// setting lie beside the archive, under the names that the game's files give them. The piece starts with the stream.
// Its writes cover one pass over the stream's regions in the order of the jump setting, then the loop once more. Warns
// about a stream whose file or jump setting is damaged.
std::vector<SyncedSequence> FindSyncedSequences(const std::filesystem::path& input,
                                                const ntdlib::sar::SoundArchive& archive);

// `sequences`, with the entry of each piece in `synced` replaced by an entry for each of its conversions. A
// conversion's index is the archive's count of sounds plus its place in `synced`.
std::vector<ntdlib::sequence_export::Sequence> WithSyncedSequences(
    std::vector<ntdlib::sequence_export::Sequence> sequences, const ntdlib::sar::SoundArchive& archive,
    const std::vector<SyncedSequence>& synced);

} // namespace hacsf2
