#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "Params.h"

class ModuleurProcessor : public juce::AudioProcessor
{
public:
    ModuleurProcessor();
    ~ModuleurProcessor() override = default;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Moduleur"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    // Patch cables (message thread). Each input jack takes one cable; output
    // jacks can feed several.
    struct Cable
    {
        int from = -1, to = -1;   // output port, input port
        juce::Colour colour;
    };

    std::vector<Cable> getCables() const
    {
        const juce::ScopedLock sl (cablesLock);
        return cables;
    }
    // Bumped whenever the cables change; the editor polls it.
    std::atomic<int> cablesVersion { 0 };
    void addCable (int from, int to, juce::Colour colour);
    void removeCableToInput (int input);
    void clearCables();
    void resetToInitPatch();
    juce::Colour nextCableColour();

    // Switches the Brain firmware app (0 = MIDI to CV, 1 = Sequencer) from the UI.
    void requestBrainMode (int mode) { requestedBrainMode = mode; }
    void panic() { allNotesOffPending = true; }

    // Brain buttons (momentary, not automatable)
    std::atomic<bool> buttonA { false }, buttonB { false };

    // Published for the editor
    std::array<std::atomic<float>, moduleur::NumLeds> ledLevels;
    std::atomic<int> brainModeForUi { 0 };

    juce::AudioProcessorValueTreeState apvts;
    juce::ValueTree uiState { "UI" };   // theme, scale (message thread)

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void publishCables();

    moduleur::Rack rack;
    moduleur::Patch patch;
    std::vector<Cable> cables;
    juce::CriticalSection cablesLock;
    int colourIndex = 0;

    std::array<std::atomic<float>*, moduleur::NumParams> paramPtrs {};
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    juce::AudioBuffer<float> scratch;

    // Brain settings mirrored for state save/restore across threads
    std::atomic<int> brainMode { 0 }, brainChannel { 0 }, brainCcMode { 0 };
    std::atomic<bool> brainPitchOnOut1 { true };
    std::atomic<bool> brainSettingsPending { false };
    std::atomic<bool> allNotesOffPending { false };
    std::atomic<int> requestedBrainMode { -1 };
    double rackSampleRate = 96000.0;
    bool needsRackReset = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModuleurProcessor)
};
