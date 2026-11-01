#include "Controls.h"

#include "DesignParts.h"
#include "ModuleurAssets.h"

namespace moduleur::ui
{
namespace
{
std::unique_ptr<juce::Drawable> loadSvg (const char* data, int size)
{
    if (auto xml = juce::XmlDocument::parse (juce::String::fromUTF8 (data, size)))
        return juce::Drawable::createFromSVG (*xml);
    return {};
}

juce::Rectangle<float> centred (juce::Rectangle<float> r, float size)
{
    return r.withSizeKeepingCentre (size, size);
}

void drawShadow (juce::Graphics& g, juce::Point<float> c, float radius, float offset, float alpha)
{
    const float spread = radius * 1.22f;
    juce::ColourGradient grad (juce::Colours::black.withAlpha (alpha), c.x + offset * 0.4f, c.y + offset,
                               juce::Colours::transparentBlack, c.x + offset * 0.4f + spread, c.y + offset, true);
    grad.addColour (0.75, juce::Colours::black.withAlpha (alpha * 0.55f));
    g.setGradientFill (grad);
    g.fillEllipse (c.x - spread + offset * 0.4f, c.y - spread + offset, spread * 2.0f, spread * 2.0f);
}

float positionToAngle (float pos)
{
    // PTV09 pots: 300 degrees of travel, centre pointing up.
    return juce::degreesToRadians (-150.0f + 300.0f * pos);
}

// Gradient-filled disc lit from the top left.
void fillCap (juce::Graphics& g, juce::Point<float> c, float r, juce::Colour light, juce::Colour shade)
{
    juce::ColourGradient face (light, c.x - r * 0.7f, c.y - r * 0.9f, shade, c.x + r * 0.6f, c.y + r, false);
    g.setGradientFill (face);
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
}

// Nut colours for jacks and panel hardware.
juce::ColourGradient nutGradient (const Theme& t, juce::Point<float> c, float r)
{
    if (t.blackJacks)
        return { juce::Colour (0xff3c3c3e), c.x - r, c.y - r, juce::Colour (0xff0b0b0c), c.x + r, c.y + r, false };
    return { juce::Colour (0xfff0f0ed), c.x - r, c.y - r, juce::Colour (0xff8f8f8b), c.x + r, c.y + r, false };
}
} // namespace

namespace
{
// Paths of the design parts, in points around their centre.
const juce::Path& trimmerCapPath()
{
    static const juce::Path p = []
    {
        auto path = juce::Drawable::parseSVGPath (design::kTrimmerCapPath);
        path.applyTransform (juce::AffineTransform::translation (-design::kTrimmerCapCentreX, -design::kTrimmerCapCentreY));
        return path;
    }();
    return p;
}

const juce::Path& cutoffPath()
{
    static const juce::Path p = []
    {
        auto path = juce::Drawable::parseSVGPath (design::kCutoffPath);
        path.applyTransform (juce::AffineTransform::translation (-design::kCutoffCentreX, -design::kCutoffCentreY));
        return path;
    }();
    return p;
}

// Maps design points around a part's centre onto the component (panel units).
juce::AffineTransform designTransform (juce::Point<float> centre, float rotation = 0.0f)
{
    return juce::AffineTransform::rotation (rotation).scaled (kPtToPanel).translated (centre);
}

void fillDesignCircle (juce::Graphics& g, juce::Point<float> centre, float radiusPt, juce::Colour colour)
{
    const float r = radiusPt * kPtToPanel;
    g.setColour (colour);
    g.fillEllipse (centre.x - r, centre.y - r, r * 2.0f, r * 2.0f);
}
} // namespace

const Theme& themeOf (const juce::Component& c)
{
    static const Theme fallback = Theme::original();
    if (auto* lf = dynamic_cast<const ThemeLookAndFeel*> (&c.getLookAndFeel()))
        return lf->theme;
    return fallback;
}

//==============================================================================
StageBackground::StageBackground()
{
    image = juce::ImageCache::getFromMemory (ModuleurAssets::stage3x_png, ModuleurAssets::stage3x_pngSize);
    setInterceptsMouseClicks (false, false);
    setBufferedToImage (true);   // re-rendered only when the editor is resized
}

void StageBackground::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff090909));
    if (image.isValid())
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (image, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
    }
}

//==============================================================================
PanelBackground::PanelBackground()
{
    copper = loadSvg (ModuleurAssets::copper_svg, ModuleurAssets::copper_svgSize);
    silk = loadSvg (ModuleurAssets::silkscreen_svg, ModuleurAssets::silkscreen_svgSize);
    setInterceptsMouseClicks (false, false);
    setBufferedToImage (true);
}

void PanelBackground::setTheme (const Theme& t)
{
    theme = t;
    repaint();
}

void PanelBackground::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    // Board / powder-coated panel with a soft sheen
    g.setColour (theme.panel);
    g.fillRect (bounds);
    {
        juce::ColourGradient sheen (juce::Colours::white.withAlpha (theme.dark ? 0.035f : 0.18f), 0, 0,
                                    juce::Colours::black.withAlpha (theme.dark ? 0.12f : 0.05f), 0, bounds.getBottom(), false);
        g.setGradientFill (sheen);
        g.fillRect (bounds);
    }

    // Copper layer (hatching, outlines) and print layer. Both SVGs share the
    // panel's 968 x 1210 viewBox.
    const auto artBounds = juce::Rectangle<float> (0, 0, 968.0f, 1210.0f);
    if (copper != nullptr)
    {
        auto c = copper->createCopy();
        c->replaceColour (juce::Colours::black, theme.copper);
        c->drawWithin (g, artBounds, juce::RectanglePlacement::stretchToFit, 1.0f);
    }
    if (silk != nullptr)
    {
        auto s = silk->createCopy();
        s->replaceColour (juce::Colours::black, theme.print);
        s->drawWithin (g, artBounds, juce::RectanglePlacement::stretchToFit, 1.0f);
    }

    // Plugin additions in the bottom strip: host audio input jacks.
    g.setColour (theme.print);
    g.setFont (juce::FontOptions (11.5f));
    g.drawText ("HOST IN/L", juce::Rectangle<float> (262.4f, 1171.0f, 80.0f, 16.0f), juce::Justification::centred);
    g.drawText ("HOST IN/R", juce::Rectangle<float> (322.8f, 1171.0f, 80.0f, 16.0f), juce::Justification::centred);

    // Black M3 hex socket screws
    static constexpr float screws[][2] = {
        { 60.5f, 60.5f }, { 907.1f, 60.5f }, { 23.8f, 167.4f }, { 218.1f, 167.4f }, { 265.7f, 167.4f }, { 460.0f, 167.4f },
        { 507.6f, 167.4f }, { 701.9f, 167.4f }, { 749.5f, 167.4f }, { 943.7f, 167.4f }, { 23.8f, 558.2f }, { 218.1f, 558.2f },
        { 265.7f, 558.2f }, { 460.0f, 558.2f }, { 507.6f, 558.2f }, { 701.9f, 558.2f }, { 749.5f, 558.2f }, { 943.7f, 558.2f },
        { 23.8f, 604.7f }, { 483.8f, 604.7f }, { 943.7f, 604.7f }, { 23.8f, 651.2f }, { 218.1f, 651.2f }, { 265.7f, 651.2f },
        { 460.0f, 651.2f }, { 507.6f, 651.2f }, { 701.9f, 651.2f }, { 749.5f, 651.2f }, { 943.7f, 651.2f }, { 23.8f, 1042.0f },
        { 218.1f, 1042.0f }, { 265.7f, 1042.0f }, { 460.0f, 1042.0f }, { 507.6f, 1042.0f }, { 701.9f, 1042.0f }, { 749.5f, 1042.0f },
        { 943.7f, 1042.0f }, { 60.5f, 1149.0f }, { 907.1f, 1149.0f },
    };
    int i = 0;
    for (auto& s : screws)
        drawScrew (g, { s[0], s[1] }, 0.37f * (float) (i++ * 7 % 11));

    drawDcJack (g, { 120.9f, 1149.0f }, theme);

    // Slider slots (PTA slide pots): dark slots cut in the board
    auto slot = [&] (float x, float y, float h)
    {
        auto r = juce::Rectangle<float> (x - 5.3f, y - h * 0.5f, 10.6f, h);
        g.setColour (juce::Colour (0xff0b0b0b));
        g.fillRoundedRectangle (r, 5.3f);
        g.setColour (theme.dark ? juce::Colours::white.withAlpha (0.07f) : juce::Colours::black.withAlpha (0.5f));
        g.drawRoundedRectangle (r.expanded (0.5f), 5.8f, 1.0f);
    };
    slot (544.3f, 408.2f, 120.9f);
    slot (604.7f, 408.2f, 120.9f);
    slot (665.2f, 408.2f, 120.9f);
    slot (181.4f, 775.2f, 197.3f);
    slot (423.3f, 775.2f, 197.3f);
}

//==============================================================================
void drawScrew (juce::Graphics& g, juce::Point<float> c, float angle)
{
    const float r = 7.6f;
    drawShadow (g, c, r, 1.6f, 0.35f);
    juce::ColourGradient head (juce::Colour (0xff4a4a4a), c.x - r * 0.6f, c.y - r * 0.8f, juce::Colour (0xff0e0e0e), c.x + r * 0.5f, c.y + r, false);
    g.setGradientFill (head);
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 0.8f);

    juce::Path hex;
    hex.addPolygon (c, 6, 3.1f, angle);
    g.setColour (juce::Colour (0xff050505));
    g.fillPath (hex);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.strokePath (hex, juce::PathStrokeType (0.6f));
}

void drawDcJack (juce::Graphics& g, juce::Point<float> c, const Theme& t)
{
    const float r = 19.0f;
    drawShadow (g, c, r, 2.0f, 0.3f);
    juce::Path nut;
    nut.addPolygon (c, 6, r, 0.2f);
    g.setGradientFill (nutGradient (t, c, r));
    g.fillPath (nut);
    g.setColour (t.blackJacks ? juce::Colours::white.withAlpha (0.08f) : juce::Colours::black.withAlpha (0.35f));
    g.strokePath (nut, juce::PathStrokeType (0.8f));
    g.setColour (juce::Colour (0xff1a1a1a));
    g.fillEllipse (centred ({ c.x, c.y, 0, 0 }, 22.0f));
    g.setColour (juce::Colour (0xffb8b8b4));
    g.fillEllipse (centred ({ c.x, c.y, 0, 0 }, 5.0f));
}

//==============================================================================
Knob::Knob (ControlKind k, int index) : kind (k), paramIndex (index)
{
    setSliderStyle (juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (positionToAngle (0.0f) + juce::MathConstants<float>::twoPi, positionToAngle (1.0f) + juce::MathConstants<float>::twoPi, true);
    setMouseDragSensitivity (kind == ControlKind::BigKnob ? 320 : 220);
    setVelocityBasedMode (false);
    setPopupDisplayEnabled (true, true, nullptr, 1200);
    setScrollWheelEnabled (true);
}

juce::String Knob::getTextFromValue (double value)
{
    return juce::String (kParams[(size_t) paramIndex].name) + ": " + describeValue (paramIndex, (float) value);
}

void Knob::paint (juce::Graphics& g)
{
    const auto& t = themeOf (*this);
    const auto bounds = getLocalBounds().toFloat();
    const auto c = bounds.getCentre();
    const float pos = (float) valueToProportionOfLength (getValue());
    const float angle = positionToAngle (pos);

    if (t.design)
    {
        if (kind == ControlKind::BigKnob)
        {
            // Cutoff: Ø58 pt cap with its pointer tab, rotating as one piece.
            g.setColour (design::kCap);
            g.fillPath (cutoffPath(), designTransform (c, angle));
            return;
        }
        // Trimmer: black Ø40 pt seat, scalloped Ø32 pt cap, 4 x 9 pt pointer bar.
        const bool orange = paramIndex == P_MIX_MAIN || paramIndex == P_VOLUME;
        fillDesignCircle (g, c, 20.0f, design::kRing);
        g.setColour (orange ? design::kOrange : design::kCap);
        g.fillPath (trimmerCapPath(), designTransform (c, angle));
        juce::Path bar;
        bar.addRoundedRectangle (-2.0f, -12.66f, 4.0f, 9.0f, 2.0f);
        g.setColour (orange ? design::kCap : design::kPointer);
        g.fillPath (bar, designTransform (c, angle));
        return;
    }

    // Output level knobs (MIXER OUT, VOLUME) are orange on the built Moduleur.
    const bool accent = paramIndex == P_MIX_MAIN || paramIndex == P_VOLUME;
    const auto cap = accent ? t.accentKnob : t.knob;
    const auto shade = accent ? t.accentKnob.darker (0.45f) : t.knobShade;
    const float shadowAlpha = t.dark ? 0.75f : 0.45f;

    if (kind == ControlKind::BigKnob)
    {
        // Large custom knob: gripped skirt and a flat top with a pointer line.
        const float r = kBigSize * 0.5f - 2.0f;
        drawShadow (g, c, r, 4.0f, shadowAlpha);

        juce::Path grip;
        const int ridges = 18;
        for (int i = 0; i < ridges * 2; ++i)
        {
            const float a = angle + juce::MathConstants<float>::twoPi * (float) i / (float) (ridges * 2);
            const float rr = (i % 2 == 0) ? r : r - 2.6f;
            const auto p = c.getPointOnCircumference (rr, a);
            if (i == 0) grip.startNewSubPath (p); else grip.lineTo (p);
        }
        grip.closeSubPath();
        g.setColour (shade);
        g.fillPath (grip);

        const float top = r * 0.82f;
        fillCap (g, c, top, cap.brighter (0.08f), cap.interpolatedWith (shade, 0.55f));
        g.setColour (juce::Colours::white.withAlpha (0.1f));
        g.drawEllipse (c.x - top + 0.5f, c.y - top + 0.5f, top * 2 - 1, top * 2 - 1, 1.0f);

        g.setColour (t.knobLine);
        g.drawLine ({ c.getPointOnCircumference (top * 0.25f, angle), c.getPointOnCircumference (top * 0.92f, angle) }, 3.4f);
        return;
    }

    // Tall trimmer topper on a PTV09: knurled cylinder seen from above.
    const float r = kTrimmerSize * 0.5f - 1.5f;
    drawShadow (g, c, r, 3.0f, shadowAlpha);

    g.setColour (shade);
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);

    const int ridges = 28;
    g.setColour (shade.darker (0.5f).withAlpha (0.55f));
    for (int i = 0; i < ridges; ++i)
    {
        const float a = angle + juce::MathConstants<float>::twoPi * (float) i / (float) ridges;
        g.drawLine ({ c.getPointOnCircumference (r - 2.8f, a), c.getPointOnCircumference (r - 0.4f, a) }, 0.9f);
    }

    const float top = r - 3.4f;
    fillCap (g, c, top, cap.brighter (0.08f), cap.interpolatedWith (shade, 0.6f));

    g.setColour (t.knobLine);
    g.drawLine ({ c.getPointOnCircumference (top * 0.15f, angle), c.getPointOnCircumference (top * 0.95f, angle) }, 2.4f);
}

//==============================================================================
Fader::Fader (int index, float t) : paramIndex (index), travel (t)
{
    setSliderStyle (juce::Slider::LinearVertical);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setPopupDisplayEnabled (true, true, nullptr, 1200);
    setSliderSnapsToMousePosition (false);
}

juce::String Fader::getTextFromValue (double value)
{
    return juce::String (kParams[(size_t) paramIndex].name) + ": " + describeValue (paramIndex, (float) value);
}

void Fader::paint (juce::Graphics& g)
{
    const auto& t = themeOf (*this);
    const auto b = getLocalBounds().toFloat();
    const float pos = (float) valueToProportionOfLength (getValue());

    if (t.design)
    {
        // The slot is part of the panel art; the cap is a white Ø20 pt disc
        // travelling between the slot's rounded ends.
        const float yTop = b.getCentreY() - travel * 0.5f;
        fillDesignCircle (g, { b.getCentreX(), yTop + (1.0f - pos) * travel }, 10.0f, design::kCap);
        return;
    }
    const float capH = 40.0f, capW = 26.0f;
    const float yTop = b.getCentreY() - travel * 0.5f;
    const float cy = yTop + (1.0f - pos) * travel;
    auto cap = juce::Rectangle<float> (b.getCentreX() - capW * 0.5f, cy - capH * 0.5f, capW, capH);

    g.setColour (juce::Colours::black.withAlpha (t.dark ? 0.6f : 0.35f));
    g.fillRoundedRectangle (cap.translated (1.5f, 3.5f).expanded (1.5f), 4.0f);

    juce::ColourGradient body (t.sliderCap.brighter (0.08f), cap.getX(), cap.getY(),
                               t.sliderCap.interpolatedWith (t.knobShade, 0.6f), cap.getRight(), cap.getBottom(), false);
    g.setGradientFill (body);
    g.fillRoundedRectangle (cap, 3.5f);
    // grip ridges
    g.setColour (t.sliderLine.withAlpha (0.12f));
    for (int i = 1; i < 6; ++i)
    {
        const float y = cap.getY() + cap.getHeight() * (float) i / 6.0f;
        if (i != 3)
            g.drawLine (cap.getX() + 3.0f, y, cap.getRight() - 3.0f, y, 1.0f);
    }
    g.setColour (t.sliderLine);
    g.fillRect (cap.getX() + 2.5f, cap.getCentreY() - 1.2f, cap.getWidth() - 5.0f, 2.4f);
}

//==============================================================================
WaveSelector::WaveSelector (juce::RangedAudioParameter& p)
    : param (p), attachment (p, [this] (float) { repaint(); })
{
    attachment.sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

WaveSelector::~WaveSelector() = default;

int WaveSelector::getIndex() const
{
    return juce::jlimit (0, 2, juce::roundToInt (param.convertFrom0to1 (param.getValue())));
}

void WaveSelector::setIndex (int i)
{
    i = juce::jlimit (0, 2, i);
    if (i != getIndex())
        attachment.setValueAsCompleteGesture ((float) i);
}

void WaveSelector::mouseDown (const juce::MouseEvent& e)
{
    dragStartIndex = getIndex();
    if (e.mods.isPopupMenu())
        return;
    // Click on a legend (right of the knob) selects it directly; on the knob it steps.
    const auto c = knobCentre();
    if (e.position.x > c.x + Knob::kBigSize * 0.5f)
    {
        int best = 0;
        float bestDist = 1.0e9f;
        for (int i = 0; i < 3; ++i)
        {
            const float a = std::atan2 (e.position.y - c.y, e.position.x - c.x);
            const float d = std::abs (a - kAngles[i]);
            if (d < bestDist) { bestDist = d; best = i; }
        }
        setIndex (best);
    }
    else
    {
        setIndex ((getIndex() + 1) % 3);
        dragStartIndex = getIndex();
    }
}

void WaveSelector::mouseDrag (const juce::MouseEvent& e)
{
    const int steps = juce::roundToInt ((float) e.getDistanceFromDragStartY() / 30.0f);
    if (std::abs (e.getDistanceFromDragStartY()) > 6)
        setIndex (dragStartIndex + steps);
}

void WaveSelector::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    wheelAccum += w.deltaY;
    if (std::abs (wheelAccum) > 0.2f)
    {
        setIndex (getIndex() + (wheelAccum < 0 ? 1 : -1));
        wheelAccum = 0.0f;
    }
}

void WaveSelector::paint (juce::Graphics& g)
{
    // The component is wider than the knob so the legends are clickable; the knob
    // sits at the left. It is the Moduleur's custom selector knob: a round cap with
    // a dished centre and a pointer tab.
    const auto& t = themeOf (*this);
    const float size = Knob::kBigSize;
    const auto c = knobCentre();

    if (t.design)
    {
        // White Ø88 pt disc, 18 x 8 pt pointer tab, Ø24 pt hub hole.
        const float a = kAngles[getIndex()];
        juce::Path body;
        body.addEllipse (-44.0f, -44.0f, 88.0f, 88.0f);
        body.addRectangle (38.0f, -4.0f, 18.0f, 8.0f);
        g.setColour (design::kCap);
        g.fillPath (body, designTransform (c, a));
        fillDesignCircle (g, c, 12.0f, design::kPanel);
        return;
    }

    const float angle = kAngles[getIndex()] + juce::MathConstants<float>::halfPi; // juce: 0 = up
    const float r = size * 0.5f - 6.0f;

    juce::Path body;
    body.addEllipse (c.x - r, c.y - r, r * 2, r * 2);
    juce::Path tab;
    tab.startNewSubPath (c.getPointOnCircumference (r * 0.9f, angle - 0.42f));
    tab.lineTo (c.getPointOnCircumference (r + 15.0f, angle));
    tab.lineTo (c.getPointOnCircumference (r * 0.9f, angle + 0.42f));
    tab.closeSubPath();
    body.addPath (tab.createPathWithRoundedCorners (5.0f));

    juce::DropShadow (juce::Colours::black.withAlpha (t.dark ? 0.8f : 0.5f), 14, { 2, 5 }).drawForPath (g, body);

    juce::ColourGradient face (t.knob.brighter (0.08f), c.x - r * 0.7f, c.y - r * 0.9f,
                               t.knob.interpolatedWith (t.knobShade, 0.6f), c.x + r * 0.6f, c.y + r, false);
    g.setGradientFill (face);
    g.fillPath (body);
    g.setColour (t.knobShade.darker (0.3f).withAlpha (0.6f));
    g.strokePath (body, juce::PathStrokeType (1.0f));

    // Dished centre (inner shadow at the top left, highlight at the bottom right)
    const float dish = r * 0.52f;
    juce::ColourGradient bowl (t.knobShade.darker (0.15f), c.x - dish, c.y - dish, t.knob.brighter (0.05f), c.x + dish, c.y + dish, false);
    g.setGradientFill (bowl);
    g.fillEllipse (c.x - dish, c.y - dish, dish * 2, dish * 2);
    g.setColour (t.knobShade.darker (0.35f).withAlpha (0.5f));
    g.drawEllipse (c.x - dish, c.y - dish, dish * 2, dish * 2, 1.0f);

    // Pointer line along the tab
    g.setColour (t.knobLine.withAlpha (0.85f));
    g.drawLine ({ c.getPointOnCircumference (r * 0.7f, angle), c.getPointOnCircumference (r + 9.0f, angle) }, 2.6f);
}

//==============================================================================
PushButton::PushButton (std::atomic<bool>& s, bool withLed) : state (s), hasLed (withLed) {}

void PushButton::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        return;
    state = true;
    repaint();
}

void PushButton::mouseUp (const juce::MouseEvent&)
{
    state = false;
    repaint();
}

void PushButton::paint (juce::Graphics& g)
{
    const auto& t = themeOf (*this);
    const auto c = getLocalBounds().toFloat().getCentre();
    const bool down = state.load();

    if (t.design)
    {
        // Black Ø44 pt bezel, grey Ø36 pt cap.
        fillDesignCircle (g, c, 22.0f, design::kRing);
        fillDesignCircle (g, c, 18.0f, down ? design::kButtonDown : design::kButton);
        return;
    }
    const float r = 18.5f;

    // Bezel
    drawShadow (g, c, r, down ? 1.5f : 3.0f, t.dark ? 0.7f : 0.4f);
    juce::ColourGradient bezel (juce::Colour (0xff2c2c2c), c.x - r, c.y - r, juce::Colour (0xff080808), c.x + r, c.y + r, false);
    g.setGradientFill (bezel);
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);

    if (hasLed)
    {
        const float ring = r - 2.0f;
        g.setColour (juce::Colour (0xff3a0a08).interpolatedWith (juce::Colour (0xffff3b2a), led));
        g.drawEllipse (c.x - ring, c.y - ring, ring * 2, ring * 2, 2.4f);
        if (led > 0.02f)
        {
            juce::ColourGradient glow (juce::Colour (0xffff3b2a).withAlpha (0.45f * led), c.x, c.y, juce::Colours::transparentBlack, c.x + r * 1.7f, c.y, true);
            g.setGradientFill (glow);
            g.fillEllipse (c.x - r * 1.7f, c.y - r * 1.7f, r * 3.4f, r * 3.4f);
        }
    }

    const float cap = r - 5.0f;
    const auto capColour = down ? t.buttonCap.darker (0.25f) : t.buttonCap;
    fillCap (g, c, cap, down ? capColour.darker (0.1f) : capColour.brighter (0.08f), capColour.interpolatedWith (t.knobShade, down ? 0.2f : 0.6f));
    g.setColour (juce::Colours::white.withAlpha (down ? 0.03f : 0.1f));
    g.drawEllipse (c.x - cap + 0.5f, c.y - cap + 0.5f, cap * 2 - 1, cap * 2 - 1, 1.0f);
}

//==============================================================================
PowerButton::PowerButton (juce::RangedAudioParameter& p)
    : param (p), attachment (p, [this] (float v) { on = v > 0.5f; repaint(); })
{
    attachment.sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void PowerButton::mouseDown (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu())
        attachment.setValueAsCompleteGesture (on ? 0.0f : 1.0f);
}

void PowerButton::paint (juce::Graphics& g)
{
    const auto& t = themeOf (*this);
    const auto c = getLocalBounds().toFloat().getCentre();

    if (t.design)
    {
        // Black Ø40 pt bezel, grey Ø30 pt cap (darker when off).
        fillDesignCircle (g, c, 20.0f, design::kRing);
        fillDesignCircle (g, c, 15.0f, on ? design::kPowerOn : design::kPowerOff);
        return;
    }
    const float r = 20.5f;
    drawShadow (g, c, r, 3.0f, 0.4f);
    juce::Path nut;
    nut.addPolygon (c, 6, r, 0.0f);
    g.setGradientFill (nutGradient (t, c, r));
    g.fillPath (nut);
    g.setColour (t.blackJacks ? juce::Colours::white.withAlpha (0.08f) : juce::Colours::black.withAlpha (0.35f));
    g.strokePath (nut, juce::PathStrokeType (0.8f));

    const float cap = on ? 12.5f : 13.5f;
    fillCap (g, c, cap, juce::Colour (0xff303030), juce::Colour (0xff0c0c0c));
}

//==============================================================================
void LedLight::setLevel (float v)
{
    v = juce::jlimit (0.0f, 1.0f, v);
    if (std::abs (v - level) > 0.02f)
    {
        level = v;
        repaint();
    }
}

void LedLight::paint (juce::Graphics& g)
{
    const auto c = getLocalBounds().toFloat().getCentre();

    if (themeOf (*this).design)
    {
        // Black Ø16 pt seat, Ø12 pt lens from dark red to red.
        fillDesignCircle (g, c, 8.0f, design::kRing);
        fillDesignCircle (g, c, 6.0f, design::kLedOff.interpolatedWith (design::kLedOn, level));
        return;
    }

    const float r = 6.0f;
    if (level > 0.01f)
    {
        juce::ColourGradient glow (juce::Colour (0xffff3020).withAlpha (0.55f * level), c.x, c.y, juce::Colours::transparentBlack, c.x + r * 3.2f, c.y, true);
        g.setGradientFill (glow);
        g.fillEllipse (c.x - r * 3.2f, c.y - r * 3.2f, r * 6.4f, r * 6.4f);
    }
    const auto off = juce::Colour (0xff5a1410), on = juce::Colour (0xffff4a36);
    juce::ColourGradient dome (off.interpolatedWith (juce::Colour (0xffffd0c0), level).brighter (0.2f), c.x - r * 0.4f, c.y - r * 0.5f,
                               off.interpolatedWith (on, level), c.x + r * 0.6f, c.y + r * 0.7f, true);
    g.setGradientFill (dome);
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 0.7f);
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.fillEllipse (c.x - r * 0.55f, c.y - r * 0.6f, r * 0.6f, r * 0.45f);
}

//==============================================================================
Jack::Jack (int p, Listener* l, bool canPatch) : port (p), listener (l), patchable (canPatch)
{
    if (port >= 0)
        setTooltip (juce::String (kPorts[(size_t) port].label) + (kPorts[(size_t) port].isOutput ? "  (output)" : "  (input)"));
    setMouseCursor (patchable ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void Jack::mouseDown (const juce::MouseEvent& e) { if (patchable && listener) listener->jackPressed (*this, e); }
void Jack::mouseDrag (const juce::MouseEvent& e) { if (patchable && listener) listener->jackDragged (*this, e); }
void Jack::mouseUp (const juce::MouseEvent& e) { if (patchable && listener) listener->jackReleased (*this, e); }

void Jack::paint (juce::Graphics& g)
{
    const auto& t = themeOf (*this);
    const auto c = getLocalBounds().toFloat().getCentre();
    const float r = 13.5f;

    if (t.design)
    {
        // The jack itself is part of the panel art; only patching feedback is drawn.
        if (highlighted || (hover && patchable))
        {
            const float rr = 22.5f * kPtToPanel;
            g.setColour ((highlighted ? juce::Colours::white : juce::Colours::white.withAlpha (0.45f)));
            g.drawEllipse (c.x - rr, c.y - rr, rr * 2.0f, rr * 2.0f, 1.6f);
        }
        return;
    }
    drawShadow (g, c, r, 2.0f, t.dark ? 0.6f : 0.35f);

    // Knurled nut
    g.setGradientFill (nutGradient (t, c, r));
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    g.setColour (t.blackJacks ? juce::Colours::white.withAlpha (0.07f) : juce::Colours::black.withAlpha (0.16f));
    for (int i = 0; i < 36; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / 36.0f;
        g.drawLine ({ c.getPointOnCircumference (r - 2.2f, a), c.getPointOnCircumference (r - 0.3f, a) }, 0.7f);
    }
    g.setColour (t.blackJacks ? juce::Colours::white.withAlpha (0.1f) : juce::Colours::black.withAlpha (0.35f));
    g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 0.7f);

    const float inner = 8.2f;
    if (t.blackJacks)
    {
        juce::ColourGradient bush (juce::Colour (0xff2e2e30), c.x - inner, c.y - inner, juce::Colour (0xff101011), c.x + inner, c.y + inner, false);
        g.setGradientFill (bush);
    }
    else
    {
        juce::ColourGradient bush (juce::Colour (0xffc8c8c4), c.x - inner, c.y - inner, juce::Colour (0xff7a7a76), c.x + inner, c.y + inner, false);
        g.setGradientFill (bush);
    }
    g.fillEllipse (c.x - inner, c.y - inner, inner * 2, inner * 2);

    g.setColour (juce::Colour (0xff030303));
    g.fillEllipse (c.x - 5.0f, c.y - 5.0f, 10.0f, 10.0f);

    if (highlighted || (hover && patchable))
    {
        g.setColour ((highlighted ? juce::Colour (0xff2e9bff) : juce::Colours::white).withAlpha (0.85f));
        g.drawEllipse (c.x - r - 2.5f, c.y - r - 2.5f, (r + 2.5f) * 2, (r + 2.5f) * 2, 2.0f);
    }
}

//==============================================================================
juce::String describeValue (int index, float v)
{
    auto pct = [] (float x) { return juce::String (juce::roundToInt (x * 100.0f)) + "%"; };
    auto db = [] (float gain) { return gain <= 1.0e-4f ? juce::String ("-inf dB") : juce::String (20.0f * std::log10 (gain), 1) + " dB"; };
    auto hz = [] (float f) { return f >= 1000.0f ? juce::String (f / 1000.0f, 2) + " kHz" : juce::String (f, f < 10.0f ? 2 : 1) + " Hz"; };
    auto ms = [] (float s) { return s >= 1.0f ? juce::String (s, 2) + " s" : juce::String (juce::roundToInt (s * 1000.0f)) + " ms"; };

    switch (index)
    {
        case P_VCO1_OCTAVE: case P_VCO2_OCTAVE:
            return "+" + juce::String (v * 12.0f * 100.0f / 230.0f, 2) + " oct";
        case P_VCO1_FINE: case P_VCO2_FINE:
        {
            const int cents = juce::roundToInt ((v - 0.5f) * 2.0f * 107.0f);
            return (cents > 0 ? "+" : "") + juce::String (cents) + " ct";
        }
        case P_VCO1_WIDTH: case P_VCO2_WIDTH:
        {
            const float thr = juce::jlimit (-4.5f, 4.5f, -((v - 0.5f) * 24.0f) * (100.0f / 220.0f));
            return "pulse " + juce::String (juce::roundToInt ((1.0f - 0.5f * (thr / 5.0f + 1.0f)) * 100.0f)) + "%";
        }
        case P_MIX_IN1: case P_MIX_IN2: case P_MIX_IN3: case P_MIX_IN4: case P_MIX_MAIN:
            return db (audioTaper (v));
        case P_MIX_GAIN: return "x" + juce::String (1.0f + 10.0f * audioTaper (v), 1);
        case P_MIX_THRESHOLD: return juce::String (6.0f * audioTaper (v), 2) + " V";
        case P_VCF_CUTOFF: return hz (juce::jlimit (8.0f, 22000.0f, 16.0f * std::exp2 (v * 12.0f * 0.85f)));
        case P_VCF_CV1: case P_VCF_CV2:
        {
            const float p = v * 2.0f - 1.0f;
            const float a = std::copysign (std::pow (std::abs (p), 1.6f), p);
            return (a > 0.005f ? "+" : "") + juce::String (juce::roundToInt (a * 100.0f)) + "%";
        }
        case P_ENV1_ATTACK: case P_ENV2_ATTACK: return ms ((0.0002f + 10.0f * audioTaper (v)) * 1.35f);
        case P_ENV1_DECAY: case P_ENV2_DECAY: case P_ENV1_RELEASE: case P_ENV2_RELEASE:
            return ms ((0.0005f + 10.0f * audioTaper (v)) * 3.0f);
        case P_CRUSH: return hz (expMap (1.0f - v, 160.0f, 19000.0f));
        case P_LFO_RATE: return hz (expMap (v, 0.1f, 200.0f));
        case P_VOLUME: return db (audioTaper (v));
        case P_DRIVE: return "x" + juce::String (1.0f + 4.5f * v, 2);
        default: return pct (v);
    }
}
} // namespace moduleur::ui
