#include "ConsoleWidgets.h"

namespace cui
{
static int currentTheme = 0;
int getTheme() { return currentTheme; }
void setTheme (int theme)
{
    currentTheme = theme;
    if (theme == 1)
    {
        Col::panel = juce::Colour (0xff2c2e33); Col::panelTint = juce::Colour (0xff26282c);
        Col::sage = juce::Colour (0xff2a302c);  Col::sand = juce::Colour (0xff302c26);
        Col::ink = juce::Colour (0xffe9e4d6);   Col::inkDim = juce::Colour (0xffa3a6ad);
    }
    else
    {
        Col::panel = juce::Colour (0xffe6e1d4); Col::panelTint = juce::Colour (0xffd9d3c3);
        Col::sage = juce::Colour (0xffd3dbcf);  Col::sand = juce::Colour (0xffe2d6bd);
        Col::ink = juce::Colour (0xff23252a);   Col::inkDim = juce::Colour (0xff62646b);
    }
}

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
    g.setGradientFill (juce::ColourGradient (Col::header.brighter (0.32f), 0, head.getY(), Col::header.darker (0.25f), 0, head.getBottom(), false));
    g.fillRect (head);
    g.setColour (juce::Colours::white.withAlpha (0.14f));            // brillo del borde superior
    g.fillRect (head.getX(), head.getY() + 0.5f, head.getWidth(), 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRect (head.getX(), head.getBottom(), head.getWidth(), 1.5f);
    g.setColour (Col::amber.withAlpha (0.35f));                       // linea de acento bajo la cabecera
    g.fillRect (head.getX(), head.getBottom() + 1.5f, head.getWidth(), 1.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.18f), 0, head.getBottom() + 2.5f,
                                             juce::Colours::transparentBlack, 0, head.getBottom() + 10.0f, false));   // sombra sobre el panel
    g.fillRect (head.getX(), head.getBottom() + 2.5f, head.getWidth(), 8.0f);
    g.restoreState();
    g.setFont (font (15.0f, true));
    g.setColour (Col::cream);
    g.drawText (title, head.reduced (12, 0), juce::Justification::centredLeft, false);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawRoundedRectangle (r, 4, 1.2f);
    smallScrew (g, r.getCentreX(), r.getBottom() - 10, 4.5f);
}

void drawLogo (juce::Graphics& g, juce::Rectangle<float> a)
{
    // Marca: microfono estilizado dentro de un anillo con ondas
    const float s = a.getHeight();
    const float cx = a.getX() + s * 0.5f, cy = a.getCentreY(), R = s * 0.44f;
    g.setGradientFill (juce::ColourGradient (Col::amber, cx, cy - R, Col::orange.darker (0.3f), cx, cy + R, false));
    g.fillEllipse (cx - R, cy - R, 2 * R, 2 * R);
    g.setColour (Col::header);
    g.fillEllipse (cx - R * 0.8f, cy - R * 0.8f, R * 1.6f, R * 1.6f);
    g.setColour (Col::amber);
    g.fillRoundedRectangle (cx - R * 0.22f, cy - R * 0.55f, R * 0.44f, R * 0.7f, R * 0.22f);
    juce::Path cup; cup.addCentredArc (cx, cy - R * 0.05f, R * 0.38f, R * 0.38f, 0, juce::MathConstants<float>::pi * 0.55f, juce::MathConstants<float>::pi * 1.45f, true);
    g.strokePath (cup, juce::PathStrokeType (R * 0.08f));
    g.fillRect (cx - R * 0.04f, cy + R * 0.33f, R * 0.08f, R * 0.2f);
    for (int k = 1; k <= 2; ++k)
    {
        juce::Path w; const float rr = R * (0.55f + 0.17f * (float) k);
        w.addCentredArc (cx, cy - R * 0.2f, rr, rr, 0, juce::MathConstants<float>::pi * 0.25f, juce::MathConstants<float>::pi * 0.6f, true);
        g.strokePath (w, juce::PathStrokeType (R * 0.06f));
        juce::Path w2; w2.addCentredArc (cx, cy - R * 0.2f, rr, rr, 0, -juce::MathConstants<float>::pi * 0.6f, -juce::MathConstants<float>::pi * 0.25f, true);
        g.strokePath (w2, juce::PathStrokeType (R * 0.06f));
    }
    // Texto
    auto t = a.withTrimmedLeft (s + 10);
    // El texto se ajusta al espacio disponible
    auto fitted = [&] (const juce::String& txt, float maxH, bool bold)
    {
        auto f = font (maxH, bold);
        const float w = juce::GlyphArrangement::getStringWidth (f, txt);
        if (w > t.getWidth()) f = font (maxH * t.getWidth() / w, bold);
        return f;
    };
    g.setColour (Col::cream);
    g.setFont (fitted ("TOP VOCAL CHAIN", s * 0.46f, true));
    g.drawText ("TOP VOCAL CHAIN", t.withHeight (s * 0.58f), juce::Justification::bottomLeft, false);
    g.setColour (Col::amber.withAlpha (0.85f));
    g.setFont (fitted ("PROFESSIONAL VOCAL PROCESSING SUITE", s * 0.26f, false));
    g.drawText ("PROFESSIONAL VOCAL PROCESSING SUITE", t.withTrimmedTop (s * 0.6f), juce::Justification::topLeft, false);
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
    const float R = juce::jmin ((float) w, (float) h) * 0.5f * 0.58f;
    const float cx = (float) x + (float) w * 0.5f, cy = (float) y + (float) h * 0.54f;
    const float ang = a0 + pos * (a1 - a0);
    const auto& pr = s.getProperties();
    const bool bipolar = (bool) pr["bipolar"];
    const bool active = s.isMouseOverOrDragging();

    // Graduacion: 31 marcas pequenas + 3 principales
    for (int i = 0; i <= 30; ++i)
    {
        const float a = a0 + (float) i / 30.0f * (a1 - a0);
        const bool major = i % 15 == 0, mid = i % 5 == 0;
        const float r1 = R * 1.33f, r2 = R * (major ? 1.50f : mid ? 1.44f : 1.39f);
        g.setColour (Col::ink.withAlpha (major ? 0.95f : mid ? 0.7f : 0.4f));
        g.drawLine (cx + std::sin (a) * r1, cy - std::cos (a) * r1, cx + std::sin (a) * r2, cy - std::cos (a) * r2, major ? 1.6f : 1.0f);
    }
    auto label = [&] (const juce::String& t, float a, float rr)
    {
        if (t.isEmpty()) return;
        const float fs = juce::jlimit (8.5f, 11.0f, R * 0.25f);
        const float lx = cx + std::sin (a) * R * rr, ly = cy - std::cos (a) * R * rr;
        g.setFont (font (fs, true));
        g.setColour (Col::inkDim.darker (0.3f));
        g.drawText (t, juce::Rectangle<float> (lx - 30, ly - fs * 0.6f, 60, fs * 1.2f), juce::Justification::centred, false);
    };
    label (pr["lmin"].toString(), a0, 1.78f);
    label (pr["lmax"].toString(), a1, 1.78f);
    label (pr["lmid"].toString(), (a0 + a1) * 0.5f, 1.68f);

    // Anillo LED: pista apagada + arco iluminado segun el valor (bipolar sale desde el centro)
    const float rr = R * 1.20f, lw = juce::jmax (2.5f, R * 0.11f);
    juce::Path track; track.addCentredArc (cx, cy, rr, rr, 0, a0, a1, true);
    g.setColour (juce::Colours::black.withAlpha (0.28f));
    g.strokePath (track, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const float from = bipolar ? (a0 + a1) * 0.5f : a0;
    if (std::abs (ang - from) > 0.01f)
    {
        juce::Path lit; lit.addCentredArc (cx, cy, rr, rr, 0, std::min (from, ang), std::max (from, ang), true);
        const auto led = cap.brighter (0.55f).withSaturation (juce::jmin (1.0f, cap.getSaturation() * 1.3f));
        g.setColour (led.withAlpha (active ? 0.35f : 0.2f));
        g.strokePath (lit, juce::PathStrokeType (lw * 2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (led);
        g.strokePath (lit, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Sombra proyectada
    g.setColour (juce::Colours::black.withAlpha (0.38f));
    g.fillEllipse (cx - R * 1.02f + 1.5f, cy - R * 1.02f + 4.0f, R * 2.04f, R * 2.04f);

    // Aro metalico exterior con reflejos
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff2f3f5), cx - R, cy - R, juce::Colour (0xff5d6168), cx + R, cy + R, false));
    g.fillEllipse (cx - R, cy - R, 2 * R, 2 * R);
    for (int k = 0; k < 2; ++k)
    {
        juce::Path wedge;
        const float base = -0.8f + (float) k * juce::MathConstants<float>::pi;
        wedge.addPieSegment (cx - R, cy - R, 2 * R, 2 * R, base - 0.3f, base + 0.3f, 0.86f);
        g.setColour (juce::Colours::white.withAlpha (k == 0 ? 0.55f : 0.22f));
        g.fillPath (wedge);
    }
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawEllipse (cx - R, cy - R, 2 * R, 2 * R, 1.0f);

    // Faldon estriado oscuro
    const float Rs = R * 0.88f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4c51), cx, cy - Rs, juce::Colour (0xff151619), cx, cy + Rs, false));
    g.fillEllipse (cx - Rs, cy - Rs, 2 * Rs, 2 * Rs);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    for (int i = 0; i < 48; ++i)
    {
        const float a = (float) i / 48.0f * juce::MathConstants<float>::twoPi;
        g.drawLine (cx + std::sin (a) * Rs * 0.86f, cy - std::cos (a) * Rs * 0.86f, cx + std::sin (a) * Rs * 0.99f, cy - std::cos (a) * Rs * 0.99f, 1.0f);
    }

    // Tapa de color con sombra interior
    const float Rc = Rs * 0.80f;
    g.setGradientFill (juce::ColourGradient (cap.brighter (0.5f), cx - Rc * 0.45f, cy - Rc * 0.6f, cap.darker (0.45f), cx + Rc * 0.6f, cy + Rc * 0.8f, true));
    g.fillEllipse (cx - Rc, cy - Rc, 2 * Rc, 2 * Rc);
    g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, cx, cy, juce::Colours::black.withAlpha (0.35f), cx + Rc, cy, true));
    g.fillEllipse (cx - Rc, cy - Rc, 2 * Rc, 2 * Rc);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.45f), cx - Rc * 0.3f, cy - Rc * 0.75f, juce::Colours::transparentWhite, cx, cy - Rc * 0.1f, true));
    g.fillEllipse (cx - Rc * 0.72f, cy - Rc * 0.92f, Rc * 1.3f, Rc * 0.85f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawEllipse (cx - Rc, cy - Rc, 2 * Rc, 2 * Rc, 1.0f);

    // Indicador: linea blanca con sombra que atraviesa faldon y tapa
    juce::Path ptr;
    const float pw = juce::jmax (2.2f, R * 0.09f);
    ptr.addRoundedRectangle (-pw * 0.5f, -Rs * 0.98f, pw, Rs * 0.62f, pw * 0.5f);
    const auto tr = juce::AffineTransform::rotation (ang).translated (cx, cy);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillPath (ptr, tr.translated (0.8f, 1.2f));
    g.setColour (juce::Colours::white);
    g.fillPath (ptr, tr);
    if (active)
    {
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillEllipse (cx - R, cy - R, 2 * R, 2 * R);
    }
}

void LnF::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
    g.setColour (juce::Colour (0xf01a1b1f));
    g.fillRoundedRectangle (r, 5);
    g.setColour (Col::amber.withAlpha (0.6f));
    g.drawRoundedRectangle (r.reduced (0.5f), 5, 1.0f);
    juce::AttributedString a;
    const auto lines = juce::StringArray::fromLines (text);
    for (int i = 0; i < lines.size(); ++i)
        a.append (lines[i] + (i + 1 < lines.size() ? "\n" : ""), font (i == 0 ? 13.5f : 12.0f, i == 0), i == 0 ? Col::amber : Col::cream);
    a.setWordWrap (juce::AttributedString::byWord);
    a.draw (g, r.reduced (10, 7));
}

juce::Rectangle<int> LnF::getTooltipBounds (const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parent)
{
    const auto lines = juce::StringArray::fromLines (text);
    int longest = 0;
    for (auto& l : lines) longest = std::max (longest, l.length());
    const int w = juce::jlimit (120, 320, longest * 7 + 24);
    const int h = 16 + lines.size() * 17 + (longest * 7 + 24 > 320 ? 16 * (lines.size()) : 0);
    return juce::Rectangle<int> (pos.x + 14, pos.y + 18, w, h).constrainedWithin (parent);
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
    auto top = currentTheme == 1 ? juce::Colour (0xff50535a) : juce::Colour (0xfff0eee8);
    auto bot = currentTheme == 1 ? juce::Colour (0xff303237) : juce::Colour (0xffb7b4ac);
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
    : title (t), paramId (id), state (s)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setPopupDisplayEnabled (false, false, nullptr);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.22f, juce::MathConstants<float>::pi * 2.78f, true);
    slider.setColour (juce::Slider::rotarySliderFillColourId, cap);
    slider.setMouseDragSensitivity (220);
    // Shift o Ctrl al arrastrar = ajuste fino
    slider.setVelocityModeParameters (0.25, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
    slider.setScrollWheelEnabled (true);
    slider.getProperties().set ("lmin", minL);
    slider.getProperties().set ("lmid", midL);
    slider.getProperties().set ("lmax", maxL);
    addAndMakeVisible (slider);
    att = std::make_unique<APVTS::SliderAttachment> (s, id, slider);
    if (auto* prm = dynamic_cast<juce::RangedAudioParameter*> (s.getParameter (id)))
    {
        const auto& r = prm->getNormalisableRange();
        slider.getProperties().set ("bipolar", r.start < 0.0f && std::abs (r.start + r.end) < 0.01f * (r.end - r.start));
        // Alt + clic = valor por defecto (el doble clic lo usa KnobSlider para escribir el valor)
        slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()), juce::ModifierKeys::altModifier);
    }
    slider.onTypeValue = [this] { showEditor(); };
    slider.onRightClick = [this] { if (auto* h = findParentComponentOfClass<MidiHost>()) h->showKnobMenu (*this); };
    slider.addListener (this);
    setDescription ({});
}

Knob::~Knob() { slider.removeListener (this); }

void Knob::setDescription (const juce::String& d)
{
    juce::String tip = title;
    if (auto* prm = state.getParameter (paramId)) tip = prm->getName (64).toUpperCase();
    if (d.isNotEmpty()) tip << "\n" << d;
    tip << "\n" << juce::String::fromUTF8 ("Doble clic: escribir valor · Shift: ajuste fino · Alt+clic: reset");
    slider.setTooltip (tip);
}

void Knob::showEditor()
{
    auto* prm = state.getParameter (paramId);
    if (prm == nullptr) return;
    editor = std::make_unique<juce::TextEditor>();
    auto& e = *editor;
    e.setFont (font (14.0f, true));
    e.setJustification (juce::Justification::centred);
    e.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff0d0b08));
    e.setColour (juce::TextEditor::textColourId, Col::amber);
    e.setColour (juce::TextEditor::outlineColourId, Col::amber);
    e.setColour (juce::TextEditor::focusedOutlineColourId, Col::amber);
    e.setText (prm->getCurrentValueAsText(), false);
    e.selectAll();
    addAndMakeVisible (e);
    e.setBounds (getLocalBounds().withSizeKeepingCentre (juce::jmin (getWidth() - 4, 96), 26));
    auto close = [this] (bool commit)
    {
        if (editor == nullptr) return;
        if (commit)
            if (auto* p = state.getParameter (paramId))
            {
                const float v = p->getValueForText (editor->getText());
                p->beginChangeGesture(); p->setValueNotifyingHost (v); p->endChangeGesture();
            }
        juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<Knob> (this)] { if (sp != nullptr) sp->editor.reset(); });
    };
    e.onReturnKey = [close] { close (true); };
    e.onEscapeKey = [close] { close (false); };
    e.onFocusLost = [close] { close (true); };
    e.grabKeyboardFocus();
}

void Knob::resized() { slider.setBounds (getLocalBounds().withTrimmedBottom (18)); }

void Knob::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().removeFromBottom (18);
    // Mientras se toca la perilla, el nombre se cambia por su valor en una pantallita ambar
    if (slider.isMouseOverOrDragging())
        if (auto* prm = state.getParameter (paramId))
        {
            const auto txt = prm->getCurrentValueAsText();
            g.setFont (font (12.5f, true));
            const float tw = juce::jlimit (40.0f, (float) getWidth() - 2.0f, (float) txt.length() * 7.8f + 14.0f);
            auto box = r.toFloat().withSizeKeepingCentre (tw, 17.0f);
            g.setColour (juce::Colour (0xff0d0b08)); g.fillRoundedRectangle (box, 3);
            g.setColour (Col::amber); g.drawText (txt, box, juce::Justification::centred, false);
            return;
        }
    g.setFont (font (juce::jlimit (10.5f, 13.0f, (float) getWidth() * 0.11f), true));
    g.setColour (Col::ink);
    g.drawText (title, r, juce::Justification::centred, false);
    // Etiqueta MIDI si la perilla esta asignada a un CC
    if (auto* host = findParentComponentOfClass<MidiHost>())
        if (const int cc = host->ccForParam (paramId); cc >= 0)
        {
            auto b = juce::Rectangle<float> ((float) getWidth() - 34.0f, 0.0f, 32.0f, 13.0f);
            g.setColour (Col::blue.withAlpha (0.85f)); g.fillRoundedRectangle (b, 3);
            g.setFont (font (9.0f, true)); g.setColour (juce::Colours::white);
            g.drawText ("CC" + juce::String (cc), b, juce::Justification::centred, false);
        }
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
        g.setGradientFill (juce::ColourGradient ((currentTheme == 1 ? juce::Colour (0xff4d5057) : juce::Colour (0xffe9e6de))
                                                     .darker (down ? 0.15f : (over ? 0.03f : 0.0f)), 0, face.getY(),
                                                 currentTheme == 1 ? juce::Colour (0xff2f3136) : juce::Colour (0xffb9b5ab), 0, face.getBottom(), false));
    g.fillRoundedRectangle (face, 2);
    g.setFont (font (juce::jmin (12.5f, face.getHeight() * 0.58f), true));
    g.setColour (on ? juce::Colour (0xff1b1408) : Col::ink);
    g.drawText (getButtonText(), face, juce::Justification::centred, false);
}

//------------------------------------------------------------------------------
void ModuleSwitch::paintButton (juce::Graphics& g, bool over, bool down)
{
    const int s = status();
    last = s;
    auto r = getLocalBounds().toFloat().reduced (1);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (r.translated (0, 1.5f), 4);
    g.setColour (juce::Colour (0xff111214));
    g.fillRoundedRectangle (r, 4);
    auto face = r.reduced (2.0f);
    const juce::Colour faceCol = s == 2 ? juce::Colour (0xffe0742a) : s == 1 ? juce::Colour (0xff2f9a57) : juce::Colour (0xff45474c);
    g.setGradientFill (juce::ColourGradient (faceCol.brighter (over ? 0.35f : 0.25f), 0, face.getY(), faceCol.darker (down ? 0.4f : 0.15f), 0, face.getBottom(), false));
    g.fillRoundedRectangle (face, 3);
    // Indicador luminoso
    const float lr = face.getHeight() * 0.22f;
    const float lx = face.getX() + face.getHeight() * 0.5f, ly = face.getCentreY();
    const juce::Colour led = s == 2 ? juce::Colour (0xffffc04d) : s == 1 ? juce::Colour (0xff9cffb9) : juce::Colour (0xff2a2b2e);
    if (s > 0)
    {
        g.setColour (led.withAlpha (0.35f));
        g.fillEllipse (lx - lr * 2.0f, ly - lr * 2.0f, lr * 4.0f, lr * 4.0f);
    }
    g.setColour (led);
    g.fillEllipse (lx - lr, ly - lr, 2 * lr, 2 * lr);
    g.setFont (font (juce::jmin (12.0f, face.getHeight() * 0.6f), true));
    g.setColour (s > 0 ? juce::Colours::white : juce::Colour (0xffa9abb1));
    g.drawText (s > 0 ? "IN" : "OFF", face.withTrimmedLeft (face.getHeight() * 0.8f), juce::Justification::centred, false);
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
