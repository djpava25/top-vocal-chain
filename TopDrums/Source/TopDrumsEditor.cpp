#include "TopDrumsEditor.h"

using namespace tvcui;
using namespace topdrumsui;

namespace
{
    const juce::Rectangle<int> unit { 12, 58, 1476, 404 }, aiStrip { 12, 472, 1476, 178 };
    const int uy = 58;
    const int secTr = 40, secCol = 400, secGlue = 680, secOut = 1200, secEnd = 1460;
    const int ay = 472;
    const juce::Rectangle<int> localBox { 272, ay + 14, 560, 150 }, claudeBox { 848, ay + 14, 436, 150 };
    const juce::Rectangle<int> transBar { secTr + 30, uy + 372, 300, 12 };
    juce::String U (const char* s) { return juce::String::fromUTF8 (s); }
}

MainView::MainView (TopDrumsProcessor& p)
    : proc (p),
      attack (p.apvts, "ts_attack", "ATTACK", "-100", "0", "+100"),
      sustain (p.apvts, "ts_sustain", "SUSTAIN", "-100", "0", "+100"),
      speed (p.apvts, "ts_speed", "SPEED", "FAST", "", "SLOW"),
      weight (p.apvts, "weight", "WEIGHT", "0", "", "100"),
      drive (p.apvts, "drive", "DRIVE", "0", "", "100"),
      tone (p.apvts, "tone", "TONE", "DARK", "0", "BRIGHT"),
      thr (p.apvts, "glue_thr", "THRESHOLD", "-40", "-20", "0"),
      ratio (p.apvts, "glue_ratio", "RATIO", "1.5", "", "10"),
      gAtt (p.apvts, "glue_att", "ATTACK", "0.1", "", "30"),
      gRel (p.apvts, "glue_rel", "RELEASE", "0.1", "", "AUTO"),
      makeup (p.apvts, "glue_makeup", "MAKEUP", "0", "", "+15"),
      gMix (p.apvts, "glue_mix", "GLUE MIX", "DRY", "", "WET"),
      scHpf (p.apvts, "glue_sc", "SC HPF", "OFF", "", "150"),
      out (p.apvts, "out", "OUTPUT", "-24", "0", "+12"),
      mix (p.apvts, "mix", "MIX", "DRY", "", "WET"),
      vuGr ([&p] { return p.grDb.load(); }, true),
      vuOut ([&p] { return p.outLevelDb.load(); }, false)
{
    for (auto* k : { &attack, &sustain, &speed, &weight, &drive, &tone, &thr, &ratio, &gAtt, &gRel, &makeup, &gMix, &scHpf, &out, &mix })
        addAndMakeVisible (k);
    attack.slider.setTooltip (U ("ATTACK\nMás = el golpe del kick y el snare corta más · Menos = golpe más suave"));
    sustain.slider.setTooltip (U ("SUSTAIN\nMás = la cola y la sala suenan más · Menos = drums secos y cortos"));
    speed.slider.setTooltip (U ("SPEED\nQué tan rápido reacciona el transient shaper"));
    weight.slider.setTooltip (U ("WEIGHT\nPeso: más sub y armónicos del kick/808 (se oyen hasta en el celular)"));
    drive.slider.setTooltip (U ("DRIVE\nSaturación suave tipo cinta: más densidad y agresividad"));
    tone.slider.setTooltip (U ("TONE\nInclina el sonido: izquierda más oscuro, derecha más brillante"));
    thr.slider.setTooltip (U ("GLUE THRESHOLD\nMás bajo = más compresión = drums más pegados"));
    gRel.slider.setTooltip (U ("GLUE RELEASE\nAUTO se adapta solo: rápido en golpes sueltos, lento si comprime seguido"));
    gMix.slider.setTooltip (U ("GLUE MIX\nCompresión en paralelo: mezcla drums comprimidos con los originales"));
    scHpf.slider.setTooltip (U ("SC HPF\nEl compresor ignora los graves: el kick/808 no hace bombear el glue"));
    mix.slider.setTooltip (U ("MIX\nMezcla todo el plugin con la señal original"));
    addAndMakeVisible (vuGr);
    addAndMakeVisible (vuOut);
    lamp (clip, "clip", U ("CLIP\nRecorta los picos suave, como los drums pesados del reggaetón moderno"));
    lamp (glueIn, "glue_on", U ("GLUE IN\nActiva el compresor de bus"));
    addAndMakeVisible (power);
    atts.push_back (std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "power", power));
    power.setTooltip ("Encendido / bypass");

    focusBox.addItemList (TopDrumsProcessor::focusNames(), 1);
    addAndMakeVisible (focusBox);
    boxAtts.push_back (std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, "ts_focus", focusBox));
    focusBox.setTooltip (U ("FOCUS\nA qué parte de la batería reacciona: todo, solo kick, solo snare o solo hats"));
    genreBox.addItemList (TopDrumsProcessor::genreNames(), 1);
    addAndMakeVisible (genreBox);
    boxAtts.push_back (std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, "genre", genreBox));
    genreBox.setTooltip (U ("Género objetivo para la IA"));

    for (auto* b : { &analyse, &applyLocal, &askBtn, &applyClaude, &keyBtn }) addAndMakeVisible (b);
    analyse.onClick = [this] { proc.startAnalysis(); aiText = U ("Escuchando tu batería... reproduce los drums 8 segundos."); };
    applyLocal.onClick = [this] { proc.applySettings (proc.getAnalysis().settings); };
    askBtn.onClick = [this] { proc.askClaude(); };
    applyClaude.onClick = [this] { proc.applySettings (proc.getClaudeSettings()); };
    keyBtn.onClick = [this] { askForKey(); };
    analyse.setTooltip (U ("La IA local escucha 8 s de tu batería y la compara con el género elegido (gratis, sin internet)"));
    askBtn.setTooltip (U ("Envía las medidas y tus ajustes a Claude por internet y te devuelve consejos (necesita tu clave de API de Anthropic)"));
    keyBtn.setTooltip (U ("Pega tu clave de API de Anthropic (console.anthropic.com). Se guarda solo en este ordenador."));
    applyLocal.setEnabled (false);
    applyClaude.setEnabled (false);
    aiText = U ("Pulsa ANALIZAR DRUMS y reproduce tu batería.");

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

void MainView::askForKey()
{
    auto* w = new juce::AlertWindow (U ("Clave de API de Anthropic"),
                                     U ("Crea una clave en console.anthropic.com (se paga aparte, por uso) y pégala aquí.\nSe guarda solo en este ordenador."),
                                     juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("key", TopDrumsProcessor::loadApiKey(), "sk-ant-...", true);
    w->addButton ("GUARDAR", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("CANCELAR", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([w] (int r)
    {
        if (r == 1) TopDrumsProcessor::saveApiKey (w->getTextEditorContents ("key"));
    }), true);
}

void MainView::buildBackground()
{
    background = juce::Image (juce::Image::ARGB, W, H, true);
    juce::Graphics g (background);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff15171c), 0, 0, Colours::rack, 0, (float) H, false));
    g.fillAll();
    brushed (g, { 0, 0, (float) W, 50 }, juce::Colour (0xff23262c), juce::Colour (0xff131418), 31, 0.6f);
    g.setColour (juce::Colours::black); g.fillRect (0, 49, W, 2);
    engraved (g, "TOP DRUMS", { 22, 10, 200, 30 }, 21, juce::Justification::centredLeft, true);
    engraved (g, "TRANSIENT SHAPER  -  COLOR  -  GLUE  -  AI MIX ASSISTANT", { 180, 10, 520, 30 }, 13,
              juce::Justification::centredLeft, false, Colours::engraveDim);
    engraved (g, "PRESET", { 960, 10, 80, 30 }, 15, juce::Justification::centredRight, false, Colours::engraveDim);

    for (auto rr : { unit, aiStrip })
    {
        auto r = rr.toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.fillRoundedRectangle (r.translated (0, 4).expanded (2), 6);
        g.saveState();
        juce::Path clipP; clipP.addRoundedRectangle (r, 5);
        g.reduceClipRegion (clipP);
        brushed (g, r, rr == unit ? juce::Colour (0xff3a3f48) : juce::Colour (0xff2a2e35), juce::Colour (0xff1f2329), rr == unit ? 515 : 77);
        g.restoreState();
        g.setColour (juce::Colours::white.withAlpha (0.2f));
        g.drawLine (r.getX() + 5, r.getY() + 0.5f, r.getRight() - 5, r.getY() + 0.5f, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawRoundedRectangle (r, 5, 1.5f);
        for (float sy : { r.getY() + 22, r.getBottom() - 22 })
        {
            drawScrew (g, r.getX() + 16, sy, 8.0f, 0.4f + sy * 0.01f);
            drawScrew (g, r.getRight() - 16, sy, 8.0f, 1.1f + sy * 0.013f);
        }
    }
    // franja roja de acento
    g.setColour (juce::Colour (0xffe0442e).withAlpha (0.6f));
    g.fillRect ((float) unit.getX(), (float) unit.getBottom() - 6, (float) unit.getWidth(), 2.0f);

    for (int x : { secCol, secGlue, secOut })
    {
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRect ((float) x - 1.0f, (float) uy + 24, 1.5f, 360.0f);
        g.setColour (juce::Colours::white.withAlpha (0.14f));
        g.fillRect ((float) x + 0.5f, (float) uy + 24, 1.0f, 360.0f);
    }
    auto section = [&] (int x, int w, const juce::String& t)
    {
        engraved (g, t, juce::Rectangle<int> (x, uy + 10, w, 28).toFloat(), 19, juce::Justification::centred, true);
        g.setColour (juce::Colour (0xffe0442e).withAlpha (0.55f));
        g.fillRect ((float) x + 20, (float) uy + 42, (float) w - 40, 1.0f);
    };
    section (secTr, secCol - secTr, "TRANSIENT");
    section (secCol, secGlue - secCol, "COLOR");
    section (secGlue, secOut - secGlue, "GLUE");
    section (secOut, secEnd - secOut, "OUTPUT");
    auto lbl = [&] (const juce::String& t, int x, int y, int w, float size = 13.0f)
    { engraved (g, t, juce::Rectangle<int> (x, y, w, 18).toFloat(), size, juce::Justification::centred, false, Colours::engraveDim); };
    lbl ("FOCUS", secTr + 210, uy + 262, 150);
    lbl ("TRANSIENTES EN VIVO", secTr + 30, uy + 352, 300, 11);
    lbl ("POWER", secOut + 160, uy + 386 - 14, 80, 13);

    // franja IA
    engraved (g, "AI MIX ASSISTANT", { 40.0f, (float) ay + 10, 230.0f, 22.0f }, 15, juce::Justification::centredLeft, true);
    engraved (g, U ("GÉNERO"), { 40.0f, (float) ay + 34, 230.0f, 14.0f }, 11, juce::Justification::centredLeft, false, Colours::engraveDim);
}

void MainView::resized()
{
    prevBtn.setBounds (1046, 11, 30, 28);
    presetBox.setBounds (1080, 11, 340, 28);
    nextBtn.setBounds (1424, 11, 30, 28);

    attack.setBounds (secTr + 20, uy + 50, 160, 180);
    sustain.setBounds (secTr + 180, uy + 50, 160, 180);
    speed.setBounds (secTr + 40, uy + 230, 120, 120);
    focusBox.setBounds (secTr + 215, uy + 282, 140, 28);

    weight.setBounds (secCol + 10, uy + 50, 130, 150);
    drive.setBounds (secCol + 140, uy + 50, 130, 150);
    tone.setBounds (secCol + 75, uy + 200, 130, 140);
    clip.setBounds (secCol + 80, uy + 346, 120, 30);

    vuGr.setBounds (secGlue + 20, uy + 52, 230, 122);
    glueIn.setBounds (secGlue + 290, uy + 64, 170, 32);
    scHpf.setBounds (secGlue + 325, uy + 102, 100, 92);
    const int gx = secGlue + 10, gw = 84;
    HwKnob* gk[] { &thr, &ratio, &gAtt, &gRel, &makeup, &gMix };
    for (int i = 0; i < 6; ++i) gk[i]->setBounds (gx + i * gw, uy + 214, gw, 118);

    vuOut.setBounds (secOut + 15, uy + 52, 230, 122);
    out.setBounds (secOut + 10, uy + 186, 120, 140);
    mix.setBounds (secOut + 130, uy + 186, 120, 140);
    power.setBounds (secOut + 180, uy + 326, 40, 50);

    genreBox.setBounds (40, ay + 50, 222, 28);
    analyse.setBounds (40, ay + 86, 222, 32);
    applyLocal.setBounds (40, ay + 132, 222, 30);
    askBtn.setBounds (1296, ay + 20, 162, 36);
    applyClaude.setBounds (1296, ay + 64, 162, 30);
    keyBtn.setBounds (1296, ay + 102, 162, 28);
}

void MainView::paint (juce::Graphics& g)
{
    g.drawImageAt (background, 0, 0);

    // barra de transientes en vivo
    auto tb = transBar.toFloat();
    lcdFrame (g, tb.expanded (3));
    g.setColour (Colours::amberDim); g.fillRect (tb);
    g.setColour (Colours::amber); g.fillRect (tb.withWidth (tb.getWidth() * juce::jlimit (0.0f, 1.0f, transShown / 18.0f)));

    // progreso del analisis
    if (proc.isAnalysing())
    {
        auto pb = juce::Rectangle<float> (40, (float) ay + 122, 222, 6);
        g.setColour (Colours::amberDim); g.fillRect (pb);
        g.setColour (Colours::amber); g.fillRect (pb.withWidth (pb.getWidth() * proc.analysisProgress()));
    }

    auto box = [&] (juce::Rectangle<int> r, const juce::String& title, const juce::String& text)
    {
        auto f = r.toFloat();
        lcdFrame (g, f);
        auto t = f.reduced (12, 8);
        glowText (g, title, t.removeFromTop (18), sans (13.0f, true), juce::Justification::centredLeft);
        g.setFont (sans (12.5f));
        g.setColour (Colours::amber.withAlpha (0.9f));
        g.drawFittedText (text, t.toNearestInt(), juce::Justification::topLeft, 10, 0.85f);
    };
    box (localBox, "IA LOCAL (GRATIS)", aiText);
    juce::String ct = claudeTextShown;
    if (proc.getAskState() == TopDrumsProcessor::AskState::running) ct = U ("Preguntando a Claude...");
    if (ct.isEmpty()) ct = U ("Opcional: pulsa CLAVE API una vez y luego PREGUNTAR A CLAUDE.\nConviene ANALIZAR DRUMS primero.");
    box (claudeBox, "CLAUDE (ONLINE)", ct);
}

void MainView::tick()
{
    vuGr.tick(); vuOut.tick();
    const float t = proc.transientDb.load();
    transShown = t > transShown ? t : transShown * 0.85f + t * 0.15f;

    const bool ready = proc.analysisReady();
    if (ready && ! lastReady)
    {
        const auto a = proc.getAnalysis();
        juce::StringArray lines;
        if (a.valid)
            lines.add (U ("Pegada ") + juce::String (a.crestDb, 1) + U (" dB · Graves ") + juce::String (a.lowDb, 1) + U (" dB · Agudos ")
                       + juce::String (a.highDb, 1) + U (" dB · ") + juce::String (a.densityPerSec, 1) + " golpes/s");
        for (auto& f : a.findings) lines.add (f);
        for (auto& r : a.recommendations) lines.add (U ("→ ") + r);
        aiText = lines.joinIntoString ("\n");
        applyLocal.setEnabled (a.valid && ! a.settings.empty());
    }
    lastReady = ready;

    const auto st = proc.getAskState();
    if (st != lastAsk)
    {
        lastAsk = st;
        if (st == TopDrumsProcessor::AskState::done || st == TopDrumsProcessor::AskState::error) claudeTextShown = proc.getClaudeText();
        applyClaude.setEnabled (st == TopDrumsProcessor::AskState::done && ! proc.getClaudeSettings().empty());
        askBtn.setEnabled (st != TopDrumsProcessor::AskState::running);
    }
    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    repaint (transBar.expanded (6));
    repaint (aiStrip);
}

//==============================================================================
TopDrumsEditor::TopDrumsEditor (TopDrumsProcessor& p) : AudioProcessorEditor (&p), view (p)
{
    view.setLookAndFeel (&lnf);
    tooltips.setLookAndFeel (&lnf);
    addAndMakeVisible (view);
    setResizable (true, true);
    setResizeLimits (MainView::W / 2, MainView::H / 2, MainView::W * 3 / 2, MainView::H * 3 / 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) MainView::W / MainView::H);
    setSize (MainView::W * 4 / 5, MainView::H * 4 / 5);
    startTimerHz (30);
}

TopDrumsEditor::~TopDrumsEditor()
{
    stopTimer();
    tooltips.setLookAndFeel (nullptr);
    view.setLookAndFeel (nullptr);
}

void TopDrumsEditor::paint (juce::Graphics& g) { g.fillAll (Colours::rack); }

void TopDrumsEditor::resized()
{
    view.setBounds (0, 0, MainView::W, MainView::H);
    view.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) MainView::W));
}
