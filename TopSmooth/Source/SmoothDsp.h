#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>
#include "DSP/Biquad.h"

// Motor de TOP SMOOTH: supresor dinamico de resonancias.
// Mira el espectro, encuentra los picos que sobresalen de su alrededor y los baja solo
// mientras suenan. Dos motores: fase lineal (STFT) y baja latencia (banco de filtros IIR).
namespace tsm
{
struct Settings
{
    int mode = 0;                 // 0 Suave (umbral adaptativo), 1 Duro
    float depth = 5.0f;           // 0..20
    float detail = 50.0f;         // 0..100 (agudeza + selectividad)
    float attackMs = 5.0f, releaseMs = 50.0f;
    float maxCut = 18.0f;         // dB
    float tilt = 0.0f;            // dB/octava sobre 1 kHz
    float lowCut = 20.0f, highCut = 20000.0f;
    std::array<float, 5> nf { 150.0f, 500.0f, 2000.0f, 5000.0f, 10000.0f };
    std::array<float, 5> ng {};   // sensibilidad de cada nodo (dB)
    std::array<float, 5> nq { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
};

// sensibilidad (dB) de la EQ de foco + tilt, y factor 0..1 de los cortes grave/agudo
inline float sensitivityDb (float hz, const Settings& s)
{
    float d = s.tilt * std::log2 (std::max (hz, 10.0f) / 1000.0f);
    for (int i = 0; i < 5; ++i)
    {
        const float o = std::log2 (std::max (hz, 10.0f) / s.nf[(size_t) i]);
        const float bw = 0.75f / std::max (0.1f, s.nq[(size_t) i]);
        d += s.ng[(size_t) i] * std::exp (-0.5f * (o / bw) * (o / bw));
    }
    return d;
}
inline float cutFade (float hz, const Settings& s)
{
    float f = 1.0f;
    if (hz < s.lowCut) f *= std::pow (hz / s.lowCut, 4.0f);
    if (hz > s.highCut) f *= std::pow (s.highCut / hz, 4.0f);
    return f;
}

//==============================================================================
// Calcula cuanto bajar cada bin (dB positivos) a partir del espectro de deteccion
class GainComputer
{
public:
    void prepare (double sampleRate, int fftSize, int hopSize)
    {
        sr = sampleRate; N = fftSize; hop = hopSize; K = N / 2 + 1;
        prefix.assign ((size_t) K + 1, 0.0f); prefixW.assign ((size_t) K + 1, 0.0f);
        ref.assign ((size_t) K, 0.0f); raw.assign ((size_t) K, 0.0f); sm.assign ((size_t) K, 0.0f);
        state.assign ((size_t) K, 0.0f); sens.assign ((size_t) K, 0.0f); fade.assign ((size_t) K, 1.0f);
        pow.assign ((size_t) K, 0.0f); magSm.assign ((size_t) K, -120.0f);
        hz.resize ((size_t) K);
        for (int k = 0; k < K; ++k) hz[(size_t) k] = (float) (k * sr / N);
        sensDirty = true;
    }
    void reset() { std::fill (state.begin(), state.end(), 0.0f); std::fill (pow.begin(), pow.end(), 0.0f); }

    void compute (const float* magDb, float* reductionDb, const Settings& s)
    {
        updateSensitivity (s);
        const float det = juce::jlimit (0.0f, 1.0f, s.detail / 100.0f);
        // espectro promediado (4-20 ms segun el ataque): las resonancias se sostienen, los picos de ruido no
        const float tau = std::clamp (s.attackMs * 3.0f, 4.0f, 20.0f) / 1000.0f;
        const float pa = std::exp (-(float) hop / (float) sr / tau);
        for (int k = 0; k < K; ++k)
        {
            const float p = std::pow (10.0f, magDb[k] / 10.0f);
            pow[(size_t) k] = pow[(size_t) k] * pa + p * (1.0f - pa);
            magSm[(size_t) k] = 10.0f * std::log10 (pow[(size_t) k] + 1e-14f);
        }
        magDb = magSm.data();
        // referencia: promedio de +-0.5 octava alrededor de cada bin
        logAverage (magDb, ref.data(), 1.0f);
        float thr = 2.0f + 5.0f * det;
        const float knee = s.mode == 0 ? 6.0f : 1.5f;
        if (s.mode == 0)
        {
            // umbral adaptativo: se adapta a lo "picudo" del material
            double sum = 0; int cnt = 0;
            for (int k = 1; k < K; ++k)
                if (hz[(size_t) k] > 100.0f && hz[(size_t) k] < 16000.0f && magDb[k] > -85.0f)
                { const float e = magDb[k] - ref[(size_t) k]; if (e > 0) { sum += e; ++cnt; } }
            thr += cnt > 0 ? 0.9f * (float) (sum / cnt) : 0.0f;
        }
        const float kd = std::min (0.97f, s.depth / (s.depth + 4.0f) * (s.mode == 1 ? 1.2f : 1.0f)) * (0.6f + s.depth / 10.0f);
        for (int k = 0; k < K; ++k)
        {
            float r = 0.0f;
            if (magDb[k] > -85.0f && hz[(size_t) k] >= 20.0f && s.depth > 0.0f)
            {
                const float e = magDb[k] - ref[(size_t) k] + 0.6f * sens[(size_t) k] - thr;
                float ke;
                if (e <= -knee * 0.5f) ke = 0.0f;
                else if (e < knee * 0.5f) ke = (e + knee * 0.5f) * (e + knee * 0.5f) / (2.0f * knee);
                else ke = e;
                r = std::min (s.maxCut, kd * ke) * fade[(size_t) k];
            }
            raw[(size_t) k] = r;
        }
        // agudeza: menos detalle = cortes mas anchos (se ensancha sin perder profundidad)
        spread (raw.data(), sm.data(), 0.30f - 0.27f * det);
        const float frameSec = (float) hop / (float) sr;
        const float a = std::exp (-frameSec / std::max (0.0003f, s.attackMs / 1000.0f));
        const float rl = std::exp (-frameSec / std::max (0.002f, s.releaseMs / 1000.0f));
        for (int k = 0; k < K; ++k)
        {
            const float target = sm[(size_t) k];
            float& st = state[(size_t) k];
            st = target + (st - target) * (target > st ? a : rl);
            reductionDb[k] = st;
        }
    }
    int bins() const { return K; }

private:
    void updateSensitivity (const Settings& s)
    {
        const bool same = ! sensDirty && s.tilt == last.tilt && s.lowCut == last.lowCut && s.highCut == last.highCut
                          && s.nf == last.nf && s.ng == last.ng && s.nq == last.nq;
        if (same) return;
        for (int k = 0; k < K; ++k) { sens[(size_t) k] = sensitivityDb (std::max (hz[(size_t) k], 10.0f), s); fade[(size_t) k] = cutFade (std::max (hz[(size_t) k], 10.0f), s); }
        last = s; sensDirty = false;
    }
    // ensanchado: maximo ponderado (triangular) en una ventana de 'oct' octavas
    void spread (const float* in, float* out, float oct)
    {
        const float hi = std::pow (2.0f, oct * 0.5f) - 1.0f;
        for (int k = 0; k < K; ++k) out[k] = in[k];
        for (int k = 1; k < K; ++k)
        {
            const float v = in[k];
            if (v <= 0.01f) continue;
            const int wdt = std::clamp ((int) std::ceil (k * hi), 1, 48);
            for (int d = 1; d <= wdt; ++d)
            {
                const float wv = v * (1.0f - (float) d / (float) (wdt + 1));
                if (k - d >= 0 && out[k - d] < wv) out[k - d] = wv;
                if (k + d < K && out[k + d] < wv) out[k + d] = wv;
            }
        }
    }
    // promedio en una ventana de 'oct' octavas centrada en cada bin, ponderado 1/f (promedio
    // uniforme en octavas: un espectro inclinado no crea "picos" falsos)
    void logAverage (const float* in, float* out, float oct)
    {
        prefix[0] = 0.0f; prefixW[0] = 0.0f;
        for (int k = 0; k < K; ++k)
        {
            const float wk = 1.0f / (float) std::max (k, 1);
            prefix[(size_t) k + 1] = prefix[(size_t) k] + std::max (in[k], -120.0f) * wk;
            prefixW[(size_t) k + 1] = prefixW[(size_t) k] + wk;
        }
        const float lo = std::pow (2.0f, -oct * 0.5f), hi = std::pow (2.0f, oct * 0.5f);
        for (int k = 0; k < K; ++k)
        {
            const int a = std::max (0, (int) std::floor (k * lo)), b = std::min (K - 1, std::max (a, (int) std::ceil (k * hi)));
            out[k] = (prefix[(size_t) b + 1] - prefix[(size_t) a]) / (prefixW[(size_t) b + 1] - prefixW[(size_t) a]);
        }
    }

    double sr = 44100.0;
    int N = 2048, hop = 512, K = 1025;
    std::vector<float> prefix, prefixW, ref, raw, sm, state, sens, fade, hz, pow, magSm;
    Settings last;
    bool sensDirty = true;
};

//==============================================================================
// Datos para la grafica (un hilo escribe, la interfaz lee)
struct Display
{
    static constexpr int points = 256;
    std::array<std::atomic<float>, points> inDb {}, outDb {}, redDb {};
    static float freqOf (int i) { return 20.0f * std::pow (1000.0f, (float) i / (float) (points - 1)); }
    void write (const float* magDb, const float* red, int K, double sr, int N)
    {
        for (int i = 0; i < points; ++i)
        {
            const float f0 = freqOf (i) / std::pow (2.0f, 0.02f), f1 = freqOf (i) * std::pow (2.0f, 0.02f);
            int a = (int) std::floor (f0 * N / sr), b = (int) std::ceil (f1 * N / sr);
            a = std::clamp (a, 1, K - 1); b = std::clamp (b, a, K - 1);
            float m = -120.0f, r = 0.0f;
            for (int k = a; k <= b; ++k) { m = std::max (m, magDb[k]); r = std::max (r, red[k]); }
            const float pin = inDb[(size_t) i].load();
            inDb[(size_t) i].store (m > pin ? m : pin + (m - pin) * 0.25f);
            outDb[(size_t) i].store (inDb[(size_t) i].load() - r);
            const float pr = redDb[(size_t) i].load();
            redDb[(size_t) i].store (r > pr ? r : pr + (r - pr) * 0.3f);
        }
    }
};

//==============================================================================
// Motor de fase lineal: STFT con ventana raiz-Hann y 75 % de solape. Latencia = N.
class SpectralEngine
{
public:
    void prepare (double sampleRate, int order)
    {
        sr = sampleRate;
        N = 1 << order; hop = N / 4; K = N / 2 + 1;
        fft = std::make_unique<juce::dsp::FFT> (order);
        win.resize ((size_t) N);
        for (int i = 0; i < N; ++i) win[(size_t) i] = std::sqrt (0.5f - 0.5f * std::cos (6.2831853f * (float) i / (float) N));
        for (int c = 0; c < 2; ++c)
        {
            in[c].assign ((size_t) N, 0.0f); det[c].assign ((size_t) N, 0.0f); out[c].assign ((size_t) N, 0.0f);
            spec[c].assign ((size_t) N * 2, 0.0f); dspec[c].assign ((size_t) N * 2, 0.0f);
            mag[c].assign ((size_t) K, -120.0f); red[c].assign ((size_t) K, 0.0f);
            gc[c].prepare (sr, N, hop);
        }
        magLink.assign ((size_t) K, -120.0f);
        tmp.assign ((size_t) K, 0.0f);
        w = 0; hc = 0;
    }
    int latency() const { return N; }
    void reset() { for (int c = 0; c < 2; ++c) { std::fill (in[c].begin(), in[c].end(), 0.0f); std::fill (det[c].begin(), det[c].end(), 0.0f); std::fill (out[c].begin(), out[c].end(), 0.0f); gc[c].reset(); } w = hc = 0; }

    // p: canales a procesar (se sobrescriben). d: deteccion (nullptr = el mismo audio)
    void process (float* const* p, const float* const* d, int numCh, int n, bool linked, const Settings& s, Display* disp)
    {
        for (int i = 0; i < n; ++i)
        {
            for (int c = 0; c < numCh; ++c)
            {
                in[c][(size_t) w] = p[c][i];
                det[c][(size_t) w] = d != nullptr ? d[c][i] : p[c][i];
                p[c][i] = out[c][(size_t) w];
                out[c][(size_t) w] = 0.0f;
            }
            w = (w + 1) % N;
            if (++hc >= hop) { hc = 0; frame (numCh, linked, d != nullptr, s, disp); }
        }
    }

private:
    void toMagDb (const std::vector<float>& sp, std::vector<float>& m)
    {
        const float norm = 4.0f / (float) N;   // ~0 dBFS para un seno a escala completa
        for (int k = 0; k < K; ++k)
        {
            const float re = sp[(size_t) (2 * k)], im = sp[(size_t) (2 * k + 1)];
            m[(size_t) k] = 10.0f * std::log10 ((re * re + im * im) * norm * norm + 1e-14f);
        }
    }
    void frame (int numCh, bool linked, bool separateDet, const Settings& s, Display* disp)
    {
        for (int c = 0; c < numCh; ++c)
        {
            auto& sp = spec[c];
            std::fill (sp.begin(), sp.end(), 0.0f);
            for (int j = 0; j < N; ++j) sp[(size_t) j] = in[c][(size_t) ((w + j) % N)] * win[(size_t) j];
            fft->performRealOnlyForwardTransform (sp.data());
            if (separateDet)
            {
                auto& dp = dspec[c];
                std::fill (dp.begin(), dp.end(), 0.0f);
                for (int j = 0; j < N; ++j) dp[(size_t) j] = det[c][(size_t) ((w + j) % N)] * win[(size_t) j];
                fft->performRealOnlyForwardTransform (dp.data());
                toMagDb (dp, mag[c]);
            }
            else toMagDb (sp, mag[c]);
        }
        if (linked || numCh == 1)
        {
            for (int k = 0; k < K; ++k)
            {
                double pw = 0; for (int c = 0; c < numCh; ++c) pw += std::pow (10.0, mag[c][(size_t) k] / 10.0);
                magLink[(size_t) k] = (float) (10.0 * std::log10 (pw / numCh + 1e-14));
            }
            gc[0].compute (magLink.data(), red[0].data(), s);
            for (int c = 1; c < numCh; ++c) red[c] = red[0];
            if (disp != nullptr) disp->write (magLink.data(), red[0].data(), K, sr, N);
        }
        else
        {
            for (int c = 0; c < numCh; ++c) gc[c].compute (mag[c].data(), red[c].data(), s);
            if (disp != nullptr)
            {
                for (int k = 0; k < K; ++k)
                {
                    magLink[(size_t) k] = std::max (mag[0][(size_t) k], mag[1][(size_t) k]);
                    tmp[(size_t) k] = std::max (red[0][(size_t) k], red[1][(size_t) k]);
                }
                disp->write (magLink.data(), tmp.data(), K, sr, N);
            }
        }
        for (int c = 0; c < numCh; ++c)
        {
            auto& sp = spec[c];
            for (int k = 0; k < K; ++k)
            {
                const float g = std::pow (10.0f, -red[c][(size_t) k] / 20.0f);
                sp[(size_t) (2 * k)] *= g; sp[(size_t) (2 * k + 1)] *= g;
                if (k > 0 && k < N / 2) { sp[(size_t) (2 * (N - k))] *= g; sp[(size_t) (2 * (N - k) + 1)] *= g; }
            }
            fft->performRealOnlyInverseTransform (sp.data());
            for (int j = 0; j < N; ++j) out[c][(size_t) ((w + j) % N)] += sp[(size_t) j] * win[(size_t) j] * 0.5f;
        }
    }

    double sr = 44100.0;
    int N = 2048, hop = 512, K = 1025, w = 0, hc = 0;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> win, magLink, tmp;
    std::array<std::vector<float>, 2> in, det, out, spec, dspec, mag, red;
    std::array<GainComputer, 2> gc;
};

//==============================================================================
// Motor de baja latencia: analiza con STFT (solo deteccion) y corta con 48 filtros de campana.
// El audio no se retrasa: latencia 0.
class FilterBankEngine
{
public:
    static constexpr int bands = 48;
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        const int order = sr > 60000 ? 11 : 10;
        N = 1 << order; hop = N / 4; K = N / 2 + 1;
        fft = std::make_unique<juce::dsp::FFT> (order);
        win.resize ((size_t) N);
        for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (6.2831853f * (float) i / (float) N);
        for (int c = 0; c < 2; ++c)
        {
            det[c].assign ((size_t) N, 0.0f); spec.assign ((size_t) N * 2, 0.0f);
            mag[c].assign ((size_t) K, -120.0f); red[c].assign ((size_t) K, 0.0f);
            gc[c].prepare (sr, N, hop);
            for (int b = 0; b < bands; ++b) { filt[c][(size_t) b].setIdentity(); filt[c][(size_t) b].reset(); cur[c][(size_t) b] = 0.0f; tgt[c][(size_t) b] = 0.0f; lastSet[c][(size_t) b] = 0.0f; }
        }
        magLink.assign ((size_t) K, -120.0f);
        for (int b = 0; b < bands; ++b) fc[(size_t) b] = 35.0f * std::pow (18000.0f / 35.0f, (float) b / (float) (bands - 1));
        w = hc = upd = 0;
    }
    void reset() { for (int c = 0; c < 2; ++c) { gc[c].reset(); for (auto& f : filt[c]) f.reset(); } }

    void process (float* const* p, const float* const* d, int numCh, int n, bool linked, const Settings& s, Display* disp)
    {
        for (int i = 0; i < n; ++i)
        {
            for (int c = 0; c < numCh; ++c) det[c][(size_t) w] = d != nullptr ? d[c][i] : p[c][i];
            w = (w + 1) % N;
            if (++hc >= hop) { hc = 0; analyse (numCh, linked, s, disp); }
            if (++upd >= 32) { upd = 0; updateFilters (numCh); }
            for (int c = 0; c < numCh; ++c)
            {
                float x = p[c][i];
                for (int b = 0; b < bands; ++b) x = filt[c][(size_t) b].process (x);
                p[c][i] = x;
            }
        }
    }

private:
    void analyse (int numCh, bool linked, const Settings& s, Display* disp)
    {
        const float norm = 4.0f / (float) N;
        for (int c = 0; c < numCh; ++c)
        {
            std::fill (spec.begin(), spec.end(), 0.0f);
            for (int j = 0; j < N; ++j) spec[(size_t) j] = det[c][(size_t) ((w + j) % N)] * win[(size_t) j];
            fft->performRealOnlyForwardTransform (spec.data(), true);
            for (int k = 0; k < K; ++k)
            {
                const float re = spec[(size_t) (2 * k)], im = spec[(size_t) (2 * k + 1)];
                mag[c][(size_t) k] = 10.0f * std::log10 ((re * re + im * im) * norm * norm + 1e-14f);
            }
        }
        if (linked || numCh == 1)
        {
            for (int k = 0; k < K; ++k)
            {
                double pw = 0; for (int c = 0; c < numCh; ++c) pw += std::pow (10.0, mag[c][(size_t) k] / 10.0);
                magLink[(size_t) k] = (float) (10.0 * std::log10 (pw / numCh + 1e-14));
            }
            gc[0].compute (magLink.data(), red[0].data(), s);
            for (int c = 1; c < numCh; ++c) red[c] = red[0];
        }
        else for (int c = 0; c < numCh; ++c) gc[c].compute (mag[c].data(), red[c].data(), s);
        if (disp != nullptr) disp->write (linked || numCh == 1 ? magLink.data() : mag[0].data(), red[0].data(), K, sr, N);
        // reduccion promedio de cada banda
        const float half = std::pow (2.0f, 0.5f * std::log2 (18000.0f / 35.0f) / (bands - 1));
        for (int c = 0; c < numCh; ++c)
            for (int b = 0; b < bands; ++b)
            {
                const int a = std::max (1, (int) std::floor (fc[(size_t) b] / half * N / sr)), e = std::min (K - 1, std::max (a, (int) std::ceil (fc[(size_t) b] * half * N / sr)));
                float acc = 0.0f, mx = 0.0f;
                for (int k = a; k <= e; ++k) { acc += red[c][(size_t) k]; mx = std::max (mx, red[c][(size_t) k]); }
                tgt[c][(size_t) b] = 0.5f * (0.5f * acc / (float) (e - a + 1) + 0.5f * mx) * 1.25f;
            }
    }
    void updateFilters (int numCh)
    {
        for (int c = 0; c < numCh; ++c)
            for (int b = 0; b < bands; ++b)
            {
                float& v = cur[c][(size_t) b];
                v += (tgt[c][(size_t) b] - v) * 0.35f;
                float& ls = lastSet[c][(size_t) b];
                if (std::abs (v - ls) < 0.04f || fc[(size_t) b] >= sr * 0.45) continue;
                ls = v;
                if (v < 0.02f) filt[c][(size_t) b].setIdentity();
                else filt[c][(size_t) b].setPeak (sr, fc[(size_t) b], 4.5, -v);
            }
    }

    double sr = 44100.0;
    int N = 1024, hop = 256, K = 513, w = 0, hc = 0, upd = 0;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> win, spec, magLink;
    std::array<std::vector<float>, 2> det, mag, red;
    std::array<GainComputer, 2> gc;
    std::array<std::array<tvc::Biquad, bands>, 2> filt;
    std::array<std::array<float, bands>, 2> cur {}, tgt {}, lastSet {};
    std::array<float, bands> fc {};
};
} // namespace tsm
