#include "Top3AProcessor.h"
#include "Top3AEditor.h"

namespace
{
    struct Preset { const char* name; std::vector<std::pair<const char*, float>> v; };
    const std::vector<Preset>& presets()
    {
        static const std::vector<Preset> p {
            { "Inicio", {} },
            { "Voz: nivelador suave", { { "peak", 4.6f }, { "gain", 4.3f } } },
            { "Voz rap al frente", { { "peak", 6.0f }, { "gain", 5.0f }, { "emph", 2.0f } } },
            { "Voz con picos (LIMIT)", { { "peak", 5.5f }, { "gain", 4.7f }, { "mode", 1 } } },
            { "Bajo / 808 parejo", { { "peak", 5.5f }, { "gain", 4.6f } } },
            { "Guitarra electrica", { { "peak", 5.2f }, { "gain", 4.6f } } },
            { "Guitarra acustica (HF)", { { "peak", 4.8f }, { "gain", 4.4f }, { "emph", 4.0f } } },
            { "Bombo con golpe", { { "peak", 6.0f }, { "gain", 5.0f } } },
            { "Caja que salta (HF EMPH)", { { "peak", 5.8f }, { "gain", 5.0f }, { "emph", 6.5f } } },
            { "Bateria en paralelo", { { "peak", 7.5f }, { "gain", 5.6f }, { "mode", 1 }, { "mix", 50.0f } } },
            { "Piano", { { "peak", 4.2f }, { "gain", 4.1f } } },
            { "Bus muy suave", { { "peak", 2.8f }, { "gain", 3.9f } } } };
        return p;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout Top3AProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;
    auto F = [&ps] (const char* id, const char* name, float lo, float hi, float d, float st, const char* unit)
    { ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, st), d, AudioParameterFloatAttributes().withLabel (unit))); };
    auto C = [&ps] (const char* id, const char* name, StringArray items, int d) { ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, d)); };
    auto B = [&ps] (const char* id, const char* name, bool d) { ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, d)); };
    F ("gain", "Gain", 0, 10, 4.0f, 0.01f, "");
    F ("peak", "Peak Reduction", 0, 10, 4.0f, 0.01f, "");
    C ("mode", "Comp / Limit", { "COMPRESS", "LIMIT" }, 0);
    F ("emph", "HF Emphasis", 0, 10, 0.0f, 0.01f, "");
    F ("mix", "Mix", 0, 100, 100.0f, 0.1f, "%");
    C ("meter", "Meter", { "GR", "OUTPUT" }, 0);
    B ("power", "Power", true);
    return { ps.begin(), ps.end() };
}

Top3AProcessor::Top3AProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true).withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Top3A", createLayout())
{
    using OS = juce::dsp::Oversampling<float>;
    os.clearOversamplingStages();
    os.addOversamplingStage (OS::filterHalfBandFIREquiripple, 0.06f, -110.0f, 0.06f, -110.0f);
    os.addOversamplingStage (OS::filterHalfBandFIREquiripple, 0.20f, -100.0f, 0.20f, -100.0f);
    os.setUsingIntegerLatency (true);
    l3a::inTable(); l3a::outTable();                                   // tablas de las etapas clase A (una sola vez)
}

bool Top3AProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    return in == out && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

l3a::Settings Top3AProcessor::settings() const
{
    auto f = [this] (const char* id) { return (double) apvts.getRawParameterValue (id)->load(); };
    l3a::Settings s;
    s.gain = f ("gain"); s.peak = f ("peak"); s.emph = f ("emph"); s.limit = f ("mode") > 0.5;
    return s;
}

void Top3AProcessor::prepareToPlay (double sr, int block)
{
    os.reset(); os.initProcessing ((size_t) std::max (1, block));
    latency = (int) std::round (os.getLatencyInSamples()); setLatencySamples (latency);
    lv.prepare (sr * (double) os.getOversamplingFactor(), std::min (2, std::max (1, getTotalNumInputChannels())));
    lv.set (settings(), true);
    dry.setSize (2, block);
    for (auto& d : delay) d.assign ((size_t) std::max (1, latency), 0.0f);
    delayPos = 0;
    mixSm.reset (sr, 0.05); onSm.reset (sr, 0.03);
    mixSm.setCurrentAndTargetValue (apvts.getRawParameterValue ("mix")->load() * 0.01f);
    onSm.setCurrentAndTargetValue (apvts.getRawParameterValue ("power")->load() > 0.5f ? 1.0f : 0.0f);
    avg = 0; avgA = 1.0 - std::exp (-1.0 / (0.010 * sr));
}

void Top3AProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples(), chs = std::min (2, getTotalNumInputChannels());
    for (int c = chs; c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, n);
    if (n == 0 || chs == 0) return;
    if (n > dry.getNumSamples()) dry.setSize (2, n, false, false, true);
    // copia seca alineada con la latencia del sobremuestreo (para MIX y POWER)
    for (int c = 0; c < chs; ++c)
    {
        auto* x = buffer.getReadPointer (c); auto* d = dry.getWritePointer (c); auto& dl = delay[(size_t) c];
        int pos = delayPos;
        for (int i = 0; i < n; ++i)
        {
            if (latency > 0) { d[i] = dl[(size_t) pos]; dl[(size_t) pos] = x[i]; if (++pos >= latency) pos = 0; }
            else d[i] = x[i];
        }
    }
    if (latency > 0) delayPos = (delayPos + n) % latency;

    lv.set (settings());
    {
        juce::dsp::AudioBlock<float> blk (buffer.getArrayOfWritePointers(), (size_t) chs, (size_t) n);
        auto up = os.processSamplesUp (blk);
        float* p0 = up.getChannelPointer (0);
        float* p1 = chs > 1 ? up.getChannelPointer (1) : nullptr;
        double v[2];
        for (size_t i = 0; i < up.getNumSamples(); ++i)
        {
            v[0] = (double) p0[i]; v[1] = p1 != nullptr ? (double) p1[i] : v[0];
            lv.process (v);
            p0[i] = (float) v[0]; if (p1 != nullptr) p1[i] = (float) v[1];
        }
        os.processSamplesDown (blk);
    }
    mixSm.setTargetValue (apvts.getRawParameterValue ("mix")->load() * 0.01f);
    onSm.setTargetValue (apvts.getRawParameterValue ("power")->load() > 0.5f ? 1.0f : 0.0f);
    for (int i = 0; i < n; ++i)
    {
        const float mix = mixSm.getNextValue(), on = onSm.getNextValue();
        float peak = 0;
        for (int c = 0; c < chs; ++c)
        {
            auto* d = buffer.getWritePointer (c);
            const float dr = dry.getSample (c, i);
            d[i] = dr * (1.0f - on) + on * (dr * (1.0f - mix) + d[i] * mix);
            peak = std::max (peak, std::abs (d[i]));
        }
        avg += ((double) peak - avg) * avgA;
    }
    grDb.store ((float) lv.grDb());
    outAvg.store ((float) avg);
}

int Top3AProcessor::getNumPrograms() { return (int) presets().size(); }
const juce::String Top3AProcessor::getProgramName (int i) { return juce::isPositiveAndBelow (i, getNumPrograms()) ? presets()[(size_t) i].name : ""; }
void Top3AProcessor::setCurrentProgram (int i)
{
    if (! juce::isPositiveAndBelow (i, getNumPrograms())) return;
    currentProgram = i;
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            if (r->getParameterID() != "power" && r->getParameterID() != "meter") r->setValueNotifyingHost (r->getDefaultValue());
    for (auto& [id, v] : presets()[(size_t) i].v)
        if (auto* r = apvts.getParameter (id)) r->setValueNotifyingHost (r->convertTo0to1 (v));
}
void Top3AProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto st = apvts.copyState(); st.setProperty ("program", currentProgram, nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary (*xml, dest);
}
void Top3AProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto t = juce::ValueTree::fromXml (*xml);
        if (t.isValid() && t.hasType (apvts.state.getType())) { currentProgram = (int) t.getProperty ("program", 0); apvts.replaceState (t); }
    }
}
juce::AudioProcessorEditor* Top3AProcessor::createEditor() { return new Top3AEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new Top3AProcessor(); }
