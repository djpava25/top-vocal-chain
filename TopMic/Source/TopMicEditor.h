#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "TopMicProcessor.h"

namespace tmicui
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
    Knob (APVTS&, const juce::String& id, bool stepped = false);
private:
    std::unique_ptr<APVTS::SliderAttachment> att;
};
class Toggle : public juce::Button
{
public:
    Toggle (APVTS&, const juce::String& id, const juce::String& label, int icon = 0);
    void paintButton (juce::Graphics&, bool over, bool down) override;
private:
    int icon;
    std::unique_ptr<APVTS::ButtonAttachment> att;
};
// boton de texto/visor que abre un menu
class Display : public juce::Component, public juce::SettableTooltipClient
{
public:
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent& e) override { if (getLocalBounds().contains (e.getPosition()) && onClick) onClick(); }
    std::function<void()> onClick;
    juce::String text; float fontSize = 22.0f; bool small = false;
};
class MicView : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit MicView (TopMicProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    TopMicProcessor& proc;
};
class PolarView : public juce::Component
{
public:
    explicit PolarView (TopMicProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
private:
    TopMicProcessor& proc;
};
void drawMic (juce::Graphics&, juce::Rectangle<float>, int model);
} // namespace tmicui

class TopMicEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TopMicEditor (TopMicProcessor&);
    ~TopMicEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void tickNow() { timerCallback(); }
    static constexpr int W = 900, H = 600;
private:
    void timerCallback() override;
    void modelMenu (const juce::String& id, juce::Component& target);
    TopMicProcessor& proc;
    tmicui::Look look;
    tmicui::MicView mic;
    tmicui::PolarView polar;
    tmicui::Display nameBox, sourceBox, micBBox, presetBox;
    tmicui::Knob pattern, filter, axis, prox, out, blend;
    tmicui::Toggle rev, phase, power, dual;
    float meter = 0.0f;
    juce::TooltipWindow tips { this, 500 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopMicEditor)
};
