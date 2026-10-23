#pragma once

#include <array>
#include <atomic>

#include "Brain.h"
#include "Modules.h"
#include "Ports.h"

namespace moduleur
{
// Front-panel control positions (0..1 pot positions, like the hardware).
struct RackParams
{
    Vco::Controls vco[2];
    SidechainMixer::Controls mix;
    DiodeVcf::Controls vcf;
    AdsrVca::Controls env[2];
    float crush = 0.3f, lfoRate = 0.4f, volume = 0.75f, lfoAmount = 0.8f, drive = 0.2f;
    float brainX = 0.5f, brainY = 0.5f, brainZ = 0.0f;
    bool buttonA = false, buttonB = false;
    bool power = true;
    bool defaultPatch = true;
};

// The cables. For every input jack: the output jack it is patched from, or -1.
// Written by the UI thread, read by the audio thread (lock free).
struct Patch
{
    std::array<std::atomic<int>, NumPorts> source;

    Patch() { clear(); }
    void clear() { for (auto& s : source) s.store (-1, std::memory_order_relaxed); }
    int get (int input) const { return source[(size_t) input].load (std::memory_order_relaxed); }
    void set (int input, int output) { source[(size_t) input].store (output, std::memory_order_relaxed); }
    bool outputHasCable (int output) const
    {
        for (int i = 0; i < NumPorts; ++i)
            if (get (i) == output)
                return true;
        return false;
    }
};

// Panel LEDs, published for the editor.
enum Led : int
{
    LED_ENV1, LED_ENV2, LED_LFO, LED_DRIVE,
    LED_BRAIN1, LED_BRAIN2, LED_BRAIN3, LED_BRAIN4, LED_BRAIN5, LED_BRAIN6, LED_BRAIN_A,
    LED_POWER,
    NumLeds
};

class Rack
{
public:
    void prepare (double rackSampleRate);
    void reset();

    // Once per block
    void setParams (const RackParams& p) { params = p; }
    void resolveRouting (const Patch& patch, bool defaultPatchBoard);

    // One sample at the rack rate. Returns the line output in volts.
    float processSample (float extL, float extR);

    // Output scaling from line-level volts to digital full scale.
    static constexpr float kVoltsToDigital = 0.5f;

    Brain brain;
    std::array<float, NumLeds> leds {};
    std::array<float, NumPorts> values {};   // jack voltages (outputs) of the last sample
    std::array<int, NumPorts> route {};      // resolved source for each input

private:
    float in (int port) const
    {
        const int s = route[(size_t) port];
        return s >= 0 ? values[(size_t) s] : 0.0f;
    }

    struct Smoothed
    {
        ParamSmoother s;
        float next (float target) { return s.next (target); }
    };

    double sr = 96000.0;
    RackParams params;
    Vco vco[2];
    SidechainMixer mixer;
    DiodeVcf vcf;
    AdsrVca env[2];
    Crusher crusher;
    Lfo lfo;
    OutputStage output;
    float powerLevel = 1.0f;

    // Smoothed continuous controls (avoid zipper noise)
    enum SmoothId
    {
        S_VCO1_OCT, S_VCO1_FINE, S_VCO1_WIDTH, S_VCO2_OCT, S_VCO2_FINE, S_VCO2_WIDTH,
        S_MIX_1, S_MIX_2, S_MIX_3, S_MIX_4, S_MIX_GAIN, S_MIX_TH, S_MIX_MAIN,
        S_VCF_CUT, S_VCF_RES, S_VCF_CV1, S_VCF_CV2,
        S_SUS1, S_SUS2, S_CRUSH, S_LFO_RATE, S_VOLUME, S_LFO_AMT, S_DRIVE,
        NumSmooth
    };
    std::array<ParamSmoother, NumSmooth> smooth;
    bool firstBlock = true;
};
} // namespace moduleur
