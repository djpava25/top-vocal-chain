#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "MiniDsp.h"

// TOP MINI CH: canal compacto estilo mini consola (preamp, filtro 75 Hz, EQ de 2 bandas, compresor de una perilla)
class TopMiniCHProcessor : public juce::AudioProcessor
{
public:
    TopMiniCHProcessor();
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Top Mini CH"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    tmc::Params readParams() const;
    float grDb() const { return gr.load(); }

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> outPeak { 0.0f }, gr { 0.0f };
private:
    tmc::Channel ch;
    int currentProgram = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopMiniCHProcessor)
};
