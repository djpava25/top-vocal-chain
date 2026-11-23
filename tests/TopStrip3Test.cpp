// Pruebas offline de Top Strip 737 V3: el modelo contra las especificaciones del hardware
#include "TopStrip3Processor.h"
#include "TopStrip3Editor.h"
#include <cstdio>

static int fails = 0;
static const double SR = 48000.0;
static const double TWO_PI = 2.0 * juce::MathConstants<double>::pi;

static double tone (const float* x, int n, double f)
{
    double re = 0, im = 0;
    for (int i = 0; i < n; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (TWO_PI * i / (n - 1));
        re += x[i] * w * std::cos (TWO_PI * f * i / SR);
        im += x[i] * w * std::sin (TWO_PI * f * i / SR);
    }
    return std::sqrt (re * re + im * im) * 4.0 / n;
}
static double db (double v) { return 20.0 * std::log10 (v + 1e-15); }

using Params = std::vector<std::pair<const char*, float>>;
static void setParams (TopStrip3Processor& p, const Params& params)
{
    for (auto& [id, v] : params) { auto* prm = p.apvts.getParameter (id); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); }
}
static juce::AudioBuffer<float> run (TopStrip3Processor& p, const Params& params, std::function<float (int)> genL,
                                     std::function<float (int)> genR, double secs = 1.0)
{
    p.setPlayConfigDetails (2, 2, SR, 512);
    setParams (p, params);
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
static juce::AudioBuffer<float> run1 (const Params& params, std::function<float (int)> gen, double secs = 1.0)
{
    TopStrip3Processor p; return run (p, params, gen, gen, secs);
}
static std::function<float (int)> sine (double f, double amp)
{
    return [f, amp] (int i) { return (float) (amp * std::sin (TWO_PI * f * i / SR)); };
}
static void report (const char* name, bool ok, const juce::String& info)
{
    if (! ok) ++fails;
    std::printf ("%-46s %s  %s\n", name, info.toRawUTF8(), ok ? "OK" : "FALLO");
}
// nivel (dB) de la componente f entre t0 y t1 segundos
static double level (const juce::AudioBuffer<float>& b, double f, double t0 = 0.5, double t1 = 0.9, int chn = 0)
{
    const int a = (int) (SR * t0), n = (int) (SR * (t1 - t0));
    return db (tone (b.getReadPointer (chn) + a, n, f));
}
static double thdPct (const juce::AudioBuffer<float>& b, double f, double t0 = 0.5, double t1 = 0.9)
{
    const int a = (int) (SR * t0), n = (int) (SR * (t1 - t0));
    const float* x = b.getReadPointer (0) + a;
    const double f1 = tone (x, n, f);
    double h = 0;
    for (int k = 2; k <= 9 && f * k < SR * 0.45; ++k) { const double v = tone (x, n, f * k); h += v * v; }
    return 100.0 * std::sqrt (h) / f1;
}
static double dbuToAmp (double dbu) { return std::pow (10.0, (dbu - 22.0) / 20.0); }   // 0 dBFS = +22 dBu
static double gainAt (const Params& prm, double f, double amp, double secs = 1.0)
{
    const double t0 = secs * 0.5, t1 = secs * 0.9;
    return level (run1 (prm, sine (f, amp), secs), f, t0, t1) - db (amp);
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;

    if (argc > 2 && juce::String (argv[1]) == "--snapshot")
    {
        TopStrip3Processor p;
        const int preset = argc > 3 ? juce::String (argv[3]).getIntValue() : 1;
        p.setCurrentProgram (preset);
        Params extra;
        if (argc > 5) extra = { { "high_gain", 1 }, { "phase", 1 }, { "p48", 1 }, { "hpf_on", 1 }, { "eq_pre", 1 }, { "comp_on", 1 },
                                { "lm_hiq", 1 }, { "lm_x10", 1 }, { "hm_hiq", 1 }, { "hm_x10", 1 }, { "sc_link", 1 }, { "eq_on", 1 }, { "pre_gain", -8 } };
        run (p, extra, sine (220, 0.06), sine (220, 0.06), 0.6);
        std::printf ("VU %.1f dBFS  GR %.1f dB\n", p.vuDb.load(), p.grDb.load());
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        if (argc > 4) ed->setSize (TopStrip3Editor::fullW, TopStrip3Editor::fullH);
        ed->setVisible (true);
        if (auto* te = dynamic_cast<TopStrip3Editor*> (ed.get())) for (int i = 0; i < 40; ++i) te->tickNow();
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
        f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("snapshot -> %s (%dx%d)\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
        return 0;
    }

    if (argc > 2 && juce::String (argv[1]) == "--anim")
    {
        // cuadros de la animacion: cambio de preset (las perillas giran con inercia) y la aguja del VU con su fisica
        TopStrip3Processor p;
        p.setPlayConfigDetails (2, 2, SR, 512); p.prepareToPlay (SR, 512);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        ed->setSize (TopStrip3Editor::fullW, TopStrip3Editor::fullH); ed->setVisible (true);
        auto* te = dynamic_cast<TopStrip3Editor*> (ed.get());
        juce::MidiBuffer mb; double ph = 0;
        for (int fr = 0; fr < 48; ++fr)
        {
            if (fr == 6) p.setCurrentProgram (2);
            if (fr == 30) p.setCurrentProgram (0);
            const float amp = fr < 10 ? 0.02f : (fr < 28 ? 0.25f : 0.06f);
            juce::AudioBuffer<float> b (2, 1600);
            for (int i = 0; i < 1600; ++i) { const float v = amp * (float) std::sin (ph); ph += TWO_PI * 220.0 / SR; b.setSample (0, i, v); b.setSample (1, i, v); }
            for (int pos = 0; pos < 1600; pos += 400) { juce::AudioBuffer<float> sub (b.getArrayOfWritePointers(), 2, pos, 400); p.processBlock (sub, mb); }
            te->tickNow(); te->tickNow();
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            juce::File f (juce::File (argv[2]).getChildFile (juce::String::formatted ("f%02d.png", fr)));
            f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
        }
        return 0;
    }
    if (argc > 1 && juce::String (argv[1]) == "--alias")
    {
        for (double over : { 0.0, 4.0, 8.0, 13.0, 20.0 })
            for (double f0 : { 3100.0, 7000.0, 11000.0 })
            {
                const double amp = std::pow (10.0, (over + 8.0 - 27.0) / 20.0);     // dB por encima del recorte (+30 dBu)
                const auto o = run1 ({ { "input", 2 }, { "high_gain", 1 }, { "pre_gain", 9 }, { "out", -30 } }, sine (f0, amp));
                const double f1 = level (o, f0); double worst = -200;
                for (double f = 300; f < 20000; f += 50)
                {
                    bool harm = false; for (int k = 1; k < 7; ++k) if (std::abs (f - f0 * k) < 150) harm = true;
                    if (! harm) worst = std::max (worst, level (o, f) - f1);
                }
                std::printf ("+%4.1f dB sobre recorte  %5.0f Hz  aliasing %.1f dB  THD %.1f %%\n", over, f0, worst, thdPct (o, f0));
            }
        return 0;
    }

    std::printf ("== PREAMP (2 x 6922, clase A) ==\n");
    {
        // 1. +4 dBu (0 VU): unidad y valvula limpia
        const auto o = run1 ({}, sine (1000, dbuToAmp (4)));
        const double g = level (o, 1000) - db (dbuToAmp (4)), t = thdPct (o, 1000);
        report ("+4 dBu (0 VU): ganancia unidad, THD baja", std::abs (g) < 0.1 && t > 0.01 && t < 0.05,
                juce::String::formatted ("%+.3f dB  THD %.3f %%", g, t));
    }
    {
        // 2. THD 0.5 % justo antes del recorte (+29 dBu dentro del preamp)
        const auto o = run1 ({ { "pre_gain", 7 }, { "out", -7 } }, sine (1000, dbuToAmp (22)));
        const double t = thdPct (o, 1000);
        report ("THD a +29 dBu (spec 0.5 % nominal)", t > 0.35 && t < 0.7, juce::String::formatted ("%.3f %%", t));
    }
    {
        // 3. 2do armonico dominante (caracter de triodo)
        const auto o = run1 ({ { "pre_gain", 4 } }, sine (1000, dbuToAmp (18)));
        const double h2 = level (o, 2000) - level (o, 1000), h3 = level (o, 3000) - level (o, 1000);
        report ("2do armonico dominante a +22 dBu", h2 > h3 + 3, juce::String::formatted ("H2 %.1f dB  H3 %.1f dB", h2, h3));
    }
    {
        // 4. salida maxima: punto de 1 % THD alrededor de +30 dBu
        double at1 = -1;
        for (double lv = 24; lv <= 36; lv += 0.25)
        {
            const auto o = run1 ({ { "pre_gain", 9 } }, sine (1000, dbuToAmp (lv - 9)), 0.6);
            if (thdPct (o, 1000, 0.3, 0.55) > 1.0) { at1 = level (o, 1000, 0.3, 0.55) + 22; break; }
        }
        report ("Salida maxima (1 % THD) ~ +30 dBu", at1 > 29 && at1 < 32, juce::String::formatted ("%+.1f dBu", at1));
    }
    {
        // 5. ganancia del preamp y HIGH GAIN (+8 LINE, +18 MIC / INSTRUMENT)
        const double gmax = gainAt ({ { "pre_gain", 9 } }, 1000, 0.01), gmin = gainAt ({ { "pre_gain", -28 } }, 1000, 0.3);
        const double hgl = gainAt ({ { "high_gain", 1 } }, 1000, 0.003), hgm = gainAt ({ { "high_gain", 1 }, { "input", 2 } }, 1000, 0.003);
        const double hgi = gainAt ({ { "high_gain", 1 }, { "input", 1 } }, 1000, 0.003);
        report ("PREAMP GAIN -28..+9 / HIGH GAIN 8 y 18 dB",
                std::abs (gmax - 9) < 0.1 && std::abs (gmin + 28) < 0.1 && std::abs (hgl - 8) < 0.1 && std::abs (hgm - 18) < 0.1 && std::abs (hgi - 18) < 0.1,
                juce::String::formatted ("%+.2f / %+.2f / LINE %+.2f  MIC %+.2f  INST %+.2f dB", gmax, gmin, hgl, hgm, hgi));
    }
    {
        // 6. MIC = trafo de entrada (3er armonico en graves a nivel alto), mismo nivel a 1 kHz
        auto h3 = [] (int mode) { const auto o = run1 ({ { "input", (float) mode } }, sine (30, dbuToAmp (20)), 1.6);
                                  return level (o, 90, 0.6, 1.5) - level (o, 30, 0.6, 1.5); };
        const double l = h3 (0), m = h3 (2);
        const double g = gainAt ({ { "input", 2 } }, 1000, dbuToAmp (4));
        report ("MIC: trafo satura graves, 1 kHz igual", m > l + 6 && std::abs (g) < 0.1,
                juce::String::formatted ("H3 a 30 Hz %.1f -> %.1f dB, 1 kHz %+.2f dB", l, m, g));
        // INSTRUMENT (Hi-Z): mas pares
        auto h2 = [] (int mode) { const auto o = run1 ({ { "input", (float) mode } }, sine (1000, dbuToAmp (20)));
                                  return level (o, 2000) - level (o, 1000); };
        const double hl = h2 (0), hi = h2 (1), hh = [] { const auto o = run1 ({ { "high_gain", 1 }, { "pre_gain", -8 } }, sine (1000, dbuToAmp (20)));
                                                         return level (o, 2000) - level (o, 1000); }();
        report ("INSTRUMENT y HIGH GAIN: mas 2do armonico", hi > hl + 1.5 && hh > hl + 3,
                juce::String::formatted ("H2 LINE %.1f  INST %.1f  HIGH GAIN %.1f dB", hl, hi, hh));
    }
    {
        // 7. respuesta plana 10 Hz - 20 kHz (spec 10 Hz - 40 kHz +-0.2 dB)
        double worst = 0; juce::String info;
        for (double f : { 10.0, 20.0, 100.0, 1000.0, 10000.0, 20000.0 })
        {
            const double g = gainAt ({}, f, dbuToAmp (4), f < 50 ? 2.0 : 1.0);
            worst = std::max (worst, std::abs (g)); info << juce::String (f, 0) << " Hz " << juce::String (g, 2) << "  ";
        }
        report ("Respuesta 10 Hz - 20 kHz +-0.2 dB", worst < 0.2, info);
    }

    std::printf ("== PASA ALTOS (6 dB/oct, 30-140 Hz) ==\n");
    {
        const double a = gainAt ({ { "hpf_on", 1 }, { "hpf", 140 } }, 140, 0.05), b = gainAt ({ { "hpf_on", 1 }, { "hpf", 140 } }, 35, 0.05, 2.0);
        const double c = gainAt ({ { "hpf_on", 1 }, { "hpf", 30 } }, 30, 0.05, 2.0);
        report ("HPF 140 Hz: -3 dB en el corte, 6 dB/oct", std::abs (a + 3.01) < 0.2 && std::abs (b + 12.3) < 0.4 && std::abs (c + 3.01) < 0.2,
                juce::String::formatted ("140 Hz %+.2f  35 Hz %+.2f  (30 Hz en 30: %+.2f) dB", a, b, c));
    }

    std::printf ("== EQ (shelves pasivos + medios activos) ==\n");
    {
        struct Case { const char* name; Params prm; double f; };
        const std::vector<Case> cases {
            { "BASS 15 Hz +24", { { "bass_gain", 24 }, { "bass_freq", 0 } }, 15 },
            { "BASS 15 Hz +24 (60 Hz)", { { "bass_gain", 24 }, { "bass_freq", 0 } }, 60 },
            { "BASS 150 Hz -24", { { "bass_gain", -24 }, { "bass_freq", 3 } }, 150 },
            { "BASS 60 Hz +12 (30 Hz)", { { "bass_gain", 12 }, { "bass_freq", 2 } }, 30 },
            { "LOW MID 100 Hz +16 Q0.2", { { "lm_gain", 16 }, { "lm_freq", 4 } }, 100 },
            { "LOW MID 100 Hz +16 Q0.2 (400 Hz)", { { "lm_gain", 16 }, { "lm_freq", 4 } }, 400 },
            { "LOW MID x10 1 kHz -16", { { "lm_gain", -16 }, { "lm_freq", 4 }, { "lm_x10", 1 } }, 1000 },
            { "HIGH MID 2 kHz +16 HI-Q", { { "hm_gain", 16 }, { "hm_freq", 8 }, { "hm_hiq", 1 } }, 2000 },
            { "HIGH MID 2 kHz +16 HI-Q (4 kHz)", { { "hm_gain", 16 }, { "hm_freq", 8 }, { "hm_hiq", 1 } }, 4000 },
            { "HIGH MID x10 12 kHz +10", { { "hm_gain", 10 }, { "hm_freq", 5 }, { "hm_x10", 1 } }, 12000 },
            { "TREBLE 10 kHz +20", { { "tr_gain", 20 }, { "tr_freq", 0 } }, 10000 },
            { "TREBLE 32 kHz +20 (16 kHz)", { { "tr_gain", 20 }, { "tr_freq", 3 } }, 16000 },
            { "TREBLE 15 kHz -20 (5 kHz)", { { "tr_gain", -20 }, { "tr_freq", 1 } }, 5000 },
        };
        for (auto& cs : cases)
        {
            TopStrip3Processor ref; auto prm = cs.prm; prm.push_back ({ "eq_on", 1 }); setParams (ref, prm);
            const double want = ref.eqResponseDb (cs.f);
            const double got = gainAt (prm, cs.f, dbuToAmp (-12), cs.f < 50 ? 2.4 : 1.0);
            report (cs.name, std::abs (got - want) < 0.25, juce::String::formatted ("medido %+.2f dB, circuito %+.2f dB", got, want));
        }
        TopStrip3Processor a; setParams (a, { { "eq_on", 1 }, { "hm_gain", 12 }, { "hm_freq", 8 } });
        TopStrip3Processor b; setParams (b, { { "eq_on", 1 }, { "hm_gain", 12 }, { "hm_freq", 8 }, { "hm_hiq", 1 } });
        report ("HI-Q estrecha la campana (Q 0.2 -> 0.85)", a.eqResponseDb (500) > b.eqResponseDb (500) + 6,
                juce::String::formatted ("a 500 Hz (centro 2 kHz): ancho %+.1f  HI-Q %+.1f dB", a.eqResponseDb (500), b.eqResponseDb (500)));
        const double off = gainAt ({ { "bass_gain", 12 }, { "tr_gain", 12 } }, 60, 0.05);
        report ("EQUALIZER fuera = sin efecto", std::abs (off) < 0.05, juce::String::formatted ("%+.3f dB", off));
    }

    std::printf ("== COMPRESOR OPTICO ==\n");
    {
        const Params c { { "comp_on", 1 }, { "thr", 0 }, { "ratio", 4 }, { "att", 10 }, { "rel", 300 } };
        const double loud = gainAt (c, 1000, dbuToAmp (16), 1.5), soft = gainAt (c, 1000, dbuToAmp (-12), 1.5);
        auto c20 = c; c20[2].second = 20;
        const double lim = gainAt (c20, 1000, dbuToAmp (16), 1.5);
        report ("Umbral 0 VU, 4:1 y 20:1 (12 dB encima)", std::abs (loud + 9) < 1.0 && std::abs (soft) < 0.2 && std::abs (lim + 11.4) < 1.0,
                juce::String::formatted ("4:1 %+.2f dB  20:1 %+.2f dB  bajo umbral %+.2f dB", loud, lim, soft));
    }
    {
        // ataque 2 ms vs 200 ms: reduccion despues de 10 ms
        auto grAfter = [] (float att)
        {
            TopStrip3Processor p;
            const int on = (int) (SR * 0.3);
            auto o = run (p, { { "comp_on", 1 }, { "thr", 0 }, { "ratio", 10 }, { "att", att }, { "rel", 300 } },
                          [on] (int i) { return (float) ((i < on ? dbuToAmp (-12) : dbuToAmp (18)) * std::sin (TWO_PI * 1000 * i / SR)); },
                          [on] (int i) { return (float) ((i < on ? dbuToAmp (-12) : dbuToAmp (18)) * std::sin (TWO_PI * 1000 * i / SR)); }, 0.5);
            const int L = p.getLatencySamples(), a = on + L + (int) (SR * 0.010);
            return db (tone (o.getReadPointer (0) + a, (int) (SR * 0.004), 1000)) - db (dbuToAmp (18));
        };
        const double f = grAfter (2), s = grAfter (200);
        report ("ATTACK 2 ms vs 200 ms (a los 10 ms)", f < -8 && s > -3, juce::String::formatted ("rapido %+.1f dB, lento %+.1f dB", f, s));
    }
    {
        // release dependiente del programa (memoria de la celda optica)
        auto residual = [] (double burst)
        {
            TopStrip3Processor p;
            const int e = (int) (SR * (0.2 + burst));
            auto g = [e] (int i) { return (float) ((i > (int) (SR * 0.2) && i < e ? dbuToAmp (16) : dbuToAmp (-14)) * std::sin (TWO_PI * 1000 * i / SR)); };
            auto o = run (p, { { "comp_on", 1 }, { "thr", 0 }, { "ratio", 4 }, { "att", 10 }, { "rel", 500 } }, g, g, 0.2 + burst + 1.0);
            const int a = e + p.getLatencySamples() + (int) (SR * 0.35);
            return db (tone (o.getReadPointer (0) + a, (int) (SR * 0.05), 1000)) - db (dbuToAmp (-14));
        };
        const double sh = residual (0.15), lg = residual (2.5);
        report ("RELEASE con memoria (frase corta vs larga)", lg < sh - 1.0 && sh > -6, juce::String::formatted ("350 ms despues: corta %+.1f dB, larga %+.1f dB", sh, lg));
    }
    {
        // SC > MIDS: los medios solo en el sidechain = de-esser
        const Params d { { "comp_on", 1 }, { "eq_on", 1 }, { "sc_link", 1 }, { "thr", -6 }, { "ratio", 6 }, { "att", 2 }, { "rel", 150 },
                         { "hm_gain", 12 }, { "hm_freq", 3 }, { "hm_x10", 1 }, { "hm_hiq", 1 } };
        const double s6k = gainAt (d, 5500, dbuToAmp (2)), s200 = gainAt (d, 200, dbuToAmp (2));
        auto lin = d; lin[0].second = 0;
        const double path = gainAt (lin, 5500, dbuToAmp (-20));
        report ("SC > MIDS: de-esser, medios fuera del audio", s6k < s200 - 6 && std::abs (path) < 0.1,
                juce::String::formatted ("5.5 kHz %+.1f dB, 200 Hz %+.1f dB, camino de audio %+.2f dB", s6k, s200, path));
    }
    {
        // EQ > COMP: el EQ empuja al compresor
        const Params base { { "comp_on", 1 }, { "eq_on", 1 }, { "thr", 0 }, { "ratio", 4 }, { "bass_gain", 12 }, { "bass_freq", 3 } };
        auto pre = base; pre.push_back ({ "eq_pre", 1 });
        const double post = gainAt (base, 60, dbuToAmp (0)), prg = gainAt (pre, 60, dbuToAmp (0));
        report ("EQ > COMP: EQ antes del compresor", prg < post - 3, juce::String::formatted ("60 Hz: EQ despues %+.1f dB, EQ antes %+.1f dB", post, prg));
    }
    {
        // sidechain enlazado en estereo
        TopStrip3Processor p;
        auto o = run (p, { { "comp_on", 1 }, { "thr", 0 }, { "ratio", 4 } }, sine (1000, dbuToAmp (16)), sine (700, dbuToAmp (-14)), 1.2);
        const double r = level (o, 700, 0.6, 1.1, 1) - db (dbuToAmp (-14));
        report ("Enlace estereo (L fuerte baja tambien R)", r < -6, juce::String::formatted ("R %+.1f dB", r));
    }

    std::printf ("== V2: CALIBRACION CON LOS IMPULSOS DEL APARATO ==\n");
    {
        // valores derivados de las capturas (dB relativos a 1 kHz, promedio de 1/6 de octava, divididos por la captura LINE plana)
        const double F[] { 20, 30, 50, 100, 200, 500, 2000, 5000, 10000, 15000, 18000 };
        const double ir1[] { -8.98, -6.07, -3.17, -1.07, -0.31, -0.05, -0.01, -0.32, -1.29, -2.52, -3.27 };   // MIC (filtro ~52 Hz)
        const double ir3[] { -8.68, -5.73, -2.82, -0.69, 0.08, 0.31, -2.11, -6.91, -11.35, -14.04, -15.09 }; // MIC + COMP + EQ
        const double ir5[] { -4.05, -3.71, -3.46, -3.23, -2.79, -1.54, 1.40, 2.69, 5.24, 6.63, 6.92 };     // LINE + EQ
        const double ir8[] { -0.08, -0.02, -0.01, 0.02, 0.01, 0.01, -0.03, -0.23, -0.87, -1.77, -2.38 };   // INSTRUMENT HIGH GAIN
        const double ir9[] { -13.07, -9.76, -6.62, -4.25, -3.04, -1.57, 1.40, 2.65, 5.05, 6.19, 6.29 };    // INSTRUMENT + EQ
        // ajustes del panel encontrados por busqueda sobre las capturas
        const Params eqA { { "eq_on", 1 }, { "bass_gain", -8.6f }, { "bass_freq", 0 }, { "tr_gain", 7.7f }, { "tr_freq", 0 },
                           { "lm_gain", -4.2f }, { "lm_freq", 4 }, { "hm_gain", -0.7f }, { "hm_freq", 3 }, { "hm_hiq", 1 } };
        const Params eqB { { "eq_on", 1 }, { "comp_on", 1 }, { "thr", 20 }, { "bass_gain", -0.2f }, { "bass_freq", 0 }, { "tr_gain", -5.6f }, { "tr_freq", 0 },
                           { "lm_gain", 0.5f }, { "lm_freq", 4 }, { "lm_x10", 1 }, { "lm_hiq", 1 }, { "hm_gain", -7.9f }, { "hm_freq", 7 }, { "hm_x10", 1 } };
        auto join = [] (Params a, const Params& b) { a.insert (a.end(), b.begin(), b.end()); return a; };
        const Params hpf52 { { "hpf_on", 1 }, { "hpf", 52.2f } };
        struct Cap { const char* name; Params prm; const double* t; };
        const std::vector<Cap> caps {
            { "Impulso 1: MIC (filtro 52 Hz)", join ({ { "input", 2 } }, hpf52), ir1 },
            { "Impulso 3: MIC + COMP + EQ", join (join ({ { "input", 2 } }, hpf52), eqB), ir3 },
            { "Impulso 5: LINE + EQ", eqA, ir5 },
            { "Impulso 8: INSTRUMENT HIGH GAIN", { { "input", 1 }, { "high_gain", 1 }, { "pre_gain", -18 } }, ir8 },
            { "Impulso 9: INSTRUMENT + EQ (filtro 52 Hz)", join (join ({ { "input", 1 } }, hpf52), eqA), ir9 },
        };
        for (auto& cp : caps)
        {
            const double amp = 0.02;
            const double ref = gainAt (cp.prm, 1000, amp);
            double worst = 0, sq = 0; juce::String info;
            for (int i = 0; i < 11; ++i)
            {
                const double g = gainAt (cp.prm, F[i], amp, F[i] < 60 ? 2.0 : 1.0) - ref;
                const double e = g - cp.t[i];
                worst = std::max (worst, std::abs (e)); sq += e * e;
            }
            info << "error max " << juce::String (worst, 2) << " dB, rms " << juce::String (std::sqrt (sq / 11.0), 2) << " dB";
            report (cp.name, worst < 0.6, info);
        }
    }
    {
        // MIX: la senal directa va alineada con el circuito (sin filtro de peine) y MIX 0 = senal original
        const double a = gainAt ({ { "mix", 50 } }, 18000, 0.05) - gainAt ({ { "mix", 50 } }, 1000, 0.05);
        const Params c { { "comp_on", 1 }, { "thr", -10 }, { "ratio", 10 } };
        auto c0 = c; c0.push_back ({ "mix", 0 });
        auto c50 = c; c50.push_back ({ "mix", 50 });
        const double g0 = gainAt (c0, 1000, dbuToAmp (10), 1.5), g100 = gainAt (c, 1000, dbuToAmp (10), 1.5), g50 = gainAt (c50, 1000, dbuToAmp (10), 1.5);
        report ("MIX en paralelo sin peine / MIX 0 = original", std::abs (a) < 0.05 && std::abs (g0) < 0.05 && g50 > g100 + 3 && g50 < -0.5,
                juce::String::formatted ("50%%: 18 kHz %+.3f dB | comp 100%% %+.1f, 50%% %+.1f, 0%% %+.2f dB", a, g100, g50, g0));
    }

    std::printf ("== V3: DESHACER, A/B, MEDIDORES ==\n");
    {
        TopStrip3Processor p;
        auto set = [&p] (const char* id, float v) { auto* q = p.apvts.getParameter (id); q->beginChangeGesture(); q->setValueNotifyingHost (q->convertTo0to1 (v)); q->endChangeGesture(); };
        auto get = [&p] (const char* id) { return p.apvts.getRawParameterValue (id)->load(); };
        p.apvts.copyState(); p.undoManager.clearUndoHistory();
        p.undoManager.beginNewTransaction ("thr"); set ("thr", -10.0f); p.apvts.copyState();
        p.undoManager.beginNewTransaction ("bass"); set ("bass_gain", 6.0f); p.apvts.copyState();
        const bool step1 = std::abs (get ("thr") + 10.0f) < 0.05f && std::abs (get ("bass_gain") - 6.0f) < 0.05f;
        p.undoManager.undo(); const bool u1 = std::abs (get ("bass_gain")) < 0.05f && std::abs (get ("thr") + 10.0f) < 0.05f;
        p.undoManager.undo(); const bool u2 = std::abs (get ("thr")) < 0.05f;
        p.undoManager.redo(); const bool r1 = std::abs (get ("thr") + 10.0f) < 0.05f;
        report ("Deshacer / rehacer por gesto", step1 && u1 && u2 && r1, juce::String::formatted ("%d %d %d %d", step1, u1, u2, r1));

        TopStrip3Processor q;
        auto setq = [&q] (const char* id, float v) { auto* r = q.apvts.getParameter (id); r->setValueNotifyingHost (r->convertTo0to1 (v)); };
        auto getq = [&q] (const char* id) { return q.apvts.getRawParameterValue (id)->load(); };
        setq ("thr", -10.0f); q.abSelect (1);
        const bool bCopied = std::abs (getq ("thr") + 10.0f) < 0.05f;
        setq ("thr", 5.0f); q.abSelect (0);
        const bool backA = std::abs (getq ("thr") + 10.0f) < 0.05f;
        q.abSelect (1);
        const bool backB = std::abs (getq ("thr") - 5.0f) < 0.05f;
        report ("Comparacion A/B", bCopied && backA && backB && q.abSlot() == 1, juce::String::formatted ("A %.1f / B %.1f", -10.0, 5.0));

        TopStrip3Processor m;
        run (m, {}, sine (1000, 0.1), sine (1000, 0.1), 0.5);
        const float load = m.dspLoad.load();
        report ("Medidor de carga DSP y sobremuestreo", load > 0.0f && load < 5.0f && m.oversamplingFactor() == 8,
                juce::String::formatted ("carga %.1f %%, %dx (%.0f kHz)", load * 100.0f, m.oversamplingFactor(), m.internalRate() / 1000.0));
    }

    std::printf ("== CALIDAD / SISTEMA ==\n");
    {
        // aliasing con recorte fuerte (MIC + HIGH GAIN + GAIN al maximo), de 4 a 20 dB por encima del recorte
        double worst = -200;
        for (double over : { 4.0, 8.0, 13.0, 20.0 })
        {
            const auto o = run1 ({ { "input", 2 }, { "high_gain", 1 }, { "pre_gain", 9 }, { "out", -30 } }, sine (7000, std::pow (10.0, (over - 19.0) / 20.0)));
            const double f1 = level (o, 7000);
            for (double f = 300; f < 20000; f += 100)
            {
                bool harm = false; for (int k = 1; k < 4; ++k) if (std::abs (f - 7000.0 * k) < 250) harm = true;
                if (! harm) worst = std::max (worst, level (o, f) - f1);
            }
        }
        report ("Saturacion 4-20 dB sin aliasing (8x + ADAA)", worst < -85, juce::String::formatted ("peor componente fuera de armonicos %.1f dB", worst));
    }
    {
        TopStrip3Processor p;
        auto o = run (p, { { "power", 0 }, { "pre_gain", 9 }, { "eq_on", 1 }, { "tr_gain", 10 } }, sine (440, 0.3), sine (440, 0.3), 0.7);
        const int L = p.getLatencySamples(); double err = 0;
        for (int i = 20000; i < 30000; ++i) err = std::max (err, std::abs ((double) o.getSample (0, i) - 0.3 * std::sin (TWO_PI * 440 * (i - L) / SR)));
        TopStrip3Processor q;
        auto ph = run (q, { { "phase", 1 } }, sine (440, 0.1), sine (440, 0.1), 0.7);
        double corr = 0; for (int i = 20000; i < 30000; ++i) corr += ph.getSample (0, i) * std::sin (TWO_PI * 440 * (i - L) / SR);
        report ("POWER apagado = original / PHASE REV invierte", err < 1e-4 && corr < 0, juce::String::formatted ("error %.6f, latencia %d", err, L));
    }
    {
        bool ok = true; juce::String info;
        TopStrip3Processor p;
        for (int i = 0; i < p.getNumPrograms(); ++i)
        {
            p.setCurrentProgram (i);
            auto o = run (p, {}, sine (220, 0.35), sine (220, 0.35), 0.6);
            const float pk = o.getMagnitude (0, (int) (SR * 0.2), (int) (SR * 0.4));
            const double pkDb = db (pk);
            if (! std::isfinite (pkDb) || pkDb > -2.0 || pkDb < -16.0) ok = false;
            info << juce::String (pkDb, 1) << " ";
        }
        report ("Presets (pico dBFS)", ok, info);
    }
    {
        TopStrip3Processor p; p.setCurrentProgram (2);
        juce::MemoryBlock mb; p.getStateInformation (mb);
        TopStrip3Processor q; q.setStateInformation (mb.getData(), (int) mb.getSize());
        const bool ok = std::abs (q.apvts.getRawParameterValue ("thr")->load() - p.apvts.getRawParameterValue ("thr")->load()) < 1e-6;
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        report ("Estado y editor", ok && ed != nullptr && ed->getWidth() > 0, juce::String (ed->getWidth()) + "x" + juce::String (ed->getHeight()));
    }

    std::printf (fails == 0 ? "\nTODAS LAS PRUEBAS OK\n" : "\n%d PRUEBAS FALLARON\n", fails);
    return fails == 0 ? 0 : 1;
}
