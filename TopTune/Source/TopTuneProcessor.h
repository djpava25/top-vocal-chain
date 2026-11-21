#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/PitchCorrector.h"
#include "DSP/Psola.h"

//==============================================================================
struct TopTuneMeters
{
    static constexpr int historySize = 400;
    std::atomic<float> pitchHz { 0 }, inMidi { 0 }, outMidi { 0 }, targetNote { -1 };
    std::atomic<int> histPos { 0 };
    float inHist[historySize] {}, outHist[historySize] {};   // 0 = sin voz
    std::atomic<float> keyHist[12] {};
    std::atomic<bool> keyListening { false };
    std::atomic<int> customMask { 0xAB5 };                  // C mayor por defecto
    std::atomic<bool> customMaskLearned { false };
};

//==============================================================================
class TopTuneProcessor : public juce::AudioProcessor
{
public:
    TopTuneProcessor();
    ~TopTuneProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Top Tune"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.1; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Mascara de 12 bits de la escala activa (bit 0 = C)
    int getActiveMask() const;
    void startKeyDetection();
    void stopKeyDetection() { meters.keyListening.store (false); }

    static const juce::StringArray& keyNames();
    static const juce::StringArray& scaleNames();
    static constexpr int customScaleIndex = 7;

    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState apvts;
    TopTuneMeters meters;
    bool lightTheme = false;                 // tema de la interfaz (negro / plata)
    static constexpr int modeRealtime = 0, modeLowLat = 1, modeHighQ = 2;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float p (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    void handleMidi (const juce::MidiBuffer&);
    int nearestInMask (float midi, int maskBits) const;
    void updateLatency();

    double sr = 44100.0;
    int currentProgram = 0;
    long long sampleClock = 0;

    tvc::PitchDetector detector;
    toptune::Psola psolaHQ, psolaLL;
    toptune::CycleShifter cycle;
    int engineMode = modeHighQ;

    // Estado de la correccion
    float corrSemis = 0.0f, corrTarget = 0.0f, smoothMidi = 0.0f, holdTime = 0.0f;
    int heldNote = -1, samplesSinceEstimate = 0;
    double period = 0.0, periodSm = 0.0;
    bool voiced = false;
    // seguimiento estable del tono: histeresis de voz y proteccion contra saltos de octava
    int voicedRun = 0, unvoicedRun = 0, octaveRun = 0;
    float lastM = 0.0f;

    // MIDI
    bool notesOn[128] {};
    int lastMidiNote = -1;
    long long lastScaleNoteTime = -1000000;

    // Senal seca alineada (para el control Mix)
    juce::AudioBuffer<float> dryDelay;
    int dryPos = 0, latency = 0;

    juce::SmoothedValue<float> outGain, mixSm;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopTuneProcessor)
};
