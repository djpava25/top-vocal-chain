#pragma once
#include <juce_dsp/juce_dsp.h>
#include "Biquad.h"
#include "Psola.h"   // KeyFinder

namespace tvc
{
//==============================================================================
// "ANALIZAR VOZ": escucha ~8 segundos de voz y genera un informe con recomendaciones.
// El hilo de audio solo acumula datos; el informe se calcula en el hilo de la interfaz
// cuando el analisis termina (ready == true).
class VoiceAnalysis
{
public:
    static constexpr int order = 11, fftSize = 1 << order, hop = fftSize / 2, numBinsDb = 220;
    static constexpr double targetVoicedSeconds = 8.0, maxSeconds = 25.0;

    struct Report
    {
        bool valid = false;
        float f0 = 0.0f;
        juce::String registro;
        int keyRoot = 0; bool minor = false; float keyConf = 0.0f;
        float rmsDb = -100.0f, dynRange = 0.0f;
        float resonanceHz = 0.0f, resonanceDb = 0.0f;
        float sibRel = -60.0f, sibHz = 7000.0f; juce::String sibLabel;
        float noiseDb = -100.0f; juce::String noiseLabel;
        float presenceRel = -60.0f; juce::String presenceLabel;
        float lowRel = -60.0f; juce::String lowLabel;
        juce::String dynLabel;
        juce::StringArray recommendations;
        // Valores a aplicar
        float inGain = 0.0f, lowCut = 80.0f, midFreq = 1000.0f, midGain = 0.0f, midQ = 1.0f,
              presGain = 2.0f, compThr = -18.0f, compRatio = 4.0f;
        bool cutResonance = false;
    };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        fft = std::make_unique<juce::dsp::FFT> (order);
        fifo.assign ((size_t) fftSize, 0.0f);
        data.assign ((size_t) fftSize * 2, 0.0f);
        window.resize ((size_t) fftSize);
        for (int i = 0; i < fftSize; ++i)
            window[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi * (float) i / (float) (fftSize - 1));
        spec.assign ((size_t) fftSize / 2 + 1, 0.0);
    }

    void start()
    {
        active.store (false);
        std::fill (spec.begin(), spec.end(), 0.0);
        std::fill (fifo.begin(), fifo.end(), 0.0f);
        std::fill (std::begin (histAll), std::end (histAll), 0);
        std::fill (std::begin (histVoiced), std::end (histVoiced), 0);
        std::fill (std::begin (keyHist), std::end (keyHist), 0.0f);
        specFrames = 0; fifoPos = 0; hopCount = 0;
        voicedSec = totalSec = 0.0; logF0Sum = 0.0; logF0Count = 0;
        ready.store (false);
        active.store (true);
    }

    void cancel() { active.store (false); }
    bool isActive() const { return active.load(); }
    bool isReady() const { return ready.load(); }
    float progress() const { return (float) juce::jlimit (0.0, 1.0, voicedSec / targetVoicedSeconds); }

    // Hilo de audio: senal mono original (antes de cualquier ganancia)
    void push (const float* x, int n) noexcept
    {
        if (! active.load()) return;
        for (int i = 0; i < n; ++i)
        {
            fifo[(size_t) fifoPos] = x[i];
            fifoPos = (fifoPos + 1) % fftSize;
            if (++hopCount >= hop) { hopCount = 0; analyseFrame(); }
        }
        totalSec += n / sr;
        if (voicedSec >= targetVoicedSeconds || totalSec >= maxSeconds)
        {
            active.store (false);
            ready.store (true);
        }
    }

    // Hilo de audio: cada estimacion de tono con voz
    void addPitch (float midi, float hz, double seconds, float confidence) noexcept
    {
        if (! active.load()) return;
        voicedSec += seconds;
        logF0Sum += std::log2 ((double) hz); ++logF0Count;
        const int pc = ((int) std::round (midi) % 12 + 12) % 12;
        keyHist[pc] += (float) seconds * confidence;
    }

    // Hilo de la interfaz: arma el informe cuando ready == true
    Report computeReport() const
    {
        Report r;
        if (voicedSec < 2.5 || specFrames < 8) return r;
        r.valid = true;
        auto U = [] (const char* s) { return juce::String::fromUTF8 (s); };

        // Tono y registro
        r.f0 = logF0Count > 0 ? (float) std::exp2 (logF0Sum / logF0Count) : 0.0f;
        struct Reg { float maxHz; const char* name; float lowCut; };
        static const Reg regs[] { { 140, "Bajo", 70 }, { 185, "Barítono", 90 }, { 250, "Tenor", 110 },
                                  { 310, "Contralto", 130 }, { 370, "Mezzosoprano", 150 }, { 99999, "Soprano", 170 } };
        for (const auto& g : regs) if (r.f0 < g.maxHz) { r.registro = U (g.name); r.lowCut = g.lowCut; break; }

        // Tonalidad
        toptune::KeyFinder::find (keyHist, r.keyRoot, r.minor, r.keyConf);

        // Nivel y dinamica (cuadros con voz)
        auto percentile = [] (const int* h, double p)
        {
            long total = 0; for (int b = 0; b < numBinsDb; ++b) total += h[b];
            if (total == 0) return -100.0f;
            long acc = 0;
            for (int b = 0; b < numBinsDb; ++b) { acc += h[b]; if ((double) acc >= p * (double) total) return -110.0f + 0.5f * (float) b; }
            return 0.0f;
        };
        r.rmsDb = percentile (histVoiced, 0.5);
        r.dynRange = percentile (histVoiced, 0.95) - percentile (histVoiced, 0.10);
        // Ruido de fondo: solo se mide si hay pausas reales (cuadros 20 dB por debajo de la voz)
        {
            const float median = percentile (histAll, 0.5);
            int quiet[numBinsDb] {}; long nq = 0, na = 0;
            for (int b = 0; b < numBinsDb; ++b)
            {
                na += histAll[b];
                if (-110.0f + 0.5f * (float) b < median - 20.0f) { quiet[b] = histAll[b]; nq += histAll[b]; }
            }
            r.noiseDb = (nq > na / 20) ? percentile (quiet, 0.5) : -100.0f;
        }
        r.dynLabel = r.dynRange > 14.0f ? "Alta" : r.dynRange > 8.0f ? "Media" : "Baja";
        r.noiseLabel = r.noiseDb <= -99.0f ? U ("Sin pausas para medir") : r.noiseDb > -50.0f ? U ("Alto (revisa la grabación)")
                     : r.noiseDb > -62.0f ? "Moderado" : "Bajo";
        if (r.noiseDb > -50.0f) r.recommendations.add (U ("Ruido de fondo alto (") + juce::String (juce::roundToInt (r.noiseDb)) + U (" dBFS): graba en un lugar más silencioso o usa una compuerta."));

        // Espectro promedio
        const int nb = fftSize / 2 + 1;
        std::vector<double> db ((size_t) nb);
        for (int k = 0; k < nb; ++k) db[(size_t) k] = 10.0 * std::log10 (spec[(size_t) k] / specFrames + 1.0e-20);
        auto bin = [&] (double hz) { return juce::jlimit (1, nb - 1, (int) std::round (hz / sr * fftSize)); };
        auto band = [&] (double lo, double hi)
        {
            double s = 0; for (int k = bin (lo); k <= bin (hi); ++k) s += spec[(size_t) k];
            return s / specFrames;
        };
        const double total = band (60, 12000) + 1e-20;
        auto rel = [&] (double lo, double hi) { return (float) (10.0 * std::log10 (band (lo, hi) / total + 1e-20)); };

        // Resonancia: el pico que mas sobresale de la curva suavizada entre 150 Hz y 1 kHz
        float bestEx = 0.0f;
        for (int k = bin (150); k <= bin (1000); ++k)
        {
            const double f = (double) k * sr / fftSize;
            double s = 0; int c = 0;
            for (int j = bin (f / 1.26); j <= bin (f * 1.26); ++j) { s += db[(size_t) j]; ++c; }
            const float ex = (float) (db[(size_t) k] - s / c);
            if (ex > bestEx) { bestEx = ex; r.resonanceHz = (float) f; }
        }
        r.resonanceDb = bestEx;
        r.cutResonance = bestEx > 3.0f;

        // Sibilancia (5-9 kHz), presencia (2.5-5 kHz) y graves (< 120 Hz)
        r.sibRel = rel (5000, 9000);
        r.sibLabel = r.sibRel > -14.0f ? "Alta" : r.sibRel > -22.0f ? "Moderada" : "Baja";
        double best = -1;
        for (int k = bin (4500); k <= bin (9500); ++k) if (spec[(size_t) k] > best) { best = spec[(size_t) k]; r.sibHz = (float) (k * sr / fftSize); }
        r.presenceRel = rel (2500, 5000);
        r.presenceLabel = r.presenceRel < -16.0f ? "Poca" : r.presenceRel > -8.0f ? "Mucha" : "Correcta";
        r.lowRel = rel (60, 120);
        r.lowLabel = r.lowRel > -14.0f ? "Exceso de graves" : "Correctos";

        // Recomendaciones (y los valores que se aplican)
        auto hz = [] (float f) { return f >= 1000.0f ? juce::String (f / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (f)) + " Hz"; };
        r.inGain = juce::jlimit (-12.0f, 12.0f, -20.0f - r.rmsDb);
        if (std::abs (r.inGain) > 2.0f)
            r.recommendations.add ((r.inGain > 0 ? "Subir" : "Bajar") + U (" la entrada ") + juce::String (std::abs (r.inGain), 1) + U (" dB (nivel ideal ~ -20 dBFS)."));
        if (r.cutResonance)
        {
            r.midFreq = juce::jlimit (200.0f, 5000.0f, r.resonanceHz);
            r.midGain = -juce::jlimit (1.5f, 4.0f, r.resonanceDb * 0.7f);
            r.midQ = 2.0f;
            r.recommendations.add ("Reducir " + juce::String (-r.midGain, 1) + " dB en " + hz (r.midFreq) + " (resonancia).");
        }
        r.compRatio = r.dynRange > 14.0f ? 4.0f : 3.0f;
        r.compThr = juce::jlimit (-50.0f, -6.0f, r.rmsDb + r.inGain - 3.0f + (r.dynRange > 14.0f ? -2.0f : 0.0f));
        r.recommendations.add (U ("Compresión de 3-5 dB (LA Vocal, ratio ") + juce::String ((int) r.compRatio) + ":1).");
        if (r.sibLabel != "Baja")
            r.recommendations.add ("De-esser en " + hz (r.sibHz) + U (" (sibilancia ") + r.sibLabel.toLowerCase() + U (", lo hace la IA)."));
        r.presGain = r.presenceLabel == "Poca" ? 3.0f : r.presenceLabel == "Mucha" ? 0.0f : 1.5f;
        if (r.presenceLabel != "Correcta")
            r.recommendations.add ((r.presGain > 1.5f ? "Incrementar presencia +" : "Mantener presencia en +") + juce::String (r.presGain, 1) + " dB en 4 kHz.");
        if (r.lowLabel != "Correctos")
            r.recommendations.add ("Low cut en " + juce::String ((int) r.lowCut) + U (" Hz (graves de más para un ") + r.registro.toLowerCase() + ").");
        r.recommendations.add (U ("Autotune en ") + toptune_keyName (r.keyRoot) + (r.minor ? " menor" : " mayor") + U (" (tonalidad detectada)."));
        return r;
    }

private:
    static juce::String toptune_keyName (int k)
    {
        static const char* names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return names[juce::jlimit (0, 11, k)];
    }

    void analyseFrame() noexcept
    {
        double e = 0.0;
        for (int i = 0; i < fftSize; ++i)
        {
            const float v = fifo[(size_t) ((fifoPos + i) % fftSize)];
            data[(size_t) i] = v * window[(size_t) i];
            e += (double) v * v;
        }
        const float rmsDb = (float) (10.0 * std::log10 (e / fftSize + 1e-20));
        const int b = juce::jlimit (0, numBinsDb - 1, (int) ((rmsDb + 110.0f) * 2.0f));
        histAll[b]++;
        if (rmsDb < -50.0f) return;   // silencio: no cuenta para el espectro
        histVoiced[b]++;
        std::fill (data.begin() + fftSize, data.end(), 0.0f);
        fft->performFrequencyOnlyForwardTransform (data.data(), true);
        for (int k = 0; k <= fftSize / 2; ++k) spec[(size_t) k] += (double) data[(size_t) k] * data[(size_t) k];
        ++specFrames;
    }

    double sr = 48000.0;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> fifo, data, window;
    std::vector<double> spec;
    int fifoPos = 0, hopCount = 0;
    long specFrames = 0;
    int histAll[numBinsDb] {}, histVoiced[numBinsDb] {};
    float keyHist[12] {};
    double voicedSec = 0.0, totalSec = 0.0, logF0Sum = 0.0;
    long logF0Count = 0;
    std::atomic<bool> active { false }, ready { false };
};
} // namespace tvc
