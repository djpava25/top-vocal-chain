#include "ChoirEngine.h"

namespace choir
{
const std::vector<ModelInfo>& models()
{
    //                   nombre        voces detune timing vibrato formante reverb decay tipo
    static const std::vector<ModelInfo> m {
        { "Gospel",          6,   12.0f, 26.0f, 0.25f, 0.9f, 0.24f, 2.4f, 2 },
        { "Pop",             4,    7.0f, 12.0f, 0.10f, 0.6f, 0.16f, 1.6f, 4 },
        { "Clasico",         8,   15.0f, 34.0f, 0.45f, 1.1f, 0.34f, 3.2f, 2 },
        { "Urbano",          3,    5.0f,  8.0f, 0.05f, 0.5f, 0.10f, 1.1f, 0 },
        { "Etereo",          8,   20.0f, 42.0f, 0.30f, 1.3f, 0.45f, 4.5f, 6 },
        { "Iglesia",        10,   14.0f, 38.0f, 0.35f, 1.0f, 0.40f, 4.0f, 3 },
    };
    return m;
}
juce::StringArray modelNames() { juce::StringArray s; for (auto& m : models()) s.add (m.name); return s; }
const juce::StringArray& micNames()
{
    static const juce::StringArray s { "Tubo Clasico", "Condensador Estudio", "Ribbon", "Dinamico Broadcast", "Valvula Vintage" };
    return s;
}

// --- tono por muestra (periodo en muestras, 0 = sin voz) y nota cantada
struct Track { std::vector<float> period, midi; };

static Track trackPitch (const std::vector<float>& mono, double sr)
{
    Track t; t.period.assign (mono.size(), 0.0f); t.midi.assign (mono.size(), 0.0f);
    tvc::PitchDetector det; det.prepare (sr);
    float curP = 0.0f, curM = 0.0f;
    std::vector<float> rawP (mono.size(), 0.0f), rawM (mono.size(), 0.0f);
    for (size_t i = 0; i < mono.size(); ++i)
    {
        if (det.push (mono[i]))
        {
            if (det.isVoiced() && det.getFrequency() > 60.0f) { curP = (float) (sr / det.getFrequency()); curM = 69.0f + 12.0f * std::log2 (det.getFrequency() / 440.0f); }
            else { curP = 0.0f; }
        }
        rawP[i] = curP; rawM[i] = curP > 0 ? curM : 0.0f;
    }
    // el detector mira ~46 ms hacia atras: se adelanta media ventana para alinear la nota con el audio
    const size_t shift = (size_t) (sr * 0.023);
    for (size_t i = 0; i < mono.size(); ++i)
    {
        const size_t j = std::min (mono.size() - 1, i + shift);
        t.period[i] = rawP[j]; t.midi[i] = rawM[j];
    }
    return t;
}

// Intervalo diatonico: cuantos semitonos hay que mover la nota cantada para subir/bajar 'steps' grados en la escala
static float diatonicShift (float midi, int key, int scale, int steps)
{
    const auto& mask = tvc::Scales::mask (scale);
    const int base = (int) std::round (midi);
    // nota de la escala mas cercana a lo cantado
    int note = base;
    for (int d = 0; d < 7; ++d)
    {
        if (mask[(size_t) (((base + d) - key + 120) % 12)]) { note = base + d; break; }
        if (mask[(size_t) (((base - d) - key + 120) % 12)]) { note = base - d; break; }
    }
    int target = note, moved = 0;
    const int dir = steps > 0 ? 1 : -1;
    while (moved < std::abs (steps)) { target += dir; if (mask[(size_t) ((target - key + 120) % 12)]) ++moved; }
    return (float) (target - note);
}

juce::AudioBuffer<float> render (const juce::AudioBuffer<float>& voice, double sr, const Settings& s,
                                 std::atomic<float>& progress, const std::atomic<bool>& cancel)
{
    const int n = voice.getNumSamples();
    std::vector<float> mono ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        float v = 0; for (int c = 0; c < voice.getNumChannels(); ++c) v += voice.getSample (c, i);
        mono[(size_t) i] = v / (float) std::max (1, voice.getNumChannels());
    }
    progress = 0.02f;
    const auto tr = trackPitch (mono, sr);
    progress = 0.08f;

    const auto& M = models()[(size_t) juce::jlimit (0, (int) models().size() - 1, s.model)];
    // partes: (tipo, grados o semitonos)
    struct Part { bool diatonic; int amount; float gain; };
    std::vector<Part> parts;
    if (s.includeUnison) parts.push_back ({ false, 0, 1.0f });
    if (s.octUp)     parts.push_back ({ false, 12, 0.7f });
    if (s.octDown)   parts.push_back ({ false, -12, 0.8f });
    if (s.thirdUp)   parts.push_back ({ true, 2, 0.85f });
    if (s.thirdDown) parts.push_back ({ true, -2, 0.85f });
    if (s.fifthUp)   parts.push_back ({ true, 4, 0.8f });
    if (s.fifthDown) parts.push_back ({ true, -4, 0.8f });

    const int tail = (int) (sr * (M.decay + 0.5));
    juce::AudioBuffer<float> out (2, n + tail);
    out.clear();
    juce::Random rnd (1234 + s.model * 77);
    const int totalVoices = (int) parts.size() * M.voices;
    int done = 0;
    const int lat = [&] { toptune::Psola p; p.prepare (sr); return p.getLatency(); }();

    for (const auto& part : parts)
        for (int v = 0; v < M.voices; ++v)
        {
            if (cancel.load()) return {};
            toptune::Psola ps; ps.prepare (sr);
            // cada cantante es distinto: afinacion, tiempo, formante, volumen, vibrato y lugar en el estereo
            const float det = (rnd.nextFloat() * 2.0f - 1.0f) * M.detuneCents;
            const int delay = (int) (sr * 0.001 * (3.0 + rnd.nextFloat() * M.timingMs));
            const float formant = std::pow (2.0f, (rnd.nextFloat() * 2.0f - 1.0f) * M.formantSpread / 12.0f) * (part.amount >= 12 ? 0.94f : part.amount <= -12 ? 1.06f : 1.0f);
            const float gain = part.gain * (0.75f + 0.25f * rnd.nextFloat()) / std::sqrt ((float) M.voices);
            const float vibRate = 4.8f + rnd.nextFloat() * 1.4f, vibPhase = rnd.nextFloat() * 6.28f;
            const float driftRate = 0.15f + rnd.nextFloat() * 0.3f, driftPhase = rnd.nextFloat() * 6.28f;
            const float pan = s.stereo ? (M.voices == 1 ? 0.0f : -0.85f + 1.7f * ((v + 0.5f) / (float) M.voices) + (rnd.nextFloat() - 0.5f) * 0.15f) : 0.0f;
            const float gl = gain * std::cos ((pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f) * 1.414f;
            const float gr = gain * std::sin ((pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f) * 1.414f;
            float lastShift = 0.0f;
            for (int i = 0; i < n + lat; ++i)
            {
                const int src = i - delay;
                float x = (src >= 0 && src < n) ? mono[(size_t) src] : 0.0f, dummy = 0.0f;
                const int ti = juce::jlimit (0, n - 1, src);
                const float period = tr.period[(size_t) ti];
                float semis = (float) part.amount;
                if (part.diatonic)
                {
                    if (period > 0.0f) lastShift = diatonicShift (tr.midi[(size_t) ti], s.key, s.scale, part.amount);
                    semis = lastShift;
                }
                const float t = (float) i / (float) sr;
                const float cents = det + 18.0f * M.vibrato * std::sin (6.2832f * vibRate * t + vibPhase) + 6.0f * std::sin (6.2832f * driftRate * t + driftPhase);
                const double ratio = std::pow (2.0, (semis + cents / 100.0) / 12.0);
                ps.process (x, dummy, false, period, ratio, formant);
                const int o = i - lat;
                if (o >= 0 && o < out.getNumSamples())
                {
                    out.addSample (0, o, x * gl);
                    out.addSample (1, o, x * gr);
                }
            }
            progress = 0.08f + 0.80f * (float) (++done) / (float) std::max (1, totalVoices);
        }

    // --- MICROFONO (color de EQ)
    for (int c = 0; c < 2; ++c)
    {
        tvc::Biquad a, b, d, e;
        switch (s.mic)
        {
            case 0:  a.setLowShelf (sr, 180, 1.5f); b.setPeak (sr, 5000, 0.8, 2.0f); d.setHighShelf (sr, 12000, 1.5f); e.setHighPass (sr, 70, 0.707); break;   // tubo clasico
            case 1:  a.setIdentity(); b.setPeak (sr, 4500, 0.8, 1.0f); d.setHighShelf (sr, 10000, 1.0f); e.setHighPass (sr, 60, 0.707); break;              // condensador
            case 2:  a.setLowShelf (sr, 200, 2.0f); b.setPeak (sr, 3000, 0.7, -1.0f); d.setHighShelf (sr, 7000, -6.0f); e.setHighPass (sr, 50, 0.707); break;  // ribbon (oscuro)
            case 3:  a.setLowShelf (sr, 150, -2.0f); b.setPeak (sr, 5000, 1.0, 4.0f); d.setHighShelf (sr, 9000, -3.0f); e.setHighPass (sr, 120, 0.707); break; // dinamico
            default: a.setPeak (sr, 220, 0.9, 2.0f); b.setPeak (sr, 2500, 0.8, 1.0f); d.setHighShelf (sr, 9000, -2.0f); e.setHighPass (sr, 70, 0.707); break;  // valvula vintage
        }
        float* d0 = out.getWritePointer (c);
        for (int i = 0; i < out.getNumSamples(); ++i) d0[i] = e.process (d.process (b.process (a.process (d0[i]))));
    }
    progress = 0.92f;
    // --- SALA
    {
        tvc::FdnReverb rev; rev.prepare (sr);
        rev.set (M.reverbType, 25.0f, M.decay, 0.7f, 0.45f, 0.5f, 0.75f, 180.0f, 9000.0f, 0.0f, s.stereo ? 1.0f : 0.0f);
        for (int i = 0; i < out.getNumSamples(); ++i)
        {
            float wl, wr;
            const float l = out.getSample (0, i), r = out.getSample (1, i);
            rev.process (l, r, wl, wr);
            out.setSample (0, i, l + wl * M.reverb);
            out.setSample (1, i, r + (s.stereo ? wr : wl) * M.reverb);
        }
    }
    // --- normaliza a -3 dBFS de pico
    const float pk = std::max (out.getMagnitude (0, 0, out.getNumSamples()), out.getMagnitude (1, 0, out.getNumSamples()));
    if (pk > 1.0e-6f) out.applyGain (0.708f / pk);
    progress = 1.0f;
    return out;
}

bool detectKey (const juce::AudioBuffer<float>& voice, double sr, int& root, bool& minor)
{
    std::vector<float> mono ((size_t) voice.getNumSamples());
    for (int i = 0; i < voice.getNumSamples(); ++i) mono[(size_t) i] = voice.getSample (0, i);
    const auto tr = trackPitch (mono, sr);
    float hist[12] {};
    for (size_t i = 0; i < tr.midi.size(); i += 64)
        if (tr.period[i] > 0.0f) hist[((int) std::round (tr.midi[i]) % 12 + 12) % 12] += 1.0f;
    float total = 0; for (float h : hist) total += h;
    if (total < 20) return false;
    float conf = 0; toptune::KeyFinder::find (hist, root, minor, conf);
    return true;
}
} // namespace choir
