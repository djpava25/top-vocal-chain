#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "SummingDSP.h"

class TopSummingProcessor : public juce::AudioProcessor
{
public:
    TopSummingProcessor();
    ~TopSummingProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Top Summing"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Nueva "unidad" analogica (otras tolerancias de componentes)
    void newAnalogUnit();
    unsigned getUnitSeed() const { return unitSeed.load(); }

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> inLevelDb { -100.0f }, outLevelDb { -100.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float p (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    void applyTolerances();

    int currentProgram = 0;
    std::atomic<unsigned> unitSeed { 1 };
    std::atomic<bool> tolerancesDirty { true };
    bool lastAnalog = true;

    juce::dsp::Oversampling<float> oversampling { 2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false };
    topsum::ConsoleChannel chan[2];
    tvc::Biquad xtalkHp[2];
    juce::Random noise;
    juce::SmoothedValue<float> driveSm, outSm, widthSm, ironSm, xtalkSm;
    int maxBlock = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopSummingProcessor)
};
