#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

// Interfaz estilo rack analogico: metal cepillado, perillas plateadas con escala
// grabada, LEDs ambar, switches de palanca, VU meter de aguja y placa metalica.
namespace tvcui
{
namespace Colours
{
    const juce::Colour rack       { 0xff0e1014 };
    const juce::Colour metalTop   { 0xff4b5566 };
    const juce::Colour metalBot   { 0xff2c3340 };
    const juce::Colour engrave    { 0xffe9e5da };   // texto grabado crema
    const juce::Colour engraveDim { 0xffb9b6ad };
    const juce::Colour amber      { 0xffffb03a };
    const juce::Colour amberDim   { 0xff4a3416 };
    const juce::Colour red        { 0xffff3a2a };
    const juce::Colour vuFace     { 0xfff1e7c6 };
}

juce::Font serif (float height, bool bold = false);

//==============================================================================
class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int h) override;
    juce::Font getSliderPopupFont (juce::Slider&) override;
    juce::Font getPopupMenuFont() override;
};

using APVTS = juce::AudioProcessorValueTreeState;

//==============================================================================
// Perilla de hardware: titulo grabado arriba, escala con marcas y etiquetas
class HwKnob : public juce::Component
{
public:
    HwKnob (APVTS&, const juce::String& paramId, const juce::String& title,
            const juce::String& minLabel, const juce::String& midLabel, const juce::String& maxLabel);
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::Slider slider;
private:
    juce::String title;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

// Switch de palanca metalico
class ToggleSwitch : public juce::Button
{
public:
    ToggleSwitch() : juce::Button ({}) { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
};

// Boton con lampara roja (como el de 48V)
class LampButton : public juce::Button
{
public:
    explicit LampButton (const juce::String& text) : juce::Button (text) { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
};

// LED ambar
class Led : public juce::Component
{
public:
    explicit Led (std::function<bool()> s) : state (std::move (s)) {}
    void paint (juce::Graphics&) override;
    void tick() { const bool v = state(); if (v != last) { last = v; repaint(); } }
private:
    std::function<bool()> state;
    bool last = false;
};

// VU meter de aguja (modo nivel o modo reduccion de ganancia)
class VuMeter : public juce::Component
{
public:
    VuMeter (std::function<float()> source, bool grMode) : src (std::move (source)), gr (grMode), needle (grMode ? 1.0f : 0.0f) {}
    void paint (juce::Graphics&) override;
    void tick();
private:
    std::function<float()> src;
    bool gr;
    float needle;   // 0..1
};

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
