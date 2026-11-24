#pragma once
#include "L3aDsp.h"   // primitivas compartidas (biquad analogico, pasatodos, transformador)

// TOP 76: amplificador limitador FET (estilo 1176LN rev D/E "blackface"), doble precision, a 4x.
//
// Cadena (como el equipo):
//   trafo de entrada -> INPUT (atenuador) -> DIVISOR FET (resistencia en serie + FET a masa) -> preamplificador
//   -> (aqui sale la cadena lateral: es un compresor de REALIMENTACION; OUTPUT no cambia la compresion)
//   -> OUTPUT -> etapa de salida clase A (trafo dentro del lazo de realimentacion) -> trafo de salida.
//   Cadena lateral: amplificador -> rectificador -> condensador de tiempo (ATTACK carga, RELEASE descarga)
//   -> tension de compuerta del FET.
//
// Datos del equipo que se reproducen (manual UA / AES Moore 2016 / tesis Moore 2017 / UA WebZine 2003):
//   * ATTACK 20 us (7) .. 800 us (1), RELEASE 50 ms (7) .. 1.1 s (1); posicion OFF = sin reduccion (solo color)
//   * relaciones 4, 8, 12, 20:1 con el UMBRAL que sube con la relacion y el codo que se endurece con ella
//   * todo depende del programa: ataque, liberacion y relacion (lazo de realimentacion real, resuelto por muestra)
//   * ALL BUTTONS: relacion entre 12 y 20:1, la curva se "pasa" (la salida BAJA cuando el golpe es muy fuerte),
//     el ataque se retrasa (deja pasar el transiente), la liberacion bombea y la distorsion sube mucho
//   * THD sin compresion: 0.01 % a 0 dBu, 0.06 % a +9 dBu, 0.25 % a +16 dBu (1 kHz). Tercer armonico marcado.
//   * con -8 dB de reduccion (ATT 3, REL 7): 0.07 % a 1 kHz y ~0.4 % a 125 Hz (rizado del detector en graves)
//
// "Sin artefactos" (lo digital): todo el lazo se resuelve de forma IMPLICITA en cada muestra (no hay retardo de una
// muestra en la realimentacion -> no oscila ni "zumba" ni con 20 us a 20:1), etapas no lineales con ADAA y 4x
// (sin aliasing), todos los cambios de perilla y de boton se deslizan (sin clics), y el detector CLEAN usa una
// envolvente exacta por cuadratura: los graves no se ensucian con el rizado del rectificador.
namespace f76
{
constexpr double pi = 3.14159265358979323846;
using l3a::Bq;

// ---------------------------------------------------------------------------------------------
// red de 90 grados: (a, b) en cuadratura -> envolvente exacta sqrt(a^2 + b^2), sin rizado de rectificador
class QuadA
{
public:
    void prepare (double fs)
    {
        // 8 + 8 polos optimizados (minimax): error de fase < 0.22 grados de 12 Hz a 40 kHz (la red de 6 da 0.6)
        static const double pa[] { 3.60750, 25.78330, 101.68600, 392.44501, 1595.15381, 6566.03951, 24179.50134, 39638.81948 };
        static const double pb[] { 12.13500, 51.60421, 198.50647, 788.41596, 3234.82085, 13290.90052, 29890.66409, 122805.93818 };
        for (int i = 0; i < 8; ++i) { a[(size_t) i].set (pa[i], fs); b[(size_t) i].set (pb[i], fs); }
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
    std::array<l3a::Ap1, 8> a, b;
};

// ---------------------------------------------------------------------------------------------
// etapa de salida clase A con el trafo en el lazo: curva blanda simetrica (3er armonico) + un poco de par.
// f(x) = x / sqrt(1 + x^2/s^2) + b * x^2 / (1 + x^2/s^2); antiderivada cerrada -> ADAA de 1er orden exacto.
class OutStage
{
public:
    double s = 2.8, b = 0.0;
    void reset() { x1 = 0; }
    inline double f (double x) const noexcept { const double q = 1.0 + x * x / (s * s); return x / std::sqrt (q) + b * x * x / q; }
    inline double F (double x) const noexcept { return s * s * std::sqrt (1.0 + x * x / (s * s)) + b * s * s * (x - s * std::atan (x / s)); }
    inline double process (double u) noexcept
    {
        // F(x1) se recalcula con los parametros actuales: si 'b' se desliza (ALL BUTTONS) no aparece ningun pico
        const double Fu = F (u), dx = u - x1;
        const double y = std::abs (dx) > 1.0e-7 ? (Fu - F (x1)) / dx : f (0.5 * (u + x1));
        x1 = u; return y;
    }
private:
    double x1 = 0;
};

// ---------------------------------------------------------------------------------------------
// divisor FET: la conductancia del canal depende un poco de la tension drenaje-fuente (el lazo de linealizacion
// del 1176 lo corrige casi todo, queda sobre todo el 3er armonico). Resuelve v*(1 + G*(1 + a2 q + a3 q^2)) = x.
inline double fetDivider (double x, double G, double a2, double a3, double vf) noexcept
{
    if (G <= 0.0) return x;
    double v = x / (1.0 + G);
    for (int i = 0; i < 3; ++i)
    {
        const double q = v / vf;
        const double h = v * (1.0 + G * (1.0 + a2 * q + a3 * q * q)) - x;
        const double d = 1.0 + G * (1.0 + 2.0 * a2 * q + 3.0 * a3 * q * q);
        if (d <= 1e-9) break;
        v -= h / d;
    }
    return v;
}

// ---------------------------------------------------------------------------------------------
// perillas -> circuito
// ATTACK: red RC real del detector (0.22 uF con 470 ohm .. 25.47 kohm): 103 us (7) .. 5.6 ms (1). Dentro del lazo el
// ataque efectivo es mucho mas rapido (por eso el manual da 20-800 us); medido en el equipo: ~1 ms de pico y la mayor
// parte de la reduccion en pocos ms (Moore, AES 2016).
inline double attackTau (double k) { k = std::clamp (k, 1.0, 7.0); return 5.6e-3 * std::pow (0.103 / 5.6, (k - 1.0) / 6.0); }
inline double releaseTau (double k) { k = std::clamp (k, 1.0, 7.0); return 1.1 * std::pow (0.05 / 1.1, (k - 1.0) / 6.0); }

// relaciones: umbral (dBFS de pico en la cadena lateral), codo (dB) y pendiente
struct RatioSpec { double R, T, knee; };
inline RatioSpec ratioSpec (int i)
{
    switch (i)
    {
        case 0:  return { 4.0,  -20.0, 10.0 / 4.0 };     // codo (dB a la salida) = codo de entrada / relacion
        case 1:  return { 8.0,  -17.5,  7.0 / 8.0 };
        case 2:  return { 12.0, -16.0,  5.0 / 12.0 };
        case 3:  return { 20.0, -14.5,  3.0 / 20.0 };
        default: return { 14.0, -18.0,  4.0 / 14.0 };    // ALL BUTTONS
    }
}

struct Settings
{
    double inDb = -6.0, outDb = 0.0, attack = 3.0, release = 5.0, hpf = 20.0;
    int ratio = 0;            // 0 4:1, 1 8:1, 2 12:1, 3 20:1, 4 ALL
    bool attackOff = false, clean = true, link = true;
};

// ---------------------------------------------------------------------------------------------
class Engine
{
public:
    static constexpr double maxGr = 42.0;
    // calibracion del color (ver pruebas)
    double fetA2 = 0.0015, fetA3 = 0.004, fetVf = 0.25;         // no linealidad del FET (normal)
    double allA2 = 0.10, allA3 = 0.30;                         // ALL BUTTONS: polarizacion fuera de rango
    double outS = 2.9, outB = 0.003, allOutB = 0.05;          // etapa de salida
    double progRho = 0.3;                                      // liberacion en dos etapas (memoria del programa)
    double vintageRipple = 0.026;                              // VINTAGE: parte detectada por rectificador (rizado)
    double peakTol = 0.015;
    double diodeKnee = 0.003;                                  // rodilla del diodo de carga (relativa)                                    // margen del tope por picos reales de la cuadratura

    void prepare (double rate, int numCh)
    {
        fs = rate; dt = 1.0 / fs; chs = std::clamp (numCh, 1, 2);
        aS = 1.0 - std::exp (-1.0 / (0.025 * fs));
        pkLen = std::max (1, (int) std::lround (0.026 * fs));
        aEnv = 1.0 - std::exp (-2.0 * pi * 5000.0 / fs);
        aHup = 1.0 - std::exp (-1.0 / (0.30 * fs)); aHdn = 1.0 - std::exp (-1.0 / (0.80 * fs));
        for (auto& c : ch)
        {
            c.inHf.setLP2 (fs, 70000.0, 0.62); c.inLf.setHP2 (fs, 6.0, 0.62);
            c.inT.prepare (fs, 5.0, 0.14, 4.0, 0.015);
            c.outT.prepare (fs, 4.0, 0.16, 4.5, 0.03);
            c.outHf.setLP2 (fs, 52000.0, 0.70);
            c.dc.setHP1 (fs, 3.0);
            c.q.prepare (fs);
        }
        for (auto& d : det) d = {};
        hpNow = -1; setHpf (20.0);
        reset();
    }
    void reset()
    {
        for (auto& c : ch)
        {
            c.inHf.reset(); c.inLf.reset(); c.inT.reset(); c.outT.reset(); c.outHf.reset(); c.dc.reset(); c.hp.reset();
            c.q.reset(); c.out.reset(); c.pkCur = c.pkPrev = c.ev1 = c.ev2 = c.ff1 = c.ff2 = 0; c.pkN = 0;
        }
        for (auto& d : det) d = {};
        cur = tgt;
    }
    void set (const Settings& s, bool snap = false)
    {
        const auto r = ratioSpec (s.ratio);
        tgt.gi = std::pow (10.0, s.inDb / 20.0); tgt.go = std::pow (10.0, s.outDb / 20.0);
        tgt.R = r.R; tgt.T = r.T; tgt.knee = r.knee; tgt.all = s.ratio == 4 ? 1.0 : 0.0;
        tgt.comp = s.attackOff ? 0.0 : 1.0;
        tgt.la = std::log (attackTau (s.attack)); tgt.lr = std::log (releaseTau (s.release));
        clean = s.clean; link = s.link;
        if (std::abs (s.hpf - hpNow) > 1e-6) setHpf (s.hpf);
        if (snap) cur = tgt;
    }

    // una muestra sobremuestreada por canal
    inline void process (double* x) noexcept
    {
        const Smooth prev = cur;
        smoothStep();
        // al cambiar la relacion (o ATTACK OFF) la reduccion actual se conserva y el lazo llega a la nueva con su ataque
        // o su liberacion: sin "hundimientos" ni saltos de volumen al apretar un boton
        if (std::abs (prev.R - cur.R) + std::abs (prev.knee - cur.knee) + std::abs (prev.comp - cur.comp) > 1e-12)
            for (auto& D : det)
                if (D.V > 0) D.V = vLaw (grLaw (D.V, prev.R, prev.knee, prev.comp, D.extra), cur.R, cur.knee, cur.comp, D.extra);
        const double R = cur.R, kTh = std::pow (10.0, -cur.T / 20.0);
        const double tauA = std::exp (cur.la) * (1.0 + 3.0 * cur.all);         // ALL: el ataque se retrasa
        const double tauR = std::exp (cur.lr) * (1.0 - 0.4 * cur.all);         // ALL: libera antes (bombea)
        const double aA = dt / tauA, aR = dt / tauR, aRs = dt / (6.0 * tauR);
        const double rip = clean ? 0.0 : vintageRipple;   // VINTAGE: parte del rectificador de onda completa (rizado real)
        double u[2] = { 0, 0 };
        for (int c = 0; c < chs; ++c)
        {
            auto& k = ch[(size_t) c];
            u[c] = k.inT.process (k.inLf.process (k.inHf.process (x[c]))) * cur.gi;
        }
        // ---- cadena lateral. La salida del FET es u * g (g cambia lento frente a la senal), asi que la envolvente de la
        // SALIDA es env(u) * g: el lazo de realimentacion se resuelve exacto y sin retardo. env(u): cuadratura exacta
        // (sin rizado) acotada por los picos reales recientes (la red de 90 grados "suena" un instante en los saltos);
        // VINTAGE agrega una parte de rectificador de onda completa (el rizado del equipo).
        double envU[2] = { 0, 0 }, ffIn[2] = { 0, 0 };
        for (int c = 0; c < chs; ++c)
        {
            auto& k = ch[(size_t) c];
            const double hs = k.hp.process (u[c]);
            const double sc = hpOn ? hs : u[c];
            double ia, ib; k.q.process (sc, ia, ib);
            const double as = std::abs (sc);
            k.pkCur = std::max (k.pkCur, as);
            if (++k.pkN >= pkLen) { k.pkPrev = k.pkCur; k.pkCur = 0; k.pkN = 0; }
            const double qe = std::sqrt (ia * ia + ib * ib), bound = std::max (k.pkPrev, k.pkCur) * (1.0 + peakTol);
            const double dq = qe - bound, kq = 0.01 * bound;                // minimo suave: sin esquinas
            const double quad = 0.5 * (qe + bound - std::sqrt (dq * dq + kq * kq));
            // filtro de la envolvente (2 polos criticos a 5 kHz): la ganancia no se mueve a frecuencias que, al multiplicar
            // la senal, se reflejarian dentro del audio. No retrasa el ataque (el mas rapido del equipo es ~100 us)
            const double raw = (1.0 - rip) * quad + rip * as;
            k.ev1 += (raw - k.ev1) * aEnv; k.ev2 += (k.ev1 - k.ev2) * aEnv;
            envU[c] = k.ev2;
            k.ff1 += (quad - k.ff1) * aEnv; k.ff2 += (k.ff1 - k.ff2) * aEnv;
            ffIn[c] = k.ff2;
        }
        const int nd = link ? 1 : chs;
        double gch[2] = { 1, 1 };
        for (int d = 0; d < nd; ++d)
        {
            auto& D = det[(size_t) d];
            const int c0 = link ? 0 : d, c1 = link ? chs - 1 : d;
            // detector directo (solo ALL BUTTONS): "se pasa" cuando el golpe es muy fuerte
            double fin = 0; for (int c = c0; c <= c1; ++c) fin = std::max (fin, ffIn[c]);
            fin *= kTh;
            D.Vf = fin > D.Vf ? (D.Vf + aA * fin) / (1.0 + aA) : D.Vf / (1.0 + aR);
            double extra = 0.0;
            if (cur.all > 1e-6 && D.Vf > 1e-9)
            {
                const double ov = 20.0 * std::log10 (D.Vf) - 6.0;              // codo suave de 1 dB
                extra = cur.all * 1.3 * 0.5 * (ov + std::sqrt (ov * ov + 1.0));
            }
            D.extra = extra;
            auto grOf = [&] (double V) { return grLaw (V, R, cur.knee, cur.comp, extra); };
            auto gOf = [&] (double V) { return std::exp (-0.11512925464970229 * grOf (V)); };
            double eu = 0; for (int c = c0; c <= c1; ++c) eu = std::max (eu, envU[c]);
            eu *= kTh;
            auto eOf = [&] (double g) { return eu * g; };
            // descarga: la REDUCCION (en dB) cae de forma exponencial con la constante de RELEASE: rapida hasta el
            // "piso" que deja la memoria del programa y mas lenta debajo (liberacion en dos etapas)
            const double grNow = grOf (D.V);
            double V1;
            if (grNow > 1e-9 && cur.comp > 1e-6)
            {
                const double floorG = progRho * D.H;
                double g1 = grNow > floorG ? floorG + (grNow - floorG) / (1.0 + aR) : grNow;
                g1 /= (1.0 + aRs);
                V1 = vLaw (g1, R, cur.knee, cur.comp, extra);
            }
            else V1 = D.V / (1.0 + aR);
            const double Vrel = V1;
            // carga: el lazo de realimentacion se resuelve implicito en la misma muestra. El diodo de carga es suave
            // (rodilla de ~0.3 %): sin conmutaciones duras muestra a muestra que dejen espurias en el espectro.
            auto diode = [] (double z, double ep) { return 0.5 * (z + std::sqrt (z * z + ep * ep)); };
            const double eps = diodeKnee * std::max (Vrel, 1e-9);
            auto F = [&] (double Vt) { return Vt - Vrel - aA * diode (eOf (gOf (Vt)) - Vt, eps); };
            if (eOf (gOf (V1)) - V1 > -12.0 * eps)
            {
                double lo = V1, hi = lo + aA * (eu + eps) + 1e-12;
                double flo = F (lo), fhi = F (hi);
                if (flo < 0 && fhi > 0)
                {
                    int side = 0;
                    for (int it = 0; it < 40; ++it)
                    {
                        const double m = hi - fhi * (hi - lo) / (fhi - flo);
                        const double mm = (m > lo && m < hi) ? m : 0.5 * (lo + hi);
                        const double fm = F (mm);
                        if (fm > 0) { hi = mm; fhi = fm; if (side == -1) flo *= 0.5; side = -1; }
                        else { lo = mm; flo = fm; if (side == 1) fhi *= 0.5; side = 1; }
                        if (hi - lo < 1e-9 * hi + 1e-15) break;
                    }
                    V1 = 0.5 * (lo + hi);
                }
            }
            D.V = V1 < 1e-15 ? 0.0 : V1;
            D.gr = grOf (D.V);
            D.H += (D.gr - D.H) * (D.gr > D.H ? aHup : aHdn);
            if (D.H < 1e-12) D.H = 0;
            const double g = std::pow (10.0, -D.gr / 20.0);
            for (int c = c0; c <= c1; ++c) gch[c] = g;
        }
        // ---- FET + preamplificador, actualizacion de la cadena lateral y salida
        const double a2 = fetA2 + (allA2 - fetA2) * cur.all, a3 = fetA3 + (allA3 - fetA3) * cur.all;
        for (int c = 0; c < chs; ++c)
        {
            auto& k = ch[(size_t) c];
            const double G = 1.0 / gch[c] - 1.0;
            const double y = fetDivider (u[c], G, a2, a3, fetVf);
            k.out.s = outS; k.out.b = outB + (allOutB - outB) * cur.all;
            double o = k.out.process (y * cur.go);
            o = k.outHf.process (k.outT.process (o));
            x[c] = k.dc.process (o);
        }
        if (! link && chs == 1) det[1] = det[0];
    }
    double grDb() const { return std::max (det[0].gr, chs > 1 && ! link ? det[1].gr : 0.0); }
    double grDb (int c) const { return det[(size_t) (link ? 0 : c)].gr; }

private:
    // ley de reduccion (dB) en funcion de la tension de control (normalizada: 1 = umbral) y su inversa
    static double grLaw (double V, double R, double w, double comp, double extra) noexcept
    {
        const double dB = V > 1e-12 ? 8.685889638065035 * std::log (V) : -240.0;
        const double kn = dB <= -0.5 * w ? 0.0 : (dB >= 0.5 * w ? dB : (dB + 0.5 * w) * (dB + 0.5 * w) / (2.0 * w));
        return std::min (maxGr, comp * ((R - 1.0) * kn + extra));
    }
    static double vLaw (double gr, double R, double w, double comp, double extra) noexcept
    {
        if (comp < 1e-6) return 0.0;
        const double kn = std::max (0.0, gr / comp - extra) / std::max (1e-6, R - 1.0);
        if (kn <= 0.0) return 0.0;
        const double d = kn >= 0.5 * w ? kn : std::sqrt (2.0 * w * kn) - 0.5 * w;
        return std::pow (10.0, d / 20.0);
    }
    struct Det { double V = 0, H = 0, Vf = 0, gr = 0, extra = 0; };
    struct Chan
    {
        Bq inHf, inLf, outHf, dc, hp;
        l3a::Transformer inT, outT;
        QuadA q;
        OutStage out;
        double pkCur = 0, pkPrev = 0, ev1 = 0, ev2 = 0, ff1 = 0, ff2 = 0; int pkN = 0;
    };
    struct Smooth { double gi = 0.5, go = 1, R = 4, T = -20, knee = 10, all = 0, comp = 1, la = -8, lr = -1; };
    void smoothStep() noexcept
    {
        auto s = [this] (double& a, double b) { a += (b - a) * aS; };
        s (cur.gi, tgt.gi); s (cur.go, tgt.go); s (cur.R, tgt.R); s (cur.T, tgt.T); s (cur.knee, tgt.knee);
        s (cur.all, tgt.all); s (cur.comp, tgt.comp); s (cur.la, tgt.la); s (cur.lr, tgt.lr);
    }
    void setHpf (double f)
    {
        hpNow = f; hpOn = f > 21.0;
        for (auto& c : ch) { const double z1 = c.hp.z1, z2 = c.hp.z2; c.hp.setHP2 (fs, std::max (20.0, f), 0.707); c.hp.z1 = z1; c.hp.z2 = z2; }
    }
    double fs = 192000.0, dt = 1.0 / 192000.0, aS = 0.001, aEnv = 0.15, aHup = 0, aHdn = 0, hpNow = -1;
    bool clean = true, link = true, hpOn = false;
    int chs = 2, pkLen = 4992;
    Smooth cur, tgt;
    std::array<Chan, 2> ch;
    std::array<Det, 2> det;
};
} // namespace f76
