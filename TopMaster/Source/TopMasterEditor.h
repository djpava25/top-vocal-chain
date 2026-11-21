#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <complex>
#include "TopMasterProcessor.h"

namespace tmui
{
using APVTS = juce::AudioProcessorValueTreeState;

class Look : public juce::LookAndFeel_V4
{
public:
    Look();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override;
};

// control generico: perilla, lista o interruptor ligado a un parametro
class Control : public juce::Component
{
public:
    Control (APVTS&, const juce::String& id, const juce::String& label, juce::Colour);
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::String id;
private:
    juce::String label;
    int kind = 0;   // 0 perilla, 1 lista, 2 interruptor
    juce::Slider slider;
    juce::ComboBox combo;
    juce::ToggleButton toggle;
    std::unique_ptr<APVTS::SliderAttachment> sa;
    std::unique_ptr<APVTS::ComboBoxAttachment> ca;
    std::unique_ptr<APVTS::ButtonAttachment> ba;
};

// fila de la cadena de modulos
class ModuleRow : public juce::Component, public juce::SettableTooltipClient
{
public:
    ModuleRow (TopMasterProcessor&, int module);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    std::function<void (int)> onSelect;
    std::function<void()> onReorder;
    int module;
    bool selected = false;
private:
    TopMasterProcessor& proc;
};

// analizador + curvas/cruces del modulo activo
class Display : public juce::Component
{
public:
    explicit Display (TopMasterProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void updateSpectrum();
    int module = 0, band = 3;
    std::function<void (int)> onBand;
private:
    float xOf (float hz) const;
    float hzOf (float x) const;
    float yDb (float db) const;       // espectro
    float yEq (float db) const;       // curva de EQ (+-12 dB)
    juce::StringArray crossoverIds() const;
    int hitTest (juce::Point<float>, juce::String& id) const;
    float eqResponse (float hz) const;
    void setP (const juce::String& id, float v);
    TopMasterProcessor& proc;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> win, buf, specIn, specOut;
    juce::String dragId; int dragBand = -1, hover = -1;
};

// panel del asistente de mastering
class AssistantPanel : public juce::Component, private juce::Timer
{
public:
    explicit AssistantPanel (TopMasterProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    std::function<void()> onClose;
private:
    void timerCallback() override;
    TopMasterProcessor& proc;
    juce::ComboBox genre, target, intensity;
    juce::ToggleButton useRef { "Usar mi referencia como objetivo" };
    juce::TextButton listen { "ESCUCHAR" }, close { "CERRAR" };
};
} // namespace tmui

class TopMasterEditor : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit TopMasterEditor (TopMasterProcessor&);
    ~TopMasterEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray&, int, int) override;
    void selectModule (int);
    void selectBand (int);
    void showAssistant (bool);
    void tickNow() { timerCallback(); }
    static constexpr int W = 1280, H = 780;
private:
    void timerCallback() override;
    void layoutRows();
    void buildControls();
    TopMasterProcessor& proc;
    tmui::Look look;
    tmui::Display display;
    juce::OwnedArray<tmui::ModuleRow> rows;
    juce::OwnedArray<tmui::Control> controls;
    juce::OwnedArray<juce::TextButton> bandBtns;
    juce::TextButton assistantBtn { "ASISTENTE" }, refBtn { "REFERENCIA A/B" }, loadRefBtn { "CARGAR REF." }, bypassBtn { "BYPASS" }, resetBtn { "REINICIAR" };
    juce::ToggleButton matchBtn { "Igualar volumen" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAtt, matchAtt;
    std::unique_ptr<tmui::Control> inGain;
    std::unique_ptr<tmui::AssistantPanel> assistant;
    std::unique_ptr<juce::FileChooser> chooser;
    int module = tmaster::EQ, band = 3;
    juce::TooltipWindow tips { this, 500 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopMasterEditor)
};
