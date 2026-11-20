#pragma once

#include "UI/RackWidgets.h"
#include "TopCompProcessor.h"

namespace topcompui
{
// Dibujo "hardware": perilla negra estriada grande y perilla de puntero (chicken-head)
class HwLook : public tvcui::LookAndFeel
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float start, float end, juce::Slider&) override;
    static void flutedKnob (juce::Graphics&, juce::Point<float> c, float r, float angle);
    static void pointerKnob (juce::Graphics&, juce::Point<float> c, float r, float angle);
};

// Perilla con escala de puntos blancos serigrafiada alrededor y titulo debajo
class PanelKnob : public juce::Component
{
public:
    struct Mark { float value; juce::String text; };
    PanelKnob (tvcui::APVTS&, const juce::String& paramId, const juce::String& title,
               std::vector<Mark> marks, bool pointer, float radius,
               float startAngle, float endAngle, const juce::String& subTitle = {});
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::Point<float> knobCentre() const;
    juce::Slider slider;
private:
    juce::String title, subTitle;
    std::vector<Mark> marks;
    bool pointer;
    float radius;
    std::unique_ptr<tvcui::APVTS::SliderAttachment> attachment;
};

// VU de aguja con cara crema retroiluminada
class PanelVu : public juce::Component
{
public:
    explicit PanelVu (TopCompProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void tick();
private:
    TopCompProcessor& proc;
    float needle = 0.686f;
};

// Palanca IN cromada
class ChromeToggle : public juce::Button
{
public:
    ChromeToggle() : juce::Button ({}) { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
};

class MainView : public juce::Component
{
public:
    static constexpr int W = 1400, H = 444, top = 38;
    explicit MainView (TopCompProcessor&);
    ~MainView() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick();
private:
    void buildBackground();
    TopCompProcessor& proc;
    HwLook look;
    juce::Image background;
    PanelKnob gain, ratio, thr, meter, attack, release, select, hpf, mode, mix, color;
    ChromeToggle power;
    PanelVu vu;
    juce::ComboBox presetBox;
    std::unique_ptr<tvcui::APVTS::ButtonAttachment> powerAtt;
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
