// Pruebas de Top EQ-1A
#include "TopEqProcessor.h"
#include "TopEqEditor.h"
#include <cstdio>

static int fails = 0;
static void report (const char* n, bool ok, const juce::String& info) { if (! ok) ++fails; std::printf ("%-48s %s  %s\n", n, info.toRawUTF8(), ok ? "OK" : "FALLO"); }
static const double SR = 48000.0, TWO_PI = 6.283185307179586;
static void setP (TopEqProcessor& p, const juce::String& id, float v) { auto* q = p.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); }
static std::vector<float> run (TopEqProcessor& p, double hz, float amp = 0.05f, double secs = 0.8)
{
    p.setPlayConfigDetails (2, 2, SR, 512); p.prepareToPlay (SR, 512);
    juce::AudioBuffer<float> b (2, 512); std::vector<float> out; double ph = 0;
    for (int blk = 0; blk < (int) (SR * secs / 512); ++blk)
    {
        for (int i = 0; i < 512; ++i) { const float v = amp * (float) std::sin (ph); ph += TWO_PI * hz / SR; b.setSample (0, i, v); b.setSample (1, i, v); }
        juce::MidiBuffer m; p.processBlock (b, m);
        for (int i = 0; i < 512; ++i) out.push_back (b.getSample (0, i));
    }
    return out;
}
static float level (const std::vector<float>& x, double hz)
{
    double re = 0, im = 0; const int a = (int) (0.45 * SR), b = (int) (0.75 * SR);
    for (int i = a; i < b; ++i) { const double w = 0.5 - 0.5 * std::cos (TWO_PI * (i - a) / (b - a)); re += x[(size_t) i] * w * std::cos (TWO_PI * hz * i / SR); im += x[(size_t) i] * w * std::sin (TWO_PI * hz * i / SR); }
    return (float) (20 * std::log10 (std::sqrt (re * re + im * im) / (b - a) * 4 + 1e-12));
}
template <typename F> static float with (F setup, double hz, float amp = 0.05f)
{ TopEqProcessor p; setup (p); return level (run (p, hz, amp), hz) - 20.0f * std::log10 (amp); }

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc > 2 && juce::String (argv[1]) == "--snapshot")
    {
        TopEqProcessor p; p.setCurrentProgram (argc > 3 ? juce::String (argv[3]).getIntValue() : 5); run (p, 100, 0.3f, 0.2);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        ed->setVisible (true); if (argc > 4) ed->setSize (TopEqEditor::W, TopEqEditor::H);
        if (auto* te = dynamic_cast<TopEqEditor*> (ed.get())) te->tickNow();
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
        f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("snapshot -> %s\n", f.getFullPathName().toRawUTF8());
        return 0;
    }
    // 1. plano con todo en 0 (spec: 20 Hz-20 kHz +0/-1 dB, SoS: ~1 dB de perdida en el grave)
    {
        float worst = 0; const float lo = with ([] (TopEqProcessor&) {}, 20), hi = with ([] (TopEqProcessor&) {}, 20000);
        for (double hz : { 50.0, 100.0, 1000.0, 5000.0, 15000.0 }) worst = std::max (worst, std::abs (with ([] (TopEqProcessor&) {}, hz)));
        report ("Todo en 0 = plano (spec +0/-1 dB)", worst < 0.3f && lo < -0.3f && lo > -1.2f && hi > -1.0f && hi < 0.3f, juce::String::formatted ("50 Hz-15 kHz +-%.2f dB, 20 Hz %.2f, 20 kHz %.2f dB", worst, lo, hi));
    }
    // 2. maximos del manual, medidos en el circuito
    {
        teq::Settings b; b.lowBoost = 10; float mb = 99, Mb = 0, ma = 0, Ma = -99;
        for (int f = 0; f < 4; ++f) { b.lowFreq = f; const float v = teq::responseDb (b, teq::lowFreqs()[f]); mb = std::min (mb, v); Mb = std::max (Mb, v); }
        teq::Settings a; a.lowAtten = 10;
        for (int f = 0; f < 4; ++f) { a.lowFreq = f; const float v = teq::responseDb (a, teq::lowFreqs()[f]); ma = std::min (ma, v); Ma = std::max (Ma, v); }
        report ("Manual: LOW BOOST 13.5 dB / ATTEN 17.5 dB", mb > 12.5f && Mb < 14.5f && ma > -18.5f && Ma < -16.5f, juce::String::formatted ("boost %.1f a %.1f, atten %.1f a %.1f dB", mb, Mb, Ma, ma));
        teq::Settings h; h.highBoost = 10; h.bandwidth = 0; float hs = 99, hb = 0;
        for (int f = 0; f < 7; ++f) { h.highFreq = f; hs = std::min (hs, teq::responseDb (h, teq::highFreqs()[f])); }
        h.highFreq = 4; const float sharp = teq::responseDb (h, 10000); h.bandwidth = 10; hb = teq::responseDb (h, 10000);
        report ("Manual 18 dB y SoS: SHARP 9 dB mas que BROAD", hs > 17.0f && sharp - hb > 7.5f && sharp - hb < 10.0f, juce::String::formatted ("sharp >= %.1f dB, 10k sharp %.1f / broad %.1f", hs, sharp, hb));
        teq::Settings t; t.highAtten = 10; t.attFreq = 0;
        report ("Manual: HIGH ATTEN 16 dB", std::abs (teq::responseDb (t, 20000) + 16.0f) < 1.0f, juce::String::formatted ("5k sel, 20 kHz %.1f dB", teq::responseDb (t, 20000)));
    }
    // 3. el truco de graves (SoS: 2.5/2.5 -> +3 dB y -2 dB; Computer Music: 10/10 a 60 Hz -> ~6 dB cerca de 600 Hz)
    {
        teq::Settings s; s.lowBoost = 2.5f; s.lowAtten = 2.5f; s.lowFreq = 2;
        float dip = 0, at = 0; for (double f = 100; f < 5000; f *= 1.02) { const float v = teq::responseDb (s, f); if (v < dip) { dip = v; at = (float) f; } }
        const float shelf = teq::responseDb (s, 25);
        report ("Truco 2.5/2.5 a 60 Hz (SoS +3 / -2 dB)", shelf > 2.0f && shelf < 4.0f && dip < -1.0f && dip > -3.0f, juce::String::formatted ("grave %+.1f dB, hueco %.1f dB en %d Hz", shelf, dip, (int) at));
        s.lowBoost = 10; s.lowAtten = 10; dip = 0;
        for (double f = 100; f < 5000; f *= 1.02) { const float v = teq::responseDb (s, f); if (v < dip) { dip = v; at = (float) f; } }
        report ("Truco 10/10 a 60 Hz (hueco ~6 dB cerca de 600)", dip < -4.0f && dip > -7.5f && at > 350 && at < 900, juce::String::formatted ("hueco %.1f dB en %d Hz", dip, (int) at));
        // y el audio real coincide con el circuito
        const float b = with ([] (TopEqProcessor& p) { setP (p, "lowBoost", 10); setP (p, "lowAtten", 10); setP (p, "lowFreq", 2); }, 550);
        teq::Settings q; q.lowBoost = 10; q.lowAtten = 10; q.lowFreq = 2;
        report ("Audio = circuito (550 Hz)", std::abs (b - teq::responseDb (q, 550)) < 0.3f, juce::String::formatted ("audio %.2f, circuito %.2f dB", b, teq::responseDb (q, 550)));
        const float hf = with ([] (TopEqProcessor& p) { setP (p, "highBoost", 10); setP (p, "bandwidth", 0); setP (p, "highFreq", 6); }, 16000);
        teq::Settings r; r.highBoost = 10; r.bandwidth = 0; r.highFreq = 6;
        report ("Audio = circuito (16 kHz SHARP)", std::abs (hf - teq::responseDb (r, 16000)) < 1.0f, juce::String::formatted ("audio %.2f, circuito %.2f dB", hf, teq::responseDb (r, 16000)));
    }
    // 4. amplificador: THD 0.15 % a +10 dBm (0 dBFS = +18 dBu), domina el 2do (SoS), limpio a nivel normal
    {
        auto thd = [] (float dbu, double hz)
        {
            TopEqProcessor p; const float amp = std::pow (10.0f, (dbu - teq::dbuRef) / 20.0f);
            auto o = run (p, hz, amp, 0.8); const float f1 = level (o, hz); double e = 0;
            for (int k = 2; k <= 7; ++k) e += std::pow (10.0, (level (o, hz * k) - f1) / 10.0);
            return std::make_tuple ((float) (100.0 * std::sqrt (e)), level (o, hz * 2) - f1, level (o, hz * 3) - f1);
        };
        const auto [t10, h2, h3] = thd (10.0f, 1000);
        report ("Spec: THD 0.15% a +10 dBm", t10 > 0.10f && t10 < 0.20f && h2 > h3, juce::String::formatted ("%.3f%% (H2 %.1f, H3 %.1f dB)", t10, h2, h3));
        const auto [t0, a2, a3] = thd (0.0f, 1000);
        report ("Nivel normal (-18 dBFS) limpio", t0 < 0.06f, juce::String::formatted ("THD %.3f%%", t0));
        const auto [tl, l2, l3] = thd (10.0f, 40);
        report ("Trafos: mas color en graves fuertes", tl > t10, juce::String::formatted ("40 Hz %.2f%% vs 1 kHz %.2f%% (H3 %.1f dB)", tl, t10, l3));
        float mx = 0; for (float d = 14; d <= 26; d += 0.5f) if (std::get<0> (thd (d, 1000)) < 1.0f) mx = d;
        report ("Spec: salida maxima ~+21 dBm", mx >= 19.5f && mx <= 23.0f, juce::String::formatted ("1%% THD en +%.1f dBu", mx));
        TopEqProcessor p; setP (p, "highBoost", 10); setP (p, "highFreq", 6); setP (p, "bandwidth", 0); setP (p, "drive", 12);
        auto o = run (p, 7000, 0.5f, 0.8); const float f1 = level (o, 7000); float worst = -200;
        for (double f = 300; f < 20000; f += 100) { bool h = false; for (int k = 1; k < 3; ++k) if (std::abs (f - 7000.0 * k) < 250) h = true; if (! h) worst = std::max (worst, level (o, f) - f1); }
        { double wf = 0; for (double f = 300; f < 20000; f += 100) { bool h = false; for (int k = 1; k < 3; ++k) if (std::abs (f - 7000.0 * k) < 250) h = true; if (! h && level (o, f) - f1 >= worst - 0.01f) wf = f; }
          report ("Saturacion fuerte sin aliasing (8x)", worst < -70, juce::String::formatted ("peor componente %.1f dB en %d Hz (salida %.1f dBFS)", worst, (int) wf, f1)); }
        const float dl = with ([] (TopEqProcessor& q) { setP (q, "drive", 12); }, 1000, 0.01f);
        report ("DRIVE no cambia el volumen", std::abs (dl) < 0.2f, juce::String::formatted ("%+.2f dB", dl));
    }
    // 5. mover perillas sin clicks ni inestabilidad
    {
        TopEqProcessor p; p.setPlayConfigDetails (2, 2, SR, 256); p.prepareToPlay (SR, 256);
        juce::AudioBuffer<float> b (2, 256); float mx = 0, jump = 0, prev = 0;
        for (int blk = 0; blk < 400; ++blk)
        {
            setP (p, "lowBoost", 5.0f + 5.0f * (float) std::sin (blk * 0.05)); setP (p, "highBoost", 5.0f + 5.0f * (float) std::sin (blk * 0.07));
            setP (p, "bandwidth", 5.0f + 5.0f * (float) std::sin (blk * 0.03)); if (blk % 60 == 0) setP (p, "highFreq", (float) ((blk / 60) % 7));
            for (int i = 0; i < 256; ++i) { const float v = 0.2f * (float) std::sin (TWO_PI * 220 * (blk * 256 + i) / SR); b.setSample (0, i, v); b.setSample (1, i, v); }
            juce::MidiBuffer m; p.processBlock (b, m);
            for (int i = 0; i < 256; ++i) { const float v = b.getSample (0, i); mx = std::max (mx, std::abs (v)); jump = std::max (jump, std::abs (v - prev)); prev = v; }
        }
        report ("Perillas en movimiento: estable y sin saltos", std::isfinite (mx) && mx < 2.0f && jump < 0.15f, juce::String::formatted ("pico %.2f, salto max %.3f", mx, jump));
    }
    // salida, switch IN, presets, estado
    {
        const float o6 = with ([] (TopEqProcessor& p) { setP (p, "out", 6); }, 1000);
        report ("Output +6 dB", std::abs (o6 - 6) < 0.1f, juce::String (o6, 2) + " dB");
        TopEqProcessor p; setP (p, "power", 0); setP (p, "lowBoost", 10); setP (p, "highBoost", 10);
        auto o = run (p, 440, 0.3f); const int L = p.getLatencySamples(); double err = 0;
        for (int i = 20000; i < 30000; ++i) err = std::max (err, std::abs ((double) o[(size_t) i] - 0.3 * std::sin (TWO_PI * 440 * (i - L) / SR)));
        report ("IN apagado = senal original", err < 1e-4, "error " + juce::String (err, 6) + ", latencia " + juce::String (L));
        bool ok = true;
        for (int i = 0; i < p.getNumPrograms(); ++i)
        {
            TopEqProcessor q; q.setCurrentProgram (i); auto r = run (q, 200, 0.3f, 0.3);
            for (float v : r) if (! std::isfinite (v) || std::abs (v) > 2.0f) ok = false;
        }
        report ("Presets estables", ok, juce::String (p.getNumPrograms()) + " presets");
        TopEqProcessor a; setP (a, "lowBoost", 7); setP (a, "highFreq", 6);
        juce::MemoryBlock mb; a.getStateInformation (mb);
        TopEqProcessor b; b.setStateInformation (mb.getData(), (int) mb.getSize());
        report ("Guardar/cargar estado", std::abs (b.apvts.getRawParameterValue ("lowBoost")->load() - 7) < 0.01f && (int) b.apvts.getRawParameterValue ("highFreq")->load() == 6, "");
        std::unique_ptr<juce::AudioProcessorEditor> ed (a.createEditor()); ed->setSize (1560, 560);
        report ("Editor escalable", ed->getWidth() == 1560, "");
    }
    std::printf (fails == 0 ? "TODAS LAS PRUEBAS OK\n" : "FALLARON %d\n", fails);
    return fails == 0 ? 0 : 1;
}
