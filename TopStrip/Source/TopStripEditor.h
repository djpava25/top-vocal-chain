#pragma once

#include "UI/RackWidgets.h"
#include "TopStripProcessor.h"

namespace topstripui
{
class MainView : public juce::Component
{
public:
    static constexpr int W = 1500, H = 500;
    explicit MainView (TopStripProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick();
private:
    void buildBackground();
    bool param (const char* id) const { return proc.apvts.getRawParameterValue (id)->load() > 0.5f; }
    void lamp (tvcui::LampButton&, const char* id, const juce::String& tip);

    TopStripProcessor& proc;
    juce::Image background;
    bool lastPower = true;
    float glow = 1.0f;

    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" };
    std::vector<std::unique_ptr<tvcui::APVTS::ButtonAttachment>> atts;

    // Preamp
    tvcui::HwKnob preGain, hpf;
    tvcui::LampButton mic { "MIC" }, phase { "PHASE" }, pad { "PAD -20" }, hpfIn { "HPF IN" };
    // Compresor
    tvcui::HwKnob thr, ratio, att, rel;
    tvcui::LampButton compIn { "COMP IN" }, eqPre { "EQ > COMP" }, scLink { "SC LINK" }, meterGr { "VU = GR" };
    // VU
    tvcui::VuMeter vuLevel, vuGr;
    // EQ
    tvcui::HwKnob bassG, bassF, lmG, lmF, hmG, hmF, trG, trF;
    tvcui::LampButton eqIn { "EQ IN" }, lmHiQ { "HI-Q" }, lmX10 { "X10" }, hmHiQ { "HI-Q" }, hmX10 { "X10" };
    // Salida
    tvcui::HwKnob out;
    tvcui::ToggleSwitch power;
};
} // namespace topstripui

class TopStripEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TopStripEditor (TopStripProcessor&);
    ~TopStripEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void tickNow() { view.tick(); }
private:
    void timerCallback() override { view.tick(); }
    tvcui::LookAndFeel lnf;
    topstripui::MainView view;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopStripEditor)
};
