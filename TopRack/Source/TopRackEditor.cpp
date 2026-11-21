#include "TopRackEditor.h"

using namespace trui;
using namespace trk;

namespace
{
    juce::String U (const char* s) { return juce::String::fromUTF8 (s); }
    const juce::Colour bg (0xff0f1013), panel (0xff17191e), panel2 (0xff1f2229), line (0xff2c3039),
                       text (0xffeceef2), dim (0xff8e95a3), gold (0xffffc04d), red (0xffff5a4e), green (0xff57e08f);
    juce::Font font (float h, bool bold = false) { return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain)); }
    juce::String upper (const juce::String& s)
    {
        return s.toUpperCase().replace (U ("á"), U ("Á")).replace (U ("é"), U ("É")).replace (U ("í"), U ("Í"))
                .replace (U ("ó"), U ("Ó")).replace (U ("ú"), U ("Ú")).replace (U ("ñ"), U ("Ñ"));
    }
    bool isLight (juce::Colour c) { return c.getPerceivedBrightness() > 0.5f; }
}

//==============================================================================
Look::Look()
{
    setColour (juce::ComboBox::backgroundColourId, panel2);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::outlineColourId, line);
    setColour (juce::ComboBox::arrowColourId, dim);
    setColour (juce::PopupMenu::backgroundColourId, panel2);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, gold.withAlpha (0.3f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::TooltipWindow::backgroundColourId, panel2);
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TooltipWindow::outlineColourId, line);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::BubbleComponent::backgroundColourId, panel2);
    setColour (juce::BubbleComponent::outlineColourId, line);
    setColour (juce::Label::textColourId, text);
    setColour (juce::TextEditor::backgroundColourId, panel2);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::ScrollBar::thumbColourId, line.brighter (0.3f));
}

void Look::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const auto accent = (juce::Colour) (juce::uint32) (juce::int64) s.getProperties().getWithDefault ("accent", (juce::int64) 0xffffc04d);
    const bool light = (bool) s.getProperties().getWithDefault ("light", false);
    const int steps = (int) s.getProperties().getWithDefault ("steps", 0);
    const juce::Point<float> c ((float) x + (float) w * 0.5f, (float) y + (float) h * 0.5f);
    const float r = (float) juce::jmin (w, h) * 0.5f - 4.0f;
    const float a = a0 + pos * (a1 - a0);
    // escala serigrafiada
    const int ticks = steps > 1 ? steps : 11;
    for (int i = 0; i < ticks; ++i)
    {
        const float t = a0 + (a1 - a0) * (float) i / (float) (ticks - 1);
        g.setColour ((light ? juce::Colours::black : juce::Colours::white).withAlpha (0.55f));
        g.drawLine (juce::Line<float> ({ c.x + (r + 1) * std::sin (t), c.y - (r + 1) * std::cos (t) }, { c.x + (r + 4) * std::sin (t), c.y - (r + 4) * std::cos (t) }), 1.2f);
    }
    const float rk = r - 3.0f;
    g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillEllipse (c.x - rk + 1.5f, c.y - rk + 2.5f, rk * 2, rk * 2);
    if (steps > 1)
    {
        // selector tipo "pico de pollo"
        juce::Path p;
        p.addEllipse (-rk * 0.62f, -rk * 0.62f, rk * 1.24f, rk * 1.24f);
        p.startNewSubPath (-rk * 0.22f, -rk * 0.98f); p.lineTo (rk * 0.22f, -rk * 0.98f); p.lineTo (rk * 0.32f, rk * 0.9f); p.lineTo (-rk * 0.32f, rk * 0.9f); p.closeSubPath();
        p.applyTransform (juce::AffineTransform::rotation (a).translated (c));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4c52), c.x - rk, c.y - rk, juce::Colour (0xff0a0a0b), c.x + rk, c.y + rk, false));
        g.fillPath (p);
        g.setColour (juce::Colours::black); g.strokePath (p, juce::PathStrokeType (1.0f));
        g.setColour (juce::Colours::white);
        g.drawLine (juce::Line<float> ({ c.x + rk * 0.25f * std::sin (a), c.y - rk * 0.25f * std::cos (a) }, { c.x + rk * 0.9f * std::sin (a), c.y - rk * 0.9f * std::cos (a) }), 2.0f);
        return;
    }
    // perilla redonda con tapa metalica
    const juce::Colour body = light ? juce::Colour (0xff1c1c1f) : juce::Colour (0xff2e3036);
    g.setGradientFill (juce::ColourGradient (body.brighter (0.4f), c.x - rk, c.y - rk, body.darker (0.6f), c.x + rk, c.y + rk, false));
    g.fillEllipse (c.x - rk, c.y - rk, rk * 2, rk * 2);
    g.setColour (juce::Colours::black.withAlpha (0.8f)); g.drawEllipse (c.x - rk, c.y - rk, rk * 2, rk * 2, 1.0f);
    const float rc = rk * 0.62f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd9dde3), c.x - rc, c.y - rc, juce::Colour (0xff6c7179), c.x + rc, c.y + rc, false));
    g.fillEllipse (c.x - rc, c.y - rc, rc * 2, rc * 2);
    g.setColour (accent);
    g.drawLine (juce::Line<float> ({ c.x + rk * 0.15f * std::sin (a), c.y - rk * 0.15f * std::cos (a) }, { c.x + rk * 0.92f * std::sin (a), c.y - rk * 0.92f * std::cos (a) }), 2.6f);
}
void Look::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1);
    const auto accent = b.findColour (juce::TextButton::buttonOnColourId);
    if (b.getToggleState()) { g.setColour (accent); g.fillRoundedRectangle (r, 5); return; }
    g.setColour (down ? panel2.brighter (0.2f) : over ? panel2.brighter (0.1f) : panel2.withAlpha (0.92f)); g.fillRoundedRectangle (r, 5);
    g.setColour (over ? accent.withAlpha (0.7f) : line); g.drawRoundedRectangle (r, 5, 1.0f);
}
void Look::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (b.getToggleState() ? bg : text);
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (3, 1), juce::Justification::centred, 1);
}
juce::Font Look::getTextButtonFont (juce::TextButton&, int h) { return font (juce::jmin (12.5f, (float) h * 0.5f), true); }
juce::Font Look::getComboBoxFont (juce::ComboBox&) { return font (13.0f); }
void Look::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (panel2); g.fillRoundedRectangle (r, 6);
    g.setColour (line); g.drawRoundedRectangle (r, 6, 1.0f);
    juce::Path a; const float ax = (float) w - 14, ay = (float) h * 0.5f;
    a.addTriangle (ax - 4.5f, ay - 2.0f, ax + 4.5f, ay - 2.0f, ax, ay + 3.5f);
    g.setColour (dim); g.fillPath (a);
}

//==============================================================================
RackKnob::RackKnob (TopRackProcessor& p, int s, int k, const KnobDef& d) : slot (s), knob (k), def (d), proc (p)
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    setPopupDisplayEnabled (true, true, nullptr);
    getProperties().set ("steps", def.steps.size());
    att = std::make_unique<APVTS::SliderAttachment> (p.apvts, TopRackProcessor::sid (s, "p" + juce::String (k)), *this);
    textFromValueFunction = [this] (double v) { return def.text ((float) v); };
    setDoubleClickReturnValue (true, def.toNorm (def.def));
    setTooltip (def.label + U (" · clic derecho: asignar a una macro"));
}
double RackKnob::snapValue (double v, DragMode)
{
    if (def.steps.size() > 1) return std::round (v * (def.steps.size() - 1)) / (double) (def.steps.size() - 1);
    return v;
}
void RackKnob::mouseDown (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu()) { juce::Slider::mouseDown (e); return; }
    juce::PopupMenu m;
    const int cur = proc.macroOf (slot, knob);
    for (int i = 0; i < 4; ++i) m.addItem (i + 1, U ("Asignar a ") + proc.macroNames[i], true, cur == i);
    m.addItem (10, "Quitar de la macro", cur >= 0);
    m.addSeparator();
    m.addItem (20, "Valor por defecto");
    juce::Component::SafePointer<RackKnob> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int r)
    {
        if (safe == nullptr || r == 0) return;
        if (r >= 1 && r <= 4) safe->proc.assignMacro (r - 1, safe->slot, safe->knob);
        else if (r == 10) safe->proc.unassign (safe->slot, safe->knob);
        else if (r == 20) safe->proc.setKnobReal (safe->slot, safe->knob, safe->def.def);
        if (auto* p = safe->getParentComponent()) p->repaint();
    });
}

//==============================================================================
SlotPanel::SlotPanel (TopRackProcessor& p, int s) : slot (s), proc (p)
{
    byp.setClickingTogglesState (false);
    byp.setColour (juce::TextButton::buttonOnColourId, green);
    byp.setTooltip ("Encender / apagar el módulo");
    byp.onClick = [this]
    {
        if (auto* prm = proc.apvts.getParameter (TopRackProcessor::sid (slot, "byp")))
            prm->setValueNotifyingHost (prm->getValue() > 0.5f ? 0.0f : 1.0f);
    };
    sc.setClickingTogglesState (true);
    sc.setColour (juce::TextButton::buttonOnColourId, gold);
    sc.setTooltip (U ("Sidechain externo: el compresor escucha otra pista (entrada sidechain del plugin)"));
    scAtt = std::make_unique<APVTS::ButtonAttachment> (p.apvts, TopRackProcessor::sid (s, "sc"), sc);
    up.onClick = [this] { proc.moveSlot (slot, -1); };
    down.onClick = [this] { proc.moveSlot (slot, 1); };
    remove.onClick = [this] { proc.setSlotType (slot, Empty); };
    up.setTooltip ("Subir"); down.setTooltip ("Bajar"); remove.setTooltip ("Quitar el módulo");
    trim.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    trim.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    trim.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    trim.setPopupDisplayEnabled (true, true, nullptr);
    trim.setTooltip (U ("Trim: ganancia antes del módulo (para ajustar niveles entre módulos)"));
    trim.getProperties().set ("accent", (juce::int64) 0xffffc04d);
    trimAtt = std::make_unique<APVTS::SliderAttachment> (p.apvts, TopRackProcessor::sid (s, "trim"), trim);
    for (auto* c : { (juce::Component*) &byp, (juce::Component*) &sc, (juce::Component*) &up, (juce::Component*) &down, (juce::Component*) &remove, (juce::Component*) &trim })
        addAndMakeVisible (c);
    build();
}

void SlotPanel::build()
{
    builtType = proc.slotType (slot);
    knobs.clear();
    const auto& d = defs()[(size_t) builtType];
    for (size_t k = 0; k < d.knobs.size(); ++k)
    {
        auto* kn = knobs.add (new RackKnob (proc, slot, (int) k, d.knobs[k]));
        kn->getProperties().set ("accent", (juce::int64) d.accent.getARGB());
        kn->getProperties().set ("light", isLight (d.panel));
        kn->onValueChange = [this] { repaint(); };
        addAndMakeVisible (kn);
    }
    const bool has = builtType != Empty;
    for (auto* c : { (juce::Component*) &byp, (juce::Component*) &up, (juce::Component*) &down, (juce::Component*) &remove, (juce::Component*) &trim }) c->setVisible (has);
    sc.setVisible (has && d.dynamics);
    resized();
    repaint();
}

void SlotPanel::refresh() { if (proc.slotType (slot) != builtType) build(); }

void SlotPanel::tick()
{
    refresh();
    const bool off = proc.apvts.getRawParameterValue (TopRackProcessor::sid (slot, "byp"))->load() > 0.5f;
    byp.setToggleState (! off, juce::dontSendNotification);
    byp.setButtonText (off ? "OFF" : "ON");
    const float g = proc.slotGr (slot);
    grShown = g > grShown ? g : grShown * 0.9f;
    if (defs()[(size_t) builtType].dynamics) repaint (getWidth() - 360, 4, 110, 22);
}

void SlotPanel::resized()
{
    const int w = getWidth();
    remove.setBounds (w - 40, 5, 26, 20);
    down.setBounds (w - 70, 5, 26, 20);
    up.setBounds (w - 100, 5, 26, 20);
    byp.setBounds (w - 146, 5, 42, 20);
    sc.setBounds (w - 186, 5, 36, 20);
    trim.setBounds (w - 222, 2, 28, 26);
    const int n = knobs.size();
    if (n == 0) return;
    const int cell = juce::jmin (74, (w - 40) / n);
    const int x0 = 20 + (w - 40 - cell * n) / 2;
    for (int i = 0; i < n; ++i) knobs[i]->setBounds (x0 + i * cell + (cell - 50) / 2, 32, 50, 50);
}

void SlotPanel::mouseDown (const juce::MouseEvent&) { if (onSelect) onSelect (slot); }

void SlotPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2);
    const auto& d = defs()[(size_t) builtType];
    if (builtType == Empty)
    {
        g.setColour (panel.withAlpha (selected ? 1.0f : 0.6f)); g.fillRoundedRectangle (r, 8);
        juce::Path box; box.addRoundedRectangle (r.reduced (2), 8);
        juce::Path dash; const float pat[] { 6, 5 };
        juce::PathStrokeType (1.4f).createDashedStroke (dash, box, pat, 2);
        g.setColour (selected ? gold : line); g.fillPath (dash);
        g.setColour (selected ? text : dim); g.setFont (font (13.5f, true));
        g.drawText (U ("RANURA ") + juce::String (slot + 1) + U (selected ? "  ·  elige un módulo de la biblioteca" : "  ·  vacía (clic para seleccionar)"),
                    r, juce::Justification::centred);
        return;
    }
    const bool light = isLight (d.panel);
    const auto ink = light ? juce::Colour (0xff15171b) : text;
    g.setGradientFill (juce::ColourGradient (d.panel.brighter (0.12f), 0, r.getY(), d.panel.darker (0.25f), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, 6);
    juce::Random rng (slot * 31 + builtType);
    for (float y = r.getY() + 2; y < r.getBottom() - 2; y += 2.0f)
    {
        g.setColour ((rng.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (0.025f));
        g.drawHorizontalLine ((int) y, r.getX() + 2, r.getRight() - 2);
    }
    g.setColour (selected ? gold : juce::Colours::black.withAlpha (0.7f)); g.drawRoundedRectangle (r, 6, selected ? 2.0f : 1.0f);
    // tornillos del rack
    for (float x : { r.getX() + 9, r.getRight() - 9 })
        for (float y : { r.getY() + 12, r.getBottom() - 12 })
        {
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe1e4e8), x - 3, y - 3, juce::Colour (0xff6a6f77), x + 3, y + 3, false));
            g.fillEllipse (x - 4.5f, y - 4.5f, 9, 9);
            g.setColour (juce::Colour (0xff3a3e44)); g.drawLine (x - 3, y, x + 3, y, 1.2f);
        }
    g.setColour (d.accent); g.setFont (font (15.0f, true));
    g.drawText (d.short_, 24, 4, 260, 22, juce::Justification::centredLeft);
    g.setColour (ink.withAlpha (0.6f)); g.setFont (font (10.5f, true));
    g.drawText (juce::String (slot + 1) + "  ·  " + upper (d.category), 24 + (int) font (15.0f, true).getStringWidthFloat (d.short_) + 12, 6, 200, 18, juce::Justification::centredLeft);
    g.setColour (ink.withAlpha (0.7f)); g.setFont (font (9.5f, true));
    g.drawText ("TRIM", getWidth() - 256, 9, 32, 12, juce::Justification::centredRight);
    if (d.dynamics)
    {
        const auto m = juce::Rectangle<float> ((float) getWidth() - 352, 10, 96, 10);
        g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (m, 3);
        g.setColour (red); g.fillRoundedRectangle (m.withWidth (juce::jmin (m.getWidth(), grShown / 20.0f * m.getWidth())), 3);
        g.setColour (ink.withAlpha (0.7f)); g.setFont (font (9.5f, true));
        g.drawText ("GR", (int) m.getX() - 20, 9, 18, 12, juce::Justification::centredRight);
    }
    // etiquetas de las perillas
    for (auto* k : knobs)
    {
        const auto b = k->getBounds();
        g.setColour (ink); g.setFont (font (10.0f, true));
        g.drawFittedText (upper (k->def.label), b.getX() - 14, b.getBottom() + 1, b.getWidth() + 28, 12, juce::Justification::centred, 1, 0.6f);
        g.setColour (ink.withAlpha (0.7f)); g.setFont (font (10.0f));
        g.drawText (k->def.text ((float) k->getValue()), b.getX() - 14, b.getBottom() + 13, b.getWidth() + 28, 12, juce::Justification::centred);
        const int m = proc.macroOf (slot, k->knob);
        if (m >= 0)
        {
            g.setColour (gold); g.fillEllipse ((float) b.getRight() - 8, (float) b.getY() - 2, 12, 12);
            g.setColour (bg); g.setFont (font (9.0f, true));
            g.drawText (juce::String (m + 1), b.getRight() - 8, b.getY() - 2, 12, 12, juce::Justification::centred);
        }
    }
}

//==============================================================================
Library::Library (TopRackProcessor&)
{
    const std::vector<std::pair<juce::String, std::vector<int>>> groups {
        { "EQ", { EqA, EqConsole, EqAmerican, AirEq } },
        { U ("DINÁMICA"), { CompFet, CompVca, CompOpto, CompBus, Stress, Monster, DeEss, Gate } },
        { "COLOR", { Revive, Bomber } },
        { "UTILIDAD", { Trim } } };
    int y = 40;
    for (const auto& [name, types] : groups)
    {
        headers.push_back ({ name, y }); y += 24;
        for (int t : types) { items.push_back ({ t, y }); y += 32; }
        y += 8;
    }
}
void Library::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (panel); g.fillRoundedRectangle (r, 10);
    g.setColour (text); g.setFont (font (13.0f, true));
    g.drawText ("BIBLIOTECA", 14, 10, 200, 20, juce::Justification::centredLeft);
    for (const auto& [name, y] : headers) { g.setColour (dim); g.setFont (font (11.0f, true)); g.drawText (name, 14, y, 180, 18, juce::Justification::centredLeft); }
    for (size_t i = 0; i < items.size(); ++i)
    {
        const auto& d = defs()[(size_t) items[i].type];
        auto row = juce::Rectangle<float> (8, (float) items[i].y, r.getWidth() - 16, 28);
        g.setColour ((int) i == hover ? panel2.brighter (0.1f) : panel2); g.fillRoundedRectangle (row, 6);
        g.setColour (d.panel); g.fillRoundedRectangle (row.getX() + 6, row.getY() + 6, 16, 16, 3);
        g.setColour (d.accent); g.drawRoundedRectangle (row.getX() + 6, row.getY() + 6, 16, 16, 3, 1.5f);
        g.setColour (text); g.setFont (font (12.5f, true));
        g.drawText (d.name, (int) row.getX() + 30, (int) row.getY(), (int) row.getWidth() - 34, 28, juce::Justification::centredLeft);
    }
    g.setColour (dim); g.setFont (font (11.0f));
    g.drawFittedText (U ("Clic en un módulo para ponerlo en la ranura seleccionada."), 12, getHeight() - 46, getWidth() - 24, 40, juce::Justification::centredLeft, 3);
}
void Library::mouseMove (const juce::MouseEvent& e)
{
    int h = -1;
    for (size_t i = 0; i < items.size(); ++i) if (e.y >= items[i].y && e.y < items[i].y + 28) h = (int) i;
    if (h != hover) { hover = h; repaint(); }
    if (h >= 0) setTooltip (defs()[(size_t) items[(size_t) h].type].help);
}
void Library::mouseDown (const juce::MouseEvent& e)
{
    for (const auto& it : items) if (e.y >= it.y && e.y < it.y + 28 && onPick) onPick (it.type);
}

//==============================================================================
TopRackEditor::TopRackEditor (TopRackProcessor& p) : AudioProcessorEditor (&p), proc (p), library (p)
{
    setLookAndFeel (&look);
    tips.setLookAndFeel (&look);
    addAndMakeVisible (library);
    library.onPick = [this] (int t) { pick (t); };
    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&rackHolder, false);
    viewport.setScrollBarsShown (true, false);
    for (int s = 0; s < numSlots; ++s)
    {
        auto* sp = slots.add (new trui::SlotPanel (proc, s));
        sp->onSelect = [this] (int i) { selectSlot (i); };
        rackHolder.addAndMakeVisible (sp);
    }
    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setTextWhenNothingSelected ("Presets de cadena");
    presetBox.onChange = [this] { if (presetBox.getSelectedId() > 0) proc.setCurrentProgram (presetBox.getSelectedId() - 1); };
    presetBox.setTooltip ("Cadenas listas para voces, baterías, bajo, bus…");
    addAndMakeVisible (presetBox);
    for (int i = 0; i < 8; ++i)
    {
        auto* b = dreams.add (new juce::TextButton (juce::String (i + 1)));
        b->setColour (juce::TextButton::buttonOnColourId, gold);
        b->setTooltip (U ("Dream Strip: clic = guardar la cadena actual (si está vacío) o cargarla. Clic derecho = sobrescribir / borrar."));
        b->onClick = [this, i]
        {
            if (juce::ModifierKeys::currentModifiers.isPopupMenu()) return;
            if (! proc.recallDream (i)) proc.storeDream (i);
        };
        b->addMouseListener (this, false);
        addAndMakeVisible (b);
    }
    osBtn.setClickingTogglesState (true);
    osBtn.setColour (juce::TextButton::buttonOnColourId, gold);
    osBtn.setTooltip (U ("Calcula el rack al doble de frecuencia: saturaciones más limpias (usa más CPU)"));
    osAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "oversample", osBtn);
    addAndMakeVisible (osBtn);
    const juce::StringArray gids { "in_gain", "out_gain", "mix" };
    for (const auto& id : gids)
    {
        auto* s = globals.add (new juce::Slider());
        s->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s->setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        s->setPopupDisplayEnabled (true, true, nullptr);
        gAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, *s));
        addAndMakeVisible (s);
    }
    for (int m = 0; m < 4; ++m)
    {
        auto* s = macros.add (new juce::Slider());
        s->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s->setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        s->getProperties().set ("accent", (juce::int64) 0xffffc04d);
        s->setTooltip (U ("Macro: mueve a la vez todas las perillas asignadas (clic derecho en una perilla para asignarla)"));
        gAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "macro" + juce::String (m + 1), *s));
        addAndMakeVisible (s);
        auto* l = macroLabels.add (new juce::Label ({}, proc.macroNames[m]));
        l->setEditable (false, true);
        l->setJustificationType (juce::Justification::centred);
        l->setFont (font (12.5f, true));
        l->onTextChange = [this, m, l] { proc.macroNames.set (m, l->getText()); };
        l->setTooltip ("Doble clic para cambiar el nombre");
        addAndMakeVisible (l);
    }
    setResizable (true, true);
    setResizeLimits (W * 2 / 3, H * 2 / 3, W * 3 / 2, H * 3 / 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) W / H);
    setSize (W, H);
    selectSlot (0);
    startTimerHz (30);
}

TopRackEditor::~TopRackEditor()
{
    stopTimer();
    for (auto* b : dreams) b->removeMouseListener (this);
    tips.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void TopRackEditor::selectSlot (int s)
{
    selected = s;
    for (auto* sp : slots) { sp->selected = sp->slot == s; sp->repaint(); }
}

void TopRackEditor::mouseDown (const juce::MouseEvent& e)
{
    // clic derecho en un Dream Strip: sobrescribir o borrar
    for (int i = 0; i < dreams.size(); ++i)
        if (e.eventComponent == dreams[i] && e.mods.isPopupMenu())
        {
            juce::PopupMenu m;
            m.addItem (1, "Guardar la cadena actual aqui");
            m.addItem (2, "Borrar", proc.dream[(size_t) i].isValid());
            juce::Component::SafePointer<TopRackEditor> safe (this);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (dreams[i]), [safe, i] (int r)
            {
                if (safe == nullptr) return;
                if (r == 1) safe->proc.storeDream (i);
                else if (r == 2) safe->proc.dream[(size_t) i] = juce::ValueTree();
                safe->repaint();
            });
        }
}

void TopRackEditor::pick (int type)
{
    int target = selected;
    if (proc.slotType (target) != Empty)
        for (int s = 0; s < numSlots; ++s) if (proc.slotType (s) == Empty) { target = s; break; }
    proc.setSlotType (target, type);
    selectSlot (target);
    viewport.setViewPosition (0, juce::jmax (0, slots[target]->getY() - 40));
}

void TopRackEditor::resized()
{
    const float s = (float) getWidth() / (float) W;
    const auto T = juce::AffineTransform::scale (s);
    auto put = [&] (juce::Component& c, int x, int y, int w, int h) { c.setBounds (x, y, w, h); c.setTransform (T); };
    put (presetBox, 230, 14, 220, 32);
    for (int i = 0; i < 8; ++i) put (*dreams[i], 560 + i * 34, 14, 30, 32);
    put (osBtn, 850, 14, 140, 32);
    for (int i = 0; i < 3; ++i) put (*globals[i], 1018 + i * 72, 8, 44, 44);
    put (library, 16, 70, 200, 734);
    put (viewport, 228, 70, 996, 640);
    rackHolder.setSize (980, numSlots * 124);
    for (int i = 0; i < numSlots; ++i) slots[i]->setBounds (0, i * 124, 980, 120);
    for (int m = 0; m < 4; ++m)
    {
        put (*macros[m], 330 + m * 210, 724, 58, 58);
        put (*macroLabels[m], 300 + m * 210, 784, 120, 20);
    }
}

void TopRackEditor::paint (juce::Graphics& g)
{
    const float sc = (float) getWidth() / (float) W;
    g.addTransform (juce::AffineTransform::scale (sc));
    g.fillAll (bg);
    g.setGradientFill (juce::ColourGradient (panel, 0, 0, bg, 0, 60, false));
    g.fillRect (0, 0, W, 60);
    g.setColour (line); g.drawHorizontalLine (60, 0, (float) W);
    // logo: modulos apilados
    for (int i = 0; i < 3; ++i)
    {
        g.setColour (i == 1 ? gold : juce::Colour (0xff3c414b));
        g.fillRoundedRectangle (18.0f, 16.0f + i * 10.0f, 34.0f, 7.0f, 2.0f);
    }
    g.setColour (text); g.setFont (font (21.0f, true));
    g.drawText ("TOP RACK", 62, 9, 160, 24, juce::Justification::centredLeft);
    g.setColour (dim); g.setFont (font (11.0f));
    g.drawText ("by Dj Pava Hm Music", 63, 32, 160, 14, juce::Justification::centredLeft);
    g.setColour (dim); g.setFont (font (11.0f, true));
    g.drawText ("DREAM STRIPS", 462, 14, 94, 32, juce::Justification::centredRight);
    const juce::StringArray gl { "ENTRADA", "SALIDA", "MEZCLA" };
    for (int i = 0; i < 3; ++i) g.drawText (gl[i], 1004 + i * 72, 48, 72, 12, juce::Justification::centred);
    // marca de dream strips guardados
    for (int i = 0; i < 8; ++i)
        if (proc.dream[(size_t) i].isValid()) { g.setColour (gold); g.fillEllipse (560.0f + i * 34 + 12, 48, 6, 6); }
    // barra de macros
    g.setColour (panel); g.fillRoundedRectangle (228.0f, 716.0f, 996.0f, 88.0f, 10.0f);
    g.setColour (dim); g.setFont (font (11.5f, true));
    g.drawText ("MACROS", 244, 724, 70, 18, juce::Justification::centredLeft);
    g.setFont (font (10.5f));
    g.drawFittedText (U ("Clic derecho en cualquier perilla → asignar a una macro"), 244, 744, 80, 50, juce::Justification::topLeft, 4);
    // medidor de salida
    const float pk = juce::jlimit (0.0f, 1.0f, (20.0f * std::log10 (proc.outPeak.load() + 1e-6f) + 60.0f) / 60.0f);
    g.setColour (panel2); g.fillRoundedRectangle (1170.0f, 726.0f, 12.0f, 70.0f, 4.0f);
    g.setColour (pk > 0.98f ? red : green); g.fillRoundedRectangle (1170.0f, 726.0f + 70.0f * (1 - pk), 12.0f, 70.0f * pk, 4.0f);
}

void TopRackEditor::timerCallback()
{
    for (auto* s : slots) s->tick();
    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1) presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    for (int m = 0; m < 4; ++m) if (macroLabels[m]->getText() != proc.macroNames[m] && ! macroLabels[m]->isBeingEdited()) macroLabels[m]->setText (proc.macroNames[m], juce::dontSendNotification);
    const float sc = (float) getWidth() / (float) W;
    repaint (juce::Rectangle<int> (1160, 720, 40, 84).transformedBy (juce::AffineTransform::scale (sc)));
    repaint (juce::Rectangle<int> (556, 44, 280, 14).transformedBy (juce::AffineTransform::scale (sc)));
}
