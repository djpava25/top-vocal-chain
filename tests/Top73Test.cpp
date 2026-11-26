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
    if (argc > 5 && juce::String (argv[1]) == "--render")          // --render entrada.f32 salida.f32 gainIdx sat3d (estereo float crudo, 48 kHz)
    {
        juce::File fin (argv[2]), fout (argv[3]); fout.deleteFile();
        juce::MemoryBlock mb; fin.loadFileAsData (mb);
        const int frames = (int) (mb.getSize() / (2 * sizeof (float)));
        const float* src = static_cast<const float*> (mb.getData());
        Top73Processor p; setP (p, "gain", (float) juce::String (argv[4]).getIntValue()); setP (p, "sat3d", (float) juce::String (argv[5]).getIntValue());
        p.setPlayConfigDetails (2, 2, SR, 512); p.prepareToPlay (SR, 512);
        juce::AudioBuffer<float> b (2, 512); std::vector<float> out ((size_t) frames * 2);
        const int lat = p.getLatencySamples();
        for (int pos = 0; pos < frames + lat; pos += 512)
        {
            for (int i = 0; i < 512; ++i) for (int c = 0; c < 2; ++c) b.setSample (c, i, pos + i < frames ? src[(size_t) (pos + i) * 2 + (size_t) c] : 0.0f);
            juce::MidiBuffer m; p.processBlock (b, m);
            for (int i = 0; i < 512; ++i) { const int o = pos + i - lat; if (o >= 0 && o < frames) for (int c = 0; c < 2; ++c) out[(size_t) o * 2 + (size_t) c] = b.getSample (c, i); }
        }
        fout.replaceWithData (out.data(), out.size() * sizeof (float));
        return 0;
    }
    if (argc > 1 && juce::String (argv[1]) == "--satmap")          // armonicos por posicion de la perilla roja (con AUTO)
    {
        for (int g = 0; g < 20; ++g)
        {
            juce::String row = n73::gainNames()[g].paddedRight (' ', 10);
            for (float inDb : { -24.0f, -18.0f, -12.0f })
            {
                Top73Processor p; setP (p, "gain", (float) g); setP (p, "sat3d", argc > 2 ? 0.0f : 1.0f);
                const float amp = std::pow (10.0f, inDb / 20.0f);
                auto o = run (p, 1000, amp, 1.0);
                const float f = level (o, 1000);
                double h = 0; for (int k = 2; k <= 9; ++k) h += std::pow (10.0, (level (o, 1000.0 * k) - f) / 10.0);
                row << juce::String::formatted ("| %+3.0f dBFS: THD %6.1f H2 %6.1f H3 %6.1f H5 %6.1f ", inDb, 10 * std::log10 (h + 1e-30), level (o, 2000) - f, level (o, 3000) - f, level (o, 5000) - f);
            }
            std::printf ("%s\n", row.toRawUTF8());
        }
        return 0;
    }
    if (argc > 2 && juce::String (argv[1]) == "--curves")          // curvas del EQ (CSV) para graficar
    {
        juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2])); f.deleteFile();
        juce::FileOutputStream os (f);
        auto row = [&os] (const juce::String& name, n73::Settings s)
        {
            n73::Settings flat; flat.gain = s.gain; flat.eq = s.eq;
            os << name;
            for (int i = 0; i <= 200; ++i) { const double hz = 20.0 * std::pow (1000.0, i / 200.0); os << "," << juce::String (n73::responseDb (s, hz) - n73::responseDb (flat, hz), 3); }
            os << "\n";
        };
        for (int lf = 1; lf <= 4; ++lf) for (float g : { 16.0f, -16.0f }) { n73::Settings s; s.lowFreq = lf; s.lowGain = g; row ("low" + juce::String (lf) + "_" + juce::String ((int) g), s); }
        for (int mf = 1; mf <= 6; ++mf) for (float g : { 18.0f, 9.0f, -18.0f }) { n73::Settings s; s.midFreq = mf; s.midGain = g; row ("mid" + juce::String (mf) + "_" + juce::String ((int) g), s); }
        for (float g : { 16.0f, 6.0f, -16.0f }) { n73::Settings s; s.hf = g; row ("hf_" + juce::String ((int) g), s); }
        for (int h = 1; h <= 4; ++h) { n73::Settings s; s.hpf = h; row ("hpf" + juce::String (h), s); }
        return 0;
    }
    if (argc > 2 && juce::String (argv[1]) == "--snapshot")
    {
        Top73Processor p; p.setCurrentProgram (argc > 3 ? juce::String (argv[3]).getIntValue() : 1);
        run (p, 300, 0.3f, 0.3);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        ed->setVisible (true); if (argc > 4) ed->setSize (Top73Editor::W, Top73Editor::H);
        if (auto* te = dynamic_cast<Top73Editor*> (ed.get())) te->tickNow();
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
        f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("snapshot -> %s\n", f.getFullPathName().toRawUTF8());
        return 0;
    }
    // 1. respuesta del preamp: spec del 1073 (+-0.5 dB 20 Hz-20 kHz, -3 dB ~40 kHz) y mediciones por impulso a ganancia alta
    {
        auto rel = [] (int gainIdx, double hz) { n73::Settings s; s.gain = gainIdx; s.eq = false; return n73::responseDb (s, hz) - n73::responseDb (s, 1000.0); };
        float worst = 0; juce::String at;
        for (int g : { micIdx (20), micIdx (35), micIdx (50), 17 })
            for (double hz : { 20.0, 30.0, 50.0, 100.0, 5000.0, 10000.0, 20000.0 })
                if (std::abs (rel (g, hz)) > worst) { worst = std::abs (rel (g, hz)); at = n73::gainNames()[g] + " @ " + juce::String ((int) hz) + " Hz"; }
        const float bump = std::max (rel (micIdx (50), 14), rel (micIdx (50), 17)), top = rel (micIdx (50), 40000);
        report ("Spec: +-0.5 dB 20 Hz-20 kHz, -3 dB ~40 kHz", worst < 0.5f && top < -2.0f && top > -4.5f && bump > 0.0f && bump < 0.6f,
                "error max " + juce::String (worst, 2) + " dB (" + at + "), 40 kHz " + juce::String (top, 1) + " dB, rebote graves +" + juce::String (bump, 2));
        struct Pt { int db; double hz; float meas; };
        const Pt pts[] { { 55, 30, -0.02f }, { 55, 10000, 0.1f }, { 55, 20000, 0.2f }, { 60, 10000, -0.01f }, { 60, 20000, 0.06f },
                         { 65, 10000, 0.32f }, { 65, 15000, 0.63f }, { 65, 20000, 0.59f }, { 70, 6000, 0.32f }, { 70, 10000, 1.28f }, { 70, 15000, 2.15f }, { 70, 20000, 3.03f },
                         { 75, 6000, 1.38f }, { 75, 10000, 2.63f }, { 75, 15000, 3.55f }, { 75, 20000, 4.42f }, { 80, 3000, 0.83f }, { 80, 6000, 2.24f }, { 80, 10000, 3.65f }, { 80, 15000, 4.73f }, { 80, 20000, 5.63f } };
        worst = 0;
        for (const auto& p : pts) { const float e = std::abs (rel (micIdx (p.db), p.hz) - p.meas); if (e > worst) { worst = e; at = juce::String (p.db) + " dB @ " + juce::String ((int) p.hz) + " Hz"; } }
        report ("MIC 55-80 igual a las mediciones por impulso", worst < 1.0f, "error max " + juce::String (worst, 2) + " dB (" + at + ")");
    }
    // 2. EQ del circuito contra las curvas medidas del clon (numeros leidos de sus graficas) y la descripcion del original
    {
        auto db = [] (std::complex<double> c) { return (float) (20 * std::log10 (std::abs (c))); };
        auto sh = [&] (int low, double a, double hz) { return db (n73::acGain (n73::shelfNet (low, a, 0.5), hz)) - db (n73::acGain (n73::shelfNet (low, 0.5, 0.5), hz)); };
        // graves 60 Hz al maximo: +19.3 en 20 Hz, +13.6 en 100 Hz, muesca ~-1.1 dB cerca de 800 Hz; corte espejo
        const float l20 = sh (2, 0, 20), l100 = sh (2, 0, 100), div = std::min (sh (2, 0, 700), sh (2, 0, 1000)), c100 = sh (2, 1, 100);
        report ("Graves (circuito) vs medicion: 60 Hz al maximo", std::abs (l20 - 19.3f) < 1.2f && std::abs (l100 - 13.6f) < 1.0f && div < -0.6f && div > -2.0f && std::abs (c100 + l100) < 1.0f,
                juce::String::formatted ("20 Hz %+.1f, 100 Hz %+.1f, muesca %.1f, corte 100 Hz %+.1f", l20, l100, div, c100));
        const float l35 = sh (1, 0, 20), l220 = sh (4, 0, 300);
        report ("Graves 35 / 220 Hz al maximo", std::abs (l35 - 21.0f) < 1.2f && std::abs (l220 - 10.5f) < 1.5f, juce::String::formatted ("35: %+.1f en 20 Hz, 220: %+.1f en 300 Hz", l35, l220));
        // agudos: perilla +16 = curva del circuito; +6 = medicion por impulso del 1073 real
        auto hfd = [] (float knob, double hz) { n73::Settings s; s.hf = knob; n73::Settings f; return n73::responseDb (s, hz) - n73::responseDb (f, hz); };
        report ("Agudos +16: +2 1k, +9 3k, +16 10k, +18 20k (circuito)", std::abs (hfd (16, 1000) - 2.0f) < 1.0f && std::abs (hfd (16, 3000) - 9.0f) < 1.0f && std::abs (hfd (16, 10000) - 16.2f) < 1.0f && std::abs (hfd (16, 20000) - 18.2f) < 1.0f && hfd (16, 330) < -0.8f,
                juce::String::formatted ("%+.1f / %+.1f / %+.1f / %+.1f, muesca 330 Hz %.1f", hfd (16, 1000), hfd (16, 3000), hfd (16, 10000), hfd (16, 20000), hfd (16, 330)));
        report ("Agudos +6 igual a la medicion del 1073", std::abs (hfd (6, 10000) - 2.9f) < 0.6f && std::abs (hfd (6, 20000) - 5.5f) < 0.8f && std::abs (hfd (6, 3000) - 0.4f) < 0.4f,
                juce::String::formatted ("3k %.2f (0.41), 10k %.2f (2.9), 20k %.2f (5.5)", hfd (6, 3000), hfd (6, 10000), hfd (6, 20000)));
        // medios: +18 en todas las frecuencias al maximo, Q proporcional y mas estrecho arriba
        auto md = [&] (int idx, double w, double hz) { return db (n73::acGain (n73::midNet (idx, w), hz)); };
        float lo = 99, hi = -99; for (int i = 1; i <= 6; ++i) { const float v = md (i, 0, n73::midFreqs()[i]); lo = std::min (lo, v); hi = std::max (hi, v); }
        report ("Medios al maximo: +18 dB en las 6 frecuencias", lo > 17.0f && hi < 19.5f, juce::String::formatted ("%.1f a %.1f dB", lo, hi));
        report ("Medios vs medicion: 3.2k en 1 kHz, corte 360 en 1 kHz", std::abs (md (4, 0, 1000) - 7.6f) < 1.0f && std::abs (md (1, 1, 1000) + 8.5f) < 1.2f,
                juce::String::formatted ("%+.1f (7.6), %+.1f (-8.5)", md (4, 0, 1000), md (1, 1, 1000)));
        auto frac = [&] (int idx, float knob) { const double w = n73::midPot (knob, idx), f0 = n73::midFreqs()[idx]; return md (idx, w, f0 * 0.5) / md (idx, w, f0); };
        report ("Q proporcional: poco refuerzo = campana mas ancha", frac (3, 6) > frac (3, 18) + 0.12f, juce::String::formatted ("1.6k, una octava abajo: +6 -> %.0f%%, +18 -> %.0f%%", 100 * frac (3, 6), 100 * frac (3, 18)));
        report ("Mas estrecho en frecuencias altas", frac (6, 18) < frac (1, 18) - 0.1f, juce::String::formatted ("una octava abajo: 360 -> %.0f%%, 7.2k -> %.0f%%", 100 * frac (1, 18), 100 * frac (6, 18)));
        auto hp = [&] (int idx, double hz) { return db (n73::acGain (n73::hpfNet (idx), hz) * n73::hpfMakeup); };
        report ("HPF pasivo de 3er orden (18 dB/oct)", hp (3, 160) > -1.5f && std::abs ((hp (3, 60) - hp (3, 30)) - 18.0f) < 1.5f && hp (3, 1000) > -0.1f,
                juce::String::formatted ("160 Hz: %.1f dB, 60->30 Hz: %.1f dB", hp (3, 160), hp (3, 60) - hp (3, 30)));
        // las perillas leen dB reales
        n73::Settings s; s.lowFreq = 3; s.lowGain = 10; s.midFreq = 4; s.midGain = -12;
        const auto p = n73::pots (s);
        const float dcLow = db (n73::acGain (n73::shelfNet (3, p.a, 0.5), 5.0)), midAt = md (4, p.w, 3200);
        report ("Perillas en dB: graves +10, medios -12", std::abs (dcLow - 10) < 0.3f && std::abs (midAt + 12) < 0.2f, juce::String::formatted ("%.2f / %.2f dB", dcLow, midAt));
    }
    // 3. el simulador en tiempo real da la misma curva que el analisis del circuito
    {
        auto setup = [] (Top73Processor& p) { setP (p, "lowFreq", 2); setP (p, "lowGain", 10); setP (p, "midFreq", 3); setP (p, "midGain", 8); setP (p, "hf", -6); setP (p, "hpf", 2); };
        n73::Settings s; s.lowFreq = 2; s.lowGain = 10; s.midFreq = 3; s.midGain = 8; s.hf = -6; s.hpf = 2; n73::Settings f;
        float worst = 0; juce::String at;
        for (double hz : { 40.0, 70.0, 150.0, 400.0, 1600.0, 4000.0, 9000.0, 15000.0 })
        {
            const float audio = with (setup, hz) - with ([] (Top73Processor&) {}, hz);
            const float model = n73::responseDb (s, hz) - n73::responseDb (f, hz);
            if (std::abs (audio - model) > worst) { worst = std::abs (audio - model); at = juce::String ((int) hz) + " Hz"; }
        }
        report ("Audio procesado = analisis del circuito", worst < 0.25f, "error max " + juce::String (worst, 2) + " dB (" + at + ")");
        const float off = with ([] (Top73Processor& p) { setP (p, "midFreq", 0); setP (p, "midGain", 12); setP (p, "lowFreq", 0); setP (p, "lowGain", 12); }, 3200)
                        - with ([] (Top73Processor&) {}, 3200);
        const float byp = with ([] (Top73Processor& p) { setP (p, "eq", 0); setP (p, "hf", 16); }, 16000) - with ([] (Top73Processor& p) { setP (p, "eq", 0); }, 16000);
        const float flat = with ([] (Top73Processor&) {}, 18000) - with ([] (Top73Processor& p) { setP (p, "eq", 0); }, 18000);
        report ("OFF / EQ out / EQ plano casi transparente", std::abs (off) < 0.05f && std::abs (byp) < 0.05f && std::abs (flat) < 0.3f, juce::String::formatted ("off %.2f, eq out %.2f, plano a 18 kHz %.2f dB", off, byp, flat));
    }
    // 4. preamp contra las especificaciones del 1073 (0 dBFS = +24 dBu), con la tecla 3D apagada
    {
        struct T { float thd, h2, h3; };
        auto thdAt = [] (int gainIdx, double hz, float outDbu)
        {
            Top73Processor p; setP (p, "gain", (float) gainIdx); setP (p, "sat3d", 0);   // circuito original (3D apagado)
            const float amp = std::pow (10.0f, (outDbu - 24.0f - n73::driveDb (gainIdx)) / 20.0f);
            auto o = run (p, hz, amp, 1.6);
            auto L = [&o] (double f0) { return level (o, f0, 0.5, 1.55); };
            const float f = L (hz);
            double h = 0; for (int k = 2; k <= 7; ++k) { if (hz * k > 23000) break; h += std::pow (10.0, (L (hz * k) - f) / 10.0); }
            return T { (float) (10 * std::log10 (h + 1e-30)), L (hz * 2) - f, L (hz * 3) - f };
        };
        auto pct = [] (float db) { return juce::String (100.0 * std::pow (10.0, db / 20.0), 3) + "%"; };
        const T k1 = thdAt (micIdx (50), 1000, 20), l50 = thdAt (micIdx (50), 50, 20);
        report ("Spec: THD <= 0.07% a +20 dBu (1 kHz y 50 Hz)", k1.thd < -63.1f && l50.thd < -63.1f && k1.thd > -75, "1 kHz " + pct (k1.thd) + ", 50 Hz " + pct (l50.thd));
        report ("Baja ganancia: 2do y 3ro parecidos (medicion BAE)", std::abs (k1.h2 - k1.h3) < 4.0f, juce::String::formatted ("H2 %.1f dB, H3 %.1f dB", k1.h2, k1.h3));
        float maxOut = 0; for (float d = 24.0f; d <= 29.0f; d += 0.25f) if (thdAt (micIdx (50), 1000, d).thd < -40.0f) maxOut = d;
        report ("Salida maxima (1% THD) +26 a +27.5 dBu", maxOut >= 25.75f && maxOut <= 27.75f, juce::String::formatted ("+%.2f dBu", maxOut));
        const T clean = thdAt (micIdx (50), 1000, 6);
        report ("Nivel normal (-18 dBFS) limpio", clean.thd < -80, juce::String::formatted ("THD %.1f dB", clean.thd));
        const T hi = thdAt (micIdx (80), 1000, 20), m60 = thdAt (micIdx (60), 1000, 20);
        report ("MIC alta: menos realimentacion, mas 2do y mas armonicos", hi.thd > m60.thd + 6 && m60.thd > k1.thd + 3 && hi.h2 > hi.h3,
                juce::String::formatted ("+20 dBu: MIC 50 %s, 60 %s, 80 %s (H2 %.1f > H3 %.1f)", pct (k1.thd).toRawUTF8(), pct (m60.thd).toRawUTF8(), pct (hi.thd).toRawUTF8(), hi.h2, hi.h3));
        const T l20 = thdAt (micIdx (50), 20, 20), l1k = thdAt (micIdx (50), 1000, 20), n20 = thdAt (micIdx (50), 20, 6);
        report ("Transformador: graves profundos con color", l20.thd > l1k.thd + 10 && l20.thd < -40, juce::String::formatted ("+20 dBu: 20 Hz %s, 1 kHz %s", pct (l20.thd).toRawUTF8(), pct (l1k.thd).toRawUTF8()));
        report ("Histeresis: peso en graves a nivel normal", n20.thd > -82 && n20.thd < -60, juce::String::formatted ("20 Hz a -18 dBFS: %s (H2 %.1f, H3 %.1f)", pct (n20.thd).toRawUTF8(), n20.h2, n20.h3));
        // al empujar la ganancia satura (con AUTO): MIC 70 a -18 dBFS -> +26 dBu internos
        Top73Processor q; setP (q, "gain", (float) micIdx (70)); auto o = run (q, 1000, 0.126f, 1.0);
        const float push = level (o, 2000) - level (o, 1000), push3 = level (o, 3000) - level (o, 1000);
        report ("MIC 70 a -18 dBFS: saturacion audible", push3 > -45 && push > -50, juce::String::formatted ("H2 %.1f dB, H3 %.1f dB", push, push3));
        Top73Processor z; setP (z, "gain", (float) micIdx (75)); auto oz = run (z, 7000, 0.126f, 1.0);
        float alias = -200; for (double fa : { 1000.0, 3000.0, 5000.0, 9000.0, 11000.0, 13000.0 }) alias = std::max (alias, level (oz, fa) - level (oz, 7000));
        report ("Saturacion fuerte (MIC 75, +31 dBu) sin aliasing", alias < -90, juce::String::formatted ("peor componente fuera de armonicos %.1f dB", alias));
        // brillo de ganancia alta con AUTO
        auto tilt = [] (int dbv) { auto cfg = [dbv] (Top73Processor& p) { setP (p, "gain", (float) micIdx (dbv)); }; return with (cfg, 15000, 0.003f) - with (cfg, 1000, 0.003f); };
        const float t50 = tilt (50), t80 = tilt (80);
        const float a50 = with ([] (Top73Processor& p) { setP (p, "gain", (float) micIdx (50)); }, 1000, 0.003f), a80 = with ([] (Top73Processor& p) { setP (p, "gain", (float) micIdx (80)); }, 1000, 0.003f);
        report ("Ganancia alta abre los agudos; AUTO mantiene el nivel", t80 - t50 > 3.5f && std::abs (a80 - a50) < 1.0f, "15k: " + juce::String (t50, 1) + " -> " + juce::String (t80, 1) + " dB, nivel " + juce::String (a80 - a50, 2) + " dB");
        Top73Processor qq; setP (qq, "auto", 0); setP (qq, "gain", (float) micIdx (60));
        report ("Sin AUTO la ganancia sube (+10 dB en MIC 60)", std::abs (level (run (qq, 1000, 0.003f), 1000) - 20 * std::log10 (0.003f) - 10) < 0.6f, "");
    }
    // 5. saturacion 3D (tecla 3D encendida, como viene por defecto)
    {
        struct H { float thd, h2, h3, rms; };
        auto meas = [] (int gainIdx, double hz, float amp, bool stereoDiff = false)
        {
            Top73Processor p; setP (p, "gain", (float) gainIdx);
            auto o = run (p, hz, amp, 1.2);
            const float f = level (o, hz, 0.6, 1.15);
            double h = 0; for (int k = 2; k <= 9; ++k) { if (hz * k > 23000) break; h += std::pow (10.0, (level (o, hz * k, 0.6, 1.15) - f) / 10.0); }
            double ms = 0; const int a = (int) (0.6 * SR), b = (int) (1.15 * SR); for (int i = a; i < b; ++i) ms += (double) o[(size_t) i] * o[(size_t) i];
            juce::ignoreUnused (stereoDiff);
            return H { (float) (10 * std::log10 (h + 1e-30)), level (o, hz * 2, 0.6, 1.15) - f, level (o, hz * 3, 0.6, 1.15) - f, (float) (10 * std::log10 (ms / (b - a))) };
        };
        const float a18 = 0.126f;                                          // -18 dBFS de pico, nivel normal de voz
        float prev = -200, minStep = 99, maxStep = -99; bool warm = true; juce::String row;
        for (int db = 50; db <= 80; db += 5)
        {
            const H m = meas (micIdx (db), 1000, a18);
            if (db > 50) { minStep = std::min (minStep, m.thd - prev); maxStep = std::max (maxStep, m.thd - prev); }
            if (db <= 70 && m.h2 < m.h3) warm = false;
            prev = m.thd; row << db << ": " << juce::String (m.thd, 0) << "  ";
        }
        report ("3D: la perilla roja agrega armonicos paso a paso", minStep > 2.5f && maxStep < 9.0f && prev > -26.0f, row);
        report ("3D: calido (2do >= 3ro) hasta MIC 70", warm, "");
        const H c50 = meas (micIdx (50), 1000, a18), c80 = meas (micIdx (80), 1000, a18);
        report ("3D: AUTO mantiene el volumen aunque sature", std::abs (c80.rms - c50.rms) < 1.0f, juce::String::formatted ("MIC 50 %.1f dB, MIC 80 %.1f dB", c50.rms, c80.rms));
        const H lo = meas (micIdx (65), 150, a18), hi = meas (micIdx (65), 5000, a18);
        report ("3D: los graves saturan mas que los agudos", lo.thd > hi.thd + 3.0f, juce::String::formatted ("MIC 65: 150 Hz %.1f dB, 5 kHz %.1f dB", lo.thd, hi.thd));
        {
            Top73Processor p; setP (p, "gain", (float) micIdx (70));
            p.setPlayConfigDetails (2, 2, SR, 512); p.prepareToPlay (SR, 512);
            juce::AudioBuffer<float> b (2, 512); double ph = 0, sl = 0, sd = 0, slr = 0, sr2 = 0;
            for (int blk = 0; blk < (int) (SR * 1.0 / 512); ++blk)
            {
                for (int i = 0; i < 512; ++i) { const float v = a18 * (float) std::sin (ph); ph += TWO_PI * 220 / SR; b.setSample (0, i, v); b.setSample (1, i, v); }
                juce::MidiBuffer m; p.processBlock (b, m);
                if (blk * 512 > SR * 0.5)
                    for (int i = 0; i < 512; ++i) { const double l = b.getSample (0, i), r = b.getSample (1, i); sl += l * l; sr2 += r * r; slr += l * r; sd += (l - r) * (l - r); }
            }
            const double diff = 10 * std::log10 (sd / sl + 1e-30), corr = slr / std::sqrt (sl * sr2);
            report ("3D: izquierda y derecha distintas, compatibles en mono", diff > -60 && diff < -20 && corr > 0.99,
                    juce::String::formatted ("diferencia %.1f dB, correlacion %.4f", diff, corr));
        }
        Top73Processor z; setP (z, "gain", (float) micIdx (80)); auto oz = run (z, 7000, a18, 1.0);
        float alias = -200; for (double fa : { 1000.0, 3000.0, 5000.0, 9000.0, 11000.0, 13000.0 }) alias = std::max (alias, level (oz, fa) - level (oz, 7000));
        report ("3D al maximo (MIC 80) sin aliasing", alias < -90, juce::String::formatted ("peor componente fuera de armonicos %.1f dB", alias));
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
            if (mx > 1.5f || mx < 0.02f) { ok = false; std::printf ("   preset %d (%s): pico %.3f\n", i, q.getProgramName (i).toRawUTF8(), mx); }
        }
        report ("Presets estables", ok, juce::String (p.getNumPrograms()) + " presets");
        Top73Processor a; setP (a, "gain", 10); setP (a, "midGain", 5);
        juce::MemoryBlock mb; a.getStateInformation (mb);
        Top73Processor b; b.setStateInformation (mb.getData(), (int) mb.getSize());
        report ("Guardar/cargar estado", (int) b.apvts.getRawParameterValue ("gain")->load() == 10 && std::abs (b.apvts.getRawParameterValue ("midGain")->load() - 5) < 0.01f, "");
        std::unique_ptr<juce::AudioProcessorEditor> ed (a.createEditor()); ed->setSize (Top73Editor::W / 2, Top73Editor::H / 2);
        report ("Editor escalable", ed->getWidth() == Top73Editor::W / 2, juce::String (ed->getWidth()) + " x " + juce::String (ed->getHeight()));
    }
    // 6. carga de CPU: 10 s de audio estereo a 48 kHz con todo el EQ puesto (8x, doble precision)
    {
        Top73Processor p; setP (p, "lowFreq", 2); setP (p, "lowGain", 6); setP (p, "midFreq", 4); setP (p, "midGain", 5); setP (p, "hf", 4); setP (p, "hpf", 2); setP (p, "gain", 9);
        p.setPlayConfigDetails (2, 2, SR, 256); p.prepareToPlay (SR, 256);
        juce::AudioBuffer<float> b (2, 256); juce::MidiBuffer m; juce::Random rnd (3);
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        for (int k = 0; k < (int) (SR * 10 / 256); ++k)
        {
            for (int c = 0; c < 2; ++c) for (int i = 0; i < 256; ++i) b.setSample (c, i, 0.2f * (rnd.nextFloat() - 0.5f));
            p.processBlock (b, m);
        }
        const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
        report ("Carga de CPU (1 nucleo)", ms < 4000.0, juce::String::formatted ("%.1f%% del tiempo real, latencia %d muestras", ms / 100.0, p.getLatencySamples()));
    }
    std::printf (fails == 0 ? "TODAS LAS PRUEBAS OK\n" : "FALLARON %d\n", fails);
    return fails == 0 ? 0 : 1;
}
