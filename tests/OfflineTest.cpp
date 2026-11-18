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
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
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

    std::printf ("\n%s (%d fallos)\n", fails == 0 ? "TODAS LAS PRUEBAS OK" : "HAY FALLOS", fails);
    return fails;
}
