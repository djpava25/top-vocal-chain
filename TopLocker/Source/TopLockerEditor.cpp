#include "TopLockerEditor.h"

using namespace tlui;
using juce::Colour; namespace Colours = juce::Colours; using juce::Graphics; using juce::Path; using juce::Rectangle; using juce::ColourGradient;

namespace
{
    juce::String U (const char* s) { return juce::String::fromUTF8 (s); }
    juce::Font font (float h, bool bold = false, bool italic = false)
    {
        return juce::Font (juce::FontOptions (h, (bold ? juce::Font::bold : 0) | (italic ? juce::Font::italic : 0)));
    }
    float textW (const juce::Font& f, const juce::String& t) { return juce::GlyphArrangement::getStringWidth (f, t); }
    constexpr float A0 = juce::MathConstants<float>::pi * 1.25f, A1 = juce::MathConstants<float>::pi * 2.75f;
    const Colour ink (0xffe8e8ea), dim (0xffa9a9ae), blue (0xff2f8cff), blueHi (0xff7cc0ff), panelTop (0xff2e2d2b), panelBot (0xff1a1918);

    void setParam (TopLockerProcessor& p, const char* id, float v)
    {
        if (auto* q = p.apvts.getParameter (id)) { q->beginChangeGesture(); q->setValueNotifyingHost (q->convertTo0to1 (v)); q->endChangeGesture(); }
    }
    float param (TopLockerProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); }

    void screw (Graphics& g, float x, float y, float r = 4.0f)
    {
        g.setGradientFill (ColourGradient (Colour (0xff8a8a8e), x - r, y - r, Colour (0xff2a2a2c), x + r, y + r, false));
        g.fillEllipse (x - r, y - r, r * 2, r * 2);
        g.setColour (Colours::black.withAlpha (0.7f)); g.drawEllipse (x - r, y - r, r * 2, r * 2, 0.8f);
        g.drawLine (x - r * 0.7f, y + r * 0.2f, x + r * 0.7f, y - r * 0.2f, 1.1f);
    }
    // panel de metal oscuro con tornillos
    void panel (Graphics& g, Rectangle<float> r, bool screws = true)
    {
        g.setColour (Colours::black.withAlpha (0.6f)); g.fillRoundedRectangle (r.translated (0, 2), 7);
        g.setGradientFill (ColourGradient (panelTop, r.getX(), r.getY(), panelBot, r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 7);
        // cepillado
        juce::Random rnd (int (r.getX() * 7 + r.getY()));
        for (float y = r.getY() + 2; y < r.getBottom() - 2; y += 1.5f)
        {
            g.setColour (Colours::white.withAlpha (0.012f + 0.02f * rnd.nextFloat()));
            g.drawHorizontalLine ((int) y, r.getX() + 3, r.getRight() - 3);
        }
        g.setColour (Colours::white.withAlpha (0.09f)); g.drawRoundedRectangle (r.reduced (1.0f), 7, 1.0f);
        g.setColour (Colours::black.withAlpha (0.9f)); g.drawRoundedRectangle (r, 7, 1.2f);
        if (screws) { screw (g, r.getX() + 9, r.getY() + 9); screw (g, r.getRight() - 9, r.getY() + 9); screw (g, r.getX() + 9, r.getBottom() - 9); screw (g, r.getRight() - 9, r.getBottom() - 9); }
    }
    void inset (Graphics& g, Rectangle<float> r, float rad = 4)
    {
        g.setGradientFill (ColourGradient (Colour (0xff0b0b0c), r.getX(), r.getY(), Colour (0xff1c1c1e), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, rad);
        g.setColour (Colours::black); g.drawRoundedRectangle (r, rad, 1.2f);
        g.setColour (Colours::white.withAlpha (0.08f)); g.drawLine (r.getX() + rad, r.getBottom() + 0.5f, r.getRight() - rad, r.getBottom() + 0.5f, 1.0f);
    }
    void title (Graphics& g, const juce::String& t, Rectangle<float> r, float size = 17.0f)
    {
        g.setFont (font (size, false)); g.setColour (Colours::black.withAlpha (0.8f)); g.drawText (t, r.translated (0, 1.5f), juce::Justification::centred);
        g.setColour (ink); g.drawText (t, r, juce::Justification::centred);
    }
    void chevron (Graphics& g, Rectangle<float> r, bool left, Colour c, float th = 2.4f)
    {
        Path p; const float cx = r.getCentreX(), cy = r.getCentreY(), s = std::min (r.getWidth(), r.getHeight()) * 0.22f;
        if (left) { p.startNewSubPath (cx + s * 0.5f, cy - s); p.lineTo (cx - s * 0.5f, cy); p.lineTo (cx + s * 0.5f, cy + s); }
        else { p.startNewSubPath (cx - s * 0.5f, cy - s); p.lineTo (cx + s * 0.5f, cy); p.lineTo (cx - s * 0.5f, cy + s); }
        g.setColour (c); g.strokePath (p, juce::PathStrokeType (th, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    // texto plateado con relieve
    void silverText (Graphics& g, const juce::String& t, juce::Font f, float x, float baseline, Colour top = Colour (0xfff4f4f6), Colour bot = Colour (0xff8d8f95))
    {
        juce::GlyphArrangement ga; ga.addLineOfText (f, t, x, baseline);
        Path p; ga.createPath (p);
        const auto b = p.getBounds();
        g.setColour (Colours::black.withAlpha (0.8f)); g.fillPath (p, juce::AffineTransform::translation (1.5f, 2.0f));
        ColourGradient cg (top, 0, b.getY(), bot, 0, b.getBottom(), false); cg.addColour (0.55, Colour (0xffc9cacf)); cg.addColour (0.6, Colour (0xff9a9ca2));
        g.setGradientFill (cg); g.fillPath (p);
    }
    void waveLogo (Graphics& g, float x, float y, float w, float h, Colour c)
    {
        Path p; p.startNewSubPath (x, y);
        const float pts[] { 0, 0, 0.18f, 0, 0.24f, -0.3f, 0.3f, 0.5f, 0.36f, -1.0f, 0.43f, 0.9f, 0.5f, -0.6f, 0.56f, 0.35f, 0.62f, -0.15f, 0.7f, 0, 1.0f, 0 };
        for (int i = 2; i < 22; i += 2) p.lineTo (x + pts[i] * w, y + pts[i + 1] * h * 0.5f);
        g.setColour (c); g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    void brandLogo (Graphics& g, float x, float y, float scale)
    {
        waveLogo (g, x, y + 22 * scale, 70 * scale, 34 * scale, Colour (0xffd8d8dc));
        auto f = font (30 * scale, true, true);
        silverText (g, "Dj Pava", f, x + 66 * scale, y + 30 * scale);
        juce::GlyphArrangement ga; ga.addLineOfText (f, "Dj Pava ", 0, 0);
        const float w = ga.getBoundingBox (0, -1, true).getWidth();
        silverText (g, "HM", f, x + 66 * scale + w, y + 30 * scale, Colour (0xff9fd2ff), Colour (0xff2468d8));
        g.setFont (font (11 * scale)); g.setColour (dim);
        g.drawText (U ("M U S I C   ·   P R O   A U D I O   T O O L S"), Rectangle<float> (x + 66 * scale, y + 34 * scale, 260 * scale, 14 * scale), juce::Justification::left);
    }

}

// ---------------- Look ----------------
Look::Look()
{
    setColour (juce::PopupMenu::backgroundColourId, Colour (0xff1c1c1e));
    setColour (juce::PopupMenu::textColourId, ink);
    setColour (juce::PopupMenu::headerTextColourId, blueHi);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colour (0xff1f4f8f));
    setColour (juce::TooltipWindow::backgroundColourId, Colour (0xff17171a));
    setColour (juce::TooltipWindow::textColourId, ink);
    setColour (juce::TooltipWindow::outlineColourId, blue.withAlpha (0.6f));
}
void Look::drawBubble (Graphics& g, juce::BubbleComponent&, const juce::Point<float>&, const Rectangle<float>& body)
{
    g.setColour (Colour (0xf0141416)); g.fillRoundedRectangle (body, 4);
    g.setColour (blue.withAlpha (0.7f)); g.drawRoundedRectangle (body.reduced (0.5f), 4, 1.0f);
}
void Look::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const juce::Point<float> c ((float) x + (float) w * 0.5f, (float) y + (float) h * 0.5f);
    const float R = (float) std::min (w, h) * 0.5f - 9.0f;
    const float a = a0 + pos * (a1 - a0);
    auto pt = [&c] (float ang, float rad) { return juce::Point<float> (c.x + rad * std::sin (ang), c.y - rad * std::cos (ang)); };
    // puntos de escala
    for (int i = 0; i <= 10; ++i)
    {
        const auto p = pt (a0 + (a1 - a0) * (float) i / 10.0f, R + 6.0f);
        g.setColour (Colour (0xffd5d5d8).withAlpha (i == 0 || i == 10 || i == 5 ? 0.95f : 0.6f));
        const float d = i == 5 ? 2.6f : 2.0f; g.fillEllipse (p.x - d * 0.5f, p.y - d * 0.5f, d, d);
    }
    // arco azul
    if ((bool) s.getProperties()["arc"])
    {
        const bool bip = (bool) s.getProperties()["bipolar"];
        const float st = bip ? (a0 + a1) * 0.5f : a0;
        Path arc; arc.addCentredArc (c.x, c.y, R + 2.0f, R + 2.0f, 0, std::min (st, a), std::max (st, a), true);
        g.setColour (blue.withAlpha (0.25f)); g.strokePath (arc, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (blue); g.strokePath (arc, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    // sombra
    g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.7f), c.x, c.y + R * 0.25f, Colours::transparentBlack, c.x + R * 1.25f, c.y + R * 0.25f, true));
    g.fillEllipse (c.x - R * 1.25f, c.y - R * 1.0f, R * 2.5f, R * 2.5f);
    // bisel negro
    g.setGradientFill (ColourGradient (Colour (0xff4a4a4e), c.x, c.y - R, Colour (0xff050505), c.x, c.y + R, false));
    g.fillEllipse (c.x - R, c.y - R, R * 2, R * 2);
    // tapa cromada (moleteada)
    const float rc = R * 0.86f;
    ColourGradient cg (Colour (0xfff7f7f9), c.x - rc * 0.45f, c.y - rc * 0.75f, Colour (0xff55575c), c.x + rc * 0.6f, c.y + rc * 0.9f, true);
    cg.addColour (0.45, Colour (0xffbfc1c6)); cg.addColour (0.8, Colour (0xff7a7c82));
    g.setGradientFill (cg); g.fillEllipse (c.x - rc, c.y - rc, rc * 2, rc * 2);
    for (int i = 0; i < 48; ++i)
    {
        const float t = (float) i / 48.0f * juce::MathConstants<float>::twoPi;
        g.setColour ((i % 2 ? Colours::black : Colours::white).withAlpha (0.1f));
        g.drawLine (juce::Line<float> (pt (t, rc * 0.93f), pt (t, rc)), 1.0f);
    }
    // reflejos conicos
    for (int k = 0; k < 2; ++k)
    {
        Path sh; const float b = k == 0 ? -0.75f : 2.4f;
        sh.addPieSegment (c.x - rc * 0.85f, c.y - rc * 0.85f, rc * 1.7f, rc * 1.7f, b, b + 0.5f, 0.0f);
        g.setColour (Colours::white.withAlpha (k == 0 ? 0.35f : 0.18f)); g.fillPath (sh);
    }
    const float rt = rc * 0.62f;
    g.setGradientFill (ColourGradient (Colour (0xffe9eaed), c.x, c.y - rt, Colour (0xff8a8c92), c.x, c.y + rt, false));
    g.fillEllipse (c.x - rt, c.y - rt, rt * 2, rt * 2);
    g.setColour (Colours::black.withAlpha (0.35f)); g.drawEllipse (c.x - rt, c.y - rt, rt * 2, rt * 2, 0.8f);
    g.setColour (Colours::black.withAlpha (0.6f)); g.drawEllipse (c.x - rc, c.y - rc, rc * 2, rc * 2, 1.0f);
    // indicador azul
    g.setColour (blue.withAlpha (0.35f)); g.drawLine (juce::Line<float> (pt (a, rc * 0.25f), pt (a, rc * 0.96f)), 5.0f);
    g.setColour (blueHi); g.drawLine (juce::Line<float> (pt (a, rc * 0.25f), pt (a, rc * 0.96f)), 2.4f);
}

// ---------------- Dial ----------------
Dial::Dial (APVTS& s, const juce::String& id, const juce::String& l, const juce::String& h, std::function<juce::String (float)> f, float d, bool arc, int li, int hi_)
    : lo (l), hi (h), fmt (std::move (f)), dia (d), loIcon (li), hiIcon (hi_)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (A0, A1, true);
    slider.setMouseDragSensitivity (220);
    slider.getProperties().set ("arc", arc);
    if (auto* p = s.getParameter (id)) slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    att = std::make_unique<APVTS::SliderAttachment> (s, id, slider);
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
}
void Dial::resized() { slider.setBounds (juce::roundToInt ((getWidth() - dia - 20) * 0.5f), 0, juce::roundToInt (dia + 20), juce::roundToInt (dia + 20)); }
void Dial::paint (Graphics& g)
{
    const float w = (float) getWidth(), k = dia + 20, x0 = (w - k) * 0.5f;
    auto icon = [&g] (int type, float cx, float cy)
    {
        g.setColour (Colour (0xffcfcfd2));
        if (type == 1) { g.fillEllipse (cx - 3.5f, cy - 9, 7, 7); g.fillRoundedRectangle (cx - 6, cy - 1, 12, 8, 3); }
        if (type == 2) { g.fillRoundedRectangle (cx - 3.5f, cy - 10, 7, 11, 3.5f); g.drawLine (cx, cy + 1, cx, cy + 6, 1.4f); g.drawLine (cx - 4, cy + 6, cx + 4, cy + 6, 1.4f); }
    };
    const float ly = k * 0.82f;
    if (loIcon) icon (loIcon, x0 + 4, ly - 10);
    if (hiIcon) icon (hiIcon, x0 + k - 4, ly - 10);
    g.setFont (font (compact ? 11.0f : 13.0f)); g.setColour (Colour (0xffd8d8db));
    if (lo.isNotEmpty()) g.drawText (lo, Rectangle<float> (x0 - 30, ly + (loIcon ? 0 : -6), 68, 16), juce::Justification::centred);
    if (hi.isNotEmpty()) g.drawText (hi, Rectangle<float> (x0 + k - 38, ly + (hiIcon ? 0 : -6), 68, 16), juce::Justification::centred);
    if (showValue && fmt)
    {
        const float vw = compact ? 60.0f : 84.0f, vh = compact ? 19.0f : 26.0f;
        Rectangle<float> vb (w * 0.5f - vw * 0.5f, k + (compact ? 0.0f : 4.0f), vw, vh);
        inset (g, vb);
        g.setFont (font (compact ? 12.5f : 16.0f)); g.setColour (ink); g.drawText (fmt ((float) slider.getValue()), vb, juce::Justification::centred);
    }
}
// ---------------- Box / Btn ----------------
void Box::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1);
    inset (g, r, 5);
    g.setColour (Colours::white.withAlpha (0.1f)); g.drawRoundedRectangle (r, 5, 1.0f);
    auto t = r.reduced (10, 0);
    if (arrow) { chevron (g, t.removeFromRight (16).withSizeKeepingCentre (16, 20), false, ink, 2.0f); }
    g.setFont (font (fontSize));
    if (label.isNotEmpty())
    {
        g.setColour (dim); const float lw = textW (font (fontSize), label) + 10;
        g.drawText (label, t.removeFromLeft (lw), juce::Justification::centredLeft);
    }
    g.setColour (ink); g.drawFittedText (text, t.toNearestInt(), centred ? juce::Justification::centred : juce::Justification::centredLeft, 1, 0.8f);
}
void Btn::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1);
    const bool over = isMouseOver();
    g.setGradientFill (ColourGradient (Colour (over ? 0xff3a3a3e : 0xff2c2c30), 0, r.getY(), Colour (0xff121214), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, 5);
    g.setColour (Colours::white.withAlpha (0.12f)); g.drawRoundedRectangle (r.reduced (1), 5, 1.0f);
    g.setColour (Colours::black); g.drawRoundedRectangle (r, 5, 1.2f);
    g.setColour (ink);
    switch (kind)
    {
        case Text: g.setFont (font (16)); g.drawText (text, r, juce::Justification::centred); break;
        case Prev: chevron (g, r, true, ink); break;
        case Next: chevron (g, r, false, ink); break;
        case Gear:
        {
            const auto c = r.getCentre(); const float R = std::min (r.getWidth(), r.getHeight()) * 0.3f;
            Path p;
            for (int i = 0; i < 16; ++i)
            {
                const float a = (float) i / 16.0f * juce::MathConstants<float>::twoPi, rr = (i % 2) ? R : R * 0.75f;
                const float a2 = a + juce::MathConstants<float>::twoPi / 16.0f;
                if (i == 0) p.startNewSubPath (c.x + rr * std::sin (a), c.y - rr * std::cos (a)); else p.lineTo (c.x + rr * std::sin (a), c.y - rr * std::cos (a));
                p.lineTo (c.x + rr * std::sin (a2), c.y - rr * std::cos (a2));
            }
            p.closeSubPath(); p.addEllipse (c.x - R * 0.32f, c.y - R * 0.32f, R * 0.64f, R * 0.64f); p.setUsingNonZeroWinding (false);
            g.fillPath (p); break;
        }
        case Info:
        {
            const auto c = r.getCentre(); const float R = std::min (r.getWidth(), r.getHeight()) * 0.32f;
            g.drawEllipse (c.x - R, c.y - R, R * 2, R * 2, 1.6f);
            g.setFont (font (R * 1.4f, true)); g.drawText ("i", Rectangle<float> (c.x - R, c.y - R, R * 2, R * 2), juce::Justification::centred);
            break;
        }
    }
}

// ---------------- Categorias ----------------
const char* Categories::name (int i) { static const char* n[] { "Studio Classics", "Vintage", "Modern", "Ribbon", "Dynamic", "Broadcast", "All Models" }; return n[juce::jlimit (0, 6, i)]; }
unsigned Categories::mask (int i) { static const unsigned m[] { lk::Classic, lk::Vintage, lk::Modern, lk::Ribbon, lk::Dynamic, lk::Broadcast, 0xffffffffu }; return m[juce::jlimit (0, 6, i)]; }
void Categories::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setGradientFill (ColourGradient (Colour (0xff222224), 0, 0, Colour (0xff141415), 0, r.getBottom(), false)); g.fillRoundedRectangle (r, 6);
    g.setColour (Colours::black); g.drawRoundedRectangle (r, 6, 1.2f);
    auto hdr = Rectangle<float> (8, 8, r.getWidth() - 16, 34);
    inset (g, hdr, 4); g.setFont (font (17)); g.setColour (ink); g.drawText ("MIC MODELS", hdr, juce::Justification::centred);
    for (int i = 0; i < 7; ++i)
    {
        Rectangle<float> it (8, 50 + (float) i * 37.0f, r.getWidth() - 16, 33);
        if (i == current)
        {
            g.setColour (blue.withAlpha (0.25f)); g.fillRoundedRectangle (it.expanded (2), 5);
            g.setGradientFill (ColourGradient (Colour (0xff1d5fbf), 0, it.getY(), Colour (0xff0e3570), 0, it.getBottom(), false)); g.fillRoundedRectangle (it, 4);
            g.setColour (blueHi); g.drawRoundedRectangle (it, 4, 1.3f);
        }
        else { g.setColour (Colours::white.withAlpha (0.05f)); g.drawLine (it.getX() + 6, it.getBottom() + 2, it.getRight() - 6, it.getBottom() + 2, 1.0f); }
        // icono
        const float ix = it.getX() + 18, iy = it.getCentreY();
        g.setColour (i == current ? Colours::white : Colour (0xffbdbdc2));
        if (i == 6) { for (int a = 0; a < 3; ++a) for (int b = 0; b < 3; ++b) g.fillRect (ix - 7 + a * 5.5f, iy - 7 + b * 5.5f, 3.6f, 3.6f); }
        else
        {
            g.fillRoundedRectangle (ix - 4.5f, iy - 11, 9, 13, 4.5f);
            g.setColour (Colours::black.withAlpha (0.4f)); for (int k = 0; k < 4; ++k) g.drawHorizontalLine ((int) (iy - 8 + k * 2.6f), ix - 4, ix + 4);
            g.setColour (i == current ? Colours::white : Colour (0xffbdbdc2)); g.fillRect (ix - 2.5f, iy + 2, 5.0f, 6.0f); g.fillRect (ix - 0.8f, iy + 8, 1.6f, 4.0f);
        }
        g.setFont (font (16)); g.setColour (i == current ? Colours::white : Colour (0xffd2d2d6));
        g.drawText (name (i), it.withTrimmedLeft (38), juce::Justification::centredLeft);
    }
}
void Categories::mouseUp (const juce::MouseEvent& e)
{
    const int i = (int) ((e.position.y - 50) / 37.0f);
    if (e.position.y >= 50 && i >= 0 && i < 7) { current = i; repaint(); if (onPick) onPick (i); }
}

// ---------------- Carrusel ----------------
std::vector<int> Carousel::list() const
{
    std::vector<int> v; const unsigned m = Categories::mask (category);
    for (int i = 0; i < (int) lk::mics().size(); ++i) if (lk::mics()[(size_t) i].cats & m) v.push_back (i);
    return v;
}
void Carousel::setCategory (int c) { category = c; first = 0; ensureVisible(); repaint(); }
void Carousel::ensureVisible()
{
    const auto v = list(); const int sel = (int) param (proc, "mic");
    const auto it = std::find (v.begin(), v.end(), sel);
    if (it != v.end())
    {
        const int pos = (int) (it - v.begin());
        if (pos < first) first = pos;
        if (pos >= first + visible) first = pos - visible + 1;
    }
    first = juce::jlimit (0, std::max (0, (int) v.size() - visible), first);
}
void Carousel::step (int dir)
{
    const auto v = list(); if (v.empty()) return;
    const int sel = (int) param (proc, "mic");
    auto it = std::find (v.begin(), v.end(), sel);
    int pos = it == v.end() ? 0 : (int) (it - v.begin()) + dir;
    pos = juce::jlimit (0, (int) v.size() - 1, pos);
    setParam (proc, "mic", (float) v[(size_t) pos]);
    ensureVisible(); repaint();
}
bool Carousel::animate()
{
    const int sel = (int) param (proc, "mic");
    if (sel != lastMic) { if (lastMic >= 0) glow = 0.0f; lastMic = sel; ensureVisible(); return true; }
    if (glow < 1.0f) { glow = std::min (1.0f, glow + 0.045f); return true; }
    return false;
}
Rectangle<float> Carousel::cardRect (int slot, bool sel) const
{
    const int n = std::min (visible, (int) list().size());
    const float cw = 110, gap = 6, total = (float) n * cw + (float) (n - 1) * gap, x0 = ((float) getWidth() - total) * 0.5f;
    Rectangle<float> r (x0 + (float) slot * (cw + gap), 16, cw, 282);
    return sel ? r.expanded (4, 0).withY (8).withHeight (296) : r;
}
int Carousel::slotAt (juce::Point<float> p) const
{
    const int n = std::min (visible, (int) list().size() - first);
    for (int s = 0; s < n; ++s) if (cardRect (s, false).expanded (3, 8).contains (p)) return s;
    return -1;
}
void Carousel::mouseUp (const juce::MouseEvent& e)
{
    const int s = slotAt (e.position); const auto v = list();
    if (s >= 0 && first + s < (int) v.size()) { setParam (proc, "mic", (float) v[(size_t) (first + s)]); repaint(); }
}
void Carousel::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const int n = (int) list().size();
    first = juce::jlimit (0, std::max (0, n - visible), first + (w.deltaY < 0 || w.deltaX < 0 ? 1 : -1));
    repaint();
}
juce::String Carousel::getTooltip()
{
    const int s = slotAt (getMouseXYRelative().toFloat()); const auto v = list();
    if (s < 0 || first + s >= (int) v.size()) return {};
    const auto& m = lk::mics()[(size_t) v[(size_t) (first + s)]];
    return juce::String (m.line1) + " " + m.line2 + "  (" + m.type + ", " + m.ref + ")\n" + U (m.desc);
}
void Carousel::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.saveState(); g.reduceClipRegion (getLocalBounds());
    const float floorY = r.getHeight() * 0.74f;
    g.setGradientFill (ColourGradient (Colour (0xff3a2415), 0, 0, Colour (0xff120a06), 0, floorY, false)); g.fillRect (r.withBottom (floorY));
    for (float x = 0; x < r.getWidth(); x += 13)
    {
        g.setColour (Colours::black.withAlpha (0.35f)); g.drawVerticalLine ((int) x, 40, floorY);
        g.setColour (Colour (0xffd08850).withAlpha (0.05f)); g.drawVerticalLine ((int) x + 2, 40, floorY);
    }
    g.setColour (Colour (0xff0d0805)); g.fillRect (0.0f, 0.0f, r.getWidth(), 40.0f);
    for (int i = 0; i < 8; ++i)
    {
        const float lx = r.getWidth() * (0.06f + 0.125f * (float) i);
        g.setGradientFill (ColourGradient (Colour (0xffffb35c).withAlpha (0.45f), lx, 40, Colours::transparentBlack, lx, 230, true));
        g.fillEllipse (lx - 90, -60, 180, 300);
        g.setColour (Colour (0xffffe0b0)); g.fillEllipse (lx - 9, 34, 18, 6);
    }
    g.setGradientFill (ColourGradient (Colour (0xff4a2e1c), 0, floorY, Colour (0xff0e0805), 0, r.getBottom(), false)); g.fillRect (r.withTop (floorY));
    g.setGradientFill (ColourGradient (Colour (0x55ffc080), r.getCentreX(), floorY, Colours::transparentBlack, r.getCentreX() + 450, floorY, true));
    g.fillRect (r.withTop (floorY).withHeight (6));

    const auto v = list(); const int sel = (int) param (proc, "mic");
    const int n = std::min (visible, (int) v.size() - first);
    const float gl = 1.0f - std::pow (1.0f - glow, 3.0f);     // suavizado
    for (int pass = 0; pass < 2; ++pass)
        for (int s = 0; s < n; ++s)
        {
            const int idx = v[(size_t) (first + s)];
            const bool isSel = idx == sel;
            if ((pass == 1) != isSel) continue;
            auto c = cardRect (s, isSel);
            if (isSel)
            {
                // foco de luz que se enciende al elegir el microfono
                Path beam; beam.startNewSubPath (c.getCentreX() - 14, 0); beam.lineTo (c.getCentreX() + 14, 0);
                beam.lineTo (c.getRight() + 10, c.getBottom() - 60); beam.lineTo (c.getX() - 10, c.getBottom() - 60); beam.closeSubPath();
                g.setGradientFill (ColourGradient (Colour (0xfffff1d8).withAlpha (0.32f * gl), c.getCentreX(), 0, Colours::transparentWhite, c.getCentreX(), c.getBottom() - 60, false));
                g.fillPath (beam);
                for (int k = 8; k > 0; --k) { g.setColour (blue.withAlpha (0.07f * gl)); g.drawRoundedRectangle (c.expanded ((float) k), 8 + (float) k, 2.0f); }
            }
            g.setGradientFill (ColourGradient (Colour (isSel ? 0x55142a48 : 0x40000000), 0, c.getY(), Colour (0xcc050505), 0, c.getBottom(), false));
            g.fillRoundedRectangle (c, 7);
            auto micArea = Rectangle<float> (c.getX() + 4, c.getY() + 8, c.getWidth() - 8, c.getHeight() - 96);
            const float fy = micArea.getBottom();
            // reflejo en el piso brillante
            g.saveState(); g.reduceClipRegion (c.toNearestInt());
            g.beginTransparencyLayer (0.22f);
            g.addTransform (juce::AffineTransform::scale (1.0f, -1.0f).translated (0, 2 * fy));
            drawMic (g, micArea, idx, false);
            g.endTransparencyLayer();
            g.restoreState();
            g.setGradientFill (ColourGradient (Colours::transparentBlack, 0, fy, Colour (0xf0080605), 0, fy + 46, false));
            g.fillRect (c.withTop (fy).withBottom (c.getBottom() - 72).reduced (1, 0));
            drawMic (g, micArea, idx, true);
            if (isSel && gl < 1.0f)   // destello al aparecer
            {
                g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.35f * (1.0f - gl)), micArea.getCentreX(), micArea.getCentreY(), Colours::transparentWhite, micArea.getRight(), micArea.getCentreY(), true));
                g.fillRect (micArea);
            }
            auto lab = c.removeFromBottom (74).reduced (4, 4);
            g.setGradientFill (ColourGradient (Colour (0xff1e1e20), 0, lab.getY(), Colour (0xff0b0b0c), 0, lab.getBottom(), false)); g.fillRoundedRectangle (lab, 4);
            g.setColour (Colours::white.withAlpha (0.08f)); g.drawRoundedRectangle (lab, 4, 1.0f);
            const auto& m = lk::mics()[(size_t) idx];
            g.setFont (font (isSel ? 17.0f : 15.5f, isSel)); g.setColour (isSel ? Colours::white : Colour (0xffd8d8dc));
            g.drawText (m.line1, Rectangle<float> (lab.getX(), lab.getY() + 4, lab.getWidth(), 19), juce::Justification::centred);
            g.drawText (m.line2, Rectangle<float> (lab.getX(), lab.getY() + 22, lab.getWidth(), 19), juce::Justification::centred);
            auto rb = Rectangle<float> (lab.getX() + 4, lab.getBottom() - 22, lab.getWidth() - 8, 17);
            g.setColour (blue.withAlpha (isSel ? 0.35f : 0.18f)); g.fillRoundedRectangle (rb, 3);
            g.setFont (font (11.5f, true)); g.setColour (isSel ? blueHi.brighter (0.3f) : Colour (0xff9cc6f2));
            g.drawFittedText (m.ref, rb.toNearestInt(), juce::Justification::centred, 1, 0.75f);
            if (isSel) { g.setColour (blueHi.withAlpha (0.4f + 0.6f * gl)); g.drawRoundedRectangle (cardRect (s, true), 7, 2.2f); }
            else { g.setColour (Colours::white.withAlpha (0.1f)); g.drawRoundedRectangle (cardRect (s, false), 7, 1.0f); }
        }
    g.setGradientFill (ColourGradient (Colours::transparentBlack, r.getCentreX(), r.getCentreY(), Colours::black.withAlpha (0.45f), 0, 0, true)); g.fillRect (r);
    g.restoreState();
    g.setColour (Colours::black); g.drawRoundedRectangle (r.reduced (0.5f), 6, 1.2f);
}

// ---------------- Visualizadores ----------------
namespace
{
    void screen (Graphics& g, Rectangle<float> r)
    {
        g.setGradientFill (ColourGradient (Colour (0xff0b1420), r.getX(), r.getY(), Colour (0xff04070c), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 5);
        g.setColour (blue.withAlpha (0.07f));
        for (float x = r.getX() + 10; x < r.getRight(); x += 10) g.drawVerticalLine ((int) x, r.getY() + 2, r.getBottom() - 2);
        for (float y = r.getY() + 10; y < r.getBottom(); y += 10) g.drawHorizontalLine ((int) y, r.getX() + 2, r.getRight() - 2);
    }
    void screenFrame (Graphics& g, Rectangle<float> r)
    {
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.06f), r.getX(), r.getY(), Colours::transparentWhite, r.getX(), r.getY() + r.getHeight() * 0.4f, false));
        g.fillRoundedRectangle (r, 5);
        g.setColour (Colours::black); g.drawRoundedRectangle (r, 5, 1.5f);
        g.setColour (Colours::white.withAlpha (0.1f)); g.drawRoundedRectangle (r.reduced (1.5f), 4, 1.0f);
    }
}
bool PositionView::animate()
{
    const float t = param (proc, "angle");
    if (std::abs (t - shown) < 0.05f) { if (shown != t) { shown = t; return true; } return false; }
    shown += (t - shown) * 0.22f; return true;
}
void PositionView::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    screen (g, r);
    const auto& m = lk::mics()[(size_t) lk::clampMic ((int) param (proc, "mic"))];
    // cabeza vista desde arriba (mirando a la derecha)
    const juce::Point<float> head (r.getX() + 32, r.getCentreY() + 6);
    g.setColour (Colour (0xff2a3442)); g.fillEllipse (head.x - 22, head.y - 30, 30, 60);          // hombros
    g.setGradientFill (ColourGradient (Colour (0xff8aa0b8), head.x - 6, head.y - 10, Colour (0xff3b4b60), head.x + 14, head.y + 14, true));
    g.fillEllipse (head.x - 14, head.y - 15, 28, 30);
    g.fillEllipse (head.x + 11, head.y - 3, 7, 6);       // nariz
    g.setColour (Colour (0xff3b4b60)); g.fillEllipse (head.x - 5, head.y - 18, 6, 5); g.fillEllipse (head.x - 5, head.y + 13, 6, 5);
    const juce::Point<float> mouth (head.x + 16, head.y);
    // microfono girando sobre su centro
    const juce::Point<float> mc (r.getRight() - 52, head.y);
    const float a = shown * juce::MathConstants<float>::pi / 180.0f;
    const float dirA = juce::MathConstants<float>::pi - a;          // 0 grados = apunta a la boca (izquierda)
    // patron polar
    Path pol;
    for (int i = 0; i <= 72; ++i)
    {
        const float t = (float) i / 72.0f * juce::MathConstants<float>::twoPi;
        const float gg = std::abs (tmic::MicChain::polar (m.pattern, t * 180.0f / juce::MathConstants<float>::pi)) * 34.0f;
        const juce::Point<float> p (mc.x + gg * std::cos (dirA + t), mc.y - gg * std::sin (dirA + t));
        if (i == 0) pol.startNewSubPath (p); else pol.lineTo (p);
    }
    pol.closeSubPath();
    g.setColour (blue.withAlpha (0.16f)); g.fillPath (pol);
    g.setColour (blue.withAlpha (0.6f)); g.strokePath (pol, juce::PathStrokeType (1.0f));
    // marcas de angulo
    g.setFont (font (9.5f));
    for (int deg : { 0, 15, 30, 45, 90 })
    {
        const float da = juce::MathConstants<float>::pi - (float) deg * juce::MathConstants<float>::pi / 180.0f;
        const float R0 = 40, R1 = 45;
        const bool on = std::abs (shown - (float) deg) < 4.0f;
        g.setColour (on ? blueHi : Colour (0xff6d7f96));
        g.drawLine (mc.x + R0 * std::cos (da), mc.y - R0 * std::sin (da), mc.x + R1 * std::cos (da), mc.y - R1 * std::sin (da), on ? 2.0f : 1.0f);
        g.drawText (juce::String (deg) + juce::String::fromUTF8 ("°"), Rectangle<float> (mc.x + 54 * std::cos (da) - 14, mc.y - 54 * std::sin (da) - 6, 28, 12), juce::Justification::centred);
    }
    // linea boca -> capsula
    Path dash; dash.startNewSubPath (mouth); dash.lineTo (mc);
    float dl[] { 3, 3 }; Path dd; juce::PathStrokeType (1.0f).createDashedStroke (dd, dash, dl, 2);
    g.setColour (Colours::white.withAlpha (0.25f)); g.fillPath (dd);
    // cuerpo del microfono desde arriba
    g.saveState();
    g.addTransform (juce::AffineTransform::rotation (-a, mc.x, mc.y));
    Rectangle<float> body (mc.x - 6, mc.y - 7, 34, 14);
    g.setGradientFill (ColourGradient (Colour (0xffdfe2e6), mc.x, mc.y - 7, Colour (0xff6a6d72), mc.x, mc.y + 7, false)); g.fillRoundedRectangle (body, 4);
    Rectangle<float> cap (mc.x - 16, mc.y - 9, 14, 18);
    g.setColour (Colour (0xff2b2e33)); g.fillRoundedRectangle (cap, 5);
    g.setColour (Colour (0xffaab3bf)); for (int i = 0; i < 4; ++i) g.drawVerticalLine ((int) (cap.getX() + 3 + i * 3), cap.getY() + 2, cap.getBottom() - 2);
    g.setColour (blueHi); g.fillEllipse (mc.x - 18, mc.y - 2, 4, 4);
    g.restoreState();
    // texto
    g.setFont (font (18, true)); g.setColour (Colours::white);
    g.drawText (juce::String (juce::roundToInt (shown)) + juce::String::fromUTF8 ("°"), Rectangle<float> (r.getX() + 8, r.getY() + 4, 60, 20), juce::Justification::left);
    g.setFont (font (10)); g.setColour (blueHi);
    const juce::String st = shown < 7 ? "ON AXIS" : shown < 50 ? "OFF AXIS" : "SIDE";
    g.drawText (st, Rectangle<float> (r.getX() + 8, r.getY() + 22, 80, 12), juce::Justification::left);
    screenFrame (g, r);
}

bool ProxView::animate()
{
    const float t = param (proc, "prox");
    if (std::abs (t - shown) < 0.05f) { if (shown != t) { shown = t; return true; } return false; }
    shown += (t - shown) * 0.22f; return true;
}
void ProxView::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    screen (g, r);
    const float floorY = r.getBottom() - 22;
    // silueta de perfil
    const float hx = r.getX() + 26, hy = r.getY() + 44;
    Path person;
    person.startNewSubPath (hx - 12, floorY); person.lineTo (hx - 14, hy + 30); person.quadraticTo (hx - 14, hy + 20, hx - 6, hy + 18);
    person.lineTo (hx - 4, hy + 12); person.quadraticTo (hx - 16, hy + 4, hx - 14, hy - 10); person.quadraticTo (hx - 10, hy - 24, hx + 2, hy - 23);
    person.quadraticTo (hx + 12, hy - 22, hx + 14, hy - 10); person.lineTo (hx + 15, hy - 4); person.lineTo (hx + 19, hy + 1); person.lineTo (hx + 15, hy + 3);
    person.lineTo (hx + 15, hy + 7); person.quadraticTo (hx + 12, hy + 14, hx + 5, hy + 14); person.lineTo (hx + 5, hy + 18);
    person.quadraticTo (hx + 16, hy + 20, hx + 18, hy + 30); person.lineTo (hx + 16, floorY); person.closeSubPath();
    g.setGradientFill (ColourGradient (Colour (0xff6f86a3), hx, hy - 20, Colour (0xff2a3546), hx, floorY, false)); g.fillPath (person);
    const juce::Point<float> mouth (hx + 17, hy + 4);
    // posicion del microfono segun la distancia (escala logaritmica)
    const float dist = lk::distanceCm (shown);
    const float x0 = mouth.x + 8, x1 = r.getRight() - 22;
    const float mx = x0 + (x1 - x0) * shown / 100.0f;
    // ondas de la voz
    for (int i = 1; i <= 3; ++i)
    {
        Path w; w.addCentredArc (mouth.x, mouth.y, 6.0f * (float) i, 8.0f * (float) i, 0, 0.6f, 2.5f, true);
        g.setColour (blueHi.withAlpha (0.45f / (float) i)); g.strokePath (w, juce::PathStrokeType (1.0f));
    }
    // pie y microfono de lado
    g.setColour (Colour (0xff3b4350)); g.drawLine (mx + 8, mouth.y + 6, mx + 8, floorY, 2.0f); g.drawLine (mx - 2, floorY, mx + 18, floorY, 2.0f);
    Rectangle<float> head (mx - 1, mouth.y - 6, 11, 12), body (mx + 9, mouth.y - 4, 16, 8);
    g.setColour (Colour (0xff2b2e33)); g.fillRoundedRectangle (head, 3);
    g.setColour (Colour (0xffaab3bf)); for (int i = 0; i < 3; ++i) g.drawVerticalLine ((int) (head.getX() + 2 + i * 3), head.getY() + 1, head.getBottom() - 1);
    g.setGradientFill (ColourGradient (Colour (0xffdfe2e6), 0, body.getY(), Colour (0xff6a6d72), 0, body.getBottom(), false)); g.fillRoundedRectangle (body, 2);
    // medida
    g.setColour (blueHi.withAlpha (0.8f));
    g.drawLine (mouth.x + 2, mouth.y - 16, mx - 1, mouth.y - 16, 1.0f);
    g.drawLine (mouth.x + 2, mouth.y - 19, mouth.x + 2, mouth.y - 13, 1.0f); g.drawLine (mx - 1, mouth.y - 19, mx - 1, mouth.y - 13, 1.0f);
    g.setFont (font (11, true)); g.setColour (Colours::white);
    g.drawText ("~" + juce::String (juce::roundToInt (dist)) + " cm", Rectangle<float> ((mouth.x + mx) * 0.5f - 30, mouth.y - 32, 60, 13), juce::Justification::centred);
    // zonas
    const int zone = dist < 8 ? 0 : dist < 25 ? 1 : 2;
    const char* zn[] { "CLOSE", "MEDIUM", "FAR" };
    const char* zd[] { "+ graves, + proximity", "balance natural", "- graves, + ambiente" };
    for (int i = 0; i < 3; ++i)
    {
        Rectangle<float> z (r.getX() + 6 + (float) i * (r.getWidth() - 12) / 3.0f, floorY + 4, (r.getWidth() - 12) / 3.0f - 3, 14);
        g.setColour (i == zone ? blue.withAlpha (0.55f) : Colours::white.withAlpha (0.06f)); g.fillRoundedRectangle (z, 3);
        g.setFont (font (9.5f, i == zone)); g.setColour (i == zone ? Colours::white : Colour (0xff7d8ca0));
        g.drawText (zn[i], z, juce::Justification::centred);
    }
    g.setFont (font (10)); g.setColour (blueHi);
    g.drawText (zd[zone], Rectangle<float> (r.getRight() - 128, r.getY() + 5, 122, 12), juce::Justification::right);
    screenFrame (g, r);
}

void ToneCurve::refresh()
{
    const auto p = proc.params();
    std::vector<lk::Band> A, B; lk::buildBands (p, A, B);
    const auto T = lk::toneBands (p);
    // color del micro = todo menos el tono
    std::vector<lk::Band> M = A; for (size_t i = 0; i + T.size() < B.size(); ++i) M.push_back (B[i]);
    mic.clear(); tone.clear();
    for (int i = 0; i < 120; ++i)
    {
        const double f = 20.0 * std::pow (1000.0, i / 119.0);
        mic.push_back (lk::bandsDb (M, 96000.0, f)); tone.push_back (lk::bandsDb (T, 96000.0, f));
    }
    repaint();
}
void ToneCurve::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    screen (g, r);
    auto X = [&r] (double f) { return r.getX() + (float) (std::log (f / 20.0) / std::log (1000.0)) * r.getWidth(); };
    auto Y = [&r] (float db) { return r.getCentreY() - juce::jlimit (-15.0f, 15.0f, db) / 15.0f * (r.getHeight() * 0.44f); };
    g.setFont (font (9));
    for (double f : { 100.0, 1000.0, 10000.0 })
    {
        g.setColour (Colours::white.withAlpha (0.1f)); g.drawVerticalLine ((int) X (f), r.getY(), r.getBottom());
        g.setColour (Colour (0xff6d7f96)); g.drawText (f >= 1000 ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f), Rectangle<float> (X (f) + 2, r.getBottom() - 12, 30, 10), juce::Justification::left);
    }
    g.setColour (Colours::white.withAlpha (0.16f)); g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());
    if (mic.size() == 120)
    {
        Path pm, pt;
        for (int i = 0; i < 120; ++i)
        {
            const float x = r.getX() + r.getWidth() * (float) i / 119.0f;
            if (i == 0) { pm.startNewSubPath (x, Y (mic[0])); pt.startNewSubPath (x, Y (tone[0])); }
            else { pm.lineTo (x, Y (mic[(size_t) i])); pt.lineTo (x, Y (tone[(size_t) i])); }
        }
        g.setColour (Colour (0xffb9a27a).withAlpha (0.65f)); g.strokePath (pm, juce::PathStrokeType (1.2f));
        Path fill = pt; fill.lineTo (r.getRight(), r.getCentreY()); fill.lineTo (r.getX(), r.getCentreY()); fill.closeSubPath();
        g.setColour (blue.withAlpha (0.22f)); g.fillPath (fill);
        g.setColour (blue.withAlpha (0.35f)); g.strokePath (pt, juce::PathStrokeType (4.0f));
        g.setColour (blueHi); g.strokePath (pt, juce::PathStrokeType (1.8f));
        // puntos de cada banda
        const auto p = proc.params();
        const double fr[] { lk::lowFreqs()[juce::jlimit (0, 3, p.lowFreq)], lk::midFreqs()[juce::jlimit (0, 5, p.midFreq)], lk::highFreqs()[juce::jlimit (0, 4, p.highFreq)] };
        for (double f : fr)
        {
            const int i = juce::jlimit (0, 119, (int) std::round (std::log (f / 20.0) / std::log (1000.0) * 119.0));
            g.setColour (Colours::white); g.fillEllipse (X (f) - 3, Y (tone[(size_t) i]) - 3, 6, 6);
        }
    }
    g.setFont (font (9, true));
    g.setColour (blueHi); g.drawText ("TONE", Rectangle<float> (r.getX() + 6, r.getY() + 3, 40, 10), juce::Justification::left);
    g.setColour (Colour (0xffb9a27a)); g.drawText ("MIC COLOR", Rectangle<float> (r.getX() + 40, r.getY() + 3, 70, 10), juce::Justification::left);
    screenFrame (g, r);
}

// ---------------- Preamp ----------------
void PreampView::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const int p = juce::jlimit (0, (int) lk::preamps().size() - 1, (int) param (proc, "pre"));
    g.setColour (Colours::black.withAlpha (0.7f)); g.fillRoundedRectangle (r.translated (0, 2), 6);
    static const juce::uint32 face[] { 0xff3a4250, 0xff3a4250, 0xff3c4a5e, 0xff2c2f34, 0xff4a3a2c, 0xff2e3136, 0xff4c4232 };
    const Colour fc (face[p]);
    g.setGradientFill (ColourGradient (fc, 0, r.getY(), fc.darker (1.2f), 0, r.getBottom(), false)); g.fillRoundedRectangle (r, 6);
    juce::Random rnd (3);
    for (float y = r.getY() + 3; y < r.getBottom() - 3; y += 1.3f) { g.setColour (Colours::white.withAlpha (0.015f + 0.025f * rnd.nextFloat())); g.drawHorizontalLine ((int) y, r.getX() + 4, r.getRight() - 4); }
    g.setColour (Colours::white.withAlpha (0.15f)); g.drawRoundedRectangle (r.reduced (4), 4, 1.0f);
    g.setColour (Colours::black); g.drawRoundedRectangle (r, 6, 1.5f);
    for (auto pt : { r.getTopLeft() + juce::Point<float> (10, 10), r.getTopRight() + juce::Point<float> (-10, 10), r.getBottomLeft() + juce::Point<float> (10, -10), r.getBottomRight() + juce::Point<float> (-10, -10) })
        screw (g, pt.x, pt.y, 3.2f);
    chevron (g, Rectangle<float> (r.getX(), r.getY(), 30, r.getHeight()), true, ink);
    chevron (g, Rectangle<float> (r.getRight() - 30, r.getY(), 30, r.getHeight()), false, ink);
    static const juce::uint32 cols[] { 0xff8a8f99, 0xffc0392b, 0xff2e86de, 0xff27ae60, 0xffe08e2b, 0xff9aa4b2, 0xffb8860b };
    const Colour bc (cols[p]);
    const float cx = r.getCentreX(), cy = r.getY() + 34, br = 14;
    Path hex;
    for (int i = 0; i < 6; ++i) { const float a = (float) i / 6.0f * juce::MathConstants<float>::twoPi; if (i == 0) hex.startNewSubPath (cx + br * std::sin (a), cy - br * std::cos (a)); else hex.lineTo (cx + br * std::sin (a), cy - br * std::cos (a)); }
    hex.closeSubPath();
    g.setGradientFill (ColourGradient (bc.withAlpha (0.4f), cx, cy, Colours::transparentBlack, cx + 70, cy, true)); g.fillEllipse (cx - 70, cy - 30, 140, 60);
    g.setGradientFill (ColourGradient (bc.brighter (0.5f), cx, cy - br, bc.darker (0.6f), cx, cy + br, false)); g.fillPath (hex);
    g.setColour (Colours::white.withAlpha (0.8f)); g.strokePath (hex, juce::PathStrokeType (1.3f));
    g.setFont (font (14, true)); g.setColour (Colours::white);
    g.drawText (juce::String (lk::preamps()[(size_t) p].name).substring (0, 1), Rectangle<float> (cx - br, cy - br, br * 2, br * 2), juce::Justification::centred);
    g.setFont (font (26)); g.setColour (Colours::black.withAlpha (0.6f));
    auto nr = Rectangle<float> (r.getX(), cy + 18, r.getWidth(), 32);
    g.drawText (lk::preamps()[(size_t) p].name, nr.translated (0, 1.5f), juce::Justification::centred);
    g.setColour (Colour (0xfff0f0f2)); g.drawText (lk::preamps()[(size_t) p].name, nr, juce::Justification::centred);
    g.setFont (font (12)); g.setColour (Colour (0xffc8ccd4));
    g.drawText (lk::preamps()[(size_t) p].style, Rectangle<float> (r.getX(), nr.getBottom() - 2, r.getWidth(), 16), juce::Justification::centred);
}
void PreampView::mouseUp (const juce::MouseEvent& e)
{
    const int p = (int) param (proc, "pre"), n = (int) lk::preamps().size();
    if (e.position.x < 34) { setParam (proc, "pre", (float) ((p + n - 1) % n)); repaint(); return; }
    if (e.position.x > (float) getWidth() - 34) { setParam (proc, "pre", (float) ((p + 1) % n)); repaint(); return; }
    juce::PopupMenu m; m.addSectionHeader ("MIC PREAMP");
    for (int i = 0; i < n; ++i) m.addItem (i + 1, juce::String (lk::preamps()[(size_t) i].name) + "  (" + lk::preamps()[(size_t) i].style + ")  -  " + U (lk::preamps()[(size_t) i].desc), true, i == p);
    juce::Component::SafePointer<PreampView> sp (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [sp] (int res) { if (sp != nullptr && res > 0) { setParam (sp->proc, "pre", (float) (res - 1)); sp->repaint(); } });
}

// ---------------- Room ----------------
void RoomView::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const int rm = juce::jlimit (0, (int) lk::rooms().size() - 1, (int) param (proc, "room"));
    auto pic = Rectangle<float> (30, 0, r.getWidth() - 60, r.getHeight() - 36);
    g.setColour (Colours::black); g.fillRoundedRectangle (pic.expanded (3), 4);
    drawRoom (g, pic, rm, param (proc, "roomSize"));
    g.setColour (Colours::white.withAlpha (0.15f)); g.drawRoundedRectangle (pic.expanded (3), 4, 1.0f);
    chevron (g, Rectangle<float> (0, 0, 30, pic.getHeight()), true, ink);
    chevron (g, Rectangle<float> (r.getWidth() - 30, 0, 30, pic.getHeight()), false, ink);
    auto nb = Rectangle<float> (r.getCentreX() - 120, r.getHeight() - 32, 240, 30);
    inset (g, nb, 5); g.setColour (Colours::white.withAlpha (0.1f)); g.drawRoundedRectangle (nb, 5, 1.0f);
    g.setFont (font (16)); g.setColour (ink); g.drawText (U (lk::rooms()[(size_t) rm].name), nb.withTrimmedRight (20), juce::Justification::centred);
    chevron (g, nb.removeFromRight (26), false, ink, 1.8f);
}
void RoomView::mouseUp (const juce::MouseEvent& e)
{
    const int rm = (int) param (proc, "room"), n = (int) lk::rooms().size();
    // orden visual de las salas
    static const int order[] { 0, 1, 2, 6, 3, 4, 7, 8, 5 };
    int pos = 0; for (int i = 0; i < n; ++i) if (order[i] == rm) pos = i;
    if (e.position.x < 30 && e.position.y < (float) getHeight() - 36) { setParam (proc, "room", (float) order[(pos + n - 1) % n]); repaint(); return; }
    if (e.position.x > (float) getWidth() - 30 && e.position.y < (float) getHeight() - 36) { setParam (proc, "room", (float) order[(pos + 1) % n]); repaint(); return; }
    juce::PopupMenu m; m.addSectionHeader ("ROOM");
    for (int i = 0; i < n; ++i) m.addItem (order[i] + 1, U (lk::rooms()[(size_t) order[i]].name), true, order[i] == rm);
    juce::Component::SafePointer<RoomView> sp (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [sp] (int res) { if (sp != nullptr && res > 0) { setParam (sp->proc, "room", (float) (res - 1)); sp->repaint(); } });
}
// ---------------- Medidor ----------------
void Meter::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colour (0xff0a0a0b)); g.fillRoundedRectangle (r, 5);
    g.setColour (Colours::black); g.drawRoundedRectangle (r, 5, 1.2f);
    g.setFont (font (14)); g.setColour (ink);
    g.drawText ("IN", Rectangle<float> (4, 4, 30, 18), juce::Justification::centred);
    g.drawText ("OUT", Rectangle<float> (r.getWidth() - 40, 4, 38, 18), juce::Justification::centred);
    const float top = 28, bot = r.getHeight() - 8, segH = (bot - top) / 24.0f;
    auto col = [] (float db) { return db > -3 ? Colour (0xffff5a2a) : db > -6 ? Colour (0xffffc021) : db > -12 ? Colour (0xffc8f02a) : Colour (0xff3fd44a); };
    for (int m = 0; m < 2; ++m)
    {
        const float lvl = m == 0 ? in : out, x = m == 0 ? 10.0f : 26.0f;
        for (int s = 0; s < 24; ++s)
        {
            const float db = -48.0f + (float) (s + 1) * 2.0f;
            Rectangle<float> seg (x, bot - (float) (s + 1) * segH + 1, 12, segH - 1.6f);
            const bool lit = lvl >= db - 2.0f;
            g.setColour (lit ? col (db) : col (db).withAlpha (0.13f)); g.fillRect (seg);
        }
    }
    g.setFont (font (11)); g.setColour (dim);
    for (int db : { 0, -6, -12, -18, -24, -36, -48 })
    {
        const float y = bot - ((float) db + 48.0f) / 48.0f * (bot - top);
        g.drawText (juce::String (db), Rectangle<float> (42, y - 7, r.getWidth() - 46, 14), juce::Justification::centredRight);
    }
}

// ---------------- Contenido ----------------
namespace
{
    juce::String pct (float v) { return juce::String (juce::roundToInt (v)) + " %"; }
    juce::String dB1 (float v) { return juce::String (v, 1) + " dB"; }
}
Content::Content (TopLockerProcessor& p)
    : proc (p), carousel (p),
      angle (p.apvts, "angle", U ("0°"), U ("90°"), [] (float v) { return juce::String (juce::roundToInt (v)) + U ("°"); }, 56),
      prox (p.apvts, "prox", "Close", "Far", [] (float v) { return "~" + juce::String (juce::roundToInt (lk::distanceCm (v))) + " cm"; }, 56),
      low (p.apvts, "low", "", "", dB1, 50, true), mid (p.apvts, "mid", "", "", dB1, 50, true), high (p.apvts, "high", "", "", dB1, 50, true),
      character (p.apvts, "character", "Clean", "Color", nullptr, 74, true),
      sat (p.apvts, "sat", "0 %", "100 %", pct, 74, true),
      out (p.apvts, "out", "-18 dB", "+18 dB", dB1, 74),
      drive (p.apvts, "preGain", "", "", [] (float v) { return juce::String (v, 1); }, 50, true),
      input (p.apvts, "preInput", "", "", [] (float v) { return (v > 0 ? "+" : "") + juce::String (v, 1) + " dB"; }, 50, true),
      roomAmt (p.apvts, "roomAmt", "", "", pct, 36, true), roomSize (p.apvts, "roomSize", "", "", pct, 36, true), roomDecay (p.apvts, "roomDecay", "", "", pct, 36, true),
      roomEarly (p.apvts, "roomEarly", "", "", pct, 36, true), roomWidth (p.apvts, "roomWidth", "", "", pct, 36, true), roomDamp (p.apvts, "roomDamp", "", "", pct, 36, true),
      mix (p.apvts, "mix", "Dry", "Wet", pct, 74),
      posView (p), proxView (p), curve (p), preamp (p), room (p)
{
    character.showValue = false;
    for (auto* d : { &low, &mid, &high, &out, &input }) d->slider.getProperties().set ("bipolar", true);
    for (auto* d : { &low, &mid, &high, &drive, &input, &roomAmt, &roomSize, &roomDecay, &roomEarly, &roomWidth, &roomDamp, &angle, &prox }) d->compact = true;
    for (juce::Component* c : std::initializer_list<juce::Component*> { &cats, &carousel, &prev, &next, &save, &load, &gear, &info, &presetBox, &charBox, &sourceBox, &lowBox, &midBox, &highBox,
                                    &angle, &prox, &low, &mid, &high, &character, &sat, &out, &drive, &input, &roomAmt, &roomSize, &roomDecay, &roomEarly, &roomWidth, &roomDamp, &mix,
                                    &posView, &proxView, &curve, &preamp, &room, &meter, &micPrev, &micNext })
        addAndMakeVisible (c);
    cats.onPick = [this] (int c) { carousel.setCategory (c); };
    micPrev.onClick = [this] { carousel.step (-1); };
    micNext.onClick = [this] { carousel.step (1); };
    prev.onClick = [this] { stepPreset (-1); };
    next.onClick = [this] { stepPreset (1); };
    save.onClick = [this] { saveDialog(); };
    load.onClick = [this] { loadDialog(); };
    gear.onClick = [this] { gearMenu(); };
    info.onClick = []
    {
        juce::String t;
        t << "TOP LOCKER  v2.0  -  Dj Pava HM Music\n\n"
          << "Cada microfono cambia respuesta, armonicos, transientes, compresion natural, proximidad y cuanta sala capta.\n\n";
        for (auto& m : lk::mics()) t << m.line1 << " " << m.line2 << " (" << m.ref << "): " << U (m.desc) << "\n";
        t << "\nPreamps:\n";
        for (auto& pr : lk::preamps()) t << pr.name << " (" << pr.style << "): " << U (pr.desc) << "\n";
        t << U ("\nLos nombres y dibujos son propios; las marcas mencionadas pertenecen a sus dueños y solo indican el carácter de referencia.");
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Top Locker", t);
    };
    presetBox.label = "Preset:"; presetBox.onClick = [this] { presetMenu(); };
    charBox.centred = true; charBox.onClick = [this] { charMenu(); };
    sourceBox.onClick = [this] { sourceMenu(); };
    for (auto* b : { &lowBox, &midBox, &highBox }) { b->fontSize = 11.5f; b->centred = true; b->arrow = false; }
    lowBox.onClick = [this] { freqMenu (0); }; midBox.onClick = [this] { freqMenu (1); }; highBox.onClick = [this] { freqMenu (2); };
    sourceBox.setTooltip (U ("Elige el micrófono con el que grabaste: Top Locker le quita su color antes de poner el nuevo."));
    preamp.setTooltip (U ("Flechas: cambiar de preamp. Centro: lista completa."));
    input.slider.setTooltip (U ("Nivel que entra al preamp: más nivel = más saturación (la salida se compensa)."));
    drive.slider.setTooltip (U ("Cuánto color/armónicos agrega el preamp."));
    prev.setTooltip ("Preset anterior"); next.setTooltip ("Preset siguiente"); save.setTooltip ("Guardar tu preset"); load.setTooltip ("Cargar un preset");
    carousel.ensureVisible();
    posView.shown = param (proc, "angle"); proxView.shown = param (proc, "prox");
    curve.refresh();
    tick();
}

void Content::resized()
{
    cats.setBounds (14, 86, 160, 316);
    carousel.setBounds (180, 86, 1086, 316);
    micPrev.setBounds (184, 210, 24, 60); micNext.setBounds (1238, 210, 24, 60);
    prev.setBounds (730, 22, 34, 36); presetBox.setBounds (768, 22, 244, 36); next.setBounds (1016, 22, 34, 36);
    save.setBounds (1058, 22, 60, 36); load.setBounds (1122, 22, 60, 36); gear.setBounds (1188, 22, 38, 36); info.setBounds (1230, 22, 38, 36);
    // fila de controles
    posView.setBounds (16, 450, 176, 104);   angle.setBounds (20, 560, 168, 100);
    proxView.setBounds (216, 450, 176, 104); prox.setBounds (220, 560, 168, 100);
    curve.setBounds (416, 450, 310, 64);
    low.setBounds (416, 534, 103, 92); mid.setBounds (519, 534, 103, 92); high.setBounds (622, 534, 103, 92);
    lowBox.setBounds (424, 632, 88, 24); midBox.setBounds (527, 632, 88, 24); highBox.setBounds (630, 632, 88, 24);
    character.setBounds (745, 452, 150, 120); charBox.setBounds (757, 580, 126, 28);
    sat.setBounds (909, 466, 120, 150);
    out.setBounds (1043, 466, 120, 150);
    meter.setBounds (1178, 424, 90, 234);
    // fila de abajo
    preamp.setBounds (18, 706, 184, 150);
    drive.setBounds (204, 702, 70, 96); input.setBounds (274, 702, 70, 96);
    room.setBounds (360, 704, 304, 180);
    roomAmt.setBounds (668, 700, 72, 80); roomSize.setBounds (742, 700, 72, 80); roomDecay.setBounds (816, 700, 72, 80);
    roomEarly.setBounds (668, 800, 72, 80); roomWidth.setBounds (742, 800, 72, 80); roomDamp.setBounds (816, 800, 72, 80);
    mix.setBounds (904, 716, 130, 150);
    sourceBox.setBounds (1060, 712, 202, 32);
}

void Content::paint (Graphics& g)
{
    g.setGradientFill (ColourGradient (Colour (0xff1d1c1b), 0, 0, Colour (0xff0c0c0c), 0, (float) H, false)); g.fillAll();
    panel (g, { 6, 6, 1268, 66 });
    brandLogo (g, 22, 12, 1.0f);
    g.setColour (Colours::white.withAlpha (0.12f)); g.drawVerticalLine (300, 16, 62); g.setColour (Colours::black); g.drawVerticalLine (301, 16, 62);
    silverText (g, "TOP LOCKER", font (40, true, true), 318, 48);
    g.setFont (font (12, true)); g.setColour (blueHi); g.drawText ("2.0", Rectangle<float> (585, 18, 40, 16), juce::Justification::left);
    g.setFont (font (11.5f)); g.setColour (dim);
    g.drawText ("V I R T U A L   M I C R O P H O N E   C O L L E C T I O N", Rectangle<float> (320, 50, 330, 16), juce::Justification::left);
    panel (g, { 6, 78, 1268, 332 }, false);
    struct P { float x, w; const char* t; };
    for (auto pp : { P { 6, 196, "MIC POSITION" }, P { 206, 196, "PROXIMITY" }, P { 406, 330, "" }, P { 740, 160, "CHARACTER" }, P { 904, 130, "SATURATION" }, P { 1038, 130, "OUTPUT" } })
    {
        panel (g, { pp.x, 416, pp.w, 250 });
        if (*pp.t) title (g, pp.t, { pp.x, 422, pp.w, 24 });
    }
    auto tb = Rectangle<float> (461, 421, 220, 25); inset (g, tb, 4); title (g, "TONE CONTROL", tb, 16);
    g.setFont (font (13)); g.setColour (ink);
    const char* tn[] { "LOW", "MID", "HIGH" };
    for (int i = 0; i < 3; ++i) g.drawText (tn[i], Rectangle<float> (416 + (float) i * 103, 517, 103, 16), juce::Justification::centred);
    // descripcion del circuito de caracter
    {
        static const char* cd[] { "brillo limpio + armonicos sutiles", "cuerpo y agudos redondos", "quita dureza 3-7 kHz", "sin coloracion", "valvula: armonicos pares", "transformador: graves saturados", "consola: mezcla par/impar" };
        g.setFont (font (11)); g.setColour (dim);
        g.drawFittedText (U (cd[juce::jlimit (0, 6, (int) param (proc, "charMode"))]), juce::Rectangle<int> (748, 612, 144, 40), juce::Justification::centredTop, 2);
    }
    panel (g, { 1172, 416, 102, 250 }, false);
    panel (g, { 6, 672, 340, 222 }); title (g, "MIC PREAMP", { 22, 678, 130, 24 }, 16);
    g.setFont (font (11, true)); g.setColour (dim);
    g.drawText ("DRIVE", Rectangle<float> (204, 800, 70, 14), juce::Justification::centred);
    g.drawText ("INPUT", Rectangle<float> (274, 800, 70, 14), juce::Justification::centred);
    g.setFont (font (10)); g.setColour (dim.withAlpha (0.8f));
    g.drawFittedText (U ("Input: cuánto entra\nDrive: cuánto color"), juce::Rectangle<int> (206, 824, 136, 40), juce::Justification::centredTop, 2);
    panel (g, { 350, 672, 540, 222 }); title (g, "ROOM", { 362, 678, 70, 24 }, 16);
    {
        const char* rn[] { "AMOUNT", "SIZE", "DECAY", "EARLY", "WIDTH", "DAMPING" };
        g.setFont (font (10.5f, true)); g.setColour (dim);
        for (int i = 0; i < 6; ++i) g.drawText (rn[i], Rectangle<float> (668 + (float) (i % 3) * 74, i < 3 ? 686.0f : 786.0f, 72, 13), juce::Justification::centred);
    }
    panel (g, { 894, 672, 150, 222 }); title (g, "MIX", { 894, 682, 150, 24 });
    panel (g, { 1048, 672, 226, 222 });
    g.setFont (font (11.5f)); g.setColour (dim); g.drawText (U ("SOURCE MIC  (con qué grabaste)"), Rectangle<float> (1060, 690, 202, 18), juce::Justification::centredLeft);
    waveLogo (g, 1062, 800, 46, 26, Colour (0xffd8d8dc));
    auto f = font (25, true, true);
    silverText (g, "Dj Pava", f, 1108, 808);
    silverText (g, "HM", f, 1108 + textW (f, "Dj Pava "), 808, Colour (0xff9fd2ff), Colour (0xff2468d8));
    g.setFont (font (14)); g.setColour (Colour (0xffc4c4c8)); g.drawText ("T O P   L O C K E R", Rectangle<float> (1108, 816, 160, 18), juce::Justification::left);
    g.setFont (font (11.5f)); g.setColour (dim); g.drawText ("v2.0", Rectangle<float> (1108, 834, 160, 14), juce::Justification::left);
    if (param (proc, "power") < 0.5f)
    {
        g.setColour (Colour (0xffff5a2a)); g.setFont (font (13, true));
        g.drawText ("BYPASS", Rectangle<float> (630, 48, 80, 16), juce::Justification::centred);
    }
}

void Content::tick()
{
    auto toDb = [] (float v) { return juce::Decibels::gainToDecibels (v, -60.0f); };
    const float i = toDb (proc.inPeak.exchange (0.0f)), o = toDb (proc.outPeak.exchange (0.0f));
    inDb = i > inDb ? i : std::max (i, inDb - 0.6f); outDb = o > outDb ? o : std::max (o, outDb - 0.6f);
    if (std::abs (meter.in - inDb) > 0.1f || std::abs (meter.out - outDb) > 0.1f) { meter.in = inDb; meter.out = outDb; meter.repaint(); }
    if (carousel.animate()) carousel.repaint();
    if (posView.animate()) posView.repaint();
    if (proxView.animate()) proxView.repaint();
    const auto p = proc.params(); const int rm = (int) param (proc, "room"); const float pw = param (proc, "power"), rs = param (proc, "roomSize");
    if (! (p == lastP) || rm != lastRoom || rs != lastSize || proc.presetName != lastPreset || pw != lastPower)
    {
        const bool curveChanged = ! (p == lastP);
        lastP = p; lastRoom = rm; lastSize = rs; lastPreset = proc.presetName; lastPower = pw;
        presetBox.text = proc.presetName; presetBox.repaint();
        charBox.text = lk::charName (p.charMode); charBox.repaint();
        const auto& s = tmic::sources()[(size_t) juce::jlimit (0, (int) tmic::sources().size() - 1, p.source)];
        sourceBox.text = U (s.name); sourceBox.repaint();
        static const char* lf[] { "60 Hz", "100 Hz", "200 Hz", "300 Hz" }; static const char* mf[] { "400 Hz", "800 Hz", "1.2 kHz", "2 kHz", "3 kHz", "5 kHz" };
        static const char* hf[] { "5 kHz", "8 kHz", "10 kHz", "12 kHz", "16 kHz" };
        lowBox.text = juce::String (lf[juce::jlimit (0, 3, p.lowFreq)]) + (p.lowType == 0 ? U (" · Shelf") : U (" · Bell"));
        midBox.text = mf[juce::jlimit (0, 5, p.midFreq)];
        highBox.text = juce::String (hf[juce::jlimit (0, 4, p.highFreq)]) + (p.highType == 0 ? U (" · Shelf") : U (" · Bell"));
        lowBox.repaint(); midBox.repaint(); highBox.repaint();
        if (curveChanged) curve.refresh();
        preamp.repaint(); room.repaint(); posView.repaint(); proxView.repaint();
        repaint (740, 600, 160, 60); repaint (620, 40, 100, 30);
    }
}

void Content::stepPreset (int dir)
{
    const int n = proc.getNumPrograms();
    proc.setCurrentProgram ((proc.getCurrentProgram() + dir + n) % n);
    tick();
}
void Content::presetMenu()
{
    juce::PopupMenu m; m.addSectionHeader ("PRESETS DE FABRICA");
    for (int i = 0; i < proc.getNumPrograms(); ++i) m.addItem (i + 1, proc.getProgramName (i), true, proc.presetName == proc.getProgramName (i));
    const auto user = proc.userPresets();
    m.addSeparator(); m.addSectionHeader ("MIS PRESETS");
    if (user.isEmpty()) m.addItem (-1, U ("(todavía no guardaste ninguno)"), false);
    for (int i = 0; i < user.size(); ++i) m.addItem (1000 + i, user[i].getFileNameWithoutExtension(), true, proc.presetName == user[i].getFileNameWithoutExtension());
    m.addSeparator();
    m.addItem (900, "Guardar preset...");
    m.addItem (901, "Cargar preset desde archivo...");
    m.addItem (902, "Abrir carpeta de presets");
    juce::Component::SafePointer<Content> sp (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetBox), [sp, user] (int r)
    {
        if (sp == nullptr || r == 0) return;
        if (r >= 1000) sp->proc.loadUserPreset (user[r - 1000]);
        else if (r == 900) sp->saveDialog();
        else if (r == 901) sp->loadDialog();
        else if (r == 902) { auto d = TopLockerProcessor::userPresetDir(); d.createDirectory(); d.startAsProcess(); }
        else if (r > 0) sp->proc.setCurrentProgram (r - 1);
        sp->tick();
    });
}
void Content::sourceMenu()
{
    juce::PopupMenu m; m.addSectionHeader (U ("¿CON QUÉ MICRÓFONO GRABASTE?"));
    const int cur = (int) param (proc, "source");
    for (int i = 0; i < (int) tmic::sources().size(); ++i) m.addItem (i + 1, U (tmic::sources()[(size_t) i].name), true, i == cur);
    juce::Component::SafePointer<Content> sp (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&sourceBox), [sp] (int r) { if (sp != nullptr && r > 0) { setParam (sp->proc, "source", (float) (r - 1)); sp->tick(); } });
}
void Content::charMenu()
{
    juce::PopupMenu m; const int cur = (int) param (proc, "charMode");
    m.addSectionHeader ("CIRCUITO DE COLORACION");
    const int order[] { lk::ChClean, lk::ChModern, lk::ChVintage, lk::ChTube, lk::ChTransformer, lk::ChAnalog, lk::ChSmooth };
    static const char* d[] { "brillo limpio y armonicos sutiles", "cuerpo en medios-graves, agudos redondos", "quita dureza en 3-7 kHz", "sin coloracion (solo la respuesta del micro)",
                             "valvula: armonicos pares, calidez", "transformador: graves saturados (impares)", "consola analoga: mezcla par/impar" };
    for (int k : order) m.addItem (k + 1, juce::String (lk::charName (k)) + "  -  " + U (d[k]), true, k == cur);
    juce::Component::SafePointer<Content> sp (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&charBox), [sp] (int r) { if (sp != nullptr && r > 0) { setParam (sp->proc, "charMode", (float) (r - 1)); sp->tick(); sp->repaint(); } });
}
void Content::freqMenu (int band)
{
    juce::PopupMenu m;
    const char* fid = band == 0 ? "lowFreq" : band == 1 ? "midFreq" : "highFreq";
    const int cur = (int) param (proc, fid);
    auto* prm = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (fid));
    m.addSectionHeader ("FRECUENCIA");
    for (int i = 0; i < prm->choices.size(); ++i) m.addItem (i + 1, prm->choices[i], true, i == cur);
    if (band != 1)
    {
        const char* tid = band == 0 ? "lowType" : "highType";
        const int t = (int) param (proc, tid);
        m.addSeparator(); m.addSectionHeader ("TIPO DE FILTRO");
        m.addItem (101, "Shelf (estante)", true, t == 0);
        m.addItem (102, "Bell (campana)", true, t == 1);
    }
    juce::Component::SafePointer<Content> sp (this);
    juce::Component* target = band == 0 ? (juce::Component*) &lowBox : band == 1 ? (juce::Component*) &midBox : (juce::Component*) &highBox;
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target), [sp, band, fid] (int r)
    {
        if (sp == nullptr || r <= 0) return;
        if (r > 100) setParam (sp->proc, band == 0 ? "lowType" : "highType", (float) (r - 101));
        else setParam (sp->proc, fid, (float) (r - 1));
        sp->tick();
    });
}
void Content::gearMenu()
{
    juce::PopupMenu m;
    m.addItem (1, "Invertir fase", true, param (proc, "phase") > 0.5f);
    m.addItem (2, "Bypass (apagar el plugin)", true, param (proc, "power") < 0.5f);
    m.addSeparator();
    m.addItem (3, "Restablecer todo (Modern Vocal Chain)");
    m.addItem (4, "Abrir carpeta de presets");
    juce::Component::SafePointer<Content> sp (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&gear), [sp] (int r)
    {
        if (sp == nullptr) return;
        auto& pr = sp->proc;
        if (r == 1) setParam (pr, "phase", param (pr, "phase") > 0.5f ? 0.0f : 1.0f);
        if (r == 2) setParam (pr, "power", param (pr, "power") > 0.5f ? 0.0f : 1.0f);
        if (r == 3) pr.setCurrentProgram (0);
        if (r == 4) { auto d = TopLockerProcessor::userPresetDir(); d.createDirectory(); d.startAsProcess(); }
        sp->tick(); sp->repaint();
    });
}
void Content::saveDialog()
{
    auto dir = TopLockerProcessor::userPresetDir(); dir.createDirectory();
    chooser = std::make_unique<juce::FileChooser> ("Guardar preset de Top Locker", dir.getChildFile (proc.presetName + ".tlpreset"), "*.tlpreset");
    juce::Component::SafePointer<Content> sp (this);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                          [sp] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (sp != nullptr && f != juce::File()) { sp->proc.saveUserPreset (f.withFileExtension ("tlpreset")); sp->tick(); }
                          });
}
void Content::loadDialog()
{
    auto dir = TopLockerProcessor::userPresetDir(); dir.createDirectory();
    chooser = std::make_unique<juce::FileChooser> ("Cargar preset de Top Locker", dir, "*.tlpreset");
    juce::Component::SafePointer<Content> sp (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [sp] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (sp != nullptr && f.existsAsFile()) { sp->proc.loadUserPreset (f); sp->tick(); }
                          });
}

// ---------------- Editor ----------------
TopLockerEditor::TopLockerEditor (TopLockerProcessor& p) : AudioProcessorEditor (p), content (p)
{
    setLookAndFeel (&look);
    addAndMakeVisible (content);
    content.setBounds (0, 0, W, H);
    setResizable (true, true);
    setResizeLimits (896, 630, 1920, 1350);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) W / (double) H);
    setSize (W, H);
    startTimerHz (60);
}
TopLockerEditor::~TopLockerEditor() { setLookAndFeel (nullptr); }
void TopLockerEditor::resized() { content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) W)); }
