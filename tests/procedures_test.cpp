// SPDX-License-Identifier: MIT

// Checks hacsf2's table of the sound archives whose games' handlers of user procedures it runs (src/user_procedures.h),
// and the model that a conversion gets. With arguments, it also checks that the table recognizes every sound archive of
// the games' files:
//
//     hacsf2_procedures_test [jamboree DIR] [superstars DIR]
//
// Each DIR holds a game's sound files, unpacked from the game's .bea archives.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ntdlib/sequence_export/frame.h"
#include "xsflib/files.h"
#include "xsflib/sha1.h"
#include "xsflib/testing/check.h"

#include "user_procedures.h"

using namespace xsflib::testing;

namespace
{

namespace bex = ntdlib::nx::bex;

// The table is in order, lists each archive once, and gives each a SHA-1 of 40 hex digits and one of the two handlers.
void TestTable()
{
    const auto archives = hacsf2::ProcedureArchives();
    const auto out_of_order = [](const hacsf2::ProcedureArchive& a, const hacsf2::ProcedureArchive& b)
    {
        return std::pair(a.size, a.sha1) >= std::pair(b.size, b.sha1);
    };
    const auto valid = [](const hacsf2::ProcedureArchive& a)
    {
        const bool hex = a.sha1.size() == 40 && a.sha1.find_first_not_of("0123456789abcdef") == std::string_view::npos;
        return hex &&
               (a.handler == bex::ProcedureHandler::kJamboree || a.handler == bex::ProcedureHandler::kSuperstars);
    };
    XSFLIB_CHECK(!archives.empty() && std::all_of(archives.begin(), archives.end(), valid));
    XSFLIB_CHECK(std::adjacent_find(archives.begin(), archives.end(), out_of_order) == archives.end());

    // Bytes of a listed archive's size, but not its SHA-1, aren't recognized.
    const std::vector<uint8_t> other(archives.front().size, 0);
    XSFLIB_CHECK(hacsf2::ProcedureHandlerOf(other) == bex::ProcedureHandler::kNone);
}

// The track's variables 14 and 15 after a call of `procedure` by `model`, with the slot and the count in them, and
// whether the call drew.
std::pair<int, bool> Call(ntdlib::sequence_export::UserProcedureModel& model, uint16_t procedure, bool local, int slot,
                          int count)
{
    std::array<int16_t, 16> locals{};
    std::array<int16_t, 16> globals{};
    std::array<int16_t, 16> tracks{};
    locals.fill(-1);
    globals.fill(-1);
    tracks.fill(-1);
    std::array<int16_t, 16>& set = local ? locals : tracks;
    set[14] = static_cast<int16_t>(slot);
    set[15] = static_cast<int16_t>(count);
    const bool drew = model(procedure, locals, globals, tracks);

    return {set[14], drew};
}

// A conversion's model draws from the constructor's state: 1, then 0, for slot 5 and a count of 3, as fn_103b80 and
// fn_479c60 do in runs of the games' code. A copy goes on from the state that it was copied in. Procedure 5 reads a
// table that lies beside the archive. Without a handler, there's no model.
void TestModels()
{
    ntdlib::sequence_export::UserProcedureModel jamboree =
        hacsf2::ProcedureModel(bex::ProcedureHandler::kJamboree, "no folder/subarc_test.fsst");
    ntdlib::sequence_export::UserProcedureModel fresh = jamboree;
    XSFLIB_CHECK(Call(jamboree, 0, false, 5, 3) == std::pair(1, true) && Call(jamboree, 0, false, 5, 3).first == 0);
    XSFLIB_CHECK(Call(fresh, 0, false, 5, 3).first == 1 && Call(fresh, 5, false, 5, 0) == std::pair(5, false));

    ntdlib::sequence_export::UserProcedureModel superstars =
        hacsf2::ProcedureModel(bex::ProcedureHandler::kSuperstars, "subarc_test.fsst");
    XSFLIB_CHECK(Call(superstars, 0, true, 5, 3) == std::pair(1, true) && Call(superstars, 0, false, 5, 3).first == 5);
    XSFLIB_CHECK(!hacsf2::ProcedureModel(bex::ProcedureHandler::kNone, "subarc_test.fsst"));

    // Table 0 beside the archive: a map whose key "Value" holds 128 values, each 2 more than its index.
    const std::filesystem::path folder = std::filesystem::temp_directory_path() / "hacsf2_procedures_test";
    std::filesystem::create_directories(folder);
    std::vector<uint8_t> file = {0x81, 0xa5, 'V', 'a', 'l', 'u', 'e', 0xdc, 0x00, 0x80};
    for (int i = 0; i < 128; ++i)
    {
        file.push_back(static_cast<uint8_t>(std::min(i + 2, 127)));
    }
    std::ofstream(folder / "00.msgpack", std::ios::binary)
        .write(reinterpret_cast<const char*>(file.data()), static_cast<std::streamsize>(file.size()));
    ntdlib::sequence_export::UserProcedureModel with_table =
        hacsf2::ProcedureModel(bex::ProcedureHandler::kJamboree, folder / "subarc_test.fsst");
    XSFLIB_CHECK(Call(with_table, 5, false, 7, 0) == std::pair(9, false));
    std::filesystem::remove_all(folder);
}

// Every sound archive under `folder` is one of the table's for `handler`, and the folder holds every archive that the
// table lists for it.
void TestGameFiles(bex::ProcedureHandler handler, const std::filesystem::path& folder)
{
    std::set<std::pair<std::size_t, std::string>> found;
    std::size_t unknown = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(folder))
    {
        const std::string extension = xsflib::Utf8(entry.path().extension());
        if (!entry.is_regular_file() || (extension != ".fsst" && extension != ".fspj" && extension != ".bfsar"))
        {
            continue;
        }

        const std::optional<std::vector<uint8_t>> bytes = ntdlib::sequence_export::ReadFile(entry.path());
        if (!bytes || hacsf2::ProcedureHandlerOf(*bytes) != handler)
        {
            ++unknown;
            Check(false, xsflib::Utf8(entry.path()) + " is recognized");
            continue;
        }
        found.insert({bytes->size(), xsflib::Sha1::Hex(xsflib::Sha1::Of(*bytes))});
    }

    const auto archives = hacsf2::ProcedureArchives();
    const auto of_handler = [handler](const hacsf2::ProcedureArchive& a)
    {
        return a.handler == handler;
    };
    const auto listed = static_cast<std::size_t>(std::count_if(archives.begin(), archives.end(), of_handler));
    Check(unknown == 0 && found.size() == listed, xsflib::Utf8(folder) + ": " + std::to_string(found.size()) + " of " +
                                                      std::to_string(listed) + " listed archives found, " +
                                                      std::to_string(unknown) + " not recognized");
}

} // namespace

int main(int argc, char** argv)
{
    TestTable();
    TestModels();
    for (int i = 1; i + 1 < argc; i += 2)
    {
        const std::string game = argv[i];
        if (game != "jamboree" && game != "superstars")
        {
            Check(false, "the game " + game + " is jamboree or superstars");
            continue;
        }

        const bex::ProcedureHandler handler =
            game == "jamboree" ? bex::ProcedureHandler::kJamboree : bex::ProcedureHandler::kSuperstars;
        TestGameFiles(handler, xsflib::PathFromUtf8(argv[i + 1]));
    }

    return Summary("hacsf2_procedures");
}
