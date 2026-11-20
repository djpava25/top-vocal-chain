// Prueba de Top Vocal Chain V2: el editor abre, las macros cambian el sonido y la captura
#include "PluginProcessor.h"
#include "V2/EditorV2.h"
#include <cstdio>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    int fails = 0;
    auto report = [&] (const char* n, bool ok, const juce::String& info) { if (! ok) ++fails; std::printf ("%-44s %s  %s\n", n, info.toRawUTF8(), ok ? "OK" : "FALLO"); };
    TopVocalChainProcessor p;
    p.setPlayConfigDetails (2, 2, 48000, 512);
    p.prepareToPlay (48000, 512);
    auto set = [&] (const char* id, float v) { auto* x = p.apvts.getParameter (id); x->setValueNotifyingHost (x->convertTo0to1 (v)); };
    set ("tune_on", 1); set ("tune_key", 9); set ("tune_scale", 2); set ("comp_on", 1); set ("deess_on", 1); set ("air_high", 30);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    ed->setVisible (true);
    report ("Editor V2 abre", dynamic_cast<tvc2::EditorV2*> (ed.get()) != nullptr, juce::String (ed->getWidth()) + "x" + juce::String (ed->getHeight()));
    juce::AudioBuffer<float> b (2, 512); juce::MidiBuffer m; double ph = 0;
    juce::Random rnd (5);
    for (int k = 0; k < 300; ++k)
    {
        for (int i = 0; i < 512; ++i)
        {
            const double tt = (k * 512 + i) / 48000.0;
            const double midi = 57.0 + 3.0 * std::floor (std::fmod (tt, 2.4) / 0.8) + 0.3 * std::sin (6.283 * 5.5 * tt) + 0.3;
            ph += 440.0 * std::pow (2.0, (midi - 69.0) / 12.0) / 48000.0;
            float v = 0; for (int h = 1; h < 10; ++h) v += (float) std::sin (6.283185307 * h * ph) / (h * 1.3f);
            v = v * 0.25f + (std::fmod (tt, 1.3) < 0.08 ? (rnd.nextFloat() - 0.5f) * 0.3f : 0.0f);   // "S" de vez en cuando
            b.setSample (0, i, v); b.setSample (1, i, v);
        }
        p.processBlock (b, m);
        if (k % 3 == 0) if (auto* e = dynamic_cast<tvc2::EditorV2*> (ed.get())) e->tickNow();
    }
    report ("De-esser actua en las S", p.meters.deessDb.load() <= 0.0f, juce::String (p.meters.deessDb.load(), 1) + " dB");
    // De-esser: con "S" fuertes (ruido agudo) baja los agudos; con voz normal casi no toca
    {
        auto hfEnergy = [] (bool on, bool sibilant)
        {
            TopVocalChainProcessor q; q.setPlayConfigDetails (2, 2, 48000, 512);
            auto s = [&] (const char* id, float v) { auto* x = q.apvts.getParameter (id); x->setValueNotifyingHost (x->convertTo0to1 (v)); };
            for (auto* id : { "tune_on", "pre_on", "eq_on", "comp_on", "rev_on", "dly_on", "wide_on" }) s (id, 0);
            s ("deess_on", on ? 1.0f : 0.0f); s ("deess_amt", 100);
            q.prepareToPlay (48000, 512);
            juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer mm; juce::Random r (9); double ph2 = 0, e = 0;
            tvc::Biquad hp; hp.setHighPass (48000, 5000, 0.707);
            for (int k = 0; k < 200; ++k)
            {
                for (int i = 0; i < 512; ++i)
                {
                    ph2 += 220.0 / 48000.0;
                    float v = 0.2f * (float) std::sin (6.283185307 * ph2);
                    float n = (r.nextFloat() - 0.5f);
                    v += sibilant ? 0.3f * n : 0.01f * n;
                    buf.setSample (0, i, v); buf.setSample (1, i, v);
                }
                q.processBlock (buf, mm);
                if (k > 60) for (int i = 0; i < 512; ++i) { const float h = hp.process (buf.getSample (0, i)); e += h * h; } else for (int i = 0; i < 512; ++i) hp.process (buf.getSample (0, i));
            }
            return 10.0 * std::log10 (e + 1e-12);
        };
        const double sOff = hfEnergy (false, true), sOn = hfEnergy (true, true), vOff = hfEnergy (false, false), vOn = hfEnergy (true, false);
        report ("De-esser baja las S", sOn < sOff - 4, juce::String::formatted ("agudos con S: %.1f -> %.1f dB", sOff, sOn));
        report ("De-esser respeta la voz normal", std::abs (vOn - vOff) < 1.5, juce::String::formatted ("voz normal: %.1f -> %.1f dB", vOff, vOn));
    }
    if (argc > 2 && juce::String (argv[1]) == "--snapshot")
    {
        if (argc > 3) if (auto* e = dynamic_cast<tvc2::EditorV2*> (ed.get())) { e->openAssistForTest(); e->tickNow(); }
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
        f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("snapshot -> %s\n", f.getFullPathName().toRawUTF8());
    }
    std::printf (fails == 0 ? "TODAS LAS PRUEBAS OK\n" : "%d FALLOS\n", fails);
    return fails;
}
