#include "Top73Editor.h"

using namespace t73;
using juce::Colour; namespace Colours = juce::Colours; using juce::Graphics; using juce::Path; using juce::Rectangle; using juce::ColourGradient;

namespace
{
    juce::String U (const char* s) { return juce::String::fromUTF8 (s); }
    juce::Font font (float h, bool bold = false) { return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : 0)); }
    constexpr float pi = juce::MathConstants<float>::pi;
    const Colour panel (0xff27313d), text (0xffe4e8ee), dimText (0xffaab3bf);
    float param (Top73Processor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); }
    void setParam (Top73Processor& p, const char* id, float v)
    { if (auto* q = p.apvts.getParameter (id)) { q->beginChangeGesture(); q->setValueNotifyingHost (q->convertTo0to1 (v)); q->endChangeGesture(); } }
    // angulos
    constexpr float G0 = pi * 60.0f / 180.0f, G1 = pi * (60.0f + 19.0f * 18.0f) / 180.0f;   // perilla roja: MIC 20 arriba-derecha, 18 grados por paso
    constexpr float C0 = pi * 1.25f, C1 = pi * 2.75f;                                          // continuas
    constexpr float S0 = pi * 1.3f, S1 = pi * 2.7f;                                             // selectores
    juce::Point<float> polar (float cx, float cy, float r, float a) { return { cx + r * std::sin (a), cy - r * std::cos (a) }; }
    void label (Graphics& g, const juce::String& t, juce::Point<float> c, float size = 11.5f, Colour col = text, bool bold = false)
    {
        g.setFont (font (size, bold)); g.setColour (col);
        g.drawText (t, Rectangle<float> (c.x - 22, c.y - 7, 44, 14), juce::Justification::centred);
    }
    void dots (Graphics& g, float cx, float cy, float r, float a0, float a1, int n)
    {
        g.setColour (text.withAlpha (0.85f));
        for (int i = 0; i < n; ++i) { const auto p = polar (cx, cy, r, a0 + (a1 - a0) * (float) i / (float) (n - 1)); g.fillEllipse (p.x - 1.4f, p.y - 1.4f, 2.8f, 2.8f); }
    }
    void screw (Graphics& g, float x, float y, float r = 6.0f)
    {
        g.setGradientFill (ColourGradient (Colour (0xffe1e3e6), x - r, y - r, Colour (0xff5c5f64), x + r, y + r, false)); g.fillEllipse (x - r, y - r, r * 2, r * 2);
        g.setColour (Colours::black.withAlpha (0.6f)); g.drawEllipse (x - r, y - r, r * 2, r * 2, 0.8f);
        g.drawLine (x - r * 0.6f, y, x + r * 0.6f, y, 1.4f); g.drawLine (x, y - r * 0.6f, x, y + r * 0.6f, 1.4f);
    }
    void led (Graphics& g, float x, float y, float r, Colour c, bool on)
    {
        if (on) { g.setGradientFill (ColourGradient (c.withAlpha (0.55f), x, y, Colours::transparentBlack, x + r * 3.5f, y, true)); g.fillEllipse (x - r * 3.5f, y - r * 3.5f, r * 7, r * 7); }
        g.setGradientFill (ColourGradient (on ? c.brighter (0.7f) : c.darker (2.2f), x - r * 0.4f, y - r * 0.4f, on ? c : c.darker (3.0f), x + r, y + r, true));
        g.fillEllipse (x - r, y - r, r * 2, r * 2);
        g.setColour (Colours::black.withAlpha (0.6f)); g.drawEllipse (x - r, y - r, r * 2, r * 2, 0.7f);
    }
}

Look::Look()
{
    setColour (juce::PopupMenu::backgroundColourId, Colour (0xff1d242c));
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colour (0xff8a2a22));
    setColour (juce::TooltipWindow::backgroundColourId, Colour (0xff1d242c));
    setColour (juce::TooltipWindow::textColourId, text);
}
void Look::drawBubble (Graphics& g, juce::BubbleComponent&, const juce::Point<float>&, const Rectangle<float>& body)
{
    g.setColour (Colour (0xf01d242c)); g.fillRoundedRectangle (body, 4);
    g.setColour (text.withAlpha (0.5f)); g.drawRoundedRectangle (body.reduced (0.5f), 4, 1.0f);
}
void Look::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& sl)
{
    auto& k = dynamic_cast<Knob&> (sl);
    const juce::Point<float> c ((float) x + (float) w * 0.5f, (float) y + (float) h * 0.5f);
    const float R = (float) std::min (w, h) * 0.5f - 2.0f;
    const float a = a0 + pos * (a1 - a0);
    // sombra proyectada (luz arriba-izquierda)
    auto castShadow = [&] (float r)
    {
        g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.65f), c.x + r * 0.22f, c.y + r * 0.3f, Colours::transparentBlack, c.x + r * 1.4f, c.y + r * 0.3f, true));
        g.fillEllipse (c.x - r * 1.05f, c.y - r * 0.9f, r * 2.6f, r * 2.6f);
    };
    // falda de acrilico transparente
    auto acrylic = [&] (float r)
    {
        g.setColour (Colours::white.withAlpha (0.06f)); g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.35f), c.x - r * 0.7f, c.y - r * 0.7f, Colours::transparentWhite, c.x, c.y, false));
        Path rim; rim.addEllipse (c.x - r, c.y - r, r * 2, r * 2); rim.addEllipse (c.x - r * 0.9f, c.y - r * 0.9f, r * 1.8f, r * 1.8f); rim.setUsingNonZeroWinding (false);
        g.fillPath (rim);
        g.setColour (Colours::white.withAlpha (0.3f)); g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 1.0f);
        juce::Random rnd ((juce::int64) (c.x * 3 + c.y));
        for (int i = 0; i < 14; ++i)   // rayones
        {
            const float t = rnd.nextFloat() * 6.28f, rr = r * (0.75f + 0.22f * rnd.nextFloat());
            const auto p = polar (c.x, c.y, rr, t);
            g.setColour (Colours::white.withAlpha (0.12f)); g.drawLine (p.x, p.y, p.x + 4 * std::cos (t * 3), p.y + 4 * std::sin (t * 3), 0.6f);
        }
    };
    // tapa estriada (bordes ondulados) vista desde arriba
    auto fluted = [&] (float r, Colour col, int n)
    {
        Path p;
        for (int i = 0; i <= n * 4; ++i)
        {
            const float t = (float) i / (float) (n * 4) * 2.0f * pi;
            const float rr = r * (0.94f + 0.06f * std::cos (t * (float) n));
            const auto q = polar (c.x, c.y, rr, t);
            if (i == 0) p.startNewSubPath (q); else p.lineTo (q);
        }
        p.closeSubPath();
        g.setGradientFill (ColourGradient (col.brighter (0.35f), c.x - r * 0.7f, c.y - r * 0.8f, col.darker (0.6f), c.x + r * 0.8f, c.y + r * 0.9f, false));
        g.fillPath (p);
        g.setColour (Colours::black.withAlpha (0.45f)); g.strokePath (p, juce::PathStrokeType (0.9f));
        // cara plana con borde interior
        const float rf = r * 0.8f;
        g.setGradientFill (ColourGradient (col.brighter (0.22f), c.x - rf * 0.6f, c.y - rf * 0.7f, col.darker (0.15f), c.x + rf * 0.7f, c.y + rf * 0.8f, false));
        g.fillEllipse (c.x - rf, c.y - rf, rf * 2, rf * 2);
        g.setColour (Colours::black.withAlpha (0.25f)); g.drawEllipse (c.x - rf, c.y - rf, rf * 2, rf * 2, 1.0f);
        g.setColour (Colours::white.withAlpha (0.25f)); g.drawEllipse (c.x - rf + 0.8f, c.y - rf + 0.8f, rf * 2, rf * 2, 0.6f);
        // desgaste
        juce::Random rnd ((juce::int64) (c.x + c.y * 7));
        for (int i = 0; i < 18; ++i)
        {
            const auto q = polar (c.x, c.y, rf * rnd.nextFloat(), rnd.nextFloat() * 6.28f);
            g.setColour (Colours::white.withAlpha (0.08f + 0.08f * rnd.nextFloat())); g.fillEllipse (q.x, q.y, 1.2f + 2 * rnd.nextFloat(), 0.8f + rnd.nextFloat());
        }
        // linea blanca en el borde
        g.setColour (Colour (0xfff4f1ea)); g.drawLine (juce::Line<float> (polar (c.x, c.y, r * 0.6f, a), polar (c.x, c.y, r * 0.97f, a)), 2.4f);
    };
    switch (k.kind)
    {
        case Red: case Blue:
        {
            const Colour col = k.kind == Red ? Colour (0xffc01e1a) : Colour (0xff1f4f8c);
            acrylic (R);
            const float rc = R * 0.66f;
            castShadow (rc);
            // aleta indicadora: hacia el valor, con linea blanca
            Path fin;
            const float fw = rc * 0.34f;
            fin.addRoundedRectangle (c.x - fw, c.y - R * 0.98f, fw * 2, R * 0.98f, fw * 0.7f);
            fin.applyTransform (juce::AffineTransform::rotation (a, c.x, c.y));
            g.setColour (Colours::black.withAlpha (0.4f)); g.fillPath (fin, juce::AffineTransform::translation (2.5f, 3.0f));
            g.setGradientFill (ColourGradient (col.brighter (0.4f), c.x - R, c.y - R, col.darker (0.6f), c.x + R, c.y + R, false)); g.fillPath (fin);
            g.setColour (Colours::black.withAlpha (0.4f)); g.strokePath (fin, juce::PathStrokeType (0.8f));
            // aleta posterior corta (como la pieza real)
            Path back; back.addRoundedRectangle (c.x - fw * 0.8f, c.y, fw * 1.6f, rc * 1.25f, fw * 0.6f);
            back.applyTransform (juce::AffineTransform::rotation (a, c.x, c.y));
            g.setGradientFill (ColourGradient (col.brighter (0.3f), c.x - R, c.y - R, col.darker (0.7f), c.x + R, c.y + R, false)); g.fillPath (back);
            // tapa redonda
            g.setGradientFill (ColourGradient (col.brighter (0.25f), c.x - rc * 0.7f, c.y - rc * 0.8f, col.darker (0.7f), c.x + rc * 0.8f, c.y + rc, false));
            g.fillEllipse (c.x - rc, c.y - rc, rc * 2, rc * 2);
            const float rf = rc * 0.84f;
            g.setGradientFill (ColourGradient (col.brighter (0.15f), c.x - rf * 0.5f, c.y - rf * 0.7f, col.darker (0.25f), c.x + rf * 0.7f, c.y + rf * 0.8f, false));
            g.fillEllipse (c.x - rf, c.y - rf, rf * 2, rf * 2);
            g.setColour (Colours::black.withAlpha (0.3f)); g.drawEllipse (c.x - rf, c.y - rf, rf * 2, rf * 2, 1.0f);
            g.setColour (Colours::white.withAlpha (0.25f)); g.drawEllipse (c.x - rf + 0.8f, c.y - rf + 0.8f, rf * 2, rf * 2, 0.6f);
            juce::Random rnd ((juce::int64) (c.x * 5));
            for (int i = 0; i < 22; ++i)
            {
                const auto q = polar (c.x, c.y, rf * rnd.nextFloat(), rnd.nextFloat() * 6.28f);
                g.setColour (Colours::white.withAlpha (0.08f + 0.1f * rnd.nextFloat())); g.fillEllipse (q.x, q.y, 1.0f + 2.5f * rnd.nextFloat(), 0.8f + rnd.nextFloat());
            }
            // linea blanca a lo largo de la aleta
            g.setColour (Colour (0xfff4f1ea)); g.drawLine (juce::Line<float> (polar (c.x, c.y, rc * 0.95f, a), polar (c.x, c.y, R * 0.93f, a)), 2.6f);
            g.setColour (Colours::black.withAlpha (0.5f)); g.drawLine (juce::Line<float> (polar (c.x, c.y, rc * 0.2f, a + pi), polar (c.x, c.y, rc * 0.0f, a + pi)), 1.0f);
            break;
        }
        case Gray:
            if ((bool) k.getProperties()["skirt"]) acrylic (R);
            castShadow (R * 0.72f);
            fluted ((bool) k.getProperties()["skirt"] ? R * 0.72f : R * 0.98f, Colour (0xff8f99a6), 18);
            break;
        case Ring:
        {
            castShadow (R);
            // aro metalico moleteado (niquel) con marca oscura
            g.setGradientFill (ColourGradient (Colour (0xffe2ddd0), c.x - R * 0.7f, c.y - R * 0.8f, Colour (0xff6d685d), c.x + R * 0.8f, c.y + R, false));
            g.fillEllipse (c.x - R, c.y - R, R * 2, R * 2);
            for (int i = 0; i < 90; ++i)
            {
                const float t = (float) i / 90.0f * 2.0f * pi;
                g.setColour ((i % 2 ? Colours::white : Colours::black).withAlpha (i % 2 ? 0.18f : 0.28f));
                g.drawLine (juce::Line<float> (polar (c.x, c.y, R * 0.84f, t), polar (c.x, c.y, R, t)), 1.0f);
            }
            // chaflan liso interior
            Path ch; ch.addEllipse (c.x - R * 0.84f, c.y - R * 0.84f, R * 1.68f, R * 1.68f); ch.addEllipse (c.x - R * k.hole, c.y - R * k.hole, R * 2 * k.hole, R * 2 * k.hole); ch.setUsingNonZeroWinding (false);
            g.setGradientFill (ColourGradient (Colour (0xfff1ede4), c.x - R * 0.6f, c.y - R * 0.6f, Colour (0xff8a857a), c.x + R * 0.6f, c.y + R * 0.6f, false)); g.fillPath (ch);
            juce::Random rnd ((juce::int64) (c.x * 11));
            for (int i = 0; i < 40; ++i)
            {
                const auto q = polar (c.x, c.y, R * (k.hole + (0.84f - k.hole) * rnd.nextFloat()), rnd.nextFloat() * 6.28f);
                g.setColour (Colours::black.withAlpha (0.12f)); g.fillEllipse (q.x, q.y, 1.4f, 1.0f);
            }
            g.setColour (Colours::black.withAlpha (0.45f)); g.drawEllipse (c.x - R, c.y - R, R * 2, R * 2, 1.0f);
            g.setColour (Colours::black.withAlpha (0.7f)); g.drawEllipse (c.x - R * k.hole, c.y - R * k.hole, R * 2 * k.hole, R * 2 * k.hole, 1.6f);
            g.setColour (Colour (0xff2a2a2a)); g.drawLine (juce::Line<float> (polar (c.x, c.y, R * (k.hole + 0.04f), a), polar (c.x, c.y, R * 0.99f, a)), 2.6f);
            break;
        }
        case Level:
            castShadow (R);
            fluted (R * 0.98f, Colour (0xff9aa3ae), 14);
            break;
    }
}

Knob::Knob (APVTS& s, const juce::String& id, Kind k, float a0, float a1, float innerHole) : kind (k), hole (innerHole)
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (a0, a1, true);
    const bool stepped = dynamic_cast<juce::AudioParameterChoice*> (s.getParameter (id)) != nullptr;
    setMouseDragSensitivity (stepped ? 140 : 220);
    setPopupDisplayEnabled (true, true, nullptr, 900);
    if (auto* p = s.getParameter (id)) setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    att = std::make_unique<APVTS::SliderAttachment> (s, id, *this);
}
bool Knob::hitTest (int x, int y)
{
    const float cx = (float) getWidth() * 0.5f, cy = (float) getHeight() * 0.5f, r = (float) std::min (getWidth(), getHeight()) * 0.5f - 2.0f;
    const float d = std::hypot ((float) x - cx, (float) y - cy);
    return d <= r && d >= r * hole;
}

void Button::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2);
    const bool on = isOn && isOn();
    g.setColour (Colours::black.withAlpha (0.6f)); g.fillRoundedRectangle (r.translated (1, 2), 3);
    g.setGradientFill (ColourGradient (on ? Colour (0xffe2ddd0) : Colour (0xfff7f4ec), r.getX(), r.getY(), on ? Colour (0xffb3ad9e) : Colour (0xffd6d1c4), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (on ? r.reduced (1.5f) : r, 3);
    g.setColour (Colours::black.withAlpha (0.35f)); g.drawRoundedRectangle (r, 3, 1.0f);
    g.setFont (font (text.length() > 3 ? 10.5f : 14.0f, true)); g.setColour (Colour (0xff3a3d42));
    g.drawText (text, r.withTrimmedBottom (r.getHeight() * 0.25f), juce::Justification::centred);
    if (isOn) led (g, r.getCentreX(), r.getBottom() - 6, 2.6f, Colour (0xffff4a2a), on);
}

Content::Content (Top73Processor& p)
    : proc (p),
      gain (p.apvts, "gain", Red, G0, G1), hf (p.apvts, "hf", Gray, C0, C1),
      midFreq (p.apvts, "midFreq", Ring, S0, S1, 0.62f), midGain (p.apvts, "midGain", Gray, C0, C1),
      lowFreq (p.apvts, "lowFreq", Ring, S0, S1, 0.62f), lowGain (p.apvts, "lowGain", Gray, C0, C1),
      hpf (p.apvts, "hpf", Blue, S0, S1), level (p.apvts, "out", Level, C0, C1)
{
    hf.getProperties().set ("skirt", true);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &gain, &hf, &midFreq, &midGain, &lowFreq, &lowGain, &hpf, &level, &autoBtn, &presetBtn, &phaseBtn, &eqBtn, &powerBtn })
        addAndMakeVisible (c);
    autoBtn.text = "AUTO"; autoBtn.isOn = [this] { return param (proc, "auto") > 0.5f; }; autoBtn.onClick = [this] { setParam (proc, "auto", param (proc, "auto") > 0.5f ? 0.0f : 1.0f); repaint(); };
    presetBtn.text = "PRESET"; presetBtn.onClick = [this] { presetMenu(); };
    phaseBtn.text = U ("Ø"); phaseBtn.isOn = [this] { return param (proc, "phase") > 0.5f; }; phaseBtn.onClick = [this] { setParam (proc, "phase", param (proc, "phase") > 0.5f ? 0.0f : 1.0f); repaint(); };
    eqBtn.text = "EQ"; eqBtn.isOn = [this] { return param (proc, "eq") > 0.5f; }; eqBtn.onClick = [this] { setParam (proc, "eq", param (proc, "eq") > 0.5f ? 0.0f : 1.0f); repaint(); };
    powerBtn.text = ""; powerBtn.onClick = [this] { setParam (proc, "power", param (proc, "power") > 0.5f ? 0.0f : 1.0f); repaint(); };
    gain.setTooltip (U ("GAIN: MIC 20-80 / LINE -20 a +10. Más ganancia = más color (con AUTO el volumen no cambia)"));
    hf.setTooltip (U ("HIGH: shelf de 12 kHz ±16 dB"));
    midFreq.setTooltip (U ("Frecuencia de los medios: 0.36, 0.7, 1.6, 3.2, 4.8, 7.2 kHz u OFF"));
    midGain.setTooltip (U ("Ganancia de los medios ±18 dB"));
    lowFreq.setTooltip (U ("Frecuencia de los graves: 35, 60, 110, 220 Hz u OFF"));
    lowGain.setTooltip (U ("Ganancia de los graves ±16 dB"));
    hpf.setTooltip (U ("Filtro pasa altos 18 dB/oct: 50, 80, 160, 300 Hz u OFF"));
    level.setTooltip (U ("LEVEL: volumen de salida (-20 a +10 dB)"));
    autoBtn.setTooltip (U ("AUTO: compensa la ganancia de la perilla roja para que solo cambie el color"));
    phaseBtn.setTooltip (U ("Invertir fase")); eqBtn.setTooltip (U ("Encender/apagar el EQ")); powerBtn.setTooltip (U ("POWER: encender/apagar el plugin"));
}

void Content::resized()
{
    auto place = [] (juce::Component& c, float cx, float cy, float r) { c.setBounds (juce::roundToInt (cx - r), juce::roundToInt (cy - r), juce::roundToInt (r * 2), juce::roundToInt (r * 2)); };
    autoBtn.setBounds (66, 98, 46, 46); presetBtn.setBounds (66, 152, 46, 46);
    place (gain, 528, 150, 48);
    place (hf, 668, 150, 34);
    place (midFreq, 800, 150, 47); place (midGain, 800, 150, 28);
    place (lowFreq, 935, 150, 47); place (lowGain, 935, 150, 28);
    place (hpf, 1058, 150, 34);
    phaseBtn.setBounds (1112, 98, 46, 46); eqBtn.setBounds (1112, 152, 46, 46);
    place (level, 1294, 128, 21);
    powerBtn.setBounds (1374, 168, 42, 30);
}

void Content::paint (Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.fillAll (Colour (0xff0b0d10));
    // frente con orejas de rack
    Rectangle<float> face (0, 26, r.getWidth(), r.getHeight() - 40);
    // tapa superior en perspectiva
    Path top; top.startNewSubPath (40, 26); top.lineTo (80, 4); top.lineTo (r.getWidth() - 80, 4); top.lineTo (r.getWidth() - 40, 26); top.closeSubPath();
    g.setGradientFill (ColourGradient (Colour (0xff3d4148), 0, 4, Colour (0xff23262b), 0, 26, false)); g.fillPath (top);
    for (int i = 0; i < 60; ++i) { g.setColour (Colours::black.withAlpha (0.5f)); g.fillRect (r.getCentreX() - 260 + (float) i * 8.7f, 11.0f, 4.0f, 9.0f); }
    g.setGradientFill (ColourGradient (panel.brighter (0.12f), 0, face.getY(), panel.darker (0.3f), 0, face.getBottom(), false)); g.fillRect (face);
    juce::Random rnd (7);
    for (int i = 0; i < 3000; ++i) { g.setColour ((rnd.nextBool() ? Colours::white : Colours::black).withAlpha (0.02f)); g.fillRect (rnd.nextFloat() * r.getWidth(), face.getY() + rnd.nextFloat() * face.getHeight(), 1.5f, 1.5f); }
    for (int i = 0; i < 160; ++i)   // rayones y desgaste del panel
    {
        const float x0 = rnd.nextFloat() * r.getWidth(), y0 = face.getY() + rnd.nextFloat() * face.getHeight(), t = rnd.nextFloat() * 6.28f, l = 4 + 26 * rnd.nextFloat();
        g.setColour (Colours::white.withAlpha (0.03f + 0.04f * rnd.nextFloat())); g.drawLine (x0, y0, x0 + l * std::cos (t), y0 + l * std::sin (t), 0.6f);
    }
    g.setColour (Colours::white.withAlpha (0.12f)); g.drawHorizontalLine ((int) face.getY(), 0, r.getWidth());
    g.setColour (Colours::black.withAlpha (0.6f)); g.drawHorizontalLine ((int) face.getBottom(), 0, r.getWidth());
    for (float x : { 18.0f, r.getWidth() - 18.0f })
        for (float y : { 60.0f, 238.0f })
        { g.setColour (Colour (0xff0b0d10)); g.fillRoundedRectangle (x - 12, y - 8, 24, 16, 8); }
    for (float x : { 44.0f, r.getWidth() - 44.0f }) { screw (g, x, 62); screw (g, x, 238); }

    // conector combo XLR/jack (decorativo) y DI
    {
        const float cx = 175, cy = 144;
        g.setColour (Colour (0xff0e0f11)); g.fillEllipse (cx - 30, cy - 30, 60, 60);
        g.setGradientFill (ColourGradient (Colour (0xff3a3c40), cx - 28, cy - 28, Colour (0xff111214), cx + 28, cy + 28, false)); g.fillEllipse (cx - 27, cy - 27, 54, 54);
        g.setColour (Colour (0xff050506)); g.fillEllipse (cx - 9, cy - 9, 18, 18);
        for (auto p : { juce::Point<float> (cx - 15, cy - 8), juce::Point<float> (cx + 15, cy - 8), juce::Point<float> (cx, cy + 17) }) { g.setColour (Colour (0xff050506)); g.fillEllipse (p.x - 4, p.y - 4, 8, 8); }
        screw (g, cx + 30, cy - 27, 4);
        g.setFont (font (11, true)); g.setColour (text);
        g.drawText ("XLR: MIC I/P", Rectangle<float> (cx - 60, cy + 36, 120, 13), juce::Justification::centred);
        g.drawText ("JACK: LINE I/P", Rectangle<float> (cx - 60, cy + 49, 120, 13), juce::Justification::centred);
        const float dx = 420;
        g.setGradientFill (ColourGradient (Colour (0xffc9ccd1), dx - 12, cy - 12, Colour (0xff4a4d52), dx + 12, cy + 12, false)); g.fillEllipse (dx - 13, cy - 13, 26, 26);
        g.setColour (Colour (0xff050506)); g.fillEllipse (dx - 7, cy - 7, 14, 14);
        label (g, "DI I/P", { dx, cy + 30 }, 11, text, true);
    }
    // ventana de preset + LED de saturacion
    {
        Rectangle<float> w (240, 112, 150, 30);
        g.setColour (Colour (0xff0a0c0f)); g.fillRoundedRectangle (w, 3);
        g.setColour (Colours::white.withAlpha (0.15f)); g.drawRoundedRectangle (w, 3, 1.0f);
        g.setFont (font (12, true)); g.setColour (Colour (0xffff6a4a));
        g.drawFittedText (proc.getProgramName (proc.getCurrentProgram()), w.reduced (6, 0).toNearestInt(), juce::Justification::centred, 1, 0.7f);
        led (g, 252, 164, 4, Colour (0xffff2a1a), outDb > -1.0f);
        g.setFont (font (10.5f, true)); g.setColour (dimText); g.drawText ("SAT", Rectangle<float> (262, 158, 40, 12), juce::Justification::left);
        g.drawText ("PRESET", Rectangle<float> (w.getX(), w.getY() - 14, 80, 12), juce::Justification::left);
    }
    // escala de la perilla roja: MIC a la izquierda (20..80) y LINE a la derecha (-20..+10)
    {
        const float cx = 528, cy = 150;
        dots (g, cx, cy, 55, G0, G1, 20);
        auto ga = [] (int i) { return G0 + (G1 - G0) * (float) i / 19.0f; };
        for (int i : { 0, 2, 4, 6, 8, 10, 12 }) label (g, juce::String (20 + 5 * i), polar (cx, cy, 70, ga (i)), 11.5f);
        for (int i : { 13, 15, 17, 19 }) { const int db = -20 + 5 * (i - 13); label (g, (db > 0 ? "+" : "") + juce::String (db), polar (cx, cy, 70, ga (i)), 11.5f); }
        label (g, "MIC.", { cx - 78, cy + 70 }, 11.5f, text, true);
        label (g, "LINE", { cx, cy - 86 }, 11.5f, text, true);
        label (g, "dB", { cx + 78, cy + 70 }, 11.5f, text, true);
    }
    // HIGH 12k
    {
        const float cx = 668, cy = 150;
        dots (g, cx, cy, 42, C0, C1, 11);
        label (g, "-", polar (cx, cy, 50, C0) + juce::Point<float> (-6, 0), 15, text, true);
        label (g, "+", polar (cx, cy, 50, C1) + juce::Point<float> (6, 0), 15, text, true);
        label (g, "0", { cx, cy + 50 }, 11.5f);
        // simbolo shelf
        Path s; s.startNewSubPath (cx - 14, cy - 70); s.lineTo (cx - 2, cy - 70); s.lineTo (cx + 6, cy - 78); s.lineTo (cx + 14, cy - 78);
        s.startNewSubPath (cx - 2, cy - 70); s.lineTo (cx + 6, cy - 62); s.lineTo (cx + 14, cy - 62);
        g.setColour (text); g.strokePath (s, juce::PathStrokeType (1.6f));
        label (g, "12 kHz", { cx, cy + 64 }, 10.5f, dimText, true);
    }
    // MEDIOS y GRAVES (aros de frecuencia)
    auto ring = [&g] (float cx, float cy, int n, std::initializer_list<const char*> labs, const char* unit, bool bell)
    {
        dots (g, cx, cy, 55, S0, S1, n);
        int i = 0;
        for (auto* l : labs) { label (g, l, polar (cx, cy, 70, S0 + (S1 - S0) * (float) i / (float) (n - 1)), 11.0f); ++i; }
        label (g, "-", { cx - 30, cy - 86 }, 15, text, true); label (g, "+", { cx + 30, cy - 86 }, 15, text, true);
        Path s; const float sy = cy - 86;
        if (bell) { s.startNewSubPath (cx - 14, sy + 4); s.lineTo (cx - 6, sy + 4); s.quadraticTo (cx, sy - 8, cx + 6, sy + 4); s.lineTo (cx + 14, sy + 4); }
        else { s.startNewSubPath (cx - 14, sy - 4); s.lineTo (cx - 6, sy - 4); s.lineTo (cx + 2, sy + 4); s.lineTo (cx + 14, sy + 4); s.startNewSubPath (cx - 6, sy - 4); s.lineTo (cx + 2, sy - 12); }
        g.setColour (text); g.strokePath (s, juce::PathStrokeType (1.6f));
        label (g, unit, { cx + 62, cy + 62 }, 11, text, true);
    };
    ring (800, 150, 7, { "OFF", "0.36", "0.7", "1.6", "3.2", "4.8", "7.2" }, "kHz", true);
    ring (935, 150, 5, { "OFF", "35", "60", "110", "220" }, "Hz", false);
    // HPF azul
    {
        const float cx = 1058, cy = 150;
        dots (g, cx, cy, 42, S0, S1, 5);
        int i = 0;
        for (auto* l : { "OFF", "50", "80", "160", "300" }) { label (g, l, polar (cx, cy, 57, S0 + (S1 - S0) * (float) i / 4.0f), 11.0f); ++i; }
        Path s; s.startNewSubPath (cx - 12, cy - 70); s.quadraticTo (cx - 6, cy - 82, cx, cy - 82); s.lineTo (cx + 12, cy - 82);
        g.setColour (text); g.strokePath (s, juce::PathStrokeType (1.6f));
        label (g, "Hz", { cx + 44, cy + 50 }, 11, text, true);
    }
    // seccion digital: LEDs de frecuencia de muestreo, I/P EQ O/P, LEVEL y medidor
    {
        const double sr = proc.getSampleRate();
        const char* srs[] { "44", "48", "88", "96", "176", "192", "INT" };
        const double srv[] { 44100, 48000, 88200, 96000, 176400, 192000, -2 };
        for (int i = 0; i < 7; ++i)
        {
            const float x = 1186 + (float) i * 22.0f;
            g.setFont (font (9.5f, true)); g.setColour (text); g.drawText (srs[i], Rectangle<float> (x - 12, 52, 24, 11), juce::Justification::centred);
            const bool on = (srv[i] > 0 && std::abs (sr - srv[i]) < 1.0) || srv[i] == -2;
            led (g, x, 72, 3.6f, srv[i] == -2 ? Colour (0xff3bd44a) : Colour (0xffffd02a), on);
        }
        const bool inMeter = param (proc, "meter") < 0.5f;
        const char* io[] { "I/P", "EQ", "O/P" };
        const bool ion[] { inMeter, param (proc, "eq") > 0.5f, ! inMeter };
        for (int i = 0; i < 3; ++i)
        {
            const float x = 1184 + (float) i * 28.0f;
            g.setFont (font (9.5f, true)); g.setColour (text); g.drawText (io[i], Rectangle<float> (x - 14, 104, 28, 11), juce::Justification::centred);
            led (g, x, 124, 3.6f, Colour (0xff3bd44a), ion[i]);
        }
        g.setColour (text); g.drawLine (1172, 132, 1172, 186, 1.0f); g.drawLine (1172, 186, 1180, 186, 1.0f);
        g.drawLine (1240, 124, 1268, 124, 1.0f);
        label (g, "LEVEL", { 1294, 92 }, 11, text, true);
        dots (g, 1294, 128, 27, C0, C1, 11);
        // medidor de LEDs (dBu: 0 dBFS = +24 dBu)
        const float lvl = (inMeter ? inDb : outDb) + 24.0f;
        const int th[] { -30, -10, 0, 5, 14, 18, 24 };
        const juce::uint32 cl[] { 0xff3bd44a, 0xff3bd44a, 0xffffd02a, 0xffffd02a, 0xffffd02a, 0xffff2a1a, 0xffff2a1a };
        for (int i = 0; i < 7; ++i)
        {
            const float x = 1190 + (float) i * 21.0f;
            led (g, x, 182, 3.8f, Colour (cl[i]), lvl >= (float) th[i]);
            g.setFont (font (9.5f, true)); g.setColour (text); g.drawText ((th[i] > 0 ? "+" : "") + juce::String (th[i]), Rectangle<float> (x - 12, 192, 24, 11), juce::Justification::centred);
        }
    }
    // logo propio y POWER
    {
        Path wv; const float lx = 1350, ly = 112;
        wv.startNewSubPath (lx, ly + 10); wv.lineTo (lx + 6, ly - 6); wv.lineTo (lx + 12, ly + 10); wv.lineTo (lx + 18, ly - 6);
        g.setColour (Colour (0xffe0261d)); g.strokePath (wv, juce::PathStrokeType (3.2f, juce::PathStrokeType::mitered));
        g.setFont (font (22, true)); g.setColour (text); g.drawText ("Top 73", Rectangle<float> (lx + 22, ly - 12, 100, 26), juce::Justification::left);
        g.setFont (font (10.5f, true)); g.setColour (dimText); g.drawText ("Dj Pava HM  PRE/EQ", Rectangle<float> (lx, ly + 14, 120, 12), juce::Justification::left);
        label (g, "POWER", { 1395, 208 }, 10.5f, text, true);
        led (g, 1395, 224, 3.2f, Colour (0xffff2a1a), param (proc, "power") > 0.5f);
    }
}

void Content::mouseUp (const juce::MouseEvent& e)
{
    if (meterSel.contains (e.position)) { setParam (proc, "meter", param (proc, "meter") < 0.5f ? 1.0f : 0.0f); repaint(); }
    if (Rectangle<float> (240, 112, 150, 30).contains (e.position)) presetMenu();
}
void Content::presetMenu()
{
    juce::PopupMenu m; m.addSectionHeader ("PRESETS");
    for (int i = 0; i < proc.getNumPrograms(); ++i) m.addItem (i + 1, proc.getProgramName (i), true, i == proc.getCurrentProgram());
    juce::Component::SafePointer<Content> sp (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetBtn), [sp] (int r) { if (sp != nullptr && r > 0) { sp->proc.setCurrentProgram (r - 1); sp->repaint(); } });
}
void Content::tick()
{
    auto toDb = [] (float v) { return juce::Decibels::gainToDecibels (v, -80.0f); };
    const float i = toDb (proc.inPeak.exchange (0.0f)), o = toDb (proc.outPeak.exchange (0.0f));
    const float ni = i > inDb ? i : std::max (i, inDb - 0.8f), no = o > outDb ? o : std::max (o, outDb - 0.8f);
    const bool meterChanged = std::abs (ni - inDb) > 0.2f || std::abs (no - outDb) > 0.2f;
    inDb = ni; outDb = no;
    if (meterChanged) { repaint (1170, 160, 170, 45); repaint (240, 150, 70, 30); }
    const auto s = proc.settings();
    const float bits = param (proc, "auto") + 2 * param (proc, "phase") + 4 * param (proc, "power") + 8 * param (proc, "meter") + 16 * (float) proc.getCurrentProgram();
    if (! (s == last) || bits != lastBits) { last = s; lastBits = bits; repaint(); }
}

Top73Editor::Top73Editor (Top73Processor& p) : AudioProcessorEditor (p), content (p)
{
    setLookAndFeel (&look);
    addAndMakeVisible (content);
    content.setBounds (0, 0, W, H);
    setResizable (true, true);
    setResizeLimits (1000, 200, 2400, 480);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) W / (double) H);
    setSize (W, H);
    startTimerHz (30);
}
Top73Editor::~Top73Editor() { setLookAndFeel (nullptr); }
void Top73Editor::resized() { content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) W)); }
