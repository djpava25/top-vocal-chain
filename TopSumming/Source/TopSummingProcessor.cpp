#include "TopSummingProcessor.h"
#include "TopSummingEditor.h"

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout TopSummingProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;

    auto addF = [&ps] (const char* id, const char* name, float lo, float hi, float def, String unit, int dec)
    {
        auto toText = [unit, dec] (float v, int) { return String (v, dec) + (unit.isNotEmpty() ? " " + unit : String()); };
        auto fromText = [] (const String& t) { return t.retainCharacters ("-0123456789.").getFloatValue(); };
        ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi), def,
                          AudioParameterFloatAttributes().withLabel (unit).withStringFromValueFunction (toText)
                                                         .withValueFromStringFunction (fromText)));
    };
    auto addB = [&ps] (const char* id, const char* name, bool def)
    {
        ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };

    addB ("sum_in", "Sum In", true);
    addB ("bus", "Modo Bus", false);   // false = Channel, true = Bus (master / grupos)
    addF ("drive", "Drive", -6.0f, 18.0f, 0.0f, "dB", 1);
    addF ("iron", "Iron", 0.0f, 100.0f, 50.0f, "%", 0);
    addF ("width", "Width", 0.0f, 200.0f, 100.0f, "%", 0);
    addF ("crosstalk", "Crosstalk", 0.0f, 100.0f, 30.0f, "%", 0);
    addF ("output", "Output", -18.0f, 6.0f, 0.0f, "dB", 1);
    addB ("analog", "Analog (tolerancias)", true);
    addB ("noise", "Ruido de fondo", false);

    return { ps.begin(), ps.end() };
}

TopSummingProcessor::TopSummingProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TopSumming", createLayout())
{
    unitSeed.store ((unsigned) juce::Random::getSystemRandom().nextInt (1 << 30) + 1u);
}

bool TopSummingProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet(), in = l.getMainInputChannelSet();
    const bool okOut = out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
    const bool okIn  = in  == juce::AudioChannelSet::mono() || in  == juce::AudioChannelSet::stereo();
    return okOut && okIn;
}

void TopSummingProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    maxBlock = std::max (32, samplesPerBlock);
    oversampling.reset();
    oversampling.initProcessing ((size_t) maxBlock);
    for (auto& c : chan) c.prepare (sampleRate * 2.0);
    for (auto& f : xtalkHp) { f.setHighPass (sampleRate * 2.0, 1500.0, 0.707); f.reset(); }
    tolerancesDirty.store (true);

    for (auto* s : { &driveSm, &outSm, &widthSm, &ironSm, &xtalkSm }) s->reset (sampleRate * 2.0, 0.05);
    driveSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (p ("drive")));
    outSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (p ("output")));
    widthSm.setCurrentAndTargetValue (p ("width") * 0.01f);
    ironSm.setCurrentAndTargetValue (p ("iron") * 0.01f);
    xtalkSm.setCurrentAndTargetValue (0.0f);

    setLatencySamples ((int) std::round (oversampling.getLatencyInSamples()));
}

void TopSummingProcessor::applyTolerances()
{
    if (p ("analog") > 0.5f) topsum::makeTolerances (unitSeed.load(), chan[0], chan[1]);
    else for (auto& c : chan) c.setTolerances (1.0f, 1.0f, 1.0f, 1.0f);
}

void TopSummingProcessor::newAnalogUnit()
{
    unitSeed.store ((unsigned) juce::Random::getSystemRandom().nextInt (1 << 30) + 1u);
    tolerancesDirty.store (true);
}

//==============================================================================
void TopSummingProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels(), numOut = getTotalNumOutputChannels();
    for (int ch = numIn; ch < numOut; ++ch) buffer.clear (ch, 0, n);
    if (n == 0 || numIn == 0) return;
    if (numIn == 1 && numOut > 1) buffer.copyFrom (1, 0, buffer, 0, 0, n);
    const int numCh = juce::jmin (2, numOut);

    if (n > maxBlock) prepareToPlay (getSampleRate(), n);

    // Las tolerancias cambian si se activa/desactiva ANALOG
    {
        const bool analogNow = p ("analog") > 0.5f;
        if (analogNow != lastAnalog) { lastAnalog = analogNow; tolerancesDirty.store (true); }
    }
    if (tolerancesDirty.exchange (false)) applyTolerances();

    inLevelDb.store (juce::Decibels::gainToDecibels (buffer.getMagnitude (0, n), -100.0f));

    const bool on = p ("sum_in") > 0.5f;
    const bool bus = p ("bus") > 0.5f;
    driveSm.setTargetValue (juce::Decibels::decibelsToGain (p ("drive")));
    outSm.setTargetValue (juce::Decibels::decibelsToGain (p ("output")));
    widthSm.setTargetValue (p ("width") * 0.01f);
    ironSm.setTargetValue (juce::jlimit (0.0f, 1.0f, p ("iron") * 0.01f * (bus ? 1.15f : 1.0f)));
    xtalkSm.setTargetValue (bus ? p ("crosstalk") * 0.01f : 0.0f);
    const bool addNoise = p ("noise") > 0.5f;

    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numCh, (size_t) n);
    auto up = oversampling.processSamplesUp (block);
    const size_t m = up.getNumSamples();
    float* L = up.getChannelPointer (0);
    float* R = numCh > 1 ? up.getChannelPointer (1) : nullptr;

    for (size_t i = 0; i < m; ++i)
    {
        const float drive = driveSm.getNextValue(), iron = ironSm.getNextValue();
        const float width = widthSm.getNextValue(), xt = xtalkSm.getNextValue(), out = outSm.getNextValue();
        if (! on) continue;

        float l = L[i], r = R ? R[i] : L[i];

        // Crosstalk del bus: los canales "se escuchan" un poco entre si (sobre todo agudos), ~ -40 dB
        if (R && xt > 0.0f)
        {
            const float hl = xtalkHp[0].process (l), hr = xtalkHp[1].process (r);
            const float amt = 0.01f * xt;
            l += hr * amt;
            r += hl * amt;
        }

        l = chan[0].process (l, drive, iron);
        if (R) r = chan[1].process (r, drive, iron);

        // Ruido de fondo analogico (~ -88 dBFS)
        if (addNoise)
        {
            l += (noise.nextFloat() - 0.5f) * 8.0e-5f;
            if (R) r += (noise.nextFloat() - 0.5f) * 8.0e-5f;
        }

        // Ancho estereo (Mid/Side)
        if (R)
        {
            const float mid = 0.5f * (l + r), side = 0.5f * (l - r) * width;
            l = mid + side; r = mid - side;
        }

        L[i] = l * out;
        if (R) R[i] = r * out;
    }

    oversampling.processSamplesDown (block);
    outLevelDb.store (juce::Decibels::gainToDecibels (buffer.getMagnitude (0, n), -100.0f));
}

//==============================================================================
struct SumPreset { const char* name; std::vector<std::pair<const char*, float>> v; };

static const std::vector<SumPreset>& sumPresets()
{
    static const std::vector<SumPreset> list {
        { "Neutral (canal)",        { { "bus", 0 }, { "drive", 0 }, { "iron", 50 } } },
        { "Canal Calido",           { { "bus", 0 }, { "drive", 4 }, { "iron", 70 } } },
        { "Canal Empujado (Hot)",   { { "bus", 0 }, { "drive", 10 }, { "iron", 85 } } },
        { "Bus de Mezcla",          { { "bus", 1 }, { "drive", 2 }, { "iron", 55 }, { "crosstalk", 30 } } },
        { "Bus Pegado (Glue)",      { { "bus", 1 }, { "drive", 6 }, { "iron", 75 }, { "crosstalk", 40 }, { "output", -2 } } },
        { "Bus de Bateria",         { { "bus", 1 }, { "drive", 9 }, { "iron", 90 }, { "crosstalk", 25 }, { "width", 110 }, { "output", -3 } } },
        { "Bus de Voces",           { { "bus", 1 }, { "drive", 3 }, { "iron", 45 }, { "crosstalk", 20 }, { "width", 95 } } },
        { "Master Abierto",         { { "bus", 1 }, { "drive", 1 }, { "iron", 40 }, { "crosstalk", 20 }, { "width", 115 } } },
        { "Vintage con Ruido",      { { "bus", 1 }, { "drive", 5 }, { "iron", 80 }, { "crosstalk", 50 }, { "noise", 1 } } },
    };
    return list;
}

int TopSummingProcessor::getNumPrograms() { return (int) sumPresets().size(); }

const juce::String TopSummingProcessor::getProgramName (int i)
{
    return juce::isPositiveAndBelow (i, (int) sumPresets().size()) ? juce::String (sumPresets()[(size_t) i].name) : juce::String();
}

void TopSummingProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) sumPresets().size())) return;
    currentProgram = index;
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
            rp->setValueNotifyingHost (rp->getDefaultValue());
    for (const auto& [id, val] : sumPresets()[(size_t) index].v)
        if (auto* rp = apvts.getParameter (id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (val));
}

void TopSummingProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("unitSeed", (int) unitSeed.load(), nullptr);   // la misma "unidad" al reabrir el proyecto
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void TopSummingProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = (int) tree.getProperty ("program", 0);
            const int seed = (int) tree.getProperty ("unitSeed", 0);
            if (seed > 0) { unitSeed.store ((unsigned) seed); tolerancesDirty.store (true); }
            apvts.replaceState (tree);
        }
}

juce::AudioProcessorEditor* TopSummingProcessor::createEditor() { return new TopSummingEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TopSummingProcessor(); }
