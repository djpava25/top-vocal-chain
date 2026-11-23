#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>
#include <complex>
#include "DSP/Biquad.h"

// TOP EQ-1A: modelo del CIRCUITO (no de curvas) de un ecualizador pasivo de programa de los 50.
//
// Red pasiva (topologia del esquema original, analizada en Barrera, Lizarraga-Seijas y Font,
// "Modeling the Pultec EQP-1A with Wave Digital Filters", SMC 2024):
//   fuente 600 ohm -> pot HF BOOST 10k (con LC resonante + pot BANDWIDTH entre cursor y salida)
//   -> [pot LOW ATTEN 100k || C] -> salida, carga en derivacion: 12k + [pot LOW BOOST 10k || C] a masa,
//   y la red HF ATTEN (pot 1k con R+C) entre el nodo central y la pata del LOW BOOST.
// Como todo es un solo circuito, las secciones interactuan igual que en el hardware
// (el "truco" de subir y bajar graves sale solo de la red, no esta programado aparte).
//
// Valores calibrados contra el manual original y las mediciones publicadas (Sound On Sound 2019):
//   LOW BOOST 13.2-13.4 dB (manual 13.5), LOW ATTEN 16.9-17.4 dB (manual 17.5), HIGH BOOST 18 dB SHARP
//   (manual 18) y 8.6 dB menos en BROAD (SoS: 9 dB), perdida pasiva ~21 dB (SoS: ~20), 10/10 a 60 Hz
//   deja un hueco de ~5 dB cerca de 550 Hz.
//
// Se simula con el metodo nodal (MNA) y reglas trapezoidales: cada condensador e inductor guarda su
// tension y corriente reales, asi que mover perillas o cambiar llaves se comporta como el aparato.
namespace teq
{
inline const float* lowFreqs()  { static const float f[] { 20, 30, 60, 100 }; return f; }
inline const float* highFreqs() { static const float f[] { 3000, 4000, 5000, 8000, 10000, 12000, 16000 }; return f; }
inline const float* attFreqs()  { static const float f[] { 5000, 10000, 20000 }; return f; }

// valores de los componentes
namespace part
{
    constexpr double Rs = 450.0;                  // fuente (trafo de entrada 600 ohm)
    constexpr double potHB = 10e3, potBW = 3.3e3, potLA = 100e3, potLB = 10e3, potHA = 1e3;
    constexpr double R2 = 140.0, R3 = 12e3, Rgrid = 1e6, RLperH = 500.0;   // R del bobinado: 0.5 ohm/mH
    inline double lowBoostC (int i) { static const double c[] { 1.5e-6, 1.0e-6, 0.47e-6, 0.28e-6 }; return c[juce::jlimit (0, 3, i)]; }
    inline double lowAttenC (int i) { static const double c[] { 56e-9, 36e-9, 20e-9, 10e-9 }; return c[juce::jlimit (0, 3, i)]; }
    inline double hfTap (int i)     { static const double l[] { 0.175, 0.100, 0.100, 0.050, 0.050, 0.025, 0.025 }; return l[juce::jlimit (0, 6, i)]; }   // bobina con tomas
    inline double hfCap (int i)     { const double f = highFreqs()[juce::jlimit (0, 6, i)], l = hfTap (i); return 1.0 / ((2.0 * juce::MathConstants<double>::pi * f) * (2.0 * juce::MathConstants<double>::pi * f) * l); }
    inline double hfAttenC (int i)  { static const double c[] { 150e-9, 68e-9, 33e-9 }; return c[juce::jlimit (0, 2, i)]; }
}

// curvas de los potenciometros (perilla 0..10 -> fraccion de resistencia)
inline double taperLog (double x, double k) { return (std::exp (k * x) - 1.0) / (std::exp (k) - 1.0); }
inline double taperRev (double x, double k) { return 1.0 - taperLog (1.0 - x, k); }

struct Settings
{
    float lowBoost = 0, lowAtten = 0, highBoost = 0, bandwidth = 5, highAtten = 0;
    int lowFreq = 2, highFreq = 4, attFreq = 1;
    bool operator== (const Settings& o) const
    {
        return lowBoost == o.lowBoost && lowAtten == o.lowAtten && highBoost == o.highBoost && bandwidth == o.bandwidth && highAtten == o.highAtten
            && lowFreq == o.lowFreq && highFreq == o.highFreq && attFreq == o.attFreq;
    }
};

// ---------------------------------------------------------------------------------------------
// la red como lista de componentes
// nodos: 0 a (entrada), 1 w (cursor HF BOOST), 2 m1, 3 m2, 4 n4 (centro), 5 x (cursor HF ATTEN), 6 m3, 7 n7, 8 out
// ---------------------------------------------------------------------------------------------
struct Netlist
{
    static constexpr int N = 9, OUT = 8, A = 0;
    struct R { int a, b; double r; };
    std::array<R, 11> res;
    // reactivos: 0 C HF boost (w-m1), 1 L HF boost (m1-m2, con R serie), 2 C low atten (n4-out), 3 C HF atten (x-m3), 4 C low boost (n7-gnd)
    std::array<int, 5> pa { 1, 2, 4, 5, 7 }, pb { 2, 3, 8, 6, -1 };
    double cHB = 0, lHB = 0, rL = 0, cLA = 0, cHA = 0, cLB = 0;

    static Netlist make (const Settings& s)
    {
        using namespace part;
        Netlist n;
        const double hb = taperRev (juce::jlimit (0.0f, 10.0f, s.highBoost) / 10.0, 1.5);
        const double ha = taperRev (juce::jlimit (0.0f, 10.0f, s.highAtten) / 10.0, 1.5);
        const double lb = taperLog (juce::jlimit (0.0f, 10.0f, s.lowBoost) / 10.0, 2.5);
        const double la = taperLog (juce::jlimit (0.0f, 10.0f, s.lowAtten) / 10.0, 2.5);
        const double bw = juce::jlimit (0.0f, 10.0f, s.bandwidth) / 10.0;
        auto r = [] (double v) { return std::max (v, 0.01); };
        n.res = { R { 0, 1, r ((1.0 - hb) * potHB) },  R { 1, 4, r (hb * potHB) },      // pot HF BOOST
                  R { 3, 4, r (bw * potBW) },                                           // BANDWIDTH
                  R { 4, 8, r (la * potLA) },                                           // LOW ATTEN
                  R { 4, 5, r ((1.0 - ha) * potHA) }, R { 5, 7, r (ha * potHA) },       // pot HF ATTEN
                  R { 6, 7, R2 }, R { 7, 8, R3 },
                  R { 7, -1, r (lb * potLB) },                                          // LOW BOOST
                  R { 8, -1, Rgrid }, R { 0, -1, 1e9 } };
        n.lHB = hfTap (s.highFreq); n.cHB = hfCap (s.highFreq); n.rL = RLperH * n.lHB;
        n.cLA = lowAttenC (s.lowFreq); n.cHA = hfAttenC (s.attFreq); n.cLB = lowBoostC (s.lowFreq);
        return n;
    }
    double cap (int k) const { return k == 0 ? cHB : k == 2 ? cLA : k == 3 ? cHA : cLB; }
};

// respuesta analogica exacta (para pruebas y para el medidor)
inline std::complex<double> analog (const Settings& s, double f)
{
    using C = std::complex<double>;
    const auto n = Netlist::make (s);
    const C jw (0.0, 2.0 * juce::MathConstants<double>::pi * f);
    C Y[Netlist::N][Netlist::N] {}; C b[Netlist::N] {};
    auto stamp = [&Y] (int a, int c, C y) { if (a >= 0) Y[a][a] += y; if (c >= 0) Y[c][c] += y; if (a >= 0 && c >= 0) { Y[a][c] -= y; Y[c][a] -= y; } };
    for (auto& r : n.res) stamp (r.a, r.b, 1.0 / r.r);
    stamp (0, -1, 1.0 / part::Rs); b[0] = 1.0 / part::Rs;
    for (int k : { 0, 2, 3, 4 }) stamp (n.pa[(size_t) k], n.pb[(size_t) k], jw * n.cap (k));
    stamp (2, 3, 1.0 / (jw * n.lHB + n.rL));
    // eliminacion gaussiana con pivoteo
    for (int i = 0; i < Netlist::N; ++i)
    {
        int p = i; for (int j = i + 1; j < Netlist::N; ++j) if (std::abs (Y[j][i]) > std::abs (Y[p][i])) p = j;
        if (p != i) { for (int c = 0; c < Netlist::N; ++c) std::swap (Y[i][c], Y[p][c]); std::swap (b[i], b[p]); }
        for (int j = i + 1; j < Netlist::N; ++j) { const C m = Y[j][i] / Y[i][i]; for (int c = i; c < Netlist::N; ++c) Y[j][c] -= m * Y[i][c]; b[j] -= m * b[i]; }
    }
    C v[Netlist::N];
    for (int i = Netlist::N - 1; i >= 0; --i) { C t = b[i]; for (int c = i + 1; c < Netlist::N; ++c) t -= Y[i][c] * v[c]; v[i] = t / Y[i][i]; }
    return v[Netlist::OUT];
}
inline double flatGain() { static const double g = std::abs (analog (Settings(), 1000.0)); return g; }
// respuesta en dB relativa a "todo en 0" (la valvula recupera la perdida de insercion)
inline float responseDb (const Settings& s, double f) { return (float) (20.0 * std::log10 (std::abs (analog (s, f)) / flatGain() + 1e-15)); }

// ---------------------------------------------------------------------------------------------
// simulacion en tiempo real: MNA trapezoidal. Se resuelve una vez por cambio de perillas y por muestra
// solo se multiplica una matriz 6x6 (salida + 5 tensiones de reactivos contra entrada + 5 historias).
// ---------------------------------------------------------------------------------------------
class PassiveNetwork
{
public:
    void prepare (double sampleRate) { T = 1.0 / sampleRate; reset(); }
    void reset() { v.fill (0.0); i.fill (0.0); }
    void set (const Settings& s)
    {
        const auto n = Netlist::make (s);
        constexpr int N = Netlist::N;
        double Y[N][N] {};
        auto stamp = [&Y] (int a, int c, double y) { if (a >= 0) Y[a][a] += y; if (c >= 0) Y[c][c] += y; if (a >= 0 && c >= 0) { Y[a][c] -= y; Y[c][a] -= y; } };
        for (auto& r : n.res) stamp (r.a, r.b, 1.0 / r.r);
        gs = 1.0 / part::Rs; stamp (0, -1, gs);
        for (int k : { 0, 2, 3, 4 }) { g[(size_t) k] = 2.0 * n.cap (k) / T; stamp (n.pa[(size_t) k], n.pb[(size_t) k], g[(size_t) k]); }
        L = n.lHB; rL = n.rL; g[1] = 1.0 / (rL + 2.0 * L / T); stamp (2, 3, g[1]);
        // inversa por Gauss-Jordan
        double Z[N][N] {};
        for (int r = 0; r < N; ++r) Z[r][r] = 1.0;
        for (int c = 0; c < N; ++c)
        {
            int p = c; for (int r = c + 1; r < N; ++r) if (std::abs (Y[r][c]) > std::abs (Y[p][c])) p = r;
            for (int k = 0; k < N; ++k) { std::swap (Y[c][k], Y[p][k]); std::swap (Z[c][k], Z[p][k]); }
            const double d = 1.0 / Y[c][c];
            for (int k = 0; k < N; ++k) { Y[c][k] *= d; Z[c][k] *= d; }
            for (int r = 0; r < N; ++r) if (r != c && Y[r][c] != 0.0) { const double m = Y[r][c]; for (int k = 0; k < N; ++k) { Y[r][k] -= m * Y[c][k]; Z[r][k] -= m * Z[c][k]; } }
        }
        auto node = [&Z] (int row, int col) { return row < 0 || col < 0 ? 0.0 : Z[row][col]; };
        // filas: tension en un par de nodos (a-b)
        auto vrow = [&] (int a, int b, int col) { return node (a, col) - node (b, col); };
        // columnas: inyeccion de corriente +1 en p, -1 en q
        auto inj = [&] (int a, int b, int p, int q) { return (p >= 0 ? vrow (a, b, p) : 0.0) - (q >= 0 ? vrow (a, b, q) : 0.0); };
        const int ra[6] { Netlist::OUT, 1, 2, 4, 5, 7 }, rb[6] { -1, 2, 3, 8, 6, -1 };
        for (int r = 0; r < 6; ++r)
        {
            M[r][0] = vrow (ra[r], rb[r], 0) * gs;                                      // entrada (fuente Norton)
            for (int k = 0; k < 5; ++k)
            {
                const int p = n.pa[(size_t) k], q = n.pb[(size_t) k];
                // condensador: historia H entra en p y sale de q. inductor: J sale de p y entra en q
                M[r][k + 1] = k == 1 ? -inj (ra[r], rb[r], p, q) : inj (ra[r], rb[r], p, q);
            }
        }
    }
    inline double process (double x) noexcept
    {
        double h[5];
        for (int k : { 0, 2, 3, 4 }) h[k] = g[(size_t) k] * v[(size_t) k] + i[(size_t) k];             // historia trapezoidal del condensador
        h[1] = g[1] * (v[1] - (rL - 2.0 * L / T) * i[1]);                                                // historia del inductor (con R serie)
        double o[6];
        for (int r = 0; r < 6; ++r) o[r] = M[r][0] * x + M[r][1] * h[0] + M[r][2] * h[1] + M[r][3] * h[2] + M[r][4] * h[3] + M[r][5] * h[4];
        for (int k : { 0, 2, 3, 4 }) { v[(size_t) k] = o[k + 1]; i[(size_t) k] = g[(size_t) k] * o[k + 1] - h[k]; }
        v[1] = o[2]; i[1] = g[1] * o[2] + h[1];
        return o[0];
    }
private:
    double T = 1.0 / 192000.0, gs = 0, L = 0.1, rL = 0;
    std::array<double, 5> g {}, v {}, i {};
    double M[6][6] {};
};

// ---------------------------------------------------------------------------------------------
// amplificador de recuperacion: 12AX7 -> 12AU7 en contrafase, realimentacion desde el trafo de salida.
// Calibrado a la especificacion: 0 dBFS = +18 dBu. THD 0.15 % a +10 dBm, salida maxima +21 dBm,
// respuesta 20 Hz-20 kHz +0/-1 dB (SoS: ~1 dB de perdida en el grave). Segun SoS el 2do armonico
// sube con el nivel: domina el par. Trafos con saturacion por flujo (graves fuertes = mas color).
// ---------------------------------------------------------------------------------------------
constexpr float dbuRef = 18.0f;
constexpr float ampScale = 0.70794578f;      // 10^((18-21)/20): v = 1 dentro del modelo = +21 dBu

inline float softClip (float y, float t, float c) noexcept
{
    const float a = std::abs (y); if (a <= t) return y;
    const float w = c - t; return std::copysign (t + w * std::tanh ((a - t) / w), y);
}
class Transformer
{
public:
    void prepare (double sr, float corner, float amount) { a = (float) std::exp (-2.0 * juce::MathConstants<double>::pi * corner / sr); k = amount; flux = 0; }
    inline float process (float x) noexcept { flux = a * flux + (1.0f - a) * x; return x - k * flux * flux * flux; }
private:
    float a = 0.999f, k = 0, flux = 0;
};
class CapHP
{
public:
    void prepare (double sr, double fc) { const double k = std::tan (juce::MathConstants<double>::pi * fc / sr); n = (float) (1.0 / (1.0 + k)); a1 = (float) ((k - 1.0) / (1.0 + k)); x1 = y1 = 0; }
    inline float process (float x) noexcept { const float y = n * (x - x1) - a1 * y1; x1 = x; y1 = y; return y; }
private:
    float n = 1, a1 = 0, x1 = 0, y1 = 0;
};

class Channel
{
public:
    void prepare (double rate)
    {
        sr = rate; net.prepare (rate);
        inT.prepare (rate, 20.0f, 0.4f); outT.prepare (rate, 20.0f, 0.9f);
        cap.prepare (rate, 3.0);
        // ancho de banda del aparato completo: graves -1 dB en 20 Hz (trafos), agudos anchos
        lo1.prepare (rate, 7.0); lo2.prepare (rate, 7.0);
        hi.setLowPass (rate, 45000.0, 0.65); hi.reset();
        makeup = 1.0 / flatGain();
    }
    void set (const Settings& s) { net.set (s); }
    // drive: nivel hacia la valvula (dB), se compensa a la salida
    inline float process (float x, float drive) noexcept
    {
        float u = inT.process (x * ampScale * drive);
        u = (float) (net.process ((double) u) * makeup);
        u = u + 0.0099f * u * u - 0.025f * u * u * u;                     // 12AX7 + 12AU7 con realimentacion
        u = u >= 0.0f ? softClip (u, 0.80f, 1.12f) : softClip (u, 0.77f, 1.09f);
        u = cap.process (u);
        u = outT.process (u);
        u = hi.process (lo2.process (lo1.process (u)));
        return u / (ampScale * drive);
    }
private:
    double sr = 192000.0, makeup = 1.0;
    PassiveNetwork net;
    Transformer inT, outT;
    CapHP cap, lo1, lo2;
    tvc::Biquad hi;
};
} // namespace teq
