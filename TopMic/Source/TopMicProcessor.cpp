#include "TopMicProcessor.h"
#include "TopMicEditor.h"

namespace
{
    juce::StringArray modelNames() { juce::StringArray s; for (auto& m : tmic::models()) s.add (m.name); return s; }
    juce::StringArray sourceNames() { juce::StringArray s; for (auto& m : tmic::sources()) s.add (juce::String::fromUTF8 (m.name)); return s; }
    struct Preset { const char* name; std::vector<std::pair<const char*, float>> v; };
    const std::vector<Preset>& presets()
    {
        static const std::vector<Preset> p {
            { "Por defecto", {} },
            { "Voz pop brillante (desde USB)", { { "model", 5 }, { "source", 4 }, { "prox", 20 } } },
            { "Voz de trap / rap (desde dinamico)", { { "model", 5 }, { "source", 1 }, { "prox", 35 }, { "filter", 1 } } },
            { "Voz R&B suave", { { "model", 1 }, { "source", 3 }, { "prox", 25 } } },
            { "Voz clasica de estudio", { { "model", 0 }, { "source", 3 }, { "prox", 10 } } },
            { "Locutor de radio", { { "model", 9 }, { "source", 0 }, { "prox", 60 } } },
            { "Podcast desde celular", { { "model", 10 }, { "source", 5 }, { "prox", 30 }, { "filter", 1 } } },
            { "Voz vintage (cinta)", { { "model", 12 }, { "source", 3 }, { "prox", 20 } } },
            { "Guitarra acustica", { { "model", 6 }, { "source", 3 }, { "pattern", 0 } } },
            { "Ampli de guitarra", { { "model", 8 }, { "source", 0 }, { "axis", 20 } } },
            { "Voz al frente (valvula de referencia, XLR)", { { "model", 14 }, { "source", 3 }, { "prox", 20 }, { "filter", 1 } } },
            { "Dual: valvula + cinta", { { "model", 0 }, { "dual", 1 }, { "modelB", 11 }, { "blend", 35 } } } };
        return p;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout TopMicProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;
    auto C = [&ps] (const char* id, const char* name, StringArray items, int d) { ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, d)); };
    auto B = [&ps] (const char* id, const char* name, bool d) { ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, d)); };
    auto F = [&ps] (const char* id, const char* name, float lo, float hi, float d, float st, const char* unit)
    { ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, st), d, AudioParameterFloatAttributes().withLabel (unit))); };
    C ("model", "Microfono", modelNames(), 0);
    C ("source", "Micro de origen", sourceNames(), 0);
    C ("pattern", "Patron", { "Omni", "Wide", "Cardioid", "Hyper", "Figure-8" }, 2);
    C ("filter", "Filtro", { "Off", "1", "2", "3" }, 0);
    F ("axis", "Eje", 0.0f, 180.0f, 0.0f, 1.0f, "deg");
    F ("prox", "Proximidad", -100.0f, 100.0f, 0.0f, 1.0f, "%");
    F ("out", "Salida", -12.0f, 12.0f, 0.0f, 0.1f, "dB");
    B ("rev", "Reverso", false);
    B ("phase", "Fase", false);
    B ("power", "Encendido", true);
    B ("dual", "Dual", false);
    C ("modelB", "Microfono B", modelNames(), 11);
    F ("blend", "Mezcla A/B", 0.0f, 100.0f, 50.0f, 1.0f, "%");
    return { ps.begin(), ps.end() };
}

TopMicProcessor::TopMicProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TopMic", createLayout()) {}

bool TopMicProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    return in == out && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

void TopMicProcessor::prepareToPlay (double sr, int)
{
    micA.prepare (sr); micB.prepare (sr);
    outSm.reset (sr, 0.05); blendSm.reset (sr, 0.05);
    outSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("out")->load()));
    blendSm.setCurrentAndTargetValue (apvts.getRawParameterValue ("blend")->load() * 0.01f);
}

tmic::Settings TopMicProcessor::settings (bool b) const
{
    auto f = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    tmic::Settings s;
    s.model = (int) f (b ? "modelB" : "model"); s.source = (int) f ("source"); s.pattern = (int) f ("pattern");
    s.filter = (int) f ("filter"); s.axis = f ("axis"); s.prox = f ("prox"); s.rev = f ("rev") > 0.5f;
    return s;
}
int TopMicProcessor::effectivePattern (bool b) const
{
    const auto s = settings (b);
    return tmic::nearestPattern (tmic::models()[(size_t) s.model], s.pattern);
}

void TopMicProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples(), chs = juce::jmin (2, getTotalNumInputChannels());
    if (n == 0 || chs == 0) return;
    inPeak.store (std::max (inPeak.load(), buffer.getMagnitude (0, n)));
    if (apvts.getRawParameterValue ("power")->load() < 0.5f) return;
    micA.configure (settings (false));
    const bool dual = apvts.getRawParameterValue ("dual")->load() > 0.5f;
    if (dual) micB.configure (settings (true));
    outSm.setTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("out")->load()) * (apvts.getRawParameterValue ("phase")->load() > 0.5f ? -1.0f : 1.0f));
    blendSm.setTargetValue (dual ? apvts.getRawParameterValue ("blend")->load() * 0.01f : 0.0f);
    float* d[2] { buffer.getWritePointer (0), chs > 1 ? buffer.getWritePointer (1) : nullptr };
    for (int i = 0; i < n; ++i)
    {
        const float g = outSm.getNextValue(), bl = blendSm.getNextValue();
        for (int c = 0; c < chs; ++c)
        {
            const float x = d[c][i];
            float y = micA.process (c, x);
            if (dual) y = y * (1.0f - bl) + micB.process (c, x) * bl;
            d[c][i] = y * g;
        }
    }
}

int TopMicProcessor::getNumPrograms() { return (int) presets().size(); }
const juce::String TopMicProcessor::getProgramName (int i) { return juce::isPositiveAndBelow (i, getNumPrograms()) ? presets()[(size_t) i].name : ""; }
void TopMicProcessor::setCurrentProgram (int i)
{
    if (! juce::isPositiveAndBelow (i, getNumPrograms())) return;
    currentProgram = i;
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm)) if (r->getParameterID() != "power") r->setValueNotifyingHost (r->getDefaultValue());
    for (auto& [id, v] : presets()[(size_t) i].v)
        if (auto* r = apvts.getParameter (id)) r->setValueNotifyingHost (r->convertTo0to1 (v));
}
void TopMicProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto s = apvts.copyState(); s.setProperty ("program", currentProgram, nullptr);
    if (auto xml = s.createXml()) copyXmlToBinary (*xml, dest);
}
void TopMicProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto t = juce::ValueTree::fromXml (*xml);
        if (t.isValid()) { currentProgram = (int) t.getProperty ("program", 0); apvts.replaceState (t); }
    }
}
juce::AudioProcessorEditor* TopMicProcessor::createEditor() { return new TopMicEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TopMicProcessor(); }
