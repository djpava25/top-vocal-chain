#pragma once
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <vector>
#include <cmath>
#include "DSP/Biquad.h"
#include "SmoothDsp.h"
#include "MasterParams.h"

// Motor de TOP MASTER: todos los modulos de la cadena de mastering
namespace tmaster
{
using APVTS = juce::AudioProcessorValueTreeState;
using AP = std::atomic<float>*;
inline AP par (APVTS& s, const juce::String& id) { auto* p = s.getRawParameterValue (id); jassert (p != nullptr); return p; }
inline float dB2g (float d) { return std::pow (10.0f, d * 0.05f); }
inline float g2dB (float g) { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }
inline float coefMs (double sr, float ms) { return 1.0f - std::exp (-1.0f / (float) (std::max (0.01f, ms) * 0.001 * sr)); }

//==============================================================================
// Divisor de bandas Linkwitz-Riley (suma plana: las bandas graves pasan por los pasa-todo de los cruces superiores)
template <int Bands>
struct Splitter
{
    std::array<juce::dsp::LinkwitzRileyFilter<float>, Bands - 1> xo;
    std::array<std::array<juce::dsp::LinkwitzRileyFilter<float>, Bands - 1>, Bands - 1> ap;
    std::array<float, Bands - 1> f {};
    void prepare (double sr)
    {
        juce::dsp::ProcessSpec spec { sr, 512, 2 };
        for (int i = 0; i < Bands - 1; ++i)
        {
            xo[(size_t) i].prepare (spec);
            for (auto& a : ap[(size_t) i]) { a.prepare (spec); a.setType (juce::dsp::LinkwitzRileyFilterType::allpass); }
            f[(size_t) i] = -1.0f;
        }
    }
    void reset() { for (auto& x : xo) x.reset(); for (auto& r : ap) for (auto& a : r) a.reset(); }
    void setFreqs (const std::array<float, Bands - 1>& nf)
    {
        for (int i = 0; i < Bands - 1; ++i)
        {
            if (std::abs (nf[(size_t) i] - f[(size_t) i]) < 0.5f) continue;
            f[(size_t) i] = nf[(size_t) i];
            xo[(size_t) i].setCutoffFrequency (f[(size_t) i]);
            for (int j = 0; j < Bands - 1; ++j) ap[(size_t) j][(size_t) i].setCutoffFrequency (f[(size_t) i]);
        }
    }
    void split (int ch, float x, std::array<float, Bands>& out)
    {
        float rest = x;
        for (int i = 0; i < Bands - 1; ++i)
        {
            float lo, hi;
            xo[(size_t) i].processSample (ch, rest, lo, hi);
            for (int j = i + 1; j < Bands - 1; ++j) lo = ap[(size_t) i][(size_t) j].processSample (ch, lo);
            out[(size_t) i] = lo;
            rest = hi;
        }
        out[(size_t) Bands - 1] = rest;
    }
};

// Detector de transientes: diferencia entre envolvente rapida y lenta (dB)
struct TransientDetector
{
    float fast = 0, slow = 0, fa = 0, fr = 0, sa = 0, sr_ = 0;
    void prepare (double sr) { fa = coefMs (sr, 0.5f); fr = coefMs (sr, 25.0f); sa = coefMs (sr, 30.0f); sr_ = coefMs (sr, 150.0f); fast = slow = 0; }
    float push (float x)
    {
        const float a = std::abs (x);
        fast += (a - fast) * (a > fast ? fa : fr);
        slow += (a - slow) * (a > slow ? sa : sr_);
        if (fast < 1e-6f) return 0.0f;
        return g2dB (fast + 1e-9f) - g2dB (slow + 1e-9f);
    }
};

//==============================================================================
struct EqModule
{
    std::array<AP, 8> on {}, f {}, g {}, q {}, ch {};
    std::array<std::array<tvc::Biquad, 2>, 8> bq;
    std::array<std::array<float, 4>, 8> last {};
    double sr = 44100;
    void bind (APVTS& s)
    {
        for (int b = 0; b < 8; ++b)
        {
            const juce::String p = "eq_b" + juce::String (b + 1);
            on[(size_t) b] = par (s, p + "_on"); f[(size_t) b] = par (s, p + "_f"); g[(size_t) b] = par (s, p + "_g");
            q[(size_t) b] = par (s, p + "_q"); ch[(size_t) b] = par (s, p + "_ch");
        }
    }
    void prepare (double r) { sr = r; for (auto& l : last) l = { -1, -1, -1, -1 }; for (auto& a : bq) for (auto& x : a) x.reset(); }
    static void design (tvc::Biquad& b, int band, double sr, float f, float g, float q)
    {
        f = std::min (f, (float) (sr * 0.45));
        switch (band)
        {
            case 0: b.setHighPass (sr, f, q); break;
            case 1: b.setLowShelf (sr, f, g); break;
            case 6: b.setHighShelf (sr, f, g); break;
            case 7: b.setLowPass (sr, f, q); break;
            default: b.setPeak (sr, f, q, g); break;
        }
    }
    void process (float* L, float* R, int n)
    {
        std::array<bool, 8> act {};
        for (int b = 0; b < 8; ++b)
        {
            act[(size_t) b] = on[(size_t) b]->load() > 0.5f && (b == 0 || b == 7 || std::abs (g[(size_t) b]->load()) > 0.01f);
            if (! act[(size_t) b]) continue;
            const std::array<float, 4> cur { f[(size_t) b]->load(), g[(size_t) b]->load(), q[(size_t) b]->load(), 0 };
            if (cur != last[(size_t) b])
            {
                last[(size_t) b] = cur;
                for (auto& x : bq[(size_t) b]) design (x, b, sr, cur[0], cur[1], cur[2]);
            }
        }
        for (int i = 0; i < n; ++i)
        {
            float m = 0.5f * (L[i] + R[i]), s = 0.5f * (L[i] - R[i]);
            for (int b = 0; b < 8; ++b)
            {
                if (! act[(size_t) b]) continue;
                const int c = (int) ch[(size_t) b]->load();
                if (c != 2) m = bq[(size_t) b][0].process (m);
                if (c != 1) s = bq[(size_t) b][1].process (s);
            }
            L[i] = m + s; R[i] = m - s;
        }
    }
};

//==============================================================================
struct DynEqModule
{
    std::array<AP, 4> on {}, f {}, q {}, thr {}, range {};
    AP att = nullptr, rel = nullptr;
    std::array<tvc::Biquad, 4> det;
    std::array<std::array<tvc::Biquad, 2>, 4> eq;
    std::array<float, 4> env {}, gNow {}, gSet {};
    std::array<std::atomic<float>, 4> grLive {};
    double sr = 44100;
    int cnt = 0;
    void bind (APVTS& s)
    {
        for (int b = 0; b < 4; ++b)
        {
            const juce::String p = "deq_b" + juce::String (b + 1);
            on[(size_t) b] = par (s, p + "_on"); f[(size_t) b] = par (s, p + "_f"); q[(size_t) b] = par (s, p + "_q");
            thr[(size_t) b] = par (s, p + "_thr"); range[(size_t) b] = par (s, p + "_range");
        }
        att = par (s, "deq_att"); rel = par (s, "deq_rel");
    }
    void prepare (double r) { sr = r; env = {}; gNow = {}; gSet = { 99, 99, 99, 99 }; for (auto& e : eq) for (auto& x : e) { x.setIdentity(); x.reset(); } for (auto& d : det) d.reset(); }
    void process (float* L, float* R, int n)
    {
        const float a = coefMs (sr, att->load()), rl = coefMs (sr, rel->load());
        for (int b = 0; b < 4; ++b) det[(size_t) b].setBandPass (sr, std::min (f[(size_t) b]->load(), (float) (sr * 0.45)), q[(size_t) b]->load());
        for (int i = 0; i < n; ++i)
        {
            const float m = 0.5f * (L[i] + R[i]);
            if (++cnt >= 16)
            {
                cnt = 0;
                for (int b = 0; b < 4; ++b)
                {
                    if (on[(size_t) b]->load() < 0.5f) { gNow[(size_t) b] = 0; continue; }
                    const float lvl = g2dB (env[(size_t) b] * 1.414f + 1e-9f);
                    const float over = std::max (0.0f, lvl - thr[(size_t) b]->load());
                    const float rg = range[(size_t) b]->load();
                    const float target = std::copysign (std::min (std::abs (rg), over * 0.5f), rg);
                    gNow[(size_t) b] += (target - gNow[(size_t) b]) * (std::abs (target) > std::abs (gNow[(size_t) b]) ? std::min (1.0f, a * 16) : std::min (1.0f, rl * 16));
                    if (std::abs (gNow[(size_t) b] - gSet[(size_t) b]) > 0.02f)
                    {
                        gSet[(size_t) b] = gNow[(size_t) b];
                        for (auto& x : eq[(size_t) b]) x.setPeak (sr, std::min (f[(size_t) b]->load(), (float) (sr * 0.45)), q[(size_t) b]->load(), gSet[(size_t) b]);
                    }
                    grLive[(size_t) b].store (gNow[(size_t) b]);
                }
            }
            float l = L[i], r = R[i];
            for (int b = 0; b < 4; ++b)
            {
                const float d = std::abs (det[(size_t) b].process (m));
                env[(size_t) b] += (d - env[(size_t) b]) * (d > env[(size_t) b] ? a : rl);
                if (on[(size_t) b]->load() < 0.5f) continue;
                l = eq[(size_t) b][0].process (l); r = eq[(size_t) b][1].process (r);
            }
            L[i] = l; R[i] = r;
        }
    }
};

//==============================================================================
struct ShaperModule
{
    AP amount = nullptr, detail = nullptr, att = nullptr, rel = nullptr, low = nullptr, high = nullptr, mode = nullptr;
    tsm::FilterBankEngine eng;
    void bind (APVTS& s) { amount = par (s, "ss_amount"); detail = par (s, "ss_detail"); att = par (s, "ss_att"); rel = par (s, "ss_rel"); low = par (s, "ss_low"); high = par (s, "ss_high"); mode = par (s, "ss_mode"); }
    void prepare (double sr) { eng.prepare (sr); }
    void process (float* L, float* R, int n)
    {
        tsm::Settings st;
        st.mode = (int) mode->load(); st.depth = amount->load(); st.detail = detail->load();
        st.attackMs = att->load(); st.releaseMs = rel->load(); st.maxCut = 12.0f;
        st.lowCut = low->load(); st.highCut = high->load();
        float* p[2] { L, R };
        eng.process (p, nullptr, 2, n, true, st, nullptr);
    }
};

//==============================================================================
struct DynModule
{
    AP x1 = nullptr, x2 = nullptr, mix = nullptr;
    std::array<AP, 3> thr {}, ratio {}, att {}, rel {}, gain {};
    Splitter<3> split;
    std::array<float, 3> env {}, gr {};
    std::array<std::atomic<float>, 3> grLive {};
    double sr = 44100;
    void bind (APVTS& s)
    {
        x1 = par (s, "dyn_x1"); x2 = par (s, "dyn_x2"); mix = par (s, "dyn_mix");
        const char* bn[3] { "l", "m", "h" };
        for (int b = 0; b < 3; ++b)
        {
            const juce::String p = juce::String ("dyn_") + bn[b];
            thr[(size_t) b] = par (s, p + "_thr"); ratio[(size_t) b] = par (s, p + "_ratio"); att[(size_t) b] = par (s, p + "_att");
            rel[(size_t) b] = par (s, p + "_rel"); gain[(size_t) b] = par (s, p + "_gain");
        }
    }
    void prepare (double r) { sr = r; split.prepare (sr); env = {}; gr = {}; }
    void process (float* L, float* R, int n)
    {
        split.setFreqs ({ x1->load(), std::max (x1->load() * 1.5f, x2->load()) });
        std::array<float, 3> a {}, rl {}, th {}, ra {}, mk {};
        const float rmsC = coefMs (sr, 5.0f);
        for (int b = 0; b < 3; ++b)
        {
            a[(size_t) b] = coefMs (sr, att[(size_t) b]->load()); rl[(size_t) b] = coefMs (sr, rel[(size_t) b]->load());
            th[(size_t) b] = thr[(size_t) b]->load(); ra[(size_t) b] = ratio[(size_t) b]->load(); mk[(size_t) b] = dB2g (gain[(size_t) b]->load());
        }
        const float wet = mix->load() / 100.0f;
        std::array<float, 3> bl {}, br {};
        for (int i = 0; i < n; ++i)
        {
            split.split (0, L[i], bl); split.split (1, R[i], br);
            float ol = 0, orr = 0;
            for (int b = 0; b < 3; ++b)
            {
                const float p = std::max (bl[(size_t) b] * bl[(size_t) b], br[(size_t) b] * br[(size_t) b]);
                env[(size_t) b] += (p - env[(size_t) b]) * rmsC;
                const float lvl = 10.0f * std::log10 (env[(size_t) b] * 2.0f + 1e-12f);
                // rodilla suave de 6 dB
                const float over = lvl - th[(size_t) b];
                float red = 0.0f;
                if (over > 3.0f) red = over * (1.0f - 1.0f / ra[(size_t) b]);
                else if (over > -3.0f) red = (1.0f - 1.0f / ra[(size_t) b]) * (over + 3.0f) * (over + 3.0f) / 12.0f;
                float& g = gr[(size_t) b];
                g += (red - g) * (red > g ? a[(size_t) b] : rl[(size_t) b]);
                const float k = dB2g (-g) * mk[(size_t) b];
                const float yl = bl[(size_t) b] * k, yr = br[(size_t) b] * k;
                ol += bl[(size_t) b] + (yl - bl[(size_t) b]) * wet;
                orr += br[(size_t) b] + (yr - br[(size_t) b]) * wet;
            }
            L[i] = ol; R[i] = orr;
        }
        for (int b = 0; b < 3; ++b) grLive[(size_t) b].store (gr[(size_t) b]);
    }
};

//==============================================================================
struct BassModule
{
    AP freq = nullptr, gain = nullptr, punch = nullptr, sustain = nullptr, mono = nullptr, monoF = nullptr;
    Splitter<2> split, monoSplit;
    TransientDetector td;
    float gSm = 1.0f;
    double sr = 44100;
    void bind (APVTS& s) { freq = par (s, "bc_freq"); gain = par (s, "bc_gain"); punch = par (s, "bc_punch"); sustain = par (s, "bc_sustain"); mono = par (s, "bc_mono"); monoF = par (s, "bc_monofreq"); }
    void prepare (double r) { sr = r; split.prepare (sr); monoSplit.prepare (sr); td.prepare (sr); gSm = 1; }
    void process (float* L, float* R, int n)
    {
        split.setFreqs ({ freq->load() });
        monoSplit.setFreqs ({ monoF->load() });
        const bool mn = mono->load() > 0.5f;
        const float g0 = gain->load(), pu = punch->load() / 100.0f, su = sustain->load() / 100.0f;
        const float sc = coefMs (sr, 1.0f);
        std::array<float, 2> a {}, b {};
        for (int i = 0; i < n; ++i)
        {
            float l = L[i], r = R[i];
            if (mn)
            {
                monoSplit.split (0, l, a); monoSplit.split (1, r, b);
                const float lo = 0.5f * (a[0] + b[0]);
                l = lo + a[1]; r = lo + b[1];
            }
            split.split (0, l, a); split.split (1, r, b);
            const float d = td.push (0.5f * (a[0] + b[0]));
            const float tg = g0 + pu * std::clamp (d, 0.0f, 12.0f) * 0.6f + su * std::clamp (4.0f - d, 0.0f, 4.0f) * 1.2f;
            gSm += (dB2g (tg) - gSm) * sc;
            L[i] = a[0] * gSm + a[1];
            R[i] = b[0] * gSm + b[1];
        }
    }
};

//==============================================================================
struct ImpactModule
{
    std::array<AP, 3> x {}; std::array<AP, 4> amt {};
    Splitter<4> split;
    std::array<TransientDetector, 4> td;
    std::array<float, 4> gSm { 1, 1, 1, 1 };
    double sr = 44100;
    void bind (APVTS& s) { for (int i = 0; i < 3; ++i) x[(size_t) i] = par (s, "imp_x" + juce::String (i + 1)); for (int b = 0; b < 4; ++b) amt[(size_t) b] = par (s, "imp_a" + juce::String (b + 1)); }
    void prepare (double r) { sr = r; split.prepare (sr); for (auto& t : td) t.prepare (sr); gSm = { 1, 1, 1, 1 }; }
    void process (float* L, float* R, int n)
    {
        const float f1 = x[0]->load(), f2 = std::max (f1 * 1.5f, x[1]->load()), f3 = std::max (f2 * 1.5f, x[2]->load());
        split.setFreqs ({ f1, f2, f3 });
        std::array<float, 4> am {};
        for (int b = 0; b < 4; ++b) am[(size_t) b] = amt[(size_t) b]->load() / 100.0f;
        const float sc = coefMs (sr, 1.0f);
        std::array<float, 4> a {}, c {};
        for (int i = 0; i < n; ++i)
        {
            split.split (0, L[i], a); split.split (1, R[i], c);
            float ol = 0, orr = 0;
            for (int b = 0; b < 4; ++b)
            {
                const float d = td[(size_t) b].push (0.5f * (a[(size_t) b] + c[(size_t) b]));
                const float tg = am[(size_t) b] * (std::clamp (d, 0.0f, 12.0f) * 0.6f - 1.0f);   // mas golpe, misma media
                gSm[(size_t) b] += (dB2g (tg) - gSm[(size_t) b]) * sc;
                ol += a[(size_t) b] * gSm[(size_t) b]; orr += c[(size_t) b] * gSm[(size_t) b];
            }
            L[i] = ol; R[i] = orr;
        }
    }
};

//==============================================================================
// Deslimitador: expande hacia arriba los transientes que el limitador aplasto
struct UnlimitModule
{
    AP amount = nullptr, sens = nullptr;
    TransientDetector td;
    tvc::Biquad hp;
    float gSm = 1.0f, meanSq = 1.0f, comp = 1.0f;
    double sr = 44100;
    std::atomic<float> boostLive { 0.0f };
    void bind (APVTS& s) { amount = par (s, "unl_amount"); sens = par (s, "unl_sens"); }
    void prepare (double r) { sr = r; td.prepare (sr); hp.setHighPass (sr, 60.0, 0.707); hp.reset(); gSm = 1; meanSq = 1; comp = 1; }
    void process (float* L, float* R, int n)
    {
        const float am = amount->load() / 100.0f, thr = 6.0f * (1.0f - sens->load() / 100.0f);
        const float sc = coefMs (sr, 0.8f), lc = coefMs (sr, 2500.0f);
        float mx = 0;
        for (int i = 0; i < n; ++i)
        {
            const float d = td.push (hp.process (0.5f * (L[i] + R[i])));
            const float boost = am * std::clamp (d - thr, 0.0f, 10.0f) * 0.9f;
            mx = std::max (mx, boost);
            gSm += (dB2g (boost) - gSm) * sc;
            meanSq += (gSm * gSm - meanSq) * lc;
            comp = 1.0f / std::sqrt (std::max (1.0f, meanSq));
            L[i] *= gSm * comp; R[i] *= gSm * comp;
        }
        boostLive.store (mx);
    }
};

//==============================================================================
// Claridad: sube con suavidad las bandas que quedan por debajo de su entorno
struct ClarityModule
{
    static constexpr int bands = 30;
    AP amount = nullptr, maxB = nullptr, tilt = nullptr, speed = nullptr;
    std::unique_ptr<juce::dsp::FFT> fft;
    int N = 2048, hop = 512, w = 0, hc = 0, upd = 0;
    std::vector<float> ring, buf, win;
    std::array<float, bands> fc {}, eDb {}, boost {}, cur {}, setv {};
    std::array<std::array<tvc::Biquad, 2>, bands> eq;
    double sr = 44100;
    void bind (APVTS& s) { amount = par (s, "cla_amount"); maxB = par (s, "cla_max"); tilt = par (s, "cla_tilt"); speed = par (s, "cla_speed"); }
    void prepare (double r)
    {
        sr = r;
        const int order = sr > 60000 ? 12 : 11;
        N = 1 << order; hop = N / 4;
        fft = std::make_unique<juce::dsp::FFT> (order);
        ring.assign ((size_t) N, 0.0f); buf.assign ((size_t) N * 2, 0.0f); win.resize ((size_t) N);
        for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (6.2831853f * i / N);
        for (int b = 0; b < bands; ++b) { fc[(size_t) b] = 120.0f * std::pow (16000.0f / 120.0f, (float) b / (bands - 1)); eDb[(size_t) b] = -100; boost[(size_t) b] = cur[(size_t) b] = 0; setv[(size_t) b] = 99; for (auto& x : eq[(size_t) b]) { x.setIdentity(); x.reset(); } }
        w = hc = upd = 0;
    }
    void analyse()
    {
        for (int j = 0; j < N; ++j) buf[(size_t) j] = ring[(size_t) ((w + j) % N)] * win[(size_t) j];
        std::fill (buf.begin() + N, buf.end(), 0.0f);
        fft->performFrequencyOnlyForwardTransform (buf.data(), true);
        const float tau = speed->load() < 0.5f ? 0.6f : speed->load() < 1.5f ? 0.25f : 0.08f;
        const float k = std::exp (-(float) hop / (float) sr / tau);
        const float half = std::pow (2.0f, 0.5f * std::log2 (16000.0f / 120.0f) / (bands - 1));
        for (int b = 0; b < bands; ++b)
        {
            const int a = std::max (1, (int) (fc[(size_t) b] / half * N / sr)), e = std::max (a, std::min (N / 2 - 1, (int) (fc[(size_t) b] * half * N / sr)));
            double p = 0; for (int i = a; i <= e; ++i) p += (double) buf[(size_t) i] * buf[(size_t) i];
            const float d = (float) (10.0 * std::log10 (p / (e - a + 1) + 1e-14));
            eDb[(size_t) b] = eDb[(size_t) b] < -99 ? d : d + (eDb[(size_t) b] - d) * k;
        }
        const float am = amount->load() / 100.0f, mb = maxB->load(), tl = tilt->load();
        for (int b = 0; b < bands; ++b)
        {
            // objetivo: promedio de +-1 octava, inclinado
            double s = 0; int c = 0;
            for (int j = 0; j < bands; ++j) if (std::abs (std::log2 (fc[(size_t) j] / fc[(size_t) b])) <= 1.0f) { s += eDb[(size_t) j]; ++c; }
            const float target = (float) (s / c) + tl * std::log2 (fc[(size_t) b] / 1000.0f);
            const float deficit = target - eDb[(size_t) b];
            boost[(size_t) b] = eDb[(size_t) b] > -80.0f ? std::clamp (deficit * 0.7f * am, 0.0f, mb) : 0.0f;
        }
    }
    void process (float* L, float* R, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            ring[(size_t) w] = 0.5f * (L[i] + R[i]);
            w = (w + 1) % N;
            if (++hc >= hop) { hc = 0; analyse(); }
            if (++upd >= 64)
            {
                upd = 0;
                for (int b = 0; b < bands; ++b)
                {
                    cur[(size_t) b] += (boost[(size_t) b] - cur[(size_t) b]) * 0.1f;
                    if (std::abs (cur[(size_t) b] - setv[(size_t) b]) > 0.03f && fc[(size_t) b] < sr * 0.45)
                    {
                        setv[(size_t) b] = cur[(size_t) b];
                        for (auto& x : eq[(size_t) b]) { if (cur[(size_t) b] < 0.02f) x.setIdentity(); else x.setPeak (sr, fc[(size_t) b], 3.0, cur[(size_t) b] * 0.6f); }
                    }
                }
            }
            float l = L[i], r = R[i];
            for (int b = 0; b < bands; ++b) { l = eq[(size_t) b][0].process (l); r = eq[(size_t) b][1].process (r); }
            L[i] = l; R[i] = r;
        }
    }
};

//==============================================================================
inline float shape (int mode, float x)
{
    switch (mode)
    {
        case 0:  return std::tanh (x + 0.25f * x * x);                     // calido (pares)
        case 1:  { const float c = std::clamp (x, -1.5f, 1.5f); return c - c * c * c / 6.75f; }   // cinta (impares suaves)
        case 2:  return x > 0 ? std::tanh (x) : std::tanh (1.6f * x) / 1.6f; // valvula (asimetrica)
        default: return std::clamp (x * 1.3f, -0.8f, 0.8f);                // retro
    }
}

struct ExciterModule
{
    AP x1 = nullptr, x2 = nullptr, mix = nullptr;
    std::array<AP, 3> amt {}, mode {};
    Splitter<3> split;
    std::array<std::array<tvc::Biquad, 2>, 3> dc;
    double sr = 44100;
    void bind (APVTS& s) { x1 = par (s, "exc_x1"); x2 = par (s, "exc_x2"); mix = par (s, "exc_mix"); for (int b = 0; b < 3; ++b) { amt[(size_t) b] = par (s, "exc_a" + juce::String (b + 1)); mode[(size_t) b] = par (s, "exc_m" + juce::String (b + 1)); } }
    void prepare (double r) { sr = r; split.prepare (sr); for (auto& a : dc) for (auto& x : a) { x.setHighPass1 (sr, 12.0); x.reset(); } }
    void process (float* L, float* R, int n)
    {
        split.setFreqs ({ x1->load(), std::max (x1->load() * 1.5f, x2->load()) });
        std::array<float, 3> k {}, am {}; std::array<int, 3> md {};
        for (int b = 0; b < 3; ++b) { am[(size_t) b] = amt[(size_t) b]->load() / 100.0f; k[(size_t) b] = 1.0f + am[(size_t) b] * 5.0f; md[(size_t) b] = (int) mode[(size_t) b]->load(); }
        const float wet = mix->load() / 100.0f;
        std::array<float, 3> a {}, c {};
        for (int i = 0; i < n; ++i)
        {
            split.split (0, L[i], a); split.split (1, R[i], c);
            float ol = 0, orr = 0;
            for (int b = 0; b < 3; ++b)
            {
                float yl = a[(size_t) b], yr = c[(size_t) b];
                if (am[(size_t) b] > 0.0f)
                {
                    const float kk = k[(size_t) b];
                    const float hl = shape (md[(size_t) b], yl * kk) / kk - yl, hr = shape (md[(size_t) b], yr * kk) / kk - yr;
                    yl += dc[(size_t) b][0].process (hl) * am[(size_t) b] * 1.5f * wet;
                    yr += dc[(size_t) b][1].process (hr) * am[(size_t) b] * 1.5f * wet;
                }
                ol += yl; orr += yr;
            }
            L[i] = ol; R[i] = orr;
        }
    }
};

//==============================================================================
struct TapeModule
{
    AP drive = nullptr, speed = nullptr, bias = nullptr, mix = nullptr;
    std::array<tvc::Biquad, 2> bump, roll, dc;
    int lastSpeed = -1;
    double sr = 44100;
    void bind (APVTS& s) { drive = par (s, "tape_drive"); speed = par (s, "tape_speed"); bias = par (s, "tape_bias"); mix = par (s, "tape_mix"); }
    void prepare (double r) { sr = r; lastSpeed = -1; for (auto& x : dc) { x.setHighPass1 (sr, 10.0); x.reset(); } }
    void process (float* L, float* R, int n)
    {
        const int sp = (int) speed->load();
        if (sp != lastSpeed)
        {
            lastSpeed = sp;
            const float bf[3] { 55, 80, 115 }, bg[3] { 2.5f, 2.0f, 1.4f }, lf[3] { 11000, 16000, 20000 };
            for (auto& x : bump) { x.setPeak (sr, bf[sp], 1.2, bg[sp]); x.reset(); }
            for (auto& x : roll) { x.setLowPass (sr, std::min ((double) lf[sp], sr * 0.45), 0.6); x.reset(); }
        }
        const float g = dB2g (drive->load() * 0.15f), bi = bias->load() / 100.0f * 0.15f, wet = mix->load() / 100.0f;
        const float norm = 1.0f / std::tanh (g * 0.5f) * 0.5f;
        float* ch[2] { L, R };
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                const float x = ch[c][i];
                float y = std::tanh ((x + bi * x * x) * g * 0.5f) * norm;
                y = dc[(size_t) c].process (y);
                y = roll[(size_t) c].process (bump[(size_t) c].process (y));
                ch[c][i] = x + (y - x) * wet;
            }
    }
};

//==============================================================================
struct ImagerModule
{
    std::array<AP, 3> x {}; std::array<AP, 4> wd {}; AP stz = nullptr;
    Splitter<4> split;
    tvc::Biquad stzHp;
    std::vector<float> dline; int dp = 0;
    double sr = 44100;
    float cLR = 0, cLL = 1e-9f, cRR = 1e-9f;
    std::atomic<float> correlation { 1.0f };
    void bind (APVTS& s) { for (int i = 0; i < 3; ++i) x[(size_t) i] = par (s, "img_x" + juce::String (i + 1)); for (int b = 0; b < 4; ++b) wd[(size_t) b] = par (s, "img_w" + juce::String (b + 1)); stz = par (s, "img_stereoize"); }
    void prepare (double r) { sr = r; split.prepare (sr); stzHp.setHighPass (sr, 300.0, 0.707); stzHp.reset(); dline.assign ((size_t) (sr * 0.02) + 1, 0.0f); dp = 0; }
    void process (float* L, float* R, int n)
    {
        const float f1 = x[0]->load(), f2 = std::max (f1 * 1.5f, x[1]->load()), f3 = std::max (f2 * 1.5f, x[2]->load());
        split.setFreqs ({ f1, f2, f3 });
        std::array<float, 4> k {};
        for (int b = 0; b < 4; ++b) k[(size_t) b] = 1.0f + wd[(size_t) b]->load() / 100.0f;
        const float st = stz->load() / 100.0f * 0.35f;
        const int d1 = (int) (sr * 0.0045), d2 = (int) (sr * 0.0093), D = (int) dline.size();
        std::array<float, 4> m {}, s {};
        const float cc = coefMs (sr, 300.0f);
        for (int i = 0; i < n; ++i)
        {
            split.split (0, 0.5f * (L[i] + R[i]), m);
            split.split (1, 0.5f * (L[i] - R[i]), s);
            float M = 0, S = 0;
            for (int b = 0; b < 4; ++b) { M += m[(size_t) b]; S += s[(size_t) b] * k[(size_t) b]; }
            // estereizar: lados nuevos a partir del centro (no cambia la suma en mono)
            const float hm = stzHp.process (M);
            dline[(size_t) dp] = hm;
            S += st * (dline[(size_t) ((dp - d1 + D) % D)] - dline[(size_t) ((dp - d2 + D) % D)]);
            dp = (dp + 1) % D;
            const float l = M + S, r = M - S;
            cLR += (l * r - cLR) * cc; cLL += (l * l - cLL) * cc; cRR += (r * r - cRR) * cc;
            L[i] = l; R[i] = r;
        }
        correlation.store (cLR / std::sqrt (cLL * cRR + 1e-12f));
    }
};

//==============================================================================
// Maximizador: limitador con anticipacion (2 ms), deteccion true-peak y modos de caracter.
// Latencia fija = L - 1 muestras (tambien cuando esta apagado, para no mover la latencia).
// minimo de una ventana deslizante (cola monotona, sin memoria dinamica en el audio)
struct MinQueue
{
    std::vector<long long> ti; std::vector<float> v; int head = 0, tail = 0, cap = 2;
    void init (int L) { cap = L + 2; ti.assign ((size_t) cap, 0); v.assign ((size_t) cap, 1.0f); head = tail = 0; }
    void push (long long t, float x, int L)
    {
        while (head != tail && v[(size_t) ((tail - 1 + cap) % cap)] >= x) tail = (tail - 1 + cap) % cap;
        ti[(size_t) tail] = t; v[(size_t) tail] = x; tail = (tail + 1) % cap;
        while (ti[(size_t) head] <= t - L) head = (head + 1) % cap;
    }
    float min() const { return v[(size_t) head]; }
};

struct Maximizer
{
    AP gain = nullptr, ceil = nullptr, mode = nullptr, ch = nullptr, tp = nullptr, link = nullptr;
    int L = 88;
    std::array<std::vector<float>, 2> delay, boxHist;
    std::array<MinQueue, 2> mq;
    std::array<float, 2> env { 1, 1 }, boxSum {};
    std::array<std::array<float, 4>, 2> hist {};
    long long t = 0;
    double sr = 44100;
    std::atomic<float> grLive { 0.0f };
    void bind (APVTS& s) { gain = par (s, "max_gain"); ceil = par (s, "max_ceil"); mode = par (s, "max_mode"); ch = par (s, "max_char"); tp = par (s, "max_tp"); link = par (s, "max_link"); }
    int latency() const { return L - 1; }
    void prepare (double r)
    {
        sr = r; L = std::max (8, (int) std::round (sr * 0.002));
        for (int c = 0; c < 2; ++c)
        {
            delay[(size_t) c].assign ((size_t) L, 0.0f); boxHist[(size_t) c].assign ((size_t) L, 1.0f);
            mq[(size_t) c].init (L);
            env[(size_t) c] = 1.0f; boxSum[(size_t) c] = (float) L; hist[(size_t) c] = {};
        }
        t = 0;
    }
    static float softClip (float x, float th)
    {
        const float a = std::abs (x);
        if (a < th * 0.7f) return x;
        return std::copysign (th * (0.7f + 0.3f * std::tanh ((a / th - 0.7f) / 0.3f)), x);
    }
    // pico entre muestras (Catmull-Rom en 3 puntos intermedios)
    static float interPeak (const std::array<float, 4>& h)
    {
        float m = std::max (std::abs (h[1]), std::abs (h[2]));
        for (float u : { 0.25f, 0.5f, 0.75f })
        {
            const float v = 0.5f * ((2 * h[1]) + (-h[0] + h[2]) * u + (2 * h[0] - 5 * h[1] + 4 * h[2] - h[3]) * u * u + (-h[0] + 3 * h[1] - 3 * h[2] + h[3]) * u * u * u);
            m = std::max (m, std::abs (v));
        }
        return m;
    }
    void process (float* Lp, float* Rp, int n, bool active)
    {
        const float g = dB2g (gain->load()), c = dB2g (ceil->load());
        const int md = (int) mode->load();
        const float chv = ch->load() / 100.0f;
        const float relMs = md == 0 ? 30.0f + chv * 770.0f : md == 1 ? 15.0f + chv * 285.0f : md == 2 ? 5.0f + chv * 115.0f : 10.0f + chv * 140.0f;
        const float rc = coefMs (sr, relMs);
        const bool truePk = tp->load() > 0.5f;
        const float lk = link->load() / 100.0f;
        const float clipTh = md == 1 ? c * 1.6f : md == 2 ? c * 1.12f : c;
        float* chans[2] { Lp, Rp };
        float maxGr = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            std::array<float, 2> x {}, req {};
            for (int k = 0; k < 2; ++k)
            {
                float v = chans[k][i];
                if (active)
                {
                    v *= g;
                    if (md >= 1) v = softClip (v, clipTh);
                }
                x[(size_t) k] = v;
                auto& h = hist[(size_t) k];
                h[0] = h[1]; h[1] = h[2]; h[2] = h[3]; h[3] = v;
                const float pk = truePk ? interPeak (h) : std::abs (v);
                req[(size_t) k] = active && pk > c ? c / pk : 1.0f;
            }
            const float rmin = std::min (req[0], req[1]);
            const int slot = (int) (t % L);
            for (int k = 0; k < 2; ++k)
            {
                const float rq = req[(size_t) k] + (rmin - req[(size_t) k]) * lk;
                mq[(size_t) k].push (t, rq, L);
                const float hold = mq[(size_t) k].min();
                float& e = env[(size_t) k];
                e = hold < e ? hold : e + (hold - e) * rc;
                // promedio de L muestras: rampa suave de ataque
                auto& bh = boxHist[(size_t) k];
                boxSum[(size_t) k] += e - bh[(size_t) slot];
                bh[(size_t) slot] = e;
                const float gk = std::min (1.0f, boxSum[(size_t) k] / (float) L);
                // salida retrasada L-1
                auto& dl = delay[(size_t) k];
                const float out = dl[(size_t) ((slot + 1) % L)];
                dl[(size_t) slot] = x[(size_t) k];
                float y = out * gk;
                if (active) y = std::clamp (y, -c, c);
                chans[k][i] = y;
                maxGr = std::max (maxGr, -g2dB (gk));
            }
            ++t;
        }
        grLive.store (maxGr);
    }
};

//==============================================================================
struct DitherModule
{
    AP bits = nullptr, shp = nullptr;
    std::array<float, 2> e1 {}, e2 {};
    juce::Random rng { 1234 };
    void bind (APVTS& s) { bits = par (s, "dit_bits"); shp = par (s, "dit_shape"); }
    void process (float* L, float* R, int n)
    {
        const float q = bits->load() < 0.5f ? 1.0f / 32768.0f : 1.0f / 8388608.0f;
        const int sh = (int) shp->load();
        float* ch[2] { L, R };
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                float x = ch[c][i];
                if (sh == 1) x -= 0.5f * e1[(size_t) c];
                else if (sh == 2) x -= 1.5f * e1[(size_t) c] - 0.6f * e2[(size_t) c];
                const float d = (rng.nextFloat() - rng.nextFloat()) * q;
                const float y = std::round ((x + d) / q) * q;
                e2[(size_t) c] = e1[(size_t) c];
                e1[(size_t) c] = y - x;
                ch[c][i] = y;
            }
    }
};

//==============================================================================
// Medidor de loudness ITU-R BS.1770 (momentaneo, corto, integrado) y true peak
struct LoudnessMeter
{
    std::array<tvc::Biquad, 2> shelf, hp;
    int blockLen = 4410, pos = 0;
    double acc = 0;
    std::array<double, 30> blocks {};
    int bIdx = 0, bCount = 0;
    std::array<int, 800> histN {};
    std::array<double, 800> histE {};
    std::array<float, 2> tpHist {};
    std::array<std::array<float, 4>, 2> th {};
    std::atomic<float> momentary { -100.0f }, shortTerm { -100.0f }, integrated { -100.0f }, truePeak { -100.0f };
    float tpMax = 0.0f;
    std::atomic<bool> resetFlag { false };
    void prepare (double sr)
    {
        for (int c = 0; c < 2; ++c) { shelf[(size_t) c].setHighShelf (sr, 1681.97, 4.0); hp[(size_t) c].setHighPass (sr, 38.13, 0.5); shelf[(size_t) c].reset(); hp[(size_t) c].reset(); }
        blockLen = (int) (sr * 0.1);
        clear();
    }
    void clear()
    {
        pos = 0; acc = 0; blocks = {}; bIdx = bCount = 0; histN = {}; histE = {}; tpMax = 0;
        momentary = shortTerm = integrated = truePeak = -100.0f;
        th = {};
    }
    static float lufs (double ms) { return ms > 1e-12 ? (float) (-0.691 + 10.0 * std::log10 (ms)) : -100.0f; }
    void process (const float* L, const float* R, int n)
    {
        if (resetFlag.exchange (false)) clear();
        const float* ch[2] { L, R };
        for (int i = 0; i < n; ++i)
        {
            double s = 0;
            for (int c = 0; c < 2; ++c)
            {
                const float y = hp[(size_t) c].process (shelf[(size_t) c].process (ch[c][i]));
                s += (double) y * y;
                auto& h = th[(size_t) c];
                h[0] = h[1]; h[1] = h[2]; h[2] = h[3]; h[3] = ch[c][i];
                tpMax = std::max (tpMax, Maximizer::interPeak (h));
            }
            acc += s;
            if (++pos >= blockLen)
            {
                blocks[(size_t) bIdx] = acc / pos;
                bIdx = (bIdx + 1) % 30; bCount = std::min (bCount + 1, 30);
                pos = 0; acc = 0;
                double m4 = 0, m30 = 0;
                const int k4 = std::min (bCount, 4);
                for (int k = 0; k < k4; ++k) m4 += blocks[(size_t) ((bIdx - 1 - k + 30) % 30)];
                for (int k = 0; k < bCount; ++k) m30 += blocks[(size_t) k];
                const float mm = lufs (m4 / std::max (1, k4));
                momentary.store (mm);
                shortTerm.store (lufs (m30 / std::max (1, bCount)));
                if (mm > -70.0f && k4 == 4)
                {
                    const int bin = std::clamp ((int) ((mm + 70.0f) * 10.0f), 0, 799);
                    histN[(size_t) bin]++; histE[(size_t) bin] += m4 / 4;
                }
                // integrado con compuerta relativa de -10 LU
                double eAll = 0; long nAll = 0;
                for (int b = 0; b < 800; ++b) { eAll += histE[(size_t) b]; nAll += histN[(size_t) b]; }
                if (nAll > 0)
                {
                    const float rel = lufs (eAll / nAll) - 10.0f;
                    double e = 0; long cnt = 0;
                    for (int b = std::max (0, (int) ((rel + 70.0f) * 10.0f)); b < 800; ++b) { e += histE[(size_t) b]; cnt += histN[(size_t) b]; }
                    if (cnt > 0) integrated.store (lufs (e / cnt));
                }
                truePeak.store (g2dB (tpMax));
            }
        }
    }
};

// Anillo sin bloqueos para el analizador de espectro
struct Tap
{
    static constexpr int size = 8192;
    std::array<float, size> buf {};
    std::atomic<int> w { 0 };
    void push (const float* L, const float* R, int n) { int p = w.load(); for (int i = 0; i < n; ++i) { buf[(size_t) p] = 0.5f * (L[i] + R[i]); p = (p + 1) % size; } w.store (p); }
    void read (float* out, int n) const { const int p = w.load(); for (int i = 0; i < n; ++i) out[i] = buf[(size_t) ((p - n + i + size * 2) % size)]; }
};

//==============================================================================
// La cadena completa
struct Chain
{
    EqModule eq; DynEqModule deq; ShaperModule ss; DynModule dyn; BassModule bass; ImpactModule imp;
    UnlimitModule unl; ClarityModule cla; ExciterModule exc; TapeModule tape; ImagerModule img; Maximizer max; DitherModule dit;
    std::array<AP, NumModules> on {};
    AP inGain = nullptr;
    std::array<std::atomic<int>, numReorderable> order {};
    LoudnessMeter inMeter, outMeter;
    Tap inTap, outTap;
    double sr = 44100;

    explicit Chain (APVTS& s)
    {
        eq.bind (s); deq.bind (s); ss.bind (s); dyn.bind (s); bass.bind (s); imp.bind (s); unl.bind (s); cla.bind (s);
        exc.bind (s); tape.bind (s); img.bind (s); max.bind (s); dit.bind (s);
        for (int m = 0; m < NumModules; ++m) on[(size_t) m] = par (s, juce::String (modulePrefix (m)) + "_on");
        inGain = par (s, "in_gain");
        const int def[numReorderable] { UNLIMIT, EQ, DEQ, SHAPER, DYN, BASS, IMPACT, CLARITY, EXCITER, TAPE, IMAGER };
        for (int i = 0; i < numReorderable; ++i) order[(size_t) i].store (def[i]);
    }
    void prepare (double r)
    {
        sr = r;
        eq.prepare (sr); deq.prepare (sr); ss.prepare (sr); dyn.prepare (sr); bass.prepare (sr); imp.prepare (sr);
        unl.prepare (sr); cla.prepare (sr); exc.prepare (sr); tape.prepare (sr); img.prepare (sr); max.prepare (sr);
        inMeter.prepare (sr); outMeter.prepare (sr);
    }
    int latency() const { return max.latency(); }
    bool isOn (int m) const { return on[(size_t) m]->load() > 0.5f; }
    void processModule (int m, float* L, float* R, int n)
    {
        switch (m)
        {
            case EQ: eq.process (L, R, n); break;
            case DEQ: deq.process (L, R, n); break;
            case SHAPER: ss.process (L, R, n); break;
            case DYN: dyn.process (L, R, n); break;
            case BASS: bass.process (L, R, n); break;
            case IMPACT: imp.process (L, R, n); break;
            case UNLIMIT: unl.process (L, R, n); break;
            case CLARITY: cla.process (L, R, n); break;
            case EXCITER: exc.process (L, R, n); break;
            case TAPE: tape.process (L, R, n); break;
            case IMAGER: img.process (L, R, n); break;
            default: break;
        }
    }
    // procesa en sitio (estereo)
    void process (float* L, float* R, int n, bool metering = true)
    {
        const float ig = dB2g (inGain->load());
        if (ig != 1.0f) for (int i = 0; i < n; ++i) { L[i] *= ig; R[i] *= ig; }
        if (metering) { inMeter.process (L, R, n); inTap.push (L, R, n); }
        for (int k = 0; k < numReorderable; ++k)
        {
            const int m = order[(size_t) k].load();
            if (isOn (m)) processModule (m, L, R, n);
        }
        max.process (L, R, n, isOn (MAXIMIZER));
        if (isOn (DITHER)) dit.process (L, R, n);
        if (metering) { outMeter.process (L, R, n); outTap.push (L, R, n); }
    }
};
} // namespace tmaster
