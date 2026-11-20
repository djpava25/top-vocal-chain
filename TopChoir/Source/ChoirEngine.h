#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "DSP/PitchCorrector.h"
#include "DSP/Psola.h"
#include "DSP/Effects.h"

// MOTOR DEL CORO (offline): toma una voz, detecta su tono nota a nota y crea varias voces por parte
// (unisono, octavas y armonias de 3ª y 5ª dentro de la tonalidad), cada una con su afinacion, tiempo,
// formante y posicion estereo, como un coro real. Despues: microfono (EQ) y sala (reverb).
namespace choir
{
struct Settings
{
    int model = 0, mic = 0, key = 9, scale = 1;
    bool octUp = false, octDown = false, thirdUp = false, thirdDown = false, fifthUp = false, fifthDown = false;
    bool stereo = true, includeUnison = true;
};

struct ModelInfo { const char* name; int voices; float detuneCents, timingMs, vibrato, formantSpread, reverb, decay; int reverbType; };
const std::vector<ModelInfo>& models();
const juce::StringArray& micNames();
juce::StringArray modelNames();

// Devuelve el audio del coro (estereo). progress: 0..1. Si cancel pasa a true, se detiene y devuelve vacio.
juce::AudioBuffer<float> render (const juce::AudioBuffer<float>& voice, double sampleRate, const Settings& s,
                                 std::atomic<float>& progress, const std::atomic<bool>& cancel);

// Detecta la tonalidad de la voz (raiz 0..11 y si es menor)
bool detectKey (const juce::AudioBuffer<float>& voice, double sampleRate, int& root, bool& minor);
}
