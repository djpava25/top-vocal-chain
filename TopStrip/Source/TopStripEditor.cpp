#include "TopStripEditor.h"

using namespace tvcui;
using namespace topstripui;

namespace
{
    const juce::Rectangle<int> unit { 12, 58, 1476, 430 };
    const int uy = 58;
    const int secPre = 50, secComp = 350, secVu = 650, secEq = 890, secOut = 1330;
    const int rowA = uy + 336, rowB = uy + 374;
}

MainView::MainView (TopStripProcessor& p)
    : proc (p),
      preGain (p.apvts, "pre_gain", "PREAMP GAIN", "-20", "+5", "+30"),
      hpf (p.apvts, "hpf", "HIGH PASS", "30", "", "140"),
      thr (p.apvts, "thr", "THRESHOLD", "-40", "-20", "0"),
      ratio (p.apvts, "ratio", "COMPRESSION", "1", "4", "20"),
      att (p.apvts, "att", "ATTACK", "FAST", "", "SLOW"),
      rel (p.apvts, "rel", "RELEASE", "FAST", "", "SLOW"),
      vuLevel ([&p] { return p.outLevelDb.load(); }, false),
      vuGr ([&p] { return p.grDb.load(); }, true),
      bassG (p.apvts, "bass_gain", "BASS", "-24", "0", "+24"),
      bassF (p.apvts, "bass_freq", "FREQ", "15", "", "150"),
      lmG (p.apvts, "lm_gain", "LOW MID", "-16", "0", "+16"),
      lmF (p.apvts, "lm_freq", "FREQ", "35", "150", "450"),
      hmG (p.apvts, "hm_gain", "HIGH MID", "-16", "0", "+16"),
      hmF (p.apvts, "hm_freq", "FREQ", "220", "1k2", "2k8"),
      trG (p.apvts, "tr_gain", "TREBLE", "-24", "0", "+24"),
      trF (p.apvts, "tr_freq", "FREQ", "10k", "", "32k"),
      out (p.apvts, "out", "OUTPUT", "-24", "0", "+12")
{
    for (auto* k : { &preGain, &hpf, &thr, &ratio, &att, &rel, &bassG, &bassF, &lmG, &lmF, &hmG, &hmF, &trG, &trF, &out })
        addAndMakeVisible (k);
    addAndMakeVisible (vuLevel);
    addChildComponent (vuGr);

    lamp (mic, "mic", "MIC: mas color de valvula al mismo volumen");
    lamp (phase, "phase", "Invierte la fase (polaridad)");
    lamp (pad, "pad", "Baja la entrada 20 dB");
    lamp (hpfIn, "hpf_on", "Activa el filtro pasa altos (quita graves y ruido de golpes)");
    lamp (compIn, "comp_on", "Activa el compresor optico");
    lamp (eqPre, "eq_pre", "El EQ va ANTES del compresor");
    lamp (scLink, "sc_link", "De-esser: el compresor escucha la banda HIGH MID");
    lamp (meterGr, "meter_gr", "El VU muestra la reduccion de ganancia del compresor");
    lamp (eqIn, "eq_on", "Activa el ecualizador");
    lamp (lmHiQ, "lm_hiq", "Banda mas estrecha");
    lamp (lmX10, "lm_x10", "Multiplica la frecuencia x10");
    lamp (hmHiQ, "hm_hiq", "Banda mas estrecha");
    lamp (hmX10, "hm_x10", "Multiplica la frecuencia x10");
    addAndMakeVisible (power);
    atts.push_back (std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "power", power));
    power.setTooltip ("Encendido / bypass");

    for (auto* c : std::initializer_list<juce::Component*> { &presetBox, &prevBtn, &nextBtn }) addAndMakeVisible (c);
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

    buildBackground();
}

void MainView::lamp (LampButton& b, const char* id, const juce::String& tip)
{
    addAndMakeVisible (b);
    b.setTooltip (tip);
    atts.push_back (std::make_unique<APVTS::ButtonAttachment> (proc.apvts, id, b));
}

void MainView::buildBackground()
{
    background = juce::Image (juce::Image::ARGB, W, H, true);
    juce::Graphics g (background);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff15171c), 0, 0, Colours::rack, 0, (float) H, false));
    g.fillAll();
    brushed (g, { 0, 0, (float) W, 50 }, juce::Colour (0xff23262c), juce::Colour (0xff131418), 17, 0.6f);
    g.setColour (juce::Colours::black); g.fillRect (0, 49, W, 2);
    engraved (g, "TOP STRIP 737", { 22, 10, 240, 30 }, 21, juce::Justification::centredLeft, true);
    engraved (g, "TUBE CHANNEL STRIP  -  PREAMP  /  OPTO COMPRESSOR  /  4-BAND EQ", { 240, 10, 560, 30 }, 13,
              juce::Justification::centredLeft, false, Colours::engraveDim);
    engraved (g, "PRESET", { 960, 10, 80, 30 }, 15, juce::Justification::centredRight, false, Colours::engraveDim);

    // Unidad de rack (grafito anodizado)
    auto r = unit.toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.fillRoundedRectangle (r.translated (0, 4).expanded (2), 6);
    g.saveState();
    juce::Path clip; clip.addRoundedRectangle (r, 5);
    g.reduceClipRegion (clip);
    brushed (g, r, juce::Colour (0xff3d424b), juce::Colour (0xff22262d), 737);
    // franja de acento ambar inferior
    g.setColour (Colours::amber.withAlpha (0.55f));
    g.fillRect (r.getX(), r.getBottom() - 6, r.getWidth(), 2.0f);
    g.restoreState();
    g.setColour (juce::Colours::white.withAlpha (0.22f));
    g.drawLine (r.getX() + 5, r.getY() + 0.5f, r.getRight() - 5, r.getY() + 0.5f, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.drawRoundedRectangle (r, 5, 1.5f);
    for (float sy : { r.getY() + 30, r.getBottom() - 30 })
    {
        drawScrew (g, r.getX() + 20, sy, 10.0f, 0.4f + sy * 0.01f);
        drawScrew (g, r.getRight() - 20, sy, 10.0f, 1.1f + sy * 0.013f);
    }

    // Separadores de seccion
    for (int x : { secComp, secVu, secEq, secOut })
    {
        const float top = r.getY() + 14, bot = r.getBottom() - 16;
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRect ((float) x - 1.0f, top + 10, 1.5f, bot - top - 20);
        g.setColour (juce::Colours::white.withAlpha (0.14f));
        g.fillRect ((float) x + 0.5f, top + 10, 1.0f, bot - top - 20);
    }

    auto section = [&] (int x, int w, const juce::String& t)
    {
        engraved (g, t, juce::Rectangle<int> (x, uy + 10, w, 28).toFloat(), 19, juce::Justification::centred, true);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRect ((float) x + 20, (float) uy + 41, (float) w - 40, 1.0f);
        g.setColour (Colours::amber.withAlpha (0.45f));
        g.fillRect ((float) x + 20, (float) uy + 42, (float) w - 40, 1.0f);
    };
    section (secPre, secComp - secPre, "TUBE PREAMP");
    section (secComp, secVu - secComp, "OPTO COMPRESSOR");
    section (secEq, secOut - secEq, "EQUALIZER");
    section (secOut, 120, "OUTPUT");

    auto lbl = [&] (const juce::String& t, int x, int y, int w, float size = 13.0f, juce::Colour c = Colours::engraveDim)
    {
        engraved (g, t, juce::Rectangle<int> (x, y, w, 18).toFloat(), size, juce::Justification::centred, false, c);
    };
    lbl ("LINE / MIC  -  CLASS A", secPre + 20, uy + 300, 260);
    lbl ("COMP  -  SIDECHAIN", secComp + 20, uy + 310, 260);
    lbl ("SHELF", secEq, uy + 300, 110);
    lbl ("PEAK", secEq + 110, uy + 300, 110);
    lbl ("PEAK", secEq + 220, uy + 300, 110);
    lbl ("SHELF", secEq + 330, uy + 300, 110);
    lbl ("POWER", secOut, uy + 296, 120, 14, Colours::engrave);

    // Placa central con el nombre
    auto plate = juce::Rectangle<float> ((float) secVu + 22, (float) uy + 206, 196, 84);
    g.setColour (juce::Colours::black.withAlpha (0.65f));
    g.fillRoundedRectangle (plate.translated (0, 3), 4);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1a1b1f), plate.getX(), plate.getY(), juce::Colour (0xff0b0b0d), plate.getX(), plate.getBottom(), false));
    g.fillRoundedRectangle (plate, 4);
    g.setColour (Colours::amber.withAlpha (0.6f));
    g.drawRoundedRectangle (plate.reduced (3), 3, 1.0f);
    glowText (g, "TOP STRIP", plate.withTrimmedTop (8).withHeight (24), serif (19.0f, true), juce::Justification::centred);
    glowText (g, "737", plate.withTrimmedTop (30).withHeight (36), serif (34.0f, true), juce::Justification::centred);
    g.setFont (sans (10.5f));
    g.setColour (Colours::engraveDim);
    g.drawText ("TUBE CHANNEL STRIP", plate.withTrimmedTop (66).withHeight (14), juce::Justification::centred);

    // Ventana de la valvula
    auto win = juce::Rectangle<float> ((float) secVu + 70, (float) uy + 306, 100, 100);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.fillRoundedRectangle (win.translated (0, 3), 8);
    g.setColour (juce::Colour (0xff09090b));
    g.fillRoundedRectangle (win, 8);
    g.setColour (juce::Colours::white.withAlpha (0.15f));
    g.drawRoundedRectangle (win, 8, 1.0f);
    for (auto pt : { win.getTopLeft() + juce::Point<float> (9, 9), win.getTopRight() + juce::Point<float> (-9, 9),
                     win.getBottomLeft() + juce::Point<float> (9, -9), win.getBottomRight() + juce::Point<float> (-9, -9) })
        drawScrew (g, pt.x, pt.y, 4.0f);

    // Placa de salida
    auto op = juce::Rectangle<float> ((float) secOut + 14, (float) uy + 370, 92, 40);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (op.translated (0, 2), 3);
    g.saveState();
    g.reduceClipRegion (op.toNearestInt());
    brushed (g, op, juce::Colour (0xffd9dbde), juce::Colour (0xff9fa3a9), 909, 1.6f);
    g.restoreState();
    g.setColour (juce::Colour (0xff141414));
    g.setFont (serif (17.0f, true));
    g.drawText ("TOP 737", op, juce::Justification::centred);
}

void MainView::resized()
{
    prevBtn.setBounds (1046, 11, 30, 28);
    presetBox.setBounds (1080, 11, 340, 28);
    nextBtn.setBounds (1424, 11, 30, 28);

    // PREAMP
    preGain.setBounds (secPre + 10, uy + 52, 170, 196);
    hpf.setBounds (secPre + 176, uy + 82, 120, 150);
    mic.setBounds (secPre + 16, rowA, 130, 32);
    phase.setBounds (secPre + 154, rowA, 130, 32);
    pad.setBounds (secPre + 16, rowB, 130, 32);
    hpfIn.setBounds (secPre + 154, rowB, 130, 32);

    // COMPRESOR
    thr.setBounds (secComp + 8, uy + 50, 142, 150);
    ratio.setBounds (secComp + 150, uy + 50, 142, 150);
    att.setBounds (secComp + 26, uy + 196, 112, 112);
    rel.setBounds (secComp + 162, uy + 196, 112, 112);
    compIn.setBounds (secComp + 16, rowA, 130, 32);
    eqPre.setBounds (secComp + 154, rowA, 130, 32);
    scLink.setBounds (secComp + 16, rowB, 130, 32);
    meterGr.setBounds (secComp + 154, rowB, 130, 32);

    // VU
    vuLevel.setBounds (secVu + 12, uy + 56, 216, 136);
    vuGr.setBounds (vuLevel.getBounds());

    // EQ
    int i = 0;
    for (auto [gk, fk] : { std::pair<HwKnob*, HwKnob*> { &bassG, &bassF }, { &lmG, &lmF }, { &hmG, &hmF }, { &trG, &trF } })
    {
        const int x = secEq + i * 110;
        gk->setBounds (x + 3, uy + 50, 104, 126);
        fk->setBounds (x + 11, uy + 182, 88, 112);
        ++i;
    }
    eqIn.setBounds (secEq + 10, rowA + 19, 90, 32);
    lmHiQ.setBounds (secEq + 120, rowA, 90, 32);
    lmX10.setBounds (secEq + 120, rowB, 90, 32);
    hmHiQ.setBounds (secEq + 230, rowA, 90, 32);
    hmX10.setBounds (secEq + 230, rowB, 90, 32);

    // SALIDA
    out.setBounds (secOut + 4, uy + 54, 112, 150);
    power.setBounds (secOut + 40, uy + 218, 40, 74);
}

void MainView::paint (juce::Graphics& g)
{
    g.drawImageAt (background, 0, 0);

    // brillo de la valvula (se apaga con POWER)
    auto win = juce::Rectangle<float> ((float) secVu + 70, (float) uy + 306, 100, 100);
    const auto c = win.getCentre();
    if (glow > 0.01f)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffc46b).withAlpha (0.95f * glow), c.x, c.y,
                                                 juce::Colour (0xffff5a1a).withAlpha (0.0f), c.x + 40, c.y, true));
        g.fillEllipse (win.reduced (12));
    }
    // filamento
    g.setColour (juce::Colour (0xff3a2a1c).interpolatedWith (juce::Colour (0xfffff0c8), glow));
    juce::Path fil;
    fil.startNewSubPath (c.x - 14, c.y + 16);
    fil.lineTo (c.x - 7, c.y - 14); fil.lineTo (c.x, c.y + 12); fil.lineTo (c.x + 7, c.y - 14); fil.lineTo (c.x + 14, c.y + 16);
    g.strokePath (fil, juce::PathStrokeType (1.6f));
}

void MainView::tick()
{
    const bool gr = param ("meter_gr");
    if (vuGr.isVisible() != gr) { vuGr.setVisible (gr); vuLevel.setVisible (! gr); }
    (gr ? vuGr : vuLevel).tick();

    const float target = param ("power") ? 1.0f : 0.0f;
    if (std::abs (glow - target) > 0.005f)
    {
        glow += (target - glow) * 0.12f;
        repaint (secVu + 60, uy + 296, 120, 120);
    }
    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
}

//==============================================================================
TopStripEditor::TopStripEditor (TopStripProcessor& p) : AudioProcessorEditor (&p), view (p)
{
    view.setLookAndFeel (&lnf);
    addAndMakeVisible (view);
    setResizable (true, true);
    setResizeLimits (MainView::W / 2, MainView::H / 2, MainView::W * 3 / 2, MainView::H * 3 / 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) MainView::W / MainView::H);
    setSize (MainView::W * 4 / 5, MainView::H * 4 / 5);
    startTimerHz (30);
}

TopStripEditor::~TopStripEditor()
{
    stopTimer();
    view.setLookAndFeel (nullptr);
}

void TopStripEditor::paint (juce::Graphics& g) { g.fillAll (Colours::rack); }

void TopStripEditor::resized()
{
    view.setBounds (0, 0, MainView::W, MainView::H);
    view.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) MainView::W));
}
