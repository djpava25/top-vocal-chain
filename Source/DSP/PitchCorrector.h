#pragma once
#include "Biquad.h"
#include <vector>
#include <array>
#include <cmath>

namespace tvc
{
//==============================================================================
// Detector de tono YIN (de Cheveigne & Kawahara) sobre una senal diezmada a ~11 kHz
// para que sea barato en CPU. Rango de voz: 70 Hz - 1000 Hz.
class PitchDetector
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        decim = std::max (1, (int) std::round (sr / 11025.0));
        dsr = sr / decim;
        aa1.setLowPass (sr, dsr * 0.42, 0.707);
        aa2.setLowPass (sr, dsr * 0.42, 0.707);

        frameSize = (int) std::round (dsr * 0.046);           // ~46 ms
        tauMax    = (int) std::ceil (dsr / 70.0);
        tauMin    = std::max (2, (int) std::floor (dsr / 1000.0));
        hop       = frameSize / 4;

        ring.assign ((size_t) frameSize, 0.0f);
        frame.assign ((size_t) frameSize, 0.0f);
        diff.assign ((size_t) tauMax + 2, 0.0f);
        reset();
    }

    void reset()
    {
        std::fill (ring.begin(), ring.end(), 0.0f);
        writePos = decimCounter = hopCounter = 0;
        aa1.reset(); aa2.reset();
        frequency = 0.0f; confidence = 0.0f; voiced = false;
    }

    // Devuelve true cuando hay una nueva estimacion disponible
    inline bool push (float x) noexcept
    {
        const float y = aa2.process (aa1.process (x));
        if (++decimCounter < decim) return false;
        decimCounter = 0;

        ring[(size_t) writePos] = y;
        writePos = (writePos + 1) % frameSize;

        if (++hopCounter >= hop)
        {
            hopCounter = 0;
            analyse();
            return true;
        }
        return false;
    }

    float getFrequency()  const noexcept { return frequency; }
    float getConfidence() const noexcept { return confidence; }
    bool  isVoiced()      const noexcept { return voiced; }

private:
    void analyse() noexcept
    {
        double energy = 0.0;
        for (int i = 0; i < frameSize; ++i)
        {
            frame[(size_t) i] = ring[(size_t) ((writePos + i) % frameSize)];
            energy += frame[(size_t) i] * frame[(size_t) i];
        }

        const float rms = (float) std::sqrt (energy / frameSize);
        if (rms < 0.003f) { voiced = false; confidence = 0.0f; return; }   // ~ -50 dBFS

        const int W = frameSize - tauMax;
        diff[0] = 1.0f;
        float running = 0.0f;
        int bestTau = -1;

        for (int tau = 1; tau <= tauMax; ++tau)
        {
            float d = 0.0f;
            for (int j = 0; j < W; ++j)
            {
                const float delta = frame[(size_t) j] - frame[(size_t) (j + tau)];
                d += delta * delta;
            }
            running += d;
            diff[(size_t) tau] = running > 0.0f ? d * (float) tau / running : 1.0f;
        }

        constexpr float threshold = 0.15f;
        for (int tau = tauMin; tau < tauMax; ++tau)
        {
            if (diff[(size_t) tau] < threshold)
            {
                while (tau + 1 < tauMax && diff[(size_t) tau + 1] < diff[(size_t) tau]) ++tau;
                bestTau = tau;
                break;
            }
        }

        if (bestTau < 0) { voiced = false; confidence = 0.0f; return; }

        // Interpolacion parabolica para precision sub-muestra
        float betterTau = (float) bestTau;
        if (bestTau > 1 && bestTau < tauMax)
        {
            const float s0 = diff[(size_t) bestTau - 1], s1 = diff[(size_t) bestTau], s2 = diff[(size_t) bestTau + 1];
            const float denom = 2.0f * (2.0f * s1 - s2 - s0);
            if (std::abs (denom) > 1.0e-9f) betterTau += (s2 - s0) / denom;
        }

        frequency  = (float) (dsr / betterTau);
        confidence = 1.0f - diff[(size_t) bestTau];
        voiced     = frequency > 65.0f && frequency < 1100.0f;
    }

    double sr = 44100.0, dsr = 11025.0;
    int decim = 4, frameSize = 512, tauMax = 160, tauMin = 11, hop = 128;
    int writePos = 0, decimCounter = 0, hopCounter = 0;
    Biquad aa1, aa2;
    std::vector<float> ring, frame, diff;
    float frequency = 0.0f, confidence = 0.0f;
    bool voiced = false;
};

//==============================================================================
// Pitch shifter de doble cabezal (linea de retardo con crossfade Hann).
// Latencia constante = minDelay + window/2.
class PitchShifter
{
public:
    void prepare (double sampleRate)
    {
        window = (float) std::round (sampleRate * 0.028);    // ventana ~28 ms
        int size = 1;
        while (size < (int) (window * 2 + 16)) size <<= 1;
        buffer.assign ((size_t) size, 0.0f);
        mask = size - 1;
        reset();
    }

    void reset()
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
        phase = 0.5;      // con phase = 0.5 un cabezal tiene ganancia 1 -> sin filtro peine
    }

    int getLatency() const noexcept { return (int) std::round (minDelay + window * 0.5f); }

    inline float process (float x, float ratio) noexcept
    {
        buffer[(size_t) writePos] = x;

        if (std::abs (ratio - 1.0f) < 0.0004f)
        {
            // Sin correccion: lleva la fase suavemente a 0.5 (un solo cabezal activo)
            const double target = 0.5, step = 0.004 / window;
            if (phase < target) phase = std::min (target, phase + step);
            else                phase = std::max (target, phase - step);
        }
        else
        {
            phase += (1.0 - ratio) / window;
            phase -= std::floor (phase);
        }

        double p2 = phase + 0.5;
        p2 -= std::floor (p2);

        const float d1 = minDelay + (float) phase * window;
        const float d2 = minDelay + (float) p2 * window;
        const float s1 = std::sin ((float) (kPi * phase));
        const float s2 = std::sin ((float) (kPi * p2));

        const float out = read (d1) * s1 * s1 + read (d2) * s2 * s2;
        writePos = (writePos + 1) & mask;
        return out;
    }

private:
    inline float read (float delay) const noexcept
    {
        const float pos = (float) writePos - delay;
        const float fl = std::floor (pos);
        const float t = pos - fl;
        const int i = (int) fl;
        const float xm1 = buffer[(size_t) ((i - 1) & mask)];
        const float x0  = buffer[(size_t) (i & mask)];
        const float x1  = buffer[(size_t) ((i + 1) & mask)];
        const float x2  = buffer[(size_t) ((i + 2) & mask)];
        // Interpolacion Hermite de 4 puntos
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * t + c2) * t + c1) * t + x0;
    }

    std::vector<float> buffer;
    int writePos = 0, mask = 0;
    float window = 1200.0f;
    const float minDelay = 4.0f;
    double phase = 0.5;
};

//==============================================================================
// Escalas musicales para el autotune
struct Scales
{
    static constexpr int numScales = 6;
    static const std::array<int, 12>& mask (int scale) noexcept
    {
        static const std::array<std::array<int, 12>, numScales> m {{
            {{ 1,1,1,1,1,1,1,1,1,1,1,1 }},   // Cromatica
            {{ 1,0,1,0,1,1,0,1,0,1,0,1 }},   // Mayor
            {{ 1,0,1,1,0,1,0,1,1,0,1,0 }},   // Menor natural
            {{ 1,0,1,1,0,1,0,1,1,0,0,1 }},   // Menor armonica
            {{ 1,0,1,0,1,0,0,1,0,1,0,0 }},   // Pentatonica mayor
            {{ 1,0,0,1,0,1,0,1,0,0,1,0 }},   // Pentatonica menor
        }};
        return m[(size_t) std::clamp (scale, 0, numScales - 1)];
    }

    static bool allowed (int midiNote, int key, int scale) noexcept
    {
        const int degree = ((midiNote - key) % 12 + 12) % 12;
        return mask (scale)[(size_t) degree] != 0;
    }

    static int nearest (float midi, int key, int scale) noexcept
    {
        const int centre = (int) std::round (midi);
        int best = centre;
        float bestDist = 1000.0f;
        for (int n = centre - 6; n <= centre + 6; ++n)
        {
            if (! allowed (n, key, scale)) continue;
            const float d = std::abs ((float) n - midi);
            if (d < bestDist) { bestDist = d; best = n; }
        }
        return best;
    }
};
} // namespace tvc
