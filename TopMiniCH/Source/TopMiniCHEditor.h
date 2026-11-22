#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "TopMiniCHProcessor.h"

namespace tmcui
{
using APVTS = juce::AudioProcessorValueTreeState;
class Look : public juce::LookAndFeel_V4
{
public:
    Look();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawBubble (juce::Graphics&, juce::BubbleComponent&, const juce::Point<float>&, const juce::Rectangle<float>&) override;
};
class Knob : public juce::Slider
{
public:
    Knob (APVTS&, const juce::String& id, juce::Colour cap);
private:
    std::unique_ptr<APVTS::SliderAttachment> att;
};
class Key : public juce::Button
{
public:
    Key (APVTS&, const juce::String& id, const juce::String& symbol = {});
    void paintButton (juce::Graphics&, bool over, bool down) override;
private:
    juce::String symbol;
    std::unique_ptr<APVTS::ButtonAttachment> att;
};
} // namespace tmcui

class TopMiniCHEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TopMiniCHEditor (TopMiniCHProcessor&);
    ~TopMiniCHEditor() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    void tickNow() { timerCallback(); }
    static constexpr int W = 270, H = 1000;
private:
    void timerCallback() override;
    void buildBackground();
    TopMiniCHProcessor& proc;
    tmcui::Look look;
    tmcui::Knob gain, hf, lf, thr;
    tmcui::Key phase, color, hpf, eqIn, hfBell, lfBell, cmp, line, hiz;
    juce::Image background;
    float meterDb = -100.0f, grShown = 0.0f;
    juce::TooltipWindow tips { this, 500 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopMiniCHEditor)
};
