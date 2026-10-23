#pragma once

#include <array>
#include <cstdint>

// The Brain module runs firmware on a Raspberry Pi Pico. This is a port of the
// behaviour of "Le Controlleur" (shmoergh/le-controlleur, MIT licence): a MIDI to
// CV converter and a probabilistic 16th-note sequencer, switched by holding A+B.
// MIDI comes from the plugin host instead of the TRS MIDI jack, and the host
// transport stands in for MIDI clock in external-sync mode.
namespace moduleur
{
class Brain
{
public:
    enum class Mode : uint8_t { MidiToCv = 0, Sequencer = 1 };
    enum class CcMode : uint8_t { Velocity = 0, ModWheel = 1, Unison = 2, Duo = 3 };
    enum class Quantization : uint8_t { Unquantized, Chromatic, Major, Minor, Pentatonic, Extra };

    struct Settings
    {
        Mode mode = Mode::MidiToCv;
        int midiChannel = 0;          // 0 = omni (plugin default), 1..16
        bool pitchOnOut1 = true;      // "CV channel A"
        CcMode ccMode = CcMode::Velocity;
    };

    void prepare (double sampleRate);
    void reset();

    void setSettings (const Settings& s);
    Settings getSettings() const { return settings; }

    // Called once per block.
    void setTransport (bool hostPlaying, double ppqAtBlockStart, double bpm, bool hasPosition);
    void setControls (float potX, float potY, float potZ, bool buttonA, bool buttonB);

    // MIDI (call at the right sample position inside the block).
    void noteOn (int channel, int note, int velocity);
    void noteOff (int channel, int note);
    void controlChange (int channel, int cc, int value);
    void pitchBend (int channel, int value14);   // 0..16383, centre 8192
    void allNotesOff();

    struct Io
    {
        float pulseIn = 0.0f;
        float out1 = 0.0f, out2 = 0.0f, pulse = 0.0f;
    };

    // Advances one sample (at the rack rate). `blockSample` is the index inside
    // the current host block, used to place host-transport steps precisely.
    void process (Io& io);

    std::array<float, 6> leds {};
    float buttonLed = 0.0f;

private:
    struct NoteVel { uint8_t note = 255, vel = 0; };
    struct Step { uint16_t pitchQ8 = 0; bool gate = false; };

    // --- buttons (app-controller.cpp) ---
    void handleButtons();
    void onButtonAPress();
    void onButtonARelease();
    void onButtonBPress();
    void onButtonBRelease();
    void switchMode();
    void afterRelease();

    // --- MIDI to CV (midi-to-cv-engine.cpp + brain-sdk midi-to-cv.cpp) ---
    void m2cUpdateCv();
    void m2cUpdateSettingsFromPots();
    void m2cPanic();
    float noteToVolts (int note) const;

    // --- sequencer (sequencer-engine.cpp) ---
    void seqEnter();
    void seqUpdate();
    void seqTick();
    void seqTogglePlay();
    void seqApplyPots (bool force);
    uint16_t applyRange (uint16_t q8) const;
    uint16_t quantize (uint16_t q8) const;
    uint8_t lengthWithSnap (uint8_t pot) const;
    double nextStepIntervalUs() const;
    void pushGateHistory (bool high);
    void showOverlay (uint8_t potIndex);
    static uint32_t xorshift (uint32_t& s);
    static uint8_t randomU8 (uint32_t& s) { return (uint8_t) (xorshift (s) & 0xFFu); }

    void setLedMask (uint8_t mask, float level = 1.0f);

    double sr = 48000.0;
    double nowUs = 0.0, usPerSample = 1.0e6 / 48000.0;
    Settings settings;

    // Panel state
    std::array<uint8_t, 3> pots {}, potsPrev {};
    bool btnA = false, btnB = false, aHeld = false, bHeld = false;

    // App controller gesture tracking
    bool aPending = false, bPending = false, aDispatched = false, bDispatched = false;
    bool dualActive = false, dualHandled = false;
    double firstPressAt = 0, dualStartedAt = 0;
    bool bWaitingSecondTap = false, rootEditHold = false;
    double bLastReleaseAt = 0;
    double animationUntil = 0;

    // MIDI to CV
    enum class M2cState { Default, SetMidiChannel, SetCv } m2cState = M2cState::Default;
    std::array<NoteVel, 25> stack {};
    int stackSize = 0;
    NoteVel lastNote { 24, 0 };
    NoteVel duoPrimary { 24, 0 }, duoSecondary { 24, 0 };
    int duoPrevStackSize = 0;
    int modWheel = 0, bend = 0;   // bend: -8192..8191
    bool gate = false;
    float cvA = 0.0f, cvB = 0.0f;
    uint8_t entryRaw[3] {};
    bool pickupArmed[3] {};
    int playheadLed = 0;
    bool keyWasPressed = false;

    // Sequencer
    std::array<Step, 64> seqA {}, seqB {};
    uint8_t seqLength = 8, seqPos = 0;
    bool playing = false, gateActive = false, shift = false, rootEdit = false;
    uint16_t bpm = 120;
    double tickIntervalUs = 125000.0, nextTickDue = -1.0, gateOffAt = 0.0;
    uint32_t tickCounter = 0;
    uint8_t swingPot = 0, rangeOct = 3, mutation = 0, rootNote = 0, octTranspose = 2;
    Quantization quant = Quantization::Minor;
    bool externalSync = false;
    uint32_t rngA = 0x12345678u, rngB = 0x87654321u;
    std::array<uint8_t, 6> gateHistory {};
    double overlayUntil = 0.0;
    uint8_t overlayMask = 0;
    std::array<uint8_t, 3> layerEntryRaw {};
    bool layerArmed[3] {};
    bool lastShiftContext = false;
    uint8_t rootRefOct = 0, rootRefNote = 0;

    // External clock
    bool pulseWasHigh = false;
    float pulseInValue = 0.0f;
    double lastPulseAt = -1.0e12, pulseIntervalUs = 125000.0;
    bool swingPending = false;
    double swingDueAt = 0.0;
    void externalStep (double intervalUs);

    // Host transport
    bool hostPlaying = false, hostPlayingPrev = false, hostHasPos = false;
    double hostPpq = 0.0, hostBpm = 120.0;
    long long lastHostStep = -1;
    int blockSampleIndex = 0;
};
} // namespace moduleur
