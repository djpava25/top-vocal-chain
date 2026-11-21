#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "SmoothDsp.h"

// TOP SMOOTH: supresor dinamico de resonancias (quita dureza, silbidos y "picos" del sonido
// solo cuando aparecen). Modos Suave/Duro, fase lineal o baja latencia (0 ms), EQ de foco,
// estereo enlazado / L-R / Mid-Side, sidechain externo, delta para escuchar lo que se quita.
class TopSmoothProcessor : public juce::AudioProcessor
{
public:
    TopSmoothProcessor();
    ~TopSmoothProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Top Smooth"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.1; }
    int getNumPrograms() override;
    int getCurrentProgram() override { return program; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;
    tsm::Settings currentSettings() const;
    tsm::Display display;
    std::atomic<float> inDb { -100.0f }, outDb { -100.0f };

private:
    void applyLatencyMode (int mode);
    double sr = 44100.0;
    tsm::SpectralEngine spectral;
    tsm::FilterBankEngine bank;
    int engineMode = -1, program = 0;
    static constexpr int chunk = 512;
    std::array<std::vector<float>, 2> work, side, dry;
    std::array<std::vector<float>, 2> delay;
    int delayPos = 0;
    float trimSm = 1.0f, mixSm = 1.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopSmoothProcessor)
};
