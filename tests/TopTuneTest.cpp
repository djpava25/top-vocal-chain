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

// Calidad: estabilidad del tono, fluctuacion de volumen y periodicidad (voz "doble" = baja periodicidad)
struct Quality { float jitterCents, ampFluctDb, periodicity; };
static Quality measure (const juce::AudioBuffer<float>& b, double sr, double t0, double t1)
{
    const float* x = b.getReadPointer (0);
    std::vector<float> ps;
    for (double t = t0; t + 0.05 < t1; t += 0.05) { const float f = estimatePitch (x + (int) (t * sr), (int) (sr * 0.05), sr); if (f > 0) ps.push_back (1200.0f * std::log2 (f / 220.0f)); }
    double m = 0; for (float v : ps) m += v; m /= std::max<size_t> (1, ps.size());
    double var = 0; for (float v : ps) var += (v - m) * (v - m); var /= std::max<size_t> (1, ps.size());
    float lo = 1e9f, hi = 0;
    const int w = (int) (sr * 0.02);
    for (int i = (int) (t0 * sr); i + w < (int) (t1 * sr); i += w / 2)
    {
        double e = 0; for (int j = 0; j < w; ++j) e += x[i + j] * x[i + j];
        const float r = (float) std::sqrt (e / w); lo = std::min (lo, r); hi = std::max (hi, r);
    }
    // periodicidad: correlacion normalizada en el periodo (media de ventanas de 40 ms)
    double per = 0; int cnt = 0;
    for (double t = t0; t + 0.05 < t1; t += 0.05)
    {
        const int a = (int) (t * sr), n = (int) (sr * 0.04);
        const float f = estimatePitch (x + a, n, sr); if (f <= 0) continue;
        const double lag = sr / f; const int L = (int) lag; const double fr = lag - L;
        double s = 0, e1 = 0, e2 = 0;
        for (int j = 0; j < n; ++j) { const double y = x[a + j + L] * (1 - fr) + x[a + j + L + 1] * fr; s += x[a + j] * y; e1 += x[a + j] * x[a + j]; e2 += y * y; }
        per += s / std::sqrt (e1 * e2 + 1e-12); ++cnt;
    }
    return { (float) std::sqrt (var), 20.0f * std::log10 (hi / std::max (lo, 1e-9f)), (float) (per / std::max (1, cnt)) };
}

static void check (const char* name, const Setup& s, float inHz, float expectHz, double sr = 44100.0, int block = 512, bool live = false)
{
    TopTuneProcessor proc;
    Setup s2 = s;
    if (live) s2.params.push_back ({ "quality", 0.0f });
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

// Voz sintetica realista: melodia desafinada con deslizamientos, vibrato, jitter, respiracion y ataques
static juce::AudioBuffer<float> realVoice (double sr, double seconds)
{
    juce::Random rng (7);
    const int notes[] { 57, 60, 62, 64, 62, 60, 59, 57 };
    const float offs[] { 0.3f, -0.35f, 0.25f, -0.2f, 0.4f, -0.3f, 0.2f, -0.25f };
    const int total = (int) (sr * seconds);
    juce::AudioBuffer<float> b (2, total);
    double ph = 0, jit = 0; tvc::Biquad bp; bp.setBandPass (sr, 3000.0, 0.7);
    const double noteLen = seconds / 8.0;
    for (int i = 0; i < total; ++i)
    {
        const double t = i / sr;
        const int k = std::min (7, (int) (t / noteLen));
        const double tn = t - k * noteLen;
        double m = notes[k] + offs[k];
        if (k > 0 && tn < 0.07) { const double prev = notes[k - 1] + offs[k - 1]; m = prev + (m - prev) * (tn / 0.07); }
        if (tn > 0.18) m += 0.35 * std::sin (2 * juce::MathConstants<double>::pi * 5.5 * t) * std::min (1.0, (tn - 0.18) / 0.2);
        jit += (rng.nextDouble() - 0.5) * 0.004; jit *= 0.995;
        const double f = 440.0 * std::pow (2.0, (m + jit - 69) / 12.0);
        ph += f / sr;
        const double env = std::min (1.0, tn / 0.03) * std::min (1.0, (noteLen - tn) / 0.04) * (1.0 + 0.04 * std::sin (2 * juce::MathConstants<double>::pi * 3.1 * t));
        double v = 0;
        for (int h = 1; h <= 18; ++h)
        {
            const double fh = f * h;
            const double e = std::exp (-std::pow ((fh - 650) / 350, 2)) + 0.7 * std::exp (-std::pow ((fh - 1100) / 450, 2)) + 0.25 * std::exp (-std::pow ((fh - 2600) / 600, 2)) + 0.03;
            v += e * std::sin (2 * juce::MathConstants<double>::pi * h * ph + h * 0.3);
        }
        const float breath = bp.process ((float) (rng.nextDouble() * 2 - 1)) * 0.02f;
        const float x = (float) (v * 0.18 * env) + breath;
        b.setSample (0, i, x); b.setSample (1, i, x);
    }
    return b;
}
static juce::AudioBuffer<float> process (TopTuneProcessor& proc, const Setup& s, const juce::AudioBuffer<float>& in, double sr, int block)
{
    proc.setPlayConfigDetails (2, 2, sr, block);
    for (auto& [id, v] : s.params) { auto* p = proc.apvts.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); }
    proc.prepareToPlay (sr, block);
    juce::AudioBuffer<float> buf (in);
    for (int pos = 0; pos < buf.getNumSamples(); pos += block)
    {
        juce::AudioBuffer<float> sub (buf.getArrayOfWritePointers(), 2, pos, std::min (block, buf.getNumSamples() - pos));
        juce::MidiBuffer m; proc.processBlock (sub, m);
    }
    return buf;
}
// desviacion de la envolvente (dB, percentil 95) y "clics" (picos de derivada respecto a la entrada)
static void envelopeAndClicks (const juce::AudioBuffer<float>& in, const juce::AudioBuffer<float>& out, int lat, double sr, float& envDev, float& clickRatio)
{
    const int w = (int) (sr * 0.012);
    std::vector<float> devs;
    for (int i = (int) (sr * 0.2); i + w + lat < out.getNumSamples(); i += w)
    {
        double ei = 0, eo = 0;
        for (int j = 0; j < w; ++j) { const float a = in.getSample (0, i + j), b = out.getSample (0, i + j + lat); ei += a * a; eo += b * b; }
        if (ei / w < 1e-4) continue;
        devs.push_back ((float) std::abs (10.0 * std::log10 ((eo + 1e-12) / (ei + 1e-12))));
    }
    std::sort (devs.begin(), devs.end());
    envDev = devs.empty() ? 99.0f : devs[(size_t) (devs.size() * 0.95)];
    auto spikes = [sr] (const juce::AudioBuffer<float>& b)
    {
        // segunda derivada: un clic es un salto brusco que no sigue la forma de onda
        std::vector<float> d;
        for (int i = 2; i < b.getNumSamples(); ++i) d.push_back (std::abs (b.getSample (0, i) - 2 * b.getSample (0, i - 1) + b.getSample (0, i - 2)));
        std::vector<float> s (d); std::nth_element (s.begin(), s.begin() + (long) (s.size() * 0.999), s.end());
        juce::ignoreUnused (sr);
        return s[(size_t) (s.size() * 0.999)];
    };
    clickRatio = spikes (out) / std::max (1e-6f, spikes (in));
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;

    if (argc > 2 && juce::String (argv[1]) == "--snapshot")
    {
        TopTuneProcessor proc;
        std::vector<std::pair<float, double>> mel { { 220.0f * std::pow (2.0f, 0.3f / 12), 0.4 }, { 246.9f * std::pow (2.0f, -0.25f / 12), 0.4 },
                                                    { 261.6f, 0.4 }, { 293.7f * std::pow (2.0f, 0.2f / 12), 0.5 }, { 329.6f, 0.6 } };
        proc.lightTheme = argc > 3 && juce::String (argv[3]) == "light";
        run (proc, { { { "scale", 2 }, { "key", 9 }, { "sustain", 8 }, { "amount", 88 }, { "speed", 110 }, { "mix", 92 } } }, 220.0f, 44100.0, 512, 2.3, &mel);
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        ed->setVisible (true);
        if (auto* te = dynamic_cast<TopTuneEditor*> (ed.get())) for (int i = 0; i < 6; ++i) te->tickNow();
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
    // Calidad de la correccion (A3 +35 cents con vibrato leve). Referencia: la senal original sin procesar
    float refVol = 0.0f;
    {
        TopTuneProcessor p;
        auto out = run (p, { { { "engine", 0 } } }, sharp, 44100.0, 256, 3.0);
        const auto q = measure (out, 44100.0, 1.0, 2.8);
        refVol = q.ampFluctDb;
        std::printf ("%-40s jitter %.2f cents  vol %.2f dB  periodicidad %.4f\n", "Referencia (senal original)", q.jitterCents, q.ampFluctDb, q.periodicity);
    }
    for (int mode = 0; mode < 3; ++mode)
    {
        TopTuneProcessor p;
        Setup st { { { "style", 1 }, { "speed", 10 }, { "quality", (float) (2 - mode) } } };
        auto out = run (p, st, sharp, 44100.0, 256, 3.0);
        const auto q = measure (out, 44100.0, 1.0, 2.8);
        const bool ok = q.jitterCents < 4.0f && q.ampFluctDb < refVol + 0.6f && q.periodicity > 0.99f;
        if (! ok) ++fails;
        std::printf ("%-40s jitter %.2f cents  vol %.2f dB  periodicidad %.4f  lat %d  %s\n", mode == 0 ? "Calidad HIGH Q" : mode == 1 ? "Calidad LOW LAT" : "Calidad REALTIME",
                     q.jitterCents, q.ampFluctDb, q.periodicity, p.getLatencySamples(), ok ? "OK" : "FALLO");
    }

    // Voz realista (melodia desafinada, vibrato, respiracion) en los tres motores
    {
        const auto voice = realVoice (44100.0, 4.0);
        const char* names[] { "Voz realista REALTIME", "Voz realista LOW LAT", "Voz realista HIGH Q" };
        for (int mode = 0; mode < 3; ++mode)
        {
            TopTuneProcessor p;
            auto out = process (p, { { { "quality", (float) mode }, { "scale", 2 }, { "key", 9 }, { "speed", 20 } } }, voice, 44100.0, 256);
            float env, clicks; envelopeAndClicks (voice, out, p.getLatencySamples(), 44100.0, env, clicks);
            bool finite = true; for (int i = 0; i < out.getNumSamples(); ++i) finite &= std::isfinite (out.getSample (0, i));
            const bool ok = finite && env < 2.0f && clicks < 2.0f;
            if (! ok) ++fails;
            std::printf ("%-40s envolvente %.2f dB  clics x%.2f  %s\n", names[mode], env, clicks, ok ? "OK" : "FALLO");
        }
    }

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
