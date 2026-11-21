#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "TopVoiceMidiProcessor.h"

namespace vmui
{
using APVTS = juce::AudioProcessorValueTreeState;

class Look : public juce::LookAndFeel_V4
{
public:
    Look();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    void drawLinearSlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider::SliderStyle, juce::Slider&) override;
};

// perilla con titulo y valor
class Knob : public juce::Component
{
public:
    Knob (APVTS&, const juce::String& id, const juce::String& title, juce::Colour accent);
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::Slider slider;
private:
    juce::String title;
    std::unique_ptr<APVTS::SliderAttachment> att;
};

// fila de 12 teclas (una octava) que activa/desactiva parametros booleanos
class KeyRow : public juce::Component
{
public:
    KeyRow (APVTS&, const juce::String& prefix, juce::StringArray labels, juce::Colour accent);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    std::function<void()> onUserChange;
    int highlight = -1;          // tecla resaltada (nota sonando)
private:
    juce::Rectangle<float> keyRect (int i) const;
    APVTS& state;
    juce::String prefix;
    juce::StringArray labels;
    juce::Colour accent;
};

// grafica de afinacion: linea de tu voz + bloques de las notas MIDI
class PitchRoll : public juce::Component
{
public:
    explicit PitchRoll (TopVoiceMidiProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
private:
    TopVoiceMidiProcessor& proc;
    float lo = 48, hi = 72;
};

// archivo .mid que se arrastra al DAW
class DragTile : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    juce::File file;
};

class Pad : public juce::Component
{
public:
    Pad (TopVoiceMidiProcessor&, int index, juce::Colour);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick();
    float flash = 0;
private:
    TopVoiceMidiProcessor& proc;
    int index;
    juce::Colour colour;
    juce::TextButton train, clear { "Borrar" };
    juce::Slider note;
    std::unique_ptr<APVTS::SliderAttachment> noteAtt;
};
} // namespace vmui

class TopVoiceMidiEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TopVoiceMidiEditor (TopVoiceMidiProcessor&);
    ~TopVoiceMidiEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void showTab (int);
    void tickNow() { timerCallback(); }
    static constexpr int W = 1100, H = 690;

private:
    void timerCallback() override;
    void layoutTab();
    void refreshDevices();
    juce::Component* make (juce::Component* c) { addChildComponent (c); return c; }

    TopVoiceMidiProcessor& proc;
    vmui::Look look;
    int tab = 0;
    juce::Rectangle<int> leftR, tabR, contentR;

    // cabecera
    juce::OwnedArray<juce::TextButton> modeBtns, tabBtns;
    juce::ComboBox deviceBox;
    juce::TextButton refreshBtn { juce::String::fromUTF8 ("\xe2\x86\xbb") };
    juce::Array<juce::MidiDeviceInfo> devices;

    // monitor (izquierda)
    vmui::Knob gain, gate, synthVol, dry;
    juce::ToggleButton synthOn { "Sinte de prueba" };
    juce::ComboBox waveBox;
    std::unique_ptr<vmui::APVTS::ButtonAttachment> synthOnAtt;
    std::unique_ptr<vmui::APVTS::ComboBoxAttachment> waveAtt;

    // melodia
    vmui::PitchRoll roll;
    vmui::Knob octave, response, stability, velSens, channel;

    // ritmo
    juce::OwnedArray<vmui::Pad> pads;
    vmui::Knob trigSens, trigCh;

    // armonia
    juce::ComboBox keyBox, scaleBox, chordBox, voicingBox;
    juce::ToggleButton autoKey { juce::String::fromUTF8 ("Tonalidad automática") }, chordOn { "Acordes" };
    vmui::KeyRow scaleKeys, chordKeys;
    juce::TextButton learnBtn { "APRENDER CON MI VOZ" };
    vmui::Knob inversion;
    std::unique_ptr<vmui::APVTS::ComboBoxAttachment> keyAtt, scaleAtt, chordAtt, voicingAtt;
    std::unique_ptr<vmui::APVTS::ButtonAttachment> autoAtt, chordOnAtt;

    // expresion
    juce::ComboBox bendBox;
    std::unique_ptr<vmui::APVTS::ComboBoxAttachment> bendAtt;
    vmui::Knob bendRange, bendThr;
    juce::OwnedArray<juce::ToggleButton> ccOn;
    juce::OwnedArray<juce::ComboBox> ccSrc;
    juce::OwnedArray<vmui::Knob> ccNum, ccSmooth;
    std::vector<std::unique_ptr<vmui::APVTS::ButtonAttachment>> ccOnAtt;
    std::vector<std::unique_ptr<vmui::APVTS::ComboBoxAttachment>> ccSrcAtt;

    // captura
    juce::TextButton recBtn { "GRABAR" }, clearBtn { "BORRAR" }, exportBtn { "EXPORTAR .MID" }, folderBtn { "Abrir carpeta" };
    juce::ToggleButton ghosts { "Limpiar notas fantasma" };
    vmui::DragTile tile;
    int capturedCount = 0, lastHitSerial = 0;
    double recStart = 0;

    juce::TooltipWindow tips { this, 500 };
    std::vector<std::vector<juce::Component*>> tabComps;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopVoiceMidiEditor)
};
