#pragma once
#include <juce_dsp/juce_dsp.h>
#include "MicModels.h"
#include <complex>

// TOP LOCKER: coleccion virtual de microfonos.
// Cada microfono tiene su propia respuesta, saturacion/armonicos, transientes, compresion natural,
// proximidad, patron y cuanta sala capta. Los nombres son propios; cada uno se inspira en el caracter
// general de un clasico de estudio (se indica en la descripcion).
namespace lk
{
using tmic::Band;
constexpr int Pk = tmic::Peak, LS = tmic::LowShelf, HS = tmic::HighShelf, HP = tmic::HighPass, LP = tmic::LowPass;
enum Sat { SatFet = 0, SatTube, SatClean, SatDyn, SatRibbon };
enum Cat { Classic = 1, Vintage = 2, Modern = 4, Ribbon = 8, Dynamic = 16, Broadcast = 32 };

struct Mic
{
    const char* line1; const char* line2;   // nombre en la tarjeta
    const char* type;                        // tipo corto
    const char* ref;                         // microfono al que imita (solo como referencia)
    const char* desc;                        // descripcion (en que se inspira)
    unsigned cats;
    std::vector<Band> eq;
    int pattern; float prox, size;
    int sat; float drive, even;              // saturacion propia
    float transient, comp;                   // + mas ataque (condensador rapido)  - mas suave (bobina/cinta)
    float roomPick;                          // cuanta sala capta
};

inline const std::vector<Mic>& mics()
{
    using tmic::Cardioid; using tmic::Figure8;
    static const std::vector<Mic> m {
        { "Studio", "Silver", "FET LDC", "tipo U87 Ai", "FET de estudio equilibrado con presencia clara arriba (inspirado en el U87 Ai)", Classic,
          { { HP, 30, 0.7f, 0 }, { Pk, 120, 1.0f, 0.5f }, { Pk, 4500, 1.5f, 1.0f }, { Pk, 9000, 1.2f, 3.0f }, { HS, 15000, 0.7f, -1.5f } },
          Cardioid, 1.0f, 0.9f, SatFet, 0.18f, 0.35f, 0.12f, 0.04f, 1.0f },
        { "Modern", "Silver", "LDC SIN TRAFO", "tipo TLM 103", "Condensador moderno: presencia fuerte en 8-12 kHz y transientes rapidos (inspirado en el TLM 103)", Modern,
          { { LS, 120, 0.7f, 0.5f }, { Pk, 8000, 1.0f, 4.0f }, { Pk, 12000, 1.5f, 1.5f } },
          Cardioid, 1.0f, 0.85f, SatClean, 0.06f, 0.2f, 0.25f, 0.0f, 1.0f },
        { "Gold", "Detail", "LDC MULTIPATRON", "tipo C414 XLII", "Detalle y definicion arriba, medios limpios (inspirado en el C414 XLII)", Modern | Classic,
          { { Pk, 100, 1.0f, -1.0f }, { Pk, 3000, 1.2f, 1.5f }, { Pk, 8500, 1.0f, 4.0f }, { HS, 14000, 0.7f, 1.5f } },
          Cardioid, 0.9f, 0.85f, SatClean, 0.07f, 0.25f, 0.30f, 0.0f, 1.05f },
        { "Crown", "Tube", "VALVULA", "tipo C-800G", "Valvula hip-hop/R&B: aire brillante, graves controlados (inspirado en el C-800G)", Modern | Classic,
          { { LS, 150, 0.7f, -2.0f }, { Pk, 400, 1.0f, -1.5f }, { Pk, 7000, 1.2f, 3.5f }, { HS, 10000, 0.7f, 5.0f } },
          Cardioid, 0.8f, 0.9f, SatTube, 0.22f, 0.7f, 0.10f, 0.08f, 0.95f },
        { "Classic", "Tube", "VALVULA", "tipo ELA M 251", "Valvula brillante y sedosa con medios abiertos (inspirado en el ELA M 251)", Vintage | Classic,
          { { LS, 100, 0.7f, -1.0f }, { Pk, 1500, 1.0f, -1.0f }, { Pk, 5000, 1.2f, 2.5f }, { HS, 10000, 0.7f, 3.0f } },
          Cardioid, 0.95f, 1.0f, SatTube, 0.30f, 0.6f, 0.05f, 0.14f, 1.0f },
        { "Black", "Reference", "VALVULA XLR", "tipo Manley Ref.", "Valvula de referencia: graves grandes, voz al frente y aire (inspirado en el Manley Reference)", Modern,
          { { LS, 100, 0.7f, 1.5f }, { Pk, 250, 1.0f, -1.0f }, { Pk, 3000, 1.2f, 2.5f }, { Pk, 7000, 1.5f, 2.0f }, { HS, 10000, 0.7f, 3.5f } },
          Cardioid, 1.0f, 1.0f, SatTube, 0.30f, 0.65f, 0.08f, 0.12f, 1.0f },
        { "Broadcast", "Black", "DINAMICO", "tipo SM7B", "Dinamico de radio/voz: calido, suave, poco cuarto (inspirado en el SM7B)", Dynamic | Broadcast,
          { { HP, 50, 0.7f, 0 }, { Pk, 200, 0.8f, 1.0f }, { Pk, 4000, 1.0f, 1.5f }, { Pk, 6500, 1.5f, 1.0f }, { HS, 8000, 0.7f, -3.0f }, { LP, 16000, 0.7f, 0 } },
          Cardioid, 0.8f, 0.7f, SatDyn, 0.10f, 0.15f, -0.35f, 0.22f, 0.6f },
        { "Ribbon", "Modern", "CINTA", "tipo R-121", "Cinta moderna: agudos suaves, medios naturales, figura 8 (inspirado en el R-121)", Ribbon,
          { { LS, 150, 0.7f, 1.0f }, { Pk, 1000, 0.6f, 1.0f }, { HS, 6000, 0.6f, -5.0f }, { LP, 15000, 0.6f, 0 } },
          Figure8, 1.5f, 1.0f, SatRibbon, 0.22f, 0.2f, -0.25f, 0.10f, 1.5f },
        { "Ribbon", "Classic", "CINTA", "tipo 4038", "Cinta britanica: suave, redonda y musical (inspirado en el Coles 4038)", Ribbon | Vintage | Broadcast,
          { { LS, 120, 0.7f, 1.5f }, { Pk, 2500, 1.0f, -1.0f }, { HS, 7000, 0.6f, -6.0f }, { LP, 16000, 0.7f, 0 } },
          Figure8, 1.4f, 1.0f, SatRibbon, 0.20f, 0.2f, -0.30f, 0.10f, 1.5f },
        { "Air", "Tube", "VALVULA", "tipo C12", "Valvula aerea: agudos abiertos y detallados (inspirado en el C12)", Vintage | Classic,
          { { Pk, 300, 1.0f, -1.0f }, { Pk, 3500, 1.3f, 1.5f }, { HS, 9000, 0.7f, 4.0f }, { Pk, 13000, 1.5f, 1.5f } },
          Cardioid, 0.9f, 1.0f, SatTube, 0.26f, 0.6f, 0.05f, 0.10f, 1.0f },
        { "Velvet", "Tube", "VALVULA", "tipo M49", "Valvula vintage: cuerpo grande, medios gruesos y agudos de seda (inspirado en el M49)", Vintage | Classic,
          { { LS, 120, 0.7f, 1.5f }, { Pk, 300, 1.0f, 1.0f }, { Pk, 3000, 1.2f, -1.0f }, { Pk, 6000, 1.5f, 2.0f }, { HS, 12000, 0.7f, -2.5f }, { LP, 18500, 0.7f, 0 } },
          Cardioid, 1.0f, 1.0f, SatTube, 0.35f, 0.75f, 0.0f, 0.18f, 1.0f },
        { "Voice", "Line", "DINAMICO", "tipo RE20", "Dinamico de locucion: claro, casi sin efecto de proximidad (inspirado en el RE20)", Dynamic | Broadcast,
          { { HP, 45, 0.7f, 0 }, { Pk, 1000, 0.8f, 0.5f }, { Pk, 6000, 1.0f, 1.5f }, { HS, 10000, 0.7f, -3.0f } },
          Cardioid, 0.3f, 0.7f, SatDyn, 0.08f, 0.15f, -0.30f, 0.16f, 0.6f } };
    return m;
}

struct Pre { const char* name; const char* style; const char* desc; std::vector<Band> eq; };
// el orden se conserva para sesiones guardadas: los nuevos van al final
inline const std::vector<Pre>& preamps()
{
    static const std::vector<Pre> p {
        { "CLEAN", "sin preamp", "Sin color: pasa directo", {} },
        { "N-73", "Neve-style", "Transformador clase A: graves gordos, medios densos y armonicos pares/impares", { { Pk, 80, 0.7f, 0.8f }, { Pk, 1600, 0.5f, 0.4f }, { HS, 16000, 0.7f, -0.8f } } },
        { "A-12", "API-style", "Op-amp discreto: medios al frente, ataque marcado y recorte con pegada", { { Pk, 120, 0.8f, 0.6f }, { Pk, 2000, 0.7f, 0.6f } } },
        { "S-4K", "SSL-style", "Consola: limpio, rapido, un poco de brillo; satura solo cuando lo empujas", { { HS, 12000, 0.7f, 0.6f } } },
        { "T-76", "Tube-style", "Valvula: calido, suave, armonicos pares y un poco de 'sag' (compresion de fuente)", { { Pk, 200, 0.8f, 0.8f }, { HS, 12000, 0.7f, -1.0f } } },
        { "M-70", "Modern clean", "Moderno transparente: ultra limpio, graves firmes y aire sutil", { { HS, 18000, 0.7f, 0.4f } } },
        { "V-610", "Vintage tube", "Valvula vintage: medios-graves gruesos, agudos redondos, mucha saturacion par y compresion", { { Pk, 120, 0.8f, 1.2f }, { Pk, 350, 0.9f, 1.0f }, { HS, 9000, 0.7f, -2.0f }, { LP, 17000, 0.7f, 0 } } } };
    return p;
}

struct Room { const char* name; float rt, pre, er; int taps; float damp, width, boxF, boxG, level; };
inline const std::vector<Room>& rooms()
{
    static const std::vector<Room> r {
        { "Seco (sin sala)", 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { "Vocal Booth", 0.22f, 1.0f, 12.0f, 14, 0.5f, 0.4f, 250.0f, 1.0f, 0.9f },
        { "Studio A", 0.55f, 4.0f, 30.0f, 18, 0.4f, 0.7f, 0, 0, 1.0f },
        { "Live Room", 1.0f, 8.0f, 45.0f, 22, 0.25f, 0.9f, 0, 0, 1.0f },
        { "Hall", 2.0f, 22.0f, 80.0f, 24, 0.35f, 1.0f, 0, 0, 1.0f },
        { "Habitacion vintage", 0.7f, 3.0f, 25.0f, 16, 0.7f, 0.6f, 300.0f, 3.0f, 1.0f },
        { "Studio B", 0.4f, 3.0f, 18.0f, 16, 0.55f, 0.6f, 180.0f, 1.0f, 1.0f },
        { "Plate", 1.8f, 0.0f, 0.0f, 0, 0.12f, 1.0f, 0, 0, 1.0f },
        { "Chamber", 1.3f, 6.0f, 20.0f, 30, 0.45f, 0.8f, 1000.0f, 2.0f, 1.0f } };
    return r;
}
// controles de la sala (0..100, 50 = como viene la sala)
struct RoomCtl
{
    float size = 50, early = 50, decay = 50, width = 50, damp = 50;
    bool operator== (const RoomCtl& o) const { return size == o.size && early == o.early && decay == o.decay && width == o.width && damp == o.damp; }
};

// respuesta al impulso sintetica de una sala (estereo)
inline juce::AudioBuffer<float> makeRoom (int index, double sr, const RoomCtl& ctl = {})
{
    const auto& rm = rooms()[(size_t) juce::jlimit (1, (int) rooms().size() - 1, index)];
    const bool plate = rm.taps == 0;
    const float sz = std::pow (2.0f, (ctl.size - 50.0f) / 50.0f);                 // 0.5x .. 2x
    const float rt = rm.rt * std::pow (2.0f, (ctl.decay - 50.0f) / 50.0f) * std::pow (1.4f, (ctl.size - 50.0f) / 50.0f);
    const float preMs = rm.pre * sz, erMs = rm.er * sz;
    const float erGain = std::pow (ctl.early / 50.0f, 1.5f);
    const float damp = rm.damp * std::pow (2.0f, (ctl.damp - 50.0f) / 25.0f);
    const float width = ctl.width < 50.0f ? rm.width * ctl.width / 50.0f : rm.width + (1.0f - rm.width) * (ctl.width - 50.0f) / 50.0f;
    const int len = (int) (sr * (preMs / 1000.0 + rt * 1.3 + 0.05));
    juce::AudioBuffer<float> ir (2, len); ir.clear();
    for (int c = 0; c < 2; ++c)
    {
        juce::Random rnd (1234 + index * 17 + c * 101);
        auto* d = ir.getWritePointer (c);
        const int pre = (int) (preMs * 0.001 * sr), erLen = std::max (1, (int) (erMs * 0.001 * sr));
        // primeras reflexiones
        for (int i = 0; i < rm.taps; ++i)
        {
            const float u = (float) (i + 1) / (float) rm.taps;
            const int t = pre + (int) ((float) erLen * std::pow (u, 1.4f) * (0.85f + 0.3f * rnd.nextFloat()));
            if (t + 2 >= len) continue;
            const float a = 0.9f * (1.0f - 0.6f * u) * (rnd.nextBool() ? 1.0f : -1.0f) * erGain;
            d[t] += a * 0.6f; d[t + 1] += a * 0.3f; d[t + 2] += a * 0.1f;
        }
        // cola difusa que se oscurece con el tiempo (la placa es densa desde el inicio y brillante)
        float lp = 0.0f;
        const int fadeLen = plate ? (int) (0.004 * sr) : erLen;
        for (int i = pre; i < len; ++i)
        {
            const float t = (float) (i - pre) / (float) sr;
            const float fade = juce::jlimit (0.0f, 1.0f, (float) (i - pre) / (float) std::max (1, fadeLen));
            const float env = std::exp (-6.91f * t / rt) * fade * fade;
            const float fc = juce::jmax (500.0f, (plate ? 16000.0f : 14000.0f) * std::exp (-t / rt * damp * 4.0f));
            const float a = std::exp (-2.0f * juce::MathConstants<float>::pi * fc / (float) sr);
            lp = a * lp + (1.0f - a) * (rnd.nextFloat() * 2.0f - 1.0f);
            d[i] += lp * env * 0.35f;
        }
    }
    {
        auto* L = ir.getWritePointer (0); auto* R = ir.getWritePointer (1);
        for (int i = 0; i < len; ++i) R[i] = width * R[i] + (1.0f - width) * L[i];
    }
    for (int c = 0; c < 2; ++c)
    {
        tvc::Biquad hp, box; hp.setHighPass (sr, plate ? 120.0 : (rm.boxF == 1000.0f ? 150.0 : 90.0), 0.7);
        if (rm.boxG != 0.0f) box.setPeak (sr, rm.boxF, 1.2, rm.boxG); else box.setIdentity();
        auto* d = ir.getWritePointer (c);
        double e = 0;
        for (int i = 0; i < len; ++i) { d[i] = box.process (hp.process (d[i])); e += (double) d[i] * d[i]; }
        const float k = rm.level / (float) std::sqrt (e + 1e-12);
        for (int i = 0; i < len; ++i) d[i] *= k;
    }
    return ir;
}

// saturacion con ganancia 1 en senal pequena; b crea armonicos pares
inline float asym (float x, float k, float b) noexcept
{
    const float t0 = std::tanh (k * b), d = k * (1.0f - t0 * t0);
    return (std::tanh (k * (x + b)) - t0) / d;
}
inline float knee (float x, float k, float p) noexcept   // recorte mas duro (op-amp)
{
    const float y = k * x;
    return x / std::pow (1.0f + std::pow (std::abs (y), p), 1.0f / p);
}

enum CharMode { ChModern = 0, ChVintage, ChSmooth, ChClean, ChTube, ChTransformer, ChAnalog };
inline const char* charName (int m) { static const char* n[] { "Modern", "Vintage", "Smooth", "Clean", "Tube", "Transformer", "Analog" }; return n[juce::jlimit (0, 6, m)]; }
inline const float* lowFreqs() { static const float f[] { 60, 100, 200, 300 }; return f; }
inline const float* midFreqs() { static const float f[] { 400, 800, 1200, 2000, 3000, 5000 }; return f; }
inline const float* highFreqs() { static const float f[] { 5000, 8000, 10000, 12000, 16000 }; return f; }

struct Params
{
    int mic = 0, source = 0, pre = 1, charMode = 0, lowFreq = 1, lowType = 0, midFreq = 2, highFreq = 2, highType = 0;
    float angle = 0, prox = 50, low = 0, mid = 0, high = 0, character = 50, sat = 15, preGain = 3.5f, preInput = 0;
    bool operator== (const Params& o) const
    {
        return mic == o.mic && source == o.source && pre == o.pre && charMode == o.charMode && lowFreq == o.lowFreq && lowType == o.lowType
            && midFreq == o.midFreq && highFreq == o.highFreq && highType == o.highType && angle == o.angle && prox == o.prox
            && low == o.low && mid == o.mid && high == o.high && character == o.character && sat == o.sat && preGain == o.preGain && preInput == o.preInput;
    }
};

inline int clampMic (int m) { return juce::jlimit (0, (int) mics().size() - 1, m); }
inline float patternRoom (int p) { static const float f[] { 1.5f, 1.25f, 1.0f, 0.85f, 1.3f }; return f[juce::jlimit (0, 4, p)]; }
// distancia aproximada que representa la proximidad (cm)
inline float distanceCm (float prox) { return 2.0f * std::pow (30.0f, prox / 100.0f); }

// curva del tono (Low / Mid / High)
inline std::vector<Band> toneBands (const Params& p)
{
    std::vector<Band> B;
    const float lf = lowFreqs()[juce::jlimit (0, 3, p.lowFreq)], mf = midFreqs()[juce::jlimit (0, 5, p.midFreq)], hf = highFreqs()[juce::jlimit (0, 4, p.highFreq)];
    if (p.low != 0) B.push_back (p.lowType == 0 ? Band { LS, lf, 0.7f, p.low } : Band { Pk, lf, 0.9f, p.low });
    if (p.mid != 0) B.push_back ({ Pk, mf, 0.8f, p.mid });
    if (p.high != 0) B.push_back (p.highType == 0 ? Band { HS, hf, 0.7f, p.high } : Band { Pk, hf, 0.9f, p.high });
    return B;
}

// bandas antes (A) y despues (B) de la saturacion del microfono
inline void buildBands (const Params& p, std::vector<Band>& A, std::vector<Band>& B, float* levelOut = nullptr, float* roomOut = nullptr)
{
    A.clear(); B.clear();
    const auto& m = mics()[(size_t) clampMic (p.mic)];
    const int pat = m.pattern;
    for (const auto& b : tmic::sources()[(size_t) juce::jlimit (0, (int) tmic::sources().size() - 1, p.source)].curve) A.push_back (tmic::inverse (b));
    for (const auto& b : m.eq) A.push_back (b);
    // angulo respecto a la boca: fuera de eje se pierden agudos y nivel
    const float ang = juce::jlimit (0.0f, 90.0f, p.angle);
    const float off = (1.0f - std::cos (ang * 3.14159265f / 180.0f));      // 0 en eje .. 1 a 90
    if (off > 0.001f) { A.push_back ({ HS, 4000, 0.6f, -6.0f * m.size * off }); A.push_back ({ Pk, 2000, 0.8f, -1.5f * m.size * off }); }
    if (levelOut != nullptr)
    {
        const float pg = tmic::MicChain::polar (pat, ang);
        *levelOut = std::pow (10.0f, std::max (-15.0f, 20.0f * std::log10 (std::max (1.0e-4f, std::abs (pg)))) * 0.6f / 20.0f);
    }
    // proximidad: cerca = mas graves y menos sala, lejos = menos graves, menos aire y mas sala
    const float d = (50.0f - p.prox) / 50.0f;
    const float pf = tmic::proximityOf (pat) * m.prox;
    if (d > 0) { A.push_back ({ LS, 160, 0.7f, d * 9.0f * pf }); A.push_back ({ Pk, 350, 0.8f, d * 2.2f * pf }); }
    else if (d < 0) { A.push_back ({ LS, 160, 0.7f, d * 3.0f * pf }); A.push_back ({ HS, 8000, 0.7f, d * 2.0f }); }
    if (roomOut != nullptr) *roomOut = m.roomPick * patternRoom (pat) * (0.35f + 0.65f * (1.0f - d));

    const auto& pr = preamps()[(size_t) juce::jlimit (0, (int) preamps().size() - 1, p.pre)];
    const float preAmt = 0.4f + 0.6f * p.preGain / 10.0f;
    for (const auto& b : pr.eq) B.push_back ({ b.type, b.f, b.q, b.type == LP ? 0.0f : b.g * preAmt });
    const float c = p.character / 100.0f;
    switch (p.charMode)
    {
        case ChModern: B.push_back ({ HS, 12000, 0.7f, 0.8f * c }); break;
        case ChVintage: B.push_back ({ Pk, 250, 0.8f, 1.5f * c }); B.push_back ({ HS, 9000, 0.7f, -3.0f * c }); if (c > 0.05f) B.push_back ({ LP, 20000.0f - 6000.0f * c, 0.7f, 0 }); break;
        case ChSmooth: B.push_back ({ Pk, 3500, 1.2f, -2.0f * c }); B.push_back ({ Pk, 7000, 2.0f, -1.5f * c }); break;
        case ChTube: B.push_back ({ Pk, 200, 0.8f, 1.2f * c }); B.push_back ({ HS, 12000, 0.7f, -1.2f * c }); break;
        case ChTransformer: B.push_back ({ Pk, 60, 1.0f, 1.0f * c }); if (c > 0.05f) B.push_back ({ LP, 20000.0f - 3000.0f * c, 0.7f, 0 }); break;
        case ChAnalog: B.push_back ({ Pk, 100, 0.7f, 0.5f * c }); B.push_back ({ HS, 15000, 0.7f, -0.6f * c }); break;
        default: break;
    }
    for (const auto& b : toneBands (p)) B.push_back (b);
}

// respuesta (dB) de una lista de bandas en una frecuencia
inline float bandsDb (const std::vector<Band>& bands, double sr, double f)
{
    double mag = 1.0;
    const double w = 2.0 * 3.14159265358979 * f / sr;
    const std::complex<double> z1 = std::polar (1.0, -w), z2 = std::polar (1.0, -2.0 * w);
    for (const auto& b : bands)
    {
        tvc::Biquad q; tmic::setBand (q, sr, b);
        const auto num = (double) q.b0 + (double) q.b1 * z1 + (double) q.b2 * z2, den = 1.0 + (double) q.a1 * z1 + (double) q.a2 * z2;
        mag *= std::abs (num / den);
    }
    return (float) (20.0 * std::log10 (mag + 1e-12));
}

// cadena completa de un microfono (corre a la frecuencia sobremuestreada)
class Engine
{
public:
    void prepare (double osRate) { sr = osRate; configured = false; reset(); }
    void reset()
    {
        for (auto& c : ch)
        {
            for (auto& b : c.a) b.reset(); for (auto& b : c.b) b.reset();
            c.lo.reset(); c.lo2.reset(); c.lo3.reset(); c.dc.reset();
            c.fast = c.slow = c.env = c.pf = c.ps = c.sag = 0; c.gr = c.tg = c.ptg = 1;
        }
    }
    float roomFactor() const { return roomPick; }

    void configure (const Params& p)
    {
        if (configured && p == last) return;
        configured = true; last = p;
        const auto& m = mics()[(size_t) clampMic (p.mic)];
        std::vector<Band> A, B;
        buildBands (p, A, B, &level, &roomPick);
        na = std::min ((int) A.size(), maxA); nb = std::min ((int) B.size(), maxB);
        for (auto& s : ch)
        {
            setAll (s.a.data(), maxA, A, na);
            setAll (s.b.data(), maxB, B, nb);
            s.lo.setLowPass1 (sr, 220.0); s.lo2.setLowPass1 (sr, 160.0); s.lo3.setLowPass1 (sr, 120.0); s.dc.setHighPass1 (sr, 7.0);
        }
        const float s01 = p.sat / 100.0f, c = p.character / 100.0f;
        satType = m.sat; even = m.even;
        charMode = p.charMode;
        micDrive = p.charMode == ChClean ? 0.0f : m.drive * c * 2.0f * (0.3f + 1.7f * s01);
        charDrive = (p.charMode >= ChTube) ? c * (0.25f + 1.0f * s01) : 0.0f;
        preType = p.pre;
        preDrive = p.pre == 0 ? 0.0f : (p.preGain / 10.0f) * (0.4f + s01 * 1.2f);
        inGain = p.pre == 0 ? 1.0f : juce::Decibels::decibelsToGain (p.preInput);
        transient = m.transient;
        preTransient = p.pre == 2 ? 0.15f + 0.25f * p.preGain / 10.0f : 0.0f;     // A-12: pegada
        sagAmt = p.pre == 4 ? 0.6f * p.preGain / 10.0f : p.pre == 6 ? 1.2f * p.preGain / 10.0f : 0.0f;   // T-76 / V-610
        ratio = 1.0f + m.comp * 2.5f;
        const auto co = [this] (float ms) { return std::exp (-1.0f / (float) (sr * ms * 0.001)); };
        aFastR = co (80.0f); aSlowA = co (12.0f); aTgA = co (0.3f); aTgR = co (3.0f);
        aEnvA = co (4.0f); aEnvR = co (90.0f); aGr = co (2.0f); aSag = co (60.0f);
    }

    inline float shaper (float x, float amount, float& fast, float& slow, float& g) const noexcept
    {
        // envolvente de picos rapida; la lenta la sigue con retraso solo al subir: en sonidos estables valen igual
        const float ax = std::abs (x);
        fast = ax > fast ? ax : aFastR * fast + (1 - aFastR) * ax;
        slow = fast > slow ? aSlowA * slow + (1 - aSlowA) * fast : fast;
        if (slow > 1.0e-6f)
        {
            const float tg = juce::jlimit (0.55f, 1.8f, 1.0f + amount * (fast / slow - 1.0f));
            g = tg > g ? aTgA * g + (1 - aTgA) * tg : aTgR * g + (1 - aTgR) * tg;
        }
        return x * g;
    }

    inline float process (int c, float x) noexcept
    {
        auto& s = ch[(size_t) c];
        for (int i = 0; i < na; ++i) x = s.a[(size_t) i].process (x);
        x *= level;
        // transientes: condensadores rapidos acentuan el ataque, dinamicos y cintas lo redondean
        if (transient != 0.0f) x = shaper (x, transient, s.fast, s.slow, s.tg);
        // compresion natural del microfono (valvula, bobina, cinta) por encima de -14 dBFS
        if (ratio > 1.001f)
        {
            const float e2 = x * x;
            s.env = e2 > s.env ? aEnvA * s.env + (1 - aEnvA) * e2 : aEnvR * s.env + (1 - aEnvR) * e2;
            const float lv = 10.0f * std::log10 (s.env + 1.0e-12f), over = std::max (0.0f, lv + 14.0f);
            const float tg = std::pow (10.0f, -over * (1.0f - 1.0f / ratio) / 20.0f);
            s.gr = aGr * s.gr + (1 - aGr) * tg;
            x *= s.gr;
        }
        // armonicos del microfono
        if (micDrive > 0.0005f)
        {
            switch (satType)
            {
                case SatTube: x = asym (x, 1.0f + 4.0f * micDrive, 0.35f * even); break;
                case SatFet: { const float lo = s.lo.process (x); x = x - lo + asym (lo, 1.0f + 9.0f * micDrive, 0.1f * even); x = asym (x, 1.0f + 3.0f * micDrive, 0.15f * even); break; }
                case SatClean: x = asym (x, 1.0f + 2.0f * micDrive, 0.2f * even); break;
                case SatDyn: x = asym (x, 1.0f + 4.0f * micDrive, 0.0f); break;      // bobina: armonicos impares
                case SatRibbon: { const float lo = s.lo.process (x); x = x - lo + asym (lo, 1.0f + 10.0f * micDrive, 0.04f); break; }
                default: break;
            }
        }
        // preamp (con nivel de entrada real: Input empuja la saturacion, la salida se compensa)
        if (preType != 0)
        {
            x *= inGain;
            const float d = preDrive;
            if (sagAmt > 0.0f)     // valvula: la fuente "cede" con picos fuertes
            {
                const float ax = std::abs (x);
                s.sag = ax > s.sag ? 0.3f * s.sag + 0.7f * ax : aSag * s.sag + (1 - aSag) * ax;
                x *= 1.0f / (1.0f + sagAmt * std::max (0.0f, s.sag - 0.25f));
            }
            switch (preType)
            {
                case 1: { const float lo = s.lo2.process (x); x = x - lo + asym (lo, 1.0f + 8.0f * d, 0.12f); x = asym (x, 1.0f + 5.0f * d, 0.25f); break; }
                case 2: x = knee (x + 0.03f * d * x * x, 1.0f + 6.0f * d, 3.0f); break;
                case 3: x = knee (x, 1.0f + 3.0f * d, 4.0f); break;
                case 4: x = asym (x, 1.0f + 6.0f * d, 0.4f); break;
                case 5: x = knee (x, 1.0f + 1.2f * d, 6.0f); break;
                case 6: { const float lo = s.lo2.process (x); x = x - lo + asym (lo, 1.0f + 6.0f * d, 0.2f); x = asym (x, 1.0f + 8.0f * d, 0.55f); break; }
                default: break;
            }
            if (preTransient > 0.0f) x = shaper (x, preTransient, s.pf, s.ps, s.ptg);
            x /= inGain;
        }
        for (int i = 0; i < nb; ++i) x = s.b[(size_t) i].process (x);
        // circuito de coloracion (Character)
        if (charDrive > 0.0005f)
        {
            const float k = charDrive;
            switch (charMode)
            {
                case ChTube: x = asym (x, 1.0f + 4.0f * k, 0.4f); break;
                case ChTransformer: { const float lo = s.lo3.process (x); x = x - lo + asym (lo, 1.0f + 3.5f * k, 0.0f); x = asym (x, 1.0f + 1.5f * k, 0.0f); break; }
                case ChAnalog: x = asym (x, 1.0f + 2.5f * k, 0.12f); break;
                default: break;
            }
        }
        if (micDrive > 0.0005f || preDrive > 0.0005f || charDrive > 0.0005f) x = s.dc.process (x);
        return x;
    }

private:
    void setAll (tvc::Biquad* bq, int count, const std::vector<Band>& bands, int n) const
    {
        for (int i = 0; i < count; ++i)
        {
            const float z1 = bq[i].z1, z2 = bq[i].z2;      // sin clics al cambiar
            if (i < n) tmic::setBand (bq[i], sr, bands[(size_t) i]); else bq[i].setIdentity();
            bq[i].z1 = z1; bq[i].z2 = z2;
        }
    }
    static constexpr int maxA = 20, maxB = 14;
    struct Ch { std::array<tvc::Biquad, maxA> a; std::array<tvc::Biquad, maxB> b; tvc::Biquad lo, lo2, lo3, dc; float fast = 0, slow = 0, tg = 1, env = 0, gr = 1, pf = 0, ps = 0, ptg = 1, sag = 0; };
    std::array<Ch, 2> ch;
    double sr = 96000.0;
    bool configured = false;
    Params last;
    int na = 0, nb = 0, satType = 0, preType = 0, charMode = 0;
    float level = 1, roomPick = 1, even = 0, micDrive = 0, preDrive = 0, charDrive = 0, inGain = 1, transient = 0, preTransient = 0, sagAmt = 0, ratio = 1;
    float aFastR = 0, aSlowA = 0, aTgA = 0, aTgR = 0, aEnvA = 0, aEnvR = 0, aGr = 0, aSag = 0;
};
} // namespace lk
