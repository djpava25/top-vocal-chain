#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "TopTuneProcessor.h"

// Interfaz de TOP TUNE: negro y plata (con tema "plata" claro opcional)
namespace ttui
{
using APVTS = juce::AudioProcessorValueTreeState;

struct Palette
{
    juce::Colour bg0, bg1, panel, text, dim, silver, glow, line, knobBody, knobRing, screen0, screen1;
    bool light;
};
const Palette& palette (bool light);

class LnF : public juce::LookAndFeel_V4
{
public:
    LnF();
    bool light = false;
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void drawBubble (juce::Graphics&, juce::BubbleComponent&, const juce::Point<float>&, const juce::Rectangle<float>&) override;
    juce::Font getSliderPopupFont (juce::Slider&) override;
};

// perilla con el valor escrito en el centro
class Knob : public juce::Slider
{
public:
    Knob (APVTS&, const juce::String& id, bool big = false);
private:
    std::unique_ptr<APVTS::SliderAttachment> att;
};

// interruptor deslizante (ENGINE / tema)
class Switch : public juce::Button
{
public:
    Switch() : juce::Button ({}) { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool, bool) override;
    bool lightTheme = false, sun = false;
};

// boton redondeado de texto (LINK, modos, etc.)
class Pill : public juce::Button
{
public:
    Pill (const juce::String& t) : juce::Button (t) {}
    void paintButton (juce::Graphics&, bool over, bool down) override;
    bool lightTheme = false, segment = false, circle = false;
    int icon = 0;            // 0 texto, 1 menu, 2 deshacer, 3 rehacer
};

class KeyPanel : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit KeyPanel (TopTuneProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void tick();
    bool lightTheme = false;
private:
    TopTuneProcessor& proc;
    float cents = 0, shownCents = 0; int note = -1; float glow = 0;
    juce::Rectangle<float> keyArea { 0, 40, 252, 80 }, scaleArea { 0, 122, 252, 30 }, detectArea { 176, 8, 68, 20 };
    int foundRoot = -1; bool foundMinor = false;
};

class Piano : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit Piano (TopTuneProcessor& p) : proc (p) { setTooltip ("Clic en una tecla para quitarla o agregarla a la escala"); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void tick();
private:
    juce::Rectangle<float> keyRect (int pc) const;
    int hit (juce::Point<float>) const;
    TopTuneProcessor& proc;
    int lastMask = -1, lastTarget = -2;
};
} // namespace ttui

class TopTuneEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TopTuneEditor (TopTuneProcessor&);
    ~TopTuneEditor() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void tickNow() { timerCallback(); }
    static constexpr int W = 1100, H = 618;
private:
    void timerCallback() override;
    void applyTheme();
    void mainMenu();
    void presetMenu();
    TopTuneProcessor& proc;
    ttui::LnF lnf;
    ttui::Pill menuBtn { {} }, presetBtn { {} }, undoBtn { {} }, redoBtn { {} }, linkBtn { "LINK" };
    ttui::Switch themeSw, engineSw;
    ttui::KeyPanel keyPanel;
    ttui::Piano piano;
    ttui::Knob sustain, strength, speed, formant, shift, fine;
    juce::Slider mix, out;
    juce::ComboBox styleBox;
    juce::OwnedArray<ttui::Pill> modes;
    std::unique_ptr<juce::ParameterAttachment> modeAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> engineAtt, linkAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> styleAtt;
    int transactionTick = 0, mode = 2;
    juce::TooltipWindow tips { this, 600 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopTuneEditor)
};
