#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "DSP/Biquad.h"
#include "DSP/PitchCorrector.h"
#include "DSP/VoiceAI.h"
#include "DSP/Effects.h"
#include "Presets.h"

//==============================================================================
// Valores que la interfaz lee en tiempo real (escritos por el hilo de audio)
struct LiveMeters
{
    std::atomic<float> inLevelDb { -100.0f }, outLevelDb { -100.0f }, compGrDb { 0.0f };
    std::atomic<float> pitchHz { 0.0f }, targetNote { -1.0f }, correctionCents { 0.0f };

    std::atomic<float> aiGain { 0 }, aiLowCut { 0 }, aiMud { 0 }, aiPresence { 0 }, aiAir { 0 },
                       aiDeEss { 0 }, aiCompThr { 0 }, aiCompRatio { 0 }, aiTuneSpeed { 0 },
                       aiConfidence { 0 }, aiAvgPitch { 0 };
    std::atomic<float> stereoWidth { 0 };   // energia lateral / central (0 = mono)
};

//==============================================================================
class TopVocalChainProcessor : public juce::AudioProcessor
{
public:
    TopVocalChainProcessor();
    ~TopVocalChainProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Top Vocal Chain"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Presets de usuario (archivos .tvcpreset)
    static juce::File getUserPresetFolder();
    bool saveUserPreset (const juce::File& file);
    bool loadUserPreset (const juce::File& file);

    void requestAIReset() { aiResetRequested.store (true); }

    juce::AudioProcessorValueTreeState apvts;
    LiveMeters meters;

    static const juce::StringArray& keyNames();
    static const juce::StringArray& scaleNames();
    static const juce::StringArray& divisionNames();
    static const juce::StringArray& preTypeNames();
    static const juce::StringArray& compTypeNames();
    static const juce::StringArray& eqTypeNames();
    static const juce::StringArray& wideModeNames();

    // Respuesta del EQ actual (para dibujar la curva). Lectura aproximada desde la interfaz.
    float getEqMagnitudeDb (double freq) const;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float p (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    bool  on (const char* id) const { return p (id) >= 0.5f; }
    void updateEq (float aiAmt);
    double getDelayMs() const;

    double sr = 44100.0;
    int maxBlock = 512;
    int currentProgram = 0;
    std::atomic<bool> aiResetRequested { false };

    // Modulos
    tvc::PitchDetector detector;
    tvc::PitchShifter shifter[2];
    float corrSemis = 0.0f, corrTarget = 0.0f;
    int heldNote = -1;
    int samplesSinceEstimate = 0;
    bool delayWasOn = true;

    tvc::VoiceAI ai;
    juce::HeapBlock<float> monoBuffer;

    juce::dsp::Oversampling<float> oversampling { 2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false };
    tvc::PreampStage preamp;

    enum EqBand { bLowCut, bLowCut2, bLow, bLowDip, bMid, bPres, bAir, bAiMud, bAiDeEss, numBands };
    enum FilterKind { fIdentity, fHP2, fHP1, fLowShelf, fHighShelf, fPeak };
    struct BandSpec { int kind = fIdentity; float f = 1000.0f, q = 0.707f, g = 0.0f; };
    tvc::Biquad eq[numBands][2];
    BandSpec eqCache[numBands];
    bool eqCacheValid = false;

    tvc::VocalCompressor comp;
    tvc::StereoDelay delay;
    tvc::Widener widener;
    double wideMs = 0.0, wideSs = 0.0;

    juce::Reverb reverb;
    juce::AudioBuffer<float> reverbBuffer;
    tvc::Biquad revHp[2];

    juce::SmoothedValue<float> inGainSm, outGainSm;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopVocalChainProcessor)
};
