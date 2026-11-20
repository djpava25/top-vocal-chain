#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// TOP VOCAL CHAIN V2: interfaz oscura neon con macros de un clic sobre el mismo motor de sonido
namespace tvc2
{
using APVTS = juce::AudioProcessorValueTreeState;

// Idioma de la interfaz (0 = espanol, 1 = ingles)
juce::String T (const char* es, const char* en);

struct Palette
{
    static inline const juce::Colour bg { 0xff0d0d11 }, panel { 0xff16161c }, card { 0xff1b1b22 }, cardHi { 0xff22222b },
                                     border { 0xff2b2b35 }, text { 0xffeceaf2 }, dim { 0xff8d8b98 }, faint { 0xff4a4955 },
                                     neonA { 0xffa65bff }, neonB { 0xffff4fc8 }, lcd { 0xff0a0a0d };
};

class LnF : public juce::LookAndFeel_V4
{
public:
    LnF();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawPopupMenuBackground (juce::Graphics&, int, int) override;
    void drawCallOutBoxBackground (juce::CallOutBox&, juce::Graphics&, const juce::Path&, juce::Image&) override;
};

class MainView;

class EditorV2 : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit EditorV2 (TopVocalChainProcessor&);
    ~EditorV2() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void tickNow();
    void openAssistForTest();
private:
    void timerCallback() override { tickNow(); }
    LnF lnf;
    std::unique_ptr<MainView> view;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditorV2)
};
} // namespace tvc2
