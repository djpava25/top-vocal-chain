#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "VbxProcessor.h"

// Panel del 6961P con la distribucion del canal original: aluminio cepillado azul, placas negras
// anodizadas en relieve, VU retroiluminado en ventana curva, perillas de aleta y palancas mini.
// Todo se dibuja con el renderizador 3D (Render3D.h). Texto de marca propio: "TOP / 6961P".
namespace vbui
{
using APVTS = juce::AudioProcessorValueTreeState;
class Look : public juce::LookAndFeel_V4
{
public:
    Look();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawBubble (juce::Graphics&, juce::BubbleComponent&, const juce::Point<float>&, const juce::Rectangle<float>&) override;
};
class Knob : public juce::Slider
{
public:
    Knob (APVTS&, const juce::String& id, float a0, float a1, std::vector<float> stepAnglesDeg = {}, bool mini = false);
    const juce::Image& image (float ang, int w, int h);
    std::vector<float> steps; bool mini;
private:
    std::unique_ptr<APVTS::SliderAttachment> att;
    juce::Image img; float imgAng = 1e9f; int imgW = 0;
};
// palanca mini cromada de 2 o 3 posiciones
class Toggle : public juce::Component, public juce::SettableTooltipClient
{
public:
    Toggle (VbxProcessor& p, const char* paramId, int positions, bool upIsHigh = true) : proc (p), id (paramId), n (positions), upHigh (upIsHigh) {}
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool decorative = false, decoOn = true;
private:
    int position() const;                      // -1 arriba, 0 centro, 1 abajo
    VbxProcessor& proc; const char* id; int n; bool upHigh;
    juce::Image cache[3];
};
class Lamp : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit Lamp (VbxProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    VbxProcessor& proc;
    juce::Image cache[2];
};
class Content : public juce::Component
{
public:
    explicit Content (VbxProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    void tick();
    static constexpr int W = 1600, strip = 46, H = strip + 442;
private:
    void paintVu (juce::Graphics&);
    VbxProcessor& proc;
    Knob input, gain, meterSel, thr, attack, release, dsFreq, dsThr, low, lowF, mid, midF, hi, hiF, out;
    Toggle phantom, hpf, phase, link, comp, deess, eq, eqLine;
    Lamp lamp;
    juce::Image plate, stripImg;
    float vu = -20.0f, vuShown = -20.0f;
    int lastProgram = -1; float lastOut = 99, lastX = -1;
    juce::Rectangle<float> presetArea { 630, 8, 340, 30 }, prevArea { 594, 8, 32, 30 }, nextArea { 974, 8, 32, 30 }, xfmrArea { 1236, 10, 72, 26 };
};
} // namespace vbui

class VbxEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VbxEditor (VbxProcessor&);
    ~VbxEditor() override;
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff0b0c0d)); }
    void tickNow() { for (int i = 0; i < 40; ++i) timerCallback(); }
    static constexpr int W = vbui::Content::W, H = vbui::Content::H;
private:
    void timerCallback() override { content.tick(); }
    vbui::Look look;
    vbui::Content content;
    juce::TooltipWindow tips { this, 600 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VbxEditor)
};
