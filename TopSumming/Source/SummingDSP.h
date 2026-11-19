#pragma once
#include "DSP/Biquad.h"
#include <cmath>
#include <random>

namespace topsum
{
//==============================================================================
// Un canal de suma estilo consola britanica (inspirado en el Neve 8816):
//   transformador de entrada -> amplificador clase A -> transformador de salida.
// Se procesa a 2x de oversampling (osr = frecuencia sobremuestreada).
struct ConsoleChannel
{
    // "Tolerancias" de componentes: cada canal es un poco distinto
    float driveTrim = 1.0f, lowFreqTol = 1.0f, highFreqTol = 1.0f, evenTol = 1.0f;

    void prepare (double osr)
    {
        sr = osr;
        dcCoef = (float) std::exp (-2.0 * tvc::kPi * 8.0 / sr);
        updateFilters();
        reset();
    }

    void reset() { lowState = 0.0f; hfState = 0.0f; dcX = dcY = 0.0f; bump.reset(); }

    void setTolerances (float drv, float lowT, float highT, float evenT)
    {
        driveTrim = drv; lowFreqTol = lowT; highFreqTol = highT; evenTol = evenT;
        updateFilters();
    }

    void updateFilters()
    {
        lowCoef = 1.0f - (float) std::exp (-2.0 * tvc::kPi * 140.0 * lowFreqTol / sr);       // banda que satura el hierro
        hfCoef  = 1.0f - (float) std::exp (-2.0 * tvc::kPi * 26000.0 * highFreqTol / sr);    // caida suave de agudos
        bump.setPeak (sr, 55.0 * lowFreqTol, 0.8, 0.45);                                   // "peso" de graves del transformador
    }

    // drive: ganancia lineal de entrada; iron: 0..1 cantidad de color
    inline float process (float x, float drive, float iron) noexcept
    {
        const float d = drive * driveTrim;
        float u = x * d;

        // Transformador de entrada: los graves saturan antes (nucleo de hierro)
        lowState += lowCoef * (u - lowState);
        const float k = 0.6f + 2.4f * iron;
        const float low = std::tanh (lowState * k) / k;
        u = (u - lowState) + low;

        // Amplificador clase A: armonicos pares suaves + rodilla blanda
        const float even = 0.12f * iron * evenTol;
        u = u + even * u * u;
        u = std::tanh (u * 0.85f) / 0.85f;

        // Transformador de salida: peso en graves y agudos redondeados
        u = bump.process (u);
        hfState += hfCoef * (u - hfState);
        u = hfState;

        // Bloqueo de DC (los pares generan DC)
        const float y = u - dcX + dcCoef * dcY;
        dcX = u; dcY = y;

        // Compensacion parcial: con mas drive suena mas denso y un poco mas fuerte
        return y / std::pow (d, 0.85f);
    }

private:
    double sr = 88200.0;
    float lowCoef = 0.01f, hfCoef = 0.9f, dcCoef = 0.999f;
    float lowState = 0.0f, hfState = 0.0f, dcX = 0.0f, dcY = 0.0f;
    tvc::Biquad bump;
};

// Genera tolerancias repetibles a partir de una semilla (cada instancia tiene su "unidad")
inline void makeTolerances (unsigned seed, ConsoleChannel& l, ConsoleChannel& r)
{
    std::mt19937 rng (seed);
    std::normal_distribution<float> n (0.0f, 1.0f);
    auto clampN = [&] (float s) { return std::clamp (n (rng) * s, -2.5f * s, 2.5f * s); };
    for (auto* c : { &l, &r })
        c->setTolerances (std::pow (10.0f, clampN (0.15f) / 20.0f),   // +-0.15 dB de ganancia
                          1.0f + clampN (0.06f),                      // +-6 % frecuencia de graves
                          1.0f + clampN (0.05f),                      // +-5 % frecuencia de agudos
                          1.0f + clampN (0.15f));                     // +-15 % armonicos pares
}
} // namespace topsum
