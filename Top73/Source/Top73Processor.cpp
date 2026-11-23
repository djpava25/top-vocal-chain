#include "Top73Processor.h"
#include "Top73Editor.h"

namespace
{
    struct Preset { const char* name; std::vector<std::pair<const char*, float>> v; };
    // gain: indice (6 = MIC 50, 8 = MIC 60, 10 = MIC 70, 17 = LINE 0)
    const std::vector<Preset>& presets()
    {
        static const std::vector<Preset> p {
            { "Voz limpia (MIC 50)", {} },
            { "Voz con brillo (60 + 12k)", { { "gain", 8 }, { "hf", 4 }, { "hpf", 2 } } },
            { "Voz saturada (MIC 70)", { { "gain", 10 }, { "hpf", 2 }, { "midFreq", 4 }, { "midGain", 2 } } },
            { "Rap agresivo (MIC 75)", { { "gain", 11 }, { "hpf", 3 }, { "midFreq", 4 }, { "midGain", 4 }, { "hf", 3 }, { "lowFreq", 3 }, { "lowGain", -2 } } },
            { "Voz R&B calida", { { "gain", 8 }, { "lowFreq", 3 }, { "lowGain", 2 }, { "midFreq", 5 }, { "midGain", -2 }, { "hf", 3 }, { "hpf", 1 } } },
            { "Bombo (60 Hz + 3.2k)", { { "gain", 9 }, { "lowFreq", 2 }, { "lowGain", 5 }, { "midFreq", 4 }, { "midGain", 4 } } },
            { "Bajo / 808 (110 Hz)", { { "gain", 9 }, { "lowFreq", 3 }, { "lowGain", 4 }, { "midFreq", 2 }, { "midGain", -3 } } },
            { "Snare (4.8k)", { { "gain", 9 }, { "midFreq", 5 }, { "midGain", 4 }, { "lowFreq", 4 }, { "lowGain", 3 }, { "hpf", 2 } } },
            { "Guitarra (1.6k)", { { "gain", 8 }, { "midFreq", 3 }, { "midGain", 3 }, { "hpf", 2 }, { "hf", 2 } } },
            { "Bus suave (LINE 0)", { { "gain", 17 }, { "hf", 1.5f }, { "lowFreq", 1 }, { "lowGain", 1.5f } } },
            { "Master color (LINE +5)", { { "gain", 18 }, { "hf", 2 }, { "lowFreq", 1 }, { "lowGain", 1 } } } };
        return p;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout Top73Processor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;
    auto F = [&ps] (const char* id, const char* name, float lo, float hi, float d, float st, const char* unit)
    { ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, st), d, AudioParameterFloatAttributes().withLabel (unit))); };
    auto C = [&ps] (const char* id, const char* name, StringArray items, int d) { ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, d)); };
    auto B = [&ps] (const char* id, const char* name, bool d) { ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, d)); };
    C ("gain", "Gain (Mic/Line)", n73::gainNames(), 6);
    F ("hf", "High 12 kHz", -16, 16, 0, 0.1f, "dB");
    C ("midFreq", "Mid Freq", { "OFF", "0.36 kHz", "0.7 kHz", "1.6 kHz", "3.2 kHz", "4.8 kHz", "7.2 kHz" }, 3);
    F ("midGain", "Mid Gain", -18, 18, 0, 0.1f, "dB");
    C ("lowFreq", "Low Freq", { "OFF", "35 Hz", "60 Hz", "110 Hz", "220 Hz" }, 2);
    F ("lowGain", "Low Gain", -16, 16, 0, 0.1f, "dB");
    C ("hpf", "High Pass", { "OFF", "50 Hz", "80 Hz", "160 Hz", "300 Hz" }, 0);
    B ("eq", "EQ In", true);
    B ("phase", "Phase", false);
    B ("auto", "Auto Gain", true);
    F ("out", "Output Level", -20, 10, 0, 0.1f, "dB");
    C ("meter", "Meter", { "I/P", "O/P" }, 1);
    B ("power", "Power", true);
    return { ps.begin(), ps.end() };
}

Top73Processor::Top73Processor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true).withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Top73", createLayout()) {}

bool Top73Processor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    return in == out && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

n73::Settings Top73Processor::settings() const
{
    auto f = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    n73::Settings s;
    s.gain = (int) f ("gain"); s.midFreq = (int) f ("midFreq"); s.lowFreq = (int) f ("lowFreq"); s.hpf = (int) f ("hpf");
    s.hf = f ("hf"); s.midGain = f ("midGain"); s.lowGain = f ("lowGain"); s.eq = f ("eq") > 0.5f;
    return s;
}

void Top73Processor::prepareToPlay (double sr, int block)
{
    os.reset(); os.initProcessing ((size_t) block);
    latency = (int) os.getLatencyInSamples(); setLatencySamples (latency);
    for (auto& c : ch) c.prepare (sr * 2.0);
    designed = false;
    dry.setSize (2, block);
    for (auto& d : delay) d.assign ((size_t) std::max (1, latency), 0.0f);
    delayPos = 0;
    outSm.reset (sr, 0.05); onSm.reset (sr, 0.03); compSm.reset (sr, 0.05);
    outSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("out")->load()));
    onSm.setCurrentAndTargetValue (apvts.getRawParameterValue ("power")->load() > 0.5f ? 1.0f : 0.0f);
    compSm.setCurrentAndTargetValue (1.0f);
}

void Top73Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples(), chs = std::min (2, getTotalNumInputChannels());
    if (n == 0 || chs == 0) return;
    if (n > dry.getNumSamples()) dry.setSize (2, n, false, false, true);
    inPeak.store (std::max (inPeak.load(), buffer.getMagnitude (0, n)));
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
    const float drive = juce::Decibels::decibelsToGain (n73::driveDb (s.gain));
    if (! designed || ! (s == last))
    {
        const auto d = n73::design (s, getSampleRate() * 2.0);
        for (auto& c : ch) c.set (d, drive);
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
    // AUTO: compensa la ganancia de la perilla roja (solo cambia el color, no el volumen)
    compSm.setTargetValue (apvts.getRawParameterValue ("auto")->load() > 0.5f ? 1.0f / drive : 1.0f);
    outSm.setTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("out")->load()) * (apvts.getRawParameterValue ("phase")->load() > 0.5f ? -1.0f : 1.0f));
    onSm.setTargetValue (apvts.getRawParameterValue ("power")->load() > 0.5f ? 1.0f : 0.0f);
    for (int i = 0; i < n; ++i)
    {
        const float g = outSm.getNextValue() * compSm.getNextValue(), on = onSm.getNextValue();
        for (int c = 0; c < chs; ++c)
        {
            auto* d = buffer.getWritePointer (c);
            d[i] = dry.getSample (c, i) * (1.0f - on) + d[i] * g * on;
        }
    }
    outPeak.store (std::max (outPeak.load(), buffer.getMagnitude (0, n)));
}

int Top73Processor::getNumPrograms() { return (int) presets().size(); }
const juce::String Top73Processor::getProgramName (int i) { return juce::isPositiveAndBelow (i, getNumPrograms()) ? presets()[(size_t) i].name : ""; }
void Top73Processor::setCurrentProgram (int i)
{
    if (! juce::isPositiveAndBelow (i, getNumPrograms())) return;
    currentProgram = i;
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            if (r->getParameterID() != "power" && r->getParameterID() != "meter" && r->getParameterID() != "auto") r->setValueNotifyingHost (r->getDefaultValue());
    for (auto& [id, v] : presets()[(size_t) i].v)
        if (auto* r = apvts.getParameter (id)) r->setValueNotifyingHost (r->convertTo0to1 (v));
}
void Top73Processor::getStateInformation (juce::MemoryBlock& dest)
{
    auto st = apvts.copyState(); st.setProperty ("program", currentProgram, nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary (*xml, dest);
}
void Top73Processor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto t = juce::ValueTree::fromXml (*xml);
        if (t.isValid() && t.hasType (apvts.state.getType())) { currentProgram = (int) t.getProperty ("program", 0); apvts.replaceState (t); }
    }
}
juce::AudioProcessorEditor* Top73Processor::createEditor() { return new Top73Editor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new Top73Processor(); }
