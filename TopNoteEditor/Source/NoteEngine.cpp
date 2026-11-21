#include "NoteEngine.h"
#include "DSP/Biquad.h"
#include "DSP/Psola.h"
#include <cstring>

namespace ned
{
namespace
{
    float interp (const std::vector<float>& v, double pos, bool zeroIsGap)
    {
        if (v.empty()) return 0.0f;
        const int i = (int) std::floor (pos);
        if (i < 0) return v.front();
        if (i >= (int) v.size() - 1) return v.back();
        const float a = v[(size_t) i], b = v[(size_t) i + 1];
        const float f = (float) (pos - i);
        if (zeroIsGap)
        {
            if (a <= 0.0f) return f > 0.5f ? b : 0.0f;
            if (b <= 0.0f) return f < 0.5f ? a : 0.0f;
        }
        return a + f * (b - a);
    }
    float median (std::vector<float> v)
    {
        if (v.empty()) return 0.0f;
        std::nth_element (v.begin(), v.begin() + (long) v.size() / 2, v.end());
        return v[v.size() / 2];
    }
}

float Curve::midiAt (double s) const   { return interp (midi, s / hop, true); }
float Curve::smoothAt (double s) const { return interp (smooth, s / hop, true); }
float Curve::dbAt (double s) const     { return interp (db, s / hop, false); }

//==============================================================================
Curve analyse (const float* x, int n, double sr, std::atomic<float>* progress, const std::atomic<bool>* cancel)
{
    Curve c;
    c.sr = sr;
    c.hop = std::max (32, (int) std::round (sr * 0.005));
    const int F = n / c.hop + 1;
    c.midi.assign ((size_t) F, 0.0f);
    c.db.assign ((size_t) F, -120.0f);

    // nivel por cuadro (ventana de 20 ms)
    const int rw = (int) (sr * 0.010);
    for (int f = 0; f < F; ++f)
    {
        const int a = std::max (0, f * c.hop - rw), b = std::min (n, f * c.hop + rw);
        double e = 0; for (int i = a; i < b; ++i) e += (double) x[i] * x[i];
        c.db[(size_t) f] = b > a ? (float) (10.0 * std::log10 (e / (b - a) + 1e-12)) : -120.0f;
    }
    float peak = -120.0f; for (float d : c.db) peak = std::max (peak, d);
    const float floorDb = std::max (peak - 50.0f, -75.0f);

    // diezmado a ~11 kHz
    const int D = std::max (1, (int) std::round (sr / 11025.0));
    const double dsr = sr / D;
    tvc::Biquad a1, a2; a1.setLowPass (sr, dsr * 0.42, 0.707); a2.setLowPass (sr, dsr * 0.42, 0.707);
    std::vector<float> d ((size_t) (n / D + 1), 0.0f);
    for (int i = 0; i < n; ++i) { const float y = a2.process (a1.process (x[i])); if (i % D == 0) d[(size_t) (i / D)] = y; }
    const int W = (int) std::round (dsr * 0.028), tauMax = (int) std::ceil (dsr / 65.0), tauMin = std::max (2, (int) std::floor (dsr / 1100.0));
    std::vector<float> diff ((size_t) tauMax + 2);
    const int nd = (int) d.size();

    for (int f = 0; f < F; ++f)
    {
        if (cancel != nullptr && cancel->load()) return c;
        if (progress != nullptr && (f & 255) == 0) progress->store ((float) f / (float) F);
        if (c.db[(size_t) f] < floorDb) continue;
        const int start = f * c.hop / D - (W + tauMax) / 2;
        if (start < 0 || start + W + tauMax + 1 >= nd) continue;
        const float* fr = d.data() + start;
        float run = 0.0f;
        diff[0] = 1.0f;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            float s = 0.0f;
            for (int j = 0; j < W; ++j) { const float q = fr[j] - fr[j + tau]; s += q * q; }
            run += s;
            diff[(size_t) tau] = run > 0.0f ? s * (float) tau / run : 1.0f;
        }
        int best = -1; float bv = 1.0f;
        for (int tau = tauMin; tau < tauMax; ++tau)
            if (diff[(size_t) tau] < 0.15f)
            {
                while (tau + 1 < tauMax && diff[(size_t) tau + 1] < diff[(size_t) tau]) ++tau;
                best = tau; bv = diff[(size_t) tau]; break;
            }
        if (best < 0) for (int tau = tauMin; tau < tauMax; ++tau) if (diff[(size_t) tau] < bv) { bv = diff[(size_t) tau]; best = tau; }
        if (best <= tauMin || best >= tauMax || bv > 0.22f) continue;
        const float A = diff[(size_t) best - 1], B = diff[(size_t) best], C = diff[(size_t) best + 1];
        const float den = A + C - 2.0f * B;
        const float sh = std::abs (den) > 1e-9f ? std::clamp (0.5f * (A - C) / den, -0.5f, 0.5f) : 0.0f;
        const float hz = (float) (dsr / (best + sh));
        if (hz < 60.0f || hz > 1100.0f) continue;
        c.midi[(size_t) f] = 69.0f + 12.0f * std::log2 (hz / 440.0f);
    }
    // mediana de 5 para quitar saltos de octava aislados
    {
        std::vector<float> m = c.midi;
        for (int f = 2; f < F - 2; ++f)
        {
            if (c.midi[(size_t) f] <= 0.0f) continue;
            std::vector<float> w;
            for (int k = -2; k <= 2; ++k) if (c.midi[(size_t) (f + k)] > 0.0f) w.push_back (c.midi[(size_t) (f + k)]);
            const float md = median (w);
            if (std::abs (c.midi[(size_t) f] - md) > 5.0f) m[(size_t) f] = md;
        }
        c.midi = m;
        // quita islas de 1-2 cuadros
        for (int f = 1; f < F - 2; ++f)
            if (c.midi[(size_t) f] > 0 && c.midi[(size_t) f - 1] <= 0 && (c.midi[(size_t) f + 1] <= 0 || c.midi[(size_t) f + 2] <= 0))
                c.midi[(size_t) f] = 0.0f;
    }
    // curva suavizada (centro + deriva, sin vibrato): dos pasadas de media movil de +-90 ms sobre cuadros con tono
    c.smooth = c.midi;
    const int R = 18;
    for (int pass = 0; pass < 2; ++pass)
    {
        std::vector<float> o ((size_t) F, 0.0f);
        for (int f = 0; f < F; ++f)
        {
            if (c.midi[(size_t) f] <= 0.0f) continue;
            double s = 0; int k = 0;
            for (int g = std::max (0, f - R); g <= std::min (F - 1, f + R); ++g)
                if (c.smooth[(size_t) g] > 0.0f && std::abs (c.midi[(size_t) g] - c.midi[(size_t) f]) < 2.5f) { s += c.smooth[(size_t) g]; ++k; }
            o[(size_t) f] = k > 0 ? (float) (s / k) : c.midi[(size_t) f];
        }
        c.smooth = std::move (o);
    }
    if (progress != nullptr) progress->store (1.0f);
    return c;
}

//==============================================================================
void measure (Note& nt, const Curve& c)
{
    const int a = nt.s0 / c.hop, b = std::max (a + 1, nt.s1 / c.hop);
    std::vector<float> v;
    const int len = b - a, trim = len >= 10 ? len * 15 / 100 : 0;
    for (int f = a + trim; f < b - trim && f < c.frames(); ++f) if (c.midi[(size_t) f] > 0) v.push_back (c.midi[(size_t) f]);
    if (v.empty()) for (int f = a; f < b && f < c.frames(); ++f) if (c.midi[(size_t) f] > 0) v.push_back (c.midi[(size_t) f]);
    if (nt.pitched && ! v.empty()) nt.center = median (v);
    double e = 0; int k = 0;
    for (int f = a; f < b && f < c.frames(); ++f) { e += std::pow (10.0, c.db[(size_t) f] / 10.0); ++k; }
    nt.levelDb = k > 0 ? (float) (10.0 * std::log10 (e / k + 1e-12)) : -100.0f;
}

std::vector<Note> segment (const Curve& c, int n, int algorithm)
{
    const int F = c.frames();
    std::vector<Note> out;
    if (F == 0) return out;
    float peak = -120.0f; for (float d : c.db) peak = std::max (peak, d);
    const float thr = std::max (peak - 48.0f, -72.0f);
    auto silent = [&] (int f) { return c.db[(size_t) f] < thr; };
    auto voiced = [&] (int f) { return c.midi[(size_t) f] > 0.0f; };

    struct Seg { int a, b; bool pitched; };
    std::vector<Seg> segs;

    if (algorithm == Percussive)
    {
        int last = -100;
        std::vector<int> onsets;
        for (int f = 3; f < F; ++f)
        {
            const float mn = std::min ({ c.db[(size_t) f - 1], c.db[(size_t) f - 2], c.db[(size_t) f - 3] });
            if (! silent (f) && c.db[(size_t) f] - mn > 6.0f && f - last >= 10) { onsets.push_back (f - 1); last = f; }
        }
        for (size_t i = 0; i < onsets.size(); ++i)
        {
            const int a = onsets[i];
            int b = i + 1 < onsets.size() ? onsets[i + 1] : F;
            int quiet = 0;
            for (int f = a + 4; f < b; ++f) { quiet = silent (f) ? quiet + 1 : 0; if (quiet >= 8) { b = f - 7; break; } }
            segs.push_back ({ a, b, false });
        }
    }
    else
    {
        // 1) tramos con tono (huecos sin tono <= 30 ms se unen)
        std::vector<Seg> runs;
        for (int f = 0; f < F;)
        {
            if (! voiced (f)) { ++f; continue; }
            int last = f, g = f + 1;
            for (; g < F; ++g)
            {
                if (voiced (g))
                {
                    if (g - last > 1 && (g - last - 1 > 6 || std::abs (c.midi[(size_t) g] - c.midi[(size_t) last]) > 1.5f)) break;
                    last = g;
                }
                else if (g - last > 6) break;
            }
            runs.push_back ({ f, last + 1, true });
            f = last + 1;
        }
        // 2) dentro de cada tramo: corte cuando el tono cambia de nota (no por vibrato)
        auto refOf = [&] (int s, int e) { std::vector<float> v; for (int k = s; k < e; ++k) if (voiced (k)) v.push_back (c.midi[(size_t) k]); return median (v); };
        for (const auto& r : runs)
        {
            int segStart = r.a, cnt = 0;
            float ref = refOf (r.a, std::min (r.b, r.a + 6));
            for (int f = r.a; f < r.b; ++f)
            {
                if (! voiced (f)) continue;
                const float p = c.midi[(size_t) f];
                if (std::abs (p - ref) > 0.8f)
                {
                    if (++cnt >= 8)
                    {
                        const int first = f - cnt + 1;
                        int s = first, lo = std::max (segStart + 4, first - 3), hi = std::min (r.b - 2, first + 3);
                        float bestSlope = -1;
                        for (int k = lo; k <= hi; ++k)
                            if (k > 0 && k + 1 < F && voiced (k - 1) && voiced (k + 1))
                            {
                                const float sl = std::abs (c.midi[(size_t) k + 1] - c.midi[(size_t) k - 1]);
                                if (sl > bestSlope) { bestSlope = sl; s = k; }
                            }
                        if (s - segStart >= 6) { segs.push_back ({ segStart, s, true }); segStart = s; }
                        ref = refOf (first, f + 1);
                        cnt = 0;
                    }
                }
                else { cnt = 0; ref += 0.03f * (p - ref); }
            }
            segs.push_back ({ segStart, r.b, true });
        }
        // 3) cortes por silabas: valles de volumen de 8 dB entre dos picos
        std::vector<Seg> split;
        for (const auto& s : segs)
        {
            int a = s.a;
            for (int m = s.a + 8; m < s.b - 8; ++m)
            {
                const float v = c.db[(size_t) m];
                bool isMin = true;
                for (int k = -4; k <= 4 && isMin; ++k) if (c.db[(size_t) (m + k)] < v) isMin = false;
                if (! isMin || m - a < 8) continue;
                float ml = -200, mr = -200;
                for (int k = std::max (a, m - 16); k < m; ++k) ml = std::max (ml, c.db[(size_t) k]);
                for (int k = m + 1; k <= std::min (s.b - 1, m + 16); ++k) mr = std::max (mr, c.db[(size_t) k]);
                if (ml - v > 8.0f && mr - v > 8.0f) { split.push_back ({ a, m, true }); a = m; }
            }
            split.push_back ({ a, s.b, true });
        }
        segs = split;
        // 4) consonantes: el sonido sin tono justo antes de una nota se une a ella (hasta 150 ms),
        //    y las colas sin tono despues de una nota (hasta 100 ms) a la nota anterior
        for (size_t i = 0; i < segs.size(); ++i)
        {
            const int prevEnd = i > 0 ? segs[i - 1].b : 0;
            int a = segs[i].a, k = 0;
            while (a - 1 >= prevEnd && ! silent (a - 1) && ! voiced (a - 1) && k < 30) { --a; ++k; }
            segs[i].a = a;
        }
        for (size_t i = 0; i < segs.size(); ++i)
        {
            const int nextStart = i + 1 < segs.size() ? segs[i + 1].a : F;
            int b = segs[i].b, k = 0;
            while (b < nextStart && ! silent (b) && ! voiced (b) && k < 20) { ++b; ++k; }
            segs[i].b = b;
        }
    }

    // 5) particion completa: notas, sonidos sin tono y silencios de relleno
    auto add = [&] (int a, int b, bool pitched, bool visible)
    {
        if (b <= a) return;
        Note nt;
        nt.s0 = std::min (n, a * c.hop); nt.s1 = std::min (n, b * c.hop);
        if (nt.s1 <= nt.s0) return;
        nt.t0 = nt.s0; nt.t1 = nt.s1; nt.pitched = pitched; nt.visible = visible;
        measure (nt, c);
        out.push_back (nt);
    };
    auto fillGap = [&] (int a, int b)
    {
        int f = a;
        while (f < b)
        {
            const bool snd = ! silent (f);
            int g = f;
            while (g < b && (! silent (g)) == snd) ++g;
            const bool visibleSound = snd && g - f >= 6;
            if (! out.empty() && ! out.back().visible && ! visibleSound) { out.back().s1 = out.back().t1 = std::min (n, g * c.hop); }
            else add (f, g, false, visibleSound);
            f = g;
        }
    };
    int pos = 0;
    for (const auto& s : segs)
    {
        if (s.b - s.a < 4) continue;
        const int a = std::max (s.a, pos);
        if (a > pos) fillGap (pos, a);
        add (a, s.b, s.pitched, true);
        pos = std::max (pos, s.b);
    }
    if (pos < F) fillGap (pos, F);
    if (! out.empty()) { out.back().s1 = out.back().t1 = n; }
    // centro de los sonidos sin tono: el de la nota con tono mas cercana (para dibujarlos en su sitio)
    for (size_t i = 0; i < out.size(); ++i)
    {
        if (out[i].pitched || ! out[i].visible) continue;
        float cen = algorithm == Percussive ? 60.0f : -1.0f;
        if (cen < 0)
            for (size_t d = 1; d < out.size() && cen < 0; ++d)
            {
                if (i + d < out.size() && out[i + d].pitched) cen = out[i + d].center;
                else if (i >= d && out[i - d].pitched) cen = out[i - d].center;
            }
        out[i].center = cen < 0 ? 60.0f : cen;
    }
    return out;
}

//==============================================================================
std::vector<float> wsola (const std::vector<float>& in, int outLen, double sr)
{
    const int inLen = (int) in.size();
    if (outLen == inLen || inLen < 8 || outLen < 8) { auto r = in; r.resize ((size_t) std::max (0, outLen), 0.0f); return r; }
    const int N = std::max (64, (int) (sr * 0.030)), Hs = N / 2, tol = (int) (sr * 0.008);
    const double alpha = (double) inLen / outLen;
    std::vector<float> win ((size_t) N), out ((size_t) outLen + N, 0.0f), norm ((size_t) outLen + N, 0.0f);
    for (int k = 0; k < N; ++k) win[(size_t) k] = 0.5f - 0.5f * std::cos (6.2831853f * (k + 0.5f) / N);
    auto at = [&] (int i) { return i >= 0 && i < inLen ? in[(size_t) i] : 0.0f; };
    int prev = 0;
    for (int m = 0; m * Hs < outLen; ++m)
    {
        const int nominal = (int) std::round (m * Hs * alpha);
        int pos = nominal;
        if (m > 0)
        {
            // busca el desplazamiento que mejor continua la forma de onda anterior
            const int natural = prev + Hs;
            double best = -1e30;
            for (int dlt = -tol; dlt <= tol; dlt += 2)
            {
                const int cand = nominal + dlt;
                double s = 0;
                for (int k = 0; k < Hs; k += 4) s += (double) at (cand + k) * at (natural + k);
                if (s > best) { best = s; pos = cand; }
            }
        }
        for (int k = 0; k < N; ++k)
        {
            const int o = m * Hs + k;
            out[(size_t) o] += win[(size_t) k] * at (pos + k);
            norm[(size_t) o] += win[(size_t) k];
        }
        prev = pos;
    }
    std::vector<float> r ((size_t) outLen);
    for (int i = 0; i < outLen; ++i) r[(size_t) i] = norm[(size_t) i] > 0.1f ? out[(size_t) i] / norm[(size_t) i] : out[(size_t) i];
    return r;
}

//==============================================================================
static bool pitchEdited (const Note& nt)
{
    return nt.visible && (nt.shift != 0.0f || nt.mod != 1.0f || nt.drift != 1.0f || nt.formant != 0.0f);
}

namespace
{
    struct Hash
    {
        uint64_t h = 1469598103934665603ull;
        void mix (uint64_t v) { h ^= v; h *= 1099511628211ull; }
        void f (float x) { uint32_t u; std::memcpy (&u, &x, 4); mix (u); }
    };
    // suavizado de fase cero (ida y vuelta): transiciones de tono entre notas sin escalones
    void smoothZeroPhase (std::vector<float>& v, double sr, double tau)
    {
        if (v.empty()) return;
        const float a = (float) std::exp (-1.0 / (tau * sr));
        float y = v.front();
        for (auto& x : v) { y = x + a * (y - x); x = y; }
        y = v.back();
        for (size_t i = v.size(); i-- > 0;) { y = v[i] + a * (y - v[i]); v[i] = y; }
    }
}

std::vector<float> Renderer::renderIsland (const float* src, int n, const Curve& c, const std::vector<Note>& notes, int a, int b)
{
    const int len = b - a;
    std::vector<float> corr ((size_t) len, 0.0f), form ((size_t) len, 0.0f), per ((size_t) len, 0.0f);
    size_t ni = 0;
    for (int s = a; s < b; ++s)
    {
        while (ni + 1 < notes.size() && notes[ni].s1 <= s) ++ni;
        const auto& nt = notes[ni];
        const float p = c.midiAt (s);
        const bool voiced = p > 0.0f;
        if (voiced) per[(size_t) (s - a)] = (float) (c.sr / (440.0 * std::pow (2.0, (p - 69.0) / 12.0)));
        if (! pitchEdited (nt) || s < nt.s0 || s >= nt.s1) continue;
        float cs = nt.shift;
        if (voiced && nt.pitched)
        {
            const float sm = c.smoothAt (s) > 0 ? c.smoothAt (s) : p;
            const float tgt = nt.center + nt.shift + nt.drift * (sm - nt.center) + nt.mod * (p - sm);
            cs = tgt - p;
        }
        corr[(size_t) (s - a)] = cs;
        form[(size_t) (s - a)] = nt.formant;
    }
    smoothZeroPhase (corr, c.sr, 0.010);
    smoothZeroPhase (form, c.sr, 0.010);

    toptune::Psola ps;
    ps.configure (70.0, 1.336, true);
    ps.prepare (c.sr);
    const int lat = ps.getLatency(), look = ps.getLookahead();
    std::vector<float> out ((size_t) len, 0.0f);
    const int first = a - lat - (int) (c.sr * 0.02);
    for (int i = first; i < b + lat; ++i)
    {
        float x = i >= 0 && i < n ? src[i] : 0.0f, dummy = 0.0f;
        const int q = i - look;                // el grano que se decide ahora se toma ~L muestras atras
        float cs = 0.0f, fs = 0.0f, pq = 0.0f;
        if (q >= a && q < b) { cs = corr[(size_t) (q - a)]; fs = form[(size_t) (q - a)]; pq = per[(size_t) (q - a)]; }
        else { const float p = c.midiAt (q); if (p > 0.0f) pq = (float) (c.sr / (440.0 * std::pow (2.0, (p - 69.0) / 12.0))); }
        ps.setResync (std::abs (cs) < 0.003f && std::abs (fs) < 0.003f);
        ps.process (x, dummy, false, pq, std::pow (2.0, cs / 12.0), std::pow (2.0, fs / 12.0));
        const int o = i - lat;
        if (o >= a && o < b) out[(size_t) (o - a)] = x;
    }
    return out;
}

std::vector<float> Renderer::render (const float* src, int n, const Curve& c, const std::vector<Note>& notes,
                                     const std::atomic<bool>* cancel, std::atomic<float>* progress)
{
    // 1) capa de tono continua sobre la linea de tiempo original
    std::vector<float> P (src, src + n);
    const int M = (int) (c.sr * 0.15), F = (int) (c.sr * 0.012);
    std::vector<std::pair<int, int>> islands;
    for (const auto& nt : notes)
    {
        if (! pitchEdited (nt) || nt.mute) continue;
        const int a = std::max (0, nt.s0 - M), b = std::min (n, nt.s1 + M);
        if (! islands.empty() && a <= islands.back().second + M) islands.back().second = std::max (islands.back().second, b);
        else islands.push_back ({ a, b });
    }
    std::map<uint64_t, std::vector<float>> usedIslands;
    for (size_t k = 0; k < islands.size(); ++k)
    {
        if (cancel != nullptr && cancel->load()) return {};
        if (progress != nullptr) progress->store (0.8f * (float) k / (float) islands.size());
        const auto [a, b] = islands[k];
        Hash h; h.mix ((uint64_t) a); h.mix ((uint64_t) b);
        for (const auto& nt : notes)
            if (nt.s1 > a && nt.s0 < b)
            {
                h.mix ((uint64_t) nt.s0); h.mix ((uint64_t) nt.s1); h.f (nt.shift); h.f (nt.mod); h.f (nt.drift); h.f (nt.formant); h.f (nt.center);
                h.mix (nt.pitched ? 1u : 0u); h.mix (nt.visible ? 1u : 0u); h.mix (nt.mute ? 1u : 0u);
            }
        auto u = usedIslands.find (h.h);
        if (u == usedIslands.end())
        {
            auto it = islandCache.find (h.h);
            if (it != islandCache.end()) u = usedIslands.emplace (h.h, std::move (it->second)).first;
            else u = usedIslands.emplace (h.h, renderIsland (src, n, c, notes, a, b)).first;
        }
        const auto& isl = u->second;
        for (int s = a; s < b; ++s)
        {
            float w = 1.0f;
            if (a > 0 && s - a < F) w = (float) (s - a) / (float) F;
            if (b < n && b - s < F) w = std::min (w, (float) (b - s) / (float) F);
            P[(size_t) s] = P[(size_t) s] * (1.0f - w) + isl[(size_t) (s - a)] * w;
        }
    }
    islandCache.swap (usedIslands);

    // 2) montaje: cada nota se toma de la capa continua; solo las que cambian de duracion pasan por WSOLA
    int total = n;
    for (const auto& nt : notes) total = std::max (total, nt.t1 + X);
    std::vector<float> out ((size_t) total + X, 0.0f);
    std::map<uint64_t, std::vector<float>> used;
    for (size_t idx = 0; idx < notes.size(); ++idx)
    {
        if (cancel != nullptr && cancel->load()) return {};
        if (progress != nullptr) progress->store (0.8f + 0.2f * (float) idx / (float) std::max<size_t> (1, notes.size()));
        const auto& nt = notes[idx];
        if (nt.mute) continue;
        const float gain = std::pow (10.0f, nt.gainDb / 20.0f);
        const int start = nt.t0 - X;
        const int len = (nt.t1 - nt.t0) + 2 * X;
        const int srcLen = (nt.s1 - nt.s0) + 2 * X;
        std::vector<float> piece ((size_t) srcLen);
        for (int k = 0; k < srcLen; ++k) { const int s = nt.s0 - X + k; piece[(size_t) k] = s >= 0 && s < n ? P[(size_t) s] : 0.0f; }
        const float* r = piece.data();
        if (len != srcLen)
        {
            Hash h; h.mix ((uint64_t) nt.s0); h.mix ((uint64_t) nt.s1); h.mix ((uint64_t) len);
            for (int k = 0; k < srcLen; k += 7) h.f (piece[(size_t) k]);
            auto u = used.find (h.h);
            if (u == used.end())
            {
                auto it = cache.find (h.h);
                if (it != cache.end()) u = used.emplace (h.h, std::move (it->second)).first;
                else u = used.emplace (h.h, wsola (piece, len, c.sr)).first;
            }
            r = u->second.data();
        }
        for (int k = 0; k < len; ++k)
        {
            const int o = start + k;
            if (o < 0 || o >= (int) out.size()) continue;
            float w = 1.0f;
            if (k < 2 * X) w = (float) k / (2.0f * X);
            else if (len - k < 2 * X) w = (float) (len - k) / (2.0f * X);
            if (start <= -X && k < 2 * X && o < X) w = 1.0f;       // inicio del archivo: sin fundido
            if (nt.s1 >= n && len - k <= 2 * X && o < nt.t1) w = 1.0f; // final del archivo: sin fundido
            out[(size_t) o] += r[k] * w * gain;
        }
    }
    cache.swap (used);
    if (progress != nullptr) progress->store (1.0f);
    return out;
}
} // namespace ned
