#pragma once

#include "UI/ConsoleWidgets.h"
#include "PluginProcessor.h"

// Interfaz estilo tira de consola (diseno original). Cada modulo es un componente
// independiente: se puede ocultar, y toda la interfaz se escala de 100 % a 200 %.
namespace tvcui
{
using cui::APVTS;

enum ModuleId { mPre, mAi, mTune, mWide, mComp, mEq, mDly, mRev, mMet, mOut, numModules };

//==============================================================================
class Module : public juce::Component
{
public:
    Module (TopVocalChainProcessor&, const juce::String& title, const char* onParam, int seed);
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    virtual void tick();

protected:
    virtual void paintStatic (juce::Graphics&) {}
    virtual void paintDynamic (juce::Graphics&) {}
    virtual void layout() {}
    virtual int status() const;              // 0 apagado, 1 activo, 2 saturando

    bool param (const char* id) const { return proc.apvts.getRawParameterValue (id)->load() > 0.5f; }
    int choice (const char* id) const { return (int) proc.apvts.getRawParameterValue (id)->load(); }
    float value (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }
    void attach (juce::Button&, const char* id);
    void setupBox (juce::ComboBox&, const juce::StringArray&, const char* id);
    void add (juce::Component& c) { addAndMakeVisible (c); }

    TopVocalChainProcessor& proc;
    juce::String title;
    const char* onParam;
    int seed;
    std::unique_ptr<cui::ModuleSwitch> inSwitch;
    std::vector<std::unique_ptr<APVTS::ButtonAttachment>> btnAtts;
    std::vector<std::unique_ptr<APVTS::ComboBoxAttachment>> boxAtts;

private:
    juce::Image cache;
    bool lastOn = true;
};

//==============================================================================
class MainView : public juce::Component
{
public:
    static constexpr int H = 900, minW = 1400;
    explicit MainView (TopVocalChainProcessor&);
    ~MainView() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick();
    int getCanvasWidth() const;
    std::function<void()> onSizeChange;

private:
    void showModulesMenu();
    void refreshPresetBox();
    bool isHidden (int id) const { return (proc.hiddenModules.load() >> id) & 1; }

    TopVocalChainProcessor& proc;
    std::array<std::unique_ptr<Module>, numModules> modules;

    juce::TextButton modulesBtn { U8 ("MÓDULOS") }, undoBtn { "UNDO" }, redoBtn { "REDO" }, resetBtn { "RESET" };
    juce::OwnedArray<cui::LitButton> scaleBtns;
    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, saveBtn { "GUARDAR" }, loadBtn { "CARGAR" };
    std::unique_ptr<juce::FileChooser> chooser;

    static juce::String U8 (const char* s) { return juce::String::fromUTF8 (s); }
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
    void updateSize();
    static float scaleFactor (int index);

private:
    void timerCallback() override;
    TopVocalChainProcessor& proc;
    cui::LnF lnf;
    tvcui::MainView view;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopVocalChainEditor)
};
