#include "TopMeterProcessor.h"
#include "TopMeterEditor.h"

namespace
{
    std::atomic<int> instanceCounter { 0 };
    struct Preset { const char* name; int target; int scale; };
    const Preset presets[] {
        { "Streaming (-14 LUFS)", 1, 0 }, { "Apple Music (-16 LUFS)", 2, 0 }, { "Podcast (-18 LUFS)", 3, 0 },
        { "Broadcast EBU R128 (-23 LUFS)", 4, 0 }, { "Broadcast ATSC A/85 (-24 LUFS)", 5, 0 }, { "Club / Master fuerte (-9 LUFS)", 0, 1 } };
}

const juce::StringArray& TopMeterProcessor::targetNames()
{
    static const juce::StringArray n { "-9 LUFS", "-14 LUFS", "-16 LUFS", "-18 LUFS", "-23 LUFS", "-24 LUFS" };
    return n;
}
const std::vector<float>& TopMeterProcessor::targetValues()
{
    static const std::vector<float> v { -9.0f, -14.0f, -16.0f, -18.0f, -23.0f, -24.0f };
    return v;
}

juce::AudioProcessorValueTreeState::ParameterLayout TopMeterProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;
    auto C = [&ps] (const char* id, const char* name, StringArray items, int def) { ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def)); };
    auto B = [&ps] (const char* id, const char* name, bool def) { ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };
    auto F = [&ps] (const char* id, const char* name, float lo, float hi, float def, float stepv)
    { ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, stepv), def)); };
    C ("lscale", "Escala de sonoridad", { "-18 a +9", "-36 a +18", "-60 a 0 LUFS" }, 0);
    C ("sync", "Sincronia", { "DAW Sync", "Libre" }, 0);
    C ("window", "Ventana", { "10 secs", "30 secs", "1 min", "3 min", "10 min" }, 1);
    B ("autoscroll", "Auto Scroll", true);
    C ("target", "Objetivo", targetNames(), 1);
    C ("graph", "Grafica", { "Short-Term", "Momentary" }, 0);
    B ("tp", "True Peak en barras", true);
    C ("lvscale", "Escala de nivel", { "Non-Linear", "Linear" }, 0);
    C ("pkhold", "Pico retenido", { "1 sec", "3 sec", "5 sec", "Inf" }, 1);
    C ("rmswin", "Ventana RMS", { "300 ms", "500 ms", "1 sec", "3 sec" }, 1);
    F ("vuref", "Referencia VU", -24.0f, -8.0f, -18.0f, 1.0f);
    F ("smin", "Espectro minimo", -120.0f, -40.0f, -120.0f, 1.0f);
    F ("smax", "Espectro maximo", -24.0f, 12.0f, 0.0f, 1.0f);
    C ("shold", "Pico del espectro", { "1 sec", "3 sec", "5 sec", "Inf" }, 1);
    C ("savg", "Promedio del espectro", { "Real Time", "Slow", "Very Slow" }, 0);
    B ("run", "Medir", true);
    return { ps.begin(), ps.end() };
}

TopMeterProcessor::TopMeterProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "TopMeter", createLayout())
{
    instanceNumber = ++instanceCounter;
    scopeL.assign (scopeSize, 0.0f); scopeR.assign (scopeSize, 0.0f); specBuf.assign (specSize, 0.0f);
}
TopMeterProcessor::~TopMeterProcessor() {}

bool TopMeterProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    return in == out && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

void TopMeterProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    meter.prepare (sr);
    lastRmsChoice = -1;
}

float TopMeterProcessor::targetLufs() const
{
    return targetValues()[(size_t) juce::jlimit (0, 5, (int) apvts.getRawParameterValue ("target")->load())];
}

void TopMeterProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples();
    const int ch = juce::jmin (2, getTotalNumInputChannels());
    if (n == 0 || ch == 0) return;
    for (int c = ch; c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, n);

    if (wantReset.exchange (false)) meter.reset();
    const int rc = (int) apvts.getRawParameterValue ("rmswin")->load();
    if (rc != lastRmsChoice) { const float w[] { 0.3f, 0.5f, 1.0f, 3.0f }; meter.setRmsWindow (w[juce::jlimit (0, 3, rc)]); lastRmsChoice = rc; }
    const float vr = apvts.getRawParameterValue ("vuref")->load();
    if (vr != lastVuRef) { meter.setVuRef (vr); lastVuRef = vr; }

    // DAW Sync: se reinicia al arrancar la reproduccion y solo mide mientras suena
    bool playing = true, known = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition()) { playing = pos->getIsPlaying(); known = true; }
    hostPlaying = playing;
    const bool sync = apvts.getRawParameterValue ("sync")->load() < 0.5f;
    if (sync && known && playing && ! wasPlaying) meter.reset();
    wasPlaying = playing;
    const bool run = apvts.getRawParameterValue ("run")->load() > 0.5f;
    if (! run) return;

    const float* L = buffer.getReadPointer (0);
    const float* R = ch > 1 ? buffer.getReadPointer (1) : nullptr;
    meter.process (L, R, n, ! sync || ! known || playing);

    int sw = scopeW.load(), pw = specW.load();
    for (int i = 0; i < n; ++i)
    {
        const float l = L[i], r = R ? R[i] : L[i];
        scopeL[(size_t) (sw & (scopeSize - 1))] = l; scopeR[(size_t) (sw & (scopeSize - 1))] = r; ++sw;
        specBuf[(size_t) (pw & (specSize - 1))] = 0.5f * (l + r); ++pw;
    }
    scopeW = sw; specW = pw;
}

int TopMeterProcessor::getNumPrograms() { return (int) std::size (presets); }
const juce::String TopMeterProcessor::getProgramName (int i) { return juce::isPositiveAndBelow (i, getNumPrograms()) ? presets[i].name : ""; }
void TopMeterProcessor::setCurrentProgram (int i)
{
    if (! juce::isPositiveAndBelow (i, getNumPrograms())) return;
    currentProgram = i;
    auto set = [this] (const char* id, float v) { if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v)); };
    set ("target", (float) presets[i].target);
    set ("lscale", (float) presets[i].scale);
}

void TopMeterProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto s = apvts.copyState();
    s.setProperty ("name", channelName, nullptr);
    s.setProperty ("program", currentProgram, nullptr);
    if (auto xml = s.createXml()) copyXmlToBinary (*xml, dest);
}
void TopMeterProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto t = juce::ValueTree::fromXml (*xml);
        if (! t.isValid()) return;
        channelName = t.getProperty ("name", "MASTER").toString();
        currentProgram = (int) t.getProperty ("program", 0);
        apvts.replaceState (t);
    }
}

juce::AudioProcessorEditor* TopMeterProcessor::createEditor() { return new TopMeterEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TopMeterProcessor(); }
