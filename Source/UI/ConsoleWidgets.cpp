#include "ConsoleWidgets.h"

namespace cui
{
juce::Font font (float h, bool bold)
{
    return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
}

void panelTexture (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour base, int seed)
{
    g.setGradientFill (juce::ColourGradient (base.brighter (0.05f), r.getX(), r.getY(), base.darker (0.06f), r.getX(), r.getBottom(), false));
    g.fillRect (r);
    // Pintura con leve granulado y desgaste
    juce::Random rnd (seed);
    const int dots = (int) (r.getWidth() * r.getHeight() / 30.0f);
    for (int i = 0; i < dots; ++i)
    {
        g.setColour ((rnd.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (0.015f + rnd.nextFloat() * 0.03f));
        g.fillRect (r.getX() + rnd.nextFloat() * r.getWidth(), r.getY() + rnd.nextFloat() * r.getHeight(), 1.0f, 1.0f);
    }
    for (int i = 0; i < 6; ++i)
    {
        const float cx = r.getX() + rnd.nextFloat() * r.getWidth(), cy = r.getY() + rnd.nextFloat() * r.getHeight();
        const float rad = 20.0f + rnd.nextFloat() * 60.0f;
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.025f), cx, cy, juce::Colours::transparentBlack, cx + rad, cy, true));
        g.fillEllipse (cx - rad, cy - rad, 2 * rad, 2 * rad);
    }
}

void smallScrew (juce::Graphics& g, float cx, float cy, float r)
{
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (cx - r, cy - r + 1, 2 * r, 2 * r);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffc9cbcf), cx - r, cy - r, juce::Colour (0xff6c6f75), cx + r, cy + r, false));
    g.fillEllipse (cx - r, cy - r, 2 * r, 2 * r);
    g.setColour (juce::Colour (0xff2c2d31));
    g.drawLine (cx - r * 0.6f, cy - r * 0.2f, cx + r * 0.6f, cy + r * 0.2f, r * 0.3f);
}

void lcd (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.fillRoundedRectangle (r.expanded (2).translated (0, 1), 4);
    g.setColour (juce::Colour (0xff3a3c40));
    g.fillRoundedRectangle (r.expanded (2), 4);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff100d09), r.getCentreX(), r.getY(), juce::Colour (0xff050403), r.getCentreX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 3);
}

void lcdText (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, float size, bool bold,
              juce::Justification j, juce::Colour c)
{
    g.setFont (font (size, bold));
    g.setColour (c.withAlpha (0.16f));
    for (auto o : { juce::Point<float> (-1, 0), juce::Point<float> (1, 0), juce::Point<float> (0, 1) })
        g.drawText (t, r + o, j, false);
    g.setColour (c);
    g.drawText (t, r, j, false);
}

void moduleFrame (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title, int seed)
{
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (r.translated (0, 3).expanded (1), 5);
    g.saveState();
    juce::Path clip; clip.addRoundedRectangle (r, 4);
    g.reduceClipRegion (clip);
    panelTexture (g, r, Col::panel, seed);
    // Cabecera grafito
    auto head = r.withHeight (36);
    g.setGradientFill (juce::ColourGradient (Col::header.brighter (0.15f), 0, head.getY(), Col::header, 0, head.getBottom(), false));
    g.fillRect (head);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRect (head.getX(), head.getBottom(), head.getWidth(), 1.5f);
    g.restoreState();
    g.setFont (font (15.0f, true));
    g.setColour (Col::cream);
    g.drawText (title, head.reduced (12, 0), juce::Justification::centredLeft, false);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawRoundedRectangle (r, 4, 1.2f);
    smallScrew (g, r.getCentreX(), r.getBottom() - 10, 4.5f);
}

//==============================================================================
LnF::LnF()
{
    setColour (juce::ComboBox::textColourId, Col::amber);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1c1d21));
    setColour (juce::PopupMenu::textColourId, Col::cream);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Col::amber.withAlpha (0.35f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::BubbleComponent::backgroundColourId, juce::Colour (0xee1c1d21));
    setColour (juce::BubbleComponent::outlineColourId, Col::amber.withAlpha (0.6f));
    setColour (juce::Label::textColourId, Col::amber);
}

void LnF::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const auto cap = s.findColour (juce::Slider::rotarySliderFillColourId);
    const float R = juce::jmin ((float) w, (float) h) * 0.5f * 0.60f;
    const float cx = (float) x + (float) w * 0.5f, cy = (float) y + (float) h * 0.54f;
    const float ang = a0 + pos * (a1 - a0);

    // Escala
    for (int i = 0; i <= 10; ++i)
    {
        const float a = a0 + (float) i / 10.0f * (a1 - a0);
        const bool major = i % 5 == 0;
        const float r1 = R * 1.12f, r2 = R * (major ? 1.30f : 1.22f);
        g.setColour (Col::ink.withAlpha (major ? 0.9f : 0.6f));
        g.drawLine (cx + std::sin (a) * r1, cy - std::cos (a) * r1, cx + std::sin (a) * r2, cy - std::cos (a) * r2, major ? 1.5f : 1.0f);
    }
    auto label = [&] (const juce::String& t, float a, float rr)
    {
        if (t.isEmpty()) return;
        const float fs = juce::jlimit (9.0f, 12.0f, R * 0.27f);
        const float lx = cx + std::sin (a) * R * rr, ly = cy - std::cos (a) * R * rr;
        g.setFont (font (fs, true));
        g.setColour (Col::ink);
        g.drawText (t, juce::Rectangle<float> (lx - 30, ly - fs * 0.6f, 60, fs * 1.2f), juce::Justification::centred, false);
    };
    const auto& pr = s.getProperties();
    label (pr["lmin"].toString(), a0, 1.58f);
    label (pr["lmax"].toString(), a1, 1.58f);
    label (pr["lmid"].toString(), (a0 + a1) * 0.5f, 1.48f);

    // Sombra
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (cx - R + 1.5f, cy - R + 3.5f, 2 * R, 2 * R);

    // Faldon gris oscuro estriado
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff55575c), cx, cy - R, juce::Colour (0xff1d1e21), cx, cy + R, false));
    g.fillEllipse (cx - R, cy - R, 2 * R, 2 * R);
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    for (int i = 0; i < 40; ++i)
    {
        const float a = (float) i / 40.0f * juce::MathConstants<float>::twoPi;
        g.drawLine (cx + std::sin (a) * R * 0.84f, cy - std::cos (a) * R * 0.84f, cx + std::sin (a) * R * 0.99f, cy - std::cos (a) * R * 0.99f, 1.2f);
    }
    // Marca del faldon
    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.drawLine (cx + std::sin (ang) * R * 0.84f, cy - std::cos (ang) * R * 0.84f, cx + std::sin (ang) * R * 0.99f, cy - std::cos (ang) * R * 0.99f, 2.0f);

    // Tapa de color
    const float Rc = R * 0.76f;
    g.setGradientFill (juce::ColourGradient (cap.brighter (0.45f), cx - Rc * 0.5f, cy - Rc * 0.6f, cap.darker (0.35f), cx + Rc * 0.6f, cy + Rc * 0.8f, true));
    g.fillEllipse (cx - Rc, cy - Rc, 2 * Rc, 2 * Rc);
    g.setColour (cap.darker (0.6f));
    g.drawEllipse (cx - Rc, cy - Rc, 2 * Rc, 2 * Rc, 1.2f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.35f), cx - Rc * 0.3f, cy - Rc * 0.7f, juce::Colours::transparentWhite, cx, cy, true));
    g.fillEllipse (cx - Rc * 0.75f, cy - Rc * 0.9f, Rc * 1.3f, Rc * 0.9f);

    // Indicador blanco
    juce::Path ptr;
    const float pw = juce::jmax (2.0f, R * 0.08f);
    ptr.addRoundedRectangle (-pw * 0.5f, -Rc * 0.95f, pw, Rc * 0.62f, pw * 0.5f);
    g.setColour (juce::Colours::white);
    g.fillPath (ptr, juce::AffineTransform::rotation (ang).translated (cx, cy));
}

void LnF::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                            juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style != juce::Slider::LinearVertical) { LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0, 0, style, s); return; }
    const float cx = (float) x + (float) w * 0.5f;
    // Ranura
    auto slot = juce::Rectangle<float> (cx - 4, (float) y, 8, (float) h);
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.fillRoundedRectangle (slot, 3);
    g.setColour (juce::Colours::white.withAlpha (0.3f));
    g.drawLine (slot.getRight() + 0.5f, slot.getY() + 3, slot.getRight() + 0.5f, slot.getBottom() - 3, 1.0f);
    // Tapa cromada
    const auto capR = juce::Rectangle<float> (cx - 26, pos - 17, 52, 34);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (capR.translated (2, 4), 4);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff4f5f6), 0, capR.getY(), juce::Colour (0xff8a8d93), 0, capR.getBottom(), false));
    g.fillRoundedRectangle (capR, 4);
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    for (int i = -2; i <= 2; ++i)
        if (i != 0) g.fillRect (capR.getX() + 5, capR.getCentreY() + (float) i * 5.5f - 0.5f, capR.getWidth() - 10, 1.5f);
    g.setColour (Col::ink);
    g.fillRect (capR.getX() + 3, capR.getCentreY() - 1.0f, capR.getWidth() - 6, 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawRoundedRectangle (capR, 4, 1.0f);
}

void LnF::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
    g.setColour (juce::Colour (0xff3a3c40));
    g.fillRoundedRectangle (r, 3);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0d0b08), 0, 0, juce::Colour (0xff1a1815), 0, (float) h, false));
    g.fillRoundedRectangle (r.reduced (1.5f), 2);
    juce::Path tri;
    const float ax = (float) w - 12.0f, ay = (float) h * 0.5f;
    tri.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (Col::amber.withAlpha (0.85f));
    g.fillPath (tri);
}

void LnF::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (5, 0, box.getWidth() - 22, box.getHeight());
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
}

juce::Font LnF::getComboBoxFont (juce::ComboBox& b)          { return font (juce::jmin (14.0f, (float) b.getHeight() * 0.6f), true); }
juce::Font LnF::getPopupMenuFont()                           { return font (15.0f); }
juce::Font LnF::getSliderPopupFont (juce::Slider&)           { return font (14.0f, true); }
juce::Font LnF::getTextButtonFont (juce::TextButton&, int h) { return font (juce::jmin (13.0f, (float) h * 0.5f), true); }

void LnF::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillRoundedRectangle (r.translated (0, 1.5f), 3);
    auto top = juce::Colour (0xfff0eee8), bot = juce::Colour (0xffb7b4ac);
    if (over) { top = top.brighter (0.05f); bot = bot.brighter (0.08f); }
    if (down) std::swap (top, bot);
    g.setGradientFill (juce::ColourGradient (top, 0, r.getY(), bot, 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, 3);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawRoundedRectangle (r, 3, 1.0f);
}

void LnF::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (Col::ink);
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, false);
}

//==============================================================================
Knob::Knob (APVTS& s, const juce::String& id, const juce::String& t, juce::Colour cap,
            const juce::String& minL, const juce::String& midL, const juce::String& maxL)
    : title (t)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setPopupDisplayEnabled (true, true, nullptr);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.22f, juce::MathConstants<float>::pi * 2.78f, true);
    slider.setColour (juce::Slider::rotarySliderFillColourId, cap);
    slider.getProperties().set ("lmin", minL);
    slider.getProperties().set ("lmid", midL);
    slider.getProperties().set ("lmax", maxL);
    addAndMakeVisible (slider);
    att = std::make_unique<APVTS::SliderAttachment> (s, id, slider);
    if (auto* prm = s.getParameter (id))
        slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
}

void Knob::resized() { slider.setBounds (getLocalBounds().withTrimmedBottom (18)); }

void Knob::paint (juce::Graphics& g)
{
    g.setFont (font (juce::jlimit (11.0f, 14.0f, (float) getWidth() * 0.115f), true));
    g.setColour (Col::ink);
    g.drawText (title, getLocalBounds().removeFromBottom (18), juce::Justification::centred, false);
}

//------------------------------------------------------------------------------
void LitButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (1);
    const bool on = getToggleState();
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (r.translated (0, 1.5f), 3);
    g.setColour (juce::Colour (0xff2b2c30));
    g.fillRoundedRectangle (r, 3);
    auto face = r.reduced (2.5f);
    if (on)
    {
        g.setColour (col.withAlpha (0.35f));
        g.fillRoundedRectangle (face.expanded (2), 3);
        g.setGradientFill (juce::ColourGradient (col.brighter (0.6f), face.getCentreX(), face.getCentreY(), col.darker (0.1f), face.getX(), face.getY(), true));
    }
    else
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe9e6de).darker (down ? 0.15f : (over ? 0.03f : 0.0f)), 0, face.getY(),
                                                 juce::Colour (0xffb9b5ab), 0, face.getBottom(), false));
    g.fillRoundedRectangle (face, 2);
    g.setFont (font (juce::jmin (12.5f, face.getHeight() * 0.58f), true));
    g.setColour (on ? juce::Colour (0xff1b1408) : Col::ink);
    g.drawText (getButtonText(), face, juce::Justification::centred, false);
}

//------------------------------------------------------------------------------
float LedMeter::levelToProp (float db) { return juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 54.0f); }   // -48 .. +6 dB

void LedMeter::tick()
{
    const float v = source();
    value = gr ? (v < value ? v : value + 0.8f) : (v > value ? v : value - 1.5f);
    if (gr) value = juce::jmin (0.0f, value);
    if (! gr)
    {
        if (v >= peak) { peak = v; peakHold = 45; }
        else if (--peakHold < 0) peak -= 1.0f;
    }
    repaint();
}

void LedMeter::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff3a3c40));
    g.fillRoundedRectangle (b, 3);
    g.setColour (juce::Colour (0xff0a0a0b));
    auto in = b.reduced (3);
    g.fillRoundedRectangle (in, 2);
    const int n = 30;
    const float segH = (in.getHeight() - (float) (n - 1) * 2.0f) / (float) n;
    const int peakSeg = gr ? -1 : (int) (levelToProp (peak) * (float) n) - 1;
    for (int i = 0; i < n; ++i)   // i = 0 abajo
    {
        const float y = in.getBottom() - (float) (i + 1) * segH - (float) i * 2.0f;
        auto seg = juce::Rectangle<float> (in.getX() + 2, y, in.getWidth() - 4, segH);
        juce::Colour c; bool lit;
        if (gr)
        {
            const int fromTop = n - 1 - i;                       // GR crece desde arriba
            const float thr = (float) (fromTop + 1) * (20.0f / (float) n);
            lit = -value >= thr - 0.0001f;
            c = Col::amber;
        }
        else
        {
            const float prop = (float) (i + 1) / (float) n;
            lit = levelToProp (value) >= prop;
            const float db = prop * 54.0f - 48.0f;
            c = db > 0.0f ? juce::Colour (0xffff3b30) : db > -12.0f ? juce::Colour (0xffffd23a) : Col::ledGreen;
            if (i == peakSeg) lit = true;
        }
        g.setColour (lit ? c : c.withAlpha (0.12f));
        g.fillRoundedRectangle (seg, 1.0f);
    }
}

//------------------------------------------------------------------------------
Fader::Fader (APVTS& s, const juce::String& id)
{
    slider.setSliderStyle (juce::Slider::LinearVertical);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setPopupDisplayEnabled (true, true, nullptr);
    addAndMakeVisible (slider);
    att = std::make_unique<APVTS::SliderAttachment> (s, id, slider);
    if (auto* prm = s.getParameter (id))
        slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
}

void Fader::resized() { slider.setBounds (getLocalBounds().withTrimmedLeft (getWidth() / 2 - 10)); }

void Fader::paint (juce::Graphics& g)
{
    g.setFont (font (12.0f, true));
    for (float v : { 12.0f, 6.0f, 0.0f, -6.0f, -12.0f, -18.0f, -24.0f })
    {
        const float y = (float) slider.getY() + (float) slider.getPositionOfValue (v);
        g.setColour (Col::ink);
        g.drawText ((v > 0 ? "+" : "") + juce::String ((int) v), juce::Rectangle<float> (0, y - 7, (float) slider.getX() - 4, 14),
                    juce::Justification::centredRight, false);
        g.fillRect ((float) slider.getX() + 2, y - 0.5f, (float) slider.getWidth() * 0.5f - 34.0f, 1.0f);
    }
}
} // namespace cui
