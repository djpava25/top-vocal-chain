#include "TopVoiceMidiEditor.h"

using namespace vmui;

namespace
{
    juce::String U (const char* s) { return juce::String::fromUTF8 (s); }
    const juce::Colour bg (0xff0d0f16), card (0xff151925), card2 (0xff1b2030), line (0xff272d40),
                       text (0xffe9edf6), dim (0xff8b93a9),
                       teal (0xff2fe0c5), pink (0xffff4f9e), amber (0xffffc04a), violet (0xff8d70ff), blue (0xff47a6ff);
    juce::Font font (float h, bool bold = false) { return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain)); }
    juce::String noteName (int n)
    {
        static const char* nm[12] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        if (n < 0) return "-";
        return juce::String (nm[n % 12]) + juce::String (n / 12 - 1);
    }
    void cardBox (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title = {}, juce::Colour accent = teal)
    {
        g.setColour (card); g.fillRoundedRectangle (r, 12);
        g.setColour (line); g.drawRoundedRectangle (r.reduced (0.5f), 12, 1.0f);
        if (title.isNotEmpty())
        {
            g.setColour (accent); g.fillRoundedRectangle (r.getX() + 16, r.getY() + 15, 4, 14, 2);
            g.setColour (text); g.setFont (font (13.0f, true));
            g.drawText (title, (int) r.getX() + 28, (int) r.getY() + 10, 400, 24, juce::Justification::centredLeft);
        }
    }
    void setParam (juce::AudioProcessorValueTreeState& s, const juce::String& id, float plain)
    {
        if (auto* p = s.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (plain));
    }
}

//==============================================================================
Look::Look()
{
    setColour (juce::ComboBox::backgroundColourId, card2);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::outlineColourId, line);
    setColour (juce::ComboBox::arrowColourId, dim);
    setColour (juce::PopupMenu::backgroundColourId, card2);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, teal.withAlpha (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::TooltipWindow::backgroundColourId, card2);
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TooltipWindow::outlineColourId, line);
    setColour (juce::ToggleButton::textColourId, text);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, bg);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::BubbleComponent::backgroundColourId, card2);
    setColour (juce::BubbleComponent::outlineColourId, line);
}

void Look::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);
    const float r = (float) juce::jmin (w, h) * 0.5f - 6.0f;
    const juce::Point<float> c ((float) x + (float) w * 0.5f, (float) y + (float) h * 0.5f);
    const float a = a0 + pos * (a1 - a0);
    juce::Path track; track.addCentredArc (c.x, c.y, r, r, 0, a0, a1, true);
    g.setColour (line); g.strokePath (track, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Path val; val.addCentredArc (c.x, c.y, r, r, 0, a0, a, true);
    g.setColour (accent.withAlpha (0.25f)); g.strokePath (val, juce::PathStrokeType (10.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (accent); g.strokePath (val, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const float ri = r - 10.0f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2b3146), c.x, c.y - ri, juce::Colour (0xff141822), c.x, c.y + ri, false));
    g.fillEllipse (c.x - ri, c.y - ri, ri * 2, ri * 2);
    g.setColour (juce::Colours::white.withAlpha (0.06f)); g.drawEllipse (c.x - ri, c.y - ri, ri * 2, ri * 2, 1.0f);
    const juce::Point<float> p1 (c.x + (ri - 4) * std::sin (a), c.y - (ri - 4) * std::cos (a));
    const juce::Point<float> p0 (c.x + (ri * 0.35f) * std::sin (a), c.y - (ri * 0.35f) * std::cos (a));
    g.setColour (text); g.drawLine (juce::Line<float> (p0, p1), 2.5f);
}

void Look::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const auto accent = b.findColour (juce::TextButton::buttonOnColourId);
    const bool on = b.getToggleState();
    const float rad = juce::jmin (r.getHeight() * 0.5f, 10.0f);
    if (on) { g.setColour (accent.withAlpha (0.25f)); g.fillRoundedRectangle (r.expanded (1), rad + 1); g.setColour (accent); g.fillRoundedRectangle (r, rad); }
    else
    {
        g.setColour (down ? card2.brighter (0.2f) : over ? card2.brighter (0.08f) : card2); g.fillRoundedRectangle (r, rad);
        g.setColour (over ? accent.withAlpha (0.6f) : line); g.drawRoundedRectangle (r, rad, 1.2f);
    }
}
void Look::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (b.getToggleState() ? bg : text);
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (6, 2), juce::Justification::centred, 2);
}
juce::Font Look::getTextButtonFont (juce::TextButton&, int h) { return font (juce::jmin (14.0f, (float) h * 0.45f), true); }
juce::Font Look::getComboBoxFont (juce::ComboBox&) { return font (14.0f); }
void Look::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& cb)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (card2); g.fillRoundedRectangle (r, 8);
    g.setColour (cb.hasKeyboardFocus (true) ? teal.withAlpha (0.6f) : line); g.drawRoundedRectangle (r, 8, 1.2f);
    juce::Path arrow; const float ax = (float) w - 18, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 5, ay - 2.5f, ax + 5, ay - 2.5f, ax, ay + 3.5f);
    g.setColour (dim); g.fillPath (arrow);
}
void Look::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    const auto accent = b.findColour (juce::ToggleButton::tickColourId);
    auto r = b.getLocalBounds().toFloat();
    auto sw = juce::Rectangle<float> (r.getX() + 2, r.getCentreY() - 10, 38, 20);
    const bool on = b.getToggleState();
    g.setColour (on ? accent : (over ? line.brighter (0.2f) : line)); g.fillRoundedRectangle (sw, 10);
    const float kx = on ? sw.getRight() - 18 : sw.getX() + 2;
    g.setColour (on ? bg : text.withAlpha (0.8f)); g.fillEllipse (kx, sw.getY() + 2, 16, 16);
    g.setColour (text); g.setFont (font (14.0f, true));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (50), juce::Justification::centredLeft);
}
void Look::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider& s)
{
    auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (0, 2);
    const auto accent = s.findColour (juce::Slider::trackColourId);
    g.setColour (card2); g.fillRoundedRectangle (r, 7);
    g.setColour (accent.withAlpha (0.35f)); g.fillRoundedRectangle (r.withWidth (juce::jmax (8.0f, pos - r.getX())), 7);
    g.setColour (line); g.drawRoundedRectangle (r, 7, 1.0f);
    g.setColour (text); g.setFont (font (13.0f, true));
    const int v = (int) s.getValue();
    g.drawText ("Nota " + juce::String (v) + "  (" + noteName (v) + ")", r, juce::Justification::centred);
}

//==============================================================================
Knob::Knob (APVTS& s, const juce::String& id, const juce::String& t, juce::Colour accent) : title (t)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    slider.setPopupDisplayEnabled (true, true, nullptr);
    addAndMakeVisible (slider);
    att = std::make_unique<APVTS::SliderAttachment> (s, id, slider);
    slider.onValueChange = [this] { repaint(); };
}
void Knob::resized() { slider.setBounds (getLocalBounds().withTrimmedTop (18).withTrimmedBottom (18)); }
void Knob::paint (juce::Graphics& g)
{
    g.setColour (dim); g.setFont (font (12.0f, true));
    g.drawText (title.toUpperCase(), 0, 0, getWidth(), 16, juce::Justification::centred);
    g.setColour (text); g.setFont (font (13.0f, true));
    g.drawText (slider.getTextFromValue (slider.getValue()), 0, getHeight() - 17, getWidth(), 16, juce::Justification::centred);
}

//==============================================================================
KeyRow::KeyRow (APVTS& s, const juce::String& p, juce::StringArray l, juce::Colour a)
    : state (s), prefix (p), labels (std::move (l)), accent (a) {}

juce::Rectangle<float> KeyRow::keyRect (int i) const
{
    static const int whiteIndex[12] { 0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6 };
    static const float blackPos[12] { 0, 0.68f, 0, 1.72f, 0, 0, 3.66f, 0, 4.70f, 0, 5.74f, 0 };
    const float ww = (float) getWidth() / 7.0f, h = (float) getHeight();
    if (whiteIndex[i] >= 0) return { whiteIndex[i] * ww + 1, 0, ww - 2, h };
    return { blackPos[i] * ww, 0, ww * 0.62f, h * 0.6f };
}
void KeyRow::paint (juce::Graphics& g)
{
    auto on = [this] (int i) { return state.getRawParameterValue (prefix + juce::String (i))->load() > 0.5f; };
    for (int pass = 0; pass < 2; ++pass)
        for (int i = 0; i < 12; ++i)
        {
            const bool black = (i == 1 || i == 3 || i == 6 || i == 8 || i == 10);
            if ((pass == 0) == black) continue;
            const auto r = keyRect (i);
            const bool o = on (i);
            g.setColour (black ? (o ? accent.darker (0.3f) : juce::Colour (0xff222736)) : (o ? accent : juce::Colour (0xffdfe3ec)).withAlpha (o ? 1.0f : 0.85f));
            g.fillRoundedRectangle (r, 5);
            if (i == highlight) { g.setColour (juce::Colours::white); g.drawRoundedRectangle (r.reduced (1.5f), 5, 3.0f); }
            g.setColour (black ? text : bg);
            g.setFont (font (black ? 11.0f : 13.0f, true));
            g.drawText (labels[i], r.withTrimmedTop (r.getHeight() - 26), juce::Justification::centred);
        }
}
void KeyRow::mouseDown (const juce::MouseEvent& e)
{
    for (int pass = 0; pass < 2; ++pass)
        for (int i = 0; i < 12; ++i)
        {
            const bool black = (i == 1 || i == 3 || i == 6 || i == 8 || i == 10);
            if ((pass == 0) != black) continue;     // primero las negras (estan encima)
            if (keyRect (i).contains (e.position))
            {
                const auto id = prefix + juce::String (i);
                setParam (state, id, state.getRawParameterValue (id)->load() > 0.5f ? 0.0f : 1.0f);
                if (onUserChange) onUserChange();
                repaint();
                return;
            }
        }
}

//==============================================================================
void PitchRoll::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff10131c)); g.fillRoundedRectangle (r, 10);
    const int N = TopVoiceMidiProcessor::histSize;
    const int pos = proc.histPos.load();
    // rango vertical automatico
    float mn = 200, mx = -1;
    for (int i = 0; i < N; ++i)
    {
        const float v = proc.pitchHist[(size_t) i].load(); if (v > 0) { mn = juce::jmin (mn, v); mx = juce::jmax (mx, v); }
        const float nn = proc.noteHist[(size_t) i].load(); if (nn >= 0) { mn = juce::jmin (mn, nn); mx = juce::jmax (mx, nn); }
    }
    if (mx > 0) { lo += (juce::jmin (mn - 4, (mn + mx) * 0.5f - 12) - lo) * 0.15f; hi += (juce::jmax (mx + 4, (mn + mx) * 0.5f + 12) - hi) * 0.15f; }
    const auto area = r.reduced (0, 8).withTrimmedLeft (44);
    auto yOf = [&] (float m) { return area.getBottom() - (m - lo) / (hi - lo) * area.getHeight(); };
    g.saveState(); g.reduceClipRegion (r.toNearestInt());
    for (int n = (int) std::floor (lo); n <= (int) std::ceil (hi); ++n)
    {
        const bool black = (n % 12 == 1 || n % 12 == 3 || n % 12 == 6 || n % 12 == 8 || n % 12 == 10);
        const float y = yOf ((float) n + 0.5f), y2 = yOf ((float) n - 0.5f);
        g.setColour (black ? juce::Colour (0xff0d1017) : juce::Colour (0xff141824)); g.fillRect (area.getX(), y, area.getWidth(), y2 - y);
        g.setColour (line.withAlpha (0.5f)); g.drawHorizontalLine ((int) y2, area.getX(), area.getRight());
        if (n % 12 == 0 || (hi - lo) < 30)
        {
            g.setColour (n % 12 == 0 ? text : dim); g.setFont (font (11.0f, n % 12 == 0));
            g.drawText (noteName (n), 4, (int) y, 38, (int) (y2 - y), juce::Justification::centredLeft);
        }
    }
    const float dx = area.getWidth() / (float) (N - 1);
    // bloques de notas MIDI
    int runStart = -1; float runNote = -1;
    for (int k = 0; k <= N; ++k)
    {
        const float nn = k < N ? proc.noteHist[(size_t) ((pos + k) % N)].load() : -1.0f;
        if (nn != runNote)
        {
            if (runNote >= 0 && runStart >= 0)
            {
                const auto br = juce::Rectangle<float> (area.getX() + runStart * dx, yOf (runNote + 0.45f), (k - runStart) * dx, yOf (runNote - 0.45f) - yOf (runNote + 0.45f));
                g.setColour (teal.withAlpha (0.35f)); g.fillRoundedRectangle (br, 4);
                g.setColour (teal); g.drawRoundedRectangle (br, 4, 1.4f);
            }
            runNote = nn; runStart = k;
        }
    }
    // linea de la voz
    juce::Path p; bool pen = false;
    for (int k = 0; k < N; ++k)
    {
        const float v = proc.pitchHist[(size_t) ((pos + k) % N)].load() + 12.0f * proc.apvts.getRawParameterValue ("octave")->load();
        if (proc.pitchHist[(size_t) ((pos + k) % N)].load() <= 0) { pen = false; continue; }
        const juce::Point<float> pt (area.getX() + k * dx, yOf (v));
        if (! pen) { p.startNewSubPath (pt); pen = true; } else p.lineTo (pt);
    }
    g.setColour (pink.withAlpha (0.3f)); g.strokePath (p, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (pink); g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.restoreState();
    g.setColour (line); g.drawRoundedRectangle (r.reduced (0.5f), 10, 1.0f);
    g.setColour (dim); g.setFont (font (11.0f));
    g.drawText (U ("rosa = tu voz   ·   verde = notas MIDI"), r.reduced (10, 6), juce::Justification::topRight);
}

//==============================================================================
void DragTile::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1);
    const bool has = file.existsAsFile();
    g.setColour (has ? teal.withAlpha (0.12f) : card2); g.fillRoundedRectangle (r, 10);
    juce::Path dash; dash.addRoundedRectangle (r, 10);
    juce::Path dd; const float pat[] { 6, 5 };
    juce::PathStrokeType (1.5f).createDashedStroke (dd, dash, pat, 2);
    g.setColour (has ? teal : line); g.fillPath (dd);
    g.setColour (has ? text : dim); g.setFont (font (15.0f, true));
    g.drawText (has ? file.getFileName() : U ("Aquí aparece tu .mid exportado"), r.withTrimmedBottom (r.getHeight() * 0.45f), juce::Justification::centredBottom);
    g.setFont (font (12.5f));
    g.setColour (dim);
    g.drawText (has ? U ("Arrástralo a FL Studio (piano roll o playlist)") : U ("Graba, luego pulsa EXPORTAR"), r.withTrimmedTop (r.getHeight() * 0.58f), juce::Justification::centredTop);
}
void DragTile::mouseDrag (const juce::MouseEvent& e)
{
    if (file.existsAsFile() && e.getDistanceFromDragStart() > 6)
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this);
}

//==============================================================================
Pad::Pad (TopVoiceMidiProcessor& p, int i, juce::Colour c) : proc (p), index (i), colour (c)
{
    train.setButtonText ("ENTRENAR");
    train.setColour (juce::TextButton::buttonOnColourId, colour);
    train.setTooltip (U ("Pulsa y haz este sonido 5-10 veces. Pulsa otra vez para terminar."));
    train.onClick = [this] { proc.trainingPad.store (proc.trainingPad.load() == index ? -1 : index); };
    clear.setColour (juce::TextButton::buttonOnColourId, colour);
    clear.onClick = [this] { proc.clearPad (index); repaint(); };
    note.setSliderStyle (juce::Slider::LinearBar);
    note.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    note.setColour (juce::Slider::trackColourId, colour);
    noteAtt = std::make_unique<APVTS::SliderAttachment> (proc.apvts, "trig_note" + juce::String (index), note);
    note.setTooltip (U ("Nota MIDI que dispara este sonido (arrastra para cambiar)"));
    for (auto* c2 : { (juce::Component*) &train, (juce::Component*) &clear, (juce::Component*) &note }) addAndMakeVisible (c2);
}
void Pad::resized()
{
    auto r = getLocalBounds().reduced (12);
    note.setBounds (r.removeFromBottom (28));
    r.removeFromBottom (8);
    clear.setBounds (r.removeFromBottom (28));
    r.removeFromBottom (6);
    train.setBounds (r.removeFromBottom (34));
}
void Pad::tick()
{
    const bool tr = proc.trainingPad.load() == index;
    train.setToggleState (tr, juce::dontSendNotification);
    train.setButtonText (tr ? "LISTO" : "ENTRENAR");
    flash *= 0.85f;
    repaint();
}
void Pad::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const bool tr = proc.trainingPad.load() == index;
    g.setColour (card); g.fillRoundedRectangle (r, 12);
    g.setColour (tr ? colour : line); g.drawRoundedRectangle (r.reduced (0.5f), 12, tr ? 2.0f : 1.0f);
    g.setColour (text); g.setFont (font (16.0f, true));
    g.drawText (TopVoiceMidiProcessor::padNames()[index].toUpperCase(), r.withHeight (40), juce::Justification::centred);
    const juce::Point<float> c (r.getCentreX(), r.getY() + 104);
    const float rad = 50.0f;
    g.setColour (colour.withAlpha (0.10f + 0.5f * flash)); g.fillEllipse (c.x - rad - 8 * flash, c.y - rad - 8 * flash, (rad + 8 * flash) * 2, (rad + 8 * flash) * 2);
    g.setColour (colour.withAlpha (0.5f + 0.5f * flash)); g.drawEllipse (c.x - rad, c.y - rad, rad * 2, rad * 2, 2.5f);
    g.setColour (colour); g.setFont (font (30.0f, true));
    static const char* sym[4] { "B", "K", "H", "X" };
    g.drawText (sym[index], juce::Rectangle<float> (rad * 2, rad * 2).withCentre (c), juce::Justification::centred);
    const int ex = proc.padExamples (index);
    g.setColour (tr ? colour : ex > 0 ? text : dim); g.setFont (font (12.5f, tr || ex > 0));
    const juce::String info = tr ? U ("¡haz el sonido! (") + juce::String (ex) + ")" : ex > 0 ? juce::String (ex) + U (" ejemplos") : U ("sin entrenar");
    g.drawText (info, r.withY (r.getY() + 160).withHeight (20), juce::Justification::centred);
}

//==============================================================================
TopVoiceMidiEditor::TopVoiceMidiEditor (TopVoiceMidiProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      gain (p.apvts, "in_gain", "Ganancia", teal), gate (p.apvts, "gate", "Puerta", amber),
      synthVol (p.apvts, "synth_vol", "Vol. sinte", violet), dry (p.apvts, "dry", "Voz", blue),
      roll (p),
      octave (p.apvts, "octave", "Octava", teal), response (p.apvts, "response", "Respuesta", teal),
      stability (p.apvts, "stability", "Estabilidad", teal), velSens (p.apvts, "vel_sens", "Velocidad", pink),
      channel (p.apvts, "channel", "Canal MIDI", violet),
      trigSens (p.apvts, "trig_sens", "Sensibilidad", amber), trigCh (p.apvts, "trig_ch", "Canal MIDI", violet),
      scaleKeys (p.apvts, "nm", { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, teal),
      chordKeys (p.apvts, "cn", { "R", "b2", "2", "b3", "3", "4", "b5", "5", "b6", "6", "b7", "7" }, pink),
      inversion (p.apvts, "inversion", U ("Inversión"), pink),
      bendRange (p.apvts, "bend_range", "Rango (semitonos)", violet), bendThr (p.apvts, "bend_thr", "Zona muerta", violet)
{
    setLookAndFeel (&look);
    tips.setLookAndFeel (&look);
    tabComps.resize (5);

    // ---- cabecera ----
    const juce::StringArray modes { U ("MELODÍA"), "RITMO", "AMBOS" };
    for (int i = 0; i < 3; ++i)
    {
        auto* b = modeBtns.add (new juce::TextButton (modes[i]));
        b->setColour (juce::TextButton::buttonOnColourId, i == 0 ? teal : i == 1 ? amber : pink);
        b->onClick = [this, i] { setParam (proc.apvts, "mode", (float) i); };
        addAndMakeVisible (b);
    }
    modeBtns[0]->setTooltip (U ("Melodía: canta o tararea y salen notas"));
    modeBtns[1]->setTooltip (U ("Ritmo: haz beatbox y salen golpes de batería"));
    modeBtns[2]->setTooltip (U ("Ambos: notas con la voz cantada y batería con los sonidos de beatbox"));
    const juce::StringArray tabs { U ("MELODÍA"), "RITMO", U ("ARMONÍA"), U ("EXPRESIÓN"), "CAPTURA" };
    for (int i = 0; i < 5; ++i)
    {
        auto* b = tabBtns.add (new juce::TextButton (tabs[i]));
        b->setColour (juce::TextButton::buttonOnColourId, teal);
        b->onClick = [this, i] { showTab (i); };
        addAndMakeVisible (b);
    }
    addAndMakeVisible (deviceBox);
    deviceBox.setTooltip (U ("Salida MIDI externa (por ejemplo loopMIDI). Además el plugin siempre manda MIDI por su propia salida."));
    deviceBox.onChange = [this]
    {
        const int id = deviceBox.getSelectedId();
        proc.setExternalDevice (id >= 2 && id - 2 < devices.size() ? devices[id - 2].identifier : juce::String());
    };
    addAndMakeVisible (refreshBtn);
    refreshBtn.setColour (juce::TextButton::buttonOnColourId, teal);
    refreshBtn.setTooltip ("Actualizar la lista de dispositivos MIDI");
    refreshBtn.onClick = [this] { refreshDevices(); };
    refreshDevices();

    // ---- monitor ----
    for (auto* c : { (juce::Component*) &gain, (juce::Component*) &gate, (juce::Component*) &synthVol, (juce::Component*) &dry,
                     (juce::Component*) &synthOn, (juce::Component*) &waveBox })
        addAndMakeVisible (c);
    gain.slider.setTooltip ("Sube si tu micro llega bajito");
    gate.slider.setTooltip (U ("Por debajo de este volumen el plugin no escucha (quita ruido de fondo). La raya amarilla en el medidor."));
    synthVol.slider.setTooltip (U ("Volumen del sinte de prueba para oír las notas sin cargar un instrumento"));
    dry.slider.setTooltip (U ("Cuánto de tu voz se oye a la salida"));
    synthOn.setColour (juce::ToggleButton::tickColourId, violet);
    synthOnAtt = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "synth_on", synthOn);
    waveBox.addItemList ({ "Seno", "Sierra", "Cuadrada" }, 1);
    waveAtt = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, "synth_wave", waveBox);

    // ---- melodia ----
    for (auto* c : { (juce::Component*) &roll, (juce::Component*) &octave, (juce::Component*) &response, (juce::Component*) &stability,
                     (juce::Component*) &velSens, (juce::Component*) &channel })
    { addChildComponent (c); tabComps[0].push_back (c); }
    octave.slider.setTooltip ("Sube o baja las notas por octavas (canta grave y toca agudo)");
    response.slider.setTooltip (U ("Cuánto espera antes de decidir la nota. Menos = más rápido, más = menos notas falsas"));
    stability.slider.setTooltip (U ("Cuánto tienes que moverte de una nota para cambiar a otra. Más = menos saltos"));
    velSens.slider.setTooltip (U ("0 = todas las notas igual de fuertes. 100 = la velocidad sigue tu volumen"));

    // ---- ritmo ----
    const juce::Colour pc[4] { amber, pink, teal, violet };
    for (int i = 0; i < 4; ++i) { auto* pd = pads.add (new Pad (proc, i, pc[i])); addChildComponent (pd); tabComps[1].push_back (pd); }
    for (auto* c : { (juce::Component*) &trigSens, (juce::Component*) &trigCh }) { addChildComponent (c); tabComps[1].push_back (c); }
    trigSens.slider.setTooltip (U ("Más = detecta golpes más suaves"));

    // ---- armonia ----
    keyBox.addItemList ({ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 1);
    scaleBox.addItemList (TopVoiceMidiProcessor::scaleNames(), 1);
    chordBox.addItemList (TopVoiceMidiProcessor::chordNames(), 1);
    voicingBox.addItemList ({ "Cerrado", "Abierto", "Amplio" }, 1);
    keyAtt = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, "key", keyBox);
    scaleAtt = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, "scale", scaleBox);
    chordAtt = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, "chord_type", chordBox);
    voicingAtt = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, "voicing", voicingBox);
    autoKey.setColour (juce::ToggleButton::tickColourId, teal);
    chordOn.setColour (juce::ToggleButton::tickColourId, pink);
    autoAtt = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "auto_key", autoKey);
    chordOnAtt = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "chord_on", chordOn);
    autoKey.setTooltip (U ("Canta unos segundos y el plugin descubre en qué tonalidad estás"));
    scaleKeys.onUserChange = [this] { setParam (proc.apvts, "scale", 7.0f); setParam (proc.apvts, "auto_key", 0.0f); };
    chordKeys.onUserChange = [this] { setParam (proc.apvts, "chord_type", 10.0f); };
    learnBtn.setColour (juce::TextButton::buttonOnColourId, teal);
    learnBtn.setTooltip (U ("Pulsa, canta las notas que quieras permitir y pulsa otra vez"));
    learnBtn.onClick = [this]
    {
        if (! proc.learning.load()) { proc.learnedMask.store (0); proc.learning.store (true); }
        else
        {
            proc.learning.store (false);
            const int m = proc.learnedMask.load();
            if (m != 0)
            {
                for (int i = 0; i < 12; ++i) setParam (proc.apvts, "nm" + juce::String (i), (m >> i) & 1 ? 1.0f : 0.0f);
                setParam (proc.apvts, "scale", 7.0f); setParam (proc.apvts, "auto_key", 0.0f);
            }
        }
    };
    for (auto* c : { (juce::Component*) &keyBox, (juce::Component*) &scaleBox, (juce::Component*) &autoKey, (juce::Component*) &scaleKeys,
                     (juce::Component*) &learnBtn, (juce::Component*) &chordOn, (juce::Component*) &chordBox, (juce::Component*) &voicingBox,
                     (juce::Component*) &inversion, (juce::Component*) &chordKeys })
    { addChildComponent (c); tabComps[2].push_back (c); }

    // ---- expresion ----
    bendBox.addItemList ({ "Apagado", U ("Inteligente (te sostiene en la nota)"), U ("Exacto (sigue cada movimiento)") }, 1);
    bendAtt = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, "bend_mode", bendBox);
    bendThr.slider.setTooltip (U ("Modo inteligente: cuánto te puedes desafinar sin que se mueva el bend"));
    for (auto* c : { (juce::Component*) &bendBox, (juce::Component*) &bendRange, (juce::Component*) &bendThr })
    { addChildComponent (c); tabComps[3].push_back (c); }
    const juce::Colour cc[4] { teal, amber, pink, violet };
    for (int i = 0; i < 4; ++i)
    {
        const auto n = juce::String (i + 1);
        auto* t = ccOn.add (new juce::ToggleButton ("CC " + n));
        t->setColour (juce::ToggleButton::tickColourId, cc[i]);
        ccOnAtt.push_back (std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "cc" + n + "_on", *t));
        auto* s = ccSrc.add (new juce::ComboBox());
        s->addItemList (TopVoiceMidiProcessor::ccSourceNames(), 1);
        ccSrcAtt.push_back (std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, "cc" + n + "_src", *s));
        auto* k1 = ccNum.add (new Knob (proc.apvts, "cc" + n + "_num", U ("NÚMERO CC"), cc[i]));
        auto* k2 = ccSmooth.add (new Knob (proc.apvts, "cc" + n + "_smooth", "Suavizado", cc[i]));
        for (auto* c : { (juce::Component*) t, (juce::Component*) s, (juce::Component*) k1, (juce::Component*) k2 })
        { addChildComponent (c); tabComps[3].push_back (c); }
    }

    // ---- captura ----
    recBtn.setColour (juce::TextButton::buttonOnColourId, pink);
    recBtn.onClick = [this]
    {
        if (proc.isCapturing()) proc.stopCapture();
        else { proc.startCapture(); recStart = juce::Time::getMillisecondCounterHiRes(); }
    };
    clearBtn.setColour (juce::TextButton::buttonOnColourId, pink);
    clearBtn.onClick = [this] { proc.clearCapture(); tile.file = juce::File(); tile.repaint(); };
    exportBtn.setColour (juce::TextButton::buttonOnColourId, teal);
    exportBtn.onClick = [this]
    {
        if (proc.isCapturing()) proc.stopCapture();
        const auto f = proc.defaultExportFile();
        if (proc.exportCapture (f, ghosts.getToggleState())) tile.file = f;
        tile.repaint();
    };
    folderBtn.setColour (juce::TextButton::buttonOnColourId, teal);
    folderBtn.onClick = [this] { proc.defaultExportFile().getParentDirectory().startAsProcess(); };
    ghosts.setToggleState (true, juce::dontSendNotification);
    ghosts.setColour (juce::ToggleButton::tickColourId, teal);
    ghosts.setTooltip (U ("Quita notas cortísimas que salen por error y compensa el retraso de detección"));
    for (auto* c : { (juce::Component*) &recBtn, (juce::Component*) &clearBtn, (juce::Component*) &exportBtn, (juce::Component*) &folderBtn,
                     (juce::Component*) &ghosts, (juce::Component*) &tile })
    { addChildComponent (c); tabComps[4].push_back (c); }

    setSize (W, H);
    showTab (0);
    startTimerHz (30);
}

TopVoiceMidiEditor::~TopVoiceMidiEditor()
{
    stopTimer();
    proc.trainingPad.store (-1);
    proc.learning.store (false);
    tips.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void TopVoiceMidiEditor::refreshDevices()
{
    const auto names = proc.externalDevices (devices);
    deviceBox.clear (juce::dontSendNotification);
    deviceBox.addItem (U ("Salida MIDI: solo la del plugin"), 1);
    for (int i = 0; i < names.size(); ++i) deviceBox.addItem ("Salida MIDI: " + names[i], i + 2);
    int sel = 1;
    for (int i = 0; i < devices.size(); ++i) if (devices[i].identifier == proc.getExternalDevice()) sel = i + 2;
    deviceBox.setSelectedId (sel, juce::dontSendNotification);
}

void TopVoiceMidiEditor::showTab (int t)
{
    tab = t;
    for (int i = 0; i < (int) tabComps.size(); ++i)
        for (auto* c : tabComps[(size_t) i]) c->setVisible (i == t);
    for (int i = 0; i < tabBtns.size(); ++i) tabBtns[i]->setToggleState (i == t, juce::dontSendNotification);
    if (t != 1) proc.trainingPad.store (-1);
    layoutTab();
    repaint();
}

void TopVoiceMidiEditor::resized()
{
    leftR = { 16, 78, 290, H - 94 };
    tabR = { 322, 78, W - 338, 38 };
    contentR = { 322, 126, W - 338, H - 142 };

    for (int i = 0; i < 3; ++i) modeBtns[i]->setBounds (330 + i * 112, 16, 106, 34);
    deviceBox.setBounds (W - 330, 18, 270, 30);
    refreshBtn.setBounds (W - 54, 18, 38, 30);
    const int tw = tabR.getWidth() / 5;
    for (int i = 0; i < 5; ++i) tabBtns[i]->setBounds (tabR.getX() + i * tw, tabR.getY(), tw - 6, tabR.getHeight());

    const int lx = leftR.getX(), ly = leftR.getY();
    gain.setBounds (lx + 22, ly + 300, 116, 116);
    gate.setBounds (lx + 152, ly + 300, 116, 116);
    synthOn.setBounds (lx + 20, ly + 432, 170, 28);
    waveBox.setBounds (lx + 180, ly + 432, 94, 28);
    synthVol.setBounds (lx + 22, ly + 464, 116, 108);
    dry.setBounds (lx + 152, ly + 464, 116, 108);
    layoutTab();
}

void TopVoiceMidiEditor::layoutTab()
{
    const auto c = contentR;
    const int x = c.getX(), y = c.getY(), w = c.getWidth();
    // melodia
    roll.setBounds (x + 16, y + 44, w - 32, 330);
    {
        const int kw = 120, gap = (w - 32 - 5 * kw) / 4;
        int kx = x + 16;
        for (auto* k : { &octave, &response, &stability, &velSens, &channel }) { k->setBounds (kx, y + 396, kw, 120); kx += kw + gap; }
    }
    // ritmo
    {
        const int pw = (w - 32 - 3 * 14) / 4;
        for (int i = 0; i < 4; ++i) pads[i]->setBounds (x + 16 + i * (pw + 14), y + 44, pw, 300);
        trigSens.setBounds (x + 16, y + 414, 120, 120);
        trigCh.setBounds (x + 150, y + 414, 120, 120);
    }
    // armonia
    keyBox.setBounds (x + 16, y + 48, 90, 32);
    scaleBox.setBounds (x + 114, y + 48, 230, 32);
    autoKey.setBounds (x + 360, y + 48, 230, 32);
    scaleKeys.setBounds (x + 16, y + 96, 520, 110);
    learnBtn.setBounds (x + 556, y + 104, w - 572, 46);
    chordOn.setBounds (x + 16, y + 270, 140, 32);
    chordBox.setBounds (x + 160, y + 270, 200, 32);
    voicingBox.setBounds (x + 370, y + 270, 140, 32);
    inversion.setBounds (x + w - 140, y + 250, 120, 120);
    chordKeys.setBounds (x + 16, y + 318, 520, 100);
    // expresion
    bendBox.setBounds (x + 16, y + 48, 330, 32);
    bendRange.setBounds (x + 370, y + 30, 120, 120);
    bendThr.setBounds (x + 500, y + 30, 120, 120);
    {
        const int cw = (w - 32 - 3 * 14) / 4;
        for (int i = 0; i < 4; ++i)
        {
            const int cx = x + 16 + i * (cw + 14);
            ccOn[i]->setBounds (cx + 12, y + 226, cw - 24, 28);
            ccSrc[i]->setBounds (cx + 12, y + 262, cw - 24, 30);
            ccNum[i]->setBounds (cx + 6, y + 302, cw / 2 - 6, 110);
            ccSmooth[i]->setBounds (cx + cw / 2, y + 302, cw / 2 - 6, 110);
        }
    }
    // captura
    recBtn.setBounds (x + 16, y + 48, 200, 56);
    clearBtn.setBounds (x + 228, y + 48, 120, 56);
    ghosts.setBounds (x + 16, y + 150, 300, 28);
    exportBtn.setBounds (x + 16, y + 194, 200, 44);
    folderBtn.setBounds (x + 228, y + 194, 140, 44);
    tile.setBounds (x + 390, y + 150, w - 406, 88);
}

void TopVoiceMidiEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);
    // cabecera
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff141826), 0, 0, bg, 0, 64, false));
    g.fillRect (0, 0, W, 64);
    g.setColour (line); g.drawHorizontalLine (64, 0, (float) W);
    // logo: onda que se convierte en notas
    {
        juce::Path wave;
        for (int i = 0; i <= 40; ++i)
        {
            const float t = (float) i / 40.0f;
            const juce::Point<float> p (18 + t * 34, 33 + std::sin (t * 12.0f) * 9.0f * (1.0f - t));
            if (i == 0) wave.startNewSubPath (p); else wave.lineTo (p);
        }
        g.setColour (pink); g.strokePath (wave, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (teal);
        g.fillRoundedRectangle (54, 22, 10, 6, 2); g.fillRoundedRectangle (60, 32, 12, 6, 2); g.fillRoundedRectangle (66, 42, 8, 6, 2);
    }
    g.setColour (text); g.setFont (font (22.0f, true));
    g.drawText ("TOP VOICE MIDI", 84, 10, 240, 28, juce::Justification::centredLeft);
    g.setColour (dim); g.setFont (font (12.0f));
    g.drawText ("by Dj Pava Hm Music", 85, 36, 240, 16, juce::Justification::centredLeft);

    // ---- monitor ----
    cardBox (g, leftR.toFloat(), "MONITOR", pink);
    const int lx = leftR.getX(), ly = leftR.getY(), lw = leftR.getWidth();
    const int note = proc.liveNote.load();
    const float live = proc.liveMidi.load();
    g.setColour (note >= 0 ? text : dim.withAlpha (0.6f));
    g.setFont (font (78.0f, true));
    g.drawText (note >= 0 ? noteName (note) : (live > 0 ? noteName ((int) std::round (live + 12 * proc.apvts.getRawParameterValue ("octave")->load())) : "--"),
                lx, ly + 44, lw, 96, juce::Justification::centred);
    // barra de cents (que tan afinado estas respecto a la nota)
    {
        const auto br = juce::Rectangle<float> ((float) lx + 30, (float) ly + 152, (float) lw - 60, 10);
        g.setColour (card2); g.fillRoundedRectangle (br, 5);
        g.setColour (dim); g.fillRect (br.getCentreX() - 1, br.getY() - 4, 2.0f, br.getHeight() + 8);
        if (live > 0)
        {
            const float cents = (live - std::round (live)) * 100.0f;
            const float xx = br.getCentreX() + cents / 50.0f * br.getWidth() * 0.5f;
            g.setColour (std::abs (cents) < 15 ? teal : amber);
            g.fillEllipse (xx - 8, br.getCentreY() - 8, 16, 16);
            g.setFont (font (12.0f));
            g.drawText ((cents >= 0 ? "+" : "") + juce::String ((int) std::round (cents)) + " cents", br.translated (0, 14).withHeight (16), juce::Justification::centred);
        }
    }
    // tonalidad
    {
        static const char* nm[12] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const bool aut = proc.apvts.getRawParameterValue ("auto_key")->load() > 0.5f;
        const int sc = proc.effectiveScale();
        juce::String k = sc == 7 ? U ("Escala personalizada") : sc == 0 ? U ("Cromática (todas las notas)")
                       : juce::String (nm[proc.effectiveKey()]) + " " + TopVoiceMidiProcessor::scaleNames()[sc].toLowerCase();
        if (aut) k << (proc.detConf.load() > 0.3f ? "  (auto)" : U ("  (auto: canta más…)"));
        g.setColour (teal); g.setFont (font (14.0f, true));
        g.drawText (k, lx, ly + 190, lw, 20, juce::Justification::centred);
    }
    // medidor de entrada con la puerta
    {
        const auto mr = juce::Rectangle<float> ((float) lx + 30, (float) ly + 236, (float) lw - 60, 14);
        g.setColour (card2); g.fillRoundedRectangle (mr, 7);
        auto toX = [&] (float db) { return mr.getX() + juce::jlimit (0.0f, 1.0f, (db + 72.0f) / 72.0f) * mr.getWidth(); };
        const float lvl = proc.inDb.load();
        g.setGradientFill (juce::ColourGradient (teal, mr.getX(), 0, pink, mr.getRight(), 0, false));
        g.fillRoundedRectangle (mr.withRight (juce::jmax (mr.getX() + 4, toX (lvl))), 7);
        g.setColour (amber); g.fillRect (toX (proc.apvts.getRawParameterValue ("gate")->load()) - 1.5f, mr.getY() - 4, 3.0f, mr.getHeight() + 8);
        g.setColour (dim); g.setFont (font (11.5f));
        g.drawText (U ("ENTRADA  (raya amarilla = puerta)"), mr.translated (0, 18).withHeight (14), juce::Justification::centred);
    }
    g.setColour (line); g.drawHorizontalLine (ly + 424, (float) lx + 16, (float) leftR.getRight() - 16);
    g.setColour (dim); g.setFont (font (11.5f));
    g.drawText (U ("Latencia de detección ≈ ") + juce::String ((int) std::round (proc.detectionLatencyMs())) + " ms",
                lx, leftR.getBottom() - 22, lw, 16, juce::Justification::centred);

    // ---- contenido ----
    cardBox (g, contentR.toFloat());
    const int x = contentR.getX(), y = contentR.getY(), w = contentR.getWidth();
    g.setColour (text); g.setFont (font (15.0f, true));
    auto heading = [&] (const juce::String& t, int yy, juce::Colour a)
    {
        g.setColour (a); g.fillRoundedRectangle ((float) x + 16, (float) yy + 5, 4, 14, 2);
        g.setColour (text); g.setFont (font (14.0f, true));
        g.drawText (t, x + 28, yy, w - 44, 24, juce::Justification::centredLeft);
    };
    auto note2 = [&] (const juce::String& t, int yy, int hgt = 40)
    {
        g.setColour (dim); g.setFont (font (13.0f));
        g.drawFittedText (t, x + 20, yy, w - 40, hgt, juce::Justification::topLeft, 3);
    };
    if (tab == 0)
    {
        heading (U ("MELODÍA  ·  canta o tararea y salen notas MIDI"), y + 12, teal);
    }
    else if (tab == 1)
    {
        heading (U ("RITMO  ·  beatbox → batería"), y + 12, amber);
        note2 (U ("1) Pulsa ENTRENAR en un pad y haz ese sonido 5-10 veces (bum, tss, ka…). 2) Pulsa LISTO. 3) Pon el modo RITMO "
                  "y toca. Sin entrenar también funciona: graves = bombo, medios = caja, agudos = hi-hat."), y + 360, 60);
        g.setColour (dim);
        g.drawFittedText (U ("Cada golpe sale con velocidad según qué tan fuerte lo hagas."), x + 300, y + 450, w - 320, 40, juce::Justification::topLeft, 2);
    }
    else if (tab == 2)
    {
        heading (U ("TONALIDAD Y ESCALA  ·  solo salen notas de la escala"), y + 12, teal);
        note2 (U ("Toca las teclas para elegir tus notas, o usa APRENDER: canta las notas que quieras y el plugin arma la escala."), y + 214, 40);
        g.setColour (proc.learning.load() ? teal : dim); g.setFont (font (12.5f, true));
        g.drawFittedText (proc.learning.load() ? U ("Escuchando… canta tus notas y pulsa otra vez") : U ("La tecla con borde blanco es la que suena"),
                          learnBtn.getX(), learnBtn.getBottom() + 8, learnBtn.getWidth(), 36, juce::Justification::centredTop, 2);
        heading (U ("ACORDES  ·  una nota cantada se vuelve un acorde"), y + 238, pink);
        note2 (U ("Diatónico = el acorde correcto de la escala para cada nota. Personalizado = toca las teclas (R = la nota que cantas)."), y + 430, 40);
    }
    else if (tab == 3)
    {
        heading (U ("PITCH BEND  ·  tus deslices y vibratos"), y + 12, violet);
        // medidor de bend
        const auto br = juce::Rectangle<float> ((float) x + 640, (float) y + 60, (float) w - 660, 16);
        g.setColour (card2); g.fillRoundedRectangle (br, 8);
        const float range = proc.apvts.getRawParameterValue ("bend_range")->load();
        const float b = proc.bendSemis.load() / juce::jmax (1.0f, range);
        g.setColour (violet);
        const float cx = br.getCentreX(), bx = cx + b * br.getWidth() * 0.5f;
        g.fillRoundedRectangle (juce::jmin (cx, bx), br.getY(), std::abs (bx - cx) + 2, br.getHeight(), 6);
        g.setColour (dim); g.fillRect (cx - 1, br.getY() - 4, 2.0f, br.getHeight() + 8);
        g.setFont (font (11.5f)); g.drawText ("BEND EN VIVO", br.translated (0, 20).withHeight (14), juce::Justification::centred);
        g.setColour (dim); g.setFont (font (12.5f));
        g.drawFittedText (U ("Inteligente: te deja en la nota aunque desafines un poco y solo dobla cuando te mueves a propósito. "
                             "Exacto: sigue todo lo que haces. Pon el mismo rango de bend en tu instrumento."),
                          x + 20, y + 92, 330, 80, juce::Justification::topLeft, 5);
        heading (U ("CONTROL CC  ·  tu voz mueve perillas de tus plugins"), y + 192, amber);
        const int cw = (w - 32 - 3 * 14) / 4;
        for (int i = 0; i < 4; ++i)
        {
            const int cx2 = x + 16 + i * (cw + 14);
            g.setColour (card2.withAlpha (0.6f)); g.fillRoundedRectangle ((float) cx2, (float) y + 218, (float) cw, 260, 10);
            const float v = proc.ccLive[(size_t) i].load();
            const auto bar = juce::Rectangle<float> ((float) cx2 + 12, (float) y + 420, (float) cw - 24, 12);
            g.setColour (line); g.fillRoundedRectangle (bar, 6);
            g.setColour (i == 0 ? teal : i == 1 ? amber : i == 2 ? pink : violet);
            g.fillRoundedRectangle (bar.withWidth (juce::jmax (6.0f, bar.getWidth() * v)), 6);
            g.setColour (dim); g.setFont (font (11.5f));
            g.drawText (juce::String ((int) std::round (v * 127)), bar.translated (0, 16).withHeight (16), juce::Justification::centred);
        }
        note2 (U ("En tu plugin usa MIDI Learn y mueve la voz para asignarlo."), y + 490, 30);
    }
    else
    {
        heading (U ("CAPTURA  ·  graba el MIDI aquí y arrástralo limpio a tu proyecto"), y + 12, pink);
        g.setColour (proc.isCapturing() ? pink : dim); g.setFont (font (15.0f, true));
        const double secs = proc.isCapturing() ? (juce::Time::getMillisecondCounterHiRes() - recStart) / 1000.0 : 0.0;
        g.drawText ((proc.isCapturing() ? U ("● GRABANDO  ") + juce::String (secs, 1) + " s   ·   " : juce::String())
                        + juce::String (capturedCount) + " notas", x + 16, y + 112, w - 32, 24, juce::Justification::centredLeft);
        heading (U ("CÓMO TOCAR UN INSTRUMENTO DE FL STUDIO EN VIVO"), y + 262, teal);
        note2 (U ("Opción A (FL Studio): abre el engrane del plugin → pestaña de ajustes del wrapper → MIDI → 'Output port' = 1. "
                  "En el instrumento que quieras tocar pon 'Input port' = 1. Arma el canal y graba.\n"
                  "Opción B (cualquier DAW): instala loopMIDI (gratis), crea un puerto, elígelo arriba en 'Salida MIDI' y actívalo como entrada en tu DAW.\n"
                  "Opción C: usa CAPTURA y arrastra el .mid al piano roll.\n"
                  "Consejo: usa audífonos para que el sinte no se cuele en el micrófono."), y + 292, 180);
    }
}

void TopVoiceMidiEditor::timerCallback()
{
    const int mode = (int) proc.apvts.getRawParameterValue ("mode")->load();
    for (int i = 0; i < 3; ++i) modeBtns[i]->setToggleState (i == mode, juce::dontSendNotification);
    learnBtn.setToggleState (proc.learning.load(), juce::dontSendNotification);
    learnBtn.setButtonText (proc.learning.load() ? U ("TERMINAR") : U ("APRENDER CON MI VOZ"));
    recBtn.setToggleState (proc.isCapturing(), juce::dontSendNotification);
    recBtn.setButtonText (proc.isCapturing() ? "PARAR" : "GRABAR");

    const int hs = proc.hitSerial.load();
    if (hs != lastHitSerial) { lastHitSerial = hs; const int pd = proc.lastPad.load(); if (pd >= 0 && pd < pads.size()) pads[pd]->flash = 1.0f; }
    if (tab == 1) for (auto* p : pads) p->tick();
    if (tab == 0) roll.repaint();
    const int ln = proc.liveNote.load();
    const bool custom = (int) proc.apvts.getRawParameterValue ("scale")->load() == 7 && proc.apvts.getRawParameterValue ("auto_key")->load() < 0.5f;
    scaleKeys.highlight = ln >= 0 ? ln % 12 : -1;
    chordKeys.highlight = -1;
    if (tab == 2) { scaleKeys.repaint(); chordKeys.repaint(); }
    scaleKeys.setAlpha (custom ? 1.0f : 0.55f);
    chordKeys.setAlpha ((int) proc.apvts.getRawParameterValue ("chord_type")->load() == 10 ? 1.0f : 0.55f);
    if (tab == 4) capturedCount = proc.capturedNotes();
    repaint (leftR);
    repaint (contentR.withTrimmedTop (0).withHeight (tab == 3 ? 520 : 140));
}
