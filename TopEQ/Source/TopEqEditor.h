#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "TopEqProcessor.h"

namespace teqi
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
    Knob (APVTS&, const juce::String& id, bool selector, float a0, float a1);
    bool selector;
private:
    std::unique_ptr<APVTS::SliderAttachment> att;
};
class Curve : public juce::Component
{
public:
    explicit Curve (TopEqProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
private:
    TopEqProcessor& proc;
};
class Switch : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit Switch (TopEqProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    TopEqProcessor& proc;
};
class Content : public juce::Component
{
public:
    explicit Content (TopEqProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    void tick();
    static constexpr int W = 1200, H = 470;
private:
    TopEqProcessor& proc;
    Knob lowBoost, lowAtten, lowFreq, highBoost, bandwidth, highFreq, highAtten, attFreq, tube, out;
    Curve curve;
    Switch power;
    teq::Settings last; float lastPower = -1, lastOut = 99;
    juce::Rectangle<float> presetArea { 860, 26, 270, 30 };
};
} // namespace teqi

class TopEqEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TopEqEditor (TopEqProcessor&);
    ~TopEqEditor() override;
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colours::black); }
    void tickNow() { timerCallback(); }
    static constexpr int W = teqi::Content::W, H = teqi::Content::H;
private:
    void timerCallback() override { content.tick(); }
    teqi::Look look;
    teqi::Content content;
    juce::TooltipWindow tips { this, 600 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopEqEditor)
};
