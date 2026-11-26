#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Top73Processor.h"

// Top 73: modulo vertical con la distribucion del canal clasico (panel azul pizarra, perilla roja de ganancia
// con falda transparente, agudos en perilla gris, medios y graves con aro blanco de frecuencia, filtro azul y
// teclas EQL / PHASE) + una bandeja al lado con presets, medidor, LEVEL, AUTO y POWER.
// Todo se dibuja con el renderizador 3D por pixel. Logotipo propio ("HM"), sin marcas ajenas.
namespace t73
{
using APVTS = juce::AudioProcessorValueTreeState;
enum class KnobType { Red, Blue, GreyBig, Grey, GreySmall, Ring };

class Look : public juce::LookAndFeel_V4
{
public:
    Look();
    void drawBubble (juce::Graphics&, juce::BubbleComponent&, const juce::Point<float>&, const juce::Rectangle<float>&) override;
};

class Knob : public juce::Slider
{
public:
    Knob (APVTS&, const juce::String& id, KnobType, std::vector<float> stepDeg, float a0Deg, float a1Deg);
    void paint (juce::Graphics&) override;
    bool hitTest (int x, int y) override;
    float angleRad() const;
    KnobType type;
    std::vector<float> steps;                    // grados por posicion (vacio = continuo)
private:
    std::unique_ptr<APVTS::SliderAttachment> att;
};

// tecla blanca (EQL / PHASE / AUTO / POWER / ...)
class Key : public juce::Component, public juce::SettableTooltipClient
{
public:
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { held = true; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override { held = false; repaint(); if (getLocalBounds().contains (e.getPosition()) && onClick) onClick(); }
    std::function<void()> onClick;
    std::function<bool()> isOn;
    juce::String text;
    float textCap = 13.0f;
private:
    bool held = false;
};

class Content : public juce::Component
{
public:
    explicit Content (Top73Processor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    void tick();
    static constexpr int W = 528, H = 1664;
private:
    void presetMenu();
    Top73Processor& proc;
    Knob gain, hf, midFreq, midGain, lowFreq, lowGain, hpf, level;
    Key eqKey, phaseKey, prevKey, nextKey, ipKey, opKey, autoKey, sat3dKey, powerKey;
    juce::Image plate;
    float inDb = -60, outDb = -60;
    n73::Settings last; float lastBits = -1;
    juce::Rectangle<float> lcd { 358, 168, 150, 62 };
};
} // namespace t73

class Top73Editor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit Top73Editor (Top73Processor&);
    ~Top73Editor() override;
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff0b0d0f)); }
    void tickNow() { for (int i = 0; i < 20; ++i) timerCallback(); }
    static constexpr int W = t73::Content::W, H = t73::Content::H;
private:
    void timerCallback() override { content.tick(); }
    t73::Look look;
    t73::Content content;
    juce::TooltipWindow tips { this, 600 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Top73Editor)
};
