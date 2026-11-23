// Pruebas de 6961P
#include "VbxProcessor.h"
#include "VbxEditor.h"
#include <cstdio>

static int fails = 0;
static void report (const char* n, bool ok, const juce::String& info) { if (! ok) ++fails; std::printf ("%-50s %s  %s\n", n, info.toRawUTF8(), ok ? "OK" : "FALLO"); }
static const double SR = 48000.0, TWO_PI = 6.283185307179586;
static void setP (VbxProcessor& p, const juce::String& id, float v) { auto* q = p.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); }
static void plain (VbxProcessor& p) { setP (p, "comp", 0); setP (p, "deess", 0); setP (p, "xfmr", 0); }
static std::vector<float> run (VbxProcessor& p, std::function<float (int)> gen, double secs)
{
    p.setPlayConfigDetails (2, 2, SR, 256); p.prepareToPlay (SR, 256);
    juce::AudioBuffer<float> b (2, 256); std::vector<float> out; int n = 0;
    for (int blk = 0; blk < (int) (SR * secs / 256); ++blk)
    {
        for (int i = 0; i < 256; ++i, ++n) { const float v = gen (n); b.setSample (0, i, v); b.setSample (1, i, v); }
        juce::MidiBuffer m; p.processBlock (b, m);
        for (int i = 0; i < 256; ++i) out.push_back (b.getSample (0, i));
    }
    return out;
}
static std::function<float (int)> sine (double hz, float amp) { return [hz, amp] (int n) { return amp * (float) std::sin (TWO_PI * hz * n / SR); }; }
static float level (const std::vector<float>& x, double hz, double t0 = 0.45, double t1 = 0.75)
{
    double re = 0, im = 0; const int a = (int) (t0 * SR), b = (int) (t1 * SR);
    for (int i = a; i < b; ++i) { const double w = 0.5 - 0.5 * std::cos (TWO_PI * (i - a) / (b - a)); re += x[(size_t) i] * w * std::cos (TWO_PI * hz * i / SR); im += x[(size_t) i] * w * std::sin (TWO_PI * hz * i / SR); }
    return (float) (20 * std::log10 (std::sqrt (re * re + im * im) / (b - a) * 4 + 1e-12));
}
static float rmsDb (const std::vector<float>& x, double t0, double t1)
{
    double e = 0; const int a = (int) (t0 * SR), b = (int) (t1 * SR);
    for (int i = a; i < b; ++i) e += (double) x[(size_t) i] * x[(size_t) i];
    return (float) (10 * std::log10 (e / (b - a) + 1e-20) + 3.01);
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc > 2 && juce::String (argv[1]) == "--snapshot")
    {
        VbxProcessor p; p.setCurrentProgram (argc > 3 ? juce::String (argv[3]).getIntValue() : 1);
        run (p, sine (300, 0.3f), 0.4);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        ed->setVisible (true); if (argc > 4) ed->setSize (VbxEditor::W, VbxEditor::H);
        if (auto* te = dynamic_cast<VbxEditor*> (ed.get())) te->tickNow();
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
        f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("snapshot -> %s\n", f.getFullPathName().toRawUTF8());
        return 0;
    }
    // 1. EQ contra las mediciones por impulso (presets ajustados), 50 Hz - 16 kHz, relativo a 1 kHz
    {
        struct Ref { int preset; const char* name; float v[12]; };
        const double hz[12] { 31, 50, 100, 200, 500, 1000, 2000, 3000, 5000, 8000, 10000, 16000 };
        const Ref refs[] {
            { 4, "Acoustic Guitar", { -5.75f, -3.67f, -1.79f, -0.70f, -0.19f, 0, 0.46f, 1.12f, 2.81f, 5.34f, 6.23f, 3.04f } },
            { 10, "Drums", { -0.31f, 3.27f, 3.86f, 2.14f, 0.32f, 0, 0.25f, 0.37f, 0.46f, 0.72f, 0.84f, 0.34f } },
            { 12, "Piano", { 3.35f, 5.92f, 6.05f, 3.28f, 0.21f, 0, 2.91f, 1.97f, 2.21f, 2.31f, 2.29f, 2.09f } },
            { 6, "Electric Guitar", { 2.11f, 5.74f, 4.50f, 1.14f, -0.60f, 0, 0.75f, 2.21f, 0.85f, 0.63f, 0.58f, 0.47f } },
            { 3, "Sexy Female Voice", { 3.26f, 3.34f, 2.90f, 2.05f, -0.26f, 0, 1.61f, 2.24f, 2.82f, 3.97f, 4.86f, 7.62f } } };
        for (const auto& r : refs)
        {
            VbxProcessor p; p.setCurrentProgram (r.preset);
            const auto s = p.eqSettings();
            float worst = 0; double at = 0;
            for (int k = 1; k < 12; ++k)
            {
                const float e = std::abs ((vbx::responseDb (s, hz[k]) - vbx::responseDb (s, 1000)) - r.v[k]);
                if (e > worst) { worst = e; at = hz[k]; }
            }
            report ((juce::String ("EQ vs medicion: ") + r.name).toRawUTF8(), worst < 2.5f, "error max " + juce::String (worst, 2) + " dB @ " + juce::String ((int) at) + " Hz");
        }
    }
    // 2. plano con todo en 0
    {
        float worst = 0, low = 0; juce::String info;
        for (double hz : { 30.0, 100.0, 1000.0, 8000.0, 18000.0 })
        {
            VbxProcessor p; plain (p); const float e = level (run (p, sine (hz, 0.01f), 0.8), hz) - 20 * std::log10 (0.01f);
            if (hz < 50) low = e; else worst = std::max (worst, std::abs (e));
            info << (int) hz << ":" << juce::String (e, 2) << " ";
        }
        report ("Plano: +-0.3 dB 100 Hz-18 kHz, +-1 dB en 30 Hz (spec)", worst < 0.3f && std::abs (low) < 1.0f, info);
    }
    // 3. compresor optico 3:1
    {
        auto outLv = [] (float amp) { VbxProcessor p; plain (p); setP (p, "comp", 1); setP (p, "thr", 8); setP (p, "attack", 0); auto o = run (p, sine (1000, amp), 1.6); return level (o, 1000, 1.2, 1.5); };
        const float a = outLv (0.1f), b = outLv (0.316f);
        const float slope = 10.0f / (b - a);
        report ("Compresor 3:1", slope > 2.6f && slope < 3.4f, juce::String::formatted ("+10 dB entrada -> +%.2f dB salida (ratio %.2f)", b - a, slope));
        VbxProcessor q; plain (q); setP (q, "comp", 1); setP (q, "thr", 10); run (q, sine (1000, 0.9f), 1.0);
        report ("Reduccion maxima 16 dB", q.mComp.load() <= 16.01f && q.mComp.load() > 14.0f, juce::String (q.mComp.load(), 1) + " dB");
    }
    // 4. ataque y release por pasos
    {
        auto burst = [] (int att, int rel)
        {
            VbxProcessor p; plain (p); setP (p, "comp", 1); setP (p, "thr", 6); setP (p, "attack", (float) att); setP (p, "release", (float) rel);
            auto o = run (p, [] (int n) { const double t = n / SR; return (float) ((t >= 0.5 && t < 1.5 ? 0.5 : 0.02) * std::sin (TWO_PI * 1000 * t)); }, 3.0);
            return std::make_pair (rmsDb (o, 0.503, 0.508) - rmsDb (o, 1.3, 1.45), rmsDb (o, 1.75, 1.8) - 20.0f * std::log10 (0.02f));
        };
        const auto f = burst (0, 0), s = burst (4, 0), rf = burst (2, 0), rs = burst (2, 4);
        report ("Attack FAST deja pasar menos pico que SLOW", s.first > f.first + 2, juce::String::formatted ("pico sobre lo estable: FAST %.1f dB, SLOW %.1f dB", f.first, s.first));
        report ("Release 0.3 s recupera antes que 5 s", rs.second < rf.second - 2, juce::String::formatted ("reduccion 250 ms despues: 0.3 s %.1f dB, 5 s %.1f dB", -rf.second, -rs.second));
    }
    // 5. de-esser y limitador
    {
        auto ds = [] (double hz, int mode) { VbxProcessor p; plain (p); setP (p, "deess", 1); setP (p, "dsF", (float) mode); setP (p, "dsThr", 8); return level (run (p, sine (hz, 0.3f), 0.8), hz) - 20 * std::log10 (0.3f); };
        const float s6 = ds (6000, 1), l1 = ds (1000, 1);
        report ("De-esser 6K: baja la sibilancia, no los medios", s6 < -6 && l1 > -1, juce::String::formatted ("6 kHz %.1f dB, 1 kHz %.1f dB", s6, l1));
        auto lim = [] (float amp) { VbxProcessor p; plain (p); setP (p, "deess", 1); setP (p, "dsF", 4); setP (p, "dsThr", 6); return level (run (p, sine (1000, amp), 0.8), 1000); };
        const float d = lim (0.5f) - lim (0.158f);
        report ("LIMIT 10:1", d < 2.0f, juce::String::formatted ("+10 dB entrada -> +%.2f dB salida", d));
    }
    // 6. preamp calibrado con la especificacion del VOXBOX (0 dBFS = +22 dBu)
    {
        // preamp solo (como la salida PREAMP OUT), a 8x igual que en el plugin
        struct H { float thd, h2, h3; };
        auto preTHD = [] (int g, double hz, float outDbu)
        {
            const double r = SR * 8.0; vbx::Preamp pa; pa.prepare (r); pa.set (g);
            const float drive = std::pow (10.0f, 5.0f * (g - 2) / 20.0f);
            const float amp = std::pow (10.0f, (outDbu - vbx::dbuRef) / 20.0f) / drive;
            const int N = (int) (r * 0.6), a0 = (int) (r * 0.3);
            std::vector<float> y ((size_t) N);
            for (int i = 0; i < N; ++i) y[(size_t) i] = pa.process (amp * (float) std::sin (TWO_PI * hz * i / r), drive);
            auto L = [&] (double f) { double re = 0, im = 0; for (int i = a0; i < N; ++i) { const double w = 0.5 - 0.5 * std::cos (TWO_PI * (i - a0) / (N - a0)); re += y[(size_t) i] * w * std::cos (TWO_PI * f * i / r); im += y[(size_t) i] * w * std::sin (TWO_PI * f * i / r); } return std::sqrt (re * re + im * im); };
            const double f1 = L (hz); double e = 0; for (int k = 2; k <= 9; ++k) { const double hk = L (hz * k); e += hk * hk; }
            return H { (float) (100.0 * std::sqrt (e) / f1), (float) (20 * std::log10 (L (hz * 2) / f1)), (float) (20 * std::log10 (L (hz * 3) / f1)) };
        };
        const auto s20 = preTHD (2, 1000, 20.0f);
        report ("Spec: THD preamp 0.3% a +20 dBu (GAIN 50)", s20.thd > 0.2f && s20.thd < 0.35f, juce::String::formatted ("%.3f%% (H2 %.1f, H3 %.1f dB)", s20.thd, s20.h2, s20.h3));
        float maxOut = 0; for (float d = 24.0f; d <= 34.0f; d += 0.25f) if (preTHD (2, 1000, d).thd < 1.5f) maxOut = d;
        report ("Spec: salida maxima ~+30 dBu a 1.5% THD", maxOut >= 29.0f && maxOut <= 31.5f, juce::String::formatted ("+%.2f dBu", maxOut));
        const auto c4 = preTHD (2, 1000, 4.0f);
        report ("Nivel normal (-18 dBFS = +4 dBu) limpio y calido", c4.thd < 0.08f && c4.h2 > c4.h3 + 10, juce::String::formatted ("THD %.3f%%, H2 %.1f, H3 %.1f dB", c4.thd, c4.h2, c4.h3));
        const auto g40 = preTHD (0, 1000, 20.0f), g60 = preTHD (4, 1000, 20.0f);
        report ("GAIN 40 limpio, 60 caliente (realimentacion)", g40.thd < s20.thd * 0.5f && g60.thd > s20.thd * 2.0f && g60.h2 > g60.h3,
                juce::String::formatted ("a +20 dBu: 40 %.2f%%, 50 %.2f%%, 60 %.2f%% (H2 %.1f > H3 %.1f)", g40.thd, s20.thd, g60.thd, g60.h2, g60.h3));
        const auto b50 = preTHD (2, 30, 20.0f);
        report ("Transformadores: color solo en graves profundos", b50.thd > s20.thd && b50.thd < 2.0f, juce::String::formatted ("30 Hz %.2f%% vs 1 kHz %.2f%%", b50.thd, s20.thd));
        // cadena completa (preamp + EQ) a +20 dBu: spec 0.35 %
        {
            VbxProcessor p; plain (p); const float amp = std::pow (10.0f, (20.0f - vbx::dbuRef) / 20.0f);
            auto o = run (p, sine (1000, amp), 0.8); const float f1 = level (o, 1000); double e = 0;
            for (int k = 2; k <= 9; ++k) e += std::pow (10.0, (level (o, 1000.0 * k) - f1) / 10.0);
            const float thd = (float) (100.0 * std::sqrt (e));
            report ("Spec: preamp + EQ 0.35% a +20 dBu", thd > 0.25f && thd < 0.45f, juce::String::formatted ("%.3f%%", thd));
        }
        // GAIN sube 5 dB por paso como el hardware, y mas armonicos
        {
            auto pre = [] (int g) { VbxProcessor p; plain (p); setP (p, "gain", (float) g); auto o = run (p, sine (200, 0.1f), 0.8); return std::make_pair (level (o, 400) - level (o, 200), level (o, 200)); };
            const auto a = pre (0), b = pre (4);
            report ("GAIN 40 -> 60: +20 dB y mas 2do armonico", b.first > a.first + 20 && std::abs (b.second - a.second - 20.0f) < 0.5f,
                    juce::String::formatted ("H2 %.1f -> %.1f dB, nivel %+.2f dB", a.first, b.first, b.second - a.second));
        }
        // aliasing: GAIN 60 recortando fuerte a 7 kHz
        {
            VbxProcessor p; plain (p); setP (p, "gain", 4); setP (p, "input", 6); auto o = run (p, sine (7000, 0.7f), 0.8);
            const float f1 = level (o, 7000); float worst = -200;
            for (double f = 300; f < 20000; f += 100) { bool harm = false; for (int k = 1; k < 4; ++k) if (std::abs (f - 7000.0 * k) < 250) harm = true; if (! harm) worst = std::max (worst, level (o, f) - f1); }
            report ("Saturacion fuerte sin aliasing (8x)", worst < -90, juce::String::formatted ("peor componente fuera de armonicos %.1f dB", worst));
        }
        auto hp = [] (int m, double hz) { VbxProcessor p; plain (p); setP (p, "hpf", (float) m); return level (run (p, sine (hz, 0.01f), 1.0), hz) - 20 * std::log10 (0.01f); };
        const float h40 = hp (1, 40), h20 = hp (1, 20);
        report ("HPF 80 Hz suave (6 dB/oct)", std::abs (h40 + 7.0f) < 1.0f && h20 - h40 < -4.5f && h20 - h40 > -7.0f, juce::String::formatted ("40 Hz %.1f dB, 20 Hz %.1f dB", h40, h20));
        auto x3 = [] (bool on) { VbxProcessor p; plain (p); setP (p, "xfmr", on ? 1.0f : 0.0f); auto o = run (p, sine (40, 0.6f), 1.0); return level (o, 120, 0.5, 0.9) - level (o, 40, 0.5, 0.9); };
        report ("XFMR: 3er armonico en graves", x3 (true) > x3 (false) + 6, juce::String::formatted ("%.1f -> %.1f dB", x3 (false), x3 (true)));
    }
    // 7. power, presets, estado, editor
    {
        VbxProcessor p; setP (p, "power", 0); setP (p, "low", 10); auto o = run (p, sine (440, 0.3f), 0.7);
        const int L = p.getLatencySamples(); double err = 0;
        for (int i = 20000; i < 30000; ++i) err = std::max (err, std::abs ((double) o[(size_t) i] - 0.3 * std::sin (TWO_PI * 440 * (i - L) / SR)));
        report ("POWER apagado = senal original", err < 1e-4, "error " + juce::String (err, 6));
        bool ok = true;
        for (int i = 0; i < p.getNumPrograms(); ++i)
        {
            VbxProcessor q; q.setCurrentProgram (i); auto r = run (q, sine (200, 0.25f), 0.4);
            float mx = 0; for (float v : r) { if (! std::isfinite (v)) ok = false; mx = std::max (mx, std::abs (v)); }
            if (mx > 2.5f || mx < 0.01f) ok = false;
        }
        report ("Presets estables", ok, juce::String (p.getNumPrograms()) + " presets");
        VbxProcessor a; setP (a, "hiF", 9); setP (a, "thr", 6.5f);
        juce::MemoryBlock mb; a.getStateInformation (mb);
        VbxProcessor b; b.setStateInformation (mb.getData(), (int) mb.getSize());
        report ("Guardar/cargar estado", (int) b.apvts.getRawParameterValue ("hiF")->load() == 9 && std::abs (b.apvts.getRawParameterValue ("thr")->load() - 6.5f) < 0.01f, "");
        std::unique_ptr<juce::AudioProcessorEditor> ed (a.createEditor()); ed->setSize (VbxEditor::W, VbxEditor::H);
        report ("Editor escalable", ed->getWidth() == VbxEditor::W, "");
    }
    std::printf (fails == 0 ? "TODAS LAS PRUEBAS OK\n" : "FALLARON %d\n", fails);
    return fails == 0 ? 0 : 1;
}
