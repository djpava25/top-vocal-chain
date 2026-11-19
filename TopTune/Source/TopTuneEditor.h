#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "TopTuneProcessor.h"

namespace ttui
{
namespace C
{
    const juce::Colour bg      { 0xff0b0d12 };
    const juce::Colour panel   { 0xff151821 };
    const juce::Colour panelHi { 0xff1e2230 };
    const juce::Colour text    { 0xffeef0f5 };
    const juce::Colour dim     { 0xff878da0 };
    const juce::Colour accent  { 0xff2ee6a6 };   // verde menta
    const juce::Colour accent2 { 0xffffb238 };   // ambar
    const juce::Colour blue    { 0xff4aa8ff };
}

using APVTS = juce::AudioProcessorValueTreeState;

class LnF : public juce::LookAndFeel_V4
{
public:
    LnF();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getLabelFont (juce::Label&) override;
};

class Knob : public juce::Component
{
public:
    Knob (APVTS&, const juce::String& id, const juce::String& label, juce::Colour col);
    void resized() override;
private:
    juce::Slider slider;
    juce::Label name;
    std::unique_ptr<APVTS::SliderAttachment> att;
};

class PitchGraph : public juce::Component
{
public:
    explicit PitchGraph (TopTuneProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
private:
    TopTuneProcessor& proc;
    float centre = 60.0f;
};

class ScaleKeyboard : public juce::Component
{
public:
    explicit ScaleKeyboard (TopTuneProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    int hitTest (juce::Point<float>) const;
    juce::Rectangle<float> keyRect (int pc) const;
    TopTuneProcessor& proc;
};

class MainView : public juce::Component
{
public:
    static constexpr int W = 960, H = 600;
    explicit MainView (TopTuneProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick();
private:
    TopTuneProcessor& proc;
    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" };

    PitchGraph graph;
    ScaleKeyboard keyboard;

    juce::ComboBox keyBox, scaleBox, styleBox, midiBox;
    juce::Label keyLbl, scaleLbl, styleLbl, midiLbl;
    std::unique_ptr<APVTS::ComboBoxAttachment> keyAtt, scaleAtt, styleAtt, midiAtt;
    juce::TextButton liveBtn { "MODO LIVE" };
    std::unique_ptr<APVTS::ButtonAttachment> liveAtt;

    juce::TextButton detectBtn { "DETECTAR TONALIDAD" }, applyBtn { "APLICAR" };
    juce::String keyResult { "Canta unos segundos..." };
    int foundRoot = -1; bool foundMinor = false;
    juce::Rectangle<int> keyResultArea;

    Knob speed, amount, shift, fine, formant, mix, out;
};
} // namespace ttui

class TopTuneEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TopTuneEditor (TopTuneProcessor&);
    ~TopTuneEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override { view.tick(); }
    ttui::LnF lnf;
    ttui::MainView view;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopTuneEditor)
};
