#pragma once

#include "UI/RackWidgets.h"
#include "PluginProcessor.h"

namespace tvcui
{
//==============================================================================
class MainView : public juce::Component
{
public:
    static constexpr int W = 1400, H = 900;
    explicit MainView (TopVocalChainProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick();
    void refreshPresetBox();

private:
    void buildBackground();
    void drawTuneDisplay (juce::Graphics&);
    void drawAiDisplay (juce::Graphics&);
    bool param (const char* id) const { return proc.apvts.getRawParameterValue (id)->load() > 0.5f; }

    TopVocalChainProcessor& proc;
    juce::Image background;

    // Barra superior
    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, saveBtn { "GUARDAR" }, loadBtn { "CARGAR" };
    std::unique_ptr<juce::FileChooser> chooser;

    // Selectores y switches
    juce::ComboBox preType, compType, eqType, keyBox, scaleBox, divBox;
    std::unique_ptr<APVTS::ComboBoxAttachment> preTypeAtt, compTypeAtt, eqTypeAtt, keyAtt, scaleAtt, divAtt;

    LampButton preOn { "PRE IN" };
    ToggleSwitch compOn, eqOn, tuneOn, aiListen, dlyOn, dlySync, dlyPing, revOn;
    std::vector<std::unique_ptr<APVTS::ButtonAttachment>> buttonAtts;
    juce::TextButton aiReset { "RESET" };

    Led compLed, eqLed, tuneLed, aiLed, dlyLed, syncLed, pingLed, revLed;
    VuMeter vu, grMeter;

    // Perillas
    HwKnob drive, inGain, warmth, trim;
    HwKnob thr, ratio, attack, release, makeup;
    HwKnob air, presence, midQ, midFreq, midGain, low, lowCut;
    HwKnob output;
    HwKnob tuneSpeed, tuneAmount;
    HwKnob aiAmount;
    HwKnob dTime, dFeedback, dMix, dHiCut;
    HwKnob rSize, rDamp, rWidth, rMix;

    juce::Rectangle<int> tuneDisplay, aiDisplay;
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
