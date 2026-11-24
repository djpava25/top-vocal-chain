// Pruebas de Top Spectra EQ: exactitud contra la curva analogica, fase lineal, dinamica, dinamica espectral,
// M/S, clics, caracter, latencias y CPU.
#include "SpqProcessor.h"
#include "SpqEditor.h"
#include <cstdio>

static int fails = 0;
static void report (const char* n, bool ok, const juce::String& info) { if (! ok) ++fails; std::printf ("%-60s %s  %s\n", n, info.toRawUTF8(), ok ? "OK" : "FALLO"); }
static const double SR = 48000.0, TWO_PI = 6.283185307179586;
static const int BS = 256;
static void setP (SpqProcessor& p, const juce::String& id, float v) { auto* q = p.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); }
static void band (SpqProcessor& p, int i, int type, float f, float g, float q, int slope = 1, int place = 0, int dm = 0, float thr = -24, float range = -6)
{
    setP (p, SpqProcessor::pid (i, "on"), 1); setP (p, SpqProcessor::pid (i, "type"), (float) type); setP (p, SpqProcessor::pid (i, "freq"), f);
    setP (p, SpqProcessor::pid (i, "gain"), g); setP (p, SpqProcessor::pid (i, "q"), q); setP (p, SpqProcessor::pid (i, "slope"), (float) slope);
    setP (p, SpqProcessor::pid (i, "place"), (float) place); setP (p, SpqProcessor::pid (i, "dmode"), (float) dm);
    setP (p, SpqProcessor::pid (i, "thr"), thr); setP (p, SpqProcessor::pid (i, "range"), range);
}
struct Run
{
    SpqProcessor& p; std::vector<float> L, R;
    explicit Run (SpqProcessor& pp) : p (pp) { p.setPlayConfigDetails (2, 2, SR, BS); p.prepareToPlay (SR, BS); }
    template <typename G, typename C> void go (double secs, G gen, C cb)
    {
        juce::AudioBuffer<float> b (2, BS);
        const int nb = (int) (secs * SR / BS);
        for (int k = 0; k < nb; ++k)
        {
            const size_t i0 = L.size();
            for (int i = 0; i < BS; ++i) for (int c = 0; c < 2; ++c) b.setSample (c, i, gen ((int) i0 + i, c));
            juce::MidiBuffer m; p.processBlock (b, m);
            for (int i = 0; i < BS; ++i) { L.push_back (b.getSample (0, i)); R.push_back (b.getSample (1, i)); }
            cb (k);
        }
    }
    template <typename G> void go (double secs, G gen) { go (secs, gen, [] (int) {}); }
};
static double level (const std::vector<float>& x, double hz, double t0, double t1)
{
    double re = 0, im = 0; const int a = (int) (t0 * SR), b = (int) (t1 * SR);
    for (int i = a; i < b; ++i) { const double w = 0.5 - 0.5 * std::cos (TWO_PI * (i - a) / (b - a)); re += x[(size_t) i] * w * std::cos (TWO_PI * hz * i / SR); im += x[(size_t) i] * w * std::sin (TWO_PI * hz * i / SR); }
    return 20 * std::log10 (std::sqrt (re * re + im * im) / (b - a) * 4 + 1e-15);
}
using Setup = std::function<void (SpqProcessor&)>;
// ganancia medida (dB) a una frecuencia con seno de -20 dBFS
static double gainAt (Setup s, double hz, double secs = 0.7)
{
    SpqProcessor p; s (p); Run r (p);
    const float a = 0.1f; r.go (secs, [=] (int i, int) { return a * (float) std::sin (TWO_PI * hz * i / SR); });
    return level (r.L, hz, secs - 0.4, secs - 0.02) + 20.0;
}
static double analogDb (Setup s, int b, double hz)
{
    SpqProcessor p; s (p); auto bp = p.bandParams (b);
    spq::ASec as[spq::maxSec]; const int n = spq::analogSections (bp, bp.g, as);
    return 20 * std::log10 (spq::analogMag (as, n, hz));
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc > 2 && juce::String (argv[1]) == "--snapshot")
    {
        SpqProcessor p; p.setCurrentProgram (argc > 3 ? juce::String (argv[3]).getIntValue() : 1);
        { Run r (p); juce::Random rnd (3); double lp = 0;
          r.go (1.5, [&] (int i, int) { lp += (rnd.nextFloat() * 2 - 1 - lp) * 0.12; return (float) (0.25 * lp + 0.08 * std::sin (TWO_PI * 110 * i / SR) + 0.03 * std::sin (TWO_PI * 5200 * i / SR)); }); }
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        ed->setVisible (true); ed->setSize (SpqEditor::W, SpqEditor::H);
        if (auto* se = dynamic_cast<SpqEditor*> (ed.get()))
        {
            if (argc > 4) se->getContent().selectBand (juce::String (argv[4]).getIntValue());
            for (int i = 0; i < 30; ++i) se->getContent().tick();
        }
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
        f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("snapshot -> %s\n", f.getFullPathName().toRawUTF8());
        return 0;
    }
    // ---------------------------------------------------------------- 1. transparencia
    {
        SpqProcessor p; Run r (p); r.go (0.5, [] (int i, int c) { return 0.3f * (float) std::sin (TWO_PI * (c ? 330 : 1234) * i / SR); });
        double m = 0; for (size_t i = 0; i < r.L.size(); ++i) m = std::max (m, std::abs (r.L[i] - 0.3 * std::sin (TWO_PI * 1234 * (double) i / SR)));
        report ("Sin bandas = senal intacta (Zero Latency)", m < 1e-6, juce::String (20 * std::log10 (m + 1e-12), 1) + " dB");
    }
    // ---------------------------------------------------------------- 2. exactitud contra la curva analogica (Zero Latency)
    {
        struct T { const char* name; int type; float f, g, q; int slope; } ts[] {
            { "Bell 1 kHz +6 dB Q1", spq::Bell, 1000, 6, 1, 1 }, { "Bell 12 kHz -9 dB Q2 (sin cramping)", spq::Bell, 12000, -9, 2, 1 },
            { "Bell 18 kHz +6 dB Q0.7", spq::Bell, 18000, 6, 0.7f, 1 }, { "High Shelf 8 kHz -6 dB 12 dB/oct", spq::HighShelf, 8000, -6, 0.71f, 1 },
            { "Low Shelf 120 Hz +4 dB 24 dB/oct", spq::LowShelf, 120, 4, 0.71f, 3 }, { "High Cut 16 kHz 24 dB/oct", spq::HighCut, 16000, 0, 0.71f, 3 } };
        for (auto& t : ts)
        {
            Setup s = [=] (SpqProcessor& p) { band (p, 0, t.type, t.f, t.g, t.q, t.slope); };
            double worst = 0, wf = 0;
            for (double hz : { 50.0, 200.0, 800.0, 2000.0, 5000.0, 9000.0, 12000.0, 15000.0, 18000.0, 20000.0 })
            { const double e = std::abs (gainAt (s, hz, 0.4) - analogDb (s, 0, hz)); if (e > worst) { worst = e; wf = hz; } }
            report ((juce::String ("ZL calca la curva analogica: ") + t.name).toRawUTF8(), worst < 0.35, "error max " + juce::String (worst, 3) + " dB en " + juce::String ((int) wf) + " Hz");
        }
        Setup cut = [] (SpqProcessor& p) { band (p, 0, spq::LowCut, 100, 0, 0.71f, 9); };
        const double a50 = gainAt (cut, 50, 1.0);
        report ("Low Cut 96 dB/oct: 1 octava abajo", std::abs (a50 + 96) < 3, juce::String (a50, 1) + " dB");
    }
    // ---------------------------------------------------------------- 3. fase lineal
    for (int m = 1; m <= 2; ++m)
    {
        SpqProcessor p; band (p, 0, spq::Bell, 1000, 9, 1.4f); band (p, 1, spq::LowCut, 60, 0, 0.71f, 5); setP (p, "mode", (float) m);
        Run r (p); r.go (1.0, [] (int i, int) { return i == 24000 ? 1.0f : 0.0f; });
        const int lat = p.getLatencySamples(), c = 24000 + lat;
        double asym = 0, peak = 0;
        for (int k = 1; k < 1500; ++k) { asym = std::max (asym, (double) std::abs (r.L[(size_t) (c + k)] - r.L[(size_t) (c - k)])); peak = std::max (peak, (double) std::abs (r.L[(size_t) (c + k)])); }
        report ((m == 1 ? "Linear Low: respuesta al impulso simetrica (fase lineal)" : "Linear High: respuesta al impulso simetrica (fase lineal)"), asym < 1e-4 * std::max (1e-6, (double) std::abs (r.L[(size_t) c])),
                "asimetria " + juce::String (20 * std::log10 (asym / std::abs (r.L[(size_t) c]) + 1e-12), 1) + " dB, latencia " + juce::String (lat));
        Setup s = [=] (SpqProcessor& q) { band (q, 0, spq::Bell, 1000, 9, 1.4f); setP (q, "mode", (float) m); };
        const double g1 = gainAt (s, 1000, 1.2), g3 = gainAt (s, 3000, 1.2);
        report ((m == 1 ? "Linear Low: curva correcta (1 kHz, 3 kHz)" : "Linear High: curva correcta (1 kHz, 3 kHz)"), std::abs (g1 - 9) < 0.2 && std::abs (g3 - analogDb (s, 0, 3000)) < 0.3,
                juce::String (g1, 2) + " / " + juce::String (g3, 2) + " dB");
    }
    // ---------------------------------------------------------------- 4. dinamica por banda
    for (int m : { 0, 1 })
    {
        auto dynLevel = [m] (float amp)
        {
            SpqProcessor p; band (p, 0, spq::Bell, 3000, 0, 1.0f, 1, 0, 1, -30, -6); setP (p, "mode", (float) m);
            Run r (p); r.go (1.2, [=] (int i, int) { return amp * (float) std::sin (TWO_PI * 3000 * i / SR); });
            return level (r.L, 3000, 0.8, 1.18) - 20 * std::log10 (amp);
        };
        const double loud = dynLevel (0.5f), quiet = dynLevel (0.005f);
        report ((m == 0 ? "Dinamica (ZL): baja -6 dB fuerte, nada suave" : "Dinamica (Linear): baja -6 dB fuerte, nada suave"), std::abs (loud + 6) < 0.6 && std::abs (quiet) < 0.2,
                juce::String (loud, 2) + " / " + juce::String (quiet, 2) + " dB");
    }
    // ---------------------------------------------------------------- 5. dinamica espectral: baja la resonancia, no el resto
    {
        auto run = [] (bool spectral, double& tone, double& noise)
        {
            SpqProcessor p; if (spectral) band (p, 0, spq::Bell, 4000, 0, 0.5f, 1, 0, 2, -50, -12);
            Run r (p); juce::Random rnd (7);
            r.go (2.0, [&] (int i, int) { return 0.05f * (rnd.nextFloat() * 2 - 1) + 0.2f * (float) std::sin (TWO_PI * 5000 * i / SR); });
            const int lat = p.getLatencySamples();
            std::vector<float> y (r.L.begin() + lat, r.L.end());
            tone = level (y, 5000, 1.0, 1.9);
            double nacc = 0; for (double hz : { 2600.0, 3000.0, 3300.0, 7000.0, 7600.0 }) nacc += level (y, hz, 1.0, 1.9); noise = nacc / 5;
        };
        double t0, n0, t1, n1; run (false, t0, n0); run (true, t1, n1);
        report ("Espectral: la resonancia de 5 kHz baja", t1 - t0 < -5.0, juce::String (t1 - t0, 2) + " dB");
        report ("Espectral: el resto de la banda casi no cambia", std::abs (n1 - n0) < 1.5, juce::String (n1 - n0, 2) + " dB");
    }
    // ---------------------------------------------------------------- 6. M/S
    {
        SpqProcessor p; band (p, 0, spq::Bell, 1000, -12, 1.0f, 1, 4); Run r (p);
        r.go (0.6, [] (int i, int) { return 0.2f * (float) std::sin (TWO_PI * 1000 * i / SR); });               // mono = sin lado
        const double mono = level (r.L, 1000, 0.3, 0.58) - 20 * std::log10 (0.2);
        SpqProcessor p2; band (p2, 0, spq::Bell, 1000, -12, 1.0f, 1, 4); Run r2 (p2);
        r2.go (0.6, [] (int i, int c) { return (c ? -0.2f : 0.2f) * (float) std::sin (TWO_PI * 1000 * i / SR); });  // solo lado
        const double side = level (r2.L, 1000, 0.3, 0.58) - 20 * std::log10 (0.2);
        report ("Banda SIDE: no toca lo mono, si el lado", std::abs (mono) < 0.05 && std::abs (side + 12) < 0.3, juce::String (mono, 2) + " / " + juce::String (side, 2) + " dB");
    }
    // ---------------------------------------------------------------- 7. sin clics al mover perillas rapido
    for (int m : { 0, 2 })
    {
        auto d2max = [m] (bool move)
        {
            SpqProcessor p; band (p, 0, spq::Bell, 800, 6, 1.0f); setP (p, "mode", (float) m); Run r (p);
            r.go (1.5, [] (int i, int) { return 0.25f * (float) std::sin (TWO_PI * 220 * i / SR); },
                  [&] (int k) { if (! move) return; const float t = (float) k / 280.0f; setP (p, SpqProcessor::pid (0, "freq"), 200.0f * std::pow (20.0f, std::fmod (t, 1.0f)));
                                                                                         setP (p, SpqProcessor::pid (0, "gain"), 12.0f * std::sin (t * 9.0f)); });
            const int lat = p.getLatencySamples(); double mx = 0; size_t at = 0;
            for (size_t i = (size_t) (0.3 * SR + lat); i < r.L.size(); ++i) { const double d = std::abs (r.L[i] - 2 * r.L[i - 1] + r.L[i - 2]); if (d > mx) { mx = d; at = i; } }
            if (move && std::getenv ("SPQDBG")) { std::printf ("  max en %zu (bloque %zu, pos %zu)\n", at, at / BS, at % BS); for (size_t i = at - 3; i < at + 3; ++i) std::printf ("   %zu %.6f\n", i, r.L[i]); }
            return mx;
        };
        const double a = d2max (false), b = d2max (true);
        report ((m == 0 ? "Sin clics moviendo FREQ y GAIN rapido (ZL)" : "Sin clics moviendo FREQ y GAIN rapido (Linear High)"), b < a * 4.5, juce::String (b / a, 2) + "x (una campana de +12 dB a 220 Hz da ~4x)");
    }
    // ---------------------------------------------------------------- 8. caracter
    {
        auto thd = [] (int ch, double hz, float amp)
        {
            SpqProcessor p; setP (p, "character", (float) ch); Run r (p);
            r.go (0.8, [=] (int i, int) { return amp * (float) std::sin (TWO_PI * hz * i / SR); });
            const double f = level (r.L, hz, 0.4, 0.78); double s = 0;
            for (int k = 2; k <= 7 && k * hz < 20000; ++k) s += std::pow (10.0, (level (r.L, k * hz, 0.4, 0.78) - f) / 10.0);
            return std::sqrt (s) * 100;
        };
        const double c = thd (0, 1000, 0.126f), s1 = thd (1, 80, 0.126f), s2 = thd (1, 2000, 0.126f), w = thd (2, 1000, 0.126f);
        report ("Clean: sin distorsion", c < 0.0005, juce::String (c, 5) + " %");
        report ("Subtle: color de trafo en graves (80 Hz) y no en agudos", s1 > 0.02 && s2 < s1 * 0.3, juce::String (s1, 3) + " % / " + juce::String (s2, 4) + " %");
        report ("Warm: valvula (~0.2 % a -18 dBFS)", w > 0.08 && w < 0.5, juce::String (w, 3) + " %");
    }
    // ---------------------------------------------------------------- 9. latencias
    {
        auto lat = [] (int m, bool sp) { SpqProcessor p; setP (p, "mode", (float) m); if (sp) band (p, 0, spq::Bell, 3000, 0, 1, 1, 0, 2); Run r (p); r.go (0.05, [] (int, int) { return 0.0f; }); return p.getLatencySamples(); };
        const int a = lat (0, false), b = lat (0, true), c = lat (1, false), d = lat (2, false);
        report ("Latencias: ZL 0 / ZL+espectral / Linear Low / Linear High", a == 0 && b > 0 && c > b && d > c,
                juce::String (a) + " / " + juce::String (b) + " / " + juce::String (c) + " / " + juce::String (d) + " muestras");
    }
    // ---------------------------------------------------------------- 10. CPU
    for (int m : { 0, 2 })
    {
        SpqProcessor p; setP (p, "mode", (float) m);
        band (p, 0, spq::LowCut, 40, 0, 0.71f, 5); band (p, 1, spq::Bell, 250, -3, 1.2f, 1, 0, 1, -30, -4); band (p, 2, spq::Bell, 3000, 2, 1); band (p, 3, spq::Bell, 6000, 0, 1, 1, 0, 2, -40, -8);
        band (p, 4, spq::HighShelf, 10000, 2, 0.71f); band (p, 5, spq::Bell, 120, 2, 0.8f, 1, 4); band (p, 6, spq::HighCut, 18000, 0, 0.71f, 3); band (p, 7, spq::Bell, 900, -2, 2, 1, 3, 1);
        Run r (p); juce::Random rnd (1);
        const double t0 = juce::Time::getMillisecondCounterHiRes();
        r.go (10.0, [&] (int, int) { return 0.2f * (rnd.nextFloat() * 2 - 1); });
        const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
        report ((m == 0 ? "CPU ZL: 8 bandas (2 dinamicas, 1 espectral), 10 s estereo" : "CPU Linear High: 8 bandas (2 dinamicas, 1 espectral), 10 s estereo"), ms < 3000,
                juce::String (ms, 0) + " ms (" + juce::String (10000.0 / ms, 1) + "x tiempo real)");
    }
    std::printf ("\n%s (%d fallos)\n", fails == 0 ? "TODO OK" : "HAY FALLOS", fails);
    return fails == 0 ? 0 : 1;
}
