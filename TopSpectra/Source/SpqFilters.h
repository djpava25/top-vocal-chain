#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>

// TOP SPECTRA EQ: diseno de filtros.
// Cada banda se describe primero como su circuito ANALOGICO (secciones de 1er y 2do orden en s). De ahi:
//   * modo Zero Latency: cada seccion se pasa a digital con la topologia que mejor encaja en su frecuencia
//     (bilineal con pre-warp en graves y medios; "matched" de Vicanek en agudos: polos por invarianza al impulso y
//     ceros ajustados a la magnitud analogica en DC, en la frecuencia de la seccion y en Nyquist). Resultado: la
//     curva digital calca la analogica hasta Nyquist, sin el "cramping" de los EQ digitales comunes.
//   * modo Linear Phase: se usa la respuesta analogica exacta para disenar el FIR.
// Pendientes de cortes y estantes hasta 96 dB/oct (Butterworth / estantes de orden alto de Holters-Zolzer).
namespace spq
{
constexpr double pi = 3.14159265358979323846;
using cd = std::complex<double>;

enum Type { Bell, LowShelf, HighShelf, LowCut, HighCut, Notch, BandPass, TiltShelf, FlatTilt, AllPass, NumTypes };
enum Place { Stereo, LeftCh, RightCh, MidCh, SideCh };
inline constexpr int numSlopes = 10;
inline constexpr int slopeDb[numSlopes] { 6, 12, 18, 24, 30, 36, 48, 60, 72, 96 };
inline constexpr int maxSec = 16;

struct Band
{
    bool on = false; int type = Bell; double f = 1000, g = 0, q = 0.707; int slope = 1; int place = Stereo;
    bool dyn = false; double thr = -24, range = -6, att = 10, rel = 120; bool below = false; bool spectral = false;
};

// seccion analogica (b2 s^2 + b1 s + b0) / (a2 s^2 + a1 s + a0)
struct ASec { double b2 = 0, b1 = 0, b0 = 1, a2 = 0, a1 = 0, a0 = 1; int kind = 0; double w0 = 0; };   // kind: 0 normal, 1 pasatodos, 2 notch

inline cd evalA (const ASec& s, double W) noexcept
{
    const cd n (s.b0 - s.b2 * W * W, s.b1 * W), d (s.a0 - s.a2 * W * W, s.a1 * W);
    return n / d;
}

// polos Butterworth: amortiguamientos d_k = 2 sin((2k+1) pi / 2N) de cada par
inline double bwDamp (int N, int k) { return 2.0 * std::sin ((2.0 * k + 1.0) * pi / (2.0 * N)); }

// ---------------------------------------------------------------------------------------------
// banda -> secciones analogicas (g en dB ya incluye la parte dinamica)
inline int analogSections (const Band& b, double gDb, ASec* o)
{
    int n = 0;
    const double w = 2.0 * pi * b.f, q = std::clamp (b.q, 0.025, 40.0);
    const int N = std::max (1, slopeDb[std::clamp (b.slope, 0, numSlopes - 1)] / 6);
    auto push = [&] (ASec s) { if (n < maxSec) o[n++] = s; };
    switch (b.type)
    {
        case Bell:
        {
            const double A = std::pow (10.0, gDb / 40.0);
            push ({ 1, A * w / q, w * w, 1, w / (A * q), w * w, 0, w });
            break;
        }
        case LowShelf: case HighShelf: case TiltShelf:
        {
            const double G = std::pow (10.0, gDb / 20.0);
            const bool hs = b.type != LowShelf;
            const double e = std::pow (G, 1.0 / (2.0 * N));
            const double wz = hs ? w / e : w * e, wp = hs ? w * e : w / e;
            double c = hs ? G : 1.0;                                            // estante alto: el nivel de agudos va al frente
            if (b.type == TiltShelf) c /= std::sqrt (G);                         // tilt: -g/2 abajo, +g/2 arriba
            // H = c * prod (s^2 + d wz s + wz^2) / (s^2 + d wp s + wp^2): DC y agudos quedan exactos por construccion
            bool first = true;
            for (int k = 0; k < N / 2; ++k)
            {
                const double d = (N == 2) ? 1.0 / q : bwDamp (N, k), cc = first ? c : 1.0; first = false;
                push ({ cc, cc * d * wz, cc * wz * wz, 1, d * wp, wp * wp, 0, wp });
            }
            if (N % 2 == 1) { const double cc = first ? c : 1.0; push ({ 0, cc, cc * wz, 0, 1, wp, 0, wp }); }
            break;
        }
        case LowCut: case HighCut:
        {
            const bool hp = b.type == LowCut;
            int hiQ = -1;
            for (int k = 0; k < N / 2; ++k)
            {
                double d = bwDamp (N, k);
                if (k == N / 2 - 1) { d *= 0.70710678 / q; hiQ = k; }   // la seccion mas aguda lleva la resonancia
                ASec s = hp ? ASec { 1, 0, 0, 1, d * w, w * w, 0, w } : ASec { 0, 0, w * w, 1, d * w, w * w, 0, w };
                push (s);
            }
            (void) hiQ;
            if (N % 2 == 1) push (hp ? ASec { 0, 1, 0, 0, 1, w, 0, w } : ASec { 0, 0, w, 0, 1, w, 0, w });
            break;
        }
        case Notch: push ({ 1, 0, w * w, 1, w / q, w * w, 2, w }); break;
        case BandPass:
        {
            const int m = std::max (1, N / 2);
            const double qm = q / std::sqrt (std::pow (2.0, 1.0 / m) - 1.0);    // el ancho a -3 dB se conserva en cascada
            for (int k = 0; k < m; ++k) push ({ 0, w / qm, 0, 1, w / qm, w * w, 0, w });
            break;
        }
        case AllPass: push ({ 1, -w / q, w * w, 1, w / q, w * w, 1, w }); break;
        case FlatTilt:
        {
            // inclinacion constante (dB/oct) de 10 Hz a 40 kHz con pares polo-cero cada octava; pivote = f
            const double sig = std::clamp (gDb / 9.97, -5.5, 5.5);              // g = inclinacion total de 20 Hz a 20 kHz
            const double r = std::pow (2.0, std::abs (sig) / 6.0);
            for (int i = 0; i < 13; ++i)
            {
                const double fi = 2.0 * pi * 10.0 * std::pow (2.0, i), wz = sig >= 0 ? fi : fi * r, wp = sig >= 0 ? fi * r : fi;
                push ({ 0, wp / wz, wp, 0, 1, wp, 0, wp });                     // (s + wz) * (wp/wz) / (s + wp): DC = 1
            }
            cd h (1, 0); for (int i = 0; i < n; ++i) h *= evalA (o[i], w);
            const double nm = 1.0 / std::max (1e-9, std::abs (h));
            o[0].b1 *= nm; o[0].b0 *= nm;
            break;
        }
        default: break;
    }
    return n;
}

inline double analogMag (const ASec* s, int n, double hz)
{
    cd h (1, 0); const double W = 2.0 * pi * hz;
    for (int i = 0; i < n; ++i) h *= evalA (s[i], W);
    return std::abs (h);
}

// ---------------------------------------------------------------------------------------------
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    inline cd resp (double wT) const noexcept
    {
        const cd z1 = std::polar (1.0, -wT), z2 = z1 * z1;
        return (b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2);
    }
};

// bilineal con pre-warp en wp (rad/s)
inline Biquad blt (const ASec& s, double fs, double wp)
{
    const double T = 1.0 / fs, wpc = std::min (wp, 0.98 * pi * fs);
    const double c = wpc / std::tan (wpc * T * 0.5), c2 = c * c;
    const double n0 = s.b2 * c2 + s.b1 * c + s.b0, n1 = 2.0 * (s.b0 - s.b2 * c2), n2 = s.b2 * c2 - s.b1 * c + s.b0;
    const double d0 = s.a2 * c2 + s.a1 * c + s.a0, d1 = 2.0 * (s.a0 - s.a2 * c2), d2 = s.a2 * c2 - s.a1 * c + s.a0;
    return { n0 / d0, n1 / d0, n2 / d0, d1 / d0, d2 / d0 };
}

// "matched": polos por invarianza al impulso, ceros por ajuste de magnitud (Vicanek 2016)
inline Biquad matched (const ASec& s, double fs)
{
    const double T = 1.0 / fs, nyq = pi * fs;
    Biquad q;
    if (s.a2 == 0.0)                                                          // 1er orden
    {
        const double wp = s.a0 / s.a1;
        q.a1 = -std::exp (-wp * T); q.a2 = 0;
        const double hdc = std::abs (evalA (s, 0.0)), hny = std::abs (evalA (s, nyq));
        q.b0 = 0.5 * ((1 + q.a1) * hdc + (1 - q.a1) * hny); q.b1 = 0.5 * ((1 + q.a1) * hdc - (1 - q.a1) * hny); q.b2 = 0;
        return q;
    }
    const double w0 = std::sqrt (s.a0 / s.a2), zeta = s.a1 / (2.0 * s.a2 * w0);
    const double w0c = std::min (w0, 0.97 * nyq);
    if (zeta < 1.0)
    {
        const double r = std::exp (-zeta * w0c * T);
        q.a1 = -2.0 * r * std::cos (w0c * std::sqrt (1.0 - zeta * zeta) * T); q.a2 = r * r;
    }
    else
    {
        const double sq = std::sqrt (zeta * zeta - 1.0), p1 = w0c * (-zeta + sq), p2 = w0c * (-zeta - sq);
        q.a1 = -(std::exp (p1 * T) + std::exp (p2 * T)); q.a2 = std::exp ((p1 + p2) * T);
    }
    if (s.kind == 1) { q.b0 = q.a2; q.b1 = q.a1; q.b2 = 1.0; return q; }     // pasatodos exacto
    if (s.kind == 2)                                                          // notch: ceros justo en w0
    {
        const double c = std::cos (std::min (w0, 0.999 * nyq) * T), k = (1.0 + q.a1 + q.a2) / (2.0 - 2.0 * c);
        q.b0 = k; q.b1 = -2.0 * c * k; q.b2 = k; return q;
    }
    const double A0 = (1 + q.a1 + q.a2) * (1 + q.a1 + q.a2), A1 = (1 - q.a1 + q.a2) * (1 - q.a1 + q.a2), A2 = -4.0 * q.a2;
    const double B0 = A0 * std::norm (evalA (s, 0.0)), B1 = A1 * std::norm (evalA (s, nyq));
    const double wm = (w0 * T < 0.9 * pi) ? w0 * T : 0.5 * pi;
    const double p1 = std::sin (wm * 0.5) * std::sin (wm * 0.5), p0 = 1.0 - p1, p2 = 4.0 * p0 * p1;
    const double Hm = std::norm (evalA (s, wm * fs));
    double B2 = (Hm * (A0 * p0 + A1 * p1 + A2 * p2) - B0 * p0 - B1 * p1) / p2;
    const double sB0 = std::sqrt (B0), sB1 = std::sqrt (B1), W = 0.5 * (sB0 + sB1);
    B2 = std::max (B2, -W * W);
    q.b0 = 0.5 * (W + std::sqrt (W * W + B2));
    q.b1 = 0.5 * (sB0 - sB1);
    q.b2 = q.b0 > 1e-300 ? -B2 / (4.0 * q.b0) : 0.0;
    return q;
}

// ajuste fino en agudos: Levenberg-Marquardt sobre el log de la magnitud (5 coeficientes) contra la seccion
// analogica en 40 puntos hasta Nyquist, partiendo del diseno "matched". Mantiene polos estables y ceros de fase minima.
inline Biquad fitSection (const ASec& s, double fs, Biquad p, int iters)
{
    constexpr int K = 40;
    // ceros fijos en DC cuando el circuito los tiene: 1 = doble (pasaaltos), 2 = simple (pasabanda)
    const int zmode = (s.b0 == 0.0 && s.b1 == 0.0) ? 1 : (s.b0 == 0.0 ? 2 : 0);
    if (zmode == 1) { p.b1 = -2.0 * p.b0; p.b2 = p.b0; }
    if (zmode == 2) { p.b1 = -p.b0 - p.b2; }
    const double nyq = pi * fs, lo = std::max (2.0 * pi * 20.0, s.w0 / 12.0);
    double W[K], tgt[K];
    for (int i = 0; i < K; ++i)
    {
        const double t = (double) i / (K - 1);
        W[i] = lo * std::pow (0.9995 * nyq / lo, t);
        tgt[i] = std::log (std::max (1e-7, std::abs (evalA (s, W[i]))));
    }
    auto cost = [&] (const Biquad& c, double* r)
    {
        double e = 0;
        for (int i = 0; i < K; ++i) { const double v = std::log (std::max (1e-7, std::abs (c.resp (W[i] / fs)))) - tgt[i]; if (r) r[i] = v; e += v * v; }
        return e;
    };
    auto stable = [] (const Biquad& c) { return std::abs (c.a2) < 0.99999 && std::abs (c.a1) < 1.0 + c.a2 - 1e-9; };
    double r[K], lam = 1e-3, e = cost (p, r);
    for (int it = 0; it < iters; ++it)
    {
        double JtJ[5][5] = {}, Jtr[5] = {};
        for (int i = 0; i < K; ++i)
        {
            const double wT = W[i] / fs;
            const cd z1 = std::polar (1.0, -wT), z2 = z1 * z1, N = p.b0 + p.b1 * z1 + p.b2 * z2, D = 1.0 + p.a1 * z1 + p.a2 * z2;
            double j[5] { (1.0 / N).real(), (z1 / N).real(), (z2 / N).real(), -(z1 / D).real(), -(z2 / D).real() };
            if (zmode == 1) { j[0] = ((1.0 - 2.0 * z1 + z2) / N).real(); j[1] = j[2] = 0; }
            if (zmode == 2) { j[0] = ((1.0 - z1) / N).real(); j[2] = ((z2 - z1) / N).real(); j[1] = 0; }
            for (int a = 0; a < 5; ++a) { Jtr[a] += j[a] * r[i]; for (int b = 0; b < 5; ++b) JtJ[a][b] += j[a] * j[b]; }
        }
        bool improved = false;
        for (int tries = 0; tries < 6 && ! improved; ++tries)
        {
            double M[5][6];
            for (int a = 0; a < 5; ++a) { for (int b = 0; b < 5; ++b) M[a][b] = JtJ[a][b] + (a == b ? lam * (JtJ[a][a] + 1e-12) : 0.0); M[a][5] = -Jtr[a]; if (JtJ[a][a] == 0.0) M[a][a] = 1.0; }
            bool ok = true;
            for (int c = 0; c < 5 && ok; ++c)
            {
                int pv = c; for (int k = c + 1; k < 5; ++k) if (std::abs (M[k][c]) > std::abs (M[pv][c])) pv = k;
                if (std::abs (M[pv][c]) < 1e-30) { ok = false; break; }
                for (int k = 0; k < 6; ++k) std::swap (M[c][k], M[pv][k]);
                for (int k = 0; k < 5; ++k) if (k != c) { const double f = M[k][c] / M[c][c]; for (int l = c; l < 6; ++l) M[k][l] -= f * M[c][l]; }
            }
            if (! ok) break;
            Biquad t = p;
            auto stp = [&] (int a) { return std::abs (M[a][a]) > 1e-30 ? M[a][5] / M[a][a] : 0.0; };
            t.b0 += stp (0); t.b1 += stp (1); t.b2 += stp (2); t.a1 += stp (3); t.a2 += stp (4);
            if (zmode == 1) { t.b1 = -2.0 * t.b0; t.b2 = t.b0; }
            if (zmode == 2) { t.b1 = -t.b0 - t.b2; }
            double rt[K];
            if (stable (t)) { const double et = cost (t, rt); if (et < e) { p = t; e = et; std::copy (rt, rt + K, r); lam *= 0.3; improved = true; break; } }
            lam *= 8.0;
        }
        if (! improved || e < 1e-12) break;
    }
    // ceros de fase minima (si alguno quedo afuera del circulo, se refleja: la magnitud no cambia)
    if (zmode == 0 && std::abs (p.b0) > 1e-300)
    {
        const double c1 = p.b1 / p.b0, c2 = p.b2 / p.b0, disc = c1 * c1 - 4.0 * c2;
        if (disc < 0) { if (c2 > 1.0) { p.b0 *= std::sqrt (c2); p.b1 = p.b1 / std::sqrt (c2); p.b2 = p.b0; p.b1 = p.b0 * c1 / std::sqrt (c2); } }
        else
        {
            double z1 = 0.5 * (-c1 + std::sqrt (disc)), z2 = 0.5 * (-c1 - std::sqrt (disc)), k = p.b0;
            if (std::abs (z1) > 1.0) { k *= std::abs (z1); z1 = 1.0 / z1; }
            if (std::abs (z2) > 1.0) { k *= std::abs (z2); z2 = 1.0 / z2; }
            p.b0 = k; p.b1 = -k * (z1 + z2); p.b2 = k * z1 * z2;
        }
    }
    return p;
}

// topologia adaptativa: bilineal con pre-warp donde es exacta (graves y medios), "matched" + ajuste fino en agudos
inline Biquad digitalize (const ASec& s, double fs, const Biquad* warm = nullptr, int iters = 12)
{
    const double wn = s.w0 / fs;                                              // rad/muestra
    if (s.kind == 1 && wn < 0.5 * pi) return blt (s, fs, s.w0);
    if (wn < 0.12 * pi) return blt (s, fs, s.w0);
    // el ajuste arranca del bilineal en medios (continuo con la zona de abajo: sin saltos al barrer) y del matched arriba
    const Biquad m = wn < 0.5 * pi ? blt (s, fs, s.w0) : matched (s, fs);
    if (s.kind != 0 || s.a2 == 0.0) return s.kind != 0 ? matched (s, fs) : m;
    const Biquad f = fitSection (s, fs, warm != nullptr ? *warm : m, warm != nullptr ? std::min (iters, 4) : iters);
    // zona de transicion (0.12 a 0.2 pi): mezcla continua bilineal -> ajustado, sin saltos al barrer la frecuencia
    const double t = std::clamp ((wn - 0.12 * pi) / (0.08 * pi), 0.0, 1.0), u = t * t * (3.0 - 2.0 * t);
    if (u >= 1.0) return f;
    const Biquad b = blt (s, fs, s.w0);
    return { b.b0 + (f.b0 - b.b0) * u, b.b1 + (f.b1 - b.b1) * u, b.b2 + (f.b2 - b.b2) * u, b.a1 + (f.a1 - b.a1) * u, b.a2 + (f.a2 - b.a2) * u };
}

// ---------------------------------------------------------------------------------------------
// seccion en ejecucion (forma directa transpuesta II, doble precision)
struct Sec
{
    Biquad c; double z1 = 0, z2 = 0;
    inline double process (double x) noexcept
    {
        const double y = c.b0 * x + z1;
        z1 = c.b1 * x - c.a1 * y + z2; z2 = c.b2 * x - c.a2 * y;
        return y;
    }
    void reset() { z1 = z2 = 0; }
};

// pasabanda de deteccion / escucha (2do orden, ganancia 1 en el centro)
inline Biquad bandpassAt (double f, double q, double fs)
{
    const double w = 2.0 * pi * std::min (f, 0.45 * fs), qq = std::clamp (q, 0.3, 20.0);
    return digitalize ({ 0, w / qq, 0, 1, w / qq, w * w, 0, w }, fs);
}

inline double noteOf (double hz) { return 69.0 + 12.0 * std::log2 (hz / 440.0); }
} // namespace spq
