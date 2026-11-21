// Pruebas de Top Console B: transparencia, preamp, filtros, EQ, compresor, de-esser, gate, expansor,
// routing al sidechain, sidechain externo, fader VCA, cut, HQ, presets y estado
#include "TopConsoleBProcessor.h"
#include "TopConsoleBEditor.h"
#include <cstdio>

static int fails = 0;
static void report (const char* n, bool ok, const juce::String& info) { if (! ok) ++fails; std::printf ("%-46s %s  %s\n", n, info.toRawUTF8(), ok ? "OK" : "FALLO"); }
static const double SR = 44100.0;
using Stereo = std::pair<std::vector<float>, std::vector<float>>;

static void setP (TopConsoleBProcessor& p, const juce::String& id, float v) { auto* q = p.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); }
static Stereo run (TopConsoleBProcessor& p, const Stereo& in, const Stereo* sc = nullptr)
{
    p.prepareToPlay (SR, 512);
    const int ch = p.getTotalNumInputChannels();
    juce::AudioBuffer<float> b (std::max (2, ch), 512);
    Stereo out;
    for (size_t pos = 0; pos + 512 <= in.first.size(); pos += 512)
    {
        b.clear();
        for (int i = 0; i < 512; ++i)
        {
            b.setSample (0, i, in.first[pos + (size_t) i]); b.setSample (1, i, in.second[pos + (size_t) i]);
            if (sc != nullptr && ch >= 4) { b.setSample (2, i, sc->first[pos + (size_t) i]); b.setSample (3, i, sc->second[pos + (size_t) i]); }
        }
        juce::MidiBuffer m; p.processBlock (b, m);
        for (int i = 0; i < 512; ++i) { out.first.push_back (b.getSample (0, i)); out.second.push_back (b.getSample (1, i)); }
    }
    return out;
}
static Stereo sine (double hz, float amp, double secs)
{
    Stereo s;
    for (int i = 0; i < (int) (SR * secs); ++i) { const float v = amp * (float) std::sin (6.283185307 * hz * i / SR); s.first.push_back (v); s.second.push_back (v); }
    return s;
}
static float tone (const std::vector<float>& x, double hz, double t0, double t1)
{
    double re = 0, im = 0; const int a = (int) (t0 * SR), b = (int) (t1 * SR);
    for (int i = a; i < b; ++i) { const double w = 0.5 - 0.5 * std::cos (6.283185307 * (i - a) / (b - a)); re += x[(size_t) i] * w * std::cos (6.283185307 * hz * i / SR); im += x[(size_t) i] * w * std::sin (6.283185307 * hz * i / SR); }
    return (float) (20 * std::log10 (std::sqrt (re * re + im * im) / (b - a) * 4 + 1e-12));
}
static float rms (const std::vector<float>& x, double t0, double t1)
{
    double e = 0; int n = 0;
    for (int i = (int) (t0 * SR); i < (int) (t1 * SR) && i < (int) x.size(); ++i) { e += (double) x[(size_t) i] * x[(size_t) i]; ++n; }
    return (float) (10 * std::log10 (e / std::max (1, n) + 1e-14));
}
static float diff (TopConsoleBProcessor& p, double hz, float amp = 0.1f)
{
    auto in = sine (hz, amp, 1.0);
    return tone (run (p, in).first, hz, 0.4, 0.9) - tone (in.first, hz, 0.4, 0.9);
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc > 2 && juce::String (argv[1]) == "--snapshot")
    {
        TopConsoleBProcessor p;
        p.setCurrentProgram (argc > 3 ? juce::String (argv[3]).getIntValue() : 2);
        juce::Random r (3); Stereo n; for (int i = 0; i < 44100; ++i) { const float v = (r.nextFloat() - 0.5f) * 0.4f; n.first.push_back (v); n.second.push_back (v); }
        run (p, n);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        ed->setVisible (true);
        if (auto* te = dynamic_cast<TopConsoleBEditor*> (ed.get())) for (int i = 0; i < 10; ++i) te->tickNow();
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
        f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("snapshot -> %s\n", f.getFullPathName().toRawUTF8());
        return 0;
    }

    // 1. por defecto: canal limpio (sin color ni compresion)
    {
        TopConsoleBProcessor p; auto in = sine (440, 0.2f, 1.0);
        auto out = run (p, in);
        float err = 0; for (size_t i = 0; i < out.first.size(); ++i) err = std::max (err, std::abs (out.first[i] - in.first[i]));
        report ("Por defecto: transparente", err < 1e-4f, "error " + juce::String (err, 6));
    }
    // 2. preamp: armonicos con volumen compensado
    {
        TopConsoleBProcessor p; setP (p, "drive", 100);
        auto in = sine (100, 0.3f, 1.0);
        auto o = run (p, in);
        const float h3 = tone (o.first, 300, 0.4, 0.9) - tone (o.first, 100, 0.4, 0.9), h2 = tone (o.first, 200, 0.4, 0.9) - tone (o.first, 100, 0.4, 0.9);
        const float lv = rms (o.first, 0.4, 0.9) - rms (in.first, 0.4, 0.9);
        report ("Preamp: armonicos y nivel parecido", std::max (h2, h3) > -40 && std::abs (lv) < 2.5f, "2o " + juce::String (h2, 1) + " / 3o " + juce::String (h3, 1) + " dB, nivel " + juce::String (lv, 1));
    }
    // 3. filtros
    {
        TopConsoleBProcessor a; setP (a, "hpf", 200);
        report ("HPF 200 Hz: 18 dB/oct", diff (a, 50) < -30.0f, juce::String (diff (a, 50), 1) + " dB en 50 Hz");
        TopConsoleBProcessor b; setP (b, "lpf", 4000);
        report ("LPF 4 kHz", diff (b, 12000) < -15.0f, juce::String (diff (b, 12000), 1) + " dB en 12 kHz");
        TopConsoleBProcessor c; setP (c, "hpf", 200); setP (c, "filt_in", 0);
        report ("FILT IN apagado", std::abs (diff (c, 50)) < 0.05f, juce::String (diff (c, 50), 2) + " dB");
    }
    // 4. EQ
    {
        TopConsoleBProcessor a; setP (a, "lmf_g", 10); setP (a, "lmf_f", 1000); setP (a, "lmf_q", 1);
        report ("LMF +10 dB en 1 kHz", std::abs (diff (a, 1000) - 10) < 0.3f, juce::String (diff (a, 1000), 2) + " dB");
        TopConsoleBProcessor b; setP (b, "hmf_g", -12); setP (b, "hmf_f", 3000); setP (b, "hmf_q", 2);
        report ("HMF -12 dB en 3 kHz", std::abs (diff (b, 3000) + 12) < 0.3f, juce::String (diff (b, 3000), 2) + " dB");
        TopConsoleBProcessor c; setP (c, "lf_g", 8); setP (c, "lf_f", 100);
        report ("LF shelf +8 dB", diff (c, 40) > 6.5f, juce::String (diff (c, 40), 1) + " dB en 40 Hz");
        TopConsoleBProcessor d; setP (d, "lf_g", 8); setP (d, "lf_f", 100); setP (d, "lf_bell", 1);
        report ("LF en campana: no sube los subgraves", diff (d, 25) < diff (c, 25) - 3.0f, juce::String (diff (d, 25), 1) + " dB en 25 Hz");
        TopConsoleBProcessor e; setP (e, "hf_g", 10); setP (e, "hf_f", 8000);
        report ("HF shelf +10 dB", diff (e, 15000) > 8.5f, juce::String (diff (e, 15000), 1) + " dB en 15 kHz");
    }
    // 5. compresor, de-esser
    {
        TopConsoleBProcessor a; setP (a, "c_ratio", 4); setP (a, "c_thr", -30);
        auto in = sine (200, 0.5f, 1.5);
        auto o = run (a, in);
        const float d = tone (o.first, 200, 1.0, 1.4) - tone (in.first, 200, 1.0, 1.4);
        report ("Compresor 4:1 comprime", d < -10.0f && a.compGr() > 10.0f, juce::String (d, 1) + " dB, GR " + juce::String (a.compGr(), 1));
        TopConsoleBProcessor b; setP (b, "c_ratio", 8); setP (b, "c_thr", -30);
        const float dInf = tone (run (b, in).first, 200, 1.0, 1.4) - tone (in.first, 200, 1.0, 1.4);
        report ("Ratio infinito limita mas", dInf < d - 3.0f, juce::String (dInf, 1) + " dB");
        // de-esser: una "s" fuerte baja todo, una voz grave no
        TopConsoleBProcessor c; setP (c, "c_ratio", 9); setP (c, "c_thr", -30);
        auto ess = sine (7000, 0.3f, 1.0), low = sine (300, 0.3f, 1.0);
        const float de = tone (run (c, ess).first, 7000, 0.5, 0.9) - tone (ess.first, 7000, 0.5, 0.9);
        TopConsoleBProcessor c2; setP (c2, "c_ratio", 9); setP (c2, "c_thr", -30);
        const float dl = tone (run (c2, low).first, 300, 0.5, 0.9) - tone (low.first, 300, 0.5, 0.9);
        report ("Modo ds: baja la sibilancia, no la voz", de < -4.0f && dl > -1.0f, juce::String (de, 1) + " dB en 7k, " + juce::String (dl, 2) + " dB en 300");
    }
    // 6. gate y expansor
    {
        TopConsoleBProcessor a; setP (a, "g_range", 40); setP (a, "g_thr", -30);
        auto quiet = sine (500, 0.005f, 1.0);
        const float dg = tone (run (a, quiet).first, 500, 0.5, 0.9) - tone (quiet.first, 500, 0.5, 0.9);
        report ("Gate cierra debajo del umbral", dg < -35.0f, juce::String (dg, 1) + " dB");
        TopConsoleBProcessor b; setP (b, "g_range", 40); setP (b, "g_thr", -30);
        auto loud = sine (500, 0.3f, 1.0);
        const float dl = tone (run (b, loud).first, 500, 0.5, 0.9) - tone (loud.first, 500, 0.5, 0.9);
        report ("Gate abierto con senal fuerte", std::abs (dl) < 0.2f, juce::String (dl, 2) + " dB");
        TopConsoleBProcessor c; setP (c, "g_range", 40); setP (c, "g_thr", -30); setP (c, "g_exp", 1);
        auto mid = sine (500, 0.01f, 1.0);   // -40 dBFS pico: 10 dB debajo del umbral
        const float de = tone (run (c, mid).first, 500, 0.5, 0.9) - tone (mid.first, 500, 0.5, 0.9);
        report ("Expansor: baja proporcional", de < -5.0f && de > -20.0f, juce::String (de, 1) + " dB");
    }
    // 7. routing al sidechain
    {
        // EQ en el sidechain: el audio no cambia de tono, pero el compresor reacciona a los graves realzados
        TopConsoleBProcessor a; setP (a, "lf_g", 12); setP (a, "lf_f", 200); setP (a, "eq_sc", 1);
        report ("EQ S/C: el EQ sale del audio", std::abs (diff (a, 60)) < 0.05f, juce::String (diff (a, 60), 2) + " dB en 60 Hz");
        TopConsoleBProcessor b; setP (b, "c_ratio", 4); setP (b, "c_thr", -20); setP (b, "hpf", 300); setP (b, "filt_sc", 1);
        const float dlow = diff (b, 60, 0.3f);
        TopConsoleBProcessor c; setP (c, "c_ratio", 4); setP (c, "c_thr", -20);
        const float dlow2 = diff (c, 60, 0.3f);
        report ("Filtros S/C: los graves no disparan", dlow > dlow2 + 6.0f, juce::String (dlow, 1) + " dB vs " + juce::String (dlow2, 1));
        TopConsoleBProcessor d;
        if (auto* bus = d.getBus (true, 1)) bus->enable (true);
        setP (d, "c_ratio", 6); setP (d, "c_thr", -30); setP (d, "ext_sc", 1);
        auto main = sine (1000, 0.05f, 2.0), key = sine (60, 0.6f, 2.0);
        for (size_t i = 0; i < (size_t) SR; ++i) { key.first[i] = 0; key.second[i] = 0; }
        auto o = run (d, main, &key);
        const float drop = tone (o.first, 1000, 0.4, 0.9) - tone (o.first, 1000, 1.5, 1.9);
        report ("Sidechain externo", d.getTotalNumInputChannels() == 4 && drop > 10.0f, juce::String (drop, 1) + " dB de compresion");
        TopConsoleBProcessor e; setP (e, "c_ratio", 6); setP (e, "c_thr", -30); setP (e, "hmf_g", -12); setP (e, "hmf_f", 1000); setP (e, "dyn_pre", 1);
        report ("Dinamica antes del EQ", std::isfinite (diff (e, 1000, 0.3f)), juce::String (diff (e, 1000, 0.3f), 1) + " dB");
    }
    // 8. fader VCA, trim, cut, fase
    {
        TopConsoleBProcessor a; setP (a, "fader", -10);
        report ("Fader -10 dB", std::abs (diff (a, 1000) + 10) < 0.1f, juce::String (diff (a, 1000), 2) + " dB");
        TopConsoleBProcessor b; setP (b, "fader", 12);
        auto in = sine (200, 0.25f, 1.0);
        auto o = run (b, in);
        const float h3 = tone (o.first, 600, 0.4, 0.9) - tone (o.first, 200, 0.4, 0.9);
        report ("Fader alto: saturacion VCA", h3 > -55.0f, "3er armonico " + juce::String (h3, 1) + " dB");
        TopConsoleBProcessor c; setP (c, "out_trim", 6); setP (c, "in_trim", -6);
        report ("Trims de entrada y salida", std::abs (diff (c, 1000)) < 0.1f, juce::String (diff (c, 1000), 2) + " dB");
        TopConsoleBProcessor d; setP (d, "cut", 1);
        report ("Cut silencia", diff (d, 1000) < -80.0f, "");
        TopConsoleBProcessor e; setP (e, "phase", 1);
        auto oe = run (e, in);
        report ("Fase invertida", std::abs (oe.first[30000] + in.first[30000]) < 1e-3f, "");
    }
    // 9. HQ
    {
        TopConsoleBProcessor p; setP (p, "hq", 1); setP (p, "drive", 60);
        auto in = sine (1000, 0.2f, 1.0);
        auto o = run (p, in);
        const float d = tone (o.first, 1000, 0.4, 0.9) - tone (in.first, 1000, 0.4, 0.9);
        report ("HQ: oversampling con latencia", p.getLatencySamples() > 0 && std::abs (d) < 1.5f, "latencia " + juce::String (p.getLatencySamples()) + ", " + juce::String (d, 2) + " dB");
    }
    // 10. presets y estado
    {
        bool ok = true; juce::String bad;
        juce::Random r (5); Stereo n; for (int i = 0; i < 66150; ++i) { const float v = (r.nextFloat() - 0.5f) * 0.3f; n.first.push_back (v); n.second.push_back (v); }
        TopConsoleBProcessor p;
        for (int i = 0; i < p.getNumPrograms(); ++i)
        {
            TopConsoleBProcessor q; q.setCurrentProgram (i);
            auto o = run (q, n);
            float mx = 0; bool fin = true; for (float v : o.first) { mx = std::max (mx, std::abs (v)); fin = fin && std::isfinite (v); }
            if (! fin || mx > 2.0f || mx < 0.01f) { ok = false; bad << q.getProgramName (i) << " "; }
        }
        report ("Presets estables", ok, juce::String (p.getNumPrograms()) + " presets " + bad);
        p.setCurrentProgram (4);
        juce::MemoryBlock mb; p.getStateInformation (mb);
        TopConsoleBProcessor q; q.setStateInformation (mb.getData(), (int) mb.getSize());
        report ("Estado se guarda", std::abs (q.apvts.getRawParameterValue ("lmf_f")->load() - 350) < 1 && q.getCurrentProgram() == 4, "");
    }
    std::printf (fails == 0 ? "TODAS LAS PRUEBAS OK\n" : "%d PRUEBAS FALLARON\n", fails);
    return fails == 0 ? 0 : 1;
}
