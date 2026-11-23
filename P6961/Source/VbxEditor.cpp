#include "VbxEditor.h"

using namespace vbui;
using juce::Colour; namespace Colours = juce::Colours; using juce::Graphics; using juce::Path; using juce::Rectangle; using juce::ColourGradient;

namespace
{
    juce::String U (const char* s) { return juce::String::fromUTF8 (s); }
    juce::Font font (float h, bool bold = false) { return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : 0)); }
    constexpr float pi = juce::MathConstants<float>::pi;
    const Colour ink (0xffeceef4), plateTop (0xff222327), plateBot (0xff121316);
    float param (VbxProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); }
    void setParam (VbxProcessor& p, const char* id, float v)
    { if (auto* q = p.apvts.getParameter (id)) { q->beginChangeGesture(); q->setValueNotifyingHost (q->convertTo0to1 (v)); q->endChangeGesture(); } }
    juce::Point<float> polar (float cx, float cy, float r, float a) { return { cx + r * std::sin (a), cy - r * std::cos (a) }; }
    // angulos
    constexpr float K0 = pi * 1.25f, K1 = pi * 2.75f;                                    // perillas continuas
    constexpr float SEL5_0 = 2 * pi - pi / 3, SEL5_1 = 2 * pi + pi / 3;                  // selector de 5 (arriba)
    constexpr float F11_0 = pi * 1.25f, F11_1 = pi * 2.75f;                              // selectores de 11 frecuencias
    constexpr float AT0 = pi / 6, AT1 = pi * 5 / 6;                                       // ATTACK: etiquetas a la derecha
    constexpr float RE0 = 2 * pi - pi / 6, RE1 = 2 * pi - pi * 5 / 6;                     // RELEASE: etiquetas a la izquierda
    constexpr float GA0 = pi * 4 / 3, GA1 = pi * 5 / 3;                                   // GAIN 40..60 a la izquierda
    void text (Graphics& g, const juce::String& t, float cx, float cy, float size = 13.0f, juce::Justification j = juce::Justification::centred, float w = 120)
    {
        g.setFont (font (size)); g.setColour (ink);
        const float x = j == juce::Justification::centredLeft ? cx : j == juce::Justification::centredRight ? cx - w : cx - w * 0.5f;
        g.drawText (t, Rectangle<float> (x, cy - size * 0.6f, w, size * 1.2f), j);
    }
    void hexScrew (Graphics& g, float x, float y, float r = 6)
    {
        g.setGradientFill (ColourGradient (Colour (0xffd8dadf), x - r, y - r, Colour (0xff60636b), x + r, y + r, false)); g.fillEllipse (x - r, y - r, r * 2, r * 2);
        g.setColour (Colours::black.withAlpha (0.55f)); g.drawEllipse (x - r, y - r, r * 2, r * 2, 0.8f);
        Path h; for (int i = 0; i < 6; ++i) { const auto p = polar (x, y, r * 0.45f, (float) i * pi / 3); if (i == 0) h.startNewSubPath (p); else h.lineTo (p); } h.closeSubPath();
        g.setColour (Colour (0xff2a2b30)); g.fillPath (h);
    }
    void brushed (Graphics& g, const Path& p, Colour top, Colour bot, int seed, float alpha)
    {
        const auto b = p.getBounds();
        g.setGradientFill (ColourGradient (top, b.getX(), b.getY(), bot, b.getX(), b.getBottom(), false)); g.fillPath (p);
        g.saveState(); g.reduceClipRegion (p);
        juce::Random rnd (seed);
        for (float y = b.getY(); y < b.getBottom(); y += 1.2f)
        { g.setColour ((rnd.nextBool() ? Colours::white : Colours::black).withAlpha (alpha * rnd.nextFloat())); g.drawHorizontalLine ((int) y, b.getX(), b.getRight()); }
        g.restoreState();
    }
    // panel redondeado a la derecha (como los de DE ESS y EQ)
    Path pill (float x, float y, float w, float h)
    {
        const float R = h * 0.5f, cx = x + w - R;
        Path p; p.startNewSubPath (x + 8, y); p.lineTo (cx, y);
        p.addCentredArc (cx, y + R, R, R, 0, 0, pi);
        p.lineTo (x + 8, y + h); p.quadraticTo (x, y + h, x, y + h - 8); p.lineTo (x, y + 8); p.quadraticTo (x, y, x + 8, y); p.closeSubPath();
        return p;
    }
    void scaleArc (Graphics& g, float cx, float cy, float r, float a0, float a1, int ticks)
    {
        Path arc; arc.addCentredArc (cx, cy, r, r, 0, a0, a1, true);
        g.setColour (ink.withAlpha (0.9f)); g.strokePath (arc, juce::PathStrokeType (1.2f));
        for (int i = 0; i < ticks; ++i)
        {
            const float a = a0 + (a1 - a0) * (float) i / (float) (ticks - 1);
            g.drawLine (juce::Line<float> (polar (cx, cy, r, a), polar (cx, cy, r + 8, a)), 1.2f);
        }
    }
    // etiqueta con linea guia hacia la perilla
    void leader (Graphics& g, float cx, float cy, float r0, float a, juce::Point<float> labelEdge)
    {
        const auto p0 = polar (cx, cy, r0, a);
        Path l; l.startNewSubPath (labelEdge); l.lineTo (p0);
        g.setColour (ink.withAlpha (0.85f)); g.strokePath (l, juce::PathStrokeType (1.0f));
    }
}

Look::Look()
{
    setColour (juce::PopupMenu::backgroundColourId, Colour (0xff1a1b20));
    setColour (juce::PopupMenu::textColourId, ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colour (0xff4b5280));
    setColour (juce::TooltipWindow::backgroundColourId, Colour (0xff1a1b20));
    setColour (juce::TooltipWindow::textColourId, ink);
}
void Look::drawBubble (Graphics& g, juce::BubbleComponent&, const juce::Point<float>&, const Rectangle<float>& body)
{
    g.setColour (Colour (0xf01a1b20)); g.fillRoundedRectangle (body, 4);
    g.setColour (ink.withAlpha (0.5f)); g.drawRoundedRectangle (body.reduced (0.5f), 4, 1.0f);
}
// perilla negra con barra (estilo de la foto) vista de frente
void Look::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&)
{
    const juce::Point<float> c ((float) x + (float) w * 0.5f, (float) y + (float) h * 0.5f);
    const float R = (float) std::min (w, h) * 0.5f - 2.0f;
    const float a = a0 + pos * (a1 - a0);
    // sombra
    g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.7f), c.x + R * 0.2f, c.y + R * 0.35f, Colours::transparentBlack, c.x + R * 1.3f, c.y + R * 0.35f, true));
    g.fillEllipse (c.x - R * 1.0f, c.y - R * 0.8f, R * 2.5f, R * 2.5f);
    // base redonda
    const float rb = R * 0.82f;
    g.setGradientFill (ColourGradient (Colour (0xff3c3d42), c.x - rb * 0.6f, c.y - rb * 0.8f, Colour (0xff050506), c.x + rb * 0.7f, c.y + rb, false));
    g.fillEllipse (c.x - rb, c.y - rb, rb * 2, rb * 2);
    g.setColour (Colours::black); g.drawEllipse (c.x - rb, c.y - rb, rb * 2, rb * 2, 1.0f);
    // barra a lo largo del indicador
    Path bar; const float bw = R * 0.52f;
    bar.addRoundedRectangle (c.x - bw * 0.5f, c.y - R, bw, R * 2.0f, bw * 0.35f);
    bar.applyTransform (juce::AffineTransform::rotation (a, c.x, c.y));
    g.setColour (Colours::black.withAlpha (0.55f)); g.fillPath (bar, juce::AffineTransform::translation (2.5f, 3.5f));
    const auto e1 = polar (c.x, c.y, R, a + pi * 0.5f), e2 = polar (c.x, c.y, R, a - pi * 0.5f);
    ColourGradient bg (Colour (0xff1c1d21), e2.x, e2.y, Colour (0xff1c1d21), e1.x, e1.y, false);
    bg.addColour (0.3, Colour (0xff5a5c63)); bg.addColour (0.42, Colour (0xff2a2b30)); bg.addColour (0.75, Colour (0xff0b0b0d));
    g.setGradientFill (bg); g.fillPath (bar);
    g.setColour (Colours::black); g.strokePath (bar, juce::PathStrokeType (0.9f));
    // brillo superior de la barra
    g.setColour (Colours::white.withAlpha (0.18f));
    g.drawLine (juce::Line<float> (polar (c.x - 0.0f, c.y, R * 0.92f, a + 0.12f), polar (c.x, c.y, R * 0.92f, a + pi - 0.12f)), 1.2f);
    // linea blanca en la punta
    g.setColour (Colour (0xfff2f2f0)); g.drawLine (juce::Line<float> (polar (c.x, c.y, R * 0.45f, a), polar (c.x, c.y, R * 0.95f, a)), 2.6f);
}

Knob::Knob (APVTS& s, const juce::String& id, float a0, float a1, bool popup)
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (a0, a1, true);
    const bool stepped = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (id)) != nullptr || dynamic_cast<juce::AudioParameterBool*> (s.getParameter (id)) != nullptr;
    setMouseDragSensitivity (stepped ? 120 : 220);
    setPopupDisplayEnabled (popup, true, nullptr, 900);
    if (auto* p = s.getParameter (id)) setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    att = std::make_unique<APVTS::SliderAttachment> (s, id, *this);
}

void Toggle::paint (Graphics& g)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    // tuerca hexagonal
    Path hx; for (int i = 0; i < 6; ++i) { const auto p = polar (c.x, c.y, 9.0f, (float) i * pi / 3 + pi / 6); if (i == 0) hx.startNewSubPath (p); else hx.lineTo (p); } hx.closeSubPath();
    g.setGradientFill (ColourGradient (Colour (0xffe4e6ea), c.x - 8, c.y - 8, Colour (0xff595c63), c.x + 8, c.y + 8, false)); g.fillPath (hx);
    g.setColour (Colours::black.withAlpha (0.5f)); g.strokePath (hx, juce::PathStrokeType (0.8f));
    g.setColour (Colour (0xff15161a)); g.fillEllipse (c.x - 4.5f, c.y - 4.5f, 9, 9);
    // palanca
    int pos = decorative ? 0 : (int) std::round (param (proc, id));
    if (inv) pos = (n - 1) - pos;
    const float dir = n == 3 ? (pos == 2 ? -1.0f : pos == 1 ? 0.0f : 1.0f) : (pos == 1 ? -1.0f : 1.0f);   // -1 = arriba
    const float len = 16.0f;
    if (std::abs (dir) < 0.01f)
    {
        g.setGradientFill (ColourGradient (Colour (0xfff7f8fa), c.x - 4, c.y - 4, Colour (0xff8a8d93), c.x + 4, c.y + 4, true)); g.fillEllipse (c.x - 5, c.y - 5, 10, 10);
    }
    else
    {
        Path bat; bat.startNewSubPath (c.x - 2.2f, c.y); bat.lineTo (c.x - 3.2f, c.y + dir * len); bat.lineTo (c.x + 3.2f, c.y + dir * len); bat.lineTo (c.x + 2.2f, c.y); bat.closeSubPath();
        g.setColour (Colours::black.withAlpha (0.4f)); g.fillPath (bat, juce::AffineTransform::translation (2, 2));
        g.setGradientFill (ColourGradient (Colour (0xfff6f7f9), c.x - 3, 0, Colour (0xff70737a), c.x + 3, 0, false)); g.fillPath (bat);
        g.setGradientFill (ColourGradient (Colour (0xffffffff), c.x - 3, c.y + dir * len - 3, Colour (0xff8a8d93), c.x + 3, c.y + dir * len + 3, false));
        g.fillEllipse (c.x - 4.2f, c.y + dir * len - 4.2f, 8.4f, 8.4f);
    }
}
void Toggle::mouseUp (const juce::MouseEvent& e)
{
    if (decorative) return;
    const int cur = (int) std::round (param (proc, id));
    int next;
    if (n == 3) { const bool up = e.position.y < (float) getHeight() * 0.5f; next = juce::jlimit (0, 2, cur + ((up != inv) ? 1 : -1)); }
    else next = 1 - cur;
    setParam (proc, id, (float) next);
    if (auto* p = getParentComponent()) p->repaint();
}

Content::Content (VbxProcessor& p)
    : proc (p),
      input (p.apvts, "input", K0, K1), gain (p.apvts, "gain", GA0, GA1, false), meterSel (p.apvts, "meter", SEL5_0, SEL5_1, false),
      thr (p.apvts, "thr", K0, K1), attack (p.apvts, "attack", AT0, AT1, false), release (p.apvts, "release", RE0, RE1, false),
      dsFreq (p.apvts, "dsF", SEL5_0, SEL5_1, false), dsThr (p.apvts, "dsThr", K0, K1),
      low (p.apvts, "low", K0, K1), lowF (p.apvts, "lowF", F11_0, F11_1, false), mid (p.apvts, "mid", K0, K1), midF (p.apvts, "midF", F11_0, F11_1, false),
      hi (p.apvts, "hi", K0, K1), hiF (p.apvts, "hiF", F11_0, F11_1, false),
      power (p.apvts, "power", 2 * pi - 0.35f, 2 * pi + 0.35f, false), out (p.apvts, "out", K0, K1),
      phantom (p, "power", 2), hpf (p, "hpf", 3), phase (p, "phase", 2, true), link (p, "link", 2), comp (p, "comp", 2), deess (p, "deess", 2), eq (p, "eq", 2), xfmr (p, "xfmr", 2)
{
    phantom.decorative = true;
    for (juce::Component* c : std::initializer_list<juce::Component*> { &input, &gain, &meterSel, &thr, &attack, &release, &dsFreq, &dsThr, &low, &lowF, &mid, &midF, &hi, &hiF, &power, &out,
                                    &phantom, &hpf, &phase, &link, &comp, &deess, &eq, &xfmr })
        addAndMakeVisible (c);
    input.setTooltip (U ("INPUT: nivel antes del compresor (-20 a +20 dB)"));
    gain.setTooltip (U ("GAIN: realimentación del preamp a válvula, 40-60 dB. 40 = limpio, 50 = normal, 60 = caliente y al frente. Sube 5 dB por paso, como el original (baja OUTPUT para compensar)"));
    meterSel.setTooltip (U ("Qué muestra el VU: G-R (compresión), LINE IN, PRE OUT, EQ OUT o D-S (de-esser)"));
    thr.setTooltip (U ("THRESHOLD del compresor óptico 3:1 (a la derecha = más compresión)"));
    attack.setTooltip (U ("ATTACK: 4, 8, 15, 30 o 70 ms")); release.setTooltip (U ("RELEASE: 0.3, 0.5, 1, 2 o 5 s (óptico, en dos etapas)"));
    dsFreq.setTooltip (U ("Frecuencia del de-esser (3K, 6K, 9K, 12K) o LIMIT 10:1")); dsThr.setTooltip (U ("THRESHOLD del de-esser / limitador"));
    low.setTooltip (U ("LOW PEAK: 0 a +10 dB")); mid.setTooltip (U ("MID DIP: 0 a -10 dB")); hi.setTooltip (U ("HI PEAK: 0 a +10 dB"));
    lowF.setTooltip (U ("Frecuencia LOW PEAK: 20 Hz a 1 kHz")); midF.setTooltip (U ("Frecuencia MID DIP: 200 Hz a 7 kHz")); hiF.setTooltip (U ("Frecuencia HI PEAK: 1.5 a 20 kHz"));
    power.setTooltip (U ("POWER: encender / apagar")); out.setTooltip (U ("OUTPUT: volumen de salida (-20 a +10 dB)"));
    hpf.setTooltip (U ("Filtro de graves 6 dB/oct: 120, 80 o FLAT (arriba/abajo)")); phase.setTooltip (U ("Fase 0° / 180°"));
    link.setTooltip (U ("LINK: compresor y de-esser enlazados en estéreo / SEPARATE: cada canal por separado"));
    comp.setTooltip (U ("COMPRESS 3:1 / BYPASS")); deess.setTooltip (U ("DE ESS / BYPASS")); eq.setTooltip (U ("EQ IN / BYPASS")); xfmr.setTooltip (U ("XFMR IN: transformador de salida (calor en graves) / BYPASS"));
    phantom.setTooltip (U ("48V: solo decorativo (en el plugin no hace falta)"));
}

void Content::resized()
{
    auto place = [] (juce::Component& c, float cx, float cy, float r) { c.setBounds (juce::roundToInt (cx - r), juce::roundToInt (cy - r), juce::roundToInt (r * 2), juce::roundToInt (r * 2)); };
    auto tog = [] (juce::Component& c, float cx, float cy) { c.setBounds (juce::roundToInt (cx - 12), juce::roundToInt (cy - 22), 24, 44); };
    tog (phantom, 118, 66); tog (hpf, 168, 150); tog (phase, 192, 238);
    place (input, 108, 350, 36); place (gain, 318, 352, 32);
    place (meterSel, 415, 352, 17);
    tog (link, 752, 66); tog (comp, 662, 206);
    place (thr, 752, 208, 36);
    place (attack, 520, 362, 30); place (release, 752, 362, 30);
    tog (deess, 904, 104);
    place (dsFreq, 1002, 112, 26); place (dsThr, 1108, 112, 30);
    place (power, 1252, 96, 24); place (out, 1252, 172, 20);
    tog (eq, 904, 262); tog (xfmr, 904, 362);
    place (low, 1005, 268, 28); place (mid, 1150, 268, 28); place (hi, 1295, 268, 28);
    place (lowF, 1005, 372, 18); place (midF, 1150, 372, 18); place (hiF, 1295, 372, 18);
}

void Content::paint (Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    // panel de aluminio cepillado azul-violeta
    Path face; face.addRectangle (r);
    brushed (g, face, Colour (0xff6d749e), Colour (0xff3d4263), 3, 0.06f);
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.12f), r.getCentreX() * 0.55f, 0, Colours::transparentWhite, r.getCentreX() * 0.55f, r.getHeight() * 0.6f, true));
    g.fillRect (r.withWidth (840));
    // orejas del rack y tornillos
    for (float x : { 24.0f, r.getWidth() - 24.0f })
    {
        for (int i = 0; i < 6; ++i) hexScrew (g, x, 62.0f + (float) i * 62.0f, 8);
        for (float y : { 10.0f, r.getHeight() - 10.0f }) { g.setColour (Colour (0xfff4f4f6)); g.fillRoundedRectangle (x - 12, y - 6, 24, 12, 6); }
    }
    // placas negras triangulares
    auto plate = [&g] (std::initializer_list<juce::Point<float>> pts, int seed)
    {
        Path p; bool first = true;
        for (auto q : pts) { if (first) p.startNewSubPath (q); else p.lineTo (q); first = false; }
        p.closeSubPath();
        p = p.createPathWithRoundedCorners (10.0f);
        g.setColour (Colours::black.withAlpha (0.5f)); g.fillPath (p, juce::AffineTransform::translation (2, 3));
        brushed (g, p, plateTop, plateBot, seed, 0.05f);
        g.setColour (Colours::white.withAlpha (0.12f)); g.strokePath (p, juce::PathStrokeType (1.2f));
        return p;
    };
    plate ({ { 44, 16 }, { 198, 16 }, { 402, 414 }, { 44, 414 } }, 11);
    plate ({ { 632, 16 }, { 812, 16 }, { 812, 414 }, { 428, 414 } }, 12);
    for (auto s : { juce::Point<float> (58, 30), juce::Point<float> (184, 30), juce::Point<float> (58, 400), juce::Point<float> (380, 400),
                    juce::Point<float> (646, 30), juce::Point<float> (798, 30), juce::Point<float> (450, 400), juce::Point<float> (798, 400) })
        hexScrew (g, s.x, s.y, 5);
    // paneles redondeados de DE ESS y EQ
    for (auto [pp, seed] : { std::pair<Path, int> { pill (850, 16, 330, 172), 21 }, std::pair<Path, int> { pill (850, 202, 590, 214), 22 } })
    {
        g.setColour (Colours::black.withAlpha (0.5f)); g.fillPath (pp, juce::AffineTransform::translation (2, 3));
        brushed (g, pp, plateTop, plateBot, seed, 0.05f);
        g.setColour (Colours::white.withAlpha (0.12f)); g.strokePath (pp, juce::PathStrokeType (1.2f));
    }
    for (auto s : { juce::Point<float> (864, 30), juce::Point<float> (864, 174), juce::Point<float> (1050, 30), juce::Point<float> (1050, 174),
                    juce::Point<float> (864, 216), juce::Point<float> (864, 402), juce::Point<float> (1205, 216), juce::Point<float> (1205, 402) })
        hexScrew (g, s.x, s.y, 5);

    // ---- VU iluminado
    {
        Path win; win.startNewSubPath (272, 72); win.quadraticTo (415, 22, 558, 72); win.lineTo (558, 124); win.quadraticTo (415, 92, 272, 124); win.closeSubPath();
        g.setColour (Colour (0xff15161b)); g.strokePath (win, juce::PathStrokeType (9.0f));
        g.setGradientFill (ColourGradient (Colour (0xffffe8a0), 415, 118, Colour (0xffff5a18), 415, 20, true)); g.fillPath (win);
        g.setGradientFill (ColourGradient (Colour (0x55ffffff), 415, 100, Colours::transparentWhite, 415, 40, false)); g.fillPath (win);
        // escala VU: pivote por debajo
        const float px = 415, py = 252, R = 176;
        const float vals[] { -20, -10, -7, -5, -3, -2, -1, 0, 1, 2, 3 }, frac[] { 0, 0.19f, 0.29f, 0.38f, 0.5f, 0.57f, 0.65f, 0.73f, 0.82f, 0.91f, 1.0f };
        auto ang = [] (float f) { return (-40.0f + 80.0f * f) * pi / 180.0f; };
        g.saveState(); g.reduceClipRegion (win);
        Path red; red.addCentredArc (px, py, R - 4, R - 4, 0, ang (0.73f), ang (1.0f), true);
        g.setColour (Colour (0xffd11c10)); g.strokePath (red, juce::PathStrokeType (4.0f));
        for (int i = 0; i < 11; ++i)
        {
            const float a = ang (frac[i]);
            g.setColour (vals[i] > 0 ? Colour (0xffd11c10) : Colour (0xff6a1a08));
            g.drawLine (juce::Line<float> (polar (px, py, R - 2, a), polar (px, py, R + 7, a)), 1.6f);
            g.setFont (font (15)); g.drawText (juce::String ((int) std::abs (vals[i])), Rectangle<float> (polar (px, py, R + 18, a).x - 12, polar (px, py, R + 18, a).y - 9, 24, 18), juce::Justification::centred);
        }
        for (int i = 0; i <= 5; ++i)
        {
            const float a = ang (0.73f * (float) i / 5.0f);
            g.setColour (Colour (0xff6a1a08)); g.fillEllipse (polar (px, py, R - 12, a).x - 1.6f, polar (px, py, R - 12, a).y - 1.6f, 3.2f, 3.2f);
            g.setFont (font (9)); g.drawText (juce::String (i * 20), Rectangle<float> (polar (px, py, R - 22, a).x - 10, polar (px, py, R - 22, a).y - 5, 20, 10), juce::Justification::centred);
        }
        // aguja
        const float f = juce::jlimit (-0.02f, 1.04f, vuShown <= -20 ? 0.0f : [&] { for (int i = 1; i < 11; ++i) if (vuShown <= vals[i]) return frac[i - 1] + (frac[i] - frac[i - 1]) * (vuShown - vals[i - 1]) / (vals[i] - vals[i - 1]); return 1.0f; }());
        const float na = ang (f);
        g.setColour (Colours::black.withAlpha (0.25f)); g.drawLine (juce::Line<float> (polar (px + 2, py + 2, 60, na), polar (px + 2, py + 2, R + 6, na)), 1.6f);
        g.setColour (Colour (0xff2a0d05)); g.drawLine (juce::Line<float> (polar (px, py, 60, na), polar (px, py, R + 6, na)), 1.4f);
        g.restoreState();
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.35f), 300, 40, Colours::transparentWhite, 360, 90, false)); g.fillPath (win);
        g.setColour (Colours::black.withAlpha (0.6f)); g.strokePath (win, juce::PathStrokeType (1.5f));
    }
    // ---- nombre grabado (letras huecas)
    {
        auto outlined = [&g] (const juce::String& t, float size, float cx, float baseline)
        {
            juce::Font f (juce::FontOptions (juce::Font::getDefaultSerifFontName(), size, juce::Font::plain));
            juce::GlyphArrangement ga; ga.addLineOfText (f, t, 0, baseline);
            const float w = ga.getBoundingBox (0, -1, true).getWidth();
            ga.moveRangeOfGlyphs (0, -1, cx - w * 0.5f, 0);
            Path p; ga.createPath (p);
            g.setColour (Colours::black.withAlpha (0.35f)); g.strokePath (p, juce::PathStrokeType (2.2f), juce::AffineTransform::translation (1, 1.5f));
            g.setColour (Colour (0xffe6e3f2)); g.strokePath (p, juce::PathStrokeType (1.3f));
        };
        outlined ("6961P", 92, 415, 238);
        outlined ("Dj Pava HM", 34, 415, 286);
    }
    // ---- placa izquierda: preamp
    text (g, "48V ON", 118, 32, 13); text (g, "PHANTOM", 118, 106, 13);
    g.setColour (ink); g.drawLine (118, 116, 118, 136, 1.0f);
    g.setColour (Colour (0xff3a0a08)); g.fillEllipse (113, 140, 10, 10);
    { Path s; s.startNewSubPath (188, 120); s.lineTo (196, 108); s.lineTo (226, 108); g.setColour (ink); g.strokePath (s, juce::PathStrokeType (1.2f)); }
    text (g, "120", 200, 124, 13, juce::Justification::centredLeft, 40); text (g, "80", 200, 150, 13, juce::Justification::centredLeft, 40); text (g, "FLAT", 200, 176, 13, juce::Justification::centredLeft, 40);
    // jack de instrumento
    g.setGradientFill (ColourGradient (Colour (0xff3a3b40), 84, 216, Colour (0xff050506), 116, 252, false)); g.fillEllipse (82, 216, 36, 36);
    g.setColour (Colour (0xff020203)); g.fillEllipse (91, 225, 18, 18);
    text (g, "INSTR", 152, 238, 12);
    text (g, U ("0°"), 214, 214, 13, juce::Justification::centredLeft, 40); text (g, "LINE", 214, 238, 13, juce::Justification::centredLeft, 50); text (g, U ("180°"), 214, 262, 13, juce::Justification::centredLeft, 50);
    text (g, "INPUT", 108, 300, 15);
    scaleArc (g, 108, 350, 40, K0, K1, 7);
    text (g, "dB", 258, 286, 13); text (g, "GAIN", 322, 300, 15);
    {
        const char* gl[] { "40", "45", "50", "55", "60" };
        for (int i = 0; i < 5; ++i)
        {
            const float a = GA0 + (GA1 - GA0) * (float) i / 4.0f, ly = 392.0f - (float) i * 21.0f;
            leader (g, 318, 352, 34, a, { 276, ly });
            text (g, gl[i], 258, ly, 14);
        }
    }
    // ---- centro: selector del medidor
    {
        const char* ml[] { "G-R", "LINE IN", "PREOUT", "EQ OUT", "D-S" };
        for (int i = 0; i < 5; ++i)
        {
            const float a = SEL5_0 + (SEL5_1 - SEL5_0) * (float) i / 4.0f;
            leader (g, 415, 352, 18, a, polar (415, 352, 25, a));
        }
        text (g, ml[2], 415, 320, 10.5f);
        text (g, ml[1], 396, 332, 10.5f, juce::Justification::centredRight, 60); text (g, ml[0], 392, 346, 10.5f, juce::Justification::centredRight, 60);
        text (g, ml[3], 434, 332, 10.5f, juce::Justification::centredLeft, 60); text (g, ml[4], 438, 346, 10.5f, juce::Justification::centredLeft, 60);
    }
    // ---- placa derecha: compresor
    text (g, "LINK", 752, 32, 14); text (g, "SEPARATE", 752, 100, 14);
    text (g, "COMPRESS 3:1", 662, 168, 13); text (g, "BYPASS", 662, 246, 14);
    text (g, "THRESHOLD", 752, 158, 14); scaleArc (g, 752, 208, 42, K0, K1, 9);
    text (g, "ATTACK", 520, 308, 15); text (g, "RELEASE", 752, 308, 15);
    {
        const char* al[] { "FAST", "MED FAST", "MEDIUM", "MED SLOW", "SLOW" };
        for (int i = 0; i < 5; ++i)
        {
            const float ly = 330.0f + (float) i * 18.0f;
            text (g, al[i], 636, ly, 13);
            leader (g, 520, 362, 33, AT0 + (AT1 - AT0) * (float) i / 4.0f, { 586, ly });
            leader (g, 752, 362, 33, RE0 + (RE1 - RE0) * (float) i / 4.0f, { 688, ly });
        }
    }
    // ---- DE ESS
    text (g, "DE ESS", 904, 72, 14); text (g, "BYPASS", 904, 140, 14);
    {
        const char* dl[] { "3K", "6K", "9K", "12K", "LIMIT" };
        for (int i = 0; i < 5; ++i)
        {
            const float a = SEL5_0 + (SEL5_1 - SEL5_0) * (float) i / 4.0f;
            leader (g, 1002, 112, 28, a, polar (1002, 112, 40, a));
            const auto lp = polar (1002, 112, 52, a);
            text (g, dl[i], lp.x + (i == 4 ? 12.0f : 0.0f), lp.y, 13);
        }
        text (g, "10:1", polar (1002, 112, 52, SEL5_1).x + 12, polar (1002, 112, 52, SEL5_1).y + 13, 12);
    }
    text (g, "THRESHOLD", 1108, 40, 14); scaleArc (g, 1108, 112, 36, K0, K1, 9);
    // ---- POWER / READY / OUTPUT / preset
    text (g, "POWER", 1252, 36, 12); text (g, "OFF", 1232, 56, 11); text (g, "ON", 1274, 56, 11);
    text (g, "READY", 1346, 36, 12);
    {
        const bool on = param (proc, "power") > 0.5f;
        if (on) { g.setGradientFill (ColourGradient (Colour (0x99ff2a10), 1346, 96, Colours::transparentBlack, 1386, 96, true)); g.fillEllipse (1306, 56, 80, 80); }
        g.setGradientFill (ColourGradient (Colour (0xffd6d8dd), 1330, 80, Colour (0xff55585e), 1362, 112, false)); g.fillEllipse (1328, 78, 36, 36);
        g.setGradientFill (ColourGradient (on ? Colour (0xffffb090) : Colour (0xff6a2018), 1340, 88, on ? Colour (0xffd0180a) : Colour (0xff240806), 1354, 106, true)); g.fillEllipse (1333, 83, 26, 26);
        g.setColour (Colours::white.withAlpha (0.55f)); g.fillEllipse (1339, 88, 7, 5);
    }
    scaleArc (g, 1252, 172, 24, K0, K1, 7); text (g, "OUTPUT", 1300, 172, 11);
    // ---- EQ
    text (g, "EQ IN", 904, 230, 14); text (g, "BYPASS", 904, 298, 14);
    text (g, "XFMR IN", 904, 330, 13); text (g, "BYPASS", 904, 398, 14);
    {
        const char* bn[] { "LOW PEAK", "MID DIP", "HI PEAK" }; const char* bm[] { "+10", "-10", "+10" };
        const float bx[] { 1005, 1150, 1295 };
        const char* fl[3][11] { { "20", "35", "50", "70", "100", "150", "200", "300", "500", "700", "1000" },
                                { "200", "300", "500", "700", "1000", "1K5", "2K", "3K", "4K", "5K", "7K" },
                                { "1K5", "2K", "3K", "4K", "5K", "6K4", "8K", "10K", "12K", "16K", "20K" } };
        for (int b = 0; b < 3; ++b)
        {
            text (g, bn[b], bx[b], 220, 14);
            scaleArc (g, bx[b], 268, 34, K0, K1, 11);
            text (g, "0dB", bx[b] - 28, 316, 12); text (g, bm[b], bx[b] + 28, 316, 12);
            for (int i = 0; i < 11; ++i)
            {
                const float a = F11_0 + (F11_1 - F11_0) * (float) i / 10.0f;
                juce::Point<float> lp;
                if (i < 5) lp = { bx[b] - 34, 404.0f - (float) i * 14.0f };
                else if (i == 5) lp = { bx[b], 337 };
                else lp = { bx[b] + 34, 348.0f + (float) (i - 6) * 14.0f };
                leader (g, bx[b], 372, 20, a, i == 5 ? juce::Point<float> (bx[b], 343) : juce::Point<float> (lp.x + (i < 5 ? 3.0f : -3.0f), lp.y));
                g.setFont (font (10.5f)); g.setColour (ink);
                if (i < 5) g.drawText (fl[b][i], Rectangle<float> (lp.x - 32, lp.y - 6, 30, 12), juce::Justification::centredRight);
                else if (i > 5) g.drawText (fl[b][i], Rectangle<float> (lp.x + 2, lp.y - 6, 30, 12), juce::Justification::centredLeft);
                else g.drawText (fl[b][i], Rectangle<float> (lp.x - 15, lp.y - 6, 30, 12), juce::Justification::centred);
            }
        }
    }
}

void Content::paintOverChildren (Graphics& g)
{
    // etiqueta de preset (tipo etiqueta pegada)
    g.setColour (Colours::black.withAlpha (0.35f)); g.fillRect (sticker.translated (1.5f, 2));
    g.setGradientFill (ColourGradient (Colour (0xfffbf9f2), sticker.getX(), sticker.getY(), Colour (0xffe1ddd0), sticker.getX(), sticker.getBottom(), false)); g.fillRect (sticker);
    g.setColour (Colours::black.withAlpha (0.3f)); g.drawRect (sticker, 0.8f);
    g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain))); g.setColour (Colour (0xff222222));
    g.drawFittedText (proc.getProgramName (proc.getCurrentProgram()), sticker.reduced (4, 1).toNearestInt(), juce::Justification::centred, 2, 0.7f);
}

void Content::mouseUp (const juce::MouseEvent& e)
{
    if (! sticker.contains (e.position)) return;
    juce::PopupMenu m; m.addSectionHeader ("PRESETS");
    for (int i = 0; i < proc.getNumPrograms(); ++i) m.addItem (i + 1, proc.getProgramName (i), true, i == proc.getCurrentProgram());
    juce::Component::SafePointer<Content> sp (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (sticker.toNearestInt())), [sp] (int r) { if (sp != nullptr && r > 0) { sp->proc.setCurrentProgram (r - 1); sp->repaint(); } });
}

void Content::tick()
{
    // VU: 0 VU = -18 dBFS; en G-R y D-S la aguja baja desde 0
    const int mode = (int) param (proc, "meter");
    const float line = proc.mLine.exchange (0), pre = proc.mPre.exchange (0), eqv = proc.mEq.exchange (0), gc = proc.mComp.exchange (0), gd = proc.mDs.exchange (0);
    float target;
    if (mode == 0) target = -gc;
    else if (mode == 4) target = -gd;
    else target = juce::Decibels::gainToDecibels ((mode == 1 ? line : mode == 2 ? pre : eqv) * 0.7071f, -60.0f) + 18.0f;
    vu = juce::jlimit (-22.0f, 4.0f, target);
    const float prev = vuShown;
    vuShown += (vu - vuShown) * 0.25f;     // balistica ~300 ms
    if (std::abs (prev - vuShown) > 0.01f) repaint (260, 16, 310, 120);
    const float bits = param (proc, "power") + 2 * param (proc, "hpf") + 8 * param (proc, "phase") + 16 * param (proc, "link") + 32 * param (proc, "comp")
                     + 64 * param (proc, "deess") + 128 * param (proc, "eq") + 256 * param (proc, "xfmr");
    if (bits != lastBits || proc.getProgramName (proc.getCurrentProgram()) != lastPreset) { lastBits = bits; lastPreset = proc.getProgramName (proc.getCurrentProgram()); repaint(); }
}

VbxEditor::VbxEditor (VbxProcessor& p) : AudioProcessorEditor (p), content (p)
{
    setLookAndFeel (&look);
    addAndMakeVisible (content);
    content.setBounds (0, 0, W, H);
    setResizable (true, true);
    setResizeLimits (1000, 287, 2400, 688);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) W / (double) H);
    setSize (W, H);
    startTimerHz (30);
}
VbxEditor::~VbxEditor() { setLookAndFeel (nullptr); }
void VbxEditor::resized() { content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) W)); }
