#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// TOP VOCAL CHAIN V3: cuatro modulos de color (Dynamics, Tone, Space, SFX) con paneles de detalle
namespace tvc3
{
class LnF : public juce::LookAndFeel_V4
{
public:
    LnF();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawPopupMenuBackground (juce::Graphics&, int, int) override;
};

class MainView;

class EditorV3 : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit EditorV3 (TopVocalChainProcessor&);
    ~EditorV3() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void tickNow();
private:
    void timerCallback() override { tickNow(); }
    LnF lnf;
    std::unique_ptr<MainView> view;
    juce::TooltipWindow tips { this, 600 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditorV3)
};
} // namespace tvc3
