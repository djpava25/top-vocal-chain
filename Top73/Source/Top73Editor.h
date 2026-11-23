#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Top73Processor.h"

namespace t73
{
using APVTS = juce::AudioProcessorValueTreeState;
enum Kind { Red, Gray, Ring, Blue, Level };
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
    Knob (APVTS&, const juce::String& id, Kind k, float a0, float a1, float innerHole = 0.0f);
    bool hitTest (int x, int y) override;
    Kind kind; float hole;
private:
    std::unique_ptr<APVTS::SliderAttachment> att;
};
class Button : public juce::Component, public juce::SettableTooltipClient
{
public:
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent& e) override { if (getLocalBounds().contains (e.getPosition()) && onClick) onClick(); }
    std::function<void()> onClick;
    std::function<bool()> isOn;
    juce::String text;
};
class Content : public juce::Component
{
public:
    explicit Content (Top73Processor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    void tick();
    static constexpr int W = 1500, H = 300;
private:
    void presetMenu();
    Top73Processor& proc;
    Knob gain, hf, midFreq, midGain, lowFreq, lowGain, hpf, level;
    Button autoBtn, presetBtn, phaseBtn, eqBtn, powerBtn;
    float inDb = -60, outDb = -60;
    n73::Settings last; float lastBits = -1;
    juce::Rectangle<float> meterSel { 1168, 100, 90, 32 };
};
} // namespace t73

class Top73Editor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit Top73Editor (Top73Processor&);
    ~Top73Editor() override;
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colours::black); }
    void tickNow() { for (int i = 0; i < 20; ++i) timerCallback(); }
    static constexpr int W = t73::Content::W, H = t73::Content::H;
private:
    void timerCallback() override { content.tick(); }
    t73::Look look;
    t73::Content content;
    juce::TooltipWindow tips { this, 600 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Top73Editor)
};
