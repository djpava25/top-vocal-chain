// Pruebas de Top Mic Collection (motor nuevo: curvas medidas, proximidad fisica, eje, electronica)
#include "TopMicProcessor.h"
#include "TopMicEditor.h"
#include "MicArt.h"
#include <juce_dsp/juce_dsp.h>
#include <cstdio>

static int fails = 0;
static void report (const char* n, bool ok, const juce::String& info) { if (! ok) ++fails; std::printf ("%-56s %s  %s\n", n, info.toRawUTF8(), ok ? "OK" : "FALLO"); }
static const double SR = 48000.0, TWO_PI = 6.283185307179586;
static const int BS = 256;
static void setP (TopMicProcessor& p, const juce::String& id, float v) { auto* q = p.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); }

struct Run
{
    TopMicProcessor& p; std::vector<float> L, R;
    explicit Run (TopMicProcessor& pp, int chans = 2) : p (pp) { p.setPlayConfigDetails (chans, chans, SR, BS); p.prepareToPlay (SR, BS); }
    template <typename G, typename C> void go (double secs, G gen, C cb)
    {
        const int chs = p.getTotalNumInputChannels();
        juce::AudioBuffer<float> b (chs, BS);
        const int nb = (int) (secs * SR / BS);
        for (int k = 0; k < nb; ++k)
        {
            const size_t i0 = L.size();
            for (int i = 0; i < BS; ++i) for (int c = 0; c < chs; ++c) b.setSample (c, i, gen ((int) i0 + i, c));
            juce::MidiBuffer m; p.processBlock (b, m);
            for (int i = 0; i < BS; ++i) { L.push_back (b.getSample (0, i)); R.push_back (b.getSample (chs > 1 ? 1 : 0, i)); }
            cb (k);
        }
    }
    template <typename G> void go (double secs, G gen) { go (secs, gen, [] (int) {}); }
};
static auto sine (double hz, float amp) { return [=] (int i, int) { return amp * (float) std::sin (TWO_PI * hz * i / SR); }; }
static double level (const std::vector<float>& x, double hz, double t0, double t1)
{
    double re = 0, im = 0; const int a = (int) (t0 * SR), b = (int) (t1 * SR);
    for (int i = a; i < b; ++i) { const double w = 0.5 - 0.5 * std::cos (TWO_PI * (i - a) / (b - a)); re += x[(size_t) i] * w * std::cos (TWO_PI * hz * i / SR); im += x[(size_t) i] * w * std::sin (TWO_PI * hz * i / SR); }
    return 20 * std::log10 (std::sqrt (re * re + im * im) / (b - a) * 4 + 1e-15);
}
static double thdPct (const std::vector<float>& x, double hz, double t0, double t1)
{
    const double f = level (x, hz, t0, t1); double s = 0;
    for (int k = 2; k <= 7 && k * hz < 20000; ++k) s += std::pow (10.0, (level (x, k * hz, t0, t1) - f) / 10.0);
    return std::sqrt (s) * 100.0;
}
static tmic::Settings S (int model, int source = 0, int pattern = tmic::Cardioid, float dist = 15, float prox = 0, float axis = 0)
{ tmic::Settings s; s.model = model; s.source = source; s.pattern = pattern; s.dist = dist; s.prox = prox; s.axis = axis; return s; }
static double R (const tmic::Settings& s, double f) { return tmic::responseDb (s, SR, f); }

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc > 2 && juce::String (argv[1]) == "--mics")
    {
        // todos los microfonos en una grilla (para revisar el dibujo)
        const int n = (int) tmic::models().size(), cols = 8, cw = 170, chh = 330;
        juce::Image img (juce::Image::ARGB, cols * cw, ((n + cols - 1) / cols) * chh, true);
        { juce::Graphics g (img); g.fillAll (juce::Colour (0xff1c1b1e));
          for (int i = 0; i < n; ++i)
          {
              const auto d = micart::designFor (i);
              const float mx = 26, my = 30, uw = d.halfW * 2 + mx * 2, uh = d.height + my * 2;
              const float s = std::min ((cw - 10) / uw, (chh - 30) / uh);
              const auto& m = micart::micImage (i, 2.0f);
              const float x = (float) (i % cols) * cw + cw * 0.5f - uw * s * 0.5f, y = (float) (i / cols) * chh + (chh - 24) - uh * s;
              g.drawImage (m, juce::Rectangle<float> (x, y, uw * s, uh * s));
              g.setColour (juce::Colours::white); g.drawText (tmic::models()[(size_t) i].name, (i % cols) * cw, (i / cols) * chh + chh - 22, cw, 18, juce::Justification::centred);
          } }
        juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2])); f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("mics -> %s\n", f.getFullPathName().toRawUTF8());
        return 0;
    }
    if (argc > 2 && juce::String (argv[1]) == "--snapshot")
    {
        TopMicProcessor p; p.setCurrentProgram (argc > 3 ? juce::String (argv[3]).getIntValue() : 0);
        { Run r (p); r.go (0.3, sine (220, 0.2f)); }
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        ed->setVisible (true); if (argc > 4) ed->setSize (TopMicEditor::W, TopMicEditor::H);
        if (auto* te = dynamic_cast<TopMicEditor*> (ed.get())) te->tickNow();
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2])); f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("snapshot -> %s\n", f.getFullPathName().toRawUTF8());
        return 0;
    }

    // ---------------------------------------------------------------- 1. ajuste de las curvas medidas
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        double worst = 0; juce::String where;
        for (int i = 0; i < (int) tmic::models().size(); ++i)
            for (int p = 0; p < 5; ++p)
            {
                const auto& m = tmic::models()[(size_t) i];
                if (! (m.patterns >> p & 1u)) continue;
                const auto& b = tmic::fitFor (0, i, p, fs);
                for (double f = 30; f <= 16000; f *= 1.04) { const double e = std::abs (tmic::bandsDb (b, fs, f) - tmic::modelDb (m, p, f)); if (e > worst) { worst = e; where = juce::String (m.name) + " " + tmic::patternName (p) + " " + juce::String ((int) f) + " Hz"; } }
            }
        for (int i = 1; i < (int) tmic::sources().size(); ++i)
        {
            const auto& b = tmic::fitFor (1, i, 0, fs);
            for (double f = 30; f <= 16000; f *= 1.04) worst = std::max (worst, std::abs (tmic::bandsDb (b, fs, f) - tmic::sourceDb (tmic::sources()[(size_t) i], f)));
        }
        report (("Curvas medidas reproducidas a " + juce::String ((int) fs) + " Hz").toRawUTF8(), worst < 0.6, "error max " + juce::String (worst, 2) + " dB (" + where + ")");
    }
    // ---------------------------------------------------------------- 2. el origen se cancela exacto
    {
        double worst = 0;
        for (auto [mi, si] : { std::pair<int, int> { 7, 7 }, { 6, 8 }, { 2, 9 }, { 9, 2 } })
            for (double f = 25; f <= 18000; f *= 1.05) worst = std::max (worst, std::abs (R (S (mi, si), f)));
        report ("Mismo micro de origen y destino = plano", worst < 0.1, "desvio max " + juce::String (worst, 3) + " dB");
    }
    // ---------------------------------------------------------------- 3. datos publicados
    {
        auto F = [] (int m, double f) { return R (S (m, 0, tmic::Cardioid, 100), f) - R (S (m, 0, tmic::Cardioid, 100), 1000); };
        double flat = 0; for (double f = 100; f <= 4000; f *= 1.1) flat = std::max (flat, std::abs (F (7, f)));
        const double p10 = F (7, 10000);
        report ("Estilo 103: plano hasta 5 kHz y +4 dB de presencia", flat < 0.8 && std::abs (p10 - 4.0) < 0.8, "plano +-" + juce::String (flat, 2) + ", 10 kHz " + juce::String (p10, 2) + " dB");
        const double v47 = F (0, 5000);
        report ("Estilo 47: presencia ancha ~5 dB en 5 kHz", v47 > 3.2 && v47 < 5.0, juce::String (v47, 2) + " dB");
        const double s800 = F (5, 12000);
        report ("Estilo 800: aire de 8 a 14 kHz (+6 dB)", s800 > 5.0, juce::String (s800, 2) + " dB en 12 kHz");
        const double r44 = F (12, 15000);
        report ("Estilo 44: agudos que caen (~-12 dB en 15 kHz)", r44 < -9.0, juce::String (r44, 1) + " dB");
        const double sm57 = F (8, 6000), sm57lo = F (8, 60);
        report ("Estilo 57: presencia en 6 kHz y graves recortados lejos", sm57 > 4.5 && sm57lo < -4.0, juce::String (sm57, 1) + " dB / 60 Hz " + juce::String (sm57lo, 1) + " dB");
    }
    // ---------------------------------------------------------------- 4. U87 contra TLM 103 (lo que los hace distintos)
    {
        const auto u87 = S (2), t103 = S (7);
        const double hi = R (t103, 12000) - R (u87, 12000), warm = R (u87, 150) - R (t103, 150), pk = R (u87, 9000) - R (u87, 12000);
        report ("Estilo 103 mas brillante que 87 en 12 kHz", hi > 2.5, juce::String (hi, 2) + " dB");
        report ("Estilo 87 mas calido en 150 Hz", warm > 0.3, juce::String (warm, 2) + " dB");
        report ("Estilo 87: pico definido en 9 kHz (no meseta)", pk > 1.8, "9k - 12k = " + juce::String (pk, 2) + " dB");
        auto thdAt = [] (int model, double hz, float amp)
        { TopMicProcessor p; setP (p, "model", (float) model); Run r (p); r.go (0.8, sine (hz, amp)); return thdPct (r.L, hz, 0.4, 0.78); };
        const double tU = thdAt (2, 50, 0.5f), tT = thdAt (7, 50, 0.5f);
        report ("Transformador del 87: calidez en graves fuertes (103 limpio)", tU > 5.0 * tT, juce::String (tU, 3) + " % contra " + juce::String (tT, 3) + " %");
    }
    // ---------------------------------------------------------------- 5. proximidad fisica
    {
        // proximidad pura del destino: origen omni (no tenia proximidad) a 5 cm contra 1 m
        auto prox = [] (int m, int pat, float cmNear, double f) { return (R (S (m, 5, pat, cmNear), f) - R (S (m, 5, pat, cmNear), 2000)) - (R (S (m, 5, pat, 100), f) - R (S (m, 5, pat, 100), 2000)); };
        auto proxV = [] (int m, int pat, float pr) { return R (S (m, 0, pat, 15, pr), 100) - R (S (m, 0, pat, 15, 0), 100); };
        const double ldc = proxV (7, tmic::Cardioid, 100), re20 = proxV (10, tmic::Cardioid, 100), omni = proxV (2, tmic::Omni, 100);
        report ("Acercar 4x: cardioide suma graves", ldc > 6 && ldc < 14, juce::String (ldc, 1) + " dB en 100 Hz");
        const double c8 = prox (11, tmic::Figure8, 5, 200), cc = prox (7, tmic::Cardioid, 5, 200);
        report ("Figura 8 suma mas graves que cardioide (200 Hz)", c8 > cc + 2, juce::String (c8, 1) + " contra " + juce::String (cc, 1) + " dB");
        report ("Variable-D (RE20) casi sin proximidad", re20 < 3.0 && re20 < ldc * 0.35, juce::String (re20, 2) + " dB");
        report ("Omni sin proximidad", std::abs (omni) < 0.6, juce::String (omni, 2) + " dB");
        const double p47 = prox (0, tmic::Cardioid, 5, 100);
        report ("Estilo 47 de cerca: hasta ~15 dB en 100 Hz (Mix)", p47 > 9 && p47 < 17, juce::String (p47, 1) + " dB a 5 cm contra 1 m");
    }
    // ---------------------------------------------------------------- 6. fuera de eje y patrones
    {
        const double lf = R (S (2, 0, tmic::Cardioid, 15, 0, 90), 1000) - R (S (2), 1000), hf = R (S (2, 0, tmic::Cardioid, 15, 0, 90), 10000) - R (S (2), 10000);
        report ("Cardioide a 90: -6 dB y menos agudos", std::abs (lf + 6.0) < 1.0 && hf < lf - 5.0, "1 kHz " + juce::String (lf, 1) + " dB / 10 kHz " + juce::String (hf, 1) + " dB");
        const double side = (R (S (2, 0, tmic::Cardioid, 15, 0, 90), 100) - R (S (2, 0, tmic::Cardioid, 15, 0, 90), 1000)) - (R (S (2), 100) - R (S (2), 1000));
        report ("De costado se pierde la proximidad (menos grave)", side < -2.0, juce::String (side, 1) + " dB en 100 Hz");
        const double om = R (S (2, 0, tmic::Omni), 9500) - R (S (2), 9500);
        report ("Multipatron: omni con mas aire que cardioide", om > 1.0, "+" + juce::String (om, 2) + " dB en 9.5 kHz");
        TopMicProcessor pa, pb; setP (pa, "model", 11); setP (pb, "model", 11); setP (pb, "rev", 1);
        Run ra (pa), rb (pb); ra.go (0.4, sine (500, 0.2f)); rb.go (0.4, sine (500, 0.2f));
        double corr = 0; for (size_t i = 9600; i < ra.L.size(); ++i) corr += (double) ra.L[i] * rb.L[i];
        report ("Figura 8: la parte de atras invierte la fase", corr < 0, "");
        TopMicProcessor pc; setP (pc, "model", 7); setP (pc, "pattern", 0);
        report ("Solo patrones que tiene cada micro", pc.effectivePattern (false) == tmic::Cardioid, tmic::patternName (pc.effectivePattern (false)));
    }
    // ---------------------------------------------------------------- 7. el audio procesado es igual al diseno
    {
        double worst = 0;
        for (auto st : { S (2, 7, tmic::Cardioid, 10, 30, 20), S (0, 8, tmic::Omni, 25, -40, 0), S (12, 1, tmic::Figure8, 6, 0, 40), S (5, 3, tmic::Cardioid, 15, 60, 0) })
        {
            for (double f : { 60.0, 200.0, 1000.0, 4000.0, 10000.0 })
            {
                TopMicProcessor p; setP (p, "model", (float) st.model); setP (p, "source", (float) st.source); setP (p, "pattern", (float) st.pattern);
                setP (p, "dist", st.dist); setP (p, "prox", st.prox); setP (p, "axis", st.axis);
                Run r (p); r.go (0.5, sine (f, 0.01f));
                worst = std::max (worst, std::abs ((level (r.L, f, 0.3, 0.49) - 20 * std::log10 (0.01)) - R (st, f)));
            }
        }
        report ("Audio procesado = curva de diseno", worst < 0.25, "desvio max " + juce::String (worst, 3) + " dB");
    }
    // ---------------------------------------------------------------- 8. sin artefactos
    {
        auto hfBurst = [] (std::function<void (TopMicProcessor&, int)> act)
        {
            TopMicProcessor p; setP (p, "model", 0); setP (p, "dist", 15);
            Run r (p); r.go (1.6, sine (300, 0.2f), [&] (int k) { act (p, k); });
            juce::dsp::IIR::Filter<float> h1, h2; h1.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (SR, 6000.0); h2.coefficients = h1.coefficients;
            std::vector<float> h (r.L.size()); for (size_t i = 0; i < h.size(); ++i) h[i] = h2.processSample (h1.processSample (r.L[i]));
            auto rms = [&] (double t0, double t1) { double s = 0; const int a = (int) (t0 * SR), b = (int) (t1 * SR); for (int i = a; i < b; ++i) s += (double) h[(size_t) i] * h[(size_t) i]; return 10 * std::log10 (s / (b - a) + 1e-30); };
            return std::make_pair (rms (0.4, 0.75), rms (0.75, 1.2));
        };
        const auto sw = hfBurst ([] (TopMicProcessor& p, int k) { if (k == (int) (0.8 * SR / BS)) { setP (p, "model", 12); setP (p, "source", 8); setP (p, "pattern", 4); } });
        report ("Cambiar de micro en pleno audio: sin clic", sw.second < -70, juce::String (sw.first, 1) + " / " + juce::String (sw.second, 1) + " dB (> 6 kHz)");
        const auto mv = hfBurst ([] (TopMicProcessor& p, int k) { if (k > (int) (0.8 * SR / BS) && k < (int) (1.1 * SR / BS)) { setP (p, "axis", (float) ((k * 37) % 180)); setP (p, "prox", (float) ((k * 53) % 200 - 100)); setP (p, "dist", (float) (2 + (k * 7) % 90)); } });
        report ("Mover eje, cercania y distancia: sin zumbido", mv.second < -70, juce::String (mv.first, 1) + " / " + juce::String (mv.second, 1) + " dB (> 6 kHz)");
        // aliasing de la valvula (15 kHz fuerte)
        TopMicProcessor pa; setP (pa, "model", 1); Run ra (pa); ra.go (0.8, sine (15000, 0.5f));
        const double f0 = level (ra.L, 15000, 0.4, 0.78); double worstA = -300;
        for (double a : { 3000.0, 6000.0, 9000.0, 18000.0, 21000.0 }) worstA = std::max (worstA, level (ra.L, a, 0.4, 0.78) - f0);
        report ("Aliasing de la valvula (15 kHz a -6 dBFS)", worstA < -80, juce::String (worstA, 1) + " dB");
        TopMicProcessor q; setP (q, "model", 1); setP (q, "prox", 100); setP (q, "dist", 2); Run rq (q); bool fin = true;
        rq.go (0.8, [] (int i, int) { return i < 12000 ? 1.0e-30f : 8.0f * (float) std::sin (TWO_PI * 40.0 * i / SR); });
        for (float v : rq.L) if (! std::isfinite (v)) fin = false;
        report ("Silencio, denormales y +18 dBFS: sin NaN/Inf", fin, "");
    }
    // ---------------------------------------------------------------- 9. electronica segun hoja de datos
    {
        auto thdAt = [] (int model, float amp) { TopMicProcessor p; setP (p, "model", (float) model); Run r (p); r.go (0.6, sine (1000, amp)); return thdPct (r.L, 1000, 0.3, 0.58); };
        const double v2lo = thdAt (1, 0.126f), v2hi = thdAt (1, 0.7f), t103 = thdAt (7, 0.7f);
        report ("Valvula estilo 67: limpia en voz normal, ensucia fuerte", v2lo < 0.3 && v2hi > 3.0 * v2lo, juce::String (v2lo, 3) + " % -> " + juce::String (v2hi, 3) + " %");
        report ("FET estilo 103 limpio incluso fuerte (138 dB SPL)", t103 < 0.08, juce::String (t103, 3) + " %");
    }
    // ---------------------------------------------------------------- 10. dual, bypass, fase, mono, CPU, presets, estado, interfaz
    {
        TopMicProcessor p0; setP (p0, "power", 0); Run r0 (p0); r0.go (0.3, sine (700, 0.3f)); double e = 0;
        for (size_t i = 0; i < r0.L.size(); ++i) e = std::max (e, (double) std::abs (r0.L[i] - 0.3f * (float) std::sin (TWO_PI * 700 * (double) i / SR)));
        report ("POWER apagado = senal intacta", e < 1e-6, juce::String (e, 9));
        TopMicProcessor pd; setP (pd, "model", 12); setP (pd, "dual", 1); setP (pd, "modelB", 5); setP (pd, "blend", 100);
        TopMicProcessor pb; setP (pb, "model", 5);
        Run rd (pd), rb (pb); rd.go (0.8, sine (12000, 0.05f)); rb.go (0.8, sine (12000, 0.05f));
        const double ld = level (rd.L, 12000, 0.5, 0.78), lb = level (rb.L, 12000, 0.5, 0.78);
        report ("DUAL al 100 % = micro B", std::abs (ld - lb) < 0.3, juce::String (ld - lb, 2) + " dB");
        TopMicProcessor pm; setP (pm, "model", 3); Run rm (pm, 1); rm.go (0.4, sine (1000, 0.3f)); bool ok = true; for (float v : rm.L) if (! std::isfinite (v)) ok = false;
        report ("Mono funciona", ok, "");
        TopMicProcessor pc; setP (pc, "model", 0); setP (pc, "dual", 1); setP (pc, "modelB", 1); setP (pc, "source", 7);
        Run rc (pc, 2);
        const double t0 = juce::Time::getMillisecondCounterHiRes();
        rc.go (10.0, [] (int i, int c) { return 0.2f * (float) std::sin (TWO_PI * (c ? 110.0 : 220.0) * i / SR); });
        const double rt = (juce::Time::getMillisecondCounterHiRes() - t0) / 10000.0;
        report ("CPU (estereo, DUAL, 48 kHz)", rt < 0.15, juce::String (rt * 100.0, 2) + " % de tiempo real");
        bool presetsOk = pc.getNumPrograms() >= 16;
        for (int i = 0; i < pc.getNumPrograms(); ++i) { pc.setCurrentProgram (i); if (pc.getProgramName (i).isEmpty()) presetsOk = false; }
        report ("Presets", presetsOk, juce::String (pc.getNumPrograms()));
        pc.setCurrentProgram (17); juce::MemoryBlock mb; pc.getStateInformation (mb);
        TopMicProcessor q; q.setStateInformation (mb.getData(), (int) mb.getSize());
        report ("Guardar / cargar estado", q.getCurrentProgram() == 17 && (int) q.apvts.getRawParameterValue ("model")->load() == (int) pc.apvts.getRawParameterValue ("model")->load()
                && std::abs (q.apvts.getRawParameterValue ("dist")->load() - pc.apvts.getRawParameterValue ("dist")->load()) < 1e-3, "");
        std::unique_ptr<juce::AudioProcessorEditor> ed (pc.createEditor());
        ed->setSize (TopMicEditor::W / 2, TopMicEditor::H / 2);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds());
        report ("Interfaz escalable", img.getWidth() == TopMicEditor::W / 2, juce::String (ed->getWidth()) + "x" + juce::String (ed->getHeight()));
    }
    std::printf ("\n%s (%d fallos)\n", fails == 0 ? "TODO OK" : "HAY FALLOS", fails);
    return fails == 0 ? 0 : 1;
}
