#include "PluginEditor.h"

using namespace tvcui;

//==============================================================================
// VISTA PRINCIPAL
//==============================================================================
namespace L
{
    const juce::Rectangle<int> unit1 { 12, 56, 1376, 430 };
    const juce::Rectangle<int> unit2 { 12, 496, 1376, 394 };
    const int u1 = 56, u2 = 496;
    const int pre = 56, comp = 316, eq = 676, out = 1076;     // unidad 1
    const int tune = 56, ai = 436, dly = 776, rev = 1106;     // unidad 2
}

MainView::MainView (TopVocalChainProcessor& p)
    : proc (p),
      compLed ([this] { return param ("comp_on"); }), eqLed ([this] { return param ("eq_on"); }),
      tuneLed ([this] { return param ("tune_on"); }), aiLed ([this] { return param ("ai_listen"); }),
      dlyLed ([this] { return param ("dly_on"); }), syncLed ([this] { return param ("dly_sync"); }),
      pingLed ([this] { return param ("dly_pingpong"); }), revLed ([this] { return param ("rev_on"); }),
      vu ([this] { return proc.meters.outLevelDb.load(); }, false),
      grMeter ([this] { return proc.meters.compGrDb.load(); }, true),
      drive (p.apvts, "pre_drive", "DRIVE", "0", "12", "+24"),
      inGain (p.apvts, "in_gain", "INPUT", "-24", "0", "+24"),
      warmth (p.apvts, "pre_warmth", "WARMTH", "0", "", "100"),
      trim (p.apvts, "pre_trim", "TRIM", "-12", "0", "+12"),
      thr (p.apvts, "comp_thr", "THRESHOLD", "-60", "-30", "0"),
      ratio (p.apvts, "comp_ratio", "RATIO", "1:1", "4:1", "20:1"),
      attack (p.apvts, "comp_att", "ATTACK", "0.1", "8ms", "100"),
      release (p.apvts, "comp_rel", "RELEASE", "10", "150ms", "1s"),
      makeup (p.apvts, "comp_makeup", "MAKEUP", "0", "", "+24"),
      air (p.apvts, "eq_air_gain", "AIR", "-12", "0", "+12"),
      presence (p.apvts, "eq_pres_gain", "PRESENCE", "-12", "0", "+12"),
      midQ (p.apvts, "eq_mid_q", "MID Q", "0.3", "1", "5"),
      midFreq (p.apvts, "eq_mid_freq", "MID FREQ", "200", "1k", "5k"),
      midGain (p.apvts, "eq_mid_gain", "MID", "-12", "0", "+12"),
      low (p.apvts, "eq_low_gain", "LOW", "-12", "0", "+12"),
      lowCut (p.apvts, "eq_lowcut", "LOW CUT", "20", "100", "500"),
      output (p.apvts, "out_gain", "OUTPUT", "-24", "", "+12"),
      tuneSpeed (p.apvts, "tune_speed", "SPEED", "0", "60ms", "400"),
      tuneAmount (p.apvts, "tune_amount", "AMOUNT", "0", "", "100"),
      aiAmount (p.apvts, "ai_amount", "INTELIGENCIA", "0", "50", "100"),
      dTime (p.apvts, "dly_time", "TIME", "10ms", "", "2s"),
      dFeedback (p.apvts, "dly_fb", "FEEDBACK", "0", "", "95"),
      dMix (p.apvts, "dly_mix", "MIX", "DRY", "", "WET"),
      dHiCut (p.apvts, "dly_hicut", "HI CUT", "1k", "5k", "20k"),
      rSize (p.apvts, "rev_size", "SIZE", "0", "", "100"),
      rDamp (p.apvts, "rev_damp", "DAMPING", "0", "", "100"),
      rWidth (p.apvts, "rev_width", "WIDTH", "0", "", "100"),
      rMix (p.apvts, "rev_mix", "MIX", "DRY", "", "WET")
{
    for (auto* k : { &drive, &inGain, &warmth, &trim, &thr, &ratio, &attack, &release, &makeup, &air, &presence,
                     &midQ, &midFreq, &midGain, &low, &lowCut, &output, &tuneSpeed, &tuneAmount, &aiAmount,
                     &dTime, &dFeedback, &dMix, &dHiCut, &rSize, &rDamp, &rWidth, &rMix })
        addAndMakeVisible (k);

    for (auto* l : { &compLed, &eqLed, &tuneLed, &aiLed, &dlyLed, &syncLed, &pingLed, &revLed })
    {
        addAndMakeVisible (l);
        l->setInterceptsMouseClicks (false, false);
    }
    addAndMakeVisible (vu);
    addAndMakeVisible (grMeter);

    auto attachBtn = [this] (juce::Button& b, const char* id)
    {
        addAndMakeVisible (b);
        buttonAtts.push_back (std::make_unique<APVTS::ButtonAttachment> (proc.apvts, id, b));
    };
    attachBtn (preOn, "pre_on");
    attachBtn (compOn, "comp_on");
    attachBtn (eqOn, "eq_on");
    attachBtn (tuneOn, "tune_on");
    attachBtn (aiListen, "ai_listen");
    attachBtn (dlyOn, "dly_on");
    attachBtn (dlySync, "dly_sync");
    attachBtn (dlyPing, "dly_pingpong");
    attachBtn (revOn, "rev_on");

    auto setupBox = [this] (juce::ComboBox& box, const juce::StringArray& items, const char* id,
                            std::unique_ptr<APVTS::ComboBoxAttachment>& att)
    {
        box.addItemList (items, 1);
        addAndMakeVisible (box);
        att = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, id, box);
    };
    setupBox (preType, TopVocalChainProcessor::preTypeNames(), "pre_type", preTypeAtt);
    setupBox (compType, TopVocalChainProcessor::compTypeNames(), "comp_type", compTypeAtt);
    setupBox (eqType, TopVocalChainProcessor::eqTypeNames(), "eq_type", eqTypeAtt);
    setupBox (keyBox, TopVocalChainProcessor::keyNames(), "tune_key", keyAtt);
    setupBox (scaleBox, TopVocalChainProcessor::scaleNames(), "tune_scale", scaleAtt);
    setupBox (divBox, TopVocalChainProcessor::divisionNames(), "dly_div", divAtt);

    aiReset.onClick = [this] { proc.requestAIReset(); };
    addAndMakeVisible (aiReset);

    // Presets
    for (auto* c : std::initializer_list<juce::Component*> { &presetBox, &prevBtn, &nextBtn, &saveBtn, &loadBtn })
        addAndMakeVisible (c);
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setTextWhenNothingSelected ("Preset");
    refreshPresetBox();
    presetBox.onChange = [this] { const int id = presetBox.getSelectedId(); if (id > 0) proc.setCurrentProgram (id - 1); };

    auto step = [this] (int dir)
    {
        const int num = proc.getNumPrograms();
        proc.setCurrentProgram ((proc.getCurrentProgram() + dir + num) % num);
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

    buildBackground();
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

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff15171c), 0, 0, Colours::rack, 0, (float) H, false));
    g.fillAll();
    brushed (g, { 0, 0, (float) W, 50 }, juce::Colour (0xff23262c), juce::Colour (0xff131418), 7, 0.6f);
    g.setColour (juce::Colours::black); g.fillRect (0, 49, W, 2);
    engraved (g, "TOP VOCAL CHAIN", { 22, 10, 320, 30 }, 20, juce::Justification::centredLeft, true);
    engraved (g, "PRESET", { 680, 10, 80, 30 }, 15, juce::Justification::centredRight, false, Colours::engraveDim);

    auto drawUnit = [&] (juce::Rectangle<int> u, int seed)
    {
        auto r = u.toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.fillRoundedRectangle (r.translated (0, 4).expanded (2), 6);
        g.saveState();
        juce::Path clip; clip.addRoundedRectangle (r, 5);
        g.reduceClipRegion (clip);
        brushed (g, r, Colours::metalTop, Colours::metalBot, seed);
        g.restoreState();
        g.setColour (juce::Colours::white.withAlpha (0.22f));
        g.drawLine (r.getX() + 5, r.getY() + 0.5f, r.getRight() - 5, r.getY() + 0.5f, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawRoundedRectangle (r, 5, 1.5f);
        for (float sy : { r.getY() + 30, r.getCentreY(), r.getBottom() - 30 })
        {
            drawScrew (g, r.getX() + 22, sy, 11.0f, 0.4f + sy * 0.01f);
            drawScrew (g, r.getRight() - 22, sy, 11.0f, 1.1f + sy * 0.013f);
        }
    };
    drawUnit (L::unit1, 101);
    drawUnit (L::unit2, 202);

    auto divider = [&] (int x, juce::Rectangle<int> u)
    {
        const float top = (float) u.getY() + 14, bot = (float) u.getBottom() - 14;
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRect ((float) x - 1.0f, top + 10, 1.5f, bot - top - 20);
        g.setColour (juce::Colours::white.withAlpha (0.16f));
        g.fillRect ((float) x + 0.5f, top + 10, 1.0f, bot - top - 20);
        drawScrew (g, (float) x, top, 5.5f);
        drawScrew (g, (float) x, bot, 5.5f);
    };
    for (int x : { L::comp, L::eq, L::out }) divider (x, L::unit1);
    for (int x : { L::ai, L::dly, L::rev }) divider (x, L::unit2);

    auto section = [&] (int x, int w, int y, const juce::String& t)
    {
        engraved (g, t, juce::Rectangle<int> (x, y + 12, w, 32).toFloat(), 23, juce::Justification::centred);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRect ((float) x + 26, (float) y + 47, (float) w - 52, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.28f));
        g.fillRect ((float) x + 26, (float) y + 48, (float) w - 52, 1.0f);
    };
    section (L::pre, 260, L::u1, "PREAMPLIFIER");
    section (L::comp, 360, L::u1, "COMPRESSOR");
    section (L::eq, 400, L::u1, "EQUALIZER");
    section (L::out, 268, L::u1, "OUTPUT");
    section (L::tune, 380, L::u2, "AUTOTUNE");
    section (L::ai, 340, L::u2, "IA VOICE");
    section (L::dly, 330, L::u2, "DELAY");
    section (L::rev, 238, L::u2, "REVERB");

    auto lbl = [&] (const juce::String& t, int x, int y, int w) { engraved (g, t, juce::Rectangle<int> (x, y, w, 20).toFloat(), 15); };
    lbl ("COMP IN", L::comp + 236, L::u1 + 92, 110);
    lbl ("EQ IN", L::eq + 316, L::u1 + 92, 80);
    lbl ("TUNE IN", L::tune + 292, L::u2 + 62, 88);
    lbl ("ESCUCHAR", L::ai + 4, L::u2 + 238, 100);
    lbl ("DELAY IN", L::dly + 236, L::u2 + 60, 90);
    lbl ("SYNC", L::dly + 236, L::u2 + 128, 90);
    lbl ("PING PONG", L::dly + 236, L::u2 + 268, 90);
    lbl ("REVERB IN", L::rev + 14, L::u2 + 62, 120);

    // Placa metalica con el nombre
    auto plate = juce::Rectangle<float> ((float) L::out + 20, (float) L::u1 + 342, 228, 74);
    g.setColour (juce::Colours::black.withAlpha (0.65f));
    g.fillRoundedRectangle (plate.translated (0, 3), 4);
    g.saveState();
    g.reduceClipRegion (plate.toNearestInt());
    brushed (g, plate, juce::Colour (0xffd9dbde), juce::Colour (0xff9fa3a9), 303, 1.6f);
    g.restoreState();
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (plate, 4, 1.4f);
    for (auto pt : { plate.getTopLeft() + juce::Point<float> (11, 11), plate.getTopRight() + juce::Point<float> (-11, 11),
                     plate.getBottomLeft() + juce::Point<float> (11, -11), plate.getBottomRight() + juce::Point<float> (-11, -11) })
        drawScrew (g, pt.x, pt.y, 5.0f);
    g.setColour (juce::Colour (0xff141414));
    g.setFont (serif (17.0f, true));
    g.drawText ("TOP VOCAL", plate.withTrimmedTop (6).withHeight (22), juce::Justification::centred);
    g.setFont (serif (34.0f, true));
    g.drawText ("CHAIN", plate.withTrimmedTop (24).withHeight (40), juce::Justification::centred);
}

//------------------------------------------------------------------------------
void MainView::resized()
{
    const int u1 = L::u1, u2 = L::u2;
    prevBtn.setBounds (768, 11, 30, 28);
    presetBox.setBounds (802, 11, 290, 28);
    nextBtn.setBounds (1096, 11, 30, 28);
    saveBtn.setBounds (1150, 11, 108, 28);
    loadBtn.setBounds (1264, 11, 108, 28);

    // PREAMP
    const int pr = L::pre;
    preType.setBounds (pr + 30, u1 + 58, 200, 26);
    drive.setBounds (pr + 40, u1 + 88, 180, 178);
    inGain.setBounds (pr + 2, u1 + 270, 86, 108);
    warmth.setBounds (pr + 87, u1 + 270, 86, 108);
    trim.setBounds (pr + 172, u1 + 270, 86, 108);
    preOn.setBounds (pr + 70, u1 + 386, 120, 30);

    // COMPRESSOR
    const int cp = L::comp;
    compType.setBounds (cp + 30, u1 + 58, 300, 26);
    thr.setBounds (cp + 20, u1 + 88, 200, 138);
    ratio.setBounds (cp + 45, u1 + 224, 150, 102);
    attack.setBounds (cp + 8, u1 + 322, 110, 102);
    release.setBounds (cp + 118, u1 + 322, 110, 102);
    compLed.setBounds (cp + 276, u1 + 114, 30, 30);
    compOn.setBounds (cp + 271, u1 + 148, 40, 52);
    makeup.setBounds (cp + 236, u1 + 206, 110, 104);
    grMeter.setBounds (cp + 236, u1 + 322, 112, 90);

    // EQUALIZER
    const int e = L::eq;
    eqType.setBounds (e + 24, u1 + 58, 282, 26);
    const int cw = 102, rh = 110;
    air.setBounds (e + 57, u1 + 88, cw, rh);
    presence.setBounds (e + 159, u1 + 88, cw, rh);
    midFreq.setBounds (e + 6, u1 + 200, cw, rh);
    midGain.setBounds (e + 108, u1 + 200, cw, rh);
    midQ.setBounds (e + 210, u1 + 200, cw, rh);
    low.setBounds (e + 57, u1 + 312, cw, rh);
    lowCut.setBounds (e + 159, u1 + 312, cw, rh);
    eqLed.setBounds (e + 341, u1 + 114, 30, 30);
    eqOn.setBounds (e + 336, u1 + 148, 40, 52);

    // OUTPUT
    const int o = L::out;
    vu.setBounds (o + 24, u1 + 62, 220, 120);
    output.setBounds (o + 44, u1 + 190, 180, 150);

    // AUTOTUNE
    const int t = L::tune;
    tuneDisplay = { t + 16, u2 + 60, 272, 92 };
    keyBox.setBounds (t + 16, u2 + 160, 78, 26);
    scaleBox.setBounds (t + 100, u2 + 160, 188, 26);
    tuneSpeed.setBounds (t + 16, u2 + 196, 136, 150);
    tuneAmount.setBounds (t + 152, u2 + 196, 136, 150);
    tuneLed.setBounds (t + 321, u2 + 86, 30, 30);
    tuneOn.setBounds (t + 316, u2 + 120, 40, 52);

    // IA VOICE
    const int a = L::ai;
    aiAmount.setBounds (a + 4, u2 + 58, 172, 176);
    aiLed.setBounds (a + 14, u2 + 268, 30, 30);
    aiListen.setBounds (a + 50, u2 + 258, 40, 52);
    aiReset.setBounds (a + 100, u2 + 268, 70, 28);
    aiDisplay = { a + 182, u2 + 60, 148, 316 };

    // DELAY
    const int d = L::dly;
    dTime.setBounds (d + 6, u2 + 60, 112, 148);
    dFeedback.setBounds (d + 118, u2 + 60, 112, 148);
    dMix.setBounds (d + 6, u2 + 214, 112, 148);
    dHiCut.setBounds (d + 118, u2 + 214, 112, 148);
    dlyLed.setBounds (d + 244, u2 + 84, 30, 30);
    dlyOn.setBounds (d + 278, u2 + 76, 38, 48);
    syncLed.setBounds (d + 244, u2 + 152, 30, 30);
    dlySync.setBounds (d + 278, u2 + 144, 38, 48);
    divBox.setBounds (d + 242, u2 + 206, 80, 26);
    pingLed.setBounds (d + 244, u2 + 292, 30, 30);
    dlyPing.setBounds (d + 278, u2 + 284, 38, 48);

    // REVERB
    const int rv = L::rev;
    revLed.setBounds (rv + 140, u2 + 57, 30, 30);
    revOn.setBounds (rv + 178, u2 + 48, 38, 48);
    rSize.setBounds (rv + 8, u2 + 100, 111, 136);
    rDamp.setBounds (rv + 119, u2 + 100, 111, 136);
    rWidth.setBounds (rv + 8, u2 + 240, 111, 136);
    rMix.setBounds (rv + 119, u2 + 240, 111, 136);
}

//------------------------------------------------------------------------------
void MainView::drawTuneDisplay (juce::Graphics& g)
{
    auto r = tuneDisplay.toFloat();
    lcdFrame (g, r);
    const float hz = proc.meters.pitchHz.load();
    const int note = (int) proc.meters.targetNote.load();
    const float cents = proc.meters.correctionCents.load();

    juce::String name = "--";
    if (note >= 0) name = TopVocalChainProcessor::keyNames()[note % 12] + juce::String (note / 12 - 1);
    glowText (g, name, r.withTrimmedLeft (14).withWidth (110).withHeight (60).translated (0, 4), sans (40.0f, true),
              juce::Justification::centredLeft, note >= 0 ? Colours::amber : Colours::amber.withAlpha (0.35f));

    auto info = r.withTrimmedLeft (120).reduced (10, 10);
    glowText (g, hz > 0 ? juce::String (hz, 1) + " Hz" : juce::String ("SIN VOZ"), info.removeFromTop (20), sans (14.0f, true), juce::Justification::centredRight);
    glowText (g, param ("tune_on") ? "CORR " + juce::String (juce::roundToInt (cents)) + " c" : juce::String ("BYPASS"),
              info.removeFromTop (20), sans (13.0f), juce::Justification::centredRight, Colours::amber.withAlpha (0.8f));

    auto bar = r.withTrimmedTop (r.getHeight() - 22).reduced (14, 6);
    g.setColour (Colours::amber.withAlpha (0.12f));
    g.fillRect (bar);
    const float c = juce::jlimit (-100.0f, 100.0f, cents) / 100.0f;
    const float mid = bar.getCentreX(), x2 = mid + c * bar.getWidth() * 0.5f;
    g.setColour (Colours::amber);
    g.fillRect (juce::jmin (mid, x2), bar.getY(), std::abs (x2 - mid) + 1.5f, bar.getHeight());
    g.setColour (Colours::amber.withAlpha (0.6f));
    for (int i = 0; i <= 10; ++i)
        g.fillRect (bar.getX() + bar.getWidth() * (float) i / 10.0f, bar.getY() - 3, 1.0f, i == 5 ? bar.getHeight() + 6 : 3.0f);
}

void MainView::drawAiDisplay (juce::Graphics& g)
{
    auto r = aiDisplay.toFloat();
    lcdFrame (g, r);
    auto t = r.reduced (8, 8);
    const auto& m = proc.meters;
    const float conf = m.aiConfidence.load(), avg = m.aiAvgPitch.load();

    glowText (g, param ("ai_listen") ? "ESCUCHANDO" : "CONGELADO", t.removeFromTop (16), sans (12.0f, true), juce::Justification::centredLeft);
    auto cb = t.removeFromTop (6);
    g.setColour (Colours::amber.withAlpha (0.15f)); g.fillRect (cb);
    g.setColour (Colours::amber); g.fillRect (cb.withWidth (cb.getWidth() * conf));
    t.removeFromTop (6);
    glowText (g, avg > 0 ? juce::String (avg < 165 ? "VOZ GRAVE " : avg < 300 ? "VOZ MEDIA " : "VOZ AGUDA ") + juce::String ((int) avg) + "Hz"
                         : juce::String ("VOZ --"),
              t.removeFromTop (18), sans (12.0f, true), juce::Justification::centredLeft);
    t.removeFromTop (6);

    auto row = [&] (const juce::String& label, float v, const juce::String& unit, int dec = 1)
    {
        auto rr = t.removeFromTop (30);
        glowText (g, label, rr.removeFromTop (13), sans (10.5f), juce::Justification::centredLeft, Colours::amber.withAlpha (0.65f));
        const auto num = dec == 0 ? juce::String (juce::roundToInt (v)) : juce::String (v, dec);
        glowText (g, (v > 0.05f ? "+" : "") + num + " " + unit, rr, sans (12.5f, true), juce::Justification::centredRight);
    };
    row ("GANANCIA", m.aiGain.load(), "dB");
    row ("LOW CUT", m.aiLowCut.load(), "Hz", 0);
    row ("BARRO 330 Hz", m.aiMud.load(), "dB");
    row ("PRESENCIA", m.aiPresence.load(), "dB");
    row ("AIRE", m.aiAir.load(), "dB");
    row ("DE-ESSER", m.aiDeEss.load(), "dB");
    row ("COMP THRESHOLD", m.aiCompThr.load(), "dB");
    row ("VEL. AUTOTUNE", m.aiTuneSpeed.load(), "ms", 0);
}

void MainView::paint (juce::Graphics& g)
{
    g.drawImageAt (background, 0, 0);
    drawTuneDisplay (g);
    drawAiDisplay (g);
}

void MainView::tick()
{
    for (auto* l : { &compLed, &eqLed, &tuneLed, &aiLed, &dlyLed, &syncLed, &pingLed, &revLed }) l->tick();
    vu.tick();
    grMeter.tick();
    repaint (tuneDisplay.expanded (6));
    repaint (aiDisplay.expanded (6));
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
    setResizeLimits (tvcui::MainView::W / 2, tvcui::MainView::H / 2, tvcui::MainView::W * 3 / 2, tvcui::MainView::H * 3 / 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) tvcui::MainView::W / tvcui::MainView::H);
    setSize (tvcui::MainView::W * 4 / 5, tvcui::MainView::H * 4 / 5);   // 1120 x 720 por defecto
    startTimerHz (30);
}

TopVocalChainEditor::~TopVocalChainEditor()
{
    stopTimer();
    view.setLookAndFeel (nullptr);
}

void TopVocalChainEditor::paint (juce::Graphics& g)
{
    g.fillAll (tvcui::Colours::rack);
}

void TopVocalChainEditor::resized()
{
    view.setBounds (0, 0, tvcui::MainView::W, tvcui::MainView::H);
    view.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) tvcui::MainView::W));
}

void TopVocalChainEditor::timerCallback()
{
    view.tick();
}
