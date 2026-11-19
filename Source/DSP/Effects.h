#pragma once
#include "Biquad.h"
#include <vector>
#include <cmath>
#include <algorithm>

namespace tvc
{
//==============================================================================
// Preamp con 5 tipos de saturacion:
//   0 Valvula      - armonicos pares calidos (Warmth = asimetria)
//   1 Cinta        - compresion suave + perdida de agudos de cinta (Warmth = mas oscuro)
//   2 Transistor   - armonicos impares, mas filo y mordida
//   3 Transformador- satura sobre todo los graves ("hierro"), Warmth = mas cuerpo
//   4 Diodo        - asimetrico y agresivo, para efectos/rap
struct PreampStage
{
    int type = 0;
    float drive = 1.0f, bias = 0.0f, tbias = 0.0f, norm = 1.0f, trim = 1.0f, warmth = 0.0f;
    float dcX[2] {}, dcY[2] {}, tapeLp[2] {}, xfLow[2] {};
    float dcCoef = 0.9995f, tapeCoef = 0.5f, xfCoef = 0.01f;
    double osr = 88200.0;

    void prepare (double oversampledRate)
    {
        osr = oversampledRate;
        dcCoef = (float) std::exp (-2.0 * kPi * 10.0 / osr);
        xfCoef = 1.0f - (float) std::exp (-2.0 * kPi * 180.0 / osr);
        reset();
    }

    void reset()
    {
        for (int c = 0; c < 2; ++c) dcX[c] = dcY[c] = tapeLp[c] = xfLow[c] = 0.0f;
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
        // Cinta: mas drive / warmth = menos agudos
        const double fc = std::max (5000.0, 20000.0 - driveDb * 250.0 - warmth01 * 7000.0);
        tapeCoef = 1.0f - (float) std::exp (-2.0 * kPi * fc / osr);
    }

    inline float process (float x, int ch) noexcept
    {
        const float u = drive * x;
        const float comp = 1.0f / std::sqrt (drive);
        float y;

        switch (type)
        {
            case 1: // Cinta
            {
                y = 0.63661977f * std::atan (u * 1.5707963f) * comp;
                tapeLp[ch] += tapeCoef * (y - tapeLp[ch]);
                y = tapeLp[ch];
                break;
            }
            case 2: // Transistor (rodilla dura, impares)
            {
                const float a = u * (1.0f + 0.15f * warmth);
                y = a / std::pow (1.0f + std::pow (std::abs (a), 2.5f), 0.4f) * comp;
                break;
            }
            case 3: // Transformador (los graves saturan primero)
            {
                xfLow[ch] += xfCoef * (x - xfLow[ch]);
                const float lowU = drive * xfLow[ch];
                const float k = 1.0f + 2.5f * warmth;
                const float v = (u - lowU) + std::tanh (lowU * k) / k;
                y = std::tanh (v * 0.8f) / 0.8f * comp;
                break;
            }
            case 4: // Diodo (asimetrico)
            {
                y = (u >= 0.0f ? 1.0f - std::exp (-u) : -(1.0f - std::exp (1.8f * u)) / 1.8f) * comp * 1.2f;
                break;
            }
            default: // 0 Valvula
                y = (std::tanh (u + bias) - tbias) * norm;
                break;
        }

        // Bloqueador de DC
        const float out = y - dcX[ch] + dcCoef * dcY[ch];
        dcX[ch] = y; dcY[ch] = out;
        return out * trim;
    }
};

//==============================================================================
// Compresor con 4 modelos:
//   0 Limpio (VCA)            - transparente, respeta Attack/Release
//   1 Opto (estilo LA-2A)     - lento y musical, release de 2 etapas que depende del programa
//   2 FET (estilo 1176)       - muy rapido, agresivo, color de armonicos impares
//   3 Vari-Mu (estilo Fairchild) - el ratio sube con el nivel, muy suave y "pegado"
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

    void set (int newType, float thrDb, float r, float attMs, float relMs, float makeupDb) noexcept
    {
        type = newType; thr = thrDb; ratio = std::max (1.0f, r); makeup = dbToGain (makeupDb);
        float att = attMs, rel = relMs, rmsMsTime = 0.0f;
        switch (type)
        {
            case 1:  att = std::max (attMs, 10.0f);              rel = relMs;        knee = 12.0f; rmsMsTime = 20.0f; break;
            case 2:  att = std::clamp (attMs * 0.1f, 0.02f, 2.0f); rel = relMs * 0.5f; knee = 2.0f;  rmsMsTime = 0.0f;  break;
            case 3:  att = std::max (attMs, 8.0f);               rel = relMs * 1.5f; knee = 15.0f; rmsMsTime = 10.0f; break;
            default: knee = 6.0f; break;
        }
        attCoef = (float) std::exp (-1.0 / (std::max (0.02f, att) * 0.001 * sr));
        relSamples = (float) (std::max (5.0f, rel) * 0.001 * sr);
        rmsCoef = rmsMsTime > 0.0f ? (float) std::exp (-1.0 / (rmsMsTime * 0.001 * sr)) : 0.0f;
    }

    inline float computeGain (float l, float r) noexcept
    {
        const float peak = std::max (std::abs (l), std::abs (r));
        float lvl = peak;
        if (rmsCoef > 0.0f)
        {
            rmsMs = rmsCoef * rmsMs + (1.0f - rmsCoef) * peak * peak;
            lvl = std::sqrt (rmsMs) * 1.414f;
        }
        const float lvlDb = gainToDb (lvl);

        // Curva de ganancia con rodilla suave
        const float over = lvlDb - thr;
        float rr = ratio;
        if (type == 3) rr = 1.0f + (ratio - 1.0f) * std::clamp ((over + knee * 0.5f) / 20.0f, 0.15f, 1.0f);
        const float slope = 1.0f / rr - 1.0f;
        float target;
        if (2.0f * over <= -knee)               target = 0.0f;
        else if (2.0f * std::abs (over) < knee) target = slope * (over + knee * 0.5f) * (over + knee * 0.5f) / (2.0f * knee);
        else                                    target = slope * over;

        // Suavizado en el dominio de la reduccion
        if (target < grDb)
            grDb = attCoef * grDb + (1.0f - attCoef) * target;
        else
        {
            float relS = relSamples;
            if (type == 1) relS *= 1.0f + 4.0f * charge;      // Opto: cuanto mas comprime, mas lento suelta
            grDb += (target - grDb) / std::max (1.0f, relS);
        }

        if (type == 1) charge += chargeCoef * (std::min (1.0f, -grDb / 10.0f) - charge);

        lastGr = grDb;
        return dbToGain (grDb) * makeup;
    }

    // Color armonico del modelo (se aplica despues de la ganancia)
    inline float colour (float x) const noexcept
    {
        switch (type)
        {
            case 1: { const float k = 0.5f + 0.02f * -grDb; return std::tanh (x * k) / k; }              // tubo suave
            case 2: { const float k = 0.9f + 0.05f * -grDb; return std::tanh (x * k) / k; }              // FET impares
            case 3: { const float k = 0.6f; return (std::tanh (x * k + 0.08f) - std::tanh (0.08f)) / k; } // tubo pares
            default: return x;
        }
    }
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
