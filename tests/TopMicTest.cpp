// Pruebas de Top Mic Collection
#include "TopMicProcessor.h"
#include "TopMicEditor.h"
#include <cstdio>

static int fails = 0;
static void report (const char* n, bool ok, const juce::String& info) { if (! ok) ++fails; std::printf ("%-46s %s  %s\n", n, info.toRawUTF8(), ok ? "OK" : "FALLO"); }
static const double SR = 48000.0;
static void setP (TopMicProcessor& p, const juce::String& id, float v) { auto* q = p.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); }
static std::vector<float> run (TopMicProcessor& p, double hz, float amp = 0.05f, double secs = 0.8)
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
static double corrWithSine (const std::vector<float>& x, double hz)
{
    double s = 0; const int a = (int) (0.5 * SR), b = (int) (0.75 * SR);
    for (int i = a; i < b; ++i) s += x[(size_t) i] * std::sin (2 * juce::MathConstants<double>::pi * hz * i / SR);
    return s;
}
static float level (const std::vector<float>& x, double hz)
{
    double re = 0, im = 0; const int a = (int) (0.45 * SR), b = (int) (0.75 * SR);
    for (int i = a; i < b; ++i) { const double w = 0.5 - 0.5 * std::cos (6.283185307 * (i - a) / (b - a)); re += x[(size_t) i] * w * std::cos (6.283185307 * hz * i / SR); im += x[(size_t) i] * w * std::sin (6.283185307 * hz * i / SR); }
    return (float) (20 * std::log10 (std::sqrt (re * re + im * im) / (b - a) * 4 + 1e-12));
}
static float resp (TopMicProcessor& p, double hz) { return level (run (p, hz), hz) - 20.0f * std::log10 (0.05f); }
template <typename F> static float with (F setup, double hz) { TopMicProcessor p; setup (p); return resp (p, hz); }

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc > 2 && juce::String (argv[1]) == "--snapshot")
    {
        TopMicProcessor p;
        setP (p, "model", argc > 3 ? juce::String (argv[3]).getFloatValue() : 0.0f); setP (p, "source", 4); setP (p, "prox", 25); setP (p, "axis", 20);
        if (argc > 4) { setP (p, "dual", 1); setP (p, "modelB", 11); }
        run (p, 220, 0.3f, 0.3);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        ed->setVisible (true);
        if (auto* te = dynamic_cast<TopMicEditor*> (ed.get())) { p.inPeak = 0.3f; te->tickNow(); te->tickNow(); }
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
        f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("snapshot -> %s\n", f.getFullPathName().toRawUTF8());
        return 0;
    }
    // cada modelo suena y es estable
    {
        bool ok = true; juce::String info;
        for (int m = 0; m < (int) tmic::models().size(); ++m)
        {
            TopMicProcessor p; setP (p, "model", (float) m); auto o = run (p, 1000, 0.3f);
            float mx = 0; bool fin = true; for (float v : o) { mx = std::max (mx, std::abs (v)); fin = fin && std::isfinite (v); }
            if (! fin || mx < 0.05f || mx > 1.2f) { ok = false; info << tmic::models()[(size_t) m].name << " "; }
        }
        report ("15 microfonos estables", ok, info);
    }
    // caracter: la cinta vintage es oscura, la valvula hip-hop es brillante
    {
        auto tilt = [] (int m) { return with ([m] (TopMicProcessor& p) { setP (p, "model", (float) m); }, 9000) - with ([m] (TopMicProcessor& p) { setP (p, "model", (float) m); }, 800); };
        const float dark = tilt (12), bright = tilt (5), dyn = with ([] (TopMicProcessor& p) { setP (p, "model", 8); }, 5500) - with ([] (TopMicProcessor& p) { setP (p, "model", 8); }, 800);
        report ("Cinta oscura / valvula hip-hop brillante", dark < -4 && bright > 4, "cinta " + juce::String (dark, 1) + " dB, valvula " + juce::String (bright, 1) + " dB (9k vs 800)");
        report ("Dinamico: presencia en 5.5 kHz", dyn > 3.5f, juce::String (dyn, 1) + " dB");
    }
    // TM-V6 (valvula de referencia): presencia al frente y aire
    {
        auto r = [] (double hz) { return with ([] (TopMicProcessor& p) { setP (p, "model", 14); }, hz); };
        const float pres = r (3000) - r (800), air = r (12000) - r (800);
        TopMicProcessor q; setP (q, "model", 14); setP (q, "pattern", 0);
        report ("TM-V6: presencia y aire, solo cardioide", pres > 1.5f && air > 2.5f && q.effectivePattern (false) == tmic::Cardioid,
                "3k +" + juce::String (pres, 1) + " dB, 12k +" + juce::String (air, 1) + " dB");
    }
    // proximidad
    {
        const float p0 = with ([] (TopMicProcessor& p) { setP (p, "model", 2); }, 100);
        const float p1 = with ([] (TopMicProcessor& p) { setP (p, "model", 2); setP (p, "prox", 100); }, 100);
        const float o1 = with ([] (TopMicProcessor& p) { setP (p, "model", 2); setP (p, "prox", 100); setP (p, "pattern", 0); }, 100);
        report ("Proximidad: cardioide +graves, omni no", p1 - p0 > 5 && std::abs (o1 - with ([] (TopMicProcessor& p) { setP (p, "model", 2); setP (p, "pattern", 0); }, 100)) < 0.5f,
                "cardioide +" + juce::String (p1 - p0, 1) + " dB en 100 Hz");
    }
    // fuera de eje
    {
        const float on8 = with ([] (TopMicProcessor& p) { setP (p, "model", 2); }, 8000), off8 = with ([] (TopMicProcessor& p) { setP (p, "model", 2); setP (p, "axis", 90); }, 8000);
        const float on5 = with ([] (TopMicProcessor& p) { setP (p, "model", 2); }, 500), off5 = with ([] (TopMicProcessor& p) { setP (p, "model", 2); setP (p, "axis", 90); }, 500);
        report ("Fuera de eje: pierde agudos", (on8 - off8) > (on5 - off5) + 2.5f, "8k " + juce::String (off8 - on8, 1) + " dB / 500 Hz " + juce::String (off5 - on5, 1) + " dB");
    }
    // figura 8: la parte de atras invierte la fase
    {
        TopMicProcessor a; setP (a, "model", 11); auto fa = run (a, 500);
        TopMicProcessor b; setP (b, "model", 11); setP (b, "axis", 180); auto fb = run (b, 500);
        report ("Figura 8: atras = fase invertida", corrWithSine (fa, 500) * corrWithSine (fb, 500) < 0, "");
        TopMicProcessor c; setP (c, "model", 11); setP (c, "pattern", 2);
        report ("Cinta solo admite figura 8", c.effectivePattern (false) == tmic::Figure8, tmic::patternName (c.effectivePattern (false)));
    }
    // quitar el micro de origen: grabar con dinamico tipo 57 y elegir ese mismo modelo ~ plano en medios/agudos
    {
        float worst = 0;
        for (double hz : { 1000.0, 3000.0, 5500.0, 8000.0 })
            worst = std::max (worst, std::abs (with ([] (TopMicProcessor& p) { setP (p, "model", 8); setP (p, "source", 1); }, hz)));
        report ("Quita el color del micro de origen", worst < 1.5f, "desvio max " + juce::String (worst, 2) + " dB");
    }
    // micros del usuario: TLM 103 con el modelo estilo 103 queda plano; C414 XLII quita su brillo
    {
        float worst = 0;
        for (double hz : { 200.0, 1000.0, 5000.0, 8000.0, 12000.0 })
            worst = std::max (worst, std::abs (with ([] (TopMicProcessor& p) { setP (p, "model", 7); setP (p, "source", 7); }, hz)));
        report ("SOURCE TLM 103 + modelo estilo 103 = plano", worst < 0.5f, "desvio max " + juce::String (worst, 2) + " dB");
        const float flat = with ([] (TopMicProcessor& p) { setP (p, "model", 1); }, 8500), c414 = with ([] (TopMicProcessor& p) { setP (p, "model", 1); setP (p, "source", 8); }, 8500);
        report ("SOURCE C414 XLII quita su presencia", flat - c414 > 3.0f, juce::String (c414 - flat, 1) + " dB en 8.5 kHz");
    }
    // filtro
    {
        const float f0 = with ([] (TopMicProcessor& p) { setP (p, "model", 1); }, 50), f3 = with ([] (TopMicProcessor& p) { setP (p, "model", 1); setP (p, "filter", 3); }, 50);
        report ("Filtro 3 corta graves", f0 - f3 > 10, juce::String (f3 - f0, 1) + " dB en 50 Hz");
    }
    // dual
    {
        const float a = with ([] (TopMicProcessor& p) { setP (p, "model", 5); }, 10000);
        const float d = with ([] (TopMicProcessor& p) { setP (p, "model", 5); setP (p, "dual", 1); setP (p, "modelB", 12); setP (p, "blend", 100); }, 10000);
        const float b = with ([] (TopMicProcessor& p) { setP (p, "model", 12); }, 10000);
        report ("Dual: mezcla A/B", std::abs (d - b) < 0.3f && a - b > 6, "100% B = " + juce::String (d, 1) + " dB (B solo " + juce::String (b, 1) + ")");
    }
    // apagado = original
    {
        TopMicProcessor p; setP (p, "power", 0); setP (p, "model", 12);
        report ("POWER apagado: sin cambios", std::abs (resp (p, 8000)) < 0.01f, "");
    }
    // presets y estado
    {
        bool ok = true; TopMicProcessor p;
        for (int i = 0; i < p.getNumPrograms(); ++i)
        {
            TopMicProcessor q; q.setCurrentProgram (i); auto o = run (q, 300, 0.3f);
            float mx = 0; for (float v : o) { if (! std::isfinite (v)) ok = false; mx = std::max (mx, std::abs (v)); }
            if (mx < 0.03f || mx > 1.5f) ok = false;
        }
        report ("Presets estables", ok, juce::String (p.getNumPrograms()) + " presets");
        p.setCurrentProgram (2);
        juce::MemoryBlock mb; p.getStateInformation (mb);
        TopMicProcessor q; q.setStateInformation (mb.getData(), (int) mb.getSize());
        report ("Estado guardado", q.getCurrentProgram() == 2 && (int) q.apvts.getRawParameterValue ("model")->load() == 5, "");
    }
    std::printf (fails == 0 ? "TODAS LAS PRUEBAS OK\n" : "%d PRUEBAS FALLARON\n", fails);
    return fails == 0 ? 0 : 1;
}
