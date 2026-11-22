#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "MeterDsp.h"

// TOP METER: medidor profesional (sonoridad LUFS, True Peak, PPM/RMS, VU, goniometro y espectro)
class TopMeterProcessor : public juce::AudioProcessor
{
public:
    TopMeterProcessor();
    ~TopMeterProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Top Meter"; }
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
    float targetLufs() const;
    void resetMeters() { wantReset = true; }
    double getRate() const { return sr; }

    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState apvts;
    tmeter::Meter meter;
    juce::String channelName { "MASTER" };
    int instanceNumber = 1;
    juce::ValueTree abStore[2]; int abSlot = 0;

    // muestras para el goniometro y el espectro (escritas por el audio, leidas por la interfaz)
    static constexpr int scopeSize = 8192, specSize = 16384;
    std::vector<float> scopeL, scopeR, specBuf;
    std::atomic<int> scopeW { 0 }, specW { 0 };
    std::atomic<bool> hostPlaying { false };

    static const juce::StringArray& targetNames();
    static const std::vector<float>& targetValues();

private:
    double sr = 48000.0;
    int currentProgram = 0;
    bool wasPlaying = false;
    std::atomic<bool> wantReset { false };
    int lastRmsChoice = -1; float lastVuRef = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopMeterProcessor)
};
