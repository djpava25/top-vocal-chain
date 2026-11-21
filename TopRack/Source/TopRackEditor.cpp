#include "TopRackEditor.h"

using namespace trui;
using namespace trk;

namespace
{
    juce::String U (const char* s) { return juce::String::fromUTF8 (s); }
    const juce::Colour bgDark (0xff0e0f10), barTop (0xff1d1e21), barBot (0xff151618), panel (0xff1b1c1e), panel2 (0xff26272b),
                       line (0xff34363b), text (0xffe9eaec), dim (0xff8b8e94), amber (0xffffb72e), red (0xffff4a3c);
    constexpr float A0 = juce::MathConstants<float>::pi * 1.2f, A1 = juce::MathConstants<float>::pi * 2.8f;
    juce::Font font (float h, bool bold = false) { return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain)); }
    juce::String upper (const juce::String& s)
    {
        return s.toUpperCase().replace (U ("á"), U ("Á")).replace (U ("é"), U ("É")).replace (U ("í"), U ("Í"))
                .replace (U ("ó"), U ("Ó")).replace (U ("ú"), U ("Ú")).replace (U ("ñ"), U ("Ñ"));
    }
    juce::String compact (float v)
    {
        const float a = std::abs (v);
        if (a >= 1000.0f) { const float k = v / 1000.0f; return (std::abs (k - std::round (k)) < 0.05f ? juce::String ((int) std::round (k)) : juce::String (k, 1)) + "k"; }
        if (std::abs (v - std::round (v)) < 0.01f) return juce::String ((int) std::round (v));
        return juce::String (v, a < 1.0f ? 2 : 1).replace ("0.", ".");
    }
    void centredText (juce::Graphics& g, const juce::String& t, float cx, float cy, float size, juce::Colour c, bool bold = true, float w = 90.0f)
    {
        g.setColour (c); g.setFont (font (size, bold));
        g.drawFittedText (t, juce::Rectangle<float> (cx - w * 0.5f, cy - size, w, size * 2).toNearestInt(), juce::Justification::centred, 1, 0.7f);
    }
    std::array<juce::Image, NumTypes>& thumbCache() { static std::array<juce::Image, NumTypes> c; return c; }
}

//==============================================================================
// Frentes de los modulos (diseno propio para cada uno)
const Face& trui::face (int type)
{
    static const std::vector<Face> f = []
    {
        std::vector<Face> v ((size_t) NumTypes);
        v[Empty] = { 0xff161718, 2, 0xff8b8e94, 0xff8b8e94, "", "", 0, {} };
        v[Trim] = { 0xff1b1c1f, 2, 0xffe8e8e8, 0xffffffff, "TRIM", "GANANCIA • FASE • ANCHO", 1,
                    { { 0, 106, 232, 30, kBlack, 0 }, { 1, 106, 318, 34, kButton, 0 }, { 2, 62, 405, 19, kBlack, 0 }, { 3, 150, 405, 19, kBlack, 0 } } };
        v[Revive] = { 0xff6b8569, 3, 0xfff1ead2, 0xfff1ead2, "Revivir", "", 1,
                      { { 0, 106, 262, 34, kBlack, 0 }, { 1, 106, 385, 34, kBlack, 0 }, { 2, 168, 462, 13, kBlack, 0 } } };
        v[EqA] = { 0xff56667b, 0, 0xffeef1f5, 0xffffffff, "EQ CLASE A", "PREAMP Y ECUALIZADOR", 0,
                   { { 7, 152, 78, 19, kCap, 0xffc0392b }, { 0, 60, 78, 16, kChicken, 0 },
                     { 5, 60, 168, 16, kChicken, 0 }, { 6, 152, 168, 19, kCap, 0xffc0392b },
                     { 3, 60, 258, 16, kChicken, 0 }, { 4, 152, 258, 19, kCap, 0xffc0392b },
                     { 1, 60, 348, 16, kChicken, 0 }, { 2, 152, 348, 19, kCap, 0xffc0392b },
                     { 8, 60, 425, 26, kButton, 0 }, { 9, 152, 425, 16, kCap, 0xff8f969e }, { 10, 106, 455, 15, kCap, 0xff8f969e } } };
        v[EqConsole] = { 0xff1c1d20, 2, 0xffe6e6e6, 0xffffffff, "EQ CONSOLA", "ECUALIZADOR DE 4 BANDAS", 0,
                         { { 13, 62, 52, 24, kButton, 0 }, { 11, 152, 62, 17, kCap, 0xffd23b30 }, { 12, 62, 100, 14, kCap, 0xffd23b30 },
                           { 8, 152, 158, 17, kCap, 0xffe88a1d }, { 9, 62, 170, 14, kCap, 0xffe88a1d }, { 10, 112, 206, 10, kCap, 0xffe88a1d },
                           { 5, 152, 254, 17, kCap, 0xff2e9a4e }, { 6, 62, 266, 14, kCap, 0xff2e9a4e }, { 7, 112, 302, 10, kCap, 0xff2e9a4e },
                           { 2, 152, 350, 17, kCap, 0xff2f62c4 }, { 3, 62, 362, 14, kCap, 0xff2f62c4 }, { 4, 106, 398, 22, kButton, 0 },
                           { 0, 46, 442, 13, kBlack, 0 }, { 1, 106, 442, 13, kBlack, 0 }, { 15, 166, 442, 13, kBlack, 0 }, { 14, 106, 484, 30, kButton, 0 } } };
        v[EqAmerican] = { 0xff1b2740, 0, 0xffd9e6ff, 0xff6fb6ff, "EQ AMERICANO", "3 BANDAS POR PASOS", 0,
                          { { 5, 62, 85, 18, kCap, 0xff2f5fb8 }, { 6, 150, 85, 18, kCap, 0xff2b7c8c },
                            { 3, 62, 190, 18, kCap, 0xff2f5fb8 }, { 4, 150, 190, 18, kCap, 0xff2b7c8c },
                            { 0, 62, 295, 18, kCap, 0xff2f5fb8 }, { 1, 150, 295, 18, kCap, 0xff2b7c8c },
                            { 2, 62, 375, 26, kButton, 0 }, { 7, 150, 375, 26, kButton, 0 }, { 8, 106, 415, 26, kButton, 0 }, { 9, 106, 462, 14, kBlack, 0 } } };
        v[CompFet] = { 0xff17181a, 2, 0xffe0e0e0, 0xffffffff, "COMP FET", "COMPRESOR DE TRANSISTOR", 1,
                       { { 0, 72, 205, 22, kBlack, 0 }, { 1, 72, 298, 22, kBlack, 0 }, { 2, 52, 385, 14, kBlack, 0 }, { 3, 118, 385, 14, kBlack, 0 },
                         { 4, 168, 172, 17, kColumn, 0 }, { 5, 106, 458, 15, kBlack, 0 } } };
        v[CompVca] = { 0xffb3271f, 3, 0xfff6efe6, 0xfffff3e0, "COMP VCA", "COMPRESOR VCA", 1,
                       { { 0, 62, 200, 19, kBlack, 0 }, { 1, 150, 200, 19, kBlack, 0 }, { 2, 62, 288, 17, kBlack, 0 }, { 3, 150, 288, 17, kBlack, 0 },
                         { 5, 62, 376, 17, kBlack, 0 }, { 7, 150, 376, 17, kBlack, 0 }, { 6, 62, 456, 13, kBlack, 0 }, { 4, 150, 456, 26, kButton, 0 } } };
        v[CompOpto] = { 0xffd8dad6, 0, 0xff1e1e20, 0xffc0262b, "Comp Óptico", "", 1,
                        { { 0, 106, 222, 27, kBlack, 0 }, { 1, 106, 325, 27, kBlack, 0 }, { 4, 58, 418, 15, kBlack, 0 }, { 3, 154, 418, 15, kBlack, 0 },
                          { 2, 106, 468, 30, kButton, 0 } } };
        v[CompBus] = { 0xff33373e, 2, 0xffe2e4e8, 0xffff6157, "COMP BUS", "COMPRESOR DE BUS", 1,
                       { { 0, 106, 212, 22, kBlack, 0 }, { 1, 62, 298, 16, kChicken, 0 }, { 2, 150, 298, 16, kChicken, 0 }, { 3, 62, 382, 16, kChicken, 0 },
                         { 4, 150, 382, 16, kBlack, 0 }, { 5, 62, 458, 14, kChicken, 0 }, { 6, 150, 458, 14, kBlack, 0 } } };
        v[Stress] = { 0xff121214, 2, 0xffe8e8e8, 0xffff3b30, "ESTRESADOR", "COMPRESOR + DISTORSIÓN", 1,
                      { { 0, 62, 205, 19, kBlack, 0 }, { 1, 150, 205, 19, kBlack, 0 }, { 2, 62, 290, 15, kBlack, 0 }, { 3, 150, 290, 15, kBlack, 0 },
                        { 4, 62, 375, 16, kChicken, 0 }, { 7, 150, 375, 16, kBlack, 0 }, { 5, 62, 456, 14, kChicken, 0 }, { 6, 150, 456, 14, kChicken, 0 } } };
        v[Monster] = { 0xffb8bbbf, 1, 0xff141414, 0xff111111, "MONSTRUO", "COMPRESIÓN PARALELA EXTREMA", 1,
                       { { 0, 106, 222, 26, kBlack, 0 }, { 1, 106, 290, 28, kButton, 0 }, { 2, 60, 362, 18, kBlack, 0 }, { 4, 152, 362, 18, kBlack, 0 },
                         { 3, 106, 440, 20, kBlack, 0 } } };
        v[Bomber] = { 0xff38452c, 3, 0xffe9f0d8, 0xffb8ff6b, "BOMBARDERO", "GOLPE • CUERPO • CRUNCH", 1,
                      { { 0, 62, 215, 21, kBlack, 0 }, { 1, 150, 215, 21, kBlack, 0 }, { 2, 106, 302, 23, kBlack, 0 }, { 3, 62, 392, 16, kBlack, 0 }, { 4, 150, 392, 16, kBlack, 0 } } };
        v[DeEss] = { 0xff141416, 2, 0xffe5e5e5, 0xffe8302a, "DE-ESSER", "CONTROL DE SIBILANCIA", 2,
                     { { 3, 136, 60, 30, kButton, 0 }, { 4, 136, 100, 30, kButton, 0 }, { 0, 132, 198, 24, kCap, 0xffe8c22a }, { 1, 132, 300, 20, kBlack, 0 },
                       { 2, 106, 418, 21, kBlack, 0 } } };
        v[AirEq] = { 0xffe3e1d9, 0, 0xff222326, 0xff1e6fd0, "EQ DE AIRE", "AIRE • MEDIOS • TIERRA", 0,
                     { { 0, 106, 85, 22, kChrome, 0 }, { 1, 106, 162, 14, kChicken, 0 }, { 2, 62, 240, 18, kChrome, 0 }, { 3, 150, 240, 16, kChrome, 0 },
                       { 4, 106, 305, 12, kChrome, 0 }, { 5, 62, 380, 18, kChrome, 0 }, { 6, 150, 380, 16, kChrome, 0 }, { 7, 106, 456, 15, kChrome, 0 } } };
        v[Gate] = { 0xffe0b42a, 0, 0xff141414, 0xff141414, "PUERTA", "GATE / EXPANSOR", 3,
                    { { 0, 106, 135, 24, kBlack, 0 }, { 1, 106, 230, 20, kBlack, 0 }, { 2, 62, 315, 16, kBlack, 0 }, { 3, 150, 315, 16, kBlack, 0 },
                      { 4, 62, 400, 16, kBlack, 0 }, { 5, 150, 400, 16, kBlack, 0 } } };
        return v;
    }();
    return f[(size_t) juce::jlimit (0, (int) NumTypes - 1, type)];
}

//==============================================================================
void trui::drawKnob (juce::Graphics& g, juce::Point<float> c, float r, float norm, int style, juce::Colour cap, int steps, juce::Colour ink, bool scale)
{
    const float a = A0 + juce::jlimit (0.0f, 1.0f, norm) * (A1 - A0);
    if (scale)
    {
        const int n = steps > 1 ? steps : 11;
        g.setColour (ink.withAlpha (0.8f));
        const float rd = r + 4.5f, ds = juce::jmax (1.3f, r * 0.08f);
        for (int i = 0; i < n; ++i)
        {
            const float t = A0 + (A1 - A0) * (float) i / (float) (n - 1);
            g.fillEllipse (c.x + rd * std::sin (t) - ds * 0.5f, c.y - rd * std::cos (t) - ds * 0.5f, ds, ds);
        }
    }
    // sombra
    for (int i = 4; i > 0; --i)
    {
        const float e = (float) i * r * 0.05f;
        g.setColour (juce::Colours::black.withAlpha (0.12f));
        g.fillEllipse (c.x - r - e * 0.5f, c.y - r + e * 1.2f, (r + e * 0.5f) * 2, (r + e * 0.4f) * 2);
    }
    auto pointer = [&] (float from, float to, float w, juce::Colour col)
    {
        juce::Path p; p.addRoundedRectangle (-w * 0.5f, -to, w, to - from, w * 0.5f);
        p.applyTransform (juce::AffineTransform::rotation (a).translated (c));
        g.setColour (col); g.fillPath (p);
    };
    auto flutes = [&] (float rOut, float rIn, int n, juce::Colour base)
    {
        g.setGradientFill (juce::ColourGradient (base.brighter (0.35f), c.x, c.y - rOut, base.darker (0.8f), c.x, c.y + rOut, false));
        g.fillEllipse (c.x - rOut, c.y - rOut, rOut * 2, rOut * 2);
        for (int i = 0; i < n; ++i)
        {
            const float t = (float) i / (float) n * juce::MathConstants<float>::twoPi + a;
            const float lit = 0.5f + 0.5f * std::cos (t + 0.7f);
            g.setColour (i % 2 == 0 ? juce::Colours::white.withAlpha (0.04f + 0.10f * lit) : juce::Colours::black.withAlpha (0.35f));
            g.drawLine (juce::Line<float> ({ c.x + (rOut - 0.5f) * std::sin (t), c.y - (rOut - 0.5f) * std::cos (t) }, { c.x + rIn * std::sin (t), c.y - rIn * std::cos (t) }), juce::jmax (0.7f, rOut * 0.05f));
        }
        g.setColour (juce::Colours::black.withAlpha (0.8f)); g.drawEllipse (c.x - rOut, c.y - rOut, rOut * 2, rOut * 2, 0.8f);
    };
    switch (style)
    {
        case kCap:
        {
            flutes (r, r * 0.78f, 36, juce::Colour (0xff2a2b2f));
            const float rc = r * 0.74f;
            juce::ColourGradient cg (cap.brighter (0.6f), c.x - rc * 0.35f, c.y - rc * 0.6f, cap.darker (0.7f), c.x + rc * 0.5f, c.y + rc * 0.9f, true);
            cg.addColour (0.45, cap);
            g.setGradientFill (cg); g.fillEllipse (c.x - rc, c.y - rc, rc * 2, rc * 2);
            g.setColour (juce::Colours::black.withAlpha (0.5f)); g.drawEllipse (c.x - rc, c.y - rc, rc * 2, rc * 2, 0.8f);
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.5f), c.x - rc * 0.2f, c.y - rc * 0.8f, juce::Colours::transparentWhite, c.x, c.y, false));
            g.fillEllipse (c.x - rc * 0.6f, c.y - rc * 0.85f, rc * 1.1f, rc * 0.7f);
            pointer (r * 0.2f, r * 0.96f, juce::jmax (1.6f, r * 0.12f), juce::Colour (0xfff2f2f2));
            break;
        }
        case kChrome:
        {
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff5d6066), c.x, c.y - r, juce::Colour (0xff1d1e21), c.x, c.y + r, false));
            g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
            const float rc = r * 0.84f;
            juce::ColourGradient cg (juce::Colour (0xfffdfdfd), c.x - rc * 0.4f, c.y - rc * 0.7f, juce::Colour (0xff7d8086), c.x + rc * 0.5f, c.y + rc, true);
            cg.addColour (0.5, juce::Colour (0xffc9cbcf));
            g.setGradientFill (cg); g.fillEllipse (c.x - rc, c.y - rc, rc * 2, rc * 2);
            for (int i = 1; i < 6; ++i)
            {
                const float rr = rc * (float) i / 6.0f;
                g.setColour (juce::Colours::black.withAlpha (0.05f)); g.drawEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2, 0.6f);
            }
            g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawEllipse (c.x - rc, c.y - rc, rc * 2, rc * 2, 0.8f);
            pointer (r * 0.25f, r * 0.9f, juce::jmax (1.6f, r * 0.11f), juce::Colour (0xff1b1c1f));
            break;
        }
        case kChicken:
        {
            juce::Path p;
            const float rk = r;
            p.addEllipse (-rk * 0.6f, -rk * 0.6f, rk * 1.2f, rk * 1.2f);
            p.startNewSubPath (-rk * 0.24f, -rk * 1.0f); p.quadraticTo (0, -rk * 1.08f, rk * 0.24f, -rk * 1.0f);
            p.lineTo (rk * 0.36f, rk * 0.85f); p.quadraticTo (0, rk * 1.0f, -rk * 0.36f, rk * 0.85f); p.closeSubPath();
            p.setUsingNonZeroWinding (true);
            p.applyTransform (juce::AffineTransform::rotation (a).translated (c));
            g.setColour (juce::Colours::black.withAlpha (0.4f)); g.fillPath (p, juce::AffineTransform::translation (0.8f, 1.6f));
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4c52), c.x - rk * 0.5f, c.y - rk, juce::Colour (0xff070708), c.x + rk * 0.4f, c.y + rk, false));
            g.fillPath (p);
            g.setColour (juce::Colours::black); g.strokePath (p, juce::PathStrokeType (0.8f));
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.25f), c.x, c.y - rk * 0.6f, juce::Colours::transparentWhite, c.x, c.y, false));
            g.fillEllipse (c.x - rk * 0.45f, c.y - rk * 0.55f, rk * 0.9f, rk * 0.6f);
            pointer (r * 0.15f, r * 0.95f, juce::jmax (1.4f, r * 0.09f), juce::Colour (0xffeeeeee));
            break;
        }
        default:   // negra estriada
        {
            flutes (r, r * 0.8f, 44, juce::Colour (0xff1f2023));
            const float rc = r * 0.76f;
            juce::ColourGradient cg (juce::Colour (0xff4a4c51), c.x - rc * 0.3f, c.y - rc * 0.7f, juce::Colour (0xff050506), c.x + rc * 0.5f, c.y + rc, true);
            cg.addColour (0.45, juce::Colour (0xff1c1d20));
            g.setGradientFill (cg); g.fillEllipse (c.x - rc, c.y - rc, rc * 2, rc * 2);
            g.setColour (juce::Colours::white.withAlpha (0.12f)); g.drawEllipse (c.x - rc, c.y - rc, rc * 2, rc * 2, 0.7f);
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.22f), c.x - rc * 0.2f, c.y - rc * 0.8f, juce::Colours::transparentWhite, c.x, c.y - rc * 0.1f, false));
            g.fillEllipse (c.x - rc * 0.65f, c.y - rc * 0.88f, rc * 1.2f, rc * 0.75f);
            pointer (r * 0.3f, r * 0.97f, juce::jmax (1.6f, r * 0.1f), juce::Colour (0xfff4f4f4));
            break;
        }
    }
}

namespace
{
    void drawToggleFace (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t, bool on)
    {
        g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (r.translated (0, 1.2f).expanded (0.8f), 3);
        if (on)
        {
            g.setColour (juce::Colour (0xffffc43a).withAlpha (0.3f)); g.fillRoundedRectangle (r.expanded (3), 5);
            juce::ColourGradient cg (juce::Colour (0xfffff1a8), r.getCentreX(), r.getY(), juce::Colour (0xffe9a21c), r.getCentreX(), r.getBottom(), false);
            g.setGradientFill (cg);
        }
        else g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4b4f), r.getCentreX(), r.getY(), juce::Colour (0xff232427), r.getCentreX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 2.5f);
        g.setColour (juce::Colours::black.withAlpha (0.7f)); g.drawRoundedRectangle (r, 2.5f, 0.8f);
        g.setColour (juce::Colours::white.withAlpha (on ? 0.5f : 0.12f)); g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + 2, r.getRight() - 2);
        g.setColour (on ? juce::Colour (0xff2a1a05) : juce::Colour (0xffc8c8c8));
        g.setFont (font (juce::jmin (10.0f, r.getHeight() * 0.55f), true));
        g.drawFittedText (t, r.reduced (2, 0).toNearestInt(), juce::Justification::centred, 1, 0.6f);
    }

    void screw (juce::Graphics& g, float x, float y)
    {
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillEllipse (x - 5.5f, y - 4.5f, 11, 11);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff55575c), x - 3, y - 4, juce::Colour (0xff0d0d0e), x + 3, y + 4, false));
        g.fillEllipse (x - 5, y - 5, 10, 10);
        g.setColour (juce::Colour (0xff050505));
        g.drawLine (x - 3, y - 3, x + 3, y + 3, 1.3f); g.drawLine (x - 3, y + 3, x + 3, y - 3, 1.3f);
        g.setColour (juce::Colours::white.withAlpha (0.15f)); g.drawEllipse (x - 5, y - 5, 10, 10, 0.6f);
    }

    // escala del VU (estatica) y aguja
    const juce::Rectangle<float> vuFace (36, 48, 140, 86);
    void drawVuFace (juce::Graphics& g, bool gr, const Face& fc)
    {
        auto bez = vuFace.expanded (8, 8);
        const bool lightPanel = juce::Colour (fc.panel).getPerceivedBrightness() > 0.55f;
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (bez.translated (0, 2), 6);
        g.setGradientFill (juce::ColourGradient (juce::Colour (lightPanel ? 0xff3a3b3e : 0xff2e2f33), bez.getX(), bez.getY(), juce::Colour (0xff050506), bez.getX(), bez.getBottom(), false));
        g.fillRoundedRectangle (bez, 6);
        g.setColour (juce::Colours::white.withAlpha (0.12f)); g.drawRoundedRectangle (bez.reduced (0.5f), 6, 0.8f);
        juce::ColourGradient cg (juce::Colour (0xfff7e7b6), vuFace.getCentreX(), vuFace.getY() + 10, juce::Colour (0xffc99d4e), vuFace.getCentreX(), vuFace.getBottom() + 30, true);
        g.setGradientFill (cg); g.fillRect (vuFace);
        const juce::Point<float> pv (vuFace.getCentreX(), vuFace.getBottom() + 22);
        const float R = 82.0f;
        const juce::Colour ink (0xff2a1d10);
        const float th0 = -0.82f, th1 = 0.82f;
        juce::Path arc; arc.addCentredArc (pv.x, pv.y, R, R, 0, th0, th1, true);
        g.setColour (ink); g.strokePath (arc, juce::PathStrokeType (1.1f));
        if (! gr) { juce::Path red; red.addCentredArc (pv.x, pv.y, R + 1.5f, R + 1.5f, 0, th0 + (th1 - th0) * 0.72f, th1, true); g.setColour (juce::Colour (0xffc8261c)); g.strokePath (red, juce::PathStrokeType (3.0f)); }
        const char* labsGr[] { "20", "10", "7", "5", "3", "1", "0" };
        const float posGr[] { 0.0f, 0.5f, 0.65f, 0.75f, 0.85f, 0.95f, 1.0f };
        const char* labsVu[] { "20", "10", "7", "5", "3", "1", "0", "+3" };
        const float posVu[] { 0.0f, 0.43f, 0.56f, 0.65f, 0.74f, 0.83f, 0.89f, 1.0f };
        const int n = gr ? 7 : 8;
        g.setFont (font (8.0f, true));
        for (int i = 0; i < n; ++i)
        {
            const float p = gr ? posGr[i] : posVu[i];
            const float t = th0 + (th1 - th0) * p;
            const bool redZone = ! gr && p > 0.86f;
            g.setColour (redZone ? juce::Colour (0xffc8261c) : ink);
            g.drawLine (juce::Line<float> ({ pv.x + R * std::sin (t), pv.y - R * std::cos (t) }, { pv.x + (R + 5) * std::sin (t), pv.y - (R + 5) * std::cos (t) }), 1.1f);
            const float rl = R + 11.0f;
            g.drawText (gr ? labsGr[i] : labsVu[i], juce::Rectangle<float> (pv.x + rl * std::sin (t) - 10, pv.y - rl * std::cos (t) - 5, 20, 10), juce::Justification::centred);
        }
        for (int i = 0; i <= 20; ++i)
        {
            const float t = th0 + (th1 - th0) * (float) i / 20.0f;
            g.setColour (ink.withAlpha (0.6f));
            g.drawLine (juce::Line<float> ({ pv.x + R * std::sin (t), pv.y - R * std::cos (t) }, { pv.x + (R + 2.5f) * std::sin (t), pv.y - (R + 2.5f) * std::cos (t) }), 0.6f);
        }
        g.setColour (ink); g.setFont (font (9.0f, true));
        g.drawText (gr ? "GR" : "VU", juce::Rectangle<float> (vuFace.getRight() - 30, vuFace.getBottom() - 16, 24, 12), juce::Justification::centred);
        g.setFont (font (7.0f, true)); g.setColour (ink.withAlpha (0.7f));
        g.drawText ("TOP RACK", juce::Rectangle<float> (vuFace.getCentreX() - 30, vuFace.getBottom() - 30, 60, 10), juce::Justification::centred);
    }
    void drawVuNeedle (juce::Graphics& g, float pos)
    {
        const juce::Point<float> pv (vuFace.getCentreX(), vuFace.getBottom() + 22);
        const float t = -0.82f + 1.64f * juce::jlimit (-0.04f, 1.04f, pos);
        g.saveState();
        g.reduceClipRegion (vuFace.toNearestInt());
        const juce::Point<float> tip (pv.x + 86 * std::sin (t), pv.y - 86 * std::cos (t));
        g.setColour (juce::Colours::black.withAlpha (0.18f)); g.drawLine (juce::Line<float> (pv.translated (2, 2), tip.translated (2, 2)), 1.4f);
        g.setColour (juce::Colour (0xff181410)); g.drawLine (juce::Line<float> (pv, tip), 1.2f);
        // tapa del pivote
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2b2e), pv.x, vuFace.getBottom() - 8, juce::Colour (0xff0a0a0b), pv.x, vuFace.getBottom(), false));
        g.fillRect (vuFace.getX(), vuFace.getBottom() - 9, vuFace.getWidth(), 9.0f);
        g.restoreState();
        // cristal
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.22f), vuFace.getX(), vuFace.getY(), juce::Colours::transparentWhite, vuFace.getX() + 50, vuFace.getY() + 40, false));
        g.fillRect (vuFace.withHeight (vuFace.getHeight() * 0.5f));
    }
    const float ledDb[] { 1, 2, 3, 4, 5, 6, 8, 10, 12, 14, 16, 20 };
    void drawLeds (juce::Graphics& g, float gr, bool withLabels, juce::Colour ink)
    {
        for (int i = 0; i < 12; ++i)
        {
            const float y = 64.0f + (float) i * 24.0f;
            const bool on = gr >= ledDb[i];
            if (withLabels) { g.setColour (ink); g.setFont (font (8.5f, true)); g.drawText (juce::String ((int) ledDb[i]), juce::Rectangle<float> (8, y - 6, 18, 12), juce::Justification::centredRight); }
            g.setColour (juce::Colours::black); g.fillEllipse (31, y - 5, 10, 10);
            if (on) { g.setColour (red.withAlpha (0.35f)); g.fillEllipse (28, y - 8, 16, 16); }
            g.setGradientFill (juce::ColourGradient (on ? juce::Colour (0xffffb0a0) : juce::Colour (0xff4a1510), 34, y - 3, on ? juce::Colour (0xffd11a10) : juce::Colour (0xff1a0605), 39, y + 4, true));
            g.fillEllipse (32, y - 4, 8, 8);
        }
    }
    void drawGateLeds (juce::Graphics& g, float gr, juce::Colour ink)
    {
        const bool open = gr < 1.0f;
        auto led = [&] (float x, juce::Colour c, bool on, const char* t)
        {
            g.setColour (juce::Colours::black); g.fillEllipse (x - 6, 56, 12, 12);
            if (on) { g.setColour (c.withAlpha (0.4f)); g.fillEllipse (x - 9, 53, 18, 18); }
            g.setGradientFill (juce::ColourGradient (on ? c.brighter (0.8f) : c.darker (2.5f), x - 2, 59, on ? c : c.darker (3.5f), x + 4, 66, true));
            g.fillEllipse (x - 4.5f, 57.5f, 9, 9);
            g.setColour (ink); g.setFont (font (8.5f, true)); g.drawText (t, juce::Rectangle<float> (x - 30, 70, 60, 12), juce::Justification::centred);
        };
        led (74, juce::Colour (0xff3dff5a), open, "ABIERTA");
        led (138, red, ! open, "CERRADA");
    }
}

void trui::drawFace (juce::Graphics& g, int type, bool withControls)
{
    const auto& fc = face (type);
    const auto& d = defs()[(size_t) type];
    const juce::Colour pc (fc.panel), ink (fc.ink);
    const auto R = juce::Rectangle<float> (0, 0, faceW, faceH);
    // panel
    g.setGradientFill (juce::ColourGradient (pc.brighter (0.10f), 0, 0, pc.darker (0.18f), 0, faceH, false));
    g.fillRoundedRectangle (R, 3);
    juce::Random rng (type * 977 + 13);
    if (fc.texture == 1)        // aluminio cepillado
        for (float y = 1; y < faceH - 1; y += 1.0f)
        {
            g.setColour ((rng.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (0.03f + 0.05f * rng.nextFloat()));
            g.drawHorizontalLine ((int) y, 1, faceW - 1);
        }
    else
        for (int i = 0; i < 2600; ++i)
        {
            g.setColour ((rng.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (fc.texture == 2 ? 0.035f : 0.05f));
            g.fillRect (rng.nextFloat() * faceW, rng.nextFloat() * faceH, 1.0f, 1.0f);
        }
    if (fc.texture == 3)        // pintura con uso
        for (int i = 0; i < 70; ++i)
        {
            const float x = rng.nextFloat() * faceW, y = rng.nextFloat() * faceH, s = 2.0f + rng.nextFloat() * 14.0f;
            g.setColour ((rng.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (0.025f + 0.03f * rng.nextFloat()));
            g.fillEllipse (x, y, s * (1.0f + rng.nextFloat()), s);
        }
    // biselado
    g.setColour (juce::Colours::white.withAlpha (0.16f)); g.drawLine (1, 1, faceW - 1, 1, 1.0f); g.drawLine (1, 1, 1, faceH - 1, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawLine (1, faceH - 1, faceW - 1, faceH - 1, 1.2f); g.drawLine (faceW - 1, 1, faceW - 1, faceH - 1, 1.2f);
    for (auto [x, y] : { std::pair<float, float> { 12, 12 }, { 200, 12 }, { 12, 548 }, { 200, 548 } }) screw (g, x, y);
    // insignia propia "TR"
    {
        const auto b = juce::Rectangle<float> (94, 6, 24, 30);
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (b.translated (0, 1.2f), 3);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3d3f44), b.getX(), b.getY(), juce::Colour (0xff121315), b.getX(), b.getBottom(), false));
        g.fillRoundedRectangle (b, 3);
        g.setColour (juce::Colour (0xffb9bcc2)); g.drawRoundedRectangle (b.reduced (1.5f), 2, 0.9f);
        g.setColour (juce::Colour (0xffe8e9eb)); g.setFont (font (11.0f, true));
        g.drawText ("TR", b, juce::Justification::centred);
    }
    // medidores (parte fija)
    if (fc.meter == 1) drawVuFace (g, d.dynamics, fc);
    if (fc.meter == 2)
    {
        g.setColour (juce::Colours::black.withAlpha (0.3f)); g.fillRoundedRectangle (24, 50, 24, 300, 4);
        g.setColour (ink.withAlpha (0.8f)); g.setFont (font (8.0f, true));
        g.drawText ("GR dB", juce::Rectangle<float> (14, 354, 44, 12), juce::Justification::centred);
        g.setColour (ink.withAlpha (0.5f)); g.drawVerticalLine (56, 70, 330);
        drawLeds (g, 0, true, ink);
    }
    if (fc.meter == 3) drawGateLeds (g, 0, ink);
    // titulos
    if (type == Revive)
    {
        g.setColour (ink); g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultSerifFontName(), 32.0f, juce::Font::bold | juce::Font::italic)));
        g.drawText (U (fc.name), juce::Rectangle<float> (0, 152, faceW, 44), juce::Justification::centred);
        // rejilla
        const auto gr = juce::Rectangle<float> (40, 494, 132, 34);
        g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (gr, 6);
        g.setColour (juce::Colour (0xff9ea89c).withAlpha (0.6f));
        for (float y = gr.getY() + 5; y < gr.getBottom() - 3; y += 5) for (float x = gr.getX() + 6 + (((int) y / 5) % 2) * 2.5f; x < gr.getRight() - 4; x += 5) g.fillEllipse (x, y, 2.2f, 2.2f);
    }
    else if (type == CompOpto)
    {
        g.setColour (juce::Colour (fc.title)); g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultSerifFontName(), 22.0f, juce::Font::bold | juce::Font::italic)));
        g.drawText (U (fc.name), juce::Rectangle<float> (0, 500, faceW, 32), juce::Justification::centred);
        g.setColour (juce::Colour (fc.title)); g.fillRect (6.0f, 496.0f, 200.0f, 1.6f); g.fillRect (6.0f, 533.0f, 140.0f, 1.6f);
        juce::Path st; st.startNewSubPath (146, 534); st.lineTo (160, 520); st.lineTo (206, 520);
        g.strokePath (st, juce::PathStrokeType (1.6f));
    }
    else
    {
        g.setColour (juce::Colour (fc.title)); g.setFont (font (type == Monster ? 22.0f : 19.0f, true).withExtraKerningFactor (0.02f));
        g.drawFittedText (U (fc.name), juce::Rectangle<int> (10, 500, 192, 26), juce::Justification::centred, 1, 0.7f);
        g.setColour (ink.withAlpha (0.75f)); g.setFont (font (7.5f, true));
        g.drawFittedText (U (fc.sub), juce::Rectangle<int> (16, 524, 180, 12), juce::Justification::centred, 1, 0.7f);
    }
    // serigrafia de los controles
    for (const auto& p : fc.pos)
    {
        if (p.k >= (int) d.knobs.size()) continue;
        const auto& kd = d.knobs[(size_t) p.k];
        const int steps = kd.steps.size();
        if (p.style == kButton)
        {
            if (p.y < 480) centredText (g, upper (kd.label), p.x, p.y + 19, 7.5f, ink, true, p.r * 2 + 30);
            if (withControls) drawToggleFace (g, { p.x - p.r, p.y - 10, p.r * 2, 20 }, upper (kd.steps[(int) kd.def]), kd.def > 0.5f);
            continue;
        }
        if (p.style == kColumn)
        {
            centredText (g, upper (kd.label), p.x, p.y - 12, 8.0f, ink);
            if (withControls) for (int i = 0; i < steps; ++i) drawToggleFace (g, { p.x - p.r, p.y + (float) i * 28, p.r * 2, 22 }, kd.steps[i], i == (int) kd.def);
            continue;
        }
        centredText (g, upper (kd.label), p.x, p.y + p.r + (p.r >= 13 ? 19.0f : 13.0f), p.r >= 20 ? 9.0f : 8.0f, ink, true, 84);
        const float rl = p.r + 11.0f;
        g.setFont (font (7.5f, true)); g.setColour (ink.withAlpha (0.85f));
        auto at = [&] (float norm, const juce::String& t)
        {
            const float a = A0 + (A1 - A0) * norm;
            g.drawText (t, juce::Rectangle<float> (p.x + rl * std::sin (a) - 14, p.y - rl * std::cos (a) - 5, 28, 10), juce::Justification::centred);
        };
        if (steps > 1 && steps <= 6 && p.r >= 14)
            for (int i = 0; i < steps; ++i) at ((float) i / (float) (steps - 1), kd.steps[i].length() > 5 ? kd.steps[i].substring (0, 4) : kd.steps[i]);
        else if (steps > 1) { at (0, kd.steps[0]); at (1, kd.steps[steps - 1]); }
        else if (p.r >= 13) { at (0, compact (kd.lo)); at (1, compact (kd.hi)); }
        if (withControls)
            drawKnob (g, { p.x, p.y }, p.r, kd.toNorm (kd.def), p.style, juce::Colour (p.cap), steps, ink, true);
    }
    if (withControls)
    {
        if (fc.meter == 1) drawVuNeedle (g, d.dynamics ? 1.0f : 0.0f);
    }
}

juce::Image trui::thumbnail (int type)
{
    auto& c = thumbCache()[(size_t) type];
    if (c.isNull())
    {
        c = juce::Image (juce::Image::ARGB, 116, 306, true);
        juce::Graphics g (c);
        g.addTransform (juce::AffineTransform::scale (116.0f / faceW));
        drawFace (g, type, true);
    }
    return c;
}

//==============================================================================
Look::Look()
{
    setColour (juce::ComboBox::backgroundColourId, panel2);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::outlineColourId, line);
    setColour (juce::ComboBox::arrowColourId, dim);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff202124));
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::headerTextColourId, amber);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, amber.withAlpha (0.3f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff202124));
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TooltipWindow::outlineColourId, line);
    setColour (juce::Label::textColourId, text);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::highlightColourId, amber.withAlpha (0.3f));
    setColour (juce::CaretComponent::caretColourId, amber);
    setColour (juce::ScrollBar::thumbColourId, juce::Colour (0xff4a4c52));
}
void Look::fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor&)
{
    g.setColour (juce::Colour (0xff111214)); g.fillRoundedRectangle (0, 0, (float) w, (float) h, 4);
    g.setColour (line); g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1, (float) h - 1, 4, 1.0f);
}
void Look::drawBubble (juce::Graphics& g, juce::BubbleComponent&, const juce::Point<float>&, const juce::Rectangle<float>& body)
{
    g.setColour (juce::Colour (0xf0151618)); g.fillRoundedRectangle (body, 4);
    g.setColour (amber.withAlpha (0.6f)); g.drawRoundedRectangle (body.reduced (0.5f), 4, 1.0f);
}
void Look::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider& s)
{
    const int style = s.getProperties().getWithDefault ("style", (int) kBlack);
    const auto cap = juce::Colour ((juce::uint32) (juce::int64) s.getProperties().getWithDefault ("cap", (juce::int64) 0xff8f969e));
    const auto ink = juce::Colour ((juce::uint32) (juce::int64) s.getProperties().getWithDefault ("ink", (juce::int64) 0xffd0d0d0));
    const int steps = s.getProperties().getWithDefault ("steps", 0);
    const bool scale = s.getProperties().getWithDefault ("scale", true);
    const float r = (float) juce::jmin (w, h) * 0.5f - 7.0f;
    drawKnob (g, { (float) x + (float) w * 0.5f, (float) y + (float) h * 0.5f }, r, pos, style, cap, steps, ink, scale);
}
void Look::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider&)
{
    const float cy = (float) y + (float) h * 0.5f;
    g.setColour (juce::Colour (0xff070708)); g.fillRoundedRectangle ((float) x, cy - 2.0f, (float) w, 4.0f, 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.08f)); g.drawHorizontalLine ((int) cy + 2, (float) x, (float) (x + w));
    g.setColour (dim.withAlpha (0.6f)); g.drawVerticalLine (x + w / 2, cy - 5, cy + 5);
    const auto th = juce::Rectangle<float> (pos - 4.5f, cy - 8.0f, 9.0f, 16.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (th.translated (0, 1.5f), 2);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe2e3e5), th.getX(), th.getY(), juce::Colour (0xff7d7f84), th.getX(), th.getBottom(), false));
    g.fillRoundedRectangle (th, 2);
    g.setColour (juce::Colour (0xff1a1a1b)); g.drawVerticalLine ((int) pos, th.getY() + 3, th.getBottom() - 3);
}
void Look::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (panel2); g.fillRoundedRectangle (r, 5);
    g.setColour (line); g.drawRoundedRectangle (r, 5, 1.0f);
    juce::Path a; const float ax = (float) w - 14, ay = (float) h * 0.5f;
    a.addTriangle (ax - 4, ay - 1.5f, ax + 4, ay - 1.5f, ax, ay - 5.5f);
    a.addTriangle (ax - 4, ay + 1.5f, ax + 4, ay + 1.5f, ax, ay + 5.5f);
    g.setColour (dim); g.fillPath (a);
}
juce::Font Look::getComboBoxFont (juce::ComboBox&) { return font (13.5f); }

//==============================================================================
RackKnob::RackKnob (TopRackProcessor& p, int s, int k, const KnobDef& d) : slot (s), knob (k), def (d), proc (p)
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (A0, A1, true);
    setPopupDisplayEnabled (true, true, nullptr, 1200);
    setMouseDragSensitivity (220);
    getProperties().set ("steps", def.steps.size());
    att = std::make_unique<APVTS::SliderAttachment> (p.apvts, TopRackProcessor::sid (s, "p" + juce::String (k)), *this);
    textFromValueFunction = [this] (double v) { return def.text ((float) v); };
    setDoubleClickReturnValue (true, def.toNorm (def.def));
    setTooltip (def.label + U (" · doble clic: valor normal · clic derecho: macros"));
}
double RackKnob::snapValue (double v, DragMode)
{
    if (def.steps.size() > 1) return std::round (v * (def.steps.size() - 1)) / (double) (def.steps.size() - 1);
    return v;
}
void RackKnob::mouseDown (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu()) { juce::Slider::mouseDown (e); return; }
    juce::PopupMenu m;
    const int cur = proc.macroOf (slot, knob);
    for (int i = 0; i < 4; ++i) m.addItem (i + 1, U ("Asignar a ") + proc.macroNames[i], true, cur == i);
    m.addItem (10, "Quitar de la macro", cur >= 0);
    m.addSeparator();
    m.addItem (20, "Valor por defecto");
    juce::Component::SafePointer<RackKnob> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int r)
    {
        if (safe == nullptr || r == 0) return;
        if (r >= 1 && r <= 4) safe->proc.assignMacro (r - 1, safe->slot, safe->knob);
        else if (r == 10) safe->proc.unassign (safe->slot, safe->knob);
        else if (r == 20) safe->proc.setKnobReal (safe->slot, safe->knob, safe->def.def);
        if (auto* p = safe->getParentComponent()) p->repaint();
    });
}

//==============================================================================
RackToggle::RackToggle (TopRackProcessor& p, int s, int k, const KnobDef& d, bool col) : def (d), column (col)
{
    auto* prm = p.apvts.getParameter (TopRackProcessor::sid (s, "p" + juce::String (k)));
    att = std::make_unique<juce::ParameterAttachment> (*prm, [this] (float v) { value = v; repaint(); }, nullptr);
    att->sendInitialUpdate();
    setTooltip (def.label);
}
void RackToggle::paint (juce::Graphics& g)
{
    const int n = def.steps.size();
    const int cur = juce::jlimit (0, n - 1, (int) std::round (value * (float) (n - 1)));
    if (! column) { drawToggleFace (g, getLocalBounds().toFloat().reduced (1.0f, 1.0f), upper (def.steps[cur]), cur == 1); return; }
    for (int i = 0; i < n; ++i) drawToggleFace (g, { 1.0f, (float) i * 28.0f + 1.0f, (float) getWidth() - 2.0f, 22.0f }, def.steps[i], i == cur);
}
void RackToggle::mouseDown (const juce::MouseEvent& e)
{
    const int n = def.steps.size();
    if (n < 2) return;
    int idx;
    if (column) idx = juce::jlimit (0, n - 1, e.y / 28);
    else idx = value > 0.5f ? 0 : 1;
    att->setValueAsCompleteGesture ((float) idx / (float) (n - 1));
}

//==============================================================================
void IconButton::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    if (framed)
    {
        g.setColour (hover ? juce::Colour (0xff303236) : juce::Colour (0xff232427)); g.fillRoundedRectangle (r, 4);
        g.setColour (juce::Colour (0xff3a3c41)); g.drawRoundedRectangle (r, 4, 1.0f);
    }
    const auto col = on ? onColour : (hover ? juce::Colour (0xffe9eaec) : juce::Colour (0xffb9bbbf));
    g.setColour (col);
    const auto c = r.getCentre();
    const float s = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f;
    juce::PathStrokeType st (1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    switch (icon)
    {
        case power:
        {
            juce::Path p; p.addCentredArc (c.x, c.y + 0.5f, s * 0.48f, s * 0.48f, 0, 0.65f, juce::MathConstants<float>::twoPi - 0.65f, true);
            g.strokePath (p, st); g.drawLine (c.x, c.y - s * 0.62f, c.x, c.y - s * 0.05f, 1.7f);
            break;
        }
        case undo: case redo:
        {
            juce::Path p; const float d = icon == undo ? 1.0f : -1.0f;
            p.startNewSubPath (c.x - d * s * 0.45f, c.y - s * 0.2f);
            p.lineTo (c.x + d * s * 0.15f, c.y - s * 0.2f);
            p.cubicTo (c.x + d * s * 0.6f, c.y - s * 0.2f, c.x + d * s * 0.6f, c.y + s * 0.45f, c.x + d * s * 0.1f, c.y + s * 0.45f);
            g.strokePath (p, st);
            juce::Path a; a.addTriangle (c.x - d * s * 0.62f, c.y - s * 0.2f, c.x - d * s * 0.3f, c.y - s * 0.45f, c.x - d * s * 0.3f, c.y + s * 0.05f);
            g.fillPath (a);
            break;
        }
        case trash:
        {
            g.drawRoundedRectangle (c.x - s * 0.32f, c.y - s * 0.3f, s * 0.64f, s * 0.8f, 1.5f, 1.6f);
            g.drawLine (c.x - s * 0.45f, c.y - s * 0.38f, c.x + s * 0.45f, c.y - s * 0.38f, 1.6f);
            g.drawLine (c.x - s * 0.12f, c.y - s * 0.52f, c.x + s * 0.12f, c.y - s * 0.52f, 1.6f);
            g.drawLine (c.x - s * 0.1f, c.y - s * 0.15f, c.x - s * 0.1f, c.y + s * 0.35f, 1.2f);
            g.drawLine (c.x + s * 0.1f, c.y - s * 0.15f, c.x + s * 0.1f, c.y + s * 0.35f, 1.2f);
            break;
        }
        case chevron: { juce::Path p; p.startNewSubPath (c.x - s * 0.18f, c.y - s * 0.35f); p.lineTo (c.x + s * 0.2f, c.y); p.lineTo (c.x - s * 0.18f, c.y + s * 0.35f); g.strokePath (p, st); break; }
        case menu: for (int i = -1; i <= 1; ++i) g.fillEllipse (c.x - 1.8f, c.y + (float) i * s * 0.4f - 1.8f, 3.6f, 3.6f); break;
        case hamburger: for (int i = -1; i <= 1; ++i) g.fillRoundedRectangle (c.x - s * 0.5f, c.y + (float) i * s * 0.36f - 1.0f, s, 2.0f, 1.0f); break;
        case up: case down:
        {
            juce::Path p; const float d = icon == up ? -1.0f : 1.0f;
            p.startNewSubPath (c.x - s * 0.35f, c.y - d * s * 0.15f); p.lineTo (c.x, c.y + d * s * 0.2f); p.lineTo (c.x + s * 0.35f, c.y - d * s * 0.15f);
            g.strokePath (p, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            break;
        }
        case home:
        {
            juce::Path p; p.startNewSubPath (c.x - s * 0.5f, c.y); p.lineTo (c.x, c.y - s * 0.5f); p.lineTo (c.x + s * 0.5f, c.y);
            p.startNewSubPath (c.x - s * 0.35f, c.y - s * 0.12f); p.lineTo (c.x - s * 0.35f, c.y + s * 0.45f); p.lineTo (c.x + s * 0.35f, c.y + s * 0.45f); p.lineTo (c.x + s * 0.35f, c.y - s * 0.12f);
            g.strokePath (p, st);
            break;
        }
        case macro:
            for (int i = -1; i <= 1; ++i)
            {
                const float x = c.x + (float) i * s * 0.36f;
                g.drawLine (x, c.y - s * 0.5f, x, c.y + s * 0.5f, 1.4f);
                g.fillRoundedRectangle (x - 3.0f, c.y + (float) (i == 0 ? -0.25f : 0.15f) * s - 1.5f, 6.0f, 3.0f, 1.0f);
            }
            break;
        case close: g.drawLine (c.x - s * 0.3f, c.y - s * 0.3f, c.x + s * 0.3f, c.y + s * 0.3f, 1.6f); g.drawLine (c.x - s * 0.3f, c.y + s * 0.3f, c.x + s * 0.3f, c.y - s * 0.3f, 1.6f); break;
        default:
            g.setFont (font (fontSize, true));
            g.drawFittedText (text, r.reduced (3, 0).toNearestInt(), juce::Justification::centred, 1, 0.6f);
            break;
    }
}

//==============================================================================
SlotColumn::SlotColumn (TopRackProcessor& p, int s) : slot (s), proc (p)
{
    power.setTooltip ("Encender / apagar el módulo");
    power.onClick = [this] (const juce::MouseEvent&)
    {
        if (auto* prm = proc.apvts.getParameter (TopRackProcessor::sid (slot, "byp"))) prm->setValueNotifyingHost (prm->getValue() > 0.5f ? 0.0f : 1.0f);
    };
    solo.setTooltip (U ("Solo: escuchar solo este módulo"));
    solo.onColour = juce::Colour (0xffffd84a);
    solo.onClick = [this] (const juce::MouseEvent&) { proc.soloSlot = proc.soloSlot.load() == slot ? -1 : slot; };
    nameBox.fontSize = 12.0f;
    nameBox.setTooltip (U ("Clic: cambiar módulo, mover o quitar"));
    nameBox.onClick = [this] (const juce::MouseEvent&) { if (onSelect) onSelect (slot); moduleMenu(); };
    close.setTooltip ("Quitar el módulo");
    close.onClick = [this] (const juce::MouseEvent&) { proc.setSlotType (slot, Empty); };
    for (auto* b : { &scInt, &scExt }) { b->fontSize = 9.5f; b->framed = false; }
    scInt.setTooltip ("El compresor escucha su propia señal");
    scExt.setTooltip (U ("Sidechain externo: el compresor escucha otra pista"));
    scInt.onClick = [this] (const juce::MouseEvent&) { if (auto* prm = proc.apvts.getParameter (TopRackProcessor::sid (slot, "sc"))) prm->setValueNotifyingHost (0.0f); };
    scExt.onClick = [this] (const juce::MouseEvent&) { if (auto* prm = proc.apvts.getParameter (TopRackProcessor::sid (slot, "sc"))) prm->setValueNotifyingHost (1.0f); };
    outGain.setSliderStyle (juce::Slider::LinearHorizontal);
    outGain.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    outGain.setPopupDisplayEnabled (true, true, nullptr);
    outGain.setTooltip (U ("OUT GAIN: volumen a la salida del módulo (doble clic = 0 dB)"));
    gainAtt = std::make_unique<APVTS::SliderAttachment> (p.apvts, TopRackProcessor::sid (s, "trim"), outGain);
    outGain.setDoubleClickReturnValue (true, 0.0);
    outGain.onValueChange = [this] { repaint (150, 592, 66, 22); };
    for (auto* c : { (juce::Component*) &power, (juce::Component*) &solo, (juce::Component*) &nameBox, (juce::Component*) &close,
                     (juce::Component*) &scInt, (juce::Component*) &scExt, (juce::Component*) &outGain })
        addAndMakeVisible (c);
    build();
}

void SlotColumn::build()
{
    builtType = proc.slotType (slot);
    knobs.clear(); toggles.clear();
    const auto& d = defs()[(size_t) builtType];
    const auto& fc = face (builtType);
    for (const auto& p : fc.pos)
    {
        if (p.k >= (int) d.knobs.size()) continue;
        const auto& kd = d.knobs[(size_t) p.k];
        if (p.style == kButton || p.style == kColumn)
        {
            auto* t = toggles.add (new RackToggle (proc, slot, p.k, kd, p.style == kColumn));
            if (p.style == kButton) t->setBounds ((int) (faceX + p.x - p.r), (int) (faceY + p.y - 11), (int) (p.r * 2), 22);
            else t->setBounds ((int) (faceX + p.x - p.r), (int) (faceY + p.y), (int) (p.r * 2), kd.steps.size() * 28);
            addAndMakeVisible (t);
            continue;
        }
        auto* kn = knobs.add (new RackKnob (proc, slot, p.k, kd));
        kn->getProperties().set ("style", p.style);
        kn->getProperties().set ("cap", (juce::int64) p.cap);
        kn->getProperties().set ("ink", (juce::int64) fc.ink);
        const int sz = (int) std::ceil (p.r * 2 + 14);
        kn->setBounds ((int) std::round (faceX + p.x) - sz / 2, (int) std::round (faceY + p.y) - sz / 2, sz, sz);
        addAndMakeVisible (kn);
    }
    const bool has = builtType != Empty;
    for (auto* c : { (juce::Component*) &power, (juce::Component*) &solo, (juce::Component*) &close, (juce::Component*) &outGain }) c->setVisible (has);
    scInt.setVisible (has && d.dynamics); scExt.setVisible (has && d.dynamics);
    nameBox.text = has ? upper (d.name) : U ("VACÍO");
    nameBox.repaint();
    repaint();
}

void SlotColumn::refresh() { if (proc.slotType (slot) != builtType) build(); }

void SlotColumn::tick()
{
    refresh();
    if (builtType == Empty) return;
    const bool off = proc.apvts.getRawParameterValue (TopRackProcessor::sid (slot, "byp"))->load() > 0.5f;
    if (power.on == off) { power.on = ! off; power.repaint(); repaint(); }
    const bool so = proc.soloSlot.load() == slot;
    if (solo.on != so) { solo.on = so; solo.repaint(); }
    const bool ext = proc.apvts.getRawParameterValue (TopRackProcessor::sid (slot, "sc"))->load() > 0.5f;
    if (scExt.on != ext) { scExt.on = ext; scInt.on = ! ext; scExt.repaint(); scInt.repaint(); }
    const auto& d = defs()[(size_t) builtType];
    float target;
    if (d.dynamics || face (builtType).meter == 2) target = proc.slotGr (slot);
    else target = trk::g2dB (proc.slotOut[(size_t) slot].load());
    meter += (target - meter) * (target > meter ? 0.45f : 0.18f);
    const int m = face (builtType).meter;
    if (m == 1) repaint (juce::Rectangle<float> (36, 48, 140, 86).translated (faceX, faceY).toNearestInt());
    else if (m == 2) repaint (juce::Rectangle<int> ((int) faceX + 26, (int) faceY + 54, 22, 300));
    else if (m == 3) repaint (juce::Rectangle<int> ((int) faceX + 50, (int) faceY + 50, 120, 22));
}

void SlotColumn::resized()
{
    power.setBounds (2, 2, 22, 22);
    solo.setBounds (26, 2, 20, 22);
    nameBox.setBounds (48, 2, 126, 22);
    close.setBounds (196, 2, 20, 22);
    outGain.setBounds (64, 594, 86, 18);
    scInt.setBounds (40, 621, 30, 18);
    scExt.setBounds (72, 621, 30, 18);
}

void SlotColumn::mouseDown (const juce::MouseEvent& e)
{
    if (onSelect) onSelect (slot);
    if (builtType == Empty && e.y > 28 && e.y < 588) moduleMenu();
}

void SlotColumn::moduleMenu()
{
    juce::PopupMenu m, add;
    const std::vector<std::pair<juce::String, std::vector<int>>> groups {
        { "EQ", { EqA, EqConsole, EqAmerican, AirEq } },
        { U ("Dinámica"), { CompFet, CompVca, CompOpto, CompBus, Stress, Monster, DeEss, Gate } },
        { "Color", { Revive, Bomber } }, { "Utilidad", { Trim } } };
    for (const auto& [name, types] : groups)
    {
        add.addSectionHeader (name);
        for (int t : types) add.addItem (100 + t, defs()[(size_t) t].name, true, t == builtType);
    }
    if (builtType == Empty) m = add;
    else
    {
        m.addSubMenu (U ("Cambiar módulo"), add);
        m.addItem (1, "Mover a la izquierda", slot > 0);
        m.addItem (2, "Mover a la derecha", slot < numSlots - 1);
        m.addSeparator();
        m.addItem (3, U ("Quitar el módulo"));
    }
    juce::Component::SafePointer<SlotColumn> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&nameBox), [safe] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& p = safe->proc;
        if (r >= 100) p.setSlotType (safe->slot, r - 100);
        else if (r == 1) { p.moveSlot (safe->slot, -1); if (safe->onSelect) safe->onSelect (safe->slot - 1); }
        else if (r == 2) { p.moveSlot (safe->slot, 1); if (safe->onSelect) safe->onSelect (safe->slot + 1); }
        else if (r == 3) p.setSlotType (safe->slot, Empty);
    });
}

bool SlotColumn::isInterestedInDragSource (const SourceDetails& d) { return d.description.toString().startsWith ("mod:"); }
void SlotColumn::itemDropped (const SourceDetails& d)
{
    dragOver = false;
    proc.setSlotType (slot, d.description.toString().fromFirstOccurrenceOf ("mod:", false, false).getIntValue());
    if (onSelect) onSelect (slot);
    repaint();
}

void SlotColumn::paint (juce::Graphics& g)
{
    const float w = (float) getWidth();
    g.setColour (juce::Colour (0xff131415)); g.fillAll();
    // cabecera
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff26272a), 0, 0, juce::Colour (0xff1c1d1f), 0, 26, false));
    g.fillRect (0.0f, 0.0f, w, 26.0f);
    g.setColour (juce::Colours::black); g.drawHorizontalLine (26, 0, w);
    g.setColour (selected ? amber : dim); g.setFont (font (13.0f, true));
    g.drawText (juce::String::charToString ((juce::juce_wchar) ('A' + slot)), 176, 2, 18, 22, juce::Justification::centred);
    if (builtType == Empty)
    {
        const auto r = juce::Rectangle<float> (faceX, faceY, faceW, faceH);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0b0b0c), r.getX(), r.getY(), juce::Colour (0xff151618), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 3);
        // rieles del rack
        for (float x : { r.getX() + 4, r.getRight() - 12 })
        {
            g.setColour (juce::Colour (0xff1e1f22)); g.fillRect (x, r.getY() + 4, 8.0f, r.getHeight() - 8);
            g.setColour (juce::Colour (0xff060607));
            for (float y = r.getY() + 14; y < r.getBottom() - 10; y += 22) g.fillRoundedRectangle (x + 2, y, 4, 8, 2);
        }
        juce::Path box; box.addRoundedRectangle (r.reduced (22, 40), 8);
        juce::Path dash; const float pat[] { 6, 5 };
        juce::PathStrokeType (1.3f).createDashedStroke (dash, box, pat, 2);
        g.setColour (dragOver ? amber : (selected ? dim : line)); g.fillPath (dash);
        const auto c = r.getCentre();
        g.setColour (dragOver ? amber : dim);
        g.drawEllipse (c.x - 18, c.y - 48, 36, 36, 1.5f);
        g.drawLine (c.x - 8, c.y - 30, c.x + 8, c.y - 30, 1.8f); g.drawLine (c.x, c.y - 38, c.x, c.y - 22, 1.8f);
        g.setFont (font (13.0f, true)); g.drawText (U ("Añadir módulo"), juce::Rectangle<float> (r.getX(), c.y - 4, r.getWidth(), 18), juce::Justification::centred);
        g.setFont (font (10.5f)); g.setColour (dim.withAlpha (0.7f));
        g.drawFittedText (U ("clic aquí o arrastra uno\ndesde el navegador"), juce::Rectangle<float> (r.getX() + 30, c.y + 16, r.getWidth() - 60, 34).toNearestInt(), juce::Justification::centred, 2);
        return;
    }
    g.saveState();
    g.addTransform (juce::AffineTransform::translation (faceX, faceY));
    drawFace (g, builtType, false);
    const auto& fc = face (builtType);
    if (fc.meter == 1)
    {
        const bool dyn = defs()[(size_t) builtType].dynamics;
        drawVuNeedle (g, dyn ? 1.0f - meter / 20.0f : (meter + 20.0f) / 23.0f);
    }
    if (fc.meter == 2) drawLeds (g, meter, false, juce::Colour (fc.ink));
    if (fc.meter == 3) drawGateLeds (g, meter, juce::Colour (fc.ink));
    g.restoreState();
    // fila OUT GAIN
    g.setColour (dim); g.setFont (font (10.0f, true));
    g.drawText ("OUT GAIN", 6, 592, 58, 22, juce::Justification::centredLeft);
    g.setColour (text.withAlpha (0.85f)); g.setFont (font (10.5f));
    g.drawText (juce::String (outGain.getValue(), 2) + "dB", 150, 592, 64, 22, juce::Justification::centredRight);
    if (defs()[(size_t) builtType].dynamics)
    {
        const auto b = juce::Rectangle<float> (6, 619, 100, 22);
        g.setColour (juce::Colour (0xff0d0d0e)); g.fillRoundedRectangle (b, 4);
        g.setColour (line); g.drawRoundedRectangle (b, 4, 1.0f);
        g.setColour (dim); g.setFont (font (9.5f, true));
        g.drawText ("S/C", 8, 619, 30, 22, juce::Justification::centred);
    }
}

void SlotColumn::paintOverChildren (juce::Graphics& g)
{
    if (builtType != Empty)
    {
        // marcas de macro sobre las perillas
        for (auto* k : knobs)
        {
            const int m = proc.macroOf (slot, k->knob);
            if (m < 0) continue;
            const auto b = k->getBounds().toFloat();
            g.setColour (amber); g.fillEllipse (b.getRight() - 12, b.getY(), 13, 13);
            g.setColour (juce::Colours::black); g.setFont (font (9.0f, true));
            g.drawText (juce::String (m + 1), juce::Rectangle<float> (b.getRight() - 12, b.getY(), 13, 13), juce::Justification::centred);
        }
        if (! power.on)
        {
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.fillRect (juce::Rectangle<float> (faceX, faceY, faceW, faceH));
        }
        if (proc.soloSlot.load() >= 0 && proc.soloSlot.load() != slot)
        {
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillRect (juce::Rectangle<float> (faceX, faceY, faceW, faceH));
        }
        if (dragOver) { g.setColour (amber.withAlpha (0.25f)); g.fillRect (juce::Rectangle<float> (faceX, faceY, faceW, faceH)); }
    }
    if (selected) { g.setColour (amber.withAlpha (0.9f)); g.drawRect (getLocalBounds().toFloat().withHeight (26.0f), 1.2f); }
}

//==============================================================================
ModuleList::ModuleList() { setFilter ("Todos", {}); }
void ModuleList::setFilter (const juce::String& cat, const juce::String& search)
{
    items.clear();
    const int order[] { EqA, EqConsole, EqAmerican, AirEq, CompFet, CompVca, CompOpto, CompBus, Stress, Monster, DeEss, Gate, Revive, Bomber, Trim };
    for (int t : order)
    {
        const auto& d = defs()[(size_t) t];
        const bool catOk = cat == "Todos" || d.category == cat || (cat == U ("Dinámica") && d.category == U ("Dinámica"));
        const bool sOk = search.isEmpty() || d.name.containsIgnoreCase (search) || d.category.containsIgnoreCase (search);
        if (catOk && sOk) items.push_back (t);
    }
    setSize (getWidth() > 0 ? getWidth() : 210, juce::jmax (40, (int) items.size() * rowH));
    hover = -1;
    repaint();
}
void ModuleList::paint (juce::Graphics& g)
{
    for (size_t i = 0; i < items.size(); ++i)
    {
        const auto row = juce::Rectangle<float> (0, (float) i * rowH, (float) getWidth(), (float) rowH);
        if ((int) i == hover) { g.setColour (juce::Colour (0xff26272b)); g.fillRect (row); }
        g.setColour (juce::Colour (0xff222326)); g.drawHorizontalLine ((int) row.getBottom() - 1, 6, row.getRight() - 6);
        const auto img = thumbnail (items[i]);
        const auto tb = juce::Rectangle<float> (12, row.getY() + 7, 54, 142);
        g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillRect (tb.translated (2, 3));
        g.drawImage (img, tb, juce::RectanglePlacement::stretchToFit);
        const auto& d = defs()[(size_t) items[i]];
        g.setColour (text); g.setFont (font (13.0f, true));
        g.drawFittedText (d.name, juce::Rectangle<float> (72, row.getCentreY() - 22, row.getWidth() - 82, 20).toNearestInt(), juce::Justification::centredRight, 1);
        g.setColour (dim); g.setFont (font (10.5f));
        g.drawText (d.category, juce::Rectangle<float> (72, row.getCentreY() - 2, row.getWidth() - 82, 14), juce::Justification::centredRight);
    }
    if (items.empty()) { g.setColour (dim); g.setFont (font (12.0f)); g.drawText ("Sin resultados", getLocalBounds(), juce::Justification::centredTop); }
}
void ModuleList::mouseMove (const juce::MouseEvent& e)
{
    const int h = itemAt (e.y);
    if (h != hover) { hover = h; repaint(); setTooltip (h >= 0 ? defs()[(size_t) items[(size_t) h]].help + U ("\n(clic o arrastra al rack)") : juce::String()); }
}
void ModuleList::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 6) return;
    const int i = itemAt (e.getMouseDownY());
    if (i < 0) return;
    if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor (this))
    {
        dragging = true;
        c->startDragging ("mod:" + juce::String (items[(size_t) i]), this, juce::ScaledImage (thumbnail (items[(size_t) i]), 1.4));
    }
}
void ModuleList::mouseUp (const juce::MouseEvent& e)
{
    if (! dragging) { const int i = itemAt (e.y); if (i >= 0 && onPick) onPick (items[(size_t) i]); }
    dragging = false;
}

//==============================================================================
TopRackEditor::TopRackEditor (TopRackProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&look);
    tips.setLookAndFeel (&look);
    // ---- barra superior
    logo.fontSize = 14.0f; logo.setTooltip ("Top Rack · by Dj Pava Hm Music");
    menuBtn.onClick = [this] (const juce::MouseEvent&)
    {
        juce::PopupMenu m;
        m.addItem (1, U ("Rack vacío"));
        m.addItem (2, U ("Quitar el solo"), proc.soloSlot.load() >= 0);
        m.addSeparator();
        m.addItem (3, "Acerca de Top Rack");
        juce::Component::SafePointer<TopRackEditor> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&menuBtn), [safe] (int r)
        {
            if (safe == nullptr) return;
            if (r == 1) safe->proc.setCurrentProgram (0);
            if (r == 2) safe->proc.soloSlot = -1;
            if (r == 3) juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Top Rack",
                U ("Rack de 8 ranuras con 15 módulos de estilo clásico.\nArrastra módulos desde el navegador, usa S para escuchar uno solo y OUT GAIN para ajustar niveles.\n\nDiseño propio de Dj Pava Hm Music."));
        });
    };
    menuBtn.framed = false;
    presetUp.onClick = [this] (const juce::MouseEvent&) { const int n = proc.getNumPrograms(); proc.setCurrentProgram ((proc.getCurrentProgram() - 1 + n) % n); };
    presetDown.onClick = [this] (const juce::MouseEvent&) { proc.setCurrentProgram ((proc.getCurrentProgram() + 1) % proc.getNumPrograms()); };
    undoBtn.onClick = [this] (const juce::MouseEvent&) { proc.undoManager.undo(); };
    redoBtn.onClick = [this] (const juce::MouseEvent&) { proc.undoManager.redo(); };
    undoBtn.setTooltip ("Deshacer"); redoBtn.setTooltip ("Rehacer");
    aBtn.onClick = [this] (const juce::MouseEvent&) { switchAB (0); };
    bBtn.onClick = [this] (const juce::MouseEvent&) { switchAB (1); };
    aBtn.setTooltip ("Ajustes A"); bBtn.setTooltip ("Ajustes B");
    copyBtn.setTooltip ("Copiar el ajuste actual al otro (A → B o B → A)");
    copyBtn.onClick = [this] (const juce::MouseEvent&) { proc.abStore[1 - proc.abSlot] = proc.snapshot(); };
    trashBtn.setTooltip (U ("Vaciar el rack"));
    trashBtn.onClick = [this] (const juce::MouseEvent&) { for (int s = 0; s < numSlots; ++s) proc.setSlotType (s, Empty); proc.soloSlot = -1; };
    hqBtn.setTooltip (U ("HQ: oversampling 2x, saturaciones más limpias (más CPU)"));
    hqBtn.onClick = [this] (const juce::MouseEvent&) { if (auto* prm = proc.apvts.getParameter ("oversample")) prm->setValueNotifyingHost (prm->getValue() > 0.5f ? 0.0f : 1.0f); };
    macroBtn.setTooltip ("Macros");
    macroBtn.onClick = [this] (const juce::MouseEvent&) { showTab (! modulesTab); };
    for (auto* b : { &logo, &menuBtn, &presetUp, &presetDown, &undoBtn, &redoBtn, &aBtn, &copyBtn, &bBtn, &trashBtn, &hqBtn, &macroBtn }) addAndMakeVisible (b);
    presetUp.framed = presetDown.framed = false;
    const juce::StringArray gids { "in_gain", "out_gain", "mix" };
    for (const auto& id : gids)
    {
        auto* s = globals.add (new juce::Slider());
        s->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s->setRotaryParameters (A0, A1, true);
        s->setPopupDisplayEnabled (true, true, nullptr);
        s->getProperties().set ("scale", false);
        gAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, *s));
        s->setDoubleClickReturnValue (true, id == "mix" ? 100.0 : 0.0);
        addAndMakeVisible (s);
    }
    globals[0]->setTooltip ("Entrada del rack"); globals[1]->setTooltip ("Salida del rack"); globals[2]->setTooltip ("Mezcla seco / procesado");
    // ---- navegador
    const juce::StringArray catNames { "Todos", U ("Dinámica"), "EQ", "Color", "Utilidad" };
    for (const auto& c : catNames)
    {
        auto* b = cats.add (new trui::IconButton (trui::IconButton::none, c));
        b->fontSize = 11.5f;
        b->onClick = [this, c] (const juce::MouseEvent&) { category = c; list.setFilter (category, search.getText()); };
        addAndMakeVisible (b);
    }
    search.setTextToShowWhenEmpty (U ("Buscar módulos..."), dim);
    search.setFont (font (13.0f));
    search.setIndents (8, 5);
    search.onTextChange = [this] { list.setFilter (category, search.getText()); };
    addAndMakeVisible (search);
    listView.setViewedComponent (&list, false);
    listView.setScrollBarsShown (true, false);
    listView.setScrollBarThickness (8);
    addAndMakeVisible (listView);
    list.onPick = [this] (int t) { pick (t); };
    for (int m = 0; m < 4; ++m)
    {
        auto* s = macros.add (new juce::Slider());
        s->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s->setRotaryParameters (A0, A1, true);
        s->getProperties().set ("style", (int) kCap);
        s->getProperties().set ("cap", (juce::int64) 0xffd99a1e);
        s->setTooltip (U ("Macro: mueve a la vez todas las perillas asignadas (clic derecho en una perilla para asignarla)"));
        gAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "macro" + juce::String (m + 1), *s));
        addChildComponent (s);
        auto* l = macroLabels.add (new juce::Label ({}, proc.macroNames[m]));
        l->setEditable (false, true);
        l->setJustificationType (juce::Justification::centred);
        l->setFont (font (13.0f, true));
        l->onTextChange = [this, m, l] { proc.macroNames.set (m, l->getText()); };
        l->setTooltip ("Doble clic para cambiar el nombre");
        addChildComponent (l);
    }
    // ---- rack
    homeBtn.setTooltip (U ("Rack vacío"));
    homeBtn.onClick = [this] (const juce::MouseEvent&) { proc.setCurrentProgram (0); };
    addAndMakeVisible (homeBtn);
    for (int i = 0; i < 8; ++i)
    {
        auto* b = dreams.add (new trui::IconButton (trui::IconButton::none, "Dream " + juce::String (i + 1)));
        b->fontSize = 11.0f;
        b->setTooltip (U ("Dream Strip: clic = guardar la cadena actual (si está vacío) o cargarla. Clic derecho = guardar / borrar."));
        b->onClick = [this, i] (const juce::MouseEvent& e) { dreamClick (i, e); };
        addAndMakeVisible (b);
    }
    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&rackHolder, false);
    viewport.setScrollBarsShown (false, true);
    viewport.setScrollBarThickness (9);
    for (int s = 0; s < numSlots; ++s)
    {
        auto* sp = slots.add (new trui::SlotColumn (proc, s));
        sp->onSelect = [this] (int i) { selectSlot (i); };
        rackHolder.addAndMakeVisible (sp);
    }
    rackHolder.setSize (numSlots * trui::SlotColumn::colW, trui::SlotColumn::colH);
    for (int s = 0; s < numSlots; ++s) slots[s]->setBounds (s * trui::SlotColumn::colW, 0, trui::SlotColumn::colW - 2, trui::SlotColumn::colH);
    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.onChange = [this] { if (presetBox.getSelectedId() > 0) proc.setCurrentProgram (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);

    setResizable (true, true);
    setResizeLimits (W * 2 / 3, H * 2 / 3, W * 3 / 2, H * 3 / 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) W / H);
    setSize (W, H);
    showTab (true);
    selectSlot (0);
    startTimerHz (30);
}

TopRackEditor::~TopRackEditor()
{
    stopTimer();
    tips.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void TopRackEditor::showTab (bool mods)
{
    modulesTab = mods;
    for (auto* b : cats) b->setVisible (mods);
    search.setVisible (mods); listView.setVisible (mods);
    for (auto* s : macros) s->setVisible (! mods);
    for (auto* l : macroLabels) l->setVisible (! mods);
    macroBtn.on = ! mods; macroBtn.repaint();
    repaint();
}

void TopRackEditor::selectSlot (int s)
{
    selected = juce::jlimit (0, numSlots - 1, s);
    for (auto* sp : slots) { sp->selected = sp->slot == selected; sp->repaint(); }
    const int colW = trui::SlotColumn::colW, vw = viewport.getViewWidth() > 0 ? viewport.getViewWidth() : 1104;
    const int x = viewport.getViewPositionX();
    if (selected * colW < x) viewport.setViewPosition (selected * colW, 0);
    else if ((selected + 1) * colW > x + vw) viewport.setViewPosition ((selected + 1) * colW - vw, 0);
    repaint (juce::Rectangle<int> (272, 742, 1104, 24).transformedBy (juce::AffineTransform::scale ((float) getWidth() / (float) W)));
}

void TopRackEditor::pick (int type)
{
    int target = selected;
    if (proc.slotType (target) != Empty)
        for (int s = 0; s < numSlots; ++s) if (proc.slotType (s) == Empty) { target = s; break; }
    proc.setSlotType (target, type);
    selectSlot (target);
}

void TopRackEditor::switchAB (int s)
{
    if (s == proc.abSlot) return;
    proc.abStore[proc.abSlot] = proc.snapshot();
    if (proc.abStore[s].isValid()) proc.recall (proc.abStore[s]);
    proc.abSlot = s;
}

void TopRackEditor::dreamClick (int i, const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        juce::PopupMenu m;
        m.addItem (1, U ("Guardar la cadena actual aquí"));
        m.addItem (2, "Borrar", proc.dream[(size_t) i].isValid());
        juce::Component::SafePointer<TopRackEditor> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (dreams[i]), [safe, i] (int r)
        {
            if (safe == nullptr) return;
            if (r == 1) safe->proc.storeDream (i);
            else if (r == 2) safe->proc.dream[(size_t) i] = juce::ValueTree();
        });
        return;
    }
    if (! proc.recallDream (i)) proc.storeDream (i);
}

void TopRackEditor::presetMenu()
{
    juce::PopupMenu m;
    for (int i = 0; i < proc.getNumPrograms(); ++i) m.addItem (i + 1, proc.getProgramName (i), true, i == proc.getCurrentProgram());
    juce::Component::SafePointer<TopRackEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (juce::Rectangle<int> (76, 7, 312, 36).transformedBy (juce::AffineTransform::scale ((float) getWidth() / (float) W)))),
                     [safe] (int r) { if (safe != nullptr && r > 0) safe->proc.setCurrentProgram (r - 1); });
}

void TopRackEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position / ((float) getWidth() / (float) W);
    if (juce::Rectangle<float> (76, 7, 312, 36).contains (p)) { presetMenu(); return; }
    if (juce::Rectangle<float> (4, 58, 36, 108).contains (p)) { showTab (true); return; }
    if (juce::Rectangle<float> (4, 172, 36, 90).contains (p)) { showTab (false); return; }
    if (juce::Rectangle<float> (272, 742, 1104, 22).contains (p)) { selectSlot ((int) ((p.x - 272) / 138.0f)); return; }
}

void TopRackEditor::resized()
{
    const float s = (float) getWidth() / (float) W;
    const auto T = juce::AffineTransform::scale (s);
    auto put = [&] (juce::Component& c, int x, int y, int w, int h) { c.setBounds (x, y, w, h); c.setTransform (T); };
    put (logo, 12, 9, 32, 32);
    put (menuBtn, 48, 9, 24, 32);
    put (presetUp, 392, 6, 20, 19);
    put (presetDown, 392, 25, 20, 19);
    put (undoBtn, 560, 9, 34, 32); put (redoBtn, 598, 9, 34, 32);
    put (aBtn, 646, 9, 30, 32); put (copyBtn, 680, 9, 22, 32); put (bBtn, 706, 9, 30, 32);
    put (trashBtn, 746, 9, 34, 32); put (hqBtn, 788, 9, 44, 32); put (macroBtn, 838, 9, 34, 32);
    for (int i = 0; i < 3; ++i) put (*globals[i], 1080 + i * 84, 6, 38, 38);
    // navegador
    const int cx[] { 50, 120, 202, 50, 158 }, cy[] { 56, 56, 56, 82, 82 }, cw[] { 68, 80, 62, 106, 106 };
    for (int i = 0; i < cats.size(); ++i) put (*cats[i], cx[i], cy[i], cw[i], 24);
    put (search, 76, 112, 188, 26);
    put (listView, 46, 144, 222, 622);
    list.setSize (listView.getMaximumVisibleWidth() > 0 ? 214 : 214, list.getHeight());
    for (int m = 0; m < 4; ++m)
    {
        put (*macroLabels[m], 60, 116 + m * 158, 194, 22);
        put (*macros[m], 120, 140 + m * 158, 74, 74);
    }
    // rack
    put (homeBtn, 274, 55, 28, 24);
    for (int i = 0; i < 8; ++i) put (*dreams[i], 308 + i * 104, 55, 100, 24);
    put (viewport, 272, 84, 1104, trui::SlotColumn::colH + 10);
    put (presetBox, 1176, 780, 196, 30);
}

void TopRackEditor::paint (juce::Graphics& g)
{
    const float sc = (float) getWidth() / (float) W;
    g.addTransform (juce::AffineTransform::scale (sc));
    g.fillAll (bgDark);
    // barra superior
    g.setGradientFill (juce::ColourGradient (barTop, 0, 0, barBot, 0, 50, false));
    g.fillRect (0, 0, W, 50);
    g.setColour (juce::Colours::black); g.drawHorizontalLine (50, 0, (float) W);
    {
        const auto r = juce::Rectangle<float> (76, 7, 312, 36);
        g.setColour (juce::Colour (0xff0c0c0d)); g.fillRoundedRectangle (r, 4);
        g.setColour (line); g.drawRoundedRectangle (r, 4, 1.0f);
        g.setColour (dim); g.setFont (font (10.5f));
        g.drawText ("Cadena", 86, 9, 200, 13, juce::Justification::centredLeft);
        g.setColour (text); g.setFont (font (14.5f, true));
        g.drawText (proc.getProgramName (proc.getCurrentProgram()), 86, 21, 290, 19, juce::Justification::centredLeft);
    }
    g.setColour (dim); g.setFont (font (9.5f, true));
    const char* gl[] { "ENTRADA", "SALIDA", "MEZCLA" };
    for (int i = 0; i < 3; ++i) g.drawText (gl[i], 1080 + i * 84 + 38, 18, 50, 14, juce::Justification::centredLeft);
    // pestanas verticales
    g.setColour (juce::Colour (0xff141517)); g.fillRect (0, 51, 44, 721);
    auto vtab = [&g] (juce::Rectangle<float> r, const juce::String& t, bool on)
    {
        g.setColour (on ? juce::Colour (0xff2a2b2f) : juce::Colour (0xff1c1d20)); g.fillRoundedRectangle (r, 5);
        g.setColour (on ? amber : line); g.drawRoundedRectangle (r, 5, 1.0f);
        g.saveState();
        g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, r.getCentreX(), r.getCentreY()));
        g.setColour (on ? amber : dim); g.setFont (font (12.5f, true));
        g.drawText (t, juce::Rectangle<float> (r.getCentreX() - r.getHeight() * 0.5f, r.getCentreY() - 10, r.getHeight(), 20), juce::Justification::centred);
        g.restoreState();
    };
    vtab ({ 6, 60, 32, 104 }, U ("Módulos"), modulesTab);
    vtab ({ 6, 172, 32, 88 }, "Macro", ! modulesTab);
    // navegador
    g.setColour (panel); g.fillRect (44, 51, 228, 721);
    g.setColour (juce::Colours::black); g.drawVerticalLine (271, 51, 772);
    if (modulesTab)
    {
        g.setColour (dim);
        for (int i = -1; i <= 1; ++i) g.fillRoundedRectangle (52.0f, 125.0f + (float) i * 5.0f - 1.0f, 16.0f, 2.0f, 1.0f);
    }
    else
    {
        g.setColour (dim); g.setFont (font (11.0f));
        g.drawFittedText (U ("Clic derecho en cualquier perilla de un módulo → asignarla a una macro.\nDoble clic en el nombre para cambiarlo."),
                          juce::Rectangle<int> (56, 62, 204, 44), juce::Justification::centredLeft, 4);
    }
    // fondo del rack
    g.setColour (juce::Colour (0xff101112)); g.fillRect (272, 51, 1108, 721);
    // pestanas inferiores de las ranuras
    for (int i = 0; i < numSlots; ++i)
    {
        const auto r = juce::Rectangle<float> (272.0f + (float) i * 138.0f, 744.0f, 137.0f, 22.0f);
        const bool on = i == selected;
        g.setColour (on ? juce::Colour (0xff2b2c30) : juce::Colour (0xff1b1c1f)); g.fillRect (r);
        g.setColour (juce::Colours::black); g.drawRect (r, 1.0f);
        const int t = proc.slotType (i);
        g.setColour (on ? text : dim); g.setFont (font (11.0f, true));
        g.drawText (juce::String::charToString ((juce::juce_wchar) ('A' + i)) + "   " + (t > 0 ? defs()[(size_t) t].name : U ("—")), r, juce::Justification::centred);
    }
    // barra inferior
    g.setGradientFill (juce::ColourGradient (barTop, 0, 772, barBot, 0, (float) H, false));
    g.fillRect (0, 772, W, H - 772);
    g.setColour (juce::Colours::black); g.drawHorizontalLine (772, 0, (float) W);
    g.setColour (text); g.setFont (font (15.0f, true));
    g.drawText ("TOP RACK", 16, 784, 120, 24, juce::Justification::centredLeft);
    g.setColour (dim); g.setFont (font (11.0f));
    g.drawText ("by Dj Pava Hm Music", 106, 786, 200, 22, juce::Justification::centredLeft);
    g.setColour (text); g.setFont (font (13.0f));
    g.drawText ("Presets:", 1100, 780, 70, 30, juce::Justification::centredRight);
}

void TopRackEditor::paintOverChildren (juce::Graphics& g)
{
    // medidor de salida del rack (arriba a la derecha)
    g.addTransform (juce::AffineTransform::scale ((float) getWidth() / (float) W));
    const float pk = juce::jlimit (0.0f, 1.0f, (20.0f * std::log10 (proc.outPeak.load() + 1e-6f) + 60.0f) / 60.0f);
    const auto r = juce::Rectangle<float> (1340, 8, 14, 34);
    g.setColour (juce::Colour (0xff070708)); g.fillRoundedRectangle (r, 3);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffff4a3c), r.getX(), r.getY(), juce::Colour (0xff47d36b), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r.withTop (r.getBottom() - r.getHeight() * pk), 3);
    g.setColour (dim); g.setFont (font (8.5f, true));
    g.drawText ("OUT", 1358, 8, 22, 34, juce::Justification::centredLeft);
}

void TopRackEditor::timerCallback()
{
    for (auto* s : slots) s->tick();
    const bool hq = proc.apvts.getRawParameterValue ("oversample")->load() > 0.5f;
    if (hqBtn.on != hq) { hqBtn.on = hq; hqBtn.repaint(); }
    if (aBtn.on != (proc.abSlot == 0)) { aBtn.on = proc.abSlot == 0; bBtn.on = ! aBtn.on; aBtn.repaint(); bBtn.repaint(); }
    for (int i = 0; i < 8; ++i)
    {
        const bool saved = proc.dream[(size_t) i].isValid();
        if (dreams[i]->on != saved) { dreams[i]->on = saved; dreams[i]->repaint(); }
    }
    for (auto* c : cats) { const bool on = c->text == category; if (c->on != on) { c->on = on; c->repaint(); } }
    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1) { presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification); repaint(); }
    for (int m = 0; m < 4; ++m) if (macroLabels[m]->getText() != proc.macroNames[m] && ! macroLabels[m]->isBeingEdited()) macroLabels[m]->setText (proc.macroNames[m], juce::dontSendNotification);
    const float sc = (float) getWidth() / (float) W;
    repaint (juce::Rectangle<int> (1336, 4, 44, 42).transformedBy (juce::AffineTransform::scale (sc)));
    static int tabTick = 0;
    if (++tabTick >= 10) { tabTick = 0; repaint (juce::Rectangle<int> (272, 742, 1104, 26).transformedBy (juce::AffineTransform::scale (sc))); }
    if (++transactionTick >= 15)
    {
        transactionTick = 0;
        if (! juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown()) proc.undoManager.beginNewTransaction();
    }
}
