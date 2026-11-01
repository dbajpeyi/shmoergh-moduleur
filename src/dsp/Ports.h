#pragma once

#include <array>
#include <cstdint>

// Every jack on the Moduleur front panel. Positions are in "panel units": the
// coordinate space of the FR4 front panel artwork (968 x 1210), taken from the
// hole centres in enclosure/fr4/01-front/graphics/front-edges.svg.
namespace moduleur
{
enum Port : int
{
    // Oscillator 1
    VCO1_SYNC, VCO1_FM, VCO1_PWM, VCO1_PITCH, VCO1_SUB, VCO1_OUT,
    // Oscillator 2
    VCO2_SYNC, VCO2_FM, VCO2_PWM, VCO2_PITCH, VCO2_NOISE, VCO2_OUT,
    // Mixer & sidechain
    MIX_IN4, MIX_OUT, MIX_IN1, MIX_IN2, MIX_IN3,
    // Filter
    VCF_HP, VCF_CV1, VCF_CV2, VCF_LP, VCF_OUT,
    // Envelope & VCA 1
    ENV1_GATE, ENV1_OUT, VCA1_IN, VCA1_CV, VCA1_OUT,
    // Envelope & VCA 2
    ENV2_GATE, ENV2_OUT, VCA2_IN, VCA2_CV, VCA2_OUT,
    // Crush / LFO / Output
    CRUSH_TRIG, CRUSH_IN, LFO_SQR, OUTPUT_IN, CRUSH_OUT, LFO_TRI, OUTPUT_PHONES,
    // Brain
    BRAIN_IN1, BRAIN_IN2, BRAIN_PULSE_IN, BRAIN_OUT1, BRAIN_OUT2, BRAIN_PULSE,
    // Host audio input (plugin addition, bottom strip)
    EXT_L, EXT_R,

    NumPorts
};

struct PortInfo
{
    const char* id;      // stable identifier used in saved patches
    const char* label;   // human readable
    bool isOutput;
    float x, y;          // panel units
};

// clang-format off
inline constexpr std::array<PortInfo, NumPorts> kPorts {{
    { "vco1.sync",   "OSC 1 Sync",       false,  60.5f, 423.3f },
    { "vco1.fm",     "OSC 1 FM",         false, 120.9f, 423.3f },
    { "vco1.pwm",    "OSC 1 PWM",        false, 181.4f, 423.3f },
    { "vco1.pitch",  "OSC 1 Pitch",      false,  60.5f, 514.0f },
    { "vco1.sub",    "OSC 1 Sub",        true,  120.9f, 514.0f },
    { "vco1.out",    "OSC 1 Out",        true,  181.4f, 514.0f },

    { "vco2.sync",   "OSC 2 Sync",       false, 302.4f, 423.3f },
    { "vco2.fm",     "OSC 2 FM",         false, 362.8f, 423.3f },
    { "vco2.pwm",    "OSC 2 PWM",        false, 423.3f, 423.3f },
    { "vco2.pitch",  "OSC 2 Pitch",      false, 302.4f, 514.0f },
    { "vco2.noise",  "OSC 2 Noise",      true,  362.8f, 514.0f },
    { "vco2.out",    "OSC 2 Out",        true,  423.3f, 514.0f },

    { "mix.in4",     "Mixer In 4",       false, 544.3f, 302.4f },
    { "mix.out",     "Mixer Out",        true,  665.2f, 302.4f },
    { "mix.in1",     "Mixer In 1",       false, 544.3f, 514.0f },
    { "mix.in2",     "Mixer In 2",       false, 604.7f, 514.0f },
    { "mix.in3",     "Mixer In 3",       false, 665.2f, 514.0f },

    { "vcf.hp",      "Filter HPF In",     false, 786.1f, 423.3f },
    { "vcf.cv1",     "Filter Cutoff CV 1",      false, 846.6f, 423.3f },
    { "vcf.cv2",     "Filter Cutoff CV 2",      false, 907.1f, 423.3f },
    { "vcf.lp",      "Filter LPF In",     false, 786.1f, 514.0f },
    { "vcf.out",     "Filter Out",       true,  907.1f, 514.0f },

    { "env1.gate",   "Env 1 Gate",       false,  60.5f, 907.1f },
    { "env1.out",    "Env 1 Out",        true,  120.9f, 907.1f },
    { "vca1.in",     "VCA 1 In",         false,  60.5f, 997.8f },
    { "vca1.cv",     "VCA 1 Control",    false, 120.9f, 997.8f },
    { "vca1.out",    "VCA 1 Out",        true,  181.4f, 997.8f },

    { "env2.gate",   "Env 2 Gate",       false, 302.4f, 907.1f },
    { "env2.out",    "Env 2 Out",        true,  362.8f, 907.1f },
    { "vca2.in",     "VCA 2 In",         false, 302.4f, 997.8f },
    { "vca2.cv",     "VCA 2 Control",    false, 362.8f, 997.8f },
    { "vca2.out",    "VCA 2 Out",        true,  423.3f, 997.8f },

    { "crush.trig",  "Crush Trigger",    false, 544.3f, 786.1f },
    { "crush.in",    "Crush In",         false, 544.3f, 907.1f },
    { "lfo.sqr",     "LFO Square",       true,  604.7f, 907.1f },
    { "output.in",   "Output Audio In",   false, 665.2f, 907.1f },
    { "crush.out",   "Crush Out",        true,  544.3f, 997.8f },
    { "lfo.tri",     "LFO Triangle",     true,  604.7f, 997.8f },
    { "output.hp",   "Headphones",       true,  665.2f, 997.8f },

    { "brain.in1",   "Brain CV In 1",       false, 786.1f, 907.1f },
    { "brain.in2",   "Brain CV In 2",       false, 846.6f, 907.1f },
    { "brain.pin",   "Brain Pulse In",   false, 907.1f, 907.1f },
    { "brain.out1",  "Brain CV Out 1",      true,  786.1f, 997.8f },
    { "brain.out2",  "Brain CV Out 2",      true,  846.6f, 997.8f },
    { "brain.pulse", "Brain Pulse Out",  true,  907.1f, 997.8f },

    { "ext.l",       "Host In L (sidechain)",        true,  302.4f, 1149.0f },
    { "ext.r",       "Host In R (sidechain)",        true,  362.8f, 1149.0f },
}};
// clang-format on

inline bool isOutput (int p) { return kPorts[(size_t) p].isOutput; }

// Normalisations (switched-jack defaults). These reproduce the passive
// "default patch" PCB (modules/08-default-patch) plus the per-module jumpers,
// so the instrument plays without any cables.
enum class NormalKind : uint8_t
{
    FromOutput,      // dst <- src output, only while nothing is plugged into src
    MirrorInput,     // dst <- whatever is plugged into the other input jack
    Always           // hard-wired (UI board jumper), regardless of src jack state
};

struct Normal
{
    int dst;
    int src;
    NormalKind kind;
    bool defaultPatchBoard;   // true: provided by the default patch PCB (can be removed)
};

// clang-format off
inline constexpr std::array<Normal, 14> kNormals {{
    { MIX_IN1,    VCO1_OUT,   NormalKind::FromOutput,  true  },  // JP3
    { MIX_IN2,    VCO2_OUT,   NormalKind::FromOutput,  true  },  // JP5
    { MIX_IN3,    VCO1_SUB,   NormalKind::FromOutput,  true  },  // JP4
    { MIX_IN4,    VCO2_NOISE, NormalKind::FromOutput,  true  },  // JP6
    { VCF_LP,     MIX_OUT,    NormalKind::FromOutput,  true  },  // JP7
    { VCF_CV1,    LFO_TRI,    NormalKind::FromOutput,  true  },  // JP8
    { VCF_CV2,    ENV2_OUT,   NormalKind::FromOutput,  true  },  // JP9
    { VCA1_IN,    VCF_OUT,    NormalKind::FromOutput,  true  },  // JP10
    { OUTPUT_IN,  VCA1_OUT,   NormalKind::FromOutput,  true  },  // JP11
    { VCO1_PITCH, VCO2_PITCH, NormalKind::MirrorInput, true  },  // JP2
    { VCO2_PITCH, VCO1_PITCH, NormalKind::MirrorInput, true  },  // JP1
    { ENV1_GATE,  ENV2_GATE,  NormalKind::MirrorInput, true  },  // JP12
    { ENV2_GATE,  ENV1_GATE,  NormalKind::MirrorInput, true  },  // JP13
    { VCA1_CV,    ENV1_OUT,   NormalKind::Always,      false },  // JP_DEFAULT_ENV_TO_VCA1
}};
// clang-format on

// VCA 2 has the same ENV->VCA jumper; kept separate so the table above mirrors the patch PCB.
inline constexpr Normal kVca2EnvNormal { VCA2_CV, ENV2_OUT, NormalKind::Always, false };
} // namespace moduleur
