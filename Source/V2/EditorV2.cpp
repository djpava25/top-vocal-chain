#include "EditorV2.h"
#include <array>

namespace tvc2
{
static int gLang = 0;   // 0 ES, 1 EN
juce::String T (const char* es, const char* en) { return juce::String::fromUTF8 (gLang == 0 ? es : en); }
static juce::String U (const char* s) { return juce::String::fromUTF8 (s); }
// Mayusculas que tambien convierten las vocales con tilde y la ene (no depende del idioma del sistema)
static juce::String upper (const juce::String& s)
{
    return s.toUpperCase().replace (U ("á"), U ("Á")).replace (U ("é"), U ("É")).replace (U ("í"), U ("Í"))
                          .replace (U ("ó"), U ("Ó")).replace (U ("ú"), U ("Ú")).replace (U ("ñ"), U ("Ñ"));
}

static juce::Font sans (float h, bool bold = false) { return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain)); }
static juce::Font mono (float h, bool bold = true)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), h, bold ? juce::Font::bold : juce::Font::plain));
}
static juce::ColourGradient neon (juce::Rectangle<float> r)
{
    return juce::ColourGradient (Palette::neonA, r.getX(), r.getCentreY(), Palette::neonB, r.getRight(), r.getCentreY(), false);
}
static void lcdBox (juce::Graphics& g, juce::Rectangle<float> r, float radius = 8.0f)
{
    g.setColour (Palette::lcd); g.fillRoundedRectangle (r, radius);
    g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawRoundedRectangle (r.translated (0, 1), radius, 1.5f);
    g.setColour (Palette::border); g.drawRoundedRectangle (r, radius, 1.0f);
}
static void panelBox (juce::Graphics& g, juce::Rectangle<float> r, float radius = 14.0f)
{
    g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (r.translated (0, 3), radius);
    g.setGradientFill (juce::ColourGradient (Palette::card.brighter (0.05f), 0, r.getY(), Palette::panel, 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, radius);
    g.setColour (Palette::border); g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.04f)); g.drawLine (r.getX() + radius, r.getY() + 1.0f, r.getRight() - radius, r.getY() + 1.0f, 1.0f);
}
static void led (juce::Graphics& g, juce::Point<float> c, float r, bool lit, juce::Colour col)
{
    if (lit) { g.setColour (col.withAlpha (0.25f)); g.fillEllipse (c.x - r * 2, c.y - r * 2, r * 4, r * 4); }
    g.setGradientFill (juce::ColourGradient (lit ? col.brighter (0.6f) : juce::Colour (0xff3a3a44), c.x - r * 0.4f, c.y - r * 0.5f,
                                             lit ? col.darker (0.3f) : juce::Colour (0xff1e1e24), c.x + r, c.y + r, true));
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
}

//==============================================================================
LnF::LnF()
{
    setColour (juce::PopupMenu::backgroundColourId, Palette::card);
    setColour (juce::PopupMenu::textColourId, Palette::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Palette::neonA.withAlpha (0.35f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::ComboBox::textColourId, Palette::text);
    setColour (juce::ComboBox::arrowColourId, Palette::dim);
    setColour (juce::TextButton::textColourOffId, Palette::dim);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::TooltipWindow::backgroundColourId, Palette::card);
    setColour (juce::TooltipWindow::textColourId, Palette::text);
    setColour (juce::TextEditor::backgroundColourId, Palette::lcd);
    setColour (juce::TextEditor::textColourId, Palette::text);
    setColour (juce::TextEditor::outlineColourId, Palette::border);
}

void LnF::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const float R = std::min (w, h) * 0.5f - 4.0f;
    const float cx = x + w * 0.5f, cy = y + h * 0.5f;
    const bool bip = (bool) s.getProperties()["bipolar"];
    // anillo de puntos: encendidos hasta el valor, con degradado morado -> rosa
    const int dots = 28;
    for (int i = 0; i <= dots; ++i)
    {
        const float t = (float) i / dots;
        const float a = a0 + t * (a1 - a0);
        const bool lit = bip ? ((t >= 0.5f && t <= pos + 0.001f) || (t <= 0.5f && t >= pos - 0.001f)) : t <= pos + 0.001f;
        const float px = cx + std::sin (a) * R, py = cy - std::cos (a) * R;
        const auto col = Palette::neonA.interpolatedWith (Palette::neonB, t);
        if (lit) { g.setColour (col.withAlpha (0.3f)); g.fillEllipse (px - 3.5f, py - 3.5f, 7, 7); }
        g.setColour (lit ? col : Palette::faint);
        const float d = lit ? 3.4f : 2.4f;
        g.fillEllipse (px - d * 0.5f, py - d * 0.5f, d, d);
    }
    // cuerpo de la perilla
    const float kr = R * 0.74f;
    g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillEllipse (cx - kr, cy - kr + 3, kr * 2, kr * 2);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff34343e), cx, cy - kr, juce::Colour (0xff131317), cx, cy + kr, false));
    g.fillEllipse (cx - kr, cy - kr, kr * 2, kr * 2);
    g.setColour (juce::Colours::white.withAlpha (0.08f)); g.drawEllipse (cx - kr + 0.5f, cy - kr + 0.5f, kr * 2 - 1, kr * 2 - 1, 1.0f);
    const float ir = kr * 0.82f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1c1c22), cx, cy - ir, juce::Colour (0xff26262e), cx, cy + ir, false));
    g.fillEllipse (cx - ir, cy - ir, ir * 2, ir * 2);
    // arco fino de valor dentro
    juce::Path arc; arc.addCentredArc (cx, cy, ir * 0.86f, ir * 0.86f, 0, bip ? (a0 + a1) * 0.5f : a0, a0 + pos * (a1 - a0), true);
    g.setColour (Palette::neonA.withAlpha (0.55f)); g.strokePath (arc, juce::PathStrokeType (1.6f));
    // punto indicador blanco
    const float ang = a0 + pos * (a1 - a0);
    const float px = cx + std::sin (ang) * kr * 0.84f, py = cy - std::cos (ang) * kr * 0.84f;
    g.setColour (juce::Colours::white.withAlpha (0.25f)); g.fillEllipse (px - 5, py - 5, 10, 10);
    g.setColour (juce::Colours::white); g.fillEllipse (px - 3, py - 3, 6, 6);
}

void LnF::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider& s)
{
    const float cx = x + w * 0.5f;
    const float top = (float) y + 6, bot = (float) (y + h) - 6;
    const int dots = 14;
    const float centreY = (top + bot) * 0.5f;
    const bool bip = (bool) s.getProperties()["bipolar"];
    for (int i = 0; i <= dots; ++i)
    {
        const float py = top + (bot - top) * (float) i / dots;
        const bool lit = bip ? ((py >= pos && py <= centreY) || (py <= pos && py >= centreY)) : py >= pos;
        g.setColour (lit ? Palette::neonA.interpolatedWith (Palette::neonB, 1.0f - (py - top) / (bot - top)) : Palette::faint);
        g.fillEllipse (cx - 1.6f, py - 1.6f, 3.2f, 3.2f);
    }
    g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillEllipse (cx - 11, pos - 9, 22, 22);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3a44), cx, pos - 10, juce::Colour (0xff141418), cx, pos + 10, false));
    g.fillEllipse (cx - 10, pos - 10, 20, 20);
    g.setColour (juce::Colours::white.withAlpha (0.12f)); g.drawEllipse (cx - 10, pos - 10, 20, 20, 1.0f);
    g.setColour (juce::Colours::white); g.fillEllipse (cx - 3, pos - 3, 6, 6);
}

void LnF::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const float rad = r.getHeight() * 0.5f;
    const bool on = b.getToggleState() || (bool) b.getProperties()["accent"];
    if (on)
    {
        g.setColour (Palette::neonB.withAlpha (0.18f)); g.fillRoundedRectangle (r.expanded (2), rad + 2);
        g.setGradientFill (neon (r)); g.fillRoundedRectangle (r, rad);
        if (down) { g.setColour (juce::Colours::black.withAlpha (0.2f)); g.fillRoundedRectangle (r, rad); }
    }
    else
    {
        g.setColour (Palette::cardHi.brighter (over ? 0.08f : 0.0f).darker (down ? 0.2f : 0.0f)); g.fillRoundedRectangle (r, rad);
        g.setColour (Palette::border); g.drawRoundedRectangle (r, rad, 1.0f);
    }
}

void LnF::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    const bool on = b.getToggleState() || (bool) b.getProperties()["accent"];
    g.setFont (sans (std::min (14.0f, b.getHeight() * 0.42f), true));
    g.setColour (on ? juce::Colours::white : (b.isEnabled() ? Palette::dim : Palette::faint));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (6, 0), juce::Justification::centred, 1);
}

void LnF::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (Palette::cardHi); g.fillRoundedRectangle (r, 8);
    g.setColour (Palette::border); g.drawRoundedRectangle (r, 8, 1.0f);
    juce::Path p; const float ax = w - 16.0f, ay = h * 0.5f;
    p.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (Palette::dim); g.fillPath (p);
}
juce::Font LnF::getComboBoxFont (juce::ComboBox& b) { return sans (std::min (14.0f, b.getHeight() * 0.45f), true); }
void LnF::positionComboBoxText (juce::ComboBox& b, juce::Label& l) { l.setBounds (8, 0, b.getWidth() - 28, b.getHeight()); l.setFont (getComboBoxFont (b)); }
void LnF::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (Palette::card);
    g.setColour (Palette::border); g.drawRect (0, 0, w, h);
}
void LnF::drawCallOutBoxBackground (juce::CallOutBox&, juce::Graphics& g, const juce::Path& path, juce::Image&)
{
    g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillPath (path, juce::AffineTransform::translation (0, 4));
    g.setColour (Palette::card); g.fillPath (path);
    g.setColour (Palette::neonA.withAlpha (0.6f)); g.strokePath (path, juce::PathStrokeType (1.2f));
}

//==============================================================================
// Perilla con su valor en el centro, nombre debajo y una "pastilla" con un dato extra
struct Knob : juce::Component
{
    juce::Slider slider;
    std::unique_ptr<APVTS::SliderAttachment> att;
    juce::String title; std::function<juce::String()> centre, pill;
    APVTS& st; juce::String id;
    Knob (APVTS& s, const juce::String& paramId, juce::String t) : st (s), id (paramId), title (std::move (t))
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        slider.setVelocityModeParameters (0.25, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
        att = std::make_unique<APVTS::SliderAttachment> (s, paramId, slider);
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (s.getParameter (paramId)))
        {
            slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
            const auto& r = p->getNormalisableRange();
            slider.getProperties().set ("bipolar", r.start < 0 && std::abs (r.start + r.end) < 0.01f * (r.end - r.start));
        }
        addAndMakeVisible (slider);
        slider.onValueChange = [this] { repaint(); };
    }
    juce::Rectangle<int> knobArea() const
    {
        auto top = getLocalBounds().withTrimmedBottom (52);
        const int s = std::min (top.getWidth(), top.getHeight());
        return top.withSizeKeepingCentre (s, s);
    }
    void resized() override { slider.setBounds (knobArea()); }
    void paintOverChildren (juce::Graphics& g) override
    {
        auto k = knobArea().toFloat();
        const auto c = centre ? centre() : st.getParameter (id)->getCurrentValueAsText();
        g.setFont (mono (12.0f)); g.setColour (Palette::text.withAlpha (0.9f));
        g.drawText (c, k.withSizeKeepingCentre (k.getWidth() * 0.6f, 16), juce::Justification::centred, false);
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().removeFromBottom (52).toFloat();
        g.setFont (sans (14.5f, true)); g.setColour (Palette::text);
        g.drawText (title, r.removeFromTop (24), juce::Justification::centred, false);
        if (pill)
        {
            const auto t = pill();
            const float w = juce::GlyphArrangement::getStringWidth (mono (10.5f), t) + 22.0f;
            auto pr = r.removeFromTop (22).withSizeKeepingCentre (w, 20);
            g.setColour (Palette::cardHi); g.fillRoundedRectangle (pr, 10);
            g.setColour (Palette::border); g.drawRoundedRectangle (pr, 10, 1.0f);
            g.setFont (mono (10.5f)); g.setColour (Palette::dim);
            g.drawText (t, pr, juce::Justification::centred, false);
        }
    }
};

// Caja de valor que se arrastra (arriba/abajo) y vuelve al valor por defecto con doble clic
struct ValueBox : juce::Component, juce::SettableTooltipClient
{
    APVTS& st; juce::String id; float startNorm = 0; std::function<juce::String()> textFn;
    bool accent = false;
    ValueBox (APVTS& s, juce::String p) : st (s), id (std::move (p)) {}
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1);
        lcdBox (g, r, 7);
        g.setFont (mono (12.5f)); g.setColour (accent ? Palette::neonB : Palette::text.withAlpha (0.9f));
        g.drawText (textFn ? textFn() : st.getParameter (id)->getCurrentValueAsText(), r, juce::Justification::centred, false);
    }
    void mouseDown (const juce::MouseEvent&) override { if (auto* p = st.getParameter (id)) { startNorm = p->getValue(); p->beginChangeGesture(); } }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (auto* p = st.getParameter (id))
            p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, startNorm - e.getDistanceFromDragStartY() * (e.mods.isShiftDown() ? 0.001f : 0.005f)));
        repaint();
    }
    void mouseUp (const juce::MouseEvent&) override { if (auto* p = st.getParameter (id)) p->endChangeGesture(); }
    void mouseDoubleClick (const juce::MouseEvent&) override { if (auto* p = st.getParameter (id)) p->setValueNotifyingHost (p->getDefaultValue()); repaint(); }
};

// Ventana emergente (engranaje) con perillas de ajuste fino
struct Popover : juce::Component
{
    juce::String title;
    juce::OwnedArray<Knob> knobs;
    Popover (APVTS& s, juce::String t, std::vector<std::pair<const char*, juce::String>> ids) : title (std::move (t))
    {
        for (auto& [id, name] : ids) addAndMakeVisible (knobs.add (new Knob (s, id, name)));
        const int cols = std::min (3, (int) ids.size()), rows = ((int) ids.size() + cols - 1) / cols;
        setSize (cols * 110 + 24, rows * 150 + 44);
    }
    void resized() override
    {
        const int cols = std::min (3, knobs.size());
        for (int i = 0; i < knobs.size(); ++i) knobs[i]->setBounds (12 + (i % cols) * 110, 38 + (i / cols) * 150, 110, 146);
    }
    void paint (juce::Graphics& g) override
    {
        g.setFont (sans (12.0f, true)); g.setColour (Palette::neonB);
        g.drawText (title.toUpperCase(), 14, 10, getWidth() - 28, 18, juce::Justification::centredLeft);
    }
};

//==============================================================================
// Tarjeta de macro: LED on/off, nombre, icono, selector <  ESTILO  >
struct MacroCard : juce::Component, juce::SettableTooltipClient
{
    struct Style { const char* es; const char* en; std::vector<std::pair<const char*, float>> v; };
    TopVocalChainProcessor& proc;
    juce::String key, titleEs, titleEn;
    std::vector<Style> styles;
    std::function<bool()> isOn;
    std::function<void (bool)> setOn;
    std::function<void (juce::Graphics&, juce::Rectangle<float>, juce::Colour)> icon;
    std::function<void (juce::Component&)> onGear;
    juce::Rectangle<float> ledR, gearR, leftR, rightR;
    float hoverGlow = 0;

    MacroCard (TopVocalChainProcessor& p, juce::String k, juce::String es, juce::String en) : proc (p), key (std::move (k)), titleEs (es), titleEn (en) {}
    int index() const { return juce::jlimit (0, (int) styles.size() - 1, (int) proc.apvts.state.getProperty ("v2_" + key, defaultIndex)); }
    int defaultIndex = 0;
    void select (int i)
    {
        i = (i + (int) styles.size()) % (int) styles.size();
        proc.undoManager.beginNewTransaction();
        proc.apvts.state.setProperty ("v2_" + key, i, nullptr);
        for (auto& [id, val] : styles[(size_t) i].v)
            if (auto* prm = proc.apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (val));
        repaint();
    }
    void resized() override
    {
        ledR = { 16, 16, 20, 20 };
        gearR = { (float) getWidth() - 46, 10, 32, 32 };
        leftR = { 14, (float) getHeight() - 52, 32, 30 };
        rightR = { (float) getWidth() - 46, (float) getHeight() - 52, 32, 30 };
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        const auto pt = e.position;
        if (ledR.expanded (6).contains (pt) && setOn) { proc.undoManager.beginNewTransaction(); setOn (! isOn()); repaint(); }
        else if (gearR.contains (pt) && onGear) onGear (*this);
        else if (leftR.contains (pt)) select (index() - 1);
        else if (rightR.contains (pt)) select (index() + 1);
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        panelBox (g, r.reduced (2), 14);
        const bool on = isOn ? isOn() : true;
        led (g, ledR.getCentre(), 7.0f, on, Palette::neonB.interpolatedWith (Palette::neonA, 0.3f));
        g.setFont (sans (12.5f, true)); g.setColour (on ? Palette::text : Palette::dim);
        g.drawText (upper (gLang == 0 ? titleEs : titleEn), 44, 14, 200, 24, juce::Justification::centredLeft);
        // engranaje
        {
            auto c = gearR.getCentre();
            g.setColour (Palette::cardHi); g.fillEllipse (gearR);
            g.setColour (Palette::border); g.drawEllipse (gearR, 1.0f);
            juce::Path gear;
            for (int i = 0; i < 8; ++i)
            {
                const float a = i * juce::MathConstants<float>::twoPi / 8;
                gear.addRectangle (-2.2f, -10.0f, 4.4f, 5.0f);
                gear.applyTransform (juce::AffineTransform::rotation (juce::MathConstants<float>::twoPi / 8));
                juce::ignoreUnused (a);
            }
            gear.addEllipse (-7.0f, -7.0f, 14, 14);
            g.setColour (Palette::dim); g.fillPath (gear, juce::AffineTransform::translation (c));
            g.setColour (Palette::cardHi); g.fillEllipse (c.x - 3.5f, c.y - 3.5f, 7, 7);
        }
        // icono neon
        auto ic = juce::Rectangle<float> (r.getCentreX() - 34, 46, 68, 68);
        const auto col = on ? Palette::neonB : Palette::faint;
        if (on) { g.setColour (Palette::neonA.withAlpha (0.10f)); g.fillEllipse (ic.expanded (14)); }
        if (icon) icon (g, ic, col);
        // linea de puntos
        for (float x = 24; x < r.getWidth() - 24; x += 6)
        {
            g.setColour (Palette::neonA.interpolatedWith (Palette::neonB, x / r.getWidth()).withAlpha (on ? 0.5f : 0.2f));
            g.fillEllipse (x, r.getHeight() - 62, 2, 2);
        }
        // selector
        auto arrow = [&] (juce::Rectangle<float> a, bool left)
        {
            juce::Path p; const auto c = a.getCentre();
            if (left) p.addTriangle (c.x + 4, c.y - 6, c.x + 4, c.y + 6, c.x - 5, c.y); else p.addTriangle (c.x - 4, c.y - 6, c.x - 4, c.y + 6, c.x + 5, c.y);
            g.setColour (Palette::neonB.withAlpha (on ? 0.9f : 0.4f)); g.fillPath (p);
        };
        arrow (leftR, true); arrow (rightR, false);
        const auto& s = styles[(size_t) index()];
        g.setFont (mono (14.0f)); g.setColour (on ? Palette::neonB.brighter (0.2f) : Palette::dim);
        g.drawText (upper (U (gLang == 0 ? s.es : s.en)), juce::Rectangle<float> (50, r.getHeight() - 54, r.getWidth() - 100, 22),
                    juce::Justification::centred, false);
        const int n = (int) styles.size();
        for (int i = 0; i < n; ++i)
        {
            g.setColour (i == index() ? Palette::neonB : Palette::faint);
            const float d = i == index() ? 4.0f : 3.0f;
            g.fillEllipse (r.getCentreX() - (n - 1) * 4.5f + i * 9.0f - d * 0.5f, r.getHeight() - 27, d, d);
        }
    }
};

// Tira de modulo: LED, nombre, lectura y engranaje
struct Strip : juce::Component
{
    TopVocalChainProcessor& proc;
    juce::String es, en;
    std::function<bool()> isOn;
    std::function<void (bool)> setOn;
    std::function<juce::String()> readout;
    std::function<void (juce::Component&)> onGear;
    Strip (TopVocalChainProcessor& p, juce::String e, juce::String n) : proc (p), es (std::move (e)), en (std::move (n)) {}
    juce::Rectangle<float> ledArea() const { return { 14, (float) getHeight() * 0.5f - 12, 24, 24 }; }
    juce::Rectangle<float> gearArea() const { return { (float) getWidth() - 44, (float) getHeight() * 0.5f - 15, 30, 30 }; }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (ledArea().expanded (6).contains (e.position) || juce::Rectangle<float> (40, 0, 150, (float) getHeight()).contains (e.position))
        { proc.undoManager.beginNewTransaction(); setOn (! isOn()); repaint(); }
        else if (gearArea().contains (e.position) && onGear) onGear (*this);
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        panelBox (g, r.reduced (2), 12);
        const bool on = isOn();
        led (g, ledArea().getCentre(), 7.0f, on, Palette::neonB.interpolatedWith (Palette::neonA, 0.3f));
        g.setFont (sans (12.5f, true)); g.setColour (on ? Palette::text : Palette::faint);
        g.drawText (upper (gLang == 0 ? es : en), 46, 0, 180, getHeight(), juce::Justification::centredLeft);
        auto box = juce::Rectangle<float> ((float) getWidth() - 176, 8, 120, (float) getHeight() - 16);
        lcdBox (g, box, 7);
        g.setFont (mono (12.0f)); g.setColour (on ? Palette::neonB.brighter (0.3f) : Palette::faint);
        g.drawText (readout(), box, juce::Justification::centred, false);
        auto c = gearArea().getCentre();
        g.setColour (Palette::cardHi); g.fillEllipse (gearArea());
        g.setColour (Palette::border); g.drawEllipse (gearArea(), 1.0f);
        juce::Path gear;
        for (int i = 0; i < 8; ++i) { gear.addRectangle (-2.0f, -9.5f, 4.0f, 4.5f); gear.applyTransform (juce::AffineTransform::rotation (juce::MathConstants<float>::twoPi / 8)); }
        gear.addEllipse (-6.5f, -6.5f, 13, 13);
        g.setColour (Palette::dim); g.fillPath (gear, juce::AffineTransform::translation (c));
        g.setColour (Palette::cardHi); g.fillEllipse (c.x - 3, c.y - 3, 6, 6);
    }
};

//==============================================================================
// Iconos propios (vector)
static void iconTune (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
{
    juce::Path w;
    for (int i = 0; i <= 40; ++i)
    {
        const float t = i / 40.0f, x = r.getX() + 4 + t * (r.getWidth() - 8);
        const float y = r.getCentreY() + 10 + std::sin (t * 9.0f) * 9.0f * (1.0f - t);
        if (i == 0) w.startNewSubPath (x, y); else w.lineTo (x, y);
    }
    g.setColour (c.withAlpha (0.3f)); g.strokePath (w, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (c); g.strokePath (w, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    // nota musical
    const float nx = r.getCentreX() + 6, ny = r.getY() + 26;
    g.drawEllipse (nx - 7, ny - 5, 12, 10, 2.2f);
    g.drawLine (nx + 5, ny, nx + 5, ny - 24, 2.2f);
    g.drawLine (nx + 5, ny - 24, nx + 14, ny - 18, 2.2f);
}
static void iconTone (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
{
    const float xs[] { 0.25f, 0.5f, 0.75f }, ys[] { 0.62f, 0.35f, 0.5f };
    for (int i = 0; i < 3; ++i)
    {
        const float x = r.getX() + r.getWidth() * xs[i];
        g.setColour (c.withAlpha (0.35f)); g.drawLine (x, r.getY() + 8, x, r.getBottom() - 8, 2.0f);
        const float y = r.getY() + r.getHeight() * ys[i];
        g.setColour (c.withAlpha (0.3f)); g.fillEllipse (x - 9, y - 9, 18, 18);
        g.setColour (c); g.drawEllipse (x - 6, y - 6, 12, 12, 2.2f);
    }
}
static void iconSpace (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
{
    const auto o = juce::Point<float> (r.getX() + 12, r.getBottom() - 12);
    g.setColour (c); g.fillEllipse (o.x - 4, o.y - 4, 8, 8);
    for (int k = 1; k <= 3; ++k)
    {
        juce::Path a; const float rr = 14.0f * k;
        a.addCentredArc (o.x, o.y, rr, rr, 0, 0.1f, juce::MathConstants<float>::halfPi - 0.1f, true);
        g.setColour (c.withAlpha (0.25f)); g.strokePath (a, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (c.withAlpha (1.0f - 0.2f * k)); g.strokePath (a, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}
static void iconCharacter (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
{
    // valvula
    auto b = r.withSizeKeepingCentre (30, 50).translated (0, -2);
    juce::Path tube; tube.addRoundedRectangle (b.getX(), b.getY(), b.getWidth(), b.getHeight(), 14, 14, true, true, false, false);
    g.setColour (c.withAlpha (0.12f)); g.fillPath (tube);
    g.setColour (c.withAlpha (0.3f)); g.strokePath (tube, juce::PathStrokeType (6.0f));
    g.setColour (c); g.strokePath (tube, juce::PathStrokeType (2.2f));
    g.fillRect (b.getX() - 3, b.getBottom(), b.getWidth() + 6, 5.0f);
    for (int i = 0; i < 3; ++i) g.drawLine (b.getX() + 7 + i * 8, b.getBottom() + 5, b.getX() + 7 + i * 8, b.getBottom() + 11, 2.0f);
    juce::Path fil; fil.startNewSubPath (b.getX() + 8, b.getBottom() - 8);
    fil.lineTo (b.getCentreX() - 3, b.getY() + 16); fil.lineTo (b.getCentreX() + 3, b.getBottom() - 14); fil.lineTo (b.getRight() - 8, b.getY() + 16);
    g.strokePath (fil, juce::PathStrokeType (1.8f));
}
static void iconLogo (juce::Graphics& g, juce::Rectangle<float> r)
{
    // anillo neon con microfono
    const auto c = r.getCentre(); const float R = r.getWidth() * 0.46f;
    g.setColour (Palette::neonA.withAlpha (0.18f)); g.fillEllipse (r.expanded (6));
    juce::Path ring; ring.addEllipse (c.x - R, c.y - R, 2 * R, 2 * R);
    g.setGradientFill (neon (r)); g.strokePath (ring, juce::PathStrokeType (4.0f));
    g.fillRoundedRectangle (c.x - R * 0.2f, c.y - R * 0.55f, R * 0.4f, R * 0.66f, R * 0.2f);
    juce::Path cup; cup.addCentredArc (c.x, c.y - R * 0.12f, R * 0.36f, R * 0.36f, 0, juce::MathConstants<float>::pi * 0.55f, juce::MathConstants<float>::pi * 1.45f, true);
    g.strokePath (cup, juce::PathStrokeType (3.0f));
    g.fillRect (c.x - 1.5f, c.y + R * 0.24f, 3.0f, R * 0.25f);
}

// Analizador pequeno para el EQ
struct MiniSpectrum
{
    static constexpr int order = 11, N = 1 << order, points = 90;
    juce::dsp::FFT fft { order };
    std::vector<float> win = std::vector<float> ((size_t) N), buf = std::vector<float> ((size_t) N * 2), l = std::vector<float> ((size_t) N), r = std::vector<float> ((size_t) N);
    std::array<float, points> sm {};
    MiniSpectrum()
    {
        for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * i / (N - 1));
        sm.fill (-120);
    }
    void update (const AudioTap& tap, double sr)
    {
        tap.read (l.data(), r.data(), N);
        for (int i = 0; i < N; ++i) buf[(size_t) i] = 0.5f * (l[(size_t) i] + r[(size_t) i]) * win[(size_t) i];
        std::fill (buf.begin() + N, buf.end(), 0.0f);
        fft.performFrequencyOnlyForwardTransform (buf.data(), true);
        for (int p = 0; p < points; ++p)
        {
            const double f0 = 30.0 * std::pow (600.0, (double) p / points), f1 = 30.0 * std::pow (600.0, (double) (p + 1) / points);
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
    static constexpr int W = 1220, H = 790;
    TopVocalChainProcessor& proc;
    MacroCard tune, tone, space, character;
    Strip comp, deess, doubler;
    Knob speed, humanize, revMix, dlyMix, air, harm, sat, width;
    juce::OwnedArray<juce::Slider> eqSliders;
    juce::OwnedArray<APVTS::SliderAttachment> eqAtts;
    juce::OwnedArray<ValueBox> eqBoxes;
    ValueBox inBox, outBox, mixBox;
    juce::TextButton plusBtn { "+" }, before, after, aBtn { "A" }, bBtn { "B" }, undoBtn, redoBtn, assist { "ASSIST" }, lang { "ES" }, majorBtn, minorBtn;
    MiniSpectrum spec;
    juce::String help;
    std::unique_ptr<juce::Component> assistPanel;
    std::unique_ptr<juce::FileChooser> chooser;
    const juce::Rectangle<float> presetR { 402, 24, 276, 72 }, keyR { 56, 418, 116, 46 }, eqGraph { 342, 392, 248, 116 }, spaceDisp { 630, 392, 248, 116 };

    static const char* eqIds (int i) { static const char* ids[] { "eq_low_gain", "eq_mid_gain", "eq_pres_gain", "eq_air_gain" }; return ids[i]; }

    explicit MainView (TopVocalChainProcessor& p)
        : proc (p),
          tune (p, "tune", U ("Afinación"), "Tuning"), tone (p, "tone", "Tono", "Tone"), space (p, "space", "Espacio", "Space"), character (p, "char", U ("Carácter"), "Character"),
          comp (p, "Compresor", "Compressor"), deess (p, "De-Esser", "De-Esser"), doubler (p, "Doubler", "Doubler"),
          speed (p.apvts, "tune_speed", "Velocidad"), humanize (p.apvts, "tune_humanize", "Humanizar"),
          revMix (p.apvts, "rev_mix", "Reverb"), dlyMix (p.apvts, "dly_mix", "Delay"),
          air (p.apvts, "air_high", "Aire"), harm (p.apvts, "air_sparkle", U ("Armónicos")),
          sat (p.apvts, "pre_drive", U ("Saturación")), width (p.apvts, "wide_width", "Ancho"),
          inBox (p.apvts, "in_gain"), outBox (p.apvts, "out_gain"), mixBox (p.apvts, "global_mix")
    {
        setupMacros();
        setupStrips();
        for (auto* c : std::initializer_list<juce::Component*> { &tune, &tone, &space, &character, &comp, &deess, &doubler }) addAndMakeVisible (c);
        for (auto* k : { &speed, &humanize, &revMix, &dlyMix, &air, &harm, &sat, &width }) addAndMakeVisible (k);
        speed.centre = [this] { return juce::String (juce::roundToInt (val ("tune_speed"))) + "ms"; };
        speed.pill = [this] { return "MS " + juce::String (juce::roundToInt (val ("tune_speed"))); };
        humanize.centre = [this] { return juce::String (juce::roundToInt (val ("tune_humanize"))) + "%"; };
        humanize.pill = [this] { return T ("NIVEL ", "LEVEL ") + juce::String (juce::roundToInt (val ("tune_humanize"))) + "%"; };
        revMix.centre = [this] { return juce::String (juce::roundToInt (val ("rev_mix"))) + "%"; };
        revMix.pill = [this] { return T ("TIEMPO ", "TIME ") + juce::String (val ("rev_decay"), 1) + "s"; };
        dlyMix.centre = [this] { return juce::String (juce::roundToInt (val ("dly_mix"))) + "%"; };
        dlyMix.pill = [this] { return T ("RITMO ", "RATE ") + TopVocalChainProcessor::divisionNames()[(int) val ("dly_div")]; };
        air.centre = [this] { return juce::String (juce::roundToInt (val ("air_high"))) + "%"; };
        air.pill = [this] { return juce::String (juce::roundToInt (val ("air_freq") / 1000.0f)) + " kHz"; };
        harm.centre = [this] { return juce::String (juce::roundToInt (val ("air_sparkle"))) + "%"; };
        harm.pill = [this] { return "SILK " + juce::String (juce::roundToInt (val ("air_silk"))) + "%"; };
        sat.centre = [this] { return juce::String (juce::roundToInt (val ("pre_drive") / 24.0f * 100.0f)) + "%"; };
        sat.pill = [this] { const auto& s = character.styles[(size_t) character.index()]; return T ("MODO ", "MODE ") + upper (U (gLang == 0 ? s.es : s.en)); };
        width.centre = [this] { return juce::String (juce::roundToInt (val ("wide_width"))) + "%"; };
        width.pill = [this] { return "ST " + juce::String (juce::roundToInt (val ("wide_width"))) + "%"; };
        speed.slider.setTooltip (T ("Velocidad de afinación: 0 ms = efecto robot, más ms = más natural", "Retune speed: 0 ms = robot effect, higher = more natural"));
        humanize.slider.setTooltip (T ("Humanizar: deja las notas largas más naturales", "Humanize: keeps long notes more natural"));
        revMix.slider.setTooltip (T ("Cantidad de reverb", "Reverb amount"));
        dlyMix.slider.setTooltip (T ("Cantidad de delay (eco)", "Delay amount"));
        air.slider.setTooltip (T ("Aire sedoso arriba (hasta 20 kHz)", "Silky top air (up to 20 kHz)"));
        harm.slider.setTooltip (T ("Armónicos reales que dan riqueza a los agudos", "Real harmonics for rich highs"));
        sat.slider.setTooltip (T ("Saturación del carácter elegido", "Saturation of the chosen character"));
        width.slider.setTooltip (T ("Ancho estéreo de la voz", "Stereo width"));

        static const char* eqEs[] { "Graves", "Medios", "Agudos", "Aire" };
        for (int i = 0; i < 4; ++i)
        {
            auto* s = eqSliders.add (new juce::Slider (juce::Slider::LinearVertical, juce::Slider::NoTextBox));
            s->getProperties().set ("bipolar", true);
            s->setVelocityModeParameters (0.25, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
            eqAtts.add (new APVTS::SliderAttachment (p.apvts, eqIds (i), *s));
            if (auto* prm = dynamic_cast<juce::RangedAudioParameter*> (p.apvts.getParameter (eqIds (i))))
                s->setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
            s->setTooltip (juce::String::fromUTF8 (eqEs[i]) + T (" (doble clic = 0)", " (double-click = reset)"));
            addAndMakeVisible (s);
            auto* b = eqBoxes.add (new ValueBox (p.apvts, eqIds (i)));
            const char* id = eqIds (i);
            b->textFn = [this, id] { const float v = val (id); return (v > 0.05f ? "+" : "") + juce::String (v, 1); };
            addAndMakeVisible (b);
        }
        for (auto* b : { &inBox, &outBox, &mixBox }) addAndMakeVisible (b);
        inBox.textFn = [this] { return juce::String (val ("in_gain"), 1) + " dB"; };
        outBox.textFn = [this] { return juce::String (val ("out_gain"), 1) + " dB"; };
        mixBox.textFn = [this] { return "MIX " + juce::String (juce::roundToInt (val ("global_mix"))) + "%"; };
        mixBox.accent = true;
        inBox.setTooltip (T ("Ganancia de entrada (arrastra arriba/abajo)", "Input gain (drag up/down)"));
        outBox.setTooltip (T ("Volumen de salida (arrastra arriba/abajo)", "Output level (drag up/down)"));
        mixBox.setTooltip (T ("Mezcla con la voz original", "Blend with the original vocal"));

        for (auto* b : { &plusBtn, &before, &after, &aBtn, &bBtn, &undoBtn, &redoBtn, &assist, &lang, &majorBtn, &minorBtn }) addAndMakeVisible (b);
        assist.getProperties().set ("accent", true);
        plusBtn.onClick = [this] { savePreset(); };
        before.onClick = [this] { proc.compareActive.store (true); };
        after.onClick = [this] { proc.compareActive.store (false); };
        aBtn.onClick = [this] { proc.switchToSlot (0); };
        bBtn.onClick = [this] { proc.switchToSlot (1); };
        undoBtn.onClick = [this] { proc.undoManager.undo(); repaint(); };
        redoBtn.onClick = [this] { proc.undoManager.redo(); repaint(); };
        assist.onClick = [this] { openAssist(); };
        lang.onClick = [this]
        {
            juce::PopupMenu m; m.addItem (1, "ES  Espanol", true, gLang == 0); m.addItem (2, "EN  English", true, gLang == 1);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&lang), [this] (int r) { if (r > 0) { gLang = r - 1; refreshTexts(); } });
        };
        majorBtn.onClick = [this] { setParam ("tune_scale", 1); };
        minorBtn.onClick = [this] { setParam ("tune_scale", 2); };
        refreshTexts();
    }

    float val (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }
    void setParam (const char* id, float v) { if (auto* prm = proc.apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (v)); }

    void refreshTexts()
    {
        before.setButtonText (T ("Antes", "Before"));
        after.setButtonText (T ("Después", "After"));
        majorBtn.setButtonText (T ("Mayor", "Major"));
        minorBtn.setButtonText (T ("Menor", "Minor"));
        lang.setButtonText (gLang == 0 ? "ES" : "EN");
        undoBtn.setButtonText ("UNDO"); redoBtn.setButtonText ("REDO");
        undoBtn.setTooltip (T ("Deshacer", "Undo")); redoBtn.setTooltip (T ("Rehacer", "Redo"));
        speed.title = T ("Velocidad", "Speed"); humanize.title = T ("Humanizar", "Humanize");
        air.title = T ("Aire", "Air"); harm.title = T ("Armónicos", "Harmonics");
        sat.title = T ("Saturación", "Saturation"); width.title = T ("Ancho", "Width");
        plusBtn.setTooltip (T ("Guardar tus ajustes como preset", "Save your settings as a preset"));
        assist.setTooltip (T ("IA: escucha tu voz 8 s y propone ajustes", "AI: listens 8 s and suggests settings"));
        before.setTooltip (T ("Escucha la voz original (con el mismo volumen)", "Hear the original (level matched)"));
        repaint();
        for (auto* c : getChildren()) c->repaint();
    }

    //------------------------------------------------------------------
    void setupMacros()
    {
        tune.icon = iconTune; tone.icon = iconTone; space.icon = iconSpace; character.icon = iconCharacter;
        tune.styles = { { "Natural", "Natural", { { "tune_on", 1 }, { "tune_mode", 0 }, { "tune_speed", 70 }, { "tune_humanize", 35 }, { "tune_flex", 30 } } },
                        { "Suave", "Soft", { { "tune_on", 1 }, { "tune_mode", 0 }, { "tune_speed", 30 }, { "tune_humanize", 20 }, { "tune_flex", 10 } } },
                        { "Moderno", "Modern", { { "tune_on", 1 }, { "tune_mode", 0 }, { "tune_speed", 10 }, { "tune_humanize", 0 }, { "tune_flex", 0 } } },
                        { "Robot", "Robot", { { "tune_on", 1 }, { "tune_mode", 1 }, { "tune_speed", 0 }, { "tune_humanize", 0 }, { "tune_flex", 0 } } } };
        tone.styles = { { "Oscuro", "Dark", { { "eq_on", 1 }, { "eq_low_gain", 2 }, { "eq_mid_gain", 0 }, { "eq_pres_gain", -2 }, { "eq_air_gain", -3 } } },
                        { "Cálido", "Warm", { { "eq_on", 1 }, { "eq_low_gain", 1.5f }, { "eq_mid_gain", -1 }, { "eq_pres_gain", 0.5f }, { "eq_air_gain", 1 } } },
                        { "Neutro", "Neutral", { { "eq_on", 1 }, { "eq_low_gain", 0 }, { "eq_mid_gain", 0 }, { "eq_pres_gain", 0 }, { "eq_air_gain", 0 } } },
                        { "Brillante", "Bright", { { "eq_on", 1 }, { "eq_low_gain", -1 }, { "eq_mid_gain", -1.5f }, { "eq_pres_gain", 3.5f }, { "eq_air_gain", 4 } } },
                        { "Aire", "Airy", { { "eq_on", 1 }, { "eq_low_gain", -1 }, { "eq_mid_gain", 0 }, { "eq_pres_gain", 1.5f }, { "eq_air_gain", 6 } } } };
        space.styles = { { "Seco", "Dry", { { "rev_on", 0 }, { "dly_on", 0 } } },
                         { "Sala", "Room", { { "rev_on", 1 }, { "rev_type", 0 }, { "rev_decay", 0.9f }, { "rev_mix", 14 }, { "dly_on", 0 } } },
                         { "Plate", "Plate", { { "rev_on", 1 }, { "rev_type", 4 }, { "rev_decay", 1.6f }, { "rev_mix", 16 }, { "dly_on", 0 } } },
                         { "Hall", "Hall", { { "rev_on", 1 }, { "rev_type", 2 }, { "rev_decay", 2.6f }, { "rev_mix", 18 }, { "dly_on", 0 } } },
                         { "Eco", "Echo", { { "rev_on", 1 }, { "rev_type", 4 }, { "rev_decay", 1.2f }, { "rev_mix", 10 }, { "dly_on", 1 }, { "dly_sync", 1 },
                                            { "dly_div", 3 }, { "dly_mix", 18 }, { "dly_fb", 30 } } } };
        character.styles = { { "Limpio", "Clean", { { "pre_on", 1 }, { "pre_type", 0 }, { "pre_drive", 2 } } },
                             { "Válvula", "Tube", { { "pre_on", 1 }, { "pre_type", 1 }, { "pre_drive", 6 } } },
                             { "Cinta", "Tape", { { "pre_on", 1 }, { "pre_type", 4 }, { "pre_drive", 6 } } },
                             { "Transformador", "Iron", { { "pre_on", 1 }, { "pre_type", 5 }, { "pre_drive", 6 } } },
                             { "Fuego", "Fire", { { "pre_on", 1 }, { "pre_type", 7 }, { "pre_drive", 12 } } } };
        tune.defaultIndex = 0; tone.defaultIndex = 2; space.defaultIndex = 2; character.defaultIndex = 1;
        auto onOff = [this] (MacroCard& c, std::vector<const char*> ids)
        {
            c.isOn = [this, ids] { for (auto* id : ids) if (val (id) > 0.5f) return true; return false; };
            c.setOn = [this, ids] (bool o) { for (auto* id : ids) setParam (id, o ? 1.0f : 0.0f); };
        };
        onOff (tune, { "tune_on" }); onOff (tone, { "eq_on" }); onOff (character, { "pre_on" });
        space.isOn = [this] { return val ("rev_on") > 0.5f || val ("dly_on") > 0.5f; };
        space.setOn = [this] (bool o) { setParam ("rev_on", o ? 1.0f : 0.0f); if (! o) setParam ("dly_on", 0.0f); };
        tune.setTooltip (T ("AFINACIÓN: elige el estilo con las flechas · LED = encender/apagar · engranaje = ajustes", "TUNING: pick a style with the arrows"));
        tone.setTooltip (T ("TONO: carácter del ecualizador", "TONE: EQ character"));
        space.setTooltip (T ("ESPACIO: reverb y eco", "SPACE: reverb and echo"));
        character.setTooltip (T ("CARÁCTER: color del preamp", "CHARACTER: preamp colour"));
        tune.onGear = [this] (juce::Component& c) { pop (c, T ("Afinación", "Tuning"), { { "tune_flex", "Flex-Tune" }, { "tune_formant", "Formant" }, { "tune_vibrato", "Vibrato" }, { "tune_transpose", "Transpose" } }); };
        tone.onGear = [this] (juce::Component& c) { pop (c, T ("Tono", "Tone"), { { "eq_lowcut", "Low Cut" }, { "eq_mid_freq", "Mid Freq" }, { "eq_hicut", "High Cut" } }); };
        space.onGear = [this] (juce::Component& c) { pop (c, T ("Espacio", "Space"), { { "rev_predelay", "Pre-Delay" }, { "rev_decay", "Decay" }, { "rev_width", "Width" }, { "dly_fb", "Feedback" }, { "dly_hicut", "Hi Cut" } }); };
        character.onGear = [this] (juce::Component& c) { pop (c, T ("Carácter", "Character"), { { "pre_drive", "Drive" }, { "pre_warmth", "Warmth" }, { "pre_trim", "Trim" } }); };
    }
    void setupStrips()
    {
        comp.isOn = [this] { return val ("comp_on") > 0.5f; };
        comp.setOn = [this] (bool o) { setParam ("comp_on", o ? 1.0f : 0.0f); };
        comp.readout = [this] { return juce::String (proc.meters.compGrDb.load(), 1) + " dB"; };
        comp.onGear = [this] (juce::Component& c) { pop (c, T ("Compresor", "Compressor"), { { "comp_thr", "Threshold" }, { "comp_ratio", "Ratio" }, { "comp_att", "Attack" }, { "comp_rel", "Release" }, { "comp_makeup", "Makeup" } }); };
        deess.isOn = [this] { return val ("deess_on") > 0.5f; };
        deess.setOn = [this] (bool o) { setParam ("deess_on", o ? 1.0f : 0.0f); };
        deess.readout = [this] { return juce::String (proc.meters.deessDb.load(), 1) + " dB"; };
        deess.onGear = [this] (juce::Component& c) { pop (c, "De-Esser", { { "deess_amt", T ("Cantidad", "Amount") }, { "deess_freq", T ("Frecuencia", "Frequency") } }); };
        doubler.isOn = [this] { return val ("wide_on") > 0.5f && (int) val ("wide_mode") == 2; };
        doubler.setOn = [this] (bool o) { if (o) { setParam ("wide_mode", 2); setParam ("wide_on", 1); } else setParam ("wide_on", 0); };
        doubler.readout = [this] { return T ("DOBLE ", "DOUBLE ") + juce::String (juce::roundToInt (val ("wide_amount"))) + "%"; };
        doubler.onGear = [this] (juce::Component& c) { pop (c, "Doubler", { { "wide_amount", T ("Cantidad", "Amount") }, { "wide_mix", "Mix" } }); };
    }
    void pop (juce::Component& target, const juce::String& title, std::vector<std::pair<const char*, juce::String>> ids)
    {
        auto content = std::make_unique<Popover> (proc.apvts, title, std::move (ids));
        content->setLookAndFeel (&getLookAndFeel());
        juce::CallOutBox::launchAsynchronously (std::move (content), target.getScreenBounds(), nullptr);
    }
    void savePreset()
    {
        chooser = std::make_unique<juce::FileChooser> (T ("Guardar preset", "Save preset"),
                                                       TopVocalChainProcessor::getUserPresetFolder().getChildFile ("Mi preset.tvcpreset"), "*.tvcpreset");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc) { const auto f = fc.getResult(); if (f != juce::File()) proc.saveUserPreset (f.withFileExtension ("tvcpreset")); });
    }
    void choosePreset()
    {
        juce::PopupMenu m;
        m.addSectionHeader (T ("FÁBRICA", "FACTORY"));
        for (int i = 0; i < proc.getNumPrograms(); ++i) m.addItem (i + 1, proc.getProgramName (i), true, i == proc.getCurrentProgram());
        const auto user = TopVocalChainProcessor::getUserPresetFolder().findChildFiles (juce::File::findFiles, false, "*.tvcpreset");
        if (! user.isEmpty())
        {
            m.addSectionHeader (T ("MIS PRESETS", "MY PRESETS"));
            for (int i = 0; i < user.size(); ++i) m.addItem (1000 + i, user[i].getFileNameWithoutExtension());
        }
        m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (presetR.toNearestInt())), [this, user] (int r)
        {
            proc.undoManager.beginNewTransaction();
            if (r >= 1000) proc.loadUserPreset (user[r - 1000]);
            else if (r > 0) proc.setCurrentProgram (r - 1);
            repaint();
        });
    }

    //------------------------------------------------------------------
    void resized() override
    {
        plusBtn.setBounds (690, 42, 38, 38);
        before.setBounds (740, 42, 72, 38); after.setBounds (814, 42, 78, 38);
        aBtn.setBounds (904, 42, 38, 38); bBtn.setBounds (946, 42, 38, 38);
        undoBtn.setBounds (996, 42, 54, 38); redoBtn.setBounds (1052, 42, 54, 38);
        assist.setBounds (1112, 40, 64, 42);
        lang.setBounds (1180, 42, 30, 38);
        // fila de tarjetas
        const int cx[] { 40, 328, 616, 904 };
        MacroCard* cards[] { &tune, &tone, &space, &character };
        for (int i = 0; i < 4; ++i) cards[i]->setBounds (cx[i], 112, 276, 190);
        comp.setBounds (40, 312, 372, 52); deess.setBounds (424, 312, 372, 52); doubler.setBounds (808, 312, 372, 52);
        // paneles inferiores
        majorBtn.setBounds (180, 418, 64, 46); minorBtn.setBounds (246, 418, 60, 46);
        speed.setBounds (52, 512, 122, 186); humanize.setBounds (184, 512, 122, 186);
        for (int i = 0; i < 4; ++i)
        {
            eqBoxes[i]->setBounds (340 + i * 63, 520, 56, 26);
            eqSliders[i]->setBounds (342 + i * 63, 552, 52, 124);
        }
        revMix.setBounds (630, 516, 122, 182); dlyMix.setBounds (762, 516, 122, 182);
        air.setBounds (918, 384, 122, 158); harm.setBounds (1048, 384, 122, 158);
        sat.setBounds (918, 548, 122, 158); width.setBounds (1048, 548, 122, 158);
        inBox.setBounds (68, 731, 76, 30); mixBox.setBounds (838, 731, 104, 30); outBox.setBounds (1078, 731, 76, 30);
        if (assistPanel) assistPanel->setBounds (getLocalBounds().withSizeKeepingCentre (620, 470));
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Palette::bg);
        // marco exterior con tornillos
        auto frame = getLocalBounds().toFloat().reduced (6);
        g.setColour (Palette::border); g.drawRoundedRectangle (frame, 22, 1.5f);
        for (auto pt : { frame.getTopLeft() + juce::Point<float> (16, 16), frame.getTopRight() + juce::Point<float> (-16, 16),
                         frame.getBottomLeft() + juce::Point<float> (16, -16), frame.getBottomRight() + juce::Point<float> (-16, -16) })
        {
            g.setColour (Palette::cardHi); g.fillEllipse (pt.x - 6, pt.y - 6, 12, 12);
            g.setColour (Palette::faint); g.drawLine (pt.x - 3.5f, pt.y, pt.x + 3.5f, pt.y, 1.4f);
        }
        // ----- cabecera
        iconLogo (g, { 44, 30, 58, 58 });
        g.setFont (sans (30.0f, true)); g.setColour (Palette::text);
        g.drawText ("Top Vocal Chain", 122, 28, 230, 36, juce::Justification::centredLeft);
        const float tw = juce::GlyphArrangement::getStringWidth (sans (30.0f, true), "Top Vocal Chain");
        juce::GlyphArrangement v2; v2.addLineOfText (sans (30.0f, true), "V2", 0, 0);
        juce::Path v2p; v2.createPath (v2p);
        g.setGradientFill (neon ({ 122 + tw + 8, 30, 40, 30 }));
        g.fillPath (v2p, juce::AffineTransform::translation (122 + tw + 10, 56));
        g.setFont (sans (10.0f, true)); g.setColour (Palette::dim);
        g.drawText (T ("CADENA VOCAL PROFESIONAL  ·  BY DJ PAVA HM MUSIC", "PROFESSIONAL VOCAL CHAIN  ·  BY DJ PAVA HM MUSIC"), 124, 66, 270, 16, juce::Justification::centredLeft);
        // pantalla de presets
        lcdBox (g, presetR, 12);
        g.setFont (mono (9.5f, false)); g.setColour (Palette::neonB.withAlpha (0.8f));
        g.drawText (T ("F Á B R I C A", "F A C T O R Y"), presetR.withHeight (26).translated (0, 6), juce::Justification::centred);
        g.setFont (mono (15.0f)); g.setColour (Palette::neonB.brighter (0.2f));
        g.drawText (proc.getProgramName (proc.getCurrentProgram()), presetR.reduced (36, 0).withTrimmedTop (28).withHeight (30), juce::Justification::centred);
        auto tri = [&] (juce::Point<float> c, bool left)
        {
            juce::Path p;
            if (left) p.addTriangle (c.x + 4, c.y - 6, c.x + 4, c.y + 6, c.x - 5, c.y); else p.addTriangle (c.x - 4, c.y - 6, c.x - 4, c.y + 6, c.x + 5, c.y);
            g.setColour (Palette::neonB); g.fillPath (p);
        };
        tri ({ presetR.getX() + 22, presetR.getCentreY() + 10 }, true);
        tri ({ presetR.getRight() - 22, presetR.getCentreY() + 10 }, false);
        // pastillas Antes/Despues y A/B
        for (auto r : { juce::Rectangle<float> (736, 38, 160, 46), juce::Rectangle<float> (900, 38, 88, 46) })
        { g.setColour (Palette::panel); g.fillRoundedRectangle (r, 23); g.setColour (Palette::border); g.drawRoundedRectangle (r, 23, 1.0f); }

        // ----- paneles inferiores
        const float px[] { 40, 328, 616, 904 };
        for (float x : px) panelBox (g, { x, 376, 276, 336 }, 16);
        // TONALIDAD
        g.setFont (sans (11.0f, true)); g.setColour (Palette::dim);
        g.drawText (T ("T O N A L I D A D", "K E Y"), 58, 388, 200, 20, juce::Justification::centredLeft);
        lcdBox (g, keyR, 10);
        tri ({ keyR.getX() + 16, keyR.getCentreY() }, true);
        tri ({ keyR.getRight() - 16, keyR.getCentreY() }, false);
        g.setFont (mono (20.0f)); g.setColour (Palette::text);
        g.drawText (TopVocalChainProcessor::keyNames()[(int) val ("tune_key")], keyR, juce::Justification::centred);
        // medidor de cents
        {
            const float y = 488, x0 = 62, x1 = 298;
            const float cents = proc.meters.voiced.load() ? juce::jlimit (-50.0f, 50.0f, -proc.meters.correctionCents.load()) : 0.0f;
            g.setFont (mono (8.5f, false)); g.setColour (Palette::faint);
            g.drawText ("-50", (int) x0 - 4, (int) y - 18, 30, 12, juce::Justification::left);
            g.drawText ("cents", (int) (x0 + x1) / 2 - 20, (int) y - 18, 40, 12, juce::Justification::centred);
            g.drawText ("+50", (int) x1 - 26, (int) y - 18, 30, 12, juce::Justification::right);
            for (int i = 0; i <= 30; ++i)
            {
                const float x = x0 + (x1 - x0) * i / 30.0f;
                g.setColour (Palette::faint); g.fillEllipse (x - 1.3f, y - 1.3f, 2.6f, 2.6f);
            }
            const float mx = juce::jmap (cents, -50.0f, 50.0f, x0, x1);
            g.setColour (Palette::neonA.withAlpha (0.3f)); g.fillEllipse (mx - 7, y - 7, 14, 14);
            g.setColour (Palette::neonA.brighter (0.3f)); g.fillEllipse (mx - 4, y - 4, 8, 8);
        }
        // EQ: analizador + curva
        lcdBox (g, eqGraph, 8);
        {
            auto p = eqGraph.reduced (6, 8).withTrimmedBottom (14);
            auto xOf = [&] (double f) { return p.getX() + (float) (std::log (f / 30.0) / std::log (600.0)) * p.getWidth(); };
            g.setColour (Palette::border.withAlpha (0.6f));
            for (double f : { 100.0, 1000.0, 10000.0 }) g.fillRect (xOf (f), p.getY(), 1.0f, p.getHeight());
            g.fillRect (p.getX(), p.getCentreY(), p.getWidth(), 1.0f);
            juce::Path s;
            for (int i = 0; i < MiniSpectrum::points; ++i)
            {
                const double f = 30.0 * std::pow (600.0, (i + 0.5) / MiniSpectrum::points);
                const float y = p.getBottom() - juce::jlimit (0.0f, 1.0f, (spec.sm[(size_t) i] + 84.0f) / 78.0f) * p.getHeight();
                if (i == 0) { s.startNewSubPath (p.getX(), p.getBottom()); s.lineTo (xOf (f), y); } else s.lineTo (xOf (f), y);
            }
            s.lineTo (p.getRight(), p.getBottom()); s.closeSubPath();
            g.setColour (Palette::neonA.withAlpha (0.12f)); g.fillPath (s);
            juce::Path c;
            for (int i = 0; i <= 80; ++i)
            {
                const double f = 30.0 * std::pow (600.0, i / 80.0);
                const float y = p.getCentreY() - juce::jlimit (-15.0f, 15.0f, proc.getEqMagnitudeDb (f)) / 15.0f * p.getHeight() * 0.5f;
                if (i == 0) c.startNewSubPath (xOf (f), y); else c.lineTo (xOf (f), y);
            }
            g.setColour (Palette::neonA.withAlpha (0.3f)); g.strokePath (c, juce::PathStrokeType (5.0f));
            g.setGradientFill (neon (p)); g.strokePath (c, juce::PathStrokeType (2.0f));
            const double nodes[] { 120.0, std::max (200.0, (double) val ("eq_mid_freq")), 4000.0, 12000.0 };
            for (double f : nodes)
            {
                const float y = p.getCentreY() - juce::jlimit (-15.0f, 15.0f, proc.getEqMagnitudeDb (f)) / 15.0f * p.getHeight() * 0.5f;
                g.setColour (juce::Colours::white); g.fillEllipse (xOf (f) - 4.5f, y - 4.5f, 9, 9);
                g.setColour (Palette::neonA); g.drawEllipse (xOf (f) - 4.5f, y - 4.5f, 9, 9, 1.5f);
            }
            g.setFont (mono (8.5f)); g.setColour (Palette::neonB.withAlpha (0.8f));
            g.drawText (T ("ANALIZADOR", "ANALYZER"), (int) eqGraph.getX() + 8, (int) eqGraph.getBottom() - 16, 80, 12, juce::Justification::left);
            g.setColour (Palette::faint);
            for (auto [f, t] : { std::pair<double, const char*> { 1000.0, "1kHz" }, { 10000.0, "10kHz" } })
                g.drawText (t, (int) xOf (f) - 20, (int) eqGraph.getBottom() - 16, 40, 12, juce::Justification::centred);
        }
        static const char* eqEs[] { "Graves", "Medios", "Agudos", "Aire" }; static const char* eqEn[] { "Lows", "Mids", "Highs", "Air" };
        g.setFont (sans (13.0f, true)); g.setColour (Palette::text);
        for (int i = 0; i < 4; ++i) g.drawText (juce::String::fromUTF8 (gLang == 0 ? eqEs[i] : eqEn[i]), 336 + i * 63, 680, 64, 20, juce::Justification::centred);
        // REVERB / DELAY: matriz de puntos
        lcdBox (g, spaceDisp, 8);
        {
            auto d = spaceDisp.reduced (10, 8);
            g.setFont (mono (8.5f)); g.setColour (Palette::dim);
            g.drawText ("REVERB", (int) d.getX(), (int) d.getY(), 80, 12, juce::Justification::left);
            g.drawText ("DELAY", (int) d.getCentreX() + 8, (int) d.getY(), 80, 12, juce::Justification::left);
            g.setColour (Palette::border); g.fillRect (d.getCentreX(), d.getY(), 1.0f, d.getHeight());
            const float lvl = juce::jlimit (0.0f, 1.0f, (proc.meters.outLevelDb.load() + 48.0f) / 42.0f);
            const int rows = 7;
            const float cell = 10.0f, top = d.getY() + 18;
            const bool revOn = val ("rev_on") > 0.5f, dlyOn = val ("dly_on") > 0.5f;
            const float decay = val ("rev_decay"), rmix = val ("rev_mix") / 100.0f;
            for (int col = 0; col < 10; ++col)
            {
                const float hgt = revOn ? juce::jlimit (0.0f, 1.0f, (0.35f + rmix * 1.6f) * std::exp (-col / (1.5f + decay * 2.0f)) * (0.55f + 0.45f * lvl)) : 0.0f;
                for (int r = 0; r < rows; ++r)
                {
                    const bool lit = (rows - r) <= hgt * rows + 0.01f;
                    const float x = d.getX() + col * cell + 4, y = top + r * cell + 4;
                    g.setColour (lit ? Palette::neonA.interpolatedWith (Palette::neonB, (float) r / rows) : Palette::faint.withAlpha (0.6f));
                    g.fillEllipse (x - (lit ? 3.0f : 2.0f), y - (lit ? 3.0f : 2.0f), lit ? 6.0f : 4.0f, lit ? 6.0f : 4.0f);
                }
            }
            const float fb = val ("dly_fb") / 100.0f;
            for (int col = 0; col < 10; ++col)
            {
                const int k = col / 2;
                const float hgt = dlyOn && col % 2 == 0 ? juce::jlimit (0.0f, 1.0f, (0.4f + val ("dly_mix") / 60.0f) * std::pow (std::max (0.05f, fb), (float) k) * (0.6f + 0.4f * lvl)) : 0.0f;
                for (int r = 0; r < rows; ++r)
                {
                    const bool lit = (rows - r) <= hgt * rows + 0.01f;
                    const float x = d.getCentreX() + 10 + col * cell, y = top + r * cell + 4;
                    g.setColour (lit ? Palette::neonB.interpolatedWith (Palette::neonA, (float) r / rows) : Palette::faint.withAlpha (0.6f));
                    g.fillEllipse (x - (lit ? 3.0f : 2.0f), y - (lit ? 3.0f : 2.0f), lit ? 6.0f : 4.0f, lit ? 6.0f : 4.0f);
                }
            }
            g.setFont (mono (8.5f)); g.setColour (Palette::dim);
            g.drawText (T ("TIPO", "TYPE"), (int) d.getX(), (int) d.getBottom() - 10, 40, 10, juce::Justification::left);
            g.setColour (Palette::neonB);
            g.drawText (revOn ? TopVocalChainProcessor::reverbTypeNames()[(int) val ("rev_type")].toUpperCase() : juce::String ("OFF"),
                        (int) d.getCentreX() - 70, (int) d.getBottom() - 10, 64, 10, juce::Justification::right);
            g.setColour (Palette::dim);
            g.drawText (T ("MEZCLA", "MIX"), (int) d.getCentreX() + 8, (int) d.getBottom() - 10, 50, 10, juce::Justification::left);
            g.setColour (Palette::neonB);
            g.drawText (dlyOn ? juce::String (juce::roundToInt (val ("dly_mix"))) + "%" : juce::String ("OFF"), (int) d.getRight() - 50, (int) d.getBottom() - 10, 50, 10, juce::Justification::right);
        }

        // ----- pie: entrada, ayuda, mix, salida
        auto foot = juce::Rectangle<float> (40, 722, 1140, 46);
        panelBox (g, foot, 14);
        g.setFont (mono (9.5f)); g.setColour (Palette::dim);
        g.drawText ("IN", 50, 731, 16, 30, juce::Justification::centredLeft);
        g.drawText ("OUT", 1162, 731, 24, 30, juce::Justification::centredLeft);
        auto dots = [&] (float x0, float x1, float db)
        {
            const int n = 24; const float lit = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f) * n;
            for (int i = 0; i < n; ++i)
            {
                const float x = x0 + (x1 - x0) * i / (n - 1);
                const bool on = i < lit;
                g.setColour (on ? (i > n - 3 ? juce::Colour (0xffff4f6a) : Palette::neonA.interpolatedWith (Palette::neonB, (float) i / n)) : Palette::faint);
                g.fillEllipse (x - 1.6f, 744.0f - 1.6f, 3.2f, 3.2f);
            }
        };
        dots (156, 372, proc.meters.inLevelDb.load());
        dots (954, 1070, proc.meters.outLevelDb.load());
        g.setFont (sans (13.0f, true)); g.setGradientFill (neon ({ 390, 730, 60, 30 }));
        g.drawText (T ("Ayuda", "Help"), 390, 731, 56, 30, juce::Justification::centredLeft);
        g.setFont (sans (12.0f)); g.setColour (Palette::dim);
        g.drawFittedText (help.isNotEmpty() ? help.upToFirstOccurrenceOf ("\n", false, false) + (help.contains ("\n") ? ": " + help.fromFirstOccurrenceOf ("\n", false, false).replace ("\n", " ") : juce::String())
                                            : T ("Doble clic restablece un control. Arrastra los valores. Shift = ajuste fino.", "Double-click resets a control. Drag values. Shift = fine."),
                          juce::Rectangle<int> (446, 731, 380, 30), juce::Justification::centredLeft, 2, 0.85f);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        const auto pt = e.position;
        if (presetR.contains (pt))
        {
            const int n = proc.getNumPrograms();
            if (pt.x < presetR.getX() + 40) { proc.undoManager.beginNewTransaction(); proc.setCurrentProgram ((proc.getCurrentProgram() + n - 1) % n); }
            else if (pt.x > presetR.getRight() - 40) { proc.undoManager.beginNewTransaction(); proc.setCurrentProgram ((proc.getCurrentProgram() + 1) % n); }
            else choosePreset();
            repaint();
        }
        if (keyR.contains (pt))
        {
            const int k = (int) val ("tune_key");
            if (pt.x < keyR.getCentreX() - 20) setParam ("tune_key", (float) ((k + 11) % 12));
            else if (pt.x > keyR.getCentreX() + 20) setParam ("tune_key", (float) ((k + 1) % 12));
            repaint();
        }
    }

    void tick()
    {
        const double sr = proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0;
        spec.update (proc.tapPost, sr);
        // texto de ayuda del control bajo el mouse
        juce::String h;
        if (auto* c = juce::Desktop::getInstance().getMainMouseSource().getComponentUnderMouse())
            if (isParentOf (c) || c == this)
                for (auto* cc = c; cc != nullptr && cc != this; cc = cc->getParentComponent())
                    if (auto* tc = dynamic_cast<juce::TooltipClient*> (cc)) { h = tc->getTooltip(); if (h.isNotEmpty()) break; }
        help = h;
        // estados de botones
        before.setToggleState (proc.compareActive.load(), juce::dontSendNotification);
        after.setToggleState (! proc.compareActive.load(), juce::dontSendNotification);
        aBtn.setToggleState (proc.getCurrentSlot() == 0, juce::dontSendNotification);
        bBtn.setToggleState (proc.getCurrentSlot() == 1, juce::dontSendNotification);
        const int sc = (int) val ("tune_scale");
        majorBtn.setToggleState (sc == 1, juce::dontSendNotification);
        minorBtn.setToggleState (sc == 2, juce::dontSendNotification);
        undoBtn.setEnabled (proc.undoManager.canUndo()); redoBtn.setEnabled (proc.undoManager.canRedo());
        for (auto* c : std::initializer_list<juce::Component*> { &comp, &deess, &doubler, &tune, &tone, &space, &character }) c->repaint();
        for (auto* b : eqBoxes) b->repaint();
        for (auto* b : { &inBox, &outBox, &mixBox }) b->repaint();
        for (auto* k : { &speed, &humanize, &revMix, &dlyMix, &air, &harm, &sat, &width }) k->repaint();
        repaint (0, 0, W, 110);
        repaint (40, 376, 1140, 336);
        repaint (40, 720, 1140, 50);
        if (assistPanel) assistPanel->repaint();
    }

    void openAssist();
};

//==============================================================================
// ASSIST: la IA escucha la voz, muestra el informe y aplica las recomendaciones
struct AssistPanel : juce::Component
{
    TopVocalChainProcessor& proc;
    juce::TextButton apply, close, again;
    std::function<void()> onClose;
    tvc::VoiceAnalysis::Report rep;
    bool shown = false;
    explicit AssistPanel (TopVocalChainProcessor& p) : proc (p)
    {
        for (auto* b : { &apply, &close, &again }) addAndMakeVisible (b);
        apply.getProperties().set ("accent", true);
        apply.setButtonText (T ("APLICAR", "APPLY")); close.setButtonText (T ("CERRAR", "CLOSE")); again.setButtonText (T ("REPETIR", "AGAIN"));
        apply.onClick = [this] { if (! (shown && rep.valid)) return; proc.undoManager.beginNewTransaction(); proc.applyReport (rep); if (onClose) onClose(); };
        close.onClick = [this] { if (proc.analysis.isActive()) proc.analysis.cancel(); if (onClose) onClose(); };
        again.onClick = [this] { shown = false; proc.startAnalysis(); };
        if (! proc.analysis.isReady()) proc.startAnalysis();
    }
    void resized() override
    {
        apply.setBounds (getWidth() - 330, getHeight() - 58, 130, 38);
        again.setBounds (getWidth() - 194, getHeight() - 58, 84, 38);
        close.setBounds (getWidth() - 104, getHeight() - 58, 84, 38);
    }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillRoundedRectangle (b.translated (0, 8), 18);
        g.setColour (Palette::card); g.fillRoundedRectangle (b, 18);
        g.setGradientFill (neon (b)); g.drawRoundedRectangle (b.reduced (1), 18, 1.6f);
        auto r = b.reduced (26, 20);
        g.setFont (sans (22.0f, true)); g.setGradientFill (neon (r.withHeight (30)));
        g.drawText ("ASSIST", r.removeFromTop (30), juce::Justification::centredLeft);
        g.setFont (sans (12.0f)); g.setColour (Palette::dim);
        g.drawText (T ("La IA escucha tu voz y propone ajustes. Tú decides si los aplicas.", "The AI listens and suggests settings. You decide."), r.removeFromTop (20), juce::Justification::centredLeft);
        r.removeFromTop (14);
        if (proc.analysis.isActive())
        {
            g.setFont (sans (16.0f, true)); g.setColour (Palette::text);
            g.drawText (T ("Canta normalmente unos 8 segundos...", "Sing normally for about 8 seconds..."), r.removeFromTop (30), juce::Justification::centredLeft);
            auto bar = r.removeFromTop (14).withTrimmedRight (40);
            g.setColour (Palette::lcd); g.fillRoundedRectangle (bar, 7);
            g.setGradientFill (neon (bar)); g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * proc.analysis.progress()), 7);
            apply.setEnabled (false);
            return;
        }
        if (proc.analysis.isReady() && ! shown) { rep = proc.analysis.computeReport(); shown = true; }
        apply.setEnabled (shown && rep.valid);
        if (! shown) return;
        if (! rep.valid)
        {
            g.setFont (sans (15.0f)); g.setColour (Palette::text);
            g.drawFittedText (T ("No se detectó suficiente voz. Pulsa REPETIR y canta al menos 8 segundos.", "Not enough voice detected. Press AGAIN and sing for 8 seconds."),
                              r.removeFromTop (50).toNearestInt(), juce::Justification::centredLeft, 2);
            return;
        }
        auto left = r.removeFromLeft (230);
        auto item = [&] (const juce::String& label, const juce::String& v)
        {
            auto rr = left.removeFromTop (40);
            g.setFont (mono (9.5f, false)); g.setColour (Palette::dim); g.drawText (label, rr.removeFromTop (14), juce::Justification::centredLeft);
            g.setFont (sans (15.5f, true)); g.setColour (Palette::text); g.drawText (v, rr, juce::Justification::centredLeft);
        };
        auto hz = [] (float f) { return f >= 1000 ? juce::String (f / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (f)) + " Hz"; };
        item (T ("VOZ DETECTADA", "VOICE"), rep.registro);
        item (T ("TONALIDAD", "KEY"), TopVocalChainProcessor::keyNames()[rep.keyRoot] + (rep.minor ? T (" menor", " minor") : T (" mayor", " major")));
        item (T ("RESONANCIA", "RESONANCE"), rep.cutResonance ? hz (rep.resonanceHz) : T ("Sin problema", "None"));
        item (T ("SIBILANCIA", "SIBILANCE"), rep.sibLabel + " (" + hz (rep.sibHz) + ")");
        item (T ("DINÁMICA", "DYNAMICS"), rep.dynLabel);
        item (T ("RUIDO", "NOISE"), rep.noiseLabel);
        g.setFont (mono (9.5f, false)); g.setColour (Palette::neonB);
        g.drawText (T ("RECOMENDACIONES", "RECOMMENDATIONS"), r.removeFromTop (16), juce::Justification::centredLeft);
        r.removeFromTop (6);
        for (auto& line : rep.recommendations)
        {
            auto rr = r.removeFromTop (38);
            g.setGradientFill (neon (rr.withWidth (8))); g.fillEllipse (rr.getX(), rr.getY() + 6, 7, 7);
            g.setFont (sans (13.0f)); g.setColour (Palette::text);
            g.drawFittedText (line, rr.withTrimmedLeft (16).toNearestInt(), juce::Justification::topLeft, 2);
        }
    }
};

void MainView::openAssist()
{
    auto p = std::make_unique<AssistPanel> (proc);
    juce::Component::SafePointer<MainView> safeThis (this);
    p->onClose = [safeThis] { juce::MessageManager::callAsync ([safeThis] { if (safeThis != nullptr) safeThis->assistPanel.reset(); }); };
    addAndMakeVisible (*p);
    assistPanel = std::move (p);
    resized();
}

//==============================================================================
EditorV2::EditorV2 (TopVocalChainProcessor& p) : AudioProcessorEditor (&p)
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

EditorV2::~EditorV2()
{
    stopTimer();
    view.reset();
    setLookAndFeel (nullptr);
}

void EditorV2::tickNow() { view->tick(); }
void EditorV2::openAssistForTest() { view->openAssist(); }
void EditorV2::paint (juce::Graphics& g) { g.fillAll (Palette::bg); }
void EditorV2::resized()
{
    view->setBounds (0, 0, MainView::W, MainView::H);
    view->setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) MainView::W));
}
} // namespace tvc2
