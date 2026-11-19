#pragma once
#include "Biquad.h"
#include "PitchCorrector.h"
#include <vector>
#include <cmath>
#include <algorithm>

namespace tvc
{
//==============================================================================
// Preamp con 8 modelos:
//   0 Clean       - transparente, solo ganancia limpia (recorte muy suave al final)
//   1 Tube        - valvula: armonicos pares calidos (Warmth = asimetria)
//   2 Vintage     - saturacion suave, cuerpo en medios-graves y agudos redondeados
//   3 Modern      - brillante y limpio: saturacion firme, graves ajustados, brillo arriba
//   4 Tape        - compresion suave y perdida de agudos de cinta
//   5 Transformer - satura sobre todo los graves ("hierro")
//   6 Transistor  - armonicos impares, filo y mordida
//   7 Diode       - asimetrico y agresivo
struct PreampStage
{
    int type = 0;
    float drive = 1.0f, bias = 0.0f, tbias = 0.0f, norm = 1.0f, trim = 1.0f, warmth = 0.0f;
    float dcX[2] {}, dcY[2] {}, tapeLp[2] {}, xfLow[2] {};
    float dcCoef = 0.9995f, tapeCoef = 0.5f, xfCoef = 0.01f;
    Biquad tone1[2], tone2[2];
    double osr = 88200.0;
    int lastType = -1; float lastWarmth = -1.0f, lastDrive = -100.0f;

    void prepare (double oversampledRate)
    {
        osr = oversampledRate;
        dcCoef = (float) std::exp (-2.0 * kPi * 10.0 / osr);
        xfCoef = 1.0f - (float) std::exp (-2.0 * kPi * 180.0 / osr);
        lastType = -1;
        reset();
    }

    void reset()
    {
        for (int c = 0; c < 2; ++c) { dcX[c] = dcY[c] = tapeLp[c] = xfLow[c] = 0.0f; tone1[c].reset(); tone2[c].reset(); }
    }

    void set (int newType, float driveDb, float warmth01, float trimDb) noexcept
    {
        type   = newType;
        drive  = dbToGain (driveDb);
        warmth = warmth01;
        bias   = 0.35f * warmth01;
        tbias  = std::tanh (bias);
        norm   = 1.0f / (std::sqrt (drive) * (1.0f - tbias * tbias));
        trim   = dbToGain (trimDb);

        // Filtros de "tono" de cada modelo (solo se recalculan si cambian)
        if (type != lastType || std::abs (warmth01 - lastWarmth) > 1.0e-3f || std::abs (driveDb - lastDrive) > 1.0e-3f)
        {
            lastType = type; lastWarmth = warmth01; lastDrive = driveDb;
            double fc = 20000.0;
            if (type == 4) fc = std::max (5000.0, 20000.0 - driveDb * 250.0 - warmth01 * 7000.0);     // cinta
            if (type == 2) fc = 15000.0 - warmth01 * 6000.0;                                          // vintage
            tapeCoef = 1.0f - (float) std::exp (-2.0 * kPi * fc / osr);
            for (int c = 0; c < 2; ++c)
            {
                if (type == 2) { tone1[c].setPeak (osr, 250.0, 0.8, 1.5 * warmth01); tone2[c].setIdentity(); }
                else if (type == 3) { tone1[c].setHighShelf (osr, 9000.0, 2.5 * warmth01 + 0.5); tone2[c].setHighPass (osr, 25.0, 0.707); }
                else { tone1[c].setIdentity(); tone2[c].setIdentity(); }
            }
        }
    }

    inline float process (float x, int ch) noexcept
    {
        const float u = drive * x;
        const float comp = 1.0f / std::sqrt (drive);
        float y;

        switch (type)
        {
            case 0: // Clean
            {
                const float a = std::abs (u);
                y = (a <= 0.8f ? u : std::copysign (0.8f + 0.2f * std::tanh ((a - 0.8f) / 0.2f), u)) * comp;
                break;
            }
            case 2: // Vintage
            {
                const float b = 0.1f * warmth;
                y = (std::tanh (u * 0.8f + b) - std::tanh (b)) / 0.8f * comp;
                tapeLp[ch] += tapeCoef * (y - tapeLp[ch]);
                y = tone1[ch].process (tapeLp[ch]);
                break;
            }
            case 3: // Modern
            {
                y = u / std::cbrt (1.0f + std::abs (u * u * u)) * comp;
                y = tone2[ch].process (tone1[ch].process (y));
                break;
            }
            case 4: // Tape
            {
                y = 0.63661977f * std::atan (u * 1.5707963f) * comp;
                tapeLp[ch] += tapeCoef * (y - tapeLp[ch]);
                y = tapeLp[ch];
                break;
            }
            case 5: // Transformer
            {
                xfLow[ch] += xfCoef * (x - xfLow[ch]);
                const float lowU = drive * xfLow[ch];
                const float k = 1.0f + 2.5f * warmth;
                const float v = (u - lowU) + std::tanh (lowU * k) / k;
                y = std::tanh (v * 0.8f) / 0.8f * comp;
                break;
            }
            case 6: // Transistor
            {
                const float a = u * (1.0f + 0.15f * warmth);
                y = a / std::pow (1.0f + std::pow (std::abs (a), 2.5f), 0.4f) * comp;
                break;
            }
            case 7: // Diode
                y = (u >= 0.0f ? 1.0f - std::exp (-u) : -(1.0f - std::exp (1.8f * u)) / 1.8f) * comp * 1.2f;
                break;
            default: // 1 Tube
                y = (std::tanh (u + bias) - tbias) * norm;
                break;
        }

        const float out = y - dcX[ch] + dcCoef * dcY[ch];
        dcX[ch] = y; dcY[ch] = out;
        return out * trim;
    }
};

//==============================================================================
// Compresor con 7 modos:
//   0 Clean VCA     - control preciso, respeta Attack y Release
//   1 Vintage Opto  - lento y musical, release de 2 etapas segun el programa
//   2 FET Punch     - muy rapido y agresivo, armonicos impares
//   3 Vari-Mu Glue  - el ratio sube con el nivel, pega la voz
//   4 LA Vocal      - opto mas rapido y denso, ideal para voces al frente
//   5 Fast          - ataque y release rapidisimos, controla picos
//   6 Smooth        - deteccion RMS lenta, compresion muy suave
struct VocalCompressor
{
    double sr = 44100.0;
    int type = 0;
    float thr = -18.0f, ratio = 4.0f, makeup = 1.0f, knee = 6.0f;
    float attCoef = 0.0f, relSamples = 1000.0f, rmsCoef = 0.0f;
    float rmsMs = 0.0f, grDb = 0.0f, charge = 0.0f, chargeCoef = 0.0f;
    float lastGr = 0.0f;   // dB de reduccion (para el vumetro)

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        chargeCoef = 1.0f - (float) std::exp (-1.0 / (1.5 * sr));
        reset();
    }
    void reset() { rmsMs = 0.0f; grDb = 0.0f; charge = 0.0f; lastGr = 0.0f; }

    void set (int newType, float thrDb, float r, float attMs, float relMs, float makeupDb, float kneeDb) noexcept
    {
        type = newType; thr = thrDb; ratio = std::max (1.0f, r); makeup = dbToGain (makeupDb);
        knee = std::max (0.01f, kneeDb);
        float att = attMs, rel = relMs, rmsMsTime = 0.0f;
        switch (type)
        {
            case 1:  att = std::max (attMs, 10.0f);                 rel = relMs;        rmsMsTime = 20.0f; break;
            case 2:  att = std::clamp (attMs * 0.1f, 0.02f, 2.0f);  rel = relMs * 0.5f; break;
            case 3:  att = std::max (attMs, 8.0f);                  rel = relMs * 1.5f; rmsMsTime = 10.0f; break;
            case 4:  att = std::max (attMs, 5.0f);                  rel = relMs;        rmsMsTime = 12.0f; break;
            case 5:  att = std::clamp (attMs * 0.05f, 0.02f, 1.0f); rel = relMs * 0.3f; break;
            case 6:  att = std::max (attMs, 20.0f);                 rel = relMs * 2.0f; rmsMsTime = 30.0f; break;
            default: break;
        }
        attCoef = (float) std::exp (-1.0 / (std::max (0.02f, att) * 0.001 * sr));
        relSamples = (float) (std::max (5.0f, rel) * 0.001 * sr);
        rmsCoef = rmsMsTime > 0.0f ? (float) std::exp (-1.0 / (rmsMsTime * 0.001 * sr)) : 0.0f;
    }

    // Curva estatica: reduccion (dB, <= 0) para un nivel de entrada en dB
    float staticGr (float lvlDb) const noexcept
    {
        const float over = lvlDb - thr;
        float rr = ratio;
        if (type == 3) rr = 1.0f + (ratio - 1.0f) * std::clamp ((over + knee * 0.5f) / 20.0f, 0.15f, 1.0f);
        const float slope = 1.0f / rr - 1.0f;
        if (2.0f * over <= -knee)               return 0.0f;
        if (2.0f * std::abs (over) < knee)      return slope * (over + knee * 0.5f) * (over + knee * 0.5f) / (2.0f * knee);
        return slope * over;
    }

    // detL/detR: senal del detector (propia o sidechain, ya filtrada)
    inline float computeGain (float detL, float detR) noexcept
    {
        const float peak = std::max (std::abs (detL), std::abs (detR));
        float lvl = peak;
        if (rmsCoef > 0.0f)
        {
            rmsMs = rmsCoef * rmsMs + (1.0f - rmsCoef) * peak * peak;
            lvl = std::sqrt (rmsMs) * 1.414f;
        }
        const float target = staticGr (gainToDb (lvl));

        if (target < grDb)
            grDb = attCoef * grDb + (1.0f - attCoef) * target;
        else
        {
            float relS = relSamples;
            if (type == 1 || type == 4) relS *= 1.0f + (type == 1 ? 4.0f : 2.5f) * charge;   // opto: suelta mas lento si comprime mucho
            grDb += (target - grDb) / std::max (1.0f, relS);
        }
        if (type == 1 || type == 4) charge += chargeCoef * (std::min (1.0f, -grDb / 10.0f) - charge);

        lastGr = grDb;
        return dbToGain (grDb) * makeup;
    }

    // Color armonico del modelo (se aplica despues de la ganancia)
    inline float colour (float x) const noexcept
    {
        switch (type)
        {
            case 1: { const float k = 0.5f + 0.02f * -grDb; return std::tanh (x * k) / k; }
            case 2: { const float k = 0.9f + 0.05f * -grDb; return std::tanh (x * k) / k; }
            case 3: { const float k = 0.6f; return (std::tanh (x * k + 0.08f) - std::tanh (0.08f)) / k; }
            case 4: { const float k = 0.7f + 0.03f * -grDb; return std::tanh (x * k) / k; }      // denso
            case 6: { const float k = 0.4f; return std::tanh (x * k) / k; }
            default: return x;
        }
    }
};

//==============================================================================
// WIDER: abre la voz en estereo. Una perilla (cantidad) y 5 efectos:
//   0 Wide      - ensancha con senal lateral decorrelacionada (100 % compatible en mono)
//   1 Chorus    - dos voces moduladas en contrafase
//   2 Double    - doblaje automatico (ADT): dos tomas "falsas" con variaciones lentas
//   3 Shift     - micro pitch shift: izquierda un poco arriba, derecha un poco abajo
//   4 Dimension - ensemble de 3 voces suaves por lado (espacioso sin sonar a chorus)
struct Widener
{
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        size = 1;
        while (size < (int) (sr * 0.08) + 8) size <<= 1;
        buf.assign ((size_t) size, 0.0f);
        mask = size - 1;
        for (auto& f : sideHp) f.setHighPass (sr, 300.0, 0.707);
        for (auto& f : dblHp)  f.setHighPass (sr, 150.0, 0.707);
        for (auto& f : dblLp)  f.setLowPass (sr, 8000.0, 0.707);
        shifter[0].prepare (sr, 0.03);
        shifter[1].prepare (sr, 0.03);
        reset();
    }

    void reset()
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        w = 0; ph = 0.0;
        for (auto* f : { &sideHp[0], &sideHp[1], &dblHp[0], &dblHp[1], &dblLp[0], &dblLp[1] }) f->reset();
        shifter[0].reset(); shifter[1].reset();
    }

    inline void process (float& l, float& r, int mode, float amt) noexcept
    {
        const float m = 0.5f * (l + r);
        buf[(size_t) w] = m;
        ph += 1.0 / sr;
        if (ph > 1000.0) ph -= 1000.0;
        const double twoPi = 2.0 * kPi;

        float wl = 0.0f, wr = 0.0f;
        switch (mode)
        {
            case 1: // Chorus
            {
                const double lfo = std::sin (twoPi * 0.75 * ph);
                wl = read (0.014 + 0.0035 * lfo);
                wr = read (0.014 - 0.0035 * lfo);
                const float mix = 0.65f * amt;
                l = l * (1.0f - 0.2f * amt) + wl * mix;
                r = r * (1.0f - 0.2f * amt) + wr * mix;
                break;
            }
            case 2: // Double (ADT)
            {
                const double a = std::sin (twoPi * 0.13 * ph) + 0.6 * std::sin (twoPi * 0.29 * ph + 1.3);
                const double b = std::sin (twoPi * 0.17 * ph + 2.1) + 0.6 * std::sin (twoPi * 0.23 * ph + 0.4);
                wl = dblLp[0].process (dblHp[0].process (read (0.024 + 0.0012 * a)));
                wr = dblLp[1].process (dblHp[1].process (read (0.032 + 0.0014 * b)));
                const float mix = 0.75f * amt;
                l = l * (1.0f - 0.25f * amt) + wl * mix;
                r = r * (1.0f - 0.25f * amt) + wr * mix;
                break;
            }
            case 3: // Shift (micro pitch)
            {
                const float cents = 5.0f + 13.0f * amt;
                wl = shifter[0].process (m, std::exp2 ( cents / 1200.0f));
                wr = shifter[1].process (m, std::exp2 (-cents / 1200.0f));
                const float mix = 0.7f * amt;
                l = l * (1.0f - 0.2f * amt) + wl * mix;
                r = r * (1.0f - 0.2f * amt) + wr * mix;
                break;
            }
            case 4: // Dimension (ensemble)
            {
                for (int v = 0; v < 3; ++v)
                {
                    const double off = (double) v * twoPi / 3.0;
                    const double base = 0.007 + 0.002 * v;
                    wl += read (base + 0.0006 * std::sin (twoPi * 0.5 * ph + off));
                    wr += read (base + 0.0006 * std::sin (twoPi * 0.5 * ph + off + kPi));
                }
                wl *= 0.4f; wr *= 0.4f;
                const float mix = 0.7f * amt;
                l = l * (1.0f - 0.2f * amt) + wl * mix;
                r = r * (1.0f - 0.2f * amt) + wr * mix;
                break;
            }
            default: // Wide: lateral decorrelado, se cancela en mono
            {
                const float s = 0.5f * (read (0.011) - read (0.017));
                const float side = sideHp[0].process (s) * 1.6f * amt;
                l += side;
                r -= side;
                break;
            }
        }
        w = (w + 1) & mask;
    }

private:
    inline float read (double seconds) const noexcept
    {
        const double pos = (double) w - seconds * sr;
        const double fl = std::floor (pos);
        const float t = (float) (pos - fl);
        const int i = (int) fl;
        const float a = buf[(size_t) (i & mask)], b = buf[(size_t) ((i + 1) & mask)];
        return a + t * (b - a);
    }

    double sr = 44100.0, ph = 0.0;
    std::vector<float> buf;
    int size = 0, mask = 0, w = 0;
    Biquad sideHp[2], dblHp[2], dblLp[2];
    PitchShifter shifter[2];
};

//==============================================================================
// Delay estereo con feedback filtrado y modo ping-pong
struct StereoDelay
{
    std::vector<float> bufL, bufR;
    int size = 0, w = 0;
    double sr = 44100.0;
    float curDelay = 1000.0f, targetDelay = 1000.0f;
    float lpL = 0.0f, lpR = 0.0f, lpCoef = 0.5f;
    float feedback = 0.25f, mix = 0.1f;
    bool pingPong = false;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        size = (int) (sr * 2.2) + 8;
        bufL.assign ((size_t) size, 0.0f);
        bufR.assign ((size_t) size, 0.0f);
        reset();
    }

    void reset()
    {
        std::fill (bufL.begin(), bufL.end(), 0.0f);
        std::fill (bufR.begin(), bufR.end(), 0.0f);
        w = 0; lpL = lpR = 0.0f;
        curDelay = targetDelay;
    }

    void set (float delayMs, float fb, float mix01, float hiCutHz, bool pp) noexcept
    {
        targetDelay = std::clamp ((float) (delayMs * 0.001 * sr), 4.0f, (float) size - 8.0f);
        feedback = std::clamp (fb, 0.0f, 0.95f);
        mix = mix01;
        lpCoef = 1.0f - (float) std::exp (-2.0 * kPi * hiCutHz / sr);
        pingPong = pp;
    }

    inline float read (const std::vector<float>& b, float d) const noexcept
    {
        float pos = (float) w - d;
        if (pos < 0.0f) pos += (float) size;
        const int i0 = (int) pos;
        const int i1 = (i0 + 1) % size;
        const float t = pos - (float) i0;
        return b[(size_t) i0] + t * (b[(size_t) i1] - b[(size_t) i0]);
    }

    inline void process (float& l, float& r) noexcept
    {
        curDelay += 0.0005f * (targetDelay - curDelay);   // cambio de tiempo sin clicks

        const float dl = read (bufL, curDelay);
        const float dr = read (bufR, curDelay);
        lpL += lpCoef * (dl - lpL);
        lpR += lpCoef * (dr - lpR);

        if (pingPong)
        {
            bufL[(size_t) w] = 0.5f * (l + r) + lpR * feedback;
            bufR[(size_t) w] = lpL * feedback;
        }
        else
        {
            bufL[(size_t) w] = l + lpL * feedback;
            bufR[(size_t) w] = r + lpR * feedback;
        }

        w = (w + 1) % size;
        l += lpL * mix;
        r += lpR * mix;
    }
};
} // namespace tvc
