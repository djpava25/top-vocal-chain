#include "TopMasterProcessor.h"
#include "TopMasterEditor.h"

using namespace tmaster;

namespace
{
    // procesador "fantasma" para simular la cadena sin tocar la del DAW
    struct SimHost : juce::AudioProcessor
    {
        SimHost() : AudioProcessor (BusesProperties().withInput ("In", juce::AudioChannelSet::stereo(), true).withOutput ("Out", juce::AudioChannelSet::stereo(), true)),
                    apvts (*this, nullptr, "TopMaster", tmaster::createLayout()) {}
        const juce::String getName() const override { return "sim"; }
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        double getTailLengthSeconds() const override { return 0; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}
        juce::AudioProcessorValueTreeState apvts;
    };
    void setParam (juce::AudioProcessorValueTreeState& s, const juce::String& id, float v)
    {
        if (auto* p = s.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
    }
    // pasa el audio por una cadena y mide el loudness integrado de la salida
    float simulate (SimHost& h, const std::array<int, numReorderable>& order, const std::vector<float>& L, const std::vector<float>& R, double sr)
    {
        Chain c (h.apvts);
        for (int i = 0; i < numReorderable; ++i) c.order[(size_t) i].store (order[(size_t) i]);
        c.prepare (sr);
        std::vector<float> l (2048), r (2048);
        for (size_t pos = 0; pos < L.size(); pos += 2048)
        {
            const int n = (int) std::min<size_t> (2048, L.size() - pos);
            std::copy (L.begin() + (long) pos, L.begin() + (long) pos + n, l.begin());
            std::copy (R.begin() + (long) pos, R.begin() + (long) pos + n, r.begin());
            c.process (l.data(), r.data(), n, true);
        }
        return c.outMeter.integrated.load();
    }
}

juce::StringArray TopMasterProcessor::genreNames()
{
    return { "Pop", "Hip-Hop / Trap", juce::String::fromUTF8 ("Reggaetón / Urbano"), "Dembow", "Tropical (bachata, salsa, merengue)",
             juce::String::fromUTF8 ("EDM / Electrónica"), "Rock", "R&B / Soul", juce::String::fromUTF8 ("Acústico") };
}
juce::StringArray TopMasterProcessor::targetNames()
{
    return { "Streaming (-14 LUFS)", "Alto (-11 LUFS)", "Muy alto (-9 LUFS)", "Club (-7.5 LUFS)" };
}

std::vector<float> TopMasterProcessor::genreCurve (int g)
{
    // balance tonal tipico por tercio de octava (dB relativos, 0 = zona 200 Hz - 5 kHz)
    static const float fr[] { 30, 60, 120, 250, 500, 1000, 2000, 4000, 8000, 12000, 16000 };
    static const float cv[9][11] {
        { 3, 4, 3, 0.5f, -0.5f, 0, -0.5f, -1.5f, -4, -7, -11 },     // pop
        { 7, 8, 5, 1, -1, 0, -1, -2.5f, -5, -8, -12 },               // hip-hop / trap
        { 5, 7, 5, 1, -0.5f, 0, -0.5f, -2, -4.5f, -7.5f, -11 },      // reggaeton
        { 6, 7, 4, 0.5f, -1, 0, 0, -1, -3, -6, -10 },                // dembow
        { 0, 3, 3, 1, 0, 0, 0, -1, -3.5f, -6.5f, -10 },              // tropical
        { 6, 7, 4, 0, -1, 0, -0.5f, -1.5f, -3.5f, -6, -9 },          // edm
        { 1, 3, 3, 1.5f, 0.5f, 0, 0, -1.5f, -4, -7, -11 },           // rock
        { 4, 6, 4, 1, 0, 0, -1, -2.5f, -5, -8, -12 },                // r&b
        { -3, 0, 1, 1, 0.5f, 0, -0.5f, -2, -4.5f, -7.5f, -11 } };    // acustico
    const auto& c = cv[juce::jlimit (0, 8, g)];
    std::vector<float> out ((size_t) numBands);
    for (int b = 0; b < numBands; ++b)
    {
        const float f = bandFreq (b);
        if (f <= fr[0]) { out[(size_t) b] = c[0] - 6.0f * std::log2 (fr[0] / f); continue; }   // por debajo de 30 Hz cae
        if (f >= fr[10]) { out[(size_t) b] = c[10] - 8.0f * std::log2 (f / fr[10]); continue; }
        int i = 0; while (fr[i + 1] < f) ++i;
        const float t = std::log2 (f / fr[i]) / std::log2 (fr[i + 1] / fr[i]);
        out[(size_t) b] = c[i] + (c[i + 1] - c[i]) * t;
    }
    return out;
}

std::vector<float> TopMasterProcessor::tonalCurve (const float* L, const float* R, int n, double sr)
{
    const int order = 13, N = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> buf ((size_t) N * 2), win ((size_t) N), acc ((size_t) N / 2 + 1, 0.0f);
    for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (6.2831853f * i / N);
    int frames = 0;
    for (int pos = 0; pos + N <= n; pos += N / 2)
    {
        for (int i = 0; i < N; ++i) buf[(size_t) i] = 0.5f * (L[pos + i] + R[pos + i]) * win[(size_t) i];
        std::fill (buf.begin() + N, buf.end(), 0.0f);
        fft.performFrequencyOnlyForwardTransform (buf.data(), true);
        for (int k = 0; k <= N / 2; ++k) acc[(size_t) k] += buf[(size_t) k] * buf[(size_t) k];
        ++frames;
    }
    std::vector<float> out ((size_t) numBands, -100.0f);
    if (frames == 0) return out;
    for (int b = 0; b < numBands; ++b)
    {
        const float f = bandFreq (b);
        const int a = std::max (1, (int) std::floor (f / std::pow (2.0f, 1.0f / 6.0f) * N / sr));
        const int e = std::min (N / 2, std::max (a, (int) std::ceil (f * std::pow (2.0f, 1.0f / 6.0f) * N / sr)));
        double p = 0; for (int k = a; k <= e; ++k) p += acc[(size_t) k];
        out[(size_t) b] = (float) (10.0 * std::log10 (p / frames + 1e-14));
    }
    return out;
}

//==============================================================================
TopMasterProcessor::TopMasterProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      juce::Thread ("TopMaster assistant"),
      apvts (*this, nullptr, "TopMaster", tmaster::createLayout())
{
    formats.registerBasicFormats();
    chain = std::make_unique<Chain> (apvts);
    startThread();
}

TopMasterProcessor::~TopMasterProcessor()
{
    signalThreadShouldExit();
    notify();
    stopThread (8000);
}

bool TopMasterProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet(), in = l.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return in == out;
}

void TopMasterProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    chain->prepare (sr);
    setLatencySamples (chain->latency());
    for (auto& d : bypassDelay) d.assign ((size_t) chain->latency() + 1, 0.0f);
    bpPos = 0;
}

std::array<int, numReorderable> TopMasterProcessor::getOrder() const
{
    std::array<int, numReorderable> o {};
    for (int i = 0; i < numReorderable; ++i) o[(size_t) i] = chain->order[(size_t) i].load();
    return o;
}
void TopMasterProcessor::setOrder (const std::array<int, numReorderable>& o)
{
    for (int i = 0; i < numReorderable; ++i) chain->order[(size_t) i].store (o[(size_t) i]);
}
void TopMasterProcessor::moveModule (int module, int delta)
{
    auto o = getOrder();
    for (int i = 0; i < numReorderable; ++i)
        if (o[(size_t) i] == module)
        {
            const int j = i + delta;
            if (j < 0 || j >= numReorderable) return;
            std::swap (o[(size_t) i], o[(size_t) j]);
            setOrder (o);
            return;
        }
}

//==============================================================================
void TopMasterProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int total = buffer.getNumSamples();
    const int numCh = juce::jmin (2, getTotalNumInputChannels());
    const bool bypass = apvts.getRawParameterValue ("bypass")->load() > 0.5f;
    const bool match = apvts.getRawParameterValue ("gainmatch")->load() > 0.5f;
    std::array<float, 2048> tmpR {}, dryL {}, dryR {};

    for (int pos = 0; pos < total; pos += 2048)
    {
        const int n = juce::jmin (2048, total - pos);
        float* L = buffer.getWritePointer (0) + pos;
        float* R = numCh > 1 ? buffer.getWritePointer (1) + pos : tmpR.data();
        if (numCh == 1) std::copy (L, L + n, R);

        // captura para el asistente (lo que entra, sin procesar)
        if (listening.load())
        {
            float pk = 0; for (int i = 0; i < n; ++i) pk = std::max (pk, std::abs (L[i]));
            int c = capLen.load();
            if (pk > 1.0e-4f)
            {
                const int room = (int) capture[0].size() - c;
                const int k = juce::jmin (room, n);
                std::copy (L, L + k, capture[0].begin() + c); std::copy (R, R + k, capture[1].begin() + c);
                c += k; capLen.store (c);
                listenSeconds.store ((float) (c / sr));
                if (room - k <= 0) { listening.store (false); wantAssistant.store (true); notify(); }
            }
        }
        // seco retrasado (para el bypass con la misma latencia)
        const int D = (int) bypassDelay[0].size();
        for (int i = 0; i < n; ++i)
        {
            const int p = (bpPos + i) % D;
            dryL[(size_t) i] = bypassDelay[0][(size_t) p]; dryR[(size_t) i] = bypassDelay[1][(size_t) p];
            bypassDelay[0][(size_t) p] = L[i]; bypassDelay[1][(size_t) p] = R[i];
        }
        bpPos = (bpPos + n) % D;

        chain->process (L, R, n, true);

        const float outS = chain->outMeter.shortTerm.load(), inS = chain->inMeter.shortTerm.load();
        if (bypass)
        {
            const float tg = match && outS > -60.0f && inS > -60.0f ? dB2g (juce::jlimit (-24.0f, 24.0f, outS - inS)) : 1.0f;
            for (int i = 0; i < n; ++i)
            {
                matchSm += (tg - matchSm) * 0.0005f;
                L[i] = dryL[(size_t) i] * matchSm; R[i] = dryR[(size_t) i] * matchSm;
            }
            matchGainDb.store (g2dB (matchSm));
        }
        if (referenceOn.load())
        {
            const juce::SpinLock::ScopedTryLockType sl (refLock);
            const int rl = refLen.load();
            if (sl.isLocked() && rl > 0)
            {
                const float g = match && outS > -60.0f ? dB2g (juce::jlimit (-24.0f, 24.0f, outS - refLufs)) : 1.0f;
                for (int i = 0; i < n; ++i)
                {
                    const int idx = (int) refPos;
                    L[i] = ref[0][(size_t) idx] * g; R[i] = ref[1][(size_t) idx] * g;
                    refPos += 1.0; if (refPos >= rl) refPos = 0.0;
                }
            }
        }
    }
    for (int c = numCh; c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, total);
}

//==============================================================================
void TopMasterProcessor::startListening()
{
    if (assistantBusy.load()) return;
    for (auto& c : capture) c.assign ((size_t) (sr * 30.0), 0.0f);
    capLen.store (0);
    listenSeconds.store (0.0f);
    listening.store (true);
}
void TopMasterProcessor::stopListeningAndRun()
{
    listening.store (false);
    wantAssistant.store (true);
    notify();
}

bool TopMasterProcessor::assistantCore (const std::vector<float>& L, const std::vector<float>& R, std::map<juce::String, float>& out, juce::String& report)
{
    const int n = (int) L.size();
    if (n < (int) (sr * 3.0)) { report = juce::String::fromUTF8 ("Necesito al menos 3 segundos de música. Dale play y vuelve a intentarlo."); return false; }
    assistantProgress.store (0.05f);
    // 1) tono medido vs objetivo
    auto meas = tonalCurve (L.data(), R.data(), n, sr);
    lastCurve = meas;
    const bool useRef = useReference && referenceCurve.size() == (size_t) numBands;
    auto tgt = useRef ? referenceCurve : genreCurve (genre);
    auto norm = [] (std::vector<float>& c)
    {
        double s = 0; int k = 0;
        for (int b = 0; b < numBands; ++b) { const float f = bandFreq (b); if (f >= 200 && f <= 5000) { s += c[(size_t) b]; ++k; } }
        for (auto& v : c) v -= (float) (s / k);
    };
    norm (meas); norm (tgt);
    const float k = intensity == 0 ? 0.45f : intensity == 1 ? 0.6f : 0.8f;
    auto diffIn = [&] (float lo, float hi)
    {
        double s = 0; int c = 0;
        for (int b = 0; b < numBands; ++b) { const float f = bandFreq (b); if (f >= lo && f <= hi) { s += tgt[(size_t) b] - meas[(size_t) b]; ++c; } }
        return c > 0 ? (float) (s / c) : 0.0f;
    };
    struct Band { int idx; float f, lo, hi; };
    const Band bands[] { { 2, 90, 40, 130 }, { 3, 250, 160, 400 }, { 4, 800, 500, 1250 }, { 5, 2500, 1600, 4000 }, { 6, 5000, 4000, 7000 }, { 7, 10000, 8000, 16000 } };
    juce::String eqTxt;
    out["eq_on"] = 1;
    out["eq_b1_on"] = 1; out["eq_b1_f"] = 25; out["eq_b1_q"] = 0.71f;
    out["eq_b8_on"] = 0;
    for (const auto& b : bands)
    {
        const float g = juce::jlimit (-4.0f, 4.0f, std::round (diffIn (b.lo, b.hi) * k * 10.0f) / 10.0f);
        const juce::String p = "eq_b" + juce::String (b.idx);
        out[p + "_on"] = 1; out[p + "_f"] = b.f; out[p + "_g"] = g; out[p + "_ch"] = 0;
        out[p + "_q"] = (b.idx == 2 || b.idx == 7) ? 0.71f : 1.0f;
        if (std::abs (g) >= 0.5f) eqTxt << (g > 0 ? "+" : "") << juce::String (g, 1) << " dB en " << (b.f >= 1000 ? juce::String (b.f / 1000.0f, 1) + "k" : juce::String ((int) b.f)) << ", ";
    }
    // 2) dinamica: umbrales segun el nivel real de cada banda
    {
        Splitter<3> sp; sp.prepare (sr); sp.setFreqs ({ 150.0f, 3000.0f });
        std::array<double, 3> e {}; std::array<float, 3> bl {}, br {};
        double peak = 0, rms = 0;
        for (int i = 0; i < n; ++i)
        {
            sp.split (0, L[(size_t) i], bl); sp.split (1, R[(size_t) i], br);
            for (int b = 0; b < 3; ++b) e[(size_t) b] += 0.5 * ((double) bl[(size_t) b] * bl[(size_t) b] + (double) br[(size_t) b] * br[(size_t) b]);
            peak = std::max (peak, (double) std::max (std::abs (L[(size_t) i]), std::abs (R[(size_t) i])));
            rms += 0.5 * ((double) L[(size_t) i] * L[(size_t) i] + (double) R[(size_t) i] * R[(size_t) i]);
        }
        const float crest = (float) (20.0 * std::log10 (peak / std::sqrt (rms / n + 1e-14) + 1e-9));
        const char* bn[3] { "l", "m", "h" };
        const float offs = intensity == 0 ? 5.0f : intensity == 1 ? 3.0f : 1.0f;
        const float ratio = intensity == 0 ? 1.4f : intensity == 1 ? 1.7f : 2.2f;
        const float att = intensity == 0 ? 30.0f : intensity == 1 ? 20.0f : 12.0f;
        out["dyn_on"] = 1; out["dyn_x1"] = 150; out["dyn_x2"] = 3000; out["dyn_mix"] = 100;
        for (int b = 0; b < 3; ++b)
        {
            const juce::String p = juce::String ("dyn_") + bn[b];
            const float lvl = (float) (10.0 * std::log10 (e[(size_t) b] / n + 1e-14)) + 3.0f;
            out[p + "_thr"] = juce::jlimit (-48.0f, 0.0f, lvl + offs);
            out[p + "_ratio"] = ratio;
            out[p + "_att"] = b == 0 ? att * 1.5f : att;
            out[p + "_rel"] = intensity == 2 ? 110.0f : 160.0f;
            out[p + "_gain"] = 0;
        }
        // mezcla ya aplastada por un limitador: activa el deslimitador
        out["unl_on"] = crest < 8.0f ? 1.0f : 0.0f;
        out["unl_amount"] = 40; out["unl_sens"] = 50;
        if (crest < 8.0f) report << juce::String::fromUTF8 ("La mezcla ya venía muy aplastada (factor de cresta ") << juce::String (crest, 1) << juce::String::fromUTF8 (" dB): activé el Deslimitador.\n");
    }
    // 3) graves en mono, imagen estereo y brillo
    out["bc_on"] = 1; out["bc_mono"] = 1; out["bc_monofreq"] = 100; out["bc_gain"] = 0; out["bc_punch"] = intensity == 2 ? 20.0f : 0.0f; out["bc_sustain"] = 0;
    const bool wideGenre = genre == 0 || genre == 2 || genre == 3 || genre == 5 || genre == 1;
    out["img_on"] = 1; out["img_x1"] = 150; out["img_x2"] = 1500; out["img_x3"] = 7000;
    out["img_w1"] = -40; out["img_w2"] = 0;
    out["img_w3"] = (wideGenre ? 5.0f : 2.0f) * (intensity + 1); out["img_w4"] = (wideGenre ? 7.0f : 3.0f) * (intensity + 1);
    out["img_stereoize"] = 0;
    out["exc_on"] = 1; out["exc_a1"] = 0; out["exc_a2"] = 0;
    out["exc_a3"] = (wideGenre ? 8.0f : 5.0f) * (intensity + 1); out["exc_m3"] = 2; out["exc_mix"] = 100;
    for (const char* off : { "deq_on", "ss_on", "imp_on", "cla_on", "tape_on", "dit_on" }) out[off] = 0;
    // 4) maximizador: busca la ganancia que llega al objetivo simulando la cadena
    const float goal = useRef && refLufs > -60.0f ? refLufs : targetLufs (target);
    out["max_on"] = 1;
    out["max_ceil"] = target <= 1 ? -1.0f : target == 2 ? -0.5f : -0.3f;
    out["max_mode"] = intensity == 2 ? 1.0f : 0.0f;
    out["max_char"] = 50; out["max_tp"] = 1; out["max_link"] = 100;
    out["max_gain"] = 0;

    SimHost sim;
    sim.apvts.replaceState (apvts.copyState());
    for (const auto& [id, v] : out) setParam (sim.apvts, id, v);
    const auto order = getOrder();
    float lufsIn;
    {
        LoudnessMeter m; m.prepare (sr);
        for (int pos = 0; pos < n; pos += 4096) m.process (L.data() + pos, R.data() + pos, std::min (4096, n - pos));
        lufsIn = m.integrated.load();
    }
    float g = 0.0f, lufs = simulate (sim, order, L, R, sr);
    assistantProgress.store (0.3f);
    for (int it = 0; it < 5; ++it)
    {
        const float err = goal - lufs;
        if (std::abs (err) < 0.25f) break;
        g = juce::jlimit (0.0f, 24.0f, g + err * (it == 0 ? 1.0f : 1.15f));
        setParam (sim.apvts, "max_gain", g);
        lufs = simulate (sim, order, L, R, sr);
        assistantProgress.store (0.3f + 0.13f * (it + 1));
        if (g >= 24.0f || threadShouldExit()) break;
    }
    out["max_gain"] = std::round (g * 10.0f) / 10.0f;
    const float gr = sim.apvts.getRawParameterValue ("max_gain")->load();
    juce::ignoreUnused (gr);

    report << juce::String::fromUTF8 ("Objetivo: ") << (useRef ? "tu referencia (" + refName + ")" : genreNames()[genre]) << ", " << juce::String (goal, 1) << " LUFS\n";
    report << "Antes: " << juce::String (lufsIn, 1) << juce::String::fromUTF8 (" LUFS  →  Después: ") << juce::String (lufs, 1) << " LUFS\n";
    report << "EQ: " << (eqTxt.isEmpty() ? juce::String::fromUTF8 ("el tono ya estaba cerca del objetivo") : eqTxt.dropLastCharacters (2)) << "\n";
    report << juce::String::fromUTF8 ("Dinámica 3 bandas ratio ") << juce::String (out["dyn_l_ratio"], 1) << ":1, graves en mono < 100 Hz, Maximizador +" << juce::String (g, 1) << " dB";
    assistantProgress.store (1.0f);
    return true;
}

void TopMasterProcessor::applyProposal (const std::map<juce::String, float>& m)
{
    for (const auto& [id, v] : m)
        if (auto* p = apvts.getParameter (id)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (v)); p->endChangeGesture(); }
}

bool TopMasterProcessor::runAssistantNow (std::function<void (const juce::String&)> onDone)
{
    listening.store (false);
    const int c = capLen.load();
    std::vector<float> l (capture[0].begin(), capture[0].begin() + c), r (capture[1].begin(), capture[1].begin() + c);
    std::map<juce::String, float> out; juce::String rep;
    const bool ok = assistantCore (l, r, out, rep);
    if (ok) applyProposal (out);
    assistantReport = rep;
    if (onDone) onDone (rep);
    return ok;
}

void TopMasterProcessor::run()
{
    while (! threadShouldExit())
    {
        wait (200);
        if (! wantAssistant.exchange (false)) continue;
        assistantBusy.store (true);
        assistantProgress.store (0.0f);
        const int c = capLen.load();
        std::vector<float> l (capture[0].begin(), capture[0].begin() + c), r (capture[1].begin(), capture[1].begin() + c);
        auto out = std::make_shared<std::map<juce::String, float>>();
        juce::String rep;
        const bool ok = assistantCore (l, r, *out, rep);
        if (threadShouldExit()) break;
        juce::WeakReference<TopMasterProcessor> safe (this);
        juce::MessageManager::callAsync ([safe, out, ok, rep]
        {
            if (auto* p = safe.get())
            {
                if (ok) p->applyProposal (*out);
                p->assistantReport = rep;
                p->assistantBusy.store (false);
            }
        });
    }
}

//==============================================================================
bool TopMasterProcessor::loadReference (const juce::File& f)
{
    std::unique_ptr<juce::AudioFormatReader> r (formats.createReaderFor (f));
    if (r == nullptr || r->lengthInSamples <= 0 || r->lengthInSamples > (juce::int64) (r->sampleRate * 900)) return false;
    juce::AudioBuffer<float> b (2, (int) r->lengthInSamples);
    r->read (&b, 0, b.getNumSamples(), 0, true, true);
    if (r->numChannels == 1) b.copyFrom (1, 0, b, 0, 0, b.getNumSamples());
    std::array<std::vector<float>, 2> v;
    const double ratio = r->sampleRate / sr;
    for (int c = 0; c < 2; ++c)
    {
        if (std::abs (ratio - 1.0) < 1e-6) v[(size_t) c].assign (b.getReadPointer (c), b.getReadPointer (c) + b.getNumSamples());
        else
        {
            v[(size_t) c].resize ((size_t) (b.getNumSamples() / ratio));
            juce::LagrangeInterpolator li;
            li.process (ratio, b.getReadPointer (c), v[(size_t) c].data(), (int) v[(size_t) c].size(), b.getNumSamples(), 0);
        }
    }
    const int len = (int) v[0].size();
    LoudnessMeter m; m.prepare (sr);
    for (int pos = 0; pos < len; pos += 4096) m.process (v[0].data() + pos, v[1].data() + pos, std::min (4096, len - pos));
    auto curve = tonalCurve (v[0].data(), v[1].data(), len, sr);
    {
        const juce::SpinLock::ScopedLockType sl (refLock);
        ref = std::move (v);
        refLen.store (len);
        refPos = 0.0;
    }
    refLufs = m.integrated.load();
    referenceCurve = curve;
    refName = f.getFileNameWithoutExtension();
    refFile = f;
    return true;
}
void TopMasterProcessor::clearReference()
{
    referenceOn.store (false);
    const juce::SpinLock::ScopedLockType sl (refLock);
    refLen.store (0); ref[0].clear(); ref[1].clear(); referenceCurve.clear(); refName = {}; refFile = juce::File();
}

//==============================================================================
void TopMasterProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    juce::String o; for (int v : getOrder()) o << v << ",";
    state.setProperty ("order", o, nullptr);
    state.setProperty ("genre", genre, nullptr); state.setProperty ("target", target, nullptr);
    state.setProperty ("intensity", intensity, nullptr); state.setProperty ("useRef", useReference, nullptr);
    state.setProperty ("refFile", refFile.getFullPathName(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}
void TopMasterProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto t = juce::ValueTree::fromXml (*xml);
    juce::StringArray o; o.addTokens (t.getProperty ("order").toString(), ",", ""); o.removeEmptyStrings();
    if (o.size() == numReorderable)
    {
        std::array<int, numReorderable> arr {};
        std::array<bool, numReorderable> seen {};
        bool ok = true;
        for (int i = 0; i < numReorderable; ++i)
        {
            arr[(size_t) i] = o[i].getIntValue();
            if (arr[(size_t) i] < 0 || arr[(size_t) i] >= numReorderable || seen[(size_t) arr[(size_t) i]]) ok = false;
            else seen[(size_t) arr[(size_t) i]] = true;
        }
        if (ok) setOrder (arr);
    }
    genre = t.getProperty ("genre", 0); target = t.getProperty ("target", 0);
    intensity = t.getProperty ("intensity", 1); useReference = t.getProperty ("useRef", false);
    const juce::File rf (t.getProperty ("refFile").toString());
    apvts.replaceState (t);
    if (rf.existsAsFile()) loadReference (rf);
}

juce::AudioProcessorEditor* TopMasterProcessor::createEditor() { return new TopMasterEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TopMasterProcessor(); }
