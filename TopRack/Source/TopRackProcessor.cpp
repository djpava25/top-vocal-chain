#include "TopRackProcessor.h"
#include "TopRackEditor.h"

using namespace trk;

namespace
{
    struct SlotPreset { int type; std::vector<std::pair<int, float>> knobs; };
    struct ChainPreset { const char* name; std::vector<SlotPreset> slots; };
    const std::vector<ChainPreset>& chains()
    {
        static const std::vector<ChainPreset> c {
            { "Rack vacío", {} },
            { "Voz pop", { { EqConsole, { { 0, 90 }, { 8, 2 }, { 9, 3000 }, { 11, 3 }, { 12, 10000 } } },
                           { CompFet, { { 0, 24 }, { 1, -2 }, { 2, 5 }, { 3, 6 }, { 4, 0 } } },
                           { CompOpto, { { 0, 35 }, { 1, 12 } } },
                           { DeEss, { { 0, 6500 }, { 1, 8 }, { 2, -28 } } },
                           { AirEq, { { 0, 3 }, { 1, 3 } } } } },
            { "Voz urbana / reggaetón", { { EqA, { { 0, 2 }, { 3, 3 }, { 4, 2 }, { 7, 3 }, { 9, 30 } } },
                                          { CompFet, { { 0, 28 }, { 1, -3 }, { 2, 6 }, { 3, 6 }, { 4, 1 } } },
                                          { Stress, { { 0, 10 }, { 4, 2 }, { 6, 1 }, { 7, 50 } } },
                                          { DeEss, { { 0, 6000 }, { 1, 10 }, { 2, -30 } } },
                                          { AirEq, { { 0, 4 }, { 1, 3 } } } } },
            { "Rap agresivo", { { EqConsole, { { 0, 100 }, { 8, 3 }, { 9, 2500 } } },
                                { CompFet, { { 0, 26 }, { 2, 6 }, { 3, 6 }, { 4, 4 }, { 5, 60 } } },
                                { DeEss, { { 0, 6000 }, { 1, 8 }, { 2, -26 } } },
                                { Revive, { { 0, 40 } } } } },
            { "Bombo", { { EqAmerican, { { 0, 1 }, { 1, 7 }, { 3, 0 }, { 4, 3 }, { 5, 0 }, { 6, 6 } } },
                         { CompVca, { { 0, -20 }, { 1, 4 }, { 2, 15 }, { 3, 120 } } },
                         { Bomber, { { 0, 40 }, { 1, 30 } } } } },
            { "Caja", { { EqConsole, { { 0, 80 }, { 5, 3 }, { 6, 200 }, { 8, 3 }, { 9, 5000 } } },
                        { CompFet, { { 0, 20 }, { 2, 3 }, { 3, 5 } } },
                        { Monster, { { 0, 40 }, { 3, 25 } } } } },
            { "Bajo", { { EqA, { { 1, 1 }, { 2, 3 }, { 3, 1 }, { 4, -2 }, { 9, 40 } } },
                        { CompOpto, { { 0, 50 }, { 1, 14 } } },
                        { Revive, { { 1, 40 } } } } },
            { "Guitarra acústica", { { EqConsole, { { 0, 90 }, { 5, -2 }, { 6, 250 }, { 8, 2 }, { 9, 3000 } } },
                                     { CompOpto, { { 0, 30 }, { 1, 11 } } },
                                     { AirEq, { { 0, 3 }, { 1, 2 } } } } },
            { "Bus de batería", { { EqAmerican, { { 0, 1 }, { 1, 6 }, { 5, 2 }, { 6, 6 } } },
                                  { CompBus, { { 0, -12 }, { 1, 1 }, { 2, 4 }, { 3, 4 }, { 4, 3 } } },
                                  { Bomber, { { 0, 30 }, { 2, 20 }, { 3, 60 } } } } },
            { "Mix bus", { { EqA, { { 7, 1.5f }, { 9, 15 } } },
                           { CompBus, { { 0, -8 }, { 1, 0 }, { 2, 5 }, { 3, 4 }, { 4, 2 } } },
                           { Revive, { { 0, 15 }, { 1, 15 } } },
                           { AirEq, { { 0, 1.5f }, { 1, 4 } } } } },
            { "Piano", { { EqConsole, { { 0, 40 }, { 5, -2 }, { 6, 400 }, { 11, 2 }, { 12, 9000 } } },
                         { CompOpto, { { 0, 25 }, { 1, 10 } } },
                         { AirEq, { { 0, 2 }, { 1, 3 } } } } },
        };
        return c;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout TopRackProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;
    auto dbAttr = [] { return AudioParameterFloatAttributes().withLabel ("dB"); };
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "in_gain", 1 }, "Entrada", NormalisableRange<float> (-24, 24, 0.1f), 0.0f, dbAttr()));
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "out_gain", 1 }, "Salida", NormalisableRange<float> (-24, 24, 0.1f), 0.0f, dbAttr()));
    ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mezcla", NormalisableRange<float> (0, 100, 0.1f), 100.0f));
    ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { "oversample", 1 }, "Oversampling 2x", false));
    for (int m = 0; m < 4; ++m) ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "macro" + String (m + 1), 1 }, "Macro " + String (m + 1), NormalisableRange<float> (0, 1), 0.0f));
    StringArray types; for (const auto& d : defs()) types.add (d.name);
    for (int s = 0; s < numSlots; ++s)
    {
        const String n = "Ranura " + String (s + 1) + " ";
        ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { sid (s, "type"), 1 }, n + "modulo", types, 0));
        ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { sid (s, "byp"), 1 }, n + "bypass", false));
        ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { sid (s, "trim"), 1 }, n + "trim", NormalisableRange<float> (-24, 24, 0.1f), 0.0f, dbAttr()));
        ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { sid (s, "sc"), 1 }, n + "sidechain externo", false));
        for (int k = 0; k < maxKnobs; ++k)
            ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { sid (s, "p" + String (k)), 1 }, n + "control " + String (k + 1), NormalisableRange<float> (0, 1), 0.5f));
    }
    return { ps.begin(), ps.end() };
}

TopRackProcessor::TopRackProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                        .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, &undoManager, "TopRack", createLayout())
{
    for (int s = 0; s < numSlots; ++s)
    {
        typeP[(size_t) s] = apvts.getRawParameterValue (sid (s, "type"));
        bypP[(size_t) s] = apvts.getRawParameterValue (sid (s, "byp"));
        trimP[(size_t) s] = apvts.getRawParameterValue (sid (s, "trim"));
        scP[(size_t) s] = apvts.getRawParameterValue (sid (s, "sc"));
        for (int k = 0; k < maxKnobs; ++k) knobP[(size_t) s][(size_t) k] = apvts.getRawParameterValue (sid (s, "p" + juce::String (k)));
    }
    for (int m = 0; m < 4; ++m) macroP[(size_t) m] = apvts.getRawParameterValue ("macro" + juce::String (m + 1));
    rack1.prepare (44100.0);
    rack2.prepare (88200.0);
    startTimerHz (30);
}

TopRackProcessor::~TopRackProcessor() { stopTimer(); }

bool TopRackProcessor::isBusesLayoutSupported (const BusesLayout& l) const
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

void TopRackProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    maxBlock = juce::jmax (512, samplesPerBlock);
    rack1.prepare (sr);
    rack2.prepare (sr * 2.0);
    os = std::make_unique<juce::dsp::Oversampling<float>> (2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    osSc = std::make_unique<juce::dsp::Oversampling<float>> (2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    os->initProcessing ((size_t) maxBlock); osSc->initProcessing ((size_t) maxBlock);
    osLatency = (int) std::round (os->getLatencyInSamples());
    for (auto& d : dry) d.assign ((size_t) maxBlock, 0.0f);
    for (auto& d : scBuf) d.assign ((size_t) maxBlock, 0.0f);
    monoR.assign ((size_t) maxBlock, 0.0f);
    for (auto& d : dryDelay) d.assign ((size_t) juce::jmax (1, osLatency) + 1, 0.0f);
    ddPos = 0;
    osActive = apvts.getRawParameterValue ("oversample")->load() > 0.5f;
    setLatencySamples (osActive ? osLatency : 0);
}

void TopRackProcessor::runRack (Rack& rack, float* L, float* R, int n, const float* scL, const float* scR)
{
    for (int s = 0; s < numSlots; ++s)
    {
        auto& slot = rack.slots[(size_t) s];
        const int t = (int) typeP[(size_t) s]->load();
        if (t != slot.cur)
        {
            slot.cur = t;
            if (t > 0 && slot.mods[(size_t) t] != nullptr) slot.mods[(size_t) t]->reset();
        }
        if (t <= 0 || bypP[(size_t) s]->load() > 0.5f) continue;
        { const int so = soloSlot.load(); if (so >= 0 && so != s && slotType (so) > 0) continue; }
        auto* mod = slot.mods[(size_t) t].get();
        if (mod == nullptr) continue;
        const auto& def = defs()[(size_t) t];
        std::array<float, maxKnobs> v {};
        for (size_t k = 0; k < def.knobs.size(); ++k) v[k] = def.knobs[k].toReal (knobP[(size_t) s][k]->load());
        const bool sc = scP[(size_t) s]->load() > 0.5f && def.dynamics && scL != nullptr;
        mod->process (L, R, n, v.data(), sc ? scL : nullptr, sc ? scR : nullptr);
        // OUT GAIN de la ranura (despues del modulo)
        const float tg = dB2g (trimP[(size_t) s]->load());
        if (std::abs (tg - slot.trimSm) > 1e-5f || tg != 1.0f)
            for (int i = 0; i < n; ++i) { slot.trimSm += (tg - slot.trimSm) * 0.002f; L[i] *= slot.trimSm; R[i] *= slot.trimSm; }
        float pk = 0; for (int i = 0; i < n; ++i) pk = std::max (pk, std::max (std::abs (L[i]), std::abs (R[i])));
        slotOut[(size_t) s].store (pk);
    }
}

void TopRackProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int total = buffer.getNumSamples();
    const int numCh = juce::jmin (2, getMainBusNumInputChannels());
    const bool wantOs = apvts.getRawParameterValue ("oversample")->load() > 0.5f;
    if (wantOs != osActive)
    {
        osActive = wantOs;
        os->reset(); osSc->reset();
        for (auto& d : dryDelay) std::fill (d.begin(), d.end(), 0.0f);
        setLatencySamples (osActive ? osLatency : 0);
    }
    // sidechain
    const float* scp[2] { nullptr, nullptr };
    if (getBusCount (true) > 1)
        if (auto* bus = getBus (true, 1); bus != nullptr && bus->isEnabled() && bus->getNumberOfChannels() > 0)
            for (int c = 0; c < 2; ++c)
            {
                const int idx = getChannelIndexInProcessBlockBuffer (true, 1, juce::jmin (c, bus->getNumberOfChannels() - 1));
                if (idx < buffer.getNumChannels()) scp[c] = buffer.getReadPointer (idx);
            }
    const float inT = dB2g (apvts.getRawParameterValue ("in_gain")->load());
    const float outT = dB2g (apvts.getRawParameterValue ("out_gain")->load());
    const float mixT = apvts.getRawParameterValue ("mix")->load() / 100.0f;
    float pkIn = 0, pkOut = 0;

    for (int pos = 0; pos < total; pos += maxBlock)
    {
        const int n = juce::jmin (maxBlock, total - pos);
        float* L = buffer.getWritePointer (0) + pos;
        float* R = numCh > 1 ? buffer.getWritePointer (1) + pos : monoR.data();   // mono: copia temporal
        if (numCh == 1) std::copy (L, L + n, monoR.begin());
        for (int i = 0; i < n; ++i)
        {
            inSm += (inT - inSm) * 0.002f;
            L[i] *= inSm; R[i] *= inSm;
            pkIn = std::max (pkIn, std::max (std::abs (L[i]), std::abs (R[i])));
        }
        // seco (retrasado igual que el oversampling)
        std::array<float*, 2> dr { dry[0].data(), dry[1].data() };
        if (numCh == 1) dr[1] = dry[0].data();
        const int D = (int) dryDelay[0].size();
        for (int i = 0; i < n; ++i)
        {
            const float xl = L[i], xr = R[i];
            if (osActive)
            {
                const int p = (ddPos + i) % D;
                dr[0][i] = dryDelay[0][(size_t) p]; if (numCh > 1) dr[1][i] = dryDelay[1][(size_t) p];
                dryDelay[0][(size_t) p] = xl; dryDelay[1][(size_t) p] = xr;
            }
            else { dr[0][i] = xl; if (numCh > 1) dr[1][i] = xr; }
        }
        if (osActive) ddPos = (ddPos + n) % D;

        const float* sl = scp[0] != nullptr ? scp[0] + pos : nullptr;
        const float* sr2 = scp[1] != nullptr ? scp[1] + pos : sl;
        if (! osActive) runRack (rack1, L, R, n, sl, sr2);
        else
        {
            float* chans[2] { L, R };
            juce::dsp::AudioBlock<float> blk (chans, 2, (size_t) n);
            auto up = os->processSamplesUp (blk);
            const float* uscL = nullptr; const float* uscR = nullptr;
            if (sl != nullptr)
            {
                std::copy (sl, sl + n, scBuf[0].begin()); std::copy (sr2, sr2 + n, scBuf[1].begin());
                float* sc2[2] { scBuf[0].data(), scBuf[1].data() };
                juce::dsp::AudioBlock<float> sb (sc2, 2, (size_t) n);
                auto su = osSc->processSamplesUp (sb);
                uscL = su.getChannelPointer (0); uscR = su.getChannelPointer (1);
            }
            runRack (rack2, up.getChannelPointer (0), up.getChannelPointer (1), (int) up.getNumSamples(), uscL, uscR);
            os->processSamplesDown (blk);
        }
        for (int i = 0; i < n; ++i)
        {
            mixSm += (mixT - mixSm) * 0.002f;
            outSm += (outT - outSm) * 0.002f;
            L[i] = (dr[0][i] + (L[i] - dr[0][i]) * mixSm) * outSm;
            if (numCh > 1) R[i] = (dr[1][i] + (R[i] - dr[1][i]) * mixSm) * outSm;
            pkOut = std::max (pkOut, std::abs (L[i]));
        }
    }
    for (int c = numCh; c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, total);
    inPeak.store (pkIn); outPeak.store (pkOut);
}

//==============================================================================
void TopRackProcessor::setP (const juce::String& id, float v)
{
    if (auto* p = apvts.getParameter (id)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (v)); p->endChangeGesture(); }
}
int TopRackProcessor::slotType (int s) const { return (int) typeP[(size_t) s]->load(); }
float TopRackProcessor::knobNorm (int s, int k) const { return knobP[(size_t) s][(size_t) k]->load(); }
void TopRackProcessor::setKnobReal (int s, int k, float real)
{
    const auto& d = defs()[(size_t) slotType (s)];
    if (k < (int) d.knobs.size()) setP (sid (s, "p" + juce::String (k)), d.knobs[(size_t) k].toNorm (real));
}
void TopRackProcessor::setSlotType (int s, int type)
{
    for (auto it = assigns.begin(); it != assigns.end();) { if (it->slot == s) it = assigns.erase (it); else ++it; }
    setP (sid (s, "type"), (float) type);
    setP (sid (s, "byp"), 0.0f);
    setP (sid (s, "trim"), 0.0f);
    setP (sid (s, "sc"), 0.0f);
    const auto& d = defs()[(size_t) type];
    for (int k = 0; k < maxKnobs; ++k)
        setP (sid (s, "p" + juce::String (k)), k < (int) d.knobs.size() ? d.knobs[(size_t) k].toNorm (d.knobs[(size_t) k].def) : 0.5f);
}
void TopRackProcessor::moveSlot (int s, int delta)
{
    const int o = s + delta;
    if (o < 0 || o >= numSlots) return;
    juce::StringArray what { "type", "byp", "trim", "sc" };
    for (int k = 0; k < maxKnobs; ++k) what.add ("p" + juce::String (k));
    std::vector<float> a, b;
    for (const auto& w : what) { a.push_back (apvts.getRawParameterValue (sid (s, w))->load()); b.push_back (apvts.getRawParameterValue (sid (o, w))->load()); }
    for (int i = 0; i < what.size(); ++i) { setP (sid (s, what[i]), b[(size_t) i]); setP (sid (o, what[i]), a[(size_t) i]); }
    for (auto& as : assigns) { if (as.slot == s) as.slot = o; else if (as.slot == o) as.slot = s; }
}
float TopRackProcessor::slotGr (int s) const
{
    const int t = slotType (s);
    if (t <= 0) return 0.0f;
    const auto& m = (osActive ? rack2 : rack1).slots[(size_t) s].mods[(size_t) t];
    return m != nullptr ? m->gr.load() : 0.0f;
}

void TopRackProcessor::assignMacro (int macro, int slot, int knob)
{
    unassign (slot, knob);
    const float cur = knobNorm (slot, knob);
    Assign a; a.macro = macro; a.slot = slot; a.knob = knob;
    a.lo = cur; a.hi = cur < 0.5f ? juce::jmin (1.0f, cur + 0.5f) : juce::jmax (0.0f, cur - 0.5f);
    assigns.push_back (a);
    lastMacro[(size_t) macro] = -1.0f;
}
void TopRackProcessor::unassign (int slot, int knob)
{
    for (auto it = assigns.begin(); it != assigns.end();) { if (it->slot == slot && it->knob == knob) it = assigns.erase (it); else ++it; }
}
int TopRackProcessor::macroOf (int slot, int knob) const
{
    for (const auto& a : assigns) if (a.slot == slot && a.knob == knob) return a.macro;
    return -1;
}
// las macros mueven de verdad las perillas asignadas (hilo de mensajes)
void TopRackProcessor::timerCallback() { applyMacros(); }
void TopRackProcessor::applyMacros()
{
    for (int m = 0; m < 4; ++m)
    {
        const float v = macroP[(size_t) m]->load();
        if (std::abs (v - lastMacro[(size_t) m]) < 1e-4f) continue;
        lastMacro[(size_t) m] = v;
        for (const auto& a : assigns)
            if (a.macro == m)
                if (auto* p = apvts.getParameter (sid (a.slot, "p" + juce::String (a.knob))))
                    p->setValueNotifyingHost (a.lo + (a.hi - a.lo) * v);
    }
}

//==============================================================================
juce::ValueTree TopRackProcessor::snapshot() const
{
    juce::ValueTree t ("Rack");
    for (int s = 0; s < numSlots; ++s)
    {
        t.setProperty (sid (s, "type"), slotType (s), nullptr);
        for (const char* w : { "byp", "trim", "sc" }) t.setProperty (sid (s, w), apvts.getRawParameterValue (sid (s, w))->load(), nullptr);
        for (int k = 0; k < maxKnobs; ++k) t.setProperty (sid (s, "p" + juce::String (k)), knobNorm (s, k), nullptr);
    }
    return t;
}
void TopRackProcessor::recall (const juce::ValueTree& t)
{
    assigns.clear();
    for (int i = 0; i < t.getNumProperties(); ++i)
    {
        const auto id = t.getPropertyName (i).toString();
        setP (id, (float) t.getProperty (id));
    }
}

int TopRackProcessor::getNumPrograms() { return (int) chains().size(); }
const juce::String TopRackProcessor::getProgramName (int i) { return juce::isPositiveAndBelow (i, (int) chains().size()) ? juce::String::fromUTF8 (chains()[(size_t) i].name) : juce::String(); }
void TopRackProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) chains().size())) return;
    program = index;
    const auto& c = chains()[(size_t) index];
    for (int s = 0; s < numSlots; ++s)
    {
        if (s < (int) c.slots.size())
        {
            setSlotType (s, c.slots[(size_t) s].type);
            for (const auto& [k, v] : c.slots[(size_t) s].knobs) setKnobReal (s, k, v);
        }
        else setSlotType (s, Empty);
    }
}

void TopRackProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", program, nullptr);
    for (int i = state.getNumChildren(); --i >= 0;) if (state.getChild (i).hasType ("Assigns") || state.getChild (i).hasType ("Dream")) state.removeChild (i, nullptr);
    juce::ValueTree as ("Assigns");
    for (const auto& a : assigns)
    {
        juce::ValueTree v ("A");
        v.setProperty ("m", a.macro, nullptr); v.setProperty ("s", a.slot, nullptr); v.setProperty ("k", a.knob, nullptr);
        v.setProperty ("lo", a.lo, nullptr); v.setProperty ("hi", a.hi, nullptr);
        as.appendChild (v, nullptr);
    }
    state.appendChild (as, nullptr);
    juce::ValueTree dr ("Dream");
    for (int i = 0; i < 8; ++i)
    {
        juce::ValueTree d ("D"); d.setProperty ("i", i, nullptr);
        if (dream[(size_t) i].isValid()) d.appendChild (dream[(size_t) i].createCopy(), nullptr);
        dr.appendChild (d, nullptr);
    }
    state.appendChild (dr, nullptr);
    juce::String names; for (auto& n : macroNames) names << n << "|";
    state.setProperty ("macroNames", names, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}
void TopRackProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto t = juce::ValueTree::fromXml (*xml);
    program = t.getProperty ("program", 0);
    assigns.clear();
    const auto as = t.getChildWithName ("Assigns");
    for (int i = 0; i < as.getNumChildren(); ++i)
    {
        const auto v = as.getChild (i);
        Assign a; a.macro = v["m"]; a.slot = v["s"]; a.knob = v["k"]; a.lo = (float) v["lo"]; a.hi = (float) v["hi"];
        assigns.push_back (a);
    }
    const auto dr = t.getChildWithName ("Dream");
    for (int i = 0; i < dr.getNumChildren() && i < 8; ++i)
        dream[(size_t) i] = dr.getChild (i).getNumChildren() > 0 ? dr.getChild (i).getChild (0).createCopy() : juce::ValueTree();
    juce::StringArray names; names.addTokens (t.getProperty ("macroNames").toString(), "|", ""); names.removeEmptyStrings();
    if (names.size() == 4) macroNames = names;
    t.removeChild (as, nullptr); t.removeChild (dr, nullptr);
    apvts.replaceState (t);
    lastMacro = { 2, 2, 2, 2 };
    for (int m = 0; m < 4; ++m) lastMacro[(size_t) m] = macroP[(size_t) m]->load();   // no pisar perillas al cargar
}

juce::AudioProcessorEditor* TopRackProcessor::createEditor() { return new TopRackEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TopRackProcessor(); }
