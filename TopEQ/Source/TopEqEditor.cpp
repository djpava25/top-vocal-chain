#include "TopEqEditor.h"

using namespace teqi;
using juce::Colour; namespace Colours = juce::Colours; using juce::Graphics; using juce::Path; using juce::Rectangle; using juce::ColourGradient;

namespace
{
    juce::String U (const char* s) { return juce::String::fromUTF8 (s); }
    juce::Font font (float h, bool bold = false, bool italic = false) { return juce::Font (juce::FontOptions (h, (bold ? juce::Font::bold : 0) | (italic ? juce::Font::italic : 0))); }
    constexpr float pi = juce::MathConstants<float>::pi;
    constexpr float A0 = pi * 1.25f, A1 = pi * 2.75f;            // perillas grandes 0..10
    const Colour face (0xff8fa1b0), ink (0xff161b21), cream (0xfff1ead8);
    float param (TopEqProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); }
    void setParam (TopEqProcessor& p, const char* id, float v)
    { if (auto* q = p.apvts.getParameter (id)) { q->beginChangeGesture(); q->setValueNotifyingHost (q->convertTo0to1 (v)); q->endChangeGesture(); } }

    // angulos de los selectores
    struct Sel { const char* id; int n; float a0, a1; };
    const Sel selLow { "lowFreq", 4, pi * 1.6f, pi * 2.4f }, selHigh { "highFreq", 7, pi * 1.35f, pi * 2.65f }, selAtt { "attFreq", 3, pi * 1.65f, pi * 2.35f };

    void engraved (Graphics& g, const juce::String& t, Rectangle<float> r, float size, bool bold = true, juce::Justification j = juce::Justification::centred)
    {
        g.setFont (font (size, bold));
        g.setColour (Colours::white.withAlpha (0.35f)); g.drawText (t, r.translated (0, 1.0f), j);
        g.setColour (ink); g.drawText (t, r, j);
    }
    void screw (Graphics& g, float x, float y, float r = 5.0f)
    {
        g.setGradientFill (ColourGradient (Colour (0xffd9dbde), x - r, y - r, Colour (0xff55585d), x + r, y + r, false));
        g.fillEllipse (x - r, y - r, r * 2, r * 2);
        g.setColour (Colours::black.withAlpha (0.6f)); g.drawEllipse (x - r, y - r, r * 2, r * 2, 0.8f);
        g.drawLine (x - r * 0.7f, y - r * 0.3f, x + r * 0.7f, y + r * 0.3f, 1.3f);
    }
}

Look::Look()
{
    setColour (juce::PopupMenu::backgroundColourId, Colour (0xff1e2226));
    setColour (juce::PopupMenu::textColourId, cream);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colour (0xff5b6e80));
    setColour (juce::TooltipWindow::backgroundColourId, Colour (0xff1e2226));
    setColour (juce::TooltipWindow::textColourId, cream);
}
void Look::drawBubble (Graphics& g, juce::BubbleComponent&, const juce::Point<float>&, const Rectangle<float>& body)
{
    g.setColour (Colour (0xf01e2226)); g.fillRoundedRectangle (body, 4);
    g.setColour (cream.withAlpha (0.6f)); g.drawRoundedRectangle (body.reduced (0.5f), 4, 1.0f);
}
void Look::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const juce::Point<float> c ((float) x + (float) w * 0.5f, (float) y + (float) h * 0.5f);
    const float R = (float) std::min (w, h) * 0.5f - 3.0f;
    const float a = a0 + pos * (a1 - a0);
    const bool sel = (bool) s.getProperties()["selector"];
    // sombra en el panel
    g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.55f), c.x + R * 0.15f, c.y + R * 0.3f, Colours::transparentBlack, c.x + R * 1.3f, c.y + R * 0.3f, true));
    g.fillEllipse (c.x - R * 1.15f, c.y - R * 0.95f, R * 2.6f, R * 2.6f);
    if (! sel)
    {
        // perilla grande de baquelita negra con falda estriada
        g.setGradientFill (ColourGradient (Colour (0xff3a3a3c), c.x - R * 0.5f, c.y - R, Colour (0xff050505), c.x + R * 0.5f, c.y + R, false));
        g.fillEllipse (c.x - R, c.y - R, R * 2, R * 2);
        for (int i = 0; i < 36; ++i)
        {
            const float t = (float) i / 36.0f * juce::MathConstants<float>::twoPi;
            g.setColour ((i % 2 ? Colours::white : Colours::black).withAlpha (i % 2 ? 0.07f : 0.3f));
            g.drawLine (c.x + R * 0.84f * std::sin (t), c.y - R * 0.84f * std::cos (t), c.x + R * std::sin (t), c.y - R * std::cos (t), 1.6f);
        }
        const float rc = R * 0.74f;
        g.setGradientFill (ColourGradient (Colour (0xff4a4a4e), c.x - rc * 0.4f, c.y - rc * 0.7f, Colour (0xff0a0a0b), c.x + rc * 0.5f, c.y + rc * 0.8f, true));
        g.fillEllipse (c.x - rc, c.y - rc, rc * 2, rc * 2);
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.35f), c.x - rc * 0.35f, c.y - rc * 0.6f, Colours::transparentWhite, c.x - rc * 0.1f, c.y - rc * 0.1f, true));
        g.fillEllipse (c.x - rc * 0.75f, c.y - rc * 0.85f, rc * 0.9f, rc * 0.7f);
        g.setColour (Colours::black); g.drawEllipse (c.x - rc, c.y - rc, rc * 2, rc * 2, 1.0f);
        // linea indicadora blanca
        g.setColour (cream);
        g.drawLine (c.x + rc * 0.25f * std::sin (a), c.y - rc * 0.25f * std::cos (a), c.x + R * 0.97f * std::sin (a), c.y - R * 0.97f * std::cos (a), 3.2f);
    }
    else
    {
        // selector tipo "cabeza de pollo"
        Path p;
        p.addRoundedRectangle (c.x - R * 0.24f, c.y - R * 1.0f, R * 0.48f, R * 1.65f, R * 0.22f);
        p.addEllipse (c.x - R * 0.55f, c.y - R * 0.55f, R * 1.1f, R * 1.1f);
        p.applyTransform (juce::AffineTransform::rotation (a, c.x, c.y));
        g.setColour (Colours::black.withAlpha (0.5f)); g.fillPath (p, juce::AffineTransform::translation (2, 3));
        g.setGradientFill (ColourGradient (Colour (0xff404043), c.x - R, c.y - R, Colour (0xff060607), c.x + R, c.y + R, false));
        g.fillPath (p);
        g.setColour (Colours::white.withAlpha (0.18f)); g.strokePath (p, juce::PathStrokeType (1.0f));
        g.setColour (cream);
        g.drawLine (c.x + R * 0.2f * std::sin (a), c.y - R * 0.2f * std::cos (a), c.x + R * 0.9f * std::sin (a), c.y - R * 0.9f * std::cos (a), 2.6f);
        g.setGradientFill (ColourGradient (Colour (0xffd6d8dc), c.x - 4, c.y - 4, Colour (0xff5c5f64), c.x + 4, c.y + 4, false));
        g.fillEllipse (c.x - R * 0.16f, c.y - R * 0.16f, R * 0.32f, R * 0.32f);
    }
}

Knob::Knob (APVTS& s, const juce::String& id, bool sel, float a0, float a1) : selector (sel)
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (a0, a1, true);
    setMouseDragSensitivity (sel ? 110 : 240);
    getProperties().set ("selector", sel);
    setPopupDisplayEnabled (! sel, true, nullptr, 900);
    if (auto* p = s.getParameter (id)) setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    att = std::make_unique<APVTS::SliderAttachment> (s, id, *this);
}

void Curve::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setGradientFill (ColourGradient (Colour (0xff12161a), 0, r.getY(), Colour (0xff070809), 0, r.getBottom(), false)); g.fillRoundedRectangle (r, 4);
    auto X = [&r] (double f) { return r.getX() + (float) (std::log (f / 20.0) / std::log (1000.0)) * r.getWidth(); };
    auto Y = [&r] (float db) { return r.getCentreY() - juce::jlimit (-18.0f, 18.0f, db) / 18.0f * r.getHeight() * 0.44f; };
    g.setColour (Colour (0xffe8b25a).withAlpha (0.1f));
    for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 }) g.drawVerticalLine ((int) X (f), r.getY(), r.getBottom());
    g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());
    const auto s = proc.settings();
    Path p;
    for (int i = 0; i <= 100; ++i)
    {
        const double f = 20.0 * std::pow (1000.0, i / 100.0);
        const float y = Y (teq::responseDb (s, f));
        if (i == 0) p.startNewSubPath (r.getX(), y); else p.lineTo (r.getX() + r.getWidth() * (float) i / 100.0f, y);
    }
    const bool on = param (proc, "power") > 0.5f;
    const Colour col = on ? Colour (0xffffb84a) : Colour (0xff6a6a6a);
    g.setColour (col.withAlpha (0.3f)); g.strokePath (p, juce::PathStrokeType (4.0f));
    g.setColour (col); g.strokePath (p, juce::PathStrokeType (1.6f));
    g.setFont (font (9)); g.setColour (Colour (0xffe8b25a).withAlpha (0.6f));
    for (auto [f, t] : { std::pair<double, const char*> { 100.0, "100" }, { 1000.0, "1k" }, { 10000.0, "10k" } }) g.drawText (t, Rectangle<float> (X (f) + 2, r.getBottom() - 11, 26, 10), juce::Justification::left);
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.08f), 0, r.getY(), Colours::transparentWhite, 0, r.getY() + r.getHeight() * 0.4f, false)); g.fillRoundedRectangle (r, 4);
    g.setColour (Colours::black); g.drawRoundedRectangle (r, 4, 2.0f);
    g.setColour (Colours::white.withAlpha (0.2f)); g.drawRoundedRectangle (r.expanded (1.5f), 5, 1.0f);
}

void Switch::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const bool on = param (proc, "power") > 0.5f;
    // lampara piloto (joya roja)
    const juce::Point<float> l (r.getX() + 26, r.getCentreY());
    g.setGradientFill (ColourGradient (Colour (0xffd9dbde), l.x - 16, l.y - 16, Colour (0xff4c4f54), l.x + 16, l.y + 16, false)); g.fillEllipse (l.x - 17, l.y - 17, 34, 34);
    if (on)
    {
        g.setGradientFill (ColourGradient (Colour (0x88ff3a1a), l.x, l.y, Colours::transparentBlack, l.x + 40, l.y, true)); g.fillEllipse (l.x - 40, l.y - 40, 80, 80);
    }
    g.setGradientFill (ColourGradient (on ? Colour (0xffffb08a) : Colour (0xff6a2a22), l.x - 4, l.y - 5, on ? Colour (0xffc0160a) : Colour (0xff2a0a08), l.x + 8, l.y + 10, true));
    g.fillEllipse (l.x - 12, l.y - 12, 24, 24);
    g.setColour (Colours::white.withAlpha (0.5f)); g.fillEllipse (l.x - 7, l.y - 8, 7, 5);
    for (int i = 0; i < 6; ++i) { g.setColour (Colours::black.withAlpha (0.15f)); g.drawLine (l.x - 12 + (float) i * 4.5f, l.y - 11, l.x - 12 + (float) i * 4.5f, l.y + 11, 0.6f); }
    // interruptor de palanca
    const juce::Point<float> t (r.getRight() - 30, r.getCentreY());
    g.setGradientFill (ColourGradient (Colour (0xffe6e8eb), t.x - 12, t.y - 12, Colour (0xff4c4f54), t.x + 12, t.y + 12, false)); g.fillEllipse (t.x - 13, t.y - 13, 26, 26);
    g.setColour (Colours::black.withAlpha (0.5f)); g.drawEllipse (t.x - 13, t.y - 13, 26, 26, 1.0f);
    const float dir = on ? -1.0f : 1.0f;
    Path bat; bat.startNewSubPath (t.x - 3, t.y); bat.lineTo (t.x - 5, t.y + dir * 26); bat.lineTo (t.x + 5, t.y + dir * 26); bat.lineTo (t.x + 3, t.y); bat.closeSubPath();
    g.setGradientFill (ColourGradient (Colour (0xfff4f5f7), t.x - 5, 0, Colour (0xff6c6f74), t.x + 5, 0, false)); g.fillPath (bat);
    g.setColour (Colour (0xffcfd2d6)); g.fillEllipse (t.x - 6, t.y + dir * 26 - 6, 12, 12);
    g.setColour (Colours::black.withAlpha (0.4f)); g.drawEllipse (t.x - 6, t.y + dir * 26 - 6, 12, 12, 0.8f);
}
void Switch::mouseUp (const juce::MouseEvent&) { setParam (proc, "power", param (proc, "power") > 0.5f ? 0.0f : 1.0f); repaint(); }

Content::Content (TopEqProcessor& p)
    : proc (p),
      lowBoost (p.apvts, "lowBoost", false, A0, A1), lowAtten (p.apvts, "lowAtten", false, A0, A1), lowFreq (p.apvts, "lowFreq", true, selLow.a0, selLow.a1),
      highBoost (p.apvts, "highBoost", false, A0, A1), bandwidth (p.apvts, "bandwidth", false, A0, A1), highFreq (p.apvts, "highFreq", true, selHigh.a0, selHigh.a1),
      highAtten (p.apvts, "highAtten", false, A0, A1), attFreq (p.apvts, "attFreq", true, selAtt.a0, selAtt.a1),
      tube (p.apvts, "tube", false, A0, A1), out (p.apvts, "out", false, A0, A1),
      curve (p), power (p)
{
    for (juce::Component* c : std::initializer_list<juce::Component*> { &lowBoost, &lowAtten, &lowFreq, &highBoost, &bandwidth, &highFreq, &highAtten, &attFreq, &tube, &out, &curve, &power })
        addAndMakeVisible (c);
    lowBoost.setTooltip (U ("LOW BOOST: sube los graves en la frecuencia elegida (hasta +13.5 dB)"));
    lowAtten.setTooltip (U ("LOW ATTEN: baja los graves. Súbelo junto con BOOST para el truco: graves gordos sin barro"));
    lowFreq.setTooltip (U ("CPS: frecuencia de los graves (20, 30, 60, 100 Hz)"));
    highBoost.setTooltip (U ("HIGH BOOST: campana de agudos en la frecuencia KCS (hasta +16 dB)"));
    bandwidth.setTooltip (U ("BANDWIDTH: ancho de la campana de agudos (izquierda = angosta, derecha = ancha)"));
    highFreq.setTooltip (U ("KCS: frecuencia del realce de agudos (3 a 16 kHz)"));
    highAtten.setTooltip (U ("HIGH ATTEN: baja los agudos de forma suave (hasta -16 dB)"));
    attFreq.setTooltip (U ("ATTEN SEL: desde dónde baja los agudos (5, 10 o 20 kHz)"));
    tube.setTooltip (U ("TUBE: color de la válvula y los transformadores (0 = limpio)"));
    out.setTooltip (U ("OUTPUT: volumen de salida (-12 a +12 dB)"));
    power.setTooltip (U ("IN: enciende o apaga el ecualizador"));
}

void Content::resized()
{
    auto place = [] (juce::Component& c, float cx, float cy, float d) { c.setBounds (juce::roundToInt (cx - d * 0.5f), juce::roundToInt (cy - d * 0.5f), juce::roundToInt (d), juce::roundToInt (d)); };
    place (lowBoost, 150, 190, 104); place (lowAtten, 330, 190, 104); place (lowFreq, 240, 362, 76);
    place (highBoost, 520, 190, 104); place (bandwidth, 700, 190, 104); place (highFreq, 610, 362, 76);
    place (highAtten, 870, 190, 104); place (attFreq, 870, 362, 76);
    curve.setBounds (968, 112, 170, 84);
    place (tube, 1005, 268, 66); place (out, 1100, 268, 66);
    power.setBounds (975, 340, 160, 60);
}

void Content::paint (Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    // orejas de rack
    g.setGradientFill (ColourGradient (Colour (0xff9aa9b4), 0, 0, Colour (0xff6c7a85), 0, r.getBottom(), false)); g.fillRect (r);
    // panel con pintura martillada
    auto pan = r.reduced (46, 0);
    g.setGradientFill (ColourGradient (face.brighter (0.15f), pan.getX(), pan.getY(), face.darker (0.25f), pan.getX(), pan.getBottom(), false)); g.fillRect (pan);
    {
        juce::Random rnd (42);
        for (int i = 0; i < 2600; ++i)
        {
            const float x = pan.getX() + rnd.nextFloat() * pan.getWidth(), y = pan.getY() + rnd.nextFloat() * pan.getHeight(), s = 2.0f + rnd.nextFloat() * 7.0f;
            g.setColour ((rnd.nextBool() ? Colours::white : Colours::black).withAlpha (0.025f + 0.03f * rnd.nextFloat()));
            g.fillEllipse (x, y, s, s * (0.6f + 0.6f * rnd.nextFloat()));
        }
    }
    g.setColour (Colours::black.withAlpha (0.45f)); g.drawLine (pan.getX(), 0, pan.getX(), r.getBottom(), 2.0f); g.drawLine (pan.getRight(), 0, pan.getRight(), r.getBottom(), 2.0f);
    for (float x : { 23.0f, r.getRight() - 23.0f })
        for (float y : { 60.0f, r.getBottom() - 60.0f })
        {
            g.setColour (Colour (0xff1a1e22)); g.fillRoundedRectangle (x - 7, y - 14, 14, 28, 7);
            g.setColour (Colours::white.withAlpha (0.3f)); g.drawRoundedRectangle (x - 7, y - 13, 14, 28, 7, 1.0f);
        }
    for (float x : { 66.0f, r.getRight() - 66.0f }) for (float y : { 20.0f, r.getBottom() - 20.0f }) screw (g, x, y);

    // placa de aluminio con el nombre
    Rectangle<float> plate (420, 18, 360, 52);
    g.setColour (Colours::black.withAlpha (0.4f)); g.fillRoundedRectangle (plate.translated (0, 2), 4);
    g.setGradientFill (ColourGradient (Colour (0xffe9ebee), plate.getX(), plate.getY(), Colour (0xffa4a8ae), plate.getX(), plate.getBottom(), false)); g.fillRoundedRectangle (plate, 4);
    for (float x = plate.getX() + 1; x < plate.getRight(); x += 1.5f) { g.setColour (Colours::white.withAlpha (0.06f)); g.drawVerticalLine ((int) x, plate.getY() + 1, plate.getBottom() - 1); }
    g.setColour (Colours::black.withAlpha (0.45f)); g.drawRoundedRectangle (plate, 4, 1.0f);
    screw (g, plate.getX() + 12, plate.getCentreY(), 4); screw (g, plate.getRight() - 12, plate.getCentreY(), 4);
    g.setFont (font (25, true)); g.setColour (Colour (0xff1c232b)); g.drawText ("TOP  EQ-1A", plate.withHeight (32).translated (0, 3), juce::Justification::centred);
    g.setFont (font (10.5f, true)); g.drawText ("P R O G R A M    E Q U A L I Z E R", plate.withTrimmedTop (32), juce::Justification::centred);
    engraved (g, "Dj Pava HM Music", { 70, 26, 300, 22 }, 19, true, juce::Justification::left);
    engraved (g, "TUBE  PASSIVE  EQUALIZER", { 70, 48, 300, 16 }, 10.5f, false, juce::Justification::left);

    // presets
    g.setColour (Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (presetArea, 4);
    g.setColour (Colours::white.withAlpha (0.25f)); g.drawRoundedRectangle (presetArea, 4, 1.0f);
    g.setFont (font (13)); g.setColour (cream);
    g.drawText ("PRESET:  " + proc.getProgramName (proc.getCurrentProgram()), presetArea.reduced (10, 0), juce::Justification::centredLeft);
    g.drawText (U ("▾"), presetArea.reduced (10, 0), juce::Justification::centredRight);

    // secciones
    auto section = [&g] (Rectangle<float> s, const char* name)
    {
        g.setColour (ink.withAlpha (0.55f)); g.drawRoundedRectangle (s, 6, 1.4f);
        g.setColour (Colours::white.withAlpha (0.2f)); g.drawRoundedRectangle (s.translated (1, 1), 6, 1.0f);
        const float tw = juce::GlyphArrangement::getStringWidth (font (13, true), name) + 20;
        Rectangle<float> t (s.getCentreX() - tw * 0.5f, s.getY() - 9, tw, 18);
        g.setColour (face); g.fillRect (t);
        engraved (g, name, t, 13);
    };
    section ({ 66, 96, 352, 352 }, "LOW FREQUENCY");
    section ({ 436, 96, 352, 352 }, "HIGH FREQUENCY");
    section ({ 806, 96, 128, 352 }, "ATTEN");
    section ({ 952, 96, 202, 352 }, "OUTPUT");
    g.setColour (ink.withAlpha (0.3f)); g.drawVerticalLine (2, 0, 0);

    // escalas 0..10 de las perillas grandes
    auto scale = [&g] (float cx, float cy, float R, const char* title, float sizeT = 14.0f, bool dB = false)
    {
        const bool small = R < 40;
        for (int i = 0; i <= 10; ++i)
        {
            const float a = A0 + (A1 - A0) * (float) i / 10.0f;
            g.setColour (ink); g.drawLine (cx + (R + 2) * std::sin (a), cy - (R + 2) * std::cos (a), cx + (R + 9) * std::sin (a), cy - (R + 9) * std::cos (a), i % 5 == 0 ? 2.0f : 1.2f);
            if (small ? i % 5 == 0 : i % 2 == 0) engraved (g, dB ? juce::String (i == 0 ? "-12" : i == 5 ? "0" : "+12") : juce::String (i), { cx + (R + 20) * std::sin (a) - 12, cy - (R + 20) * std::cos (a) - 8, 24, 16 }, small ? 11.5f : 13);
        }
        engraved (g, title, { cx - 70, cy + R + 22, 140, 18 }, sizeT);
    };
    scale (150, 190, 52, "BOOST"); scale (330, 190, 52, "ATTEN");
    scale (520, 190, 52, "BOOST"); scale (700, 190, 52, "BANDWIDTH");
    engraved (g, "SHARP", { 640, 248, 60, 14 }, 10, false); engraved (g, "BROAD", { 700, 248, 60, 14 }, 10, false);
    scale (870, 190, 52, "ATTEN");
    scale (1005, 268, 33, "TUBE", 12); scale (1100, 268, 33, "OUTPUT", 12, true);
    // escalas de los selectores
    auto selScale = [&g] (float cx, float cy, float R, const Sel& s, std::initializer_list<const char*> labels, const char* title)
    {
        int i = 0;
        for (auto* l : labels)
        {
            const float a = s.a0 + (s.a1 - s.a0) * (float) i / (float) (s.n - 1);
            g.setColour (ink); g.drawLine (cx + (R + 2) * std::sin (a), cy - (R + 2) * std::cos (a), cx + (R + 8) * std::sin (a), cy - (R + 8) * std::cos (a), 1.6f);
            engraved (g, l, { cx + (R + 22) * std::sin (a) - 18, cy - (R + 22) * std::cos (a) - 8, 36, 16 }, 13);
            ++i;
        }
        engraved (g, title, { cx - 60, cy + R + 8, 120, 16 }, 12);
    };
    selScale (240, 362, 38, selLow, { "20", "30", "60", "100" }, "CPS");
    selScale (610, 362, 38, selHigh, { "3", "4", "5", "8", "10", "12", "16" }, "KCS");
    selScale (870, 362, 38, selAtt, { "5", "10", "20" }, "ATTEN SEL  KCS");
    engraved (g, "IN", { 1092, 404, 40, 14 }, 11);
    engraved (g, "POWER", { 975, 404, 56, 14 }, 11);
}

void Content::mouseUp (const juce::MouseEvent& e)
{
    if (! presetArea.contains (e.position)) return;
    juce::PopupMenu m; m.addSectionHeader ("PRESETS");
    for (int i = 0; i < proc.getNumPrograms(); ++i) m.addItem (i + 1, proc.getProgramName (i), true, i == proc.getCurrentProgram());
    juce::Component::SafePointer<Content> sp (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (presetArea.toNearestInt())), [sp] (int r)
    { if (sp != nullptr && r > 0) { sp->proc.setCurrentProgram (r - 1); sp->repaint(); } });
}

void Content::tick()
{
    const auto s = proc.settings(); const float pw = param (proc, "power"), o = param (proc, "out");
    if (! (s == last) || pw != lastPower || o != lastOut) { last = s; lastPower = pw; lastOut = o; curve.repaint(); power.repaint(); repaint (presetArea.toNearestInt()); }
}

TopEqEditor::TopEqEditor (TopEqProcessor& p) : AudioProcessorEditor (p), content (p)
{
    setLookAndFeel (&look);
    addAndMakeVisible (content);
    content.setBounds (0, 0, W, H);
    setResizable (true, true);
    setResizeLimits (840, 329, 1920, 752);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) W / (double) H);
    setSize (W, H);
    startTimerHz (30);
}
TopEqEditor::~TopEqEditor() { setLookAndFeel (nullptr); }
void TopEqEditor::resized() { content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) W)); }
