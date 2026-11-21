#include "TopTuneEditor.h"

using namespace ttui;

namespace
{
    juce::String U (const char* s) { return juce::String::fromUTF8 (s); }
    juce::Font font (float h, bool bold = false) { return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain)); }
    juce::Font spaced (float h, float k = 0.18f) { return font (h, true).withExtraKerningFactor (k); }
    const juce::Colour inTune (0xff57f0a0);
    constexpr float KA0 = juce::MathConstants<float>::pi * 1.25f, KA1 = juce::MathConstants<float>::pi * 2.75f;
    const char* scaleUpper[] { "CROMÁTICA", "MAYOR", "MENOR", "MENOR ARMÓNICA", "PENTA MAYOR", "PENTA MENOR", "BLUES", "PERSONAL" };
    const char* blackNames[] { "", "C#/D\xe2\x99\xad", "", "D#/E\xe2\x99\xad", "", "", "F#/G\xe2\x99\xad", "", "G#/A\xe2\x99\xad", "", "A#/B\xe2\x99\xad", "" };
}

const Palette& ttui::palette (bool light)
{
    static const Palette dark { juce::Colour (0xff1c1d20), juce::Colour (0xff0e0f11), juce::Colour (0xff141518), juce::Colour (0xffe6e8ec), juce::Colour (0xff8e9299),
                                juce::Colour (0xffcfd4db), juce::Colour (0xfff6f8fb), juce::Colour (0xff2b2d31), juce::Colour (0xff16171a), juce::Colour (0xff050506),
                                juce::Colour (0xff09090a), juce::Colour (0xff18191c), false };
    static const Palette silver { juce::Colour (0xffe4e6e9), juce::Colour (0xffc3c6cb), juce::Colour (0xffd6d8db), juce::Colour (0xff17181b), juce::Colour (0xff5b5f66),
                                  juce::Colour (0xff2c2f34), juce::Colour (0xff0d0e10), juce::Colour (0xffa6a9af), juce::Colour (0xffe8e9eb), juce::Colour (0xff222326),
                                  juce::Colour (0xff09090a), juce::Colour (0xff18191c), true };
    return light ? silver : dark;
}

//==============================================================================
LnF::LnF()
{
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff17181b));
    setColour (juce::PopupMenu::textColourId, juce::Colour (0xffe6e8ec));
    setColour (juce::PopupMenu::headerTextColourId, juce::Colour (0xff8e9299));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff3a3d43));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff17181b));
    setColour (juce::TooltipWindow::textColourId, juce::Colour (0xffe6e8ec));
    setColour (juce::TooltipWindow::outlineColourId, juce::Colour (0xff3a3d43));
    setColour (juce::ComboBox::textColourId, juce::Colour (0xffe6e8ec));
}

void LnF::drawBubble (juce::Graphics& g, juce::BubbleComponent&, const juce::Point<float>&, const juce::Rectangle<float>& body)
{
    g.setColour (juce::Colour (0xf0111214)); g.fillRoundedRectangle (body, 5);
    g.setColour (juce::Colour (0xff6d7178)); g.drawRoundedRectangle (body.reduced (0.5f), 5, 1.0f);
}
juce::Font LnF::getSliderPopupFont (juce::Slider&) { return font (13.0f, true); }

void LnF::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const auto& P = palette (light);
    const juce::Point<float> c ((float) x + (float) w * 0.5f, (float) y + (float) h * 0.5f);
    const float R = (float) juce::jmin (w, h) * 0.5f - 10.0f;
    // puntos de escala
    for (int i = 0; i <= 10; ++i)
    {
        const float t = a0 + (a1 - a0) * (float) i / 10.0f;
        g.setColour (P.dim.withAlpha (i == 0 || i == 10 || i == 5 ? 0.9f : 0.5f));
        const float rr = R + 6.5f, d = i % 5 == 0 ? 2.4f : 1.7f;
        g.fillEllipse (c.x + rr * std::sin (t) - d * 0.5f, c.y - rr * std::cos (t) - d * 0.5f, d, d);
    }
    // sombra suave
    for (int i = 6; i > 0; --i)
    {
        const float e = (float) i * R * 0.035f;
        g.setColour (juce::Colours::black.withAlpha (P.light ? 0.05f : 0.10f));
        g.fillEllipse (c.x - R - e, c.y - R - e + e * 1.4f, (R + e) * 2, (R + e) * 2);
    }
    // aro exterior
    const float ringW = R * 0.15f;
    if (P.light)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3c40), c.x, c.y - R, juce::Colour (0xff0c0c0d), c.x, c.y + R, false));
        g.fillEllipse (c.x - R, c.y - R, R * 2, R * 2);
    }
    else
    {
        juce::ColourGradient rg (juce::Colour (0xfff2f4f7), c.x - R * 0.5f, c.y - R, juce::Colour (0xff55595f), c.x + R * 0.4f, c.y + R, false);
        rg.addColour (0.55, juce::Colour (0xff9da2a9));
        g.setGradientFill (rg); g.fillEllipse (c.x - R, c.y - R, R * 2, R * 2);
        // cepillado concentrico
        for (int i = 0; i < 4; ++i) { const float rr = R - ringW * (0.2f + 0.2f * (float) i); g.setColour (juce::Colours::black.withAlpha (0.05f)); g.drawEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2, 0.6f); }
    }
    // cuerpo
    const float rb = R - ringW;
    juce::ColourGradient bg (P.light ? juce::Colour (0xfff7f8f9) : juce::Colour (0xff2c2e33), c.x - rb * 0.4f, c.y - rb * 0.8f,
                             P.light ? juce::Colour (0xffbfc2c7) : juce::Colour (0xff060607), c.x + rb * 0.5f, c.y + rb, true);
    bg.addColour (0.55, P.knobBody);
    g.setGradientFill (bg); g.fillEllipse (c.x - rb, c.y - rb, rb * 2, rb * 2);
    g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawEllipse (c.x - rb, c.y - rb, rb * 2, rb * 2, 1.0f);
    // reflejo
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (P.light ? 0.6f : 0.10f), c.x, c.y - rb, juce::Colours::transparentWhite, c.x, c.y, false));
    g.fillEllipse (c.x - rb * 0.8f, c.y - rb * 0.95f, rb * 1.6f, rb * 1.1f);
    // arco de valor sobre el aro
    const float ra = R - ringW * 0.5f;
    const float v = a0 + pos * (a1 - a0);
    const bool bipolar = s.getProperties().getWithDefault ("bipolar", false);
    const float from = bipolar ? (a0 + a1) * 0.5f : a0;
    juce::Path arc; arc.addCentredArc (c.x, c.y, ra, ra, 0, juce::jmin (from, v), juce::jmax (from, v), true);
    const auto arcCol = P.light ? juce::Colour (0xfff2f3f5) : juce::Colour (0xfff7f9fc);
    if (! P.light) { g.setColour (arcCol.withAlpha (0.18f)); g.strokePath (arc, juce::PathStrokeType (ringW * 1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)); }
    g.setColour (P.light ? juce::Colour (0xffd9dbde) : juce::Colour (0xff121315));
    if (! P.light) { juce::Path track; track.addCentredArc (c.x, c.y, ra, ra, 0, a0, a1, true); g.setColour (juce::Colours::black.withAlpha (0.35f)); g.strokePath (track, juce::PathStrokeType (ringW * 0.45f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)); }
    g.setColour (arcCol); g.strokePath (arc, juce::PathStrokeType (ringW * 0.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    // indicador
    g.setColour (P.text.withAlpha (0.8f));
    g.drawLine (juce::Line<float> ({ c.x + rb * 0.78f * std::sin (v), c.y - rb * 0.78f * std::cos (v) }, { c.x + rb * 0.93f * std::sin (v), c.y - rb * 0.93f * std::cos (v) }), 2.0f);
    // valor
    auto t = s.getTextFromValue (s.getValue()).replace (" %", "%").replace (" st", "").replace (" cents", "c");
    g.setColour (P.text); g.setFont (font (juce::jmax (11.0f, rb * 0.36f), true));
    g.drawFittedText (t, juce::Rectangle<float> (c.x - rb * 0.8f, c.y - rb * 0.3f, rb * 1.6f, rb * 0.6f).toNearestInt(), juce::Justification::centred, 1, 0.7f);
}

void LnF::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider& s)
{
    const auto& P = palette (light);
    const float cy = (float) y + (float) h * 0.5f;
    const auto track = juce::Rectangle<float> ((float) x, cy - 6.0f, (float) w, 12.0f);
    g.setColour (P.light ? juce::Colour (0xffb7bac0) : juce::Colour (0xff060607)); g.fillRoundedRectangle (track, 6);
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.5f), 0, track.getY(), juce::Colours::transparentBlack, 0, track.getY() + 5, false));
    g.fillRoundedRectangle (track, 6);
    const bool bipolar = s.getProperties().getWithDefault ("bipolar", false);
    const float x0 = bipolar ? track.getCentreX() : track.getX() + 3;
    auto fill = juce::Rectangle<float> (juce::jmin (x0, pos), cy - 3.5f, std::abs (pos - x0), 7.0f);
    if (P.light) g.setColour (juce::Colour (0xff1d1e21));
    else
    {
        g.setColour (juce::Colours::white.withAlpha (0.15f)); g.fillRoundedRectangle (fill.expanded (2.0f, 2.5f), 5);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff8c9198), track.getX(), 0, juce::Colour (0xfff8f9fb), track.getRight(), 0, false));
    }
    g.fillRoundedRectangle (fill, 3.5f);
    // tirador
    const auto th = juce::Rectangle<float> (pos - 11, cy - 11, 22, 22);
    g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillEllipse (th.translated (0, 2));
    juce::ColourGradient tg (juce::Colour (0xfff7f8fa), th.getX(), th.getY(), juce::Colour (0xff80858c), th.getRight(), th.getBottom(), false);
    g.setGradientFill (tg); g.fillEllipse (th);
    g.setColour (juce::Colours::black.withAlpha (0.5f)); g.drawEllipse (th, 0.8f);
    g.setColour (juce::Colour (0xff4a4d52)); g.drawLine (pos, th.getY() + 6, pos, th.getBottom() - 6, 1.4f);
}

void LnF::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (juce::Colour (0xff141517)); g.fillRoundedRectangle (r, 6);
    g.setColour (juce::Colour (0xff33363b)); g.drawRoundedRectangle (r, 6, 1.0f);
    g.setColour (juce::Colour (0xff9aa0a8)); g.setFont (spaced (11.0f, 0.15f));
    g.drawText ("ESTILO", 12, 0, 60, h, juce::Justification::centredLeft);
    juce::Path a; const float ax = (float) w - 16, ay = (float) h * 0.5f;
    a.startNewSubPath (ax - 4, ay - 2); a.lineTo (ax, ay + 2); a.lineTo (ax + 4, ay - 2);
    g.setColour (juce::Colour (0xffcfd4db)); g.strokePath (a, juce::PathStrokeType (1.6f));
}
void LnF::positionComboBoxText (juce::ComboBox& b, juce::Label& l)
{
    l.setBounds (68, 0, b.getWidth() - 90, b.getHeight());
    l.setFont (font (13.0f, true));
    l.setColour (juce::Label::textColourId, juce::Colour (0xfff2f4f7));
}
juce::Font LnF::getComboBoxFont (juce::ComboBox&) { return font (13.0f, true); }

//==============================================================================
Knob::Knob (APVTS& s, const juce::String& id, bool big)
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (KA0, KA1, true);
    setMouseDragSensitivity (big ? 300 : 220);
    setVelocityModeParameters (0.6, 1, 0.08, true, juce::ModifierKeys::shiftModifier);
    att = std::make_unique<APVTS::SliderAttachment> (s, id, *this);
    if (auto* p = s.getParameter (id)) setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
}

//==============================================================================
void Switch::paintButton (juce::Graphics& g, bool, bool)
{
    const auto& P = palette (lightTheme);
    auto r = getLocalBounds().toFloat();
    if (sun) r = r.withWidth (r.getHeight() * 2.0f);
    r = r.reduced (1.5f);
    const bool on = getToggleState();
    g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (r.translated (0, 1), r.getHeight() * 0.5f);
    if (on && ! sun)
    {
        g.setColour (P.glow.withAlpha (P.light ? 0.0f : 0.25f)); g.fillRoundedRectangle (r.expanded (3), r.getHeight() * 0.5f + 3);
        g.setGradientFill (juce::ColourGradient (P.light ? juce::Colour (0xff2a2c30) : juce::Colour (0xfffafbfc), r.getX(), r.getY(), P.light ? juce::Colour (0xff0d0e10) : juce::Colour (0xff9aa0a8), r.getX(), r.getBottom(), false));
    }
    else g.setColour (P.light ? juce::Colour (0xffb5b8bd) : juce::Colour (0xff2a2c30));
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (juce::Colours::black.withAlpha (0.4f)); g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 0.8f);
    const float d = r.getHeight() - 4.0f;
    const float kx = on ? r.getRight() - d - 2 : r.getX() + 2;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffffff), kx, r.getY(), juce::Colour (0xff8d9198), kx + d, r.getBottom(), false));
    if (on && ! sun && ! P.light) g.setColour (juce::Colour (0xff15161a));
    g.fillEllipse (kx, r.getY() + 2, d, d);
    if (sun)
    {
        const float cx = r.getRight() + 13, cy = r.getCentreY();
        g.setColour (P.dim);
        g.drawEllipse (cx - 4, cy - 4, 8, 8, 1.3f);
        for (int i = 0; i < 8; ++i)
        {
            const float a = (float) i * juce::MathConstants<float>::pi / 4;
            g.drawLine (cx + 6 * std::cos (a), cy + 6 * std::sin (a), cx + 8.5f * std::cos (a), cy + 8.5f * std::sin (a), 1.2f);
        }
    }
}

void Pill::paintButton (juce::Graphics& g, bool over, bool down)
{
    const auto& P = palette (lightTheme);
    auto r = getLocalBounds().toFloat().reduced (1.5f);
    const bool on = getToggleState();
    if (segment)
    {
        if (on)
        {
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff34373c), r.getX(), r.getY(), juce::Colour (0xff1b1c1f), r.getX(), r.getBottom(), false));
            g.fillRoundedRectangle (r, 6);
            g.setColour (juce::Colour (0xff4b4f55)); g.drawRoundedRectangle (r, 6, 1.0f);
            g.setColour (juce::Colours::white.withAlpha (0.25f)); g.setFont (spaced (13.0f, 0.08f));
            g.drawText (getButtonText(), r.translated (0, 0.5f).expanded (1), juce::Justification::centred);
        }
        g.setColour (on ? juce::Colour (0xfff7f9fc) : (over ? juce::Colour (0xffb7bcc4) : juce::Colour (0xff7d828a)));
        g.setFont (spaced (13.0f, 0.08f));
        g.drawText (getButtonText(), r, juce::Justification::centred);
        return;
    }
    // boton fisico
    const float cr = circle ? r.getHeight() * 0.5f : juce::jmin (10.0f, r.getHeight() * 0.35f);
    g.setColour (juce::Colours::black.withAlpha (P.light ? 0.12f : 0.4f)); g.fillRoundedRectangle (r.translated (0, 1.5f), cr);
    juce::ColourGradient bg (P.light ? juce::Colour (over ? 0xfff8f9fa : 0xffeeeff1) : juce::Colour (over ? 0xff2e3035 : 0xff25272b), r.getX(), r.getY(),
                             P.light ? juce::Colour (0xffc9ccd0) : juce::Colour (0xff131416), r.getX(), r.getBottom(), false);
    g.setGradientFill (bg); g.fillRoundedRectangle (r.translated (0, down ? 0.8f : 0.0f), cr);
    g.setColour (on ? P.glow : P.line); g.drawRoundedRectangle (r, cr, on ? 1.4f : 1.0f);
    if (on && ! P.light) { g.setColour (P.glow.withAlpha (0.15f)); g.drawRoundedRectangle (r.expanded (2), cr + 2, 3.0f); }
    g.setColour (on ? P.glow : P.text);
    const auto c = r.getCentre();
    if (icon == 1) { for (int i = -1; i <= 1; ++i) g.fillRoundedRectangle (c.x - 7, c.y + (float) i * 5.0f - 1.0f, 14, 2.0f, 1.0f); return; }
    if (icon == 2 || icon == 3)
    {
        const float d = icon == 2 ? -1.0f : 1.0f, rr = r.getHeight() * 0.24f;
        juce::Path p; p.addCentredArc (c.x, c.y, rr, rr, 0, d * 0.6f + (d < 0 ? 0.0f : 0.0f), d * (juce::MathConstants<float>::twoPi - 0.9f), true);
        g.strokePath (p, juce::PathStrokeType (1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const float ea = d * 0.6f;
        const juce::Point<float> e (c.x + rr * std::sin (ea), c.y - rr * std::cos (ea));
        juce::Path a; a.addTriangle (e.x - d * 4.5f, e.y - 3.5f, e.x + d * 1.5f, e.y - 4.5f, e.x - d * 0.5f, e.y + 2.5f);
        g.fillPath (a);
        return;
    }
    g.setFont (circle ? font (13.0f, true) : (getButtonText().length() > 6 ? font (16.0f, true) : spaced (12.5f, 0.12f)));
    g.drawFittedText (getButtonText(), r.reduced (8, 0).toNearestInt(), juce::Justification::centred, 1, 0.7f);
}

//==============================================================================
void KeyPanel::tick()
{
    const float m = proc.meters.inMidi.load();
    const bool voiced = proc.meters.pitchHz.load() > 0.0f;
    if (voiced)
    {
        note = (int) std::round (m);
        cents = (m - (float) note) * 100.0f;
        glow += (1.0f - glow) * 0.4f;
    }
    else glow *= 0.85f;
    shownCents += (cents - shownCents) * 0.35f;
    if (proc.meters.keyListening.load())
    {
        float h[12]; float tot = 0; for (int i = 0; i < 12; ++i) { h[i] = proc.meters.keyHist[i].load(); tot += h[i]; }
        if (tot > 1.0f) { float conf; toptune::KeyFinder::find (h, foundRoot, foundMinor, conf); }
    }
    repaint();
}

void KeyPanel::paint (juce::Graphics& g)
{
    const auto& P = palette (lightTheme);
    auto r = getLocalBounds().toFloat();
    // pantalla de cristal negro
    for (int i = 5; i > 0; --i) { g.setColour (juce::Colours::black.withAlpha (P.light ? 0.04f : 0.10f)); g.fillRoundedRectangle (r.reduced (10).expanded ((float) i * 1.2f).translated (0, (float) i * 0.8f), 16 + (float) i); }
    r = r.reduced (10);
    g.setGradientFill (juce::ColourGradient (P.screen1, r.getX(), r.getY(), P.screen0, r.getRight(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 16);
    juce::Path gloss; gloss.addRoundedRectangle (r, 16);
    g.saveState(); g.reduceClipRegion (gloss);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.07f), r.getX(), r.getY(), juce::Colours::transparentWhite, r.getCentreX(), r.getCentreY() + 20, false));
    g.fillRect (r);
    g.restoreState();
    g.setColour (juce::Colour (0xff2e3034)); g.drawRoundedRectangle (r, 16, 1.2f);
    const auto text = juce::Colour (0xffeef0f3), dim = juce::Colour (0xff8d9198);
    const float cx = r.getCentreX();
    g.setColour (dim); g.setFont (spaced (11.0f, 0.3f));
    g.drawText ("KEY & SCALE", juce::Rectangle<float> (r.getX(), r.getY() + 12, r.getWidth(), 16), juce::Justification::centred);
    // boton AUTO (detectar tonalidad)
    const bool listening = proc.meters.keyListening.load();
    detectArea = juce::Rectangle<float> (r.getRight() - 58, r.getY() + 10, 48, 18);
    g.setColour (listening ? juce::Colour (0xfff6f8fb) : juce::Colour (0xff2a2c31)); g.fillRoundedRectangle (detectArea, 9);
    g.setColour (listening ? juce::Colour (0xff111214) : dim); g.setFont (spaced (9.5f, 0.1f));
    g.drawText (listening ? "STOP" : "AUTO", detectArea, juce::Justification::centred);
    // tonalidad
    const int key = (int) proc.apvts.getRawParameterValue ("key")->load();
    const int scale = (int) proc.apvts.getRawParameterValue ("scale")->load();
    const auto kn = TopTuneProcessor::keyNames()[key];
    keyArea = juce::Rectangle<float> (r.getX() + 20, r.getY() + 32, r.getWidth() - 40, 74);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffffff), cx, keyArea.getY() + 8, juce::Colour (0xffa9aeb6), cx, keyArea.getBottom(), false));
    g.setFont (font (scale == 0 ? 50.0f : 76.0f, true));
    g.drawText (scale == 0 ? U ("ALL") : kn, keyArea, juce::Justification::centred);
    scaleArea = juce::Rectangle<float> (r.getX() + 20, r.getY() + 106, r.getWidth() - 40, 26);
    g.setColour (juce::Colour (0xffb4b9c1)); g.setFont (spaced (19.0f, 0.12f));
    g.drawText (U (scaleUpper[juce::jlimit (0, 7, scale)]), scaleArea, juce::Justification::centred);
    if (listening && foundRoot >= 0)
    {
        g.setColour (dim); g.setFont (font (10.5f, true));
        g.drawText (U ("Escuchando: ") + TopTuneProcessor::keyNames()[foundRoot] + (foundMinor ? " menor" : " mayor") + U (" · STOP para aplicar"),
                    juce::Rectangle<float> (r.getX(), r.getY() + 136, r.getWidth(), 14), juce::Justification::centred);
    }
    // nota detectada
    const bool voiced = glow > 0.05f && note >= 0;
    const float ty = r.getY() + 166;
    g.setColour (dim); g.setFont (spaced (10.0f, 0.2f));
    g.drawText ("DETECTADO", juce::Rectangle<float> (r.getX(), ty - 8, cx - r.getX() - 4, 16), juce::Justification::centredRight);
    const auto nn = voiced ? TopTuneProcessor::keyNames()[((note % 12) + 12) % 12] : juce::String ("-");
    const bool tuned = std::abs (shownCents) < 6.0f;
    g.setColour ((voiced ? (tuned ? inTune : text) : dim).withAlpha (voiced ? 1.0f : 0.6f)); g.setFont (font (24.0f, true));
    g.drawText (nn, juce::Rectangle<float> (cx + 2, ty - 14, 44, 28), juce::Justification::centredLeft);
    g.setColour (dim); g.setFont (font (11.0f, true));
    g.drawText ((shownCents >= 0 ? "+" : "") + juce::String ((int) std::round (voiced ? shownCents : 0.0f)) + U ("¢"),
                juce::Rectangle<float> (cx + 2 + font (24.0f, true).getStringWidthFloat (nn) + 4, ty - 7, 44, 16), juce::Justification::centredLeft);
    // afinador -50..+50
    const float y0 = r.getY() + 186, y1 = r.getY() + 222, xa = r.getX() + 26, xb = r.getRight() - 26;
    for (int i = 0; i <= 20; ++i)
    {
        const float x = xa + (xb - xa) * (float) i / 20.0f;
        g.setColour (juce::Colour (0xff3a3c41).withAlpha (i == 10 ? 1.0f : 0.8f));
        g.drawLine (x, y0 + (i % 5 == 0 ? 0.0f : 6.0f), x, y1 - (i % 5 == 0 ? 0.0f : 6.0f), i == 10 ? 1.4f : 1.0f);
    }
    g.setColour (juce::Colour (0xff6a6e75).withAlpha (0.35f)); g.drawHorizontalLine ((int) ((y0 + y1) * 0.5f), xa, xb);
    const float nx = xa + (xb - xa) * (0.5f + juce::jlimit (-50.0f, 50.0f, voiced ? shownCents : 0.0f) / 100.0f);
    const auto nc = voiced ? (tuned ? inTune : text) : dim;
    g.setColour (nc.withAlpha (0.25f * (voiced ? 1.0f : 0.4f))); g.fillRoundedRectangle (nx - 5, y0 - 4, 10, y1 - y0 + 8, 5);
    g.setColour (nc); g.fillRoundedRectangle (nx - 1.5f, y0 - 3, 3, y1 - y0 + 6, 1.5f);
    g.drawEllipse (nx - 6, (y0 + y1) * 0.5f - 6, 12, 12, 1.4f);
    g.setColour (dim.withAlpha (0.8f)); g.setFont (font (10.5f, true));
    g.drawText (U ("−50"), juce::Rectangle<float> (xa - 14, y1 + 6, 40, 14), juce::Justification::centredLeft);
    g.drawText ("0", juce::Rectangle<float> (cx - 20, y1 + 6, 40, 14), juce::Justification::centred);
    g.drawText ("+50", juce::Rectangle<float> (xb - 26, y1 + 6, 40, 14), juce::Justification::centredRight);
}

void KeyPanel::mouseUp (const juce::MouseEvent& e)
{
    const auto p = e.position;
    auto setChoice = [this] (const char* id, int v) { if (auto* prm = proc.apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 ((float) v)); };
    if (detectArea.contains (p))
    {
        if (proc.meters.keyListening.load())
        {
            proc.stopKeyDetection();
            if (foundRoot >= 0) { setChoice ("key", foundRoot); setChoice ("scale", foundMinor ? 2 : 1); }
        }
        else { foundRoot = -1; proc.startKeyDetection(); }
        return;
    }
    juce::Component::SafePointer<KeyPanel> safe (this);
    if (keyArea.contains (p))
    {
        juce::PopupMenu m; m.addSectionHeader ("Tonalidad");
        const int cur = (int) proc.apvts.getRawParameterValue ("key")->load();
        for (int i = 0; i < 12; ++i) m.addItem (i + 1, TopTuneProcessor::keyNames()[i], true, i == cur);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe, setChoice] (int r) { if (safe != nullptr && r > 0) setChoice ("key", r - 1); });
    }
    else if (scaleArea.contains (p))
    {
        juce::PopupMenu m; m.addSectionHeader ("Escala");
        const int cur = (int) proc.apvts.getRawParameterValue ("scale")->load();
        for (int i = 0; i < TopTuneProcessor::scaleNames().size(); ++i) m.addItem (i + 1, TopTuneProcessor::scaleNames()[i], true, i == cur);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe, setChoice] (int r) { if (safe != nullptr && r > 0) setChoice ("scale", r - 1); });
    }
}

//==============================================================================
juce::Rectangle<float> Piano::keyRect (int pc) const
{
    const auto r = getLocalBounds().toFloat().reduced (24, 22);
    const float wk = r.getWidth() / 7.0f;
    static const int whiteIdx[12] { 0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6 };
    static const int blackAfter[12] { -1, 0, -1, 1, -1, -1, 3, -1, 4, -1, 5, -1 };
    if (whiteIdx[pc] >= 0) return { r.getX() + wk * (float) whiteIdx[pc] + 2, r.getY(), wk - 4, r.getHeight() };
    const float bw = wk * 0.74f;
    return { r.getX() + wk * (float) (blackAfter[pc] + 1) - bw * 0.5f, r.getY() + 2, bw, r.getHeight() * 0.58f };
}
int Piano::hit (juce::Point<float> p) const
{
    for (int pc : { 1, 3, 6, 8, 10 }) if (keyRect (pc).contains (p)) return pc;
    for (int pc : { 0, 2, 4, 5, 7, 9, 11 }) if (keyRect (pc).contains (p)) return pc;
    return -1;
}
void Piano::tick()
{
    const int m = proc.getActiveMask();
    const int t = (int) proc.meters.targetNote.load();
    if (m != lastMask || t != lastTarget) { lastMask = m; lastTarget = t; repaint(); }
}
void Piano::mouseDown (const juce::MouseEvent& e)
{
    const int pc = hit (e.position);
    if (pc < 0) return;
    const int mask = proc.getActiveMask() ^ (1 << pc);
    if (mask == 0) return;
    proc.meters.customMask.store (mask);
    if (auto* prm = proc.apvts.getParameter ("scale")) prm->setValueNotifyingHost (prm->convertTo0to1 ((float) TopTuneProcessor::customScaleIndex));
    repaint();
}
void Piano::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    for (int i = 5; i > 0; --i) { g.setColour (juce::Colours::black.withAlpha (0.08f)); g.fillRoundedRectangle (r.reduced (10).expanded ((float) i * 1.2f).translated (0, (float) i * 0.8f), 18 + (float) i); }
    r = r.reduced (10);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff141518), r.getX(), r.getY(), juce::Colour (0xff050506), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 18);
    g.setColour (juce::Colour (0xff2e3034)); g.drawRoundedRectangle (r, 18, 1.2f);
    const int mask = proc.getActiveMask();
    const int target = (int) proc.meters.targetNote.load();
    const int tpc = target >= 0 ? target % 12 : -1;
    const char* whiteNames[12] { "C", "", "D", "", "E", "F", "", "G", "", "A", "", "B" };
    for (int pc : { 0, 2, 4, 5, 7, 9, 11 })
    {
        const auto k = keyRect (pc);
        const bool in = (mask >> pc) & 1, cur = pc == tpc;
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillRoundedRectangle (k.translated (0, 2), 7);
        juce::ColourGradient kg (in ? juce::Colour (0xfff4f5f7) : juce::Colour (0xff8a8e94), k.getX(), k.getY(), in ? juce::Colour (0xffc9ccd1) : juce::Colour (0xff5e6268), k.getX(), k.getBottom(), false);
        kg.addColour (0.85, in ? juce::Colour (0xffdfe1e5) : juce::Colour (0xff767a80));
        g.setGradientFill (kg); g.fillRoundedRectangle (k, 7);
        // borde inferior de estado
        const auto bar = k.withTop (k.getBottom() - 5).reduced (1.0f, 0);
        if (in)
        {
            g.setColour (juce::Colour (0xff15161a)); g.fillRoundedRectangle (bar, 2.5f);
            if (cur) { g.setColour (juce::Colours::white.withAlpha (0.45f)); g.fillRoundedRectangle (k.expanded (3), 9); g.setColour (juce::Colour (0xfff4f5f7)); g.fillRoundedRectangle (k, 7); g.setColour (inTune); g.fillRoundedRectangle (bar, 2.5f); }
        }
        g.setColour (in ? juce::Colour (0xff141518) : juce::Colour (0xff3e4146)); g.setFont (font (22.0f, true));
        g.drawText (whiteNames[pc], k.withTop (k.getBottom() - 46).withHeight (32), juce::Justification::centred);
    }
    for (int pc : { 1, 3, 6, 8, 10 })
    {
        const auto k = keyRect (pc);
        const bool in = (mask >> pc) & 1, cur = pc == tpc;
        g.setColour (juce::Colours::black.withAlpha (0.7f)); g.fillRoundedRectangle (k.translated (1, 4), 7);
        if (cur && in) { g.setColour (juce::Colours::white.withAlpha (0.35f)); g.fillRoundedRectangle (k.expanded (3), 9); }
        juce::ColourGradient kg (juce::Colour (in ? 0xff34363b : 0xff1d1e21), k.getX(), k.getY(), juce::Colour (0xff060607), k.getX(), k.getBottom(), false);
        g.setGradientFill (kg); g.fillRoundedRectangle (k, 7);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.12f), k.getX(), k.getY(), juce::Colours::transparentWhite, k.getX(), k.getY() + 30, false));
        g.fillRoundedRectangle (k.reduced (3, 2).withHeight (28), 5);
        if (in) { g.setColour (cur ? inTune : juce::Colour (0xffd3d7de)); g.fillRoundedRectangle (k.withTop (k.getBottom() - 4).reduced (8, 0), 2); }
        g.setColour (in ? juce::Colour (0xffe8ebef) : juce::Colour (0xff595d63)); g.setFont (font (13.0f, true));
        g.drawText (U (blackNames[pc]), k.withTop (k.getBottom() - 34).withHeight (22), juce::Justification::centred);
    }
}

//==============================================================================
TopTuneEditor::TopTuneEditor (TopTuneProcessor& p)
    : AudioProcessorEditor (&p), proc (p), keyPanel (p), piano (p),
      sustain (p.apvts, "sustain"), strength (p.apvts, "amount"), speed (p.apvts, "speed", true), formant (p.apvts, "formant"), shift (p.apvts, "shift"), fine (p.apvts, "fine")
{
    setLookAndFeel (&lnf);
    tips.setLookAndFeel (&lnf);
    menuBtn.circle = true; menuBtn.icon = 1; menuBtn.setTooltip (U ("Menú: MIDI, tonalidad, acerca de"));
    menuBtn.onClick = [this] { mainMenu(); };
    presetBtn.setTooltip ("Presets");
    presetBtn.onClick = [this] { presetMenu(); };
    undoBtn.circle = redoBtn.circle = true; undoBtn.icon = 2; redoBtn.icon = 3;
    undoBtn.setTooltip ("Deshacer"); redoBtn.setTooltip ("Rehacer");
    undoBtn.onClick = [this] { proc.undoManager.undo(); };
    redoBtn.onClick = [this] { proc.undoManager.redo(); };
    themeSw.sun = true; themeSw.setToggleState (proc.lightTheme, juce::dontSendNotification);
    themeSw.setTooltip ("Tema negro / plata");
    themeSw.onClick = [this] { proc.lightTheme = themeSw.getToggleState(); applyTheme(); };
    engineSw.setTooltip ("Encender / apagar la afinación");
    engineAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "engine", engineSw);
    linkBtn.setClickingTogglesState (true);
    linkBtn.setTooltip (U ("LINK: el formant acompaña al SHIFT (transposición con carácter)"));
    linkAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "link", linkBtn);

    sustain.setTooltip (U ("SUSTAIN: las notas cortas quedan naturales; solo las notas sostenidas se afinan por completo"));
    strength.setTooltip (U ("STRENGTH: cuánto se corrige (100% = afinación total)"));
    speed.setTooltip (U ("SPEED: 0 ms = efecto robótico; 20-50 ms = natural"));
    formant.setTooltip (U ("FORMANT: color de la voz (más grave / más aguda) sin cambiar la nota"));
    shift.setTooltip ("SHIFT: transponer en semitonos");
    fine.setTooltip ("FINE: ajuste fino en cents");
    for (auto* k : { &formant, &shift, &fine }) k->getProperties().set ("bipolar", true);
    for (auto* k : { &sustain, &strength, &speed, &formant, &shift, &fine }) addAndMakeVisible (k);
    for (auto* s : { &mix, &out })
    {
        s->setSliderStyle (juce::Slider::LinearHorizontal);
        s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s->setPopupDisplayEnabled (true, true, nullptr);
        addAndMakeVisible (s);
    }
    out.getProperties().set ("bipolar", true);
    sAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "mix", mix));
    sAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "out", out));
    mix.setDoubleClickReturnValue (true, 100.0); out.setDoubleClickReturnValue (true, 0.0);
    mix.onValueChange = [this] { repaint(); }; out.onValueChange = [this] { repaint(); };
    mix.setTooltip ("MIX: voz afinada / voz original"); out.setTooltip ("OUTPUT: volumen de salida");

    styleBox.addItemList ({ "Natural", "Expresivo", "Sostenido" }, 1);
    styleAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "style", styleBox);
    styleBox.setTooltip (U ("Natural: conserva el vibrato · Expresivo: corrige cada movimiento · Sostenido: deja libres los ataques"));

    const juce::StringArray modeNames { "REALTIME", "LOW LAT", "HIGH Q" };
    for (int i = 0; i < 3; ++i)
    {
        auto* b = modes.add (new ttui::Pill (modeNames[i]));
        b->segment = true;
        b->onClick = [this, i] { modeAtt->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }
    modes[0]->setTooltip (U ("REALTIME: ~7 ms, para grabar o cantar en vivo (sin formant)"));
    modes[1]->setTooltip (U ("LOW LAT: ~29 ms, conserva formantes"));
    modes[2]->setTooltip (U ("HIGH Q: máxima calidad para mezcla (~43 ms, compensado por el DAW)"));
    modeAtt = std::make_unique<juce::ParameterAttachment> (*proc.apvts.getParameter ("quality"), [this] (float v)
    {
        mode = (int) std::round (v);
        for (int i = 0; i < modes.size(); ++i) modes[i]->setToggleState (i == mode, juce::dontSendNotification);
        formant.setEnabled (mode != 0); linkBtn.setEnabled (mode != 0);
        repaint();
    }, nullptr);
    modeAtt->sendInitialUpdate();

    for (juce::Component* c : { (juce::Component*) &menuBtn, (juce::Component*) &presetBtn, (juce::Component*) &undoBtn, (juce::Component*) &redoBtn,
                                (juce::Component*) &themeSw, (juce::Component*) &engineSw, (juce::Component*) &linkBtn, (juce::Component*) &keyPanel,
                                (juce::Component*) &piano, (juce::Component*) &styleBox })
        addAndMakeVisible (c);

    presetBtn.setButtonText (proc.getProgramName (proc.getCurrentProgram()));
    applyTheme();
    setResizable (true, true);
    setResizeLimits (W * 2 / 3, H * 2 / 3, W * 3 / 2, H * 3 / 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) W / H);
    setSize (W, H);
    startTimerHz (30);
}

TopTuneEditor::~TopTuneEditor()
{
    stopTimer();
    tips.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void TopTuneEditor::applyTheme()
{
    const bool l = proc.lightTheme;
    lnf.light = l;
    for (auto* b : { &menuBtn, &presetBtn, &undoBtn, &redoBtn, &linkBtn }) b->lightTheme = l;
    themeSw.lightTheme = engineSw.lightTheme = l;
    keyPanel.lightTheme = l;
    repaint();
    for (auto* c : getChildren()) c->repaint();
}

void TopTuneEditor::mainMenu()
{
    juce::PopupMenu m, midi, key, scale;
    const int md = (int) proc.apvts.getRawParameterValue ("midi")->load();
    midi.addItem (100, "MIDI apagado", true, md == 0);
    midi.addItem (101, U ("MIDI Repitch (la voz sigue el teclado)"), true, md == 1);
    midi.addItem (102, U ("MIDI Escala (toca las notas de la escala)"), true, md == 2);
    const int k = (int) proc.apvts.getRawParameterValue ("key")->load(), sc = (int) proc.apvts.getRawParameterValue ("scale")->load();
    for (int i = 0; i < 12; ++i) key.addItem (200 + i, TopTuneProcessor::keyNames()[i], true, i == k);
    for (int i = 0; i < TopTuneProcessor::scaleNames().size(); ++i) scale.addItem (300 + i, TopTuneProcessor::scaleNames()[i], true, i == sc);
    m.addSubMenu ("Tonalidad", key);
    m.addSubMenu ("Escala", scale);
    m.addItem (1, proc.meters.keyListening.load() ? U ("Detener la detección de tonalidad") : U ("Detectar la tonalidad (AUTO)"));
    m.addSeparator();
    m.addSubMenu ("MIDI", midi);
    m.addSeparator();
    m.addItem (2, "Acerca de Top Tune");
    juce::Component::SafePointer<TopTuneEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&menuBtn), [safe] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& pr = safe->proc;
        auto setC = [&pr] (const char* id, int v) { if (auto* prm = pr.apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 ((float) v)); };
        if (r >= 300) setC ("scale", r - 300);
        else if (r >= 200) setC ("key", r - 200);
        else if (r >= 100) setC ("midi", r - 100);
        else if (r == 1) { if (pr.meters.keyListening.load()) pr.stopKeyDetection(); else pr.startKeyDetection(); }
        else if (r == 2) juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Top Tune",
            U ("Afinador de voz con 3 motores:\nREALTIME (~7 ms) para grabar, LOW LAT (~29 ms) y HIGH Q (máxima calidad).\n\nToca las teclas del piano para armar tu escala.\nDiseño propio de Dj Pava Hm Music."));
    });
}

void TopTuneEditor::presetMenu()
{
    juce::PopupMenu m;
    for (int i = 0; i < proc.getNumPrograms(); ++i) m.addItem (i + 1, proc.getProgramName (i), true, i == proc.getCurrentProgram());
    juce::Component::SafePointer<TopTuneEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetBtn), [safe] (int r) { if (safe != nullptr && r > 0) safe->proc.setCurrentProgram (r - 1); });
}

void TopTuneEditor::resized()
{
    const float s = (float) getWidth() / (float) W;
    const auto T = juce::AffineTransform::scale (s);
    auto put = [&] (juce::Component& c, int x, int y, int w, int h) { c.setBounds (x, y, w, h); c.setTransform (T); };
    put (menuBtn, 16, 12, 34, 34);
    put (presetBtn, 466, 9, 168, 38);
    put (themeSw, 930, 18, 66, 22);
    put (undoBtn, 1010, 12, 34, 34);
    put (redoBtn, 1052, 12, 34, 34);
    put (keyPanel, 8, 60, 270, 270);
    put (piano, 286, 66, 800, 256);
    put (sustain, 147 - 46, 446 - 46, 92, 92);
    put (strength, 294 - 52, 446 - 52, 104, 104);
    put (speed, 550 - 76, 434 - 76, 152, 152);
    put (formant, 781 - 46, 412 - 46, 92, 92);
    put (shift, 990 - 46, 412 - 46, 92, 92);
    put (fine, 885 - 30, 452 - 30, 60, 60);
    put (linkBtn, 860, 392, 50, 24);
    put (engineSw, 920, 352, 40, 20);
    put (mix, 776, 498, 220, 26);
    put (out, 776, 532, 220, 26);
    put (styleBox, 16, 584, 196, 28);
    for (int i = 0; i < 3; ++i) put (*modes[i], 874 + i * 72, 584, 72, 28);
}

void TopTuneEditor::paint (juce::Graphics& g)
{
    const auto& P = palette (proc.lightTheme);
    g.addTransform (juce::AffineTransform::scale ((float) getWidth() / (float) W));
    const float fw = (float) W, fh = (float) H;
    g.setGradientFill (juce::ColourGradient (P.bg0, 0, 0, P.bg1, 0, fh, false));
    g.fillAll();
    // textura cepillada sutil
    juce::Random rng (21);
    for (float y = 0; y < 572; y += 1.0f)
    {
        g.setColour ((rng.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (P.light ? 0.025f : 0.018f));
        g.drawHorizontalLine ((int) y, 0, fw);
    }
    // cabecera
    g.setGradientFill (juce::ColourGradient (P.light ? juce::Colour (0xffeff0f2) : juce::Colour (0xff222327), 0, 0, P.light ? juce::Colour (0xffd9dbde) : juce::Colour (0xff17181b), 0, 58, false));
    g.fillRect (0.0f, 0.0f, fw, 58.0f);
    g.setColour (P.line); g.drawHorizontalLine (58, 0, fw);
    g.setFont (font (25.0f, true));
    g.setColour (P.text); g.drawText ("Top", 62, 14, 60, 30, juce::Justification::centredLeft);
    const float tw = font (25.0f, true).getStringWidthFloat ("Top");
    g.setGradientFill (juce::ColourGradient (P.light ? juce::Colour (0xff5a5e65) : juce::Colour (0xffffffff), 62 + tw, 14, P.light ? juce::Colour (0xff0e0f11) : juce::Colour (0xff8f949b), 62 + tw, 44, false));
    g.drawText ("Tune", (int) (62 + tw + 2), 14, 80, 30, juce::Justification::centredLeft);
    // separadores
    g.setColour (P.line); g.drawHorizontalLine (334, 16, fw - 16);
    g.setColour (P.light ? juce::Colours::white.withAlpha (0.6f) : juce::Colours::white.withAlpha (0.04f)); g.drawHorizontalLine (335, 16, fw - 16);
    for (float x : { 421.0f, 687.0f }) { g.setColour (P.line); g.drawVerticalLine ((int) x, 372, 560); }
    auto label = [&] (const juce::String& t, float cx, float cy, float size = 12.5f)
    {
        g.setColour (P.dim); g.setFont (spaced (size, 0.2f));
        g.drawText (t, juce::Rectangle<float> (cx - 120, cy - 9, 240, 18), juce::Justification::centred);
    };
    label (U ("CORRECCIÓN"), 221, 355, 13.0f);
    label ("SUSTAIN", 147, 506); label ("STRENGTH", 294, 512); label ("SPEED", 550, 524);
    label ("FORMANT", 781, 466); label ("SHIFT", 990, 466); label ("FINE", 885, 486, 10.0f);
    g.setColour (P.dim); g.setFont (spaced (13.0f, 0.2f));
    g.drawText ("ENGINE", 820, 352, 92, 20, juce::Justification::centredRight);
    g.setFont (spaced (11.5f, 0.15f));
    g.drawText ("MIX", 716, 501, 54, 20, juce::Justification::centredRight);
    g.drawText ("OUTPUT", 706, 535, 64, 20, juce::Justification::centredRight);
    g.setColour (P.text); g.setFont (font (14.0f, true));
    g.drawText (juce::String ((int) std::round (mix.getValue())) + "%", 1004, 501, 70, 20, juce::Justification::centredLeft);
    g.drawText ((out.getValue() > 0.05 ? "+" : "") + juce::String (out.getValue(), 1) + " dB", 1004, 535, 80, 20, juce::Justification::centredLeft);
    if (mode == 0) { g.setColour (P.dim.withAlpha (0.8f)); g.setFont (font (10.0f, true)); g.drawText (U ("formant: solo LOW LAT / HIGH Q"), 716, 476, 140, 12, juce::Justification::centredLeft); }
    // pie negro
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff111214), 0, 572, juce::Colour (0xff050506), 0, fh, false));
    g.fillRect (0.0f, 572.0f, fw, fh - 572.0f);
    g.setColour (juce::Colours::black); g.drawHorizontalLine (572, 0, fw);
    // segmento de motores
    {
        const auto seg = juce::Rectangle<float> (868, 580, 228, 36);
        g.setColour (juce::Colour (0xff0b0c0d)); g.fillRoundedRectangle (seg, 9);
        g.setColour (juce::Colour (0xff2a2c30)); g.drawRoundedRectangle (seg, 9, 1.0f);
    }
    // logo propio: ondas + T
    {
        const float cx = 550, cy = 597;
        const float hs[] { 6, 12, 18, 10, 4 };
        for (int i = 0; i < 5; ++i)
        {
            const float h = hs[i];
            g.setColour (juce::Colour (0xffd9dde3).withAlpha (0.85f));
            g.fillRoundedRectangle (cx - 40 + (float) i * 6.0f, cy - h * 0.5f, 2.6f, h, 1.3f);
            g.fillRoundedRectangle (cx + 15 + (float) (4 - i) * 6.0f, cy - hs[4 - i] * 0.5f, 2.6f, hs[4 - i], 1.3f);
        }
        g.setGradientFill (juce::ColourGradient (juce::Colours::white, cx, cy - 12, juce::Colour (0xff9aa0a8), cx, cy + 12, false));
        g.fillRoundedRectangle (cx - 9, cy - 12, 18, 3.6f, 1.5f);
        g.fillRoundedRectangle (cx - 1.8f, cy - 12, 3.6f, 24, 1.5f);
    }
    g.setColour (juce::Colour (0xff6d7178)); g.setFont (font (10.5f));
    g.drawText ("by Dj Pava Hm Music", 230, 590, 200, 16, juce::Justification::centredLeft);
}

void TopTuneEditor::paintOverChildren (juce::Graphics&) {}

void TopTuneEditor::timerCallback()
{
    keyPanel.tick();
    piano.tick();
    const auto name = proc.getProgramName (proc.getCurrentProgram());
    if (presetBtn.getButtonText() != name) presetBtn.setButtonText (name);
    if (++transactionTick >= 15)
    {
        transactionTick = 0;
        if (! juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown()) proc.undoManager.beginNewTransaction();
    }
}
