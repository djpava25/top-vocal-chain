#include "TopSmoothProcessor.h"
#include "TopSmoothEditor.h"

namespace
{
    struct Preset { const char* name; std::vector<std::pair<juce::String, float>> v; };
    const std::vector<Preset>& presets()
    {
        static const std::vector<Preset> p {
            { "Por defecto", {} },
            { "Voz: quitar dureza", { { "depth", 6 }, { "detail", 55 }, { "attack", 3 }, { "release", 60 }, { "maxcut", 12 }, { "lowcut", 300 }, { "n4_g", 4 } } },
            { "Voz: sibilancia (S)", { { "mode", 1 }, { "depth", 8 }, { "detail", 70 }, { "attack", 1 }, { "release", 40 }, { "lowcut", 4000 }, { "n4_g", 6 } } },
            { "Rap agresivo", { { "depth", 10 }, { "detail", 60 }, { "attack", 2 }, { "release", 50 }, { "maxcut", 15 }, { "lowcut", 1000 } } },
            { "Guitarra acustica", { { "depth", 5 }, { "detail", 45 }, { "lowcut", 120 }, { "n1_g", 3 }, { "n3_g", 2 } } },
            { "Platillos / hi-hats", { { "depth", 7 }, { "detail", 50 }, { "attack", 1 }, { "release", 30 }, { "lowcut", 3000 } } },
            { "Bus de mezcla", { { "depth", 3 }, { "detail", 35 }, { "maxcut", 6 }, { "release", 120 } } },
            { "Master suave", { { "depth", 2 }, { "detail", 30 }, { "maxcut", 4 }, { "release", 150 } } },
            { "Graves resonantes", { { "depth", 6 }, { "detail", 60 }, { "release", 120 }, { "lowcut", 30 }, { "highcut", 400 } } },
            { "Grabando en vivo (0 ms)", { { "latency", 1 }, { "depth", 5 }, { "detail", 50 } } },
        };
        return p;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout TopSmoothProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;
    auto addF = [&ps] (const String& id, const String& name, NormalisableRange<float> r, float def, const String& unit, int dec)
    {
        auto toText = [unit, dec] (float v, int) { return String (v, dec) + (unit.isNotEmpty() ? " " + unit : String()); };
        auto fromText = [] (const String& t) { return t.retainCharacters ("-0123456789.").getFloatValue(); };
        ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def,
                      AudioParameterFloatAttributes().withLabel (unit).withStringFromValueFunction (toText).withValueFromStringFunction (fromText)));
    };
    auto skew = [] (float a, float b, float c) { NormalisableRange<float> r (a, b); r.setSkewForCentre (c); return r; };
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Modo", StringArray { "Suave", "Duro" }, 0));
    addF ("depth", "Profundidad", { 0.0f, 20.0f, 0.01f }, 5.0f, "", 1);
    addF ("detail", "Detalle", { 0.0f, 100.0f, 0.1f }, 50.0f, "%", 0);
    addF ("attack", "Ataque", skew (0.5f, 50.0f, 5.0f), 5.0f, "ms", 1);
    addF ("release", "Release", skew (5.0f, 500.0f, 60.0f), 60.0f, "ms", 0);
    addF ("maxcut", "Corte maximo", { 1.0f, 30.0f, 0.1f }, 18.0f, "dB", 1);
    addF ("tilt", "Inclinacion", { -6.0f, 6.0f, 0.01f }, 0.0f, "dB/oct", 1);
    addF ("mix", "Mezcla", { 0.0f, 100.0f, 0.1f }, 100.0f, "%", 0);
    addF ("trim", "Trim", { -12.0f, 12.0f, 0.01f }, 0.0f, "dB", 1);
    ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { "delta", 1 }, "Delta (escuchar lo que se quita)", false));
    ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { "bypass", 1 }, "Bypass", false));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "latency", 1 }, "Latencia", StringArray { "Fase lineal", "Baja latencia (0 ms)" }, 0));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "stereo", 1 }, "Estereo", StringArray { "Enlazado", "L / R", "Mid / Side" }, 0));
    ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { "sidechain", 1 }, "Sidechain externo", false));
    addF ("lowcut", "Corte grave", skew (20.0f, 2000.0f, 200.0f), 20.0f, "Hz", 0);
    addF ("highcut", "Corte agudo", skew (1000.0f, 20000.0f, 8000.0f), 20000.0f, "Hz", 0);
    const float nf[5] { 150.0f, 500.0f, 2000.0f, 5000.0f, 10000.0f };
    for (int i = 0; i < 5; ++i)
    {
        const auto n = "n" + String (i + 1);
        addF (n + "_f", "Nodo " + String (i + 1) + " frecuencia", skew (30.0f, 18000.0f, 1000.0f), nf[i], "Hz", 0);
        addF (n + "_g", "Nodo " + String (i + 1) + " sensibilidad", { -12.0f, 12.0f, 0.01f }, 0.0f, "dB", 1);
        addF (n + "_q", "Nodo " + String (i + 1) + " ancho", skew (0.3f, 4.0f, 1.0f), 1.0f, "", 2);
    }
    return { ps.begin(), ps.end() };
}

TopSmoothProcessor::TopSmoothProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                        .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "TopSmooth", createLayout())
{
    for (auto& a : display.inDb) a.store (-120.0f);
    for (auto& a : display.outDb) a.store (-120.0f);
    for (auto& a : display.redDb) a.store (0.0f);
}

bool TopSmoothProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet(), in = l.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    if (in != out) return false;
    if (l.inputBuses.size() > 1)
    {
        const auto sc = l.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo()) return false;
    }
    return true;
}

tsm::Settings TopSmoothProcessor::currentSettings() const
{
    auto f = [this] (const juce::String& id) { return apvts.getRawParameterValue (id)->load(); };
    tsm::Settings s;
    s.mode = (int) f ("mode"); s.depth = f ("depth"); s.detail = f ("detail");
    s.attackMs = f ("attack"); s.releaseMs = f ("release"); s.maxCut = f ("maxcut"); s.tilt = f ("tilt");
    s.lowCut = f ("lowcut"); s.highCut = f ("highcut");
    for (int i = 0; i < 5; ++i)
    {
        const auto n = "n" + juce::String (i + 1);
        s.nf[(size_t) i] = f (n + "_f"); s.ng[(size_t) i] = f (n + "_g"); s.nq[(size_t) i] = f (n + "_q");
    }
    return s;
}

void TopSmoothProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    const int order = sr <= 50000.0 ? 11 : sr <= 100000.0 ? 12 : 13;
    spectral.prepare (sr, order);
    bank.prepare (sr);
    for (int c = 0; c < 2; ++c)
    {
        work[(size_t) c].assign (chunk, 0.0f); side[(size_t) c].assign (chunk, 0.0f); dry[(size_t) c].assign (chunk, 0.0f);
        delay[(size_t) c].assign ((size_t) spectral.latency(), 0.0f);
    }
    delayPos = 0;
    engineMode = -1;
    applyLatencyMode ((int) apvts.getRawParameterValue ("latency")->load());
}

void TopSmoothProcessor::applyLatencyMode (int mode)
{
    if (mode == engineMode) return;
    engineMode = mode;
    spectral.reset(); bank.reset();
    for (auto& d : delay) std::fill (d.begin(), d.end(), 0.0f);
    setLatencySamples (mode == 0 ? spectral.latency() : 0);
}

void TopSmoothProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const auto s = currentSettings();
    applyLatencyMode ((int) apvts.getRawParameterValue ("latency")->load());
    const int stereoMode = (int) apvts.getRawParameterValue ("stereo")->load();
    const bool delta = apvts.getRawParameterValue ("delta")->load() > 0.5f;
    const bool bypass = apvts.getRawParameterValue ("bypass")->load() > 0.5f;
    const float mixT = bypass ? 0.0f : apvts.getRawParameterValue ("mix")->load() / 100.0f;
    const float trimT = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("trim")->load());

    auto mainIn = getBusBuffer (buffer, true, 0);
    const int numCh = juce::jmin (2, mainIn.getNumChannels());
    const int n = buffer.getNumSamples();
    // sidechain: punteros directos (sin copiar ni reservar memoria)
    const float* scp[2] { nullptr, nullptr };
    int scCh = 0;
    if (apvts.getRawParameterValue ("sidechain")->load() > 0.5f && getBusCount (true) > 1)
        if (auto* bus = getBus (true, 1); bus != nullptr && bus->isEnabled())
        {
            scCh = juce::jmin (2, bus->getNumberOfChannels());
            for (int c = 0; c < scCh; ++c)
            {
                const int idx = getChannelIndexInProcessBlockBuffer (true, 1, c);
                scp[c] = idx < buffer.getNumChannels() ? buffer.getReadPointer (idx) : nullptr;
                if (scp[c] == nullptr) scCh = 0;
            }
        }
    const bool useSc = scCh > 0;
    const bool ms = stereoMode == 2 && numCh == 2;
    const bool linked = stereoMode == 0;
    float pkIn = 0, pkOut = 0;

    for (int pos = 0; pos < n; pos += chunk)
    {
        const int m = juce::jmin (chunk, n - pos);
        for (int c = 0; c < numCh; ++c)
            for (int i = 0; i < m; ++i) { const float x = mainIn.getSample (c, pos + i); work[(size_t) c][(size_t) i] = x; pkIn = juce::jmax (pkIn, std::abs (x)); }
        if (useSc)
            for (int c = 0; c < numCh; ++c)
                for (int i = 0; i < m; ++i) side[(size_t) c][(size_t) i] = scp[juce::jmin (c, scCh - 1)][pos + i];
        if (ms)
            for (int i = 0; i < m; ++i)
            {
                const float l = work[0][(size_t) i], r = work[1][(size_t) i];
                work[0][(size_t) i] = 0.5f * (l + r); work[1][(size_t) i] = 0.5f * (l - r);
                if (useSc) { const float a = side[0][(size_t) i], b = side[1][(size_t) i]; side[0][(size_t) i] = 0.5f * (a + b); side[1][(size_t) i] = 0.5f * (a - b); }
            }
        // seco: retrasado igual que el motor de fase lineal
        for (int c = 0; c < numCh; ++c)
            for (int i = 0; i < m; ++i)
            {
                if (engineMode == 0)
                {
                    const size_t dp = (size_t) ((delayPos + i) % (int) delay[(size_t) c].size());
                    dry[(size_t) c][(size_t) i] = delay[(size_t) c][dp];
                    delay[(size_t) c][dp] = work[(size_t) c][(size_t) i];
                }
                else dry[(size_t) c][(size_t) i] = work[(size_t) c][(size_t) i];
            }
        if (engineMode == 0) delayPos = (delayPos + m) % (int) delay[0].size();

        float* pp[2] { work[0].data(), work[1].data() };
        const float* dp[2] { side[0].data(), side[1].data() };
        if (engineMode == 0) spectral.process (pp, useSc ? dp : nullptr, numCh, m, linked, s, &display);
        else bank.process (pp, useSc ? dp : nullptr, numCh, m, linked, s, &display);

        for (int i = 0; i < m; ++i)
        {
            mixSm += (mixT - mixSm) * 0.002f;
            trimSm += (trimT - trimSm) * 0.002f;
            float o[2];
            for (int c = 0; c < numCh; ++c)
            {
                const float wet = work[(size_t) c][(size_t) i], d = dry[(size_t) c][(size_t) i];
                o[c] = delta && ! bypass ? (d - wet) * mixSm : d + (wet - d) * mixSm;
            }
            if (ms) { const float mm = o[0], ss = o[1]; o[0] = mm + ss; o[1] = mm - ss; }
            for (int c = 0; c < numCh; ++c)
            {
                const float y = o[c] * (bypass ? 1.0f : trimSm);
                buffer.setSample (c, pos + i, y);
                pkOut = juce::jmax (pkOut, std::abs (y));
            }
        }
    }
    for (int c = numCh; c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, n);
    inDb.store (juce::Decibels::gainToDecibels (pkIn, -100.0f));
    outDb.store (juce::Decibels::gainToDecibels (pkOut, -100.0f));
}

int TopSmoothProcessor::getNumPrograms() { return (int) presets().size(); }
const juce::String TopSmoothProcessor::getProgramName (int i) { return juce::isPositiveAndBelow (i, (int) presets().size()) ? juce::String::fromUTF8 (presets()[(size_t) i].name) : juce::String(); }
void TopSmoothProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presets().size())) return;
    program = index;
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
            if (rp->getParameterID() != "bypass" && rp->getParameterID() != "sidechain")
                rp->setValueNotifyingHost (rp->getDefaultValue());
    for (const auto& [id, val] : presets()[(size_t) index].v)
        if (auto* rp = apvts.getParameter (id)) rp->setValueNotifyingHost (rp->convertTo0to1 (val));
}

void TopSmoothProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", program, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}
void TopSmoothProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto t = juce::ValueTree::fromXml (*xml);
            program = (int) t.getProperty ("program", 0);
            apvts.replaceState (t);
        }
}

juce::AudioProcessorEditor* TopSmoothProcessor::createEditor() { return new TopSmoothEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TopSmoothProcessor(); }
