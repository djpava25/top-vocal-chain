#include "TopLockerProcessor.h"
#include "TopLockerEditor.h"

namespace
{
    juce::StringArray micNames() { juce::StringArray s; for (auto& m : lk::mics()) s.add (juce::String (m.line1) + " " + m.line2); return s; }
    juce::StringArray sourceNames() { juce::StringArray s; for (auto& m : tmic::sources()) s.add (juce::String::fromUTF8 (m.name)); return s; }
    juce::StringArray preNames() { juce::StringArray s; for (auto& m : lk::preamps()) s.add (m.name); return s; }
    juce::StringArray roomNames() { juce::StringArray s; for (auto& m : lk::rooms()) s.add (m.name); return s; }
    struct Preset { const char* name; std::vector<std::pair<const char*, float>> v; };
    const std::vector<Preset>& presets()
    {
        static const std::vector<Preset> p {
            { "Modern Vocal Chain", {} },
            { "Male Vocal", { { "mic", 10 }, { "pre", 1 }, { "preGain", 4 }, { "prox", 40 }, { "low", 1 }, { "high", 1 }, { "room", 1 }, { "roomAmt", 15 } } },
            { "Female Vocal", { { "mic", 4 }, { "pre", 3 }, { "preGain", 3 }, { "prox", 55 }, { "mid", -0.5f }, { "high", 1.5f }, { "room", 2 }, { "roomAmt", 20 } } },
            { "Trap", { { "mic", 3 }, { "pre", 2 }, { "preGain", 5 }, { "prox", 35 }, { "high", 2 }, { "sat", 30 }, { "roomAmt", 10 } } },
            { "Dembow", { { "mic", 2 }, { "pre", 2 }, { "preGain", 5.5f }, { "mid", 1.5f }, { "high", 2 }, { "sat", 35 }, { "room", 0 } } },
            { "Reggaeton", { { "mic", 5 }, { "pre", 1 }, { "preGain", 4.5f }, { "low", 0.5f }, { "high", 1.5f }, { "sat", 25 }, { "roomAmt", 15 } } },
            { "Rap", { { "mic", 6 }, { "pre", 2 }, { "preGain", 6 }, { "prox", 30 }, { "mid", 2 }, { "high", 2.5f }, { "sat", 30 }, { "room", 0 } } },
            { "R&B", { { "mic", 9 }, { "pre", 4 }, { "preGain", 4 }, { "charMode", 4 }, { "prox", 45 }, { "room", 2 }, { "roomAmt", 25 } } },
            { "Pop", { { "mic", 0 }, { "pre", 3 }, { "preGain", 3 }, { "high", 2 }, { "room", 2 }, { "roomAmt", 20 } } },
            { "Balada", { { "mic", 10 }, { "pre", 6 }, { "preGain", 3.5f }, { "charMode", 1 }, { "character", 60 }, { "room", 3 }, { "roomAmt", 30 } } },
            { "Drill", { { "mic", 3 }, { "pre", 2 }, { "preGain", 6.5f }, { "sat", 40 }, { "prox", 30 }, { "low", -1 }, { "high", 2 }, { "room", 0 } } },
            { "Afrobeat", { { "mic", 4 }, { "pre", 1 }, { "preGain", 4 }, { "high", 1 }, { "room", 2 }, { "roomAmt", 20 } } },
            { "Podcast / Locucion", { { "mic", 11 }, { "pre", 1 }, { "preGain", 4 }, { "prox", 30 }, { "room", 0 } } },
            { "Voz vintage", { { "mic", 8 }, { "pre", 1 }, { "preGain", 5 }, { "charMode", 1 }, { "character", 70 }, { "room", 5 }, { "roomAmt", 30 } } },
            { "Rock", { { "mic", 6 }, { "pre", 2 }, { "preGain", 7 }, { "sat", 45 }, { "mid", 1.5f }, { "room", 2 }, { "roomAmt", 15 } } },
            { "Coro / Gospel", { { "mic", 7 }, { "pre", 1 }, { "preGain", 3.5f }, { "prox", 65 }, { "room", 8 }, { "roomAmt", 35 } } },
            { "Voz con Plate", { { "mic", 4 }, { "pre", 5 }, { "preGain", 3 }, { "high", 1.5f }, { "room", 7 }, { "roomAmt", 25 }, { "roomDecay", 40 } } },
            { "Voz analoga (transformador)", { { "mic", 0 }, { "pre", 1 }, { "preGain", 5 }, { "charMode", 5 }, { "character", 70 }, { "sat", 30 } } },
            { "Lofi / vintage 610", { { "mic", 8 }, { "pre", 6 }, { "preGain", 7 }, { "charMode", 1 }, { "character", 80 }, { "room", 5 }, { "roomAmt", 25 } } } };
        return p;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout TopLockerProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;
    auto C = [&ps] (const char* id, const char* name, StringArray items, int d) { ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, d)); };
    auto B = [&ps] (const char* id, const char* name, bool d) { ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, d)); };
    auto F = [&ps] (const char* id, const char* name, float lo, float hi, float d, float st, const char* unit)
    { ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, st), d, AudioParameterFloatAttributes().withLabel (unit))); };
    C ("mic", "Microfono", micNames(), 3);
    C ("source", "Micro de origen", sourceNames(), 0);
    F ("angle", "Angulo", 0, 90, 0, 1, "deg");
    F ("prox", "Proximidad", 0, 100, 50, 1, "%");
    F ("low", "Low", -12, 12, 0, 0.1f, "dB");
    F ("mid", "Mid", -12, 12, 0, 0.1f, "dB");
    F ("high", "High", -12, 12, 0, 0.1f, "dB");
    C ("lowFreq", "Low Freq", { "60 Hz", "100 Hz", "200 Hz", "300 Hz" }, 1);
    C ("lowType", "Low Tipo", { "Shelf", "Bell" }, 0);
    C ("midFreq", "Mid Freq", { "400 Hz", "800 Hz", "1.2 kHz", "2 kHz", "3 kHz", "5 kHz" }, 2);
    C ("highFreq", "High Freq", { "5 kHz", "8 kHz", "10 kHz", "12 kHz", "16 kHz" }, 2);
    C ("highType", "High Tipo", { "Shelf", "Bell" }, 0);
    F ("character", "Character", 0, 100, 50, 1, "%");
    C ("charMode", "Circuito de caracter", { "Modern", "Vintage", "Smooth", "Clean", "Tube", "Transformer", "Analog" }, 0);
    F ("sat", "Saturation", 0, 100, 15, 1, "%");
    F ("out", "Output", -18, 18, 0, 0.1f, "dB");
    C ("pre", "Mic Preamp", preNames(), 1);
    F ("preGain", "Preamp Drive", 0, 10, 3.5f, 0.1f, "");
    F ("preInput", "Preamp Input", -12, 18, 0, 0.1f, "dB");
    C ("room", "Room", roomNames(), 1);
    F ("roomAmt", "Room Amount", 0, 100, 20, 1, "%");
    F ("roomSize", "Room Size", 0, 100, 50, 1, "%");
    F ("roomEarly", "Early Reflections", 0, 100, 50, 1, "%");
    F ("roomDecay", "Decay", 0, 100, 50, 1, "%");
    F ("roomWidth", "Width", 0, 100, 50, 1, "%");
    F ("roomDamp", "Damping", 0, 100, 50, 1, "%");
    F ("mix", "Mix", 0, 100, 100, 1, "%");
    B ("phase", "Fase", false);
    B ("power", "Encendido", true);
    return { ps.begin(), ps.end() };
}

TopLockerProcessor::TopLockerProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TopLocker", createLayout())
{
    presetName = presets()[0].name;
    builder.startThread();
}
TopLockerProcessor::~TopLockerProcessor() { builder.stopThread (2000); }

lk::RoomCtl TopLockerProcessor::roomCtl() const
{
    auto f = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    lk::RoomCtl c; c.size = f ("roomSize"); c.early = f ("roomEarly"); c.decay = f ("roomDecay"); c.width = f ("roomWidth"); c.damp = f ("roomDamp");
    return c;
}
void TopLockerProcessor::requestRoom (int room, const lk::RoomCtl& c)
{
    const juce::ScopedTryLock sl (roomLock);   // nunca bloquea el audio
    if (sl.isLocked()) { wantRoom = room; wantCtl = c; }
}
void TopLockerProcessor::Builder::run()
{
    while (! threadShouldExit())
    {
        int room; lk::RoomCtl c; double sr;
        {
            const juce::ScopedLock sl (owner.roomLock);
            room = owner.wantRoom; c = owner.wantCtl; sr = owner.roomSr;
            if (room <= 0 || (room == owner.builtRoom && c == owner.builtCtl)) room = -1;
        }
        if (room > 0)
        {
            auto ir = lk::makeRoom (room, sr, c);
            owner.conv.loadImpulseResponse (std::move (ir), sr, juce::dsp::Convolution::Stereo::yes, juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
            const juce::ScopedLock sl (owner.roomLock);
            owner.builtRoom = room; owner.builtCtl = c;
        }
        wait (40);
    }
}

bool TopLockerProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    return in == out && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

void TopLockerProcessor::prepareToPlay (double sr, int block)
{
    os.reset(); os.initProcessing ((size_t) block);
    latency = (int) os.getLatencyInSamples();
    setLatencySamples (latency);
    engine.prepare (sr * 2.0);
    dry.setSize (2, block); roomBuf.setSize (2, block);
    for (auto& d : delay) d.assign ((size_t) juce::jmax (1, latency), 0.0f);
    delayPos = 0;
    const juce::dsp::ProcessSpec spec { sr, (juce::uint32) block, 2 };
    conv.prepare (spec);
    {
        const juce::ScopedLock sl (roomLock);
        roomSr = sr; builtRoom = -1;
        wantRoom = (int) apvts.getRawParameterValue ("room")->load(); wantCtl = roomCtl();
    }
    builder.notify();
    outSm.reset (sr, 0.05); mixSm.reset (sr, 0.05); roomSm.reset (sr, 0.08);
    outSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("out")->load()));
    mixSm.setCurrentAndTargetValue (apvts.getRawParameterValue ("mix")->load() * 0.01f);
    roomSm.setCurrentAndTargetValue (0.0f);
}

lk::Params TopLockerProcessor::params() const
{
    auto f = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    lk::Params p;
    p.mic = (int) f ("mic"); p.source = (int) f ("source"); p.pre = (int) f ("pre"); p.charMode = (int) f ("charMode");
    p.angle = f ("angle"); p.prox = f ("prox");
    p.lowFreq = (int) f ("lowFreq"); p.lowType = (int) f ("lowType"); p.midFreq = (int) f ("midFreq"); p.highFreq = (int) f ("highFreq"); p.highType = (int) f ("highType");
    p.preInput = f ("preInput"); p.low = f ("low"); p.mid = f ("mid"); p.high = f ("high");
    p.character = f ("character"); p.sat = f ("sat"); p.preGain = f ("preGain");
    return p;
}

void TopLockerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples(), chs = juce::jmin (2, getTotalNumInputChannels());
    if (n == 0 || chs == 0) return;
    if (n > dry.getNumSamples()) { dry.setSize (2, n, false, false, true); roomBuf.setSize (2, n, false, false, true); }
    inPeak.store (std::max (inPeak.load(), buffer.getMagnitude (0, n)));
    const bool on = apvts.getRawParameterValue ("power")->load() > 0.5f;

    // senal seca retrasada igual que la procesada (latencia del sobremuestreo)
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
    if (! on)
    {
        for (int c = 0; c < chs; ++c) buffer.copyFrom (c, 0, dry, c, 0, n);
        outPeak.store (std::max (outPeak.load(), buffer.getMagnitude (0, n)));
        return;
    }

    engine.configure (params());
    {
        juce::dsp::AudioBlock<float> blk (buffer.getArrayOfWritePointers(), (size_t) chs, (size_t) n);
        auto up = os.processSamplesUp (blk);
        for (int c = 0; c < chs; ++c)
        {
            auto* d = up.getChannelPointer ((size_t) c);
            for (size_t i = 0; i < up.getNumSamples(); ++i) d[i] = engine.process (c, d[i]);
        }
        os.processSamplesDown (blk);
    }

    // sala
    const int room = (int) apvts.getRawParameterValue ("room")->load();
    if (room > 0) requestRoom (room, roomCtl());
    roomSm.setTargetValue (room > 0 ? apvts.getRawParameterValue ("roomAmt")->load() * 0.01f * engine.roomFactor() * 0.8f : 0.0f);
    if (room > 0 || roomSm.isSmoothing())
    {
        for (int c = 0; c < 2; ++c) roomBuf.copyFrom (c, 0, buffer, juce::jmin (c, chs - 1), 0, n);
        if (room > 0)
        {
            juce::dsp::AudioBlock<float> rb (roomBuf.getArrayOfWritePointers(), 2, (size_t) n);
            conv.process (juce::dsp::ProcessContextReplacing<float> (rb));
        }
        else roomBuf.clear();
        for (int i = 0; i < n; ++i)
        {
            const float g = roomSm.getNextValue();
            if (chs == 1) buffer.getWritePointer (0)[i] += g * 0.5f * (roomBuf.getSample (0, i) + roomBuf.getSample (1, i));
            else for (int c = 0; c < 2; ++c) buffer.getWritePointer (c)[i] += g * roomBuf.getSample (c, i);
        }
    }

    // mezcla y salida
    outSm.setTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("out")->load()) * (apvts.getRawParameterValue ("phase")->load() > 0.5f ? -1.0f : 1.0f));
    mixSm.setTargetValue (apvts.getRawParameterValue ("mix")->load() * 0.01f);
    for (int i = 0; i < n; ++i)
    {
        const float g = outSm.getNextValue(), m = mixSm.getNextValue();
        for (int c = 0; c < chs; ++c)
        {
            auto* d = buffer.getWritePointer (c);
            d[i] = (dry.getSample (c, i) * (1.0f - m) + d[i] * m) * g;
        }
    }
    outPeak.store (std::max (outPeak.load(), buffer.getMagnitude (0, n)));
}

int TopLockerProcessor::getNumPrograms() { return (int) presets().size(); }
const juce::String TopLockerProcessor::getProgramName (int i) { return juce::isPositiveAndBelow (i, getNumPrograms()) ? presets()[(size_t) i].name : ""; }
void TopLockerProcessor::setCurrentProgram (int i)
{
    if (! juce::isPositiveAndBelow (i, getNumPrograms())) return;
    currentProgram = i;
    presetName = presets()[(size_t) i].name;
    // el micro de origen (con el que grabaste) y el encendido no cambian con el preset
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            if (r->getParameterID() != "power" && r->getParameterID() != "source") r->setValueNotifyingHost (r->getDefaultValue());
    for (auto& [id, v] : presets()[(size_t) i].v)
        if (auto* r = apvts.getParameter (id)) r->setValueNotifyingHost (r->convertTo0to1 (v));
}

juce::File TopLockerProcessor::userPresetDir()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("TopVocal").getChildFile ("Top Locker").getChildFile ("Presets");
}
juce::Array<juce::File> TopLockerProcessor::userPresets() const
{
    auto a = userPresetDir().findChildFiles (juce::File::findFiles, false, "*.tlpreset");
    a.sort();
    return a;
}
bool TopLockerProcessor::saveUserPreset (const juce::File& f)
{
    auto s = apvts.copyState(); s.removeProperty ("program", nullptr);
    auto xml = s.createXml();
    if (xml == nullptr) return false;
    f.getParentDirectory().createDirectory();
    if (! xml->writeTo (f)) return false;
    presetName = f.getFileNameWithoutExtension();
    return true;
}
bool TopLockerProcessor::loadUserPreset (const juce::File& f)
{
    auto xml = juce::XmlDocument::parse (f);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return false;
    const auto t = juce::ValueTree::fromXml (*xml);
    const float src = apvts.getRawParameterValue ("source")->load();
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
        {
            const auto id = r->getParameterID();
            if (id == "power" || id == "source") continue;
            const auto child = t.getChildWithProperty ("id", id);
            if (child.isValid()) r->setValueNotifyingHost (r->convertTo0to1 ((float) child.getProperty ("value")));
        }
    juce::ignoreUnused (src);
    presetName = f.getFileNameWithoutExtension();
    return true;
}

void TopLockerProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto s = apvts.copyState(); s.setProperty ("program", currentProgram, nullptr); s.setProperty ("presetName", presetName, nullptr);
    if (auto xml = s.createXml()) copyXmlToBinary (*xml, dest);
}
void TopLockerProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto t = juce::ValueTree::fromXml (*xml);
        if (t.isValid())
        {
            currentProgram = (int) t.getProperty ("program", 0);
            presetName = t.getProperty ("presetName", presets()[0].name).toString();
            apvts.replaceState (t);
        }
    }
}
juce::AudioProcessorEditor* TopLockerProcessor::createEditor() { return new TopLockerEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TopLockerProcessor(); }
