// Prueba de Top Vocal Chain V3: editor, gate, SFX (5 tipos), SPC POST / FLT POST y captura
#include "PluginProcessor.h"
#include "V3/EditorV3.h"
#include <cstdio>

static int fails = 0;
static void report (const char* n, bool ok, const juce::String& info) { if (! ok) ++fails; std::printf ("%-46s %s  %s\n", n, info.toRawUTF8(), ok ? "OK" : "FALLO"); }

static void set (TopVocalChainProcessor& p, const char* id, float v) { auto* x = p.apvts.getParameter (id); x->setValueNotifyingHost (x->convertTo0to1 (v)); }

// Procesa una senal y devuelve la salida (canal izquierdo)
static std::vector<float> run (std::vector<std::pair<const char*, float>> prm, std::function<float (int)> gen, int blocks = 120)
{
    TopVocalChainProcessor p; p.setPlayConfigDetails (2, 2, 48000, 512);
    for (auto* id : { "tune_on", "pre_on", "eq_on", "comp_on", "rev_on", "dly_on", "wide_on", "air_on", "limit_on" }) set (p, id, 0);
    for (auto& [id, v] : prm) set (p, id, v);
    p.prepareToPlay (48000, 512);
    juce::AudioBuffer<float> b (2, 512); juce::MidiBuffer m; std::vector<float> out;
    for (int k = 0; k < blocks; ++k)
    {
        for (int i = 0; i < 512; ++i) { const float v = gen (k * 512 + i); b.setSample (0, i, v); b.setSample (1, i, v); }
        p.processBlock (b, m);
        for (int i = 0; i < 512; ++i) out.push_back (b.getSample (0, i));
    }
    return out;
}
static double rmsDb (const std::vector<float>& v, size_t from, size_t to) { double s = 0; for (size_t i = from; i < to; ++i) s += v[i] * v[i]; return 10 * std::log10 (s / (double) (to - from) + 1e-20); }
static double toneDb (const std::vector<float>& v, double hz, size_t from, size_t to)
{
    double re = 0, im = 0; for (size_t i = from; i < to; ++i) { const double ph = 6.283185307 * hz * i / 48000.0; re += v[i] * std::cos (ph); im += v[i] * std::sin (ph); }
    return 20 * std::log10 (2.0 * std::sqrt (re * re + im * im) / (double) (to - from) + 1e-12);
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    auto sine = [] (double hz, double a) { return [=] (int i) { return (float) (a * std::sin (6.283185307 * hz * i / 48000.0)); }; };

    // GATE: ruido bajo entre frases se apaga, la voz pasa
    {
        auto gen = [] (int i) { juce::Random r ((juce::int64) i * 7919); const bool voice = (i / 24000) % 2 == 0;
                                return voice ? (float) (0.3 * std::sin (6.283185307 * 220.0 * i / 48000.0)) : (r.nextFloat() - 0.5f) * 0.002f; };
        auto off = run ({}, gen, 200), on = run ({ { "gate_thr", -45 } }, gen, 200);
        // la salida llega con la latencia del plugin (~36 ms): se mide dentro de cada tramo con margen
        const double nOff = rmsDb (off, 80000, 94000), nOn = rmsDb (on, 80000, 94000);
        const double vOff = rmsDb (off, 56000, 70000), vOn = rmsDb (on, 56000, 70000);
        report ("GATE silencia el ruido entre frases", nOn < nOff - 25 && nOn > -190 && std::abs (vOn - vOff) < 0.5, juce::String::formatted ("ruido %.1f -> %.1f dB, voz %.1f -> %.1f", nOff, nOn, vOff, vOn));
    }
    // SFX: cada tipo cambia el sonido sin explotar el volumen
    {
        auto dry = run ({}, sine (440, 0.3));
        const double d3 = toneDb (dry, 1320, 20000, 60000);
        const char* names[] { "Tape Sat", "Tube", "Radio", "Lo-Fi", "Crush" };
        for (int t = 0; t < 5; ++t)
        {
            auto w = run ({ { "sfx_on", 1 }, { "sfx_type", (float) t }, { "sfx_amt", 70 } }, sine (440, 0.3));
            const double lvl = rmsDb (w, 20000, 60000) - rmsDb (dry, 20000, 60000);
            double diff = 0; for (size_t i = 20000; i < 60000; ++i) diff += std::abs (w[i] - dry[i]);
            bool finite = true; for (float v : w) finite &= std::isfinite (v);
            report ((juce::String ("SFX ") + names[t]).toRawUTF8(), finite && diff / 40000 > 0.005 && lvl > -15 && lvl < 6,
                    juce::String::formatted ("nivel %+.1f dB, 3er armonico %.1f -> %.1f dB", lvl, d3, toneDb (w, 1320, 20000, 60000)));
        }
    }
    // SPC POST: con el SFX despues del espacio, la cola de la reverb tambien se satura
    {
        auto burst = [] (int i) { return i < 4800 ? (float) (0.5 * std::sin (6.283185307 * 300.0 * i / 48000.0)) : 0.0f; };
        auto pre = run ({ { "rev_on", 1 }, { "rev_mix", 60 }, { "sfx_on", 1 }, { "sfx_type", 4 }, { "sfx_amt", 100 }, { "sfx_pre_space", 1 } }, burst, 60);
        auto post = run ({ { "rev_on", 1 }, { "rev_mix", 60 }, { "sfx_on", 1 }, { "sfx_type", 4 }, { "sfx_amt", 100 }, { "sfx_pre_space", 0 } }, burst, 60);
        const double tPre = rmsDb (pre, 12000, 30000), tPost = rmsDb (post, 12000, 30000);
        report ("SPC POST cambia el orden SFX / espacio", std::abs (tPre - tPost) > 1.0, juce::String::formatted ("cola: SFX antes %.1f dB, despues %.1f dB", tPre, tPost));
    }
    // FLT POST: los filtros se aplican despues del SFX (quitan los graves que crea la distorsion)
    {
        auto a = run ({ { "sfx_on", 1 }, { "sfx_type", 4 }, { "sfx_amt", 100 }, { "eq_lowcut", 400 } }, sine (100, 0.3));
        auto b = run ({ { "sfx_on", 1 }, { "sfx_type", 4 }, { "sfx_amt", 100 }, { "eq_lowcut", 400 }, { "flt_post", 1 } }, sine (100, 0.3));
        const double la = toneDb (a, 100, 20000, 60000), lb = toneDb (b, 100, 20000, 60000);
        report ("FLT POST filtra despues del SFX", lb < la - 6, juce::String::formatted ("100 Hz: %.1f -> %.1f dB", la, lb));
    }
    // Editor V3 y captura
    {
        TopVocalChainProcessor p; p.setPlayConfigDetails (2, 2, 48000, 512); p.prepareToPlay (48000, 512);
        set (p, "comp_on", 1); set (p, "deess_on", 1); set (p, "deess_amt", 50); set (p, "sfx_on", 1); set (p, "gate_thr", -50);
        set (p, "rev_on", 1); set (p, "dly_on", 1); set (p, "rev_duck", 32); set (p, "dly_duck", 17); set (p, "air_high", 45);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        ed->setVisible (true);
        report ("Editor V3 abre", dynamic_cast<tvc3::EditorV3*> (ed.get()) != nullptr, juce::String (ed->getWidth()) + "x" + juce::String (ed->getHeight()));
        juce::AudioBuffer<float> b (2, 512); juce::MidiBuffer m; double ph = 0; juce::Random rnd (5);
        for (int k = 0; k < 300; ++k)
        {
            for (int i = 0; i < 512; ++i)
            {
                const double tt = (k * 512 + i) / 48000.0;
                ph += 440.0 * std::pow (2.0, (57.0 + 3.0 * std::floor (std::fmod (tt, 2.4) / 0.8) - 69.0) / 12.0) / 48000.0;
                float v = 0; for (int h = 1; h < 10; ++h) v += (float) std::sin (6.283185307 * h * ph) / (h * 1.3f);
                v = v * 0.25f + (std::fmod (tt, 1.3) < 0.08 ? (rnd.nextFloat() - 0.5f) * 0.3f : 0.0f);
                b.setSample (0, i, v); b.setSample (1, i, v);
            }
            p.processBlock (b, m);
            if (k % 3 == 0) if (auto* e = dynamic_cast<tvc3::EditorV3*> (ed.get())) e->tickNow();
        }
        if (argc > 2 && juce::String (argv[1]) == "--snapshot")
        {
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
            f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
            std::printf ("snapshot -> %s\n", f.getFullPathName().toRawUTF8());
        }
    }
    std::printf (fails == 0 ? "TODAS LAS PRUEBAS OK\n" : "%d FALLOS\n", fails);
    return fails;
}
