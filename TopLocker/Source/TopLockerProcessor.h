#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "LockerDsp.h"

// TOP LOCKER: coleccion virtual de microfonos con preamp, sala y presets por genero
class TopLockerProcessor : public juce::AudioProcessor
{
public:
    TopLockerProcessor();
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Top Locker"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 2.8; }
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    lk::Params params() const;
    float roomPickup() const { return engine.roomFactor(); }

    // presets del usuario
    static juce::File userPresetDir();
    juce::Array<juce::File> userPresets() const;
    bool saveUserPreset (const juce::File&);
    bool loadUserPreset (const juce::File&);
    juce::String presetName;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> inPeak { 0.0f }, outPeak { 0.0f };
private:
    lk::Engine engine;
    juce::dsp::Oversampling<float> os { 2, 1, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true };
    juce::dsp::ConvolutionMessageQueue queue;
    std::array<std::unique_ptr<juce::dsp::Convolution>, 5> convs;
    juce::AudioBuffer<float> dry, roomBuf;
    std::array<std::vector<float>, 2> delay;
    int delayPos = 0, latency = 0, lastRoom = -1;
    juce::SmoothedValue<float> outSm, mixSm, roomSm;
    int currentProgram = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopLockerProcessor)
};
