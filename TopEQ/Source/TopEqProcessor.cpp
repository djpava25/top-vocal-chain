#include "TopEqProcessor.h"
#include "TopEqEditor.h"

namespace
{
    struct Preset { const char* name; std::vector<std::pair<const char*, float>> v; };
    const std::vector<Preset>& presets()
    {
        static const std::vector<Preset> p {
            { "Plano (solo valvula)", {} },
            { "Voz: aire 12k", { { "highBoost", 4 }, { "highFreq", 5 }, { "bandwidth", 8 }, { "highAtten", 1.5f }, { "attFreq", 2 } } },
            { "Voz: cuerpo y aire", { { "lowBoost", 2 }, { "lowFreq", 3 }, { "highBoost", 3.5f }, { "highFreq", 4 }, { "bandwidth", 7 } } },
            { "Voz rap al frente", { { "lowAtten", 3 }, { "lowFreq", 3 }, { "highBoost", 5 }, { "highFreq", 2 }, { "bandwidth", 4 }, { "tube", 45 } } },
            { "Truco del grave 60 Hz", { { "lowBoost", 6 }, { "lowAtten", 4 }, { "lowFreq", 2 } } },
            { "Bombo gordo", { { "lowBoost", 7 }, { "lowAtten", 5 }, { "lowFreq", 2 }, { "highBoost", 3 }, { "highFreq", 2 }, { "bandwidth", 3 } } },
            { "808 / Bajo (100 Hz)", { { "lowBoost", 5 }, { "lowAtten", 3 }, { "lowFreq", 3 }, { "highAtten", 3 }, { "attFreq", 0 } } },
            { "Guitarra acustica", { { "lowAtten", 2 }, { "lowFreq", 3 }, { "highBoost", 3 }, { "highFreq", 4 }, { "bandwidth", 6 } } },
            { "Master: brillo", { { "lowBoost", 1.5f }, { "lowFreq", 1 }, { "highBoost", 2 }, { "highFreq", 6 }, { "bandwidth", 9 }, { "tube", 20 } } },
            { "Master: suave", { { "highAtten", 2 }, { "attFreq", 1 }, { "lowBoost", 1 }, { "lowFreq", 1 }, { "tube", 25 } } },
            { "Lofi oscuro", { { "lowAtten", 4 }, { "lowFreq", 3 }, { "highAtten", 7 }, { "attFreq", 0 }, { "tube", 80 } } } };
        return p;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout TopEqProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;
    auto F = [&ps] (const char* id, const char* name, float lo, float hi, float d, float st, const char* unit)
    { ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, st), d, AudioParameterFloatAttributes().withLabel (unit))); };
    auto C = [&ps] (const char* id, const char* name, StringArray items, int d) { ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, d)); };
    F ("lowBoost", "Low Boost", 0, 10, 0, 0.05f, "");
    F ("lowAtten", "Low Atten", 0, 10, 0, 0.05f, "");
    C ("lowFreq", "Low Freq (CPS)", { "20 Hz", "30 Hz", "60 Hz", "100 Hz" }, 2);
    F ("highBoost", "High Boost", 0, 10, 0, 0.05f, "");
    F ("bandwidth", "Bandwidth", 0, 10, 5, 0.05f, "");
    C ("highFreq", "High Freq (KCS)", { "3 kHz", "4 kHz", "5 kHz", "8 kHz", "10 kHz", "12 kHz", "16 kHz" }, 4);
    F ("highAtten", "High Atten", 0, 10, 0, 0.05f, "");
    C ("attFreq", "Atten Sel (KCS)", { "5 kHz", "10 kHz", "20 kHz" }, 1);
    F ("tube", "Tube", 0, 100, 30, 1, "%");
    F ("out", "Output", -12, 12, 0, 0.1f, "dB");
    ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { "power", 1 }, "In", true));
    return { ps.begin(), ps.end() };
}

TopEqProcessor::TopEqProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true).withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TopEQ1A", createLayout()) {}

bool TopEqProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    return in == out && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

teq::Settings TopEqProcessor::settings() const
{
    auto f = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    teq::Settings s;
    s.lowBoost = f ("lowBoost"); s.lowAtten = f ("lowAtten"); s.highBoost = f ("highBoost"); s.bandwidth = f ("bandwidth"); s.highAtten = f ("highAtten");
    s.tube = f ("tube"); s.lowFreq = (int) f ("lowFreq"); s.highFreq = (int) f ("highFreq"); s.attFreq = (int) f ("attFreq");
    return s;
}

void TopEqProcessor::prepareToPlay (double sr, int block)
{
    os.reset(); os.initProcessing ((size_t) block);
    latency = (int) os.getLatencyInSamples(); setLatencySamples (latency);
    for (auto& c : ch) c.prepare (sr * 2.0);
    designed = false;
    dry.setSize (2, block);
    for (auto& d : delay) d.assign ((size_t) std::max (1, latency), 0.0f);
    delayPos = 0;
    outSm.reset (sr, 0.05); outSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("out")->load()));
    onSm.reset (sr, 0.03); onSm.setCurrentAndTargetValue (apvts.getRawParameterValue ("power")->load() > 0.5f ? 1.0f : 0.0f);
}

void TopEqProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples(), chs = std::min (2, getTotalNumInputChannels());
    if (n == 0 || chs == 0) return;
    if (n > dry.getNumSamples()) dry.setSize (2, n, false, false, true);
    // senal original alineada con la latencia (para el switch IN sin saltos)
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

    const auto s = settings();
    if (! designed || ! (s == last))
    {
        const auto d = teq::design (s, getSampleRate() * 2.0);
        for (auto& c : ch) { c.setEq (d); c.setTube (s.tube); }
        last = s; designed = true;
    }
    {
        juce::dsp::AudioBlock<float> blk (buffer.getArrayOfWritePointers(), (size_t) chs, (size_t) n);
        auto up = os.processSamplesUp (blk);
        for (int c = 0; c < chs; ++c)
        {
            auto* d = up.getChannelPointer ((size_t) c);
            for (size_t i = 0; i < up.getNumSamples(); ++i) d[i] = ch[(size_t) c].process (d[i]);
        }
        os.processSamplesDown (blk);
    }
    outSm.setTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("out")->load()));
    onSm.setTargetValue (apvts.getRawParameterValue ("power")->load() > 0.5f ? 1.0f : 0.0f);
    for (int i = 0; i < n; ++i)
    {
        const float g = outSm.getNextValue(), on = onSm.getNextValue();
        for (int c = 0; c < chs; ++c)
        {
            auto* d = buffer.getWritePointer (c);
            d[i] = dry.getSample (c, i) * (1.0f - on) + d[i] * g * on;
        }
    }
    outPeak.store (std::max (outPeak.load(), buffer.getMagnitude (0, n)));
}

int TopEqProcessor::getNumPrograms() { return (int) presets().size(); }
const juce::String TopEqProcessor::getProgramName (int i) { return juce::isPositiveAndBelow (i, getNumPrograms()) ? presets()[(size_t) i].name : ""; }
void TopEqProcessor::setCurrentProgram (int i)
{
    if (! juce::isPositiveAndBelow (i, getNumPrograms())) return;
    currentProgram = i;
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm)) if (r->getParameterID() != "power") r->setValueNotifyingHost (r->getDefaultValue());
    for (auto& [id, v] : presets()[(size_t) i].v)
        if (auto* r = apvts.getParameter (id)) r->setValueNotifyingHost (r->convertTo0to1 (v));
}
void TopEqProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto st = apvts.copyState(); st.setProperty ("program", currentProgram, nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary (*xml, dest);
}
void TopEqProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto t = juce::ValueTree::fromXml (*xml);
        if (t.isValid() && t.hasType (apvts.state.getType())) { currentProgram = (int) t.getProperty ("program", 0); apvts.replaceState (t); }
    }
}
juce::AudioProcessorEditor* TopEqProcessor::createEditor() { return new TopEqEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TopEqProcessor(); }
