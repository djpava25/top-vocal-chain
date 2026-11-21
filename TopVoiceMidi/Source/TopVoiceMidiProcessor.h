#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include "VoiceMidiDsp.h"

// TOP VOICE MIDI: canta, tararea o haz beatbox y el plugin lo convierte en MIDI.
//  - Melodia: tono -> notas MIDI con velocidad segun tu volumen
//  - Ritmo: bombo / caja / hi-hat de beatbox entrenables -> notas de bateria
//  - Armonia: tonalidad automatica, escalas, escala aprendida con tu voz y acordes
//  - Expresion: pitch bend (inteligente o exacto) y 4 CC (envolvente, brillo, afinacion, vocal)
//  - Captura: graba el MIDI dentro del plugin, limpia notas fantasma y exporta un .mid
class TopVoiceMidiProcessor : public juce::AudioProcessor
{
public:
    TopVoiceMidiProcessor();
    ~TopVoiceMidiProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Top Voice MIDI"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    static juce::StringArray scaleNames();
    static juce::StringArray chordNames();
    static juce::StringArray ccSourceNames();
    static juce::StringArray padNames() { return { "Bombo", "Caja", "Hi-Hat", "Extra" }; }

    // ---- estado en vivo para la interfaz ----
    std::atomic<float> inDb { -100.0f }, liveMidi { 0.0f }, bendSemis { 0.0f };
    std::atomic<int> liveNote { -1 }, lastPad { -1 }, hitSerial { 0 }, noteSerial { 0 };
    std::array<std::atomic<float>, 4> ccLive {};
    std::atomic<int> detKey { 0 };
    std::atomic<bool> detMinor { false };
    std::atomic<float> detConf { 0.0f };
    // historial de afinacion (para la grafica): escritor unico (audio), lector la UI
    static constexpr int histSize = 400;
    std::array<std::atomic<float>, histSize> pitchHist {}, noteHist {};
    std::atomic<int> histPos { 0 };

    // ---- entrenamiento del beatbox ----
    std::atomic<int> trainingPad { -1 };
    int padExamples (int pad) const;
    void clearPad (int pad);
    // ---- escala aprendida con la voz ----
    std::atomic<bool> learning { false };
    std::atomic<int> learnedMask { 0 };
    // tonalidad efectiva (auto o manual) y mascara
    int effectiveKey() const;
    int effectiveScale() const;   // indice en scaleNames()

    // ---- captura MIDI ----
    void startCapture();
    void stopCapture();
    void clearCapture();
    bool isCapturing() const { return capturing.load(); }
    int capturedNotes();                         // hilo de mensajes
    bool exportCapture (const juce::File&, bool cleanGhosts);
    juce::File defaultExportFile() const;

    // ---- salida MIDI externa (loopMIDI, interfaces, etc.) ----
    juce::StringArray externalDevices (juce::Array<juce::MidiDeviceInfo>& infos) const;
    void setExternalDevice (const juce::String& identifier);
    juce::String getExternalDevice() const { return extId; }

    // latencia aproximada de deteccion (ms)
    float detectionLatencyMs() const;

    // parametros leidos una vez por bloque
    struct Cfg
    {
        int mode = 0, octave = 0, channel = 1, key = 0, scale = 0, chordType = 0, voicing = 0, inversion = 0, bendMode = 0, trigCh = 10, wave = 1;
        float inGain = 1, gateDb = -48, dry = 0, synthVol = 0.7f, responseMs = 20, stability = 0.35f, velSens = 0.7f, bendRange = 2, bendThr = 0.4f, trigSens = 0.5f;
        bool synthOn = true, autoKey = false, chordOn = false;
        std::array<int, 12> mask {}, chordMask {};
        std::array<bool, 4> ccOn {}; std::array<int, 4> ccSrc {}, ccNum {}; std::array<float, 4> ccSmooth {};
        std::array<int, 4> trigNote {};
    };

private:
    struct Ev { double t; juce::uint8 b[3]; };
    Cfg cfg;
    int histDec = 0;
    void emit (const juce::MidiMessage&, int sample, juce::MidiBuffer&);
    void melodyFrame (int sample, juce::MidiBuffer&);
    void notesOn (int root, int vel, juce::MidiBuffer&, int sample);
    void notesOff (juce::MidiBuffer&, int sample);
    int buildChord (int root, std::array<int, 8>& out) const;
    void updateKey();
    void drainCapture();

    double sr = 44100.0;
    vmidi::FastPitch pitch;
    vmidi::HitAnalyser hits;
    vmidi::HitClassifier classifier;
    juce::SpinLock classifierLock;
    vmidi::MonitorSynth synth;
    tvc::Biquad bandHi, bandLo, bandMid;

    // estado de la melodia
    float env = 0, envDb = -100, lastAper = 1.0f;
    float med[3] { 0, 0, 0 };
    int medPos = 0, curRoot = -1, cand = -1, candSamples = 0, unvoicedSamples = 0;
    float candPeakDb = -100;
    std::array<int, 8> sounding {};
    int numSounding = 0, lastBend = 8192;
    float bendSm = 0;
    float eHi = 0, eAll = 0, eLo = 0, eMid = 0;
    std::array<float, 4> ccSm {};
    std::array<int, 4> ccSent { -1, -1, -1, -1 };
    float hist[12] {};
    int keyTimer = 0;
    // ritmo
    float fastEnv = 0, slowEnv = 0;
    int refractory = 0;
    struct PendingOff { int note = -1, ch = 10, samplesLeft = 0; };
    std::array<PendingOff, 16> offs {};

    // captura
    std::atomic<bool> capturing { false };
    double capTime = 0;
    juce::AbstractFifo capFifo { 16384 };
    std::vector<Ev> capBuf;
    juce::MidiMessageSequence captured;          // hilo de mensajes
    juce::CriticalSection captureLock;

    // salida externa
    juce::AbstractFifo extFifo { 4096 };
    std::vector<Ev> extBuf;
    juce::String extId;
    std::unique_ptr<juce::MidiOutput> extOut;
    juce::CriticalSection extLock;
    struct Sender : juce::Thread
    {
        explicit Sender (TopVoiceMidiProcessor& p) : juce::Thread ("TopVoiceMidi MIDI out"), owner (p) {}
        void run() override;
        TopVoiceMidiProcessor& owner;
    } sender { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopVoiceMidiProcessor)
};
