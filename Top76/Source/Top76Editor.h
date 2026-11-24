#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Top76Processor.h"

// Top 76: frente negro de 2 unidades de rack con la distribucion del limitador FET clasico (INPUT y OUTPUT grandes
// con tapa de aluminio torneado y cuerpo moleteado, ATTACK y RELEASE chicos, columna de RATIO, VU retroiluminado
// con aguja fisica y columna de METER) + una franja inferior (presets, LINK, CLEAN/VINTAGE, SC HPF y MIX).
// Todo se dibuja con el renderizador 3D por pixel. Marca propia: modelo TOP 76, fabricante AnalogHM.
namespace t76
{
using APVTS = juce::AudioProcessorValueTreeState;

class Look : public juce::LookAndFeel_V4
{
public:
    Look();
    void drawBubble (juce::Graphics&, juce::BubbleComponent&, const juce::Point<float>&, const juce::Rectangle<float>&) override;
};

// perilla: kind 0 = grande (INPUT/OUTPUT), 1 = chica (ATTACK/RELEASE), 2 = bandeja
class Knob : public juce::Slider
{
public:
    Knob (APVTS&, const juce::String& id, int kind, float startDeg, float endDeg);
    void paint (juce::Graphics&) override;
    bool hitTest (int x, int y) override;
    float angleRad() const;
    int kind;
private:
    std::unique_ptr<APVTS::SliderAttachment> att;
};

// palanca de bate (dos posiciones)
class Toggle : public juce::Component, public juce::SettableTooltipClient
{
public:
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent& e) override { if (getLocalBounds().contains (e.getPosition()) && onClick) onClick(); }
    std::function<void()> onClick;
    std::function<bool()> isRight;
};

// tecla de la columna (RATIO / METER): se hunde cuando esta activa
class PushKey : public juce::Component, public juce::SettableTooltipClient
{
public:
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { held = true; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override;
    std::function<void (const juce::ModifierKeys&)> onClick;
    std::function<bool()> isDown;
    std::function<float()> glow;                         // luz ambar del medidor que se filtra alrededor (0..1)
private:
    bool held = false;
};

// flechas de preset
class Key : public juce::Component, public juce::SettableTooltipClient
{
public:
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { held = true; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override { held = false; repaint(); if (getLocalBounds().contains (e.getPosition()) && onClick) onClick(); }
    std::function<void()> onClick;
    bool left = true;
private:
    bool held = false;
};

class Content : public juce::Component
{
public:
    explicit Content (Top76Processor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    void tick();
    void animate();
    static constexpr int W = 1400, H = 360;
    float needlePos() const { return vuPos; }
private:
    void presetMenu();
    Top76Processor& proc;
    Knob input, output, attack, release, hpf, mix;
    std::array<PushKey, 4> ratioKeys, meterKeys;
    Toggle linkSw, detSw;
    Key prevKey, nextKey;
    juce::Image plate, cover, glass;
    std::unique_ptr<juce::VBlankAttachment> vblank;
    float vuPos = 1.0f, vuVel = 0, tgtSm = 1.0f, lamp = 1.0f;
    int lastBits = -1;
    double lastT = 0;
};
} // namespace t76

class Top76Editor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit Top76Editor (Top76Processor&);
    ~Top76Editor() override;
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff0b0b0c)); }
    void tickNow() { for (int i = 0; i < 90; ++i) { juce::Thread::sleep (16); timerCallback(); } }
    static constexpr int W = t76::Content::W, H = t76::Content::H;
    t76::Content& getContent() { return content; }
private:
    void timerCallback() override { content.tick(); }
    t76::Look look;
    t76::Content content;
    juce::TooltipWindow tips { this, 600 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Top76Editor)
};
