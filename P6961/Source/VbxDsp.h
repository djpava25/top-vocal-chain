#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <complex>
#include "DSP/Biquad.h"

// 6961P: canal de voz a valvulas (estilo VoxBox): HPF -> INPUT -> compresor optico 3:1 -> preamp a valvula (40-60 dB)
// -> EQ pasivo pico-valle-pico -> amplificador de linea -> de-esser / limitador (post EQ) -> salida.
// Formas de las bandas ajustadas a 14 mediciones por impulso: LOW PEAK Q 0.6, MID DIP Q 1.1, HI PEAK Q 0.8.
namespace vbx
{
inline const float* lowFreqs() { static const float f[] { 20, 35, 50, 70, 100, 150, 200, 300, 500, 700, 1000 }; return f; }
inline const float* midFreqs() { static const float f[] { 200, 300, 500, 700, 1000, 1500, 2000, 3000, 4000, 5000, 7000 }; return f; }
inline const float* hiFreqs()  { static const float f[] { 1500, 2000, 3000, 4000, 5000, 6400, 8000, 10000, 12000, 16000, 20000 }; return f; }
inline const float* dsFreqs()  { static const float f[] { 3000, 6000, 9000, 12000 }; return f; }      // indice 4 = LIMIT 10:1
inline const float* attackMs() { static const float f[] { 4, 8, 15, 30, 70 }; return f; }              // FAST .. SLOW
inline const float* releaseS() { static const float f[] { 0.3f, 0.5f, 1.0f, 2.0f, 5.0f }; return f; }
constexpr float lowQ = 0.6f, midQ = 1.1f, hiQ = 0.8f;

inline float asym (float x, float k, float b) noexcept
{
    const float t0 = std::tanh (k * b), d = k * (1.0f - t0 * t0);
    return (std::tanh (k * (x + b)) - t0) / d;
}

struct EqSettings
{
    bool on = true; int lowF = 4, midF = 4, hiF = 7, hpf = 0; float low = 0, mid = 0, hi = 0;
    bool operator== (const EqSettings& o) const { return on == o.on && lowF == o.lowF && midF == o.midF && hiF == o.hiF && hpf == o.hpf && low == o.low && mid == o.mid && hi == o.hi; }
};
// HPF 6 dB/oct (fase minima) + 3 bandas pasivas
inline std::array<tvc::Biquad, 4> design (const EqSettings& s, double sr)
{
    std::array<tvc::Biquad, 4> f;
    for (auto& b : f) b.setIdentity();
    if (s.hpf > 0)
    {
        const double fc = s.hpf == 1 ? 80.0 : 120.0, k = std::tan (tvc::kPi * fc / sr), n = 1.0 / (1.0 + k);
        f[0].setNormalised (n, -n, 0.0, 1.0, (k - 1.0) * n, 0.0);
    }
    if (s.on)
    {
        if (s.low > 0.01f) f[1].setPeak (sr, lowFreqs()[juce::jlimit (0, 10, s.lowF)], lowQ, s.low);
        if (s.mid > 0.01f) f[2].setPeak (sr, midFreqs()[juce::jlimit (0, 10, s.midF)], midQ, -s.mid);
        if (s.hi > 0.01f)  f[3].setPeak (sr, std::min ((double) hiFreqs()[juce::jlimit (0, 10, s.hiF)], sr * 0.45), hiQ, s.hi);
    }
    return f;
}
inline float responseDb (const EqSettings& s, double f, double sr = 96000.0)
{
    auto fl = design (s, sr);
    const double w = 2.0 * tvc::kPi * f / sr;
    const std::complex<double> z1 = std::polar (1.0, -w), z2 = std::polar (1.0, -2.0 * w);
    double mag = 1.0;
    for (auto& q : fl) mag *= std::abs (((double) q.b0 + (double) q.b1 * z1 + (double) q.b2 * z2) / (1.0 + (double) q.a1 * z1 + (double) q.a2 * z2));
    return (float) (20.0 * std::log10 (mag + 1e-12));
}

// celda optica (LDR): deteccion RMS/pico, codo suave, ataque por pasos y release en dos etapas
class Opto
{
public:
    void prepare (double rate) { sr = rate; env = fast = slow = rms = peak = 0; }
    void set (float thrDb, float ratio, float attMs, float relS, float maxGr)
    {
        thr = thrDb; r = ratio; mx = maxGr;
        aAtt = std::exp (-1.0f / (float) (sr * attMs * 0.001));
        aRel = std::exp (-1.0f / (float) (sr * relS * 0.35));        // primera etapa: rapida
        aRel2 = std::exp (-1.0f / (float) (sr * relS * 1.6));        // segunda etapa: memoria de la celda
        aRms = std::exp (-1.0f / (float) (sr * 0.008));
        aPk = std::exp (-1.0f / (float) (sr * 0.004));
    }
    // devuelve la reduccion en dB (positiva)
    inline float detect (float sc) noexcept
    {
        rms = aRms * rms + (1 - aRms) * sc * sc;
        const float a = std::abs (sc);
        peak = a > peak ? a : aPk * peak + (1 - aPk) * a;
        const float lv = juce::Decibels::gainToDecibels (0.5f * (std::sqrt (2.0f * rms) + peak) + 1.0e-7f, -140.0f);
        const float knee = 6.0f, o = lv - thr;
        float gr = 0;
        if (o > knee * 0.5f) gr = (1 - 1 / r) * o;
        else if (o > -knee * 0.5f) { const float k = o + knee * 0.5f; gr = (1 - 1 / r) * k * k / (2 * knee); }
        gr = std::min (gr, mx);
        if (gr > env) { env = aAtt * env + (1 - aAtt) * gr; fast = slow = env; }
        else
        {
            fast = aRel * fast + (1 - aRel) * gr;
            slow = aRel2 * slow + (1 - aRel2) * gr;
            env = std::max (fast, slow * 0.6f);
        }
        return env;
    }
    float reduction() const { return env; }
private:
    double sr = 96000.0;
    float thr = 0, r = 3, mx = 16, aAtt = 0, aRel = 0, aRel2 = 0, aRms = 0, aPk = 0;
    float env = 0, fast = 0, slow = 0, rms = 0, peak = 0;
};
} // namespace vbx
