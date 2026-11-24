#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "SpqProcessor.h"

// Interfaz de Top Spectra EQ: pantalla grande con analizador y nodos que se arrastran (flujo rapido sin menus),
// y una franja inferior con los controles de la banda elegida.
namespace spqui
{
using APVTS = juce::AudioProcessorValueTreeState;

class Look : public juce::LookAndFeel_V4
{
public:
    Look();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int h) override;
    juce::Font getPopupMenuFont() override;
    void drawLabel (juce::Graphics&, juce::Label&) override;
};

juce::Colour bandColour (int i);

class Graph : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit Graph (SpqProcessor&);
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void updateAnalyzer();
    std::function<void (int)> onSelect;
    int selected = -1;
    float dbRange = 12.0f;
    int anaMode = 3;                                   // 0 off, 1 pre, 2 post, 3 pre + post
private:
    float xOfF (double f) const;
    double fOfX (float x) const;
    float yOfDb (double db) const;
    double dbOfY (float y) const;
    juce::Point<float> nodePos (int i) const;
    int hitNode (juce::Point<float>) const;
    bool hasGain (int type) const;
    void bandMenu (int i);
    int createBand (double f, double g, int type);
    void setP (int band, const char* what, float v);
    double bandDb (int i, double f, double extraDb) const;
    double peakNear (double f) const;
    SpqProcessor& proc;
    int hover = -1, dragging = -1; bool grab = false;
    juce::Point<float> downPos; double downF = 0, downG = 0, downQ = 1;
    juce::Point<float> mousePos { -100, -100 };
    // analizador
    juce::dsp::FFT fft { 13 };
    std::vector<float> fftBuf, win;
    std::vector<float> specPre, specPost;              // dB en 400 puntos log
};

class Content : public juce::Component, private juce::Timer
{
public:
    explicit Content (SpqProcessor&);
    ~Content() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    static constexpr int W = 1200, H = 680;
    void tick() { timerCallback(); }
    void selectBand (int b) { select (b); }
private:
    void timerCallback() override;
    void select (int band);
    void refreshPanel();
    SpqProcessor& proc;
    Graph graph;
    // barra superior
    juce::ComboBox mode, character, ana, range;
    juce::TextButton autoGain { "AUTO GAIN" }, undoB { "UNDO" }, redoB { "REDO" }, prevP { "<" }, nextP { ">" }, presetName;
    juce::Slider scale, output;
    std::unique_ptr<APVTS::ComboBoxAttachment> modeAtt, charAtt;
    std::unique_ptr<APVTS::ButtonAttachment> autoAtt;
    std::unique_ptr<APVTS::SliderAttachment> scaleAtt, outAtt;
    // panel de banda
    juce::ComboBox type, slope, place;
    juce::Slider freq, gain, q, thr, rangeK, att, rel;
    juce::TextButton dOff { "OFF" }, dDyn { "DYNAMIC" }, dSpec { "SPECTRAL" }, below { "BELOW" }, soloB { "SOLO" }, del { "DELETE" };
    std::unique_ptr<APVTS::ComboBoxAttachment> typeAtt, slopeAtt, placeAtt;
    std::unique_ptr<APVTS::SliderAttachment> fAtt, gAtt, qAtt, thrAtt, rgAtt, atAtt, rlAtt;
    std::unique_ptr<APVTS::ButtonAttachment> belowAtt;
    int sel = -1, lastDm = -1;
};
} // namespace spqui

class SpqEditor : public juce::AudioProcessorEditor
{
public:
    explicit SpqEditor (SpqProcessor&);
    ~SpqEditor() override;
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff101216)); }
    static constexpr int W = spqui::Content::W, H = spqui::Content::H;
    spqui::Content& getContent() { return content; }
private:
    spqui::Look look;
    spqui::Content content;
    juce::TooltipWindow tips { this, 700 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpqEditor)
};
