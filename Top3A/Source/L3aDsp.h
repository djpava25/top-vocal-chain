#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

// TOP 3A: nivelador optico de estado solido (estilo LA-3A), en doble precision y a 4x.
//
// Cadena (como el diagrama de bloques del equipo):
//   trafo de entrada -> ATENUADOR OPTICO (resistencia en serie + fotorresistencia a masa) -> amplificador de entrada
//   -> (aqui sale la cadena lateral: por eso GAIN no cambia la compresion) -> potenciometro GAIN -> etapa de salida
//   clase A -> trafo de salida.
//   Cadena lateral (realimentacion): PEAK REDUCTION -> filtro (los agudos comprimen menos; HF EMPH los refuerza)
//   -> excitador -> panel electroluminiscente (EL) -> luz -> fotorresistencia.
//
// Celda T4 (lo que da el sonido):
//  * Panel EL: el brillo sigue la ENVOLVENTE de la tension (red de 90 grados -> envolvente exacta, sin rizado de
//    rectificador), con ley ~V^4 (rodilla blanda y ~3:1), saturacion del brillo y un pequeno parpadeo a 2f (el
//    panel enciende en cada semiciclo: produce la distorsion de graves que da la especificacion, <0.7% a 50 Hz).
//  * Fotorresistencia de CdS: generacion por la luz + recombinacion bimolecular (n^2). Eso da, por fisica:
//      - ataque que depende del nivel: picos grandes -> mucha luz -> la resistencia cae en ~1 ms; picos chicos -> lento
//      - liberacion en dos etapas: rapida al principio (~50% en 60-150 ms) y una cola lenta de segundos
//    + trampas de carga ("memoria"): con compresion larga las trampas se llenan y al soltar devuelven cargas,
//      asi la liberacion es mas lenta despues de comprimir mucho tiempo y mas rapida tras un golpe corto.
//  * COMP / LIMIT: en LIMIT parte de la senal de entrada entra tambien a la cadena lateral (mezcla de
//    realimentacion y alimentacion directa): la relacion sube con la compresion fuerte (hacia ~20-50:1).
namespace l3a
{
constexpr double pi = 3.14159265358979323846;

// ---------------------------------------------------------------------------------------------
// biquad desde el prototipo analogico (bilineal con pre-warp)
struct Bq
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    inline double process (double x) noexcept { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    void setAnalog (double B0, double B1, double B2, double A0, double A1, double A2, double fs, double fw)
    {
        const double w = 2 * pi * fw, c = w / std::tan (w / (2 * fs)), c2 = c * c;
        const double n0 = B0 * c2 + B1 * c + B2, n1 = 2 * (B2 - B0 * c2), n2 = B0 * c2 - B1 * c + B2;
        const double d0 = A0 * c2 + A1 * c + A2, d1 = 2 * (A2 - A0 * c2), d2 = A0 * c2 - A1 * c + A2;
        b0 = n0 / d0; b1 = n1 / d0; b2 = n2 / d0; a1 = d1 / d0; a2 = d2 / d0;
    }
    void setHP2 (double fs, double f, double q) { const double w = 2 * pi * f; setAnalog (1, 0, 0, 1, w / q, w * w, fs, f); }
    void setLP2 (double fs, double f, double q) { const double w = 2 * pi * f; setAnalog (0, 0, w * w, 1, w / q, w * w, fs, f); }
    void setHP1 (double fs, double f) { const double w = 2 * pi * f; setAnalog (0, 1, 0, 0, 1, w, fs, f); }
    // shelf de agudos de 2do orden (ganancia dB arriba de f)
    void setHighShelf (double fs, double f, double q, double db)
    {
        const double A = std::pow (10.0, db / 40.0), w = 2 * pi * f, sa = std::sqrt (A);
        setAnalog (A * A, A * sa / q * w, A * w * w, 1, sa / q * w, A * w * w, fs, f);
    }
};

// ---------------------------------------------------------------------------------------------
// red de 90 grados (dos cadenas de pasatodos de 1er orden): error < 0.6 grados de 10 Hz a 30 kHz.
// a^2 + b^2 = envolvente^2 sin rizado; (a^2 - b^2) / (a^2 + b^2) = cos(2 fase) (parpadeo a 2f)
struct Ap1
{
    double c = 0, x1 = 0, y1 = 0;
    void set (double fp, double fs) { const double t = pi * fp / fs; c = (t - 1) / (t + 1); }
    void reset() { x1 = y1 = 0; }
    inline double process (double x) noexcept { const double y = c * x + x1 - c * y1; x1 = x; y1 = y; return y; }
};
class Quad
{
public:
    void prepare (double fs)
    {
        static const double pa[] { 15.3109, 84.0034, 422.6165, 2120.5145, 10817.5986, 96320.9937 };
        static const double pb[] { 4.1528, 36.9768, 188.6335, 946.4847, 4761.7129, 26125.1361 };
        for (int i = 0; i < 6; ++i) { a[(size_t) i].set (pa[i], fs); b[(size_t) i].set (pb[i], fs); }
        reset();
    }
    void reset() { for (auto& s : a) s.reset(); for (auto& s : b) s.reset(); }
    inline void process (double x, double& ya, double& yb) noexcept
    {
        ya = x; yb = x;
        for (auto& s : a) ya = s.process (ya);
        for (auto& s : b) yb = s.process (yb);
        if (std::abs (ya) < 1e-30) ya = 0;
        if (std::abs (yb) < 1e-30) yb = 0;
    }
private:
    std::array<Ap1, 6> a, b;
};

// ---------------------------------------------------------------------------------------------
// etapa clase A con realimentacion (transistor: curva exponencial, corte suave abajo y saturacion arriba),
// lazo resuelto por biseccion y tabulado con su antiderivada (ADAA de 1er orden: sin aliasing)
struct StageChar { double T = 300, c = 3.5, s = 1.12, p = 1.6; };
inline double classAOpen (const StageChar& k, double e)
{
    const double y = k.c * (std::exp (std::clamp (e / k.c, -60.0, 60.0)) - 1.0);
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
private:
    int n = 0;
    std::vector<double> F, f;
};
// amplificador de entrada (mucha realimentacion, casi transparente) y etapa de salida (+24 dBu, +27 en picos)
inline StageChar inAmpChar() { StageChar k; k.T = 700; k.c = 3.2; k.s = 1.6; k.p = 1.6; return k; }
inline StageChar outAmpChar() { StageChar k; k.T = 95; k.c = 2.6; k.s = 1.12; k.p = 1.9; return k; }
inline const CurveTable& inTable() { static const CurveTable t = [] { CurveTable c; c.build (inAmpChar()); return c; }(); return t; }
inline const CurveTable& outTable() { static const CurveTable t = [] { CurveTable c; c.build (outAmpChar()); return c; }(); return t; }

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
// el ADAA promedia media muestra por etapa: este FIR de fase lineal lo devuelve a plano
class AdaaEq
{
public:
    void reset() { z1 = z2 = 0; }
    inline double process (double x, double a) noexcept { const double y = -a * x + (1 + 2 * a) * z1 - a * z2; z2 = z1; z1 = x; return y; }
private:
    double z1 = 0, z2 = 0;
};

// ---------------------------------------------------------------------------------------------
// transformador: el flujo es la integral de la tension (inductancia del primario): la distorsion sube al bajar la
// frecuencia. Corriente de magnetizacion = histeresis de Rayleigh + saturacion cubica + termino par.
class Transformer
{
public:
    void prepare (double fs, double fL, double kRayleigh, double kCubic, double kEven)
    {
        a = std::exp (-2.0 * pi * fL / fs); kr = kRayleigh; kc = kCubic; ke = kEven; dc.setHP2 (fs, 2.0, 0.6); reset();
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
// celda optica T4: panel EL + fotorresistencia de CdS con trampas
struct CellSpec
{
    double beta = 8.0;        // recombinacion bimolecular (1/s, en unidades de conductancia normalizada)
    double kappa = 8.0;       // generacion por la luz
    double tauR = 2.5;        // recombinacion lineal (cola final)
    double kf = 0.30;         // llenado de trampas
    double ke = 0.70;         // vaciado de trampas
    double Nt = 3.5;          // capacidad de las trampas (memoria)
    double Lsat = 2000.0;     // saturacion del brillo del panel (reduccion maxima ~33 dB)
    double tauEl = 0.00006;   // persistencia del fosforo
    double flicker = 0.20;    // parpadeo a 2f del panel
};
class OptoCell
{
public:
    void prepare (double fs, const CellSpec& s) { sp = s; dt = 1.0 / fs; aEl = 1.0 - std::exp (-dt / sp.tauEl); reset(); }
    void reset() { el = n = m = 0; }
    // L: luz normalizada (1 = rodilla); devuelve la conductancia normalizada (ganancia = 1 / (1 + n))
    inline double step (double L) noexcept
    {
        el += (L - el) * aEl;
        const double Lg = el / (1.0 + el / sp.Lsat);
        n = (n + dt * (sp.kappa * Lg + sp.ke * sp.Nt * m)) / (1.0 + dt * (sp.beta * n + 1.0 / sp.tauR + sp.kf * sp.Nt * (1.0 - m)));
        m = (m + dt * sp.kf * n) / (1.0 + dt * (sp.kf * n + sp.ke));
        if (n < 1e-12) n = 0;
        if (m < 1e-12) m = 0;
        if (el < 1e-20) el = 0;
        return n;
    }
    double conductance() const { return n; }
    double trap() const { return m; }
    CellSpec sp;
private:
    double dt = 1.0 / 192000.0, aEl = 0.01, el = 0, n = 0, m = 0;
};

// ---------------------------------------------------------------------------------------------
// perillas -> valores del circuito
// potenciometro logaritmico: (b^t - 1) / (b - 1)
inline double taper (double t, double b) { t = std::clamp (t, 0.0, 1.0); return (std::pow (b, t) - 1.0) / (b - 1.0); }
// GAIN 0-10: hasta +30 dB (posicion "30 dB" del equipo); ganancia unidad cerca de 3.7
inline double gainLin (double knob) { return std::pow (10.0, 30.0 / 20.0) * taper (knob / 10.0, 200.0); }
inline double gainDb (double knob) { const double g = gainLin (knob); return g > 1e-9 ? 20.0 * std::log10 (g) : -180.0; }
// PEAK REDUCTION 0-10: nivel que llega al excitador del panel
inline double peakLin (double knob) { return taper (knob / 10.0, 4.68); }
// HF EMPH 0-10: refuerzo de agudos en la cadena lateral (hasta +19 dB -> ~10 dB mas de reduccion en 15 kHz)
inline double emphDb (double knob) { return 1.9 * std::clamp (knob, 0.0, 10.0); }

struct Settings
{
    double gain = 4.0, peak = 4.0, emph = 0.0;
    bool limit = false;
};

// ---------------------------------------------------------------------------------------------
// el equipo completo (estereo con una sola celda: imagen estable)
class Leveler
{
public:
    static constexpr double inScale = 0.7943282347242815;        // 0 dBFS = +24 dBu; v = 1 -> +26 dBu
    static constexpr double vEl = 0.0340;                        // tension del excitador en la rodilla (v)
    void prepare (double rate, int numCh)
    {
        fs = rate; chs = std::clamp (numCh, 1, 2);
        cell.prepare (fs, spec);
        const double aSm = 1.0 - std::exp (-1.0 / (0.020 * fs));
        smA = aSm;
        for (auto& c : ch)
        {
            c.inHf.setLP2 (fs, 80000.0, 0.60); c.inLf.setHP2 (fs, 5.0, 0.60);
            c.inT.prepare (fs, 5.0, 0.18, 5.0, 0.02);
            c.cap1.setHP1 (fs, 3.0); c.cap2.setHP2 (fs, 1.5, 0.5);
            c.outT.prepare (fs, 4.0, 0.20, 5.5, 0.06);
            c.outHf.setLP2 (fs, 48000.0, 0.72);
            c.amp1.set (&inTable()); c.amp2.set (&outTable());
            c.qs.prepare (fs); c.qf.prepare (fs);
        }
        setSidechainFilter (0.0);
        reset();
    }
    void reset()
    {
        cell.reset();
        for (auto& c : ch)
        {
            c.inHf.reset(); c.inLf.reset(); c.inT.reset(); c.cap1.reset(); c.cap2.reset(); c.outT.reset(); c.outHf.reset();
            c.amp1.reset(); c.amp2.reset(); c.eq.reset(); c.sc.reset(); c.qs.reset(); c.qf.reset();
        }
        gp = gpT; pr = prT; lam = lamT;
    }
    void set (const Settings& s, bool snap = false)
    {
        gpT = gainLin (s.gain); prT = peakLin (s.peak); lamT = s.limit ? lambdaLimit : 0.0;
        if (std::abs (s.emph - emphNow) > 1e-6) setSidechainFilter (s.emph);
        if (snap) { gp = gpT; pr = prT; lam = lamT; }
    }
    // procesa una muestra por canal (sobremuestreada)
    inline void process (double* x) noexcept
    {
        gp += (gpT - gp) * smA; pr += (prT - pr) * smA; lam += (lamT - lam) * smA;
        const double att = 1.0 / (1.0 + cell.conductance());
        double L = 0.0;
        for (int c = 0; c < chs; ++c)
        {
            auto& k = ch[(size_t) c];
            // entrada: trafo (graves con flujo) -> atenuador optico -> amplificador de entrada
            double u = k.inT.process (k.inLf.process (k.inHf.process (x[c] * inScale)));
            const double s1 = k.cap1.process (k.amp1.process (u * att));
            // cadena lateral: realimentacion desde la salida del amplificador de entrada (+ entrada en LIMIT)
            double a1, b1, a2, b2;
            k.qs.process (k.sc.process (s1) * pr, a1, b1);
            k.qf.process (u * pr * lam, a2, b2);
            const double e2 = a2 * a2 + b2 * b2;
            const double g2 = 1.0 / std::sqrt (1.0 + e2 / (ffMax * ffMax * vEl * vEl));   // el excitador limita la parte directa
            const double A = a1 + g2 * a2, B = b1 + g2 * b2, E2 = A * A + B * B;
            const double cos2 = E2 > 1e-30 ? (A * A - B * B) / E2 : 0.0;
            const double q = E2 * invVel2;
            L = std::max (L, q * q * (1.0 + spec.flicker * cos2));
            // salida: GAIN -> etapa clase A -> trafo de salida
            double o = k.cap2.process (k.amp2.process (s1 * gp));
            o = k.outHf.process (k.outT.process (k.eq.process (o, 0.25)));
            x[c] = o / inScale;
        }
        cell.step (L);
    }
    double grDb() const { return 20.0 * std::log10 (1.0 + cell.conductance()); }
    double lambdaLimit = 0.12, ffMax = 8.0;                      // LIMIT: parte de la entrada en la cadena lateral (y su tope)
    double trap() const { return cell.trap(); }
    CellSpec spec;
private:
    void setSidechainFilter (double emphKnob)
    {
        emphNow = emphKnob;
        for (auto& c : ch) { const double z1 = c.sc.z1, z2 = c.sc.z2; c.sc.setHighShelf (fs, 5500.0, 0.62, -4.0 + emphDb (emphKnob)); c.sc.z1 = z1; c.sc.z2 = z2; }
    }
    struct Chan
    {
        Bq inHf, inLf, cap1, cap2, outHf, sc;
        Transformer inT, outT;
        AdaaStage amp1, amp2;
        AdaaEq eq;
        Quad qs, qf;
    };
    static constexpr double invVel2 = 1.0 / (vEl * vEl);
    double fs = 192000.0, smA = 0.001, emphNow = -1.0;
    double gp = 1, gpT = 1, pr = 0.5, prT = 0.5, lam = 0, lamT = 0;
    int chs = 2;
    OptoCell cell;
    std::array<Chan, 2> ch;
};
} // namespace l3a
