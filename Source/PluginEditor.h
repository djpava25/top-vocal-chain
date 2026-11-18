#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

namespace tvcui
{
namespace Colours
{
    const juce::Colour bg       { 0xff0d0f14 };
    const juce::Colour panel    { 0xff171a22 };
    const juce::Colour panelHi  { 0xff1f2330 };
    const juce::Colour text     { 0xffe8eaf0 };
    const juce::Colour dim      { 0xff8a90a2 };
    const juce::Colour cyan     { 0xff00d2ff };
    const juce::Colour purple   { 0xffb45cff };
    const juce::Colour orange   { 0xffff8a3d };
    const juce::Colour green    { 0xff3dffa0 };
    const juce::Colour pink     { 0xffff4f9a };
    const juce::Colour yellow   { 0xffffd23d };
}

//==============================================================================
class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int h) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getLabelFont (juce::Label&) override;
};

//==============================================================================
using APVTS = juce::AudioProcessorValueTreeState;

class Knob : public juce::Component
{
public:
    Knob (APVTS&, const juce::String& paramId, const juce::String& label, juce::Colour accent);
    void resized() override;
    juce::Slider slider;
    juce::Label name;
private:
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

class Fader : public juce::Component
{
public:
    Fader (APVTS&, const juce::String& paramId, const juce::String& label, std::atomic<float>& level);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick();
private:
    juce::Slider slider;
    juce::Label name;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
    std::atomic<float>& levelSource;
    float shown = -60.0f;
    juce::Rectangle<int> meterArea;
};

class Panel : public juce::Component
{
public:
    Panel (APVTS&, const juce::String& title, const juce::String& powerParamId, juce::Colour accent);
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::Rectangle<int> content() const { return getLocalBounds().reduced (10).withTrimmedTop (30); }
    juce::Colour accent;
protected:
    juce::String title;
    juce::TextButton power { "ON" };
    std::unique_ptr<APVTS::ButtonAttachment> powerAttachment;
};

//==============================================================================
class TunePanel : public Panel
{
public:
    TunePanel (TopVocalChainProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick() { repaint (displayArea); }
private:
    TopVocalChainProcessor& proc;
    juce::ComboBox keyBox, scaleBox;
    std::unique_ptr<APVTS::ComboBoxAttachment> keyAtt, scaleAtt;
    Knob speed, amount;
    juce::Rectangle<int> displayArea;
};

class AIPanel : public Panel
{
public:
    AIPanel (TopVocalChainProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick() { repaint (readoutArea); }
private:
    TopVocalChainProcessor& proc;
    Knob amount;
    juce::TextButton listen { "ESCUCHAR" }, resetBtn { "REINICIAR" };
    std::unique_ptr<APVTS::ButtonAttachment> listenAtt;
    juce::Rectangle<int> readoutArea;
};

class PreampPanel : public Panel
{
public:
    PreampPanel (APVTS&);
    void resized() override;
private:
    Knob drive, warmth, trim;
};

class EqPanel : public Panel
{
public:
    EqPanel (APVTS&);
    void resized() override;
private:
    Knob lowCut, low, midFreq, midGain, midQ, presence, air;
};

class CompPanel : public Panel
{
public:
    CompPanel (TopVocalChainProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick();
private:
    TopVocalChainProcessor& proc;
    Knob thr, ratio, makeup, attack, release;
    juce::Rectangle<int> grArea;
    float shownGr = 0.0f;
};

class DelayPanel : public Panel
{
public:
    DelayPanel (APVTS&);
    void resized() override;
private:
    Knob time, feedback, mix, hiCut;
    juce::TextButton sync { "SYNC" }, pingPong { "PING PONG" };
    juce::ComboBox division;
    std::unique_ptr<APVTS::ButtonAttachment> syncAtt, ppAtt;
    std::unique_ptr<APVTS::ComboBoxAttachment> divAtt;
};

class ReverbPanel : public Panel
{
public:
    ReverbPanel (APVTS&);
    void resized() override;
private:
    Knob size, damping, width, mix;
};

//==============================================================================
class MainView : public juce::Component
{
public:
    static constexpr int W = 1200, H = 720;
    MainView (TopVocalChainProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick();
    void refreshPresetBox();
private:
    TopVocalChainProcessor& proc;
    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, saveBtn { "GUARDAR" }, loadBtn { "CARGAR" };
    std::unique_ptr<juce::FileChooser> chooser;

    Fader inFader, outFader;
    TunePanel tune;
    AIPanel aiPanel;
    PreampPanel preamp;
    EqPanel eqPanel;
    CompPanel compPanel;
    DelayPanel delayPanel;
    ReverbPanel reverbPanel;
};
} // namespace tvcui

//==============================================================================
class TopVocalChainEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TopVocalChainEditor (TopVocalChainProcessor&);
    ~TopVocalChainEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    tvcui::LookAndFeel lnf;
    tvcui::MainView view;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopVocalChainEditor)
};
