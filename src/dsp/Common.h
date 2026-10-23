#pragma once

#include <cmath>
#include <cstdint>
#include <algorithm>

// Small DSP helpers shared by the Moduleur module models. Everything in the
// rack works in volts, like the hardware: audio is roughly +/-5 V, envelopes
// 0..8 V, pitch 1 V/oct.
namespace moduleur
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;

inline float clampf (float x, float lo, float hi) { return std::min (hi, std::max (lo, x)); }

// Op-amp rail saturation: linear up to `knee`, then a smooth bend into `rail`.
inline float railClip (float x, float rail = 10.5f, float knee = 8.5f)
{
    const float ax = std::abs (x);
    if (ax <= knee)
        return x;
    const float span = rail - knee;
    const float over = (ax - knee) / span;
    return std::copysign (knee + span * std::tanh (over), x);
}

// Audio ("A", logarithmic) potentiometer taper: ~10% output at mid travel.
inline float audioTaper (float pos)
{
    pos = clampf (pos, 0.0f, 1.0f);
    return (std::pow (10.0f, 2.0f * pos) - 1.0f) / 99.0f;
}

// Maps 0..1 onto lo..hi exponentially.
inline float expMap (float pos, float lo, float hi)
{
    return lo * std::pow (hi / lo, clampf (pos, 0.0f, 1.0f));
}

// Band-limited step correction (PolyBLEP). t: phase in [0,1), dt: phase increment.
inline float polyBlep (float t, float dt)
{
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt)
    {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

// xorshift32 noise source (deterministic, real-time safe).
struct Rng
{
    uint32_t state = 0x9e3779b9u;
    explicit Rng (uint32_t seed = 0x9e3779b9u) : state (seed ? seed : 1u) {}
    uint32_t next()
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }
    float uniform() { return (float) (next() >> 8) * (1.0f / 16777216.0f); }          // [0,1)
    float bipolar() { return uniform() * 2.0f - 1.0f; }                                 // [-1,1)
    float gaussianish() { return (uniform() + uniform() + uniform() + uniform() - 2.0f) * 1.7320508f; }
};

// One-pole low/high pass (TPT form, stable for any cutoff below Nyquist).
struct OnePole
{
    float g = 0.0f, s = 0.0f;
    void setCutoff (float hz, float sampleRate)
    {
        const float wc = std::tan (kPi * std::min (hz, sampleRate * 0.49f) / sampleRate);
        g = wc / (1.0f + wc);
    }
    float lowpass (float x)
    {
        const float v = (x - s) * g;
        const float y = v + s;
        s = y + v;
        return y;
    }
    float highpass (float x) { return x - lowpass (x); }
    void reset (float v = 0.0f) { s = v; }
};

// Exponential (RC) slew towards a target with a given time constant.
struct RcSlew
{
    float value = 0.0f;
    float step (float target, float tauSeconds, float sampleRate)
    {
        const float a = 1.0f - std::exp (-1.0f / (std::max (tauSeconds, 1.0e-6f) * sampleRate));
        value += (target - value) * a;
        return value;
    }
};

// Smooths a control value to avoid zipper noise.
struct ParamSmoother
{
    float value = 0.0f, coeff = 0.0f;
    void prepare (float sampleRate, float timeMs = 8.0f) { coeff = 1.0f - std::exp (-1.0f / (timeMs * 0.001f * sampleRate)); }
    void snap (float v) { value = v; }
    float next (float target) { value += (target - value) * coeff; return value; }
};
} // namespace moduleur
