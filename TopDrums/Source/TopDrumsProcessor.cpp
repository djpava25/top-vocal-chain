#include "TopDrumsProcessor.h"
#include "TopDrumsEditor.h"

using tvc::dbToGain;
using tvc::gainToDb;

//==============================================================================
const juce::StringArray& TopDrumsProcessor::focusNames()   { static const juce::StringArray s { "Full", "Kick", "Snare", "Hats" }; return s; }
const juce::StringArray& TopDrumsProcessor::ratioNames()   { static const juce::StringArray s { "1.5", "2", "4", "10" }; return s; }
const juce::StringArray& TopDrumsProcessor::attackNames()  { static const juce::StringArray s { "0.1", "0.3", "1", "3", "10", "30" }; return s; }
const juce::StringArray& TopDrumsProcessor::releaseNames() { static const juce::StringArray s { "0.1", "0.3", "0.6", "1.2", "Auto" }; return s; }
const juce::StringArray& TopDrumsProcessor::scNames()      { static const juce::StringArray s { "Off", "60 Hz", "90 Hz", "150 Hz" }; return s; }

namespace
{
    // Perfil objetivo de cada genero para la IA local:
    // crest = pegada (pico/RMS), low = peso de graves (<120 Hz vs total), high = brillo (>6 kHz vs total)
    struct Genre { const char* name; float crest, low, high; const char* character; };
    const std::vector<Genre>& genres()
    {
        static const std::vector<Genre> g {
            { "General",            13.0f,  -7.0f, -16.0f, "equilibrada" },
            { "Bad Bunny Heavy",    10.0f,  -3.5f, -18.0f, "pesada, con mucho sub y bien pegada" },
            { "Reggaeton Moderno",  11.0f,  -4.5f, -16.0f, "moderna, con kick gordo y snare brillante" },
            { "Reggaeton Clasico",  12.5f,  -6.0f, -16.0f, "clasica, con el dembow claro y menos sub" },
            { "Dembow",             10.5f,  -5.0f, -14.0f, "agresiva, rapida y con mucho golpe" },
            { "Trap Latino",        11.0f,  -3.0f, -15.0f, "con 808 dominante y hats definidos" },
            { "Corridos Tumbados",  13.5f,  -7.0f, -15.0f, "natural, con kick redondo y aire" },
            { "Bachata",            15.0f, -10.0f, -13.0f, "viva, con guira y bongos al frente" },
            { "Salsa",              16.0f, -11.0f, -13.0f, "natural y abierta, percusion en vivo" },
            { "Merengue",           14.0f,  -9.0f, -13.0f, "con tambora punzante y guira brillante" },
            { "Cumbia",             14.0f,  -8.0f, -14.0f, "calida y con groove" },
            { "Drill",              11.0f,  -4.0f, -14.0f, "dura, con 808 deslizante y snare seco" },
            { "Afrobeats",          13.0f,  -6.0f, -15.0f, "suave y con rebote" },
            { "Pop",                12.0f,  -6.5f, -14.0f, "limpia, controlada y brillante" },
            { "Hip-Hop Boom Bap",   12.0f,  -5.5f, -18.0f, "gorda, saturada y algo oscura" },
            { "Rock",               13.0f,  -7.5f, -14.0f, "con mucho cuerpo y sala" },
            { "Lo-Fi",              10.0f,  -6.0f, -22.0f, "oscura, comprimida y con textura" },
        };
        return g;
    }

    struct Preset { const char* name; int genre; std::vector<std::pair<const char*, float>> v; };
    const std::vector<Preset>& drumPresets()
    {
        static const std::vector<Preset> p {
            { "Default", 0, {} },
            { "Bad Bunny Heavy", 1, { { "ts_attack", 35 }, { "ts_sustain", 10 }, { "weight", 65 }, { "drive", 35 }, { "tone", -10 }, { "glue_on", 1 },
                                      { "glue_thr", -22 }, { "glue_ratio", 2 }, { "glue_att", 5 }, { "glue_rel", 4 }, { "glue_makeup", 5 }, { "glue_mix", 85 },
                                      { "glue_sc", 2 }, { "clip", 1 } } },
            { "Reggaeton Moderno", 2, { { "ts_attack", 30 }, { "ts_sustain", -5 }, { "weight", 50 }, { "drive", 25 }, { "tone", 10 }, { "glue_on", 1 },
                                        { "glue_thr", -18 }, { "glue_ratio", 2 }, { "glue_att", 4 }, { "glue_rel", 1 }, { "glue_makeup", 4 }, { "glue_mix", 80 },
                                        { "glue_sc", 2 }, { "clip", 1 } } },
            { "Reggaeton Clasico", 3, { { "ts_attack", 20 }, { "weight", 35 }, { "drive", 20 }, { "tone", 5 }, { "glue_on", 1 }, { "glue_thr", -14 },
                                        { "glue_ratio", 1 }, { "glue_att", 4 }, { "glue_rel", 2 }, { "glue_makeup", 2 }, { "glue_mix", 70 } } },
            { "Dembow", 4, { { "ts_attack", 45 }, { "ts_sustain", -20 }, { "weight", 45 }, { "drive", 40 }, { "tone", 20 }, { "glue_on", 1 }, { "glue_thr", -20 },
                             { "glue_ratio", 2 }, { "glue_att", 3 }, { "glue_rel", 0 }, { "glue_makeup", 5 }, { "glue_mix", 80 }, { "glue_sc", 1 }, { "clip", 1 } } },
            { "Trap Latino", 5, { { "ts_attack", 30 }, { "ts_sustain", 15 }, { "ts_focus", 1 }, { "weight", 70 }, { "drive", 30 }, { "tone", 5 }, { "glue_on", 1 },
                                  { "glue_thr", -16 }, { "glue_ratio", 2 }, { "glue_att", 5 }, { "glue_rel", 4 }, { "glue_makeup", 4 }, { "glue_mix", 75 },
                                  { "glue_sc", 3 }, { "clip", 1 } } },
            { "Corridos Tumbados", 6, { { "ts_attack", 15 }, { "ts_sustain", 10 }, { "weight", 25 }, { "drive", 15 }, { "tone", 10 }, { "glue_on", 1 },
                                        { "glue_thr", -12 }, { "glue_ratio", 1 }, { "glue_att", 5 }, { "glue_rel", 2 }, { "glue_makeup", 2 }, { "glue_mix", 60 },
                                        { "glue_sc", 2 } } },
            { "Bachata", 7, { { "ts_attack", 25 }, { "ts_sustain", -10 }, { "weight", 15 }, { "drive", 10 }, { "tone", 20 }, { "glue_on", 1 }, { "glue_thr", -10 },
                              { "glue_ratio", 1 }, { "glue_att", 4 }, { "glue_rel", 1 }, { "glue_makeup", 1 }, { "glue_mix", 50 } } },
            { "Salsa", 8, { { "ts_attack", 30 }, { "weight", 10 }, { "drive", 10 }, { "tone", 15 }, { "glue_on", 1 }, { "glue_thr", -10 }, { "glue_ratio", 1 },
                            { "glue_att", 4 }, { "glue_rel", 1 }, { "glue_makeup", 1 }, { "glue_mix", 50 } } },
            { "Merengue", 9, { { "ts_attack", 40 }, { "ts_sustain", -15 }, { "weight", 20 }, { "drive", 15 }, { "tone", 15 }, { "glue_on", 1 }, { "glue_thr", -12 },
                               { "glue_ratio", 2 }, { "glue_att", 3 }, { "glue_rel", 0 }, { "glue_makeup", 2 }, { "glue_mix", 60 } } },
            { "Cumbia", 10, { { "ts_attack", 20 }, { "ts_sustain", 5 }, { "weight", 20 }, { "drive", 15 }, { "tone", 10 }, { "glue_on", 1 }, { "glue_thr", -12 },
                              { "glue_ratio", 1 }, { "glue_att", 4 }, { "glue_rel", 2 }, { "glue_makeup", 2 }, { "glue_mix", 60 } } },
            { "Drill", 11, { { "ts_attack", 35 }, { "ts_sustain", -10 }, { "ts_focus", 2 }, { "weight", 55 }, { "drive", 30 }, { "tone", 15 }, { "glue_on", 1 },
                             { "glue_thr", -18 }, { "glue_ratio", 2 }, { "glue_att", 3 }, { "glue_rel", 1 }, { "glue_makeup", 4 }, { "glue_mix", 75 }, { "clip", 1 } } },
            { "Afrobeats", 12, { { "ts_attack", 20 }, { "ts_sustain", 10 }, { "weight", 35 }, { "drive", 15 }, { "tone", 10 }, { "glue_on", 1 }, { "glue_thr", -14 },
                                 { "glue_ratio", 1 }, { "glue_att", 5 }, { "glue_rel", 4 }, { "glue_makeup", 2 }, { "glue_mix", 70 } } },
            { "Pop", 13, { { "ts_attack", 20 }, { "weight", 30 }, { "drive", 15 }, { "tone", 15 }, { "glue_on", 1 }, { "glue_thr", -14 }, { "glue_ratio", 2 },
                           { "glue_att", 4 }, { "glue_rel", 4 }, { "glue_makeup", 3 }, { "glue_mix", 70 } } },
            { "Hip-Hop Boom Bap", 14, { { "ts_attack", 25 }, { "ts_sustain", 15 }, { "weight", 40 }, { "drive", 40 }, { "tone", -20 }, { "glue_on", 1 },
                                        { "glue_thr", -16 }, { "glue_ratio", 2 }, { "glue_att", 5 }, { "glue_rel", 2 }, { "glue_makeup", 4 }, { "glue_mix", 80 } } },
            { "Rock", 15, { { "ts_attack", 30 }, { "ts_sustain", 20 }, { "weight", 25 }, { "drive", 20 }, { "tone", 10 }, { "glue_on", 1 }, { "glue_thr", -14 },
                            { "glue_ratio", 2 }, { "glue_att", 4 }, { "glue_rel", 1 }, { "glue_makeup", 3 }, { "glue_mix", 75 } } },
            { "Lo-Fi", 16, { { "ts_attack", -20 }, { "ts_sustain", 20 }, { "weight", 30 }, { "drive", 50 }, { "tone", -50 }, { "glue_on", 1 }, { "glue_thr", -20 },
                             { "glue_ratio", 3 }, { "glue_att", 5 }, { "glue_rel", 3 }, { "glue_makeup", 4 }, { "glue_mix", 70 } } },
        };
        return p;
    }
}

const juce::StringArray& TopDrumsProcessor::genreNames()
{
    static const juce::StringArray s = []
    {
        juce::StringArray a;
        for (auto& g : genres()) a.add (g.name);
        return a;
    }();
    return s;
}

juce::String TopDrumsProcessor::genreForProgram (int program)
{
    if (! juce::isPositiveAndBelow (program, (int) drumPresets().size())) return {};
    return genres()[(size_t) drumPresets()[(size_t) program].genre].name;
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout TopDrumsProcessor::createLayout()
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

    addB ("power", "Power", true);
    // Transient
    addF ("ts_attack", "Attack", { -100.0f, 100.0f, 0.1f }, 0.0f, "%", 0);
    addF ("ts_sustain", "Sustain", { -100.0f, 100.0f, 0.1f }, 0.0f, "%", 0);
    addF ("ts_speed", "Speed", { 0.0f, 100.0f, 0.1f }, 40.0f, "%", 0);
    addC ("ts_focus", "Focus", focusNames(), 0);
    // Color
    addF ("weight", "Weight", { 0.0f, 100.0f, 0.1f }, 0.0f, "%", 0);
    addF ("drive", "Drive", { 0.0f, 100.0f, 0.1f }, 0.0f, "%", 0);
    addF ("tone", "Tone", { -100.0f, 100.0f, 0.1f }, 0.0f, "", 0);
    addB ("clip", "Clip", false);
    // Glue
    addB ("glue_on", "Glue On", false);
    addF ("glue_thr", "Glue Threshold", { -40.0f, 0.0f, 0.1f }, -12.0f, "dB", 1);
    addC ("glue_ratio", "Glue Ratio", ratioNames(), 2);
    addC ("glue_att", "Glue Attack", attackNames(), 4);
    addC ("glue_rel", "Glue Release", releaseNames(), 4);
    addF ("glue_makeup", "Glue Makeup", { 0.0f, 15.0f, 0.1f }, 0.0f, "dB", 1);
    addF ("glue_mix", "Glue Mix", { 0.0f, 100.0f, 0.1f }, 100.0f, "%", 0);
    addC ("glue_sc", "Glue SC HPF", scNames(), 1);
    // Salida
    addF ("out", "Output", { -24.0f, 12.0f, 0.1f }, 0.0f, "dB", 1);
    addF ("mix", "Mix", { 0.0f, 100.0f, 0.1f }, 100.0f, "%", 0);
    addC ("genre", "Genero (IA)", genreNames(), 0);
    return { ps.begin(), ps.end() };
}

TopDrumsProcessor::TopDrumsProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TopDrums", createLayout())
{
}

TopDrumsProcessor::~TopDrumsProcessor()
{
    pool.removeAllJobs (true, 35000);   // espera a que termine una consulta a Claude en curso
}

bool TopDrumsProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet(), in = l.getMainInputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) && in == out;
}

void TopDrumsProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
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

    eFA = eSA = eFR = eSR = tsGainDb = 0.0f;
    glueGr = glueSlow = 0.0f;
    lastFocus = lastSc = -1;
    focusA.reset(); focusB.reset();
    for (int c = 0; c < 2; ++c) for (auto* f : { &weightLp[c], &tiltLo[c], &tiltHi[c], &scHp[c] }) f->reset();
    inPeak = outPeak = 0.0f;

    outSm.reset (sr, 0.03);     outSm.setCurrentAndTargetValue (dbToGain (p ("out")));
    mixSm.reset (sr, 0.03);     mixSm.setCurrentAndTargetValue (p ("mix") * 0.01f);
    powerSm.reset (sr, 0.02);   powerSm.setCurrentAndTargetValue (on ("power") ? 1.0f : 0.0f);
    glueMixSm.reset (sr, 0.02); glueMixSm.setCurrentAndTargetValue (on ("glue_on") ? p ("glue_mix") * 0.01f : 0.0f);
    anLow.setLowPass (sr, 120.0, 0.707);
    anHigh.setHighPass (sr, 6000.0, 0.707);
    updateFilters();
}

void TopDrumsProcessor::updateFilters()
{
    const int focus = (int) p ("ts_focus");
    if (focus != lastFocus)
    {
        lastFocus = focus;
        switch (focus)
        {
            case 1:  focusA.setLowPass (sr, 150.0, 0.707);   focusB.setLowPass (sr, 150.0, 0.707); break;     // Kick
            case 2:  focusA.setHighPass (sr, 180.0, 0.707);  focusB.setLowPass (sr, 5000.0, 0.707); break;    // Snare
            case 3:  focusA.setHighPass (sr, 6000.0, 0.707); focusB.setIdentity(); break;                       // Hats
            default: focusA.setIdentity(); focusB.setIdentity(); break;
        }
    }
    const int sc = (int) p ("glue_sc");
    if (sc != lastSc)
    {
        lastSc = sc;
        static const float f[] { 0.0f, 60.0f, 90.0f, 150.0f };
        for (auto& h : scHp) { if (sc == 0) h.setIdentity(); else h.setHighPass (sr, f[sc], 0.707); }
    }
    // Tono: inclinacion alrededor de 1 kHz (+ = brillante, - = oscuro)
    const float tilt = p ("tone") * 0.01f * 6.0f;
    for (int c = 0; c < 2; ++c)
    {
        tiltLo[c].setLowShelf (sr, 250.0, -tilt);
        tiltHi[c].setHighShelf (sr, 4000.0, tilt);
        weightLp[c].setLowPass (sr * 2.0, 100.0, 0.8);
    }
}

void TopDrumsProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int chans = std::min (2, buffer.getNumChannels());
    for (int c = chans; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);
    if (n == 0 || chans == 0) return;
    updateFilters();

    dryBuf.setSize (2, n, false, false, true);
    for (int c = 0; c < chans; ++c) dryBuf.copyFrom (c, 0, buffer, c, 0, n);

    float pk = 0.0f;
    for (int c = 0; c < chans; ++c) pk = std::max (pk, buffer.getMagnitude (c, 0, n));
    inPeak = std::max (pk, inPeak * 0.9f);
    inLevelDb.store (gainToDb (inPeak));

    float* L = buffer.getWritePointer (0);
    float* R = chans > 1 ? buffer.getWritePointer (1) : nullptr;

    // ---------- IA LOCAL: mide la bateria tal como entra
    if (analysing.load())
    {
        const juce::ScopedLock sl (anLock);
        for (int i = 0; i < n; ++i)
        {
            const float m = R ? 0.5f * (L[i] + R[i]) : L[i];
            anSumSq += (double) m * m;
            const float lo = anLow.process (m), hi = anHigh.process (m);
            anLowSq += (double) lo * lo; anHighSq += (double) hi * hi;
            anPeak = std::max (anPeak, (double) std::abs (m));
            anBlockSq += (double) m * m;
            if (++anBlockN >= (int) (sr * 0.05)) { anShortRms.push_back ((float) std::sqrt (anBlockSq / anBlockN)); anBlockSq = 0; anBlockN = 0; }
        }
        // golpes: se cuentan con el detector de transientes (abajo)
        const int done = anSamples.fetch_add (n) + n;
        if (done >= anTarget) finishAnalysis();
    }

    // ---------- TRANSIENT SHAPER
    {
        const float speed = 0.4f + 2.2f * p ("ts_speed") * 0.01f;      // escala de tiempos (rapido .. lento)
        const float aF = std::exp (-1.0f / (0.0003f * speed * (float) sr)), aS = std::exp (-1.0f / (0.012f * speed * (float) sr));
        const float rF = std::exp (-1.0f / (0.030f * speed * (float) sr)), rS = std::exp (-1.0f / (0.220f * speed * (float) sr));
        const float rCommon = std::exp (-1.0f / (0.080f * (float) sr)), aCommon = std::exp (-1.0f / (0.0005f * (float) sr));
        const float att = p ("ts_attack") * 0.01f, sus = p ("ts_sustain") * 0.01f;
        const float gSm = std::exp (-1.0f / (0.0007f * (float) sr));
        float maxT = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float m = R ? 0.5f * (L[i] + R[i]) : L[i];
            const float d = std::abs (focusB.process (focusA.process (m)));
            // ataque: dos envolventes con distinto ataque (misma caida)
            eFA = d + (d > eFA ? aF : rCommon) * (eFA - d);
            eSA = d + (d > eSA ? aS : rCommon) * (eSA - d);
            // sustain: dos envolventes con distinta caida (mismo ataque)
            eFR = d + (d > eFR ? aCommon : rF) * (eFR - d);
            eSR = d + (d > eSR ? aCommon : rS) * (eSR - d);
            const float aDiff = juce::jlimit (0.0f, 24.0f, gainToDb (eFA + 1e-6f) - gainToDb (eSA + 1e-6f));
            const float sDiff = juce::jlimit (0.0f, 24.0f, gainToDb (eSR + 1e-6f) - gainToDb (eFR + 1e-6f));
            maxT = std::max (maxT, aDiff);
            const float target = juce::jlimit (-18.0f, 18.0f, att * 0.9f * aDiff + sus * 0.9f * sDiff);
            tsGainDb = target + gSm * (tsGainDb - target);
            const float g = dbToGain (tsGainDb);
            L[i] *= g; if (R) R[i] *= g;

            if (analysing.load())
            {
                if (anRefractory > 0) --anRefractory;
                else if (aDiff > 6.0f) { ++anOnsets; anPunchSum += aDiff; anRefractory = (int) (sr * 0.07); }
            }
        }
        transientDb.store (maxT);
    }

    // ---------- COLOR (2x): peso de graves con armonicos + saturacion
    {
        const float weight = p ("weight") * 0.01f, drive = p ("drive") * 0.01f;
        // siempre pasa por el oversampling (aunque WEIGHT y DRIVE esten en 0) para que la latencia sea constante
        {
            juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) chans, (size_t) n);
            auto up = oversampling.processSamplesUp (block);
            const float dg = 1.0f + drive * 5.0f;
            const float makeup = 1.0f + drive * 0.35f;   // compensa la perdida de picos
            for (size_t c = 0; c < up.getNumChannels(); ++c)
            {
                auto* d = up.getChannelPointer (c);
                for (size_t i = 0; i < up.getNumSamples(); ++i)
                {
                    float x = d[i];
                    // WEIGHT: mas sub + armonicos del kick/808 (se oyen hasta en el celular)
                    if (weight > 0.0f)
                    {
                        const float lo = weightLp[c].process (x);
                        x += weight * (0.9f * lo + 0.6f * std::tanh (lo * 4.0f));
                    }
                    // DRIVE: saturacion suave tipo cinta, mismo volumen
                    if (drive > 0.0f) x = std::tanh (x * dg) / dg * makeup;
                    d[i] = x;
                }
            }
            oversampling.processSamplesDown (block);
        }
        if (std::abs (p ("tone")) > 0.5f)
            for (int c = 0; c < chans; ++c)
            {
                auto* d = buffer.getWritePointer (c);
                for (int i = 0; i < n; ++i) d[i] = tiltHi[c].process (tiltLo[c].process (d[i]));
            }
    }

    // ---------- GLUE (compresor de bus, en paralelo con GLUE MIX)
    {
        glueMixSm.setTargetValue (on ("glue_on") ? p ("glue_mix") * 0.01f : 0.0f);
        static const float ratios[] { 1.5f, 2.0f, 4.0f, 10.0f }, atts[] { 0.1f, 0.3f, 1.0f, 3.0f, 10.0f, 30.0f }, rels[] { 0.1f, 0.3f, 0.6f, 1.2f };
        const float ratio = ratios[juce::jlimit (0, 3, (int) p ("glue_ratio"))];
        const float attMs = atts[juce::jlimit (0, 5, (int) p ("glue_att"))];
        const int relIdx = juce::jlimit (0, 4, (int) p ("glue_rel"));
        const bool autoRel = relIdx == 4;
        const float relS = autoRel ? 0.1f : rels[relIdx];
        const float aC = std::exp (-1.0f / (attMs * 0.001f * (float) sr));
        const float rC = std::exp (-1.0f / (relS * (float) sr));
        const float rFastAuto = std::exp (-1.0f / (0.1f * (float) sr)), rSlowAuto = std::exp (-1.0f / (1.2f * (float) sr));
        const float thr = p ("glue_thr"), makeup = dbToGain (p ("glue_makeup")), knee = 6.0f;
        float maxGr = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float dl = scHp[0].process (L[i]), dr = R ? scHp[1].process (R[i]) : dl;
            const float lvl = gainToDb (std::max (std::abs (dl), std::abs (dr)) + 1e-9f);
            const float over = lvl - thr;
            float target = 0.0f;
            if (over > knee * 0.5f) target = over * (1.0f - 1.0f / ratio);
            else if (over > -knee * 0.5f) { const float k = over + knee * 0.5f; target = (1.0f - 1.0f / ratio) * k * k / (2.0f * knee); }
            if (target > glueGr) glueGr = target + aC * (glueGr - target);
            else if (autoRel)
            {
                // auto release: rapido tras golpes sueltos, lento si comprime seguido (bombeo musical)
                glueSlow = target + rSlowAuto * (glueSlow - target);
                const float fast = target + rFastAuto * (glueGr - target);
                glueGr = std::max (fast, glueSlow * 0.6f);
            }
            else glueGr = target + rC * (glueGr - target);
            maxGr = std::max (maxGr, glueGr);
            const float g = dbToGain (-glueGr) * makeup;
            const float mx = glueMixSm.getNextValue();
            L[i] = L[i] + mx * (L[i] * g - L[i]);
            if (R) R[i] = R[i] + mx * (R[i] * g - R[i]);
        }
        grDb.store (on ("glue_on") ? -maxGr : 0.0f);
    }

    // ---------- SALIDA: output, clip suave, mix global y POWER (dry alineado con la latencia)
    {
        const bool clip = on ("clip");
        outSm.setTargetValue (dbToGain (p ("out")));
        mixSm.setTargetValue (p ("mix") * 0.01f);
        powerSm.setTargetValue (on ("power") ? 1.0f : 0.0f);
        const int len = dryLatency + 1;
        for (int i = 0; i < n; ++i)
        {
            const float og = outSm.getNextValue(), mx = mixSm.getNextValue(), pw = powerSm.getNextValue();
            const int rd = (dryPos + 1) % len;
            for (int c = 0; c < chans; ++c)
            {
                auto& ring = dryRing[c];
                ring[(size_t) dryPos] = dryBuf.getSample (c, i);
                const float dry = ring[(size_t) rd];
                float wet = buffer.getSample (c, i) * og;
                if (clip) { const float a = std::abs (wet); if (a > 0.7f) wet = std::copysign (0.7f + 0.3f * std::tanh ((a - 0.7f) / 0.3f), wet); }
                wet = dry + (wet - dry) * mx;
                buffer.setSample (c, i, dry + (wet - dry) * pw);
            }
            dryPos = rd;
        }
    }
    if (chans == 1 && buffer.getNumChannels() > 1) buffer.copyFrom (1, 0, buffer, 0, 0, n);

    pk = 0.0f;
    for (int c = 0; c < chans; ++c) pk = std::max (pk, buffer.getMagnitude (c, 0, n));
    outPeak = std::max (pk, outPeak * 0.9f);
    outLevelDb.store (gainToDb (outPeak));
}

//==============================================================================
// IA LOCAL
void TopDrumsProcessor::startAnalysis()
{
    const juce::ScopedLock sl (anLock);
    anSumSq = anLowSq = anHighSq = anPeak = anPunchSum = 0.0;
    anOnsets = anRefractory = 0;
    anShortRms.clear(); anBlockSq = 0; anBlockN = 0;
    anLow.reset(); anHigh.reset();
    anTarget = (int) (sr * 8.0);
    anSamples.store (0);
    analysisDone.store (false);
    analysing.store (true);
}

float TopDrumsProcessor::analysisProgress() const
{
    return anTarget > 0 ? juce::jlimit (0.0f, 1.0f, (float) anSamples.load() / (float) anTarget) : 0.0f;
}

void TopDrumsProcessor::finishAnalysis()
{
    // se llama con anLock tomado
    analysing.store (false);
    Analysis a;
    const double nS = std::max (1, anSamples.load());
    const double rms = std::sqrt (anSumSq / nS);
    a.rmsDb = gainToDb ((float) rms + 1e-9f);
    a.valid = a.rmsDb > -50.0f && anOnsets >= 4;
    a.crestDb = gainToDb ((float) anPeak + 1e-9f) - a.rmsDb;
    a.lowDb = gainToDb ((float) std::sqrt (anLowSq / nS) + 1e-9f) - a.rmsDb;
    a.highDb = gainToDb ((float) std::sqrt (anHighSq / nS) + 1e-9f) - a.rmsDb;
    a.densityPerSec = (float) anOnsets / (float) (nS / sr);
    a.punchDb = anOnsets > 0 ? (float) (anPunchSum / anOnsets) : 0.0f;
    if (anShortRms.size() > 10)
    {
        auto v = anShortRms; std::sort (v.begin(), v.end());
        const float hi = v[(size_t) (v.size() * 0.95)], lo = std::max (1e-6f, v[(size_t) (v.size() * 0.20)]);
        a.dynRangeDb = gainToDb (hi) - gainToDb (lo);
    }

    const auto& g = genres()[(size_t) juce::jlimit (0, (int) genres().size() - 1, getGenre())];
    auto add = [&a] (const char* id, float v) { a.settings.push_back ({ id, v }); };
    auto U = [] (const char* s) { return juce::String::fromUTF8 (s); };
    if (! a.valid)
    {
        a.findings.add (U ("No se detectó suficiente batería. Reproduce los drums al menos 8 segundos."));
    }
    else
    {
        a.findings.add (U ("Objetivo: batería ") + U (g.character) + " (" + g.name + ")");
        // Pegada / glue
        const float dc = a.crestDb - g.crest;
        if (dc > 3.0f)
        {
            a.findings.add (U ("Picos sueltos: la batería no está pegada (pegada ") + juce::String (a.crestDb, 1) + " dB, objetivo " + juce::String (g.crest, 1) + ")");
            a.recommendations.add (U ("Más GLUE: threshold más bajo y ratio 4 para unir la batería"));
            add ("glue_on", 1); add ("glue_thr", juce::jlimit (-30.0f, -8.0f, -10.0f - dc * 1.5f)); add ("glue_ratio", 2); add ("glue_mix", 80);
        }
        else if (dc < -3.0f)
        {
            a.findings.add (U ("Batería aplastada: le falta golpe (pegada ") + juce::String (a.crestDb, 1) + " dB)");
            a.recommendations.add (U ("Sube ATTACK para recuperar el golpe y usa menos GLUE"));
            add ("ts_attack", juce::jlimit (0.0f, 70.0f, 25.0f - dc * 6.0f)); add ("glue_thr", -8.0f);
        }
        else a.findings.add (U ("Pegada correcta para el género (") + juce::String (a.crestDb, 1) + " dB)");
        // Golpe de los transientes
        if (a.punchDb < 8.0f)
        {
            a.findings.add (U ("Transientes débiles (") + juce::String (a.punchDb, 1) + " dB)");
            a.recommendations.add (U ("ATTACK +30 a +45 % para que el kick y el snare corten"));
            add ("ts_attack", 35.0f);
        }
        // Peso de graves
        const float dl = a.lowDb - g.low;
        if (dl < -2.5f)
        {
            a.findings.add (U ("Le falta peso abajo (graves ") + juce::String (a.lowDb, 1) + " dB, objetivo " + juce::String (g.low, 1) + ")");
            a.recommendations.add (U ("Sube WEIGHT: más sub y armónicos del kick/808"));
            add ("weight", juce::jlimit (20.0f, 85.0f, 30.0f - dl * 8.0f));
            add ("glue_sc", 2);
        }
        else if (dl > 2.5f)
        {
            a.findings.add (U ("Demasiados graves (") + juce::String (a.lowDb, 1) + " dB): puede sonar embarrado");
            a.recommendations.add (U ("Baja WEIGHT y usa el SC HPF del glue en 150 Hz para que no bombee"));
            add ("weight", 0.0f); add ("glue_sc", 3);
        }
        else a.findings.add (U ("Peso de graves correcto"));
        // Brillo
        const float dh = a.highDb - g.high;
        if (dh < -3.0f)  { a.findings.add (U ("Batería opaca (agudos ") + juce::String (a.highDb, 1) + " dB)"); a.recommendations.add (U ("TONE hacia brillante (+20 a +40)")); add ("tone", juce::jlimit (10.0f, 50.0f, -dh * 6.0f)); }
        else if (dh > 3.0f) { a.findings.add (U ("Agudos muy fuertes (") + juce::String (a.highDb, 1) + " dB): hats/platos pueden molestar"); a.recommendations.add (U ("TONE hacia oscuro (-15 a -30)")); add ("tone", juce::jlimit (-40.0f, -10.0f, -dh * 5.0f)); }
        else a.findings.add (U ("Brillo correcto"));
        // Para estilos pesados: saturacion y clip
        if (g.low > -5.0f && a.crestDb > 9.0f)
        {
            a.recommendations.add (U ("Para un sonido pesado: DRIVE 30-40 % y CLIP encendido"));
            add ("drive", 35.0f); add ("clip", 1.0f);
        }
        if (a.recommendations.isEmpty()) a.recommendations.add (U ("¡Tu batería ya está muy cerca del sonido del género! Solo ajusta a tu gusto."));
    }
    lastAnalysis = a;
    analysisDone.store (true);
}

TopDrumsProcessor::Analysis TopDrumsProcessor::getAnalysis()
{
    const juce::ScopedLock sl (anLock);
    return lastAnalysis;
}

void TopDrumsProcessor::applySettings (const std::vector<std::pair<juce::String, float>>& s)
{
    for (const auto& [id, v] : s)
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
        {
            if (id == "power" || id == "mix" || id == "genre") continue;   // la IA no toca estos
            const auto& r = rp->getNormalisableRange();
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (rp->convertTo0to1 (juce::jlimit (r.start, r.end, v)));
            rp->endChangeGesture();
        }
}

//==============================================================================
// CLAUDE ONLINE
static juce::File settingsFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Top Vocal Chain").getChildFile ("TopDrums.settings");
}

juce::String TopDrumsProcessor::loadApiKey()
{
    auto f = settingsFile();
    if (auto xml = juce::XmlDocument::parse (f)) return xml->getStringAttribute ("apiKey");
    return {};
}

void TopDrumsProcessor::saveApiKey (const juce::String& key)
{
    auto f = settingsFile();
    f.getParentDirectory().createDirectory();
    juce::XmlElement x ("TopDrumsSettings");
    x.setAttribute ("apiKey", key.trim());
    x.writeTo (f);
}

juce::String TopDrumsProcessor::buildClaudeRequest (const Analysis& a)
{
    juce::DynamicObject::Ptr cur = new juce::DynamicObject();
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
            cur->setProperty (rp->getParameterID(), rp->convertFrom0to1 (rp->getValue()));

    juce::String info;
    info << "Genero elegido: " << genreNames()[getGenre()] << "\n";
    if (a.valid)
        info << "Medidas de la bateria (antes del plugin): pegada (pico/RMS) " << juce::String (a.crestDb, 1) << " dB; graves <120 Hz "
             << juce::String (a.lowDb, 1) << " dB respecto al total; agudos >6 kHz " << juce::String (a.highDb, 1) << " dB; fuerza media de transientes "
             << juce::String (a.punchDb, 1) << " dB; golpes por segundo " << juce::String (a.densityPerSec, 1) << "; nivel RMS " << juce::String (a.rmsDb, 1)
             << " dBFS; rango dinamico " << juce::String (a.dynRangeDb, 1) << " dB.\n";
    else
        info << "No hay medidas (el usuario aun no analizo la bateria).\n";
    info << "Ajustes actuales del plugin (JSON): " << juce::JSON::toString (juce::var (cur.get()), true) << "\n";

    const juce::String system = juce::String::fromUTF8 (
        "Eres un ingeniero de mezcla experto en musica latina y urbana. Ayudas a un productor que usa el plugin Top Drums "
        "(transient shaper + color + glue) en su bus de bateria. Responde en espanol, muy sencillo, en 3-6 consejos cortos. "
        "Parametros validos y rangos: ts_attack -100..100, ts_sustain -100..100, ts_speed 0..100, ts_focus 0 Full/1 Kick/2 Snare/3 Hats, "
        "weight 0..100, drive 0..100, tone -100..100, clip 0/1, glue_on 0/1, glue_thr -40..0, glue_ratio 0=1.5 1=2 2=4 3=10, "
        "glue_att 0=0.1ms 1=0.3 2=1 3=3 4=10 5=30, glue_rel 0=0.1s 1=0.3 2=0.6 3=1.2 4=Auto, glue_makeup 0..15, glue_mix 0..100, "
        "glue_sc 0=Off 1=60Hz 2=90Hz 3=150Hz, out -24..12. "
        "Devuelve SOLO un JSON con esta forma: {\"consejos\": [\"...\"], \"ajustes\": {\"id\": valor}}. "
        "En 'ajustes' pon solo los parametros que conviene cambiar.");

    juce::DynamicObject::Ptr msg = new juce::DynamicObject();
    msg->setProperty ("role", "user");
    msg->setProperty ("content", info + juce::String::fromUTF8 ("¿Qué me recomiendas para que mi batería suene como el género elegido?"));
    juce::DynamicObject::Ptr body = new juce::DynamicObject();
    body->setProperty ("model", "claude-sonnet-5-5");
    body->setProperty ("max_tokens", 1024);
    body->setProperty ("system", system);
    body->setProperty ("messages", juce::Array<juce::var> { juce::var (msg.get()) });
    return juce::JSON::toString (juce::var (body.get()), true);
}

bool TopDrumsProcessor::parseClaudeReply (const juce::String& body, juce::String& text, std::vector<std::pair<juce::String, float>>& settings)
{
    const auto v = juce::JSON::parse (body);
    juce::String reply;
    if (auto* content = v.getProperty ("content", {}).getArray())
        for (auto& block : *content)
            if (block.getProperty ("type", {}).toString() == "text") reply << block.getProperty ("text", {}).toString();
    if (reply.isEmpty())
    {
        const auto err = v.getProperty ("error", {}).getProperty ("message", {}).toString();
        text = err.isNotEmpty() ? "Error de la API: " + err : juce::String ("Respuesta vacia de la API.");
        return false;
    }
    // el modelo devuelve JSON (a veces dentro de ```json ... ```)
    const int a = reply.indexOfChar ('{'), b = reply.lastIndexOfChar ('}');
    const auto j = (a >= 0 && b > a) ? juce::JSON::parse (reply.substring (a, b + 1)) : juce::var();
    juce::StringArray tips;
    if (auto* arr = j.getProperty ("consejos", {}).getArray())
        for (auto& t : *arr) tips.add (t.toString());
    if (auto* obj = j.getProperty ("ajustes", {}).getDynamicObject())
        for (auto& pr : obj->getProperties())
            settings.push_back ({ pr.name.toString(), (float) pr.value });
    text = tips.isEmpty() ? reply.trim() : "- " + tips.joinIntoString ("\n- ");
    return true;
}

void TopDrumsProcessor::askClaude()
{
    if (askState.load() == AskState::running) return;
    const auto key = loadApiKey();
    if (key.isEmpty())
    {
        const juce::ScopedLock sl (claudeLock);
        claudeText = juce::String::fromUTF8 ("Falta tu clave de API. Pulsa CLAVE API y pega la clave que creaste en console.anthropic.com.");
        claudeSettings.clear();
        askState.store (AskState::error);
        return;
    }
    const auto request = buildClaudeRequest (getAnalysis());
    askState.store (AskState::running);
    pool.addJob ([this, key, request]
    {
        int status = 0;
        juce::StringPairArray respHeaders;
        const juce::String headers = "x-api-key: " + key + "\r\nanthropic-version: 2023-06-01\r\ncontent-type: application/json";
        auto url = juce::URL ("https://api.anthropic.com/v1/messages").withPOSTData (request);
        auto stream = url.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                                                 .withExtraHeaders (headers)
                                                 .withConnectionTimeoutMs (30000)
                                                 .withStatusCode (&status)
                                                 .withResponseHeaders (&respHeaders));
        juce::String text; std::vector<std::pair<juce::String, float>> settings;
        bool ok = false;
        if (stream == nullptr) text = juce::String::fromUTF8 ("No se pudo conectar con Claude. Revisa tu internet.");
        else
        {
            const auto body = stream->readEntireStreamAsString();
            ok = parseClaudeReply (body, text, settings);
            if (status == 401) { ok = false; text = juce::String::fromUTF8 ("La clave de API no es válida. Pulsa CLAVE API y vuelve a pegarla."); }
        }
        const juce::ScopedLock sl (claudeLock);
        claudeText = text; claudeSettings = settings;
        askState.store (ok ? AskState::done : AskState::error);
    });
}

juce::String TopDrumsProcessor::getClaudeText() { const juce::ScopedLock sl (claudeLock); return claudeText; }
std::vector<std::pair<juce::String, float>> TopDrumsProcessor::getClaudeSettings() { const juce::ScopedLock sl (claudeLock); return claudeSettings; }

//==============================================================================
int TopDrumsProcessor::getNumPrograms() { return (int) drumPresets().size(); }

const juce::String TopDrumsProcessor::getProgramName (int i)
{
    return juce::isPositiveAndBelow (i, (int) drumPresets().size()) ? juce::String (drumPresets()[(size_t) i].name) : juce::String();
}

void TopDrumsProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) drumPresets().size())) return;
    currentProgram = index;
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
            rp->setValueNotifyingHost (rp->getDefaultValue());
    for (const auto& [id, val] : drumPresets()[(size_t) index].v)
        if (auto* rp = apvts.getParameter (id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (val));
    if (auto* g = apvts.getParameter ("genre"))
        g->setValueNotifyingHost (g->convertTo0to1 ((float) drumPresets()[(size_t) index].genre));
}

void TopDrumsProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void TopDrumsProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = (int) tree.getProperty ("program", 0);
            apvts.replaceState (tree);
        }
}

juce::AudioProcessorEditor* TopDrumsProcessor::createEditor() { return new TopDrumsEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TopDrumsProcessor(); }
