#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace tvc;

// Limitador suave de seguridad: transparente hasta -0.9 dBFS, nunca pasa de 0 dBFS
static inline float safetyClip (float x) noexcept
{
    const float a = std::abs (x);
    if (a <= 0.9f) return x;
    return std::copysign (0.9f + 0.1f * std::tanh ((a - 0.9f) * 10.0f), x);
}

//==============================================================================
const juce::StringArray& TopVocalChainProcessor::keyNames()
{
    static const juce::StringArray k { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return k;
}

const juce::StringArray& TopVocalChainProcessor::scaleNames()
{
    static const juce::StringArray s { "Cromatica", "Mayor", "Menor", "Menor Armonica", "Penta Mayor", "Penta Menor" };
    return s;
}

const juce::StringArray& TopVocalChainProcessor::divisionNames()
{
    static const juce::StringArray d { "1/2", "1/4", "1/4 D", "1/4 T", "1/8", "1/8 D", "1/8 T", "1/16" };
    return d;
}

const juce::StringArray& TopVocalChainProcessor::preTypeNames()
{
    static const juce::StringArray s { "Valvula", "Cinta", "Transistor", "Transformador", "Diodo" };
    return s;
}

const juce::StringArray& TopVocalChainProcessor::compTypeNames()
{
    static const juce::StringArray s { "Limpio (VCA)", "Opto (estilo LA-2A)", "FET (estilo 1176)", "Vari-Mu (estilo Fairchild)" };
    return s;
}

const juce::StringArray& TopVocalChainProcessor::eqTypeNames()
{
    static const juce::StringArray s { "Limpio (Digital)", "Britanico (estilo Neve 1073)", "Americano (estilo API 550)", "Pasivo (estilo Pultec EQP-1A)" };
    return s;
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout TopVocalChainProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;

    auto addF = [&ps] (const char* id, const char* name, float lo, float hi, float def, const char* unit, float skewCentre = 0.0f)
    {
        NormalisableRange<float> r (lo, hi, 0.0f);
        if (skewCentre > 0.0f) r.setSkewForCentre (skewCentre);
        const float span = hi - lo;
        const int decimals = span >= 100.0f ? 0 : (span >= 10.0f ? 1 : 2);
        const String u (unit);
        auto toText = [decimals, u] (float v, int)
        {
            String s = (u == "Hz" && v >= 1000.0f) ? String (v / 1000.0f, 1) + " kHz"
                                                   : String (v, decimals) + (u.isNotEmpty() ? " " + u : String());
            return s;
        };
        auto fromText = [] (const String& t) { return t.retainCharacters ("-0123456789.").getFloatValue()
                                                      * (t.containsIgnoreCase ("k") ? 1000.0f : 1.0f); };
        ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def,
                                                             AudioParameterFloatAttributes().withLabel (unit)
                                                                                            .withStringFromValueFunction (toText)
                                                                                            .withValueFromStringFunction (fromText)));
    };
    auto addB = [&ps] (const char* id, const char* name, bool def)
    {
        ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };
    auto addC = [&ps] (const char* id, const char* name, const StringArray& items, int def)
    {
        ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def));
    };

    // Global
    addF ("in_gain",  "Input",  -24.0f, 24.0f, 0.0f, "dB");
    addF ("out_gain", "Output", -24.0f, 12.0f, 0.0f, "dB");

    // IA
    addF ("ai_amount", "IA Voice", 0.0f, 100.0f, 50.0f, "%");
    addB ("ai_listen", "IA Escuchar", true);

    // Autotune
    addB ("tune_on", "Autotune On", true);
    addC ("tune_key", "Tonalidad", keyNames(), 0);
    addC ("tune_scale", "Escala", scaleNames(), 0);
    addF ("tune_speed", "Velocidad", 0.0f, 400.0f, 40.0f, "ms", 60.0f);
    addF ("tune_amount", "Cantidad", 0.0f, 100.0f, 100.0f, "%");

    // Preamp
    addB ("pre_on", "Preamp On", true);
    addC ("pre_type", "Tipo de Saturacion", preTypeNames(), 0);
    addF ("pre_drive", "Drive", 0.0f, 24.0f, 4.0f, "dB");
    addF ("pre_warmth", "Warmth", 0.0f, 100.0f, 40.0f, "%");
    addF ("pre_trim", "Trim", -12.0f, 12.0f, 0.0f, "dB");

    // EQ
    addB ("eq_on", "EQ On", true);
    addC ("eq_type", "Tipo de EQ", eqTypeNames(), 0);
    addF ("eq_lowcut", "Low Cut", 20.0f, 500.0f, 80.0f, "Hz", 100.0f);
    addF ("eq_low_gain", "Low", -12.0f, 12.0f, 0.0f, "dB");
    addF ("eq_mid_freq", "Mid Freq", 200.0f, 5000.0f, 1000.0f, "Hz", 1000.0f);
    addF ("eq_mid_gain", "Mid", -12.0f, 12.0f, 0.0f, "dB");
    addF ("eq_mid_q", "Mid Q", 0.3f, 5.0f, 1.0f, "", 1.0f);
    addF ("eq_pres_gain", "Presencia", -12.0f, 12.0f, 2.0f, "dB");
    addF ("eq_air_gain", "Aire", -12.0f, 12.0f, 2.0f, "dB");

    // Compresor
    addB ("comp_on", "Comp On", true);
    addC ("comp_type", "Tipo de Compresor", compTypeNames(), 0);
    addF ("comp_thr", "Threshold", -60.0f, 0.0f, -18.0f, "dB");
    addF ("comp_ratio", "Ratio", 1.0f, 20.0f, 4.0f, ":1", 4.0f);
    addF ("comp_att", "Attack", 0.1f, 100.0f, 5.0f, "ms", 8.0f);
    addF ("comp_rel", "Release", 10.0f, 1000.0f, 120.0f, "ms", 150.0f);
    addF ("comp_makeup", "Makeup", 0.0f, 24.0f, 3.0f, "dB");

    // Delay
    addB ("dly_on", "Delay On", true);
    addF ("dly_time", "Tiempo", 10.0f, 2000.0f, 250.0f, "ms", 300.0f);
    addB ("dly_sync", "Sync", true);
    addC ("dly_div", "Division", divisionNames(), 4);
    addF ("dly_fb", "Feedback", 0.0f, 95.0f, 25.0f, "%");
    addF ("dly_mix", "Mix", 0.0f, 100.0f, 12.0f, "%");
    addF ("dly_hicut", "Hi Cut", 1000.0f, 20000.0f, 6000.0f, "Hz", 5000.0f);
    addB ("dly_pingpong", "Ping Pong", false);

    // Reverb
    addB ("rev_on", "Reverb On", true);
    addF ("rev_size", "Size", 0.0f, 100.0f, 45.0f, "%");
    addF ("rev_damp", "Damping", 0.0f, 100.0f, 50.0f, "%");
    addF ("rev_width", "Width", 0.0f, 100.0f, 100.0f, "%");
    addF ("rev_mix", "Mix", 0.0f, 100.0f, 15.0f, "%");

    return { ps.begin(), ps.end() };
}

//==============================================================================
TopVocalChainProcessor::TopVocalChainProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TopVocalChain", createLayout())
{
}

bool TopVocalChainProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    const auto in = layouts.getMainInputChannelSet();
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

//==============================================================================
void TopVocalChainProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    maxBlock = std::max (32, samplesPerBlock);

    detector.prepare (sr);
    for (auto& s : shifter) s.prepare (sr);
    corrSemis = corrTarget = 0.0f;
    heldNote = -1;

    ai.prepare (sr);
    monoBuffer.allocate ((size_t) maxBlock, true);

    oversampling.reset();
    oversampling.initProcessing ((size_t) maxBlock);
    preamp.prepare (sr * 2.0);

    for (auto& band : eq) for (auto& f : band) f.reset();
    eqCacheValid = false;

    comp.prepare (sr);
    delay.prepare (sr);

    reverb.reset();
    reverb.setSampleRate (sr);
    reverbBuffer.setSize (2, maxBlock);
    for (auto& f : revHp) { f.setHighPass (sr, 250.0, 0.707); f.reset(); }

    inGainSm.reset (sr, 0.05);
    outGainSm.reset (sr, 0.05);
    inGainSm.setCurrentAndTargetValue (dbToGain (p ("in_gain")));
    outGainSm.setCurrentAndTargetValue (dbToGain (p ("out_gain")));

    setLatencySamples (shifter[0].getLatency() + (int) std::round (oversampling.getLatencyInSamples()));
}

//==============================================================================
double TopVocalChainProcessor::getDelayMs() const
{
    if (! on ("dly_sync"))
        return p ("dly_time");

    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm())
                if (*b > 20.0) bpm = *b;

    static constexpr double beats[] { 2.0, 1.0, 1.5, 2.0 / 3.0, 0.5, 0.75, 1.0 / 3.0, 0.25 };
    const int idx = juce::jlimit (0, 7, (int) p ("dly_div"));
    return juce::jlimit (10.0, 2000.0, beats[idx] * 60000.0 / bpm);
}

static float snapTo (float v, std::initializer_list<float> steps)
{
    float best = *steps.begin(), bestD = 1.0e9f;
    for (float s : steps)
    {
        const float d = std::abs (std::log (v / s));
        if (d < bestD) { bestD = d; best = s; }
    }
    return best;
}

void TopVocalChainProcessor::updateEq (float aiAmt)
{
    const auto& s = ai.get();
    const int type = (int) p ("eq_type");
    const float lowCut = juce::jlimit (20.0f, 600.0f, p ("eq_lowcut") + aiAmt * s.lowCutHz);
    const float lowG = p ("eq_low_gain"), midF = p ("eq_mid_freq"), midQ = p ("eq_mid_q"), midG = p ("eq_mid_gain");
    const float presG = p ("eq_pres_gain") + aiAmt * s.presenceDb;
    const float airG = p ("eq_air_gain") + aiAmt * s.airDb;

    BandSpec b[numBands];
    b[bAiMud]   = { fPeak, 330.0f, 1.2f, aiAmt * s.mudDb };
    b[bAiDeEss] = { fPeak, 7000.0f, 2.0f, aiAmt * s.deEssDb };

    switch (type)
    {
        case 1: // Britanico (estilo Neve 1073): frecuencias por pasos, filtro de 18 dB/oct, curvas anchas
        {
            if (lowCut >= 35.0f)
            {
                const float f = snapTo (lowCut, { 50.0f, 80.0f, 160.0f, 300.0f });
                b[bLowCut]  = { fHP2, f, 0.9f, 0.0f };
                b[bLowCut2] = { fHP1, f, 0.707f, 0.0f };
            }
            b[bLow]    = { fLowShelf, 110.0f, 0.707f, lowG };
            b[bLowDip] = { fPeak, 90.0f, 0.9f, lowG * 0.25f };                 // "cuerpo" extra del shelf Neve
            b[bMid]    = { fPeak, snapTo (midF, { 360.0f, 700.0f, 1600.0f, 3200.0f, 4800.0f }), 0.7f, midG };
            b[bPres]   = { fPeak, 4800.0f, 0.8f, presG };
            b[bAir]    = { fHighShelf, 12000.0f, 0.707f, airG };
            break;
        }
        case 2: // Americano (estilo API 550): Q proporcional (mas ganancia = curva mas estrecha)
        {
            auto pq = [] (float g) { return 0.4f + 0.15f * std::abs (g); };
            b[bLowCut] = { fHP2, lowCut, 0.707f, 0.0f };
            b[bLow]    = { fLowShelf, 100.0f, 0.707f, lowG };
            b[bMid]    = { fPeak, snapTo (midF, { 400.0f, 800.0f, 1500.0f, 3000.0f, 5000.0f }), pq (midG), midG };
            b[bPres]   = { fPeak, 5000.0f, pq (presG), presG };
            b[bAir]    = { fHighShelf, 10000.0f, 0.707f, airG };
            break;
        }
        case 3: // Pasivo (estilo Pultec EQP-1A): truco de graves (boost + corte), agudos amplios y sedosos
        {
            b[bLowCut] = { fHP2, lowCut, 0.5f, 0.0f };
            b[bLow]    = { fLowShelf, 140.0f, 0.707f, lowG };
            if (lowG > 0.0f) b[bLowDip] = { fPeak, 380.0f, 0.8f, -lowG * 0.4f };   // el "truco Pultec"
            b[bMid]    = { fPeak, midF, std::max (0.3f, midQ * 0.5f), midG };
            b[bPres]   = { fPeak, 8000.0f, 0.5f, presG };
            b[bAir]    = airG >= 0.0f ? BandSpec { fHighShelf, 12000.0f, 0.707f, airG }
                                      : BandSpec { fHighShelf, 10000.0f, 0.707f, airG };
            break;
        }
        default: // Limpio (Digital)
            b[bLowCut] = { fHP2, lowCut, 0.707f, 0.0f };
            b[bLow]    = { fLowShelf, 150.0f, 0.707f, lowG };
            b[bMid]    = { fPeak, midF, midQ, midG };
            b[bPres]   = { fPeak, 4000.0f, 0.9f, presG };
            b[bAir]    = { fHighShelf, 11000.0f, 0.707f, airG };
            break;
    }

    for (int k = 0; k < numBands; ++k)
    {
        const auto& n = b[k];
        auto& c = eqCache[k];
        if (eqCacheValid && n.kind == c.kind && std::abs (n.f - c.f) < 1.0e-3f
            && std::abs (n.q - c.q) < 1.0e-4f && std::abs (n.g - c.g) < 1.0e-3f)
            continue;
        c = n;
        for (auto& filt : eq[k])
        {
            switch (n.kind)
            {
                case fHP2:       filt.setHighPass (sr, n.f, n.q); break;
                case fHP1:       filt.setHighPass1 (sr, n.f); break;
                case fLowShelf:  filt.setLowShelf (sr, n.f, n.g); break;
                case fHighShelf: filt.setHighShelf (sr, n.f, n.g); break;
                case fPeak:      filt.setPeak (sr, n.f, n.q, n.g); break;
                default:         filt.setIdentity(); break;
            }
        }
    }
    eqCacheValid = true;
}

//==============================================================================
void TopVocalChainProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    for (int ch = numIn; ch < numOut; ++ch) buffer.clear (ch, 0, n);
    if (n == 0 || numIn == 0) return;

    // Mono -> si la salida es estereo duplicamos
    if (numIn == 1 && numOut > 1) buffer.copyFrom (1, 0, buffer, 0, 0, n);
    const int numCh = juce::jmin (2, numOut);

    float* L = buffer.getWritePointer (0);
    float* R = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

    if (n > maxBlock)
    {
        // Host envio un bloque mas grande de lo anunciado: re-preparar
        prepareToPlay (sr, n);
        L = buffer.getWritePointer (0);
        R = numCh > 1 ? buffer.getWritePointer (1) : nullptr;
    }

    if (aiResetRequested.exchange (false)) ai.reset();

    const float aiAmt = p ("ai_amount") * 0.01f;
    const bool listen = on ("ai_listen");

    // --- Medidor de entrada
    meters.inLevelDb.store (gainToDb (buffer.getMagnitude (0, n)));

    // --- Ganancia de entrada (+ ajuste IA de gain staging)
    inGainSm.setTargetValue (dbToGain (p ("in_gain") + aiAmt * ai.get().gainDb));
    for (int i = 0; i < n; ++i)
    {
        const float g = inGainSm.getNextValue();
        L[i] *= g;
        if (R) R[i] *= g;
        monoBuffer[i] = R ? 0.5f * (L[i] + R[i]) : L[i];
    }

    // --- IA escucha la voz
    ai.process (monoBuffer.get(), n, listen);

    // --- AUTOTUNE
    {
        const bool tuneOn = on ("tune_on");
        const int key = (int) p ("tune_key");
        const int scale = (int) p ("tune_scale");
        const float amount = p ("tune_amount") * 0.01f;
        const float speedMs = juce::jlimit (0.0f, 400.0f, p ("tune_speed") + aiAmt * ai.get().tuneSpeedMs);
        const float voicedCoef = 1.0f - std::exp (-1.0f / (std::max (0.3f, speedMs) * 0.001f * (float) sr));
        const float releaseCoef = 1.0f - std::exp (-1.0f / (0.030f * (float) sr));
        bool voicedNow = heldNote >= 0;

        for (int i = 0; i < n; ++i)
        {
            ++samplesSinceEstimate;
            if (detector.push (monoBuffer[i]))
            {
                const double secs = samplesSinceEstimate / sr;
                samplesSinceEstimate = 0;

                if (detector.isVoiced())
                {
                    const float hz = detector.getFrequency();
                    const float midi = 69.0f + 12.0f * std::log2 (hz / 440.0f);
                    const int cand = Scales::nearest (midi, key, scale);
                    if (! (heldNote >= 0 && Scales::allowed (heldNote, key, scale) && std::abs (midi - (float) heldNote) < 0.75f))
                        heldNote = cand;

                    const float off = (float) heldNote - midi;
                    corrTarget = tuneOn ? off * amount : 0.0f;
                    voicedNow = true;
                    ai.addPitchObservation (hz, -off * 100.0f, secs, listen);

                    meters.pitchHz.store (hz);
                    meters.targetNote.store ((float) heldNote);
                }
                else
                {
                    corrTarget = 0.0f;
                    heldNote = -1;
                    voicedNow = false;
                    meters.pitchHz.store (0.0f);
                    meters.targetNote.store (-1.0f);
                }
            }

            corrSemis += (corrTarget - corrSemis) * (voicedNow ? voicedCoef : releaseCoef);
            const float ratio = std::exp2 (corrSemis / 12.0f);
            L[i] = shifter[0].process (L[i], ratio);
            if (R) R[i] = shifter[1].process (R[i], ratio);
        }
        meters.correctionCents.store (corrSemis * 100.0f);
    }

    // --- PREAMP (oversampling 2x para saturacion limpia)
    {
        const bool preOn = on ("pre_on");
        preamp.set ((int) p ("pre_type"), p ("pre_drive"), p ("pre_warmth") * 0.01f, p ("pre_trim"));

        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numCh, (size_t) n);
        auto up = oversampling.processSamplesUp (block);
        if (preOn)
        {
            for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
            {
                auto* d = up.getChannelPointer (ch);
                for (size_t i = 0; i < up.getNumSamples(); ++i)
                    d[i] = preamp.process (d[i], (int) ch);
            }
        }
        oversampling.processSamplesDown (block);
    }

    // --- EQ
    if (on ("eq_on"))
    {
        updateEq (aiAmt);
        const int eqType = (int) p ("eq_type");
        const float eqColour = eqType == 1 ? 0.35f : eqType == 2 ? 0.25f : eqType == 3 ? 0.3f : 0.0f;
        const float eqBias = eqType == 3 ? 0.15f : 0.0f;    // Pultec: valvula, armonicos pares
        for (int ch = 0; ch < numCh; ++ch)
        {
            float* d = buffer.getWritePointer (ch);
            for (int i = 0; i < n; ++i)
            {
                float x = d[i];
                for (int b = 0; b < numBands; ++b) x = eq[b][ch].process (x);
                // Color analogico del modelo de EQ (transformadores / amplificadores)
                if (eqColour > 0.0f) x += eqColour * (std::tanh (x * 1.5f + eqBias) - std::tanh (eqBias) - 1.5f * x) / 1.5f;
                d[i] = x;
            }
        }
    }

    // --- COMPRESOR
    if (on ("comp_on"))
    {
        const auto& s = ai.get();
        comp.set ((int) p ("comp_type"), p ("comp_thr") + aiAmt * s.compThrDb, p ("comp_ratio") + aiAmt * s.compRatio,
                  p ("comp_att"), p ("comp_rel"), p ("comp_makeup"));
        float maxGr = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float g = comp.computeGain (L[i], R ? R[i] : L[i]);
            L[i] = comp.colour (L[i] * g);
            if (R) R[i] = comp.colour (R[i] * g);
            maxGr = std::min (maxGr, comp.lastGr);
        }
        meters.compGrDb.store (maxGr);
    }
    else
    {
        comp.reset();
        meters.compGrDb.store (0.0f);
    }

    // --- DELAY
    {
        const bool dOn = on ("dly_on");
        if (dOn)
        {
            if (! delayWasOn) delay.reset();
            delay.set ((float) getDelayMs(), p ("dly_fb") * 0.01f, p ("dly_mix") * 0.01f, p ("dly_hicut"), on ("dly_pingpong"));
            for (int i = 0; i < n; ++i)
            {
                float l = L[i], r = R ? R[i] : L[i];
                delay.process (l, r);
                L[i] = R ? l : 0.5f * (l + r);
                if (R) R[i] = r;
            }
        }
        delayWasOn = dOn;
    }

    // --- REVERB (con low cut de 250 Hz en la senal humeda para que no embarre)
    if (on ("rev_on"))
    {
        juce::Reverb::Parameters rp;
        rp.roomSize = 0.3f + 0.68f * p ("rev_size") * 0.01f;
        rp.damping  = p ("rev_damp") * 0.01f;
        rp.width    = p ("rev_width") * 0.01f;
        rp.wetLevel = 0.33f;
        rp.dryLevel = 0.0f;
        reverb.setParameters (rp);

        const float mix = p ("rev_mix") * 0.01f;
        for (int ch = 0; ch < numCh; ++ch)
        {
            const float* src = buffer.getReadPointer (ch);
            float* w = reverbBuffer.getWritePointer (ch);
            for (int i = 0; i < n; ++i) w[i] = revHp[ch].process (src[i]);
        }

        if (numCh > 1) reverb.processStereo (reverbBuffer.getWritePointer (0), reverbBuffer.getWritePointer (1), n);
        else           reverb.processMono (reverbBuffer.getWritePointer (0), n);

        for (int ch = 0; ch < numCh; ++ch)
            buffer.addFrom (ch, 0, reverbBuffer, ch, 0, n, mix);
    }

    // --- SALIDA
    outGainSm.setTargetValue (dbToGain (p ("out_gain")));
    for (int i = 0; i < n; ++i)
    {
        const float g = outGainSm.getNextValue();
        L[i] = safetyClip (L[i] * g);
        if (R) R[i] = safetyClip (R[i] * g);
    }
    meters.outLevelDb.store (gainToDb (buffer.getMagnitude (0, n)));

    // --- Publicar sugerencias de la IA para la interfaz
    const auto& s = ai.get();
    meters.aiGain.store (s.gainDb);        meters.aiLowCut.store (s.lowCutHz);
    meters.aiMud.store (s.mudDb);          meters.aiPresence.store (s.presenceDb);
    meters.aiAir.store (s.airDb);          meters.aiDeEss.store (s.deEssDb);
    meters.aiCompThr.store (s.compThrDb);  meters.aiCompRatio.store (s.compRatio);
    meters.aiTuneSpeed.store (s.tuneSpeedMs);
    meters.aiConfidence.store (s.confidence);
    meters.aiAvgPitch.store (s.avgPitchHz);
}

//==============================================================================
int TopVocalChainProcessor::getNumPrograms()
{
    return (int) getFactoryPresets().size();
}

const juce::String TopVocalChainProcessor::getProgramName (int index)
{
    const auto& list = getFactoryPresets();
    if (index >= 0 && index < (int) list.size()) return list[(size_t) index].name;
    return {};
}

void TopVocalChainProcessor::setCurrentProgram (int index)
{
    const auto& list = getFactoryPresets();
    if (index < 0 || index >= (int) list.size()) return;
    currentProgram = index;

    for (auto* param : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
            rp->setValueNotifyingHost (rp->getDefaultValue());

    for (const auto& [id, value] : list[(size_t) index].values)
        if (auto* rp = apvts.getParameter (id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (value));

    requestAIReset();
}

//==============================================================================
void TopVocalChainProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void TopVocalChainProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = (int) tree.getProperty ("program", 0);
            apvts.replaceState (tree);
        }
    }
}

juce::File TopVocalChainProcessor::getUserPresetFolder()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("Top Vocal Chain").getChildFile ("Presets");
    dir.createDirectory();
    return dir;
}

bool TopVocalChainProcessor::saveUserPreset (const juce::File& file)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        return xml->writeTo (file.withFileExtension ("tvcpreset"));
    return false;
}

bool TopVocalChainProcessor::loadUserPreset (const juce::File& file)
{
    if (auto xml = juce::XmlDocument::parse (file))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            requestAIReset();
            return true;
        }
    }
    return false;
}

//==============================================================================
juce::AudioProcessorEditor* TopVocalChainProcessor::createEditor()
{
    return new TopVocalChainEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TopVocalChainProcessor();
}
