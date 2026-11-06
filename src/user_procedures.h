// SPDX-License-Identifier: MIT

// The handlers of user procedures that hacsf2 runs as the game's code does (ntdlib/nx/bex/user_procedures.h): those of
// Super Mario Party Jamboree and Mario Party Superstars. Their sounds pick a variant through a user procedure. A table
// recognizes each sound archive of the two games by its size and SHA-1 (procedure_archives.cpp). Each sequence of
// such an archive converts with the game's handler, from the state that the handler's constructor gives it.

#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string_view>

#include "ntdlib/nx/bex/user_procedures.h"
#include "ntdlib/sequence_export/performance.h"

namespace hacsf2
{

// A sound archive of a game whose handler hacsf2 runs.
struct ProcedureArchive
{
    std::size_t size = 0;
    std::string_view sha1;
    ntdlib::nx::bex::ProcedureHandler handler = ntdlib::nx::bex::ProcedureHandler::kNone;
};

// The table: the two games' sound archives, in the order of their sizes, then of their SHA-1s.
std::span<const ProcedureArchive> ProcedureArchives();

// The handler of the game whose sound archive `archive` is, or kNone for another archive.
ntdlib::nx::bex::ProcedureHandler ProcedureHandlerOf(std::span<const uint8_t> archive);

// The model of `handler` for a conversion, or an empty model for kNone. Each conversion starts from a copy of the
// model, at the state that the handler's constructor gives it. Jamboree's handler gets the tables of its procedure 5
// that lie beside the archive at `input`, or in the game's folders: audio/userproc/conv/00.msgpack to 15.msgpack, with
// the archive in audio/sounddata/<name>/. Prints a warning for a table that it can't read.
ntdlib::sequence_export::UserProcedureModel ProcedureModel(ntdlib::nx::bex::ProcedureHandler handler,
                                                           const std::filesystem::path& input);

} // namespace hacsf2
