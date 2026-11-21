#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "ConsoleDsp.h"

// TOP CONSOLE B: canal de consola de estilo serie 4000 B (diseno propio).
class TopConsoleBProcessor : public juce::AudioProcessor
{
public:
    TopConsoleBProcessor();
    ~TopConsoleBProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Top Console B"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.2; }
    int getNumPrograms() override;
    int getCurrentProgram() override { return program; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState apvts;
    juce::ValueTree abStore[2]; int abSlot = 0;          // comparacion A/B
    tcb::Params readParams() const;

    std::atomic<float> inPeakL { 0 }, inPeakR { 0 }, outPeakL { 0 }, outPeakR { 0 };
    std::atomic<float> inPeak { 0 }, inRms { 0 }, outPeak { 0 }, outRms { 0 };
    float compGr() const { return (hqActive ? strip2 : strip1).compGr.load(); }
    float gateGr() const { return (hqActive ? strip2 : strip1).gateGr.load(); }

private:
    double sr = 44100.0;
    tcb::Strip strip1, strip2;
    std::unique_ptr<juce::dsp::Oversampling<float>> os, osSc;
    bool hqActive = false;
    int maxBlock = 512, program = 0;
    std::vector<float> monoR, scA, scB;
    float inRmsAcc = 0, outRmsAcc = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopConsoleBProcessor)
};
