#include "TopVoiceMidiProcessor.h"
#include "TopVoiceMidiEditor.h"

namespace
{
    using Cfg = TopVoiceMidiProcessor::Cfg;
    Cfg cfgFrom (juce::AudioProcessorValueTreeState& s)
    {
        auto f = [&s] (const juce::String& id) { return s.getRawParameterValue (id)->load(); };
        Cfg c;
        c.mode = (int) f ("mode"); c.octave = (int) std::round (f ("octave")); c.channel = (int) f ("channel");
        c.key = (int) f ("key"); c.scale = (int) f ("scale"); c.chordType = (int) f ("chord_type");
        c.voicing = (int) f ("voicing"); c.inversion = (int) std::round (f ("inversion")); c.bendMode = (int) f ("bend_mode");
        c.trigCh = (int) f ("trig_ch"); c.wave = (int) f ("synth_wave");
        c.inGain = juce::Decibels::decibelsToGain (f ("in_gain")); c.gateDb = f ("gate"); c.dry = f ("dry") / 100.0f;
        c.synthVol = f ("synth_vol") / 100.0f; c.responseMs = f ("response"); c.stability = f ("stability") / 100.0f;
        c.velSens = f ("vel_sens") / 100.0f; c.bendRange = f ("bend_range"); c.bendThr = f ("bend_thr") / 100.0f;
        c.trigSens = f ("trig_sens") / 100.0f;
        c.synthOn = f ("synth_on") > 0.5f; c.autoKey = f ("auto_key") > 0.5f; c.chordOn = f ("chord_on") > 0.5f;
        for (int i = 0; i < 12; ++i)
        {
            c.mask[(size_t) i] = f ("nm" + juce::String (i)) > 0.5f ? 1 : 0;
            c.chordMask[(size_t) i] = f ("cn" + juce::String (i)) > 0.5f ? 1 : 0;
        }
        for (int i = 0; i < 4; ++i)
        {
            const auto n = juce::String (i + 1);
            c.ccOn[(size_t) i] = f ("cc" + n + "_on") > 0.5f; c.ccSrc[(size_t) i] = (int) f ("cc" + n + "_src");
            c.ccNum[(size_t) i] = (int) std::round (f ("cc" + n + "_num")); c.ccSmooth[(size_t) i] = f ("cc" + n + "_smooth") / 100.0f;
            c.trigNote[(size_t) i] = (int) std::round (f ("trig_note" + juce::String (i)));
        }
        return c;
    }
}

juce::StringArray TopVoiceMidiProcessor::scaleNames()
{
    return { juce::String::fromUTF8 ("Cromática"), "Mayor", "Menor", juce::String::fromUTF8 ("Menor armónica"),
             juce::String::fromUTF8 ("Pentatónica mayor"), juce::String::fromUTF8 ("Pentatónica menor"), "Blues", "Personalizada (tu voz)" };
}
juce::StringArray TopVoiceMidiProcessor::chordNames()
{
    return { "Mayor", "Menor", juce::String::fromUTF8 ("Diatónico (triada)"), juce::String::fromUTF8 ("Diatónico 7ma"), "Power (5ta)",
             "Sus2", "Sus4", "Maj7", "Min7", "Add9", "Personalizado" };
}
juce::StringArray TopVoiceMidiProcessor::ccSourceNames()
{
    return { "Envolvente (volumen)", "Brillo", juce::String::fromUTF8 ("Afinación"), "Vocal (a / u)" };
}

juce::AudioProcessorValueTreeState::ParameterLayout TopVoiceMidiProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;
    auto addF = [&ps] (const String& id, const String& name, NormalisableRange<float> r, float def, String unit)
    {
        ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, AudioParameterFloatAttributes().withLabel (unit)));
    };
    auto addI = [&ps] (const String& id, const String& name, int lo, int hi, int def)
    { ps.push_back (std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, name, lo, hi, def)); };
    auto addB = [&ps] (const String& id, const String& name, bool def)
    { ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };
    auto addC = [&ps] (const String& id, const String& name, const StringArray& c, int def)
    { ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, c, def)); };

    addC ("mode", "Modo", { String::fromUTF8 ("Melodía"), "Ritmo", "Ambos" }, 0);
    addF ("in_gain", "Ganancia de entrada", { -12.0f, 24.0f, 0.1f }, 0.0f, "dB");
    addF ("gate", "Puerta (sensibilidad)", { -70.0f, -20.0f, 0.1f }, -48.0f, "dB");
    addF ("dry", "Voz (monitoreo)", { 0.0f, 100.0f, 1.0f }, 0.0f, "%");
    addB ("synth_on", "Sinte de prueba", true);
    addF ("synth_vol", "Volumen del sinte", { 0.0f, 100.0f, 1.0f }, 70.0f, "%");
    addC ("synth_wave", "Onda del sinte", { "Seno", "Sierra", "Cuadrada" }, 1);
    addI ("octave", "Octava", -3, 3, 0);
    addF ("response", "Respuesta", { 5.0f, 120.0f, 1.0f }, 10.0f, "ms");
    addF ("stability", "Estabilidad", { 0.0f, 100.0f, 1.0f }, 35.0f, "%");
    addF ("vel_sens", "Sensibilidad de velocidad", { 0.0f, 100.0f, 1.0f }, 70.0f, "%");
    addI ("channel", "Canal MIDI (melodia)", 1, 16, 1);
    addC ("key", "Tonalidad", { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0);
    addC ("scale", "Escala", scaleNames(), 0);
    addB ("auto_key", "Tonalidad automatica", false);
    for (int i = 0; i < 12; ++i) addB ("nm" + String (i), "Nota permitida " + String (i), true);
    addB ("chord_on", "Acordes", false);
    addC ("chord_type", "Tipo de acorde", chordNames(), 2);
    addC ("voicing", "Voicing", { "Cerrado", "Abierto", "Amplio" }, 0);
    addI ("inversion", "Inversion", 0, 2, 0);
    for (int i = 0; i < 12; ++i) addB ("cn" + String (i), "Acorde personalizado " + String (i), i == 0 || i == 4 || i == 7);
    addC ("bend_mode", "Pitch bend", { "Apagado", "Inteligente", "Exacto" }, 0);
    addI ("bend_range", "Rango del bend", 1, 24, 2);
    addF ("bend_thr", "Zona muerta del bend", { 10.0f, 100.0f, 1.0f }, 40.0f, "cents");
    const int ccDefNum[4] { 11, 74, 1, 71 };
    for (int i = 0; i < 4; ++i)
    {
        const auto n = String (i + 1);
        addB ("cc" + n + "_on", "CC " + n + " activo", false);
        addC ("cc" + n + "_src", "CC " + n + " fuente", ccSourceNames(), i);
        addI ("cc" + n + "_num", "CC " + n + " numero", 0, 127, ccDefNum[i]);
        addF ("cc" + n + "_smooth", "CC " + n + " suavizado", { 0.0f, 100.0f, 1.0f }, 30.0f, "%");
    }
    addF ("trig_sens", "Sensibilidad del beatbox", { 0.0f, 100.0f, 1.0f }, 50.0f, "%");
    addI ("trig_ch", "Canal MIDI (bateria)", 1, 16, 10);
    const int notes[4] { 36, 38, 42, 46 };
    for (int i = 0; i < 4; ++i) addI ("trig_note" + String (i), "Nota pad " + String (i + 1), 0, 127, notes[i]);
    return { ps.begin(), ps.end() };
}

TopVoiceMidiProcessor::TopVoiceMidiProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TopVoiceMidi", createLayout())
{
    capBuf.resize ((size_t) capFifo.getTotalSize());
    extBuf.resize ((size_t) extFifo.getTotalSize());
    for (auto& a : pitchHist) a.store (0.0f);
    for (auto& a : noteHist) a.store (-1.0f);
    for (auto& a : ccLive) a.store (0.0f);
}

TopVoiceMidiProcessor::~TopVoiceMidiProcessor()
{
    sender.stopThread (1000);
}

bool TopVoiceMidiProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet(), in = l.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void TopVoiceMidiProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    pitch.prepare (sr);
    hits.prepare (sr);
    synth.prepare (sr);
    bandHi.setHighPass (sr, 2500.0, 0.707);
    bandLo.setBandPass (sr, 450.0, 1.0);
    bandMid.setBandPass (sr, 1100.0, 1.0);
    env = 0; envDb = -100; curRoot = -1; cand = -1; numSounding = 0; lastBend = 8192; bendSm = 0;
    fastEnv = slowEnv = 0; refractory = 0;
    for (auto& o : offs) o.note = -1;
}

float TopVoiceMidiProcessor::detectionLatencyMs() const
{
    return (float) (1000.0 * pitch.latencySamples() / sr) + apvts.getRawParameterValue ("response")->load();
}

int TopVoiceMidiProcessor::effectiveKey() const
{
    return apvts.getRawParameterValue ("auto_key")->load() > 0.5f ? detKey.load() : (int) apvts.getRawParameterValue ("key")->load();
}
int TopVoiceMidiProcessor::effectiveScale() const
{
    if (apvts.getRawParameterValue ("auto_key")->load() > 0.5f) return detMinor.load() ? 2 : 1;
    return (int) apvts.getRawParameterValue ("scale")->load();
}

//==============================================================================
void TopVoiceMidiProcessor::emit (const juce::MidiMessage& m, int sample, juce::MidiBuffer& out)
{
    out.addEvent (m, sample);
    const auto* raw = m.getRawData();
    Ev e { capTime + sample / sr, { raw[0], m.getRawDataSize() > 1 ? raw[1] : (juce::uint8) 0, m.getRawDataSize() > 2 ? raw[2] : (juce::uint8) 0 } };
    if (capturing.load())
    {
        int s1, n1, s2, n2;
        capFifo.prepareToWrite (1, s1, n1, s2, n2);
        if (n1 > 0) { capBuf[(size_t) s1] = e; capFifo.finishedWrite (1); }
    }
    if (extId.isNotEmpty())
    {
        int s1, n1, s2, n2;
        extFifo.prepareToWrite (1, s1, n1, s2, n2);
        if (n1 > 0) { extBuf[(size_t) s1] = e; extFifo.finishedWrite (1); }
    }
}

int TopVoiceMidiProcessor::buildChord (int root, std::array<int, 8>& out) const
{
    int n = 0;
    if (! cfg.chordOn) { out[0] = root; return 1; }
    const auto& mask = cfg.mask;
    const bool custom = cfg.scale == 7 && ! cfg.autoKey;     // la escala personalizada usa notas absolutas (C..B)
    const int key = cfg.autoKey ? detKey.load() : (custom ? 0 : cfg.key);
    const auto& sm = custom ? mask : vmidi::Theory::scaleMask (cfg.autoKey ? (detMinor.load() ? 2 : 1) : cfg.scale);
    auto push = [&] (int note) { if (n < 8) out[(size_t) n++] = note; };
    switch (cfg.chordType)
    {
        case 0: push (root); push (root + 4); push (root + 7); break;
        case 1: push (root); push (root + 3); push (root + 7); break;
        case 2: push (root); push (vmidi::Theory::diatonicUp (root, key, sm, 2)); push (vmidi::Theory::diatonicUp (root, key, sm, 4)); break;
        case 3: push (root); push (vmidi::Theory::diatonicUp (root, key, sm, 2)); push (vmidi::Theory::diatonicUp (root, key, sm, 4));
                push (vmidi::Theory::diatonicUp (root, key, sm, 6)); break;
        case 4: push (root); push (root + 7); push (root + 12); break;
        case 5: push (root); push (root + 2); push (root + 7); break;
        case 6: push (root); push (root + 5); push (root + 7); break;
        case 7: push (root); push (root + 4); push (root + 7); push (root + 11); break;
        case 8: push (root); push (root + 3); push (root + 7); push (root + 10); break;
        case 9: push (root); push (root + 4); push (root + 7); push (root + 14); break;
        default: for (int i = 0; i < 12; ++i) if (cfg.chordMask[(size_t) i] || i == 0) push (root + i); break;
    }
    std::sort (out.begin(), out.begin() + n);
    // inversiones: sube la nota mas grave una octava
    for (int k = 0; k < cfg.inversion && n > 1; ++k) { out[0] += 12; std::sort (out.begin(), out.begin() + n); }
    if (cfg.voicing == 1 && n >= 3) { out[1] += 12; std::sort (out.begin(), out.begin() + n); }          // abierto
    if (cfg.voicing == 2 && n < 8) { out[(size_t) n] = out[0] - 12; ++n; std::sort (out.begin(), out.begin() + n); }   // amplio: bajo una octava abajo
    int m = 0;
    for (int i = 0; i < n; ++i)
        if (out[(size_t) i] >= 0 && out[(size_t) i] <= 127 && (m == 0 || out[(size_t) m - 1] != out[(size_t) i])) out[(size_t) m++] = out[(size_t) i];
    return m;
}

void TopVoiceMidiProcessor::notesOn (int root, int vel, juce::MidiBuffer& out, int sample)
{
    if (cfg.bendMode != 0 && lastBend != 8192)
    {
        lastBend = 8192; bendSm = 0;
        emit (juce::MidiMessage::pitchWheel (cfg.channel, 8192), sample, out);
        synth.setBend (0.0f); bendSemis.store (0.0f);
    }
    numSounding = buildChord (root, sounding);
    for (int i = 0; i < numSounding; ++i)
    {
        emit (juce::MidiMessage::noteOn (cfg.channel, sounding[(size_t) i], (juce::uint8) vel), sample, out);
        synth.noteOn (sounding[(size_t) i], (float) vel / 127.0f);
    }
    curRoot = root;
    liveNote.store (root);
    noteSerial.fetch_add (1);
    if (learning.load()) learnedMask.fetch_or (1 << (((root % 12) + 12) % 12));
}

void TopVoiceMidiProcessor::notesOff (juce::MidiBuffer& out, int sample)
{
    for (int i = 0; i < numSounding; ++i)
    {
        emit (juce::MidiMessage::noteOff (cfg.channel, sounding[(size_t) i]), sample, out);
        synth.noteOff (sounding[(size_t) i]);
    }
    numSounding = 0;
    curRoot = -1;
    liveNote.store (-1);
}

void TopVoiceMidiProcessor::updateKey()
{
    static const double maj[12] { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
    static const double mnr[12] { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };
    double total = 0; for (float h : hist) total += h;
    if (total < 1.5) return;
    double best = -2; int root = 0; bool minor = false;
    for (int m = 0; m < 2; ++m)
        for (int k = 0; k < 12; ++k)
        {
            const double* p = m == 0 ? maj : mnr;
            double mh = 0, mp = 0;
            for (int i = 0; i < 12; ++i) { mh += hist[i]; mp += p[i]; }
            mh /= 12; mp /= 12;
            double num = 0, dh = 0, dp = 0;
            for (int i = 0; i < 12; ++i)
            {
                const double a = hist[(i + k) % 12] - mh, b = p[i] - mp;
                num += a * b; dh += a * a; dp += b * b;
            }
            const double c = num / std::sqrt (dh * dp + 1e-12);
            if (c > best) { best = c; root = k; minor = m == 1; }
        }
    detKey.store (root); detMinor.store (minor); detConf.store ((float) juce::jlimit (0.0, 1.0, best));
}

// Se llama cada ~3 ms con una nueva estimacion de tono
void TopVoiceMidiProcessor::melodyFrame (int sample, juce::MidiBuffer& out)
{
    const float frameSec = 0.0029f;
    const int frameSamples = (int) (sr * frameSec);
    const float f = pitch.getFrequency(), aper = pitch.getAperiodicity();
    lastAper = aper;
    const bool voiced = envDb > cfg.gateDb && aper < 0.25f && f > 70.0f && f < 1100.0f;
    float m = 0.0f;
    if (voiced)
    {
        const float raw = 69.0f + 12.0f * std::log2 (f / 440.0f);
        med[medPos] = raw; medPos = (medPos + 1) % 3;
        // mediana de 3 para quitar errores de octava aislados
        float a = med[0], b = med[1], c = med[2];
        if (a > b) std::swap (a, b);
        if (b > c) std::swap (b, c);
        if (a > b) std::swap (a, b);
        m = std::abs (raw - b) > 6.0f ? b : raw;
    }
    liveMidi.store (voiced ? m : 0.0f);

    const bool melodyOn = cfg.mode != 1;
    const bool custom = ! cfg.autoKey && cfg.scale == 7;
    const int key = cfg.autoKey ? detKey.load() : (custom ? 0 : cfg.key);
    const auto& mask = custom ? cfg.mask : vmidi::Theory::scaleMask (cfg.autoKey ? (detMinor.load() ? 2 : 1) : cfg.scale);
    const int responseSamples = (int) (sr * cfg.responseMs / 1000.0f);

    if (voiced && melodyOn)
    {
        unvoicedSamples = 0;
        const float mo = m + 12.0f * (float) cfg.octave;
        const int q = juce::jlimit (0, 127, vmidi::Theory::nearest (mo, key, mask));
        const float stick = 0.5f + 0.6f * cfg.stability;
        const bool wantsChange = curRoot < 0 || (q != curRoot && std::abs (mo - (float) curRoot) > stick);
        if (wantsChange)
        {
            if (q == cand) { candSamples += frameSamples; candPeakDb = std::max (candPeakDb, envDb); }
            else { cand = q; candSamples = 0; candPeakDb = envDb; }
            const int need = curRoot < 0 ? responseSamples : responseSamples / 2;
            if (candSamples >= need)
            {
                const float dyn = 20.0f + 107.0f * juce::jlimit (0.0f, 1.0f, (candPeakDb - cfg.gateDb) / (-8.0f - cfg.gateDb));
                const int vel = juce::jlimit (1, 127, (int) std::round (100.0f + (dyn - 100.0f) * cfg.velSens));
                if (curRoot >= 0) notesOff (out, sample);
                notesOn (q, vel, out, sample);
                cand = -1; candSamples = 0;
            }
        }
        else { cand = -1; candSamples = 0; }

        // pitch bend
        if (cfg.bendMode != 0 && curRoot >= 0)
        {
            float dev = mo - (float) curRoot;
            if (cfg.bendMode == 1)       // inteligente: te mantiene en la nota hasta que te alejas a proposito
                dev = std::abs (dev) < cfg.bendThr ? 0.0f : dev - std::copysign (cfg.bendThr, dev);
            bendSm += (dev - bendSm) * 0.35f;
            const float b = juce::jlimit (-cfg.bendRange, cfg.bendRange, bendSm);
            const int v = juce::jlimit (0, 16383, (int) std::round (8192.0f + b / cfg.bendRange * 8191.0f));
            if (std::abs (v - lastBend) >= 24)
            {
                lastBend = v;
                emit (juce::MidiMessage::pitchWheel (cfg.channel, v), sample, out);
                synth.setBend (b); bendSemis.store (b);
            }
        }
        // estadistica para la tonalidad automatica
        const int pc = (((int) std::round (m)) % 12 + 12) % 12;
        hist[pc] += frameSec;
    }
    else
    {
        cand = -1; candSamples = 0;
        unvoicedSamples += frameSamples;
        if (curRoot >= 0 && (unvoicedSamples >= (int) (sr * 0.035) || ! melodyOn)) notesOff (out, sample);
    }
    for (float& h : hist) h *= 0.99985f;
    if (++keyTimer >= 150) { keyTimer = 0; updateKey(); }

    // CC: envolvente, brillo, afinacion, vocal
    float src[4];
    src[0] = juce::jlimit (0.0f, 1.0f, (envDb + 60.0f) / 60.0f);
    src[1] = juce::jlimit (0.0f, 1.0f, std::sqrt (eHi / (eAll + 1e-12f)) * 2.2f);
    src[2] = voiced ? juce::jlimit (0.0f, 1.0f, (m - 36.0f) / 48.0f) : ccSm[2];
    src[3] = envDb > cfg.gateDb ? juce::jlimit (0.0f, 1.0f, eMid / (eLo + eMid + 1e-12f)) : 0.0f;
    for (int i = 0; i < 4; ++i)
    {
        const int s = juce::jlimit (0, 3, cfg.ccSrc[(size_t) i]);
        const float tau = 0.004f + cfg.ccSmooth[(size_t) i] * 0.3f;
        auto& sm = ccSm[(size_t) i];
        float target = src[s];
        if (s == 2 && ! voiced) target = sm;
        sm += (target - sm) * (1.0f - std::exp (-frameSec / tau));
        ccLive[(size_t) i].store (sm);
        if (! cfg.ccOn[(size_t) i]) { ccSent[(size_t) i] = -1; continue; }
        const int v = juce::jlimit (0, 127, (int) std::round (sm * 127.0f));
        if (v != ccSent[(size_t) i])
        {
            ccSent[(size_t) i] = v;
            emit (juce::MidiMessage::controllerEvent (cfg.channel, cfg.ccNum[(size_t) i], v), sample, out);
        }
    }
    // historial para la grafica (1 de cada 3 cuadros, ~3.5 s visibles)
    if (++histDec >= 3)
    {
        histDec = 0;
        const int p = histPos.load();
        pitchHist[(size_t) p].store (voiced ? m : 0.0f);
        noteHist[(size_t) p].store ((float) curRoot);
        histPos.store ((p + 1) % histSize);
    }
}

void TopVoiceMidiProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    midi.clear();
    cfg = cfgFrom (apvts);
    synth.setWave (cfg.wave);

    const int n = buffer.getNumSamples();
    const int inCh = getTotalNumInputChannels(), outCh = getTotalNumOutputChannels();
    for (int ch = inCh; ch < outCh; ++ch) buffer.clear (ch, 0, n);

    // golpes de bateria pendientes de soltar
    for (auto& o : offs)
        if (o.note >= 0)
        {
            if (o.samplesLeft < n) { emit (juce::MidiMessage::noteOff (o.ch, o.note), std::max (0, o.samplesLeft), midi); o.note = -1; }
            else o.samplesLeft -= n;
        }

    const float atkE = 1.0f - std::exp (-1.0f / (0.001f * (float) sr)), relE = 1.0f - std::exp (-1.0f / (0.060f * (float) sr));
    const float relFast = std::exp (-1.0f / (0.012f * (float) sr));
    const float atkSlow = 1.0f - std::exp (-1.0f / (0.030f * (float) sr)), relSlow = 1.0f - std::exp (-1.0f / (0.180f * (float) sr));
    const float eCoef = 1.0f - std::exp (-1.0f / (0.020f * (float) sr));
    const float onsetRatio = 3.2f - 2.0f * cfg.trigSens;
    const float hitGate = juce::Decibels::decibelsToGain (cfg.gateDb + 4.0f);
    const bool rhythmOn = cfg.mode != 0;
    float peakIn = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        float x = 0.0f;
        for (int ch = 0; ch < inCh; ++ch) x += buffer.getSample (ch, i);
        x = x / (float) std::max (1, inCh) * cfg.inGain;
        const float ax = std::abs (x);
        peakIn = std::max (peakIn, ax);

        env += (ax - env) * (ax > env ? atkE : relE);
        envDb = juce::Decibels::gainToDecibels (env * 1.4f, -100.0f);
        const float hiS = bandHi.process (x), loS = bandLo.process (x), miS = bandMid.process (x);
        eHi += (hiS * hiS - eHi) * eCoef; eAll += (x * x - eAll) * eCoef;
        eLo += (loS * loS - eLo) * eCoef; eMid += (miS * miS - eMid) * eCoef;

        if (pitch.push (x)) melodyFrame (i, midi);

        // ---- ritmo: deteccion de golpes ----
        fastEnv = ax > fastEnv ? ax : fastEnv * relFast;
        slowEnv += (ax - slowEnv) * (ax > slowEnv ? atkSlow : relSlow);
        if (refractory > 0) --refractory;
        if ((rhythmOn || trainingPad.load() >= 0) && refractory == 0 && ! hits.active()
            && fastEnv > hitGate && fastEnv > slowEnv * onsetRatio)
        {
            hits.start();
            refractory = (int) (sr * 0.075);
        }
        if (hits.push (x))
        {
            const auto feat = hits.features();
            const float pk = juce::Decibels::gainToDecibels (hits.getPeak(), -100.0f);
            const int vel = juce::jlimit (1, 127, (int) std::round (30.0f + 97.0f * juce::jlimit (0.0f, 1.0f, (pk + 40.0f) / 38.0f)));
            const int tp = trainingPad.load();
            if (tp >= 0)
            {
                const juce::SpinLock::ScopedTryLockType sl (classifierLock);
                if (sl.isLocked()) classifier.add (tp, feat);
                lastPad.store (tp); hitSerial.fetch_add (1);
                synth.drum (tp, (float) vel / 127.0f);
            }
            else if (rhythmOn && ! (cfg.mode == 2 && (curRoot >= 0 || lastAper < 0.2f)))
            {
                int pad = 1;
                {
                    const juce::SpinLock::ScopedTryLockType sl (classifierLock);
                    if (sl.isLocked()) pad = classifier.classify (feat);
                }
                const int note = cfg.trigNote[(size_t) pad];
                emit (juce::MidiMessage::noteOn (cfg.trigCh, note, (juce::uint8) vel), i, midi);
                for (auto& o : offs) if (o.note < 0) { o.note = note; o.ch = cfg.trigCh; o.samplesLeft = i + (int) (sr * 0.06); break; }
                synth.drum (pad, (float) vel / 127.0f);
                lastPad.store (pad); hitSerial.fetch_add (1);
            }
        }

        const float s = cfg.synthOn ? synth.render() * cfg.synthVol : 0.0f;
        const float o = x * cfg.dry + s;
        for (int ch = 0; ch < outCh; ++ch) buffer.setSample (ch, i, o);
    }
    if (! cfg.synthOn) synth.allOff();
    inDb.store (juce::Decibels::gainToDecibels (peakIn, -100.0f));
    capTime += n / sr;
}

//==============================================================================
int TopVoiceMidiProcessor::padExamples (int pad) const { return classifier.examples[(size_t) juce::jlimit (0, 3, pad)]; }
void TopVoiceMidiProcessor::clearPad (int pad)
{
    const juce::SpinLock::ScopedLockType sl (classifierLock);
    classifier.clear (juce::jlimit (0, 3, pad));
}

void TopVoiceMidiProcessor::startCapture()
{
    const juce::ScopedLock sl (captureLock);
    drainCapture();
    capturing.store (true);
}
void TopVoiceMidiProcessor::stopCapture()
{
    capturing.store (false);
    const juce::ScopedLock sl (captureLock);
    drainCapture();
}
void TopVoiceMidiProcessor::clearCapture()
{
    const juce::ScopedLock sl (captureLock);
    drainCapture();
    captured.clear();
}
void TopVoiceMidiProcessor::drainCapture()
{
    int s1, n1, s2, n2;
    const int ready = capFifo.getNumReady();
    capFifo.prepareToRead (ready, s1, n1, s2, n2);
    auto add = [this] (int start, int num)
    {
        for (int k = 0; k < num; ++k)
        {
            const auto& e = capBuf[(size_t) (start + k)];
            const int len = juce::MidiMessage::getMessageLengthFromFirstByte (e.b[0]);
            captured.addEvent (juce::MidiMessage (e.b, len, e.t));
        }
    };
    add (s1, n1); add (s2, n2);
    capFifo.finishedRead (n1 + n2);
}
int TopVoiceMidiProcessor::capturedNotes()
{
    const juce::ScopedLock sl (captureLock);
    drainCapture();
    int c = 0;
    for (auto* e : captured) if (e->message.isNoteOn()) ++c;
    return c;
}

juce::File TopVoiceMidiProcessor::defaultExportFile() const
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Top Voice MIDI");
    dir.createDirectory();
    return dir.getChildFile ("TopVoiceMIDI_" + juce::Time::getCurrentTime().formatted ("%Y%m%d_%H%M%S") + ".mid");
}

bool TopVoiceMidiProcessor::exportCapture (const juce::File& file, bool cleanGhosts)
{
    juce::MidiMessageSequence seq;
    {
        const juce::ScopedLock sl (captureLock);
        drainCapture();
        seq = captured;
    }
    if (seq.getNumEvents() == 0) return false;
    seq.sort();
    seq.updateMatchedPairs();
    if (cleanGhosts)
    {
        // quita notas fantasma (< 35 ms)
        for (int i = seq.getNumEvents(); --i >= 0;)
        {
            auto* e = seq.getEventPointer (i);
            if (e->message.isNoteOn() && e->noteOffObject != nullptr
                && e->noteOffObject->message.getTimeStamp() - e->message.getTimeStamp() < 0.035)
            {
                const int off = seq.getIndexOf (e->noteOffObject);
                if (off > i) { seq.deleteEvent (off, false); seq.deleteEvent (i, false); }
            }
        }
    }
    // compensa la latencia de deteccion y empieza en 0
    const double lat = detectionLatencyMs() / 1000.0;
    const double first = seq.getStartTime();
    const double shift = juce::jmax (0.0, first);
    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) bpm = *b;
    const int tpq = 960;
    juce::MidiMessageSequence out;
    out.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::round (60000000.0 / bpm)), 0);
    for (auto* e : seq)
    {
        auto m = e->message;
        const double t = juce::jmax (0.0, m.getTimeStamp() - shift - (m.isNoteOnOrOff() ? lat * 0.5 : 0.0));
        m.setTimeStamp (std::round (t * bpm / 60.0 * tpq));
        out.addEvent (m);
    }
    out.sort();
    out.updateMatchedPairs();
    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (tpq);
    mf.addTrack (out);
    file.deleteFile();
    juce::FileOutputStream os (file);
    return os.openedOk() && mf.writeTo (os);
}

//==============================================================================
juce::StringArray TopVoiceMidiProcessor::externalDevices (juce::Array<juce::MidiDeviceInfo>& infos) const
{
    infos = juce::MidiOutput::getAvailableDevices();
    juce::StringArray names;
    for (auto& d : infos) names.add (d.name);
    return names;
}

void TopVoiceMidiProcessor::setExternalDevice (const juce::String& identifier)
{
    {
        const juce::ScopedLock sl (extLock);
        extOut.reset();
        if (identifier.isNotEmpty()) extOut = juce::MidiOutput::openDevice (identifier);
        extId = extOut != nullptr ? identifier : juce::String();
    }
    if (extId.isNotEmpty() && ! sender.isThreadRunning()) sender.startThread();
}

void TopVoiceMidiProcessor::Sender::run()
{
    while (! threadShouldExit())
    {
        wait (1);
        auto& f = owner.extFifo;
        const int ready = f.getNumReady();
        if (ready == 0) continue;
        int s1, n1, s2, n2;
        f.prepareToRead (ready, s1, n1, s2, n2);
        {
            const juce::ScopedLock sl (owner.extLock);
            auto send = [this] (int start, int num)
            {
                for (int k = 0; k < num; ++k)
                {
                    const auto& e = owner.extBuf[(size_t) (start + k)];
                    if (owner.extOut != nullptr)
                        owner.extOut->sendMessageNow (juce::MidiMessage (e.b, juce::MidiMessage::getMessageLengthFromFirstByte (e.b[0])));
                }
            };
            send (s1, n1); send (s2, n2);
        }
        f.finishedRead (n1 + n2);
    }
}

//==============================================================================
void TopVoiceMidiProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    juce::ValueTree hitsTree ("Hits");
    {
        const juce::SpinLock::ScopedLockType sl (classifierLock);
        for (int p = 0; p < 4; ++p)
        {
            juce::ValueTree pad ("Pad");
            pad.setProperty ("n", classifier.examples[(size_t) p], nullptr);
            juce::String v;
            for (float x : classifier.mean[(size_t) p].v) v << x << " ";
            pad.setProperty ("v", v.trim(), nullptr);
            hitsTree.appendChild (pad, nullptr);
        }
    }
    state.removeChild (state.getChildWithName ("Hits"), nullptr);
    state.appendChild (hitsTree, nullptr);
    state.setProperty ("extMidi", extId, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void TopVoiceMidiProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            const auto hitsTree = tree.getChildWithName ("Hits");
            {
                const juce::SpinLock::ScopedLockType sl (classifierLock);
                for (int p = 0; p < 4 && p < hitsTree.getNumChildren(); ++p)
                {
                    const auto pad = hitsTree.getChild (p);
                    classifier.examples[(size_t) p] = (int) pad.getProperty ("n");
                    juce::StringArray vals; vals.addTokens (pad.getProperty ("v").toString(), " ", "");
                    for (int i = 0; i < vmidi::HitFeatures::num && i < vals.size(); ++i)
                        classifier.mean[(size_t) p].v[(size_t) i] = vals[i].getFloatValue();
                }
            }
            const auto ext = tree.getProperty ("extMidi").toString();
            tree.removeChild (tree.getChildWithName ("Hits"), nullptr);
            apvts.replaceState (tree);
            if (ext.isNotEmpty()) setExternalDevice (ext);
        }
}

juce::AudioProcessorEditor* TopVoiceMidiProcessor::createEditor() { return new TopVoiceMidiEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TopVoiceMidiProcessor(); }
