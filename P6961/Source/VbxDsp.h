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

// ---------------------------------------------------------------------------------------------
// PREAMP A VALVULA calibrado con la especificacion del VOXBOX (manual y hoja de datos):
//   0 dBFS = +22 dBu (-18 dBFS = +4 dBu).  v = 1 dentro del modelo = +30 dBu.
//   THD preamp 1 kHz a +20 dBu ~0.3 %, preamp + EQ ~0.35 %, salida maxima ~+30 dBu a 1.5 % THD.
//   GAIN 40..60 NO es un pad: cambia la realimentacion negativa global (20 dB en pasos de 5).
//   40 = mas realimentacion (limpio, recorte mas duro), 50 = normal, 60 = casi sin realimentacion
//   (mas 2do armonico, mas adelante, recorte mas suave). El nivel sube 5 dB por paso, como el hardware.
// ---------------------------------------------------------------------------------------------
constexpr float dbuRef = 22.0f;                                   // 0 dBFS = +22 dBu
constexpr float inScale = 0.39810717f;                            // 10^((22-30)/20)

inline float softClip (float y, float t, float c) noexcept
{
    const float a = std::abs (y); if (a <= t) return y;
    const float w = c - t; return std::copysign (t + w * std::tanh ((a - t) / w), y);
}

// condensador de acople (paso alto de 1er orden, sin el limite de 10 Hz del Biquad)
class CapHP
{
public:
    void prepare (double sr, double fc) { const double k = std::tan (tvc::kPi * fc / sr); n = (float) (1.0 / (1.0 + k)); a1 = (float) ((k - 1.0) / (1.0 + k)); x1 = y1 = 0; }
    void reset() { x1 = y1 = 0; }
    inline float process (float x) noexcept { const float y = n * (x - x1) - a1 * y1; x1 = x; y1 = y; return y; }
private:
    float n = 1, a1 = 0, x1 = 0, y1 = 0;
};

// transformador: saturacion por flujo magnetico (solo graves profundos + nivel alto)
class Transformer
{
public:
    void prepare (double sr, float fluxCorner, float amount) { a = (float) std::exp (-2.0 * tvc::kPi * fluxCorner / sr); k = amount; flux = 0; }
    inline float process (float v) noexcept { flux = a * flux + (1.0f - a) * v; return v - k * flux * flux * flux; }
    void reset() { flux = 0; }
private:
    float a = 0.999f, k = 0, flux = 0;
};

// etapa de valvula clase A con realimentacion: y = u + a2 u^2 - a3 u^3, recorte suave asimetrico
struct TubeChar { float a2 = 0.0158f, a3 = 0.040f, tp = 0.87f, tn = 0.79f, cp = 1.10f, cn = 1.04f; };
inline TubeChar preChar (int gainIdx)
{
    static const float d[5] { 0.3162f, 0.5623f, 1.0f, 1.7783f, 3.1623f };   // menos realimentacion = mas distorsion
    const int g = juce::jlimit (0, 4, gainIdx);
    TubeChar c; c.a2 *= d[g]; c.a3 *= d[g];
    const float soft = (float) g / 4.0f;                                   // 60: codo mas suave y mas temprano
    c.tp = 0.97f - 0.2f * soft; c.tn = c.tp - 0.08f;
    return c;
}
inline TubeChar lineChar() { TubeChar c; c.a2 = 0.0044f; c.a3 = 0.010f; c.tp = 1.25f; c.tn = 1.18f; c.cp = 1.45f; c.cn = 1.38f; return c; }   // 5751 + 6414 (mas margen que el preamp)

class TubeStage
{
public:
    void prepare (double sr) { cap.prepare (sr, 2.0); }       // condensador de acople: sin DC
    void set (const TubeChar& c) { ch = c; }
    inline float process (float u) noexcept
    {
        float y = u + ch.a2 * u * u - ch.a3 * u * u * u;
        y = y >= 0.0f ? softClip (y, ch.tp, ch.cp) : softClip (y, ch.tn, ch.cn);
        return cap.process (y);
    }
private:
    TubeChar ch; CapHP cap;
};

// preamp completo: trafo de entrada -> 12AX7 + 6414 (realimentacion global) -> trafo de salida
class Preamp
{
public:
    void prepare (double sr)
    {
        inT.prepare (sr, 20.0f, 0.3f); outT.prepare (sr, 20.0f, 1.0f); tube.prepare (sr);
        dc.prepare (sr, 4.0);
    }
    void set (int gainIdx) { tube.set (preChar (gainIdx)); }
    // x en escala digital; drive = ganancia del paso de GAIN (50 = 1)
    inline float process (float x, float drive) noexcept
    {
        float v = inT.process (x * inScale);
        v = tube.process (v * drive);
        v = outT.process (v);
        return dc.process (v) / inScale;
    }
private:
    Transformer inT, outT; TubeStage tube; CapHP dc;
};

// amplificador de linea del EQ (recupera la perdida del EQ pasivo) + trafo de salida opcional (XFMR)
class LineAmp
{
public:
    void prepare (double sr) { tube.prepare (sr); tube.set (lineChar()); xf.prepare (sr, 20.0f, 1.6f); dc.prepare (sr, 4.0); air.setLowPass (sr, 42000.0, 0.6); air.reset(); }
    inline float process (float x, bool xfmr) noexcept
    {
        float v = tube.process (x * inScale);
        if (xfmr) v = air.process (xf.process (v));
        return dc.process (v) / inScale;
    }
private:
    TubeStage tube; Transformer xf; CapHP dc; tvc::Biquad air;
};

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
