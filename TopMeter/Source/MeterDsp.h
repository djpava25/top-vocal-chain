#pragma once
#include <array>
#include <atomic>
#include <cmath>
#include <vector>
#include <algorithm>

// Motor de medicion de TOP METER
//  - Sonoridad ITU-R BS.1770-4 / EBU R128: filtro K, Momentary (400 ms), Short-Term (3 s),
//    Integrated con compuertas (-70 LUFS absoluta, -10 LU relativa) y LRA (EBU Tech 3342)
//  - True Peak con sobremuestreo x4 (FIR polifasico de 48 coeficientes)
//  - VU, pico, RMS, correlacion y balance
namespace tmeter
{
inline float toDb (double p, float floor = -150.0f) { return p > 1.0e-15 ? (float) (10.0 * std::log10 (p)) : floor; }

struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    inline double process (double x) noexcept
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1 = z2 = 0; }
};

// filtro K (pre-filtro de cabeza + RLB) para cualquier frecuencia de muestreo
inline void designK (double fs, Biquad& shelf, Biquad& hp)
{
    {
        const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
        const double K = std::tan (3.141592653589793 * f0 / fs);
        const double Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
        const double a0 = 1.0 + K / Q + K * K;
        shelf.b0 = (Vh + Vb * K / Q + K * K) / a0; shelf.b1 = 2.0 * (K * K - Vh) / a0; shelf.b2 = (Vh - Vb * K / Q + K * K) / a0;
        shelf.a1 = 2.0 * (K * K - 1.0) / a0; shelf.a2 = (1.0 - K / Q + K * K) / a0;
    }
    {
        const double f0 = 38.13547087602444, Q = 0.5003270373238773;
        const double K = std::tan (3.141592653589793 * f0 / fs);
        const double a0 = 1.0 + K / Q + K * K;
        hp.b0 = 1.0; hp.b1 = -2.0; hp.b2 = 1.0;
        hp.a1 = 2.0 * (K * K - 1.0) / a0; hp.a2 = (1.0 - K / Q + K * K) / a0;
    }
}

// histograma de sonoridad (bins de 0.05 dB entre -70 y +10): compuertas sin guardar todos los bloques
struct LoudHist
{
    static constexpr float lo = -70.0f, hi = 10.0f, step = 0.05f;
    static constexpr int bins = 1600;
    std::array<double, bins> energy {};
    std::array<int, bins> count {};
    void reset() { energy.fill (0.0); count.fill (0); }
    void add (double power)
    {
        const float l = -0.691f + toDb (power);
        if (l < lo) return;
        const int b = std::clamp ((int) ((l - lo) / step), 0, bins - 1);
        energy[(size_t) b] += power; ++count[(size_t) b];
    }
    // energia media de los bloques por encima de 'gate' (LUFS)
    double meanAbove (float gate, int* n = nullptr) const
    {
        double e = 0; int c = 0;
        for (int b = std::max (0, (int) std::ceil ((gate - lo) / step)); b < bins; ++b) { e += energy[(size_t) b]; c += count[(size_t) b]; }
        if (n) *n = c;
        return c > 0 ? e / c : 0.0;
    }
    float integrated() const
    {
        int n = 0;
        const double m = meanAbove (lo, &n);
        if (n == 0) return -150.0f;
        const float rel = -0.691f + toDb (m) - 10.0f;
        const double m2 = meanAbove (rel, &n);
        return n > 0 ? -0.691f + toDb (m2) : -150.0f;
    }
    float percentile (float gate, float p) const
    {
        int total = 0;
        const int b0 = std::max (0, (int) std::ceil ((gate - lo) / step));
        for (int b = b0; b < bins; ++b) total += count[(size_t) b];
        if (total == 0) return -150.0f;
        const double want = p * (double) (total - 1);
        int acc = 0;
        for (int b = b0; b < bins; ++b)
        {
            acc += count[(size_t) b];
            if ((double) acc > want) return lo + ((float) b + 0.5f) * step;
        }
        return hi;
    }
    // LRA segun EBU Tech 3342: compuerta relativa -20 LU, percentiles 10 y 95
    float lra() const
    {
        int n = 0;
        const double m = meanAbove (lo, &n);
        if (n < 2) return 0.0f;
        const float rel = -0.691f + toDb (m) - 20.0f;
        return std::max (0.0f, percentile (rel, 0.95f) - percentile (rel, 0.10f));
    }
};

// True Peak: interpolacion x4 con FIR polifasico (48 coeficientes, 12 por fase)
struct TruePeak
{
    static constexpr int taps = 12, phases = 4;
    std::array<std::array<float, taps>, phases> h {};
    std::array<float, taps * 2> buf {};
    int pos = 0;
    void prepare()
    {
        const int N = taps * phases;
        std::array<double, taps * phases> c {};
        for (int n = 0; n < N; ++n)
        {
            const double t = ((double) n - (N - 1) * 0.5) / phases;
            const double sinc = std::abs (t) < 1e-9 ? 1.0 : std::sin (3.141592653589793 * t) / (3.141592653589793 * t);
            const double w = 0.42 - 0.5 * std::cos (2 * 3.141592653589793 * n / (N - 1)) + 0.08 * std::cos (4 * 3.141592653589793 * n / (N - 1));
            c[(size_t) n] = sinc * w;
        }
        for (int p = 0; p < phases; ++p)
        {
            double s = 0;
            for (int k = 0; k < taps; ++k) s += c[(size_t) (p + k * phases)];
            for (int k = 0; k < taps; ++k) h[(size_t) p][(size_t) k] = (float) (c[(size_t) (p + k * phases)] / s);
        }
        buf.fill (0); pos = 0;
    }
    inline float process (float x) noexcept
    {
        buf[(size_t) pos] = buf[(size_t) (pos + taps)] = x;
        pos = (pos + 1) % taps;
        float pk = std::abs (x);
        for (int p = 0; p < phases; ++p)
        {
            float y = 0;
            const auto& hp = h[(size_t) p];
            for (int k = 0; k < taps; ++k) y += hp[(size_t) k] * buf[(size_t) (pos + taps - 1 - k)];
            pk = std::max (pk, std::abs (y));
        }
        return pk;
    }
};

struct Meter
{
    static constexpr int historySize = 36000;          // 1 hora de valores cada 100 ms
    // ---- resultados (lectura desde la interfaz) ----
    std::atomic<float> momentary { -150 }, shortTerm { -150 }, integrated { -150 }, lra { 0 };
    std::atomic<float> momMax { -150 }, stMax { -150 }, tpMax { -150 }, tpRecent { -150 };
    std::atomic<float> peak[2], peakMax[2], rmsDb[2], rmsMax[2], tpCur[2], vu[2], vuMax[2];
    std::atomic<float> correlation { 0 }, balance { 0 };
    std::vector<float> histM, histS;                    // historia de Momentary y Short-Term (cada 100 ms)
    std::atomic<int> histCount { 0 };                   // bloques escritos desde el ultimo reset
    LoudHist gating, stHist;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (int c = 0; c < 2; ++c) { designK (sr, ks[c], kh[c]); tp[c].prepare(); }
        sub = std::max (1, (int) std::round (sr * 0.1));
        histM.assign (historySize, -150.0f); histS.assign (historySize, -150.0f);
        setRmsWindow (0.5f);
        vuCoef = (float) (1.0 - std::exp (-1.0 / (0.065 * sr)));
        corrCoef = (float) (1.0 - std::exp (-1.0 / (0.3 * sr)));
        reset();
    }
    void setRmsWindow (float sec) { rmsCoef = (float) (1.0 - std::exp (-1.0 / (std::max (0.01f, sec) * sr))); }
    void setVuRef (float dbfs) { vuRef = dbfs; }

    void reset()
    {
        for (int c = 0; c < 2; ++c)
        {
            ks[c].reset(); kh[c].reset();
            peak[c] = -150; peakMax[c] = -150; rmsDb[c] = -150; rmsMax[c] = -150; tpCur[c] = -150; vu[c] = -40; vuMax[c] = -40;
            ms[c] = 0; v1[c] = v2[c] = 0; pkAcc[c] = 0; tpAcc[c] = 0;
        }
        subs.fill (0.0); subPos = 0; subFill = 0; acc = 0; accN = 0;
        gating.reset(); stHist.reset();
        momentary = shortTerm = integrated = -150.0f; lra = 0.0f;
        momMax = stMax = tpMax = tpRecent = -150.0f;
        std::fill (histM.begin(), histM.end(), -150.0f); std::fill (histS.begin(), histS.end(), -150.0f);
        histCount = 0;
        recentTp.fill (0.0f); recentPos = 0;
        lr = l2 = r2 = 0;
    }

    // analiza un bloque estereo (R puede ser nullptr = mono)
    void process (const float* L, const float* R, int n, bool measureLoudness)
    {
        for (int i = 0; i < n; ++i)
        {
            const float x[2] { L[i], R ? R[i] : L[i] };
            for (int c = 0; c < 2; ++c)
            {
                const float a = std::abs (x[c]);
                pkAcc[c] = std::max (pkAcc[c], a);
                tpAcc[c] = std::max (tpAcc[c], tp[c].process (x[c]));
                ms[c] += (x[c] * x[c] - ms[c]) * rmsCoef;
                v1[c] += (a - v1[c]) * vuCoef; v2[c] += (v1[c] - v2[c]) * vuCoef;    // VU: 2 polos ~300 ms
            }
            lr += (x[0] * x[1] - lr) * corrCoef; l2 += (x[0] * x[0] - l2) * corrCoef; r2 += (x[1] * x[1] - r2) * corrCoef;
            if (measureLoudness)
            {
                double e = 0;
                for (int c = 0; c < (R ? 2 : 1); ++c) { const double z = kh[c].process (ks[c].process (x[c])); e += z * z; }
                acc += e; ++accN;
                if (accN >= sub) endSubBlock();
            }
        }
        for (int c = 0; c < 2; ++c)
        {
            const float pdb = toDb ((double) pkAcc[c] * pkAcc[c]);
            peak[c] = pdb; peakMax[c] = std::max (peakMax[c].load(), pdb);
            const float tdb = toDb ((double) tpAcc[c] * tpAcc[c]);
            tpCur[c] = tdb;
            tpMax = std::max (tpMax.load(), tdb);
            recentTp[(size_t) recentPos] = std::max (recentTp[(size_t) recentPos], tpAcc[c]);
            const float r = toDb (ms[c]);
            rmsDb[c] = r; rmsMax[c] = std::max (rmsMax[c].load(), r);
            // VU calibrado: seno con valor medio rectificado -> su valor eficaz (x1.111), 0 VU = referencia
            const float v = 20.0f * std::log10 (std::max (1e-9f, v2[c] * 1.1107f)) - vuRef;
            vu[c] = v; vuMax[c] = std::max (vuMax[c].load(), v);
            pkAcc[c] = 0; tpAcc[c] = 0;
        }
        const double den = std::sqrt ((double) l2 * r2);
        correlation = den > 1e-12 ? (float) std::clamp ((double) lr / den, -1.0, 1.0) : 0.0f;
        balance = (l2 + r2) > 1e-12f ? (float) std::clamp ((r2 - l2) / (l2 + r2), -1.0f, 1.0f) : 0.0f;
    }

private:
    void endSubBlock()
    {
        subs[(size_t) subPos] = acc / (double) accN;
        subPos = (subPos + 1) % 30; subFill = std::min (30, subFill + 1);
        acc = 0; accN = 0;
        auto meanLast = [this] (int k)
        {
            double e = 0; const int m = std::min (k, subFill);
            for (int j = 1; j <= m; ++j) e += subs[(size_t) ((subPos - j + 30) % 30)];
            return m > 0 ? e / k : 0.0;              // ventana completa (los bloques faltantes cuentan como silencio)
        };
        const double pm = meanLast (4), ps = meanLast (30);
        const float M = -0.691f + toDb (pm), S = -0.691f + toDb (ps);
        momentary = M; shortTerm = S;
        if (subFill >= 4) { gating.add (pm); momMax = std::max (momMax.load(), M); }
        if (subFill >= 30) { stHist.add (ps); stMax = std::max (stMax.load(), S); }
        integrated = gating.integrated();
        lra = stHist.lra();
        // pico verdadero de los ultimos 3 s (para PSR)
        float r = 0; for (float v : recentTp) r = std::max (r, v);
        tpRecent = toDb ((double) r * r);
        recentPos = (recentPos + 1) % 30; recentTp[(size_t) recentPos] = 0.0f;
        const int hc = histCount.load();
        histM[(size_t) (hc % historySize)] = subFill >= 4 ? M : -150.0f;
        histS[(size_t) (hc % historySize)] = subFill >= 30 ? S : (subFill >= 4 ? -150.0f : -150.0f);
        histCount = hc + 1;
    }

    double sr = 48000.0;
    Biquad ks[2], kh[2];
    TruePeak tp[2];
    int sub = 4800, subPos = 0, subFill = 0, accN = 0;
    double acc = 0;
    std::array<double, 30> subs {};
    std::array<float, 30> recentTp {};
    int recentPos = 0;
    float ms[2] {}, v1[2] {}, v2[2] {}, pkAcc[2] {}, tpAcc[2] {};
    float lr = 0, l2 = 0, r2 = 0;
    float rmsCoef = 0.001f, vuCoef = 0.001f, corrCoef = 0.001f, vuRef = -18.0f;
};
} // namespace tmeter
