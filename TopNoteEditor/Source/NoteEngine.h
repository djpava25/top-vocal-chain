#pragma once
#include <vector>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <map>
#include <algorithm>

// Motor de TOP NOTE EDITOR: analiza una toma, la divide en notas ("blobs") y vuelve a
// generar el audio con las ediciones de cada nota (tono, vibrato, deriva, formantes, volumen,
// posicion y duracion).
namespace ned
{
// Curva de analisis: un valor cada 'hop' muestras
struct Curve
{
    double sr = 44100.0;
    int hop = 220;
    std::vector<float> midi;     // tono (MIDI con decimales), 0 = sin tono
    std::vector<float> smooth;   // tono suavizado (centro + deriva, sin vibrato)
    std::vector<float> db;       // nivel RMS en dB
    float midiAt (double sample) const;
    float smoothAt (double sample) const;
    float dbAt (double sample) const;
    int frames() const { return (int) midi.size(); }
};

struct Note
{
    int s0 = 0, s1 = 0;          // rango original (muestras)
    int t0 = 0, t1 = 0;          // rango editado (posicion y duracion)
    float center = 60.0f;        // tono detectado (centro)
    bool pitched = true;         // false = consonante / ruido / percusion
    bool visible = true;         // false = silencio de relleno
    float shift = 0.0f;          // semitonos (con centesimas)
    float mod = 1.0f;            // escala del vibrato (0..2)
    float drift = 1.0f;          // escala de la deriva (0..2)
    float formant = 0.0f;        // semitonos de formante
    float gainDb = 0.0f;
    bool mute = false;
    float levelDb = -100.0f;     // nivel medido (para nivelar)

    float target() const { return center + shift; }
    bool sameEdits (const Note& o) const
    {
        return s0 == o.s0 && s1 == o.s1 && t0 == o.t0 && t1 == o.t1 && shift == o.shift && mod == o.mod
            && drift == o.drift && formant == o.formant && gainDb == o.gainDb && mute == o.mute;
    }
};

enum Algorithm { Melodic = 0, Percussive = 1 };

// Analiza el tono de toda la toma (YIN, ventana 30 ms, salto 5 ms)
Curve analyse (const float* x, int n, double sr, std::atomic<float>* progress = nullptr, const std::atomic<bool>* cancel = nullptr);
// Divide en notas
std::vector<Note> segment (const Curve&, int n, int algorithm);
// Recalcula centro y nivel de una nota desde la curva
void measure (Note&, const Curve&);

// Render con cache por nota
class Renderer
{
public:
    // devuelve el audio editado completo (longitud >= n)
    std::vector<float> render (const float* src, int n, const Curve&, const std::vector<Note>&,
                               const std::atomic<bool>* cancel = nullptr, std::atomic<float>* progress = nullptr);
    void clearCache() { cache.clear(); islandCache.clear(); }
    static constexpr int X = 128;   // solape de cruce entre notas
private:
    // Las notas con cambios de tono se procesan juntas en "islas" continuas (sin empalmes entre notas),
    // con transiciones de tono suaves; en los bordes la isla vuelve a coincidir con el original.
    std::vector<float> renderIsland (const float* src, int n, const Curve&, const std::vector<Note>&, int a, int b);
    std::map<uint64_t, std::vector<float>> cache, islandCache;
};

std::vector<float> wsola (const std::vector<float>& in, int outLen, double sr);
} // namespace ned
