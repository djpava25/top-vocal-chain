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
    if (k->kind == KnobKind::mini)
    {
        const juce::Point<float> c ((float) x + (float) w * 0.5f, (float) y + (float) h * 0.5f);
        const float R = (float) std::min (w, h) * 0.5f - 5.0f;
        const bool hot = s.isMouseOverOrDragging();
        const float a0z = a0 + (a1 - a0) * (float) ((0.0 - s.getMinimum()) / (s.getMaximum() - s.getMinimum()));   // 0 dB
        Path track; track.addCentredArc (c.x, c.y, R + 3.5f, R + 3.5f, 0, a0, a1, true);
        g.setColour (Colour (0xff2a2e33)); g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        Path val; val.addCentredArc (c.x, c.y, R + 3.5f, R + 3.5f, 0, std::min (a0z, ang), std::max (a0z, ang), true);
        if (hot) { g.setColour (Colour (0x40f0a030)); g.strokePath (val, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)); }
        g.setColour (hot ? Colour (0xffffb340) : Colour (0xffd28a2a)); g.strokePath (val, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        const int d = (int) (R * 2.0f) + 6;
        g.drawImage (k->image (ang, d, d), Rectangle<float> (c.x - (float) d * 0.5f, c.y - (float) d * 0.5f, (float) d, (float) d));
        return;
    }
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
        img = r3d::render (iw, ih, [R, ang] (float px, float py) { return parts::skirtKnob (px, py, R, ang); }, { 0.03f * R, 0.12f * R, 0.20f * R, 0.55f, 0.06f * R, 0.85f });
    }
    else if (kind == KnobKind::mini)
    {
        const float R = half / 1.15f;
        img = r3d::render (iw, ih, [R, ang] (float px, float py) { return parts::miniKnob (px, py, R, ang); }, { 0.03f * R, 0.10f * R, 0.12f * R, 0.6f, 0.05f * R, 0.8f });
    }
    else
    {
        const float L = half / 1.25f;
        const bool bar = kind == KnobKind::bar;
        img = r3d::render (iw, ih, [L, ang, bar] (float px, float py) { return bar ? parts::barKnob (px, py, L, ang) : parts::peakKnob (px, py, L, ang); },
                           { 0.04f * L, 0.14f * L, 0.16f * L, 0.55f, 0.05f * L, 0.8f });
    }
    imgAng = ang; imgW = w;
    return img;
}

Knob::Knob (APVTS& s, const juce::String& id, KnobKind k, float a0, float a1) : kind (k), pid (id)
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (a0, a1, true);
    setMouseDragSensitivity (k == KnobKind::bar || k == KnobKind::pointer ? 120 : 260);
    getProperties().set ("kind", (int) k);
    setPopupDisplayEnabled (false, false, nullptr);          // el valor se muestra con la etiqueta flotante del panel
    if (k == KnobKind::mini) setRepaintsOnMouseActivity (true);
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
        // encendida: halo rojo amplio y nucleo brillante (se nota a simple vista)
        g.setGradientFill (ColourGradient (Colour (0x55ff2a0c), c.x, c.y, Colour (0x00ff2a0c), c.x + 64, c.y, true));
        g.fillEllipse (c.x - 64, c.y - 64, 128, 128);
        g.setGradientFill (ColourGradient (Colour (0x70ff5a2a), c.x, c.y, Colour (0x00ff5a2a), c.x + 34, c.y, true));
        g.fillEllipse (c.x - 34, c.y - 34, 68, 68);
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
    // etiqueta flotante con el valor real al mover cualquier perilla
    for (auto* k : { &lowBoost, &lowAtten, &lowFreq, &highBoost, &bandwidth, &highFreq, &highAtten, &attFreq, &drive, &out })
    {
        auto show = [this, k] { if (pillKnob != nullptr || k->isMouseButtonDown()) { pillKnob = k; pillTime = juce::Time::getMillisecondCounterHiRes() * 0.001; repaint (pillArea); } };
        k->onDragStart = [this, k] { pillKnob = k; pillTime = juce::Time::getMillisecondCounterHiRes() * 0.001; repaint (pillArea); };
        k->onValueChange = [this, k, show] { if (k->isMouseButtonDown() || k->isMouseOver()) { pillKnob = k; pillTime = juce::Time::getMillisecondCounterHiRes() * 0.001; repaint (pillArea); } };
    }
    buildPanel();
}

void Content::demoPill() { pillKnob = &lowBoost; pillTime = juce::Time::getMillisecondCounterHiRes() * 0.001; repaint(); }

juce::String Content::valueText (const Knob& k) const
{
    const auto s = proc.settings();
    auto hz = [] (float f) { return f >= 1000.0f ? juce::String (f / 1000.0f, f >= 10000.0f ? 0 : 0) + " kHz" : juce::String ((int) f) + " Hz"; };
    auto db = [] (float v) { return (v >= 0 ? "+" : "") + juce::String (v, 1) + " dB"; };
    const float v = (float) k.getValue();
    const float lf = teq::lowFreqs()[s.lowFreq], hf = teq::highFreqs()[s.highFreq], af = teq::attFreqs()[s.attFreq];
    if (k.pid == "lowBoost") { teq::Settings t; t.lowBoost = v; t.lowFreq = s.lowFreq; return "BOOST " + juce::String (v, 1) + U ("  ·  ") + db (teq::responseDb (t, lf)) + " @ " + hz (lf); }
    if (k.pid == "lowAtten") { teq::Settings t; t.lowAtten = v; t.lowFreq = s.lowFreq; return "ATTEN " + juce::String (v, 1) + U ("  ·  ") + db (teq::responseDb (t, lf)) + " @ " + hz (lf); }
    if (k.pid == "highBoost") { teq::Settings t; t.highBoost = v; t.highFreq = s.highFreq; t.bandwidth = s.bandwidth; return "BOOST " + juce::String (v, 1) + U ("  ·  ") + db (teq::responseDb (t, hf)) + " @ " + hz (hf); }
    if (k.pid == "highAtten") { teq::Settings t; t.highAtten = v; t.attFreq = s.attFreq; return "ATTEN " + juce::String (v, 1) + U ("  ·  ") + db (teq::responseDb (t, af)) + " @ " + hz (af); }
    if (k.pid == "bandwidth")
    {
        // ancho real de la campana a -3 dB (en octavas), medido sobre el circuito
        teq::Settings t; t.highBoost = 10; t.bandwidth = v; t.highFreq = s.highFreq;
        const float peak = teq::responseDb (t, hf); float lo = hf, hi = hf;
        for (float f = hf; f > 200.0f; f /= 1.03f) { if (teq::responseDb (t, f) < peak - 3.0f) { lo = f; break; } }
        for (float f = hf; f < 60000.0f; f *= 1.03f) { if (teq::responseDb (t, f) < peak - 3.0f) { hi = f; break; } }
        return "BANDWIDTH " + juce::String (v, 1) + U ("  ·  ") + juce::String (std::log2 (hi / lo), 1) + " oct";
    }
    if (k.pid == "lowFreq") return "CPS  " + hz (lf);
    if (k.pid == "highFreq") return "KCS  " + hz (hf);
    if (k.pid == "attFreq") return "ATTEN SEL  " + hz (af);
    if (k.pid == "drive") return "DRIVE  " + db (v);
    if (k.pid == "out") return "OUTPUT  " + db (v);
    return juce::String (v, 1);
}

void Content::paintOverChildren (Graphics& g)
{
    if (pillKnob == nullptr) return;
    const double age = juce::Time::getMillisecondCounterHiRes() * 0.001 - pillTime;
    const bool held = pillKnob->isMouseButtonDown();
    if (! held && age > 1.6) return;
    const float alpha = held ? 1.0f : (float) juce::jlimit (0.0, 1.0, (1.6 - age) / 0.4);
    const auto txt = valueText (*pillKnob);
    const auto f = font (13, true);
    const float tw = juce::GlyphArrangement::getStringWidth (f, txt) + 26;
    const auto b = pillKnob->getBounds().toFloat();
    float y = b.getY() - 4;
    const bool strip = b.getCentreY() < S;
    const bool topRow = b.getCentreY() < S + 200;                  // fila de arriba: debajo de la perilla (no tapa el titulo)
    Rectangle<float> r (b.getCentreX() - tw * 0.5f, strip ? S + 4 : (topRow ? b.getBottom() - 12.0f : std::max (S + 3.0f, y - 26.0f)), tw, 26);
    r.setX (juce::jlimit (4.0f, (float) W - tw - 4.0f, r.getX()));
    pillArea = r.expanded (6).toNearestInt();
    g.setColour (Colours::black.withAlpha (0.35f * alpha)); g.fillRoundedRectangle (r.translated (0, 2), 13);
    g.setGradientFill (ColourGradient (Colour (0xff23272b).withAlpha (0.94f * alpha), 0, r.getY(), Colour (0xff131517).withAlpha (0.94f * alpha), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, 13);
    g.setColour (Colour (0xffd28a2a).withAlpha (0.8f * alpha)); g.drawRoundedRectangle (r.reduced (0.5f), 13, 1.0f);
    g.setFont (f); g.setColour (Colour (0xffffd9a0).withAlpha (alpha));
    g.drawText (txt, r, juce::Justification::centred);
}

void Content::resized()
{
    auto place = [] (juce::Component& c, float cx, float cy, float d) { c.setBounds (juce::roundToInt (cx - d * 0.5f), juce::roundToInt (cy - d * 0.5f), juce::roundToInt (d), juce::roundToInt (d)); };
    const float dk = Rk * kSkirt * 2.0f, ds = 150.0f;
    place (lowBoost, xLB, yTop, dk); place (lowAtten, xLA, yTop, dk); place (highBoost, xHB, yTop, dk); place (highAtten, xHA, yTop, dk);
    place (bandwidth, xBW, yBot, dk);
    place (lowFreq, xCPS, yBot + 12, ds); place (highFreq, xKCS, yBot, ds); place (attFreq, xSEL, yTop + 4, 120);
    place (drive, 1262, 23, 42); place (out, 1412, 23, 42);
    place (eqIn, xIN, S + 318, 76);
    place (power, xPow, S + 321, 80);
    place (lamp, xLamp, S + 217, 130);
}

namespace
{
    // serigrafia (se dibuja como mascara y despues toma la textura de la pintura)
    void drawSilk (Graphics& g)
    {
        auto t = [&g] (const juce::String& txt, Rectangle<float> r, float size)
        { g.setFont (font (size * 0.98f).withExtraKerningFactor (0.08f)); g.drawText (txt, r, juce::Justification::centred); };
        g.setColour (Colours::white);
        t ("IN", { xIN - 30, S + 260, 60, 24 }, 18);
        auto scale = [&g, &t] (float cx, float cy, const char* title)
        {
            for (int i = 0; i <= 10; ++i)
            {
                const float a = A0 + (A1 - A0) * (float) i / 10.0f;
                const auto p = polar ({ cx, cy }, Rnum + 1.0f, a);
                t (juce::String (i), { p.x - 16, p.y - 11, 32, 22 }, 18.5f);
                g.drawLine ({ polar ({ cx, cy }, 60.5f, a), polar ({ cx, cy }, 64.5f, a) }, i % 5 == 0 ? 2.0f : 1.4f);       // marca de posicion
                if (i < 10) { const auto d = polar ({ cx, cy }, 62.5f, a + (A1 - A0) / 20.0f); g.fillEllipse (d.x - 0.9f, d.y - 0.9f, 1.8f, 1.8f); }
            }
            if (title != nullptr) t (title, { cx - 90, S + 12, 180, 26 }, 18);
        };
        scale (xLB, yTop, "BOOST"); scale (xLA, yTop, "ATTEN"); scale (xHB, yTop, "BOOST"); scale (xHA, yTop, "ATTEN");
        scale (xBW, yBot, nullptr);
        t ("SHARP", { xBW - 100, S + 377, 70, 18 }, 12); t ("BROAD", { xBW + 34, S + 377, 70, 18 }, 12);
        auto labels = [&g, &t] (float cx, float cy, const float* angs, std::initializer_list<const char*> ls, float r, float t0, float t1)
        {
            int i = 0;
            for (auto* l : ls)
            {
                const float a = angs[i++] * deg;
                const auto p = polar ({ cx, cy }, r, a); t (l, { p.x - 24, p.y - 11, 48, 22 }, 18);
                g.drawLine ({ polar ({ cx, cy }, t0, a), polar ({ cx, cy }, t1, a) }, 2.0f);                          // posicion del selector
            }
        };
        labels (xCPS, yBot + 12, cpsA, { "20", "30", "60", "100" }, 84, 63, 69);
        labels (xKCS, yBot, kcsA, { "3", "4", "5", "8", "10", "12", "16" }, 77, 58, 62);
        labels (xSEL, yTop + 4, selA, { "5", "10", "20" }, 66, 50, 54);
        t ("CPS", { xCPS - 40, S + 216, 80, 24 }, 18);
        t ("KCS", { xKCS - 40, S + 218, 80, 24 }, 18);
        t ("ATTEN SEL", { xSEL - 80, S + 12, 160, 26 }, 18);
        t ("OFF", { xPow - 24, S + 266, 48, 22 }, 17);
        t ("ON", { xPow + 22, S + 274, 40, 22 }, 17);
        t ("SIGNAL", { 1350, S + 252, 76, 18 }, 11.5f);
        t ("LOW FREQUENCY", { xCPS - 140, S + 396, 280, 26 }, 18);
        t ("BANDWIDTH", { xBW - 120, S + 396, 240, 26 }, 18);
        t ("HIGH FREQUENCY", { xKCS - 140, S + 396, 280, 26 }, 18);
    }
    // texto grabado en la placa de nombre (negro)
    constexpr float bx0 = 96, by0 = S + 140, bw = 262, bh = 100;
    void drawBadgeText (Graphics& g)
    {
        g.setColour (Colours::white);
        g.setFont (font (34, true).withExtraKerningFactor (0.06f));
        g.drawText ("TOP EQ-1A", Rectangle<float> (bx0, by0 + 8, bw, 40), juce::Justification::centred);
        g.setFont (font (15, true).withExtraKerningFactor (0.16f));
        g.drawText ("PROGRAM EQUALIZER", Rectangle<float> (bx0, by0 + 47, bw, 20), juce::Justification::centred);
        g.drawLine (bx0 + 36, by0 + 71, bx0 + bw - 36, by0 + 71, 1.2f);
        g.setFont (font (13, true).withExtraKerningFactor (0.08f));
        g.drawText ("Dj Pava Hm Music", Rectangle<float> (bx0, by0 + 75, bw, 18), juce::Justification::centred);
    }
    // ranuras grabadas que separan LOW / HIGH / ATTEN
    void drawGrooves (Graphics& g)
    {
        g.setColour (Colours::white);
        g.drawLine (777, S + 22, 777, S + 226, 1.6f);
        g.drawLine (1007, S + 22, 1007, S + 204, 1.6f);
    }
    std::vector<float> blurMask (const juce::Image& m, int w, int h, int r)
    {
        std::vector<float> a ((size_t) (w * h)), t2 ((size_t) (w * h));
        { juce::Image::BitmapData bd (m, juce::Image::BitmapData::readOnly); for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) a[(size_t) (y * w + x)] = bd.getPixelColour (x, y).getFloatAlpha(); }
        for (int pass = 0; r > 0 && pass < 2; ++pass)
        {
            for (int y = 0; y < h; ++y) { float acc = 0; for (int x = -r; x < w + r; ++x) { if (x + r < w) acc += a[(size_t) (y * w + x + r)]; if (x - r - 1 >= 0) acc -= a[(size_t) (y * w + x - r - 1)]; if (x >= 0 && x < w) t2[(size_t) (y * w + x)] = acc / (float) (2 * r + 1); } }
            for (int x = 0; x < w; ++x) { float acc = 0; for (int y = -r; y < h + r; ++y) { if (y + r < h) acc += t2[(size_t) ((y + r) * w + x)]; if (y - r - 1 >= 0) acc -= t2[(size_t) ((y - r - 1) * w + x)]; if (y >= 0 && y < h) a[(size_t) (y * w + x)] = acc / (float) (2 * r + 1); } }
        }
        return a;
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
        const auto toImg = juce::AffineTransform::translation (0, -S).scaled (k);
        auto newMask = [iw, ih] { return juce::Image (juce::Image::SingleChannel, iw, ih, true); };
        juce::Image mask = newMask(), badge = newMask(), badgeTxt = newMask(), groove = newMask(), scratch = newMask();
        { Graphics mg (mask); mg.addTransform (toImg); drawSilk (mg); }
        { Graphics mg (badge); mg.addTransform (toImg); mg.setColour (Colours::white); mg.fillRoundedRectangle (bx0, by0, bw, bh, 7); }
        { Graphics mg (badgeTxt); mg.addTransform (toImg); drawBadgeText (mg); }
        { Graphics mg (groove); mg.addTransform (toImg); drawGrooves (mg); }
        {
            // rayones finisimos y aleatorios (desgaste controlado, casi invisible)
            Graphics mg (scratch); mg.addTransform (toImg);
            juce::Random rnd (2026);
            for (int i = 0; i < 170; ++i)
            {
                const float x = 70 + rnd.nextFloat() * (Content::W - 140), y = S + 10 + rnd.nextFloat() * 440, len = 4 + rnd.nextFloat() * 34, a = (rnd.nextFloat() - 0.5f) * 0.9f;
                mg.setColour (Colours::white.withAlpha (0.25f + 0.5f * rnd.nextFloat()));
                mg.drawLine (x, y, x + len * std::cos (a), y + len * std::sin (a), 0.5f + 0.4f * rnd.nextFloat());
            }
        }
        juce::Image::BitmapData mb (mask, juce::Image::BitmapData::readOnly);
        const auto halo = blurMask (mask, iw, ih, 2), badgeB = blurMask (badge, iw, ih, 2), badgeA = blurMask (badge, iw, ih, 0),
                   badgeT = blurMask (badgeTxt, iw, ih, 0), grooveA = blurMask (groove, iw, ih, 0), scratchA = blurMask (scratch, iw, ih, 0);
        const float knobsX[] { xLB, xLA, xHB, xHA }, sigX = 1388, sigY = S + 232;
        struct Scr { float x, y, slot; };
        const Scr screws[] { { 121, S + 83, 0.62f }, { 120, S + 345, 0.30f }, { 1434, S + 87, 0.95f }, { 1434, S + 350, 0.40f } };
        const Scr badgeScrews[] { { bx0 + 12, by0 + bh * 0.5f, 0.3f }, { bx0 + bw - 12, by0 + bh * 0.5f, 1.1f } };
        const float slotsY[] { S + 121, S + 306 };
        const auto paintMat = r3d::srgb (48.0f / 255.0f, 112.0f / 255.0f, 142.0f / 255.0f, 0, 0.45f);
        const auto ink = r3d::srgb (0.95f, 0.96f, 0.96f, 0, 0.40f);
        const float cxI = (float) iw * 0.5f, cyI = (float) ih * 0.5f;
        auto fn = [&] (float rx, float ry)
        {
            const float px = rx + cxI, py = ry + cyI;                       // pixel de la placa
            const float X = px / k, Y = py / k + S;                         // coordenadas del panel
            r3d::Sample s; s.m = paintMat;
            // piel de naranja de la pintura + leve ondulacion de la chapa
            float h = 0.16f * r3d::fbm (px * 0.16f, py * 0.16f, 1, 3) + 0.05f * r3d::fbm (px * 0.7f, py * 0.7f, 2, 2) + 4.0f * r3d::fbm (px * 0.003f, py * 0.003f, 3, 2);
            const float tone = (0.97f + 0.06f * r3d::fbm (px * 0.01f, py * 0.01f, 4, 3))     // pintura no perfectamente pareja
                             * (1.30f - 0.42f * (px / (float) iw) - 0.30f * (py / (float) ih));  // caida de luz (arriba a la izquierda mas claro)
            // variacion de luz muy suave (vineta) y pintura un poco gastada alrededor de las perillas
            const float vx = X / (float) Content::W - 0.5f, vy = (Y - S) / 466.0f - 0.5f;
            float t2 = tone * (1.0f - 0.10f * (vx * vx * 2.0f + vy * vy * 1.5f));
            for (float kx : knobsX)
            {
                const float rr = std::sqrt ((X - kx) * (X - kx) + (Y - yTop) * (Y - yTop));
                if (rr > Rk + 1 && rr < Rk + 14) t2 *= 1.0f + 0.035f * (1.0f - std::abs (rr - Rk - 7.0f) / 7.0f) * r3d::fbm (px * 0.08f, py * 0.08f, 8, 2);
            }
            s.m.r *= t2; s.m.g *= t2; s.m.b *= t2;
            const size_t ii = (size_t) (juce::jlimit (0, ih - 1, (int) py) * iw + juce::jlimit (0, iw - 1, (int) px));
            // rayones: un poco mas claros y brillantes
            if (scratchA[ii] > 0) { const float q = 0.10f * scratchA[ii]; s.m.r *= 1 + q; s.m.g *= 1 + q; s.m.b *= 1 + q; s.m.gloss += 0.2f * scratchA[ii]; h += 0.05f * scratchA[ii]; }
            // ranuras de separacion grabadas
            if (grooveA[ii] > 0) { s.m.r *= 1 - 0.35f * grooveA[ii]; s.m.g *= 1 - 0.35f * grooveA[ii]; s.m.b *= 1 - 0.35f * grooveA[ii]; h -= 0.9f * grooveA[ii]; }
            // contraste de la serigrafia: la pintura se oscurece apenas alrededor de los numeros
            if (halo[ii] > 0) { const float q = 1.0f - 0.28f * halo[ii]; s.m.r *= q; s.m.g *= q; s.m.b *= q; }
            // serigrafia con desgaste
            const int mx = juce::jlimit (0, iw - 1, (int) px), my = juce::jlimit (0, ih - 1, (int) py);
            float a = mb.getPixelColour (mx, my).getFloatAlpha();
            if (a > 0)
            {
                a *= 0.86f + 0.14f * r3d::fbm (px * 0.45f, py * 0.45f, 5, 2);
                s.m = parts::mixMat (s.m, ink, a); h += 0.7f * a;
            }
            // placa de nombre de aluminio cepillado con letras grabadas en negro
            if (badgeA[ii] > 0)
            {
                auto al = r3d::srgb (0.80f, 0.81f, 0.83f, 1, 0.55f);
                const float br = 0.90f + 0.10f * r3d::fbm (px * 0.01f, py * 1.5f, 12, 3);
                al.r *= br; al.g *= br; al.b *= br;
                s.m = parts::mixMat (s.m, al, badgeA[ii]);
                if (badgeT[ii] > 0) { auto blk = r3d::srgb (0.03f, 0.03f, 0.035f, 0, 0.2f); s.m = parts::mixMat (s.m, blk, std::min (1.0f, badgeT[ii] * 1.3f)); h -= 0.35f * badgeT[ii]; }
            }
            h += 2.5f * badgeB[ii];
            // aro cromado del LED de senal
            {
                const float dx = (X - sigX) * k, dy = (Y - sigY) * k, rr = std::sqrt (dx * dx + dy * dy), Rb = 9.0f * k;
                if (rr < Rb + 1) { const float hb = 1.6f * k * r3d::fillet (Rb - rr, 3.0f * k); s.m = parts::mixMat (s.m, parts::chrome (0.75f), r3d::clamp01 (Rb - rr + 0.5f)); h += hb; }
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
            for (auto& sc : badgeScrews)
            {
                const float dx = (X - sc.x) * k, dy = (Y - sc.y) * k, Rs = 5.0f * k, rs = std::sqrt (dx * dx + dy * dy);
                if (rs < Rs + 1) { const auto ss = parts::screw (dx, dy, Rs, sc.slot); s.m = parts::mixMat (s.m, ss.m, ss.m.alpha); h = h * (1 - ss.m.alpha) + (h + 1.5f + ss.h) * ss.m.alpha; }
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
    for (auto [k, xx] : { std::pair<Knob*, float> { &drive, 1288.0f }, { &out, 1438.0f } })
    {
        const float v = (float) k->getValue();
        g.setColour (k->isMouseOverOrDragging() ? Colour (0xffffc060) : Colour (0xffb9c6cf));
        g.drawText ((v > 0 ? "+" : "") + juce::String (v, 1) + " dB", Rectangle<float> (xx, 4, 64, 38), juce::Justification::centredLeft);
    }
    // LED de senal: verde con senal, ambar fuerte, rojo cerca de 0 dBFS
    {
        const juce::Point<float> c (1388, S + 232); const float r = 5.5f;
        const float db = juce::Decibels::gainToDecibels (sigLevel, -100.0f);
        const bool on = param (proc, "power") > 0.5f && db > -50.0f;
        Colour col = db > -3.0f ? Colour (0xffff3020) : db > -12.0f ? Colour (0xffffb020) : Colour (0xff40e060);
        const float lum = on ? juce::jlimit (0.25f, 1.0f, (db + 50.0f) / 40.0f) : 0.0f;
        if (on) { g.setGradientFill (ColourGradient (col.withAlpha (0.55f * lum), c.x, c.y, col.withAlpha (0.0f), c.x + r * 4.0f, c.y, true)); g.fillEllipse (c.x - r * 4, c.y - r * 4, r * 8, r * 8); }
        g.setGradientFill (ColourGradient (on ? col.interpolatedWith (Colours::white, 0.55f * lum) : Colour (0xff2a2d2a), c.x - r * 0.3f, c.y - r * 0.4f,
                                           on ? col.withMultipliedBrightness (0.55f + 0.45f * lum) : Colour (0xff0e100e), c.x + r, c.y + r, true));
        g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
        g.setColour (Colours::white.withAlpha (0.35f)); g.fillEllipse (c.x - r * 0.55f, c.y - r * 0.65f, r * 0.7f, r * 0.45f);
    }
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
    // LED de senal (sube rapido, baja suave)
    const float pk = proc.inPeak.exchange (0.0f);
    const float prevSig = sigLevel;
    sigLevel = pk > sigLevel ? pk : sigLevel * 0.82f + pk * 0.18f;
    if (std::abs (juce::Decibels::gainToDecibels (sigLevel, -100.0f) - juce::Decibels::gainToDecibels (prevSig, -100.0f)) > 0.5f) repaint (1360, (int) S + 205, 56, 56);
    // etiqueta flotante: se desvanece
    if (pillKnob != nullptr) repaint (pillArea);
    repaint (Rectangle<int> (1180, 0, 380, (int) S));
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
