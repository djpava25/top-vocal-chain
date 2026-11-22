#include "TopMiniCHEditor.h"

using namespace tmcui;

namespace
{
    juce::String U (const char* s) { return juce::String::fromUTF8 (s); }
    juce::Font font (float h, bool bold = false) { return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain)); }
    constexpr float A0 = juce::MathConstants<float>::pi * 7.0f / 6.0f, A1 = juce::MathConstants<float>::pi * 17.0f / 6.0f;
    const juce::Colour ink (0xfff2f2f0), red (0xffd0444c), magenta (0xffc23d98), grey (0xff3d3e42), green (0xff3f9a4d);
    struct KnobPos { float x, y, r; };
    const KnobPos kGain { 188, 140, 34 }, kHf { 76, 464, 33 }, kLf { 190, 538, 33 }, kThr { 188, 700, 33 };
    // botones (x, y, ancho, alto)
    const juce::Rectangle<int> bPhase (44, 90, 42, 40), bColor (186, 252, 42, 40), bHpf (186, 306, 42, 40), bEqIn (186, 378, 42, 40),
                               bHfBell (186, 424, 42, 40), bLfBell (44, 530, 42, 40), bCmp (44, 662, 42, 40), bLine (186, 788, 42, 40), bHiz (186, 840, 42, 40);
}

Look::Look()
{
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1e1f22));
    setColour (juce::PopupMenu::textColourId, ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff45474d));
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff1e1f22));
    setColour (juce::TooltipWindow::textColourId, ink);
}
void Look::drawBubble (juce::Graphics& g, juce::BubbleComponent&, const juce::Point<float>&, const juce::Rectangle<float>& body)
{
    g.setColour (juce::Colour (0xf0141516)); g.fillRoundedRectangle (body, 4);
    g.setColour (juce::Colour (0xff6a6c72)); g.drawRoundedRectangle (body.reduced (0.5f), 4, 1.0f);
}
void Look::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const auto cap = juce::Colour ((juce::uint32) (juce::int64) s.getProperties().getWithDefault ("cap", (juce::int64) 0xff3d3e42));
    const juce::Point<float> c ((float) x + (float) w * 0.5f, (float) y + (float) h * 0.5f);
    const float R = (float) juce::jmin (w, h) * 0.5f - 4.0f;
    const float a = a0 + pos * (a1 - a0);
    // sombra
    for (int i = 6; i > 0; --i)
    {
        const float e = (float) i * 1.2f;
        g.setColour (juce::Colours::black.withAlpha (0.10f));
        g.fillEllipse (c.x - R - e * 0.4f, c.y - R + e * 0.9f, (R + e * 0.4f) * 2, (R + e * 0.5f) * 2);
    }
    // faldon oscuro
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3b3f), c.x, c.y - R, juce::Colour (0xff0b0b0c), c.x, c.y + R, false));
    g.fillEllipse (c.x - R, c.y - R, R * 2, R * 2);
    // tapa brillante de color
    const float rc = R * 0.88f;
    const juce::Point<float> cc (c.x, c.y - R * 0.03f);
    juce::ColourGradient cg (cap.brighter (0.7f), cc.x - rc * 0.35f, cc.y - rc * 0.6f, cap.darker (0.55f), cc.x + rc * 0.55f, cc.y + rc * 0.95f, true);
    cg.addColour (0.42, cap.brighter (0.1f));
    g.setGradientFill (cg); g.fillEllipse (cc.x - rc, cc.y - rc, rc * 2, rc * 2);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.45f), cc.x, cc.y - rc, juce::Colours::black.withAlpha (0.4f), cc.x, cc.y + rc, false));
    g.drawEllipse (cc.x - rc, cc.y - rc, rc * 2, rc * 2, 1.2f);
    // reflejo
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.55f), cc.x - rc * 0.15f, cc.y - rc * 0.85f, juce::Colours::transparentWhite, cc.x, cc.y - rc * 0.05f, false));
    g.fillEllipse (cc.x - rc * 0.62f, cc.y - rc * 0.9f, rc * 1.15f, rc * 0.72f);
    // indicador blanco
    juce::Path p; const float pw = juce::jmax (2.6f, R * 0.11f);
    p.addRoundedRectangle (-pw * 0.5f, -rc * 0.98f, pw, rc * 0.58f, pw * 0.5f);
    p.applyTransform (juce::AffineTransform::rotation (a).translated (cc));
    g.setColour (juce::Colours::black.withAlpha (0.25f)); g.fillPath (p, juce::AffineTransform::translation (0.5f, 1.0f));
    g.setColour (juce::Colours::white); g.fillPath (p);
}

Knob::Knob (APVTS& s, const juce::String& id, juce::Colour cap)
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (A0, A1, true);
    setPopupDisplayEnabled (true, true, nullptr, 1000);
    setMouseDragSensitivity (220);
    getProperties().set ("cap", (juce::int64) cap.getARGB());
    att = std::make_unique<APVTS::SliderAttachment> (s, id, *this);
    if (auto* p = s.getParameter (id)) setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
}

Key::Key (APVTS& s, const juce::String& id, const juce::String& sym) : juce::Button (id), symbol (sym)
{
    setClickingTogglesState (true);
    att = std::make_unique<APVTS::ButtonAttachment> (s, id, *this);
}
void Key::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    const bool on = getToggleState();
    // sombra y marco
    g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (r.translated (0.5f, 2.0f), 3);
    g.setColour (juce::Colour (0xff0d0d0e)); g.fillRoundedRectangle (r.expanded (1.0f), 3.5f);
    auto capR = r.translated (0, on || down ? 1.2f : 0.0f);
    juce::ColourGradient kg (on ? juce::Colour (0xfffbfbfa) : juce::Colour (over ? 0xffe6e6e5 : 0xffd9d9d8), capR.getX(), capR.getY(),
                             on ? juce::Colour (0xffcfcfcd) : juce::Colour (0xff9fa0a2), capR.getX(), capR.getBottom(), false);
    kg.addColour (0.2, on ? juce::Colour (0xfff2f2f0) : juce::Colour (0xffcfd0d0));
    g.setGradientFill (kg); g.fillRoundedRectangle (capR, 3);
    if (on)
    {
        // iluminado: luz blanca calida por dentro
        g.setGradientFill (juce::ColourGradient (juce::Colour (0x55fff3d0), capR.getCentreX(), capR.getCentreY(), juce::Colours::transparentWhite, capR.getX(), capR.getY(), true));
        g.fillRoundedRectangle (capR.reduced (3), 3);
        g.setColour (juce::Colour (0xffffd37a)); g.fillRoundedRectangle (capR.getX() + 8, capR.getBottom() - 4.5f, capR.getWidth() - 16, 2.2f, 1.0f);
    }
    g.setColour (juce::Colours::white.withAlpha (0.7f)); g.drawHorizontalLine ((int) capR.getY() + 1, capR.getX() + 3, capR.getRight() - 3);
    g.setColour (juce::Colours::black.withAlpha (0.45f)); g.drawRoundedRectangle (capR, 3, 0.8f);
    if (symbol.isNotEmpty()) { g.setColour (juce::Colour (0xff2a2a2c)); g.setFont (font (14.0f, true)); g.drawText (symbol, capR, juce::Justification::centred); }
}

//==============================================================================
TopMiniCHEditor::TopMiniCHEditor (TopMiniCHProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      gain (p.apvts, "gain", red), hf (p.apvts, "hf", magenta), lf (p.apvts, "lf", grey), thr (p.apvts, "thr", green),
      phase (p.apvts, "phase"), color (p.apvts, "color"), hpf (p.apvts, "hpf"), eqIn (p.apvts, "eq_in"), hfBell (p.apvts, "hf_bell"),
      lfBell (p.apvts, "lf_bell"), cmp (p.apvts, "cmp"), line (p.apvts, "line"), hiz (p.apvts, "hiz")
{
    setLookAndFeel (&look);
    tips.setLookAndFeel (&look);
    gain.setTooltip (U ("GAIN: ganancia del preamp (limpio, estilo SuperAnalogue)"));
    hf.setTooltip (U ("HF: agudos. Shelf a 3.5 kHz, o campana a 5 kHz con BELL"));
    lf.setTooltip (U ("LF: graves. Shelf a 60 Hz, o campana a 200 Hz con BELL"));
    thr.setRotaryParameters (A1, A0, true);          // como el hardware: +10 a la izquierda, -20 a la derecha
    thr.setTooltip (U ("THRESHOLD: compresor de una perilla (2:1, rodilla suave). Gíralo a la derecha para comprimir más"));
    phase.setTooltip ("Invertir fase");
    color.setTooltip (U ("COLOR: armónicos analógicos suaves (en el lugar del 48V del hardware)"));
    hpf.setTooltip ("Filtro pasa altos 75 Hz, 12 dB/oct");
    eqIn.setTooltip ("Encender el EQ");
    hfBell.setTooltip ("HF en campana (5 kHz)"); lfBell.setTooltip ("LF en campana (200 Hz)");
    cmp.setTooltip ("Encender el compresor");
    line.setTooltip (U ("LINE: entrada de línea (9 dB menos de ganancia)"));
    hiz.setTooltip (U ("Hi-Z: carácter de entrada de instrumento (guitarra/bajo directo)"));
    for (juce::Component* c : { (juce::Component*) &gain, (juce::Component*) &hf, (juce::Component*) &lf, (juce::Component*) &thr,
                                (juce::Component*) &phase, (juce::Component*) &color, (juce::Component*) &hpf, (juce::Component*) &eqIn,
                                (juce::Component*) &hfBell, (juce::Component*) &lfBell, (juce::Component*) &cmp, (juce::Component*) &line, (juce::Component*) &hiz })
        addAndMakeVisible (c);
    buildBackground();
    setResizable (true, true);
    setResizeLimits (W * 6 / 10, H * 6 / 10, W * 3 / 2, H * 3 / 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) W / H);
    setSize (W * 8 / 10, H * 8 / 10);
    startTimerHz (30);
}

TopMiniCHEditor::~TopMiniCHEditor() { stopTimer(); tips.setLookAndFeel (nullptr); setLookAndFeel (nullptr); }

void TopMiniCHEditor::buildBackground()
{
    background = juce::Image (juce::Image::ARGB, W * 3, H * 3, true);
    juce::Graphics g (background);
    g.addTransform (juce::AffineTransform::scale (3.0f));
    const float fw = (float) W, fh = (float) H;
    // panel de aluminio oscuro cepillado
    const auto panel = juce::Rectangle<float> (0, 0, fw, fh);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3b3e), 0, 0, juce::Colour (0xff27282b), fw, fh, false));
    g.fillRoundedRectangle (panel, 8);
    juce::Random rng (66);
    for (float y = 1; y < fh - 1; y += 0.7f)
    {
        g.setColour ((rng.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (0.018f + 0.02f * rng.nextFloat()));
        g.drawHorizontalLine ((int) y, 2, fw - 2);
    }
    g.setColour (juce::Colours::white.withAlpha (0.18f)); g.drawRoundedRectangle (panel.reduced (0.7f), 8, 1.0f);
    // tornillos
    for (float y : { 13.0f, 986.0f })
    {
        g.setColour (juce::Colours::black.withAlpha (0.7f)); g.fillEllipse (128, y - 6, 13, 13);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff45464a), 130, y - 5, juce::Colour (0xff09090a), 138, y + 5, false));
        g.fillEllipse (129, y - 6, 12, 12);
        g.setColour (juce::Colour (0xff050505)); g.drawLine (131.5f, y, 138.5f, y, 1.6f); g.drawLine (135, y - 3.5f, 135, y + 3.5f, 1.6f);
    }
    // marca propia
    g.setColour (ink); g.setFont (font (25.0f, true).withExtraKerningFactor (0.02f));
    g.drawText ("TOP", 14, 22, 80, 28, juce::Justification::centredLeft);
    g.setFont (font (25.0f, true));
    const float wMini = font (25.0f, true).getStringWidthFloat ("MiNi");
    g.setFont (font (25.0f)); const float wCh = font (25.0f).getStringWidthFloat (" CH");
    g.setFont (font (25.0f, true)); g.drawText ("MiNi", juce::Rectangle<float> (fw - 14 - wMini - wCh, 22, wMini + 2, 28), juce::Justification::centredLeft);
    g.setFont (font (25.0f)); g.drawText (" CH", juce::Rectangle<float> (fw - 14 - wCh, 22, wCh + 2, 28), juce::Justification::centredLeft);
    auto label = [&g] (const char* t, float x, float y)
    {
        const auto r = juce::Rectangle<float> (x, y, 72, 19);
        g.setColour (ink); g.fillRect (r);
        g.setColour (juce::Colour (0xff1b1b1d)); g.setFont (font (14.0f, true));
        g.drawText (t, r, juce::Justification::centred);
    };
    label ("INPUT", 13, 57); label ("EQ", 13, 384); label ("COMP", 13, 608);
    auto txt = [&g] (const juce::String& t, float cx, float cy, float size, bool bold = true, juce::Justification j = juce::Justification::centred)
    {
        g.setColour (ink); g.setFont (font (size, bold));
        const float w = 120;
        juce::Rectangle<float> r = j == juce::Justification::centredLeft ? juce::Rectangle<float> (cx, cy - size, w, size * 2)
                                 : j == juce::Justification::centredRight ? juce::Rectangle<float> (cx - w, cy - size, w, size * 2)
                                 : juce::Rectangle<float> (cx - w * 0.5f, cy - size, w, size * 2);
        g.drawText (t, r, j);
    };
    // lineas
    g.setColour (ink.withAlpha (0.85f));
    g.fillRect (8.0f, 368.0f, fw - 16, 1.6f);
    { juce::Path pth; pth.startNewSubPath (8, 592); pth.lineTo (186, 592); pth.lineTo (214, 624); pth.lineTo (fw - 8, 624); g.strokePath (pth, juce::PathStrokeType (1.6f)); }
    g.fillRect (8.0f, 776.0f, fw - 16, 1.6f);
    g.fillRect (124.0f, 444.0f, 62.0f, 1.4f);       // HF -> BELL
    g.fillRect (86.0f, 550.0f, 62.0f, 1.4f);        // BELL -> LF
    g.fillRect (163.0f, 272.0f, 23.0f, 1.4f);       // LED -> COLOR
    g.fillRect (116.0f, 834.0f, 70.0f, 1.4f);       // jack -> LINE/Hi-Z
    g.fillRect (186.0f, 834.0f, 0.0f, 0.0f);
    // INPUT
    txt (U ("Ø"), 24, 110, 19, false);
    txt ("GAIN", kGain.x, 66, 15);
    txt ("-20", kGain.x - 34, 212, 12.5f, false); txt ("+20", kGain.x + 36, 212, 12.5f, false); txt ("dB", kGain.x, 214, 12.5f, false);
    const char* ml[] { "24", "12", "0", "9", "21" };
    for (int i = 0; i < 5; ++i) txt (ml[i], 50, 200.0f + (float) i * 26.0f, 13, false, juce::Justification::centredRight);
    txt ("COLOR", 232, 272, 10, true, juce::Justification::centredLeft);
    {   // icono del pasa altos
        juce::Path pth; pth.startNewSubPath (236, 336); pth.quadraticTo (242, 318, 248, 318); pth.lineTo (264, 318);
        g.setColour (ink); g.strokePath (pth, juce::PathStrokeType (1.5f));
    }
    // EQ
    txt ("HF", 24, 428, 15);
    txt ("0", kHf.x, 413, 12, false); txt ("dB", kHf.x, 508, 12.5f, false); txt (U ("−"), kHf.x - 24, 504, 14); txt ("+", kHf.x + 26, 504, 14);
    txt ("EQ", 234, 390, 12, true, juce::Justification::centredLeft); txt ("IN", 234, 405, 12, true, juce::Justification::centredLeft);
    txt ("BELL", 232, 444, 12, true, juce::Justification::centredLeft);
    txt ("LF", 248, 500, 15);
    txt ("0", kLf.x, 484, 13, false); txt ("dB", kLf.x, 580, 12.5f, false); txt (U ("−"), kLf.x - 24, 577, 14); txt ("+", kLf.x + 26, 577, 14);
    txt ("BELL", 38, 550, 12, true, juce::Justification::centredRight);
    // COMP
    txt ("THRESHOLD", kThr.x, 642, 14);
    txt ("+10", kThr.x - 40, 750, 12.5f); txt ("-20", kThr.x + 42, 750, 12.5f); txt ("dB", kThr.x, 756, 12.5f, false);
    txt ("CMP", 38, 674, 12, true, juce::Justification::centredRight); txt ("IN", 38, 690, 12, true, juce::Justification::centredRight);
    txt ("[LINE]", 232, 808, 11, true, juce::Justification::centredLeft); txt ("Hi-Z", 232, 860, 11, true, juce::Justification::centredLeft);
    // puntos de escala alrededor de las perillas
    for (const auto& k : { kGain, kHf, kLf, kThr })
        for (int i = 0; i <= 10; ++i)
        {
            const float a = A0 + (A1 - A0) * (float) i / 10.0f, rd = k.r + 9.0f;
            const float d = (i == 0 || i == 5 || i == 10) ? 3.4f : 2.6f;
            g.setColour (ink); g.fillEllipse (k.x + rd * std::sin (a) - d * 0.5f, k.y - rd * std::cos (a) - d * 0.5f, d, d);
        }
    // conector de entrada (jack)
    {
        const float cx = 77, cy = 834;
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillEllipse (cx - 40, cy - 37, 80, 80);
        juce::ColourGradient nut (juce::Colour (0xfff0f0f0), cx - 20, cy - 34, juce::Colour (0xff5a5b5f), cx + 22, cy + 34, false);
        nut.addColour (0.5, juce::Colour (0xffa9aaae));
        g.setGradientFill (nut); g.fillEllipse (cx - 38, cy - 38, 76, 76);
        g.setColour (juce::Colours::black.withAlpha (0.35f)); g.drawEllipse (cx - 38, cy - 38, 76, 76, 1.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4b4f), cx, cy - 28, juce::Colour (0xffd2d3d6), cx, cy + 28, false));
        g.fillEllipse (cx - 28, cy - 28, 56, 56);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff050505), cx, cy - 16, juce::Colour (0xff2a2a2d), cx, cy + 16, false));
        g.fillEllipse (cx - 17, cy - 17, 34, 34);
        g.setColour (juce::Colour (0xff000000)); g.fillEllipse (cx - 9, cy - 9, 18, 18);
    }
    g.setColour (ink); g.setFont (font (13.0f, true));
    g.drawText ("MiNi", 18, 952, 60, 22, juce::Justification::centredLeft);
    g.setColour (ink.withAlpha (0.55f)); g.setFont (font (10.5f));
    g.drawText ("Dj Pava Hm Music", 140, 952, 120, 22, juce::Justification::centredRight);
}

void TopMiniCHEditor::resized()
{
    const float s = (float) getWidth() / (float) W;
    const auto T = juce::AffineTransform::scale (s);
    auto putK = [&] (juce::Component& c, const KnobPos& k) { const int sz = (int) (k.r * 2 + 8); c.setBounds ((int) k.x - sz / 2, (int) k.y - sz / 2, sz, sz); c.setTransform (T); };
    auto putB = [&] (juce::Component& c, juce::Rectangle<int> r) { c.setBounds (r); c.setTransform (T); };
    putK (gain, kGain); putK (hf, kHf); putK (lf, kLf); putK (thr, kThr);
    putB (phase, bPhase); putB (color, bColor); putB (hpf, bHpf); putB (eqIn, bEqIn); putB (hfBell, bHfBell);
    putB (lfBell, bLfBell); putB (cmp, bCmp); putB (line, bLine); putB (hiz, bHiz);
}

void TopMiniCHEditor::paint (juce::Graphics& g)
{
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImageTransformed (background, juce::AffineTransform::scale ((float) getWidth() / (float) (W * 3)));
}

void TopMiniCHEditor::paintOverChildren (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale ((float) getWidth() / (float) W));
    auto led = [&g] (juce::Rectangle<float> r, juce::Colour c, bool on)
    {
        g.setColour (juce::Colours::black.withAlpha (0.8f)); g.fillRect (r.expanded (1.2f));
        if (on) { g.setColour (c.withAlpha (0.25f)); g.fillRect (r.expanded (3.0f)); }
        g.setGradientFill (juce::ColourGradient (on ? c.brighter (0.5f) : c.darker (2.2f), r.getX(), r.getY(), on ? c : c.darker (3.0f), r.getX(), r.getBottom(), false));
        g.fillRect (r);
        g.setColour (juce::Colours::white.withAlpha (on ? 0.35f : 0.06f)); g.fillRect (r.getX() + 2, r.getY() + 2, r.getWidth() - 4, 2.0f);
    };
    // medidor de 5 LED en dBu (0 dBu = -18 dBFS)
    const float th[] { 24, 12, 0, -9, -21 };
    const juce::Colour cols[] { juce::Colour (0xffe0303a), juce::Colour (0xffe8c23a), juce::Colour (0xff3cf05a), juce::Colour (0xff3cf05a), juce::Colour (0xff3cf05a) };
    const float dbu = meterDb + 18.0f;
    for (int i = 0; i < 5; ++i) led ({ 62.0f, 191.0f + (float) i * 26.0f, 26.0f, 17.0f }, cols[i], dbu >= th[i]);
    // LED del color (al lado del boton)
    {
        const bool on = color.getToggleState();
        const float cx = 154, cy = 272;
        g.setColour (juce::Colours::black.withAlpha (0.8f)); g.fillEllipse (cx - 9, cy - 9, 18, 18);
        if (on) { g.setColour (juce::Colour (0x66ff3030)); g.fillEllipse (cx - 13, cy - 13, 26, 26); }
        g.setGradientFill (juce::ColourGradient (on ? juce::Colour (0xffffa0a0) : juce::Colour (0xff6a1616), cx - 3, cy - 4, on ? juce::Colour (0xffd01818) : juce::Colour (0xff2a0606), cx + 5, cy + 6, true));
        g.fillEllipse (cx - 7, cy - 7, 14, 14);
    }
    // LED de reduccion de ganancia (verde / amarillo / rojo)
    const float grs[] { 1.0f, 4.0f, 8.0f };
    const juce::Colour gc[] { juce::Colour (0xff3cf05a), juce::Colour (0xffe8c23a), juce::Colour (0xffe0303a) };
    for (int i = 0; i < 3; ++i) led ({ 34.0f + (float) i * 17.0f, 636.0f, 13.0f, 24.0f }, gc[i], grShown >= grs[i]);
}

void TopMiniCHEditor::mouseUp (const juce::MouseEvent& e)
{
    // clic derecho (o clic en el logo de abajo): presets
    const auto p = e.position / ((float) getWidth() / (float) W);
    if (! e.mods.isPopupMenu() && ! juce::Rectangle<float> (0, 940, (float) W, 60).contains (p)) return;
    juce::PopupMenu m; m.addSectionHeader ("Presets");
    for (int i = 0; i < proc.getNumPrograms(); ++i) m.addItem (i + 1, proc.getProgramName (i), true, i == proc.getCurrentProgram());
    juce::Component::SafePointer<TopMiniCHEditor> safe (this);
    m.showMenuAsync ({}, [safe] (int r) { if (safe != nullptr && r > 0) safe->proc.setCurrentProgram (r - 1); });
}

void TopMiniCHEditor::timerCallback()
{
    const float pk = 20.0f * std::log10 (std::max (1.0e-6f, proc.outPeak.exchange (0.0f)));
    meterDb = pk > meterDb ? pk : meterDb - 1.2f;
    const float gr = proc.grDb();
    grShown = gr > grShown ? gr : grShown + (gr - grShown) * 0.2f;
    repaint (juce::Rectangle<int> (20, 180, 160, 680).transformedBy (juce::AffineTransform::scale ((float) getWidth() / (float) W)));
}
