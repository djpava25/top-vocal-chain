#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "TopMicProcessor.h"

// TOP MIC COLLECTION (AnalogHM): vitrina con el microfono dibujado en 3D por pixel, grafica de respuesta
// (destino, origen y resultado), diagrama polar con la posicion de la voz y los controles.
namespace tmicui
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
    Knob (APVTS&, const juce::String& id);
private:
    std::unique_ptr<APVTS::SliderAttachment> att;
};
class Toggle : public juce::Button
{
public:
    Toggle (APVTS&, const juce::String& id, const juce::String& label, int icon = 0);
    void paintButton (juce::Graphics&, bool over, bool down) override;
private:
    int icon;
    std::unique_ptr<APVTS::ButtonAttachment> att;
};
// fila de botones para un parametro de opciones (patron, filtro)
class Choice : public juce::Component, public juce::SettableTooltipClient
{
public:
    Choice (TopMicProcessor& p, const juce::String& id, juce::StringArray labels) : proc (p), pid (id), names (std::move (labels)) {}
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    std::function<bool (int)> enabled;     // opcion disponible
    std::function<int()> shown;            // opcion activa que se muestra (p. ej. el patron efectivo)
private:
    TopMicProcessor& proc;
    juce::String pid;
    juce::StringArray names;
};
class Display : public juce::Component, public juce::SettableTooltipClient
{
public:
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent& e) override { if (getLocalBounds().contains (e.getPosition()) && onClick) onClick(); }
    std::function<void()> onClick;
    juce::String text; float fontSize = 14.0f;
};
class Showcase : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit Showcase (TopMicProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    std::function<void()> onName;
private:
    TopMicProcessor& proc;
};
class ResponseView : public juce::Component
{
public:
    explicit ResponseView (TopMicProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
private:
    TopMicProcessor& proc;
};
class PolarView : public juce::Component
{
public:
    explicit PolarView (TopMicProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
private:
    TopMicProcessor& proc;
};
} // namespace tmicui

class TopMicEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TopMicEditor (TopMicProcessor&);
    ~TopMicEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void tickNow() { timerCallback(); }
    static constexpr int W = 1100, H = 660;
private:
    void timerCallback() override;
    void modelMenu (const juce::String& id, juce::Component& target);
    TopMicProcessor& proc;
    tmicui::Look look;
    tmicui::Showcase show;
    tmicui::ResponseView resp;
    tmicui::PolarView polar;
    tmicui::Display sourceBox, micBBox, presetBox;
    tmicui::Choice pattern, filter;
    tmicui::Knob dist, prox, axis, out, blend;
    tmicui::Toggle rev, phase, power, dual;
    float meter = 0.0f;
    juce::int64 lastHash = 0;
    juce::TooltipWindow tips { this, 500 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopMicEditor)
};
