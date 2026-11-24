#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Top3AProcessor.h"

// Top 3A: frente negro de medio rack con la distribucion del nivelador clasico (GAIN y PEAK REDUCTION con falda de
// acrilico ahumado y tapa de aluminio, VU ambar con aguja fisica, palancas GR/OUTPUT y POWER con piloto) + una
// franja inferior tipo panel trasero (presets, COMP/LIMIT, HF EMPH y MIX).
// Todo se dibuja con el renderizador 3D por pixel. Marca propia: modelo TOP 3A, fabricante AnalogHM (logo HM).
namespace t3a
{
using APVTS = juce::AudioProcessorValueTreeState;

class Look : public juce::LookAndFeel_V4
{
public:
    Look();
    void drawBubble (juce::Graphics&, juce::BubbleComponent&, const juce::Point<float>&, const juce::Rectangle<float>&) override;
};

class Knob : public juce::Slider
{
public:
    Knob (APVTS&, const juce::String& id, bool small, int finish);
    void paint (juce::Graphics&) override;
    bool hitTest (int x, int y) override;
    float angleRad() const;
    bool small; int finish;
private:
    std::unique_ptr<APVTS::SliderAttachment> att;
};

// palanca de bate (dos posiciones, izquierda / derecha)
class Toggle : public juce::Component, public juce::SettableTooltipClient
{
public:
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent& e) override { if (getLocalBounds().contains (e.getPosition()) && onClick) onClick(); }
    std::function<void()> onClick;
    std::function<bool()> isRight;
};

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
    explicit Content (Top3AProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    void tick();
    void animate();
    static constexpr int W = 1000, H = 500;
    float needleDb() const { return vuShown; }
private:
    void presetMenu();
    Top3AProcessor& proc;
    Knob gain, peak, emph, mix;
    Toggle meterSw, powerSw, modeSw;
    Key prevKey, nextKey;
    juce::Image plate, cover, glass;
    std::unique_ptr<juce::VBlankAttachment> vblank;
    float vuPos = 0, vuVel = 0, vuShown = -30, tgtSm = 0;
    int lastBits = -1;
    double lastT = 0;
};
} // namespace t3a

class Top3AEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit Top3AEditor (Top3AProcessor&);
    ~Top3AEditor() override;
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff0b0b0c)); }
    void tickNow() { for (int i = 0; i < 90; ++i) { juce::Thread::sleep (16); timerCallback(); } }
    static constexpr int W = t3a::Content::W, H = t3a::Content::H;
    t3a::Content& getContent() { return content; }
private:
    void timerCallback() override { content.tick(); }
    t3a::Look look;
    t3a::Content content;
    juce::TooltipWindow tips { this, 600 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Top3AEditor)
};
