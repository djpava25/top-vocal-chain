#include "SpqProcessor.h"
#include "SpqEditor.h"

using namespace spq;

namespace
{
    const juce::StringArray typeNames { "Bell", "Low Shelf", "High Shelf", "Low Cut", "High Cut", "Notch", "Band Pass", "Tilt Shelf", "Flat Tilt", "All Pass" };
    const juce::StringArray placeNames { "Stereo", "Left", "Right", "Mid", "Side" };
    juce::StringArray slopeNames() { juce::StringArray s; for (int d : slopeDb) s.add (juce::String (d) + " dB/oct"); return s; }

    // saturacion con ADAA de 1er orden (curva blanda simetrica + par), antiderivada cerrada
    struct Sat
    {
        static double f (double x, double s, double b) { const double q = 1.0 + x * x / (s * s); return x / std::sqrt (q) + b * x * x / q; }
        static double F (double x, double s, double b) { return s * s * std::sqrt (1.0 + x * x / (s * s)) + b * s * s * (x - s * std::atan (x / s)); }
        static double adaa (double x, double& x1, double s, double b)
        {
            const double dx = x - x1;
            const double y = std::abs (dx) > 1e-7 ? (F (x, s, b) - F (x1, s, b)) / dx : f (0.5 * (x + x1), s, b);
            x1 = x; return y;
        }
    };

    struct Preset { const char* name; std::vector<std::vector<std::pair<const char*, float>>> bands; std::vector<std::pair<const char*, float>> glob; };
    // { tipo, f, g, q, pendiente, lugar, modo dinamico, umbral, rango }
    const std::vector<Preset>& presets()
    {
        auto B = [] (int type, float f, float g, float q, int slope = 1, int place = 0, int dm = 0, float thr = -24, float range = -6)
        { return std::vector<std::pair<const char*, float>> { { "on", 1 }, { "type", (float) type }, { "freq", f }, { "gain", g }, { "q", q }, { "slope", (float) slope }, { "place", (float) place }, { "dmode", (float) dm }, { "thr", thr }, { "range", range } }; };
        static const std::vector<Preset> p {
            { "Inicio", {}, {} },
            { "Voz: limpia y al frente", { B (3, 90, 0, 0.71f, 3), B (0, 320, -2.5f, 1.2f, 1, 0, 1, -28, -4), B (0, 3200, 2.0f, 0.9f), B (0, 6500, 0, 2.5f, 1, 0, 2, -30, -6), B (2, 11000, 2.5f, 0.71f) }, {} },
            { "Voz: quitar dureza (espectral)", { B (3, 80, 0, 0.71f, 3), B (0, 4200, 0, 0.6f, 1, 0, 2, -34, -9) }, {} },
            { "De-esser espectral", { B (2, 7000, 0, 0.71f, 1, 0, 2, -32, -10) }, {} },
            { "Bombo: punch y sin barro", { B (3, 28, 0, 0.71f, 5), B (0, 60, 3, 1.2f), B (0, 350, -4, 1.4f, 1, 0, 1, -26, -4), B (0, 3500, 3, 1.5f) }, {} },
            { "Bajo / 808 controlado", { B (3, 25, 0, 0.71f, 7), B (1, 90, 0, 0.71f, 1, 0, 1, -18, -5), B (0, 220, -2, 1.0f) }, {} },
            { "Guitarra acustica", { B (3, 80, 0, 0.71f, 3), B (0, 220, -3, 1.0f), B (0, 2800, 0, 1.0f, 1, 0, 2, -30, -6), B (2, 10000, 2, 0.71f) }, {} },
            { "Master: brillo suave (M/S)", { B (3, 22, 0, 0.71f, 4), B (2, 12000, 1.5f, 0.71f, 1, 4), B (0, 180, -1, 0.8f, 1, 4), B (0, 3000, 0, 0.8f, 1, 0, 2, -24, -3) }, { { "mode", 1 }, { "character", 1 } } },
            { "Bus calido (Warm)", { B (1, 110, 1.5f, 0.71f), B (2, 9000, 1.0f, 0.71f) }, { { "character", 2 } } } };
        return p;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout SpqProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;
    auto fl = [&ps] (const String& id, const String& name, NormalisableRange<float> r, float d, std::function<String (float, int)> txt)
    { ps.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, d, AudioParameterFloatAttributes().withStringFromValueFunction (txt))); };
    auto logR = [] (float lo, float hi) { return NormalisableRange<float> (lo, hi, [] (float a, float b, float t) { return a * std::pow (b / a, t); }, [] (float a, float b, float v) { return std::log (v / a) / std::log (b / a); }); };
    auto hz = [] (float v, int) { return v >= 1000 ? String (v / 1000, v >= 10000 ? 1 : 2) + " kHz" : String (v, 1) + " Hz"; };
    auto db = [] (float v, int) { return (v > 0 ? "+" : "") + String (v, 2) + " dB"; };
    auto ms = [] (float v, int) { return v < 10 ? String (v, 2) + " ms" : String (v, 0) + " ms"; };
    const float defF[] { 60, 100, 160, 250, 400, 630, 1000, 1600, 2500, 4000, 6300, 10000 };
    for (int i = 0; i < numBands; ++i)
    {
        const String n = "Band " + String (i + 1) + " ";
        ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { pid (i, "on"), 1 }, n + "On", false));
        ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { pid (i, "type"), 1 }, n + "Type", typeNames, 0));
        fl (pid (i, "freq"), n + "Freq", logR (10.0f, 30000.0f), defF[i % 12], hz);
        fl (pid (i, "gain"), n + "Gain", { -30.0f, 30.0f, 0.01f }, 0.0f, db);
        fl (pid (i, "q"), n + "Q", logR (0.025f, 40.0f), 1.0f, [] (float v, int) { return String (v, 2); });
        ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { pid (i, "slope"), 1 }, n + "Slope", slopeNames(), 1));
        ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { pid (i, "place"), 1 }, n + "Stereo", placeNames, 0));
        ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { pid (i, "dmode"), 1 }, n + "Dynamics", StringArray { "Off", "Dynamic", "Spectral" }, 0));
        fl (pid (i, "thr"), n + "Threshold", { -80.0f, 0.0f, 0.01f }, -24.0f, db);
        fl (pid (i, "range"), n + "Range", { -30.0f, 30.0f, 0.01f }, -6.0f, db);
        fl (pid (i, "att"), n + "Attack", logR (0.1f, 500.0f), 10.0f, ms);
        fl (pid (i, "rel"), n + "Release", logR (5.0f, 5000.0f), 120.0f, ms);
        ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { pid (i, "below"), 1 }, n + "Below", false));
    }
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Phase Mode", StringArray { "Zero Latency", "Linear Phase Low", "Linear Phase High" }, 0));
    ps.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "character", 1 }, "Character", StringArray { "Clean", "Subtle", "Warm" }, 0));
    fl ("output", "Output", { -36.0f, 36.0f, 0.01f }, 0.0f, db);
    ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { "autogain", 1 }, "Auto Gain", false));
    fl ("scale", "Gain Scale", { 0.0f, 200.0f, 0.1f }, 100.0f, [] (float v, int) { return String (v, 0) + " %"; });
    ps.push_back (std::make_unique<AudioParameterBool> (ParameterID { "scext", 1 }, "External Sidechain", false));
    return { ps.begin(), ps.end() };
}

SpqProcessor::SpqProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                                       .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, &undo, "TopSpectra", createLayout())
{
    anaPre.assign (anaSize, 0.0f); anaPost.assign (anaSize, 0.0f);
    for (auto& d : dynNow) d.store (0.0f);
    for (auto& d : specNow) d.store (0.0f);
}

bool SpqProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (in != out || (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo())) return false;
    if (l.inputBuses.size() > 1) { const auto sc = l.inputBuses[1]; if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo()) return false; }
    return true;
}

spq::Band SpqProcessor::bandParams (int i) const
{
    auto f = [&] (const char* w) { return apvts.getRawParameterValue (pid (i, w))->load(); };
    Band b;
    b.on = f ("on") > 0.5f; b.type = juce::jlimit (0, NumTypes - 1, (int) std::lround (f ("type")));
    b.f = f ("freq"); b.g = f ("gain") * apvts.getRawParameterValue ("scale")->load() * 0.01; b.q = f ("q");
    b.slope = (int) std::lround (f ("slope")); b.place = (int) std::lround (f ("place"));
    const int dm = (int) std::lround (f ("dmode"));
    b.dyn = dm == DynBand; b.spectral = dm == DynSpectral;
    b.thr = f ("thr"); b.range = f ("range"); b.att = f ("att"); b.rel = f ("rel"); b.below = f ("below") > 0.5f;
    return b;
}
int SpqProcessor::dynModeOf (int i) const { return (int) std::lround (apvts.getRawParameterValue (pid (i, "dmode"))->load()); }

void SpqProcessor::prepareToPlay (double rate, int block)
{
    sr = rate; blockMax = block;
    int r = 1; while (48000.0 * r * 1.5 < sr) r *= 2;                       // 44.1/48 -> 1, 88.2/96 -> 2, 176/192 -> 4
    hopBase = 512 * r;
    fir.prepare (16384 * r, 2048 * r);
    spec.prepare (2048 * r);
    const int maxBins = 16384 * r / 2 + 1;
    for (auto* v : { &gSt, &gL, &gR, &gM, &gS, &mP, &mQ, &mU, &mV }) v->assign ((size_t) maxBins, 1.0);
    envBuf.assign ((size_t) spec.size() / 2 + 1, 0.0); wBuf.assign ((size_t) spec.size() / 2 + 1, 0.0); prefix.assign ((size_t) spec.size() / 2 + 2, 0.0);
    for (auto& b : bands)
    {
        b.curve.assign ((size_t) maxBins, 1.0); b.spGain.assign ((size_t) spec.size() / 2 + 1, 0.0);
        b.valid = false; b.gApplied = 1e9; b.dyn = b.dynT = 0; b.env = 0; b.curveValid = false;
        for (auto& c : b.sec) for (auto& s : c) s.reset();
        b.det.reset();
    }
    delayL.assign ((size_t) (16384 * r / 2 + 2048 * r + 16), 0.0f); delayR = delayL; delayW = 0;
    curMode = -1; curSpec = false; firOn = false;
    for (auto& s : soloS) s.reset();
    outLin = outLinT = 1; for (int c = 0; c < 2; ++c) { charLp[c] = chX1[c] = dcX[c] = dcY[c] = 0; }
    masksDirty = true;
    configure ((int) std::lround (apvts.getRawParameterValue ("mode")->load()), false);
}

void SpqProcessor::configure (int mode, bool spectralNeeded)
{
    curMode = mode; curSpec = spectralNeeded;
    const int r = hopBase / 512;
    firOn = mode != ZeroLatency || spectralNeeded;
    // salto: 512 (Low y espectral) / 2048 en High (nucleo de 16384: mismo detalle, 4 veces menos CPU)
    if (firOn) fir.configure ((mode == LinearHigh ? 16384 : mode == LinearLow ? 4096 : 2048) * r, (mode == LinearHigh ? 2048 : 512) * r);
    for (auto& b : bands) { b.curveValid = false; std::fill (b.spGain.begin(), b.spGain.end(), 0.0); }
    latencyNow = firOn ? fir.latency() : 0;
    setLatencySamples (latencyNow);
    masksDirty = true;
}

void SpqProcessor::designIIR (BandRT& b, double g, bool warm)
{
    const int n = analogSections (b.p, g, b.as);
    if (n != b.n) { for (auto& c : b.sec) for (auto& s : c) s.reset(); warm = false; }
    for (int i = 0; i < n; ++i)
    {
        const Biquad d = digitalize (b.as[i], sr, warm ? &b.dig[i] : nullptr);
        b.dig[i] = d; b.sec[0][i].c = d; b.sec[1][i].c = d;
    }
    b.n = n; b.gApplied = g;
}

// peso de la region de cada banda para la dinamica espectral
void SpqProcessor::bandWeight (const Band& p, std::vector<double>& w) const
{
    const int bins = spec.size() / 2 + 1;
    const double bw = 2.0 * std::asinh (1.0 / (2.0 * std::max (0.05, p.q))) / std::log (2.0);
    for (int k = 0; k < bins; ++k)
    {
        const double f = std::max (1.0, k * sr / spec.size()), x = f / p.f;
        double v;
        switch (p.type)
        {
            case LowShelf: case LowCut: v = 1.0 / (1.0 + std::pow (x, 4.0)); break;
            case HighShelf: case HighCut: v = std::pow (x, 4.0) / (1.0 + std::pow (x, 4.0)); break;
            case TiltShelf: case FlatTilt: case AllPass: v = 1.0; break;
            default: { const double d = std::log2 (x) / std::max (0.08, 0.6 * bw); v = std::exp (-d * d * d * d); }
        }
        w[(size_t) k] = (f < 15.0 ? 0.0 : v);
    }
}

void SpqProcessor::buildMasks()
{
    const int K = fir.kernelLength(), bins = K / 2 + 1;
    for (auto* v : { &gSt, &gL, &gR, &gM, &gS }) std::fill (v->begin(), v->begin() + bins, 1.0);
    std::vector<double>* grp[5] { &gSt, &gL, &gR, &gM, &gS };
    bool hasMS = false, hasLR = false;
    const bool lin = curMode != ZeroLatency;
    for (auto& b : bands)
    {
        if (! b.p.on) continue;
        auto& g = *grp[std::clamp (b.p.place, 0, 4)];
        if (lin)
        {
            const double gg = b.p.g + b.dyn;
            if (! b.curveValid || std::abs (gg - b.curveG) > 0.02)
            {
                ASec as[maxSec]; const int n = analogSections (b.p, gg, as);
                for (int k = 0; k < bins; ++k) b.curve[(size_t) k] = analogMag (as, n, std::max (0.5, k * sr / K));
                b.curveG = gg; b.curveValid = true;
            }
            for (int k = 0; k < bins; ++k) g[(size_t) k] *= b.curve[(size_t) k];
            if (b.p.place >= MidCh && (b.p.type != AllPass)) hasMS = true;
            if (b.p.place == LeftCh || b.p.place == RightCh) hasLR = true;
        }
        if (b.p.spectral)
        {
            const int sb = spec.size() / 2;
            for (int k = 0; k < bins; ++k)
            {
                const double pos = (double) k * sb / (K / 2);                    // misma frecuencia en la rejilla del analisis
                const int i0 = std::min (sb - 1, (int) pos); const double t = pos - i0;
                const double dB = b.spGain[(size_t) i0] + (b.spGain[(size_t) i0 + 1] - b.spGain[(size_t) i0]) * t;
                if (dB != 0.0) g[(size_t) k] *= std::pow (10.0, dB / 20.0);
            }
            if (b.p.place >= MidCh) hasMS = true;
            if (b.p.place == LeftCh || b.p.place == RightCh) hasLR = true;
        }
    }
    if (! hasMS)
    {
        for (int k = 0; k < bins; ++k) { mP[(size_t) k] = gSt[(size_t) k] * gL[(size_t) k]; mV[(size_t) k] = gSt[(size_t) k] * gR[(size_t) k]; }
        if (hasLR) fir.design (mP.data(), nullptr, nullptr, mV.data());
        else fir.design (mP.data(), nullptr, nullptr, mP.data());
    }
    else
    {
        for (int k = 0; k < bins; ++k)
        {
            const double st = gSt[(size_t) k], a = 0.5 * (gM[(size_t) k] + gS[(size_t) k]), d = 0.5 * (gM[(size_t) k] - gS[(size_t) k]);
            mP[(size_t) k] = gL[(size_t) k] * st * a; mQ[(size_t) k] = gL[(size_t) k] * st * d;
            mU[(size_t) k] = gR[(size_t) k] * st * d; mV[(size_t) k] = gR[(size_t) k] * st * a;
        }
        if (hasLR) fir.design (mP.data(), mQ.data(), mU.data(), mV.data());
        else fir.design (mP.data(), mQ.data(), mQ.data(), mP.data());
    }
}

void SpqProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    auto mainIn = getBusBuffer (buffer, true, 0);
    const int n = buffer.getNumSamples(), chs = std::min (2, mainIn.getNumChannels());
    for (int c = chs; c < getTotalNumOutputChannels(); ++c) buffer.clear (c, 0, n);
    if (n == 0 || chs == 0) return;
    const bool scExt = apvts.getRawParameterValue ("scext")->load() > 0.5f && getBusCount (true) > 1 && getBus (true, 1)->isEnabled();
    auto scBuf = scExt ? getBusBuffer (buffer, true, 1) : mainIn;
    const int scCh = scBuf.getNumChannels();

    // ---- parametros
    const int mode = (int) std::lround (apvts.getRawParameterValue ("mode")->load());
    bool anySpectral = false, anyMS = false;
    for (int i = 0; i < numBands; ++i)
    {
        auto& b = bands[(size_t) i];
        const Band p = bandParams (i);
        // cambios de estructura: inmediatos. FREQ / GAIN / Q: se deslizan (~25 ms) para no hacer clics
        const bool structural = ! b.valid || p.on != b.p.on || p.type != b.p.type || p.slope != b.p.slope || p.place != b.p.place || p.dyn != b.p.dyn || p.spectral != b.p.spectral;
        if (structural && b.valid && p.type != b.p.type) { for (auto& c : b.sec) for (auto& s : c) s.reset(); }
        if (structural)
        {
            const bool wasOn = b.valid && b.p.on;
            b.p = p;
            if (! wasOn) { b.tgt = p; }                                        // banda nueva: arranca en su lugar
            b.gApplied = 1e9; b.curveValid = false; masksDirty = true;
            b.det.c = bandpassAt (p.f, p.q, sr);
        }
        else { b.p.thr = p.thr; b.p.range = p.range; b.p.att = p.att; b.p.rel = p.rel; b.p.below = p.below; }
        b.tgt = p; b.valid = true;
        if (! p.dyn) { b.dyn = 0; b.dynT = 0; }
        if (p.on && p.spectral) anySpectral = true;
        if (p.on && p.place >= MidCh) anyMS = true;
        if (! p.on || ! p.spectral) std::fill (b.spGain.begin(), b.spGain.end(), 0.0);
    }
    (void) anyMS;
    if (mode != curMode || (mode == ZeroLatency && anySpectral != curSpec)) configure (mode, anySpectral);
    const bool lin = curMode != ZeroLatency;
    const int ch = (int) std::lround (apvts.getRawParameterValue ("character")->load());
    // ganancia de salida + auto gain (promedio de la curva estatica estereo en escala log 30 Hz - 16 kHz)
    double ag = 0.0;
    if (apvts.getRawParameterValue ("autogain")->load() > 0.5f)
    {
        double acc = 0; int cnt = 0;
        for (int k = 0; k < 48; ++k)
        {
            const double f = 30.0 * std::pow (16000.0 / 30.0, k / 47.0);
            double m = 1.0;
            for (auto& b : bands) if (b.p.on && (b.p.place == Stereo || b.p.place == MidCh)) { ASec as[maxSec]; const int ns = analogSections (b.p, b.p.g, as); m *= analogMag (as, ns, f); }
            acc += 20.0 * std::log10 (std::max (1e-6, m)); ++cnt;
        }
        ag = -acc / cnt;
    }
    autoGainDb.store ((float) ag);
    outLinT = std::pow (10.0, (apvts.getRawParameterValue ("output")->load() + ag) / 20.0);
    const double aOut = 1.0 - std::exp (-1.0 / (0.03 * sr));
    const int soloB = solo.load();
    if (soloB >= 0 && soloB < numBands) { const auto& p = bands[(size_t) soloB].p; for (auto& s : soloS) s.c = bandpassAt (p.f, std::max (0.5, p.q), sr); }

    // ---- coeficientes de la envolvente de cada banda
    double ka[numBands], kr[numBands];
    for (int i = 0; i < numBands; ++i)
    {
        ka[i] = 1.0 - std::exp (-1.0 / (bands[(size_t) i].p.att * 0.001 * sr));
        kr[i] = 1.0 - std::exp (-1.0 / (bands[(size_t) i].p.rel * 0.001 * sr));
    }
    const double kDyn = 1.0 - std::exp (-1.0 / (0.002 * sr));                // ganancia dinamica sin escalones
    const double kPar = 1.0 - std::exp (-1.0 / (0.025 * sr));                // perillas
    constexpr int sub = 32;

    float* L = buffer.getWritePointer (0);
    float* R = chs > 1 ? buffer.getWritePointer (1) : nullptr;
    int aw = anaW.load();
    for (int i0 = 0; i0 < n; i0 += sub)
    {
        const int i1 = std::min (n, i0 + sub);
        // ---- (cada 32 muestras) ganancia dinamica y secciones IIR de las bandas que se mueven
        for (int bi = 0; bi < numBands; ++bi)
        {
            auto& b = bands[(size_t) bi];
            if (! b.p.on) continue;
            {
                // deslizamiento de FREQ (log), GAIN y Q (log)
                const double k = 1.0 - std::pow (1.0 - kPar, (double) (i1 - i0));
                const double lf = std::log (b.p.f), lq = std::log (b.p.q), tf = std::log (b.tgt.f), tq = std::log (b.tgt.q);
                bool moved = false;
                auto glide = [&] (double cur, double tgt, double eps, double& out) { if (std::abs (tgt - cur) > eps) { out = cur + (tgt - cur) * k; if (std::abs (tgt - out) <= eps) out = tgt; moved = true; } };
                double nf = lf, nq = lq, ng = b.p.g;
                glide (lf, tf, 1e-5, nf); glide (lq, tq, 1e-5, nq); glide (b.p.g, b.tgt.g, 1e-4, ng);
                if (moved)
                {
                    b.p.f = std::exp (nf); b.p.q = std::exp (nq); b.p.g = ng;
                    b.gApplied = 1e9; b.curveValid = false; masksDirty = true;
                    b.det.c = bandpassAt (b.p.f, b.p.q, sr);
                }
            }
            if (b.p.dyn)
            {
                const double lvl = 20.0 * std::log10 (b.env + 1e-9);
                double x = b.p.below ? (b.p.thr - lvl) : (lvl - b.p.thr);
                const double amt = x < -3.0 ? 0.0 : (x > 3.0 ? x : (x + 3.0) * (x + 3.0) / 12.0);
                b.dynT = std::copysign (std::min (std::abs (b.p.range), 0.75 * amt), b.p.range);
            }
            b.dyn += (b.dynT - b.dyn) * (1.0 - std::pow (1.0 - kDyn, (double) (i1 - i0)));
            dynNow[(size_t) bi].store ((float) b.dyn);
            if (! lin)
            {
                const double g = b.p.g + b.dyn;
                if (std::abs (g - b.gApplied) > 0.01) designIIR (b, g, b.n > 0);
            }
            else if (std::abs (b.p.g + b.dyn - b.curveG) > 0.02) masksDirty = true;
        }
        for (int i = i0; i < i1; ++i)
        {
            double l = L[i], r = R != nullptr ? R[i] : l;
            const double inMono = 0.5 * (l + r);
            // ---- deteccion (entrada o sidechain externo) por banda
            const double sl = scBuf.getSample (0, i), srr = scCh > 1 ? scBuf.getSample (1, i) : sl;
            for (auto& b : bands)
            {
                if (! b.p.on || ! b.p.dyn) continue;
                const double src = b.p.place == LeftCh ? sl : b.p.place == RightCh ? srr : b.p.place == SideCh ? 0.5 * (sl - srr) : 0.5 * (sl + srr);
                const double a = std::abs (b.det.process (src));
                const int idx = (int) (&b - bands.data());
                b.env += (a - b.env) * (a > b.env ? ka[idx] : kr[idx]);
            }
            if (firOn) spec.push (0.5 * (sl + srr), 0.5 * (sl - srr));
            // ---- EQ minima fase (Zero Latency)
            if (! lin)
            {
                for (auto& b : bands)
                {
                    if (! b.p.on || b.n == 0) continue;
                    switch (b.p.place)
                    {
                        case Stereo: for (int k = 0; k < b.n; ++k) { l = b.sec[0][k].process (l); r = b.sec[1][k].process (r); } break;
                        case LeftCh: for (int k = 0; k < b.n; ++k) l = b.sec[0][k].process (l); break;
                        case RightCh: for (int k = 0; k < b.n; ++k) r = b.sec[1][k].process (r); break;
                        case MidCh: { double m = 0.5 * (l + r); const double s = 0.5 * (l - r); for (int k = 0; k < b.n; ++k) m = b.sec[0][k].process (m); l = m + s; r = m - s; } break;
                        case SideCh: { const double m = 0.5 * (l + r); double s = 0.5 * (l - r); for (int k = 0; k < b.n; ++k) s = b.sec[0][k].process (s); l = m + s; r = m - s; } break;
                        default: break;
                    }
                }
            }
            // ---- motor de fase lineal / espectral
            if (firOn)
            {
                if (fir.atHopStart())
                {
                    if (anySpectral)
                    {
                        spec.analyze();
                        const int bins = spec.size() / 2 + 1;
                        const double hopSec = (double) fir.hop() / sr;
                        for (auto& b : bands)
                        {
                            if (! b.p.on || ! b.p.spectral) continue;
                            const auto& lv = b.p.place == SideCh ? spec.dbB : spec.dbA;
                            // envolvente espectral: promedio en dB en +-1/6 de octava (lo "normal" alrededor de cada bin)
                            prefix[0] = 0; for (int k = 0; k < bins; ++k) prefix[(size_t) k + 1] = prefix[(size_t) k] + lv[(size_t) k];
                            for (int k = 0; k < bins; ++k)
                            {
                                const int a = std::max (0, std::min (k - 2, (int) (k * 0.891))), c = std::min (bins - 1, std::max (k + 2, (int) (k * 1.122) + 1));
                                envBuf[(size_t) k] = (prefix[(size_t) c + 1] - prefix[(size_t) a]) / (c - a + 1);
                            }
                            bandWeight (b.p, wBuf);
                            const double ca = 1.0 - std::exp (-hopSec / std::max (0.001, b.p.att * 0.001 * 4.0)), cr = 1.0 - std::exp (-hopSec / (b.p.rel * 0.001));
                            const double rng = b.p.range, mag = std::abs (rng);
                            for (int k = 0; k < bins; ++k)
                            {
                                const double ref = b.p.below ? std::min (b.p.thr, envBuf[(size_t) k] - 3.0) : std::max (b.p.thr, envBuf[(size_t) k] + 3.0);
                                const double over = b.p.below ? ref - lv[(size_t) k] : lv[(size_t) k] - ref;
                                const double tgt = std::copysign (std::clamp (over * 0.8, 0.0, mag), rng) * wBuf[(size_t) k];
                                double& gk = b.spGain[(size_t) k];
                                gk += (tgt - gk) * (std::abs (tgt) > std::abs (gk) ? ca : cr);
                            }
                        }
                        masksDirty = true;
                    }
                    if (masksDirty) { buildMasks(); masksDirty = false; }
                }
                double ol, orr; fir.push (l, r, ol, orr); l = ol; r = orr;
            }
            // ---- caracter (despues de la EQ)
            if (ch > 0)
            {
                double* io[2] { &l, &r };
                for (int c = 0; c < 2; ++c)
                {
                    double& x = *io[c];
                    if (ch == 1)                                                // Subtle: trafo (graves saturan mas)
                    {
                        charLp[c] += (x - charLp[c]) * 0.033 * 48000.0 / sr;
                        x = x - charLp[c] + Sat::adaa (charLp[c], chX1[c], 2.0, 0.012);
                    }
                    else x = Sat::adaa (x, chX1[c], 1.1, 0.03);                 // Warm: valvula (par + impar)
                    const double y = x - dcX[c] + 0.9995 * dcY[c]; dcX[c] = x; dcY[c] = y; x = y;
                }
            }
            outLin += (outLinT - outLin) * aOut;
            l *= outLin; r *= outLin;
            // ---- solo de banda: se escucha solo esa region (alineado con la latencia)
            if (soloB >= 0)
            {
                const int D = (int) delayL.size();
                delayL[(size_t) delayW] = (float) soloS[0].process (L[i]); delayR[(size_t) delayW] = (float) soloS[1].process (R != nullptr ? R[i] : L[i]);
                const int rd = (delayW - latencyNow + D) % D;
                l = delayL[(size_t) rd]; r = delayR[(size_t) rd];
                delayW = (delayW + 1) % D;
            }
            anaPre[(size_t) aw] = (float) inMono; anaPost[(size_t) aw] = (float) (0.5 * (l + r)); aw = (aw + 1) & (anaSize - 1);
            L[i] = (float) l; if (R != nullptr) R[i] = (float) r;
        }
    }
    anaW.store (aw);
    // reduccion espectral para la pantalla (estereo/mid, 256 puntos log 20 Hz - 20 kHz)
    if (anySpectral)
        for (int k = 0; k < dispPts; ++k)
        {
            const double f = 20.0 * std::pow (1000.0, k / (double) (dispPts - 1)), pos = f * spec.size() / sr;
            const int i0 = std::min (spec.size() / 2 - 1, (int) pos);
            double t = 0; for (auto& b : bands) if (b.p.on && b.p.spectral) t += b.spGain[(size_t) i0];
            specNow[(size_t) k].store ((float) t);
        }
    else for (auto& v : specNow) v.store (0.0f);
}

int SpqProcessor::getNumPrograms() { return (int) presets().size(); }
const juce::String SpqProcessor::getProgramName (int i) { return juce::isPositiveAndBelow (i, getNumPrograms()) ? presets()[(size_t) i].name : ""; }
void SpqProcessor::setCurrentProgram (int i)
{
    if (! juce::isPositiveAndBelow (i, getNumPrograms())) return;
    currentProgram = i;
    for (auto* prm : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm)) r->setValueNotifyingHost (r->getDefaultValue());
    const auto& p = presets()[(size_t) i];
    for (size_t b = 0; b < p.bands.size(); ++b)
        for (auto& [id, v] : p.bands[b])
            if (auto* r = apvts.getParameter (pid ((int) b, id))) r->setValueNotifyingHost (r->convertTo0to1 (v));
    for (auto& [id, v] : p.glob) if (auto* r = apvts.getParameter (id)) r->setValueNotifyingHost (r->convertTo0to1 (v));
}
void SpqProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto st = apvts.copyState(); st.setProperty ("program", currentProgram, nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary (*xml, dest);
}
void SpqProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto t = juce::ValueTree::fromXml (*xml);
        if (t.isValid() && t.hasType (apvts.state.getType())) { currentProgram = (int) t.getProperty ("program", 0); apvts.replaceState (t); }
    }
}
juce::AudioProcessorEditor* SpqProcessor::createEditor() { return new SpqEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SpqProcessor(); }
