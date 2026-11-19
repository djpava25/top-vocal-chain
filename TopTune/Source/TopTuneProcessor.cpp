#include "TopTuneProcessor.h"
#include "TopTuneEditor.h"

//==============================================================================
const juce::StringArray& TopTuneProcessor::keyNames()
{
    static const juce::StringArray k { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return k;
}

const juce::StringArray& TopTuneProcessor::scaleNames()
{
    static const juce::StringArray s { "Cromatica", "Mayor", "Menor", "Menor Armonica", "Penta Mayor",
                                       "Penta Menor", "Blues", "Personalizada" };
    return s;
}

static constexpr int scaleMasks[7] = {
    0xFFF,                                                   // Cromatica
    (1<<0)|(1<<2)|(1<<4)|(1<<5)|(1<<7)|(1<<9)|(1<<11),        // Mayor
    (1<<0)|(1<<2)|(1<<3)|(1<<5)|(1<<7)|(1<<8)|(1<<10),        // Menor
    (1<<0)|(1<<2)|(1<<3)|(1<<5)|(1<<7)|(1<<8)|(1<<11),        // Menor armonica
    (1<<0)|(1<<2)|(1<<4)|(1<<7)|(1<<9),                       // Penta mayor
    (1<<0)|(1<<3)|(1<<5)|(1<<7)|(1<<10),                      // Penta menor
    (1<<0)|(1<<3)|(1<<5)|(1<<6)|(1<<7)|(1<<10),               // Blues
};

static inline float safetyClip (float x) noexcept
{
    const float a = std::abs (x);
    if (a <= 0.9f) return x;
    return std::copysign (0.9f + 0.1f * std::tanh ((a - 0.9f) * 10.0f), x);
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout TopTuneProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;

    auto addF = [&ps] (const char* id, const char* name, NormalisableRange<float> r, float def, String unit, int decimals)
    {
        auto toText = [unit, decimals] (float v, int) { return String (v, decimals) + (unit.isNotEmpty() ? " " + unit : String()); };
        auto fromText = [] (const String& t) { return t.retainCharacters ("-0123456789.").getFloatValue(); };
        ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def,
                          AudioParameterFloatAttributes().withLabel (unit).withStringFromValueFunction (toText)
                                                         .withValueFromStringFunction (fromText)));
    };
    auto addC = [&ps] (const char* id, const char* name, const StringArray& items, int def)
    {
        ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def));
    };

    NormalisableRange<float> speed (0.0f, 200.0f); speed.setSkewForCentre (30.0f);

    addC ("key", "Tonalidad", keyNames(), 0);
    addC ("scale", "Escala", scaleNames(), 0);
    addF ("speed", "Velocidad", speed, 25.0f, "ms", 0);
    addF ("amount", "Cantidad", { 0.0f, 100.0f }, 100.0f, "%", 0);
    addC ("style", "Estilo", { "Natural", "Expresivo", "Sostenido" }, 0);
    addF ("shift", "Pitch Shift", { -12.0f, 12.0f, 1.0f }, 0.0f, "st", 0);
    addF ("fine", "Fino", { -100.0f, 100.0f, 1.0f }, 0.0f, "cents", 0);
    addF ("formant", "Formant", { -5.0f, 5.0f, 0.1f }, 0.0f, "st", 1);
    addC ("midi", "Modo MIDI", { "MIDI Off", "MIDI Repitch", "MIDI Escala" }, 0);
    ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { "live", 1 }, "Modo Live", false));
    addF ("mix", "Mix", { 0.0f, 100.0f }, 100.0f, "%", 0);
    addF ("out", "Output", { -12.0f, 12.0f, 0.1f }, 0.0f, "dB", 1);

    return { ps.begin(), ps.end() };
}

//==============================================================================
TopTuneProcessor::TopTuneProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TopTune", createLayout())
{
}

bool TopTuneProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in = layouts.getMainInputChannelSet();
    const bool okOut = out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
    const bool okIn  = in  == juce::AudioChannelSet::mono() || in  == juce::AudioChannelSet::stereo();
    return okOut && okIn;
}

void TopTuneProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    detector.prepare (sr);
    psola.prepare (sr);
    for (auto& s : liveShifter) s.prepare (sr, 0.012);

    corrSemis = corrTarget = holdTime = 0.0f;
    heldNote = -1; samplesSinceEstimate = 0; period = 0.0; voiced = false;

    const int maxLat = std::max (psola.getLatency(), liveShifter[0].getLatency()) + 8;
    dryDelay.setSize (2, maxLat + 1);
    dryDelay.clear();
    dryPos = 0;

    outGain.reset (sr, 0.05); mixSm.reset (sr, 0.05);
    outGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (p ("out")));
    mixSm.setCurrentAndTargetValue (p ("mix") * 0.01f);

    liveActive = p ("live") > 0.5f;
    updateLatency();
    juce::ignoreUnused (samplesPerBlock);
}

void TopTuneProcessor::updateLatency()
{
    latency = liveActive ? liveShifter[0].getLatency() : psola.getLatency();
    setLatencySamples (latency);
}

//==============================================================================
int TopTuneProcessor::getActiveMask() const
{
    const int scale = (int) p ("scale");
    if (scale == customScaleIndex) return meters.customMask.load() & 0xFFF;
    const int key = (int) p ("key");
    const int m = scaleMasks[juce::jlimit (0, 6, scale)];
    return ((m << key) | (m >> (12 - key))) & 0xFFF;   // rotar a la tonalidad
}

int TopTuneProcessor::nearestInMask (float midi, int maskBits) const
{
    if (maskBits == 0) maskBits = 0xFFF;
    const int centre = (int) std::round (midi);
    int best = centre; float bestD = 1000.0f;
    for (int n = centre - 7; n <= centre + 7; ++n)
    {
        const int pc = ((n % 12) + 12) % 12;
        if (! (maskBits & (1 << pc))) continue;
        const float d = std::abs ((float) n - midi);
        if (d < bestD) { bestD = d; best = n; }
    }
    return best;
}

void TopTuneProcessor::startKeyDetection()
{
    for (auto& h : meters.keyHist) h.store (0.0f);
    meters.keyListening.store (true);
}

void TopTuneProcessor::handleMidi (const juce::MidiBuffer& midi)
{
    const int mode = (int) p ("midi");
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn())
        {
            const int n = m.getNoteNumber();
            notesOn[n] = true;
            lastMidiNote = n;
            if (mode == 2)
            {
                const long long now = sampleClock + meta.samplePosition;
                int mask = meters.customMask.load();
                if (now - lastScaleNoteTime > (long long) (sr * 1.5)) mask = 0;   // nueva escala
                mask |= 1 << (n % 12);
                meters.customMask.store (mask);
                meters.customMaskLearned.store (true);
                lastScaleNoteTime = now;
            }
        }
        else if (m.isNoteOff())
        {
            const int n = m.getNoteNumber();
            notesOn[n] = false;
            if (n == lastMidiNote)
            {
                lastMidiNote = -1;
                for (int k = 127; k >= 0; --k) if (notesOn[k]) { lastMidiNote = k; break; }
            }
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            std::fill (std::begin (notesOn), std::end (notesOn), false);
            lastMidiNote = -1;
        }
    }
}

//==============================================================================
void TopTuneProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels(), numOut = getTotalNumOutputChannels();
    for (int ch = numIn; ch < numOut; ++ch) buffer.clear (ch, 0, n);
    if (n == 0 || numIn == 0) { sampleClock += n; return; }
    if (numIn == 1 && numOut > 1) buffer.copyFrom (1, 0, buffer, 0, 0, n);
    const bool stereo = numOut > 1;

    handleMidi (midi);

    const bool wantLive = p ("live") > 0.5f;
    if (wantLive != liveActive)
    {
        liveActive = wantLive;
        psola.reset();
        for (auto& s : liveShifter) s.reset();
        dryDelay.clear();
        updateLatency();
    }

    const int mask = getActiveMask();
    const int style = (int) p ("style");
    const int midiMode = (int) p ("midi");
    const float amount = p ("amount") * 0.01f;
    const float speedMs = p ("speed");
    const float shiftSemis = p ("shift") + p ("fine") * 0.01f;
    const double formant = std::exp2 ((double) p ("formant") / 12.0);
    const float speedCoef = 1.0f - std::exp (-1.0f / (std::max (0.2f, speedMs) * 0.001f * (float) sr));
    const float releaseCoef = 1.0f - std::exp (-1.0f / (0.04f * (float) sr));
    const float hysteresis = style == 1 ? 0.55f : 0.75f;

    outGain.setTargetValue (juce::Decibels::decibelsToGain (p ("out")));
    mixSm.setTargetValue (p ("mix") * 0.01f);

    float* L = buffer.getWritePointer (0);
    float* R = stereo ? buffer.getWritePointer (1) : nullptr;
    const int dlen = dryDelay.getNumSamples();

    for (int i = 0; i < n; ++i)
    {
        const float mono = R ? 0.5f * (L[i] + R[i]) : L[i];
        ++samplesSinceEstimate;

        if (detector.push (mono))
        {
            const float hopSec = (float) (samplesSinceEstimate / sr);
            samplesSinceEstimate = 0;
            const bool wasVoiced = voiced;
            voiced = detector.isVoiced();

            if (voiced)
            {
                const float hz = detector.getFrequency();
                period = sr / hz;
                const float m = 69.0f + 12.0f * std::log2 (hz / 440.0f);

                // Natural: sigue el centro de la nota -> conserva vibrato y deslizamientos
                if (! wasVoiced || std::abs (m - smoothMidi) > 1.5f) smoothMidi = m;
                else smoothMidi += (m - smoothMidi) * (1.0f - std::exp (-hopSec / 0.12f));

                int target;
                if (midiMode == 1 && lastMidiNote >= 0)
                {
                    target = lastMidiNote;             // MIDI Repitch: la voz sigue el teclado
                    while (target - m > 6.0f) target -= 12;   // octava mas cercana
                    while (m - target > 6.0f) target += 12;
                }
                else
                {
                    const int pc = heldNote >= 0 ? ((heldNote % 12) + 12) % 12 : 0;
                    const bool keep = heldNote >= 0 && (mask & (1 << pc)) && std::abs (m - (float) heldNote) < hysteresis;
                    target = keep ? heldNote : nearestInMask (m, mask);
                }

                holdTime = (target == heldNote) ? holdTime + hopSec : 0.0f;
                heldNote = target;

                const float base = style == 0 ? smoothMidi : m;
                float corr = (float) target - base;
                if (style == 2) corr *= juce::jlimit (0.0f, 1.0f, (holdTime - 0.06f) / 0.15f);  // Sostenido
                corrTarget = corr * ((midiMode == 1 && lastMidiNote >= 0) ? 1.0f : amount);

                if (meters.keyListening.load())
                {
                    const int pc = ((int) std::round (m) % 12 + 12) % 12;
                    meters.keyHist[pc].store (meters.keyHist[pc].load() + hopSec * detector.getConfidence());
                }

                meters.pitchHz.store (hz);
                meters.inMidi.store (m);
                meters.targetNote.store ((float) target);
            }
            else
            {
                corrTarget = 0.0f;
                heldNote = -1;
                holdTime = 0.0f;
                period = 0.0;
                meters.pitchHz.store (0.0f);
                meters.targetNote.store (-1.0f);
            }

            // Historia para la grafica
            const int hp = (meters.histPos.load() + 1) % TopTuneMeters::historySize;
            meters.inHist[hp]  = voiced ? meters.inMidi.load() : 0.0f;
            meters.outHist[hp] = voiced ? meters.inMidi.load() + corrSemis + shiftSemis : 0.0f;
            meters.histPos.store (hp);
        }

        corrSemis += (corrTarget - corrSemis) * (voiced ? speedCoef : releaseCoef);
        const double ratio = std::exp2 ((double) (corrSemis + shiftSemis) / 12.0);

        // Senal seca retrasada la misma latencia
        const float dryL = L[i], dryR = R ? R[i] : L[i];
        int rd = dryPos - latency; if (rd < 0) rd += dlen;
        const float delL = dryDelay.getSample (0, rd), delR = dryDelay.getSample (1, rd);
        dryDelay.setSample (0, dryPos, dryL); dryDelay.setSample (1, dryPos, dryR);
        dryPos = (dryPos + 1) % dlen;

        float l = L[i], r = R ? R[i] : 0.0f;
        if (liveActive)
        {
            l = liveShifter[0].process (l, (float) ratio);
            if (R) r = liveShifter[1].process (r, (float) ratio);
        }
        else
        {
            psola.process (l, r, R != nullptr, voiced ? period : 0.0, ratio, formant);
        }

        const float mix = mixSm.getNextValue(), g = outGain.getNextValue();
        L[i] = safetyClip ((l * mix + delL * (1.0f - mix)) * g);
        if (R) R[i] = safetyClip ((r * mix + delR * (1.0f - mix)) * g);
    }

    meters.outMidi.store (meters.inMidi.load() + corrSemis + shiftSemis);
    sampleClock += n;
}

//==============================================================================
struct TTPreset { const char* name; std::vector<std::pair<const char*, float>> v; };

static const std::vector<TTPreset>& presets()
{
    static const std::vector<TTPreset> list {
        { "Natural Lead",        { { "style", 0 }, { "speed", 35 }, { "amount", 90 } } },
        { "Trap Duro",           { { "style", 1 }, { "speed", 0 }, { "amount", 100 }, { "scale", 2 } } },
        { "Pop Expresivo",       { { "style", 1 }, { "speed", 15 }, { "amount", 100 }, { "scale", 1 } } },
        { "Balada Sostenida",    { { "style", 2 }, { "speed", 60 }, { "amount", 85 }, { "scale", 1 } } },
        { "Hook Hip Hop",        { { "style", 1 }, { "speed", 5 }, { "amount", 100 }, { "scale", 5 } } },
        { "Armonia +3ra Mayor",  { { "shift", 4 }, { "mix", 50 }, { "speed", 20 } } },
        { "Armonia +5ta",        { { "shift", 7 }, { "mix", 50 }, { "speed", 20 } } },
        { "Octava Abajo",        { { "shift", -12 }, { "mix", 50 }, { "formant", -1 } } },
        { "Voz Grave (Formant)", { { "formant", -3 }, { "amount", 0 } } },
        { "Voz Aguda (Formant)", { { "formant", 3 }, { "amount", 0 } } },
        { "Live / Grabacion",    { { "live", 1 }, { "style", 1 }, { "speed", 20 } } },
    };
    return list;
}

int TopTuneProcessor::getNumPrograms() { return (int) presets().size(); }

const juce::String TopTuneProcessor::getProgramName (int i)
{
    return juce::isPositiveAndBelow (i, (int) presets().size()) ? juce::String (presets()[(size_t) i].name) : juce::String();
}

void TopTuneProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presets().size())) return;
    currentProgram = index;
    // La tonalidad/escala del usuario se respeta salvo que el preset la defina
    const float keyNow = apvts.getParameter ("key")->getValue();
    const float scaleNow = apvts.getParameter ("scale")->getValue();
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
            rp->setValueNotifyingHost (rp->getDefaultValue());
    apvts.getParameter ("key")->setValueNotifyingHost (keyNow);
    apvts.getParameter ("scale")->setValueNotifyingHost (scaleNow);
    for (const auto& [id, val] : presets()[(size_t) index].v)
        if (auto* rp = apvts.getParameter (id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (val));
}

void TopTuneProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("customMask", meters.customMask.load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void TopTuneProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = (int) tree.getProperty ("program", 0);
            meters.customMask.store ((int) tree.getProperty ("customMask", 0xAB5));
            apvts.replaceState (tree);
        }
}

juce::AudioProcessorEditor* TopTuneProcessor::createEditor() { return new TopTuneEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TopTuneProcessor(); }
