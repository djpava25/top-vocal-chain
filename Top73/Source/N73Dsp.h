#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <complex>
#include "DSP/Biquad.h"

// TOP 73: preamp clase A con transformadores + EQ de 3 bandas + HPF (estilo 1073).
// La respuesta de cada posicion de ganancia sale de mediciones por impulso de un 1073 vintage:
//  - de 20 a 50 dB: muesca en 21 Hz (camino de baja ganancia), graves planos
//  - de 55 a 80 dB: corte resonante en ~13 Hz y agudos que se "abren" al subir la ganancia (+5.6 dB a 20 kHz en 80)
//  - el shelf de 12 kHz medido: centrado en 12.4 kHz, Q 0.48 (ajustes hechos a 96 kHz, error < 0.1 dB)
namespace n73
{
// posiciones de la perilla roja en el orden del equipo (sentido horario): MIC 20..80 y luego LINE -20..+10
inline const juce::StringArray& gainNames()
{
    static const juce::StringArray s { "MIC 20", "MIC 25", "MIC 30", "MIC 35", "MIC 40", "MIC 45", "MIC 50", "MIC 55", "MIC 60", "MIC 65", "MIC 70", "MIC 75", "MIC 80",
                                       "LINE -20", "LINE -15", "LINE -10", "LINE -5", "LINE 0", "LINE +5", "LINE +10" };
    return s;
}
inline bool isMic (int idx) { return idx <= 12; }
inline int micDb (int idx) { return 20 + 5 * idx; }
inline int lineDb (int idx) { return -20 + 5 * (idx - 13); }
// ganancia que "empuja" la etapa (dB respecto a MIC 50 / LINE 0)
inline float driveDb (int idx) { return isMic (idx) ? (float) (micDb (idx) - 50) : (float) lineDb (idx); }

struct Curve { bool lowPath; float notchF, notchG, notchQ, lsF, lsG; float hpF, hpQ, hsF, hsG, hsQ; };
// medidas ajustadas (ver README): 20..80 en pasos de 5
inline Curve measured (int db)
{
    struct M { int db; Curve c; };
    static const M t[] {
        { 20, { true, 21.1f, -6.76f, 3.47f, 625.0f, -1.06f, 0, 0, 0, 0, 0 } },
        { 30, { true, 21.05f, -6.77f, 3.70f, 100.0f, -0.27f, 0, 0, 0, 0, 0 } },
        { 40, { true, 21.03f, -6.89f, 3.74f, 100.0f, -0.28f, 0, 0, 0, 0, 0 } },
        { 45, { true, 20.96f, -6.64f, 3.90f, 100.0f, -0.32f, 0, 0, 0, 0, 0 } },
        { 50, { true, 21.05f, -6.61f, 3.76f, 100.0f, -0.26f, 0, 0, 0, 0, 0 } },
        { 55, { false, 0, 0, 0, 0, 0, 13.9f, 0.78f, 16531.0f, 0.35f, 0.50f } },
        { 60, { false, 0, 0, 0, 0, 0, 13.77f, 0.79f, 13639.0f, 0.08f, 1.34f } },
        { 65, { false, 0, 0, 0, 0, 0, 12.38f, 0.83f, 10864.0f, 0.73f, 0.76f } },
        { 70, { false, 0, 0, 0, 0, 0, 12.59f, 0.92f, 11279.0f, 3.11f, 0.63f } },
        { 75, { false, 0, 0, 0, 0, 0, 10.91f, 0.99f, 9166.0f, 4.87f, 0.52f } },
        { 80, { false, 0, 0, 0, 0, 0, 13.16f, 0.90f, 8272.0f, 6.49f, 0.46f } } };
    for (const auto& m : t) if (m.db == db) return m.c;
    // 25 y 35: mitad entre vecinos
    const Curve a = measured (db - 5), b = measured (db + 5);
    Curve c = a;
    c.lsF = 0.5f * (a.lsF + b.lsF); c.lsG = 0.5f * (a.lsG + b.lsG); c.notchG = 0.5f * (a.notchG + b.notchG);
    return c;
}
inline Curve curveFor (int idx) { return isMic (idx) ? measured (micDb (idx)) : measured (30); }

inline const float* midFreqs() { static const float f[] { 0, 360, 700, 1600, 3200, 4800, 7200 }; return f; }    // 0 = OFF
inline const float* lowFreqs() { static const float f[] { 0, 35, 60, 110, 220 }; return f; }
inline const float* hpfFreqs() { static const float f[] { 0, 50, 80, 160, 300 }; return f; }

inline void shelfQ (tvc::Biquad& b, double sr, double f, double q, double gainDb, bool high)
{
    const double A = std::pow (10.0, gainDb / 40.0), w0 = 2.0 * tvc::kPi * std::min (f, sr * 0.45) / sr;
    const double c = std::cos (w0), s = std::sin (w0), al = s / (2.0 * q), sA = 2.0 * std::sqrt (A) * al;
    if (high) b.setNormalised (A * ((A + 1) + (A - 1) * c + sA), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sA),
                               (A + 1) - (A - 1) * c + sA, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sA);
    else b.setNormalised (A * ((A + 1) - (A - 1) * c + sA), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sA),
                          (A + 1) + (A - 1) * c + sA, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sA);
}
inline void hp1 (tvc::Biquad& b, double sr, double f)
{
    const double k = std::tan (tvc::kPi * f / sr), n = 1.0 / (1.0 + k);
    b.setNormalised (n, -n, 0.0, 1.0, (k - 1.0) * n, 0.0);
}

struct Settings
{
    int gain = 6, midFreq = 3, lowFreq = 2, hpf = 0;
    float hf = 0, midGain = 0, lowGain = 0;
    bool eq = true;
    bool operator== (const Settings& o) const
    { return gain == o.gain && midFreq == o.midFreq && lowFreq == o.lowFreq && hpf == o.hpf && hf == o.hf && midGain == o.midGain && lowGain == o.lowGain && eq == o.eq; }
};

// filtros: [0..2] preamp medido, [3..8] EQ + HPF
constexpr int nFilters = 9;
inline std::array<tvc::Biquad, nFilters> design (const Settings& s, double sr)
{
    std::array<tvc::Biquad, nFilters> f;
    for (auto& b : f) b.setIdentity();
    const Curve c = curveFor (s.gain);
    if (c.lowPath)
    {
        f[0].setPeak (sr, c.notchF, c.notchQ, c.notchG);
        shelfQ (f[1], sr, c.lsF, 0.7, c.lsG, false);
    }
    else
    {
        f[0].setHighPass (sr, c.hpF, c.hpQ);
        if (c.hsG > 0.05f) shelfQ (f[1], sr, c.hsF, c.hsQ, c.hsG, true);
    }
    if (s.eq)
    {
        // agudos: shelf de 12 kHz (forma medida)
        if (std::abs (s.hf) > 0.01f) shelfQ (f[3], sr, 12400.0, 0.48, s.hf * 1.25f, true);   // medido: "+6" = 7.5 dB de estante centrado en 12.4 kHz
        // medios: campana con inductor (ancha), 6 frecuencias
        const float mf = midFreqs()[juce::jlimit (0, 6, s.midFreq)];
        if (mf > 0 && std::abs (s.midGain) > 0.01f) f[4].setPeak (sr, mf, 0.9, s.midGain);
        // graves: shelf resonante (pequeno rebote tipico)
        const float lf = lowFreqs()[juce::jlimit (0, 4, s.lowFreq)];
        if (lf > 0 && std::abs (s.lowGain) > 0.01f) shelfQ (f[5], sr, lf, 1.0, s.lowGain, false);
        // HPF 18 dB/oct (1er + 2do orden)
        const float hf = hpfFreqs()[juce::jlimit (0, 4, s.hpf)];
        if (hf > 0) { hp1 (f[6], sr, hf); f[7].setHighPass (sr, hf, 1.0); }
    }
    return f;
}

inline float responseDb (const Settings& s, double f, double sr = 384000.0)
{
    auto fl = design (s, sr);
    const double w = 2.0 * tvc::kPi * f / sr;
    const std::complex<double> z1 = std::polar (1.0, -w), z2 = std::polar (1.0, -2.0 * w);
    double mag = 1.0;
    for (auto& q : fl) mag *= std::abs (((double) q.b0 + (double) q.b1 * z1 + (double) q.b2 * z2) / (1.0 + (double) q.a1 * z1 + (double) q.a2 * z2));
    return (float) (20.0 * std::log10 (mag + 1e-12));
}

inline float asym (float x, float k, float b) noexcept
{
    const float t0 = std::tanh (k * b), d = k * (1.0f - t0 * t0);
    return (std::tanh (k * (x + b)) - t0) / d;
}

// ---------------------------------------------------------------------------------------------
// PREAMP (calibrado con las especificaciones del 1073):
//  - THD <= 0.07% de 50 Hz a 10 kHz con la salida a +20 dBu; salida maxima > +26 dBu
//  - 2do y 3er armonico casi iguales (el 3ro un poco mayor); a ganancia maxima un poco mas de 2do
//    y algo de armonicos altos
// Escala interna: 0 dBFS = +24 dBu (seno). v = 1.0 equivale a +26 dBu.
//  * Transformadores (entrada Marinair / salida LO1166 con entrehierro): modelo de flujo magnetico:
//    el flujo es la integral de la senal, asi que la saturacion crece con el nivel y baja con la frecuencia
//    (solo colorea graves profundos y niveles altos).
//  * Etapas clase A: polinomio de 2do/3er orden muy pequeno (nivel de spec) + recorte suave cerca de +26 dBu,
//    un poco asimetrico (clase A). Con mas ganancia de mic hay menos realimentacion: mas armonicos.
// ---------------------------------------------------------------------------------------------
struct Character { float a2 = 0.0009f, a3 = 0.0035f, a5 = 0.0f; };   // por etapa (hay dos): +20 dBu -> ~0.05% THD
inline Character characterFor (int gainIdx)
{
    Character c;
    if (isMic (gainIdx))
    {
        const int db = micDb (gainIdx);
        if (db > 55)
        {
            const float f = std::pow (10.0f, (float) (db - 55) / 25.0f);   // MIC 80: x10
            c.a2 *= f * 1.5f; c.a3 *= f; c.a5 = 0.002f * (f - 1.0f) / 9.0f;
        }
    }
    return c;
}
// recorte de clase A: lineal hasta la rodilla y luego curva tanh continua en pendiente (sin quiebre brusco,
// los armonicos altos caen rapido = sin aspereza ni aliasing). Asimetrico como una etapa clase A.
inline float softClip (float y, float t, float c) noexcept
{
    const float a = std::abs (y);
    if (a <= t) return y;
    const float w = c - t;
    return std::copysign (t + w * std::tanh ((a - t) / w), y);
}
inline float classAClip (float y) noexcept { return y >= 0.0f ? softClip (y, 0.80f, 1.16f) : softClip (y, 0.74f, 1.06f); }

class Transformer
{
public:
    void prepare (double sr, float fluxCorner, float amount)
    {
        a = std::exp (-2.0f * juce::MathConstants<float>::pi * fluxCorner / (float) sr);
        k = amount; flux = 0;
    }
    void reset() { flux = 0; }
    inline float process (float v) noexcept
    {
        flux = a * flux + (1.0f - a) * v;                 // integral con fuga (flujo magnetico)
        return v - k * flux * flux * flux;                // corriente de magnetizacion no lineal (3er armonico)
    }
private:
    float a = 0.999f, k = 0.16f, flux = 0;
};

// un canal: transformador de entrada -> clase A -> (curva medida) -> EQ -> clase A de salida -> transformador de salida
class Channel
{
public:
    static constexpr float inScale = 0.7943282f;          // +24 dBu (0 dBFS) -> +26 dBu = 1.0
    void prepare (double rate)
    {
        sr = rate; for (auto& b : f) b.reset();
        inT.prepare (sr, 20.0f, 0.035f);                   // Marinair de entrada
        outT.prepare (sr, 20.0f, 0.018f);                  // LO1166 con entrehierro (satura menos)
        dc.setHighPass1 (sr, 5.0); dc.reset();
        cap1.setHighPass1 (sr, 2.0); cap2.setHighPass1 (sr, 2.0); cap1.reset(); cap2.reset();   // condensadores de acople
    }
    void set (const std::array<tvc::Biquad, nFilters>& d, float driveGain, Character c)
    {
        for (size_t i = 0; i < (size_t) nFilters; ++i) { const float z1 = f[i].z1, z2 = f[i].z2; f[i] = d[i]; f[i].z1 = z1; f[i].z2 = z2; }
        drive = driveGain; ch = c;
    }
    inline float stage (float v) const noexcept
    {
        const float v2 = v * v;
        return classAClip (v + ch.a2 * v2 - ch.a3 * v2 * v - ch.a5 * v2 * v2 * v);
    }
    inline float process (float x) noexcept
    {
        float v = x * drive * inScale;
        v = inT.process (v);
        v = cap1.process (stage (v));                     // amplificador de mic (clase A) + acople
        v = f[0].process (v); v = f[1].process (v); v = f[2].process (v);
        for (int i = 3; i < nFilters; ++i) v = f[(size_t) i].process (v);
        v = cap2.process (stage (v));                     // amplificador de salida (clase A) + acople (sin DC en el trafo)
        v = outT.process (v);
        return dc.process (v) / inScale;
    }
private:
    double sr = 384000.0;
    std::array<tvc::Biquad, nFilters> f;
    Transformer inT, outT;
    tvc::Biquad dc, cap1, cap2;
    float drive = 1.0f;
    Character ch;
};
} // namespace n73
