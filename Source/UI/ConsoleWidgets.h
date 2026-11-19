#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

// Widgets estilo tira de consola (diseno original de Top Vocal Chain):
// paneles marfil, cabeceras grafito, perillas con tapa de color, botones
// iluminados, medidores LED de segmentos y fader de salida.
namespace cui
{
namespace Col
{
    const juce::Colour backdrop  { 0xff141518 };
    const juce::Colour panel     { 0xffe6e1d4 };
    const juce::Colour panelTint { 0xffd9d3c3 };
    const juce::Colour sage      { 0xffd3dbcf };
    const juce::Colour sand      { 0xffe2d6bd };
    const juce::Colour header    { 0xff26282c };
    const juce::Colour ink       { 0xff23252a };
    const juce::Colour inkDim    { 0xff62646b };
    const juce::Colour cream     { 0xfff3eee2 };
    const juce::Colour red       { 0xffc6402d };
    const juce::Colour orange    { 0xffd8772a };
    const juce::Colour green     { 0xff3d8a58 };
    const juce::Colour blue      { 0xff3b67a8 };
    const juce::Colour purple    { 0xff7a56a6 };
    const juce::Colour mustard   { 0xffc39f2a };
    const juce::Colour slate     { 0xff56677a };
    const juce::Colour charcoal  { 0xff3b3d42 };
    const juce::Colour amber     { 0xffffb03a };
    const juce::Colour ledGreen  { 0xff46e07a };
}

juce::Font font (float h, bool bold = false);
void panelTexture (juce::Graphics&, juce::Rectangle<float>, juce::Colour, int seed);
void smallScrew (juce::Graphics&, float cx, float cy, float r);
void lcd (juce::Graphics&, juce::Rectangle<float>);
void lcdText (juce::Graphics&, const juce::String&, juce::Rectangle<float>, float size, bool bold,
              juce::Justification, juce::Colour = Col::amber);
void moduleFrame (juce::Graphics&, juce::Rectangle<float>, const juce::String& title, int seed);

//==============================================================================
class LnF : public juce::LookAndFeel_V4
{
public:
    LnF();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getSliderPopupFont (juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
};

using APVTS = juce::AudioProcessorValueTreeState;

// Perilla de consola: tapa de color, escala alrededor, nombre debajo
class Knob : public juce::Component
{
public:
    Knob (APVTS&, const juce::String& id, const juce::String& title, juce::Colour cap,
          const juce::String& minL, const juce::String& midL, const juce::String& maxL);
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::Slider slider;
private:
    juce::String title;
    std::unique_ptr<APVTS::SliderAttachment> att;
};

// Boton cuadrado iluminado (IN, SYNC, modos...)
class LitButton : public juce::Button
{
public:
    LitButton (const juce::String& text, juce::Colour light) : juce::Button (text), col (light) {}
    void paintButton (juce::Graphics&, bool over, bool down) override;
private:
    juce::Colour col;
};

// Medidor LED vertical (nivel o reduccion de ganancia)
class LedMeter : public juce::Component
{
public:
    LedMeter (std::function<float()> src, bool grMode) : source (std::move (src)), gr (grMode), value (grMode ? 0.0f : -100.0f) {}
    void paint (juce::Graphics&) override;
    void tick();
    static float levelToProp (float db);
private:
    std::function<float()> source;
    bool gr;
    float value, peak = -100.0f;
    int peakHold = 0;
};

// Fader de salida con escala
class Fader : public juce::Component
{
public:
    Fader (APVTS&, const juce::String& id);
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    juce::Slider slider;
    std::unique_ptr<APVTS::SliderAttachment> att;
};
} // namespace cui
