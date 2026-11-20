#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "ChoirEngine.h"

// TOP CHOIR: carga una voz, crea un coro (unisono, octavas, terceras y quintas en la tonalidad)
// y lo guarda como WAV para arrastrarlo al DAW. Tambien se puede escuchar desde el plugin.
class TopChoirProcessor : public juce::AudioProcessor
{
public:
    TopChoirProcessor();
    ~TopChoirProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Top Choir"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // archivo de voz
    bool loadVoice (const juce::File&);
    void clearAll();
    bool hasVoice() const { return voiceLoaded.load(); }
    juce::File getVoiceFile() const { return voiceFile; }
    const juce::AudioBuffer<float>& getVoice() const { return voice; }
    double getVoiceRate() const { return voiceRate; }
    bool autoDetectKey();

    // generar el coro
    void createVocals();
    bool isRendering() const { return rendering.load(); }
    float renderProgress() const { return progress.load(); }
    bool hasChoir() const { return choirReady.load(); }
    juce::File getChoirFile() const { const juce::ScopedLock sl (lock); return choirFile; }
    const juce::AudioBuffer<float>& getChoir() const { return choirBuf; }
    static juce::File outputFolder();

    // escuchar
    std::atomic<bool> playing { false }, mixLead { false };
    std::atomic<double> playPos { 0.0 };   // en muestras del archivo

    choir::Settings currentSettings() const;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    float p (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    juce::AudioFormatManager formats;
    juce::AudioBuffer<float> voice, choirBuf;
    double voiceRate = 44100.0, hostRate = 44100.0;
    juce::File voiceFile, choirFile;
    std::atomic<bool> voiceLoaded { false }, choirReady { false }, rendering { false }, cancel { false };
    std::atomic<float> progress { 0.0f };
    juce::CriticalSection lock;
    juce::ThreadPool pool { 1 };
    juce::LagrangeInterpolator interp[2];
    std::vector<float> tmp;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopChoirProcessor)
};
