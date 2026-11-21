#include "TopConsoleBEditor.h"

using namespace tcbui;

namespace
{
    juce::String U (const char* s) { return juce::String::fromUTF8 (s); }
    const juce::Colour ink (0xffe9e6df), inkDim (0xff9c998f), panelA (0xff2a2b2e), panelB (0xff1d1e20), frame (0xff0e0e0f);
    const juce::Colour capRed (0xffc8342c), capGreen (0xff2e9a55), capBlue (0xff2d63c4), capBrown (0xff4a3426), capWhite (0xffd9d8d2),
                       capGrey (0xff8d9198), capOrange (0xffe07a2a), capYellow (0xffd9b43a), capBlack (0xff2a2a2c);
    juce::Font font (float h, bool bold = false) { return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain)); }
    juce::Font plate (float h) { return font (h, true).withExtraKerningFactor (0.08f); }
    void screw (juce::Graphics& g, float x, float y)
    {
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillEllipse (x - 5, y - 4, 11, 11);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd6d9dd), x - 3, y - 3, juce::Colour (0xff5b5f66), x + 4, y + 4, false));
        g.fillEllipse (x - 5, y - 5, 10, 10);
        g.setColour (juce::Colour (0xff35383d)); g.drawLine (x - 3.5f, y - 1, x + 3.5f, y + 1, 1.4f);
    }
}

//==============================================================================
Look::Look()
{
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff151618));
    setColour (juce::ComboBox::textColourId, ink);
    setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff3a3c40));
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1b1c1f));
    setColour (juce::PopupMenu::textColourId, ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, capOrange.withAlpha (0.35f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff1b1c1f));
    setColour (juce::TooltipWindow::textColourId, ink);
    setColour (juce::TooltipWindow::outlineColourId, juce::Colour (0xff3a3c40));
    setColour (juce::BubbleComponent::backgroundColourId, juce::Colour (0xff1b1c1f));
    setColour (juce::BubbleComponent::outlineColourId, juce::Colour (0xff3a3c40));
    setColour (juce::Slider::textBoxTextColourId, ink);
}

void Look::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const auto cap = juce::Colour ((juce::uint32) (juce::int64) s.getProperties().getWithDefault ("cap", (juce::int64) 0xff8d9198));
    const int ticks = (int) s.getProperties().getWithDefault ("ticks", 11);
    const juce::Point<float> c ((float) x + (float) w * 0.5f, (float) y + (float) h * 0.5f);
    const float R = (float) juce::jmin (w, h) * 0.5f - 9.0f;
    const float a = a0 + pos * (a1 - a0);
    // escala
    for (int i = 0; i < ticks; ++i)
    {
        const float t = a0 + (a1 - a0) * (float) i / (float) (ticks - 1);
        const bool major = i == 0 || i == ticks - 1 || i == ticks / 2;
        g.setColour (ink.withAlpha (major ? 0.9f : 0.55f));
        const float r0 = R + 3.0f, r1 = R + (major ? 8.0f : 6.0f);
        g.drawLine (juce::Line<float> ({ c.x + r0 * std::sin (t), c.y - r0 * std::cos (t) }, { c.x + r1 * std::sin (t), c.y - r1 * std::cos (t) }), major ? 1.6f : 1.1f);
    }
    // sombra
    for (int i = 4; i > 0; --i)
    {
        const float e = (float) i * 1.6f;
        g.setColour (juce::Colours::black.withAlpha (0.11f));
        g.fillEllipse (c.x - R - e + 2.0f, c.y - R - e + 4.0f, (R + e) * 2, (R + e) * 2);
    }
    // faldon moleteado
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4c51), c.x - R, c.y - R, juce::Colour (0xff0d0d0e), c.x + R, c.y + R, false));
    g.fillEllipse (c.x - R, c.y - R, R * 2, R * 2);
    for (int i = 0; i < 60; ++i)
    {
        const float t = (float) i / 60.0f * juce::MathConstants<float>::twoPi + a;
        const float lit = 0.5f + 0.5f * std::cos (t + 0.8f);
        g.setColour ((i % 2 == 0 ? juce::Colours::white : juce::Colours::black).withAlpha (i % 2 == 0 ? 0.05f + 0.12f * lit : 0.35f));
        g.drawLine (juce::Line<float> ({ c.x + (R - 0.5f) * std::sin (t), c.y - (R - 0.5f) * std::cos (t) }, { c.x + (R - 4.0f) * std::sin (t), c.y - (R - 4.0f) * std::cos (t) }), 1.0f);
    }
    g.setColour (juce::Colours::black); g.drawEllipse (c.x - R, c.y - R, R * 2, R * 2, 1.0f);
    // cuerpo
    const float rb = R - 4.5f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3b3d42), c.x - rb * 0.5f, c.y - rb, juce::Colour (0xff121214), c.x + rb * 0.4f, c.y + rb, false));
    g.fillEllipse (c.x - rb, c.y - rb, rb * 2, rb * 2);
    // tapa de color
    const float rc = rb * 0.72f;
    juce::ColourGradient cg (cap.brighter (0.45f), c.x - rc * 0.45f, c.y - rc * 0.55f, cap.darker (0.45f), c.x + rc * 0.7f, c.y + rc * 0.8f, true);
    cg.addColour (0.5, cap);
    g.setGradientFill (cg); g.fillEllipse (c.x - rc, c.y - rc, rc * 2, rc * 2);
    g.setColour (juce::Colours::black.withAlpha (0.55f)); g.drawEllipse (c.x - rc, c.y - rc, rc * 2, rc * 2, 1.0f);
    // brillo
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.42f), c.x - rc * 0.3f, c.y - rc * 0.8f, juce::Colours::transparentWhite, c.x - rc * 0.1f, c.y - rc * 0.05f, false));
    g.fillEllipse (c.x - rc * 0.72f, c.y - rc * 0.88f, rc * 1.3f, rc * 0.85f);
    // indicador
    const bool lightCap = cap.getPerceivedBrightness() > 0.6f;
    g.setColour (lightCap ? juce::Colour (0xff121212) : juce::Colours::white);
    g.drawLine (juce::Line<float> ({ c.x + rc * 0.15f * std::sin (a), c.y - rc * 0.15f * std::cos (a) }, { c.x + (rb - 1.0f) * std::sin (a), c.y - (rb - 1.0f) * std::cos (a) }), juce::jmax (2.0f, R * 0.09f));
}

void Look::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider& s)
{
    // fader largo: ranura, escala y tapa
    const float cx = (float) x + (float) w * 0.6f;
    const auto slot = juce::Rectangle<float> (cx - 4, (float) y, 8, (float) h);
    g.setColour (juce::Colour (0xff050506)); g.fillRoundedRectangle (slot, 4);
    g.setColour (juce::Colours::white.withAlpha (0.08f)); g.drawRoundedRectangle (slot.translated (0, 1), 4, 1.0f);
    const float marks[] { 12, 6, 0, -5, -10, -20, -30, -40, -60 };
    g.setFont (font (11.0f, true));
    for (float m : marks)
    {
        const float yy = (float) y + (float) h * (1.0f - (float) s.valueToProportionOfLength (m));
        g.setColour (ink.withAlpha (m == 0 ? 0.95f : 0.6f));
        g.drawLine (cx - 34, yy, cx - 12, yy, m == 0 ? 1.6f : 1.0f);
        g.drawLine (cx + 12, yy, cx + 22, yy, m == 0 ? 1.6f : 1.0f);
        g.drawText (m <= -59 ? U ("∞") : (m > 0 ? "+" : "") + juce::String ((int) m), (int) cx - 64, (int) yy - 7, 28, 14, juce::Justification::centredRight);
    }
    // tapa
    const float cy = pos;
    const auto capR = juce::Rectangle<float> (cx - 22, cy - 34, 44, 68);
    for (int i = 4; i > 0; --i) { g.setColour (juce::Colours::black.withAlpha (0.12f)); g.fillRoundedRectangle (capR.expanded ((float) i * 1.5f).translated (2, 5), 6); }
    juce::ColourGradient fg (juce::Colour (0xff6b6e74), capR.getX(), capR.getY(), juce::Colour (0xff1c1d20), capR.getX(), capR.getBottom(), false);
    fg.addColour (0.48, juce::Colour (0xff3a3c40)); fg.addColour (0.52, juce::Colour (0xff2a2b2e));
    g.setGradientFill (fg); g.fillRoundedRectangle (capR, 5);
    for (int i = 0; i < 6; ++i)
    {
        const float yy = capR.getY() + 8 + (float) i * 4.0f;
        g.setColour (juce::Colours::white.withAlpha (0.10f)); g.drawHorizontalLine ((int) yy, capR.getX() + 4, capR.getRight() - 4);
        g.setColour (juce::Colours::black.withAlpha (0.4f)); g.drawHorizontalLine ((int) yy + 1, capR.getX() + 4, capR.getRight() - 4);
        g.setColour (juce::Colours::white.withAlpha (0.10f)); g.drawHorizontalLine ((int) (capR.getBottom() - 8 - i * 4.0f), capR.getX() + 4, capR.getRight() - 4);
    }
    g.setColour (juce::Colours::white); g.fillRect (capR.getX() + 3, cy - 1.2f, capR.getWidth() - 6, 2.4f);
    g.setColour (juce::Colours::black); g.drawRoundedRectangle (capR, 5, 1.0f);
}

void Look::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (juce::Colour (0xff101113)); g.fillRoundedRectangle (r, 4);
    g.setColour (juce::Colour (0xff3a3c40)); g.drawRoundedRectangle (r, 4, 1.0f);
    juce::Path a; const float ax = (float) w - 14, ay = (float) h * 0.5f;
    a.addTriangle (ax - 4.5f, ay - 2.0f, ax + 4.5f, ay - 2.0f, ax, ay + 3.5f);
    g.setColour (inkDim); g.fillPath (a);
}
juce::Font Look::getComboBoxFont (juce::ComboBox&) { return font (13.0f, true); }

//==============================================================================
Knob::Knob (APVTS& s, const juce::String& id, const juce::String& t, juce::Colour cap, float radius, const juce::String& a, const juce::String& b, int ticks)
    : title (t), minT (a), maxT (b)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setPopupDisplayEnabled (true, true, nullptr);
    slider.getProperties().set ("cap", (juce::int64) cap.getARGB());
    slider.getProperties().set ("ticks", ticks);
    slider.setMouseDragSensitivity (220);
    addAndMakeVisible (slider);
    att = std::make_unique<APVTS::SliderAttachment> (s, id, slider);
    if (auto* p = s.getParameter (id)) slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    juce::ignoreUnused (radius);
}
void Knob::resized() { slider.setBounds (getLocalBounds().withTrimmedTop (14).withTrimmedBottom (6).reduced (12, 0)); }
void Knob::paint (juce::Graphics& g)
{
    g.setColour (ink); g.setFont (plate (10.5f));
    g.drawText (title, 0, 0, getWidth(), 13, juce::Justification::centred);
    g.setColour (inkDim); g.setFont (font (9.5f, true));
    const auto b = slider.getBounds();
    g.drawText (minT, 1, b.getBottom() - 12, getWidth() / 2, 12, juce::Justification::centredLeft);
    g.drawText (maxT, getWidth() / 2, b.getBottom() - 12, getWidth() / 2 - 1, 12, juce::Justification::centredRight);
}

//==============================================================================
LightButton::LightButton (APVTS& s, const juce::String& id, const juce::String& label, juce::Colour l) : juce::Button (label), led (l)
{
    setClickingTogglesState (true);
    att = std::make_unique<APVTS::ButtonAttachment> (s, id, *this);
}
void LightButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (1.5f);
    const bool on = getToggleState();
    // bisel metalico
    g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (r.translated (0, 2), 4);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff55585e), r.getX(), r.getY(), juce::Colour (0xff1a1b1d), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 4);
    auto capR = r.reduced (3.0f).translated (0, down ? 1.0f : 0.0f);
    if (on)
    {
        g.setColour (led.withAlpha (0.35f)); g.fillRoundedRectangle (capR.expanded (3), 5);
        g.setGradientFill (juce::ColourGradient (led.brighter (0.6f), capR.getCentreX(), capR.getY(), led.darker (0.2f), capR.getCentreX(), capR.getBottom(), false));
    }
    else g.setGradientFill (juce::ColourGradient (juce::Colour (over ? 0xff3e4045 : 0xff34363a), capR.getCentreX(), capR.getY(), juce::Colour (0xff1e1f22), capR.getCentreX(), capR.getBottom(), false));
    g.fillRoundedRectangle (capR, 3);
    g.setColour (juce::Colours::white.withAlpha (on ? 0.35f : 0.1f)); g.fillRoundedRectangle (capR.withHeight (capR.getHeight() * 0.4f).reduced (2, 1), 2);
    g.setColour (on ? juce::Colour (0xff141414) : ink.withAlpha (0.85f)); g.setFont (plate (10.5f));
    g.drawFittedText (getButtonText(), capR.toNearestInt(), juce::Justification::centred, 1, 0.7f);
}

//==============================================================================
LedMeter::LedMeter (std::function<float()> p, std::function<float()> r, bool reduction) : peak (std::move (p)), rms (std::move (r)), gr (reduction) {}
void LedMeter::tick()
{
    const float p = peak(), r = rms ? rms() : -100.0f;
    pk = p > pk ? p : (gr ? pk + (p - pk) * 0.25f : pk - 1.2f);
    rm = r > rm ? r : rm - 0.8f;
    if (p > hold) { hold = p; holdT = 40; } else if (--holdT < 0) hold -= 1.0f;
    repaint();
}
void LedMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff050506)); g.fillRoundedRectangle (r, 3);
    g.setColour (juce::Colours::white.withAlpha (0.08f)); g.drawRoundedRectangle (r.reduced (0.5f), 3, 1.0f);
    const int segs = 24;
    const auto in = r.reduced (3.0f);
    const float sh = in.getHeight() / (float) segs;
    const int cols = rms ? 2 : 1;
    for (int c = 0; c < cols; ++c)
    {
        const float colW = (in.getWidth() - (float) (cols - 1) * 2.0f) / (float) cols;
        const float x = in.getX() + (float) c * (colW + 2.0f);
        const float v = c == 0 ? pk : rm;
        for (int i = 0; i < segs; ++i)
        {
            const float frac = (float) i / (float) (segs - 1);
            bool lit; juce::Colour col;
            if (gr)
            {
                const float db = frac * 20.0f;                // de arriba hacia abajo
                lit = v > db + 0.25f;
                col = juce::Colour (0xffffb21f);
                const float yy = in.getY() + (float) i * sh;
                g.setColour (lit ? col : col.withAlpha (0.10f));
                g.fillRoundedRectangle (x, yy + 1, colW, sh - 2, 1.5f);
                continue;
            }
            const float db = -60.0f + frac * 66.0f;
            lit = v >= db || (c == 0 && std::abs (hold - db) < 1.4f);
            col = db > 0 ? juce::Colour (0xffff3b30) : db > -9 ? juce::Colour (0xffffc23a) : juce::Colour (0xff3ee07a);
            const float yy = in.getBottom() - (float) (i + 1) * sh;
            if (lit) { g.setColour (col.withAlpha (0.35f)); g.fillRoundedRectangle (x - 1, yy, colW + 2, sh, 2); }
            g.setColour (lit ? col : col.withAlpha (0.09f));
            g.fillRoundedRectangle (x, yy + 1, colW, sh - 2, 1.5f);
        }
    }
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.08f), r.getX(), r.getY(), juce::Colours::transparentWhite, r.getRight(), r.getY() + r.getHeight() * 0.4f, false));
    g.fillRoundedRectangle (r, 3);
}

//==============================================================================
void EqGraph::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0a0b0c)); g.fillRoundedRectangle (r, 6);
    g.setColour (juce::Colours::black); g.drawRoundedRectangle (r, 6, 2.0f);
    const float W = r.getWidth(), H = r.getHeight();
    auto xOf = [W] (float f) { return W * std::log (f / 20.0f) / std::log (1000.0f); };
    auto yOf = [H] (float db) { return H * 0.5f - db / 18.0f * H * 0.42f; };
    g.setFont (font (9.5f));
    for (float f : { 30.f, 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f })
    {
        g.setColour (juce::Colour (0xff1e2124)); g.drawVerticalLine ((int) xOf (f), 4, H - 4);
        g.setColour (juce::Colour (0xff5a5e64));
        g.drawText (f >= 1000 ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f), (int) xOf (f) + 2, (int) H - 14, 30, 12, juce::Justification::centredLeft);
    }
    for (float db : { -12.f, -6.f, 0.f, 6.f, 12.f })
    {
        g.setColour (db == 0 ? juce::Colour (0xff34383d) : juce::Colour (0xff1e2124)); g.drawHorizontalLine ((int) yOf (db), 4, W - 4);
    }
    const auto p = proc.readParams();
    const double sr = 48000.0;
    std::array<tvc::Biquad, 4> eq; tcb::Eq::design (eq, sr, p);
    tvc::Biquad hp2, hp1, lp; hp2.setHighPass (sr, p.hpf, 1.0); hp1.setHighPass1 (sr, p.hpf); lp.setLowPass (sr, std::min ((double) p.lpf, sr * 0.45), 0.707);
    auto mag = [sr] (const tvc::Biquad& b, float hz)
    {
        const double w = 2.0 * juce::MathConstants<double>::pi * hz / sr;
        const std::complex<double> z1 = std::polar (1.0, -w), z2 = z1 * z1;
        return (float) (20.0 * std::log10 (std::abs ((double) b.b0 + (double) b.b1 * z1 + (double) b.b2 * z2) / std::abs (1.0 + (double) b.a1 * z1 + (double) b.a2 * z2) + 1e-12));
    };
    juce::Path curve;
    for (int i = 0; i <= 260; ++i)
    {
        const float f = 20.0f * std::pow (1000.0f, (float) i / 260.0f);
        float db = 0;
        if (p.eqIn && ! p.eqSc) for (auto& b : eq) db += mag (b, f);
        if (p.filtIn && ! p.filtSc) { if (p.hpf > 17.0f) db += mag (hp2, f) + mag (hp1, f); if (p.lpf < 21500.0f) db += mag (lp, f); }
        const float y = juce::jlimit (2.0f, H - 2.0f, yOf (db));
        if (i == 0) curve.startNewSubPath (xOf (f), y); else curve.lineTo (xOf (f), y);
    }
    juce::Path fill (curve); fill.lineTo (W, yOf (0)); fill.lineTo (0, yOf (0)); fill.closeSubPath();
    g.setColour (capOrange.withAlpha (0.12f)); g.fillPath (fill);
    g.setColour (capOrange.withAlpha (0.35f)); g.strokePath (curve, juce::PathStrokeType (5.0f));
    g.setColour (juce::Colour (0xffffb35c)); g.strokePath (curve, juce::PathStrokeType (1.8f));
    // reflejo del cristal
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.05f), 0, 0, juce::Colours::transparentWhite, 0, H * 0.5f, false));
    g.fillRoundedRectangle (r.reduced (2), 5);
}

//==============================================================================
tcbui::Knob* TopConsoleBEditor::add (const juce::String& id, const juce::String& title, juce::Colour cap, float r, const juce::String& a, const juce::String& b, int x, int y, int ticks)
{
    auto* k = knobs.add (new tcbui::Knob (proc.apvts, id, title, cap, r, a, b, ticks));
    const int sz = (int) (r * 2 + 18);
    layout.push_back ({ k, { x - sz / 2 - 22, y - sz / 2 - 14, sz + 44, sz + 20 } });
    addAndMakeVisible (k);
    return k;
}
tcbui::LightButton* TopConsoleBEditor::addBtn (const juce::String& id, const juce::String& label, juce::Colour led, int x, int y, int w)
{
    auto* b = buttons.add (new tcbui::LightButton (proc.apvts, id, label, led));
    layout.push_back ({ b, { x - w / 2, y, w, 26 } });
    addAndMakeVisible (b);
    return b;
}

TopConsoleBEditor::TopConsoleBEditor (TopConsoleBProcessor& p) : AudioProcessorEditor (&p), proc (p), graph (p)
{
    setLookAndFeel (&look);
    tips.setLookAndFeel (&look);
    const juce::Colour ledG (0xff4dff8a), ledR (0xffff4a3c), ledY (0xffffc23a), ledB (0xff5fb6ff);
    // ENTRADA
    add ("in_trim", "TRIM", capGrey, 20, "-20", "+20", 94, 150);
    add ("drive", "DRIVE", capOrange, 27, "0", "MAX", 94, 262)->slider.setTooltip (U ("Preamp con transformador: más armónicos y saturación sin cambiar el volumen"));
    addBtn ("phase", U ("Ø"), ledY, 64, 330, 50);
    addBtn ("hq", "HQ", ledB, 124, 330, 50)->setTooltip (U ("HQ: oversampling para saturaciones más limpias"));
    // FILTROS
    add ("hpf", "HPF", capWhite, 24, "OFF", "350", 257, 300);
    add ("lpf", "LPF", capWhite, 24, "3k", "OFF", 257, 410);
    addBtn ("filt_in", "FILT IN", ledG, 257, 486, 76);
    addBtn ("filt_sc", "S/C", ledY, 257, 520, 76)->setTooltip ("Los filtros solo afectan al sidechain de la dinámica");
    // EQ
    add ("lf_g", "LF", capBrown, 31, "-16", "+16", 406, 300, 13);
    add ("lf_f", "FREQ", capBlack, 22, "30", "450", 406, 410);
    addBtn ("lf_bell", "BELL", ledG, 406, 478, 60);
    add ("lmf_g", "LMF", capBlue, 31, "-16", "+16", 538, 300, 13);
    add ("lmf_f", "FREQ", capBlue, 22, "200", "2.5k", 538, 410);
    add ("lmf_q", "Q", capBlue, 18, "0.5", "3", 538, 500);
    add ("hmf_g", "HMF", capGreen, 31, "-16", "+16", 670, 300, 13);
    add ("hmf_f", "FREQ", capGreen, 22, "600", "7k", 670, 410);
    add ("hmf_q", "Q", capGreen, 18, "0.5", "3", 670, 500);
    add ("hf_g", "HF", capRed, 31, "-16", "+16", 802, 300, 13);
    add ("hf_f", "FREQ", capRed, 22, "1.5k", "16k", 802, 410);
    addBtn ("hf_bell", "BELL", ledG, 802, 478, 60);
    addBtn ("eq_in", "EQ IN", ledG, 406, 590, 76);
    addBtn ("eq_sc", "EQ S/C", ledY, 494, 590, 76)->setTooltip ("El EQ solo afecta al sidechain de la dinámica");
    // DINAMICA
    add ("c_ratio", "RATIO", capGrey, 22, "1", "ds", 935, 174, 10)->slider.setTooltip (U ("Ratio de compresión. La última posición 'ds' convierte el compresor en de-esser"));
    add ("c_thr", "THRESHOLD", capGrey, 26, "-40", "0", 935, 276);
    add ("c_rel", "RELEASE", capGrey, 20, ".1", "4", 935, 374);
    addBtn ("c_fast", "FAST ATK", ledY, 935, 432, 76);
    add ("g_range", "RANGE", capGreen, 20, "0", "40", 1085, 166);
    add ("g_thr", "THRESHOLD", capGreen, 22, "-60", "0", 1085, 254);
    add ("g_rel", "RELEASE", capGreen, 18, ".1", "4", 1085, 336);
    add ("g_hold", "HOLD", capGreen, 18, "0", "4", 1085, 412);
    addBtn ("g_fast", "FAST", ledY, 1058, 470, 50);
    addBtn ("g_exp", "EXP", ledG, 1112, 470, 50)->setTooltip ("Expansor 1:2 en vez de puerta");
    addBtn ("dyn_in", "DYN IN", ledG, 930, 548, 76);
    addBtn ("dyn_pre", "PRE EQ", ledY, 1028, 548, 76)->setTooltip (U ("Dinámica antes del EQ (por defecto va después)"));
    addBtn ("ext_sc", "EXT S/C", ledR, 1126, 548, 76)->setTooltip ("Sidechain externo (otra pista)");
    // SALIDA
    add ("out_trim", "OUT TRIM", capGrey, 20, "-20", "+20", 1250, 586);
    addBtn ("cut", "CUT", ledR, 1330, 600, 50);
    fader.setSliderStyle (juce::Slider::LinearVertical);
    fader.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    fader.setPopupDisplayEnabled (true, true, nullptr);
    fader.setTooltip (U ("Fader VCA: por encima de 0 dB añade la saturación sutil del VCA"));
    faderAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "fader", fader);
    fader.setDoubleClickReturnValue (true, 0.0);
    layout.push_back ({ &fader, { 1196, 108, 110, 432 } });
    addAndMakeVisible (fader);
    addAndMakeVisible (graph);
    layout.push_back ({ &graph, { 192, 104, 668, 132 } });

    inMeter = std::make_unique<tcbui::LedMeter> ([this] { return tcb::g2dB (proc.inPeak.load()); }, [this] { return tcb::g2dB (proc.inRms.load()); }, false);
    outMeter = std::make_unique<tcbui::LedMeter> ([this] { return tcb::g2dB (proc.outPeak.load()); }, [this] { return tcb::g2dB (proc.outRms.load()); }, false);
    compMeter = std::make_unique<tcbui::LedMeter> ([this] { return proc.compGr(); }, nullptr, true);
    gateMeter = std::make_unique<tcbui::LedMeter> ([this] { return proc.gateGr(); }, nullptr, true);
    layout.push_back ({ inMeter.get(), { 74, 384, 40, 234 } });
    layout.push_back ({ outMeter.get(), { 1316, 112, 40, 424 } });
    layout.push_back ({ compMeter.get(), { 1004, 124, 16, 336 } });
    layout.push_back ({ gateMeter.get(), { 1150, 124, 16, 336 } });
    for (auto* m : { inMeter.get(), outMeter.get(), compMeter.get(), gateMeter.get() }) addAndMakeVisible (m);

    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { if (presetBox.getSelectedId() > 0) proc.setCurrentProgram (presetBox.getSelectedId() - 1); };
    layout.push_back ({ &presetBox, { 1080, 16, 220, 28 } });
    addAndMakeVisible (presetBox);

    buildBackground();
    setResizable (true, true);
    setResizeLimits (W * 2 / 3, H * 2 / 3, W * 3 / 2, H * 3 / 2);
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

void TopConsoleBEditor::buildBackground()
{
    background = juce::Image (juce::Image::ARGB, W, H, true);
    juce::Graphics g (background);
    // chasis
    g.setGradientFill (juce::ColourGradient (panelA, 0, 0, panelB, 0, (float) H, false));
    g.fillAll();
    juce::Random rng (4000);
    for (int y = 0; y < H; ++y)
    {
        g.setColour ((rng.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (0.012f + 0.018f * rng.nextFloat()));
        g.drawHorizontalLine (y, 0, (float) W);
    }
    // placa superior
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff111214), 0, 0, juce::Colour (0xff1b1c1f), 0, 58, false));
    g.fillRect (0, 0, W, 58);
    g.setColour (juce::Colour (0xff3b3d42)); g.drawHorizontalLine (58, 0, (float) W);
    g.setColour (juce::Colours::black); g.drawHorizontalLine (59, 0, (float) W);
    g.setColour (ink); g.setFont (plate (22.0f));
    g.drawText ("TOP CONSOLE", 54, 10, 220, 24, juce::Justification::centredLeft);
    g.setColour (capOrange); g.setFont (plate (13.0f));
    g.drawText ("B-SERIES CHANNEL", 56, 33, 220, 16, juce::Justification::centredLeft);
    // logo: tres franjas de colores
    const juce::Colour lc[] { capRed, capGreen, capBlue };
    for (int i = 0; i < 3; ++i) { g.setColour (lc[i]); g.fillRoundedRectangle (18.0f, 14.0f + i * 10.0f, 26.0f, 6.0f, 2.0f); }
    g.setColour (inkDim); g.setFont (font (11.0f, true));
    g.drawText ("by Dj Pava Hm Music", 270, 22, 200, 16, juce::Justification::centredLeft);
    g.drawText ("PRESET", 1010, 16, 64, 28, juce::Justification::centredRight);

    // secciones grabadas
    struct Sec { int x, w; const char* t; };
    const Sec secs[] { { 14, 160, "INPUT" }, { 182, 150, "FILTERS" }, { 340, 530, "EQUALISER" }, { 878, 300, "DYNAMICS" }, { 1186, 180, "OUTPUT" } };
    for (const auto& s : secs)
    {
        const auto r = juce::Rectangle<float> ((float) s.x, 68.0f, (float) s.w, (float) H - 80.0f);
        g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (r.translated (0, 2), 6);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff27282b), r.getX(), r.getY(), juce::Colour (0xff1f2023), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 6);
        g.setColour (juce::Colours::white.withAlpha (0.06f)); g.drawRoundedRectangle (r.reduced (0.5f), 6, 1.0f);
        g.setColour (frame); g.fillRoundedRectangle (r.getX() + 6, r.getY() + 6, r.getWidth() - 12, 20, 3);
        g.setColour (ink); g.setFont (plate (11.5f));
        g.drawText (s.t, (int) r.getX(), (int) r.getY() + 6, (int) r.getWidth(), 20, juce::Justification::centred);
        screw (g, r.getX() + 10, r.getBottom() - 10); screw (g, r.getRight() - 10, r.getBottom() - 10);
    }
    // franjas de color de las bandas del EQ
    const int bx[] { 406, 538, 670, 802 };
    const juce::Colour bc[] { capBrown, capBlue, capGreen, capRed };
    for (int i = 0; i < 4; ++i)
    {
        g.setColour (bc[i]); g.fillRoundedRectangle ((float) bx[i] - 50, 246.0f, 100.0f, 4.0f, 2.0f);
        if (i > 0) { g.setColour (juce::Colours::black.withAlpha (0.5f)); g.drawVerticalLine (bx[i] - 66, 250.0f, 560.0f); g.setColour (juce::Colours::white.withAlpha (0.05f)); g.drawVerticalLine (bx[i] - 65, 250.0f, 560.0f); }
    }
    // subtitulos de la dinamica
    g.setColour (juce::Colours::black.withAlpha (0.5f)); g.drawVerticalLine (1028, 100.0f, 520.0f);
    g.setColour (inkDim); g.setFont (plate (10.0f));
    g.drawText ("COMPRESSOR", 884, 98, 140, 14, juce::Justification::centred);
    g.drawText ("GATE / EXP", 1034, 98, 140, 14, juce::Justification::centred);
    g.drawText ("GR", 996, 464, 32, 12, juce::Justification::centred);
    g.drawText ("GR", 1142, 464, 32, 12, juce::Justification::centred);
    g.drawText ("IN", 74, 620, 40, 12, juce::Justification::centred);
    g.drawText ("OUT", 1316, 540, 40, 12, juce::Justification::centred);
    g.drawText ("VCA FADER", 1196, 96, 110, 12, juce::Justification::centred);
}

void TopConsoleBEditor::resized()
{
    const float s = (float) getWidth() / (float) W;
    const auto T = juce::AffineTransform::scale (s);
    for (auto& [c, r] : layout) { c->setBounds (r); c->setTransform (T); }
}

void TopConsoleBEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale ((float) getWidth() / (float) W));
    g.drawImageAt (background, 0, 0);
}

void TopConsoleBEditor::timerCallback()
{
    for (auto* m : { inMeter.get(), outMeter.get(), compMeter.get(), gateMeter.get() }) m->tick();
    graph.repaint();
    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1) presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
}
