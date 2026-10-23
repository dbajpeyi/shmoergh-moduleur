#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

namespace moduleur::ui
{
// Draws the patch cables on top of the panel. Purely visual: the jacks handle
// the mouse and tell the editor, which edits the processor's cable list.
class CableLayer : public juce::Component
{
public:
    explicit CableLayer (ModuleurProcessor& p) : processor (p) { setInterceptsMouseClicks (false, false); }

    void paint (juce::Graphics&) override;

    void setDrag (int fixedPort, juce::Point<float> loosePos, juce::Colour colour);
    void clearDrag();
    void setOpacity (float o) { opacity = o; repaint(); }

    static juce::Point<float> portPosition (int port);

private:
    void drawCable (juce::Graphics&, juce::Point<float> a, juce::Point<float> b, juce::Colour colour, bool looseEnd);
    void drawPlug (juce::Graphics&, juce::Point<float> p, juce::Colour colour);

    ModuleurProcessor& processor;
    bool dragging = false;
    int dragFixed = -1;
    juce::Point<float> dragPos;
    juce::Colour dragColour;
    float opacity = 0.9f;
};
} // namespace moduleur::ui
