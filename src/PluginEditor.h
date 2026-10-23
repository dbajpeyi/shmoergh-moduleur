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

private:
    // The panel in its native 968 x 1210 units; scaled as a whole.
    struct Content : public juce::Component
    {
        std::function<void (const juce::MouseEvent&)> onPopup;
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu() && onPopup)
                onPopup (e);
        }
    };

    void jackPressed (moduleur::ui::Jack&, const juce::MouseEvent&) override;
    void jackDragged (moduleur::ui::Jack&, const juce::MouseEvent&) override;
    void jackReleased (moduleur::ui::Jack&, const juce::MouseEvent&) override;
    moduleur::ui::Jack* jackAt (juce::Point<float> panelPos, bool wantOutput);

    void timerCallback() override;
    void showMenu();
    void applyTheme();
    void setScale (float s);

    ModuleurProcessor& processor;
    Content content;
    moduleur::ui::PanelBackground background;
    moduleur::ui::CableLayer cables;
    juce::TooltipWindow tooltips { this, 600 };

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
