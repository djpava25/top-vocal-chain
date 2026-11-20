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
    inline juce::Colour panel     { 0xffe6e1d4 };
    inline juce::Colour panelTint { 0xffd9d3c3 };
    inline juce::Colour sage      { 0xffd3dbcf };
    inline juce::Colour sand      { 0xffe2d6bd };
    const juce::Colour header    { 0xff26282c };
    inline juce::Colour ink       { 0xff23252a };
    inline juce::Colour inkDim    { 0xff62646b };
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
// Tema: 0 = Hardware (paneles claros), 1 = Oscuro
void setTheme (int theme);
int getTheme();
void panelTexture (juce::Graphics&, juce::Rectangle<float>, juce::Colour, int seed);
void smallScrew (juce::Graphics&, float cx, float cy, float r);
void lcd (juce::Graphics&, juce::Rectangle<float>);
void lcdText (juce::Graphics&, const juce::String&, juce::Rectangle<float>, float size, bool bold,
              juce::Justification, juce::Colour = Col::amber);
void moduleFrame (juce::Graphics&, juce::Rectangle<float>, const juce::String& title, int seed);
void drawLogo (juce::Graphics&, juce::Rectangle<float> area);

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
    void drawTooltip (juce::Graphics&, const juce::String&, int w, int h) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String&, juce::Point<int>, juce::Rectangle<int>) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
};

using APVTS = juce::AudioProcessorValueTreeState;

class Knob;
// La ventana del plugin implementa esto para el MIDI Learn de las perillas (cada instancia la suya)
struct MidiHost
{
    virtual ~MidiHost() = default;
    virtual int ccForParam (const juce::String& paramId) = 0;
    virtual void showKnobMenu (Knob&) = 0;
};

// Slider de perilla: doble clic = escribir el valor exacto, Alt+clic = valor por defecto,
// Shift (o Ctrl) al arrastrar = ajuste fino, rueda del mouse = ajuste fino
class KnobSlider : public juce::Slider
{
public:
    std::function<void()> onTypeValue, onRightClick;
    void mouseDoubleClick (const juce::MouseEvent&) override { if (onTypeValue) onTypeValue(); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu() && onRightClick) { onRightClick(); return; }
        juce::Slider::mouseDown (e);
    }
};

// Perilla de consola: aro metalico, anillo LED segun el valor, tapa de color, escala y nombre
class Knob : public juce::Component, private juce::Slider::Listener
{
public:
    Knob (APVTS&, const juce::String& id, const juce::String& title, juce::Colour cap,
          const juce::String& minL, const juce::String& midL, const juce::String& maxL);
    ~Knob() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void setDescription (const juce::String& d);
    const juce::String& getParamId() const { return paramId; }
    KnobSlider slider;

private:
    void sliderValueChanged (juce::Slider*) override { repaint (getLocalBounds().removeFromBottom (18)); }
    void sliderDragStarted (juce::Slider*) override { repaint(); }
    void sliderDragEnded (juce::Slider*) override { repaint(); }
    void showEditor();
    juce::String title, paramId;
    APVTS& state;
    std::unique_ptr<APVTS::SliderAttachment> att;
    std::unique_ptr<juce::TextEditor> editor;
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

// Interruptor IN de cada modulo: verde = activo, gris = apagado, naranja = saturando
class ModuleSwitch : public juce::Button
{
public:
    explicit ModuleSwitch (std::function<int()> statusFn) : juce::Button ("IN"), status (std::move (statusFn)) {}
    void paintButton (juce::Graphics&, bool over, bool down) override;
    void tick() { const int s = status(); if (s != last) { last = s; repaint(); } }
private:
    std::function<int()> status;   // 0 = apagado, 1 = activo, 2 = saturando
    int last = -1;
};

// Medidor LED vertical (nivel o reduccion de ganancia)
class LedMeter : public juce::Component
{
public:
    LedMeter (std::function<float()> src, bool grMode) : source (std::move (src)), gr (grMode), value (grMode ? 0.0f : -100.0f) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { peak = -100.0f; peakHold = 0; repaint(); }   // clic = reset del pico
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
