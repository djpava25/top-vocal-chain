#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <map>
#include <mutex>
#include <tuple>
#include <vector>
#include "MicData.h"

// Motor de TOP MIC COLLECTION (doble precision, sin latencia).
//
//  1. Curva del capsula: cada respuesta medida (MicData.h) se ajusta a una cadena de filtros de pico y repisa que
//     son de fase minima e INVERTIBLES. El micro de origen se quita con su cadena invertida exacta (mismo micro de
//     origen y de destino = plano al 100 %), y luego se aplica el destino.
//  2. Proximidad fisica: un microfono de gradiente a distancia r responde  P + (1-a) cos(t) c / (j w r).
//     Se convierte de la distancia real (con el patron del micro de origen) a la distancia virtual (con el patron
//     del destino) en un solo filtro de 1er orden: tambien depende del angulo (de costado no hay proximidad).
//  3. Fuera de eje: el diafragma se comporta como un piston de radio R: los agudos caen con k R sen(t).
//  4. Electronica: valvula / FET (armonicos segun su SPL maximo de hoja de datos) y transformador (saturacion por
//     flujo: solo en graves fuertes, la "calidez" que no tiene un micro sin transformador).
namespace tmic
{
constexpr double kPi = 3.14159265358979323846, kC = 343.0;
enum BandType { Peak = 0, LowShelf, HighShelf, HighPass, HighPass1 };
struct Band { int type; double f, q, g; };

inline const char* patternName (int p) { static const char* n[] { "OMNI", "WIDE", "CARDIOID", "HYPER", "FIGURE-8" }; return n[std::clamp (p, 0, 4)]; }
inline int nearestPattern (const Model& m, int want)
{
    for (int d = 0; d < 5; ++d)
    {
        if (want - d >= 0 && (m.patterns >> (want - d) & 1u)) return want - d;
        if (want + d < 5 && (m.patterns >> (want + d) & 1u)) return want + d;
    }
    return m.defPattern;
}
// a de g(t) = a + (1-a) cos t
inline double patternA (const Model& m, int p)
{
    switch (p) { case Omni: return 1.0; case Wide: return 0.7; case Cardioid: return m.cardA; case Hyper: return 0.25; default: return 0.0; }
}
inline float polarGain (const Model& m, int p, float deg) { const double a = patternA (m, p); return (float) (a + (1.0 - a) * std::cos (deg * kPi / 180.0)); }

// ------------------------------------------------------------------------------------------------ curvas
// interpolacion monotona (PCHIP) en log de frecuencia
inline double curveDb (const Curve& c, double f)
{
    if (c.empty()) return 0.0;
    const double x = std::log (std::max (1.0, f));
    const size_t n = c.size();
    auto X = [&] (size_t i) { return std::log (c[i].first); };
    if (x <= X (0)) return c[0].second;
    if (x >= X (n - 1))
    {
        const double sl = n > 1 ? (c[n - 1].second - c[n - 2].second) / (X (n - 1) - X (n - 2)) : 0.0;
        return std::max (-45.0, c[n - 1].second + std::min (0.0, sl) * (x - X (n - 1)));
    }
    size_t i = 0; while (i + 1 < n && X (i + 1) < x) ++i;
    auto slope = [&] (size_t k)
    {
        if (k == 0 || k == n - 1)
        {
            const size_t a = k == 0 ? 0 : n - 2;
            return (c[a + 1].second - c[a].second) / (X (a + 1) - X (a));
        }
        const double d0 = (c[k].second - c[k - 1].second) / (X (k) - X (k - 1)), d1 = (c[k + 1].second - c[k].second) / (X (k + 1) - X (k));
        if (d0 * d1 <= 0) return 0.0;
        const double h0 = X (k) - X (k - 1), h1 = X (k + 1) - X (k), w1 = 2 * h1 + h0, w2 = h1 + 2 * h0;
        return (w1 + w2) / (w1 / d0 + w2 / d1);
    };
    const double h = X (i + 1) - X (i), t = (x - X (i)) / h, t2 = t * t, t3 = t2 * t;
    return (2 * t3 - 3 * t2 + 1) * c[i].second + (t3 - 2 * t2 + t) * h * slope (i) + (-2 * t3 + 3 * t2) * c[i + 1].second + (t3 - t2) * h * slope (i + 1);
}
// cambio de la curva de un multipatron de doble membrana segun el patron (respecto de cardioide)
inline double patternDeltaDb (int p, double f)
{
    auto bell = [f] (double fc, double oct) { const double u = std::log2 (f / fc) / (oct * 0.5); return std::exp (-0.5 * u * u); };
    auto lowS = [f] (double fc) { const double r = f / fc; return 1.0 / (1.0 + r * r); };
    auto highS = [f] (double fc) { const double r = fc / f; return 1.0 / (1.0 + r * r); };
    switch (p)
    {
        case Omni: return 2.0 * bell (9500, 1.4) - 1.5 * lowS (60);          // omni de doble membrana: aire arriba, menos grave
        case Wide: return 1.0 * bell (9500, 1.4) - 0.75 * lowS (60);
        case Hyper: return -0.5 * bell (9000, 1.4);
        case Figure8: return 1.0 * bell (5000, 1.6) - 1.5 * highS (9000);     // figura 8: presencia en 5 kHz, menos aire
        default: return 0.0;
    }
}
inline double modelDb (const Model& m, int p, double f)
{
    double d = curveDb (m.curve, f) - curveDb (m.curve, 1000.0);
    if (m.dualDiaphragm && p != Cardioid) d += patternDeltaDb (p, f) - patternDeltaDb (p, 1000.0);
    return d;
}
// el color del origen se quita como maximo 12 dB (no se puede recuperar lo que el micro no capto)
inline double sourceDb (const Source& s, double f) { return std::max (-12.0, curveDb (s.curve, f) - curveDb (s.curve, 1000.0)); }

// ------------------------------------------------------------------------------------------------ biquads RBJ
struct Coefs { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };
inline Coefs design (const Band& b, double fs)
{
    const double w0 = 2 * kPi * std::min (b.f, 0.49 * fs) / fs, cw = std::cos (w0), sw = std::sin (w0), al = sw / (2 * b.q);
    const double A = std::pow (10.0, b.g / 40.0), sA = std::sqrt (A);
    double b0, b1, b2, a0, a1, a2;
    switch (b.type)
    {
        case Peak: b0 = 1 + al * A; b1 = -2 * cw; b2 = 1 - al * A; a0 = 1 + al / A; a1 = -2 * cw; a2 = 1 - al / A; break;
        case LowShelf:
            b0 = A * ((A + 1) - (A - 1) * cw + 2 * sA * al); b1 = 2 * A * ((A - 1) - (A + 1) * cw); b2 = A * ((A + 1) - (A - 1) * cw - 2 * sA * al);
            a0 = (A + 1) + (A - 1) * cw + 2 * sA * al; a1 = -2 * ((A - 1) + (A + 1) * cw); a2 = (A + 1) + (A - 1) * cw - 2 * sA * al; break;
        case HighShelf:
            b0 = A * ((A + 1) + (A - 1) * cw + 2 * sA * al); b1 = -2 * A * ((A - 1) + (A + 1) * cw); b2 = A * ((A + 1) + (A - 1) * cw - 2 * sA * al);
            a0 = (A + 1) - (A - 1) * cw + 2 * sA * al; a1 = 2 * ((A - 1) - (A + 1) * cw); a2 = (A + 1) - (A - 1) * cw - 2 * sA * al; break;
        case HighPass1:
        {
            const double K = std::tan (kPi * std::min (b.f, 0.49 * fs) / fs);
            return { 1 / (1 + K), -1 / (1 + K), 0, (K - 1) / (K + 1), 0 };
        }
        default: b0 = (1 + cw) / 2; b1 = -(1 + cw); b2 = (1 + cw) / 2; a0 = 1 + al; a1 = -2 * cw; a2 = 1 - al; break;
    }
    return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
}
inline double magDb (const Coefs& c, double cw, double c2w)
{
    const double num = c.b0 * c.b0 + c.b1 * c.b1 + c.b2 * c.b2 + 2 * (c.b0 * c.b1 + c.b1 * c.b2) * cw + 2 * c.b0 * c.b2 * c2w;
    const double den = 1 + c.a1 * c.a1 + c.a2 * c.a2 + 2 * (c.a1 + c.a1 * c.a2) * cw + 2 * c.a2 * c2w;
    return 10.0 * std::log10 (std::max (1e-30, num) / std::max (1e-30, den));
}
inline double bandsDb (const std::vector<Band>& bs, double fs, double f)
{
    const double w = 2 * kPi * f / fs, cw = std::cos (w), c2w = std::cos (2 * w);
    double s = 0; for (const auto& b : bs) s += magDb (design (b, fs), cw, c2w);
    return s;
}

// ------------------------------------------------------------------------------------------------ ajuste de curva
// greedy (repisas en los extremos + picos donde mas falta) y luego descenso por coordenadas de todos los parametros
inline std::vector<Band> fitCurve (const std::function<double (double)>& curve, double fs, int maxBands = 12)
{
    const int N = 112;
    const double fLo = 20.0, fHi = std::min (20000.0, 0.46 * fs);
    std::vector<double> f ((size_t) N), T ((size_t) N), W ((size_t) N), cw ((size_t) N), c2w ((size_t) N);
    for (int i = 0; i < N; ++i)
    {
        const double x = std::exp (std::log (fLo) + (std::log (fHi) - std::log (fLo)) * i / (N - 1));
        f[(size_t) i] = x; T[(size_t) i] = curve (x);
        W[(size_t) i] = (x >= 30 && x <= 16000) ? 1.0 : 0.35;
        const double w = 2 * kPi * x / fs; cw[(size_t) i] = std::cos (w); c2w[(size_t) i] = std::cos (2 * w);
    }
    std::vector<Band> bands;
    std::vector<std::vector<double>> contrib;
    std::vector<double> total ((size_t) N, 0.0);
    auto eval = [&] (const Band& b, std::vector<double>& out) { const Coefs c = design (b, fs); out.resize ((size_t) N); for (int i = 0; i < N; ++i) out[(size_t) i] = magDb (c, cw[(size_t) i], c2w[(size_t) i]); };
    auto add = [&] (const Band& b) { std::vector<double> c; eval (b, c); for (int i = 0; i < N; ++i) total[(size_t) i] += c[(size_t) i]; bands.push_back (b); contrib.push_back (std::move (c)); };
    auto resid = [&] (int i) { return T[(size_t) i] - total[(size_t) i]; };
    auto findHalf = [&] (int from, int dir, double ref)
    {
        int i = from;
        while (i + dir >= 0 && i + dir < N && std::abs (resid (i + dir)) > std::abs (ref) * 0.5 && resid (i + dir) * ref > 0) i += dir;
        return i;
    };
    // repisas en los extremos
    if (std::abs (resid (0)) > 0.8) { const double g = resid (0); const int k = findHalf (0, +1, g); add ({ LowShelf, std::clamp (f[(size_t) k], 25.0, 800.0), 0.6, std::clamp (g * 1.05, -24.0, 24.0) }); }
    if (std::abs (resid (N - 1)) > 0.8) { const double g = resid (N - 1); const int k = findHalf (N - 1, -1, g); add ({ HighShelf, std::clamp (f[(size_t) k], 2000.0, 0.45 * fs), 0.6, std::clamp (g * 1.05, -30.0, 24.0) }); }
    // picos
    while ((int) bands.size() < maxBands)
    {
        int bi = -1; double bv = 0.15;
        for (int i = 2; i < N - 2; ++i) { const double r = std::abs (resid (i)) * W[(size_t) i]; if (r > bv) { bv = r; bi = i; } }
        if (bi < 0) break;
        const double g = resid (bi);
        const int lo = findHalf (bi, -1, g), hi = findHalf (bi, +1, g);
        const double bw = std::max (0.25, std::log2 (f[(size_t) hi] / f[(size_t) lo]));
        const double p2 = std::pow (2.0, bw), q = std::clamp (std::sqrt (p2) / (p2 - 1.0), 0.3, 6.0);
        add ({ Peak, f[(size_t) bi], q, std::clamp (g, -18.0, 18.0) });
    }
    // refinamiento
    auto err = [&] (const std::vector<double>& tot) { double e = 0; for (int i = 0; i < N; ++i) { const double d = T[(size_t) i] - tot[(size_t) i]; e += W[(size_t) i] * d * d; } return e; };
    double best = err (total);
    double sF = 0.10, sQ = 0.20, sG = 0.6;
    std::vector<double> trial ((size_t) N), cand;
    for (int sweep = 0; sweep < 14; ++sweep)
    {
        for (size_t b = 0; b < bands.size(); ++b)
            for (int prm = 0; prm < 3; ++prm)
                for (int dir : { -1, 1 })
                {
                    Band nb = bands[b];
                    if (prm == 0) nb.f = std::clamp (nb.f * std::exp (dir * sF), 15.0, 0.47 * fs);
                    else if (prm == 1) nb.q = std::clamp (nb.q * std::exp (dir * sQ), 0.25, 8.0);
                    else nb.g = std::clamp (nb.g + dir * sG, -30.0, 24.0);
                    eval (nb, cand);
                    for (int i = 0; i < N; ++i) trial[(size_t) i] = total[(size_t) i] - contrib[b][(size_t) i] + cand[(size_t) i];
                    const double e = err (trial);
                    if (e < best) { best = e; bands[b] = nb; contrib[b] = cand; total = trial; }
                }
        sF *= 0.7; sQ *= 0.7; sG *= 0.7;
    }
    return bands;
}

// cache de ajustes (por modelo/patron/origen y frecuencia de muestreo); se comparte entre instancias
inline const std::vector<Band>& fitFor (int kind, int index, int pattern, double fs)
{
    static std::mutex mx;
    static std::map<std::tuple<int, int, int, int>, std::vector<Band>> cache;
    const auto key = std::make_tuple (kind, index, pattern, (int) std::lround (fs));
    std::lock_guard<std::mutex> lock (mx);
    auto it = cache.find (key);
    if (it != cache.end()) return it->second;
    std::vector<Band> b;
    if (kind == 0) { const auto& m = models()[(size_t) index]; b = fitCurve ([&] (double f) { return modelDb (m, pattern, f); }, fs); }
    else { const auto& s = sources()[(size_t) index]; if (! s.curve.empty()) b = fitCurve ([&] (double f) { return sourceDb (s, f); }, fs); }
    return cache[key] = b;
}
inline void prefetchAll (double fs)
{
    for (int i = 0; i < (int) models().size(); ++i) for (int p = 0; p < 5; ++p) if (models()[(size_t) i].patterns >> p & 1u) fitFor (0, i, p, fs);
    for (int i = 0; i < (int) sources().size(); ++i) fitFor (1, i, 0, fs);
}

struct Settings
{
    int model = 0, source = 0, pattern = Cardioid, filter = 0;
    float axis = 0, prox = 0, dist = 15;   // grados, -100..100, cm
    bool rev = false;
    bool sameStatic (const Settings& o) const { return model == o.model && source == o.source && nearestPattern (models()[(size_t) model], pattern) == nearestPattern (models()[(size_t) o.model], o.pattern) && filter == o.filter; }
};

// filtros fijos de la cadena: destino + origen invertido + filtro de graves del micro + seguridad de subgraves
inline std::vector<Band> staticBands (const Settings& s, double fs)
{
    const int mi = std::clamp (s.model, 0, (int) models().size() - 1), si = std::clamp (s.source, 0, (int) sources().size() - 1);
    const int pat = nearestPattern (models()[(size_t) mi], s.pattern);
    std::vector<Band> b = fitFor (0, mi, pat, fs);
    for (const auto& x : fitFor (1, si, 0, fs)) b.push_back ({ x.type, x.f, x.q, -x.g });       // inversa exacta
    static const double hp[] { 0, 60, 100, 160 }, hq[] { 0, 0.6, 0.65, 0.7 };
    if (s.filter > 0 && s.filter < 4) b.push_back ({ HighPass, hp[s.filter], hq[s.filter], 0 });
    b.push_back ({ HighPass1, 3.0, 0.7, 0 });                                                      // solo corriente continua
    return b;
}

// parametros continuos (proximidad, eje)
struct Dyn { double P = 1, K = 0, ws = 100, offFc = 20000, offDepth = 0; };
inline Dyn dynamicsFor (const Settings& s, double fs)
{
    const auto& m = models()[(size_t) std::clamp (s.model, 0, (int) models().size() - 1)];
    const auto& src = sources()[(size_t) std::clamp (s.source, 0, (int) sources().size() - 1)];
    const int pat = nearestPattern (m, s.pattern);
    const double a = patternA (m, pat);
    const double th = (s.rev ? 180.0 - s.axis : s.axis) * kPi / 180.0, ct = std::cos (th);
    const double r0 = 0.03;                                                  // tamano de la boca/fuente + capsula
    const double rReal = std::clamp ((double) s.dist, 1.0, 300.0) * 0.01;
    const double rVirt = std::clamp (rReal * std::pow (2.0, -s.prox / 50.0), 0.01, 3.0);
    const double eR = std::sqrt (rReal * rReal + r0 * r0), eV = std::sqrt (rVirt * rVirt + r0 * r0);
    Dyn d;
    d.P = a + (1.0 - a) * ct;
    const double Gt = m.kp * (1.0 - a) * ct * kC / eV;
    // origen sin proximidad (omni): el polo se deja en 20 Hz (debajo la subida de graves se aplana, como en un micro real)
    d.ws = std::max (2 * kPi * 20.0, src.kp * (1.0 - src.a) * kC / eR);
    double lf = Gt / d.ws;                                                   // ganancia en graves profundos
    lf = std::clamp (lf, -40.0, 40.0);
    d.K = lf - d.P;
    // fuera de eje: piston de radio R (la figura 8 es simetrica adelante/atras)
    const double st = pat == Figure8 ? std::abs (std::sin (th)) : (th <= kPi / 2 ? std::sin (th) : 1.0);
    d.offFc = std::min (0.45 * fs, kC * 1.6 / (2 * kPi * m.radius * std::max (1e-3, st)));
    const double u = std::clamp (st / 0.3, 0.0, 1.0);
    d.offDepth = (1.0 - m.offFloor) * u * u * (3 - 2 * u);
    return d;
}

// respuesta total (para la grafica y las pruebas): filtros fijos + proximidad + eje
inline double responseDb (const Settings& s, double fs, double f)
{
    double db = bandsDb (staticBands (s, fs), fs, f);
    const Dyn d = dynamicsFor (s, fs);
    // frecuencias "deformadas" como en los filtros TPT digitales (asi la grafica es exacta tambien cerca de Nyquist)
    const double tf = std::tan (kPi * std::min (f, 0.499 * fs) / fs);
    // P + K ws / (j w + ws)
    const double wr = tf / std::tan (std::min (0.49 * kPi, d.ws / (2 * fs)));
    const double den = wr * wr + 1.0, re = d.P + d.K / den, im = -d.K * wr / den;
    db += 20 * std::log10 (std::max (1e-9, std::hypot (re, im)));
    // 1 - depth (1 - LP2)
    const double r = tf / std::tan (kPi * std::min (d.offFc, 0.47 * fs) / fs), q = 0.6;
    const double lre = 1 - r * r, lim = r / q, lden = lre * lre + lim * lim;
    const double lpRe = lre / lden, lpIm = -lim / lden;
    const double ore = 1 - d.offDepth * (1 - lpRe), oim = d.offDepth * lpIm;
    db += 20 * std::log10 (std::max (1e-9, std::hypot (ore, oim)));
    return db;
}

// ------------------------------------------------------------------------------------------------ cadena de un micro
class Voice
{
public:
    void prepare (double rate) { fs = rate; reset(); }
    void reset()
    {
        for (auto& c : ch) { for (auto& z : c.z) z = { 0, 0 }; for (auto& z : c.lpz) z = { 0, 0 }; c.lp = c.s1 = c.s2 = c.dc = c.dcx = c.flux = c.tdc1 = c.tdc2 = 0; }
        primed = false;
    }
    void setStatic (const Settings& s)
    {
        const auto bs = staticBands (s, fs);
        nb = (int) std::min (bs.size(), (size_t) maxBands);
        for (int i = 0; i < nb; ++i) co[(size_t) i] = design (bs[(size_t) i], fs);
        const auto& m = models()[(size_t) std::clamp (s.model, 0, (int) models().size() - 1)];
        // electronica: armonicos segun su SPL maximo de hoja de datos (0 dBFS = 120 dB SPL: una voz fuerte de cerca
        // con picos en -6 dBFS llega a ~114 dB SPL, justo donde una valvula estilo 67 empieza a ensuciar)
        const double Am = std::pow (10.0, (m.maxSpl - 120.0) / 20.0);
        double H2 = 0, H3 = 0;
        if (m.amp == Triode) { H2 = 0.005; H3 = 0.0008; }
        else if (m.amp == Pentode) { H2 = 0.0045; H3 = 0.0015; }
        else if (m.amp == Fet) { H2 = 0.004; H3 = 0.001; }
        h2 = 2 * H2 / Am; h3 = -4 * H3 / (Am * Am);
        // tope suave antes del polinomio para que la curva sea siempre creciente
        clipX = 16.0;
        hasAmp = m.amp != NoAmp;
        if (std::abs (h3) > 1e-12) clipX = std::min (clipX, 0.8 * std::sqrt (1.0 / (3.0 * std::abs (h3))));
        if (std::abs (h2) > 1e-12) clipX = std::min (clipX, 0.4 / std::abs (h2));
        kRay = 1.0 * m.trans; kCub = 20.0 * m.trans; kEven = 0.4 * m.trans;
        aFlux = std::exp (-2 * kPi * 6.0 / fs);
        aDc = std::exp (-2 * kPi * 4.0 / fs);
        tdcCo = design ({ HighPass, 2.0, 0.6, 0 }, fs);
        {   // paso bajo de 4to orden en 6.5 kHz (dos Butterworth de 2do orden en serie) para la banda de la valvula
            const double w0 = 2 * kPi * 6500.0 / fs, cw = std::cos (w0), al = std::sin (w0) / (2 * 0.70710678);
            const double a0 = 1 + al;
            lpCo = { (1 - cw) / 2 / a0, (1 - cw) / a0, (1 - cw) / 2 / a0, -2 * cw / a0, (1 - al) / a0 };
        }
    }
    void setDyn (const Dyn& d, bool snap)
    {
        tgt = d;
        if (snap || ! primed) { cur = d; primed = true; updateCoefs(); }
    }
    // procesa un bloque de un canal (la proximidad/eje se suavizan cada 16 muestras)
    inline double process (int c, double x) noexcept
    {
        auto& s = ch[(size_t) c];
        for (int i = 0; i < nb; ++i)
        {
            const Coefs& k = co[(size_t) i]; auto& z = s.z[(size_t) i];
            const double y = k.b0 * x + z[0];
            z[0] = k.b1 * x - k.a1 * y + z[1]; z[1] = k.b2 * x - k.a2 * y; x = y;
        }
        // proximidad: P x + K LP(x)
        { const double v = (x - s.lp) * gLp; const double lp = v + s.lp; s.lp = lp + v; x = cur.P * x + cur.K * lp; }
        // fuera de eje: x - depth (x - LP2(x))
        if (cur.offDepth > 1e-6)
        {
            const double v3 = x - s.s2, v1 = a1 * s.s1 + a2 * v3, v2 = s.s2 + a2 * s.s1 + a3 * v3;
            s.s1 = 2 * v1 - s.s1; s.s2 = 2 * v2 - s.s2;
            x = x - cur.offDepth * (x - v2);
        }
        else { s.s1 = 0; s.s2 = x; }
        // valvula / FET: armonicos 2 y 3 (el 2 sin corriente continua). Se generan desde la parte de la senal bajo ~6.5 kHz
        // (sus armonicos caen en la banda audible); lo de arriba pasa limpio: asi no hay aliasing sin sobremuestrear.
        if (hasAmp)
        {
            double xl = x;
            for (int k = 0; k < 2; ++k)
            {
                const Coefs& q = lpCo; auto& z = s.lpz[(size_t) k];
                const double y = q.b0 * xl + z[0]; z[0] = q.b1 * xl - q.a1 * y + z[1]; z[1] = q.b2 * xl - q.a2 * y; xl = y;
            }
            const double xc = clipX * std::tanh (xl / clipX);
            const double e = h2 * xc * xc;
            s.dc = aDc * s.dc + (1 - aDc) * e;
            x = x - xl + xc + (e - s.dc) + h3 * xc * xc * xc;
        }
        // transformador: flujo = integral de la tension -> la corriente de magnetizacion distorsiona solo graves
        if (kCub > 0.0)
        {
            s.flux = aFlux * s.flux + (1 - aFlux) * x;
            const double fl = s.flux;
            const double im = kRay * fl * std::abs (fl) + kCub * fl * fl * fl + kEven * fl * fl;
            const double y = x - im;
            const double yy = tdcCo.b0 * y + s.tdc1; s.tdc1 = tdcCo.b1 * y - tdcCo.a1 * yy + s.tdc2; s.tdc2 = tdcCo.b2 * y - tdcCo.a2 * yy;
            x = yy;
        }
        return x;
    }
    // cada 16 muestras: acercar los parametros continuos al objetivo
    inline void tick (int n) noexcept
    {
        const double a = 1.0 - std::exp (-(double) n / (0.025 * fs));
        cur.P += (tgt.P - cur.P) * a; cur.K += (tgt.K - cur.K) * a;
        cur.ws += (tgt.ws - cur.ws) * a; cur.offFc += (tgt.offFc - cur.offFc) * a; cur.offDepth += (tgt.offDepth - cur.offDepth) * a;
        updateCoefs();
    }
private:
    void updateCoefs()
    {
        const double g = std::tan (std::min (0.49 * kPi, cur.ws / (2 * fs)));
        gLp = g / (1 + g);
        const double gs = std::tan (kPi * std::min (cur.offFc, 0.47 * fs) / fs), k = 1.0 / 0.6;
        a1 = 1.0 / (1.0 + gs * (gs + k)); a2 = gs * a1; a3 = gs * a2;
    }
    static constexpr int maxBands = 32;
    struct Ch { std::array<std::array<double, 2>, maxBands> z {}; std::array<std::array<double, 2>, 2> lpz {}; double lp = 0, s1 = 0, s2 = 0, dc = 0, dcx = 0, flux = 0, tdc1 = 0, tdc2 = 0; };
    std::array<Ch, 2> ch;
    std::array<Coefs, maxBands> co;
    int nb = 0;
    double fs = 48000.0, gLp = 0.01, a1 = 1, a2 = 0, a3 = 0;
    bool hasAmp = false;
    double h2 = 0, h3 = 0, clipX = 4, kRay = 0, kCub = 0, kEven = 0, aFlux = 0.999, aDc = 0.999;
    Coefs tdcCo, lpCo;
    Dyn cur, tgt;
    bool primed = false;
};

// un microfono con cambio de modelo sin clics: la cadena nueva se "precalienta" y luego se cruza
class MicChain
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        prefetchAll (sr);
        for (auto& v : voice) v.prepare (sr);
        preroll = (int) (0.020 * sr); fadeLen = (int) (0.030 * sr);
        active = 0; fading = -1; hasLast = false; pending = false;
    }
    void reset() { for (auto& v : voice) v.reset(); fading = -1; }
    // se llama una vez por bloque
    void configure (const Settings& s)
    {
        if (! hasLast)
        {
            voice[(size_t) active].setStatic (s); voice[(size_t) active].setDyn (dynamicsFor (s, sr), true);
            last = s; hasLast = true; return;
        }
        if (! s.sameStatic (last))
        {
            if (fading < 0) startFade (s);
            else { pending = true; pend = s; }
        }
        last = s;
        const Dyn d = dynamicsFor (s, sr);
        voice[(size_t) active].setDyn (d, false);
        if (fading >= 0) voice[(size_t) (1 - active)].setDyn (d, false);
    }
    inline void process (double* x, int chs) noexcept
    {
        if (++tickCount >= 16) { tickCount = 0; voice[0].tick (16); voice[1].tick (16); }
        if (fading < 0) { for (int c = 0; c < chs; ++c) x[c] = voice[(size_t) active].process (c, x[c]); return; }
        auto& nv = voice[(size_t) (1 - active)];
        const double t = fading < preroll ? 0.0 : std::min (1.0, (double) (fading - preroll) / fadeLen);
        const double g = 0.5 - 0.5 * std::cos (kPi * t);
        for (int c = 0; c < chs; ++c) { const double a = voice[(size_t) active].process (c, x[c]), b = nv.process (c, x[c]); x[c] = a + (b - a) * g; }
        if (++fading >= preroll + fadeLen)
        {
            active = 1 - active; fading = -1;
            if (pending) { pending = false; startFade (pend); }
        }
    }
    bool isFading() const { return fading >= 0; }
private:
    void startFade (const Settings& s)
    {
        auto& nv = voice[(size_t) (1 - active)];
        nv.reset(); nv.setStatic (s); nv.setDyn (dynamicsFor (s, sr), true);
        fading = 0;
    }
    std::array<Voice, 2> voice;
    double sr = 48000.0;
    int active = 0, fading = -1, preroll = 960, fadeLen = 1440, tickCount = 0;
    bool hasLast = false, pending = false;
    Settings last, pend;
};
} // namespace tmic
