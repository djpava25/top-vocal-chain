#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "MasterDsp.h"

// TOP MASTER: suite de mastering con 13 modulos, asistente de mastering, medidores de loudness,
// pista de referencia A/B y EQ para igualar el tono de una referencia.
class TopMasterProcessor : public juce::AudioProcessor, private juce::Thread
{
public:
    TopMasterProcessor();
    ~TopMasterProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Top Master"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.2; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    std::unique_ptr<tmaster::Chain> chain;

    // ---- orden de los modulos ----
    std::array<int, tmaster::numReorderable> getOrder() const;
    void setOrder (const std::array<int, tmaster::numReorderable>&);
    void moveModule (int module, int delta);

    // ---- asistente de mastering ----
    static juce::StringArray genreNames();
    static juce::StringArray targetNames();
    static juce::StringArray intensityNames() { return { "Suave", "Medio", "Fuerte" }; }
    int genre = 0, target = 0, intensity = 1;
    bool useReference = false;
    std::atomic<bool> listening { false }, assistantBusy { false };
    std::atomic<float> listenSeconds { 0.0f }, assistantProgress { 0.0f };
    juce::String assistantReport;
    void startListening();
    void stopListeningAndRun();
    bool runAssistantNow (std::function<void (const juce::String&)> onDone = nullptr);   // pruebas: sin hilo
    static float targetLufs (int t) { const float v[] { -14.0f, -11.0f, -9.0f, -7.5f }; return v[juce::jlimit (0, 3, t)]; }

    // ---- referencia ----
    bool loadReference (const juce::File&);
    void clearReference();
    bool hasReference() const { return refLen > 0; }
    juce::String getReferenceName() const { return refName; }
    float getReferenceLufs() const { return refLufs; }
    std::atomic<bool> referenceOn { false };
    std::vector<float> referenceCurve;          // tono de la referencia (31 bandas)
    std::vector<float> lastCurve;               // tono de lo ultimo analizado
    std::atomic<float> matchGainDb { 0.0f };

    // analisis por tercios de octava (relativo)
    static std::vector<float> tonalCurve (const float* L, const float* R, int n, double sr);
    static std::vector<float> genreCurve (int genre);
    static float bandFreq (int b) { return 25.0f * std::pow (2.0f, (float) b / 3.0f); }
    static constexpr int numBands = 31;

private:
    void run() override;
    bool assistantCore (const std::vector<float>& L, const std::vector<float>& R, std::map<juce::String, float>& out, juce::String& report);
    void applyProposal (const std::map<juce::String, float>&);

    double sr = 44100.0;
    juce::AudioFormatManager formats;
    std::array<std::vector<float>, 2> bypassDelay;
    int bpPos = 0;
    float matchSm = 1.0f;

    // captura para el asistente
    std::array<std::vector<float>, 2> capture;
    std::atomic<int> capLen { 0 };
    std::atomic<bool> wantAssistant { false };

    // referencia
    std::array<std::vector<float>, 2> ref;
    std::atomic<int> refLen { 0 };
    double refPos = 0.0;
    float refLufs = -100.0f;
    juce::String refName;
    juce::File refFile;
    juce::SpinLock refLock;

    JUCE_DECLARE_WEAK_REFERENCEABLE (TopMasterProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopMasterProcessor)
};
