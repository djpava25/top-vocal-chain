#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "DSP/Biquad.h"

// TOP STRIP 737: channel strip de valvula (preamp + compresor optico + EQ de 4 bandas)
class TopStripProcessor : public juce::AudioProcessor
{
public:
    TopStripProcessor();
    ~TopStripProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Top Strip 737"; }
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

    static const juce::StringArray& bassFreqs();
    static const juce::StringArray& lowMidFreqs();
    static const juce::StringArray& highMidFreqs();
    static const juce::StringArray& trebleFreqs();
    static float choiceHz (const juce::StringArray& list, int index);

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> inLevelDb { -100.0f }, outLevelDb { -100.0f }, grDb { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float p (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    bool on (const char* id) const { return p (id) > 0.5f; }
    void updateFilters();
    float tube (float x, int ch) noexcept;

    int currentProgram = 0;
    double sr = 44100.0;
    int maxBlock = 512;

    juce::dsp::Oversampling<float> oversampling { 2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false };
    tvc::Biquad hpf[2], bass[2], lowMid[2], highMid[2], treble[2], scBand[2];
    float dcX[2] {}, dcY[2] {}, dcCoef = 0.9995f;
    float tubeDrive = 1.0f, tubeComp = 1.0f;

    // Compresor optico
    float env = 0.0f, optoSlow = 0.0f, grSm = 0.0f;

    juce::SmoothedValue<float> inGainSm, outGainSm, powerSm;
    juce::AudioBuffer<float> dryBuf;
    std::vector<float> dryRing[2];
    int dryPos = 0, dryLatency = 0;
    float inPeak = 0.0f, outPeak = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopStripProcessor)
};
