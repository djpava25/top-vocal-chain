#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include "DSP/Biquad.h"

// Motor de TOP CONSOLE B: canal de consola estilo serie 4000 B.
// Preamp con transformador -> filtros -> EQ 4 bandas -> dinamica (o dinamica antes) -> fader VCA -> trim
namespace tcb
{
inline float dB2g (float d) { return std::pow (10.0f, d * 0.05f); }
inline float g2dB (float g) { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }
inline float coefMs (double sr, float ms) { return 1.0f - std::exp (-1.0f / (float) (std::max (0.005f, ms) * 0.001 * sr)); }

// posiciones del selector de ratio (la ultima es el modo de-esser)
inline const std::array<float, 10>& ratios() { static const std::array<float, 10> r { 1, 1.5f, 2, 3, 4, 6, 10, 20, 100, 4 }; return r; }
constexpr int dsPosition = 9;

struct Params
{
    float inTrim = 0, drive = 0; bool phase = false;
    float hpf = 16, lpf = 22000; bool filtIn = true, filtSc = false;
    float lfG = 0, lfF = 100, hmfG = 0, hmfF = 2500, hmfQ = 1, lmfG = 0, lmfF = 600, lmfQ = 1, hfG = 0, hfF = 8000;
    bool lfBell = false, hfBell = false, eqIn = true, eqSc = false;
    int ratio = 0; float thr = -20, rel = 0.3f; bool fast = false;
    float gRange = 0, gThr = -40, gRel = 0.25f, gHold = 0; bool gFast = false, gExp = false;
    bool dynIn = true, dynPre = false, extSc = false;
    float fader = 0, outTrim = 0; bool cut = false;
    bool preIn = true, mono = false, listen = false; float width = 1, pan = 0, mix = 1;
};

//==============================================================================
// Filtros: pasa altos 18 dB/oct y pasa bajos 12 dB/oct
struct Filters
{
    std::array<tvc::Biquad, 2> hp2, hp1, lp;
    float lastH = -1, lastL = -1;
    double sr = 44100;
    void prepare (double r) { sr = r; lastH = lastL = -1; for (auto* a : { &hp2, &hp1, &lp }) for (auto& b : *a) b.reset(); }
    bool hpOn (const Params& p) const { return p.hpf > 17.0f; }
    bool lpOn (const Params& p) const { return p.lpf < 21500.0f; }
    void update (const Params& p)
    {
        if (p.hpf != lastH) { lastH = p.hpf; for (auto& b : hp2) b.setHighPass (sr, p.hpf, 1.0); for (auto& b : hp1) b.setHighPass1 (sr, p.hpf); }
        if (p.lpf != lastL) { lastL = p.lpf; for (auto& b : lp) b.setLowPass (sr, std::min ((double) p.lpf, sr * 0.45), 0.707); }
    }
    float process (int c, float x, const Params& p)
    {
        if (hpOn (p)) x = hp1[(size_t) c].process (hp2[(size_t) c].process (x));
        if (lpOn (p)) x = lp[(size_t) c].process (x);
        return x;
    }
};

// EQ de 4 bandas. Medios de ancho de banda constante (la Q no cambia con la ganancia)
struct Eq
{
    std::array<std::array<tvc::Biquad, 4>, 2> b;
    std::array<float, 16> last {};
    std::array<bool, 4> on {};
    double sr = 44100;
    void prepare (double r) { sr = r; last.fill (-999.0f); for (auto& c : b) for (auto& x : c) x.reset(); }
    static void design (std::array<tvc::Biquad, 4>& d, double sr, const Params& p)
    {
        const double ny = sr * 0.45;
        if (p.lfBell) d[0].setPeak (sr, p.lfF, 0.9, p.lfG); else d[0].setLowShelf (sr, p.lfF, p.lfG);
        d[1].setPeak (sr, std::min ((double) p.lmfF, ny), p.lmfQ, p.lmfG);
        d[2].setPeak (sr, std::min ((double) p.hmfF, ny), p.hmfQ, p.hmfG);
        if (p.hfBell) d[3].setPeak (sr, std::min ((double) p.hfF, ny), 0.9, p.hfG); else d[3].setHighShelf (sr, std::min ((double) p.hfF, ny), p.hfG);
    }
    void update (const Params& p)
    {
        const std::array<float, 16> cur { p.lfG, p.lfF, (float) p.lfBell, p.lmfG, p.lmfF, p.lmfQ, p.hmfG, p.hmfF, p.hmfQ, p.hfG, p.hfF, (float) p.hfBell, 0, 0, 0, 0 };
        on = { std::abs (p.lfG) > 0.02f, std::abs (p.lmfG) > 0.02f, std::abs (p.hmfG) > 0.02f, std::abs (p.hfG) > 0.02f };
        if (cur == last) return;
        last = cur;
        for (auto& c : b) design (c, sr, p);
    }
    float process (int c, float x)
    {
        for (int i = 0; i < 4; ++i) if (on[(size_t) i]) x = b[(size_t) c][(size_t) i].process (x);
        return x;
    }
};

//==============================================================================
// Preamp con transformador: armonicos (mas en graves) con ganancia compensada
struct Preamp
{
    std::array<tvc::Biquad, 2> lowSplit, dc;
    void prepare (double sr) { for (auto& f : lowSplit) { f.setLowPass (sr, 250.0, 0.707); f.reset(); } for (auto& f : dc) { f.setHighPass1 (sr, 8.0); f.reset(); } }
    float process (int c, float x, float drive)
    {
        if (drive <= 0.001f) return x;
        const float d = drive * drive;
        const float lo = lowSplit[(size_t) c].process (x);
        const float k = 1.0f + d * 2.2f;
        // la parte grave satura mas (nucleo del transformador), con algo de 2o armonico
        auto sat = [] (float v, float kk) { return std::tanh (kk * v + 0.12f * kk * v * v) / kk; };
        const float y = sat (x - lo, k * 0.6f) + sat (lo, k);
        return dc[(size_t) c].process (y);
    }
};

// Compresor de realimentacion con deteccion de pico (VCA en el lazo) y modo de-esser
struct Compressor
{
    float env = 0, gr = 0, lastOut = 0;
    std::array<tvc::Biquad, 2> dsHp;
    double sr = 44100;
    void prepare (double r) { sr = r; env = gr = lastOut = 0; for (auto& f : dsHp) { f.setHighPass (sr, 4500.0, 0.707); f.reset(); } }
    // devuelve la ganancia lineal; 'detL/R' es la senal de deteccion
    float step (float detL, float detR, const Params& p)
    {
        const bool ds = p.ratio == dsPosition;
        float d;
        if (ds) d = std::max (std::abs (dsHp[0].process (detL)), std::abs (dsHp[1].process (detR))) * 4.0f;   // la sibilancia dispara
        else d = std::max (std::abs (detL), std::abs (detR));
        // realimentacion: el detector mira la senal ya comprimida
        const float fb = ds ? d : d * dB2g (-gr);
        const float a = coefMs (sr, ds ? 0.5f : (p.fast ? 0.3f : 3.0f));
        const float rl = coefMs (sr, ds ? 50.0f : p.rel * 1000.0f);
        env += (fb - env) * (fb > env ? coefMs (sr, 0.1f) : coefMs (sr, 30.0f));
        const float ratio = ratios()[(size_t) p.ratio];
        const float over = g2dB (env) - p.thr;
        float target = 0.0f;
        if (ratio > 1.0f && over > 0.0f) target = std::min (40.0f, over * (ratio - 1.0f) * (ds ? 0.25f : 1.0f));
        if (ds) target = std::min (target, 12.0f);
        gr += (target - gr) * (target > gr ? a : rl);
        return dB2g (-gr);
    }
};

// Gate / expansor con hold e histeresis
struct Gate
{
    float env = 0, gain = 1, gDb = 0; int hold = 0; bool open = true;
    double sr = 44100;
    void prepare (double r) { sr = r; env = 0; gain = 1; gDb = 0; hold = 0; open = true; }
    float step (float detL, float detR, const Params& p)
    {
        if (p.gRange <= 0.01f) { gain = 1; gDb = 0; return 1.0f; }
        const float d = std::max (std::abs (detL), std::abs (detR));
        env += (d - env) * (d > env ? 0.5f : coefMs (sr, 2.0f));
        const float lvl = g2dB (env);
        if (lvl > p.gThr) { open = true; hold = (int) (p.gHold * sr); }
        else if (lvl < p.gThr - 3.0f) { if (hold > 0) --hold; else open = false; }
        float target = 0.0f;
        if (! open)
        {
            if (p.gExp) target = -std::min (p.gRange, (p.gThr - lvl));       // expansor 1:2
            else target = -p.gRange;
        }
        // ataque exponencial rapido; cierre en rampa lineal en dB (RELEASE = tiempo de cerrar todo el rango)
        if (target > gDb) gDb += (target - gDb) * coefMs (sr, p.gFast ? 0.1f : 1.5f);
        else gDb = std::max (target, gDb - p.gRange / (float) (std::max (0.01f, p.gRel) * sr));
        gain = dB2g (gDb);
        return gain;
    }
};

// Fader VCA: ganancia + saturacion sutil que crece con el nivel
struct VcaFader
{
    std::array<tvc::Biquad, 2> dc;
    float gSm = 1.0f;
    void prepare (double sr) { for (auto& f : dc) { f.setHighPass1 (sr, 6.0); f.reset(); } gSm = 1; }
    float process (int c, float x, float faderDb)
    {
        const float k = std::max (0.0f, faderDb) / 12.0f;      // por encima de 0 dB aparece el color del VCA
        float y = x * gSm;
        if (k > 0.0f)
        {
            const float drv = k * 0.6f;
            y = std::tanh (y * drv + 0.08f * k * y * y) / drv;
            y = dc[(size_t) c].process (y);
        }
        return y;
    }
};

//==============================================================================
struct Strip
{
    Filters filt, filtSc; Eq eq, eqSc; Preamp pre; Compressor comp; Gate gate; VcaFader vca;
    float inSm = 1, outSm = 1;
    double sr = 44100;
    std::atomic<float> compGr { 0 }, gateGr { 0 };
    void prepare (double r)
    {
        sr = r;
        filt.prepare (sr); filtSc.prepare (sr); eq.prepare (sr); eqSc.prepare (sr); pre.prepare (sr);
        comp.prepare (sr); gate.prepare (sr); vca.prepare (sr);
    }
    void process (float* L, float* R, int n, const float* scL, const float* scR, const Params& p)
    {
        filt.update (p); filtSc.update (p); eq.update (p); eqSc.update (p);
        const float inT = dB2g (p.inTrim) * (p.phase ? -1.0f : 1.0f);
        const float faderG = p.fader <= -59.9f ? 0.0f : dB2g (p.fader);
        const float outT = dB2g (p.outTrim) * (p.cut ? 0.0f : 1.0f);
        const float drive = p.drive / 100.0f;
        float cmax = 0, gmin = 1;
        for (int i = 0; i < n; ++i)
        {
            inSm += (inT - inSm) * 0.002f;
            float x[2] { L[i] * inSm, R[i] * inSm };
            if (p.preIn) for (int c = 0; c < 2; ++c) x[c] = pre.process (c, x[c], drive);
            bool listened = false;
            auto eqStage = [&] (float* v)
            {
                for (int c = 0; c < 2; ++c)
                {
                    if (p.filtIn && ! p.filtSc) v[c] = filt.process (c, v[c], p);
                    if (p.eqIn && ! p.eqSc) v[c] = eq.process (c, v[c]);
                }
            };
            auto dynStage = [&] (float* v)
            {
                if (! p.dynIn) return;
                float s[2] { v[0], v[1] };
                if (p.extSc && scL != nullptr) { s[0] = scL[i]; s[1] = scR != nullptr ? scR[i] : scL[i]; }
                for (int c = 0; c < 2; ++c)
                {
                    if (p.filtIn && p.filtSc) s[c] = filtSc.process (c, s[c], p);
                    if (p.eqIn && p.eqSc) s[c] = eqSc.process (c, s[c]);
                }
                const float gc = comp.step (s[0], s[1], p);
                const float gg = gate.step (s[0], s[1], p);
                cmax = std::max (cmax, comp.gr);
                gmin = std::min (gmin, gg);
                const float gcm = p.mix * gc + (1.0f - p.mix);       // mezcla paralela del compresor
                if (p.listen) { listened = true; for (int c = 0; c < 2; ++c) v[c] = s[c]; return; }
                for (int c = 0; c < 2; ++c) v[c] *= gcm * gg;
            };
            if (p.dynPre) { dynStage (x); eqStage (x); }
            else { eqStage (x); dynStage (x); }
            vca.gSm += (faderG - vca.gSm) * 0.002f;
            outSm += (outT - outSm) * 0.002f;
            if (listened) { L[i] = x[0]; R[i] = x[1]; continue; }
            for (int c = 0; c < 2; ++c) x[c] = vca.process (c, x[c], p.fader) * outSm;
            // imagen estereo: ancho M/S, mono y paneo (balance)
            if (p.width != 1.0f || p.mono)
            {
                const float m = 0.5f * (x[0] + x[1]), sd = 0.5f * (x[0] - x[1]) * (p.mono ? 0.0f : p.width);
                x[0] = m + sd; x[1] = m - sd;
            }
            if (p.pan != 0.0f) { x[0] *= std::min (1.0f, 1.0f - p.pan); x[1] *= std::min (1.0f, 1.0f + p.pan); }
            L[i] = x[0]; R[i] = x[1];
        }
        compGr.store (p.dynIn ? cmax : 0.0f);
        gateGr.store (p.dynIn ? -g2dB (gmin) : 0.0f);
    }
};
} // namespace tcb
