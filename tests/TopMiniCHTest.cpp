// Pruebas de Top Mini CH: transparencia, ganancia, fase, LINE, filtro 75 Hz, EQ shelf/campana, compresor 2:1, presets
#include "TopMiniCHProcessor.h"
#include "TopMiniCHEditor.h"
#include <cstdio>

static int fails = 0;
static void report (const char* n, bool ok, const juce::String& info) { if (! ok) ++fails; std::printf ("%-46s %s  %s\n", n, info.toRawUTF8(), ok ? "OK" : "FALLO"); }
static const double SR = 48000.0;
static void setP (TopMiniCHProcessor& p, const juce::String& id, float v) { auto* q = p.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); }
static std::vector<float> run (TopMiniCHProcessor& p, double hz, float amp, double secs = 1.0)
{
    p.setPlayConfigDetails (2, 2, SR, 512); p.prepareToPlay (SR, 512);
    juce::AudioBuffer<float> b (2, 512); std::vector<float> out; double ph = 0;
    for (int blk = 0; blk < (int) (SR * secs / 512); ++blk)
    {
        for (int i = 0; i < 512; ++i) { const float v = amp * (float) std::sin (ph); ph += 2 * juce::MathConstants<double>::pi * hz / SR; b.setSample (0, i, v); b.setSample (1, i, v); }
        juce::MidiBuffer m; p.processBlock (b, m);
        for (int i = 0; i < 512; ++i) out.push_back (b.getSample (0, i));
    }
    return out;
}
static float level (const std::vector<float>& x, double hz, double t0 = 0.6, double t1 = 0.95)
{
    double re = 0, im = 0; const int a = (int) (t0 * SR), b = (int) (t1 * SR);
    for (int i = a; i < b; ++i) { const double w = 0.5 - 0.5 * std::cos (6.283185307 * (i - a) / (b - a)); re += x[(size_t) i] * w * std::cos (6.283185307 * hz * i / SR); im += x[(size_t) i] * w * std::sin (6.283185307 * hz * i / SR); }
    return (float) (20 * std::log10 (std::sqrt (re * re + im * im) / (b - a) * 4 + 1e-12));
}
static float diff (TopMiniCHProcessor& p, double hz, float amp = 0.1f) { return level (run (p, hz, amp), hz) - 20.0f * std::log10 (amp); }

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc > 2 && juce::String (argv[1]) == "--snapshot")
    {
        TopMiniCHProcessor p;
        setP (p, "gain", 6); setP (p, "hf", 7); setP (p, "lf", -3); setP (p, "hpf", 1); setP (p, "cmp", 1); setP (p, "thr", -8); setP (p, "color", 1);
        run (p, 220, 0.35f, 0.5);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        ed->setSize (TopMiniCHEditor::W, TopMiniCHEditor::H); ed->setVisible (true);
        if (auto* te = dynamic_cast<TopMiniCHEditor*> (ed.get())) { p.outPeak = 0.25f; for (int i = 0; i < 3; ++i) te->tickNow(); }
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
        f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("snapshot -> %s\n", f.getFullPathName().toRawUTF8());
        return 0;
    }
    {
        TopMiniCHProcessor p; auto in = 0.2f; auto o = run (p, 440, in);
        float err = 0; double ph = 0;
        for (size_t i = 0; i < o.size(); ++i) { const float v = in * (float) std::sin (ph); ph += 2 * juce::MathConstants<double>::pi * 440 / SR; if (i > 4800) err = std::max (err, std::abs (o[i] - v)); }
        report ("Por defecto: transparente", err < 2e-3f, "error " + juce::String (err, 5));
    }
    { TopMiniCHProcessor p; setP (p, "gain", 12); report ("Gain +12 dB", std::abs (diff (p, 1000) - 12) < 0.2f, juce::String (diff (p, 1000), 2) + " dB"); }
    { TopMiniCHProcessor p; setP (p, "line", 1); report ("LINE: 9 dB menos", std::abs (diff (p, 1000) + 9) < 0.2f, juce::String (diff (p, 1000), 2) + " dB"); }
    {
        TopMiniCHProcessor p; setP (p, "hpf", 1);
        const float a = diff (p, 75), b = diff (p, 19), c = diff (p, 1000);
        report ("Filtro 75 Hz 12 dB/oct", std::abs (a + 3) < 0.7f && b < -20 && std::abs (c) < 0.1f, juce::String (a, 1) + " dB en 75 / " + juce::String (b, 1) + " dB en 19 Hz");
    }
    {
        TopMiniCHProcessor p; setP (p, "hf", 12);
        report ("HF shelf 3.5 kHz +12", diff (p, 12000) > 10.5f && std::abs (diff (p, 3500) - 6) < 1.5f && std::abs (diff (p, 300)) < 0.5f,
                juce::String (diff (p, 12000), 1) + " dB en 12k, " + juce::String (diff (p, 3500), 1) + " en 3.5k");
        TopMiniCHProcessor q; setP (q, "hf", 12); setP (q, "hf_bell", 1);
        report ("HF campana 5 kHz", std::abs (diff (q, 5000) - 12) < 0.4f && diff (q, 16000) < 6, juce::String (diff (q, 5000), 1) + " en 5k, " + juce::String (diff (q, 16000), 1) + " en 16k");
    }
    {
        TopMiniCHProcessor p; setP (p, "lf", -10);
        report ("LF shelf 60 Hz -10", diff (p, 25) < -8.5f && std::abs (diff (p, 1000)) < 0.4f, juce::String (diff (p, 25), 1) + " dB en 25 Hz");
        TopMiniCHProcessor q; setP (q, "lf", 10); setP (q, "lf_bell", 1);
        report ("LF campana 200 Hz", std::abs (diff (q, 200) - 10) < 0.4f && diff (q, 30) < 4, juce::String (diff (q, 200), 1) + " en 200, " + juce::String (diff (q, 30), 1) + " en 30 Hz");
        TopMiniCHProcessor r; setP (r, "lf", 10); setP (r, "eq_in", 0);
        report ("EQ IN apagado", std::abs (diff (r, 60)) < 0.1f, "");
    }
    {
        // compresor 2:1: 10 dB mas de entrada por encima del umbral -> ~5 dB mas de salida
        TopMiniCHProcessor a; setP (a, "cmp", 1); setP (a, "thr", -20);
        TopMiniCHProcessor b; setP (b, "cmp", 1); setP (b, "thr", -20);
        const float la = level (run (a, 500, 0.1f), 500), lb = level (run (b, 500, 0.316f), 500);
        report ("Compresor 2:1", std::abs ((lb - la) - 5.0f) < 1.0f && a.grDb() > 3.0f, juce::String (lb - la, 2) + " dB de salida por 10 dB de entrada, GR " + juce::String (a.grDb(), 1));
        TopMiniCHProcessor c; setP (c, "cmp", 1); setP (c, "thr", 10);
        report ("Umbral alto: no comprime", std::abs (diff (c, 500, 0.1f) - 0.0f) < 0.3f, "");
    }
    {
        TopMiniCHProcessor p; setP (p, "phase", 1); auto o = run (p, 440, 0.2f);
        double ph = 0; float err = 0;
        for (size_t i = 0; i < o.size(); ++i) { const float v = 0.2f * (float) std::sin (ph); ph += 2 * juce::MathConstants<double>::pi * 440 / SR; if (i > 4800) err = std::max (err, std::abs (o[i] + v)); }
        report ("Fase invertida", err < 2e-3f, "");
    }
    {
        TopMiniCHProcessor p; setP (p, "color", 1); auto o = run (p, 200, 0.5f);
        const float h2 = level (o, 400) - level (o, 200), h3 = level (o, 600) - level (o, 200);
        report ("COLOR agrega armonicos", std::max (h2, h3) > -50 && std::max (h2, h3) < -15, "2o " + juce::String (h2, 1) + " / 3o " + juce::String (h3, 1) + " dB");
        TopMiniCHProcessor q; setP (q, "hiz", 1);
        report ("Hi-Z: mas brillo", diff (q, 5500) > 1.0f && std::abs (diff (q, 300)) < 0.5f, juce::String (diff (q, 5500), 2) + " dB en 5.5k");
    }
    {
        bool ok = true; juce::String bad;
        TopMiniCHProcessor p;
        for (int i = 0; i < p.getNumPrograms(); ++i)
        {
            TopMiniCHProcessor q; q.setCurrentProgram (i); auto o = run (q, 180, 0.3f);
            float mx = 0; bool fin = true; for (float v : o) { mx = std::max (mx, std::abs (v)); fin = fin && std::isfinite (v); }
            if (! fin || mx > 1.5f || mx < 0.02f) { ok = false; bad << q.getProgramName (i) << " "; }
        }
        report ("Presets estables", ok, juce::String (p.getNumPrograms()) + " presets " + bad);
        p.setCurrentProgram (1);
        juce::MemoryBlock mb; p.getStateInformation (mb);
        TopMiniCHProcessor q; q.setStateInformation (mb.getData(), (int) mb.getSize());
        report ("Estado guardado", q.getCurrentProgram() == 1 && q.apvts.getRawParameterValue ("cmp")->load() > 0.5f, "");
    }
    std::printf (fails == 0 ? "TODAS LAS PRUEBAS OK\n" : "%d PRUEBAS FALLARON\n", fails);
    return fails == 0 ? 0 : 1;
}
