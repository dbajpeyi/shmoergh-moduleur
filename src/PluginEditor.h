#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "ui/CableLayer.h"
#include "ui/Controls.h"

class ModuleurEditor : public juce::AudioProcessorEditor,
                       public moduleur::ui::Jack::Listener,
                       private juce::Timer
{
public:
    explicit ModuleurEditor (ModuleurProcessor&);
    ~ModuleurEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr float kPanelWidth = 968.0f;
    static constexpr float kPanelHeight = 1210.0f;
    static constexpr float kKeyboardHeight = 150.0f;   // optional strip under the panel

private:
    // The panel in its native 968 x 1210 units; scaled as a whole. Right-click,
    // or a long press on touch screens, opens the menu.
    struct Content : public juce::Component
    {
        std::function<void()> onPopup;
        void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff111111)); }
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu())
            {
                if (onPopup) onPopup();
                return;
            }
            pressed = true;
            moved = false;
            const int id = ++pressId;
            juce::Timer::callAfterDelay (kLongPressMs, [safe = juce::Component::SafePointer<Content> (this), id]
            {
                if (safe != nullptr && safe->pressed && ! safe->moved && safe->pressId == id && safe->onPopup)
                {
                    safe->pressed = false;
                    safe->onPopup();
                }
            });
        }
        void mouseDrag (const juce::MouseEvent& e) override { moved = moved || e.getDistanceFromDragStart() > 8; }
        void mouseUp (const juce::MouseEvent&) override { pressed = false; }

        int pressId = 0;
        bool pressed = false, moved = false;
    };

    static constexpr int kLongPressMs = 550;

    void jackPressed (moduleur::ui::Jack&, const juce::MouseEvent&) override;
    void jackDragged (moduleur::ui::Jack&, const juce::MouseEvent&) override;
    void jackReleased (moduleur::ui::Jack&, const juce::MouseEvent&) override;
    moduleur::ui::Jack* jackAt (juce::Point<float> panelPos, bool wantOutput);

    void timerCallback() override;
    void showMenu();
    void applyTheme();
    void setScale (float s);
    bool keyboardVisible() const;
    void setKeyboardVisible (bool);
    float contentHeight() const { return kPanelHeight + (keyboardVisible() ? kKeyboardHeight : 0.0f); }
    void updateContentSize();
    void removeCablesAt (int port);

    ModuleurProcessor& processor;
    Content content;
    moduleur::ui::PanelBackground background;
    moduleur::ui::CableLayer cables;
    juce::TooltipWindow tooltips { this, 600 };
    juce::MidiKeyboardComponent keyboard;

    std::vector<std::unique_ptr<juce::Component>> controls;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;
    std::vector<std::unique_ptr<moduleur::ui::Jack>> jacks;
    std::array<moduleur::ui::LedLight*, moduleur::NumLeds> leds {};
    moduleur::ui::PushButton* buttonA = nullptr;

    // Drag state
    int dragFixedPort = -1;
    juce::Colour dragColour;
    bool dragMoved = false;
    ModuleurProcessor::Cable grabbed;   // cable lifted out of an input jack
    moduleur::ui::Jack* hoverTarget = nullptr;
    int lastCablesVersion = -1;
};
