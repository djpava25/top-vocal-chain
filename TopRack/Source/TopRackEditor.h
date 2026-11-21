#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "TopRackProcessor.h"

namespace trui
{
using APVTS = juce::AudioProcessorValueTreeState;

class Look : public juce::LookAndFeel_V4
{
public:
    Look();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
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

class SlotPanel : public juce::Component
{
public:
    SlotPanel (TopRackProcessor&, int slot);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void refresh();            // reconstruye si cambio el modulo
    void tick();
    std::function<void (int)> onSelect;
    bool selected = false;
    int slot;
private:
    void build();
    TopRackProcessor& proc;
    int builtType = -1;
    juce::OwnedArray<RackKnob> knobs;
    juce::TextButton byp { "ON" }, sc { "SC" }, up { juce::String::fromUTF8 ("\xe2\x96\xb2") }, down { juce::String::fromUTF8 ("\xe2\x96\xbc") }, remove { "X" };
    juce::Slider trim;
    std::unique_ptr<APVTS::SliderAttachment> trimAtt;
    std::unique_ptr<APVTS::ButtonAttachment> scAtt;
    float grShown = 0;
};

class Library : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit Library (TopRackProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    std::function<void (int)> onPick;
private:
    struct Item { int type; int y; };
    std::vector<Item> items;
    std::vector<std::pair<juce::String, int>> headers;
    int hover = -1;
};
} // namespace trui

class TopRackEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TopRackEditor (TopRackProcessor&);
    ~TopRackEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void tickNow() { timerCallback(); }
    void selectSlot (int);
    void mouseDown (const juce::MouseEvent&) override;
    static constexpr int W = 1240, H = 820;
private:
    void timerCallback() override;
    void pick (int type);
    TopRackProcessor& proc;
    trui::Look look;
    trui::Library library;
    juce::Viewport viewport;
    juce::Component rackHolder;
    juce::OwnedArray<trui::SlotPanel> slots;
    juce::OwnedArray<juce::TextButton> dreams;
    juce::ComboBox presetBox;
    juce::TextButton osBtn { "OVERSAMPLING 2X" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> osAtt;
    juce::OwnedArray<juce::Slider> globals, macros;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> gAtt;
    juce::OwnedArray<juce::Label> macroLabels;
    int selected = 0;
    juce::TooltipWindow tips { this, 500 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopRackEditor)
};
