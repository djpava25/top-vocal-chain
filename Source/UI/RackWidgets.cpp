#include "RackWidgets.h"

namespace tvcui
{
juce::Font serif (float h, bool bold)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultSerifFontName(), h,
                                          bold ? juce::Font::bold : juce::Font::plain));
}

juce::Font sans (float h, bool bold)
{
    return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
}

// Texto grabado: sombra oscura + texto crema
void engraved (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, float size,
               juce::Justification j, bool bold, juce::Colour col)
{
    g.setFont (serif (size, bold));
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawText (t, r.translated (0.0f, 1.2f), j, false);
    g.setColour (col);
    g.drawText (t, r, j, false);
}

void drawScrew (juce::Graphics& g, float cx, float cy, float r, float angle)
{
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillEllipse (cx - r - 1, cy - r + 1, 2 * r + 2, 2 * r + 2);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3d42), cx - r, cy - r, juce::Colour (0xff0d0e10), cx + r, cy + r, false));
    g.fillEllipse (cx - r, cy - r, 2 * r, 2 * r);
    const float ri = r * 0.78f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff5a5e66), cx - ri, cy - ri, juce::Colour (0xff15171a), cx + ri, cy + ri, false));
    g.fillEllipse (cx - ri, cy - ri, 2 * ri, 2 * ri);
    for (int k = 0; k < 2; ++k)   // ranura Phillips
    {
        const float a = angle + (float) k * juce::MathConstants<float>::halfPi;
        const float dx = std::cos (a) * ri * 0.75f, dy = std::sin (a) * ri * 0.75f;
        g.setColour (juce::Colours::black);
        g.drawLine (cx - dx, cy - dy, cx + dx, cy + dy, r * 0.28f);
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawLine (cx - dx + 0.8f, cy - dy + 0.8f, cx + dx + 0.8f, cy + dy + 0.8f, 0.8f);
    }
}

void brushed (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour top, juce::Colour bot, int seed, float strength)
{
    g.setGradientFill (juce::ColourGradient (top, r.getX(), r.getY(), bot, r.getX(), r.getBottom(), false));
    g.fillRect (r);
    juce::Random rnd (seed);
    const int lines = (int) (r.getWidth() * r.getHeight() / 160.0f);
    for (int i = 0; i < lines; ++i)
    {
        const float y = r.getY() + rnd.nextFloat() * r.getHeight();
        const float x = r.getX() + rnd.nextFloat() * r.getWidth();
        const float len = 40.0f + rnd.nextFloat() * 500.0f;
        const bool light = rnd.nextBool();
        g.setColour ((light ? juce::Colours::white : juce::Colours::black).withAlpha ((0.015f + rnd.nextFloat() * 0.04f) * strength));
        g.fillRect (x, y, juce::jmin (len, r.getRight() - x), 1.0f);
    }
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.06f * strength), r.getX(), r.getY(),
                                             juce::Colours::transparentWhite, r.getCentreX(), r.getBottom(), false));
    g.fillRect (r);
}

//==============================================================================
// LOOK AND FEEL
//==============================================================================
LookAndFeel::LookAndFeel()
{
    setColour (juce::ComboBox::textColourId, Colours::amber);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff15171c));
    setColour (juce::PopupMenu::textColourId, Colours::engrave);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colours::amber.withAlpha (0.35f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::BubbleComponent::backgroundColourId, juce::Colour (0xee101114));
    setColour (juce::BubbleComponent::outlineColourId, Colours::amber.withAlpha (0.6f));
    setColour (juce::Label::textColourId, Colours::amber);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                    float a0, float a1, juce::Slider& s)
{
    const float cx = (float) x + (float) w * 0.5f, cy = (float) y + (float) h * 0.52f;
    const float R = juce::jmin ((float) w, (float) h) * 0.5f * 0.64f;
    const float ang = a0 + pos * (a1 - a0);

    // Escala: marcas
    const int ticks = 21;
    for (int i = 0; i < ticks; ++i)
    {
        const float a = a0 + (float) i / (float) (ticks - 1) * (a1 - a0);
        const bool major = i % 5 == 0;
        const float r1 = R * 1.1f, r2 = R * (major ? 1.30f : 1.20f);
        const float sx = std::sin (a), sy = -std::cos (a);
        g.setColour (Colours::engraveDim.withAlpha (major ? 0.95f : 0.7f));
        g.drawLine (cx + sx * r1, cy + sy * r1, cx + sx * r2, cy + sy * r2, major ? 1.6f : 1.1f);
    }

    // Escala: etiquetas
    auto label = [&] (const juce::String& t, float a, float rr)
    {
        if (t.isEmpty()) return;
        const float fs = juce::jlimit (9.5f, 14.0f, R * 0.30f);
        const float lx = cx + std::sin (a) * R * rr, ly = cy - std::cos (a) * R * rr;
        engraved (g, t, { lx - 34.0f, ly - fs * 0.6f, 68.0f, fs * 1.2f }, fs);
    };
    const auto& props = s.getProperties();
    label (props["lmin"].toString(), a0, 1.58f);
    label (props["lmax"].toString(), a1, 1.58f);
    label (props["lmid"].toString(), (a0 + a1) * 0.5f, 1.48f);

    // Sombra
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (cx - R * 1.02f + 2.0f, cy - R * 1.02f + 4.0f, R * 2.04f, R * 2.04f);

    // Faldon negro estriado
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2b2e), cx, cy - R, juce::Colour (0xff050506), cx, cy + R, false));
    g.fillEllipse (cx - R, cy - R, 2 * R, 2 * R);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    for (int i = 0; i < 72; ++i)
    {
        const float a = (float) i / 72.0f * juce::MathConstants<float>::twoPi;
        g.drawLine (cx + std::sin (a) * R * 0.86f, cy - std::cos (a) * R * 0.86f,
                    cx + std::sin (a) * R * 0.99f, cy - std::cos (a) * R * 0.99f, 1.0f);
    }
    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.drawEllipse (cx - R + 0.5f, cy - R + 0.5f, 2 * R - 1, 2 * R - 1, 0.8f);

    // Tapa de aluminio torneado
    const float Rc = R * 0.74f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffeceef1), cx - Rc, cy - Rc, juce::Colour (0xff7d8189), cx + Rc, cy + Rc, false));
    g.fillEllipse (cx - Rc, cy - Rc, 2 * Rc, 2 * Rc);
    int ring = 0;
    for (float rr = Rc - 1.5f; rr > 2.0f; rr -= 1.6f, ++ring)
    {
        g.setColour ((ring % 2 == 0 ? juce::Colours::white : juce::Colours::black).withAlpha (0.05f));
        g.drawEllipse (cx - rr, cy - rr, 2 * rr, 2 * rr, 0.8f);
    }
    for (int k = 0; k < 2; ++k)   // reflejos conicos
    {
        juce::Path wedge;
        const float base = -0.9f + (float) k * juce::MathConstants<float>::pi;
        wedge.addPieSegment (cx - Rc, cy - Rc, 2 * Rc, 2 * Rc, base - 0.35f, base + 0.35f, 0.0f);
        g.setColour (juce::Colours::white.withAlpha (k == 0 ? 0.28f : 0.14f));
        g.fillPath (wedge);
    }
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawEllipse (cx - Rc, cy - Rc, 2 * Rc, 2 * Rc, 1.0f);

    // Indicador
    juce::Path ptr;
    const float pw = juce::jmax (2.2f, R * 0.07f);
    ptr.addRoundedRectangle (-pw * 0.5f, -R * 0.97f, pw, R * 0.78f, pw * 0.5f);
    const auto tr = juce::AffineTransform::rotation (ang).translated (cx, cy);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.strokePath (ptr, juce::PathStrokeType (1.2f), tr);
    g.setColour (juce::Colour (0xfff7f7f2));
    g.fillPath (ptr, tr);
}

void LookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (r, 3.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff060607), 0, 0, juce::Colour (0xff17181b), 0, (float) h, false));
    g.fillRoundedRectangle (r.reduced (1.0f), 3.0f);
    g.setColour (juce::Colours::white.withAlpha (0.14f));
    g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 0.8f);
    juce::Path tri;
    const float ax = (float) w - 13.0f, ay = (float) h * 0.5f;
    tri.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (Colours::amber.withAlpha (0.85f));
    g.fillPath (tri);
}

void LookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 0, box.getWidth() - 24, box.getHeight());
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
}

juce::Font LookAndFeel::getComboBoxFont (juce::ComboBox& b)          { return serif (juce::jmin (15.0f, (float) b.getHeight() * 0.62f)); }
juce::Font LookAndFeel::getPopupMenuFont()                           { return serif (15.0f); }
juce::Font LookAndFeel::getSliderPopupFont (juce::Slider&)           { return sans (14.0f, true); }
juce::Font LookAndFeel::getTextButtonFont (juce::TextButton&, int h) { return serif (juce::jmin (14.0f, (float) h * 0.5f), true); }

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (r.translated (0, 1.5f), 4.0f);
    auto top = juce::Colour (0xffd9dbdf), bot = juce::Colour (0xff8d9097);
    if (over) { top = top.brighter (0.1f); bot = bot.brighter (0.1f); }
    if (down) std::swap (top, bot);
    g.setGradientFill (juce::ColourGradient (top, 0, r.getY(), bot, 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

void LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (juce::Colour (0xff1b1c20));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, false);
}

//==============================================================================
// COMPONENTES
//==============================================================================
HwKnob::HwKnob (APVTS& s, const juce::String& id, const juce::String& t,
                const juce::String& minL, const juce::String& midL, const juce::String& maxL)
    : title (t)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setPopupDisplayEnabled (true, true, nullptr);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.22f, juce::MathConstants<float>::pi * 2.78f, true);
    slider.getProperties().set ("lmin", minL);
    slider.getProperties().set ("lmid", midL);
    slider.getProperties().set ("lmax", maxL);
    addAndMakeVisible (slider);
    attachment = std::make_unique<APVTS::SliderAttachment> (s, id, slider);
    if (auto* prm = s.getParameter (id))
        slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
}

void HwKnob::resized()
{
    slider.setBounds (getLocalBounds().withTrimmedTop (20));
}

void HwKnob::paint (juce::Graphics& g)
{
    engraved (g, title, getLocalBounds().removeFromTop (20).toFloat(), juce::jlimit (12.0f, 17.0f, (float) getWidth() * 0.12f));
}

//------------------------------------------------------------------------------
void ToggleSwitch::paintButton (juce::Graphics& g, bool over, bool)
{
    const auto b = getLocalBounds().toFloat();
    const float cx = b.getCentreX(), cy = b.getCentreY();
    const float nr = juce::jmin (b.getWidth(), b.getHeight()) * 0.26f;

    juce::Path hex;   // tuerca hexagonal
    for (int i = 0; i < 6; ++i)
    {
        const float a = (float) i / 6.0f * juce::MathConstants<float>::twoPi + 0.52f;
        const float px = cx + std::cos (a) * nr * 1.25f, py = cy + std::sin (a) * nr * 1.25f;
        if (i == 0) hex.startNewSubPath (px, py); else hex.lineTo (px, py);
    }
    hex.closeSubPath();
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillPath (hex, juce::AffineTransform::translation (1.0f, 2.0f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd5d7db), cx - nr, cy - nr, juce::Colour (0xff60636a), cx + nr, cy + nr, false));
    g.fillPath (hex);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.strokePath (hex, juce::PathStrokeType (0.8f));
    g.setColour (juce::Colour (0xff1b1c1f));
    g.fillEllipse (cx - nr * 0.62f, cy - nr * 0.62f, nr * 1.24f, nr * 1.24f);

    // Palanca (arriba = ON)
    const bool on = getToggleState();
    const float len = b.getHeight() * 0.40f, lw = nr * 0.62f;
    const float tipY = on ? cy - len : cy + len;
    juce::Path lever;
    lever.addRoundedRectangle (cx - lw * 0.5f, juce::jmin (cy, tipY), lw, std::abs (tipY - cy), lw * 0.5f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillPath (lever, juce::AffineTransform::translation (1.5f, 2.0f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff2f3f5), cx - lw, 0, juce::Colour (0xff767a82), cx + lw, 0, false));
    g.fillPath (lever);
    const float br = lw * 0.75f;
    g.setGradientFill (juce::ColourGradient (juce::Colours::white, cx - br, tipY - br, juce::Colour (0xff6f737b), cx + br, tipY + br, false));
    g.fillEllipse (cx - br, tipY - br, 2 * br, 2 * br);
    if (over) { g.setColour (juce::Colours::white.withAlpha (0.06f)); g.fillEllipse (b.reduced (2)); }
}

//------------------------------------------------------------------------------
void LampButton::paintButton (juce::Graphics& g, bool over, bool)
{
    auto b = getLocalBounds().toFloat().reduced (1);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (b.translated (0, 1.5f), 3);
    g.setColour (juce::Colour (0xff0b0b0d));
    g.fillRoundedRectangle (b, 3);
    g.setColour (juce::Colours::white.withAlpha (0.15f));
    g.drawRoundedRectangle (b, 3, 0.8f);

    auto lamp = b.removeFromLeft (b.getHeight() * 1.25f).reduced (4);
    if (getToggleState())
    {
        g.setColour (Colours::red.withAlpha (0.35f));
        g.fillRoundedRectangle (lamp.expanded (3), 4);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffb3a8), lamp.getCentreX(), lamp.getCentreY(),
                                                 Colours::red.darker (0.2f), lamp.getX(), lamp.getY(), true));
    }
    else
        g.setColour (juce::Colour (0xff3a0d0a).brighter (over ? 0.2f : 0.0f));
    g.fillRoundedRectangle (lamp, 2);
    engraved (g, getButtonText(), b.withTrimmedRight (4), juce::jmin (16.0f, b.getHeight() * 0.62f), juce::Justification::centred, true);
}

//------------------------------------------------------------------------------
void Led::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float cx = b.getCentreX(), cy = b.getCentreY(), r = juce::jmin (b.getWidth(), b.getHeight()) * 0.3f;
    last = state();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd8dadd), cx - r * 1.4f, cy - r * 1.4f, juce::Colour (0xff3a3c40), cx + r * 1.4f, cy + r * 1.4f, false));
    g.fillEllipse (cx - r * 1.32f, cy - r * 1.32f, r * 2.64f, r * 2.64f);
    if (last)
    {
        g.setGradientFill (juce::ColourGradient (Colours::amber.withAlpha (0.55f), cx, cy, Colours::amber.withAlpha (0.0f), cx + r * 1.6f, cy, true));
        g.fillEllipse (cx - r * 1.66f, cy - r * 1.66f, r * 3.32f, r * 3.32f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffff4c2), cx - r * 0.2f, cy - r * 0.3f, juce::Colour (0xffe07a00), cx + r, cy + r, true));
    }
    else
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6b4a1c), cx - r * 0.3f, cy - r * 0.3f, Colours::amberDim, cx + r, cy + r, true));
    g.fillEllipse (cx - r, cy - r, 2 * r, 2 * r);
    g.setColour (juce::Colours::white.withAlpha (last ? 0.6f : 0.2f));
    g.fillEllipse (cx - r * 0.45f, cy - r * 0.6f, r * 0.5f, r * 0.35f);
}

//------------------------------------------------------------------------------
// Posicion (0..1) de las marcas de un VU real
static float vuPos (float vu)
{
    static const float db[]  { -20, -10, -7, -5, -3, -2, -1, 0, 1, 2, 3 };
    static const float pos[] { 0.0f, 0.22f, 0.33f, 0.43f, 0.55f, 0.61f, 0.68f, 0.75f, 0.83f, 0.91f, 1.0f };
    if (vu <= db[0]) return 0.0f;
    for (int i = 1; i < 11; ++i)
        if (vu <= db[i]) return pos[i - 1] + (vu - db[i - 1]) / (db[i] - db[i - 1]) * (pos[i] - pos[i - 1]);
    return 1.0f;
}

void VuMeter::tick()
{
    float target;
    if (gr) target = 1.0f - juce::jlimit (0.0f, 20.0f, -src()) / 20.0f;
    else    target = vuPos (src() + 15.0f);       // 0 VU ~ -15 dBFS de pico
    needle += (target - needle) * 0.22f;
    repaint();
}

void VuMeter::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (b.translated (0, 2), 5);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2b2d31), 0, b.getY(), juce::Colour (0xff0b0c0e), 0, b.getBottom(), false));
    g.fillRoundedRectangle (b, 5);
    auto face = b.reduced (6);
    g.setGradientFill (juce::ColourGradient (Colours::vuFace, face.getCentreX(), face.getY(), juce::Colour (0xffd8c99c), face.getCentreX(), face.getBottom(), false));
    g.fillRoundedRectangle (face, 3);

    const float R = juce::jmin (face.getHeight() * 1.02f, face.getWidth() * 0.58f);
    const float px = face.getCentreX(), py = face.getBottom() + R * 0.30f;
    const float aMin = -0.82f, aMax = 0.82f;
    auto pt = [&] (float p, float rr) { const float a = aMin + p * (aMax - aMin); return juce::Point<float> (px + std::sin (a) * rr, py - std::cos (a) * rr); };

    g.saveState();
    g.reduceClipRegion (face.toNearestInt());

    juce::Path arc; arc.addCentredArc (px, py, R * 0.84f, R * 0.84f, 0, aMin, aMax, true);
    g.setColour (juce::Colour (0xff2a2622)); g.strokePath (arc, juce::PathStrokeType (1.2f));
    if (! gr)
    {
        juce::Path redArc; redArc.addCentredArc (px, py, R * 0.86f, R * 0.86f, 0, aMin + 0.75f * (aMax - aMin), aMax, true);
        g.setColour (juce::Colour (0xffc8281e)); g.strokePath (redArc, juce::PathStrokeType (4.0f));
    }

    g.setFont (sans (juce::jmax (8.0f, R * 0.11f)));
    auto mark = [&] (float p, const juce::String& t, bool redMark)
    {
        auto a = pt (p, R * 0.80f), c = pt (p, R * 0.90f), l = pt (p, R * 1.0f);
        g.setColour (redMark ? juce::Colour (0xffc8281e) : juce::Colour (0xff2a2622));
        g.drawLine ({ a, c }, 1.2f);
        if (t.isNotEmpty()) g.drawText (t, juce::Rectangle<float> (l.x - 14, l.y - 7, 28, 12), juce::Justification::centred);
    };
    if (gr)
        for (int d : { 20, 15, 10, 7, 5, 3, 1, 0 }) mark (1.0f - (float) d / 20.0f, juce::String (d), false);
    else
        for (int v : { -20, -10, -7, -5, -3, -2, -1, 0, 1, 2, 3 })
            mark (vuPos ((float) v), v > 0 ? "+" + juce::String (v) : juce::String (std::abs (v)), v > 0);

    g.setFont (serif (R * 0.24f, true));
    g.setColour (juce::Colour (0xff2a2622));
    g.drawText (gr ? "GR" : "VU", juce::Rectangle<float> (px - R * 0.5f, py - R * 0.62f, R, R * 0.28f), juce::Justification::centred);

    const auto tip = pt (needle, R * 0.93f);
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.drawLine (px + 2, py, tip.x + 2, tip.y + 2, 1.6f);
    g.setColour (juce::Colour (0xff111111));
    g.drawLine (px, py, tip.x, tip.y, 1.5f);
    g.restoreState();

    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.25f), face.getX(), face.getY(),
                                             juce::Colours::transparentWhite, face.getX(), face.getCentreY(), false));
    g.fillRoundedRectangle (face.withHeight (face.getHeight() * 0.5f), 3);
}


void lcdFrame (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (r.translated (0, 2).expanded (2), 5);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2b2d31), 0, r.getY(), juce::Colour (0xff0b0c0e), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r.expanded (3), 5);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff120c05), r.getCentreX(), r.getY(), juce::Colour (0xff070503), r.getCentreX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 3);
}

void glowText (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, juce::Font f,
               juce::Justification j, juce::Colour c)
{
    g.setFont (f);
    g.setColour (c.withAlpha (0.18f));
    for (auto off : { juce::Point<float> (-1, 0), juce::Point<float> (1, 0), juce::Point<float> (0, -1), juce::Point<float> (0, 1) })
        g.drawText (t, r + off, j, false);
    g.setColour (c);
    g.drawText (t, r, j, false);
}

} // namespace tvcui
