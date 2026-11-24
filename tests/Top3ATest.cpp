// Pruebas de Top 3A (contra las especificaciones publicadas del nivelador optico clasico)
#include "Top3AProcessor.h"
#include "Top3AEditor.h"
#include <cstdio>

static int fails = 0;
static void report (const char* n, bool ok, const juce::String& info) { if (! ok) ++fails; std::printf ("%-52s %s  %s\n", n, info.toRawUTF8(), ok ? "OK" : "FALLO"); }
static const double SR = 48000.0, TWO_PI = 6.283185307179586;
static const int BS = 256;
static void setP (Top3AProcessor& p, const juce::String& id, float v) { auto* q = p.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); }
static float dbAmp (double d) { return (float) std::pow (10.0, d / 20.0); }

// genera y procesa; gen(i, ch) -> muestra; cb(bloque) se llama tras cada bloque
struct Run
{
    Top3AProcessor& p; std::vector<float> L, R; std::vector<float> gr;
    explicit Run (Top3AProcessor& pp, int chans = 2) : p (pp) { p.setPlayConfigDetails (chans, chans, SR, BS); p.prepareToPlay (SR, BS); }
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
// nivel (dB) de una frecuencia en una ventana
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
// GR en estado estable con un seno
static double steadyGr (std::function<void (Top3AProcessor&)> setup, double hz, double inDb, double secs = 2.5, double* outDb = nullptr)
{
    Top3AProcessor p; setup (p); Run r (p); r.go (secs, sine (hz, dbAmp (inDb)));
    if (outDb) *outDb = level (r.L, hz, secs - 0.5, secs - 0.02);
    return r.gr.back();
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc > 2 && juce::String (argv[1]) == "--snapshot")
    {
        Top3AProcessor p; p.setCurrentProgram (argc > 3 ? juce::String (argv[3]).getIntValue() : 2);
        { Run r (p); r.go (1.2, [] (int i, int) { return 0.25f * (float) std::sin (TWO_PI * 220.0 * i / SR) * (float) (0.6 + 0.4 * std::sin (TWO_PI * 1.3 * i / SR)); }); }
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        ed->setVisible (true); if (argc > 4) ed->setSize (Top3AEditor::W, Top3AEditor::H);
        if (auto* te = dynamic_cast<Top3AEditor*> (ed.get())) te->tickNow();
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
        f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("snapshot -> %s\n", f.getFullPathName().toRawUTF8());
        return 0;
    }
    const double unityGain = 3.75;
    auto plain = [=] (double pr, bool limit = false, double gain = 3.75, double emph = 0) { return [=] (Top3AProcessor& p) { setP (p, "peak", (float) pr); setP (p, "mode", limit ? 1.0f : 0.0f); setP (p, "gain", (float) gain); setP (p, "emph", (float) emph); }; };

    // ---------------------------------------------------------------- 1. respuesta (la "IR" estatica) con PEAK REDUCTION en 0
    {
        Top3AProcessor p0; plain (0) (p0); Run r0 (p0); r0.go (0.6, sine (1000, 0.1f));
        const double ref = level (r0.L, 1000, 0.3, 0.58);
        report ("Ganancia unidad (GAIN 3.75, PR 0)", std::abs (ref + 20.0) < 0.35, juce::String (ref + 20.0, 2) + " dB");
        double lo = 1e9, hi = -1e9; juce::String det;
        for (double hz : { 20.0, 30.0, 50.0, 100.0, 500.0, 2000.0, 5000.0, 10000.0, 15000.0, 20000.0 })
        {
            Top3AProcessor p; plain (0) (p); Run r (p); r.go (0.8, sine (hz, 0.1f));
            const double d = level (r.L, hz, 0.4, 0.78) - ref; lo = std::min (lo, d); hi = std::max (hi, d);
            det << juce::String ((int) hz) << ":" << juce::String (d, 2) << " ";
        }
        report ("Respuesta 20 Hz-20 kHz dentro de +-0.5 dB", lo > -0.5 && hi < 0.5, det);
    }
    // ---------------------------------------------------------------- 2. coloracion estatica (THD) segun la especificacion
    {
        auto thdAt = [] (double gain, double pr, double hz, double inDb, double secs, double& gr)
        { Top3AProcessor p; setP (p, "gain", (float) gain); setP (p, "peak", (float) pr); Run r (p); r.go (secs, sine (hz, dbAmp (inDb))); gr = r.gr.back(); return thdPct (r.L, hz, secs - 0.5, secs - 0.02); };
        double gr;
        const double t4 = thdAt (unityGain, 0, 1000, -20, 0.8, gr);
        report ("THD 1 kHz a +4 dBu (nivel nominal)", t4 < 0.05, juce::String (t4, 4) + " %");
        const double t24 = thdAt (unityGain, 0, 1000, 0, 0.8, gr);
        report ("THD 1 kHz a +24 dBu (spec < 0.35 %)", t24 < 0.35 && t24 > 0.05, juce::String (t24, 3) + " %");
        const double t50 = thdAt (unityGain, 10, 50, -4, 3.5, gr);
        report ("THD 50 Hz con ~15 dB de GR (spec <= 0.7 %)", t50 < 0.7 && gr > 13 && gr < 17, juce::String (t50, 3) + " % (GR " + juce::String (gr, 1) + ")");
        const double t1g = thdAt (unityGain, 10, 1000, 2, 3.0, gr);
        report ("THD 1 kHz con ~18 dB de GR (spec tipico < 0.3 %)", t1g < 0.3 && gr > 15, juce::String (t1g, 3) + " % (GR " + juce::String (gr, 1) + ")");
        const double t30 = thdAt (unityGain, 0, 30, -4, 1.0, gr), t1k = thdAt (unityGain, 0, 1000, -4, 0.8, gr);
        report ("Trafos: graves saturan mas que medios (30 Hz vs 1 kHz)", t30 > 1.5 * t1k, juce::String (t30, 3) + " % vs " + juce::String (t1k, 3) + " %");
    }
    // ---------------------------------------------------------------- 3. curva estatica
    {
        const double g1 = steadyGr (plain (10), 1000, -34.0);
        report ("Umbral: PR 10 empieza a comprimir en ~-10 dBu", g1 > 0.4 && g1 < 2.0, "GR " + juce::String (g1, 2) + " dB a -34 dBFS");
        double o1, o2; steadyGr (plain (10), 1000, -8, 2.5, &o1); steadyGr (plain (10), 1000, 4, 2.5, &o2);
        const double ratio = 12.0 / (o2 - o1);
        report ("COMPRESS: relacion fija ~3:1 (rodilla blanda)", ratio > 2.2 && ratio < 3.6, juce::String (ratio, 2) + ":1");
        const double ins[] { -28, -24, -8, -4, 0, 4, 8, 12 };
        double ol[8]; bool mono = true; double rl = 0;
        for (int k = 0; k < 8; ++k)
        {
            steadyGr (plain (10, true), 1000, ins[k], 2.5, &ol[k]);
            if (k > 0 && ol[k] < ol[k - 1] - 0.05) mono = false;
            if (k > 2) rl = std::max (rl, (ins[k] - ins[k - 1]) / std::max (1e-3, ol[k] - ol[k - 1]));
        }
        const double rlo = 4.0 / std::max (1e-3, ol[1] - ol[0]);
        report ("LIMIT: la relacion sube con compresion fuerte", rl > 15.0 && rl > 4.0 * rlo, juce::String (rlo, 1) + ":1 -> " + juce::String (rl, 1) + ":1");
        report ("LIMIT: sin sobrecompresion (salida nunca baja)", mono, juce::String (ol[0], 1) + " .. " + juce::String (ol[7], 1) + " dB");
        const double ga = steadyGr (plain (6, false, 2.0), 1000, -10), gb = steadyGr (plain (6, false, 8.0), 1000, -10);
        report ("GAIN no cambia la compresion", std::abs (ga - gb) < 0.05, juce::String (ga, 2) + " vs " + juce::String (gb, 2) + " dB");
        const double gmax = steadyGr (plain (10), 1000, 30, 1.5);
        report ("Reduccion maxima acotada (spec 40 dB)", gmax > 25 && gmax <= 40, juce::String (gmax, 1) + " dB");
        const double glf = steadyGr (plain (8), 200, -12), ghf = steadyGr (plain (8), 12000, -12);
        report ("Agudos comprimen menos que graves", ghf < glf - 0.5, juce::String (glf, 2) + " vs " + juce::String (ghf, 2) + " dB");
        const double e0 = steadyGr (plain (5, false, 3.75, 10), 500, -14), e1 = steadyGr (plain (5, false, 3.75, 10), 15000, -14);
        report ("HF EMPH: hasta ~10 dB mas de GR en 15 kHz", e1 - e0 > 6.0 && e1 - e0 < 13.0, juce::String (e1 - e0, 1) + " dB mas");
    }
    // ---------------------------------------------------------------- 4. dinamica de la celda T4
    {
        auto attack = [] (double inDb, double& t50, double& t90, double& fin)
        {
            Top3AProcessor p; setP (p, "peak", 5.0f); setP (p, "gain", 3.75f);
            Run r (p); r.go (0.3, [] (int, int) { return 0.0f; });
            r.gr.clear();
            const size_t base = r.L.size();
            // GR por muestra no esta expuesto: se mide en bloques de 256 (5.3 ms) y se interpola con la envolvente de salida
            std::vector<float> g;
            Top3AProcessor& pp = p; (void) pp;
            r.go (1.5, [&] (int i, int) { return dbAmp (inDb) * (float) std::sin (TWO_PI * 1000.0 * (i - (int) base) / SR); });
            fin = r.gr.back();
            // envolvente de salida (pico por ciclo de 1 kHz) relativa al primer ciclo => GR(t)
            const int cyc = (int) (SR / 1000.0), lat = p.getLatencySamples();
            auto pk = [&] (size_t c0) { float m = 0; for (size_t i = c0; i < c0 + (size_t) cyc; ++i) m = std::max (m, std::abs (r.L[i])); return m; };
            const size_t s0 = base + (size_t) lat;
            const float first = pk (s0);
            t50 = t90 = -1;
            for (size_t c = s0; c + (size_t) cyc < r.L.size(); c += (size_t) (cyc / 4))
            {
                const double grNow = 20 * std::log10 (first / std::max (1e-9f, pk (c)));
                const double t = (double) (c - s0) / SR * 1000.0;
                if (t50 < 0 && grNow >= 0.5 * fin) t50 = t;
                if (t90 < 0 && grNow >= 0.9 * fin) { t90 = t; break; }
            }
        };
        double a50, a90, af; attack (0.0, a50, a90, af);
        report ("Ataque con pico grande: < 1.5 ms (spec)", a50 >= 0 && a50 < 1.5, "50% en " + juce::String (a50, 2) + " ms (GR " + juce::String (af, 1) + ")");
        double b50, b90, bf; attack (-14.0, b50, b90, bf);
        report ("Ataque con pico chico: lento (decenas de ms)", b90 > 30 && b90 < 300, "90% en " + juce::String (b90, 1) + " ms (GR " + juce::String (bf, 1) + ")");
        auto release = [] (double burst, double& t50, double& t1, double& g0)
        {
            Top3AProcessor p; setP (p, "peak", 5.0f); setP (p, "gain", 3.75f);
            Run r (p); r.go (burst, sine (1000, dbAmp (-6.0)));
            g0 = r.gr.back(); t50 = t1 = -1; int k0 = (int) r.gr.size();
            r.go (8.0, [] (int, int) { return 0.0f; }, [&] (int) {
                const double g = r.gr.back(), t = (double) ((int) r.gr.size() - k0) * BS / SR;
                if (t50 < 0 && g <= 0.5 * g0) t50 = t * 1000.0;
                if (t1 < 0 && g <= 1.0) t1 = t; });
        };
        double r50, r1, rg; release (0.2, r50, r1, rg);
        report ("Liberacion etapa 1: ~50% en 60-150 ms", r50 > 40 && r50 < 220, juce::String (r50, 0) + " ms (desde " + juce::String (rg, 1) + " dB)");
        report ("Liberacion etapa 2: 0.5-5 s", r1 > 0.4 && r1 < 5.0, "< 1 dB en " + juce::String (r1, 2) + " s");
        double l50, l1, lg; release (10.0, l50, l1, lg);
        report ("Memoria: tras compresion larga suelta mas lento", l1 > 1.8 * r1 && l1 < 6.0, juce::String (r1, 2) + " s -> " + juce::String (l1, 2) + " s");
    }
    // ---------------------------------------------------------------- 5. sin artefactos
    {
        auto alias = [] (double pr, double inDb)
        {
            Top3AProcessor p; setP (p, "peak", (float) pr); setP (p, "gain", 3.75f); Run r (p); r.go (1.6, sine (15000, dbAmp (inDb)));
            const double f = level (r.L, 15000, 1.0, 1.58); double worst = -300;
            for (double a : { 3000.0, 6000.0, 9000.0, 12000.0, 18000.0, 21000.0 }) worst = std::max (worst, level (r.L, a, 1.0, 1.58) - f);
            return worst;
        };
        const double a1 = alias (0, -1), a2 = alias (10, 0);
        report ("Aliasing 15 kHz caliente (sin GR) < -85 dB", a1 < -85, juce::String (a1, 1) + " dB");
        report ("Aliasing 15 kHz con mucha GR < -85 dB", a2 < -85, juce::String (a2, 1) + " dB");
        // cambios bruscos de perillas: sin clics (energia > 8 kHz no salta)
        Top3AProcessor p; setP (p, "peak", 5.0f); setP (p, "gain", 4.0f);
        Run r (p);
        r.go (1.6, sine (500, dbAmp (-14)), [&] (int k) {
            if (k == (int) (0.8 * SR / BS)) { setP (p, "gain", 7.0f); setP (p, "peak", 9.0f); setP (p, "mode", 1.0f); setP (p, "emph", 8.0f); setP (p, "mix", 60.0f); } });
        juce::dsp::IIR::Filter<float> hp1, hp2; hp1.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (SR, 8000.0); hp2.coefficients = hp1.coefficients;
        std::vector<float> h (r.L.size()); for (size_t i = 0; i < h.size(); ++i) h[i] = hp2.processSample (hp1.processSample (r.L[i]));
        auto rmsDb = [&] (double t0, double t1) { double s = 0; const int a = (int) (t0 * SR), b = (int) (t1 * SR); for (int i = a; i < b; ++i) s += (double) h[(size_t) i] * h[(size_t) i]; return 10 * std::log10 (s / (b - a) + 1e-30); };
        const double before = rmsDb (0.5, 0.79), during = rmsDb (0.79, 0.95), after = rmsDb (1.3, 1.58);
        report ("Mover perillas de golpe: sin clics (> 8 kHz)", during < std::max (before, after) + 6.0 && during < -80, juce::String (before, 1) + " / " + juce::String (during, 1) + " / " + juce::String (after, 1) + " dB");
        // silencio / denormales / entrada enorme: sin NaN
        Top3AProcessor q; setP (q, "peak", 10.0f); Run rq (q); bool finite = true;
        rq.go (1.0, [] (int i, int) { return i < 24000 ? 1.0e-30f : 30.0f * (float) std::sin (TWO_PI * 60.0 * i / SR); });
        for (float v : rq.L) if (! std::isfinite (v)) finite = false;
        report ("Silencio, denormales y +30 dBFS: sin NaN/Inf", finite, "");
    }
    // ---------------------------------------------------------------- 6. bypass, mezcla, estereo, latencia
    {
        auto dryCheck = [] (std::function<void (Top3AProcessor&)> setup)
        {
            Top3AProcessor p; setup (p); Run r (p); auto g = [] (int i, int c) { return 0.3f * (float) std::sin (TWO_PI * (c ? 313.0 : 441.0) * i / SR + 0.2 * c); };
            r.go (1.0, g);
            const int lat = p.getLatencySamples(); double err = 0;
            for (int i = (int) (0.5 * SR); i < (int) (0.99 * SR); ++i) err = std::max (err, (double) std::abs (r.L[(size_t) i] - g (i - lat, 0)));
            return err;
        };
        const double e1 = dryCheck ([] (Top3AProcessor& p) { setP (p, "power", 0.0f); });
        report ("POWER apagado = senal seca exacta (alineada)", e1 < 1e-6, juce::String (e1, 9));
        const double e2 = dryCheck ([] (Top3AProcessor& p) { setP (p, "mix", 0.0f); });
        report ("MIX 0 % = senal seca exacta", e2 < 1e-6, juce::String (e2, 9));
        Top3AProcessor p; setP (p, "peak", 8.0f); Run r (p);
        r.go (2.0, [] (int i, int c) { return c == 0 ? 0.5f * (float) std::sin (TWO_PI * 1000.0 * i / SR) : 0.01f * (float) std::sin (TWO_PI * 300.0 * i / SR); });
        const double rIn = 20 * std::log10 (0.01), rOut = level (r.R, 300, 1.5, 1.98) - rIn, l1k = level (r.R, 1000, 1.5, 1.98);
        report ("Estereo enlazado: una celda para los dos canales", rOut < -4.0 && l1k < -100, "R baja " + juce::String (rOut, 1) + " dB, cruce " + juce::String (l1k, 0) + " dB");
        Top3AProcessor pm; setP (pm, "peak", 6.0f); Run rm (pm, 1); rm.go (1.0, sine (1000, 0.3f)); bool ok = true; for (float v : rm.L) if (! std::isfinite (v)) ok = false;
        report ("Mono funciona", ok && rm.gr.back() > 1.0, "GR " + juce::String (rm.gr.back(), 1));
        report ("Latencia reportada (4x)", p.getLatencySamples() > 0 && p.getLatencySamples() < 200, juce::String (p.getLatencySamples()) + " muestras");
    }
    // ---------------------------------------------------------------- 7. CPU, presets, estado, interfaz
    {
        Top3AProcessor p; setP (p, "peak", 7.0f); Run r (p, 2);
        const double t0 = juce::Time::getMillisecondCounterHiRes();
        r.go (10.0, [] (int i, int c) { return 0.2f * (float) std::sin (TWO_PI * (c ? 110.0 : 220.0) * i / SR); });
        const double rt = (juce::Time::getMillisecondCounterHiRes() - t0) / 10000.0;
        report ("CPU (estereo, 48 kHz)", rt < 0.25, juce::String (rt * 100.0, 1) + " % de tiempo real");
        bool presetsOk = p.getNumPrograms() >= 10;
        for (int i = 0; i < p.getNumPrograms(); ++i) { p.setCurrentProgram (i); if (p.getProgramName (i).isEmpty()) presetsOk = false; }
        report ("Presets", presetsOk, juce::String (p.getNumPrograms()));
        p.setCurrentProgram (9); juce::MemoryBlock mb; p.getStateInformation (mb);
        Top3AProcessor q; q.setStateInformation (mb.getData(), (int) mb.getSize());
        report ("Guardar / cargar estado", std::abs (q.apvts.getRawParameterValue ("peak")->load() - p.apvts.getRawParameterValue ("peak")->load()) < 1e-4 && q.getCurrentProgram() == 9, "");
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        ed->setSize (Top3AEditor::W / 2, Top3AEditor::H / 2);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds());
        report ("Interfaz escalable", img.getWidth() == Top3AEditor::W / 2 && ed->getHeight() == Top3AEditor::H / 2, juce::String (ed->getWidth()) + "x" + juce::String (ed->getHeight()));
    }
    std::printf ("\n%s (%d fallos)\n", fails == 0 ? "TODO OK" : "HAY FALLOS", fails);
    return fails == 0 ? 0 : 1;
}
