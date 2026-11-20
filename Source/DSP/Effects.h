#pragma once
#include "Biquad.h"
#include "PitchCorrector.h"
#include <vector>
#include <cmath>
#include <algorithm>

namespace tvc
{
//==============================================================================
// Preamp con 8 modelos:
//   0 Clean       - transparente, solo ganancia limpia (recorte muy suave al final)
//   1 Tube        - valvula: armonicos pares calidos (Warmth = asimetria)
//   2 Vintage     - saturacion suave, cuerpo en medios-graves y agudos redondeados
//   3 Modern      - brillante y limpio: saturacion firme, graves ajustados, brillo arriba
//   4 Tape        - compresion suave y perdida de agudos de cinta
//   5 Transformer - satura sobre todo los graves ("hierro")
//   6 Transistor  - armonicos impares, filo y mordida
//   7 Diode       - asimetrico y agresivo
struct PreampStage
{
    int type = 0;
    float drive = 1.0f, bias = 0.0f, tbias = 0.0f, norm = 1.0f, trim = 1.0f, warmth = 0.0f;
    float dcX[2] {}, dcY[2] {}, tapeLp[2] {}, xfLow[2] {};
    float dcCoef = 0.9995f, tapeCoef = 0.5f, xfCoef = 0.01f;
    Biquad tone1[2], tone2[2];
    double osr = 88200.0;
    int lastType = -1; float lastWarmth = -1.0f, lastDrive = -100.0f;

    void prepare (double oversampledRate)
    {
        osr = oversampledRate;
        dcCoef = (float) std::exp (-2.0 * kPi * 10.0 / osr);
        xfCoef = 1.0f - (float) std::exp (-2.0 * kPi * 180.0 / osr);
        lastType = -1;
        reset();
    }

    void reset()
    {
        for (int c = 0; c < 2; ++c) { dcX[c] = dcY[c] = tapeLp[c] = xfLow[c] = 0.0f; tone1[c].reset(); tone2[c].reset(); }
    }

    void set (int newType, float driveDb, float warmth01, float trimDb) noexcept
    {
        type   = newType;
        drive  = dbToGain (driveDb);
        warmth = warmth01;
        bias   = 0.35f * warmth01;
        tbias  = std::tanh (bias);
        norm   = 1.0f / (std::sqrt (drive) * (1.0f - tbias * tbias));
        trim   = dbToGain (trimDb);

        // Filtros de "tono" de cada modelo (solo se recalculan si cambian)
        if (type != lastType || std::abs (warmth01 - lastWarmth) > 1.0e-3f || std::abs (driveDb - lastDrive) > 1.0e-3f)
        {
            lastType = type; lastWarmth = warmth01; lastDrive = driveDb;
            double fc = 20000.0;
            if (type == 4) fc = std::max (5000.0, 20000.0 - driveDb * 250.0 - warmth01 * 7000.0);     // cinta
            if (type == 2) fc = 15000.0 - warmth01 * 6000.0;                                          // vintage
            tapeCoef = 1.0f - (float) std::exp (-2.0 * kPi * fc / osr);
            for (int c = 0; c < 2; ++c)
            {
                if (type == 2) { tone1[c].setPeak (osr, 250.0, 0.8, 1.5 * warmth01); tone2[c].setIdentity(); }
                else if (type == 3) { tone1[c].setHighShelf (osr, 9000.0, 2.5 * warmth01 + 0.5); tone2[c].setHighPass (osr, 25.0, 0.707); }
                else { tone1[c].setIdentity(); tone2[c].setIdentity(); }
            }
        }
    }

    inline float process (float x, int ch) noexcept
    {
        const float u = drive * x;
        const float comp = 1.0f / std::sqrt (drive);
        float y;

        switch (type)
        {
            case 0: // Clean
            {
                const float a = std::abs (u);
                y = (a <= 0.8f ? u : std::copysign (0.8f + 0.2f * std::tanh ((a - 0.8f) / 0.2f), u)) * comp;
                break;
            }
            case 2: // Vintage
            {
                const float b = 0.1f * warmth;
                y = (std::tanh (u * 0.8f + b) - std::tanh (b)) / 0.8f * comp;
                tapeLp[ch] += tapeCoef * (y - tapeLp[ch]);
                y = tone1[ch].process (tapeLp[ch]);
                break;
            }
            case 3: // Modern
            {
                y = u / std::cbrt (1.0f + std::abs (u * u * u)) * comp;
                y = tone2[ch].process (tone1[ch].process (y));
                break;
            }
            case 4: // Tape
            {
                y = 0.63661977f * std::atan (u * 1.5707963f) * comp;
                tapeLp[ch] += tapeCoef * (y - tapeLp[ch]);
                y = tapeLp[ch];
                break;
            }
            case 5: // Transformer
            {
                xfLow[ch] += xfCoef * (x - xfLow[ch]);
                const float lowU = drive * xfLow[ch];
                const float k = 1.0f + 2.5f * warmth;
                const float v = (u - lowU) + std::tanh (lowU * k) / k;
                y = std::tanh (v * 0.8f) / 0.8f * comp;
                break;
            }
            case 6: // Transistor
            {
                const float a = u * (1.0f + 0.15f * warmth);
                y = a / std::pow (1.0f + std::pow (std::abs (a), 2.5f), 0.4f) * comp;
                break;
            }
            case 7: // Diode
                y = (u >= 0.0f ? 1.0f - std::exp (-u) : -(1.0f - std::exp (1.8f * u)) / 1.8f) * comp * 1.2f;
                break;
            default: // 1 Tube
                y = (std::tanh (u + bias) - tbias) * norm;
                break;
        }

        const float out = y - dcX[ch] + dcCoef * dcY[ch];
        dcX[ch] = y; dcY[ch] = out;
        return out * trim;
    }
};

//==============================================================================
// Compresor con 7 modos:
//   0 Clean VCA     - control preciso, respeta Attack y Release
//   1 Vintage Opto  - lento y musical, release de 2 etapas segun el programa
//   2 FET Punch     - muy rapido y agresivo, armonicos impares
//   3 Vari-Mu Glue  - el ratio sube con el nivel, pega la voz
//   4 LA Vocal      - opto mas rapido y denso, ideal para voces al frente
//   5 Fast          - ataque y release rapidisimos, controla picos
//   6 Smooth        - deteccion RMS lenta, compresion muy suave
struct VocalCompressor
{
    double sr = 44100.0;
    int type = 0;
    float thr = -18.0f, ratio = 4.0f, makeup = 1.0f, knee = 6.0f;
    float attCoef = 0.0f, relSamples = 1000.0f, rmsCoef = 0.0f;
    float rmsMs = 0.0f, grDb = 0.0f, charge = 0.0f, chargeCoef = 0.0f;
    float lastGr = 0.0f;   // dB de reduccion (para el vumetro)

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        chargeCoef = 1.0f - (float) std::exp (-1.0 / (1.5 * sr));
        reset();
    }
    void reset() { rmsMs = 0.0f; grDb = 0.0f; charge = 0.0f; lastGr = 0.0f; }

    void set (int newType, float thrDb, float r, float attMs, float relMs, float makeupDb, float kneeDb) noexcept
    {
        type = newType; thr = thrDb; ratio = std::max (1.0f, r); makeup = dbToGain (makeupDb);
        knee = std::max (0.01f, kneeDb);
        float att = attMs, rel = relMs, rmsMsTime = 0.0f;
        switch (type)
        {
            case 1:  att = std::max (attMs, 10.0f);                 rel = relMs;        rmsMsTime = 20.0f; break;
            case 2:  att = std::clamp (attMs * 0.1f, 0.02f, 2.0f);  rel = relMs * 0.5f; break;
            case 3:  att = std::max (attMs, 8.0f);                  rel = relMs * 1.5f; rmsMsTime = 10.0f; break;
            case 4:  att = std::max (attMs, 5.0f);                  rel = relMs;        rmsMsTime = 12.0f; break;
            case 5:  att = std::clamp (attMs * 0.05f, 0.02f, 1.0f); rel = relMs * 0.3f; break;
            case 6:  att = std::max (attMs, 20.0f);                 rel = relMs * 2.0f; rmsMsTime = 30.0f; break;
            case 7:  att = std::max (attMs, 3.0f);                  rel = relMs * 1.2f; rmsMsTime = 8.0f;  break;   // Tube
            case 8:  att = std::max (attMs, 0.3f);                  rel = relMs;        rmsMsTime = 4.0f;  break;   // Modern Digital
            default: break;
        }
        attCoef = (float) std::exp (-1.0 / (std::max (0.02f, att) * 0.001 * sr));
        relSamples = (float) (std::max (5.0f, rel) * 0.001 * sr);
        rmsCoef = rmsMsTime > 0.0f ? (float) std::exp (-1.0 / (rmsMsTime * 0.001 * sr)) : 0.0f;
    }

    // Curva estatica: reduccion (dB, <= 0) para un nivel de entrada en dB
    float staticGr (float lvlDb) const noexcept
    {
        const float over = lvlDb - thr;
        float rr = ratio;
        if (type == 3 || type == 7) rr = 1.0f + (ratio - 1.0f) * std::clamp ((over + knee * 0.5f) / 20.0f, 0.15f, 1.0f);
        const float slope = 1.0f / rr - 1.0f;
        if (2.0f * over <= -knee)               return 0.0f;
        if (2.0f * std::abs (over) < knee)      return slope * (over + knee * 0.5f) * (over + knee * 0.5f) / (2.0f * knee);
        return slope * over;
    }

    // detL/detR: senal del detector (propia o sidechain, ya filtrada)
    inline float computeGain (float detL, float detR) noexcept
    {
        const float peak = std::max (std::abs (detL), std::abs (detR));
        float lvl = peak;
        if (rmsCoef > 0.0f)
        {
            rmsMs = rmsCoef * rmsMs + (1.0f - rmsCoef) * peak * peak;
            lvl = std::sqrt (rmsMs) * 1.414f;
        }
        const float target = staticGr (gainToDb (lvl));

        if (target < grDb)
            grDb = attCoef * grDb + (1.0f - attCoef) * target;
        else
        {
            float relS = relSamples;
            if (type == 1 || type == 4) relS *= 1.0f + (type == 1 ? 4.0f : 2.5f) * charge;   // opto: suelta mas lento si comprime mucho
            if (type == 8) relS *= 0.4f + 1.6f * charge;                                     // auto-release: rapido en picos, lento si comprime seguido
            grDb += (target - grDb) / std::max (1.0f, relS);
        }
        if (type == 1 || type == 4 || type == 8) charge += chargeCoef * (std::min (1.0f, -grDb / 10.0f) - charge);

        lastGr = grDb;
        return dbToGain (grDb) * makeup;
    }

    // Color armonico del modelo (se aplica despues de la ganancia)
    inline float colour (float x) const noexcept
    {
        switch (type)
        {
            case 1: { const float k = 0.5f + 0.02f * -grDb; return std::tanh (x * k) / k; }
            case 2: { const float k = 0.9f + 0.05f * -grDb; return std::tanh (x * k) / k; }
            case 3: { const float k = 0.6f; return (std::tanh (x * k + 0.08f) - std::tanh (0.08f)) / k; }
            case 4: { const float k = 0.7f + 0.03f * -grDb; return std::tanh (x * k) / k; }      // denso
            case 6: { const float k = 0.4f; return std::tanh (x * k) / k; }
            case 7: { const float k = 0.8f + 0.04f * -grDb; return (std::tanh (x * k + 0.12f) - std::tanh (0.12f)) / k; }   // valvula: armonicos pares
            default: return x;
        }
    }
};

//==============================================================================
// WIDER: abre la voz en estereo. Una perilla (cantidad) y 5 efectos:
//   0 Wide      - ensancha con senal lateral decorrelacionada (100 % compatible en mono)
//   1 Chorus    - dos voces moduladas en contrafase
//   2 Double    - doblaje automatico (ADT): dos tomas "falsas" con variaciones lentas
//   3 Shift     - micro pitch shift: izquierda un poco arriba, derecha un poco abajo
//   4 Dimension - ensemble de 3 voces suaves por lado (espacioso sin sonar a chorus)
struct Widener
{
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        size = 1;
        while (size < (int) (sr * 0.08) + 8) size <<= 1;
        buf.assign ((size_t) size, 0.0f);
        mask = size - 1;
        for (auto& f : sideHp) f.setHighPass (sr, 300.0, 0.707);
        for (auto& f : dblHp)  f.setHighPass (sr, 150.0, 0.707);
        for (auto& f : dblLp)  f.setLowPass (sr, 8000.0, 0.707);
        shifter[0].prepare (sr, 0.03);
        shifter[1].prepare (sr, 0.03);
        reset();
    }

    void reset()
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        w = 0; ph = 0.0;
        for (auto* f : { &sideHp[0], &sideHp[1], &dblHp[0], &dblHp[1], &dblLp[0], &dblLp[1] }) f->reset();
        shifter[0].reset(); shifter[1].reset();
    }

    inline void process (float& l, float& r, int mode, float amt) noexcept
    {
        const float m = 0.5f * (l + r);
        buf[(size_t) w] = m;
        ph += 1.0 / sr;
        if (ph > 1000.0) ph -= 1000.0;
        const double twoPi = 2.0 * kPi;

        float wl = 0.0f, wr = 0.0f;
        switch (mode)
        {
            case 1: // Chorus
            {
                const double lfo = std::sin (twoPi * 0.75 * ph);
                wl = read (0.014 + 0.0035 * lfo);
                wr = read (0.014 - 0.0035 * lfo);
                const float mix = 0.65f * amt;
                l = l * (1.0f - 0.2f * amt) + wl * mix;
                r = r * (1.0f - 0.2f * amt) + wr * mix;
                break;
            }
            case 2: // Double (ADT)
            {
                const double a = std::sin (twoPi * 0.13 * ph) + 0.6 * std::sin (twoPi * 0.29 * ph + 1.3);
                const double b = std::sin (twoPi * 0.17 * ph + 2.1) + 0.6 * std::sin (twoPi * 0.23 * ph + 0.4);
                wl = dblLp[0].process (dblHp[0].process (read (0.024 + 0.0012 * a)));
                wr = dblLp[1].process (dblHp[1].process (read (0.032 + 0.0014 * b)));
                const float mix = 0.75f * amt;
                l = l * (1.0f - 0.25f * amt) + wl * mix;
                r = r * (1.0f - 0.25f * amt) + wr * mix;
                break;
            }
            case 3: // Shift (micro pitch)
            {
                const float cents = 5.0f + 13.0f * amt;
                wl = shifter[0].process (m, std::exp2 ( cents / 1200.0f));
                wr = shifter[1].process (m, std::exp2 (-cents / 1200.0f));
                const float mix = 0.7f * amt;
                l = l * (1.0f - 0.2f * amt) + wl * mix;
                r = r * (1.0f - 0.2f * amt) + wr * mix;
                break;
            }
            case 4: // Dimension (ensemble)
            {
                for (int v = 0; v < 3; ++v)
                {
                    const double off = (double) v * twoPi / 3.0;
                    const double base = 0.007 + 0.002 * v;
                    wl += read (base + 0.0006 * std::sin (twoPi * 0.5 * ph + off));
                    wr += read (base + 0.0006 * std::sin (twoPi * 0.5 * ph + off + kPi));
                }
                wl *= 0.4f; wr *= 0.4f;
                const float mix = 0.7f * amt;
                l = l * (1.0f - 0.2f * amt) + wl * mix;
                r = r * (1.0f - 0.2f * amt) + wr * mix;
                break;
            }
            default: // Wide: lateral decorrelado, se cancela en mono
            {
                const float s = 0.5f * (read (0.011) - read (0.017));
                const float side = sideHp[0].process (s) * 1.6f * amt;
                l += side;
                r -= side;
                break;
            }
        }
        w = (w + 1) & mask;
    }

private:
    inline float read (double seconds) const noexcept
    {
        const double pos = (double) w - seconds * sr;
        const double fl = std::floor (pos);
        const float t = (float) (pos - fl);
        const int i = (int) fl;
        const float a = buf[(size_t) (i & mask)], b = buf[(size_t) ((i + 1) & mask)];
        return a + t * (b - a);
    }

    double sr = 44100.0, ph = 0.0;
    std::vector<float> buf;
    int size = 0, mask = 0, w = 0;
    Biquad sideHp[2], dblHp[2], dblLp[2];
    PitchShifter shifter[2];
};

//==============================================================================
// Delay estereo: feedback con filtros (Lo Cut / Hi Cut), saturacion, modulacion,
// ducking (el eco baja mientras cantas), ancho estereo y modo ping-pong.
struct StereoDelay
{
    std::vector<float> bufL, bufR;
    int size = 0, w = 0;
    double sr = 44100.0, lfo = 0.0;
    float curDelay = 1000.0f, targetDelay = 1000.0f;
    float lpL = 0.0f, lpR = 0.0f, lpCoef = 0.5f;
    Biquad hpL, hpR;
    float feedback = 0.25f, mix = 0.1f, sat = 0.0f, modDepth = 0.0f, duck = 0.0f, width = 1.0f;
    float env = 0.0f, envAtt = 0.0f, envRel = 0.0f, lastLoCut = -1.0f;
    bool pingPong = false;
    int model = 0;   // 0 Digital, 1 Tape, 2 Analog, 3 Slap
    double flutter = 0.0;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        size = (int) (sr * 2.3) + 8;
        bufL.assign ((size_t) size, 0.0f);
        bufR.assign ((size_t) size, 0.0f);
        envAtt = 1.0f - (float) std::exp (-1.0 / (0.005 * sr));
        envRel = 1.0f - (float) std::exp (-1.0 / (0.25 * sr));
        lastLoCut = -1.0f;
        reset();
    }

    void reset()
    {
        std::fill (bufL.begin(), bufL.end(), 0.0f);
        std::fill (bufR.begin(), bufR.end(), 0.0f);
        w = 0; lpL = lpR = 0.0f; env = 0.0f; lfo = 0.0;
        hpL.reset(); hpR.reset();
        curDelay = targetDelay;
    }

    void set (float delayMs, float fb, float mix01, float hiCutHz, bool pp,
              float loCutHz = 20.0f, float sat01 = 0.0f, float mod01 = 0.0f, float duck01 = 0.0f, float width01 = 1.0f,
              int newModel = 0) noexcept
    {
        model = newModel;
        // Cada modelo cambia el caracter de las repeticiones
        if (model == 1) { hiCutHz = std::min (hiCutHz, 7500.0f); sat01 = std::max (sat01, 0.3f); mod01 = std::max (mod01, 0.15f); }      // Tape
        if (model == 2) { hiCutHz = std::min (hiCutHz, 4200.0f); sat01 = std::max (sat01, 0.2f); }                                         // Analog (BBD)
        if (model == 3) { delayMs = std::clamp (delayMs, 60.0f, 160.0f); fb = std::min (fb, 0.2f); }                                       // Slap
        targetDelay = std::clamp ((float) (delayMs * 0.001 * sr), 8.0f, (float) size - 300.0f);
        feedback = std::clamp (fb, 0.0f, 0.95f);
        mix = mix01;
        lpCoef = 1.0f - (float) std::exp (-2.0 * kPi * hiCutHz / sr);
        pingPong = pp;
        if (std::abs (loCutHz - lastLoCut) > 0.5f)
        {
            lastLoCut = loCutHz;
            if (loCutHz <= 20.5f) { hpL.setIdentity(); hpR.setIdentity(); }
            else { hpL.setHighPass (sr, loCutHz, 0.707); hpR.setHighPass (sr, loCutHz, 0.707); }
        }
        sat = sat01; modDepth = mod01; duck = duck01; width = width01;
    }

    inline float read (const std::vector<float>& b, float d) const noexcept
    {
        float pos = (float) w - d;
        while (pos < 0.0f) pos += (float) size;
        const int i0 = (int) pos;
        const int i1 = (i0 + 1) % size;
        const float t = pos - (float) i0;
        return b[(size_t) i0] + t * (b[(size_t) i1] - b[(size_t) i0]);
    }

    inline float shape (float x) const noexcept
    {
        if (sat <= 0.0f) return x;
        const float k = 1.0f + 4.0f * sat;
        return std::tanh (x * k) / k * (1.0f + sat * 0.5f);
    }

    inline void process (float& l, float& r) noexcept
    {
        curDelay += 0.0005f * (targetDelay - curDelay);   // cambio de tiempo sin clicks

        // Modulacion suave del tiempo (como una cinta)
        lfo += (model == 2 ? 1.1 : 0.6) / sr; if (lfo > 1.0) lfo -= 1.0;
        float m = modDepth * 0.003f * (float) sr * (float) std::sin (2.0 * kPi * lfo);
        if (model == 1)   // cinta: wow + flutter
        {
            flutter += 6.3 / sr; if (flutter > 1.0) flutter -= 1.0;
            m += 0.00012f * (float) sr * (float) std::sin (2.0 * kPi * flutter);
        }
        const float dl = read (bufL, curDelay + m);
        const float dr = read (bufR, curDelay - m);
        lpL += lpCoef * (dl - lpL);
        lpR += lpCoef * (dr - lpR);
        const float fl = shape (hpL.process (lpL)), fr = shape (hpR.process (lpR));

        if (pingPong)
        {
            bufL[(size_t) w] = 0.5f * (l + r) + fr * feedback;
            bufR[(size_t) w] = fl * feedback;
        }
        else
        {
            bufL[(size_t) w] = l + fl * feedback;
            bufR[(size_t) w] = r + fr * feedback;
        }
        w = (w + 1) % size;

        // Ducking: el eco baja mientras hay voz
        const float lvl = std::max (std::abs (l), std::abs (r));
        env += (lvl > env ? envAtt : envRel) * (lvl - env);
        float g = mix;
        if (duck > 0.0f)
        {
            const float envDb = gainToDb (env);
            g *= dbToGain (-duck * 18.0f * std::clamp ((envDb + 45.0f) / 30.0f, 0.0f, 1.0f));
        }
        // Ancho del eco
        const float mid = 0.5f * (fl + fr), side = 0.5f * (fl - fr) * width;
        l += (mid + side) * g;
        r += (mid - side) * g;
    }
};

//==============================================================================
// Reverb FDN (8 lineas, matriz Householder) con 5 tipos:
//   0 Room, 1 Plate, 2 Hall, 3 Chamber, 4 Vocal Plate
// Pre-delay, Decay (RT60), Size, Damping, Early Reflections, Diffusion,
// Lo Cut / Hi Cut de la cola, Ducking y Width.
struct FdnReverb
{
    static constexpr int N = 8;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        const int maxLen = (int) (sr * 0.2) + 16;
        for (auto& d : lines) d.assign ((size_t) maxLen, 0.0f);
        pre.assign ((size_t) (sr * 0.3) + 16, 0.0f);
        for (auto& a : ap) a.assign ((size_t) (sr * 0.02) + 4, 0.0f);
        envAtt = 1.0f - (float) std::exp (-1.0 / (0.005 * sr));
        envRel = 1.0f - (float) std::exp (-1.0 / (0.3 * sr));
        lastLo = lastHi = -1.0f;
        reset();
    }

    void reset()
    {
        for (auto& d : lines) std::fill (d.begin(), d.end(), 0.0f);
        std::fill (pre.begin(), pre.end(), 0.0f);
        for (auto& a : ap) std::fill (a.begin(), a.end(), 0.0f);
        for (auto& s : damp) s = 0.0f;
        for (auto& f : loF) f.reset();
        for (auto& f : hiF) f.reset();
        lw = pw = 0; for (auto& i : apw) i = 0;
        env = 0.0f; modPh = 0.0;
    }

    void set (int newType, float preMs, float decayS, float size01, float damp01, float er01, float diff01,
              float loCut, float hiCut, float duck01, float width01) noexcept
    {
        type = std::clamp (newType, 0, 6);
        static const float base[7][N] {
            { 7.9f, 11.3f, 13.1f, 16.7f, 19.3f, 23.1f, 27.7f, 31.3f },     // Room
            { 9.1f, 12.7f, 15.3f, 19.9f, 23.3f, 28.1f, 33.7f, 37.9f },     // Plate
            { 29.3f, 37.1f, 43.7f, 51.1f, 59.3f, 67.9f, 73.1f, 83.7f },    // Hall
            { 13.1f, 17.9f, 21.7f, 26.3f, 31.1f, 36.7f, 41.3f, 47.9f },    // Chamber
            { 10.7f, 14.3f, 17.9f, 22.1f, 26.9f, 31.3f, 38.1f, 43.3f },    // Vocal Plate
            { 12.1f, 16.3f, 19.7f, 24.1f, 28.7f, 33.1f, 39.7f, 45.1f },    // Vintage (camara de los 70: oscura y modulada)
            { 19.7f, 25.3f, 31.1f, 37.9f, 44.3f, 51.7f, 58.3f, 66.1f } };  // Modern (brillante, densa y lisa)
        const float scale = 0.6f + 0.8f * size01;
        const float rt = std::max (0.1f, decayS);
        const int maxLen = (int) lines[0].size() - 4;
        for (int i = 0; i < N; ++i)
        {
            len[i] = std::clamp ((int) (base[type][i] * 0.001f * scale * (float) sr), 16, maxLen);
            gain[i] = std::pow (10.0f, -3.0f * (float) len[i] / (rt * (float) sr));
        }
        // Damping: los plates son brillantes, los cuartos/halls mas oscuros
        const float brightness = (type == 1 || type == 4) ? 0.85f : type == 6 ? 1.0f : type == 5 ? 0.38f : (type == 2 ? 0.55f : 0.65f);
        const float dampHz = 1500.0f + (1.0f - damp01) * 14000.0f * brightness;
        dampCoef = 1.0f - (float) std::exp (-2.0 * kPi * dampHz / sr);
        preLen = std::clamp ((int) (preMs * 0.001f * (float) sr), 0, (int) pre.size() - 2);
        er = er01 * ((type == 1 || type == 4) ? 0.4f : 1.0f);
        apCoef = std::min (0.8f, 0.3f + 0.45f * diff01 + (type == 6 ? 0.08f : 0.0f));
        duck = duck01; width = width01;
        static const float apMs[4] { 1.3f, 2.1f, 3.7f, 5.3f };   // difusores
        for (int k = 0; k < 4; ++k) apLen[k] = std::clamp ((int) (apMs[k] * 0.001f * (float) sr * (0.7f + 0.6f * size01)), 8, (int) ap[k].size() - 2);
        if (std::abs (loCut - lastLo) > 0.5f) { lastLo = loCut; for (auto& f : loF) { if (loCut <= 20.5f) f.setIdentity(); else f.setHighPass (sr, loCut, 0.707); } }
        if (std::abs (hiCut - lastHi) > 0.5f) { lastHi = hiCut; for (auto& f : hiF) { if (hiCut >= 19999.0f) f.setIdentity(); else f.setLowPass (sr, hiCut, 0.707); } }
    }

    // Procesa una muestra: entrada estereo -> salida humeda (wl, wr)
    inline void process (float inL, float inR, float& wl, float& wr) noexcept
    {
        const float x = 0.5f * (inL + inR);
        pre[(size_t) pw] = x;
        int rp = pw - preLen; if (rp < 0) rp += (int) pre.size();
        float s = pre[(size_t) rp];
        pw = (pw + 1) % (int) pre.size();

        // Reflexiones tempranas (desde la senal con pre-delay)
        static const float erMs[7][6] { { 7, 11, 17, 23, 29, 37 }, { 3, 5, 8, 11, 13, 17 }, { 17, 29, 41, 53, 67, 79 },
                                        { 11, 19, 27, 35, 43, 51 }, { 4, 7, 9, 12, 15, 19 }, { 9, 15, 22, 31, 38, 47 },
                                        { 6, 13, 21, 29, 37, 46 } };
        float erL = 0.0f, erR = 0.0f;
        if (er > 0.0f)
            for (int k = 0; k < 6; ++k)
            {
                int ep = rp - (int) (erMs[type][k] * 0.001f * (float) sr);
                while (ep < 0) ep += (int) pre.size();
                const float tap = pre[(size_t) ep] * (0.9f - 0.12f * (float) k) * ((k & 1) ? -1.0f : 1.0f);
                if (k % 2 == 0) erL += tap; else erR += tap;
            }

        // Difusores (allpass en serie)
        for (int k = 0; k < 4; ++k)
        {
            auto& a = ap[(size_t) k];
            int r = apw[k] - apLen[k]; if (r < 0) r += (int) a.size();
            const float d = a[(size_t) r];
            const float v = s + apCoef * d;
            a[(size_t) apw[k]] = v;
            s = d - apCoef * v;
            apw[k] = (apw[k] + 1) % (int) a.size();
        }

        // FDN con matriz Householder
        modPh += 0.37 / sr; if (modPh > 1.0) modPh -= 1.0;
        const float mod = (type == 0) ? 0.0f : (float) (std::sin (2.0 * kPi * modPh) * (type == 5 ? 0.0011 : type == 6 ? 0.0006 : 0.0004) * sr);
        float out[N], sum = 0.0f;
        const int L = (int) lines[0].size();
        for (int i = 0; i < N; ++i)
        {
            float pos = (float) (lw - len[i]) - (i < 2 ? mod * (i == 0 ? 1.0f : -1.0f) : 0.0f);
            while (pos < 0.0f) pos += (float) L;
            const int i0 = (int) pos; const float t = pos - (float) i0;
            const auto& d = lines[(size_t) i];
            float v = d[(size_t) i0] + t * (d[(size_t) ((i0 + 1) % L)] - d[(size_t) i0]);
            damp[i] += dampCoef * (v - damp[i]);
            out[i] = damp[i] * gain[i];
            sum += out[i];
        }
        const float hh = sum * (2.0f / (float) N);
        for (int i = 0; i < N; ++i)
            lines[(size_t) i][(size_t) lw] = out[i] - hh + s * 0.35f;
        lw = (lw + 1) % L;

        float tl = out[0] - out[2] + out[4] - out[6] + out[1] * 0.5f;
        float tr = out[1] - out[3] + out[5] - out[7] + out[2] * 0.5f;
        tl = tl * 0.45f + erL * er * 0.5f;
        tr = tr * 0.45f + erR * er * 0.5f;
        tl = hiF[0].process (loF[0].process (tl));
        tr = hiF[1].process (loF[1].process (tr));

        // Ducking: la cola baja mientras cantas
        const float lvl = std::max (std::abs (inL), std::abs (inR));
        env += (lvl > env ? envAtt : envRel) * (lvl - env);
        float g = 1.0f;
        if (duck > 0.0f) g = dbToGain (-duck * 15.0f * std::clamp ((gainToDb (env) + 45.0f) / 30.0f, 0.0f, 1.0f));

        const float mid = 0.5f * (tl + tr), side = 0.5f * (tl - tr) * width;
        wl = (mid + side) * g;
        wr = (mid - side) * g;
    }

private:
    double sr = 44100.0, modPh = 0.0;
    int type = 4;
    std::vector<float> lines[N], pre, ap[4];
    int len[N] {}, lw = 0, pw = 0, preLen = 0, apLen[4] {}, apw[4] {};
    float gain[N] {}, damp[N] {}, dampCoef = 0.5f, er = 0.3f, apCoef = 0.6f, duck = 0.0f, width = 1.0f;
    float env = 0.0f, envAtt = 0.0f, envRel = 0.0f, lastLo = -1.0f, lastHi = -1.0f;
    Biquad loF[2], hiF[2];
};

//==============================================================================
// Medidores profesionales: RMS, LUFS momentaneo e integrado (ITU-R BS.1770 / EBU R128),
// True Peak (sobremuestreo 4x) y correlacion.
struct LoudnessMeter
{
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (int c = 0; c < 2; ++c)
        {
            // Filtro K: shelf de cabeza (+4 dB arriba de ~1.7 kHz) + pasa-altos RLB (~38 Hz)
            kShelf[c].setHighShelf (sr, 1681.97, 3.9998);
            kHp[c].setHighPass (sr, 38.135, 0.5003);
        }
        hop = std::max (1, (int) (sr * 0.1));
        reset();
    }

    void reset()
    {
        for (int c = 0; c < 2; ++c) { kShelf[c].reset(); kHp[c].reset(); }
        std::fill (std::begin (hopPow), std::end (hopPow), 0.0);
        hopIdx = 0; hopCount = 0; hopAcc = 0.0;
        momentary = -100.0f;
        std::fill (std::begin (hist), std::end (hist), 0.0);
        std::fill (std::begin (histCount), std::end (histCount), 0);
        tpHold = 0.0f; peakHold = 0.0f; rmsMs = 0.0;
        for (auto& h : tpHist) std::fill (std::begin (h), std::end (h), 0.0f);
    }

    void process (const float* l, const float* r, int n) noexcept
    {
        const double rmsCoef = std::exp (-1.0 / (0.3 * sr));
        for (int i = 0; i < n; ++i)
        {
            const float xl = l[i], xr = r ? r[i] : l[i];
            // K-weighting
            const float kl = kHp[0].process (kShelf[0].process (xl));
            const float kr = kHp[1].process (kShelf[1].process (xr));
            hopAcc += (double) kl * kl + (double) kr * kr;
            if (++hopCount >= hop)
            {
                hopPow[hopIdx] = hopAcc / hop;
                hopIdx = (hopIdx + 1) % 4;
                hopAcc = 0.0; hopCount = 0;
                const double block = 0.25 * (hopPow[0] + hopPow[1] + hopPow[2] + hopPow[3]);   // ventana de 400 ms
                const float lufs = (float) (-0.691 + 10.0 * std::log10 (block + 1.0e-12));
                momentary = lufs;
                if (lufs > -70.0f)       // compuerta absoluta
                {
                    const int bin = std::clamp ((int) ((lufs + 70.0f) * 10.0f), 0, numBins - 1);
                    hist[bin] += block; histCount[bin]++;
                }
            }
            // RMS (300 ms) y pico
            rmsMs = rmsCoef * rmsMs + (1.0 - rmsCoef) * 0.5 * ((double) xl * xl + (double) xr * xr);
            peakHold = std::max (peakHold, std::max (std::abs (xl), std::abs (xr)));
            // True peak: interpolacion 4x con sinc enventanado
            for (int c = 0; c < 2; ++c)
            {
                auto& h = tpHist[c];
                for (int k = 0; k < tpTaps - 1; ++k) h[k] = h[k + 1];
                h[tpTaps - 1] = c == 0 ? xl : xr;
                for (int ph = 1; ph < 4; ++ph)
                {
                    float acc = 0.0f;
                    for (int k = 0; k < tpTaps; ++k) acc += h[k] * tpCoef (ph, k);
                    tpHold = std::max (tpHold, std::abs (acc));
                }
                tpHold = std::max (tpHold, std::abs (h[tpTaps / 2]));
            }
        }
    }

    float getMomentary() const noexcept { return momentary; }
    float getRmsDb() const noexcept { return (float) (10.0 * std::log10 (rmsMs + 1.0e-12)); }
    float getPeakHoldDb() const noexcept { return gainToDb (peakHold); }
    float getTruePeakDb() const noexcept { return gainToDb (tpHold); }

    float getIntegrated() const noexcept
    {
        double sum = 0.0; long cnt = 0;
        for (int b = 0; b < numBins; ++b) { sum += hist[b]; cnt += histCount[b]; }
        if (cnt == 0) return -100.0f;
        const double ungated = -0.691 + 10.0 * std::log10 (sum / (double) cnt);
        const double relGate = ungated - 10.0;   // compuerta relativa -10 LU
        sum = 0.0; cnt = 0;
        for (int b = 0; b < numBins; ++b)
            if (-70.0 + (b + 0.5) * 0.1 >= relGate) { sum += hist[b]; cnt += histCount[b]; }
        if (cnt == 0) return -100.0f;
        return (float) (-0.691 + 10.0 * std::log10 (sum / (double) cnt));
    }

private:
    static constexpr int numBins = 800, tpTaps = 12;
    static float tpCoef (int phase, int k) noexcept
    {
        static float table[4][tpTaps] {};
        static bool init = false;
        if (! init)
        {
            for (int p = 0; p < 4; ++p)
                for (int j = 0; j < tpTaps; ++j)
                {
                    const double t = (double) (j - tpTaps / 2) + (double) p / 4.0;
                    const double sinc = std::abs (t) < 1e-9 ? 1.0 : std::sin (kPi * t) / (kPi * t);
                    const double win = 0.5 + 0.5 * std::cos (kPi * t / (tpTaps / 2 + 1));
                    table[p][j] = (float) (sinc * win);
                }
            init = true;
        }
        return table[phase][k];
    }

    double sr = 48000.0;
    Biquad kShelf[2], kHp[2];
    int hop = 4800, hopIdx = 0, hopCount = 0;
    double hopPow[4] {}, hopAcc = 0.0;
    float momentary = -100.0f;
    double hist[numBins] {};
    long histCount[numBins] {};
    float tpHist[2][tpTaps] {};
    float tpHold = 0.0f, peakHold = 0.0f;
    double rmsMs = 0.0;
};

} // namespace tvc
