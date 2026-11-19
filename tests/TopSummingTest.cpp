// Pruebas offline de Top Summing
#include "TopSummingProcessor.h"
#include "TopSummingEditor.h"
#include <cstdio>

static int fails = 0;
static const double SR = 48000.0;

static double tone (const float* x, int n, double f)
{
    double re = 0, im = 0;
    for (int i = 0; i < n; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (2 * juce::MathConstants<double>::pi * i / (n - 1));
        re += x[i] * w * std::cos (2 * juce::MathConstants<double>::pi * f * i / SR);
        im += x[i] * w * std::sin (2 * juce::MathConstants<double>::pi * f * i / SR);
    }
    return std::sqrt (re * re + im * im) * 4.0 / n;
}

static double db (double v) { return 20.0 * std::log10 (v + 1e-12); }

static juce::AudioBuffer<float> run (TopSummingProcessor& p, std::vector<std::pair<const char*, float>> params,
                                     std::function<float (int)> genL, std::function<float (int)> genR, double secs = 1.0)
{
    p.setPlayConfigDetails (2, 2, SR, 512);
    for (auto& [id, v] : params) { auto* prm = p.apvts.getParameter (id); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); }
    p.prepareToPlay (SR, 512);
    const int total = (int) (SR * secs);
    juce::AudioBuffer<float> b (2, total);
    for (int i = 0; i < total; ++i) { b.setSample (0, i, genL (i)); b.setSample (1, i, genR (i)); }
    juce::MidiBuffer m;
    for (int pos = 0; pos < total; pos += 512)
    {
        juce::AudioBuffer<float> sub (b.getArrayOfWritePointers(), 2, pos, std::min (512, total - pos));
        p.processBlock (sub, m);
    }
    return b;
}

static std::function<float (int)> sine (double f, double amp)
{
    return [f, amp] (int i) { return (float) (amp * std::sin (2 * juce::MathConstants<double>::pi * f * i / SR)); };
}
static std::function<float (int)> silence() { return [] (int) { return 0.0f; }; }

static void report (const char* name, bool ok, const juce::String& info)
{
    if (! ok) ++fails;
    std::printf ("%-44s %s  %s\n", name, info.toRawUTF8(), ok ? "OK" : "FALLO");
}

static double thd (const juce::AudioBuffer<float>& b, double f)
{
    const float* x = b.getReadPointer (0) + (int) (SR * 0.5);
    const int n = (int) (SR * 0.4);
    const double f1 = tone (x, n, f);
    double h = 0;
    for (int k = 2; k <= 7; ++k) { const double a = tone (x, n, f * k); h += a * a; }
    return db (std::sqrt (h) / f1);
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;

    if (argc > 2 && juce::String (argv[1]) == "--snapshot")
    {
        TopSummingProcessor p;
        run (p, { { "bus", 1 }, { "drive", 4 } }, sine (220, 0.2), sine (220, 0.2), 0.5);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        ed->setVisible (true);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1200.0f / (float) ed->getWidth());
        juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
        f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("snapshot -> %s\n", f.getFullPathName().toRawUTF8());
        return 0;
    }

    const double amp = 0.125;   // -18 dBFS (nivel de trabajo tipico de una pista)

    // 1. Neutral: casi transparente a nivel normal
    {
        TopSummingProcessor p;
        auto o = run (p, { { "analog", 0 } }, sine (1000, amp), sine (1000, amp));
        const double g = db (tone (o.getReadPointer (0) + 24000, 19200, 1000) / amp);
        const double t = thd (o, 1000);
        report ("Neutral 1 kHz -18 dBFS", std::abs (g) < 1.0 && t < -40, juce::String::formatted ("ganancia %+.2f dB  THD %.1f dB", g, t));
    }

    // 2. Mas DRIVE = mas armonicos
    {
        TopSummingProcessor a, b;
        const double t0 = thd (run (a, { { "drive", 0 } }, sine (1000, amp), sine (1000, amp)), 1000);
        const double t12 = thd (run (b, { { "drive", 12 } }, sine (1000, amp), sine (1000, amp)), 1000);
        report ("Drive 0 vs +12 dB (1 kHz)", t12 > t0 + 10, juce::String::formatted ("THD %.1f -> %.1f dB", t0, t12));
    }

    // 3. El hierro satura mas los graves
    {
        TopSummingProcessor a, b;
        const double tl = thd (run (a, { { "drive", 9 }, { "iron", 100 } }, sine (60, amp), sine (60, amp)), 60);
        const double th = thd (run (b, { { "drive", 9 }, { "iron", 100 } }, sine (2000, amp), sine (2000, amp)), 2000);
        report ("Iron: graves saturan mas que agudos", tl > th + 3, juce::String::formatted ("THD 60 Hz %.1f dB  vs  2 kHz %.1f dB", tl, th));
    }

    // 4. IRON 0 vs 100
    {
        TopSummingProcessor a, b;
        const double t0 = thd (run (a, { { "drive", 6 }, { "iron", 0 } }, sine (100, amp), sine (100, amp)), 100);
        const double t1 = thd (run (b, { { "drive", 6 }, { "iron", 100 } }, sine (100, amp), sine (100, amp)), 100);
        report ("Iron 0 vs 100 (100 Hz)", t1 > t0 + 6, juce::String::formatted ("THD %.1f -> %.1f dB", t0, t1));
    }

    // 5. Crosstalk: solo en modo BUS
    {
        TopSummingProcessor a, b;
        auto oc = run (a, { { "bus", 0 }, { "crosstalk", 100 } }, sine (5000, amp), silence());
        auto ob = run (b, { { "bus", 1 }, { "crosstalk", 100 } }, sine (5000, amp), silence());
        const double xc = db (tone (oc.getReadPointer (1) + 24000, 19200, 5000) / amp);
        const double xb = db (tone (ob.getReadPointer (1) + 24000, 19200, 5000) / amp);
        report ("Crosstalk (L -> R, 5 kHz)", xc < -90 && xb > -55 && xb < -30, juce::String::formatted ("Channel %.1f dB  Bus %.1f dB", xc, xb));
    }

    // 6. Width
    {
        TopSummingProcessor a, b;
        auto om = run (a, { { "width", 0 }, { "analog", 0 } }, sine (440, amp), silence());
        auto ow = run (b, { { "width", 200 }, { "analog", 0 } }, sine (440, amp), silence());
        const double lm = tone (om.getReadPointer (0) + 24000, 19200, 440), rm = tone (om.getReadPointer (1) + 24000, 19200, 440);
        const double lw = tone (ow.getReadPointer (0) + 24000, 19200, 440), rw = tone (ow.getReadPointer (1) + 24000, 19200, 440);
        report ("Width 0% = mono / 200% = mas ancho", std::abs (db (lm / rm)) < 0.1 && db (rw / lw) < -8,
                juce::String::formatted ("0%%: L/R %.2f dB   200%%: R/L %.1f dB", db (lm / rm), db (rw / lw)));
    }

    // 7. Tolerancias analogicas: L y R levemente distintos
    {
        TopSummingProcessor a, b;
        auto oa = run (a, { { "analog", 1 }, { "drive", 6 } }, sine (100, amp), sine (100, amp));
        auto ob = run (b, { { "analog", 0 }, { "drive", 6 } }, sine (100, amp), sine (100, amp));
        auto diff = [] (const juce::AudioBuffer<float>& o)
        {
            double e = 0, d = 0;
            for (int i = 24000; i < 43200; ++i) { const double l = o.getSample (0, i), r = o.getSample (1, i); e += l * l; d += (l - r) * (l - r); }
            return 10 * std::log10 (d / e + 1e-20);
        };
        const double da = diff (oa), dbb = diff (ob);
        report ("Analog ON: L/R distintos, OFF: identicos", da > -70 && da < -15 && dbb < -100,
                juce::String::formatted ("diferencia L-R: ON %.1f dB  OFF %.1f dB", da, dbb));
    }

    // 8. Ruido de fondo
    {
        TopSummingProcessor a, b;
        auto on = run (a, { { "noise", 1 } }, silence(), silence());
        auto off = run (b, { { "noise", 0 } }, silence(), silence());
        const double rn = db (on.getRMSLevel (0, 24000, 19200)), rf = db (off.getRMSLevel (0, 24000, 19200));
        report ("Ruido de fondo", rn > -105 && rn < -80 && rf < -140, juce::String::formatted ("NOISE on %.1f dBFS  off %.1f dBFS", rn, rf));
    }

    // 9. SUM IN apagado: pasa limpio
    {
        TopSummingProcessor a;
        auto o = run (a, { { "sum_in", 0 }, { "drive", 18 }, { "iron", 100 } }, sine (1000, amp), sine (1000, amp));
        const double g = db (tone (o.getReadPointer (0) + 24000, 19200, 1000) / amp), t = thd (o, 1000);
        report ("SUM IN apagado (bypass)", std::abs (g) < 0.2 && t < -90, juce::String::formatted ("ganancia %+.2f dB  THD %.1f dB", g, t));
    }

    // 10. Todos los presets: estable, sin clip a -6 dBFS
    for (int k = 0; k < 9; ++k)
    {
        TopSummingProcessor a;
        a.setCurrentProgram (k);
        auto o = run (a, {}, sine (80, 0.5), sine (120, 0.5));
        bool fin = true; float pk = 0;
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < o.getNumSamples(); ++i) { const float v = o.getSample (ch, i); fin &= std::isfinite (v); pk = std::max (pk, std::abs (v)); }
        report (("Preset: " + a.getProgramName (k)).toRawUTF8(), fin && pk < 1.5f, juce::String::formatted ("pico %.2f  latencia %d", pk, a.getLatencySamples()));
    }

    std::printf ("\n%s (%d fallos)\n", fails == 0 ? "TODAS LAS PRUEBAS OK" : "HAY FALLOS", fails);
    return fails;
}
