#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>
#include <complex>
#include <vector>

// Top VoxStrips (antes 6961P): canal de voz a valvulas, todo en doble precision y a 8x.
// Orden del hardware: HPF -> INPUT -> compresor optico ELOP 3:1 (antes del preamp) -> preamp a valvula (40-60 dB)
// -> EQ pasivo pico-valle-pico -> de-esser LC / limitador 10:1 -> etapa final a valvula (5751 + 6414) -> XFMR -> salida.
// Formas de las bandas ajustadas a 14 mediciones por impulso: LOW PEAK Q 0.6, MID DIP Q 1.1, HI PEAK Q 0.8.
namespace vbx
{
constexpr double pi = 3.14159265358979323846;
inline const float* lowFreqs() { static const float f[] { 20, 35, 50, 70, 100, 150, 200, 300, 500, 700, 1000 }; return f; }
inline const float* midFreqs() { static const float f[] { 200, 300, 500, 700, 1000, 1500, 2000, 3000, 4000, 5000, 7000 }; return f; }
inline const float* hiFreqs()  { static const float f[] { 1500, 2000, 3000, 4000, 5000, 6400, 8000, 10000, 12000, 16000, 20000 }; return f; }
inline const float* dsFreqs()  { static const float f[] { 3000, 6000, 9000, 12000 }; return f; }      // indice 4 = LIMIT 10:1
// especificacion del compresor: ataque 4-70 ms, release 0.3 / 0.5 / 1 / 2 / 5 s
inline const float* attackMs() { static const float f[] { 4, 8, 15, 30, 70 }; return f; }              // FAST .. SLOW
inline const float* releaseS() { static const float f[] { 0.3f, 0.5f, 1.0f, 2.0f, 5.0f }; return f; }
constexpr double lowQ = 0.6, midQ = 1.1, hiQ = 0.8;

// umbrales calibrados con la especificacion: con THRESHOLD al maximo y un seno de +4 dBu a 1 kHz
// el compresor reduce 8 dB y el limitador 5 dB (0 dBFS = +22 dBu, +4 dBu = -18 dBFS).
inline double compThrDb (double knob) { return -3.0 * knob; }             // 10 -> -30 dBFS
inline double limitThrDb (double knob) { return -2.3556 * knob; }         // 10 -> -23.6 dBFS
inline double deessThrDb (double knob) { return -6.0 - 3.4 * knob; }

// ---------------------------------------------------------------------------------------------
// filtros a partir del prototipo analogico (bilineal con pre-warp), en doble precision
struct OnePole
{
    double b0 = 1, b1 = 0, a1 = 0, s = 0;
    void reset() { s = 0; }
    inline double process (double x) noexcept { const double y = b0 * x + s; s = b1 * x - a1 * y; return y; }
    void setAnalog (double B0, double B1, double A0, double A1, double fs, double fw)
    {
        const double w = 2 * pi * fw, c = w / std::tan (w / (2 * fs));
        const double n0 = B0 * c + B1, n1 = -B0 * c + B1, d0 = A0 * c + A1, d1 = -A0 * c + A1;
        b0 = n0 / d0; b1 = n1 / d0; a1 = d1 / d0;
    }
    void setIdentity() { b0 = 1; b1 = 0; a1 = 0; }
};
struct Bq
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    inline double process (double x) noexcept
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void setAnalog (double B0, double B1, double B2, double A0, double A1, double A2, double fs, double fw)
    {
        const double w = 2 * pi * fw, c = w / std::tan (w / (2 * fs)), c2 = c * c;
        const double n0 = B0 * c2 + B1 * c + B2, n1 = 2 * (B2 - B0 * c2), n2 = B0 * c2 - B1 * c + B2;
        const double d0 = A0 * c2 + A1 * c + A2, d1 = 2 * (A2 - A0 * c2), d2 = A0 * c2 - A1 * c + A2;
        b0 = n0 / d0; b1 = n1 / d0; b2 = n2 / d0; a1 = d1 / d0; a2 = d2 / d0;
    }
    void setIdentity() { b0 = 1; b1 = b2 = a1 = a2 = 0; }
    // pico / valle del EQ pasivo: H(s) = (s^2 + s w A/Q + w^2) / (s^2 + s w /(A Q) + w^2)
    void setPeak (double fs, double f0, double q, double db)
    {
        const double A = std::pow (10.0, db / 40.0), w = 2 * pi * f0;
        setAnalog (1, w * A / q, w * w, 1, w / (A * q), w * w, fs, f0);
    }
};

struct EqSettings
{
    bool on = true; int lowF = 4, midF = 4, hiF = 7, hpf = 0; float low = 0, mid = 0, hi = 0;
    bool operator== (const EqSettings& o) const { return on == o.on && lowF == o.lowF && midF == o.midF && hiF == o.hiF && hpf == o.hpf && low == o.low && mid == o.mid && hi == o.hi; }
};
inline double hpfHz (int m) { return m == 1 ? 80.0 : 120.0; }
// respuesta analogica exacta del EQ (HPF 6 dB/oct + 3 bandas pasivas)
inline std::complex<double> eqS (const EqSettings& s, double f)
{
    const std::complex<double> jw (0.0, 2 * pi * f);
    std::complex<double> h (1.0, 0.0);
    if (s.hpf > 0) { const double w = 2 * pi * hpfHz (s.hpf); h *= jw / (jw + w); }
    auto peak = [&jw] (double f0, double q, double db)
    {
        const double A = std::pow (10.0, db / 40.0), w = 2 * pi * f0;
        return (jw * jw + jw * (w * A / q) + w * w) / (jw * jw + jw * (w / (A * q)) + w * w);
    };
    if (s.on)
    {
        if (s.low > 0.01f) h *= peak (lowFreqs()[juce::jlimit (0, 10, s.lowF)], lowQ, s.low);
        if (s.mid > 0.01f) h *= peak (midFreqs()[juce::jlimit (0, 10, s.midF)], midQ, -s.mid);
        if (s.hi > 0.01f)  h *= peak (hiFreqs()[juce::jlimit (0, 10, s.hiF)], hiQ, s.hi);
    }
    return h;
}
inline float responseDb (const EqSettings& s, double f) { return (float) (20.0 * std::log10 (std::abs (eqS (s, f)) + 1e-12)); }

struct EqFilters
{
    OnePole hpf; Bq low, mid, hi;
    void reset() { hpf.reset(); low.reset(); mid.reset(); hi.reset(); }
    // cambia coeficientes conservando el estado (sin clics)
    void design (const EqSettings& s, double fs)
    {
        if (s.hpf > 0) { const double w = 2 * pi * hpfHz (s.hpf); hpf.setAnalog (1, 0, 1, w, fs, hpfHz (s.hpf)); } else hpf.setIdentity();
        low.setIdentity(); mid.setIdentity(); hi.setIdentity();
        if (! s.on) return;
        if (s.low > 0.01f) low.setPeak (fs, lowFreqs()[juce::jlimit (0, 10, s.lowF)], lowQ, s.low);
        if (s.mid > 0.01f) mid.setPeak (fs, midFreqs()[juce::jlimit (0, 10, s.midF)], midQ, -s.mid);
        if (s.hi > 0.01f)  hi.setPeak (fs, hiFreqs()[juce::jlimit (0, 10, s.hiF)], hiQ, s.hi);
    }
    inline double bands (double x) noexcept { return hi.process (mid.process (low.process (x))); }
};

// ---------------------------------------------------------------------------------------------
// VALVULAS calibradas con la especificacion del VOXBOX:
//   0 dBFS = +22 dBu (-18 dBFS = +4 dBu).  v = 1 dentro del modelo = +30 dBu.
//   THD preamp 1 kHz a +20 dBu ~0.3 %, salida maxima ~+30 dBu a 1.5 % THD.
//   GAIN 40..60 cambia la realimentacion negativa global (20 dB en pasos de 5): 40 limpio y recorte
//   duro, 60 casi sin realimentacion (mas 2do armonico, recorte mas suave). Nivel +5 dB por paso.
// Las curvas se integran con ADAA de 1er orden (antiderivada en tabla): saturar fuerte no deja aliasing.
// ---------------------------------------------------------------------------------------------
constexpr float dbuRef = 22.0f;
constexpr double inScale = 0.39810717055349725;                      // 10^((22-30)/20)

inline double softClip (double y, double t, double c) noexcept
{
    const double a = std::abs (y); if (a <= t) return y;
    const double w = c - t; return std::copysign (t + w * std::tanh ((a - t) / w), y);
}
// y = u + a2 u^2 - a3 u^3 con recorte suave asimetrico; u0/u1 limitan la rejilla antes de que el polinomio se doble
struct TubeChar { double a2 = 0.0158, a3 = 0.040, tp = 0.87, tn = 0.79, cp = 1.10, cn = 1.04, u0 = 2.0, u1 = 2.8; };
inline double tubeCurve (const TubeChar& c, double u) noexcept
{
    u = softClip (u, c.u0, c.u1);
    const double y = u + c.a2 * u * u - c.a3 * u * u * u;
    return y >= 0.0 ? softClip (y, c.tp, c.cp) : softClip (y, c.tn, c.cn);
}
inline TubeChar foldGuard (TubeChar c)
{
    const double um = (2 * c.a2 + std::sqrt (4 * c.a2 * c.a2 + 12 * c.a3)) / (6 * c.a3);   // pendiente 0 del polinomio
    c.u0 = 0.7 * um; c.u1 = 0.95 * um; return c;
}
inline TubeChar preChar (int gainIdx)
{
    static const double d[5] { 0.3162, 0.5623, 1.0, 1.7783, 3.1623 };          // menos realimentacion = mas distorsion
    const int g = juce::jlimit (0, 4, gainIdx);
    TubeChar c; c.a2 *= d[g]; c.a3 *= d[g];
    const double soft = g / 4.0;                                                // 60: codo mas suave y mas temprano
    c.tp = 0.97 - 0.2 * soft; c.tn = c.tp - 0.08;
    return foldGuard (c);
}
inline TubeChar lineChar() { TubeChar c; c.a2 = 0.0044; c.a3 = 0.010; c.tp = 1.25; c.tn = 1.18; c.cp = 1.45; c.cn = 1.38; return foldGuard (c); }   // 5751 + 6414

class CurveTable
{
public:
    static constexpr double X = 12.0, h = 1.0 / 256.0, invH = 256.0;
    void build (const TubeChar& c)
    {
        ch = c;
        n = (int) std::lround (2.0 * X / h);
        F.assign ((size_t) n + 1, 0.0); f.assign ((size_t) n + 1, 0.0);
        f[0] = tubeCurve (ch, -X);
        double acc = 0.0;
        for (int i = 1; i <= n; ++i)
        {
            const double x0 = -X + (i - 1) * h;
            const double fm = tubeCurve (ch, x0 + 0.5 * h), f1 = tubeCurve (ch, x0 + h);
            acc += h / 6.0 * (f[(size_t) i - 1] + 4.0 * fm + f1);                  // Simpson
            F[(size_t) i] = acc; f[(size_t) i] = f1;
        }
    }
    inline double integral (double x) const noexcept
    {
        if (x <= -X) return F[0] + f[0] * (x + X);
        if (x >= X) return F[(size_t) n] + f[(size_t) n] * (x - X);
        const double pos = (x + X) * invH;
        int i = (int) pos; if (i >= n) i = n - 1;
        const double t = pos - i, t2 = t * t, t3 = t2 * t;
        const size_t k = (size_t) i;
        return (2 * t3 - 3 * t2 + 1) * F[k] + (t3 - 2 * t2 + t) * h * f[k] + (-2 * t3 + 3 * t2) * F[k + 1] + (t3 - t2) * h * f[k + 1];
    }
    inline double curve (double x) const noexcept { return tubeCurve (ch, x); }
private:
    TubeChar ch; int n = 0;
    std::vector<double> F, f;
};
// tablas compartidas (se construyen una sola vez)
inline const CurveTable& preTable (int g)
{
    static const std::array<CurveTable, 5> t = [] { std::array<CurveTable, 5> a; for (int i = 0; i < 5; ++i) a[(size_t) i].build (preChar (i)); return a; }();
    return t[(size_t) juce::jlimit (0, 4, g)];
}
inline const CurveTable& lineTable() { static const CurveTable t = [] { CurveTable c; c.build (lineChar()); return c; }(); return t; }

// condensador de acople (paso alto de 1er orden)
class CapHP
{
public:
    void prepare (double sr, double fc) { const double k = std::tan (pi * fc / sr); n = 1.0 / (1.0 + k); a1 = (k - 1.0) / (1.0 + k); x1 = y1 = 0; }
    void reset() { x1 = y1 = 0; }
    inline double process (double x) noexcept { const double y = n * (x - x1) - a1 * y1; x1 = x; y1 = y; return y; }
private:
    double n = 1, a1 = 0, x1 = 0, y1 = 0;
};
// transformador: saturacion por flujo magnetico (solo graves profundos a nivel alto)
class Transformer
{
public:
    void prepare (double sr, double fluxCorner, double amount) { a = std::exp (-2.0 * pi * fluxCorner / sr); k = amount; flux = 0; }
    inline double process (double v) noexcept { flux = a * flux + (1.0 - a) * v; return v - k * flux * flux * flux; }
    void reset() { flux = 0; }
private:
    double a = 0.999, k = 0, flux = 0;
};
// etapa a valvula con ADAA y condensador de acople
class Tube
{
public:
    void prepare (double fs) { cap.prepare (fs, 1.5); reset(); }
    void reset() { cap.reset(); x1 = 0; F1 = tab != nullptr ? tab->integral (0.0) : 0.0; }
    void set (const CurveTable* t) { tab = t; F1 = tab->integral (x1); }
    inline double process (double u) noexcept
    {
        const double Fu = tab->integral (u), dx = u - x1;
        const double y = std::abs (dx) > 1.0e-4 ? (Fu - F1) / dx : tab->curve (0.5 * (u + x1));
        x1 = u; F1 = Fu;
        return cap.process (y);
    }
private:
    const CurveTable* tab = nullptr;
    double x1 = 0, F1 = 0;
    CapHP cap;
};
// el ADAA promedia media muestra por etapa (-0.12 dB a 20 kHz a 8x): este FIR de fase lineal lo devuelve a plano (a = etapas / 8)
class AdaaEq
{
public:
    void reset() { z1 = z2 = 0; }
    inline double process (double x, double a) noexcept { const double y = -a * x + (1 + 2 * a) * z1 - a * z2; z2 = z1; z1 = x; return y; }
private:
    double z1 = 0, z2 = 0;
};

// preamp completo: trafo de entrada -> 12AX7LPS + 6414 (realimentacion global) -> trafo de salida
class Preamp
{
public:
    void prepare (double sr) { inT.prepare (sr, 20.0, 0.3); outT.prepare (sr, 20.0, 1.0); tube.prepare (sr); tube.set (&preTable (2)); dc.prepare (sr, 2.0); }
    void set (int gainIdx) { tube.set (&preTable (gainIdx)); }
    void reset() { inT.reset(); outT.reset(); tube.reset(); dc.reset(); }
    // x en escala digital; drive = ganancia del paso de GAIN (50 = 1)
    inline double process (double x, double drive) noexcept
    {
        double v = inT.process (x * inScale);
        v = tube.process (v * drive);
        v = outT.process (v);
        return dc.process (v) / inScale;
    }
private:
    Transformer inT, outT; Tube tube; CapHP dc;
};

// etapa final (5751 + 6414, recupera la perdida del EQ pasivo) + trafo de salida opcional (XFMR)
class LineAmp
{
public:
    void prepare (double sr)
    {
        tube.prepare (sr); tube.set (&lineTable()); xf.prepare (sr, 20.0, 1.6); dc.prepare (sr, 2.0);
        const double w = 2 * pi * 42000.0;
        air.setAnalog (0, 0, w * w, 1, w / 0.6, w * w, sr, 42000.0); air.reset();
    }
    void reset() { tube.reset(); xf.reset(); dc.reset(); air.reset(); }
    inline double process (double x, bool xfmr) noexcept
    {
        double v = tube.process (x * inScale);
        if (xfmr) v = air.process (xf.process (v));
        return dc.process (v) / inScale;
    }
private:
    Tube tube; Transformer xf; CapHP dc; Bq air;
};

// ---------------------------------------------------------------------------------------------
// ELOP (celda optica: LED + fotorresistencia) con compresion "parametrica" de cuatro constantes de tiempo:
//   1. ataque segun ATTACK (4-70 ms), mas rapido cuanto mas fuerte es el pico y la reduccion
//   2. release rapido del pico (unas 12 veces el ataque): los golpes cortos se recuperan rapido
//   3. "plataforma" que se carga con la compresion sostenida y se suelta con RELEASE (0.3-5 s);
//      el ataque cabalga encima de esa plataforma, y RELEASE tambien cambia como se carga (interaccion)
//   4. memoria de la celda: cuanto mas tiempo comprime, mas lento vuelve; cerca de 0 dB frena suave
// ---------------------------------------------------------------------------------------------
class Elop
{
public:
    void prepare (double rate) { fs = rate; reset(); }
    void reset() { pk = pl = mem = rms = peak = cell = 0; }
    void set (double thrDb, double ratio, double attMs, double relS, int relIdx, double maxGr)
    {
        thr = thrDb; r = std::max (1.0, ratio); mx = maxGr;
        tAtt = std::max (0.5, attMs) * 0.001 * (0.85 + 0.075 * juce::jlimit (0, 4, relIdx));
        aPkRel = coef (tAtt * 12.0);
        tPlUp = tAtt * 10.0 + 0.04 * relS;
        tPlDn = relS * 0.45;
        aRms = coef (0.005); aPk = coef (0.030);
        aMemUp = coef (2.5); aMemDn = coef (6.0); aCell = coef (0.0008);
    }
    double curve (double lv) const noexcept
    {
        const double knee = 6.0, o = lv - thr;
        double gr = 0;
        if (o > knee * 0.5) gr = (1 - 1 / r) * o;
        else if (o > -knee * 0.5) { const double k = o + knee * 0.5; gr = (1 - 1 / r) * k * k / (2 * knee); }
        const double s = 3.0, b = mx - s;                                    // la celda llega a su minimo de resistencia suave
        return gr > b ? b + s * std::tanh ((gr - b) / s) : gr;
    }
    // sc en escala digital; devuelve la reduccion en dB (positiva)
    inline double process (double sc) noexcept
    {
        rms = aRms * rms + (1 - aRms) * sc * sc;
        const double a = std::abs (sc);
        peak = a > peak ? a : aPk * peak + (1 - aPk) * a;
        const double det = 0.7 * std::sqrt (2.0 * rms) + 0.3 * peak;
        const double gr = curve (20.0 * std::log10 (det + 1.0e-9));
        // 1-2: pico
        if (gr > pk) { const double t = tAtt / (1.0 + 0.12 * (gr - pk)); pk += (gr - pk) * step (t); }
        else pk = aPkRel * pk + (1 - aPkRel) * gr;
        // 3-4: plataforma con memoria y frenado cerca de 0 dB
        const double m = std::min (1.0, mem / 8.0);
        if (gr > pl) pl += (gr - pl) * step (tPlUp);
        else pl += (gr - pl) * step (tPlDn * (1.0 + m) * (1.0 + 1.5 * std::exp (-pl / 1.5)));
        const double env = std::max (pk, pl);
        mem += (env - mem) * (env > mem ? 1 - aMemUp : 1 - aMemDn);
        cell = aCell * cell + (1 - aCell) * env;                             // inercia de la LDR
        return cell;
    }
    double reduction() const { return cell; }
private:
    double coef (double t) const { return std::exp (-1.0 / (fs * t)); }
    inline double step (double t) const noexcept { const double x = 1.0 / (fs * t); return x < 0.01 ? x * (1.0 - 0.5 * x) : 1.0 - std::exp (-x); }
    double fs = 384000.0, thr = 0, r = 3, mx = 16, tAtt = 0.008, aPkRel = 0.99, tPlUp = 0.1, tPlDn = 0.2;
    double aRms = 0, aPk = 0, aMemUp = 0, aMemDn = 0, aCell = 0;
    double pk = 0, pl = 0, mem = 0, rms = 0, peak = 0, cell = 0;
};

// de-esser pasivo: red LC en serie con la fotorresistencia, en derivacion a masa.
//   H(s) = (s^2 + s w g / (Q0 (1-g)) + w^2) / (s^2 + s w / (Q0 (1-g)) + w^2),  g = ganancia en el centro
// con poca reduccion la muesca es ancha y suave; al bajar mas se hace mas profunda y mas estrecha (como el LC real).
// Filtro de variable de estado (TPT): admite cambiar la profundidad en cada muestra sin clics.
class LcNotch
{
public:
    void prepare (double fs, double f0, double q0) { gw = std::tan (pi * f0 / fs); Q0 = q0; reset(); }
    void reset() { s1 = s2 = 0; }
    inline double process (double x, double g) noexcept
    {
        g = juce::jlimit (1.0e-3, 0.9999, g);
        const double k = 1.0 / (Q0 * (1.0 - g));
        const double a1 = 1.0 / (1.0 + gw * (gw + k)), a2 = gw * a1, a3 = gw * a2;
        const double v3 = x - s2, v1 = a1 * s1 + a2 * v3, v2 = s2 + a2 * s1 + a3 * v3;
        s1 = 2 * v1 - s1; s2 = 2 * v2 - s2;
        return x - (1.0 - g) * k * v1;                                         // x - (1-g) k * pasa-banda
    }
private:
    double gw = 0.1, Q0 = 1.0, s1 = 0, s2 = 0;
};
} // namespace vbx
