#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace moduleur;

namespace
{
// Cable colours of the panel design: yellow, orange, blue, green, red.
const juce::Colour kCableColours[] = {
    juce::Colour (0xffffc900), juce::Colour (0xffff8700), juce::Colour (0xff0074ef), juce::Colour (0xff00af6f), juce::Colour (0xffff4100),
};

int portFromId (const juce::String& id)
{
    for (int i = 0; i < NumPorts; ++i)
        if (id == kPorts[(size_t) i].id)
            return i;
    return -1;
}

juce::String percent (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout ModuleurProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (const auto& def : kParams)
    {
        const juce::ParameterID pid { def.id, 1 };
        switch (def.kind)
        {
            case ControlKind::Selector:
                layout.add (std::make_unique<juce::AudioParameterChoice> (pid, def.name, juce::StringArray { "Tri", "Saw", "Square" },
                                                                          (int) def.defaultValue));
                break;
            case ControlKind::Toggle:
                layout.add (std::make_unique<juce::AudioParameterBool> (pid, def.name, def.defaultValue > 0.5f));
                break;
            case ControlKind::Knob:
            case ControlKind::BigKnob:
            case ControlKind::Trimmer:
            case ControlKind::Slider:
                layout.add (std::make_unique<juce::AudioParameterFloat> (
                    pid, def.name, juce::NormalisableRange<float> (0.0f, 1.0f), def.defaultValue,
                    juce::AudioParameterFloatAttributes().withStringFromValueFunction (percent)));
                break;
        }
    }
    return layout;
}

ModuleurProcessor::ModuleurProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Ext In", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    for (int i = 0; i < NumParams; ++i)
        paramPtrs[(size_t) i] = apvts.getRawParameterValue (kParams[(size_t) i].id);
    for (auto& l : ledLevels)
        l.store (0.0f);
    resetToInitPatch();
}

bool ModuleurProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    const auto in = layouts.getMainInputChannelSet();
    return in.isDisabled() || in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void ModuleurProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    oversampling->initProcessing ((size_t) samplesPerBlock);
    setLatencySamples (juce::roundToInt (oversampling->getLatencyInSamples()));

    scratch.setSize (2, samplesPerBlock);
    rackSampleRate = sampleRate * 2.0;
    rack.prepare (rackSampleRate);
    brainSettingsPending = true;
}

void ModuleurProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0 || oversampling == nullptr)
        return;
    if (needsRackReset)
    {
        rack.prepare (rackSampleRate);
        needsRackReset = false;
    }
    if (scratch.getNumSamples() < numSamples)
        scratch.setSize (2, numSamples, false, false, true);

    // --- controls ---
    float values[NumParams];
    for (int i = 0; i < NumParams; ++i)
    {
        // Saved sessions are untrusted input: never let a non-finite value reach the DSP.
        const float v = paramPtrs[(size_t) i]->load (std::memory_order_relaxed);
        values[i] = std::isfinite (v) ? v : kParams[(size_t) i].defaultValue;
    }
    auto params = makeRackParams (values);
    params.buttonA = buttonA.load();
    params.buttonB = buttonB.load();
    rack.setParams (params);
    rack.resolveRouting (patch, params.defaultPatch);

    if (brainSettingsPending.exchange (false))
    {
        Brain::Settings s;
        s.mode = (Brain::Mode) brainMode.load();
        s.midiChannel = brainChannel.load();
        s.ccMode = (Brain::CcMode) brainCcMode.load();
        s.pitchOnOut1 = brainPitchOnOut1.load();
        rack.brain.setSettings (s);
    }
    if (const int m = requestedBrainMode.exchange (-1); m >= 0)
    {
        auto s = rack.brain.getSettings();
        s.mode = (Brain::Mode) m;
        rack.brain.setSettings (s);
    }
    if (allNotesOffPending.exchange (false))
        rack.brain.allNotesOff();

    // --- host transport (stands in for MIDI clock in the sequencer) ---
    bool playing = false, hasPos = false;
    double ppq = 0.0, bpm = 120.0;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            playing = pos->getIsPlaying();
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto p = pos->getPpqPosition()) { ppq = *p; hasPos = true; }
        }
    }
    rack.brain.setTransport (playing, ppq, bpm, hasPos);

    // --- host audio into the HOST L / HOST R jacks ---
    const int numIn = getTotalNumInputChannels();
    for (int ch = 0; ch < 2; ++ch)
    {
        if (numIn > 0)
            scratch.copyFrom (ch, 0, buffer, std::min (ch, numIn - 1), 0, numSamples);
        else
            scratch.clear (ch, 0, numSamples);
    }

    juce::dsp::AudioBlock<float> block (scratch.getArrayOfWritePointers(), 2, (size_t) numSamples);
    auto up = oversampling->processSamplesUp (block);
    float* upL = up.getChannelPointer (0);
    float* upR = up.getChannelPointer (1);

    auto handleMidi = [this] (const juce::MidiMessage& m)
    {
        auto& brain = rack.brain;
        if (m.isNoteOn())
            brain.noteOn (m.getChannel(), m.getNoteNumber(), m.getVelocity());
        else if (m.isNoteOff())
            brain.noteOff (m.getChannel(), m.getNoteNumber());
        else if (m.isPitchWheel())
            brain.pitchBend (m.getChannel(), m.getPitchWheelValue());
        else if (m.isAllNotesOff() || m.isAllSoundOff())
            brain.allNotesOff();
        else if (m.isController())
            brain.controlChange (m.getChannel(), m.getControllerNumber(), m.getControllerValue());
    };

    auto it = midi.cbegin();
    const auto end = midi.cend();
    for (int n = 0; n < numSamples; ++n)
    {
        while (it != end && (*it).samplePosition <= n)
        {
            handleMidi ((*it).getMessage());
            ++it;
        }
        for (int k = 0; k < 2; ++k)
        {
            const size_t idx = (size_t) (2 * n + k);
            float y = rack.processSample (upL[idx], upR[idx]) * Rack::kVoltsToDigital;
            if (! std::isfinite (y))
            {
                // Last-resort safety: output silence and rebuild the rack next block.
                y = 0.0f;
                needsRackReset = true;
            }
            upL[idx] = y;
            upR[idx] = y;
        }
    }
    for (; it != end; ++it)
        handleMidi ((*it).getMessage());

    oversampling->processSamplesDown (block);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.copyFrom (ch, 0, scratch, std::min (ch, 1), 0, numSamples);

    // --- publish state for the editor / save ---
    for (int i = 0; i < NumLeds; ++i)
        ledLevels[(size_t) i].store (rack.leds[(size_t) i], std::memory_order_relaxed);
    const auto s = rack.brain.getSettings();
    brainMode = (int) s.mode;
    brainModeForUi = (int) s.mode;
    brainChannel = s.midiChannel;
    brainCcMode = (int) s.ccMode;
    brainPitchOnOut1 = s.pitchOnOut1;
}

//==============================================================================
// Callers hold cablesLock.
void ModuleurProcessor::publishCables()
{
    for (int i = 0; i < NumPorts; ++i)
    {
        int src = -1;
        for (const auto& c : cables)
            if (c.to == i)
                src = c.from;
        patch.set (i, src);
    }
    ++cablesVersion;
}

juce::Colour ModuleurProcessor::nextCableColour()
{
    const juce::ScopedLock sl (cablesLock);
    const auto c = kCableColours[colourIndex % (int) std::size (kCableColours)];
    ++colourIndex;
    return c;
}

void ModuleurProcessor::addCable (int from, int to, juce::Colour colour)
{
    const juce::ScopedLock sl (cablesLock);
    if (from < 0 || to < 0 || from >= NumPorts || to >= NumPorts || ! isOutput (from) || isOutput (to))
        return;
    cables.erase (std::remove_if (cables.begin(), cables.end(), [to] (const Cable& c) { return c.to == to; }), cables.end());
    cables.push_back ({ from, to, colour });
    publishCables();
}

void ModuleurProcessor::removeCableToInput (int input)
{
    const juce::ScopedLock sl (cablesLock);
    cables.erase (std::remove_if (cables.begin(), cables.end(), [input] (const Cable& c) { return c.to == input; }), cables.end());
    publishCables();
}

void ModuleurProcessor::clearCables()
{
    const juce::ScopedLock sl (cablesLock);
    cables.clear();
    publishCables();
}

void ModuleurProcessor::resetToInitPatch()
{
    const juce::ScopedLock sl (cablesLock);
    cables.clear();
    colourIndex = 0;
    for (const auto& c : kInitCables)
        cables.push_back ({ c.from, c.to, nextCableColour() });
    publishCables();
}

//==============================================================================
void ModuleurProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    juce::ValueTree patchTree ("PATCH");
    for (const auto& c : getCables())
    {
        juce::ValueTree cable ("CABLE");
        cable.setProperty ("from", kPorts[(size_t) c.from].id, nullptr);
        cable.setProperty ("to", kPorts[(size_t) c.to].id, nullptr);
        cable.setProperty ("colour", c.colour.toString(), nullptr);
        patchTree.appendChild (cable, nullptr);
    }
    state.appendChild (patchTree, nullptr);

    juce::ValueTree brainTree ("BRAIN");
    brainTree.setProperty ("mode", brainMode.load(), nullptr);
    brainTree.setProperty ("midiChannel", brainChannel.load(), nullptr);
    brainTree.setProperty ("ccMode", brainCcMode.load(), nullptr);
    brainTree.setProperty ("pitchOnOut1", brainPitchOnOut1.load(), nullptr);
    state.appendChild (brainTree, nullptr);
    state.appendChild (uiState.createCopy(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void ModuleurProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr)
        return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.hasType (apvts.state.getType()))
        return;

    auto patchTree = state.getChildWithName ("PATCH");
    auto brainTree = state.getChildWithName ("BRAIN");
    auto ui = state.getChildWithName ("UI");
    state.removeChild (patchTree, nullptr);
    state.removeChild (brainTree, nullptr);
    state.removeChild (ui, nullptr);
    apvts.replaceState (state);

    if (patchTree.isValid())
    {
        // At most one cable per input jack; anything malformed is skipped.
        const juce::ScopedLock sl (cablesLock);
        cables.clear();
        std::array<bool, NumPorts> inputUsed {};
        for (auto cable : patchTree)
        {
            if (! cable.hasType ("CABLE"))
                continue;
            const int from = portFromId (cable["from"].toString());
            const int to = portFromId (cable["to"].toString());
            if (from < 0 || to < 0 || ! isOutput (from) || isOutput (to) || inputUsed[(size_t) to])
                continue;
            inputUsed[(size_t) to] = true;
            cables.push_back ({ from, to, juce::Colour::fromString (cable["colour"].toString()).withAlpha (1.0f) });
        }
        colourIndex = (int) cables.size();
        publishCables();
    }
    if (brainTree.isValid())
    {
        brainMode = juce::jlimit (0, 1, (int) brainTree["mode"]);
        brainChannel = juce::jlimit (0, 16, (int) brainTree["midiChannel"]);
        brainCcMode = juce::jlimit (0, 3, (int) brainTree["ccMode"]);
        brainPitchOnOut1 = (bool) brainTree["pitchOnOut1"];
        brainSettingsPending = true;
    }
    if (ui.isValid())
        uiState.copyPropertiesFrom (ui, nullptr);
}

juce::AudioProcessorEditor* ModuleurProcessor::createEditor() { return new ModuleurEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ModuleurProcessor(); }
