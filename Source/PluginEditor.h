#pragma once

#include "UI/ConsoleWidgets.h"
#include "PluginProcessor.h"

// Interfaz estilo tira de consola (diseno original): modulos verticales lado a lado.
namespace tvcui
{
class MainView : public juce::Component
{
public:
    static constexpr int W = 1492, H = 900;
    explicit MainView (TopVocalChainProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick();
    void refreshPresetBox();

private:
    void buildBackground();
    void drawDisplays (juce::Graphics&);
    bool param (const char* id) const { return proc.apvts.getRawParameterValue (id)->load() > 0.5f; }
    int choice (const char* id) const { return (int) proc.apvts.getRawParameterValue (id)->load(); }
    void attachButton (juce::Button&, const char* id);
    void setupBox (juce::ComboBox&, const juce::StringArray&, const char* id);

    TopVocalChainProcessor& proc;
    juce::Image background;

    // Barra superior
    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, saveBtn { "GUARDAR" }, loadBtn { "CARGAR" };
    std::unique_ptr<juce::FileChooser> chooser;

    // Selectores
    juce::ComboBox preType, compType, eqType, keyBox, scaleBox, divBox;
    std::vector<std::unique_ptr<cui::APVTS::ComboBoxAttachment>> boxAtts;

    // Botones iluminados
    cui::LitButton preIn { "IN", cui::Col::red }, tuneIn { "IN", cui::Col::ledGreen }, wideIn { "IN", cui::Col::ledGreen },
                   compIn { "IN", cui::Col::ledGreen }, eqIn { "IN", cui::Col::ledGreen }, dlyIn { "IN", cui::Col::ledGreen },
                   revIn { "IN", cui::Col::ledGreen }, aiListen { "ESCUCHAR", cui::Col::amber },
                   dlySync { "SYNC", cui::Col::amber }, dlyPing { "PING PONG", cui::Col::amber };
    std::vector<std::unique_ptr<cui::APVTS::ButtonAttachment>> btnAtts;
    juce::OwnedArray<cui::LitButton> wideModes;
    juce::TextButton aiReset { "RESET" };

    // Perillas
    cui::Knob drive, inGain, warmth, trim, aiAmount, tuneSpeed, tuneAmount, wideAmount;
    cui::Knob thr, ratio, attack, release, makeup;
    cui::Knob air, presence, midFreq, midGain, midQ, low, lowCut;
    cui::Knob dTime, dFeedback, dMix, dHiCut, rSize, rDamp, rWidth, rMix;
    cui::Fader output;
    cui::LedMeter inMeter, outMeter, grMeter;

    juce::Rectangle<int> tuneDisp, aiDisp, wideDisp, eqCurve, eqInfo, compInfo, preInfo, dlyDisp, meterDisp, noteDisp;
};
} // namespace tvcui

//==============================================================================
class TopVocalChainEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TopVocalChainEditor (TopVocalChainProcessor&);
    ~TopVocalChainEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    cui::LnF lnf;
    tvcui::MainView view;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopVocalChainEditor)
};
