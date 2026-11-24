// Pruebas de Top 76 contra las mediciones publicadas del limitador FET clasico (1176LN):
// manual UA (tiempos), AES 2016 / tesis de A. Moore (THD, rafagas), UA WebZine 2003 (umbral segun relacion, 0.5 % THD)
#include "Top76Processor.h"
#include "Top76Editor.h"
#include <cstdio>

static int fails = 0;
static void report (const char* n, bool ok, const juce::String& info) { if (! ok) ++fails; std::printf ("%-58s %s  %s\n", n, info.toRawUTF8(), ok ? "OK" : "FALLO"); }
static const double SR = 48000.0, TWO_PI = 6.283185307179586;
static const int BS = 256;
static void setP (Top76Processor& p, const juce::String& id, float v) { auto* q = p.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); }
static float dbAmp (double d) { return (float) std::pow (10.0, d / 20.0); }

struct Run
{
    Top76Processor& p; std::vector<float> L, R; std::vector<float> gr;
    explicit Run (Top76Processor& pp, int chans = 2) : p (pp) { p.setPlayConfigDetails (chans, chans, SR, BS); p.prepareToPlay (SR, BS); }
    template <typename G, typename C> void go (double secs, G gen, C cb)
    {
        const int chs = p.getTotalNumInputChannels();
        juce::AudioBuffer<float> b (chs, BS);
        const int nb = (int) (secs * SR / BS);
        for (int k = 0; k < nb; ++k)
        {
            const size_t i0 = L.size();
            for (int i = 0; i < BS; ++i) for (int c = 0; c < chs; ++c) b.setSample (c, i, gen ((int) i0 + i, c));
            juce::MidiBuffer m; p.processBlock (b, m);
            for (int i = 0; i < BS; ++i) { L.push_back (b.getSample (0, i)); R.push_back (b.getSample (chs > 1 ? 1 : 0, i)); }
            gr.push_back (p.grDb.load());
            cb (k);
        }
    }
    template <typename G> void go (double secs, G gen) { go (secs, gen, [] (int) {}); }
};
static auto sine (double hz, float amp) { return [=] (int i, int) { return amp * (float) std::sin (TWO_PI * hz * i / SR); }; }
static double level (const std::vector<float>& x, double hz, double t0, double t1)
{
    double re = 0, im = 0; const int a = (int) (t0 * SR), b = (int) (t1 * SR);
    for (int i = a; i < b; ++i) { const double w = 0.5 - 0.5 * std::cos (TWO_PI * (i - a) / (b - a)); re += x[(size_t) i] * w * std::cos (TWO_PI * hz * i / SR); im += x[(size_t) i] * w * std::sin (TWO_PI * hz * i / SR); }
    return 20 * std::log10 (std::sqrt (re * re + im * im) / (b - a) * 4 + 1e-15);
}
static double thdPct (const std::vector<float>& x, double hz, double t0, double t1)
{
    const double f = level (x, hz, t0, t1); double s = 0;
    for (int k = 2; k <= 9 && k * hz < 20000; ++k) s += std::pow (10.0, (level (x, k * hz, t0, t1) - f) / 10.0);
    return std::sqrt (s) * 100.0;
}
using Setup = std::function<void (Top76Processor&)>;
static Setup cfg (float in, float att, float rel, int ratio, bool vintage = false, float out = 0.0f)
{
    return [=] (Top76Processor& p) { setP (p, "input", in); setP (p, "attack", att); setP (p, "release", rel); setP (p, "ratio", (float) ratio); setP (p, "detector", vintage ? 1.0f : 0.0f); setP (p, "output", out); };
}
// GR (dB) en estado estable y nivel de salida
static double steady (Setup s, double hz, double inDb, double secs = 1.5, double* outDb = nullptr, std::vector<float>* keep = nullptr)
{
    Top76Processor p; s (p); Run r (p); r.go (secs, sine (hz, dbAmp (inDb)));
    if (outDb) *outDb = level (r.L, hz, secs - 0.5, secs - 0.02);
    if (keep) *keep = r.L;
    return r.gr.back();
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc > 2 && juce::String (argv[1]) == "--snapshot")
    {
        Top76Processor p; p.setCurrentProgram (argc > 3 ? juce::String (argv[3]).getIntValue() : 1);
        if (argc > 5) setP (p, "meter", juce::String (argv[5]).getFloatValue());
        { Run r (p); r.go (1.2, [] (int i, int) { return 0.30f * (float) std::sin (TWO_PI * 220.0 * i / SR) * (float) (0.6 + 0.4 * std::sin (TWO_PI * 1.3 * i / SR)); }); }
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        ed->setVisible (true); if (argc > 4 && juce::String (argv[4]) == "full") ed->setSize (Top76Editor::W, Top76Editor::H);
        if (auto* te = dynamic_cast<Top76Editor*> (ed.get())) te->tickNow();
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
        f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("snapshot -> %s\n", f.getFullPathName().toRawUTF8());
        return 0;
    }
    const auto colour = cfg (0, 0, 5, 0);                                  // ATTACK OFF: solo color

    // ---------------------------------------------------------------- 1. ganancia y respuesta en frecuencia
    {
        double out; steady (colour, 1000, -30, 0.6, &out);
        report ("Ganancia unidad (INPUT 0, OUTPUT 0, ATTACK OFF)", std::abs (out + 30) < 0.2, juce::String (out + 30, 2) + " dB");
        double lo = 1e9, hi = -1e9;
        for (double hz : { 20.0, 40.0, 100.0, 1000.0, 5000.0, 10000.0, 16000.0, 20000.0 })
        { double o; steady (colour, hz, -30, 0.8, &o); lo = std::min (lo, o + 30); hi = std::max (hi, o + 30); }
        report ("Respuesta 20 Hz - 20 kHz (espec. +-1 dB)", lo > -1.0 && hi < 1.0, juce::String (lo, 2) + " .. " + juce::String (hi, 2) + " dB");
    }
    // ---------------------------------------------------------------- 2. THD sin compresion (Moore: 0.01 / 0.06 / 0.25 %)
    {
        const double dbu[] { 0, 9, 16 }, lo[] { 0.005, 0.03, 0.15 }, hi[] { 0.02, 0.10, 0.40 }, ref[] { 0.01, 0.06, 0.25 };
        for (int i = 0; i < 3; ++i)
        {
            std::vector<float> y; steady (colour, 1000, dbu[i] - 24, 0.6, nullptr, &y);
            const double t = thdPct (y, 1000, 0.2, 0.58);
            report (("THD 1 kHz sin compresion a " + juce::String ((int) dbu[i]) + " dBu (equipo " + juce::String (ref[i]) + " %)").toRawUTF8(), t > lo[i] && t < hi[i], juce::String (t, 3) + " %");
        }
    }
    // ---------------------------------------------------------------- 3. THD con -8 dB de reduccion (ATT 3, REL 7, 4:1)
    auto inFor8 = [] (bool vintage, double hz)
    {
        double a = -10, b = 30;
        for (int it = 0; it < 18; ++it) { const double m = 0.5 * (a + b); (steady (cfg ((float) m, 3, 7, 0, vintage), hz, -24, 0.5) > 8.0 ? b : a) = m; }
        return 0.5 * (a + b);
    };
    double thdV125 = 0, thdC125 = 0, thd4 = 0;
    for (int v = 1; v >= 0; --v)
        for (double hz : { 1000.0, 125.0 })
        {
            std::vector<float> y; const double g = steady (cfg ((float) inFor8 (v == 1, hz), 3, 7, 0, v == 1), hz, -24, 1.2, nullptr, &y);
            const double t = thdPct (y, hz, 0.7, 1.18);
            if (hz < 200) (v ? thdV125 : thdC125) = t; else if (v) thd4 = t;
            juce::String name = juce::String (v ? "VINTAGE" : "CLEAN") + " THD -8 dB GR a " + juce::String ((int) hz) + " Hz" + (v ? (hz > 200 ? " (equipo 0.07 %)" : " (equipo 0.40 %)") : "");
            const bool ok = v ? (hz > 200 ? (t > 0.04 && t < 0.15) : (t > 0.2 && t < 0.6)) : t < 0.1;
            report (name.toRawUTF8(), ok && std::abs (g - 8) < 0.5, juce::String (t, 3) + " %  (GR " + juce::String (g, 2) + ")");
        }
    report ("CLEAN: graves sin distorsion de rizado (125 Hz)", thdC125 < thdV125 * 0.35, juce::String (thdC125, 3) + " vs " + juce::String (thdV125, 3) + " %");
    // ---------------------------------------------------------------- 4. relaciones y umbral
    {
        const int Rn[] { 4, 8, 12, 20 };
        double prevTh = -100;
        bool thUp = true;
        for (int r = 0; r < 4; ++r)
        {
            double o1, o2; steady (cfg (0, 3, 4, r), 1000, -6, 0.8, &o1); steady (cfg (0, 3, 4, r), 1000, 0, 0.8, &o2);
            const double slope = 6.0 / std::max (1e-6, o2 - o1);
            report (("Relacion " + juce::String (Rn[r]) + ":1 (pendiente medida)").toRawUTF8(), std::abs (slope / Rn[r] - 1) < 0.12, juce::String (slope, 2) + ":1");
            double a = -40, b = 0;                                                        // umbral: entrada que da 1 dB de reduccion
            for (int it = 0; it < 14; ++it) { const double m = 0.5 * (a + b); (steady (cfg (0, 3, 4, r), 1000, m, 0.5) > 1.0 ? b : a) = m; }
            const double th = 0.5 * (a + b);
            if (th <= prevTh) thUp = false;
            prevTh = th;
        }
        report ("El umbral sube con la relacion (4 -> 20)", thUp, "ultimo " + juce::String (prevTh, 1) + " dBFS");
        double o1, o2; std::vector<float> yA, y4;
        steady (cfg (0, 3, 4, 4), 1000, -10, 0.8, &o1); steady (cfg (0, 3, 4, 4), 1000, 0, 0.8, &o2, &yA);
        report ("ALL BUTTONS: la curva se pasa (+10 dB entrada -> salida no sube)", o2 <= o1 + 0.3, juce::String (o2 - o1, 2) + " dB");
        steady (cfg (0, 3, 4, 0), 1000, 0, 0.8, nullptr, &y4);
        const double tA = thdPct (yA, 1000, 0.3, 0.78), t4 = thdPct (y4, 1000, 0.3, 0.78);
        report ("ALL BUTTONS: mucha mas distorsion que 4:1", tA > 3.0 * t4, juce::String (tA, 2) + " vs " + juce::String (t4, 2) + " %");
    }
    // ---------------------------------------------------------------- 5. tiempos (rafaga +12 dB)
    {
        // reduccion medida periodo a periodo: pico de salida / pico de entrada (alineados con la latencia), 1 kHz
        auto burst = [] (float att, float rel, bool up, double& t63, double& over)
        {
            Top76Processor p; cfg (6, att, rel, 0) (p); Run r (p);
            const double t0 = up ? 0.6 : 1.2;
            auto in = [=] (int i) { const bool hiLv = up ? (i >= t0 * SR) : (i < t0 * SR); return (hiLv ? 0.25f : 0.0625f) * (float) std::sin (TWO_PI * 1000.0 * i / SR); };
            r.go (up ? 0.9 : 3.2, [=] (int i, int) { return in (i); });
            const int lat = p.getLatencySamples(), P = 24, i0 = (int) (t0 * SR);
            std::vector<double> gr;                                            // un valor por periodo desde el salto
            for (int s0 = i0; s0 + P + lat < (int) r.L.size(); s0 += P)
            {
                double mo = 0, mi = 0;
                for (int j = s0; j < s0 + P; ++j) { mo = std::max (mo, (double) std::abs (r.L[(size_t) (j + lat)])); mi = std::max (mi, (double) std::abs (in (j))); }
                gr.push_back (20 * std::log10 (mi / std::max (1e-12, mo)));
            }
            double before = 0;
            { double mo = 0, mi = 0; for (int j = i0 - 4 * P; j < i0 - 3 * P; ++j) { mo = std::max (mo, (double) std::abs (r.L[(size_t) (j + lat)])); mi = std::max (mi, (double) std::abs (in (j))); } before = 20 * std::log10 (mi / std::max (1e-12, mo)); }
            const double fin = gr.back();
            t63 = -1; over = 0;
            for (size_t k = 0; k < gr.size(); ++k)
            {
                if (t63 < 0 && std::abs (gr[k] - before) >= 0.63 * std::abs (fin - before)) t63 = (double) (k + 1) * P / SR;
                if (up) over = std::max (over, gr[k] - fin);
            }
        };
        double a1, a3, a7, o1, o3, o7;
        burst (1, 4, true, a1, o1); burst (3, 4, true, a3, o3); burst (7, 4, true, a7, o7);
        report ("ATTACK: 7 mas rapido que 3, 3 mas rapido que 1", a7 < a3 && a3 < a1, juce::String (a7 * 1e3, 2) + " / " + juce::String (a3 * 1e3, 2) + " / " + juce::String (a1 * 1e3, 2) + " ms");
        report ("ATTACK 3: 63 % de la reduccion en pocos ms (Moore ~5 ms)", a3 > 0.0008 && a3 < 0.007, juce::String (a3 * 1e3, 2) + " ms");
        report ("ATTACK 7 sin sobre-reduccion (artefacto de detector)", o7 < 1.0, juce::String (o7, 2) + " dB");
        double r7, r1, d;
        burst (3, 7, false, r7, d); burst (3, 1, false, r1, d);
        report ("RELEASE 7 (espec. 50 ms)", r7 > 0.03 && r7 < 0.14, juce::String (r7 * 1e3, 1) + " ms");
        report ("RELEASE 1 (espec. 1.1 s)", r1 > 0.5 && r1 < 1.6, juce::String (r1 * 1e3, 0) + " ms");
    }
    // ---------------------------------------------------------------- 6. sin artefactos digitales
    {
        // aliasing: 9 kHz muy saturado en ALL BUTTONS; armonicos reflejados (a 48 kHz) en 3, 6 y 12 kHz
        Top76Processor p; cfg (24, 7, 7, 4, true, -12) (p); Run r (p); r.go (1.0, sine (9000, dbAmp (-6)));
        const double f = level (r.L, 9000, 0.4, 0.98);
        double worst = -300; for (double hz : { 3000.0, 6000.0, 12000.0, 15000.0 }) worst = std::max (worst, level (r.L, hz, 0.4, 0.98) - f);
        report ("Aliasing (9 kHz saturado en ALL, peor reflejo)", worst < -90, juce::String (worst, 1) + " dBc");
        // clics: cambiar RATIO y ALL de golpe mientras suena. Un clic es un pico aislado: se compara la 2da diferencia
        // maxima de cada ventana de 2 ms con la de las ventanas vecinas (10 ms antes y despues)
        {
            Top76Processor p; cfg (6, 5, 6, 0) (p); Run r (p);
            r.go (1.2, sine (220, dbAmp (-12)), [&] (int k) { if (k == 60) setP (p, "ratio", 3); if (k == 90) setP (p, "ratio", 4); if (k == 130) setP (p, "ratio", 1); if (k == 150) setP (p, "ratio", 0); });
            const int Wn = 96; std::vector<double> mw;
            for (size_t i = (size_t) (0.2 * SR); i + Wn < r.L.size(); i += Wn)
            { double m = 0; for (size_t j = i; j < i + Wn; ++j) m = std::max (m, (double) std::abs (r.L[j] - 2 * r.L[j - 1] + r.L[j - 2])); mw.push_back (m); }
            double worst = 0;
            size_t wk = 0; for (size_t k = 5; k + 5 < mw.size(); ++k) { const double q = mw[k] / (0.5 * (mw[k - 5] + mw[k + 5])); if (q > worst) { worst = q; wk = k; } }
            if (std::getenv ("T76DBG")) std::printf ("  pico en %.3f s\n", 0.2 + (double) wk * Wn / SR);
            report ("Sin clics al cambiar RATIO / ALL (pico aislado vs vecinos)", worst < 1.5, juce::String (worst, 2) + "x");
        }
        // INPUT +18 dB de golpe: ningun salto de muestra a muestra mayor que la pendiente maxima de un seno a ese nivel
        {
            Top76Processor p; cfg (6, 5, 6, 0) (p); Run r (p); float peakOut = 0;
            r.go (1.0, sine (220, dbAmp (-12)), [&] (int k) { if (k == 100) setP (p, "input", 24); });
            for (size_t i = (size_t) (0.25 * SR); i < r.L.size(); ++i) peakOut = std::max (peakOut, std::abs (r.L[i]));
            double md = 0; for (size_t i = (size_t) (0.25 * SR); i < r.L.size(); ++i) md = std::max (md, (double) std::abs (r.L[i] - r.L[i - 1]));
            const double bound = peakOut * TWO_PI * 220 / SR;
            report ("Sin clics al girar INPUT +18 dB de golpe (salto max / pendiente seno)", md < bound * 1.15, juce::String (md / bound, 2) + "x");
        }
        // MIX 0 = senal original alineada (null)
        Top76Processor pm; cfg (12, 5, 5, 3) (pm); setP (pm, "mix", 0); Run rm (pm);
        rm.go (0.5, [] (int i, int) { return 0.3f * (float) std::sin (TWO_PI * 330 * i / SR) + 0.1f * (float) std::sin (TWO_PI * 4100 * i / SR); });
        const int lat = pm.getLatencySamples(); double nul = 0;
        for (size_t i = (size_t) (0.2 * SR); i < rm.L.size(); ++i) { const int j = (int) i - lat; const double x = 0.3 * std::sin (TWO_PI * 330 * j / SR) + 0.1 * std::sin (TWO_PI * 4100 * j / SR); nul = std::max (nul, std::abs (rm.L[i] - x)); }
        report ("MIX 0 = original exacto (latencia compensada)", nul < 1e-5, juce::String (20 * std::log10 (nul + 1e-12), 1) + " dB, lat " + juce::String (lat));
        // OFF (boton METER OFF) = bypass
        Top76Processor po; cfg (12, 5, 5, 3) (po); setP (po, "meter", 3); Run ro (po); ro.go (0.5, sine (440, 0.4f));
        double nb = 0; for (size_t i = (size_t) (0.2 * SR); i < ro.L.size(); ++i) { const int j = (int) i - lat; nb = std::max (nb, std::abs (ro.L[i] - 0.4 * std::sin (TWO_PI * 440 * j / SR))); }
        report ("METER OFF = equipo apagado (bypass limpio)", nb < 1e-5, juce::String (20 * std::log10 (nb + 1e-12), 1) + " dB");
    }
    // ---------------------------------------------------------------- 7. estereo
    {
        auto lr = [] (bool link)
        {
            Top76Processor p; cfg (8, 4, 5, 0) (p); setP (p, "link", link ? 1.0f : 0.0f); Run r (p);
            r.go (1.0, [] (int i, int c) { return (c == 0 ? 0.5f : 0.02f) * (float) std::sin (TWO_PI * 700 * i / SR); });
            return level (r.R, 700, 0.5, 0.98) - 20 * std::log10 (0.02);
        };
        const double gl = lr (true), gd = lr (false);
        report ("LINK: el lado debil baja con el fuerte / DUAL: no", gl < -6 && std::abs (gd - 8) < 1.0, "link " + juce::String (gl, 1) + " dB, dual " + juce::String (gd, 1) + " dB");
    }
    // ---------------------------------------------------------------- 8. CPU
    {
        Top76Processor p; cfg (10, 5, 6, 0) (p); Run r (p);
        const double t0 = juce::Time::getMillisecondCounterHiRes();
        r.go (10.0, [] (int i, int c) { return 0.3f * (float) std::sin (TWO_PI * (c ? 220 : 330) * i / SR); });
        const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
        report ("CPU: 10 s estereo a 48 kHz (x4 interno)", ms < 2500, juce::String (ms, 0) + " ms (" + juce::String (10000.0 / ms, 1) + "x tiempo real)");
    }
    std::printf ("\n%s (%d fallos)\n", fails == 0 ? "TODO OK" : "HAY FALLOS", fails);
    return fails == 0 ? 0 : 1;
}
