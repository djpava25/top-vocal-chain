// Pruebas de Top Smooth: transparencia, reduccion de resonancias, baja latencia, delta, corte maximo,
// foco, cortes, estereo enlazado / L-R / M-S, sidechain, mezcla y presets
#include "TopSmoothProcessor.h"
#include "TopSmoothEditor.h"
#include <cstdio>

static int fails = 0;
static void report (const char* n, bool ok, const juce::String& info) { if (! ok) ++fails; std::printf ("%-44s %s  %s\n", n, info.toRawUTF8(), ok ? "OK" : "FALLO"); }
static const double SR = 44100.0;

// ruido rosado aproximado + resonancia (seno) opcional
static std::vector<float> signal (double secs, float toneAmp, double toneHz, int seed)
{
    juce::Random r (seed);
    std::vector<float> x ((size_t) (SR * secs));
    float b0 = 0, b1 = 0, b2 = 0;
    for (size_t i = 0; i < x.size(); ++i)
    {
        const float w = r.nextFloat() * 2 - 1;
        b0 = 0.99765f * b0 + w * 0.0990460f; b1 = 0.96300f * b1 + w * 0.2965164f; b2 = 0.57000f * b2 + w * 1.0526913f;
        x[i] = (b0 + b1 + b2 + w * 0.1848f) * 0.05f + toneAmp * (float) std::sin (6.283185307 * toneHz * i / SR);
    }
    return x;
}
static float bandDb (const std::vector<float>& x, double hz, double q, double t0, double t1)
{
    tvc::Biquad b; b.setBandPass (SR, hz, q);
    double e = 0; int n = 0;
    for (int i = 0; i < (int) x.size(); ++i)
    {
        const float y = b.process (x[(size_t) i]);
        if (i >= (int) (t0 * SR) && i < (int) (t1 * SR)) { e += y * y; ++n; }
    }
    return (float) (10 * std::log10 (e / std::max (1, n) + 1e-20));
}
static void setP (TopSmoothProcessor& p, const juce::String& id, float v) { auto* q = p.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); }

// procesa (L, R) y devuelve (L, R)
static std::pair<std::vector<float>, std::vector<float>> run (TopSmoothProcessor& p, const std::vector<float>& l, const std::vector<float>& r,
                                                              const std::vector<float>* scL = nullptr, const std::vector<float>* scR = nullptr)
{
    p.prepareToPlay (SR, 512);
    const int chans = p.getTotalNumInputChannels();
    juce::AudioBuffer<float> buf (std::max (2, chans), 512);
    std::vector<float> ol, orr;
    for (size_t pos = 0; pos + 512 <= l.size(); pos += 512)
    {
        buf.clear();
        for (int i = 0; i < 512; ++i)
        {
            buf.setSample (0, i, l[pos + (size_t) i]); buf.setSample (1, i, r[pos + (size_t) i]);
            if (scL != nullptr && chans >= 4) { buf.setSample (2, i, (*scL)[pos + (size_t) i]); buf.setSample (3, i, (*scR)[pos + (size_t) i]); }
        }
        juce::MidiBuffer m;
        p.processBlock (buf, m);
        for (int i = 0; i < 512; ++i) { ol.push_back (buf.getSample (0, i)); orr.push_back (buf.getSample (1, i)); }
    }
    return { ol, orr };
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto x = signal (3.0, 0.08f, 3000.0, 1);
    const float toneIn = bandDb (x, 3000, 30, 1.5, 2.8), noiseIn = bandDb (x, 900, 2, 1.5, 2.8);

    if (argc > 2 && juce::String (argv[1]) == "--snapshot")
    {
        TopSmoothProcessor p;
        setP (p, "depth", 7); setP (p, "n4_g", 5); setP (p, "lowcut", 120);
        run (p, x, x);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        ed->setVisible (true);
        if (auto* te = dynamic_cast<TopSmoothEditor*> (ed.get())) te->tickNow();
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
        f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("snapshot -> %s\n", f.getFullPathName().toRawUTF8());
        return 0;
    }

    // 1. profundidad 0 = transparente (solo retraso)
    {
        TopSmoothProcessor p; setP (p, "depth", 0);
        auto [l, r] = run (p, x, x);
        const int lat = p.getLatencySamples();
        float err = 0;
        for (size_t i = 0; i + (size_t) lat < l.size(); ++i) err = std::max (err, std::abs (l[i + (size_t) lat] - x[i]));
        report ("Profundidad 0 = transparente", err < 1e-4f && lat > 0, "error " + juce::String (err, 7) + ", latencia " + juce::String (lat) + " muestras");
    }
    // 2. baja la resonancia y deja el resto
    float dropDefault = 0;
    {
        TopSmoothProcessor p;
        auto [l, r] = run (p, x, x);
        dropDefault = toneIn - bandDb (l, 3000, 30, 1.5, 2.8);
        const float dn = bandDb (l, 900, 2, 1.5, 2.8) - noiseIn;
        report ("Baja la resonancia de 3 kHz", dropDefault > 6.0f, juce::String (dropDefault, 1) + " dB menos");
        report ("No toca el resto del espectro", std::abs (dn) < 1.5f, juce::String (dn, 2) + " dB en 900 Hz");
    }
    // 3. modo duro corta mas que suave
    {
        TopSmoothProcessor p; setP (p, "mode", 1);
        auto [l, r] = run (p, x, x);
        const float d = toneIn - bandDb (l, 3000, 30, 1.5, 2.8);
        report ("Modo duro corta mas", d > dropDefault, juce::String (d, 1) + " dB vs " + juce::String (dropDefault, 1));
    }
    // 4. baja latencia
    {
        TopSmoothProcessor p; setP (p, "latency", 1);
        auto [l, r] = run (p, x, x);
        const float d = toneIn - bandDb (l, 3000, 30, 1.5, 2.8);
        const float dn = bandDb (l, 900, 2, 1.5, 2.8) - noiseIn;
        report ("Baja latencia: 0 muestras", p.getLatencySamples() == 0, juce::String (p.getLatencySamples()));
        report ("Baja latencia: baja la resonancia", d > 4.0f && std::abs (dn) < 2.0f, juce::String (d, 1) + " dB, resto " + juce::String (dn, 2) + " dB");
    }
    // 5. delta: solo lo que se quita
    {
        TopSmoothProcessor p; setP (p, "delta", 1);
        auto [l, r] = run (p, x, x);
        const float t = bandDb (l, 3000, 30, 1.5, 2.8), nz = bandDb (l, 900, 2, 1.5, 2.8);
        report ("Delta = lo que se quita", t > toneIn - 4 && nz < noiseIn - 10, "tono " + juce::String (t - toneIn, 1) + " dB, resto " + juce::String (nz - noiseIn, 1) + " dB");
    }
    // 6. corte maximo
    {
        TopSmoothProcessor p; setP (p, "depth", 20); setP (p, "maxcut", 3);
        auto [l, r] = run (p, x, x);
        const float d = toneIn - bandDb (l, 3000, 30, 1.5, 2.8);
        report ("Corte maximo 3 dB", d < 3.6f && d > 1.0f, juce::String (d, 2) + " dB");
    }
    // 7. corte agudo en 1.5 kHz: no toca los 3 kHz
    {
        TopSmoothProcessor p; setP (p, "highcut", 1500);
        auto [l, r] = run (p, x, x);
        const float d = toneIn - bandDb (l, 3000, 30, 1.5, 2.8);
        report ("Corte agudo protege los 3 kHz", d < 1.5f, juce::String (d, 2) + " dB");
    }
    // 8. foco: sensibilidad -12 dB en 3 kHz reduce menos
    {
        TopSmoothProcessor p; setP (p, "n3_f", 3000); setP (p, "n3_g", -12);
        auto [l, r] = run (p, x, x);
        const float d = toneIn - bandDb (l, 3000, 30, 1.5, 2.8);
        report ("Foco -12 dB en 3 kHz corta menos", d < dropDefault - 2.0f, juce::String (d, 1) + " dB vs " + juce::String (dropDefault, 1));
    }
    // 9. estereo: resonancia solo en la izquierda
    {
        const auto clean = signal (3.0, 0.0f, 3000.0, 2);
        const float rIn = bandDb (clean, 3000, 30, 1.5, 2.8);
        TopSmoothProcessor a; setP (a, "stereo", 1);
        auto [l1, r1] = run (a, x, clean);
        TopSmoothProcessor b; setP (b, "stereo", 0);
        auto [l2, r2] = run (b, x, clean);
        const float dl = toneIn - bandDb (l1, 3000, 30, 1.5, 2.8), dr = rIn - bandDb (r1, 3000, 30, 1.5, 2.8), drLinked = rIn - bandDb (r2, 3000, 30, 1.5, 2.8);
        report ("L/R: cada lado por separado", dl > 6.0f && std::abs (dr) < 1.0f, "izq -" + juce::String (dl, 1) + " dB, der " + juce::String (-dr, 2) + " dB");
        report ("Enlazado: los dos lados juntos", drLinked > 2.0f, "der -" + juce::String (drLinked, 1) + " dB");
    }
    // 10. mid/side: resonancia solo en los lados
    {
        std::vector<float> l (x.size()), r (x.size());
        const auto n = signal (3.0, 0.0f, 3000.0, 4);
        for (size_t i = 0; i < x.size(); ++i) { const float t = 0.08f * (float) std::sin (6.283185307 * 3000.0 * i / SR); l[i] = n[i] + t; r[i] = n[i] - t; }
        TopSmoothProcessor p; setP (p, "stereo", 2);
        auto [ol, orr] = run (p, l, r);
        std::vector<float> sIn (l.size()), sOut (ol.size());
        for (size_t i = 0; i < l.size(); ++i) sIn[i] = 0.5f * (l[i] - r[i]);
        for (size_t i = 0; i < ol.size(); ++i) sOut[i] = 0.5f * (ol[i] - orr[i]);
        const float d = bandDb (sIn, 3000, 30, 1.5, 2.8) - bandDb (sOut, 3000, 30, 1.5, 2.8);
        report ("Mid/Side: corta la resonancia de los lados", d > 5.0f, juce::String (d, 1) + " dB");
    }
    // 11. sidechain externo
    {
        TopSmoothProcessor p;
        if (auto* bus = p.getBus (true, 1)) bus->enable (true);
        setP (p, "sidechain", 1); setP (p, "depth", 10);
        const auto clean = signal (3.0, 0.0f, 3000.0, 5);
        const float nIn = bandDb (clean, 3000, 8, 1.5, 2.8);
        auto [l, r] = run (p, clean, clean, &x, &x);
        const float d = nIn - bandDb (l, 3000, 8, 1.5, 2.8);
        report ("Sidechain: detecta en otra pista", p.getTotalNumInputChannels() == 4 && d > 3.0f, juce::String (d, 1) + " dB en 3 kHz de la pista principal");
    }
    // 12. mezcla 0 = original (con retraso)
    {
        TopSmoothProcessor p; setP (p, "mix", 0);
        auto [l, r] = run (p, x, x);
        const int lat = p.getLatencySamples();
        float err = 0;
        for (size_t i = 20000; i + (size_t) lat < l.size(); ++i) err = std::max (err, std::abs (l[i + (size_t) lat] - x[i]));
        report ("Mezcla 0 % = original alineado", err < 1e-4f, "error " + juce::String (err, 7));
    }
    // 13. presets estables
    {
        bool ok = true; juce::String bad;
        TopSmoothProcessor p;
        for (int i = 0; i < p.getNumPrograms(); ++i)
        {
            p.setCurrentProgram (i);
            auto [l, r] = run (p, x, x);
            float mx = 0; bool fin = true;
            for (float v : l) { mx = std::max (mx, std::abs (v)); fin = fin && std::isfinite (v); }
            if (! fin || mx > 2.0f || mx < 0.01f) { ok = false; bad << p.getProgramName (i) << " "; }
        }
        report ("Presets estables", ok, juce::String (p.getNumPrograms()) + " presets " + bad);
    }
    std::printf (fails == 0 ? "TODAS LAS PRUEBAS OK\n" : "%d PRUEBAS FALLARON\n", fails);
    return fails == 0 ? 0 : 1;
}
