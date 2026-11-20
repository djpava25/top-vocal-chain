#include "TopCompEditor.h"

using namespace tvcui;
using namespace topcompui;

namespace
{
    const juce::Rectangle<int> unit { 10, 46, 1180, 344 };
    const int uy = 46;
    juce::String U (const char* s) { return juce::String::fromUTF8 (s); }
    const juce::Colour white (0xfff2f4f8), whiteDim (0xffc9d4e6);
}

MainView::MainView (TopCompProcessor& p)
    : proc (p),
      gain (p.apvts, "gain", "GAIN", "-20", "+5", "+30"),
      ratio (p.apvts, "ratio", "RATIO", "2:1", "4:1", "10:1"),
      thr (p.apvts, "thr", "THRESHOLD", "-40", "-20", "0"),
      mix (p.apvts, "mix", "DRY / WET", "DRY", "", "WET"),
      attack (p.apvts, "attack", "ATTACK", "FAST", "", "SLOW"),
      release (p.apvts, "release", "RELEASE", "FAST", "", "SLOW"),
      select (p.apvts, "select", "ATTACK / RELEASE", "FIXED", "FIX/MAN", "MANUAL"),
      hpf (p.apvts, "hpf", "SIDECHAIN HPF", "OFF", "", "500"),
      color (p.apvts, "color", "COLOR", "0", "100", "200"),
      vuGr ([&p] { return p.grDb.load(); }, true),
      vuOut ([&p] { return p.outLevelDb.load(); }, false)
{
    // THRESHOLD como en el hardware: a la derecha = threshold mas bajo = mas compresion
    thr.slider.setRotaryParameters (juce::MathConstants<float>::pi * 2.78f, juce::MathConstants<float>::pi * 1.22f, true);
    for (auto* k : { &gain, &ratio, &thr, &mix, &attack, &release, &select, &hpf, &color }) addAndMakeVisible (k);
    gain.slider.setTooltip (U ("GAIN: volumen de salida. Subirlo empuja el transformador de salida (más armónicos)"));
    ratio.slider.setTooltip (U ("RATIO: cuánto comprime por encima del threshold (2:1 suave … 10:1 fuerte)"));
    thr.slider.setTooltip (U ("THRESHOLD: a la derecha = empieza a comprimir antes (más compresión)"));
    mix.slider.setTooltip (U ("DRY/WET: compresión en paralelo"));
    attack.slider.setTooltip (U ("ATTACK (en Manual): rápido = atrapa el golpe, lento = deja pasar el golpe"));
    release.slider.setTooltip (U ("RELEASE (en Fix/Man y Manual): qué tan rápido suelta"));
    select.slider.setTooltip (U ("FIXED: ataque y release automáticos según la música\nFIX/MAN: ataque automático, release manual\nMANUAL: los dos a mano"));
    hpf.slider.setTooltip (U ("SIDECHAIN HPF: el compresor ignora los graves (no bombea con el bombo o el bajo)"));
    color.slider.setTooltip (U ("COLOR: cantidad de armónicos de los transformadores y de la célula al comprimir (100 % = normal)"));
    addAndMakeVisible (vuGr); addChildComponent (vuOut);
    addAndMakeVisible (power);
    atts.push_back (std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "power", power));
    power.setTooltip ("IN: encendido / bypass");
    const juce::StringArray mtxt { "LR", "M", "S", U ("Δ") };
    for (int i = 0; i < 4; ++i)
    {
        auto* b = modes.add (new juce::TextButton (mtxt[i]));
        b->setClickingTogglesState (false);
        b->onClick = [this, i] { auto* prm = proc.apvts.getParameter ("mode"); prm->setValueNotifyingHost (prm->convertTo0to1 ((float) i)); };
        addAndMakeVisible (b);
    }
    modes[0]->setTooltip (U ("LR: estéreo enlazado")); modes[1]->setTooltip (U ("M: comprime solo el centro"));
    modes[2]->setTooltip (U ("S: comprime solo los lados")); modes[3]->setTooltip (U ("Δ (delta): escucha solo lo que el compresor quita"));
    for (auto* b : { &vuGrBtn, &vuOutBtn }) addAndMakeVisible (b);
    vuGrBtn.onClick = [this] { auto* prm = proc.apvts.getParameter ("vu_out"); prm->setValueNotifyingHost (0.0f); };
    vuOutBtn.onClick = [this] { auto* prm = proc.apvts.getParameter ("vu_out"); prm->setValueNotifyingHost (1.0f); };
    vuGrBtn.setTooltip (U ("VU muestra la reducción de ganancia")); vuOutBtn.setTooltip (U ("VU muestra el nivel de salida"));
    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { if (presetBox.getSelectedId() > 0) proc.setCurrentProgram (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);
    buildBackground();
}

void MainView::buildBackground()
{
    background = juce::Image (juce::Image::ARGB, W, H, true);
    juce::Graphics g (background);
    g.fillAll (Colours::rack);
    brushed (g, { 0, 0, (float) W, 40 }, juce::Colour (0xff23262c), juce::Colour (0xff131418), 9, 0.6f);
    engraved (g, "TOP BLUE COMP", { 18, 6, 220, 28 }, 18, juce::Justification::centredLeft, true);
    engraved (g, "COLOR COMPRESSOR  -  BY DJ PAVA HM MUSIC", { 210, 6, 420, 28 }, 12, juce::Justification::centredLeft, false, Colours::engraveDim);
    engraved (g, "PRESET", { 860, 6, 70, 28 }, 13, juce::Justification::centredRight, false, Colours::engraveDim);

    auto r = unit.toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.7f)); g.fillRoundedRectangle (r.translated (0, 4).expanded (2), 8);
    g.saveState();
    juce::Path clip; clip.addRoundedRectangle (r, 7); g.reduceClipRegion (clip);
    brushed (g, r, juce::Colour (0xff2f68a8), juce::Colour (0xff1c477a), 1176, 0.8f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.10f), 0, r.getY(), juce::Colours::transparentWhite, 0, r.getY() + 80, false));
    g.fillRect (r);
    g.restoreState();
    g.setColour (juce::Colours::black.withAlpha (0.8f)); g.drawRoundedRectangle (r, 7, 1.5f);
    // orejas del rack con ranuras
    for (float x : { r.getX() + 20, r.getRight() - 20 })
        for (float y : { r.getY() + 46, r.getBottom() - 46 })
        {
            g.setColour (juce::Colour (0xff0f2a4a)); g.fillRoundedRectangle (x - 14, y - 7, 28, 14, 7);
            g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawRoundedRectangle (x - 14, y - 7, 28, 14, 7, 1.0f);
        }
    // marca propia
    auto txt = [&] (const juce::String& t, juce::Rectangle<float> a, float size, bool bold, juce::Colour c = white, juce::Justification j = juce::Justification::centred)
    {
        g.setFont (sans (size, bold)); g.setColour (juce::Colours::black.withAlpha (0.35f)); g.drawText (t, a.translated (0, 1), j);
        g.setColour (c); g.drawText (t, a, j);
    };
    txt ("TOP", { 52, (float) uy + 70, 160, 36 }, 32, true);
    txt ("BLUE COMP", { 52, (float) uy + 104, 160, 22 }, 17, true);
    txt ("COLOR COMPRESSOR TBC-1", { 40, (float) uy + 130, 184, 16 }, 10.5f, false, whiteDim);
    txt ("IN", { 92, (float) uy + 206, 80, 16 }, 13, true);
    txt ("AUTO", { 872, (float) uy + 8, 50, 16 }, 10, true, whiteDim);
    txt ("MODE", { 1040, (float) uy + 102, 120, 14 }, 10, true, whiteDim);
    juce::Rectangle<float> sig { 900, (float) uy + 238, 140, 30 };
    tvcui::glowText (g, "Dj Pava", sig, serif (22.0f, true), juce::Justification::centred);
    txt ("HM MUSIC", { 900, (float) uy + 266, 140, 14 }, 10, true, whiteDim);
}

void MainView::resized()
{
    presetBox.setBounds (940, 8, 240, 26);
    power.setBounds (112, uy + 228, 40, 70);
    gain.setBounds (230, uy + 18, 150, 160);
    ratio.setBounds (400, uy + 18, 150, 160);
    thr.setBounds (570, uy + 18, 160, 160);
    mix.setBounds (742, uy + 30, 120, 140);
    attack.setBounds (230, uy + 180, 150, 160);
    release.setBounds (400, uy + 180, 150, 160);
    select.setBounds (570, uy + 188, 150, 150);
    hpf.setBounds (730, uy + 188, 150, 150);
    vuGr.setBounds (880, uy + 30, 250, 140);
    vuOut.setBounds (vuGr.getBounds());
    vuGrBtn.setBounds (924, uy + 6, 46, 20); vuOutBtn.setBounds (974, uy + 6, 50, 20);
    for (int i = 0; i < 4; ++i) modes[i]->setBounds (1046 + i * 30, uy + 180, 28, 24);
    color.setBounds (1056, uy + 210, 110, 128);
}

void MainView::paint (juce::Graphics& g)
{
    g.drawImageAt (background, 0, 0);
    // lampara roja de encendido
    const bool on = proc.apvts.getRawParameterValue ("power")->load() > 0.5f;
    const juce::Point<float> c (960, (float) uy + 216);
    g.setColour (juce::Colour (0xff8b9097)); g.fillEllipse (c.x - 17, c.y - 17, 34, 34);
    g.setColour (juce::Colour (0xff3d4045)); g.drawEllipse (c.x - 17, c.y - 17, 34, 34, 1.5f);
    if (on) { g.setColour (juce::Colour (0xffff3020).withAlpha (0.35f)); g.fillEllipse (c.x - 22, c.y - 22, 44, 44); }
    g.setGradientFill (juce::ColourGradient (on ? juce::Colour (0xffff8a70) : juce::Colour (0xff5a1a14), c.x - 4, c.y - 5,
                                             on ? juce::Colour (0xffc01a10) : juce::Colour (0xff2a0a08), c.x + 10, c.y + 10, true));
    g.fillEllipse (c.x - 11, c.y - 11, 22, 22);
}

void MainView::tick()
{
    const bool showOut = proc.apvts.getRawParameterValue ("vu_out")->load() > 0.5f;
    if (vuOut.isVisible() != showOut) { vuOut.setVisible (showOut); vuGr.setVisible (! showOut); }
    (showOut ? vuOut : vuGr).tick();
    vuGrBtn.setToggleState (! showOut, juce::dontSendNotification);
    vuOutBtn.setToggleState (showOut, juce::dontSendNotification);
    const int m = (int) proc.apvts.getRawParameterValue ("mode")->load();
    for (int i = 0; i < 4; ++i) modes[i]->setToggleState (i == m, juce::dontSendNotification);
    // ATTACK y RELEASE se atenuan cuando el modo no los usa
    const int sel = (int) proc.apvts.getRawParameterValue ("select")->load();
    attack.setAlpha (sel == 2 ? 1.0f : 0.45f);
    release.setAlpha (sel == 0 ? 0.45f : 1.0f);
    const bool on = proc.apvts.getRawParameterValue ("power")->load() > 0.5f;
    if (on != lastPower) { lastPower = on; repaint (930, uy + 186, 60, 60); }
    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1) presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
}

//==============================================================================
TopCompEditor::TopCompEditor (TopCompProcessor& p) : AudioProcessorEditor (&p), view (p)
{
    view.setLookAndFeel (&lnf);
    tips.setLookAndFeel (&lnf);
    addAndMakeVisible (view);
    setResizable (true, true);
    setResizeLimits (MainView::W / 2, MainView::H / 2, MainView::W * 3 / 2, MainView::H * 3 / 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) MainView::W / MainView::H);
    setSize (MainView::W * 9 / 10, MainView::H * 9 / 10);
    startTimerHz (30);
}
TopCompEditor::~TopCompEditor() { stopTimer(); tips.setLookAndFeel (nullptr); view.setLookAndFeel (nullptr); }
void TopCompEditor::paint (juce::Graphics& g) { g.fillAll (Colours::rack); }
void TopCompEditor::resized()
{
    view.setBounds (0, 0, MainView::W, MainView::H);
    view.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) MainView::W));
}
