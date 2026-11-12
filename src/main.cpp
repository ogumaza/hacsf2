// SPDX-License-Identifier: MIT

// hacsf2: convert Nintendo Switch sound archive sequences (.bfsar) to MIDI, SoundFont and SFZ.
//
//     hacsf2 [options] <archive.bfsar> [<archive.bfsar> ...]
//
// Output goes beside each archive. Windows accepts files dropped on the executable; macOS uses Hacsf2.app.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "ntdlib/bezel/sound_levels.h"
#include "ntdlib/nx/bex/rhythm_modes.h"
#include "ntdlib/sar/formats.h"
#include "ntdlib/sar/game_rules.h"
#include "ntdlib/sequence_export/archive_converter.h"
#include "ntdlib/sequence_export/consoles.h"
#include "ntdlib/sequence_export/conversion.h"
#include "ntdlib/sequence_export/convert.h"
#include "ntdlib/sequence_export/frame.h"
#include "ntdlib/sequence_export/performance.h"
#include "xsflib/binary.h"
#include "xsflib/files.h"

#include "profiles.h"
#include "rhythm_pieces.h"
#include "sounding_start.h"
#include "synced_pieces.h"
#include "user_procedures.h"

namespace
{

namespace se = ntdlib::sequence_export;

// An archive that hacsf2 converts, its game's rules (profiles.h), the pieces that wait for values that the game gives
// them (rhythm_pieces.h), and the conversions of the pieces that follow the stream under them (synced_pieces.h). The
// rhythm pieces' controller is sound `controller` of this archive, or of `controller_archive` when another archive
// holds it, or nothing when no archive beside this one does.
struct Input
{
    ntdlib::sar::SoundArchive archive;
    const ntdlib::sar::ArchiveRules* rules = nullptr;
    ntdlib::nx::bex::RhythmMode rhythm;
    std::vector<hacsf2::SyncedSequence> synced;
    std::optional<ntdlib::sar::SoundArchive> controller_archive;
    std::optional<uint32_t> controller;
};

// The sound archive named `name` (its file name without the extension) near the archive at `input`: in the same
// folder, or in a folder of its name beside the archive's folder, as in the game's files (audio/sounddata/<name>/
// <name>.fsst). Opened with `open`. Nothing when neither file is there.
template <typename Open>
std::optional<ntdlib::sar::SoundArchive> OpenNearby(const std::filesystem::path& input, std::string_view name,
                                                    const Open& open)
{
    const std::filesystem::path file = xsflib::PathFromUtf8(std::string(name) + xsflib::Utf8(input.extension()));
    for (const std::filesystem::path& path :
         {input.parent_path() / file,
          input.parent_path().parent_path() / xsflib::PathFromUtf8(std::string(name)) / file})
    {
        std::error_code error;
        if (std::filesystem::is_regular_file(path, error))
        {
            return open(path, xsflib::Utf8(path.filename()));
        }
    }

    return std::nullopt;
}

// The opener of the sound archives that the frame's converter of archives uses.
using ArchiveOpener = decltype(se::Converter<se::PerformOptions, ntdlib::sar::SoundArchive, se::Conversion>::open);

// Reads the archive at `input`, named `name`, with `open`, and what hacsf2 finds in it and beside it (Input). Returns
// nothing if the archive can't be read.
std::optional<Input> OpenInput(const std::filesystem::path& input, const std::string& name, const ArchiveOpener& open)
{
    std::optional<ntdlib::sar::SoundArchive> archive = open(input, name);
    if (!archive)
    {
        return std::nullopt;
    }

    const ntdlib::sar::ArchiveRules* rules = &ntdlib::sar::RulesOf(archive->Bytes());
    const ntdlib::nx::bex::RhythmMode rhythm = ntdlib::nx::bex::RhythmModeOf(archive->Bytes());
    std::vector<hacsf2::SyncedSequence> synced = hacsf2::FindSyncedSequences(input, *archive);
    Input opened{.archive = std::move(*archive), .rules = rules, .rhythm = rhythm, .synced = std::move(synced)};

    // A rhythm piece converts beside its mode's controller, from this archive or from the archive near it that the
    // game holds it in.
    if (!rhythm.pieces.empty())
    {
        const std::string controller(rhythm.controller.label);
        opened.controller = opened.archive.FindSound(controller);
        if (!opened.controller)
        {
            opened.controller_archive = OpenNearby(input, rhythm.controller.archive, open);
            opened.controller =
                opened.controller_archive ? opened.controller_archive->FindSound(controller) : std::nullopt;
        }

        if (!opened.controller)
        {
            std::fprintf(stderr,
                         "warning: %s: %s%s isn't beside the archive. The rhythm pieces convert without the drums "
                         "of %s.\n",
                         name.c_str(), std::string(rhythm.controller.archive).c_str(),
                         xsflib::Utf8(input.extension()).c_str(), controller.c_str());
        }
    }

    return opened;
}

// Sets up the conversions of `source`, the archive at `input`. Reads the levels of the sounds of a Bezel Engine game
// into `levels`, from the sound sheets in `sheets`, or from a Data/SheetBin folder near the archive when `sheets` is
// empty. Sets the handler of user procedures of the game in `perform` (user_procedures.h).
void PrepareInput(const std::filesystem::path& input, const Input& source,
                  const se::Options<se::PerformOptions>& options, se::PerformOptions& perform,
                  const std::filesystem::path& sheets, ntdlib::bezel::SoundLevels& levels)
{
    const std::optional<std::filesystem::path> folder = sheets.empty() ? ntdlib::bezel::FindSoundSheets(input) : sheets;
    levels = folder ? ntdlib::bezel::ReadSoundLevels(*folder) : ntdlib::bezel::SoundLevels{};
    if (levels.Count() > 0 && !options.quiet)
    {
        std::printf("  levels of %zu sounds from the sound sheets in %s\n", levels.Count(),
                    xsflib::Utf8(*folder).c_str());
    }

    if (levels.Count() == 0 && !sheets.empty())
    {
        std::fprintf(stderr, "warning: no sound sheets in %s\n", xsflib::Utf8(sheets).c_str());
    }

    perform.user_procedures = hacsf2::ProcedureModel(hacsf2::ProcedureHandlerOf(source.archive.Bytes()), input);
}

// Converts sound `sound` of `input`, a rhythm piece, beside its mode's controller, in the state `in_state`. Returns
// nothing when the piece converts alone: it isn't a rhythm piece, its controller isn't there, or the controller doesn't
// hand over to it. `alone` then says why, for the last of those.
std::optional<se::Conversion> ConvertBesideController(const Input& input, uint32_t sound,
                                                      const se::PerformOptions& in_state,
                                                      const se::ArchiveConverter<ntdlib::sar::SoundArchive>& c,
                                                      const ntdlib::bezel::SoundLevels& levels, std::string& alone)
{
    const ntdlib::nx::bex::RhythmPiece* piece =
        ntdlib::nx::bex::RhythmPieceOf(input.rhythm.pieces, input.archive.Sounds()[sound].name);
    if (!piece || !input.controller)
    {
        return std::nullopt;
    }

    const ntdlib::sar::SoundArchive& holder = input.controller_archive ? *input.controller_archive : input.archive;
    se::CompanionScript script = hacsf2::RhythmScript(*piece);
    script.volume = levels.LevelOf(holder.Sounds()[*input.controller].name);
    try
    {
        return se::ConvertBeside(input.archive, sound, holder, *input.controller, script, in_state, c.settings,
                                 c.program);
    }
    catch (const xsflib::FormatError& e)
    {
        alone = std::string("converted without the drums of the rhythm controller: ") + e.what();
    }

    return std::nullopt;
}

// Converts entry `index` of the frame's list of the sequences of `input`, with `perform` and the sound `levels`.
se::Conversion ConvertInput(const Input& input, std::size_t index, const se::PerformOptions& perform,
                            const se::ArchiveConverter<ntdlib::sar::SoundArchive>& c,
                            const ntdlib::bezel::SoundLevels& levels)
{
    // A synced conversion's index follows the archive's sounds (WithSyncedSequences). A later state's index holds the
    // state's number above the sound's (WithStates).
    const std::size_t sounds = input.archive.Sounds().size();
    if (hacsf2::StateOf(index) == 0 && index >= sounds)
    {
        const hacsf2::SyncedSequence& sequence = input.synced.at(index - sounds);
        se::PerformOptions with_changes = perform;
        with_changes.procedure_changes = sequence.changes;
        with_changes.volume = levels.LevelOf(input.archive.Sounds()[sequence.sound].name);
        return se::ConvertSequence(input.archive, static_cast<uint32_t>(sequence.sound), with_changes, c.settings,
                                   c.program);
    }

    const uint32_t sound = hacsf2::SoundOf(index);
    const std::string& name = input.archive.Sounds()[sound].name;
    se::PerformOptions in_state = hacsf2::InState(perform, *input.rules, name, hacsf2::StateOf(index));
    in_state.volume = levels.LevelOf(name);

    // A rhythm piece plays beside its mode's controller. Without the controller, or when the controller doesn't hand
    // over to it, the piece converts alone, with the values that the controller would give it.
    std::string alone;
    if (std::optional<se::Conversion> beside = ConvertBesideController(input, sound, in_state, c, levels, alone))
    {
        return std::move(*beside);
    }

    hacsf2::AddRhythmVariables(input.rhythm.pieces, name, in_state.variables);
    const auto convert = [&c, &input, sound](const se::PerformOptions& options)
    {
        return se::ConvertSequence(input.archive, sound, options, c.settings, c.program);
    };

    se::Conversion converted = hacsf2::ConvertSounding(convert, in_state, ntdlib::sar::StartsOf(*input.rules, name));
    if (!alone.empty())
    {
        converted.warnings.push_back(alone);
    }

    return converted;
}

// The frame's converter of `archive_converter`'s archives, with a conversion for each state of a sound. A piece that
// follows the beat and the chords of the stream under it converts once for each stream whose files lie beside the
// archive, in place of its plain conversion. The sequences of the two games whose handlers of user procedures hacsf2
// runs (user_procedures.h) convert with the game's handler. A Bezel Engine game's sounds convert at the levels of its
// sound sheets, from the folder `sheets`, or from a Data/SheetBin folder near the archive when `sheets` is empty. Each
// input's levels go to `levels`. `archive_converter`, `sheets` and `levels` have to outlive the converter.
se::Converter<se::PerformOptions, Input, se::Conversion> MakeConverter(
    const se::ArchiveConverter<ntdlib::sar::SoundArchive>& archive_converter, const std::filesystem::path& sheets,
    ntdlib::bezel::SoundLevels& levels)
{
    const se::Converter<se::PerformOptions, ntdlib::sar::SoundArchive, se::Conversion> base =
        se::MakeArchiveConverter(archive_converter);

    se::Converter<se::PerformOptions, Input, se::Conversion> converter;
    converter.program = base.program;
    converter.version_line = base.version_line;
    converter.input_word = base.input_word;
    converter.folder_message = base.folder_message;
    converter.samples_folder = base.samples_folder;
    converter.usage = base.usage;
    converter.option = base.option;
    converter.open = [open = base.open](const std::filesystem::path& input, const std::string& name)
    {
        return OpenInput(input, name, open);
    };
    converter.sequences = [](const Input& input)
    {
        return hacsf2::WithSyncedSequences(hacsf2::WithStates(se::ArchiveSequences(input.archive), *input.rules),
                                           input.archive, input.synced);
    };
    converter.prepare = [&sheets, &levels](const std::filesystem::path& input, const Input& source,
                                           const se::Options<se::PerformOptions>& options, se::PerformOptions& perform)
    {
        PrepareInput(input, source, options, perform, sheets, levels);
    };
    converter.convert =
        [&archive_converter, &levels](Input& input, std::size_t index, const se::PerformOptions& perform)
    {
        return ConvertInput(input, index, perform, archive_converter, levels);
    };

    return converter;
}

} // namespace

int main(int argc, char** argv)
{
    se::ArchiveConverter<ntdlib::sar::SoundArchive> archive_converter;
    archive_converter.program = "hacsf2";
    archive_converter.version_line = "hacsf2 " HACSF2_VERSION;
    archive_converter.description =
        "hacsf2: convert Nintendo Switch sound archive sequences (.bfsar) to MIDI,\n"
        "SoundFont 2 and SFZ.\n";
    archive_converter.extension = ".bfsar";
    archive_converter.settings = se::SwitchSettings();
    archive_converter.group_files = true;

    // A Bezel Engine game gives its sounds levels in its sound sheets, outside the sound archive.
    std::filesystem::path sheets;
    archive_converter.extra_options =
        "  --sheets DIR   read the levels of a Bezel Engine game's sounds from the sound\n"
        "                 sheets in DIR (default: a Data/SheetBin folder near the archive)\n";
    archive_converter.option =
        [&sheets](const std::string& option, se::OptionReader& reader, se::PerformOptions& perform)
    {
        if (option == "--sheets")
        {
            sheets = xsflib::PathFromUtf8(reader.Value());
            return true;
        }

        return se::VarOption(option, reader, perform);
    };

    ntdlib::bezel::SoundLevels levels;
    const se::Converter<se::PerformOptions, Input, se::Conversion> converter =
        MakeConverter(archive_converter, sheets, levels);
    const auto run = [&converter](const std::vector<std::string>& args)
    {
        return se::Run(args, converter);
    };

    return se::RunMain(argc, argv, run);
}
