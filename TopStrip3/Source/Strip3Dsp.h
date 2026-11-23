#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <complex>
#include <algorithm>
#include <vector>

// TOP STRIP 737: modelo del canal de valvulas clase A (preamp + compresor optico + EQ discreto)
// calibrado con las especificaciones publicadas del hardware (hoja de datos y manual de operacion):
//   - preamp: dos triodos 6922 en cascada con realimentacion minima, THD/IMD 0.5 % justo antes del recorte,
//     salida maxima +30 dBu, respuesta 10 Hz - 40 kHz.  0 VU = +4 dBu.
//   - HIGH GAIN: +8 dB en LINE, +18 dB en MIC / INSTRUMENT.  INPUT: LINE / INSTRUMENT (Hi-Z) / MIC (trafo).
//   - pasa altos variable 30-140 Hz, 6 dB/oct.
//   - compresor optico pasivo (LED + LDR) con triodo de ganancia: umbral -30..+20, ratio 1:1..20:1,
//     ataque 2-200 ms, release 100 ms - 5 s (dependiente del programa: la celda tiene memoria).
//   - EQ: BASS shelf pasivo 15/30/60/150 Hz +-24 dB, LOW MID 35-450 Hz (x10: 350 Hz-4.5 kHz) +-16 dB,
//     HIGH MID 220 Hz-2.8 kHz (x10: 2.2-28 kHz) +-16 dB, HI-Q: Q 0.2 -> 0.85, TREBLE shelf pasivo
//     10/15/20/32 kHz +-20 dB.  SC->MIDS: las bandas medias solo actuan en el sidechain. EQ->COMP: EQ antes.
// Todo el camino no lineal corre a 8x con filtros de -110 dB y en doble precision.
namespace s737c
{
constexpr double pi = 3.14159265358979323846;
constexpr double dbuRef = 22.0;                     // 0 dBFS = +22 dBu  (-18 dBFS = +4 dBu = 0 VU)
constexpr double inScale = 0.39810717055349720;    // 10^((22-30)/20): v = 1 en el modelo = +30 dBu (recorte)

// ---------------------------------------------------------------------------------------------
// filtros en doble precision a partir del prototipo analogico (bilineal con pre-warp)
struct OnePole
{
    double b0 = 1, b1 = 0, a1 = 0, s = 0;
    void reset() { s = 0; }
    inline double process (double x) noexcept { const double y = b0 * x + s; s = b1 * x - a1 * y; return y; }
    // H(s) = (B0 s + B1) / (A0 s + A1), pre-warp en fw
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
    // H(s) = (B0 s^2 + B1 s + B2) / (A0 s^2 + A1 s + A2), pre-warp en fw
    void setAnalog (double B0, double B1, double B2, double A0, double A1, double A2, double fs, double fw)
    {
        const double w = 2 * pi * fw, c = w / std::tan (w / (2 * fs)), c2 = c * c;
        const double n0 = B0 * c2 + B1 * c + B2, n1 = 2 * (B2 - B0 * c2), n2 = B0 * c2 - B1 * c + B2;
        const double d0 = A0 * c2 + A1 * c + A2, d1 = 2 * (A2 - A0 * c2), d2 = A0 * c2 - A1 * c + A2;
        b0 = n0 / d0; b1 = n1 / d0; b2 = n2 / d0; a1 = d1 / d0; a2 = d2 / d0;
    }
    void setIdentity() { b0 = 1; b1 = b2 = a1 = a2 = 0; }
};

// ---- prototipos analogicos (funciones de transferencia del circuito) ----
// shelf pasivo de 1er orden: la mitad de la ganancia (en dB) cae justo en la frecuencia del panel
inline std::complex<double> lowShelfS (std::complex<double> s, double f, double db)
{
    const double g = std::pow (10.0, db / 20.0), w = 2 * pi * f, r = std::sqrt (g);
    return (s + w * r) / (s + w / r);
}
inline std::complex<double> highShelfS (std::complex<double> s, double f, double db)
{
    const double g = std::pow (10.0, db / 20.0), w = 2 * pi * f, r = std::sqrt (g);
    return (g * s + w * r) / (s + w * r);
}
// campana activa (realimentacion): Q 0.2 ancho / 0.85 HI-Q
inline std::complex<double> bellS (std::complex<double> s, double f, double q, double db)
{
    const double a = std::pow (10.0, db / 40.0), w = 2 * pi * f;
    return (s * s + s * (a / q) * w + w * w) / (s * s + s * w / (a * q) + w * w);
}
inline std::complex<double> hpfS (std::complex<double> s, double f) { return s / (s + 2 * pi * f); }

inline void setLowShelf (OnePole& p, double fs, double f, double db)
{
    if (std::abs (db) < 1.0e-3) { p.setIdentity(); return; }
    const double g = std::pow (10.0, db / 20.0), w = 2 * pi * f, r = std::sqrt (g);
    p.setAnalog (1.0, w * r, 1.0, w / r, fs, f);
}
inline void setHighShelf (OnePole& p, double fs, double f, double db)
{
    if (std::abs (db) < 1.0e-3) { p.setIdentity(); return; }
    const double g = std::pow (10.0, db / 20.0), w = 2 * pi * f, r = std::sqrt (g);
    p.setAnalog (g, w * r, 1.0, w * r, fs, std::min (f, fs * 0.2));
}
inline void setBell (Bq& b, double fs, double f, double q, double db)
{
    if (std::abs (db) < 1.0e-3) { b.setIdentity(); return; }
    const double a = std::pow (10.0, db / 40.0), w = 2 * pi * f;
    b.setAnalog (1.0, (a / q) * w, w * w, 1.0, w / (a * q), w * w, fs, std::min (f, fs * 0.2));
}
inline void setHpf (OnePole& p, double fs, double f) { p.setAnalog (1.0, 0.0, 1.0, 2 * pi * f, fs, f); }
inline void setLpf (OnePole& p, double fs, double f) { p.setAnalog (0.0, 2 * pi * f, 1.0, 2 * pi * f, fs, std::min (f, fs * 0.2)); }

// ---------------------------------------------------------------------------------------------
// V2: respuesta de cada entrada medida con los impulsos del aparato (9 capturas, 48 kHz), relativa a LINE.
// Solo se guardan los numeros ajustados (polos de 1er orden, error rms 0.03 dB), nunca los archivos.
//   MIC:        paso bajo 16.9 kHz (igual en ganancia alta y baja)
//   INSTRUMENT: paso alto 3.2 Hz; paso bajo 21.1 kHz con HIGH GAIN, 46 kHz sin HIGH GAIN
//   LINE:       plana (referencia)
// Las capturas tambien confirman: pasa altos de 1er orden (6 dB/oct) y la forma del EQ (shelves de 1er orden con la
// mitad de la ganancia en la frecuencia del panel, campanas Q 0.2 / 0.85): 3 capturas con EQ, error rms 0.06-0.12 dB.
struct InputVoicing { double lp = 0, hp = 0; };
inline InputVoicing inputVoicing (int mode, bool highGain)
{
    if (mode == 2) return { 16900.0, 0.0 };
    if (mode == 1) return { highGain ? 21100.0 : 46000.0, 3.2 };
    return {};
}

// ---------------------------------------------------------------------------------------------
// circuito no lineal
inline double softClip (double y, double t, double c) noexcept
{
    const double a = std::abs (y);
    if (a <= t) return y;
    const double w = c - t;
    return std::copysign (t + w * std::tanh ((a - t) / w), y);
}

// condensador de acople (paso alto de 1er orden, sin limite inferior)
class CapHP
{
public:
    void prepare (double fs, double fc) { const double k = std::tan (pi * fc / fs); n = 1.0 / (1.0 + k); a1 = (k - 1.0) / (1.0 + k); x1 = y1 = 0; }
    void reset() { x1 = y1 = 0; }
    inline double process (double x) noexcept { const double y = n * (x - x1) - a1 * y1; x1 = x; y1 = y; return y; }
private:
    double n = 1, a1 = 0, x1 = 0, y1 = 0;
};

// trafo de entrada de microfono: saturacion por flujo (solo graves profundos a nivel alto)
class Transformer
{
public:
    void prepare (double fs, double fluxCorner, double amount) { a = std::exp (-2 * pi * fluxCorner / fs); k = amount; flux = 0; }
    void reset() { flux = 0; }
    inline double process (double v) noexcept { flux = a * flux + (1 - a) * v; return v - k * flux * flux * flux; }
private:
    double a = 0.999, k = 0, flux = 0;
};

// etapa de valvula clase A: y = u + a2 u^2 - a3 u^3 (2do armonico dominante) + recorte suave asimetrico
struct TubeChar { double a2 = 0.0075, a3 = 0.0080, tp = 0.93, tn = 0.89, cp = 1.12, cn = 1.07; };

inline TubeChar preChar (int inputMode, bool highGain)
{
    TubeChar c;                                         // dos 6922 en cascada, realimentacion minima
    if (inputMode == 1) c.a2 *= 1.30;                   // INSTRUMENT: entrada Hi-Z, mas pares
    if (inputMode == 2) c.a2 *= 1.15;                   // MIC: mas ganancia interna
    if (highGain) { c.a2 *= 1.6; c.a3 *= 1.3; }         // HIGH GAIN: menos realimentacion
    return c;
}
inline TubeChar compChar() { TubeChar c; c.a2 = 0.0030; c.a3 = 0.0030; c.tp = 1.05; c.tn = 1.00; c.cp = 1.25; c.cn = 1.20; return c; }  // triodo de ganancia del opto
inline TubeChar outChar()  { TubeChar c; c.a2 = 0.0025; c.a3 = 0.0030; c.tp = 1.00; c.tn = 0.96; c.cp = 1.20; c.cn = 1.15; return c; }  // etapa de salida

// curva estatica completa de la etapa: rejilla -> triodo (polinomio) -> recorte suave asimetrico
inline double tubeCurve (const TubeChar& ch, double u) noexcept
{
    u = softClip (u, 2.0, 3.0);                           // la rejilla deja de seguir mucho antes de que el polinomio se doble
    const double y = u + ch.a2 * u * u - ch.a3 * u * u * u;
    return y >= 0.0 ? softClip (y, ch.tp, ch.cp) : softClip (y, ch.tn, ch.cn);
}

// tabla de la antiderivada de la curva (para anti-aliasing por antiderivada, ADAA de 1er orden):
// el recorte se integra de forma continua entre muestras, asi la saturacion fuerte no genera aliasing
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
            acc += h / 6.0 * (f[(size_t) i - 1] + 4.0 * fm + f1);           // Simpson
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

class Tube
{
public:
    void prepare (double fs) { cap.prepare (fs, 1.0); reset(); }
    void reset() { cap.reset(); x1 = 0; F1 = tab != nullptr ? tab->integral (0.0) : 0.0; }
    void set (const CurveTable* t) { tab = t; F1 = tab->integral (x1); }
    inline double process (double u) noexcept
    {
        const double Fu = tab->integral (u), dx = u - x1;
        const double y = std::abs (dx) > 1.0e-3 ? (Fu - F1) / dx : tab->curve (0.5 * (u + x1));
        x1 = u; F1 = Fu;
        return cap.process (y);
    }
private:
    const CurveTable* tab = nullptr;
    double x1 = 0, F1 = 0;
    CapHP cap;
};

// el ADAA promedia media muestra por etapa (-0.12 dB a 20 kHz a 8x): este FIR de fase lineal lo devuelve a plano
class AdaaEq
{
public:
    void reset() { z1 = z2 = 0; }
    inline double process (double x, double a) noexcept { const double y = -a * x + (1 + 2 * a) * z1 - a * z2; z2 = z1; z1 = x; return y; }
private:
    double z1 = 0, z2 = 0;
};

// ---------------------------------------------------------------------------------------------
// compresor optico: el sidechain enciende un LED, la LDR (fotorresistencia) atenua de forma pasiva.
//   - deteccion mezcla RMS / pico, umbral relativo a 0 VU (+4 dBu)
//   - codo suave que se estrecha con la ratio (opto suave a 2:1, firme a 20:1)
//   - ataque segun ATTACK y mas rapido cuanto mas fuerte es el transitorio (el LED brilla mas)
//   - release en dos etapas: una rapida y una lenta cuya duracion crece con la compresion sostenida
//     (memoria de la celda): frases largas y comprimidas se recuperan mas despacio, golpes cortos rapido.
class Opto
{
public:
    void prepare (double rate) { fs = rate; reset(); }
    void reset() { env = fast = slow = mem = rms = peak = cell = 0; }
    void set (double thrDb, double ratio, double attMs, double relMs)
    {
        thr = thrDb + 4.0;                              // dBu
        r = std::max (1.0, ratio);
        knee = 10.0 - 6.0 * (r - 1.0) / 19.0;
        tAtt = std::max (0.5, attMs) * 0.001 / 1.5;
        const double rel = std::max (0.05, relMs * 0.001);
        aRelFast = std::exp (-1.0 / (fs * rel * 0.15));
        relSlow = rel * 0.35;
        aRms = std::exp (-1.0 / (fs * 0.005));
        aPk = std::exp (-1.0 / (fs * 0.030));
        aMemUp = std::exp (-1.0 / (fs * 2.5));
        aMemDn = std::exp (-1.0 / (fs * 6.0));
        aCell = std::exp (-1.0 / (fs * 0.0008));
    }
    // sc en unidades del modelo (v = 1 = +30 dBu); devuelve la reduccion en dB (positiva)
    inline double process (double sc) noexcept
    {
        rms = aRms * rms + (1 - aRms) * sc * sc;
        const double a = std::abs (sc);
        peak = a > peak ? a : aPk * peak + (1 - aPk) * a;
        const double det = 0.75 * std::sqrt (2.0 * rms) + 0.25 * peak;
        const double lv = 20.0 * std::log10 (det + 1.0e-9) + 30.0;          // dBu
        const double o = lv - thr;
        double gr = 0;
        if (o > knee * 0.5) gr = (1 - 1 / r) * o;
        else if (o > -knee * 0.5) { const double k = o + knee * 0.5; gr = (1 - 1 / r) * k * k / (2 * knee); }
        gr = std::min (gr, 40.0);

        if (gr > env)
        {
            const double t = tAtt / (1.0 + 0.08 * std::max (0.0, gr - env));
            const double aA = std::exp (-1.0 / (fs * t));
            env = aA * env + (1 - aA) * gr;
            fast = slow = env;
        }
        else
        {
            const double m = std::min (1.0, mem / 8.0), w = 0.3 + 0.5 * m;
            const double aS = std::exp (-1.0 / (fs * relSlow * (1.0 + 3.0 * m)));
            fast = aRelFast * fast + (1 - aRelFast) * gr;
            slow = aS * slow + (1 - aS) * gr;
            env = std::max (fast, w * slow + (1 - w) * gr);
        }
        const double aM = env > mem ? aMemUp : aMemDn;
        mem = aM * mem + (1 - aM) * env;
        cell = aCell * cell + (1 - aCell) * env;                             // inercia de la LDR
        return cell;
    }
    double reduction() const { return cell; }
private:
    double fs = 384000.0, thr = 4, r = 3, knee = 8, tAtt = 0.01, aRelFast = 0.99, relSlow = 0.2;
    double aRms = 0, aPk = 0, aMemUp = 0, aMemDn = 0, aCell = 0;
    double env = 0, fast = 0, slow = 0, mem = 0, rms = 0, peak = 0, cell = 0;
};
} // namespace s737c
