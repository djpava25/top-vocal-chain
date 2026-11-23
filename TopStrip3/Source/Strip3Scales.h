#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <cmath>

// Escalas del panel: valor -> angulo de la perilla (grados, 0 = arriba), medidas en la serigrafia del original.
// Los parametros usan exactamente estas curvas, asi el indicador apunta al numero impreso.
namespace s737cscale
{
struct Pt { float v, a; };
using Table = std::vector<Pt>;

inline float angleOf (const Table& t, float v)
{
    if (v <= t.front().v) return t.front().a;
    for (size_t i = 1; i < t.size(); ++i)
        if (v <= t[i].v) { const float k = (v - t[i - 1].v) / (t[i].v - t[i - 1].v); return t[i - 1].a + k * (t[i].a - t[i - 1].a); }
    return t.back().a;
}
inline float valueOf (const Table& t, float a)
{
    if (a <= t.front().a) return t.front().v;
    for (size_t i = 1; i < t.size(); ++i)
        if (a <= t[i].a) { const float k = (a - t[i - 1].a) / (t[i].a - t[i - 1].a); return t[i - 1].v + k * (t[i].v - t[i - 1].v); }
    return t.back().v;
}
inline Table logTable (float lo, float hi, float a0, float a1)
{
    Table t;
    for (int i = 0; i <= 32; ++i) { const float k = (float) i / 32.0f; t.push_back ({ lo * std::pow (hi / lo, k), a0 + k * (a1 - a0) }); }
    return t;
}
// escala estandar del panel (todas las perillas de ganancia): numeros a +-14, 39, 65, 92, 125 grados
inline Table sym (std::initializer_list<float> right, std::initializer_list<float> left, std::initializer_list<float> angles)
{
    Table t; std::vector<float> r (right), l (left), a (angles);
    for (size_t i = l.size(); i-- > 0;) t.push_back ({ -l[i], -a[i] });
    t.push_back ({ 0, 0 });
    for (size_t i = 0; i < r.size(); ++i) t.push_back ({ r[i], a[i] });
    return t;
}
inline const Table& preGain() { static const Table t { { -28, -140 }, { -20, -92 }, { -14, -65 }, { -8, -39 }, { -4, -14 }, { 0, 0 }, { 2, 14 }, { 4, 39 }, { 6, 65 }, { 7.5f, 92 }, { 9, 140 } }; return t; }
inline const Table& thr()   { static const Table t = sym ({ 2, 5, 10, 15, 20 }, { 5, 10, 15, 20, 30 }, { 14, 39, 65, 92, 125 }); return t; }
inline const Table& ratio() { static const Table t { { 1, -125 }, { 2, -92 }, { 3, -65 }, { 4, -39 }, { 5, -14 }, { 6, 14 }, { 7, 39 }, { 8, 65 }, { 10, 92 }, { 20, 125 } }; return t; }
inline const Table& out()   { static const Table t = sym ({ 2, 4, 6, 8, 10 }, { 2, 4, 6, 8, 40 }, { 14, 39, 65, 92, 125 }); return t; }
inline const Table& bass()  { static const Table t = sym ({ 2, 4, 6, 12, 24 }, { 2, 4, 6, 12, 24 }, { 14, 39, 65, 92, 125 }); return t; }
inline const Table& mids()  { static const Table t = sym ({ 2, 4, 6, 10, 16 }, { 2, 4, 6, 10, 16 }, { 14, 39, 65, 92, 125 }); return t; }
inline const Table& treble(){ static const Table t = sym ({ 2, 4, 6, 12, 20 }, { 2, 4, 6, 12, 20 }, { 14, 39, 65, 92, 125 }); return t; }
inline const Table& hpf()   { static const Table t { { 30, -137.5f }, { 35, -110 }, { 40, -79 }, { 45, -52 }, { 55, -27 }, { 60, 0 }, { 70, 27 }, { 80, 52 }, { 100, 79 }, { 120, 110 }, { 140, 137.5f } }; return t; }
inline const Table& attack()  { static const Table t = logTable (2.0f, 200.0f, -128.0f, 128.0f); return t; }
inline const Table& release() { static const Table t = logTable (100.0f, 5000.0f, -128.0f, 128.0f); return t; }

// rango normalizado proporcional al angulo de la perilla
inline juce::NormalisableRange<float> range (const Table& tab, float interval)
{
    const Table t = tab;
    const float a0 = t.front().a, a1 = t.back().a;
    auto from0 = [t, a0, a1] (float, float, float n) { return valueOf (t, a0 + n * (a1 - a0)); };
    auto to0 = [t, a0, a1] (float, float, float v) { return (angleOf (t, v) - a0) / (a1 - a0); };
    auto snap = [interval] (float s, float e, float v) { if (interval > 0) v = s + interval * std::round ((v - s) / interval); return juce::jlimit (s, e, v); };
    return { t.front().v, t.back().v, from0, to0, snap };
}
} // namespace s737cscale
