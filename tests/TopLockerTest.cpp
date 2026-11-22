// Pruebas de Top Locker
#include "TopLockerProcessor.h"
#include "TopLockerEditor.h"
#include <cstdio>

static int fails = 0;
static void report (const char* n, bool ok, const juce::String& info) { if (! ok) ++fails; std::printf ("%-50s %s  %s\n", n, info.toRawUTF8(), ok ? "OK" : "FALLO"); }
static const double SR = 48000.0;
static const double TWO_PI = 6.283185307179586;
static void setP (TopLockerProcessor& p, const juce::String& id, float v) { auto* q = p.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); }
static void clean (TopLockerProcessor& p) { setP (p, "pre", 0); setP (p, "character", 0); setP (p, "sat", 0); setP (p, "room", 0); }
static std::vector<float> run (TopLockerProcessor& p, double hz, float amp = 0.05f, double secs = 0.8)
{
    p.setPlayConfigDetails (2, 2, SR, 512); p.prepareToPlay (SR, 512);
    juce::AudioBuffer<float> b (2, 512); std::vector<float> out; double ph = 0;
    for (int blk = 0; blk < (int) (SR * secs / 512); ++blk)
    {
        for (int i = 0; i < 512; ++i) { const float v = amp * (float) std::sin (ph); ph += TWO_PI * hz / SR; b.setSample (0, i, v); b.setSample (1, i, v); }
        juce::MidiBuffer m; p.processBlock (b, m);
        for (int i = 0; i < 512; ++i) out.push_back (b.getSample (0, i));
    }
    return out;
}
static float level (const std::vector<float>& x, double hz)
{
    double re = 0, im = 0; const int a = (int) (0.45 * SR), b = (int) (0.75 * SR);
    for (int i = a; i < b; ++i) { const double w = 0.5 - 0.5 * std::cos (TWO_PI * (i - a) / (b - a)); re += x[(size_t) i] * w * std::cos (TWO_PI * hz * i / SR); im += x[(size_t) i] * w * std::sin (TWO_PI * hz * i / SR); }
    return (float) (20 * std::log10 (std::sqrt (re * re + im * im) / (b - a) * 4 + 1e-12));
}
static float resp (TopLockerProcessor& p, double hz, float amp = 0.05f) { return level (run (p, hz, amp), hz) - 20.0f * std::log10 (amp); }
template <typename F> static float with (F setup, double hz, float amp = 0.05f) { TopLockerProcessor p; clean (p); setup (p); return resp (p, hz, amp); }

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc > 2 && juce::String (argv[1]) == "--mics")
    {
        const int cw = 260, chh = 560;
        juce::Image img (juce::Image::RGB, cw * 6, chh * 2, true);
        juce::Graphics g (img);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a1a10), 0, 0, juce::Colour (0xff0a0705), 0, (float) chh, false)); g.fillAll();
        for (int i = 0; i < 12; ++i)
            tlui::drawMic (g, juce::Rectangle<float> ((float) (i % 6) * cw + 10, (float) (i / 6) * chh + 10, (float) cw - 20, (float) chh - 30), i, true);
        juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
        f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
        return 0;
    }
    if (argc > 2 && juce::String (argv[1]) == "--snapshot")
    {
        TopLockerProcessor p;
        if (argc > 3) setP (p, "mic", juce::String (argv[3]).getFloatValue());
        if (argc > 5) setP (p, "room", juce::String (argv[5]).getFloatValue());
        run (p, 220, 0.3f, 0.3);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        if (argc > 4) ed->setSize (juce::String (argv[4]).getIntValue(), juce::String (argv[4]).getIntValue() * 900 / 1280);
        if (argc > 6) { setP (p, "angle", 30); setP (p, "prox", 30); setP (p, "high", 4); setP (p, "low", -2); setP (p, "highFreq", 3); }
        ed->setVisible (true);
        if (auto* te = dynamic_cast<TopLockerEditor*> (ed.get())) { p.inPeak = 0.3f; p.outPeak = 0.5f; te->tickNow(); }
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
        f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("snapshot -> %s\n", f.getFullPathName().toRawUTF8());
        return 0;
    }
    const int N = (int) lk::mics().size();
    // 12 microfonos estables con ajustes por defecto (preamp, sala, saturacion)
    {
        bool ok = true; juce::String info;
        for (int m = 0; m < N; ++m)
        {
            TopLockerProcessor p; setP (p, "mic", (float) m); auto o = run (p, 1000, 0.3f);
            float mx = 0; bool fin = true; for (float v : o) { mx = std::max (mx, std::abs (v)); fin = fin && std::isfinite (v); }
            if (! fin || mx < 0.08f || mx > 1.2f) { ok = false; info << m << " "; }
        }
        report ("12 microfonos estables", ok && N == 12, juce::String (N) + " modelos " + info);
    }
    // cada microfono tiene su propia respuesta (no hay dos iguales)
    {
        const double fr[] { 80, 200, 500, 1000, 3000, 6000, 9000, 12000 };
        std::vector<std::vector<float>> r ((size_t) N);
        for (int m = 0; m < N; ++m) for (double f : fr) r[(size_t) m].push_back (with ([m] (TopLockerProcessor& p) { setP (p, "mic", (float) m); }, f));
        float minD = 99;
        for (int a = 0; a < N; ++a) for (int b = a + 1; b < N; ++b)
        {
            float d = 0; for (size_t k = 0; k < r[0].size(); ++k) d = std::max (d, std::abs (r[(size_t) a][k] - r[(size_t) b][k]));
            minD = std::min (minD, d);
        }
        report ("Respuesta propia en cada microfono", minD > 1.0f, "diferencia minima entre dos micros " + juce::String (minD, 1) + " dB");
        const float ribbonTilt = r[8][6] - r[8][3], crownTilt = r[3][6] - r[3][3];
        report ("Cinta oscura / valvula hip-hop brillante", ribbonTilt < -4 && crownTilt > 3, juce::String (ribbonTilt, 1) + " / " + juce::String (crownTilt, 1) + " dB (9k vs 1k)");
    }
    // micro de origen: grabado con TLM 103 y elegido Modern Silver = plano
    {
        float worst = 0;
        for (double hz : { 200.0, 1000.0, 5000.0, 8000.0, 12000.0 })
            worst = std::max (worst, std::abs (with ([] (TopLockerProcessor& p) { setP (p, "mic", 1); setP (p, "source", 7); }, hz)));
        report ("SOURCE TLM 103 + Modern Silver = plano", worst < 0.6f, "desvio max " + juce::String (worst, 2) + " dB");
        const float a = with ([] (TopLockerProcessor& p) { setP (p, "mic", 2); }, 8500), b = with ([] (TopLockerProcessor& p) { setP (p, "mic", 2); setP (p, "source", 8); }, 8500);
        report ("SOURCE C414 XLII + Gold Detail = plano", std::abs (b) < 0.8f && a > 3.0f, "con origen " + juce::String (b, 2) + " dB, sin origen " + juce::String (a, 1) + " dB");
    }
    // armonicos: valvula = pares, dinamico/cinta = impares
    {
        auto harm = [] (int mic, float& h2, float& h3)
        {
            TopLockerProcessor p; clean (p); setP (p, "mic", (float) mic); setP (p, "character", 100); setP (p, "sat", 80);
            auto o = run (p, 200, 0.5f);
            const float f = level (o, 200); h2 = level (o, 400) - f; h3 = level (o, 600) - f;
        };
        float t2, t3, d2, d3, r2, r3; harm (10, t2, t3); harm (6, d2, d3); harm (7, r2, r3);
        report ("Valvula: armonicos pares (2do > 3ro)", t2 > t3 && t2 > -50, "H2 " + juce::String (t2, 1) + " dB, H3 " + juce::String (t3, 1) + " dB");
        report ("Dinamico: armonicos impares (3ro > 2do)", d3 > d2, "H2 " + juce::String (d2, 1) + " dB, H3 " + juce::String (d3, 1) + " dB");
        report ("Cinta: saturacion de transformador (impar)", r3 > r2, "H2 " + juce::String (r2, 1) + " dB, H3 " + juce::String (r3, 1) + " dB");
        TopLockerProcessor q; clean (q); setP (q, "mic", 10); auto o = run (q, 200, 0.5f);
        const float h = level (o, 400) - level (o, 200);
        report ("Clean (character 0) sin armonicos", h < -70, "H2 " + juce::String (h, 1) + " dB");
    }
    // transientes: condensador rapido vs dinamico
    {
        auto onset = [] (int mic)
        {
            TopLockerProcessor p; clean (p); setP (p, "mic", (float) mic);
            p.setPlayConfigDetails (2, 2, SR, 512); p.prepareToPlay (SR, 512);
            juce::AudioBuffer<float> b (2, 512); std::vector<float> out; long n = 0;
            for (int blk = 0; blk < 40; ++blk)
            {
                for (int i = 0; i < 512; ++i, ++n)
                {
                    const double t = (double) n / SR - 0.1;
                    const float v = t < 0 ? 0.0f : (float) (0.05 * std::sin (TWO_PI * 1000 * t) * ((int) (t / 0.25) % 2 == 0 ? 1.0 : 0.0));
                    b.setSample (0, i, v); b.setSample (1, i, v);
                }
                juce::MidiBuffer m; p.processBlock (b, m);
                for (int i = 0; i < 512; ++i) out.push_back (b.getSample (0, i));
            }
            float pk = 0, st = 0;
            for (int i = (int) (0.1 * SR); i < (int) (0.106 * SR); ++i) pk = std::max (pk, std::abs (out[(size_t) i]));
            for (int i = (int) (0.25 * SR); i < (int) (0.3 * SR); ++i) st = std::max (st, std::abs (out[(size_t) i]));
            return 20.0f * std::log10 (pk / st);
        };
        const float cond = onset (2), dyn = onset (6);
        report ("Transientes: condensador > dinamico", cond - dyn > 1.0f, "ataque " + juce::String (cond, 1) + " dB vs " + juce::String (dyn, 1) + " dB");
    }
    // comportamiento dinamico: la valvula comprime suave en niveles altos
    {
        auto gr = [] (int mic) { return with ([mic] (TopLockerProcessor& p) { setP (p, "mic", (float) mic); }, 1000, 0.01f) - with ([mic] (TopLockerProcessor& p) { setP (p, "mic", (float) mic); }, 1000, 0.7f); };
        const float tube = gr (10), modern = gr (1);
        report ("Compresion natural: valvula si, moderno no", tube > 1.0f && modern < 0.3f, juce::String (tube, 1) + " dB vs " + juce::String (modern, 1) + " dB");
    }
    // proximidad y posicion
    {
        auto at = [] (int mic, float prox, double hz) { return with ([mic, prox] (TopLockerProcessor& p) { setP (p, "mic", (float) mic); setP (p, "prox", prox); }, hz); };
        const float rib = at (7, 0, 100) - at (7, 50, 100), voice = at (11, 0, 100) - at (11, 50, 100), far = at (0, 100, 100) - at (0, 50, 100);
        report ("Proximidad: cinta mucho, locucion poco", rib > 6 && voice < 3 && voice > 0, "cinta +" + juce::String (rib, 1) + " dB, locucion +" + juce::String (voice, 1) + " dB");
        report ("Proximidad lejos: menos graves", far < -1, juce::String (far, 1) + " dB");
        auto ang = [] (float v, double hz) { return with ([v] (TopLockerProcessor& p) { setP (p, "mic", 0); setP (p, "angle", v); }, hz); };
        const float a45 = ang (45, 8000) - ang (0, 8000), a90 = ang (90, 8000) - ang (0, 8000), lvl90 = ang (90, 500) - ang (0, 500);
        report ("Angulo: fuera de eje pierde agudos y nivel", a45 < -1.5f && a90 < a45 - 2 && lvl90 < -1, "45: " + juce::String (a45, 1) + " dB, 90: " + juce::String (a90, 1) + " dB (8k), nivel " + juce::String (lvl90, 1));
    }
    // circuitos de caracter: Tube pares, Transformer impares en graves, Clean nada
    {
        auto harm = [] (int mode, float& h2, float& h3)
        {
            TopLockerProcessor p; clean (p); setP (p, "mic", 11); setP (p, "charMode", (float) mode); setP (p, "character", 100); setP (p, "sat", 80);
            auto o = run (p, 100, 0.5f);
            const float f = level (o, 100); h2 = level (o, 200) - f; h3 = level (o, 300) - f;
        };
        float t2, t3, x2, x3, c2, c3, a2, a3;
        harm (lk::ChTube, t2, t3); harm (lk::ChTransformer, x2, x3); harm (lk::ChClean, c2, c3); harm (lk::ChAnalog, a2, a3);
        report ("Character Tube: armonicos pares", t2 > t3 && t2 > -40, "H2 " + juce::String (t2, 1) + " H3 " + juce::String (t3, 1));
        report ("Character Transformer: impares en graves", x3 > x2 && x3 > -45, "H2 " + juce::String (x2, 1) + " H3 " + juce::String (x3, 1));
        report ("Character Analog: mezcla par/impar", a2 > -60 && a3 > -60, "H2 " + juce::String (a2, 1) + " H3 " + juce::String (a3, 1));
        report ("Character Clean: sin armonicos de color", c2 < -60 && c3 < -50, "H2 " + juce::String (c2, 1) + " H3 " + juce::String (c3, 1));
    }
    // tono con frecuencia y tipo seleccionables
    {
        auto t = [] (int lf, int type, double hz) { return with ([lf, type] (TopLockerProcessor& p) { setP (p, "mic", 1); setP (p, "low", 8); setP (p, "lowFreq", (float) lf); setP (p, "lowType", (float) type); }, hz)
                                                   - with ([] (TopLockerProcessor& p) { setP (p, "mic", 1); }, hz); };
        const float bell300 = t (3, 1, 300), bell300at60 = t (3, 1, 60), shelf60 = t (0, 0, 40);
        const float mid5k = with ([] (TopLockerProcessor& p) { setP (p, "mic", 1); setP (p, "mid", 8); setP (p, "midFreq", 5); }, 5000) - with ([] (TopLockerProcessor& p) { setP (p, "mic", 1); }, 5000);
        report ("Tone: frecuencia y tipo seleccionables", bell300 > 6 && bell300at60 < 3 && shelf60 > 6 && mid5k > 6,
                "bell300 " + juce::String (bell300, 1) + " (60Hz " + juce::String (bell300at60, 1) + "), shelf60 " + juce::String (shelf60, 1) + ", mid5k " + juce::String (mid5k, 1));
    }
    // preamps: color y nivel parecido en senal baja
    {
        bool ok = true; juce::String info;
        for (int pr = 1; pr < (int) lk::preamps().size(); ++pr)
        {
            TopLockerProcessor p; clean (p); setP (p, "mic", 1); setP (p, "pre", (float) pr); setP (p, "preGain", 10); setP (p, "sat", 60);
            auto o = run (p, 200, 0.5f);
            const float f = level (o, 200), thd = std::max (level (o, 400), level (o, 600)) - f, lvl = with ([pr] (TopLockerProcessor& q) { setP (q, "mic", 1); setP (q, "pre", (float) pr); }, 1000);
            info << lk::preamps()[(size_t) pr].name << " " << juce::String (thd, 0) << "dB ";
            if (thd < -60 || std::abs (lvl) > 3) ok = false;
        }
        report ("Preamps con armonicos y nivel estable", ok && lk::preamps().size() == 7, info);
        auto thd = [] (int pr, float in)
        {
            TopLockerProcessor p; clean (p); setP (p, "mic", 1); setP (p, "pre", (float) pr); setP (p, "preGain", 5); setP (p, "preInput", in);
            auto o = run (p, 200, 0.2f); const float f = level (o, 200);
            return std::pair<float, float> (std::max (level (o, 400), level (o, 600)) - f, f - 20.0f * std::log10 (0.2f));
        };
        const auto lo = thd (1, 0), hi = thd (1, 9);
        report ("Input: mas entrada = mas saturacion, mismo nivel", hi.first > lo.first + 4 && std::abs (hi.second - lo.second) < 3.0f,
                "THD " + juce::String (lo.first, 1) + " -> " + juce::String (hi.first, 1) + " dB, nivel " + juce::String (hi.second - lo.second, 1) + " dB");
        // cada preamp tiene un perfil distinto (armonicos 2 y 3)
        std::vector<std::pair<float, float>> prof; bool distinct = true;
        for (int pr = 1; pr < (int) lk::preamps().size(); ++pr)
        {
            TopLockerProcessor p; clean (p); setP (p, "mic", 1); setP (p, "pre", (float) pr); setP (p, "preGain", 8); setP (p, "preInput", 6);
            auto o = run (p, 150, 0.4f); const float f = level (o, 150);
            prof.push_back ({ level (o, 300) - f, level (o, 450) - f });
        }
        for (size_t a = 0; a < prof.size(); ++a) for (size_t b = a + 1; b < prof.size(); ++b)
            if (std::abs (prof[a].first - prof[b].first) < 1.0f && std::abs (prof[a].second - prof[b].second) < 1.0f) distinct = false;
        report ("Cada preamp suena distinto", distinct, juce::String ((int) prof.size()) + " perfiles");
    }
    // caracter vintage mas oscuro, smooth quita dureza
    {
        auto ch = [] (int mode, double hz) { return with ([mode] (TopLockerProcessor& p) { setP (p, "mic", 0); setP (p, "charMode", (float) mode); setP (p, "character", 100); }, hz); };
        const float v = ch (1, 12000) - ch (0, 12000), s = ch (2, 3500) - ch (0, 3500);
        report ("Character: Vintage oscuro, Smooth suave", v < -3 && s < -1.5f, juce::String (v, 1) + " / " + juce::String (s, 1) + " dB");
    }
    // tono
    {
        auto t = [] (const char* id, double hz) { return with ([id] (TopLockerProcessor& p) { setP (p, "mic", 1); setP (p, id, 6); }, hz) - with ([] (TopLockerProcessor& p) { setP (p, "mic", 1); }, hz); };
        const float l = t ("low", 60), m = t ("mid", 1200), h = t ("high", 15000);
        report ("Tone Low/Mid/High", l > 4.5f && m > 5 && h > 4.5f, juce::String (l, 1) + " / " + juce::String (m, 1) + " / " + juce::String (h, 1) + " dB");
    }
    // sala: cola despues del sonido, mas en sala grande, y la cinta capta mas sala que el dinamico
    {
        auto tail = [] (int room, int mic, float decay = 50)
        {
            TopLockerProcessor p; clean (p); setP (p, "mic", (float) mic); setP (p, "room", (float) room); setP (p, "roomAmt", 100); setP (p, "roomDecay", decay);
            p.setPlayConfigDetails (2, 2, SR, 512); p.prepareToPlay (SR, 512);
            juce::Thread::sleep (400);
            juce::AudioBuffer<float> b (2, 512); juce::MidiBuffer m;
            for (int k = 0; k < 20; ++k) { b.clear(); p.processBlock (b, m); }
            juce::Thread::sleep (200);
            for (int k = 0; k < 10; ++k) { b.clear(); p.processBlock (b, m); }
            double e = 0;
            for (int blk = 0; blk < 120; ++blk)
            {
                b.clear(); if (blk == 0) for (int i = 0; i < 200; ++i) { const float v = 0.5f * (float) std::sin (TWO_PI * 1000 * i / SR); b.setSample (0, i, v); b.setSample (1, i, v); }
                p.processBlock (b, m);
                if (blk * 512 > (int) (0.15 * SR)) for (int i = 0; i < 512; ++i) e += (double) b.getSample (0, i) * b.getSample (0, i);
            }
            return (float) (10 * std::log10 (e + 1e-12));
        };
        const float dry = tail (0, 0), booth = tail (1, 0), hall = tail (4, 0), hallRib = tail (4, 7), hallDyn = tail (4, 6);
        report ("Room: cola de sala (seco < booth < sala grande)", dry < -100 && booth < hall && hall > -40, juce::String (dry, 0) + " / " + juce::String (booth, 0) + " / " + juce::String (hall, 0) + " dB");
        report ("Room: cinta capta mas sala que dinamico", hallRib > hallDyn + 3, juce::String (hallRib, 1) + " vs " + juce::String (hallDyn, 1) + " dB");
        const float plate = tail (7, 0), chamber = tail (8, 0), studioB = tail (6, 0), shortD = tail (2, 0, 0), longD = tail (2, 0, 100);
        report ("Room: Plate, Chamber, Studio B y Decay", plate > -60 && chamber > -60 && studioB > -100 && longD > shortD + 6,
                "plate " + juce::String (plate, 0) + ", chamber " + juce::String (chamber, 0) + ", B " + juce::String (studioB, 0) + ", decay " + juce::String (shortD, 0) + " -> " + juce::String (longD, 0) + " dB");
    }
    // mix 0 = senal original (alineada con la latencia)
    {
        TopLockerProcessor p; setP (p, "mix", 0);
        auto o = run (p, 440, 0.3f);
        const int L = p.getLatencySamples(); double err = 0;
        for (int i = 20000; i < 30000; ++i) err = std::max (err, std::abs ((double) o[(size_t) i] - 0.3 * std::sin (TWO_PI * 440 * (i - L) / SR)));
        report ("Mix 0 = senal original alineada", err < 1e-4 && L > 0, "latencia " + juce::String (L) + " muestras, error " + juce::String (err, 6));
        TopLockerProcessor q; clean (q); setP (q, "mic", 1); setP (q, "mix", 50);
        const float lv = resp (q, 1000);
        report ("Mix 50 sin cancelaciones", std::abs (lv - with ([] (TopLockerProcessor& r) { setP (r, "mic", 1); }, 1000)) < 1.0f, juce::String (lv, 2) + " dB");
    }
    // presets
    {
        TopLockerProcessor p; bool ok = true; setP (p, "source", 8);
        for (int i = 0; i < p.getNumPrograms(); ++i)
        {
            p.setCurrentProgram (i); auto o = run (p, 300, 0.3f, 0.3);
            float mx = 0; for (float v : o) { mx = std::max (mx, std::abs (v)); if (! std::isfinite (v)) ok = false; }
            if (mx > 1.5f || mx < 0.05f) ok = false;
        }
        report ("Presets estables y conservan el SOURCE MIC", ok && (int) p.apvts.getRawParameterValue ("source")->load() == 8 && p.getNumPrograms() >= 19,
                juce::String (p.getNumPrograms()) + " presets");
    }
    // estado y presets del usuario
    {
        TopLockerProcessor a; setP (a, "mic", 7); setP (a, "room", 3); setP (a, "preGain", 6.5f); a.presetName = "Mi voz";
        juce::MemoryBlock mb; a.getStateInformation (mb);
        TopLockerProcessor b; b.setStateInformation (mb.getData(), (int) mb.getSize());
        report ("Guardar/cargar estado", (int) b.apvts.getRawParameterValue ("mic")->load() == 7 && (int) b.apvts.getRawParameterValue ("room")->load() == 3 && b.presetName == "Mi voz", "");
        auto f = juce::File::createTempFile ("tlpreset");
        TopLockerProcessor c; setP (c, "mic", 9); setP (c, "high", 3); c.saveUserPreset (f);
        TopLockerProcessor d; setP (d, "source", 7); const bool ok = d.loadUserPreset (f);
        report ("Preset del usuario guardar/cargar", ok && (int) d.apvts.getRawParameterValue ("mic")->load() == 9 && std::abs (d.apvts.getRawParameterValue ("high")->load() - 3) < 0.01f
                && (int) d.apvts.getRawParameterValue ("source")->load() == 7, f.getFileName());
        f.deleteFile();
    }
    // power apagado = pasa la senal (con la misma latencia)
    {
        TopLockerProcessor p; setP (p, "power", 0); auto o = run (p, 440, 0.3f);
        const int L = p.getLatencySamples(); double err = 0;
        for (int i = 20000; i < 30000; ++i) err = std::max (err, std::abs ((double) o[(size_t) i] - 0.3 * std::sin (TWO_PI * 440 * (i - L) / SR)));
        report ("Bypass limpio", err < 1e-4, juce::String (err, 6));
    }
    // mono
    {
        TopLockerProcessor p; p.setPlayConfigDetails (1, 1, SR, 256); p.prepareToPlay (SR, 256);
        juce::AudioBuffer<float> b (1, 256); juce::MidiBuffer m; bool ok = true;
        for (int k = 0; k < 200; ++k) { for (int i = 0; i < 256; ++i) b.setSample (0, i, 0.2f * (float) std::sin (0.05 * (k * 256 + i))); p.processBlock (b, m); ok = ok && std::isfinite (b.getSample (0, 100)); }
        report ("Mono funciona", ok && b.getMagnitude (0, 256) > 0.01f, "");
    }
    // editor en varios tamanos
    {
        TopLockerProcessor p; std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        ed->setSize (900, 600); ed->setSize (1500, 1000);
        report ("Editor escalable", ed->getWidth() == 1500, juce::String (ed->getWidth()) + "x" + juce::String (ed->getHeight()));
        if (auto* te = dynamic_cast<TopLockerEditor*> (ed.get())) { setP (p, "mic", 6); setP (p, "angle", 45); te->tickNow(); }
    }
    std::printf (fails == 0 ? "TODAS LAS PRUEBAS OK\n" : "FALLARON %d\n", fails);
    return fails == 0 ? 0 : 1;
}
