#pragma once
#include <vector>
#include <utility>

// Datos de TOP MIC COLLECTION.
// Curvas de respuesta en campo lejano (~1 m, en eje, patron nativo), en dB relativos a 1 kHz. Se armaron con las
// graficas y descripciones publicadas (hojas de datos de los fabricantes y pruebas de Sound On Sound, Mix, Tape Op,
// bonedo y otras); la carta exacta de cada unidad varia +-2 dB entre ejemplares, como en el equipo real.
// Los nombres de los modelos son propios; cada uno se inspira en el clasico indicado en "desc".
namespace tmic
{
using Curve = std::vector<std::pair<double, double>>;   // (Hz, dB)
enum Pattern { Omni = 0, Wide, Cardioid, Hyper, Figure8, NumPatterns };
enum Look { TubeLdc = 0, FetLdc, GoldTube, StageDyn, BroadcastDyn, LongDyn, RibbonModern, RibbonVintage, RibbonBbc };
enum Amp { NoAmp = 0, Triode, Pentode, Fet };

struct Model
{
    const char* name;
    const char* desc;
    const char* kind;        // texto corto: "VALVULA", "FET", ...
    int look;                // dibujo
    int variant;             // acabado/forma dentro del dibujo
    Curve curve;             // respuesta medida (patron nativo)
    unsigned patterns;       // patrones disponibles (bits)
    int defPattern;
    bool dualDiaphragm;      // multipatron de doble membrana (cambia la curva con el patron)
    double kp;               // fuerza de la proximidad (1 = gradiente de presion puro; Variable-D ~0.15)
    double radius;           // radio acustico efectivo (m): cuanto agudo pierde fuera de eje
    double offFloor;         // nivel minimo de agudos fuera de eje (difraccion)
    double maxSpl;           // SPL con 0.5 % THD (hoja de datos)
    int amp;                 // tipo de amplificador
    double trans;            // cuanto transformador (0 = sin transformador)
    double cardA;            // coeficiente del patron "cardioide" de este micro (0.5 = cardioide, 0.4 = mas cerrado)
};

inline const std::vector<Model>& models()
{
    const unsigned ALL = 31u, OC = (1u << Omni) | (1u << Cardioid), OCF = OC | (1u << Figure8), C = 1u << Cardioid, F8 = 1u << Figure8;
    static const std::vector<Model> m {
        // 0 ------------------------------------------------------------------- estilo U47 (valvula VF14 + BV8)
        { "TM-V1", "Válvula vintage estilo 47: cuerpo grande, presencia ancha de ~5 dB alrededor de 5 kHz, la rejilla suma brillo en 10 kHz y cae rápido arriba de 15 kHz",
          "VÁLVULA", TubeLdc, 0,
          { { 20, -4.0 }, { 30, -2.0 }, { 40, -1.0 }, { 50, -0.5 }, { 80, 0.0 }, { 100, 0.3 }, { 150, 0.5 }, { 200, 0.5 }, { 300, 0.3 }, { 500, 0.0 }, { 1000, 0.0 },
            { 1500, 0.3 }, { 2000, 0.8 }, { 3000, 2.0 }, { 4000, 3.3 }, { 5000, 4.0 }, { 6000, 3.5 }, { 7000, 2.6 }, { 8000, 2.1 }, { 9000, 2.4 }, { 10000, 3.0 },
            { 11000, 2.5 }, { 12000, 1.0 }, { 13000, -0.5 }, { 14000, -2.0 }, { 15000, -4.0 }, { 16000, -6.5 }, { 18000, -11.0 }, { 20000, -15.0 } },
          OC, Cardioid, true, 1.0, 0.018, 0.18, 117, Pentode, 0.8, 0.5 },
        // 1 ------------------------------------------------------------------- estilo U67 (EF86 + BV12)
        { "TM-V2", "Válvula estilo 67: medios llenos al frente, presencia suave de ~3 dB de 2 a 10 kHz con pico en 12 kHz y corte de graves siempre activo",
          "VÁLVULA", TubeLdc, 1,
          { { 20, -6.0 }, { 30, -3.5 }, { 40, -2.0 }, { 50, -1.2 }, { 60, -0.8 }, { 80, -0.3 }, { 100, 0.0 }, { 200, 0.2 }, { 300, 0.3 }, { 500, 0.35 }, { 700, 0.3 },
            { 1000, 0.0 }, { 1500, 0.3 }, { 2000, 0.8 }, { 3000, 1.5 }, { 4000, 2.0 }, { 5000, 2.3 }, { 6000, 2.3 }, { 7000, 2.2 }, { 8000, 2.3 }, { 9000, 2.6 },
            { 10000, 3.0 }, { 11000, 3.2 }, { 12000, 3.2 }, { 13000, 2.4 }, { 14000, 1.0 }, { 15000, -0.5 }, { 16000, -2.0 }, { 18000, -6.0 }, { 20000, -10.0 } },
          OCF, Cardioid, true, 1.0, 0.017, 0.18, 114, Pentode, 0.7, 0.5 },
        // 2 ------------------------------------------------------------------- estilo U87 Ai (FET + transformador)
        { "TM-F1", "FET estilo 87: plano y equilibrado, un poco de calidez en 100-200 Hz, mordida suave en 2-3 kHz y un pico definido en ~9 kHz; transformador de salida",
          "FET", FetLdc, 0,
          { { 20, -3.0 }, { 30, -1.2 }, { 40, -0.5 }, { 50, -0.2 }, { 80, 0.0 }, { 100, 0.25 }, { 150, 0.45 }, { 200, 0.4 }, { 300, 0.2 }, { 500, 0.0 }, { 1000, 0.0 },
            { 1500, 0.2 }, { 2000, 0.5 }, { 3000, 0.8 }, { 4000, 0.6 }, { 5000, 0.6 }, { 6000, 1.0 }, { 7000, 1.8 }, { 8000, 2.6 }, { 9000, 3.0 }, { 10000, 2.6 },
            { 11000, 1.6 }, { 12000, 0.6 }, { 13000, -0.3 }, { 14000, -1.0 }, { 15000, -1.8 }, { 16000, -2.8 }, { 18000, -5.5 }, { 20000, -9.0 } },
          OCF, Cardioid, true, 1.0, 0.017, 0.18, 117, Fet, 0.55, 0.5 },
        // 3 ------------------------------------------------------------------- estilo C12 (6072 + T14)
        { "TM-V3", "Válvula estilo C12: medios planos, un hueco leve en 4-5 kHz y una joroba aérea de ~4 dB en 8-10 kHz",
          "VÁLVULA", TubeLdc, 2,
          { { 20, -2.0 }, { 30, -1.0 }, { 50, -0.3 }, { 100, 0.0 }, { 200, 0.0 }, { 500, -0.3 }, { 1000, 0.0 }, { 2000, 0.2 }, { 3000, 0.6 }, { 4000, 0.2 }, { 5000, -0.3 },
            { 6000, 0.5 }, { 7000, 1.8 }, { 8000, 3.0 }, { 9000, 3.8 }, { 10000, 4.0 }, { 11000, 3.6 }, { 12000, 3.0 }, { 13000, 2.2 }, { 14000, 1.2 }, { 15000, 0.3 },
            { 16000, -1.0 }, { 18000, -4.0 }, { 20000, -8.0 } },
          ALL, Cardioid, true, 1.0, 0.017, 0.18, 125, Triode, 0.55, 0.5 },
        // 4 ------------------------------------------------------------------- estilo ELA M 251 (6072 + T14/1)
        { "TM-V4", "Válvula estilo 251: medios un poco recogidos, doble pico (+2.4 dB en 3-4 kHz y +3.6 dB en 10 kHz); el transformador recorta un poco el grave",
          "VÁLVULA", GoldTube, 1,
          { { 20, -2.5 }, { 30, -1.2 }, { 50, -0.4 }, { 100, 0.0 }, { 200, -0.3 }, { 500, -0.7 }, { 1000, -0.6 }, { 1500, 0.0 }, { 2000, 0.8 }, { 3000, 2.0 },
            { 4000, 2.4 }, { 5000, 2.0 }, { 6000, 1.2 }, { 7000, 1.2 }, { 8000, 2.2 }, { 9000, 3.2 }, { 10000, 3.6 }, { 11000, 3.2 }, { 12000, 2.4 }, { 13000, 1.4 },
            { 14000, 0.4 }, { 15000, -0.6 }, { 16000, -1.8 }, { 18000, -4.5 }, { 20000, -8.0 } },
          OCF, Cardioid, true, 1.0, 0.017, 0.18, 120, Triode, 0.7, 0.5 },
        // 5 ------------------------------------------------------------------- estilo C-800G (6AU6, enfriado)
        { "TM-V5", "Válvula estilo 800 (hip-hop): medios-altos suaves sin mordida y un refuerzo grande de 8 a 14 kHz (+6 dB): aire sin EQ",
          "VÁLVULA", GoldTube, 0,
          { { 20, -2.0 }, { 30, -1.0 }, { 50, -0.3 }, { 100, 0.0 }, { 200, -0.2 }, { 300, -0.4 }, { 500, -0.5 }, { 1000, -0.3 }, { 2000, -0.4 }, { 3000, -0.6 },
            { 4000, -0.3 }, { 5000, 0.4 }, { 6000, 1.4 }, { 7000, 2.6 }, { 8000, 3.8 }, { 9000, 4.8 }, { 10000, 5.4 }, { 11000, 5.8 }, { 12000, 6.0 }, { 13000, 5.8 },
            { 14000, 5.0 }, { 15000, 3.6 }, { 16000, 2.0 }, { 17000, 0.0 }, { 18000, -2.5 }, { 20000, -8.0 } },
          OC, Cardioid, true, 1.0, 0.018, 0.18, 125, Pentode, 0.3, 0.5 },
        // 6 ------------------------------------------------------------------- estilo C414 XLII
        { "TM-F2", "FET estilo 414 XLII: subida desde 3 kHz hasta ~+4.6 dB en 9-10 kHz (presencia tipo C12), sin transformador, nueve patrones",
          "FET", FetLdc, 1,
          { { 20, -1.5 }, { 30, -0.6 }, { 40, -0.3 }, { 50, -0.1 }, { 100, 0.0 }, { 200, -0.2 }, { 500, -0.3 }, { 1000, 0.0 }, { 2000, 0.3 }, { 3000, 1.0 }, { 4000, 1.8 },
            { 5000, 2.5 }, { 6000, 3.2 }, { 7000, 3.8 }, { 8000, 4.3 }, { 9000, 4.6 }, { 10000, 4.6 }, { 11000, 4.4 }, { 12000, 4.0 }, { 13000, 3.5 }, { 14000, 3.0 },
            { 15000, 2.4 }, { 16000, 1.6 }, { 18000, -0.5 }, { 20000, -3.0 } },
          ALL, Cardioid, true, 1.0, 0.016, 0.2, 140, Fet, 0.0, 0.5 },
        // 7 ------------------------------------------------------------------- estilo TLM 103
        { "TM-F3", "FET estilo 103: plano hasta 5 kHz y una meseta ancha de +4 dB hasta 15 kHz; grave que baja suave bajo 60 Hz; sin transformador, muy limpio",
          "FET", FetLdc, 2,
          { { 20, -3.5 }, { 30, -2.0 }, { 40, -1.2 }, { 50, -0.6 }, { 60, -0.3 }, { 80, -0.1 }, { 100, 0.0 }, { 1000, 0.0 }, { 2000, 0.0 }, { 3000, 0.1 }, { 4000, 0.4 },
            { 5000, 1.0 }, { 6000, 2.0 }, { 7000, 2.9 }, { 8000, 3.5 }, { 9000, 3.9 }, { 10000, 4.0 }, { 11000, 4.0 }, { 12000, 3.9 }, { 13000, 3.6 }, { 14000, 3.0 },
            { 15000, 2.0 }, { 16000, 0.0 }, { 18000, -4.5 }, { 20000, -9.0 } },
          C, Cardioid, false, 1.0, 0.016, 0.2, 138, Fet, 0.0, 0.5 },
        // 8 ------------------------------------------------------------------- estilo SM57
        { "TM-D1", "Dinámico estilo 57: graves recortados lejos (la proximidad los devuelve de cerca), leve hueco en 400 Hz y subida de presencia de +5.5 dB en 6 kHz",
          "DINÁMICO", StageDyn, 0,
          { { 20, -16.0 }, { 30, -12.0 }, { 40, -9.0 }, { 50, -7.0 }, { 60, -5.5 }, { 80, -3.5 }, { 100, -2.5 }, { 150, -1.0 }, { 200, -0.5 }, { 300, -0.3 }, { 400, -0.6 },
            { 500, -0.4 }, { 700, -0.2 }, { 1000, 0.0 }, { 1500, 0.5 }, { 2000, 1.2 }, { 3000, 2.5 }, { 4000, 3.8 }, { 5000, 5.0 }, { 6000, 5.5 }, { 7000, 4.5 },
            { 8000, 3.0 }, { 9000, 2.6 }, { 10000, 2.0 }, { 11000, 0.5 }, { 12000, -1.5 }, { 13000, -3.5 }, { 14000, -6.0 }, { 15000, -9.0 }, { 16000, -12.0 },
            { 18000, -18.0 }, { 20000, -24.0 } },
          C, Cardioid, false, 0.45, 0.010, 0.25, 150, NoAmp, 0.25, 0.5 },
        // 9 ------------------------------------------------------------------- estilo SM7B (respuesta plana)
        { "TM-D2", "Dinámico de radio estilo SM7: plano y suave de 50 Hz a 20 kHz, una subida leve en 4-6 kHz y agudos que bajan despacio",
          "DINÁMICO", BroadcastDyn, 0,
          { { 20, -10.0 }, { 30, -6.0 }, { 40, -3.5 }, { 50, -2.2 }, { 60, -1.5 }, { 80, -0.6 }, { 100, -0.2 }, { 200, 0.0 }, { 300, 0.2 }, { 500, 0.2 }, { 1000, 0.0 },
            { 2000, 0.4 }, { 3000, 1.0 }, { 4000, 1.6 }, { 5000, 2.0 }, { 6000, 2.0 }, { 7000, 1.6 }, { 8000, 1.0 }, { 9000, 0.4 }, { 10000, 0.0 }, { 11000, -0.6 },
            { 12000, -1.4 }, { 13000, -2.0 }, { 14000, -2.8 }, { 15000, -3.6 }, { 16000, -4.6 }, { 18000, -7.0 }, { 20000, -10.0 } },
          C, Cardioid, false, 0.7, 0.013, 0.25, 150, NoAmp, 0.1, 0.5 },
        // 10 ------------------------------------------------------------------ estilo RE20 (Variable-D)
        { "TM-D3", "Dinámico de locución estilo RE20: plano de 80 Hz a 6 kHz, casi sin efecto de proximidad (Variable-D) y agudos que caen arriba de 7 kHz",
          "DINÁMICO", LongDyn, 0,
          { { 20, -12.0 }, { 30, -7.0 }, { 40, -4.0 }, { 45, -3.0 }, { 50, -2.3 }, { 60, -1.4 }, { 80, -0.3 }, { 100, 0.0 }, { 1000, 0.0 }, { 2000, 0.0 }, { 3000, 0.2 },
            { 4000, 0.4 }, { 5000, 0.4 }, { 6000, 0.0 }, { 7000, -0.6 }, { 8000, -1.2 }, { 9000, -1.6 }, { 10000, -2.0 }, { 11000, -2.5 }, { 12000, -3.2 },
            { 13000, -4.0 }, { 14000, -5.0 }, { 15000, -6.0 }, { 16000, -7.5 }, { 18000, -10.0 }, { 20000, -14.0 } },
          C, Cardioid, false, 0.15, 0.014, 0.25, 150, NoAmp, 0.1, 0.5 },
        // 11 ------------------------------------------------------------------ estilo R-121
        { "TM-R1", "Cinta moderna estilo 121: plana de 30 Hz a 15 kHz, la zona de 80 Hz a 1 kHz ~1 dB abajo y agudos un par de dB bajos arriba de 7 kHz",
          "CINTA", RibbonModern, 0,
          { { 20, -3.0 }, { 30, -1.0 }, { 40, -0.2 }, { 50, 0.3 }, { 60, 0.3 }, { 80, -0.8 }, { 100, -1.0 }, { 200, -1.0 }, { 500, -1.0 }, { 1000, -0.8 }, { 2000, 0.0 },
            { 3000, 0.5 }, { 4000, 0.5 }, { 5000, 0.0 }, { 6000, -0.6 }, { 7000, -1.2 }, { 8000, -1.8 }, { 9000, -2.2 }, { 10000, -2.6 }, { 11000, -3.0 },
            { 12000, -3.5 }, { 13000, -4.2 }, { 14000, -5.0 }, { 15000, -6.0 }, { 16000, -7.5 }, { 18000, -10.5 }, { 20000, -14.0 } },
          F8, Figure8, false, 1.0, 0.007, 0.3, 135, NoAmp, 0.4, 0.0 },
        // 12 ------------------------------------------------------------------ estilo RCA 44
        { "TM-R2", "Cinta vintage estilo 44: grave redondo, plano de 50 Hz a ~5 kHz y luego una caída larga (~-10 dB en 15 kHz); proximidad enorme",
          "CINTA", RibbonVintage, 0,
          { { 20, -6.0 }, { 30, -3.0 }, { 40, -1.5 }, { 50, -0.5 }, { 80, 0.5 }, { 100, 0.8 }, { 150, 1.0 }, { 200, 1.0 }, { 300, 0.8 }, { 500, 0.4 }, { 1000, 0.0 },
            { 2000, -0.3 }, { 3000, -0.6 }, { 4000, -1.0 }, { 5000, -1.5 }, { 6000, -2.2 }, { 7000, -3.0 }, { 8000, -3.8 }, { 9000, -4.8 }, { 10000, -5.8 },
            { 11000, -7.0 }, { 12000, -8.0 }, { 13000, -9.5 }, { 14000, -11.0 }, { 15000, -12.5 }, { 16000, -15.0 }, { 18000, -20.0 }, { 20000, -25.0 } },
          F8, Figure8, false, 1.0, 0.012, 0.3, 130, NoAmp, 0.6, 0.0 },
        // 13 ------------------------------------------------------------------ estilo 4038
        { "TM-R3", "Cinta británica estilo 4038: muy plana de 30 Hz a 15 kHz con agudos que bajan de a poco (cortesía de la BBC)",
          "CINTA", RibbonBbc, 0,
          { { 20, -3.0 }, { 30, -1.0 }, { 40, -0.4 }, { 50, 0.0 }, { 100, 0.2 }, { 200, 0.2 }, { 500, 0.0 }, { 1000, 0.0 }, { 2000, -0.2 }, { 3000, -0.3 }, { 4000, -0.5 },
            { 5000, -0.8 }, { 6000, -1.0 }, { 7000, -1.4 }, { 8000, -1.8 }, { 9000, -2.2 }, { 10000, -2.6 }, { 11000, -3.0 }, { 12000, -3.6 }, { 13000, -4.4 },
            { 14000, -5.4 }, { 15000, -6.5 }, { 16000, -8.5 }, { 18000, -13.0 }, { 20000, -18.0 } },
          F8, Figure8, false, 1.0, 0.010, 0.3, 125, NoAmp, 0.4, 0.0 },
        // 14 ------------------------------------------------------------------ estilo Manley Reference Cardioid
        { "TM-V6", "Válvula de referencia estilo Manley: medios ricos al frente y ~+6 dB de 10 a 14 kHz (resonancia de diafragma grueso); triodos",
          "VÁLVULA", TubeLdc, 3,
          { { 20, -1.0 }, { 30, -0.5 }, { 50, 0.0 }, { 100, 0.3 }, { 200, 0.3 }, { 300, 0.0 }, { 500, -0.3 }, { 1000, 0.0 }, { 2000, 0.6 }, { 3000, 1.6 }, { 4000, 1.8 },
            { 5000, 1.6 }, { 6000, 1.8 }, { 7000, 2.6 }, { 8000, 3.6 }, { 9000, 4.6 }, { 10000, 5.4 }, { 11000, 5.8 }, { 12000, 6.0 }, { 13000, 6.0 }, { 14000, 5.8 },
            { 15000, 5.2 }, { 16000, 4.4 }, { 18000, 2.5 }, { 20000, 0.0 } },
          C, Cardioid, false, 1.0, 0.018, 0.18, 130, Triode, 0.5, 0.5 },
        // 15 ------------------------------------------------------------------ estilo U47 fet (nuevo)
        { "TM-F4", "FET estilo 47 fet: grave grande y limpio (bombo, bajo), leve subida en 2 kHz, agudos cerrados hasta 16 kHz; más directivo arriba de 500 Hz",
          "FET", FetLdc, 3,
          { { 20, -6.0 }, { 30, -3.0 }, { 40, -1.5 }, { 50, -0.8 }, { 60, -0.4 }, { 80, 0.0 }, { 100, 0.3 }, { 150, 0.5 }, { 200, 0.4 }, { 300, 0.2 }, { 500, 0.0 },
            { 1000, 0.0 }, { 1500, 0.6 }, { 2000, 1.2 }, { 2500, 1.3 }, { 3000, 1.0 }, { 4000, 0.6 }, { 5000, 0.8 }, { 6000, 1.0 }, { 7000, 0.6 }, { 8000, 0.2 },
            { 9000, 0.4 }, { 10000, 0.6 }, { 11000, 0.0 }, { 12000, -1.0 }, { 13000, -2.2 }, { 14000, -3.5 }, { 15000, -5.0 }, { 16000, -7.0 }, { 18000, -11.0 },
            { 20000, -15.0 } },
          C, Cardioid, false, 1.0, 0.019, 0.16, 137, Fet, 0.5, 0.4 } };
    return m;
}

// microfonos con los que se puede haber grabado (su color se quita)
struct Source { const char* name; const char* shortName; Curve curve; double a; double kp; };
inline const std::vector<Source>& sources()
{
    static const std::vector<Source> s {
        { "Plano / neutro", "FLAT", {}, 0.5, 1.0 },
        { "Dinámico de escenario (tipo 58)", "DYN", { { 20, -15 }, { 30, -11 }, { 40, -8 }, { 50, -6 }, { 60, -4.8 }, { 80, -3 }, { 100, -2 }, { 150, -0.8 }, { 200, -0.3 },
            { 300, 0 }, { 1000, 0 }, { 2000, 1 }, { 3000, 2.6 }, { 4000, 4 }, { 5000, 4.6 }, { 6000, 4.4 }, { 7000, 3.4 }, { 8000, 2.4 }, { 9000, 2 }, { 10000, 1 },
            { 11000, -0.5 }, { 12000, -2.5 }, { 13000, -4.5 }, { 14000, -7 }, { 15000, -10 }, { 16000, -13 }, { 18000, -19 }, { 20000, -25 } }, 0.5, 0.45 },
        { "Dinámico de radio (tipo SM7)", "BCST", models()[9].curve, 0.5, 0.7 },
        { "Condensador económico (tipo 2020)", "LDC", { { 20, -4 }, { 30, -2.5 }, { 50, -1.6 }, { 70, -1.8 }, { 100, -1.4 }, { 200, -0.6 }, { 300, -0.2 }, { 500, 0 }, { 1000, 0 },
            { 1500, 0.4 }, { 2000, 0.8 }, { 3000, 0.6 }, { 4000, 0.8 }, { 5000, 1.8 }, { 6000, 2.6 }, { 7000, 3 }, { 8000, 3.2 }, { 9000, 3.4 }, { 10000, 3.6 },
            { 12000, 3.6 }, { 14000, 3.2 }, { 15000, 2.6 }, { 16000, 1.6 }, { 18000, -1 }, { 20000, -4 } }, 0.5, 1.0 },
        { "Micrófono USB", "USB", { { 20, -8 }, { 30, -5 }, { 50, -3 }, { 100, -1.5 }, { 200, -0.5 }, { 500, 0 }, { 1000, 0 }, { 2000, 0.8 }, { 3000, 2 }, { 4000, 3 },
            { 5000, 3.4 }, { 6000, 3.4 }, { 8000, 3 }, { 10000, 2.4 }, { 12000, 1.2 }, { 14000, -0.5 }, { 16000, -3 }, { 18000, -7 }, { 20000, -12 } }, 0.5, 0.8 },
        { "Celular / laptop", "PHONE", { { 20, -30 }, { 50, -20 }, { 100, -10 }, { 150, -5 }, { 200, -2.5 }, { 300, -0.8 }, { 500, 0 }, { 1000, 0 }, { 2000, 2 }, { 2500, 3 },
            { 3000, 2.6 }, { 4000, 1.5 }, { 5000, 0.5 }, { 6000, -1 }, { 7000, -3 }, { 8000, -5.5 }, { 9000, -9 }, { 10000, -13 }, { 12000, -20 }, { 15000, -30 },
            { 20000, -40 } }, 1.0, 0.0 },
        { "Condensador de diafragma pequeño", "SDC", { { 20, -3 }, { 30, -1.6 }, { 50, -0.8 }, { 100, -0.3 }, { 200, 0 }, { 1000, 0 }, { 2000, 0 }, { 4000, 0.5 }, { 6000, 1.2 },
            { 8000, 1.8 }, { 10000, 2.2 }, { 12000, 2.0 }, { 14000, 1.4 }, { 16000, 0.6 }, { 18000, -1 }, { 20000, -3 } }, 0.5, 1.0 },
        // micros del usuario (los indices no cambian para no romper sesiones guardadas)
        { "Neumann TLM 103", "TLM103", models()[7].curve, 0.5, 1.0 },
        { "AKG C414 XLII", "C414", models()[6].curve, 0.5, 1.0 },
        { "Neumann U 87 Ai", "U87AI", models()[2].curve, 0.5, 1.0 } };
    return s;
}
} // namespace tmic
