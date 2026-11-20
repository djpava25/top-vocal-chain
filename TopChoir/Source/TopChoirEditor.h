#pragma once

#include "TopChoirProcessor.h"

class TopChoirEditor : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit TopChoirEditor (TopChoirProcessor&);
    ~TopChoirEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragOver = false; repaint(); }
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void tickNow() { timerCallback(); }
    static constexpr int W = 440, H = 900;
private:
    void timerCallback() override;
    void buildPeaks();
    void browse();
    void showSettings();

    struct LnF : juce::LookAndFeel_V4
    {
        LnF();
        void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
        void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    } lnf;

    TopChoirProcessor& proc;
    juce::TextButton startOver { "EMPEZAR" }, gear { "..." }, play { ">" }, mixBtn { "M" }, clearBtn { "X" }, browseBtn { "Buscar archivo" },
                     modelL { "<" }, modelR { ">" }, micL { "<" }, micR { ">" }, octUp, octDown, keyBtn, scaleBtn,
                     thirdUp, thirdDown, fifthUp, fifthDown, stereoBtn, create, openFolder;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> atts;
    std::unique_ptr<juce::FileChooser> chooser;
    std::vector<float> peaks;
    juce::File peaksFor; bool peaksChoir = false;
    bool dragOver = false;
    juce::String status;
    juce::Rectangle<int> waveR { 70, 104, 350, 92 }, dropR { 20, 236, 400, 110 }, resultR { 20, 810, 400, 70 };
};
