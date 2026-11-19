#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <complex>

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
    static const juce::StringArray s { "Cromatica", "Mayor", "Menor (Natural)", "Menor Armonica", "Penta Mayor", "Penta Menor", "Blues" };
    return s;
}

const juce::StringArray& TopVocalChainProcessor::divisionNames()
{
    static const juce::StringArray d { "1/2", "1/4", "1/4 D", "1/4 T", "1/8", "1/8 D", "1/8 T", "1/16", "1/16 D", "1/16 T" };
    return d;
}

const juce::StringArray& TopVocalChainProcessor::preTypeNames()
{
    static const juce::StringArray s { "Clean", "Tube", "Vintage", "Modern", "Tape", "Transformer", "Transistor", "Diode" };
    return s;
}

const juce::StringArray& TopVocalChainProcessor::compTypeNames()
{
    static const juce::StringArray s { "Clean VCA", "Vintage Opto", "FET Punch", "Vari-Mu Glue", "LA Vocal", "Fast", "Smooth" };
    return s;
}

const juce::StringArray& TopVocalChainProcessor::eqTypeNames()
{
    static const juce::StringArray s { "Limpio (Digital)", "Britanico (estilo Neve 1073)", "Americano (estilo API 550)", "Pasivo (estilo Pultec EQP-1A)" };
    return s;
}

const juce::StringArray& TopVocalChainProcessor::reverbTypeNames()
{
    static const juce::StringArray s { "Room", "Plate", "Hall", "Chamber", "Vocal Plate" };
    return s;
}

const juce::StringArray& TopVocalChainProcessor::slopeNames()
{
    static const juce::StringArray s { "6 dB/oct", "12 dB/oct", "18 dB/oct", "24 dB/oct" };
    return s;
}

const juce::StringArray& TopVocalChainProcessor::tuneModeNames()
{
    static const juce::StringArray s { "Natural", "Robotic" };
    return s;
}

const juce::StringArray& TopVocalChainProcessor::wideModeNames()
{
    static const juce::StringArray s { "Wide", "Chorus", "Double", "Shift", "Dimension" };
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
    addB ("global_bypass", "Bypass", false);
    addF ("global_mix", "Dry/Wet", 0.0f, 100.0f, 100.0f, "%");
    addB ("limit_on", "Limitador", false);

    // IA
    addF ("ai_amount", "IA Voice", 0.0f, 100.0f, 50.0f, "%");
    addB ("ai_listen", "IA Escuchar", true);

    // Autotune
    addB ("tune_on", "Autotune On", true);
    addC ("tune_key", "Tonalidad", keyNames(), 0);
    addC ("tune_scale", "Escala", scaleNames(), 0);
    addF ("tune_speed", "Velocidad", 0.0f, 400.0f, 40.0f, "ms", 60.0f);
    addF ("tune_amount", "Cantidad", 0.0f, 100.0f, 100.0f, "%");
    addC ("tune_mode", "Modo Autotune", tuneModeNames(), 0);
    addF ("tune_humanize", "Humanize", 0.0f, 100.0f, 0.0f, "%");
    addF ("tune_flex", "Flex-Tune", 0.0f, 100.0f, 0.0f, "%");
    addF ("tune_formant", "Formant", -5.0f, 5.0f, 0.0f, "st");
    addF ("tune_vibrato", "Vibrato", 0.0f, 100.0f, 0.0f, "cents");
    addF ("tune_tracking", "Tracking", 0.0f, 100.0f, 50.0f, "%");
    {
        NormalisableRange<float> r (-12.0f, 12.0f, 1.0f);
        ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "tune_transpose", 1 }, "Transpose", r, 0.0f,
                          AudioParameterFloatAttributes().withLabel ("st")
                              .withStringFromValueFunction ([] (float v, int) { return (v > 0 ? "+" : "") + String ((int) v) + " st"; })));
    }
    addB ("tune_lowlat", "Autotune Baja Latencia", false);

    // Preamp
    addB ("pre_on", "Preamp On", true);
    addC ("pre_type", "Modelo de Preamp", preTypeNames(), 1);
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
    addC ("eq_hp_slope", "Low Cut Slope", slopeNames(), 1);
    addF ("eq_hicut", "High Cut", 2000.0f, 22000.0f, 22000.0f, "Hz", 9000.0f);
    addC ("eq_lp_slope", "High Cut Slope", slopeNames(), 1);

    // Compresor
    addB ("comp_on", "Comp On", true);
    addC ("comp_type", "Tipo de Compresor", compTypeNames(), 0);
    addF ("comp_thr", "Threshold", -60.0f, 0.0f, -18.0f, "dB");
    addF ("comp_ratio", "Ratio", 1.0f, 20.0f, 4.0f, ":1", 4.0f);
    addF ("comp_att", "Attack", 0.1f, 100.0f, 5.0f, "ms", 8.0f);
    addF ("comp_rel", "Release", 10.0f, 1000.0f, 120.0f, "ms", 150.0f);
    addF ("comp_makeup", "Makeup", 0.0f, 24.0f, 3.0f, "dB");
    addF ("comp_knee", "Knee", 0.0f, 24.0f, 6.0f, "dB");
    addF ("comp_mix", "Comp Mix", 0.0f, 100.0f, 100.0f, "%");
    addF ("comp_sc_hpf", "Detector HPF", 20.0f, 500.0f, 20.0f, "Hz", 100.0f);
    addB ("comp_sc_ext", "Sidechain Externo", false);

    // Stereo Lab
    addB ("wide_on", "Stereo Lab On", true);
    addC ("wide_mode", "Stereo Lab Efecto", wideModeNames(), 0);
    addF ("wide_amount", "Efecto", 0.0f, 100.0f, 30.0f, "%");
    addF ("wide_width", "Stereo Width", 0.0f, 200.0f, 100.0f, "%");
    addF ("wide_mix", "Stereo Lab Mix", 0.0f, 100.0f, 100.0f, "%");
    addB ("wide_mono", "Mono", false);
    addB ("wide_safe", "Modo Seguro Voz", true);

    // Delay
    addB ("dly_on", "Delay On", true);
    addF ("dly_time", "Tiempo", 10.0f, 2000.0f, 250.0f, "ms", 300.0f);
    addB ("dly_sync", "Sync", true);
    addC ("dly_div", "Division", divisionNames(), 4);
    addF ("dly_fb", "Feedback", 0.0f, 95.0f, 25.0f, "%");
    addF ("dly_mix", "Mix", 0.0f, 100.0f, 12.0f, "%");
    addF ("dly_hicut", "Hi Cut", 1000.0f, 20000.0f, 6000.0f, "Hz", 5000.0f);
    addB ("dly_pingpong", "Ping Pong", false);
    addF ("dly_locut", "Delay Lo Cut", 20.0f, 1000.0f, 120.0f, "Hz", 150.0f);
    addF ("dly_sat", "Delay Saturacion", 0.0f, 100.0f, 0.0f, "%");
    addF ("dly_mod", "Delay Modulacion", 0.0f, 100.0f, 0.0f, "%");
    addF ("dly_duck", "Delay Ducking", 0.0f, 100.0f, 0.0f, "%");
    addF ("dly_width", "Delay Width", 0.0f, 200.0f, 100.0f, "%");

    // Reverb
    addB ("rev_on", "Reverb On", true);
    addF ("rev_size", "Size", 0.0f, 100.0f, 45.0f, "%");
    addF ("rev_damp", "Damping", 0.0f, 100.0f, 50.0f, "%");
    addF ("rev_width", "Width", 0.0f, 100.0f, 100.0f, "%");
    addF ("rev_mix", "Mix", 0.0f, 100.0f, 15.0f, "%");
    addC ("rev_type", "Tipo de Reverb", reverbTypeNames(), 4);
    addF ("rev_predelay", "Pre-delay", 0.0f, 200.0f, 20.0f, "ms", 40.0f);
    addF ("rev_decay", "Decay", 0.2f, 8.0f, 1.6f, "s", 1.5f);
    addF ("rev_er", "Early Reflections", 0.0f, 100.0f, 30.0f, "%");
    addF ("rev_diff", "Diffusion", 0.0f, 100.0f, 70.0f, "%");
    addF ("rev_locut", "Reverb Lo Cut", 20.0f, 1000.0f, 250.0f, "Hz", 200.0f);
    addF ("rev_hicut", "Reverb Hi Cut", 1000.0f, 20000.0f, 9000.0f, "Hz", 6000.0f);
    addF ("rev_duck", "Reverb Ducking", 0.0f, 100.0f, 0.0f, "%");

    return { ps.begin(), ps.end() };
}

//==============================================================================
TopVocalChainProcessor::TopVocalChainProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                        .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, &undoManager, "TopVocalChain", createLayout())
{
}

bool TopVocalChainProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    const auto in = layouts.getMainInputChannelSet();
    if (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo()) return false;
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo()) return false;
    }
    return true;
}

void TopVocalChainProcessor::applyReport (const tvc::VoiceAnalysis::Report& r)
{
    if (! r.valid) return;
    undoManager.beginNewTransaction();
    auto set = [this] (const char* id, float v)
    {
        if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (prm->getNormalisableRange().snapToLegalValue (v)));
    };
    set ("in_gain", r.inGain);
    if (r.cutResonance) { set ("eq_mid_freq", r.midFreq); set ("eq_mid_gain", r.midGain); set ("eq_mid_q", r.midQ); }
    set ("eq_pres_gain", r.presGain);
    if (r.lowLabel != "Correctos") set ("eq_lowcut", r.lowCut);
    set ("comp_type", 4.0f);                  // LA Vocal
    set ("comp_thr", r.compThr);
    set ("comp_ratio", r.compRatio);
    set ("comp_knee", 6.0f);
    if (r.sibLabel != "Baja") set ("ai_amount", std::max (60.0f, p ("ai_amount")));
    set ("tune_key", (float) r.keyRoot);
    set ("tune_scale", r.minor ? 2.0f : 1.0f);
    set ("eq_on", 1.0f); set ("comp_on", 1.0f);
    requestAIReset();
}

void TopVocalChainProcessor::startKeyDetection()
{
    for (auto& h : meters.keyHist) h.store (0.0f);
    meters.keyListening.store (true);
}

void TopVocalChainProcessor::updateLatency()
{
    const int tuneLat = lowLatActive ? shifter[0].getLatency() : psola.getLatency();
    dryLatency = tuneLat + (int) std::round (oversampling.getLatencyInSamples());
    dryRing.clear(); scRing.clear();
    dryPos = scPos = 0;
    setLatencySamples (dryLatency);
}

//==============================================================================
void TopVocalChainProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    maxBlock = std::max (32, samplesPerBlock);

    detector.prepare (sr);
    for (auto& s : shifter) s.prepare (sr);
    psola.prepare (sr);
    corrSemis = corrTarget = 0.0f;
    heldNote = -1;
    formantSm = 1.0f; transposeSm = 0.0f; smoothMidi = 0.0f; holdTime = 0.0f; vibPhase = 0.0; period = 0.0; tuneCoef = 0.0f;
    lowLatActive = on ("tune_lowlat");

    ai.prepare (sr);
    monoBuffer.allocate ((size_t) maxBlock, true);

    oversampling.reset();
    oversampling.initProcessing ((size_t) maxBlock);
    preamp.prepare (sr * 2.0);

    for (auto& band : eq) for (auto& f : band) f.reset();
    eqCacheValid = false;

    comp.prepare (sr);
    compView.prepare (sr);
    for (auto& f : scHp) f.reset();
    scHpFreq = -1.0f;
    delay.prepare (sr);
    widener.prepare (sr);
    wideMs = wideSs = 0.0;

    fdn.prepare (sr);
    loud.prepare (sr);
    analysis.prepare (sr);
    for (auto& f : labSideHp) { f.setHighPass (sr, 150.0, 0.707); f.reset(); }
    labDry.setSize (2, maxBlock);

    inGainSm.reset (sr, 0.05);
    outGainSm.reset (sr, 0.05);
    inGainSm.setCurrentAndTargetValue (dbToGain (p ("in_gain")));
    outGainSm.setCurrentAndTargetValue (dbToGain (p ("out_gain")));

    // Fundidos de bypass por modulo (20 ms)
    for (auto* s : { &eqMixSm, &compMixSm, &wideMixSm, &dlyMixSm, &revMixSm, &procSm, &globalMixSm }) s->reset (sr, 0.02);
    preMixSm.reset (sr * 2.0, 0.02);
    preMixSm.setCurrentAndTargetValue (on ("pre_on") ? 1.0f : 0.0f);
    eqMixSm.setCurrentAndTargetValue (on ("eq_on") ? 1.0f : 0.0f);
    compMixSm.setCurrentAndTargetValue (on ("comp_on") ? 1.0f : 0.0f);
    wideMixSm.setCurrentAndTargetValue (on ("wide_on") ? 1.0f : 0.0f);
    dlyMixSm.setCurrentAndTargetValue (on ("dly_on") ? 1.0f : 0.0f);
    revMixSm.setCurrentAndTargetValue (on ("rev_on") ? 1.0f : 0.0f);
    procSm.setCurrentAndTargetValue (on ("global_bypass") ? 0.0f : 1.0f);
    globalMixSm.setCurrentAndTargetValue (p ("global_mix") * 0.01f);
    delayWasOn = on ("dly_on");
    revActive = on ("rev_on");

    scratch.setSize (2, maxBlock);
    scBuf.setSize (2, maxBlock);
    dryAligned.setSize (2, maxBlock);
    const int maxLat = std::max (psola.getLatency(), shifter[0].getLatency()) + (int) std::round (oversampling.getLatencyInSamples());
    dryRing.setSize (2, maxLat + 8);
    scRing.setSize (2, maxLat + 8);
    rmsProc = rmsDry = 1.0e-6;
    limGain = 1.0f;
    limRelCoef = 1.0f - (float) std::exp (-1.0 / (0.08 * sr));

    updateLatency();
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

    static constexpr double beats[] { 2.0, 1.0, 1.5, 2.0 / 3.0, 0.5, 0.75, 1.0 / 3.0, 0.25, 0.375, 1.0 / 6.0 };
    const int idx = juce::jlimit (0, 9, (int) p ("dly_div"));
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

void TopVocalChainProcessor::eqModelFreqs (int type, float airGain, float& lowF, float& presF, float& airF)
{
    switch (type)
    {
        case 1:  lowF = 110.0f; presF = 4800.0f; airF = 12000.0f; break;
        case 2:  lowF = 100.0f; presF = 5000.0f; airF = 10000.0f; break;
        case 3:  lowF = 140.0f; presF = 8000.0f; airF = airGain >= 0.0f ? 12000.0f : 10000.0f; break;
        default: lowF = 150.0f; presF = 4000.0f; airF = 11000.0f; break;
    }
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

    // Pendientes del Low Cut y High Cut (6 / 12 / 18 / 24 dB por octava)
    {
        const int hs = juce::jlimit (0, 3, (int) p ("eq_hp_slope"));
        if (b[bLowCut].kind != fIdentity)
        {
            const float f = b[bLowCut].f, q = b[bLowCut].q;
            switch (hs)
            {
                case 0:  b[bLowCut] = { fHP1, f, 0.707f, 0.0f }; b[bLowCut2] = {}; break;
                case 2:  b[bLowCut] = { fHP2, f, 1.0f, 0.0f };   b[bLowCut2] = { fHP1, f, 0.707f, 0.0f }; break;
                case 3:  b[bLowCut] = { fHP2, f, 0.541f, 0.0f }; b[bLowCut2] = { fHP2, f, 1.307f, 0.0f }; break;
                default: b[bLowCut] = { fHP2, f, q, 0.0f };      b[bLowCut2] = {}; break;
            }
        }
        const float hc = p ("eq_hicut");
        const int ls = juce::jlimit (0, 3, (int) p ("eq_lp_slope"));
        if (hc < 20000.0f)
        {
            switch (ls)
            {
                case 0:  b[bHighCut] = { fLP1, hc, 0.707f, 0.0f }; b[bHighCut2] = {}; break;
                case 2:  b[bHighCut] = { fLP2, hc, 1.0f, 0.0f };   b[bHighCut2] = { fLP1, hc, 0.707f, 0.0f }; break;
                case 3:  b[bHighCut] = { fLP2, hc, 0.541f, 0.0f }; b[bHighCut2] = { fLP2, hc, 1.307f, 0.0f }; break;
                default: b[bHighCut] = { fLP2, hc, 0.707f, 0.0f }; b[bHighCut2] = {}; break;
            }
        }
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
                case fLP2:       filt.setLowPass (sr, n.f, n.q); break;
                case fLP1:       filt.setLowPass1 (sr, n.f); break;
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
    const int numIn = getMainBusNumInputChannels();
    const int numOut = getMainBusNumOutputChannels();
    const int scChannels = getBusCount (true) > 1 && getBus (true, 1)->isEnabled() ? getBus (true, 1)->getNumberOfChannels() : 0;
    // Copia del sidechain antes de tocar el buffer (alineado con la latencia)
    if (n > maxBlock) prepareToPlay (sr, n);
    const bool scExt = scChannels > 0 && on ("comp_sc_ext");
    {
        const int ringSize = scRing.getNumSamples();
        for (int i = 0; i < n; ++i)
        {
            const float sl = scChannels > 0 ? buffer.getSample (numIn, i) : 0.0f;
            const float sr2 = scChannels > 1 ? buffer.getSample (numIn + 1, i) : sl;
            int rd = scPos - dryLatency; if (rd < 0) rd += ringSize;
            scRing.setSample (0, scPos, sl); scRing.setSample (1, scPos, sr2);
            scratch.setSample (0, i, scRing.getSample (0, rd)); scratch.setSample (1, i, scRing.getSample (1, rd));
            scPos = (scPos + 1) % ringSize;
        }
    }
    scBuf.copyFrom (0, 0, scratch, 0, 0, n);
    scBuf.copyFrom (1, 0, scratch, 1, 0, n);
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

    // --- Senal original alineada con la latencia del plugin (para bypass / dry-wet / comparar)
    {
        const int ringSize = dryRing.getNumSamples();
        for (int i = 0; i < n; ++i)
        {
            int rd = dryPos - dryLatency; if (rd < 0) rd += ringSize;
            dryRing.setSample (0, dryPos, L[i]);
            dryRing.setSample (1, dryPos, R ? R[i] : L[i]);
            dryAligned.setSample (0, i, dryRing.getSample (0, rd));
            dryAligned.setSample (1, i, dryRing.getSample (1, rd));
            dryPos = (dryPos + 1) % ringSize;
        }
    }

    // Fundido de bypass por modulo: procesa y mezcla con la senal previa sin clicks.
    // Devuelve false si el modulo esta totalmente apagado.
    auto xfade = [&] (juce::SmoothedValue<float>& sm, bool isOn, auto&& fn) -> bool
    {
        sm.setTargetValue (isOn ? 1.0f : 0.0f);
        if (! sm.isSmoothing())
        {
            if (sm.getCurrentValue() < 0.5f) return false;
            fn();
            return true;
        }
        for (int ch = 0; ch < numCh; ++ch) scratch.copyFrom (ch, 0, buffer, ch, 0, n);
        fn();
        for (int i = 0; i < n; ++i)
        {
            const float g = sm.getNextValue();
            for (int ch = 0; ch < numCh; ++ch)
            {
                float* d = buffer.getWritePointer (ch);
                const float dry = scratch.getSample (ch, i);
                d[i] = dry + g * (d[i] - dry);
            }
        }
        return true;
    };

    // --- Medidor de entrada
    meters.inLevelDb.store (gainToDb (buffer.getMagnitude (0, n)));

    // --- IA: ANALIZAR VOZ (con la senal original, antes de cualquier ganancia)
    if (analysis.isActive())
    {
        for (int i = 0; i < n; ++i) monoBuffer[i] = R ? 0.5f * (L[i] + R[i]) : L[i];
        analysis.push (monoBuffer.get(), n);
    }

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

    // --- AUTOTUNE (al apagarlo la correccion, el formant y el transpose se deslizan a 0: sin clicks)
    {
        const bool tuneOn = on ("tune_on");
        const bool robotic = (int) p ("tune_mode") == 1;
        const int key = (int) p ("tune_key");
        const int scale = (int) p ("tune_scale");
        const float amount = robotic ? 1.0f : p ("tune_amount") * 0.01f;
        const float baseSpeed = robotic ? 0.0f : juce::jlimit (0.0f, 400.0f, p ("tune_speed") + aiAmt * ai.get().tuneSpeedMs);
        const float humanize = robotic ? 0.0f : p ("tune_humanize") * 0.01f;
        const float flex = robotic ? 0.0f : p ("tune_flex") * 0.01f;
        const float vibCents = tuneOn ? p ("tune_vibrato") : 0.0f;
        const float formTarget = tuneOn ? std::exp2 (p ("tune_formant") / 12.0f) : 1.0f;
        const float transTarget = tuneOn ? p ("tune_transpose") : 0.0f;
        detector.setThreshold (0.05f + 0.20f * p ("tune_tracking") * 0.01f);

        const bool wantLow = on ("tune_lowlat");
        if (wantLow != lowLatActive)
        {
            lowLatActive = wantLow;
            psola.reset();
            for (auto& sh : shifter) sh.reset();
            updateLatency();
        }

        const float releaseCoef = 1.0f - std::exp (-1.0f / (0.030f * (float) sr));
        const float paramCoef = 1.0f - std::exp (-1.0f / (0.030f * (float) sr));
        float& voicedCoef = tuneCoef;     // persiste entre bloques (Humanize la va ajustando)
        if (voicedCoef <= 0.0f || heldNote < 0)
            voicedCoef = 1.0f - std::exp (-1.0f / (std::max (0.3f, baseSpeed) * 0.001f * (float) sr));
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
                    period = sr / hz;
                    const float midi = 69.0f + 12.0f * std::log2 (hz / 440.0f);
                    const int cand = Scales::nearest (midi, key, scale);
                    const int prevNote = heldNote;
                    if (! (heldNote >= 0 && Scales::allowed (heldNote, key, scale) && std::abs (midi - (float) heldNote) < 0.75f))
                        heldNote = cand;
                    holdTime = (heldNote == prevNote) ? holdTime + (float) secs : 0.0f;

                    float off = (float) heldNote - midi;
                    // Flex-Tune: deja natural lo que esta lejos de la nota; solo afina al acercarse
                    float w = 1.0f;
                    if (flex > 0.0f)
                    {
                        const float zone = 0.5f + (0.12f - 0.5f) * flex, a = std::abs (off);
                        w = a <= zone ? 1.0f : std::max (0.0f, 1.0f - (a - zone) / zone);
                    }
                    corrTarget = tuneOn ? off * amount * w : 0.0f;

                    // Humanize: las notas sostenidas se corrigen mas despacio (suena mas natural)
                    const float effSpeed = baseSpeed + humanize * 250.0f * juce::jlimit (0.0f, 1.0f, (holdTime - 0.15f) / 0.3f);
                    voicedCoef = 1.0f - std::exp (-1.0f / (std::max (0.3f, effSpeed) * 0.001f * (float) sr));

                    voicedNow = true;
                    ai.addPitchObservation (hz, -off * 100.0f, secs, listen);
                    analysis.addPitch (midi, hz, secs, detector.getConfidence());
                    if (meters.keyListening.load())
                    {
                        const int pc = ((int) std::round (midi) % 12 + 12) % 12;
                        meters.keyHist[pc].store (meters.keyHist[pc].load() + (float) secs * detector.getConfidence());
                    }
                    meters.pitchHz.store (hz);
                    meters.sungMidi.store (midi);
                    meters.targetNote.store ((float) heldNote);
                    meters.voiced.store (true);
                }
                else
                {
                    corrTarget = 0.0f;
                    heldNote = -1;
                    holdTime = 0.0f;
                    period = 0.0;
                    voicedNow = false;
                    meters.pitchHz.store (0.0f);
                    meters.targetNote.store (-1.0f);
                    meters.voiced.store (false);
                }
            }

            corrSemis += (corrTarget - corrSemis) * (voicedNow ? voicedCoef : releaseCoef);
            formantSm += (formTarget - formantSm) * paramCoef;
            transposeSm += (transTarget - transposeSm) * paramCoef;

            // Vibrato artificial en notas sostenidas (entra despues de 0.25 s)
            float vib = 0.0f;
            if (vibCents > 0.0f && voicedNow)
            {
                vibPhase += 5.5 / sr;
                if (vibPhase > 1.0) vibPhase -= 1.0;
                const float ramp = juce::jlimit (0.0f, 1.0f, (holdTime - 0.25f) / 0.4f);
                vib = vibCents * 0.01f * ramp * (float) std::sin (2.0 * juce::MathConstants<double>::pi * vibPhase);
            }

            const float ratio = std::exp2 ((corrSemis + transposeSm + vib) / 12.0f);
            if (lowLatActive)
            {
                L[i] = shifter[0].process (L[i], ratio);
                if (R) R[i] = shifter[1].process (R[i], ratio);
            }
            else
            {
                float r = R ? R[i] : 0.0f;
                psola.process (L[i], r, R != nullptr, voicedNow ? period : 0.0, ratio, formantSm);
                if (R) R[i] = r;
            }
        }
        meters.correctionCents.store (corrSemis * 100.0f);
    }

    // --- PREAMP (oversampling 2x; el fundido de bypass se hace dentro, a la frecuencia sobremuestreada)
    {
        preMixSm.setTargetValue (on ("pre_on") ? 1.0f : 0.0f);
        preamp.set ((int) p ("pre_type"), p ("pre_drive"), p ("pre_warmth") * 0.01f, p ("pre_trim"));

        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numCh, (size_t) n);
        auto up = oversampling.processSamplesUp (block);
        float peak = 0.0f;
        if (preMixSm.isSmoothing() || preMixSm.getCurrentValue() > 0.5f)
        {
            const size_t nc = up.getNumChannels();
            for (size_t i = 0; i < up.getNumSamples(); ++i)
            {
                const float g = preMixSm.getNextValue();
                for (size_t ch = 0; ch < nc; ++ch)
                {
                    float* d = up.getChannelPointer (ch);
                    const float x = d[i];
                    const float y = preamp.process (x, (int) ch);
                    d[i] = x + g * (y - x);
                    peak = std::max (peak, std::abs (d[i]));
                }
            }
        }
        meters.preHot.store (peak > 0.9f);
        oversampling.processSamplesDown (block);
    }

    // --- EQ
    {
        const bool ran = xfade (eqMixSm, on ("eq_on"), [&]
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
        });
        meters.eqHot.store (ran && buffer.getMagnitude (0, n) > 0.98f);
    }

    // --- COMPRESOR (knee, mix paralelo, filtro del detector y sidechain externo)
    {
        const auto& s = ai.get();
        const int cType = (int) p ("comp_type");
        const float cThr = p ("comp_thr") + aiAmt * s.compThrDb, cRatio = p ("comp_ratio") + aiAmt * s.compRatio;
        compView.set (cType, cThr, cRatio, p ("comp_att"), p ("comp_rel"), p ("comp_makeup"), p ("comp_knee"));

        const float hpf = p ("comp_sc_hpf");
        if (std::abs (hpf - scHpFreq) > 0.01f)
        {
            scHpFreq = hpf;
            for (auto& f : scHp) { if (hpf <= 20.5f) f.setIdentity(); else f.setHighPass (sr, hpf, 0.707); }
        }

        float maxGr = 0.0f, inPk = 0.0f, outPk = 0.0f;
        const bool ran = xfade (compMixSm, on ("comp_on"), [&]
        {
            comp.set (cType, cThr, cRatio, p ("comp_att"), p ("comp_rel"), p ("comp_makeup"), p ("comp_knee"));
            const float mix = p ("comp_mix") * 0.01f;
            for (int i = 0; i < n; ++i)
            {
                const float l = L[i], r = R ? R[i] : L[i];
                float dl = scExt ? scBuf.getSample (0, i) : l;
                float dr = scExt ? scBuf.getSample (1, i) : r;
                dl = scHp[0].process (dl);
                dr = scHp[1].process (dr);
                const float g = comp.computeGain (dl, dr);
                inPk = std::max (inPk, std::max (std::abs (l), std::abs (r)));
                const float cl = comp.colour (l * g), cr = comp.colour (r * g);
                L[i] = l + mix * (cl - l);
                if (R) R[i] = r + mix * (cr - r);
                outPk = std::max (outPk, std::max (std::abs (L[i]), R ? std::abs (R[i]) : 0.0f));
                maxGr = std::min (maxGr, comp.lastGr);
            }
        });
        if (! ran) comp.reset();
        meters.compGrDb.store (maxGr);
        meters.compInDb.store (ran ? gainToDb (inPk) : -100.0f);
        meters.compOutDb.store (ran ? gainToDb (outPk) : -100.0f);
    }

    // --- STEREO LAB: efecto (Wide/Chorus/Double/Shift/Dimension) + Width M/S, Modo Seguro, Mono y Mix
    if (R != nullptr)
    {
        const int mode = (int) p ("wide_mode");
        const bool safe = on ("wide_safe");
        const float amt = p ("wide_amount") * 0.01f * (safe ? 0.7f : 1.0f);
        const float width = std::min (p ("wide_width") * 0.01f, safe ? 1.3f : 2.0f);
        const bool mono = on ("wide_mono");
        const float labMix = p ("wide_mix") * 0.01f;
        // SAFE VOCAL corrige la fase solo: si la correlacion cae por debajo de +0.1, va cerrando el ancho
        // (hasta 5 %) y lo devuelve poco a poco cuando la fase vuelve a estar bien.
        if (safe && ! mono && on ("wide_on"))
        {
            const float corr = meters.labCorr.load();
            const float blockSec = (float) n / (float) getSampleRate();
            if (corr < 0.1f) phaseGuard = std::max (0.05f, phaseGuard * std::exp (-blockSec / 0.08f));
            else if (corr > 0.3f) phaseGuard = std::min (1.0f, phaseGuard + blockSec / 1.5f);
        }
        else phaseGuard = 1.0f;
        meters.phaseGuard.store (phaseGuard);
        const float sideGain = width * phaseGuard;
        if (! xfade (wideMixSm, on ("wide_on"), [&]
            {
                labDry.copyFrom (0, 0, buffer, 0, 0, n);
                labDry.copyFrom (1, 0, buffer, 1, 0, n);
                for (int i = 0; i < n; ++i)
                {
                    float l = L[i], r = R[i];
                    widener.process (l, r, mode, amt);
                    const float m = 0.5f * (l + r);
                    float sd = 0.5f * (l - r) * sideGain;
                    if (safe) sd = labSideHp[1].process (labSideHp[0].process (sd));   // graves siempre en mono (24 dB/oct)
                    if (mono) sd = 0.0f;
                    const float dl = labDry.getSample (0, i), dr = labDry.getSample (1, i);
                    L[i] = dl + labMix * ((m + sd) - dl);
                    R[i] = dr + labMix * ((m - sd) - dr);
                }
            }))
        {
            // Apagado: seguimos llenando sus lineas de retardo para que al encenderlo entre sin saltos
            for (int i = 0; i < n; ++i) { float l = L[i], r = R[i]; widener.process (l, r, mode, amt); }
        }

        double ms = 0.0, ss = 0.0, ll = 0.0, rr = 0.0, lr = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const double m = 0.5 * (L[i] + R[i]), sd = 0.5 * (L[i] - R[i]);
            ms += m * m; ss += sd * sd; ll += (double) L[i] * L[i]; rr += (double) R[i] * R[i]; lr += (double) L[i] * R[i];
        }
        wideMs = 0.9 * wideMs + 0.1 * ms; wideSs = 0.9 * wideSs + 0.1 * ss;
        meters.stereoWidth.store ((float) std::sqrt (wideSs / (wideMs + 1.0e-12)));
        if (ll + rr > 1.0e-8)
        {
            labLL = 0.85 * labLL + 0.15 * ll; labRR = 0.85 * labRR + 0.15 * rr; labLR = 0.85 * labLR + 0.15 * lr;
            meters.labCorr.store ((float) (labLR / std::sqrt (labLL * labRR + 1.0e-20)));
        }
    }

    // --- DELAY
    {
        const bool dOn = on ("dly_on");
        if (dOn && ! delayWasOn) delay.reset();
        delayWasOn = xfade (dlyMixSm, dOn, [&]
        {
            delay.set ((float) getDelayMs(), p ("dly_fb") * 0.01f, p ("dly_mix") * 0.01f, p ("dly_hicut"), on ("dly_pingpong"),
                       p ("dly_locut"), p ("dly_sat") * 0.01f, p ("dly_mod") * 0.01f, p ("dly_duck") * 0.01f, p ("dly_width") * 0.01f);
            for (int i = 0; i < n; ++i)
            {
                float l = L[i], r = R ? R[i] : L[i];
                delay.process (l, r);
                L[i] = R ? l : 0.5f * (l + r);
                if (R) R[i] = r;
            }
        });
    }

    // --- REVERB (FDN con 5 tipos)
    {
        const bool rOn = on ("rev_on");
        if (rOn && ! revActive) fdn.reset();
        revActive = xfade (revMixSm, rOn, [&]
        {
            fdn.set ((int) p ("rev_type"), p ("rev_predelay"), p ("rev_decay"), p ("rev_size") * 0.01f, p ("rev_damp") * 0.01f,
                     p ("rev_er") * 0.01f, p ("rev_diff") * 0.01f, p ("rev_locut"), p ("rev_hicut"), p ("rev_duck") * 0.01f,
                     p ("rev_width") * 0.013f);
            const float mix = p ("rev_mix") * 0.01f;
            for (int i = 0; i < n; ++i)
            {
                float wl, wr;
                fdn.process (L[i], R ? R[i] : L[i], wl, wr);
                if (R) { L[i] += wl * mix; R[i] += wr * mix; }
                else L[i] += 0.5f * (wl + wr) * mix;
            }
        });
    }

    // --- SALIDA: ganancia, dry/wet global, limitador, bypass global y comparar (A = original, B = procesado)
    outGainSm.setTargetValue (dbToGain (p ("out_gain")));
    for (int i = 0; i < n; ++i)
    {
        const float g = outGainSm.getNextValue();
        L[i] *= g;
        if (R) R[i] *= g;
    }

    // Volumen percibido de la senal procesada vs la original (para comparar al mismo volumen)
    {
        double sp = 0.0, sd = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const double pl = L[i], pr = R ? R[i] : L[i];
            const double dl = dryAligned.getSample (0, i), dr = dryAligned.getSample (1, i);
            sp += pl * pl + pr * pr; sd += dl * dl + dr * dr;
        }
        if (sd > 1.0e-7 * n)
        {
            const double a = 1.0 - std::exp (-(double) n / (1.5 * sr));
            rmsProc += a * (sp / n - rmsProc);
            rmsDry  += a * (sd / n - rmsDry);
        }
    }
    const float matchDb = juce::jlimit (-24.0f, 24.0f, (float) (10.0 * std::log10 ((rmsProc + 1.0e-12) / (rmsDry + 1.0e-12))));
    meters.matchGainDb.store (matchDb);

    const bool comparing = compareActive.load();
    const float dryGain = comparing ? dbToGain (matchDb) : 1.0f;
    const bool limitOn = on ("limit_on");
    globalMixSm.setTargetValue (p ("global_mix") * 0.01f);
    procSm.setTargetValue ((on ("global_bypass") || comparing) ? 0.0f : 1.0f);

    float outPeak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float dl = dryAligned.getSample (0, i), dr = dryAligned.getSample (1, i);
        const float gm = globalMixSm.getNextValue(), gp = procSm.getNextValue();
        float l = dl + gm * (L[i] - dl);
        float r = R ? dr + gm * (R[i] - dr) : 0.0f;

        if (limitOn)   // limitador de pico, techo -0.3 dBFS
        {
            const float pk = std::max (std::abs (l), R ? std::abs (r) : 0.0f);
            const float target = pk > 0.966f ? 0.966f / pk : 1.0f;
            limGain = std::min (target, limGain + (1.0f - limGain) * limRelCoef);
            l *= limGain; r *= limGain;
        }
        else limGain = 1.0f;

        l = dl * dryGain + gp * (l - dl * dryGain);
        r = dr * dryGain + gp * (r - dr * dryGain);
        outPeak = std::max (outPeak, std::max (std::abs (l), R ? std::abs (r) : 0.0f));
        L[i] = safetyClip (l);
        if (R) R[i] = safetyClip (r);
    }
    meters.outClip.store (outPeak > 0.999f);
    meters.outLevelDb.store (gainToDb (buffer.getMagnitude (0, n)));

    // Medidores profesionales: LUFS, True Peak, RMS y correlacion de la salida
    if (meters.resetMeters.exchange (false)) loud.reset();
    loud.process (L, R, n);
    meters.lufsM.store (loud.getMomentary());
    meters.lufsI.store (loud.getIntegrated());
    meters.truePeakDb.store (loud.getTruePeakDb());
    meters.rmsDb.store (loud.getRmsDb());
    meters.peakHoldDb.store (loud.getPeakHoldDb());
    if (R != nullptr)
    {
        double ll = 0.0, rr = 0.0, lr = 0.0;
        for (int i = 0; i < n; ++i) { ll += (double) L[i] * L[i]; rr += (double) R[i] * R[i]; lr += (double) L[i] * R[i]; }
        if (ll + rr > 1.0e-8)
        {
            outLL = 0.85 * outLL + 0.15 * ll; outRR = 0.85 * outRR + 0.15 * rr; outLR = 0.85 * outLR + 0.15 * lr;
            meters.outCorr.store ((float) (outLR / std::sqrt (outLL * outRR + 1.0e-20)));
        }
    }

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
void TopVocalChainProcessor::switchToSlot (int slot)
{
    slot = juce::jlimit (0, 1, slot);
    if (slot == currentSlot) return;
    slots[currentSlot] = apvts.copyState();
    currentSlot = slot;
    if (slots[slot].isValid()) apvts.replaceState (slots[slot].createCopy());
    else slots[slot] = apvts.copyState();          // la primera vez B empieza igual que A
    requestAIReset();
}

void TopVocalChainProcessor::copyToOtherSlot()
{
    slots[1 - currentSlot] = apvts.copyState();
}

void TopVocalChainProcessor::resetAll()
{
    setCurrentProgram (0);
}

void TopVocalChainProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("slot", currentSlot, nullptr);
    state.setProperty ("hidden", hiddenModules.load(), nullptr);
    state.setProperty ("uiScale", uiScaleIndex.load(), nullptr);
    for (int k = 0; k < 2; ++k)
        if (slots[k].isValid() && k != currentSlot)
        {
            juce::ValueTree child ("SLOT");
            child.setProperty ("index", k, nullptr);
            child.appendChild (slots[k].createCopy(), nullptr);
            state.appendChild (child, nullptr);
        }
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
            currentSlot = juce::jlimit (0, 1, (int) tree.getProperty ("slot", 0));
            hiddenModules.store ((int) tree.getProperty ("hidden", 0));
            uiScaleIndex.store ((int) tree.getProperty ("uiScale", 0));
            slots[0] = slots[1] = juce::ValueTree();
            for (int k = tree.getNumChildren(); --k >= 0;)
            {
                auto child = tree.getChild (k);
                if (child.hasType ("SLOT"))
                {
                    const int idx = juce::jlimit (0, 1, (int) child.getProperty ("index", 0));
                    if (child.getNumChildren() > 0) slots[idx] = child.getChild (0).createCopy();
                    tree.removeChild (k, nullptr);
                }
            }
            apvts.replaceState (tree);
            undoManager.clearUndoHistory();
        }
    }
}

float TopVocalChainProcessor::getEqMagnitudeDb (double f) const
{
    if (! on ("eq_on")) return 0.0f;
    const double w = 2.0 * juce::MathConstants<double>::pi * f / sr;
    const std::complex<double> z1 = std::polar (1.0, -w), z2 = std::polar (1.0, -2.0 * w);
    double db = 0.0;
    for (int b = 0; b < numBands; ++b)
    {
        const auto& q = eq[b][0];
        const auto num = (double) q.b0 + (double) q.b1 * z1 + (double) q.b2 * z2;
        const auto den = 1.0 + (double) q.a1 * z1 + (double) q.a2 * z2;
        db += 20.0 * std::log10 (std::max (1.0e-9, std::abs (num / den)));
    }
    return (float) db;
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
