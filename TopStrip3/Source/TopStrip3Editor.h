#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "TopStrip3Processor.h"
#include "Strip3Parts.h"

// Panel del Top Strip 737 V3: misma distribucion del canal original con mas realismo:
//  - perillas en capas: cuerpo 3D renderizado una vez + textura de mecanizado que gira con la perilla +
//    indicador con luz fija (el brillo cambia segun el angulo) y animacion suave a 60 fps
//  - texturas distintas: preamp torneado, compresor cepillado radial, EQ granallado
//  - VU retroiluminado calido con aguja de fisica real (2do orden, 300 ms, 1.5 % de sobrepaso), sombra de la
//    aguja, cristal con reflejos y escala detallada
//  - botones con bombilla que se enciende y apaga con suavidad y fuga de luz sobre el aluminio
//  - barra superior: logo HM MUSIC, selector de presets, A/B, deshacer/rehacer, carga DSP y sobremuestreo
// Texto de marca propio: "TOP STRIP" / "Top 737" / "DJ PAVA - HM MUSIC" (no se copian nombres ni logotipos).
namespace s7ui3
{
using APVTS = juce::AudioProcessorValueTreeState;
class Look : public juce::LookAndFeel_V4
{
public:
    Look();
    void drawLinearSlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider::SliderStyle, juce::Slider&) override;
};
class Knob : public juce::Slider
{
public:
    Knob (APVTS&, const juce::String& id, float a0deg, float a1deg, std::vector<float> stepAnglesDeg = {});
    void paint (juce::Graphics&) override;
    float targetAngleDeg() const;
    bool animate (float dt);                    // true si cambio el angulo mostrado
    std::vector<float> steps;
    float a0 = -135, a1 = 135;
    float radius = 20.0f, faceShift = 0.0f, faceShiftY = 0.0f;   // en pixeles del contenido
    s7p3::KnobStyle style;
    juce::String pid;
private:
    void buildImages();
    std::unique_ptr<APVTS::SliderAttachment> att;
    juce::Image body, overlay;
    int bodyW = 0;
    float shown = 0.0f, vel = 0.0f; bool init = false;
};
// boton de acrilico rojo iluminado (se apaga con el POWER), con encendido y apagado suaves
class LampBtn : public juce::Component, public juce::SettableTooltipClient
{
public:
    LampBtn (TopStrip3Processor& p, const char* paramId, const char* capText) : proc (p), id (paramId), text (capText) {}
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool animate (float dt);
private:
    bool lit() const;
    TopStrip3Processor& proc; const char* id; juce::String text;
    juce::Image cache[2];
    float level = -1.0f;
};
class Rocker : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit Rocker (TopStrip3Processor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    TopStrip3Processor& proc;
    juce::Image cache[2];
};
class Content : public juce::Component, private juce::Slider::Listener
{
public:
    explicit Content (TopStrip3Processor&);
    ~Content() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void tick (float dt);
    static constexpr int W = 1600, strip = 54, panelH = 293, H = strip + panelH;
private:
    void sliderValueChanged (juce::Slider*) override;
    void sliderDragStarted (juce::Slider*) override;
    void paintVu (juce::Graphics&);
    void drawVuFace (juce::Graphics&, const juce::Path& win, juce::Rectangle<float> wb, float glowLevel);
    void drawVuGlass (juce::Graphics&, const juce::Path& win, juce::Rectangle<float> wb);
    juce::Image vuFaceImg, vuGlassImg; float vuFaceGlow = -1.0f;
    void paintStrip (juce::Graphics&);
    void newGesture (const char* name);
    TopStrip3Processor& proc;
    Knob preGain, input, hpf, thr, ratio, attack, release, bass, bassF, lm, lmF, hm, hmF, treble, trebleF, out;
    LampBtn bHigh, bPhase, bP48, bFilter, bPre, bMeter, bComp, bLmQ, bLmX, bHmQ, bHmX, bSc, bEq;
    Rocker rocker;
    juce::Slider mix;
    std::unique_ptr<APVTS::SliderAttachment> mixAtt;
    juce::Image panel, stripImg, ledOn, ledOff;
    // aguja del VU: posicion lineal (1 = 0 VU) con fisica de 2do orden
    float needle = 0.0f, needleVel = 0.0f, glow = 1.0f;
    bool lastPower = true;
    int lastProgram = -1, lastSlot = -1;
    bool lastUndo = false, lastRedo = false;
    float dspShown = 0.0f, sigShown = 0.0f;
    int hover = -1;
    Knob* pillKnob = nullptr; double pillTime = 0;
    // barra superior
    juce::Rectangle<float> undoArea { 300, 12, 34, 30 }, redoArea { 338, 12, 34, 30 },
                           aArea { 396, 12, 30, 30 }, bArea { 428, 12, 30, 30 }, copyArea { 462, 12, 44, 30 },
                           prevArea { 560, 11, 32, 32 }, presetArea { 596, 9, 380, 36 }, nextArea { 980, 11, 32, 32 };
};
} // namespace s7ui3

class TopStrip3Editor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TopStrip3Editor (TopStrip3Processor&);
    ~TopStrip3Editor() override;
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff0b0c0d)); }
    void tickNow() { content.tick (1.0f / 60.0f); }
    static constexpr int fullW = s7ui3::Content::W, fullH = s7ui3::Content::H;
private:
    void timerCallback() override;
    s7ui3::Look look;
    s7ui3::Content content;
    juce::TooltipWindow tips { this, 600 };
    double lastTick = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopStrip3Editor)
};
