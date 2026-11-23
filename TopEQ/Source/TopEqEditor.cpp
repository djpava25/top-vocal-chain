#include "TopEqEditor.h"
#include "Parts.h"

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
    auto* k = dynamic_cast<Knob*> (&s);
    if (k == nullptr) return;
    float ang = a0 + pos * (a1 - a0);
    if (k->kind == KnobKind::mini) { drawMini (g, { (float) x + (float) w * 0.5f, (float) y + (float) h * 0.5f }, (float) std::min (w, h) * 0.5f - 2.0f, ang); return; }
    if (k->kind != KnobKind::skirt)
    {
        const int n = (int) s.getMaximum() + 1, idx = juce::jlimit (0, n - 1, (int) std::round (s.getValue()));
        const float* tbl = n == 4 ? cpsA : n == 7 ? kcsA : selA;
        ang = tbl[idx] * deg;
    }
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (k->image (ang, w, h), Rectangle<float> ((float) x, (float) y, (float) w, (float) h));
}

const juce::Image& Knob::image (float ang, int w, int h)
{
    if (std::abs (ang - imgAng) < 1e-4f && imgW == w && img.isValid()) return img;
    const int iw = w * 2, ih = h * 2;                                     // doble resolucion (nitido al agrandar)
    const float half = (float) std::min (iw, ih) * 0.5f;
    if (kind == KnobKind::skirt)
    {
        const float R = half / kSkirt;
        img = r3d::render (iw, ih, [R, ang] (float px, float py) { return parts::skirtKnob (px, py, R, ang); }, { 0.05f * R, 0.16f * R, 0.16f * R, 0.80f });
    }
    else
    {
        const float L = half / 1.25f;
        const bool bar = kind == KnobKind::bar;
        img = r3d::render (iw, ih, [L, ang, bar] (float px, float py) { return bar ? parts::barKnob (px, py, L, ang) : parts::peakKnob (px, py, L, ang); },
                           { 0.06f * L, 0.18f * L, 0.12f * L, 0.75f });
    }
    imgAng = ang; imgW = w;
    return img;
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
    const bool on = param (proc, pid) > 0.5f;
    auto& im = cache[on ? 1 : 0];
    if (! im.isValid())
    {
        const int w = getWidth() * 2, h = getHeight() * 2; const float R = 17.0f * 2.0f;
        im = r3d::render (w, h, [R, on] (float x, float y) { return parts::toggle (x, y, R, on); }, { 0.08f * R, 0.30f * R, 0.20f * R, 0.70f });
    }
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (im, getLocalBounds().toFloat());
}
void Toggle::mouseUp (const juce::MouseEvent&) { setParam (proc, pid, param (proc, pid) > 0.5f ? 0.0f : 1.0f); repaint(); }

void PowerKnob::paint (Graphics& g)
{
    const bool on = param (proc, "power") > 0.5f;
    auto& im = cache[on ? 1 : 0];
    if (! im.isValid())
    {
        const int w = getWidth() * 2, h = getHeight() * 2; const float R = 27.0f * 2.0f, ang = (on ? 38.0f : -18.0f) * deg;
        im = r3d::render (w, h, [R, ang] (float x, float y) { return parts::powerKnob (x, y, R, ang); }, { 0.06f * R, 0.18f * R, 0.16f * R, 0.80f });
    }
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (im, getLocalBounds().toFloat());
}
void PowerKnob::mouseUp (const juce::MouseEvent&) { setParam (proc, "power", param (proc, "power") > 0.5f ? 0.0f : 1.0f); repaint(); }

void Jewel::paint (Graphics& g)
{
    const bool on = param (proc, "power") > 0.5f;
    const auto c = getLocalBounds().toFloat().getCentre();
    if (on)
    {
        g.setGradientFill (ColourGradient (Colour (0x55ff2a0c), c.x, c.y, Colour (0x00ff2a0c), c.x + 62, c.y, true));
        g.fillEllipse (c.x - 62, c.y - 62, 124, 124);
    }
    auto& im = cache[on ? 1 : 0];
    if (! im.isValid())
    {
        const int w = getWidth() * 2, h = getHeight() * 2; const float R = 30.0f * 2.0f;
        im = r3d::render (w, h, [R, on] (float x, float y) { return parts::jewel (x, y, R, on); }, { 0.05f * R, 0.14f * R, 0.14f * R, 0.70f });
    }
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (im, getLocalBounds().toFloat());
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

namespace
{
    // serigrafia (se dibuja como mascara y despues toma la textura de la pintura)
    void drawSilk (Graphics& g)
    {
        auto t = [&g] (const juce::String& txt, Rectangle<float> r, float size)
        { g.setFont (font (size).withExtraKerningFactor (0.07f)); g.drawText (txt, r, juce::Justification::centred); };
        g.setColour (Colours::white);
        t ("TOP", { 127, S + 157, 200, 26 }, 19);
        t ("PROGRAM EQUALIZER", { 87, S + 181, 280, 26 }, 19);
        t ("EQ-1A", { 127, S + 205, 200, 26 }, 19);
        t ("IN", { xIN - 30, S + 260, 60, 24 }, 18);
        auto scale = [&t] (float cx, float cy, const char* title)
        {
            for (int i = 0; i <= 10; ++i)
            {
                const auto p = polar ({ cx, cy }, Rnum, A0 + (A1 - A0) * (float) i / 10.0f);
                t (juce::String (i), { p.x - 16, p.y - 11, 32, 22 }, 17);
            }
            if (title != nullptr) t (title, { cx - 90, S + 12, 180, 26 }, 18);
        };
        scale (xLB, yTop, "BOOST"); scale (xLA, yTop, "ATTEN"); scale (xHB, yTop, "BOOST"); scale (xHA, yTop, "ATTEN");
        scale (xBW, yBot, nullptr);
        t ("SHARP", { xBW - 100, S + 377, 70, 18 }, 11.5f); t ("BROAD", { xBW + 34, S + 377, 70, 18 }, 11.5f);
        auto labels = [&t] (float cx, float cy, const float* angs, std::initializer_list<const char*> ls, float r)
        {
            int i = 0;
            for (auto* l : ls) { const auto p = polar ({ cx, cy }, r, angs[i++] * deg); t (l, { p.x - 24, p.y - 11, 48, 22 }, 17); }
        };
        labels (xCPS, yBot + 12, cpsA, { "20", "30", "60", "100" }, 82);
        labels (xKCS, yBot, kcsA, { "3", "4", "5", "8", "10", "12", "16" }, 74);
        labels (xSEL, yTop + 4, selA, { "5", "10", "20" }, 64);
        t ("CPS", { xCPS - 40, S + 218, 80, 24 }, 18);
        t ("KCS", { xKCS - 40, S + 220, 80, 24 }, 18);
        t ("ATTEN SEL", { xSEL - 80, S + 14, 160, 26 }, 18);
        t ("OFF", { xPow - 24, S + 266, 48, 22 }, 17);
        t ("ON", { xPow + 22, S + 274, 40, 22 }, 17);
        t ("LOW FREQUENCY", { xCPS - 140, S + 396, 280, 26 }, 18);
        t ("BANDWIDTH", { xBW - 120, S + 396, 240, 26 }, 18);
        t ("HIGH FREQUENCY", { xKCS - 140, S + 396, 280, 26 }, 18);
    }

    // placa pintada: se calcula una sola vez por proceso (tarda unos cientos de ms)
    juce::Image renderPlate()
    {
        static juce::Image cached;
        static juce::SpinLock lock;
        const juce::SpinLock::ScopedLockType sl (lock);
        if (cached.isValid()) return cached;
        constexpr float k = 1.5f;                                          // resolucion de la placa
        const int iw = (int) (Content::W * k), ih = (int) ((float) (Content::H - (int) S) * k);
        juce::Image mask (juce::Image::SingleChannel, iw, ih, true);
        {
            Graphics mg (mask);
            mg.addTransform (juce::AffineTransform::translation (0, -S).scaled (k));
            drawSilk (mg);
        }
        juce::Image::BitmapData mb (mask, juce::Image::BitmapData::readOnly);
        struct Scr { float x, y, slot; };
        const Scr screws[] { { 121, S + 83, 0.62f }, { 120, S + 345, 0.30f }, { 1434, S + 87, 0.95f }, { 1434, S + 350, 0.40f } };
        const float slotsY[] { S + 121, S + 306 };
        const auto paintMat = r3d::srgb (46.0f / 255.0f, 69.0f / 255.0f, 86.0f / 255.0f, 0, 0.55f);
        const auto ink = r3d::srgb (0.93f, 0.94f, 0.93f, 0, 0.42f);
        const float cxI = (float) iw * 0.5f, cyI = (float) ih * 0.5f;
        auto fn = [&] (float rx, float ry)
        {
            const float px = rx + cxI, py = ry + cyI;                       // pixel de la placa
            const float X = px / k, Y = py / k + S;                         // coordenadas del panel
            r3d::Sample s; s.m = paintMat;
            // piel de naranja de la pintura + leve ondulacion de la chapa
            float h = 0.16f * r3d::fbm (px * 0.16f, py * 0.16f, 1, 3) + 0.05f * r3d::fbm (px * 0.7f, py * 0.7f, 2, 2) + 4.0f * r3d::fbm (px * 0.003f, py * 0.003f, 3, 2);
            const float tone = 0.94f + 0.12f * r3d::fbm (px * 0.01f, py * 0.01f, 4, 3);   // pintura no perfectamente pareja
            s.m.r *= tone; s.m.g *= tone; s.m.b *= tone;
            // serigrafia con desgaste
            const int mx = juce::jlimit (0, iw - 1, (int) px), my = juce::jlimit (0, ih - 1, (int) py);
            float a = mb.getPixelColour (mx, my).getFloatAlpha();
            if (a > 0)
            {
                a *= 0.86f + 0.14f * r3d::fbm (px * 0.45f, py * 0.45f, 5, 2);
                s.m = parts::mixMat (s.m, ink, a); h += 0.7f * a;
            }
            // borde de la chapa arriba y abajo
            const float dEdge = std::min (py, (float) ih - 1 - py);
            h -= 3.0f * (1.0f - r3d::fillet (dEdge, 4.0f));
            // ranuras abiertas de las orejas
            for (float sy : slotsY)
                for (bool left : { true, false })
                {
                    const float x0 = left ? -20.0f : (float) Content::W - 48.0f;
                    const float qx = std::max ({ x0 + 11 - X, X - (x0 + 68 - 11), 0.0f }), qy = std::max (std::abs (Y - sy), 0.0f);
                    const float d = (std::sqrt (qx * qx + qy * qy) - 11.0f) * k;   // distancia al borde de la ranura
                    if (d < 0.5f) s.m.alpha = r3d::clamp01 (d + 0.5f);
                    if (d < 5.0f) h -= 3.0f * (1.0f - r3d::fillet (std::max (d, 0.0f), 5.0f));
                }
            // tornillos
            for (auto& sc : screws)
            {
                const float dx = (X - sc.x) * k, dy = (Y - sc.y) * k, Rs = 13.0f * k, rs = std::sqrt (dx * dx + dy * dy);
                if (rs < Rs + 1)
                {
                    const auto ss = parts::screw (dx, dy, Rs, sc.slot);
                    s.m = parts::mixMat (s.m, ss.m, ss.m.alpha); s.m.alpha = 1;
                    h = h * (1 - ss.m.alpha) + (4.0f + ss.h) * ss.m.alpha;
                }
                else
                {
                    const float ao = 1.0f - 0.55f * std::exp (-std::pow ((rs - Rs) / (0.30f * Rs), 2.0f));
                    s.m.r *= ao; s.m.g *= ao; s.m.b *= ao;
                }
            }
            s.h = h; return s;
        };
        const std::function<r3d::V3 (float, float)> view = [iw, ih] (float x, float y) { return r3d::norm ({ -x, -0.15f * (float) ih - y, 1.1f * (float) iw }); };
        cached = r3d::render (iw, ih, fn, {}, &view);
        return cached;
    }
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
    g.setColour (holeCol); g.fillRect (0.0f, S, (float) W, (float) H - S);
    plate = renderPlate();
}

void Content::paint (Graphics& g)
{
    g.drawImageAt (panel, 0, 0);
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (plate, Rectangle<float> (0, S, (float) W, (float) H - S));
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
