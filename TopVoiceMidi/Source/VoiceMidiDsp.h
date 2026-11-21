#pragma once
#include <array>
#include <cmath>
#include <vector>
#include <algorithm>
#include "DSP/Biquad.h"

// Piezas DSP de TOP VOICE MIDI: detector de tono de baja latencia, extractor de rasgos
// para el beatbox (bombo / caja / hi-hat), clasificador entrenable y sintes de monitoreo.
namespace vmidi
{
constexpr float kTwoPi = 6.283185307f;

//==============================================================================
// YIN de baja latencia: decima a ~11 kHz, ventana de ~24 ms y una estimacion cada ~3 ms
class FastPitch
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        decim = std::max (1, (int) std::round (sr / 11025.0));
        dsr = sr / decim;
        aa1.setLowPass (sr, dsr * 0.42, 0.707);
        aa2.setLowPass (sr, dsr * 0.42, 0.707);
        tauMax = (int) std::ceil (dsr / 75.0);       // nota mas grave ~75 Hz
        tauMin = std::max (2, (int) std::floor (dsr / 1100.0));
        win = (int) std::round (dsr * 0.0115);        // ventana de integracion
        size = win + tauMax + 2;
        hop = std::max (8, (int) std::round (dsr * 0.0029));
        ring.assign ((size_t) size, 0.0f);
        frame.assign ((size_t) size, 0.0f);
        d.assign ((size_t) tauMax + 2, 0.0f);
        reset();
    }
    void reset()
    {
        std::fill (ring.begin(), ring.end(), 0.0f);
        wp = dc = hc = 0; freq = 0; aperiodic = 1; aa1.reset(); aa2.reset();
    }
    // true cuando hay una nueva estimacion
    bool push (float x)
    {
        const float y = aa2.process (aa1.process (x));
        if (++dc < decim) return false;
        dc = 0;
        ring[(size_t) wp] = y; wp = (wp + 1) % size;
        if (++hc < hop) return false;
        hc = 0;
        analyse();
        return true;
    }
    float getFrequency() const { return freq; }
    float getAperiodicity() const { return aperiodic; }  // 0 = tono perfecto, 1 = ruido
    // latencia aproximada del detector en muestras de la frecuencia original
    int latencySamples() const { return (int) ((double) (win + tauMax) * 0.5 * decim + hop * decim); }

private:
    void analyse()
    {
        for (int i = 0; i < size; ++i) frame[(size_t) i] = ring[(size_t) ((wp + i) % size)];
        const float* f = frame.data();
        d[0] = 1.0f;
        float run = 0.0f;
        int best = -1;
        float bestVal = 1.0f;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            float s = 0.0f;
            for (int j = 0; j < win; ++j) { const float q = f[j] - f[j + tau]; s += q * q; }
            run += s;
            d[(size_t) tau] = run > 0.0f ? s * (float) tau / run : 1.0f;
        }
        // primer minimo bajo el umbral
        for (int tau = tauMin; tau < tauMax; ++tau)
        {
            if (d[(size_t) tau] < 0.15f)
            {
                while (tau + 1 < tauMax && d[(size_t) tau + 1] < d[(size_t) tau]) ++tau;
                best = tau; bestVal = d[(size_t) tau]; break;
            }
        }
        if (best < 0)
        {
            for (int tau = tauMin; tau < tauMax; ++tau) if (d[(size_t) tau] < bestVal) { bestVal = d[(size_t) tau]; best = tau; }
        }
        if (best <= tauMin || best >= tauMax) { aperiodic = 1.0f; return; }
        // interpolacion parabolica
        const float a = d[(size_t) best - 1], b = d[(size_t) best], c = d[(size_t) best + 1];
        const float den = a + c - 2.0f * b;
        const float shift = std::abs (den) > 1e-9f ? 0.5f * (a - c) / den : 0.0f;
        freq = (float) (dsr / ((float) best + std::clamp (shift, -0.5f, 0.5f)));
        aperiodic = std::clamp (bestVal, 0.0f, 1.0f);
    }

    double sr = 44100, dsr = 11025;
    int decim = 4, tauMin = 10, tauMax = 147, win = 128, size = 280, hop = 32;
    int wp = 0, dc = 0, hc = 0;
    std::vector<float> ring, frame, d;
    tvc::Biquad aa1, aa2;
    float freq = 0, aperiodic = 1;
};

//==============================================================================
// Rasgos de un golpe de beatbox (energia relativa por bandas + cruces por cero)
struct HitFeatures
{
    static constexpr int num = 6;
    std::array<float, num> v {};
};

class HitAnalyser
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        lo.setLowPass (sr, 160.0, 0.707);
        m1.setBandPass (sr, 450.0, 0.9);
        m2.setBandPass (sr, 1800.0, 0.9);
        hi.setHighPass (sr, 6000.0, 0.707);
        length = (int) std::round (sr * 0.014);    // 14 ms para decidir
        reset();
    }
    void reset() { lo.reset(); m1.reset(); m2.reset(); hi.reset(); count = -1; prev = 0; }
    void start() { count = 0; eLo = eM1 = eM2 = eHi = eAll = 0; zc = 0; peak = 0; }
    bool active() const { return count >= 0; }
    // procesa siempre (para que los filtros esten calientes); devuelve true al completar la ventana
    bool push (float x)
    {
        const float a = lo.process (x), b = m1.process (x), c = m2.process (x), d = hi.process (x);
        if (count < 0) { prev = x; return false; }
        eLo += a * a; eM1 += b * b; eM2 += c * c; eHi += d * d; eAll += x * x;
        if ((x >= 0) != (prev >= 0)) ++zc;
        prev = x;
        peak = std::max (peak, std::abs (x));
        if (++count >= length) { count = -1; return true; }
        return false;
    }
    HitFeatures features() const
    {
        HitFeatures f;
        const float t = eAll + 1e-9f;
        auto db = [t] (float e) { return 10.0f * std::log10 (e / t + 1e-6f) / 30.0f; };
        f.v[0] = db (eLo); f.v[1] = db (eM1); f.v[2] = db (eM2); f.v[3] = db (eHi);
        f.v[4] = (float) zc / (float) std::max (1, length) * 4.0f;
        f.v[5] = (10.0f * std::log10 ((eHi + eM2 + 1e-9f) / (eLo + eM1 + 1e-9f))) / 30.0f;
        return f;
    }
    float getPeak() const { return peak; }
private:
    double sr = 44100;
    tvc::Biquad lo, m1, m2, hi;
    int length = 600, count = -1, zc = 0;
    float eLo = 0, eM1 = 0, eM2 = 0, eHi = 0, eAll = 0, prev = 0, peak = 0;
};

// Clasificador de vecino mas cercano: cada pad guarda la media de sus ejemplos
struct HitClassifier
{
    static constexpr int pads = 4;
    std::array<HitFeatures, pads> mean {};
    std::array<int, pads> examples {};

    void add (int pad, const HitFeatures& f)
    {
        auto& m = mean[(size_t) pad];
        const int n = ++examples[(size_t) pad];
        for (int i = 0; i < HitFeatures::num; ++i) m.v[(size_t) i] += (f.v[(size_t) i] - m.v[(size_t) i]) / (float) n;
    }
    void clear (int pad) { examples[(size_t) pad] = 0; mean[(size_t) pad] = {}; }

    // pads entrenados: vecino mas cercano. Sin entrenar: reglas por defecto (graves = bombo, agudos = hi-hat)
    int classify (const HitFeatures& f) const
    {
        int best = -1; float bestD = 1e9f;
        for (int p = 0; p < pads; ++p)
        {
            if (examples[(size_t) p] == 0) continue;
            float dd = 0;
            for (int i = 0; i < HitFeatures::num; ++i) { const float q = f.v[(size_t) i] - mean[(size_t) p].v[(size_t) i]; dd += q * q; }
            if (dd < bestD) { bestD = dd; best = p; }
        }
        if (best >= 0 && trainedCount() >= 2) return best;
        // reglas por defecto
        const float tilt = f.v[5];
        if (f.v[0] > -0.12f && tilt < -0.25f) return 0;       // bombo: domina la banda grave
        if (tilt > 0.10f || f.v[4] > 0.9f) return 2;           // hi-hat: agudo y ruidoso
        return best >= 0 ? best : 1;                           // caja
    }
    int trainedCount() const { int n = 0; for (int e : examples) n += e > 0 ? 1 : 0; return n; }
};

//==============================================================================
// Sinte de monitoreo: 8 voces diente de sierra con filtro y envolvente, mas una bateria simple
class MonitorSynth
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto& v : voices) { v.active = false; v.filt.reset(); }
        for (auto& d : drums) d.active = false;
    }
    void noteOn (int note, float vel)
    {
        Voice* v = nullptr;
        for (auto& x : voices) if (x.active && x.note == note) v = &x;
        if (v == nullptr) for (auto& x : voices) if (! x.active) { v = &x; break; }
        if (v == nullptr) v = &voices[0];
        v->active = true; v->released = false; v->note = note; v->vel = vel; v->env = std::max (v->env, 0.0f);
        v->hz = 440.0f * std::pow (2.0f, (float) (note - 69) / 12.0f);
        v->filt.setLowPass (sr, std::min (sr * 0.45, 600.0 + 5200.0 * vel), 0.8);
    }
    void noteOff (int note) { for (auto& v : voices) if (v.active && v.note == note) v.released = true; }
    void allOff() { for (auto& v : voices) v.released = true; }
    void setBend (float semis) { bend = std::pow (2.0f, semis / 12.0f); }
    void setWave (int w) { wave = w; }
    void drum (int pad, float vel)
    {
        for (auto& d : drums) if (! d.active)
        {
            d.active = true; d.pad = pad; d.t = 0; d.vel = vel; d.ph = 0;
            d.f.setHighPass (sr, pad == 2 ? 7000.0 : pad == 3 ? 3500.0 : 1200.0, 0.7);
            return;
        }
    }
    float render()
    {
        float out = 0.0f;
        for (auto& v : voices)
        {
            if (! v.active) continue;
            v.env += v.released ? -v.env * (float) (1.0 / (0.12 * sr)) * 4.0f : (1.0f - v.env) * (float) (1.0 / (0.004 * sr));
            if (v.released && v.env < 1e-4f) { v.active = false; v.env = 0; continue; }
            v.ph += v.hz * bend / (float) sr; if (v.ph >= 1.0f) v.ph -= 1.0f;
            float s;
            if (wave == 0)      s = std::sin (kTwoPi * v.ph);
            else if (wave == 1) s = 2.0f * v.ph - 1.0f;
            else                s = v.ph < 0.5f ? 0.7f : -0.7f;
            out += v.filt.process (s) * v.env * (0.25f + 0.55f * v.vel) * 0.3f;
        }
        for (auto& d : drums)
        {
            if (! d.active) continue;
            const float t = (float) (d.t++ / sr);
            float s = 0.0f;
            noise = noise * 1664525u + 1013904223u;
            const float n = (float) (noise >> 8) / 8388608.0f - 1.0f;
            if (d.pad == 0)      { d.ph += (45.0f + 110.0f * std::exp (-t * 30.0f)) / (float) sr; s = std::sin (kTwoPi * d.ph) * std::exp (-t * 9.0f); if (t > 0.45f) d.active = false; }
            else if (d.pad == 1) { d.ph += 190.0f / (float) sr; s = (0.45f * std::sin (kTwoPi * d.ph) * std::exp (-t * 25.0f) + 0.7f * d.f.process (n) * std::exp (-t * 16.0f)); if (t > 0.3f) d.active = false; }
            else                 { s = 0.6f * d.f.process (n) * std::exp (-t * (d.pad == 2 ? 45.0f : 12.0f)); if (t > 0.4f) d.active = false; }
            out += s * d.vel * 0.55f;
        }
        return out;
    }
private:
    struct Voice { bool active = false, released = false; int note = 0; float vel = 0, env = 0, ph = 0, hz = 0; tvc::Biquad filt; };
    struct Drum { bool active = false; int pad = 0; long t = 0; float vel = 0, ph = 0; tvc::Biquad f; };
    double sr = 44100;
    std::array<Voice, 8> voices;
    std::array<Drum, 6> drums;
    float bend = 1.0f;
    int wave = 1;
    unsigned noise = 22222u;
};

//==============================================================================
// Escalas y acordes
struct Theory
{
    static const std::array<int, 12>& scaleMask (int scale)
    {
        static const std::array<std::array<int, 12>, 7> m {{
            {{ 1,1,1,1,1,1,1,1,1,1,1,1 }},   // Cromatica
            {{ 1,0,1,0,1,1,0,1,0,1,0,1 }},   // Mayor
            {{ 1,0,1,1,0,1,0,1,1,0,1,0 }},   // Menor
            {{ 1,0,1,1,0,1,0,1,1,0,0,1 }},   // Menor armonica
            {{ 1,0,1,0,1,0,0,1,0,1,0,0 }},   // Pentatonica mayor
            {{ 1,0,0,1,0,1,0,1,0,0,1,0 }},   // Pentatonica menor
            {{ 1,0,0,1,0,1,1,1,0,0,1,0 }},   // Blues
        }};
        return m[(size_t) std::clamp (scale, 0, 6)];
    }
    // nota permitida mas cercana a una altura continua
    static int nearest (float midi, int key, const std::array<int, 12>& mask)
    {
        const int c = (int) std::round (midi);
        int best = c; float bd = 1e9f;
        for (int n = c - 7; n <= c + 7; ++n)
        {
            if (! mask[(size_t) (((n - key) % 12 + 12) % 12)]) continue;
            const float dd = std::abs ((float) n - midi);
            if (dd < bd) { bd = dd; best = n; }
        }
        return best;
    }
    // siguiente grado de la escala hacia arriba (steps grados)
    static int diatonicUp (int note, int key, const std::array<int, 12>& mask, int steps)
    {
        int n = note;
        for (int s = 0; s < steps; ++s)
        {
            int guard = 0;
            do { ++n; } while (! mask[(size_t) (((n - key) % 12 + 12) % 12)] && ++guard < 12);
        }
        return n;
    }
};
} // namespace vmidi
