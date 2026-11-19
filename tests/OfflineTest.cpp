// Prueba offline: genera una "voz" sintetica desafinada, la pasa por la cadena
// completa y verifica afinacion, estabilidad numerica, niveles y uso de CPU.
#include "PluginProcessor.h"
#include <cstdio>

static float estimatePitch (const float* x, int n, double sr)
{
    // Autocorrelacion normalizada, 70-1000 Hz
    int best = -1; double bestVal = 0.0;
    const int minLag = (int) (sr / 1000.0), maxLag = (int) (sr / 70.0);
    std::vector<double> ac ((size_t) maxLag + 2, 0.0);
    for (int lag = minLag; lag <= maxLag + 1; ++lag)
    {
        double s = 0, e1 = 0, e2 = 0;
        for (int i = 0; i + lag < n; ++i) { s += x[i] * x[i + lag]; e1 += x[i] * x[i]; e2 += x[i + lag] * x[i + lag]; }
        ac[(size_t) lag] = s / std::sqrt (e1 * e2 + 1e-12);
    }
    for (int lag = minLag + 1; lag <= maxLag; ++lag)
        if (ac[(size_t) lag] > ac[(size_t) lag - 1] && ac[(size_t) lag] >= ac[(size_t) lag + 1] && ac[(size_t) lag] > bestVal * 1.0 && ac[(size_t) lag] > 0.8)
        { best = lag; bestVal = ac[(size_t) lag]; break; }
    if (best < 0) return 0.0f;
    const double a = ac[(size_t) best - 1], b = ac[(size_t) best], c = ac[(size_t) best + 1];
    const double shift = 0.5 * (a - c) / (a - 2 * b + c);
    return (float) (sr / (best + shift));
}


// ---------------------------------------------------------------------------
// Pruebas de los modelos (saturacion / compresor / EQ) con todo lo demas apagado
static juce::AudioBuffer<float> runIsolated (std::vector<std::pair<const char*, float>> params,
                                             std::function<float (int, double)> gen, double seconds, double sr = 48000.0)
{
    TopVocalChainProcessor proc;
    proc.setPlayConfigDetails (2, 2, sr, 512);
    std::vector<std::pair<const char*, float>> base { { "tune_on", 0 }, { "pre_on", 0 }, { "eq_on", 0 }, { "comp_on", 0 },
                                                      { "dly_on", 0 }, { "rev_on", 0 }, { "ai_amount", 0 } };
    for (auto& kv : params) base.push_back (kv);
    for (auto& [id, v] : base) { auto* prm = proc.apvts.getParameter (id); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); }
    proc.prepareToPlay (sr, 512);
    const int total = (int) (sr * seconds);
    juce::AudioBuffer<float> b (2, total);
    for (int i = 0; i < total; ++i) { const float v = gen (i, sr); b.setSample (0, i, v); b.setSample (1, i, v); }
    juce::MidiBuffer m;
    for (int pos = 0; pos < total; pos += 512)
    {
        juce::AudioBuffer<float> sub (b.getArrayOfWritePointers(), 2, pos, std::min (512, total - pos));
        proc.processBlock (sub, m);
    }
    return b;
}

static double harmonicLevel (const float* x, int n, double sr, double f)
{
    double re = 0, im = 0;
    for (int i = 0; i < n; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (2 * juce::MathConstants<double>::pi * i / (n - 1));
        re += x[i] * w * std::cos (2 * juce::MathConstants<double>::pi * f * i / sr);
        im += x[i] * w * std::sin (2 * juce::MathConstants<double>::pi * f * i / sr);
    }
    return std::sqrt (re * re + im * im) * 4.0 / n;
}

static int testModels()
{
    int f = 0;
    const double sr = 48000.0;
    auto sine = [] (double hz, double amp) { return [hz, amp] (int i, double s) { return (float) (amp * std::sin (2 * juce::MathConstants<double>::pi * hz * i / s)); }; };

    std::printf ("\n== SATURACION (seno 200 Hz, drive 12 dB) ==\n");
    for (int t = 0; t < 5; ++t)
    {
        auto o = runIsolated ({ { "pre_on", 1 }, { "pre_type", (float) t }, { "pre_drive", 12 }, { "pre_warmth", 50 } }, sine (200, 0.3), 1.0, sr);
        const int st = 24000, n = 19200;
        const double h1 = harmonicLevel (o.getReadPointer (0) + st, n, sr, 200), h2 = harmonicLevel (o.getReadPointer (0) + st, n, sr, 400),
                     h3 = harmonicLevel (o.getReadPointer (0) + st, n, sr, 600), h5 = harmonicLevel (o.getReadPointer (0) + st, n, sr, 1000);
        float peak = o.getMagnitude (0, st, n); bool fin = true;
        for (int i = 0; i < o.getNumSamples(); ++i) fin &= std::isfinite (o.getSample (0, i));
        const bool ok = fin && peak <= 1.0f && h1 > 0.05;
        if (! ok) ++f;
        std::printf ("  %-14s H2 %6.1f dB  H3 %6.1f dB  H5 %6.1f dB (rel. fundamental)  peak %.2f  %s\n",
                     TopVocalChainProcessor::preTypeNames()[t].toRawUTF8(), 20 * std::log10 (h2 / h1 + 1e-9), 20 * std::log10 (h3 / h1 + 1e-9),
                     20 * std::log10 (h5 / h1 + 1e-9), peak, ok ? "OK" : "FALLO");
    }

    std::printf ("\n== COMPRESOR (salto de -30 a -6 dBFS, thr -24, ratio 4, attack 5 ms) ==\n");
    for (int t = 0; t < 4; ++t)
    {
        auto gen = [] (int i, double s) { const double amp = i < s * 0.5 ? 0.0316 : 0.5; return (float) (amp * std::sin (2 * juce::MathConstants<double>::pi * 1000 * i / s)); };
        auto o = runIsolated ({ { "comp_on", 1 }, { "comp_type", (float) t }, { "comp_thr", -24 }, { "comp_ratio", 4 }, { "comp_att", 5 },
                                { "comp_rel", 120 }, { "comp_makeup", 0 } }, gen, 1.5, sr);
        const double lat = 0.5 + 679.0 / sr;   // latencia del plugin a 48 kHz
        auto lvl = [&] (double t0) { return 20 * std::log10 (o.getMagnitude (0, (int) (t0 * sr), (int) (0.001 * sr)) + 1e-9); };
        const double atJump = lvl (lat + 0.0005), at5ms = lvl (lat + 0.005), at50ms = lvl (lat + 0.05), steady = lvl (1.4);
        bool fin = true; for (int i = 0; i < o.getNumSamples(); ++i) fin &= std::isfinite (o.getSample (0, i));
        const bool ok = fin && steady < -8.0 && atJump < 0.5;
        if (! ok) ++f;
        std::printf ("  %-28s 1ms %6.1f  5ms %6.1f  50ms %6.1f  estable %6.1f dBFS (entrada -6)  %s\n",
                     TopVocalChainProcessor::compTypeNames()[t].toRawUTF8(), atJump, at5ms, at50ms, steady, ok ? "OK" : "FALLO");
    }

    std::printf ("\n== EQ (Mid +9 dB @ 1000 Hz, Low +6, Aire +6) respuesta en dB ==\n");
    const double freqs[] { 40, 60, 100, 220, 700, 1000, 1600, 4000, 8000, 14000 };
    std::printf ("  %-30s", "");
    for (double fr : freqs) std::printf ("%7.0f", fr);
    std::printf ("\n");
    for (int t = 0; t < 4; ++t)
    {
        std::printf ("  %-30s", TopVocalChainProcessor::eqTypeNames()[t].toRawUTF8());
        bool ok = true;
        for (double fr : freqs)
        {
            auto o = runIsolated ({ { "eq_on", 1 }, { "eq_type", (float) t }, { "eq_lowcut", 20 }, { "eq_low_gain", 6 }, { "eq_mid_freq", 1000 },
                                    { "eq_mid_gain", 9 }, { "eq_mid_q", 1 }, { "eq_pres_gain", 0 }, { "eq_air_gain", 6 } }, sine (fr, 0.05), 0.5, sr);
            const double g = 20 * std::log10 (harmonicLevel (o.getReadPointer (0) + 9600, 9600, sr, fr) / 0.05);
            ok &= std::isfinite (g) && g < 20.0;
            std::printf ("%7.1f", g);
        }
        if (! ok) ++f;
        std::printf ("  %s\n", ok ? "OK" : "FALLO");
    }
    return f;
}

static int runCase (const char* label, int program, float inputHz, float expectHz, double sr, int block)
{
    TopVocalChainProcessor proc;
    proc.setPlayConfigDetails (2, 2, sr, block);
    proc.setCurrentProgram (program);
    proc.prepareToPlay (sr, block);

    const int seconds = 6;
    const int total = (int) sr * seconds;
    juce::AudioBuffer<float> all (2, total);
    double ph = 0.0;
    for (int i = 0; i < total; ++i)
    {
        const double t = i / sr;
        const double f = inputHz * (1.0 + 0.002 * std::sin (2.0 * juce::MathConstants<double>::pi * 5.0 * t));
        ph += f / sr;
        double v = 0.0;
        for (int h = 1; h <= 12; ++h) v += std::sin (2.0 * juce::MathConstants<double>::pi * h * ph) / (h * 1.3);
        const double env = (t > 4.5 && t < 5.0) ? 0.0 : 0.25;   // un silencio
        const float s = (float) (v * env);
        all.setSample (0, i, s); all.setSample (1, i, s);
    }

    juce::MidiBuffer midi;
    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    for (int pos = 0; pos < total; pos += block)
    {
        const int len = std::min (block, total - pos);
        juce::AudioBuffer<float> sub (all.getArrayOfWritePointers(), 2, pos, len);
        proc.processBlock (sub, midi);
    }
    const double ms = juce::Time::getMillisecondCounterHiRes() - t0;

    bool finite = true; float peak = 0.0f;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < total; ++i)
        {
            const float v = all.getSample (ch, i);
            if (! std::isfinite (v)) finite = false;
            peak = std::max (peak, std::abs (v));
        }

    // Mide el tono en 3.5 s - 3.6 s (reverb/delay presentes, pero la voz domina)
    const int start = (int) (sr * 3.5);
    const float outHz = estimatePitch (all.getReadPointer (0) + start, (int) (sr * 0.1), sr);
    const float centsErr = expectHz > 0 && outHz > 0 ? 1200.0f * std::log2 (outHz / expectHz) : 9999.0f;
    const double cpu = 100.0 * ms / (seconds * 1000.0);

    const bool ok = finite && peak < 4.0f && (expectHz <= 0 || std::abs (centsErr) < 10.0f);
    std::printf ("%-34s in %.1f Hz -> out %.1f Hz (esperado %.1f, error %+.1f cents) peak %.2f  CPU %.2f%%  latencia %d  %s\n",
                 label, inputHz, outHz, expectHz, centsErr, peak, cpu, proc.getLatencySamples(), ok ? "OK" : "FALLO");
    std::printf ("    IA: gain %+.1f dB  lowcut %+.0f Hz  mud %+.1f  pres %+.1f  aire %+.1f  deess %+.1f  thr %+.1f  ratio %+.1f  conf %.2f  voz %.0f Hz\n",
                 proc.meters.aiGain.load(), proc.meters.aiLowCut.load(), proc.meters.aiMud.load(), proc.meters.aiPresence.load(),
                 proc.meters.aiAir.load(), proc.meters.aiDeEss.load(), proc.meters.aiCompThr.load(), proc.meters.aiCompRatio.load(),
                 proc.meters.aiConfidence.load(), proc.meters.aiAvgPitch.load());
    return ok ? 0 : 1;
}

static int snapshot (const char* path)
{
    TopVocalChainProcessor proc;
    proc.setPlayConfigDetails (2, 2, 44100.0, 512);
    proc.prepareToPlay (44100.0, 512);
    // Pasa un poco de audio para que los medidores y la IA tengan datos
    juce::AudioBuffer<float> b (2, 512); juce::MidiBuffer m; double ph = 0;
    for (int k = 0; k < 400; ++k)
    {
        for (int i = 0; i < 512; ++i) { ph += 224.5 / 44100.0; float v = 0; for (int h = 1; h < 10; ++h) v += (float) std::sin (6.283185307 * h * ph) / (h * 1.3f); b.setSample (0, i, v * 0.25f); b.setSample (1, i, v * 0.25f); }
        proc.processBlock (b, m);
    }
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setVisible (true);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.25f);
    juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (path));
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("snapshot %dx%d -> %s\n", img.getWidth(), img.getHeight(), f.getFullPathName().toRawUTF8());
    return 0;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc > 2 && juce::String (argv[1]) == "--snapshot") return snapshot (argv[2]);
    int fails = 0;
    const float a3 = 220.0f;
    const float sharp35 = a3 * std::pow (2.0f, 0.35f / 12.0f);
    const float flat40  = a3 * std::pow (2.0f, -0.40f / 12.0f);

    fails += runCase ("Default 44.1k (A3 +35c)",      0, sharp35, a3, 44100.0, 512);
    fails += runCase ("Default 48k (A3 -40c)",        0, flat40, a3, 48000.0, 256);
    fails += runCase ("Trap Duro C menor 96k (A3 +35c)", 1, sharp35, 233.08f, 96000.0, 1024);
    // C menor: A no esta en la escala -> debe ir a G# (Ab) o A# (Bb). 220*2^(0.35/12) -> mas cerca de A#? no: G#=207.65, A#=233.08
    fails += runCase ("Reggaeton C menor (A3 +35c)",  3, sharp35, 233.08f, 44100.0, 128);
    for (int prog = 0; prog < (int) tvc::getFactoryPresets().size(); ++prog)
        fails += runCase (tvc::getFactoryPresets()[(size_t) prog].name, prog, sharp35, 0.0f, 44100.0, 480);

    fails += testModels();
    std::printf ("\n%s (%d fallos)\n", fails == 0 ? "TODAS LAS PRUEBAS OK" : "HAY FALLOS", fails);
    return fails;
}
