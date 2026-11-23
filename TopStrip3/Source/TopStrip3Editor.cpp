#include "TopStrip3Editor.h"
#include <map>

using namespace s7ui3;
using juce::Colour; namespace Colours = juce::Colours; using juce::Graphics; using juce::Path; using juce::Rectangle; using juce::ColourGradient;
using Pt = juce::Point<float>;

namespace
{
    juce::String U (const char* s) { return juce::String::fromUTF8 (s); }
    constexpr float pi = juce::MathConstants<float>::pi, deg = pi / 180.0f;
    float param (TopStrip3Processor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); }
    bool powerOn (TopStrip3Processor& p) { return param (p, "power") > 0.5f; }
    void setParam (TopStrip3Processor& p, const char* id, float v)
    { if (auto* q = p.apvts.getParameter (id)) { q->beginChangeGesture(); q->setValueNotifyingHost (q->convertTo0to1 (v)); q->endChangeGesture(); } }
    Pt polar (Pt c, float r, float aDeg) { return { c.x + r * std::sin (aDeg * deg), c.y - r * std::cos (aDeg * deg) }; }

    // ------------------------------------------------------------------ medidas tomadas de la foto frontal del original
    // (panel de 1322 x 242 px en la foto, de x = 92 a 1414 y de y = 140 a 382)
    constexpr float X0 = 92.0f, Y0 = 140.0f, S = (float) Content::strip;
    constexpr float sc = (float) Content::W / 1322.0f;
    Pt P (float x, float y) { return { (x - X0) * sc, S + (y - Y0) * sc }; }
    float L (float v) { return v * sc; }
    juce::AffineTransform photoToContent() { return juce::AffineTransform::translation (-X0, -Y0).scaled (sc).translated (0, S); }

    struct KP { float x, y, r; };
    const KP kPre { 233.3f, 213.5f, 27.5f }, kInput { 321.5f, 216.7f, 26.0f }, kHpf { 321.7f, 337.0f, 21.0f },
             kThr { 495.0f, 218.5f, 27.3f }, kRatio { 582.7f, 218.5f, 27.3f }, kAtt { 495.8f, 338.3f, 21.5f }, kRel { 580.7f, 338.3f, 21.5f },
             kBass { 932.3f, 215.0f, 27.5f }, kLm { 1019.3f, 215.0f, 27.5f }, kHm { 1106.7f, 215.0f, 27.5f }, kTr { 1192.7f, 215.0f, 27.5f },
             kOut { 1277.7f, 213.0f, 24.0f },
             kBassF { 932.0f, 335.0f, 19.5f }, kLmF { 1018.5f, 335.0f, 19.5f }, kHmF { 1105.0f, 335.0f, 19.5f }, kTrF { 1192.5f, 335.0f, 19.5f };
    constexpr float kBox = 1.55f;                       // la imagen de la perilla es mas grande que la perilla (sombra y perspectiva)
    std::vector<float> stepsFrom (float a0, float step, int n) { std::vector<float> v; for (int i = 0; i < n; ++i) v.push_back (a0 + step * (float) i); return v; }

    struct BP { float x, y; };
    constexpr float capHW = 14.75f, capHH = 7.85f;      // mitad del boton (foto)
    const BP bpHigh { 406.7f, 209.7f }, bpPhase { 406.7f, 244.0f }, bpP48 { 406.7f, 279.0f }, bpFilter { 406.7f, 313.3f },
             bpPre { 474.3f, 279.0f }, bpMeter { 517.8f, 279.0f }, bpComp { 583.3f, 279.0f },
             bpLmQ { 998.7f, 278.8f }, bpLmX { 1042.2f, 278.8f }, bpHmQ { 1085.5f, 278.8f }, bpHmX { 1128.3f, 278.8f },
             bpSc { 1256.2f, 278.8f }, bpEq { 1300.0f, 278.8f };

    // VU (cara, bisel y escala medidos en la foto)
    const Pt vuFace { 758.5f, 229.2f }; constexpr float vuRx = 94.2f, vuRy = 45.8f;
    const Pt vuBezel { 758.8f, 231.3f }; constexpr float vuBx = 103.8f, vuBy = 56.3f;
    const Pt vuPivot { 758.5f, 369.6f }; constexpr float vuR = 163.0f;
    const float vuDb[]  { -20, -10, -7, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3 };
    const float vuAng[] { -31.5f, -23.3f, -17.9f, -15.3f, -12.7f, -9.3f, -5.9f, -1.2f, 3.2f, 8.8f, 15.4f, 22.8f, 32.1f };
    float vuAngle (float db)
    {
        if (db <= vuDb[0]) return vuAng[0] - (vuDb[0] - db) * 0.45f;
        for (int i = 1; i < 13; ++i) if (db <= vuDb[i]) { const float t = (db - vuDb[i - 1]) / (vuDb[i] - vuDb[i - 1]); return vuAng[i - 1] + t * (vuAng[i] - vuAng[i - 1]); }
        return vuAng[12] + (db - 3.0f) * 3.0f;
    }
    const Pt ledPos { 758.3f, 350.0f }; constexpr float ledR = 6.6f;

    // ------------------------------------------------------------------ tipografias (la del panel es una sans humanista tipo Optima)
    juce::String pickFont (std::initializer_list<const char*> names, const juce::String& fallback)
    {
        static const juce::StringArray all = juce::Font::findAllTypefaceNames();
        for (auto* n : names) if (all.contains (n, true)) return n;
        return fallback;
    }
    const juce::String& sansName() { static const juce::String n = pickFont ({ "Optima", "Segoe UI", "Fira Sans", "Candara" }, juce::Font::getDefaultSansSerifFontName()); return n; }
    const juce::String& serifName() { static const juce::String n = pickFont ({ "Times New Roman", "Times", "Liberation Serif", "DejaVu Serif" }, juce::Font::getDefaultSerifFontName()); return n; }
    // tamano calibrado por la altura real de las mayusculas (igual en Windows, Mac y Linux)
    juce::Font capFont (const juce::String& name, int style, float cap)
    {
        static std::map<juce::String, float> ratio;
        static juce::SpinLock lock;
        const juce::String key = name + juce::String (style);
        float r;
        {
            const juce::SpinLock::ScopedLockType sl (lock);
            auto it = ratio.find (key);
            if (it == ratio.end())
            {
                juce::GlyphArrangement ga; ga.addLineOfText (juce::Font (juce::FontOptions (name, 100.0f, style)), "H", 0, 0);
                Path p; ga.createPath (p);
                r = juce::jmax (0.3f, p.getBounds().getHeight() / 100.0f);
                ratio[key] = r;
            }
            else r = it->second;
        }
        return juce::Font (juce::FontOptions (name, cap / r, style));
    }
    juce::Font sans (float cap, bool bold = true) { return capFont (sansName(), bold ? juce::Font::bold : juce::Font::plain, cap); }
    juce::Font serif (float cap, int style = juce::Font::plain) { return capFont (serifName(), style, cap); }

    // texto centrado en (x, y) por su contorno real (no por la caja de la fuente)
    void textAt (Graphics& g, const juce::String& t, float x, float y, const juce::Font& f, float hs = 1.0f, float kern = 0.0f, float shear = 0.0f)
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (f.withHorizontalScale (hs).withExtraKerningFactor (kern), t, 0, 0);
        Path p; ga.createPath (p);
        if (shear != 0.0f) p.applyTransform (juce::AffineTransform::shear (shear, 0.0f));     // cursiva sintetica (igual en todos los sistemas)
        const auto b = p.getBounds();
        p.applyTransform (juce::AffineTransform::translation (x - b.getCentreX(), y - b.getCentreY()));
        g.fillPath (p);
    }
    void radial (Graphics& g, Pt c, float r0, float r1, float a, float w) { g.drawLine ({ polar (c, r0, a), polar (c, r1, a) }, w); }

    // escala de perilla del panel (igual en ganancia, umbral, ratio, EQ y salida): numeros entre rayas largas,
    // barra gruesa arriba y topes gruesos a +-140 grados
    void knobScale (Graphics& g, const KP& k, std::vector<const char*> left, std::vector<const char*> right, const char* top, bool thickCenter)
    {
        const Pt c (k.x, k.y);
        const float f = k.r < 26.0f ? 0.86f : 1.0f;
        const float la[] { 14.0f, 39.0f, 65.0f, 92.0f, 125.0f }, lr[] { 46.0f, 44.5f, 40.5f, 37.0f, 35.0f };
        const float ta[] { 26.5f, 52.0f, 78.5f, 108.5f };
        const float t0 = 30.0f * f, t1 = 48.5f * f;
        // las rayas laterales no cruzan la mitad del espacio hasta la perilla vecina (como en el panel)
        auto tEnd = [t1] (float a) { const float s = std::abs (std::sin (a * deg)); return s > 0.5f ? std::min (t1, 37.5f / s) : t1; };
        for (float a : ta) { radial (g, c, t0, tEnd (a), a, 1.25f); radial (g, c, t0, tEnd (a), -a, 1.25f); }
        radial (g, c, 29.5f * f, 41.0f * f, 140.0f, 3.1f);
        radial (g, c, 29.5f * f, 41.0f * f, -140.0f, 3.1f);
        const float r0 = k.y - 163.0f;                           // el "0" va justo debajo del titulo
        if (thickCenter) radial (g, c, r0 - 15.0f, r0 - 5.0f, 0, 3.1f);
        else radial (g, c, t0, t1, 0, 1.25f);
        const auto fnt = sans (6.1f);
        for (size_t i = 0; i < 5; ++i)
        {
            if (i < left.size())  { const auto q = polar (c, lr[i] * f, -la[i]); textAt (g, left[i], q.x, q.y, fnt, 0.95f); }
            if (i < right.size()) { const auto q = polar (c, lr[i] * f, la[i]);  textAt (g, right[i], q.x, q.y, fnt, 0.95f); }
        }
        if (top != nullptr) { const auto q = polar (c, r0, 0); textAt (g, top, q.x, q.y, fnt, 0.95f); }
    }
    void hzBox (Graphics& g, float x, float y)
    {
        g.fillRoundedRectangle (x - 7.4f, y - 5.6f, 14.8f, 11.2f, 1.6f);
    }
    // mascara de tinta blanca (letras de las placas negras)
    void hzText (Graphics& g, float x, float y) { textAt (g, "Hz", x, y + 0.2f, sans (6.0f), 0.95f); }

    void tri (Graphics& g, float x, float y, float s, bool right)
    {
        Path t; const float d = right ? 1.0f : -1.0f;
        t.addTriangle (x - d * s, y - s, x - d * s, y + s, x + d * s, y);
        g.fillPath (t);
    }

    // ------------------------------------------------------------------ serigrafia negra
    void drawSilk (Graphics& g)
    {
        g.setColour (Colours::white);
        const auto title = sans (7.6f), sub = sans (5.6f), num = sans (5.7f), small = sans (5.0f);
        // ---- preamp
        textAt (g, "PREAMP GAIN", kPre.x, 153.5f, title, 0.95f);
        knobScale (g, kPre, { "26", "22", "16", "10", "2" }, { "32", "35", "38", "42", "45" }, "0", true);
        textAt (g, "- dB +", kPre.x, 245.0f, num, 0.95f);
        textAt (g, "-28", 207.0f, 257.7f, num, 0.95f); textAt (g, "LINE", kPre.x, 257.7f, num, 0.95f); textAt (g, "+9", 261.5f, 257.7f, num, 0.95f);
        textAt (g, "INSTRUMENT", 233.3f, 272.3f, title, 0.92f); textAt (g, "HIGH Z INPUT", 233.3f, 282.0f, small, 0.95f);
        // ---- input
        textAt (g, "INPUT", kInput.x, 152.5f, title, 0.95f); textAt (g, "INSTRUMENT", kInput.x, 165.5f, sub, 0.95f);
        textAt (g, "LINE", 300.0f, 181.0f, num, 0.95f); textAt (g, "MIC", 349.0f, 181.0f, num, 0.95f);
        {
            const Pt c (kInput.x, kInput.y);
            Path a1; a1.addCentredArc (c.x, c.y, 34.8f, 34.8f, 0, -145.0f * deg, -52.0f * deg, true);
            Path a2; a2.addCentredArc (c.x, c.y, 34.8f, 34.8f, 0, 52.0f * deg, 145.0f * deg, true);
            g.strokePath (a1, juce::PathStrokeType (2.3f)); g.strokePath (a2, juce::PathStrokeType (2.3f));
            radial (g, c, 36.5f, 42.0f, 0, 2.6f);
        }
        // ---- pasa altos
        textAt (g, "HIGH PASS", kHpf.x, 272.3f, title, 0.95f); textAt (g, "FREQUENCY", kHpf.x, 282.0f, small, 0.95f);
        {
            const char* t[] { "30", "35", "40", "45", "55", "60", "70", "80", "100", "120", "140" };
            const Pt pos[] { { 303.3f, 362.0f }, { 291.7f, 349.3f }, { 287.3f, 332.3f }, { 291.0f, 316.0f }, { 303.3f, 304.0f }, { 321.0f, 299.0f },
                             { 340.0f, 304.0f }, { 351.7f, 316.0f }, { 356.7f, 332.7f }, { 353.3f, 349.3f }, { 343.0f, 362.3f } };
            for (int i = 0; i < 11; ++i) textAt (g, t[i], pos[i].x, pos[i].y, num, 0.95f);
        }
        // ---- input mode
        textAt (g, "INPUT", 405.7f, 154.0f, title, 0.95f); textAt (g, "MODE", 405.7f, 166.7f, title, 0.95f);
        textAt (g, "HIGH", 405.7f, 184.0f, sub, 0.95f); textAt (g, "GAIN", 405.7f, 192.3f, sub, 0.95f);
        textAt (g, "PHASE", 405.7f, 227.3f, sub, 0.95f); textAt (g, "+48v", 405.7f, 262.3f, sub, 0.95f); textAt (g, "FILTER", 405.7f, 297.0f, sub, 0.95f);
        {
            Path h; h.startNewSubPath (395.0f, 338.5f); h.lineTo (399.0f, 338.5f); h.lineTo (405.0f, 331.5f); h.lineTo (416.5f, 331.5f);
            g.strokePath (h, juce::PathStrokeType (1.3f));
        }
        // ---- compresor
        textAt (g, "THRESHOLD", kThr.x, 154.0f, title, 0.95f); textAt (g, "COMPRESSION", kRatio.x, 154.0f, title, 0.95f);
        knobScale (g, kThr, { "5", "10", "15", "20", "30" }, { "2", "5", "10", "15", "20" }, "0", true);
        knobScale (g, kRatio, { "5", "4", "3", "2", "1" }, { "6", "7", "8", "10", "20" }, nullptr, false);
        textAt (g, "- dB +", kThr.x, 249.6f, num, 0.95f); textAt (g, "RATIO", kRatio.x, 249.6f, sans (6.6f), 0.95f);
        textAt (g, "EQ", 463.3f, 261.7f, sub, 0.95f); tri (g, 469.3f, 261.7f, 1.6f, true); textAt (g, "COMP", 480.6f, 261.7f, sub, 0.95f);
        textAt (g, "METER", bpMeter.x, 261.7f, sub, 0.95f); textAt (g, "COMPRESSOR", bpComp.x, 261.7f, sub, 0.95f);
        textAt (g, "ATTACK", kAtt.x, 296.0f, sans (6.6f), 0.95f); textAt (g, "RELEASE", kRel.x, 296.0f, sans (6.6f), 0.95f); textAt (g, "VU", 539.5f, 296.0f, sub, 0.95f);
        for (const KP* k : { &kAtt, &kRel })
        {
            const Pt c (k->x, k->y);
            Path a; a.addCentredArc (c.x, c.y, 29.8f, 29.8f, 0, -131.0f * deg, 131.0f * deg, true);
            g.strokePath (a, juce::PathStrokeType (2.1f));
            for (int i = 0; i < 9; ++i) radial (g, c, 24.5f, 29.8f, -120.0f + 30.0f * (float) i, 1.1f);
            textAt (g, "FAST", k->x - 22.5f, 366.7f, small, 0.95f); textAt (g, "SLOW", k->x + 23.5f, 366.7f, small, 0.95f);
        }
        // ---- EQ
        textAt (g, "BASS", kBass.x, 155.0f, title, 0.95f); textAt (g, "LOW MID", kLm.x, 155.0f, title, 0.95f);
        textAt (g, "HIGH MID", kHm.x, 155.0f, title, 0.95f); textAt (g, "TREBLE", kTr.x, 155.0f, title, 0.95f); textAt (g, "OUTPUT", kOut.x, 155.0f, title, 0.95f);
        knobScale (g, kBass, { "2", "4", "6", "12", "24" }, { "2", "4", "6", "12", "24" }, "0", true);
        knobScale (g, kLm, { "2", "4", "6", "10", "16" }, { "2", "4", "6", "10", "16" }, "0", true);
        knobScale (g, kHm, { "2", "4", "6", "10", "16" }, { "2", "4", "6", "10", "16" }, "0", true);
        knobScale (g, kTr, { "2", "4", "6", "12", "20" }, { "2", "4", "6", "12", "20" }, "0", true);
        knobScale (g, kOut, { "2", "4", "6", "8", "40" }, { "2", "4", "6", "8", "10" }, "0", true);
        for (const KP* k : { &kBass, &kLm, &kHm, &kTr, &kOut }) textAt (g, "- dB +", k->x, 249.3f, num, 0.95f);
        textAt (g, "SHELF", kBassF.x, 262.7f, sub, 0.95f); textAt (g, "SHELF", kTrF.x, 262.7f, sub, 0.95f);
        for (int side = 0; side < 2; ++side)
        {
            const float cx = side == 0 ? kBassF.x : kTrF.x, d = side == 0 ? 1.0f : -1.0f, cy = 278.3f;
            Path s; s.startNewSubPath (cx - 9.5f * d, cy - 4.2f); s.lineTo (cx - 2.5f * d, cy - 4.2f);
            s.cubicTo (cx + 2.8f * d, cy - 4.2f, cx + 2.8f * d, cy + 4.2f, cx - 2.5f * d, cy + 4.2f); s.lineTo (cx - 9.5f * d, cy + 4.2f);
            s.startNewSubPath (cx + 1.2f * d, cy); s.lineTo (cx + 9.5f * d, cy);
            g.strokePath (s, juce::PathStrokeType (1.35f));
        }
        textAt (g, "HI-Q", bpLmQ.x, 262.7f, sub, 0.95f); textAt (g, "FREQUENCY", bpLmX.x, 262.7f, sub, 0.95f);
        textAt (g, "HI-Q", bpHmQ.x, 262.7f, sub, 0.95f); textAt (g, "FREQUENCY", bpHmX.x, 262.7f, sub, 0.95f);
        textAt (g, "SC", 1244.8f, 262.7f, sub, 0.95f); tri (g, 1251.3f, 262.7f, 1.6f, false); textAt (g, "MIDS", 1263.2f, 262.7f, sub, 0.95f);
        textAt (g, "EQUALIZER", bpEq.x, 262.7f, sub, 0.95f);
        {
            const char* t[] { "15", "30", "60", "150" }; const Pt pos[] { { 905.0f, 310.0f }, { 922.7f, 300.7f }, { 940.7f, 300.7f }, { 958.3f, 310.0f } };
            for (int i = 0; i < 4; ++i) textAt (g, t[i], pos[i].x, pos[i].y, num, 0.95f);
            const char* u[] { "10k", "15k", "20k", "32k" }; const Pt pu[] { { 1166.7f, 309.0f }, { 1183.3f, 300.0f }, { 1200.7f, 300.0f }, { 1218.3f, 309.0f } };
            for (int i = 0; i < 4; ++i) textAt (g, u[i], pu[i].x, pu[i].y, num, 0.95f);
        }
        {
            const char* t[] { "35", "40", "50", "75", "100", "150", "200", "250", "300", "350", "450" };
            const char* u[] { "220", "250", "350", "550", "1k", "1k2", "1k4", "1k6", "2k", "2k5", "2k8" };
            const Pt pos[] { { 1000.0f, 361.7f }, { 988.3f, 350.0f }, { 984.0f, 332.7f }, { 988.3f, 316.7f }, { 1000.7f, 304.3f }, { 1018.3f, 299.3f },
                             { 1036.7f, 304.3f }, { 1048.3f, 316.7f }, { 1052.7f, 332.7f }, { 1048.3f, 350.0f }, { 1036.7f, 361.7f } };
            for (int i = 0; i < 11; ++i)
            {
                textAt (g, t[i], pos[i].x, pos[i].y, num, 0.95f);
                textAt (g, u[i], pos[i].x + (kHmF.x - kLmF.x) + (pos[i].x < kLmF.x - 3 ? -1.5f : (pos[i].x > kLmF.x + 3 ? 1.5f : 0.0f)), pos[i].y, num, 0.95f);
            }
        }
        for (const KP* k : { &kBassF, &kTrF })
        {
            Path a; a.addCentredArc (k->x, k->y, 35.3f, 35.3f, 0, 63.0f * deg, 297.0f * deg, true);
            g.strokePath (a, juce::PathStrokeType (2.2f));
        }
        for (float x : { kHpf.x, kBassF.x, kLmF.x, kHmF.x, kTrF.x }) hzBox (g, x, 365.5f);
        // ---- placas de marca (texto propio, sin copiar nombres ni logotipos); las ventanas se vacian despues
        for (auto b : { Rectangle<float> (119.3f, 222.3f, 65.7f, 84.4f), Rectangle<float> (1326.7f, 223.3f, 64.0f, 83.4f) })
        {
            g.drawRoundedRectangle (b.reduced (0.5f), 4.0f, 1.0f);
            g.fillRoundedRectangle (b.reduced (3.2f), 3.0f);
        }
        // ---- placa central: marca propia
        textAt (g, "TOP STRIP", 758.5f, 307.8f, serif (15.8f), 0.94f, 0.07f);
        textAt (g, "VACUUM TUBE", 758.5f, 328.0f, serif (8.0f), 1.0f, 0.19f);
    }
    // ventanas plateadas de las placas (se vacian en la mascara y luego se dibuja su contenido)
    const Rectangle<float> badgeWinL (129.6f, 231.3f, 42.3f, 44.7f), badgeWinR (1338.5f, 243.3f, 40.4f, 43.4f);
    void drawBadgeWindows (Graphics& g)
    {
        g.setColour (Colours::white);
        g.drawRect (badgeWinL.reduced (2.1f), 0.8f);
        textAt (g, "Top", badgeWinL.getCentreX(), badgeWinL.getCentreY(), serif (15.0f), 0.90f, 0.0f, -0.22f);
        // emblema propio: simbolo de un triodo (valvula de vacio)
        const Pt c = badgeWinR.getCentre();
        g.drawEllipse (c.x - 15.0f, c.y - 15.0f, 30.0f, 30.0f, 1.4f);
        g.drawLine (c.x - 6.0f, c.y - 7.0f, c.x + 6.0f, c.y - 7.0f, 1.6f); g.drawLine (c.x, c.y - 7.0f, c.x, c.y - 15.0f, 1.2f);
        for (int i = 0; i < 3; ++i) g.drawLine (c.x - 8.0f + 5.6f * (float) i, c.y, c.x - 5.0f + 5.6f * (float) i, c.y, 1.1f);
        g.drawLine (c.x - 15.0f, c.y, c.x - 9.5f, c.y, 1.1f);
        Path k; k.startNewSubPath (c.x - 6.0f, c.y + 5.0f); k.lineTo (c.x - 5.0f, c.y + 7.5f); k.lineTo (c.x + 5.0f, c.y + 7.5f); k.lineTo (c.x + 6.0f, c.y + 5.0f);
        g.strokePath (k, juce::PathStrokeType (1.3f));
        g.drawLine (c.x + 3.0f, c.y + 7.5f, c.x + 3.0f, c.y + 15.0f, 1.2f);
    }
    void drawWhiteInk (Graphics& g)
    {
        g.setColour (Colours::white);
        for (float x : { kHpf.x, kBassF.x, kLmF.x, kHmF.x, kTrF.x }) hzText (g, x, 365.5f);
        textAt (g, "737", 152.2f, 290.8f, serif (10.5f, juce::Font::bold), 1.0f, 0.02f);
        textAt (g, "DJ PAVA", 1358.7f, 234.4f, serif (6.2f, juce::Font::bold), 0.95f, 0.06f);
        textAt (g, "HM MUSIC", 1358.7f, 296.2f, serif (6.2f, juce::Font::bold), 0.92f, 0.04f);
    }

    // ------------------------------------------------------------------ placa de aluminio (se calcula una vez)
    std::vector<float> maskOf (const juce::Image& m, int w, int h, int r)
    {
        std::vector<float> a ((size_t) (w * h)), t ((size_t) (w * h));
        { juce::Image::BitmapData bd (m, juce::Image::BitmapData::readOnly); for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) a[(size_t) (y * w + x)] = bd.getPixelColour (x, y).getFloatAlpha(); }
        if (r <= 0) return a;
        for (int pass = 0; pass < 2; ++pass)
        {
            for (int y = 0; y < h; ++y) { float acc = 0; for (int x = -r; x < w + r; ++x) { if (x + r < w) acc += a[(size_t) (y * w + x + r)]; if (x - r - 1 >= 0) acc -= a[(size_t) (y * w + x - r - 1)]; if (x >= 0 && x < w) t[(size_t) (y * w + x)] = acc / (float) (2 * r + 1); } }
            for (int x = 0; x < w; ++x) { float acc = 0; for (int y = -r; y < h + r; ++y) { if (y + r < h) acc += t[(size_t) ((y + r) * w + x)]; if (y - r - 1 >= 0) acc -= t[(size_t) ((y - r - 1) * w + x)]; if (y >= 0 && y < h) a[(size_t) (y * w + x)] = acc / (float) (2 * r + 1); } }
        }
        return a;
    }
    juce::Image renderPanel()
    {
        static juce::Image cached; static juce::SpinLock lock;
        const juce::SpinLock::ScopedLockType sl (lock);
        if (cached.isValid()) return cached;
        constexpr float k = 1.5f;
        const int iw = (int) ((float) Content::W * k), ih = (int) ((float) Content::panelH * k);
        const float pk = sc * k;                                             // pixeles de imagen por pixel de la foto
        const auto toImg = juce::AffineTransform::translation (-X0, -Y0).scaled (pk);
        auto newMask = [iw, ih] { return juce::Image (juce::Image::SingleChannel, iw, ih, true); };
        juce::Image silk = newMask(), white = newMask(), plate = newMask(), bezel = newMask(), face = newMask(), holes = newMask(), edge = newMask();
        { Graphics g (silk); g.addTransform (toImg); drawSilk (g); }
        for (auto w : { badgeWinL, badgeWinR }) silk.clear (w.transformedBy (toImg).getSmallestIntegerContainer());
        { Graphics g (silk); g.addTransform (toImg); drawBadgeWindows (g); }
        { Graphics g (white); g.addTransform (toImg); drawWhiteInk (g); }
        { Graphics g (plate); g.addTransform (toImg); g.setColour (Colours::white); g.fillRect (Rectangle<float> (641.0f, 141.5f, 235.0f, 239.0f)); }
        { Graphics g (bezel); g.addTransform (toImg); g.setColour (Colours::white); g.fillEllipse (vuBezel.x - vuBx, vuBezel.y - vuBy, vuBx * 2, vuBy * 2); }
        { Graphics g (face); g.addTransform (toImg); g.setColour (Colours::white); g.fillEllipse (vuFace.x - vuRx, vuFace.y - vuRy, vuRx * 2, vuRy * 2); }
        {
            Graphics g (holes); g.addTransform (toImg); g.setColour (Colours::white);
            g.fillRoundedRectangle (102.0f, 150.0f, 28.0f, 16.7f, 8.3f); g.fillRoundedRectangle (102.0f, 357.7f, 29.0f, 16.7f, 8.3f);
            g.fillRoundedRectangle (1381.0f, 150.0f, 28.0f, 16.7f, 8.3f); g.fillRoundedRectangle (1377.0f, 357.7f, 29.0f, 16.7f, 8.3f);
            g.fillEllipse (540.0f - 4.3f, 310.0f - 4.3f, 8.6f, 8.6f);                                 // ajuste del VU
        }
        { Graphics g (edge); g.addTransform (toImg); g.setColour (Colours::white); g.fillRoundedRectangle (X0, Y0, 1322.0f, 242.0f, 7.0f); }
        juce::Image badges = newMask(), vuSh = newMask(), scratch = newMask();
        {
            // micro rayas del aluminio: casi todas siguen el cepillado, unas pocas cruzadas
            Graphics g (scratch); juce::Random rng (737);
            for (int i = 0; i < 420; ++i)
            {
                const float x = rng.nextFloat() * (float) iw, y = rng.nextFloat() * (float) ih;
                const bool cross = rng.nextFloat() < 0.18f;
                const float a = cross ? (rng.nextFloat() - 0.5f) * 2.6f : (rng.nextFloat() - 0.5f) * 0.10f;
                const float len = (cross ? 6.0f + 30.0f * rng.nextFloat() : 30.0f + 160.0f * rng.nextFloat());
                g.setColour (Colours::white.withAlpha (0.25f + 0.6f * rng.nextFloat()));
                g.drawLine (x, y, x + len * std::cos (a), y + len * std::sin (a), cross ? 0.6f : 0.8f);
            }
        }
        {
            Graphics g (badges); g.addTransform (toImg); g.setColour (Colours::white);
            for (auto b : { Rectangle<float> (119.3f, 222.3f, 65.7f, 84.4f), Rectangle<float> (1326.7f, 223.3f, 64.0f, 83.4f) }) g.fillRoundedRectangle (b.reduced (3.2f), 3.0f);
        }
        for (auto w : { badgeWinL, badgeWinR }) badges.clear (w.transformedBy (toImg).getSmallestIntegerContainer());
        { Graphics g (vuSh); g.addTransform (toImg); g.setColour (Colours::white); g.fillEllipse (vuBezel.x - vuBx + 1.2f, vuBezel.y - vuBy + 3.0f, vuBx * 2, vuBy * 2); }
        const auto silkA = maskOf (silk, iw, ih, 0), whiteA = maskOf (white, iw, ih, 0), plateA = maskOf (plate, iw, ih, 0), plateB = maskOf (plate, iw, ih, 3),
                   bezA = maskOf (bezel, iw, ih, 0), bezB = maskOf (bezel, iw, ih, 4), faceA = maskOf (face, iw, ih, 1), holeA = maskOf (holes, iw, ih, 0),
                   edgeA = maskOf (edge, iw, ih, 0), edgeB = maskOf (edge, iw, ih, 4),
                   badgeA = maskOf (badges, iw, ih, 0), badgeB = maskOf (badges, iw, ih, 2), vuShB = maskOf (vuSh, iw, ih, 6),
                   scrA = maskOf (scratch, iw, ih, 0);
        struct Scr { float x, y, r, rot; };
        const std::vector<Scr> screws { { 174.3f, 183.0f, 13.5f, 0.2f }, { 174.3f, 344.0f, 13.5f, 0.9f }, { 1337.0f, 185.0f, 13.5f, 0.5f }, { 1336.0f, 344.3f, 13.5f, 0.1f },
                                        { 661.0f, 168.3f, 9.5f, 0.7f }, { 857.7f, 168.3f, 9.5f, 0.3f }, { 661.0f, 364.3f, 9.5f, 1.1f }, { 857.7f, 364.3f, 9.5f, 0.4f } };
        auto alu = r3d::srgb (0.845f, 0.848f, 0.845f, 0, 0.52f); alu.f0 = 0.05f;
        auto ink = r3d::srgb (0.004f, 0.004f, 0.005f, 0, 0.18f); ink.f0 = 0.008f;
        auto enamel = r3d::srgb (0.010f, 0.010f, 0.011f, 0, 0.80f); enamel.f0 = 0.045f;      // negro de las placas (esmaltado, con bisel)
        const auto paint = r3d::srgb (0.93f, 0.93f, 0.92f, 0, 0.45f);
        const auto bez = r3d::srgb (0.74f, 0.745f, 0.745f, 0, 0.35f);
        const float cxI = (float) iw * 0.5f, cyI = (float) ih * 0.5f;
        auto fn = [&] (float rx, float ry)
        {
            const float px = rx + cxI, py = ry + cyI;
            const int ix = juce::jlimit (0, iw - 1, (int) px), iy = juce::jlimit (0, ih - 1, (int) py);
            const size_t i = (size_t) (iy * iw + ix);
            r3d::Sample s; s.m = alu;
            // aluminio cepillado en horizontal: vetas largas + lineas finas
            const float streak = r3d::fbm (px * 0.0025f, py * 0.75f, 1, 3), fine = r3d::fbm (px * 0.012f, py * 2.6f, 2, 2);
            const float hair = r3d::vnoise (px * 0.025f, py * 3.6f, 9), hair2 = r3d::vnoise (px * 0.06f, py * 7.0f, 13);
            float tone = 0.875f + 0.09f * streak + 0.06f * fine + 0.08f * hair + 0.05f * hair2;
            // zona central mas clara y luminosa (como la luz de estudio sobre el aluminio)
            const float cxn = (px - cxI) / (float) iw;
            tone *= 0.965f + 0.07f * std::exp (-cxn * cxn * 9.0f);
            // reflejo anisotropico del cepillado (banda horizontal del softbox) y micro rayas que atrapan la luz
            const float cyn = py / (float) ih - 0.30f;
            tone *= 1.0f + 0.045f * std::exp (-cyn * cyn * 18.0f);
            tone *= 1.0f + 0.10f * scrA[i] * (r3d::hash (ix / 9, iy / 9, 3) > 0.35f ? 1.0f : -0.7f);
            float h = 0.35f * streak + 0.15f * fine + 0.06f * hair2;
            // placa central (mas clara y levemente elevada, con canto)
            if (plateA[i] > 0) tone *= 1.0f + 0.065f * plateA[i];
            h += 3.0f * plateB[i];
            // bisel del VU: aro mate que baja hacia la cara
            if (bezA[i] > 0)
            {
                s.m = s7p3::mixM (s.m, bez, bezA[i]);
                const float yy = juce::jlimit (-1.0f, 1.0f, (py / pk + Y0 - vuBezel.y) / vuBy);
                tone = (0.97f + 0.03f * fine) * (0.90f + 0.12f * yy);                 // aro mas oscuro arriba y claro abajo
            }
            h -= 4.0f * bezB[i];
            if (faceA[i] > 0) { s.m = s7p3::mixM (s.m, r3d::srgb (0.80f, 0.74f, 0.64f, 0, 0.4f), faceA[i]); h -= 3.0f * faceA[i]; }
            s.m.r *= tone; s.m.g *= tone; s.m.b *= tone;
            // serigrafia
            if (silkA[i] > 0) { s.m = s7p3::mixM (s.m, badgeA[i] > 0.5f ? enamel : ink, std::min (1.0f, silkA[i] * 1.15f)); h += 0.45f * silkA[i] + 1.6f * badgeB[i]; }
            // sombra del bisel del VU sobre la placa
            if (vuShB[i] > 0) { const float k2 = 1.0f - 0.38f * vuShB[i] * (1.0f - bezA[i]); s.m.r *= k2; s.m.g *= k2; s.m.b *= k2; }
            if (whiteA[i] > 0) { s.m = s7p3::mixM (s.m, paint, whiteA[i]); h += 0.3f * whiteA[i]; }
            // canto de la chapa y ranuras de las orejas
            h -= 3.0f * (1.0f - edgeB[i]);
            s.m.alpha = edgeA[i] * (1.0f - holeA[i]);
            // tornillos, jack y LED (piezas 3D)
            const float X = px / pk + X0, Y = py / pk + Y0;
            for (auto& q : screws)
            {
                if (std::abs (X - q.x) > q.r * 2.0f || std::abs (Y - q.y) > q.r * 2.0f) continue;
                const float dx = (X - q.x) * pk, dy = (Y - q.y) * pk, R = q.r * pk, rr = std::sqrt (dx * dx + dy * dy);
                if (rr < R + 1) { const auto p = s7p3::screw (dx, dy, R, q.rot); s.m = s7p3::mixM (s.m, p.m, p.m.alpha); s.m.alpha = 1; h = h * (1 - p.m.alpha) + (h + 1.0f + p.h) * p.m.alpha; }
                else
                {
                    // sombra proyectada hacia abajo y un poco a la derecha + oclusion de contacto
                    const float sx = dx - 0.12f * R, sy = dy - 0.24f * R, sd = std::sqrt (sx * sx + sy * sy);
                    const float shadow = 0.55f * r3d::smooth ((1.05f * R - sd) / (0.30f * R) + 0.5f);
                    const float ao = (1.0f - 0.40f * std::exp (-std::pow ((rr - R) / (0.30f * R), 2.0f))) * (1.0f - shadow);
                    s.m.r *= ao; s.m.g *= ao; s.m.b *= ao;
                }
            }
            if (std::abs (X - 236.7f) < 40 && std::abs (Y - 333.3f) < 40)
            {
                const float dx = (X - 236.7f) * pk, dy = (Y - 333.3f) * pk, R = 36.7f * pk, rr = std::sqrt (dx * dx + dy * dy);
                if (rr < R + 1) { const auto p = s7p3::jack (dx, dy, R); s.m = s7p3::mixM (s.m, p.m, p.m.alpha); h = h * (1 - p.m.alpha) + (h + p.h) * p.m.alpha; }
            }
            if (std::abs (X - ledPos.x) < 10 && std::abs (Y - ledPos.y) < 10)
            {
                const float dx = (X - ledPos.x) * pk, dy = (Y - ledPos.y) * pk, R = ledR * pk, rr = std::sqrt (dx * dx + dy * dy);
                if (rr < R + 1) { const auto p = s7p3::led (dx, dy, R, false); s.m = s7p3::mixM (s.m, p.m, p.m.alpha); h = h * (1 - p.m.alpha) + (h + p.h) * p.m.alpha; }
            }
            if (std::abs (X - 540.0f) < 6 && std::abs (Y - 310.0f) < 6)                          // tornillo de calibracion del VU (dentro del agujero)
            {
                const float dx = (X - 540.0f), dy = (Y - 310.0f), rr = std::sqrt (dx * dx + dy * dy);
                if (rr < 4.6f) { s.m = r3d::srgb (0.10f, 0.10f, 0.105f, 0, 0.5f); s.m.alpha = 1; h = -2.0f + (std::abs (dx * 0.7f + dy * 0.7f) < 0.6f ? -0.8f : 0.0f) - 1.5f * (1.0f - r3d::clamp01 ((4.6f - rr) / 1.0f)); }
            }
            s.h = h; return s;
        };
        const std::function<r3d::V3 (float, float)> view = [iw, ih] (float x, float y) { return r3d::norm ({ -x, -0.1f * (float) ih - y, 1.3f * (float) iw }); };
        cached = r3d::render (iw, ih, fn, {}, &view);
        return cached;
    }
}


//==============================================================================
namespace
{
    const Pt lightDir { -0.62f, -0.78f };                          // luz principal: arriba a la izquierda (como el panel)
    // textura de mecanizado que gira con la perilla (alfa: blanco = brillo, negro = sombra)
    juce::Image makeOverlay (int face, int size, int seed)
    {
        juce::Image img (juce::Image::ARGB, size, size, true);
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
        const float c = (float) size * 0.5f;
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x)
            {
                const float dx = (float) x + 0.5f - c, dy = (float) y + 0.5f - c, r = std::sqrt (dx * dx + dy * dy);
                if (r > c) continue;
                const float phi = std::atan2 (dy, dx), rn = r / c;
                float v = 0.0f;
                if (face == 0)      v = 0.55f * std::sin ((std::sqrt ((dx - 0.06f * c) * (dx - 0.06f * c) + dy * dy)) * 1.9f) + 0.45f * (r3d::fbm (phi * 30.0f, rn * 3.0f, seed, 2) - 0.5f) * 2.0f;
                else if (face == 1) v = (r3d::fbm (phi * 70.0f, rn * 1.5f, seed, 3) - 0.5f) * 3.2f;                  // vetas radiales
                else                v = (r3d::vnoise ((float) x * 0.9f, (float) y * 0.9f, seed) - 0.5f) * 2.4f;      // granallado
                // brillo de mecanizado en un sector: al girar la perilla la luz cambia un poco
                const float sector = std::pow (std::max (0.0f, std::cos (phi + 0.7f)), 10.0f) * (0.4f + 0.6f * rn);
                const float edge = r3d::smooth ((c - r) / 2.0f);
                const float a = juce::jlimit (-1.0f, 1.0f, v) * (face == 0 ? 0.075f : face == 2 ? 0.10f : 0.12f) + 0.16f * sector;
                bd.setPixelColour (x, y, a >= 0 ? Colours::white.withAlpha (a * edge) : Colours::black.withAlpha (-a * edge));
            }
        return img;
    }
}

Look::Look()
{
    setColour (juce::PopupMenu::backgroundColourId, Colour (0xff17181a));
    setColour (juce::PopupMenu::textColourId, Colour (0xffece8e2));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colour (0xff8a1e26));
    setColour (juce::PopupMenu::headerTextColourId, Colour (0xffc9a9a4));
    setColour (juce::TooltipWindow::backgroundColourId, Colour (0xff17181a));
    setColour (juce::TooltipWindow::textColourId, Colour (0xffece8e2));
}
void Look::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider&)
{
    const Rectangle<float> r ((float) x, (float) y + (float) h * 0.5f - 4.0f, (float) w, 8.0f);
    g.setColour (Colour (0xff050606)); g.fillRoundedRectangle (r, 4.0f);
    g.setColour (Colours::white.withAlpha (0.08f)); g.drawHorizontalLine ((int) r.getBottom(), r.getX() + 3, r.getRight() - 3);
    g.setGradientFill (ColourGradient (Colour (0xffff6b70), r.getX(), 0, Colour (0xffa3141e), pos, 0, false));
    g.fillRoundedRectangle (r.withRight (pos).reduced (1.5f), 3.0f);
    const float cx = pos, cy = r.getCentreY();
    g.setColour (Colours::black.withAlpha (0.45f)); g.fillEllipse (cx - 7.0f, cy - 5.5f, 15.0f, 15.0f);
    g.setGradientFill (ColourGradient (Colour (0xfff7f7f5), cx - 4, cy - 7, Colour (0xff7f8285), cx + 5, cy + 7, false));
    g.fillEllipse (cx - 7.5f, cy - 7.5f, 15.0f, 15.0f);
    g.setColour (Colour (0xff2a2b2c)); g.drawEllipse (cx - 7.5f, cy - 7.5f, 15.0f, 15.0f, 1.0f);
}

//==============================================================================
Knob::Knob (APVTS& s, const juce::String& id, float aa0, float aa1, std::vector<float> st) : steps (std::move (st)), a0 (aa0), a1 (aa1), pid (id)
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (a0 * deg, a1 * deg, true);
    setMouseDragSensitivity (steps.empty() ? 260 : 120);
    if (auto* p = s.getParameter (id)) setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    att = std::make_unique<APVTS::SliderAttachment> (s, id, *this);
    style.pointer = false;
}
float Knob::targetAngleDeg() const
{
    if (! steps.empty())
    {
        const int idx = juce::jlimit (0, (int) steps.size() - 1, (int) std::round (getValue() - getMinimum()));
        return steps[(size_t) idx];
    }
    return a0 + (float) getNormalisableRange().convertTo0to1 (getValue()) * (a1 - a0);
}
bool Knob::animate (float dt)
{
    const float t = targetAngleDeg();
    if (! init) { shown = t; vel = 0; init = true; return true; }
    if (std::abs (t - shown) < 0.01f && std::abs (vel) < 0.05f) { if (shown != t) { shown = t; return true; } return false; }
    // resorte criticamente amortiguado (~120 ms): las perillas se mueven con inercia suave
    const float w = steps.empty() ? 34.0f : 30.0f;
    const int sub = 4; const float h = dt / (float) sub;
    for (int i = 0; i < sub; ++i) { const float a = w * w * (t - shown) - 2.0f * w * vel; vel += a * h; shown += vel * h; }
    return true;
}
void Knob::buildImages()
{
    const int iw = getWidth() * 2, ih = getHeight() * 2;
    const float R = radius * 2.0f, shift = faceShift * 2.0f, shiftY = faceShiftY * 2.0f;
    const auto st = style;
    // cuerpo 3D sin indicador + sombra proyectada abajo-derecha y sombra de contacto oscura
    body = r3d::render (iw, ih, [R, shift, shiftY, st] (float px, float py) { return s7p3::knob (px, py, R, 0.0f, shift, shiftY, st); },
                        { 0.11f * R, 0.13f * R, 0.12f * R, 0.78f, 0.05f * R, 0.85f });
    const int os = (int) std::ceil (R * st.faceFrac * 2.0f);
    overlay = makeOverlay (st.face, os, 31 + (int) (radius * 7.0f) + st.face * 5);
    bodyW = getWidth();
}
void Knob::paint (Graphics& g)
{
    if (! body.isValid() || bodyW != getWidth()) buildImages();
    if (! init) animate (0.0f);
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (body, getLocalBounds().toFloat());
    const Pt c (getWidth() * 0.5f + faceShift, getHeight() * 0.5f + faceShiftY);
    const float fr = radius * style.faceFrac - 0.6f;
    const float ang = shown * deg;
    {
        // mecanizado que gira con la perilla
        juce::Graphics::ScopedSaveState ss (g);
        Path clip; clip.addEllipse (c.x - fr, c.y - fr, fr * 2, fr * 2);
        g.reduceClipRegion (clip);
        g.drawImageTransformed (overlay, juce::AffineTransform::translation (-(float) overlay.getWidth() * 0.5f, -(float) overlay.getHeight() * 0.5f)
                                              .scaled (0.5f).rotated (ang).translated (c.x, c.y));
    }
    // indicador blanco en relieve: luz fija arriba-izquierda, sombra abajo-derecha
    const float R = radius, inR = style.ptrIn * R, outR = style.ptrOut * R, w = std::max (2.4f, style.ptrW * R);
    Path bar; bar.addRoundedRectangle (-w * 0.5f, -outR, w, outR - inR, w * 0.45f);
    const auto xf = juce::AffineTransform::rotation (ang).translated (c.x, c.y);
    const Pt nrm (std::cos (ang), std::sin (ang));                     // perpendicular al indicador
    {
        Path sh = bar; sh.applyTransform (xf.translated (1.1f, 1.5f));
        g.setColour (Colours::black.withAlpha (0.30f)); g.fillPath (sh);
        Path sh2 = bar; sh2.applyTransform (xf.translated (0.5f, 0.7f));
        g.setColour (Colours::black.withAlpha (0.22f)); g.fillPath (sh2);
    }
    Path b = bar; b.applyTransform (xf);
    const Pt mid = c + Pt (std::sin (ang), -std::cos (ang)) * ((inR + outR) * 0.5f);
    const float side = nrm.x * lightDir.x + nrm.y * lightDir.y;      // que lado mira a la luz
    auto shade = [] (float k) { k = juce::jlimit (0.0f, 1.0f, k); return Colour (0xff8e949a).interpolatedWith (Colour (0xffffffff), k); };
    ColourGradient grad (shade (0.55f + 0.6f * side), mid.x + nrm.x * w * 0.5f, mid.y + nrm.y * w * 0.5f,
                         shade (0.55f - 0.6f * side), mid.x - nrm.x * w * 0.5f, mid.y - nrm.y * w * 0.5f, false);
    grad.addColour (0.45, Colour (0xfffdfdfc));
    g.setGradientFill (grad); g.fillPath (b);
    // brillo en el canto del indicador: depende del angulo respecto a la luz
    const float glint = 0.25f + 0.65f * std::abs (side);
    const Pt off = nrm * (w * 0.22f * (side >= 0 ? 1.0f : -1.0f));
    const Pt p0 = c + Pt (std::sin (ang), -std::cos (ang)) * (inR + w * 0.6f) + off, p1 = c + Pt (std::sin (ang), -std::cos (ang)) * (outR - w * 0.6f) + off;
    g.setColour (Colours::white.withAlpha (glint)); g.drawLine ({ p0, p1 }, 0.8f);
    g.setColour (Colours::black.withAlpha (0.38f)); g.strokePath (b, juce::PathStrokeType (0.6f));
}

//==============================================================================
bool LampBtn::lit() const { return param (proc, id) > 0.5f && powerOn (proc); }
bool LampBtn::animate (float dt)
{
    const float t = lit() ? 1.0f : 0.0f;
    if (level < 0) { level = t; return true; }
    if (std::abs (t - level) < 0.002f) { if (level != t) { level = t; return true; } return false; }
    // la bombilla tarda un poco en calentarse y en apagarse (~90 ms)
    level += (t - level) * juce::jmin (1.0f, dt * (t > level ? 13.0f : 9.0f));
    return true;
}
void LampBtn::paint (Graphics& g)
{
    if (level < 0) level = lit() ? 1.0f : 0.0f;
    for (int on = 0; on < 2; ++on)
    {
        auto& im = cache[on];
        if (im.isValid()) continue;
        const int w = getWidth() * 2, h = getHeight() * 2;
        juce::Image tm (juce::Image::SingleChannel, w, h, true);
        {
            Graphics tg (tm); tg.setColour (Colours::white);
            textAt (tg, text, (float) w * 0.5f, (float) h * 0.5f + L (0.3f) * 2, sans (L (5.4f) * 2.0f), 0.95f);
        }
        const float hw = L (capHW) * 2.0f, hh = L (capHH) * 2.0f;
        auto textFn = [&tm, w, h] (float x, float y)
        {
            const int ix = (int) (x + (float) w * 0.5f), iy = (int) (y + (float) h * 0.5f);
            if (ix < 0 || iy < 0 || ix >= w || iy >= h) return 0.0f;
            return tm.getPixelAt (ix, iy).getFloatAlpha();
        };
        im = r3d::render (w, h, [hw, hh, on, &textFn] (float x, float y) { return s7p3::lampButton (x, y, hw, hh, on == 1, textFn); },
                          { 0.07f * hh, 0.24f * hh, 0.22f * hh, 0.55f, 0.08f * hh, 0.6f });
    }
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    const auto bnd = getLocalBounds().toFloat();
    if (level < 1.0f) g.drawImage (cache[0], bnd);
    if (level > 0.0f)
    {
        g.setOpacity (level); g.drawImage (cache[1], bnd); g.setOpacity (1.0f);
        // luz que se escapa por la ranura alrededor de la tapa y resplandor sobre el aluminio
        const auto c = bnd.getCentre();
        const Rectangle<float> cap (c.x - L (capHW), c.y - L (capHH) + L (1.0f), L (capHW) * 2, L (capHH) * 2);
        g.setColour (Colour (0xffff3a40).withAlpha (0.30f * level)); g.drawRoundedRectangle (cap.expanded (1.2f), 3.0f, 1.2f);
        g.setColour (Colour (0xffff2a30).withAlpha (0.12f * level)); g.drawRoundedRectangle (cap.expanded (2.8f), 4.0f, 2.6f);
        ColourGradient halo (Colour (0xffff2a30).withAlpha (0.20f * level), c.x, c.y, Colour (0x00ff2a30), c.x + L (capHW) * 1.7f, c.y, true);
        g.setGradientFill (halo);
        g.fillEllipse (c.x - L (capHW) * 1.7f, c.y - L (capHH) * 2.3f, L (capHW) * 3.4f, L (capHH) * 4.6f);
    }
}
void LampBtn::mouseUp (const juce::MouseEvent& e)
{
    if (! getLocalBounds().contains (e.getPosition())) return;
    proc.undoManager.beginNewTransaction (text.isEmpty() ? "Meter" : text);
    setParam (proc, id, param (proc, id) > 0.5f ? 0.0f : 1.0f);
}

void Rocker::paint (Graphics& g)
{
    const int on = powerOn (proc) ? 1 : 0;
    auto& im = cache[on];
    if (! im.isValid())
    {
        const int w = getWidth() * 2, h = getHeight() * 2;
        juce::Image sm (juce::Image::SingleChannel, w, h, true);
        {
            Graphics sg (sm); sg.setColour (Colours::white);
            const float cx = (float) w * 0.5f, cy = (float) h * 0.5f, u = (float) w / 70.0f;
            sg.drawEllipse (cx - 13.0f * u - 3.2f * u, cy - 4.3f * u, 6.4f * u, 8.6f * u, 1.1f * u);
            sg.fillRect (cx + 6.0f * u, cy - 0.6f * u, 10.0f * u, 1.2f * u);
        }
        const float hw = (float) w * 0.5f - 2.0f, hh = (float) h * 0.5f - 2.0f;
        im = r3d::render (w, h, [hw, hh, on, &sm, w, h] (float x, float y)
        {
            const int ix = (int) (x + (float) w * 0.5f), iy = (int) (y + (float) h * 0.5f);
            const float m = (ix >= 0 && iy >= 0 && ix < w && iy < h) ? sm.getPixelAt (ix, iy).getFloatAlpha() : 0.0f;
            return s7p3::rocker (x, y, hw, hh, on == 1, m);
        }, { 0.03f * hh, 0.08f * hh, 0.10f * hh, 0.5f, 0.04f * hh, 0.5f });
    }
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (im, getLocalBounds().toFloat());
}
void Rocker::mouseUp (const juce::MouseEvent&)
{
    proc.undoManager.beginNewTransaction ("Power");
    setParam (proc, "power", powerOn (proc) ? 0.0f : 1.0f); repaint();
}

//==============================================================================
namespace
{
    // fondo de la barra superior con el logo y el nombre (se dibuja una vez)
    void drawHmLogo (Graphics& g, float x, float y, float d)
    {
        const Pt c (x + d * 0.5f, y + d * 0.5f);
        g.setGradientFill (ColourGradient (Colour (0xfff4f4f2), c.x - d * 0.3f, y, Colour (0xff6e7174), c.x + d * 0.3f, y + d, false));
        g.fillEllipse (x, y, d, d);
        g.setColour (Colour (0xff0d0e0f)); g.fillEllipse (x + 2.2f, y + 2.2f, d - 4.4f, d - 4.4f);
        g.setGradientFill (ColourGradient (Colour (0xff2a2c2f), c.x, y, Colour (0xff0a0b0c), c.x, y + d, false));
        g.fillEllipse (x + 3.4f, y + 3.4f, d - 6.8f, d - 6.8f);
        // monograma HM: H y M unidas por el trazo central, con una onda roja (la senal) debajo
        const float s = d / 34.0f;
        Path hm;
        auto bar = [&hm, s, c] (float x0, float y0, float x1, float y1) { hm.addRectangle (c.x + x0 * s, c.y + y0 * s, (x1 - x0) * s, (y1 - y0) * s); };
        bar (-11.5f, -8.0f, -8.6f, 6.0f); bar (-8.6f, -1.6f, -3.0f, 1.0f); bar (-3.0f, -8.0f, -0.1f, 6.0f);
        Path m; m.startNewSubPath (c.x + 1.6f * s, c.y + 6.0f * s); m.lineTo (c.x + 1.6f * s, c.y - 8.0f * s); m.lineTo (c.x + 6.6f * s, c.y - 0.5f * s);
        m.lineTo (c.x + 11.6f * s, c.y - 8.0f * s); m.lineTo (c.x + 11.6f * s, c.y + 6.0f * s);
        g.setGradientFill (ColourGradient (Colour (0xffffffff), c.x, c.y - 8 * s, Colour (0xffa9adb1), c.x, c.y + 6 * s, false));
        g.fillPath (hm);
        g.strokePath (m, juce::PathStrokeType (2.7f * s, juce::PathStrokeType::mitered, juce::PathStrokeType::square));
        Path wv; wv.startNewSubPath (c.x - 11.0f * s, c.y + 10.0f * s);
        for (int i = 0; i <= 24; ++i) { const float t = (float) i / 24.0f; wv.lineTo (c.x + (-11.0f + 22.0f * t) * s, c.y + (10.0f - 1.8f * std::sin (t * 12.566f) * std::sin (t * 3.1416f)) * s); }
        g.setColour (Colour (0xffff3b45)); g.strokePath (wv, juce::PathStrokeType (1.2f * s));
    }
    juce::Image buildStrip()
    {
        const int w = Content::W, h = Content::strip;
        juce::Image img (juce::Image::ARGB, w, h, true);
        Graphics g (img);
        // aluminio anodizado negro cepillado con bisel inferior
        g.setGradientFill (ColourGradient (Colour (0xff222427), 0, 0, Colour (0xff101112), 0, (float) h, false)); g.fillAll();
        juce::Random rng (5);
        for (int i = 0; i < 900; ++i)
        {
            const float y = rng.nextFloat() * (float) h, x = rng.nextFloat() * (float) w, len = 40.0f + rng.nextFloat() * 260.0f;
            g.setColour (Colours::white.withAlpha (0.012f + 0.02f * rng.nextFloat())); g.drawHorizontalLine ((int) y, x, x + len);
        }
        g.setColour (Colours::white.withAlpha (0.07f)); g.drawHorizontalLine (0, 0, (float) w);
        g.setColour (Colours::black); g.drawHorizontalLine (h - 1, 0, (float) w);
        g.setColour (Colours::white.withAlpha (0.05f)); g.drawHorizontalLine (h - 2, 0, (float) w);
        // logo propio
        drawHmLogo (g, 16, 10, 34);
        g.setFont (juce::Font (juce::FontOptions (sansName(), 15.0f, juce::Font::bold)).withExtraKerningFactor (0.20f));
        g.setGradientFill (ColourGradient (Colour (0xfff7f7f5), 0, 14, Colour (0xff9da1a5), 0, 32, false));
        g.drawText ("HM MUSIC", Rectangle<float> (58, 11, 180, 18), juce::Justification::centredLeft);
        g.setFont (juce::Font (juce::FontOptions (sansName(), 10.0f, juce::Font::bold)).withExtraKerningFactor (0.32f));
        g.setColour (Colour (0xffd64a50));
        g.drawText ("DJ PAVA", Rectangle<float> (59, 29, 180, 14), juce::Justification::centredLeft);
        // nombre del producto con presencia
        const float nx = 1396;
        g.setFont (juce::Font (juce::FontOptions (sansName(), 21.0f, juce::Font::bold)).withExtraKerningFactor (0.06f));
        g.setColour (Colours::black.withAlpha (0.6f)); g.drawText ("TOP STRIP", Rectangle<float> (nx + 1, 13, 130, 28), juce::Justification::centredLeft);
        g.setGradientFill (ColourGradient (Colour (0xffffffff), 0, 13, Colour (0xff8f9397), 0, 40, false));
        g.drawText ("TOP STRIP", Rectangle<float> (nx, 12, 130, 28), juce::Justification::centredLeft);
        const float tw = juce::GlyphArrangement::getStringWidth (juce::Font (juce::FontOptions (sansName(), 21.0f, juce::Font::bold)).withExtraKerningFactor (0.06f), "TOP STRIP");
        g.setColour (Colour (0x40ff2a30)); g.drawText ("737", Rectangle<float> (nx + tw + 7, 12, 60, 28).translated (0, 1), juce::Justification::centredLeft);
        g.setGradientFill (ColourGradient (Colour (0xffff5a60), 0, 13, Colour (0xffb5121c), 0, 40, false));
        g.drawText ("737", Rectangle<float> (nx + tw + 6, 12, 60, 28), juce::Justification::centredLeft);
        const float vx = nx + tw + 6 + juce::GlyphArrangement::getStringWidth (juce::Font (juce::FontOptions (sansName(), 21.0f, juce::Font::bold)), "737") + 8;
        const Rectangle<float> vb (vx, 17, 26, 18);
        g.setColour (Colour (0xff1a0a0b)); g.fillRoundedRectangle (vb, 4);
        g.setColour (Colour (0xffd8343c)); g.drawRoundedRectangle (vb.reduced (0.5f), 4, 1.2f);
        g.setFont (juce::Font (juce::FontOptions (sansName(), 11.0f, juce::Font::bold))); g.setColour (Colour (0xffffe3e3));
        g.drawText ("V3", vb, juce::Justification::centred);
        return img;
    }
    void iconArrow (Graphics& g, Rectangle<float> r, bool redo, Colour c)
    {
        const auto ct = r.getCentre(); const float s = redo ? 1.0f : -1.0f;
        Path a; a.addCentredArc (ct.x, ct.y + 2.0f, 6.5f, 6.0f, 0, -1.9f * s, 0.9f * s, true);
        g.setColour (c); g.strokePath (a, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const Pt tip = Pt (ct.x + 6.5f * std::sin (0.9f * s), ct.y + 2.0f - 6.0f * std::cos (0.9f * s));
        Path h; h.addTriangle (tip.x + 3.6f * s, tip.y - 1.0f, tip.x - 1.2f * s, tip.y + 3.4f, tip.x - 1.0f * s, tip.y - 3.6f);
        g.fillPath (h);
    }
    void stripButton (Graphics& g, Rectangle<float> r, bool hot, bool active, float radius = 5.0f)
    {
        g.setColour (Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (r.translated (0, 1.5f), radius);
        g.setGradientFill (ColourGradient (active ? Colour (0xff4a1215) : (hot ? Colour (0xff3b3e42) : Colour (0xff2c2e31)), 0, r.getY(),
                                           active ? Colour (0xff22080a) : Colour (0xff17181a), 0, r.getBottom(), false));
        g.fillRoundedRectangle (r, radius);
        g.setColour (Colours::white.withAlpha (hot ? 0.20f : 0.11f)); g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + 3, r.getRight() - 3);
        g.setColour (active ? Colour (0xffc8323a) : Colours::black); g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
    }
}

Content::Content (TopStrip3Processor& p)
    : proc (p),
      preGain (p.apvts, "pre_gain", -140, 140), input (p.apvts, "input", -60, 60, { -40, 0, 40 }), hpf (p.apvts, "hpf", -137.5f, 137.5f),
      thr (p.apvts, "thr", -125, 125), ratio (p.apvts, "ratio", -125, 125), attack (p.apvts, "att", -128, 128), release (p.apvts, "rel", -128, 128),
      bass (p.apvts, "bass_gain", -125, 125), bassF (p.apvts, "bass_freq", -60, 60, { -46, -15, 15, 46 }),
      lm (p.apvts, "lm_gain", -125, 125), lmF (p.apvts, "lm_freq", -150, 150, stepsFrom (-150, 30, 11)),
      hm (p.apvts, "hm_gain", -125, 125), hmF (p.apvts, "hm_freq", -150, 150, stepsFrom (-150, 30, 11)),
      treble (p.apvts, "tr_gain", -125, 125), trebleF (p.apvts, "tr_freq", -60, 60, { -46, -15, 15, 46 }),
      out (p.apvts, "out", -125, 125),
      bHigh (p, "high_gain", "ON"), bPhase (p, "phase", "REV"), bP48 (p, "p48", "ON"), bFilter (p, "hpf_on", "IN"),
      bPre (p, "eq_pre", "PRE"), bMeter (p, "meter_gr", ""), bComp (p, "comp_on", "IN"),
      bLmQ (p, "lm_hiq", "IN"), bLmX (p, "lm_x10", "X10"), bHmQ (p, "hm_hiq", "IN"), bHmX (p, "hm_x10", "X10"),
      bSc (p, "sc_link", "ON"), bEq (p, "eq_on", "IN"),
      rocker (p)
{
    r3d::envFloor = 1.6f;                                  // fondo blanco de foto de producto: el bisel inferior brilla como en el original
    struct KS { Knob* k; const KP* kp; int face; };
    for (auto [k, kp, face] : { KS { &preGain, &kPre, 0 }, { &input, &kInput, 0 }, { &hpf, &kHpf, 0 },
                                { &thr, &kThr, 1 }, { &ratio, &kRatio, 1 }, { &attack, &kAtt, 1 }, { &release, &kRel, 1 },
                                { &bass, &kBass, 2 }, { &bassF, &kBassF, 2 }, { &lm, &kLm, 2 }, { &lmF, &kLmF, 2 }, { &hm, &kHm, 2 }, { &hmF, &kHmF, 2 },
                                { &treble, &kTr, 2 }, { &trebleF, &kTrF, 2 }, { &out, &kOut, 2 } })
    {
        k->radius = L (kp->r);
        // perspectiva de la foto: la cara se corre hacia afuera del centro del panel y deja ver el lado moleteado
        const float shPhoto = kp->x < 495.0f ? -7.0f * juce::jmin (1.0f, (495.0f - kp->x) / 262.0f) : juce::jmin (7.0f, 0.011f * (kp->x - 495.0f));
        k->faceShift = L (shPhoto) * kp->r / 27.0f;
        k->faceShiftY = L (0.055f * (kp->y - 272.0f)) * kp->r / 27.0f;
        k->style.face = face;
        if (face == 1) { k->style.knurl = 110; k->style.ptrW = 0.12f; }                       // compresor: moleteado fino
        if (face == 2) { k->style.knurl = 80; k->style.ptrW = 0.11f; k->style.ptrOut = 0.70f; } // EQ: indicador mas largo y fino
        if (kp->r < 22.0f) { k->style.faceFrac = 0.74f; k->style.knurl = 70; k->style.ptrIn = 0.22f; k->style.ptrOut = 0.70f; k->style.ptrW = 0.15f; }
        addAndMakeVisible (k);
        k->addListener (this);
    }
    for (auto* b : { &bHigh, &bPhase, &bP48, &bFilter, &bPre, &bMeter, &bComp, &bLmQ, &bLmX, &bHmQ, &bHmX, &bSc, &bEq }) addAndMakeVisible (b);
    addAndMakeVisible (rocker);
    mix.setSliderStyle (juce::Slider::LinearHorizontal);
    mix.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    mix.setDoubleClickReturnValue (true, 100.0);
    mixAtt = std::make_unique<APVTS::SliderAttachment> (p.apvts, "mix", mix);
    mix.setTooltip (U ("MIX: mezcla la señal original con la procesada (compresión en paralelo). 100 % = solo el canal"));
    mix.addListener (this);
    addAndMakeVisible (mix);

    preGain.setTooltip (U ("PREAMP GAIN: cuánto se empuja la válvula (escala LINE: -28 a +9 dB). Más ganancia = más color"));
    input.setTooltip (U ("INPUT: LINE (limpio), INSTRUMENT (Hi-Z) o MIC (transformador). Respuesta medida con impulsos del aparato"));
    hpf.setTooltip (U ("HIGH PASS: filtro de graves de 6 dB/oct, 30 a 140 Hz (se activa con FILTER IN)"));
    thr.setTooltip (U ("THRESHOLD del compresor óptico: -30 a +20 dB respecto a 0 VU (-18 dBFS)"));
    ratio.setTooltip (U ("COMPRESSION (RATIO): de 1:1 a 20:1"));
    attack.setTooltip (U ("ATTACK: de 2 ms (FAST) a 200 ms (SLOW)"));
    release.setTooltip (U ("RELEASE: de 100 ms (FAST) a 5 s (SLOW). La celda óptica recuerda: tras frases largas suelta más despacio"));
    bass.setTooltip (U ("BASS: shelf pasivo ±24 dB")); bassF.setTooltip (U ("Frecuencia del BASS: 15, 30, 60 o 150 Hz"));
    lm.setTooltip (U ("LOW MID: campana ±16 dB")); lmF.setTooltip (U ("Frecuencia del LOW MID: 35-450 Hz (x10 = 350 Hz - 4.5 kHz)"));
    hm.setTooltip (U ("HIGH MID: campana ±16 dB")); hmF.setTooltip (U ("Frecuencia del HIGH MID: 220 Hz - 2.8 kHz (x10 = 2.2 - 28 kHz)"));
    treble.setTooltip (U ("TREBLE: shelf pasivo ±20 dB")); trebleF.setTooltip (U ("Frecuencia del TREBLE: 10, 15, 20 o 32 kHz (aire)"));
    out.setTooltip (U ("OUTPUT: volumen de salida (-40 a +10 dB)"));
    bHigh.setTooltip (U ("HIGH GAIN: +8 dB en LINE, +18 dB en MIC / INSTRUMENT, con menos realimentación (más color)"));
    bPhase.setTooltip (U ("PHASE REV: invierte la polaridad")); bP48.setTooltip (U ("+48v: solo decorativo (en el plugin no hace falta)"));
    bFilter.setTooltip (U ("FILTER IN: activa el pasa altos"));
    bPre.setTooltip (U ("EQ > COMP PRE: el EQ va antes del compresor")); bMeter.setTooltip (U ("METER: el VU muestra la compresión (GR) en vez del nivel"));
    bComp.setTooltip (U ("COMPRESSOR IN"));
    bLmQ.setTooltip (U ("HI-Q: campana más estrecha (Q 0.2 -> 0.85)")); bHmQ.setTooltip (U ("HI-Q: campana más estrecha (Q 0.2 -> 0.85)"));
    bLmX.setTooltip (U ("X10: frecuencias del LOW MID por 10")); bHmX.setTooltip (U ("X10: frecuencias del HIGH MID por 10"));
    bSc.setTooltip (U ("SC > MIDS: las bandas medias solo actúan en el sidechain del compresor (de-esser)"));
    bEq.setTooltip (U ("EQUALIZER IN"));
    rocker.setTooltip (U ("POWER: apagado = señal original"));

    panel = renderPanel();
    {
        const int d = (int) std::ceil (L (ledR) * 2.0f) + 4;
        const float R = L (ledR) * 2.0f;
        ledOn = r3d::render (d * 2, d * 2, [R] (float x, float y) { return s7p3::led (x, y, R, true); });
        ledOff = r3d::render (d * 2, d * 2, [R] (float x, float y) { return s7p3::led (x, y, R, false); });
    }
    stripImg = buildStrip();
    glow = powerOn (proc) ? 1.0f : 0.0f;
    setRepaintsOnMouseActivity (false);
}
Content::~Content()
{
    for (auto* k : { &preGain, &input, &hpf, &thr, &ratio, &attack, &release, &bass, &bassF, &lm, &lmF, &hm, &hmF, &treble, &trebleF, &out }) k->removeListener (this);
    mix.removeListener (this);
}

void Content::newGesture (const char* name) { proc.undoManager.beginNewTransaction (name); }
void Content::sliderValueChanged (juce::Slider* s)
{
    if (s == &mix) { repaint (1040, 0, 220, strip); return; }
    pillKnob = dynamic_cast<Knob*> (s); pillTime = juce::Time::getMillisecondCounterHiRes();
}
void Content::sliderDragStarted (juce::Slider* s)
{
    if (auto* k = dynamic_cast<Knob*> (s)) { newGesture (k->pid.toRawUTF8()); sliderValueChanged (s); }
    else newGesture ("Mix");
}

void Content::resized()
{
    auto place = [] (Knob& c, const KP& k)
    {
        const Pt p = P (k.x, k.y); const float d = L (k.r) * kBox * 2.0f;
        c.setBounds (juce::roundToInt (p.x - d * 0.5f), juce::roundToInt (p.y - d * 0.5f), juce::roundToInt (d), juce::roundToInt (d));
    };
    place (preGain, kPre); place (input, kInput); place (hpf, kHpf); place (thr, kThr); place (ratio, kRatio); place (attack, kAtt); place (release, kRel);
    place (bass, kBass); place (bassF, kBassF); place (lm, kLm); place (lmF, kLmF); place (hm, kHm); place (hmF, kHmF); place (treble, kTr); place (trebleF, kTrF);
    place (out, kOut);
    auto btn = [] (LampBtn& b, const BP& q)
    {
        const Pt p = P (q.x, q.y); const float w = L (capHW) * 2.0f + 12.0f, h = L (capHH) * 2.0f + 12.0f;
        b.setBounds (juce::roundToInt (p.x - w * 0.5f), juce::roundToInt (p.y - h * 0.5f), juce::roundToInt (w), juce::roundToInt (h));
    };
    btn (bHigh, bpHigh); btn (bPhase, bpPhase); btn (bP48, bpP48); btn (bFilter, bpFilter); btn (bPre, bpPre); btn (bMeter, bpMeter); btn (bComp, bpComp);
    btn (bLmQ, bpLmQ); btn (bLmX, bpLmX); btn (bHmQ, bpHmQ); btn (bHmX, bpHmX); btn (bSc, bpSc); btn (bEq, bpEq);
    { const Pt a = P (1240.0f, 306.7f), b = P (1310.0f, 359.0f); rocker.setBounds (Rectangle<float> (a, b).toNearestInt()); }
    mix.setBounds (1078, 12, 118, 30);
}

//==============================================================================
namespace
{
    // posicion lineal de la aguja (1 = 0 VU) -> angulo del medidor
    constexpr float restAngle = -36.0f;
    float needleAngle (float p)
    {
        if (p <= 0.1f) return restAngle + (vuAng[0] - restAngle) * juce::jmax (0.0f, p) / 0.1f;
        return vuAngle (20.0f * std::log10 (p));
    }
}

void Content::paintVu (Graphics& g)
{
    const auto xf = photoToContent();
    Path win; win.addEllipse (vuFace.x - vuRx, vuFace.y - vuRy, vuRx * 2, vuRy * 2); win.applyTransform (xf);
    const auto wb = win.getBounds();
    const float gl = glow;
    // luz calida que se escapa hacia el bisel
    if (gl > 0.01f)
    {
        juce::Graphics::ScopedSaveState s0 (g);
        Path ring; ring.addEllipse (wb.expanded (L (7.0f), L (6.0f))); ring.setUsingNonZeroWinding (false); ring.addEllipse (wb);
        g.reduceClipRegion (ring);
        g.setGradientFill (ColourGradient (Colour (0xffffb54a).withAlpha (0.30f * gl), wb.getCentreX(), wb.getCentreY(),
                                           Colour (0x00ffb54a), wb.getCentreX() + wb.getWidth() * 0.62f, wb.getCentreY(), true));
        g.fillEllipse (wb.expanded (L (7.0f), L (6.0f)));
    }
    // capas en cache: cara + escala (cambia solo con la lampara) y cristal (fija); la aguja se dibuja en vivo
    const auto area = wb.expanded (2.0f).getSmallestIntegerContainer();
    auto layer = [&area] (std::function<void (Graphics&)> fn)
    {
        juce::Image im (juce::Image::ARGB, area.getWidth() * 2, area.getHeight() * 2, true);
        Graphics gi (im); gi.addTransform (juce::AffineTransform::translation ((float) -area.getX(), (float) -area.getY()).scaled (2.0f));
        fn (gi); return im;
    };
    if (! vuFaceImg.isValid() || std::abs (vuFaceGlow - gl) > 0.015f || (gl > 0.999f && vuFaceGlow < 0.999f) || (gl < 0.001f && vuFaceGlow > 0.001f))
    {
        vuFaceImg = layer ([this, &win, &wb, gl] (Graphics& g) { drawVuFace (g, win, wb, gl); });
        vuFaceGlow = gl;
    }
    if (! vuGlassImg.isValid()) vuGlassImg = layer ([this, &win, &wb] (Graphics& g) { drawVuGlass (g, win, wb); });
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (vuFaceImg, area.toFloat());
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (win);
        const Pt pv = P (vuPivot.x, vuPivot.y);
    // ---- aguja: afinada hacia la punta, con sombra suave sobre la escala y reflejo tenue en el cristal
        {
            const float a = needleAngle (needle) * deg;
            Path nd;
            const float r0 = L (88.0f), r1 = L (vuR + 12.0f), w0 = L (1.05f), w1 = L (0.42f);
            nd.startNewSubPath (-w0, -r0); nd.lineTo (w0, -r0); nd.lineTo (w1, -r1); nd.lineTo (-w1, -r1); nd.closeSubPath();
            const auto nx = juce::AffineTransform::rotation (a).translated (pv.x, pv.y);
            Path s1 = nd; s1.applyTransform (nx.translated (L (2.4f), L (2.6f)));
            g.setColour (Colour (0xff3a2208).withAlpha (0.16f)); g.fillPath (s1);
            Path s2 = nd; s2.applyTransform (nx.translated (L (1.3f), L (1.4f)));
            g.setColour (Colour (0xff3a2208).withAlpha (0.20f)); g.fillPath (s2);
            Path n = nd; n.applyTransform (nx);
            g.setColour (Colour (0xff0e0c0a)); g.fillPath (n);
            if (gl > 0.01f)
            {
                Path gh = nd; gh.applyTransform (nx.translated (-L (2.6f), -L (0.4f)));
                g.setColour (Colour (0xfffff2cc).withAlpha (0.16f * gl)); g.fillPath (gh);
            }
        }
    }
    g.drawImage (vuGlassImg, area.toFloat());
}

void Content::drawVuFace (Graphics& g, const Path& win, Rectangle<float> wb, float gl)
{
    juce::Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (win);
    // cara: crema retroiluminada por una lampara calida (punto caliente abajo al centro)
    {
        const Colour off0 (0xffcdbfa6), off1 (0xff9c8c72);
        const Colour on0 (0xfffff4cf), on1 (0xffe39a42);
        const Colour c0 = off0.interpolatedWith (on0, gl), c1 = off1.interpolatedWith (on1, gl);
        const Pt hot = P (vuFace.x, vuFace.y + 18.0f);
        ColourGradient face (c0, hot.x, hot.y, c1, hot.x + L (112), hot.y, true);
        face.addColour (0.50, off0.interpolatedWith (Colour (0xfff9d58a), gl));
        g.setGradientFill (face); g.fillPath (win);
        // grano del papel de la escala
        juce::Random rng (11);
        g.setColour (Colours::black.withAlpha (0.025f));
        for (int i = 0; i < 260; ++i) { const float x = wb.getX() + rng.nextFloat() * wb.getWidth(), y = wb.getY() + rng.nextFloat() * wb.getHeight(); g.fillRect (x, y, 0.8f, 0.8f); }
    }
    const Pt pv = P (vuPivot.x, vuPivot.y);
    const Colour inkC (0xff2e241b), redC (0xffc81e24);
    // ---- escala detallada
    {
        g.setColour (inkC);
        Path arc; arc.addCentredArc (pv.x, pv.y, L (vuR), L (vuR), 0, vuAng[0] * deg, vuAngle (0) * deg, true);
        g.strokePath (arc, juce::PathStrokeType (L (1.0f)));
        // zona roja: banda gruesa de 0 a +3
        Path red; red.addCentredArc (pv.x, pv.y, L (vuR + 1.6f), L (vuR + 1.6f), 0, vuAngle (0) * deg, vuAng[12] * deg, true);
        g.setColour (redC); g.strokePath (red, juce::PathStrokeType (L (3.4f)));
        Path redThin; redThin.addCentredArc (pv.x, pv.y, L (vuR), L (vuR), 0, vuAngle (0) * deg, vuAng[12] * deg, true);
        g.strokePath (redThin, juce::PathStrokeType (L (1.0f)));
        // rayas principales y secundarias (cada 0.5 dB arriba de -3)
        for (int i = 0; i < 13; ++i)
        {
            const bool redZ = vuDb[i] > 0;
            const bool major = ! (vuDb[i] == -6 || vuDb[i] == -4);
            g.setColour (redZ ? redC : inkC);
            g.drawLine ({ polar (pv, L (vuR), vuAng[i]), polar (pv, L (vuR + (major ? 7.5f : 5.0f)), vuAng[i]) }, L (major ? 1.2f : 0.9f));
        }
        for (float db = -2.5f; db < 3.0f; db += 1.0f)
        {
            g.setColour (db > 0 ? redC : inkC);
            g.drawLine ({ polar (pv, L (vuR), vuAngle (db)), polar (pv, L (vuR + 3.8f), vuAngle (db)) }, L (0.75f));
        }
        for (float db : { -15.0f, -8.0f }) { g.setColour (inkC); g.drawLine ({ polar (pv, L (vuR), vuAngle (db)), polar (pv, L (vuR + 3.8f), vuAngle (db)) }, L (0.75f)); }
        g.setColour (inkC);
        g.drawLine ({ polar (pv, L (vuR), vuAng[0]), polar (pv, L (vuR + 9.0f), vuAng[0] - 3.5f) }, L (1.05f));
        g.setColour (redC);
        g.drawLine ({ polar (pv, L (vuR + 1.6f), vuAng[12]), polar (pv, L (vuR + 9.0f), vuAng[12] + 3.5f) }, L (1.05f));
        const auto nf = serif (L (6.2f));
        const char* lab[] { "20", "10", "7", nullptr, "5", nullptr, "3", "2", "1", "0", "1", "2", "3" };
        for (int i = 1; i < 12; ++i)
        {
            if (lab[i] == nullptr) continue;
            g.setColour (vuDb[i] > 0 ? redC : inkC);
            const auto q = polar (pv, L (vuR + 15.5f), vuAng[i]);
            textAt (g, lab[i], q.x, q.y, nf);
        }
        { const Pt a = P (670.0f, 214.5f), b = P (847.0f, 214.5f); g.setColour (inkC); textAt (g, "20", a.x, a.y, nf); g.setColour (redC); textAt (g, "3", b.x, b.y, nf); }
        // escala de porcentaje (puntos debajo del arco)
        g.setColour (inkC.withAlpha (0.85f));
        const auto pf = sans (L (3.4f), false);
        for (int pc = 0; pc <= 100; pc += 20)
        {
            const float a = pc == 0 ? vuAng[0] : vuAngle (20.0f * std::log10 ((float) pc / 100.0f));
            const auto d = polar (pv, L (vuR - 5.0f), a);
            g.fillEllipse (d.x - L (0.9f), d.y - L (0.9f), L (1.8f), L (1.8f));
            const auto q = polar (pv, L (vuR - 10.5f), a);
            if (pc > 0) textAt (g, juce::String (pc), q.x, q.y, pf);
        }
        g.setColour (inkC);
        const Pt vu = P (vuFace.x, 236.0f), dc = P (vuFace.x, 249.0f);
        textAt (g, "VU", vu.x, vu.y, serif (L (10.0f)), 1.08f, 0.08f);
        textAt (g, "decibels", dc.x, dc.y, juce::Font (juce::FontOptions (serifName(), L (10.5f), juce::Font::plain)), 1.0f, 0.22f);
        const Pt mi = P (695.0f, 234.3f), pl = P (821.7f, 234.3f);
        g.fillRect (Rectangle<float> (mi.x - L (4.5f), mi.y - L (0.6f), L (9.0f), L (1.2f)));
        g.setColour (redC);
        g.fillRect (Rectangle<float> (pl.x - L (4.5f), pl.y - L (0.6f), L (9.0f), L (1.2f)));
        g.fillRect (Rectangle<float> (pl.x - L (0.6f), pl.y - L (4.5f), L (1.2f), L (9.0f)));
    }
}

void Content::drawVuGlass (Graphics& g, const Path& win, Rectangle<float> wb)
{
    juce::Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (win);
    // ---- cristal: sombra interior del bisel, vineta, reflejos
    {
        const Pt top = P (0, vuFace.y - vuRy), mid = P (0, vuFace.y - vuRy + 24);
        g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.50f), 0, top.y, Colours::transparentBlack, 0, mid.y, false)); g.fillPath (win);
        ColourGradient vig (Colours::transparentBlack, wb.getCentreX(), wb.getCentreY() + L (6), Colours::black.withAlpha (0.30f), wb.getCentreX() + wb.getWidth() * 0.55f, wb.getCentreY(), true);
        vig.addColour (0.72, Colours::transparentBlack);
        g.setGradientFill (vig); g.fillPath (win);
        // reflejo ancho del cristal (curvo) y destello diagonal
        Path refl; refl.addEllipse (wb.getX() - wb.getWidth() * 0.10f, wb.getY() - wb.getHeight() * 0.55f, wb.getWidth() * 0.95f, wb.getHeight() * 0.95f);
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.20f), wb.getX() + wb.getWidth() * 0.2f, wb.getY(), Colours::white.withAlpha (0.0f),
                                           wb.getX() + wb.getWidth() * 0.35f, wb.getCentreY(), false));
        g.fillPath (refl);
        Path streak; const float sx = wb.getX() + wb.getWidth() * 0.66f;
        streak.addQuadrilateral (sx, wb.getY(), sx + L (10), wb.getY(), sx - L (18), wb.getBottom(), sx - L (28), wb.getBottom());
        g.setColour (Colours::white.withAlpha (0.055f)); g.fillPath (streak);
        // canto brillante del cristal arriba y linea oscura de contacto con el bisel
        Path rim; rim.addCentredArc (wb.getCentreX(), wb.getCentreY(), wb.getWidth() * 0.5f - 1.0f, wb.getHeight() * 0.5f - 1.0f, 0, -1.25f, 1.25f, true);
        g.setColour (Colours::white.withAlpha (0.35f)); g.strokePath (rim, juce::PathStrokeType (0.9f));
        g.setColour (Colours::black.withAlpha (0.45f)); g.strokePath (win, juce::PathStrokeType (1.4f));
    }
}

void Content::paintStrip (Graphics& g)
{
    g.drawImageAt (stripImg, 0, 0);
    const bool canUndo = proc.undoManager.canUndo(), canRedo = proc.undoManager.canRedo();
    // deshacer / rehacer
    stripButton (g, undoArea, hover == 0, false); iconArrow (g, undoArea, false, canUndo ? Colour (0xffeae7e2) : Colour (0xff5d6064));
    stripButton (g, redoArea, hover == 1, false); iconArrow (g, redoArea, true, canRedo ? Colour (0xffeae7e2) : Colour (0xff5d6064));
    // A / B y copia
    const int sl = proc.abSlot();
    for (int i = 0; i < 2; ++i)
    {
        const auto r = i == 0 ? aArea : bArea;
        stripButton (g, r, hover == 2 + i, sl == i, 4.0f);
        g.setFont (juce::Font (juce::FontOptions (sansName(), 14.0f, juce::Font::bold)));
        g.setColour (sl == i ? Colour (0xffffd9d9) : Colour (0xff8e9296));
        g.drawText (i == 0 ? "A" : "B", r, juce::Justification::centred);
        if (sl == i) { g.setColour (Colour (0x55ff3a40)); g.drawRoundedRectangle (r.expanded (1.5f), 5.0f, 1.5f); }
    }
    stripButton (g, copyArea, hover == 4, false, 4.0f);
    g.setFont (juce::Font (juce::FontOptions (sansName(), 11.0f, juce::Font::bold))); g.setColour (Colour (0xffcfcfcb));
    g.drawText (sl == 0 ? U ("A▸B") : U ("B▸A"), copyArea, juce::Justification::centred);
    g.setFont (juce::Font (juce::FontOptions (sansName(), 8.5f, juce::Font::bold)).withExtraKerningFactor (0.18f)); g.setColour (Colour (0xff7d8185));
    g.drawText ("UNDO   REDO", Rectangle<float> (undoArea.getX(), 0, redoArea.getRight() - undoArea.getX(), 11), juce::Justification::centred);
    g.drawText ("COMPARE", Rectangle<float> (aArea.getX(), 0, copyArea.getRight() - aArea.getX(), 11), juce::Justification::centred);
    // selector de presets: pantalla de cristal negro empotrada
    for (auto [r, t] : { std::pair<Rectangle<float>, int> { prevArea, -1 }, { nextArea, 1 } })
    {
        const bool hot = hover == (t < 0 ? 5 : 6);
        g.setColour (Colours::black.withAlpha (0.5f)); g.fillEllipse (r.translated (0, 1.5f));
        g.setGradientFill (ColourGradient (hot ? Colour (0xfff2f2f0) : Colour (0xffd4d6d8), r.getX(), r.getY(), Colour (0xff6b6e72), r.getRight(), r.getBottom(), false));
        g.fillEllipse (r);
        g.setColour (Colour (0xff1c1d1f)); g.drawEllipse (r.reduced (0.5f), 1.0f);
        Path tr; const auto c = r.getCentre(); const float sgn = (float) t;
        tr.addTriangle (c.x - 3.5f * sgn, c.y - 5.5f, c.x - 3.5f * sgn, c.y + 5.5f, c.x + 5.0f * sgn, c.y);
        g.setColour (Colour (0xff1a1b1d)); g.fillPath (tr);
    }
    {
        const auto r = presetArea;
        g.setColour (Colours::white.withAlpha (0.07f)); g.drawRoundedRectangle (r.expanded (1.0f).translated (0, 1.0f), 7, 1.0f);
        g.setGradientFill (ColourGradient (Colour (0xff020203), 0, r.getY(), Colour (0xff121315), 0, r.getBottom(), false)); g.fillRoundedRectangle (r, 6);
        g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.8f), 0, r.getY(), Colours::transparentBlack, 0, r.getY() + 9, false)); g.fillRoundedRectangle (r, 6);
        g.setColour (Colours::white.withAlpha (0.06f)); g.fillRoundedRectangle (r.withHeight (r.getHeight() * 0.45f).reduced (3, 2), 4);
        g.setColour (hover == 7 ? Colour (0xff8a2a30) : Colours::black); g.drawRoundedRectangle (r.reduced (0.5f), 6, 1.0f);
        g.setFont (juce::Font (juce::FontOptions (sansName(), 8.5f, juce::Font::bold)).withExtraKerningFactor (0.22f)); g.setColour (Colour (0xff9a6a5a));
        g.drawText ("PRESET " + juce::String (proc.getCurrentProgram() + 1).paddedLeft ('0', 2), r.withTrimmedLeft (12).withHeight (14).translated (0, 3), juce::Justification::centredLeft);
        g.setFont (juce::Font (juce::FontOptions (sansName(), 16.0f, juce::Font::bold)).withExtraKerningFactor (0.03f));
        g.setColour (Colour (0x55ff7a40)); g.drawText (proc.getProgramName (proc.getCurrentProgram()), r.translated (0, 4).expanded (0.5f), juce::Justification::centred);
        g.setColour (Colour (0xffffe6cc)); g.drawText (proc.getProgramName (proc.getCurrentProgram()), r.translated (0, 4), juce::Justification::centred);
        Path ch; const float cx = r.getRight() - 16, cy = r.getCentreY() + 1;
        ch.addTriangle (cx - 5, cy - 2.5f, cx + 5, cy - 2.5f, cx, cy + 3.5f);
        g.setColour (Colour (0xffb5876f)); g.fillPath (ch);
    }
    // MIX
    g.setFont (juce::Font (juce::FontOptions (sansName(), 11.0f, juce::Font::bold)).withExtraKerningFactor (0.14f)); g.setColour (Colour (0xffa9aaa6));
    g.drawText ("MIX", Rectangle<float> (1034, 0, 40, (float) strip), juce::Justification::centredRight);
    g.setColour (Colour (0xffeeeeea));
    g.drawText (juce::String ((int) std::round (param (proc, "mix"))) + " %", Rectangle<float> (1200, 0, 46, (float) strip), juce::Justification::centredLeft);
    // indicadores: sobremuestreo, carga DSP y senal
    {
        const float x0 = 1252;
        const Rectangle<float> os (x0, 10, 62, 16);
        g.setColour (Colour (0xff0a0b0c)); g.fillRoundedRectangle (os, 3);
        g.setColour (Colour (0xff3a2a14)); g.drawRoundedRectangle (os.reduced (0.5f), 3, 1.0f);
        g.setColour (Colour (0xffffb34a)); g.fillEllipse (os.getX() + 6, os.getCentreY() - 2.5f, 5, 5);
        g.setColour (Colour (0x44ffb34a)); g.fillEllipse (os.getX() + 4, os.getCentreY() - 4.5f, 9, 9);
        g.setFont (juce::Font (juce::FontOptions (sansName(), 10.0f, juce::Font::bold)).withExtraKerningFactor (0.08f)); g.setColour (Colour (0xffffd9a0));
        g.drawText ("OS " + juce::String (proc.oversamplingFactor()) + "x", os.withTrimmedLeft (14), juce::Justification::centredLeft);
        const Rectangle<float> dsp (x0, 30, 62, 14);
        g.setColour (Colour (0xff0a0b0c)); g.fillRoundedRectangle (dsp, 3);
        const float fill = juce::jlimit (0.0f, 1.0f, dspShown);
        g.setGradientFill (ColourGradient (Colour (0xff3fd16b), dsp.getX(), 0, Colour (0xffff4040), dsp.getRight(), 0, false));
        g.fillRoundedRectangle (dsp.reduced (2).withWidth (juce::jmax (2.0f, (dsp.getWidth() - 4) * fill)), 2);
        g.setFont (juce::Font (juce::FontOptions (sansName(), 9.0f, juce::Font::bold))); g.setColour (Colour (0xffdedede));
        g.drawText ("DSP " + juce::String (juce::roundToInt (fill * 100.0f)) + "%", dsp, juce::Justification::centred);
        const Pt led (x0 + 74, 27);
        const Colour sg = Colour (0xff2a3a2c).interpolatedWith (Colour (0xff5cff7a), sigShown);
        g.setColour (sg.withAlpha (0.35f * sigShown)); g.fillEllipse (led.x - 7, led.y - 7, 14, 14);
        g.setColour (sg); g.fillEllipse (led.x - 3.5f, led.y - 3.5f, 7, 7);
        g.setColour (Colours::white.withAlpha (0.4f)); g.fillEllipse (led.x - 2, led.y - 2.6f, 2.4f, 2.0f);
        g.setFont (juce::Font (juce::FontOptions (sansName(), 7.5f, juce::Font::bold))); g.setColour (Colour (0xff7d8185));
        g.drawText ("SIG", Rectangle<float> (led.x - 12, led.y + 5, 24, 9), juce::Justification::centred);
    }
}

void Content::paint (Graphics& g)
{
    paintStrip (g);
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (panel, Rectangle<float> (0, S, (float) W, (float) panelH));
    paintVu (g);
    // LED de encendido (domo rojo) con halo calido que sube y baja con la lampara
    {
        const Pt c = P (ledPos.x, ledPos.y); const float d = (float) ledOn.getWidth() * 0.5f;
        g.drawImage (ledOff, Rectangle<float> (c.x - d * 0.5f, c.y - d * 0.5f, d, d));
        if (glow > 0.01f)
        {
            const float gr = L (ledR) * 2.5f;
            ColourGradient halo (Colour (0x70ff3a46).withMultipliedAlpha (glow), c.x, c.y, Colour (0x00ff2030), c.x + gr, c.y, true);
            halo.addColour (0.45, Colour (0x28ff2a3a).withMultipliedAlpha (glow));
            g.setGradientFill (halo);
            g.fillEllipse (c.x - gr, c.y - gr, gr * 2, gr * 2);
            g.setOpacity (glow); g.drawImage (ledOn, Rectangle<float> (c.x - d * 0.5f, c.y - d * 0.5f, d, d)); g.setOpacity (1.0f);
        }
    }
}

void Content::paintOverChildren (Graphics& g)
{
    if (pillKnob == nullptr) return;
    const double age = juce::Time::getMillisecondCounterHiRes() - pillTime;
    if (age > 1800.0) return;
    const float alpha = age < 1400.0 ? 1.0f : (float) (1.0 - (age - 1400.0) / 400.0);
    auto* prm = proc.apvts.getParameter (pillKnob->pid);
    if (prm == nullptr) return;
    const juce::String t = prm->getName (40).toUpperCase() + "   " + prm->getCurrentValueAsText();
    const auto f = juce::Font (juce::FontOptions (13.0f, juce::Font::bold));
    const float w = juce::GlyphArrangement::getStringWidth (f, t) + 26.0f, h = 24.0f;
    const auto kb = pillKnob->getBounds().toFloat();
    float x = kb.getCentreX() - w * 0.5f, y = kb.getBottom() - 6.0f;
    if (y + h > (float) H - 2) y = kb.getY() - h + 6.0f;
    x = juce::jlimit (4.0f, (float) W - w - 4.0f, x);
    const Rectangle<float> r (x, y, w, h);
    g.setColour (Colours::black.withAlpha (0.35f * alpha)); g.fillRoundedRectangle (r.translated (0, 2), 12);
    g.setColour (Colour (0xe0141516).withMultipliedAlpha (alpha)); g.fillRoundedRectangle (r, 12);
    g.setColour (Colour (0x90ff4040).withMultipliedAlpha (alpha)); g.drawRoundedRectangle (r.reduced (0.5f), 12, 1.0f);
    g.setFont (f); g.setColour (Colour (0xfff0eeea).withMultipliedAlpha (alpha));
    g.drawText (t, r, juce::Justification::centred);
}

void Content::mouseMove (const juce::MouseEvent& e)
{
    int h = -1;
    const Rectangle<float> areas[] { undoArea, redoArea, aArea, bArea, copyArea, prevArea, nextArea, presetArea };
    for (int i = 0; i < 8; ++i) if (areas[i].contains (e.position)) h = i;
    if (h != hover) { hover = h; repaint (0, 0, W, strip); }
}
void Content::mouseExit (const juce::MouseEvent&) { if (hover != -1) { hover = -1; repaint (0, 0, W, strip); } }

void Content::mouseUp (const juce::MouseEvent& e)
{
    const int n = proc.getNumPrograms();
    if (undoArea.contains (e.position)) { proc.undoManager.undo(); repaint (0, 0, W, strip); return; }
    if (redoArea.contains (e.position)) { proc.undoManager.redo(); repaint (0, 0, W, strip); return; }
    if (aArea.contains (e.position)) { proc.abSelect (0); repaint (0, 0, W, strip); return; }
    if (bArea.contains (e.position)) { proc.abSelect (1); repaint (0, 0, W, strip); return; }
    if (copyArea.contains (e.position)) { proc.abCopyToOther(); repaint (0, 0, W, strip); return; }
    if (prevArea.contains (e.position)) { proc.setCurrentProgram ((proc.getCurrentProgram() + n - 1) % n); repaint (0, 0, W, strip); return; }
    if (nextArea.contains (e.position)) { proc.setCurrentProgram ((proc.getCurrentProgram() + 1) % n); repaint (0, 0, W, strip); return; }
    if (! presetArea.contains (e.position)) return;
    juce::PopupMenu m; m.addSectionHeader ("PRESETS");
    for (int i = 0; i < n; ++i) m.addItem (i + 1, proc.getProgramName (i), true, i == proc.getCurrentProgram());
    juce::Component::SafePointer<Content> sp (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (presetArea.toNearestInt())), [sp] (int r)
    { if (sp != nullptr && r > 0) { sp->proc.setCurrentProgram (r - 1); sp->repaint(); } });
}

void Content::tick (float dt)
{
    dt = juce::jlimit (0.001f, 0.05f, dt);
    const bool on = powerOn (proc);
    // perillas y botones animados
    for (auto* k : { &preGain, &input, &hpf, &thr, &ratio, &attack, &release, &bass, &bassF, &lm, &lmF, &hm, &hmF, &treble, &trebleF, &out })
        if (k->animate (dt)) k->repaint();
    for (auto* b : { &bHigh, &bPhase, &bP48, &bFilter, &bPre, &bMeter, &bComp, &bLmQ, &bLmX, &bHmQ, &bHmX, &bSc, &bEq })
        if (b->animate (dt)) b->repaint();
    // VU: mecanismo de 2do orden (300 ms al 99 %, 1.5 % de sobrepaso) sobre la tension lineal, con topes
    float target = 0.0f;
    if (on)
    {
        const float db = param (proc, "meter_gr") > 0.5f ? proc.grDb.load() : proc.vuDb.load() + 18.0f;
        target = juce::jlimit (0.0f, 1.6f, std::pow (10.0f, juce::jmax (-60.0f, db) / 20.0f));
    }
    const float prevN = needle, prevG = glow;
    {
        constexpr float wn = 13.0f, zeta = 0.80f; const int sub = 8; const float h = dt / (float) sub;
        for (int i = 0; i < sub; ++i)
        {
            const float a = wn * wn * (target - needle) - 2.0f * zeta * wn * needleVel;
            needleVel += a * h; needle += needleVel * h;
            if (needle < 0.0f) { needle = 0.0f; needleVel = -needleVel * 0.25f; }
            if (needle > 1.55f) { needle = 1.55f; needleVel = -needleVel * 0.25f; }
        }
    }
    glow += ((on ? 1.0f : 0.0f) - glow) * juce::jmin (1.0f, dt * (on ? 4.0f : 6.0f));      // la lampara del VU calienta y se enfria
    if (std::abs (prevN - needle) > 1e-5f || std::abs (prevG - glow) > 1e-4f)
    {
        const Pt a = P (vuBezel.x - vuBx - 4, vuBezel.y - vuBy - 4), b = P (vuBezel.x + vuBx + 4, vuBezel.y + vuBy + 4);
        repaint (Rectangle<float> (a, b).toNearestInt());
        if (std::abs (prevG - glow) > 1e-4f) { const Pt c = P (ledPos.x, ledPos.y); repaint (Rectangle<float> (c.x - 30, c.y - 30, 60, 60).toNearestInt()); }
    }
    if (on != lastPower) { lastPower = on; rocker.repaint(); }
    // barra superior: carga DSP, senal, presets, A/B, deshacer
    const float dspT = juce::jlimit (0.0f, 1.0f, proc.dspLoad.load()), sigT = proc.inLevelDb.load() > -60.0f ? 1.0f : 0.0f;
    const float pd = dspShown, ps = sigShown;
    dspShown += (dspT - dspShown) * juce::jmin (1.0f, dt * 3.0f);
    sigShown += (sigT - sigShown) * juce::jmin (1.0f, dt * (sigT > sigShown ? 25.0f : 4.0f));
    const int prog = proc.getCurrentProgram(), sl = proc.abSlot();
    const bool cu = proc.undoManager.canUndo(), cr = proc.undoManager.canRedo();
    if (prog != lastProgram || sl != lastSlot || cu != lastUndo || cr != lastRedo || std::abs (pd - dspShown) > 0.002f || std::abs (ps - sigShown) > 0.01f)
    {
        lastProgram = prog; lastSlot = sl; lastUndo = cu; lastRedo = cr;
        repaint (0, 0, W, strip);
    }
    if (pillKnob != nullptr)
    {
        const double age = juce::Time::getMillisecondCounterHiRes() - pillTime;
        if (age < 2000.0) repaint (pillKnob->getBounds().expanded (220, 40));
        else pillKnob = nullptr;
    }
}

//==============================================================================
TopStrip3Editor::TopStrip3Editor (TopStrip3Processor& p) : AudioProcessorEditor (p), content (p)
{
    setLookAndFeel (&look);
    addAndMakeVisible (content);
    content.setBounds (0, 0, fullW, fullH);
    setResizable (true, true);
    setResizeLimits (1000, (int) (1000.0 * fullH / fullW), 2400, (int) (2400.0 * fullH / fullW));
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) fullW / (double) fullH);
    setSize (1500, (int) (1500.0 * fullH / fullW));
    lastTick = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (60);
}
TopStrip3Editor::~TopStrip3Editor() { setLookAndFeel (nullptr); }
void TopStrip3Editor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float dt = (float) ((now - lastTick) * 0.001);
    lastTick = now;
    content.tick (dt);
}
void TopStrip3Editor::resized() { content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) fullW)); }
