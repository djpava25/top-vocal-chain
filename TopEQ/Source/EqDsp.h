#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <complex>
#include "DSP/Biquad.h"

// TOP EQ-1A: ecualizador pasivo a valvulas (estilo programa clasico de los 50).
// Curvas modeladas: el realce de graves es una curva resonante (2do orden) y la atenuacion es de 1er orden
// un poco mas arriba: por eso subir y bajar a la vez da graves gordos con un hueco suave encima ("el truco").
namespace teq
{
inline const float* lowFreqs()  { static const float f[] { 20, 30, 60, 100 }; return f; }
inline const float* highFreqs() { static const float f[] { 3000, 4000, 5000, 8000, 10000, 12000, 16000 }; return f; }
inline const float* attFreqs()  { static const float f[] { 5000, 10000, 20000 }; return f; }
constexpr float lowBoostMax = 13.5f, lowAttenMax = 17.5f, highBoostMax = 16.0f, highAttenMax = 16.0f;

// ---- calculo de coeficientes (prewarp bilineal)
inline void lowShelfQ (tvc::Biquad& b, double sr, double f, double q, double gainDb)
{
    const double A = std::pow (10.0, gainDb / 40.0), w0 = 2.0 * tvc::kPi * std::min (f, sr * 0.45) / sr;
    const double c = std::cos (w0), s = std::sin (w0), al = s / (2.0 * q), sA = 2.0 * std::sqrt (A) * al;
    b.setNormalised (A * ((A + 1) - (A - 1) * c + sA), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sA),
                     (A + 1) + (A - 1) * c + sA, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sA);
}
inline void highShelfQ (tvc::Biquad& b, double sr, double f, double q, double gainDb)
{
    const double A = std::pow (10.0, gainDb / 40.0), w0 = 2.0 * tvc::kPi * std::min (f, sr * 0.45) / sr;
    const double c = std::cos (w0), s = std::sin (w0), al = s / (2.0 * q), sA = 2.0 * std::sqrt (A) * al;
    b.setNormalised (A * ((A + 1) + (A - 1) * c + sA), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sA),
                     (A + 1) - (A - 1) * c + sA, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sA);
}
// estante de 1er orden: ganancia g (dB) en graves, 0 dB en agudos; f = frecuencia de transicion
inline void lowShelf1 (tvc::Biquad& b, double sr, double f, double gainDb)
{
    const double G = std::pow (10.0, gainDb / 20.0), sq = std::sqrt (G);
    const double wc = 2.0 * sr * std::tan (tvc::kPi * std::min (f, sr * 0.45) / sr);
    // H(s) = (s + wc*sqrt(G)) / (s + wc/sqrt(G)) * ... normalizado: DC = G, HF = 1
    const double wz = wc * sq, wp = wc / sq, k = 2.0 * sr;
    b.setNormalised (k + wz * 1.0, wz - k, 0.0, k + wp, wp - k, 0.0);
    // DC = wz/wp = G  ;  HF = 1
}

struct Settings
{
    float lowBoost = 0, lowAtten = 0, highBoost = 0, bandwidth = 5, highAtten = 0, tube = 30;
    int lowFreq = 2, highFreq = 4, attFreq = 1;
    bool operator== (const Settings& o) const
    {
        return lowBoost == o.lowBoost && lowAtten == o.lowAtten && highBoost == o.highBoost && bandwidth == o.bandwidth && highAtten == o.highAtten
            && tube == o.tube && lowFreq == o.lowFreq && highFreq == o.highFreq && attFreq == o.attFreq;
    }
};

// las 4 secciones como filtros
inline std::array<tvc::Biquad, 4> design (const Settings& s, double sr)
{
    std::array<tvc::Biquad, 4> f;
    const float lf = lowFreqs()[juce::jlimit (0, 3, s.lowFreq)];
    const float hf = highFreqs()[juce::jlimit (0, 6, s.highFreq)];
    const float af = attFreqs()[juce::jlimit (0, 2, s.attFreq)];
    // realce de graves: curva resonante con un pequeno pico cerca de la frecuencia elegida
    const float lb = lowBoostMax * s.lowBoost / 10.0f;
    if (lb > 0.01f) lowShelfQ (f[0], sr, lf * 1.1, 0.95, lb); else f[0].setIdentity();
    // atenuacion de graves: 1er orden, mas arriba (deja un hueco suave encima del realce)
    // en el circuito pasivo las dos secciones interactuan: con el realce puesto, la atenuacion pesa menos en el grave profundo
    const float la = lowAttenMax * s.lowAtten / 10.0f * (1.0f - 0.45f * lb / lowBoostMax);
    if (la > 0.01f) lowShelf1 (f[1], sr, lf * 3.4, -la * 0.62f); else f[1].setIdentity();
    // realce de agudos: campana resonante; BANDWIDTH 0 = angosta, 10 = ancha
    const float hb = highBoostMax * s.highBoost / 10.0f;
    const double q = 2.4 * std::pow (0.16, s.bandwidth / 10.0);       // 2.4 .. 0.38
    if (hb > 0.01f) f[2].setPeak (sr, std::min ((double) hf, sr * 0.45), q, hb); else f[2].setIdentity();
    // atenuacion de agudos: estante suave
    const float ha = highAttenMax * s.highAtten / 10.0f;
    if (ha > 0.01f) highShelfQ (f[3], sr, af * 0.75, 0.55, -ha); else f[3].setIdentity();
    return f;
}

inline float responseDb (const Settings& s, double f)
{
    const double sr = 96000.0;
    auto fl = design (s, sr);
    const double w = 2.0 * tvc::kPi * f / sr;
    const std::complex<double> z1 = std::polar (1.0, -w), z2 = std::polar (1.0, -2.0 * w);
    double mag = 1.0;
    for (auto& q : fl)
        mag *= std::abs (((double) q.b0 + (double) q.b1 * z1 + (double) q.b2 * z2) / (1.0 + (double) q.a1 * z1 + (double) q.a2 * z2));
    return (float) (20.0 * std::log10 (mag + 1e-12));
}

inline float asym (float x, float k, float b) noexcept
{
    const float t0 = std::tanh (k * b), d = k * (1.0f - t0 * t0);
    return (std::tanh (k * (x + b)) - t0) / d;
}

// cadena completa por canal (a frecuencia sobremuestreada)
class Channel
{
public:
    void prepare (double rate) { sr = rate; for (auto& b : eq) b.reset(); inLo.setLowPass1 (sr, 120.0); outLo.setLowPass1 (sr, 90.0); dc.setHighPass1 (sr, 6.0); inLo.reset(); outLo.reset(); dc.reset(); }
    void setEq (const std::array<tvc::Biquad, 4>& d)
    {
        for (size_t i = 0; i < 4; ++i) { const float z1 = eq[i].z1, z2 = eq[i].z2; eq[i] = d[i]; eq[i].z1 = z1; eq[i].z2 = z2; }
    }
    void setTube (float t) { tube = t / 100.0f; }
    inline float process (float x) noexcept
    {
        // transformador de entrada: graves fuertes con un poco de 3er armonico
        if (tube > 0.001f) { const float lo = inLo.process (x); x = x - lo + asym (lo, 1.0f + 2.0f * tube, 0.0f); }
        for (auto& b : eq) x = b.process (x);
        // amplificador de valvula (recupera la perdida del EQ pasivo): armonicos pares suaves
        if (tube > 0.001f)
        {
            x = asym (x, 1.0f + 1.6f * tube, 0.14f);
            const float lo = outLo.process (x); x = x - lo + asym (lo, 1.0f + 1.5f * tube, 0.0f);   // transformador de salida
            x = dc.process (x);
        }
        return x;
    }
private:
    double sr = 96000.0;
    std::array<tvc::Biquad, 4> eq;
    tvc::Biquad inLo, outLo, dc;
    float tube = 0.3f;
};
} // namespace teq
