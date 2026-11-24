#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "L3aDsp.h"

// TOP 3A: nivelador optico de estado solido (estilo LA-3A). Celda T4 simulada (panel EL + fotorresistencia con
// memoria), amplificadores clase A y transformadores; todo a 4x en doble precision.
class Top3AProcessor : public juce::AudioProcessor
{
public:
    Top3AProcessor();
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Top 3A"; }
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
    l3a::Settings settings() const;
    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> grDb { 0.0f }, outAvg { 0.0f };       // para el VU (reduccion en dB / salida rectificada promedio)
    const l3a::Leveler& engine() const { return lv; }
private:
    juce::dsp::Oversampling<float> os { 2 };                 // 4x con filtros de -110 dB y latencia entera (ver constructor)
    l3a::Leveler lv;
    juce::AudioBuffer<float> dry;
    std::array<std::vector<float>, 2> delay;
    int delayPos = 0, latency = 0;
    juce::SmoothedValue<float> mixSm, onSm;
    double avg = 0, avgA = 0.001;
    int currentProgram = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Top3AProcessor)
};
