#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
#include <cmath>
#include "DSP/Biquad.h"

// Modulos de TOP RACK. Cada modulo tiene hasta 16 controles; el procesador le pasa los valores
// reales ya convertidos (dB, Hz, ms o el indice de la posicion en los selectores).
namespace trk
{
constexpr int maxKnobs = 16;
constexpr int numSlots = 8;
enum Type { Empty = 0, Trim, Revive, EqA, EqConsole, EqAmerican, CompFet, CompVca, CompOpto, CompBus, Stress, Monster, Bomber, DeEss, AirEq, Gate, NumTypes };

struct KnobDef
{
    juce::String label, unit;
    float lo = 0, hi = 1, def = 0, centre = 0;
    int dec = 1;
    juce::StringArray steps;            // si no esta vacio: selector por pasos
    float toReal (float norm) const
    {
        if (! steps.isEmpty()) return (float) juce::jlimit (0, steps.size() - 1, (int) std::round (norm * (float) (steps.size() - 1)));
        juce::NormalisableRange<float> r (lo, hi);
        if (centre > lo && centre < hi && lo >= 0) r.setSkewForCentre (centre);
        return r.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, norm));
    }
    float toNorm (float real) const
    {
        if (! steps.isEmpty()) return steps.size() > 1 ? real / (float) (steps.size() - 1) : 0.0f;
        juce::NormalisableRange<float> r (lo, hi);
        if (centre > lo && centre < hi && lo >= 0) r.setSkewForCentre (centre);
        return r.convertTo0to1 (juce::jlimit (lo, hi, real));
    }
    juce::String text (float norm) const
    {
        const float v = toReal (norm);
        if (! steps.isEmpty()) return steps[(int) v];
        return juce::String (v, dec) + (unit.isNotEmpty() ? " " + unit : juce::String());
    }
};

struct ModuleDef
{
    juce::String name, short_, category, help;
    juce::Colour panel, accent;
    bool dynamics = false;
    std::vector<KnobDef> knobs;
};

inline const std::vector<ModuleDef>& defs()
{
    static const std::vector<ModuleDef> d = []
    {
        auto U = [] (const char* s) { return juce::String::fromUTF8 (s); };
        auto K = [] (juce::String l, float lo, float hi, float def, float c, juce::String u, int dec) { KnobDef k; k.label = l; k.lo = lo; k.hi = hi; k.def = def; k.centre = c; k.unit = u; k.dec = dec; return k; };
        auto S = [] (juce::String l, juce::StringArray st, int def) { KnobDef k; k.label = l; k.steps = st; k.lo = 0; k.hi = (float) (st.size() - 1); k.def = (float) def; return k; };
        std::vector<ModuleDef> v ((size_t) NumTypes);
        v[Empty] = { U ("Vacío"), "", "", "", juce::Colour (0xff1a1d24), juce::Colour (0xff555b66), false, {} };
        v[Trim] = { "Trim", "TRIM", "Utilidad", U ("Ganancia, fase, panorama y ancho estéreo para ordenar niveles entre módulos."),
                    juce::Colour (0xff2b2f37), juce::Colour (0xffd8dde6), false,
                    { K ("Ganancia", -24, 24, 0, 0, "dB", 1), S ("Fase", { "Normal", "Invertida" }, 0), K ("Panorama", -100, 100, 0, 0, "", 0), K ("Ancho", 0, 200, 100, 100, "%", 0) } };
        v[Revive] = { "Revivir", "REVIVIR", "Color", U ("Dos perillas: Brillo (destello en los agudos) y Cuerpo (calidez y grosor en los graves)."),
                      juce::Colour (0xff3a2a52), juce::Colour (0xffc9a7ff), false,
                      { K ("Brillo", 0, 100, 0, 50, "%", 0), K ("Cuerpo", 0, 100, 0, 50, "%", 0), K ("Salida", -12, 12, 0, 0, "dB", 1) } };
        v[EqA] = { "EQ Clase A", "EQ CLASE A", "EQ", U ("Ecualizador de estilo británico clase A con dos bandas medias, graves gruesos y saturación de transformador."),
                   juce::Colour (0xff4a5a6e), juce::Colour (0xffe14b3b), false,
                   { S ("Pasa altos", { "Off", "50", "80", "160", "300" }, 0), S ("Graves Hz", { "35", "60", "110", "220" }, 1), K ("Graves", -16, 16, 0, 0, "dB", 1),
                     S ("Medio 1 Hz", { "360", "700", "1.6k", "3.2k", "4.8k", "7.2k" }, 1), K ("Medio 1", -18, 18, 0, 0, "dB", 1),
                     S ("Medio 2 Hz", { "360", "700", "1.6k", "3.2k", "4.8k", "7.2k" }, 3), K ("Medio 2", -18, 18, 0, 0, "dB", 1),
                     K ("Agudos 12k", -16, 16, 0, 0, "dB", 1), S ("Q medios", { "Normal", "Alta" }, 0), K ("Drive", 0, 100, 20, 30, "%", 0), K ("Salida", -12, 12, 0, 0, "dB", 1) } };
        v[EqConsole] = { "EQ Consola", "EQ CONSOLA", "EQ", U ("EQ de consola de 4 bandas con filtros. Tipo Negro: Q fija. Tipo Marrón: la Q se estrecha al subir la ganancia."),
                         juce::Colour (0xff23262c), juce::Colour (0xff4fb3ff), false,
                         { K ("Pasa altos", 16, 350, 16, 60, "Hz", 0), K ("Pasa bajos", 3000, 22000, 22000, 10000, "Hz", 0),
                           K ("LF", -16, 16, 0, 0, "dB", 1), K ("LF Hz", 30, 450, 100, 100, "Hz", 0), S ("LF forma", { "Shelf", "Campana" }, 0),
                           K ("LMF", -16, 16, 0, 0, "dB", 1), K ("LMF Hz", 200, 2500, 600, 700, "Hz", 0), K ("LMF Q", 0.5f, 2.5f, 1.0f, 1.0f, "", 2),
                           K ("HMF", -16, 16, 0, 0, "dB", 1), K ("HMF Hz", 600, 7000, 2500, 2000, "Hz", 0), K ("HMF Q", 0.5f, 2.5f, 1.0f, 1.0f, "", 2),
                           K ("HF", -16, 16, 0, 0, "dB", 1), K ("HF Hz", 1500, 16000, 8000, 6000, "Hz", 0), S ("HF forma", { "Shelf", "Campana" }, 0),
                           S ("Tipo", { "Negro", U ("Marrón") }, 0), K ("Salida", -12, 12, 0, 0, "dB", 1) } };
        const juce::StringArray apiGain { "-12", "-9", "-6", "-4", "-2", "0", "+2", "+4", "+6", "+9", "+12" };
        v[EqAmerican] = { "EQ Americano", "EQ AMERICANO", "EQ", U ("EQ de 3 bandas por pasos con Q proporcional: suena musical y con golpe."),
                          juce::Colour (0xff15171c), juce::Colour (0xff5ab0ff), false,
                          { S ("Graves Hz", { "50", "100", "200", "300", "400" }, 1), S ("Graves", apiGain, 5), S ("Graves forma", { "Shelf", "Campana" }, 0),
                            S ("Medios Hz", { "400", "800", "1.5k", "3k", "5k" }, 2), S ("Medios", apiGain, 5),
                            S ("Agudos Hz", { "5k", "7k", "10k", "12.5k", "15k" }, 2), S ("Agudos", apiGain, 5), S ("Agudos forma", { "Shelf", "Campana" }, 0),
                            S ("Filtro", { "Off", "On" }, 0), K ("Salida", -12, 12, 0, 0, "dB", 1) } };
        v[CompFet] = { "Comp FET", "COMP FET", U ("Dinámica"), U ("Compresor FET rapidísimo y con carácter. Más Entrada = más compresión. 7 = el más rápido. 'Todos' = el famoso modo con todos los botones."),
                       juce::Colour (0xff1c1d20), juce::Colour (0xffb8bec9), true,
                       { K ("Entrada", 0, 48, 18, 18, "dB", 1), K ("Salida", -24, 24, 0, 0, "dB", 1), K ("Ataque", 1, 7, 4, 4, "", 1), K ("Release", 1, 7, 4, 4, "", 1),
                         S ("Ratio", { "4", "8", "12", "20", "Todos" }, 0), K ("Mezcla", 0, 100, 100, 50, "%", 0) } };
        v[CompVca] = { "Comp VCA", "COMP VCA", U ("Dinámica"), U ("Compresor VCA limpio y preciso: ideal para controlar el golpe de baterías y bajos."),
                       juce::Colour (0xff2a3038), juce::Colour (0xffffb547), true,
                       { K ("Umbral", -40, 0, -18, -18, "dB", 1), K ("Ratio", 1, 20, 4, 4, ":1", 1), K ("Ataque", 0.1f, 100, 10, 10, "ms", 1), K ("Release", 10, 2000, 150, 150, "ms", 0),
                         S ("Rodilla", { "Dura", "Suave" }, 1), K ("Ganancia", -12, 24, 0, 0, "dB", 1), K ("SC pasa altos", 20, 500, 20, 100, "Hz", 0), K ("Mezcla", 0, 100, 100, 50, "%", 0) } };
        v[CompOpto] = { U ("Comp Óptico"), U ("COMP ÓPTICO"), U ("Dinámica"), U ("Compresor óptico suave y musical: Reducción de picos sube la compresión, Ganancia compensa."),
                        juce::Colour (0xff8d9196), juce::Colour (0xff20242a), true,
                        { K (U ("Reducción"), 0, 100, 40, 50, "", 0), K ("Ganancia", 0, 40, 10, 15, "dB", 1), S ("Modo", { "Comprimir", "Limitar" }, 0),
                          K (U ("Énfasis agudos"), 0, 100, 0, 50, "%", 0), K ("Mezcla", 0, 100, 100, 50, "%", 0) } };
        v[CompBus] = { "Comp Bus", "COMP BUS", U ("Dinámica"), U ("Compresor de bus estilo consola: 'pega' la mezcla o la batería. Auto release se adapta solo."),
                       juce::Colour (0xff3b3f46), juce::Colour (0xffff6157), true,
                       { K ("Umbral", -30, 10, -10, -10, "dB", 1), S ("Ratio", { "2", "4", "10" }, 1), S ("Ataque ms", { "0.1", "0.3", "1", "3", "10", "30" }, 4),
                         S ("Release s", { "0.1", "0.3", "0.6", "1.2", "Auto" }, 4), K ("Ganancia", 0, 20, 0, 6, "dB", 1),
                         S ("SC pasa altos", { "Off", "60", "90", "150" }, 0), K ("Mezcla", 0, 100, 100, 50, "%", 0) } };
        v[Stress] = { "Estresador", "ESTRESADOR", U ("Dinámica"), U ("Compresor todoterreno con distorsión armónica: desde limpio hasta destruir. 'Nuke' es un limitador brutal."),
                      juce::Colour (0xff111214), juce::Colour (0xffff3b30), true,
                      { K ("Entrada", 0, 48, 12, 12, "dB", 1), K ("Salida", -24, 24, 0, 0, "dB", 1), K ("Ataque", 0, 10, 5, 5, "", 1), K ("Release", 0, 10, 5, 5, "", 1),
                        S ("Ratio", { "1:1", "2:1", "3:1", "4:1", "6:1", "10:1", "20:1", "Nuke" }, 3), S ("Detector", { "Normal", "Pasa altos", "Banda" }, 0),
                        S ("Audio", { "Limpio", "Dist 2", "Dist 3" }, 0), K ("Mezcla", 0, 100, 100, 50, "%", 0) } };
        v[Monster] = { "Monstruo", "MONSTRUO", U ("Dinámica"), U ("Compresión paralela extrema: mezcla un aplastado brutal con la señal original."),
                       juce::Colour (0xff5a1414), juce::Colour (0xffffd23f), true,
                       { K ("Cantidad", 0, 100, 50, 50, "%", 0), S ("Modo", { "Normal", "Crush" }, 0), K ("Tono", -100, 100, 0, 0, "", 0), K ("Mezcla", 0, 100, 40, 50, "%", 0), K ("Salida", -12, 12, 0, 0, "dB", 1) } };
        v[Bomber] = { "Bombardero", "BOMBARDERO", "Color", U ("Golpe, cuerpo y mordida para baterías y mezclas con 4 perillas."),
                      juce::Colour (0xff2c3b2a), juce::Colour (0xff9bff6b), false,
                      { K ("Golpe", 0, 100, 0, 50, "%", 0), K ("Cuerpo", 0, 100, 0, 50, "%", 0), K ("Crunch", 0, 100, 0, 50, "%", 0), K ("Mezcla", 0, 100, 100, 50, "%", 0), K ("Salida", -12, 12, 0, 0, "dB", 1) } };
        v[DeEss] = { "De-Esser", "DE-ESSER", U ("Dinámica"), U ("Baja las eses y la sibilancia. Escuchar = oyes solo lo que detecta."),
                     juce::Colour (0xff202a33), juce::Colour (0xff4fe3d0), false,
                     { K ("Frecuencia", 2000, 12000, 6000, 5500, "Hz", 0), K ("Rango", 0, 20, 8, 8, "dB", 1), K ("Umbral", -50, 0, -24, -24, "dB", 1),
                       S ("Modo", { "Banda", "Ancho" }, 0), S ("Escuchar", { "Off", "On" }, 0) } };
        v[AirEq] = { "EQ de Aire", "EQ DE AIRE", "EQ", U ("Aire muy suave arriba, medios y 'tierra' abajo: brillo sin dureza."),
                     juce::Colour (0xffd8dde3), juce::Colour (0xff1e7bd6), false,
                     { K ("Aire", 0, 12, 0, 4, "dB", 1), S ("Aire Hz", { "2.5k", "5k", "10k", "20k", "40k" }, 2), K ("Medios", -12, 12, 0, 0, "dB", 1),
                       K ("Medios Hz", 200, 8000, 1000, 1200, "Hz", 0), K ("Medios Q", 0.3f, 3.0f, 0.8f, 1.0f, "", 2), K ("Tierra", -12, 12, 0, 0, "dB", 1),
                       K ("Tierra Hz", 20, 250, 60, 70, "Hz", 0), K ("Salida", -12, 12, 0, 0, "dB", 1) } };
        v[Gate] = { "Puerta", "PUERTA", U ("Dinámica"), U ("Puerta de ruido / expansor: cierra cuando la señal baja del umbral."),
                    juce::Colour (0xff2e2a1e), juce::Colour (0xffffd36b), true,
                    { K ("Umbral", -80, 0, -40, -40, "dB", 1), K ("Rango", 0, 80, 40, 40, "dB", 1), K ("Ataque", 0.05f, 50, 0.5f, 2, "ms", 2),
                      K ("Hold", 0, 500, 20, 50, "ms", 0), K ("Release", 5, 2000, 120, 150, "ms", 0), K ("SC pasa altos", 20, 2000, 20, 200, "Hz", 0) } };
        return v;
    }();
    return d;
}

//==============================================================================
inline float dB2g (float d) { return std::pow (10.0f, d * 0.05f); }
inline float g2dB (float g) { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }
inline float coefMs (double sr, float ms) { return 1.0f - std::exp (-1.0f / (float) (std::max (0.005f, ms) * 0.001 * sr)); }
// saturacion suave con algo de armonicos pares
inline float warm (float x, float drive)
{
    if (drive <= 0.0f) return x;
    const float k = 1.0f + drive * 4.0f;
    return std::tanh (k * x + 0.15f * drive * x * x) / k * (1.0f + drive * 0.2f);
}
// ganancia de un compresor (dB de reduccion) con rodilla
inline float reduction (float lvl, float thr, float ratio, float knee)
{
    const float over = lvl - thr;
    const float slope = 1.0f - 1.0f / std::max (1.0f, ratio);
    if (knee <= 0.01f) return over > 0 ? over * slope : 0.0f;
    if (over <= -knee * 0.5f) return 0.0f;
    if (over >= knee * 0.5f) return over * slope;
    return slope * (over + knee * 0.5f) * (over + knee * 0.5f) / (2.0f * knee);
}

struct Module
{
    virtual ~Module() = default;
    virtual void prepare (double r) { sr = r; reset(); }
    virtual void reset() {}
    // v: valores reales de las perillas. scL/scR: sidechain externo (o nullptr)
    virtual void process (float* L, float* R, int n, const float* v, const float* scL, const float* scR) = 0;
    std::atomic<float> gr { 0.0f };
    double sr = 44100;
};

//==============================================================================
struct TrimModule : Module
{
    float gl = 1, gr_ = 1;
    void process (float* L, float* R, int n, const float* v, const float*, const float*) override
    {
        const float g = dB2g (v[0]) * (v[1] > 0.5f ? -1.0f : 1.0f);
        const float pan = v[2] / 100.0f, w = v[3] / 100.0f;
        const float tl = g * std::min (1.0f, 1.0f - pan), tr = g * std::min (1.0f, 1.0f + pan);
        for (int i = 0; i < n; ++i)
        {
            gl += (tl - gl) * 0.002f; gr_ += (tr - gr_) * 0.002f;
            const float m = 0.5f * (L[i] + R[i]), s = 0.5f * (L[i] - R[i]) * w;
            L[i] = (m + s) * gl; R[i] = (m - s) * gr_;
        }
    }
};

struct ReviveModule : Module
{
    std::array<tvc::Biquad, 2> hp, lp, dcH, dcL, shelfH, shelfL;
    float lastB = -1, lastT = -1;
    void reset() override { for (auto* a : { &hp, &lp, &dcH, &dcL, &shelfH, &shelfL }) for (auto& b : *a) b.reset(); lastB = lastT = -1; }
    void prepare (double r) override
    {
        sr = r;
        for (auto& b : hp) b.setHighPass (sr, 3500.0, 0.707);
        for (auto& b : lp) b.setLowPass (sr, 280.0, 0.707);
        for (auto& b : dcH) b.setHighPass1 (sr, 2000.0);
        for (auto& b : dcL) b.setHighPass1 (sr, 25.0);
        reset();
    }
    void process (float* L, float* R, int n, const float* v, const float*, const float*) override
    {
        const float b = v[0] / 100.0f, t = v[1] / 100.0f, out = dB2g (v[2]);
        if (b != lastB || t != lastT)
        {
            lastB = b; lastT = t;
            for (auto& f : shelfH) f.setHighShelf (sr, 9000.0, b * 3.0);
            for (auto& f : shelfL) f.setLowShelf (sr, 120.0, t * 3.0);
        }
        float* ch[2] { L, R };
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                float x = ch[c][i];
                const float h = hp[(size_t) c].process (x);
                const float hs = dcH[(size_t) c].process (std::tanh (h * 6.0f) / 6.0f - h);       // armonicos en agudos
                const float l = lp[(size_t) c].process (x);
                const float ls = dcL[(size_t) c].process (std::tanh (l * 3.0f + 0.3f * l * l) / 3.0f - l);   // calidez par en graves
                x += hs * b * 2.5f + ls * t * 2.0f;
                x = shelfL[(size_t) c].process (shelfH[(size_t) c].process (x));
                ch[c][i] = x * out;
            }
    }
};

//==============================================================================
// EQ generico de una cadena de biquads con recalculo cuando cambian las perillas
struct EqBase : Module
{
    static constexpr int maxBands = 8;
    std::array<std::array<tvc::Biquad, maxBands>, 2> bq;
    std::array<bool, maxBands> act {};
    std::array<float, maxKnobs> last {};
    float drive = 0, outG = 1;
    void reset() override { for (auto& c : bq) for (auto& b : c) { b.reset(); } last.fill (-999.0f); }
    void setBand (int i, bool on) { act[(size_t) i] = on; }
    virtual void design (const float* v) = 0;
    void process (float* L, float* R, int n, const float* v, const float*, const float*) override
    {
        bool changed = false;
        for (int k = 0; k < maxKnobs; ++k) if (v[k] != last[(size_t) k]) { changed = true; last[(size_t) k] = v[k]; }
        if (changed) design (v);
        float* ch[2] { L, R };
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                float x = ch[c][i];
                if (drive > 0) x = warm (x, drive);
                for (int b = 0; b < maxBands; ++b) if (act[(size_t) b]) x = bq[(size_t) c][(size_t) b].process (x);
                ch[c][i] = x * outG;
            }
    }
    void all (int b, const std::function<void (tvc::Biquad&)>& f) { for (auto& c : bq) f (c[(size_t) b]); }
};

struct EqAModule : EqBase
{
    void design (const float* v) override
    {
        const float hpf[] { 0, 50, 80, 160, 300 }, lf[] { 35, 60, 110, 220 }, mf[] { 360, 700, 1600, 3200, 4800, 7200 };
        const int h = (int) v[0];
        setBand (0, h > 0); setBand (1, h > 0);
        if (h > 0) { all (0, [&] (tvc::Biquad& b) { b.setHighPass (sr, hpf[h], 0.75); }); all (1, [&] (tvc::Biquad& b) { b.setHighPass1 (sr, hpf[h]); }); }
        // graves inductivos: shelf con un leve rebote
        setBand (2, std::abs (v[2]) > 0.05f);
        all (2, [&] (tvc::Biquad& b) { b.setLowShelf (sr, lf[(int) v[1]], v[2]); });
        setBand (3, std::abs (v[2]) > 0.05f);
        all (3, [&] (tvc::Biquad& b) { b.setPeak (sr, lf[(int) v[1]] * 2.2, 1.2, -v[2] * 0.18); });
        const double q = v[8] > 0.5f ? 1.6 : 0.7;
        setBand (4, std::abs (v[4]) > 0.05f); all (4, [&] (tvc::Biquad& b) { b.setPeak (sr, std::min (sr * 0.45, (double) mf[(int) v[3]]), q, v[4]); });
        setBand (5, std::abs (v[6]) > 0.05f); all (5, [&] (tvc::Biquad& b) { b.setPeak (sr, std::min (sr * 0.45, (double) mf[(int) v[5]]), q, v[6]); });
        setBand (6, std::abs (v[7]) > 0.05f); all (6, [&] (tvc::Biquad& b) { b.setHighShelf (sr, std::min (sr * 0.45, 12000.0), v[7]); });
        setBand (7, false);
        drive = v[9] / 100.0f * 0.6f;
        outG = dB2g (v[10]);
    }
};

struct EqConsoleModule : EqBase
{
    void design (const float* v) override
    {
        const bool brown = v[14] > 0.5f;
        auto propQ = [brown] (float q, float g) { return brown ? q * (0.6f + std::abs (g) / 12.0f) : q; };
        setBand (0, v[0] > 17.0f); all (0, [&] (tvc::Biquad& b) { b.setHighPass (sr, v[0], 0.7); });
        setBand (1, v[0] > 17.0f); all (1, [&] (tvc::Biquad& b) { b.setHighPass1 (sr, v[0]); });
        setBand (2, v[1] < 21500.0f); all (2, [&] (tvc::Biquad& b) { b.setLowPass (sr, std::min ((double) v[1], sr * 0.45), 0.7); });
        setBand (3, std::abs (v[2]) > 0.05f);
        all (3, [&] (tvc::Biquad& b) { if (v[4] > 0.5f) b.setPeak (sr, v[3], 0.8, v[2]); else b.setLowShelf (sr, v[3], v[2]); });
        setBand (4, std::abs (v[5]) > 0.05f); all (4, [&] (tvc::Biquad& b) { b.setPeak (sr, v[6], propQ (v[7], v[5]), v[5]); });
        setBand (5, std::abs (v[8]) > 0.05f); all (5, [&] (tvc::Biquad& b) { b.setPeak (sr, std::min ((double) v[9], sr * 0.45), propQ (v[10], v[8]), v[8]); });
        setBand (6, std::abs (v[11]) > 0.05f);
        all (6, [&] (tvc::Biquad& b) { const double f = std::min ((double) v[12], sr * 0.45); if (v[13] > 0.5f) b.setPeak (sr, f, 0.8, v[11]); else b.setHighShelf (sr, f, v[11]); });
        setBand (7, false);
        drive = 0.05f;
        outG = dB2g (v[15]);
    }
};

struct EqAmericanModule : EqBase
{
    void design (const float* v) override
    {
        const float gains[] { -12, -9, -6, -4, -2, 0, 2, 4, 6, 9, 12 };
        const float lf[] { 50, 100, 200, 300, 400 }, mf[] { 400, 800, 1500, 3000, 5000 }, hf[] { 5000, 7000, 10000, 12500, 15000 };
        auto pq = [] (float g) { return 0.4 + std::abs (g) / 12.0 * 1.4; };   // Q proporcional
        const float gl = gains[(int) v[1]], gm = gains[(int) v[4]], gh = gains[(int) v[6]];
        setBand (0, gl != 0); all (0, [&] (tvc::Biquad& b) { if (v[2] > 0.5f) b.setPeak (sr, lf[(int) v[0]], pq (gl), gl); else b.setLowShelf (sr, lf[(int) v[0]], gl); });
        setBand (1, gm != 0); all (1, [&] (tvc::Biquad& b) { b.setPeak (sr, mf[(int) v[3]], pq (gm), gm); });
        setBand (2, gh != 0); all (2, [&] (tvc::Biquad& b) { const double f = std::min ((double) hf[(int) v[5]], sr * 0.45); if (v[7] > 0.5f) b.setPeak (sr, f, pq (gh), gh); else b.setHighShelf (sr, f, gh); });
        setBand (3, v[8] > 0.5f); all (3, [&] (tvc::Biquad& b) { b.setHighPass (sr, 50.0, 0.7); });
        setBand (4, v[8] > 0.5f); all (4, [&] (tvc::Biquad& b) { b.setLowPass (sr, std::min (15000.0, sr * 0.45), 0.7); });
        setBand (5, false); setBand (6, false); setBand (7, false);
        drive = 0.08f;
        outG = dB2g (v[9]);
    }
};

//==============================================================================
// Compresor FET (ataque ultrarrapido, color de transistor)
struct FetModule : Module
{
    float env = 0, g = 0, smix = 1;
    std::array<tvc::Biquad, 2> dc;
    void reset() override { env = 0; g = 0; for (auto& d : dc) d.reset(); }
    void prepare (double r) override { sr = r; for (auto& d : dc) d.setHighPass1 (sr, 15.0); reset(); }
    void process (float* L, float* R, int n, const float* v, const float* scL, const float* scR) override
    {
        const float in = dB2g (v[0]), out = dB2g (v[1]);
        const float atk = 0.8f + (0.02f - 0.8f) * (v[2] - 1.0f) / 6.0f;            // 7 = mas rapido
        const float rel = 1100.0f + (50.0f - 1100.0f) * (v[3] - 1.0f) / 6.0f;
        const int ri = (int) v[4];
        const float ratio = ri == 0 ? 4.0f : ri == 1 ? 8.0f : ri == 2 ? 12.0f : 20.0f;
        const bool allBtn = ri == 4;
        const float a = coefMs (sr, atk), r = coefMs (sr, rel), mix = v[5] / 100.0f;
        const float thr = allBtn ? -22.0f : -18.0f;
        float mx = 0;
        for (int i = 0; i < n; ++i)
        {
            const float xl = L[i] * in, xr = R[i] * in;
            const float d = scL != nullptr ? std::max (std::abs (scL[i]), std::abs (scR[i])) * in : std::max (std::abs (xl), std::abs (xr));
            env += (d - env) * (d > env ? a : r);
            float red = reduction (g2dB (env), thr, ratio, allBtn ? 0.0f : 4.0f);
            if (allBtn) red *= 1.35f;
            g += (red - g) * (red > g ? 0.5f : 0.2f);
            mx = std::max (mx, g);
            const float k = dB2g (-g) * out * 0.5f;    // -6 dB de compensacion de la etapa de entrada
            const float drv = std::min (1.0f, g / 20.0f) * (allBtn ? 0.5f : 0.2f);
            float yl = xl * k, yr = xr * k;
            if (drv > 0.0f) { yl = dc[0].process (warm (yl, drv)); yr = dc[1].process (warm (yr, drv)); }
            L[i] = L[i] * out + (yl - L[i] * out) * mix;
            R[i] = R[i] * out + (yr - R[i] * out) * mix;
        }
        gr.store (mx);
    }
};

struct VcaModule : Module
{
    float env = 0, g = 0;
    std::array<tvc::Biquad, 2> schp;
    float lastHp = -1;
    void reset() override { env = 0; g = 0; for (auto& b : schp) b.reset(); lastHp = -1; }
    void process (float* L, float* R, int n, const float* v, const float* scL, const float* scR) override
    {
        if (v[6] != lastHp) { lastHp = v[6]; for (auto& b : schp) b.setHighPass (sr, v[6], 0.707); }
        const float a = coefMs (sr, v[2]), r = coefMs (sr, v[3]), mk = dB2g (v[5]), mix = v[7] / 100.0f;
        const float knee = v[4] > 0.5f ? 8.0f : 0.0f;
        const float rmsC = coefMs (sr, 3.0f);
        float mx = 0;
        for (int i = 0; i < n; ++i)
        {
            float dl = scL != nullptr ? scL[i] : L[i], dr = scR != nullptr ? scR[i] : R[i];
            if (v[6] > 21.0f) { dl = schp[0].process (dl); dr = schp[1].process (dr); }
            const float p = std::max (dl * dl, dr * dr);
            env += (p - env) * rmsC;
            const float red = reduction (10.0f * std::log10 (env * 2.0f + 1e-12f), v[0], v[1], knee);
            g += (red - g) * (red > g ? a : r);
            mx = std::max (mx, g);
            const float k = dB2g (-g) * mk;
            L[i] = L[i] + (L[i] * k - L[i]) * mix;
            R[i] = R[i] + (R[i] * k - R[i]) * mix;
        }
        gr.store (mx);
    }
};

// Compresor optico: ataque ~10 ms y release en dos etapas que depende de cuanto ha comprimido
struct OptoModule : Module
{
    float env = 0, gFast = 0, gSlow = 0, memory = 0;
    std::array<tvc::Biquad, 2> emph;
    float lastE = -1;
    void reset() override { env = gFast = gSlow = memory = 0; lastE = -1; for (auto& b : emph) b.reset(); }
    void process (float* L, float* R, int n, const float* v, const float* scL, const float* scR) override
    {
        if (v[3] != lastE) { lastE = v[3]; for (auto& b : emph) b.setHighShelf (sr, 3000.0, v[3] / 100.0f * 12.0f); }
        const float thr = -(v[0] / 100.0f) * 40.0f, ratio = v[2] > 0.5f ? 20.0f : 3.0f;
        const float gain = dB2g (v[1] - 10.0f), mix = v[4] / 100.0f;
        const float a = coefMs (sr, 10.0f), rf = coefMs (sr, 60.0f), rmsC = coefMs (sr, 5.0f);
        float mx = 0;
        for (int i = 0; i < n; ++i)
        {
            float dl = scL != nullptr ? scL[i] : L[i], dr = scR != nullptr ? scR[i] : R[i];
            dl = emph[0].process (dl); dr = emph[1].process (dr);
            env += (std::max (dl * dl, dr * dr) - env) * rmsC;
            const float red = reduction (10.0f * std::log10 (env * 2.0f + 1e-12f), thr, ratio, 6.0f);
            // memoria de la celula: si comprime mucho rato, suelta mas lento (0.5 - 5 s)
            memory += (std::min (1.0f, red / 10.0f) - memory) * coefMs (sr, 2000.0f);
            const float rs = coefMs (sr, 500.0f + memory * 4500.0f);
            gFast += (red - gFast) * (red > gFast ? a : rf);
            gSlow += (red * 0.5f - gSlow) * (red * 0.5f > gSlow ? a : rs);
            const float g = std::max (gFast * 0.6f, gSlow);
            mx = std::max (mx, g);
            const float k = dB2g (-g) * gain;
            L[i] = L[i] + (warm (L[i] * k, 0.05f) - L[i]) * mix;
            R[i] = R[i] + (warm (R[i] * k, 0.05f) - R[i]) * mix;
        }
        gr.store (mx);
    }
};

// Compresor de bus estilo consola
struct BusModule : Module
{
    float env = 0, g = 0, slowRel = 0;
    std::array<tvc::Biquad, 2> schp;
    int lastHp = -1;
    void reset() override { env = g = slowRel = 0; lastHp = -1; for (auto& b : schp) b.reset(); }
    void process (float* L, float* R, int n, const float* v, const float* scL, const float* scR) override
    {
        const float ratios[] { 2, 4, 10 }, atks[] { 0.1f, 0.3f, 1, 3, 10, 30 }, rels[] { 100, 300, 600, 1200, 0 }, hps[] { 0, 60, 90, 150 };
        const int hp = (int) v[5];
        if (hp != lastHp) { lastHp = hp; if (hp > 0) for (auto& b : schp) b.setHighPass (sr, hps[hp], 0.707); }
        const float ratio = ratios[(int) v[1]], a = coefMs (sr, atks[(int) v[2]]);
        const bool autoRel = (int) v[3] == 4;
        const float mk = dB2g (v[4]), mix = v[6] / 100.0f;
        float mx = 0;
        for (int i = 0; i < n; ++i)
        {
            float dl = scL != nullptr ? scL[i] : L[i], dr = scR != nullptr ? scR[i] : R[i];
            if (hp > 0) { dl = schp[0].process (dl); dr = schp[1].process (dr); }
            const float d = std::max (std::abs (dl), std::abs (dr));
            env += (d - env) * (d > env ? coefMs (sr, 0.2f) : coefMs (sr, 20.0f));
            const float red = reduction (g2dB (env), v[0], ratio, 6.0f);
            float r;
            if (autoRel)
            {
                // auto: release rapido para picos, lento cuando la compresion es sostenida
                slowRel += (red - slowRel) * coefMs (sr, 400.0f);
                r = coefMs (sr, 100.0f + std::min (1.0f, slowRel / 6.0f) * 1100.0f);
            }
            else r = coefMs (sr, rels[(int) v[3]]);
            g += (red - g) * (red > g ? a : r);
            mx = std::max (mx, g);
            const float k = dB2g (-g) * mk;
            L[i] = L[i] + (L[i] * k - L[i]) * mix;
            R[i] = R[i] + (R[i] * k - R[i]) * mix;
        }
        gr.store (mx);
    }
};

// Estresador: compresor con ratios por pasos, detector filtrado y distorsion de 2o / 3er armonico
struct StressModule : Module
{
    float env = 0, g = 0;
    std::array<tvc::Biquad, 2> detHp, detBand, dc;
    void reset() override { env = g = 0; for (auto* a : { &detHp, &detBand, &dc }) for (auto& b : *a) b.reset(); }
    void prepare (double r) override
    {
        sr = r;
        for (auto& b : detHp) b.setHighPass (sr, 100.0, 0.707);
        for (auto& b : detBand) b.setPeak (sr, 6000.0, 1.0, 12.0);
        for (auto& b : dc) b.setHighPass1 (sr, 12.0);
        reset();
    }
    void process (float* L, float* R, int n, const float* v, const float* scL, const float* scR) override
    {
        const float in = dB2g (v[0]), out = dB2g (v[1]), mix = v[7] / 100.0f;
        const float atk = 0.05f * std::pow (600.0f, v[2] / 10.0f);          // 0.05 .. 30 ms
        const float rel = 50.0f * std::pow (70.0f, v[3] / 10.0f);            // 50 ms .. 3.5 s
        const int ri = (int) v[4];
        const float ratios[] { 1, 2, 3, 4, 6, 10, 20, 50 };
        const bool nuke = ri == 7;
        const int det = (int) v[5], dist = (int) v[6];
        const float a = coefMs (sr, atk), r = coefMs (sr, rel);
        float mx = 0;
        for (int i = 0; i < n; ++i)
        {
            const float xl = L[i] * in, xr = R[i] * in;
            float dl = scL != nullptr ? scL[i] * in : xl, dr = scR != nullptr ? scR[i] * in : xr;
            if (det == 1) { dl = detHp[0].process (dl); dr = detHp[1].process (dr); }
            else if (det == 2) { dl = detBand[0].process (dl); dr = detBand[1].process (dr); }
            const float d = std::max (std::abs (dl), std::abs (dr));
            env += (d - env) * (d > env ? a : r);
            const float red = ri == 0 ? 0.0f : reduction (g2dB (env), nuke ? -24.0f : -20.0f, ratios[ri], nuke ? 0.0f : (ri <= 3 ? 10.0f : 3.0f));
            g += (red - g) * (red > g ? 0.6f : 0.3f);
            mx = std::max (mx, g);
            const float k = dB2g (-g) * out * 0.5f;
            float yl = xl * k, yr = xr * k;
            if (dist == 1) { yl = dc[0].process (yl + 0.3f * yl * yl); yr = dc[1].process (yr + 0.3f * yr * yr); }       // 2o armonico
            else if (dist == 2) { yl = std::tanh (yl * 3.0f) / 3.0f * 1.25f; yr = std::tanh (yr * 3.0f) / 3.0f * 1.25f; }                                 // 3er armonico
            L[i] = L[i] * out + (yl - L[i] * out) * mix;
            R[i] = R[i] * out + (yr - R[i] * out) * mix;
        }
        gr.store (mx);
    }
};

// Monstruo: aplastado brutal en paralelo
struct MonsterModule : Module
{
    float env = 0, g = 0;
    std::array<tvc::Biquad, 2> tilt;
    float lastT = -999;
    void reset() override { env = g = 0; lastT = -999; for (auto& b : tilt) b.reset(); }
    void process (float* L, float* R, int n, const float* v, const float* scL, const float* scR) override
    {
        if (v[2] != lastT) { lastT = v[2]; for (auto& b : tilt) b.setHighShelf (sr, 2500.0, v[2] / 100.0f * 9.0f); }
        const float thr = -6.0f - v[0] / 100.0f * 36.0f;
        const bool crush = v[1] > 0.5f;
        const float mix = v[3] / 100.0f, out = dB2g (v[4]);
        const float a = coefMs (sr, 0.3f), r = coefMs (sr, 60.0f);
        float mx = 0;
        for (int i = 0; i < n; ++i)
        {
            const float d = scL != nullptr ? std::max (std::abs (scL[i]), std::abs (scR[i])) : std::max (std::abs (L[i]), std::abs (R[i]));
            env += (d - env) * (d > env ? a : r);
            const float red = reduction (g2dB (env), thr, 20.0f, 2.0f);
            g += (red - g) * 0.5f;
            mx = std::max (mx, g);
            const float k = dB2g (-g + std::min (24.0f, -thr * 0.8f));
            float wl = tilt[0].process (L[i] * k), wr = tilt[1].process (R[i] * k);
            if (crush) { wl = std::tanh (wl * 2.0f) * 0.6f; wr = std::tanh (wr * 2.0f) * 0.6f; }
            L[i] = (L[i] + wl * mix) * out / (1.0f + mix * 0.5f);
            R[i] = (R[i] + wr * mix) * out / (1.0f + mix * 0.5f);
        }
        gr.store (mx);
    }
};

// Bombardero: golpe (transientes), cuerpo (graves dinamicos), crunch (saturacion)
struct BomberModule : Module
{
    float fast = 0, slow = 0, gT = 1;
    std::array<tvc::Biquad, 2> body, dc;
    float lastB = -1;
    void reset() override { fast = slow = 0; gT = 1; lastB = -1; for (auto& b : body) b.reset(); for (auto& b : dc) b.reset(); }
    void prepare (double r) override { sr = r; for (auto& b : dc) b.setHighPass1 (sr, 15.0); reset(); }
    void process (float* L, float* R, int n, const float* v, const float*, const float*) override
    {
        if (v[1] != lastB) { lastB = v[1]; for (auto& b : body) b.setPeak (sr, 80.0, 0.8, v[1] / 100.0f * 7.0f); }
        const float pu = v[0] / 100.0f, cr = v[2] / 100.0f, mix = v[3] / 100.0f, out = dB2g (v[4]);
        const float fa = coefMs (sr, 0.5f), fr = coefMs (sr, 30.0f), sa = coefMs (sr, 25.0f), srl = coefMs (sr, 150.0f), sm = coefMs (sr, 1.0f);
        for (int i = 0; i < n; ++i)
        {
            const float a = std::max (std::abs (L[i]), std::abs (R[i]));
            fast += (a - fast) * (a > fast ? fa : fr);
            slow += (a - slow) * (a > slow ? sa : srl);
            const float diff = fast > 1e-6f ? std::clamp (g2dB (fast) - g2dB (slow + 1e-9f), 0.0f, 12.0f) : 0.0f;
            gT += (dB2g (pu * diff * 0.7f) - gT) * sm;
            float yl = body[0].process (L[i] * gT), yr = body[1].process (R[i] * gT);
            if (cr > 0) { yl = dc[0].process (warm (yl, cr * 0.8f)); yr = dc[1].process (warm (yr, cr * 0.8f)); }
            L[i] = (L[i] + (yl - L[i]) * mix) * out;
            R[i] = (R[i] + (yr - R[i]) * mix) * out;
        }
    }
};

// De-esser: banda (solo corta la zona de las eses) o ancho (baja todo)
struct DeEssModule : Module
{
    std::array<tvc::Biquad, 2> det;
    juce::dsp::LinkwitzRileyFilter<float> xo;
    float env = 0, g = 0, lastF = -1;
    void prepare (double r) override { sr = r; xo.prepare ({ sr, 512, 2 }); reset(); }
    void reset() override { env = g = 0; lastF = -1; for (auto& b : det) b.reset(); xo.reset(); }
    void process (float* L, float* R, int n, const float* v, const float* scL, const float* scR) override
    {
        if (v[0] != lastF) { lastF = v[0]; for (auto& b : det) b.setHighPass (sr, v[0] * 0.8, 0.707); xo.setCutoffFrequency (v[0] * 0.8f); }
        const float range = v[1], thr = v[2];
        const bool wide = v[3] > 0.5f, listen = v[4] > 0.5f;
        const float a = coefMs (sr, 0.5f), r = coefMs (sr, 60.0f);
        float mx = 0;
        for (int i = 0; i < n; ++i)
        {
            const float sl = det[0].process (scL != nullptr ? scL[i] : L[i]), sr2 = det[1].process (scR != nullptr ? scR[i] : R[i]);
            const float d = std::max (std::abs (sl), std::abs (sr2));
            env += (d - env) * (d > env ? a : r);
            const float red = std::min (range, std::max (0.0f, g2dB (env) - thr) * 0.8f);
            g += (red - g) * 0.3f;
            mx = std::max (mx, g);
            const float k = dB2g (-g);
            if (listen) { L[i] = sl; R[i] = sr2; continue; }
            if (wide) { L[i] *= k; R[i] *= k; continue; }
            // division Linkwitz-Riley: graves + agudos suman plano; solo se baja la banda aguda
            float lo, hi;
            xo.processSample (0, L[i], lo, hi); L[i] = lo + hi * k;
            xo.processSample (1, R[i], lo, hi); R[i] = lo + hi * k;
        }
        gr.store (mx);
    }
};

struct AirEqModule : EqBase
{
    void design (const float* v) override
    {
        const float af[] { 2500, 5000, 10000, 20000, 40000 };
        const double f = std::min ((double) af[(int) v[1]], sr * 0.45);
        setBand (0, v[0] > 0.05f); all (0, [&] (tvc::Biquad& b) { b.setHighShelf (sr, f * (f > 15000 ? 0.6 : 1.0), v[0]); });
        setBand (1, std::abs (v[2]) > 0.05f); all (1, [&] (tvc::Biquad& b) { b.setPeak (sr, v[3], v[4], v[2]); });
        setBand (2, std::abs (v[5]) > 0.05f); all (2, [&] (tvc::Biquad& b) { b.setLowShelf (sr, v[6], v[5]); });
        for (int i = 3; i < maxBands; ++i) setBand (i, false);
        drive = 0; outG = dB2g (v[7]);
    }
};

struct GateModule : Module
{
    std::array<tvc::Biquad, 2> hp;
    float env = 0, gain = 1, lastHp = -1;
    int hold = 0;
    bool open = false;
    void reset() override { env = 0; gain = 1; hold = 0; open = false; lastHp = -1; for (auto& b : hp) b.reset(); }
    void process (float* L, float* R, int n, const float* v, const float* scL, const float* scR) override
    {
        if (v[5] != lastHp) { lastHp = v[5]; for (auto& b : hp) b.setHighPass (sr, v[5], 0.707); }
        const float floorG = dB2g (-v[1]);
        const float a = coefMs (sr, v[2]), r = coefMs (sr, v[4]), ec = coefMs (sr, 1.0f);
        const int holdS = (int) (v[3] * 0.001 * sr);
        float mn = 1;
        for (int i = 0; i < n; ++i)
        {
            float dl = scL != nullptr ? scL[i] : L[i], dr = scR != nullptr ? scR[i] : R[i];
            if (v[5] > 21.0f) { dl = hp[0].process (dl); dr = hp[1].process (dr); }
            const float d = std::max (std::abs (dl), std::abs (dr));
            env += (d - env) * (d > env ? 0.5f : ec);
            const float lvl = g2dB (env);
            if (lvl > v[0]) { open = true; hold = holdS; }
            else if (lvl < v[0] - 4.0f) { if (hold > 0) --hold; else open = false; }
            const float t = open ? 1.0f : floorG;
            gain += (t - gain) * (t > gain ? a : r);
            mn = std::min (mn, gain);
            L[i] *= gain; R[i] *= gain;
        }
        gr.store (-g2dB (mn));
    }
};

inline std::unique_ptr<Module> create (int t)
{
    switch (t)
    {
        case Trim: return std::make_unique<TrimModule>();
        case Revive: return std::make_unique<ReviveModule>();
        case EqA: return std::make_unique<EqAModule>();
        case EqConsole: return std::make_unique<EqConsoleModule>();
        case EqAmerican: return std::make_unique<EqAmericanModule>();
        case CompFet: return std::make_unique<FetModule>();
        case CompVca: return std::make_unique<VcaModule>();
        case CompOpto: return std::make_unique<OptoModule>();
        case CompBus: return std::make_unique<BusModule>();
        case Stress: return std::make_unique<StressModule>();
        case Monster: return std::make_unique<MonsterModule>();
        case Bomber: return std::make_unique<BomberModule>();
        case DeEss: return std::make_unique<DeEssModule>();
        case AirEq: return std::make_unique<AirEqModule>();
        case Gate: return std::make_unique<GateModule>();
        default: return nullptr;
    }
}
} // namespace trk
