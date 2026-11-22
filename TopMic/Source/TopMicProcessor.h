#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "MicModels.h"

// TOP MIC COLLECTION: emulador de microfonos clasicos con patron, eje, proximidad, filtro y modo dual
class TopMicProcessor : public juce::AudioProcessor
{
public:
    TopMicProcessor();
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Top Mic Collection"; }
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
    tmic::Settings settings (bool b) const;
    int effectivePattern (bool b) const;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> inPeak { 0.0f };
private:
    tmic::MicChain micA, micB;
    juce::SmoothedValue<float> outSm, blendSm;
    int currentProgram = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopMicProcessor)
};
