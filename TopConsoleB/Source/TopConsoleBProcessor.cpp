#include "TopConsoleBProcessor.h"
#include "TopConsoleBEditor.h"

namespace
{
    struct Preset { const char* name; std::vector<std::pair<const char*, float>> v; };
    const std::vector<Preset>& presets()
    {
        static const std::vector<Preset> p {
            { "Canal limpio", {} },
            { "Voz al frente", { { "drive", 25 }, { "hpf", 90 }, { "lmf_g", -2 }, { "lmf_f", 300 }, { "hmf_g", 3 }, { "hmf_f", 3200 }, { "hf_g", 4 }, { "hf_f", 10000 },
                                 { "c_ratio", 4 }, { "c_thr", -22 }, { "c_rel", 0.25f } } },
            { "Voz urbana", { { "drive", 40 }, { "hpf", 100 }, { "lmf_g", -3 }, { "lmf_f", 350 }, { "hmf_g", 4 }, { "hmf_f", 2800 }, { "hf_g", 5 }, { "hf_f", 12000 },
                              { "c_ratio", 5 }, { "c_thr", -26 }, { "c_rel", 0.15f }, { "c_fast", 1 }, { "fader", 2 } } },
            { "De-esser de voz", { { "c_ratio", 9 }, { "c_thr", -30 } } },
            { "Bombo", { { "drive", 30 }, { "hpf", 30 }, { "lf_g", 5 }, { "lf_f", 60 }, { "lf_bell", 1 }, { "lmf_g", -6 }, { "lmf_f", 350 }, { "lmf_q", 1.5f },
                         { "hmf_g", 4 }, { "hmf_f", 3500 }, { "c_ratio", 4 }, { "c_thr", -18 }, { "c_rel", 0.15f }, { "g_range", 30 }, { "g_thr", -35 }, { "g_rel", 0.2f } } },
            { "Caja", { { "drive", 35 }, { "hpf", 80 }, { "lmf_g", 3 }, { "lmf_f", 220 }, { "hmf_g", -3 }, { "hmf_f", 900 }, { "hf_g", 4 }, { "hf_f", 8000 },
                        { "c_ratio", 4 }, { "c_thr", -20 }, { "c_fast", 1 }, { "c_rel", 0.1f }, { "g_range", 20 }, { "g_thr", -30 }, { "g_fast", 1 } } },
            { "Bajo", { { "drive", 45 }, { "hpf", 30 }, { "lf_g", 3 }, { "lf_f", 80 }, { "lmf_g", -3 }, { "lmf_f", 250 }, { "hmf_g", 3 }, { "hmf_f", 900 },
                        { "c_ratio", 4 }, { "c_thr", -24 }, { "c_rel", 0.3f } } },
            { "Guitarra acustica", { { "hpf", 90 }, { "lmf_g", -3 }, { "lmf_f", 250 }, { "hmf_g", 2 }, { "hmf_f", 3000 }, { "hf_g", 3 }, { "hf_f", 12000 },
                                     { "c_ratio", 3 }, { "c_thr", -20 }, { "c_rel", 0.4f } } },
            { "Overheads", { { "hpf", 180 }, { "lmf_g", -2 }, { "lmf_f", 450 }, { "hf_g", 3 }, { "hf_f", 12000 }, { "c_ratio", 2 }, { "c_thr", -14 } } },
            { "Bus de bateria", { { "drive", 30 }, { "lf_g", 2 }, { "lf_f", 70 }, { "hf_g", 2 }, { "hf_f", 10000 }, { "c_ratio", 4 }, { "c_thr", -16 }, { "c_rel", 0.1f }, { "fader", 3 } } },
            { "Piano", { { "hpf", 50 }, { "lmf_g", -2 }, { "lmf_f", 400 }, { "hf_g", 2 }, { "hf_f", 9000 }, { "c_ratio", 2 }, { "c_thr", -18 }, { "c_rel", 0.5f } } },
        };
        return p;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout TopConsoleBProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;
    auto F = [&ps] (const String& id, const String& name, float lo, float hi, float def, float centre, const String& unit, int dec)
    {
        NormalisableRange<float> r (lo, hi);
        if (centre > lo && centre < hi && lo >= 0) r.setSkewForCentre (centre);
        auto toText = [unit, dec, lo, hi, id] (float v, int)
        {
            if (id == "hpf" && v <= 17.0f) return String ("OFF");
            if (id == "lpf" && v >= 21500.0f) return String ("OFF");
            if (id == "fader" && v <= -59.9f) return String::fromUTF8 ("-\xe2\x88\x9e");
            if (unit == "Hz" && v >= 1000.0f) return String (v / 1000.0f, 2) + " kHz";
            juce::ignoreUnused (lo, hi);
            return String (v, dec) + (unit.isNotEmpty() ? " " + unit : String());
        };
        auto fromText = [] (const String& t) { return t.retainCharacters ("-0123456789.").getFloatValue() * (t.containsIgnoreCase ("k") ? 1000.0f : 1.0f); };
        ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def,
                      AudioParameterFloatAttributes().withLabel (unit).withStringFromValueFunction (toText).withValueFromStringFunction (fromText)));
    };
    auto B = [&ps] (const String& id, const String& name, bool def) { ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };
    F ("in_trim", "Trim de entrada", -20, 20, 0, 0, "dB", 1);
    F ("drive", "Drive del preamp", 0, 100, 0, 40, "%", 0);
    B ("phase", "Fase", false);
    B ("hq", "HQ (oversampling)", false);
    F ("hpf", "Pasa altos", 16, 350, 16, 70, "Hz", 0);
    F ("lpf", "Pasa bajos", 3000, 22000, 22000, 9000, "Hz", 0);
    B ("filt_in", "Filtros activos", true);
    B ("filt_sc", "Filtros al sidechain", false);
    F ("lf_g", "LF ganancia", -16, 16, 0, 0, "dB", 1);   F ("lf_f", "LF frecuencia", 30, 450, 100, 100, "Hz", 0);    B ("lf_bell", "LF campana", false);
    F ("lmf_g", "LMF ganancia", -16, 16, 0, 0, "dB", 1); F ("lmf_f", "LMF frecuencia", 200, 2500, 600, 700, "Hz", 0); F ("lmf_q", "LMF Q", 0.5f, 3.0f, 1.0f, 1.2f, "", 2);
    F ("hmf_g", "HMF ganancia", -16, 16, 0, 0, "dB", 1); F ("hmf_f", "HMF frecuencia", 600, 7000, 2500, 2000, "Hz", 0); F ("hmf_q", "HMF Q", 0.5f, 3.0f, 1.0f, 1.2f, "", 2);
    F ("hf_g", "HF ganancia", -16, 16, 0, 0, "dB", 1);   F ("hf_f", "HF frecuencia", 1500, 16000, 8000, 6000, "Hz", 0); B ("hf_bell", "HF campana", false);
    B ("eq_in", "EQ activo", true);
    B ("eq_sc", "EQ al sidechain", false);
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "c_ratio", 1 }, "Ratio",
                  StringArray { "1", "1.5", "2", "3", "4", "6", "10", "20", String::fromUTF8 ("\xe2\x88\x9e"), "ds" }, 0));
    F ("c_thr", "Umbral compresor", -40, 0, -20, -20, "dB", 1);
    F ("c_rel", "Release compresor", 0.1f, 4.0f, 0.3f, 0.6f, "s", 2);
    B ("c_fast", "Ataque rapido", false);
    F ("g_range", "Rango gate", 0, 40, 0, 15, "dB", 1);
    F ("g_thr", "Umbral gate", -60, 0, -40, -30, "dB", 1);
    F ("g_rel", "Release gate", 0.1f, 4.0f, 0.25f, 0.6f, "s", 2);
    F ("g_hold", "Hold gate", 0, 4.0f, 0, 0.5f, "s", 2);
    B ("g_fast", "Gate ataque rapido", false);
    B ("g_exp", "Modo expansor", false);
    B ("dyn_in", "Dinamica activa", true);
    B ("dyn_pre", "Dinamica antes del EQ", false);
    B ("ext_sc", "Sidechain externo", false);
    F ("fader", "Fader", -60, 12, 0, -10, "dB", 1);
    F ("out_trim", "Trim de salida", -20, 20, 0, 0, "dB", 1);
    B ("cut", "Cut", false);
    B ("power", "Encendido", true);
    B ("pre_in", "Preamp activo", true);
    F ("width", "Ancho estereo", 0, 2, 1, 1, "", 2);
    F ("pan", "Paneo", -1, 1, 0, 0, "", 2);
    F ("c_mix", "Mezcla del compresor", 0, 100, 100, 50, "%", 1);
    B ("mono", "Mono", false);
    B ("listen", "Escuchar sidechain", false);
    return { ps.begin(), ps.end() };
}

TopConsoleBProcessor::TopConsoleBProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                        .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, &undoManager, "TopConsoleB", createLayout())
{
    strip1.prepare (44100.0); strip2.prepare (88200.0);
}

bool TopConsoleBProcessor::isBusesLayoutSupported (const BusesLayout& l) const
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

tcb::Params TopConsoleBProcessor::readParams() const
{
    auto f = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    auto b = [&f] (const char* id) { return f (id) > 0.5f; };
    tcb::Params p;
    p.inTrim = f ("in_trim"); p.drive = f ("drive"); p.phase = b ("phase");
    p.hpf = f ("hpf"); p.lpf = f ("lpf"); p.filtIn = b ("filt_in"); p.filtSc = b ("filt_sc");
    p.lfG = f ("lf_g"); p.lfF = f ("lf_f"); p.lfBell = b ("lf_bell");
    p.lmfG = f ("lmf_g"); p.lmfF = f ("lmf_f"); p.lmfQ = f ("lmf_q");
    p.hmfG = f ("hmf_g"); p.hmfF = f ("hmf_f"); p.hmfQ = f ("hmf_q");
    p.hfG = f ("hf_g"); p.hfF = f ("hf_f"); p.hfBell = b ("hf_bell");
    p.eqIn = b ("eq_in"); p.eqSc = b ("eq_sc");
    p.ratio = (int) f ("c_ratio"); p.thr = f ("c_thr"); p.rel = f ("c_rel"); p.fast = b ("c_fast");
    p.gRange = f ("g_range"); p.gThr = f ("g_thr"); p.gRel = f ("g_rel"); p.gHold = f ("g_hold"); p.gFast = b ("g_fast"); p.gExp = b ("g_exp");
    p.dynIn = b ("dyn_in"); p.dynPre = b ("dyn_pre"); p.extSc = b ("ext_sc");
    p.fader = f ("fader"); p.outTrim = f ("out_trim"); p.cut = b ("cut");
    p.preIn = b ("pre_in"); p.mono = b ("mono"); p.listen = b ("listen"); p.width = f ("width"); p.pan = f ("pan"); p.mix = f ("c_mix") / 100.0f;
    return p;
}

void TopConsoleBProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    maxBlock = juce::jmax (512, samplesPerBlock);
    strip1.prepare (sr); strip2.prepare (sr * 2.0);
    os = std::make_unique<juce::dsp::Oversampling<float>> (2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    osSc = std::make_unique<juce::dsp::Oversampling<float>> (2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    os->initProcessing ((size_t) maxBlock); osSc->initProcessing ((size_t) maxBlock);
    monoR.assign ((size_t) maxBlock, 0.0f); scA.assign ((size_t) maxBlock, 0.0f); scB.assign ((size_t) maxBlock, 0.0f);
    hqActive = apvts.getRawParameterValue ("hq")->load() > 0.5f;
    setLatencySamples (hqActive ? (int) std::round (os->getLatencyInSamples()) : 0);
}

void TopConsoleBProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const auto p = readParams();
    const bool wantHq = apvts.getRawParameterValue ("hq")->load() > 0.5f;
    if (wantHq != hqActive)
    {
        hqActive = wantHq;
        os->reset(); osSc->reset();
        setLatencySamples (hqActive ? (int) std::round (os->getLatencyInSamples()) : 0);
    }
    const int total = buffer.getNumSamples(), numCh = juce::jmin (2, getMainBusNumInputChannels());
    const float* scp[2] { nullptr, nullptr };
    if (getBusCount (true) > 1)
        if (auto* bus = getBus (true, 1); bus != nullptr && bus->isEnabled() && bus->getNumberOfChannels() > 0)
            for (int c = 0; c < 2; ++c)
            {
                const int idx = getChannelIndexInProcessBlockBuffer (true, 1, juce::jmin (c, bus->getNumberOfChannels() - 1));
                if (idx < buffer.getNumChannels()) scp[c] = buffer.getReadPointer (idx);
            }
    float ip = 0, op = 0, ipc[2] { 0, 0 }, opc[2] { 0, 0 };
    const bool power = apvts.getRawParameterValue ("power")->load() > 0.5f;
    double ie = 0, oe = 0;
    for (int pos = 0; pos < total; pos += maxBlock)
    {
        const int n = juce::jmin (maxBlock, total - pos);
        float* L = buffer.getWritePointer (0) + pos;
        float* R = numCh > 1 ? buffer.getWritePointer (1) + pos : monoR.data();
        if (numCh == 1) std::copy (L, L + n, monoR.begin());
        for (int i = 0; i < n; ++i)
        {
            ipc[0] = std::max (ipc[0], std::abs (L[i])); ipc[1] = std::max (ipc[1], std::abs (R[i]));
            const float a = std::max (std::abs (L[i]), std::abs (R[i])); ip = std::max (ip, a); ie += (double) a * a;
        }
        if (! power) { op = ip; opc[0] = ipc[0]; opc[1] = ipc[1]; oe = ie; continue; }
        const float* sl = scp[0] != nullptr ? scp[0] + pos : nullptr;
        const float* sr2 = scp[1] != nullptr ? scp[1] + pos : sl;
        if (! hqActive) strip1.process (L, R, n, sl, sr2, p);
        else
        {
            float* ch[2] { L, R };
            juce::dsp::AudioBlock<float> blk (ch, 2, (size_t) n);
            auto up = os->processSamplesUp (blk);
            const float* uL = nullptr; const float* uR = nullptr;
            if (sl != nullptr)
            {
                std::copy (sl, sl + n, scA.begin()); std::copy (sr2, sr2 + n, scB.begin());
                float* sc[2] { scA.data(), scB.data() };
                juce::dsp::AudioBlock<float> sb (sc, 2, (size_t) n);
                auto su = osSc->processSamplesUp (sb);
                uL = su.getChannelPointer (0); uR = su.getChannelPointer (1);
            }
            strip2.process (up.getChannelPointer (0), up.getChannelPointer (1), (int) up.getNumSamples(), uL, uR, p);
            os->processSamplesDown (blk);
        }
        for (int i = 0; i < n; ++i)
        {
            opc[0] = std::max (opc[0], std::abs (L[i])); opc[1] = std::max (opc[1], std::abs (R[i]));
            const float a = std::max (std::abs (L[i]), std::abs (R[i])); op = std::max (op, a); oe += (double) a * a;
        }
    }
    for (int c = numCh; c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, total);
    inPeak.store (ip); outPeak.store (op);
    inPeakL.store (ipc[0]); inPeakR.store (numCh > 1 ? ipc[1] : ipc[0]); outPeakL.store (opc[0]); outPeakR.store (numCh > 1 ? opc[1] : opc[0]);
    inRmsAcc += ((float) std::sqrt (ie / std::max (1, total)) - inRmsAcc) * 0.3f;
    outRmsAcc += ((float) std::sqrt (oe / std::max (1, total)) - outRmsAcc) * 0.3f;
    inRms.store (inRmsAcc); outRms.store (outRmsAcc);
}

int TopConsoleBProcessor::getNumPrograms() { return (int) presets().size(); }
const juce::String TopConsoleBProcessor::getProgramName (int i) { return juce::isPositiveAndBelow (i, (int) presets().size()) ? juce::String (presets()[(size_t) i].name) : juce::String(); }
void TopConsoleBProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presets().size())) return;
    program = index;
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
            if (rp->getParameterID() != "hq" && rp->getParameterID() != "ext_sc" && rp->getParameterID() != "power") rp->setValueNotifyingHost (rp->getDefaultValue());
    for (const auto& [id, v] : presets()[(size_t) index].v)
        if (auto* rp = apvts.getParameter (id)) rp->setValueNotifyingHost (rp->convertTo0to1 (v));
}

void TopConsoleBProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto s = apvts.copyState(); s.setProperty ("program", program, nullptr);
    if (auto xml = s.createXml()) copyXmlToBinary (*xml, dest);
}
void TopConsoleBProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) { auto t = juce::ValueTree::fromXml (*xml); program = t.getProperty ("program", 0); apvts.replaceState (t); }
}

juce::AudioProcessorEditor* TopConsoleBProcessor::createEditor() { return new TopConsoleBEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TopConsoleBProcessor(); }
