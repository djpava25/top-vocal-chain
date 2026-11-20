// Pruebas offline de Top Strip 737
#include "TopStripProcessor.h"
#include "TopStripEditor.h"
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

static juce::AudioBuffer<float> run (TopStripProcessor& p, std::vector<std::pair<const char*, float>> params,
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


static double gainAt (std::vector<std::pair<const char*, float>> prm, double f, double amp = 0.05)
{
    TopStripProcessor p;
    auto o = run (p, prm, sine (f, amp), sine (f, amp), 0.6);
    return db (tone (o.getReadPointer (0) + 14400, 9600, f) / amp);
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc > 2 && juce::String (argv[1]) == "--snapshot")
    {
        TopStripProcessor p;
        run (p, { { "comp_on", 1 }, { "eq_on", 1 }, { "hpf_on", 1 }, { "bass_gain", 4 }, { "hm_gain", 3 }, { "tr_gain", 4 } },
             sine (220, 0.3), sine (220, 0.3), 0.5);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        ed->setVisible (true);
        if (auto* te = dynamic_cast<TopStripEditor*> (ed.get())) for (int i = 0; i < 20; ++i) te->tickNow();
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1500.0f / (float) ed->getWidth());
        juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
        f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("snapshot -> %s\n", f.getFullPathName().toRawUTF8());
        return 0;
    }

    // 1. Neutro: transparente a nivel de trabajo
    {
        const double g = gainAt ({}, 1000, 0.125);
        TopStripProcessor p;
        const double t = thd (run (p, {}, sine (1000, 0.125), sine (1000, 0.125)), 1000);
        report ("Neutro 1 kHz -18 dBFS", std::abs (g) < 0.5 && t < -35, juce::String::formatted ("ganancia %+.2f dB  THD %.1f dB", g, t));
    }
    // 2. Preamp gain y PAD
    {
        const double g10 = gainAt ({ { "pre_gain", 10 } }, 1000, 0.01), gp = gainAt ({ { "pad", 1 } }, 1000, 0.3);
        report ("Preamp +10 dB / Pad -20", std::abs (g10 - 10) < 0.6 && std::abs (gp + 20) < 0.6, juce::String::formatted ("%+.2f / %+.2f dB", g10, gp));
    }
    // 3. MIC = mas valvula al mismo nivel
    {
        TopStripProcessor a, b;
        const double tl = thd (run (a, { { "pre_gain", 6 } }, sine (500, 0.15), sine (500, 0.15)), 500);
        const double tm = thd (run (b, { { "pre_gain", 6 }, { "mic", 1 } }, sine (500, 0.15), sine (500, 0.15)), 500);
        const double gm = gainAt ({ { "mic", 1 } }, 1000, 0.01);
        report ("MIC: mas armonicos, mismo nivel", tm > tl + 6 && std::abs (gm) < 0.5, juce::String::formatted ("THD %.1f -> %.1f dB, nivel %+.2f dB", tl, tm, gm));
    }
    // 4. Pasa altos
    {
        const double g = gainAt ({ { "hpf_on", 1 }, { "hpf", 140 } }, 40), g1k = gainAt ({ { "hpf_on", 1 }, { "hpf", 140 } }, 1000);
        report ("High pass 140 Hz", g < -14 && std::abs (g1k) < 0.5, juce::String::formatted ("40 Hz %+.1f dB, 1 kHz %+.1f dB", g, g1k));
    }
    // 5. EQ: cada banda
    {
        const double b = gainAt ({ { "eq_on", 1 }, { "bass_gain", 12 }, { "bass_freq", 3 } }, 60);
        const double lm = gainAt ({ { "eq_on", 1 }, { "lm_gain", 10 }, { "lm_freq", 4 } }, 100);
        const double lmx = gainAt ({ { "eq_on", 1 }, { "lm_gain", 10 }, { "lm_freq", 4 }, { "lm_x10", 1 } }, 1000);
        const double hm = gainAt ({ { "eq_on", 1 }, { "hm_gain", -10 }, { "hm_freq", 8 } }, 2000);
        const double tr = gainAt ({ { "eq_on", 1 }, { "tr_gain", 10 }, { "tr_freq", 0 } }, 15000);
        const double off = gainAt ({ { "eq_on", 0 }, { "bass_gain", 12 } }, 60);
        report ("EQ bass/low mid/x10/high mid/treble", b > 9 && lm > 9 && lmx > 9 && hm < -9 && tr > 8 && std::abs (off) < 0.5,
                juce::String::formatted ("%+.1f %+.1f %+.1f %+.1f %+.1f (off %+.1f)", b, lm, lmx, hm, tr, off));
    }
    // 6. Hi-Q estrecha la banda
    {
        const double wide = gainAt ({ { "eq_on", 1 }, { "hm_gain", 12 }, { "hm_freq", 4 } }, 1600);
        const double nar = gainAt ({ { "eq_on", 1 }, { "hm_gain", 12 }, { "hm_freq", 4 }, { "hm_hiq", 1 } }, 1600);
        report ("Hi-Q mas estrecho", nar < wide - 3, juce::String::formatted ("a 1.6 kHz: normal %+.1f, Hi-Q %+.1f dB", wide, nar));
    }
    // 7. Compresor optico: reduce a nivel alto, no toca a nivel bajo
    {
        const double loud = gainAt ({ { "comp_on", 1 }, { "thr", -24 }, { "ratio", 4 } }, 1000, 0.5);
        const double soft = gainAt ({ { "comp_on", 1 }, { "thr", -24 }, { "ratio", 4 } }, 1000, 0.005);
        TopStripProcessor p; run (p, { { "comp_on", 1 }, { "thr", -24 }, { "ratio", 4 } }, sine (1000, 0.5), sine (1000, 0.5), 0.6);
        report ("Compresor -24 dB 4:1", loud < -8 && std::abs (soft) < 0.5 && p.grDb.load() < -8,
                juce::String::formatted ("fuerte %+.1f dB, suave %+.1f dB, GR %.1f", loud, soft, p.grDb.load()));
    }
    // 8. SC LINK (de-esser) comprime mas las altas que los graves
    {
        auto prm = std::vector<std::pair<const char*, float>> { { "comp_on", 1 }, { "sc_link", 1 }, { "thr", -20 }, { "ratio", 6 }, { "hm_freq", 10 } };
        const double hi = gainAt (prm, 3000, 0.2), lo = gainAt (prm, 200, 0.2);
        report ("SC LINK: de-esser", hi < lo - 4, juce::String::formatted ("200 Hz %+.1f dB, 3 kHz %+.1f dB", lo, hi));
    }
    // 9. Fase y Power (bypass alineado)
    {
        TopStripProcessor p;
        auto o = run (p, { { "phase", 1 } }, sine (1000, 0.1), sine (1000, 0.1), 0.3);
        TopStripProcessor q;
        auto by = run (q, { { "power", 0 }, { "pre_gain", 20 }, { "eq_on", 1 }, { "bass_gain", 20 } }, sine (1000, 0.1), sine (1000, 0.1), 0.3);
        const int lat = q.getLatencySamples();
        double err = 0, corr = 0;
        for (int i = 4000; i < 14000; ++i)
        {
            const double ref = 0.1 * std::sin (2 * juce::MathConstants<double>::pi * 1000 * (i - lat) / SR);
            err = std::max (err, std::abs (by.getSample (0, i) - ref));
            corr += o.getSample (0, i) * by.getSample (0, i);
        }
        report ("Power off = bypass exacto / Phase invierte", err < 1e-4 && corr < 0, juce::String::formatted ("error bypass %.6f, latencia %d", err, lat));
    }
    // 10. Presets cargan sin romper el nivel
    {
        bool ok = true; juce::String info;
        for (int i = 0; i < TopStripProcessor().getNumPrograms(); ++i)
        {
            TopStripProcessor p; p.setPlayConfigDetails (2, 2, SR, 512); p.setCurrentProgram (i);
            auto o = run (p, {}, sine (440, 0.1), sine (440, 0.1), 0.5);
            const double pk = db (o.getMagnitude (0, 0, o.getNumSamples()));
            if (pk > 0 || pk < -40 || std::isnan (pk)) ok = false;
            info << juce::String (pk, 1) << " ";
        }
        report ("Presets (pico dBFS)", ok, info);
    }

    std::printf (fails == 0 ? "TODAS LAS PRUEBAS OK\n" : "%d FALLOS\n", fails);
    return fails == 0 ? 0 : 1;
}
