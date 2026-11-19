#pragma once
#include <cmath>
#include <algorithm>

namespace tvc
{
constexpr double kPi = 3.14159265358979323846;

// Filtro biquad (formulas RBJ Audio EQ Cookbook). Sin asignacion de memoria:
// seguro para usar en el hilo de audio.
struct Biquad
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;

    void reset() noexcept { z1 = z2 = 0.0f; }

    inline float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void setNormalised (double B0, double B1, double B2, double A0, double A1, double A2) noexcept
    {
        const double inv = 1.0 / A0;
        b0 = (float) (B0 * inv); b1 = (float) (B1 * inv); b2 = (float) (B2 * inv);
        a1 = (float) (A1 * inv); a2 = (float) (A2 * inv);
    }

    void setIdentity() noexcept { b0 = 1.0f; b1 = b2 = a1 = a2 = 0.0f; }

    // Pasa-altos de 1er orden (6 dB/oct)
    void setHighPass1 (double sr, double f) noexcept
    {
        const double K = std::tan (kPi * clampFreq (f, sr) / sr);
        const double n = 1.0 / (1.0 + K);
        b0 = (float) n; b1 = (float) -n; b2 = 0.0f;
        a1 = (float) ((K - 1.0) * n); a2 = 0.0f;
    }

    // Pasa-bajos de 1er orden (6 dB/oct)
    void setLowPass1 (double sr, double f) noexcept
    {
        const double K = std::tan (kPi * clampFreq (f, sr) / sr);
        const double n = 1.0 / (1.0 + K);
        b0 = (float) (K * n); b1 = (float) (K * n); b2 = 0.0f;
        a1 = (float) ((K - 1.0) * n); a2 = 0.0f;
    }

    static double clampFreq (double f, double sr) noexcept { return std::clamp (f, 10.0, sr * 0.49); }

    void setLowPass (double sr, double f, double q) noexcept
    {
        const double w0 = 2.0 * kPi * clampFreq (f, sr) / sr, c = std::cos (w0), al = std::sin (w0) / (2.0 * q);
        setNormalised ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }

    void setHighPass (double sr, double f, double q) noexcept
    {
        const double w0 = 2.0 * kPi * clampFreq (f, sr) / sr, c = std::cos (w0), al = std::sin (w0) / (2.0 * q);
        setNormalised ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }

    void setBandPass (double sr, double f, double q) noexcept
    {
        const double w0 = 2.0 * kPi * clampFreq (f, sr) / sr, c = std::cos (w0), al = std::sin (w0) / (2.0 * q);
        setNormalised (al, 0.0, -al, 1 + al, -2 * c, 1 - al);
    }

    void setPeak (double sr, double f, double q, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = 2.0 * kPi * clampFreq (f, sr) / sr, c = std::cos (w0), al = std::sin (w0) / (2.0 * q);
        setNormalised (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }

    void setLowShelf (double sr, double f, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = 2.0 * kPi * clampFreq (f, sr) / sr, c = std::cos (w0), s = std::sin (w0);
        const double al = s / 2.0 * std::sqrt (2.0), sA = 2.0 * std::sqrt (A) * al;
        setNormalised (A * ((A + 1) - (A - 1) * c + sA),
                       2 * A * ((A - 1) - (A + 1) * c),
                       A * ((A + 1) - (A - 1) * c - sA),
                       (A + 1) + (A - 1) * c + sA,
                       -2 * ((A - 1) + (A + 1) * c),
                       (A + 1) + (A - 1) * c - sA);
    }

    void setHighShelf (double sr, double f, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = 2.0 * kPi * clampFreq (f, sr) / sr, c = std::cos (w0), s = std::sin (w0);
        const double al = s / 2.0 * std::sqrt (2.0), sA = 2.0 * std::sqrt (A) * al;
        setNormalised (A * ((A + 1) + (A - 1) * c + sA),
                       -2 * A * ((A - 1) + (A + 1) * c),
                       A * ((A + 1) + (A - 1) * c - sA),
                       (A + 1) - (A - 1) * c + sA,
                       2 * ((A - 1) - (A + 1) * c),
                       (A + 1) - (A - 1) * c - sA);
    }
};

inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g) noexcept  { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }
} // namespace tvc
