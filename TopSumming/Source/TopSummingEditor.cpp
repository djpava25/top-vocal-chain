#include "TopSummingEditor.h"

using namespace tvcui;
using namespace topsumui;

namespace
{
    const juce::Rectangle<int> unit { 12, 56, 1176, 412 };
    const int uy = 56;
    const int secIn = 56, secIron = 306, secSt = 586, secOut = 856;
}

MainView::MainView (TopSummingProcessor& p)
    : proc (p),
      busLed ([this] { return param ("bus"); }),
      analogLed ([this] { return param ("analog"); }),
      noiseLed ([this] { return param ("noise"); }),
      vu ([this] { return proc.outLevelDb.load(); }, false),
      drive (p.apvts, "drive", "DRIVE", "-6", "+6", "+18"),
      iron (p.apvts, "iron", "IRON", "0", "50", "100"),
      width (p.apvts, "width", "WIDTH", "MONO", "100", "200"),
      crosstalk (p.apvts, "crosstalk", "CROSSTALK", "0", "", "100"),
      output (p.apvts, "output", "OUTPUT", "-18", "", "+6")
{
    for (auto* k : { &drive, &iron, &width, &crosstalk, &output }) addAndMakeVisible (k);
    for (auto* l : { &busLed, &analogLed, &noiseLed }) { addAndMakeVisible (l); l->setInterceptsMouseClicks (false, false); }
    addAndMakeVisible (vu);

    auto attach = [this] (juce::Button& b, const char* id)
    {
        addAndMakeVisible (b);
        atts.push_back (std::make_unique<APVTS::ButtonAttachment> (proc.apvts, id, b));
    };
    attach (sumIn, "sum_in");
    attach (busSw, "bus");
    attach (analogSw, "analog");
    attach (noiseSw, "noise");

    newUnitBtn.onClick = [this] { proc.newAnalogUnit(); };
    newUnitBtn.setTooltip ("Cambia las tolerancias de componentes, como usar otra consola fisica");
    addAndMakeVisible (newUnitBtn);

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

void MainView::buildBackground()
{
    background = juce::Image (juce::Image::ARGB, W, H, true);
    juce::Graphics g (background);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff15171c), 0, 0, Colours::rack, 0, (float) H, false));
    g.fillAll();
    brushed (g, { 0, 0, (float) W, 50 }, juce::Colour (0xff23262c), juce::Colour (0xff131418), 9, 0.6f);
    g.setColour (juce::Colours::black); g.fillRect (0, 49, W, 2);
    engraved (g, "TOP SUMMING", { 22, 10, 220, 30 }, 20, juce::Justification::centredLeft, true);
    engraved (g, "SUMADOR ANALOGICO  -  ESTILO CONSOLA BRITANICA 8816", { 220, 10, 440, 30 }, 13,
              juce::Justification::centredLeft, false, Colours::engraveDim);
    engraved (g, "PRESET", { 680, 10, 80, 30 }, 15, juce::Justification::centredRight, false, Colours::engraveDim);

    // Unidad de rack
    auto r = unit.toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.fillRoundedRectangle (r.translated (0, 4).expanded (2), 6);
    g.saveState();
    juce::Path clip; clip.addRoundedRectangle (r, 5);
    g.reduceClipRegion (clip);
    brushed (g, r, Colours::metalTop, Colours::metalBot, 404);
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

    for (int x : { secIron, secSt, secOut })
    {
        const float top = r.getY() + 14, bot = r.getBottom() - 14;
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRect ((float) x - 1.0f, top + 10, 1.5f, bot - top - 20);
        g.setColour (juce::Colours::white.withAlpha (0.16f));
        g.fillRect ((float) x + 0.5f, top + 10, 1.0f, bot - top - 20);
        drawScrew (g, (float) x, top, 5.5f);
        drawScrew (g, (float) x, bot, 5.5f);
    }

    auto section = [&] (int x, int w, const juce::String& t)
    {
        engraved (g, t, juce::Rectangle<int> (x, uy + 12, w, 32).toFloat(), 23);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRect ((float) x + 26, (float) uy + 47, (float) w - 52, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.28f));
        g.fillRect ((float) x + 26, (float) uy + 48, (float) w - 52, 1.0f);
    };
    section (secIn, 250, "INPUT");
    section (secIron, 280, "TRANSFORMER");
    section (secSt, 270, "STEREO");
    section (secOut, 288, "OUTPUT");

    auto lbl = [&] (const juce::String& t, int x, int y, int w, float size = 15.0f)
    {
        engraved (g, t, juce::Rectangle<int> (x, y, w, 20).toFloat(), size);
    };
    lbl ("BUS", secIn + 20, uy + 270, 70);
    lbl ("CHANNEL", secIn + 10, uy + 382, 90, 14);
    lbl ("ANALOG", secIron + 20, uy + 284, 110);
    lbl ("NOISE", secIron + 20, uy + 334, 110);
    lbl ("(solo en modo BUS)", secSt + 20, uy + 386, 230, 12);

    // Placa
    auto plate = juce::Rectangle<float> ((float) secOut + 30, (float) uy + 334, 228, 70);
    g.setColour (juce::Colours::black.withAlpha (0.65f));
    g.fillRoundedRectangle (plate.translated (0, 3), 4);
    g.saveState();
    g.reduceClipRegion (plate.toNearestInt());
    brushed (g, plate, juce::Colour (0xffd9dbde), juce::Colour (0xff9fa3a9), 505, 1.6f);
    g.restoreState();
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (plate, 4, 1.4f);
    for (auto pt : { plate.getTopLeft() + juce::Point<float> (11, 11), plate.getTopRight() + juce::Point<float> (-11, 11),
                     plate.getBottomLeft() + juce::Point<float> (11, -11), plate.getBottomRight() + juce::Point<float> (-11, -11) })
        drawScrew (g, pt.x, pt.y, 5.0f);
    g.setColour (juce::Colour (0xff141414));
    g.setFont (serif (17.0f, true));
    g.drawText ("TOP", plate.withTrimmedTop (5).withHeight (22), juce::Justification::centred);
    g.setFont (serif (30.0f, true));
    g.drawText ("SUMMING", plate.withTrimmedTop (22).withHeight (40), juce::Justification::centred);
}

void MainView::resized()
{
    prevBtn.setBounds (768, 11, 30, 28);
    presetBox.setBounds (802, 11, 300, 28);
    nextBtn.setBounds (1106, 11, 30, 28);

    // INPUT
    drive.setBounds (secIn + 30, uy + 60, 190, 206);
    busLed.setBounds (secIn + 26, uy + 296, 30, 30);
    busSw.setBounds (secIn + 62, uy + 300, 40, 74);
    sumIn.setBounds (secIn + 122, uy + 322, 104, 32);

    // TRANSFORMER
    iron.setBounds (secIron + 45, uy + 60, 190, 206);
    analogLed.setBounds (secIron + 136, uy + 279, 30, 30);
    analogSw.setBounds (secIron + 172, uy + 270, 40, 48);
    noiseLed.setBounds (secIron + 136, uy + 329, 30, 30);
    noiseSw.setBounds (secIron + 172, uy + 320, 40, 48);
    newUnitBtn.setBounds (secIron + 65, uy + 376, 150, 26);

    // STEREO
    width.setBounds (secSt + 40, uy + 60, 190, 206);
    crosstalk.setBounds (secSt + 70, uy + 262, 130, 124);

    // OUTPUT
    vu.setBounds (secOut + 34, uy + 62, 220, 116);
    output.setBounds (secOut + 64, uy + 182, 160, 148);
}

void MainView::paint (juce::Graphics& g)
{
    g.drawImageAt (background, 0, 0);
}

void MainView::tick()
{
    for (auto* l : { &busLed, &analogLed, &noiseLed }) l->tick();
    vu.tick();
    const bool bus = param ("bus");
    if (crosstalk.isEnabled() != bus) { crosstalk.setEnabled (bus); crosstalk.setAlpha (bus ? 1.0f : 0.45f); }
    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
        presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
}

//==============================================================================
TopSummingEditor::TopSummingEditor (TopSummingProcessor& p) : AudioProcessorEditor (&p), view (p)
{
    view.setLookAndFeel (&lnf);
    addAndMakeVisible (view);
    setResizable (true, true);
    setResizeLimits (MainView::W / 2, MainView::H / 2, MainView::W * 3 / 2, MainView::H * 3 / 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) MainView::W / MainView::H);
    setSize (MainView::W * 9 / 10, MainView::H * 9 / 10);
    startTimerHz (30);
}

TopSummingEditor::~TopSummingEditor()
{
    stopTimer();
    view.setLookAndFeel (nullptr);
}

void TopSummingEditor::paint (juce::Graphics& g) { g.fillAll (Colours::rack); }

void TopSummingEditor::resized()
{
    view.setBounds (0, 0, MainView::W, MainView::H);
    view.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) MainView::W));
}
