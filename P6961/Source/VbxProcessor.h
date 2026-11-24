#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "VbxDsp.h"

// Top VoxStrips: canal de voz a valvulas (compresor optico ELOP + preamp + EQ pasivo + de-esser LC / limitador)
class VbxProcessor : public juce::AudioProcessor
{
public:
    VbxProcessor();
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Top VoxStrips"; }
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
    vbx::EqSettings eqSettings() const;
    juce::AudioProcessorValueTreeState apvts;
    // medidor: niveles (pico lineal) y reducciones (dB)
    std::atomic<float> mLine { 0 }, mPre { 0 }, mEq { 0 }, mComp { 0 }, mDs { 0 };
    int oversamplingFactor() const { return (int) os.getOversamplingFactor(); }
private:
    // toda la cadena corre a 8x con filtros de -110 dB y latencia entera
    juce::dsp::Oversampling<float> os { 2 };
    struct Ch
    {
        vbx::EqFilters eq;
        vbx::Preamp pre;
        vbx::LineAmp line;
        vbx::LcNotch notch;
        vbx::Bq dsSc;
        vbx::AdaaEq adaa;
    };
    std::array<Ch, 2> ch;
    std::array<vbx::Elop, 2> comp, ds;
    vbx::EqSettings lastEq; bool designed = false;
    int lastDsF = -1, lastGain = -1;
    double fs8 = 384000.0;
    juce::AudioBuffer<float> dry;
    std::array<std::vector<float>, 2> delay;
    int delayPos = 0, latency = 0;
    juce::SmoothedValue<double> driveSm, inSm;
    juce::SmoothedValue<float> outSm, onSm;
    int currentProgram = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VbxProcessor)
};
