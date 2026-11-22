#include "TopMiniCHProcessor.h"
#include "TopMiniCHEditor.h"

namespace
{
    struct Preset { const char* name; std::vector<std::pair<const char*, float>> v; };
    const std::vector<Preset>& presets()
    {
        static const std::vector<Preset> p {
            { "Canal limpio", {} },
            { "Voz al frente", { { "hpf", 1 }, { "hf", 5 }, { "lf", -2 }, { "lf_bell", 1 }, { "cmp", 1 }, { "thr", -6 } } },
            { "Voz suave", { { "hpf", 1 }, { "hf", 3 }, { "cmp", 1 }, { "thr", 0 } } },
            { "Rap / voz urbana", { { "hpf", 1 }, { "hf", 6 }, { "hf_bell", 1 }, { "lf", 3 }, { "lf_bell", 1 }, { "cmp", 1 }, { "thr", -12 }, { "color", 1 } } },
            { "Podcast", { { "hpf", 1 }, { "hf", 2 }, { "lf", -3 }, { "lf_bell", 1 }, { "cmp", 1 }, { "thr", -10 } } },
            { "Guitarra acustica", { { "hpf", 1 }, { "hf", 4 }, { "lf", -4 }, { "lf_bell", 1 }, { "cmp", 1 }, { "thr", 2 } } },
            { "Guitarra electrica DI (Hi-Z)", { { "hiz", 1 }, { "line", 1 }, { "hf", 2 }, { "hf_bell", 1 }, { "color", 1 } } },
            { "Bajo DI", { { "hiz", 1 }, { "line", 1 }, { "lf", 4 }, { "hf", -3 }, { "cmp", 1 }, { "thr", -8 } } },
            { "Bombo", { { "lf", 6 }, { "hf", 3 }, { "hf_bell", 1 }, { "cmp", 1 }, { "thr", -4 } } },
            { "Caja", { { "hpf", 1 }, { "lf", 4 }, { "lf_bell", 1 }, { "hf", 5 }, { "cmp", 1 }, { "thr", -6 } } } };
        return p;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout TopMiniCHProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;
    auto B = [&ps] (const char* id, const char* name, bool d) { ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, d)); };
    auto F = [&ps] (const char* id, const char* name, float lo, float hi, float d, const char* unit)
    {
        ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, 0.1f), d,
            AudioParameterFloatAttributes().withLabel (unit).withStringFromValueFunction ([] (float v, int) { return (v > 0.04f ? "+" : "") + String (v, 1); })));
    };
    B ("phase", "Fase", false);
    F ("gain", "Gain", -20.0f, 20.0f, 0.0f, "dB");
    B ("color", "Color analogico", false);
    B ("hpf", "Filtro 75 Hz", false);
    F ("hf", "HF", -15.0f, 15.0f, 0.0f, "dB");
    B ("hf_bell", "HF Bell", false);
    F ("lf", "LF", -15.0f, 15.0f, 0.0f, "dB");
    B ("lf_bell", "LF Bell", false);
    B ("eq_in", "EQ In", true);
    F ("thr", "Threshold", -20.0f, 10.0f, 10.0f, "dBu");
    B ("cmp", "Comp In", false);
    B ("line", "Line", false);
    B ("hiz", "Hi-Z", false);
    return { ps.begin(), ps.end() };
}

TopMiniCHProcessor::TopMiniCHProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TopMiniCH", createLayout()) {}

bool TopMiniCHProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    return in == out && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

void TopMiniCHProcessor::prepareToPlay (double sampleRate, int) { ch.prepare (sampleRate); }

tmc::Params TopMiniCHProcessor::readParams() const
{
    auto f = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    tmc::Params p;
    p.phase = f ("phase") > 0.5f; p.gain = f ("gain"); p.color = f ("color") > 0.5f; p.hpf = f ("hpf") > 0.5f;
    p.hf = f ("hf"); p.hfBell = f ("hf_bell") > 0.5f; p.lf = f ("lf"); p.lfBell = f ("lf_bell") > 0.5f; p.eqIn = f ("eq_in") > 0.5f;
    p.thr = f ("thr"); p.cmpIn = f ("cmp") > 0.5f; p.line = f ("line") > 0.5f; p.hiz = f ("hiz") > 0.5f;
    return p;
}

void TopMiniCHProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples(), chs = juce::jmin (2, getTotalNumInputChannels());
    if (n == 0 || chs == 0) return;
    ch.process (buffer.getWritePointer (0), chs > 1 ? buffer.getWritePointer (1) : nullptr, n, readParams());
    outPeak.store (std::max (outPeak.load(), buffer.getMagnitude (0, n)));
    gr.store (ch.grDb());
}

int TopMiniCHProcessor::getNumPrograms() { return (int) presets().size(); }
const juce::String TopMiniCHProcessor::getProgramName (int i) { return juce::isPositiveAndBelow (i, getNumPrograms()) ? presets()[(size_t) i].name : ""; }
void TopMiniCHProcessor::setCurrentProgram (int i)
{
    if (! juce::isPositiveAndBelow (i, getNumPrograms())) return;
    currentProgram = i;
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            if (r->getParameterID() != "gain") r->setValueNotifyingHost (r->getDefaultValue());
    for (auto& [id, v] : presets()[(size_t) i].v)
        if (auto* r = apvts.getParameter (id)) r->setValueNotifyingHost (r->convertTo0to1 (v));
}

void TopMiniCHProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto s = apvts.copyState(); s.setProperty ("program", currentProgram, nullptr);
    if (auto xml = s.createXml()) copyXmlToBinary (*xml, dest);
}
void TopMiniCHProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto t = juce::ValueTree::fromXml (*xml);
        if (t.isValid()) { currentProgram = (int) t.getProperty ("program", 0); apvts.replaceState (t); }
    }
}

juce::AudioProcessorEditor* TopMiniCHProcessor::createEditor() { return new TopMiniCHEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TopMiniCHProcessor(); }
