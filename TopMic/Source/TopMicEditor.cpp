#include "TopMicEditor.h"

using namespace tmicui;

namespace
{
    juce::String U (const char* s) { return juce::String::fromUTF8 (s); }
    juce::Font font (float h, bool bold = false) { return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain)); }
    constexpr float A0 = juce::MathConstants<float>::pi * 1.25f, A1 = juce::MathConstants<float>::pi * 2.75f;
    const juce::Colour panelA (0xff3a3029), panelB (0xff2a221d), ink (0xffe9e1d6), dim (0xffb3a797), amber (0xfff2a13a), teal (0xff5fe0c0);
    void groove (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setColour (juce::Colours::black.withAlpha (0.55f)); g.drawRect (r, 1.2f);
        g.setColour (juce::Colours::white.withAlpha (0.07f)); g.drawRect (r.translated (1, 1), 1.0f);
    }
}

Look::Look()
{
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff241d18));
    setColour (juce::PopupMenu::textColourId, ink);
    setColour (juce::PopupMenu::headerTextColourId, amber);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff5a4634));
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff241d18));
    setColour (juce::TooltipWindow::textColourId, ink);
}
void Look::drawBubble (juce::Graphics& g, juce::BubbleComponent&, const juce::Point<float>&, const juce::Rectangle<float>& body)
{
    g.setColour (juce::Colour (0xf01b1512)); g.fillRoundedRectangle (body, 4);
    g.setColour (amber.withAlpha (0.6f)); g.drawRoundedRectangle (body.reduced (0.5f), 4, 1.0f);
}
void Look::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const juce::Point<float> c ((float) x + (float) w * 0.5f, (float) y + (float) h * 0.5f);
    const float R = (float) juce::jmin (w, h) * 0.5f - 4.0f;
    const float a = a0 + pos * (a1 - a0);
    juce::ignoreUnused (s);
    for (int i = 5; i > 0; --i) { g.setColour (juce::Colours::black.withAlpha (0.12f)); g.fillEllipse (c.x - R - (float) i * 0.5f, c.y - R + (float) i * 1.1f, (R + (float) i * 0.5f) * 2, (R + (float) i * 0.6f) * 2); }
    // faldon negro estriado
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3b3b3e), c.x, c.y - R, juce::Colour (0xff050505), c.x, c.y + R, false));
    g.fillEllipse (c.x - R, c.y - R, R * 2, R * 2);
    for (int i = 0; i < 40; ++i)
    {
        const float t = (float) i / 40.0f * juce::MathConstants<float>::twoPi;
        g.setColour (juce::Colours::white.withAlpha (i % 2 ? 0.05f : 0.0f));
        g.drawLine (juce::Line<float> ({ c.x + R * std::sin (t), c.y - R * std::cos (t) }, { c.x + R * 0.82f * std::sin (t), c.y - R * 0.82f * std::cos (t) }), 1.0f);
    }
    // tapa de aluminio
    const float rc = R * 0.66f;
    juce::ColourGradient cg (juce::Colour (0xfffafafa), c.x - rc * 0.4f, c.y - rc * 0.8f, juce::Colour (0xff7c7d80), c.x + rc * 0.5f, c.y + rc, true);
    cg.addColour (0.5, juce::Colour (0xffc7c8cb));
    g.setGradientFill (cg); g.fillEllipse (c.x - rc, c.y - rc, rc * 2, rc * 2);
    for (int i = 1; i < 6; ++i) { const float rr = rc * (float) i / 6.0f; g.setColour (juce::Colours::black.withAlpha (0.04f)); g.drawEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2, 0.6f); }
    g.setColour (juce::Colours::black.withAlpha (0.5f)); g.drawEllipse (c.x - rc, c.y - rc, rc * 2, rc * 2, 0.8f);
    // indicador
    g.setColour (juce::Colour (0xff111111));
    g.drawLine (juce::Line<float> ({ c.x + rc * 0.15f * std::sin (a), c.y - rc * 0.15f * std::cos (a) }, { c.x + R * 0.95f * std::sin (a), c.y - R * 0.95f * std::cos (a) }), juce::jmax (2.2f, R * 0.09f));
    g.setColour (juce::Colours::white); g.drawLine (juce::Line<float> ({ c.x + rc * 1.02f * std::sin (a), c.y - rc * 1.02f * std::cos (a) }, { c.x + R * 0.95f * std::sin (a), c.y - R * 0.95f * std::cos (a) }), 1.6f);
}

Knob::Knob (APVTS& s, const juce::String& id, bool stepped)
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (A0, A1, true);
    setPopupDisplayEnabled (! stepped, true, nullptr, 1000);
    setMouseDragSensitivity (stepped ? 120 : 220);
    att = std::make_unique<APVTS::SliderAttachment> (s, id, *this);
    if (auto* p = s.getParameter (id)) setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
}

Toggle::Toggle (APVTS& s, const juce::String& id, const juce::String& label, int ic) : juce::Button (label), icon (ic)
{
    setClickingTogglesState (true);
    att = std::make_unique<APVTS::ButtonAttachment> (s, id, *this);
}
void Toggle::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (2);
    const bool on = getToggleState();
    g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillRoundedRectangle (r.translated (0, 1.5f), 2.5f);
    juce::ColourGradient kg (on ? juce::Colour (0xfffff2d6) : juce::Colour (over ? 0xffdcd6cc : 0xffcbc5bb), r.getX(), r.getY(),
                             on ? juce::Colour (0xffe8b86a) : juce::Colour (0xff8f8a82), r.getX(), r.getBottom(), false);
    g.setGradientFill (kg); g.fillRoundedRectangle (r.translated (0, down ? 1.0f : 0.0f), 2.5f);
    g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawRoundedRectangle (r, 2.5f, 0.9f);
    g.setColour (juce::Colour (0xff2b241e));
    const auto c = r.getCentre();
    if (icon == 1) { g.drawEllipse (c.x - 6, c.y - 6, 12, 12, 1.6f); g.drawLine (c.x - 8, c.y + 8, c.x + 8, c.y - 8, 1.6f); }
    else if (icon == 2)
    {
        juce::Path p; p.addCentredArc (c.x, c.y + 1, 6, 6, 0, 0.6f, juce::MathConstants<float>::twoPi - 0.6f, true);
        g.setColour (on ? juce::Colour (0xffb85a10) : juce::Colour (0xff2b241e));
        g.strokePath (p, juce::PathStrokeType (1.7f)); g.drawLine (c.x, c.y - 7, c.x, c.y - 1, 1.7f);
    }
    else { g.setFont (font (12.0f, true)); g.drawText (getButtonText(), r, juce::Justification::centred); }
}

void Display::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1);
    g.setColour (juce::Colour (0xff120d0a)); g.fillRoundedRectangle (r, 3);
    g.setColour (juce::Colours::black); g.drawRoundedRectangle (r, 3, 1.4f);
    g.setColour (juce::Colours::white.withAlpha (0.06f)); g.drawHorizontalLine ((int) r.getBottom() - 1, r.getX() + 2, r.getRight() - 2);
    g.setColour (amber.withAlpha (0.25f)); g.setFont (font (fontSize, true));
    g.drawText (text, r.translated (0, 0.6f).expanded (0.8f), juce::Justification::centred);
    g.setColour (amber); g.drawText (text, r, juce::Justification::centred);
}

//==============================================================================
void tmicui::drawMic (juce::Graphics& g, juce::Rectangle<float> area, int model)
{
    const auto& m = tmic::models()[(size_t) juce::jlimit (0, (int) tmic::models().size() - 1, model)];
    const float W = area.getWidth(), H = area.getHeight(), cx = area.getCentreX(), y0 = area.getY();
    auto metal = [&g] (juce::Rectangle<float> r, juce::Colour base, float corner)
    {
        juce::ColourGradient mg (base.darker (0.6f), r.getX(), 0, base.darker (0.5f), r.getRight(), 0, false);
        mg.addColour (0.18, base.brighter (0.35f)); mg.addColour (0.32, base.brighter (0.6f)); mg.addColour (0.55, base); mg.addColour (0.85, base.darker (0.25f));
        g.setGradientFill (mg); g.fillRoundedRectangle (r, corner);
        g.setColour (juce::Colours::black.withAlpha (0.4f)); g.drawRoundedRectangle (r, corner, 0.8f);
    };
    auto mesh = [&g] (juce::Path shape, juce::Colour base, float step)
    {
        const auto r = shape.getBounds();
        juce::ColourGradient mg (base.darker (0.55f), r.getX(), 0, base.darker (0.5f), r.getRight(), 0, false);
        mg.addColour (0.3, base.brighter (0.5f)); mg.addColour (0.6, base);
        g.setGradientFill (mg); g.fillPath (shape);
        g.saveState(); g.reduceClipRegion (shape);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        for (float d = -r.getHeight(); d < r.getWidth() + r.getHeight(); d += step)
        {
            g.drawLine (r.getX() + d, r.getY(), r.getX() + d - r.getHeight(), r.getBottom(), 0.9f);
            g.drawLine (r.getX() + d, r.getY(), r.getX() + d + r.getHeight(), r.getBottom(), 0.9f);
        }
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.25f), r.getX(), r.getY(), juce::Colours::transparentWhite, r.getX(), r.getCentreY(), false));
        g.fillPath (shape);
        g.restoreState();
        g.setColour (juce::Colours::black.withAlpha (0.45f)); g.strokePath (shape, juce::PathStrokeType (1.0f));
    };
    auto badge = [&g] (float x, float y, float s, juce::Colour col)
    {
        juce::Path d; d.startNewSubPath (x, y - s); d.lineTo (x + s * 1.3f, y); d.lineTo (x, y + s); d.lineTo (x - s * 1.3f, y); d.closeSubPath();
        g.setColour (col.withAlpha (0.9f)); g.strokePath (d, juce::PathStrokeType (1.4f));
        g.setFont (font (s * 0.95f, true)); g.drawText ("T", juce::Rectangle<float> (x - s, y - s, s * 2, s * 2), juce::Justification::centred);
    };
    // sombra en el piso
    g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillEllipse (cx - W * 0.22f, y0 + H * 0.93f, W * 0.44f, H * 0.04f);
    switch (m.body)
    {
        case tmic::TubeLdc: case tmic::GoldTube:
        {
            const bool gold = m.body == tmic::GoldTube;
            const bool black = juce::String (m.name) == "TM-V6";      // cuerpo negro con rejilla cromada
            const juce::Colour bodyC = gold ? juce::Colour (0xff8a7340) : black ? juce::Colour (0xff2a2a2c) : juce::Colour (0xffbfc0c2);
            const float bw = W * 0.4f;
            metal ({ cx - bw * 0.42f, y0 + H * 0.88f, bw * 0.84f, H * 0.07f }, juce::Colour (0xff9a9b9e), 3);     // conector
            metal ({ cx - bw * 0.5f, y0 + H * 0.44f, bw, H * 0.46f }, bodyC, 6);                                    // cuerpo
            metal ({ cx - bw * 0.53f, y0 + H * 0.40f, bw * 1.06f, H * 0.05f }, juce::Colour (0xffd8d9dc), 3);      // aro
            juce::Path grille; grille.addRoundedRectangle (cx - bw * 0.55f, y0 + H * 0.08f, bw * 1.1f, H * 0.33f, bw * 0.3f, bw * 0.3f, true, true, false, false);
            mesh (grille, gold ? juce::Colour (0xff3a3632) : juce::Colour (0xffd2d3d5), 4.0f);
            badge (cx, y0 + H * 0.58f, W * 0.05f, gold ? juce::Colour (0xffeedcae) : black ? juce::Colour (0xffd8d8d8) : juce::Colour (0xff333333));
            break;
        }
        case tmic::FetLdc:
        {
            const bool dark = juce::String (m.name) == "TM-F2";
            const juce::Colour bodyC = dark ? juce::Colour (0xff3a3b3e) : juce::Colour (0xffc5c3bc);
            const float bw = W * 0.36f;
            metal ({ cx - bw * 0.5f, y0 + H * 0.46f, bw, H * 0.44f }, bodyC, 4);
            juce::Path grille; grille.addRoundedRectangle (cx - bw * 0.56f, y0 + H * 0.14f, bw * 1.12f, H * 0.34f, bw * 0.18f);
            mesh (grille, dark ? juce::Colour (0xff5a5b5f) : juce::Colour (0xffd8d6cf), 3.6f);
            metal ({ cx - bw * 0.58f, y0 + H * 0.29f, bw * 1.16f, H * 0.025f }, juce::Colour (0xffb0b1b4), 2);
            badge (cx, y0 + H * 0.58f, W * 0.045f, dark ? juce::Colour (0xffdddddd) : juce::Colour (0xff333333));
            break;
        }
        case tmic::StageDyn:
        {
            juce::Path b; b.startNewSubPath (cx - W * 0.10f, y0 + H * 0.36f); b.lineTo (cx + W * 0.10f, y0 + H * 0.36f); b.lineTo (cx + W * 0.06f, y0 + H * 0.92f); b.lineTo (cx - W * 0.06f, y0 + H * 0.92f); b.closeSubPath();
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff111111), cx - W * 0.1f, 0, juce::Colour (0xff050505), cx + W * 0.1f, 0, false));
            g.fillPath (b);
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.18f), cx - W * 0.05f, 0, juce::Colours::transparentWhite, cx, 0, false)); g.fillPath (b);
            juce::Path ball; ball.addEllipse (cx - W * 0.13f, y0 + H * 0.12f, W * 0.26f, H * 0.27f);
            mesh (ball, juce::Colour (0xffbfc0c2), 3.4f);
            metal ({ cx - W * 0.11f, y0 + H * 0.35f, W * 0.22f, H * 0.03f }, juce::Colour (0xff9a9b9e), 3);
            break;
        }
        case tmic::BroadcastDyn:
        {
            metal ({ cx - W * 0.17f, y0 + H * 0.42f, W * 0.34f, H * 0.4f }, juce::Colour (0xff2a2a2c), 8);
            juce::Path foam; foam.addRoundedRectangle (cx - W * 0.19f, y0 + H * 0.1f, W * 0.38f, H * 0.36f, W * 0.17f);
            g.setColour (juce::Colour (0xff1c1c1e)); g.fillPath (foam);
            juce::Random r (7);
            for (int i = 0; i < 500; ++i) { g.setColour (juce::Colours::white.withAlpha (0.04f + 0.04f * r.nextFloat())); const auto b = foam.getBounds(); g.fillEllipse (b.getX() + r.nextFloat() * b.getWidth(), b.getY() + r.nextFloat() * b.getHeight(), 2, 2); }
            g.saveState(); g.reduceClipRegion (foam); g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.12f), cx - W * 0.1f, 0, juce::Colours::transparentWhite, cx + W * 0.05f, 0, false)); g.fillPath (foam); g.restoreState();
            metal ({ cx - W * 0.26f, y0 + H * 0.55f, W * 0.52f, H * 0.05f }, juce::Colour (0xff3a3a3c), 4);     // yugo
            metal ({ cx - W * 0.04f, y0 + H * 0.8f, W * 0.08f, H * 0.13f }, juce::Colour (0xff3a3a3c), 3);
            break;
        }
        case tmic::LongDyn:
        {
            metal ({ cx - W * 0.12f, y0 + H * 0.08f, W * 0.24f, H * 0.84f }, juce::Colour (0xff7a7f86), 8);
            g.setColour (juce::Colour (0xff1b1c1e));
            for (int i = 0; i < 14; ++i) g.fillRoundedRectangle (cx - W * 0.09f, y0 + H * 0.11f + (float) i * H * 0.024f, W * 0.18f, H * 0.012f, 2);
            badge (cx, y0 + H * 0.62f, W * 0.045f, juce::Colour (0xff222222));
            break;
        }
        case tmic::RibbonModern:
        {
            metal ({ cx - W * 0.11f, y0 + H * 0.1f, W * 0.22f, H * 0.82f }, juce::Colour (0xff2b2b2d), 6);
            juce::Path slot; slot.addRoundedRectangle (cx - W * 0.085f, y0 + H * 0.13f, W * 0.17f, H * 0.34f, 6);
            mesh (slot, juce::Colour (0xff6d6e72), 3.2f);
            badge (cx, y0 + H * 0.62f, W * 0.04f, juce::Colour (0xffcccccc));
            break;
        }
        case tmic::RibbonVintage:
        {
            metal ({ cx - W * 0.03f, y0 + H * 0.66f, W * 0.06f, H * 0.26f }, juce::Colour (0xffa3a4a7), 3);
            juce::Path yoke; yoke.addCentredArc (cx, y0 + H * 0.42f, W * 0.27f, H * 0.26f, 0, juce::MathConstants<float>::pi * 0.62f, juce::MathConstants<float>::pi * 1.38f, true);
            g.setColour (juce::Colour (0xff9a9b9e)); g.strokePath (yoke, juce::PathStrokeType (W * 0.025f));
            juce::Path pill; pill.addRoundedRectangle (cx - W * 0.2f, y0 + H * 0.1f, W * 0.4f, H * 0.56f, W * 0.2f);
            mesh (pill, juce::Colour (0xffd4d5d8), 4.5f);
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            for (int i = 1; i < 6; ++i) g.drawHorizontalLine ((int) (y0 + H * 0.1f + H * 0.56f * (float) i / 6.0f), cx - W * 0.19f, cx + W * 0.19f);
            break;
        }
        default:   // RibbonBbc
        {
            metal ({ cx - W * 0.03f, y0 + H * 0.66f, W * 0.06f, H * 0.26f }, juce::Colour (0xff3a3a3c), 3);
            metal ({ cx - W * 0.25f, y0 + H * 0.15f, W * 0.04f, H * 0.5f }, juce::Colour (0xff4a4a4c), 2);
            metal ({ cx + W * 0.21f, y0 + H * 0.15f, W * 0.04f, H * 0.5f }, juce::Colour (0xff4a4a4c), 2);
            juce::Path oval; oval.addEllipse (cx - W * 0.2f, y0 + H * 0.12f, W * 0.4f, H * 0.52f);
            mesh (oval, juce::Colour (0xff8d8e91), 3.8f);
            break;
        }
    }
}

void MicView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    // estudio: pared de madera, piso y luz
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff9b7a5a), r.getX(), r.getY(), juce::Colour (0xff4a3627), r.getX(), r.getBottom() * 0.72f, false));
    g.fillRect (r.withHeight (r.getHeight() * 0.72f));
    for (float x = r.getX(); x < r.getRight(); x += 26) { g.setColour (juce::Colours::black.withAlpha (0.14f)); g.drawVerticalLine ((int) x, r.getY(), r.getBottom() * 0.72f); }
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff7d6a54), r.getX(), r.getBottom() * 0.72f, juce::Colour (0xff3b2f25), r.getX(), r.getBottom(), false));
    g.fillRect (r.withTop (r.getBottom() * 0.72f));
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.18f), r.getCentreX(), r.getY() + r.getHeight() * 0.3f, juce::Colours::transparentWhite, r.getX(), r.getY(), true));
    g.fillRect (r);
    drawMic (g, r.reduced (10, 6), (int) proc.apvts.getRawParameterValue ("model")->load());
    // flechas e info
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    juce::Path la; la.startNewSubPath (16, r.getBottom() - 34); la.lineTo (9, r.getBottom() - 26); la.lineTo (16, r.getBottom() - 18);
    juce::Path ra; ra.startNewSubPath (r.getRight() - 16, r.getBottom() - 34); ra.lineTo (r.getRight() - 9, r.getBottom() - 26); ra.lineTo (r.getRight() - 16, r.getBottom() - 18);
    g.strokePath (la, juce::PathStrokeType (2.0f)); g.strokePath (ra, juce::PathStrokeType (2.0f));
    g.setColour (juce::Colours::white.withAlpha (0.55f)); g.drawEllipse (8, 8, 16, 16, 1.2f);
    g.setFont (font (11.0f, true)); g.drawText ("i", 8, 8, 16, 16, juce::Justification::centred);
    g.setColour (juce::Colours::black); g.drawRect (r, 2.0f);
}
void MicView::mouseUp (const juce::MouseEvent& e)
{
    const int n = (int) tmic::models().size();
    const int cur = (int) proc.apvts.getRawParameterValue ("model")->load();
    int next = -1;
    if (e.x < getWidth() / 3) next = (cur - 1 + n) % n;
    else if (e.x > getWidth() * 2 / 3) next = (cur + 1) % n;
    if (next >= 0) if (auto* p = proc.apvts.getParameter ("model")) p->setValueNotifyingHost (p->convertTo0to1 ((float) next));
}

//==============================================================================
void PolarView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    // marco metalico
    g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (r.translated (0, 3), 14);
    juce::ColourGradient fg (juce::Colour (0xffd9d6cf), r.getX(), r.getY(), juce::Colour (0xff6c685f), r.getRight(), r.getBottom(), false);
    g.setGradientFill (fg); g.fillRoundedRectangle (r, 14);
    for (auto p : { juce::Point<float> (r.getX() + 9, r.getY() + 9), juce::Point<float> (r.getRight() - 9, r.getY() + 9), juce::Point<float> (r.getX() + 9, r.getBottom() - 9), juce::Point<float> (r.getRight() - 9, r.getBottom() - 9) })
    { g.setColour (juce::Colour (0xff3a3632)); g.fillEllipse (p.x - 3, p.y - 3, 6, 6); }
    auto scr = r.reduced (18);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff20262a), scr.getCentreX(), scr.getCentreY(), juce::Colour (0xff060708), scr.getX(), scr.getY(), true));
    g.fillRoundedRectangle (scr, 22);
    g.setColour (juce::Colours::black); g.drawRoundedRectangle (scr, 22, 2.0f);
    const auto c = scr.getCentre(); const float R = scr.getHeight() * 0.43f;
    const juce::Colour grid (0xff6f7f86);
    // anillos de dB y radios
    for (int k = 0; k <= 6; ++k)
    {
        const float rr = R * (1.0f - (float) k / 6.0f);
        g.setColour (grid.withAlpha (k == 0 ? 0.6f : 0.3f)); g.drawEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2, 0.8f);
        if (k > 0 && k < 6) { g.setFont (font (9.5f)); g.drawText (juce::String (-k * 6), juce::Rectangle<float> (c.x - 14, c.y - rr - 1, 28, 11), juce::Justification::centred); }
    }
    for (int d = 0; d < 360; d += 30)
    {
        const float a = (float) d * juce::MathConstants<float>::pi / 180.0f;
        g.setColour (grid.withAlpha (0.25f)); g.drawLine (c.x, c.y, c.x + R * std::sin (a), c.y - R * std::cos (a), 0.7f);
    }
    g.setColour (grid.withAlpha (0.7f)); g.setFont (font (10.0f));
    g.drawText ("0", juce::Rectangle<float> (c.x - 10, c.y - R - 13, 20, 12), juce::Justification::centred);
    g.drawText ("180", juce::Rectangle<float> (c.x - 14, c.y + R + 1, 28, 12), juce::Justification::centred);
    g.drawText ("90", juce::Rectangle<float> (c.x + R + 2, c.y - 6, 20, 12), juce::Justification::centredLeft);
    g.drawText ("90", juce::Rectangle<float> (c.x - R - 22, c.y - 6, 20, 12), juce::Justification::centredRight);
    auto curve = [&] (int pattern, juce::Colour col, float width)
    {
        juce::Path p;
        for (int d = 0; d <= 360; d += 2)
        {
            const float gdb = 20.0f * std::log10 (std::max (1.0e-3f, std::abs (tmic::MicChain::polar (pattern, (float) d))));
            const float rr = R * juce::jlimit (0.0f, 1.0f, (gdb + 36.0f) / 36.0f);
            const float a = (float) d * juce::MathConstants<float>::pi / 180.0f;
            const juce::Point<float> pt (c.x + rr * std::sin (a), c.y - rr * std::cos (a));
            if (d == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        p.closeSubPath();
        g.setColour (col.withAlpha (0.12f)); g.fillPath (p);
        g.setColour (col.withAlpha (0.35f)); g.strokePath (p, juce::PathStrokeType (width + 3.0f));
        g.setColour (col); g.strokePath (p, juce::PathStrokeType (width));
    };
    if (proc.apvts.getRawParameterValue ("dual")->load() > 0.5f) curve (proc.effectivePattern (true), teal, 1.6f);
    curve (proc.effectivePattern (false), amber, 2.0f);
    // posicion de la fuente segun el eje
    const bool rev = proc.apvts.getRawParameterValue ("rev")->load() > 0.5f;
    const float ax = proc.apvts.getRawParameterValue ("axis")->load();
    const float a = (rev ? 180.0f - ax : ax) * juce::MathConstants<float>::pi / 180.0f;
    const juce::Point<float> sp (c.x + R * 1.02f * std::sin (a), c.y - R * 1.02f * std::cos (a));
    g.setColour (juce::Colours::white.withAlpha (0.25f)); g.drawLine (c.x, c.y, sp.x, sp.y, 1.0f);
    g.setColour (juce::Colours::white); g.fillEllipse (sp.x - 4, sp.y - 4, 8, 8);
    g.setColour (grid.withAlpha (0.9f)); g.setFont (font (12.0f, true));
    g.drawText ("+", juce::Rectangle<float> (c.x - 40, scr.getBottom() - 20, 20, 16), juce::Justification::centred);
    g.drawText (U ("−"), juce::Rectangle<float> (c.x + 20, scr.getBottom() - 20, 20, 16), juce::Justification::centred);
    g.setColour (amber); g.setFont (font (11.0f, true));
    g.drawText (tmic::patternName (proc.effectivePattern (false)), juce::Rectangle<float> (scr.getX() + 14, scr.getY() + 10, 120, 14), juce::Justification::centredLeft);
    // reflejo del cristal
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.08f), scr.getX(), scr.getY(), juce::Colours::transparentWhite, scr.getCentreX(), scr.getCentreY(), false));
    g.fillRoundedRectangle (scr, 22);
}

//==============================================================================
TopMicEditor::TopMicEditor (TopMicProcessor& p)
    : AudioProcessorEditor (&p), proc (p), mic (p), polar (p),
      pattern (p.apvts, "pattern", true), filter (p.apvts, "filter", true), axis (p.apvts, "axis"), prox (p.apvts, "prox"), out (p.apvts, "out"), blend (p.apvts, "blend"),
      rev (p.apvts, "rev", "REV"), phase (p.apvts, "phase", {}, 1), power (p.apvts, "power", {}, 2), dual (p.apvts, "dual", "DUAL")
{
    setLookAndFeel (&look);
    tips.setLookAndFeel (&look);
    mic.setTooltip (U ("Clic a la izquierda / derecha para cambiar de micrófono"));
    nameBox.setTooltip (U ("Clic para elegir el micrófono"));
    nameBox.onClick = [this] { modelMenu ("model", nameBox); };
    sourceBox.small = true; sourceBox.fontSize = 13.0f;
    sourceBox.setTooltip (U ("SOURCE MIC: el micrófono con el que grabaste (su color se quita antes de aplicar el modelo)"));
    sourceBox.onClick = [this]
    {
        juce::PopupMenu m; m.addSectionHeader (U ("¿Con qué micrófono grabaste?"));
        const int cur = (int) proc.apvts.getRawParameterValue ("source")->load();
        for (int i = 0; i < (int) tmic::sources().size(); ++i) m.addItem (i + 1, U (tmic::sources()[(size_t) i].name), true, i == cur);
        juce::Component::SafePointer<TopMicEditor> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&sourceBox), [safe] (int r)
        { if (safe != nullptr && r > 0) if (auto* q = safe->proc.apvts.getParameter ("source")) q->setValueNotifyingHost (q->convertTo0to1 ((float) (r - 1))); });
    };
    micBBox.small = true; micBBox.fontSize = 13.0f;
    micBBox.setTooltip (U ("Micrófono B del modo DUAL (curva verde)"));
    micBBox.onClick = [this] { modelMenu ("modelB", micBBox); };
    presetBox.small = true; presetBox.fontSize = 12.0f;
    presetBox.setTooltip ("Presets");
    presetBox.onClick = [this]
    {
        juce::PopupMenu m;
        for (int i = 0; i < proc.getNumPrograms(); ++i) m.addItem (i + 1, proc.getProgramName (i), true, i == proc.getCurrentProgram());
        juce::Component::SafePointer<TopMicEditor> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetBox), [safe] (int r) { if (safe != nullptr && r > 0) safe->proc.setCurrentProgram (r - 1); });
    };
    pattern.setTooltip (U ("PATTERN: patrón polar (solo los que tiene el micrófono elegido)"));
    filter.setTooltip ("FILTER: filtro de graves del micrófono (60, 100 o 160 Hz)");
    axis.setTooltip (U ("AXIS: ángulo del micrófono respecto a la voz (fuera de eje = menos agudos)"));
    prox.setTooltip (U ("PROXIMITY: + más cerca (más graves), − más lejos"));
    out.setTooltip ("OUTPUT"); blend.setTooltip (U ("Mezcla entre el micrófono A y el B"));
    rev.setTooltip (U ("REV: usar la parte de atrás del micrófono"));
    phase.setTooltip ("Invertir fase"); power.setTooltip ("Encender / apagar"); dual.setTooltip (U ("DUAL: dos micrófonos a la vez"));
    for (juce::Component* c : { (juce::Component*) &mic, (juce::Component*) &polar, (juce::Component*) &nameBox, (juce::Component*) &sourceBox,
                                (juce::Component*) &micBBox, (juce::Component*) &presetBox, (juce::Component*) &pattern, (juce::Component*) &filter,
                                (juce::Component*) &axis, (juce::Component*) &prox, (juce::Component*) &out, (juce::Component*) &blend,
                                (juce::Component*) &rev, (juce::Component*) &phase, (juce::Component*) &power, (juce::Component*) &dual })
        addAndMakeVisible (c);
    setResizable (true, true);
    setResizeLimits (W * 2 / 3, H * 2 / 3, W * 3 / 2, H * 3 / 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) W / H);
    setSize (W, H);
    startTimerHz (30);
}
TopMicEditor::~TopMicEditor() { stopTimer(); tips.setLookAndFeel (nullptr); setLookAndFeel (nullptr); }

void TopMicEditor::modelMenu (const juce::String& id, juce::Component& target)
{
    juce::PopupMenu m;
    const int cur = (int) proc.apvts.getRawParameterValue (id)->load();
    auto group = [] (int body) { return body == tmic::TubeLdc || body == tmic::GoldTube ? 0 : body == tmic::FetLdc ? 1 : body <= tmic::LongDyn ? 2 : 3; };
    const juce::StringArray gn { U ("Válvula"), "FET", U ("Dinámicos"), "Cinta" };
    for (int gi = 0; gi < 4; ++gi)
    {
        m.addSectionHeader (gn[gi]);
        for (int i = 0; i < (int) tmic::models().size(); ++i)
            if (group (tmic::models()[(size_t) i].body) == gi)
                m.addItem (i + 1, juce::String (tmic::models()[(size_t) i].name) + "  -  " + U (tmic::models()[(size_t) i].desc), true, i == cur);
    }
    juce::Component::SafePointer<TopMicEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&target), [safe, id] (int r)
    { if (safe != nullptr && r > 0) if (auto* q = safe->proc.apvts.getParameter (id)) q->setValueNotifyingHost (q->convertTo0to1 ((float) (r - 1))); });
}

void TopMicEditor::resized()
{
    const float s = (float) getWidth() / (float) W;
    const auto T = juce::AffineTransform::scale (s);
    auto put = [&] (juce::Component& c, int x, int y, int w, int h) { c.setBounds (x, y, w, h); c.setTransform (T); };
    put (mic, 16, 16, 220, 448);
    put (nameBox, 16, 476, 220, 44);
    put (pattern, 296, 62, 76, 76);
    put (filter, 306, 186, 56, 56);
    put (axis, 306, 300, 56, 56);
    put (prox, 306, 418, 56, 56);
    put (polar, 486, 104, 344, 300);
    put (out, 622, 442, 72, 72);
    put (rev, 548, 462, 44, 26);
    put (phase, 726, 462, 44, 26);
    // pie
    put (sourceBox, 16, 562, 96, 26);
    put (dual, 128, 562, 54, 26);
    put (micBBox, 188, 562, 74, 26);
    put (blend, 266, 552, 44, 44);
    put (presetBox, 708, 562, 110, 26);
    put (power, 834, 562, 46, 26);
}

void TopMicEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale ((float) getWidth() / (float) W));
    // panel marron con textura
    g.setGradientFill (juce::ColourGradient (panelA, 0, 0, panelB, 0, (float) H, false));
    g.fillAll();
    juce::Random rng (12);
    for (int i = 0; i < 9000; ++i) { g.setColour ((rng.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (0.03f)); g.fillRect (rng.nextFloat() * W, rng.nextFloat() * H, 1.0f, 1.0f); }
    groove (g, { 4, 4, 250, 536 }); groove (g, { 254, 4, 168, 536 }); groove (g, { 422, 4, 474, 536 });
    g.setColour (juce::Colour (0xff1c1612)); g.fillRect (0, 542, W, H - 542);
    g.setColour (juce::Colours::black); g.drawHorizontalLine (542, 0, (float) W);
    // marca propia
    g.setColour (ink); g.setFont (font (30.0f).withExtraKerningFactor (0.12f));
    g.drawText ("TOP MIC", 486, 22, 220, 38, juce::Justification::centredLeft);
    g.setColour (dim); g.setFont (font (11.0f, true).withExtraKerningFactor (0.1f));
    g.drawText ("MICROPHONE COLLECTION", 488, 58, 220, 14, juce::Justification::centredLeft);
    g.setColour (dim.withAlpha (0.6f)); g.fillRect (708.0f, 26.0f, 1.2f, 46.0f);
    g.setColour (ink); g.setFont (font (18.0f));
    g.drawText ("Dj Pava", 718, 26, 160, 24, juce::Justification::centredLeft);
    g.setColour (dim); g.setFont (font (12.0f)); g.drawText ("Hm Music", 718, 48, 160, 18, juce::Justification::centredLeft);
    // etiquetas de la columna central
    auto label = [&g] (const juce::String& t, float cx, float y, float size = 11.5f, juce::Colour c = ink)
    { g.setColour (c); g.setFont (font (size, true)); g.drawText (t, juce::Rectangle<float> (cx - 60, y, 120, 14), juce::Justification::centred); };
    const float kc = 334;
    label ("PATTERN", kc, 30);
    // LED de patrones: los disponibles del modelo y el elegido
    {
        const auto& m = tmic::models()[(size_t) proc.apvts.getRawParameterValue ("model")->load()];
        const int eff = proc.effectivePattern (false);
        for (int i = 0; i < 5; ++i)
        {
            const float a = A0 + (A1 - A0) * (float) i / 4.0f, rr = 50.0f;
            const float x = kc + rr * std::sin (a), y = 100 - rr * std::cos (a);
            const bool avail = (m.patterns >> i) & 1u;
            if (i == eff) { g.setColour (amber.withAlpha (0.35f)); g.fillEllipse (x - 7, y - 7, 14, 14); }
            g.setColour (i == eff ? amber : avail ? juce::Colour (0xffd8cdbd) : juce::Colour (0xff5a4c40));
            g.fillEllipse (x - 3.5f, y - 3.5f, 7, 7);
        }
        g.setColour (dim); g.setFont (font (9.5f, true));
        g.drawText (tmic::patternName (eff), juce::Rectangle<float> (kc - 60, 146, 120, 12), juce::Justification::centred);
    }
    label ("FILTER", kc, 166);
    auto tick = [&g] (float cx, float cy, float r, float pos, const juce::String& t)
    {
        const float a = A0 + (A1 - A0) * pos;
        g.setColour (juce::Colour (0xffd8cdbd)); g.setFont (font (10.0f));
        g.drawText (t, juce::Rectangle<float> (cx + (r + 12) * std::sin (a) - 16, cy - (r + 12) * std::cos (a) - 6, 32, 12), juce::Justification::centred);
        g.fillEllipse (cx + (r + 2) * std::sin (a) - 1.2f, cy - (r + 2) * std::cos (a) - 1.2f, 2.4f, 2.4f);
    };
    tick (kc, 214, 26, 0.0f, "OFF"); tick (kc, 214, 26, 1.0f / 3.0f, "1"); tick (kc, 214, 26, 2.0f / 3.0f, "2"); tick (kc, 214, 26, 1.0f, "3");
    label ("AXIS", kc, 274);
    tick (kc, 328, 26, 0.0f, "0"); tick (kc, 328, 26, 0.25f, "45"); tick (kc, 328, 26, 0.5f, "90"); tick (kc, 328, 26, 0.75f, "135"); tick (kc, 328, 26, 1.0f, "180");
    label ("deg", kc, 362, 9.5f, dim);
    label ("PROXIMITY", kc, 392);
    tick (kc, 446, 26, 0.0f, "-100"); tick (kc, 446, 26, 0.5f, "0"); tick (kc, 446, 26, 1.0f, "+100");
    label ("%", kc, 480, 9.5f, dim);
    label ("OUTPUT", 658, 424);
    tick (658, 478, 34, 0.0f, "-12"); tick (658, 478, 34, 0.5f, "0"); tick (658, 478, 34, 1.0f, "+12");
    // pie
    g.setColour (dim); g.setFont (font (10.0f, true));
    g.drawText ("SOURCE MIC", 16, 546, 96, 14, juce::Justification::centred);
    g.drawText ("MIC B", 188, 546, 74, 14, juce::Justification::centred);
    g.drawText ("PRESET", 708, 546, 110, 14, juce::Justification::centred);
    g.drawText ("POWER", 834, 546, 46, 14, juce::Justification::centred);
    g.drawText ("INPUT", 316, 568, 40, 14, juce::Justification::centredLeft);
    // medidor de entrada
    const auto mr = juce::Rectangle<float> (360, 568, 330, 14);
    g.setColour (juce::Colour (0xff0d0a08)); g.fillRect (mr);
    g.setColour (juce::Colours::black); g.drawRect (mr, 1.0f);
    const float pos = juce::jlimit (0.0f, 1.0f, (20.0f * std::log10 (std::max (1.0e-6f, meter)) + 60.0f) / 60.0f);
    g.setGradientFill (juce::ColourGradient (teal.darker (0.3f), mr.getX(), 0, pos > 0.95f ? juce::Colour (0xffff5a4a) : teal, mr.getRight(), 0, false));
    g.fillRect (mr.reduced (2).withWidth ((mr.getWidth() - 4) * pos));
    const int marks[] { 60, 48, 36, 30, 24, 18, 12, 6, 3, 0 };
    g.setFont (font (9.0f)); g.setColour (dim);
    for (int mk : marks) { const float x = mr.getX() + mr.getWidth() * (1.0f - (float) mk / 60.0f); g.drawText (juce::String (mk), juce::Rectangle<float> (x - 10, 552, 20, 12), juce::Justification::centred); }
    // textos dinamicos de los visores
}

void TopMicEditor::timerCallback()
{
    const int m = (int) proc.apvts.getRawParameterValue ("model")->load();
    const juce::String nm = tmic::models()[(size_t) m].name;
    if (nameBox.text != nm) { nameBox.text = nm; nameBox.setTooltip (U (tmic::models()[(size_t) m].desc)); mic.setTooltip (U (tmic::models()[(size_t) m].desc) + U ("\n(clic a los lados para cambiar)")); nameBox.repaint(); mic.repaint(); repaint(); }
    const juce::String src = tmic::sources()[(size_t) proc.apvts.getRawParameterValue ("source")->load()].shortName;
    if (sourceBox.text != src) { sourceBox.text = src; sourceBox.repaint(); }
    const bool dualOn = proc.apvts.getRawParameterValue ("dual")->load() > 0.5f;
    const juce::String mb = dualOn ? juce::String (tmic::models()[(size_t) proc.apvts.getRawParameterValue ("modelB")->load()].name) : juce::String ("--");
    if (micBBox.text != mb) { micBBox.text = mb; micBBox.repaint(); }
    blend.setEnabled (dualOn); micBBox.setEnabled (dualOn);
    const auto pn = proc.getProgramName (proc.getCurrentProgram());
    if (presetBox.text != pn) { presetBox.text = pn; presetBox.repaint(); }
    const float pk = proc.inPeak.exchange (0.0f);
    meter = pk > meter ? pk : meter * 0.9f;
    polar.repaint();
    const float s = (float) getWidth() / (float) W;
    repaint (juce::Rectangle<int> (254, 4, 168, 160).transformedBy (juce::AffineTransform::scale (s)));
    repaint (juce::Rectangle<int> (300, 544, 400, 50).transformedBy (juce::AffineTransform::scale (s)));
}
