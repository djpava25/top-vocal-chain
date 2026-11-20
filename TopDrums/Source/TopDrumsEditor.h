#pragma once

#include "UI/RackWidgets.h"
#include "TopDrumsProcessor.h"

namespace topdrumsui
{
class MainView : public juce::Component
{
public:
    static constexpr int W = 1500, H = 660;
    explicit MainView (TopDrumsProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick();
private:
    void buildBackground();
    bool param (const char* id) const { return proc.apvts.getRawParameterValue (id)->load() > 0.5f; }
    void lamp (tvcui::LampButton&, const char* id, const juce::String& tip);
    void askForKey();

    TopDrumsProcessor& proc;
    juce::Image background;

    juce::ComboBox presetBox, focusBox, genreBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" };
    std::vector<std::unique_ptr<tvcui::APVTS::ButtonAttachment>> atts;
    std::vector<std::unique_ptr<tvcui::APVTS::ComboBoxAttachment>> boxAtts;

    tvcui::HwKnob attack, sustain, speed, weight, drive, tone, thr, ratio, gAtt, gRel, makeup, gMix, scHpf, out, mix;
    tvcui::LampButton clip { "CLIP" }, glueIn { "GLUE IN" };
    tvcui::ToggleSwitch power;
    tvcui::VuMeter vuGr, vuOut;

    // IA
    juce::TextButton analyse { "ANALIZAR DRUMS" }, applyLocal { "APLICAR IA" }, askBtn { "PREGUNTAR A CLAUDE" },
                     applyClaude { "APLICAR CLAUDE" }, keyBtn { "CLAVE API" };
    juce::String aiText, claudeTextShown;
    bool lastReady = false;
    TopDrumsProcessor::AskState lastAsk = TopDrumsProcessor::AskState::idle;
    float transShown = 0.0f;
};
} // namespace topdrumsui

class TopDrumsEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TopDrumsEditor (TopDrumsProcessor&);
    ~TopDrumsEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void tickNow() { view.tick(); }
private:
    void timerCallback() override { view.tick(); }
    tvcui::LookAndFeel lnf;
    topdrumsui::MainView view;
    juce::TooltipWindow tooltips { this, 500 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopDrumsEditor)
};
