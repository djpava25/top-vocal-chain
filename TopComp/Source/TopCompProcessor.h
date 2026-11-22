#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "DSP/Biquad.h"
#include "TubeOptoCompressor.h"

// TOP BLUE COMP: compresor optico a valvulas (estilo CL 1B) con el motor TubeOptoCompressor:
// codo suave de 10 dB, release optico en dos etapas, valvula asimetrica y transformador de salida.
class TopCompProcessor : public juce::AudioProcessor
{
public:
    TopCompProcessor();
    ~TopCompProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Top Blue Comp"; }
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

    static const juce::StringArray& selectNames();
    static const juce::StringArray& hpfNames();
    static const juce::StringArray& modeNames();

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> grDb { 0.0f }, outLevelDb { -100.0f }, inLevelDb { -100.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float p (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    bool on (const char* id) const { return p (id) > 0.5f; }

    int currentProgram = 0;
    double sr = 44100.0, osr = 88200.0;
    int maxBlock = 512;

    juce::dsp::Oversampling<float> oversampling { 2, 1, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true };
    tvc::Biquad scHp[2];
    TubeOptoCompressor comp;
    int lastHpf = -1;
    float dcX[2] {}, dcY[2] {};

    juce::SmoothedValue<float> gainSm, mixSm, powerSm;
    std::vector<float> dryRing[2];
    int dryPos = 0, dryLatency = 0;
    juce::AudioBuffer<float> dryBuf;
    float inPeak = 0, outPeak = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopCompProcessor)
};
