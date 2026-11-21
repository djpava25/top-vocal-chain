#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <complex>
#include "TopConsoleBProcessor.h"

namespace tcbui
{
using APVTS = juce::AudioProcessorValueTreeState;

class Look : public juce::LookAndFeel_V4
{
public:
    Look();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    int getSliderThumbRadius (juce::Slider& s) override { return s.isVertical() ? 36 : LookAndFeel_V4::getSliderThumbRadius (s); }
};

// perilla de consola: tapa de color, faldon moleteado y escala serigrafiada
class Knob : public juce::Component
{
public:
    Knob (APVTS&, const juce::String& id, const juce::String& title, juce::Colour cap, float radius,
          const juce::String& minText, const juce::String& maxText, int ticks = 11);
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::Slider slider;
    juce::String title;
private:
    juce::String minT, maxT;
    std::unique_ptr<APVTS::SliderAttachment> att;
};

// interruptor iluminado
class LightButton : public juce::Button
{
public:
    LightButton (APVTS&, const juce::String& id, const juce::String& label, juce::Colour led);
    void paintButton (juce::Graphics&, bool over, bool down) override;
private:
    juce::Colour led;
    std::unique_ptr<APVTS::ButtonAttachment> att;
};

// medidor de LEDs vertical
class LedMeter : public juce::Component
{
public:
    LedMeter (std::function<float()> peakDb, std::function<float()> rmsDb, bool reduction);
    void paint (juce::Graphics&) override;
    void tick();
private:
    std::function<float()> peak, rms;
    bool gr;
    float pk = -100, rm = -100, hold = -100; int holdT = 0;
};

class EqGraph : public juce::Component
{
public:
    explicit EqGraph (TopConsoleBProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
private:
    TopConsoleBProcessor& proc;
};
} // namespace tcbui

class TopConsoleBEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TopConsoleBEditor (TopConsoleBProcessor&);
    ~TopConsoleBEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void tickNow() { timerCallback(); }
    static constexpr int W = 1380, H = 660;
private:
    void timerCallback() override;
    void buildBackground();
    TopConsoleBProcessor& proc;
    tcbui::Look look;
    juce::Image background;
    juce::OwnedArray<tcbui::Knob> knobs;
    juce::OwnedArray<tcbui::LightButton> buttons;
    tcbui::EqGraph graph;
    std::unique_ptr<tcbui::LedMeter> inMeter, outMeter, compMeter, gateMeter;
    juce::Slider fader;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> faderAtt;
    juce::ComboBox presetBox;
    juce::TooltipWindow tips { this, 500 };
    tcbui::Knob* add (const juce::String& id, const juce::String& title, juce::Colour cap, float r, const juce::String& a, const juce::String& b, int x, int y, int ticks = 11);
    tcbui::LightButton* addBtn (const juce::String& id, const juce::String& label, juce::Colour led, int x, int y, int w = 54);
    std::vector<std::pair<juce::Component*, juce::Rectangle<int>>> layout;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopConsoleBEditor)
};
