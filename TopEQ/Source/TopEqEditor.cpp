#include "TopEqEditor.h"

using namespace teqi;
using juce::Colour; namespace Colours = juce::Colours; using juce::Graphics; using juce::Path; using juce::Rectangle; using juce::ColourGradient;

namespace
{
    juce::String U (const char* s) { return juce::String::fromUTF8 (s); }
    juce::Font font (float h, bool bold = false) { return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain)); }
    constexpr float pi = juce::MathConstants<float>::pi, twoPi = juce::MathConstants<float>::twoPi;
    constexpr float deg = pi / 180.0f;
    constexpr float A0 = -137.5f * deg, A1 = 137.5f * deg;          // escala 0..10 como el original
    const Colour panelCol (0xff2e4556), silkCol (0xffe7ebee), holeCol (0xff0c0d0e);
    float param (TopEqProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); }
    void setParam (TopEqProcessor& p, const char* id, float v)
    { if (auto* q = p.apvts.getParameter (id)) { q->beginChangeGesture(); q->setValueNotifyingHost (q->convertTo0to1 (v)); q->endChangeGesture(); } }
    juce::Point<float> polar (juce::Point<float> c, float r, float a) { return { c.x + r * std::sin (a), c.y - r * std::cos (a) }; }

    // geometria (medida sobre la foto frontal del original, escalada a 1560 px)
    constexpr float S = 46.0f;                                   // alto de la barra superior
    constexpr float yTop = S + 115, yBot = S + 321;
    constexpr float xLB = 449, xLA = 644, xHB = 910, xHA = 1104, xSEL = 1299;
    constexpr float xIN = 354, xCPS = 542, xBW = 775, xKCS = 1006, xLamp = 1258, xPow = 1322;
    constexpr float Rk = 57.0f, Rnum = 72.0f;                    // radio de la falda y de los numeros
    constexpr float kSkirt = 1.32f;                              // componente mas grande que la perilla (sombra)
    // angulos de los selectores (grados, medidos en la foto)
    const float cpsA[] { -36, -10, 14, 42 }, kcsA[] { -89, -61, -31, 0, 30, 61, 91 }, selA[] { -34, 0, 34 };

    void silk (Graphics& g, const juce::String& t, Rectangle<float> r, float size, juce::Justification j = juce::Justification::centred, float alpha = 0.96f)
    {
        g.setFont (font (size).withExtraKerningFactor (0.07f));
        g.setColour (Colours::black.withAlpha (0.18f)); g.drawText (t, r.translated (0.5f, 0.7f), j);
        g.setColour (silkCol.withAlpha (alpha)); g.drawText (t, r, j);
    }
    // tornillo cromado de ranura
    void screw (Graphics& g, float x, float y, float r, float slot)
    {
        for (int i = 3; i >= 1; --i) { g.setColour (Colours::black.withAlpha (0.10f)); g.fillEllipse (x - r + (float) i * 0.6f, y - r + (float) i, r * 2 + (float) i, r * 2 + (float) i); }
        g.setGradientFill (ColourGradient (Colour (0xfffbfcfc), x - r * 0.5f, y - r * 0.6f, Colour (0xff7b8086), x + r * 0.7f, y + r * 0.9f, true));
        g.fillEllipse (x - r, y - r, r * 2, r * 2);
        g.setColour (Colour (0xff4b4f55)); g.drawEllipse (x - r, y - r, r * 2, r * 2, 0.8f);
        const auto a = polar ({ x, y }, r * 0.85f, slot), b = polar ({ x, y }, r * 0.85f, slot + pi);
        g.setColour (Colour (0xff3a3e44)); g.drawLine ({ a, b }, r * 0.28f);
        g.setColour (Colours::white.withAlpha (0.6f)); g.drawLine ({ a.translated (0, r * 0.16f), b.translated (0, r * 0.16f) }, 0.7f);
    }
    void dropShadow (Graphics& g, juce::Point<float> c, float r, float dy)
    {
        for (int i = 6; i >= 1; --i)
        {
            const float e = (float) i * r * 0.035f;
            g.setColour (Colours::black.withAlpha (0.075f));
            g.fillEllipse (c.x - r - e + r * 0.04f, c.y - r - e + dy, (r + e) * 2, (r + e) * 2);
        }
    }

    // ------------------------------------------------------------------ perilla de falda con estrella de 8 lobulos
    void drawSkirt (Graphics& g, juce::Point<float> c, float R, float ang)
    {
        dropShadow (g, c, R, R * 0.13f);
        g.setGradientFill (ColourGradient (Colour (0xff2a2a2b), c.x - R * 0.25f, c.y - R * 0.45f, Colour (0xff040404), c.x + R * 0.2f, c.y + R, true));
        g.fillEllipse (c.x - R, c.y - R, R * 2, R * 2);
        {
            juce::Graphics::ScopedSaveState ss (g);
            Path ring; ring.addEllipse (c.x - R, c.y - R, R * 2, R * 2); ring.setUsingNonZeroWinding (false); ring.addEllipse (c.x - R * 0.6f, c.y - R * 0.6f, R * 1.2f, R * 1.2f);
            g.reduceClipRegion (ring);
            constexpr int n = 160;
            for (int i = 0; i < n; ++i)
            {
                const float t0 = (float) i / n * twoPi, t1 = (float) (i + 1) / n * twoPi, tm = 0.5f * (t0 + t1);
                const float b = 0.30f * std::pow (std::max (0.0f, std::cos (tm - 0.22f * pi)), 6.0f) + 0.10f * std::pow (std::max (0.0f, std::cos (tm + 0.35f * pi)), 4.0f);
                if (b < 0.01f) continue;
                Path w; w.startNewSubPath (c); w.lineTo (polar (c, R, t0)); w.lineTo (polar (c, R, t1)); w.closeSubPath();
                g.setColour (Colours::white.withAlpha (b)); g.fillPath (w);
            }
            g.setGradientFill (ColourGradient (Colours::transparentBlack, c.x, c.y, Colours::black.withAlpha (0.55f), c.x + R, c.y, true));
            g.fillEllipse (c.x - R, c.y - R, R * 2, R * 2);
        }
        g.setColour (Colours::black.withAlpha (0.9f)); g.drawEllipse (c.x - R, c.y - R, R * 2, R * 2, 1.0f);
        g.setColour (Colour (0xfff2f0ea));
        g.drawLine ({ polar (c, R * 0.70f, ang), polar (c, R * 0.94f, ang) }, R * 0.045f);

        const float ro = R * 0.70f, ri = R * 0.57f;
        Path star;
        for (int i = 0; i <= 400; ++i)
        {
            const float t = (float) i / 400.0f * twoPi;
            const float s = std::sqrt (0.5f + 0.5f * std::cos (8.0f * (t - ang)));
            const auto p = polar (c, ri + (ro - ri) * s, t);
            if (i == 0) star.startNewSubPath (p); else star.lineTo (p);
        }
        star.closeSubPath();
        g.setColour (Colours::black.withAlpha (0.45f)); g.fillPath (star, juce::AffineTransform::translation (R * 0.02f, R * 0.06f));
        g.setGradientFill (ColourGradient (Colour (0xff1c1c1d), c.x, c.y - ro, Colour (0xff030303), c.x, c.y + ro, false)); g.fillPath (star);
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (star);
            constexpr int n = 240;
            for (int i = 0; i < n; ++i)
            {
                const float t0 = (float) i / n * twoPi, t1 = (float) (i + 1) / n * twoPi, tm = 0.5f * (t0 + t1);
                const float slope = -std::sin (8.0f * (tm - ang));
                const float face = std::max (0.0f, std::cos (tm - 0.15f * pi));
                const float b = 0.10f * face * (0.6f + 0.4f * slope) + 0.06f * std::pow (face, 8.0f);
                if (b < 0.004f) continue;
                Path w; w.startNewSubPath (c); w.lineTo (polar (c, ro * 1.02f, t0)); w.lineTo (polar (c, ro * 1.02f, t1)); w.closeSubPath();
                g.setColour (Colours::white.withAlpha (b)); g.fillPath (w);
            }
        }
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.30f), c.x, c.y - ro, Colours::transparentWhite, c.x, c.y, false));
        g.strokePath (star, juce::PathStrokeType (1.0f));
        const float rc = R * 0.48f, rr = R * 0.38f;
        g.setGradientFill (ColourGradient (Colour (0xff1a1a1b), c.x, c.y - rc, Colour (0xff050505), c.x, c.y + rc, false));
        g.fillEllipse (c.x - rc, c.y - rc, rc * 2, rc * 2);
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.35f), c.x, c.y - rc, Colours::transparentWhite, c.x, c.y - rc * 0.2f, false));
        g.drawEllipse (c.x - rc + 0.8f, c.y - rc + 0.8f, rc * 2 - 1.6f, rc * 2 - 1.6f, 1.4f);
        g.setGradientFill (ColourGradient (Colour (0xff020202), c.x, c.y - rr, Colour (0xff161617), c.x, c.y + rr, false));
        g.fillEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2);
        g.setGradientFill (ColourGradient (Colours::transparentWhite, c.x, c.y, Colours::white.withAlpha (0.22f), c.x, c.y + rr, false));
        g.drawEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2, 1.2f);
    }

    // ------------------------------------------------------------------ perilla de barra (CPS / KCS)
    void drawBar (Graphics& g, juce::Point<float> c, float L, float ang)
    {
        const auto xf = juce::AffineTransform::rotation (ang).translated (c.x, c.y);
        const float hub = L * 0.50f;
        dropShadow (g, c, hub, L * 0.12f);
        g.setGradientFill (ColourGradient (Colour (0xff222223), c.x - hub * 0.4f, c.y - hub * 0.6f, Colour (0xff030303), c.x + hub * 0.4f, c.y + hub, true));
        g.fillEllipse (c.x - hub, c.y - hub, hub * 2, hub * 2);
        g.setColour (Colours::white.withAlpha (0.12f)); g.drawEllipse (c.x - hub + 1, c.y - hub + 1, hub * 2 - 2, hub * 2 - 2, 1.0f);
        Path bar;
        bar.startNewSubPath (-L * 0.25f, -L * 1.0f); bar.lineTo (L * 0.25f, -L * 1.0f);
        bar.lineTo (L * 0.29f, L * 0.80f); bar.quadraticTo (L * 0.29f, L * 0.92f, L * 0.18f, L * 0.92f);
        bar.lineTo (-L * 0.18f, L * 0.92f); bar.quadraticTo (-L * 0.29f, L * 0.92f, -L * 0.29f, L * 0.80f);
        bar.closeSubPath();
        g.setColour (Colours::black.withAlpha (0.35f)); g.fillPath (bar, xf.translated (L * 0.06f, L * 0.12f));
        const auto l = juce::Point<float> (-L * 0.3f, 0).transformedBy (xf), r = juce::Point<float> (L * 0.3f, 0).transformedBy (xf);
        g.setGradientFill (ColourGradient (Colour (0xff262627), l.x, l.y, Colour (0xff020202), r.x, r.y, false)); g.fillPath (bar, xf);
        Path edge; edge.startNewSubPath (-L * 0.22f, -L * 0.3f); edge.lineTo (-L * 0.25f, L * 0.82f);
        g.setColour (Colours::white.withAlpha (0.16f)); g.strokePath (edge, juce::PathStrokeType (L * 0.035f), xf);
        Path fl, fr;
        fl.startNewSubPath (-L * 0.25f, -L * 1.0f); fl.lineTo (-L * 0.01f, -L * 1.0f); fl.lineTo (-L * 0.01f, -L * 0.40f); fl.lineTo (-L * 0.255f, -L * 0.30f); fl.closeSubPath();
        fr.startNewSubPath (L * 0.01f, -L * 1.0f); fr.lineTo (L * 0.25f, -L * 1.0f); fr.lineTo (L * 0.265f, -L * 0.52f); fr.lineTo (L * 0.01f, -L * 0.40f); fr.closeSubPath();
        const auto t = juce::Point<float> (0, -L).transformedBy (xf), b = juce::Point<float> (0, -L * 0.35f).transformedBy (xf);
        g.setGradientFill (ColourGradient (Colour (0xffd7d8d8), t.x, t.y, Colour (0xff8b8d90), b.x, b.y, false)); g.fillPath (fl, xf);
        g.setGradientFill (ColourGradient (Colour (0xffffffff), t.x, t.y, Colour (0xffc9cbcd), b.x, b.y, false)); g.fillPath (fr, xf);
        g.setColour (Colours::black.withAlpha (0.55f)); g.strokePath (bar, juce::PathStrokeType (1.0f), xf);
    }

    // ------------------------------------------------------------------ selector de pico (ATTEN SEL)
    void drawPeak (Graphics& g, juce::Point<float> c, float L, float ang)
    {
        const auto xf = juce::AffineTransform::rotation (ang).translated (c.x, c.y);
        const float hub = L * 0.46f;
        dropShadow (g, c, hub, L * 0.12f);
        g.setGradientFill (ColourGradient (Colour (0xff262627), c.x - hub * 0.4f, c.y - hub * 0.6f, Colour (0xff030303), c.x + hub * 0.4f, c.y + hub, true));
        g.fillEllipse (c.x - hub, c.y - hub, hub * 2, hub * 2);
        Path p;
        p.startNewSubPath (0, -L * 1.05f);
        p.quadraticTo (L * 0.10f, -L * 0.6f, L * 0.24f, L * 0.55f);
        p.quadraticTo (0, L * 0.72f, -L * 0.24f, L * 0.55f);
        p.quadraticTo (-L * 0.10f, -L * 0.6f, 0, -L * 1.05f);
        p.closeSubPath();
        g.setColour (Colours::black.withAlpha (0.35f)); g.fillPath (p, xf.translated (L * 0.06f, L * 0.12f));
        const auto l = juce::Point<float> (-L * 0.25f, 0).transformedBy (xf), r = juce::Point<float> (L * 0.25f, 0).transformedBy (xf);
        g.setGradientFill (ColourGradient (Colour (0xff3a3a3b), l.x, l.y, Colour (0xff030303), r.x, r.y, false)); g.fillPath (p, xf);
        for (float x : { -0.12f, -0.04f, 0.04f, 0.12f })
        {
            Path s; s.startNewSubPath (L * x * 0.4f, -L * 0.6f); s.lineTo (L * x, L * 0.5f);
            g.setColour (Colours::white.withAlpha (0.10f)); g.strokePath (s, juce::PathStrokeType (L * 0.02f), xf);
        }
        g.setColour (Colour (0xfff2f0ea));
        g.drawLine ({ juce::Point<float> (0, -L * 1.0f).transformedBy (xf), juce::Point<float> (0, -L * 0.55f).transformedBy (xf) }, L * 0.06f);
        g.setColour (Colours::black.withAlpha (0.6f)); g.strokePath (p, juce::PathStrokeType (0.9f), xf);
    }

    void drawMini (Graphics& g, juce::Point<float> c, float R, float ang)
    {
        g.setGradientFill (ColourGradient (Colour (0xff3c3e42), c.x - R * 0.5f, c.y - R * 0.7f, Colour (0xff0b0c0d), c.x + R * 0.5f, c.y + R * 0.8f, false));
        g.fillEllipse (c.x - R, c.y - R, R * 2, R * 2);
        g.setColour (Colours::black); g.drawEllipse (c.x - R, c.y - R, R * 2, R * 2, 1.0f);
        g.setColour (Colour (0xffe8e4da)); g.drawLine ({ polar (c, R * 0.25f, ang), polar (c, R * 0.9f, ang) }, 2.2f);
    }
}

Look::Look()
{
    setColour (juce::PopupMenu::backgroundColourId, Colour (0xff1a1c1e));
    setColour (juce::PopupMenu::textColourId, silkCol);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, panelCol.brighter (0.2f));
    setColour (juce::PopupMenu::headerTextColourId, Colour (0xff9fb3c0));
    setColour (juce::TooltipWindow::backgroundColourId, Colour (0xff1a1c1e));
    setColour (juce::TooltipWindow::textColourId, silkCol);
}
void Look::drawBubble (Graphics& g, juce::BubbleComponent&, const juce::Point<float>&, const Rectangle<float>& body)
{
    g.setColour (Colour (0xf01a1c1e)); g.fillRoundedRectangle (body, 4);
    g.setColour (silkCol.withAlpha (0.5f)); g.drawRoundedRectangle (body.reduced (0.5f), 4, 1.0f);
}
void Look::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const juce::Point<float> c ((float) x + (float) w * 0.5f, (float) y + (float) h * 0.5f);
    const auto kind = (KnobKind) (int) s.getProperties()["kind"];
    const float half = (float) std::min (w, h) * 0.5f;
    if (kind == KnobKind::skirt) drawSkirt (g, c, half / kSkirt, a0 + pos * (a1 - a0));
    else if (kind == KnobKind::mini) drawMini (g, c, half - 2.0f, a0 + pos * (a1 - a0));
    else
    {
        const int n = (int) s.getMaximum() + 1, idx = juce::jlimit (0, n - 1, (int) std::round (s.getValue()));
        const float* tbl = n == 4 ? cpsA : n == 7 ? kcsA : selA;
        if (kind == KnobKind::bar) drawBar (g, c, half / 1.25f, tbl[idx] * deg);
        else drawPeak (g, c, half / 1.25f, tbl[idx] * deg);
    }
}

Knob::Knob (APVTS& s, const juce::String& id, KnobKind k, float a0, float a1) : kind (k)
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (a0, a1, true);
    setMouseDragSensitivity (k == KnobKind::bar || k == KnobKind::pointer ? 120 : 260);
    getProperties().set ("kind", (int) k);
    setPopupDisplayEnabled (k == KnobKind::skirt || k == KnobKind::mini, true, nullptr, 900);
    if (auto* p = s.getParameter (id)) setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    att = std::make_unique<APVTS::SliderAttachment> (s, id, *this);
}

void Toggle::paint (Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    const juce::Point<float> c (r.getCentreX(), r.getCentreY());
    const bool on = param (proc, pid) > 0.5f;
    Path hex; hex.addPolygon (c, 6, 17.0f, 0.0f);
    g.setColour (Colours::black.withAlpha (0.3f)); g.fillPath (hex, juce::AffineTransform::translation (1.5f, 3));
    g.setGradientFill (ColourGradient (Colour (0xfff5f6f7), c.x - 12, c.y - 14, Colour (0xff666a70), c.x + 12, c.y + 14, false)); g.fillPath (hex);
    g.setColour (Colour (0xff50545a)); g.strokePath (hex, juce::PathStrokeType (0.9f));
    for (int i = 0; i < 6; ++i) { g.setColour (Colours::black.withAlpha (0.12f)); g.drawLine ({ c, polar (c, 17.0f, (float) i * pi / 3.0f) }, 0.5f); }
    g.setGradientFill (ColourGradient (Colour (0xffe4e6e8), c.x - 8, c.y - 8, Colour (0xff4a4e54), c.x + 8, c.y + 8, false)); g.fillEllipse (c.x - 10, c.y - 10, 20, 20);
    g.setColour (Colours::black.withAlpha (0.5f)); g.drawEllipse (c.x - 10, c.y - 10, 20, 20, 0.9f);
    const float dir = on ? -1.0f : 1.0f;
    const juce::Point<float> tip (c.x, c.y + dir * 15.0f);
    g.setGradientFill (ColourGradient (Colour (0xffdfe1e3), c.x - 3, c.y, Colour (0xff5d6167), c.x + 3, c.y, false));
    g.fillRect (Rectangle<float> (c.x - 3.0f, std::min (c.y, tip.y), 6.0f, std::abs (tip.y - c.y)));
    g.setColour (Colours::black.withAlpha (0.35f)); g.fillEllipse (tip.x - 8.5f, tip.y - 7.0f, 18, 18);
    g.setGradientFill (ColourGradient (Colour (0xff4a4a4c), tip.x - 3, tip.y - 4, Colour (0xff020202), tip.x + 5, tip.y + 6, true));
    g.fillEllipse (tip.x - 9, tip.y - 9, 18, 18);
    g.setColour (Colours::white.withAlpha (0.45f)); g.fillEllipse (tip.x - 5, tip.y - 6, 5, 3.5f);
}
void Toggle::mouseUp (const juce::MouseEvent&) { setParam (proc, pid, param (proc, pid) > 0.5f ? 0.0f : 1.0f); repaint(); }

void PowerKnob::paint (Graphics& g)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    const float R = 27.0f, ang = (param (proc, "power") > 0.5f ? 38.0f : -18.0f) * deg;
    dropShadow (g, c, R, R * 0.14f);
    Path tab; tab.addTriangle (-6.0f, -R + 4, 6.0f, -R + 4, 0.0f, -R - 9);
    g.setColour (Colour (0xff070707)); g.fillPath (tab, juce::AffineTransform::rotation (ang).translated (c.x, c.y));
    g.setGradientFill (ColourGradient (Colour (0xff2c2c2d), c.x - R * 0.3f, c.y - R * 0.6f, Colour (0xff030303), c.x + R * 0.3f, c.y + R, true));
    g.fillEllipse (c.x - R, c.y - R, R * 2, R * 2);
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.28f), c.x, c.y - R, Colours::transparentWhite, c.x, c.y - R * 0.2f, false));
    g.drawEllipse (c.x - R + 1, c.y - R + 1, R * 2 - 2, R * 2 - 2, 1.6f);
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.10f), c.x - R * 0.4f, c.y - R * 0.5f, Colours::transparentWhite, c.x, c.y, true));
    g.fillEllipse (c.x - R * 0.8f, c.y - R * 0.8f, R * 1.2f, R * 0.9f);
    g.setColour (Colours::black); g.drawEllipse (c.x - R, c.y - R, R * 2, R * 2, 1.0f);
}
void PowerKnob::mouseUp (const juce::MouseEvent&) { setParam (proc, "power", param (proc, "power") > 0.5f ? 0.0f : 1.0f); repaint(); }

void Jewel::paint (Graphics& g)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    const float Rb = 30.0f, Rj = 21.0f;
    const bool on = param (proc, "power") > 0.5f;
    if (on)
    {
        g.setGradientFill (ColourGradient (Colour (0x66ff3010), c.x, c.y, Colour (0x00ff3010), c.x + Rb * 2.0f, c.y, true));
        g.fillEllipse (c.x - Rb * 2, c.y - Rb * 2, Rb * 4, Rb * 4);
    }
    dropShadow (g, c, Rb, 3.0f);
    g.setGradientFill (ColourGradient (Colour (0xfffafbfb), c.x - Rb * 0.6f, c.y - Rb * 0.8f, Colour (0xff595d63), c.x + Rb * 0.6f, c.y + Rb, false));
    g.fillEllipse (c.x - Rb, c.y - Rb, Rb * 2, Rb * 2);
    g.setGradientFill (ColourGradient (Colour (0xff5d6167), c.x - Rb * 0.6f, c.y - Rb * 0.8f, Colour (0xfff2f3f4), c.x + Rb * 0.6f, c.y + Rb, false));
    g.fillEllipse (c.x - Rb * 0.82f, c.y - Rb * 0.82f, Rb * 1.64f, Rb * 1.64f);
    g.setColour (Colour (0xff3e4247)); g.drawEllipse (c.x - Rb, c.y - Rb, Rb * 2, Rb * 2, 0.9f);
    g.setGradientFill (ColourGradient (on ? Colour (0xffff4a1e) : Colour (0xff5a1410), c.x, c.y, on ? Colour (0xffa80c06) : Colour (0xff250605), c.x + Rj, c.y, true));
    g.fillEllipse (c.x - Rj, c.y - Rj, Rj * 2, Rj * 2);
    juce::Random rnd (11);
    for (int i = 0; i < 70; ++i)
    {
        const float a = rnd.nextFloat() * twoPi, d = Rj * std::sqrt (rnd.nextFloat()) * 0.92f, s = 1.2f + rnd.nextFloat() * 2.4f;
        const auto p = polar (c, d, a);
        const Colour col = rnd.nextFloat() < 0.6f ? Colour (0xffffd24a) : Colour (0xffff8a2a);
        g.setColour (col.withAlpha (on ? 0.35f + 0.5f * rnd.nextFloat() : 0.05f)); g.fillEllipse (p.x - s * 0.5f, p.y - s * 0.5f, s, s * 0.8f);
    }
    g.setColour (Colours::black.withAlpha (0.5f)); g.drawEllipse (c.x - Rj, c.y - Rj, Rj * 2, Rj * 2, 1.0f);
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.6f), c.x - Rj * 0.4f, c.y - Rj * 0.5f, Colours::transparentWhite, c.x - Rj * 0.1f, c.y - Rj * 0.1f, true));
    g.fillEllipse (c.x - Rj * 0.75f, c.y - Rj * 0.8f, Rj * 0.7f, Rj * 0.5f);
}

Content::Content (TopEqProcessor& p)
    : proc (p),
      lowBoost (p.apvts, "lowBoost", KnobKind::skirt, A0, A1), lowAtten (p.apvts, "lowAtten", KnobKind::skirt, A0, A1),
      lowFreq (p.apvts, "lowFreq", KnobKind::bar, 0, 1),
      highBoost (p.apvts, "highBoost", KnobKind::skirt, A0, A1), bandwidth (p.apvts, "bandwidth", KnobKind::skirt, A0, A1),
      highFreq (p.apvts, "highFreq", KnobKind::bar, 0, 1),
      highAtten (p.apvts, "highAtten", KnobKind::skirt, A0, A1), attFreq (p.apvts, "attFreq", KnobKind::pointer, 0, 1),
      drive (p.apvts, "drive", KnobKind::mini, -pi * 0.75f, pi * 0.75f), out (p.apvts, "out", KnobKind::mini, -pi * 0.75f, pi * 0.75f),
      eqIn (p, "eqIn"), power (p), lamp (p)
{
    for (juce::Component* c : std::initializer_list<juce::Component*> { &lowBoost, &lowAtten, &lowFreq, &highBoost, &bandwidth, &highFreq, &highAtten, &attFreq, &drive, &out, &eqIn, &power, &lamp })
        addAndMakeVisible (c);
    lowBoost.setTooltip (U ("LOW BOOST: sube los graves en la frecuencia CPS (hasta +13.5 dB)"));
    lowAtten.setTooltip (U ("LOW ATTEN: baja los graves, un poco más arriba que el BOOST. Úsalos juntos para el truco: graves gordos sin barro"));
    lowFreq.setTooltip (U ("CPS: frecuencia de los graves (20, 30, 60, 100 Hz)"));
    highBoost.setTooltip (U ("HIGH BOOST: campana de agudos en la frecuencia KCS (hasta +18 dB)"));
    bandwidth.setTooltip (U ("BANDWIDTH: ancho de la campana (SHARP = angosta, BROAD = ancha y más suave)"));
    highFreq.setTooltip (U ("KCS: frecuencia del realce de agudos (3 a 16 kHz)"));
    highAtten.setTooltip (U ("HIGH ATTEN: baja los agudos en estante suave (hasta -16 dB)"));
    attFreq.setTooltip (U ("ATTEN SEL: frecuencia del corte de agudos (5, 10 o 20 kHz)"));
    drive.setTooltip (U ("DRIVE: cuánto empujas las válvulas y los transformadores (el volumen se compensa)"));
    out.setTooltip (U ("OUTPUT: volumen de salida (-12 a +12 dB)"));
    eqIn.setTooltip (U ("IN: mete o saca la ecualización sin cambiar el volumen (las válvulas siguen sonando)"));
    power.setTooltip (U ("OFF / ON: apaga todo el aparato (señal original)"));
    lamp.setTooltip (U ("Lámpara piloto"));
    buildPanel();
}

void Content::resized()
{
    auto place = [] (juce::Component& c, float cx, float cy, float d) { c.setBounds (juce::roundToInt (cx - d * 0.5f), juce::roundToInt (cy - d * 0.5f), juce::roundToInt (d), juce::roundToInt (d)); };
    const float dk = Rk * kSkirt * 2.0f, ds = 150.0f;
    place (lowBoost, xLB, yTop, dk); place (lowAtten, xLA, yTop, dk); place (highBoost, xHB, yTop, dk); place (highAtten, xHA, yTop, dk);
    place (bandwidth, xBW, yBot, dk);
    place (lowFreq, xCPS, yBot + 12, ds); place (highFreq, xKCS, yBot, ds); place (attFreq, xSEL, yTop + 4, 120);
    place (drive, 1262, 23, 32); place (out, 1412, 23, 32);
    place (eqIn, xIN, S + 318, 60);
    place (power, xPow, S + 321, 80);
    place (lamp, xLamp, S + 217, 130);
}

void Content::buildPanel()
{
    panel = juce::Image (juce::Image::ARGB, W, H, true);
    Graphics g (panel);
    g.setGradientFill (ColourGradient (Colour (0xff1e2124), 0, 0, Colour (0xff111315), 0, S, false)); g.fillRect (0.0f, 0.0f, (float) W, S);
    g.setFont (font (14, true).withExtraKerningFactor (0.08f)); g.setColour (Colour (0xffcfd8de));
    g.drawText ("Dj Pava Hm Music", Rectangle<float> (20, 0, 300, S), juce::Justification::centredLeft);
    g.setFont (font (11, true)); g.setColour (Colour (0xff9fb0bb));
    g.drawText ("DRIVE", Rectangle<float> (1170, 4, 72, 38), juce::Justification::centredRight);
    g.drawText ("OUTPUT", Rectangle<float> (1312, 4, 80, 38), juce::Justification::centredRight);

    const Rectangle<float> pan (0, S, (float) W, (float) H - S);
    g.setGradientFill (ColourGradient (panelCol.brighter (0.06f), 0, S, panelCol.darker (0.10f), 0, (float) H, false)); g.fillRect (pan);
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.035f), 0, S, Colours::transparentWhite, (float) W, (float) H, false)); g.fillRect (pan);
    {
        juce::Image::BitmapData bd (panel, 0, (int) S, W, H - (int) S, juce::Image::BitmapData::readWrite);
        juce::Random rnd (7);
        for (int y = 0; y < bd.height; ++y)
            for (int x = 0; x < bd.width; ++x)
                bd.setPixelColour (x, y, bd.getPixelColour (x, y).withMultipliedBrightness (1.0f + (rnd.nextFloat() - 0.5f) * 0.035f));
    }
    g.setColour (Colours::white.withAlpha (0.10f)); g.drawHorizontalLine ((int) S, 0, (float) W);
    g.setColour (Colours::black.withAlpha (0.45f)); g.drawHorizontalLine (H - 1, 0, (float) W);
    for (float y : { S + 121, S + 306 })
        for (bool left : { true, false })
        {
            const float x0 = left ? -20.0f : (float) W - 48.0f;
            Rectangle<float> slot (x0, y - 11, 68, 22);
            g.setColour (holeCol); g.fillRoundedRectangle (slot, 11);
            g.setColour (Colours::white.withAlpha (0.10f)); g.drawRoundedRectangle (slot.translated (0, 1), 11, 1.0f);
        }
    screw (g, 121, S + 83, 13, 0.62f); screw (g, 120, S + 345, 13, 0.30f); screw (g, 1434, S + 87, 13, 0.95f); screw (g, 1434, S + 350, 13, 0.40f);

    silk (g, "TOP", { 127, S + 157, 200, 26 }, 19);
    silk (g, "PROGRAM EQUALIZER", { 87, S + 181, 280, 26 }, 19);
    silk (g, "EQ-1A", { 127, S + 205, 200, 26 }, 19);
    silk (g, "IN", { xIN - 30, S + 260, 60, 24 }, 18);

    auto scale = [&g] (float cx, float cy, const char* title)
    {
        for (int i = 0; i <= 10; ++i)
        {
            const float a = A0 + (A1 - A0) * (float) i / 10.0f;
            const auto p = polar ({ cx, cy }, Rnum, a);
            silk (g, juce::String (i), { p.x - 16, p.y - 11, 32, 22 }, 17);
        }
        if (title != nullptr) silk (g, title, { cx - 90, S + 12, 180, 26 }, 18);
    };
    scale (xLB, yTop, "BOOST"); scale (xLA, yTop, "ATTEN"); scale (xHB, yTop, "BOOST"); scale (xHA, yTop, "ATTEN");
    scale (xBW, yBot, nullptr);
    silk (g, "SHARP", { xBW - 100, S + 377, 70, 18 }, 11.5f); silk (g, "BROAD", { xBW + 34, S + 377, 70, 18 }, 11.5f);
    auto labels = [&g] (float cx, float cy, const float* angs, std::initializer_list<const char*> ls, float r)
    {
        int i = 0;
        for (auto* l : ls) { const auto p = polar ({ cx, cy }, r, angs[i++] * deg); silk (g, l, { p.x - 24, p.y - 11, 48, 22 }, 17); }
    };
    labels (xCPS, yBot + 12, cpsA, { "20", "30", "60", "100" }, 82);
    labels (xKCS, yBot, kcsA, { "3", "4", "5", "8", "10", "12", "16" }, 74);
    labels (xSEL, yTop + 4, selA, { "5", "10", "20" }, 64);
    silk (g, "CPS", { xCPS - 40, S + 218, 80, 24 }, 18);
    silk (g, "KCS", { xKCS - 40, S + 220, 80, 24 }, 18);
    silk (g, "ATTEN SEL", { xSEL - 80, S + 14, 160, 26 }, 18);
    silk (g, "OFF", { xPow - 24, S + 266, 48, 22 }, 17);
    silk (g, "ON", { xPow + 22, S + 274, 40, 22 }, 17);
    silk (g, "LOW FREQUENCY", { xCPS - 140, S + 396, 280, 26 }, 18);
    silk (g, "BANDWIDTH", { xBW - 120, S + 396, 240, 26 }, 18);
    silk (g, "HIGH FREQUENCY", { xKCS - 140, S + 396, 280, 26 }, 18);
}

void Content::paint (Graphics& g)
{
    g.drawImageAt (panel, 0, 0);
    for (auto [r, t] : { std::pair<Rectangle<float>, const char*> { prevArea, "<" }, { nextArea, ">" } })
    {
        g.setGradientFill (ColourGradient (Colour (0xff2e3337), 0, r.getY(), Colour (0xff1b1e21), 0, r.getBottom(), false)); g.fillRoundedRectangle (r, 4);
        g.setColour (Colours::black); g.drawRoundedRectangle (r, 4, 1.0f);
        Path tri; const auto c = r.getCentre(); const float s = t[0] == '<' ? -1.0f : 1.0f;
        tri.addTriangle (c.x - 4 * s, c.y - 6, c.x - 4 * s, c.y + 6, c.x + 5 * s, c.y);
        g.setColour (Colour (0xffcfd8de)); g.fillPath (tri);
    }
    g.setGradientFill (ColourGradient (Colour (0xff0a0b0c), 0, presetArea.getY(), Colour (0xff181a1c), 0, presetArea.getBottom(), false)); g.fillRoundedRectangle (presetArea, 4);
    g.setColour (Colours::black); g.drawRoundedRectangle (presetArea, 4, 1.0f);
    g.setFont (font (14, true)); g.setColour (Colour (0xffe8edf0));
    g.drawText (proc.getProgramName (proc.getCurrentProgram()), presetArea, juce::Justification::centred);
    g.setFont (font (11)); g.setColour (Colour (0xff9fb0bb));
    g.drawText (juce::String (param (proc, "drive"), 1) + " dB", Rectangle<float> (1282, 4, 56, 38), juce::Justification::centredLeft);
    g.drawText (juce::String (param (proc, "out"), 1) + " dB", Rectangle<float> (1432, 4, 60, 38), juce::Justification::centredLeft);
}

void Content::mouseUp (const juce::MouseEvent& e)
{
    const int n = proc.getNumPrograms();
    if (prevArea.contains (e.position)) { proc.setCurrentProgram ((proc.getCurrentProgram() + n - 1) % n); repaint(); return; }
    if (nextArea.contains (e.position)) { proc.setCurrentProgram ((proc.getCurrentProgram() + 1) % n); repaint(); return; }
    if (! presetArea.contains (e.position)) return;
    juce::PopupMenu m; m.addSectionHeader ("PRESETS");
    for (int i = 0; i < n; ++i) m.addItem (i + 1, proc.getProgramName (i), true, i == proc.getCurrentProgram());
    juce::Component::SafePointer<Content> sp (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (presetArea.toNearestInt())), [sp] (int r)
    { if (sp != nullptr && r > 0) { sp->proc.setCurrentProgram (r - 1); sp->repaint(); } });
}

void Content::tick()
{
    const int prog = proc.getCurrentProgram();
    const float pw = param (proc, "power"), in = param (proc, "eqIn"), dr = param (proc, "drive"), o = param (proc, "out");
    if (pw != lastPower) { lamp.repaint(); power.repaint(); }
    if (in != lastIn) eqIn.repaint();
    if (prog != lastProgram || dr != lastDrive || o != lastOut) repaint (Rectangle<int> (540, 0, 1000, (int) S));
    lastProgram = prog; lastPower = pw; lastIn = in; lastDrive = dr; lastOut = o;
}

TopEqEditor::TopEqEditor (TopEqProcessor& p) : AudioProcessorEditor (p), content (p)
{
    setLookAndFeel (&look);
    addAndMakeVisible (content);
    content.setBounds (0, 0, W, H);
    setResizable (true, true);
    setResizeLimits (936, 307, 2340, 768);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) W / (double) H);
    setSize (1170, 384);
    startTimerHz (30);
}
TopEqEditor::~TopEqEditor() { setLookAndFeel (nullptr); }
void TopEqEditor::resized() { content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) W)); }
