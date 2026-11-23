#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "VbxDsp.h"

// 6961P: canal de voz a valvulas (preamp + compresor optico + EQ pasivo + de-esser/limitador)
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
    const juce::String getName() const override { return "6961P"; }
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
private:
    juce::dsp::Oversampling<float> os { 2, 1, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true };
    // el preamp corre 4x mas (8x en total) para que la saturacion no deje aliasing
    juce::dsp::Oversampling<float> osPre { 2, 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true };
    std::array<vbx::Preamp, 2> pre;
    std::array<vbx::LineAmp, 2> line;
    std::array<float, 2> pad1 { 0, 0 };
    bool padOdd = false;
    int lastGain = -1;
    juce::SmoothedValue<float> driveSm;
    struct Ch
    {
        std::array<tvc::Biquad, 4> eq;
        tvc::Biquad dsSc, dsAud;
    };
    std::array<Ch, 2> ch;
    std::array<vbx::Opto, 2> comp, ds;
    vbx::EqSettings lastEq; bool designed = false;
    int lastDsF = -1;
    juce::AudioBuffer<float> dry;
    std::array<std::vector<float>, 2> delay;
    int delayPos = 0, latency = 0;
    juce::SmoothedValue<float> outSm, onSm, inSm;
    int currentProgram = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VbxProcessor)
};
