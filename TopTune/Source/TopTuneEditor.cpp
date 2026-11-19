#include "TopTuneEditor.h"

using namespace ttui;

static juce::Font font (float h, bool bold = false)
{
    return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
}

static juce::String noteName (int midi)
{
    return TopTuneProcessor::keyNames()[((midi % 12) + 12) % 12] + juce::String (midi / 12 - 1);
}

//==============================================================================
LnF::LnF()
{
    setColour (juce::Slider::textBoxTextColourId, C::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, C::dim);
    setColour (juce::ComboBox::backgroundColourId, C::panelHi);
    setColour (juce::ComboBox::outlineColourId, C::dim.withAlpha (0.25f));
    setColour (juce::ComboBox::textColourId, C::text);
    setColour (juce::ComboBox::arrowColourId, C::accent);
    setColour (juce::PopupMenu::backgroundColourId, C::panel);
    setColour (juce::PopupMenu::textColourId, C::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, C::accent.withAlpha (0.3f));
    setColour (juce::TextButton::textColourOffId, C::dim);
    setColour (juce::TextButton::textColourOnId, C::bg);
    setColour (juce::TextButton::buttonOnColourId, C::accent);
}

void LnF::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const auto col = s.findColour (juce::Slider::rotarySliderFillColourId);
    const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f, cx = b.getCentreX(), cy = b.getCentreY();
    const float lw = juce::jmax (3.0f, r * 0.1f), ar = r - lw;
    const float ang = a0 + pos * (a1 - a0);

    juce::Path track; track.addCentredArc (cx, cy, ar, ar, 0, a0, a1, true);
    g.setColour (C::bg.brighter (0.18f));
    g.strokePath (track, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const bool bip = s.getMinimum() < 0 && s.getMaximum() > 0;
    const float from = bip ? a0 + (float) s.valueToProportionOfLength (0.0) * (a1 - a0) : a0;
    juce::Path v; v.addCentredArc (cx, cy, ar, ar, 0, juce::jmin (from, ang), juce::jmax (from, ang), true);
    g.setColour (col);
    g.strokePath (v, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float kr = r * 0.66f;
    g.setGradientFill (juce::ColourGradient (C::panelHi.brighter (0.3f), cx, cy - kr, C::bg, cx, cy + kr, false));
    g.fillEllipse (cx - kr, cy - kr, 2 * kr, 2 * kr);
    juce::Path ptr; ptr.addRoundedRectangle (-lw * 0.45f, -kr + 4, lw * 0.9f, kr * 0.42f, lw * 0.4f);
    g.setColour (col.brighter (0.3f));
    g.fillPath (ptr, juce::AffineTransform::rotation (ang).translated (cx, cy));
}

void LnF::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1);
    const auto on = b.findColour (juce::TextButton::buttonOnColourId);
    auto fill = b.getToggleState() ? on : C::panelHi;
    if (over) fill = fill.brighter (0.1f);
    if (down) fill = fill.darker (0.2f);
    g.setColour (fill); g.fillRoundedRectangle (r, 6);
    g.setColour (b.getToggleState() ? on.brighter (0.3f) : C::dim.withAlpha (0.25f));
    g.drawRoundedRectangle (r, 6, 1);
}

juce::Font LnF::getTextButtonFont (juce::TextButton&, int h) { return font (juce::jmin (13.0f, h * 0.5f), true); }
juce::Font LnF::getComboBoxFont (juce::ComboBox&) { return font (14.0f); }
juce::Font LnF::getLabelFont (juce::Label& l) { return font (juce::jmin (13.0f, juce::jmax (8, l.getHeight()) * 0.8f)); }

//==============================================================================
Knob::Knob (APVTS& s, const juce::String& id, const juce::String& label, juce::Colour col)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    slider.setColour (juce::Slider::rotarySliderFillColourId, col);
    addAndMakeVisible (slider);
    name.setText (label, juce::dontSendNotification);
    name.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (name);
    att = std::make_unique<APVTS::SliderAttachment> (s, id, slider);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    name.setBounds (r.removeFromTop (18));
    slider.setBounds (r);
}

//==============================================================================
void PitchGraph::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (C::panel); g.fillRoundedRectangle (b, 10);
    auto area = b.reduced (12, 12).withTrimmedLeft (34);

    const auto& m = proc.meters;
    const int N = TopTuneMeters::historySize, pos = m.histPos.load();

    // Centrar la vista en la voz reciente
    float sum = 0; int cnt = 0;
    for (int k = 0; k < 60; ++k) { const float v = m.inHist[(pos - k + N) % N]; if (v > 0) { sum += v; ++cnt; } }
    if (cnt > 0) centre += (sum / cnt - centre) * 0.08f;
    const float lo = centre - 8.5f, hi = centre + 8.5f;
    auto yOf = [&] (float midi) { return area.getBottom() - (midi - lo) / (hi - lo) * area.getHeight(); };

    // Lineas de notas (las de la escala resaltadas)
    const int mask = proc.getActiveMask();
    for (int note = (int) std::ceil (lo); note <= (int) hi; ++note)
    {
        const bool inScale = mask & (1 << (((note % 12) + 12) % 12));
        const float y = yOf ((float) note);
        g.setColour (inScale ? C::accent.withAlpha (0.16f) : C::dim.withAlpha (0.06f));
        g.fillRect (area.getX(), y - 0.5f, area.getWidth(), inScale ? 1.5f : 1.0f);
        g.setFont (font (10.5f, inScale));
        g.setColour (inScale ? C::text.withAlpha (0.8f) : C::dim.withAlpha (0.45f));
        g.drawText (noteName (note), (int) b.getX() + 6, (int) y - 7, 36, 14, juce::Justification::centredLeft);
    }

    auto drawLine = [&] (const float* hist, juce::Colour col, float width)
    {
        juce::Path p; bool started = false;
        for (int k = 0; k < N; ++k)
        {
            const float v = hist[(pos + 1 + k) % N];
            const float x = area.getX() + area.getWidth() * (float) k / (float) (N - 1);
            if (v <= 0 || v < lo - 2 || v > hi + 2) { started = false; continue; }
            if (! started) { p.startNewSubPath (x, yOf (v)); started = true; }
            else p.lineTo (x, yOf (v));
        }
        g.setColour (col);
        g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };
    drawLine (m.inHist, C::dim.withAlpha (0.7f), 1.5f);
    drawLine (m.outHist, C::accent, 2.5f);

    // Nota actual grande
    const int tn = (int) m.targetNote.load();
    g.setFont (font (34.0f, true));
    g.setColour (tn >= 0 ? C::text : C::dim.withAlpha (0.5f));
    g.drawText (tn >= 0 ? noteName (tn) : juce::String ("--"), area.removeFromTop (40).removeFromRight (120), juce::Justification::centredRight);
    if (tn >= 0)
    {
        const float cents = (m.inMidi.load() - (float) tn) * 100.0f;
        g.setFont (font (12.0f));
        g.setColour (std::abs (cents) < 10 ? C::accent : C::accent2);
        g.drawText ((cents >= 0 ? "+" : "") + juce::String ((int) cents) + " cents  |  " + juce::String (m.pitchHz.load(), 1) + " Hz",
                    area.removeFromTop (16).removeFromRight (200), juce::Justification::centredRight);
    }

    g.setFont (font (11.0f));
    g.setColour (C::dim);
    g.drawText ("gris = tu voz   verde = salida afinada", b.reduced (14, 8).removeFromBottom (14), juce::Justification::centredRight);
}

//==============================================================================
juce::Rectangle<float> ScaleKeyboard::keyRect (int pc) const
{
    static const int whiteIdx[12] { 0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6 };
    static const float blackPos[12] { 0, 1, 0, 2, 0, 0, 4, 0, 5, 0, 6, 0 };
    auto b = getLocalBounds().toFloat().reduced (2);
    const float ww = b.getWidth() / 7.0f;
    if (whiteIdx[pc] >= 0) return { b.getX() + ww * whiteIdx[pc], b.getY(), ww - 2, b.getHeight() };
    const float bw = ww * 0.6f;
    return { b.getX() + ww * blackPos[pc] - bw * 0.5f - 1, b.getY(), bw, b.getHeight() * 0.6f };
}

int ScaleKeyboard::hitTest (juce::Point<float> pt) const
{
    for (int pc : { 1, 3, 6, 8, 10 }) if (keyRect (pc).contains (pt)) return pc;
    for (int pc : { 0, 2, 4, 5, 7, 9, 11 }) if (keyRect (pc).contains (pt)) return pc;
    return -1;
}

void ScaleKeyboard::paint (juce::Graphics& g)
{
    const int mask = proc.getActiveMask();
    const int tn = (int) proc.meters.targetNote.load();
    const int cur = tn >= 0 ? tn % 12 : -1;

    auto drawKey = [&] (int pc, bool black)
    {
        auto r = keyRect (pc);
        const bool on = mask & (1 << pc);
        juce::Colour fill = black ? juce::Colour (0xff20232c) : juce::Colour (0xffd9dce4);
        if (on) fill = black ? C::accent.darker (0.35f) : C::accent.brighter (0.25f);
        g.setColour (fill); g.fillRoundedRectangle (r, 4);
        if (pc == cur) { g.setColour (C::accent2); g.drawRoundedRectangle (r.reduced (1), 4, 3); }
        g.setColour (on ? C::bg : (black ? C::dim : juce::Colour (0xff555a66)));
        g.setFont (font (11.0f, true));
        g.drawText (TopTuneProcessor::keyNames()[pc], r.removeFromBottom (18).toNearestInt(), juce::Justification::centred);
    };
    for (int pc : { 0, 2, 4, 5, 7, 9, 11 }) drawKey (pc, false);
    for (int pc : { 1, 3, 6, 8, 10 }) drawKey (pc, true);
}

void ScaleKeyboard::mouseDown (const juce::MouseEvent& e)
{
    const int pc = hitTest (e.position);
    if (pc < 0) return;
    // Al tocar una tecla se crea/edita la escala personalizada
    proc.meters.customMask.store (proc.getActiveMask() ^ (1 << pc));
    auto* sp = proc.apvts.getParameter ("scale");
    sp->setValueNotifyingHost (sp->convertTo0to1 ((float) TopTuneProcessor::customScaleIndex));
    repaint();
}

//==============================================================================
MainView::MainView (TopTuneProcessor& p)
    : proc (p), graph (p), keyboard (p),
      speed (p.apvts, "speed", "VELOCIDAD", C::accent),
      amount (p.apvts, "amount", "CANTIDAD", C::accent),
      shift (p.apvts, "shift", "PITCH SHIFT", C::blue),
      fine (p.apvts, "fine", "FINO", C::blue),
      formant (p.apvts, "formant", "FORMANT", C::accent2),
      mix (p.apvts, "mix", "MIX", C::dim.brighter (0.4f)),
      out (p.apvts, "out", "OUTPUT", C::dim.brighter (0.4f))
{
    for (auto* c : std::initializer_list<juce::Component*> { &presetBox, &prevBtn, &nextBtn, &graph, &keyboard,
             &keyBox, &scaleBox, &styleBox, &midiBox, &keyLbl, &scaleLbl, &styleLbl, &midiLbl, &liveBtn,
             &detectBtn, &applyBtn, &speed, &amount, &shift, &fine, &formant, &mix, &out })
        addAndMakeVisible (c);

    keyBox.addItemList (TopTuneProcessor::keyNames(), 1);
    scaleBox.addItemList (TopTuneProcessor::scaleNames(), 1);
    styleBox.addItemList ({ "Natural", "Expresivo", "Sostenido" }, 1);
    midiBox.addItemList ({ "MIDI Off", "MIDI Repitch", "MIDI Escala" }, 1);
    keyAtt   = std::make_unique<APVTS::ComboBoxAttachment> (p.apvts, "key", keyBox);
    scaleAtt = std::make_unique<APVTS::ComboBoxAttachment> (p.apvts, "scale", scaleBox);
    styleAtt = std::make_unique<APVTS::ComboBoxAttachment> (p.apvts, "style", styleBox);
    midiAtt  = std::make_unique<APVTS::ComboBoxAttachment> (p.apvts, "midi", midiBox);

    for (auto [lbl, txt] : { std::pair<juce::Label*, const char*> { &keyLbl, "TONALIDAD" }, { &scaleLbl, "ESCALA" },
                             { &styleLbl, "ESTILO" }, { &midiLbl, "MIDI" } })
        lbl->setText (txt, juce::dontSendNotification);

    liveBtn.setClickingTogglesState (true);
    liveBtn.setColour (juce::TextButton::buttonOnColourId, C::accent2);
    liveAtt = std::make_unique<APVTS::ButtonAttachment> (p.apvts, "live", liveBtn);

    detectBtn.setClickingTogglesState (true);
    detectBtn.onClick = [this]
    {
        if (detectBtn.getToggleState()) { proc.startKeyDetection(); keyResult = "Escuchando... canta o reproduce la pista"; foundRoot = -1; }
        else proc.stopKeyDetection();
    };
    applyBtn.onClick = [this]
    {
        if (foundRoot < 0) return;
        auto* k = proc.apvts.getParameter ("key");
        auto* s = proc.apvts.getParameter ("scale");
        k->setValueNotifyingHost (k->convertTo0to1 ((float) foundRoot));
        s->setValueNotifyingHost (s->convertTo0to1 (foundMinor ? 2.0f : 1.0f));
    };

    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { if (presetBox.getSelectedId() > 0) proc.setCurrentProgram (presetBox.getSelectedId() - 1); };
    auto step = [this] (int d)
    {
        const int n = proc.getNumPrograms();
        proc.setCurrentProgram ((proc.getCurrentProgram() + d + n) % n);
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    };
    prevBtn.onClick = [step] { step (-1); };
    nextBtn.onClick = [step] { step (1); };
}

void MainView::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (C::bg.brighter (0.07f), 0, 0, C::bg, 0, (float) H, false));
    g.fillAll();

    g.setFont (font (28.0f, true));
    g.setColour (C::text);  g.drawText ("TOP", 20, 12, 70, 36, juce::Justification::centredLeft);
    g.setColour (C::accent); g.drawText ("TUNE", 80, 12, 120, 36, juce::Justification::centredLeft);
    g.setFont (font (11.0f));
    g.setColour (C::dim);
    g.drawText ("AFINACION EN TIEMPO REAL  -  PITCH & FORMANT  -  MIDI", 180, 22, 360, 18, juce::Justification::centredLeft);
    g.setColour (C::accent.withAlpha (0.15f)); g.fillRect (0, 59, W, 1);

    // Panel derecho
    g.setColour (C::panel);
    g.fillRoundedRectangle (648.0f, 72.0f, 300.0f, 352.0f, 10.0f);
    g.fillRoundedRectangle (12.0f, 444.0f, 936.0f, 146.0f, 10.0f);

    // Resultado del detector de tonalidad
    auto r = keyResultArea.toFloat();
    g.setColour (C::bg); g.fillRoundedRectangle (r, 6);
    g.setFont (font (13.0f, foundRoot >= 0));
    g.setColour (foundRoot >= 0 ? C::accent : C::dim);
    g.drawText (keyResult, keyResultArea.reduced (8, 0), juce::Justification::centred);

    g.setFont (font (11.0f));
    g.setColour (C::dim);
    g.drawText (proc.getLatencySamples() > 0 ? "Latencia: " + juce::String (1000.0 * proc.getLatencySamples() / juce::jmax (1.0, proc.getSampleRate()), 1) + " ms"
                                             : juce::String(),
                660, 396, 276, 18, juce::Justification::centredLeft);
}

void MainView::resized()
{
    auto hdr = juce::Rectangle<int> (W - 380, 14, 360, 32);
    nextBtn.setBounds (hdr.removeFromRight (32)); hdr.removeFromRight (4);
    prevBtn.setBounds (hdr.removeFromLeft (32));  hdr.removeFromLeft (4);
    presetBox.setBounds (hdr);

    graph.setBounds (12, 72, 624, 270);
    keyboard.setBounds (12, 350, 624, 74);

    auto col = juce::Rectangle<int> (660, 82, 276, 330);
    auto row = [&col] (juce::Label& l, juce::ComboBox& c)
    {
        auto rr = col.removeFromTop (30);
        l.setBounds (rr.removeFromLeft (84));
        c.setBounds (rr);
        col.removeFromTop (8);
    };
    row (keyLbl, keyBox);
    row (scaleLbl, scaleBox);
    row (styleLbl, styleBox);
    row (midiLbl, midiBox);
    liveBtn.setBounds (col.removeFromTop (30));
    col.removeFromTop (16);
    detectBtn.setBounds (col.removeFromTop (32));
    col.removeFromTop (8);
    keyResultArea = col.removeFromTop (30);
    col.removeFromTop (8);
    applyBtn.setBounds (col.removeFromTop (30));

    auto knobs = juce::Rectangle<int> (20, 452, 920, 132);
    const int w = knobs.getWidth() / 7;
    for (auto* k : { &speed, &amount, &shift, &fine, &formant, &mix, &out })
        k->setBounds (knobs.removeFromLeft (w).reduced (6, 0));
}

void MainView::tick()
{
    graph.repaint();
    keyboard.repaint();

    if (proc.meters.customMaskLearned.exchange (false))
    {
        auto* sp = proc.apvts.getParameter ("scale");
        sp->setValueNotifyingHost (sp->convertTo0to1 ((float) TopTuneProcessor::customScaleIndex));
    }

    if (proc.meters.keyListening.load())
    {
        float h[12]; float total = 0;
        for (int i = 0; i < 12; ++i) { h[i] = proc.meters.keyHist[i].load(); total += h[i]; }
        if (total > 2.0f)
        {
            int root; bool minor; float conf;
            toptune::KeyFinder::find (h, root, minor, conf);
            foundRoot = root; foundMinor = minor;
            keyResult = TopTuneProcessor::keyNames()[root] + (minor ? " Menor" : " Mayor")
                      + "   (" + juce::String (juce::roundToInt (conf * 100)) + "%)";
        }
        repaint (keyResultArea);
    }

    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
}

//==============================================================================
TopTuneEditor::TopTuneEditor (TopTuneProcessor& p) : AudioProcessorEditor (&p), view (p)
{
    view.setLookAndFeel (&lnf);
    addAndMakeVisible (view);
    setResizable (true, true);
    setResizeLimits (ttui::MainView::W * 6 / 10, ttui::MainView::H * 6 / 10, ttui::MainView::W * 2, ttui::MainView::H * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) ttui::MainView::W / ttui::MainView::H);
    setSize (ttui::MainView::W, ttui::MainView::H);
    startTimerHz (30);
}

TopTuneEditor::~TopTuneEditor()
{
    stopTimer();
    view.setLookAndFeel (nullptr);
}

void TopTuneEditor::paint (juce::Graphics& g) { g.fillAll (ttui::C::bg); }

void TopTuneEditor::resized()
{
    view.setBounds (0, 0, ttui::MainView::W, ttui::MainView::H);
    view.setTransform (juce::AffineTransform::scale ((float) getWidth() / ttui::MainView::W));
}
