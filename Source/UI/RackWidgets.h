#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

// Widgets compartidos estilo rack analogico (Top Vocal Chain, Top Summing):
// metal cepillado, perillas plateadas con escala, switches, LEDs, VU de aguja.
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
juce::Font sans (float height, bool bold = false);
void engraved (juce::Graphics&, const juce::String&, juce::Rectangle<float>, float size,
               juce::Justification = juce::Justification::centred, bool bold = false,
               juce::Colour = Colours::engrave);
void drawScrew (juce::Graphics&, float cx, float cy, float r, float angle = 0.6f);
void brushed (juce::Graphics&, juce::Rectangle<float>, juce::Colour top, juce::Colour bot, int seed, float strength = 1.0f);
void lcdFrame (juce::Graphics&, juce::Rectangle<float>);
void glowText (juce::Graphics&, const juce::String&, juce::Rectangle<float>, juce::Font,
               juce::Justification, juce::Colour = Colours::amber);

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

} // namespace tvcui
