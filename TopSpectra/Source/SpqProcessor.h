#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "SpqFilters.h"
#include "SpqFir.h"

// TOP SPECTRA EQ (AnalogHM): EQ parametrico de 24 bandas con dinamica por banda, dinamica ESPECTRAL por banda,
// modos de fase Zero Latency / Linear Phase, modos de caracter (Clean / Subtle / Warm) y analizador.
class SpqProcessor : public juce::AudioProcessor
{
public:
    static constexpr int numBands = 24, dispPts = 256;
    enum DynMode { DynOff, DynBand, DynSpectral };
    enum PhaseMode { ZeroLatency, LinearLow, LinearHigh };

    SpqProcessor();
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Top Spectra EQ"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static juce::String pid (int band, const char* what) { return "b" + juce::String (band) + "_" + what; }
    spq::Band bandParams (int i) const;                   // lectura de parametros (UI y audio)
    int dynModeOf (int i) const;
    double fsNow() const { return sr; }

    juce::UndoManager undo;
    juce::AudioProcessorValueTreeState apvts;

    // ---- datos para la pantalla (sin bloqueos)
    std::array<std::atomic<float>, numBands> dynNow {};            // ganancia dinamica actual de cada banda (dB)
    std::array<std::atomic<float>, dispPts> specNow {};            // reduccion espectral total en 256 puntos log (dB)
    std::atomic<int> solo { -1 };                                  // banda en escucha (solo), -1 = ninguna
    static constexpr int anaSize = 32768;
    std::vector<float> anaPre, anaPost; std::atomic<int> anaW { 0 };
    std::atomic<float> autoGainDb { 0.0f };

private:
    struct BandRT
    {
        spq::Band p, tgt; int n = 0; bool valid = false;
        spq::ASec as[spq::maxSec];
        spq::Sec sec[2][spq::maxSec];
        spq::Biquad dig[spq::maxSec];
        double gApplied = 1e9, dyn = 0, dynT = 0, env = 0;          // ganancia con la que estan disenadas las secciones
        spq::Sec det;
        double curveG = 1e9; bool curveValid = false;
        std::vector<double> curve, spGain;                          // curva FIR (K/2+1) / dinamica espectral (dB por bin)
    };
    void configure (int mode, bool spectralNeeded);
    void designIIR (BandRT&, double g, bool warm);
    void buildMasks();
    void bandWeight (const spq::Band&, std::vector<double>& w) const;
    double sr = 48000; int blockMax = 512, hopBase = 512, curMode = -1; bool curSpec = false, firOn = false;
    std::array<BandRT, numBands> bands;
    spq::FirStage fir;
    spq::SpecAnalyzer spec;
    int specHopCounter = 0;
    std::vector<double> gSt, gL, gR, gM, gS, mP, mQ, mU, mV, envBuf, wBuf, prefix;
    std::vector<float> delayL, delayR; int delayW = 0, latencyNow = 0;
    spq::Sec soloS[2];
    double outLin = 1, outLinT = 1, charLp[2] { 0, 0 }, chX1[2] { 0, 0 }, dcX[2] { 0, 0 }, dcY[2] { 0, 0 };
    int currentProgram = 0;
    bool masksDirty = true;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpqProcessor)
};
