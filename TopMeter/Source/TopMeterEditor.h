#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "TopMeterProcessor.h"

namespace tmui
{
using APVTS = juce::AudioProcessorValueTreeState;

// "chip" de la barra de opciones: muestra una opcion y abre un menu al hacer clic
class Chip : public juce::Component, public juce::SettableTooltipClient
{
public:
    Chip (APVTS&, const juce::String& id, const juce::String& prefix, std::vector<float> floatValues = {}, const juce::String& unit = {});
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }
    bool toggle = false;      // parametro bool: el chip se enciende/apaga
private:
    juce::RangedAudioParameter* param;
    std::unique_ptr<juce::ParameterAttachment> att;
    juce::String prefix, unit;
    std::vector<float> values;
    float value = 0; bool hover = false;
};

class Btn : public juce::Component, public juce::SettableTooltipClient
{
public:
    enum Icon { text, power, play, sync, undo, redo, left, right, help, resetIcon };
    Btn (Icon i, const juce::String& t = {}) : icon (i), label (t) {}
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent& e) override { if (getLocalBounds().contains (e.getPosition()) && onClick) onClick(); }
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }
    std::function<void()> onClick;
    Icon icon; juce::String label;
    bool on = false, hover = false, flat = false, accentText = false;
};

// grafica de sonoridad: barra Momentary, historia, histograma y escalas
class LoudnessView : public juce::Component
{
public:
    explicit LoudnessView (TopMeterProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    double viewEnd = -1.0;      // bloque final mostrado (-1 = seguir en vivo)
private:
    TopMeterProcessor& proc;
};

class StatsView : public juce::Component
{
public:
    explicit StatsView (TopMeterProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
private:
    TopMeterProcessor& proc;
};

class OverviewView : public juce::Component, public juce::SettableTooltipClient
{
public:
    OverviewView (TopMeterProcessor& p, LoudnessView& l) : proc (p), lv (l) { setTooltip ("Arrastra para revisar momentos anteriores (desactiva Auto Scroll)"); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent& e) override { mouseDrag (e); }
    void mouseDrag (const juce::MouseEvent&) override;
private:
    TopMeterProcessor& proc;
    LoudnessView& lv;
};

class LevelView : public juce::Component
{
public:
    explicit LevelView (TopMeterProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void tick();
private:
    TopMeterProcessor& proc;
    float shown[2] { -150, -150 }, rms[2] { -150, -150 }, hold[2] { -150, -150 }; double holdT[2] { 0, 0 };
};

class ScopeView : public juce::Component
{
public:
    explicit ScopeView (TopMeterProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
private:
    TopMeterProcessor& proc;
    float gain = 1.0f;
};

class VuView : public juce::Component
{
public:
    VuView (TopMeterProcessor& p, int ch) : proc (p), channel (ch) {}
    void paint (juce::Graphics&) override;
    void tick();
private:
    TopMeterProcessor& proc;
    int channel;
    float needle = -20.0f, vel = 0.0f;
};

class SpectrumView : public juce::Component
{
public:
    explicit SpectrumView (TopMeterProcessor& p);
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hoverBand = -1; }
    void mouseDown (const juce::MouseEvent&) override { selected = hoverBand; }
    void tick();
    void resetPeaks();
    static constexpr int bands = 31;
    int hoverBand = -1, selected = -1;
    std::array<float, bands> level {}, peak {}, maxv {};
    static float centre (int b) { return 1000.0f * std::pow (2.0f, (float) (b - 17) / 3.0f); }
private:
    TopMeterProcessor& proc;
    juce::dsp::FFT fft { 14 };
    std::vector<float> data, window;
    std::array<double, bands> pow {};
    std::array<double, bands> peakT {};
};
} // namespace tmui

class TopMeterEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TopMeterEditor (TopMeterProcessor&);
    ~TopMeterEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void tickNow() { timerCallback(); }
    static constexpr int W = 1060, H = 948;
private:
    void timerCallback() override;
    void switchAB (int slot);
    TopMeterProcessor& proc;
    tmui::Btn powerBtn { tmui::Btn::power }, playBtn { tmui::Btn::play }, syncBtn { tmui::Btn::sync }, resetBtn { tmui::Btn::text, "RESET" },
              undoBtn { tmui::Btn::undo }, redoBtn { tmui::Btn::redo }, aBtn { tmui::Btn::text, "A" }, bBtn { tmui::Btn::text, "B" },
              prevBtn { tmui::Btn::left }, nextBtn { tmui::Btn::right }, presetBtn { tmui::Btn::text }, helpBtn { tmui::Btn::help },
              specReset { tmui::Btn::resetIcon };
    juce::Label nameLabel;
    juce::OwnedArray<tmui::Chip> chips;
    tmui::LoudnessView loud;
    tmui::StatsView stats;
    tmui::OverviewView overview;
    tmui::LevelView levels;
    tmui::ScopeView scope;
    tmui::VuView vuL, vuR;
    tmui::SpectrumView spectrum;
    int transactionTick = 0;
    juce::TooltipWindow tips { this, 500 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopMeterEditor)
};
