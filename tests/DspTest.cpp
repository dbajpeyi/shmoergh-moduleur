// Headless checks of the rack: renders the default patch from MIDI, verifies
// pitch tracking, levels, filter self-oscillation and the Brain sequencer, and
// writes WAV files to the working directory for listening.
#include <juce_audio_formats/juce_audio_formats.h>

#include <cstdio>
#include <vector>

#include "Params.h"

using namespace moduleur;

namespace
{
int failures = 0;

void check (bool ok, const char* what)
{
    std::printf ("[%s] %s\n", ok ? " ok " : "FAIL", what);
    if (! ok)
        ++failures;
}

std::array<float, NumParams> defaults()
{
    std::array<float, NumParams> v {};
    for (int i = 0; i < NumParams; ++i)
        v[(size_t) i] = kParams[(size_t) i].defaultValue;
    return v;
}

struct Harness
{
    static constexpr double kRate = 96000.0;
    Rack rack;
    Patch patch;
    std::array<float, NumParams> values = defaults();

    Harness()
    {
        rack.prepare (kRate);
        for (auto c : kInitCables)
            patch.set (c.to, c.from);
        update();
    }

    void update()
    {
        auto p = makeRackParams (values.data());
        rack.setParams (p);
        rack.resolveRouting (patch, p.defaultPatch);
        rack.brain.setTransport (false, 0.0, 120.0, false);
    }

    std::vector<float> render (double seconds, int probePort = -1, std::vector<float>* probe = nullptr)
    {
        std::vector<float> out;
        const int n = (int) (seconds * kRate);
        out.reserve ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            if (i % 64 == 0)
                update();
            out.push_back (rack.processSample (0.0f, 0.0f) * Rack::kVoltsToDigital);
            if (probe != nullptr)
                probe->push_back (rack.values[(size_t) probePort]);
        }
        return out;
    }
};

double rms (const std::vector<float>& x, size_t from = 0, size_t to = 0)
{
    to = to == 0 ? x.size() : to;
    double s = 0;
    for (size_t i = from; i < to; ++i)
        s += (double) x[i] * x[i];
    return std::sqrt (s / (double) std::max<size_t> (1, to - from));
}

float peak (const std::vector<float>& x)
{
    float p = 0;
    for (auto v : x)
        p = std::max (p, std::abs (v));
    return p;
}

bool finite (const std::vector<float>& x)
{
    for (auto v : x)
        if (! std::isfinite (v))
            return false;
    return true;
}

// Frequency from upward zero crossings (after removing the mean).
double estimateFrequency (const std::vector<float>& x, double rate, size_t from)
{
    double mean = 0;
    for (size_t i = from; i < x.size(); ++i) mean += x[i];
    mean /= (double) (x.size() - from);
    double first = -1, last = -1;
    int count = 0;
    for (size_t i = from + 1; i < x.size(); ++i)
    {
        const double a = x[i - 1] - mean, b = x[i] - mean;
        if (a < 0 && b >= 0)
        {
            const double t = (double) (i - 1) + a / (a - b);
            if (first < 0) first = t; else ++count;
            last = t;
        }
    }
    return count > 0 ? rate * count / (last - first) : 0.0;
}

void writeWav (const char* name, const std::vector<float>& x, double rate)
{
    juce::File f = juce::File::getCurrentWorkingDirectory().getChildFile (name);
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::FileOutputStream> stream (f.createOutputStream());
    if (stream == nullptr)
        return;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (stream.get(), rate, 1, 24, {}, 0));
    if (w == nullptr)
        return;
    stream.release();
    juce::AudioBuffer<float> buf (1, (int) x.size());
    for (size_t i = 0; i < x.size(); ++i)
        buf.setSample (0, (int) i, x[i]);
    w->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
}
} // namespace

int main()
{
    // 1. Default patch, MIDI note through the Brain.
    {
        Harness h;
        std::vector<float> osc;
        h.rack.brain.noteOn (1, 60, 100);
        auto out = h.render (1.0, VCO1_OUT, &osc);
        h.rack.brain.noteOff (1, 60);
        auto tail = h.render (1.5);
        out.insert (out.end(), tail.begin(), tail.end());

        check (finite (out), "default patch output is finite");
        const double r = rms (out, 4800, 96000);
        std::printf ("       note RMS %.3f, peak %.3f\n", r, peak (out));
        check (r > 0.02, "default patch makes sound while the note is held");
        check (peak (out) < 1.0f, "output stays below full scale");
        check (rms (out, out.size() - 9600) < 0.002, "release decays to silence");

        const double f = estimateFrequency (osc, Harness::kRate, 9600);
        std::printf ("       OSC 1 frequency %.2f Hz (expected 261.63)\n", f);
        check (std::abs (f - 261.63) < 261.63 * 0.006, "OSC 1 tracks MIDI note 60 at 1 V/oct");
        writeWav ("test-default-patch.wav", out, Harness::kRate);
    }

    // 2. Octave tracking: C1 (MIDI 24) and C5 (MIDI 72).
    for (int note : { 24, 48, 84 })
    {
        Harness h;
        std::vector<float> osc;
        h.rack.brain.noteOn (1, note, 100);
        h.render (0.5, VCO1_OUT, &osc);
        const double expected = 440.0 * std::pow (2.0, (note - 69) / 12.0);
        const double f = estimateFrequency (osc, Harness::kRate, 4800);
        std::printf ("       note %d: %.2f Hz (expected %.2f)\n", note, f, expected);
        check (std::abs (f / expected - 1.0) < 0.006, "pitch tracking across octaves");
    }

    // 3. Filter self-oscillates at full resonance with no input.
    {
        Harness h;
        h.values[P_MIX_MAIN] = 0.0f;
        h.values[P_VCF_RESO] = 1.0f;
        h.values[P_VCF_CUTOFF] = 0.5f;
        h.values[P_VCF_CV1] = 0.5f;
        h.values[P_VCF_CV2] = 0.5f;
        h.values[P_ENV1_SUSTAIN] = 1.0f;
        h.patch.set (VCA1_CV, -1);
        h.patch.set (OUTPUT_IN, VCF_OUT);   // listen to the filter directly
        h.update();
        // kick the filter
        std::vector<float> vcfOut;
        h.render (1.0, VCF_OUT, &vcfOut);
        const double r = rms (vcfOut, 48000);
        const double f = estimateFrequency (vcfOut, Harness::kRate, 48000);
        std::printf ("       self-oscillation %.2f V RMS at %.1f Hz\n", r, f);
        check (r > 0.3 && finite (vcfOut), "filter self-oscillates at maximum resonance");

        h.values[P_VCF_RESO] = 0.6f;
        h.update();
        std::vector<float> damped;
        h.render (1.0, VCF_OUT, &damped);
        check (rms (damped, 48000) < 0.05, "filter is stable below the oscillation point");
    }

    // 4. Brain sequencer: hold A+B to switch modes, press A to play.
    {
        Harness h;
        auto press = [&h] (bool a, bool b, double seconds)
        {
            auto p = makeRackParams (h.values.data());
            p.buttonA = a;
            p.buttonB = b;
            const int n = (int) (seconds * Harness::kRate);
            for (int i = 0; i < n; ++i)
            {
                h.rack.setParams (p);
                h.rack.processSample (0, 0);
            }
        };
        press (true, true, 1.7);
        press (false, false, 0.1);
        check (h.rack.brain.getSettings().mode == Brain::Mode::Sequencer, "A+B long press switches to the sequencer");
        press (true, false, 0.05);
        press (false, false, 0.05);
        int gates = 0;
        bool last = false;
        std::vector<float> audio;
        for (int i = 0; i < (int) (4.0 * Harness::kRate); ++i)
        {
            if (i % 64 == 0) h.update();
            audio.push_back (h.rack.processSample (0, 0) * Rack::kVoltsToDigital);
            const bool g = h.rack.values[BRAIN_PULSE] > 1.0f;
            gates += g && ! last;
            last = g;
        }
        std::printf ("       sequencer produced %d gates in 4 s\n", gates);
        check (gates >= 12 && gates <= 32, "sequencer runs at ~120 BPM 16ths with random gates");
        check (finite (audio) && peak (audio) < 1.0f, "sequenced audio is sane");
        writeWav ("test-sequencer.wav", audio, Harness::kRate);
    }

    // 5. Cables break normals: plug VCO2 OUT somewhere and Mixer IN 2 loses it.
    {
        Harness h;
        check (h.rack.route[MIX_IN2] == VCO2_OUT, "Mixer IN 2 normalled to OSC 2 OUT");
        h.patch.set (VCF_HP, VCO2_OUT);
        h.update();
        check (h.rack.route[MIX_IN2] == -1, "patching OSC 2 OUT removes its default connection");
        check (h.rack.route[VCO2_PITCH] == BRAIN_OUT1, "OSC 2 pitch mirrors the cable in OSC 1 PITCH");
        check (h.rack.route[ENV2_GATE] == BRAIN_PULSE, "ENV 2 gate mirrors the cable in ENV 1 GATE");
        h.values[P_DEFAULT_PATCH] = 0.0f;
        h.update();
        check (h.rack.route[VCF_LP] == -1, "removing the default patch board disconnects normals");
        check (h.rack.route[VCA1_CV] == ENV1_OUT, "ENV->VCA jumper stays without the patch board");
    }

    std::printf ("%s (%d failure%s)\n", failures == 0 ? "PASS" : "FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
