#include "TopConsoleBEditor.h"

using namespace tcbui;

namespace
{
    juce::String U (const char* s) { return juce::String::fromUTF8 (s); }
    const juce::Colour ink (0xffd2d2d0), inkDim (0xff8e8f91), amber (0xffffa040), amberDim (0xff7a4a2a);
    const juce::Colour capGrey (0xffbdbec0), capBlue (0xff2f5fb8), capRed (0xffb3262b), capGreen (0xff2f7d3a);
    const juce::Colour ledRed (0xffff3b30), ledAmber (0xffffb547);
    constexpr float A0 = juce::MathConstants<float>::pi * 7.0f / 6.0f, A1 = juce::MathConstants<float>::pi * 17.0f / 6.0f;
    juce::Font font (float h, bool bold = false) { return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain)); }
    juce::Font monoFont (float h, bool bold = true)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), h, bold ? juce::Font::bold : juce::Font::plain)).withExtraKerningFactor (0.06f);
    }
    void text (juce::Graphics& g, const juce::String& t, float cx, float cy, float size, juce::Colour c = ink, bool bold = false, juce::Justification j = juce::Justification::centred)
    {
        g.setColour (c); g.setFont (font (size, bold));
        const float w = 160;
        auto r = j == juce::Justification::centredLeft ? juce::Rectangle<float> (cx, cy - size, w, size * 2)
               : j == juce::Justification::centredRight ? juce::Rectangle<float> (cx - w, cy - size, w, size * 2)
                                                        : juce::Rectangle<float> (cx - w / 2, cy - size, w, size * 2);
        g.drawText (t, r, j, false);
    }
    // cristal oscuro ambar (visores)
    void amberGlass (juce::Graphics& g, juce::Rectangle<float> r, float corner = 3.0f)
    {
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillRoundedRectangle (r.expanded (1.5f), corner + 1);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1a100b), r.getX(), r.getY(), juce::Colour (0xff2e1b10), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, corner);
        g.setColour (juce::Colour (0xff4a2e1c).withAlpha (0.6f)); g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + 2, r.getRight() - 2);
    }
    void glassShine (juce::Graphics& g, juce::Rectangle<float> r, float corner = 3.0f)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.07f), r.getX(), r.getY(), juce::Colours::transparentWhite, r.getX(), r.getCentreY(), false));
        g.fillRoundedRectangle (r, corner);
    }
}

//==============================================================================
Look::Look()
{
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1b1c1f));
    setColour (juce::PopupMenu::textColourId, ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, amber.withAlpha (0.3f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff1b1c1f));
    setColour (juce::TooltipWindow::textColourId, ink);
    setColour (juce::TooltipWindow::outlineColourId, juce::Colour (0xff3a3c40));
}

void Look::drawBubble (juce::Graphics& g, juce::BubbleComponent&, const juce::Point<float>&, const juce::Rectangle<float>& body)
{
    g.setColour (juce::Colour (0xf01a100b)); g.fillRoundedRectangle (body, 3);
    g.setColour (amber.withAlpha (0.5f)); g.drawRoundedRectangle (body.reduced (0.5f), 3, 1.0f);
}
juce::Font Look::getSliderPopupFont (juce::Slider&) { return monoFont (13.0f); }

void Look::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const auto cap = juce::Colour ((juce::uint32) (juce::int64) s.getProperties().getWithDefault ("cap", (juce::int64) 0xffbdbec0));
    const bool coloured = s.getProperties().getWithDefault ("coloured", false);
    const juce::Point<float> c ((float) x + (float) w * 0.5f, (float) y + (float) h * 0.5f);
    const float R = (float) juce::jmin (w, h) * 0.5f - 4.0f;
    const float a = a0 + pos * (a1 - a0);
    // sombra proyectada hacia abajo
    for (int i = 5; i > 0; --i)
    {
        const float e = (float) i * 1.3f;
        g.setColour (juce::Colours::black.withAlpha (0.10f));
        g.fillEllipse (c.x - R - e * 0.6f, c.y - R + e * 0.9f, (R + e * 0.6f) * 2, (R + e * 0.5f) * 2);
    }
    // faldon (base oscura, visto un poco desde arriba)
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff505257), c.x, c.y - R, juce::Colour (0xff0e0e10), c.x, c.y + R * 1.1f, false));
    g.fillEllipse (c.x - R, c.y - R + R * 0.06f, R * 2, R * 2);
    g.setColour (juce::Colours::black.withAlpha (0.8f)); g.drawEllipse (c.x - R, c.y - R + R * 0.06f, R * 2, R * 2, 0.8f);
    // estrias del faldon
    for (int i = 0; i < 48; ++i)
    {
        const float t = (float) i / 48.0f * juce::MathConstants<float>::twoPi + a;
        const float lit = 0.5f + 0.5f * std::cos (t + 0.6f);
        g.setColour (juce::Colours::white.withAlpha (0.03f + 0.07f * lit));
        g.drawLine (juce::Line<float> ({ c.x + (R - 0.6f) * std::sin (t), c.y + R * 0.06f - (R - 0.6f) * std::cos (t) },
                                       { c.x + (R * 0.9f) * std::sin (t), c.y + R * 0.06f - (R * 0.9f) * std::cos (t) }), 0.8f);
    }
    // tapa (cupula)
    const float rc = R * 0.80f;
    const juce::Point<float> cc (c.x, c.y - R * 0.04f);
    if (coloured)
    {
        juce::ColourGradient cg (cap.brighter (0.55f), cc.x - rc * 0.35f, cc.y - rc * 0.6f, cap.darker (0.65f), cc.x + rc * 0.45f, cc.y + rc * 0.9f, true);
        cg.addColour (0.45, cap);
        g.setGradientFill (cg);
    }
    else
    {
        juce::ColourGradient cg (juce::Colour (0xfff2f2f2), cc.x - rc * 0.35f, cc.y - rc * 0.7f, juce::Colour (0xff6c6d70), cc.x + rc * 0.5f, cc.y + rc * 0.95f, true);
        cg.addColour (0.5, juce::Colour (0xffb9babd));
        g.setGradientFill (cg);
    }
    g.fillEllipse (cc.x - rc, cc.y - rc, rc * 2, rc * 2);
    // anillo de borde de la tapa
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (coloured ? 0.35f : 0.6f), cc.x, cc.y - rc, juce::Colours::black.withAlpha (0.45f), cc.x, cc.y + rc, false));
    g.drawEllipse (cc.x - rc, cc.y - rc, rc * 2, rc * 2, 1.1f);
    // reflejo especular
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (coloured ? 0.45f : 0.55f), cc.x - rc * 0.25f, cc.y - rc * 0.75f,
                                             juce::Colours::transparentWhite, cc.x - rc * 0.05f, cc.y - rc * 0.05f, false));
    g.fillEllipse (cc.x - rc * 0.62f, cc.y - rc * 0.85f, rc * 1.1f, rc * 0.75f);
    // indicador
    const juce::Colour ptr = coloured ? juce::Colour (0xffe9e9e9) : juce::Colour (0xff202022);
    juce::Path p;
    p.addRoundedRectangle (-rc * 0.065f, -rc * 0.98f, rc * 0.13f, rc * 0.78f, rc * 0.06f);
    p.applyTransform (juce::AffineTransform::rotation (a).translated (cc.x, cc.y));
    if (coloured) { g.setColour (juce::Colours::black.withAlpha (0.3f)); g.fillPath (p, juce::AffineTransform::translation (0.4f, 0.8f)); }
    g.setColour (ptr); g.fillPath (p);
    juce::ignoreUnused (s);
}

//==============================================================================
Knob::Knob (APVTS& s, const juce::String& id, juce::Colour cap, bool coloured)
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (A0, A1, true);
    setPopupDisplayEnabled (true, true, nullptr, 1200);
    getProperties().set ("cap", (juce::int64) cap.getARGB());
    getProperties().set ("coloured", coloured);
    setMouseDragSensitivity (240);
    setVelocityModeParameters (0.6, 1, 0.08, true, juce::ModifierKeys::shiftModifier);
    att = std::make_unique<APVTS::SliderAttachment> (s, id, *this);
    param = s.getParameter (id);
    if (param != nullptr) setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
}

//==============================================================================
PushButton::PushButton (APVTS& s, const juce::String& id, const juce::String& label, Style st, juce::Colour l)
    : juce::Button (label), style (st), led (l)
{
    setClickingTogglesState (true);
    att = std::make_unique<APVTS::ButtonAttachment> (s, id, *this);
}

void PushButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    const bool on = getToggleState();
    const float cr = style == flat ? 3.0f : 3.5f;
    if (style == flat)
    {
        g.setColour (on ? juce::Colour (0xff5a4a3a) : juce::Colour (over ? 0xff4a4b4f : 0xff3c3d40)); g.fillRoundedRectangle (r, cr);
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawRoundedRectangle (r, cr, 1.0f);
        g.setColour (on ? amber : juce::Colour (0xffb0b0b0)); g.setFont (font (r.getHeight() * 0.68f, true));
        g.drawText (getButtonText(), r, juce::Justification::centred);
        return;
    }
    // sombra
    for (int i = 3; i > 0; --i) { g.setColour (juce::Colours::black.withAlpha (0.16f)); g.fillRoundedRectangle (r.expanded ((float) i * 0.8f).translated (0, (float) i * 0.9f + 0.5f), cr + 1); }
    g.setColour (juce::Colour (0xff0c0c0d)); g.fillRoundedRectangle (r.expanded (1.0f), cr + 0.5f);
    auto capR = r.translated (0, down ? 0.8f : 0.0f);
    const bool glow = (style == lit || style == power) || (style == big && on && led != juce::Colours::transparentBlack);
    if (glow)
    {
        const auto c = on ? led : led.withSaturation (led.getSaturation() * 0.55f).darker (1.6f);
        if (on) { g.setColour (led.withAlpha (0.28f)); g.fillRoundedRectangle (capR.expanded (4.0f), cr + 3); g.setColour (led.withAlpha (0.18f)); g.fillRoundedRectangle (capR.expanded (7.0f), cr + 6); }
        juce::ColourGradient cg (on ? c.brighter (0.8f) : c.brighter (0.25f), capR.getCentreX(), capR.getY(), on ? c.darker (0.15f) : c.darker (0.4f), capR.getCentreX(), capR.getBottom(), false);
        if (on) cg.addColour (0.45, c.brighter (0.35f));
        g.setGradientFill (cg); g.fillRoundedRectangle (capR, cr);
        if (on)
        {
            // nucleo de la luz
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.55f), capR.getCentreX(), capR.getCentreY(), juce::Colours::transparentWhite, capR.getCentreX(), capR.getY() - capR.getHeight() * 0.1f, true));
            g.fillRoundedRectangle (capR.reduced (capR.getWidth() * 0.12f, capR.getHeight() * 0.12f), cr);
        }
    }
    else
    {
        const bool pressed = on && style == grey;
        juce::ColourGradient cg (juce::Colour (pressed ? 0xffb4b5b8 : (over ? 0xffe9e9ea : 0xffdcdcde)), capR.getCentreX(), capR.getY(),
                                 juce::Colour (pressed ? 0xff6f7073 : 0xff8f9093), capR.getCentreX(), capR.getBottom(), false);
        cg.addColour (0.15, juce::Colour (pressed ? 0xffa8a9ac : 0xffd0d1d3));
        g.setGradientFill (cg); g.fillRoundedRectangle (capR, cr);
        if (style == big && on)
        {
            g.setColour (led.withAlpha (0.25f)); g.fillRoundedRectangle (capR, cr);
        }
        if (pressed)
        {
            // indicador ambar fino en la parte baja del boton
            const auto ind = capR.removeFromBottom (capR.getHeight() * 0.22f).reduced (capR.getWidth() * 0.2f, 1.0f);
            g.setColour (ledAmber.withAlpha (0.35f)); g.fillRoundedRectangle (ind.expanded (2), 2);
            g.setColour (ledAmber); g.fillRoundedRectangle (ind, 1.5f);
            capR = r.translated (0, 0.8f);
        }
    }
    // biselado
    g.setColour (juce::Colours::white.withAlpha (glow && on ? 0.35f : 0.45f)); g.drawHorizontalLine ((int) capR.getY() + 1, capR.getX() + 2, capR.getRight() - 2);
    g.setColour (juce::Colours::black.withAlpha (0.55f)); g.drawRoundedRectangle (capR, cr, 0.8f);
    if (style == power)
    {
        const auto c = capR.getCentre(); const float rr = capR.getHeight() * 0.24f;
        juce::Path pw; pw.addCentredArc (c.x, c.y + 0.5f, rr, rr, 0, 0.6f, juce::MathConstants<float>::twoPi - 0.6f, true);
        g.setColour (on ? juce::Colour (0xffb5501a) : juce::Colour (0xff8a6a50)); g.strokePath (pw, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.drawLine (c.x, c.y - rr - 1.5f, c.x, c.y - 0.5f, 1.6f);
        return;
    }
    if (getButtonText().isNotEmpty())
    {
        juce::Colour tc = textCol;
        if (glow) tc = on ? (led.getPerceivedBrightness() > 0.7f ? juce::Colour (0xffd4661c) : juce::Colours::white) : led.withAlpha (0.55f);
        g.setColour (tc); g.setFont (font (style == big ? 12.5f : 11.0f, true));
        g.drawText (getButtonText(), capR, juce::Justification::centred);
    }
}

//==============================================================================
void tcbui::TextButton::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    if (boxed)
    {
        r = r.reduced (1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillRoundedRectangle (r.translated (0, 1.2f), 3);
        juce::ColourGradient cg (juce::Colour (hover ? 0xffe2e2e4 : 0xffcfd0d2), r.getX(), r.getY(), juce::Colour (0xff8a8b8e), r.getX(), r.getBottom(), false);
        g.setGradientFill (cg); g.fillRoundedRectangle (r, 3);
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawRoundedRectangle (r, 3, 0.8f);
    }
    g.setColour (active ? amber : (hover && ! boxed ? col.brighter (0.4f) : col));
    g.setFont (mono ? monoFont (fontSize) : font (fontSize, true));
    g.drawText (text, r, juce::Justification::centred);
}

//==============================================================================
void BarMeter::tick()
{
    const float in[2] { tcb::g2dB (getL()), tcb::g2dB (getR()) };
    for (int c = 0; c < 2; ++c)
    {
        v[c] = in[c] > v[c] ? in[c] : std::max (in[c], v[c] - 0.9f);
        if (in[c] > hold[c]) { hold[c] = in[c]; holdT[c] = 45; } else if (--holdT[c] < 0) hold[c] -= 0.7f;
    }
    const float gnew = getGr ? getGr() : 0.0f;
    gr = gnew > gr ? gnew : gr + (gnew - gr) * 0.25f;
    repaint();
}

void BarMeter::paint (juce::Graphics& g)
{
    const float H = (float) getHeight();
    auto yOf = [H] (float db) { return 3.0f + juce::jlimit (0.0f, 1.0f, -db / 60.0f) * (H - 8.0f); };
    const float bw = 17.0f, thin = 6.0f;
    const float xs[2] { thinOnLeft ? 14.0f : 0.0f, thinOnLeft ? 34.0f : 20.0f };
    const float xt = thinOnLeft ? 0.0f : 45.0f;
    auto slot = [&] (float x, float w)
    {
        const auto r = juce::Rectangle<float> (x, 0, w, H);
        g.setColour (juce::Colours::black.withAlpha (0.7f)); g.fillRect (r.expanded (1.0f));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2b120b), x, 0, juce::Colour (0xff3a170c), x, H, false));
        g.fillRect (r);
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.35f), x, 0, juce::Colours::transparentBlack, x + w * 0.5f, 0, false));
        g.fillRect (r.withWidth (w * 0.5f));
    };
    auto bar = [&] (float x, float w, float yTop, float yBot)
    {
        if (yBot - yTop < 0.5f) return;
        const auto r = juce::Rectangle<float> (x, yTop, w, yBot - yTop);
        g.setColour (juce::Colour (0xffff5a10).withAlpha (0.18f)); g.fillRect (r.expanded (2.0f, 1.0f));
        juce::ColourGradient cg (juce::Colour (0xffe2521a), x, 0, juce::Colour (0xffb8340c), x + w, 0, false);
        cg.addColour (0.35, juce::Colour (0xffff6a22));
        g.setGradientFill (cg); g.fillRect (r);
        g.setColour (juce::Colour (0xffffb07a)); g.fillRect (x, yTop, w, 1.8f);
    };
    for (int c = 0; c < 2; ++c)
    {
        slot (xs[c], bw);
        bar (xs[c], bw, yOf (v[c]), H - 1.0f);
        if (hold[c] > -60.0f) { g.setColour (juce::Colour (0xffffa060)); g.fillRect (xs[c], yOf (hold[c]) - 1.0f, bw, 2.0f); }
    }
    slot (xt, thin);
    if (gr > 0.05f)
    {
        const float y1 = 3.0f + juce::jlimit (0.0f, 1.0f, gr / 30.0f) * (H - 8.0f);
        const auto r = juce::Rectangle<float> (xt, 0, thin, y1);
        g.setColour (ledAmber.withAlpha (0.25f)); g.fillRect (r.expanded (1.5f, 0));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffc070), xt, 0, juce::Colour (0xffff8a20), xt, y1, false));
        g.fillRect (r);
    }
}

//==============================================================================
void AttenLeds::tick()
{
    comp = std::max (proc.compGr(), comp * 0.85f);
    gate = std::max (proc.gateGr(), gate * 0.85f);
    repaint();
}
void AttenLeds::paint (juce::Graphics& g)
{
    const float ys[] { 9, 25, 41, 57, 72 }, th[] { 20, 14, 10, 6, 3 };
    auto led = [&g] (float cx, float cy, juce::Colour c, bool on)
    {
        const float r = 5.6f;
        g.setColour (juce::Colours::black.withAlpha (0.8f)); g.fillEllipse (cx - r - 1, cy - r - 0.5f, (r + 1) * 2, (r + 1) * 2);
        if (on) { g.setColour (c.withAlpha (0.3f)); g.fillEllipse (cx - r - 3.5f, cy - r - 3.5f, (r + 3.5f) * 2, (r + 3.5f) * 2); }
        juce::ColourGradient cg (on ? c.brighter (0.9f) : c.darker (2.2f).withMultipliedSaturation (0.6f), cx - r * 0.3f, cy - r * 0.4f,
                                 on ? c.darker (0.2f) : juce::Colour (0xff120b08), cx + r, cy + r, true);
        g.setGradientFill (cg); g.fillEllipse (cx - r, cy - r, r * 2, r * 2);
        g.setColour (juce::Colours::white.withAlpha (on ? 0.6f : 0.12f)); g.fillEllipse (cx - r * 0.45f, cy - r * 0.6f, r * 0.6f, r * 0.4f);
    };
    for (int i = 0; i < 5; ++i)
    {
        led (9.0f, ys[i], ledRed, gate >= th[i]);
        led (43.0f, ys[i], juce::Colour (0xff3dff5a), comp >= th[i]);
    }
}

//==============================================================================
namespace { const float faderMarks[] { 12, 6, 0, -5, -10, -20, -30, -40, -50, -60 }; }
float Fader::yOfDb (float db)
{
    db = juce::jlimit (-60.0f, 12.0f, db);
    for (int i = 0; i < 9; ++i)
        if (db >= faderMarks[i + 1])
            return top + step * ((float) i + (faderMarks[i] - db) / (faderMarks[i] - faderMarks[i + 1]));
    return top + step * 9.0f;
}
float Fader::dbOfY (float y)
{
    const float t = juce::jlimit (0.0f, 9.0f, (y - top) / step);
    const int i = juce::jmin (8, (int) t);
    return faderMarks[i] + (faderMarks[i + 1] - faderMarks[i]) * (t - (float) i);
}
Fader::Fader (APVTS& s, const juce::String& id) : param (s.getParameter (id))
{
    att = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v) { value = v; repaint(); }, nullptr);
    att->sendInitialUpdate();
    setRepaintsOnMouseActivity (false);
}
void Fader::paint (juce::Graphics& g)
{
    const float cx = 26.0f, cy = yOfDb (value) - originY;
    const auto capR = juce::Rectangle<float> (cx - 16, cy - 31, 32, 62);
    for (int i = 5; i > 0; --i) { g.setColour (juce::Colours::black.withAlpha (0.10f)); g.fillRoundedRectangle (capR.expanded ((float) i * 0.9f).translated (1.0f, (float) i * 1.5f + 2.0f), 3); }
    auto block = [&g] (juce::Rectangle<float> r, bool upper)
    {
        juce::ColourGradient cg (juce::Colour (0xffe9e9ea), r.getX(), r.getY(), juce::Colour (0xff86878a), r.getX(), r.getBottom(), false);
        cg.addColour (upper ? 0.85 : 0.15, juce::Colour (upper ? 0xffb3b4b7 : 0xffcfd0d2));
        g.setGradientFill (cg); g.fillRoundedRectangle (r, 2.5f);
        for (int i = 0; i < 5; ++i)
        {
            const float yy = r.getY() + 5.0f + (float) i * 5.0f;
            g.setColour (juce::Colours::black.withAlpha (0.28f)); g.fillRect (r.getX() + 1.5f, yy, r.getWidth() - 3.0f, 1.0f);
            g.setColour (juce::Colours::white.withAlpha (0.55f)); g.fillRect (r.getX() + 1.5f, yy + 1.0f, r.getWidth() - 3.0f, 0.8f);
        }
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.25f), r.getX(), 0, juce::Colours::transparentBlack, r.getX() + 6, 0, false));
        g.fillRoundedRectangle (r.withWidth (6), 2.5f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, r.getRight() - 6, 0, juce::Colours::black.withAlpha (0.3f), r.getRight(), 0, false));
        g.fillRoundedRectangle (r.withX (r.getRight() - 6).withWidth (6), 2.5f);
        g.setColour (juce::Colours::black.withAlpha (0.7f)); g.drawRoundedRectangle (r, 2.5f, 0.9f);
    };
    block (capR.withHeight (30), true);
    block (capR.withTrimmedTop (32), false);
    g.setColour (juce::Colour (0xff1a1a1b)); g.fillRect (capR.getX() + 1, cy - 1.0f, capR.getWidth() - 2, 2.0f);
}
void Fader::mouseDown (const juce::MouseEvent& e)
{
    const float y = e.position.y + originY, capY = yOfDb (value);
    dragOffset = std::abs (y - capY) < 31.0f ? y - capY : 0.0f;
    att->beginGesture();
    if (dragOffset == 0.0f) att->setValueAsPartOfGesture (dbOfY (y));
}
void Fader::mouseDrag (const juce::MouseEvent& e)
{
    const float y = e.position.y + originY - dragOffset;
    float db = dbOfY (y);
    if (e.mods.isShiftDown()) db = value + (dbOfY (y) - value) * 0.2f;
    att->setValueAsPartOfGesture (db);
}
void Fader::mouseUp (const juce::MouseEvent&) { att->endGesture(); }
void Fader::mouseDoubleClick (const juce::MouseEvent&) { att->setValueAsCompleteGesture (0.0f); }
void Fader::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    att->setValueAsCompleteGesture (juce::jlimit (-60.0f, 12.0f, value + w.deltaY * 4.0f));
}

//==============================================================================
ValueDisplay::ValueDisplay (APVTS& s, const juce::String& id, std::function<juce::String (float)> fmt) : param (s.getParameter (id)), format (std::move (fmt))
{
    att = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v) { value = v; repaint(); }, nullptr);
    att->sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}
void ValueDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.5f);
    amberGlass (g, r);
    g.setColour (amber.withAlpha (0.25f)); g.setFont (monoFont (13.5f));
    g.drawText (format (value), r.translated (0, 0.5f).expanded (0.6f), juce::Justification::centred);
    g.setColour (juce::Colour (0xffe8935a)); g.drawText (format (value), r, juce::Justification::centred);
    glassShine (g, r);
}
void ValueDisplay::mouseDown (const juce::MouseEvent&) { startNorm = param->convertTo0to1 (value); att->beginGesture(); }
void ValueDisplay::mouseDrag (const juce::MouseEvent& e)
{
    const float n = juce::jlimit (0.0f, 1.0f, startNorm - (float) e.getDistanceFromDragStartY() / (e.mods.isShiftDown() ? 800.0f : 200.0f));
    att->setValueAsPartOfGesture (param->convertFrom0to1 (n));
}
void ValueDisplay::mouseUp (const juce::MouseEvent&) { att->endGesture(); }
void ValueDisplay::mouseDoubleClick (const juce::MouseEvent&) { att->setValueAsCompleteGesture (param->convertFrom0to1 (param->getDefaultValue())); }

//==============================================================================
namespace
{
    // digito de 7 segmentos inclinado
    void sevenSeg (juce::Graphics& g, int digit, float x, float y, float w, float h, juce::Colour on, juce::Colour off)
    {
        static const int segs[10] { 0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f };
        const int m = segs[juce::jlimit (0, 9, digit)];
        const float t = w * 0.2f, sl = h * 0.12f;
        auto P = [x, y, h, sl] (float px, float py) { return juce::Point<float> (x + px + sl * (1.0f - py / h), y + py); };
        auto seg = [&] (int i, juce::Point<float> a, juce::Point<float> b, bool horiz)
        {
            juce::Path p;
            const auto d = (b - a); const float len = d.getDistanceFromOrigin(); const auto u = d / len;
            const juce::Point<float> nrm (-u.y, u.x);
            const float ht = t * 0.5f;
            p.startNewSubPath (a + u * 0.6f);
            p.lineTo (a + u * (ht + 0.6f) + nrm * ht); p.lineTo (b - u * (ht + 0.6f) + nrm * ht);
            p.lineTo (b - u * 0.6f); p.lineTo (b - u * (ht + 0.6f) - nrm * ht); p.lineTo (a + u * (ht + 0.6f) - nrm * ht);
            p.closeSubPath();
            juce::ignoreUnused (horiz);
            const bool lit = (m >> i) & 1;
            if (lit) { g.setColour (on.withAlpha (0.35f)); g.strokePath (p, juce::PathStrokeType (2.2f)); }
            g.setColour (lit ? on : off); g.fillPath (p);
        };
        const float hh = h * 0.5f;
        seg (0, P (0, 0), P (w, 0), true);
        seg (1, P (w, 0), P (w, hh), false);
        seg (2, P (w, hh), P (w, h), false);
        seg (3, P (0, h), P (w, h), true);
        seg (4, P (0, hh), P (0, h), false);
        seg (5, P (0, 0), P (0, hh), false);
        seg (6, P (0, hh), P (w, hh), true);
    }
}
void PresetDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.5f);
    amberGlass (g, r);
    const int n = proc.getCurrentProgram() + 1;
    const int d[3] { (n / 100) % 10, (n / 10) % 10, n % 10 };
    for (int i = 0; i < 3; ++i) sevenSeg (g, d[i], r.getX() + 7.0f + (float) i * 14.5f, r.getY() + 3.0f, 9.0f, r.getHeight() - 6.0f, juce::Colour (0xffff3322), juce::Colour (0xff3a160e));
    const auto name = proc.getProgramName (proc.getCurrentProgram()).toUpperCase();
    auto tr = r.withTrimmedLeft (56).withTrimmedRight (6);
    g.setFont (monoFont (14.0f));
    g.setColour (amber.withAlpha (0.22f)); g.drawText (name, tr.translated (0, 0.6f).expanded (0.8f), juce::Justification::centredLeft, true);
    g.setColour (juce::Colour (0xffe0925a)); g.drawText (name, tr, juce::Justification::centredLeft, true);
    // lineas de la pantalla
    g.setColour (juce::Colours::black.withAlpha (0.12f));
    for (float y = r.getY() + 1; y < r.getBottom(); y += 2.0f) g.drawHorizontalLine ((int) y, r.getX() + 1, r.getRight() - 1);
    glassShine (g, r);
}
void PresetDisplay::showMenu()
{
    juce::PopupMenu m;
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        m.addItem (i + 1, juce::String (i + 1).paddedLeft ('0', 3) + "  " + proc.getProgramName (i), true, i == proc.getCurrentProgram());
    juce::Component::SafePointer<PresetDisplay> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int r)
    {
        if (safe != nullptr && r > 0) { safe->proc.setCurrentProgram (r - 1); safe->repaint(); }
    });
}

//==============================================================================
void RoutingDisplay::toggle (const char* id)
{
    if (auto* p = proc.apvts.getParameter (id)) p->setValueNotifyingHost (flag (id) ? 0.0f : 1.0f);
    repaint();
}
void RoutingDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.5f);
    // marco biselado y cristal
    g.setColour (juce::Colours::black.withAlpha (0.7f)); g.fillRoundedRectangle (r.expanded (1.5f), 6);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff22150e), r.getX(), r.getY(), juce::Colour (0xff3a2416), r.getRight(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 5);
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.55f), r.getCentreX(), r.getCentreY(), juce::Colours::transparentBlack, r.getX(), r.getY(), true));
    g.fillRoundedRectangle (r, 5);
    const auto on = juce::Colour (0xfff0a060), off = juce::Colour (0xff6e4630);
    auto item = [&] (const char* label, const char* id, float x)
    {
        const bool f = flag (id);
        g.setColour (f ? on : off);
        g.drawEllipse (x, 17.0f, 7.0f, 7.0f, 1.1f);
        if (f) { g.setColour (on.withAlpha (0.35f)); g.fillEllipse (x - 2, 15.0f, 11.0f, 11.0f); g.setColour (on); g.fillEllipse (x + 1.5f, 18.5f, 4.0f, 4.0f); }
        g.setFont (monoFont (10.5f)); g.setColour (f ? on : off);
        g.drawText (label, (int) x + 9, 13, 60, 14, juce::Justification::centredLeft);
    };
    item ("FILTERS", "filt_sc", 12); item ("EQ", "eq_sc", 82); item ("EXT", "ext_sc", 122);
    // camino de la senal
    const bool pre = flag ("dyn_pre");
    g.setColour (on.withAlpha (0.75f));
    g.drawLine (50, 30, 50, 50, 1.6f);
    const char* rows[3] { "FILTERS", "EQ", "DYN" };
    const char* rowsPre[3] { "DYN", "FILTERS", "EQ" };
    for (int i = 0; i < 3; ++i)
    {
        const auto row = juce::Rectangle<float> (14, 52.0f + (float) i * 18.0f, 74, 15);
        g.setColour (juce::Colour (0xff4a2e1c).withAlpha (0.6f)); g.fillRect (row);
        g.setColour (on); g.setFont (monoFont (10.5f));
        g.drawText (pre ? rowsPre[i] : rows[i], row, juce::Justification::centred);
        juce::Path t;
        if (i > 0) { t.addTriangle (row.getX() + 6, row.getCentreY() + 2, row.getX() + 12, row.getCentreY() + 2, row.getX() + 9, row.getCentreY() - 2.5f); }
        if (i < 2) { t.addTriangle (row.getRight() - 12, row.getCentreY() - 2, row.getRight() - 6, row.getCentreY() - 2, row.getRight() - 9, row.getCentreY() + 2.5f); }
        g.fillPath (t);
    }
    g.drawLine (50, 106, 50, 120, 1.6f);
    juce::Path arrow; arrow.addTriangle (44, 118, 56, 118, 50, 126); g.fillPath (arrow);
    // columna derecha: estado
    g.setColour (juce::Colour (0xff4a2e1c).withAlpha (0.35f));
    for (int i = 0; i < 3; ++i) g.fillRect (98.0f, 52.0f + (float) i * 18.0f, 70.0f, 15.0f);
    g.setFont (monoFont (9.5f)); g.setColour (off.brighter (0.3f));
    g.drawText (flag ("ext_sc") ? "KEY: EXT" : "KEY: INT", 98, 52, 70, 15, juce::Justification::centred);
    g.drawText (flag ("dyn_in") ? "DYN ON" : "DYN OFF", 98, 70, 70, 15, juce::Justification::centred);
    g.drawText (flag ("hq") ? "HQ 2X" : "HQ OFF", 98, 88, 70, 15, juce::Justification::centred);
    // altavoz: escuchar el sidechain
    const bool lis = flag ("listen");
    const auto spk = lis ? on : off.darker (0.2f);
    juce::Path sp; sp.addRectangle (128, 111, 6, 9); sp.startNewSubPath (134, 111); sp.lineTo (142, 105); sp.lineTo (142, 126); sp.lineTo (134, 120); sp.closeSubPath();
    g.setColour (spk); g.fillPath (sp);
    juce::Path w1; w1.addCentredArc (143, 115.5f, 6, 6, 0, 0.5f, 2.6f, true); juce::Path w2; w2.addCentredArc (143, 115.5f, 10, 10, 0, 0.5f, 2.6f, true);
    g.strokePath (w1, juce::PathStrokeType (1.4f)); g.strokePath (w2, juce::PathStrokeType (1.4f));
    // lineas y reflejo
    g.setColour (juce::Colours::black.withAlpha (0.14f));
    for (float y = r.getY() + 1; y < r.getBottom(); y += 2.0f) g.drawHorizontalLine ((int) y, r.getX() + 2, r.getRight() - 2);
    glassShine (g, r, 5);
}
void RoutingDisplay::mouseUp (const juce::MouseEvent& e)
{
    const auto p = e.position;
    if (p.y < 32) { toggle (p.x < 80 ? "filt_sc" : p.x < 118 ? "eq_sc" : "ext_sc"); return; }
    if (p.x < 92 && p.y >= 48 && p.y < 110) { toggle ("dyn_pre"); return; }
    if (p.x > 118 && p.y > 100) toggle ("listen");
}

//==============================================================================
tcbui::Knob* TopConsoleBEditor::add (const juce::String& id, juce::Colour cap, bool coloured, float cx, float cy, float r,
                                     std::vector<std::pair<float, juce::String>> labels, const juce::String& unit, const juce::String& tip)
{
    auto* k = knobs.add (new tcbui::Knob (proc.apvts, id, cap, coloured));
    const int sz = (int) std::ceil (r * 2 + 8);
    layout.push_back ({ k, { (int) std::round (cx) - sz / 2, (int) std::round (cy) - sz / 2, sz, sz + 3 } });
    if (tip.isNotEmpty()) k->setTooltip (tip);
    addAndMakeVisible (k);
    scales.push_back ({ cx, cy, r, std::move (labels), unit, id });
    return k;
}
tcbui::PushButton* TopConsoleBEditor::addBtn (const juce::String& id, const juce::String& label, tcbui::PushButton::Style st, juce::Colour led,
                                              float cx, float cy, float w, float h, const juce::String& tip)
{
    auto* b = buttons.add (new tcbui::PushButton (proc.apvts, id, label, st, led));
    layout.push_back ({ b, { (int) std::round (cx - w / 2 - 2), (int) std::round (cy - h / 2 - 2), (int) w + 4, (int) h + 4 } });
    if (tip.isNotEmpty()) b->setTooltip (tip);
    addAndMakeVisible (b);
    return b;
}
tcbui::TextButton* TopConsoleBEditor::addText (const juce::String& t, juce::Colour c, juce::Rectangle<int> r, std::function<void()> f, float size)
{
    auto* b = texts.add (new tcbui::TextButton (t, c, size));
    b->onClick = std::move (f);
    layout.push_back ({ b, r });
    addAndMakeVisible (b);
    return b;
}

TopConsoleBEditor::TopConsoleBEditor (TopConsoleBProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&look);
    tips.setLookAndFeel (&look);
    using PB = tcbui::PushButton;
    const auto noLed = juce::Colours::transparentBlack;
    const std::vector<std::pair<float, juce::String>> gainL { { -15, "15" }, { -5, "5" }, { 0, "0" }, { 5, "5" }, { 15, "15" } };
    const std::vector<std::pair<float, juce::String>> qL { { 0.5f, ".5" }, { 1, "1" }, { 1.5f, "1.5" }, { 2, "2" }, { 3, "3" } };
    const std::vector<std::pair<float, juce::String>> relL { { 0.1f, ".1" }, { 0.2f, ".2" }, { 0.4f, ".4" }, { 0.8f, ".8" }, { 1.6f, "1.6" }, { 4, "4" } };

    // ---- FILTROS
    add ("lpf", capGrey, false, 56, 44, 18, { { 3000, "3" }, { 5000, "5" }, { 8000, "8" }, { 12000, "12" }, { 22000, "out" } }, "kHz", "Pasa bajos 12 dB/oct (al tope = apagado)");
    add ("hpf", capGrey, false, 145, 86, 18, { { 16, "out" }, { 30, "30" }, { 70, "70" }, { 120, "120" }, { 200, "200" }, { 350, "350" } }, "Hz", "Pasa altos 18 dB/oct (al minimo = apagado)");
    // ---- EQ
    add ("hf_g", capBlue, true, 56, 193, 17, gainL, "dB+-", "HF: ganancia de agudos");
    addBtn ("hf_bell", {}, PB::grey, noLed, 146, 182, 40, 20, U ("HF en campana en vez de shelf"));
    add ("hf_f", capGrey, false, 145, 236, 17, { { 1500, "1.5" }, { 3000, "3" }, { 8000, "8" }, { 16000, "16" } }, "kHz");
    add ("hmf_g", capBlue, true, 56, 291, 17, gainL, "dB+-", "HMF: medios altos");
    add ("hmf_f", capGrey, false, 145, 336, 17, { { 600, ".6" }, { 1500, "1.5" }, { 3000, "3" }, { 7000, "7" } }, "kHz");
    add ("hmf_q", capGrey, false, 56, 376, 16, qL, "Q");
    addBtn ("eq_in", "IN", PB::lit, ledRed, 147, 431, 22, 34, "EQ activo");
    add ("lmf_g", capBlue, true, 56, 471, 17, gainL, "dB+-", "LMF: medios graves");
    add ("lmf_f", capGrey, false, 145, 516, 17, { { 200, ".2" }, { 500, ".5" }, { 1000, "1" }, { 2500, "2.5" } }, "kHz");
    add ("lmf_q", capGrey, false, 56, 558, 16, qL, "Q");
    add ("lf_f", capGrey, false, 145, 616, 17, { { 30, "30" }, { 60, "60" }, { 100, "100" }, { 200, "200" }, { 450, "450" } }, "Hz");
    add ("lf_g", capBlue, true, 56, 656, 17, gainL, "dB+-", "LF: graves");
    addBtn ("lf_bell", {}, PB::grey, noLed, 146, 681, 40, 20, U ("LF en campana en vez de shelf"));
    // ---- CENTRO
    add ("in_trim", capGrey, false, 366, 91, 19, { { -20, "-20" }, { -12, "12" }, { 0, "0" }, { 12, "12" }, { 20, "+20" } }, "dB", "Trim de entrada");
    addBtn ("phase", {}, PB::grey, noLed, 445, 93, 21, 23, "Invertir fase");
    add ("drive", capRed, true, 366, 182, 19, { { 0, "0" }, { 15, "15" }, { 40, "40" }, { 70, "70" }, { 100, "100" } }, "%",
         U ("Preamp con transformador: más armónicos y saturación sin cambiar el volumen"));
    addBtn ("pre_in", {}, PB::lit, ledRed, 445, 182, 21, 21, "Preamp encendido");
    add ("width", capBlue, true, 366, 276, 18, { { 0, "0" }, { 1, "1" }, { 2, "2" } }, {}, U ("Ancho estéreo (0 = mono, 2 = muy ancho)"));
    add ("pan", capBlue, true, 444, 276, 18, { { -1, "L" }, { 0, "C" }, { 1, "R" } }, {}, "Paneo");
    addBtn ("mono", "MONO", PB::big, ledAmber, 366, 358, 40, 40, U ("Suma la señal a mono"));
    addBtn ("cut", "CUT", PB::big, ledRed, 366, 429, 40, 40, "Silenciar el canal")->textCol = juce::Colour (0xffb3262b);
    addBtn ("c_fast", {}, PB::grey, noLed, 366, 523, 21, 21, U ("Ataque rápido del compresor"));
    addBtn ("g_fast", {}, PB::grey, noLed, 366, 553, 21, 21, U ("Ataque rápido del gate"));
    addBtn ("listen", {}, PB::lit, juce::Colour (0xffd81e1e), 366, 620, 21, 21, U ("Escuchar la señal del sidechain"));
    addBtn ("power", {}, PB::power, ledAmber, 240, 21, 38, 21, "Encender / apagar el plugin");
    // ---- DINAMICA
    add ("c_ratio", capGrey, false, 665, 88, 19, { { 0, "out" }, { 2, "2" }, { 4, "4" }, { 6, "10" }, { 9, "ds" } }, {},
         U ("Ratio. La última posición 'ds' es el modo de-esser"));
    add ("c_thr", capGrey, false, 754, 138, 19, { { -40, "-40" }, { -30, "-30" }, { -10, "-10" }, { 0, "0" } }, "dB", "Umbral del compresor");
    add ("c_rel", capGrey, false, 665, 191, 19, relL, "s", "Release del compresor");
    addBtn ("dyn_in", "IN", PB::lit, ledAmber, 665, 266, 22, 34, U ("Dinámica activa"));
    add ("g_range", capGreen, true, 665, 344, 19, { { 0, "out" }, { 10, "10" }, { 20, "20" }, { 40, "40" } }, "dB", "Rango del gate / expansor");
    add ("g_thr", capGreen, true, 754, 394, 19, { { -60, "-60" }, { -45, "-45" }, { -15, "-15" }, { 0, "0" } }, "dB", "Umbral del gate");
    add ("g_rel", capGreen, true, 665, 447, 19, relL, "s", "Release del gate");
    addBtn ("g_exp", {}, PB::lit, ledAmber, 754, 462, 40, 20, "Expansor 1:2 en vez de puerta");

    // ---- medidores y visores
    inMeter = std::make_unique<tcbui::BarMeter> ([this] { return proc.inPeakL.load(); }, [this] { return proc.inPeakR.load(); }, [this] { return proc.gateGr(); }, false);
    outMeter = std::make_unique<tcbui::BarMeter> ([this] { return proc.outPeakL.load(); }, [this] { return proc.outPeakR.load(); }, [this] { return proc.compGr(); }, true);
    layout.push_back ({ inMeter.get(), { 237, 83, 51, 613 } });
    layout.push_back ({ outMeter.get(), { 521, 83, 51, 613 } });
    inMeter->setTooltip ("Entrada L/R (barra fina: reduccion del gate)");
    outMeter->setTooltip ("Salida L/R (barra fina: reduccion del compresor)");
    atten = std::make_unique<tcbui::AttenLeds> (proc);
    atten->setTooltip (U ("Atenuación en dB: rojo = gate, verde = compresor"));
    layout.push_back ({ atten.get(), { 728, 226, 54, 82 } });
    fader = std::make_unique<tcbui::Fader> (proc.apvts, "fader");
    fader->setTooltip (U ("Fader VCA (doble clic = 0 dB, Shift = fino). Por encima de 0 dB añade color"));
    layout.push_back ({ fader.get(), { 418, (int) tcbui::Fader::originY, 54, 375 } });
    mixBox = std::make_unique<tcbui::ValueDisplay> (proc.apvts, "c_mix", [] (float v) { return juce::String (v, 1) + " %"; });
    mixBox->setTooltip (U ("Mezcla paralela del compresor (arrastra arriba/abajo)"));
    layout.push_back ({ mixBox.get(), { 711, 9, 62, 25 } });
    trimBox = std::make_unique<tcbui::ValueDisplay> (proc.apvts, "out_trim", [] (float v) { return juce::String (v, 1) + " dB"; });
    trimBox->setTooltip ("Trim de salida (arrastra arriba/abajo)");
    layout.push_back ({ trimBox.get(), { 336, 675, 61, 25 } });
    presetTop = std::make_unique<tcbui::PresetDisplay> (proc);
    presetTop->setTooltip ("Clic para elegir preset");
    layout.push_back ({ presetTop.get(), { 277, 10, 255, 25 } });
    routing = std::make_unique<tcbui::RoutingDisplay> (proc);
    routing->setTooltip (U ("Arriba: qué pasa al sidechain. Lista: orden (clic para cambiar). Altavoz: escuchar sidechain"));
    layout.push_back ({ routing.get(), { 618, 566, 184, 140 } });
    for (juce::Component* c : { (juce::Component*) inMeter.get(), (juce::Component*) outMeter.get(), (juce::Component*) atten.get(), (juce::Component*) fader.get(),
                                (juce::Component*) mixBox.get(), (juce::Component*) trimBox.get(), (juce::Component*) presetTop.get(), (juce::Component*) routing.get() })
        addAndMakeVisible (c);

    // ---- barras de texto
    const juce::Colour barTxt (0xff8a8b8e);
    addText ("UNDO", barTxt, { 12, 724, 50, 22 }, [this] { proc.undoManager.undo(); })->setTooltip ("Deshacer");
    addText ("REDO", barTxt, { 64, 724, 50, 22 }, [this] { proc.undoManager.redo(); })->setTooltip ("Rehacer");
    abBtn[0] = addText ("A", barTxt, { 150, 724, 20, 22 }, [this] { switchAB (0); });
    abBtn[1] = addText ("B", barTxt, { 176, 724, 20, 22 }, [this] { switchAB (1); });
    abBtn[0]->setTooltip ("Comparar ajustes A / B"); abBtn[1]->setTooltip ("Comparar ajustes A / B");
    addText (U ("◁"), amberDim, { 210, 724, 22, 22 }, [this] { stepPreset (-1); }, 13.0f);
    bottomName = addText ({}, juce::Colour (0xffc8794a), { 240, 724, 332, 22 }, [this] { presetTop->showMenu(); }, 15.0f);
    bottomName->mono = true;
    addText (U ("▶"), amberDim, { 578, 724, 22, 22 }, [this] { stepPreset (1); }, 13.0f);
    auto* hq = addBtn ("hq", "HQ", PB::flat, noLed, 750, 734, 44, 20, U ("HQ: oversampling 2x para saturaciones más limpias"));
    juce::ignoreUnused (hq);
    auto* help = addText ("?", juce::Colour (0xff2a2a2c), { 779, 724, 22, 22 }, [this]
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Top Console B",
            U ("Canal de consola estilo serie 4000 B.\n\n"
               "Izquierda: filtros y EQ de 4 bandas (bell = campana).\n"
               "Centro: trim, drive del preamp, ancho, paneo, mono, cut y fader VCA.\n"
               "Derecha: compresor (ratio 'ds' = de-esser, mix paralelo) y gate/expansor.\n"
               "Visor de ruteo: qué alimenta el sidechain y el orden de los procesos.\n\n"
               "Diseño propio de Dj Pava Hm Music."));
    }, 15.0f);
    help->boxed = true; help->setTooltip ("Ayuda");
    auto* init = addText ("INIT", juce::Colour (0xff2a2a2c), { 549, 8, 42, 22 }, [this] { proc.setCurrentProgram (0); }, 11.5f);
    init->boxed = true; init->setTooltip ("Volver al canal limpio");

    buildBackground();
    setResizable (true, true);
    setResizeLimits (W * 3 / 4, H * 3 / 4, W * 2, H * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) W / H);
    setSize (W, H);
    startTimerHz (30);
}

TopConsoleBEditor::~TopConsoleBEditor()
{
    stopTimer();
    tips.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void TopConsoleBEditor::switchAB (int slot)
{
    if (slot == proc.abSlot) return;
    proc.abStore[proc.abSlot] = proc.apvts.copyState();
    if (proc.abStore[slot].isValid()) proc.apvts.replaceState (proc.abStore[slot].createCopy());
    proc.abSlot = slot;
}
void TopConsoleBEditor::stepPreset (int d)
{
    const int n = proc.getNumPrograms();
    proc.setCurrentProgram ((proc.getCurrentProgram() + d + n) % n);
}

void TopConsoleBEditor::buildBackground()
{
    background = juce::Image (juce::Image::ARGB, W * 2, H * 2, true);
    juce::Graphics g (background);
    g.addTransform (juce::AffineTransform::scale (2.0f));
    const float fw = (float) W, fh = (float) H;
    auto panel = [&g] (juce::Rectangle<float> r, juce::Colour a, juce::Colour b)
    {
        g.setGradientFill (juce::ColourGradient (a, r.getX(), r.getY(), b, r.getX(), r.getBottom(), false));
        g.fillRect (r);
    };
    // chasis por secciones
    panel ({ 0, 0, fw, fh }, juce::Colour (0xff222326), juce::Colour (0xff1c1d20));
    panel ({ 0, 0, 202, 715 }, juce::Colour (0xff232427), juce::Colour (0xff1d1e21));
    panel ({ 608, 0, 204, 715 }, juce::Colour (0xff232427), juce::Colour (0xff1d1e21));
    panel ({ 202, 0, 406, 45 }, juce::Colour (0xff1b1c1e), juce::Colour (0xff18191b));
    panel ({ 202, 45, 101, 670 }, juce::Colour (0xff1a1b1d), juce::Colour (0xff17181a));
    panel ({ 507, 45, 101, 670 }, juce::Colour (0xff1a1b1d), juce::Colour (0xff17181a));
    panel ({ 303, 45, 204, 189 }, juce::Colour (0xff222326), juce::Colour (0xff1f2023));
    panel ({ 303, 234, 204, 481 }, juce::Colour (0xff303236), juce::Colour (0xff27292c));
    g.setColour (juce::Colours::white.withAlpha (0.08f)); g.drawHorizontalLine (234, 303, 507);
    g.setColour (juce::Colours::black.withAlpha (0.5f)); g.drawHorizontalLine (233, 303, 507);
    // textura fina
    juce::Random rng (4000);
    for (int i = 0; i < 26000; ++i)
    {
        g.setColour ((rng.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (0.025f));
        g.fillRect (rng.nextFloat() * fw, rng.nextFloat() * fh, 0.5f, 0.5f);
    }
    // sombras interiores de las zonas de medidores y barra superior
    auto innerShadow = [&g] (juce::Rectangle<float> r)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.45f), r.getX(), r.getY(), juce::Colours::transparentBlack, r.getX(), r.getY() + 10, false));
        g.fillRect (r.withHeight (10));
    };
    innerShadow ({ 202, 45, 101, 670 }); innerShadow ({ 507, 45, 101, 670 }); innerShadow ({ 303, 45, 204, 189 });
    // divisores verticales
    for (float x : { 202.0f, 303.0f, 507.0f, 608.0f })
    {
        g.setColour (juce::Colours::black.withAlpha (0.65f)); g.fillRect (x - 1.0f, 0.0f, 1.5f, 715.0f);
        g.setColour (juce::Colours::white.withAlpha (0.05f)); g.fillRect (x + 0.5f, 0.0f, 0.8f, 715.0f);
    }
    g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawHorizontalLine (45, 202, 608);
    // barra inferior
    panel ({ 0, 715, fw, 37 }, juce::Colour (0xff141517), juce::Colour (0xff111213));
    g.setColour (juce::Colours::black); g.fillRect (0.0f, 714.0f, fw, 1.5f);
    g.setColour (juce::Colours::white.withAlpha (0.04f)); g.drawHorizontalLine (716, 0, fw);
    for (float x : { 202.0f, 608.0f }) { g.setColour (juce::Colours::black.withAlpha (0.8f)); g.fillRect (x - 1.0f, 715.0f, 1.5f, 37.0f); }
    text (g, "v1.0", 712, 735, 11.0f, juce::Colour (0xff6e6f72), true);

    // ---- escalas serigrafiadas de las perillas
    for (const auto& s : scales)
    {
        auto* prm = proc.apvts.getParameter (s.id);
        const float rd = s.r + 4.5f;
        for (int i = 0; i <= 10; ++i)
        {
            const float a = A0 + (A1 - A0) * (float) i / 10.0f;
            g.setColour (inkDim.withAlpha (0.85f));
            g.fillEllipse (s.cx + rd * std::sin (a) - 0.9f, s.cy - rd * std::cos (a) - 0.9f, 1.8f, 1.8f);
        }
        for (const auto& [v, t] : s.labels)
        {
            const float a = A0 + (A1 - A0) * prm->convertTo0to1 (v);
            const float rl = s.r + 12.5f;
            text (g, t, s.cx + rl * std::sin (a), s.cy - rl * std::cos (a), 10.5f, ink);
        }
        if (s.unit == "dB+-")
        {
            text (g, U ("−"), s.cx - 8, s.cy + s.r + 10, 12.0f, ink, true);
            text (g, "+", s.cx + 9, s.cy + s.r + 10, 12.0f, ink, true);
            text (g, "dB", s.cx, s.cy + s.r + 21, 10.5f, ink);
        }
        else if (s.unit.isNotEmpty()) text (g, s.unit, s.cx, s.cy + s.r + 18, 10.5f, ink);
    }

    // ---- columna izquierda
    text (g, "low pass", 56, 100, 11.5f);
    text (g, "high pass", 146, 46, 11.5f);
    text (g, "FILTERS", 12, 139, 12.5f, ink, false, juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xff595b5f)); g.fillRect (0.0f, 151.0f, 202.0f, 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.4f)); g.fillRect (0.0f, 153.0f, 202.0f, 1.0f);
    text (g, "bell", 157, 166, 11.5f);
    text (g, "bell", 157, 702, 11.5f);
    text (g, "HF", 12, 241, 12.5f, ink, false, juce::Justification::centredLeft);
    text (g, "HMF", 12, 344, 12.5f, ink, false, juce::Justification::centredLeft);
    text (g, "LMF", 12, 525, 12.5f, ink, false, juce::Justification::centredLeft);
    text (g, "LF", 12, 634, 12.5f, ink, false, juce::Justification::centredLeft);
    text (g, "EQ", 166, 431, 15.0f, ink, false, juce::Justification::centredLeft);
    auto engrave = [&g] (std::initializer_list<juce::Point<float>> pts)
    {
        juce::Path p; bool first = true;
        for (auto pt : pts) { if (first) p.startNewSubPath (pt); else p.lineTo (pt); first = false; }
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.strokePath (p, juce::PathStrokeType (1.2f), juce::AffineTransform::translation (0, 1));
        g.setColour (juce::Colour (0xff66686c)); g.strokePath (p, juce::PathStrokeType (1.1f));
    };
    engrave ({ { 12, 252 }, { 82, 252 }, { 112, 286 }, { 187, 286 } });
    engrave ({ { 12, 431 }, { 127, 431 } });
    engrave ({ { 12, 613 }, { 82, 613 }, { 112, 579 }, { 187, 579 } });

    // ---- centro
    text (g, "INPUT", 253, 68, 12.0f);
    text (g, "OUTPUT", 556, 68, 12.0f);
    for (int i = 0; i <= 10; ++i)
    {
        const float y = 86.0f + (float) i * 60.8f;
        const auto c = i <= 2 ? juce::Colour (0xffe8402c) : juce::Colour (0xff4ccf5a);
        text (g, juce::String (i * 6), 224, y, 10.0f, c, true);
        text (g, juce::String (i * 6), 589, y, 10.0f, c, true);
    }
    text (g, "in trim", 334, 58, 11.5f);
    text (g, U ("Ø"), 468, 94, 13.0f, ink, false, juce::Justification::centredLeft);
    text (g, "drive", 330, 148, 11.5f);
    text (g, "pre", 462, 182, 11.5f, ink, false, juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xff66686c)); g.fillRect (402.0f, 181.5f, 22.0f, 1.2f);
    // piloto
    g.setColour (juce::Colours::black.withAlpha (0.8f)); g.fillEllipse (409, 141, 12, 12);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3c40), 413, 144, juce::Colour (0xff0e0e10), 418, 151, true)); g.fillEllipse (410, 142, 10, 10);
    text (g, "width", 366, 313, 11.5f);
    text (g, "pan", 444, 313, 11.5f);
    text (g, "attack", 366, 505, 11.5f);
    text (g, "comp", 352, 523, 11.0f, ink, false, juce::Justification::centredRight);
    text (g, "gate", 352, 553, 11.0f, ink, false, juce::Justification::centredRight);
    text (g, "listen", 366, 600, 11.5f);
    text (g, "s/c", 352, 620, 11.0f, ink, false, juce::Justification::centredRight);
    text (g, "out trim", 366, 664, 11.5f);
    // ranura del fader y escala
    {
        const float x = 444, y0 = 343, y1 = 691;
        g.setColour (juce::Colours::black.withAlpha (0.4f)); g.fillRoundedRectangle (x - 4.0f, y0 - 1.0f, 8.0f, y1 - y0 + 2.0f, 4.0f);
        g.setColour (juce::Colour (0xff060607)); g.fillRoundedRectangle (x - 3.0f, y0, 6.0f, y1 - y0, 3.0f);
        g.setColour (juce::Colours::white.withAlpha (0.07f)); g.drawRoundedRectangle (x - 3.5f, y0 + 0.5f, 7.0f, y1 - y0, 3.5f, 0.8f);
        const char* lab[] { "12", "6", "0", "5", "10", "20", "30", "40", "50", "\xe2\x88\x9e" };
        for (int i = 0; i < 10; ++i)
        {
            const float y = tcbui::Fader::top + (float) i * tcbui::Fader::step;
            g.setColour (ink); g.fillEllipse (473.0f, y - 1.0f, 2.0f, 2.0f);
            text (g, U (lab[i]), 478, y, i == 9 ? 14.0f : 11.0f, ink, false, juce::Justification::centredLeft);
        }
    }

    // ---- derecha
    text (g, "COMPRESSOR", 620, 21, 12.5f, ink, false, juce::Justification::centredLeft);
    text (g, "mix", 777, 21, 11.5f, ink, false, juce::Justification::centredLeft);
    text (g, "ratio", 638, 52, 11.5f);
    text (g, "threshold", 754, 100, 11.5f);
    text (g, "release", 640, 153, 11.5f);
    text (g, "DYN", 629, 266, 16.0f);
    g.setColour (juce::Colour (0xff66686c)); g.fillRect (680.0f, 265.5f, 44.0f, 1.2f);
    {
        juce::Path br; br.startNewSubPath (730, 226); br.lineTo (724, 226); br.lineTo (724, 309); br.lineTo (730, 309);
        br.startNewSubPath (778, 226); br.lineTo (784, 226); br.lineTo (784, 309); br.lineTo (778, 309);
        g.setColour (juce::Colour (0xff66686c)); g.strokePath (br, juce::PathStrokeType (1.1f));
        const float ys[] { 235, 251, 267, 283, 298 }; const char* t[] { "20", "14", "10", "6", "3" };
        for (int i = 0; i < 5; ++i) text (g, t[i], 754, ys[i], 10.5f);
        text (g, "atten", 769, 220, 11.0f);
        text (g, "dB", 737, 316, 10.5f);
    }
    text (g, "range", 638, 306, 11.5f);
    text (g, "threshold", 754, 356, 11.5f);
    text (g, "release", 640, 404, 11.5f);
    text (g, "exp", 765, 482, 11.5f);
    text (g, "GATE / EXPANDER", 620, 509, 12.5f, ink, false, juce::Justification::centredLeft);
    // placa de marca (propia)
    {
        const auto pr = juce::Rectangle<float> (614, 533, 193, 21);
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (pr.translated (0, 1), 3);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2b2e), pr.getX(), pr.getY(), juce::Colour (0xff1c1d1f), pr.getX(), pr.getBottom(), false));
        g.fillRoundedRectangle (pr, 3);
        g.setColour (juce::Colour (0xff9fa0a3)); g.drawRoundedRectangle (pr.reduced (0.6f), 3, 1.2f);
        g.setColour (ink); g.setFont (font (12.0f).withExtraKerningFactor (0.03f));
        g.drawFittedText (U ("TOP CONSOLE B • DJ PAVA HM"), pr.reduced (6, 0).toNearestInt(), juce::Justification::centred, 1, 0.8f);
    }
}

void TopConsoleBEditor::resized()
{
    const float s = (float) getWidth() / (float) W;
    const auto T = juce::AffineTransform::scale (s);
    for (auto& [c, r] : layout) { c->setBounds (r); c->setTransform (T); }
}

void TopConsoleBEditor::paint (juce::Graphics& g)
{
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImageTransformed (background, juce::AffineTransform::scale ((float) getWidth() / (float) (W * 2)));
}

void TopConsoleBEditor::timerCallback()
{
    inMeter->tick(); outMeter->tick(); atten->tick();
    routing->repaint(); presetTop->repaint();
    const auto name = proc.getProgramName (proc.getCurrentProgram()).toUpperCase();
    if (bottomName->text != name) { bottomName->text = name; bottomName->repaint(); }
    for (int i = 0; i < 2; ++i) if (abBtn[i]->active != (proc.abSlot == i)) { abBtn[i]->active = proc.abSlot == i; abBtn[i]->repaint(); }
    if (++transactionTick >= 15)
    {
        transactionTick = 0;
        if (! juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown()) proc.undoManager.beginNewTransaction();
    }
}
