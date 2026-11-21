#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "RackModules.h"

// TOP RACK: rack de 8 ranuras con 15 modulos (EQs y compresores de estilo clasico, color y utilidades),
// trim por ranura, sidechain externo, 4 macros, 8 Dream Strips, presets de cadena y oversampling 2x.
class TopRackProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    TopRackProcessor();
    ~TopRackProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Top Rack"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }
    int getNumPrograms() override;
    int getCurrentProgram() override { return program; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;
    static juce::String sid (int slot, const juce::String& what) { return "s" + juce::String (slot + 1) + "_" + what; }

    // ---- ranuras ----
    int slotType (int slot) const;
    void setSlotType (int slot, int type);        // pone las perillas en sus valores por defecto
    void moveSlot (int slot, int delta);
    float slotGr (int slot) const;
    float knobNorm (int slot, int knob) const;
    void setKnobReal (int slot, int knob, float real);

    // ---- macros ----
    struct Assign { int macro = 0, slot = 0, knob = 0; float lo = 0, hi = 1; };
    std::vector<Assign> assigns;                   // solo hilo de mensajes
    void assignMacro (int macro, int slot, int knob);
    void unassign (int slot, int knob);
    int macroOf (int slot, int knob) const;
    juce::StringArray macroNames { "Macro 1", "Macro 2", "Macro 3", "Macro 4" };

    // ---- dream strips ----
    std::array<juce::ValueTree, 8> dream;
    juce::ValueTree snapshot() const;
    void recall (const juce::ValueTree&);
    void storeDream (int i) { dream[(size_t) i] = snapshot(); }
    bool recallDream (int i) { if (! dream[(size_t) i].isValid()) return false; recall (dream[(size_t) i]); return true; }

    std::atomic<float> inPeak { 0.0f }, outPeak { 0.0f };
    void applyMacros();

private:
    void timerCallback() override;
    struct Slot
    {
        std::array<std::unique_ptr<trk::Module>, trk::NumTypes> mods;
        int cur = 0;
        float trimSm = 1.0f;
    };
    struct Rack
    {
        std::array<Slot, trk::numSlots> slots;
        void prepare (double sr) { for (auto& s : slots) { for (int t = 1; t < trk::NumTypes; ++t) { if (s.mods[(size_t) t] == nullptr) s.mods[(size_t) t] = trk::create (t); s.mods[(size_t) t]->prepare (sr); } s.cur = 0; } }
    };
    void runRack (Rack&, float* L, float* R, int n, const float* scL, const float* scR);
    void setP (const juce::String& id, float v);

    double sr = 44100.0;
    Rack rack1, rack2;
    std::unique_ptr<juce::dsp::Oversampling<float>> os, osSc;
    bool osActive = false;
    int maxBlock = 512;
    std::array<std::vector<float>, 2> dry, scBuf;
    std::vector<float> monoR;
    std::array<std::vector<float>, 2> dryDelay;
    int ddPos = 0, osLatency = 0;
    float inSm = 1, outSm = 1, mixSm = 1;
    int program = 0;
    // valores de perillas por ranura (para el hilo de audio)
    std::array<std::atomic<float>*, trk::numSlots> typeP {}, bypP {}, trimP {}, scP {};
    std::array<std::array<std::atomic<float>*, trk::maxKnobs>, trk::numSlots> knobP {};
    std::array<std::atomic<float>*, 4> macroP {};
    std::array<float, 4> lastMacro { -1, -1, -1, -1 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopRackProcessor)
};
