#pragma once
#include <array>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "DSP/Biquad.h"
#include "DSP/PitchCorrector.h"
#include "DSP/VoiceAI.h"
#include "DSP/Effects.h"
#include "DSP/Psola.h"
#include "DSP/VoiceAnalysis.h"
#include "Presets.h"

//==============================================================================
// Copia del audio para los visualizadores (espectro del EQ e imagen estereo).
// El hilo de audio escribe; la interfaz lee las ultimas muestras sin bloquear.
struct AudioTap
{
    static constexpr int size = 8192;
    std::array<float, size> l {}, r {};
    std::atomic<int> write { 0 };
    void push (const float* L, const float* R, int n) noexcept
    {
        int w = write.load (std::memory_order_relaxed);
        for (int i = 0; i < n; ++i)
        {
            l[(size_t) w] = L[i];
            r[(size_t) w] = R != nullptr ? R[i] : L[i];
            w = (w + 1) & (size - 1);
        }
        write.store (w, std::memory_order_release);
    }
    // Copia las ultimas n muestras (la mas nueva al final)
    void read (float* outL, float* outR, int n) const noexcept
    {
        const int w = write.load (std::memory_order_acquire);
        for (int i = 0; i < n; ++i)
        {
            const auto idx = (size_t) ((w - n + i + size) & (size - 1));
            if (outL) outL[i] = l[idx];
            if (outR) outR[i] = r[idx];
        }
    }
};

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
    std::atomic<bool> preHot { false }, eqHot { false }, outClip { false };   // modulos saturando
    std::atomic<float> preSat { 0.0f };    // cantidad de saturacion del preamp (0 limpio .. 1 fuerte)
    std::atomic<float> eqDynDb { 0.0f };   // reduccion actual del EQ dinamico (dB, <= 0)
    std::atomic<float> airLevel { 0.0f }, airSmoothDb { 0.0f };   // VOCAL AIR: energia de agudos y suavizado actual
    std::atomic<float> matchGainDb { 0 };   // compensacion de volumen al comparar
    std::atomic<float> sungMidi { 0 };      // nota cantada (MIDI con decimales)
    std::atomic<bool> voiced { false };
    std::atomic<float> compInDb { -100 }, compOutDb { -100 };
    std::atomic<float> keyHist[12] {};      // histograma para detectar la tonalidad
    std::atomic<bool> keyListening { false };
    // Stereo Lab y medidores profesionales
    std::atomic<float> labCorr { 1 }, outCorr { 1 };
        std::atomic<float> phaseGuard { 1.0f };   // SAFE VOCAL: 1 = ancho completo, <1 = reducido para salvar la fase
    std::atomic<float> lufsM { -100 }, lufsI { -100 }, truePeakDb { -100 }, rmsDb { -100 }, peakHoldDb { -100 };
    std::atomic<bool> resetMeters { false };
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
    bool acceptsMidi() const override  { return true; }   // MIDI Learn (CC -> parametros)
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

    juce::UndoManager undoManager;           // deshacer / rehacer
    juce::AudioProcessorValueTreeState apvts;
    LiveMeters meters;
    static const juce::StringArray& qualityNames();
    // MIDI Learn: cada CC (0..127) puede controlar un parametro (indice en getParameters(), -1 = libre)
    std::array<std::atomic<int>, 128> midiMap;
    std::atomic<int> midiLearnParam { -1 }, lastLearnedCc { -1 };
    void startMidiLearn (const juce::String& paramId);
    void clearMidi (const juce::String& paramId);
    int ccForParam (const juce::String& paramId) const;
    std::atomic<int> uiTheme { 0 };      // 0 Hardware (claro), 1 Oscuro
    juce::ValueTree exportConfig();
    bool importConfig (const juce::ValueTree&);
    double getDelayMs() const;
    AudioTap tapPre, tapPost;   // antes y despues de la cadena

    // A/B: dos ranuras de ajustes para comparar cadenas
    void switchToSlot (int slot);
    void copyToOtherSlot();
    int getCurrentSlot() const { return currentSlot; }
    void resetAll();

    std::atomic<bool> compareActive { false };   // escuchar el original con el volumen igualado
    std::atomic<int> hiddenModules { 0 };       // bits: modulos ocultos en la interfaz
    std::atomic<int> uiScaleIndex { 0 };        // 0=100%, 1=125%, 2=150%, 3=200%

    static const juce::StringArray& keyNames();
    static const juce::StringArray& scaleNames();
    static const juce::StringArray& divisionNames();
    static const juce::StringArray& delayTypeNames();
    static const juce::StringArray& eqMsNames();
    static const juce::StringArray& preTypeNames();
    static const juce::StringArray& compTypeNames();
    static const juce::StringArray& eqTypeNames();
    static const juce::StringArray& wideModeNames();
    static const juce::StringArray& slopeNames();
    static const juce::StringArray& tuneModeNames();
    void startKeyDetection();

    // IA: ANALIZAR VOZ -> APLICAR RECOMENDACIONES
    tvc::VoiceAnalysis analysis;
    void startAnalysis() { analysis.start(); }
    void applyReport (const tvc::VoiceAnalysis::Report&);
    static const juce::StringArray& reverbTypeNames();

    // Curva del compresor (para dibujarla): reduccion en dB para un nivel de entrada
    float getCompStaticGr (float levelDb) const { return compView.staticGr (levelDb); }
    // Frecuencias fijas de LOW / PRESENCE / AIR segun el modelo de EQ (para la grafica)
    static void eqModelFreqs (int type, float airGain, float& lowF, float& presF, float& airF);

    // Respuesta del EQ actual (para dibujar la curva). Lectura aproximada desde la interfaz.
    float getEqMagnitudeDb (double freq) const;
    float getAirMagnitudeDb (double freq) const;   // curva del modulo VOCAL AIR

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float p (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    bool  on (const char* id) const { return p (id) >= 0.5f; }
    void updateEq (float aiAmt);

    double sr = 44100.0;
    int maxBlock = 512;
    int currentProgram = 0;
    std::atomic<bool> aiResetRequested { false };

    // Modulos
    tvc::PitchDetector detector;
    tvc::PitchShifter shifter[2];          // motor de baja latencia
    toptune::Psola psola;                  // motor natural (conserva formantes, permite Formant)
    bool lowLatActive = false;
    float formantSm = 1.0f, transposeSm = 0.0f, smoothMidi = 0.0f, holdTime = 0.0f;
    double vibPhase = 0.0, period = 0.0;
    float tuneCoef = 0.0f;
    void updateLatency();
    float corrSemis = 0.0f, corrTarget = 0.0f;
    int heldNote = -1;
    int samplesSinceEstimate = 0;
    bool delayWasOn = true;

    tvc::VoiceAI ai;
    juce::HeapBlock<float> monoBuffer;

    juce::dsp::Oversampling<float> oversampling { 2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false };
    juce::dsp::Oversampling<float> oversampling4 { 2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false };
    tvc::PreampStage preampEco, preamp4;
    int qualityActive = 1;               // 0 Eco (1x), 1 Normal (2x), 2 High (4x)
    int osLatency() const;
    tvc::PreampStage preamp;

    enum EqBand { bLowCut, bLowCut2, bLow, bLowDip, bMid, bPres, bAir, bHighCut, bHighCut2, bAiMud, bAiDeEss, numBands };
    enum FilterKind { fIdentity, fHP2, fHP1, fLowShelf, fHighShelf, fPeak, fLP2, fLP1 };
    struct BandSpec { int kind = fIdentity; float f = 1000.0f, q = 0.707f, g = 0.0f; };
    tvc::Biquad eq[numBands][2];
    tvc::Biquad soloBp[2], dynDet;         // SOLO de la banda MID y detector del EQ dinamico
    float dynEnv = 0.0f, dynGainDb = 0.0f;
    // VOCAL AIR: banda media-alta (presencia aireada) + estante de agudos + excitador, con suavizado dinamico
    tvc::Biquad airMid[2], airHigh[2], airSib;
    tvc::Biquad airSrcHp[2], airSrcLp[2], airHarmHp[2], airHarmHp2[2];   // generador de armonicos (fuente 2.5-7 kHz)
    float airEnvSib = 0.0f, airEnvAll = 0.0f, airRed = 0.0f, airGm = 0.0f, airGh = 0.0f, airSrcEnv[2] {};
    juce::Random analogNoise { 1234 };
    BandSpec eqCache[numBands];
    bool eqCacheValid = false;

    tvc::VocalCompressor comp, compView;
    tvc::Biquad scHp[2];
    float scHpFreq = -1.0f;
    juce::AudioBuffer<float> scRing, scBuf;
    int scPos = 0;
    tvc::StereoDelay delay;
    tvc::Widener widener;
    double wideMs = 0.0, wideSs = 0.0;

    tvc::FdnReverb fdn;
    tvc::LoudnessMeter loud;
    tvc::Biquad labSideHp[2];
    float phaseGuard = 1.0f;
    juce::AudioBuffer<float> labDry;
    double labLL = 1.0, labRR = 1.0, labLR = 1.0, outLL = 1.0, outRR = 1.0, outLR = 1.0;

    juce::SmoothedValue<float> inGainSm, outGainSm;

    // Bypass sin clicks (fundidos de 20 ms por modulo) y seccion global
    juce::SmoothedValue<float> airMixSm, preMixSm, eqMixSm, compMixSm, wideMixSm, dlyMixSm, revMixSm, procSm, globalMixSm;
    juce::AudioBuffer<float> scratch, dryAligned, dryRing;
    int dryPos = 0, dryLatency = 0;
    double rmsProc = 1.0e-6, rmsDry = 1.0e-6;
    float limGain = 1.0f, limRelCoef = 0.001f;
    bool revActive = false;
    juce::ValueTree slots[2];
    int currentSlot = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopVocalChainProcessor)
};
