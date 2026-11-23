#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "VbxProcessor.h"

namespace vbui
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
    Knob (APVTS&, const juce::String& id, float a0, float a1, bool popup = true);
private:
    std::unique_ptr<APVTS::SliderAttachment> att;
};
// palanca cromada (2 o 3 posiciones)
class Toggle : public juce::Component, public juce::SettableTooltipClient
{
public:
    Toggle (VbxProcessor& p, const char* paramId, int positions, bool invert = false) : proc (p), id (paramId), n (positions), inv (invert) {}
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool decorative = false;
private:
    VbxProcessor& proc; const char* id; int n; bool inv;
};
class Content : public juce::Component
{
public:
    explicit Content (VbxProcessor&);
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    void tick();
    static constexpr int W = 1500, H = 430;
private:
    VbxProcessor& proc;
    Knob input, gain, meterSel, thr, attack, release, dsFreq, dsThr, low, lowF, mid, midF, hi, hiF, power, out;
    Toggle phantom, hpf, phase, link, comp, deess, eq, xfmr;
    float vu = -20.0f, vuShown = -20.0f;
    juce::Rectangle<float> sticker { 1352, 148, 100, 26 };
    float lastBits = -1; juce::String lastPreset;
};
} // namespace vbui

class VbxEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VbxEditor (VbxProcessor&);
    ~VbxEditor() override;
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colours::black); }
    void tickNow() { for (int i = 0; i < 40; ++i) timerCallback(); }
    static constexpr int W = vbui::Content::W, H = vbui::Content::H;
private:
    void timerCallback() override { content.tick(); }
    vbui::Look look;
    vbui::Content content;
    juce::TooltipWindow tips { this, 600 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VbxEditor)
};
