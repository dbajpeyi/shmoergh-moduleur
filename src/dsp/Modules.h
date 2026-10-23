#pragma once

#include "Common.h"

// Behavioural models of the analog Moduleur modules. Component values quoted in
// comments refer to the KiCad schematics in shmoergh/moduleur.
namespace moduleur
{
//==============================================================================
// Emits a signal one sample late so that band-limited step corrections can be
// applied to both sides of a discontinuity (PolyBLEP with look-back).
struct BlepLine
{
    float pending = 0.0f, corrNow = 0.0f;

    // A step of height h happened d (0..1) samples before the current sample.
    void step (float h, float d)
    {
        const float half = 0.5f * h;
        pending += half * d * d;
        corrNow -= half * (1.0f - d) * (1.0f - d);
    }

    float emit (float naiveNow)
    {
        const float out = pending;
        pending = naiveNow + corrNow;
        corrNow = 0.0f;
        return out;
    }
};

//==============================================================================
// 02-vco: saw core (LM311 reset comparator + J111 discharge), diode triangle
// shaper, comparator square with PW control, CD4040 /2 sub, transistor noise.
class Vco
{
public:
    enum Wave { Tri = 0, Saw = 1, Sqr = 2 };

    struct Controls
    {
        int wave = Saw;
        float octave = 0.0f;   // RV_TUNE1 position 0..1
        float fine = 0.5f;     // RV_FINE1 position 0..1
        float width = 0.5f;    // RV_PWM1 position 0..1
    };

    struct Inputs { float pitch = 0, fm = 0, pwm = 0, sync = 0; };
    struct Outputs { float out = 0, sub = 0, noise = 0; };

    void prepare (float sampleRate, uint32_t seed)
    {
        sr = sampleRate;
        rng = Rng (seed);
        noiseLp.setCutoff (14000.0f, sr);
        noiseHp.setCutoff (2.0f, sr);
        driftLp.setCutoff (0.15f, sr);
        phase = rng.uniform();
    }

    Outputs process (const Controls& c, const Inputs& in)
    {
        // Pitch summing node (U1A): keyboard CV, TUNE pot (0..+5.2 V via 130k/100k
        // divider) and FINE pot (+/-1.07 V via 510k/100k/510k), all through 100k.
        const float tuneV = c.octave * 12.0f * 100.0f / 230.0f;
        const float fineV = (c.fine - 0.5f) * 2.0f * 1.07f;
        drift = driftLp.lowpass (rng.bipolar() * 40.0f);   // a cent or two of analog wander
        const float pitchV = in.pitch + tuneV + fineV + drift * 0.02f;

        // FM goes through 1M into the reference current node (12 V / 1M): linear FM.
        const float fmScale = std::max (0.02f, 1.0f + in.fm / 12.0f);
        float freq = kBaseHz * std::exp2 (clampf (pitchV, -6.0f, 11.0f)) * fmScale;
        freq = clampf (freq, 0.05f, sr * 0.45f);
        const float dt = freq / sr;

        // Square threshold (U5B): -(WIDTH(+/-12V) + PWM) * 100k/220k, compared to the saw.
        const float widthV = (c.width - 0.5f) * 24.0f;
        const float thrV = clampf (-(widthV + in.pwm) * (100.0f / 220.0f), -4.5f, 4.5f);
        const float p0 = 0.5f * (thrV / 5.0f + 1.0f);   // phase where the square goes high

        // Hard sync: U2B fires when SYNC rises through about -1.09 V.
        float syncFrac = -1.0f;
        const bool syncHigh = in.sync > (syncWasHigh ? -1.3f : -0.9f);
        if (syncHigh && ! syncWasHigh)
        {
            const float denom = in.sync - lastSync;
            syncFrac = denom > 1.0e-6f ? clampf ((in.sync - (-1.09f)) / denom, 0.0f, 0.999f) : 0.0f;
        }
        syncWasHigh = syncHigh;
        lastSync = in.sync;

        float p = phase + dt;

        auto crossRising = [&] (float from, float to, float frac0)
        {
            // square rising edge between phases [from, to) of this sample
            if (from < p0 && to >= p0)
            {
                sqrHigh = true;
                sqr.step (2.0f, frac0 + (to - p0) / dt);
            }
        };

        if (syncFrac >= 0.0f)
        {
            // phase at the reset instant
            const float pEv = std::min (phase + dt * (1.0f - syncFrac), 1.0f);
            crossRising (phase, pEv, syncFrac);
            resetDiscontinuity (pEv, syncFrac);
            p = dt * syncFrac;
            crossRising (0.0f, p, 0.0f);
        }
        else
        {
            if (p >= 1.0f)
            {
                crossRising (phase, 1.0f, (p - 1.0f) / dt);
                const float d = (p - 1.0f) / dt;
                resetDiscontinuity (1.0f, d);
                p -= 1.0f;
                crossRising (0.0f, p, 0.0f);
            }
            else
            {
                crossRising (phase, p, 0.0f);
            }
        }
        phase = p;

        // The square is a comparator: if PWM moved the threshold past the phase,
        // it switches immediately.
        if (! sqrHigh && phase >= p0) { sqrHigh = true; sqr.step (2.0f, 0.0f); }
        else if (sqrHigh && phase < p0) { sqrHigh = false; sqr.step (-2.0f, 0.0f); }

        // Naive waveforms (normalised to +/-1)
        const float sawN = 2.0f * phase - 1.0f;
        const float triN = 1.0f - 2.0f * std::abs (sawN);
        const float sqrN = sqrHigh ? 1.0f : -1.0f;
        const float subN = subHigh ? 1.0f : -1.0f;

        const float sawV = saw.emit (sawN) * 5.0f;    // 10 Vpp (R50 note)
        const float triV = tri.emit (-triN) * 5.0f;
        const float sqrV = sqr.emit (sqrN) * 4.6f;
        const float subV = sub.emit (subN) * 5.0f;    // CD4040 Q0, AC coupled ~10 Vpp

        // Avalanche-ish transistor noise, x148 gain, AC coupled.
        const float n = noiseHp.highpass (noiseLp.lowpass (rng.gaussianish())) * 2.1f;

        Outputs o;
        o.out = c.wave == Tri ? triV : (c.wave == Saw ? sawV : sqrV);
        o.sub = subV;
        o.noise = clampf (n, -10.0f, 10.0f);
        return o;
    }

    static constexpr float kBaseHz = 32.703196f; // 0 V = C1 (tuning procedure, wiki/Calibration)

private:
    // Saw falls to -1, square drops low, triangle jumps, sub flip-flop toggles.
    void resetDiscontinuity (float pEv, float d)
    {
        const float sawBefore = 2.0f * pEv - 1.0f;
        saw.step (-1.0f - sawBefore, d);
        const float triBefore = -(1.0f - 2.0f * std::abs (sawBefore));
        tri.step (1.0f - triBefore, d);   // tri(0) = -(1 - 2) = +1
        if (sqrHigh)
        {
            sqrHigh = false;
            sqr.step (-2.0f, d);
        }
        subHigh = ! subHigh;
        sub.step (subHigh ? 2.0f : -2.0f, d);
    }

    float sr = 48000.0f;
    float phase = 0.0f;
    bool sqrHigh = false, subHigh = false, syncWasHigh = false;
    float lastSync = 0.0f, drift = 0.0f;
    BlepLine saw, tri, sqr, sub;
    Rng rng;
    OnePole noiseLp, noiseHp, driftLp;
};

//==============================================================================
// 03-sidechain-mixer: 3 DC-coupled channels into a diff-pair VCA, channel 4 with
// gain (A = 1..11) feeding a peak detector -> threshold subtractor -> VCA gain
// reduction, then a 2-channel output mixer with main volume.
class SidechainMixer
{
public:
    struct Controls { float in1 = 0, in2 = 0, in3 = 0, in4 = 0, gain = 0, threshold = 1, main = 0.8f; };
    struct Inputs { float in1 = 0, in2 = 0, in3 = 0, in4 = 0; };

    void prepare (float sampleRate) { sr = sampleRate; }

    float process (const Controls& c, const Inputs& in)
    {
        const float sub3 = in.in1 * audioTaper (c.in1) + in.in2 * audioTaper (c.in2) + in.in3 * audioTaper (c.in3);

        // Channel 4 gain stage (U3A): 1 + RV_CH_4_GAIN / 10k
        const float ch4 = railClip (in.in4 * (1.0f + 10.0f * audioTaper (c.gain)));

        // Peak detector (U3B + D2 + C9 10u with attack/release trimmers)
        const float rect = std::max (0.0f, ch4);
        const float tau = rect > peak ? 0.004f : 0.18f;
        peak += (rect - peak) * (1.0f - std::exp (-1.0f / (tau * sr)));

        // Threshold (0..6 V) minus peak; only negative values reach the VCA CV (D1).
        const float thresholdV = 6.0f * audioTaper (c.threshold);
        const float reduction = std::min (0.0f, thresholdV - peak);
        gainReduction = clampf (1.0f + reduction / 4.0f, 0.0f, 1.0f);

        const float vca = railClip (sub3) * gainReduction;
        const float out = (vca + ch4 * audioTaper (c.in4)) * audioTaper (c.main);
        return railClip (out);
    }

    float getGainReduction() const { return gainReduction; }

private:
    float sr = 48000.0f, peak = 0.0f, gainReduction = 1.0f;
};

//==============================================================================
// 04-vcf: 3-pole diode ladder (D1..D10, C10/C12/C14 1n) driven by a diff pair,
// with separate LP input (into the ladder) and HP input (into the capacitor
// reference). Solved per sample with a trapezoidal (TPT) implicit step where the
// diode/diff-pair tanh non-linearities are linearised from the previous state.
class DiodeVcf
{
public:
    struct Controls { float cutoff = 0.5f, reso = 0.0f, cv1 = 0.5f, cv2 = 0.5f; };
    struct Inputs { float lp = 0, hp = 0, cv1 = 0, cv2 = 0; };

    void prepare (float sampleRate)
    {
        sr = sampleRate;
        dcBlock.setCutoff (4.0f, sr);
        for (auto& v : s) v = 0.0f;
        for (auto& v : vPrev) v = 0.0f;
    }

    float process (const Controls& c, const Inputs& in)
    {
        // CV summing (U2A/U2B): main cutoff 0..12 V plus two attenuverted CVs.
        const float cvV = c.cutoff * 12.0f + in.cv1 * attenuverter (c.cv1) + in.cv2 * attenuverter (c.cv2);
        const float fc = clampf (16.0f * std::exp2 (cvV * 0.85f), 8.0f, std::min (22000.0f, sr * 0.3f));

        // Self-oscillation of the 3-pole diode ladder sits at sqrt(6) * wc.
        const double g = std::tan ((double) kPi * fc / (2.4494897 * sr));
        // Resonance loop gain: linear oscillation threshold is k = 29; RV_MAX_RESO
        // trims the top end just past that ("smooth to crazy").
        const float resoPos = clampf (c.reso, 0.0f, 1.0f);
        const double k = 31.5 * (double) (resoPos * resoPos * (3.0f - 2.0f * resoPos)) ;

        const double u = in.hp / kScale;     // HP input drives the capacitor reference
        // LP input into the diff pair, plus the circuit's own noise floor (lets the
        // filter start self-oscillating like the hardware does).
        const double x0 = (in.lp + 2.0e-4f * noise.bipolar()) / kScale;

        // Conductances from the previous solution (tanh(z)/z).
        const double c0 = tanhc (x0 - k * vPrev[2]);
        const double a = tanhc (vPrev[0] - vPrev[1]);
        const double b = tanhc (vPrev[1] - vPrev[2]);
        const double e = tanhc (vPrev[2]);

        // M v = r  (see derivation in docs/DSP.md)
        const double m00 = 1.0 + g * a, m01 = -g * a, m02 = g * c0 * k;
        const double m10 = -g * a, m11 = 1.0 + g * a + g * b, m12 = -g * b;
        const double m20 = 0.0, m21 = -g * b, m22 = 1.0 + g * b + g * e;
        const double r0 = s[0] + u + g * c0 * x0;
        const double r1 = s[1] + u;
        const double r2 = s[2] + u;

        const double det = m00 * (m11 * m22 - m12 * m21) - m01 * (m10 * m22 - m12 * m20) + m02 * (m10 * m21 - m11 * m20);
        const double inv = 1.0 / det;
        const double v0 = (r0 * (m11 * m22 - m12 * m21) - m01 * (r1 * m22 - m12 * r2) + m02 * (r1 * m21 - m11 * r2)) * inv;
        const double v1 = (m00 * (r1 * m22 - m12 * r2) - r0 * (m10 * m22 - m12 * m20) + m02 * (m10 * r2 - r1 * m20)) * inv;
        const double v2 = (m00 * (m11 * r2 - r1 * m21) - m01 * (m10 * r2 - r1 * m20) + r0 * (m10 * m21 - m11 * m20)) * inv;

        // TPT state update: s = 2 * c_new - s, with c = v - u
        s[0] = 2.0 * (v0 - u) - s[0];
        s[1] = 2.0 * (v1 - u) - s[1];
        s[2] = 2.0 * (v2 - u) - s[2];
        vPrev[0] = v0; vPrev[1] = v1; vPrev[2] = v2;

        // Output buffer chain (U4A x3.1, U4B x10) is AC coupled; resonance eats
        // passband level like the hardware, partially made up here.
        const float makeup = 1.0f + 0.7f * (float) k;
        const float y = dcBlock.highpass ((float) v2 * kScale * makeup);
        return railClip (y);
    }

private:
    static double tanhc (double z)
    {
        const double az = std::abs (z);
        if (az < 1.0e-4)
            return 1.0;
        return std::tanh (z) / z;
    }

    // Logarithmic attenuverter (U1A/U1B with R4/R5 shaping): centre = off.
    static float attenuverter (float pos)
    {
        const float p = clampf (pos * 2.0f - 1.0f, -1.0f, 1.0f);
        return std::copysign (std::pow (std::abs (p), 1.6f), p);
    }

    static constexpr float kScale = 2.2f; // volts per normalised ladder unit
    float sr = 48000.0f;
    double s[3] {}, vPrev[3] {};
    OnePole dcBlock;
    Rng noise { 0x5eed1234u };
};

//==============================================================================
// 05-adsr-vca: LM324 RC envelope (10u main cap, A500k-1M timing pots) and a
// diff-pair VCA whose tail current follows the CV.
class AdsrVca
{
public:
    struct Controls { float attack = 0.1f, decay = 0.4f, sustain = 0.7f, release = 0.4f; };
    struct Inputs { float gate = 0, vcaIn = 0, vcaCv = 0; bool cvIsOwnEnvelope = false; };
    struct Outputs { float env = 0, vca = 0; };

    void prepare (float sampleRate) { sr = sampleRate; }

    Outputs process (const Controls& c, const Inputs& in)
    {
        // U1A gate comparator, 0.5 V reference with hysteresis (R5 1M).
        const bool gateHigh = in.gate > (gateOn ? 0.35f : 0.5f);
        if (gateHigh && ! gateOn)
            attacking = true;            // C3 trigger pulse re-arms the attack
        gateOn = gateHigh;

        float target, tau;
        if (! gateOn)
        {
            attacking = false;
            target = 0.0f;
            tau = timeConstant (c.release, 0.0005f);
        }
        else if (attacking)
        {
            // Charges towards the op-amp rail; the Schmitt trigger flips near the top.
            target = kPeak * 1.35f;
            tau = timeConstant (c.attack, 0.0002f);
        }
        else
        {
            target = kPeak * clampf (c.sustain, 0.0f, 1.0f);
            tau = timeConstant (c.decay, 0.0005f);
        }

        const float a = 1.0f - std::exp (-1.0f / (tau * sr));
        level += (target - level) * a;
        if (attacking && level >= kPeak)
        {
            level = kPeak;
            attacking = false;
        }

        Outputs o;
        o.env = level / kPeak * 8.0f;    // RV_OUT_GAIN1 set to the recommended 8 V

        // VCA: CV through 200k into U2A (33k fb) sets the tail current; bias
        // trimmed so 0 V is silent and ~8 V is unity.
        const float cv = in.cvIsOwnEnvelope ? o.env : in.vcaCv;   // JP_DEFAULT_ENV_TO_VCA
        const float g = clampf (cv, 0.0f, 10.5f) / 8.0f;
        const float x = in.vcaIn;
        o.vca = railClip (x * g * (1.0f - 0.015f * x * x / (1.0f + 0.015f * x * x)));
        return o;
    }

    float ledLevel() const { return level / kPeak; }

private:
    // 10u * (Rmin + A-taper pot up to ~1M): ~0.2 ms .. 10 s time constants
    static float timeConstant (float pos, float minTau)
    {
        return minTau + 10.0f * audioTaper (pos);
    }

    static constexpr float kPeak = 6.8f; // buffered cap voltage at the top of the attack
    float sr = 48000.0f, level = 0.0f;
    bool gateOn = false, attacking = false;
};

//==============================================================================
// 06-utils-output / bitcrusher: JFET sample & hold clocked by a relaxation
// oscillator (160 Hz .. 19 kHz with the A1M CRUSH pot) unless TRIGGER is patched.
class Crusher
{
public:
    void prepare (float sampleRate) { sr = sampleRate; }

    float process (float crushPos, float input, bool triggerPatched, float trigger)
    {
        bool sample = false;
        if (triggerPatched)
        {
            // C18/R21 differentiator + U2B comparator at ~3 V: rising edges only
            const bool high = trigger > (trigHigh ? 2.0f : 3.0f);
            sample = high && ! trigHigh;
            trigHigh = high;
        }
        else
        {
            const float f = expMap (1.0f - crushPos, 160.0f, 19000.0f);
            clockPhase += f / sr;
            if (clockPhase >= 1.0f)
            {
                clockPhase -= std::floor (clockPhase);
                sample = true;
            }
        }
        if (sample)
            held = clampf (input, -11.4f, 11.4f);
        return held;
    }

    float internalClock() const { return clockPhase < 0.5f ? 11.0f : -11.0f; }

private:
    float sr = 48000.0f, clockPhase = 0.0f, held = 0.0f;
    bool trigHigh = false;
};

// Triangle/square LFO (U201A Schmitt + U201B integrator), ~0.1..200 Hz.
class Lfo
{
public:
    void prepare (float sampleRate) { sr = sampleRate; }

    void process (float ratePos, float& tri, float& sqr)
    {
        const float f = expMap (ratePos, 0.1f, 200.0f);
        const float slope = 4.0f * f / sr;
        value += rising ? slope : -slope;
        if (value >= 1.0f) { value = 2.0f - value; rising = false; }
        if (value <= -1.0f) { value = -2.0f - value; rising = true; }
        tri = value * 4.6f;
        sqr = rising ? 4.9f : -4.9f;
    }

    float ledLevel() const { return std::max (0.0f, value); }

private:
    float sr = 48000.0f, value = 0.0f;
    bool rising = true;
};

// Output: line-level trim (10 Vpp -> 2.8 Vpp), AC coupling, DRIVE gain (1..5.5x),
// red-LED clipper, VOLUME (A10k) and buffer.
class OutputStage
{
public:
    void prepare (float sampleRate)
    {
        sr = sampleRate;
        acCouple.setCutoff (1.6f, sr);
    }

    float process (float input, float drivePos, float volumePos)
    {
        float x = acCouple.highpass (input * 0.28f);
        x *= 1.0f + 4.5f * clampf (drivePos, 0.0f, 1.0f);
        x = railClip (x);

        // 1k into anti-parallel red LEDs (~1.75 V knee)
        const float knee = 1.75f;
        const float r = std::abs (x) / knee;
        const float clipped = x / std::pow (1.0f + std::pow (r, 3.0f), 1.0f / 3.0f);
        clip = std::max (clip * 0.9995f, clampf ((std::abs (x) - 1.3f) / 1.5f, 0.0f, 1.0f));

        return clipped * audioTaper (volumePos);
    }

    float ledLevel() const { return clip; }

private:
    float sr = 48000.0f, clip = 0.0f;
    OnePole acCouple;
};
} // namespace moduleur
