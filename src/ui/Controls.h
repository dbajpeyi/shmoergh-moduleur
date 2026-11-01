#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "Params.h"

// Front-panel components. Everything is laid out in panel units (968 x 1210)
// inside a content component that the editor scales with an AffineTransform.
namespace moduleur::ui
{
// The Figma design is drawn in points on a 1024 x 1280 pt panel.
inline constexpr float kPtToPanel = 968.0f / 1024.0f;
// The design's full composition (backdrop + handle + panel), in panel units.
inline constexpr float kStageWidth = 1144.0f * kPtToPanel;
inline constexpr float kStageHeight = 1527.0f * kPtToPanel;
inline constexpr float kPanelOriginX = 60.0f * kPtToPanel;
inline constexpr float kPanelOriginY = 187.0f * kPtToPanel;

// Old panel looks (DIY FR4 boards, shaded hardware) are kept behind this
// build-time feature flag; the Figma design is the only look by default.
#ifndef MODULEUR_PANEL_THEMES
 #define MODULEUR_PANEL_THEMES 0
#endif
inline constexpr bool kPanelThemesEnabled = MODULEUR_PANEL_THEMES != 0;

struct Theme
{
    juce::Colour panel, print, copper, background;
    bool dark = false;
    bool design = false;                      // flat parts of the Figma design

    // Hardware fitted to the panel
    juce::Colour knob, knobShade, knobLine;   // knob caps and their pointer line
    juce::Colour accentKnob;                  // output level knobs (MIXER OUT, VOLUME)
    juce::Colour sliderCap, sliderLine;
    juce::Colour buttonCap;
    bool blackJacks = false;                  // black or nickel jack nuts

    // The built Moduleur: black powder-coated aluminium, grey print and hatching,
    // white knobs with orange output levels, black jacks.
    static Theme original()
    {
        Theme t;
        t.panel = juce::Colour (0xff1c1c1e);
        t.print = juce::Colour (0xff9a9a9b);
        t.copper = juce::Colour (0xff4b4b4e);
        t.background = juce::Colour (0xff0d0d0d);
        t.dark = true;
        t.knob = juce::Colour (0xfff2f1ed);
        t.knobShade = juce::Colour (0xffb9b8b3);
        t.knobLine = juce::Colour (0xff2b2b2b);
        t.accentKnob = juce::Colour (0xffee6a28);
        t.sliderCap = juce::Colour (0xfff2f1ed);
        t.sliderLine = juce::Colour (0xff2b2b2b);
        t.buttonCap = juce::Colour (0xffe4e3df);
        t.blackJacks = true;
        return t;
    }

    // DIY FR4 enclosure panels (exposed ENIG copper), with black knobs.
    static Theme fr4 (bool blackBoard)
    {
        Theme t;
        t.panel = blackBoard ? juce::Colour (0xff171717) : juce::Colour (0xfff2f0e9);
        t.print = blackBoard ? juce::Colour (0xfff1efe8) : juce::Colour (0xff151515);
        t.copper = blackBoard ? juce::Colour (0xffc9a24a) : juce::Colour (0xffc39a45);
        t.background = blackBoard ? juce::Colour (0xff0d0d0d) : juce::Colour (0xff1c1c1c);
        t.dark = blackBoard;
        t.knob = juce::Colour (0xff2c2c2c);
        t.knobShade = juce::Colour (0xff101010);
        t.knobLine = juce::Colour (0xfff3f1ea);
        t.accentKnob = t.knob;
        t.sliderCap = juce::Colour (0xff2c2c2c);
        t.sliderLine = juce::Colour (0xfff3f1ea);
        t.buttonCap = juce::Colour (0xff2a2a2a);
        t.blackJacks = false;
        return t;
    }

    static Theme white() { return fr4 (false); }
    static Theme black() { return fr4 (true); }

    // The Figma design ("Shmoergh - Moduleur"): flat white/orange knobs on a
    // #252527 panel with #9A9A9C print.
    static Theme figma()
    {
        Theme t = original();
        t.panel = juce::Colour (0xff252527);
        t.print = juce::Colour (0xff9a9a9c);
        t.copper = juce::Colour (0xff4b4b50);
        t.background = juce::Colour (0xff090909);
        t.design = true;
        return t;
    }
};

// Colours of the design's flat parts.
namespace design
{
inline const juce::Colour kPanel { 0xff252527 }, kRing { 0xff000000 }, kCap { 0xffffffff }, kPointer { 0xff111111 },
                          kOrange { 0xffff8700 }, kLedOn { 0xffff0000 }, kLedOff { 0xff361819 },
                          kButton { 0xffbfbfbf }, kButtonDown { 0xff8f8f8f }, kPowerOn { 0xffb7b7b7 }, kPowerOff { 0xff6b6b6b };
} // namespace design

// The editor installs this LookAndFeel; controls read their colours from it.
struct ThemeLookAndFeel : public juce::LookAndFeel_V4
{
    Theme theme = Theme::figma();
};

const Theme& themeOf (const juce::Component&);

//==============================================================================
// The design's static art: backdrop, carry handle, panel print, hatching,
// jacks and screws, rendered from the Figma export (assets/design).
class StageBackground : public juce::Component
{
public:
    StageBackground();
    void paint (juce::Graphics&) override;

private:
    juce::Image image;
};

//==============================================================================
// Panel artwork: front panel with copper/hatching layer and print layer.
class PanelBackground : public juce::Component
{
public:
    PanelBackground();
    void setTheme (const Theme& t);
    void paint (juce::Graphics&) override;

private:
    std::unique_ptr<juce::Drawable> copper, silk;
    Theme theme = Theme::figma();
};

//==============================================================================
// Rotary pot with a Thonk "tall trimmer topper", or the large 3D-printed knob.
class Knob : public juce::Slider
{
public:
    Knob (ControlKind kind, int paramIndex);
    void paint (juce::Graphics&) override;
    juce::String getTextFromValue (double value) override;
    int getParamIndex() const { return paramIndex; }

    static constexpr float kTrimmerSize = 40.0f;
    static constexpr float kBigSize = 88.0f;

private:
    ControlKind kind;
    int paramIndex;
};

// PTA slide potentiometer in its panel slot.
class Fader : public juce::Slider
{
public:
    Fader (int paramIndex, float travel);
    void paint (juce::Graphics&) override;
    juce::String getTextFromValue (double value) override;

    float getTravel() const { return travel; }

private:
    int paramIndex;
    float travel;
};

// 3-position rotary switch (OSC wave: TRI / SAW / SQR).
class WaveSelector : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit WaveSelector (juce::RangedAudioParameter& p);
    ~WaveSelector() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    // Angles (radians, 0 = pointing right, positive = clockwise) towards the
    // TRI / SAW / SQR legends on the panel (from the design's label boxes).
    static constexpr float kAngles[3] = { 0.0f, 0.367f, 0.660f };
    static constexpr float kLeftPad = 16.0f;   // room for the knob's shadow
    juce::Point<float> knobCentre() const { return { kLeftPad + Knob::kBigSize * 0.5f, (float) getHeight() * 0.5f }; }

private:
    void setIndex (int i);
    int getIndex() const;
    juce::RangedAudioParameter& param;
    juce::ParameterAttachment attachment;
    int dragStartIndex = 0;
    float wheelAccum = 0.0f;
};

// C&K D6R style round push button (Brain A/B), momentary; A carries an LED.
class PushButton : public juce::Component
{
public:
    PushButton (std::atomic<bool>& state, bool withLed);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void setLed (float v) { if (std::abs (v - led) > 0.01f) { led = v; repaint(); } }

private:
    std::atomic<bool>& state;
    bool hasLed;
    float led = 0.0f;
};

// Latching power button bound to the "power" parameter.
class PowerButton : public juce::Component
{
public:
    explicit PowerButton (juce::RangedAudioParameter& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::RangedAudioParameter& param;
    juce::ParameterAttachment attachment;
    bool on = true;
};

// 3 mm LED.
class LedLight : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    void setLevel (float v);

private:
    float level = 0.0f;
};

// Thonkiconn jack. Mouse handling is delegated to the cable controller.
class Jack : public juce::Component, public juce::SettableTooltipClient
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void jackPressed (Jack&, const juce::MouseEvent&) = 0;
        virtual void jackDragged (Jack&, const juce::MouseEvent&) = 0;
        virtual void jackReleased (Jack&, const juce::MouseEvent&) = 0;
    };

    Jack (int port, Listener* listener, bool patchable = true);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }

    int getPort() const { return port; }
    void setHighlighted (bool h) { if (h != highlighted) { highlighted = h; repaint(); } }

    static constexpr float kSize = 40.0f;

private:
    int port;
    Listener* listener;
    bool patchable, hover = false, highlighted = false;
};

// Decorative hardware (screws, DC barrel jack) drawn over the artwork.
void drawScrew (juce::Graphics&, juce::Point<float> centre, float angle);
void drawDcJack (juce::Graphics&, juce::Point<float> centre, const Theme&);
juce::String describeValue (int paramIndex, float value);
} // namespace moduleur::ui
