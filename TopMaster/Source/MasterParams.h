#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

// Tabla de parametros de TOP MASTER (la usan el procesador y la interfaz)
namespace tmaster
{
enum Module { EQ = 0, DEQ, SHAPER, DYN, BASS, IMPACT, UNLIMIT, CLARITY, EXCITER, TAPE, IMAGER, MAXIMIZER, DITHER, NumModules };
constexpr int numReorderable = 11;   // Maximizador y Dither siempre al final

inline const char* modulePrefix (int m)
{
    static const char* p[] { "eq", "deq", "ss", "dyn", "bc", "imp", "unl", "cla", "exc", "tape", "img", "max", "dit" };
    return p[m];
}
inline juce::String moduleName (int m)
{
    static const char* n[] { "Ecualizador", "EQ Dinámico", "Moldeador Espectral", "Dinámica", "Control de Graves", "Impacto",
                             "Deslimitador", "Claridad", "Excitador", "Cinta Vintage", "Imagen Estéreo", "Maximizador", "Dither" };
    return juce::String::fromUTF8 (n[m]);
}
inline juce::String moduleHelp (int m)
{
    static const char* h[] {
        "EQ de 8 bandas. Cada banda puede trabajar en estéreo, solo en el centro (Mid) o solo en los lados (Side).",
        "Bandas que solo cortan (o suben) cuando esa zona pasa del umbral: perfecto para graves que retumban o agudos que pican.",
        "Suaviza resonancias y dureza solo cuando aparecen (como Top Smooth).",
        "Compresor de 3 bandas: controla graves, medios y agudos por separado.",
        "Ajusta el volumen, golpe y cola de los graves, y pone en mono lo más grave.",
        "Da más golpe (o lo suaviza) en 4 zonas del espectro sin cambiar el volumen general.",
        "Devuelve vida a mezclas aplastadas por un limitador: recupera los transientes.",
        "Abre la mezcla subiendo con suavidad las zonas tapadas del espectro.",
        "Añade armónicos (brillo y calidez) en 3 bandas con distintos colores de saturación.",
        "Color de cinta analógica: saturación suave, realce de graves y agudos redondeados.",
        "Ancho estéreo por bandas: graves más centrados, agudos más abiertos.",
        "Limitador final: sube el volumen sin pasar del techo (true peak).",
        "Ruido de cuantización para exportar a 16 o 24 bits sin distorsión digital." };
    return juce::String::fromUTF8 (h[m]);
}

struct FloatDef { juce::String id, name, unit; float lo, hi, def, centre; int dec; int module; };
struct ChoiceDef { juce::String id, name; juce::StringArray items; int def; int module; };
struct BoolDef { juce::String id, name; bool def; int module; };

struct Table
{
    std::vector<FloatDef> floats;
    std::vector<ChoiceDef> choices;
    std::vector<BoolDef> bools;
};

inline const Table& table()
{
    static const Table t = []
    {
        Table t;
        auto U = [] (const char* s) { return juce::String::fromUTF8 (s); };
        auto F = [&t] (juce::String id, juce::String name, float lo, float hi, float def, float centre, juce::String unit, int dec, int m)
        { t.floats.push_back ({ id, name, unit, lo, hi, def, centre, dec, m }); };
        auto C = [&t] (juce::String id, juce::String name, juce::StringArray items, int def, int m) { t.choices.push_back ({ id, name, items, def, m }); };
        auto B = [&t] (juce::String id, juce::String name, bool def, int m) { t.bools.push_back ({ id, name, def, m }); };

        F ("in_gain", "Entrada", -12, 12, 0, 0, "dB", 1, -1);
        B ("gainmatch", "Igualar volumen al comparar", true, -1);
        B ("bypass", "Bypass", false, -1);
        for (int m = 0; m < NumModules; ++m) B (juce::String (modulePrefix (m)) + "_on", moduleName (m) + " activo", m == EQ || m == MAXIMIZER, m);

        const float eqF[8] { 30, 80, 200, 600, 2000, 5000, 10000, 18000 };
        for (int b = 1; b <= 8; ++b)
        {
            const juce::String p = "eq_b" + juce::String (b);
            B (p + "_on", "EQ banda " + juce::String (b), b >= 2 && b <= 7, EQ);
            F (p + "_f", "Frecuencia", 20, 20000, eqF[b - 1], 1000, "Hz", 0, EQ);
            F (p + "_g", "Ganancia", -12, 12, 0, 0, "dB", 1, EQ);
            F (p + "_q", "Q", 0.3f, 8.0f, (b == 1 || b == 2 || b == 7 || b == 8) ? 0.71f : 1.0f, 1.0f, "", 2, EQ);
            C (p + "_ch", "Canal", { U ("Estéreo"), "Mid", "Side" }, 0, EQ);
        }
        const float deqF[4] { 200, 800, 3000, 8000 };
        for (int b = 1; b <= 4; ++b)
        {
            const juce::String p = "deq_b" + juce::String (b);
            B (p + "_on", "EQ dinamico banda " + juce::String (b), true, DEQ);
            F (p + "_f", "Frecuencia", 20, 20000, deqF[b - 1], 1000, "Hz", 0, DEQ);
            F (p + "_q", "Q", 0.3f, 8.0f, 1.5f, 1.0f, "", 2, DEQ);
            F (p + "_thr", "Umbral", -60, 0, -24, -24, "dB", 1, DEQ);
            F (p + "_range", "Rango", -12, 12, -3, 0, "dB", 1, DEQ);
        }
        F ("deq_att", "Ataque", 0.5f, 100, 5, 10, "ms", 1, DEQ);
        F ("deq_rel", "Release", 10, 1000, 100, 150, "ms", 0, DEQ);

        F ("ss_amount", "Cantidad", 0, 20, 4, 5, "", 1, SHAPER);
        F ("ss_detail", "Detalle", 0, 100, 50, 50, "%", 0, SHAPER);
        F ("ss_att", "Ataque", 0.5f, 50, 5, 5, "ms", 1, SHAPER);
        F ("ss_rel", "Release", 5, 500, 80, 60, "ms", 0, SHAPER);
        F ("ss_low", "Desde", 20, 2000, 200, 200, "Hz", 0, SHAPER);
        F ("ss_high", "Hasta", 1000, 20000, 20000, 8000, "Hz", 0, SHAPER);
        C ("ss_mode", "Modo", { "Suave", "Duro" }, 0, SHAPER);

        F ("dyn_x1", "Cruce grave", 40, 600, 150, 150, "Hz", 0, DYN);
        F ("dyn_x2", "Cruce agudo", 800, 12000, 3000, 3000, "Hz", 0, DYN);
        const char* bn[3] { "l", "m", "h" };
        const char* bl[3] { "Graves", "Medios", "Agudos" };
        for (int b = 0; b < 3; ++b)
        {
            const juce::String p = juce::String ("dyn_") + bn[b];
            F (p + "_thr", juce::String (bl[b]) + U (" umbral"), -48, 0, -18, -18, "dB", 1, DYN);
            F (p + "_ratio", juce::String (bl[b]) + " ratio", 1, 10, 1.5f, 2.5f, ":1", 2, DYN);
            F (p + "_att", juce::String (bl[b]) + " ataque", 0.1f, 200, 20, 10, "ms", 1, DYN);
            F (p + "_rel", juce::String (bl[b]) + " release", 10, 2000, 150, 150, "ms", 0, DYN);
            F (p + "_gain", juce::String (bl[b]) + " ganancia", -12, 12, 0, 0, "dB", 1, DYN);
        }
        F ("dyn_mix", "Mezcla", 0, 100, 100, 50, "%", 0, DYN);

        F ("bc_freq", "Frecuencia", 40, 300, 120, 100, "Hz", 0, BASS);
        F ("bc_gain", "Ganancia", -12, 12, 0, 0, "dB", 1, BASS);
        F ("bc_punch", "Golpe", -100, 100, 0, 0, "%", 0, BASS);
        F ("bc_sustain", "Cola", -100, 100, 0, 0, "%", 0, BASS);
        B ("bc_mono", "Graves en mono", true, BASS);
        F ("bc_monofreq", "Mono debajo de", 40, 300, 100, 100, "Hz", 0, BASS);

        F ("imp_x1", "Cruce 1", 40, 600, 120, 120, "Hz", 0, IMPACT);
        F ("imp_x2", "Cruce 2", 300, 4000, 1000, 1000, "Hz", 0, IMPACT);
        F ("imp_x3", "Cruce 3", 2000, 16000, 6000, 6000, "Hz", 0, IMPACT);
        for (int b = 1; b <= 4; ++b) F ("imp_a" + juce::String (b), "Banda " + juce::String (b), -100, 100, 0, 0, "%", 0, IMPACT);

        F ("unl_amount", "Cantidad", 0, 100, 50, 50, "%", 0, UNLIMIT);
        F ("unl_sens", "Sensibilidad", 0, 100, 50, 50, "%", 0, UNLIMIT);

        F ("cla_amount", "Cantidad", 0, 100, 40, 50, "%", 0, CLARITY);
        F ("cla_max", U ("Subida máx"), 0, 12, 6, 6, "dB", 1, CLARITY);
        F ("cla_tilt", U ("Inclinación"), -3, 3, 0, 0, "dB/oct", 1, CLARITY);
        C ("cla_speed", "Velocidad", { "Lenta", "Media", U ("Rápida") }, 1, CLARITY);

        F ("exc_x1", "Cruce grave", 50, 1000, 200, 200, "Hz", 0, EXCITER);
        F ("exc_x2", "Cruce agudo", 1000, 12000, 3000, 3000, "Hz", 0, EXCITER);
        for (int b = 1; b <= 3; ++b)
        {
            F ("exc_a" + juce::String (b), juce::String (bl[b - 1]) + " cantidad", 0, 100, 0, 30, "%", 0, EXCITER);
            C ("exc_m" + juce::String (b), juce::String (bl[b - 1]) + " color", { U ("Cálido"), "Cinta", U ("Válvula"), "Retro" }, b == 3 ? 2 : 0, EXCITER);
        }
        F ("exc_mix", "Mezcla", 0, 100, 100, 50, "%", 0, EXCITER);

        F ("tape_drive", "Drive", 0, 100, 30, 40, "%", 0, TAPE);
        C ("tape_speed", "Velocidad", { "7.5 ips", "15 ips", "30 ips" }, 1, TAPE);
        F ("tape_bias", "Bias", -100, 100, 0, 0, "%", 0, TAPE);
        F ("tape_mix", "Mezcla", 0, 100, 100, 50, "%", 0, TAPE);

        F ("img_x1", "Cruce 1", 40, 600, 150, 150, "Hz", 0, IMAGER);
        F ("img_x2", "Cruce 2", 300, 4000, 1500, 1500, "Hz", 0, IMAGER);
        F ("img_x3", "Cruce 3", 2000, 16000, 7000, 7000, "Hz", 0, IMAGER);
        for (int b = 1; b <= 4; ++b) F ("img_w" + juce::String (b), "Ancho " + juce::String (b), -100, 100, 0, 0, "%", 0, IMAGER);
        F ("img_stereoize", U ("Estereizar"), 0, 100, 0, 30, "%", 0, IMAGER);

        F ("max_gain", "Ganancia", 0, 24, 0, 8, "dB", 1, MAXIMIZER);
        F ("max_ceil", "Techo", -3, 0, -1, -1, "dBTP", 1, MAXIMIZER);
        C ("max_mode", "Modo", { "Transparente", "Punch", "Agresivo", "Clipper" }, 0, MAXIMIZER);
        F ("max_char", U ("Carácter"), 0, 100, 50, 50, "%", 0, MAXIMIZER);
        B ("max_tp", "True peak", true, MAXIMIZER);
        F ("max_link", "Enlace estéreo", 0, 100, 100, 50, "%", 0, MAXIMIZER);

        C ("dit_bits", "Bits", { "16 bits", "24 bits" }, 0, DITHER);
        C ("dit_shape", "Forma del ruido", { "Ninguna", "Suave", "Fuerte" }, 1, DITHER);
        return t;
    }();
    return t;
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;
    for (const auto& f : table().floats)
    {
        NormalisableRange<float> r (f.lo, f.hi);
        if (f.centre > f.lo && f.centre < f.hi && f.lo >= 0.0f) r.setSkewForCentre (f.centre);
        const auto unit = f.unit; const int dec = f.dec;
        auto toText = [unit, dec] (float v, int) { return String (v, dec) + (unit.isNotEmpty() ? " " + unit : String()); };
        auto fromText = [] (const String& s) { return s.retainCharacters ("-0123456789.").getFloatValue(); };
        ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { f.id, 1 }, f.name, r, f.def,
                      AudioParameterFloatAttributes().withLabel (f.unit).withStringFromValueFunction (toText).withValueFromStringFunction (fromText)));
    }
    for (const auto& c : table().choices) ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { c.id, 1 }, c.name, c.items, c.def));
    for (const auto& b : table().bools) ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { b.id, 1 }, b.name, b.def));
    return { ps.begin(), ps.end() };
}
} // namespace tmaster
