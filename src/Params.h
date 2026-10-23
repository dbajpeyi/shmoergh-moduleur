#pragma once

#include <array>

#include "dsp/Rack.h"

// One entry per front-panel control. Positions are panel units (see Ports.h).
namespace moduleur
{
enum class ControlKind { Knob, BigKnob, Trimmer, Slider, Selector, Toggle };

struct ParamDef
{
    const char* id;
    const char* name;
    ControlKind kind;
    float defaultValue;
    float x, y;
    int choices = 0;   // Selector only
};

enum ParamIndex : int
{
    P_VCO1_WAVE, P_VCO1_OCTAVE, P_VCO1_FINE, P_VCO1_WIDTH,
    P_VCO2_WAVE, P_VCO2_OCTAVE, P_VCO2_FINE, P_VCO2_WIDTH,
    P_MIX_IN4, P_MIX_GAIN, P_MIX_MAIN, P_MIX_THRESHOLD, P_MIX_IN1, P_MIX_IN2, P_MIX_IN3,
    P_VCF_RESO, P_VCF_CUTOFF, P_VCF_CV1, P_VCF_CV2,
    P_ENV1_ATTACK, P_ENV1_DECAY, P_ENV1_RELEASE, P_ENV1_SUSTAIN,
    P_ENV2_ATTACK, P_ENV2_DECAY, P_ENV2_RELEASE, P_ENV2_SUSTAIN,
    P_CRUSH, P_LFO_RATE, P_VOLUME, P_LFO_AMOUNT, P_DRIVE,
    P_BRAIN_X, P_BRAIN_Y, P_BRAIN_Z,
    P_POWER, P_DEFAULT_PATCH,
    NumParams
};

// clang-format off
inline constexpr std::array<ParamDef, NumParams> kParams {{
    { "vco1_wave",    "OSC 1 Wave",          ControlKind::Selector,  1.0f,   75.6f, 196.5f, 3 },
    { "vco1_octave",  "OSC 1 Octave",        ControlKind::Trimmer,   0.0f,   60.5f, 302.4f },
    { "vco1_fine",    "OSC 1 Fine",          ControlKind::Trimmer,   0.5f,  120.9f, 302.4f },
    { "vco1_width",   "OSC 1 Width",         ControlKind::Trimmer,   0.5f,  181.4f, 302.4f },

    { "vco2_wave",    "OSC 2 Wave",          ControlKind::Selector,  2.0f,  317.5f, 196.5f, 3 },
    { "vco2_octave",  "OSC 2 Octave",        ControlKind::Trimmer,   0.0f,  302.4f, 302.4f },
    { "vco2_fine",    "OSC 2 Fine",          ControlKind::Trimmer,   0.53f, 362.8f, 302.4f },
    { "vco2_width",   "OSC 2 Width",         ControlKind::Trimmer,   0.5f,  423.3f, 302.4f },

    { "mix_in4",      "Mixer In 4 Level",    ControlKind::Trimmer,   0.0f,  544.3f, 211.7f },
    { "mix_gain",     "Mixer In 4 Gain",     ControlKind::Trimmer,   0.0f,  604.7f, 211.7f },
    { "mix_main",     "Mixer Out",           ControlKind::Trimmer,   0.95f, 665.2f, 211.7f },
    { "mix_thresh",   "Sidechain Threshold", ControlKind::Trimmer,   1.0f,  604.7f, 302.4f },
    { "mix_in1",      "Mixer In 1",          ControlKind::Slider,    0.9f, 544.3f, 408.2f },
    { "mix_in2",      "Mixer In 2",          ControlKind::Slider,    0.85f, 604.7f, 408.2f },
    { "mix_in3",      "Mixer In 3",          ControlKind::Slider,    0.0f,  665.2f, 408.2f },

    { "vcf_reso",     "Filter Resonance",    ControlKind::Trimmer,   0.3f,  786.1f, 211.7f },
    { "vcf_cutoff",   "Filter Cutoff",       ControlKind::BigKnob,   0.5f,  892.0f, 196.5f },
    { "vcf_cv1",      "Filter CV 1 Level",   ControlKind::Trimmer,   0.5f,  846.6f, 302.4f },
    { "vcf_cv2",      "Filter CV 2 Level",   ControlKind::Trimmer,   0.72f, 907.1f, 302.4f },

    { "env1_attack",  "Env 1 Attack",        ControlKind::Trimmer,   0.08f,  60.5f, 695.4f },
    { "env1_decay",   "Env 1 Decay",         ControlKind::Trimmer,   0.45f, 120.9f, 695.4f },
    { "env1_release", "Env 1 Release",       ControlKind::Trimmer,   0.25f,   60.5f, 786.1f },
    { "env1_sustain", "Env 1 Sustain",       ControlKind::Slider,    0.65f, 181.4f, 775.2f },

    { "env2_attack",  "Env 2 Attack",        ControlKind::Trimmer,   0.05f, 302.4f, 695.4f },
    { "env2_decay",   "Env 2 Decay",         ControlKind::Trimmer,   0.42f, 362.8f, 695.4f },
    { "env2_release", "Env 2 Release",       ControlKind::Trimmer,   0.25f,  302.4f, 786.1f },
    { "env2_sustain", "Env 2 Sustain",       ControlKind::Slider,    0.25f, 423.3f, 775.2f },

    { "crush",        "Crush",               ControlKind::Trimmer,   0.3f,  544.3f, 695.4f },
    { "lfo_rate",     "LFO Rate",            ControlKind::Trimmer,   0.45f, 604.7f, 695.4f },
    { "volume",       "Volume",              ControlKind::Trimmer,   0.9f,  665.2f, 695.4f },
    { "lfo_amount",   "LFO Amount",          ControlKind::Trimmer,   0.8f,  604.7f, 786.1f },
    { "drive",        "Drive",               ControlKind::Trimmer,   0.2f, 665.2f, 786.1f },

    { "brain_x",      "Brain X",             ControlKind::Trimmer,   0.335f,  786.1f, 695.4f },
    { "brain_y",      "Brain Y",             ControlKind::Trimmer,   0.5f,  846.6f, 695.4f },
    { "brain_z",      "Brain Z",             ControlKind::Trimmer,   0.0f,  907.1f, 695.4f },

    { "power",        "Power",               ControlKind::Toggle,    1.0f,  181.4f, 1149.0f },
    { "default_patch","Default Patch Board", ControlKind::Toggle,    1.0f,    0.0f,    0.0f },
}};
// clang-format on

// Turns raw parameter values (index order above) into the rack's control struct.
inline RackParams makeRackParams (const float* v)
{
    RackParams p;
    p.vco[0] = { (int) (v[P_VCO1_WAVE] + 0.5f), v[P_VCO1_OCTAVE], v[P_VCO1_FINE], v[P_VCO1_WIDTH] };
    p.vco[1] = { (int) (v[P_VCO2_WAVE] + 0.5f), v[P_VCO2_OCTAVE], v[P_VCO2_FINE], v[P_VCO2_WIDTH] };
    p.mix = { v[P_MIX_IN1], v[P_MIX_IN2], v[P_MIX_IN3], v[P_MIX_IN4], v[P_MIX_GAIN], v[P_MIX_THRESHOLD], v[P_MIX_MAIN] };
    p.vcf = { v[P_VCF_CUTOFF], v[P_VCF_RESO], v[P_VCF_CV1], v[P_VCF_CV2] };
    p.env[0] = { v[P_ENV1_ATTACK], v[P_ENV1_DECAY], v[P_ENV1_SUSTAIN], v[P_ENV1_RELEASE] };
    p.env[1] = { v[P_ENV2_ATTACK], v[P_ENV2_DECAY], v[P_ENV2_SUSTAIN], v[P_ENV2_RELEASE] };
    p.crush = v[P_CRUSH];
    p.lfoRate = v[P_LFO_RATE];
    p.volume = v[P_VOLUME];
    p.lfoAmount = v[P_LFO_AMOUNT];
    p.drive = v[P_DRIVE];
    p.brainX = v[P_BRAIN_X];
    p.brainY = v[P_BRAIN_Y];
    p.brainZ = v[P_BRAIN_Z];
    p.power = v[P_POWER] > 0.5f;
    p.defaultPatch = v[P_DEFAULT_PATCH] > 0.5f;
    return p;
}

// Cables present on a fresh instance: Brain (MIDI to CV) drives pitch and gate.
// The default patch board mirrors them to OSC 2 and ENV 2.
struct InitCable { int from, to; };
inline constexpr std::array<InitCable, 2> kInitCables {{
    { BRAIN_OUT1, VCO1_PITCH },
    { BRAIN_PULSE, ENV1_GATE },
}};
} // namespace moduleur
