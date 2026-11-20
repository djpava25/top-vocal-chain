#include "TopChoirProcessor.h"
#include "TopChoirEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout TopChoirProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;
    auto addB = [&ps] (const char* id, const char* name, bool def) { ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };
    auto addC = [&ps] (const char* id, const char* name, const StringArray& c, int def) { ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, c, def)); };
    addC ("model", "Modelo de coro", choir::modelNames(), 0);
    addC ("mic", "Microfono", choir::micNames(), 0);
    addC ("key", "Tonalidad", StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 9);
    addC ("scale", "Escala", StringArray { "Cromatica", "Mayor", "Menor" }, 1);
    addB ("oct_up", "Octava arriba", false);
    addB ("oct_down", "Octava abajo", false);
    addB ("third_up", "Tercera arriba", false);
    addB ("third_down", "Tercera abajo", false);
    addB ("fifth_up", "Quinta arriba", false);
    addB ("fifth_down", "Quinta abajo", false);
    addB ("stereo", "Estereo", true);
    addB ("unison", "Unisono", true);
    return { ps.begin(), ps.end() };
}

TopChoirProcessor::TopChoirProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TopChoir", createLayout())
{
    formats.registerBasicFormats();
}

TopChoirProcessor::~TopChoirProcessor()
{
    cancel = true;
    pool.removeAllJobs (true, 60000);
}

bool TopChoirProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet(), in = l.getMainInputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) && in == out;
}

void TopChoirProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    hostRate = sampleRate;
    tmp.assign ((size_t) samplesPerBlock * 8 + 64, 0.0f);
    for (auto& i : interp) i.reset();
}

juce::File TopChoirProcessor::outputFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Top Choir");
}

bool TopChoirProcessor::loadVoice (const juce::File& f)
{
    std::unique_ptr<juce::AudioFormatReader> r (formats.createReaderFor (f));
    if (r == nullptr || r->lengthInSamples <= 0 || r->lengthInSamples > (juce::int64) (r->sampleRate * 60 * 10)) return false;
    juce::AudioBuffer<float> b ((int) std::min<juce::uint32> (2, r->numChannels), (int) r->lengthInSamples);
    r->read (&b, 0, b.getNumSamples(), 0, true, true);
    {
        const juce::ScopedLock sl (lock);
        playing = false;
        voice = std::move (b);
        voiceRate = r->sampleRate;
        voiceFile = f;
        choirReady = false;
        choirBuf.setSize (0, 0);
        choirFile = juce::File();
        playPos = 0.0;
    }
    voiceLoaded = true;
    return true;
}

void TopChoirProcessor::clearAll()
{
    cancel = true;
    const juce::ScopedLock sl (lock);
    playing = false; voiceLoaded = false; choirReady = false;
    voice.setSize (0, 0); choirBuf.setSize (0, 0);
    voiceFile = juce::File(); choirFile = juce::File();
}

bool TopChoirProcessor::autoDetectKey()
{
    if (! hasVoice()) return false;
    int root = 0; bool minor = false;
    if (! choir::detectKey (voice, voiceRate, root, minor)) return false;
    auto set = [this] (const char* id, float v) { auto* x = apvts.getParameter (id); x->setValueNotifyingHost (x->convertTo0to1 (v)); };
    set ("key", (float) root); set ("scale", minor ? 2.0f : 1.0f);
    return true;
}

choir::Settings TopChoirProcessor::currentSettings() const
{
    choir::Settings s;
    s.model = (int) p ("model"); s.mic = (int) p ("mic"); s.key = (int) p ("key");
    const int sc = (int) p ("scale"); s.scale = sc == 2 ? 2 : sc;   // 0 cromatica, 1 mayor, 2 menor natural
    s.octUp = p ("oct_up") > 0.5f; s.octDown = p ("oct_down") > 0.5f;
    s.thirdUp = p ("third_up") > 0.5f; s.thirdDown = p ("third_down") > 0.5f;
    s.fifthUp = p ("fifth_up") > 0.5f; s.fifthDown = p ("fifth_down") > 0.5f;
    s.stereo = p ("stereo") > 0.5f; s.includeUnison = p ("unison") > 0.5f;
    return s;
}

void TopChoirProcessor::createVocals()
{
    if (! hasVoice() || rendering.load()) return;
    cancel = false; rendering = true; progress = 0.0f; playing = false;
    const auto settings = currentSettings();
    pool.addJob ([this, settings]
    {
        juce::AudioBuffer<float> src; double sr; juce::File vf;
        { const juce::ScopedLock sl (lock); src.makeCopyOf (voice); sr = voiceRate; vf = voiceFile; }
        auto out = choir::render (src, sr, settings, progress, cancel);
        if (cancel.load() || out.getNumSamples() == 0) { rendering = false; return; }
        // guardar WAV 24 bit
        auto folder = outputFolder(); folder.createDirectory();
        juce::String name = vf.getFileNameWithoutExtension() + "_coro_" + choir::models()[(size_t) settings.model].name;
        auto file = folder.getNonexistentChildFile (name, ".wav");
        if (auto stream = std::unique_ptr<juce::FileOutputStream> (file.createOutputStream()))
        {
            juce::WavAudioFormat wav;
            const int ch = settings.stereo ? 2 : 1;
            if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (stream.get(), sr, (unsigned) ch, 24, {}, 0)))
            {
                stream.release();
                if (ch == 1) { out.addFrom (0, 0, out, 1, 0, out.getNumSamples()); out.applyGain (0, 0, out.getNumSamples(), 0.5f); }
                w->writeFromAudioSampleBuffer (out, 0, out.getNumSamples());
            }
        }
        {
            const juce::ScopedLock sl (lock);
            choirBuf = std::move (out);
            choirFile = file;
            playPos = 0.0;
        }
        choirReady = true;
        rendering = false;
    });
}

void TopChoirProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (! playing.load()) return;   // sin reproducir: el audio de la pista pasa tal cual
    const juce::ScopedTryLock sl (lock);
    if (! sl.isLocked()) return;
    const bool useChoir = choirReady.load() && choirBuf.getNumSamples() > 0;
    const auto& src = useChoir ? choirBuf : voice;
    if (src.getNumSamples() == 0) { playing = false; return; }
    const double ratio = voiceRate / hostRate;   // reproducir con la velocidad correcta aunque el DAW use otra frecuencia
    const double start = playPos.load();
    const int needed = (int) std::ceil (n * ratio) + 8;
    if ((int) tmp.size() < needed) return;
    int used = 0;
    for (int c = 0; c < std::min (2, buffer.getNumChannels()); ++c)
    {
        const int sc = std::min (c, src.getNumChannels() - 1);
        for (int i = 0; i < needed; ++i)
        {
            const int idx = (int) start + i;
            float v = idx < src.getNumSamples() ? src.getSample (sc, idx) : 0.0f;
            if (useChoir && mixLead.load() && idx < voice.getNumSamples()) v += 0.8f * voice.getSample (std::min (c, voice.getNumChannels() - 1), idx);
            tmp[(size_t) i] = v;
        }
        used = interp[c].process (ratio, tmp.data(), buffer.getWritePointer (c), n);
    }
    if (buffer.getNumChannels() > 2) for (int c = 2; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);
    const double next = start + used;
    playPos = next;
    if (next >= src.getNumSamples()) { playing = false; playPos = 0.0; for (auto& i : interp) i.reset(); }
}

void TopChoirProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("voiceFile", voiceFile.getFullPathName(), nullptr);
    state.setProperty ("choirFile", getChoirFile().getFullPathName(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void TopChoirProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            apvts.replaceState (tree);
            const juce::File vf (tree.getProperty ("voiceFile").toString());
            if (vf.existsAsFile()) loadVoice (vf);
            const juce::File cf (tree.getProperty ("choirFile").toString());
            if (cf.existsAsFile())
                if (std::unique_ptr<juce::AudioFormatReader> r (formats.createReaderFor (cf)); r != nullptr)
                {
                    juce::AudioBuffer<float> b ((int) std::min<juce::uint32> (2, r->numChannels), (int) r->lengthInSamples);
                    r->read (&b, 0, b.getNumSamples(), 0, true, true);
                    const juce::ScopedLock sl (lock);
                    choirBuf = std::move (b); choirFile = cf; choirReady = true;
                }
        }
}

juce::AudioProcessorEditor* TopChoirProcessor::createEditor() { return new TopChoirEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TopChoirProcessor(); }
