#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "TopEqProcessor.h"

// Panel estilo hardware: placa azul pizarra martillada, perillas de baquelita estriadas,
// selectores "cabeza de pollo", palanca IN y lampara de joya roja que brilla con la senal.
// Diseno y nombres propios (no se copia ninguna marca ni logo).
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
enum class KnobKind { fluted, chicken, mini };
class Knob : public juce::Slider
{
public:
    Knob (APVTS&, const juce::String& id, KnobKind, float a0, float a1);
    KnobKind kind;
private:
    std::unique_ptr<APVTS::SliderAttachment> att;
};
class Toggle : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit Toggle (TopEqProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    TopEqProcessor& proc;
};
class Jewel : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit Jewel (TopEqProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    float glow = 0;
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
    static constexpr int W = 1560, H = 560, strip = 54;
private:
    void buildPanel();
    TopEqProcessor& proc;
    Knob lowBoost, lowAtten, lowFreq, highBoost, bandwidth, highFreq, highAtten, attFreq, drive, out;
    Toggle power;
    Jewel lamp;
    juce::Image panel;
    int lastProgram = -1; float lastPower = -1;
    juce::Rectangle<float> presetArea { 600, 12, 360, 30 }, prevArea { 562, 12, 34, 30 }, nextArea { 964, 12, 34, 30 };
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
