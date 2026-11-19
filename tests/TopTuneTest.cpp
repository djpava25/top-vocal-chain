// Pruebas offline de Top Tune: correccion, estilos, pitch shift, formant, MIDI y detector de tonalidad.
#include "TopTuneProcessor.h"
#include "TopTuneEditor.h"
#include <cstdio>

static float estimatePitch (const float* x, int n, double sr)
{
    const int minLag = (int) (sr / 1000.0), maxLag = (int) (sr / 60.0);
    std::vector<double> ac ((size_t) maxLag + 2, 0.0);
    for (int lag = minLag; lag <= maxLag + 1; ++lag)
    {
        double s = 0, e1 = 0, e2 = 0;
        for (int i = 0; i + lag < n; ++i) { s += x[i] * x[i + lag]; e1 += x[i] * x[i]; e2 += x[i + lag] * x[i + lag]; }
        ac[(size_t) lag] = s / std::sqrt (e1 * e2 + 1e-12);
    }
    for (int lag = minLag + 1; lag <= maxLag; ++lag)
        if (ac[(size_t) lag] > 0.75 && ac[(size_t) lag] > ac[(size_t) lag - 1] && ac[(size_t) lag] >= ac[(size_t) lag + 1])
        {
            const double a = ac[(size_t) lag - 1], b = ac[(size_t) lag], c = ac[(size_t) lag + 1];
            return (float) (sr / (lag + 0.5 * (a - c) / (a - 2 * b + c)));
        }
    return 0.0f;
}

static float hfRatio (const float* x, int n, double sr)
{
    tvc::Biquad hp; hp.setHighPass (sr, 1500.0, 0.707);
    double e = 0, eh = 0;
    for (int i = 0; i < n; ++i) { const float h = hp.process (x[i]); e += x[i] * x[i]; eh += h * h; }
    return (float) (eh / (e + 1e-12));
}

struct Setup { std::vector<std::pair<const char*, float>> params; int midiNote = -1; std::vector<int> scaleNotes; };

static juce::AudioBuffer<float> run (TopTuneProcessor& proc, const Setup& s, float hz, double sr, int block, double seconds,
                                     const std::vector<std::pair<float, double>>* melody = nullptr)
{
    proc.setPlayConfigDetails (2, 2, sr, block);
    for (auto& [id, v] : s.params) { auto* p = proc.apvts.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); }
    proc.prepareToPlay (sr, block);

    const int total = (int) (sr * seconds);
    juce::AudioBuffer<float> buf (2, total);
    double ph = 0.0, t0 = 0.0; size_t mi = 0;
    for (int i = 0; i < total; ++i)
    {
        const double t = i / sr;
        double f = hz;
        if (melody)
        {
            while (mi + 1 < melody->size() && t - t0 >= (*melody)[mi].second) { t0 += (*melody)[mi].second; ++mi; }
            f = (*melody)[mi].first;
        }
        f *= 1.0 + 0.002 * std::sin (2 * juce::MathConstants<double>::pi * 5.0 * t);
        ph += f / sr;
        double v = 0;
        for (int h = 1; h <= 14; ++h)
        {
            // envolvente espectral fija tipo vocal (formante ~700 Hz y ~1200 Hz)
            const double fh = f * h;
            const double env = std::exp (-std::pow ((fh - 700) / 400, 2)) + 0.6 * std::exp (-std::pow ((fh - 1200) / 500, 2)) + 0.05;
            v += env * std::sin (2 * juce::MathConstants<double>::pi * h * ph);
        }
        buf.setSample (0, i, (float) (v * 0.2)); buf.setSample (1, i, (float) (v * 0.2));
    }

    for (int pos = 0; pos < total; pos += block)
    {
        const int len = std::min (block, total - pos);
        juce::MidiBuffer midi;
        if (pos == 0)
        {
            if (s.midiNote >= 0) midi.addEvent (juce::MidiMessage::noteOn (1, s.midiNote, (juce::uint8) 100), 0);
            int k = 0;
            for (int note : s.scaleNotes) midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), k++);
        }
        juce::AudioBuffer<float> sub (buf.getArrayOfWritePointers(), 2, pos, len);
        proc.processBlock (sub, midi);
    }
    return buf;
}

static int fails = 0;

static void check (const char* name, const Setup& s, float inHz, float expectHz, double sr = 44100.0, int block = 512, bool live = false)
{
    TopTuneProcessor proc;
    Setup s2 = s;
    if (live) s2.params.push_back ({ "live", 1.0f });
    auto out = run (proc, s2, inHz, sr, block, 3.0);
    bool finite = true; float peak = 0;
    for (int i = 0; i < out.getNumSamples(); ++i) { const float v = out.getSample (0, i); finite &= std::isfinite (v); peak = std::max (peak, std::abs (v)); }
    const float got = estimatePitch (out.getReadPointer (0) + (int) (sr * 2.0), (int) (sr * 0.12), sr);
    const float cents = got > 0 ? 1200.0f * std::log2 (got / expectHz) : 9999.0f;
    const bool ok = finite && peak <= 1.0f && std::abs (cents) < 12.0f;
    if (! ok) ++fails;
    std::printf ("%-40s in %6.1f -> %6.1f Hz (esperado %6.1f, %+6.1f cents) peak %.2f lat %4d  %s\n",
                 name, inHz, got, expectHz, cents, peak, proc.getLatencySamples(), ok ? "OK" : "FALLO");
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;

    if (argc > 2 && juce::String (argv[1]) == "--snapshot")
    {
        TopTuneProcessor proc;
        std::vector<std::pair<float, double>> mel { { 220.0f * std::pow (2.0f, 0.3f / 12), 0.4 }, { 246.9f * std::pow (2.0f, -0.25f / 12), 0.4 },
                                                    { 261.6f, 0.4 }, { 293.7f * std::pow (2.0f, 0.2f / 12), 0.5 }, { 329.6f, 0.6 } };
        run (proc, { { { "scale", 2 }, { "key", 9 } } }, 220.0f, 44100.0, 512, 2.3, &mel);
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        ed->setVisible (true);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
        f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("snapshot -> %s\n", f.getFullPathName().toRawUTF8());
        return 0;
    }

    const float a3 = 220.0f, sharp = a3 * std::pow (2.0f, 0.35f / 12.0f), flat = a3 * std::pow (2.0f, -0.4f / 12.0f);
    const float bb3 = 233.08f;

    check ("Natural cromatica (A3 +35c)",            { { { "style", 0 } } }, sharp, a3);
    check ("Natural 48k (A3 -40c)",                  { { { "style", 0 } } }, flat, a3, 48000.0, 256);
    check ("Expresivo C menor vel 0 (A3+35c->Bb)",   { { { "style", 1 }, { "speed", 0 }, { "scale", 2 } } }, sharp, bb3);
    check ("Sostenido Mayor A (A3+35c->A3)",         { { { "style", 2 }, { "scale", 1 }, { "key", 9 } } }, sharp, a3);
    check ("Escala Blues 96k",                       { { { "scale", 6 }, { "key", 9 } } }, sharp, a3, 96000.0, 1024);
    check ("Pitch +12 (sin afinar)",                 { { { "amount", 0 }, { "shift", 12 } } }, a3, 440.0f);
    check ("Pitch -12 (sin afinar)",                 { { { "amount", 0 }, { "shift", -12 } } }, a3, 110.0f);
    check ("Pitch +7 + afinacion",                   { { { "shift", 7 } } }, sharp, a3 * std::pow (2.0f, 7.0f / 12.0f));
    check ("Fino +50 cents",                         { { { "amount", 0 }, { "fine", 50 } } }, a3, a3 * std::pow (2.0f, 0.5f / 12.0f));
    check ("Formant +3 (tono no cambia)",            { { { "amount", 0 }, { "formant", 3 } } }, a3, a3);
    check ("Formant -3 (tono no cambia)",            { { { "amount", 0 }, { "formant", -3 } } }, a3, a3);
    check ("MIDI Repitch E (A3 -> E3)",              { { { "midi", 1 } }, 64 }, sharp, 164.81f);
    check ("MIDI Repitch C4 (A3 -> C4)",             { { { "midi", 1 } }, 60 }, sharp, 261.63f);
    check ("Modo Live cromatica",                    { { { "style", 1 } } }, sharp, a3, 44100.0, 512, true);
    check ("Modo Live + shift +5",                   { { { "amount", 0 }, { "shift", 5 } } }, a3, a3 * std::pow (2.0f, 5.0f / 12.0f), 44100.0, 128, true);

    // Formant: cambia el color (energia en agudos) sin cambiar el tono
    {
        TopTuneProcessor p0, pUp, pDn;
        auto o0 = run (p0, { { { "amount", 0 } } }, a3, 44100.0, 512, 2.0);
        auto oU = run (pUp, { { { "amount", 0 }, { "formant", 4 } } }, a3, 44100.0, 512, 2.0);
        auto oD = run (pDn, { { { "amount", 0 }, { "formant", -4 } } }, a3, 44100.0, 512, 2.0);
        const int st = 44100, len = 22050;
        const float h0 = hfRatio (o0.getReadPointer (0) + st, len, 44100), hU = hfRatio (oU.getReadPointer (0) + st, len, 44100), hD = hfRatio (oD.getReadPointer (0) + st, len, 44100);
        const bool ok = hU > h0 * 1.3f && hD < h0 * 0.8f;
        if (! ok) ++fails;
        std::printf ("%-40s agudos: normal %.3f  formant+4 %.3f  formant-4 %.3f  %s\n", "Formant cambia el color", h0, hU, hD, ok ? "OK" : "FALLO");
    }

    // MIDI Escala: tocar C, D#, G -> escala personalizada
    {
        TopTuneProcessor p;
        Setup s { { { "midi", 2 }, { "scale", 7 } }, -1, { 60, 63, 67 } };
        auto out = run (p, s, sharp, 44100.0, 512, 2.5);
        const int mask = p.meters.customMask.load();
        const float got = estimatePitch (out.getReadPointer (0) + 88200, 5000, 44100.0);
        // A3+35c (57.35) -> notas permitidas C, D#, G -> G3 (55) o C4 (60): mas cerca G3 (2.35) vs C4 (2.65)
        const bool ok = mask == ((1 << 0) | (1 << 3) | (1 << 7)) && std::abs (1200 * std::log2 (got / 196.0f)) < 12;
        if (! ok) ++fails;
        std::printf ("%-40s mascara 0x%03X  salida %.1f Hz (esperado G3 196.0)  %s\n", "MIDI Escala (C, D#, G)", mask, got, ok ? "OK" : "FALLO");
    }

    // Detector de tonalidad con una melodia en A menor
    {
        TopTuneProcessor p;
        p.startKeyDetection();
        auto hz = [] (int midi) { return 440.0f * std::pow (2.0f, (midi - 69) / 12.0f); };
        std::vector<std::pair<float, double>> mel;
        for (int rep = 0; rep < 3; ++rep)
            for (int n : { 57, 60, 64, 62, 60, 59, 57, 64, 65, 64, 62, 57 })
                mel.push_back ({ hz (n), 0.3 });
        run (p, {}, 220.0f, 44100.0, 512, 10.5, &mel);
        float h[12]; for (int i = 0; i < 12; ++i) h[i] = p.meters.keyHist[i].load();
        int root; bool minor; float conf;
        toptune::KeyFinder::find (h, root, minor, conf);
        const bool ok = root == 9 && minor;
        if (! ok) ++fails;
        std::printf ("%-40s detectado %s %s (%.0f%%)  %s\n", "Detector de tonalidad (A menor)",
                     TopTuneProcessor::keyNames()[root].toRawUTF8(), minor ? "menor" : "mayor", conf * 100, ok ? "OK" : "FALLO");
    }

    std::printf ("\n%s (%d fallos)\n", fails == 0 ? "TODAS LAS PRUEBAS OK" : "HAY FALLOS", fails);
    return fails;
}
