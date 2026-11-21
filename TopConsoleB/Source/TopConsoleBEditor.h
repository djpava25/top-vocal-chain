#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <complex>
#include "TopConsoleBProcessor.h"

namespace tcbui
{
using APVTS = juce::AudioProcessorValueTreeState;

class Look : public juce::LookAndFeel_V4
{
public:
    Look();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawBubble (juce::Graphics&, juce::BubbleComponent&, const juce::Point<float>&, const juce::Rectangle<float>&) override;
    juce::Font getSliderPopupFont (juce::Slider&) override;
    int getSliderPopupPlacement (juce::Slider&) override { return juce::BubbleComponent::above; }
};

// perilla de consola (tapa gris o de color); la escala serigrafiada se dibuja en el fondo
class Knob : public juce::Slider
{
public:
    Knob (APVTS&, const juce::String& id, juce::Colour cap, bool coloured);
    juce::RangedAudioParameter* param = nullptr;
private:
    std::unique_ptr<APVTS::SliderAttachment> att;
};

// boton de consola: gris apagado, iluminado (rojo/ambar) encendido
class PushButton : public juce::Button
{
public:
    enum Style { grey, lit, big, flat, power };
    PushButton (APVTS&, const juce::String& id, const juce::String& label, Style, juce::Colour led = juce::Colour (0xffff4a3c));
    void paintButton (juce::Graphics&, bool over, bool down) override;
    juce::Colour textCol { 0xff1d1d1f };
private:
    Style style; juce::Colour led;
    std::unique_ptr<APVTS::ButtonAttachment> att;
};

// boton de texto de las barras
class TextButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    TextButton (const juce::String& t, juce::Colour c, float size = 12.0f) : text (t), col (c), fontSize (size) {}
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent& e) override { if (getLocalBounds().contains (e.getPosition()) && onClick) onClick(); }
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }
    std::function<void()> onClick;
    juce::String text; juce::Colour col; float fontSize; bool hover = false, boxed = false, active = false, mono = false;
};

// medidor de barras naranjas (L/R) + barra fina de reduccion de ganancia
class BarMeter : public juce::Component, public juce::SettableTooltipClient
{
public:
    BarMeter (std::function<float()> l, std::function<float()> r, std::function<float()> gr, bool thinLeft)
        : getL (std::move (l)), getR (std::move (r)), getGr (std::move (gr)), thinOnLeft (thinLeft) {}
    void paint (juce::Graphics&) override;
    void tick();
private:
    std::function<float()> getL, getR, getGr;
    bool thinOnLeft;
    float v[2] { -100, -100 }, hold[2] { -100, -100 }, gr = 0; int holdT[2] { 0, 0 };
};

// columna de LEDs de atenuacion (gate en rojo, compresor en verde)
class AttenLeds : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit AttenLeds (TopConsoleBProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void tick();
private:
    TopConsoleBProcessor& proc;
    float comp = 0, gate = 0;
};

// fader largo con escala de consola
class Fader : public juce::Component, public juce::SettableTooltipClient
{
public:
    Fader (APVTS&, const juce::String& id);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    static float yOfDb (float db);        // en coordenadas del editor
    static float dbOfY (float y);
    static constexpr float top = 366.0f, step = 33.8f, originY = 330.0f;
private:
    juce::RangedAudioParameter* param;
    std::unique_ptr<juce::ParameterAttachment> att;
    float value = 0, dragOffset = 0;
};

// visor de valor arrastrable (mix, out trim)
class ValueDisplay : public juce::Component, public juce::SettableTooltipClient
{
public:
    ValueDisplay (APVTS&, const juce::String& id, std::function<juce::String (float)> fmt);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
private:
    juce::RangedAudioParameter* param;
    std::unique_ptr<juce::ParameterAttachment> att;
    std::function<juce::String (float)> format;
    float value = 0, startNorm = 0;
};

// visor de preset superior: numero de 7 segmentos + nombre
class PresetDisplay : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit PresetDisplay (TopConsoleBProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override { showMenu(); }
    void showMenu();
private:
    TopConsoleBProcessor& proc;
};

// visor de ruteo: que alimenta el sidechain y orden de los procesos
class RoutingDisplay : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit RoutingDisplay (TopConsoleBProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    TopConsoleBProcessor& proc;
    bool flag (const char* id) const { return proc.apvts.getRawParameterValue (id)->load() > 0.5f; }
    void toggle (const char* id);
};
} // namespace tcbui

class TopConsoleBEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TopConsoleBEditor (TopConsoleBProcessor&);
    ~TopConsoleBEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void tickNow() { timerCallback(); }
    static constexpr int W = 812, H = 752;
private:
    void timerCallback() override;
    void buildBackground();
    void switchAB (int slot);
    void stepPreset (int d);
    TopConsoleBProcessor& proc;
    tcbui::Look look;
    juce::Image background;          // a doble resolucion
    juce::OwnedArray<tcbui::Knob> knobs;
    juce::OwnedArray<tcbui::PushButton> buttons;
    juce::OwnedArray<tcbui::TextButton> texts;
    std::unique_ptr<tcbui::BarMeter> inMeter, outMeter;
    std::unique_ptr<tcbui::AttenLeds> atten;
    std::unique_ptr<tcbui::Fader> fader;
    std::unique_ptr<tcbui::ValueDisplay> mixBox, trimBox;
    std::unique_ptr<tcbui::PresetDisplay> presetTop;
    std::unique_ptr<tcbui::RoutingDisplay> routing;
    tcbui::TextButton* bottomName = nullptr; tcbui::TextButton* abBtn[2] { nullptr, nullptr };
    juce::TooltipWindow tips { this, 600 };
    int transactionTick = 0;
    struct Scale { float cx, cy, r; std::vector<std::pair<float, juce::String>> labels; juce::String unit; juce::String id; };
    std::vector<Scale> scales;
    tcbui::Knob* add (const juce::String& id, juce::Colour cap, bool coloured, float cx, float cy, float r,
                      std::vector<std::pair<float, juce::String>> labels, const juce::String& unit, const juce::String& tip = {});
    tcbui::PushButton* addBtn (const juce::String& id, const juce::String& label, tcbui::PushButton::Style st, juce::Colour led,
                               float cx, float cy, float w, float h, const juce::String& tip = {});
    tcbui::TextButton* addText (const juce::String& t, juce::Colour c, juce::Rectangle<int> r, std::function<void()> f, float size = 12.0f);
    std::vector<std::pair<juce::Component*, juce::Rectangle<int>>> layout;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopConsoleBEditor)
};
