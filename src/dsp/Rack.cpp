#include "Rack.h"

namespace moduleur
{
void Rack::prepare (double rackSampleRate)
{
    sr = rackSampleRate;
    const float fs = (float) sr;
    vco[0].prepare (fs, 0x1234abcdu);
    vco[1].prepare (fs, 0x9876fedcu);
    mixer.prepare (fs);
    vcf.prepare (fs);
    env[0].prepare (fs);
    env[1].prepare (fs);
    crusher.prepare (fs);
    lfo.prepare (fs);
    output.prepare (fs);
    brain.prepare (sr);
    for (auto& s : smooth)
        s.prepare (fs, 10.0f);
    reset();
}

void Rack::reset()
{
    values.fill (0.0f);
    route.fill (-1);
    leds.fill (0.0f);
    firstBlock = true;
}

void Rack::resolveRouting (const Patch& patch, bool defaultPatchBoard)
{
    for (int p = 0; p < NumPorts; ++p)
        route[(size_t) p] = isOutput (p) ? -1 : patch.get (p);

    auto applyNormal = [&] (const Normal& n)
    {
        if (patch.get (n.dst) >= 0)
            return;   // a cable in the destination breaks the switched normal
        if (n.defaultPatchBoard && ! defaultPatchBoard)
            return;

        switch (n.kind)
        {
            case NormalKind::FromOutput:
                if (! patch.outputHasCable (n.src))
                    route[(size_t) n.dst] = n.src;
                break;
            case NormalKind::MirrorInput:
                route[(size_t) n.dst] = patch.get (n.src);
                break;
            case NormalKind::Always:
                route[(size_t) n.dst] = n.src;
                break;
        }
    };

    for (const auto& n : kNormals)
        applyNormal (n);
    applyNormal (kVca2EnvNormal);
}

float Rack::processSample (float extL, float extR)
{
    if (firstBlock)
    {
        const float snaps[NumSmooth] = {
            params.vco[0].octave, params.vco[0].fine, params.vco[0].width,
            params.vco[1].octave, params.vco[1].fine, params.vco[1].width,
            params.mix.in1, params.mix.in2, params.mix.in3, params.mix.in4, params.mix.gain, params.mix.threshold, params.mix.main,
            params.vcf.cutoff, params.vcf.reso, params.vcf.cv1, params.vcf.cv2,
            params.env[0].sustain, params.env[1].sustain,
            params.crush, params.lfoRate, params.volume, params.lfoAmount, params.drive
        };
        for (int i = 0; i < NumSmooth; ++i)
            smooth[(size_t) i].snap (snaps[i]);
        firstBlock = false;
    }

    auto sm = [this] (int id, float target) { return smooth[(size_t) id].next (target); };

    values[EXT_L] = extL * 5.0f;
    values[EXT_R] = extR * 5.0f;

    // --- Brain ---
    brain.setControls (params.brainX, params.brainY, params.brainZ, params.buttonA, params.buttonB);
    Brain::Io bio;
    bio.pulseIn = in (BRAIN_PULSE_IN);
    brain.process (bio);
    values[BRAIN_OUT1] = bio.out1;
    values[BRAIN_OUT2] = bio.out2;
    values[BRAIN_PULSE] = bio.pulse;

    // --- Oscillators ---
    for (int i = 0; i < 2; ++i)
    {
        const int base = i == 0 ? VCO1_SYNC : VCO2_SYNC;
        Vco::Controls c = params.vco[i];
        c.octave = sm (i * 3 + S_VCO1_OCT, c.octave);
        c.fine = sm (i * 3 + S_VCO1_FINE, c.fine);
        c.width = sm (i * 3 + S_VCO1_WIDTH, c.width);
        Vco::Inputs vi;
        vi.sync = in (base + 0);
        vi.fm = in (base + 1);
        vi.pwm = in (base + 2);
        vi.pitch = in (base + 3);
        const auto o = vco[i].process (c, vi);
        values[(size_t) base + 4] = i == 0 ? o.sub : o.noise;   // SUB on OSC 1, NOISE on OSC 2 (JP1 jumper)
        values[(size_t) base + 5] = o.out;
    }

    // --- Mixer ---
    {
        SidechainMixer::Controls c;
        c.in1 = sm (S_MIX_1, params.mix.in1);
        c.in2 = sm (S_MIX_2, params.mix.in2);
        c.in3 = sm (S_MIX_3, params.mix.in3);
        c.in4 = sm (S_MIX_4, params.mix.in4);
        c.gain = sm (S_MIX_GAIN, params.mix.gain);
        c.threshold = sm (S_MIX_TH, params.mix.threshold);
        c.main = sm (S_MIX_MAIN, params.mix.main);
        SidechainMixer::Inputs mi { in (MIX_IN1), in (MIX_IN2), in (MIX_IN3), in (MIX_IN4) };
        values[MIX_OUT] = mixer.process (c, mi);
    }

    // --- LFO ---
    {
        float tri = 0.0f, sqr = 0.0f;
        lfo.process (sm (S_LFO_RATE, params.lfoRate), tri, sqr);
        const float amt = audioTaper (sm (S_LFO_AMT, params.lfoAmount));
        values[LFO_TRI] = tri * amt;
        values[LFO_SQR] = sqr * amt;
        leds[LED_LFO] = lfo.ledLevel();
    }

    // --- Filter ---
    {
        DiodeVcf::Controls c;
        c.cutoff = sm (S_VCF_CUT, params.vcf.cutoff);
        c.reso = sm (S_VCF_RES, params.vcf.reso);
        c.cv1 = sm (S_VCF_CV1, params.vcf.cv1);
        c.cv2 = sm (S_VCF_CV2, params.vcf.cv2);
        DiodeVcf::Inputs fi { in (VCF_LP), in (VCF_HP), in (VCF_CV1), in (VCF_CV2) };
        values[VCF_OUT] = vcf.process (c, fi);
    }

    // --- Envelopes & VCAs ---
    for (int i = 0; i < 2; ++i)
    {
        const int base = i == 0 ? ENV1_GATE : ENV2_GATE;
        AdsrVca::Controls c = params.env[i];
        c.sustain = sm (i == 0 ? S_SUS1 : S_SUS2, c.sustain);
        AdsrVca::Inputs ei;
        ei.gate = in (base);
        ei.vcaIn = in (base + 2);
        // VCA CV normalled to this module's own envelope (jumper): use this sample's envelope.
        ei.cvIsOwnEnvelope = route[(size_t) base + 3] == base + 1;
        ei.vcaCv = in (base + 3);
        const auto o = env[i].process (c, ei);
        values[(size_t) base + 1] = o.env;
        values[(size_t) base + 4] = o.vca;
        leds[(size_t) (i == 0 ? LED_ENV1 : LED_ENV2)] = env[i].ledLevel();
    }

    // --- Crush ---
    {
        const bool trigPatched = route[CRUSH_TRIG] >= 0;
        values[CRUSH_OUT] = crusher.process (sm (S_CRUSH, params.crush), in (CRUSH_IN), trigPatched, in (CRUSH_TRIG));
    }

    // --- Output ---
    float line = output.process (in (OUTPUT_IN), sm (S_DRIVE, params.drive), sm (S_VOLUME, params.volume));
    leds[LED_DRIVE] = output.ledLevel();

    const float powerTarget = params.power ? 1.0f : 0.0f;
    powerLevel += (powerTarget - powerLevel) * 0.0005f;
    line *= powerLevel;
    values[OUTPUT_PHONES] = line;
    leds[LED_POWER] = powerLevel;

    for (size_t i = 0; i < 6; ++i)
        leds[LED_BRAIN1 + i] = brain.leds[i];
    leds[LED_BRAIN_A] = brain.buttonLed;

    return line;
}
} // namespace moduleur
