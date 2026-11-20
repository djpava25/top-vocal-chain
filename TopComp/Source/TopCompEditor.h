#pragma once

#include "UI/RackWidgets.h"
#include "TopCompProcessor.h"

namespace topcompui
{
class MainView : public juce::Component
{
public:
    static constexpr int W = 1200, H = 400;
    explicit MainView (TopCompProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick();
private:
    void buildBackground();
    TopCompProcessor& proc;
    juce::Image background;
    tvcui::HwKnob gain, ratio, thr, mix, attack, release, select, hpf, color;
    tvcui::ToggleSwitch power;
    tvcui::VuMeter vuGr, vuOut;
    juce::OwnedArray<juce::TextButton> modes;
    juce::TextButton vuGrBtn { "GR" }, vuOutBtn { "OUT" };
    juce::ComboBox presetBox;
    std::vector<std::unique_ptr<tvcui::APVTS::ButtonAttachment>> atts;
    bool lastPower = true;
};
} // namespace topcompui

class TopCompEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TopCompEditor (TopCompProcessor&);
    ~TopCompEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void tickNow() { view.tick(); }
private:
    void timerCallback() override { view.tick(); }
    tvcui::LookAndFeel lnf;
    topcompui::MainView view;
    juce::TooltipWindow tips { this, 500 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopCompEditor)
};
