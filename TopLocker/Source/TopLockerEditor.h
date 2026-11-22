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
    bool showValue = true;
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

// boton del encabezado
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

// lista de categorias (izquierda)
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

// carrusel de microfonos
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
    std::vector<int> list() const;
    int category = 6, first = 0;
    static constexpr int visible = 9;
private:
    juce::Rectangle<float> cardRect (int slot, bool selected) const;
    int slotAt (juce::Point<float>) const;
    TopLockerProcessor& proc;
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
    float in = 0, out = 0;   // dB
};

void drawMic (juce::Graphics&, juce::Rectangle<float> area, int mic);
void drawRoom (juce::Graphics&, juce::Rectangle<float> area, int room);

// todo el panel en coordenadas fijas 1200x800 (se escala completo)
class Content : public juce::Component
{
public:
    explicit Content (TopLockerProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick();
private:
    void presetMenu();
    void sourceMenu();
    void charMenu();
    void gearMenu();
    void saveDialog();
    void loadDialog();
    void stepPreset (int dir);
    TopLockerProcessor& proc;
public:
    Categories cats;
    Carousel carousel;
    Btn prev { Btn::Prev }, next { Btn::Next }, save { Btn::Text, "Save" }, load { Btn::Text, "Load" }, gear { Btn::Gear }, info { Btn::Info };
    Box presetBox, charBox, sourceBox;
    Dial position, prox, low, mid, high, character, sat, out, preGain, roomAmt, mix;
    PreampView preamp;
    RoomView room;
    Meter meter;
private:
    Btn micPrev { Btn::Prev }, micNext { Btn::Next };
    std::unique_ptr<juce::FileChooser> chooser;
    lk::Params lastP; int lastRoom = -1; juce::String lastPreset; float lastPower = -1;
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
    void tickNow() { timerCallback(); }
    static constexpr int W = 1200, H = 800;
private:
    void timerCallback() override { content.tick(); }
    tlui::Look look;
    tlui::Content content;
    juce::TooltipWindow tips { this, 600 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopLockerEditor)
};
