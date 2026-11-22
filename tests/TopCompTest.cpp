// Pruebas offline de Top Blue Comp
#include "TopCompProcessor.h"
#include "TopCompEditor.h"
#include <cstdio>

static int fails = 0;
static const double SR = 48000.0;

static double tone (const float* x, int n, double f)
{
    double re = 0, im = 0;
    for (int i = 0; i < n; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (2 * juce::MathConstants<double>::pi * i / (n - 1));
        re += x[i] * w * std::cos (2 * juce::MathConstants<double>::pi * f * i / SR);
        im += x[i] * w * std::sin (2 * juce::MathConstants<double>::pi * f * i / SR);
    }
    return std::sqrt (re * re + im * im) * 4.0 / n;
}

static double db (double v) { return 20.0 * std::log10 (v + 1e-12); }

static juce::AudioBuffer<float> run (TopCompProcessor& p, std::vector<std::pair<const char*, float>> params,
                                     std::function<float (int)> genL, std::function<float (int)> genR, double secs = 1.0)
{
    p.setPlayConfigDetails (2, 2, SR, 512);
    for (auto& [id, v] : params) { auto* prm = p.apvts.getParameter (id); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); }
    p.prepareToPlay (SR, 512);
    const int total = (int) (SR * secs);
    juce::AudioBuffer<float> b (2, total);
    for (int i = 0; i < total; ++i) { b.setSample (0, i, genL (i)); b.setSample (1, i, genR (i)); }
    juce::MidiBuffer m;
    for (int pos = 0; pos < total; pos += 512)
    {
        juce::AudioBuffer<float> sub (b.getArrayOfWritePointers(), 2, pos, std::min (512, total - pos));
        p.processBlock (sub, m);
    }
    return b;
}

static std::function<float (int)> sine (double f, double amp)
{
    return [f, amp] (int i) { return (float) (amp * std::sin (2 * juce::MathConstants<double>::pi * f * i / SR)); };
}
static std::function<float (int)> silence() { return [] (int) { return 0.0f; }; }

static void report (const char* name, bool ok, const juce::String& info)
{
    if (! ok) ++fails;
    std::printf ("%-44s %s  %s\n", name, info.toRawUTF8(), ok ? "OK" : "FALLO");
}

static double thd (const juce::AudioBuffer<float>& b, double f)
{
    const float* x = b.getReadPointer (0) + (int) (SR * 0.5);
    const int n = (int) (SR * 0.4);
    const double f1 = tone (x, n, f);
    double h = 0;
    for (int k = 2; k <= 7; ++k) { const double a = tone (x, n, f * k); h += a * a; }
    return db (std::sqrt (h) / f1);
}


static double harmonicDb (const juce::AudioBuffer<float>& b, double f, int k)
{
    const int from = b.getNumSamples() / 2, len = b.getNumSamples() / 3;
    return db (tone (b.getReadPointer (0) + from, len, f * k) / (tone (b.getReadPointer (0) + from, len, f) + 1e-12));
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc > 2 && juce::String (argv[1]) == "--snapshot")
    {
        TopCompProcessor p;
        run (p, { { "thr", -24 }, { "ratio", 4 }, { "gain", 6 } }, sine (220, 0.4), sine (220, 0.4), 0.5);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        ed->setVisible (true);
        if (auto* te = dynamic_cast<TopCompEditor*> (ed.get())) for (int i = 0; i < 25; ++i) te->tickNow();
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1400.0f / (float) ed->getWidth());
        juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
        f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("snapshot -> %s\n", f.getFullPathName().toRawUTF8());
        return 0;
    }
    auto level = [] (const juce::AudioBuffer<float>& b, double f) { const int from = b.getNumSamples() / 2; return db (tone (b.getReadPointer (0) + from, b.getNumSamples() / 3, f)); };

    // 1. Por debajo del threshold: transparente en nivel (la valvula colorea sin subir el volumen)
    {
        TopCompProcessor p; auto o = run (p, { { "thr", -10 } }, sine (1000, 0.01), sine (1000, 0.01));
        const double g = level (o, 1000) - db (0.01), h2 = harmonicDb (o, 1000, 2);
        report ("Sin comprimir: nivel igual, casi limpio", std::abs (g) < 0.5 && h2 < -55, juce::String::formatted ("ganancia %+.2f dB, 2o armonico %.1f dB", g, h2));
        TopCompProcessor q; auto o2 = run (q, { { "thr", 0 }, { "color", 200 } }, sine (1000, 0.01), sine (1000, 0.01));
        report ("Drive no sube el volumen", std::abs (level (o2, 1000) - db (0.01)) < 0.5, juce::String::formatted ("%+.2f dB con COLOR 200%%", level (o2, 1000) - db (0.01)));
    }
    // 2. Comprime segun threshold y ratio
    {
        TopCompProcessor p; auto o = run (p, { { "thr", -30 }, { "ratio", 4 }, { "select", 2 }, { "attack", 10 } }, sine (1000, 0.3), sine (1000, 0.3));
        report ("Comprime (-30 dB, 4:1)", p.grDb.load() < -10 && level (o, 1000) < db (0.3) - 8, juce::String::formatted ("GR %.1f dB, nivel %+.1f dB", p.grDb.load(), level (o, 1000) - db (0.3)));
    }
    // 3. Codo suave de 10 dB: justo en el threshold ya comprime un poco (~1 dB a 4:1)
    {
        TopCompProcessor p; run (p, { { "thr", db (0.3) }, { "ratio", 4 }, { "select", 2 }, { "attack", 10 } }, sine (1000, 0.3), sine (1000, 0.3));
        const float gr = -p.grDb.load();
        report ("Codo suave 10 dB", gr > 0.5f && gr < 1.6f, juce::String::formatted ("GR en el threshold %.2f dB (teoria 0.94)", gr));
    }
    // 4. Valvula: 2do armonico (asimetria de triodo) y sube con COLOR
    {
        TopCompProcessor a, b;
        auto lo = run (a, { { "thr", -20 }, { "ratio", 4 }, { "gain", 10 }, { "color", 50 } }, sine (1000, 0.3), sine (1000, 0.3));
        auto hi = run (b, { { "thr", -20 }, { "ratio", 4 }, { "gain", 10 }, { "color", 200 } }, sine (1000, 0.3), sine (1000, 0.3));
        const double l2 = harmonicDb (lo, 1000, 2), h2 = harmonicDb (hi, 1000, 2), h3 = harmonicDb (hi, 1000, 3);
        report ("Valvula: 2o y 3er armonico, crecen con COLOR", h2 > l2 + 6 && h2 > -60 && h3 > -60, juce::String::formatted ("H2 %.1f -> %.1f dB, H3 %.1f dB", l2, h2, h3));
    }
    // 5. Transformador de salida: +0.8 dB en graves
    {
        TopCompProcessor a, b;
        auto lo = run (a, { { "thr", 0 }, { "color", 0 } }, sine (50, 0.01), sine (50, 0.01));
        auto mi = run (b, { { "thr", 0 }, { "color", 0 } }, sine (1000, 0.01), sine (1000, 0.01));
        const double d = (level (lo, 50) - db (0.01)) - (level (mi, 1000) - db (0.01));
        report ("Transformador: +0.8 dB en graves", d > 0.5 && d < 1.0, juce::String::formatted ("50 Hz vs 1 kHz %+.2f dB", d));
    }
    // 6. Release optico en dos etapas: suelta rapido al principio y lento al final, sin saltos
    {
        TopCompProcessor p;
        auto gen = [] (int i) { const double t = i / SR; return (float) ((t < 1.0 ? 0.5 : 0.02) * std::sin (2 * juce::MathConstants<double>::pi * 1000 * t)); };
        p.setPlayConfigDetails (2, 2, SR, 64);
        for (auto [id, v] : std::vector<std::pair<const char*, float>> { { "thr", -30 }, { "ratio", 10 }, { "select", 2 }, { "attack", 10 }, { "release", 30 } })
        { auto* prm = p.apvts.getParameter (id); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); }
        p.prepareToPlay (SR, 64);
        juce::AudioBuffer<float> b (2, 64); juce::MidiBuffer m; std::vector<float> grs;
        for (int blk = 0; blk < (int) (2.5 * SR / 64); ++blk)
        {
            for (int i = 0; i < 64; ++i) { const float v = gen (blk * 64 + i); b.setSample (0, i, v); b.setSample (1, i, v); }
            p.processBlock (b, m); grs.push_back (-p.grDb.load());
        }
        auto at = [&grs] (double t) { return grs[(size_t) (t * SR / 64)]; };
        const float g0 = at (0.99), g1 = at (1.05), g2 = at (1.4), g3 = at (2.4);
        float jump = 0; for (size_t k = (size_t) (1.0 * SR / 64); k + 1 < grs.size(); ++k) jump = std::max (jump, grs[k] - grs[k + 1]);
        const float fastRate = (g0 - g2) / 0.41f, slowRate = (g2 - g3) / 1.0f;
        report ("Release optico en dos etapas", fastRate > 3.0f * slowRate && slowRate > 0 && g3 >= 0,
                juce::String::formatted ("GR %.1f -> %.1f -> %.1f -> %.1f dB (%.0f dB/s y luego %.0f dB/s)", g0, g1, g2, g3, fastRate, slowRate));
        report ("Sin saltos al soltar", jump < 1.5f, juce::String::formatted ("salto max %.2f dB por bloque", jump));
    }
    // 7. Graves fuertes no se distorsionan por el detector
    {
        TopCompProcessor a; auto o = run (a, { { "thr", -30 }, { "ratio", 10 }, { "color", 0 }, { "select", 2 }, { "attack", 0 } }, sine (60, 0.4), sine (60, 0.4), 1.5);
        report ("Detector no distorsiona graves", thd (o, 60) < -30, juce::String::formatted ("THD %.1f dB a 60 Hz", thd (o, 60)));
    }
    // 8. COLOR 0 = compresion limpia
    {
        TopCompProcessor a; auto o = run (a, { { "thr", -36 }, { "ratio", 10 }, { "color", 0 } }, sine (1000, 0.3), sine (1000, 0.3));
        report ("COLOR 0 = limpio", harmonicDb (o, 1000, 2) < -60, juce::String::formatted ("2o armonico %.1f dB", harmonicDb (o, 1000, 2)));
    }
    // 6. Sidechain HPF: el bajo deja de hacer comprimir
    {
        TopCompProcessor a, b;
        run (a, { { "thr", -30 }, { "hpf", 0 } }, sine (50, 0.3), sine (50, 0.3));
        run (b, { { "thr", -30 }, { "hpf", 5 } }, sine (50, 0.3), sine (50, 0.3));
        report ("Sidechain HPF", b.grDb.load() > a.grDb.load() + 6, juce::String::formatted ("GR sin filtro %.1f, con 500 Hz %.1f dB", a.grDb.load(), b.grDb.load()));
    }
    // 7. Modo M: una senal solo en los lados (L = -R) no hace comprimir
    {
        TopCompProcessor a, b;
        run (a, { { "thr", -30 }, { "mode", 1 } }, sine (1000, 0.3), sine (1000, -0.3));
        run (b, { { "thr", -30 }, { "mode", 0 } }, sine (1000, 0.3), sine (1000, -0.3));
        report ("Modo M (solo centro)", a.grDb.load() > -0.5 && b.grDb.load() < -10, juce::String::formatted ("GR modo M %.1f, LR %.1f dB", a.grDb.load(), b.grDb.load()));
    }
    // 8. DRY/WET 0 e IN apagado = bypass exacto
    {
        TopCompProcessor a, b;
        auto o1 = run (a, { { "thr", -36 }, { "mix", 0 } }, sine (1000, 0.3), sine (1000, 0.3));
        auto o2 = run (b, { { "thr", -36 }, { "power", 0 } }, sine (1000, 0.3), sine (1000, 0.3));
        const int lat = a.getLatencySamples(); double e = 0;
        for (int i = 4000; i < 20000; ++i)
        {
            const double ref = 0.3 * std::sin (2 * juce::MathConstants<double>::pi * 1000 * (i - lat) / SR);
            e = std::max ({ e, std::abs (o1.getSample (0, i) - ref), std::abs (o2.getSample (0, i) - ref) });
        }
        report ("DRY/WET 0 e IN off = original exacto", e < 1e-4, juce::String::formatted ("error %.6f", e));
    }
    // 9. Delta: escucha lo que se quita (sin compresion casi silencio)
    {
        TopCompProcessor a; auto o = run (a, { { "thr", 0 }, { "mode", 3 }, { "color", 0 } }, sine (1000, 0.05), sine (1000, 0.05));
        report ("Delta sin compresion = casi silencio", level (o, 1000) < db (0.05) - 30, juce::String::formatted ("%.1f dB", level (o, 1000)));
    }
    // 10. Presets
    {
        bool ok = true; juce::String info;
        for (int i = 0; i < TopCompProcessor().getNumPrograms(); ++i)
        {
            TopCompProcessor p; p.setPlayConfigDetails (2, 2, SR, 512); p.setCurrentProgram (i);
            auto o = run (p, {}, sine (220, 0.3), sine (220, 0.3), 0.5);
            const double pk = db (o.getMagnitude (0, 0, o.getNumSamples()));
            if (! std::isfinite (pk) || pk > 3 || pk < -30) ok = false;
            info << juce::String (pk, 1) << " ";
        }
        report ("Presets (pico dBFS)", ok, info);
    }
    std::printf (fails == 0 ? "TODAS LAS PRUEBAS OK\n" : "%d FALLOS\n", fails);
    return fails == 0 ? 0 : 1;
}
