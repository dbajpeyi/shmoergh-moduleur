// Runs the actual plugin processor the way a host would: MIDI in, audio out,
// state save/restore.
#include "PluginProcessor.h"

#include <cstdio>

namespace
{
int failures = 0;
void check (bool ok, const char* what)
{
    std::printf ("[%s] %s\n", ok ? " ok " : "FAIL", what);
    if (! ok)
        ++failures;
}

double renderRms (ModuleurProcessor& p, int blocks, int blockSize, juce::MidiBuffer* firstMidi = nullptr)
{
    juce::AudioBuffer<float> buffer (2, blockSize);
    double sum = 0;
    long n = 0;
    for (int b = 0; b < blocks; ++b)
    {
        buffer.clear();
        juce::MidiBuffer midi;
        if (b == 0 && firstMidi != nullptr)
            midi = *firstMidi;
        p.processBlock (buffer, midi);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < blockSize; ++i)
            {
                const float v = buffer.getSample (ch, i);
                if (! std::isfinite (v))
                    return -1.0;
                sum += (double) v * v;
                ++n;
            }
    }
    return std::sqrt (sum / (double) n);
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juce;
    std::unique_ptr<ModuleurProcessor> proc (dynamic_cast<ModuleurProcessor*> (createPluginFilter()));
    check (proc != nullptr, "plugin instance created");
    proc->setPlayConfigDetails (0, 2, 48000.0, 512);
    proc->prepareToPlay (48000.0, 512);

    check (renderRms (*proc, 20, 512) < 1.0e-3, "silent without notes");

    juce::MidiBuffer on;
    on.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 100), 100);
    const double rms = renderRms (*proc, 40, 512, &on);
    std::printf ("       held note RMS %.3f\n", rms);
    check (rms > 0.02 && rms < 0.7, "MIDI note produces a sane level");

    juce::MidiBuffer off;
    off.addEvent (juce::MidiMessage::noteOff (1, 57), 0);
    renderRms (*proc, 300, 512, &off);
    check (renderRms (*proc, 10, 512) < 2.0e-3, "note off decays to silence");

    // State round trip with a modified patch
    proc->addCable (moduleur::LFO_SQR, moduleur::VCO1_PWM, juce::Colours::red);
    proc->apvts.getParameter ("vcf_cutoff")->setValueNotifyingHost (0.123f);
    juce::MemoryBlock state;
    proc->getStateInformation (state);

    std::unique_ptr<ModuleurProcessor> other (dynamic_cast<ModuleurProcessor*> (createPluginFilter()));
    other->setStateInformation (state.getData(), (int) state.getSize());
    bool hasCable = false;
    for (const auto& c : other->getCables())
        hasCable = hasCable || (c.from == moduleur::LFO_SQR && c.to == moduleur::VCO1_PWM);
    check (hasCable, "cables survive save/restore");
    check (std::abs (other->apvts.getParameter ("vcf_cutoff")->getValue() - 0.123f) < 1.0e-4f, "parameters survive save/restore");
    check (other->getCables().size() == proc->getCables().size(), "cable count matches after restore");

    // Different block sizes / sample rates
    for (double sr : { 44100.0, 96000.0 })
    {
        other->setPlayConfigDetails (0, 2, sr, 64);
        other->prepareToPlay (sr, 64);
        juce::MidiBuffer m;
        m.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 90), 3);
        const double r = renderRms (*other, 400, 64, &m);
        std::printf ("       %.0f Hz / 64 samples: RMS %.3f\n", sr, r);
        check (r > 0.02 && r < 0.7, "works at other sample rates and small blocks");
    }

    std::printf ("%s (%d failure%s)\n", failures == 0 ? "PASS" : "FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
