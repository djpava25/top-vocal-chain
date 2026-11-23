#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "N73Dsp.h"

// TOP 73: preamp + EQ clase A con transformadores (estilo 1073), curvas medidas por impulso
class Top73Processor : public juce::AudioProcessor
{
public:
    Top73Processor();
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Top 73"; }
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
    n73::Settings settings() const;
    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> inPeak { 0.0f }, outPeak { 0.0f };
private:
    juce::dsp::Oversampling<float> os { 2, 1, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true };
    std::array<n73::Channel, 2> ch;
    n73::Settings last; bool designed = false;
    juce::AudioBuffer<float> dry;
    std::array<std::vector<float>, 2> delay;
    int delayPos = 0, latency = 0;
    juce::SmoothedValue<float> outSm, onSm, compSm;
    int currentProgram = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Top73Processor)
};
