#pragma once
#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

// TOP SPECTRA EQ: motor de fase lineal y espectral.
//  * FFT compleja radix-2 en doble precision.
//  * FirStage: convolucion "overlap-save" estereo con matriz de nucleos 2x2 (L/R/M/S por banda), salto de 512
//    muestras (a 48 kHz) y cambio de nucleo con fundido (sin clics cuando la EQ dinamica o espectral se mueve).
//  * Los nucleos salen de la curva deseada (mascara real, fase cero) -> IFFT -> centrado -> ventana Blackman-Harris.
namespace spq
{
class FFT
{
public:
    void prepare (int n)
    {
        if (n == N) return;
        N = n; lg = 0; while ((1 << lg) < N) ++lg;
        rev.assign ((size_t) N, 0);
        for (int i = 0; i < N; ++i) { int r = 0; for (int b = 0; b < lg; ++b) if (i & (1 << b)) r |= 1 << (lg - 1 - b); rev[(size_t) i] = r; }
        twr.resize ((size_t) N / 2); twi.resize ((size_t) N / 2);
        for (int i = 0; i < N / 2; ++i) { twr[(size_t) i] = std::cos (2.0 * 3.14159265358979323846 * i / N); twi[(size_t) i] = -std::sin (2.0 * 3.14159265358979323846 * i / N); }
    }
    // inverse = true: sin escalar (la escala 1/N la pone quien llama). Aritmetica manual (sin __muldc3)
    void run (std::complex<double>* x, bool inverse) const
    {
        double* d = reinterpret_cast<double*> (x);
        for (int i = 0; i < N; ++i) { const int r = rev[(size_t) i]; if (i < r) { std::swap (d[2 * i], d[2 * r]); std::swap (d[2 * i + 1], d[2 * r + 1]); } }
        const double sg = inverse ? -1.0 : 1.0;
        for (int len = 2; len <= N; len <<= 1)
        {
            const int half = len >> 1, step = N / len;
            for (int i = 0; i < N; i += len)
            {
                double* a = d + 2 * i; double* b = d + 2 * (i + half);
                for (int j = 0; j < half; ++j)
                {
                    const double wr = twr[(size_t) (j * step)], wi = sg * twi[(size_t) (j * step)];
                    const double br = b[2 * j], bi = b[2 * j + 1];
                    const double vr = br * wr - bi * wi, vi = br * wi + bi * wr;
                    const double ar = a[2 * j], ai = a[2 * j + 1];
                    a[2 * j] = ar + vr; a[2 * j + 1] = ai + vi; b[2 * j] = ar - vr; b[2 * j + 1] = ai - vi;
                }
            }
        }
    }
    int size() const { return N; }
private:
    int N = 0, lg = 0;
    std::vector<int> rev;
    std::vector<double> twr, twi;
};

using cvec = std::vector<std::complex<double>>;

class FirStage
{
public:
    // reserva todo para el nucleo mas largo (no hay memoria nueva en el hilo de audio)
    void prepare (int maxK, int maxHop)
    {
        H = maxHop; maxN = 1; while (maxN < maxK + H) maxN <<= 1;
        for (auto* v : { &inL, &inR }) v->assign ((size_t) maxN, 0.0);
        for (auto* v : { &newL, &newR, &outL, &outR }) v->assign ((size_t) H, 0.0);
        for (auto& k : cur) k.assign ((size_t) maxN, 0.0);
        for (auto& k : nxt) k.assign ((size_t) maxN, 0.0);
        work.assign ((size_t) maxN, 0.0); work2.assign ((size_t) maxN, 0.0); kbuf.assign ((size_t) maxK, 0.0);
        tmpL.assign ((size_t) H, 0.0); tmpR.assign ((size_t) H, 0.0);
        win.assign ((size_t) maxK, 0.0);
        K = 0;
    }
    void configure (int k, int hopSize)
    {
        K = k; H = hopSize; N = 1; while (N < K + H) N <<= 1;
        fft.prepare (N); kfft.prepare (K);
        for (int i = 0; i < K; ++i)
        {
            const double t = 2.0 * 3.14159265358979323846 * i / K;            // Blackman-Harris 4 terminos (-92 dB)
            win[(size_t) i] = 0.35875 - 0.48829 * std::cos (t) + 0.14128 * std::cos (2 * t) - 0.01168 * std::cos (3 * t);
        }
        reset();
        hasCross = false; pendingNew = false; useCross = false;
        // nucleo identidad (impulso centrado) en P y V
        std::vector<double> one ((size_t) K / 2 + 1, 1.0);
        design (one.data(), nullptr, nullptr, one.data());
        for (int i = 0; i < 4; ++i) std::swap (cur[i], nxt[i]);
        curCross = nxtCross; pendingNew = false;
    }
    void reset()
    {
        std::fill (newL.begin(), newL.end(), 0.0); std::fill (newR.begin(), newR.end(), 0.0);
        std::fill (inL.begin(), inL.end(), 0.0); std::fill (inR.begin(), inR.end(), 0.0);
        std::fill (outL.begin(), outL.end(), 0.0); std::fill (outR.begin(), outR.end(), 0.0);
        pos = 0;
    }
    int latency() const { return K / 2 + H; }
    int hop() const { return H; }
    int kernelLength() const { return K; }
    int toBoundary() const { return H - pos; }

    // mascaras reales (fase cero) en K/2+1 puntos de 0 a fs/2: L' = P*L + Q*R, R' = U*L + V*R (Q, U pueden ser nulos)
    void design (const double* P, const double* Q, const double* U, const double* V)
    {
        nxtCross = Q != nullptr || U != nullptr;
        // los nucleos se disenan de a dos (mascara A + j mascara B: ambas reales y pares -> impulsos reales)
        const bool same = (P == V || V == nullptr) && (Q == U);                  // sin bandas L/R: V = P y U = Q
        designPair (P, Q, nxt[0], nxt[1]);
        if (same) { nxt[3] = nxt[0]; nxt[2] = nxt[1]; }
        else designPair (V, U, nxt[3], nxt[2]);
        pendingNew = true;
    }
    void designPair (const double* A, const double* B, cvec& outA, cvec& outB)
    {
        if (A == nullptr && B == nullptr) { std::fill (outA.begin(), outA.begin() + N, 0.0); std::fill (outB.begin(), outB.begin() + N, 0.0); return; }
        for (int k = 0; k <= K / 2; ++k) work[(size_t) k] = { A != nullptr ? A[k] : 0.0, B != nullptr ? B[k] : 0.0 };
        for (int k = 1; k < K / 2; ++k) work[(size_t) (K - k)] = work[(size_t) k];
        kfft.run (work.data(), true);                                          // impulsos de fase cero (circulares)
        std::fill (work2.begin(), work2.begin() + N, 0.0);
        for (int n = 0; n < K; ++n) work2[(size_t) n] = work[(size_t) ((n - K / 2 + K) % K)] * (win[(size_t) n] / K);
        fft.run (work2.data(), false);
        for (int n = 0; n < N; ++n)
        {
            const auto zz = work2[(size_t) n], zc = std::conj (work2[(size_t) ((N - n) & (N - 1))]);
            outA[(size_t) n] = { 0.5 * (zz.real() + zc.real()), 0.5 * (zz.imag() + zc.imag()) };
            outB[(size_t) n] = { 0.5 * (zz.imag() - zc.imag()), -0.5 * (zz.real() - zc.real()) };
        }
        if (A == nullptr) std::fill (outA.begin(), outA.begin() + N, 0.0);
        if (B == nullptr) std::fill (outB.begin(), outB.begin() + N, 0.0);
    }
    inline void push (double l, double r, double& ol, double& orr) noexcept
    {
        newL[(size_t) pos] = l; newR[(size_t) pos] = r;
        ol = outL[(size_t) pos]; orr = outR[(size_t) pos];
        if (++pos >= H) { block(); pos = 0; }
    }

private:
    void convolve (const cvec* k, bool cross, std::vector<double>& yl, std::vector<double>& yr)
    {
        // espectros de L y R desde una FFT compleja de (L + jR)
        const double* z = reinterpret_cast<const double*> (work.data()); double* o = reinterpret_cast<double*> (work2.data());
        const double* k0 = reinterpret_cast<const double*> (k[0].data()); const double* k1 = reinterpret_cast<const double*> (k[1].data());
        const double* k2 = reinterpret_cast<const double*> (k[2].data()); const double* k3 = reinterpret_cast<const double*> (k[3].data());
        for (int n = 0; n < N; ++n)
        {
            const int m = (N - n) & (N - 1);
            const double zr = z[2 * n], zi = z[2 * n + 1], cr = z[2 * m], ci = -z[2 * m + 1];
            const double lr = 0.5 * (zr + cr), li = 0.5 * (zi + ci);                  // X_L
            const double rr = 0.5 * (zi - ci), ri = -0.5 * (zr - cr);                 // X_R = -j/2 (z - zc)
            double yLr = k0[2 * n] * lr - k0[2 * n + 1] * li, yLi = k0[2 * n] * li + k0[2 * n + 1] * lr;
            double yRr = k3[2 * n] * rr - k3[2 * n + 1] * ri, yRi = k3[2 * n] * ri + k3[2 * n + 1] * rr;
            if (cross)
            {
                yLr += k1[2 * n] * rr - k1[2 * n + 1] * ri; yLi += k1[2 * n] * ri + k1[2 * n + 1] * rr;
                yRr += k2[2 * n] * lr - k2[2 * n + 1] * li; yRi += k2[2 * n] * li + k2[2 * n + 1] * lr;
            }
            o[2 * n] = yLr - yRi; o[2 * n + 1] = yLi + yRr;                        // Y_L + j Y_R
        }
        fft.run (work2.data(), true);
        for (int i = 0; i < H; ++i) { const auto v = work2[(size_t) (N - H + i)] / (double) N; yl[(size_t) i] = v.real(); yr[(size_t) i] = v.imag(); }
    }
    void block()
    {
        // la ventana de entrada avanza H muestras
        std::move (inL.begin() + H, inL.begin() + N, inL.begin()); std::move (inR.begin() + H, inR.begin() + N, inR.begin());
        std::copy (newL.begin(), newL.end(), inL.begin() + (N - H)); std::copy (newR.begin(), newR.end(), inR.begin() + (N - H));
        for (int n = 0; n < N; ++n) work[(size_t) n] = { inL[(size_t) n], inR[(size_t) n] };
        fft.run (work.data(), false);
        convolve (cur, curCross, outL, outR);
        if (pendingNew)
        {
            convolve (nxt, nxtCross, tmpL, tmpR);
            for (int i = 0; i < H; ++i)
            {
                const double t = 0.5 - 0.5 * std::cos (3.14159265358979323846 * (i + 0.5) / H);    // fundido coseno
                outL[(size_t) i] += (tmpL[(size_t) i] - outL[(size_t) i]) * t;
                outR[(size_t) i] += (tmpR[(size_t) i] - outR[(size_t) i]) * t;
            }
            for (int i = 0; i < 4; ++i) std::swap (cur[i], nxt[i]);
            curCross = nxtCross; pendingNew = false;
        }
    }
    FFT fft, kfft;
    int K = 0, N = 0, H = 512, maxN = 0, pos = 0;
    std::vector<double> inL, inR, newL, newR, outL, outR, kbuf, win, tmpL, tmpR;
    cvec work, work2;
    cvec cur[4], nxt[4];
    bool curCross = false, nxtCross = false, pendingNew = false, hasCross = false, useCross = false;
public:
    bool atHopStart() const { return pos == 0; }
};

// analisis espectral para la dinamica espectral: dos senales por FFT (Hann), nivel en dB por bin (0 dB = seno a 0 dBFS)
class SpecAnalyzer
{
public:
    void prepare (int size)
    {
        Ka = size; fft.prepare (Ka);
        bufA.assign ((size_t) Ka, 0.0); bufB.assign ((size_t) Ka, 0.0); work.assign ((size_t) Ka, 0.0);
        win.resize ((size_t) Ka); for (int i = 0; i < Ka; ++i) win[(size_t) i] = 0.5 - 0.5 * std::cos (2.0 * 3.14159265358979323846 * i / Ka);
        dbA.assign ((size_t) Ka / 2 + 1, -150.0); dbB.assign ((size_t) Ka / 2 + 1, -150.0); w = 0;
    }
    inline void push (double a, double b) noexcept { bufA[(size_t) w] = a; bufB[(size_t) w] = b; if (++w >= Ka) w = 0; }
    void analyze()
    {
        for (int i = 0; i < Ka; ++i) { const int j = (w + i) % Ka; work[(size_t) i] = { bufA[(size_t) j] * win[(size_t) i], bufB[(size_t) j] * win[(size_t) i] }; }
        fft.run (work.data(), false);
        const double nrm = 4.0 / Ka;
        for (int k = 0; k <= Ka / 2; ++k)
        {
            const auto z = work[(size_t) k], zc = std::conj (work[(size_t) ((Ka - k) % Ka)]);
            const double a = std::abs (0.5 * (z + zc)) * nrm, b = std::abs (0.5 * (z - zc)) * nrm;
            dbA[(size_t) k] = 20.0 * std::log10 (a + 1e-12); dbB[(size_t) k] = 20.0 * std::log10 (b + 1e-12);
        }
    }
    int size() const { return Ka; }
    std::vector<double> dbA, dbB;
private:
    FFT fft; int Ka = 2048, w = 0;
    std::vector<double> bufA, bufB, win; cvec work;
};
} // namespace spq
