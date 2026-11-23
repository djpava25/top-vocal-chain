#include "TopStrip3Processor.h"
#include "TopStrip3Editor.h"
#include "Strip3Scales.h"

//==============================================================================
const juce::StringArray& TopStrip3Processor::bassFreqs()    { static const juce::StringArray s { "15 Hz", "30 Hz", "60 Hz", "150 Hz" }; return s; }
const juce::StringArray& TopStrip3Processor::lowMidFreqs()  { static const juce::StringArray s { "35 Hz", "40 Hz", "50 Hz", "75 Hz", "100 Hz", "150 Hz", "200 Hz", "250 Hz", "300 Hz", "350 Hz", "450 Hz" }; return s; }
const juce::StringArray& TopStrip3Processor::highMidFreqs() { static const juce::StringArray s { "220 Hz", "250 Hz", "350 Hz", "550 Hz", "1 kHz", "1.2 kHz", "1.4 kHz", "1.6 kHz", "2 kHz", "2.5 kHz", "2.8 kHz" }; return s; }
const juce::StringArray& TopStrip3Processor::trebleFreqs()  { static const juce::StringArray s { "10 kHz", "15 kHz", "20 kHz", "32 kHz" }; return s; }

float TopStrip3Processor::choiceHz (const juce::StringArray& list, int i)
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
        // umbral relativo a 0 VU (-18 dBFS); ataque y release en ms, como en el panel
        static const std::vector<Preset> p {
            { "Default", {} },
            { "Voz Calida", { { "pre_gain", 4 }, { "comp_on", 1 }, { "thr", -2 }, { "ratio", 3 }, { "att", 20 }, { "rel", 600 }, { "eq_on", 1 },
                              { "bass_gain", 3 }, { "bass_freq", 1 }, { "lm_gain", -2 }, { "lm_freq", 7 }, { "hm_gain", 2 }, { "hm_freq", 8 },
                              { "tr_gain", 4 }, { "tr_freq", 1 }, { "out", 6 } } },
            { "Voz Rap Presencia", { { "pre_gain", 6 }, { "comp_on", 1 }, { "thr", -3 }, { "ratio", 4 }, { "att", 8 }, { "rel", 350 }, { "out", 7 },
                                     { "eq_on", 1 }, { "hpf_on", 1 }, { "hpf", 100 }, { "lm_gain", -4 }, { "lm_freq", 8 },
                                     { "hm_gain", 4 }, { "hm_freq", 9 }, { "tr_gain", 5 }, { "tr_freq", 1 } } },
            { "Voz Aire (Pop)", { { "comp_on", 1 }, { "thr", 0 }, { "ratio", 2.5f }, { "att", 25 }, { "rel", 800 }, { "eq_on", 1 },
                                  { "hpf_on", 1 }, { "hpf", 80 }, { "tr_gain", 8 }, { "tr_freq", 3 }, { "hm_gain", 1.5f }, { "hm_freq", 7 }, { "out", 4 } } },
            { "De-Esser (SC Mids)", { { "comp_on", 1 }, { "sc_link", 1 }, { "eq_on", 1 }, { "thr", 6 }, { "ratio", 6 }, { "att", 2 }, { "rel", 150 },
                                      { "hm_gain", 12 }, { "hm_freq", 3 }, { "hm_x10", 1 }, { "hm_hiq", 1 } } },
            { "Bass DI", { { "input", 1 }, { "pre_gain", 6 }, { "comp_on", 1 }, { "thr", -4 }, { "ratio", 4 }, { "att", 15 }, { "rel", 500 }, { "out", 3 },
                           { "eq_on", 1 }, { "bass_gain", 6 }, { "bass_freq", 2 }, { "lm_gain", -3 }, { "lm_freq", 7 }, { "hm_gain", 3 }, { "hm_freq", 4 } } },
            { "Guitarra Acustica", { { "hpf_on", 1 }, { "hpf", 80 }, { "comp_on", 1 }, { "thr", 2 }, { "ratio", 2 }, { "att", 30 }, { "rel", 700 },
                                     { "eq_on", 1 }, { "lm_gain", -3 }, { "lm_freq", 8 }, { "tr_gain", 5 }, { "tr_freq", 1 }, { "out", 4 } } },
            { "Color de Valvula", { { "input", 2 }, { "pre_gain", 9 }, { "out", -9 } } },
            { "Valvula Caliente (High Gain)", { { "input", 2 }, { "high_gain", 1 }, { "pre_gain", -10 }, { "out", -3 }, { "comp_on", 1 },
                                                { "thr", 0 }, { "ratio", 3 }, { "att", 20 }, { "rel", 600 } } },
            { "Bombo Peso Sub", { { "comp_on", 1 }, { "thr", -2 }, { "ratio", 4 }, { "att", 30 }, { "rel", 300 }, { "eq_on", 1 },
                                  { "bass_gain", 8 }, { "bass_freq", 0 }, { "lm_gain", -5 }, { "lm_freq", 9 }, { "hm_gain", 4 }, { "hm_freq", 4 }, { "out", 5 } } },
        };
        return p;
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout TopStrip3Processor::createLayout()
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
    namespace S = s737cscale;

    addB ("power", "Power", true);
    // Preamp (escala LINE del panel: -28 .. 0 arriba .. +9)
    addF ("pre_gain", "Preamp Gain", S::range (S::preGain(), 0.1f), 0.0f, "dB", 1);
    addC ("input", "Input", { "LINE", "INSTRUMENT", "MIC" }, 0);
    addB ("high_gain", "High Gain", false);
    addB ("phase", "Phase Rev", false);
    addB ("p48", "+48v", false);
    addB ("hpf_on", "Filter In", false);
    addF ("hpf", "High Pass Freq", S::range (S::hpf(), 1.0f), 60.0f, "Hz", 0);
    // Compresor optico
    addB ("comp_on", "Compressor In", false);
    addF ("thr", "Threshold", S::range (S::thr(), 0.1f), 0.0f, "dB", 1);
    addF ("ratio", "Compression Ratio", S::range (S::ratio(), 0.1f), 3.0f, ":1", 1);
    addF ("att", "Attack", S::range (S::attack(), 0.1f), 20.0f, "ms", 1);
    addF ("rel", "Release", S::range (S::release(), 1.0f), 500.0f, "ms", 0);
    addB ("eq_pre", "EQ > Comp", false);
    addB ("sc_link", "SC > Mids", false);
    addB ("meter_gr", "Meter GR", false);
    // EQ
    addB ("eq_on", "Equalizer In", false);
    addF ("bass_gain", "Bass", S::range (S::bass(), 0.1f), 0.0f, "dB", 1);
    addC ("bass_freq", "Bass Shelf", bassFreqs(), 2);
    addF ("lm_gain", "Low Mid", S::range (S::mids(), 0.1f), 0.0f, "dB", 1);
    addC ("lm_freq", "Low Mid Freq", lowMidFreqs(), 5);
    addB ("lm_hiq", "Low Mid Hi-Q", false);
    addB ("lm_x10", "Low Mid x10", false);
    addF ("hm_gain", "High Mid", S::range (S::mids(), 0.1f), 0.0f, "dB", 1);
    addC ("hm_freq", "High Mid Freq", highMidFreqs(), 8);
    addB ("hm_hiq", "High Mid Hi-Q", false);
    addB ("hm_x10", "High Mid x10", false);
    addF ("tr_gain", "Treble", S::range (S::treble(), 0.1f), 0.0f, "dB", 1);
    addC ("tr_freq", "Treble Shelf", trebleFreqs(), 1);
    // Salida
    addF ("out", "Output", S::range (S::out(), 0.1f), 0.0f, "dB", 1);
    addF ("mix", "Mix", { 0.0f, 100.0f, 1.0f }, 100.0f, "%", 0);              // V2: mezcla en paralelo (compresion de Nueva York)
    return { ps.begin(), ps.end() };
}

TopStrip3Processor::TopStrip3Processor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "TopStrip737V3", createLayout())
{
    using OS = juce::dsp::Oversampling<float>;
    os.clearOversamplingStages();
    os.addOversamplingStage (OS::filterHalfBandFIREquiripple, 0.06f, -110.0f, 0.06f, -110.0f);
    os.addOversamplingStage (OS::filterHalfBandFIREquiripple, 0.20f, -100.0f, 0.20f, -100.0f);
    os.addOversamplingStage (OS::filterHalfBandFIREquiripple, 0.30f, -100.0f, 0.30f, -100.0f);
    os.setUsingIntegerLatency (true);
    for (int m = 0; m < 3; ++m) for (int h = 0; h < 2; ++h) preTab[m][h].build (s737c::preChar (m, h == 1));
    compTab.build (s737c::compChar()); outTab.build (s737c::outChar());
}

bool TopStrip3Processor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet(), in = l.getMainInputChannelSet();
    const bool okOut = out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
    return okOut && in == out;
}

TopStrip3Processor::EqState TopStrip3Processor::targetEq() const
{
    EqState s;
    s.bass = p ("bass_gain"); s.lm = p ("lm_gain"); s.hm = p ("hm_gain"); s.tr = p ("tr_gain"); s.hpf = p ("hpf");
    s.bf = (int) p ("bass_freq"); s.lf = (int) p ("lm_freq"); s.hf = (int) p ("hm_freq"); s.tf = (int) p ("tr_freq");
    s.lq = on ("lm_hiq"); s.lx = on ("lm_x10"); s.hq = on ("hm_hiq"); s.hx = on ("hm_x10");
    return s;
}

void TopStrip3Processor::design (const EqState& s)
{
    const double fs = osRate;
    const double lmF = choiceHz (lowMidFreqs(), s.lf) * (s.lx ? 10.0 : 1.0);
    const double hmF = choiceHz (highMidFreqs(), s.hf) * (s.hx ? 10.0 : 1.0);
    for (auto& c : ch)
    {
        s737c::setHpf (c.hpf, fs, s.hpf);
        s737c::setLowShelf (c.bass, fs, choiceHz (bassFreqs(), s.bf), s.bass);
        s737c::setHighShelf (c.treble, fs, choiceHz (trebleFreqs(), s.tf), s.tr);
        s737c::setBell (c.lm, fs, lmF, s.lq ? 0.85 : 0.2, s.lm);
        s737c::setBell (c.hm, fs, hmF, s.hq ? 0.85 : 0.2, s.hm);
        // copia para el sidechain (SC > MIDS)
        c.scLm.b0 = c.lm.b0; c.scLm.b1 = c.lm.b1; c.scLm.b2 = c.lm.b2; c.scLm.a1 = c.lm.a1; c.scLm.a2 = c.lm.a2;
        c.scHm.b0 = c.hm.b0; c.scHm.b1 = c.hm.b1; c.scHm.b2 = c.hm.b2; c.scHm.a1 = c.hm.a1; c.scHm.a2 = c.hm.a2;
    }
    cur = s;
}

double TopStrip3Processor::eqResponseDb (double hz) const
{
    const auto s = targetEq();
    const std::complex<double> jw (0.0, 2 * s737c::pi * hz);
    std::complex<double> h (1.0, 0.0);
    if (on ("hpf_on")) h *= s737c::hpfS (jw, s.hpf);
    if (on ("eq_on"))
    {
        h *= s737c::lowShelfS (jw, choiceHz (bassFreqs(), s.bf), s.bass);
        h *= s737c::highShelfS (jw, choiceHz (trebleFreqs(), s.tf), s.tr);
        if (! on ("sc_link"))
        {
            h *= s737c::bellS (jw, choiceHz (lowMidFreqs(), s.lf) * (s.lx ? 10.0 : 1.0), s.lq ? 0.85 : 0.2, s.lm);
            h *= s737c::bellS (jw, choiceHz (highMidFreqs(), s.hf) * (s.hx ? 10.0 : 1.0), s.hq ? 0.85 : 0.2, s.hm);
        }
    }
    return 20.0 * std::log10 (std::abs (h) + 1e-12);
}

void TopStrip3Processor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    maxBlock = std::max (32, samplesPerBlock);
    os.reset();
    os.initProcessing ((size_t) maxBlock);
    osRate = sr * (double) os.getOversamplingFactor();
    latency = (int) std::round (os.getLatencyInSamples());
    setLatencySamples (latency);
    for (auto& r : dryRing) r.assign ((size_t) latency + 1, 0.0f);
    dryPos = 0;
    dryBuf.setSize (2, maxBlock);

    for (auto& c : ch)
    {
        c.inT.prepare (osRate, 20.0, 0.8);
        c.pre.set (&preTab[0][0]); c.comp.set (&compTab); c.out.set (&outTab);
        c.pre.prepare (osRate); c.comp.prepare (osRate); c.out.prepare (osRate);
        c.adaaEq.reset();
        for (auto* f : { &c.hpf, &c.bass, &c.treble }) f->reset();
        for (auto* f : { &c.lm, &c.hm, &c.scLm, &c.scHm }) f->reset();
    }
    lastMode = -1;
    opto.prepare (osRate);
    inPeak = outPeak = 0.0f;

    const auto t = targetEq();
    const float vals[5] { t.bass, t.lm, t.hm, t.tr, t.hpf };
    for (int k = 0; k < 5; ++k) { eqSm[k].reset (sr, 0.04); eqSm[k].setCurrentAndTargetValue (vals[k]); }
    design (t);

    preSm.reset (osRate, 0.03); preSm.setCurrentAndTargetValue (1.0f);
    mixSm.reset (osRate, 0.03); mixSm.setCurrentAndTargetValue (p ("mix") * 0.01f);
    for (auto& c : ch) { c.inLp.setIdentity(); c.inHp.setIdentity(); c.inLp.reset(); c.inHp.reset(); std::fill (std::begin (c.dry), std::end (c.dry), 0.0); }
    dryPosOs = 0;
    outSm.reset (sr, 0.03);   outSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (p ("out")));
    powerSm.reset (sr, 0.02); powerSm.setCurrentAndTargetValue (on ("power") ? 1.0f : 0.0f);
}

void TopStrip3Processor::abSelect (int newSlot)
{
    newSlot = juce::jlimit (0, 1, newSlot);
    if (newSlot == slot) return;
    abState[slot] = apvts.copyState();
    if (abState[newSlot].isValid()) apvts.replaceState (abState[newSlot].createCopy());
    slot = newSlot;
}
void TopStrip3Processor::abCopyToOther() { abState[1 - slot] = apvts.copyState(); }

void TopStrip3Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const auto t0 = juce::Time::getHighResolutionTicks();
    struct LoadMeter
    {
        TopStrip3Processor& p; juce::int64 t0; int n; double sr;
        ~LoadMeter()
        {
            const double used = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
            const double avail = n > 0 ? (double) n / sr : 1.0;
            p.loadSm += (used / avail - p.loadSm) * 0.05;
            p.dspLoad.store ((float) p.loadSm);
        }
    } loadMeter { *this, t0, buffer.getNumSamples(), sr };
    const int n = buffer.getNumSamples();
    const int chans = std::min (2, buffer.getNumChannels());
    for (int c = chans; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);
    if (n == 0 || chans == 0) return;
    if (n > maxBlock) { prepareToPlay (sr, n); }

    // copia "dry" para el POWER sin clicks
    dryBuf.setSize (2, n, false, false, true);
    for (int c = 0; c < chans; ++c) dryBuf.copyFrom (c, 0, buffer, c, 0, n);

    float pk = 0.0f;
    for (int c = 0; c < chans; ++c) pk = std::max (pk, buffer.getMagnitude (c, 0, n));
    inPeak = std::max (pk, inPeak * 0.9f);
    inLevelDb.store (juce::Decibels::gainToDecibels (inPeak, -100.0f));

    // ---- parametros
    const int mode = juce::jlimit (0, 2, (int) p ("input"));
    const bool highGain = on ("high_gain");
    if (mode != lastMode || highGain != lastHigh)
    {
        const auto vo = s737c::inputVoicing (mode, highGain);
        for (auto& c : ch)
        {
            c.pre.set (&preTab[mode][highGain ? 1 : 0]);
            if (vo.lp > 0) s737c::setLpf (c.inLp, osRate, vo.lp); else c.inLp.setIdentity();
            if (vo.hp > 0) s737c::setHpf (c.inHp, osRate, vo.hp); else c.inHp.setIdentity();
        }
        lastMode = mode; lastHigh = highGain;
    }
    const float hgDb = highGain ? (mode == 0 ? 8.0f : 18.0f) : 0.0f;              // manual: +8 dB LINE, +18 dB MIC / INSTRUMENT
    preSm.setTargetValue (juce::Decibels::decibelsToGain (p ("pre_gain") + hgDb));
    mixSm.setTargetValue (p ("mix") * 0.01f);
    const double phaseSign = on ("phase") ? -1.0 : 1.0;
    const bool hpfOn = on ("hpf_on"), compOn = on ("comp_on"), eqOn = on ("eq_on"), eqPre = on ("eq_pre");
    const bool scMids = eqOn && on ("sc_link");
    opto.set (p ("thr"), p ("ratio"), p ("att"), p ("rel"));

    auto tgt = targetEq();
    const float tv[5] { tgt.bass, tgt.lm, tgt.hm, tgt.tr, tgt.hpf };
    for (int k = 0; k < 5; ++k) eqSm[k].setTargetValue (tv[k]);

    // ---- circuito a 8x
    double maxGr = 0.0;
    {
        juce::dsp::AudioBlock<float> blk (buffer.getArrayOfWritePointers(), (size_t) chans, (size_t) n);
        auto up = os.processSamplesUp (blk);
        const int factor = (int) os.getOversamplingFactor();
        const int N = (int) up.getNumSamples();
        constexpr int hostSteps = 16;
        float* d[2] { up.getChannelPointer (0), chans > 1 ? up.getChannelPointer (1) : nullptr };
        const double adaaA = (compOn ? 3.0 : 2.0) / 8.0;                    // etapas con ADAA en el camino

        auto eqAudio = [scMids] (Channel& c, double v)
        {
            v = c.bass.process (v);
            if (! scMids) { v = c.lm.process (v); v = c.hm.process (v); }
            return c.treble.process (v);
        };

        for (int i = 0; i < N; ++i)
        {
            if (i % (hostSteps * factor) == 0)
            {
                // perillas suavizadas: el circuito se recalcula cada 16 muestras del host mientras se mueven
                auto s = tgt;
                for (int k = 0; k < 5; ++k) eqSm[k].skip (hostSteps);
                s.bass = eqSm[0].getCurrentValue(); s.lm = eqSm[1].getCurrentValue(); s.hm = eqSm[2].getCurrentValue();
                s.tr = eqSm[3].getCurrentValue(); s.hpf = eqSm[4].getCurrentValue();
                if (! designed || ! (s == cur)) { design (s); designed = true; }
            }

            double v[2] {}, sc[2] {}, raw[2] {};
            const double gPre = (double) preSm.getNextValue() * phaseSign, mix = (double) mixSm.getNextValue();
            for (int c = 0; c < chans; ++c)
            {
                auto& C = ch[c];
                raw[c] = (double) d[c][i] * phaseSign;
                double x = (double) d[c][i] * gPre * s737c::inScale;
                x = C.inHp.process (C.inLp.process (x));                // respuesta medida de la entrada
                if (mode == 2) x = C.inT.process (x);                   // trafo de entrada (MIC)
                x = C.pre.process (x);                                   // 6922 x2
                if (hpfOn) x = C.hpf.process (x);
                if (eqOn && eqPre) x = eqAudio (C, x);
                v[c] = x;
                sc[c] = scMids && compOn ? C.scHm.process (C.scLm.process (x)) : x;
            }
            if (compOn)
            {
                const double s = chans > 1 && std::abs (sc[1]) > std::abs (sc[0]) ? sc[1] : sc[0];   // sidechain enlazado
                const double gr = opto.process (s);
                maxGr = std::max (maxGr, gr);
                const double g = std::exp (-gr * 0.11512925464970228);       // 10^(-gr/20)
                for (int c = 0; c < chans; ++c) v[c] = ch[c].comp.process (v[c] * g);
            }
            for (int c = 0; c < chans; ++c)
            {
                double x = v[c];
                if (eqOn && ! eqPre) x = eqAudio (ch[c], x);
                x = ch[c].out.process (x);
                x = ch[c].adaaEq.process (x, adaaA);
                // MIX: la senal directa se retrasa lo mismo que el circuito (2 o 2.5 muestras a 8x) y se mezcla sin filtro de peine
                auto& C = ch[c];
                C.dry[dryPosOs] = raw[c];
                const double dDry = compOn ? 0.5 * (C.dry[(dryPosOs + 6) & 7] + C.dry[(dryPosOs + 5) & 7]) : C.dry[(dryPosOs + 6) & 7];
                const double wet = x / s737c::inScale;
                d[c][i] = (float) (dDry + (wet - dDry) * mix);
            }
            dryPosOs = (dryPosOs + 1) & 7;
        }
        if (! compOn) opto.reset();
        os.processSamplesDown (blk);
    }
    grDb.store ((float) -maxGr);

    // ---- salida + POWER (bypass alineado con la latencia)
    outSm.setTargetValue (juce::Decibels::decibelsToGain (p ("out")));
    powerSm.setTargetValue (on ("power") ? 1.0f : 0.0f);
    const int L = latency + 1;
    double ms = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const float og = outSm.getNextValue(), pw = powerSm.getNextValue();
        const int rd = (dryPos + 1) % L;
        for (int c = 0; c < chans; ++c)
        {
            auto& ring = dryRing[c];
            ring[(size_t) dryPos] = dryBuf.getSample (c, i);
            const float dry = ring[(size_t) rd];
            const float wet = buffer.getSample (c, i) * og;
            const float y = dry + (wet - dry) * pw;
            buffer.setSample (c, i, y);
            ms += (double) y * y;
        }
        dryPos = rd;
    }
    if (chans == 1 && buffer.getNumChannels() > 1) buffer.copyFrom (1, 0, buffer, 0, 0, n);

    pk = 0.0f;
    for (int c = 0; c < chans; ++c) pk = std::max (pk, buffer.getMagnitude (c, 0, n));
    outPeak = std::max (pk, outPeak * 0.9f);
    outLevelDb.store (juce::Decibels::gainToDecibels (outPeak, -100.0f));
    vuDb.store ((float) (10.0 * std::log10 (2.0 * ms / (double) (n * chans) + 1e-12)));   // nivel RMS de seno en dBFS
}

//==============================================================================
int TopStrip3Processor::getNumPrograms() { return (int) stripPresets().size(); }

const juce::String TopStrip3Processor::getProgramName (int i)
{
    return juce::isPositiveAndBelow (i, (int) stripPresets().size()) ? juce::String (stripPresets()[(size_t) i].name) : juce::String();
}

void TopStrip3Processor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) stripPresets().size())) return;
    currentProgram = index;
    undoManager.beginNewTransaction ("Preset");
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
            rp->setValueNotifyingHost (rp->getDefaultValue());
    for (const auto& [id, val] : stripPresets()[(size_t) index].v)
        if (auto* rp = apvts.getParameter (id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (val));
}

void TopStrip3Processor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void TopStrip3Processor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = (int) tree.getProperty ("program", 0);
            apvts.replaceState (tree);
        }
}

juce::AudioProcessorEditor* TopStrip3Processor::createEditor() { return new TopStrip3Editor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TopStrip3Processor(); }
