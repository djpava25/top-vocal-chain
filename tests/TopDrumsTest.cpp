// Pruebas offline de Top Drums
#include "TopDrumsProcessor.h"
#include "TopDrumsEditor.h"
#include <cstdio>

static int fails = 0;
static const double SR = 48000.0;
static const double twoPi = 2 * juce::MathConstants<double>::pi;

static void report (const char* name, bool ok, const juce::String& info)
{
    if (! ok) ++fails;
    std::printf ("%-46s %s  %s\n", name, info.toRawUTF8(), ok ? "OK" : "FALLO");
}
static double db (double v) { return 20.0 * std::log10 (v + 1e-12); }

// Bateria sintetica: kick (55 Hz con caida de tono) cada 0.5 s, snare en contratiempo, hats cada 1/8
static float drums (int i, double sr, float hats = 1.0f, float sub = 1.0f)
{
    auto noise = [] (int k) { uint32_t x = (uint32_t) k * 2654435761u; x ^= x >> 13; x *= 0x5bd1e995u; x ^= x >> 15; return (double) (x & 0xffff) / 65535.0 - 0.5; };
    const double t = i / sr;
    const double tk = std::fmod (t, 0.5), ts = std::fmod (t + 0.25, 0.5), th = std::fmod (t, 0.125);
    const double fk = 50.0 + 90.0 * std::exp (-tk * 40.0);
    double k = std::sin (twoPi * fk * tk) * std::exp (-tk * 9.0) * sub;
    double s = (noise (i) * 1.2 + 0.5 * std::sin (twoPi * 190 * ts)) * std::exp (-ts * 22.0) * (std::fmod (t, 1.0) >= 0.5 ? 1.0 : 0.0);
    double h = noise (i + 77777) * 0.35 * std::exp (-th * 70.0) * hats;
    return (float) (0.5 * k + 0.35 * s + h * 0.6);
}

static juce::AudioBuffer<float> run (TopDrumsProcessor& p, std::vector<std::pair<const char*, float>> prm, std::function<float (int)> gen, double secs = 2.0)
{
    p.setPlayConfigDetails (2, 2, SR, 512);
    for (auto& [id, v] : prm) { auto* x = p.apvts.getParameter (id); x->setValueNotifyingHost (x->convertTo0to1 (v)); }
    p.prepareToPlay (SR, 512);
    const int total = (int) (SR * secs);
    juce::AudioBuffer<float> b (2, total);
    for (int i = 0; i < total; ++i) { const float v = gen (i); b.setSample (0, i, v); b.setSample (1, i, v); }
    juce::MidiBuffer m;
    for (int pos = 0; pos < total; pos += 512)
    {
        juce::AudioBuffer<float> sub (b.getArrayOfWritePointers(), 2, pos, std::min (512, total - pos));
        p.processBlock (sub, m);
    }
    return b;
}

static double rms (const juce::AudioBuffer<float>& b, int from, int len) { return b.getRMSLevel (0, from, len); }
static double crest (const juce::AudioBuffer<float>& b, int from, int len) { return db (b.getMagnitude (0, from, len)) - db (rms (b, from, len)); }

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    auto dr = [] (int i) { return drums (i, SR); };
    if (argc > 2 && juce::String (argv[1]) == "--snapshot")
    {
        TopDrumsProcessor p;
        p.setCurrentProgram (1);
        run (p, {}, dr, 1.0);
        p.startAnalysis();
        run (p, {}, dr, 9.0);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        ed->setVisible (true);
        if (auto* te = dynamic_cast<TopDrumsEditor*> (ed.get())) for (int i = 0; i < 20; ++i) te->tickNow();
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1500.0f / (float) ed->getWidth());
        juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
        f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("snapshot -> %s\n", f.getFullPathName().toRawUTF8());
        return 0;
    }
    const int from = (int) (SR * 0.5), len = (int) (SR * 1.5);
    TopDrumsProcessor ref; auto dry = run (ref, { { "power", 0 } }, dr);
    const int lat = ref.getLatencySamples();

    // 1. Neutro (por defecto todo en 0 y glue apagado): igual que la entrada en todo el rango audible (< 20 kHz)
    {
        TopDrumsProcessor p; auto o = run (p, {}, dr);
        tvc::Biquad la, lb; la.setLowPass (SR, 18000, 0.707); lb.setLowPass (SR, 18000, 0.707);
        double e = 0, s = 0;
        for (int i = 0; i < from + len; ++i)
        {
            const double x = la.process (o.getSample (0, i)), y = lb.process (dry.getSample (0, i));
            if (i >= from) { e += (x - y) * (x - y); s += y * y; }
        }
        const double errDb = 10 * std::log10 (e / s + 1e-20);
        report ("Neutro = transparente (audible)", errDb < -50, juce::String::formatted ("diferencia %.1f dB (latencia %d)", errDb, lat));
    }
    // 2. Attack: + mas pegada, - menos
    {
        TopDrumsProcessor a, b;
        const double c0 = crest (dry, from, len), cp = crest (run (a, { { "ts_attack", 100 } }, dr), from, len), cm = crest (run (b, { { "ts_attack", -100 } }, dr), from, len);
        report ("ATTACK +/- cambia la pegada", cp > c0 + 2 && cm < c0 - 1, juce::String::formatted ("pegada %.1f -> +100: %.1f, -100: %.1f dB", c0, cp, cm));
    }
    // 3. Sustain: + mas cola
    {
        TopDrumsProcessor a;
        auto o = run (a, { { "ts_sustain", 100 } }, dr);
        // energia en la cola del kick (150-450 ms despues de cada golpe, sin snare: segundo 0.5..1.0 tiene kick en 0.5)
        auto tail = [&] (const juce::AudioBuffer<float>& b) { return db (b.getRMSLevel (0, (int) (SR * 1.15) + lat, (int) (SR * 0.08))); };
        report ("SUSTAIN + alarga la cola", tail (o) > tail (dry) + 3, juce::String::formatted ("cola %.1f -> %.1f dB", tail (dry), tail (o)));
    }
    // 4. Weight: mas graves
    {
        TopDrumsProcessor a;
        auto lowE = [&] (const juce::AudioBuffer<float>& b) { tvc::Biquad lp; lp.setLowPass (SR, 100, 0.707); double s = 0; for (int i = from; i < from + len; ++i) { const float v = lp.process (b.getSample (0, i)); s += v * v; } return 10 * std::log10 (s / len + 1e-12); };
        auto o = run (a, { { "weight", 100 } }, dr);
        report ("WEIGHT sube el peso", lowE (o) > lowE (dry) + 4, juce::String::formatted ("graves %.1f -> %.1f dB", lowE (dry), lowE (o)));
    }
    // 5. Glue: con ataque rapido comprime y baja la pegada; con ataque lento deja pasar el golpe (mas punch)
    {
        TopDrumsProcessor a, b;
        auto fast = run (a, { { "glue_on", 1 }, { "glue_thr", -30 }, { "glue_ratio", 2 }, { "glue_att", 0 } }, dr);
        auto slow = run (b, { { "glue_on", 1 }, { "glue_thr", -30 }, { "glue_ratio", 2 }, { "glue_att", 5 } }, dr);
        const double gr = db (rms (fast, from, len)) - db (rms (dry, from, len));
        report ("GLUE comprime", gr < -6 && crest (fast, from, len) < crest (dry, from, len),
                juce::String::formatted ("nivel %+.1f dB, pegada %.1f -> %.1f", gr, crest (dry, from, len), crest (fast, from, len)));
        report ("GLUE ataque lento = mas punch", crest (slow, from, len) > crest (fast, from, len) + 2,
                juce::String::formatted ("pegada rapido %.1f, lento %.1f dB", crest (fast, from, len), crest (slow, from, len)));
    }
    // 6. Clip: picos controlados
    {
        TopDrumsProcessor a;
        auto o = run (a, { { "clip", 1 }, { "out", 12 } }, dr);
        report ("CLIP controla los picos", o.getMagnitude (0, from, len) <= 1.0f, juce::String::formatted ("pico %.3f con +12 dB", o.getMagnitude (0, from, len)));
    }
    // 7. Drive: sin explotar el volumen
    {
        TopDrumsProcessor a;
        auto o = run (a, { { "drive", 100 } }, dr);
        const double d = db (rms (o, from, len)) - db (rms (dry, from, len));
        report ("DRIVE mantiene el volumen", std::abs (d) < 4, juce::String::formatted ("cambio de nivel %+.1f dB", d));
    }
    // 8. Presets: todos funcionan y suenan con nivel razonable
    {
        bool ok = true; juce::String info;
        for (int i = 0; i < TopDrumsProcessor().getNumPrograms(); ++i)
        {
            TopDrumsProcessor p; p.setPlayConfigDetails (2, 2, SR, 512); p.setCurrentProgram (i);
            auto o = run (p, {}, dr, 1.5);
            const double pk = db (o.getMagnitude (0, 0, o.getNumSamples()));
            bool finite = true; for (int s = 0; s < o.getNumSamples(); ++s) finite &= std::isfinite (o.getSample (0, s));
            if (! finite || pk > 1.0 || pk < -30) ok = false;
            info << juce::String (pk, 1) << " ";
        }
        report ("Presets por genero (pico dBFS)", ok, info);
    }
    // 9. IA local: analiza y recomienda segun el genero
    {
        auto analyse = [&] (int genre, float sub)
        {
            TopDrumsProcessor p;
            run (p, { { "genre", (float) genre } }, [sub] (int i) { return drums (i, SR, 1.0f, sub); }, 0.5);
            p.startAnalysis();
            juce::AudioBuffer<float> b (2, 512); juce::MidiBuffer m; int k = 0;
            while (! p.analysisReady() && k < 2000)
            {
                for (int i = 0; i < 512; ++i) { const float v = drums (k * 512 + i, SR, 1.0f, sub); b.setSample (0, i, v); b.setSample (1, i, v); }
                p.processBlock (b, m); ++k;
            }
            return p.getAnalysis();
        };
        const auto heavy = analyse (1, 0.1f), salsa = analyse (8, 0.1f);
        auto wantsWeight = [] (const TopDrumsProcessor::Analysis& a) { for (auto& s : a.settings) if (s.first == "weight" && s.second > 10) return true; return false; };
        report ("IA local analiza la bateria", heavy.valid && heavy.densityPerSec > 2 && ! heavy.recommendations.isEmpty(),
                juce::String::formatted ("pegada %.1f, graves %.1f, agudos %.1f dB, %.1f golpes/s", heavy.crestDb, heavy.lowDb, heavy.highDb, heavy.densityPerSec));
        report ("IA: Bad Bunny pide peso, Salsa no", wantsWeight (heavy) && ! wantsWeight (salsa), "segun el perfil del genero");
        TopDrumsProcessor p; p.applySettings (heavy.settings);
        report ("APLICAR IA cambia los ajustes", p.apvts.getRawParameterValue ("weight")->load() > 10, "ok");
    }
    // 10. Claude: pedido JSON valido y lectura de la respuesta
    {
        TopDrumsProcessor p;
        const auto req = juce::JSON::parse (p.buildClaudeRequest ({}));
        report ("Pedido a Claude (JSON)", req.getProperty ("model", {}).toString().startsWith ("claude-") && req.getProperty ("messages", {}).isArray()
                                              && (int) req.getProperty ("max_tokens", 0) > 0, req.getProperty ("model", {}).toString());
        const juce::String reply = R"({"type":"message","content":[{"type":"text","text":"```json\n{\"consejos\":[\"Sube WEIGHT\",\"Usa GLUE\"],\"ajustes\":{\"weight\":60,\"glue_thr\":-20}}\n```"}]})";
        juce::String text; std::vector<std::pair<juce::String, float>> st;
        const bool ok = TopDrumsProcessor::parseClaudeReply (reply, text, st);
        report ("Respuesta de Claude -> consejos y ajustes", ok && st.size() == 2 && text.contains ("WEIGHT"), juce::String ((int) st.size()) + " ajustes");
        const juce::String err = R"({"type":"error","error":{"type":"authentication_error","message":"invalid x-api-key"}})";
        text = {}; st.clear();
        report ("Error de la API se muestra", ! TopDrumsProcessor::parseClaudeReply (err, text, st) && text.contains ("invalid"), text);
    }

    std::printf (fails == 0 ? "TODAS LAS PRUEBAS OK\n" : "%d FALLOS\n", fails);
    return fails == 0 ? 0 : 1;
}
