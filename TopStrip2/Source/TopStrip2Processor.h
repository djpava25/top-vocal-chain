#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Strip2Dsp.h"

// TOP STRIP 737: canal de valvulas clase A (preamp 6922 + compresor optico + EQ discreto de 4 bandas)
// modelado a nivel de circuito y calibrado con las especificaciones del hardware (ver Strip2Dsp.h)
class TopStrip2Processor : public juce::AudioProcessor
{
public:
    TopStrip2Processor();
    ~TopStrip2Processor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Top Strip 737 V2"; }
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

    // respuesta analogica del EQ + pasa altos con los ajustes actuales (para la interfaz / pruebas)
    double eqResponseDb (double hz) const;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> inLevelDb { -100.0f }, outLevelDb { -100.0f }, grDb { 0.0f }, vuDb { -100.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float p (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    bool on (const char* id) const { return p (id) > 0.5f; }

    struct EqState
    {
        float bass = 0, lm = 0, hm = 0, tr = 0, hpf = 60;
        int bf = 2, lf = 5, hf = 8, tf = 1;
        bool lq = false, lx = false, hq = false, hx = false;
        bool operator== (const EqState& o) const
        {
            return bass == o.bass && lm == o.lm && hm == o.hm && tr == o.tr && hpf == o.hpf && bf == o.bf && lf == o.lf
                && hf == o.hf && tf == o.tf && lq == o.lq && lx == o.lx && hq == o.hq && hx == o.hx;
        }
    };
    EqState targetEq() const;
    void design (const EqState&);

    struct Channel
    {
        s737b::Transformer inT;
        s737b::Tube pre, comp, out;
        s737b::OnePole hpf, bass, treble;
        s737b::Bq lm, hm, scLm, scHm;
        s737b::AdaaEq adaaEq;
        s737b::OnePole inLp, inHp;                  // respuesta medida de la entrada (V2)
        double dry[8] {};                           // senal directa alineada (MIX), a 8x
    };
    s737b::CurveTable preTab[3][2], compTab, outTab;     // curvas de las valvulas (LINE/INST/MIC x HIGH GAIN)

    int currentProgram = 0;
    double sr = 44100.0, osRate = 352800.0;
    int maxBlock = 512, latency = 0;
    int lastMode = -1; bool lastHigh = false;

    juce::dsp::Oversampling<float> os { 2 };          // 8x con filtros de -110 dB (ver constructor)
    Channel ch[2];
    s737b::Opto opto;
    EqState cur;
    bool designed = false;

    juce::SmoothedValue<float> preSm, outSm, powerSm, mixSm;
    int dryPosOs = 0;
    juce::SmoothedValue<float> eqSm[5];                // bass, low mid, high mid, treble, hpf

    juce::AudioBuffer<float> dryBuf;
    std::vector<float> dryRing[2];
    int dryPos = 0;
    float inPeak = 0.0f, outPeak = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopStrip2Processor)
};
