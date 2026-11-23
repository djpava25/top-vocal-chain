#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "TopStrip2Processor.h"
#include "Strip2Parts.h"

// Panel del Top Strip 737 con la distribucion del canal original: aluminio plateado cepillado, placa central
// con VU de cara crema, perillas de aluminio pulido con lado moleteado, botones de acrilico rojo iluminados,
// jack de instrumento e interruptor basculante. Todo se dibuja con el renderizador 3D (Render3D.h).
// Texto de marca propio: "TOP STRIP" / "Top 737" / "DJ PAVA - HM MUSIC" (no se copian nombres ni logotipos).
namespace s7ui2
{
using APVTS = juce::AudioProcessorValueTreeState;
class Look : public juce::LookAndFeel_V4
{
public:
    Look();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider::SliderStyle, juce::Slider&) override;
};
class Knob : public juce::Slider
{
public:
    Knob (APVTS&, const juce::String& id, float a0deg, float a1deg, std::vector<float> stepAnglesDeg = {});
    const juce::Image& image (float ang, int w, int h);
    std::vector<float> steps;
    float radius = 20.0f, faceShift = 0.0f, faceShiftY = 0.0f;       // en pixeles del contenido
    s7p2::KnobStyle style;
    juce::String pid;
private:
    std::unique_ptr<APVTS::SliderAttachment> att;
    juce::Image img; float imgAng = 1e9f; int imgW = 0;
};
// boton de acrilico rojo iluminado (se apaga con el POWER)
class LampBtn : public juce::Component, public juce::SettableTooltipClient
{
public:
    LampBtn (TopStrip2Processor& p, const char* paramId, const char* capText) : proc (p), id (paramId), text (capText) {}
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void refresh();
private:
    bool lit() const;
    TopStrip2Processor& proc; const char* id; juce::String text;
    juce::Image cache[2]; int shown = -1;
};
class Rocker : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit Rocker (TopStrip2Processor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    TopStrip2Processor& proc;
    juce::Image cache[2];
};
class Content : public juce::Component, private juce::Slider::Listener
{
public:
    explicit Content (TopStrip2Processor&);
    ~Content() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    void tick();
    static constexpr int W = 1600, strip = 46, panelH = 293, H = strip + panelH;
private:
    void sliderValueChanged (juce::Slider*) override;
    void sliderDragStarted (juce::Slider*) override;
    void paintVu (juce::Graphics&);
    TopStrip2Processor& proc;
    Knob preGain, input, hpf, thr, ratio, attack, release, bass, bassF, lm, lmF, hm, hmF, treble, trebleF, out;
    LampBtn bHigh, bPhase, bP48, bFilter, bPre, bMeter, bComp, bLmQ, bLmX, bHmQ, bHmX, bSc, bEq;
    Rocker rocker;
    juce::Slider mix;                                            // V2: mezcla en paralelo
    std::unique_ptr<APVTS::SliderAttachment> mixAtt;
    juce::Image panel, stripImg, ledOn, ledOff;
    float vuShown = -30.0f;
    bool lastPower = true;
    int lastProgram = -1;
    Knob* pillKnob = nullptr; double pillTime = 0;
    juce::Rectangle<float> presetArea { 630, 8, 340, 30 }, prevArea { 594, 8, 32, 30 }, nextArea { 974, 8, 32, 30 };
};
} // namespace s7ui2

class TopStrip2Editor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TopStrip2Editor (TopStrip2Processor&);
    ~TopStrip2Editor() override;
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff0b0c0d)); }
    void tickNow() { timerCallback(); }
    static constexpr int fullW = s7ui2::Content::W, fullH = s7ui2::Content::H;
private:
    void timerCallback() override { content.tick(); }
    s7ui2::Look look;
    s7ui2::Content content;
    juce::TooltipWindow tips { this, 600 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopStrip2Editor)
};
