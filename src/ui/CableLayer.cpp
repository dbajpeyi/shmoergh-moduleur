#include "CableLayer.h"

#include "Controls.h"

namespace moduleur::ui
{
namespace
{
// Cable body -> plug colours of the design. Unknown colours (e.g. from patches
// saved by older versions) get a slightly darker plug.
juce::Colour plugColourFor (juce::Colour body)
{
    static const std::pair<juce::uint32, juce::uint32> pairs[] = {
        { 0xffffc900, 0xffebb300 }, { 0xffff8700, 0xfff67600 }, { 0xff0074ef, 0xff0066dd },
        { 0xff00af6f, 0xff009960 }, { 0xffff4100, 0xfffd3c00 },
    };
    for (auto [b, p] : pairs)
        if (body.getARGB() == b)
            return juce::Colour (p);
    return body.darker (0.12f);
}

juce::Path cablePath (juce::Point<float> a, juce::Point<float> b, float sag)
{
    juce::Path p;
    p.startNewSubPath (a);
    p.cubicTo (a.translated (0, sag), b.translated (0, sag), b);
    return p;
}

// Droop fitted to the cables in the design (about 66 pt for a 190 pt span,
// 103 pt for a 580 pt span).
float designSag (juce::Point<float> a, juce::Point<float> b)
{
    return juce::jmin (230.0f, 60.0f + 0.128f * a.getDistanceFrom (b));
}
} // namespace

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
    if (themeOf (*this).design)
    {
        // Design cables: flat 10 pt bands with round ends and a soft shadow; the
        // plug is a slightly darker Ø32 pt disc filling the jack face.
        struct Wire { juce::Point<float> a, b; juce::Colour colour; };
        std::vector<Wire> wires;
        for (const auto& c : processor.getCables())
            wires.push_back ({ portPosition (c.from), portPosition (c.to), c.colour });
        if (dragging && dragFixed >= 0)
            wires.push_back ({ portPosition (dragFixed), dragPos, dragColour });

        const float plugR = 16.0f * kPtToPanel;
        const juce::PathStrokeType stroke (10.0f * kPtToPanel, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        const juce::DropShadow shadow (juce::Colours::black.withAlpha (themeOf (*this).palette.cableShadow * opacity),
                                       juce::roundToInt (9.0f * kPtToPanel), { 2, 4 });

        for (size_t i = 0; i < wires.size(); ++i)
        {
            const auto& w = wires[i];
            const bool loose = dragging && i + 1 == wires.size();
            g.setColour (plugColourFor (w.colour).withMultipliedAlpha (opacity));
            g.fillEllipse (w.a.x - plugR, w.a.y - plugR, plugR * 2, plugR * 2);
            if (! loose)
                g.fillEllipse (w.b.x - plugR, w.b.y - plugR, plugR * 2, plugR * 2);
        }
        for (const auto& w : wires)
        {
            juce::Path outline;
            stroke.createStrokedPath (outline, cablePath (w.a, w.b, designSag (w.a, w.b)));
            shadow.drawForPath (g, outline);
            g.setColour (w.colour.withMultipliedAlpha (opacity));
            g.fillPath (outline);
        }
        return;
    }

    for (const auto& c : processor.getCables())
        drawCable (g, portPosition (c.from), portPosition (c.to), c.colour, false);
    if (dragging && dragFixed >= 0)
        drawCable (g, portPosition (dragFixed), dragPos, dragColour, true);
}
} // namespace moduleur::ui
