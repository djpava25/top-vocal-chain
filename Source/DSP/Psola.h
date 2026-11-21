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
    static constexpr double minFreq = 80.0;      // voz mas grave por defecto (configure() puede cambiarla)
    // minFreq: voz mas grave soportada; maxFormant: rango de formantes; refine: alinear marcas por correlacion
    void configure (double minF, double maxForm, bool refineMarks) noexcept { minF_ = minF; maxFormant = maxForm; refine = refineMarks; }

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        G = std::ceil (sr / minF_);
        // el alineado de marcas necesita 1/8 de periodo extra de anticipacion
        L = std::ceil (G * maxFormant + G * (refine ? 0.65 : 0.5) + 4.0);
        int size = 1;
        while (size < (int) (L + 3 * G + 16)) size <<= 1;
        inL.assign ((size_t) size, 0.0f); inR.assign ((size_t) size, 0.0f);
        outL.assign ((size_t) size, 0.0f); outR.assign ((size_t) size, 0.0f);
        mask = size - 1;
        envCoef = (float) (1.0 - std::exp (-1.0 / (0.025 * sr)));
        gainCoef = (float) (1.0 - std::exp (-1.0 / (0.03 * sr)));
        reset();
    }

    void reset()
    {
        std::fill (inL.begin(), inL.end(), 0.0f);  std::fill (inR.begin(), inR.end(), 0.0f);
        std::fill (outL.begin(), outL.end(), 0.0f); std::fill (outR.begin(), outR.end(), 0.0f);
        t = 0; nextSynth = 0.0; mark = 0.0; havePrev = false;
        pIn = pOut = 0.0f; lvl = 1.0f;
    }

    int getLatency() const noexcept { return (int) (L + G); }
    int getLookahead() const noexcept { return (int) L; }
    // Resync: con el tono sin cambios, las marcas vuelven a caer exactamente sobre la senal original
    // (la salida pasa a ser identica a la entrada retrasada -> se puede empalmar con el original sin peine)
    void setResync (bool r) noexcept { resync = r; }
    // Mantener el volumen: compara la energia de salida con la de la entrada en el mismo instante
    void setLevelKeeper (bool k) noexcept { keepLevel = k; }

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
            // Con voz: grano de 2 periodos (PSOLA clasico). Sin voz: grano mas largo para no "picar".
            const double half = period > 0.0 ? T : std::min (G, std::max (T, H));
            const double target = (double) t - L;

            if (std::abs (target - mark) > 2.0 * T || period <= 0.0) { if (std::abs (target - mark) > 2.0 * T) mark = target; havePrev = false; }
            while (target - mark > 0.5 * T)
            {
                const double prev = mark;
                mark += T;
                // marcas sincronizadas con el ciclo real de la voz: evita la aspereza cuando el periodo estimado se desvia
                if (refine && period > 0.0 && havePrev) mark = refineMark (prev, mark, T);
                havePrev = period > 0.0;
            }
            if (resync)
            {
                if (period <= 0.0) mark = target;
                else mark -= std::clamp (mark - target, -T * 0.125, T * 0.125);
            }

            // ganancia de la superposicion: ventanas Hann de semiancho 'half' cada H muestras suman half/H.
            // Los granos se suman en parte en fase y en parte no; el resto lo corrige el control de volumen (keepLevel).
            const double scale = period > 0.0 ? std::pow (std::min (1.0, H / half), 0.6) : std::min (1.0, std::sqrt (H / half));
            const long long c = t + (long long) G;
            const int hl = (int) half;
            // coseno por recurrencia (rotacion) en vez de std::cos en cada muestra
            const double dth = 3.14159265358979 / half;
            double cr = std::cos (-dth * hl), ci = std::sin (-dth * hl);
            const double rr = std::cos (dth), ri = std::sin (dth);
            for (int k = -hl; k <= hl; ++k)
            {
                const float w = (float) (scale * (0.5 + 0.5 * cr));
                const double tmp = cr * rr - ci * ri; ci = cr * ri + ci * rr; cr = tmp;
                const double src = mark + (double) k * formant;
                const size_t o = (size_t) ((c + k) & mask);
                outL[o] += w * read (inL, src);
                if (stereo) outR[o] += w * read (inR, src);
            }
            nextSynth += std::max (8.0, H);
        }

        const size_t idx = (size_t) (t & mask);
        float ol = outL[idx], orr = stereo ? outR[idx] : 0.0f;
        outL[idx] = 0.0f; if (stereo) outR[idx] = 0.0f;
        if (keepLevel)
        {
            // energia de la entrada en el mismo instante de la salida (t - latencia)
            const size_t ri2 = (size_t) ((t - (long long) (L + G)) & mask);
            const float xi = stereo ? 0.5f * (inL[ri2] + inR[ri2]) : inL[ri2];
            const float xo = stereo ? 0.5f * (ol + orr) : ol;
            pIn += (xi * xi - pIn) * envCoef;
            pOut += (xo * xo - pOut) * envCoef;
            float tg = 1.0f;
            if (pIn > 1.0e-8f && pOut > 1.0e-8f && ! resync) tg = std::clamp (std::sqrt (pIn / pOut), 0.75f, 1.33f);
            lvl += (tg - lvl) * gainCoef;
            ol *= lvl; orr *= lvl;
        }
        l = ol; if (stereo) r = orr;
        ++t;
    }

private:
    // busca, cerca de la marca prevista, el desplazamiento cuyo ciclo mas se parece al ciclo anterior
    double refineMark (double prev, double predicted, double T) const noexcept
    {
        const int range = std::max (1, (int) (T * 0.125));
        const int halfLen = std::max (4, (int) (T * 0.5));
        const long long p0 = (long long) std::floor (prev), m0 = (long long) std::floor (predicted);
        double best = -1.0e30; int bestD = 0;
        for (int d = -range; d <= range; ++d)
        {
            double s = 0.0, e = 1.0e-9;
            for (int j = -halfLen; j < halfLen; j += 2)
            {
                const float a = inL[(size_t) ((p0 + j) & mask)], b = inL[(size_t) ((m0 + d + j) & mask)];
                s += (double) a * b; e += (double) b * b;
            }
            const double score = s / std::sqrt (e) - 0.002 * std::abs (d);
            if (score > best) { best = score; bestD = d; }
        }
        return predicted + (double) bestD;
    }

    inline float read (const std::vector<float>& b, double pos) const noexcept
    {
        const double fl = std::floor (pos);
        const float f = (float) (pos - fl);
        const long long i = (long long) fl;
        const float a = b[(size_t) (i & mask)], c = b[(size_t) ((i + 1) & mask)];
        return a + f * (c - a);
    }

    double minF_ = minFreq, maxFormant = 1.336;
    bool refine = false, havePrev = false, resync = false, keepLevel = true;
    float pIn = 0.0f, pOut = 0.0f, lvl = 1.0f, envCoef = 0.001f, gainCoef = 0.001f;
    double sr = 44100.0, G = 630.0, L = 1160.0;
    std::vector<float> inL, inR, outL, outR;
    long long t = 0, mask = 0;
    double nextSynth = 0.0, mark = 0.0;
};

//==============================================================================
// Motor REALTIME (~7 ms): remuestreo continuo + saltos de un ciclo completo.
// La cabeza de lectura avanza a la velocidad del cambio de tono; cuando se aleja de su posicion ideal salta
// exactamente un periodo (afinado por correlacion) con un fundido corto. Como las dos copias estan en fase,
// no hay "voz doble" ni efecto peine.
class CycleShifter
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        D = std::round (sr * 0.0075);
        int size = 1;
        while (size < (int) (D * 6 + sr * 0.03)) size <<= 1;
        bufL.assign ((size_t) size, 0.0f); bufR.assign ((size_t) size, 0.0f);
        mask = size - 1;
        reset();
    }
    void reset()
    {
        std::fill (bufL.begin(), bufL.end(), 0.0f); std::fill (bufR.begin(), bufR.end(), 0.0f);
        w = 0; d = D; dOld = D; xfLeft = 0; xfLen = 1;
    }
    int getLatency() const noexcept { return (int) D; }

    inline void process (float& l, float& r, bool stereo, double period, double ratio) noexcept
    {
        bufL[(size_t) (w & mask)] = l;
        if (stereo) bufR[(size_t) (w & mask)] = r;
        ratio = std::clamp (ratio, 0.5, 2.0);
        const double step = 1.0 - ratio;
        d += step;
        if (xfLeft > 0) dOld += step;
        if (xfLeft == 0)
        {
            double T = period > 0.0 ? period : sr * 0.004;
            T = std::clamp (T, 32.0, D * 1.6);
            const double lim = T * 0.55;
            if (d > D + lim || d < std::max (4.0, D - lim))
            {
                double J = d > D ? -T : T;
                if (period > 0.0) J = refineJump (d, J, T);
                if (d + J < 4.0) J += T;
                dOld = d; d += J;
                xfLen = xfLeft = (int) std::clamp (T, 48.0, sr * 0.008);
            }
        }
        float ol = read (bufL, d), orr = stereo ? read (bufR, d) : 0.0f;
        if (xfLeft > 0)
        {
            const float p = 1.0f - (float) xfLeft / (float) xfLen;
            const float wn = 0.5f - 0.5f * std::cos (3.14159265f * p);
            ol = ol * wn + read (bufL, dOld) * (1.0f - wn);
            if (stereo) orr = orr * wn + read (bufR, dOld) * (1.0f - wn);
            --xfLeft;
        }
        l = ol; if (stereo) r = orr;
        ++w;
    }

private:
    double refineJump (double dNow, double J, double T) const noexcept
    {
        const int range = std::max (1, (int) (T * 0.1));
        const int len = (int) std::min (T, 256.0);
        const long long a0 = w - (long long) std::floor (dNow);
        double best = -1.0e30, bestJ = J;
        for (int k = -range; k <= range; ++k)
        {
            const double Jc = J + k;
            if (dNow + Jc < 4.0 || dNow + Jc + len > (double) mask - 4) continue;
            const long long b0 = w - (long long) std::floor (dNow + Jc);
            double s = 0.0, e = 1.0e-9;
            for (int j = 0; j < len; j += 2)
            {
                const float a = bufL[(size_t) ((a0 - j) & mask)], b = bufL[(size_t) ((b0 - j) & mask)];
                s += (double) a * b; e += (double) b * b;
            }
            const double score = s / std::sqrt (e);
            if (score > best) { best = score; bestJ = Jc; }
        }
        return bestJ;
    }
    // interpolacion cubica (Hermite): remuestreo limpio
    inline float read (const std::vector<float>& b, double delay) const noexcept
    {
        const double pos = (double) w - delay;
        const double fl = std::floor (pos);
        const float f = (float) (pos - fl);
        const long long i = (long long) fl;
        const float xm1 = b[(size_t) ((i - 1) & mask)], x0 = b[(size_t) (i & mask)], x1 = b[(size_t) ((i + 1) & mask)], x2 = b[(size_t) ((i + 2) & mask)];
        const float c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }
    double sr = 44100.0, D = 330.0, d = 330.0, dOld = 330.0;
    std::vector<float> bufL, bufR;
    long long w = 0, mask = 0;
    int xfLeft = 0, xfLen = 1;
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
