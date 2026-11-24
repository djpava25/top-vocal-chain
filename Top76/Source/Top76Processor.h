#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "F76Dsp.h"

// TOP 76: amplificador limitador FET (estilo 1176LN blackface). Lazo de realimentacion resuelto por muestra,
// FET, clase A y transformadores; todo a 4x en doble precision. Marca propia AnalogHM.
class Top76Processor : public juce::AudioProcessor
{
public:
    Top76Processor();
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Top 76"; }
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
    static constexpr float attackOffBelow = 0.6f;            // ATTACK por debajo de esto = posicion OFF
    f76::Settings settings() const;
    bool powered() const { return apvts.getRawParameterValue ("meter")->load() < 2.5f; }
    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> grDb { 0.0f }, outAvg { 0.0f };       // VU: reduccion (dB) / salida rectificada promedio
    f76::Engine& engine() { return eng; }
private:
    juce::dsp::Oversampling<float> os { 2 };
    f76::Engine eng;
    juce::AudioBuffer<float> dry;
    std::array<std::vector<float>, 2> delay;
    int delayPos = 0, latency = 0;
    juce::SmoothedValue<float> mixSm, onSm;
    double avg = 0, avgA = 0.001;
    int currentProgram = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Top76Processor)
};
