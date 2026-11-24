#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <array>
#include "DSP/Biquad.h"

// Copia para TOP LOCKER del motor anterior de TOP MIC COLLECTION: curvas de respuesta, patrones, efecto de proximidad, fuera de eje y saturacion.
// Los nombres son propios; cada modelo se inspira en el caracter sonoro general de un clasico de estudio.
namespace tmic
{
enum BandType { Peak, LowShelf, HighShelf, HighPass, LowPass };
struct Band { int type; float f, q, g; };
enum Pattern { Omni = 0, Wide, Cardioid, Hyper, Figure8, NumPatterns };
enum Body { TubeLdc = 0, FetLdc, GoldTube, StageDyn, BroadcastDyn, LongDyn, RibbonModern, RibbonVintage, RibbonBbc };

struct Model
{
    const char* name;        // nombre en el visor
    const char* desc;        // descripcion
    int body;                // dibujo
    std::vector<Band> eq;    // curva propia
    unsigned patterns;       // bits de patrones disponibles
    int defPattern;
    float proximity;         // fuerza del efecto de proximidad
    float size;              // tamano del diafragma (perdida de agudos fuera de eje)
    float tube, trans;       // saturacion de valvula / transformador
};

inline const std::vector<Model>& models()
{
    const unsigned ALL = 31u, OC = (1u << Omni) | (1u << Cardioid), C = 1u << Cardioid, F8 = 1u << Figure8;
    static const std::vector<Model> m {
        { "TM-V1", "Válvula vintage: cuerpo cálido y medios gruesos (estilo 47 de los 50)", TubeLdc,
          { { LowShelf, 120, 0.7f, 1.5f }, { Peak, 250, 1.0f, 1.5f }, { Peak, 3000, 1.2f, -1.0f }, { Peak, 5500, 1.5f, 2.0f }, { HighShelf, 12000, 0.7f, -3.0f }, { LowPass, 18500, 0.7f, 0 } },
          OC, Cardioid, 1.0f, 1.0f, 0.28f, 0.10f },
        { "TM-V2", "Válvula suave: medios sedosos y brillo aireado (estilo 67)", TubeLdc,
          { { Peak, 200, 1.0f, 1.0f }, { Peak, 2500, 1.2f, -1.5f }, { Peak, 10000, 1.0f, 2.5f }, { HighShelf, 15000, 0.7f, -1.0f } },
          ALL, Cardioid, 1.0f, 1.0f, 0.22f, 0.08f },
        { "TM-F1", "FET de estudio: equilibrado, presencia clara (estilo 87)", FetLdc,
          { { HighPass, 30, 0.7f, 0 }, { Peak, 120, 1.0f, 0.5f }, { Peak, 4500, 1.5f, 1.0f }, { Peak, 9000, 1.2f, 3.0f }, { HighShelf, 15000, 0.7f, -1.5f } },
          (1u << Omni) | (1u << Cardioid) | (1u << Figure8), Cardioid, 1.0f, 0.9f, 0.0f, 0.10f },
        { "TM-V3", "Válvula aérea: agudos abiertos y detallados (estilo C12)", TubeLdc,
          { { Peak, 300, 1.0f, -1.0f }, { Peak, 3500, 1.3f, 1.5f }, { HighShelf, 9000, 0.7f, 4.0f }, { Peak, 13000, 1.5f, 1.5f } },
          ALL, Cardioid, 0.9f, 1.0f, 0.22f, 0.05f },
        { "TM-V4", "Válvula brillante: presencia moderna y aire (estilo 251)", TubeLdc,
          { { LowShelf, 100, 0.7f, -1.0f }, { Peak, 1500, 1.0f, -1.0f }, { Peak, 5000, 1.2f, 2.5f }, { HighShelf, 10000, 0.7f, 3.0f } },
          (1u << Omni) | (1u << Cardioid) | (1u << Figure8), Cardioid, 0.95f, 1.0f, 0.24f, 0.06f },
        { "TM-V5", "Válvula hip-hop: aire brillante y graves controlados (estilo 800)", GoldTube,
          { { LowShelf, 150, 0.7f, -2.0f }, { Peak, 400, 1.0f, -1.5f }, { Peak, 7000, 1.2f, 3.5f }, { HighShelf, 10000, 0.7f, 5.5f } },
          C, Cardioid, 0.8f, 0.9f, 0.16f, 0.0f },
        { "TM-F2", "FET brillante: definición y detalle en agudos (estilo 414)", FetLdc,
          { { Peak, 100, 1.0f, -1.0f }, { Peak, 3000, 1.2f, 1.0f }, { Peak, 12000, 1.0f, 4.0f }, { HighShelf, 16000, 0.7f, 1.0f } },
          ALL, Cardioid, 0.9f, 0.85f, 0.0f, 0.04f },
        { "TM-F3", "FET moderno: presencia fuerte y bajo ruido (estilo 103)", FetLdc,
          { { LowShelf, 120, 0.7f, 0.5f }, { Peak, 8000, 1.0f, 4.0f }, { Peak, 12000, 1.5f, 1.5f } },
          C, Cardioid, 1.0f, 0.85f, 0.0f, 0.03f },
        { "TM-D1", "Dinámico de escenario: medios con pegada, presencia 5 kHz (estilo 57)", StageDyn,
          { { HighPass, 110, 0.6f, 0 }, { Peak, 200, 0.8f, -1.0f }, { Peak, 3000, 1.0f, 2.0f }, { Peak, 5500, 1.4f, 5.0f }, { HighShelf, 9000, 0.7f, -3.0f }, { LowPass, 13500, 0.7f, 0 } },
          C, Cardioid, 1.3f, 0.6f, 0.0f, 0.12f },
        { "TM-D2", "Dinámico de radio: cálido, suave y cercano (estilo SM7)", BroadcastDyn,
          { { HighPass, 55, 0.7f, 0 }, { Peak, 200, 0.8f, 1.0f }, { Peak, 4000, 1.0f, 2.5f }, { HighShelf, 8000, 0.7f, -2.0f }, { LowPass, 16000, 0.7f, 0 } },
          C, Cardioid, 0.8f, 0.7f, 0.0f, 0.08f },
        { "TM-D3", "Dinámico de locución: poco efecto de proximidad, claro (estilo RE20)", LongDyn,
          { { HighPass, 45, 0.7f, 0 }, { Peak, 1000, 0.8f, 0.5f }, { Peak, 6000, 1.0f, 1.5f }, { HighShelf, 10000, 0.7f, -3.0f } },
          C, Cardioid, 0.3f, 0.7f, 0.0f, 0.06f },
        { "TM-R1", "Cinta moderna: agudos suaves, medios naturales (estilo 121)", RibbonModern,
          { { LowShelf, 150, 0.7f, 1.0f }, { Peak, 1000, 0.6f, 1.0f }, { HighShelf, 6000, 0.6f, -5.0f }, { LowPass, 15000, 0.6f, 0 } },
          F8, Figure8, 1.5f, 1.0f, 0.0f, 0.15f },
        { "TM-R2", "Cinta vintage: grave redondo y oscuro, voz de radio antigua (estilo 44)", RibbonVintage,
          { { LowShelf, 200, 0.7f, 3.0f }, { Peak, 800, 0.7f, 1.0f }, { HighShelf, 4000, 0.6f, -7.0f }, { LowPass, 11000, 0.7f, 0 } },
          F8, Figure8, 1.8f, 1.2f, 0.0f, 0.20f },
        { "TM-R3", "Cinta británica: suave y musical (estilo 4038)", RibbonBbc,
          { { LowShelf, 120, 0.7f, 1.5f }, { Peak, 2500, 1.0f, -1.0f }, { HighShelf, 8000, 0.6f, -6.0f }, { LowPass, 16000, 0.7f, 0 } },
          F8, Figure8, 1.4f, 1.0f, 0.0f, 0.12f },
        // se agrega al final para no cambiar los numeros de los modelos en sesiones guardadas (en el menu aparece con las valvulas)
        { "TM-V6", "Válvula de referencia: graves grandes, presencia al frente y aire (estilo Manley Reference)", TubeLdc,
          { { LowShelf, 100, 0.7f, 1.5f }, { Peak, 250, 1.0f, -1.0f }, { Peak, 3000, 1.2f, 2.5f }, { Peak, 7000, 1.5f, 2.0f }, { HighShelf, 10000, 0.7f, 3.5f } },
          C, Cardioid, 1.0f, 1.0f, 0.30f, 0.08f } };
    return m;
}

// microfonos con los que se puede grabar (su color se quita antes de aplicar el modelo)
struct Source { const char* name; const char* shortName; std::vector<Band> curve; };
inline const std::vector<Source>& sources()
{
    static const std::vector<Source> s {
        { "Plano / neutro", "FLAT", {} },
        { "Dinámico de escenario (tipo 57/58)", "DYN", { { HighPass, 110, 0.6f, 0 }, { Peak, 3000, 1.0f, 2.0f }, { Peak, 5500, 1.4f, 5.0f }, { HighShelf, 9000, 0.7f, -3.0f } } },
        { "Dinámico de radio (tipo SM7)", "BCST", { { HighPass, 55, 0.7f, 0 }, { Peak, 200, 0.8f, 1.0f }, { Peak, 4000, 1.0f, 2.5f }, { HighShelf, 8000, 0.7f, -2.0f } } },
        { "Condensador de estudio económico", "LDC", { { LowShelf, 100, 0.7f, 0.5f }, { Peak, 9000, 1.0f, 3.0f }, { HighShelf, 14000, 0.7f, -1.0f } } },
        { "Micrófono USB", "USB", { { LowShelf, 150, 0.7f, -2.0f }, { Peak, 4000, 1.0f, 2.0f }, { HighShelf, 10000, 0.7f, 2.0f } } },
        { "Celular / laptop", "PHONE", { { HighPass, 180, 0.7f, 0 }, { Peak, 2500, 1.0f, 3.0f }, { LowPass, 9000, 0.7f, 0 } } },
        { "Condensador de diafragma pequeño", "SDC", { { LowShelf, 80, 0.7f, -1.0f }, { Peak, 10000, 1.0f, 2.0f } } },
        // micros del usuario (al final para no cambiar sesiones guardadas)
        { "Neumann TLM 103", "TLM103", { { LowShelf, 120, 0.7f, 0.5f }, { Peak, 8000, 1.0f, 4.0f }, { Peak, 12000, 1.5f, 1.5f } } },
        { "AKG C414 XLII", "C414", { { Peak, 100, 1.0f, -1.0f }, { Peak, 3000, 1.2f, 1.5f }, { Peak, 8500, 1.0f, 4.0f }, { HighShelf, 14000, 0.7f, 1.5f } } } };
    return s;
}

inline const char* patternName (int p) { static const char* n[] { "OMNI", "WIDE", "CARDIOID", "HYPER", "FIGURE-8" }; return n[std::clamp (p, 0, 4)]; }
// coeficiente del patron polar: g(t) = a + (1-a) cos t
inline float patternA (int p) { static const float a[] { 1.0f, 0.7f, 0.5f, 0.25f, 0.0f }; return a[std::clamp (p, 0, 4)]; }
inline float proximityOf (int p) { static const float f[] { 0.0f, 0.45f, 1.0f, 1.15f, 1.4f }; return f[std::clamp (p, 0, 4)]; }
inline int nearestPattern (const Model& m, int want)
{
    for (int d = 0; d < 5; ++d)
    {
        if (want - d >= 0 && (m.patterns >> (want - d) & 1u)) return want - d;
        if (want + d < 5 && (m.patterns >> (want + d) & 1u)) return want + d;
    }
    return m.defPattern;
}

inline void setBand (tvc::Biquad& b, double sr, const Band& band, float gainScale = 1.0f)
{
    const double ny = sr * 0.45;
    switch (band.type)
    {
        case Peak: b.setPeak (sr, std::min ((double) band.f, ny), band.q, band.g * gainScale); break;
        case LowShelf: b.setLowShelf (sr, band.f, band.g * gainScale); break;
        case HighShelf: b.setHighShelf (sr, std::min ((double) band.f, ny), band.g * gainScale); break;
        case HighPass: b.setHighPass (sr, band.f, band.q); break;
        case LowPass: if (band.f >= ny) b.setIdentity(); else b.setLowPass (sr, band.f, band.q); break;
    }
}
// inversa aproximada de una banda (para quitar el color del micro de origen)
inline Band inverse (const Band& b)
{
    if (b.type == HighPass) return { LowShelf, b.f * 1.2f, 0.7f, 6.0f };   // no se puede recuperar todo: refuerzo limitado
    if (b.type == LowPass) return { HighShelf, b.f * 0.8f, 0.7f, 6.0f };
    return { b.type, b.f, b.q, -b.g };
}

struct Settings
{
    int model = 0, source = 0, pattern = Cardioid, filter = 0;
    float axis = 0, prox = 0;   // grados, -100..100
    bool rev = false;
};

// cadena de un microfono
class MicChain
{
public:
    void prepare (double sampleRate) { sr = sampleRate; last = {}; last.model = -1; reset(); }
    void reset() { for (auto& c : ch) { for (auto& b : c.bq) b.reset(); c.dc.reset(); } }

    // nivel del patron en el angulo (para la grafica y el nivel fuera de eje)
    static float polar (int pattern, float deg)
    {
        const float a = patternA (pattern);
        return a + (1.0f - a) * std::cos (deg * 3.14159265f / 180.0f);
    }

    void configure (const Settings& s)
    {
        if (s.model == last.model && s.source == last.source && s.pattern == last.pattern && s.filter == last.filter
            && std::abs (s.axis - last.axis) < 0.5f && std::abs (s.prox - last.prox) < 0.5f && s.rev == last.rev) return;
        last = s;
        const auto& m = models()[(size_t) std::clamp (s.model, 0, (int) models().size() - 1)];
        const int pat = nearestPattern (m, s.pattern);
        std::vector<Band> bands;
        for (const auto& b : sources()[(size_t) std::clamp (s.source, 0, (int) sources().size() - 1)].curve) bands.push_back (inverse (b));
        for (const auto& b : m.eq) bands.push_back (b);
        // patron: omni un poco mas abierto arriba, figura 8 mas suave
        if (pat == Omni) bands.push_back ({ HighShelf, 10000, 0.7f, 1.0f });
        if (pat == Figure8 && m.body != RibbonModern && m.body != RibbonVintage && m.body != RibbonBbc) bands.push_back ({ HighShelf, 8000, 0.7f, -1.0f });
        // efecto de proximidad
        const float prox = s.prox / 100.0f * proximityOf (pat) * m.proximity * 9.0f;
        if (std::abs (prox) > 0.05f) { bands.push_back ({ LowShelf, 160, 0.7f, prox }); bands.push_back ({ Peak, 350, 0.8f, prox * 0.25f }); }
        // fuera de eje (REV = el microfono mirando al reves)
        float ang = s.rev ? 180.0f - s.axis : s.axis;
        const float toneAng = pat == Figure8 ? std::min (ang, 180.0f - ang) : ang;
        const float off = (1.0f - std::cos (toneAng * 3.14159265f / 180.0f)) * 0.5f;     // 0 de frente .. 1 de espaldas
        if (off > 0.001f) { bands.push_back ({ HighShelf, 4000, 0.6f, -9.0f * m.size * off }); bands.push_back ({ Peak, 2000, 0.8f, -2.0f * m.size * off }); }
        const float pg = polar (pat, ang);
        level = std::pow (10.0f, std::max (-15.0f, 20.0f * std::log10 (std::max (1.0e-4f, std::abs (pg)))) * 0.6f / 20.0f);
        polarity = pg < 0.0f ? -1.0f : 1.0f;
        // filtro del microfono
        if (s.filter == 1) bands.push_back ({ HighPass, 60, 0.6f, 0 });
        if (s.filter == 2) bands.push_back ({ HighPass, 100, 0.7f, 0 });
        if (s.filter == 3) bands.push_back ({ HighPass, 160, 0.75f, 0 });
        nb = std::min ((int) bands.size(), maxBands);
        for (auto& c : ch)
            for (int i = 0; i < maxBands; ++i)
            {
                const float z1 = c.bq[(size_t) i].z1, z2 = c.bq[(size_t) i].z2;
                if (i < nb) setBand (c.bq[(size_t) i], sr, bands[(size_t) i]); else c.bq[(size_t) i].setIdentity();
                c.bq[(size_t) i].z1 = z1; c.bq[(size_t) i].z2 = z2;       // sin clics al cambiar
            }
        tube = m.tube; trans = m.trans;
        for (auto& c : ch) c.dc.setHighPass1 (sr, 10.0);
    }

    inline float process (int c, float x) noexcept
    {
        auto& s = ch[(size_t) c];
        for (int i = 0; i < nb; ++i) x = s.bq[(size_t) i].process (x);
        if (tube > 0.0f) { const float k = 1.0f + tube * 2.0f; x = std::tanh (x * k) / k + tube * 0.12f * x * x; }   // valvula: armonicos pares
        if (trans > 0.0f) { const float k = 1.0f + trans * 3.0f; x = std::tanh (x * k) / k; }                       // transformador
        if (tube > 0.0f) x = s.dc.process (x);
        return x * level * polarity;
    }

private:
    static constexpr int maxBands = 20;
    struct Ch { std::array<tvc::Biquad, maxBands> bq; tvc::Biquad dc; };
    std::array<Ch, 2> ch;
    double sr = 48000.0;
    int nb = 0;
    float tube = 0, trans = 0, level = 1, polarity = 1;
    Settings last;
};
} // namespace tmic
