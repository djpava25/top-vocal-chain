#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "TopSmoothProcessor.h"

namespace tsmui
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

class Knob : public juce::Component
{
public:
    Knob (APVTS&, const juce::String& id, const juce::String& title, juce::Colour, bool big = false);
    void paint (juce::Graphics&) override;
    void resized() override;
    void rebind (const juce::String& id, const juce::String& title);
    juce::Slider slider;
private:
    APVTS& state;
    juce::String title;
    bool big;
    std::unique_ptr<APVTS::SliderAttachment> att;
};

// Grafica: espectro, reduccion en vivo y EQ de foco con nodos arrastrables
class Graph : public juce::Component
{
public:
    explicit Graph (TopSmoothProcessor&);
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    std::function<void (int)> onSelect;
    int selected = 2;
private:
    float xOf (float hz) const;
    float hzOf (float x) const;
    float sensY (float db) const;
    float specY (float db) const;
    int hitTarget (juce::Point<float>) const;     // 0..4 nodos, 10 corte grave, 11 corte agudo
    void set (const juce::String& id, float v);
    void gesture (const juce::String& id, bool begin);
    TopSmoothProcessor& proc;
    int dragging = -1, hover = -1;
};
} // namespace tsmui

class TopSmoothEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TopSmoothEditor (TopSmoothProcessor&);
    ~TopSmoothEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void tickNow() { timerCallback(); }
    static constexpr int W = 1040, H = 640;
private:
    void timerCallback() override;
    void selectNode (int);
    TopSmoothProcessor& proc;
    tsmui::Look look;
    tsmui::Graph graph;
    tsmui::Knob depth, detail, attack, release, maxcut, tilt, mix, trim, nodeF, nodeG, nodeQ;
    juce::TextButton soft { "SUAVE" }, hard { "DURO" }, delta { "DELTA" }, bypass { "BYPASS" }, sidechain { "SIDECHAIN" };
    juce::ComboBox presetBox, latencyBox, stereoBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> latencyAtt, stereoAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> btnAtt;
    float inM = -100, outM = -100;
    juce::TooltipWindow tips { this, 500 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopSmoothEditor)
};
