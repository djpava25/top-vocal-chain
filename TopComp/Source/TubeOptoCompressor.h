#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include "DSP/Biquad.h"

// Compresor optico a valvulas (estilo CL 1B), basado en el codigo de Dj Pava HM:
//  - deteccion hibrida RMS/pico
//  - codo suave amplio de 10 dB
//  - balistica optica: ataque suave y relajacion en dos etapas (memoria de la celda optica)
//  - etapa de valvula asimetrica (2do y 3er armonico) y transformador de salida (+0.8 dB en 100 Hz)
// Correcciones: la deteccion ya no sigue cada muestra (no distorsiona graves), las dos envolventes de
// release siguen al ataque (sin saltos al soltar), la valvula no sube el volumen y hay un transformador por canal.
class TubeOptoCompressor
{
public:
    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        envelope = releaseFastEnv = releaseSlowEnv = 0.0f;
        rms = peak = 0.0f;
        rmsCoeff = std::exp (-1.0f / (float) (sampleRate * 0.010));     // RMS de 10 ms
        peakCoeff = std::exp (-1.0f / (float) (sampleRate * 0.005));    // pico con caida de 5 ms
        for (auto& f : transformerFilter) { f.setLowShelf (sampleRate, 100.0, 0.8); f.reset(); }
    }

    // Parametros principales estilo CL 1B
    void setParameters (float thrDb, float newRatio, float attackMs, float releaseMs, float newDrive)
    {
        thresholdDb = thrDb;
        ratio = juce::jlimit (2.0f, 10.0f, newRatio);                     // CL 1B va de 2:1 a 10:1
        drive = newDrive;
        attackCoeff = std::exp (-1.0f / (float) (sampleRate * attackMs * 0.001));
        releaseFastCoeff = std::exp (-1.0f / (float) (sampleRate * releaseMs * 0.001));
        releaseSlowCoeff = std::exp (-1.0f / (float) (sampleRate * releaseMs * 0.0045));   // segunda etapa (memoria optica)
    }

    // detector + curva + balistica; devuelve la reduccion en dB (positiva)
    float detect (float sidechain) noexcept
    {
        // 1. deteccion hibrida RMS/pico (en una onda senoidal ambas valen el pico)
        const float x2 = sidechain * sidechain, ax = std::abs (sidechain);
        rms = rmsCoeff * rms + (1.0f - rmsCoeff) * x2;
        peak = ax > peak ? ax : peakCoeff * peak + (1.0f - peakCoeff) * ax;
        const float level = 0.5f * (std::sqrt (2.0f * rms) + peak);
        const float inputDb = juce::Decibels::gainToDecibels (level + 1.0e-6f, -120.0f);

        // 2. codo suave amplio de 10 dB
        const float kneeWidth = 10.0f, overshoot = inputDb - thresholdDb;
        float gainReductionDb = 0.0f;
        if (overshoot <= -kneeWidth * 0.5f) gainReductionDb = 0.0f;
        else if (overshoot < kneeWidth * 0.5f)
        {
            const float k = overshoot + kneeWidth * 0.5f;
            gainReductionDb = ((1.0f / ratio) - 1.0f) * (k * k) / (2.0f * kneeWidth);
        }
        else gainReductionDb = ((1.0f / ratio) - 1.0f) * overshoot;
        const float targetEnv = -gainReductionDb;

        // 3. balistica optica: ataque suave + relajacion en dos etapas
        if (targetEnv > envelope)
        {
            envelope = attackCoeff * envelope + (1.0f - attackCoeff) * targetEnv;
            releaseFastEnv = releaseSlowEnv = envelope;                  // las dos etapas parten del mismo punto
        }
        else
        {
            releaseFastEnv = releaseFastCoeff * releaseFastEnv + (1.0f - releaseFastCoeff) * targetEnv;
            releaseSlowEnv = releaseSlowCoeff * releaseSlowEnv + (1.0f - releaseSlowCoeff) * targetEnv;
            envelope = std::max (releaseFastEnv, releaseSlowEnv * 0.65f);
        }
        return envelope;
    }

    // 5 y 6. valvula (asimetrica: 2do y 3er armonico) + transformador de salida
    float colour (int channel, float x) noexcept
    {
        return transformerFilter[(size_t) (channel & 1)].process (applyTubeSaturation (x, drive));
    }
    float getReductionDb() const noexcept { return envelope; }

private:
    static float applyTubeSaturation (float x, float driveAmount) noexcept
    {
        if (driveAmount <= 0.01f) return x;
        const float k = 1.0f + driveAmount * 2.5f;
        const float bias = 0.05f * driveAmount;                            // asimetria sutil = 2do armonico (triodo)
        const float t0 = std::tanh (bias);
        // compensacion exacta: en senal baja la ganancia es 1 (la valvula colorea sin subir el volumen)
        return (std::tanh (k * x + bias) - t0) / (k * (1.0f - t0 * t0));
    }

    double sampleRate = 44100.0;
    float thresholdDb = -15.0f, ratio = 4.0f, drive = 0.2f;
    float attackCoeff = 0.0f, releaseFastCoeff = 0.0f, releaseSlowCoeff = 0.0f, rmsCoeff = 0.0f, peakCoeff = 0.0f;
    float envelope = 0.0f, releaseFastEnv = 0.0f, releaseSlowEnv = 0.0f, rms = 0.0f, peak = 0.0f;
    std::array<tvc::Biquad, 2> transformerFilter;
};
