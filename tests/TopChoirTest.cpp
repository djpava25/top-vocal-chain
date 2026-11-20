// Pruebas de Top Choir: armonias correctas en la tonalidad, octavas, estereo, archivo WAV y captura
#include "TopChoirProcessor.h"
#include "TopChoirEditor.h"
#include <cstdio>

static int fails = 0;
static void report (const char* n, bool ok, const juce::String& info) { if (! ok) ++fails; std::printf ("%-44s %s  %s\n", n, info.toRawUTF8(), ok ? "OK" : "FALLO"); }
static const double SR = 44100.0;

// Voz sintetica: notas sostenidas A4 (440) y E4 (329.6), con armonicos tipo vocal
static juce::AudioBuffer<float> makeVoice (double secs)
{
    juce::AudioBuffer<float> b (1, (int) (SR * secs));
    double ph = 0;
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const double t = i / SR;
        const double hz = t < secs * 0.5 ? 440.0 : 329.63;
        ph += hz / SR;
        double v = 0; for (int h = 1; h <= 12; ++h) v += std::sin (6.283185307 * h * ph) / h * (h == 3 ? 1.6 : 1.0);
        const double env = std::min (1.0, std::min (t * 20, (secs - t) * 20));
        b.setSample (0, i, (float) (0.15 * v * env));
    }
    return b;
}
// energia en una frecuencia (ventana Hann)
static double bandDb (const juce::AudioBuffer<float>& b, int ch, double hz, double t0, double t1)
{
    const int a = (int) (t0 * SR), e = (int) (t1 * SR);
    double re = 0, im = 0;
    for (int i = a; i < e; ++i) { const double w = 0.5 - 0.5 * std::cos (6.283185307 * (i - a) / (e - a)); const double ph = 6.283185307 * hz * i / SR; re += b.getSample (ch, i) * w * std::cos (ph); im += b.getSample (ch, i) * w * std::sin (ph); }
    return 20 * std::log10 (std::sqrt (re * re + im * im) / (e - a) + 1e-12);
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto voice = makeVoice (3.0);
    std::atomic<float> prog { 0 }; std::atomic<bool> cancel { false };

    // 1. Tonalidad detectada (A mayor / F# menor relativa: raiz A o F#)
    {
        int root = -1; bool minor = false;
        const bool ok = choir::detectKey (voice, SR, root, minor);
        report ("Detecta la tonalidad", ok && (root == 9 || root == 4 || root == 6), juce::String::formatted ("raiz %d %s", root, minor ? "menor" : "mayor"));
    }
    // 2. Tercera arriba en La mayor: sobre A4 (440) -> C#5 (554.4); sobre E4 (329.6) -> G#4 (415.3)
    {
        choir::Settings s; s.key = 9; s.scale = 1; s.thirdUp = true; s.includeUnison = false; s.model = 3;
        auto out = choir::render (voice, SR, s, prog, cancel);
        const double cs = bandDb (out, 0, 554.37, 0.4, 1.3), wrong = bandDb (out, 0, 523.25, 0.4, 1.3);
        const double gs = bandDb (out, 0, 415.30, 1.9, 2.8), wrong2 = bandDb (out, 0, 392.0, 1.9, 2.8);
        report ("Tercera arriba (diatonica en La mayor)", cs > wrong + 15 && gs > wrong2 + 15,
                juce::String::formatted ("C#5 %.1f vs C5 %.1f dB | G#4 %.1f vs G4 %.1f dB", cs, wrong, gs, wrong2));
    }
    // 3. Quinta abajo y octava arriba
    {
        choir::Settings s; s.key = 9; s.scale = 1; s.fifthDown = true; s.octUp = true; s.includeUnison = false; s.model = 3;
        auto out = choir::render (voice, SR, s, prog, cancel);
        const double d5 = bandDb (out, 0, 293.66, 0.4, 1.3), oct = bandDb (out, 0, 880.0, 0.4, 1.3), off = bandDb (out, 0, 311.1, 0.4, 1.3);
        report ("Quinta abajo (D4) y octava arriba (A5)", d5 > off + 15 && oct > off + 10, juce::String::formatted ("D4 %.1f, A5 %.1f, D#4 %.1f dB", d5, oct, off));
    }
    // 4. Estereo vs mono y nivel
    {
        choir::Settings st; st.model = 0; choir::Settings mo = st; mo.stereo = false;
        auto a = choir::render (voice, SR, st, prog, cancel), b = choir::render (voice, SR, mo, prog, cancel);
        double dS = 0, dM = 0; for (int i = 0; i < a.getNumSamples(); ++i) { dS += std::abs (a.getSample (0, i) - a.getSample (1, i)); dM += std::abs (b.getSample (0, i) - b.getSample (1, i)); }
        const float pk = a.getMagnitude (0, 0, a.getNumSamples());
        bool finite = true; for (int i = 0; i < a.getNumSamples(); ++i) finite &= std::isfinite (a.getSample (0, i));
        report ("Coro estereo abierto / mono centrado", dS > 50 * dM + 1 && finite && pk < 0.75f && pk > 0.5f, juce::String::formatted ("dif L-R st %.1f, mono %.3f, pico %.2f", dS, dM, pk));
    }
    // 5. Plugin completo: carga WAV, crea voces y guarda el archivo
    {
        auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("voz_prueba.wav");
        tmp.deleteFile();
        { juce::WavAudioFormat wav; std::unique_ptr<juce::FileOutputStream> os (tmp.createOutputStream());
          std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), SR, 1, 24, {}, 0)); os.release(); w->writeFromAudioSampleBuffer (voice, 0, voice.getNumSamples()); }
        TopChoirProcessor p; p.setPlayConfigDetails (2, 2, 48000, 512); p.prepareToPlay (48000, 512);
        const bool loaded = p.loadVoice (tmp);
        auto set = [&] (const char* id, float v) { auto* x = p.apvts.getParameter (id); x->setValueNotifyingHost (x->convertTo0to1 (v)); };
        set ("third_up", 1); set ("oct_down", 1);
        p.createVocals();
        for (int i = 0; i < 600 && p.isRendering(); ++i) juce::Thread::sleep (100);
        const auto f = p.getChoirFile();
        report ("Crea el coro y guarda el WAV", loaded && p.hasChoir() && f.existsAsFile() && f.getSize() > 10000, f.getFullPathName());
        // reproduccion a otra frecuencia de muestreo del DAW
        p.playing = true;
        juce::AudioBuffer<float> b (2, 512); juce::MidiBuffer m; float mx = 0;
        for (int k = 0; k < 100; ++k) { b.clear(); p.processBlock (b, m); mx = std::max (mx, b.getMagnitude (0, 0, 512)); }
        report ("Se escucha desde el plugin", mx > 0.05f, juce::String::formatted ("pico %.2f", mx));
        if (argc > 2 && juce::String (argv[1]) == "--snapshot")
        {
            std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
            ed->setVisible (true);
            if (auto* te = dynamic_cast<TopChoirEditor*> (ed.get())) te->tickNow();
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
            out.deleteFile(); juce::FileOutputStream os (out); juce::PNGImageFormat().writeImageToStream (img, os);
            std::printf ("snapshot -> %s\n", out.getFullPathName().toRawUTF8());
        }
        p.playing = false;
        f.deleteFile(); tmp.deleteFile();
    }
    std::printf (fails == 0 ? "TODAS LAS PRUEBAS OK\n" : "%d FALLOS\n", fails);
    return fails;
}
