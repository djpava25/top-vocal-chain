#include "VbxProcessor.h"
#include "VbxEditor.h"

namespace
{
    struct Preset { const char* name; std::vector<std::pair<const char*, float>> v; };
    // indices de frecuencia: LOW 20,35,50,70,100,150,200,300,500,700,1K | MID 200,300,500,700,1K,1K5,2K,3K,4K,5K,7K | HI 1K5,2K,3K,4K,5K,6K4,8K,10K,12K,16K,20K
    const std::vector<Preset>& presets()
    {
        static const std::vector<Preset> p {
            { "Voz limpia", { { "comp", 1 }, { "thr", 4 }, { "deess", 1 }, { "dsThr", 4 } } },
            // ajustados a las mediciones por impulso (nombre de cada IR)
            { "Male Vocal (medido)", { { "lowF", 1 }, { "low", 2.5f }, { "midF", 8 }, { "mid", 10 }, { "hiF", 10 }, { "hi", 2.6f }, { "comp", 1 }, { "thr", 4 } } },
            { "Female Vox (medido)", { { "lowF", 0 }, { "low", 1.9f }, { "midF", 1 }, { "mid", 0.6f }, { "hiF", 10 }, { "hi", 6 }, { "comp", 1 }, { "thr", 4 }, { "deess", 1 }, { "dsF", 1 } } },
            { "Sexy Female Voice (medido)", { { "lowF", 2 }, { "low", 2.9f }, { "hiF", 10 }, { "hi", 7.6f }, { "comp", 1 }, { "thr", 5 }, { "deess", 1 }, { "dsF", 1 } } },
            { "Acoustic Guitar (medido)", { { "hpf", 1 }, { "lowF", 1 }, { "low", 4.5f }, { "hiF", 7 }, { "hi", 6.5f } } },
            { "Acoustic Guitar 2 (medido)", { { "hpf", 1 }, { "hiF", 7 }, { "hi", 3.8f } } },
            { "Electric Guitar (medido)", { { "lowF", 2 }, { "low", 7 }, { "midF", 2 }, { "mid", 1.1f }, { "hiF", 2 }, { "hi", 1.9f } } },
            { "Bass (medido)", { { "lowF", 2 }, { "low", 1.8f }, { "hiF", 8 }, { "hi", 1.4f }, { "comp", 1 }, { "thr", 5 }, { "attack", 2 } } },
            { "Kick (medido)", { { "lowF", 3 }, { "low", 7.1f }, { "midF", 10 }, { "mid", 10 } } },
            { "Snare (medido)", { { "lowF", 4 }, { "low", 10 }, { "midF", 3 }, { "mid", 3.4f }, { "hiF", 6 }, { "hi", 10 } } },
            { "Drums (medido)", { { "lowF", 4 }, { "low", 3 }, { "midF", 3 }, { "mid", 0.6f }, { "hiF", 6 }, { "hi", 0.7f } } },
            { "Drum Bus (medido)", { { "lowF", 5 }, { "low", 1 }, { "hiF", 6 }, { "hi", 1 }, { "comp", 1 }, { "thr", 3 }, { "attack", 3 }, { "release", 1 } } },
            { "Piano (medido)", { { "lowF", 4 }, { "low", 4.6f }, { "midF", 3 }, { "mid", 2.5f }, { "hiF", 2 }, { "hi", 1.2f } } },
            { "Strings (medido)", { { "lowF", 5 }, { "low", 1.1f }, { "hiF", 6 }, { "hi", 0.9f } } },
            { "Rap al frente", { { "hpf", 1 }, { "gain", 3 }, { "comp", 1 }, { "thr", 6 }, { "attack", 1 }, { "release", 0 }, { "midF", 2 }, { "mid", 2 }, { "hiF", 6 }, { "hi", 4 }, { "deess", 1 }, { "dsF", 1 }, { "dsThr", 5 } } },
            { "Podcast / locucion", { { "hpf", 2 }, { "comp", 1 }, { "thr", 5 }, { "lowF", 3 }, { "low", 2 }, { "midF", 2 }, { "mid", 2 }, { "deess", 1 }, { "dsF", 0 } } },
            { "Limitador de salida", { { "deess", 1 }, { "dsF", 4 }, { "dsThr", 5 } } } };
        return p;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout VbxProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;
    auto F = [&ps] (const char* id, const char* name, float lo, float hi, float d, float st, const char* unit)
    { ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, st), d, AudioParameterFloatAttributes().withLabel (unit))); };
    auto C = [&ps] (const char* id, const char* name, StringArray items, int d) { ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, d)); };
    auto B = [&ps] (const char* id, const char* name, bool d) { ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, d)); };
    C ("hpf", "Low Filter", { "FLAT", "80", "120" }, 0);
    B ("phase", "Phase 180", false);
    F ("input", "Input", -20, 20, 0, 0.1f, "dB");
    C ("gain", "Gain", { "40", "45", "50", "55", "60" }, 2);
    C ("meter", "Meter", { "G-R", "LINE IN", "PRE OUT", "EQ OUT", "D-S" }, 0);
    B ("link", "Link", true);
    B ("comp", "Compress 3:1", true);
    F ("thr", "Threshold", 0, 10, 3, 0.01f, "");
    C ("attack", "Attack", { "FAST", "MED FAST", "MEDIUM", "MED SLOW", "SLOW" }, 1);
    C ("release", "Release", { "FAST", "MED FAST", "MEDIUM", "MED SLOW", "SLOW" }, 2);
    B ("deess", "De-Ess", false);
    C ("dsF", "De-Ess Freq", { "3K", "6K", "9K", "12K", "LIMIT 10:1" }, 1);
    F ("dsThr", "De-Ess Threshold", 0, 10, 3, 0.01f, "");
    B ("eq", "EQ In", true);
    B ("eqLine", "EQ Input Line", false);                 // EQ INPUT: PRE O/P (falso) o LINE IN (sin preamp ni compresor)
    B ("xfmr", "Xfmr In", true);
    F ("low", "Low Peak", 0, 10, 0, 0.05f, "dB");
    C ("lowF", "Low Freq", { "20", "35", "50", "70", "100", "150", "200", "300", "500", "700", "1000" }, 4);
    F ("mid", "Mid Dip", 0, 10, 0, 0.05f, "dB");
    C ("midF", "Mid Freq", { "200", "300", "500", "700", "1000", "1K5", "2K", "3K", "4K", "5K", "7K" }, 4);
    F ("hi", "Hi Peak", 0, 10, 0, 0.05f, "dB");
    C ("hiF", "Hi Freq", { "1K5", "2K", "3K", "4K", "5K", "6K4", "8K", "10K", "12K", "16K", "20K" }, 7);
    F ("out", "Output", -20, 10, 0, 0.1f, "dB");
    B ("power", "Power", true);
    return { ps.begin(), ps.end() };
}

VbxProcessor::VbxProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true).withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "P6961", createLayout())
{
    using OS = juce::dsp::Oversampling<float>;
    os.clearOversamplingStages();
    os.addOversamplingStage (OS::filterHalfBandFIREquiripple, 0.06f, -110.0f, 0.06f, -110.0f);
    os.addOversamplingStage (OS::filterHalfBandFIREquiripple, 0.20f, -100.0f, 0.20f, -100.0f);
    os.addOversamplingStage (OS::filterHalfBandFIREquiripple, 0.30f, -100.0f, 0.30f, -100.0f);
    os.setUsingIntegerLatency (true);
    vbx::preTable (0); vbx::lineTable();                                  // construye las tablas fuera del hilo de audio
}

bool VbxProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    return in == out && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

vbx::EqSettings VbxProcessor::eqSettings() const
{
    auto f = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    vbx::EqSettings s;
    s.on = f ("eq") > 0.5f; s.hpf = (int) f ("hpf");
    s.low = f ("low"); s.mid = f ("mid"); s.hi = f ("hi");
    s.lowF = (int) f ("lowF"); s.midF = (int) f ("midF"); s.hiF = (int) f ("hiF");
    return s;
}

void VbxProcessor::prepareToPlay (double sr, int block)
{
    os.reset(); os.initProcessing ((size_t) std::max (1, block));
    latency = (int) std::round (os.getLatencyInSamples());
    setLatencySamples (latency);
    fs8 = sr * (double) os.getOversamplingFactor();
    for (auto& c : ch)
    {
        c.eq.reset(); c.pre.prepare (fs8); c.line.prepare (fs8); c.dsSc.reset(); c.adaa.reset();
        c.notch.prepare (fs8, 6000.0, 1.0);
    }
    for (auto& o : comp) o.prepare (fs8);
    for (auto& o : ds) o.prepare (fs8);
    designed = false; lastDsF = -1; lastGain = -1;
    dry.setSize (2, std::max (1, block));
    for (auto& d : delay) d.assign ((size_t) std::max (1, latency), 0.0f);
    delayPos = 0;
    outSm.reset (sr, 0.05); onSm.reset (sr, 0.03); inSm.reset (fs8, 0.05); driveSm.reset (fs8, 0.03);
    outSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("out")->load()));
    inSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain ((double) apvts.getRawParameterValue ("input")->load()));
    driveSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (5.0 * (double) ((int) apvts.getRawParameterValue ("gain")->load() - 2)));
    onSm.setCurrentAndTargetValue (apvts.getRawParameterValue ("power")->load() > 0.5f ? 1.0f : 0.0f);
}

void VbxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples(), chs = std::min (2, getTotalNumInputChannels());
    if (n == 0 || chs == 0) return;
    if (n > dry.getNumSamples()) dry.setSize (2, n, false, false, true);
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

    auto P = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    const auto es = eqSettings();
    if (! designed || ! (es == lastEq)) { for (auto& c : ch) c.eq.design (es, fs8); lastEq = es; designed = true; }
    const int dsF = juce::jlimit (0, 4, (int) P ("dsF"));
    if (dsF != lastDsF)
    {
        if (dsF < 4)
            for (auto& c : ch)
            {
                c.notch.prepare (fs8, vbx::dsFreqs()[dsF], 1.0);
                c.dsSc.setAnalog (0, 2 * vbx::pi * vbx::dsFreqs()[dsF] / 1.4, 0, 1, 2 * vbx::pi * vbx::dsFreqs()[dsF] / 1.4,
                                  std::pow (2 * vbx::pi * vbx::dsFreqs()[dsF], 2.0), fs8, vbx::dsFreqs()[dsF]);
                c.dsSc.reset();
            }
        lastDsF = dsF;
    }
    const bool lineToEq = P ("eqLine") > 0.5f;
    const bool compOn = P ("comp") > 0.5f && ! lineToEq, dsOn = P ("deess") > 0.5f, link = P ("link") > 0.5f || chs == 1, xfmr = P ("xfmr") > 0.5f;
    const int relIdx = juce::jlimit (0, 4, (int) P ("release"));
    for (auto& o : comp) o.set (vbx::compThrDb (P ("thr")), 3.0, vbx::attackMs()[juce::jlimit (0, 4, (int) P ("attack"))], vbx::releaseS()[relIdx], relIdx, 16.0);
    for (auto& o : ds) o.set (dsF == 4 ? vbx::limitThrDb (P ("dsThr")) : vbx::deessThrDb (P ("dsThr")), 10.0, 2.0, 0.5, 1, 32.0);
    const int gainIdx = juce::jlimit (0, 4, (int) P ("gain"));
    if (gainIdx != lastGain) { for (auto& c : ch) c.pre.set (gainIdx); lastGain = gainIdx; }
    driveSm.setTargetValue (juce::Decibels::decibelsToGain (5.0 * (double) (gainIdx - 2)));       // 50 dB = referencia, +5 dB por paso
    inSm.setTargetValue (juce::Decibels::decibelsToGain ((double) P ("input")));
    const double adaaA = (lineToEq ? 1.0 : 2.0) / 8.0;                    // etapas con ADAA en el camino

    double pkLine = 0, pkPre = 0, pkEq = 0, grC = 0, grD = 0;
    {
        juce::dsp::AudioBlock<float> blk (buffer.getArrayOfWritePointers(), (size_t) chs, (size_t) n);
        auto up = os.processSamplesUp (blk);
        const int un = (int) up.getNumSamples();
        float* X[2] = { up.getChannelPointer (0), chs > 1 ? up.getChannelPointer (1) : nullptr };
        for (int i = 0; i < un; ++i)
        {
            const double ig = inSm.getNextValue(), dr = driveSm.getNextValue();
            double v[2] = { 0, 0 };
            for (int c = 0; c < chs; ++c) { v[c] = ch[(size_t) c].eq.hpf.process ((double) X[c][i]) * ig; pkLine = std::max (pkLine, std::abs (v[c])); }
            // 1. compresor optico ELOP, antes del preamp
            if (compOn)
            {
                if (link)
                {
                    const double sc = chs > 1 ? (std::abs (v[0]) > std::abs (v[1]) ? v[0] : v[1]) : v[0];
                    const double gr = comp[0].process (sc), g = std::exp (-0.11512925464970229 * gr);
                    for (int c = 0; c < chs; ++c) v[c] *= g;
                    grC = std::max (grC, gr);
                }
                else for (int c = 0; c < chs; ++c) { const double gr = comp[(size_t) c].process (v[c]); v[c] *= std::exp (-0.11512925464970229 * gr); grC = std::max (grC, gr); }
            }
            // 2. preamp a valvula  3. EQ pasivo
            for (int c = 0; c < chs; ++c)
            {
                auto& s = ch[(size_t) c];
                if (! lineToEq) v[c] = s.pre.process (v[c], dr);
                pkPre = std::max (pkPre, std::abs (v[c]));
                v[c] = s.eq.bands (v[c]);
                pkEq = std::max (pkEq, std::abs (v[c]));
            }
            // 4. de-esser LC / limitador 10:1 (despues del EQ, antes de la etapa final)
            if (dsOn)
            {
                if (dsF == 4)
                {
                    const double sc = chs > 1 ? std::max (std::abs (v[0]), std::abs (v[1])) : std::abs (v[0]);
                    const double gr = ds[0].process (sc), g = std::exp (-0.11512925464970229 * gr);
                    for (int c = 0; c < chs; ++c) v[c] *= g;
                    grD = std::max (grD, gr);
                }
                else
                {
                    double scv[2] = { 0, 0 };
                    for (int c = 0; c < chs; ++c) scv[c] = ch[(size_t) c].dsSc.process (v[c]);
                    if (link)
                    {
                        const double sc = chs > 1 ? (std::abs (scv[0]) > std::abs (scv[1]) ? scv[0] : scv[1]) : scv[0];
                        const double gr = ds[0].process (sc), g = std::exp (-0.11512925464970229 * gr);
                        for (int c = 0; c < chs; ++c) v[c] = ch[(size_t) c].notch.process (v[c], g);
                        grD = std::max (grD, gr);
                    }
                    else for (int c = 0; c < chs; ++c)
                    {
                        const double gr = ds[(size_t) c].process (scv[c]);
                        v[c] = ch[(size_t) c].notch.process (v[c], std::exp (-0.11512925464970229 * gr)); grD = std::max (grD, gr);
                    }
                }
            }
            // 5. etapa final a valvula + XFMR, y compensacion del ADAA
            for (int c = 0; c < chs; ++c)
            {
                auto& s = ch[(size_t) c];
                v[c] = s.adaa.process (s.line.process (v[c], xfmr), adaaA);
                X[c][i] = (float) v[c];
            }
        }
        os.processSamplesDown (blk);
    }
    mLine.store (std::max (mLine.load(), (float) pkLine)); mPre.store (std::max (mPre.load(), (float) pkPre)); mEq.store (std::max (mEq.load(), (float) pkEq));
    mComp.store (std::max (mComp.load(), (float) grC)); mDs.store (std::max (mDs.load(), (float) grD));

    outSm.setTargetValue (juce::Decibels::decibelsToGain (P ("out")) * (P ("phase") > 0.5f ? -1.0f : 1.0f));
    onSm.setTargetValue (P ("power") > 0.5f ? 1.0f : 0.0f);
    for (int i = 0; i < n; ++i)
    {
        const float g = outSm.getNextValue(), on = onSm.getNextValue();
        for (int c = 0; c < chs; ++c) { auto* d = buffer.getWritePointer (c); d[i] = dry.getSample (c, i) * (1.0f - on) + d[i] * g * on; }
    }
}

int VbxProcessor::getNumPrograms() { return (int) presets().size(); }
const juce::String VbxProcessor::getProgramName (int i) { return juce::isPositiveAndBelow (i, getNumPrograms()) ? presets()[(size_t) i].name : ""; }
void VbxProcessor::setCurrentProgram (int i)
{
    if (! juce::isPositiveAndBelow (i, getNumPrograms())) return;
    currentProgram = i;
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
            if (r->getParameterID() != "power" && r->getParameterID() != "meter" && r->getParameterID() != "link" && r->getParameterID() != "eqLine") r->setValueNotifyingHost (r->getDefaultValue());
    for (auto& [id, v] : presets()[(size_t) i].v)
        if (auto* r = apvts.getParameter (id)) r->setValueNotifyingHost (r->convertTo0to1 (v));
}
void VbxProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto st = apvts.copyState(); st.setProperty ("program", currentProgram, nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary (*xml, dest);
}
void VbxProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto t = juce::ValueTree::fromXml (*xml);
        if (t.isValid() && t.hasType (apvts.state.getType())) { currentProgram = (int) t.getProperty ("program", 0); apvts.replaceState (t); }
    }
}
juce::AudioProcessorEditor* VbxProcessor::createEditor() { return new VbxEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VbxProcessor(); }
