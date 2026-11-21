// Pruebas de Top Note Editor: analisis en notas, transparencia, tono, macros, vibrato, tiempo, duracion,
// formantes, volumen, deshacer, estado guardado, transferencia anclada al DAW y algoritmo percusivo
#include "TopNoteEditorProcessor.h"
#include "TopNoteEditorEditor.h"
#include <cstdio>

static int fails = 0;
static void report (const char* n, bool ok, const juce::String& info) { if (! ok) ++fails; std::printf ("%-46s %s  %s\n", n, info.toRawUTF8(), ok ? "OK" : "FALLO"); }
static const double SR = 44100.0;

struct Seg { double midi, secs, amp, vib; bool consonant; };
static std::vector<float> voice (const std::vector<Seg>& segs)
{
    std::vector<float> out;
    double ph = 0; juce::Random r (3);
    for (const auto& s : segs)
    {
        const int len = (int) (SR * s.secs);
        for (int j = 0; j < len; ++j)
        {
            const double t = j / SR;
            double v = 0;
            if (s.consonant) v = (r.nextFloat() * 2 - 1) * 0.15 * s.amp;
            else if (s.amp > 0)
            {
                const double m = s.midi + s.vib * std::sin (6.2831853 * 5.5 * t) * std::min (1.0, t * 4);
                ph += 440.0 * std::pow (2.0, (m - 69) / 12.0) / SR;
                for (int h = 1; h <= 12; ++h) v += std::sin (6.2831853 * h * ph) / h * (h == 2 || h == 3 ? 1.4 : 1.0);
                v *= s.amp * std::min (1.0, std::min (t * 40, (s.secs - t) * 40));
            }
            out.push_back ((float) v);
        }
    }
    return out;
}

static std::vector<int> pitchedNotes (const TopNoteEditorProcessor& p)
{
    std::vector<int> v;
    for (int i = 0; i < (int) p.notes.size(); ++i) if (p.notes[(size_t) i].visible && p.notes[(size_t) i].pitched) v.push_back (i);
    return v;
}
static float pitchOf (const std::vector<float>& x, double t0, double t1, float* stdev = nullptr)
{
    const auto c = ned::analyse (x.data(), (int) x.size(), SR);
    std::vector<float> v;
    for (int f = (int) (t0 * SR / c.hop); f < (int) (t1 * SR / c.hop) && f < c.frames(); ++f) if (c.midi[(size_t) f] > 0) v.push_back (c.midi[(size_t) f]);
    if (v.empty()) return 0;
    double s = 0; for (float m : v) s += m;
    const float mean = (float) (s / v.size());
    if (stdev) { double q = 0; for (float m : v) q += (m - mean) * (m - mean); *stdev = (float) std::sqrt (q / v.size()); }
    std::sort (v.begin(), v.end());
    return v[v.size() / 2];
}
static float rmsDb (const std::vector<float>& x, double t0, double t1)
{
    double e = 0; int n = 0;
    for (int i = (int) (t0 * SR); i < (int) (t1 * SR) && i < (int) x.size(); ++i) { e += (double) x[(size_t) i] * x[(size_t) i]; ++n; }
    return n > 0 ? (float) (10 * std::log10 (e / n + 1e-12)) : -120.0f;
}
static float centroid (const std::vector<float>& x, double t0, double t1)
{
    // centroide espectral aproximado por bandas
    double num = 0, den = 0;
    for (double hz = 200; hz < 6000; hz *= 1.12)
    {
        double re = 0, im = 0;
        const int a = (int) (t0 * SR), b = (int) (t1 * SR);
        for (int i = a; i < b; ++i) { const double w = 0.5 - 0.5 * std::cos (6.2831853 * (i - a) / (b - a)); re += x[(size_t) i] * w * std::cos (6.2831853 * hz * i / SR); im += x[(size_t) i] * w * std::sin (6.2831853 * hz * i / SR); }
        const double m = std::sqrt (re * re + im * im);
        num += m * hz; den += m;
    }
    return (float) (num / (den + 1e-12));
}

struct FakeHead : juce::AudioPlayHead
{
    juce::int64 pos = 0; bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo i; i.setTimeInSamples (pos); i.setIsPlaying (playing); i.setBpm (120.0);
        i.setTimeSignature (TimeSignature { 4, 4 });
        return i;
    }
};

static const std::vector<Seg> take {
    { 0, 0.15, 0, 0, false },
    { 57.0, 0.6, 0.15, 0.0, false },     // A3 afinado
    { 60.3, 0.6, 0.15, 0.35, false },    // C4 +30 cents con vibrato
    { 0, 0.15, 0, 0, false },
    { 0, 0.06, 1.0, 0, true },           // consonante "s"
    { 64.0, 0.6, 0.06, 0.0, false },     // E4 mas suave
    { 0, 0.2, 0, 0, false } };

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto src = voice (take);

    if (argc > 2 && juce::String (argv[1]) == "--snapshot")
    {
        TopNoteEditorProcessor p; p.prepareToPlay (SR, 512);
        p.loadBuffer (src, SR, 0, "Voz de prueba");
        p.waitUntilIdle (20000);
        const auto pn = pitchedNotes (p);
        if (pn.size() >= 3) { p.beginEdit(); p.notes[(size_t) pn[1]].shift = -0.3f; p.notes[(size_t) pn[2]].shift = 1.0f; p.notes[(size_t) pn[2]].t0 += 4000; p.notes[(size_t) pn[2]].t1 += 4000; p.notesChanged(); }
        p.waitUntilIdle (20000);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        auto* te = dynamic_cast<TopNoteEditorEditor*> (ed.get());
        ed->setVisible (true);
        te->tickNow();
        te->getView().fitAll();
        if (argc > 3) te->getView().tool = (tne::Tool) juce::String (argv[3]).getIntValue();
        if (pn.size() >= 3) te->getView().selectAll();
        te->tickNow();
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
        f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("snapshot -> %s\n", f.getFullPathName().toRawUTF8());
        return 0;
    }

    TopNoteEditorProcessor p; p.prepareToPlay (SR, 512);
    p.loadBuffer (src, SR, 0, "Voz de prueba");
    report ("Analisis termina", p.waitUntilIdle (20000), "");
    auto pn = pitchedNotes (p);
    juce::String cs; for (int i : pn) cs << juce::String (p.notes[(size_t) i].center, 2) << " ";
    report ("Detecta 3 notas", pn.size() == 3, juce::String ((int) pn.size()) + " notas: " + cs);
    if (pn.size() != 3) { std::printf ("%d PRUEBAS FALLARON\n", ++fails); return 1; }
    const auto& n0 = p.notes[(size_t) pn[0]]; const auto& n1 = p.notes[(size_t) pn[1]]; const auto& n2 = p.notes[(size_t) pn[2]];
    report ("Centros correctos (57 / 60.3 / 64)", std::abs (n0.center - 57) < 0.1 && std::abs (n1.center - 60.3f) < 0.12 && std::abs (n2.center - 64) < 0.1, cs);
    report ("La consonante va con su nota", n2.s0 < (int) (SR * 1.5), "inicio E4 " + juce::String (n2.s0 / SR, 3) + " s (consonante 1.50 s)");
    // particion completa
    {
        bool ok = p.notes.front().s0 == 0 && p.notes.back().s1 == (int) src.size();
        for (size_t i = 1; i < p.notes.size(); ++i) ok = ok && p.notes[i].s0 == p.notes[i - 1].s1;
        report ("Las notas cubren toda la toma", ok, juce::String ((int) p.notes.size()) + " segmentos");
    }

    // transparencia sin ediciones
    {
        const auto out = p.renderNow();
        float err = 0; for (size_t i = 0; i < src.size(); ++i) err = std::max (err, std::abs (out[i] - src[i]));
        report ("Sin ediciones = audio original", err < 1e-5f, "error max " + juce::String (err, 7));
    }
    // tono +2 semitonos en A3
    {
        p.beginEdit(); p.notes[(size_t) pn[0]].shift = 2.0f; p.notesChanged();
        const auto out = p.renderNow();
        const float m = pitchOf (out, 0.25, 0.65);
        const float other = pitchOf (out, 1.6, 1.9);
        report ("Subir A3 +2 = B3", std::abs (m - 59.0f) < 0.15f, juce::String (m, 2));
        report ("Las otras notas no cambian", std::abs (other - 64.0f) < 0.1f, juce::String (other, 2));
        p.undo();
        report ("Deshacer", p.notes[(size_t) pn[0]].shift == 0.0f && p.canRedo(), "shift " + juce::String (p.notes[(size_t) pn[0]].shift));
        p.redo();
        report ("Rehacer", p.notes[(size_t) pn[0]].shift == 2.0f, "shift " + juce::String (p.notes[(size_t) pn[0]].shift));
        p.undo();
    }
    // macro corregir afinacion + quitar vibrato
    {
        float sdBefore = 0; pitchOf (src, 0.9, 1.3, &sdBefore);
        p.macroCorrectPitch ({ pn[1] }, 1.0f, 0.0f);
        p.beginEdit(); p.notes[(size_t) pn[1]].mod = 0.0f; p.notesChanged();
        const auto out = p.renderNow();
        float sdAfter = 0;
        const float m = pitchOf (out, 0.9, 1.3, &sdAfter);
        report ("Corregir afinacion: C4 +30 ct -> C4", std::abs (m - 60.0f) < 0.08f, juce::String (m, 3) + "  (shift " + juce::String (p.notes[(size_t) pn[1]].shift, 2) + ")");
        report ("Modulacion 0 % quita el vibrato", sdAfter < sdBefore * 0.4f, "desv. " + juce::String (sdBefore, 3) + " -> " + juce::String (sdAfter, 3));
        p.undo(); p.undo();
    }
    // tiempo: mover E4 100 ms mas tarde
    {
        const int d = (int) (SR * 0.1);
        const double e0 = n2.t0 / SR;
        p.beginEdit(); p.notes[(size_t) pn[2]].t0 += d; p.notes[(size_t) pn[2]].t1 += d; p.notesChanged();
        const auto out = p.renderNow();
        const float before = rmsDb (out, e0 + 0.07, e0 + 0.09), after = rmsDb (out, e0 + 0.62, e0 + 0.66);
        const float m = pitchOf (out, e0 + 0.3, e0 + 0.5);
        report ("Mover nota en el tiempo", before < after - 20 && std::abs (m - 64) < 0.1f, juce::String (before, 1) + " dB antes / " + juce::String (after, 1) + " dB en la nueva posicion");
        p.undo();
    }
    // duracion x1.5
    {
        const auto& N = p.notes[(size_t) pn[0]];
        const int len = N.t1 - N.t0;
        p.beginEdit(); p.notes[(size_t) pn[0]].t1 = N.t0 + len * 3 / 2; p.notesChanged();
        const auto out = p.renderNow();
        const double newEnd = p.notes[(size_t) pn[0]].t1 / SR;
        const float lvl = rmsDb (out, newEnd - 0.12, newEnd - 0.05);
        const float m = pitchOf (out, newEnd - 0.4, newEnd - 0.1);
        report ("Alargar la nota (x1.5) mantiene el tono", lvl > -35 && std::abs (m - 57) < 0.15f, juce::String (lvl, 1) + " dB al final, tono " + juce::String (m, 2));
        p.undo();
    }
    // formantes
    {
        const float c0 = centroid (src, 0.3, 0.6);
        p.beginEdit(); p.notes[(size_t) pn[0]].formant = 3.0f; p.notesChanged();
        const auto out = p.renderNow();
        const float c1 = centroid (out, 0.3, 0.6), m = pitchOf (out, 0.25, 0.65);
        report ("Formantes +3: timbre mas brillante, misma nota", c1 > c0 * 1.08f && std::abs (m - 57) < 0.15f, juce::String (c0, 0) + " Hz -> " + juce::String (c1, 0) + " Hz, tono " + juce::String (m, 2));
        p.undo();
    }
    // volumen y nivelar
    {
        p.beginEdit(); p.notes[(size_t) pn[0]].gainDb = -12.0f; p.notesChanged();
        const auto out = p.renderNow();
        const float d = rmsDb (out, 0.3, 0.6) - rmsDb (src, 0.3, 0.6);
        report ("Amplitud -12 dB", std::abs (d + 12) < 0.3f, juce::String (d, 2) + " dB");
        p.undo();
        p.macroLevel ({}, 1.0f, 1.0f);
        const auto lv = p.renderNow();
        const float spreadBefore = std::abs (rmsDb (src, 0.3, 0.6) - rmsDb (src, 1.7, 2.0));
        const float spreadAfter = std::abs (rmsDb (lv, 0.3, 0.6) - rmsDb (lv, 1.7, 2.0));
        report ("Nivelar notas iguala el volumen", spreadAfter < spreadBefore * 0.35f, juce::String (spreadBefore, 1) + " dB -> " + juce::String (spreadAfter, 1) + " dB");
        p.undo();
    }
    // cuantizar tiempo (120 BPM, 1/16 = 5512 muestras)
    {
        p.macroQuantizeTime ({}, 0.25, 1.0f);
        bool ok = true;
        for (int i : pitchedNotes (p)) ok = ok && (p.notes[(size_t) i].t0 % 5512 == 0 || p.notes[(size_t) i].t0 % 5513 == 0 || std::fmod (p.notes[(size_t) i].t0, 5512.5) < 1.0);
        report ("Cuantizar a 1/16", ok, "inicio C4 " + juce::String (p.notes[(size_t) pn[1]].t0));
        p.undo();
    }
    // render en segundo plano y estado guardado
    {
        p.beginEdit(); p.notes[(size_t) pn[1]].shift = -1.0f; p.notes[(size_t) pn[2]].formant = 1.5f; p.notesChanged();
        p.waitUntilIdle (20000);
        juce::MemoryBlock mb; p.getStateInformation (mb);
        TopNoteEditorProcessor q; q.prepareToPlay (SR, 512);
        q.setStateInformation (mb.getData(), (int) mb.getSize());
        q.waitUntilIdle (20000);
        const auto qp = pitchedNotes (q);
        const bool ok = qp.size() == 3 && q.notes[(size_t) qp[1]].shift == -1.0f && q.notes[(size_t) qp[2]].formant == 1.5f && q.sourceLength() == (int) src.size();
        report ("Estado guardado (audio + ediciones)", ok, juce::String ((int) qp.size()) + " notas, " + juce::String (q.sourceLength()) + " muestras");
        // reproduccion sincronizada con el DAW
        FakeHead head; head.pos = 0; head.playing = true;
        q.setPlayHead (&head);
        juce::AudioBuffer<float> buf (2, 512);
        std::vector<float> played;
        for (int b = 0; b < (int) src.size() / 512; ++b)
        {
            buf.clear(); juce::MidiBuffer m;
            q.processBlock (buf, m);
            for (int i = 0; i < 512; ++i) played.push_back (buf.getSample (0, i));
            head.pos += 512;
        }
        const auto ref = q.renderNow();
        float err = 0; for (size_t i = 0; i < played.size(); ++i) err = std::max (err, std::abs (played[i] - ref[i]));
        report ("Reproduce las ediciones al ritmo del DAW", err < 1e-5f, "error " + juce::String (err, 7));
        q.setPlayHead (nullptr);
    }
    // TRANSFERIR anclado a la linea de tiempo (empieza en el segundo 2)
    {
        TopNoteEditorProcessor t; t.prepareToPlay (SR, 512);
        FakeHead head; head.pos = (juce::int64) (SR * 2); head.playing = true;
        t.setPlayHead (&head);
        t.setTransfer (true);
        juce::AudioBuffer<float> buf (2, 512);
        for (int pos = 0; pos + 512 <= (int) src.size(); pos += 512)
        {
            for (int ch = 0; ch < 2; ++ch) buf.copyFrom (ch, 0, src.data() + pos, 512);
            juce::MidiBuffer m; t.processBlock (buf, m);
            head.pos += 512;
        }
        t.setTransfer (false);
        t.waitUntilIdle (20000);
        const auto tp = pitchedNotes (t);
        report ("Transferir: graba y analiza", t.getOrigin() == (juce::int64) (SR * 2) && tp.size() == 3,
                "origen " + juce::String (t.getOrigin() / SR, 2) + " s, " + juce::String ((int) tp.size()) + " notas");
        // al volver a reproducir, en la zona de la toma sale el audio del plugin (aunque la pista quede muda)
        head.pos = (juce::int64) (SR * 2.3);
        buf.clear(); juce::MidiBuffer m; t.processBlock (buf, m);
        report ("Reproduce la toma en su sitio", buf.getMagnitude (0, 0, 512) > 0.05f, "pico " + juce::String (buf.getMagnitude (0, 0, 512), 3));
        t.setPlayHead (nullptr);
    }
    // algoritmo percusivo
    {
        std::vector<float> drums ((size_t) (SR * 2.0), 0.0f);
        juce::Random r (9);
        for (int k = 0; k < 6; ++k)
            for (int i = 0; i < (int) (SR * 0.08); ++i)
                drums[(size_t) (k * SR * 0.3 + SR * 0.1 + i)] = (r.nextFloat() * 2 - 1) * 0.5f * std::exp (-i / (SR * 0.015));
        TopNoteEditorProcessor d; d.prepareToPlay (SR, 512);
        d.reanalyse (ned::Percussive);
        d.loadBuffer (drums, SR, 0, "Bateria");
        d.reanalyse (ned::Percussive);
        d.waitUntilIdle (20000);
        int vis = 0, pitched = 0;
        for (const auto& nt : d.notes) if (nt.visible) { ++vis; if (nt.pitched) ++pitched; }
        report ("Percusivo: un golpe = una nota", vis == 6 && pitched == 0, juce::String (vis) + " notas");
    }
    // exportar WAV
    {
        const auto f = p.exportWav();
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (f));
        report ("Exporta WAV", rd != nullptr && rd->lengthInSamples >= (juce::int64) src.size(), f.getFileName());
        rd.reset();
        f.deleteFile();
    }
    std::printf (fails == 0 ? "TODAS LAS PRUEBAS OK\n" : "%d PRUEBAS FALLARON\n", fails);
    return fails == 0 ? 0 : 1;
}
