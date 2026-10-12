// SPDX-License-Identifier: MIT

// Converts every sequence of a sound archive (.bfsar) with the Switch's settings and checks each conversion. The unit
// tests of the code in ntdlib and seqlib are in those libraries.
//
//     hacsf2_test <archive.bfsar>

#include "ntdlib/sequence_export/consoles.h"
#include "ntdlib/testing/sequence_export_check.h"

int main(int argc, char** argv)
{
    return ntdlib::testing::RunArchiveCheck(argc, argv, ntdlib::sequence_export::SwitchSettings(), "hacsf2", ".bfsar");
}
