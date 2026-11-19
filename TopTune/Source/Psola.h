#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

namespace toptune
{
//==============================================================================
// TD-PSOLA en tiempo real (estereo).
//  - Cambia el tono SIN cambiar los formantes (la voz no suena a "ardilla").
//  - Cambia los formantes de forma independiente (voz mas grave/aguda de caracter).
// Latencia fija = L + G muestras (se reporta al DAW).
class Psola
{
public:
    static constexpr double minFreq = 80.0;      // voz mas grave soportada
    static constexpr double maxFormant = 1.336;  // +5 semitonos de formante

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        G = std::ceil (sr / minFreq);
        L = std::ceil (G * maxFormant + G * 0.5 + 4.0);
        int size = 1;
        while (size < (int) (L + 3 * G + 16)) size <<= 1;
        inL.assign ((size_t) size, 0.0f); inR.assign ((size_t) size, 0.0f);
        outL.assign ((size_t) size, 0.0f); outR.assign ((size_t) size, 0.0f);
        mask = size - 1;
        reset();
    }

    void reset()
    {
        std::fill (inL.begin(), inL.end(), 0.0f);  std::fill (inR.begin(), inR.end(), 0.0f);
        std::fill (outL.begin(), outL.end(), 0.0f); std::fill (outR.begin(), outR.end(), 0.0f);
        t = 0; nextSynth = 0.0; mark = 0.0;
    }

    int getLatency() const noexcept { return (int) (L + G); }

    // period: periodo detectado en muestras (<= 0 si no hay voz)
    inline void process (float& l, float& r, bool stereo, double period, double ratio, double formant) noexcept
    {
        inL[(size_t) (t & mask)] = l;
        if (stereo) inR[(size_t) (t & mask)] = r;

        if ((double) t >= nextSynth)
        {
            double T = period > 0.0 ? period : sr * 0.006;
            T = std::clamp (T, sr / 1100.0, G);
            ratio = std::clamp (ratio, 0.25, 4.0);
            formant = std::clamp (formant, 1.0 / maxFormant, maxFormant);

            const double H = T / ratio;
            // Con voz: grano de 2 periodos (PSOLA clasico, el tono baja de verdad).
            // Sin voz (consonantes/ruido): grano mas largo para no "picar" el sonido.
            const double half = period > 0.0 ? T : std::min (G, std::max (T, H));
            const double target = (double) t - L;

            if (std::abs (target - mark) > 2.0 * T) mark = target;
            while (target - mark > 0.5 * T) mark += T;

            // Compensacion de nivel: los granos superpuestos suman de forma parcialmente coherente
            const double scale = std::min (1.0, std::pow (H / half, 0.6));
            const long long c = t + (long long) G;
            const int hl = (int) half;

            for (int k = -hl; k <= hl; ++k)
            {
                const double w = 0.5 + 0.5 * std::cos (3.14159265358979 * (double) k / half);
                const double src = mark + (double) k * formant;
                const size_t o = (size_t) ((c + k) & mask);
                outL[o] += (float) (w * scale) * read (inL, src);
                if (stereo) outR[o] += (float) (w * scale) * read (inR, src);
            }
            nextSynth += std::max (8.0, H);
        }

        const size_t idx = (size_t) (t & mask);
        l = outL[idx]; outL[idx] = 0.0f;
        if (stereo) { r = outR[idx]; outR[idx] = 0.0f; }
        ++t;
    }

private:
    inline float read (const std::vector<float>& b, double pos) const noexcept
    {
        const double fl = std::floor (pos);
        const float f = (float) (pos - fl);
        const long long i = (long long) fl;
        const float a = b[(size_t) (i & mask)], c = b[(size_t) ((i + 1) & mask)];
        return a + f * (c - a);
    }

    double sr = 44100.0, G = 552.0, L = 1016.0;
    std::vector<float> inL, inR, outL, outR;
    long long t = 0, mask = 0;
    double nextSynth = 0.0, mark = 0.0;
};

//==============================================================================
// Detector de tonalidad (perfiles de Krumhansl-Kessler)
struct KeyFinder
{
    static void find (const float hist[12], int& root, bool& minor, float& confidence)
    {
        static const double maj[12] { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
        static const double mnr[12] { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };

        double best = -2.0; root = 0; minor = false;
        for (int m = 0; m < 2; ++m)
        {
            const double* prof = m == 0 ? maj : mnr;
            for (int k = 0; k < 12; ++k)
            {
                const double c = corr (hist, prof, k);
                if (c > best) { best = c; root = k; minor = m == 1; }
            }
        }
        confidence = (float) std::clamp (best, 0.0, 1.0);
    }

private:
    static double corr (const float* h, const double* p, int shift)
    {
        double mh = 0, mp = 0;
        for (int i = 0; i < 12; ++i) { mh += h[i]; mp += p[i]; }
        mh /= 12; mp /= 12;
        double num = 0, dh = 0, dp = 0;
        for (int i = 0; i < 12; ++i)
        {
            const double a = h[(i + shift) % 12] - mh, b = p[i] - mp;
            num += a * b; dh += a * a; dp += b * b;
        }
        return dh > 0 && dp > 0 ? num / std::sqrt (dh * dp) : 0.0;
    }
};
} // namespace toptune
