#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "TopEqProcessor.h"

// Panel con la distribucion del ecualizador de programa original (placa azul pizarra pintada,
// perillas de baquelita con falda y estrella, perillas de barra, selector de pico, palanca IN,
// lampara de joya y perilla OFF/ON). El texto de marca es propio: "TOP / PROGRAM EQUALIZER / EQ-1A".
namespace teqi
{
using APVTS = juce::AudioProcessorValueTreeState;
class Look : public juce::LookAndFeel_V4
{
public:
    Look();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawBubble (juce::Graphics&, juce::BubbleComponent&, const juce::Point<float>&, const juce::Rectangle<float>&) override;
};
enum class KnobKind { skirt, bar, pointer, mini };
class Knob : public juce::Slider
{
public:
    Knob (APVTS&, const juce::String& id, KnobKind, float a0, float a1);
    KnobKind kind;
    const juce::Image& image (float ang, int w, int h);     // imagen 3D en cache (se rehace solo si se mueve)
private:
    juce::Image img; float imgAng = 1e9f; int imgW = 0;
    std::unique_ptr<APVTS::SliderAttachment> att;
};
// palanca IN (mete o saca la red del EQ) y perilla OFF/ON (todo el aparato)
class Toggle : public juce::Component, public juce::SettableTooltipClient
{
public:
    Toggle (TopEqProcessor& p, const char* id) : proc (p), pid (id) {}
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    TopEqProcessor& proc; const char* pid;
    juce::Image cache[2];
};
class PowerKnob : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit PowerKnob (TopEqProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    TopEqProcessor& proc;
    juce::Image cache[2];
};
class Jewel : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit Jewel (TopEqProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
private:
    TopEqProcessor& proc;
    juce::Image cache[2];
};
class Content : public juce::Component
{
public:
    explicit Content (TopEqProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    void tick();
    static constexpr int W = 1560, strip = 46, H = strip + 466;
private:
    void buildPanel();
    TopEqProcessor& proc;
    Knob lowBoost, lowAtten, lowFreq, highBoost, bandwidth, highFreq, highAtten, attFreq, drive, out;
    Toggle eqIn;
    PowerKnob power;
    Jewel lamp;
    juce::Image panel, plate;
    int lastProgram = -1; float lastPower = -1, lastIn = -1, lastDrive = 99, lastOut = 99;
    juce::Rectangle<float> presetArea { 610, 8, 340, 30 }, prevArea { 574, 8, 32, 30 }, nextArea { 954, 8, 32, 30 };
};
} // namespace teqi

class TopEqEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TopEqEditor (TopEqProcessor&);
    ~TopEqEditor() override;
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff0c0d0e)); }
    void tickNow() { timerCallback(); }
    static constexpr int W = teqi::Content::W, H = teqi::Content::H;
private:
    void timerCallback() override { content.tick(); }
    teqi::Look look;
    teqi::Content content;
    juce::TooltipWindow tips { this, 600 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopEqEditor)
};
