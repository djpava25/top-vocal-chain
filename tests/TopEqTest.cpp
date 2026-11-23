// Pruebas de Top EQ-1A
#include "TopEqProcessor.h"
#include "TopEqEditor.h"
#include <cstdio>

static int fails = 0;
static void report (const char* n, bool ok, const juce::String& info) { if (! ok) ++fails; std::printf ("%-46s %s  %s\n", n, info.toRawUTF8(), ok ? "OK" : "FALLO"); }
static const double SR = 48000.0, TWO_PI = 6.283185307179586;
static void setP (TopEqProcessor& p, const juce::String& id, float v) { auto* q = p.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); }
static std::vector<float> run (TopEqProcessor& p, double hz, float amp = 0.05f, double secs = 0.8)
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
static float level (const std::vector<float>& x, double hz)
{
    double re = 0, im = 0; const int a = (int) (0.45 * SR), b = (int) (0.75 * SR);
    for (int i = a; i < b; ++i) { const double w = 0.5 - 0.5 * std::cos (TWO_PI * (i - a) / (b - a)); re += x[(size_t) i] * w * std::cos (TWO_PI * hz * i / SR); im += x[(size_t) i] * w * std::sin (TWO_PI * hz * i / SR); }
    return (float) (20 * std::log10 (std::sqrt (re * re + im * im) / (b - a) * 4 + 1e-12));
}
template <typename F> static float with (F setup, double hz, float amp = 0.05f)
{ TopEqProcessor p; setP (p, "tube", 0); setup (p); return level (run (p, hz, amp), hz) - 20.0f * std::log10 (amp); }

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc > 2 && juce::String (argv[1]) == "--snapshot")
    {
        TopEqProcessor p; p.setCurrentProgram (5);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        ed->setVisible (true);
        if (auto* te = dynamic_cast<TopEqEditor*> (ed.get())) te->tickNow();
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
        f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("snapshot -> %s\n", f.getFullPathName().toRawUTF8());
        return 0;
    }
    // plano con todo en 0
    {
        float worst = 0;
        for (double hz : { 30.0, 100.0, 1000.0, 5000.0, 15000.0 }) worst = std::max (worst, std::abs (with ([] (TopEqProcessor&) {}, hz)));
        report ("Todo en 0 = plano", worst < 0.1f, "desvio " + juce::String (worst, 3) + " dB");
    }
    // graves
    {
        const float b = with ([] (TopEqProcessor& p) { setP (p, "lowBoost", 10); setP (p, "lowFreq", 2); }, 30);
        const float a = with ([] (TopEqProcessor& p) { setP (p, "lowAtten", 10); setP (p, "lowFreq", 2); }, 30);
        report ("Low Boost / Low Atten (60 Hz)", b > 11 && b < 16 && a < -9, "boost " + juce::String (b, 1) + " dB, atten " + juce::String (a, 1) + " dB a 30 Hz");
        // el truco: realce abajo y hueco encima
        auto both = [] (double hz) { return with ([] (TopEqProcessor& p) { setP (p, "lowBoost", 8); setP (p, "lowAtten", 8); setP (p, "lowFreq", 2); }, hz); };
        const float sub = both (45), dip = both (250), mids = both (2000);
        report ("Truco boost+atten: grave gordo y hueco encima", sub > 2.5f && dip < -1.5f && std::abs (mids) < 1.0f,
                "45 Hz " + juce::String (sub, 1) + " dB, 250 Hz " + juce::String (dip, 1) + " dB, 2k " + juce::String (mids, 1));
    }
    // agudos
    {
        const float hb = with ([] (TopEqProcessor& p) { setP (p, "highBoost", 10); setP (p, "highFreq", 4); }, 10000);
        auto bw = [] (float b) { return with ([b] (TopEqProcessor& p) { setP (p, "highBoost", 8); setP (p, "highFreq", 4); setP (p, "bandwidth", b); }, 5000); };
        const float sharp = bw (0), broad = bw (10);
        report ("High Boost en la frecuencia (10 kHz)", hb > 14 && hb < 17, juce::String (hb, 1) + " dB");
        report ("Bandwidth: angosta vs ancha", broad > sharp + 4, "a 5 kHz: angosta " + juce::String (sharp, 1) + ", ancha " + juce::String (broad, 1) + " dB");
        const float at = with ([] (TopEqProcessor& p) { setP (p, "highAtten", 10); setP (p, "attFreq", 1); }, 16000), at1k = with ([] (TopEqProcessor& p) { setP (p, "highAtten", 10); setP (p, "attFreq", 1); }, 1000);
        report ("High Atten (10 kHz)", at < -10 && at1k > -1.5f, "16k " + juce::String (at, 1) + " dB, 1k " + juce::String (at1k, 1) + " dB");
    }
    // valvula: color sin cambiar el nivel
    {
        TopEqProcessor p; setP (p, "tube", 100); auto o = run (p, 100, 0.5f);
        const float f = level (o, 100), h2 = level (o, 200) - f, h3 = level (o, 300) - f;
        const float lv = with ([] (TopEqProcessor& q) { setP (q, "tube", 100); }, 1000, 0.01f);
        report ("Valvula: armonicos y nivel estable", h2 > -50 && h3 > -60 && std::abs (lv) < 0.3f, "H2 " + juce::String (h2, 1) + " H3 " + juce::String (h3, 1) + " dB, nivel " + juce::String (lv, 2));
        TopEqProcessor q; setP (q, "tube", 0); auto c = run (q, 100, 0.5f);
        report ("Tube 0 = limpio", level (c, 200) - level (c, 100) < -80, juce::String (level (c, 200) - level (c, 100), 1) + " dB");
    }
    // salida, switch IN, presets, estado
    {
        const float o6 = with ([] (TopEqProcessor& p) { setP (p, "out", 6); }, 1000);
        report ("Output +6 dB", std::abs (o6 - 6) < 0.1f, juce::String (o6, 2) + " dB");
        TopEqProcessor p; setP (p, "power", 0); setP (p, "lowBoost", 10); setP (p, "highBoost", 10);
        auto o = run (p, 440, 0.3f); const int L = p.getLatencySamples(); double err = 0;
        for (int i = 20000; i < 30000; ++i) err = std::max (err, std::abs ((double) o[(size_t) i] - 0.3 * std::sin (TWO_PI * 440 * (i - L) / SR)));
        report ("IN apagado = senal original", err < 1e-4, "error " + juce::String (err, 6) + ", latencia " + juce::String (L));
        bool ok = true;
        for (int i = 0; i < p.getNumPrograms(); ++i)
        {
            TopEqProcessor q; q.setCurrentProgram (i); auto r = run (q, 200, 0.3f, 0.3);
            for (float v : r) if (! std::isfinite (v) || std::abs (v) > 2.0f) ok = false;
        }
        report ("Presets estables", ok, juce::String (p.getNumPrograms()) + " presets");
        TopEqProcessor a; setP (a, "lowBoost", 7); setP (a, "highFreq", 6);
        juce::MemoryBlock mb; a.getStateInformation (mb);
        TopEqProcessor b; b.setStateInformation (mb.getData(), (int) mb.getSize());
        report ("Guardar/cargar estado", std::abs (b.apvts.getRawParameterValue ("lowBoost")->load() - 7) < 0.01f && (int) b.apvts.getRawParameterValue ("highFreq")->load() == 6, "");
        std::unique_ptr<juce::AudioProcessorEditor> ed (a.createEditor()); ed->setSize (900, 353);
        report ("Editor escalable", ed->getWidth() == 900, "");
    }
    std::printf (fails == 0 ? "TODAS LAS PRUEBAS OK\n" : "FALLARON %d\n", fails);
    return fails == 0 ? 0 : 1;
}
