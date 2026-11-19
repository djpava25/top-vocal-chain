#include "PluginEditor.h"

using namespace tvcui;
using namespace cui;

static juce::String U (const char* s) { return juce::String::fromUTF8 (s); }

namespace M
{
    // Columnas de la tira (x, ancho) y modulos
    const juce::Rectangle<int> pre  { 12,   56, 214, 420 };
    const juce::Rectangle<int> ai   { 12,  482, 214, 404 };
    const juce::Rectangle<int> tune { 232,  56, 232, 404 };
    const juce::Rectangle<int> wide { 232, 466, 232, 420 };
    const juce::Rectangle<int> comp { 470,  56, 214, 830 };
    const juce::Rectangle<int> eq   { 690,  56, 232, 830 };
    const juce::Rectangle<int> dly  { 928,  56, 214, 460 };
    const juce::Rectangle<int> rev  { 928, 522, 214, 364 };
    const juce::Rectangle<int> met  { 1148, 56, 200, 830 };
    const juce::Rectangle<int> out  { 1354, 56, 126, 830 };

    inline juce::Rectangle<int> at (juce::Rectangle<int> m, int x, int y, int w, int h) { return { m.getX() + x, m.getY() + y, w, h }; }
    inline juce::Rectangle<int> inBtn (juce::Rectangle<int> m) { return { m.getRight() - 56, m.getY() + 7, 48, 22 }; }
}

//==============================================================================
MainView::MainView (TopVocalChainProcessor& p)
    : proc (p),
      drive (p.apvts, "pre_drive", "DRIVE", Col::red, "0", "12", "24"),
      inGain (p.apvts, "in_gain", "INPUT", Col::slate, "-24", "0", "+24"),
      warmth (p.apvts, "pre_warmth", "WARMTH", Col::orange, "0", "", "100"),
      trim (p.apvts, "pre_trim", "TRIM", Col::slate, "-12", "0", "+12"),
      aiAmount (p.apvts, "ai_amount", "INTELIGENCIA", Col::purple, "0", "50", "100"),
      tuneSpeed (p.apvts, "tune_speed", "SPEED", Col::green, "0", "60", "400"),
      tuneAmount (p.apvts, "tune_amount", "AMOUNT", Col::green, "0", "", "100"),
      wideAmount (p.apvts, "wide_amount", "WIDER", Col::purple, "0", "50", "100"),
      thr (p.apvts, "comp_thr", "THRESHOLD", Col::charcoal, "-60", "-30", "0"),
      ratio (p.apvts, "comp_ratio", "RATIO", Col::charcoal, "1:1", "4:1", "20:1"),
      attack (p.apvts, "comp_att", "ATTACK", Col::slate, "0.1", "8", "100"),
      release (p.apvts, "comp_rel", "RELEASE", Col::slate, "10", "150", "1s"),
      makeup (p.apvts, "comp_makeup", "MAKEUP", Col::red, "0", "12", "24"),
      air (p.apvts, "eq_air_gain", "AIR", Col::green, "-12", "0", "+12"),
      presence (p.apvts, "eq_pres_gain", "PRESENCE", Col::green, "-12", "0", "+12"),
      midFreq (p.apvts, "eq_mid_freq", "MID FREQ", Col::blue, "200", "1k", "5k"),
      midGain (p.apvts, "eq_mid_gain", "MID", Col::blue, "-12", "0", "+12"),
      midQ (p.apvts, "eq_mid_q", "MID Q", Col::charcoal, "0.3", "1", "5"),
      low (p.apvts, "eq_low_gain", "LOW", Col::orange, "-12", "0", "+12"),
      lowCut (p.apvts, "eq_lowcut", "LOW CUT", Col::orange, "20", "100", "500"),
      dTime (p.apvts, "dly_time", "TIME", Col::mustard, "10", "", "2s"),
      dFeedback (p.apvts, "dly_fb", "FEEDBACK", Col::mustard, "0", "", "95"),
      dMix (p.apvts, "dly_mix", "MIX", Col::charcoal, "0", "", "100"),
      dHiCut (p.apvts, "dly_hicut", "HI CUT", Col::slate, "1k", "5k", "20k"),
      rSize (p.apvts, "rev_size", "SIZE", Col::blue, "0", "", "100"),
      rDamp (p.apvts, "rev_damp", "DAMPING", Col::slate, "0", "", "100"),
      rWidth (p.apvts, "rev_width", "WIDTH", Col::slate, "0", "", "100"),
      rMix (p.apvts, "rev_mix", "MIX", Col::charcoal, "0", "", "100"),
      output (p.apvts, "out_gain"),
      inMeter ([this] { return proc.meters.inLevelDb.load(); }, false),
      outMeter ([this] { return proc.meters.outLevelDb.load(); }, false),
      grMeter ([this] { return proc.meters.compGrDb.load(); }, true)
{
    for (auto* k : { &drive, &inGain, &warmth, &trim, &aiAmount, &tuneSpeed, &tuneAmount, &wideAmount, &thr, &ratio,
                     &attack, &release, &makeup, &air, &presence, &midFreq, &midGain, &midQ, &low, &lowCut,
                     &dTime, &dFeedback, &dMix, &dHiCut, &rSize, &rDamp, &rWidth, &rMix })
        addAndMakeVisible (k);
    addAndMakeVisible (output);
    for (auto* m : { &inMeter, &outMeter, &grMeter }) addAndMakeVisible (m);

    attachButton (preIn, "pre_on");
    attachButton (tuneIn, "tune_on");
    attachButton (wideIn, "wide_on");
    attachButton (compIn, "comp_on");
    attachButton (eqIn, "eq_on");
    attachButton (dlyIn, "dly_on");
    attachButton (revIn, "rev_on");
    attachButton (aiListen, "ai_listen");
    attachButton (dlySync, "dly_sync");
    attachButton (dlyPing, "dly_pingpong");

    setupBox (preType, TopVocalChainProcessor::preTypeNames(), "pre_type");
    setupBox (compType, TopVocalChainProcessor::compTypeNames(), "comp_type");
    setupBox (eqType, TopVocalChainProcessor::eqTypeNames(), "eq_type");
    setupBox (keyBox, TopVocalChainProcessor::keyNames(), "tune_key");
    setupBox (scaleBox, TopVocalChainProcessor::scaleNames(), "tune_scale");
    setupBox (divBox, TopVocalChainProcessor::divisionNames(), "dly_div");

    // Selector de efecto del WIDER (botones tipo radio)
    const auto& modes = TopVocalChainProcessor::wideModeNames();
    for (int i = 0; i < modes.size(); ++i)
    {
        auto* b = wideModes.add (new LitButton (modes[i].toUpperCase(), Col::amber));
        b->onClick = [this, i]
        {
            auto* prm = proc.apvts.getParameter ("wide_mode");
            prm->setValueNotifyingHost (prm->convertTo0to1 ((float) i));
        };
        addAndMakeVisible (b);
    }

    aiReset.onClick = [this] { proc.requestAIReset(); };
    addAndMakeVisible (aiReset);

    // Presets
    for (auto* c : std::initializer_list<juce::Component*> { &presetBox, &prevBtn, &nextBtn, &saveBtn, &loadBtn })
        addAndMakeVisible (c);
    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setTextWhenNothingSelected ("Preset");
    refreshPresetBox();
    presetBox.onChange = [this] { const int id = presetBox.getSelectedId(); if (id > 0) proc.setCurrentProgram (id - 1); };
    auto step = [this] (int d)
    {
        const int n = proc.getNumPrograms();
        proc.setCurrentProgram ((proc.getCurrentProgram() + d + n) % n);
        refreshPresetBox();
    };
    prevBtn.onClick = [step] { step (-1); };
    nextBtn.onClick = [step] { step (1); };

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

    buildBackground();
}

void MainView::attachButton (juce::Button& b, const char* id)
{
    addAndMakeVisible (b);
    btnAtts.push_back (std::make_unique<APVTS::ButtonAttachment> (proc.apvts, id, b));
}

void MainView::setupBox (juce::ComboBox& box, const juce::StringArray& items, const char* id)
{
    box.addItemList (items, 1);
    addAndMakeVisible (box);
    boxAtts.push_back (std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, id, box));
}

void MainView::refreshPresetBox()
{
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
}

//------------------------------------------------------------------------------
void MainView::buildBackground()
{
    background = juce::Image (juce::Image::ARGB, W, H, true);
    juce::Graphics g (background);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1d1f23), 0, 0, Col::backdrop, 0, (float) H, false));
    g.fillAll();

    // Barra superior
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c2e33), 0, 0, juce::Colour (0xff17181b), 0, 48, false));
    g.fillRect (0, 0, W, 48);
    g.setColour (juce::Colours::black); g.fillRect (0, 47, W, 2);
    g.setFont (font (22.0f, true));
    g.setColour (Col::cream);
    g.drawText ("TOP VOCAL CHAIN", 18, 8, 260, 32, juce::Justification::centredLeft);
    g.setFont (font (12.0f));
    g.setColour (Col::cream.withAlpha (0.55f));
    g.drawText (U ("CONSOLA VOCAL  ·  PREAMP  ·  AUTOTUNE  ·  COMP  ·  EQ  ·  WIDER  ·  DELAY  ·  REVERB  ·  IA"),
                250, 8, 560, 32, juce::Justification::centredLeft);
    g.setFont (font (13.0f, true));
    g.drawText ("PRESET", 760, 8, 70, 32, juce::Justification::centredRight);

    // Modulos
    int seed = 11;
    for (auto [r, t] : { std::pair<juce::Rectangle<int>, const char*> { M::pre, "PREAMP" }, { M::ai, "IA VOICE" },
                         { M::tune, "AUTOTUNE" }, { M::wide, "WIDER" }, { M::comp, "COMPRESSOR" },
                         { M::eq, "EQUALIZER" }, { M::dly, "DELAY" }, { M::rev, "REVERB" },
                         { M::met, "METERS" }, { M::out, "OUTPUT" } })
        moduleFrame (g, r.toFloat(), t, seed += 17);

    // Bandas del EQ (sub-paneles tintados)
    auto band = [&] (int y, int h, juce::Colour c, const char* name)
    {
        auto r = M::at (M::eq, 8, y, 216, h).toFloat();
        g.setColour (c);
        g.fillRoundedRectangle (r, 4);
        g.setColour (juce::Colours::black.withAlpha (0.15f));
        g.drawRoundedRectangle (r, 4, 1.0f);
        g.setFont (font (11.0f, true));
        g.setColour (Col::inkDim);
        g.drawText (name, r.reduced (8, 4).toNearestInt(), juce::Justification::topLeft);
    };
    band (82, 140, Col::sage, "HIGH");
    band (228, 260, Col::panelTint, "MID");
    band (494, 140, Col::sand, "LOW");

    // Sub-panel del compresor (makeup + GR)
    auto cpan = M::at (M::comp, 8, 528, 198, 150).toFloat();
    g.setColour (Col::sand); g.fillRoundedRectangle (cpan, 4);
    g.setColour (juce::Colours::black.withAlpha (0.15f)); g.drawRoundedRectangle (cpan, 4, 1.0f);

    // Escala de los medidores
    g.setFont (font (11.0f, true));
    g.setColour (Col::ink);
    const auto meterTop = (float) M::met.getY() + 104, meterH = 560.0f - 6.0f;
    for (int db : { 6, 0, -6, -12, -18, -24, -36, -48 })
    {
        const float prop = LedMeter::levelToProp ((float) db);
        const float y = meterTop + 3 + meterH * (1.0f - prop);
        g.drawText ((db > 0 ? "+" : "") + juce::String (db), M::met.getX() + 6, (int) y - 7, 24, 14, juce::Justification::centredRight);
    }
    for (int gr : { 0, 5, 10, 15, 20 })
    {
        const float y = meterTop + 3 + meterH * ((float) gr / 20.0f);
        g.drawText (juce::String (gr), M::met.getX() + 170, (int) y - 7, 24, 14, juce::Justification::centredLeft);
    }
    for (auto [x, t] : { std::pair<int, const char*> { 34, "IN" }, { 80, "OUT" }, { 132, "GR" } })
        g.drawText (t, M::met.getX() + x - 6, M::met.getY() + 668, 42, 16, juce::Justification::centred);

    // Placa del fader
    auto plate = M::at (M::out, 10, 720, 106, 96).toFloat();
    g.setGradientFill (juce::ColourGradient (Col::header.brighter (0.2f), 0, plate.getY(), Col::header, 0, plate.getBottom(), false));
    g.fillRoundedRectangle (plate, 4);
    g.setColour (Col::cream);
    g.setFont (font (14.0f, true));
    g.drawText ("TOP", plate.withHeight (32).translated (0, 10), juce::Justification::centred);
    g.setFont (font (14.0f, true));
    g.drawText ("VOCAL", plate.withHeight (32).translated (0, 30), juce::Justification::centred);
    g.setFont (font (14.0f, true));
    g.drawText ("CHAIN", plate.withHeight (32).translated (0, 50), juce::Justification::centred);
    g.setFont (font (12.0f, true));
    g.setColour (Col::ink);
    g.drawText ("dB", M::at (M::out, 10, 40, 40, 16), juce::Justification::centredLeft);
}

//------------------------------------------------------------------------------
void MainView::resized()
{
    prevBtn.setBounds (836, 10, 30, 28);
    presetBox.setBounds (870, 10, 300, 28);
    nextBtn.setBounds (1174, 10, 30, 28);
    saveBtn.setBounds (1224, 10, 120, 28);
    loadBtn.setBounds (1352, 10, 120, 28);

    // PREAMP
    preIn.setBounds (M::inBtn (M::pre));
    preType.setBounds (M::at (M::pre, 14, 46, 186, 24));
    drive.setBounds (M::at (M::pre, 37, 90, 140, 140));
    inGain.setBounds (M::at (M::pre, 6, 230, 100, 96));
    warmth.setBounds (M::at (M::pre, 108, 230, 100, 96));
    trim.setBounds (M::at (M::pre, 57, 326, 100, 86));
    preInfo = {};

    // IA VOICE
    aiListen.setBounds (M::inBtn (M::ai).withLeft (M::ai.getRight() - 96));
    aiAmount.setBounds (M::at (M::ai, 37, 44, 140, 140));
    aiReset.setBounds (M::at (M::ai, 70, 188, 74, 24));
    aiDisp = M::at (M::ai, 12, 220, 190, 170);

    // AUTOTUNE
    tuneIn.setBounds (M::inBtn (M::tune));
    tuneDisp = M::at (M::tune, 12, 46, 208, 98);
    keyBox.setBounds (M::at (M::tune, 12, 152, 70, 24));
    scaleBox.setBounds (M::at (M::tune, 88, 152, 132, 24));
    tuneSpeed.setBounds (M::at (M::tune, 8, 186, 108, 126));
    tuneAmount.setBounds (M::at (M::tune, 116, 186, 108, 126));

    // WIDER
    wideIn.setBounds (M::inBtn (M::wide));
    for (int i = 0; i < wideModes.size(); ++i)
    {
        const int row = i < 3 ? 0 : 1;
        const int x = row == 0 ? 12 + i * 70 : 12 + (i - 3) * 105;
        const int w = row == 0 ? 66 : 101;
        wideModes[i]->setBounds (M::at (M::wide, x, 46 + row * 30, w, 26));
    }
    wideAmount.setBounds (M::at (M::wide, 36, 112, 160, 170));
    wideDisp = M::at (M::wide, 12, 292, 208, 106);

    // COMPRESSOR
    compIn.setBounds (M::inBtn (M::comp));
    compType.setBounds (M::at (M::comp, 14, 46, 186, 24));
    thr.setBounds (M::at (M::comp, 22, 80, 170, 168));
    ratio.setBounds (M::at (M::comp, 37, 252, 140, 140));
    attack.setBounds (M::at (M::comp, 4, 400, 104, 116));
    release.setBounds (M::at (M::comp, 106, 400, 104, 116));
    makeup.setBounds (M::at (M::comp, 14, 536, 120, 132));
    compInfo = M::at (M::comp, 14, 690, 186, 120);

    // EQUALIZER
    eqIn.setBounds (M::inBtn (M::eq));
    eqType.setBounds (M::at (M::eq, 14, 46, 204, 26));
    air.setBounds (M::at (M::eq, 12, 100, 104, 116));
    presence.setBounds (M::at (M::eq, 116, 100, 104, 116));
    midFreq.setBounds (M::at (M::eq, 12, 246, 104, 116));
    midGain.setBounds (M::at (M::eq, 116, 246, 104, 116));
    midQ.setBounds (M::at (M::eq, 64, 366, 104, 116));
    low.setBounds (M::at (M::eq, 12, 512, 104, 116));
    lowCut.setBounds (M::at (M::eq, 116, 512, 104, 116));
    eqCurve = M::at (M::eq, 12, 644, 208, 104);
    eqInfo = M::at (M::eq, 12, 754, 208, 56);

    // DELAY
    dlyIn.setBounds (M::inBtn (M::dly));
    dTime.setBounds (M::at (M::dly, 4, 44, 104, 116));
    dFeedback.setBounds (M::at (M::dly, 106, 44, 104, 116));
    dMix.setBounds (M::at (M::dly, 4, 162, 104, 116));
    dHiCut.setBounds (M::at (M::dly, 106, 162, 104, 116));
    dlySync.setBounds (M::at (M::dly, 14, 290, 84, 26));
    divBox.setBounds (M::at (M::dly, 106, 290, 94, 26));
    dlyPing.setBounds (M::at (M::dly, 14, 324, 186, 26));
    dlyDisp = M::at (M::dly, 14, 362, 186, 76);

    // REVERB
    revIn.setBounds (M::inBtn (M::rev));
    rSize.setBounds (M::at (M::rev, 4, 46, 104, 116));
    rDamp.setBounds (M::at (M::rev, 106, 46, 104, 116));
    rWidth.setBounds (M::at (M::rev, 4, 172, 104, 116));
    rMix.setBounds (M::at (M::rev, 106, 172, 104, 116));

    // METERS
    meterDisp = M::at (M::met, 12, 46, 176, 48);
    inMeter.setBounds (M::at (M::met, 34, 104, 30, 560));
    outMeter.setBounds (M::at (M::met, 80, 104, 30, 560));
    grMeter.setBounds (M::at (M::met, 136, 104, 30, 560));
    noteDisp = M::at (M::met, 12, 694, 176, 120);

    // OUTPUT
    output.setBounds (M::at (M::out, 6, 58, 112, 650));
}

//------------------------------------------------------------------------------
static const char* preText[]  { "Válvula: cálida, armónicos pares", "Cinta: suave y redonda, agudos dulces",
                                "Transistor: filo y mordida", "Transformador: cuerpo en los graves",
                                "Diodo: agresivo y asimétrico" };
static const char* compText[] { "Limpio (VCA): transparente, respeta Attack y Release.",
                                "Opto (estilo LA-2A): lento y musical. Cuanto más comprime, más lento suelta.",
                                "FET (estilo 1176): atrapa los picos en ~1 ms. Agresivo y con presencia.",
                                "Vari-Mu (estilo Fairchild): el ratio sube con el nivel. Suave y pegado." };
static const char* eqText[]   { "Limpio: preciso, sin color.",
                                "Estilo Neve 1073: frecuencias por pasos, curvas anchas, color de transformador.",
                                "Estilo API 550: Q proporcional (más ganancia, curva más estrecha). Punch.",
                                "Estilo Pultec: truco de graves (realce + corte) y aire sedoso." };
static const char* wideText[] { "Ensancha la voz sin perder nada en mono.",
                                "Dos voces moduladas en contrafase.",
                                "Doblaje automático: dos tomas falsas a los lados.",
                                "Izquierda unos cents arriba, derecha abajo.",
                                "Ensemble de 3 voces, espacioso y sutil." };

void MainView::drawDisplays (juce::Graphics& g)
{
    const auto& m = proc.meters;

    // AUTOTUNE
    {
        auto r = tuneDisp.toFloat();
        lcd (g, r);
        const int note = (int) m.targetNote.load();
        const float hz = m.pitchHz.load(), cents = m.correctionCents.load();
        juce::String name = "--";
        if (note >= 0) name = TopVocalChainProcessor::keyNames()[note % 12] + juce::String (note / 12 - 1);
        lcdText (g, name, r.withTrimmedLeft (12).withWidth (100).withHeight (60), 38.0f, true, juce::Justification::centredLeft,
                 note >= 0 ? Col::amber : Col::amber.withAlpha (0.35f));
        auto info = r.withTrimmedLeft (106).reduced (8, 10);
        lcdText (g, hz > 0 ? juce::String (hz, 1) + " Hz" : U ("SIN VOZ"), info.removeFromTop (18), 13.0f, true, juce::Justification::centredRight);
        lcdText (g, param ("tune_on") ? "CORR " + juce::String (juce::roundToInt (cents)) + " c" : juce::String ("BYPASS"),
                 info.removeFromTop (18), 12.0f, false, juce::Justification::centredRight, Col::amber.withAlpha (0.8f));
        auto bar = r.withTrimmedTop (r.getHeight() - 24).reduced (12, 7);
        g.setColour (Col::amber.withAlpha (0.12f)); g.fillRect (bar);
        const float c = juce::jlimit (-100.0f, 100.0f, cents) / 100.0f;
        const float mid = bar.getCentreX(), x2 = mid + c * bar.getWidth() * 0.5f;
        g.setColour (Col::amber); g.fillRect (juce::jmin (mid, x2), bar.getY(), std::abs (x2 - mid) + 1.5f, bar.getHeight());
        g.setColour (Col::amber.withAlpha (0.6f)); g.fillRect (mid - 0.5f, bar.getY() - 3, 1.0f, bar.getHeight() + 6);
    }

    // IA VOICE
    {
        auto r = aiDisp.toFloat();
        lcd (g, r);
        auto t = r.reduced (8, 7);
        lcdText (g, param ("ai_listen") ? "ESCUCHANDO" : "CONGELADO", t.removeFromTop (15), 11.5f, true, juce::Justification::centredLeft);
        auto cb = t.removeFromTop (5);
        g.setColour (Col::amber.withAlpha (0.15f)); g.fillRect (cb);
        g.setColour (Col::amber); g.fillRect (cb.withWidth (cb.getWidth() * m.aiConfidence.load()));
        t.removeFromTop (4);
        const float avg = m.aiAvgPitch.load();
        lcdText (g, avg > 0 ? juce::String (avg < 165 ? "VOZ GRAVE " : avg < 300 ? "VOZ MEDIA " : "VOZ AGUDA ") + juce::String ((int) avg) + " Hz"
                            : juce::String ("VOZ --"), t.removeFromTop (15), 11.5f, true, juce::Justification::centredLeft);
        auto row = [&] (const juce::String& label, float v, const juce::String& unit, int dec = 1)
        {
            auto rr = t.removeFromTop (15.5f);
            lcdText (g, label, rr, 10.5f, false, juce::Justification::centredLeft, Col::amber.withAlpha (0.65f));
            const auto num = dec == 0 ? juce::String (juce::roundToInt (v)) : juce::String (v, dec);
            lcdText (g, (v > 0.05f ? "+" : "") + num + " " + unit, rr, 11.0f, true, juce::Justification::centredRight);
        };
        row ("GANANCIA", m.aiGain.load(), "dB");
        row ("LOW CUT", m.aiLowCut.load(), "Hz", 0);
        row ("BARRO", m.aiMud.load(), "dB");
        row ("PRESENCIA", m.aiPresence.load(), "dB");
        row ("AIRE", m.aiAir.load(), "dB");
        row ("DE-ESSER", m.aiDeEss.load(), "dB");
        row ("COMP THR", m.aiCompThr.load(), "dB");
        row ("VEL. TUNE", m.aiTuneSpeed.load(), "ms", 0);
    }

    // WIDER
    {
        auto r = wideDisp.toFloat();
        lcd (g, r);
        auto t = r.reduced (10, 8);
        const int mode = juce::jlimit (0, 4, choice ("wide_mode"));
        lcdText (g, TopVocalChainProcessor::wideModeNames()[mode].toUpperCase() + (param ("wide_on") ? "" : "  (OFF)"),
                 t.removeFromTop (18), 14.0f, true, juce::Justification::centredLeft);
        g.setFont (font (11.5f));
        g.setColour (Col::amber.withAlpha (0.75f));
        g.drawFittedText (U (wideText[mode]), t.removeFromTop (32).toNearestInt(), juce::Justification::topLeft, 2);
        t.removeFromTop (4);
        lcdText (g, U ("ANCHO ESTÉREO"), t.removeFromTop (14), 10.5f, false, juce::Justification::centredLeft, Col::amber.withAlpha (0.65f));
        auto bar = t.removeFromTop (12);
        g.setColour (Col::amber.withAlpha (0.12f)); g.fillRect (bar);
        const float wv = juce::jlimit (0.0f, 1.0f, m.stereoWidth.load() * 1.6f);
        g.setColour (Col::amber); g.fillRect (bar.withSizeKeepingCentre (bar.getWidth() * wv, bar.getHeight()));
    }

    // EQ: curva de respuesta
    {
        auto r = eqCurve.toFloat();
        lcd (g, r);
        auto plot = r.reduced (6, 6);
        auto xOf = [&] (double f) { return plot.getX() + (float) (std::log10 (f / 20.0) / 3.0) * plot.getWidth(); };
        auto yOf = [&] (float db) { return plot.getCentreY() - juce::jlimit (-15.0f, 15.0f, db) / 15.0f * plot.getHeight() * 0.5f; };
        g.setColour (Col::amber.withAlpha (0.12f));
        for (double f : { 100.0, 1000.0, 10000.0 }) g.fillRect (xOf (f), plot.getY(), 1.0f, plot.getHeight());
        g.fillRect (plot.getX(), yOf (0), plot.getWidth(), 1.0f);
        juce::Path curve;
        for (int i = 0; i <= 100; ++i)
        {
            const double f = 20.0 * std::pow (1000.0, i / 100.0);
            const float y = yOf (proc.getEqMagnitudeDb (f));
            if (i == 0) curve.startNewSubPath (xOf (f), y); else curve.lineTo (xOf (f), y);
        }
        g.setColour (Col::amber.withAlpha (0.25f));
        g.strokePath (curve, juce::PathStrokeType (4.0f));
        g.setColour (Col::amber);
        g.strokePath (curve, juce::PathStrokeType (1.6f));
        g.setFont (font (9.5f));
        g.setColour (Col::amber.withAlpha (0.5f));
        g.drawText ("100", juce::Rectangle<float> (xOf (100) + 2, plot.getBottom() - 11, 30, 10), juce::Justification::left);
        g.drawText ("1k", juce::Rectangle<float> (xOf (1000) + 2, plot.getBottom() - 11, 30, 10), juce::Justification::left);
        g.drawText ("10k", juce::Rectangle<float> (xOf (10000) + 2, plot.getBottom() - 11, 30, 10), juce::Justification::left);
    }

    // Textos de modelo
    g.setFont (font (11.5f));
    g.setColour (Col::inkDim);
    g.drawFittedText (U (eqText[juce::jlimit (0, 3, choice ("eq_type"))]), eqInfo, juce::Justification::topLeft, 3);
    g.setFont (font (12.0f, true));
    g.setColour (Col::ink);
    g.drawText ("MODELO", compInfo.withHeight (16), juce::Justification::topLeft);
    g.setFont (font (11.5f));
    g.setColour (Col::inkDim);
    g.drawFittedText (U (compText[juce::jlimit (0, 3, choice ("comp_type"))]), compInfo.withTrimmedTop (18), juce::Justification::topLeft, 5);
    g.setFont (font (11.0f));
    g.drawFittedText (U (preText[juce::jlimit (0, 4, choice ("pre_type"))]), M::at (M::pre, 14, 73, 186, 14), juce::Justification::centred, 1);

    // Medidor de GR del compresor (valor)
    {
        auto r = M::at (M::comp, 140, 556, 58, 100).toFloat();
        lcd (g, r);
        lcdText (g, "GR", r.withHeight (24), 11.0f, true, juce::Justification::centred, Col::amber.withAlpha (0.7f));
        lcdText (g, juce::String (m.compGrDb.load(), 1), r.withTrimmedTop (30).withHeight (30), 17.0f, true, juce::Justification::centred);
        lcdText (g, "dB", r.withTrimmedTop (62).withHeight (20), 10.5f, false, juce::Justification::centred, Col::amber.withAlpha (0.7f));
    }

    // DELAY
    {
        auto r = dlyDisp.toFloat();
        lcd (g, r);
        const auto t = param ("dly_sync") ? TopVocalChainProcessor::divisionNames()[choice ("dly_div")] + "  SYNC"
                                          : juce::String (juce::roundToInt (proc.apvts.getRawParameterValue ("dly_time")->load())) + " ms";
        lcdText (g, param ("dly_on") ? t : juce::String ("OFF"), r.withTrimmedTop (8).withHeight (28), 20.0f, true, juce::Justification::centred);
        lcdText (g, param ("dly_pingpong") ? "PING PONG" : U ("ESTÉREO"), r.withTrimmedTop (42).withHeight (20), 11.5f, false,
                 juce::Justification::centred, Col::amber.withAlpha (0.7f));
    }

    // METERS: valores pico
    {
        auto r = meterDisp.toFloat();
        lcd (g, r);
        auto left = r.withWidth (r.getWidth() * 0.5f), right = r.withTrimmedLeft (r.getWidth() * 0.5f);
        auto val = [] (float db) { return db < -90.0f ? juce::String ("-inf") : juce::String (db, 1); };
        lcdText (g, "IN", left.withHeight (18), 10.5f, true, juce::Justification::centred, Col::amber.withAlpha (0.7f));
        lcdText (g, val (m.inLevelDb.load()), left.withTrimmedTop (18), 16.0f, true, juce::Justification::centred);
        lcdText (g, "OUT", right.withHeight (18), 10.5f, true, juce::Justification::centred, Col::amber.withAlpha (0.7f));
        lcdText (g, val (m.outLevelDb.load()), right.withTrimmedTop (18), 16.0f, true, juce::Justification::centred);
    }

    // METERS: nota y ancho
    {
        auto r = noteDisp.toFloat();
        lcd (g, r);
        auto t = r.reduced (10, 8);
        const int note = (int) m.targetNote.load();
        lcdText (g, "NOTA", t.removeFromTop (14), 10.5f, false, juce::Justification::centredLeft, Col::amber.withAlpha (0.65f));
        lcdText (g, note >= 0 ? TopVocalChainProcessor::keyNames()[note % 12] + juce::String (note / 12 - 1) : juce::String ("--"),
                 t.removeFromTop (30), 26.0f, true, juce::Justification::centredLeft);
        lcdText (g, U ("ESTÉREO"), t.removeFromTop (14), 10.5f, false, juce::Justification::centredLeft, Col::amber.withAlpha (0.65f));
        lcdText (g, juce::String (juce::roundToInt (juce::jlimit (0.0f, 1.0f, m.stereoWidth.load() * 1.6f) * 100.0f)) + " %",
                 t.removeFromTop (24), 18.0f, true, juce::Justification::centredLeft);
    }
}

void MainView::paint (juce::Graphics& g)
{
    g.drawImageAt (background, 0, 0);
    drawDisplays (g);
}

void MainView::tick()
{
    for (auto* mtr : { &inMeter, &outMeter, &grMeter }) mtr->tick();
    const int mode = choice ("wide_mode");
    for (int i = 0; i < wideModes.size(); ++i)
        if (wideModes[i]->getToggleState() != (i == mode))
            wideModes[i]->setToggleState (i == mode, juce::dontSendNotification);

    for (auto r : { tuneDisp, aiDisp, wideDisp, eqCurve, eqInfo, compInfo, dlyDisp, meterDisp, noteDisp })
        repaint (r.expanded (4));
    repaint (M::at (M::comp, 136, 552, 66, 108));
    repaint (M::at (M::pre, 10, 70, 194, 18));

    if (presetBox.getSelectedId() > 0 && presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
        refreshPresetBox();
}

//==============================================================================
TopVocalChainEditor::TopVocalChainEditor (TopVocalChainProcessor& p)
    : AudioProcessorEditor (&p), view (p)
{
    view.setLookAndFeel (&lnf);
    addAndMakeVisible (view);
    setResizable (true, true);
    setResizeLimits (tvcui::MainView::W / 2, tvcui::MainView::H / 2, tvcui::MainView::W * 3 / 2, tvcui::MainView::H * 3 / 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) tvcui::MainView::W / tvcui::MainView::H);
    setSize (tvcui::MainView::W * 3 / 4, tvcui::MainView::H * 3 / 4);   // 1119 x 675 por defecto
    startTimerHz (30);
}

TopVocalChainEditor::~TopVocalChainEditor()
{
    stopTimer();
    view.setLookAndFeel (nullptr);
}

void TopVocalChainEditor::paint (juce::Graphics& g) { g.fillAll (cui::Col::backdrop); }

void TopVocalChainEditor::resized()
{
    view.setBounds (0, 0, tvcui::MainView::W, tvcui::MainView::H);
    view.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) tvcui::MainView::W));
}

void TopVocalChainEditor::timerCallback() { view.tick(); }
