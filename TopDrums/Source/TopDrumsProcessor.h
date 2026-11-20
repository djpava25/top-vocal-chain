#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "DSP/Biquad.h"

// TOP DRUMS: transient shaper + color (peso, saturacion, tono) + glue (compresor de bus) para baterias.
// IA: analisis local de la bateria con recomendaciones por genero, y consulta opcional a Claude (API de Anthropic).
class TopDrumsProcessor : public juce::AudioProcessor
{
public:
    TopDrumsProcessor();
    ~TopDrumsProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Top Drums"; }
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

    static const juce::StringArray& focusNames();
    static const juce::StringArray& ratioNames();
    static const juce::StringArray& attackNames();
    static const juce::StringArray& releaseNames();
    static const juce::StringArray& scNames();

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> inLevelDb { -100.0f }, outLevelDb { -100.0f }, grDb { 0.0f }, transientDb { 0.0f };

    //==========================================================================
    // IA LOCAL: escucha ~8 s de bateria y compara con el perfil del genero elegido
    struct Analysis
    {
        bool valid = false;
        float crestDb = 0, lowDb = 0, highDb = 0, punchDb = 0, densityPerSec = 0, rmsDb = -100, dynRangeDb = 0;
        juce::StringArray findings, recommendations;
        std::vector<std::pair<juce::String, float>> settings;   // ajustes sugeridos (id -> valor)
    };
    void startAnalysis();
    bool isAnalysing() const { return analysing.load(); }
    float analysisProgress() const;
    bool analysisReady() const { return analysisDone.load(); }
    Analysis getAnalysis();
    static juce::String genreForProgram (int program);
    int getGenre() const { return (int) apvts.getRawParameterValue ("genre")->load(); }
    static const juce::StringArray& genreNames();

    // Aplica una lista de ajustes (de la IA local o de Claude), con limites seguros
    void applySettings (const std::vector<std::pair<juce::String, float>>&);

    //==========================================================================
    // CLAUDE ONLINE (opcional): necesita una clave de API de Anthropic (se guarda solo en este ordenador)
    enum class AskState { idle, running, done, error };
    void askClaude();
    AskState getAskState() const { return askState.load(); }
    juce::String getClaudeText();
    std::vector<std::pair<juce::String, float>> getClaudeSettings();
    static juce::String loadApiKey();
    static void saveApiKey (const juce::String&);
    juce::String buildClaudeRequest (const Analysis&);           // JSON del pedido (publico para las pruebas)
    static bool parseClaudeReply (const juce::String& body, juce::String& text, std::vector<std::pair<juce::String, float>>& settings);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float p (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    bool on (const char* id) const { return p (id) > 0.5f; }
    void updateFilters();
    void finishAnalysis();

    int currentProgram = 0;
    double sr = 44100.0;
    int maxBlock = 512;

    // Transient shaper (detector enlazado L/R)
    tvc::Biquad focusA, focusB;
    int lastFocus = -1;
    float eFA = 0, eSA = 0, eFR = 0, eSR = 0, tsGainDb = 0;

    // Color
    tvc::Biquad weightLp[2], tiltLo[2], tiltHi[2];
    juce::dsp::Oversampling<float> oversampling { 2, 1, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true };   // fase lineal: MIX en paralelo sin cancelaciones

    // Glue
    tvc::Biquad scHp[2];
    int lastSc = -1;
    float glueGr = 0.0f, glueSlow = 0.0f;

    juce::SmoothedValue<float> outSm, mixSm, powerSm, glueMixSm;
    std::vector<float> dryRing[2];
    int dryPos = 0, dryLatency = 0;
    juce::AudioBuffer<float> dryBuf;
    float inPeak = 0, outPeak = 0;

    // Analisis
    std::atomic<bool> analysing { false }, analysisDone { false };
    std::atomic<int> anSamples { 0 };
    int anTarget = 0;
    double anSumSq = 0, anLowSq = 0, anHighSq = 0, anPeak = 0, anPunchSum = 0;
    int anOnsets = 0, anRefractory = 0;
    std::vector<float> anShortRms;
    double anBlockSq = 0; int anBlockN = 0;
    tvc::Biquad anLow, anHigh;
    juce::CriticalSection anLock;
    Analysis lastAnalysis;

    // Claude
    std::atomic<AskState> askState { AskState::idle };
    juce::CriticalSection claudeLock;
    juce::String claudeText;
    std::vector<std::pair<juce::String, float>> claudeSettings;
    juce::ThreadPool pool { 1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopDrumsProcessor)
};
