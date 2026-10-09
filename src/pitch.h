// SPDX-License-Identifier: MIT

// nn::atk's pitch ratios (1/256-semitone units) and LFOs.

#pragma once

#include <cstdint>

namespace hacsf2
{

// Convert `pitch` from 1/256 semitones to a frequency ratio, following Util::CalcPitchRatio (0xc1a580). Apply whole
// octaves, then single-precision 2^(i/12) and 2^(i/3072) tables for the remainder.
float PitchRatio(int pitch);

// nn::atk's LFO curves (CurveLfoParam::curve).
enum class LfoCurve : uint8_t
{
    kSine = 0,
    kTriangle = 1,
    kSaw = 2,
    kSquare = 3,
    kRandom = 4,
};

// nn::atk's channel LFO (CurveLfo): a delayed periodic curve, advanced a millisecond at a time (Update, 0xc19d20;
// GetValue, 0xc19dc0).
class Lfo
{
public:
    // Set MML depth and range, the speed in Hz, the delay in milliseconds, the curve and the phase the cycle starts at,
    // in 127ths of a cycle.
    void Set(uint8_t depth, float speed, uint8_t range, uint32_t delay_ms, LfoCurve curve = LfoCurve::kSine,
             uint8_t phase = 0);

    // Advance by `msec` milliseconds. Use up the delay first.
    void Update(int msec);

    // Return the curve times depth / 128 times range, in semitones for a pitch LFO. The random curve isn't modelled and
    // gives 0. A curve that the library has no function for gives 1. Those are curves 5 and up. A game would register
    // functions for them.
    float Value() const;

private:
    float depth_ = 0.0f;  // MML depth / 128
    float speed_ = 6.25f; // Hz
    uint32_t delay_ = 0;  // milliseconds
    uint8_t range_ = 1;   // semitones, for a pitch LFO
    LfoCurve curve_ = LfoCurve::kSine;
    uint8_t phase_ = 0;          // the cycle's start, in 127ths
    uint32_t delay_counter_ = 0; // elapsed delay in milliseconds
    float counter_ = 0.0f;       // the place in the cycle, 0-1
    bool started_ = false;       // the cycle has begun at its phase
};

} // namespace hacsf2
