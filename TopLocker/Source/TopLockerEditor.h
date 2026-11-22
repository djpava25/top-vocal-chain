#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "TopLockerProcessor.h"

namespace tlui
{
using APVTS = juce::AudioProcessorValueTreeState;
class Look : public juce::LookAndFeel_V4
{
public:
    Look();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawBubble (juce::Graphics&, juce::BubbleComponent&, const juce::Point<float>&, const juce::Rectangle<float>&) override;
};

// perilla cromada con textos de minimo/maximo y caja de valor
class Dial : public juce::Component
{
public:
    Dial (APVTS&, const juce::String& id, const juce::String& lo, const juce::String& hi, std::function<juce::String (float)> fmt, float diameter, bool arc = false, int loIcon = 0, int hiIcon = 0);
    void resized() override;
    void paint (juce::Graphics&) override;
    bool showValue = true, compact = false;
    juce::Slider slider;
private:
    std::unique_ptr<APVTS::SliderAttachment> att;
    juce::String lo, hi;
    std::function<juce::String (float)> fmt;
    float dia;
    int loIcon, hiIcon;
};

// caja de texto con flecha que abre un menu
class Box : public juce::Component, public juce::SettableTooltipClient
{
public:
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent& e) override { if (getLocalBounds().contains (e.getPosition()) && onClick) onClick(); }
    std::function<void()> onClick;
    juce::String label, text; float fontSize = 15.0f; bool arrow = true, centred = false;
};

class Btn : public juce::Component, public juce::SettableTooltipClient
{
public:
    enum Kind { Text, Prev, Next, Gear, Info };
    Btn (Kind k, const juce::String& t = {}) : kind (k), text (t) {}
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent& e) override { if (getLocalBounds().contains (e.getPosition()) && onClick) onClick(); }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    std::function<void()> onClick;
    Kind kind; juce::String text;
};

class Categories : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    std::function<void (int)> onPick;
    int current = 6;
    static const char* name (int i);
    static unsigned mask (int i);
};

class Carousel : public juce::Component, public juce::TooltipClient
{
public:
    explicit Carousel (TopLockerProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    juce::String getTooltip() override;
    void setCategory (int c);
    void step (int dir);
    void ensureVisible();
    bool animate();                  // devuelve true si hay que repintar
    std::vector<int> list() const;
    int category = 6, first = 0;
    float glow = 1.0f;               // animacion al elegir microfono (0 -> 1)
    static constexpr int visible = 9;
private:
    juce::Rectangle<float> cardRect (int slot, bool selected) const;
    int slotAt (juce::Point<float>) const;
    TopLockerProcessor& proc;
    int lastMic = -1;
};

// vista desde arriba: el microfono gira segun el angulo
class PositionView : public juce::Component
{
public:
    explicit PositionView (TopLockerProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    bool animate();
    float shown = 0;
private:
    TopLockerProcessor& proc;
};
// vista de lado: distancia entre la boca y el microfono
class ProxView : public juce::Component
{
public:
    explicit ProxView (TopLockerProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    bool animate();
    float shown = 50;
private:
    TopLockerProcessor& proc;
};
// curva de EQ (tono) y color del microfono
class ToneCurve : public juce::Component
{
public:
    explicit ToneCurve (TopLockerProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void refresh();
private:
    TopLockerProcessor& proc;
    std::vector<float> mic, tone;
};

class PreampView : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit PreampView (TopLockerProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    TopLockerProcessor& proc;
};

class RoomView : public juce::Component
{
public:
    explicit RoomView (TopLockerProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    TopLockerProcessor& proc;
};

class Meter : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    float in = 0, out = 0;
};

void drawMic (juce::Graphics&, juce::Rectangle<float> area, int mic, bool detail = true);
void drawRoom (juce::Graphics&, juce::Rectangle<float> area, int room, float size);

class Content : public juce::Component
{
public:
    explicit Content (TopLockerProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick();
    static constexpr int W = 1280, H = 900;
private:
    void presetMenu();
    void sourceMenu();
    void charMenu();
    void gearMenu();
    void freqMenu (int band);
    void saveDialog();
    void loadDialog();
    void stepPreset (int dir);
    TopLockerProcessor& proc;
public:
    Categories cats;
    Carousel carousel;
    Btn prev { Btn::Prev }, next { Btn::Next }, save { Btn::Text, "Save" }, load { Btn::Text, "Load" }, gear { Btn::Gear }, info { Btn::Info };
    Box presetBox, charBox, sourceBox, lowBox, midBox, highBox;
    Dial angle, prox, low, mid, high, character, sat, out, drive, input, roomAmt, roomSize, roomDecay, roomEarly, roomWidth, roomDamp, mix;
    PositionView posView;
    ProxView proxView;
    ToneCurve curve;
    PreampView preamp;
    RoomView room;
    Meter meter;
private:
    Btn micPrev { Btn::Prev }, micNext { Btn::Next };
    std::unique_ptr<juce::FileChooser> chooser;
    lk::Params lastP; int lastRoom = -1; float lastSize = -1; juce::String lastPreset; float lastPower = -1;
    float inDb = -60, outDb = -60;
};
} // namespace tlui

class TopLockerEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TopLockerEditor (TopLockerProcessor&);
    ~TopLockerEditor() override;
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colours::black); }
    void tickNow() { for (int i = 0; i < 40; ++i) timerCallback(); }
    static constexpr int W = tlui::Content::W, H = tlui::Content::H;
private:
    void timerCallback() override { content.tick(); }
    tlui::Look look;
    tlui::Content content;
    juce::TooltipWindow tips { this, 600 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopLockerEditor)
};
