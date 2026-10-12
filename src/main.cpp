// SPDX-License-Identifier: MIT

// hacsf2: convert Nintendo Switch sound archive sequences (.bfsar) to MIDI, SoundFont and SFZ.
//
//     hacsf2 [options] <archive.bfsar> [<archive.bfsar> ...]
//
// Output goes beside each archive. Windows accepts files dropped on the executable; macOS uses Hacsf2.app.

#include "ntdlib/sar/formats.h"
#include "ntdlib/sequence_export/archive_converter.h"
#include "ntdlib/sequence_export/consoles.h"

int main(int argc, char** argv)
{
    ntdlib::sequence_export::ArchiveConverter<ntdlib::sar::SoundArchive> converter;
    converter.program = "hacsf2";
    converter.version_line = "hacsf2 " HACSF2_VERSION;
    converter.description =
        "hacsf2: convert Nintendo Switch sound archive sequences (.bfsar) to MIDI,\n"
        "SoundFont 2 and SFZ.\n";
    converter.extension = ".bfsar";
    converter.settings = ntdlib::sequence_export::SwitchSettings();
    converter.group_files = true;

    return ntdlib::sequence_export::RunArchiveConverter(argc, argv, converter);
}
