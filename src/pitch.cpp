// SPDX-License-Identifier: MIT

// nn::atk's pitch ratio and LFO (see pitch.h), from WarioWare: Get It Together!'s executable (the addresses are offsets
// into its main). Keep the game's exact table values and constants to reproduce its rounding consistently across
// platforms.

#include "pitch.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace hacsf2
{

namespace
{

// 2^(i/12) for the semitones of an octave (0x1afcb1c), the Wii U's table.
constexpr float kSemitoneTable[12] = {
    0x1.0000000000000p+0f, 0x1.0f38fa0000000p+0f, 0x1.1f59ac0000000p+0f, 0x1.306fe00000000p+0f,
    0x1.428a300000000p+0f, 0x1.55b8100000000p+0f, 0x1.6a09e60000000p+0f, 0x1.7f910e0000000p+0f,
    0x1.965fea0000000p+0f, 0x1.ae89fa0000000p+0f, 0x1.c823e00000000p+0f, 0x1.e3437e0000000p+0f,
};

// 2^(i/3072) for the 1/256 semitones of a semitone (0x1afcb4c), the Wii U's table.
constexpr float kFineTable[256] = {
    0x1.0000000000000p+0f, 0x1.000eca0000000p+0f, 0x1.001d940000000p+0f, 0x1.002c600000000p+0f, 0x1.003b2c0000000p+0f,
    0x1.0049fa0000000p+0f, 0x1.0058c80000000p+0f, 0x1.0067980000000p+0f, 0x1.0076680000000p+0f, 0x1.0085380000000p+0f,
    0x1.00940a0000000p+0f, 0x1.00a2dc0000000p+0f, 0x1.00b1b00000000p+0f, 0x1.00c0840000000p+0f, 0x1.00cf580000000p+0f,
    0x1.00de2e0000000p+0f, 0x1.00ed060000000p+0f, 0x1.00fbde0000000p+0f, 0x1.010ab60000000p+0f, 0x1.0119900000000p+0f,
    0x1.01286a0000000p+0f, 0x1.0137440000000p+0f, 0x1.0146200000000p+0f, 0x1.0154fc0000000p+0f, 0x1.0163da0000000p+0f,
    0x1.0172ba0000000p+0f, 0x1.0181980000000p+0f, 0x1.0190780000000p+0f, 0x1.019f5a0000000p+0f, 0x1.01ae3c0000000p+0f,
    0x1.01bd1e0000000p+0f, 0x1.01cc020000000p+0f, 0x1.01dae60000000p+0f, 0x1.01e9cc0000000p+0f, 0x1.01f8b20000000p+0f,
    0x1.02079a0000000p+0f, 0x1.0216820000000p+0f, 0x1.02256a0000000p+0f, 0x1.0234540000000p+0f, 0x1.02433e0000000p+0f,
    0x1.02522a0000000p+0f, 0x1.0261160000000p+0f, 0x1.0270040000000p+0f, 0x1.027ef20000000p+0f, 0x1.028de00000000p+0f,
    0x1.029cd00000000p+0f, 0x1.02abc00000000p+0f, 0x1.02bab20000000p+0f, 0x1.02c9a40000000p+0f, 0x1.02d8980000000p+0f,
    0x1.02e78c0000000p+0f, 0x1.02f6800000000p+0f, 0x1.0305760000000p+0f, 0x1.03146c0000000p+0f, 0x1.0323640000000p+0f,
    0x1.03325c0000000p+0f, 0x1.0341560000000p+0f, 0x1.0350500000000p+0f, 0x1.035f4a0000000p+0f, 0x1.036e460000000p+0f,
    0x1.037d420000000p+0f, 0x1.038c400000000p+0f, 0x1.039b3e0000000p+0f, 0x1.03aa3e0000000p+0f, 0x1.03b93e0000000p+0f,
    0x1.03c8400000000p+0f, 0x1.03d7420000000p+0f, 0x1.03e6440000000p+0f, 0x1.03f5480000000p+0f, 0x1.04044c0000000p+0f,
    0x1.0413520000000p+0f, 0x1.0422580000000p+0f, 0x1.04315e0000000p+0f, 0x1.0440660000000p+0f, 0x1.044f700000000p+0f,
    0x1.045e780000000p+0f, 0x1.046d840000000p+0f, 0x1.047c8e0000000p+0f, 0x1.048b9c0000000p+0f, 0x1.049aa80000000p+0f,
    0x1.04a9b60000000p+0f, 0x1.04b8c60000000p+0f, 0x1.04c7d60000000p+0f, 0x1.04d6e60000000p+0f, 0x1.04e5f80000000p+0f,
    0x1.04f50a0000000p+0f, 0x1.05041c0000000p+0f, 0x1.0513300000000p+0f, 0x1.0522460000000p+0f, 0x1.05315c0000000p+0f,
    0x1.0540720000000p+0f, 0x1.054f8a0000000p+0f, 0x1.055ea20000000p+0f, 0x1.056dbc0000000p+0f, 0x1.057cd60000000p+0f,
    0x1.058bf20000000p+0f, 0x1.059b0e0000000p+0f, 0x1.05aa2a0000000p+0f, 0x1.05b9480000000p+0f, 0x1.05c8660000000p+0f,
    0x1.05d7860000000p+0f, 0x1.05e6a60000000p+0f, 0x1.05f5c80000000p+0f, 0x1.0604ea0000000p+0f, 0x1.06140c0000000p+0f,
    0x1.0623300000000p+0f, 0x1.0632540000000p+0f, 0x1.06417a0000000p+0f, 0x1.0650a00000000p+0f, 0x1.065fc80000000p+0f,
    0x1.066ef00000000p+0f, 0x1.067e1a0000000p+0f, 0x1.068d440000000p+0f, 0x1.069c6e0000000p+0f, 0x1.06ab9a0000000p+0f,
    0x1.06bac60000000p+0f, 0x1.06c9f40000000p+0f, 0x1.06d9220000000p+0f, 0x1.06e8520000000p+0f, 0x1.06f7820000000p+0f,
    0x1.0706b20000000p+0f, 0x1.0715e40000000p+0f, 0x1.0725180000000p+0f, 0x1.07344a0000000p+0f, 0x1.0743800000000p+0f,
    0x1.0752b40000000p+0f, 0x1.0761ea0000000p+0f, 0x1.0771220000000p+0f, 0x1.07805a0000000p+0f, 0x1.078f920000000p+0f,
    0x1.079ecc0000000p+0f, 0x1.07ae080000000p+0f, 0x1.07bd420000000p+0f, 0x1.07cc800000000p+0f, 0x1.07dbbc0000000p+0f,
    0x1.07eafa0000000p+0f, 0x1.07fa3a0000000p+0f, 0x1.08097a0000000p+0f, 0x1.0818ba0000000p+0f, 0x1.0827fc0000000p+0f,
    0x1.08373e0000000p+0f, 0x1.0846820000000p+0f, 0x1.0855c60000000p+0f, 0x1.08650c0000000p+0f, 0x1.0874520000000p+0f,
    0x1.0883980000000p+0f, 0x1.0892e00000000p+0f, 0x1.08a22a0000000p+0f, 0x1.08b1720000000p+0f, 0x1.08c0be0000000p+0f,
    0x1.08d0080000000p+0f, 0x1.08df540000000p+0f, 0x1.08eea20000000p+0f, 0x1.08fdf00000000p+0f, 0x1.090d3e0000000p+0f,
    0x1.091c8e0000000p+0f, 0x1.092be00000000p+0f, 0x1.093b300000000p+0f, 0x1.094a840000000p+0f, 0x1.0959d60000000p+0f,
    0x1.09692a0000000p+0f, 0x1.0978800000000p+0f, 0x1.0987d60000000p+0f, 0x1.09972c0000000p+0f, 0x1.09a6840000000p+0f,
    0x1.09b5de0000000p+0f, 0x1.09c5360000000p+0f, 0x1.09d4920000000p+0f, 0x1.09e3ec0000000p+0f, 0x1.09f3480000000p+0f,
    0x1.0a02a60000000p+0f, 0x1.0a12040000000p+0f, 0x1.0a21620000000p+0f, 0x1.0a30c20000000p+0f, 0x1.0a40240000000p+0f,
    0x1.0a4f840000000p+0f, 0x1.0a5ee80000000p+0f, 0x1.0a6e4a0000000p+0f, 0x1.0a7dae0000000p+0f, 0x1.0a8d140000000p+0f,
    0x1.0a9c7a0000000p+0f, 0x1.0aabe00000000p+0f, 0x1.0abb480000000p+0f, 0x1.0acab00000000p+0f, 0x1.0ada1a0000000p+0f,
    0x1.0ae9840000000p+0f, 0x1.0af8f00000000p+0f, 0x1.0b085c0000000p+0f, 0x1.0b17ca0000000p+0f, 0x1.0b27380000000p+0f,
    0x1.0b36a60000000p+0f, 0x1.0b46160000000p+0f, 0x1.0b55860000000p+0f, 0x1.0b64f80000000p+0f, 0x1.0b746a0000000p+0f,
    0x1.0b83de0000000p+0f, 0x1.0b93520000000p+0f, 0x1.0ba2c80000000p+0f, 0x1.0bb23e0000000p+0f, 0x1.0bc1b40000000p+0f,
    0x1.0bd12c0000000p+0f, 0x1.0be0a40000000p+0f, 0x1.0bf01e0000000p+0f, 0x1.0bff980000000p+0f, 0x1.0c0f140000000p+0f,
    0x1.0c1e900000000p+0f, 0x1.0c2e0e0000000p+0f, 0x1.0c3d8c0000000p+0f, 0x1.0c4d0a0000000p+0f, 0x1.0c5c8a0000000p+0f,
    0x1.0c6c0c0000000p+0f, 0x1.0c7b8e0000000p+0f, 0x1.0c8b100000000p+0f, 0x1.0c9a940000000p+0f, 0x1.0caa180000000p+0f,
    0x1.0cb99c0000000p+0f, 0x1.0cc9220000000p+0f, 0x1.0cd8aa0000000p+0f, 0x1.0ce8320000000p+0f, 0x1.0cf7ba0000000p+0f,
    0x1.0d07440000000p+0f, 0x1.0d16ce0000000p+0f, 0x1.0d265a0000000p+0f, 0x1.0d35e60000000p+0f, 0x1.0d45740000000p+0f,
    0x1.0d55020000000p+0f, 0x1.0d64920000000p+0f, 0x1.0d74220000000p+0f, 0x1.0d83b20000000p+0f, 0x1.0d93440000000p+0f,
    0x1.0da2d60000000p+0f, 0x1.0db26a0000000p+0f, 0x1.0dc1fe0000000p+0f, 0x1.0dd1940000000p+0f, 0x1.0de12a0000000p+0f,
    0x1.0df0c20000000p+0f, 0x1.0e005a0000000p+0f, 0x1.0e0ff20000000p+0f, 0x1.0e1f8c0000000p+0f, 0x1.0e2f280000000p+0f,
    0x1.0e3ec40000000p+0f, 0x1.0e4e600000000p+0f, 0x1.0e5dfe0000000p+0f, 0x1.0e6d9c0000000p+0f, 0x1.0e7d3c0000000p+0f,
    0x1.0e8cdc0000000p+0f, 0x1.0e9c7c0000000p+0f, 0x1.0eac1e0000000p+0f, 0x1.0ebbc20000000p+0f, 0x1.0ecb660000000p+0f,
    0x1.0edb0a0000000p+0f, 0x1.0eeab00000000p+0f, 0x1.0efa560000000p+0f, 0x1.0f09fe0000000p+0f, 0x1.0f19a60000000p+0f,
    0x1.0f29500000000p+0f,
};

// nn::util's sine table (nn::util::detail::SinCosSampleTable, in the Switch SDK's module): for each of 256 steps of a
// turn, the sine, and the step to the next step's sine. Its values aren't the sine's samples: they're fitted for
// interpolation.
constexpr float kSineTable[256][2] = {
    {0.0f, 0x1.92194p-6f},
    {0x1.92194p-6f, 0x1.91db3ep-6f},
    {0x1.91fa4p-5f, 0x1.915f46p-6f},
    {0x1.2d54fp-4f, 0x1.90a566p-6f},
    {0x1.917e4ap-4f, 0x1.8fadcp-6f},
    {0x1.f569bap-4f, 0x1.8e7878p-6f},
    {0x1.2c83ecp-3f, 0x1.8d05bep-6f},
    {0x1.5e24a4p-3f, 0x1.8b55ccp-6f},
    {0x1.8f8f5ep-3f, 0x1.8968e2p-6f},
    {0x1.c0bc7ap-3f, 0x1.873f4ep-6f},
    {0x1.f1a464p-3f, 0x1.84d964p-6f},
    {0x1.111fc8p-2f, 0x1.823784p-6f},
    {0x1.29434p-2f, 0x1.7f5a18p-6f},
    {0x1.4138e2p-2f, 0x1.7c418cp-6f},
    {0x1.58fcfap-2f, 0x1.78ee5ep-6f},
    {0x1.708bep-2f, 0x1.75611p-6f},
    {0x1.87e1f2p-2f, 0x1.719a2cp-6f},
    {0x1.9efb94p-2f, 0x1.6d9a4cp-6f},
    {0x1.b5d538p-2f, 0x1.69620cp-6f},
    {0x1.cc6b5ap-2f, 0x1.64f21p-6f},
    {0x1.e2ba7ap-2f, 0x1.604b0ap-6f},
    {0x1.f8bf2cp-2f, 0x1.5b6dbp-6f},
    {0x1.073b04p-1f, 0x1.565ac2p-6f},
    {0x1.11eddap-1f, 0x1.51130cp-6f},
    {0x1.1c7672p-1f, 0x1.4b9758p-6f},
    {0x1.26d32cp-1f, 0x1.45e884p-6f},
    {0x1.31027p-1f, 0x1.40076ep-6f},
    {0x1.3b02acp-1f, 0x1.39f4fep-6f},
    {0x1.44d254p-1f, 0x1.33b226p-6f},
    {0x1.4e6fe6p-1f, 0x1.2d3fd8p-6f},
    {0x1.57d9e4p-1f, 0x1.269f18p-6f},
    {0x1.610edcp-1f, 0x1.1fd0eap-6f},
    {0x1.6a0d64p-1f, 0x1.18d658p-6f},
    {0x1.72d416p-1f, 0x1.11b078p-6f},
    {0x1.7b619ap-1f, 0x1.0a6064p-6f},
    {0x1.83b49ep-1f, 0x1.02e73ep-6f},
    {0x1.8bcbd8p-1f, 0x1.f68c52p-7f},
    {0x1.93a608p-1f, 0x1.e6fcaap-7f},
    {0x1.9b41fcp-1f, 0x1.d721ecp-7f},
    {0x1.a29e84p-1f, 0x1.c6fe86p-7f},
    {0x1.a9ba7ep-1f, 0x1.b694f6p-7f},
    {0x1.b094dp-1f, 0x1.a5e7c6p-7f},
    {0x1.b72c7p-1f, 0x1.94f984p-7f},
    {0x1.bd8056p-1f, 0x1.83ccd2p-7f},
    {0x1.c38f8ap-1f, 0x1.72645p-7f},
    {0x1.c9591ap-1f, 0x1.60c2b2p-7f},
    {0x1.cedc26p-1f, 0x1.4eeaaep-7f},
    {0x1.d417dp-1f, 0x1.3cdf06p-7f},
    {0x1.d90b4cp-1f, 0x1.2aa28p-7f},
    {0x1.ddb5d6p-1f, 0x1.1837ecp-7f},
    {0x1.e216b6p-1f, 0x1.05a222p-7f},
    {0x1.e62d3ep-1f, 0x1.e5c802p-8f},
    {0x1.e9f8cep-1f, 0x1.c000d6p-8f},
    {0x1.ed78dp-1f, 0x1.99f494p-8f},
    {0x1.f0acbap-1f, 0x1.73a91cp-8f},
    {0x1.f3940cp-1f, 0x1.4d2452p-8f},
    {0x1.f62e54p-1f, 0x1.266c2ap-8f},
    {0x1.f87b2cp-1f, 0x1.ff0d36p-9f},
    {0x1.fa7a3ap-1f, 0x1.b0f34ap-9f},
    {0x1.fc2b2cp-1f, 0x1.62969ap-9f},
    {0x1.fd8dc4p-1f, 0x1.14033cp-9f},
    {0x1.fea1c6p-1f, 0x1.8a8a9cp-10f},
    {0x1.ff670cp-1f, 0x1.d9a3d4p-11f},
    {0x1.ffdd76p-1f, 0x1.14580ep-12f},
    {0x1p+0f, -0x1.14580ep-12f},
    {0x1.ffdd76p-1f, -0x1.d9a3d4p-11f},
    {0x1.ff670cp-1f, -0x1.8a8a9cp-10f},
    {0x1.fea1c6p-1f, -0x1.14033cp-9f},
    {0x1.fd8dc4p-1f, -0x1.62969ap-9f},
    {0x1.fc2b2cp-1f, -0x1.b0f34ap-9f},
    {0x1.fa7a3ap-1f, -0x1.ff0d36p-9f},
    {0x1.f87b2cp-1f, -0x1.266c2ap-8f},
    {0x1.f62e54p-1f, -0x1.4d2452p-8f},
    {0x1.f3940cp-1f, -0x1.73a91cp-8f},
    {0x1.f0acbap-1f, -0x1.99f494p-8f},
    {0x1.ed78dp-1f, -0x1.c000d6p-8f},
    {0x1.e9f8cep-1f, -0x1.e5c802p-8f},
    {0x1.e62d3ep-1f, -0x1.05a222p-7f},
    {0x1.e216b6p-1f, -0x1.1837ecp-7f},
    {0x1.ddb5d6p-1f, -0x1.2aa28p-7f},
    {0x1.d90b4cp-1f, -0x1.3cdf06p-7f},
    {0x1.d417dp-1f, -0x1.4eeaaep-7f},
    {0x1.cedc26p-1f, -0x1.60c2b2p-7f},
    {0x1.c9591ap-1f, -0x1.72645p-7f},
    {0x1.c38f8ap-1f, -0x1.83ccd2p-7f},
    {0x1.bd8056p-1f, -0x1.94f984p-7f},
    {0x1.b72c7p-1f, -0x1.a5e7c6p-7f},
    {0x1.b094dp-1f, -0x1.b694f6p-7f},
    {0x1.a9ba7ep-1f, -0x1.c6fe86p-7f},
    {0x1.a29e84p-1f, -0x1.d721ecp-7f},
    {0x1.9b41fcp-1f, -0x1.e6fcaap-7f},
    {0x1.93a608p-1f, -0x1.f68c52p-7f},
    {0x1.8bcbd8p-1f, -0x1.02e73ep-6f},
    {0x1.83b49ep-1f, -0x1.0a6064p-6f},
    {0x1.7b619ap-1f, -0x1.11b078p-6f},
    {0x1.72d416p-1f, -0x1.18d658p-6f},
    {0x1.6a0d64p-1f, -0x1.1fd0eap-6f},
    {0x1.610edcp-1f, -0x1.269f18p-6f},
    {0x1.57d9e4p-1f, -0x1.2d3fd8p-6f},
    {0x1.4e6fe6p-1f, -0x1.33b226p-6f},
    {0x1.44d254p-1f, -0x1.39f4fep-6f},
    {0x1.3b02acp-1f, -0x1.40076ep-6f},
    {0x1.31027p-1f, -0x1.45e884p-6f},
    {0x1.26d32cp-1f, -0x1.4b9758p-6f},
    {0x1.1c7672p-1f, -0x1.51130cp-6f},
    {0x1.11eddap-1f, -0x1.565ac2p-6f},
    {0x1.073b04p-1f, -0x1.5b6dbp-6f},
    {0x1.f8bf2cp-2f, -0x1.604b0ap-6f},
    {0x1.e2ba7ap-2f, -0x1.64f21p-6f},
    {0x1.cc6b5ap-2f, -0x1.69620cp-6f},
    {0x1.b5d538p-2f, -0x1.6d9a4cp-6f},
    {0x1.9efb94p-2f, -0x1.719a2cp-6f},
    {0x1.87e1f2p-2f, -0x1.75611p-6f},
    {0x1.708bep-2f, -0x1.78ee5ep-6f},
    {0x1.58fcfap-2f, -0x1.7c418cp-6f},
    {0x1.4138e2p-2f, -0x1.7f5a18p-6f},
    {0x1.29434p-2f, -0x1.823784p-6f},
    {0x1.111fc8p-2f, -0x1.84d964p-6f},
    {0x1.f1a464p-3f, -0x1.873f4ep-6f},
    {0x1.c0bc7ap-3f, -0x1.8968e2p-6f},
    {0x1.8f8f5ep-3f, -0x1.8b55ccp-6f},
    {0x1.5e24a4p-3f, -0x1.8d05bep-6f},
    {0x1.2c83ecp-3f, -0x1.8e7878p-6f},
    {0x1.f569bap-4f, -0x1.8fadcp-6f},
    {0x1.917e4ap-4f, -0x1.90a566p-6f},
    {0x1.2d54fp-4f, -0x1.915f46p-6f},
    {0x1.91fa4p-5f, -0x1.91db3ep-6f},
    {0x1.92194p-6f, -0x1.92194p-6f},
    {0.0f, -0x1.92194p-6f},
    {-0x1.92194p-6f, -0x1.91db3ep-6f},
    {-0x1.91fa4p-5f, -0x1.915f46p-6f},
    {-0x1.2d54fp-4f, -0x1.90a566p-6f},
    {-0x1.917e4ap-4f, -0x1.8fadcp-6f},
    {-0x1.f569bap-4f, -0x1.8e7878p-6f},
    {-0x1.2c83ecp-3f, -0x1.8d05bep-6f},
    {-0x1.5e24a4p-3f, -0x1.8b55ccp-6f},
    {-0x1.8f8f5ep-3f, -0x1.8968e2p-6f},
    {-0x1.c0bc7ap-3f, -0x1.873f4ep-6f},
    {-0x1.f1a464p-3f, -0x1.84d964p-6f},
    {-0x1.111fc8p-2f, -0x1.823784p-6f},
    {-0x1.29434p-2f, -0x1.7f5a18p-6f},
    {-0x1.4138e2p-2f, -0x1.7c418cp-6f},
    {-0x1.58fcfap-2f, -0x1.78ee5ep-6f},
    {-0x1.708bep-2f, -0x1.75611p-6f},
    {-0x1.87e1f2p-2f, -0x1.719a2cp-6f},
    {-0x1.9efb94p-2f, -0x1.6d9a4cp-6f},
    {-0x1.b5d538p-2f, -0x1.69620cp-6f},
    {-0x1.cc6b5ap-2f, -0x1.64f21p-6f},
    {-0x1.e2ba7ap-2f, -0x1.604b0ap-6f},
    {-0x1.f8bf2cp-2f, -0x1.5b6dbp-6f},
    {-0x1.073b04p-1f, -0x1.565ac2p-6f},
    {-0x1.11eddap-1f, -0x1.51130cp-6f},
    {-0x1.1c7672p-1f, -0x1.4b9758p-6f},
    {-0x1.26d32cp-1f, -0x1.45e884p-6f},
    {-0x1.31027p-1f, -0x1.40076ep-6f},
    {-0x1.3b02acp-1f, -0x1.39f4fep-6f},
    {-0x1.44d254p-1f, -0x1.33b226p-6f},
    {-0x1.4e6fe6p-1f, -0x1.2d3fd8p-6f},
    {-0x1.57d9e4p-1f, -0x1.269f18p-6f},
    {-0x1.610edcp-1f, -0x1.1fd0eap-6f},
    {-0x1.6a0d64p-1f, -0x1.18d658p-6f},
    {-0x1.72d416p-1f, -0x1.11b078p-6f},
    {-0x1.7b619ap-1f, -0x1.0a6064p-6f},
    {-0x1.83b49ep-1f, -0x1.02e73ep-6f},
    {-0x1.8bcbd8p-1f, -0x1.f68c52p-7f},
    {-0x1.93a608p-1f, -0x1.e6fcaap-7f},
    {-0x1.9b41fcp-1f, -0x1.d721ecp-7f},
    {-0x1.a29e84p-1f, -0x1.c6fe86p-7f},
    {-0x1.a9ba7ep-1f, -0x1.b694f6p-7f},
    {-0x1.b094dp-1f, -0x1.a5e7c6p-7f},
    {-0x1.b72c7p-1f, -0x1.94f984p-7f},
    {-0x1.bd8056p-1f, -0x1.83ccd2p-7f},
    {-0x1.c38f8ap-1f, -0x1.72645p-7f},
    {-0x1.c9591ap-1f, -0x1.60c2b2p-7f},
    {-0x1.cedc26p-1f, -0x1.4eeaaep-7f},
    {-0x1.d417dp-1f, -0x1.3cdf06p-7f},
    {-0x1.d90b4cp-1f, -0x1.2aa28p-7f},
    {-0x1.ddb5d6p-1f, -0x1.1837ecp-7f},
    {-0x1.e216b6p-1f, -0x1.05a222p-7f},
    {-0x1.e62d3ep-1f, -0x1.e5c802p-8f},
    {-0x1.e9f8cep-1f, -0x1.c000d6p-8f},
    {-0x1.ed78dp-1f, -0x1.99f494p-8f},
    {-0x1.f0acbap-1f, -0x1.73a91cp-8f},
    {-0x1.f3940cp-1f, -0x1.4d2452p-8f},
    {-0x1.f62e54p-1f, -0x1.266c2ap-8f},
    {-0x1.f87b2cp-1f, -0x1.ff0d36p-9f},
    {-0x1.fa7a3ap-1f, -0x1.b0f34ap-9f},
    {-0x1.fc2b2cp-1f, -0x1.62969ap-9f},
    {-0x1.fd8dc4p-1f, -0x1.14033cp-9f},
    {-0x1.fea1c6p-1f, -0x1.8a8a9cp-10f},
    {-0x1.ff670cp-1f, -0x1.d9a3d4p-11f},
    {-0x1.ffdd76p-1f, -0x1.14580ep-12f},
    {-0x1p+0f, 0x1.14580ep-12f},
    {-0x1.ffdd76p-1f, 0x1.d9a3d4p-11f},
    {-0x1.ff670cp-1f, 0x1.8a8a9cp-10f},
    {-0x1.fea1c6p-1f, 0x1.14033cp-9f},
    {-0x1.fd8dc4p-1f, 0x1.62969ap-9f},
    {-0x1.fc2b2cp-1f, 0x1.b0f34ap-9f},
    {-0x1.fa7a3ap-1f, 0x1.ff0d36p-9f},
    {-0x1.f87b2cp-1f, 0x1.266c2ap-8f},
    {-0x1.f62e54p-1f, 0x1.4d2452p-8f},
    {-0x1.f3940cp-1f, 0x1.73a91cp-8f},
    {-0x1.f0acbap-1f, 0x1.99f494p-8f},
    {-0x1.ed78dp-1f, 0x1.c000d6p-8f},
    {-0x1.e9f8cep-1f, 0x1.e5c802p-8f},
    {-0x1.e62d3ep-1f, 0x1.05a222p-7f},
    {-0x1.e216b6p-1f, 0x1.1837ecp-7f},
    {-0x1.ddb5d6p-1f, 0x1.2aa28p-7f},
    {-0x1.d90b4cp-1f, 0x1.3cdf06p-7f},
    {-0x1.d417dp-1f, 0x1.4eeaaep-7f},
    {-0x1.cedc26p-1f, 0x1.60c2b2p-7f},
    {-0x1.c9591ap-1f, 0x1.72645p-7f},
    {-0x1.c38f8ap-1f, 0x1.83ccd2p-7f},
    {-0x1.bd8056p-1f, 0x1.94f984p-7f},
    {-0x1.b72c7p-1f, 0x1.a5e7c6p-7f},
    {-0x1.b094dp-1f, 0x1.b694f6p-7f},
    {-0x1.a9ba7ep-1f, 0x1.c6fe86p-7f},
    {-0x1.a29e84p-1f, 0x1.d721ecp-7f},
    {-0x1.9b41fcp-1f, 0x1.e6fcaap-7f},
    {-0x1.93a608p-1f, 0x1.f68c52p-7f},
    {-0x1.8bcbd8p-1f, 0x1.02e73ep-6f},
    {-0x1.83b49ep-1f, 0x1.0a6064p-6f},
    {-0x1.7b619ap-1f, 0x1.11b078p-6f},
    {-0x1.72d416p-1f, 0x1.18d658p-6f},
    {-0x1.6a0d64p-1f, 0x1.1fd0eap-6f},
    {-0x1.610edcp-1f, 0x1.269f18p-6f},
    {-0x1.57d9e4p-1f, 0x1.2d3fd8p-6f},
    {-0x1.4e6fe6p-1f, 0x1.33b226p-6f},
    {-0x1.44d254p-1f, 0x1.39f4fep-6f},
    {-0x1.3b02acp-1f, 0x1.40076ep-6f},
    {-0x1.31027p-1f, 0x1.45e884p-6f},
    {-0x1.26d32cp-1f, 0x1.4b9758p-6f},
    {-0x1.1c7672p-1f, 0x1.51130cp-6f},
    {-0x1.11eddap-1f, 0x1.565ac2p-6f},
    {-0x1.073b04p-1f, 0x1.5b6dbp-6f},
    {-0x1.f8bf2cp-2f, 0x1.604b0ap-6f},
    {-0x1.e2ba7ap-2f, 0x1.64f21p-6f},
    {-0x1.cc6b5ap-2f, 0x1.69620cp-6f},
    {-0x1.b5d538p-2f, 0x1.6d9a4cp-6f},
    {-0x1.9efb94p-2f, 0x1.719a2cp-6f},
    {-0x1.87e1f2p-2f, 0x1.75611p-6f},
    {-0x1.708bep-2f, 0x1.78ee5ep-6f},
    {-0x1.58fcfap-2f, 0x1.7c418cp-6f},
    {-0x1.4138e2p-2f, 0x1.7f5a18p-6f},
    {-0x1.29434p-2f, 0x1.823784p-6f},
    {-0x1.111fc8p-2f, 0x1.84d964p-6f},
    {-0x1.f1a464p-3f, 0x1.873f4ep-6f},
    {-0x1.c0bc7ap-3f, 0x1.8968e2p-6f},
    {-0x1.8f8f5ep-3f, 0x1.8b55ccp-6f},
    {-0x1.5e24a4p-3f, 0x1.8d05bep-6f},
    {-0x1.2c83ecp-3f, 0x1.8e7878p-6f},
    {-0x1.f569bap-4f, 0x1.8fadcp-6f},
    {-0x1.917e4ap-4f, 0x1.90a566p-6f},
    {-0x1.2d54fp-4f, 0x1.915f46p-6f},
    {-0x1.91fa4p-5f, 0x1.91db3ep-6f},
    {-0x1.92194p-6f, 0x1.92194p-6f},
};

// AArch64's conversion of a float to a 32-bit integer (fcvtzs): toward zero, held to the integer's range, and 0 for
// NaN.
int32_t ToInt32(float x)
{
    if (std::isnan(x))
    {
        return 0;
    }
    if (x <= -2147483648.0f)
    {
        return INT32_MIN;
    }
    if (x >= 2147483648.0f)
    {
        return INT32_MAX;
    }

    return static_cast<int32_t>(x);
}

// AArch64's conversion of a float to an unsigned 32-bit integer with `fraction_bits` fraction bits (fcvtzu): toward
// zero, held to the integer's range, and 0 for NaN.
uint32_t ToFixedU32(float x, int fraction_bits)
{
    const double scaled = std::ldexp(static_cast<double>(x), fraction_bits);
    if (!(scaled >= 1.0))
    {
        return 0;
    }
    if (scaled >= 4294967296.0)
    {
        return 0xFFFFFFFF;
    }

    return static_cast<uint32_t>(scaled);
}

// The sine of a turn's fraction: the turn in 2^32 steps, the table's entry for the top 8 bits, and its step to the next
// entry times the low 24 bits. The product and the sum are each rounded (CurveSine, 0xc19c10).
float CurveSine(float turn)
{
    const uint32_t step = ToFixedU32(turn, 32);
    const float fraction = static_cast<float>(step & 0xFFFFFF) * 0x1p-24f;
    const float* entry = kSineTable[step >> 24];
    const float rise = fraction * entry[1];
    return entry[0] + rise;
}

// Four straight lines through 0, 1, 0, -1 and 0. The product and the sum are each rounded (CurveTriangle, 0xc19c50).
// The line is the quarter of the turn. The game doesn't check it: the fifth line, for a turn of 1, is the first.
float CurveTriangle(float turn)
{
    constexpr float kSlopes[5] = {4.0f, -4.0f, -4.0f, 4.0f, 4.0f};
    constexpr float kOffsets[5] = {0.0f, 2.0f, 2.0f, -4.0f, 0.0f};
    const uint32_t line = std::min<uint32_t>(ToFixedU32(turn, 2), 4);
    const float rise = kSlopes[line] * turn;
    return rise + kOffsets[line];
}

} // namespace

float PitchRatio(int pitch)
{
    // Whole octaves first, then semitones and 1/256 semitones from the tables. Doubling and halving are exact.
    int octave = 0;
    if (pitch < 0)
    {
        const int down = (-1 - pitch) / 3072 + 1;
        octave = -down;
        pitch += down * 3072;
    }
    if (pitch >= 3072)
    {
        const int up = (pitch - 3072) / 3072 + 1;
        octave += up;
        pitch -= up * 3072;
    }

    float ratio = 1.0f;
    for (int i = 0; i < octave; i++)
    {
        ratio += ratio;
    }
    for (int i = 0; i > octave; i--)
    {
        ratio *= 0.5f;
    }

    const int semitone = pitch / 256;
    const int fine = pitch - semitone * 256;
    if (semitone != 0)
    {
        ratio *= kSemitoneTable[semitone];
    }
    if (fine != 0)
    {
        ratio *= kFineTable[fine];
    }

    return ratio;
}

void Lfo::Set(uint8_t depth, float speed, uint8_t range, uint32_t delay_ms, LfoCurve curve, uint8_t phase)
{
    // The track stores the MML depth over 128 (MmlParser::CommandProc, 0xc21330).
    depth_ = static_cast<float>(depth) * 0.0078125f;
    speed_ = speed;
    range_ = range;
    delay_ = delay_ms;
    curve_ = curve;
    phase_ = phase;
}

void Lfo::Update(int msec)
{
    if (delay_counter_ < delay_)
    {
        if (delay_counter_ + static_cast<uint32_t>(msec) <= delay_)
        {
            delay_counter_ += static_cast<uint32_t>(msec);
            return;
        }

        msec -= static_cast<int>(delay_ - delay_counter_);
        delay_counter_ = delay_;
    }
    if (!(speed_ > 0.0f))
    {
        return;
    }

    // The cycle starts at the phase and advances by the speed times the seconds. The cycle wraps to its fractional
    // part.
    if (!started_)
    {
        started_ = true;
        counter_ = static_cast<float>(phase_) / 127.0f;
    }

    const float counter = counter_ + speed_ * static_cast<float>(msec) / 1000.0f;
    counter_ = counter - static_cast<float>(ToInt32(counter));
}

float Lfo::Value() const
{
    if (depth_ == 0.0f || delay_counter_ < delay_)
    {
        return 0.0f;
    }

    float value = 0.0f;
    switch (curve_)
    {
    case LfoCurve::kSine:
        value = CurveSine(counter_);
        break;
    case LfoCurve::kTriangle:
        value = CurveTriangle(counter_);
        break;
    case LfoCurve::kSaw:
        value = counter_;
        break;
    case LfoCurve::kSquare:
        value = counter_ - 0.5f >= 0.0f ? 1.0f : 0.0f;
        break;
    case LfoCurve::kRandom:
        break;
    default:
        value = 1.0f;
        break;
    }

    return value * depth_ * static_cast<float>(range_);
}

} // namespace hacsf2
