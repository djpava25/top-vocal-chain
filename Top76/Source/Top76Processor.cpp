#include "Top76Processor.h"
#include "Top76Editor.h"

namespace
{
    // ratio: 0 = 4:1, 1 = 8:1, 2 = 12:1, 3 = 20:1, 4 = ALL BUTTONS
    struct Preset { const char* name; std::vector<std::pair<const char*, float>> v; };
    const std::vector<Preset>& presets()
    {
        static const std::vector<Preset> p {
            { "Inicio", {} },
            { "Voz pop al frente", { { "input", 4.0f }, { "output", 2.0f }, { "attack", 3.0f }, { "release", 6.0f }, { "ratio", 0 } } },
            { "Voz rap agresiva", { { "input", 9.0f }, { "output", -1.0f }, { "attack", 5.0f }, { "release", 7.0f }, { "ratio", 1 } } },
            { "Voz reggaeton (deja el golpe)", { { "input", 6.0f }, { "output", 1.0f }, { "attack", 2.0f }, { "release", 6.5f }, { "ratio", 0 }, { "hpf", 90.0f } } },
            { "Voz con mucho aire (paralelo)", { { "input", 12.0f }, { "output", 0.0f }, { "attack", 6.0f }, { "release", 7.0f }, { "ratio", 3 }, { "mix", 45.0f } } },
            { "Bombo con punch", { { "input", 6.0f }, { "output", 1.5f }, { "attack", 1.5f }, { "release", 6.0f }, { "ratio", 0 } } },
            { "Caja con cuerpo", { { "input", 8.0f }, { "output", 0.0f }, { "attack", 2.5f }, { "release", 6.5f }, { "ratio", 1 } } },
            { "Bajo / 808 parejo", { { "input", 5.0f }, { "output", 1.0f }, { "attack", 3.0f }, { "release", 3.5f }, { "ratio", 0 }, { "detector", 0 } } },
            { "Bateria ALL BUTTONS (paralelo)", { { "input", 12.0f }, { "output", -2.0f }, { "attack", 6.0f }, { "release", 7.0f }, { "ratio", 4 }, { "mix", 40.0f } } },
            { "Room aplastado (ALL)", { { "input", 15.0f }, { "output", -4.0f }, { "attack", 7.0f }, { "release", 7.0f }, { "ratio", 4 }, { "detector", 1 } } },
            { "Guitarra electrica", { { "input", 6.0f }, { "output", 0.0f }, { "attack", 3.0f }, { "release", 5.0f }, { "ratio", 0 } } },
            { "Guitarra acustica", { { "input", 3.0f }, { "output", 1.0f }, { "attack", 2.0f }, { "release", 5.0f }, { "ratio", 0 }, { "hpf", 70.0f } } },
            { "Piano", { { "input", 2.0f }, { "output", 0.5f }, { "attack", 2.0f }, { "release", 4.0f }, { "ratio", 0 } } },
            { "Bus de bateria suave", { { "input", 0.0f }, { "output", 0.5f }, { "attack", 1.0f }, { "release", 6.0f }, { "ratio", 0 }, { "hpf", 80.0f } } },
            { "Limitador de picos 20:1", { { "input", 3.0f }, { "output", 0.0f }, { "attack", 7.0f }, { "release", 6.0f }, { "ratio", 3 } } },
            { "Solo color (ATTACK OFF)", { { "input", 8.0f }, { "output", -8.0f }, { "attack", 0.0f } } } };
        return p;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout Top76Processor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;
    auto dB = [] (float v, int) { return (v >= 0 ? "+" : "") + String (v, 1) + " dB"; };
    auto F = [&ps] (const char* id, const char* name, NormalisableRange<float> r, float d, std::function<String (float, int)> txt)
    { ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, d, AudioParameterFloatAttributes().withStringFromValueFunction (txt))); };
    auto C = [&ps] (const char* id, const char* name, StringArray items, int d) { ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, d)); };
    auto B = [&ps] (const char* id, const char* name, bool d) { ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, d)); };
    F ("input", "Input", { -24.0f, 30.0f, 0.01f }, 0.0f, dB);
    F ("output", "Output", { -24.0f, 24.0f, 0.01f }, 0.0f, dB);
    F ("attack", "Attack", { 0.0f, 7.0f, 0.01f }, 3.0f, [] (float v, int)
       { if (v < attackOffBelow) return String ("OFF"); const double t = f76::attackTau (std::max (1.0, (double) v)); return String (v, 1) + "  (" + String (juce::roundToInt (t * 1e6)) + " us)"; });
    F ("release", "Release", { 1.0f, 7.0f, 0.01f }, 5.0f, [] (float v, int)
       { const double t = f76::releaseTau (v); return String (v, 1) + "  (" + (t >= 1.0 ? String (t, 2) + " s" : String (juce::roundToInt (t * 1000)) + " ms") + ")"; });
    C ("ratio", "Ratio", { "4:1", "8:1", "12:1", "20:1", "ALL" }, 0);
    C ("meter", "Meter", { "GR", "+8", "+4", "OFF" }, 0);
    F ("mix", "Mix", { 0.0f, 100.0f, 0.1f }, 100.0f, [] (float v, int) { return String (v, 0) + " %"; });
    F ("hpf", "SC HPF", NormalisableRange<float> (20.0f, 500.0f, 0.1f, 0.45f), 20.0f, [] (float v, int) { return v <= 21.0f ? String ("OFF") : String (juce::roundToInt (v)) + " Hz"; });
    B ("link", "Stereo Link", true);
    C ("detector", "Detector", { "CLEAN", "VINTAGE" }, 0);
    return { ps.begin(), ps.end() };
}

Top76Processor::Top76Processor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true).withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Top76", createLayout())
{
    using OS = juce::dsp::Oversampling<float>;
    os.clearOversamplingStages();
    os.addOversamplingStage (OS::filterHalfBandFIREquiripple, 0.06f, -110.0f, 0.06f, -110.0f);
    os.addOversamplingStage (OS::filterHalfBandFIREquiripple, 0.20f, -100.0f, 0.20f, -100.0f);
    os.setUsingIntegerLatency (true);
}

bool Top76Processor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    return in == out && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

f76::Settings Top76Processor::settings() const
{
    auto f = [this] (const char* id) { return (double) apvts.getRawParameterValue (id)->load(); };
    f76::Settings s;
    s.inDb = f ("input"); s.outDb = f ("output");
    const double a = f ("attack");
    s.attackOff = a < attackOffBelow; s.attack = std::max (1.0, a);
    s.release = f ("release"); s.ratio = juce::jlimit (0, 4, (int) std::lround (f ("ratio")));
    s.hpf = f ("hpf"); s.link = f ("link") > 0.5; s.clean = f ("detector") < 0.5;
    return s;
}

void Top76Processor::prepareToPlay (double sr, int block)
{
    os.reset(); os.initProcessing ((size_t) std::max (1, block));
    latency = (int) std::round (os.getLatencyInSamples()); setLatencySamples (latency);
    eng.prepare (sr * (double) os.getOversamplingFactor(), std::min (2, std::max (1, getTotalNumInputChannels())));
    eng.set (settings(), true);
    dry.setSize (2, block);
    for (auto& d : delay) d.assign ((size_t) std::max (1, latency), 0.0f);
    delayPos = 0;
    mixSm.reset (sr, 0.05); onSm.reset (sr, 0.03);
    mixSm.setCurrentAndTargetValue (apvts.getRawParameterValue ("mix")->load() * 0.01f);
    onSm.setCurrentAndTargetValue (powered() ? 1.0f : 0.0f);
    avg = 0; avgA = 1.0 - std::exp (-1.0 / (0.010 * sr));
}

void Top76Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples(), chs = std::min (2, getTotalNumInputChannels());
    for (int c = chs; c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, n);
    if (n == 0 || chs == 0) return;
    if (n > dry.getNumSamples()) dry.setSize (2, n, false, false, true);
    // copia seca alineada con la latencia del sobremuestreo (MIX y OFF sin desfase)
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

    eng.set (settings());
    {
        juce::dsp::AudioBlock<float> blk (buffer.getArrayOfWritePointers(), (size_t) chs, (size_t) n);
        auto up = os.processSamplesUp (blk);
        float* p0 = up.getChannelPointer (0);
        float* p1 = chs > 1 ? up.getChannelPointer (1) : nullptr;
        double v[2];
        for (size_t i = 0; i < up.getNumSamples(); ++i)
        {
            v[0] = (double) p0[i]; v[1] = p1 != nullptr ? (double) p1[i] : 0.0;
            eng.process (v);
            p0[i] = (float) v[0]; if (p1 != nullptr) p1[i] = (float) v[1];
        }
        os.processSamplesDown (blk);
    }
    mixSm.setTargetValue (apvts.getRawParameterValue ("mix")->load() * 0.01f);
    onSm.setTargetValue (powered() ? 1.0f : 0.0f);
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
    grDb.store ((float) eng.grDb());
    outAvg.store ((float) avg);
}

int Top76Processor::getNumPrograms() { return (int) presets().size(); }
const juce::String Top76Processor::getProgramName (int i) { return juce::isPositiveAndBelow (i, getNumPrograms()) ? presets()[(size_t) i].name : ""; }
void Top76Processor::setCurrentProgram (int i)
{
    if (! juce::isPositiveAndBelow (i, getNumPrograms())) return;
    currentProgram = i;
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            if (r->getParameterID() != "meter" && r->getParameterID() != "link") r->setValueNotifyingHost (r->getDefaultValue());
    for (auto& [id, v] : presets()[(size_t) i].v)
        if (auto* r = apvts.getParameter (id)) r->setValueNotifyingHost (r->convertTo0to1 (v));
}
void Top76Processor::getStateInformation (juce::MemoryBlock& dest)
{
    auto st = apvts.copyState(); st.setProperty ("program", currentProgram, nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary (*xml, dest);
}
void Top76Processor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto t = juce::ValueTree::fromXml (*xml);
        if (t.isValid() && t.hasType (apvts.state.getType())) { currentProgram = (int) t.getProperty ("program", 0); apvts.replaceState (t); }
    }
}
juce::AudioProcessorEditor* Top76Processor::createEditor() { return new Top76Editor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new Top76Processor(); }
