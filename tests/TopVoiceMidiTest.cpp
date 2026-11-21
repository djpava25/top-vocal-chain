// Pruebas de Top Voice MIDI: notas, escala, acordes, velocidad, bend, CC, beatbox, tonalidad y captura
#include "TopVoiceMidiProcessor.h"
#include "TopVoiceMidiEditor.h"
#include <cstdio>

static int fails = 0;
static void report (const char* n, bool ok, const juce::String& info) { if (! ok) ++fails; std::printf ("%-44s %s  %s\n", n, info.toRawUTF8(), ok ? "OK" : "FALLO"); }
static const double SR = 44100.0;

struct Seg { double hz; double secs; float amp; };
// voz sintetica con armonicos, ataque suave y vibrato leve
static juce::AudioBuffer<float> voice (const std::vector<Seg>& segs, double glideTo = 0)
{
    double total = 0; for (auto& s : segs) total += s.secs;
    juce::AudioBuffer<float> b (1, (int) (SR * total));
    double ph = 0; int i = 0;
    for (size_t k = 0; k < segs.size(); ++k)
    {
        const int len = (int) (SR * segs[k].secs);
        for (int j = 0; j < len; ++j, ++i)
        {
            const double t = j / SR;
            double hz = segs[k].hz;
            if (glideTo > 0 && k == segs.size() - 1) hz = segs[k].hz + (glideTo - segs[k].hz) * juce::jlimit (0.0, 1.0, (t - 0.15) / 0.2);
            double v = 0;
            if (hz > 0)
            {
                ph += hz * (1.0 + 0.002 * std::sin (6.283 * 5.0 * t)) / SR;
                for (int h = 1; h <= 10; ++h) v += std::sin (6.283185307 * h * ph) / h * (h == 2 ? 1.3 : 1.0);
            }
            const double e = std::min (1.0, std::min (t * 60, (segs[k].secs - t) * 60));
            b.setSample (0, i, (float) (segs[k].amp * v * e));
        }
    }
    return b;
}

static juce::MidiBuffer run (TopVoiceMidiProcessor& p, const juce::AudioBuffer<float>& in, std::vector<std::pair<double, juce::MidiMessage>>* all = nullptr)
{
    juce::MidiBuffer collected;
    const int bs = 256;
    juce::AudioBuffer<float> buf (2, bs);
    for (int pos = 0; pos < in.getNumSamples(); pos += bs)
    {
        const int n = std::min (bs, in.getNumSamples() - pos);
        buf.setSize (2, n, false, false, true);
        for (int ch = 0; ch < 2; ++ch) buf.copyFrom (ch, 0, in, 0, pos, n);
        juce::MidiBuffer m;
        p.processBlock (buf, m);
        for (const auto meta : m)
        {
            collected.addEvent (meta.getMessage(), pos + meta.samplePosition);
            if (all) all->push_back ({ (pos + meta.samplePosition) / SR, meta.getMessage() });
        }
    }
    return collected;
}
static void setP (TopVoiceMidiProcessor& p, const juce::String& id, float v) { auto* q = p.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); }
static std::vector<int> noteOns (const juce::MidiBuffer& m, int ch = -1)
{
    std::vector<int> v;
    for (const auto meta : m) { const auto msg = meta.getMessage(); if (msg.isNoteOn() && (ch < 0 || msg.getChannel() == ch)) v.push_back (msg.getNoteNumber()); }
    return v;
}
static juce::String join (const std::vector<int>& v) { juce::String s; for (int x : v) s << x << " "; return s.trim(); }
static double hz (int midi) { return 440.0 * std::pow (2.0, (midi - 69) / 12.0); }

// golpes de beatbox sinteticos
static void addHit (juce::AudioBuffer<float>& b, int at, int type, float amp, juce::Random& r)
{
    double ph = 0;
    for (int i = 0; i < (int) (SR * 0.12) && at + i < b.getNumSamples(); ++i)
    {
        const double t = i / SR;
        double s;
        if (type == 0) { ph += (55 + 120 * std::exp (-t * 40)) / SR; s = std::sin (6.283 * ph) * std::exp (-t * 25); }      // bombo "bum"
        else if (type == 1) { ph += 210 / SR; s = 0.4 * std::sin (6.283 * ph) * std::exp (-t * 30) + 0.8 * (r.nextFloat() * 2 - 1) * std::exp (-t * 22); } // caja "ka"
        else { s = (r.nextFloat() * 2 - 1); s = s - 0.95 * (i > 0 ? 0 : 0); s *= std::exp (-t * 60); }                      // hi-hat "ts"
        b.addSample (0, at + i, (float) (amp * s));
    }
}
static juce::AudioBuffer<float> hiHatFilter (juce::AudioBuffer<float> b)
{
    tvc::Biquad h; h.setHighPass (SR, 5000, 0.7);
    for (int i = 0; i < b.getNumSamples(); ++i) b.setSample (0, i, h.process (b.getSample (0, i)));
    return b;
}
static juce::AudioBuffer<float> hits (const std::vector<int>& types, juce::Random& r, float amp = 0.6f)
{
    juce::AudioBuffer<float> out (1, (int) (SR * (0.3 * types.size() + 0.3)));
    out.clear();
    for (size_t k = 0; k < types.size(); ++k)
    {
        juce::AudioBuffer<float> one (1, (int) (SR * 0.2)); one.clear();
        addHit (one, 0, types[k], amp, r);
        if (types[k] == 2) one = hiHatFilter (one);
        out.addFrom (0, (int) (SR * (0.1 + 0.3 * k)), one, 0, 0, one.getNumSamples());
    }
    return out;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc > 2 && juce::String (argv[1]) == "--snapshot")
    {
        TopVoiceMidiProcessor p; p.prepareToPlay (SR, 256);
        setP (p, "auto_key", 1); setP (p, "bend_mode", 1); setP (p, "cc1_on", 1);
        run (p, voice ({ { hz (57), 0.4, 0.2f }, { hz (60), 0.4, 0.2f }, { hz (64), 0.5, 0.25f }, { hz (62), 0.6, 0.2f } }));
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        auto* te = dynamic_cast<TopVoiceMidiEditor*> (ed.get());
        te->showTab (argc > 3 ? juce::String (argv[3]).getIntValue() : 0);
        ed->setVisible (true);
        for (int i = 0; i < 5; ++i) te->tickNow();
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
        f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("snapshot -> %s\n", f.getFullPathName().toRawUTF8());
        return 0;
    }

    // 1. Melodia: A3 -> C4 -> E4 -> silencio
    {
        TopVoiceMidiProcessor p; p.prepareToPlay (SR, 256);
        std::vector<std::pair<double, juce::MidiMessage>> ev;
        const auto m = run (p, voice ({ { 0, 0.2, 0 }, { hz (57), 0.5, 0.2f }, { hz (60), 0.5, 0.2f }, { hz (64), 0.5, 0.2f }, { 0, 0.3, 0 } }), &ev);
        const auto ons = noteOns (m);
        double firstOn = -1; int offsAfter = 0, ons2 = 0;
        for (auto& [t, msg] : ev) { if (msg.isNoteOn() && firstOn < 0) firstOn = t; if (msg.isNoteOn()) ++ons2; if (msg.isNoteOff() && t > 1.7) ++offsAfter; }
        report ("Melodia: notas correctas", ons == std::vector<int> { 57, 60, 64 }, "notas " + join (ons));
        report ("Latencia de la primera nota", firstOn > 0.2 && firstOn - 0.2 < 0.045, juce::String ((firstOn - 0.2) * 1000.0, 1) + " ms");
        report ("Silencio suelta la nota", offsAfter >= 1, juce::String (offsAfter) + " note-off al final");
    }
    // 2. Escala: C mayor, se canta C#4 desafinado -> sale C o D
    {
        TopVoiceMidiProcessor p; p.prepareToPlay (SR, 256);
        setP (p, "scale", 1); setP (p, "key", 0);
        const auto ons = noteOns (run (p, voice ({ { 0, 0.1, 0 }, { hz (61) * 1.004, 0.5, 0.2f } })));
        report ("Escala mayor: C# no sale", ! ons.empty() && (ons[0] == 60 || ons[0] == 62), "nota " + join (ons));
    }
    // 3. Octava
    {
        TopVoiceMidiProcessor p; p.prepareToPlay (SR, 256);
        setP (p, "octave", 1);
        const auto ons = noteOns (run (p, voice ({ { 0, 0.1, 0 }, { hz (57), 0.4, 0.2f } })));
        report ("Octava +1", ons == std::vector<int> { 69 }, "nota " + join (ons));
    }
    // 4. Acordes: mayor y diatonico en A menor
    {
        TopVoiceMidiProcessor p; p.prepareToPlay (SR, 256);
        setP (p, "chord_on", 1); setP (p, "chord_type", 0);
        auto ons = noteOns (run (p, voice ({ { 0, 0.1, 0 }, { hz (57), 0.4, 0.2f } })));
        std::sort (ons.begin(), ons.end());
        report ("Acorde mayor sobre A3", ons == std::vector<int> { 57, 61, 64 }, join (ons));
        TopVoiceMidiProcessor q; q.prepareToPlay (SR, 256);
        setP (q, "chord_on", 1); setP (q, "chord_type", 2); setP (q, "scale", 2); setP (q, "key", 9);
        auto ons2 = noteOns (run (q, voice ({ { 0, 0.1, 0 }, { hz (62), 0.4, 0.2f } })));
        std::sort (ons2.begin(), ons2.end());
        report ("Diatonico en A menor sobre D (Dm)", ons2 == std::vector<int> { 62, 65, 69 }, join (ons2));
    }
    // 5. Velocidad sigue el volumen
    {
        auto velOf = [] (float amp)
        {
            TopVoiceMidiProcessor p; p.prepareToPlay (SR, 256);
            for (const auto meta : run (p, voice ({ { 0, 0.1, 0 }, { hz (60), 0.4, amp } }))) if (meta.getMessage().isNoteOn()) return (int) meta.getMessage().getVelocity();
            return -1;
        };
        const int soft = velOf (0.03f), loud = velOf (0.4f);
        report ("Velocidad: fuerte > suave", soft > 0 && loud > soft + 15, "suave " + juce::String (soft) + "  fuerte " + juce::String (loud));
    }
    // 6. Pitch bend exacto sigue un desliz, inteligente ignora desafines pequenos
    {
        auto maxBend = [] (int mode, double glide)
        {
            TopVoiceMidiProcessor p; p.prepareToPlay (SR, 256);
            setP (p, "bend_mode", (float) mode); setP (p, "stability", 100);
            int mx = 8192;
            for (const auto meta : run (p, voice ({ { 0, 0.1, 0 }, { hz (60), 0.6, 0.2f } }, glide)))
                if (meta.getMessage().isPitchWheel()) mx = std::max (mx, meta.getMessage().getPitchWheelValue());
            return mx;
        };
        const int exact = maxBend (2, hz (60) * std::pow (2.0, 0.3 / 12.0));
        const int smartSmall = maxBend (1, hz (60) * std::pow (2.0, 0.25 / 12.0));
        const int smartBig = maxBend (1, hz (60) * std::pow (2.0, 0.9 / 12.0));
        report ("Bend exacto: +30 cents", exact > 8192 + 900 && exact < 8192 + 1700, juce::String (exact - 8192));
        report ("Bend inteligente: ignora +25 cents", smartSmall < 8192 + 100, juce::String (smartSmall - 8192));
        report ("Bend inteligente: sigue +90 cents", smartBig > 8192 + 1500, juce::String (smartBig - 8192));
    }
    // 7. CC de envolvente
    {
        TopVoiceMidiProcessor p; p.prepareToPlay (SR, 256);
        setP (p, "cc1_on", 1); setP (p, "cc1_num", 11);
        int n = 0, mx = 0;
        for (const auto meta : run (p, voice ({ { 0, 0.1, 0 }, { hz (60), 0.4, 0.3f }, { 0, 0.3, 0 } })))
            if (meta.getMessage().isController() && meta.getMessage().getControllerNumber() == 11) { ++n; mx = std::max (mx, meta.getMessage().getControllerValue()); }
        report ("CC11 sigue la envolvente", n > 10 && mx > 70, juce::String (n) + " mensajes, max " + juce::String (mx));
    }
    // 8. Beatbox sin entrenar y entrenado
    {
        juce::Random r (5);
        TopVoiceMidiProcessor p; p.prepareToPlay (SR, 256);
        setP (p, "mode", 1);
        const auto ons = noteOns (run (p, hits ({ 0, 2, 0, 2 }, r)), 10);
        report ("Beatbox sin entrenar (bombo/hi-hat)", ons == std::vector<int> { 36, 42, 36, 42 }, join (ons));

        TopVoiceMidiProcessor q; q.prepareToPlay (SR, 256);
        setP (q, "mode", 1);
        for (int pad : { 0, 1, 2 })
        {
            q.trainingPad.store (pad);
            run (q, hits ({ pad, pad, pad, pad, pad, pad }, r));
        }
        q.trainingPad.store (-1);
        const int ex = q.padExamples (0) + q.padExamples (1) + q.padExamples (2);
        const auto ons2 = noteOns (run (q, hits ({ 1, 0, 2, 1, 0, 2, 1 }, r)), 10);
        report ("Beatbox entrenado (caja/bombo/hihat)", ons2 == std::vector<int> { 38, 36, 42, 38, 36, 42, 38 }, join (ons2) + "  (" + juce::String (ex) + " ejemplos)");
        // estado guardado conserva el entrenamiento
        juce::MemoryBlock mb; q.getStateInformation (mb);
        TopVoiceMidiProcessor z; z.setStateInformation (mb.getData(), (int) mb.getSize());
        report ("Entrenamiento se guarda", z.padExamples (1) == q.padExamples (1) && z.padExamples (1) > 0, juce::String (z.padExamples (1)) + " ejemplos de caja");
    }
    // 9. Tonalidad automatica (A menor armonica)
    {
        TopVoiceMidiProcessor p; p.prepareToPlay (SR, 256);
        setP (p, "auto_key", 1);
        std::vector<Seg> s;
        for (int rep = 0; rep < 2; ++rep)
            for (int n : { 57, 60, 64, 62, 59, 68, 69, 64, 65, 57 }) s.push_back ({ hz (n), 0.35, 0.2f });
        run (p, voice (s));
        report ("Tonalidad automatica = A menor", p.detKey.load() == 9 && p.detMinor.load(), juce::String (p.detKey.load()) + (p.detMinor.load() ? " menor" : " mayor"));
    }
    // 10. Captura y exportacion .mid
    {
        TopVoiceMidiProcessor p; p.prepareToPlay (SR, 256);
        p.startCapture();
        run (p, voice ({ { 0, 0.1, 0 }, { hz (57), 0.3, 0.2f }, { hz (60), 0.3, 0.2f }, { 0, 0.2, 0 } }));
        p.stopCapture();
        const auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("tvm_test.mid");
        const bool ok = p.exportCapture (f, true);
        juce::MidiFile mf; juce::FileInputStream is (f); const bool read = mf.readFrom (is);
        int notes = 0;
        if (read) for (int t = 0; t < mf.getNumTracks(); ++t) for (auto* e : *mf.getTrack (t)) if (e->message.isNoteOn()) ++notes;
        report ("Captura exporta .mid", ok && read && notes == 2, juce::String (p.capturedNotes()) + " capturadas, " + juce::String (notes) + " en el archivo");
    }
    // 11. Escala aprendida con la voz
    {
        TopVoiceMidiProcessor p; p.prepareToPlay (SR, 256);
        p.learnedMask.store (0); p.learning.store (true);
        run (p, voice ({ { 0, 0.1, 0 }, { hz (57), 0.3, 0.2f }, { hz (62), 0.3, 0.2f }, { hz (64), 0.3, 0.2f } }));
        p.learning.store (false);
        const int m = p.learnedMask.load();
        report ("Aprender escala con la voz", m == ((1 << 9) | (1 << 2) | (1 << 4)), "mascara " + juce::String::toHexString (m));
    }
    std::printf (fails == 0 ? "TODAS LAS PRUEBAS OK\n" : "%d PRUEBAS FALLARON\n", fails);
    return fails == 0 ? 0 : 1;
}
