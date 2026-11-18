#pragma once
#include "Biquad.h"
#include <cmath>
#include <algorithm>

namespace tvc
{
//==============================================================================
// "IA Voice": motor de analisis adaptativo que escucha la voz en tiempo real y
// calcula correcciones para cada modulo de la cadena.
//
// Mide (solo mientras hay voz, ignorando silencios):
//   - Nivel RMS de la toma            -> ganancia de entrada
//   - Energia de graves (< 150 Hz)    -> low cut
//   - Energia "barro" (200-500 Hz)    -> recorte de mud
//   - Energia de agudos (> 4 kHz)     -> presencia / aire
//   - Sibilancia (5-9 kHz)            -> de-esser estatico
//   - Rango dinamico                  -> umbral y ratio del compresor
//   - Desafinacion media (cents)      -> velocidad del autotune
//
// La perilla "IA Voice" (0-100 %) decide cuanto de estas correcciones se aplica.
class VoiceAI
{
public:
    struct Suggestions
    {
        float gainDb = 0, lowCutHz = 0, mudDb = 0, presenceDb = 0, airDb = 0,
              deEssDb = 0, compThrDb = 0, compRatio = 0, tuneSpeedMs = 0;
        float confidence = 0;       // 0..1
        float avgPitchHz = 0;
    };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        low.setLowPass (sr, 150.0, 0.707);
        mudHp.setHighPass (sr, 200.0, 0.707);
        mudLp.setLowPass (sr, 500.0, 0.707);
        high.setHighPass (sr, 4000.0, 0.707);
        sibHp.setHighPass (sr, 5000.0, 0.707);
        sibLp.setLowPass (sr, 9000.0, 0.707);
        shortCoef = (float) std::exp (-1.0 / (0.03 * sr));     // RMS corto 30 ms
        reset();
    }

    void reset()
    {
        low.reset(); mudHp.reset(); mudLp.reset(); high.reset(); sibHp.reset(); sibLp.reset();
        msTotal = msLow = msMud = msHigh = msSib = 1.0e-6;
        shortMs = 0.0f;
        dynMean = -30.0; dynSq = 900.0;
        devCents = 0.0; pitchLog = 0.0;
        activeSeconds = 0.0; pitchSeconds = 0.0;
        current = {};
    }

    // Llamar una vez por bloque con la senal mono de entrada
    void process (const float* mono, int n, bool learning) noexcept
    {
        if (! learning || n <= 0) return;

        double bTotal = 0, bLow = 0, bMud = 0, bHigh = 0, bSib = 0;
        int activeSamples = 0;

        for (int i = 0; i < n; ++i)
        {
            const float x = mono[i];
            const float l = low.process (x);
            const float m = mudLp.process (mudHp.process (x));
            const float h = high.process (x);
            const float s = sibLp.process (sibHp.process (x));

            shortMs = shortCoef * shortMs + (1.0f - shortCoef) * x * x;
            if (shortMs > gateMs)
            {
                ++activeSamples;
                bTotal += x * x; bLow += l * l; bMud += m * m; bHigh += h * h; bSib += s * s;
            }
        }

        if (activeSamples < n / 4) return;   // casi todo silencio: no aprender

        const double secs = (double) activeSamples / sr;
        activeSeconds += secs;

        // Promedios lentos (~4 s)
        const double a = 1.0 - std::exp (-secs / 4.0);
        const double inv = 1.0 / activeSamples;
        msTotal += a * (bTotal * inv - msTotal);
        msLow   += a * (bLow   * inv - msLow);
        msMud   += a * (bMud   * inv - msMud);
        msHigh  += a * (bHigh  * inv - msHigh);
        msSib   += a * (bSib   * inv - msSib);

        const double shortDb = 10.0 * std::log10 (std::max ((double) shortMs, 1.0e-12));
        dynMean += a * (shortDb - dynMean);
        dynSq   += a * (shortDb * shortDb - dynSq);

        update();
    }

    // Desafinacion medida por el autotune (cents respecto a la nota mas cercana)
    void addPitchObservation (float hz, float centsOff, double seconds, bool learning) noexcept
    {
        if (! learning || hz <= 0.0f) return;
        pitchSeconds += seconds;
        const double a = 1.0 - std::exp (-seconds / 3.0);
        devCents += a * (std::abs (centsOff) - devCents);
        const double lg = std::log2 ((double) hz);
        pitchLog = pitchSeconds <= seconds ? lg : pitchLog + a * (lg - pitchLog);
    }

    const Suggestions& get() const noexcept { return current; }

private:
    static float rel (double band, double total) noexcept
    {
        return (float) (10.0 * std::log10 (std::max (band, 1.0e-12) / std::max (total, 1.0e-12)));
    }

    void update() noexcept
    {
        Suggestions s;
        const float totalDb  = (float) (10.0 * std::log10 (std::max (msTotal, 1.0e-12)));
        const float lowRel   = rel (msLow, msTotal);
        const float mudRel   = rel (msMud, msTotal);
        const float highRel  = rel (msHigh, msTotal);
        const float sibRel   = rel (msSib, msTotal);
        const float dynRange = (float) std::sqrt (std::max (0.0, dynSq - dynMean * dynMean));

        s.gainDb      = std::clamp (-20.0f - totalDb, -12.0f, 12.0f);
        s.lowCutHz    = std::clamp ((lowRel + 16.0f) * 8.0f, 0.0f, 120.0f);
        s.mudDb       = -std::clamp ((mudRel + 5.0f) * 0.8f, 0.0f, 5.0f);
        s.presenceDb  = std::clamp ((-22.0f - highRel) * 0.35f, -3.0f, 4.0f);
        s.airDb       = std::clamp ((-24.0f - highRel) * 0.40f, -3.0f, 5.0f);
        s.deEssDb     = -std::clamp ((sibRel + 24.0f) * 0.8f, 0.0f, 6.0f);
        s.compThrDb   = -std::clamp ((dynRange - 4.0f) * 1.0f, 0.0f, 8.0f);
        s.compRatio   = std::clamp ((dynRange - 4.0f) * 0.25f, 0.0f, 2.5f);
        s.tuneSpeedMs = -std::clamp (((float) devCents - 12.0f) * 1.2f, 0.0f, 60.0f);
        s.avgPitchHz  = pitchSeconds > 0.2 ? (float) std::exp2 (pitchLog) : 0.0f;

        // La confianza crece con los segundos de voz escuchados (plena a los 3 s)
        s.confidence = (float) std::clamp (activeSeconds / 3.0, 0.0, 1.0);

        // Mientras la confianza es baja las sugerencias se aplican parcialmente
        const float c = s.confidence;
        s.gainDb *= c; s.lowCutHz *= c; s.mudDb *= c; s.presenceDb *= c; s.airDb *= c;
        s.deEssDb *= c; s.compThrDb *= c; s.compRatio *= c; s.tuneSpeedMs *= c;
        current = s;
    }

    double sr = 44100.0;
    Biquad low, mudHp, mudLp, high, sibHp, sibLp;
    float shortMs = 0.0f, shortCoef = 0.999f;
    static constexpr float gateMs = 1.0e-5f;   // ~ -50 dBFS
    double msTotal = 1e-6, msLow = 1e-6, msMud = 1e-6, msHigh = 1e-6, msSib = 1e-6;
    double dynMean = -30.0, dynSq = 900.0;
    double devCents = 0.0, pitchLog = 0.0;
    double activeSeconds = 0.0, pitchSeconds = 0.0;
    Suggestions current;
};
} // namespace tvc
