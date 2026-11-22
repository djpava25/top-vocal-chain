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

    // ---------- microfonos ----------
    void metal (Graphics& g, const Path& p, Colour base, float shine = 1.0f)
    {
        const auto r = p.getBounds();
        ColourGradient gr (base.darker (1.1f), r.getX(), 0, base.darker (1.0f), r.getRight(), 0, false);
        gr.addColour (0.10, base.darker (0.35f));
        gr.addColour (0.27, base.brighter (0.65f * shine));
        gr.addColour (0.38, base.brighter (0.18f));
        gr.addColour (0.62, base);
        gr.addColour (0.84, base.darker (0.55f));
        g.setGradientFill (gr); g.fillPath (p);
        g.setColour (Colours::black.withAlpha (0.55f)); g.strokePath (p, juce::PathStrokeType (0.7f));
    }
    void mesh (Graphics& g, const Path& p, Colour base, float u)
    {
        const auto r = p.getBounds();
        g.saveState(); g.reduceClipRegion (p);
        g.setColour (base.darker (1.4f)); g.fillRect (r);
        const float s = std::max (1.5f, 2.3f * u);
        g.setColour (base.withAlpha (0.85f));
        for (float x = r.getX() - r.getHeight(); x < r.getRight(); x += s)
        {
            g.drawLine (x, r.getY(), x + r.getHeight(), r.getBottom(), 0.5f * u);
            g.drawLine (x + r.getHeight(), r.getY(), x, r.getBottom(), 0.5f * u);
        }
        ColourGradient sh (Colours::black.withAlpha (0.8f), r.getX(), 0, Colours::black.withAlpha (0.75f), r.getRight(), 0, false);
        sh.addColour (0.28, Colours::white.withAlpha (0.32f)); sh.addColour (0.42, Colours::transparentWhite); sh.addColour (0.7, Colours::black.withAlpha (0.2f));
        g.setGradientFill (sh); g.fillRect (r);
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.22f), 0, r.getY(), Colours::transparentWhite, 0, r.getY() + r.getHeight() * 0.45f, false));
        g.fillRect (r);
        g.restoreState();
        g.setColour (Colours::black.withAlpha (0.6f)); g.strokePath (p, juce::PathStrokeType (0.8f * u));
    }
    void band (Graphics& g, float cx, float y, float w, float h, Colour c, float rad = 1.0f)
    {
        Path p; p.addRoundedRectangle (cx - w * 0.5f, y, w, h, rad); metal (g, p, c, 1.1f);
    }
    Path trapezoid (float cx, float y0, float y1, float w0, float w1, float rad)
    {
        Path p; p.startNewSubPath (cx - w0 * 0.5f, y0); p.lineTo (cx + w0 * 0.5f, y0); p.lineTo (cx + w1 * 0.5f, y1 - rad);
        p.quadraticTo (cx + w1 * 0.5f, y1, cx + w1 * 0.5f - rad, y1); p.lineTo (cx - w1 * 0.5f + rad, y1);
        p.quadraticTo (cx - w1 * 0.5f, y1, cx - w1 * 0.5f, y1 - rad); p.closeSubPath(); return p;
    }
    Path head (float cx, float y, float w, float h, float topR)
    {
        Path p; p.addRoundedRectangle (cx - w * 0.5f, y, w, h, topR, topR, true, true, false, false); return p;
    }
    void spider (Graphics& g, float cx, float yT, float yB, float inner, float outer, float u, bool front)
    {
        const Colour m (0xffb9bcc2);
        if (! front)
        {
            g.setColour (m.darker (0.8f));
            g.drawEllipse (cx - outer * 0.5f, yT - 3 * u, outer, 6 * u, 1.8f * u);
            g.drawEllipse (cx - outer * 0.5f, yB - 3 * u, outer, 6 * u, 1.8f * u);
            return;
        }
        auto tube = [&] (float x0, float y0, float x1, float y1, float th)
        {
            g.setColour (Colours::black.withAlpha (0.6f)); g.drawLine (x0, y0 + 1, x1, y1 + 1, th + 0.8f);
            g.setColour (m); g.drawLine (x0, y0, x1, y1, th);
            g.setColour (Colours::white.withAlpha (0.6f)); g.drawLine (x0, y0 - th * 0.25f, x1, y1 - th * 0.25f, th * 0.3f);
        };
        // aros delanteros
        Path a; a.addCentredArc (cx, yT, outer * 0.5f, 3 * u, 0, juce::MathConstants<float>::halfPi, juce::MathConstants<float>::pi + juce::MathConstants<float>::halfPi, true);
        Path b; b.addCentredArc (cx, yB, outer * 0.5f, 3 * u, 0, juce::MathConstants<float>::halfPi, juce::MathConstants<float>::pi + juce::MathConstants<float>::halfPi, true);
        g.setColour (m); g.strokePath (a, juce::PathStrokeType (2.2f * u)); g.strokePath (b, juce::PathStrokeType (2.2f * u));
        // brazos en X
        for (float s : { -1.0f, 1.0f })
        {
            tube (cx + s * outer * 0.5f, yT, cx + s * inner * 0.5f, yB, 1.6f * u);
            tube (cx + s * inner * 0.5f, yT, cx + s * outer * 0.5f, yB, 1.6f * u);
            g.setColour (Colour (0xffc0392b).withAlpha (0.0f));
        }
        // elasticos
        g.setColour (Colour (0xff1b1b1b));
        for (float s : { -1.0f, 1.0f })
        {
            g.drawLine (cx + s * outer * 0.5f, yT + 2 * u, cx + s * inner * 0.5f, yT + 8 * u, 0.8f * u);
            g.drawLine (cx + s * outer * 0.5f, yB - 2 * u, cx + s * inner * 0.5f, yB - 8 * u, 0.8f * u);
        }
        // pie
        band (g, cx, yB + 3 * u, 7 * u, 22 * u, Colour (0xff6d7076));
    }
    void badge (Graphics& g, float cx, float cy, float r, Colour c)
    {
        g.setColour (Colours::black.withAlpha (0.6f)); g.fillEllipse (cx - r - 0.5f, cy - r + 0.8f, r * 2 + 1, r * 2 + 1);
        g.setGradientFill (ColourGradient (c.brighter (0.6f), cx - r, cy - r, c.darker (0.7f), cx + r, cy + r, false));
        g.fillEllipse (cx - r, cy - r, r * 2, r * 2);
        g.setColour (Colours::white.withAlpha (0.7f)); g.drawEllipse (cx - r * 0.55f, cy - r * 0.55f, r * 1.1f, r * 1.1f, 0.7f);
    }
    struct Cond { float hw, hh, topR, bw0, bw1, bh; Colour meshC, bodyC; int rings; bool spider; Colour badgeC; bool fins = false, display = false; float neck = 5.0f; };
    void condenser (Graphics& g, float cx, float yb, float u, const Cond& c)
    {
        const float stand = 16 * u, y1 = yb - stand, y0 = y1 - c.bh * u, yh = y0 - c.neck * u - c.hh * u;
        if (c.spider) spider (g, cx, y0 - 2 * u, y0 + c.bh * u * 0.62f, c.bw0 * u, (c.hw + 26) * u, u, false);
        band (g, cx, y1 - 2 * u, 9 * u, stand + 2 * u, Colour (0xff4a4c50));
        Path body = trapezoid (cx, y0, y1, c.bw0 * u, c.bw1 * u, 3 * u);
        metal (g, body, c.bodyC);
        if (c.fins)
            for (int i = 0; i < 7; ++i)
            {
                const float y = y1 - (8 + i * 5.0f) * u;
                g.setColour (Colours::black.withAlpha (0.45f)); g.drawHorizontalLine ((int) y, cx - c.bw1 * u * 0.48f, cx + c.bw1 * u * 0.48f);
                g.setColour (Colours::white.withAlpha (0.15f)); g.drawHorizontalLine ((int) y + 1, cx - c.bw1 * u * 0.48f, cx + c.bw1 * u * 0.48f);
            }
        if (c.display)
        {
            Rectangle<float> d (cx - 5 * u, y0 + 10 * u, 10 * u, 5 * u);
            g.setColour (Colour (0xff101418)); g.fillRoundedRectangle (d, 1);
            g.setColour (Colour (0xff6fc3ff).withAlpha (0.8f)); g.fillRect (d.reduced (2 * u, 1.6f * u));
            g.setColour (Colours::white.withAlpha (0.5f)); g.drawEllipse (cx - 6 * u, y0 + 22 * u, 12 * u, 12 * u, 0.9f * u);
        }
        band (g, cx, y0 - c.neck * u, std::max (c.bw0, c.hw * 0.92f) * u, c.neck * u + 1, c.bodyC.brighter (0.1f));
        Path h = head (cx, yh, c.hw * u, c.hh * u, c.topR * u);
        mesh (g, h, c.meshC, u);
        for (int i = 1; i <= c.rings; ++i)
            band (g, cx, yh + c.hh * u * (float) i / (float) (c.rings + 1) - 1.2f * u, c.hw * u + 1.0f, 2.4f * u, c.bodyC.brighter (0.2f));
        if (! c.badgeC.isTransparent()) badge (g, cx, y0 + 7 * u, 4.2f * u, c.badgeC);
        if (c.spider) spider (g, cx, y0 - 2 * u, y0 + c.bh * u * 0.62f, c.bw0 * u, (c.hw + 26) * u, u, true);
    }
}

void tlui::drawMic (Graphics& g, Rectangle<float> r, int idx)
{
    const float u = std::min (r.getWidth() / 100.0f, r.getHeight() / 230.0f);
    const float cx = r.getCentreX(), yb = r.getBottom();
    // sombra en el piso
    g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.75f), cx, yb, Colours::transparentBlack, cx + 40 * u, yb, true));
    g.fillEllipse (cx - 40 * u, yb - 6 * u, 80 * u, 12 * u);
    const Colour silver (0xffc3c6cc), nickel (0xffb7b4aa), champagne (0xffcdbf9c), black (0xff2a2b2e), gold (0xffd8ab45);
    switch (idx)
    {
        case 0: condenser (g, cx, yb, u, { 46, 72, 23, 40, 33, 98, Colour (0xffd2d4d8), silver, 0, true, Colour (0xff2a2d33) }); break;
        case 1: condenser (g, cx, yb, u, { 44, 56, 22, 40, 38, 84, Colour (0xff5a5d63), silver, 0, false, Colours::transparentBlack, false, false, 4 }); break;
        case 2:
        {
            Cond c { 44, 74, 9, 40, 36, 96, gold, black, 0, false, Colours::transparentBlack, false, true, 6 };
            condenser (g, cx, yb, u, c);
            // marco negro alrededor de la rejilla
            const float yh = yb - 16 * u - 96 * u - 6 * u - 74 * u;
            Path f; f.addRoundedRectangle (cx - 22 * u, yh, 44 * u, 74 * u, 9 * u, 9 * u, true, true, false, false);
            g.setColour (Colour (0xff141416)); g.strokePath (f, juce::PathStrokeType (2.2f * u));
            break;
        }
        case 3: condenser (g, cx, yb, u, { 50, 80, 25, 46, 44, 104, Colour (0xffd8cdb0), champagne, 0, true, Colour (0xff1e1e22), true }); break;
        case 4: condenser (g, cx, yb, u, { 42, 76, 8, 38, 36, 94, Colour (0xffcfd2d6), silver, 2, false, Colour (0xff3d4e6a) }); break;
        case 5:
        {
            condenser (g, cx, yb, u, { 44, 72, 22, 42, 40, 98, Colour (0xffcfd2d6), black, 1, false, Colours::transparentBlack, false, false, 6 });
            const float y0 = yb - 16 * u - 98 * u;
            Rectangle<float> plate (cx - 13 * u, y0 + 30 * u, 26 * u, 8 * u);
            g.setColour (Colour (0xff0c0c0d)); g.fillRoundedRectangle (plate, 1.5f);
            g.setColour (silver); g.drawRoundedRectangle (plate, 1.5f, 0.6f * u);
            g.setFont (font (6.0f * u, true)); g.drawText ("REF", plate, juce::Justification::centred);
            band (g, cx, y0 + 88 * u, 42 * u, 6 * u, silver);
            break;
        }
        case 6:   // dinamico de radio con horquilla
        {
            const float pivX = cx, pivY = yb - 110 * u;
            band (g, cx, pivY, 8 * u, yb - pivY - 4 * u, Colour (0xff3a3b3f));
            g.saveState();
            g.addTransform (juce::AffineTransform::rotation (-0.42f, pivX, pivY));
            Path bodyP; bodyP.addRoundedRectangle (pivX - 18 * u, pivY - 38 * u, 36 * u, 82 * u, 9 * u);
            metal (g, bodyP, black, 0.6f);
            for (int i = 0; i < 6; ++i) { g.setColour (Colours::black.withAlpha (0.4f)); g.drawHorizontalLine ((int) (pivY + (8 + i * 5) * u), pivX - 17 * u, pivX + 17 * u); }
            // espuma
            Path foam; foam.addRoundedRectangle (pivX - 24 * u, pivY - 112 * u, 48 * u, 80 * u, 22 * u);
            g.setGradientFill (ColourGradient (Colour (0xff3b3c40), pivX - 24 * u, 0, Colour (0xff09090a), pivX + 24 * u, 0, false));
            g.fillPath (foam);
            juce::Random rnd (7);
            g.saveState(); g.reduceClipRegion (foam);
            for (int i = 0; i < 500; ++i) { g.setColour (Colours::white.withAlpha (0.05f * rnd.nextFloat())); g.fillEllipse (pivX - 24 * u + rnd.nextFloat() * 48 * u, pivY - 112 * u + rnd.nextFloat() * 80 * u, 1.2f * u, 1.2f * u); }
            g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.18f), pivX - 10 * u, pivY - 100 * u, Colours::transparentWhite, pivX + 10 * u, pivY - 60 * u, true));
            g.fillPath (foam);
            g.restoreState();
            g.setColour (Colours::black); g.strokePath (foam, juce::PathStrokeType (0.8f * u));
            g.restoreState();
            // horquilla
            Path yoke; yoke.startNewSubPath (cx - 26 * u, pivY - 18 * u); yoke.lineTo (cx - 26 * u, pivY + 14 * u); yoke.quadraticTo (cx - 26 * u, pivY + 26 * u, cx, pivY + 26 * u);
            yoke.quadraticTo (cx + 26 * u, pivY + 26 * u, cx + 26 * u, pivY + 14 * u); yoke.lineTo (cx + 26 * u, pivY - 18 * u);
            g.setColour (Colour (0xff111113)); g.strokePath (yoke, juce::PathStrokeType (5 * u));
            g.setColour (Colours::white.withAlpha (0.18f)); g.strokePath (yoke, juce::PathStrokeType (1.2f * u));
            g.setColour (Colour (0xff6b6d72)); g.fillEllipse (cx + 20 * u, pivY - 12 * u, 12 * u, 12 * u);
            break;
        }
        case 7:   // cinta moderna, delgada con ventana
        {
            band (g, cx, yb - 18 * u, 9 * u, 18 * u, Colour (0xff4a4c50));
            Path b; b.addRoundedRectangle (cx - 14 * u, yb - 178 * u, 28 * u, 162 * u, 12 * u);
            metal (g, b, silver);
            Path win; win.addRoundedRectangle (cx - 8 * u, yb - 168 * u, 16 * u, 92 * u, 7 * u);
            mesh (g, win, Colour (0xffd5d7dc), u);
            g.setColour (Colour (0xff1a1a1a).withAlpha (0.7f)); g.fillRect (cx - 1.2f * u, yb - 162 * u, 2.4f * u, 80 * u);
            for (int i = 0; i < 3; ++i) band (g, cx, yb - (64 - i * 6) * u, 28 * u, 2.2f * u, silver.darker (0.2f));
            Rectangle<float> lbl (cx - 9 * u, yb - 48 * u, 18 * u, 6 * u);
            g.setColour (Colour (0xff121212)); g.fillRect (lbl);
            break;
        }
        case 8:   // cinta britanica: cabeza cuadrada en pie
        {
            band (g, cx, yb - 70 * u, 8 * u, 70 * u, Colour (0xff3e4044));
            Path neck; neck.addRoundedRectangle (cx - 12 * u, yb - 120 * u, 24 * u, 56 * u, 5 * u);
            metal (g, neck, silver);
            Path h; h.addRoundedRectangle (cx - 30 * u, yb - 190 * u, 60 * u, 66 * u, 20 * u);
            mesh (g, h, Colour (0xff6a6c70), u);
            g.setColour (silver); g.strokePath (h, juce::PathStrokeType (3.2f * u));
            g.setColour (Colours::white.withAlpha (0.5f)); g.strokePath (h, juce::PathStrokeType (0.9f * u), juce::AffineTransform::translation (-0.6f * u, -0.6f * u));
            g.setColour (Colour (0xff8d9096)); g.fillEllipse (cx - 5 * u, yb - 128 * u, 10 * u, 10 * u);
            break;
        }
        case 9: condenser (g, cx, yb, u, { 40, 64, 20, 30, 28, 116, Colour (0xffdcd3ba), Colour (0xffd2c39a), 0, false, Colour (0xff8c6d2c), false, false, 7 }); break;
        case 10: condenser (g, cx, yb, u, { 54, 84, 27, 46, 44, 86, Colour (0xffc7c4bb), nickel, 3, false, Colour (0xff4b4b4b), false, false, 6 }); break;
        case 11:  // dinamico de locucion: cilindro largo
        {
            band (g, cx + 22 * u, yb - 96 * u, 7 * u, 96 * u, Colour (0xff3e4044));
            Path b; b.addRoundedRectangle (cx - 17 * u, yb - 200 * u, 34 * u, 170 * u, 6 * u);
            metal (g, b, Colour (0xffbab5a7));
            Path gr; gr.addRoundedRectangle (cx - 17 * u, yb - 200 * u, 34 * u, 66 * u, 6 * u, 6 * u, true, true, false, false);
            g.setColour (Colour (0xff1d1d1f)); g.fillPath (gr);
            for (int i = 0; i < 18; ++i) { g.setColour (Colours::white.withAlpha (0.12f)); g.drawHorizontalLine ((int) (yb - (196 - i * 3.5f) * u), cx - 15 * u, cx + 15 * u); }
            g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.2f), cx - 12 * u, 0, Colours::transparentWhite, cx + 4 * u, 0, false)); g.fillPath (gr);
            band (g, cx + 14 * u, yb - 100 * u, 18 * u, 8 * u, Colour (0xff55575c));
            break;
        }
        default: break;
    }
}

void tlui::drawRoom (Graphics& g, Rectangle<float> r, int room)
{
    g.saveState(); g.reduceClipRegion (r.toNearestInt());
    struct RS { Colour wall, floor, ceil; float depth; int style; };
    static const RS rs[] { { Colour (0xff0a0a0a), Colour (0xff0a0a0a), Colour (0xff0a0a0a), 0.5f, 0 },
                           { Colour (0xff3a3a3c), Colour (0xff2b221b), Colour (0xff202022), 0.62f, 1 },
                           { Colour (0xff5a3a22), Colour (0xff3a2a1d), Colour (0xff231a12), 0.5f, 2 },
                           { Colour (0xff6e5544), Colour (0xff4a3a2c), Colour (0xff2b2622), 0.38f, 3 },
                           { Colour (0xff3a3e48), Colour (0xff3a2c22), Colour (0xff1a1c22), 0.26f, 4 },
                           { Colour (0xff4a3020), Colour (0xff2e2016), Colour (0xff1e140e), 0.55f, 5 } };
    const auto& s = rs[juce::jlimit (0, 5, room)];
    g.setColour (s.ceil); g.fillRect (r);
    if (s.style == 0)
    {
        g.setColour (dim); g.setFont (font (16, true)); g.drawText ("SIN SALA (SECO)", r, juce::Justification::centred);
        g.restoreState(); return;
    }
    const float d = s.depth;
    Rectangle<float> back (r.getCentreX() - r.getWidth() * d * 0.5f, r.getY() + r.getHeight() * (0.5f - d * 0.45f), r.getWidth() * d, r.getHeight() * d * 0.9f);
    // piso
    Path fl; fl.startNewSubPath (r.getX(), r.getBottom()); fl.lineTo (back.getX(), back.getBottom()); fl.lineTo (back.getRight(), back.getBottom()); fl.lineTo (r.getRight(), r.getBottom()); fl.closeSubPath();
    g.setGradientFill (ColourGradient (s.floor.brighter (0.4f), 0, back.getBottom(), s.floor.darker (0.4f), 0, r.getBottom(), false)); g.fillPath (fl);
    for (int i = -6; i <= 6; ++i) { g.setColour (Colours::black.withAlpha (0.25f)); g.drawLine (back.getCentreX() + i * back.getWidth() / 12.0f, back.getBottom(), r.getCentreX() + i * r.getWidth() / 12.0f, r.getBottom(), 0.7f); }
    // paredes laterales
    for (int side = 0; side < 2; ++side)
    {
        Path w;
        if (side == 0) { w.startNewSubPath (r.getX(), r.getY()); w.lineTo (back.getX(), back.getY()); w.lineTo (back.getX(), back.getBottom()); w.lineTo (r.getX(), r.getBottom()); }
        else { w.startNewSubPath (r.getRight(), r.getY()); w.lineTo (back.getRight(), back.getY()); w.lineTo (back.getRight(), back.getBottom()); w.lineTo (r.getRight(), r.getBottom()); }
        w.closeSubPath();
        g.setColour (s.wall.darker (0.5f)); g.fillPath (w);
        g.saveState(); g.reduceClipRegion (w);
        for (int i = 0; i < 9; ++i)
        {
            const float t = (float) i / 9.0f, x0 = side == 0 ? r.getX() + (back.getX() - r.getX()) * t : r.getRight() - (r.getRight() - back.getRight()) * t;
            g.setColour (Colours::black.withAlpha (s.style == 1 ? 0.4f : 0.25f)); g.drawVerticalLine ((int) x0, r.getY(), r.getBottom());
        }
        g.restoreState();
    }
    // pared del fondo
    g.setGradientFill (ColourGradient (s.wall.brighter (0.25f), back.getCentreX(), back.getY(), s.wall.darker (0.3f), back.getCentreX(), back.getBottom(), false));
    g.fillRect (back);
    if (s.style == 1)   // espuma acustica
        for (float x = back.getX(); x < back.getRight(); x += 6) for (float y = back.getY(); y < back.getBottom(); y += 6)
        { g.setColour (Colours::black.withAlpha (((int) (x + y) / 6) % 2 ? 0.25f : 0.05f)); g.fillRect (x, y, 6.0f, 6.0f); }
    if (s.style == 2 || s.style == 5)   // listones de madera
        for (float x = back.getX(); x < back.getRight(); x += 5) { g.setColour (Colours::black.withAlpha (0.3f)); g.drawVerticalLine ((int) x, back.getY(), back.getBottom()); }
    if (s.style == 3)   // ladrillo
        for (float y = back.getY(); y < back.getBottom(); y += 5)
        {
            g.setColour (Colours::black.withAlpha (0.3f)); g.drawHorizontalLine ((int) y, back.getX(), back.getRight());
            for (float x = back.getX() + (((int) ((y - back.getY()) / 5)) % 2 ? 5.0f : 0.0f); x < back.getRight(); x += 10) g.drawVerticalLine ((int) x, y, y + 5);
        }
    if (s.style == 4)   // sala grande: ventanales y columnas
        for (int i = 0; i < 5; ++i) { g.setColour (Colour (0xff8fb4d8).withAlpha (0.25f)); g.fillRect (back.getX() + back.getWidth() * (0.08f + i * 0.19f), back.getY() + back.getHeight() * 0.15f, back.getWidth() * 0.1f, back.getHeight() * 0.55f); }
    // luces calidas
    for (int i = 0; i < 3; ++i)
    {
        const float lx = back.getX() + back.getWidth() * (0.2f + 0.3f * i);
        g.setGradientFill (ColourGradient (Colour (0xffffc070).withAlpha (0.45f), lx, r.getY() + 2, Colours::transparentWhite, lx, back.getBottom(), true));
        g.fillEllipse (lx - back.getWidth() * 0.2f, r.getY() - 10, back.getWidth() * 0.4f, (back.getBottom() - r.getY()) * 1.1f);
    }
    if (s.style == 5)   // lampara vintage
    {
        g.setColour (Colour (0xffffd28a)); g.fillEllipse (back.getRight() - 14, back.getBottom() - 30, 8, 8);
        g.setGradientFill (ColourGradient (Colour (0x66ffb050), back.getRight() - 10, back.getBottom() - 26, Colours::transparentWhite, back.getRight() + 20, back.getBottom(), true));
        g.fillRect (back.getRight() - 50, back.getBottom() - 60, 80.0f, 70.0f);
    }
    // microfono en el centro
    const float mx = back.getCentreX(), my = back.getBottom() + 8;
    g.setColour (Colour (0xff101010)); g.drawLine (mx, my, mx, my - back.getHeight() * 0.55f, 1.6f);
    g.drawLine (mx - 10, my + 3, mx + 10, my + 3, 1.6f);
    g.setColour (Colour (0xffc9ccd2)); g.fillRoundedRectangle (mx - 3.5f, my - back.getHeight() * 0.55f - 14, 7, 16, 3);
    // vineta
    g.setGradientFill (ColourGradient (Colours::transparentBlack, r.getCentreX(), r.getCentreY(), Colours::black.withAlpha (0.65f), r.getX(), r.getY(), true));
    g.fillRect (r);
    g.restoreState();
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
        if (type == 3 || type == 4)
        {
            Path p; p.startNewSubPath (cx - 11, cy); const float a = type == 3 ? 5.0f : 2.5f;
            for (int i = 0; i <= 22; ++i) p.lineTo (cx - 11 + (float) i, cy + a * std::sin ((float) i * 0.9f) * (type == 3 ? 1.0f - std::abs ((float) i - 11) / 12.0f : 1.0f));
            g.strokePath (p, juce::PathStrokeType (1.3f));
        }
    };
    const float ly = k * 0.82f;
    if (loIcon) icon (loIcon, x0 + 4, ly - 10);
    if (hiIcon) icon (hiIcon, x0 + k - 4, ly - 10);
    g.setFont (font (13.0f)); g.setColour (Colour (0xffd8d8db));
    g.drawText (lo, Rectangle<float> (x0 - 30, ly + (loIcon ? 0 : -6), 68, 16), juce::Justification::centred);
    g.drawText (hi, Rectangle<float> (x0 + k - 38, ly + (hiIcon ? 0 : -6), 68, 16), juce::Justification::centred);
    if (showValue && fmt)
    {
        Rectangle<float> vb (w * 0.5f - 42, k + 4, 84, 26);
        inset (g, vb);
        g.setFont (font (16.0f)); g.setColour (ink); g.drawText (fmt ((float) slider.getValue()), vb, juce::Justification::centred);
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
Rectangle<float> Carousel::cardRect (int slot, bool sel) const
{
    const int n = std::min (visible, (int) list().size());
    const float cw = 100, gap = 6, total = (float) n * cw + (float) (n - 1) * gap, x0 = ((float) getWidth() - total) * 0.5f;
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
    // estudio de fondo: madera, luces calidas y piso brillante
    const float floorY = r.getHeight() * 0.74f;
    g.setGradientFill (ColourGradient (Colour (0xff3a2415), 0, 0, Colour (0xff120a06), 0, floorY, false)); g.fillRect (r.withBottom (floorY));
    for (float x = 0; x < r.getWidth(); x += 13)
    {
        g.setColour (Colours::black.withAlpha (0.35f)); g.drawVerticalLine ((int) x, 40, floorY);
        g.setColour (Colour (0xffd08850).withAlpha (0.05f)); g.drawVerticalLine ((int) x + 2, 40, floorY);
    }
    g.setColour (Colour (0xff0d0805)); g.fillRect (0.0f, 0.0f, r.getWidth(), 40.0f);
    for (int i = 0; i < 7; ++i)
    {
        const float lx = r.getWidth() * (0.08f + 0.14f * (float) i);
        g.setGradientFill (ColourGradient (Colour (0xffffb35c).withAlpha (0.5f), lx, 40, Colours::transparentBlack, lx, 230, true));
        g.fillEllipse (lx - 90, -60, 180, 300);
        g.setColour (Colour (0xffffe0b0)); g.fillEllipse (lx - 9, 34, 18, 6);
    }
    g.setGradientFill (ColourGradient (Colour (0xff4a2e1c), 0, floorY, Colour (0xff0e0805), 0, r.getBottom(), false)); g.fillRect (r.withTop (floorY));
    g.setGradientFill (ColourGradient (Colour (0x55ffc080), r.getCentreX(), floorY, Colours::transparentBlack, r.getCentreX() + 400, floorY, true));
    g.fillRect (r.withTop (floorY).withHeight (6));

    const auto v = list(); const int sel = (int) param (proc, "mic");
    const int n = std::min (visible, (int) v.size() - first);
    for (int pass = 0; pass < 2; ++pass)
        for (int s = 0; s < n; ++s)
        {
            const int idx = v[(size_t) (first + s)];
            const bool isSel = idx == sel;
            if ((pass == 1) != isSel) continue;      // la seleccionada al final (encima)
            auto c = cardRect (s, isSel);
            if (isSel) for (int k = 6; k > 0; --k) { g.setColour (blue.withAlpha (0.06f)); g.drawRoundedRectangle (c.expanded ((float) k), 8 + (float) k, 2.0f); }
            g.setGradientFill (ColourGradient (Colour (isSel ? 0x55142a48 : 0x40000000), 0, c.getY(), Colour (0xcc050505), 0, c.getBottom(), false));
            g.fillRoundedRectangle (c, 7);
            // microfono con reflejo en el piso
            auto micArea = Rectangle<float> (c.getX() + 4, c.getY() + 8, c.getWidth() - 8, c.getHeight() - 96);
            const float fy = micArea.getBottom();
            g.saveState(); g.reduceClipRegion (c.toNearestInt());
            g.beginTransparencyLayer (0.2f);
            g.addTransform (juce::AffineTransform::scale (1.0f, -1.0f).translated (0, 2 * fy));
            drawMic (g, micArea, idx);
            g.endTransparencyLayer();
            g.restoreState();
            g.setGradientFill (ColourGradient (Colours::transparentBlack, 0, fy, Colour (0xf0080605), 0, fy + 50, false));
            g.fillRect (c.withTop (fy).withBottom (c.getBottom() - 72).reduced (1, 0));
            drawMic (g, micArea, idx);
            // nombre
            auto lab = c.removeFromBottom (74).reduced (4, 4);
            g.setGradientFill (ColourGradient (Colour (0xff1e1e20), 0, lab.getY(), Colour (0xff0b0b0c), 0, lab.getBottom(), false)); g.fillRoundedRectangle (lab, 4);
            g.setColour (Colours::white.withAlpha (0.08f)); g.drawRoundedRectangle (lab, 4, 1.0f);
            const auto& m = lk::mics()[(size_t) idx];
            g.setFont (font (isSel ? 17.0f : 15.5f, isSel)); g.setColour (isSel ? Colours::white : Colour (0xffd8d8dc));
            g.drawText (m.line1, Rectangle<float> (lab.getX(), lab.getY() + 4, lab.getWidth(), 19), juce::Justification::centred);
            g.drawText (m.line2, Rectangle<float> (lab.getX(), lab.getY() + 22, lab.getWidth(), 19), juce::Justification::centred);
            // a que microfono imita
            auto rb = Rectangle<float> (lab.getX() + 4, lab.getBottom() - 22, lab.getWidth() - 8, 17);
            g.setColour (blue.withAlpha (isSel ? 0.35f : 0.18f)); g.fillRoundedRectangle (rb, 3);
            g.setFont (font (11.5f, true)); g.setColour (isSel ? blueHi.brighter (0.3f) : Colour (0xff9cc6f2));
            g.drawFittedText (m.ref, rb.toNearestInt(), juce::Justification::centred, 1, 0.75f);
            if (isSel) { g.setColour (blueHi); g.drawRoundedRectangle (cardRect (s, true), 7, 2.2f); }
            else { g.setColour (Colours::white.withAlpha (0.1f)); g.drawRoundedRectangle (cardRect (s, false), 7, 1.0f); }
        }
    // vineta
    g.setGradientFill (ColourGradient (Colours::transparentBlack, r.getCentreX(), r.getCentreY(), Colours::black.withAlpha (0.45f), 0, 0, true)); g.fillRect (r);
    g.restoreState();
    g.setColour (Colours::black); g.drawRoundedRectangle (r.reduced (0.5f), 6, 1.2f);
}

// ---------------- Preamp ----------------
void PreampView::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const int p = (int) param (proc, "pre");
    g.setColour (Colours::black.withAlpha (0.7f)); g.fillRoundedRectangle (r.translated (0, 2), 6);
    g.setGradientFill (ColourGradient (Colour (0xff3a4250), 0, r.getY(), Colour (0xff171b22), 0, r.getBottom(), false)); g.fillRoundedRectangle (r, 6);
    juce::Random rnd (3);
    for (float y = r.getY() + 3; y < r.getBottom() - 3; y += 1.3f) { g.setColour (Colours::white.withAlpha (0.015f + 0.025f * rnd.nextFloat())); g.drawHorizontalLine ((int) y, r.getX() + 4, r.getRight() - 4); }
    g.setColour (Colours::white.withAlpha (0.15f)); g.drawRoundedRectangle (r.reduced (4), 4, 1.0f);
    g.setColour (Colours::black); g.drawRoundedRectangle (r, 6, 1.5f);
    for (auto pt : { r.getTopLeft() + juce::Point<float> (10, 10), r.getTopRight() + juce::Point<float> (-10, 10), r.getBottomLeft() + juce::Point<float> (10, -10), r.getBottomRight() + juce::Point<float> (-10, -10) })
        screw (g, pt.x, pt.y, 3.2f);
    chevron (g, Rectangle<float> (r.getX(), r.getY(), 32, r.getHeight()), true, ink);
    chevron (g, Rectangle<float> (r.getRight() - 32, r.getY(), 32, r.getHeight()), false, ink);
    // insignia propia por tipo de preamp
    static const juce::uint32 cols[] { 0xff8a8f99, 0xffc0392b, 0xff2e86de, 0xff27ae60, 0xffe08e2b };
    const Colour bc (cols[juce::jlimit (0, 4, p)]);
    const float cx = r.getCentreX(), cy = r.getY() + r.getHeight() * 0.32f, br = 15;
    Path hex;
    for (int i = 0; i < 6; ++i) { const float a = (float) i / 6.0f * juce::MathConstants<float>::twoPi; if (i == 0) hex.startNewSubPath (cx + br * std::sin (a), cy - br * std::cos (a)); else hex.lineTo (cx + br * std::sin (a), cy - br * std::cos (a)); }
    hex.closeSubPath();
    g.setGradientFill (ColourGradient (bc.brighter (0.5f), cx, cy - br, bc.darker (0.6f), cx, cy + br, false)); g.fillPath (hex);
    g.setColour (Colours::white.withAlpha (0.8f)); g.strokePath (hex, juce::PathStrokeType (1.3f));
    g.setFont (font (15, true)); g.setColour (Colours::white);
    g.drawText (juce::String (lk::preamps()[(size_t) p].name).substring (0, 1), Rectangle<float> (cx - br, cy - br, br * 2, br * 2), juce::Justification::centred);
    g.setGradientFill (ColourGradient (bc.withAlpha (0.35f), cx, cy, Colours::transparentBlack, cx + 70, cy, true)); g.fillEllipse (cx - 70, cy - 30, 140, 60);
    g.setFont (font (26)); g.setColour (Colours::black.withAlpha (0.6f));
    g.drawText (lk::preamps()[(size_t) p].name, r.withTrimmedTop (r.getHeight() * 0.52f).translated (0, 1.5f).withHeight (34), juce::Justification::centred);
    g.setColour (Colour (0xfff0f0f2)); g.drawText (lk::preamps()[(size_t) p].name, r.withTrimmedTop (r.getHeight() * 0.52f).withHeight (34), juce::Justification::centred);
}
void PreampView::mouseUp (const juce::MouseEvent& e)
{
    const int p = (int) param (proc, "pre"), n = (int) lk::preamps().size();
    if (e.position.x < 36) { setParam (proc, "pre", (float) ((p + n - 1) % n)); repaint(); return; }
    if (e.position.x > (float) getWidth() - 36) { setParam (proc, "pre", (float) ((p + 1) % n)); repaint(); return; }
    juce::PopupMenu m; m.addSectionHeader ("MIC PREAMP");
    for (int i = 0; i < n; ++i) m.addItem (i + 1, juce::String (lk::preamps()[(size_t) i].name) + "  -  " + U (lk::preamps()[(size_t) i].desc), true, i == p);
    juce::Component::SafePointer<PreampView> sp (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [sp] (int res) { if (sp != nullptr && res > 0) { setParam (sp->proc, "pre", (float) (res - 1)); sp->repaint(); } });
}

// ---------------- Room ----------------
void RoomView::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const int rm = (int) param (proc, "room");
    auto pic = Rectangle<float> (30, 0, r.getWidth() - 60, r.getHeight() - 36);
    g.setColour (Colours::black); g.fillRoundedRectangle (pic.expanded (3), 4);
    drawRoom (g, pic, rm);
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
    if (e.position.x < 30 && e.position.y < (float) getHeight() - 36) { setParam (proc, "room", (float) ((rm + n - 1) % n)); repaint(); return; }
    if (e.position.x > (float) getWidth() - 30 && e.position.y < (float) getHeight() - 36) { setParam (proc, "room", (float) ((rm + 1) % n)); repaint(); return; }
    juce::PopupMenu m; m.addSectionHeader ("ROOM");
    for (int i = 0; i < n; ++i) m.addItem (i + 1, U (lk::rooms()[(size_t) i].name), true, i == rm);
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
Content::Content (TopLockerProcessor& p)
    : proc (p), carousel (p),
      position (p.apvts, "position", "Front", "Back", [] (float v) { return juce::String (juce::roundToInt (v)) + " %"; }, 84, false, 1, 2),
      prox (p.apvts, "prox", "Close", "Far", [] (float v) { return juce::String (juce::roundToInt (v)) + " %"; }, 84, false, 3, 4),
      low (p.apvts, "low", "-12", "+12", [] (float v) { return juce::String (v, 1) + " dB"; }, 62),
      mid (p.apvts, "mid", "-12", "+12", [] (float v) { return juce::String (v, 1) + " dB"; }, 62),
      high (p.apvts, "high", "-12", "+12", [] (float v) { return juce::String (v, 1) + " dB"; }, 62),
      character (p.apvts, "character", "Clean", "Vintage", nullptr, 82, true),
      sat (p.apvts, "sat", "0 %", "100 %", [] (float v) { return juce::String (juce::roundToInt (v)) + " %"; }, 80, true),
      out (p.apvts, "out", "-18 dB", "+18 dB", [] (float v) { return juce::String (v, 1) + " dB"; }, 80),
      preGain (p.apvts, "preGain", "0", "10", [] (float v) { return juce::String (v, 1); }, 66, true),
      roomAmt (p.apvts, "roomAmt", "0", "100", [] (float v) { return juce::String (juce::roundToInt (v)) + " %"; }, 58, true),
      mix (p.apvts, "mix", "Dry", "Wet", [] (float v) { return juce::String (juce::roundToInt (v)) + " %"; }, 80),
      preamp (p), room (p)
{
    character.showValue = false;
    for (auto* d : { &low, &mid, &high, &out }) d->slider.getProperties().set ("bipolar", true);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &cats, &carousel, &prev, &next, &save, &load, &gear, &info, &presetBox, &charBox, &sourceBox,
                                    &position, &prox, &low, &mid, &high, &character, &sat, &out, &preGain, &roomAmt, &mix, &preamp, &room, &meter, &micPrev, &micNext })
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
        t << "TOP LOCKER  v1.0.0  -  Dj Pava HM Music\n\n"
          << "Cada microfono cambia respuesta, armonicos, transientes, compresion natural, proximidad y cuanta sala capta.\n\n";
        for (auto& m : lk::mics()) t << m.line1 << " " << m.line2 << ": " << U (m.desc) << "\n";
        t << U ("\nLos nombres y dibujos son propios; las marcas mencionadas pertenecen a sus dueños y solo indican el carácter de referencia.");
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Top Locker", t);
    };
    presetBox.label = "Preset:"; presetBox.onClick = [this] { presetMenu(); };
    charBox.centred = true; charBox.onClick = [this] { charMenu(); };
    sourceBox.onClick = [this] { sourceMenu(); };
    sourceBox.setTooltip (U ("Elige el micrófono con el que grabaste: Top Locker le quita su color antes de poner el nuevo."));
    preamp.setTooltip (U ("Toca las flechas para cambiar de preamp o el centro para ver la lista"));
    prev.setTooltip ("Preset anterior"); next.setTooltip ("Preset siguiente"); save.setTooltip ("Guardar tu preset"); load.setTooltip ("Cargar un preset");
    carousel.ensureVisible();
    tick();
}

void Content::resized()
{
    cats.setBounds (14, 86, 160, 316);
    carousel.setBounds (180, 86, 1006, 316);
    micPrev.setBounds (184, 210, 24, 60); micNext.setBounds (1158, 210, 24, 60);
    prev.setBounds (650, 22, 34, 36); presetBox.setBounds (688, 22, 244, 36); next.setBounds (936, 22, 34, 36);
    save.setBounds (978, 22, 60, 36); load.setBounds (1042, 22, 60, 36); gear.setBounds (1108, 22, 38, 36); info.setBounds (1150, 22, 38, 36);
    position.setBounds (12, 450, 152, 150);
    prox.setBounds (176, 450, 152, 150);
    low.setBounds (342, 482, 96, 120); mid.setBounds (438, 482, 96, 120); high.setBounds (534, 482, 96, 120);
    character.setBounds (645, 450, 150, 112); charBox.setBounds (668, 566, 104, 28);
    sat.setBounds (806, 450, 136, 150);
    out.setBounds (950, 450, 136, 150);
    meter.setBounds (1098, 422, 88, 178);
    preamp.setBounds (22, 652, 226, 128);
    preGain.setBounds (252, 646, 90, 140);
    room.setBounds (362, 646, 316, 140);
    roomAmt.setBounds (678, 652, 92, 126);
    mix.setBounds (786, 646, 130, 140);
    sourceBox.setBounds (944, 652, 236, 32);
}

void Content::paint (Graphics& g)
{
    // fondo
    g.setGradientFill (ColourGradient (Colour (0xff1d1c1b), 0, 0, Colour (0xff0c0c0c), 0, 800, false)); g.fillAll();
    // encabezado
    panel (g, { 6, 6, 1188, 66 });
    brandLogo (g, 22, 12, 1.0f);
    g.setColour (Colours::white.withAlpha (0.12f)); g.drawVerticalLine (300, 16, 62); g.setColour (Colours::black); g.drawVerticalLine (301, 16, 62);
    silverText (g, "TOP LOCKER", font (40, true, true), 318, 48);
    g.setFont (font (11.5f)); g.setColour (dim);
    g.drawText ("V I R T U A L   M I C R O P H O N E   C O L L E C T I O N", Rectangle<float> (320, 50, 330, 16), juce::Justification::left);
    // area de microfonos
    panel (g, { 6, 78, 1188, 332 }, false);
    // fila de controles
    struct P { float x, w; const char* t; };
    for (auto pp : { P { 6, 162, "MIC POSITION" }, P { 172, 162, "PROXIMITY" }, P { 338, 296, "" }, P { 638, 164, "CHARACTER" }, P { 806, 138, "SATURATION" }, P { 948, 142, "OUTPUT" } })
    {
        panel (g, { pp.x, 416, pp.w, 190 });
        if (*pp.t) title (g, pp.t, { pp.x, 424, pp.w, 24 });
    }
    auto tb = Rectangle<float> (378, 424, 216, 26); inset (g, tb, 4); title (g, "TONE CONTROL", tb, 17);
    g.setFont (font (16)); g.setColour (ink);
    g.drawText ("Low", Rectangle<float> (342, 458, 96, 20), juce::Justification::centred);
    g.drawText ("Mid", Rectangle<float> (438, 458, 96, 20), juce::Justification::centred);
    g.drawText ("High", Rectangle<float> (534, 458, 96, 20), juce::Justification::centred);
    panel (g, { 1094, 416, 100, 190 }, false);
    // fila de abajo
    panel (g, { 6, 612, 342, 182 }); title (g, "MIC PREAMP", { 26, 620, 120, 26 });
    panel (g, { 352, 612, 420, 182 }); title (g, "ROOM", { 362, 620, 80, 26 });
    g.setFont (font (12)); g.setColour (dim); g.drawText ("AMOUNT", Rectangle<float> (682, 626, 86, 20), juce::Justification::centred);
    panel (g, { 776, 612, 150, 182 }); title (g, "MIX", { 776, 620, 150, 26 });
    panel (g, { 930, 612, 264, 182 });
    g.setFont (font (12)); g.setColour (dim); g.drawText (U ("SOURCE MIC  (con qué grabaste)"), Rectangle<float> (944, 628, 236, 20), juce::Justification::centredLeft);
    waveLogo (g, 946, 726, 60, 30, Colour (0xffd8d8dc));
    auto f = font (28, true, true);
    silverText (g, "Dj Pava", f, 1006, 735);
    silverText (g, "HM", f, 1006 + textW (f, "Dj Pava "), 735, Colour (0xff9fd2ff), Colour (0xff2468d8));
    g.setFont (font (15)); g.setColour (Colour (0xffc4c4c8)); g.drawText ("T O P   L O C K E R", Rectangle<float> (1006, 744, 180, 20), juce::Justification::left);
    g.setFont (font (12)); g.setColour (dim); g.drawText ("v1.0.0", Rectangle<float> (1006, 764, 180, 16), juce::Justification::left);
    if (param (proc, "power") < 0.5f)
    {
        g.setColour (Colour (0xffff5a2a)); g.setFont (font (13, true));
        g.drawText ("BYPASS", Rectangle<float> (560, 48, 80, 16), juce::Justification::centred);
    }
}

void Content::tick()
{
    auto toDb = [] (float v) { return juce::Decibels::gainToDecibels (v, -60.0f); };
    const float i = toDb (proc.inPeak.exchange (0.0f)), o = toDb (proc.outPeak.exchange (0.0f));
    inDb = i > inDb ? i : std::max (i, inDb - 1.2f); outDb = o > outDb ? o : std::max (o, outDb - 1.2f);
    if (std::abs (meter.in - inDb) > 0.1f || std::abs (meter.out - outDb) > 0.1f) { meter.in = inDb; meter.out = outDb; meter.repaint(); }
    const auto p = proc.params(); const int rm = (int) param (proc, "room"); const float pw = param (proc, "power");
    if (! (p == lastP) || rm != lastRoom || proc.presetName != lastPreset || pw != lastPower)
    {
        if (p.mic != lastP.mic) { carousel.ensureVisible(); }
        lastP = p; lastRoom = rm; lastPreset = proc.presetName; lastPower = pw;
        presetBox.text = proc.presetName; presetBox.repaint();
        static const char* modes[] { "Modern", "Vintage", "Smooth" };
        charBox.text = modes[juce::jlimit (0, 2, p.charMode)]; charBox.repaint();
        const auto& s = tmic::sources()[(size_t) juce::jlimit (0, (int) tmic::sources().size() - 1, p.source)];
        sourceBox.text = U (s.name); sourceBox.repaint();
        carousel.repaint(); preamp.repaint(); room.repaint(); repaint (540, 40, 120, 30);
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
    m.addItem (1, U ("Modern  -  brillo limpio y armónicos sutiles"), true, cur == 0);
    m.addItem (2, U ("Vintage  -  cuerpo en medios-graves y agudos redondos"), true, cur == 1);
    m.addItem (3, U ("Smooth  -  quita dureza en 3-7 kHz"), true, cur == 2);
    juce::Component::SafePointer<Content> sp (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&charBox), [sp] (int r) { if (sp != nullptr && r > 0) { setParam (sp->proc, "charMode", (float) (r - 1)); sp->tick(); } });
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
    setResizeLimits (840, 560, 1800, 1200);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) W / (double) H);
    setSize (W, H);
    startTimerHz (30);
}
TopLockerEditor::~TopLockerEditor() { setLookAndFeel (nullptr); }
void TopLockerEditor::resized() { content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) W)); }
