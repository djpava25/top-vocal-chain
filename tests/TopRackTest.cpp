// Pruebas de Top Rack: transparencia, cada modulo, sidechain, oversampling, macros, mover, dream strips, presets y estado
#include "TopRackProcessor.h"
#include "TopRackEditor.h"
#include <cstdio>

using namespace trk;
static int fails = 0;
static void report (const char* n, bool ok, const juce::String& info) { if (! ok) ++fails; std::printf ("%-46s %s  %s\n", n, info.toRawUTF8(), ok ? "OK" : "FALLO"); }
static const double SR = 44100.0;
using Stereo = std::pair<std::vector<float>, std::vector<float>>;

static Stereo run (TopRackProcessor& p, const Stereo& in, const Stereo* sc = nullptr)
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
static Stereo noise (float amp, double secs, int seed = 1)
{
    juce::Random r (seed); Stereo s;
    for (int i = 0; i < (int) (SR * secs); ++i) { const float v = amp * (r.nextFloat() * 2 - 1); s.first.push_back (v); s.second.push_back (v); }
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
static float bandRms (const std::vector<float>& x, double hz, double t0, double t1)
{
    tvc::Biquad b; b.setBandPass (SR, hz, 3); std::vector<float> y; for (float v : x) y.push_back (b.process (v));
    return rms (y, t0, t1);
}
static TopRackProcessor* one (int type, std::vector<std::pair<int, float>> knobs)
{
    auto* p = new TopRackProcessor();
    p->setSlotType (0, type);
    for (auto& [k, v] : knobs) p->setKnobReal (0, k, v);
    return p;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc > 2 && juce::String (argv[1]) == "--snapshot")
    {
        TopRackProcessor p;
        const juce::String which = argc > 3 ? juce::String (argv[3]) : juce::String ("2");
        if (which.startsWith ("t"))      // t1 = modulos 1..8, t2 = modulos 8..15
            for (int s = 0; s < trk::numSlots; ++s) p.setSlotType (s, which == "t1" ? s + 1 : juce::jmin ((int) trk::NumTypes - 1, s + 8));
        else p.setCurrentProgram (which.getIntValue());
        p.assignMacro (0, 1, 0);
        p.storeDream (0);
        run (p, noise (0.2f, 1.0));
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        ed->setVisible (true);
        if (auto* te = dynamic_cast<TopRackEditor*> (ed.get())) { te->selectSlot (1); for (int i = 0; i < 8; ++i) te->tickNow(); }
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
        f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("snapshot -> %s\n", f.getFullPathName().toRawUTF8());
        return 0;
    }

    // 1. rack vacio = transparente
    {
        TopRackProcessor p; auto in = noise (0.3f, 1.0);
        auto out = run (p, in);
        float err = 0; for (size_t i = 0; i < out.first.size(); ++i) err = std::max (err, std::abs (out.first[i] - in.first[i]));
        report ("Rack vacio = transparente", err < 1e-6f && p.getLatencySamples() == 0, "error " + juce::String (err, 8));
    }
    // 2. todos los modulos con sus valores por defecto: estables y sin saltos de volumen enormes
    {
        bool ok = true; juce::String info;
        auto in = noise (0.1f, 1.5);
        for (int t = 1; t < NumTypes; ++t)
        {
            std::unique_ptr<TopRackProcessor> p (one (t, {}));
            auto out = run (*p, in);
            bool fin = true; for (float v : out.first) fin = fin && std::isfinite (v);
            const float d = rms (out.first, 0.5, 1.4) - rms (in.first, 0.5, 1.4);
            if (! fin || std::abs (d) > 12.0f) { ok = false; info << defs()[(size_t) t].name << " (" << juce::String (d, 1) << " dB) "; }
        }
        report ("15 modulos estables por defecto", ok, info);
    }
    // 3. EQs
    {
        std::unique_ptr<TopRackProcessor> a (one (EqA, { { 3, 2 }, { 4, 10 }, { 9, 0 } }));
        auto in = sine (1600, 0.05f, 1.0);
        const float d = tone (run (*a, in).first, 1600, 0.4, 0.9) - tone (in.first, 1600, 0.4, 0.9);
        report ("EQ Clase A: +10 dB en 1.6k", std::abs (d - 10) < 0.6f, juce::String (d, 2) + " dB");
        std::unique_ptr<TopRackProcessor> c (one (EqConsole, { { 0, 200 } }));
        auto lo = sine (50, 0.1f, 1.0);
        const float dc = tone (run (*c, lo).first, 50, 0.4, 0.9) - tone (lo.first, 50, 0.4, 0.9);
        report ("EQ Consola: pasa altos 200 Hz", dc < -20.0f, juce::String (dc, 1) + " dB en 50 Hz");
        std::unique_ptr<TopRackProcessor> u (one (EqAmerican, { { 0, 1 }, { 1, 10 } }));
        const float du = tone (run (*u, lo).first, 50, 0.4, 0.9) - tone (lo.first, 50, 0.4, 0.9);
        report ("EQ Americano: +12 graves", du > 9.0f && du < 13.0f, juce::String (du, 1) + " dB en 50 Hz");
        std::unique_ptr<TopRackProcessor> air (one (AirEq, { { 0, 10 }, { 1, 2 } }));
        auto hi = sine (15000, 0.05f, 1.0);
        const float da = tone (run (*air, hi).first, 15000, 0.4, 0.9) - tone (hi.first, 15000, 0.4, 0.9);
        report ("EQ de Aire: +aire en 15k", da > 6.0f, juce::String (da, 1) + " dB");
    }
    // 4. compresores
    {
        auto loud = sine (200, 0.5f, 1.5);
        auto check = [&] (const char* name, int type, std::vector<std::pair<int, float>> k, float minGr)
        {
            std::unique_ptr<TopRackProcessor> p (one (type, k));
            run (*p, loud);
            const float g = p->slotGr (0);
            report (name, g > minGr, "GR " + juce::String (g, 1) + " dB");
        };
        check ("Comp FET comprime", CompFet, { { 0, 30 } }, 6.0f);
        check ("Comp VCA comprime", CompVca, { { 0, -30 }, { 1, 8 } }, 10.0f);
        check ("Comp Optico comprime", CompOpto, { { 0, 80 } }, 4.0f);
        check ("Comp Bus comprime", CompBus, { { 0, -20 } }, 5.0f);
        check ("Estresador: Nuke limita fuerte", Stress, { { 0, 30 }, { 4, 7 } }, 15.0f);
        check ("Monstruo aplasta", Monster, { { 0, 100 } }, 15.0f);
        std::unique_ptr<TopRackProcessor> s (one (Stress, { { 0, 6 }, { 4, 1 }, { 6, 2 } }));
        auto in = sine (300, 0.4f, 1.0);
        auto o = run (*s, in);
        const float h3 = tone (o.first, 900, 0.4, 0.9) - tone (o.first, 300, 0.4, 0.9);
        report ("Estresador Dist 3: 3er armonico", h3 > -40.0f, juce::String (h3, 1) + " dB");
    }
    // 5. de-esser, puerta, bombardero, revivir, trim
    {
        auto mixd = sine (500, 0.1f, 1.5);
        auto hiss = noise (0.0f, 1.5);
        { juce::Random r (2); tvc::Biquad hp; hp.setHighPass (SR, 5000, 0.7); for (size_t i = 0; i < mixd.first.size(); ++i) { const float v = hp.process ((r.nextFloat() * 2 - 1) * 0.4f); mixd.first[i] += v; mixd.second[i] += v; } }
        std::unique_ptr<TopRackProcessor> d (one (DeEss, { { 0, 6000 }, { 1, 12 }, { 2, -40 } }));
        auto o = run (*d, mixd);
        const float s7 = bandRms (o.first, 8000, 0.5, 1.4) - bandRms (mixd.first, 8000, 0.5, 1.4);
        const float m5 = tone (o.first, 500, 0.5, 1.4) - tone (mixd.first, 500, 0.5, 1.4);
        report ("De-Esser: baja las eses, no los medios", s7 < -5.0f && std::abs (m5) < 1.0f, juce::String (s7, 1) + " dB en 8k, " + juce::String (m5, 2) + " dB en 500");
        std::unique_ptr<TopRackProcessor> g (one (Gate, { { 0, -30 }, { 1, 40 } }));
        auto quiet = noise (0.003f, 1.0);
        const float dg = rms (run (*g, quiet).first, 0.4, 0.9) - rms (quiet.first, 0.4, 0.9);
        report ("Puerta: cierra el ruido bajo", dg < -30.0f, juce::String (dg, 1) + " dB");
        Stereo drum; for (int i = 0; i < (int) (SR * 2); ++i) { const double t = std::fmod (i / SR, 0.4); const float v = (float) (std::sin (6.283 * 80 * t) * std::exp (-t * 12) * 0.4); drum.first.push_back (v); drum.second.push_back (v); }
        std::unique_ptr<TopRackProcessor> b (one (Bomber, { { 0, 100 } }));
        auto ob = run (*b, drum);
        auto crest = [] (const std::vector<float>& x) { float pk = 0; for (size_t i = (size_t) SR; i < x.size(); ++i) pk = std::max (pk, std::abs (x[i])); return 20 * std::log10 (pk) - rms (x, 1, 1.9); };
        report ("Bombardero: mas golpe", crest (ob.first) > crest (drum.first) + 1.0f, juce::String (crest (drum.first), 1) + " -> " + juce::String (crest (ob.first), 1) + " dB cresta");
        std::unique_ptr<TopRackProcessor> rv (one (Revive, { { 0, 100 } }));
        auto s1 = sine (5000, 0.3f, 1.0);
        const float hh = tone (run (*rv, s1).first, 15000, 0.4, 0.9);
        report ("Revivir: brillo con armonicos", hh > -70.0f, "3er arm. " + juce::String (hh, 1) + " dB");
        std::unique_ptr<TopRackProcessor> tr (one (Trim, { { 0, -6 }, { 1, 1 } }));
        auto ot = run (*tr, s1);
        const float corr = ot.first[30000] / s1.first[30000];
        report ("Trim: -6 dB y fase invertida", std::abs (corr + 0.501f) < 0.01f, "factor " + juce::String (corr, 3));
    }
    // 6. sidechain externo
    {
        TopRackProcessor p;
        if (auto* bus = p.getBus (true, 1)) bus->enable (true);
        p.setSlotType (0, CompVca); p.setKnobReal (0, 0, -30); p.setKnobReal (0, 1, 10);
        if (auto* prm = p.apvts.getParameter (TopRackProcessor::sid (0, "sc"))) prm->setValueNotifyingHost (1.0f);
        auto main = sine (1000, 0.05f, 2.0);
        auto key = sine (60, 0.6f, 2.0);
        for (size_t i = 0; i < key.first.size(); ++i) if (i < (size_t) SR) { key.first[i] = 0; key.second[i] = 0; }
        auto o = run (p, main, &key);
        const float quietPart = tone (o.first, 1000, 0.4, 0.9), keyedPart = tone (o.first, 1000, 1.4, 1.9);
        report ("Sidechain: otra pista hace comprimir", p.getTotalNumInputChannels() == 4 && quietPart - keyedPart > 10.0f, juce::String (quietPart - keyedPart, 1) + " dB de compresion");
    }
    // 7. oversampling
    {
        TopRackProcessor p; p.setSlotType (0, EqConsole);
        if (auto* prm = p.apvts.getParameter ("oversample")) prm->setValueNotifyingHost (1.0f);
        auto in = sine (1000, 0.2f, 1.0);
        auto o = run (p, in);
        const float d = tone (o.first, 1000, 0.4, 0.9) - tone (in.first, 1000, 0.4, 0.9);
        report ("Oversampling 2x: latencia y nivel", p.getLatencySamples() > 0 && std::abs (d) < 0.5f, "latencia " + juce::String (p.getLatencySamples()) + ", " + juce::String (d, 2) + " dB");
    }
    // 8. macros, mover, dream strips, presets y estado
    {
        TopRackProcessor p;
        p.setSlotType (0, CompVca); p.setSlotType (1, EqA);
        p.assignMacro (0, 0, 0);
        const float before = p.knobNorm (0, 0);
        if (auto* m = p.apvts.getParameter ("macro1")) m->setValueNotifyingHost (1.0f);
        p.applyMacros();
        const float after = p.knobNorm (0, 0);
        report ("Macro mueve la perilla asignada", std::abs (after - before) > 0.3f, juce::String (before, 2) + " -> " + juce::String (after, 2));
        p.moveSlot (0, 1);
        report ("Mover modulo de ranura", p.slotType (0) == EqA && p.slotType (1) == CompVca && p.macroOf (1, 0) == 0, defs()[(size_t) p.slotType (0)].name + " / " + defs()[(size_t) p.slotType (1)].name);
        p.storeDream (2);
        p.setCurrentProgram (0);
        const bool emptied = p.slotType (0) == Empty;
        p.recallDream (2);
        report ("Dream Strip guarda y recupera", emptied && p.slotType (0) == EqA && p.slotType (1) == CompVca, "");
        p.assignMacro (1, 1, 2);
        juce::MemoryBlock mb; p.getStateInformation (mb);
        TopRackProcessor q; q.setStateInformation (mb.getData(), (int) mb.getSize());
        report ("Estado: rack, macros y dream strips", q.slotType (1) == CompVca && q.macroOf (1, 2) == 1 && q.dream[2].isValid(), "");
        bool ok = true; juce::String bad;
        auto voice = noise (0.08f, 1.5, 7);
        for (int i = 0; i < p.getNumPrograms(); ++i)
        {
            TopRackProcessor r; r.setCurrentProgram (i);
            auto o = run (r, voice);
            float mx = 0; bool fin = true; for (float v : o.first) { mx = std::max (mx, std::abs (v)); fin = fin && std::isfinite (v); }
            if (! fin || mx > 4.0f || (i > 0 && mx < 0.005f)) { ok = false; bad << r.getProgramName (i) << " "; }
        }
        report ("Presets de cadena estables", ok, juce::String (p.getNumPrograms()) + " presets " + bad);
    }
    std::printf (fails == 0 ? "TODAS LAS PRUEBAS OK\n" : "%d PRUEBAS FALLARON\n", fails);
    return fails == 0 ? 0 : 1;
}
