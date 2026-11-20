#include "EditorV3.h"
#include <array>

namespace tvc3
{
using APVTS = juce::AudioProcessorValueTreeState;
static juce::String U (const char* s) { return juce::String::fromUTF8 (s); }

namespace C
{
    const juce::Colour bg { 0xff0a0b0e }, top { 0xff15171c }, panelA { 0xff2b3039 }, panelB { 0xff1f232a }, groove { 0xff111317 },
                       text { 0xffdde1ea }, dim { 0xff8d94a1 }, faint { 0xff4a505b }, blue { 0xff86d0ff }, blueDeep { 0xff3a8fd8 },
                       dyn { 0xffff3f86 }, tone { 0xff2ec7ff }, space { 0xff9c5cff }, sfx { 0xffffc42e }, lcd { 0xff0d0f13 };
}
static juce::Font sans (float h, bool bold = false) { return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain)); }
static juce::Font spaced (float h) { auto f = sans (h); f.setExtraKerningFactor (0.08f); return f; }

static void groovePanel (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setGradientFill (juce::ColourGradient (C::panelA, r.getX(), r.getY(), C::panelB, r.getX(), r.getBottom(), false));
    g.fillRect (r);
    g.setColour (juce::Colours::white.withAlpha (0.05f)); g.fillRect (r.getX(), r.getY(), r.getWidth(), 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillRect (r.getRight() - 1.0f, r.getY(), 1.0f, r.getHeight());
}
static void lcd (juce::Graphics& g, juce::Rectangle<float> r, float rad = 8.0f)
{
    g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillRoundedRectangle (r.expanded (2), rad + 2);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1b2430), r.getX(), r.getY(), C::lcd, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, rad);
    g.setColour (juce::Colour (0xff3a4250)); g.drawRoundedRectangle (r, rad, 1.0f);
}

//==============================================================================
LnF::LnF()
{
    setColour (juce::PopupMenu::backgroundColourId, C::panelB);
    setColour (juce::PopupMenu::textColourId, C::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, C::blueDeep.withAlpha (0.5f));
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xf0171a20));
    setColour (juce::TooltipWindow::textColourId, C::text);
    setColour (juce::TooltipWindow::outlineColourId, C::blueDeep);
    setColour (juce::TextEditor::backgroundColourId, C::lcd);
}

void LnF::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const float R = std::min (w, h) * 0.5f - 3.0f;
    const float cx = x + w * 0.5f, cy = y + h * 0.5f;
    const bool small = R < 26.0f;
    const auto col = s.findColour (juce::Slider::rotarySliderFillColourId);
    // sombra y cuerpo exterior
    g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillEllipse (cx - R, cy - R + 4, 2 * R, 2 * R);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff353a44), cx, cy - R, juce::Colour (0xff15181d), cx, cy + R, false));
    g.fillEllipse (cx - R, cy - R, 2 * R, 2 * R);
    // arco de valor con brillo
    const float ar = R * 0.86f;
    juce::Path track; track.addCentredArc (cx, cy, ar, ar, 0, a0, a1, true);
    g.setColour (juce::Colours::black.withAlpha (0.45f)); g.strokePath (track, juce::PathStrokeType (small ? 2.0f : 4.0f));
    const float ang = a0 + pos * (a1 - a0);
    if (pos > 0.002f)
    {
        juce::Path arc; arc.addCentredArc (cx, cy, ar, ar, 0, a0, ang, true);
        g.setColour (col.withAlpha (0.25f)); g.strokePath (arc, juce::PathStrokeType (small ? 5.0f : 10.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (col); g.strokePath (arc, juce::PathStrokeType (small ? 2.0f : 3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    // marcas de escala
    if (! small)
        for (int i = 0; i <= 10; ++i)
        {
            const float a = a0 + (a1 - a0) * i / 10.0f;
            g.setColour (juce::Colours::white.withAlpha (0.18f));
            g.drawLine (cx + std::sin (a) * R * 0.62f, cy - std::cos (a) * R * 0.62f, cx + std::sin (a) * R * 0.7f, cy - std::cos (a) * R * 0.7f, 1.0f);
        }
    // tapa interior
    const float ir = R * (small ? 0.62f : 0.56f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c3038), cx, cy - ir, juce::Colour (0xff1a1d23), cx, cy + ir, false));
    g.fillEllipse (cx - ir, cy - ir, 2 * ir, 2 * ir);
    g.setColour (juce::Colours::white.withAlpha (0.06f)); g.drawEllipse (cx - ir, cy - ir, 2 * ir, 2 * ir, 1.0f);
    // punto indicador
    const float pr = small ? R * 0.74f : R * 0.72f;
    const float px = cx + std::sin (ang) * pr, py = cy - std::cos (ang) * pr;
    g.setColour (juce::Colours::white.withAlpha (0.3f)); g.fillEllipse (px - 5, py - 5, 10, 10);
    g.setColour (juce::Colours::white); g.fillEllipse (px - (small ? 2.0f : 3.5f), py - (small ? 2.0f : 3.5f), small ? 4.0f : 7.0f, small ? 4.0f : 7.0f);
}

void LnF::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle st, juce::Slider& s)
{
    const bool vert = st == juce::Slider::LinearVertical;
    const bool bip = (bool) s.getProperties()["bipolar"];
    const auto col = s.findColour (juce::Slider::trackColourId);
    if (vert)
    {
        const float cx = x + w * 0.5f, t = (float) y + 10, b = (float) (y + h) - 10;
        g.setColour (C::groove); g.fillRoundedRectangle (cx - 3.5f, t, 7, b - t, 3.5f);
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawRoundedRectangle (cx - 3.5f, t, 7, b - t, 3.5f, 1.0f);
        const float from = bip ? (t + b) * 0.5f : b;
        g.setColour (col.withAlpha (0.25f)); g.fillRoundedRectangle (cx - 3.0f, std::min (from, pos), 6, std::abs (pos - from), 3);
        g.setColour (col); g.fillRoundedRectangle (cx - 1.2f, std::min (from, pos), 2.4f, std::abs (pos - from), 1.2f);
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillEllipse (cx - 13, pos - 11, 26, 26);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3d424c), cx, pos - 12, juce::Colour (0xff1b1e24), cx, pos + 12, false));
        g.fillEllipse (cx - 12, pos - 12, 24, 24);
        g.setColour (juce::Colour (0xff9aa3b2)); g.drawEllipse (cx - 9, pos - 9, 18, 18, 1.4f);
    }
    else
    {
        const float cy = y + h * 0.5f, l = (float) x + 10, r = (float) (x + w) - 10;
        g.setColour (C::groove); g.fillRoundedRectangle (l, cy - 4, r - l, 8, 4);
        const float from = bip ? (l + r) * 0.5f : l;
        g.setColour (col); g.fillRoundedRectangle (std::min (from, pos), cy - 1.2f, std::abs (pos - from), 2.4f, 1.2f);
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (pos - 7, cy - 14, 14, 30, 4);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a505b), pos, cy - 14, juce::Colour (0xff22262d), pos, cy + 14, false));
        g.fillRoundedRectangle (pos - 6, cy - 14, 12, 28, 4);
        g.setColour (juce::Colour (0xffb7bfcc)); g.drawLine (pos, cy - 8, pos, cy + 8, 1.4f);
    }
}

void LnF::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool pill = (bool) b.getProperties()["pill"];
    const float rad = pill ? r.getHeight() * 0.5f : 6.0f;
    const bool on = b.getToggleState();
    g.setColour (on ? juce::Colour (0xff1c2a38) : (pill ? juce::Colour (0xff1a1d23) : C::top.brighter (over ? 0.08f : 0.0f).darker (down ? 0.2f : 0.0f)));
    g.fillRoundedRectangle (r, rad);
    g.setColour (on ? C::blue.withAlpha (0.8f) : juce::Colour (0xff3a404b));
    g.drawRoundedRectangle (r, rad, 1.2f);
}
void LnF::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (spaced (std::min (15.0f, b.getHeight() * 0.42f)));
    g.setColour (b.getToggleState() ? C::blue : (b.isEnabled() ? C::text : C::faint));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 0), juce::Justification::centred, 1);
}
void LnF::drawPopupMenuBackground (juce::Graphics& g, int w, int h) { g.fillAll (C::panelB); g.setColour (C::faint); g.drawRect (0, 0, w, h); }

//==============================================================================
struct Knob : juce::Component
{
    juce::Slider slider;
    std::unique_ptr<APVTS::SliderAttachment> att;
    juce::String title;
    Knob (APVTS& s, const juce::String& id, juce::String t, juce::Colour col = C::blue, bool reversed = false) : title (std::move (t))
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f;
        if (reversed) slider.setRotaryParameters (a1, a0, true); else slider.setRotaryParameters (a0, a1, true);
        slider.setColour (juce::Slider::rotarySliderFillColourId, col);
        slider.setVelocityModeParameters (0.25, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
        slider.setPopupDisplayEnabled (true, true, nullptr);
        att = std::make_unique<APVTS::SliderAttachment> (s, id, slider);
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (s.getParameter (id)))
            slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
        addAndMakeVisible (slider);
    }
    void resized() override { slider.setBounds (title.isEmpty() ? getLocalBounds() : getLocalBounds().withTrimmedBottom (30)); }
    void paint (juce::Graphics& g) override
    {
        if (title.isEmpty()) return;
        g.setFont (spaced (17.0f)); g.setColour (C::dim);
        g.drawText (title, getLocalBounds().removeFromBottom (28), juce::Justification::centred, false);
    }
};

// Caja negra con etiqueta y valor (arrastrar = cambiar, doble clic = reset)
struct ValueBox : juce::Component, juce::SettableTooltipClient
{
    APVTS& st; juce::String id, label; std::function<juce::String()> textFn; float startNorm = 0;
    ValueBox (APVTS& s, juce::String p, juce::String l) : st (s), id (std::move (p)), label (std::move (l)) {}
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1);
        g.setColour (juce::Colour (0xff0e1014)); g.fillRoundedRectangle (r, 5);
        g.setColour (juce::Colour (0xff353b46)); g.drawRoundedRectangle (r, 5, 1.0f);
        const auto v = textFn ? textFn() : st.getParameter (id)->getCurrentValueAsText();
        auto f1 = spaced (13.0f), f2 = sans (14.5f, true);
        const float w1 = juce::GlyphArrangement::getStringWidth (f1, label), w2 = juce::GlyphArrangement::getStringWidth (f2, v);
        const float x0 = r.getCentreX() - (w1 + 10 + w2) * 0.5f;
        g.setFont (f1); g.setColour (C::text); g.drawText (label, juce::Rectangle<float> (x0, r.getY(), w1 + 2, r.getHeight()), juce::Justification::centredLeft, false);
        g.setFont (f2); g.setColour (juce::Colours::white); g.drawText (v, juce::Rectangle<float> (x0 + w1 + 10, r.getY(), w2 + 4, r.getHeight()), juce::Justification::centredLeft, false);
    }
    void mouseDown (const juce::MouseEvent&) override { if (auto* p = st.getParameter (id)) { startNorm = p->getValue(); p->beginChangeGesture(); } }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (auto* p = st.getParameter (id)) p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, startNorm - e.getDistanceFromDragStartY() * (e.mods.isShiftDown() ? 0.001f : 0.005f)));
        repaint();
    }
    void mouseUp (const juce::MouseEvent&) override { if (auto* p = st.getParameter (id)) p->endChangeGesture(); }
    void mouseDoubleClick (const juce::MouseEvent&) override { if (auto* p = st.getParameter (id)) p->setValueNotifyingHost (p->getDefaultValue()); repaint(); }
};

// Rango de dos manijas: Low Cut (izquierda) y High Cut (derecha)
struct CutRange : juce::Component, juce::SettableTooltipClient
{
    APVTS& st; int drag = -1;
    explicit CutRange (APVTS& s) : st (s) { setTooltip (U ("FILTROS: la manija izquierda es el Low Cut (quita graves), la derecha el High Cut (quita agudos)")); }
    float track (float x01) const { return 30.0f + x01 * ((float) getWidth() - 60.0f); }
    void paint (juce::Graphics& g) override
    {
        auto* lc = st.getParameter ("eq_lowcut"); auto* hc = st.getParameter ("eq_hicut");
        const float a = track (lc->getValue() * 0.5f), b = track (0.5f + hc->getValue() * 0.5f), cy = getHeight() * 0.5f;
        g.setColour (C::groove); g.fillRoundedRectangle (30, cy - 4, getWidth() - 60.0f, 8, 4);
        g.setColour (C::blue); g.fillRoundedRectangle (30, cy - 1.2f, a - 30, 2.4f, 1.2f);
        g.fillRoundedRectangle (b, cy - 1.2f, getWidth() - 30 - b, 2.4f, 1.2f);
        for (float px : { a, b })
        {
            g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (px - 7, cy - 14, 14, 30, 4);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a505b), px, cy - 14, juce::Colour (0xff22262d), px, cy + 14, false));
            g.fillRoundedRectangle (px - 6, cy - 14, 12, 28, 4);
            g.setColour (juce::Colour (0xffb7bfcc)); g.drawLine (px, cy - 8, px, cy + 8, 1.4f);
        }
        // iconos de filtro
        g.setColour (C::dim);
        juce::Path hp; hp.startNewSubPath (6, cy + 8); hp.quadraticTo (8, cy - 8, 22, cy - 8); g.strokePath (hp, juce::PathStrokeType (1.6f));
        juce::Path lp; lp.startNewSubPath (getWidth() - 22.0f, cy - 8); lp.quadraticTo (getWidth() - 8.0f, cy - 8, getWidth() - 6.0f, cy + 8); g.strokePath (lp, juce::PathStrokeType (1.6f));
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        auto* lc = st.getParameter ("eq_lowcut"); auto* hc = st.getParameter ("eq_hicut");
        drag = std::abs (e.x - track (lc->getValue() * 0.5f)) < std::abs (e.x - track (0.5f + hc->getValue() * 0.5f)) ? 0 : 1;
        (drag == 0 ? lc : hc)->beginChangeGesture();
        mouseDrag (e);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const float x01 = juce::jlimit (0.0f, 1.0f, ((float) e.x - 30.0f) / ((float) getWidth() - 60.0f));
        if (drag == 0) st.getParameter ("eq_lowcut")->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, x01 * 2.0f));
        else if (drag == 1) st.getParameter ("eq_hicut")->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, (x01 - 0.5f) * 2.0f));
        repaint();
    }
    void mouseUp (const juce::MouseEvent&) override { if (drag >= 0) st.getParameter (drag == 0 ? "eq_lowcut" : "eq_hicut")->endChangeGesture(); drag = -1; }
    void mouseDoubleClick (const juce::MouseEvent&) override
    { for (auto* id : { "eq_lowcut", "eq_hicut" }) st.getParameter (id)->setValueNotifyingHost (st.getParameter (id)->getDefaultValue()); repaint(); }
};

//==============================================================================
// Iconos propios
static void glowStroke (juce::Graphics& g, const juce::Path& p, juce::Colour c, float w)
{
    g.setColour (c.withAlpha (0.18f)); g.strokePath (p, juce::PathStrokeType (w * 5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (c.withAlpha (0.35f)); g.strokePath (p, juce::PathStrokeType (w * 2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (c); g.strokePath (p, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
static void iconDynamics (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
{
    // forma de onda comprimida: barras verticales con envolvente
    juce::Path p;
    const int n = 17;
    for (int i = 0; i < n; ++i)
    {
        const float t = (float) i / (n - 1), x = r.getX() + t * r.getWidth();
        const float h = r.getHeight() * 0.5f * (0.35f + 0.65f * std::pow (std::sin (t * juce::MathConstants<float>::pi), 0.5f)) * (i % 2 ? 0.75f : 1.0f);
        p.startNewSubPath (x, r.getCentreY() - h); p.lineTo (x, r.getCentreY() + h);
    }
    glowStroke (g, p, c, 2.4f);
    juce::Path line; line.startNewSubPath (r.getX() - 6, r.getCentreY()); line.lineTo (r.getRight() + 6, r.getCentreY());
    glowStroke (g, line, c, 1.6f);
}
static void iconTone (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
{
    juce::Path ring; ring.addEllipse (r.reduced (4));
    glowStroke (g, ring, c, 2.2f);
    juce::Path bell; const auto in = r.reduced (16);
    bell.startNewSubPath (in.getX(), in.getCentreY() + 10);
    bell.cubicTo (in.getCentreX() - 8, in.getCentreY() + 10, in.getCentreX() - 8, in.getY() + 2, in.getCentreX(), in.getY() + 2);
    bell.cubicTo (in.getCentreX() + 8, in.getY() + 2, in.getCentreX() + 8, in.getCentreY() + 10, in.getRight(), in.getCentreY() + 10);
    glowStroke (g, bell, c, 2.4f);
    juce::Path base; base.startNewSubPath (in.getX(), in.getBottom() - 4); base.lineTo (in.getRight(), in.getBottom() - 4);
    glowStroke (g, base, c, 1.6f);
}
static void iconSpace (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
{
    const auto o = r.getCentre();
    juce::Path dot; dot.addEllipse (o.x - 5, o.y - 5, 10, 10);
    g.setColour (c); g.fillPath (dot);
    for (int k = 1; k <= 3; ++k)
    {
        juce::Path a; const float rr = 13.0f * k;
        a.addCentredArc (o.x, o.y, rr, rr * 0.82f, 0, -1.1f, 1.1f, true);
        juce::Path b; b.addCentredArc (o.x, o.y, rr, rr * 0.82f, 0, juce::MathConstants<float>::pi - 1.1f, juce::MathConstants<float>::pi + 1.1f, true);
        glowStroke (g, a, c.withAlpha (1.0f - 0.18f * k), 2.2f);
        glowStroke (g, b, c.withAlpha (1.0f - 0.18f * k), 2.2f);
    }
}
static void iconSfx (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
{
    // casete
    auto b = r.withSizeKeepingCentre (r.getWidth(), r.getHeight() * 0.68f);
    juce::Path body; body.addRoundedRectangle (b, 6);
    glowStroke (g, body, c, 2.2f);
    for (float cx : { b.getX() + b.getWidth() * 0.3f, b.getX() + b.getWidth() * 0.7f })
    {
        juce::Path reel; reel.addEllipse (cx - 9, b.getY() + b.getHeight() * 0.42f - 9, 18, 18);
        glowStroke (g, reel, c, 2.0f);
    }
    juce::Path win; win.addRoundedRectangle (b.getX() + 14, b.getBottom() - 16, b.getWidth() - 28, 10, 3);
    glowStroke (g, win, c, 1.6f);
}

//==============================================================================
// Tarjeta de modulo: power, nombre de color, icono, < estilo >, puntos y perilla pequena
struct Card : juce::Component, juce::SettableTooltipClient
{
    struct Style { const char* name; std::vector<std::pair<const char*, float>> v; };
    TopVocalChainProcessor& proc;
    juce::String key, title; juce::Colour col;
    std::vector<Style> styles;
    std::function<bool()> isOn; std::function<void (bool)> setOn;
    std::function<void (juce::Graphics&, juce::Rectangle<float>, juce::Colour)> icon;
    std::unique_ptr<Knob> trim; juce::String trimId, trimLabel;
    int defaultIndex = 0;
    Card (TopVocalChainProcessor& p, juce::String k, juce::String t, juce::Colour c) : proc (p), key (std::move (k)), title (std::move (t)), col (c) {}
    void setTrim (const juce::String& id, const juce::String& label)
    {
        trimId = id; trimLabel = label;
        trim = std::make_unique<Knob> (proc.apvts, id, juce::String(), C::dim);
        addAndMakeVisible (*trim);
    }
    int index() const { return juce::jlimit (0, (int) styles.size() - 1, (int) proc.apvts.state.getProperty ("v3_" + key, defaultIndex)); }
    void select (int i)
    {
        i = (i + (int) styles.size()) % (int) styles.size();
        proc.undoManager.beginNewTransaction();
        proc.apvts.state.setProperty ("v3_" + key, i, nullptr);
        for (auto& [id, val] : styles[(size_t) i].v) if (auto* prm = proc.apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (val));
        if (setOn && ! isOn()) setOn (true);
        repaint();
    }
    juce::Rectangle<float> powerR() const { return { 18, 16, 34, 34 }; }
    juce::Rectangle<float> leftR() const { return { 16, 70, 44, 60 }; }
    juce::Rectangle<float> rightR() const { return { (float) getWidth() - 60, 70, 44, 60 }; }
    void resized() override { if (trim) trim->setBounds (getWidth() - 50, getHeight() - 62, 36, 36); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (powerR().expanded (6).contains (e.position) && setOn) { proc.undoManager.beginNewTransaction(); setOn (! isOn()); }
        else if (leftR().contains (e.position)) select (index() - 1);
        else if (rightR().contains (e.position)) select (index() + 1);
        repaint();
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1b1e24), 0, 0, juce::Colour (0xff121418), 0, r.getHeight(), false));
        g.fillRect (r);
        const bool on = isOn ? isOn() : true;
        // reflejo diagonal sutil
        juce::Path sheen; sheen.startNewSubPath (r.getWidth() * 0.55f, 0); sheen.lineTo (r.getWidth(), 0); sheen.lineTo (r.getWidth(), r.getHeight() * 0.35f); sheen.closeSubPath();
        g.setColour (juce::Colours::white.withAlpha (0.02f)); g.fillPath (sheen);
        if (on) { g.setGradientFill (juce::ColourGradient (col.withAlpha (0.16f), r.getCentreX(), 100, col.withAlpha (0.0f), r.getCentreX() + 110, 100, true)); g.fillEllipse (r.getCentreX() - 110, -10, 220, 220); }
        // power
        {
            const auto c = powerR().getCentre();
            juce::Path pw; pw.addCentredArc (c.x, c.y, 11, 11, 0, 0.6f, juce::MathConstants<float>::twoPi - 0.6f, true);
            pw.startNewSubPath (c.x, c.y - 14); pw.lineTo (c.x, c.y - 3);
            g.setColour (on ? col : C::faint); g.strokePath (pw, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setFont (spaced (21.0f)); g.setColour (on ? col : C::faint);
        g.drawText (title, r.withHeight (66), juce::Justification::centred, false);
        if (icon) icon (g, juce::Rectangle<float> (r.getCentreX() - 44, 66, 88, 66), on ? col : C::faint);
        for (int s = 0; s < 2; ++s)
        {
            const auto a = (s == 0 ? leftR() : rightR()).getCentre();
            juce::Path chev; chev.startNewSubPath (a.x + (s ? -5.0f : 5.0f), a.y - 11); chev.lineTo (a.x + (s ? 6.0f : -6.0f), a.y); chev.lineTo (a.x + (s ? -5.0f : 5.0f), a.y + 11);
            g.setColour (C::text.withAlpha (0.8f)); g.strokePath (chev, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setFont (spaced (24.0f)); g.setColour (on ? C::text.withAlpha (0.75f) : C::faint);
        g.drawText (styles[(size_t) index()].name, juce::Rectangle<float> (0, 136, r.getWidth(), 30), juce::Justification::centred, false);
        const int n = (int) styles.size();
        for (int i = 0; i < n; ++i)
        {
            g.setColour (i == index() ? C::text : C::faint);
            g.fillEllipse (r.getCentreX() - (n - 1) * 5.0f + i * 10.0f - 2.5f, 170, 5, 5);
        }
        if (trim)
        {
            g.setFont (sans (11.0f)); g.setColour (C::dim);
            g.drawText (trimLabel.isNotEmpty() ? trimLabel : proc.apvts.getParameter (trimId)->getCurrentValueAsText(),
                        juce::Rectangle<float> (r.getWidth() - 80, r.getHeight() - 24, 76, 16), juce::Justification::centred, false);
        }
    }
};

// Barra de medidor vertical (dB)
static float dbTo01 (float db) { static const float marks[] { 0, -3, -6, -12, -24, -48, -96 }; for (int i = 0; i < 6; ++i) if (db >= marks[i + 1]) return 1.0f - (i + (marks[i] - db) / (marks[i] - marks[i + 1])) / 6.0f; return 0.0f; }
static void vbar (juce::Graphics& g, juce::Rectangle<float> r, float v01, juce::Colour c)
{
    g.setColour (C::groove); g.fillRoundedRectangle (r, r.getWidth() * 0.5f);
    g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawRoundedRectangle (r, r.getWidth() * 0.5f, 1.0f);
    const float h = r.getHeight() * juce::jlimit (0.0f, 1.0f, v01);
    if (h > 1) { g.setColour (c); g.fillRoundedRectangle (r.withTrimmedTop (r.getHeight() - h).reduced (1.5f, 1.5f), r.getWidth() * 0.4f); }
}
static void vbarTop (juce::Graphics& g, juce::Rectangle<float> r, float grDb, juce::Colour c)
{
    g.setColour (C::groove); g.fillRoundedRectangle (r, r.getWidth() * 0.5f);
    g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawRoundedRectangle (r, r.getWidth() * 0.5f, 1.0f);
    const float frac = 1.0f - dbTo01 (-std::abs (grDb));
    if (frac > 0.002f) { g.setColour (c); g.fillRoundedRectangle (r.withHeight (r.getHeight() * frac).reduced (1.5f, 1.5f), r.getWidth() * 0.4f); }
}

// Analizador del EQ
struct Spectrum
{
    static constexpr int order = 11, N = 1 << order, points = 80;
    juce::dsp::FFT fft { order };
    std::vector<float> win = std::vector<float> ((size_t) N), buf = std::vector<float> ((size_t) N * 2), l = std::vector<float> ((size_t) N), r = std::vector<float> ((size_t) N);
    std::array<float, points> sm {};
    Spectrum() { for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * i / (N - 1)); sm.fill (-120); }
    void update (const AudioTap& tap, double sr)
    {
        tap.read (l.data(), r.data(), N);
        for (int i = 0; i < N; ++i) buf[(size_t) i] = 0.5f * (l[(size_t) i] + r[(size_t) i]) * win[(size_t) i];
        std::fill (buf.begin() + N, buf.end(), 0.0f);
        fft.performFrequencyOnlyForwardTransform (buf.data(), true);
        for (int p = 0; p < points; ++p)
        {
            const double f0 = 40.0 * std::pow (500.0, (double) p / points), f1 = 40.0 * std::pow (500.0, (double) (p + 1) / points);
            int b0 = juce::jlimit (1, N / 2 - 1, (int) (f0 / sr * N)), b1 = juce::jlimit (b0 + 1, N / 2, (int) (f1 / sr * N));
            float m = 0; for (int b = b0; b < b1; ++b) m = std::max (m, buf[(size_t) b]);
            const float db = juce::Decibels::gainToDecibels (m * 4.0f / N, -120.0f) + 4.5f * (float) std::log2 (std::sqrt (f0 * f1) / 1000.0);
            auto& s = sm[(size_t) p]; s = db > s ? s + (db - s) * 0.6f : s + (db - s) * 0.12f;
        }
    }
};

//==============================================================================
class MainView : public juce::Component
{
public:
    static constexpr int W = 1100, H = 700;
    TopVocalChainProcessor& proc;
    Card dyn, tone, space, sfx;
    juce::Slider gate;
    std::unique_ptr<APVTS::SliderAttachment> gateAtt;
    Knob comp, deess, reverb, delay, sfxAmt, enhancer, inKnob, outKnob;
    ValueBox ratioBox, focusBox, vrbDuck, dlyDuck, timeBox, rateBox;
    juce::OwnedArray<juce::Slider> eq;
    juce::OwnedArray<APVTS::SliderAttachment> eqAtt;
    CutRange cuts;
    juce::Slider spaceTone;
    std::unique_ptr<APVTS::SliderAttachment> spaceToneAtt;
    juce::TextButton undoBtn { "UNDO" }, redoBtn { "REDO" }, prevBtn { "<" }, nextBtn { ">" }, plusBtn { "+" }, assistBtn { "AI" }, bypassBtn { "BYPASS" },
                     spcPost { "SPC POST" }, fltPost { "FLT POST" }, msBtn { "ST" }, pingBtn { "PP" };
    std::vector<std::unique_ptr<APVTS::ButtonAttachment>> btnAtt;
    Spectrum spec;
    std::unique_ptr<juce::Component> assist;
    std::unique_ptr<juce::FileChooser> chooser;
    float inShown = -100, outShown = -100;
    const juce::Rectangle<float> presetR { 420, 16, 260, 42 }, eqLcd { 290, 268, 244, 128 }, spLcd { 564, 268, 244, 128 };

    static constexpr int px (int i) { return 2 + i * 274; }

    explicit MainView (TopVocalChainProcessor& p)
        : proc (p),
          dyn (p, "dyn", "DYNAMICS", C::dyn), tone (p, "tone", "TONE", C::tone), space (p, "space", "SPACE", C::space), sfx (p, "sfx", "SFX", C::sfx),
          comp (p.apvts, "comp_thr", "COMP", C::blue, true), deess (p.apvts, "deess_amt", "DE-ESS"),
          reverb (p.apvts, "rev_mix", "REVERB"), delay (p.apvts, "dly_mix", "DELAY"),
          sfxAmt (p.apvts, "sfx_amt", "SFX"), enhancer (p.apvts, "air_high", "ENHANCER"),
          inKnob (p.apvts, "in_gain", {}, C::dim), outKnob (p.apvts, "out_gain", {}, C::dim),
          ratioBox (p.apvts, "comp_ratio", "RATIO"), focusBox (p.apvts, "deess_freq", "FOCUS"),
          vrbDuck (p.apvts, "rev_duck", "VRB DUCK"), dlyDuck (p.apvts, "dly_duck", "DLY DUCK"),
          timeBox (p.apvts, "rev_decay", "TIME"), rateBox (p.apvts, "dly_div", "RATE"),
          cuts (p.apvts)
    {
        setupCards();
        for (auto* c : std::initializer_list<juce::Component*> { &dyn, &tone, &space, &sfx, &comp, &deess, &reverb, &delay, &sfxAmt, &enhancer, &inKnob, &outKnob,
                                                                  &ratioBox, &focusBox, &vrbDuck, &dlyDuck, &timeBox, &rateBox, &cuts })
            addAndMakeVisible (c);
        // GATE
        gate.setSliderStyle (juce::Slider::LinearVertical); gate.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        gate.setColour (juce::Slider::trackColourId, C::blue);
        gate.setTooltip (U ("GATE: silencia el ruido entre frases. Abajo del todo = apagado"));
        gateAtt = std::make_unique<APVTS::SliderAttachment> (p.apvts, "gate_thr", gate);
        gate.setDoubleClickReturnValue (true, -80.0);
        addAndMakeVisible (gate);
        // valores
        ratioBox.textFn = [this] { return juce::String (val ("comp_ratio"), 1) + ":1"; };
        focusBox.textFn = [this] { return juce::String (val ("deess_freq") / 1000.0f, 1) + " kHz"; };
        vrbDuck.textFn = [this] { return juce::String (juce::roundToInt (val ("rev_duck"))) + "%"; };
        dlyDuck.textFn = [this] { return juce::String (juce::roundToInt (val ("dly_duck"))) + "%"; };
        timeBox.textFn = [this] { return juce::String (val ("rev_decay"), 1) + "s"; };
        rateBox.textFn = [this] { return TopVocalChainProcessor::divisionNames()[(int) val ("dly_div")]; };
        comp.slider.setTooltip (U ("COMP: a la derecha = más compresión (baja el threshold)"));
        deess.slider.setTooltip (U ("DE-ESS: cuánto bajan las \"S\". FOCUS = en qué frecuencia"));
        deess.slider.onValueChange = [this] { setParam ("deess_on", val ("deess_amt") > 0.5f ? 1.0f : 0.0f); };
        reverb.slider.setTooltip (U ("Cantidad de reverb")); delay.slider.setTooltip (U ("Cantidad de delay"));
        sfxAmt.slider.setTooltip (U ("SFX: cantidad del efecto elegido arriba (Tape Sat, Tube, Radio, Lo-Fi, Crush)"));
        enhancer.slider.setTooltip (U ("ENHANCER: aire sedoso y armónicos arriba (hasta 20 kHz)"));
        enhancer.slider.onValueChange = [this] { setParam ("air_sparkle", val ("air_high") * 0.6f); };
        inKnob.slider.setTooltip (U ("Ganancia de entrada")); outKnob.slider.setTooltip (U ("Volumen de salida"));
        ratioBox.setTooltip (U ("RATIO del compresor (arrastra arriba/abajo)"));
        vrbDuck.setTooltip (U ("La reverb baja mientras cantas y sube en las pausas"));
        dlyDuck.setTooltip (U ("El delay baja mientras cantas y sube en las pausas"));
        // EQ
        static const char* ids[] { "eq_low_gain", "eq_mid_gain", "eq_pres_gain", "eq_air_gain" };
        for (int i = 0; i < 4; ++i)
        {
            auto* s = eq.add (new juce::Slider (juce::Slider::LinearVertical, juce::Slider::NoTextBox));
            s->getProperties().set ("bipolar", true);
            s->setColour (juce::Slider::trackColourId, C::blue);
            s->setVelocityModeParameters (0.25, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
            s->setPopupDisplayEnabled (true, true, nullptr);
            eqAtt.add (new APVTS::SliderAttachment (p.apvts, ids[i], *s));
            s->setDoubleClickReturnValue (true, 0.0);
            addAndMakeVisible (s);
        }
        // tono del espacio: un solo control para el High Cut de reverb y delay
        spaceTone.setSliderStyle (juce::Slider::LinearHorizontal); spaceTone.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        spaceTone.setColour (juce::Slider::trackColourId, C::blue);
        spaceToneAtt = std::make_unique<APVTS::SliderAttachment> (p.apvts, "rev_hicut", spaceTone);
        spaceTone.onValueChange = [this] { setParam ("dly_hicut", juce::jlimit (1000.0f, 20000.0f, val ("rev_hicut"))); };
        spaceTone.setTooltip (U ("Tono del espacio: izquierda = reverb y delay oscuros, derecha = brillantes"));
        addAndMakeVisible (spaceTone);
        // botones
        for (auto* b : { &undoBtn, &redoBtn, &prevBtn, &nextBtn, &plusBtn, &assistBtn, &bypassBtn, &spcPost, &fltPost, &msBtn, &pingBtn }) addAndMakeVisible (b);
        for (auto* b : { &spcPost, &fltPost, &msBtn, &pingBtn }) b->getProperties().set ("pill", true);
        for (auto* b : { &bypassBtn, &spcPost, &fltPost, &pingBtn }) b->setClickingTogglesState (true);
        btnAtt.push_back (std::make_unique<APVTS::ButtonAttachment> (p.apvts, "global_bypass", bypassBtn));
        btnAtt.push_back (std::make_unique<APVTS::ButtonAttachment> (p.apvts, "sfx_pre_space", spcPost));
        btnAtt.push_back (std::make_unique<APVTS::ButtonAttachment> (p.apvts, "flt_post", fltPost));
        btnAtt.push_back (std::make_unique<APVTS::ButtonAttachment> (p.apvts, "dly_pingpong", pingBtn));
        spcPost.setTooltip (U ("SPC POST: la reverb y el delay van después del SFX"));
        fltPost.setTooltip (U ("FLT POST: los filtros Low/High Cut se aplican también después del SFX"));
        pingBtn.setTooltip (U ("Ping Pong: el eco salta de izquierda a derecha"));
        msBtn.setTooltip (U ("EQ en ST (estéreo) o solo en el centro (MID, la voz principal)"));
        msBtn.onClick = [this] { setParam ("eq_ms", (int) val ("eq_ms") == 1 ? 0.0f : 1.0f); };
        undoBtn.onClick = [this] { proc.undoManager.undo(); };
        redoBtn.onClick = [this] { proc.undoManager.redo(); };
        prevBtn.onClick = [this] { stepPreset (-1); };
        nextBtn.onClick = [this] { stepPreset (1); };
        plusBtn.onClick = [this] { savePreset(); };
        plusBtn.setTooltip (U ("Guardar tus ajustes como preset"));
        assistBtn.onClick = [this] { openAssist(); };
        assistBtn.setTooltip (U ("AI ASSIST: la IA escucha tu voz y propone ajustes"));
    }
    float val (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }
    void setParam (const char* id, float v) { if (auto* prm = proc.apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (v)); }
    void stepPreset (int d) { const int n = proc.getNumPrograms(); proc.undoManager.beginNewTransaction(); proc.setCurrentProgram ((proc.getCurrentProgram() + d + n) % n); repaint(); }
    void savePreset()
    {
        chooser = std::make_unique<juce::FileChooser> (U ("Guardar preset"), TopVocalChainProcessor::getUserPresetFolder().getChildFile ("Mi preset.tvcpreset"), "*.tvcpreset");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc) { const auto f = fc.getResult(); if (f != juce::File()) proc.saveUserPreset (f.withFileExtension ("tvcpreset")); });
    }
    void choosePreset()
    {
        juce::PopupMenu m;
        for (int i = 0; i < proc.getNumPrograms(); ++i) m.addItem (i + 1, proc.getProgramName (i), true, i == proc.getCurrentProgram());
        const auto user = TopVocalChainProcessor::getUserPresetFolder().findChildFiles (juce::File::findFiles, false, "*.tvcpreset");
        if (! user.isEmpty()) { m.addSeparator(); for (int i = 0; i < user.size(); ++i) m.addItem (1000 + i, user[i].getFileNameWithoutExtension()); }
        m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (presetR.toNearestInt())), [this, user] (int r)
        {
            proc.undoManager.beginNewTransaction();
            if (r >= 1000) proc.loadUserPreset (user[r - 1000]); else if (r > 0) proc.setCurrentProgram (r - 1);
            repaint();
        });
    }
    void setupCards()
    {
        dyn.icon = iconDynamics; tone.icon = iconTone; space.icon = iconSpace; sfx.icon = iconSfx;
        dyn.styles = { { "SMOOTH", { { "comp_type", 1 }, { "comp_ratio", 3 }, { "comp_att", 20 }, { "comp_rel", 150 } } },
                       { "PUNCHY", { { "comp_type", 2 }, { "comp_ratio", 4 }, { "comp_att", 3 }, { "comp_rel", 80 } } },
                       { "AGGRESSIVE", { { "comp_type", 2 }, { "comp_ratio", 8 }, { "comp_att", 1 }, { "comp_rel", 60 } } },
                       { "VINTAGE", { { "comp_type", 3 }, { "comp_ratio", 3 }, { "comp_att", 15 }, { "comp_rel", 200 } } },
                       { "CLEAN", { { "comp_type", 0 }, { "comp_ratio", 2.5f }, { "comp_att", 10 }, { "comp_rel", 120 } } },
                       { "MODERN", { { "comp_type", 8 }, { "comp_ratio", 4 }, { "comp_att", 5 }, { "comp_rel", 100 } } } };
        tone.styles = { { "NATURAL", { { "eq_low_gain", 0 }, { "eq_mid_gain", 0 }, { "eq_pres_gain", 0 }, { "eq_air_gain", 0 } } },
                        { "WARM", { { "eq_low_gain", 1.5f }, { "eq_mid_gain", -1 }, { "eq_pres_gain", 0.5f }, { "eq_air_gain", 1 } } },
                        { "BRIGHT", { { "eq_low_gain", -1 }, { "eq_mid_gain", -1.5f }, { "eq_pres_gain", 3.5f }, { "eq_air_gain", 4 } } },
                        { "AIRY", { { "eq_low_gain", -1 }, { "eq_mid_gain", 0 }, { "eq_pres_gain", 1.5f }, { "eq_air_gain", 6 } } },
                        { "DARK", { { "eq_low_gain", 2 }, { "eq_mid_gain", 0 }, { "eq_pres_gain", -2 }, { "eq_air_gain", -3 } } },
                        { "RADIO", { { "eq_low_gain", -4 }, { "eq_mid_gain", 2 }, { "eq_pres_gain", 4 }, { "eq_air_gain", -2 } } } };
        space.styles = { { "ROOM", { { "rev_on", 1 }, { "rev_type", 0 }, { "rev_decay", 0.9f }, { "rev_mix", 14 }, { "dly_on", 0 } } },
                         { "PLATE", { { "rev_on", 1 }, { "rev_type", 4 }, { "rev_decay", 1.6f }, { "rev_mix", 16 }, { "dly_on", 0 } } },
                         { "HALL", { { "rev_on", 1 }, { "rev_type", 2 }, { "rev_decay", 2.6f }, { "rev_mix", 18 }, { "dly_on", 0 } } },
                         { "MASSIVE", { { "rev_on", 1 }, { "rev_type", 6 }, { "rev_decay", 4.5f }, { "rev_mix", 22 }, { "dly_on", 1 }, { "dly_sync", 1 }, { "dly_div", 3 }, { "dly_mix", 12 }, { "dly_fb", 35 } } },
                         { "ECHO", { { "rev_on", 1 }, { "rev_type", 4 }, { "rev_decay", 1.2f }, { "rev_mix", 10 }, { "dly_on", 1 }, { "dly_sync", 1 }, { "dly_div", 5 }, { "dly_mix", 20 }, { "dly_fb", 40 } } },
                         { "SLAP", { { "rev_on", 1 }, { "rev_type", 0 }, { "rev_decay", 0.6f }, { "rev_mix", 8 }, { "dly_on", 1 }, { "dly_type", 3 }, { "dly_sync", 0 }, { "dly_time", 110 }, { "dly_mix", 22 }, { "dly_fb", 10 } } } };
        sfx.styles = { { "TAPE SAT", { { "sfx_type", 0 } } }, { "TUBE", { { "sfx_type", 1 } } }, { "RADIO", { { "sfx_type", 2 } } },
                       { "LO-FI", { { "sfx_type", 3 } } }, { "CRUSH", { { "sfx_type", 4 } } } };
        dyn.defaultIndex = 0; tone.defaultIndex = 0; space.defaultIndex = 1; sfx.defaultIndex = 0;
        dyn.isOn = [this] { return val ("comp_on") > 0.5f; }; dyn.setOn = [this] (bool o) { setParam ("comp_on", o); };
        tone.isOn = [this] { return val ("eq_on") > 0.5f; }; tone.setOn = [this] (bool o) { setParam ("eq_on", o); };
        space.isOn = [this] { return val ("rev_on") > 0.5f || val ("dly_on") > 0.5f; };
        space.setOn = [this] (bool o) { setParam ("rev_on", o); if (! o) setParam ("dly_on", 0); };
        sfx.isOn = [this] { return val ("sfx_on") > 0.5f; }; sfx.setOn = [this] (bool o) { setParam ("sfx_on", o); };
        dyn.setTrim ("comp_makeup", {}); tone.setTrim ("air_trim", {}); space.setTrim ("rev_width", "WIDTH"); sfx.setTrim ("sfx_trim", {});
        dyn.setTooltip (U ("DYNAMICS: compresor. Flechas = estilo, power = encender, perilla = makeup"));
        tone.setTooltip (U ("TONE: ecualizador. Flechas = carácter"));
        space.setTooltip (U ("SPACE: reverb y delay. Flechas = tipo de espacio"));
        sfx.setTooltip (U ("SFX: efectos de color (Tape Sat, Tube, Radio, Lo-Fi, Crush)"));
    }

    void resized() override
    {
        undoBtn.setBounds (276, 16, 64, 42); redoBtn.setBounds (340, 16, 64, 42);
        prevBtn.setBounds (684, 16, 44, 42); nextBtn.setBounds (728, 16, 44, 42);
        plusBtn.setBounds (782, 16, 44, 42);
        assistBtn.setBounds (900, 16, 50, 42); bypassBtn.setBounds (960, 16, 124, 42);
        Card* cards[] { &dyn, &tone, &space, &sfx };
        for (int i = 0; i < 4; ++i) cards[i]->setBounds (px (i), 74, 272, 180);
        // DYNAMICS
        gate.setBounds (16, 288, 44, 350);
        comp.setBounds (146, 264, 120, 150); ratioBox.setBounds (152, 418, 108, 30);
        deess.setBounds (146, 458, 120, 150); focusBox.setBounds (152, 612, 108, 30);
        // TONE
        for (int i = 0; i < 4; ++i) eq[i]->setBounds (px (1) + 18 + i * 60, 404, 54, 160);
        msBtn.setBounds (px (1) + 206, 588, 56, 20);
        cuts.setBounds (px (1) + 10, 612, 252, 38);
        // SPACE
        vrbDuck.setBounds ((int) spLcd.getX() + 4, (int) spLcd.getBottom() - 34, 116, 30);
        dlyDuck.setBounds ((int) spLcd.getCentreX() + 2, (int) spLcd.getBottom() - 34, 116, 30);
        reverb.setBounds (px (2) + 10, 404, 124, 156); delay.setBounds (px (2) + 138, 404, 124, 156);
        timeBox.setBounds (px (2) + 14, 566, 116, 30); rateBox.setBounds (px (2) + 142, 566, 92, 30); pingBtn.setBounds (px (2) + 236, 568, 30, 26);
        spaceTone.setBounds (px (2) + 40, 608, 192, 36);
        // SFX
        spcPost.setBounds (px (3) + 12, 266, 96, 28); fltPost.setBounds (px (3) + 164, 266, 96, 28);
        sfxAmt.setBounds (px (3) + 46, 296, 180, 190);
        enhancer.setBounds (px (3) + 56, 484, 160, 166);
        inKnob.setBounds (10, 657, 40, 40); outKnob.setBounds (W - 50, 657, 40, 40);
        if (assist) assist->setBounds (getLocalBounds().withSizeKeepingCentre (600, 440));
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (C::bg);
        // ---- cabecera
        {
            // logo propio: dos trazos en V + texto
            juce::Path v; v.startNewSubPath (22, 22); v.lineTo (36, 52); v.lineTo (50, 22);
            v.startNewSubPath (30, 22); v.lineTo (36, 36); v.lineTo (42, 22);
            g.setColour (C::tone.withAlpha (0.25f)); g.strokePath (v, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (C::tone); g.strokePath (v, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            const auto tf = spaced (21.0f);
            g.setFont (tf); g.setColour (C::text);
            const float tw = juce::GlyphArrangement::getStringWidth (tf, "VOCAL CHAIN");
            g.drawText ("VOCAL CHAIN", 62, 16, (int) tw + 6, 42, juce::Justification::centredLeft);
            auto badge = juce::Rectangle<float> (62 + tw + 10, 25, 40, 24);
            g.setColour (C::tone.withAlpha (0.25f)); g.fillRoundedRectangle (badge.expanded (3), 6);
            g.setColour (C::tone.darker (0.2f)); g.fillRoundedRectangle (badge, 4);
            g.setFont (sans (15.0f, true)); g.setColour (C::bg); g.drawText ("V3", badge, juce::Justification::centred);
            // preset
            g.setColour (C::top); g.fillRect (presetR);
            g.setColour (juce::Colour (0xff3a404b)); g.drawRect (presetR, 1.0f);
            g.setFont (spaced (19.0f)); g.setColour (C::text);
            g.drawText (proc.getProgramName (proc.getCurrentProgram()), presetR.reduced (16, 0), juce::Justification::centredLeft, true);
        }
        // ---- paneles inferiores
        for (int i = 0; i < 4; ++i) groovePanel (g, juce::Rectangle<float> ((float) px (i), 258, 272, 394));
        // DYNAMICS: escala, GR y DS
        g.setFont (spaced (14.0f)); g.setColour (C::dim);
        g.drawText ("GATE", 18, 266, 60, 18, juce::Justification::centredLeft);
        g.drawText ("GR", 82, 266, 30, 18, juce::Justification::centred);
        g.drawText ("DS", 108, 266, 30, 18, juce::Justification::centred);
        const float mTop = 300, mBot = 628;
        static const float marks[] { 0, -3, -6, -12, -24, -48, -96 };
        g.setFont (sans (11.5f));
        for (int i = 0; i < 7; ++i)
        {
            const float y = mTop + (mBot - mTop) * i / 6.0f;
            g.setColour (C::dim); g.drawText ((marks[i] < 0 ? "-" : "") + juce::String ((int) std::abs (marks[i])), 58, (int) y - 7, 24, 14, juce::Justification::centred);
            g.setColour (C::faint); g.fillRect (124.0f, y, 6.0f, 1.0f);
        }
        // nivel de entrada dentro del gate (barra azul)
        vbar (g, { 34, mTop, 8, mBot - mTop }, dbTo01 (inShown), C::blue.withAlpha (0.55f));
        vbarTop (g, { 92, mTop, 8, mBot - mTop }, proc.meters.compGrDb.load(), C::blue);
        vbarTop (g, { 116, mTop, 8, mBot - mTop }, proc.meters.deessDb.load(), C::blue);
        // TONE: analizador
        lcd (g, eqLcd);
        {
            auto p = eqLcd.reduced (8, 10).withTrimmedBottom (16);
            auto xOf = [&] (double f) { return p.getX() + (float) (std::log (f / 40.0) / std::log (500.0)) * p.getWidth(); };
            auto yOf = [&] (float db) { return p.getCentreY() - juce::jlimit (-14.0f, 14.0f, db) / 14.0f * p.getHeight() * 0.5f; };
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            for (double f : { 100.0, 250.0, 1000.0, 4000.0, 10000.0 }) g.fillRect (xOf (f), p.getY(), 1.0f, p.getHeight());
            for (float db : { -12.0f, 0.0f, 12.0f }) g.fillRect (p.getX(), yOf (db), p.getWidth(), 1.0f);
            g.setFont (sans (10.5f)); g.setColour (C::dim);
            g.drawText ("12 dB", (int) p.getX() + 2, (int) yOf (12) + 1, 40, 12, juce::Justification::left);
            g.drawText ("0 dB", (int) p.getX() + 2, (int) yOf (0) + 1, 40, 12, juce::Justification::left);
            g.drawText ("-12 dB", (int) p.getX() + 2, (int) yOf (-12) - 13, 40, 12, juce::Justification::left);
            juce::Path s;
            for (int i = 0; i < Spectrum::points; ++i)
            {
                const double f = 40.0 * std::pow (500.0, (i + 0.5) / Spectrum::points);
                const float y = p.getBottom() - juce::jlimit (0.0f, 1.0f, (spec.sm[(size_t) i] + 84.0f) / 78.0f) * p.getHeight();
                if (i == 0) s.startNewSubPath (xOf (f), y); else s.lineTo (xOf (f), y);
            }
            g.setColour (juce::Colours::white.withAlpha (0.25f)); g.strokePath (s, juce::PathStrokeType (1.0f));
            juce::Path c, fill;
            for (int i = 0; i <= 90; ++i)
            {
                const double f = 40.0 * std::pow (500.0, i / 90.0);
                const float y = yOf (proc.getEqMagnitudeDb (f));
                if (i == 0) { c.startNewSubPath (xOf (f), y); fill.startNewSubPath (xOf (f), p.getBottom()); fill.lineTo (xOf (f), y); }
                else { c.lineTo (xOf (f), y); fill.lineTo (xOf (f), y); }
            }
            fill.lineTo (p.getRight(), p.getBottom()); fill.closeSubPath();
            g.setGradientFill (juce::ColourGradient (C::blueDeep.withAlpha (0.35f), 0, p.getY(), C::blueDeep.withAlpha (0.05f), 0, p.getBottom(), false));
            g.fillPath (fill);
            g.setColour (C::blue); g.strokePath (c, juce::PathStrokeType (2.2f));
            for (double f : { 120.0, std::max (200.0, (double) val ("eq_mid_freq")), 4000.0, 12000.0 })
            { g.setColour (C::blue); g.fillEllipse (xOf (f) - 4, yOf (proc.getEqMagnitudeDb (f)) - 4, 8, 8); }
            g.setFont (spaced (11.5f)); g.setColour (C::blue);
            g.drawText ("ANALYZER", (int) eqLcd.getX() + 10, (int) eqLcd.getBottom() - 20, 80, 14, juce::Justification::left);
            g.setFont (sans (10.5f)); g.setColour (C::dim);
            for (auto [f, t] : { std::pair<double, const char*> { 250.0, "250Hz" }, { 1000.0, "1kHz" }, { 4000.0, "4kHz" }, { 10000.0, "10kHz" } })
                if (xOf (f) > eqLcd.getX() + 90) g.drawText (t, (int) xOf (f) - 20, (int) eqLcd.getBottom() - 20, 40, 14, juce::Justification::centred);
        }
        g.setFont (spaced (16.0f)); g.setColour (C::dim);
        static const char* eqL[] { "LOW", "MID", "HIGH", "AIR+" };
        for (int i = 0; i < 4; ++i) g.drawText (eqL[i], px (1) + 10 + i * 60, 566, 70, 22, juce::Justification::centred);
        // SPACE: medidores
        lcd (g, spLcd);
        {
            const bool rOn = val ("rev_on") > 0.5f, dOn = val ("dly_on") > 0.5f;
            const float lvl = dbTo01 (outShown);
            auto half = [&] (juce::Rectangle<float> a, const juce::String& t, bool on, float mix, float duck)
            {
                g.setFont (spaced (13.0f)); g.setColour (on ? C::tone : C::faint);
                g.drawText (t, a.withHeight (18).translated (8, 6), juce::Justification::centredLeft);
                const float y0 = a.getY() + 28, h = a.getHeight() - 72;
                const float in = lvl, out = on ? lvl * (0.4f + mix * 1.2f) * (1.0f - duck * 0.5f) : 0.0f;
                vbar (g, { a.getX() + 12, y0, 4, h }, in, C::tone); vbar (g, { a.getX() + 19, y0, 4, h }, in * 0.9f, C::tone);
                vbar (g, { a.getX() + 44, y0, 4, h }, on ? 1.0f - duck * in : 0.0f, C::faint.brighter (0.6f));
                vbar (g, { a.getX() + 51, y0, 4, h }, on ? 1.0f - duck * in * 0.9f : 0.0f, C::faint.brighter (0.6f));
                for (int k = 0; k < 2; ++k)
                    for (int s = 0; s < 14; ++s)
                    {
                        const float sy = y0 + h - (s + 1) * (h / 14.0f);
                        const bool lit = (float) s / 14.0f < juce::jlimit (0.0f, 1.0f, out) * (k ? 0.92f : 1.0f);
                        g.setColour (lit ? (s > 10 ? juce::Colour (0xffffa63d) : C::tone) : juce::Colour (0xff262c35));
                        g.fillRect (a.getX() + 74 + k * 9.0f, sy, 7.0f, h / 14.0f - 1.5f);
                    }
                g.setFont (sans (9.0f)); g.setColour (C::faint);
                g.drawText ("IN", (int) a.getX() + 8, (int) (y0 + h + 2), 22, 10, juce::Justification::centred);
                g.drawText ("DUCK", (int) a.getX() + 34, (int) (y0 + h + 2), 32, 10, juce::Justification::centred);
                g.drawText ("OUT", (int) a.getX() + 70, (int) (y0 + h + 2), 26, 10, juce::Justification::centred);
            };
            half (spLcd.withWidth (spLcd.getWidth() * 0.5f), "REVERB", rOn, val ("rev_mix") / 100.0f, val ("rev_duck") / 100.0f);
            half (spLcd.withTrimmedLeft (spLcd.getWidth() * 0.5f), "DELAY", dOn, val ("dly_mix") / 100.0f, val ("dly_duck") / 100.0f);
            g.setColour (juce::Colour (0xff3a4250)); g.fillRect (spLcd.getCentreX(), spLcd.getY() + 4, 1.0f, spLcd.getHeight() - 8);
        }
        // iconos del tono del espacio
        g.setColour (C::dim);
        { const float y = 626; juce::Path a; a.startNewSubPath ((float) px (2) + 14, y - 6); a.lineTo ((float) px (2) + 24, y - 6); a.lineTo ((float) px (2) + 32, y + 6); g.strokePath (a, juce::PathStrokeType (1.6f));
          juce::Path b; b.startNewSubPath ((float) px (2) + 240, y + 6); b.lineTo ((float) px (2) + 248, y - 6); b.lineTo ((float) px (2) + 258, y - 6); g.strokePath (b, juce::PathStrokeType (1.6f)); }
        // ---- pie
        g.setColour (C::bg); g.fillRect (0, 654, W, 46);
        g.setFont (spaced (18.0f)); g.setColour (C::text.withAlpha (0.8f));
        g.drawText ((val ("in_gain") >= 0 ? "+" : "") + juce::String (val ("in_gain"), 1) + " dB", 56, 660, 84, 34, juce::Justification::centredLeft);
        g.drawText ("INPUT", 140, 660, 70, 34, juce::Justification::centredLeft);
        g.drawText ("OUTPUT", 740, 660, 90, 34, juce::Justification::centredLeft);
        g.drawText ((val ("out_gain") >= 0 ? "+" : "") + juce::String (val ("out_gain"), 1) + " dB", 970, 660, 80, 34, juce::Justification::centredRight);
        auto dots = [&] (float x0, float x1, float db)
        {
            const int n = 40; const float lit = dbTo01 (db) * n;
            for (int i = 0; i < n; ++i)
                for (int rr = 0; rr < 3; ++rr)
                {
                    const float x = x0 + (x1 - x0) * i / (n - 1), y = 670.0f + rr * 5.0f;
                    const float t = (float) i / n;
                    g.setColour (i < lit ? (t > 0.85f ? juce::Colour (0xffff5b3a) : t > 0.65f ? juce::Colour (0xffffb23d) : C::tone) : juce::Colour (0xff262a31));
                    g.fillEllipse (x - 1.6f, y - 1.6f, 3.2f, 3.2f);
                }
        };
        dots (214, 420, inShown);
        dots (836, 960, outShown);
        g.setFont (spaced (15.0f)); g.setColour (C::dim);
        g.drawText ("DJ PAVA HM MUSIC", 440, 660, 290, 34, juce::Justification::centred);
    }
    void mouseUp (const juce::MouseEvent& e) override { if (presetR.contains (e.position)) choosePreset(); }

    void tick()
    {
        spec.update (proc.tapPost, proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0);
        const float in = proc.meters.inLevelDb.load(), out = proc.meters.outLevelDb.load();
        inShown = in > inShown ? in : inShown - 1.5f;
        outShown = out > outShown ? out : outShown - 1.5f;
        msBtn.setButtonText ((int) val ("eq_ms") == 1 ? "MID" : "ST");
        msBtn.setToggleState ((int) val ("eq_ms") == 1, juce::dontSendNotification);
        undoBtn.setEnabled (proc.undoManager.canUndo()); redoBtn.setEnabled (proc.undoManager.canRedo());
        for (auto* c : std::initializer_list<juce::Component*> { &dyn, &tone, &space, &sfx, &ratioBox, &focusBox, &vrbDuck, &dlyDuck, &timeBox, &rateBox, &cuts }) c->repaint();
        repaint (0, 0, W, 72);
        repaint (0, 258, W, H - 258);
        if (assist) assist->repaint();
    }
    void openAssist();
};

//==============================================================================
// AI ASSIST
struct AssistPanel : juce::Component
{
    TopVocalChainProcessor& proc;
    juce::TextButton apply { "APPLY" }, again { "AGAIN" }, close { "CLOSE" };
    std::function<void()> onClose;
    tvc::VoiceAnalysis::Report rep; bool shown = false;
    explicit AssistPanel (TopVocalChainProcessor& p) : proc (p)
    {
        for (auto* b : { &apply, &again, &close }) addAndMakeVisible (b);
        apply.setButtonText (U ("APLICAR")); again.setButtonText (U ("REPETIR")); close.setButtonText (U ("CERRAR"));
        apply.onClick = [this] { if (! (shown && rep.valid)) return; proc.undoManager.beginNewTransaction(); proc.applyReport (rep); if (onClose) onClose(); };
        again.onClick = [this] { shown = false; proc.startAnalysis(); };
        close.onClick = [this] { if (proc.analysis.isActive()) proc.analysis.cancel(); if (onClose) onClose(); };
        if (! proc.analysis.isReady()) proc.startAnalysis();
    }
    void resized() override
    {
        apply.setBounds (getWidth() - 330, getHeight() - 56, 124, 38);
        again.setBounds (getWidth() - 200, getHeight() - 56, 90, 38);
        close.setBounds (getWidth() - 104, getHeight() - 56, 86, 38);
    }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.65f)); g.fillRoundedRectangle (b.translated (0, 8), 12);
        g.setGradientFill (juce::ColourGradient (C::panelA, 0, 0, C::panelB, 0, b.getHeight(), false)); g.fillRoundedRectangle (b, 12);
        g.setColour (C::tone); g.drawRoundedRectangle (b.reduced (0.5f), 12, 1.4f);
        auto r = b.reduced (24, 18);
        g.setFont (spaced (22.0f)); g.setColour (C::tone);
        g.drawText ("AI ASSIST", r.removeFromTop (30), juce::Justification::centredLeft);
        g.setFont (sans (12.5f)); g.setColour (C::dim);
        g.drawText (U ("La IA escucha tu voz y propone ajustes. Tú decides si los aplicas."), r.removeFromTop (20), juce::Justification::centredLeft);
        r.removeFromTop (14);
        if (proc.analysis.isActive())
        {
            g.setFont (sans (16.0f, true)); g.setColour (C::text);
            g.drawText (U ("Canta normalmente unos 8 segundos..."), r.removeFromTop (28), juce::Justification::centredLeft);
            auto bar = r.removeFromTop (10).withTrimmedRight (30);
            g.setColour (C::groove); g.fillRoundedRectangle (bar, 5);
            g.setColour (C::tone); g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * proc.analysis.progress()), 5);
            return;
        }
        if (proc.analysis.isReady() && ! shown) { rep = proc.analysis.computeReport(); shown = true; }
        if (! shown) return;
        if (! rep.valid)
        {
            g.setFont (sans (15.0f)); g.setColour (C::text);
            g.drawFittedText (U ("No se detectó suficiente voz. Pulsa REPETIR y canta al menos 8 segundos."), r.removeFromTop (44).toNearestInt(), juce::Justification::centredLeft, 2);
            return;
        }
        auto left = r.removeFromLeft (220);
        auto hz = [] (float f) { return f >= 1000 ? juce::String (f / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (f)) + " Hz"; };
        auto item = [&] (const juce::String& l, const juce::String& v)
        {
            auto rr = left.removeFromTop (40);
            g.setFont (spaced (11.0f)); g.setColour (C::dim); g.drawText (l, rr.removeFromTop (14), juce::Justification::centredLeft);
            g.setFont (sans (15.5f, true)); g.setColour (C::text); g.drawText (v, rr, juce::Justification::centredLeft);
        };
        item (U ("VOZ"), rep.registro);
        item (U ("TONALIDAD"), TopVocalChainProcessor::keyNames()[rep.keyRoot] + (rep.minor ? U (" menor") : U (" mayor")));
        item (U ("RESONANCIA"), rep.cutResonance ? hz (rep.resonanceHz) : U ("Sin problema"));
        item (U ("SIBILANCIA"), rep.sibLabel + " (" + hz (rep.sibHz) + ")");
        item (U ("DINÁMICA"), rep.dynLabel);
        item (U ("RUIDO"), rep.noiseLabel);
        g.setFont (spaced (12.0f)); g.setColour (C::tone);
        g.drawText (U ("RECOMENDACIONES"), r.removeFromTop (18), juce::Justification::centredLeft);
        for (auto& line : rep.recommendations)
        {
            auto rr = r.removeFromTop (36);
            g.setColour (C::tone); g.fillEllipse (rr.getX(), rr.getY() + 6, 6, 6);
            g.setFont (sans (13.0f)); g.setColour (C::text);
            g.drawFittedText (line, rr.withTrimmedLeft (14).toNearestInt(), juce::Justification::topLeft, 2);
        }
    }
};

void MainView::openAssist()
{
    auto p = std::make_unique<AssistPanel> (proc);
    juce::Component::SafePointer<MainView> safeThis (this);
    p->onClose = [safeThis] { juce::MessageManager::callAsync ([safeThis] { if (safeThis != nullptr) safeThis->assist.reset(); }); };
    addAndMakeVisible (*p);
    assist = std::move (p);
    resized();
}

//==============================================================================
EditorV3::EditorV3 (TopVocalChainProcessor& p) : AudioProcessorEditor (&p)
{
    setLookAndFeel (&lnf);
    view = std::make_unique<MainView> (p);
    addAndMakeVisible (*view);
    setResizable (true, true);
    setResizeLimits (MainView::W / 2, MainView::H / 2, MainView::W * 2, MainView::H * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) MainView::W / MainView::H);
    setSize (MainView::W, MainView::H);
    startTimerHz (30);
}
EditorV3::~EditorV3() { stopTimer(); view.reset(); setLookAndFeel (nullptr); }
void EditorV3::tickNow() { view->tick(); }
void EditorV3::paint (juce::Graphics& g) { g.fillAll (C::bg); }
void EditorV3::resized()
{
    view->setBounds (0, 0, MainView::W, MainView::H);
    view->setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) MainView::W));
}
} // namespace tvc3
