#include "TopStripProcessor.h"
#include "TopStripEditor.h"

using tvc::dbToGain;

//==============================================================================
const juce::StringArray& TopStripProcessor::bassFreqs()    { static const juce::StringArray s { "15 Hz", "30 Hz", "60 Hz", "150 Hz" }; return s; }
const juce::StringArray& TopStripProcessor::lowMidFreqs()  { static const juce::StringArray s { "35 Hz", "40 Hz", "50 Hz", "75 Hz", "100 Hz", "150 Hz", "200 Hz", "250 Hz", "300 Hz", "350 Hz", "450 Hz" }; return s; }
const juce::StringArray& TopStripProcessor::highMidFreqs() { static const juce::StringArray s { "220 Hz", "250 Hz", "350 Hz", "550 Hz", "1 kHz", "1.2 kHz", "1.4 kHz", "1.6 kHz", "2 kHz", "2.5 kHz", "2.8 kHz" }; return s; }
const juce::StringArray& TopStripProcessor::trebleFreqs()  { static const juce::StringArray s { "10 kHz", "15 kHz", "20 kHz", "32 kHz" }; return s; }

float TopStripProcessor::choiceHz (const juce::StringArray& list, int i)
{
    const auto t = list[juce::jlimit (0, list.size() - 1, i)];
    const float v = t.retainCharacters ("0123456789.").getFloatValue();
    return t.containsIgnoreCase ("k") ? v * 1000.0f : v;
}

namespace
{
    struct Preset { const char* name; std::vector<std::pair<const char*, float>> v; };
    const std::vector<Preset>& stripPresets()
    {
        static const std::vector<Preset> p {
            { "Default", {} },
            { "Voz Calida", { { "pre_gain", 4 }, { "comp_on", 1 }, { "thr", -18 }, { "ratio", 3 }, { "eq_on", 1 },
                              { "lm_gain", -3 }, { "lm_freq", 6 }, { "hm_gain", 2 }, { "hm_freq", 8 }, { "tr_gain", 3 }, { "tr_freq", 1 } } },
            { "Voz Rap Presencia", { { "pre_gain", 6 }, { "comp_on", 1 }, { "thr", -22 }, { "ratio", 5 }, { "att", 10 }, { "rel", 25 },
                                     { "eq_on", 1 }, { "hpf_on", 1 }, { "hpf", 100 }, { "lm_gain", -4 }, { "lm_freq", 8 },
                                     { "hm_gain", 4 }, { "hm_freq", 9 }, { "tr_gain", 4 }, { "tr_freq", 1 } } },
            { "Voz Aire (Pop)", { { "comp_on", 1 }, { "thr", -16 }, { "ratio", 2.5f }, { "eq_on", 1 }, { "hpf_on", 1 }, { "hpf", 90 },
                                  { "tr_gain", 6 }, { "tr_freq", 2 }, { "hm_gain", 1.5f }, { "hm_freq", 7 } } },
            { "De-Esser Suave", { { "comp_on", 1 }, { "sc_link", 1 }, { "thr", -24 }, { "ratio", 4 }, { "att", 5 }, { "rel", 20 },
                                  { "eq_on", 0 }, { "hm_gain", 8 }, { "hm_freq", 10 } } },
            { "Bass DI", { { "mic", 0 }, { "pre_gain", 8 }, { "comp_on", 1 }, { "thr", -20 }, { "ratio", 4 }, { "eq_on", 1 },
                           { "bass_gain", 4 }, { "bass_freq", 2 }, { "lm_gain", -3 }, { "lm_freq", 7 }, { "hm_gain", 3 }, { "hm_freq", 4 } } },
            { "Guitarra Acustica", { { "hpf_on", 1 }, { "hpf", 80 }, { "comp_on", 1 }, { "thr", -14 }, { "ratio", 2 }, { "eq_on", 1 },
                                     { "lm_gain", -3 }, { "lm_freq", 8 }, { "tr_gain", 4 }, { "tr_freq", 1 } } },
            { "Color de Valvula", { { "mic", 1 }, { "pre_gain", 12 }, { "out", -10 } } },
        };
        return p;
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout TopStripProcessor::createLayout()
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
    auto skewed = [] (float lo, float hi, float centre) { NormalisableRange<float> r (lo, hi); r.setSkewForCentre (centre); return r; };

    addB ("power", "Power", true);
    // Preamp
    addF ("pre_gain", "Preamp Gain", { -20.0f, 30.0f, 0.1f }, 0.0f, "dB", 1);
    addB ("mic", "Mic (mas valvula)", false);
    addB ("phase", "Phase", false);
    addB ("pad", "Pad -20", false);
    addB ("hpf_on", "High Pass", false);
    addF ("hpf", "High Pass Freq", skewed (30.0f, 140.0f, 70.0f), 60.0f, "Hz", 0);
    // Compresor optico
    addB ("comp_on", "Comp In", false);
    addF ("thr", "Threshold", { -40.0f, 0.0f, 0.1f }, -12.0f, "dB", 1);
    addF ("ratio", "Compression", skewed (1.0f, 20.0f, 4.0f), 3.0f, ":1", 1);
    addF ("att", "Attack", skewed (0.5f, 80.0f, 10.0f), 15.0f, "ms", 1);
    addF ("rel", "Release", { 0.0f, 100.0f, 1.0f }, 40.0f, "%", 0);
    addB ("eq_pre", "EQ antes del Comp", false);
    addB ("sc_link", "Sidechain High Mid (De-Ess)", false);
    addB ("meter_gr", "VU en GR", false);
    // EQ
    addB ("eq_on", "EQ In", false);
    addF ("bass_gain", "Bass", { -24.0f, 24.0f, 0.1f }, 0.0f, "dB", 1);
    addC ("bass_freq", "Bass Freq", bassFreqs(), 2);
    addF ("lm_gain", "Low Mid", { -16.0f, 16.0f, 0.1f }, 0.0f, "dB", 1);
    addC ("lm_freq", "Low Mid Freq", lowMidFreqs(), 5);
    addB ("lm_hiq", "Low Mid Hi-Q", false);
    addB ("lm_x10", "Low Mid x10", false);
    addF ("hm_gain", "High Mid", { -16.0f, 16.0f, 0.1f }, 0.0f, "dB", 1);
    addC ("hm_freq", "High Mid Freq", highMidFreqs(), 8);
    addB ("hm_hiq", "High Mid Hi-Q", false);
    addB ("hm_x10", "High Mid x10", false);
    addF ("tr_gain", "Treble", { -24.0f, 24.0f, 0.1f }, 0.0f, "dB", 1);
    addC ("tr_freq", "Treble Freq", trebleFreqs(), 1);
    // Salida
    addF ("out", "Output", { -24.0f, 12.0f, 0.1f }, 0.0f, "dB", 1);
    return { ps.begin(), ps.end() };
}

TopStripProcessor::TopStripProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TopStrip737", createLayout())
{
}

bool TopStripProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet(), in = l.getMainInputChannelSet();
    const bool okOut = out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
    return okOut && in == out;
}

void TopStripProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    maxBlock = std::max (32, samplesPerBlock);
    oversampling.reset();
    oversampling.initProcessing ((size_t) maxBlock);
    dryLatency = (int) std::round (oversampling.getLatencyInSamples());
    setLatencySamples (dryLatency);
    for (auto& r : dryRing) r.assign ((size_t) dryLatency + 1, 0.0f);
    dryPos = 0;
    dryBuf.setSize (2, maxBlock);

    for (int c = 0; c < 2; ++c)
    {
        for (auto* f : { &hpf[c], &bass[c], &lowMid[c], &highMid[c], &treble[c], &scBand[c] }) f->reset();
        dcX[c] = dcY[c] = 0.0f;
    }
    dcCoef = (float) std::exp (-2.0 * tvc::kPi * 8.0 / (sr * 2.0));
    env = optoSlow = grSm = 0.0f;
    inPeak = outPeak = 0.0f;

    inGainSm.reset (sr, 0.03);  inGainSm.setCurrentAndTargetValue (1.0f);
    outGainSm.reset (sr, 0.03); outGainSm.setCurrentAndTargetValue (dbToGain (p ("out")));
    powerSm.reset (sr, 0.02);   powerSm.setCurrentAndTargetValue (on ("power") ? 1.0f : 0.0f);
    updateFilters();
}

void TopStripProcessor::updateFilters()
{
    const double nyq = sr * 0.45;
    const float lmF = choiceHz (lowMidFreqs(), (int) p ("lm_freq")) * (on ("lm_x10") ? 10.0f : 1.0f);
    const float hmF = choiceHz (highMidFreqs(), (int) p ("hm_freq")) * (on ("hm_x10") ? 10.0f : 1.0f);
    for (int c = 0; c < 2; ++c)
    {
        if (on ("hpf_on")) hpf[c].setHighPass (sr, p ("hpf"), 0.707); else hpf[c].setIdentity();
        bass[c].setLowShelf (sr, choiceHz (bassFreqs(), (int) p ("bass_freq")), p ("bass_gain"));
        lowMid[c].setPeak (sr, std::min ((double) lmF, nyq), on ("lm_hiq") ? 2.4 : 0.7, p ("lm_gain"));
        highMid[c].setPeak (sr, std::min ((double) hmF, nyq), on ("hm_hiq") ? 2.4 : 0.7, p ("hm_gain"));
        treble[c].setHighShelf (sr, std::min ((double) choiceHz (trebleFreqs(), (int) p ("tr_freq")), nyq), p ("tr_gain"));
        scBand[c].setBandPass (sr, std::min ((double) hmF, nyq), 1.4);
    }
}

// Etapa de valvula clase A: saturacion suave asimetrica (armonicos pares) normalizada a ganancia unitaria
float TopStripProcessor::tube (float x, int ch) noexcept
{
    constexpr float bias = 0.15f;
    static const float tb = std::tanh (bias), slope = 1.0f - tb * tb;
    const float y = (std::tanh (tubeDrive * x + bias) - tb) / (tubeDrive * slope);
    // bloqueo de DC por la asimetria
    const float out = y - dcX[ch] + dcCoef * dcY[ch];
    dcX[ch] = y; dcY[ch] = out;
    return out;
}

void TopStripProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int chans = std::min (2, buffer.getNumChannels());
    for (int c = chans; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);
    if (n == 0 || chans == 0) return;

    updateFilters();

    // copia "dry" para el bypass sin clicks
    dryBuf.setSize (2, n, false, false, true);
    for (int c = 0; c < chans; ++c) dryBuf.copyFrom (c, 0, buffer, c, 0, n);

    // ---- medidor de entrada
    float pk = 0.0f;
    for (int c = 0; c < chans; ++c) pk = std::max (pk, buffer.getMagnitude (c, 0, n));
    inPeak = std::max (pk, inPeak * 0.9f);
    inLevelDb.store (juce::Decibels::gainToDecibels (inPeak, -100.0f));

    // ---- PREAMP: pad, fase, ganancia, pasa altos
    const bool mic = on ("mic");
    inGainSm.setTargetValue (dbToGain (p ("pre_gain") + (on ("pad") ? -20.0f : 0.0f)) * (on ("phase") ? -1.0f : 1.0f));
    for (int i = 0; i < n; ++i)
    {
        const float g = inGainSm.getNextValue();
        for (int c = 0; c < chans; ++c)
        {
            auto* d = buffer.getWritePointer (c);
            d[i] = hpf[c].process (d[i] * g);
        }
    }

    // ---- valvula (2x oversampling). MIC = mas color de valvula al mismo nivel
    tubeDrive = mic ? 3.0f : 1.0f;
    {
        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) chans, (size_t) n);
        auto up = oversampling.processSamplesUp (block);
        for (size_t c = 0; c < up.getNumChannels(); ++c)
        {
            auto* d = up.getChannelPointer (c);
            for (size_t i = 0; i < up.getNumSamples(); ++i) d[i] = tube (d[i], (int) c);
        }
        oversampling.processSamplesDown (block);
    }

    // ---- EQ y compresor optico (orden seleccionable)
    const bool eqOn = on ("eq_on"), compOn = on ("comp_on"), eqPre = on ("eq_pre"), scLink = on ("sc_link");
    const float thr = p ("thr"), ratio = p ("ratio");
    const float attCoef = std::exp (-1.0f / (0.001f * p ("att") * (float) sr));
    const float relMs = 60.0f + p ("rel") * 0.01f * 1940.0f;     // 60 ms .. 2 s
    const float relFast = std::exp (-1.0f / (0.001f * relMs * 0.35f * (float) sr));
    const float relSlow = std::exp (-1.0f / (0.001f * relMs * 3.0f * (float) sr));
    const float slowRise = std::exp (-1.0f / (2.0f * (float) sr));   // memoria de la celda optica
    const float hmBoost = dbToGain (std::max (6.0f, p ("hm_gain") + 6.0f));
    float maxGr = 0.0f;

    auto eq = [this] (float x, int c) { return treble[c].process (highMid[c].process (lowMid[c].process (bass[c].process (x)))); };

    for (int i = 0; i < n; ++i)
    {
        float s[2] { buffer.getSample (0, i), chans > 1 ? buffer.getSample (1, i) : 0.0f };
        if (eqOn && eqPre) for (int c = 0; c < chans; ++c) s[c] = eq (s[c], c);

        if (compOn)
        {
            float det = 0.0f;
            for (int c = 0; c < chans; ++c)
            {
                float d = s[c];
                if (scLink) d = d * 0.3f + scBand[c].process (d) * hmBoost;    // de-esser: el sidechain escucha las altas medias
                det = std::max (det, std::abs (d));
            }
            // detector optico: ataque segun ATTACK, release en dos etapas que depende del programa
            const float coef = det > env ? attCoef : (env - optoSlow > 0.0f ? relFast : relSlow);
            env = det + coef * (env - det);
            optoSlow = env + slowRise * (optoSlow - env);
            const float lvl = juce::Decibels::gainToDecibels (std::max (env, optoSlow * 0.7f), -100.0f);
            const float over = lvl - thr;
            const float knee = 8.0f;
            float gr = 0.0f;
            if (over > knee * 0.5f) gr = over * (1.0f - 1.0f / ratio);
            else if (over > -knee * 0.5f) { const float k = over + knee * 0.5f; gr = (1.0f - 1.0f / ratio) * k * k / (2.0f * knee); }
            const float g = dbToGain (-gr);
            for (int c = 0; c < chans; ++c) s[c] *= g;
            maxGr = std::max (maxGr, gr);
        }
        else env = optoSlow = 0.0f;

        if (eqOn && ! eqPre) for (int c = 0; c < chans; ++c) s[c] = eq (s[c], c);
        for (int c = 0; c < chans; ++c) buffer.setSample (c, i, s[c]);
    }
    grSm = maxGr > grSm ? maxGr : grSm * 0.85f + maxGr * 0.15f;
    grDb.store (-grSm);

    // ---- salida + POWER (bypass alineado con la latencia)
    outGainSm.setTargetValue (dbToGain (p ("out")));
    powerSm.setTargetValue (on ("power") ? 1.0f : 0.0f);
    const int L = dryLatency + 1;
    for (int i = 0; i < n; ++i)
    {
        const float og = outGainSm.getNextValue(), pw = powerSm.getNextValue();
        const int rd = (dryPos + 1) % L;
        for (int c = 0; c < chans; ++c)
        {
            auto& ring = dryRing[c];
            ring[(size_t) dryPos] = dryBuf.getSample (c, i);
            const float dry = ring[(size_t) rd];
            const float wet = buffer.getSample (c, i) * og;
            buffer.setSample (c, i, dry + (wet - dry) * pw);
        }
        dryPos = rd;
    }
    if (chans == 1 && buffer.getNumChannels() > 1) buffer.copyFrom (1, 0, buffer, 0, 0, n);

    pk = 0.0f;
    for (int c = 0; c < chans; ++c) pk = std::max (pk, buffer.getMagnitude (c, 0, n));
    outPeak = std::max (pk, outPeak * 0.9f);
    outLevelDb.store (juce::Decibels::gainToDecibels (outPeak, -100.0f));
}

//==============================================================================
int TopStripProcessor::getNumPrograms() { return (int) stripPresets().size(); }

const juce::String TopStripProcessor::getProgramName (int i)
{
    return juce::isPositiveAndBelow (i, (int) stripPresets().size()) ? juce::String (stripPresets()[(size_t) i].name) : juce::String();
}

void TopStripProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) stripPresets().size())) return;
    currentProgram = index;
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
            rp->setValueNotifyingHost (rp->getDefaultValue());
    for (const auto& [id, val] : stripPresets()[(size_t) index].v)
        if (auto* rp = apvts.getParameter (id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (val));
}

void TopStripProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void TopStripProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = (int) tree.getProperty ("program", 0);
            apvts.replaceState (tree);
        }
}

juce::AudioProcessorEditor* TopStripProcessor::createEditor() { return new TopStripEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TopStripProcessor(); }
