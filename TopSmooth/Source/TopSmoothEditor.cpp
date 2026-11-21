#include "TopSmoothEditor.h"

using namespace tsmui;

namespace
{
    juce::String U (const char* s) { return juce::String::fromUTF8 (s); }
    const juce::Colour bg (0xff0f1219), panel (0xff161b25), panel2 (0xff1d2330), line (0xff2a3242),
                       text (0xffe8ecf4), dim (0xff8992a6), mint (0xff5ee8c5), coral (0xffff6b6b),
                       violet (0xffa58bff), sky (0xff5fb4ff), sand (0xffffc66b);
    juce::Font font (float h, bool bold = false) { return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain)); }
    const juce::Colour nodeCol[5] { sky, mint, sand, coral, violet };
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
    setColour (juce::PopupMenu::highlightedBackgroundColourId, mint.withAlpha (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::TooltipWindow::backgroundColourId, panel2);
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TooltipWindow::outlineColourId, line);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::BubbleComponent::backgroundColourId, panel2);
    setColour (juce::BubbleComponent::outlineColourId, line);
}
void Look::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);
    const float r = (float) juce::jmin (w, h) * 0.5f - 6.0f;
    const juce::Point<float> c ((float) x + (float) w * 0.5f, (float) y + (float) h * 0.5f);
    const float a = a0 + pos * (a1 - a0);
    const float th = r > 50 ? 9.0f : 5.0f;
    juce::Path tr; tr.addCentredArc (c.x, c.y, r, r, 0, a0, a1, true);
    g.setColour (line); g.strokePath (tr, juce::PathStrokeType (th, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    // valor: desde el centro en perillas bipolares
    const bool bip = s.getMinimum() < 0 && s.getMaximum() > 0;
    const float from = bip ? (a0 + a1) * 0.5f : a0;
    juce::Path v; v.addCentredArc (c.x, c.y, r, r, 0, juce::jmin (from, a), juce::jmax (from, a), true);
    g.setColour (accent.withAlpha (0.25f)); g.strokePath (v, juce::PathStrokeType (th * 2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (accent); g.strokePath (v, juce::PathStrokeType (th, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const float ri = r - th - 6.0f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2d3446), c.x, c.y - ri, juce::Colour (0xff141922), c.x, c.y + ri, false));
    g.fillEllipse (c.x - ri, c.y - ri, ri * 2, ri * 2);
    g.setColour (juce::Colours::white.withAlpha (0.07f)); g.drawEllipse (c.x - ri, c.y - ri, ri * 2, ri * 2, 1.0f);
    g.setColour (text);
    g.drawLine (juce::Line<float> ({ c.x + ri * 0.3f * std::sin (a), c.y - ri * 0.3f * std::cos (a) }, { c.x + (ri - 3) * std::sin (a), c.y - (ri - 3) * std::cos (a) }), r > 50 ? 3.5f : 2.5f);
}
void Look::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1);
    const auto accent = b.findColour (juce::TextButton::buttonOnColourId);
    if (b.getToggleState()) { g.setColour (accent.withAlpha (0.25f)); g.fillRoundedRectangle (r.expanded (1), 9); g.setColour (accent); g.fillRoundedRectangle (r, 8); return; }
    g.setColour (down ? panel2.brighter (0.2f) : over ? panel2.brighter (0.08f) : panel2); g.fillRoundedRectangle (r, 8);
    g.setColour (over ? accent.withAlpha (0.7f) : line); g.drawRoundedRectangle (r, 8, 1.2f);
}
void Look::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (b.getToggleState() ? bg : text);
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 2), juce::Justification::centred, 1);
}
juce::Font Look::getTextButtonFont (juce::TextButton&, int h) { return font (juce::jmin (13.0f, (float) h * 0.42f), true); }
juce::Font Look::getComboBoxFont (juce::ComboBox&) { return font (13.5f); }
void Look::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (panel2); g.fillRoundedRectangle (r, 8);
    g.setColour (line); g.drawRoundedRectangle (r, 8, 1.2f);
    juce::Path a; const float ax = (float) w - 15, ay = (float) h * 0.5f;
    a.addTriangle (ax - 4.5f, ay - 2.0f, ax + 4.5f, ay - 2.0f, ax, ay + 3.5f);
    g.setColour (dim); g.fillPath (a);
}

//==============================================================================
Knob::Knob (APVTS& s, const juce::String& id, const juce::String& t, juce::Colour accent, bool b) : state (s), title (t), big (b)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.22f, juce::MathConstants<float>::pi * 2.78f, true);
    slider.setPopupDisplayEnabled (true, true, nullptr);
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
    att = std::make_unique<APVTS::SliderAttachment> (s, id, slider);
}
void Knob::rebind (const juce::String& id, const juce::String& t)
{
    att.reset();
    title = t;
    att = std::make_unique<APVTS::SliderAttachment> (state, id, slider);
    repaint();
}
void Knob::resized() { slider.setBounds (getLocalBounds().withTrimmedTop (big ? 26 : 18).withTrimmedBottom (big ? 26 : 18)); }
void Knob::paint (juce::Graphics& g)
{
    g.setColour (big ? text : dim); g.setFont (font (big ? 15.0f : 11.5f, true));
    g.drawText (title, 0, 0, getWidth(), big ? 22 : 16, juce::Justification::centred);
    g.setColour (text); g.setFont (font (big ? 18.0f : 12.5f, true));
    g.drawText (slider.getTextFromValue (slider.getValue()), 0, getHeight() - (big ? 24 : 17), getWidth(), big ? 22 : 16, juce::Justification::centred);
}

//==============================================================================
Graph::Graph (TopSmoothProcessor& p) : proc (p) {}

float Graph::xOf (float hz) const { return 8.0f + (float) (getWidth() - 16) * std::log (hz / 20.0f) / std::log (1000.0f); }
float Graph::hzOf (float x) const { return 20.0f * std::pow (1000.0f, (x - 8.0f) / (float) (getWidth() - 16)); }
float Graph::sensY (float db) const { return (float) getHeight() * 0.62f - db / 12.0f * (float) getHeight() * 0.26f; }
float Graph::specY (float db) const { return 22.0f + (float) (getHeight() - 44) * juce::jlimit (0.0f, 1.0f, -db / 96.0f); }

int Graph::hitTarget (juce::Point<float> p) const
{
    const auto s = proc.currentSettings();
    for (int i = 0; i < 5; ++i)
        if (p.getDistanceFrom ({ xOf (s.nf[(size_t) i]), sensY (s.ng[(size_t) i]) }) < 12.0f) return i;
    if (std::abs (p.x - xOf (s.lowCut)) < 7.0f) return 10;
    if (std::abs (p.x - xOf (s.highCut)) < 7.0f) return 11;
    return -1;
}
void Graph::set (const juce::String& id, float v)
{
    if (auto* p = proc.apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
}
void Graph::gesture (const juce::String& id, bool begin)
{
    if (auto* p = proc.apvts.getParameter (id)) { if (begin) p->beginChangeGesture(); else p->endChangeGesture(); }
}

void Graph::paint (juce::Graphics& g)
{
    const float W = (float) getWidth(), H = (float) getHeight();
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0c0f15)); g.fillRoundedRectangle (r, 12);
    g.saveState();
    juce::Path clip; clip.addRoundedRectangle (r, 12); g.reduceClipRegion (clip);
    // rejilla
    for (float f : { 30.f, 50.f, 100.f, 200.f, 300.f, 500.f, 1000.f, 2000.f, 3000.f, 5000.f, 10000.f, 20000.f })
    {
        const float x = xOf (f);
        g.setColour (line.withAlpha (0.55f)); g.drawVerticalLine ((int) x, 0, H);
        g.setColour (dim); g.setFont (font (10.5f));
        g.drawText (f >= 1000 ? juce::String (f / 1000.0f, 0) + "k" : juce::String ((int) f), (int) x - 20, (int) H - 18, 40, 14, juce::Justification::centred);
    }
    for (float db : { -24.f, -48.f, -72.f })
    {
        g.setColour (line.withAlpha (0.35f)); g.drawHorizontalLine ((int) specY (db), 0, W);
    }
    const auto& d = proc.display;
    const int P = tsm::Display::points;
    // espectro de entrada (relleno) y de salida (linea)
    juce::Path in, outP;
    in.startNewSubPath (xOf (tsm::Display::freqOf (0)), H);
    for (int i = 0; i < P; ++i)
    {
        const float x = xOf (tsm::Display::freqOf (i));
        in.lineTo (x, specY (d.inDb[(size_t) i].load()));
        if (i == 0) outP.startNewSubPath (x, specY (d.outDb[(size_t) i].load())); else outP.lineTo (x, specY (d.outDb[(size_t) i].load()));
    }
    in.lineTo (xOf (tsm::Display::freqOf (P - 1)), H); in.closeSubPath();
    g.setGradientFill (juce::ColourGradient (sky.withAlpha (0.28f), 0, 40, sky.withAlpha (0.04f), 0, H, false));
    g.fillPath (in);
    g.setColour (mint.withAlpha (0.9f)); g.strokePath (outP, juce::PathStrokeType (1.6f));
    // reduccion en vivo (cae desde arriba)
    {
        juce::Path red; red.startNewSubPath (xOf (tsm::Display::freqOf (0)), 0);
        float mx = 0;
        for (int i = 0; i < P; ++i)
        {
            const float v = d.redDb[(size_t) i].load(); mx = juce::jmax (mx, v);
            red.lineTo (xOf (tsm::Display::freqOf (i)), juce::jmin (H * 0.48f, v / 24.0f * H * 0.48f));
        }
        red.lineTo (xOf (tsm::Display::freqOf (P - 1)), 0); red.closeSubPath();
        g.setGradientFill (juce::ColourGradient (coral.withAlpha (0.85f), 0, 0, coral.withAlpha (0.25f), 0, H * 0.4f, false));
        g.fillPath (red);
        g.setColour (bg.withAlpha (0.8f)); g.fillRoundedRectangle (W - 170, 26, 160, 20, 6);
        g.setColour (coral); g.setFont (font (11.5f, true));
        g.drawText (U ("reducción máx ") + juce::String (mx, 1) + " dB", (int) W - 168, 28, 156, 16, juce::Justification::centred);
    }
    // zonas fuera de los cortes
    const auto s = proc.currentSettings();
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRect (0.0f, 0.0f, xOf (s.lowCut), H);
    g.fillRect (xOf (s.highCut), 0.0f, W - xOf (s.highCut), H);
    for (int k = 10; k <= 11; ++k)
    {
        const float x = xOf (k == 10 ? s.lowCut : s.highCut);
        g.setColour (hover == k || dragging == k ? text : dim);
        g.drawLine (x, 0, x, H, hover == k || dragging == k ? 2.5f : 1.5f);
        g.fillRoundedRectangle (x - 5, H * 0.5f - 18, 10, 36, 4);
    }
    // curva de sensibilidad (EQ de foco)
    {
        juce::Path c;
        for (int i = 0; i <= 300; ++i)
        {
            const float f = 20.0f * std::pow (1000.0f, (float) i / 300.0f);
            const float y = sensY (tsm::sensitivityDb (f, s));
            if (i == 0) c.startNewSubPath (xOf (f), y); else c.lineTo (xOf (f), y);
        }
        g.setColour (juce::Colours::white.withAlpha (0.18f)); g.drawHorizontalLine ((int) sensY (0), 0, W);
        g.setColour (violet.withAlpha (0.25f)); g.strokePath (c, juce::PathStrokeType (6.0f));
        g.setColour (violet); g.strokePath (c, juce::PathStrokeType (2.0f));
    }
    for (int i = 0; i < 5; ++i)
    {
        const juce::Point<float> pt (xOf (s.nf[(size_t) i]), sensY (s.ng[(size_t) i]));
        const bool on = i == selected || i == hover || i == dragging;
        g.setColour (nodeCol[i].withAlpha (on ? 0.35f : 0.15f)); g.fillEllipse (pt.x - 15, pt.y - 15, 30, 30);
        g.setColour (on ? nodeCol[i] : bg); g.fillEllipse (pt.x - 10, pt.y - 10, 20, 20);
        g.setColour (nodeCol[i]); g.drawEllipse (pt.x - 10, pt.y - 10, 20, 20, 2.0f);
        g.setColour (on ? bg : text); g.setFont (font (11.0f, true));
        g.drawText (juce::String (i + 1), juce::Rectangle<float> (20, 20).withCentre (pt), juce::Justification::centred);
    }
    g.restoreState();
    g.setColour (line); g.drawRoundedRectangle (r.reduced (0.5f), 12, 1.0f);
    g.setColour (dim); g.setFont (font (11.0f));
    g.drawText (U ("azul = entra  ·  verde = sale  ·  rojo = lo que se quita  ·  morado = foco (arrastra los puntos)"), 12, 6, (int) W - 220, 16, juce::Justification::centredLeft);
}

void Graph::mouseMove (const juce::MouseEvent& e)
{
    const int h = hitTarget (e.position);
    if (h != hover) { hover = h; repaint(); }
    setMouseCursor (h >= 10 ? juce::MouseCursor::LeftRightResizeCursor : h >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
}
void Graph::mouseDown (const juce::MouseEvent& e)
{
    dragging = hitTarget (e.position);
    if (dragging >= 0 && dragging < 5)
    {
        selected = dragging;
        if (onSelect) onSelect (selected);
        const auto n = "n" + juce::String (dragging + 1);
        gesture (n + "_f", true); gesture (n + "_g", true);
    }
    else if (dragging == 10) gesture ("lowcut", true);
    else if (dragging == 11) gesture ("highcut", true);
    repaint();
}
void Graph::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0) return;
    const float hz = juce::jlimit (20.0f, 20000.0f, hzOf (e.position.x));
    if (dragging < 5)
    {
        const auto n = "n" + juce::String (dragging + 1);
        set (n + "_f", juce::jlimit (30.0f, 18000.0f, hz));
        set (n + "_g", juce::jlimit (-12.0f, 12.0f, ((float) getHeight() * 0.62f - e.position.y) / ((float) getHeight() * 0.26f) * 12.0f));
    }
    else if (dragging == 10) set ("lowcut", juce::jlimit (20.0f, 2000.0f, hz));
    else set ("highcut", juce::jlimit (1000.0f, 20000.0f, hz));
    repaint();
}
void Graph::mouseUp (const juce::MouseEvent&)
{
    if (dragging >= 0 && dragging < 5) { const auto n = "n" + juce::String (dragging + 1); gesture (n + "_f", false); gesture (n + "_g", false); }
    else if (dragging == 10) gesture ("lowcut", false);
    else if (dragging == 11) gesture ("highcut", false);
    dragging = -1;
}
void Graph::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int h = hitTarget (e.position);
    if (h >= 0 && h < 5) set ("n" + juce::String (h + 1) + "_g", 0.0f);
    else if (h == 10) set ("lowcut", 20.0f);
    else if (h == 11) set ("highcut", 20000.0f);
}
void Graph::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const int h = hitTarget (e.position);
    const int idx = h >= 0 && h < 5 ? h : selected;
    const auto id = "n" + juce::String (idx + 1) + "_q";
    if (auto* p = proc.apvts.getParameter (id))
    {
        const float q = proc.apvts.getRawParameterValue (id)->load();
        p->setValueNotifyingHost (p->convertTo0to1 (juce::jlimit (0.3f, 4.0f, q * (1.0f + w.deltaY))));
    }
}

//==============================================================================
TopSmoothEditor::TopSmoothEditor (TopSmoothProcessor& p)
    : AudioProcessorEditor (&p), proc (p), graph (p),
      depth (p.apvts, "depth", "PROFUNDIDAD", coral, true),
      detail (p.apvts, "detail", "DETALLE", mint), attack (p.apvts, "attack", "ATAQUE", sky), release (p.apvts, "release", "RELEASE", sky),
      maxcut (p.apvts, "maxcut", U ("CORTE MÁX"), coral), tilt (p.apvts, "tilt", U ("INCLINACIÓN"), violet),
      mix (p.apvts, "mix", "MEZCLA", sand), trim (p.apvts, "trim", "TRIM", sand),
      nodeF (p.apvts, "n3_f", "FRECUENCIA", sand), nodeG (p.apvts, "n3_g", "SENSIBILIDAD", sand), nodeQ (p.apvts, "n3_q", "ANCHO", sand)
{
    setLookAndFeel (&look);
    tips.setLookAndFeel (&look);
    addAndMakeVisible (graph);
    graph.onSelect = [this] (int i) { selectNode (i); };
    for (auto* k : { &depth, &detail, &attack, &release, &maxcut, &tilt, &mix, &trim, &nodeF, &nodeG, &nodeQ }) addAndMakeVisible (k);
    depth.slider.setTooltip (U ("Cuánto se bajan las resonancias. Empieza en 3-6 y sube hasta que la dureza desaparezca"));
    detail.slider.setTooltip (U ("Más detalle = cortes más estrechos solo en los picos más marcados. Menos = más amplio y suave"));
    attack.slider.setTooltip (U ("Qué tan rápido reacciona. Rápido para sibilancia, lento para dejar pasar el ataque"));
    release.slider.setTooltip (U ("Qué tan rápido suelta"));
    maxcut.slider.setTooltip (U ("Límite de cuánto puede bajar cualquier frecuencia (permite subir la profundidad sin pasarse)"));
    tilt.slider.setTooltip (U ("Inclina la sensibilidad: a la derecha trabaja más en agudos, a la izquierda en graves"));
    mix.slider.setTooltip (U ("Mezcla con la señal original"));
    trim.slider.setTooltip (U ("Volumen de salida"));

    soft.setColour (juce::TextButton::buttonOnColourId, mint);
    hard.setColour (juce::TextButton::buttonOnColourId, coral);
    soft.setTooltip (U ("Suave: el más transparente, con umbral que se adapta al material"));
    hard.setTooltip (U ("Duro: más preciso y fuerte, bueno para sonidos muy resonantes"));
    soft.onClick = [this] { if (auto* p = proc.apvts.getParameter ("mode")) p->setValueNotifyingHost (0.0f); };
    hard.onClick = [this] { if (auto* p = proc.apvts.getParameter ("mode")) p->setValueNotifyingHost (1.0f); };
    delta.setClickingTogglesState (true); bypass.setClickingTogglesState (true); sidechain.setClickingTogglesState (true);
    delta.setColour (juce::TextButton::buttonOnColourId, sand);
    bypass.setColour (juce::TextButton::buttonOnColourId, coral);
    sidechain.setColour (juce::TextButton::buttonOnColourId, violet);
    delta.setTooltip (U ("Escucha solo lo que se está quitando (para ajustar sin pasarte)"));
    sidechain.setTooltip (U ("Detecta con otra pista (entrada sidechain) y corta en esta"));
    btnAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "delta", delta));
    btnAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "bypass", bypass));
    btnAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "sidechain", sidechain));
    for (auto* b : { &soft, &hard, &delta, &bypass, &sidechain }) addAndMakeVisible (b);

    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { if (presetBox.getSelectedId() > 0) proc.setCurrentProgram (presetBox.getSelectedId() - 1); };
    latencyBox.addItemList ({ "Fase lineal", "Baja latencia (0 ms)" }, 1);
    stereoBox.addItemList ({ "Enlazado", "L / R", "Mid / Side" }, 1);
    latencyAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "latency", latencyBox);
    stereoAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "stereo", stereoBox);
    latencyBox.setTooltip (U ("Fase lineal: máxima calidad (latencia ~46 ms, el DAW la compensa). Baja latencia: 0 ms para grabar o en vivo"));
    stereoBox.setTooltip (U ("Enlazado: igual en los dos lados. L / R: cada lado por separado. Mid / Side: centro y lados por separado"));
    for (auto* c : { &presetBox, &latencyBox, &stereoBox }) addAndMakeVisible (c);

    selectNode (2);
    setResizable (true, true);
    setResizeLimits (W * 2 / 3, H * 2 / 3, W * 3 / 2, H * 3 / 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) W / H);
    setSize (W, H);
    startTimerHz (30);
}

TopSmoothEditor::~TopSmoothEditor()
{
    stopTimer();
    tips.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void TopSmoothEditor::selectNode (int i)
{
    graph.selected = i;
    const auto n = "n" + juce::String (i + 1);
    nodeF.rebind (n + "_f", "FRECUENCIA");
    nodeG.rebind (n + "_g", "SENSIBILIDAD");
    nodeQ.rebind (n + "_q", "ANCHO");
    for (auto* k : { &nodeF, &nodeG, &nodeQ }) k->slider.setColour (juce::Slider::rotarySliderFillColourId, nodeCol[i]);
    repaint();
}

void TopSmoothEditor::resized()
{
    const float s = (float) getWidth() / (float) W;
    for (auto* c : getChildren()) c->setTransform (juce::AffineTransform::scale (s));
    presetBox.setBounds (268, 12, 230, 32);
    latencyBox.setBounds (506, 12, 196, 32);
    stereoBox.setBounds (710, 12, 120, 32);
    sidechain.setBounds (838, 12, 92, 32);
    bypass.setBounds (936, 12, 88, 32);
    graph.setBounds (16, 64, 744, 392);
    const juce::Array<tsmui::Knob*> row { &detail, &attack, &release, &maxcut, &tilt, &mix, &trim };
    for (int i = 0; i < row.size(); ++i) row[i]->setBounds (16 + i * 106, 474, 102, 116);
    depth.setBounds (776, 60, 248, 214);
    soft.setBounds (790, 282, 106, 32);
    hard.setBounds (904, 282, 106, 32);
    delta.setBounds (790, 322, 220, 32);
    nodeF.setBounds (776, 390, 82, 104);
    nodeG.setBounds (859, 390, 82, 104);
    nodeQ.setBounds (942, 390, 82, 104);
}

void TopSmoothEditor::paint (juce::Graphics& g)
{
    const float sc = (float) getWidth() / (float) W;
    g.addTransform (juce::AffineTransform::scale (sc));
    g.fillAll (bg);
    g.setGradientFill (juce::ColourGradient (panel, 0, 0, bg, 0, 56, false));
    g.fillRect (0, 0, W, 56);
    // logo: onda que se alisa
    {
        juce::Path w1;
        for (int i = 0; i <= 40; ++i)
        {
            const float t = (float) i / 40.0f;
            const float y = 28.0f + std::sin (t * 16.0f) * 9.0f * (1.0f - t) * (1.0f - t);
            if (i == 0) w1.startNewSubPath (18.0f + t * 36.0f, y); else w1.lineTo (18.0f + t * 36.0f, y);
        }
        g.setGradientFill (juce::ColourGradient (coral, 18, 0, mint, 54, 0, false));
        g.strokePath (w1, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setColour (text); g.setFont (font (21.0f, true));
    g.drawText ("TOP SMOOTH", 64, 8, 200, 24, juce::Justification::centredLeft);
    g.setColour (dim); g.setFont (font (11.0f));
    g.drawText (U ("by Dj Pava Hm Music"), 65, 31, 200, 14, juce::Justification::centredLeft);

    // panel derecho
    g.setColour (panel); g.fillRoundedRectangle (776.0f, 64.0f, 248.0f, 526.0f, 12.0f);
    g.setColour (line); g.drawRoundedRectangle (776.5f, 64.5f, 247.0f, 525.0f, 12.0f, 1.0f);
    g.setColour (nodeCol[graph.selected]); g.fillRoundedRectangle (790.0f, 368.0f, 4.0f, 14.0f, 2.0f);
    g.setColour (text); g.setFont (font (12.5f, true));
    g.drawText (U ("FOCO  ·  PUNTO ") + juce::String (graph.selected + 1), 800, 363, 200, 24, juce::Justification::centredLeft);
    g.setColour (dim); g.setFont (font (11.0f));
    g.drawText (U ("rueda del ratón sobre un punto = ancho"), 790, 494, 230, 14, juce::Justification::centredLeft);
    // medidores
    auto meter = [&] (const juce::String& t, float db, int y)
    {
        const auto r = juce::Rectangle<float> (826.0f, (float) y, 180.0f, 10.0f);
        g.setColour (dim); g.setFont (font (11.0f, true)); g.drawText (t, 790, y - 3, 34, 16, juce::Justification::centredLeft);
        g.setColour (panel2); g.fillRoundedRectangle (r, 5);
        const float v = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
        g.setGradientFill (juce::ColourGradient (mint, r.getX(), 0, coral, r.getRight(), 0, false));
        g.fillRoundedRectangle (r.withWidth (juce::jmax (4.0f, r.getWidth() * v)), 5);
    };
    meter ("IN", inM, 528);
    meter ("OUT", outM, 552);
}

void TopSmoothEditor::timerCallback()
{
    const int mode = (int) proc.apvts.getRawParameterValue ("mode")->load();
    soft.setToggleState (mode == 0, juce::dontSendNotification);
    hard.setToggleState (mode == 1, juce::dontSendNotification);
    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1) presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    auto fall = [] (float& m, float v) { m = v > m ? v : m - 1.2f; };
    fall (inM, proc.inDb.load()); fall (outM, proc.outDb.load());
    graph.repaint();
    repaint (juce::Rectangle<int> (780, 520, 240, 50).transformedBy (juce::AffineTransform::scale ((float) getWidth() / (float) W)));
}
