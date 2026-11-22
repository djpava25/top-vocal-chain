// Pruebas de Top Meter con las senales de referencia EBU Tech 3341 / 3342
#include "TopMeterProcessor.h"
#include "TopMeterEditor.h"
#include <cstdio>

static int fails = 0;
static void report (const char* n, bool ok, const juce::String& info) { if (! ok) ++fails; std::printf ("%-48s %s  %s\n", n, info.toRawUTF8(), ok ? "OK" : "FALLO"); }
static const double SR = 48000.0;

struct Part { double secs, dbfs, hz; bool stereo = true; double phase = 0; bool invertR = false; };
static void feed (TopMeterProcessor& p, const std::vector<Part>& parts, juce::AudioBuffer<float>* keep = nullptr)
{
    juce::AudioBuffer<float> b (2, 512);
    double ph = 0;
    for (const auto& pt : parts)
    {
        const double a = std::pow (10.0, pt.dbfs / 20.0);
        int left = (int) (pt.secs * SR);
        ph = pt.phase;
        while (left > 0)
        {
            const int n = std::min (512, left);
            juce::AudioBuffer<float> sub (b.getArrayOfWritePointers(), 2, 0, n);
            for (int i = 0; i < n; ++i)
            {
                const float v = (float) (a * std::sin (ph)); ph += 2 * juce::MathConstants<double>::pi * pt.hz / SR;
                sub.setSample (0, i, v); sub.setSample (1, i, pt.stereo ? (pt.invertR ? -v : v) : 0.0f);
            }
            juce::AudioBuffer<float> copy; if (keep) copy.makeCopyOf (sub);
            juce::MidiBuffer m; p.processBlock (sub, m);
            if (keep) for (int c = 0; c < 2; ++c) for (int i = 0; i < n; ++i) if (sub.getSample (c, i) != copy.getSample (c, i)) keep->setSample (0, 0, 1.0f);
            left -= n;
        }
    }
}
static std::unique_ptr<TopMeterProcessor> make (bool freeSync = true)
{
    auto p = std::make_unique<TopMeterProcessor>();
    p->setPlayConfigDetails (2, 2, SR, 512);
    if (freeSync) p->apvts.getParameter ("sync")->setValueNotifyingHost (1.0f);
    p->prepareToPlay (SR, 512);
    return p;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc > 2 && juce::String (argv[1]) == "--snapshot")
    {
        auto p = make();
        // musica de prueba: acordes con ritmo y algo de estereo
        juce::AudioBuffer<float> b (2, 512); juce::Random rng (5); double ph[4] {};
        const double f[] { 110, 220, 329.6, 1760 };
        for (int blk = 0; blk < (int) (SR * 40 / 512); ++blk)
        {
            for (int i = 0; i < 512; ++i)
            {
                const double t = (blk * 512 + i) / SR;
                const double env = 0.55 + 0.45 * std::sin (t * 2.1) * std::sin (t * 0.37);
                double l = 0, r = 0;
                for (int k = 0; k < 4; ++k) { ph[k] += 2 * juce::MathConstants<double>::pi * f[k] / SR; const double v = std::sin (ph[k]) / (k + 1.5); l += v * (k % 2 ? 0.8 : 1.0); r += v * (k % 2 ? 1.0 : 0.8); }
                const double kick = std::fmod (t, 0.5) < 0.08 ? std::sin (2 * juce::MathConstants<double>::pi * 55 * t) * (1 - std::fmod (t, 0.5) / 0.08) : 0.0;
                const double noise = (rng.nextDouble() - 0.5) * 0.08;
                b.setSample (0, i, (float) ((l * 0.35 + kick * 0.6 + noise) * env));
                b.setSample (1, i, (float) ((r * 0.35 + kick * 0.6 + noise * 0.7) * env));
            }
            juce::MidiBuffer m; p->processBlock (b, m);
        }
        p->channelName = "MASTER";
        std::unique_ptr<juce::AudioProcessorEditor> ed (p->createEditor());
        ed->setSize (TopMeterEditor::W, TopMeterEditor::H);
        ed->setVisible (true);
        if (auto* te = dynamic_cast<TopMeterEditor*> (ed.get())) for (int i = 0; i < 40; ++i) te->tickNow();
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File fl (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
        fl.deleteFile(); juce::FileOutputStream os (fl); juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("snapshot -> %s\n", fl.getFullPathName().toRawUTF8());
        return 0;
    }

    // EBU 3341 caso 1: seno 1 kHz estereo a -23 dBFS durante 20 s -> M, S, I = -23.0 (+-0.1)
    {
        auto p = make(); feed (*p, { { 20, -23, 1000 } });
        auto& m = p->meter;
        report ("EBU 3341 #1: -23 dBFS -> -23 LUFS", std::abs (m.integrated - (-23)) < 0.1f && std::abs (m.momentary - (-23)) < 0.1f && std::abs (m.shortTerm - (-23)) < 0.1f,
                "I " + juce::String (m.integrated.load(), 2) + "  M " + juce::String (m.momentary.load(), 2) + "  S " + juce::String (m.shortTerm.load(), 2));
    }
    // caso 2: -33 dBFS -> -33 LUFS
    {
        auto p = make(); feed (*p, { { 20, -33, 1000 } });
        report ("EBU 3341 #2: -33 dBFS -> -33 LUFS", std::abs (p->meter.integrated - (-33)) < 0.1f, "I " + juce::String (p->meter.integrated.load(), 2));
    }
    // caso 3: -36 / -23 / -36 (10 s cada uno) -> I = -23 (la compuerta relativa quita las partes bajas)
    {
        auto p = make(); feed (*p, { { 10, -36, 1000 }, { 60, -23, 1000 }, { 10, -36, 1000 } });
        report ("EBU 3341 #3: compuerta relativa", std::abs (p->meter.integrated - (-23)) < 0.1f, "I " + juce::String (p->meter.integrated.load(), 2));
    }
    // caso 4: -72 / -36 / -23 / -36 / -72 -> I = -23 (compuerta absoluta -70)
    {
        auto p = make(); feed (*p, { { 10, -72, 1000 }, { 10, -36, 1000 }, { 60, -23, 1000 }, { 10, -36, 1000 }, { 10, -72, 1000 } });
        report ("EBU 3341 #4: compuerta absoluta", std::abs (p->meter.integrated - (-23)) < 0.1f, "I " + juce::String (p->meter.integrated.load(), 2));
    }
    // caso 5: -26 (20 s) / -20 (20.1 s) / -26 (20 s) -> I = -23
    {
        auto p = make(); feed (*p, { { 20, -26, 1000 }, { 20.1, -20, 1000 }, { 20, -26, 1000 } });
        report ("EBU 3341 #5: niveles mezclados", std::abs (p->meter.integrated - (-23)) < 0.1f, "I " + juce::String (p->meter.integrated.load(), 2));
    }
    // EBU 3342 caso 1: -20 / -30 dBFS (20 s cada uno) -> LRA = 10 LU (+-1)
    {
        auto p = make(); feed (*p, { { 20, -20, 1000 }, { 20, -30, 1000 } });
        report ("EBU 3342 #1: LRA 10 LU", std::abs (p->meter.lra - 10) < 1.0f, "LRA " + juce::String (p->meter.lra.load(), 2));
    }
    // EBU 3342 caso 2: -20 / -15 -> LRA 5
    {
        auto p = make(); feed (*p, { { 20, -20, 1000 }, { 20, -15, 1000 } });
        report ("EBU 3342 #2: LRA 5 LU", std::abs (p->meter.lra - 5) < 1.0f, "LRA " + juce::String (p->meter.lra.load(), 2));
    }
    // True Peak: seno a fs/4 con fase 45 grados -> las muestras marcan -3 dB pero el pico real es 0 dB
    {
        auto p = make(); feed (*p, { { 2, 0, SR / 4, true, juce::MathConstants<double>::pi / 4 } });
        const float tp = p->meter.tpMax.load(), sp = p->meter.peakMax[0].load();
        report ("True Peak entre muestras", std::abs (tp) < 0.6f && sp < -2.5f, "muestras " + juce::String (sp, 2) + " dB / true peak " + juce::String (tp, 2) + " dBTP");
    }
    // VU: seno con RMS -18 dBFS (pico -15 dBFS) = 0 VU
    {
        auto p = make(); feed (*p, { { 3, -15, 1000 } });
        report ("VU: -18 dBFS RMS = 0 VU", std::abs (p->meter.vu[0].load()) < 0.3f, juce::String (p->meter.vu[0].load(), 2) + " VU");
        report ("RMS de un seno", std::abs (p->meter.rmsDb[0].load() - (-18.01f)) < 0.2f, juce::String (p->meter.rmsDb[0].load(), 2) + " dBFS");
    }
    // correlacion y balance
    {
        auto p = make(); feed (*p, { { 2, -10, 440 } });
        const float c1 = p->meter.correlation.load();
        auto q = make(); feed (*q, { { 2, -10, 440, true, 0, true } });
        const float c2 = q->meter.correlation.load();
        auto r = make(); feed (*r, { { 2, -10, 440, false } });
        report ("Correlacion: mono +1 / fase invertida -1", c1 > 0.99f && c2 < -0.99f, juce::String (c1, 3) + " / " + juce::String (c2, 3));
        report ("Balance: solo izquierda", r->meter.balance.load() < -0.99f, juce::String (r->meter.balance.load(), 3));
    }
    // el audio pasa sin cambios
    {
        auto p = make(); juce::AudioBuffer<float> flag (1, 1); flag.clear();
        feed (*p, { { 1, -6, 330 } }, &flag);
        report ("El audio pasa intacto", flag.getSample (0, 0) == 0.0f, "");
    }
    // reset y pausa
    {
        auto p = make(); feed (*p, { { 5, -23, 1000 } });
        p->resetMeters(); feed (*p, { { 0.05, -23, 1000 } });
        const bool rs = p->meter.integrated.load() < -100;
        p->apvts.getParameter ("run")->setValueNotifyingHost (0.0f);
        const int hc = p->meter.histCount.load();
        feed (*p, { { 2, -23, 1000 } });
        report ("Reset y pausa", rs && p->meter.histCount.load() == hc, "");
    }
    // presets
    {
        auto p = make(); p->setCurrentProgram (3);
        report ("Preset broadcast -> objetivo -23 LUFS", std::abs (p->targetLufs() - (-23)) < 0.01f, juce::String (p->targetLufs(), 1));
        juce::MemoryBlock mb; p->channelName = "VOZ"; p->getStateInformation (mb);
        TopMeterProcessor q; q.setStateInformation (mb.getData(), (int) mb.getSize());
        report ("Estado guardado", q.channelName == "VOZ" && std::abs (q.targetLufs() - (-23)) < 0.01f, q.channelName);
    }
    std::printf (fails == 0 ? "TODAS LAS PRUEBAS OK\n" : "%d PRUEBAS FALLARON\n", fails);
    return fails == 0 ? 0 : 1;
}
