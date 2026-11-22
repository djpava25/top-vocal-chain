#pragma once
#include <juce_dsp/juce_dsp.h>
#include "MicModels.h"

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
        { "Studio", "Silver", "FET LDC", "FET de estudio equilibrado con presencia clara arriba (inspirado en el U87 Ai)", Classic,
          { { HP, 30, 0.7f, 0 }, { Pk, 120, 1.0f, 0.5f }, { Pk, 4500, 1.5f, 1.0f }, { Pk, 9000, 1.2f, 3.0f }, { HS, 15000, 0.7f, -1.5f } },
          Cardioid, 1.0f, 0.9f, SatFet, 0.18f, 0.35f, 0.12f, 0.04f, 1.0f },
        { "Modern", "Silver", "LDC SIN TRAFO", "Condensador moderno: presencia fuerte en 8-12 kHz y transientes rapidos (inspirado en el TLM 103)", Modern,
          { { LS, 120, 0.7f, 0.5f }, { Pk, 8000, 1.0f, 4.0f }, { Pk, 12000, 1.5f, 1.5f } },
          Cardioid, 1.0f, 0.85f, SatClean, 0.06f, 0.2f, 0.25f, 0.0f, 1.0f },
        { "Gold", "Detail", "LDC MULTIPATRON", "Detalle y definicion arriba, medios limpios (inspirado en el C414 XLII)", Modern | Classic,
          { { Pk, 100, 1.0f, -1.0f }, { Pk, 3000, 1.2f, 1.5f }, { Pk, 8500, 1.0f, 4.0f }, { HS, 14000, 0.7f, 1.5f } },
          Cardioid, 0.9f, 0.85f, SatClean, 0.07f, 0.25f, 0.30f, 0.0f, 1.05f },
        { "Crown", "Tube", "VALVULA", "Valvula hip-hop/R&B: aire brillante, graves controlados (inspirado en el C-800G)", Modern | Classic,
          { { LS, 150, 0.7f, -2.0f }, { Pk, 400, 1.0f, -1.5f }, { Pk, 7000, 1.2f, 3.5f }, { HS, 10000, 0.7f, 5.0f } },
          Cardioid, 0.8f, 0.9f, SatTube, 0.22f, 0.7f, 0.10f, 0.08f, 0.95f },
        { "Classic", "Tube", "VALVULA", "Valvula brillante y sedosa con medios abiertos (inspirado en el ELA M 251)", Vintage | Classic,
          { { LS, 100, 0.7f, -1.0f }, { Pk, 1500, 1.0f, -1.0f }, { Pk, 5000, 1.2f, 2.5f }, { HS, 10000, 0.7f, 3.0f } },
          Cardioid, 0.95f, 1.0f, SatTube, 0.30f, 0.6f, 0.05f, 0.14f, 1.0f },
        { "Black", "Reference", "VALVULA XLR", "Valvula de referencia: graves grandes, voz al frente y aire (inspirado en el Manley Reference)", Modern,
          { { LS, 100, 0.7f, 1.5f }, { Pk, 250, 1.0f, -1.0f }, { Pk, 3000, 1.2f, 2.5f }, { Pk, 7000, 1.5f, 2.0f }, { HS, 10000, 0.7f, 3.5f } },
          Cardioid, 1.0f, 1.0f, SatTube, 0.30f, 0.65f, 0.08f, 0.12f, 1.0f },
        { "Broadcast", "Black", "DINAMICO", "Dinamico de radio/voz: calido, suave, poco cuarto (inspirado en el SM7B)", Dynamic | Broadcast,
          { { HP, 50, 0.7f, 0 }, { Pk, 200, 0.8f, 1.0f }, { Pk, 4000, 1.0f, 1.5f }, { Pk, 6500, 1.5f, 1.0f }, { HS, 8000, 0.7f, -3.0f }, { LP, 16000, 0.7f, 0 } },
          Cardioid, 0.8f, 0.7f, SatDyn, 0.10f, 0.15f, -0.35f, 0.22f, 0.6f },
        { "Ribbon", "Modern", "CINTA", "Cinta moderna: agudos suaves, medios naturales, figura 8 (inspirado en el R-121)", Ribbon,
          { { LS, 150, 0.7f, 1.0f }, { Pk, 1000, 0.6f, 1.0f }, { HS, 6000, 0.6f, -5.0f }, { LP, 15000, 0.6f, 0 } },
          Figure8, 1.5f, 1.0f, SatRibbon, 0.22f, 0.2f, -0.25f, 0.10f, 1.5f },
        { "Ribbon", "Classic", "CINTA", "Cinta britanica: suave, redonda y musical (inspirado en el Coles 4038)", Ribbon | Vintage | Broadcast,
          { { LS, 120, 0.7f, 1.5f }, { Pk, 2500, 1.0f, -1.0f }, { HS, 7000, 0.6f, -6.0f }, { LP, 16000, 0.7f, 0 } },
          Figure8, 1.4f, 1.0f, SatRibbon, 0.20f, 0.2f, -0.30f, 0.10f, 1.5f },
        { "Air", "Tube", "VALVULA", "Valvula aerea: agudos abiertos y detallados (inspirado en el C12)", Vintage | Classic,
          { { Pk, 300, 1.0f, -1.0f }, { Pk, 3500, 1.3f, 1.5f }, { HS, 9000, 0.7f, 4.0f }, { Pk, 13000, 1.5f, 1.5f } },
          Cardioid, 0.9f, 1.0f, SatTube, 0.26f, 0.6f, 0.05f, 0.10f, 1.0f },
        { "Velvet", "Tube", "VALVULA", "Valvula vintage: cuerpo grande, medios gruesos y agudos de seda (inspirado en el M49)", Vintage | Classic,
          { { LS, 120, 0.7f, 1.5f }, { Pk, 300, 1.0f, 1.0f }, { Pk, 3000, 1.2f, -1.0f }, { Pk, 6000, 1.5f, 2.0f }, { HS, 12000, 0.7f, -2.5f }, { LP, 18500, 0.7f, 0 } },
          Cardioid, 1.0f, 1.0f, SatTube, 0.35f, 0.75f, 0.0f, 0.18f, 1.0f },
        { "Voice", "Line", "DINAMICO", "Dinamico de locucion: claro, casi sin efecto de proximidad (inspirado en el RE20)", Dynamic | Broadcast,
          { { HP, 45, 0.7f, 0 }, { Pk, 1000, 0.8f, 0.5f }, { Pk, 6000, 1.0f, 1.5f }, { HS, 10000, 0.7f, -3.0f } },
          Cardioid, 0.3f, 0.7f, SatDyn, 0.08f, 0.15f, -0.30f, 0.16f, 0.6f } };
    return m;
}

struct Pre { const char* name; const char* desc; std::vector<Band> eq; };
inline const std::vector<Pre>& preamps()
{
    static const std::vector<Pre> p {
        { "CLEAN", "Limpio: sin color", {} },
        { "N-73", "Preamp con transformador: graves gordos y medios densos (inspirado en el Neve 1073)", { { Pk, 80, 0.7f, 0.8f }, { Pk, 1600, 0.5f, 0.4f }, { HS, 16000, 0.7f, -0.8f } } },
        { "A-312", "Preamp con pegada: medios al frente y ataque (inspirado en el API 312)", { { Pk, 120, 0.8f, 0.6f }, { Pk, 2000, 0.7f, 0.6f } } },
        { "S-4K", "Preamp de consola: limpio, rapido y un poco de brillo (inspirado en el SSL 4000)", { { HS, 12000, 0.7f, 0.6f } } },
        { "T-12", "Preamp a valvula: calido, suave, armonicos pares", { { Pk, 200, 0.8f, 0.8f }, { HS, 12000, 0.7f, -1.0f } } } };
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
        { "Sala grande", 2.0f, 22.0f, 80.0f, 24, 0.35f, 1.0f, 0, 0, 1.0f },
        { "Habitacion vintage", 0.7f, 3.0f, 25.0f, 16, 0.7f, 0.6f, 300.0f, 3.0f, 1.0f } };
    return r;
}

// respuesta al impulso sintetica de una sala (estereo)
inline juce::AudioBuffer<float> makeRoom (int index, double sr)
{
    const auto& rm = rooms()[(size_t) juce::jlimit (1, (int) rooms().size() - 1, index)];
    const int len = (int) (sr * (rm.pre / 1000.0 + rm.rt * 1.3 + 0.05));
    juce::AudioBuffer<float> ir (2, len); ir.clear();
    for (int c = 0; c < 2; ++c)
    {
        juce::Random rnd (1234 + index * 17 + c * 101);
        auto* d = ir.getWritePointer (c);
        const int pre = (int) (rm.pre * 0.001 * sr), erLen = (int) (rm.er * 0.001 * sr);
        // primeras reflexiones
        for (int i = 0; i < rm.taps; ++i)
        {
            const float u = (float) (i + 1) / (float) rm.taps;
            const int t = pre + (int) (erLen * std::pow (u, 1.4f) * (0.85f + 0.3f * rnd.nextFloat()));
            if (t + 2 >= len) continue;
            const float a = 0.9f * (1.0f - 0.6f * u) * (rnd.nextBool() ? 1.0f : -1.0f);
            d[t] += a * 0.6f; d[t + 1] += a * 0.3f; d[t + 2] += a * 0.1f;
        }
        // cola difusa que se oscurece con el tiempo
        float lp = 0.0f;
        for (int i = pre; i < len; ++i)
        {
            const float t = (float) (i - pre) / (float) sr;
            const float fade = juce::jlimit (0.0f, 1.0f, (float) (i - pre) / (float) juce::jmax (1, erLen));
            const float env = std::exp (-6.91f * t / rm.rt) * fade * fade;
            const float fc = juce::jmax (500.0f, 14000.0f * std::exp (-t / rm.rt * rm.damp * 4.0f));
            const float a = std::exp (-2.0f * juce::MathConstants<float>::pi * fc / (float) sr);
            lp = a * lp + (1.0f - a) * (rnd.nextFloat() * 2.0f - 1.0f);
            d[i] += lp * env * 0.35f;
        }
    }
    // ancho estereo
    {
        auto* L = ir.getWritePointer (0); auto* R = ir.getWritePointer (1);
        for (int i = 0; i < len; ++i) R[i] = rm.width * R[i] + (1.0f - rm.width) * L[i];
    }
    // cuerpo de la sala y limpieza de graves
    for (int c = 0; c < 2; ++c)
    {
        tvc::Biquad hp, box; hp.setHighPass (sr, 90.0, 0.7);
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

struct Params
{
    int mic = 0, source = 0, pre = 1, charMode = 0;
    float position = 50, prox = 50, low = 0, mid = 0, high = 0, character = 50, sat = 15, preGain = 3.5f;
    bool operator== (const Params& o) const
    {
        return mic == o.mic && source == o.source && pre == o.pre && charMode == o.charMode && position == o.position && prox == o.prox
            && low == o.low && mid == o.mid && high == o.high && character == o.character && sat == o.sat && preGain == o.preGain;
    }
};

inline int clampMic (int m) { return juce::jlimit (0, (int) mics().size() - 1, m); }
inline float patternRoom (int p) { static const float f[] { 1.5f, 1.25f, 1.0f, 0.85f, 1.3f }; return f[juce::jlimit (0, 4, p)]; }

// cadena completa de un microfono (corre a la frecuencia sobremuestreada)
class Engine
{
public:
    void prepare (double osRate) { sr = osRate; configured = false; reset(); }
    void reset()
    {
        for (auto& c : ch) { for (auto& b : c.a) b.reset(); for (auto& b : c.b) b.reset(); c.lo.reset(); c.lo2.reset(); c.dc.reset(); c.fast = c.slow = c.env = 0; c.gr = c.tg = 1; }
    }
    // cuanta sala capta el microfono con estos ajustes
    float roomFactor() const { return roomPick; }

    void configure (const Params& p)
    {
        if (configured && p == last) return;
        configured = true; last = p;
        const auto& m = mics()[(size_t) clampMic (p.mic)];
        const int pat = m.pattern;
        std::vector<Band> A, B;
        for (const auto& b : tmic::sources()[(size_t) juce::jlimit (0, (int) tmic::sources().size() - 1, p.source)].curve) A.push_back (tmic::inverse (b));
        for (const auto& b : m.eq) A.push_back (b);
        // posicion: hacia "Front" el capsula apunta directo a la boca (mas presencia), hacia "Back" se sale del eje
        float ang = 0;
        if (p.position < 50.0f)
        {
            const float f = (50.0f - p.position) / 50.0f;
            A.push_back ({ Pk, 5500, 1.0f, 2.5f * f }); A.push_back ({ HS, 11000, 0.7f, 1.5f * f }); A.push_back ({ LS, 150, 0.7f, 1.0f * f });
        }
        else
        {
            ang = (p.position - 50.0f) / 50.0f * 180.0f;
            const float toneAng = pat == tmic::Figure8 ? std::min (ang, 180.0f - ang) : ang;
            const float off = (1.0f - std::cos (toneAng * 3.14159265f / 180.0f)) * 0.5f;
            if (off > 0.001f) { A.push_back ({ HS, 4000, 0.6f, -9.0f * m.size * off }); A.push_back ({ Pk, 2000, 0.8f, -2.0f * m.size * off }); }
        }
        const float pg = tmic::MicChain::polar (pat, ang);
        level = std::pow (10.0f, std::max (-15.0f, 20.0f * std::log10 (std::max (1.0e-4f, std::abs (pg)))) * 0.6f / 20.0f) * (pg < 0 ? -1.0f : 1.0f);
        // proximidad: cerca = mas graves y menos sala, lejos = menos graves, menos aire y mas sala
        const float d = (50.0f - p.prox) / 50.0f;
        const float pf = tmic::proximityOf (pat) * m.prox;
        if (d > 0) { A.push_back ({ LS, 160, 0.7f, d * 9.0f * pf }); A.push_back ({ Pk, 350, 0.8f, d * 2.2f * pf }); }
        else if (d < 0) { A.push_back ({ LS, 160, 0.7f, d * 3.0f * pf }); A.push_back ({ HS, 8000, 0.7f, d * 2.0f }); }
        roomPick = m.roomPick * patternRoom (pat) * (0.35f + 0.65f * (1.0f - d));

        // despues de la saturacion: preamp, caracter y tono
        const auto& pr = preamps()[(size_t) juce::jlimit (0, (int) preamps().size() - 1, p.pre)];
        const float preAmt = 0.4f + 0.6f * p.preGain / 10.0f;
        for (const auto& b : pr.eq) B.push_back ({ b.type, b.f, b.q, b.g * preAmt });
        const float c = p.character / 100.0f;
        if (p.charMode == 0) B.push_back ({ HS, 12000, 0.7f, 0.8f * c });
        if (p.charMode == 1) { B.push_back ({ Pk, 250, 0.8f, 1.5f * c }); B.push_back ({ HS, 9000, 0.7f, -3.0f * c }); if (c > 0.05f) B.push_back ({ LP, 20000.0f - 6000.0f * c, 0.7f, 0 }); }
        if (p.charMode == 2) { B.push_back ({ Pk, 3500, 1.2f, -2.0f * c }); B.push_back ({ Pk, 7000, 2.0f, -1.5f * c }); }
        if (p.low != 0) B.push_back ({ LS, 100, 0.7f, p.low });
        if (p.mid != 0) B.push_back ({ Pk, 1200, 0.8f, p.mid });
        if (p.high != 0) B.push_back ({ HS, 10000, 0.7f, p.high });

        na = std::min ((int) A.size(), maxA); nb = std::min ((int) B.size(), maxB);
        for (auto& s : ch)
        {
            setAll (s.a.data(), maxA, A, na);
            setAll (s.b.data(), maxB, B, nb);
            s.lo.setLowPass1 (sr, 220.0); s.lo2.setLowPass1 (sr, 160.0); s.dc.setHighPass1 (sr, 7.0);
        }
        // armonicos y dinamica
        const float s01 = p.sat / 100.0f;
        satType = m.sat; even = m.even;
        micDrive = m.drive * c * 2.0f * (0.3f + 1.7f * s01);
        preType = p.pre; preDrive = p.pre == 0 ? 0.0f : (p.preGain / 10.0f) * (0.4f + s01 * 1.2f);
        transient = m.transient;
        ratio = 1.0f + m.comp * 2.5f;
        const auto co = [this] (float ms) { return std::exp (-1.0f / (float) (sr * ms * 0.001)); };
        aFastR = co (80.0f); aSlowA = co (12.0f); aTgA = co (0.3f); aTgR = co (3.0f);
        aEnvA = co (4.0f); aEnvR = co (90.0f); aGr = co (2.0f);
    }

    inline float process (int c, float x) noexcept
    {
        auto& s = ch[(size_t) c];
        for (int i = 0; i < na; ++i) x = s.a[(size_t) i].process (x);
        x *= level;
        // transientes: condensadores rapidos acentuan el ataque, dinamicos y cintas lo redondean
        // (envolvente de picos rapida; la lenta la sigue con retraso solo al subir: en sonidos estables valen igual)
        const float ax = std::abs (x);
        s.fast = ax > s.fast ? ax : aFastR * s.fast + (1 - aFastR) * ax;
        s.slow = s.fast > s.slow ? aSlowA * s.slow + (1 - aSlowA) * s.fast : s.fast;
        if (transient != 0.0f && s.slow > 1.0e-6f)
        {
            const float tg = juce::jlimit (0.55f, 1.8f, 1.0f + transient * (s.fast / s.slow - 1.0f));
            s.tg = tg > s.tg ? aTgA * s.tg + (1 - aTgA) * tg : aTgR * s.tg + (1 - aTgR) * tg;
            x *= s.tg;
        }
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
            const float k = 1.0f + 6.0f * micDrive;
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
        for (int i = 0; i < nb; ++i) x = s.b[(size_t) i].process (x);
        // preamp
        if (preDrive > 0.0005f)
        {
            const float d = preDrive;
            switch (preType)
            {
                case 1: { const float lo = s.lo2.process (x); x = x - lo + asym (lo, 1.0f + 8.0f * d, 0.12f); x = asym (x, 1.0f + 5.0f * d, 0.25f); break; }
                case 2: x = knee (x + 0.03f * d * x * x, 1.0f + 6.0f * d, 3.0f); break;
                case 3: x = knee (x, 1.0f + 3.0f * d, 4.0f); break;
                case 4: x = asym (x, 1.0f + 6.0f * d, 0.4f); break;
                default: break;
            }
        }
        if (micDrive > 0.0005f || preDrive > 0.0005f) x = s.dc.process (x);
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
    static constexpr int maxA = 20, maxB = 12;
    struct Ch { std::array<tvc::Biquad, maxA> a; std::array<tvc::Biquad, maxB> b; tvc::Biquad lo, lo2, dc; float fast = 0, slow = 0, tg = 1, env = 0, gr = 1; };
    std::array<Ch, 2> ch;
    double sr = 96000.0;
    bool configured = false;
    Params last;
    int na = 0, nb = 0, satType = 0, preType = 0;
    float level = 1, roomPick = 1, even = 0, micDrive = 0, preDrive = 0, transient = 0, ratio = 1;
    float aFastR = 0, aSlowA = 0, aTgA = 0, aTgR = 0, aEnvA = 0, aEnvR = 0, aGr = 0;
};
} // namespace lk
