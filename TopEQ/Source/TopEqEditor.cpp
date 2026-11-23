#include "TopEqEditor.h"

using namespace teqi;
using juce::Colour; namespace Colours = juce::Colours; using juce::Graphics; using juce::Path; using juce::Rectangle; using juce::ColourGradient;

namespace
{
    juce::String U (const char* s) { return juce::String::fromUTF8 (s); }
    juce::Font font (float h, bool bold = false) { return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain)); }
    constexpr float pi = juce::MathConstants<float>::pi, twoPi = juce::MathConstants<float>::twoPi;
    constexpr float A0 = -pi * 0.8f, A1 = pi * 0.8f;               // perillas 0..10 (0 abajo a la izquierda)
    const Colour slate (0xff3d5461), silkCol (0xffe9edf0), ink (0xff101315);
    float param (TopEqProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); }
    void setParam (TopEqProcessor& p, const char* id, float v)
    { if (auto* q = p.apvts.getParameter (id)) { q->beginChangeGesture(); q->setValueNotifyingHost (q->convertTo0to1 (v)); q->endChangeGesture(); } }
    juce::Point<float> polar (juce::Point<float> c, float r, float a) { return { c.x + r * std::sin (a), c.y - r * std::cos (a) }; }

    struct Sel { const char* id; int n; float a0, a1; };
    const Sel selLow { "lowFreq", 4, -pi * 0.33f, pi * 0.33f }, selHigh { "highFreq", 7, -pi * 0.62f, pi * 0.62f }, selAtt { "attFreq", 3, -pi * 0.25f, pi * 0.25f };

    // posiciones en el panel (coordenadas del contenido, el panel empieza en y = strip)
    constexpr float rowTop = 252, rowBot = 466;
    constexpr float xLB = 455, xLA = 660, xCPS = 557, xHB = 905, xBW = 860, xKCS = 1060, xHA = 1270, xSEL = 1270;
    constexpr float yBW = 446;
    constexpr float knobD = 160, selD = 126, kF = 1.5f, kC = 1.45f;     // el componente es mas grande que la perilla (sombras)

    // texto blanco serigrafiado con espaciado
    void silk (Graphics& g, const juce::String& t, Rectangle<float> r, float size, bool bold = true, juce::Justification j = juce::Justification::centred, float alpha = 0.95f)
    {
        auto f = font (size, bold).withExtraKerningFactor (0.06f);
        g.setFont (f);
        g.setColour (Colours::black.withAlpha (0.25f)); g.drawText (t, r.translated (0.6f, 0.8f), j);
        g.setColour (silkCol.withAlpha (alpha)); g.drawText (t, r, j);
    }
    void screw (Graphics& g, float x, float y, float r = 7.0f)
    {
        g.setColour (Colours::black.withAlpha (0.35f)); g.fillEllipse (x - r + 1, y - r + 2, r * 2, r * 2);
        g.setGradientFill (ColourGradient (Colour (0xfff2f3f5), x - r * 0.6f, y - r * 0.7f, Colour (0xff6d7177), x + r, y + r, true));
        g.fillEllipse (x - r, y - r, r * 2, r * 2);
        g.setColour (Colours::black.withAlpha (0.5f)); g.drawEllipse (x - r, y - r, r * 2, r * 2, 0.8f);
        // cabeza Phillips
        g.setColour (Colour (0xff3a3d42));
        g.drawLine (x - r * 0.55f, y, x + r * 0.55f, y, 1.8f); g.drawLine (x, y - r * 0.55f, x, y + r * 0.55f, 1.8f);
        g.setColour (Colours::white.withAlpha (0.5f));
        g.drawLine (x - r * 0.55f, y + 1, x + r * 0.55f, y + 1, 0.6f);
    }

    // ------------------------------------------------------------------ perilla estriada de baquelita
    void drawFluted (Graphics& g, juce::Point<float> c, float R, float ang)
    {
        const float rs = R * 1.17f;                         // falda
        // sombra suave sobre el panel
        for (int i = 6; i >= 1; --i)
        {
            const float e = (float) i * R * 0.035f;
            g.setColour (Colours::black.withAlpha (0.07f));
            g.fillEllipse (c.x - rs - e * 0.4f + R * 0.10f, c.y - rs - e * 0.3f + R * 0.18f, (rs + e * 0.6f) * 2, (rs + e * 0.6f) * 2);
        }
        // falda: disco negro brillante
        g.setGradientFill (ColourGradient (Colour (0xff2b2c2e), c.x - rs * 0.6f, c.y - rs * 0.7f, Colour (0xff050506), c.x + rs * 0.6f, c.y + rs * 0.8f, false));
        g.fillEllipse (c.x - rs, c.y - rs, rs * 2, rs * 2);
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.22f), c.x - rs * 0.5f, c.y - rs * 0.9f, Colours::transparentWhite, c.x, c.y - rs * 0.2f, false));
        g.drawEllipse (c.x - rs + 1.5f, c.y - rs + 1.5f, rs * 2 - 3, rs * 2 - 3, 2.0f);
        g.setColour (Colours::black); g.drawEllipse (c.x - rs, c.y - rs, rs * 2, rs * 2, 1.0f);
        // linea indicadora blanca sobre la falda
        g.setColour (Colour (0xfff4f1ea));
        g.drawLine ({ polar (c, R * 0.95f, ang), polar (c, rs * 0.98f, ang) }, 4.0f);

        // cuerpo estriado: 18 estrias que giran con la perilla
        constexpr int flutes = 18;
        Path body;
        for (int i = 0; i <= 360; ++i)
        {
            const float t = (float) i / 360.0f * twoPi;
            const float s = 0.5f + 0.5f * std::cos ((t - ang) * flutes);
            const float r = R * (0.93f + 0.07f * std::pow (s, 0.6f));
            const auto p = polar (c, r, t);
            if (i == 0) body.startNewSubPath (p); else body.lineTo (p);
        }
        body.closeSubPath();
        g.setColour (Colour (0xff09090a)); g.fillPath (body);
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (body);
            const float light = -pi * 0.30f;             // luz arriba a la izquierda
            constexpr int seg = 216;
            for (int i = 0; i < seg; ++i)
            {
                const float t0 = (float) i / seg * twoPi, t1 = (float) (i + 1) / seg * twoPi, tm = 0.5f * (t0 + t1);
                // pendiente de la estria: lado que mira a la luz se ilumina
                const float slope = -std::sin ((tm - ang) * flutes);
                const float facing = std::cos (tm - light);
                const float b = juce::jlimit (0.0f, 1.0f, 0.10f + 0.10f * facing + 0.16f * slope * (0.5f + 0.5f * facing) + 0.12f * std::max (0.0f, facing));
                Path w; w.startNewSubPath (c); w.lineTo (polar (c, R * 1.05f, t0)); w.lineTo (polar (c, R * 1.05f, t1)); w.closeSubPath();
                g.setColour (Colour::greyLevel (b * 0.55f)); g.fillPath (w);
            }
            // brillo vertical tipo baquelita
            g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.16f), c.x - R * 0.6f, c.y - R, Colours::transparentWhite, c.x, c.y, false));
            g.fillPath (body);
        }
        g.setColour (Colours::black.withAlpha (0.9f)); g.strokePath (body, juce::PathStrokeType (1.0f));

        // tapa superior de metal oscuro cepillado (brillo conico fijo, como la luz)
        const float rc = R * 0.74f;
        g.setColour (Colours::black.withAlpha (0.6f)); g.fillEllipse (c.x - rc - 2, c.y - rc - 1, rc * 2 + 4, rc * 2 + 4);
        constexpr int cs = 180;
        for (int i = 0; i < cs; ++i)
        {
            const float t0 = (float) i / cs * twoPi, t1 = (float) (i + 1) / cs * twoPi, tm = 0.5f * (t0 + t1);
            const float b = 0.19f + 0.30f * std::pow (std::abs (std::cos (tm + pi * 0.22f)), 4.0f) + 0.025f * std::sin (tm * 37.0f);
            Path w; w.startNewSubPath (c); w.lineTo (polar (c, rc, t0)); w.lineTo (polar (c, rc, t1)); w.closeSubPath();
            g.setColour (Colour::fromFloatRGBA (b * 0.95f, b, b * 1.04f, 1.0f)); g.fillPath (w);
        }
        g.setGradientFill (ColourGradient (Colours::transparentBlack, c.x, c.y, Colours::black.withAlpha (0.45f), c.x + rc, c.y, true));
        g.fillEllipse (c.x - rc, c.y - rc, rc * 2, rc * 2);
        g.setColour (Colours::white.withAlpha (0.25f)); g.drawEllipse (c.x - rc + 1, c.y - rc + 1, rc * 2 - 2, rc * 2 - 2, 1.0f);
        g.setColour (Colours::black.withAlpha (0.8f)); g.drawEllipse (c.x - rc, c.y - rc, rc * 2, rc * 2, 1.2f);
    }

    // ------------------------------------------------------------------ selector "cabeza de pollo"
    void drawChicken (Graphics& g, juce::Point<float> c, float R, float ang)
    {
        const float hub = R * 0.62f;
        Path ptr;
        // puntero largo de baquelita (punta redondeada, cola corta)
        ptr.startNewSubPath (-R * 0.17f, R * 0.52f);
        ptr.quadraticTo (-R * 0.30f, 0.0f, -R * 0.16f, -R * 1.02f);
        ptr.quadraticTo (0.0f, -R * 1.16f, R * 0.16f, -R * 1.02f);
        ptr.quadraticTo (R * 0.30f, 0.0f, R * 0.17f, R * 0.52f);
        ptr.quadraticTo (0.0f, R * 0.66f, -R * 0.17f, R * 0.52f);
        ptr.closeSubPath();
        const auto xf = juce::AffineTransform::rotation (ang).translated (c.x, c.y);
        // sombras
        for (int i = 4; i >= 1; --i)
        {
            g.setColour (Colours::black.withAlpha (0.08f));
            g.fillEllipse (c.x - hub - (float) i * 1.5f + R * 0.08f, c.y - hub - (float) i + R * 0.14f, (hub + (float) i * 1.5f) * 2, (hub + (float) i * 1.5f) * 2);
            g.fillPath (ptr, xf.translated (R * 0.08f + (float) i, R * 0.14f + (float) i));
        }
        // base redonda (cupula)
        g.setGradientFill (ColourGradient (Colour (0xff3b3c3f), c.x - hub * 0.5f, c.y - hub * 0.7f, Colour (0xff040405), c.x + hub * 0.6f, c.y + hub * 0.8f, true));
        g.fillEllipse (c.x - hub, c.y - hub, hub * 2, hub * 2);
        g.setColour (Colours::black); g.drawEllipse (c.x - hub, c.y - hub, hub * 2, hub * 2, 1.0f);
        // puntero
        const auto a = polar (c, R, ang - 0.6f), b = polar (c, R, ang + 0.9f);
        g.setGradientFill (ColourGradient (Colour (0xff4a4b4f), a.x, a.y, Colour (0xff070708), b.x, b.y, false));
        g.fillPath (ptr, xf);
        // filo brillante a lo largo
        Path edge; edge.startNewSubPath (-R * 0.12f, R * 0.30f); edge.quadraticTo (-R * 0.22f, -R * 0.2f, -R * 0.11f, -R * 0.95f);
        g.setColour (Colours::white.withAlpha (0.28f)); g.strokePath (edge, juce::PathStrokeType (R * 0.05f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), xf);
        g.setColour (Colours::black.withAlpha (0.9f)); g.strokePath (ptr, juce::PathStrokeType (1.0f), xf);
        // linea blanca indicadora
        g.setColour (Colour (0xfff4f1ea));
        g.drawLine ({ polar (c, R * 0.30f, ang), polar (c, R * 1.02f, ang) }, R * 0.07f);
        // reflejo en la cupula
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.35f), c.x - hub * 0.35f, c.y - hub * 0.6f, Colours::transparentWhite, c.x - hub * 0.1f, c.y - hub * 0.1f, true));
        g.fillEllipse (c.x - hub * 0.75f, c.y - hub * 0.85f, hub * 0.8f, hub * 0.55f);
    }

    // ------------------------------------------------------------------ perilla chica de la barra superior
    void drawMini (Graphics& g, juce::Point<float> c, float R, float ang)
    {
        g.setGradientFill (ColourGradient (Colour (0xff3c3e42), c.x - R * 0.5f, c.y - R * 0.7f, Colour (0xff0b0c0d), c.x + R * 0.5f, c.y + R * 0.8f, false));
        g.fillEllipse (c.x - R, c.y - R, R * 2, R * 2);
        g.setColour (Colours::black); g.drawEllipse (c.x - R, c.y - R, R * 2, R * 2, 1.0f);
        g.setColour (Colours::white.withAlpha (0.15f)); g.drawEllipse (c.x - R + 1.5f, c.y - R + 1.5f, R * 2 - 3, R * 2 - 3, 1.0f);
        g.setColour (Colour (0xffe9a03b)); g.drawLine ({ polar (c, R * 0.25f, ang), polar (c, R * 0.9f, ang) }, 2.4f);
    }
}

Look::Look()
{
    setColour (juce::PopupMenu::backgroundColourId, Colour (0xff1c2024));
    setColour (juce::PopupMenu::textColourId, silkCol);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, slate);
    setColour (juce::PopupMenu::headerTextColourId, Colour (0xff9fb3c0));
    setColour (juce::TooltipWindow::backgroundColourId, Colour (0xff1c2024));
    setColour (juce::TooltipWindow::textColourId, silkCol);
}
void Look::drawBubble (Graphics& g, juce::BubbleComponent&, const juce::Point<float>&, const Rectangle<float>& body)
{
    g.setColour (Colour (0xf01c2024)); g.fillRoundedRectangle (body, 4);
    g.setColour (silkCol.withAlpha (0.5f)); g.drawRoundedRectangle (body.reduced (0.5f), 4, 1.0f);
}
void Look::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const juce::Point<float> c ((float) x + (float) w * 0.5f, (float) y + (float) h * 0.5f);
    const float a = a0 + pos * (a1 - a0);
    const auto kind = (KnobKind) (int) s.getProperties()["kind"];
    if (kind == KnobKind::fluted) drawFluted (g, c, (float) std::min (w, h) * 0.5f / kF, a);
    else if (kind == KnobKind::chicken) drawChicken (g, c, (float) std::min (w, h) * 0.5f / kC, a);
    else drawMini (g, c, (float) std::min (w, h) * 0.5f - 2.0f, a);
}

Knob::Knob (APVTS& s, const juce::String& id, KnobKind k, float a0, float a1) : kind (k)
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (a0, a1, true);
    setMouseDragSensitivity (k == KnobKind::chicken ? 120 : 260);
    getProperties().set ("kind", (int) k);
    setPopupDisplayEnabled (k != KnobKind::chicken, true, nullptr, 900);
    if (auto* p = s.getParameter (id)) setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    att = std::make_unique<APVTS::SliderAttachment> (s, id, *this);
}

// ------------------------------------------------------------------ palanca IN
void Toggle::paint (Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    const juce::Point<float> c (r.getCentreX(), r.getCentreY() + 6);
    const bool on = param (proc, "power") > 0.5f;
    // tuerca hexagonal cromada
    Path hex; hex.addPolygon (c, 6, 17.0f, pi / 6.0f);
    g.setColour (Colours::black.withAlpha (0.35f)); g.fillPath (hex, juce::AffineTransform::translation (2, 3));
    g.setGradientFill (ColourGradient (Colour (0xfff3f4f6), c.x - 14, c.y - 14, Colour (0xff5d6167), c.x + 14, c.y + 14, false)); g.fillPath (hex);
    g.setColour (Colours::black.withAlpha (0.55f)); g.strokePath (hex, juce::PathStrokeType (1.0f));
    for (int i = 0; i < 6; ++i)
    {
        const float t = pi / 6.0f + (float) i * pi / 3.0f;
        g.setColour (Colours::white.withAlpha (i < 3 ? 0.25f : 0.0f)); g.drawLine ({ c, polar (c, 15.0f, t) }, 0.6f);
    }
    // casquillo roscado
    g.setGradientFill (ColourGradient (Colour (0xffd9dce0), c.x - 9, c.y - 9, Colour (0xff3f4247), c.x + 9, c.y + 9, false)); g.fillEllipse (c.x - 9.5f, c.y - 9.5f, 19, 19);
    g.setColour (Colours::black.withAlpha (0.6f)); g.drawEllipse (c.x - 9.5f, c.y - 9.5f, 19, 19, 1.0f); g.drawEllipse (c.x - 6.5f, c.y - 6.5f, 13, 13, 0.7f);
    g.setColour (Colour (0xff15171a)); g.fillEllipse (c.x - 4.5f, c.y - 4.5f, 9, 9);
    // palanca (arriba = IN)
    const float dir = on ? -1.0f : 1.0f, len = 40.0f;
    const juce::Point<float> tip (c.x + (on ? -3.0f : 3.0f), c.y + dir * len);
    Path bat;
    bat.startNewSubPath (c.x - 3.2f, c.y); bat.lineTo (tip.x - 4.6f, tip.y); bat.lineTo (tip.x + 4.6f, tip.y); bat.lineTo (c.x + 3.2f, c.y); bat.closeSubPath();
    g.setColour (Colours::black.withAlpha (0.35f)); g.fillPath (bat, juce::AffineTransform::translation (4, 5));
    g.setGradientFill (ColourGradient (Colour (0xff6a6e74), tip.x - 5, 0, Colour (0xfff7f8fa), tip.x + 1, 0, false)); g.fillPath (bat);
    g.setColour (Colours::black.withAlpha (0.45f)); g.strokePath (bat, juce::PathStrokeType (0.8f));
    g.setGradientFill (ColourGradient (Colour (0xffffffff), tip.x - 3, tip.y - 3, Colour (0xff7d8187), tip.x + 4, tip.y + 4, true));
    g.fillEllipse (tip.x - 5.5f, tip.y - 5.5f, 11, 11);
    g.setColour (Colours::black.withAlpha (0.45f)); g.drawEllipse (tip.x - 5.5f, tip.y - 5.5f, 11, 11, 0.8f);
}
void Toggle::mouseUp (const juce::MouseEvent&) { setParam (proc, "power", param (proc, "power") > 0.5f ? 0.0f : 1.0f); repaint(); }

// ------------------------------------------------------------------ lampara de joya roja
void Jewel::paint (Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    const juce::Point<float> c = r.getCentre();
    const float Rb = std::min (r.getWidth(), r.getHeight()) * 0.17f, Rj = Rb * 0.70f;
    const bool on = param (proc, "power") > 0.5f;
    const float lum = on ? 0.30f + 0.70f * glow : 0.06f;
    // resplandor sobre el panel
    if (lum > 0.1f)
    {
        g.setGradientFill (ColourGradient (Colour (0xffff2a10).withAlpha (0.55f * lum), c.x, c.y, Colour (0x00ff2a10), c.x + Rb * 2.6f, c.y, true));
        g.fillEllipse (c.x - Rb * 2.6f, c.y - Rb * 2.6f, Rb * 5.2f, Rb * 5.2f);
    }
    // bisel cromado
    g.setColour (Colours::black.withAlpha (0.35f)); g.fillEllipse (c.x - Rb + 2, c.y - Rb + 3, Rb * 2, Rb * 2);
    g.setGradientFill (ColourGradient (Colour (0xfff9fafb), c.x - Rb * 0.7f, c.y - Rb * 0.8f, Colour (0xff50545a), c.x + Rb * 0.8f, c.y + Rb * 0.9f, false));
    g.fillEllipse (c.x - Rb, c.y - Rb, Rb * 2, Rb * 2);
    g.setGradientFill (ColourGradient (Colour (0xff5a5e64), c.x - Rb * 0.7f, c.y - Rb * 0.8f, Colour (0xffeef0f2), c.x + Rb * 0.8f, c.y + Rb * 0.9f, false));
    g.fillEllipse (c.x - Rb * 0.84f, c.y - Rb * 0.84f, Rb * 1.68f, Rb * 1.68f);
    g.setColour (Colours::black.withAlpha (0.6f)); g.drawEllipse (c.x - Rb, c.y - Rb, Rb * 2, Rb * 2, 1.0f);
    // joya facetada: anillos de facetas triangulares
    const Colour base = Colour::fromFloatRGBA (0.45f + 0.55f * lum, 0.03f + 0.10f * lum * lum, 0.02f + 0.06f * lum * lum, 1.0f);
    g.setColour (base.darker (0.6f)); g.fillEllipse (c.x - Rj, c.y - Rj, Rj * 2, Rj * 2);
    const int n = 12;
    for (int ring = 0; ring < 2; ++ring)
    {
        const float r0 = ring == 0 ? Rj * 0.45f : Rj * 0.98f, r1 = ring == 0 ? 0.0f : Rj * 0.45f;
        for (int i = 0; i < n; ++i)
        {
            const float t0 = (float) i / n * twoPi + (ring ? 0.0f : pi / n), t1 = (float) (i + 1) / n * twoPi + (ring ? 0.0f : pi / n), tm = 0.5f * (t0 + t1);
            const float facing = 0.5f + 0.5f * std::cos (tm + pi * 0.25f);
            const float k = (ring ? 0.55f : 0.85f) + 0.45f * facing * (i % 2 ? 1.0f : 0.7f);
            Path f;
            if (ring == 0) { f.startNewSubPath (c); f.lineTo (polar (c, r0, t0)); f.lineTo (polar (c, r0, t1)); }
            else { f.startNewSubPath (polar (c, r1, t0 + pi / n)); f.lineTo (polar (c, r0, t0)); f.lineTo (polar (c, r0, t1)); f.lineTo (polar (c, r1, t1 + pi / n)); }
            f.closeSubPath();
            g.setColour (base.withMultipliedBrightness (k)); g.fillPath (f);
            g.setColour (Colours::black.withAlpha (0.18f)); g.strokePath (f, juce::PathStrokeType (0.6f));
        }
    }
    // reflejo
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.75f), c.x - Rj * 0.35f, c.y - Rj * 0.45f, Colours::transparentWhite, c.x - Rj * 0.05f, c.y - Rj * 0.05f, true));
    g.fillEllipse (c.x - Rj * 0.65f, c.y - Rj * 0.7f, Rj * 0.6f, Rj * 0.45f);
    if (lum > 0.3f)
    {
        g.setGradientFill (ColourGradient (Colour (0xffffd0a0).withAlpha (0.6f * lum), c.x, c.y, Colour (0x00ff6040), c.x + Rj * 0.6f, c.y, true));
        g.fillEllipse (c.x - Rj * 0.6f, c.y - Rj * 0.6f, Rj * 1.2f, Rj * 1.2f);
    }
}
void Jewel::mouseUp (const juce::MouseEvent&) { setParam (proc, "power", param (proc, "power") > 0.5f ? 0.0f : 1.0f); repaint(); }

// ------------------------------------------------------------------ contenido
Content::Content (TopEqProcessor& p)
    : proc (p),
      lowBoost (p.apvts, "lowBoost", KnobKind::fluted, A0, A1), lowAtten (p.apvts, "lowAtten", KnobKind::fluted, A0, A1),
      lowFreq (p.apvts, "lowFreq", KnobKind::chicken, selLow.a0, selLow.a1),
      highBoost (p.apvts, "highBoost", KnobKind::fluted, A0, A1), bandwidth (p.apvts, "bandwidth", KnobKind::fluted, A0, A1),
      highFreq (p.apvts, "highFreq", KnobKind::chicken, selHigh.a0, selHigh.a1),
      highAtten (p.apvts, "highAtten", KnobKind::fluted, A0, A1), attFreq (p.apvts, "attFreq", KnobKind::chicken, selAtt.a0, selAtt.a1),
      drive (p.apvts, "drive", KnobKind::mini, -pi * 0.75f, pi * 0.75f), out (p.apvts, "out", KnobKind::mini, -pi * 0.75f, pi * 0.75f),
      power (p), lamp (p)
{
    for (juce::Component* c : std::initializer_list<juce::Component*> { &lowBoost, &lowAtten, &lowFreq, &highBoost, &bandwidth, &highFreq, &highAtten, &attFreq, &drive, &out, &power, &lamp })
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
    power.setTooltip (U ("IN: enciende o apaga el ecualizador (comparar con el original)"));
    lamp.setTooltip (U ("La lámpara brilla con tu señal"));
    buildPanel();
}

void Content::resized()
{
    auto place = [] (juce::Component& c, float cx, float cy, float d) { c.setBounds (juce::roundToInt (cx - d * 0.5f), juce::roundToInt (cy - d * 0.5f), juce::roundToInt (d), juce::roundToInt (d)); };
    place (lowBoost, xLB, rowTop, knobD); place (lowAtten, xLA, rowTop, knobD); place (lowFreq, xCPS, rowBot, selD);
    place (highBoost, xHB, rowTop, knobD); place (bandwidth, xBW, yBW, knobD * 0.86f); place (highFreq, xKCS, rowBot, selD);
    place (highAtten, xHA, rowTop, knobD); place (attFreq, xSEL, rowBot, selD);
    place (drive, 1262, 27, 36); place (out, 1412, 27, 36);
    power.setBounds (140, 380, 80, 140);
    lamp.setBounds (1335, 165, 170, 170);
}

// panel pintado una sola vez: textura martillada, orejas de rack, tornillos, escalas y textos
void Content::buildPanel()
{
    panel = juce::Image (juce::Image::ARGB, W, H, true);
    Graphics g (panel);
    const float top = (float) strip;
    // barra superior (fuera del panel)
    g.setGradientFill (ColourGradient (Colour (0xff23272b), 0, 0, Colour (0xff141619), 0, top, false)); g.fillRect (0.0f, 0.0f, (float) W, top);
    g.setColour (Colours::black); g.drawHorizontalLine (strip - 1, 0, (float) W);
    g.setFont (font (15, true).withExtraKerningFactor (0.08f)); g.setColour (Colour (0xffcfd8de));
    g.drawText ("Dj Pava Hm Music", Rectangle<float> (24, 0, 300, top), juce::Justification::centredLeft);
    g.setFont (font (11)); g.setColour (Colour (0xff7f8c95));
    g.drawText (U ("TOP EQ-1A  ·  circuito pasivo + válvulas"), Rectangle<float> (24, 30, 320, 20), juce::Justification::centredLeft);
    g.setFont (font (11, true)); g.setColour (Colour (0xff9fb0bb));
    g.drawText ("DRIVE", Rectangle<float> (1170, 8, 70, 38), juce::Justification::centredRight);
    g.drawText ("OUTPUT", Rectangle<float> (1312, 8, 78, 38), juce::Justification::centredRight);

    // panel azul pizarra
    const Rectangle<float> pan (0, top, (float) W, (float) H - top);
    g.setGradientFill (ColourGradient (slate.brighter (0.10f), 0, top, slate.darker (0.22f), 0, (float) H, false)); g.fillRect (pan);
    {
        // textura "piel de naranja": ruido por pixel + manchas suaves
        juce::Image::BitmapData bd (panel, 0, strip, W, H - strip, juce::Image::BitmapData::readWrite);
        juce::Random rnd (7);
        for (int y = 0; y < bd.height; ++y)
            for (int x = 0; x < bd.width; ++x)
            {
                const auto px = bd.getPixelColour (x, y);
                const float n = (rnd.nextFloat() - 0.5f) * 0.07f + 0.025f * std::sin ((float) x * 0.21f + (float) y * 0.13f) * std::sin ((float) y * 0.17f - (float) x * 0.05f);
                bd.setPixelColour (x, y, px.withMultipliedBrightness (1.0f + n));
            }
    }
    juce::Random rnd (42);
    for (int i = 0; i < 4200; ++i)
    {
        const float x = rnd.nextFloat() * (float) W, y = top + rnd.nextFloat() * ((float) H - top), s = 1.5f + rnd.nextFloat() * 4.5f;
        g.setColour ((rnd.nextBool() ? Colours::white : Colours::black).withAlpha (0.018f + 0.022f * rnd.nextFloat()));
        g.fillEllipse (x, y, s, s * (0.7f + 0.5f * rnd.nextFloat()));
    }
    // luz rasante de arriba a la izquierda
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.07f), 0, top, Colours::transparentWhite, (float) W * 0.6f, (float) H, false)); g.fillRect (pan);
    g.setColour (Colours::white.withAlpha (0.18f)); g.drawHorizontalLine (strip, 0, (float) W);
    g.setColour (Colours::black.withAlpha (0.5f)); g.drawHorizontalLine (H - 1, 0, (float) W);

    // orejas de rack: ranuras y tornillos
    for (float x : { 30.0f, (float) W - 30.0f })
    {
        g.setColour (Colours::black.withAlpha (0.25f)); g.drawVerticalLine ((int) (x < 100 ? 62 : W - 62), top, (float) H);
        for (float y : { top + 64.0f, (float) H - 64.0f })
        {
            Rectangle<float> slot (x - 18, y - 8, 36, 16);
            g.setColour (Colour (0xff101315)); g.fillRoundedRectangle (slot, 8);
            g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.8f), 0, slot.getY(), Colours::transparentBlack, 0, slot.getBottom(), false)); g.fillRoundedRectangle (slot, 8);
            g.setColour (Colours::white.withAlpha (0.22f)); g.drawRoundedRectangle (slot.translated (0, 1), 8, 1.0f);
        }
    }
    for (float x : { 92.0f, (float) W - 92.0f }) for (float y : { top + 22.0f, (float) H - 22.0f }) screw (g, x, y);

    // logotipo propio: curva del "truco" dentro de una placa
    {
        Rectangle<float> badge (98, top + 70, 226, 70);
        Path wave;
        for (int i = 0; i <= 120; ++i)
        {
            const float t = (float) i / 120.0f;
            const float y = 0.55f * std::exp (-std::pow ((t - 0.12f) / 0.13f, 2.0f)) - 0.32f * std::exp (-std::pow ((t - 0.42f) / 0.12f, 2.0f)) + 0.45f * std::exp (-std::pow ((t - 0.83f) / 0.10f, 2.0f));
            const juce::Point<float> p (badge.getX() + t * badge.getWidth(), badge.getCentreY() - y * badge.getHeight() * 0.55f);
            if (i == 0) wave.startNewSubPath (p); else wave.lineTo (p);
        }
        for (int k = 0; k < 4; ++k)
        {
            g.setColour (silkCol.withAlpha (0.18f + 0.22f * (float) (k == 0)));
            g.strokePath (wave, juce::PathStrokeType (k == 0 ? 2.2f : 0.8f), juce::AffineTransform::translation (0, (float) k * 4.0f));
        }
        silk (g, "TOP  EQ-1A", { 98, top + 150, 226, 36 }, 30, true);
        silk (g, "PROGRAM  EQUALIZER", { 98, top + 186, 226, 20 }, 14.5f, true, juce::Justification::centred, 0.85f);
        silk (g, "TUBE  PASSIVE  NETWORK", { 98, top + 208, 226, 16 }, 10.5f, false, juce::Justification::centred, 0.6f);
        silk (g, "IN", { 205, 392, 40, 20 }, 16);
    }

    // escala 0..10 con puntos (perillas grandes)
    auto scale = [&g] (float cx, float cy, float R, const char* title, float titleDy)
    {
        for (int i = 0; i <= 20; ++i)
        {
            const float a = A0 + (A1 - A0) * (float) i / 20.0f;
            const auto p = polar ({ cx, cy }, R + 22.0f, a);
            if (i % 2 == 0) silk (g, juce::String (i / 2), { p.x - 14, p.y - 10, 28, 20 }, 17, false);
            else { g.setColour (silkCol.withAlpha (0.85f)); const auto d = polar ({ cx, cy }, R + 20.0f, a); g.fillEllipse (d.x - 2.0f, d.y - 2.0f, 4.0f, 4.0f); }
        }
        silk (g, title, { cx - 100, cy - R - 62 + titleDy, 200, 24 }, 19);
    };
    const float Rk = knobD * 0.5f / kF * 1.17f;               // radio de la falda
    scale (xLB, rowTop, Rk, "BOOST", 0); scale (xLA, rowTop, Rk, "ATTEN", 0);
    scale (xHB, rowTop, Rk, "BOOST", 0); scale (xHA, rowTop, Rk, "ATTEN", 0);
    {
        const float Rb = knobD * 0.86f * 0.5f / kF * 1.17f;
        for (int i = 0; i <= 20; ++i)
        {
            const float a = A0 + (A1 - A0) * (float) i / 20.0f;
            const auto p = polar ({ xBW, yBW }, Rb + 19.0f, a);
            if (i % 2 == 0) silk (g, juce::String (i / 2), { p.x - 12, p.y - 9, 24, 18 }, 14, false);
            else { g.setColour (silkCol.withAlpha (0.85f)); const auto d = polar ({ xBW, yBW }, Rb + 18.0f, a); g.fillEllipse (d.x - 1.7f, d.y - 1.7f, 3.4f, 3.4f); }
        }
        silk (g, "SHARP", { xBW - 122, yBW + 62, 64, 18 }, 12.5f, true); silk (g, "BROAD", { xBW + 58, yBW + 62, 64, 18 }, 12.5f, true);
        silk (g, "BANDWIDTH", { xBW - 100, yBW - Rb - 58, 200, 22 }, 16);
    }
    // selectores
    auto selScale = [&g] (float cx, float cy, const Sel& s, std::initializer_list<const char*> labels, const char* title, float r)
    {
        int i = 0;
        for (auto* l : labels)
        {
            const float a = s.a0 + (s.a1 - s.a0) * (float) i / (float) (s.n - 1);
            const auto p = polar ({ cx, cy }, r, a);
            silk (g, l, { p.x - 22, p.y - 11, 44, 22 }, 18, false);
            ++i;
        }
        silk (g, title, { cx - 80, cy - r - 34, 160, 22 }, 17);
    };
    selScale (xCPS, rowBot, selLow, { "20", "30", "60", "100" }, "CPS", 72);
    selScale (xKCS, rowBot, selHigh, { "3", "4", "5", "8", "10", "12", "16" }, "KCS", 74);
    selScale (xSEL, rowBot, selAtt, { "5", "10", "20" }, "ATTEN SEL", 70);
    // nombres de las secciones con linea
    auto section = [&g] (float x0, float x1, float y, const char* name)
    {
        const float tw = juce::GlyphArrangement::getStringWidth (font (17, true).withExtraKerningFactor (0.06f), name) + 26;
        const float cx = 0.5f * (x0 + x1);
        g.setColour (silkCol.withAlpha (0.55f));
        g.drawLine (x0, y, cx - tw * 0.5f, y, 1.2f); g.drawLine (cx + tw * 0.5f, y, x1, y, 1.2f);
        g.drawLine (x0, y, x0, y - 10, 1.2f); g.drawLine (x1, y, x1, y - 10, 1.2f);
        silk (g, name, { cx - tw * 0.5f, y - 11, tw, 22 }, 17);
    };
    section (370, 745, (float) H - 22, "LOW FREQUENCY");
    section (770, 1150, (float) H - 22, "HIGH FREQUENCY");
    section (1180, 1360, (float) H - 22, "ATTEN");
    silk (g, "Dj Pava Hm Music", { 1340, (float) H - 90, 170, 18 }, 12.5f, true, juce::Justification::centred, 0.75f);
    silk (g, U ("HAND WIRED  ·  CLASS A"), { 1340, (float) H - 70, 170, 16 }, 9.5f, false, juce::Justification::centred, 0.5f);
}

void Content::paint (Graphics& g)
{
    g.drawImageAt (panel, 0, 0);
    // barra de presets (estilo pantalla)
    for (auto [r, t] : { std::pair<Rectangle<float>, const char*> { prevArea, "<" }, { nextArea, ">" } })
    {
        g.setGradientFill (ColourGradient (Colour (0xff2e3337), 0, r.getY(), Colour (0xff1b1e21), 0, r.getBottom(), false)); g.fillRoundedRectangle (r, 4);
        g.setColour (Colours::black); g.drawRoundedRectangle (r, 4, 1.0f);
        Path tri; const auto c = r.getCentre(); const float s = t[0] == '<' ? -1.0f : 1.0f;
        tri.addTriangle (c.x - 4 * s, c.y - 6, c.x - 4 * s, c.y + 6, c.x + 5 * s, c.y);
        g.setColour (Colour (0xffcfd8de)); g.fillPath (tri);
    }
    g.setGradientFill (ColourGradient (Colour (0xff0c0e10), 0, presetArea.getY(), Colour (0xff1a1d20), 0, presetArea.getBottom(), false)); g.fillRoundedRectangle (presetArea, 4);
    g.setColour (Colours::black); g.drawRoundedRectangle (presetArea, 4, 1.0f);
    g.setColour (Colours::white.withAlpha (0.08f)); g.drawRoundedRectangle (presetArea.translated (0, 1), 4, 1.0f);
    g.setFont (font (15, true)); g.setColour (Colour (0xffe8edf0));
    g.drawText (proc.getProgramName (proc.getCurrentProgram()), presetArea, juce::Justification::centred);
    // valores de las perillas chicas
    g.setFont (font (11)); g.setColour (Colour (0xff9fb0bb));
    g.drawText (juce::String (param (proc, "drive"), 1) + " dB", Rectangle<float> (1284, 8, 56, 38), juce::Justification::centredLeft);
    g.drawText (juce::String (param (proc, "out"), 1) + " dB", Rectangle<float> (1434, 8, 60, 38), juce::Justification::centredLeft);
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
    // la joya sigue el nivel de entrada (sube rapido, baja lento como un filamento)
    const float pk = proc.inPeak.exchange (0.0f);
    const float target = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (pk, -60.0f) + 42.0f) / 42.0f);
    lamp.glow = target > lamp.glow ? lamp.glow + 0.6f * (target - lamp.glow) : lamp.glow * 0.86f + target * 0.14f;
    lamp.repaint();
    const int prog = proc.getCurrentProgram(); const float pw = param (proc, "power");
    if (prog != lastProgram || pw != lastPower) { lastProgram = prog; lastPower = pw; power.repaint(); }
    repaint (Rectangle<int> (540, 0, 1000, strip));
}

TopEqEditor::TopEqEditor (TopEqProcessor& p) : AudioProcessorEditor (p), content (p)
{
    setLookAndFeel (&look);
    addAndMakeVisible (content);
    content.setBounds (0, 0, W, H);
    setResizable (true, true);
    setResizeLimits (936, 336, 2340, 840);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) W / (double) H);
    setSize (1170, 420);
    startTimerHz (30);
}
TopEqEditor::~TopEqEditor() { setLookAndFeel (nullptr); }
void TopEqEditor::resized() { content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) W)); }
