#include "TopCompProcessor.h"
#include "TopCompEditor.h"

using tvc::dbToGain;
using tvc::gainToDb;

const juce::StringArray& TopCompProcessor::selectNames() { static const juce::StringArray s { "Fixed", "Fix/Man", "Manual" }; return s; }
const juce::StringArray& TopCompProcessor::hpfNames()    { static const juce::StringArray s { "Off", "60 Hz", "120 Hz", "180 Hz", "250 Hz", "500 Hz" }; return s; }
const juce::StringArray& TopCompProcessor::modeNames()   { static const juce::StringArray s { "LR", "M", "S", "Delta" }; return s; }

namespace
{
    struct Preset { const char* name; std::vector<std::pair<const char*, float>> v; };
    const std::vector<Preset>& compPresets()
    {
        static const std::vector<Preset> p {
            { "Default", {} },
            { "Voz Suave (Fixed)", { { "thr", -18 }, { "ratio", 3 }, { "select", 0 }, { "gain", 4 }, { "hpf", 2 } } },
            { "Voz al Frente", { { "thr", -24 }, { "ratio", 4 }, { "select", 2 }, { "attack", 25 }, { "release", 40 }, { "gain", 7 }, { "hpf", 2 } } },
            { "Rap Agresivo", { { "thr", -28 }, { "ratio", 8 }, { "select", 2 }, { "attack", 10 }, { "release", 25 }, { "gain", 9 }, { "hpf", 3 } } },
            { "Bus de Bateria", { { "thr", -20 }, { "ratio", 4 }, { "select", 2 }, { "attack", 55 }, { "release", 30 }, { "gain", 5 }, { "hpf", 1 }, { "mix", 70 } } },
            { "Compresion Paralela", { { "thr", -32 }, { "ratio", 10 }, { "select", 2 }, { "attack", 5 }, { "release", 35 }, { "gain", 10 }, { "mix", 40 } } },
            { "Bajo Gordo", { { "thr", -20 }, { "ratio", 4 }, { "select", 1 }, { "release", 55 }, { "gain", 6 } } },
            { "Pegamento Mix (M/S)", { { "thr", -14 }, { "ratio", 2 }, { "select", 0 }, { "gain", 2 }, { "hpf", 3 }, { "mode", 1 } } },
        };
        return p;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout TopCompProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;
    auto addF = [&ps] (const char* id, const char* name, NormalisableRange<float> r, float def, String unit, int dec)
    {
        auto toText = [unit, dec] (float v, int) { return String (v, dec) + (unit.isNotEmpty() ? " " + unit : String()); };
        auto fromText = [] (const String& t) { return t.retainCharacters ("-0123456789.").getFloatValue(); };
        ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def,
                          AudioParameterFloatAttributes().withLabel (unit).withStringFromValueFunction (toText).withValueFromStringFunction (fromText)));
    };
    auto addB = [&ps] (const char* id, const char* name, bool def) { ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };
    auto addC = [&ps] (const char* id, const char* name, const StringArray& c, int def) { ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, c, def)); };

    addB ("power", "In", true);
    addF ("gain", "Gain", { -20.0f, 30.0f, 0.1f }, 0.0f, "dB", 1);
    NormalisableRange<float> ratio (2.0f, 10.0f, 0.01f); ratio.setSkewForCentre (4.0f);
    addF ("ratio", "Ratio", ratio, 4.0f, ":1", 1);
    addF ("thr", "Threshold", { -40.0f, 0.0f, 0.1f }, -15.0f, "dB", 1);
    addF ("mix", "Dry/Wet", { 0.0f, 100.0f, 0.1f }, 100.0f, "%", 0);
    addF ("attack", "Attack", { 0.0f, 100.0f, 0.1f }, 30.0f, "%", 0);
    addF ("release", "Release", { 0.0f, 100.0f, 0.1f }, 40.0f, "%", 0);
    addC ("select", "Attack/Release Select", selectNames(), 0);
    addC ("hpf", "Sidechain HPF", hpfNames(), 0);
    addC ("mode", "Modo (LR / M / S / Delta)", modeNames(), 0);
    addF ("color", "Color (armonicos)", { 0.0f, 200.0f, 0.1f }, 100.0f, "%", 0);
    addC ("meter", "VU (Input / Compression / Output)", { "Input", "Compression", "Output" }, 1);
    return { ps.begin(), ps.end() };
}

TopCompProcessor::TopCompProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TopBlueComp", createLayout())
{
}

bool TopCompProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet(), in = l.getMainInputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) && in == out;
}

void TopCompProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate; osr = sr * 2.0;
    maxBlock = std::max (32, samplesPerBlock);
    oversampling.reset();
    oversampling.initProcessing ((size_t) maxBlock);
    dryLatency = (int) std::round (oversampling.getLatencyInSamples());
    setLatencySamples (dryLatency);
    for (auto& r : dryRing) r.assign ((size_t) dryLatency + 1, 0.0f);
    dryPos = 0;
    dryBuf.setSize (2, maxBlock);
    env = gr = grSlow = charge = 0.0f;
    lastHpf = -1;
    for (int c = 0; c < 2; ++c)
    {
        scHp[c].reset();
        outLow[c].setLowPass (osr, 140.0, 0.6); outLow[c].reset();
        inLow[c].setLowPass (osr, 90.0, 0.6); inLow[c].reset();
        dcX[c] = dcY[c] = hfState[c] = 0.0f;
    }
    inPeak = outPeak = 0.0f;
    gainSm.reset (osr, 0.03); gainSm.setCurrentAndTargetValue (dbToGain (p ("gain")));
    mixSm.reset (sr, 0.03);   mixSm.setCurrentAndTargetValue (p ("mix") * 0.01f);
    powerSm.reset (sr, 0.02); powerSm.setCurrentAndTargetValue (on ("power") ? 1.0f : 0.0f);
}

void TopCompProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int chans = std::min (2, buffer.getNumChannels());
    for (int c = chans; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);
    if (n == 0 || chans == 0) return;

    dryBuf.setSize (2, n, false, false, true);
    for (int c = 0; c < chans; ++c) dryBuf.copyFrom (c, 0, buffer, c, 0, n);
    if (chans == 1) dryBuf.copyFrom (1, 0, buffer, 0, 0, n);

    float pk = 0.0f;
    for (int c = 0; c < chans; ++c) pk = std::max (pk, buffer.getMagnitude (c, 0, n));
    inPeak = std::max (pk, inPeak * 0.9f);
    inLevelDb.store (gainToDb (inPeak));

    const int hpf = (int) p ("hpf");
    if (hpf != lastHpf)
    {
        lastHpf = hpf;
        static const float f[] { 0, 60, 120, 180, 250, 500 };
        for (auto& h : scHp) { if (hpf == 0) h.setIdentity(); else h.setHighPass (osr, f[hpf], 0.707); }
    }

    // ---- tiempos segun ATTACK/RELEASE SELECT
    const int sel = (int) p ("select");
    const float a01 = p ("attack") * 0.01f, r01 = p ("release") * 0.01f;
    const float attMs = sel == 2 ? 0.1f * std::pow (500.0f, a01) : 6.0f;              // manual: 0.1 .. 50 ms
    const float relMs = sel == 0 ? 120.0f : 50.0f * std::pow (40.0f, r01);           // manual: 50 ms .. 2 s
    const float aC = std::exp (-1.0f / (attMs * 0.001f * (float) osr));
    const float rC = std::exp (-1.0f / (relMs * 0.001f * (float) osr));
    const float rSlow = std::exp (-1.0f / (1.6f * (float) osr));                       // segunda etapa (Fixed)
    const float detC = std::exp (-1.0f / (0.002f * (float) osr));
    const float chargeC = std::exp (-1.0f / (2.0f * (float) osr));
    const float thr = p ("thr"), ratio = p ("ratio"), knee = 6.0f;
    const float color = p ("color") * 0.01f;
    const int mode = (int) p ("mode");
    gainSm.setTargetValue (dbToGain (p ("gain")));
    float maxGr = 0.0f;

    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) chans, (size_t) n);
    auto up = oversampling.processSamplesUp (block);
    float* L = up.getChannelPointer (0);
    float* R = chans > 1 ? up.getChannelPointer (1) : nullptr;
    const int un = (int) up.getNumSamples();

    // dc blocker a 8 Hz (los armonicos pares generan algo de DC)
    const float dcK = std::exp (-2.0f * juce::MathConstants<float>::pi * 8.0f / (float) osr);
    for (int i = 0; i < un; ++i)
    {
        float l = L[i], r = R ? R[i] : l;
        // ---- TRANSFORMADOR DE ENTRADA: un poco de saturacion en graves fuertes (3er armonico)
        for (int c = 0; c < 2; ++c)
        {
            float& x = c == 0 ? l : r;
            const float lo = inLow[c].process (x);
            x += color * 0.06f * (std::tanh (lo * 2.5f) / 2.5f - lo);
        }
        // ---- M/S
        float a = l, b = r;
        if (mode == 1 || mode == 2) { a = 0.5f * (l + r); b = 0.5f * (l - r); }
        // ---- DETECTOR (enlazado), con HPF del sidechain
        float det;
        if (mode == 1)      det = std::abs (scHp[0].process (a));
        else if (mode == 2) det = std::abs (scHp[0].process (b));
        else                det = std::max (std::abs (scHp[0].process (l)), std::abs (scHp[1].process (r)));
        env = det + detC * (env - det) * (det < env ? 1.0f : 0.0f);
        if (det > env) env = det;
        const float lvl = gainToDb (env + 1e-9f), over = lvl - thr;
        float target = 0.0f;
        if (over > knee * 0.5f) target = over * (1.0f - 1.0f / ratio);
        else if (over > -knee * 0.5f) { const float k = over + knee * 0.5f; target = (1.0f - 1.0f / ratio) * k * k / (2.0f * knee); }
        if (target > gr) gr = target + aC * (gr - target);
        else if (sel == 0)
        {
            // FIXED: release de dos etapas que depende del programa (rapido al principio, lento despues de comprimir mucho)
            charge = target + chargeC * (charge - target);
            const float fast = target + rC * (gr - target);
            grSlow = target + rSlow * (grSlow - target);
            gr = std::max (fast, grSlow * juce::jlimit (0.0f, 0.8f, charge / 10.0f));
        }
        else gr = target + rC * (gr - target);
        if (sel != 0) grSlow = gr;
        maxGr = std::max (maxGr, gr);
        const float g = dbToGain (-gr);
        // ---- CELULA DE GANANCIA: al comprimir aparecen armonicos pares (2º), proporcionales a la reduccion
        const float h2 = color * 0.12f * juce::jlimit (0.0f, 1.0f, gr / 24.0f);
        auto cell = [g, h2] (float x) { const float y = x * g; return y + h2 * y * x * 1.5f; };   // y^2 = 2o armonico (par); el DC se quita despues
        if (mode == 1)      { a = cell (a); l = a + b; r = a - b; }
        else if (mode == 2) { b = cell (b); l = a + b; r = a - b; }
        else                { l = cell (l); r = cell (r); }
        // ---- GANANCIA + TRANSFORMADOR DE SALIDA: graves que se saturan suave + ligera caida de agudos
        const float og = gainSm.getNextValue();
        for (int c = 0; c < 2; ++c)
        {
            float& x = c == 0 ? l : r;
            x *= og;
            const float lo = outLow[c].process (x);
            x += color * 0.18f * (std::tanh (lo * 1.8f) / 1.8f - lo);
            x = 0.9f * x + 0.1f * std::tanh (x);                         // limite suave del amplificador
            hfState[c] += 0.82f * (x - hfState[c]);                       // caida muy suave por encima de 30 kHz
            x = hfState[c] * 0.3f + x * 0.7f;
            const float yv = x - dcX[c] + dcK * dcY[c];
            dcX[c] = x; dcY[c] = yv; x = yv;
        }
        L[i] = l; if (R) R[i] = r;
    }
    oversampling.processSamplesDown (block);
    grDb.store (-maxGr);

    // ---- DRY/WET, DELTA y IN (dry alineado con la latencia)
    mixSm.setTargetValue (p ("mix") * 0.01f);
    powerSm.setTargetValue (on ("power") ? 1.0f : 0.0f);
    const int len = dryLatency + 1;
    for (int i = 0; i < n; ++i)
    {
        const float mx = mixSm.getNextValue(), pw = powerSm.getNextValue();
        const int rd = (dryPos + 1) % len;
        for (int c = 0; c < chans; ++c)
        {
            auto& ring = dryRing[c];
            ring[(size_t) dryPos] = dryBuf.getSample (c, i);
            const float dry = ring[(size_t) rd];
            float wet = buffer.getSample (c, i);
            wet = dry + (wet - dry) * mx;
            if (mode == 3) wet = dry - wet;                              // DELTA: escuchas lo que el compresor cambia
            buffer.setSample (c, i, dry + (wet - dry) * pw);
        }
        dryPos = rd;
    }
    if (chans == 1 && buffer.getNumChannels() > 1) buffer.copyFrom (1, 0, buffer, 0, 0, n);
    pk = 0.0f;
    for (int c = 0; c < chans; ++c) pk = std::max (pk, buffer.getMagnitude (c, 0, n));
    outPeak = std::max (pk, outPeak * 0.9f);
    outLevelDb.store (gainToDb (outPeak));
}

int TopCompProcessor::getNumPrograms() { return (int) compPresets().size(); }
const juce::String TopCompProcessor::getProgramName (int i)
{ return juce::isPositiveAndBelow (i, (int) compPresets().size()) ? juce::String (compPresets()[(size_t) i].name) : juce::String(); }
void TopCompProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) compPresets().size())) return;
    currentProgram = index;
    for (auto* prm : getParameters()) if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm)) rp->setValueNotifyingHost (rp->getDefaultValue());
    for (const auto& [id, val] : compPresets()[(size_t) index].v) if (auto* rp = apvts.getParameter (id)) rp->setValueNotifyingHost (rp->convertTo0to1 (val));
}
void TopCompProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}
void TopCompProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = (int) tree.getProperty ("program", 0);
            apvts.replaceState (tree);
        }
}
juce::AudioProcessorEditor* TopCompProcessor::createEditor() { return new TopCompEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TopCompProcessor(); }
