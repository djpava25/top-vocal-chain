#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "NoteEngine.h"

// TOP NOTE EDITOR: editor de notas por "blobs".
//  1) TRANSFERIR: graba la pista mientras el DAW reproduce (queda anclada a la linea de tiempo)
//     o IMPORTAR un archivo de audio.
//  2) Analiza y divide en notas (algoritmo melodico o percusivo).
//  3) Editas tono, tiempo, duracion, vibrato, deriva, formantes y volumen de cada nota.
//  4) Al reproducir el DAW, el plugin suena con tus ediciones en su sitio. Tambien exporta WAV.
class TopNoteEditorProcessor : public juce::AudioProcessor, private juce::Thread, private juce::AsyncUpdater
{
public:
    TopNoteEditorProcessor();
    ~TopNoteEditorProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Top Note Editor"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
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

    enum Status { Empty = 0, Recording, Analysing, Ready };
    std::atomic<int> status { Empty };
    std::atomic<float> progress { 0.0f }, renderProgress { 1.0f };
    std::atomic<bool> rendering { false };

    // ---- fuente ----
    void setTransfer (bool on);
    bool isTransferring() const { return recArmed.load(); }
    double recordedSeconds() const { return recMax.load() / juce::jmax (1.0, sr); }
    bool importFile (const juce::File&);
    void loadBuffer (std::vector<float> mono, double rate, juce::int64 originSample, const juce::String& name);
    void reanalyse (int algorithm);
    int getAlgorithm() const { return algorithm; }
    bool hasSource() const { return source != nullptr && ! source->empty(); }
    int sourceLength() const { return source != nullptr ? (int) source->size() : 0; }
    std::shared_ptr<const std::vector<float>> getSource() const { return source; }
    std::shared_ptr<const ned::Curve> getCurve() const { return curve; }
    juce::int64 getOrigin() const { return origin; }
    double getRate() const { return sr; }
    juce::String getSourceName() const { return sourceName; }
    void applyPendingAnalysis();          // hilo de mensajes (lo llama el AsyncUpdater)
    bool waitUntilIdle (int timeoutMs);   // para pruebas

    // ---- notas (solo hilo de mensajes) ----
    std::vector<ned::Note> notes;
    std::atomic<int> modelVersion { 0 };
    void beginEdit();
    void notesChanged();
    void undo();
    void redo();
    bool canUndo() const { return ! undoStack.empty(); }
    bool canRedo() const { return ! redoStack.empty(); }

    // ---- macros ----
    struct Selection { std::vector<int> idx; };
    std::vector<int> targetsOf (const std::vector<int>& selected) const;   // seleccion o todas
    void macroCorrectPitch (const std::vector<int>& sel, float centerAmt, float driftAmt);
    void macroQuantizeTime (const std::vector<int>& sel, double gridBeats, float amount);
    void macroLevel (const std::vector<int>& sel, float louder, float softer);
    int snapPitch (float midi) const;   // nota de la escala (o semitono) mas cercana

    // ---- reproduccion ----
    std::atomic<double> playPos { -1.0 };     // en muestras relativas a la fuente
    std::atomic<bool> hostPlaying { false };
    std::atomic<bool> previewing { false };
    void startPreview (double fromSample);
    void stopPreview() { previewing.store (false); }
    std::atomic<double> bpm { 120.0 };
    std::atomic<int> sigNum { 4 };

    // ---- exportar ----
    juce::File exportWav();
    juce::File lastExport;
    std::vector<float> renderNow();       // render sincronico (pruebas / exportar)

private:
    void run() override;
    void handleAsyncUpdate() override { applyPendingAnalysis(); }
    void writeSourceFile();

    double sr = 44100.0;
    juce::AudioFormatManager formats;

    std::shared_ptr<const std::vector<float>> source;
    std::shared_ptr<const ned::Curve> curve;
    juce::int64 origin = 0;
    juce::String sourceName { "Toma" };
    juce::File sourceFile;
    int algorithm = ned::Melodic;

    // grabacion
    std::vector<float> rec;
    std::atomic<bool> recArmed { false }, recHasOrigin { false };
    std::atomic<int> recMax { 0 };
    juce::int64 recOrigin = 0;

    // trabajo en segundo plano
    juce::CriticalSection jobLock;
    std::atomic<bool> wantAnalyse { false }, wantRender { false }, cancelJob { false };
    int jobAlgorithm = 0;
    bool keepNotesOnAnalyse = false;
    std::vector<ned::Note> renderNotes;        // copia para el hilo de render (bajo jobLock)
    std::shared_ptr<const ned::Curve> pendingCurve;
    std::vector<ned::Note> pendingNotes;
    bool pendingReady = false;
    ned::Renderer renderer;

    std::shared_ptr<const std::vector<float>> rendered;
    juce::SpinLock outLock;
    double previewPos = 0.0;

    std::vector<std::vector<ned::Note>> undoStack, redoStack;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopNoteEditorProcessor)
};
