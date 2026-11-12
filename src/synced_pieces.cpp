// SPDX-License-Identifier: MIT

#include "synced_pieces.h"

#include <cstdint>
#include <cstdio>
#include <exception>
#include <optional>
#include <string_view>
#include <utility>

#include "ntdlib/nx/bex/music_sync.h"
#include "xsflib/files.h"

namespace hacsf2
{

namespace
{

namespace bex = ntdlib::nx::bex;
namespace se = ntdlib::sequence_export;

// The file name of a path among the game's files.
std::filesystem::path FileName(std::string_view path)
{
    const std::size_t slash = path.rfind('/');
    return xsflib::PathFromUtf8(std::string(slash == std::string_view::npos ? path : path.substr(slash + 1)));
}

} // namespace

std::vector<SyncedSequence> FindSyncedSequences(const std::filesystem::path& input,
                                                const ntdlib::sar::SoundArchive& archive)
{
    std::vector<SyncedSequence> synced;
    for (const bex::SyncedPiece& piece : bex::SyncedPieces(archive.Bytes()))
    {
        const std::optional<uint32_t> sound = archive.FindSound(std::string(piece.label));
        if (!sound)
        {
            continue;
        }

        for (const bex::SyncedStream& stream : piece.streams)
        {
            const std::optional<std::vector<uint8_t>> stream_file =
                se::ReadFile(input.parent_path() / FileName(stream.stream_path));
            const std::optional<std::vector<uint8_t>> jump_setting =
                se::ReadFile(input.parent_path() / FileName(stream.jump_setting_path));
            if (!stream_file || !jump_setting)
            {
                continue;
            }

            // The piece's first tick comes in the frame after its start (PerformOptions::seed). The writes' frames
            // count from the start.
            bex::SyncedPlay play;
            try
            {
                play = bex::PlaySynced(piece, *stream_file, *jump_setting, 0);
            }
            catch (const std::exception& e)
            {
                std::fprintf(stderr, "warning: %s over %s: %s\n", std::string(piece.label).c_str(),
                             std::string(stream.label).c_str(), e.what());
                continue;
            }

            SyncedSequence sequence;
            sequence.sound = *sound;
            sequence.name = std::string(piece.label) + " " + std::string(stream.label);
            for (const bex::ProcedureWrite& write : play.writes)
            {
                const uint64_t frame = write.frame == 0 ? 0 : write.frame - 1;
                sequence.changes.push_back({frame, write.procedure, static_cast<uint8_t>(write.variable), write.value});
            }

            synced.push_back(std::move(sequence));
        }
    }

    return synced;
}

std::vector<se::Sequence> WithSyncedSequences(std::vector<se::Sequence> sequences,
                                              const ntdlib::sar::SoundArchive& archive,
                                              const std::vector<SyncedSequence>& synced)
{
    std::vector<se::Sequence> with;
    for (se::Sequence& sequence : sequences)
    {
        bool replaced = false;
        for (std::size_t i = 0; i < synced.size(); ++i)
        {
            if (synced[i].sound == sequence.index)
            {
                with.push_back({archive.Sounds().size() + i, synced[i].name});
                replaced = true;
            }
        }

        if (!replaced)
        {
            with.push_back(std::move(sequence));
        }
    }

    return with;
}

} // namespace hacsf2
