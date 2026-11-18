#pragma once
#include "Biquad.h"
#include <vector>
#include <cmath>
#include <algorithm>

namespace tvc
{
//==============================================================================
// Preamp con saturacion tipo valvula (armonicos pares controlados por "Warmth")
struct PreampStage
{
    float drive = 1.0f, bias = 0.0f, norm = 1.0f, trim = 1.0f;
    float dcX[2] {}, dcY[2] {};
    float dcCoef = 0.9995f;

    void prepare (double oversampledRate) { dcCoef = (float) std::exp (-2.0 * kPi * 10.0 / oversampledRate); reset(); }
    void reset() { dcX[0] = dcX[1] = dcY[0] = dcY[1] = 0.0f; }

    void set (float driveDb, float warmth01, float trimDb) noexcept
    {
        drive = dbToGain (driveDb);
        bias  = 0.35f * warmth01;
        const float tb = std::tanh (bias);
        tbias = tb;
        // Compensacion parcial de ganancia: mantiene el nivel percibido parecido
        norm  = 1.0f / (std::sqrt (drive) * (1.0f - tb * tb));
        trim  = dbToGain (trimDb);
    }

    inline float process (float x, int ch) noexcept
    {
        float y = (std::tanh (drive * x + bias) - tbias) * norm;
        // Bloqueador de DC (la asimetria genera DC)
        const float out = y - dcX[ch] + dcCoef * dcY[ch];
        dcX[ch] = y; dcY[ch] = out;
        return out * trim;
    }

    float tbias = 0.0f;
};

//==============================================================================
// Compresor feed-forward con rodilla suave, estereo enlazado
struct VocalCompressor
{
    double sr = 44100.0;
    float envDb = -120.0f, attCoef = 0.0f, relCoef = 0.0f;
    float thr = -18.0f, ratio = 4.0f, makeup = 1.0f;
    static constexpr float knee = 6.0f;
    float lastGr = 0.0f;   // dB de reduccion (para el vumetro)

    void prepare (double sampleRate) { sr = sampleRate; reset(); }
    void reset() { envDb = -120.0f; lastGr = 0.0f; }

    void set (float thrDb, float r, float attMs, float relMs, float makeupDb) noexcept
    {
        thr = thrDb; ratio = std::max (1.0f, r);
        attCoef = (float) std::exp (-1.0 / (std::max (0.05f, attMs) * 0.001 * sr));
        relCoef = (float) std::exp (-1.0 / (std::max (1.0f, relMs) * 0.001 * sr));
        makeup = dbToGain (makeupDb);
    }

    inline float computeGain (float l, float r) noexcept
    {
        const float lvl = gainToDb (std::max (std::abs (l), std::abs (r)));
        const float c = lvl > envDb ? attCoef : relCoef;
        envDb = c * envDb + (1.0f - c) * lvl;

        const float over = envDb - thr;
        float gr = 0.0f;
        const float slope = 1.0f / ratio - 1.0f;
        if (2.0f * over <= -knee)            gr = 0.0f;
        else if (2.0f * std::abs (over) < knee) gr = slope * (over + knee * 0.5f) * (over + knee * 0.5f) / (2.0f * knee);
        else                                 gr = slope * over;

        lastGr = gr;
        return dbToGain (gr) * makeup;
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
