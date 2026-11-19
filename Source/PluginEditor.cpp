#include "PluginEditor.h"

using namespace tvcui;

static juce::Font uiFont (float h, bool bold = false)
{
    return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
}

//==============================================================================
// LOOK AND FEEL
//==============================================================================
LookAndFeel::LookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, Colours::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, Colours::dim);
    setColour (juce::ComboBox::backgroundColourId, Colours::panelHi);
    setColour (juce::ComboBox::outlineColourId, Colours::dim.withAlpha (0.3f));
    setColour (juce::ComboBox::textColourId, Colours::text);
    setColour (juce::ComboBox::arrowColourId, Colours::cyan);
    setColour (juce::PopupMenu::backgroundColourId, Colours::panel);
    setColour (juce::PopupMenu::textColourId, Colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colours::cyan.withAlpha (0.3f));
    setColour (juce::TextButton::textColourOffId, Colours::dim);
    setColour (juce::TextButton::textColourOnId, Colours::bg);
    setColour (juce::TextButton::buttonColourId, Colours::panelHi);
    setColour (juce::TextButton::buttonOnColourId, Colours::cyan);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                    float startAngle, float endAngle, juce::Slider& s)
{
    const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float cx = bounds.getCentreX(), cy = bounds.getCentreY();
    const float arcR = radius - 3.0f;
    const float lineW = juce::jmax (2.5f, radius * 0.09f);
    const float angle = startAngle + pos * (endAngle - startAngle);

    // Pista
    juce::Path track;
    track.addCentredArc (cx, cy, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (Colours::bg.brighter (0.15f));
    g.strokePath (track, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Arco de valor (bipolar si el rango cruza el cero)
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float from = bipolar ? startAngle + (float) s.valueToProportionOfLength (0.0) * (endAngle - startAngle) : startAngle;
    juce::Path val;
    val.addCentredArc (cx, cy, arcR, arcR, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
    g.setColour (accent);
    g.strokePath (val, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Cuerpo
    const float kr = radius * 0.70f;
    juce::ColourGradient grad (Colours::panelHi.brighter (0.25f), cx, cy - kr, Colours::bg, cx, cy + kr, false);
    g.setGradientFill (grad);
    g.fillEllipse (cx - kr, cy - kr, kr * 2.0f, kr * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawEllipse (cx - kr, cy - kr, kr * 2.0f, kr * 2.0f, 1.2f);

    // Indicador
    juce::Path ptr;
    ptr.addRoundedRectangle (-lineW * 0.5f, -kr + 3.0f, lineW, kr * 0.45f, lineW * 0.5f);
    g.setColour (accent.brighter (0.3f));
    g.fillPath (ptr, juce::AffineTransform::rotation (angle).translated (cx, cy));
}

void LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                    juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style != juce::Slider::LinearVertical)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0, 0, style, s);
        return;
    }

    const auto accent = s.findColour (juce::Slider::trackColourId);
    const float cx = (float) x + (float) w * 0.5f;

    // Ranura
    g.setColour (Colours::bg);
    g.fillRoundedRectangle (cx - 3.0f, (float) y, 6.0f, (float) h, 3.0f);
    g.setColour (accent.withAlpha (0.8f));
    g.fillRoundedRectangle (cx - 2.0f, pos, 4.0f, (float) (y + h) - pos, 2.0f);

    // Marcas
    g.setColour (Colours::dim.withAlpha (0.35f));
    for (int i = 0; i <= 10; ++i)
    {
        const float yy = (float) y + (float) h * (float) i / 10.0f;
        g.drawHorizontalLine ((int) yy, cx - 14.0f, cx - 7.0f);
        g.drawHorizontalLine ((int) yy, cx + 7.0f, cx + 14.0f);
    }

    // Tapa del fader
    const juce::Rectangle<float> cap (cx - 17.0f, pos - 10.0f, 34.0f, 20.0f);
    juce::ColourGradient grad (Colours::panelHi.brighter (0.5f), cap.getX(), cap.getY(), Colours::panelHi, cap.getX(), cap.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (cap, 3.0f);
    g.setColour (accent);
    g.fillRect (cap.getX() + 4.0f, cap.getCentreY() - 1.0f, cap.getWidth() - 8.0f, 2.0f);
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const auto onCol = b.findColour (juce::TextButton::buttonOnColourId);
    auto fill = b.getToggleState() ? onCol : Colours::panelHi;
    if (over) fill = fill.brighter (0.1f);
    if (down) fill = fill.darker (0.2f);
    g.setColour (fill);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (b.getToggleState() ? onCol.brighter (0.3f) : Colours::dim.withAlpha (0.3f));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
}

juce::Font LookAndFeel::getTextButtonFont (juce::TextButton&, int h) { return uiFont (juce::jmin (13.0f, (float) h * 0.55f), true); }
juce::Font LookAndFeel::getComboBoxFont (juce::ComboBox&)            { return uiFont (13.0f); }
juce::Font LookAndFeel::getLabelFont (juce::Label& l)                { return uiFont (juce::jmin (13.0f, (float) juce::jmax (8, l.getHeight()) * 0.8f)); }

//==============================================================================
// COMPONENTES
//==============================================================================
Knob::Knob (APVTS& s, const juce::String& id, const juce::String& label, juce::Colour accent)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 16);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    slider.setPopupDisplayEnabled (false, false, nullptr);
    addAndMakeVisible (slider);

    name.setText (label, juce::dontSendNotification);
    name.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (name);

    attachment = std::make_unique<APVTS::SliderAttachment> (s, id, slider);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    name.setBounds (r.removeFromTop (16));
    slider.setBounds (r);
}

//------------------------------------------------------------------------------
Fader::Fader (APVTS& s, const juce::String& id, const juce::String& label, std::atomic<float>& level)
    : levelSource (level)
{
    slider.setSliderStyle (juce::Slider::LinearVertical);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 18);
    slider.setColour (juce::Slider::trackColourId, Colours::cyan);
    addAndMakeVisible (slider);
    name.setText (label, juce::dontSendNotification);
    name.setJustificationType (juce::Justification::centred);
    name.setColour (juce::Label::textColourId, Colours::text);
    addAndMakeVisible (name);
    attachment = std::make_unique<APVTS::SliderAttachment> (s, id, slider);
}

void Fader::resized()
{
    auto r = getLocalBounds().reduced (6);
    name.setBounds (r.removeFromTop (20));
    meterArea = r.removeFromRight (10).withTrimmedBottom (22).reduced (0, 8);
    slider.setBounds (r);
}

void Fader::tick()
{
    const float v = juce::jlimit (-60.0f, 6.0f, levelSource.load());
    shown = v > shown ? v : shown - 1.2f;
    shown = juce::jmax (-60.0f, shown);
    repaint (meterArea);
}

void Fader::paint (juce::Graphics& g)
{
    g.setColour (Colours::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 10.0f);
    g.setColour (Colours::cyan.withAlpha (0.25f));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 10.0f, 1.0f);

    auto m = meterArea.toFloat();
    g.setColour (Colours::bg);
    g.fillRoundedRectangle (m, 3.0f);
    const float prop = (shown + 60.0f) / 66.0f;
    auto lit = m.withTop (m.getBottom() - m.getHeight() * prop);
    juce::ColourGradient grad (Colours::green, 0, m.getBottom(), Colours::pink, 0, m.getY(), false);
    grad.addColour (0.8, Colours::yellow);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (lit, 3.0f);
}

//------------------------------------------------------------------------------
Panel::Panel (APVTS& s, const juce::String& t, const juce::String& powerId, juce::Colour a)
    : accent (a), title (t)
{
    if (powerId.isNotEmpty())
    {
        power.setClickingTogglesState (true);
        power.setColour (juce::TextButton::buttonOnColourId, accent);
        addAndMakeVisible (power);
        powerAttachment = std::make_unique<APVTS::ButtonAttachment> (s, powerId, power);
    }
}

void Panel::resized()
{
    power.setBounds (getWidth() - 56, 8, 46, 22);
}

void Panel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::panel);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (accent.withAlpha (0.28f));
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);

    g.setColour (accent);
    g.fillRoundedRectangle (12.0f, 13.0f, 4.0f, 14.0f, 2.0f);
    g.setFont (uiFont (15.0f, true));
    g.setColour (Colours::text);
    g.drawText (title, 22, 8, getWidth() - 90, 24, juce::Justification::centredLeft);
}

static void setupTypeBox (juce::ComboBox& box, const juce::StringArray& items, juce::Colour accent)
{
    box.addItemList (items, 1);
    box.setColour (juce::ComboBox::arrowColourId, accent);
    box.setColour (juce::ComboBox::outlineColourId, accent.withAlpha (0.45f));
    box.setTooltip ("Modelo");
}

//==============================================================================
// AUTOTUNE
//==============================================================================
TunePanel::TunePanel (TopVocalChainProcessor& p)
    : Panel (p.apvts, "AUTOTUNE", "tune_on", Colours::cyan), proc (p),
      speed (p.apvts, "tune_speed", "VELOCIDAD", Colours::cyan),
      amount (p.apvts, "tune_amount", "CANTIDAD", Colours::cyan)
{
    keyBox.addItemList (TopVocalChainProcessor::keyNames(), 1);
    scaleBox.addItemList (TopVocalChainProcessor::scaleNames(), 1);
    addAndMakeVisible (keyBox);
    addAndMakeVisible (scaleBox);
    keyAtt = std::make_unique<APVTS::ComboBoxAttachment> (p.apvts, "tune_key", keyBox);
    scaleAtt = std::make_unique<APVTS::ComboBoxAttachment> (p.apvts, "tune_scale", scaleBox);
    addAndMakeVisible (speed);
    addAndMakeVisible (amount);
}

void TunePanel::resized()
{
    Panel::resized();
    auto r = content();
    auto top = r.removeFromTop (26);
    keyBox.setBounds (top.removeFromLeft (80));
    top.removeFromLeft (8);
    scaleBox.setBounds (top);
    r.removeFromTop (6);
    displayArea = r.removeFromTop (92);
    r.removeFromTop (6);
    speed.setBounds (r.removeFromLeft (r.getWidth() / 2));
    amount.setBounds (r);
}

void TunePanel::paint (juce::Graphics& g)
{
    Panel::paint (g);
    auto d = displayArea.toFloat();
    g.setColour (Colours::bg);
    g.fillRoundedRectangle (d, 8.0f);

    const float hz = proc.meters.pitchHz.load();
    const int note = (int) proc.meters.targetNote.load();
    const float cents = proc.meters.correctionCents.load();

    juce::String noteName = "--";
    if (note >= 0)
        noteName = TopVocalChainProcessor::keyNames()[note % 12] + juce::String (note / 12 - 1);

    g.setColour (note >= 0 ? Colours::cyan : Colours::dim);
    g.setFont (uiFont (40.0f, true));
    g.drawText (noteName, d.withHeight (56.0f).toNearestInt(), juce::Justification::centred);

    g.setFont (uiFont (12.0f));
    g.setColour (Colours::dim);
    g.drawText (hz > 0.0f ? juce::String (hz, 1) + " Hz" : juce::String ("sin voz"),
                d.withTrimmedTop (50.0f).withHeight (16.0f).toNearestInt(), juce::Justification::centred);

    // Barra de correccion (+-100 cents)
    auto bar = d.withTrimmedTop (70.0f).withHeight (10.0f).reduced (20.0f, 0.0f);
    g.setColour (Colours::panelHi);
    g.fillRoundedRectangle (bar, 4.0f);
    const float c = juce::jlimit (-100.0f, 100.0f, cents) / 100.0f;
    const float mid = bar.getCentreX();
    const float x2 = mid + c * bar.getWidth() * 0.5f;
    g.setColour (Colours::cyan);
    g.fillRoundedRectangle (juce::jmin (mid, x2), bar.getY(), std::abs (x2 - mid) + 2.0f, bar.getHeight(), 4.0f);
    g.setColour (Colours::text);
    g.fillRect (mid - 0.5f, bar.getY() - 2.0f, 1.0f, bar.getHeight() + 4.0f);
}

//==============================================================================
// IA VOICE
//==============================================================================
AIPanel::AIPanel (TopVocalChainProcessor& p)
    : Panel (p.apvts, "IA VOICE  -  escucha tu voz", {}, Colours::purple), proc (p),
      amount (p.apvts, "ai_amount", "INTELIGENCIA", Colours::purple)
{
    addAndMakeVisible (amount);
    listen.setClickingTogglesState (true);
    listen.setColour (juce::TextButton::buttonOnColourId, Colours::purple);
    addAndMakeVisible (listen);
    listenAtt = std::make_unique<APVTS::ButtonAttachment> (p.apvts, "ai_listen", listen);
    resetBtn.onClick = [this] { proc.requestAIReset(); };
    addAndMakeVisible (resetBtn);
}

void AIPanel::resized()
{
    Panel::resized();
    auto r = content();
    auto left = r.removeFromLeft (160);
    auto buttons = left.removeFromBottom (60);
    amount.setBounds (left);
    listen.setBounds (buttons.removeFromTop (28).reduced (4, 0));
    buttons.removeFromTop (4);
    resetBtn.setBounds (buttons.removeFromTop (26).reduced (4, 0));
    r.removeFromLeft (10);
    readoutArea = r;
}

void AIPanel::paint (juce::Graphics& g)
{
    Panel::paint (g);
    auto r = readoutArea.toFloat();
    g.setColour (Colours::bg);
    g.fillRoundedRectangle (r, 8.0f);
    auto t = r.reduced (10.0f, 8.0f);

    const auto& m = proc.meters;
    const float conf = m.aiConfidence.load();
    const float avg = m.aiAvgPitch.load();
    const bool listening = proc.apvts.getRawParameterValue ("ai_listen")->load() > 0.5f;

    g.setFont (uiFont (12.0f, true));
    g.setColour (listening ? Colours::purple : Colours::dim);
    g.drawText (listening ? "ESCUCHANDO..." : "ANALISIS CONGELADO", t.removeFromTop (16.0f).toNearestInt(), juce::Justification::centredLeft);

    // Barra de confianza
    auto cb = t.removeFromTop (8.0f);
    g.setColour (Colours::panelHi);
    g.fillRoundedRectangle (cb, 3.0f);
    g.setColour (Colours::purple);
    g.fillRoundedRectangle (cb.withWidth (cb.getWidth() * conf), 3.0f);
    t.removeFromTop (6.0f);

    juce::String voice = "Voz: --";
    if (avg > 0.0f)
        voice = juce::String ("Voz ") + (avg < 165.0f ? "grave" : avg < 300.0f ? "media" : "aguda")
              + "  (" + juce::String ((int) avg) + " Hz)";
    g.setFont (uiFont (13.0f, true));
    g.setColour (Colours::text);
    g.drawText (voice, t.removeFromTop (18.0f).toNearestInt(), juce::Justification::centredLeft);
    t.removeFromTop (4.0f);

    auto line = [&] (const juce::String& label, float value, const juce::String& unit, int decimals = 1)
    {
        auto row = t.removeFromTop (17.0f).toNearestInt();
        g.setFont (uiFont (12.0f));
        g.setColour (Colours::dim);
        g.drawText (label, row, juce::Justification::centredLeft);
        g.setColour (std::abs (value) > 0.05f ? Colours::text : Colours::dim);
        const auto num = decimals == 0 ? juce::String (juce::roundToInt (value)) : juce::String (value, decimals);
        g.drawText ((value > 0.05f ? "+" : "") + num + " " + unit, row, juce::Justification::centredRight);
    };

    line ("Ganancia de entrada", m.aiGain.load(), "dB");
    line ("Low cut", m.aiLowCut.load(), "Hz", 0);
    line ("Limpieza de barro (330 Hz)", m.aiMud.load(), "dB");
    line ("Presencia (4 kHz)", m.aiPresence.load(), "dB");
    line ("Aire (11 kHz)", m.aiAir.load(), "dB");
    line ("De-esser (7 kHz)", m.aiDeEss.load(), "dB");
    line ("Comp threshold", m.aiCompThr.load(), "dB");
    line ("Comp ratio", m.aiCompRatio.load(), ":1");
    line ("Velocidad autotune", m.aiTuneSpeed.load(), "ms", 0);
}

//==============================================================================
PreampPanel::PreampPanel (APVTS& s)
    : Panel (s, "PREAMP", "pre_on", Colours::orange),
      drive (s, "pre_drive", "DRIVE", Colours::orange),
      warmth (s, "pre_warmth", "WARMTH", Colours::orange),
      trim (s, "pre_trim", "TRIM", Colours::orange)
{
    setupTypeBox (type, TopVocalChainProcessor::preTypeNames(), accent);
    addAndMakeVisible (type);
    typeAtt = std::make_unique<APVTS::ComboBoxAttachment> (s, "pre_type", type);
    addAndMakeVisible (drive); addAndMakeVisible (warmth); addAndMakeVisible (trim);
}

void PreampPanel::resized()
{
    Panel::resized();
    auto r = content();
    type.setBounds (r.removeFromTop (26));
    r.removeFromTop (6);
    drive.setBounds (r.removeFromTop (r.getHeight() / 2 + 14).reduced (30, 0));
    warmth.setBounds (r.removeFromLeft (r.getWidth() / 2));
    trim.setBounds (r);
}

//==============================================================================
EqPanel::EqPanel (APVTS& s)
    : Panel (s, "EQ", "eq_on", Colours::green),
      lowCut (s, "eq_lowcut", "LOW CUT", Colours::green),
      low (s, "eq_low_gain", "LOW", Colours::green),
      midFreq (s, "eq_mid_freq", "MID FREQ", Colours::green),
      midGain (s, "eq_mid_gain", "MID", Colours::green),
      midQ (s, "eq_mid_q", "MID Q", Colours::green),
      presence (s, "eq_pres_gain", "PRESENCIA", Colours::green),
      air (s, "eq_air_gain", "AIRE", Colours::green)
{
    setupTypeBox (type, TopVocalChainProcessor::eqTypeNames(), accent);
    addAndMakeVisible (type);
    typeAtt = std::make_unique<APVTS::ComboBoxAttachment> (s, "eq_type", type);
    for (auto* k : { &lowCut, &low, &midFreq, &midGain, &midQ, &presence, &air })
        addAndMakeVisible (k);
}

void EqPanel::resized()
{
    Panel::resized();
    auto r = content();
    type.setBounds (r.removeFromTop (26));
    r.removeFromTop (6);
    auto row1 = r.removeFromTop (r.getHeight() / 2);
    const int w1 = row1.getWidth() / 4;
    for (auto* k : { &lowCut, &low, &midFreq, &midGain }) k->setBounds (row1.removeFromLeft (w1));
    const int w2 = r.getWidth() / 4;
    r.removeFromLeft (w2 / 2);
    for (auto* k : { &midQ, &presence, &air }) k->setBounds (r.removeFromLeft (w2));
}

//==============================================================================
CompPanel::CompPanel (TopVocalChainProcessor& p)
    : Panel (p.apvts, "COMPRESOR", "comp_on", Colours::yellow), proc (p),
      thr (p.apvts, "comp_thr", "THRESHOLD", Colours::yellow),
      ratio (p.apvts, "comp_ratio", "RATIO", Colours::yellow),
      makeup (p.apvts, "comp_makeup", "MAKEUP", Colours::yellow),
      attack (p.apvts, "comp_att", "ATTACK", Colours::yellow),
      release (p.apvts, "comp_rel", "RELEASE", Colours::yellow)
{
    setupTypeBox (type, TopVocalChainProcessor::compTypeNames(), accent);
    addAndMakeVisible (type);
    typeAtt = std::make_unique<APVTS::ComboBoxAttachment> (p.apvts, "comp_type", type);
    for (auto* k : { &thr, &ratio, &makeup, &attack, &release })
        addAndMakeVisible (k);
}

void CompPanel::resized()
{
    Panel::resized();
    auto r = content();
    type.setBounds (r.removeFromTop (26));
    r.removeFromTop (6);
    auto row1 = r.removeFromTop (r.getHeight() / 2);
    const int w = row1.getWidth() / 3;
    for (auto* k : { &thr, &ratio, &makeup }) k->setBounds (row1.removeFromLeft (w));
    attack.setBounds (r.removeFromLeft (w));
    release.setBounds (r.removeFromLeft (w));
    grArea = r.reduced (8, 4);
}

void CompPanel::tick()
{
    const float gr = proc.meters.compGrDb.load();
    shownGr = gr < shownGr ? gr : juce::jmin (0.0f, shownGr + 0.6f);
    repaint (grArea);
}

void CompPanel::paint (juce::Graphics& g)
{
    Panel::paint (g);
    auto r = grArea.toFloat();
    g.setFont (uiFont (11.0f, true));
    g.setColour (Colours::dim);
    g.drawText ("GR", r.removeFromTop (14.0f).toNearestInt(), juce::Justification::centred);
    auto val = r.removeFromBottom (16.0f);
    auto bar = r.withSizeKeepingCentre (16.0f, r.getHeight());
    g.setColour (Colours::bg);
    g.fillRoundedRectangle (bar, 3.0f);
    const float prop = juce::jlimit (0.0f, 1.0f, -shownGr / 24.0f);
    g.setColour (Colours::yellow);
    g.fillRoundedRectangle (bar.withHeight (bar.getHeight() * prop), 3.0f);
    g.setColour (Colours::text);
    g.drawText (juce::String (shownGr, 1), val.toNearestInt(), juce::Justification::centred);
}

//==============================================================================
DelayPanel::DelayPanel (APVTS& s)
    : Panel (s, "DELAY", "dly_on", Colours::pink),
      time (s, "dly_time", "TIEMPO", Colours::pink),
      feedback (s, "dly_fb", "FEEDBACK", Colours::pink),
      mix (s, "dly_mix", "MIX", Colours::pink),
      hiCut (s, "dly_hicut", "HI CUT", Colours::pink)
{
    for (auto* k : { &time, &feedback, &mix, &hiCut }) addAndMakeVisible (k);
    for (auto* b : { &sync, &pingPong })
    {
        b->setClickingTogglesState (true);
        b->setColour (juce::TextButton::buttonOnColourId, Colours::pink);
        addAndMakeVisible (b);
    }
    division.addItemList (TopVocalChainProcessor::divisionNames(), 1);
    addAndMakeVisible (division);
    syncAtt = std::make_unique<APVTS::ButtonAttachment> (s, "dly_sync", sync);
    ppAtt = std::make_unique<APVTS::ButtonAttachment> (s, "dly_pingpong", pingPong);
    divAtt = std::make_unique<APVTS::ComboBoxAttachment> (s, "dly_div", division);
}

void DelayPanel::resized()
{
    Panel::resized();
    auto r = content();
    auto controls = r.removeFromBottom (30);
    r.removeFromBottom (8);
    auto row1 = r.removeFromTop (r.getHeight() / 2);
    time.setBounds (row1.removeFromLeft (row1.getWidth() / 2));
    feedback.setBounds (row1);
    mix.setBounds (r.removeFromLeft (r.getWidth() / 2));
    hiCut.setBounds (r);

    const int w = controls.getWidth() / 3;
    sync.setBounds (controls.removeFromLeft (w).reduced (3, 2));
    division.setBounds (controls.removeFromLeft (w).reduced (3, 2));
    pingPong.setBounds (controls.reduced (3, 2));
}

//==============================================================================
ReverbPanel::ReverbPanel (APVTS& s)
    : Panel (s, "REVERB", "rev_on", Colours::cyan.interpolatedWith (Colours::purple, 0.5f)),
      size (s, "rev_size", "SIZE", accent),
      damping (s, "rev_damp", "DAMPING", accent),
      width (s, "rev_width", "WIDTH", accent),
      mix (s, "rev_mix", "MIX", accent)
{
    for (auto* k : { &size, &damping, &width, &mix }) addAndMakeVisible (k);
}

void ReverbPanel::resized()
{
    Panel::resized();
    auto r = content();
    auto row1 = r.removeFromTop (r.getHeight() / 2);
    size.setBounds (row1.removeFromLeft (row1.getWidth() / 2));
    damping.setBounds (row1);
    width.setBounds (r.removeFromLeft (r.getWidth() / 2));
    mix.setBounds (r);
}

//==============================================================================
// VISTA PRINCIPAL
//==============================================================================
MainView::MainView (TopVocalChainProcessor& p)
    : proc (p),
      inFader (p.apvts, "in_gain", "INPUT", p.meters.inLevelDb),
      outFader (p.apvts, "out_gain", "OUTPUT", p.meters.outLevelDb),
      tune (p), aiPanel (p), preamp (p.apvts), eqPanel (p.apvts), compPanel (p),
      delayPanel (p.apvts), reverbPanel (p.apvts)
{
    for (auto* c : std::initializer_list<juce::Component*> { &inFader, &outFader, &tune, &aiPanel, &preamp,
                                                             &eqPanel, &compPanel, &delayPanel, &reverbPanel,
                                                             &presetBox, &prevBtn, &nextBtn, &saveBtn, &loadBtn })
        addAndMakeVisible (c);

    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setTextWhenNothingSelected ("Preset");
    refreshPresetBox();

    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id > 0) proc.setCurrentProgram (id - 1);
    };

    auto step = [this] (int dir)
    {
        const int num = proc.getNumPrograms();
        const int next = (proc.getCurrentProgram() + dir + num) % num;
        proc.setCurrentProgram (next);
        refreshPresetBox();
    };
    prevBtn.onClick = [step] { step (-1); };
    nextBtn.onClick = [step] { step (+1); };

    saveBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Guardar preset", TopVocalChainProcessor::getUserPresetFolder(), "*.tvcpreset");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f != juce::File() && proc.saveUserPreset (f))
                                      presetBox.setText (f.getFileNameWithoutExtension(), juce::dontSendNotification);
                              });
    };

    loadBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Cargar preset", TopVocalChainProcessor::getUserPresetFolder(), "*.tvcpreset");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f.existsAsFile() && proc.loadUserPreset (f))
                                      presetBox.setText (f.getFileNameWithoutExtension(), juce::dontSendNotification);
                              });
    };
}

void MainView::refreshPresetBox()
{
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
}

void MainView::paint (juce::Graphics& g)
{
    juce::ColourGradient bgGrad (Colours::bg.brighter (0.06f), 0, 0, Colours::bg, 0, (float) H, false);
    g.setGradientFill (bgGrad);
    g.fillAll();

    // Encabezado
    g.setFont (uiFont (26.0f, true));
    g.setColour (Colours::text);
    g.drawText ("TOP", 20, 14, 60, 36, juce::Justification::centredLeft);
    g.setColour (Colours::cyan);
    g.drawText ("VOCAL CHAIN", 74, 14, 220, 36, juce::Justification::centredLeft);
    g.setFont (uiFont (11.0f));
    g.setColour (Colours::dim);
    g.drawText ("AUTOTUNE  -  PREAMP  -  EQ  -  COMP  -  DELAY  -  REVERB  -  IA", 270, 22, 400, 20, juce::Justification::centredLeft);

    g.setColour (Colours::cyan.withAlpha (0.15f));
    g.fillRect (0, 63, W, 1);
}

void MainView::resized()
{
    auto header = juce::Rectangle<int> (W - 560, 16, 540, 32);
    loadBtn.setBounds (header.removeFromRight (90));
    header.removeFromRight (6);
    saveBtn.setBounds (header.removeFromRight (90));
    header.removeFromRight (12);
    nextBtn.setBounds (header.removeFromRight (32));
    header.removeFromRight (4);
    auto prevArea = header.removeFromLeft (32);
    prevBtn.setBounds (prevArea);
    header.removeFromLeft (4);
    presetBox.setBounds (header);

    auto r = juce::Rectangle<int> (0, 64, W, H - 64).reduced (12);
    const int gap = 10;

    auto row1 = r.removeFromTop (318);
    r.removeFromTop (gap);
    auto row2 = r;

    inFader.setBounds (row1.removeFromLeft (90));       row1.removeFromLeft (gap);
    outFader.setBounds (row1.removeFromRight (90));     row1.removeFromRight (gap);
    tune.setBounds (row1.removeFromLeft (310));          row1.removeFromLeft (gap);
    preamp.setBounds (row1.removeFromRight (250));       row1.removeFromRight (gap);
    aiPanel.setBounds (row1);

    eqPanel.setBounds (row2.removeFromLeft (390));       row2.removeFromLeft (gap);
    compPanel.setBounds (row2.removeFromLeft (300));     row2.removeFromLeft (gap);
    reverbPanel.setBounds (row2.removeFromRight (220));  row2.removeFromRight (gap);
    delayPanel.setBounds (row2);
}

void MainView::tick()
{
    inFader.tick();
    outFader.tick();
    tune.tick();
    aiPanel.tick();
    compPanel.tick();
    if (presetBox.getSelectedId() > 0 && presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
        refreshPresetBox();
}

//==============================================================================
// EDITOR
//==============================================================================
TopVocalChainEditor::TopVocalChainEditor (TopVocalChainProcessor& p)
    : AudioProcessorEditor (&p), view (p)
{
    view.setLookAndFeel (&lnf);
    addAndMakeVisible (view);

    setResizable (true, true);
    setResizeLimits (tvcui::MainView::W * 6 / 10, tvcui::MainView::H * 6 / 10,
                     tvcui::MainView::W * 2, tvcui::MainView::H * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) tvcui::MainView::W / tvcui::MainView::H);
    setSize (tvcui::MainView::W, tvcui::MainView::H);
    startTimerHz (30);
}

TopVocalChainEditor::~TopVocalChainEditor()
{
    stopTimer();
    view.setLookAndFeel (nullptr);
}

void TopVocalChainEditor::paint (juce::Graphics& g)
{
    g.fillAll (tvcui::Colours::bg);
}

void TopVocalChainEditor::resized()
{
    const float scale = (float) getWidth() / (float) tvcui::MainView::W;
    view.setBounds (0, 0, tvcui::MainView::W, tvcui::MainView::H);
    view.setTransform (juce::AffineTransform::scale (scale));
}

void TopVocalChainEditor::timerCallback()
{
    view.tick();
}
