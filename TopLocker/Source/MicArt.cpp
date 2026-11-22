// Dibujos de los microfonos de Top Locker (estilo foto: rejilla de malla cuadrada, niquel mate,
// suspension elastica, sombras y reflejos). Diseno propio: sin logos ni marcas.
#include "TopLockerEditor.h"

using juce::Colour; namespace Colours = juce::Colours; using juce::Graphics; using juce::Path; using juce::Rectangle; using juce::ColourGradient;

namespace
{
struct Mat { Colour base; float gloss, grain; };
const Mat nickel { Colour (0xffb9b8b1), 0.45f, 0.6f }, nickelWarm { Colour (0xffc2b9a2), 0.45f, 0.6f }, chrome { Colour (0xffc9ccd2), 1.0f, 0.2f },
          blackMatte { Colour (0xff2b2b2e), 0.35f, 0.5f }, blackSatin { Colour (0xff1f2023), 0.6f, 0.3f }, darkSteel { Colour (0xff55575c), 0.6f, 0.3f },
          beige { Colour (0xffb7b1a2), 0.4f, 0.5f };

// sombreado de cilindro: luz desde arriba-izquierda, reflejo de borde a la derecha
void shade (Graphics& g, const Path& p, const Mat& m, bool detail)
{
    const auto r = p.getBounds();
    const Colour b = m.base;
    ColourGradient gr (b.darker (1.6f), r.getX(), 0, b.darker (0.6f), r.getRight(), 0, false);
    gr.addColour (0.06, b.darker (0.7f));
    gr.addColour (0.20, b.brighter (0.25f + 0.55f * m.gloss));
    gr.addColour (0.30, b.brighter (0.1f + 0.2f * m.gloss));
    gr.addColour (0.50, b);
    gr.addColour (0.74, b.darker (0.55f));
    gr.addColour (0.90, b.darker (1.1f));
    gr.addColour (0.965, b.brighter (0.05f));     // reflejo del borde
    g.setGradientFill (gr); g.fillPath (p);
    g.saveState(); g.reduceClipRegion (p);
    if (detail && m.grain > 0)
    {
        juce::Random rnd ((juce::int64) (r.getX() * 13 + r.getY() * 7));
        const int n = (int) (r.getWidth() * r.getHeight() * 0.25f * m.grain);
        for (int i = 0; i < n; ++i)
        {
            g.setColour ((rnd.nextBool() ? Colours::white : Colours::black).withAlpha (0.035f + 0.035f * rnd.nextFloat()));
            g.fillRect (r.getX() + rnd.nextFloat() * r.getWidth(), r.getY() + rnd.nextFloat() * r.getHeight(), 0.8f, 0.8f + 1.5f * rnd.nextFloat());
        }
    }
    // oclusion arriba/abajo
    ColourGradient ao (Colours::black.withAlpha (0.35f), 0, r.getY(), Colours::transparentBlack, 0, r.getY() + std::min (8.0f, r.getHeight() * 0.25f), false);
    g.setGradientFill (ao); g.fillRect (r);
    ColourGradient ao2 (Colours::transparentBlack, 0, r.getBottom() - std::min (10.0f, r.getHeight() * 0.3f), Colours::black.withAlpha (0.3f), 0, r.getBottom(), false);
    g.setGradientFill (ao2); g.fillRect (r);
    g.restoreState();
    g.setColour (Colours::black.withAlpha (0.55f)); g.strokePath (p, juce::PathStrokeType (0.7f));
}

// cilindro con perspectiva (vista un poco desde arriba): bordes inferior y superior curvos
Path cyl (float cx, float y0, float y1, float w0, float w1)
{
    const float e0 = w0 * 0.11f, e1 = w1 * 0.11f;
    Path p; p.startNewSubPath (cx - w0 * 0.5f, y0); p.lineTo (cx - w1 * 0.5f, y1);
    p.quadraticTo (cx, y1 + 2 * e1, cx + w1 * 0.5f, y1); p.lineTo (cx + w0 * 0.5f, y0);
    p.quadraticTo (cx, y0 + 2 * e0, cx - w0 * 0.5f, y0); p.closeSubPath(); return p;
}
void cylinder (Graphics& g, float cx, float y0, float y1, float w0, float w1, const Mat& m, bool detail, bool cap = false)
{
    shade (g, cyl (cx, y0, y1, w0, w1), m, detail);
    if (cap)
    {
        const float e = w0 * 0.11f;
        g.setGradientFill (ColourGradient (m.base.brighter (0.5f), cx - w0 * 0.3f, y0 - e, m.base.darker (0.4f), cx + w0 * 0.4f, y0 + e, false));
        g.fillEllipse (cx - w0 * 0.5f, y0 - e, w0, 2 * e);
        g.setColour (Colours::black.withAlpha (0.5f)); g.drawEllipse (cx - w0 * 0.5f, y0 - e, w0, 2 * e, 0.6f);
    }
}
// ranura entre piezas (sigue la curva del cilindro)
void groove (Graphics& g, float cx, float y, float w, float th = 1.4f)
{
    const float e = w * 0.11f;
    Path p; p.startNewSubPath (cx - w * 0.5f, y); p.quadraticTo (cx, y + 2 * e, cx + w * 0.5f, y);
    g.setColour (Colours::black.withAlpha (0.75f)); g.strokePath (p, juce::PathStrokeType (th));
    g.setColour (Colours::white.withAlpha (0.25f)); g.strokePath (p, juce::PathStrokeType (0.6f), juce::AffineTransform::translation (0, th));
}

// rejilla de malla cuadrada tejida, con segunda capa interna, sombra de la capsula y brillo cilindrico
void weave (Graphics& g, const Path& p, Colour wire, float u, bool detail, float capsule = 0.55f)
{
    const auto r = p.getBounds();
    g.saveState(); g.reduceClipRegion (p);
    g.setColour (wire.darker (2.2f)); g.fillRect (r);
    if (detail)
    {
        const float cell = std::max (1.35f, 1.9f * u);
        // capa interna (mas oscura y desplazada: da profundidad y moire)
        g.setColour (wire.darker (1.0f).withAlpha (0.55f));
        for (float x = r.getX() + cell * 0.5f; x < r.getRight(); x += cell * 1.07f) g.fillRect (x, r.getY(), cell * 0.32f, r.getHeight());
        for (float y = r.getY() + cell * 0.5f; y < r.getBottom(); y += cell * 1.07f) g.fillRect (r.getX(), y, r.getWidth(), cell * 0.32f);
        // sombra de la capsula a traves de la malla
        g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.55f), r.getCentreX(), r.getCentreY(), Colours::transparentBlack, r.getCentreX() + r.getWidth() * capsule, r.getCentreY(), true));
        g.fillEllipse (r.getCentreX() - r.getWidth() * capsule, r.getCentreY() - r.getHeight() * 0.42f, r.getWidth() * capsule * 2, r.getHeight() * 0.84f);
        // malla exterior tejida
        const float t = cell * 0.42f;
        int row = 0;
        for (float y = r.getY(); y < r.getBottom(); y += cell, ++row)
        {
            g.setColour (wire.withAlpha (0.92f)); g.fillRect (r.getX(), y, r.getWidth(), t);
            int col = 0;
            for (float x = r.getX(); x < r.getRight(); x += cell, ++col)
                if ((row + col) % 2 == 0) { g.setColour (wire.brighter (0.5f).withAlpha (0.55f)); g.fillRect (x, y, cell * 0.5f, t * 0.55f); }
        }
        g.setColour (wire.darker (0.25f).withAlpha (0.9f));
        for (float x = r.getX(); x < r.getRight(); x += cell) g.fillRect (x, r.getY(), t, r.getHeight());
    }
    else
    {
        g.setColour (wire.withAlpha (0.6f)); g.fillRect (r);
    }
    // forma cilindrica + luz superior
    ColourGradient sh (Colours::black.withAlpha (0.85f), r.getX(), 0, Colours::black.withAlpha (0.8f), r.getRight(), 0, false);
    sh.addColour (0.08, Colours::black.withAlpha (0.35f)); sh.addColour (0.24, Colours::white.withAlpha (0.28f)); sh.addColour (0.36, Colours::transparentWhite);
    sh.addColour (0.68, Colours::black.withAlpha (0.25f)); sh.addColour (0.93, Colours::black.withAlpha (0.55f)); sh.addColour (0.97, Colours::white.withAlpha (0.12f));
    g.setGradientFill (sh); g.fillRect (r);
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.2f), 0, r.getY(), Colours::transparentWhite, 0, r.getY() + r.getHeight() * 0.3f, false)); g.fillRect (r);
    g.restoreState();
    g.setColour (wire.darker (1.0f)); g.strokePath (p, juce::PathStrokeType (1.1f * u));
    g.setColour (Colours::white.withAlpha (0.18f)); g.strokePath (p, juce::PathStrokeType (0.5f * u), juce::AffineTransform::translation (0.6f * u, 0.6f * u));
}
// cabeza: rectangulo con esquinas superiores redondeadas y borde inferior curvo
Path headPath (float cx, float y0, float y1, float w, float rad)
{
    const float e = w * 0.11f;
    Path p; p.startNewSubPath (cx - w * 0.5f, y1); p.lineTo (cx - w * 0.5f, y0 + rad);
    p.quadraticTo (cx - w * 0.5f, y0, cx - w * 0.5f + rad, y0); p.lineTo (cx + w * 0.5f - rad, y0);
    p.quadraticTo (cx + w * 0.5f, y0, cx + w * 0.5f, y0 + rad); p.lineTo (cx + w * 0.5f, y1);
    p.quadraticTo (cx, y1 + 2 * e, cx - w * 0.5f, y1); p.closeSubPath(); return p;
}
// tapa superior visible de una cabeza plana
void headTop (Graphics& g, float cx, float y0, float w, float rad, Colour wire)
{
    const float e = w * 0.07f;
    Path t; t.addRoundedRectangle (cx - w * 0.5f + rad * 0.6f, y0 - e, w - rad * 1.2f, 2 * e, e);
    g.setGradientFill (ColourGradient (wire.darker (0.2f), cx - w * 0.3f, y0 - e, wire.darker (1.4f), cx + w * 0.4f, y0 + e, false));
    g.fillPath (t);
    g.setColour (Colours::white.withAlpha (0.25f)); g.drawLine (cx - w * 0.3f, y0 - e * 0.6f, cx + w * 0.1f, y0 - e * 0.6f, 0.6f);
    g.setColour (Colours::black.withAlpha (0.45f)); g.strokePath (t, juce::PathStrokeType (0.6f));
}
void tube (Graphics& g, float x0, float y0, float x1, float y1, float th, Colour c)
{
    g.setColour (Colours::black.withAlpha (0.5f)); g.drawLine (x0 + 0.6f, y0 + 1.0f, x1 + 0.6f, y1 + 1.0f, th + 0.6f);
    g.setColour (c); g.drawLine (x0, y0, x1, y1, th);
    g.setColour (c.brighter (0.8f).withAlpha (0.7f)); g.drawLine (x0 - th * 0.2f, y0, x1 - th * 0.2f, y1, th * 0.3f);
}
void ringArc (Graphics& g, float cx, float cy, float rx, float ry, bool front, float th, Colour c)
{
    Path a; const float pi = juce::MathConstants<float>::pi;
    if (front) a.addCentredArc (cx, cy, rx, ry, 0, pi * 0.5f, pi * 1.5f, true);
    else a.addCentredArc (cx, cy, rx, ry, 0, -pi * 0.5f, pi * 0.5f, true);
    g.setColour (Colours::black.withAlpha (front ? 0.5f : 0.3f)); g.strokePath (a, juce::PathStrokeType (th + 0.8f), juce::AffineTransform::translation (0, 1));
    g.setColour (front ? c : c.darker (0.8f)); g.strokePath (a, juce::PathStrokeType (th));
    if (front) { g.setColour (c.brighter (0.8f).withAlpha (0.6f)); g.strokePath (a, juce::PathStrokeType (th * 0.3f), juce::AffineTransform::translation (0, -th * 0.25f)); }
}

// suspension tipo arana (aros metalicos, varillas y elasticos cruzados)
void spider (Graphics& g, float cx, float yT, float yB, float yFoot, float R, float bodyW, float u, bool front, Colour metal, Colour cord)
{
    const float eT = R * 0.22f;
    if (! front)
    {
        ringArc (g, cx, yT, R, eT, false, 2.4f * u, metal);
        ringArc (g, cx, yB, R * 0.92f, eT * 0.9f, false, 2.4f * u, metal);
        // varillas traseras
        tube (g, cx - R * 0.55f, yT - eT * 0.8f, cx - R * 0.45f, yB - eT * 0.7f, 1.3f * u, metal.darker (0.7f));
        tube (g, cx + R * 0.55f, yT - eT * 0.8f, cx + R * 0.45f, yB - eT * 0.7f, 1.3f * u, metal.darker (0.7f));
        return;
    }
    // varillas: de cada lado una X entre aros y luego al pie
    for (float s : { -1.0f, 1.0f })
    {
        tube (g, cx + s * R, yT, cx + s * R * 0.92f, yB, 2.4f * u, metal);
        tube (g, cx + s * R * 0.92f, yB, cx + s * 4 * u, yFoot, 2.4f * u, metal);
        tube (g, cx + s * R * 0.62f, yT + eT * 0.8f, cx + s * R * 0.55f, yB + eT * 0.8f, 2.0f * u, metal.darker (0.15f));
        tube (g, cx + s * R * 0.55f, yB + eT * 0.8f, cx + s * 2 * u, yFoot, 2.0f * u, metal.darker (0.15f));
    }
    ringArc (g, cx, yT, R, eT, true, 2.6f * u, metal);
    ringArc (g, cx, yB, R * 0.92f, eT * 0.9f, true, 2.6f * u, metal);
    // elasticos cruzados por delante del cuerpo
    const float ym = (yT + yB) * 0.5f;
    for (float s : { -1.0f, 1.0f })
    {
        g.setColour (Colours::black.withAlpha (0.35f)); g.drawLine (cx + s * R, yT + eT + 1, cx - s * bodyW * 0.5f, ym + 1, 1.0f * u);
        g.setColour (cord); g.drawLine (cx + s * R, yT + eT, cx - s * bodyW * 0.5f, ym, 0.8f * u);
        g.setColour (cord.withAlpha (0.85f)); g.drawLine (cx + s * R * 0.92f, yB + eT * 0.9f, cx - s * bodyW * 0.45f, ym + (yB - yT) * 0.25f, 0.7f * u);
    }
    // pie roscado
    cylinder (g, cx, yFoot - 4 * u, yFoot + 6 * u, 9 * u, 8 * u, darkSteel, false, true);
}

// soporte de pie: pinza simple con tubo
void standClip (Graphics& g, float cx, float y, float yb, float u, bool detail)
{
    cylinder (g, cx, y, yb, 7 * u, 7 * u, blackSatin, detail);
    cylinder (g, cx, y - 3 * u, y + 9 * u, 16 * u, 14 * u, blackMatte, detail, true);
}
void xlr (Graphics& g, float cx, float y, float yb, float u, bool detail)
{
    cylinder (g, cx, y, y + 14 * u, 10 * u, 9 * u, chrome, detail);
    for (int i = 0; i < 4; ++i) groove (g, cx, y + (3 + i * 2.5f) * u, 10 * u, 0.6f);
    Path cable; cable.startNewSubPath (cx, y + 14 * u); cable.cubicTo (cx, y + 30 * u, cx - 6 * u, yb - 10 * u, cx - 5 * u, yb + 4 * u);
    g.setColour (Colour (0xff6c6e72)); g.strokePath (cable, juce::PathStrokeType (4.2f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (Colours::white.withAlpha (0.2f)); g.strokePath (cable, juce::PathStrokeType (1.2f * u), juce::AffineTransform::translation (-1.0f * u, 0));
}
void emblem (Graphics& g, float cx, float cy, float r, Colour c)
{
    g.setColour (Colours::black.withAlpha (0.5f)); g.fillEllipse (cx - r, cy - r + 0.6f, r * 2, r * 2);
    g.setGradientFill (ColourGradient (c.brighter (0.5f), cx - r, cy - r, c.darker (0.7f), cx + r, cy + r, false)); g.fillEllipse (cx - r, cy - r, r * 2, r * 2);
    g.setColour (Colours::white.withAlpha (0.65f)); g.drawEllipse (cx - r * 0.55f, cy - r * 0.55f, r * 1.1f, r * 1.1f, 0.6f);
}
// ventanita del patron (azul)
void patternWindow (Graphics& g, float cx, float cy, float u)
{
    Rectangle<float> w (cx - 3.2f * u, cy - 2.6f * u, 6.4f * u, 5.2f * u);
    g.setColour (Colour (0xff0d1020)); g.fillRoundedRectangle (w, 1.0f * u);
    g.setColour (Colour (0xff3b4fd8)); g.fillRoundedRectangle (w.reduced (0.9f * u), 0.8f * u);
    g.setColour (Colours::white.withAlpha (0.85f)); g.drawEllipse (w.getCentreX() - 1.3f * u, w.getCentreY() - 1.2f * u, 2.6f * u, 2.4f * u, 0.5f * u);
}
} // namespace

void tlui::drawMic (Graphics& g, Rectangle<float> r, int idx, bool detail)
{
    const float u = std::min (r.getWidth() / 100.0f, r.getHeight() / 230.0f);
    const float cx = r.getCentreX(), yb = r.getBottom();
    // sombra de contacto + sombra proyectada hacia atras
    g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.7f), cx + 6 * u, yb, Colours::transparentBlack, cx + 50 * u, yb, true));
    g.fillEllipse (cx - 44 * u, yb - 7 * u, 100 * u, 14 * u);
    g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.85f), cx, yb, Colours::transparentBlack, cx + 18 * u, yb, true));
    g.fillEllipse (cx - 18 * u, yb - 3 * u, 36 * u, 6 * u);

    const Colour silverWire (0xffcfd0cc), darkWire (0xff3a3b3e), goldWire (0xffd9b04f), cord (0xffdcdcd6);
    switch (idx)
    {
        case 0:   // Studio Silver: niquel mate, cabeza plana, arana elastica
        {
            const float hT = yb - 214 * u, hB = yb - 146 * u, bT = yb - 140 * u, bB = yb - 58 * u, R = 34 * u;
            spider (g, cx, yb - 118 * u, yb - 54 * u, yb - 12 * u, R, 40 * u, u, false, Colour (0xff8f8e88), cord);
            cylinder (g, cx, bB, yb - 44 * u, 40 * u, 28 * u, nickel, detail);                 // cono inferior
            cylinder (g, cx, yb - 44 * u, yb - 38 * u, 28 * u, 26 * u, darkSteel, detail);
            cylinder (g, cx, bT, bB, 40 * u, 40 * u, nickel, detail);
            cylinder (g, cx, hB, bT, 44 * u, 42 * u, nickel, detail);                         // cuello
            groove (g, cx, bT - 1.5f * u, 42 * u, 1.6f);
            patternWindow (g, cx, hB + 5 * u, u);
            weave (g, headPath (cx, hT, hB, 44 * u, 9 * u), silverWire, u, detail);
            headTop (g, cx, hT, 44 * u, 9 * u, silverWire);
            emblem (g, cx, bB - 14 * u, 3.2f * u, Colour (0xff3a4a8a));
            spider (g, cx, yb - 118 * u, yb - 54 * u, yb - 12 * u, R, 40 * u, u, true, Colour (0xff8f8e88), cord);
            break;
        }
        case 1:   // Modern Silver: corto, cabeza redonda de malla oscura
        {
            standClip (g, cx, yb - 66 * u, yb, u, detail);
            cylinder (g, cx, yb - 76 * u, yb - 66 * u, 26 * u, 22 * u, darkSteel, detail);
            cylinder (g, cx, yb - 150 * u, yb - 76 * u, 40 * u, 36 * u, nickel, detail);
            groove (g, cx, yb - 148 * u, 40 * u);
            Path h = headPath (cx, yb - 206 * u, yb - 150 * u, 44 * u, 21 * u);
            weave (g, h, Colour (0xff8d8f93), u, detail, 0.5f);
            cylinder (g, cx, yb - 153 * u, yb - 149 * u, 44 * u, 44 * u, nickel, detail);
            emblem (g, cx, yb - 132 * u, 2.6f * u, Colour (0xff2b2d33));
            break;
        }
        case 2:   // Gold Detail: rejilla dorada plana, cuerpo negro
        {
            standClip (g, cx, yb - 62 * u, yb, u, detail);
            cylinder (g, cx, yb - 150 * u, yb - 62 * u, 40 * u, 34 * u, blackSatin, detail);
            groove (g, cx, yb - 124 * u, 40 * u, 0.8f);
            // simbolos de patron + LED
            for (int i = 0; i < 5; ++i) { g.setColour (Colours::white.withAlpha (0.55f)); g.drawEllipse (cx - 13 * u + i * 6.0f * u, yb - 139 * u, 3.2f * u, 3.2f * u, 0.5f * u); }
            g.setColour (Colour (0xff6fd0ff)); g.fillEllipse (cx - 1.5f * u, yb - 132 * u, 3 * u, 3 * u);
            Path h = headPath (cx, yb - 226 * u, yb - 152 * u, 46 * u, 6 * u);
            weave (g, h, goldWire, u, detail, 0.5f);
            headTop (g, cx, yb - 226 * u, 46 * u, 6 * u, goldWire);
            g.setColour (Colour (0xff111113)); g.strokePath (h, juce::PathStrokeType (2.4f * u));
            cylinder (g, cx, yb - 154 * u, yb - 149 * u, 46 * u, 44 * u, blackSatin, detail);
            break;
        }
        case 3:   // Crown Tube: negro, disipador lateral, aro de goma
        {
            // pie y disipador detras
            cylinder (g, cx + 26 * u, yb - 118 * u, yb, 8 * u, 8 * u, blackSatin, detail);
            Rectangle<float> hs (cx + 16 * u, yb - 150 * u, 30 * u, 48 * u);
            // soporte + caja del disipador con aletas
            g.setColour (Colour (0xff141416)); g.fillRect (cx + 14 * u, yb - 134 * u, 6 * u, 16 * u);
            g.setGradientFill (ColourGradient (Colour (0xff34353a), hs.getX(), 0, Colour (0xff0b0b0c), hs.getRight(), 0, false)); g.fillRect (hs);
            Path side; side.startNewSubPath (hs.getRight(), hs.getY()); side.lineTo (hs.getRight() + 5 * u, hs.getY() - 4 * u); side.lineTo (hs.getRight() + 5 * u, hs.getBottom() - 4 * u); side.lineTo (hs.getRight(), hs.getBottom()); side.closeSubPath();
            g.setColour (Colour (0xff070708)); g.fillPath (side);
            Path topF; topF.startNewSubPath (hs.getX(), hs.getY()); topF.lineTo (hs.getX() + 5 * u, hs.getY() - 4 * u); topF.lineTo (hs.getRight() + 5 * u, hs.getY() - 4 * u); topF.lineTo (hs.getRight(), hs.getY()); topF.closeSubPath();
            g.setColour (Colour (0xff3e3f44)); g.fillPath (topF);
            for (int i = 0; i < 9; ++i)
            {
                const float x = hs.getX() + 3 * u + (float) i * 3.1f * u;
                g.setColour (Colours::white.withAlpha (0.2f)); g.drawLine (x, hs.getY() + 2 * u, x, hs.getBottom() - 2 * u, 0.8f * u);
                g.setColour (Colours::black.withAlpha (0.7f)); g.drawLine (x + 1.3f * u, hs.getY() + 2 * u, x + 1.3f * u, hs.getBottom() - 2 * u, 0.9f * u);
            }
            g.setColour (Colours::black); g.drawRect (hs, 0.8f);
            xlr (g, cx, yb - 62 * u, yb, u, detail);
            cylinder (g, cx, yb - 150 * u, yb - 62 * u, 42 * u, 40 * u, blackMatte, detail);
            for (float y : { 140.0f, 118.0f, 100.0f, 80.0f }) groove (g, cx, yb - y * u, 42 * u, 1.0f);
            g.setColour (Colours::white.withAlpha (0.5f)); g.drawEllipse (cx - 9 * u, yb - 112 * u, 2.4f * u, 2.4f * u, 0.5f * u);
            g.fillRoundedRectangle (cx - 4 * u, yb - 111.6f * u, 8 * u, 1.8f * u, 0.8f * u); g.drawEllipse (cx + 6.5f * u, yb - 112 * u, 2.4f * u, 2.4f * u, 0.5f * u);
            // aro de goma con elasticos
            const float ry = yb - 88 * u;
            ringArc (g, cx, ry, 30 * u, 7 * u, false, 5.5f * u, Colour (0xff232326));
            for (float s : { -1.0f, 1.0f }) for (float dy : { -14.0f, 12.0f })
            { g.setColour (Colour (0xff121214)); g.drawLine (cx + s * 28 * u, ry + dy * 0.3f * u, cx + s * 19 * u, ry + dy * u, 2.0f * u); }
            ringArc (g, cx, ry, 30 * u, 7 * u, true, 5.5f * u, Colour (0xff2a2a2d));
            Path h = headPath (cx, yb - 226 * u, yb - 152 * u, 48 * u, 10 * u);
            weave (g, h, darkWire, u, detail, 0.45f);
            headTop (g, cx, yb - 226 * u, 48 * u, 10 * u, darkWire);
            cylinder (g, cx, yb - 154 * u, yb - 148 * u, 48 * u, 44 * u, blackMatte, detail);
            break;
        }
        case 4:   // Classic Tube: plateado delgado, rejilla cilindrica con dos aros
        {
            cylinder (g, cx, yb - 42 * u, yb, 8 * u, 8 * u, darkSteel, detail);
            cylinder (g, cx, yb - 50 * u, yb - 40 * u, 20 * u, 18 * u, chrome, detail);
            for (int i = 0; i < 4; ++i) groove (g, cx, yb - (48 - i * 2.4f) * u, 20 * u, 0.5f);
            cylinder (g, cx, yb - 146 * u, yb - 50 * u, 38 * u, 34 * u, chrome, detail);
            emblem (g, cx, yb - 120 * u, 3.6f * u, Colour (0xff27324f));
            Path h = headPath (cx, yb - 224 * u, yb - 148 * u, 42 * u, 6 * u);
            weave (g, h, silverWire, u, detail);
            headTop (g, cx, yb - 224 * u, 42 * u, 6 * u, silverWire);
            for (float y : { 199.0f, 174.0f }) cylinder (g, cx, yb - y * u, yb - (y - 3) * u, 43 * u, 43 * u, chrome, detail);
            cylinder (g, cx, yb - 150 * u, yb - 145 * u, 42 * u, 40 * u, chrome, detail);
            break;
        }
        case 5:   // Black Reference: cuerpo negro, cabeza plateada redondeada, arana negra
        {
            spider (g, cx, yb - 120 * u, yb - 56 * u, yb - 12 * u, 34 * u, 42 * u, u, false, Colour (0xff3a3a3d), Colour (0xff1a1a1a));
            cylinder (g, cx, yb - 56 * u, yb - 44 * u, 42 * u, 34 * u, chrome, detail);
            cylinder (g, cx, yb - 146 * u, yb - 56 * u, 42 * u, 42 * u, blackSatin, detail);
            Rectangle<float> plate (cx - 11 * u, yb - 120 * u, 22 * u, 7 * u);
            g.setColour (Colour (0xff0b0b0c)); g.fillRoundedRectangle (plate, 1.2f * u);
            g.setColour (Colour (0xffc9ccd2)); g.drawRoundedRectangle (plate, 1.2f * u, 0.5f * u);
            g.setFont (juce::Font (juce::FontOptions (5.2f * u, juce::Font::bold))); g.drawText ("REF", plate, juce::Justification::centred);
            Path h = headPath (cx, yb - 216 * u, yb - 148 * u, 44 * u, 20 * u);
            weave (g, h, silverWire, u, detail);
            cylinder (g, cx, yb - 150 * u, yb - 145 * u, 44 * u, 42 * u, chrome, detail);
            spider (g, cx, yb - 120 * u, yb - 56 * u, yb - 12 * u, 34 * u, 42 * u, u, true, Colour (0xff3a3a3d), Colour (0xff1a1a1a));
            break;
        }
        case 6:   // Broadcast Black: dinamico en horquilla con espuma
        {
            const float pivX = cx + 6 * u, pivY = yb - 112 * u;
            cylinder (g, pivX, pivY + 20 * u, yb, 8 * u, 8 * u, blackSatin, detail);
            g.saveState();
            g.addTransform (juce::AffineTransform::rotation (-0.26f, pivX, pivY));
            cylinder (g, pivX, pivY - 34 * u, pivY + 44 * u, 36 * u, 34 * u, blackSatin, detail);
            for (int i = 0; i < 4; ++i) groove (g, pivX, pivY + (14 + i * 7.0f) * u, 35 * u, 0.6f);
            Path foam; foam.addRoundedRectangle (pivX - 24 * u, pivY - 112 * u, 48 * u, 82 * u, 22 * u);
            g.setGradientFill (ColourGradient (Colour (0xff3d3e42), pivX - 24 * u, 0, Colour (0xff070708), pivX + 24 * u, 0, false)); g.fillPath (foam);
            if (detail)
            {
                juce::Random rnd (7);
                g.saveState(); g.reduceClipRegion (foam);
                for (int i = 0; i < 900; ++i)
                {
                    g.setColour ((rnd.nextBool() ? Colours::white : Colours::black).withAlpha (0.08f * rnd.nextFloat()));
                    g.fillEllipse (pivX - 24 * u + rnd.nextFloat() * 48 * u, pivY - 112 * u + rnd.nextFloat() * 82 * u, 1.3f * u, 1.3f * u);
                }
                g.restoreState();
            }
            g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.16f), pivX - 12 * u, pivY - 104 * u, Colours::transparentWhite, pivX + 6 * u, pivY - 64 * u, true)); g.fillPath (foam);
            g.setColour (Colours::black); g.strokePath (foam, juce::PathStrokeType (0.8f * u));
            g.restoreState();
            Path yoke; yoke.startNewSubPath (pivX - 25 * u, pivY - 16 * u); yoke.lineTo (pivX - 25 * u, pivY + 12 * u); yoke.quadraticTo (pivX - 25 * u, pivY + 26 * u, pivX, pivY + 26 * u);
            yoke.quadraticTo (pivX + 25 * u, pivY + 26 * u, pivX + 25 * u, pivY + 12 * u); yoke.lineTo (pivX + 25 * u, pivY - 16 * u);
            g.setColour (Colour (0xff0e0e10)); g.strokePath (yoke, juce::PathStrokeType (5.0f * u));
            g.setColour (Colours::white.withAlpha (0.2f)); g.strokePath (yoke, juce::PathStrokeType (1.0f * u), juce::AffineTransform::translation (-1.0f * u, -0.5f * u));
            for (float s : { -1.0f, 1.0f })
            {
                g.setGradientFill (ColourGradient (Colour (0xffa0a3a8), pivX + s * 25 * u - 4 * u, pivY - 8 * u, Colour (0xff3a3c40), pivX + s * 25 * u + 4 * u, pivY + 2 * u, false));
                g.fillEllipse (pivX + s * 25 * u - 5 * u, pivY - 8 * u, 10 * u, 10 * u);
            }
            break;
        }
        case 7:   // Ribbon Modern: cinta delgada cromada con ventana
        {
            xlr (g, cx, yb - 30 * u, yb, u, detail);
            Path b; b.addRoundedRectangle (cx - 14 * u, yb - 196 * u, 28 * u, 168 * u, 13 * u);
            shade (g, b, chrome, detail);
            Path win; win.addRoundedRectangle (cx - 8.5f * u, yb - 186 * u, 17 * u, 98 * u, 8 * u);
            weave (g, win, silverWire, u, detail, 0.3f);
            g.setColour (Colour (0xff6b5a3a).withAlpha (0.8f)); g.fillRect (cx - 1.4f * u, yb - 180 * u, 2.8f * u, 86 * u);
            for (int i = 0; i < 3; ++i) groove (g, cx, yb - (78 - i * 5.0f) * u, 28 * u, 0.7f);
            break;
        }
        case 8:   // Ribbon Classic: cabeza cuadrada redondeada sobre horquilla
        {
            cylinder (g, cx, yb - 80 * u, yb, 8 * u, 8 * u, darkSteel, detail);
            cylinder (g, cx, yb - 128 * u, yb - 76 * u, 24 * u, 22 * u, nickel, detail, false);
            Path h; h.addRoundedRectangle (cx - 31 * u, yb - 200 * u, 62 * u, 70 * u, 22 * u);
            weave (g, h, Colour (0xff8a8c90), u, detail, 0.4f);
            g.setColour (Colour (0xffc3c6cc)); g.strokePath (h, juce::PathStrokeType (3.6f * u));
            g.setColour (Colours::white.withAlpha (0.55f)); g.strokePath (h, juce::PathStrokeType (1.0f * u), juce::AffineTransform::translation (-0.7f * u, -0.7f * u));
            for (float s : { -1.0f, 1.0f }) emblem (g, cx + s * 33 * u, yb - 165 * u, 3.5f * u, Colour (0xff8d9096));
            break;
        }
        case 9:   // Air Tube: cuerpo vintage que se angosta hacia abajo
        {
            // montura giratoria con tuerca moleteada
            cylinder (g, cx, yb - 26 * u, yb, 16 * u, 16 * u, nickel, detail, false);
            for (int i = 0; i < 9; ++i) { g.setColour (Colours::black.withAlpha (0.35f)); g.drawVerticalLine ((int) (cx - 7 * u + i * 1.75f * u), yb - 24 * u, yb - 2 * u); }
            g.setColour (Colour (0xffa9aaa6)); g.fillRoundedRectangle (cx - 22 * u, yb - 40 * u, 14 * u, 6 * u, 2 * u);
            Path taper; taper.startNewSubPath (cx - 20 * u, yb - 124 * u); taper.lineTo (cx + 20 * u, yb - 124 * u); taper.lineTo (cx + 11 * u, yb - 28 * u); taper.lineTo (cx - 11 * u, yb - 28 * u); taper.closeSubPath();
            shade (g, taper, nickelWarm, detail);
            groove (g, cx, yb - 92 * u, 32 * u, 1.0f);
            Rectangle<float> box (cx - 21 * u, yb - 152 * u, 42 * u, 28 * u);
            Path bx; bx.addRoundedRectangle (box, 2 * u); shade (g, bx, nickelWarm, detail);
            for (int i = 0; i < 4; ++i) { g.setColour (Colours::black.withAlpha (0.5f)); g.drawEllipse (cx - 9 * u + i * 5.0f * u, yb - 146 * u, 2.6f * u, 2.6f * u, 0.5f * u); }
            emblem (g, cx, yb - 133 * u, 3.4f * u, Colour (0xff8a8a86));
            Path h = headPath (cx, yb - 222 * u, yb - 154 * u, 40 * u, 4 * u);
            weave (g, h, silverWire, u, detail);
            headTop (g, cx, yb - 222 * u, 40 * u, 4 * u, silverWire);
            break;
        }
        case 10:  // Velvet Tube: cabeza grande en cupula con aros, horquilla
        {
            Path yk; yk.startNewSubPath (cx - 29 * u, yb - 110 * u); yk.lineTo (cx - 29 * u, yb - 40 * u); yk.quadraticTo (cx - 29 * u, yb - 28 * u, cx, yb - 28 * u);
            yk.quadraticTo (cx + 29 * u, yb - 28 * u, cx + 29 * u, yb - 40 * u); yk.lineTo (cx + 29 * u, yb - 110 * u);
            g.setColour (Colour (0xff5d5c57)); g.strokePath (yk, juce::PathStrokeType (4 * u));
            g.setColour (Colours::white.withAlpha (0.3f)); g.strokePath (yk, juce::PathStrokeType (1.2f * u), juce::AffineTransform::translation (-1 * u, 0));
            cylinder (g, cx, yb - 28 * u, yb, 8 * u, 8 * u, darkSteel, detail);
            cylinder (g, cx, yb - 132 * u, yb - 46 * u, 48 * u, 42 * u, nickelWarm, detail);
            for (float s : { -1.0f, 1.0f }) emblem (g, cx + s * 26 * u, yb - 100 * u, 4 * u, Colour (0xff7a7870));
            Path h = headPath (cx, yb - 222 * u, yb - 134 * u, 56 * u, 28 * u);
            weave (g, h, Colour (0xffc9c5b8), u, detail);
            for (float y : { 194.0f, 170.0f, 148.0f }) cylinder (g, cx, yb - y * u, yb - (y - 3) * u, 57 * u, 57 * u, nickelWarm, detail);
            cylinder (g, cx, yb - 136 * u, yb - 131 * u, 56 * u, 50 * u, nickelWarm, detail);
            break;
        }
        case 11:  // Voice Line: cilindro largo beige con frente oscuro
        {
            cylinder (g, cx + 22 * u, yb - 100 * u, yb, 7 * u, 7 * u, darkSteel, detail);
            Rectangle<float> clamp (cx + 12 * u, yb - 108 * u, 18 * u, 10 * u);
            g.setGradientFill (ColourGradient (Colour (0xff6c6e72), clamp.getX(), clamp.getY(), Colour (0xff2c2d30), clamp.getX(), clamp.getBottom(), false)); g.fillRoundedRectangle (clamp, 2 * u);
            cylinder (g, cx, yb - 206 * u, yb - 34 * u, 34 * u, 34 * u, beige, detail, false);
            g.setColour (Colour (0xff2a2a2c)); g.fillEllipse (cx - 17 * u, yb - 206 * u - 3.7f * u, 34 * u, 7.4f * u);
            Path gr = cyl (cx, yb - 206 * u, yb - 140 * u, 34 * u, 34 * u);
            g.setColour (Colour (0xff1a1a1c)); g.fillPath (gr);
            if (detail) for (int i = 0; i < 24; ++i) groove (g, cx, yb - (204 - i * 2.6f) * u, 33 * u, 0.5f);
            g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.2f), cx - 12 * u, 0, Colours::transparentWhite, cx + 2 * u, 0, false)); g.fillPath (gr);
            groove (g, cx, yb - 140 * u, 34 * u, 1.2f);
            cylinder (g, cx, yb - 36 * u, yb - 26 * u, 22 * u, 20 * u, darkSteel, detail);
            break;
        }
        default: break;
    }
}

void tlui::drawRoom (Graphics& g, Rectangle<float> r, int room, float size)
{
    g.saveState(); g.reduceClipRegion (r.toNearestInt());
    struct RS { Colour wall, floor, ceil; float depth; int style; };
    static const RS rs[] { { Colour (0xff0a0a0a), Colour (0xff0a0a0a), Colour (0xff0a0a0a), 0.5f, 0 },
                           { Colour (0xff3a3a3c), Colour (0xff2b221b), Colour (0xff202022), 0.62f, 1 },
                           { Colour (0xff5a3a22), Colour (0xff3a2a1d), Colour (0xff231a12), 0.5f, 2 },
                           { Colour (0xff6e5544), Colour (0xff4a3a2c), Colour (0xff2b2622), 0.38f, 3 },
                           { Colour (0xff3a3e48), Colour (0xff3a2c22), Colour (0xff1a1c22), 0.26f, 4 },
                           { Colour (0xff4a3020), Colour (0xff2e2016), Colour (0xff1e140e), 0.55f, 5 },
                           { Colour (0xff3e4a52), Colour (0xff2c2620), Colour (0xff1c2024), 0.55f, 6 },
                           { Colour (0xff1c1d20), Colour (0xff141416), Colour (0xff101012), 0.5f, 7 },
                           { Colour (0xff6c6a64), Colour (0xff4a4844), Colour (0xff2c2b28), 0.42f, 8 } };
    const auto& s = rs[juce::jlimit (0, 8, room)];
    g.setColour (s.ceil); g.fillRect (r);
    if (s.style == 0)
    {
        g.setColour (Colour (0xffa9a9ae)); g.setFont (juce::Font (juce::FontOptions (16, juce::Font::bold))); g.drawText ("SIN SALA (SECO)", r, juce::Justification::centred);
        g.restoreState(); return;
    }
    if (s.style == 7)   // placa de reverberacion: lamina metalica en su marco
    {
        auto f = r.reduced (r.getWidth() * 0.12f, r.getHeight() * 0.16f);
        g.setColour (Colour (0xff2a1f14)); g.fillRect (f.expanded (6));
        g.setGradientFill (ColourGradient (Colour (0xffd8dade), f.getX(), f.getY(), Colour (0xff6e7076), f.getRight(), f.getBottom(), false)); g.fillRect (f);
        juce::Random rnd (5);
        for (int i = 0; i < 160; ++i) { g.setColour (Colours::white.withAlpha (0.08f * rnd.nextFloat())); g.drawHorizontalLine ((int) (f.getY() + rnd.nextFloat() * f.getHeight()), f.getX(), f.getRight()); }
        for (int i = 0; i < 8; ++i)
        {
            const float x = f.getX() + f.getWidth() * (float) i / 7.0f;
            g.setColour (Colour (0xffc9ccd2)); g.drawLine (x, f.getY(), x, f.getY() - 6, 1.0f); g.drawLine (x, f.getBottom(), x, f.getBottom() + 6, 1.0f);
        }
        g.setColour (Colour (0xff1a1a1c)); g.fillEllipse (f.getCentreX() - 6, f.getCentreY() - 6, 12, 12);
        g.setColour (Colour (0xffd04a2a)); g.fillEllipse (f.getRight() - 30, f.getBottom() - 22, 8, 8);
        g.restoreState(); return;
    }
    const float d = juce::jlimit (0.15f, 0.8f, s.depth * std::pow (0.7f, (size - 50.0f) / 50.0f));
    Rectangle<float> back (r.getCentreX() - r.getWidth() * d * 0.5f, r.getY() + r.getHeight() * (0.5f - d * 0.45f), r.getWidth() * d, r.getHeight() * d * 0.9f);
    Path fl; fl.startNewSubPath (r.getX(), r.getBottom()); fl.lineTo (back.getX(), back.getBottom()); fl.lineTo (back.getRight(), back.getBottom()); fl.lineTo (r.getRight(), r.getBottom()); fl.closeSubPath();
    g.setGradientFill (ColourGradient (s.floor.brighter (0.4f), 0, back.getBottom(), s.floor.darker (0.4f), 0, r.getBottom(), false)); g.fillPath (fl);
    for (int i = -6; i <= 6; ++i) { g.setColour (Colours::black.withAlpha (0.25f)); g.drawLine (back.getCentreX() + (float) i * back.getWidth() / 12.0f, back.getBottom(), r.getCentreX() + (float) i * r.getWidth() / 12.0f, r.getBottom(), 0.7f); }
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
    g.setGradientFill (ColourGradient (s.wall.brighter (0.25f), back.getCentreX(), back.getY(), s.wall.darker (0.3f), back.getCentreX(), back.getBottom(), false));
    g.fillRect (back);
    if (s.style == 1 || s.style == 6)
        for (float x = back.getX(); x < back.getRight(); x += 6) for (float y = back.getY(); y < back.getBottom(); y += 6)
        { g.setColour (Colours::black.withAlpha (((int) (x + y) / 6) % 2 ? 0.25f : 0.05f)); g.fillRect (x, y, 6.0f, 6.0f); }
    if (s.style == 2 || s.style == 5)
        for (float x = back.getX(); x < back.getRight(); x += 5) { g.setColour (Colours::black.withAlpha (0.3f)); g.drawVerticalLine ((int) x, back.getY(), back.getBottom()); }
    if (s.style == 3)
        for (float y = back.getY(); y < back.getBottom(); y += 5)
        {
            g.setColour (Colours::black.withAlpha (0.3f)); g.drawHorizontalLine ((int) y, back.getX(), back.getRight());
            for (float x = back.getX() + (((int) ((y - back.getY()) / 5)) % 2 ? 5.0f : 0.0f); x < back.getRight(); x += 10) g.drawVerticalLine ((int) x, y, y + 5);
        }
    if (s.style == 4)
        for (int i = 0; i < 5; ++i) { g.setColour (Colour (0xff8fb4d8).withAlpha (0.25f)); g.fillRect (back.getX() + back.getWidth() * (0.08f + (float) i * 0.19f), back.getY() + back.getHeight() * 0.15f, back.getWidth() * 0.1f, back.getHeight() * 0.55f); }
    if (s.style == 8)   // camara de eco: azulejos y bocina
    {
        for (float y = back.getY(); y < back.getBottom(); y += 7) { g.setColour (Colours::black.withAlpha (0.2f)); g.drawHorizontalLine ((int) y, back.getX(), back.getRight()); }
        for (float x = back.getX(); x < back.getRight(); x += 7) { g.setColour (Colours::black.withAlpha (0.2f)); g.drawVerticalLine ((int) x, back.getY(), back.getBottom()); }
        Rectangle<float> spk (back.getX() + 6, back.getBottom() - back.getHeight() * 0.55f, back.getWidth() * 0.18f, back.getHeight() * 0.5f);
        g.setColour (Colour (0xff1a1a1a)); g.fillRect (spk);
        g.setColour (Colour (0xff444444)); g.fillEllipse (spk.withSizeKeepingCentre (spk.getWidth() * 0.7f, spk.getWidth() * 0.7f));
    }
    for (int i = 0; i < 3; ++i)
    {
        const float lx = back.getX() + back.getWidth() * (0.2f + 0.3f * (float) i);
        g.setGradientFill (ColourGradient (Colour (0xffffc070).withAlpha (0.45f), lx, r.getY() + 2, Colours::transparentWhite, lx, back.getBottom(), true));
        g.fillEllipse (lx - back.getWidth() * 0.2f, r.getY() - 10, back.getWidth() * 0.4f, (back.getBottom() - r.getY()) * 1.1f);
    }
    if (s.style == 5)
    {
        g.setColour (Colour (0xffffd28a)); g.fillEllipse (back.getRight() - 14, back.getBottom() - 30, 8, 8);
        g.setGradientFill (ColourGradient (Colour (0x66ffb050), back.getRight() - 10, back.getBottom() - 26, Colours::transparentWhite, back.getRight() + 20, back.getBottom(), true));
        g.fillRect (back.getRight() - 50, back.getBottom() - 60, 80.0f, 70.0f);
    }
    const float mx = back.getCentreX(), my = back.getBottom() + 8;
    g.setColour (Colour (0xff101010)); g.drawLine (mx, my, mx, my - back.getHeight() * 0.55f, 1.6f);
    g.drawLine (mx - 10, my + 3, mx + 10, my + 3, 1.6f);
    g.setColour (Colour (0xffc9ccd2)); g.fillRoundedRectangle (mx - 3.5f, my - back.getHeight() * 0.55f - 14, 7, 16, 3);
    g.setGradientFill (ColourGradient (Colours::transparentBlack, r.getCentreX(), r.getCentreY(), Colours::black.withAlpha (0.65f), r.getX(), r.getY(), true));
    g.fillRect (r);
    g.restoreState();
}
