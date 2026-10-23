#include "CableLayer.h"

namespace moduleur::ui
{
juce::Point<float> CableLayer::portPosition (int port)
{
    return { kPorts[(size_t) port].x, kPorts[(size_t) port].y };
}

void CableLayer::setDrag (int fixedPort, juce::Point<float> loosePos, juce::Colour colour)
{
    dragging = true;
    dragFixed = fixedPort;
    dragPos = loosePos;
    dragColour = colour;
    repaint();
}

void CableLayer::clearDrag()
{
    dragging = false;
    repaint();
}

void CableLayer::drawPlug (juce::Graphics& g, juce::Point<float> p, juce::Colour colour)
{
    // 3.5 mm patch cable plug seen from the front: coloured boot with a ring.
    const float r = 12.5f;
    g.setColour (juce::Colours::black.withAlpha (0.35f * opacity));
    g.fillEllipse (p.x - r + 1.5f, p.y - r + 3.0f, r * 2, r * 2);
    juce::ColourGradient boot (colour.brighter (0.35f), p.x - r * 0.6f, p.y - r * 0.8f, colour.darker (0.45f), p.x + r * 0.6f, p.y + r, false);
    g.setGradientFill (boot);
    g.fillEllipse (p.x - r, p.y - r, r * 2, r * 2);
    g.setColour (colour.darker (0.6f));
    g.drawEllipse (p.x - r, p.y - r, r * 2, r * 2, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.drawEllipse (p.x - r * 0.55f, p.y - r * 0.55f, r * 1.1f, r * 1.1f, 1.4f);
}

void CableLayer::drawCable (juce::Graphics& g, juce::Point<float> a, juce::Point<float> b, juce::Colour colour, bool looseEnd)
{
    // Gravity: a cubic with both handles pulled down, more sag for longer cables.
    const float dist = a.getDistanceFrom (b);
    const float sag = juce::jmin (180.0f, 24.0f + dist * 0.38f);
    juce::Path p;
    p.startNewSubPath (a);
    p.cubicTo (a.translated (0, sag), b.translated (0, sag), b);

    g.setColour (juce::Colours::black.withAlpha (0.28f * opacity));
    g.strokePath (p, juce::PathStrokeType (8.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                  juce::AffineTransform::translation (2.5f, 6.0f));
    g.setColour (colour.darker (0.45f).withMultipliedAlpha (opacity));
    g.strokePath (p, juce::PathStrokeType (8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (colour.withMultipliedAlpha (opacity));
    g.strokePath (p, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (colour.brighter (0.6f).withAlpha (0.35f * opacity));
    g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                  juce::AffineTransform::translation (-1.2f, -1.2f));

    drawPlug (g, a, colour);
    drawPlug (g, b, looseEnd ? colour.withMultipliedAlpha (0.85f) : colour);
}

void CableLayer::paint (juce::Graphics& g)
{
    for (const auto& c : processor.getCables())
        drawCable (g, portPosition (c.from), portPosition (c.to), c.colour, false);
    if (dragging && dragFixed >= 0)
        drawCable (g, portPosition (dragFixed), dragPos, dragColour, true);
}
} // namespace moduleur::ui
