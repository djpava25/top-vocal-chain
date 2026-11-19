#pragma once

#include "UI/RackWidgets.h"
#include "TopSummingProcessor.h"

namespace topsumui
{
class MainView : public juce::Component
{
public:
    static constexpr int W = 1200, H = 480;
    explicit MainView (TopSummingProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick();
private:
    void buildBackground();
    bool param (const char* id) const { return proc.apvts.getRawParameterValue (id)->load() > 0.5f; }

    TopSummingProcessor& proc;
    juce::Image background;

    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, newUnitBtn { "NUEVA UNIDAD" };

    tvcui::LampButton sumIn { "SUM IN" };
    tvcui::ToggleSwitch busSw, analogSw, noiseSw;
    std::vector<std::unique_ptr<tvcui::APVTS::ButtonAttachment>> atts;
    tvcui::Led busLed, analogLed, noiseLed;
    tvcui::VuMeter vu;
    tvcui::HwKnob drive, iron, width, crosstalk, output;
};
} // namespace topsumui

class TopSummingEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TopSummingEditor (TopSummingProcessor&);
    ~TopSummingEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override { view.tick(); }
    tvcui::LookAndFeel lnf;
    topsumui::MainView view;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopSummingEditor)
};
