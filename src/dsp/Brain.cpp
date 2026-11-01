#include "Brain.h"

#include <algorithm>
#include <cmath>

namespace moduleur
{
namespace
{
constexpr double kShortPressMaxUs = 400000.0;
constexpr double kLongPressMinUs = 1500000.0;
constexpr double kDualPressWindowUs = 120000.0;
constexpr double kDoubleTapWindowUs = 350000.0;
constexpr double kGatePulseUs = 20000.0;
constexpr double kOverlayHoldUs = 1000000.0;
constexpr uint16_t kQ8PerSemitone = 256;
constexpr uint16_t kSourceRangeOctaves = 5;
constexpr uint32_t kRandomMaxQ8 = kSourceRangeOctaves * 12u * kQ8PerSemitone;
constexpr float kGateVolts = 10.0f;
constexpr float kMaxCcVolts = 5.0f;

uint8_t barMask (uint8_t value255)
{
    if (value255 == 0)
        return 0;
    int lit = std::clamp ((value255 * 6 + 127) / 255, 1, 6);
    return (uint8_t) ((1u << lit) - 1u);
}
} // namespace

uint32_t Brain::xorshift (uint32_t& s)
{
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}

void Brain::prepare (double sampleRate)
{
    sr = sampleRate;
    usPerSample = 1.0e6 / sr;
    reset();
}

void Brain::reset()
{
    nowUs = 0.0;
    stack.fill ({});
    stackSize = 0;
    gate = false;
    gateActive = false;
    m2cState = M2cState::Default;
    for (auto& l : leds) l = 0.0f;

    rngA = 0x12345678u;
    rngB = 0x87654321u;
    for (size_t i = 0; i < seqA.size(); ++i)
    {
        seqA[i].pitchQ8 = (uint16_t) (xorshift (rngA) % (kRandomMaxQ8 + 1u));
        seqA[i].gate = randomU8 (rngA) >= 102u;
        seqB[i].pitchQ8 = (uint16_t) (xorshift (rngB) % (kRandomMaxQ8 + 1u));
        seqB[i].gate = randomU8 (rngB) >= 102u;
    }
    seqPos = 0;
    playing = false;
    if (settings.mode == Mode::Sequencer)
        seqEnter();
}

void Brain::setSettings (const Settings& requested)
{
    Settings s = requested;
    s.mode = s.mode == Mode::Sequencer ? Mode::Sequencer : Mode::MidiToCv;
    s.midiChannel = std::clamp (s.midiChannel, 0, 16);
    s.ccMode = (CcMode) std::clamp ((int) s.ccMode, 0, 3);
    const bool modeChanged = s.mode != settings.mode;
    settings = s;
    if (modeChanged && settings.mode == Mode::Sequencer)
        seqEnter();
}

void Brain::setTransport (bool isPlaying, double ppq, double tempo, bool hasPosition)
{
    hostPlaying = isPlaying;
    hostPpq = ppq;
    hostBpm = tempo > 1.0 ? tempo : 120.0;
    hostHasPos = hasPosition;
    blockSampleIndex = 0;
}

void Brain::setControls (float x, float y, float z, bool a, bool b)
{
    auto raw = [] (float v) { return (uint8_t) std::clamp ((int) std::lround (v * 255.0f), 0, 255); };
    pots = { raw (x), raw (y), raw (z) };
    btnA = a;
    btnB = b;
}

//==============================================================================
// MIDI

void Brain::noteOn (int channel, int note, int velocity)
{
    if (settings.midiChannel != 0 && channel != settings.midiChannel)
        return;
    if (velocity <= 0)
    {
        noteOff (channel, note);
        return;
    }

    if (settings.mode == Mode::Sequencer)
    {
        if (rootEdit)
            rootNote = (uint8_t) (note % 12);
        return;
    }

    bool found = false;
    for (int i = 0; i < stackSize; ++i)
        found = found || stack[(size_t) i].note == note;
    if (! found && stackSize < (int) stack.size())
        stack[(size_t) stackSize++] = { (uint8_t) note, (uint8_t) velocity };

    m2cUpdateCv();
    gate = true;
}

void Brain::noteOff (int channel, int note)
{
    if (settings.midiChannel != 0 && channel != settings.midiChannel)
        return;
    if (settings.mode == Mode::Sequencer)
        return;

    for (int i = 0; i < stackSize; ++i)
    {
        if (stack[(size_t) i].note == note)
        {
            for (int j = i; j < stackSize - 1; ++j)
                stack[(size_t) j] = stack[(size_t) j + 1];
            stack[(size_t) --stackSize] = {};
            break;
        }
    }
    m2cUpdateCv();
    if (stackSize == 0)
        gate = false;
}

void Brain::controlChange (int channel, int cc, int value)
{
    if (settings.midiChannel != 0 && channel != settings.midiChannel)
        return;
    if (cc == 1)
    {
        modWheel = value;
        if (settings.ccMode == CcMode::ModWheel)
            m2cUpdateCv();
    }
    else if (cc == 123 || cc == 120)
    {
        allNotesOff();
    }
}

void Brain::pitchBend (int channel, int value14)
{
    if (settings.midiChannel != 0 && channel != settings.midiChannel)
        return;
    bend = std::clamp (value14 - 8192, -8192, 8191);
    m2cUpdateCv();
}

void Brain::allNotesOff() { m2cPanic(); }

float Brain::noteToVolts (int note) const
{
    // kZeroCVMidiNote = 24: 0 V is C1. Pitch bend +/-2 semitones.
    const double semis = (double) (note - 24) + (bend >= 0 ? bend / 8191.0 : bend / 8192.0) * 2.0;
    return (float) (semis / 12.0);
}

void Brain::m2cUpdateCv()
{
    NoteVel play = stackSize > 0 ? stack[(size_t) stackSize - 1] : lastNote;
    if (stackSize > 0)
        lastNote = play;

    float pitch = noteToVolts (play.note);
    float second = 0.0f;

    switch (settings.ccMode)
    {
        case CcMode::Duo:
            if (stackSize > 1)
            {
                duoPrimary = stack[(size_t) stackSize - 2];
                duoSecondary = stack[(size_t) stackSize - 1];
            }
            else if (stackSize == 1 && duoPrevStackSize == 0)
            {
                duoPrimary = duoSecondary = stack[0];
            }
            pitch = noteToVolts (duoPrimary.note);
            second = noteToVolts (duoSecondary.note);
            break;
        case CcMode::Unison: second = pitch; break;
        case CcMode::ModWheel: second = (float) modWheel * kMaxCcVolts / 127.0f; break;
        case CcMode::Velocity:
        default: second = (float) play.vel * kMaxCcVolts / 127.0f; break;
    }
    duoPrevStackSize = stackSize;

    // DAC range is 0..10 V
    pitch = std::clamp (pitch, 0.0f, 10.0f);
    second = std::clamp (second, 0.0f, 10.0f);
    if (settings.pitchOnOut1) { cvA = pitch; cvB = second; }
    else { cvB = pitch; cvA = second; }
}

void Brain::m2cPanic()
{
    gate = false;
    stack.fill ({});
    stackSize = 0;
    duoPrevStackSize = 0;
    m2cState = M2cState::Default;
    gateActive = false;
}

void Brain::m2cUpdateSettingsFromPots()
{
    auto armed = [this] (int i, int threshold)
    {
        if (! pickupArmed[i] && std::abs ((int) pots[(size_t) i] - (int) entryRaw[i]) > threshold)
            pickupArmed[i] = true;
        return pickupArmed[i];
    };

    if (m2cState == M2cState::SetMidiChannel)
    {
        if (armed (0, 2))
            settings.midiChannel = pots[0] / 16 + 1;
        setLedMask ((uint8_t) (settings.midiChannel & 0x3F));
        if (settings.midiChannel == 0)
            setLedMask (0x3F, 0.25f);  // omni: all LEDs dim
    }
    else if (m2cState == M2cState::SetCv)
    {
        if (armed (1, 6))
            settings.pitchOnOut1 = pots[1] < 127;
        if (armed (2, 6))
            settings.ccMode = (CcMode) std::clamp ((4 * pots[2]) / 256, 0, 3);

        uint8_t mask = settings.pitchOnOut1 ? 0b000001 : 0b000010;
        switch (settings.ccMode)
        {
            case CcMode::Velocity: mask |= 0b001000; break;
            case CcMode::ModWheel: mask |= 0b010000; break;
            case CcMode::Unison:   mask |= 0b011000; break;
            case CcMode::Duo:      mask |= 0b100000; break;
        }
        setLedMask (mask);
        m2cUpdateCv();
    }
}

//==============================================================================
// Buttons (gesture logic of app-controller.cpp)

void Brain::onButtonAPress()
{
    if (! bHeld)
    {
        firstPressAt = nowUs;
        if (settings.mode == Mode::Sequencer)
        {
            seqTogglePlay();
            aDispatched = true;
            aPending = false;
            return;
        }
        aPending = true;
        return;
    }
    if (nowUs - firstPressAt <= kDualPressWindowUs)
    {
        if (settings.mode == Mode::Sequencer && bPending)
        {
            shift = false;
            rootEdit = false;
        }
        dualActive = true;
        dualHandled = false;
        dualStartedAt = nowUs;
        aPending = bPending = aDispatched = bDispatched = false;
        return;
    }
    aPending = true;
}

void Brain::onButtonBPress()
{
    if (! aHeld)
    {
        if (settings.mode == Mode::Sequencer)
        {
            const bool secondTap = bWaitingSecondTap && nowUs - bLastReleaseAt <= kDoubleTapWindowUs;
            if (secondTap)
            {
                bWaitingSecondTap = false;
                rootEditHold = true;
                rootEdit = true;
                rootRefOct = pots[1];
                rootRefNote = pots[2];
                bPending = false;
                firstPressAt = nowUs;
                return;
            }
            rootEditHold = false;
            bWaitingSecondTap = true;
            shift = true;
        }
        firstPressAt = nowUs;
        bPending = true;
        return;
    }
    if (nowUs - firstPressAt <= kDualPressWindowUs)
    {
        if (settings.mode == Mode::Sequencer && aDispatched)
            seqTogglePlay();   // revert the optimistic play/pause
        shift = false;
        rootEdit = false;
        dualActive = true;
        dualHandled = false;
        dualStartedAt = nowUs;
        aPending = bPending = aDispatched = bDispatched = false;
        return;
    }
    bPending = true;
}

void Brain::onButtonARelease()
{
    if (! dualActive)
    {
        if (settings.mode == Mode::Sequencer)
        {
            aPending = aDispatched = false;
        }
        else
        {
            if (aPending && ! aDispatched)
            {
                m2cState = M2cState::SetMidiChannel;
                entryRaw[0] = pots[0];
                pickupArmed[0] = false;
                aDispatched = true;
            }
            if (aDispatched && m2cState == M2cState::SetMidiChannel)
                m2cState = M2cState::Default;
            aPending = aDispatched = false;
        }
    }
}

void Brain::onButtonBRelease()
{
    if (! dualActive)
    {
        if (settings.mode == Mode::Sequencer)
        {
            const double held = nowUs - firstPressAt;
            if (rootEditHold)
            {
                rootEdit = false;
                rootEditHold = false;
                bWaitingSecondTap = false;
            }
            else
            {
                shift = false;
                bWaitingSecondTap = held <= kShortPressMaxUs;
            }
            bLastReleaseAt = nowUs;
            bPending = bDispatched = false;
        }
        else
        {
            if (bPending && ! bDispatched)
            {
                m2cState = M2cState::SetCv;
                entryRaw[1] = pots[1];
                entryRaw[2] = pots[2];
                pickupArmed[1] = pickupArmed[2] = false;
                bDispatched = true;
            }
            if (bDispatched && m2cState == M2cState::SetCv)
                m2cState = M2cState::Default;
            bPending = bDispatched = false;
        }
    }
}

void Brain::switchMode()
{
    settings.mode = settings.mode == Mode::MidiToCv ? Mode::Sequencer : Mode::MidiToCv;
    m2cPanic();
    playing = false;
    gateActive = false;
    animationUntil = nowUs + 6 * 60000.0;
    if (settings.mode == Mode::Sequencer)
        seqEnter();
}

void Brain::handleButtons()
{
    if (btnA != aHeld)
    {
        aHeld = btnA;
        if (btnA) onButtonAPress(); else { onButtonARelease(); afterRelease(); }
    }
    if (btnB != bHeld)
    {
        bHeld = btnB;
        if (btnB) onButtonBPress(); else { onButtonBRelease(); afterRelease(); }
    }

    // Long dual press toggles the app mode
    if (dualActive && ! dualHandled && nowUs - dualStartedAt >= kLongPressMinUs)
    {
        switchMode();
        dualHandled = true;
    }

    // MIDI2CV: dispatch a pending single press once the dual-press window has passed
    if (settings.mode == Mode::MidiToCv && ! dualActive && firstPressAt > 0 && nowUs - firstPressAt >= kDualPressWindowUs)
    {
        if (aPending && ! aDispatched && m2cState == M2cState::Default)
        {
            m2cState = M2cState::SetMidiChannel;
            entryRaw[0] = pots[0];
            pickupArmed[0] = false;
            aDispatched = true;
            aPending = false;
        }
        if (bPending && ! bDispatched && m2cState == M2cState::Default)
        {
            m2cState = M2cState::SetCv;
            entryRaw[1] = pots[1];
            entryRaw[2] = pots[2];
            pickupArmed[1] = pickupArmed[2] = false;
            bDispatched = true;
            bPending = false;
        }
    }
}

void Brain::afterRelease()
{
    if (aHeld || bHeld)
        return;
    // Short dual press in MIDI2CV = panic
    if (dualActive && ! dualHandled && nowUs - dualStartedAt <= kShortPressMaxUs && settings.mode == Mode::MidiToCv)
        m2cPanic();
    dualActive = false;
    dualHandled = false;
    firstPressAt = 0;
    dualStartedAt = 0;
    rootEditHold = false;
    aPending = bPending = aDispatched = bDispatched = false;
}

//==============================================================================
// Sequencer

void Brain::seqEnter()
{
    playing = false;
    gateActive = false;
    seqPos = 0;
    tickCounter = 0;
    nextTickDue = -1.0;
    shift = false;
    rootEdit = false;
    lastShiftContext = false;
    gateHistory.fill (0);
    overlayUntil = 0.0;
    potsPrev = pots;
    for (int i = 0; i < 3; ++i) { layerEntryRaw[(size_t) i] = pots[(size_t) i]; layerArmed[i] = true; }
    seqApplyPots (true);
}

void Brain::seqTogglePlay()
{
    playing = ! playing;
    nextTickDue = -1.0;
    tickCounter = 0;
    swingPending = false;
    if (! playing)
        gateActive = false;
}

uint8_t Brain::lengthWithSnap (uint8_t pot) const
{
    static constexpr uint8_t snaps[] = { 2, 4, 8, 16, 32 };
    const int mapped = 2 + (pot * 30) / 255;
    int best = 255, result = mapped;
    for (auto t : snaps)
    {
        const int dist = std::abs (mapped - t);
        const int window = std::max (1, t / 16);
        if (dist <= window && dist < best) { best = dist; result = t; }
    }
    return (uint8_t) result;
}

void Brain::seqApplyPots (bool force)
{
    if (shift != lastShiftContext)
    {
        // Layer changed: functions only take over once their pot moves (pickup).
        for (int i = 0; i < 3; ++i) { layerEntryRaw[(size_t) i] = pots[(size_t) i]; layerArmed[i] = false; }
        lastShiftContext = shift;
    }

    for (int i = 0; i < 3; ++i)
    {
        if (! layerArmed[i] && std::abs ((int) pots[(size_t) i] - (int) layerEntryRaw[(size_t) i]) > 1)
            layerArmed[i] = true;
    }

    auto changed = [&] (int i) { return force || (layerArmed[i] && pots[(size_t) i] != potsPrev[(size_t) i]); };

    if (rootEdit)
    {
        if (std::abs ((int) pots[1] - (int) rootRefOct) >= 3)
            octTranspose = (uint8_t) ((pots[1] * 6) / 256);
        if (std::abs ((int) pots[2] - (int) rootRefNote) >= 3)
            rootNote = (uint8_t) ((pots[2] * 12) / 256);
        uint8_t mask = barMask ((uint8_t) std::min (255, (rootNote + 1) * 255 / 12));
        setLedMask (mask, (rootNote == 1 || rootNote == 3 || rootNote == 6 || rootNote == 8 || rootNote == 10) ? 0.35f : 1.0f);
        overlayUntil = nowUs + kOverlayHoldUs;
        potsPrev = pots;
        return;
    }

    if (changed (0))
    {
        if (shift)
            swingPot = pots[0];
        else
        {
            externalSync = pots[0] == 0;
            if (! externalSync)
            {
                bpm = (uint16_t) (60 + (pots[0] * 180) / 255);
                tickIntervalUs = 60000000.0 / (bpm * 4.0);
            }
        }
        if (! force) showOverlay (0);
    }
    if (changed (1))
    {
        if (shift)
            quant = (Quantization) std::min (5, (pots[1] * 6) / 256);
        else
            rangeOct = (uint8_t) ((pots[1] * 7) / 256);
        if (! force) showOverlay (1);
    }
    if (changed (2))
    {
        if (shift)
        {
            const uint8_t newLength = lengthWithSnap (pots[2]);
            for (int i = seqLength; i < newLength; ++i)
            {
                seqA[(size_t) i].pitchQ8 = (uint16_t) (xorshift (rngA) % (kRandomMaxQ8 + 1u));
                seqA[(size_t) i].gate = randomU8 (rngA) >= 102u;
                seqB[(size_t) i].pitchQ8 = (uint16_t) (xorshift (rngB) % (kRandomMaxQ8 + 1u));
                seqB[(size_t) i].gate = randomU8 (rngB) >= 102u;
            }
            seqLength = newLength;
            if (seqPos >= seqLength) seqPos = 0;
        }
        else
            mutation = pots[2];
        if (! force) showOverlay (2);
    }
    potsPrev = pots;
}

void Brain::showOverlay (uint8_t potIndex)
{
    uint8_t value = pots[potIndex];
    if (shift && potIndex == 2)
    {
        const int lit = seqLength >= 32 ? 6 : seqLength >= 16 ? 5 : seqLength >= 8 ? 4 : seqLength >= 4 ? 3 : 2;
        overlayMask = (uint8_t) ((1u << lit) - 1u);
    }
    else if (potIndex == 0 && ! shift && externalSync)
        overlayMask = 0b100001;
    else
        overlayMask = barMask (value);
    overlayUntil = nowUs + kOverlayHoldUs;
}

uint16_t Brain::applyRange (uint16_t q8) const
{
    if (rangeOct == 0)
        return 0;
    uint32_t scaled = ((uint32_t) q8 * rangeOct + kSourceRangeOctaves / 2u) / kSourceRangeOctaves;
    const uint32_t maxQ8 = (uint32_t) rangeOct * 12u * kQ8PerSemitone;
    return (uint16_t) std::min (scaled, maxQ8);
}

uint16_t Brain::quantize (uint16_t q8) const
{
    static constexpr uint8_t chromatic[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
    static constexpr uint8_t major[] = { 0, 2, 4, 5, 7, 9, 11 };
    static constexpr uint8_t minor[] = { 0, 2, 3, 5, 7, 8, 10 };
    static constexpr uint8_t penta[] = { 0, 2, 4, 7, 9 };
    static constexpr uint8_t extra[] = { 0, 1, 4, 5, 7, 8, 11 };

    const uint8_t* scale = chromatic;
    int size = 12;
    switch (quant)
    {
        case Quantization::Major:      scale = major; size = 7; break;
        case Quantization::Minor:      scale = minor; size = 7; break;
        case Quantization::Pentatonic: scale = penta; size = 5; break;
        case Quantization::Extra:      scale = extra; size = 7; break;
        case Quantization::Unquantized:
        case Quantization::Chromatic:  break;
    }

    const uint32_t maxQ8 = (uint32_t) rangeOct * 12u * kQ8PerSemitone;
    const int32_t src = (int32_t) std::min<uint32_t> (q8, maxQ8);
    int32_t result = src;
    if (quant != Quantization::Unquantized)
    {
        const int32_t rounded = (src + kQ8PerSemitone / 2) / kQ8PerSemitone;
        const int32_t centre = rounded / 12;
        int32_t bestDist = 0x7FFFFFFF, best = rounded;
        for (int32_t oct = centre - 1; oct <= centre + 1; ++oct)
            for (int i = 0; i < size; ++i)
            {
                const int32_t cand = oct * 12 + scale[i];
                const int32_t dist = std::abs (cand * kQ8PerSemitone - src);
                if (dist < bestDist) { bestDist = dist; best = cand; }
            }
        result = best * kQ8PerSemitone;
    }
    result += ((int32_t) octTranspose * 12 + rootNote) * kQ8PerSemitone;
    return (uint16_t) std::clamp<int32_t> (result, 0, 10 * 12 * kQ8PerSemitone);
}

double Brain::nextStepIntervalUs() const
{
    const double swingDelta = tickIntervalUs * (swingPot / 255.0) * 0.5;
    if (swingDelta <= 0.0)
        return tickIntervalUs;
    return (tickCounter & 1u) == 0u ? std::max (1.0, tickIntervalUs - swingDelta) : tickIntervalUs + swingDelta;
}

void Brain::pushGateHistory (bool high)
{
    for (size_t i = 0; i + 1 < gateHistory.size(); ++i)
        gateHistory[i] = gateHistory[i + 1];
    gateHistory.back() = high ? 1 : 0;
}

void Brain::seqTick()
{
    const uint8_t idx = seqPos;
    if (mutation > 0)
    {
        if (randomU8 (rngA) < mutation) seqA[idx].pitchQ8 = (uint16_t) (xorshift (rngA) % (kRandomMaxQ8 + 1u));
        if (randomU8 (rngB) < mutation) seqB[idx].pitchQ8 = (uint16_t) (xorshift (rngB) % (kRandomMaxQ8 + 1u));
        if (randomU8 (rngA) < mutation) seqA[idx].gate = randomU8 (rngA) >= 102u;
    }

    const Step& a = seqA[idx];
    const Step& b = seqB[idx];
    if (a.gate)
    {
        cvA = (float) quantize (applyRange (a.pitchQ8)) / (12.0f * kQ8PerSemitone);
        cvB = (float) quantize (applyRange (b.pitchQ8)) / (12.0f * kQ8PerSemitone);
        gateActive = true;
        gateOffAt = nowUs + kGatePulseUs;
    }
    else
    {
        gateActive = false;
    }
    pushGateHistory (a.gate);
    if ((tickCounter % 4u) == 0u)
        buttonLed = 1.0f;
    ++tickCounter;
    seqPos = (uint8_t) ((idx + 1) % seqLength);
}

void Brain::externalStep (double intervalUs)
{
    if (! playing)
        return;
    tickIntervalUs = intervalUs;
    if (swingPending)
    {
        swingPending = false;
        seqTick();
    }
    const double swingDelta = intervalUs * (swingPot / 255.0) * 0.5;
    if (swingDelta > 0.0 && (tickCounter & 1u) == 1u)
    {
        swingPending = true;
        swingDueAt = nowUs + swingDelta;
        return;
    }
    seqTick();
}

void Brain::seqUpdate()
{
    seqApplyPots (false);

    // PULSE IN edges (priority external source)
    const bool pulseHigh = pulseInValue > (pulseWasHigh ? 0.8f : 1.5f);
    const bool pulseEdge = pulseHigh && ! pulseWasHigh;
    pulseWasHigh = pulseHigh;
    if (pulseEdge)
    {
        const double interval = nowUs - lastPulseAt;
        if (interval >= 1000.0 && interval <= 2000000.0)
            pulseIntervalUs += (interval - pulseIntervalUs) / 8.0;
        lastPulseAt = nowUs;
    }
    const double timeout = std::clamp (pulseIntervalUs * 2.5, 150000.0, 2000000.0);
    const bool pulseActive = nowUs - lastPulseAt <= timeout;

    if (gateActive && nowUs >= gateOffAt)
        gateActive = false;

    if (swingPending && nowUs >= swingDueAt)
    {
        swingPending = false;
        seqTick();
    }

    if (externalSync)
    {
        if (pulseActive)
        {
            if (pulseEdge)
                externalStep (pulseIntervalUs);
        }
        else if (hostHasPos)
        {
            // Host transport replaces MIDI clock: Start/Stop follow the DAW.
            if (hostPlaying && ! hostPlayingPrev)
            {
                playing = true;
                seqPos = 0;
                tickCounter = 0;
                lastHostStep = -1;
            }
            else if (! hostPlaying && hostPlayingPrev)
            {
                playing = false;
                gateActive = false;
            }
            hostPlayingPrev = hostPlaying;

            if (hostPlaying)
            {
                const double ppq = hostPpq + blockSampleIndex * (hostBpm / 60.0) / sr;
                const long long step = (long long) std::floor (ppq * 4.0 + 1.0e-9);
                if (step != lastHostStep)
                {
                    if (lastHostStep >= 0 || std::abs (ppq * 4.0 - (double) step) < 0.05)
                        externalStep (60000000.0 / (hostBpm * 4.0));
                    lastHostStep = step;
                }
            }
        }
    }
    else if (playing)
    {
        if (nextTickDue < 0.0)
            nextTickDue = nowUs;
        int safety = 0;
        while (nowUs >= nextTickDue && safety++ < 8)
        {
            seqTick();
            nextTickDue += nextStepIntervalUs();
        }
    }

    // LEDs: overlay for a second after a pot move, else the gate history FIFO.
    if (nowUs < overlayUntil && ! rootEdit)
        setLedMask (overlayMask);
    else if (! rootEdit)
    {
        uint8_t mask = 0;
        for (size_t i = 0; i < gateHistory.size(); ++i)
            if (gateHistory[i]) mask |= (uint8_t) (1u << i);
        setLedMask (mask);
    }
}

//==============================================================================

void Brain::setLedMask (uint8_t mask, float level)
{
    for (size_t i = 0; i < leds.size(); ++i)
        leds[i] = (mask >> i) & 1u ? level : 0.0f;
}

void Brain::process (Io& io)
{
    nowUs += usPerSample;
    pulseInValue = io.pulseIn;
    handleButtons();

    if (settings.mode == Mode::MidiToCv)
    {
        if (m2cState != M2cState::Default)
        {
            m2cUpdateSettingsFromPots();
        }
        else
        {
            if (gate)
            {
                for (auto& l : leds) l = 0.0f;
                leds[(size_t) playheadLed] = 1.0f;
                keyWasPressed = true;
            }
            else
            {
                if (keyWasPressed)
                {
                    playheadLed = (playheadLed + 1) % 6;
                    keyWasPressed = false;
                }
                for (auto& l : leds) l = 0.0f;
            }
        }
        io.pulse = gate ? kGateVolts : 0.0f;
        buttonLed = btnA ? 1.0f : 0.0f;
    }
    else
    {
        seqUpdate();
        io.pulse = gateActive ? kGateVolts : 0.0f;
        buttonLed = std::max (0.0f, buttonLed - (float) (usPerSample / 80000.0));
        if (! playing) buttonLed = 0.0f;
    }

    // Mode-switch animation: a quick chase over the LED strip.
    if (nowUs < animationUntil)
    {
        const int idx = std::clamp ((int) ((6 * 60000.0 - (animationUntil - nowUs)) / 60000.0), 0, 5);
        for (auto& l : leds) l = 0.0f;
        leds[(size_t) idx] = 1.0f;
    }

    io.out1 = cvA;
    io.out2 = cvB;
    ++blockSampleIndex;
}
} // namespace moduleur
