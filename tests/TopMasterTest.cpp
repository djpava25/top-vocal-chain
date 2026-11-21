// Pruebas de Top Master: transparencia, cada modulo, medidor LUFS, orden, estado, asistente, referencia y bypass igualado
#include "TopMasterProcessor.h"
#include "TopMasterEditor.h"
#include <cstdio>

using namespace tmaster;
static int fails = 0;
static void report (const char* n, bool ok, const juce::String& info) { if (! ok) ++fails; std::printf ("%-46s %s  %s\n", n, info.toRawUTF8(), ok ? "OK" : "FALLO"); }
static const double SR = 44100.0;
using Stereo = std::pair<std::vector<float>, std::vector<float>>;

static void setP (TopMasterProcessor& p, const juce::String& id, float v) { auto* q = p.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); }
static Stereo run (TopMasterProcessor& p, const Stereo& in, bool prepare = true)
{
    if (prepare) p.prepareToPlay (SR, 512);
    juce::AudioBuffer<float> b (2, 512);
    Stereo out;
    for (size_t pos = 0; pos + 512 <= in.first.size(); pos += 512)
    {
        for (int i = 0; i < 512; ++i) { b.setSample (0, i, in.first[pos + (size_t) i]); b.setSample (1, i, in.second[pos + (size_t) i]); }
        juce::MidiBuffer m; p.processBlock (b, m);
        for (int i = 0; i < 512; ++i) { out.first.push_back (b.getSample (0, i)); out.second.push_back (b.getSample (1, i)); }
    }
    return out;
}
static Stereo sine (double hz, float amp, double secs, float rightAmp = -1)
{
    Stereo s; const int n = (int) (SR * secs);
    for (int i = 0; i < n; ++i) { const float v = (float) std::sin (6.283185307 * hz * i / SR); s.first.push_back (amp * v); s.second.push_back ((rightAmp < -0.5f ? amp : rightAmp) * v); }
    return s;
}
static float tone (const std::vector<float>& x, double hz, double t0, double t1)
{
    double re = 0, im = 0; const int a = (int) (t0 * SR), b = (int) (t1 * SR);
    for (int i = a; i < b; ++i) { const double w = 0.5 - 0.5 * std::cos (6.283185307 * (i - a) / (b - a)); re += x[(size_t) i] * w * std::cos (6.283185307 * hz * i / SR); im += x[(size_t) i] * w * std::sin (6.283185307 * hz * i / SR); }
    return (float) (20 * std::log10 (std::sqrt (re * re + im * im) / (b - a) * 4 + 1e-12));
}
static float rms (const std::vector<float>& x, double t0, double t1)
{
    double e = 0; int n = 0;
    for (int i = (int) (t0 * SR); i < (int) (t1 * SR) && i < (int) x.size(); ++i) { e += (double) x[(size_t) i] * x[(size_t) i]; ++n; }
    return (float) (10 * std::log10 (e / std::max (1, n) + 1e-14));
}
static float lufsOf (const Stereo& s)
{
    LoudnessMeter m; m.prepare (SR);
    for (size_t pos = 0; pos < s.first.size(); pos += 4096) m.process (s.first.data() + pos, s.second.data() + pos, (int) std::min<size_t> (4096, s.first.size() - pos));
    return m.integrated.load();
}
// "cancion" sintetica: bombo, bajo, ruido de mezcla (estereo) y voz
static Stereo song (double secs, float level, int seed = 1)
{
    juce::Random r (seed);
    Stereo s; const int n = (int) (SR * secs);
    float b0 = 0, b1 = 0, b2 = 0, c0 = 0;
    for (int i = 0; i < n; ++i)
    {
        const double t = i / SR;
        const double beat = std::fmod (t, 0.5);
        const float kick = (float) (std::sin (6.283185307 * (50 + 80 * std::exp (-beat * 30)) * beat) * std::exp (-beat * 9));
        const float bass = 0.35f * (float) std::sin (6.283185307 * 55 * t);
        const float w = r.nextFloat() * 2 - 1, w2 = r.nextFloat() * 2 - 1;
        b0 = 0.99765f * b0 + w * 0.099046f; b1 = 0.963f * b1 + w * 0.2965164f; b2 = 0.57f * b2 + w * 1.0526913f;
        c0 = 0.9f * c0 + w2 * 0.3f;
        const float pad = (b0 + b1 + b2) * 0.08f;
        const float voice = 0.15f * (float) std::sin (6.283185307 * 330 * t + 2 * std::sin (6.283 * 5 * t)) * (0.6f + 0.4f * (float) std::sin (6.283 * 0.5 * t));
        s.first.push_back (level * (kick + bass + pad + voice + c0 * 0.05f));
        s.second.push_back (level * (kick + bass + pad * 0.9f + voice - c0 * 0.05f));
    }
    return s;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;

    if (argc > 2 && juce::String (argv[1]) == "--snapshot")
    {
        TopMasterProcessor p;
        auto in = song (12.0, 0.25f);
        p.prepareToPlay (SR, 512);
        p.listening.store (false);
        p.startListening(); run (p, in, false); p.runAssistantNow();
        run (p, in, false);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        auto* te = dynamic_cast<TopMasterEditor*> (ed.get());
        ed->setVisible (true);
        const int mod = argc > 3 ? juce::String (argv[3]).getIntValue() : 0;
        te->selectModule (mod);
        if (argc > 4) te->showAssistant (true);
        for (int i = 0; i < 4; ++i) te->tickNow();
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
        f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("snapshot -> %s\n", f.getFullPathName().toRawUTF8());
        return 0;
    }

    // 1. por defecto (EQ plano + maximizador sin ganancia) = transparente
    {
        TopMasterProcessor p; auto in = song (3.0, 0.2f);
        auto out = run (p, in);
        const int lat = p.getLatencySamples();
        float err = 0; for (size_t i = 0; i + (size_t) lat < out.first.size(); ++i) err = std::max (err, std::abs (out.first[i + (size_t) lat] - in.first[i]));
        report ("Por defecto: transparente", err < 1e-6f, "error " + juce::String (err, 8) + ", latencia " + juce::String (lat));
    }
    // 2. medidor LUFS (seno 1 kHz a -20 dBFS en los dos canales ~ -20 LUFS)
    {
        const float l = lufsOf (sine (1000, 0.1f, 5.0));
        report ("Medidor LUFS (1 kHz -20 dBFS)", std::abs (l + 20.0f) < 0.6f, juce::String (l, 2) + " LUFS");
    }
    // 3. maximizador: techo true peak y mas volumen
    {
        TopMasterProcessor p; setP (p, "max_gain", 10); setP (p, "max_ceil", -1.0f);
        auto in = song (6.0, 0.3f);
        auto out = run (p, in);
        LoudnessMeter m; m.prepare (SR);
        for (size_t pos = 0; pos < out.first.size(); pos += 4096) m.process (out.first.data() + pos, out.second.data() + pos, (int) std::min<size_t> (4096, out.first.size() - pos));
        const float tp = m.truePeak.load(), gain = m.integrated.load() - lufsOf (in);
        report ("Maximizador: true peak <= techo", tp <= -0.9f, juce::String (tp, 2) + " dBTP");
        report ("Maximizador: mas volumen", gain > 5.0f, "+" + juce::String (gain, 1) + " LU");
    }
    // 4. EQ: +6 dB en 1 kHz, y banda solo Side no toca un mono
    {
        TopMasterProcessor p; setP (p, "eq_b4_f", 1000); setP (p, "eq_b4_g", 6); setP (p, "eq_b4_q", 1);
        auto in = sine (1000, 0.1f, 2.0);
        auto out = run (p, in);
        const float d = tone (out.first, 1000, 1.0, 1.9) - tone (in.first, 1000, 1.0, 1.9);
        report ("EQ: +6 dB en 1 kHz", std::abs (d - 6.0f) < 0.3f, juce::String (d, 2) + " dB");
        TopMasterProcessor q; setP (q, "eq_b4_f", 1000); setP (q, "eq_b4_g", 6); setP (q, "eq_b4_ch", 2);
        auto o2 = run (q, in);
        const float d2 = tone (o2.first, 1000, 1.0, 1.9) - tone (in.first, 1000, 1.0, 1.9);
        report ("EQ en Side no toca el centro", std::abs (d2) < 0.1f, juce::String (d2, 3) + " dB");
    }
    // 5. dinamica multibanda
    {
        TopMasterProcessor p; setP (p, "dyn_on", 1); setP (p, "dyn_l_thr", -30); setP (p, "dyn_l_ratio", 4);
        auto in = sine (60, 0.5f, 3.0);
        auto out = run (p, in);
        const float d = tone (out.first, 60, 2.0, 2.9) - tone (in.first, 60, 2.0, 2.9);
        report ("Dinamica: comprime los graves", d < -6.0f && p.chain->dyn.grLive[0].load() > 6.0f, juce::String (d, 1) + " dB, GR " + juce::String (p.chain->dyn.grLive[0].load(), 1));
    }
    // 6. imagen estereo
    {
        juce::Random r (3);
        Stereo in; for (int i = 0; i < (int) (SR * 3); ++i) { const float a = r.nextFloat() - 0.5f, b = r.nextFloat() - 0.5f; in.first.push_back (0.2f * (a + 0.3f * b)); in.second.push_back (0.2f * (a - 0.3f * b)); }
        auto side = [] (const Stereo& s) { std::vector<float> v; for (size_t i = 0; i < s.first.size(); ++i) v.push_back (0.5f * (s.first[i] - s.second[i])); return v; };
        auto mid = [] (const Stereo& s) { std::vector<float> v; for (size_t i = 0; i < s.first.size(); ++i) v.push_back (0.5f * (s.first[i] + s.second[i])); return v; };
        TopMasterProcessor a; setP (a, "img_on", 1); for (int b = 1; b <= 4; ++b) setP (a, "img_w" + juce::String (b), 100);
        TopMasterProcessor m; setP (m, "img_on", 1); for (int b = 1; b <= 4; ++b) setP (m, "img_w" + juce::String (b), -100);
        const float s0 = rms (side (in), 1, 2.8), sw = rms (side (run (a, in)), 1, 2.8), sm = rms (side (run (m, in)), 1, 2.8);
        report ("Imagen: ancho +100 / -100 (mono)", sw > s0 + 5 && sm < s0 - 30, juce::String (sw - s0, 1) + " dB / " + juce::String (sm - s0, 1) + " dB");
        TopMasterProcessor z; setP (z, "img_on", 1); setP (z, "img_stereoize", 100);
        auto mono = sine (1000, 0.2f, 3.0);
        auto oz = run (z, mono);
        const float sideZ = rms (side (oz), 1, 2.8), midD = rms (mid (oz), 1, 2.8) - rms (mid (mono), 1, 2.8);
        report ("Estereizar: abre un mono sin cambiar el mono", sideZ > -40 && std::abs (midD) < 0.2f, "lados " + juce::String (sideZ, 1) + " dB, centro " + juce::String (midD, 2) + " dB");
    }
    // 7. control de graves: graves en mono
    {
        TopMasterProcessor p; setP (p, "bc_on", 1); setP (p, "bc_mono", 1); setP (p, "bc_monofreq", 120);
        auto in = sine (50, 0.3f, 3.0, -0.3f);   // fase opuesta: todo en los lados
        auto out = run (p, in);
        std::vector<float> s; for (size_t i = 0; i < out.first.size(); ++i) s.push_back (0.5f * (out.first[i] - out.second[i]));
        const float d = rms (s, 1.5, 2.8) - 20 * std::log10 (0.3f / 1.414f);
        report ("Control de graves: mono debajo de 120 Hz", d < -20.0f, juce::String (d, 1) + " dB de lados");
    }
    // 8. excitador: agrega armonicos
    {
        TopMasterProcessor p; setP (p, "exc_on", 1); setP (p, "exc_a3", 70); setP (p, "exc_m3", 0);
        auto in = sine (4000, 0.2f, 2.0);
        auto out = run (p, in);
        const float h2 = tone (out.first, 8000, 1.0, 1.9) - tone (out.first, 4000, 1.0, 1.9);
        const float h2in = tone (in.first, 8000, 1.0, 1.9) - tone (in.first, 4000, 1.0, 1.9);
        report ("Excitador: armonicos en agudos", h2 > -45.0f && h2in < -80.0f, "2o armonico " + juce::String (h2, 1) + " dB");
    }
    // 9. cinta
    {
        TopMasterProcessor p; setP (p, "tape_on", 1); setP (p, "tape_drive", 80);
        auto in = sine (200, 0.4f, 2.0);
        auto out = run (p, in);
        const float h3 = tone (out.first, 600, 1.0, 1.9) - tone (out.first, 200, 1.0, 1.9);
        const float lv = rms (out.first, 1, 1.9) - rms (in.first, 1, 1.9);
        report ("Cinta: saturacion con nivel parecido", h3 > -50 && std::abs (lv) < 3.0f, "3er arm. " + juce::String (h3, 1) + " dB, nivel " + juce::String (lv, 1) + " dB");
    }
    // 10. deslimitador: recupera transientes de una mezcla aplastada
    {
        auto in = song (4.0, 0.9f);
        for (auto* ch : { &in.first, &in.second }) for (auto& v : *ch) v = std::tanh (v * 3.0f) * 0.8f;
        auto crest = [] (const std::vector<float>& x) { float pk = 0; for (size_t i = (size_t) SR; i < x.size(); ++i) pk = std::max (pk, std::abs (x[i])); return 20 * std::log10 (pk) - rms (x, 1, 3.9); };
        TopMasterProcessor p; setP (p, "unl_on", 1); setP (p, "unl_amount", 100); setP (p, "unl_sens", 70);
        auto out = run (p, in);
        report ("Deslimitador: mas factor de cresta", crest (out.first) > crest (in.first) + 1.5f, juce::String (crest (in.first), 1) + " -> " + juce::String (crest (out.first), 1) + " dB");
    }
    // 11. claridad: sube una zona hundida
    {
        juce::Random r (4);
        Stereo in; tvc::Biquad notch; notch.setPeak (SR, 2000, 2.0, -12);
        for (int i = 0; i < (int) (SR * 4); ++i) { const float v = notch.process ((r.nextFloat() - 0.5f) * 0.3f); in.first.push_back (v); in.second.push_back (v); }
        TopMasterProcessor p; setP (p, "cla_on", 1); setP (p, "cla_amount", 100); setP (p, "cla_max", 9);
        auto out = run (p, in);
        auto band = [] (const std::vector<float>& x, double hz) { tvc::Biquad b; b.setBandPass (SR, hz, 4); std::vector<float> y; for (float v : x) y.push_back (b.process (v)); return rms (y, 2, 3.9); };
        const float d = (band (out.first, 2000) - band (in.first, 2000)) - (band (out.first, 600) - band (in.first, 600));
        report ("Claridad: sube la zona tapada", d > 1.0f, "+" + juce::String (d, 1) + " dB en 2 kHz");
    }
    // 12. moldeador espectral: baja una resonancia
    {
        juce::Random r (5);
        Stereo in; for (int i = 0; i < (int) (SR * 3); ++i) { const float v = (r.nextFloat() - 0.5f) * 0.1f + 0.1f * (float) std::sin (6.283185307 * 3000 * i / SR); in.first.push_back (v); in.second.push_back (v); }
        TopMasterProcessor p; setP (p, "ss_on", 1); setP (p, "ss_amount", 8);
        auto out = run (p, in);
        const float d = tone (out.first, 3000, 1.5, 2.9) - tone (in.first, 3000, 1.5, 2.9);
        report ("Moldeador espectral: baja la resonancia", d < -3.0f, juce::String (d, 1) + " dB");
    }
    // 13. dither a 16 bits
    {
        TopMasterProcessor p; setP (p, "dit_on", 1); setP (p, "dit_bits", 0);
        auto out = run (p, sine (440, 0.3f, 1.0));
        bool ok = true; for (size_t i = 0; i < out.first.size(); ++i) { const float q = out.first[i] * 32768.0f; if (std::abs (q - std::round (q)) > 1e-3f) { ok = false; break; } }
        report ("Dither: salida en 16 bits", ok, "");
    }
    // 14. EQ dinamico: corta solo cuando la banda pasa el umbral
    {
        TopMasterProcessor p; setP (p, "deq_on", 1); setP (p, "deq_b1_f", 100); setP (p, "deq_b1_thr", -30); setP (p, "deq_b1_range", -8);
        for (int b = 2; b <= 4; ++b) setP (p, "deq_b" + juce::String (b) + "_on", 0);
        auto loud = run (p, sine (100, 0.5f, 2.0));
        const float dl = tone (loud.first, 100, 1.2, 1.9) - tone (sine (100, 0.5f, 2.0).first, 100, 1.2, 1.9);
        TopMasterProcessor q; setP (q, "deq_on", 1); setP (q, "deq_b1_f", 100); setP (q, "deq_b1_thr", -30); setP (q, "deq_b1_range", -8);
        for (int b = 2; b <= 4; ++b) setP (q, "deq_b" + juce::String (b) + "_on", 0);
        auto soft = run (q, sine (100, 0.005f, 2.0));
        const float ds = tone (soft.first, 100, 1.2, 1.9) - tone (sine (100, 0.005f, 2.0).first, 100, 1.2, 1.9);
        report ("EQ dinamico: corta solo lo fuerte", dl < -5.0f && std::abs (ds) < 0.5f, "fuerte " + juce::String (dl, 1) + " dB, suave " + juce::String (ds, 2) + " dB");
    }
    // 15. orden y estado
    {
        TopMasterProcessor p;
        p.moveModule (EQ, -1);
        setP (p, "eq_b3_g", 4.5f);
        p.genre = 3; p.target = 2;
        juce::MemoryBlock mb; p.getStateInformation (mb);
        TopMasterProcessor q; q.setStateInformation (mb.getData(), (int) mb.getSize());
        report ("Orden y ajustes se guardan", q.getOrder() == p.getOrder() && q.getOrder()[0] == EQ && std::abs (q.apvts.getRawParameterValue ("eq_b3_g")->load() - 4.5f) < 0.01f && q.genre == 3,
                "primer modulo: " + moduleName (q.getOrder()[0]));
    }
    // 16. asistente de mastering: llega al volumen objetivo
    {
        TopMasterProcessor p; p.prepareToPlay (SR, 512);
        auto in = song (14.0, 0.12f);
        const float before = lufsOf (in);
        for (int g : { 0, 2 })
        {
            p.genre = g; p.target = 0; p.intensity = 1;
            p.startListening(); run (p, in, false);
            const bool ok = p.runAssistantNow();
            auto out = run (p, in);
            const float l = lufsOf (out);
            report (g == 0 ? "Asistente (Pop, -14 LUFS)" : "Asistente (Reggaeton, -14 LUFS)", ok && std::abs (l + 14.0f) < 1.2f && p.apvts.getRawParameterValue ("max_on")->load() > 0.5f,
                    juce::String (before, 1) + " -> " + juce::String (l, 1) + " LUFS, maximizador +" + juce::String (p.apvts.getRawParameterValue ("max_gain")->load(), 1) + " dB");
        }
        std::printf ("    %s\n", p.assistantReport.replace ("\n", "\n    ").toRawUTF8());
        // con objetivo "muy alto"
        p.target = 2; p.startListening(); run (p, in, false); p.runAssistantNow();
        const float l2 = lufsOf (run (p, in));
        report ("Asistente (-9 LUFS)", std::abs (l2 + 9.0f) < 1.5f, juce::String (l2, 1) + " LUFS");
    }
    // 17. referencia: carga, igualar tono/volumen y escuchar A/B
    {
        auto refAudio = song (8.0, 0.5f, 9);
        tvc::Biquad hs; hs.setHighShelf (SR, 6000, 6.0);
        tvc::Biquad hs2; hs2.setHighShelf (SR, 6000, 6.0);
        for (auto& v : refAudio.first) v = hs.process (v);
        for (auto& v : refAudio.second) v = hs2.process (v);
        const auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("tm_ref.wav");
        {
            f.deleteFile();
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (f.createOutputStream().release(), SR, 2, 24, {}, 0));
            const float* ch[2] { refAudio.first.data(), refAudio.second.data() };
            w->writeFromFloatArrays (ch, 2, (int) refAudio.first.size());
        }
        TopMasterProcessor p; p.prepareToPlay (SR, 512);
        const bool loaded = p.loadReference (f);
        report ("Carga la referencia", loaded && std::abs (p.getReferenceLufs() - lufsOf (refAudio)) < 0.3f, juce::String (p.getReferenceLufs(), 1) + " LUFS");
        auto in = song (14.0, 0.12f);
        p.useReference = true;
        p.startListening(); run (p, in, false); p.runAssistantNow();
        const float l = lufsOf (run (p, in));
        const float hiShelf = p.apvts.getRawParameterValue ("eq_b7_g")->load();
        report ("Asistente con referencia: volumen y brillo", std::abs (l - p.getReferenceLufs()) < 1.5f && hiShelf > 1.0f,
                juce::String (l, 1) + " LUFS (ref " + juce::String (p.getReferenceLufs(), 1) + "), agudos +" + juce::String (hiShelf, 1) + " dB");
        p.referenceOn.store (true);
        auto ab = run (p, song (2.0, 0.12f), false);
        report ("A/B: suena la referencia", rms (ab.first, 0.5, 1.9) > -40.0f, juce::String (rms (ab.first, 0.5, 1.9), 1) + " dB");
        f.deleteFile();
    }
    // 18. bypass igualado: mismo volumen que lo procesado
    {
        TopMasterProcessor p; setP (p, "max_gain", 8);
        auto in = song (8.0, 0.15f);
        auto proc = run (p, in);
        setP (p, "bypass", 1);
        auto byp = run (p, in, false);
        const float d = rms (byp.first, 6, 7.9) - rms (proc.first, 6, 7.9);
        report ("Bypass igualado al volumen procesado", std::abs (d) < 1.5f, juce::String (d, 2) + " dB de diferencia");
    }
    std::printf (fails == 0 ? "TODAS LAS PRUEBAS OK\n" : "%d PRUEBAS FALLARON\n", fails);
    return fails == 0 ? 0 : 1;
}
