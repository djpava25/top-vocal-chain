#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "TopRackProcessor.h"

// Interfaz de TOP RACK estilo "rack de hardware": modulos lado a lado con frentes propios,
// navegador de modulos con miniaturas, Dream Strips, A/B, deshacer y HQ.
namespace trui
{
using APVTS = juce::AudioProcessorValueTreeState;

// posicion de un control dentro del frente del modulo (212 x 560)
struct KPos { int k; float x, y, r; int style; juce::uint32 cap; };
enum KStyle { kBlack = 0, kCap, kChrome, kChicken, kButton, kColumn };
struct Face
{
    juce::uint32 panel; int texture; juce::uint32 ink, title;
    const char* name; const char* sub; int meter;      // 0 ninguno, 1 VU, 2 LEDs, 3 puerta
    std::vector<KPos> pos;
};
const Face& face (int type);
constexpr float faceW = 212.0f, faceH = 560.0f;
void drawKnob (juce::Graphics&, juce::Point<float> c, float r, float norm, int style, juce::Colour cap, int steps, juce::Colour ink, bool scale);
void drawFace (juce::Graphics&, int type, bool withControls);        // en coordenadas del frente
juce::Image thumbnail (int type);

class Look : public juce::LookAndFeel_V4
{
public:
    Look();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void drawBubble (juce::Graphics&, juce::BubbleComponent&, const juce::Point<float>&, const juce::Rectangle<float>&) override;
    void drawTextEditorOutline (juce::Graphics&, int, int, juce::TextEditor&) override {}
    void fillTextEditorBackground (juce::Graphics&, int, int, juce::TextEditor&) override;
};

// perilla de modulo (continua o por pasos) con menu para asignar macros
class RackKnob : public juce::Slider
{
public:
    RackKnob (TopRackProcessor&, int slot, int knob, const trk::KnobDef&);
    double snapValue (double v, DragMode) override;
    void mouseDown (const juce::MouseEvent&) override;
    int slot, knob;
    trk::KnobDef def;
private:
    TopRackProcessor& proc;
    std::unique_ptr<APVTS::SliderAttachment> att;
};

// boton iluminado de dos posiciones (o columna de botones para selectores)
class RackToggle : public juce::Component, public juce::SettableTooltipClient
{
public:
    RackToggle (TopRackProcessor&, int slot, int knob, const trk::KnobDef&, bool column);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    trk::KnobDef def;
private:
    std::unique_ptr<juce::ParameterAttachment> att;
    float value = 0; bool column;
};

// boton pequeno con icono / texto
class IconButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    enum Icon { none, power, undo, redo, trash, chevron, menu, up, down, home, macro, close, hamburger };
    IconButton (Icon i, const juce::String& t = {}) : icon (i), text (t) {}
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent& e) override { if (getLocalBounds().contains (e.getPosition()) && onClick) onClick (e); }
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }
    std::function<void (const juce::MouseEvent&)> onClick;
    Icon icon; juce::String text;
    bool on = false, hover = false, framed = true;
    juce::Colour onColour { 0xffffb72e };
    float fontSize = 13.0f;
};

class SlotColumn : public juce::Component, public juce::DragAndDropTarget
{
public:
    SlotColumn (TopRackProcessor&, int slot);
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override { dragOver = true; repaint(); }
    void itemDragExit (const SourceDetails&) override { dragOver = false; repaint(); }
    void itemDropped (const SourceDetails&) override;
    void refresh();
    void tick();
    void moduleMenu();
    std::function<void (int)> onSelect;
    bool selected = false;
    int slot;
    static constexpr int colW = 218, colH = 646;
    static constexpr float faceX = 3.0f, faceY = 28.0f;
private:
    void build();
    TopRackProcessor& proc;
    int builtType = -1;
    juce::OwnedArray<RackKnob> knobs;
    juce::OwnedArray<RackToggle> toggles;
    IconButton power { IconButton::power }, solo { IconButton::none, "S" }, nameBox { IconButton::none }, close { IconButton::close };
    IconButton scInt { IconButton::none, "INT" }, scExt { IconButton::none, "EXT" };
    juce::Slider outGain;
    std::unique_ptr<APVTS::SliderAttachment> gainAtt;
    float meter = 0, peakOut = -100;
    bool dragOver = false;
};

// lista de modulos del navegador (miniaturas)
class ModuleList : public juce::Component, public juce::SettableTooltipClient
{
public:
    ModuleList();
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void setFilter (const juce::String& category, const juce::String& search);
    std::function<void (int)> onPick;
    static constexpr int rowH = 156;
private:
    int itemAt (int y) const { const int i = y / rowH; return juce::isPositiveAndBelow (i, (int) items.size()) ? i : -1; }
    std::vector<int> items;
    int hover = -1; bool dragging = false;
};
} // namespace trui

class TopRackEditor : public juce::AudioProcessorEditor, public juce::DragAndDropContainer, private juce::Timer
{
public:
    explicit TopRackEditor (TopRackProcessor&);
    ~TopRackEditor() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void tickNow() { timerCallback(); }
    void selectSlot (int);
    static constexpr int W = 1380, H = 820;
private:
    void timerCallback() override;
    void pick (int type);
    void showTab (bool modules);
    void presetMenu();
    void switchAB (int slot);
    void dreamClick (int i, const juce::MouseEvent&);
    TopRackProcessor& proc;
    trui::Look look;
    // barra superior
    trui::IconButton logo { trui::IconButton::none, "TR" }, menuBtn { trui::IconButton::menu }, presetUp { trui::IconButton::up }, presetDown { trui::IconButton::down };
    trui::IconButton undoBtn { trui::IconButton::undo }, redoBtn { trui::IconButton::redo }, aBtn { trui::IconButton::none, "A" }, copyBtn { trui::IconButton::chevron },
                     bBtn { trui::IconButton::none, "B" }, trashBtn { trui::IconButton::trash }, hqBtn { trui::IconButton::none, "HQ" }, macroBtn { trui::IconButton::macro };
    juce::OwnedArray<juce::Slider> globals;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> gAtt;
    // navegador
    trui::IconButton tabModules { trui::IconButton::none }, tabMacro { trui::IconButton::none };
    juce::OwnedArray<trui::IconButton> cats;
    juce::TextEditor search;
    trui::ModuleList list;
    juce::Viewport listView;
    juce::OwnedArray<juce::Slider> macros;
    juce::OwnedArray<juce::Label> macroLabels;
    bool modulesTab = true;
    juce::String category { "Todos" };
    // rack
    trui::IconButton homeBtn { trui::IconButton::home };
    juce::OwnedArray<trui::IconButton> dreams;
    juce::Viewport viewport;
    juce::Component rackHolder;
    juce::OwnedArray<trui::SlotColumn> slots;
    juce::ComboBox presetBox;
    int selected = 0, transactionTick = 0;
    juce::TooltipWindow tips { this, 500 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopRackEditor)
};
