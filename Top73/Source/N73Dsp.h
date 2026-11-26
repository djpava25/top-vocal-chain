#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>
#include <complex>
#include <vector>

// TOP 73 (motor 2): canal clase A con transformadores + EQ de 3 bandas + HPF (estilo 1073), en doble precision y a 8x.
//
//  * El EQ NO son filtros "parecidos": se simula el CIRCUITO (analisis nodal modificado + integracion trapezoidal)
//    con los valores de componentes del esquema:
//      - shelving de graves: realimentacion tipo Baxandall alrededor de una etapa inversora (rebote bajo la
//        frecuencia elegida y pequena "muesca" por encima)
//      - shelving de agudos (12 kHz): pendiente muy suave de 1er orden cuyo giro sube cuando se refuerza poco
//        (ajustado a la medicion por impulso de un 1073 real en +6 y a la curva del circuito en +16), con su muesca
//      - medios: inductor + condensador en serie en la red de realimentacion (Q proporcional: mas estrecho
//        cuanto mas se refuerza y cuanto mas alta es la frecuencia)
//      - HPF: red pasiva C-L-C de 3er orden (18 dB/oct) cargada con 5k1, como el original
//  * Preamp: etapas clase A de un transistor con realimentacion (curva exponencial: corte suave abajo,
//    saturacion arriba). Al subir la ganancia de MIC hay menos realimentacion: mas 2do armonico y armonicos altos.
//  * Transformador de salida con entrehierro: saturacion por flujo (histeresis tipo Rayleigh + cubico + par por la
//    corriente continua de la etapa clase A). Solo colorea graves y niveles altos.
namespace n73
{
constexpr double pi = 3.14159265358979323846;

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
inline int realGainDb (int idx) { return isMic (idx) ? micDb (idx) : lineDb (idx); }
// ganancia que "empuja" la etapa (dB respecto a MIC 50 / LINE 0)
inline float driveDb (int idx) { return isMic (idx) ? (float) (micDb (idx) - 50) : (float) lineDb (idx); }

// respuesta medida por impulso de un 1073 vintage a ganancia alta (MIC 55-80): el transformador de entrada
// resuena en graves (~13 Hz) y los agudos se "abren" al subir la ganancia (+5.6 dB en 20 kHz en MIC 80)
struct HiGain { float hpF, hpQ, hsF, hsG, hsQ; };
inline bool hasHiGain (int idx) { return isMic (idx) && micDb (idx) >= 55; }
inline HiGain hiGain (int idx)
{
    static const HiGain t[] { { 13.9f, 0.78f, 16531.0f, 0.35f, 0.50f }, { 13.77f, 0.79f, 13639.0f, 0.08f, 1.34f }, { 12.38f, 0.83f, 10864.0f, 0.73f, 0.76f },
                              { 12.59f, 0.92f, 11279.0f, 3.11f, 0.63f }, { 10.91f, 0.99f, 9166.0f, 4.87f, 0.52f }, { 13.16f, 0.90f, 8272.0f, 6.49f, 0.46f } };
    return t[juce::jlimit (0, 5, (micDb (idx) - 55) / 5)];
}

inline const float* midFreqs() { static const float f[] { 0, 360, 700, 1600, 3200, 4800, 7200 }; return f; }    // 0 = OFF
inline const float* lowFreqs() { static const float f[] { 0, 35, 60, 110, 220 }; return f; }
inline const float* hpfFreqs() { static const float f[] { 0, 50, 80, 160, 300 }; return f; }

struct Settings
{
    int gain = 6, midFreq = 3, lowFreq = 2, hpf = 0;
    float hf = 0, midGain = 0, lowGain = 0;
    bool eq = true, sat3d = true, autoGain = true;
    bool operator== (const Settings& o) const
    { return gain == o.gain && midFreq == o.midFreq && lowFreq == o.lowFreq && hpf == o.hpf && hf == o.hf && midGain == o.midGain && lowGain == o.lowGain && eq == o.eq && sat3d == o.sat3d && autoGain == o.autoGain; }
};

// ---------------------------------------------------------------------------------------------
// biquad en doble precision desde el prototipo analogico (bilineal con pre-warp)
struct Bq
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    inline double process (double x) noexcept { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    void setIdentity() { b0 = 1; b1 = b2 = a1 = a2 = 0; }
    void setAnalog (double B0, double B1, double B2, double A0, double A1, double A2, double fs, double fw)
    {
        const double w = 2 * pi * fw, c = w / std::tan (w / (2 * fs)), c2 = c * c;
        const double n0 = B0 * c2 + B1 * c + B2, n1 = 2 * (B2 - B0 * c2), n2 = B0 * c2 - B1 * c + B2;
        const double d0 = A0 * c2 + A1 * c + A2, d1 = 2 * (A2 - A0 * c2), d2 = A0 * c2 - A1 * c + A2;
        b0 = n0 / d0; b1 = n1 / d0; b2 = n2 / d0; a1 = d1 / d0; a2 = d2 / d0;
    }
    void setHP2 (double fs, double f, double q) { const double w = 2 * pi * f; setAnalog (1, 0, 0, 1, w / q, w * w, fs, f); }
    void setLP2 (double fs, double f, double q) { const double w = 2 * pi * f; setAnalog (0, 0, w * w, 1, w / q, w * w, fs, f); }
    // shelf de agudos RBJ (asi se ajustaron las mediciones por impulso)
    void setHighShelfRbj (double fs, double f, double q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w0 = 2.0 * pi * std::min (f, fs * 0.45) / fs;
        const double c = std::cos (w0), s = std::sin (w0), al = s / (2.0 * q), sA = 2.0 * std::sqrt (A) * al;
        const double a0 = (A + 1) - (A - 1) * c + sA;
        b0 = A * ((A + 1) + (A - 1) * c + sA) / a0; b1 = -2 * A * ((A - 1) + (A + 1) * c) / a0; b2 = A * ((A + 1) + (A - 1) * c - sA) / a0;
        a1 = 2 * ((A - 1) - (A + 1) * c) / a0; a2 = ((A + 1) - (A - 1) * c - sA) / a0;
    }
    void setLowShelfRbj (double fs, double f, double q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w0 = 2.0 * pi * f / fs;
        const double c = std::cos (w0), s = std::sin (w0), al = s / (2.0 * q), sA = 2.0 * std::sqrt (A) * al;
        const double a0 = (A + 1) + (A - 1) * c + sA;
        b0 = A * ((A + 1) - (A - 1) * c + sA) / a0; b1 = 2 * A * ((A - 1) - (A + 1) * c) / a0; b2 = A * ((A + 1) - (A - 1) * c - sA) / a0;
        a1 = -2 * ((A - 1) + (A + 1) * c) / a0; a2 = ((A + 1) + (A - 1) * c - sA) / a0;
    }
    std::complex<double> at (double f, double fs) const
    {
        const double w = 2 * pi * f / fs; const std::complex<double> z1c = std::polar (1.0, -w), z2c = std::polar (1.0, -2 * w);
        return (b0 + b1 * z1c + b2 * z2c) / (1.0 + a1 * z1c + a2 * z2c);
    }
};

// ---------------------------------------------------------------------------------------------
// SIMULADOR DE CIRCUITO LINEAL: resistencias, condensadores, inductores, una fuente de entrada y un amplificador
// operacional ideal (entrada + a masa). Condensadores e inductores por la regla trapezoidal (modelo compania):
//   C: i = G v - J,  G = 2C/T,  J' = 2 G v - J        L: i = G v + J,  G = T/(2L),  J' = 2 G v + J
// Se resuelve una vez por ajuste y queda una forma de estado: y = g u + h.J ;  J' = A J + B u
// ---------------------------------------------------------------------------------------------
struct Netlist
{
    static constexpr int MaxE = 24, MaxN = 14;
    struct E { char k; int a, b; double v; };
    E e[MaxE]; int ne = 0, nodes = 0, in = 0, out = 0, opMinus = -1, opOut = -1;
    void R (int a, int b, double v) { e[ne++] = { 'R', a, b, std::max (v, 1.0e-3) }; }
    void C (int a, int b, double v) { e[ne++] = { 'C', a, b, v }; }
    void L (int a, int b, double v) { e[ne++] = { 'L', a, b, v }; }
};

template <typename T>
inline bool gaussSolve (T* A, int n, T* B, int nr)      // A n x n (filas), B n x nr; resultado en B
{
    for (int c = 0; c < n; ++c)
    {
        int p = c; double best = std::abs (A[c * n + c]);
        for (int r = c + 1; r < n; ++r) if (std::abs (A[r * n + c]) > best) { best = std::abs (A[r * n + c]); p = r; }
        if (best < 1e-300) return false;
        if (p != c) { for (int k = 0; k < n; ++k) std::swap (A[c * n + k], A[p * n + k]); for (int k = 0; k < nr; ++k) std::swap (B[c * nr + k], B[p * nr + k]); }
        const T inv = T (1) / A[c * n + c];
        for (int r = 0; r < n; ++r)
        {
            if (r == c) continue;
            const T f = A[r * n + c] * inv;
            if (f == T (0)) continue;
            for (int k = c; k < n; ++k) A[r * n + k] -= f * A[c * n + k];
            for (int k = 0; k < nr; ++k) B[r * nr + k] -= f * B[c * nr + k];
        }
    }
    for (int r = 0; r < n; ++r) { const T inv = T (1) / A[r * n + r]; for (int k = 0; k < nr; ++k) B[r * nr + k] *= inv; }
    return true;
}

// ganancia compleja del circuito a una frecuencia (para calibrar las perillas)
inline std::complex<double> acGain (const Netlist& nl, double f)
{
    using C = std::complex<double>;
    const int m = nl.nodes + 1 + (nl.opOut >= 0 ? 1 : 0);
    C A[Netlist::MaxN + 2][Netlist::MaxN + 2] {}, B[Netlist::MaxN + 2] {};
    const C s (0.0, 2 * pi * f);
    auto st = [&] (int a, int b, C y) { if (a >= 0) A[a][a] += y; if (b >= 0) A[b][b] += y; if (a >= 0 && b >= 0) { A[a][b] -= y; A[b][a] -= y; } };
    for (int i = 0; i < nl.ne; ++i)
    {
        const auto& el = nl.e[i];
        st (el.a, el.b, el.k == 'R' ? C (1.0 / el.v) : el.k == 'C' ? s * el.v : 1.0 / (s * el.v));
    }
    const int src = nl.nodes; A[nl.in][src] += 1.0; A[src][nl.in] = 1.0; B[src] = 1.0;
    if (nl.opOut >= 0) { const int r = nl.nodes + 1; A[nl.opOut][r] += 1.0; A[r][nl.opMinus] = -1.0; }
    C flat[(Netlist::MaxN + 2) * (Netlist::MaxN + 2)];
    for (int r = 0; r < m; ++r) for (int c = 0; c < m; ++c) flat[r * m + c] = A[r][c];
    C rhs[Netlist::MaxN + 2]; for (int r = 0; r < m; ++r) rhs[r] = B[r];
    if (! gaussSolve (flat, m, rhs, 1)) return 1.0;
    return rhs[nl.out];
}

class LinearStage
{
public:
    static constexpr int MaxR = 8;
    void build (const Netlist& nl, double fs)
    {
        const double T = 1.0 / fs;
        const int m = nl.nodes + 1 + (nl.opOut >= 0 ? 1 : 0);
        nr = 0;
        int ra[MaxR], rb[MaxR]; double S[MaxR], Rd[MaxR], sg[MaxR];
        double M[(Netlist::MaxN + 2) * (Netlist::MaxN + 2)] {};
        auto st = [&] (int a, int b, double y) { if (a >= 0) M[a * m + a] += y; if (b >= 0) M[b * m + b] += y; if (a >= 0 && b >= 0) { M[a * m + b] -= y; M[b * m + a] -= y; } };
        for (int i = 0; i < nl.ne; ++i)
        {
            const auto& el = nl.e[i];
            if (el.k == 'R') { st (el.a, el.b, 1.0 / el.v); continue; }
            const double G = el.k == 'C' ? 2.0 * el.v / T : T / (2.0 * el.v);
            st (el.a, el.b, G);
            ra[nr] = el.a; rb[nr] = el.b; S[nr] = 2.0 * G; Rd[nr] = el.k == 'C' ? -1.0 : 1.0; sg[nr] = el.k == 'C' ? 1.0 : -1.0; ++nr;
        }
        const int src = nl.nodes; M[nl.in * m + src] += 1.0; M[src * m + nl.in] = 1.0;
        if (nl.opOut >= 0) { const int r = nl.nodes + 1; M[nl.opOut * m + r] += 1.0; M[r * m + nl.opMinus] = -1.0; }
        const int nc = nr + 1;                                              // columna 0 = entrada, 1.. = fuentes J
        double Bm[(Netlist::MaxN + 2) * (MaxR + 1)] {};
        Bm[src * nc + 0] = 1.0;
        for (int k = 0; k < nr; ++k) { if (ra[k] >= 0) Bm[ra[k] * nc + 1 + k] += sg[k]; if (rb[k] >= 0) Bm[rb[k] * nc + 1 + k] -= sg[k]; }
        if (! gaussSolve (M, m, Bm, nc)) { nr = 0; g = 1; return; }
        auto volt = [&] (int node, int col) { return node >= 0 ? Bm[node * nc + col] : 0.0; };
        g = volt (nl.out, 0);
        for (int j = 0; j < nr; ++j) h[j] = volt (nl.out, 1 + j);
        for (int k = 0; k < nr; ++k)
        {
            B[k] = S[k] * (volt (ra[k], 0) - volt (rb[k], 0));
            for (int j = 0; j < nr; ++j) A[k][j] = S[k] * (volt (ra[k], 1 + j) - volt (rb[k], 1 + j)) + (k == j ? Rd[k] : 0.0);
        }
    }
    void reset() { for (auto& j : J) j = 0; }
    inline double process (double u) noexcept
    {
        double y = g * u, nj[MaxR];
        for (int j = 0; j < nr; ++j) y += h[j] * J[j];
        for (int k = 0; k < nr; ++k) { double a = B[k] * u; for (int j = 0; j < nr; ++j) a += A[k][j] * J[j]; nj[k] = a; }
        for (int k = 0; k < nr; ++k) J[k] = nj[k];
        return y;
    }
private:
    int nr = 0;
    double A[MaxR][MaxR] {}, B[MaxR] {}, h[MaxR] {}, g = 1, J[MaxR] {};
};

// ---------------------------------------------------------------------------------------------
// LOS TRES CIRCUITOS DEL EQ (valores del esquema). Cada etapa activa invierte la fase: dos inversiones = fase normal.
// ---------------------------------------------------------------------------------------------
// shelving: a = posicion del pot de graves (0 = refuerzo maximo), b = pot de agudos (0 = refuerzo maximo)
inline Netlist shelfNet (int lowIdx, double a, double b)
{
    static const double cb[] { 100e-9, 200e-9, 100e-9, 47e-9, 15e-9 };    // OFF (pot al centro), 35, 60, 110, 220 Hz
    enum { IN, N1, N2, TW, N3, N5, OUT, B1, BW, B3, CB, VN, NN };
    Netlist n; n.nodes = NN; n.in = IN; n.out = OUT; n.opMinus = VN; n.opOut = OUT;
    const double Rp = 50e3, Rt = 10e3;
    n.R (IN, N1, 620); n.C (N1, N2, 22e-9); n.R (N2, TW, b * Rt); n.R (TW, N3, (1 - b) * Rt); n.C (TW, VN, 15e-9);
    n.C (N3, N5, 22e-9); n.R (N5, OUT, 620);
    n.R (IN, B1, 6800); n.R (B1, BW, a * Rp); n.R (BW, B3, (1 - a) * Rp); n.R (B3, OUT, 6800); n.R (BW, VN, 12e3);
    n.C (BW, CB, 10e-9); n.R (CB, -1, 12e3);
    const double c = cb[juce::jlimit (0, 4, lowIdx)];
    n.C (B1, CB, c); n.C (B3, CB, c);
    n.C (VN, OUT, 100e-12);                                                // compensacion de la etapa (plano +-0.2 dB a 20 kHz)
    return n;
}
// medios: LC en serie (inductor con su resistencia de cobre) desde la tierra virtual al cursor del pot (w = 0: refuerzo)
struct MidLC { double C, L, Rs; };
inline MidLC midLC (int idx)
{
    static const MidLC t[] { { 100e-9, 2.0, 820 }, { 100e-9, 2.0, 820 }, { 47e-9, 1.098, 560 }, { 22e-9, 0.449, 360 },
                             { 12.2e-9, 0.2003, 330 }, { 5.49e-9, 0.2003, 330 }, { 2.44e-9, 0.2003, 330 } };
    return t[juce::jlimit (0, 6, idx)];
}
inline Netlist midNet (int idx, double w)
{
    enum { IN, M1, OUT, VN, P, Q, R, W, NN };
    Netlist n; n.nodes = NN; n.in = IN; n.out = OUT; n.opMinus = VN; n.opOut = OUT;
    const auto lc = midLC (idx);
    n.R (IN, M1, 6800); n.R (M1, OUT, 6800); n.R (M1, VN, 8200); n.R (VN, P, 2700);
    n.C (P, Q, lc.C); n.L (Q, R, lc.L); n.R (R, W, lc.Rs);
    n.R (IN, W, w * 10e3); n.R (W, OUT, (1 - w) * 10e3);
    n.C (VN, OUT, 150e-12);
    return n;
}
// HPF pasivo de 3er orden: 1k2 de fuente, C serie - L a masa (con su resistencia) - C serie, carga de 5k1
inline Netlist hpfNet (int idx)
{
    struct H { double C, L, Rs; };
    static const H t[] { { 1e-6, 10.0, 1000 }, { 1e-6, 10.0, 1000 }, { 0.47e-6, 6.99, 820 }, { 0.22e-6, 3.0, 470 }, { 0.122e-6, 1.3, 300 } };
    const auto v = t[juce::jlimit (0, 4, idx)];
    enum { IN, X, J, Y, K, NN };
    Netlist n; n.nodes = NN; n.in = IN; n.out = Y;
    n.R (IN, X, 1200); n.C (X, J, v.C); n.C (J, Y, v.C); n.L (J, K, v.L); n.R (K, -1, v.Rs); n.R (Y, -1, 5100);
    return n;
}
constexpr double hpfMakeup = 6300.0 / 5100.0;                              // la carga baja 1.8 dB en la banda de paso

// perillas en dB -> posicion del potenciometro
inline double lowPot (float db)                                            // ganancia en continua del Baxandall: (R4+(1-a)Rp)/(R3+a Rp)
{
    const double g = std::pow (10.0, db / 20.0), Rp = 50e3, R = 6800;
    return juce::jlimit (0.0, 1.0, (R + Rp - g * R) / (Rp * (1 + g)));
}
template <typename F> inline double bisect (F gainDbAt, double target)     // la ganancia baja al mover el pot de 0 a 1
{
    double lo = 0, hi = 1;
    for (int i = 0; i < 34; ++i) { const double m = 0.5 * (lo + hi); (gainDbAt (m) > target ? lo : hi) = m; }
    return 0.5 * (lo + hi);
}
// agudos 12 kHz: shelf de 1er orden H = (1 + s/wz) / (1 + s/wp), wp = wz * G.
//   refuerzo maximo (+18.6 dB): giro en 1.06 kHz (igual a la curva del circuito: +9 dB en 3 kHz, +16 en 10 kHz)
//   perilla +6 (+7.7 dB): giro en 8.8 kHz (medicion por impulso del 1073 real: +0.4 en 3k, +2.9 en 10k, +5.5 en 20k)
//   corte = espejo del refuerzo; muesca/rebote en 330 Hz que crece con la cantidad (1.4 dB al maximo)
inline void highShelf (float knob, double fs, Bq& sh, Bq& dv)
{
    const double g = knob * (1.36 - 0.0125 * std::abs (knob));
    if (std::abs (g) < 0.01) { sh.setIdentity(); dv.setIdentity(); return; }
    const double ag = std::abs (g), G = std::pow (10.0, ag / 20.0);
    const double fz = 1060.0 * std::pow (2.0, (18.6 - ag) * 0.28), fp = fz * G, wz = 2 * pi * fz, wp = 2 * pi * fp;
    if (g > 0) sh.setAnalog (0, 1 / wz, 1, 0, 1 / wp, 1, fs, 10000.0);
    else       sh.setAnalog (0, 1 / wp, 1, 0, 1 / wz, 1, fs, 10000.0);
    const double d = (g > 0 ? -1.4 : 1.4) * (ag / 18.6) * (ag / 18.6), A = std::pow (10.0, d / 40.0), w0 = 2 * pi * 330.0, q = 0.6;
    dv.setAnalog (1, w0 * A / q, w0 * w0, 1, w0 / (A * q), w0 * w0, fs, 330.0);
}
inline double midPot (float db, int idx)                                   // ganancia en la frecuencia central = valor de la perilla
{
    if (idx <= 0 || std::abs (db) < 0.01f) return 0.5;
    const double f0 = midFreqs()[idx];
    return bisect ([&] (double w) { return 20 * std::log10 (std::abs (acGain (midNet (idx, w), f0))); }, db);
}
struct EqPots { double a = 0.5, b = 0.5, w = 0.5; };
inline EqPots pots (const Settings& s)
{
    EqPots p;
    p.a = s.lowFreq > 0 ? lowPot (s.lowGain) : 0.5;
    p.b = 0.5;                                                             // los agudos van aparte (highShelf)
    p.w = s.midFreq > 0 ? midPot (s.midGain, s.midFreq) : 0.5;
    return p;
}

// ---------------------------------------------------------------------------------------------
// respuesta lineal completa (para pruebas e interfaz): transformador de entrada + respuesta medida + EQ + HPF
// ---------------------------------------------------------------------------------------------
inline void inputFilters (int gainIdx, double fs, Bq& hp, Bq& hs, Bq& lp)
{
    if (hasHiGain (gainIdx)) { const auto c = hiGain (gainIdx); hp.setHP2 (fs, c.hpF, c.hpQ); if (c.hsG > 0.05f) hs.setHighShelfRbj (fs, c.hsF, c.hsQ, c.hsG); else hs.setIdentity(); }
    else { hp.setHP2 (fs, 7.5, 0.8); hs.setIdentity(); }                   // transformador: -3 dB ~7 Hz con un leve rebote en ~16 Hz
    lp.setLP2 (fs, 40000.0, 0.72);                                         // spec: -3 dB a 40 kHz (EQ out)
}
inline float responseDb (const Settings& s, double f, double fs = 384000.0)
{
    Bq hp, hs, lp; inputFilters (s.gain, fs, hp, hs, lp);
    std::complex<double> h = hp.at (f, fs) * hs.at (f, fs) * lp.at (f, fs);
    if (s.eq)
    {
        const auto p = pots (s);
        Bq sh, dv; highShelf (s.hf, fs, sh, dv);
        h *= acGain (shelfNet (s.lowFreq, p.a, p.b), f) * sh.at (f, fs) * dv.at (f, fs) * acGain (midNet (std::max (1, s.midFreq), p.w), f);
        if (s.hpf > 0) h *= acGain (hpfNet (s.hpf), f) * hpfMakeup;
    }
    return (float) (20.0 * std::log10 (std::abs (h) + 1e-12));
}

// ---------------------------------------------------------------------------------------------
// ETAPA CLASE A con realimentacion (curva estatica de lazo cerrado, resuelta una vez y guardada en tabla):
//   lazo abierto: transistor y = c (e^(e/c) - 1) (corte suave en -c), saturacion suave en +s
//   lazo cerrado: y = F(T (x (1+T)/T - y)), T = ganancia de lazo. Mucha realimentacion = limpio hasta cerca del
//   recorte; poca realimentacion (MIC alta) = mas 2do armonico y armonicos altos desde antes.
// Calibrado (0 dBFS = +24 dBu, v = 1 = +26 dBu): con T = 300, dos etapas: 0.05% a +20 dBu con H2 ~ H3 (spec 0.07%),
// salida maxima ~+26.5 dBu, -85 dB a nivel normal.
// ---------------------------------------------------------------------------------------------
struct StageChar { double T = 300, c = 3.5, s = 1.12, p = 1.6; };
inline double classAOpen (const StageChar& k, double e)
{
    const double y = k.c * (std::exp (juce::jlimit (-60.0, 60.0, e / k.c)) - 1.0);
    return y / std::pow (1.0 + std::pow (std::abs (y / k.s), k.p), 1.0 / k.p);
}
inline double classAClosed (const StageChar& k, double x)
{
    const double xs = x * (1.0 + k.T) / k.T;
    double lo = -k.c * 0.99999, hi = k.s;
    for (int i = 0; i < 56; ++i)
    {
        const double m = 0.5 * (lo + hi);
        (classAOpen (k, k.T * (xs - m)) - m > 0 ? lo : hi) = m;
    }
    return 0.5 * (lo + hi);
}
inline StageChar micAmp (int gainIdx)
{
    StageChar k;
    if (isMic (gainIdx))
    {
        const int g = micDb (gainIdx);
        k.T = g > 50 ? 300.0 * std::pow (10.0, -(g - 50) * 0.8 / 20.0)      // MIC 55-80: entra la 2da etapa, menos realimentacion
                     : 300.0 * std::pow (10.0, (50 - g) * 0.4 / 20.0);       // MIC 20-45: mas realimentacion
        if (g > 50) k.c = 3.5 - 0.06 * (g - 50);                             // la 2da etapa trabaja mas cerca del corte: mas 2do armonico
    }
    else k.T = 600.0;                                                      // LINE
    return k;
}
inline StageChar outAmp() { return {}; }

// tabla de la antiderivada (ADAA de 1er orden): el recorte se integra entre muestras -> sin aliasing
class CurveTable
{
public:
    static constexpr double X = 14.0, h = 1.0 / 128.0, invH = 128.0;
    void build (const StageChar& k)
    {
        n = (int) std::lround (2.0 * X / h);
        F.assign ((size_t) n + 1, 0.0); f.assign ((size_t) n + 1, 0.0);
        f[0] = classAClosed (k, -X);
        double acc = 0.0;
        for (int i = 1; i <= n; ++i)
        {
            const double x0 = -X + (i - 1) * h;
            const double fm = classAClosed (k, x0 + 0.5 * h), f1 = classAClosed (k, x0 + h);
            acc += h / 6.0 * (f[(size_t) i - 1] + 4.0 * fm + f1);
            F[(size_t) i] = acc; f[(size_t) i] = f1;
        }
        F0 = integral (0.0);
    }
    inline double integral (double x) const noexcept
    {
        if (x <= -X) return F[0] + f[0] * (x + X);
        if (x >= X) return F[(size_t) n] + f[(size_t) n] * (x - X);
        const double pos = (x + X) * invH;
        int i = (int) pos; if (i >= n) i = n - 1;
        const double t = pos - i, t2 = t * t, t3 = t2 * t;
        const size_t q = (size_t) i;
        return (2 * t3 - 3 * t2 + 1) * F[q] + (t3 - 2 * t2 + t) * h * f[q] + (-2 * t3 + 3 * t2) * F[q + 1] + (t3 - t2) * h * f[q + 1];
    }
    inline double curve (double x) const noexcept
    {
        if (x <= -X) return f[0];
        if (x >= X) return f[(size_t) n];
        const double pos = (x + X) * invH; int i = (int) pos; if (i >= n) i = n - 1;
        const double t = pos - i; return f[(size_t) i] + t * (f[(size_t) i + 1] - f[(size_t) i]);
    }
    double F0 = 0;
private:
    int n = 0;
    std::vector<double> F, f;
};
// tablas compartidas: 13 posiciones de MIC, LINE y la etapa de salida (se construyen una sola vez)
inline const CurveTable& micTable (int gainIdx)
{
    static const std::array<CurveTable, 14> t = [] { std::array<CurveTable, 14> a; for (int i = 0; i < 14; ++i) a[(size_t) i].build (micAmp (i)); return a; }();
    return t[(size_t) (isMic (gainIdx) ? gainIdx : 13)];
}
inline const CurveTable& outTable() { static const CurveTable t = [] { CurveTable c; c.build (outAmp()); return c; }(); return t; }

class AdaaStage
{
public:
    void set (const CurveTable* t) { tab = t; F1 = tab->integral (x1); }
    void reset() { x1 = 0; F1 = tab != nullptr ? tab->integral (0.0) : 0.0; }
    inline double process (double u) noexcept
    {
        const double Fu = tab->integral (u), dx = u - x1;
        const double y = std::abs (dx) > 1.0e-5 ? (Fu - F1) / dx : tab->curve (0.5 * (u + x1));
        x1 = u; F1 = Fu; return y;
    }
private:
    const CurveTable* tab = nullptr;
    double x1 = 0, F1 = 0;
};
// el ADAA promedia media muestra por etapa: este FIR de fase lineal lo devuelve a plano (a = etapas / 8)
class AdaaEq
{
public:
    void reset() { z1 = z2 = 0; }
    inline double process (double x, double a) noexcept { const double y = -a * x + (1 + 2 * a) * z1 - a * z2; z2 = z1; z1 = x; return y; }
private:
    double z1 = 0, z2 = 0;
};

// transformador: el flujo es la integral de la tension (con la inductancia del primario, ~5 Hz), asi la distorsion
// sube a medida que baja la frecuencia. Corriente de magnetizacion = histeresis de Rayleigh (proporcional al nivel,
// se nota tambien a nivel normal) + saturacion cubica + termino par (corriente continua de la etapa clase A en el
// transformador con entrehierro).
class Transformer
{
public:
    void prepare (double fs, double kRayleigh, double kCubic, double kEven)
    {
        a = std::exp (-2.0 * pi * 5.0 / fs); kr = kRayleigh; kc = kCubic; ke = kEven; flux = 0; dc.setHP2 (fs, 2.0, 0.6); dc.reset();
    }
    void reset() { flux = 0; dc.reset(); }
    inline double process (double v) noexcept
    {
        flux = a * flux + (1.0 - a) * v;
        const double i = kr * flux * std::abs (flux) + kc * flux * flux * flux + ke * flux * flux;
        return dc.process (v - i);
    }
private:
    double a = 0.9999, kr = 0, kc = 0, ke = 0, flux = 0;
    Bq dc;
};

// ---------------------------------------------------------------------------------------------
// SATURACION 3D (tecla "3D", encendida por defecto). El circuito original es muy limpio hasta MIC 60 y despues
// recorta de golpe; esta etapa, delante del amplificador de mic, hace que la perilla roja agregue armonicos
// de forma progresiva (no solo volumen). Tres dimensiones:
//   1) armonicos: valvula asimetrica con ganancia unitaria -> el 2do armonico crece con cada paso de la perilla
//      y desde ~MIC 65 aparece el 3ro; arriba, un techo suave redondea los picos antes de que la etapa clase A
//      los rompa. La asimetria "respira" con la envolvente: las partes fuertes tienen mas armonicos pares.
//   2) frecuencia: los graves entran +3 dB y los agudos -5 dB a la saturacion y se devuelven exactos despues:
//      peso en los graves y agudos sin aspereza (la respuesta lineal queda igual).
//   3) estereo: el canal derecho tiene otras tolerancias (asimetria, empuje, techo, frecuencias de enfasis), asi
//      izquierda y derecha no saturan identico y el sonido gana profundidad; lo limpio es igual en los dos.
// ---------------------------------------------------------------------------------------------
class Sat3D
{
public:
    void prepare (double fs, int channel)
    {
        const double t = channel == 0 ? 0.0 : 1.0;
        const double fl = 180.0 * (1.0 + 0.06 * t), fh = 5000.0 * (1.0 - 0.06 * t);
        loIn.setLowShelfRbj (fs, fl, 0.6, 3.0);   loOut.setLowShelfRbj (fs, fl, 0.6, -3.0);
        hiIn.setHighShelfRbj (fs, fh, 0.6, -5.0); hiOut.setHighShelfRbj (fs, fh, 0.6, 5.0);
        k = 0.40 * (1.0 + 0.08 * t); b0 = 0.10 * (1.0 - 0.15 * t);
        knee = 0.80 * (1.0 - 0.03 * t); ceil = 1.05 * (1.0 - 0.03 * t);
        envC = 1.0 - std::exp (-1.0 / (0.03 * fs));
        dc.setHP2 (fs, 2.0, 0.5);
        reset();
    }
    void reset() { loIn.reset(); loOut.reset(); hiIn.reset(); hiOut.reset(); dc.reset(); env = 0; }
    void setPush (double p) noexcept { push = p; }                     // mas densidad de armonicos (MIC 70-80)
    inline double process (double x) noexcept
    {
        double v = hiIn.process (loIn.process (x));
        env += envC * (std::abs (v) - env);
        const double b = b0 * (1.0 + 0.8 * std::min (1.0, env / knee));
        const double tb = std::tanh (b);
        const double kk = k * push;
        v = (std::tanh (kk * v + b) - tb) / (kk * (1.0 - tb * tb));    // ganancia 1 en senal chica, armonicos al crecer
        const double av = std::abs (v);
        if (av > knee) v = std::copysign (knee + (ceil - knee) * std::tanh ((av - knee) / (ceil - knee)), v);   // techo suave
        v = dc.process (v);
        return hiOut.process (loOut.process (v));
    }
private:
    Bq loIn, loOut, hiIn, hiOut, dc;
    double k = 0.4, b0 = 0.1, knee = 0.8, ceil = 1.05, env = 0, envC = 0.001, push = 1.0;
};

// ---------------------------------------------------------------------------------------------
// un canal completo a 8x: trafo de entrada -> amplificador de mic -> EQ (circuito) -> HPF -> etapa de salida -> LO1166
// ---------------------------------------------------------------------------------------------
class Channel
{
public:
    static constexpr double inScale = 0.7943282347242815;                 // +24 dBu (0 dBFS) -> +26 dBu = 1.0
    void prepare (double rate, int channel = 0)
    {
        fs = rate;
        sat.prepare (fs, channel);
        mkC = 1.0 - std::exp (-1.0 / (0.4 * fs)); gC = 1.0 - std::exp (-1.0 / (0.3 * fs));
        inT.prepare (fs, 0.14, 2.4, 0.07);                                 // trafo de linea (31267): solo pesa en LINE
        outT.prepare (fs, 0.17, 3.4, 0.08);                                // LO1166 con entrehierro
        cap1.setHP2 (fs, 1.5, 0.5); cap2.setHP2 (fs, 1.5, 0.5);
        mic.set (&micTable (6)); amp.set (&outTable());
        reset();
    }
    void reset()
    {
        inT.reset(); outT.reset(); hp.reset(); hs.reset(); lp.reset(); cap1.reset(); cap2.reset(); hiSh.reset(); hiDv.reset();
        shelf.reset(); mid.reset(); hpf.reset(); mic.reset(); amp.reset(); adaa.reset(); sat.reset(); msIn = msOut = 0; mk = 1;
    }
    void set (const Settings& s, const EqPots& p, double driveGain)
    {
        inputFilters (s.gain, fs, hp, hs, lp);
        mic.set (&micTable (s.gain));
        // con 3D, de MIC 65 para arriba la ganancia extra no se va toda en nivel (que rompe la etapa clase A):
        // el nivel sube 0.2 dB por dB y la densidad de armonicos de la etapa 3D sube 0.8 dB por dB. El volumen
        // final es el mismo (se devuelve a la salida), asi AUTO y la ganancia de la perilla no cambian.
        const double over = s.sat3d ? std::max (0.0, (double) driveDb (s.gain) - 15.0) : 0.0;
        trim = std::pow (10.0, -over * 0.8 / 20.0);
        sat.setPush (1.0 / trim);
        drive = driveGain * trim;
        primary = std::pow (10.0, -realGainDb (s.gain) / 20.0) / std::max (1e-9, driveGain * trim);   // nivel en el conector
        eqOn = s.eq; hpfOn = s.eq && s.hpf > 0; sat3d = s.sat3d; autoMk = s.sat3d && s.autoGain;
        if (! autoMk) mk = 1;
        if (eqOn)
        {
            highShelf (s.hf, fs, hiSh, hiDv);
            shelf.build (shelfNet (s.lowFreq, p.a, p.b), fs);
            mid.build (midNet (std::max (1, s.midFreq), p.w), fs);
            if (hpfOn) hpf.build (hpfNet (s.hpf), fs);
        }
    }
    inline double process (double x) noexcept
    {
        double u = x * inScale * drive;
        // trafo de entrada: satura segun el nivel en el conector (MIC: decenas de dB por debajo -> casi lineal)
        u = inT.process (u * primary) / primary;
        u = lp.process (hs.process (hp.process (u)));
        if (sat3d)
        {
            if (autoMk) msIn += mkC * (u * u - msIn);
            u = sat.process (u);                                           // saturacion 3D (perilla roja = armonicos)
        }
        u = cap1.process (mic.process (u));
        if (autoMk)
        {
            // AUTO con 3D: la saturacion aplasta los picos y el volumen baja; se mide lo que se pierde en
            // esta parte (lenta, 0.4 s) y se devuelve al final, asi la perilla cambia el color y no el volumen
            msOut += mkC * (u * u - msOut);
            if (msIn > 1.0e-9) mk += gC * (juce::jlimit (1.0, 2.5, std::sqrt (msIn / std::max (msOut, 1.0e-12))) - mk);
        }                                // amplificador de mic clase A + acople
        if (eqOn)
        {
            u = mid.process (hiDv.process (hiSh.process (shelf.process (u))));
            if (hpfOn) u = hpf.process (u) * hpfMakeup;
        }
        u = cap2.process (amp.process (u));                                // etapa de salida clase A (2N3055)
        u = outT.process (adaa.process (u, 0.25));                         // LO1166 + compensacion del ADAA (2 etapas)
        return u * mk / (inScale * trim);
    }
private:
    double fs = 384000.0, drive = 1, primary = 1, trim = 1;
    bool eqOn = true, hpfOn = false, sat3d = true, autoMk = true;
    double msIn = 0, msOut = 0, mk = 1, mkC = 1.0e-5, gC = 1.0e-5;
    Sat3D sat;
    Transformer inT, outT;
    Bq hp, hs, lp, cap1, cap2, hiSh, hiDv;
    LinearStage shelf, mid, hpf;
    AdaaStage mic, amp;
    AdaaEq adaa;
};
} // namespace n73
