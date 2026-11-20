#include "TopChoirEditor.h"

namespace
{
    const juce::Colour bg (0xff10202a), panel (0xff17303c), panelHi (0xff1f3f4d), line (0xff0a151b), gold (0xffeec66b), text (0xffe2ecef),
                       dim (0xff8fb0ba), coral (0xffd2553f), coralDark (0xff8e2a1c), cyan (0xff62e0f0), btn (0xff142229);
    juce::String U (const char* s) { return juce::String::fromUTF8 (s); }
    juce::Font sans (float h, bool b = false) { return juce::Font (juce::FontOptions (h, b ? juce::Font::bold : juce::Font::plain)); }
}

TopChoirEditor::LnF::LnF()
{
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, coralDark);
}
void TopChoirEditor::LnF::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.5f);
    const bool on = b.getToggleState(), red = (bool) b.getProperties()["red"];
    g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (r.translated (0, 2), 6);
    if (red)
    {
        g.setGradientFill (juce::ColourGradient (coral.brighter (over ? 0.1f : 0.0f), r.getCentreX(), r.getCentreY(), coralDark, r.getX(), r.getY(), true));
        g.fillRoundedRectangle (r, 6);
    }
    else
    {
        g.setColour (btn.brighter (over ? 0.12f : 0.0f).darker (down ? 0.3f : 0.0f)); g.fillRoundedRectangle (r, 6);
        if (on) { g.setColour (cyan.withAlpha (0.12f)); g.fillRoundedRectangle (r, 6); }
    }
    g.setColour (on ? cyan.withAlpha (0.7f) : juce::Colours::black.withAlpha (0.6f)); g.drawRoundedRectangle (r, 6, 1.2f);
}
void TopChoirEditor::LnF::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    const bool on = b.getToggleState(), red = (bool) b.getProperties()["red"];
    const float size = (float) b.getProperties().getWithDefault ("size", 16.0);
    g.setFont (sans (size, ! red));
    auto r = b.getLocalBounds();
    if (on) { g.setColour (cyan.withAlpha (0.35f)); g.drawFittedText (b.getButtonText(), r.translated (0, 1), juce::Justification::centred, 1); }
    g.setColour (on ? cyan : red ? text : (b.isEnabled() ? text.withAlpha (0.85f) : dim.withAlpha (0.4f)));
    g.drawFittedText (b.getButtonText(), r, juce::Justification::centred, 1);
}

TopChoirEditor::TopChoirEditor (TopChoirProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    for (auto* b : { &startOver, &gear, &play, &mixBtn, &clearBtn, &browseBtn, &modelL, &modelR, &micL, &micR, &octUp, &octDown, &keyBtn, &scaleBtn,
                     &thirdUp, &thirdDown, &fifthUp, &fifthDown, &stereoBtn, &create, &openFolder })
        addAndMakeVisible (b);
    keyBtn.getProperties().set ("red", true); scaleBtn.getProperties().set ("red", true);
    for (auto* b : { &keyBtn, &scaleBtn }) b->getProperties().set ("size", 20.0);
    create.getProperties().set ("size", 22.0);
    octUp.setButtonText (U ("Octava Arriba")); octDown.setButtonText (U ("Octava Abajo"));
    thirdUp.setButtonText (U ("Arriba")); thirdDown.setButtonText (U ("Abajo")); fifthUp.setButtonText (U ("Arriba")); fifthDown.setButtonText (U ("Abajo"));
    create.setButtonText (U ("Crear Voces")); openFolder.setButtonText (U ("Abrir carpeta"));
    auto attach = [this] (juce::Button& b, const char* id)
    {
        b.setClickingTogglesState (true);
        atts.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, id, b));
    };
    attach (octUp, "oct_up"); attach (octDown, "oct_down"); attach (thirdUp, "third_up"); attach (thirdDown, "third_down");
    attach (fifthUp, "fifth_up"); attach (fifthDown, "fifth_down"); attach (stereoBtn, "stereo");
    mixBtn.setClickingTogglesState (true);
    mixBtn.onClick = [this] { proc.mixLead = mixBtn.getToggleState(); };
    mixBtn.setTooltip (U ("M: escuchar el coro junto con tu voz original"));
    play.onClick = [this] { if (proc.hasVoice()) { proc.playing = ! proc.playing.load(); if (proc.playing) proc.playPos = 0.0; } };
    clearBtn.onClick = [this] { proc.clearAll(); peaks.clear(); status = {}; repaint(); };
    startOver.onClick = [this] { proc.clearAll(); peaks.clear(); status = {}; repaint(); };
    browseBtn.onClick = [this] { browse(); };
    gear.onClick = [this] { showSettings(); };
    auto step = [this] (const char* id, int d)
    {
        auto* prm = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id));
        const int n = prm->choices.size(), v = (prm->getIndex() + d + n) % n;
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) v));
        repaint();
    };
    modelL.onClick = [step] { step ("model", -1); }; modelR.onClick = [step] { step ("model", 1); };
    micL.onClick = [step] { step ("mic", -1); }; micR.onClick = [step] { step ("mic", 1); };
    scaleBtn.onClick = [step] { step ("scale", 1); };
    keyBtn.onClick = [this]
    {
        juce::PopupMenu m;
        m.addItem (100, U ("Detectar tonalidad de la voz"), proc.hasVoice());
        m.addSeparator();
        const juce::StringArray keys { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        for (int i = 0; i < 12; ++i) m.addItem (i + 1, keys[i], true, (int) proc.apvts.getRawParameterValue ("key")->load() == i);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&keyBtn), [this] (int r)
        {
            if (r == 100) status = proc.autoDetectKey() ? U ("Tonalidad detectada") : U ("No se pudo detectar la tonalidad");
            else if (r > 0) { auto* prm = proc.apvts.getParameter ("key"); prm->setValueNotifyingHost (prm->convertTo0to1 ((float) (r - 1))); }
            repaint();
        });
    };
    create.onClick = [this] { proc.createVocals(); status = {}; };
    openFolder.onClick = [] { TopChoirProcessor::outputFolder().createDirectory(); TopChoirProcessor::outputFolder().startAsProcess(); };
    stereoBtn.setTooltip (U ("ST = coro abierto en estéreo · MONO = todo al centro"));
    create.setTooltip (U ("Genera el coro y lo guarda como WAV (Documentos/Top Choir)"));
    setSize (W, H);
    startTimerHz (20);
}

TopChoirEditor::~TopChoirEditor() { stopTimer(); setLookAndFeel (nullptr); }


void TopChoirEditor::resized()
{
    startOver.setBounds (20, 20, 118, 44); gear.setBounds (384, 24, 38, 38);
    play.setBounds (20, 104, 42, 40); mixBtn.setBounds (20, 152, 42, 40);
    clearBtn.setBounds (waveR.getRight() - 30, waveR.getY() + 6, 24, 22);
    browseBtn.setBounds (dropR.getCentreX() - 80, dropR.getCentreY() + 2, 160, 34);
    modelL.setBounds (210, 384, 48, 50); modelR.setBounds (372, 384, 48, 50);
    micL.setBounds (210, 446, 48, 50); micR.setBounds (372, 446, 48, 50);
    octUp.setBounds (208, 528, 104, 42); octDown.setBounds (316, 528, 104, 42);
    keyBtn.setBounds (208, 582, 96, 50); scaleBtn.setBounds (308, 582, 112, 50);
    thirdUp.setBounds (300, 642, 58, 38); thirdDown.setBounds (362, 642, 58, 38);
    fifthUp.setBounds (300, 688, 58, 38); fifthDown.setBounds (362, 688, 58, 38);
    stereoBtn.setBounds (150, 752, 70, 34);
    create.setBounds (234, 744, 186, 52);
    openFolder.setBounds (resultR.getRight() - 112, resultR.getY() + 18, 104, 34);
}

void TopChoirEditor::buildPeaks()
{
    const bool useChoir = proc.hasChoir();
    const auto f = useChoir ? proc.getChoirFile() : proc.getVoiceFile();
    if (f == peaksFor && useChoir == peaksChoir && ! peaks.empty()) return;
    peaksFor = f; peaksChoir = useChoir;
    const auto& b = useChoir ? proc.getChoir() : proc.getVoice();
    const int cols = waveR.getWidth() - 8;
    peaks.assign ((size_t) cols, 0.0f);
    if (b.getNumSamples() == 0) return;
    const int per = std::max (1, b.getNumSamples() / cols);
    for (int c = 0; c < cols; ++c)
        peaks[(size_t) c] = b.getMagnitude (0, c * per, std::min (per, b.getNumSamples() - c * per));
    const float mx = *std::max_element (peaks.begin(), peaks.end());
    if (mx > 0) for (auto& v : peaks) v /= mx;
}

void TopChoirEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);
    auto section = [&] (int y0, int y1, juce::Colour c) { g.setColour (c); g.fillRect (0, y0, W, y1 - y0); g.setColour (line); g.fillRect (0, y1 - 3, W, 3); };
    section (0, 84, panel); section (84, 216, panelHi); section (216, 366, panel); section (366, 510, panel); section (510, 738, panel); section (738, H, panel);
    // titulo
    g.setFont (sans (40.0f, true)); g.setColour (gold.withAlpha (0.25f));
    g.drawText ("TOP CHOIR", 150, 18, 230, 50, juce::Justification::centred);
    g.setColour (gold); g.drawText ("TOP CHOIR", 149, 16, 230, 50, juce::Justification::centred);
    g.setFont (sans (10.0f)); g.setColour (dim);
    g.drawText ("BY DJ PAVA HM MUSIC", 150, 62, 230, 14, juce::Justification::centred);
    // forma de onda
    auto w = waveR.toFloat();
    g.setGradientFill (juce::ColourGradient (coral, w.getCentreX(), w.getCentreY(), coralDark.darker (0.4f), w.getX(), w.getY(), true));
    g.fillRoundedRectangle (w, 6);
    g.setColour (juce::Colours::black.withAlpha (0.5f)); g.drawRoundedRectangle (w, 6, 1.5f);
    buildPeaks();
    if (proc.hasVoice() && ! peaks.empty())
    {
        g.setColour (juce::Colour (0xffffd9a0));
        for (size_t i = 0; i < peaks.size(); ++i)
        {
            const float h = std::max (1.0f, peaks[i] * (w.getHeight() * 0.42f));
            g.fillRect (w.getX() + 4 + (float) i, w.getCentreY() - h, 1.0f, h * 2);
        }
        const auto& b = proc.hasChoir() ? proc.getChoir() : proc.getVoice();
        if (proc.playing.load() && b.getNumSamples() > 0)
        {
            const float x = w.getX() + 4 + (float) (proc.playPos.load() / b.getNumSamples()) * (w.getWidth() - 8);
            g.setColour (juce::Colours::white); g.fillRect (x, w.getY() + 4, 2.0f, w.getHeight() - 8);
        }
        g.setFont (sans (11.0f, true)); g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.drawText (proc.hasChoir() ? U ("CORO") : U ("VOZ ORIGINAL"), w.reduced (8, 4), juce::Justification::topLeft);
    }
    else
    {
        g.setColour (juce::Colour (0xffffd9a0).withAlpha (0.6f)); g.fillRect (w.getX() + 6, w.getCentreY(), w.getWidth() - 12, 1.5f);
    }
    // nombre del archivo
    g.setFont (sans (19.0f)); g.setColour (text);
    g.drawText (proc.hasVoice() ? proc.getVoiceFile().getFileName() : U ("Sin archivo"), 74, 196, 340, 22, juce::Justification::centredLeft, true);
    {
        juce::Path doc; doc.addRoundedRectangle (40.0f, 199.0f, 14.0f, 17.0f, 2.0f);
        g.setColour (text); g.strokePath (doc, juce::PathStrokeType (1.5f));
    }
    // zona para soltar archivos
    {
        auto d = dropR.toFloat();
        juce::Path border; border.addRoundedRectangle (d, 6);
        juce::Path dashed; const float dl[] { 8, 6 };
        juce::PathStrokeType (1.6f).createDashedStroke (dashed, border, dl, 2);
        g.setColour (dragOver ? cyan : dim.withAlpha (0.7f)); g.fillPath (dashed);
        if (dragOver) { g.setColour (cyan.withAlpha (0.08f)); g.fillRoundedRectangle (d, 6); }
        g.setFont (sans (16.0f)); g.setColour (dim);
        g.drawText (U ("Arrastra tu voz aquí o"), d.withHeight (d.getHeight() * 0.5f), juce::Justification::centredBottom);
    }
    // filas de selectores
    auto label = [&] (const juce::String& t, int y, int h = 50)
    {
        g.setFont (sans (21.0f)); g.setColour (text); g.drawText (t, 22, y, 180, h, juce::Justification::centredLeft);
        g.setColour (dim.withAlpha (0.3f)); g.fillRect (22 + (int) juce::GlyphArrangement::getStringWidth (sans (21.0f), t) + 10, y + h / 2, 30, 1);
    };
    auto redBox = [&] (juce::Rectangle<int> r, const juce::String& t)
    {
        auto f = r.toFloat().reduced (1.5f);
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (f.translated (0, 2), 6);
        g.setGradientFill (juce::ColourGradient (coral, f.getCentreX(), f.getCentreY(), coralDark, f.getX(), f.getY(), true));
        g.fillRoundedRectangle (f, 6);
        g.setFont (sans (19.0f)); g.setColour (text); g.drawFittedText (t, r.reduced (4, 0), juce::Justification::centred, 1);
    };
    label (U ("Modelo"), 384); label (U ("Micrófono"), 446);
    redBox ({ 262, 384, 106, 50 }, choir::modelNames()[(int) proc.apvts.getRawParameterValue ("model")->load()]);
    redBox ({ 262, 446, 106, 50 }, choir::micNames()[(int) proc.apvts.getRawParameterValue ("mic")->load()]);
    label (U ("Octavas"), 524); label (U ("Armonías"), 582);
    g.setFont (sans (19.0f)); g.setColour (text);
    g.drawText (U ("Tercera"), 196, 642, 98, 38, juce::Justification::centredRight);
    g.drawText (U ("Quinta"), 196, 688, 98, 38, juce::Justification::centredRight);
    // generar
    label (U ("Crear coro"), 744);
    g.setFont (sans (12.0f, true)); g.setColour (dim);
    g.drawText ("MONO / ST", 150, 788, 70, 14, juce::Justification::centred);
    if (proc.isRendering())
    {
        auto pr = juce::Rectangle<float> (234, 800, 186, 6);
        g.setColour (line); g.fillRoundedRectangle (pr, 3);
        g.setColour (cyan); g.fillRoundedRectangle (pr.withWidth (pr.getWidth() * proc.renderProgress()), 3);
    }
    // resultado: arrastrar al DAW
    {
        auto r = resultR.toFloat().withTrimmedTop (4);
        if (proc.hasChoir())
        {
            g.setColour (cyan.withAlpha (0.1f)); g.fillRoundedRectangle (r.withTrimmedRight (120), 6);
            g.setColour (cyan.withAlpha (0.6f)); g.drawRoundedRectangle (r.withTrimmedRight (120), 6, 1.2f);
            g.setFont (sans (13.0f, true)); g.setColour (cyan);
            g.drawText (U ("ARRASTRA EL CORO A FL STUDIO >>"), r.withTrimmedRight (120).withHeight (30).translated (10, 4), juce::Justification::centredLeft);
            g.setFont (sans (12.0f)); g.setColour (text);
            g.drawText (proc.getChoirFile().getFileName(), r.withTrimmedRight (124).withTrimmedTop (30).translated (10, 0), juce::Justification::centredLeft, true);
        }
        else
        {
            g.setFont (sans (13.0f)); g.setColour (dim);
            g.drawFittedText (status.isNotEmpty() ? status : (proc.hasVoice() ? U ("Elige las voces y pulsa Crear Voces.") : U ("Carga una voz para empezar (WAV, AIFF, FLAC, MP3).")),
                              r.withTrimmedRight (120).toNearestInt().reduced (8, 0), juce::Justification::centredLeft, 2);
        }
    }
}

void TopChoirEditor::timerCallback()
{
    const auto* m = proc.apvts.getRawParameterValue ("scale");
    static const char* scales[] { "Cromática", "Mayor", "Menor" };
    scaleBtn.setButtonText (U ("Escala · ") + U (scales[juce::jlimit (0, 2, (int) m->load())]));
    const juce::StringArray keys { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    keyBtn.setButtonText (U ("Tono · ") + keys[(int) proc.apvts.getRawParameterValue ("key")->load()]);
    stereoBtn.setButtonText (stereoBtn.getToggleState() ? "ST" : "MONO");
    play.setButtonText (proc.playing.load() ? "||" : ">");
    create.setEnabled (proc.hasVoice() && ! proc.isRendering());
    create.setButtonText (proc.isRendering() ? U ("Creando... ") + juce::String (juce::roundToInt (proc.renderProgress() * 100)) + "%" : U ("Crear Voces"));
    openFolder.setVisible (proc.hasChoir());
    browseBtn.setVisible (true);
    repaint();
}

bool TopChoirEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files) if (f.endsWithIgnoreCase (".wav") || f.endsWithIgnoreCase (".aif") || f.endsWithIgnoreCase (".aiff") || f.endsWithIgnoreCase (".flac") || f.endsWithIgnoreCase (".mp3")) return true;
    return false;
}
void TopChoirEditor::filesDropped (const juce::StringArray& files, int, int)
{
    dragOver = false;
    if (! files.isEmpty()) status = proc.loadVoice (juce::File (files[0])) ? juce::String() : U ("No se pudo abrir ese archivo.");
    peaks.clear(); repaint();
}
void TopChoirEditor::browse()
{
    chooser = std::make_unique<juce::FileChooser> (U ("Elige tu voz"), juce::File::getSpecialLocation (juce::File::userMusicDirectory), "*.wav;*.aif;*.aiff;*.flac;*.mp3");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (f.existsAsFile()) { status = proc.loadVoice (f) ? juce::String() : U ("No se pudo abrir ese archivo."); peaks.clear(); repaint(); }
    });
}
void TopChoirEditor::showSettings()
{
    juce::PopupMenu m;
    m.addItem (1, U ("Abrir carpeta de coros"));
    m.addItem (2, U ("Detectar tonalidad de la voz"), proc.hasVoice());
    m.addItem (3, U ("Incluir unísono (mismas notas que tu voz)"), true, proc.apvts.getRawParameterValue ("unison")->load() > 0.5f);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&gear), [this] (int r)
    {
        if (r == 1) { TopChoirProcessor::outputFolder().createDirectory(); TopChoirProcessor::outputFolder().startAsProcess(); }
        if (r == 2) status = proc.autoDetectKey() ? U ("Tonalidad detectada") : U ("No se pudo detectar la tonalidad");
        if (r == 3) { auto* p = proc.apvts.getParameter ("unison"); p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f); }
    });
}
void TopChoirEditor::mouseUp (const juce::MouseEvent&) {}
void TopChoirEditor::mouseDrag (const juce::MouseEvent& e)
{
    // arrastrar el coro terminado al DAW
    if (proc.hasChoir() && resultR.withTrimmedRight (120).contains (e.getMouseDownPosition()) && e.getDistanceFromDragStart() > 6)
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ proc.getChoirFile().getFullPathName() }, false, this);
}
