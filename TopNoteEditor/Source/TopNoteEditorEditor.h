#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <set>
#include "TopNoteEditorProcessor.h"

namespace tne
{
enum Tool { Main = 0, Modulation, Drift, Formant, Amplitude, Separate };

class Look : public juce::LookAndFeel_V4
{
public:
    Look();
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    void drawLinearSlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider::SliderStyle, juce::Slider&) override;
};

// Lienzo de notas
class NoteView : public juce::Component, public juce::KeyListener, private juce::ScrollBar::Listener
{
public:
    explicit NoteView (TopNoteEditorProcessor&);
    ~NoteView() override;
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&, juce::Component*) override;
    bool keyPressed (const juce::KeyPress& k) override { return keyPressed (k, this); }

    void fitAll();
    void zoom (float factor, float aroundX);
    void followPlayhead();             // la vista sigue la linea de reproduccion
    void resized() override;
    std::vector<int> selection() const { return { sel.begin(), sel.end() }; }
    void selectAll();
    int hoverNote() const { return hover; }
    Tool tool = Main;
    std::function<void()> onChange;
    double cursorSample = 0.0;

    static constexpr int keyW = 58, rulerH = 26;

private:
    void scrollBarMoved (juce::ScrollBar*, double newRangeStart) override;
    void syncBars();
    double totalSeconds() const;
    double visibleSeconds() const { return (double) (getWidth() - keyW - barW) / pxPerSec; }
    float visibleSemis() const { return (float) (getHeight() - rulerH - barW) / pxPerSemi; }
    void setViewStart (double s);
    juce::ScrollBar hbar { false }, vbar { true };
    static constexpr int barW = 14;
    double panStartView = 0.0; float panStartTop = 0.0f;
    float xOf (double sample) const;
    double sampleOf (float x) const;
    float yOf (float midi) const;
    float midiOf (float y) const;
    int hit (juce::Point<float>, int& edge) const;     // edge: -1 izquierda, 1 derecha, 0 cuerpo
    juce::Rectangle<float> noteRect (const ned::Note&) const;
    double snapTime (double sample) const;
    double gridSamples() const;
    void split (int idx, double targetSample);
    bool mergeAt (juce::Point<float>);
    void liveChange();
    void validateSelection();

    TopNoteEditorProcessor& proc;
    double viewStart = 0.0;           // segundos
    float pxPerSec = 120.0f, pxPerSemi = 16.0f, topMidi = 84.0f;
    std::set<int> sel;
    int hover = -1, hoverEdge = 0;

    enum DragMode { None, Move, ResizeL, ResizeR, Param, Band, Scrub, Pan };
    DragMode drag = None;
    int axis = 0;                     // 0 sin decidir, 1 vertical, 2 horizontal
    int primary = -1;
    bool editBegun = false;
    std::vector<ned::Note> dragStart;
    juce::Rectangle<float> band;
    juce::uint32 lastLive = 0;
    int lastVersionFit = -1;
};

class MacroPanel : public juce::Component
{
public:
    MacroPanel (TopNoteEditorProcessor&, NoteView&, int type);
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    TopNoteEditorProcessor& proc;
    NoteView& view;
    int type;
    juce::Slider a, b;
    juce::ComboBox grid;
    juce::TextButton apply { "APLICAR" };
    juce::String la, lb;
};

class DragTile : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    juce::File file;
};
} // namespace tne

class TopNoteEditorEditor : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit TopNoteEditorEditor (TopNoteEditorProcessor&);
    ~TopNoteEditorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray&, int, int) override;
    void tickNow() { timerCallback(); }
    tne::NoteView& getView() { return view; }
    static constexpr int W = 1280, H = 780;

private:
    void timerCallback() override;
    void setTool (int);
    void openMacro (int type, juce::Component& anchor);

    TopNoteEditorProcessor& proc;
    tne::Look look;
    tne::NoteView view;
    juce::TextButton transferBtn { "TRANSFERIR" }, importBtn { "IMPORTAR" }, undoBtn { "DESHACER" }, redoBtn { "REHACER" };
    juce::ComboBox algoBox, keyBox, scaleBox;
    juce::ToggleButton snapBtn { "Ajustar a escala" };
    juce::OwnedArray<juce::TextButton> tools;
    juce::TextButton macroPitch { juce::String::fromUTF8 ("CORREGIR AFINACIÓN") }, macroTime { "CUANTIZAR TIEMPO" }, macroLevel { "NIVELAR NOTAS" };
    juce::TextButton playBtn { "PLAY" }, exportBtn { "EXPORTAR WAV" }, fitBtn { "VER TODO" };
    tne::DragTile tile;
    juce::Slider zoomSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAtt, scaleAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> snapAtt;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::TooltipWindow tips { this, 500 };
    int lastStatus = -1;
    double lastZoom = 0.4;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopNoteEditorEditor)
};
