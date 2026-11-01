#include "PluginEditor.h"

using namespace moduleur;
using namespace moduleur::ui;

namespace
{
constexpr float kDefaultScale = 0.62f;   // of the stage (1081 x 1443 units)

juce::Rectangle<int> boxAt (float x, float y, float w, float h)
{
    return juce::Rectangle<float> (x - w * 0.5f, y - h * 0.5f, w, h).getSmallestIntegerContainer();
}

const char* kHelp =
    "PATCHING\n"
    "Drag from any jack to another to patch a cable. Drag a cable out of an input to move it, "
    "or drop it on the panel to unplug it. Right-click a jack to remove its cables.\n\n"
    "The default patch board is fitted: OSC 1+2, SUB and NOISE feed the mixer, the mixer feeds the "
    "filter, the filter feeds VCA 1 and the output. LFO TRI goes to filter CV/1 and ENV 2 to CV/2 "
    "(turn the LEVEL attenuverters to hear them). Plugging into a jack breaks its default connection.\n\n"
    "BRAIN (Le Controlleur firmware)\n"
    "MIDI from your DAW arrives at the Brain. MIDI to CV: first CV out = pitch, second = velocity, "
    "PULSE = gate.\n"
    "Hold 1 + turn A: MIDI channel. Hold 2 + turn B: pitch on the first or second CV out; 2 + C: "
    "velocity, mod wheel, unison or duophonic. Tap 1+2: panic.\n"
    "Hold 1+2 for 1.5 s to switch to the Sequencer: 1 play/pause, A tempo (fully left = sync to "
    "PULSE in or the DAW transport), B range, C randomness. Hold 2 for swing / scale / length; "
    "double-tap and hold 2 to transpose.\n\n"
    "HOST L / HOST R carry the plugin's (sidechain) audio input.\n\n"
    "Double-click a knob to reset it. Right-click the panel for dark or light appearance, size and patch options.";
} // namespace

ModuleurEditor::ModuleurEditor (ModuleurProcessor& p)
    : AudioProcessorEditor (p), processor (p), cables (p)
{
    setLookAndFeel (&lookAndFeel);
    stage.setSize (juce::roundToInt (kStageWidth), juce::roundToInt (kStageHeight));
    stage.onPopup = [this] (const juce::MouseEvent&) { showMenu(); };
    addAndMakeVisible (stage);
    stageBackground.setBounds (stage.getLocalBounds());
    stage.addAndMakeVisible (stageBackground);

    content.setBounds (juce::roundToInt (kPanelOriginX), juce::roundToInt (kPanelOriginY), (int) kPanelWidth, (int) kPanelHeight);
    content.onPopup = [this] (const juce::MouseEvent&) { showMenu(); };
    stage.addAndMakeVisible (content);

    background.setBounds (content.getLocalBounds());
    content.addAndMakeVisible (background);

    // --- controls from the parameter table ---
    for (int i = 0; i < NumParams; ++i)
    {
        const auto& def = kParams[(size_t) i];
        auto* param = processor.apvts.getParameter (def.id);
        switch (def.kind)
        {
            case ControlKind::Knob:
            case ControlKind::Trimmer:
            case ControlKind::BigKnob:
            {
                auto knob = std::make_unique<Knob> (def.kind, i);
                const float size = def.kind == ControlKind::BigKnob ? Knob::kBigSize : Knob::kTrimmerSize;
                knob->setBounds (boxAt (def.x, def.y, size * 1.3f + 4.0f, size * 1.3f + 4.0f));
                knob->setDoubleClickReturnValue (true, def.defaultValue);
                attachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, def.id, *knob));
                content.addAndMakeVisible (*knob);
                controls.push_back (std::move (knob));
                break;
            }
            case ControlKind::Slider:
            {
                const bool longSlot = i == P_ENV1_SUSTAIN || i == P_ENV2_SUSTAIN;
                // Cap travel between the slot's rounded ends: 112 pt (mixer) / 172 pt (sustain).
                const float travel = (longSlot ? 172.0f : 112.0f) * kPtToPanel;
                auto fader = std::make_unique<Fader> (i, travel);
                fader->setBounds (boxAt (def.x, def.y, 34, travel + 46));
                fader->setDoubleClickReturnValue (true, def.defaultValue);
                attachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, def.id, *fader));
                content.addAndMakeVisible (*fader);
                controls.push_back (std::move (fader));
                break;
            }
            case ControlKind::Selector:
            {
                auto sel = std::make_unique<WaveSelector> (*param);
                sel->setBounds (juce::Rectangle<float> (def.x - Knob::kBigSize * 0.5f - WaveSelector::kLeftPad, def.y - 80.0f, 166.0f, 160.0f).getSmallestIntegerContainer());
                sel->setTooltip (juce::String (def.name) + " (click to step, or click a legend)");
                content.addAndMakeVisible (*sel);
                controls.push_back (std::move (sel));
                break;
            }
            case ControlKind::Toggle:
            {
                if (i == P_POWER)
                {
                    auto power = std::make_unique<PowerButton> (*param);
                    power->setBounds (boxAt (def.x, def.y, 56, 56));
                    content.addAndMakeVisible (*power);
                    controls.push_back (std::move (power));
                }
                break;
            }
        }
    }

    // --- Brain buttons ---
    {
        auto a = std::make_unique<PushButton> (processor.buttonA, true);
        a->setBounds (boxAt (786.1f, 786.1f, 54, 54));
        buttonA = a.get();
        content.addAndMakeVisible (*a);
        controls.push_back (std::move (a));
        auto b = std::make_unique<PushButton> (processor.buttonB, false);
        b->setBounds (boxAt (846.6f, 786.1f, 54, 54));
        content.addAndMakeVisible (*b);
        controls.push_back (std::move (b));
    }

    // --- LEDs ---
    auto addLed = [this] (int id, float x, float y)
    {
        auto led = std::make_unique<LedLight>();
        led->setBounds (boxAt (x, y, 44, 44));
        led->setInterceptsMouseClicks (false, false);
        leds[(size_t) id] = led.get();
        content.addAndMakeVisible (*led);
        controls.push_back (std::move (led));
    };
    addLed (LED_ENV1, 196.5f, 650.1f);
    addLed (LED_ENV2, 438.4f, 650.1f);
    addLed (LED_LFO, 619.8f, 650.1f);
    addLed (LED_DRIVE, 680.3f, 650.1f);
    const float brainLedX[] = { 771.0f, 801.3f, 831.5f, 861.7f, 892.0f, 922.2f };
    for (int i = 0; i < 6; ++i)
        addLed (LED_BRAIN1 + i, brainLedX[i], 861.5f);
    addLed (LED_POWER, 846.6f, 1149.0f);

    // --- jacks ---
    for (int port = 0; port < NumPorts; ++port)
    {
        auto jack = std::make_unique<Jack> (port, this);
        jack->setBounds (boxAt (kPorts[(size_t) port].x, kPorts[(size_t) port].y, Jack::kSize, Jack::kSize));
        content.addAndMakeVisible (*jack);
        jacks.push_back (std::move (jack));
    }
    {
        auto midi = std::make_unique<Jack> (-1, nullptr, false);
        midi->setTooltip ("MIDI In: receives MIDI from the host");
        midi->setBounds (boxAt (907.1f, 786.1f, Jack::kSize, Jack::kSize));
        content.addAndMakeVisible (*midi);
        controls.push_back (std::move (midi));
    }

    cables.setBounds (content.getLocalBounds());
    content.addAndMakeVisible (cables);

    applyTheme();

    // Read the saved size first: setting the limits triggers resized().
    float scale = (float) processor.uiState.getProperty ("stageScale", kDefaultScale);
    scale = std::isfinite (scale) ? juce::jlimit (0.4f, 1.5f, scale) : kDefaultScale;
    setResizable (true, true);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio (kStageWidth / kStageHeight);
    setResizeLimits (juce::roundToInt (kStageWidth * 0.4f), juce::roundToInt (kStageHeight * 0.4f),
                     juce::roundToInt (kStageWidth * 1.5f), juce::roundToInt (kStageHeight * 1.5f));
    setSize (juce::roundToInt (kStageWidth * scale), juce::roundToInt (kStageHeight * scale));

    startTimerHz (30);
}

ModuleurEditor::~ModuleurEditor()
{
    setLookAndFeel (nullptr);
    processor.buttonA = false;
    processor.buttonB = false;
}

void ModuleurEditor::paint (juce::Graphics& g)
{
    g.fillAll (lookAndFeel.theme.light ? juce::Colour (0xfff8f8f7) : juce::Colour (0xff090909));
}

void ModuleurEditor::resized()
{
    const float scale = (float) getWidth() / kStageWidth;
    stage.setTransform (juce::AffineTransform::scale (scale));
    processor.uiState.setProperty ("stageScale", scale, nullptr);
}

void ModuleurEditor::setScale (float s)
{
    setSize (juce::roundToInt (kStageWidth * s), juce::roundToInt (kStageHeight * s));
}

void ModuleurEditor::applyTheme()
{
    // The design comes in dark and light; the legacy looks only when compiled in.
    const auto id = processor.uiState.getProperty ("theme", "figma").toString();
    if (id == "figma-light")
        lookAndFeel.theme = Theme::figmaLight();
    else if (! kPanelThemesEnabled || id == "figma")
        lookAndFeel.theme = Theme::figma();
    else
        lookAndFeel.theme = id == "white" ? Theme::white() : (id == "black" ? Theme::black() : Theme::original());
    background.setTheme (lookAndFeel.theme);
    background.setVisible (! lookAndFeel.theme.design);
    stageBackground.setLight (lookAndFeel.theme.light);
    stage.repaint();
    repaint();
    const float opacity = (float) processor.uiState.getProperty ("cableOpacity", 0.9f);
    cables.setOpacity (std::isfinite (opacity) ? juce::jlimit (0.2f, 1.0f, opacity) : 0.9f);
}

void ModuleurEditor::timerCallback()
{
    if (const int v = processor.cablesVersion.load(); v != lastCablesVersion)
    {
        lastCablesVersion = v;
        cables.repaint();
    }
    for (int i = 0; i < NumLeds; ++i)
    {
        const float v = processor.ledLevels[(size_t) i].load (std::memory_order_relaxed);
        if (i == LED_BRAIN_A)
        {
            if (buttonA != nullptr)
                buttonA->setLed (v);
        }
        else if (leds[(size_t) i] != nullptr)
        {
            leds[(size_t) i]->setLevel (v);
        }
    }
}

//==============================================================================
// Patching

Jack* ModuleurEditor::jackAt (juce::Point<float> pos, bool wantOutput)
{
    Jack* best = nullptr;
    float bestDist = 24.0f;
    for (auto& j : jacks)
    {
        if (isOutput (j->getPort()) != wantOutput)
            continue;
        const float d = pos.getDistanceFrom (CableLayer::portPosition (j->getPort()));
        if (d < bestDist)
        {
            bestDist = d;
            best = j.get();
        }
    }
    return best;
}

void ModuleurEditor::jackPressed (Jack& jack, const juce::MouseEvent& e)
{
    const int port = jack.getPort();
    dragMoved = false;
    grabbed = {};

    if (e.mods.isPopupMenu())
    {
        if (isOutput (port))
        {
            std::vector<int> inputs;
            for (const auto& c : processor.getCables())
                if (c.from == port)
                    inputs.push_back (c.to);
            for (int in : inputs)
                processor.removeCableToInput (in);
        }
        else
        {
            processor.removeCableToInput (port);
        }
        dragFixedPort = -1;
        return;
    }

    if (! isOutput (port))
    {
        for (const auto& c : processor.getCables())
        {
            if (c.to == port)
            {
                // Lift this cable out of the input; keep its output end plugged.
                grabbed = c;
                dragFixedPort = c.from;
                dragColour = c.colour;
                processor.removeCableToInput (port);
                cables.setDrag (dragFixedPort, CableLayer::portPosition (port), dragColour);
                return;
            }
        }
    }

    dragFixedPort = port;
    dragColour = processor.nextCableColour();
    cables.setDrag (dragFixedPort, CableLayer::portPosition (port), dragColour);
}

void ModuleurEditor::jackDragged (Jack&, const juce::MouseEvent& e)
{
    if (dragFixedPort < 0)
        return;
    const auto pos = e.getEventRelativeTo (&content).position;
    dragMoved = dragMoved || e.getDistanceFromDragStart() > 4;
    cables.setDrag (dragFixedPort, pos, dragColour);

    auto* target = jackAt (pos, ! isOutput (dragFixedPort));
    if (target != hoverTarget)
    {
        if (hoverTarget != nullptr)
            hoverTarget->setHighlighted (false);
        hoverTarget = target;
        if (hoverTarget != nullptr)
            hoverTarget->setHighlighted (true);
    }
}

void ModuleurEditor::jackReleased (Jack&, const juce::MouseEvent& e)
{
    if (dragFixedPort < 0)
        return;
    const auto pos = e.getEventRelativeTo (&content).position;
    auto* target = jackAt (pos, ! isOutput (dragFixedPort));

    if (hoverTarget != nullptr)
        hoverTarget->setHighlighted (false);
    hoverTarget = nullptr;
    cables.clearDrag();

    if (target != nullptr)
    {
        const int from = isOutput (dragFixedPort) ? dragFixedPort : target->getPort();
        const int to = isOutput (dragFixedPort) ? target->getPort() : dragFixedPort;
        processor.addCable (from, to, dragColour);
    }
    else if (! dragMoved && grabbed.to >= 0)
    {
        // A plain click on a patched input: put the cable back.
        processor.addCable (grabbed.from, grabbed.to, grabbed.colour);
    }
    dragFixedPort = -1;
    grabbed = {};
}

//==============================================================================
void ModuleurEditor::showMenu()
{
    juce::PopupMenu menu;
    const auto themeId = processor.uiState.getProperty ("theme", "figma").toString();
    auto* defaultPatch = processor.apvts.getParameter ("default_patch");
    const bool boardFitted = defaultPatch->getValue() > 0.5f;
    const int brainMode = processor.brainModeForUi.load();

    juce::PopupMenu theme;
    auto addTheme = [this, &theme, &themeId] (const char* name, const char* id)
    {
        theme.addItem (name, true, themeId == id, [this, id] { processor.uiState.setProperty ("theme", id, nullptr); applyTheme(); });
    };
    addTheme ("Dark", "figma");
    addTheme ("Light", "figma-light");
    if (kPanelThemesEnabled)
    {
        theme.addSeparator();
        addTheme ("Shaded hardware", "original");
        addTheme ("White FR4 (DIY panel)", "white");
        addTheme ("Black FR4 (DIY panel)", "black");
    }
    menu.addSubMenu ("Appearance", theme);

    juce::PopupMenu size;
    for (float s : { 0.45f, 0.55f, 0.62f, 0.75f, 0.9f, 1.1f })
        size.addItem (juce::String (juce::roundToInt (s * 100.0f)) + "%", [this, s] { setScale (s); });
    menu.addSubMenu ("Size", size);

    juce::PopupMenu opacity;
    const float current = (float) processor.uiState.getProperty ("cableOpacity", 0.9f);
    for (float o : { 1.0f, 0.9f, 0.6f, 0.35f })
        opacity.addItem (juce::String (juce::roundToInt (o * 100.0f)) + "%", true, std::abs (current - o) < 0.01f,
                         [this, o] { processor.uiState.setProperty ("cableOpacity", o, nullptr); applyTheme(); });
    menu.addSubMenu ("Cable opacity", opacity);

    menu.addSeparator();
    juce::PopupMenu brain;
    brain.addItem ("MIDI to CV", true, brainMode == 0, [this] { processor.requestBrainMode (0); });
    brain.addItem ("Sequencer", true, brainMode == 1, [this] { processor.requestBrainMode (1); });
    menu.addSubMenu ("Brain firmware", brain);
    menu.addItem ("Default patch board fitted", true, boardFitted,
                  [defaultPatch, boardFitted] { defaultPatch->setValueNotifyingHost (boardFitted ? 0.0f : 1.0f); });
    menu.addItem ("Remove all cables", [this] { processor.clearCables(); });
    menu.addItem ("Init patch (cables + knobs)", [this]
    {
        processor.resetToInitPatch();
        for (const auto& def : kParams)
            if (auto* prm = processor.apvts.getParameter (def.id))
                prm->setValueNotifyingHost (prm->convertTo0to1 (def.defaultValue));
    });
    menu.addItem ("Panic (all notes off)", [this] { processor.panic(); });
    menu.addSeparator();
    menu.addItem ("Quick guide...", []
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, "Moduleur", kHelp);
    });

    menu.showMenuAsync (juce::PopupMenu::Options().withParentComponent (this));
}
