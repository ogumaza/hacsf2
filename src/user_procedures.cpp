// SPDX-License-Identifier: MIT

#include "user_procedures.h"

#include <algorithm>
#include <cstdio>
#include <exception>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include "ntdlib/sequence_export/frame.h"
#include "xsflib/files.h"
#include "xsflib/sha1.h"

namespace hacsf2
{
namespace
{

namespace bex = ntdlib::nx::bex;
namespace se = ntdlib::sequence_export;

// The file of table `number` of Jamboree's procedure 5, or nothing: beside the archive at `input`, or where the game's
// folders hold it for an archive in audio/sounddata/<name>/ or in _Resident/.
std::optional<std::filesystem::path> TableFile(const std::filesystem::path& input, uint32_t number)
{
    const std::string name = std::string(number < 10 ? "0" : "") + std::to_string(number) + ".msgpack";
    const std::filesystem::path folder = input.parent_path();
    const std::filesystem::path in_sounddata = folder.parent_path().parent_path() / "userproc" / "conv" / name;
    const std::filesystem::path in_resident = folder.parent_path() / "audio" / "userproc" / "conv" / name;
    for (const std::filesystem::path& path : {folder / name, in_sounddata, in_resident})
    {
        std::error_code ignored;
        if (std::filesystem::is_regular_file(path, ignored))
        {
            return path;
        }
    }

    return std::nullopt;
}

} // namespace

bex::ProcedureHandler ProcedureHandlerOf(std::span<const uint8_t> archive)
{
    std::string sha1;
    for (const ProcedureArchive& a : ProcedureArchives())
    {
        if (archive.size() != a.size)
        {
            continue;
        }

        if (sha1.empty())
        {
            sha1 = xsflib::Sha1::Hex(xsflib::Sha1::Of(archive));
        }
        if (sha1 == a.sha1)
        {
            return a.handler;
        }
    }

    return bex::ProcedureHandler::kNone;
}

se::UserProcedureModel ProcedureModel(bex::ProcedureHandler handler, const std::filesystem::path& input)
{
    if (handler == bex::ProcedureHandler::kNone)
    {
        return {};
    }

    bex::UserProcedures model(handler);
    const uint32_t tables = handler == bex::ProcedureHandler::kJamboree ? bex::UserProcedures::kTableCount : 0;
    for (uint32_t number = 0; number < tables; ++number)
    {
        const std::optional<std::filesystem::path> path = TableFile(input, number);
        if (!path)
        {
            continue;
        }

        const std::optional<std::vector<uint8_t>> bytes = se::ReadFile(*path);
        try
        {
            if (!bytes)
            {
                throw std::runtime_error("can't read the table");
            }
            model.SetTable(number, bex::ReadProcedureTable(*bytes));
        }
        catch (const std::exception& e)
        {
            std::fprintf(stderr, "warning: %s: %s\n", xsflib::Utf8(*path).c_str(), e.what());
        }
    }

    // Each copy of the function holds its state. A conversion therefore starts from the handler's first state.
    return [model](uint16_t procedure, std::span<int16_t, 16> local, std::span<int16_t, 16> global,
                   std::span<int16_t, 16> track) mutable
    {
        bex::ProcedureVariables variables;
        std::copy(local.begin(), local.end(), variables.local.begin());
        std::copy(global.begin(), global.end(), variables.global.begin());
        std::copy(track.begin(), track.end(), variables.track.begin());
        if (!model.Run(procedure, variables))
        {
            return false;
        }

        std::copy(variables.local.begin(), variables.local.end(), local.begin());
        std::copy(variables.global.begin(), variables.global.end(), global.begin());
        std::copy(variables.track.begin(), variables.track.end(), track.begin());

        return model.KeepsState(procedure);
    };
}

} // namespace hacsf2
