#pragma once
#include <cmath>
#include <algorithm>
#include "DSP/Biquad.h"

// Motor de TOP MINI CH: preamp limpio, filtro 75 Hz, EQ de 2 bandas (shelf/campana) y compresor de una perilla
namespace tmc
{
inline float dB2g (float d) { return std::pow (10.0f, d * 0.05f); }
inline float g2dB (float g) { return 20.0f * std::log10 (std::max (1.0e-9f, g)); }

struct Params
{
    float gain = 0; bool phase = false, color = false, hpf = false, line = false, hiz = false;
    float hf = 0, lf = 0; bool hfBell = false, lfBell = false, eqIn = true;
    float thr = 10; bool cmpIn = false;          // umbral en dBu (+10 .. -20); 0 dBu = -18 dBFS
};

class Channel
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        hp.setHighPass (sr, 75.0, 0.7071);                 // 75 Hz, 12 dB/oct
        hizTone.setPeak (sr, 5500.0, 0.8, 1.6);            // Hi-Z: brillo de pastilla con carga alta
        dcL.setHighPass1 (sr, 8.0); dcR = dcL;
        reset();
    }
    void reset()
    {
        for (auto* b : { &hpL, &hpR, &hfL, &hfR, &lfL, &lfR, &hzL, &hzR, &dcL, &dcR }) b->reset();
        env = 0; gr = 0; gSm = 1; lastHf = lastLf = 999; lastHfBell = lastLfBell = -1;
    }
    float grDb() const { return gr; }

    void process (float* L, float* R, int n, const Params& p)
    {
        updateEq (p);
        const float in = dB2g (p.gain - (p.line ? 9.0f : 0.0f)) * (p.phase ? -1.0f : 1.0f);
        const float thrDbfs = p.thr - 18.0f;
        const float makeup = p.cmpIn ? dB2g (std::max (0.0f, -18.0f - thrDbfs) * 0.25f) : 1.0f;   // compensacion automatica suave
        const float rel = (float) std::exp (-1.0 / (0.30 * sr));
        for (int i = 0; i < n; ++i)
        {
            float x[2] { L[i] * in, R ? R[i] * in : 0.0f };
            const int chs = R ? 2 : 1;
            for (int c = 0; c < chs; ++c)
            {
                float v = x[c];
                if (p.hiz) { v = (c ? hzR : hzL).process (v); v = v + 0.06f * v * v - 0.03f * v * v * v; }        // Hi-Z: brillo + armonicos
                if (p.color) v = color (v);                                                                       // color analogico
                v = superAnalogue (v);
                if (p.hpf) v = (c ? hpR : hpL).process (v);
                if (p.eqIn) { v = (c ? hfR : hfL).process (v); v = (c ? lfR : lfL).process (v); }
                x[c] = v;
            }
            if (p.cmpIn)
            {
                // detector de avance (feed-forward), ataque segun el programa y release fijo
                const float lvl = std::max (std::abs (x[0]), chs > 1 ? std::abs (x[1]) : 0.0f);
                const float ldb = g2dB (lvl);
                const float over = ldb - g2dB (env + 1.0e-9f);
                const float att = (float) std::exp (-1.0 / ((over > 6.0f ? 0.002 : over > 2.0f ? 0.008 : 0.020) * sr));
                env = lvl > env ? lvl + (env - lvl) * att : lvl + (env - lvl) * rel;
                const float e = g2dB (env), knee = 10.0f;
                float red;
                const float d = e - thrDbfs;
                if (d <= -knee * 0.5f) red = 0.0f;
                else if (d >= knee * 0.5f) red = d * 0.5f;                               // 2:1
                else { const float t = d + knee * 0.5f; red = 0.5f * t * t / (2.0f * knee); }   // rodilla suave ("over easy")
                gr = red;
                const float g = dB2g (-red) * makeup;
                gSm += (g - gSm) * 0.02f;
                for (int c = 0; c < chs; ++c) x[c] *= gSm;
            }
            else { gr = 0; gSm = 1; }
            if (p.color || p.hiz) for (int c = 0; c < chs; ++c) x[c] = (c ? dcR : dcL).process (x[c]);   // solo la saturacion asimetrica genera DC
            L[i] = x[0]; if (R) R[i] = x[1];
        }
    }

private:
    // preamp SuperAnalogue: transparente, solo redondea muy cerca del maximo
    static float superAnalogue (float v)
    {
        const float a = std::abs (v);
        if (a < 0.9f) return v;
        const float s = v > 0 ? 1.0f : -1.0f;
        return s * (0.9f + 0.1f * std::tanh ((a - 0.9f) / 0.1f));
    }
    // color: segundo y tercer armonico suaves (tipo transformador)
    static float color (float v) { return std::tanh (v * 1.15f) / 1.15f + 0.04f * v * v; }

    void updateEq (const Params& p)
    {
        if (p.hf != lastHf || (int) p.hfBell != lastHfBell)
        {
            if (p.hfBell) hfL.setPeak (sr, 5000.0, 0.9, p.hf); else hfL.setHighShelf (sr, 3500.0, p.hf);
            const float z1 = hfR.z1, z2 = hfR.z2; hfR = hfL; hfR.z1 = z1; hfR.z2 = z2;   // conserva el estado: sin clics al mover
            lastHf = p.hf; lastHfBell = p.hfBell;
        }
        if (p.lf != lastLf || (int) p.lfBell != lastLfBell)
        {
            if (p.lfBell) lfL.setPeak (sr, 200.0, 0.9, p.lf); else lfL.setLowShelf (sr, 60.0, p.lf);
            const float z1 = lfR.z1, z2 = lfR.z2; lfR = lfL; lfR.z1 = z1; lfR.z2 = z2;
            lastLf = p.lf; lastLfBell = p.lfBell;
        }
        hpL.b0 = hp.b0; hpL.b1 = hp.b1; hpL.b2 = hp.b2; hpL.a1 = hp.a1; hpL.a2 = hp.a2;
        hpR.b0 = hp.b0; hpR.b1 = hp.b1; hpR.b2 = hp.b2; hpR.a1 = hp.a1; hpR.a2 = hp.a2;
        hzL.b0 = hizTone.b0; hzL.b1 = hizTone.b1; hzL.b2 = hizTone.b2; hzL.a1 = hizTone.a1; hzL.a2 = hizTone.a2;
        hzR.b0 = hizTone.b0; hzR.b1 = hizTone.b1; hzR.b2 = hizTone.b2; hzR.a1 = hizTone.a1; hzR.a2 = hizTone.a2;
    }

    double sr = 48000.0;
    tvc::Biquad hp, hizTone, hpL, hpR, hfL, hfR, lfL, lfR, hzL, hzR, dcL, dcR;
    float lastHf = 999, lastLf = 999; int lastHfBell = -1, lastLfBell = -1;
    float env = 0, gr = 0, gSm = 1;
};
} // namespace tmc
