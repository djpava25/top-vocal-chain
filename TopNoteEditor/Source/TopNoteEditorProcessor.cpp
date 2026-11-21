#include "TopNoteEditorProcessor.h"
#include "TopNoteEditorEditor.h"

namespace
{
    const std::array<int, 12>& scaleMask (int scale)
    {
        static const std::array<std::array<int, 12>, 7> m {{
            {{ 1,1,1,1,1,1,1,1,1,1,1,1 }}, {{ 1,0,1,0,1,1,0,1,0,1,0,1 }}, {{ 1,0,1,1,0,1,0,1,1,0,1,0 }},
            {{ 1,0,1,1,0,1,0,1,1,0,0,1 }}, {{ 1,0,1,0,1,0,0,1,0,1,0,0 }}, {{ 1,0,0,1,0,1,0,1,0,0,1,0 }},
            {{ 1,0,0,1,0,1,1,1,0,0,1,0 }} }};
        return m[(size_t) juce::jlimit (0, 6, scale)];
    }
}

juce::StringArray TopNoteEditorProcessor::scaleNames()
{
    return { juce::String::fromUTF8 ("Cromática"), "Mayor", "Menor", juce::String::fromUTF8 ("Menor armónica"),
             juce::String::fromUTF8 ("Pentatónica mayor"), juce::String::fromUTF8 ("Pentatónica menor"), "Blues" };
}

juce::AudioProcessorValueTreeState::ParameterLayout TopNoteEditorProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "key", 1 }, "Tonalidad",
                  StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "scale", 1 }, "Escala", scaleNames(), 0));
    ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { "snap", 1 }, "Ajustar a la escala", true));
    return { ps.begin(), ps.end() };
}

TopNoteEditorProcessor::TopNoteEditorProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      juce::Thread ("TopNoteEditor worker"),
      apvts (*this, nullptr, "TopNoteEditor", createLayout())
{
    formats.registerBasicFormats();
    startThread();
}

TopNoteEditorProcessor::~TopNoteEditorProcessor()
{
    cancelJob = true;
    wantRender = true;      // corta un render en curso
    signalThreadShouldExit();
    notify();
    stopThread (5000);
    cancelPendingUpdate();
}

bool TopNoteEditorProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet(), in = l.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return in == out;
}

void TopNoteEditorProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
}

//==============================================================================
void TopNoteEditorProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples(), inCh = getTotalNumInputChannels(), outCh = getTotalNumOutputChannels();
    for (int ch = inCh; ch < outCh; ++ch) buffer.clear (ch, 0, n);

    bool playing = false;
    juce::int64 pos = 0;
    if (auto* ph = getPlayHead())
        if (auto p = ph->getPosition())
        {
            playing = p->getIsPlaying();
            if (auto t = p->getTimeInSamples()) pos = *t;
            if (auto b = p->getBpm()) bpm.store (*b);
            if (auto ts = p->getTimeSignature()) sigNum.store (ts->numerator);
        }
    hostPlaying.store (playing);

    // ---- TRANSFERIR: graba lo que entra, anclado a la linea de tiempo del DAW ----
    if (recArmed.load())
    {
        if (playing && ! rec.empty())
        {
            if (! recHasOrigin.load()) { recOrigin = pos; recHasOrigin.store (true); }
            const juce::int64 base = pos - recOrigin;
            for (int i = 0; i < n; ++i)
            {
                const juce::int64 idx = base + i;
                if (idx < 0 || idx >= (juce::int64) rec.size()) continue;
                float x = 0.0f;
                for (int ch = 0; ch < inCh; ++ch) x += buffer.getSample (ch, i);
                rec[(size_t) idx] = x / (float) juce::jmax (1, inCh);
            }
            if (base + n > recMax.load() && base + n <= (juce::int64) rec.size()) recMax.store ((int) (base + n));
            playPos.store ((double) base);
        }
        return;   // se escucha la entrada tal cual mientras se transfiere
    }

    std::shared_ptr<const std::vector<float>> out, src;
    {
        const juce::SpinLock::ScopedTryLockType l (outLock);
        if (l.isLocked()) { out = rendered; src = source; }
    }
    const std::vector<float>* buf = out != nullptr ? out.get() : src.get();
    if (buf == nullptr || buf->empty()) return;
    const int len = (int) buf->size();

    if (playing)
    {
        previewing.store (false);
        const juce::int64 base = pos - origin;
        playPos.store ((double) base);
        for (int i = 0; i < n; ++i)
        {
            const juce::int64 idx = base + i;
            if (idx < 0 || idx >= len) continue;      // fuera de la toma: pasa la entrada
            const float v = (*buf)[(size_t) idx];
            for (int ch = 0; ch < outCh; ++ch) buffer.setSample (ch, i, v);
        }
    }
    else if (previewing.load())
    {
        for (int i = 0; i < n; ++i)
        {
            const int idx = (int) previewPos;
            const float v = idx >= 0 && idx < len ? (*buf)[(size_t) idx] : 0.0f;
            for (int ch = 0; ch < outCh; ++ch) buffer.setSample (ch, i, v);
            previewPos += 1.0;
        }
        if (previewPos >= len) previewing.store (false);
        playPos.store (previewPos);
    }
}

void TopNoteEditorProcessor::startPreview (double fromSample)
{
    previewPos = juce::jmax (0.0, fromSample);
    previewing.store (true);
}

//==============================================================================
void TopNoteEditorProcessor::setTransfer (bool on)
{
    if (on)
    {
        stopPreview();
        rec.assign ((size_t) (sr * 60.0 * 8.0), 0.0f);     // hasta 8 minutos
        recMax.store (0);
        recHasOrigin.store (false);
        recArmed.store (true);
        status.store (Recording);
        return;
    }
    if (! recArmed.load()) return;
    recArmed.store (false);
    juce::Thread::sleep (40);     // deja terminar el bloque de audio en curso
    const int len = recMax.load();
    if (len > (int) (sr * 0.2))
    {
        std::vector<float> m (rec.begin(), rec.begin() + len);
        rec.clear(); rec.shrink_to_fit();
        loadBuffer (std::move (m), sr, recOrigin, "Transferencia");
    }
    else
    {
        rec.clear(); rec.shrink_to_fit();
        status.store (hasSource() ? Ready : Empty);
    }
}

bool TopNoteEditorProcessor::importFile (const juce::File& f)
{
    std::unique_ptr<juce::AudioFormatReader> r (formats.createReaderFor (f));
    if (r == nullptr || r->lengthInSamples <= 0 || r->lengthInSamples > (juce::int64) (r->sampleRate * 600)) return false;
    juce::AudioBuffer<float> b ((int) juce::jmin<juce::uint32> (2, r->numChannels), (int) r->lengthInSamples);
    r->read (&b, 0, b.getNumSamples(), 0, true, true);
    std::vector<float> m ((size_t) b.getNumSamples());
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        float x = 0; for (int ch = 0; ch < b.getNumChannels(); ++ch) x += b.getSample (ch, i);
        m[(size_t) i] = x / (float) b.getNumChannels();
    }
    loadBuffer (std::move (m), r->sampleRate, 0, f.getFileNameWithoutExtension());
    return true;
}

void TopNoteEditorProcessor::loadBuffer (std::vector<float> mono, double rate, juce::int64 originSample, const juce::String& name)
{
    if (std::abs (rate - sr) > 1.0)
    {
        // remuestrea a la frecuencia del proyecto
        const double ratio = rate / sr;
        std::vector<float> o ((size_t) (mono.size() / ratio));
        juce::LagrangeInterpolator li;
        li.process (ratio, mono.data(), o.data(), (int) o.size(), (int) mono.size(), 0);
        mono = std::move (o);
    }
    stopPreview();
    {
        const juce::ScopedLock sl (jobLock);
        const juce::SpinLock::ScopedLockType l (outLock);
        source = std::make_shared<const std::vector<float>> (std::move (mono));
        rendered = nullptr;
    }
    origin = originSample;
    sourceName = name;
    notes.clear();
    undoStack.clear(); redoStack.clear();
    writeSourceFile();
    reanalyse (algorithm);
}

void TopNoteEditorProcessor::writeSourceFile()
{
    if (! hasSource()) return;
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Top Note Editor").getChildFile ("Tomas");
    dir.createDirectory();
    sourceFile = dir.getChildFile (juce::Uuid().toString() + ".wav").getNonexistentSibling (false);
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (sourceFile.createOutputStream());
    if (os == nullptr) return;
    if (auto* w = wav.createWriterFor (os.get(), sr, 1, 24, {}, 0))
    {
        os.release();
        std::unique_ptr<juce::AudioFormatWriter> writer (w);
        const float* ptr = source->data();
        writer->writeFromFloatArrays (&ptr, 1, (int) source->size());
    }
}

void TopNoteEditorProcessor::reanalyse (int alg)
{
    if (! hasSource()) return;
    algorithm = alg;
    {
        const juce::ScopedLock sl (jobLock);
        jobAlgorithm = alg;
        keepNotesOnAnalyse = false;
    }
    cancelJob = true;
    status.store (Analysing);
    progress.store (0.0f);
    wantAnalyse = true;
    notify();
}

void TopNoteEditorProcessor::applyPendingAnalysis()
{
    std::vector<ned::Note> nn;
    bool keep;
    {
        const juce::ScopedLock sl (jobLock);
        if (! pendingReady) return;
        pendingReady = false;
        curve = pendingCurve;
        nn = std::move (pendingNotes);
        keep = keepNotesOnAnalyse;
        keepNotesOnAnalyse = false;
    }
    if (! keep)
    {
        notes = std::move (nn);
        undoStack.clear(); redoStack.clear();
    }
    status.store (Ready);
    notesChanged();
}

bool TopNoteEditorProcessor::waitUntilIdle (int timeoutMs)
{
    const auto end = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;
    while (juce::Time::getMillisecondCounter() < end)
    {
        applyPendingAnalysis();
        if (! wantAnalyse.load() && ! wantRender.load() && ! rendering.load() && status.load() != Analysing) return true;
        juce::Thread::sleep (10);
    }
    return false;
}

void TopNoteEditorProcessor::run()
{
    std::shared_ptr<const std::vector<float>> retired;    // el audio anterior se libera aqui, no en el hilo de audio
    while (! threadShouldExit())
    {
        wait (100);
        if (threadShouldExit()) break;
        if (wantAnalyse.exchange (false))
        {
            cancelJob = false;
            std::shared_ptr<const std::vector<float>> src;
            int alg; bool keep;
            {
                const juce::ScopedLock sl (jobLock);
                src = source; alg = jobAlgorithm; keep = keepNotesOnAnalyse;
            }
            if (src == nullptr) continue;
            renderer.clearCache();
            auto c = std::make_shared<ned::Curve> (ned::analyse (src->data(), (int) src->size(), sr, &progress, &cancelJob));
            if (cancelJob.load() || threadShouldExit()) continue;
            auto nn = keep ? std::vector<ned::Note>() : ned::segment (*c, (int) src->size(), alg);
            {
                const juce::ScopedLock sl (jobLock);
                pendingCurve = c;
                pendingNotes = std::move (nn);
                pendingReady = true;
            }
            triggerAsyncUpdate();
        }
        if (wantRender.load())
        {
            rendering = true;
            wantRender = false;
            std::vector<ned::Note> nn;
            std::shared_ptr<const std::vector<float>> src;
            std::shared_ptr<const ned::Curve> c;
            {
                const juce::ScopedLock sl (jobLock);
                nn = renderNotes; src = source; c = curve;
            }
            if (src != nullptr && c != nullptr && ! threadShouldExit())
            {
                auto out = renderer.render (src->data(), (int) src->size(), *c, nn, &wantRender, &renderProgress);
                if (! out.empty())
                {
                    auto sp = std::make_shared<const std::vector<float>> (std::move (out));
                    const juce::SpinLock::ScopedLockType l (outLock);
                    retired = rendered;
                    rendered = sp;
                }
            }
            rendering = false;
        }
    }
}

//==============================================================================
void TopNoteEditorProcessor::notesChanged()
{
    {
        const juce::ScopedLock sl (jobLock);
        renderNotes = notes;
        // la curva se lee bajo el mismo candado
    }
    modelVersion.fetch_add (1);
    wantRender = true;
    notify();
}

void TopNoteEditorProcessor::beginEdit()
{
    undoStack.push_back (notes);
    if (undoStack.size() > 200) undoStack.erase (undoStack.begin());
    redoStack.clear();
}
void TopNoteEditorProcessor::undo()
{
    if (undoStack.empty()) return;
    redoStack.push_back (notes);
    notes = undoStack.back(); undoStack.pop_back();
    notesChanged();
}
void TopNoteEditorProcessor::redo()
{
    if (redoStack.empty()) return;
    undoStack.push_back (notes);
    notes = redoStack.back(); redoStack.pop_back();
    notesChanged();
}

int TopNoteEditorProcessor::snapPitch (float m) const
{
    const int scale = (int) apvts.getRawParameterValue ("scale")->load();
    const bool snap = apvts.getRawParameterValue ("snap")->load() > 0.5f;
    if (! snap || scale == 0) return (int) std::round (m);
    const int key = (int) apvts.getRawParameterValue ("key")->load();
    const auto& mask = scaleMask (scale);
    int best = (int) std::round (m); float bd = 1e9f;
    for (int k = (int) std::round (m) - 6; k <= (int) std::round (m) + 6; ++k)
        if (mask[(size_t) (((k - key) % 12 + 12) % 12)] && std::abs ((float) k - m) < bd) { bd = std::abs ((float) k - m); best = k; }
    return best;
}

std::vector<int> TopNoteEditorProcessor::targetsOf (const std::vector<int>& sel) const
{
    std::vector<int> r;
    if (sel.empty()) { for (int i = 0; i < (int) notes.size(); ++i) if (notes[(size_t) i].visible) r.push_back (i); }
    else for (int i : sel) if (i >= 0 && i < (int) notes.size() && notes[(size_t) i].visible) r.push_back (i);
    return r;
}

void TopNoteEditorProcessor::macroCorrectPitch (const std::vector<int>& sel, float centerAmt, float driftAmt)
{
    beginEdit();
    for (int i : targetsOf (sel))
    {
        auto& nt = notes[(size_t) i];
        if (! nt.pitched) continue;
        const float cur = nt.center + nt.shift;
        nt.shift += ((float) snapPitch (cur) - cur) * centerAmt;
        nt.drift = juce::jlimit (0.0f, 2.0f, 1.0f - driftAmt);
    }
    notesChanged();
}

void TopNoteEditorProcessor::macroQuantizeTime (const std::vector<int>& sel, double gridBeats, float amount)
{
    beginEdit();
    const double grid = gridBeats * sr * 60.0 / juce::jmax (20.0, bpm.load());
    for (int i : targetsOf (sel))
    {
        auto& nt = notes[(size_t) i];
        const double hostPos = (double) (origin + nt.t0);
        const double nearest = std::round (hostPos / grid) * grid;
        const int delta = (int) std::round ((nearest - hostPos) * amount);
        nt.t0 += delta; nt.t1 += delta;
        if (nt.t0 < 0) { nt.t1 -= nt.t0; nt.t0 = 0; }
    }
    notesChanged();
}

void TopNoteEditorProcessor::macroLevel (const std::vector<int>& sel, float louder, float softer)
{
    const auto t = targetsOf (sel);
    double sum = 0; int k = 0;
    for (int i : t) if (notes[(size_t) i].levelDb > -70.0f) { sum += notes[(size_t) i].levelDb; ++k; }
    if (k == 0) return;
    const float mean = (float) (sum / k);
    beginEdit();
    for (int i : t)
    {
        auto& nt = notes[(size_t) i];
        if (nt.levelDb <= -70.0f) continue;
        const float d = nt.levelDb - mean;
        nt.gainDb = juce::jlimit (-24.0f, 24.0f, -d * (d > 0 ? louder : softer));
    }
    notesChanged();
}

//==============================================================================
std::vector<float> TopNoteEditorProcessor::renderNow()
{
    if (! hasSource() || curve == nullptr) return {};
    ned::Renderer r;
    return r.render (source->data(), (int) source->size(), *curve, notes);
}

juce::File TopNoteEditorProcessor::dragFile()
{
    if (lastExport.existsAsFile() && exportedVersion == modelVersion.load() && exportedAlign == alignToSong) return lastExport;
    return exportWav();
}

juce::File TopNoteEditorProcessor::exportWav()
{
    const int version = modelVersion.load();
    auto out = renderNow();
    if (out.empty()) return {};
    // alineado con la cancion: silencio desde el compas 1 hasta donde empezaba la toma
    if (alignToSong && origin > 0 && origin < (juce::int64) (sr * 60 * 30))
        out.insert (out.begin(), (size_t) origin, 0.0f);
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Top Note Editor");
    dir.createDirectory();
    auto f = dir.getChildFile (juce::File::createLegalFileName (sourceName) + "_afinada_" + juce::Time::getCurrentTime().formatted ("%H%M%S") + ".wav").getNonexistentSibling (false);
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream());
    if (os == nullptr) return {};
    if (auto* w = wav.createWriterFor (os.get(), sr, 1, 24, {}, 0))
    {
        os.release();
        std::unique_ptr<juce::AudioFormatWriter> writer (w);
        const float* ptr = out.data();
        writer->writeFromFloatArrays (&ptr, 1, (int) out.size());
    }
    lastExport = f;
    exportedVersion = version; exportedAlign = alignToSong;
    return f;
}

//==============================================================================
void TopNoteEditorProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.removeChild (state.getChildWithName ("Session"), nullptr);
    juce::ValueTree s ("Session");
    s.setProperty ("file", sourceFile.getFullPathName(), nullptr);
    s.setProperty ("origin", (juce::int64) origin, nullptr);
    s.setProperty ("rate", sr, nullptr);
    s.setProperty ("algorithm", algorithm, nullptr);
    s.setProperty ("name", sourceName, nullptr);
    s.setProperty ("align", alignToSong, nullptr);
    for (const auto& nt : notes)
    {
        juce::ValueTree v ("N");
        v.setProperty ("s0", nt.s0, nullptr); v.setProperty ("s1", nt.s1, nullptr);
        v.setProperty ("t0", nt.t0, nullptr); v.setProperty ("t1", nt.t1, nullptr);
        v.setProperty ("c", nt.center, nullptr); v.setProperty ("p", nt.pitched, nullptr); v.setProperty ("v", nt.visible, nullptr);
        v.setProperty ("sh", nt.shift, nullptr); v.setProperty ("mo", nt.mod, nullptr); v.setProperty ("dr", nt.drift, nullptr);
        v.setProperty ("fo", nt.formant, nullptr); v.setProperty ("g", nt.gainDb, nullptr); v.setProperty ("m", nt.mute, nullptr);
        v.setProperty ("l", nt.levelDb, nullptr);
        s.appendChild (v, nullptr);
    }
    state.appendChild (s, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void TopNoteEditorProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto tree = juce::ValueTree::fromXml (*xml);
    const auto s = tree.getChildWithName ("Session");
    tree.removeChild (s, nullptr);
    apvts.replaceState (tree);
    if (! s.isValid()) return;
    const juce::File f (s.getProperty ("file").toString());
    std::unique_ptr<juce::AudioFormatReader> r (f.existsAsFile() ? formats.createReaderFor (f) : nullptr);
    if (r == nullptr) return;
    std::vector<float> m ((size_t) r->lengthInSamples);
    {
        juce::AudioBuffer<float> b (1, (int) r->lengthInSamples);
        r->read (&b, 0, b.getNumSamples(), 0, true, false);
        std::copy (b.getReadPointer (0), b.getReadPointer (0) + b.getNumSamples(), m.begin());
    }
    sr = (double) s.getProperty ("rate", r->sampleRate);
    {
        const juce::ScopedLock sl (jobLock);
        const juce::SpinLock::ScopedLockType l (outLock);
        source = std::make_shared<const std::vector<float>> (std::move (m));
        rendered = nullptr;
    }
    sourceFile = f;
    origin = (juce::int64) s.getProperty ("origin");
    algorithm = (int) s.getProperty ("algorithm");
    sourceName = s.getProperty ("name").toString();
    alignToSong = (bool) s.getProperty ("align", true);
    notes.clear();
    for (int i = 0; i < s.getNumChildren(); ++i)
    {
        const auto v = s.getChild (i);
        ned::Note nt;
        nt.s0 = v["s0"]; nt.s1 = v["s1"]; nt.t0 = v["t0"]; nt.t1 = v["t1"];
        nt.center = (float) v["c"]; nt.pitched = v["p"]; nt.visible = v["v"];
        nt.shift = (float) v["sh"]; nt.mod = (float) v["mo"]; nt.drift = (float) v["dr"];
        nt.formant = (float) v["fo"]; nt.gainDb = (float) v["g"]; nt.mute = v["m"]; nt.levelDb = (float) v["l"];
        notes.push_back (nt);
    }
    undoStack.clear(); redoStack.clear();
    {
        const juce::ScopedLock sl (jobLock);
        jobAlgorithm = algorithm;
        keepNotesOnAnalyse = ! notes.empty();
    }
    cancelJob = true;
    status.store (Analysing);
    wantAnalyse = true;
    notify();
}

juce::AudioProcessorEditor* TopNoteEditorProcessor::createEditor() { return new TopNoteEditorEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TopNoteEditorProcessor(); }
