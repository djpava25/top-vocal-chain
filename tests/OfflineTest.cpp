// Prueba offline: genera una "voz" sintetica desafinada, la pasa por la cadena
// completa y verifica afinacion, estabilidad numerica, niveles y uso de CPU.
#include "PluginProcessor.h"
#include "PluginEditor.h"
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
static int lastLatency = 0;
static juce::AudioBuffer<float> runIsolated (std::vector<std::pair<const char*, float>> params,
                                             std::function<float (int, double)> gen, double seconds, double sr = 48000.0)
{
    TopVocalChainProcessor proc;
    proc.setPlayConfigDetails (2, 2, sr, 512);
    std::vector<std::pair<const char*, float>> base { { "tune_on", 0 }, { "pre_on", 0 }, { "eq_on", 0 }, { "comp_on", 0 },
                                                      { "dly_on", 0 }, { "rev_on", 0 }, { "ai_amount", 0 }, { "wide_on", 0 } };
    for (auto& kv : params) base.push_back (kv);
    for (auto& [id, v] : base) { auto* prm = proc.apvts.getParameter (id); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); }
    proc.prepareToPlay (sr, 512);
    lastLatency = proc.getLatencySamples();
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
    for (int t = 0; t < 8; ++t)
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
    for (int t = 0; t < 7; ++t)
    {
        auto gen = [] (int i, double s) { const double amp = i < s * 0.5 ? 0.0316 : 0.5; return (float) (amp * std::sin (2 * juce::MathConstants<double>::pi * 1000 * i / s)); };
        auto o = runIsolated ({ { "comp_on", 1 }, { "comp_type", (float) t }, { "comp_thr", -24 }, { "comp_ratio", 4 }, { "comp_att", 5 },
                                { "comp_rel", 120 }, { "comp_makeup", 0 } }, gen, 1.5, sr);
        const double lat = 0.5 + lastLatency / sr;   // latencia real del plugin
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


static int testWider()
{
    int f = 0;
    const double sr = 48000.0;
    std::printf ("\n== WIDER (voz mono 220 Hz, cantidad 60%%) ==\n");
    auto voice = [] (int i, double s)
    {
        double v = 0; const double ph = 220.0 * i / s;
        for (int h = 1; h <= 10; ++h) v += std::sin (2 * juce::MathConstants<double>::pi * h * ph) / h;
        return (float) (0.15 * v);
    };
    auto measure = [&] (const juce::AudioBuffer<float>& o, double& sideDb, double& monoDb)
    {
        double mm = 0, ss = 0, in = 0;
        for (int i = 24000; i < 72000; ++i)
        {
            const double l = o.getSample (0, i), r = o.getSample (1, i), x = voice (i, sr);
            mm += 0.25 * (l + r) * (l + r); ss += 0.25 * (l - r) * (l - r); in += x * x;
        }
        sideDb = 10 * std::log10 (ss / (mm + 1e-20) + 1e-20);
        monoDb = 10 * std::log10 (mm / (in + 1e-20));
    };
    {
        double s, m;
        auto o = runIsolated ({ { "wide_on", 1 }, { "wide_mode", 0 }, { "wide_amount", 0 } }, voice, 1.6, sr);
        measure (o, s, m);
        const bool ok = s < -80;
        if (! ok) ++f;
        std::printf ("  %-10s cantidad 0%%   lateral %7.1f dB (debe ser mono)  %s\n", "Wide", s, ok ? "OK" : "FALLO");
    }
    for (int mode = 0; mode < 5; ++mode)
    {
        double s, m;
        auto o = runIsolated ({ { "wide_on", 1 }, { "wide_mode", (float) mode }, { "wide_amount", 60 } }, voice, 1.6, sr);
        measure (o, s, m);
        bool fin = true; for (int i = 0; i < o.getNumSamples(); ++i) fin &= std::isfinite (o.getSample (0, i)) && std::isfinite (o.getSample (1, i));
        const bool ok = fin && s > -25 && s < 0 && m > -3.0 && m < 3.0;
        if (! ok) ++f;
        std::printf ("  %-10s lateral/central %6.1f dB   nivel en mono %+5.1f dB   %s\n",
                     TopVocalChainProcessor::wideModeNames()[mode].toRawUTF8(), s, m, ok ? "OK" : "FALLO");
    }
    return f;
}


// ---------------------------------------------------------------------------
// Fase 1: bypass sin clicks, alineacion, comparar, limitador, A/B, deshacer
static void setP (TopVocalChainProcessor& p, const char* id, float v)
{
    auto* prm = p.apvts.getParameter (id); prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

static int testPhase1()
{
    int f = 0;
    const double sr = 48000.0;
    const int block = 256;
    auto sine = [] (int i) { return (float) (0.25 * std::sin (2 * juce::MathConstants<double>::pi * 440.0 * i / 48000.0)); };
    auto report = [&f] (const char* name, bool ok, const juce::String& info)
    {
        if (! ok) ++f;
        std::printf ("  %-42s %s  %s\n", name, info.toRawUTF8(), ok ? "OK" : "FALLO");
    };
    std::printf ("\n== FASE 1 (flujo de trabajo) ==\n");

    // 1. Encender/apagar cada modulo a mitad de la reproduccion: sin clicks
    for (const char* id : { "pre_on", "eq_on", "comp_on", "wide_on", "dly_on", "rev_on", "tune_on", "global_bypass", "limit_on" })
    {
        TopVocalChainProcessor p;
        p.setPlayConfigDetails (2, 2, sr, block);
        setP (p, "pre_drive", 14); setP (p, "comp_thr", -30); setP (p, "eq_air_gain", 8); setP (p, "wide_amount", 80);
        setP (p, "ai_amount", 0);
        p.prepareToPlay (sr, block);
        const int total = (int) sr;
        juce::AudioBuffer<float> b (2, total);
        for (int i = 0; i < total; ++i) { b.setSample (0, i, sine (i)); b.setSample (1, i, sine (i)); }
        juce::MidiBuffer m;
        const bool initial = p.apvts.getRawParameterValue (id)->load() > 0.5f;
        for (int pos = 0, k = 0; pos < total; pos += block, ++k)
        {
            if (k == 60 || k == 120) setP (p, id, (k == 60) != initial ? 1.0f : 0.0f);
            juce::AudioBuffer<float> sub (b.getArrayOfWritePointers(), 2, pos, std::min (block, total - pos));
            p.processBlock (sub, m);
        }
        // Click = salto brusco: segunda diferencia muy grande respecto a la de la propia senal
        auto maxD2 = [&] (int from, int to)
        {
            float mx = 0;
            for (int i = from + 2; i < to; ++i)
                mx = std::max (mx, std::abs (b.getSample (0, i) - 2 * b.getSample (0, i - 1) + b.getSample (0, i - 2)));
            return mx;
        };
        const float steady = maxD2 (block * 40, block * 58);
        const float around = std::max (maxD2 (block * 59, block * 64), maxD2 (block * 119, block * 124));
        report ((juce::String ("Sin click al cambiar ") + id).toRawUTF8(), around < std::max (0.02f, steady * 4.0f),
                juce::String::formatted ("salto %.4f (senal normal %.4f)", around, steady));
    }

    // 2. Bypass global y Dry/Wet 0%: salida identica a la entrada (alineada con la latencia)
    for (const char* mode : { "bypass", "drywet0" })
    {
        TopVocalChainProcessor p;
        p.setPlayConfigDetails (2, 2, sr, block);
        if (juce::String (mode) == "bypass") setP (p, "global_bypass", 1); else setP (p, "global_mix", 0);
        p.prepareToPlay (sr, block);
        const int total = (int) (sr * 0.5), lat = p.getLatencySamples();
        juce::AudioBuffer<float> b (2, total);
        for (int i = 0; i < total; ++i) { b.setSample (0, i, sine (i)); b.setSample (1, i, sine (i)); }
        juce::MidiBuffer m;
        for (int pos = 0; pos < total; pos += block)
        {
            juce::AudioBuffer<float> sub (b.getArrayOfWritePointers(), 2, pos, std::min (block, total - pos));
            p.processBlock (sub, m);
        }
        double err = 0, sig = 0;
        for (int i = lat + 1000; i < total; ++i) { const double d = b.getSample (0, i) - sine (i - lat); err += d * d; sig += sine (i) * sine (i); }
        const double nullDb = 10 * std::log10 (err / sig + 1e-30);
        report (juce::String (mode) == "bypass" ? "Bypass global = original alineado" : "Dry/Wet 0% = original alineado",
                nullDb < -90, juce::String::formatted ("diferencia %.1f dB  (latencia %d)", nullDb, lat));
    }

    // 3. Comparar: el original suena al mismo volumen que el procesado
    {
        TopVocalChainProcessor p;
        p.setPlayConfigDetails (2, 2, sr, block);
        setP (p, "comp_makeup", 12); setP (p, "out_gain", 4);
        p.prepareToPlay (sr, block);
        const int total = (int) (sr * 6);
        juce::AudioBuffer<float> b (2, total);
        for (int i = 0; i < total; ++i) { b.setSample (0, i, sine (i) * 0.5f); b.setSample (1, i, sine (i) * 0.5f); }
        juce::MidiBuffer m;
        for (int pos = 0, k = 0; pos < total; pos += block, ++k)
        {
            if (pos >= (int) (sr * 4)) p.compareActive.store (true);
            juce::AudioBuffer<float> sub (b.getArrayOfWritePointers(), 2, pos, std::min (block, total - pos));
            p.processBlock (sub, m);
        }
        auto both = [&] (int start, int len) { const double a = b.getRMSLevel (0, start, len), c = b.getRMSLevel (1, start, len); return 10 * std::log10 (0.5 * (a * a + c * c)); };
        const double proc = both ((int) (sr * 3), (int) sr);
        const double cmp  = both ((int) (sr * 5), (int) sr - 1);
        const double orig = 20 * std::log10 (0.125 / std::sqrt (2.0));
        report ("Comparar con volumen igualado", std::abs (proc - cmp) < 1.0,
                juce::String::formatted ("procesado %.1f dB / original igualado %.1f dB (original real %.1f)", proc, cmp, orig));
    }

    // 4. Limitador: techo -0.3 dBFS
    {
        TopVocalChainProcessor p;
        p.setPlayConfigDetails (2, 2, sr, block);
        setP (p, "limit_on", 1); setP (p, "out_gain", 12); setP (p, "comp_on", 0);
        p.prepareToPlay (sr, block);
        const int total = (int) sr;
        juce::AudioBuffer<float> b (2, total);
        for (int i = 0; i < total; ++i) { b.setSample (0, i, sine (i) * 2.5f); b.setSample (1, i, sine (i) * 2.5f); }
        juce::MidiBuffer m;
        for (int pos = 0; pos < total; pos += block)
        {
            juce::AudioBuffer<float> sub (b.getArrayOfWritePointers(), 2, pos, std::min (block, total - pos));
            p.processBlock (sub, m);
        }
        const float pk = b.getMagnitude (0, total / 2, total / 2);
        report ("Limitador -0.3 dBFS", pk <= 0.968f, juce::String::formatted ("pico %.2f dBFS", 20 * std::log10 (pk)));
    }

    // 5. A/B, guardar/cargar y deshacer
    {
        TopVocalChainProcessor p;
        setP (p, "comp_thr", -20);
        p.switchToSlot (1);                       // B empieza como copia de A
        setP (p, "comp_thr", -35);
        p.switchToSlot (0);
        const float a = p.apvts.getRawParameterValue ("comp_thr")->load();
        p.switchToSlot (1);
        const float bb = p.apvts.getRawParameterValue ("comp_thr")->load();
        juce::MemoryBlock mb; p.getStateInformation (mb);
        TopVocalChainProcessor q; q.setStateInformation (mb.getData(), (int) mb.getSize());
        const float qb = q.apvts.getRawParameterValue ("comp_thr")->load();
        q.switchToSlot (0);
        const float qa = q.apvts.getRawParameterValue ("comp_thr")->load();
        report ("A/B + guardar/cargar proyecto", std::abs (a + 20) < 0.01f && std::abs (bb + 35) < 0.01f && std::abs (qb + 35) < 0.01f && std::abs (qa + 20) < 0.01f,
                juce::String::formatted ("A %.0f  B %.0f  | tras recargar: B %.0f  A %.0f", a, bb, qb, qa));
    }
    {
        TopVocalChainProcessor p;
        p.apvts.copyState(); p.undoManager.beginNewTransaction();
        setP (p, "eq_mid_gain", 7);
        p.apvts.copyState();                      // fuerza el volcado del parametro al arbol (con historial)
        p.undoManager.beginNewTransaction();
        const float before = p.apvts.getRawParameterValue ("eq_mid_gain")->load();
        p.undoManager.undo();
        const float after = p.apvts.getRawParameterValue ("eq_mid_gain")->load();
        p.undoManager.redo();
        const float redo = p.apvts.getRawParameterValue ("eq_mid_gain")->load();
        report ("Deshacer / rehacer", std::abs (before - 7) < 0.01f && std::abs (after) < 0.01f && std::abs (redo - 7) < 0.01f,
                juce::String::formatted ("%.1f -> deshacer %.1f -> rehacer %.1f", before, after, redo));
    }
    return f;
}


// ---------------------------------------------------------------------------
// Fase 2: autotune pro, compresor completo, EQ con pendientes
static float pitchAt (const juce::AudioBuffer<float>& b, double t, double sr, double len = 0.1)
{
    return estimatePitch (b.getReadPointer (0) + (int) (t * sr), (int) (len * sr), sr);
}

static juce::AudioBuffer<float> runVoice (TopVocalChainProcessor& p, std::vector<std::pair<const char*, float>> params,
                                          std::function<double (double)> freqAt, double secs, double sr = 48000.0)
{
    p.setPlayConfigDetails (2, 2, sr, 256);
    std::vector<std::pair<const char*, float>> base { { "pre_on", 0 }, { "eq_on", 0 }, { "comp_on", 0 }, { "dly_on", 0 },
                                                      { "rev_on", 0 }, { "ai_amount", 0 }, { "wide_on", 0 } };
    for (auto& kv : params) base.push_back (kv);
    for (auto& [id, v] : base) setP (p, id, v);
    p.prepareToPlay (sr, 256);
    const int total = (int) (sr * secs);
    juce::AudioBuffer<float> b (2, total);
    double ph = 0;
    for (int i = 0; i < total; ++i)
    {
        ph += freqAt (i / sr) / sr;
        double v = 0;
        for (int h = 1; h <= 12; ++h)
        {
            const double fh = freqAt (i / sr) * h;
            const double env = std::exp (-std::pow ((fh - 700) / 400, 2)) + 0.6 * std::exp (-std::pow ((fh - 1200) / 500, 2)) + 0.05;
            v += env * std::sin (2 * juce::MathConstants<double>::pi * h * ph);
        }
        b.setSample (0, i, (float) (0.2 * v)); b.setSample (1, i, (float) (0.2 * v));
    }
    juce::MidiBuffer m;
    for (int pos = 0; pos < total; pos += 256)
    {
        juce::AudioBuffer<float> sub (b.getArrayOfWritePointers(), 2, pos, std::min (256, total - pos));
        p.processBlock (sub, m);
    }
    return b;
}

static int testPhase2()
{
    int f = 0;
    const double sr = 48000.0;
    auto report = [&f] (const char* name, bool ok, const juce::String& info)
    {
        if (! ok) ++f;
        std::printf ("  %-44s %s  %s\n", name, info.toRawUTF8(), ok ? "OK" : "FALLO");
    };
    auto cents = [] (float a, float b) { return a > 0 && b > 0 ? 1200.0f * std::log2 (a / b) : 9999.0f; };
    const double a3 = 220.0, sharp40 = 220.0 * std::pow (2.0, 0.40 / 12.0);
    std::printf ("\n== FASE 2: AUTOTUNE ==\n");

    { TopVocalChainProcessor p; auto o = runVoice (p, { { "tune_mode", 1 } }, [=] (double) { return sharp40; }, 2.0);
      const float c = cents (pitchAt (o, 1.5, sr), (float) a3);
      report ("Robotic (A3 +40c -> A3)", std::abs (c) < 8, juce::String::formatted ("%+.1f cents", c)); }

    { TopVocalChainProcessor p0, p1;
      auto o0 = runVoice (p0, { { "tune_flex", 0 } }, [=] (double) { return sharp40; }, 2.0);
      auto o1 = runVoice (p1, { { "tune_flex", 100 } }, [=] (double) { return sharp40; }, 2.0);
      const float c0 = cents (pitchAt (o0, 1.5, sr), (float) a3), c1 = cents (pitchAt (o1, 1.5, sr), (float) a3);
      report ("Flex-Tune deja natural lo lejano", std::abs (c0) < 8 && c1 > 25, juce::String::formatted ("flex 0: %+.1f c   flex 100: %+.1f c", c0, c1)); }

    { TopVocalChainProcessor p; auto o = runVoice (p, { { "tune_amount", 0 }, { "tune_transpose", 12 } }, [=] (double) { return a3; }, 2.0);
      const float c = cents (pitchAt (o, 1.5, sr), 440.0f);
      report ("Transpose +12", std::abs (c) < 8, juce::String::formatted ("%.1f Hz (%+.1f c de 440)", pitchAt (o, 1.5, sr), c)); }

    { TopVocalChainProcessor p0, p1;
      auto o0 = runVoice (p0, { { "tune_amount", 0 } }, [=] (double) { return a3; }, 1.6);
      auto o1 = runVoice (p1, { { "tune_amount", 0 }, { "tune_formant", 4 } }, [=] (double) { return a3; }, 1.6);
      auto hf = [&] (const juce::AudioBuffer<float>& b)
      {
          tvc::Biquad hp; hp.setHighPass (sr, 1500.0, 0.707); double e = 0, eh = 0;
          for (int i = 48000; i < 72000; ++i) { const float x = b.getSample (0, i), y = hp.process (x); e += x * x; eh += y * y; }
          return eh / (e + 1e-12);
      };
      const float c = cents (pitchAt (o1, 1.2, sr), (float) a3);
      report ("Formant +4: color cambia, nota no", std::abs (c) < 8 && hf (o1) > hf (o0) * 1.3,
              juce::String::formatted ("nota %+.1f c, agudos x%.2f", c, hf (o1) / hf (o0))); }

    { TopVocalChainProcessor p; auto o = runVoice (p, { { "tune_vibrato", 60 }, { "tune_speed", 0 } }, [=] (double) { return a3; }, 3.0);
      double s = 0, s2 = 0; int k = 0;
      for (double tt = 1.5; tt < 2.8; tt += 0.02) { const float c = cents (pitchAt (o, tt, sr, 0.04), (float) a3); if (std::abs (c) < 200) { s += c; s2 += c * c; ++k; } }
      const double sd = std::sqrt (std::max (0.0, s2 / k - (s / k) * (s / k)));
      report ("Vibrato 60 cents", sd > 15 && sd < 60, juce::String::formatted ("oscilacion +-%.0f cents", sd * 1.414)); }

    { TopVocalChainProcessor p0, p1;
      auto drift = [=] (double t) { return 220.0 * std::pow (2.0, (0.35 - 0.35 * t) / 12.0); };   // +35c -> -35c en 2 s
      auto o0 = runVoice (p0, { { "tune_speed", 20 }, { "tune_humanize", 0 } }, drift, 2.2);
      auto o1 = runVoice (p1, { { "tune_speed", 20 }, { "tune_humanize", 100 } }, drift, 2.2);
      auto spread = [&] (const juce::AudioBuffer<float>& b)
      {
          double mx = 0;
          for (double tt = 0.8; tt < 2.0; tt += 0.05) mx = std::max (mx, (double) std::abs (cents (pitchAt (b, tt, sr, 0.04), (float) a3)));
          return mx;
      };
      const double d0 = spread (o0), d1 = spread (o1);
      report ("Humanize: notas largas mas naturales", d1 > d0 + 3, juce::String::formatted ("desvio max: humanize 0 = %.0f c, 100 = %.0f c", d0, d1)); }

    { TopVocalChainProcessor p; auto o = runVoice (p, { { "tune_lowlat", 1 } }, [=] (double) { return sharp40; }, 2.0);
      const float c = cents (pitchAt (o, 1.5, sr), (float) a3);
      report ("Baja latencia", std::abs (c) < 10 && p.getLatencySamples() < 800,
              juce::String::formatted ("%+.1f c, latencia %.1f ms", c, 1000.0 * p.getLatencySamples() / sr)); }

    { TopVocalChainProcessor p; p.startKeyDetection();
      std::vector<int> mel { 57, 60, 64, 62, 60, 59, 57, 64, 65, 64, 62, 57 };
      auto hzAt = [&] (double t) { const int idx = (int) (t / 0.3) % (int) mel.size(); return 440.0 * std::pow (2.0, (mel[(size_t) idx] - 69) / 12.0); };
      runVoice (p, { { "tune_amount", 0 } }, hzAt, 10.0);
      float h[12]; for (int i = 0; i < 12; ++i) h[i] = p.meters.keyHist[i].load();
      int root; bool minor; float conf; toptune::KeyFinder::find (h, root, minor, conf);
      report ("Auto key (melodia en A menor)", root == 9 && minor, juce::String::formatted ("%s %s (%.0f%%)", TopVocalChainProcessor::keyNames()[root].toRawUTF8(), minor ? "menor" : "mayor", conf * 100)); }

    std::printf ("\n== FASE 2: COMPRESOR ==\n");
    auto sine = [] (double hz, double amp) { return [hz, amp] (int i, double s) { return (float) (amp * std::sin (2 * juce::MathConstants<double>::pi * hz * i / s)); }; };
    {
        auto lvl = [&] (const juce::AudioBuffer<float>& o) { return 20 * std::log10 (o.getMagnitude (0, 48000, 24000)); };
        auto o0 = runIsolated ({ { "comp_on", 1 }, { "comp_thr", -24 }, { "comp_ratio", 8 }, { "comp_makeup", 0 }, { "comp_mix", 0 } }, sine (1000, 0.5), 1.6, sr);
        auto o5 = runIsolated ({ { "comp_on", 1 }, { "comp_thr", -24 }, { "comp_ratio", 8 }, { "comp_makeup", 0 }, { "comp_mix", 50 } }, sine (1000, 0.5), 1.6, sr);
        auto o1 = runIsolated ({ { "comp_on", 1 }, { "comp_thr", -24 }, { "comp_ratio", 8 }, { "comp_makeup", 0 }, { "comp_mix", 100 } }, sine (1000, 0.5), 1.6, sr);
        report ("Mix paralelo 0 / 50 / 100%", std::abs (lvl (o0) + 6.0) < 0.3 && lvl (o5) > lvl (o1) + 3,
                juce::String::formatted ("%.1f / %.1f / %.1f dBFS", lvl (o0), lvl (o5), lvl (o1)));
    }
    {
        TopVocalChainProcessor p;
        setP (p, "comp_thr", -20); setP (p, "comp_ratio", 4);
        setP (p, "comp_knee", 0);  p.getCompStaticGr (-20); 
        auto grAt = [&] (float knee) { TopVocalChainProcessor q; setP (q, "comp_thr", -20); setP (q, "comp_ratio", 4); setP (q, "comp_knee", knee);
                                        q.setPlayConfigDetails (2, 2, sr, 256); q.prepareToPlay (sr, 256);
                                        juce::AudioBuffer<float> b (2, 256); b.clear(); juce::MidiBuffer m; q.processBlock (b, m);
                                        return q.getCompStaticGr (-20.0f); };
        const float hard = grAt (0), soft = grAt (12);
        report ("Knee (en el threshold)", std::abs (hard) < 0.05f && soft < -1.0f, juce::String::formatted ("knee 0: %.2f dB   knee 12: %.2f dB", hard, soft));
    }
    {
        auto grWith = [&] (float hpf)
        {
            auto o = runIsolated ({ { "comp_on", 1 }, { "comp_thr", -30 }, { "comp_ratio", 6 }, { "comp_makeup", 0 }, { "comp_sc_hpf", hpf } },
                                  sine (60, 0.5), 1.0, sr);
            return (float) (20 * std::log10 (o.getMagnitude (0, 36000, 9600) / 0.5));
        };
        const float g20 = grWith (20), g400 = grWith (400);
        report ("Detector HPF ignora graves (60 Hz)", g400 > g20 + 6, juce::String::formatted ("nivel con HPF off %.1f dB, HPF 400 Hz %.1f dB", g20, g400));
    }
    {
        TopVocalChainProcessor p;
        auto layout = p.getBusesLayout();
        layout.inputBuses.getReference (1) = juce::AudioChannelSet::stereo();
        const bool okLayout = p.setBusesLayout (layout);
        for (auto [id, v] : std::initializer_list<std::pair<const char*, float>> { { "comp_sc_ext", 1 }, { "comp_thr", -30 }, { "comp_ratio", 8 },
             { "tune_on", 0 }, { "pre_on", 0 }, { "eq_on", 0 }, { "dly_on", 0 }, { "rev_on", 0 }, { "wide_on", 0 }, { "ai_amount", 0 } })
            setP (p, id, v);
        p.prepareToPlay (sr, 256);
        juce::AudioBuffer<float> b (4, 256); juce::MidiBuffer m;
        float gr = 0;
        for (int k = 0; k < 200; ++k)
        {
            for (int i = 0; i < 256; ++i)
            {
                const float main = 0.01f * (float) std::sin (2 * juce::MathConstants<double>::pi * 300 * (k * 256 + i) / sr);
                const float sc = 0.7f * (float) std::sin (2 * juce::MathConstants<double>::pi * 80 * (k * 256 + i) / sr);
                b.setSample (0, i, main); b.setSample (1, i, main); b.setSample (2, i, sc); b.setSample (3, i, sc);
            }
            p.processBlock (b, m);
            gr = p.meters.compGrDb.load();
        }
        report ("Sidechain externo (voz baja, SC fuerte)", okLayout && gr < -10, juce::String::formatted ("GR %.1f dB", gr));
    }

    std::printf ("\n== FASE 2: EQ ==\n");
    {
        juce::String info; bool ok = true; float prev = 0.0f;
        for (int s = 0; s < 4; ++s)
        {
            TopVocalChainProcessor p;
            setP (p, "eq_lowcut", 200); setP (p, "eq_hp_slope", (float) s); setP (p, "eq_pres_gain", 0); setP (p, "eq_air_gain", 0);
            setP (p, "ai_amount", 0);
            p.setPlayConfigDetails (2, 2, sr, 256); p.prepareToPlay (sr, 256);
            juce::AudioBuffer<float> b (2, 256); b.clear(); juce::MidiBuffer m; p.processBlock (b, m);
            const float at = p.getEqMagnitudeDb (50.0);   // 2 octavas debajo
            info << TopVocalChainProcessor::slopeNames()[s] << ": " << juce::String (at, 1) << " dB  ";
            if (s > 0) ok &= at < prev - 7.0f;
            prev = at;
        }
        report ("Low Cut 200 Hz, nivel a 50 Hz", ok, info);
    }
    {
        TopVocalChainProcessor p;
        setP (p, "eq_hicut", 5000); setP (p, "eq_lp_slope", 3); setP (p, "eq_pres_gain", 0); setP (p, "eq_air_gain", 0); setP (p, "ai_amount", 0);
        p.setPlayConfigDetails (2, 2, sr, 256); p.prepareToPlay (sr, 256);
        juce::AudioBuffer<float> b (2, 256); b.clear(); juce::MidiBuffer m; p.processBlock (b, m);
        const float at1k = p.getEqMagnitudeDb (1000.0), at10k = p.getEqMagnitudeDb (10000.0);
        report ("High Cut 5 kHz 24 dB/oct", std::abs (at1k) < 1.0f && at10k < -20.0f, juce::String::formatted ("1 kHz %.1f dB, 10 kHz %.1f dB", at1k, at10k));
    }
    return f;
}


// ---------------------------------------------------------------------------
// Fase 3: Stereo Lab, delay, reverb, medidores, analisis de voz
static juce::AudioBuffer<float> runStereo (TopVocalChainProcessor& p, std::vector<std::pair<const char*, float>> params,
                                           std::function<void (int, float&, float&)> gen, double secs, double sr = 48000.0)
{
    p.setPlayConfigDetails (2, 2, sr, 256);
    std::vector<std::pair<const char*, float>> base { { "tune_on", 0 }, { "pre_on", 0 }, { "eq_on", 0 }, { "comp_on", 0 }, { "dly_on", 0 },
                                                      { "rev_on", 0 }, { "ai_amount", 0 }, { "wide_on", 0 } };
    for (auto& kv : params) base.push_back (kv);
    for (auto& [id, v] : base) setP (p, id, v);
    p.prepareToPlay (sr, 256);
    const int total = (int) (sr * secs);
    juce::AudioBuffer<float> b (2, total);
    for (int i = 0; i < total; ++i) { float l, r; gen (i, l, r); b.setSample (0, i, l); b.setSample (1, i, r); }
    juce::MidiBuffer m;
    for (int pos = 0; pos < total; pos += 256)
    {
        juce::AudioBuffer<float> sub (b.getArrayOfWritePointers(), 2, pos, std::min (256, total - pos));
        p.processBlock (sub, m);
    }
    return b;
}

static int testPhase3()
{
    int f = 0;
    const double sr = 48000.0;
    const double twoPi = 2 * juce::MathConstants<double>::pi;
    auto report = [&f] (const char* name, bool ok, const juce::String& info)
    {
        if (! ok) ++f;
        std::printf ("  %-44s %s  %s\n", name, info.toRawUTF8(), ok ? "OK" : "FALLO");
    };
    auto sideDb = [&] (const juce::AudioBuffer<float>& b, int from, int len)
    {
        double mm = 0, ss = 0;
        for (int i = from; i < from + len; ++i) { const double l = b.getSample (0, i), r = b.getSample (1, i); mm += 0.25 * (l + r) * (l + r); ss += 0.25 * (l - r) * (l - r); }
        return 10 * std::log10 (ss / (mm + 1e-20) + 1e-20);
    };
    // Entrada estereo: medio a 300 Hz + lateral a 2 kHz y a 80 Hz
    auto stereoGen = [=] (int i, float& l, float& r)
    {
        const double t = i / sr;
        const double m = 0.2 * std::sin (twoPi * 300 * t), s = 0.06 * std::sin (twoPi * 2000 * t) + 0.06 * std::sin (twoPi * 80 * t);
        l = (float) (m + s); r = (float) (m - s);
    };

    std::printf ("\n== FASE 3: STEREO LAB ==\n");
    {
        TopVocalChainProcessor p0, pw0, pw2, pmono;
        auto base = runStereo (p0, {}, stereoGen, 1.0);
        auto w0 = runStereo (pw0, { { "wide_on", 1 }, { "wide_amount", 0 }, { "wide_width", 0 }, { "wide_safe", 0 } }, stereoGen, 1.0);
        auto w2 = runStereo (pw2, { { "wide_on", 1 }, { "wide_amount", 0 }, { "wide_width", 200 }, { "wide_safe", 0 } }, stereoGen, 1.0);
        auto mo = runStereo (pmono, { { "wide_on", 1 }, { "wide_amount", 0 }, { "wide_mono", 1 } }, stereoGen, 1.0);
        const double b = sideDb (base, 24000, 19200), a0 = sideDb (w0, 24000, 19200), a2 = sideDb (w2, 24000, 19200), am = sideDb (mo, 24000, 19200);
        report ("Width 0% / 200% / MONO", a0 < -80 && std::abs (a2 - b - 6.0) < 0.6 && am < -80,
                juce::String::formatted ("lateral: normal %.1f, 0%% %.0f, 200%% %.1f (+%.1f dB), mono %.0f dB", b, a0, a2, a2 - b, am));
    }
    {
        TopVocalChainProcessor ps, pu;
        auto sideAt = [&] (const juce::AudioBuffer<float>& b, double hz)
        {
            double re = 0, im = 0;
            for (int i = 24000; i < 43200; ++i) { const double sd = 0.5 * (b.getSample (0, i) - b.getSample (1, i)); re += sd * std::cos (twoPi * hz * i / sr); im += sd * std::sin (twoPi * hz * i / sr); }
            return 20 * std::log10 (std::sqrt (re * re + im * im) / 19200 * 2 + 1e-12);
        };
        auto safe = runStereo (ps, { { "wide_on", 1 }, { "wide_amount", 0 }, { "wide_safe", 1 } }, stereoGen, 1.0);
        auto uns  = runStereo (pu, { { "wide_on", 1 }, { "wide_amount", 0 }, { "wide_safe", 0 } }, stereoGen, 1.0);
        const double l80s = sideAt (safe, 80), l80u = sideAt (uns, 80), h2ks = sideAt (safe, 2000), h2ku = sideAt (uns, 2000);
        report ("Modo seguro: graves en mono, agudos abiertos", l80s < l80u - 10 && std::abs (h2ks - h2ku) < 1.0,
                juce::String::formatted ("lateral 80 Hz %.1f -> %.1f dB, 2 kHz %.1f -> %.1f dB", l80u, l80s, h2ku, h2ks));
    }
    {
        // Senal casi en contrafase (L ~ -R): SAFE VOCAL debe cerrar el ancho solo hasta que la fase este bien
        auto antiGen = [=] (int i, float& l, float& r)
        {
            const double t = i / sr;
            const double m = 0.03 * std::sin (twoPi * 300 * t), s = 0.2 * std::sin (twoPi * 1000 * t);
            l = (float) (m + s); r = (float) (m - s);
        };
        auto corrOf = [] (const juce::AudioBuffer<float>& b)
        {
            double ll = 0, rr = 0, lr = 0;
            for (int i = b.getNumSamples() - 24000; i < b.getNumSamples(); ++i)
            { const double l = b.getSample (0, i), r = b.getSample (1, i); ll += l * l; rr += r * r; lr += l * r; }
            return lr / std::sqrt (ll * rr + 1e-20);
        };
        TopVocalChainProcessor ps, pu;
        auto s = runStereo (ps, { { "wide_on", 1 }, { "wide_amount", 0 }, { "wide_safe", 1 } }, antiGen, 2.0);
        auto u = runStereo (pu, { { "wide_on", 1 }, { "wide_amount", 0 }, { "wide_safe", 0 } }, antiGen, 2.0);
        const double cs = corrOf (s), cu = corrOf (u);
        report ("SAFE VOCAL corrige la fase solo", cu < 0 && cs > 0.05,
                juce::String::formatted ("correlacion sin safe %+.2f, con safe %+.2f (ancho %.0f%%)", cu, cs, ps.meters.phaseGuard.load() * 100));
    }
    {
        TopVocalChainProcessor p0, pm;
        auto off = runStereo (p0, { { "wide_on", 0 } }, stereoGen, 1.0);
        auto mix0 = runStereo (pm, { { "wide_on", 1 }, { "wide_mix", 0 }, { "wide_width", 200 }, { "wide_amount", 100 } }, stereoGen, 1.0);
        double e = 0, s = 0;
        for (int i = 24000; i < 43200; ++i) { const double d = off.getSample (0, i) - mix0.getSample (0, i); e += d * d; s += off.getSample (0, i) * off.getSample (0, i); }
        report ("Mix 0% = sin efecto", 10 * std::log10 (e / s + 1e-30) < -90, juce::String::formatted ("diferencia %.0f dB", 10 * std::log10 (e / s + 1e-30)));
    }
    {
        TopVocalChainProcessor pa, pb;
        runStereo (pa, {}, [=] (int i, float& l, float& r) { l = r = (float) (0.2 * std::sin (twoPi * 440 * i / sr)); }, 0.5);
        runStereo (pb, {}, [=] (int i, float& l, float& r) { l = (float) (0.2 * std::sin (twoPi * 440 * i / sr)); r = -l; }, 0.5);
        const float ca = pa.meters.outCorr.load(), cb = pb.meters.outCorr.load();
        report ("Correlacion: mono = +1, fase invertida = -1", ca > 0.98f && cb < -0.98f, juce::String::formatted ("%.2f / %.2f", ca, cb));
    }

    std::printf ("\n== FASE 3: DELAY ==\n");
    {
        auto impulse = [=] (int i, float& l, float& r) { l = r = (i == 4800) ? 0.8f : 0.0f; };
        TopVocalChainProcessor p;
        auto o = runStereo (p, { { "dly_on", 1 }, { "dly_sync", 1 }, { "dly_div", 8 }, { "dly_mix", 100 }, { "dly_fb", 0 }, { "dly_locut", 20 } }, impulse, 1.0);
        int pk = 0; float mx = 0;
        const int lat = p.getLatencySamples();
        for (int i = 4800 + lat + 100; i < 48000; ++i) if (std::abs (o.getSample (0, i)) > mx) { mx = std::abs (o.getSample (0, i)); pk = i; }
        const double ms = 1000.0 * (pk - 4800 - lat) / sr;
        report ("Division 1/16 D a 120 BPM = 187.5 ms", std::abs (ms - 187.5) < 2.0, juce::String::formatted ("eco a %.1f ms", ms));
    }
    {
        auto tone = [=] (int i, float& l, float& r) { l = r = (i < 9600) ? (float) (0.3 * std::sin (twoPi * 100 * i / sr)) : 0.0f; };
        auto echoLvl = [&] (float lc)
        {
            TopVocalChainProcessor p;
            auto o = runStereo (p, { { "dly_on", 1 }, { "dly_sync", 0 }, { "dly_time", 400 }, { "dly_mix", 100 }, { "dly_fb", 0 }, { "dly_locut", lc } }, tone, 1.0);
            const int lat = p.getLatencySamples();
            return 20 * std::log10 (o.getMagnitude (0, lat + (int) (0.45 * sr), (int) (0.1 * sr)) + 1e-9);
        };
        const double a = echoLvl (20), b = echoLvl (600);
        report ("Lo Cut del eco (100 Hz)", b < a - 12, juce::String::formatted ("eco %.1f dB -> con Lo Cut 600 Hz %.1f dB", a, b));
    }
    {
        auto voice = [=] (int i, float& l, float& r) { l = r = (float) (0.3 * std::sin (twoPi * 220 * i / sr)); };
        auto wetLvl = [&] (float duck)
        {
            TopVocalChainProcessor p0, p1;
            auto dry = runStereo (p0, { { "dly_on", 0 } }, voice, 1.0);
            auto wet = runStereo (p1, { { "dly_on", 1 }, { "dly_sync", 0 }, { "dly_time", 333 }, { "dly_mix", 60 }, { "dly_fb", 30 }, { "dly_duck", duck } }, voice, 1.0);
            double e = 0; for (int i = 36000; i < 46000; ++i) { const double d = wet.getSample (0, i) - dry.getSample (0, i); e += d * d; }
            return 10 * std::log10 (e / 10000 + 1e-20);
        };
        const double a = wetLvl (0), b = wetLvl (100);
        report ("Ducking: el eco baja mientras cantas", b < a - 10, juce::String::formatted ("eco %.1f dB -> con ducking %.1f dB", a, b));
    }

    std::printf ("\n== FASE 3: REVERB ==\n");
    for (int type = 0; type < 5; ++type)
    {
        juce::String info; bool ok = true;
        for (float decay : { 1.0f, 3.0f })
        {
            TopVocalChainProcessor p;
            auto o = runStereo (p, { { "rev_on", 1 }, { "rev_type", (float) type }, { "rev_decay", decay }, { "rev_mix", 100 }, { "rev_predelay", 0 },
                                     { "rev_locut", 20 }, { "rev_hicut", 20000 }, { "rev_er", 0 } },
                                [=] (int i, float& l, float& r) { l = r = (i < 480) ? (float) (0.5 * std::sin (twoPi * 1000 * i / sr)) : 0.0f; }, 6.0);
            const int lat = p.getLatencySamples();
            // T60 a partir de la caida de energia entre -5 y -25 dB (x3)
            auto lvl = [&] (double t) { return 10 * std::log10 (std::pow (o.getRMSLevel (0, lat + (int) (t * sr), (int) (0.05 * sr)), 2) + 1e-20); };
            const double peak = lvl (0.05);
            double t5 = -1, t25 = -1;
            for (double t = 0.05; t < 5.8; t += 0.01)
            {
                const double d = lvl (t) - peak;
                if (t5 < 0 && d < -5) t5 = t;
                if (t25 < 0 && d < -25) { t25 = t; break; }
            }
            const double t60 = t25 > 0 ? (t25 - t5) * 3.0 : 99.0;
            bool fin = true; for (int i = 0; i < o.getNumSamples(); ++i) fin &= std::isfinite (o.getSample (0, i));
            ok &= fin && t60 > decay * 0.55 && t60 < decay * 1.6;
            info << "decay " << juce::String (decay, 0) << " s -> T60 " << juce::String (t60, 2) << " s   ";
        }
        report ((juce::String ("Tipo ") + TopVocalChainProcessor::reverbTypeNames()[type]).toRawUTF8(), ok, info);
    }
    {
        TopVocalChainProcessor p, pd;
        auto imp = [=] (int i, float& l, float& r) { l = r = (i == 0) ? 0.9f : 0.0f; };
        auto o = runStereo (p, { { "rev_on", 1 }, { "rev_predelay", 80 }, { "rev_mix", 100 }, { "rev_er", 0 }, { "rev_locut", 20 } }, imp, 1.0);
        auto d = runStereo (pd, { { "rev_on", 0 } }, imp, 1.0);
        const int lat = p.getLatencySamples();
        int first = -1;
        for (int i = lat; i < 48000; ++i) if (std::abs (o.getSample (0, i) - d.getSample (0, i)) > 1e-4f) { first = i - lat; break; }
        report ("Pre-delay 80 ms", first > 0 && 1000.0 * first / sr > 78.0, juce::String::formatted ("la cola empieza a %.1f ms", 1000.0 * first / sr));
    }

    std::printf ("\n== FASE 3: MEDIDORES ==\n");
    {
        tvc::LoudnessMeter lm; lm.prepare (sr);
        const float amp = std::pow (10.0f, -23.0f / 20.0f);
        std::vector<float> l ((size_t) (sr * 10)), r ((size_t) (sr * 10));
        for (size_t i = 0; i < l.size(); ++i) l[i] = r[i] = amp * (float) std::sin (twoPi * 1000 * (double) i / sr);
        lm.process (l.data(), r.data(), (int) l.size());
        report ("LUFS (EBU: seno 1 kHz estereo -23 dBFS = -23 LUFS)", std::abs (lm.getIntegrated() + 23.0f) < 0.5f && std::abs (lm.getMomentary() + 23.0f) < 0.5f,
                juce::String::formatted ("integrado %.2f  momentaneo %.2f LUFS", lm.getIntegrated(), lm.getMomentary()));
    }
    {
        tvc::LoudnessMeter lm; lm.prepare (sr);
        std::vector<float> l ((size_t) sr);
        for (size_t i = 0; i < l.size(); ++i) l[i] = 0.9f * (float) std::sin (twoPi * (sr / 4) * (double) i / sr + juce::MathConstants<double>::pi / 4);
        float sp = 0; for (auto v : l) sp = std::max (sp, std::abs (v));
        lm.process (l.data(), l.data(), (int) l.size());
        report ("True Peak detecta picos entre muestras", lm.getTruePeakDb() > 20 * std::log10 (sp) + 2.0f,
                juce::String::formatted ("pico de muestras %.1f dB, true peak %.1f dB (real -0.9)", 20 * std::log10 (sp), lm.getTruePeakDb()));
    }

    std::printf ("\n== FASE 3: IA ANALIZAR VOZ ==\n");
    {
        TopVocalChainProcessor p;
        p.setPlayConfigDetails (2, 2, sr, 256);
        p.prepareToPlay (sr, 256);
        p.startAnalysis();
        std::vector<int> mel { 57, 60, 64, 62, 60, 59, 57, 64, 65, 64, 62, 57 };
        tvc::Biquad res; res.setPeak (sr, 350.0, 4.0, 10.0);   // resonancia artificial en 350 Hz
        juce::Random rnd (3);
        double ph = 0; juce::MidiBuffer m;
        juce::AudioBuffer<float> b (2, 256);
        int k = 0;
        while (! p.analysis.isReady() && k < 4000)
        {
            for (int i = 0; i < 256; ++i)
            {
                const double t = (k * 256 + i) / sr;
                const int note = mel[(size_t) ((int) (t / 0.35) % (int) mel.size())];
                const double hz = 440.0 * std::pow (2.0, (note - 69) / 12.0);
                ph += hz / sr;
                double v = 0;
                for (int h = 1; h <= 25; ++h) v += std::sin (twoPi * h * ph) / h;
                const bool sib = std::fmod (t, 1.4) < 0.08;
                float x = (float) res.process ((float) (0.05 * v)) + (sib ? (rnd.nextFloat() - 0.5f) * 0.05f : 0.0f);
                b.setSample (0, i, x); b.setSample (1, i, x);
            }
            p.processBlock (b, m);
            ++k;
        }
        const auto rep = p.analysis.computeReport();
        const bool ok = rep.valid && rep.keyRoot == 9 && rep.minor && rep.cutResonance && std::abs (rep.resonanceHz - 350.0f) < 60.0f
                        && rep.registro.isNotEmpty() && rep.inGain > 3.0f;
        report ("Analisis completo", ok, juce::String::formatted ("%.1f s", k * 256 / sr));
        std::printf ("      Registro %s (%.0f Hz) | tonalidad %s %s (%.0f%%) | nivel %.1f dBFS | dinamica %.1f dB (%s)\n",
                     rep.registro.toRawUTF8(), rep.f0, TopVocalChainProcessor::keyNames()[rep.keyRoot].toRawUTF8(), rep.minor ? "menor" : "mayor",
                     rep.keyConf * 100, rep.rmsDb, rep.dynRange, rep.dynLabel.toRawUTF8());
        std::printf ("      Resonancia %.0f Hz (+%.1f dB) | sibilancia %s (%.1f kHz) | presencia %s | graves %s | ruido %s\n",
                     rep.resonanceHz, rep.resonanceDb, rep.sibLabel.toRawUTF8(), rep.sibHz / 1000, rep.presenceLabel.toRawUTF8(), rep.lowLabel.toRawUTF8(), rep.noiseLabel.toRawUTF8());
        for (auto& rr : rep.recommendations) std::printf ("      - %s\n", rr.toRawUTF8());
        p.applyReport (rep);
        const bool applied = std::abs (p.apvts.getRawParameterValue ("eq_mid_freq")->load() - rep.midFreq) < 2.0f
                             && (int) p.apvts.getRawParameterValue ("tune_key")->load() == 9
                             && (int) p.apvts.getRawParameterValue ("comp_type")->load() == 4;
        report ("Aplicar recomendaciones", applied, juce::String::formatted ("EQ %.0f Hz %.1f dB, entrada %+.1f dB, comp thr %.1f",
                p.apvts.getRawParameterValue ("eq_mid_freq")->load(), p.apvts.getRawParameterValue ("eq_mid_gain")->load(),
                p.apvts.getRawParameterValue ("in_gain")->load(), p.apvts.getRawParameterValue ("comp_thr")->load()));
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

static int snapshot (const char* path, int hidden, int scaleIdx, bool states)
{
    TopVocalChainProcessor proc;
    proc.hiddenModules.store (hidden);
    proc.uiScaleIndex.store (scaleIdx);
    if (states)
    {
        auto set = [&] (const char* id, float v) { auto* prm = proc.apvts.getParameter (id); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); };
        set ("rev_on", 0); set ("pre_drive", 24); set ("in_gain", 12);   // reverb apagada y preamp saturando
    }
    proc.setPlayConfigDetails (2, 2, 44100.0, 512);
    proc.prepareToPlay (44100.0, 512);
    juce::AudioBuffer<float> b (2, 512); juce::MidiBuffer m; double ph = 0;
    std::unique_ptr<juce::AudioProcessorEditor> live;
    if (! states)
    {
        auto set = [&] (const char* id, float v) { auto* prm = proc.apvts.getParameter (id); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); };
        set ("tune_on", 1); set ("tune_key", 9); set ("tune_scale", 2); set ("wide_on", 1); set ("wide_mode", 1); set ("eq_on", 1);
        set ("air_mid", 35); set ("air_high", 55); set ("air_sparkle", 20);
        if (std::getenv ("TVC_DARK") != nullptr) proc.uiTheme.store (1);
        live.reset (proc.createEditor());
        live->setVisible (true);
    }
    juce::Random rnd (7);
    for (int k = 0; k < 400; ++k)
    {
        for (int i = 0; i < 512; ++i)
        {
            const double tt = (k * 512 + i) / 44100.0;
            // voz que se desliza entre notas con vibrato (para ver el grafico de afinacion)
            const double midi = 57.0 + 3.0 * std::floor (std::fmod (tt, 2.4) / 0.8) + 0.35 * std::sin (6.283 * 5.5 * tt) + 0.3;
            const double hz = 440.0 * std::pow (2.0, (midi - 69.0) / 12.0);
            ph += hz / 44100.0; float v = 0; for (int h = 1; h < 10; ++h) v += (float) std::sin (6.283185307 * h * ph) / (h * 1.3f);
            v = v * 0.25f + (rnd.nextFloat() - 0.5f) * 0.01f;
            b.setSample (0, i, v); b.setSample (1, i, v);
        }
        proc.processBlock (b, m);
        if (live != nullptr && k % 3 == 0)
            if (auto* te = dynamic_cast<TopVocalChainEditor*> (live.get())) te->tickNow();
    }
    if (live != nullptr)
    {
        if (std::getenv ("TVC_BROWSER") != nullptr)
            if (auto* te = dynamic_cast<TopVocalChainEditor*> (live.get())) te->openBrowserForTest();
        auto img = live->createComponentSnapshot (live->getLocalBounds(), true, 1.0f);
        juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (path));
        f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("snapshot -> %s\n", f.getFullPathName().toRawUTF8());
        return 0;
    }
    if (states)   // tambien hace un ANALIZAR VOZ para mostrar el informe
    {
        proc.startAnalysis();
        std::vector<int> mel { 57, 60, 64, 62, 60, 59, 57, 64, 65, 64, 62, 57 };
        tvc::Biquad res; res.setPeak (44100.0, 350.0, 4.0, 10.0);
        double ph2 = 0; int k = 0;
        while (! proc.analysis.isReady() && k < 4000)
        {
            for (int i = 0; i < 512; ++i)
            {
                const double tt = (k * 512 + i) / 44100.0;
                const double hz = 440.0 * std::pow (2.0, (mel[(size_t) ((int) (tt / 0.35) % 12)] - 69) / 12.0);
                ph2 += hz / 44100.0; double v = 0; for (int h = 1; h <= 25; ++h) v += std::sin (6.283185307 * h * ph2) / h;
                const float x = res.process ((float) (0.05 * v)) + ((std::fmod (tt, 0.9) < 0.12) ? 0.0f : 0.0f);
                b.setSample (0, i, x); b.setSample (1, i, x * 0.9f);
            }
            proc.processBlock (b, m); ++k;
        }
    }
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setVisible (true);
    if (auto* te = dynamic_cast<TopVocalChainEditor*> (ed.get())) { te->tickNow(); te->tickNow(); }
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (path));
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("snapshot %dx%d -> %s\n", img.getWidth(), img.getHeight(), f.getFullPathName().toRawUTF8());
    return 0;
}


//==============================================================================
// FASE 4: calidad, compresores nuevos, delay/reverb nuevos, EQ (notch, M/S, solo, dinamico), ANALOG, MIDI, configuracion
static int testPhase4()
{
    int f = 0;
    const double sr = 48000.0, twoPi = 2 * juce::MathConstants<double>::pi;
    auto report = [&f] (const char* name, bool ok, const juce::String& info)
    {
        if (! ok) ++f;
        std::printf ("  %-44s %s  %s\n", name, info.toRawUTF8(), ok ? "OK" : "FALLO");
    };
    std::printf ("\nFASE 4\n");
    auto sine = [=] (double hz, double a) { return [=] (int i, double s) { return (float) (a * std::sin (twoPi * hz * i / s)); }; };
    auto lvl = [&] (const juce::AudioBuffer<float>& b, double hz)
    { return 20.0 * std::log10 (harmonicLevel (b.getReadPointer (0) + b.getNumSamples() / 2, b.getNumSamples() / 3, sr, hz) + 1e-12); };
    auto finite = [] (const juce::AudioBuffer<float>& b)
    {
        for (int c = 0; c < b.getNumChannels(); ++c)
            for (int i = 0; i < b.getNumSamples(); ++i) if (! std::isfinite (b.getSample (c, i)) || std::abs (b.getSample (c, i)) > 4.0f) return false;
        return true;
    };

    // Calidad: la latencia cambia con el oversampling, y High satura con menos aliasing que Eco
    {
        int lat[3];
        double alias[3];
        for (int q = 0; q < 3; ++q)
        {
            auto b = runIsolated ({ { "pre_on", 1 }, { "pre_type", 7 }, { "pre_drive", 24 }, { "quality", (float) q } }, sine (7000, 0.5), 0.5, sr);
            lat[q] = lastLatency;
            // 3er armonico de 7 kHz = 21 kHz -> se pliega a 27 kHz->21k? con sr 48k el 5o (35k) se pliega a 13 kHz
            alias[q] = lvl (b, 13000.0);
        }
        report ("Calidad Eco/Normal/High (latencia)", lat[0] < lat[1] && lat[1] < lat[2], juce::String::formatted ("%d / %d / %d muestras", lat[0], lat[1], lat[2]));
        report ("High tiene menos aliasing que Eco", alias[2] < alias[0] - 6, juce::String::formatted ("alias 13k: Eco %.1f dB, High %.1f dB", alias[0], alias[2]));
    }
    // Compresores nuevos: Tube y Modern Digital comprimen
    for (int type : { 7, 8 })
    {
        auto loud = runIsolated ({ { "comp_on", 1 }, { "comp_type", (float) type }, { "comp_thr", -30 }, { "comp_ratio", 6 }, { "comp_makeup", 0 } }, sine (1000, 0.5), 1.0, sr);
        const double g = lvl (loud, 1000) - 20.0 * std::log10 (0.5);
        report (type == 7 ? "Compresor Tube" : "Compresor Modern Digital", g < -10 && finite (loud), juce::String::formatted ("ganancia %.1f dB", g));
    }
    // Delay: Slap limita el tiempo a eco corto; Tape/Analog oscurecen las repeticiones
    {
        auto echoAt = [&] (std::vector<std::pair<const char*, float>> prm, double hz)
        {
            prm.insert (prm.end(), { { "dly_on", 1 }, { "dly_sync", 0 }, { "dly_mix", 100 }, { "dly_fb", 0 } });
            auto b = runIsolated (prm, sine (hz, 0.2), 1.0, sr);
            return lvl (b, hz);
        };
        auto echoPeak = [&] (int type)
        {
            auto imp = runIsolated ({ { "dly_on", 1 }, { "dly_type", (float) type }, { "dly_sync", 0 }, { "dly_time", 300 }, { "dly_mix", 100 },
                                      { "dly_fb", 0 }, { "dly_hicut", 20000 } }, [] (int i, double) { return i == 1000 ? 1.0f : 0.0f; }, 1.0, sr);
            float pk = 0; for (int i = 1000 + lastLatency + 200; i < imp.getNumSamples(); ++i) pk = std::max (pk, std::abs (imp.getSample (0, i)));
            return pk;
        };
        juce::ignoreUnused (echoAt);
        const float dig = echoPeak (0), ana = echoPeak (2);
        report ("Delay Analog mas oscuro que Digital", ana < dig * 0.7f, juce::String::formatted ("pico del eco: digital %.2f, analog %.2f", dig, ana));
        TopVocalChainProcessor p; setP (p, "dly_type", 3); setP (p, "dly_time", 600); setP (p, "dly_sync", 0);
        report ("Delay Slap: eco corto", p.getDelayMs() > 0, "ok");
        auto imp = runIsolated ({ { "dly_on", 1 }, { "dly_type", 3 }, { "dly_sync", 0 }, { "dly_time", 600 }, { "dly_mix", 100 }, { "dly_fb", 0 } },
                                [] (int i, double) { return i == 1000 ? 1.0f : 0.0f; }, 1.0, sr);
        int peakAt = 0; float pk = 0;
        for (int i = 1000 + lastLatency + 50; i < imp.getNumSamples(); ++i) if (std::abs (imp.getSample (0, i)) > pk) { pk = std::abs (imp.getSample (0, i)); peakAt = i; }
        const double ms = (peakAt - 1000 - lastLatency) * 1000.0 / sr;
        report ("Slap: eco entre 60 y 160 ms", ms > 55 && ms < 170, juce::String::formatted ("eco a %.0f ms", ms));
    }
    // Reverb Vintage / Modern: suenan y Vintage es mas oscura
    {
        auto rev = [&] (int type)
        { return runIsolated ({ { "rev_on", 1 }, { "rev_type", (float) type }, { "rev_mix", 100 }, { "rev_decay", 2 } }, sine (6000, 0.2), 1.0, sr); };
        auto v = rev (5), m = rev (6);
        report ("Reverb Vintage / Modern", finite (v) && finite (m) && lvl (v, 6000) < lvl (m, 6000),
                juce::String::formatted ("6 kHz: vintage %.1f, modern %.1f dB", lvl (v, 6000), lvl (m, 6000)));
    }
    // EQ: Notch, Mid/Side, Solo y dinamico
    {
        auto eq = [&] (std::vector<std::pair<const char*, float>> prm, double hz, double a = 0.1)
        { prm.insert (prm.end(), { { "eq_on", 1 }, { "eq_lowcut", 20 }, { "eq_hicut", 20000 } }); return lvl (runIsolated (prm, sine (hz, a), 0.6, sr), hz) - 20 * std::log10 (a); };
        const double notch = eq ({ { "eq_mid_type", 1 }, { "eq_mid_freq", 1000 } }, 1000), near = eq ({ { "eq_mid_type", 1 }, { "eq_mid_freq", 1000 } }, 1500);
        report ("EQ Notch en 1 kHz", notch < -20 && near > -6, juce::String::formatted ("1 kHz %.1f dB, 1.5 kHz %.1f dB", notch, near));
        const double side = eq ({ { "eq_ms", 2 }, { "eq_mid_gain", 12 }, { "eq_mid_freq", 1000 } }, 1000);   // senal mono: Side no la toca
        const double mid = eq ({ { "eq_ms", 1 }, { "eq_mid_gain", 12 }, { "eq_mid_freq", 1000 } }, 1000);
        report ("EQ Mid/Side (voz mono)", std::abs (side) < 1 && mid > 10, juce::String::formatted ("Side %.1f dB, Mid %+.1f dB", side, mid));
        const double soloIn = eq ({ { "eq_solo", 1 }, { "eq_mid_freq", 1000 } }, 1000), soloOut = eq ({ { "eq_solo", 1 }, { "eq_mid_freq", 1000 } }, 100);
        report ("EQ Solo Mid", soloOut < soloIn - 12, juce::String::formatted ("1 kHz %.1f dB, 100 Hz %.1f dB", soloIn, soloOut));
        const double dynLoud = eq ({ { "eq_dyn", 100 }, { "eq_dyn_thr", -40 }, { "eq_mid_freq", 1000 } }, 1000, 0.5);
        const double dynSoft = eq ({ { "eq_dyn", 100 }, { "eq_dyn_thr", -40 }, { "eq_mid_freq", 1000 } }, 1000, 0.003);
        report ("EQ dinamico baja solo cuando suena fuerte", dynLoud < -4 && std::abs (dynSoft) < 1, juce::String::formatted ("fuerte %.1f dB, suave %.1f dB", dynLoud, dynSoft));
    }
    // ANALOG: L y R dejan de ser identicos (tolerancias) pero sin cambiar el nivel
    {
        auto b = runIsolated ({ { "pre_on", 1 }, { "pre_drive", 12 }, { "pre_analog", 1 } }, sine (500, 0.3), 0.5, sr);
        double diff = 0; for (int i = 10000; i < 20000; ++i) diff = std::max (diff, (double) std::abs (b.getSample (0, i) - b.getSample (1, i)));
        report ("ANALOG: canales distintos", diff > 1e-4 && finite (b), juce::String::formatted ("diferencia L/R %.4f", diff));
    }
    // MIDI Learn: aprende un CC y luego lo usa
    {
        TopVocalChainProcessor p;
        p.setPlayConfigDetails (2, 2, sr, 512); p.prepareToPlay (sr, 512);
        juce::AudioBuffer<float> b (2, 512); b.clear();
        p.startMidiLearn ("comp_thr");
        juce::MidiBuffer m; m.addEvent (juce::MidiMessage::controllerEvent (1, 21, 0), 0);
        p.processBlock (b, m);
        juce::MidiBuffer m2; m2.addEvent (juce::MidiMessage::controllerEvent (1, 21, 127), 0);
        p.processBlock (b, m2);
        const float thr = p.apvts.getRawParameterValue ("comp_thr")->load();
        report ("MIDI Learn (CC21 -> Threshold)", p.ccForParam ("comp_thr") == 21 && thr > -0.5f, juce::String::formatted ("CC %d, threshold %.1f", p.ccForParam ("comp_thr"), thr));
        // se guarda con el proyecto
        juce::MemoryBlock mb; p.getStateInformation (mb);
        TopVocalChainProcessor q; q.setStateInformation (mb.getData(), (int) mb.getSize());
        report ("MIDI se guarda en el proyecto", q.ccForParam ("comp_thr") == 21, "ok");
        // exportar / importar configuracion
        setP (p, "rev_mix", 33); p.uiTheme.store (1);
        auto cfg = p.exportConfig();
        TopVocalChainProcessor r; const bool ok = r.importConfig (cfg);
        report ("Exportar / importar configuracion", ok && r.uiTheme.load() == 1 && r.ccForParam ("comp_thr") == 21
                    && std::abs (r.apvts.getRawParameterValue ("rev_mix")->load() - 33) < 0.5f, "ok");
    }
    return f;
}

// VOCAL AIR: neutro en 0, sube agudos / presencia, SMOOTH baja el aire en las "S"
static int testAir()
{
    int f = 0;
    const double sr = 48000.0, twoPi = 2 * juce::MathConstants<double>::pi;
    auto report = [&f] (const char* name, bool ok, const juce::String& info)
    {
        if (! ok) ++f;
        std::printf ("  %-44s %s  %s\n", name, info.toRawUTF8(), ok ? "OK" : "FALLO");
    };
    std::printf ("\nVOCAL AIR\n");
    auto gain = [&] (std::vector<std::pair<const char*, float>> prm, double hz, double a = 0.05)
    {
        auto b = runIsolated (prm, [=] (int i, double s) { return (float) (a * std::sin (twoPi * hz * i / s)); }, 0.6, sr);
        return 20.0 * std::log10 (harmonicLevel (b.getReadPointer (0) + b.getNumSamples() / 2, b.getNumSamples() / 3, sr, hz) / a + 1e-12);
    };
    const double neutral = gain ({ { "air_on", 1 } }, 12000);
    report ("Air en 0 = neutro", std::abs (neutral) < 0.2, juce::String::formatted ("12 kHz %+.2f dB", neutral));
    const double hi = gain ({ { "air_on", 1 }, { "air_high", 100 }, { "air_smooth", 0 } }, 14000), hiLow = gain ({ { "air_on", 1 }, { "air_high", 100 }, { "air_smooth", 0 } }, 300);
    report ("HIGH AIR sube agudos, no graves", hi > 9 && std::abs (hiLow) < 0.5, juce::String::formatted ("14 kHz %+.1f dB, 300 Hz %+.1f dB", hi, hiLow));
    const double mid = gain ({ { "air_on", 1 }, { "air_mid", 100 }, { "air_smooth", 0 } }, 5000);
    report ("MID AIR sube presencia", mid > 6, juce::String::formatted ("5 kHz %+.1f dB", mid));
    // "S" (ruido agudo concentrado en 7 kHz) con SMOOTH 100: el aire se reduce
    const double sNo = gain ({ { "air_on", 1 }, { "air_high", 100 }, { "air_freq", 6000 }, { "air_smooth", 0 } }, 7000, 0.2);
    const double sYes = gain ({ { "air_on", 1 }, { "air_high", 100 }, { "air_freq", 6000 }, { "air_smooth", 100 } }, 7000, 0.2);
    report ("SMOOTH protege las S", sYes < sNo - 4, juce::String::formatted ("7 kHz: sin smooth %+.1f, con smooth %+.1f dB", sNo, sYes));
    // SPARKLE crea armonicos nuevos por encima
    auto sp = [&] (float v)
    {
        auto b = runIsolated ({ { "air_on", 1 }, { "air_sparkle", v }, { "air_freq", 8000 } }, [=] (int i, double s) { return (float) (0.3 * std::sin (twoPi * 6000 * i / s)); }, 0.6, sr);
        return 20.0 * std::log10 (harmonicLevel (b.getReadPointer (0) + b.getNumSamples() / 2, b.getNumSamples() / 3, sr, 18000) + 1e-12);
    };
    const double s0 = sp (0), s1 = sp (100);
    report ("SPARKLE crea brillo nuevo", s1 > s0 + 20, juce::String::formatted ("armonico 18 kHz: %.1f -> %.1f dB", s0, s1));
    return f;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc > 2 && juce::String (argv[1]) == "--snapshot")
        return snapshot (argv[2], argc > 3 ? atoi (argv[3]) : 0, argc > 4 ? atoi (argv[4]) : 0, argc > 5);
    int fails = 0;
    const float a3 = 220.0f;
    const float sharp35 = a3 * std::pow (2.0f, 0.35f / 12.0f);
    const float flat40  = a3 * std::pow (2.0f, -0.40f / 12.0f);

    fails += runCase ("Default 44.1k (A3 +35c)",      0, sharp35, a3, 44100.0, 512);
    fails += runCase ("Default 48k (A3 -40c)",        0, flat40, a3, 48000.0, 256);
    fails += runCase ("Dominican Dembow C menor 96k", 1, sharp35, 233.08f, 96000.0, 1024);
    // C menor: A no esta en la escala -> debe ir a G# (Ab) o A# (Bb). 220*2^(0.35/12) -> mas cerca de A#? no: G#=207.65, A#=233.08
    fails += runCase ("Modern Reggaeton C menor",  3, sharp35, 233.08f, 44100.0, 128);
    for (int prog = 0; prog < (int) tvc::getFactoryPresets().size(); ++prog)
        fails += runCase (tvc::getFactoryPresets()[(size_t) prog].name, prog, sharp35, 0.0f, 44100.0, 480);

    fails += testModels();
    fails += testWider();
    fails += testPhase1();
    fails += testPhase2();
    fails += testPhase3();
    fails += testPhase4();
    fails += testAir();
    std::printf ("\n%s (%d fallos)\n", fails == 0 ? "TODAS LAS PRUEBAS OK" : "HAY FALLOS", fails);
    return fails;
}
