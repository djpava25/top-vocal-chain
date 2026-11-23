// Pruebas de Top 73
#include "Top73Processor.h"
#include "Top73Editor.h"
#include <cstdio>

static int fails = 0;
static void report (const char* n, bool ok, const juce::String& info) { if (! ok) ++fails; std::printf ("%-48s %s  %s\n", n, info.toRawUTF8(), ok ? "OK" : "FALLO"); }
static const double SR = 48000.0, TWO_PI = 6.283185307179586;
static void setP (Top73Processor& p, const juce::String& id, float v) { auto* q = p.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); }
static std::vector<float> run (Top73Processor& p, double hz, float amp = 0.01f, double secs = 0.8)
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
static float level (const std::vector<float>& x, double hz, double t0 = 0.4, double t1 = 0.78)
{
    double re = 0, im = 0; const int a = (int) (t0 * SR), b = (int) (t1 * SR);
    for (int i = a; i < b; ++i) { const double w = 0.5 - 0.5 * std::cos (TWO_PI * (i - a) / (b - a)); re += x[(size_t) i] * w * std::cos (TWO_PI * hz * i / SR); im += x[(size_t) i] * w * std::sin (TWO_PI * hz * i / SR); }
    return (float) (20 * std::log10 (std::sqrt (re * re + im * im) / (b - a) * 4 + 1e-12));
}
template <typename F> static float with (F setup, double hz, float amp = 0.01f) { Top73Processor p; setup (p); return level (run (p, hz, amp), hz) - 20.0f * std::log10 (amp); }
static int micIdx (int db) { return (db - 20) / 5; }

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc > 2 && juce::String (argv[1]) == "--snapshot")
    {
        Top73Processor p; p.setCurrentProgram (argc > 3 ? juce::String (argv[3]).getIntValue() : 1);
        run (p, 300, 0.3f, 0.3);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        ed->setVisible (true);
        if (auto* te = dynamic_cast<Top73Editor*> (ed.get())) te->tickNow();
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
        f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("snapshot -> %s\n", f.getFullPathName().toRawUTF8());
        return 0;
    }
    // 1. fidelidad a las mediciones por impulso (respuesta relativa a 1 kHz, EQ plano)
    {
        struct Pt { int db; double hz; float meas; };
        const Pt pts[] {
            { 20, 30, -2.6f }, { 20, 100, -0.87f }, { 20, 3000, 0.15f }, { 20, 15000, 0.09f },
            { 50, 30, -1.74f }, { 50, 50, -0.39f }, { 50, 1000, 0.0f }, { 50, 10000, 0.08f },
            { 55, 30, -0.02f }, { 55, 100, -0.01f }, { 55, 10000, 0.1f }, { 55, 20000, 0.2f },
            { 60, 10000, -0.01f }, { 60, 20000, 0.06f },
            { 65, 10000, 0.32f }, { 65, 15000, 0.63f }, { 65, 20000, 0.59f },
            { 70, 6000, 0.32f }, { 70, 10000, 1.28f }, { 70, 15000, 2.15f }, { 70, 20000, 3.03f },
            { 75, 6000, 1.38f }, { 75, 10000, 2.63f }, { 75, 15000, 3.55f }, { 75, 20000, 4.42f },
            { 80, 3000, 0.83f }, { 80, 6000, 2.24f }, { 80, 10000, 3.65f }, { 80, 15000, 4.73f }, { 80, 20000, 5.63f } };
        float worst = 0; juce::String at;
        for (const auto& p : pts)
        {
            n73::Settings s; s.gain = micIdx (p.db);
            const float model = n73::responseDb (s, p.hz) - n73::responseDb (s, 1000.0);
            const float e = std::abs (model - p.meas);
            if (e > worst) { worst = e; at = juce::String (p.db) + " dB @ " + juce::String ((int) p.hz) + " Hz"; }
        }
        report ("Curvas iguales a las mediciones (25 Hz-20 kHz)", worst < 1.0f, "error max " + juce::String (worst, 2) + " dB (" + at + ")");
        n73::Settings s; s.gain = micIdx (50); s.hf = 6.0f;
        n73::Settings f; f.gain = micIdx (50);
        auto d = [&] (double hz) { return (n73::responseDb (s, hz) - n73::responseDb (s, 1000)) - (n73::responseDb (f, hz) - n73::responseDb (f, 1000)); };
        report ("Shelf 12k +6 igual a la medicion", std::abs (d (10000) - 2.9f) < 0.6f && std::abs (d (20000) - 5.5f) < 0.8f && std::abs (d (3000) - 0.4f) < 0.4f,
                "3k " + juce::String (d (3000), 2) + " (0.41), 10k " + juce::String (d (10000), 2) + " (2.9), 20k " + juce::String (d (20000), 2) + " (5.5)");
    }
    // 2. procesando audio real: los agudos se abren con la ganancia (AUTO compensa el volumen)
    {
        auto tilt = [] (int db) { auto cfg = [db] (Top73Processor& p) { setP (p, "gain", (float) micIdx (db)); }; return with (cfg, 15000, 0.003f) - with (cfg, 1000, 0.003f); };
        const float t50 = tilt (50), t80 = tilt (80);
        const float l50 = with ([] (Top73Processor& p) { setP (p, "gain", (float) micIdx (50)); }, 1000, 0.003f), l80 = with ([] (Top73Processor& p) { setP (p, "gain", (float) micIdx (80)); }, 1000, 0.003f);
        report ("Audio: brillo de alta ganancia + AUTO", t80 - t50 > 3.5f && std::abs (l80 - l50) < 1.0f,
                "15k: " + juce::String (t50, 1) + " -> " + juce::String (t80, 1) + " dB, nivel " + juce::String (l80 - l50, 2) + " dB");
        Top73Processor q; setP (q, "auto", 0); setP (q, "gain", (float) micIdx (60));
        report ("Sin AUTO la ganancia sube (+10 dB en MIC 60)", std::abs (level (run (q, 1000, 0.003f), 1000) - 20 * std::log10 (0.003f) - 10) < 0.6f, "");
    }
    // 3. preamp contra las especificaciones del 1073 (0 dBFS = +24 dBu)
    {
        // THD (armonicos 2..7) y H2/H3 para una salida dada en dBu
        struct T { float thd, h2, h3; };
        auto thdAt = [] (int gainIdx, double hz, float outDbu)
        {
            Top73Processor p; setP (p, "gain", (float) gainIdx);
            // nivel interno = entrada + ganancia (respecto a MIC 50); con AUTO la salida vuelve al nivel de entrada
            const float amp = std::pow (10.0f, (outDbu - 24.0f - n73::driveDb (gainIdx)) / 20.0f);
            auto o = run (p, hz, amp, 1.6);
            auto L = [&o] (double f0) { return level (o, f0, 0.5, 1.55); };          // ventana larga: exacta tambien en 20-50 Hz
            const float f = L (hz);
            double h = 0; for (int k = 2; k <= 7; ++k) { if (hz * k > 23000) break; h += std::pow (10.0, (L (hz * k) - f) / 10.0); }
            return T { (float) (10 * std::log10 (h + 1e-30)), L (hz * 2) - f, L (hz * 3) - f };
        };
        auto pct = [] (float db) { return juce::String (100.0 * std::pow (10.0, db / 20.0), 3) + "%"; };
        const T k1 = thdAt (micIdx (50), 1000, 20), l50 = thdAt (micIdx (50), 50, 20);
        report ("Spec: THD <= 0.07% a +20 dBu (1 kHz y 50 Hz)", k1.thd < -63.1f && l50.thd < -63.1f && k1.thd > -90,
                "1 kHz " + pct (k1.thd) + ", 50 Hz " + pct (l50.thd));
        report ("Spec: 2do y 3ro parecidos (3ro un poco mayor)", k1.h3 >= k1.h2 - 1.0f && k1.h3 - k1.h2 < 8.0f, juce::String::formatted ("H2 %.1f dB, H3 %.1f dB", k1.h2, k1.h3));
        const T clip = thdAt (micIdx (50), 1000, 26.5f), c24 = thdAt (micIdx (50), 1000, 24);
        report ("Spec: satura cerca de +26 dBu", clip.thd > -46 && clip.thd < -26 && c24.thd < -40, "+24 dBu " + pct (c24.thd) + ", +26.5 dBu " + pct (clip.thd));
        const T clean = thdAt (micIdx (50), 1000, 6);
        report ("Nivel normal (-18 dBFS) limpio: no quita calidad", clean.thd < -80, juce::String::formatted ("THD %.1f dB", clean.thd));
        const T hi = thdAt (micIdx (80), 1000, 20);
        report ("Ganancia alta (MIC 80): mas riqueza, mas 2do que 3ro", hi.thd > k1.thd + 12 && hi.h2 > hi.h3, juce::String::formatted ("THD %.1f dB (MIC 50: %.1f), H2 %.1f, H3 %.1f", hi.thd, k1.thd, hi.h2, hi.h3));
        const T l20 = thdAt (micIdx (50), 20, 20), l1k = thdAt (micIdx (50), 1000, 20);
        report ("Transformadores: mas color en graves profundos", l20.thd > l1k.thd + 10, juce::String::formatted ("20 Hz %.1f dB, 1 kHz %.1f dB", l20.thd, l1k.thd));
        // al empujar la ganancia satura (con AUTO): MIC 70 a -18 dBFS -> +26 dBu internos
        Top73Processor q; setP (q, "gain", (float) micIdx (70)); auto o = run (q, 1000, 0.126f, 1.0);
        const float push = level (o, 2000) - level (o, 1000), push3 = level (o, 3000) - level (o, 1000);
        report ("MIC 70 a -18 dBFS: saturacion audible", push3 > -45 && push > -55, juce::String::formatted ("H2 %.1f dB, H3 %.1f dB", push, push3));
        // sin aliasing al saturar fuerte (7 kHz a MIC 80)
        Top73Processor z; setP (z, "gain", (float) micIdx (75)); auto oz = run (z, 7000, 0.126f, 1.0);
        float alias = -200; for (double fa : { 1000.0, 3000.0, 5000.0, 9000.0, 11000.0, 13000.0 }) alias = std::max (alias, level (oz, fa) - level (oz, 7000));
        report ("Saturacion fuerte (MIC 75, +31 dBu) sin aliasing (8x)", alias < -60, juce::String::formatted ("peor componente fuera de armonicos %.1f dB", alias));
    }
    // 4. EQ
    {
        auto eq = [] (std::vector<std::pair<const char*, float>> v, double hz) { return with ([v] (Top73Processor& p) { for (auto& [k, x] : v) setP (p, k, x); }, hz) - with ([] (Top73Processor&) {}, hz); };
        const float hf = eq ({ { "hf", 16 } }, 16000), mid = eq ({ { "midFreq", 4 }, { "midGain", 12 } }, 3200), low = eq ({ { "lowFreq", 3 }, { "lowGain", 10 } }, 50);
        const float hp = eq ({ { "hpf", 3 } }, 60), off = eq ({ { "midFreq", 0 }, { "midGain", 12 } }, 3200), byp = eq ({ { "eq", 0 }, { "hf", 16 } }, 16000);
        report ("High 12k / Mid 3.2k / Low 110", hf > 12 && std::abs (mid - 12) < 1 && low > 8, juce::String (hf, 1) + " / " + juce::String (mid, 1) + " / " + juce::String (low, 1) + " dB");
        report ("HPF 160 Hz (18 dB/oct), OFF y EQ out", hp < -12 && std::abs (off) < 0.1f && std::abs (byp) < 0.1f, "60 Hz " + juce::String (hp, 1) + " dB, off " + juce::String (off, 2) + ", eq out " + juce::String (byp, 2));
    }
    // 5. power, fase, presets, estado, editor
    {
        Top73Processor p; setP (p, "power", 0); setP (p, "gain", 12); auto o = run (p, 440, 0.3f);
        const int L = p.getLatencySamples(); double err = 0;
        for (int i = 20000; i < 30000; ++i) err = std::max (err, std::abs ((double) o[(size_t) i] - 0.3 * std::sin (TWO_PI * 440 * (i - L) / SR)));
        report ("POWER apagado = senal original", err < 1e-4, "error " + juce::String (err, 6));
        bool ok = true; juce::String info;
        for (int i = 0; i < p.getNumPrograms(); ++i)
        {
            Top73Processor q; q.setCurrentProgram (i); auto r = run (q, 200, 0.2f, 0.3);
            float mx = 0; for (float v : r) { if (! std::isfinite (v)) ok = false; mx = std::max (mx, std::abs (v)); }
            if (mx > 1.5f || mx < 0.02f) ok = false;
        }
        report ("Presets estables", ok, juce::String (p.getNumPrograms()) + " presets");
        Top73Processor a; setP (a, "gain", 10); setP (a, "midGain", 5);
        juce::MemoryBlock mb; a.getStateInformation (mb);
        Top73Processor b; b.setStateInformation (mb.getData(), (int) mb.getSize());
        report ("Guardar/cargar estado", (int) b.apvts.getRawParameterValue ("gain")->load() == 10 && std::abs (b.apvts.getRawParameterValue ("midGain")->load() - 5) < 0.01f, "");
        std::unique_ptr<juce::AudioProcessorEditor> ed (a.createEditor()); ed->setSize (1000, 200);
        report ("Editor escalable", ed->getWidth() == 1000, "");
    }
    std::printf (fails == 0 ? "TODAS LAS PRUEBAS OK\n" : "FALLARON %d\n", fails);
    return fails == 0 ? 0 : 1;
}
