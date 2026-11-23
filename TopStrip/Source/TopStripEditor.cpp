#include "TopStripEditor.h"
#include <map>

using namespace s7ui;
using juce::Colour; namespace Colours = juce::Colours; using juce::Graphics; using juce::Path; using juce::Rectangle; using juce::ColourGradient;
using Pt = juce::Point<float>;

namespace
{
    juce::String U (const char* s) { return juce::String::fromUTF8 (s); }
    constexpr float pi = juce::MathConstants<float>::pi, deg = pi / 180.0f;
    float param (TopStripProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); }
    bool powerOn (TopStripProcessor& p) { return param (p, "power") > 0.5f; }
    void setParam (TopStripProcessor& p, const char* id, float v)
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
    const KP kPre { 233.3f, 213.5f, 28.0f }, kInput { 321.5f, 216.7f, 26.0f }, kHpf { 321.7f, 337.0f, 21.0f },
             kThr { 495.0f, 218.5f, 28.0f }, kRatio { 582.7f, 218.5f, 28.0f }, kAtt { 495.8f, 338.3f, 21.5f }, kRel { 580.7f, 338.3f, 21.5f },
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
    const Pt ledPos { 758.3f, 350.0f }; constexpr float ledR = 6.2f;

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
        const float t0 = 29.0f * f, t1 = 54.5f * f;
        // las rayas laterales no cruzan la mitad del espacio hasta la perilla vecina (como en el panel)
        auto tEnd = [t1] (float a) { const float s = std::abs (std::sin (a * deg)); return s > 0.5f ? std::min (t1, 39.0f / s) : t1; };
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
        textAt (g, "- dB +", kThr.x, 248.3f, num, 0.95f); textAt (g, "RATIO", kRatio.x, 248.3f, sans (6.6f), 0.95f);
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
        for (const KP* k : { &kBass, &kLm, &kHm, &kTr, &kOut }) textAt (g, "- dB +", k->x, 248.3f, num, 0.95f);
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
        textAt (g, "TOP STRIP", 758.5f, 307.8f, serif (14.5f), 0.98f, 0.07f);
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
        const auto silkA = maskOf (silk, iw, ih, 0), whiteA = maskOf (white, iw, ih, 0), plateA = maskOf (plate, iw, ih, 0), plateB = maskOf (plate, iw, ih, 3),
                   bezA = maskOf (bezel, iw, ih, 0), bezB = maskOf (bezel, iw, ih, 4), faceA = maskOf (face, iw, ih, 1), holeA = maskOf (holes, iw, ih, 0),
                   edgeA = maskOf (edge, iw, ih, 0), edgeB = maskOf (edge, iw, ih, 4);
        struct Scr { float x, y, r, rot; };
        const std::vector<Scr> screws { { 174.3f, 183.0f, 13.5f, 0.2f }, { 174.3f, 344.0f, 13.5f, 0.9f }, { 1337.0f, 185.0f, 13.5f, 0.5f }, { 1336.0f, 344.3f, 13.5f, 0.1f },
                                        { 661.0f, 168.3f, 9.5f, 0.7f }, { 857.7f, 168.3f, 9.5f, 0.3f }, { 661.0f, 364.3f, 9.5f, 1.1f }, { 857.7f, 364.3f, 9.5f, 0.4f } };
        auto alu = r3d::srgb (0.80f, 0.80f, 0.795f, 0, 0.50f); alu.f0 = 0.05f;
        auto ink = r3d::srgb (0.012f, 0.012f, 0.014f, 0, 0.25f); ink.f0 = 0.015f;
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
            const float hair = r3d::vnoise (px * 0.025f, py * 3.6f, 9);
            float tone = 0.905f + 0.06f * streak + 0.045f * fine + 0.06f * hair;
            float h = 0.30f * streak + 0.12f * fine;
            // placa central (mas clara y levemente elevada, con canto)
            if (plateA[i] > 0) tone *= 1.0f + 0.065f * plateA[i];
            h += 3.0f * plateB[i];
            // bisel del VU: aro mate que baja hacia la cara
            if (bezA[i] > 0) { s.m = s7p::mixM (s.m, bez, bezA[i]); tone = 0.97f + 0.03f * fine; }
            h -= 4.0f * bezB[i];
            if (faceA[i] > 0) { s.m = s7p::mixM (s.m, r3d::srgb (0.80f, 0.74f, 0.64f, 0, 0.4f), faceA[i]); h -= 3.0f * faceA[i]; }
            s.m.r *= tone; s.m.g *= tone; s.m.b *= tone;
            // serigrafia
            if (silkA[i] > 0) { s.m = s7p::mixM (s.m, ink, silkA[i] * (0.93f + 0.07f * r3d::fbm (px * 0.5f, py * 0.5f, 5, 2))); h += 0.45f * silkA[i]; }
            if (whiteA[i] > 0) { s.m = s7p::mixM (s.m, paint, whiteA[i]); h += 0.3f * whiteA[i]; }
            // canto de la chapa y ranuras de las orejas
            h -= 3.0f * (1.0f - edgeB[i]);
            s.m.alpha = edgeA[i] * (1.0f - holeA[i]);
            // tornillos, jack y LED (piezas 3D)
            const float X = px / pk + X0, Y = py / pk + Y0;
            for (auto& q : screws)
            {
                if (std::abs (X - q.x) > q.r * 1.9f || std::abs (Y - q.y) > q.r * 1.9f) continue;
                const float dx = (X - q.x) * pk, dy = (Y - q.y) * pk, R = q.r * pk, rr = std::sqrt (dx * dx + dy * dy);
                if (rr < R + 1) { const auto p = s7p::screw (dx, dy, R, q.rot); s.m = s7p::mixM (s.m, p.m, p.m.alpha); s.m.alpha = 1; h = h * (1 - p.m.alpha) + (h + 1.0f + p.h) * p.m.alpha; }
                else { const float ao = 1.0f - 0.40f * std::exp (-std::pow ((rr - R) / (0.30f * R), 2.0f)); s.m.r *= ao; s.m.g *= ao; s.m.b *= ao; }
            }
            if (std::abs (X - 236.7f) < 40 && std::abs (Y - 333.3f) < 40)
            {
                const float dx = (X - 236.7f) * pk, dy = (Y - 333.3f) * pk, R = 36.7f * pk, rr = std::sqrt (dx * dx + dy * dy);
                if (rr < R + 1) { const auto p = s7p::jack (dx, dy, R); s.m = s7p::mixM (s.m, p.m, p.m.alpha); h = h * (1 - p.m.alpha) + (h + p.h) * p.m.alpha; }
            }
            if (std::abs (X - ledPos.x) < 10 && std::abs (Y - ledPos.y) < 10)
            {
                const float dx = (X - ledPos.x) * pk, dy = (Y - ledPos.y) * pk, R = ledR * pk, rr = std::sqrt (dx * dx + dy * dy);
                if (rr < R + 1) { const auto p = s7p::led (dx, dy, R, false); s.m = s7p::mixM (s.m, p.m, p.m.alpha); h = h * (1 - p.m.alpha) + (h + p.h) * p.m.alpha; }
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
Look::Look()
{
    setColour (juce::PopupMenu::backgroundColourId, Colour (0xff1b1d20));
    setColour (juce::PopupMenu::textColourId, Colour (0xffe8ecef));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colour (0xff7a2a2e));
    setColour (juce::PopupMenu::headerTextColourId, Colour (0xffb9a9a9));
    setColour (juce::TooltipWindow::backgroundColourId, Colour (0xff1b1d20));
    setColour (juce::TooltipWindow::textColourId, Colour (0xffe8ecef));
}
void Look::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    auto* k = dynamic_cast<Knob*> (&s);
    if (k == nullptr) return;
    float ang = a0 + pos * (a1 - a0);
    if (! k->steps.empty())
    {
        const int idx = juce::jlimit (0, (int) k->steps.size() - 1, (int) std::round (s.getValue() - s.getMinimum()));
        ang = k->steps[(size_t) idx] * deg;
    }
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (k->image (ang, w, h), Rectangle<float> ((float) x, (float) y, (float) w, (float) h));
}

Knob::Knob (APVTS& s, const juce::String& id, float a0, float a1, std::vector<float> st) : steps (std::move (st)), pid (id)
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (a0 * deg, a1 * deg, true);
    setMouseDragSensitivity (steps.empty() ? 260 : 120);
    if (auto* p = s.getParameter (id)) setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    att = std::make_unique<APVTS::SliderAttachment> (s, id, *this);
}
const juce::Image& Knob::image (float ang, int w, int h)
{
    if (std::abs (ang - imgAng) < 1e-4f && imgW == w && img.isValid()) return img;
    const int iw = w * 2, ih = h * 2;
    const float R = (float) std::min (iw, ih) * 0.5f / kBox, shift = faceShift * 2.0f;
    const auto st = style;
    const float shiftY = faceShiftY * 2.0f;
    img = r3d::render (iw, ih, [R, ang, shift, shiftY, st] (float px, float py) { return s7p::knob (px, py, R, ang, shift, shiftY, st); },
                       { 0.07f * R, 0.10f * R, 0.20f * R, 0.50f, 0.08f * R, 0.55f });
    imgAng = ang; imgW = w;
    return img;
}

bool LampBtn::lit() const { return param (proc, id) > 0.5f && powerOn (proc); }
void LampBtn::paint (Graphics& g)
{
    const int on = lit() ? 1 : 0;
    auto& im = cache[on];
    if (! im.isValid())
    {
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
        im = r3d::render (w, h, [hw, hh, on, &textFn] (float x, float y) { return s7p::lampButton (x, y, hw, hh, on == 1, textFn); },
                          { 0.05f * hh, 0.22f * hh, 0.25f * hh, 0.45f, 0.10f * hh, 0.5f });
    }
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (im, getLocalBounds().toFloat());
    if (on)
    {
        // resplandor rojo sobre el aluminio alrededor del boton
        const auto c = getLocalBounds().toFloat().getCentre();
        g.setGradientFill (ColourGradient (Colour (0x30ff2020), c.x, c.y, Colour (0x00ff2020), c.x + L (capHW) * 1.6f, c.y, true));
        g.fillEllipse (c.x - L (capHW) * 1.6f, c.y - L (capHH) * 2.2f, L (capHW) * 3.2f, L (capHH) * 4.4f);
    }
    shown = on;
}
void LampBtn::mouseUp (const juce::MouseEvent& e)
{
    if (! getLocalBounds().contains (e.getPosition())) return;
    setParam (proc, id, param (proc, id) > 0.5f ? 0.0f : 1.0f);
    repaint();
}
void LampBtn::refresh() { if ((lit() ? 1 : 0) != shown) repaint(); }

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
            return s7p::rocker (x, y, hw, hh, on == 1, m);
        }, { 0.0f, 0.04f * hh, 0.08f * hh, 0.4f, 0.04f * hh, 0.4f });
    }
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (im, getLocalBounds().toFloat());
}
void Rocker::mouseUp (const juce::MouseEvent&) { setParam (proc, "power", powerOn (proc) ? 0.0f : 1.0f); repaint(); }

//==============================================================================
Content::Content (TopStripProcessor& p)
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
    struct KS { Knob* k; const KP* kp; };
    for (auto [k, kp] : { KS { &preGain, &kPre }, { &input, &kInput }, { &hpf, &kHpf }, { &thr, &kThr }, { &ratio, &kRatio }, { &attack, &kAtt }, { &release, &kRel },
                          { &bass, &kBass }, { &bassF, &kBassF }, { &lm, &kLm }, { &lmF, &kLmF }, { &hm, &kHm }, { &hmF, &kHmF }, { &treble, &kTr }, { &trebleF, &kTrF },
                          { &out, &kOut } })
    {
        k->radius = L (kp->r);
        // perspectiva de la foto: la cara se corre hacia afuera del centro del panel y deja ver el lado moleteado
        const float shPhoto = kp->x < 495.0f ? -7.0f * juce::jmin (1.0f, (495.0f - kp->x) / 262.0f) : juce::jmin (7.0f, 0.011f * (kp->x - 495.0f));
        k->faceShift = L (shPhoto) * kp->r / 27.0f;
        k->faceShiftY = L (0.055f * (kp->y - 272.0f)) * kp->r / 27.0f;
        if (kp->r < 22.0f) { k->style.faceFrac = 0.74f; k->style.bevel = 0.18f; k->style.knurl = 70; k->style.ptrIn = 0.22f; k->style.ptrOut = 0.72f; k->style.ptrW = 0.15f; }
        addAndMakeVisible (k);
        k->addListener (this);
    }
    for (auto* b : { &bHigh, &bPhase, &bP48, &bFilter, &bPre, &bMeter, &bComp, &bLmQ, &bLmX, &bHmQ, &bHmX, &bSc, &bEq }) addAndMakeVisible (b);
    addAndMakeVisible (rocker);

    preGain.setTooltip (U ("PREAMP GAIN: cuánto se empuja la válvula (escala LINE: -28 a +9 dB). Más ganancia = más color"));
    input.setTooltip (U ("INPUT: LINE (limpio), INSTRUMENT (Hi-Z, más armónicos pares) o MIC (con transformador: graves con más cuerpo)"));
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
        ledOn = r3d::render (d * 2, d * 2, [R] (float x, float y) { return s7p::led (x, y, R, true); });
        ledOff = r3d::render (d * 2, d * 2, [R] (float x, float y) { return s7p::led (x, y, R, false); });
    }
    // barra superior
    stripImg = juce::Image (juce::Image::ARGB, W, strip, true);
    Graphics g (stripImg);
    g.setGradientFill (ColourGradient (Colour (0xff1e2022), 0, 0, Colour (0xff111213), 0, S, false)); g.fillAll();
    g.setColour (Colours::black); g.drawHorizontalLine (strip - 1, 0, (float) W);
    g.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)).withExtraKerningFactor (0.08f)); g.setColour (Colour (0xffd9d9d6));
    g.drawText ("Dj Pava Hm Music", Rectangle<float> (20, 0, 300, S), juce::Justification::centredLeft);
    g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)).withExtraKerningFactor (0.12f)); g.setColour (Colour (0xffa9a9a4));
    g.drawText ("TOP STRIP 737", Rectangle<float> ((float) W - 320, 0, 300, S), juce::Justification::centredRight);
}
Content::~Content() { for (auto* k : { &preGain, &input, &hpf, &thr, &ratio, &attack, &release, &bass, &bassF, &lm, &lmF, &hm, &hmF, &treble, &trebleF, &out }) k->removeListener (this); }

void Content::sliderValueChanged (juce::Slider* s) { pillKnob = dynamic_cast<Knob*> (s); pillTime = juce::Time::getMillisecondCounterHiRes(); repaint(); }
void Content::sliderDragStarted (juce::Slider* s) { sliderValueChanged (s); }

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
        const Pt p = P (q.x, q.y); const float w = L (capHW) * 2.0f + 10.0f, h = L (capHH) * 2.0f + 10.0f;
        b.setBounds (juce::roundToInt (p.x - w * 0.5f), juce::roundToInt (p.y - h * 0.5f), juce::roundToInt (w), juce::roundToInt (h));
    };
    btn (bHigh, bpHigh); btn (bPhase, bpPhase); btn (bP48, bpP48); btn (bFilter, bpFilter); btn (bPre, bpPre); btn (bMeter, bpMeter); btn (bComp, bpComp);
    btn (bLmQ, bpLmQ); btn (bLmX, bpLmX); btn (bHmQ, bpHmQ); btn (bHmX, bpHmX); btn (bSc, bpSc); btn (bEq, bpEq);
    { const Pt a = P (1240.0f, 306.7f), b = P (1310.0f, 359.0f); rocker.setBounds (Rectangle<float> (a, b).toNearestInt()); }
}

void Content::paintVu (Graphics& g)
{
    const auto xf = photoToContent();
    Path win; win.addEllipse (vuFace.x - vuRx, vuFace.y - vuRy, vuRx * 2, vuRy * 2); win.applyTransform (xf);
    juce::Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (win);
    const bool on = powerOn (proc);
    // cara crema (un poco mas oscura en los bordes)
    const Pt cc = P (vuFace.x, vuFace.y - 12.0f);
    g.setGradientFill (ColourGradient (on ? Colour (0xffe4d8c2) : Colour (0xffcdbfa8), cc.x, cc.y, on ? Colour (0xffb5a385) : Colour (0xff9e8e74), cc.x + L (110), cc.y, true));
    g.fillPath (win);
    const Pt pv = P (vuPivot.x, vuPivot.y);
    const Colour inkC (0xff3a3027);
    g.setColour (inkC);
    // arco base y rayas
    {
        Path arc; arc.addCentredArc (pv.x, pv.y, L (vuR), L (vuR), 0, vuAng[0] * deg, vuAng[12] * deg, true);
        g.strokePath (arc, juce::PathStrokeType (L (1.05f)));
        for (int i = 0; i < 13; ++i)
        {
            const bool major = ! (vuDb[i] == -6 || vuDb[i] == -4);
            g.drawLine ({ polar (pv, L (vuR), vuAng[i]), polar (pv, L (vuR + (major ? 7.0f : 4.8f)), vuAng[i]) }, L (major ? 1.15f : 0.9f));
        }
        // extremos inclinados hacia el numero (como en el original)
        g.drawLine ({ polar (pv, L (vuR), vuAng[0]), polar (pv, L (vuR + 9.0f), vuAng[0] - 3.5f) }, L (1.05f));
        g.drawLine ({ polar (pv, L (vuR), vuAng[12]), polar (pv, L (vuR + 9.0f), vuAng[12] + 3.5f) }, L (1.05f));
        const auto nf = serif (L (6.2f));
        const char* lab[] { "20", "10", "7", nullptr, "5", nullptr, "3", "2", "1", "0", "1", "2", "3" };
        for (int i = 1; i < 12; ++i)
        {
            if (lab[i] == nullptr) continue;
            const auto q = polar (pv, L (vuR + 15.5f), vuAng[i]);
            textAt (g, lab[i], q.x, q.y, nf);
        }
        { const Pt a = P (670.0f, 214.5f), b = P (847.0f, 214.5f); textAt (g, "20", a.x, a.y, nf); textAt (g, "3", b.x, b.y, nf); }
        const Pt vu = P (vuFace.x, 235.0f), dc = P (vuFace.x, 248.3f);
        textAt (g, "VU", vu.x, vu.y, serif (L (8.6f)), 1.05f, 0.08f);
        textAt (g, "decibels", dc.x, dc.y, juce::Font (juce::FontOptions (serifName(), L (8.4f), juce::Font::plain)), 1.0f, 0.22f);
        const Pt mi = P (695.0f, 234.3f), pl = P (821.7f, 234.3f);
        g.fillRect (Rectangle<float> (mi.x - L (4.5f), mi.y - L (0.6f), L (9.0f), L (1.2f)));
        g.fillRect (Rectangle<float> (pl.x - L (4.5f), pl.y - L (0.6f), L (9.0f), L (1.2f)));
        g.fillRect (Rectangle<float> (pl.x - L (0.6f), pl.y - L (4.5f), L (1.2f), L (9.0f)));
    }
    // aguja (pivote oculto debajo de la cara)
    {
        const float a = vuAngle (vuShown);
        g.setColour (Colours::black.withAlpha (0.18f));
        g.drawLine ({ polar (pv, L (92), a).translated (L (2.2f), L (2.0f)), polar (pv, L (vuR + 11), a).translated (L (2.2f), L (2.0f)) }, L (1.4f));
        g.setColour (Colour (0xff15110e));
        g.drawLine ({ polar (pv, L (92), a), polar (pv, L (vuR + 11), a) }, L (0.95f));
    }
    // sombra interior del bisel y reflejo del vidrio
    const Pt top = P (0, vuFace.y - vuRy), mid = P (0, vuFace.y - vuRy + 26);
    g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.32f), 0, top.y, Colours::transparentBlack, 0, mid.y, false)); g.fillPath (win);
    const Pt r0 = P (vuFace.x - 70, vuFace.y - 40), r1 = P (vuFace.x - 10, vuFace.y + 10);
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.16f), r0.x, r0.y, Colours::transparentWhite, r1.x, r1.y, false)); g.fillPath (win);
    if (! on) { g.setColour (Colours::black.withAlpha (0.10f)); g.fillPath (win); }
}

void Content::paint (Graphics& g)
{
    g.drawImageAt (stripImg, 0, 0);
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (panel, Rectangle<float> (0, S, (float) W, (float) panelH));
    paintVu (g);
    // LED de encendido
    {
        const Pt c = P (ledPos.x, ledPos.y); const float d = (float) ledOn.getWidth() * 0.5f;
        const bool on = powerOn (proc);
        if (on)
        {
            const float gr = L (ledR) * 3.2f;
            g.setGradientFill (ColourGradient (Colour (0x70ff2a2a), c.x, c.y, Colour (0x00ff2a2a), c.x + gr, c.y, true));
            g.fillEllipse (c.x - gr, c.y - gr, gr * 2, gr * 2);
        }
        g.drawImage (on ? ledOn : ledOff, Rectangle<float> (c.x - d * 0.5f, c.y - d * 0.5f, d, d));
    }
    // barra superior: presets
    for (auto [r, t] : { std::pair<Rectangle<float>, const char*> { prevArea, "<" }, { nextArea, ">" } })
    {
        g.setGradientFill (ColourGradient (Colour (0xff303235), 0, r.getY(), Colour (0xff1c1d1f), 0, r.getBottom(), false)); g.fillRoundedRectangle (r, 4);
        g.setColour (Colours::black); g.drawRoundedRectangle (r, 4, 1.0f);
        Path tr; const auto c = r.getCentre(); const float sgn = t[0] == '<' ? -1.0f : 1.0f;
        tr.addTriangle (c.x - 4 * sgn, c.y - 6, c.x - 4 * sgn, c.y + 6, c.x + 5 * sgn, c.y);
        g.setColour (Colour (0xffd9d9d6)); g.fillPath (tr);
    }
    g.setGradientFill (ColourGradient (Colour (0xff0a0b0b), 0, presetArea.getY(), Colour (0xff18191a), 0, presetArea.getBottom(), false)); g.fillRoundedRectangle (presetArea, 4);
    g.setColour (Colours::black); g.drawRoundedRectangle (presetArea, 4, 1.0f);
    g.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold))); g.setColour (Colour (0xffeeeeea));
    g.drawText (proc.getProgramName (proc.getCurrentProgram()), presetArea, juce::Justification::centred);
}

void Content::paintOverChildren (Graphics& g)
{
    // valor exacto mientras se mueve una perilla
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
    g.setColour (Colour (0xe0141516).withMultipliedAlpha (alpha)); g.fillRoundedRectangle (r, 12);
    g.setColour (Colour (0x90ff4040).withMultipliedAlpha (alpha)); g.drawRoundedRectangle (r.reduced (0.5f), 12, 1.0f);
    g.setFont (f); g.setColour (Colour (0xfff0eeea).withMultipliedAlpha (alpha));
    g.drawText (t, r, juce::Justification::centred);
}

void Content::mouseUp (const juce::MouseEvent& e)
{
    const int n = proc.getNumPrograms();
    if (prevArea.contains (e.position)) { proc.setCurrentProgram ((proc.getCurrentProgram() + n - 1) % n); repaint(); return; }
    if (nextArea.contains (e.position)) { proc.setCurrentProgram ((proc.getCurrentProgram() + 1) % n); repaint(); return; }
    if (! presetArea.contains (e.position)) return;
    juce::PopupMenu m; m.addSectionHeader ("PRESETS");
    for (int i = 0; i < n; ++i) m.addItem (i + 1, proc.getProgramName (i), true, i == proc.getCurrentProgram());
    juce::Component::SafePointer<Content> sp (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (presetArea.toNearestInt())), [sp] (int r)
    { if (sp != nullptr && r > 0) { sp->proc.setCurrentProgram (r - 1); sp->repaint(); } });
}

void Content::tick()
{
    // VU: 0 VU = +4 dBu = -18 dBFS; con METER la aguja muestra la compresion (baja desde 0 VU)
    const bool on = powerOn (proc);
    float target = param (proc, "meter_gr") > 0.5f ? proc.grDb.load() : proc.vuDb.load() + 18.0f;
    if (! on) target = -30.0f;
    target = juce::jlimit (-30.0f, 4.0f, target);
    const float prev = vuShown;
    vuShown += (target - vuShown) * 0.22f;        // balistica VU (~300 ms)
    if (std::abs (prev - vuShown) > 0.01f)
    {
        const Pt a = P (vuFace.x - vuRx, vuFace.y - vuRy), b = P (vuFace.x + vuRx, vuFace.y + vuRy);
        repaint (Rectangle<float> (a, b).expanded (2).toNearestInt());
    }
    if (on != lastPower)
    {
        lastPower = on; rocker.repaint();
        const Pt c = P (ledPos.x, ledPos.y); repaint (Rectangle<float> (c.x - 30, c.y - 30, 60, 60).toNearestInt());
        const Pt a = P (vuFace.x - vuRx, vuFace.y - vuRy), b = P (vuFace.x + vuRx, vuFace.y + vuRy);
        repaint (Rectangle<float> (a, b).expanded (2).toNearestInt());
    }
    for (auto* b : { &bHigh, &bPhase, &bP48, &bFilter, &bPre, &bMeter, &bComp, &bLmQ, &bLmX, &bHmQ, &bHmX, &bSc, &bEq }) b->refresh();
    const int prog = proc.getCurrentProgram();
    if (prog != lastProgram) { lastProgram = prog; repaint (0, 0, W, strip); }
    if (pillKnob != nullptr)
    {
        const double age = juce::Time::getMillisecondCounterHiRes() - pillTime;
        if (age < 2000.0) repaint (pillKnob->getBounds().expanded (220, 40));
        else pillKnob = nullptr;
    }
}

//==============================================================================
TopStripEditor::TopStripEditor (TopStripProcessor& p) : AudioProcessorEditor (p), content (p)
{
    setLookAndFeel (&look);
    addAndMakeVisible (content);
    content.setBounds (0, 0, fullW, fullH);
    setResizable (true, true);
    setResizeLimits (1000, (int) (1000.0 * fullH / fullW), 2400, (int) (2400.0 * fullH / fullW));
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) fullW / (double) fullH);
    setSize (1500, (int) (1500.0 * fullH / fullW));
    startTimerHz (30);
}
TopStripEditor::~TopStripEditor() { setLookAndFeel (nullptr); }
void TopStripEditor::resized() { content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) fullW)); }
