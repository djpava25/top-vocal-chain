#include "Top76Editor.h"
#include "T3AParts.h"
#include <map>

using namespace t76;
using juce::Colour; namespace Colours = juce::Colours; using juce::Graphics; using juce::Path; using juce::Rectangle; using juce::ColourGradient;
using Pt = juce::Point<float>;

namespace
{
    juce::String U (const char* s) { return juce::String::fromUTF8 (s); }
    constexpr float pi = juce::MathConstants<float>::pi, deg = pi / 180.0f;
    float param (Top76Processor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); }
    void setParam (Top76Processor& p, const char* id, float v)
    { if (auto* q = p.apvts.getParameter (id)) { q->beginChangeGesture(); q->setValueNotifyingHost (q->convertTo0to1 (v)); q->endChangeGesture(); } }
    Pt polar (Pt c, float r, float a) { return { c.x + r * std::sin (a), c.y - r * std::cos (a) }; }

    // ------------------------------------------------------------------ tipografia (calibrada por la altura real de las mayusculas)
    juce::String pickFont (std::initializer_list<const char*> names, const juce::String& fallback)
    {
        static const juce::StringArray all = juce::Font::findAllTypefaceNames();
        for (auto* n : names) if (all.contains (n, true)) return n;
        return fallback;
    }
    const juce::String& sansName() { static const juce::String n = pickFont ({ "Helvetica", "Helvetica Neue", "Arial", "Liberation Sans" }, juce::Font::getDefaultSansSerifFontName()); return n; }
    const juce::String& wideName() { static const juce::String n = pickFont ({ "Eurostile", "Microgramma", "Arial Black", "Helvetica Neue", "Arial", "Liberation Sans" }, sansName()); return n; }
    juce::Font capFont (const juce::String& name, int style, float cap)
    {
        static std::map<juce::String, float> ratio;
        const juce::String key = name + juce::String (style);
        auto it = ratio.find (key);
        float r;
        if (it == ratio.end())
        {
            juce::GlyphArrangement ga; ga.addLineOfText (juce::Font (juce::FontOptions (name, 100.0f, style)), "H", 0, 0);
            Path p; ga.createPath (p);
            r = juce::jmax (0.3f, p.getBounds().getHeight() / 100.0f); ratio[key] = r;
        }
        else r = it->second;
        return juce::Font (juce::FontOptions (name, cap / r, style));
    }
    juce::Font sans (float cap, bool bold = false) { return capFont (sansName(), bold ? juce::Font::bold : juce::Font::plain, cap); }
    juce::Font wide (float cap) { return capFont (wideName(), juce::Font::bold, cap); }
    // texto centrado en (x, y) por su contorno real; 'track' = espacio extra entre letras (px)
    void textAt (Graphics& g, const juce::String& t, float x, float y, const juce::Font& f, float hs = 1.0f, float track = 0.0f)
    {
        juce::GlyphArrangement ga; ga.addLineOfText (f.withHorizontalScale (hs).withExtraKerningFactor (track), t, 0, 0);
        Path p; ga.createPath (p);
        const auto b = p.getBounds();
        p.applyTransform (juce::AffineTransform::translation (x - b.getCentreX(), y - b.getCentreY()));
        g.fillPath (p);
    }
    juce::String ch (int c) { return juce::String::charToString ((juce::juce_wchar) c); }

    // ------------------------------------------------------------------ geometria (frente 2U: 1388 x 255)
    const Rectangle<float> panelR { 6, 6, 1388, 255 }, trayR { 6, 270, 1388, 84 };
    const Pt cIn { 206, 142 }, cOut { 479, 142 }, cAtt { 696, 84 }, cRel { 696, 200 }, cHpf { 1062, 312 }, cMix { 1292, 312 };
    constexpr float bodyR = 46.0f, capR = 33.0f, dotsR = 62.0f, numsR = 78.0f, knobHalf = 58.0f;
    constexpr float sBodyR = 21.0f, sCapR = 14.5f, sDotsR = 33.0f, sNumsR = 44.0f, sKnobHalf = 30.0f;
    constexpr float tBodyR = 17.0f, tCapR = 11.5f, tDotsR = 26.0f, tKnobHalf = 24.0f;
    constexpr float bigA0 = -158.0f, bigA1 = 158.0f;                  // INPUT / OUTPUT
    constexpr float attA0 = -180.0f, attA1 = 135.0f;                  // ATTACK: OFF abajo, 1..7 cada 45 grados
    constexpr float relA0 = -135.0f, relA1 = 135.0f;
    constexpr float trA0 = -135.0f, trA1 = 135.0f;
    const float keyX = 879, meterX = 1210, keyYs[] { 72, 114, 156, 198 };
    constexpr float keyW = 24, keyH = 36;
    const Pt screws[] { { 99, 62 }, { 99, 206 }, { 1300, 62 }, { 1300, 206 } };
    const Pt slots[] { { 28, 24 }, { 28, 243 }, { 1372, 24 }, { 1372, 243 } };
    const Pt nutLink { 530, 312 }, nutDet { 800, 312 };
    constexpr float nutR = 13.0f;
    // VU
    const Rectangle<float> bezelR { 926, 52, 248, 140 }, windowR { 940, 64, 220, 116 }, lipR { 938, 160, 224, 22 };
    const Pt pivot { 1050, 300 };
    constexpr float arcR = 196.0f, swing = 28.0f, fullP = 1.4125375f;       // +3 VU = 1.4125 x (0 VU)
    float vuAngleP (float p) { return (-swing + 2.0f * swing * p / fullP) * deg; }
    float vuAngleDb (float db) { return vuAngleP (std::pow (10.0f, db / 20.0f)); }
    // bandeja
    const Rectangle<float> lcdR { 74, 293, 236, 38 }, keyPrevR { 34, 294, 34, 36 }, keyNextR { 316, 294, 34, 36 };

    // ------------------------------------------------------------------ serigrafia blanca del frente y de la bandeja
    void drawSilk (Graphics& g)
    {
        g.setColour (Colours::white);
        auto T = [&g] (const juce::String& t, float x, float y, float cap, bool bold = true, float hs = 1.0f, float tr = 0.0f)
        { textAt (g, t, x, y, sans (cap, bold), hs, tr); if (! bold) textAt (g, t, x + 0.4f, y, sans (cap, bold), hs, tr); };
        auto dot = [&g] (Pt c, float r) { g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2); };
        // ---- escalas de INPUT / OUTPUT: inf 48 36 30 24 18 12 6 0 (como el equipo) con puntos intermedios
        const char* bigNums[] { "", "48", "36", "30", "24", "18", "12", "6", "0" };
        for (Pt c : { cIn, cOut })
        {
            for (int i = 0; i <= 16; ++i)
            {
                const float a = (bigA0 + (bigA1 - bigA0) * (float) i / 16.0f) * deg;
                dot (polar (c, dotsR, a), i % 2 == 0 ? 2.3f : 1.5f);
            }
            for (int i = 0; i <= 8; ++i)
            {
                const float a = (bigA0 + (bigA1 - bigA0) * (float) i / 8.0f) * deg;
                const Pt q = polar (c, numsR, a);
                if (i == 0) { Path p; p.addCentredArc (0, 0, 2.6f, 2.6f, 0, 0, 2 * pi, true); const float w = 3.0f;          // infinito dibujado
                              g.strokePath (p, juce::PathStrokeType (1.5f), juce::AffineTransform::translation (q.x - w, q.y));
                              g.strokePath (p, juce::PathStrokeType (1.5f), juce::AffineTransform::translation (q.x + w, q.y)); }
                else T (bigNums[i], q.x, q.y, 9.5f, false);
            }
        }
        T ("INPUT", cIn.x, 238, 10.5f, true, 1.05f, 0.08f);
        T ("OUTPUT", cOut.x, 238, 10.5f, true, 1.05f, 0.08f);
        // ---- ATTACK / RELEASE: 1 3 5 7 con puntos en 2 4 6; OFF abajo del ATTACK
        for (Pt c : { cAtt, cRel })
        {
            for (int v = 1; v <= 7; ++v)
            {
                const float a = (-135.0f + 45.0f * (float) (v - 1)) * deg;
                if (v % 2 == 0) dot (polar (c, sDotsR, a), 1.9f);
                else { const Pt q = polar (c, sNumsR, a); T (juce::String (v), q.x, q.y, 8.5f, false); dot (polar (c, sDotsR, a), 1.4f); }
            }
        }
        T ("OFF", cAtt.x, cAtt.y + sNumsR + 1.0f, 8.0f, false, 1.0f, 0.05f);
        T ("ATTACK", cAtt.x, 22, 9.5f, true, 1.05f, 0.08f);
        T ("RELEASE", cRel.x, 248, 9.5f, true, 1.05f, 0.08f);
        // ---- RATIO y METER
        T ("RATIO", keyX, 32, 9.5f, true, 1.05f, 0.08f);
        const char* rn[] { "20", "12", "8", "4" };
        for (int i = 0; i < 4; ++i) T (rn[i], keyX - 37, keyYs[i], 10.5f, false);
        T ("METER", meterX + 8, 32, 9.5f, true, 1.05f, 0.08f);
        const char* mn[] { "GR", "+8", "+4", "OFF" };
        for (int i = 0; i < 4; ++i) T (mn[i], meterX + 34, keyYs[i], 10.0f, false);
        // ---- marca propia (no es la del fabricante original): emblema HM + modelo + fabricante
        g.drawEllipse (1031.0f - 13.0f, 32.0f - 13.0f, 26, 26, 2.0f);
        T ("HM", 1031, 32, 8.0f, true, 0.92f);
        textAt (g, "STUDIO", 1081, 37, sans (7.5f, true).italicised(), 1.0f, 0.06f);
        T ("TOP 76   LIMITING  AMPLIFIER", 1050, 212, 9.0f, true, 1.0f, 0.16f);
        textAt (g, "ANALOG HM", 1050, 236, wide (15.0f), 1.18f, 0.04f);
        T ("DJ PAVA HM MUSIC", 1300, 250, 5.8f, true, 1.0f, 0.12f);
        // ---- bandeja
        T ("PRESET", lcdR.getCentreX(), 283, 7.5f, true, 1.1f, 0.1f);
        T ("DUAL", nutLink.x - 52, 312, 9.0f, true); T ("LINK", nutLink.x + 50, 312, 9.0f, true);
        T ("STEREO", nutLink.x, 340, 6.8f, true, 1.0f, 0.12f);
        T ("CLEAN", nutDet.x - 56, 312, 9.0f, true); T ("VINTAGE", nutDet.x + 62, 312, 9.0f, true);
        T ("DETECTOR", nutDet.x, 340, 6.8f, true, 1.0f, 0.12f);
        T ("SC HPF", cHpf.x - 66, 312, 9.0f, true); T ("MIX", cMix.x - 56, 312, 9.0f, true);
        for (Pt c : { cHpf, cMix })
            for (int i = 0; i <= 10; ++i) dot (polar (c, tDotsR, (-135.0f + 27.0f * (float) i) * deg), 1.3f);
        T ("OFF", cHpf.x - 24, 342, 6.5f, false); T ("500", cHpf.x + 25, 342, 6.5f, false);
        T ("DRY", cMix.x - 26, 342, 6.3f, true); T ("WET", cMix.x + 26, 342, 6.3f, true);
    }
    // ------------------------------------------------------------------ escala del VU (tinta negra / roja sobre la cara ambar)
    void drawFace (Graphics& g, bool red)
    {
        const float dbs[] { -20, -10, -7, -5, -3, -1, 0, 1, 2, 3 };
        const juce::String en = ch (0x2013);
        auto T = [&g] (const juce::String& t, float x, float y, float cap) { textAt (g, t, x, y, sans (cap, false), 0.92f); textAt (g, t, x + 0.25f, y, sans (cap, false), 0.92f); };
        g.setColour (Colours::white);
        if (! red)
        {
            Path arc; arc.addCentredArc (pivot.x, pivot.y, arcR, arcR, 0, vuAngleDb (-20) - 0.01f, vuAngleDb (0), true);
            g.strokePath (arc, juce::PathStrokeType (1.2f));
        }
        else
        {
            Path arc; arc.addCentredArc (pivot.x, pivot.y, arcR, arcR, 0, vuAngleDb (0), vuAngleDb (3) + 0.004f, true);
            g.strokePath (arc, juce::PathStrokeType (1.2f));
            Path band; const int N = 24;
            for (int i = 0; i <= N; ++i) { const float t = (float) i / N, a = vuAngleDb (0) + t * (vuAngleDb (3) - vuAngleDb (0)); const Pt p = polar (pivot, arcR - 1.5f, a); if (i == 0) band.startNewSubPath (p); else band.lineTo (p); }
            for (int i = N; i >= 0; --i) { const float t = (float) i / N, a = vuAngleDb (0) + t * (vuAngleDb (3) - vuAngleDb (0)); band.lineTo (polar (pivot, arcR - 1.5f - (3.5f + 7.5f * t), a)); }
            band.closeSubPath(); g.fillPath (band);
        }
        for (float db : dbs)
        {
            if ((db > 0) != red) continue;
            const float a = vuAngleDb (db);
            g.drawLine ({ polar (pivot, arcR, a), polar (pivot, arcR + 9.0f, a) }, 1.3f);
            const Pt q = polar (pivot, arcR + 18.0f, a);
            const juce::String s = db > 0 ? "+" + juce::String ((int) db) : db < 0 ? en + juce::String ((int) -db) : juce::String ("0");
            T (s, q.x, q.y, 7.6f);
        }
        if (! red)
        {
            for (float db : { -15.0f, -6.0f, -4.0f, -2.0f, -0.5f }) { const float a = vuAngleDb (db); g.drawLine ({ polar (pivot, arcR, a), polar (pivot, arcR + 5.0f, a) }, 1.0f); }
            for (int i = 0; i <= 5; ++i) { const float p = 0.2f * (float) i, a = vuAngleP (p); const Pt q = polar (pivot, arcR - 10.0f, a); T (juce::String (i * 20), q.x, q.y, 6.2f); }
            T ("VU", 957, 79, 7.8f);
            g.drawEllipse (1050 - 7.0f, 140 - 7.0f, 14, 14, 1.0f);
            textAt (g, "HM", 1050, 140, sans (4.6f, true), 0.92f);
        }
        else
        {
            T ("VU", 1143, 79, 7.8f);
            for (float db : { 0.5f, 1.5f, 2.5f }) { const float a = vuAngleDb (db); g.drawLine ({ polar (pivot, arcR, a), polar (pivot, arcR + 5.0f, a) }, 1.0f); }
        }
    }

    // ------------------------------------------------------------------ mascaras para la placa
    std::vector<float> maskOf (int w, int h, float k, const std::function<void (Graphics&)>& draw, int r)
    {
        juce::Image m (juce::Image::SingleChannel, w, h, true);
        { Graphics g (m); g.addTransform (juce::AffineTransform::scale (k)); g.setColour (Colours::white); draw (g); }
        std::vector<float> a ((size_t) (w * h)), t ((size_t) (w * h));
        { juce::Image::BitmapData bd (m, juce::Image::BitmapData::readOnly); for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) a[(size_t) (y * w + x)] = bd.getPixelColour (x, y).getFloatAlpha(); }
        for (int pass = 0; pass < (r > 0 ? 2 : 0); ++pass)
        {
            for (int y = 0; y < h; ++y) { float acc = 0; for (int x = -r; x < w + r; ++x) { if (x + r < w) acc += a[(size_t) (y * w + x + r)]; if (x - r - 1 >= 0) acc -= a[(size_t) (y * w + x - r - 1)]; if (x >= 0 && x < w) t[(size_t) (y * w + x)] = acc / (float) (2 * r + 1); } }
            for (int x = 0; x < w; ++x) { float acc = 0; for (int y = -r; y < h + r; ++y) { if (y + r < h) acc += t[(size_t) ((y + r) * w + x)]; if (y - r - 1 >= 0) acc -= t[(size_t) ((y - r - 1) * w + x)]; if (y >= 0 && y < h) a[(size_t) (y * w + x)] = acc / (float) (2 * r + 1); } }
        }
        return a;
    }

    // ------------------------------------------------------------------ placa (se calcula una vez)
    juce::Image renderPlate()
    {
        static juce::Image cached;
        if (cached.isValid()) return cached;
        constexpr float k = 1.5f;
        const int iw = (int) ((float) Content::W * k), ih = (int) ((float) Content::H * k);
        const auto panel = maskOf (iw, ih, k, [] (Graphics& g) { g.fillRoundedRectangle (panelR, 2.0f); }, 1);
        const auto tray = maskOf (iw, ih, k, [] (Graphics& g) { g.fillRoundedRectangle (trayR, 2.5f); }, 1);
        const auto silk = maskOf (iw, ih, k, [] (Graphics& g) { drawSilk (g); }, 0);
        const auto holes = maskOf (iw, ih, k, [] (Graphics& g)
        {
            for (Pt c : slots) g.fillRoundedRectangle (c.x - 11.0f, c.y - 5.5f, 22.0f, 11.0f, 5.5f);
            g.fillEllipse (342 - 3.5f, 197 - 3.5f, 7, 7);                         // agujero del ajuste interno
        }, 1);
        const auto wells = maskOf (iw, ih, k, [] (Graphics& g)
        { for (float x : { keyX, meterX }) g.fillRoundedRectangle (x - keyW * 0.5f - 3.0f, keyYs[0] - keyH * 0.5f - 3.0f, keyW + 6.0f, keyYs[3] - keyYs[0] + keyH + 6.0f, 3.0f); }, 1);
        const auto outer = maskOf (iw, ih, k, [] (Graphics& g) { g.fillRoundedRectangle (bezelR, 6.0f); }, 3);
        const auto ledge = maskOf (iw, ih, k, [] (Graphics& g) { g.fillRoundedRectangle (windowR.expanded (5.0f), 4.0f); }, 2);
        const auto win = maskOf (iw, ih, k, [] (Graphics& g) { g.fillRoundedRectangle (windowR, 2.5f); }, 2);
        const auto winSh = maskOf (iw, ih, k, [] (Graphics& g) { g.fillRoundedRectangle (windowR.translated (3.0f, 10.0f).reduced (5.0f, 0.0f), 3.0f); }, 10);
        const auto inkB = maskOf (iw, ih, k, [] (Graphics& g) { drawFace (g, false); }, 0);
        const auto inkR = maskOf (iw, ih, k, [] (Graphics& g) { drawFace (g, true); }, 0);
        const auto glassM = maskOf (iw, ih, k, [] (Graphics& g) { g.fillRoundedRectangle (lcdR, 4.0f); }, 1);
        const auto paintM = t3p::panelPaint(), inkM = t3p::ink();
        const auto chassis = r3d::srgb (0.025f, 0.025f, 0.028f, 0, 0.3f);
        const float cx = (float) iw * 0.5f, cy = (float) ih * 0.5f;
        auto fn = [&] (float rx, float ry)
        {
            const float px = rx + cx, py = ry + cy;
            const int ix = juce::jlimit (0, iw - 1, (int) px), iy = juce::jlimit (0, ih - 1, (int) py);
            const size_t i = (size_t) (iy * iw + ix);
            const float X = px / k, Y = py / k;
            r3d::Sample s; s.m = chassis;
            float h = 0;
            if (panel[i] > 0)
            {
                // aluminio negro anodizado con cepillado horizontal (vetas largas y finas que brillan con la luz)
                const float streak = r3d::vnoise (px * 0.0025f, py * 1.35f, 43), fineS = r3d::vnoise (px * 0.02f, py * 2.6f, 47);
                const float cloud = r3d::fbm (px * 0.004f, py * 0.004f, 3, 3), grain = r3d::hash (ix, iy, 41);
                const float tone = 0.78f + 0.06f * cloud + 0.06f * (streak - 0.5f) + 0.04f * (fineS - 0.5f) + 0.025f * (grain - 0.5f);
                r3d::Mat m = paintM; m.r *= tone; m.g *= tone; m.b *= tone * 1.03f; m.gloss = 0.46f + 0.10f * streak;
                s.m = t3p::mixM (s.m, m, panel[i]);
                h += 4.0f * panel[i] + 0.12f * fineS + 0.05f * streak + 0.02f * grain;
                // bisel del borde del frente
                const float ex = std::min ({ X - panelR.getX(), panelR.getRight() - X, Y - panelR.getY(), panelR.getBottom() - Y });
                if (ex < 3.0f) h -= 1.2f * (1.0f - ex / 3.0f) * panel[i];
            }
            if (tray[i] > 0)
            {
                const float tone = 0.94f + 0.06f * r3d::fbm (px * 0.02f, py * 0.02f, 5, 3) + 0.05f * (r3d::vnoise (px * 0.003f, py * 0.7f, 13) - 0.5f);
                r3d::Mat m = r3d::srgb (0.140f * tone, 0.145f * tone, 0.155f * tone, 0, 0.42f);
                s.m = t3p::mixM (s.m, m, tray[i]);
                h += 4.0f * tray[i];
            }
            if (silk[i] > 0) { s.m = t3p::mixM (s.m, inkM, silk[i]); h += 0.35f * silk[i]; }
            if (holes[i] > 0) { s.m = t3p::mixM (s.m, r3d::srgb (0.008f, 0.008f, 0.009f, 0, 0.2f), holes[i]); h -= 7.0f * holes[i]; }
            if (wells[i] > 0) { s.m = t3p::mixM (s.m, r3d::srgb (0.012f, 0.012f, 0.013f, 0, 0.25f), wells[i]); h -= 3.0f * wells[i]; }
            // marco del medidor (plastico negro en relieve) y la cara ambar hundida
            const float o = outer[i], w = win[i], le = ledge[i];
            if (o > 0)
            {
                h += 10.0f * o - 5.0f * le - 14.0f * w;
                s.m = t3p::mixM (s.m, t3p::plastic (0.030f, 0.030f, 0.033f, 0.72f), juce::jlimit (0.0f, 1.0f, o * 1.4f));
                if (le > 0.02f && w < 0.98f) s.m = t3p::mixM (s.m, t3p::plastic (0.018f, 0.018f, 0.020f, 0.38f), juce::jlimit (0.0f, 1.0f, le * (1.0f - w)));
                if (w > 0)
                {
                    const float lit = 0.25f + 0.75f * winSh[i];
                    const float gx = (X - 1050.0f) / 115.0f, gy = (Y - 125.0f) / 55.0f;
                    const float glow = (1.0f - 0.24f * gx * gx - 0.12f * gy * gy) * lit;
                    r3d::Mat face = r3d::srgb (0.97f, 0.84f, 0.58f, 0, 0.22f); face.f0 = 0.03f;
                    face.r *= lit; face.g *= lit; face.b *= lit;
                    face.er = 0.48f * glow; face.eg = 0.32f * glow; face.eb = 0.11f * glow;
                    const float tex = 0.97f + 0.03f * r3d::vnoise (px * 0.3f, py * 0.3f, 21);
                    face.r *= tex; face.g *= tex; face.b *= tex;
                    if (inkB[i] > 0) { r3d::Mat in = r3d::srgb (0.06f, 0.055f, 0.05f, 0, 0.3f); in.er = face.er * 0.10f; in.eg = face.eg * 0.10f; in.eb = face.eb * 0.10f; face = t3p::mixM (face, in, inkB[i]); }
                    if (inkR[i] > 0) { r3d::Mat in = r3d::srgb (0.82f, 0.10f, 0.07f, 0, 0.3f); in.er = face.er * 0.55f; in.eg = face.eg * 0.06f; in.eb = face.eb * 0.04f; face = t3p::mixM (face, in, inkR[i]); }
                    s.m = t3p::mixM (s.m, face, w);
                }
            }
            if (glassM[i] > 0) { auto gm = r3d::srgb (0.010f, 0.010f, 0.011f, 0, 0.95f); gm.f0 = 0.06f; s.m = t3p::mixM (s.m, gm, glassM[i]); h -= 3.0f * glassM[i]; }
            // tuercas de las palancas de la bandeja
            for (Pt c : { nutLink, nutDet })
            {
                const float dx = (X - c.x) * k, dy = (Y - c.y) * k, R = nutR * k, rr = std::sqrt (dx * dx + dy * dy);
                if (rr < R + 1) { const auto q = t3p::hexNut (dx, dy, R); if (q.m.alpha > 0) { s.m = t3p::mixM (s.m, q.m, q.m.alpha); h += q.h * q.m.alpha; } }
                if (rr < R * 1.6f) { const float ao = 1.0f - 0.45f * std::exp (-std::pow ((rr - 0.95f * R) / (0.25f * R), 2.0f)); s.m.r *= ao; s.m.g *= ao; s.m.b *= ao; }
            }
            for (Pt c : screws)
            {
                const float dx = (X - c.x) * k, dy = (Y - c.y) * k, R = 8.5f * k, rr = std::sqrt (dx * dx + dy * dy);
                if (rr < R + 1) { const auto q = t3p::phillips (dx, dy, R); s.m = t3p::mixM (s.m, q.m, q.m.alpha); h += q.h * q.m.alpha; }
                else if (rr < R * 1.7f) { const float ao = 1.0f - 0.45f * std::exp (-std::pow ((rr - R) / (0.3f * R), 2.0f)); s.m.r *= ao; s.m.g *= ao; s.m.b *= ao; }
            }
            // sombra de contacto alrededor de las perillas (se apoyan sobre el frente)
            struct Kc { Pt c; float R; };
            for (const Kc& kc : { Kc { cIn, bodyR }, Kc { cOut, bodyR }, Kc { cAtt, sBodyR }, Kc { cRel, sBodyR }, Kc { cHpf, tBodyR }, Kc { cMix, tBodyR } })
            {
                const float dx = X - kc.c.x - 1.5f, dy = Y - kc.c.y - 3.0f, rr = std::sqrt (dx * dx + dy * dy);
                if (rr < kc.R * 1.5f) { const float ao = 1.0f - 0.55f * std::exp (-std::pow ((rr - kc.R) / (0.16f * kc.R), 2.0f)) - (rr < kc.R ? 0.4f : 0.0f); s.m.r *= ao; s.m.g *= ao; s.m.b *= ao; }
            }
            s.h = h; return s;
        };
        cached = r3d::render (iw, ih, fn);
        return cached;
    }
    // labio inferior del medidor con el tornillo del cero (se dibuja ENCIMA de la aguja)
    juce::Image renderCover()
    {
        static juce::Image cached;
        if (cached.isValid()) return cached;
        constexpr float k = 2.0f;
        const int iw = (int) (lipR.getWidth() * k), ih = (int) (lipR.getHeight() * k);
        const float cx = (float) iw * 0.5f, cy = (float) ih * 0.5f;
        cached = r3d::render (iw, ih, [=] (float rx, float ry)
        {
            const float X = (rx + cx) / k, Y = (ry + cy) / k;
            r3d::Sample s; s.m = t3p::plastic (0.040f, 0.040f, 0.044f, 0.55f);
            const float top = Y - 2.0f;
            s.m.alpha = juce::jlimit (0.0f, 1.0f, top * k + 0.5f) * juce::jlimit (0.0f, 1.0f, (X - 2.0f) * k + 0.5f) * juce::jlimit (0.0f, 1.0f, (lipR.getWidth() - 2.0f - X) * k + 0.5f);
            float h = 8.0f * r3d::fillet (top, 4.0f);
            const float tex = 0.92f + 0.08f * r3d::vnoise (rx * 0.2f, ry * 0.2f, 31);
            s.m.r *= tex; s.m.g *= tex; s.m.b *= tex;
            const float dx = (X - lipR.getWidth() * 0.5f) * k, dy = (Y - 12.0f) * k, R = 5.5f * k, rr = std::sqrt (dx * dx + dy * dy);
            if (rr < R + 1)
            {
                const float u = rr / R;
                r3d::Mat sm = t3p::plastic (0.06f, 0.06f, 0.065f, 0.7f);
                float hs = 8.0f + 0.25f * R * std::sqrt (std::max (0.0f, 1.0f - u * u));
                if (std::abs (dx * 0.5f + dy * 0.866f) < 0.12f * R && rr < 0.8f * R) { hs -= 0.18f * R; sm = t3p::plastic (0.01f, 0.01f, 0.01f, 0.2f); }
                const float a = juce::jlimit (0.0f, 1.0f, R + 0.5f - rr);
                s.m = t3p::mixM (s.m, sm, a); h = h * (1 - a) + hs * a;
            }
            s.h = h; return s;
        }, { 0, 2.5f * k, 3.5f * k, 0.45f, 0, 0 });
        return cached;
    }
    // vidrio del VU (reflejos, ondulado del cristal, polvo y canto) por encima de la aguja
    juce::Image renderGlass()
    {
        static juce::Image cached;
        if (cached.isValid()) return cached;
        constexpr float k = 2.0f;
        const int iw = (int) (windowR.getWidth() * k), ih = (int) (windowR.getHeight() * k);
        juce::Image img (juce::Image::ARGB, iw, ih, true);
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
        const float Wd = windowR.getWidth(), Hd = windowR.getHeight();
        for (int y = 0; y < ih; ++y)
            for (int x = 0; x < iw; ++x)
            {
                const float X = (float) x / k, Y = (float) y / k;
                const float wave = r3d::fbm (X * 0.025f, Y * 0.07f, 51, 3);
                const float sk = X * 0.22f;
                float a = 0.0f;
                a += 0.15f * r3d::smooth ((Y + sk * 0.10f - 2.0f) / 3.0f) * r3d::smooth ((20.0f - (Y + sk * 0.10f) + 4.0f * (wave - 0.5f)) / 10.0f)
                           * r3d::smooth ((X - 12.0f) / 20.0f) * r3d::smooth ((Wd * 0.70f - X) / 40.0f);
                a += 0.040f * r3d::smooth ((X - (Wd - 40.0f)) / 6.0f) * r3d::smooth ((Wd - 22.0f - X) / 6.0f) * r3d::smooth ((Hd - 20.0f - Y) / 35.0f);
                a += 0.055f * (1.0f - r3d::smooth ((X + Y * 0.7f) / 100.0f));
                a += 0.016f * r3d::clamp01 ((r3d::fbm (X * 0.04f, Y * 0.04f, 57, 3) - 0.55f) * 4.0f);
                const float sp = r3d::hash (x / 3, y / 3, 61);
                if (sp > 0.9975f) a += 0.10f * r3d::hash (x, y, 62);
                if (Y < 1.2f) a += 0.22f;
                if (X < 1.0f || X > Wd - 1.0f) a += 0.08f;
                a *= 0.85f + 0.3f * wave;
                bd.setPixelColour (x, y, Colours::white.withAlpha (juce::jlimit (0.0f, 0.5f, a)));
            }
        cached = img;
        return cached;
    }

    // ------------------------------------------------------------------ imagenes de piezas (cache)
    std::map<juce::String, juce::Image>& cache() { static std::map<juce::String, juce::Image> c; return c; }
    void knobDims (int kind, float& half, float& R, float& Rc) { if (kind == 0) { half = knobHalf; R = bodyR; Rc = capR; } else if (kind == 1) { half = sKnobHalf; R = sBodyR; Rc = sCapR; } else { half = tKnobHalf; R = tBodyR; Rc = tCapR; } }
    const juce::Image& bodyImage (int kind, int px)
    {
        const juce::String key = "k" + juce::String (kind) + "_" + juce::String (px);
        auto& c = cache();
        auto it = c.find (key);
        if (it != c.end()) return it->second;
        float half, R0, Rc0; knobDims (kind, half, R0, Rc0);
        const float sc = (float) px / (2.0f * half), R = R0 * sc, Rc = Rc0 * sc;
        auto img = r3d::render (px, px, [=] (float x, float y) { return t3p::knobBody (x, y, R, Rc, kind == 0 ? 54 : 34, 0.55f * R, -0.05f * R, -0.06f * R, 0, 5 + kind * 3); },
                                { 0.05f * R, 0.11f * R, 0.13f * R, 0.62f, 0.05f * R, 0.5f });
        return c[key] = img;
    }
    const juce::Image& leverImage (bool right, int px)
    {
        const juce::String key = juce::String (right ? "R" : "L") + juce::String (px);
        auto& c = cache();
        auto it = c.find (key);
        if (it != c.end()) return it->second;
        const float sc = (float) px / 56.0f, R = nutR * sc;
        auto img = r3d::render (px, px, [=] (float x, float y) { return t3p::batLever (x, y, R, right ? 1.0f : -1.0f); }, { 0.10f * R * (right ? 1.0f : -1.0f), 0.30f * R, 0.20f * R, 0.55f, 0, 0 });
        return c[key] = img;
    }
    const juce::Image& keyImage (int w, int h, bool pressed, bool tall)
    {
        const juce::String key = juce::String (tall ? "T" : "K") + juce::String (w) + "x" + juce::String (h) + (pressed ? "d" : "u");
        auto& c = cache();
        auto it = c.find (key);
        if (it != c.end()) return it->second;
        auto img = r3d::render (w, h, [=] (float x, float y)
        {
            auto s = t3p::squareKey (x, y, (float) w * 0.5f - 3.0f, (float) h * 0.5f - 3.0f, pressed);
            if (tall)
            {
                // tecla negra de baquelita con la cara un poco concava; hundida = mas abajo y mas mate
                const float u = y / ((float) h * 0.5f);
                s.h += (pressed ? -1.5f : 1.0f) - 1.2f * (1.0f - u * u) * (pressed ? 0.4f : 1.0f);
                s.m = t3p::plastic (pressed ? 0.040f : 0.055f, pressed ? 0.040f : 0.056f, pressed ? 0.043f : 0.062f, pressed ? 0.50f : 0.70f);
                s.m.alpha = t3p::squareKey (x, y, (float) w * 0.5f - 3.0f, (float) h * 0.5f - 3.0f, pressed).m.alpha;
            }
            return s;
        }, { 0, pressed ? 1.0f : 2.5f, 3.0f, 0.55f, 2.0f, 0.45f });
        return c[key] = img;
    }
}

// =====================================================================================================
Look::Look()
{
    setColour (juce::PopupMenu::backgroundColourId, Colour (0xff1b1c1f));
    setColour (juce::PopupMenu::textColourId, Colour (0xffe8e6e2));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colour (0xff8a5a1c));
    setColour (juce::PopupMenu::headerTextColourId, Colour (0xffb0aca4));
    setColour (juce::TooltipWindow::backgroundColourId, Colour (0xff1b1c1f));
    setColour (juce::TooltipWindow::textColourId, Colour (0xffe8e6e2));
}
void Look::drawBubble (Graphics& g, juce::BubbleComponent&, const juce::Point<float>&, const Rectangle<float>& body)
{
    g.setColour (Colour (0xf01b1c1f)); g.fillRoundedRectangle (body, 4);
    g.setColour (Colour (0x80e8e6e2)); g.drawRoundedRectangle (body.reduced (0.5f), 4, 1.0f);
}

Knob::Knob (APVTS& s, const juce::String& id, int k, float a0, float a1) : kind (k)
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (a0 * deg + 2 * pi, a1 * deg + 2 * pi, true);
    setMouseDragSensitivity (kind == 0 ? 300 : 220);
    setPopupDisplayEnabled (true, true, nullptr, 900);
    setVelocityBasedMode (false);
    if (auto* p = s.getParameter (id)) setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    att = std::make_unique<APVTS::SliderAttachment> (s, id, *this);
}
float Knob::angleRad() const
{
    const auto rp = getRotaryParameters();
    const float pos = (float) getNormalisableRange().convertTo0to1 (getValue());
    return rp.startAngleRadians + pos * (rp.endAngleRadians - rp.startAngleRadians);
}
bool Knob::hitTest (int x, int y)
{
    float half, R, Rc; knobDims (kind, half, R, Rc);
    const float c = (float) getWidth() * 0.5f, dx = (float) x - c, dy = (float) y - c;
    return dx * dx + dy * dy < (R + 6) * (R + 6);
}
void Knob::paint (Graphics& g)
{
    const float S = (float) getWidth(), half = S * 0.5f;
    float h0, R, Rc; knobDims (kind, h0, R, Rc);
    const float a = angleRad();
    g.setOpacity (1.0f);
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (bodyImage (kind, juce::roundToInt (S * 2.0f)), Rectangle<float> (0, 0, S, S));
    // raya blanca sobre el aro negro moleteado + marca grabada en la tapa
    const Pt top = Pt (half, half) + Pt (-0.05f * R, -0.06f * R);
    const float w = kind == 0 ? 3.2f : 2.2f;
    const juce::Line<float> lt (polar (top, Rc + (kind == 0 ? 2.5f : 1.5f), a), polar (top, R - (kind == 0 ? 2.0f : 1.2f), a));
    g.setColour (Colours::black.withAlpha (0.55f)); g.drawLine (lt, w + 1.6f);
    g.setColour (Colour (0xfff6f5f0)); g.drawLine (lt, w);
    const juce::Line<float> lc (polar (top, Rc * 0.30f, a), polar (top, Rc * 0.86f, a));
    g.setColour (Colours::black.withAlpha (0.45f)); g.drawLine (lc, kind == 0 ? 1.6f : 1.1f);
    g.setColour (Colours::white.withAlpha (0.35f)); g.drawLine ({ lc.getStart().translated (0.6f, 0.6f), lc.getEnd().translated (0.6f, 0.6f) }, 0.6f);
}

void Toggle::paint (Graphics& g)
{
    const bool right = isRight != nullptr && isRight();
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (leverImage (right, getWidth() * 2), getLocalBounds().toFloat());
}

void PushKey::mouseUp (const juce::MouseEvent& e)
{
    held = false; repaint();
    if (getLocalBounds().contains (e.getPosition()) && onClick) onClick (e.mods);
}
void PushKey::paint (Graphics& g)
{
    const bool down = held || (isDown != nullptr && isDown());
    const int w = getWidth(), h = getHeight();
    const float gl = glow != nullptr ? glow() : 0.0f;
    if (down && gl > 0.01f)                                          // luz de la lampara que se escapa alrededor de la tecla hundida
    {
        const auto r = getLocalBounds().toFloat().reduced (2.0f);
        for (int i = 3; i >= 1; --i)
        {
            g.setColour (Colour (0xffffa23a).withAlpha (0.10f * gl * (float) i / 3.0f));
            g.drawRoundedRectangle (r.reduced ((float) (3 - i) * 0.7f), 3.0f, 1.2f);
        }
    }
    g.setOpacity (1.0f);
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (keyImage (w * 2, h * 2, down, true), getLocalBounds().toFloat());
    if (down && gl > 0.01f)
    {
        g.setColour (Colour (0xffffb050).withAlpha (0.10f * gl));
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (5.0f, 6.0f), 2.0f);
    }
}

void Key::paint (Graphics& g)
{
    const int w = getWidth(), h = getHeight();
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (keyImage (w * 2, h * 2, held, false), getLocalBounds().toFloat());
    g.setColour (Colour (0xffd8d4cc));
    const float cx = (float) w * 0.5f, cy = (float) h * 0.5f + (held ? 1.0f : 0.0f), s = 5.5f;
    Path p;
    if (left) p.addTriangle (cx - s, cy, cx + s * 0.7f, cy - s, cx + s * 0.7f, cy + s);
    else p.addTriangle (cx + s, cy, cx - s * 0.7f, cy - s, cx - s * 0.7f, cy + s);
    g.fillPath (p);
}

// =====================================================================================================
Content::Content (Top76Processor& p)
    : proc (p),
      input (p.apvts, "input", 0, bigA0, bigA1), output (p.apvts, "output", 0, bigA0, bigA1),
      attack (p.apvts, "attack", 1, attA0, attA1), release (p.apvts, "release", 1, relA0, relA1),
      hpf (p.apvts, "hpf", 2, trA0, trA1), mix (p.apvts, "mix", 2, trA0, trA1)
{
    plate = renderPlate();
    cover = renderCover();
    glass = renderGlass();
    vblank = std::make_unique<juce::VBlankAttachment> (this, [this] { animate(); });
    for (juce::Component* c : std::initializer_list<juce::Component*> { &input, &output, &attack, &release, &hpf, &mix, &linkSw, &detSw, &prevKey, &nextKey })
        addAndMakeVisible (c);
    // RATIO: teclas de arriba a abajo 20, 12, 8, 4. Shift-clic (o clic derecho) = ALL BUTTONS
    const int ratioOfKey[] { 3, 2, 1, 0 };
    for (int i = 0; i < 4; ++i)
    {
        auto& k = ratioKeys[(size_t) i];
        addAndMakeVisible (k);
        const int r = ratioOfKey[i];
        k.isDown = [this, r] { const int cur = (int) std::lround (param (proc, "ratio")); return cur == r || cur == 4; };
        k.onClick = [this, r] (const juce::ModifierKeys& m)
        {
            const int cur = (int) std::lround (param (proc, "ratio"));
            const bool all = m.isShiftDown() || m.isPopupMenu() || m.isAltDown();
            setParam (proc, "ratio", (float) (all ? (cur == 4 ? r : 4) : r));
            for (auto& q : ratioKeys) q.repaint();
        };
        k.glow = [this] { return lamp; };
        k.setTooltip (U ("RATIO: 4:1 y 8:1 comprimen, 12:1 y 20:1 limitan (el umbral sube con la relación).\nShift-clic o clic derecho: ALL BUTTONS (las cuatro hundidas)"));
    }
    const char* meterTips[] { "METER GR: la aguja muestra la reducción de ganancia", "METER +8: nivel de salida (0 VU = +8 dBu = -16 dBFS)",
                              "METER +4: nivel de salida (0 VU = +4 dBu = -20 dBFS)", "OFF: apaga el equipo (pasa la señal sin procesar)" };
    for (int i = 0; i < 4; ++i)
    {
        auto& k = meterKeys[(size_t) i];
        addAndMakeVisible (k);
        k.isDown = [this, i] { return (int) std::lround (param (proc, "meter")) == i; };
        k.onClick = [this, i] (const juce::ModifierKeys&) { setParam (proc, "meter", (float) i); for (auto& q : meterKeys) q.repaint(); repaint(); };
        k.glow = [this] { return lamp; };
        k.setTooltip (U (meterTips[i]));
    }
    auto flip = [this] (const char* id) { setParam (proc, id, param (proc, id) > 0.5f ? 0.0f : 1.0f); repaint(); };
    linkSw.isRight = [this] { return param (proc, "link") > 0.5f; }; linkSw.onClick = [flip] { flip ("link"); };
    detSw.isRight = [this] { return param (proc, "detector") > 0.5f; }; detSw.onClick = [flip] { flip ("detector"); };
    prevKey.left = true; nextKey.left = false;
    prevKey.onClick = [this] { const int n = proc.getNumPrograms(); proc.setCurrentProgram ((proc.getCurrentProgram() + n - 1) % n); repaint(); };
    nextKey.onClick = [this] { const int n = proc.getNumPrograms(); proc.setCurrentProgram ((proc.getCurrentProgram() + 1) % n); repaint(); };
    input.setTooltip (U ("INPUT: cuánta señal entra al FET = cuánta compresión (no tiene umbral: el umbral es este control)"));
    output.setTooltip (U ("OUTPUT: volumen de salida (no cambia la compresión)"));
    attack.setTooltip (U ("ATTACK: 1 = lento, 7 = rapidísimo (gira a la izquierda del 1 = OFF: solo el color, sin compresión)"));
    release.setTooltip (U ("RELEASE: 1 = lento (1.1 s), 7 = rápido (50 ms). Depende del programa: tras compresión larga suelta en dos etapas"));
    hpf.setTooltip (U ("SC HPF: la cadena lateral no escucha los graves (el bajo no hace bombear la voz). OFF a la izquierda"));
    mix.setTooltip (U ("MIX: compresión en paralelo (seco / comprimido), sin desfase"));
    linkSw.setTooltip (U ("STEREO: DUAL = cada lado comprime solo; LINK = los dos lados igual (imagen estable)"));
    detSw.setTooltip (U ("DETECTOR: CLEAN = envolvente exacta, graves limpios sin distorsión de rizado. VINTAGE = rectificador del equipo (más sucio en graves con release rápido)"));
    prevKey.setTooltip (U ("Preset anterior")); nextKey.setTooltip (U ("Preset siguiente"));
    lamp = proc.powered() ? 1.0f : 0.0f;
    lastT = juce::Time::getMillisecondCounterHiRes();
}

void Content::resized()
{
    auto place = [] (juce::Component& c, Pt ctr, float hb) { c.setBounds (juce::roundToInt (ctr.x - hb), juce::roundToInt (ctr.y - hb), juce::roundToInt (hb * 2), juce::roundToInt (hb * 2)); };
    place (input, cIn, knobHalf); place (output, cOut, knobHalf);
    place (attack, cAtt, sKnobHalf); place (release, cRel, sKnobHalf);
    place (hpf, cHpf, tKnobHalf); place (mix, cMix, tKnobHalf);
    place (linkSw, nutLink, 28); place (detSw, nutDet, 28);
    for (int i = 0; i < 4; ++i)
    {
        ratioKeys[(size_t) i].setBounds (juce::roundToInt (keyX - keyW * 0.5f), juce::roundToInt (keyYs[i] - keyH * 0.5f), (int) keyW, (int) keyH);
        meterKeys[(size_t) i].setBounds (juce::roundToInt (meterX - keyW * 0.5f), juce::roundToInt (keyYs[i] - keyH * 0.5f), (int) keyW, (int) keyH);
    }
    prevKey.setBounds (keyPrevR.toNearestInt()); nextKey.setBounds (keyNextR.toNearestInt());
}

void Content::paint (Graphics& g)
{
    g.fillAll (Colour (0xff0b0b0c));
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (plate, Rectangle<float> (0, 0, (float) W, (float) H));
    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (windowR.toNearestInt());
        // lampara apagada: la cara ambar queda en penumbra
        if (lamp < 0.999f) { g.setColour (Colour (0xff120c05).withAlpha (0.72f * (1.0f - lamp))); g.fillRect (windowR); }
        // aguja (sombra suave sobre la cara + cuerpo negro laqueado)
        const float a = vuAngleP (vuPos);
        const Pt tip = polar (pivot, arcR + 13.0f, a), base = polar (pivot, 118.0f, a);
        for (int i = 4; i >= 1; --i)
        {
            g.setColour (Colour (0xff3a2208).withAlpha (0.030f * (float) i * (0.4f + 0.6f * lamp)));
            g.drawLine ({ base.translated (2.0f, 3.5f), tip.translated (2.0f, 3.5f) }, 0.5f + 1.0f * (float) i);
        }
        Path nd;
        const Pt n (std::cos (a), std::sin (a));
        nd.startNewSubPath (base + n * 1.2f); nd.lineTo (tip + n * 0.4f); nd.lineTo (tip - n * 0.4f); nd.lineTo (base - n * 1.2f); nd.closeSubPath();
        g.setColour (Colour (0xff0d0c0b)); g.fillPath (nd);
        g.setColour (Colours::white.withAlpha (0.10f));
        g.drawLine ({ base - n * 0.6f, tip - n * 0.2f }, 0.45f);
        g.setOpacity (1.0f);
        g.drawImage (cover, lipR);
        g.setColour (Colours::black.withAlpha (0.10f)); g.fillRect (windowR.getX(), windowR.getY() + 1.2f, windowR.getWidth(), 1.6f);
        g.setOpacity (1.0f);
        g.drawImage (glass, windowR);
    }
    // ---- nombre del preset (pantalla ambar)
    g.setColour (Colour (0xffffb347).withAlpha (0.35f + 0.65f * lamp));
    g.setFont (sans (9.5f, true));
    g.drawFittedText (proc.getProgramName (proc.getCurrentProgram()), lcdR.reduced (8, 5).toNearestInt(), juce::Justification::centred, 2, 0.9f);
}

void Content::mouseUp (const juce::MouseEvent& e)
{
    if (lcdR.contains (e.position)) presetMenu();
}
void Content::presetMenu()
{
    juce::PopupMenu m; m.addSectionHeader ("PRESETS");
    for (int i = 0; i < proc.getNumPrograms(); ++i) m.addItem (i + 1, proc.getProgramName (i), true, i == proc.getCurrentProgram());
    juce::Component::SafePointer<Content> sp (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (lcdR.toNearestInt())), [sp] (int r) { if (sp != nullptr && r > 0) { sp->proc.setCurrentProgram (r - 1); sp->repaint(); } });
}

void Content::tick()
{
    animate();
    const int bits = (int) std::lround (param (proc, "meter")) + 4 * (int) std::lround (param (proc, "ratio")) + (param (proc, "link") > 0.5f ? 32 : 0)
                   + (param (proc, "detector") > 0.5f ? 64 : 0) + 128 * proc.getCurrentProgram();
    if (bits != lastBits) { lastBits = bits; for (auto& k : ratioKeys) k.repaint(); for (auto& k : meterKeys) k.repaint(); repaint(); }
}
void Content::animate()
{
    // VU: bobina movil de 2do orden (wn 13 rad/s, amortiguamiento 0.8 -> ~300 ms, como la norma VU), integrada con el
    // tiempo real entre cuadros: la aguja se mueve continua a cualquier refresco.
    const double now = juce::Time::getMillisecondCounterHiRes();
    float dt = (float) juce::jlimit (0.0, 0.1, (now - lastT) * 0.001);
    lastT = now;
    if (dt <= 0.0002f) return;
    const int meter = (int) std::lround (param (proc, "meter"));
    const bool power = meter < 3;
    const float lampPrev = lamp;
    lamp += ((power ? 1.0f : 0.0f) - lamp) * (1.0f - std::exp (-dt / (power ? 0.18f : 0.35f)));   // filamento: enciende y se apaga suave
    float tgt = 0.0f;
    if (power)
    {
        if (meter == 0) tgt = std::pow (10.0f, -proc.grDb.load() / 20.0f);                         // GR: 0 VU en reposo
        else tgt = proc.outAvg.load() * 1.5707963f / (meter == 1 ? 0.158489f : 0.1f);             // +8: 0 VU = -16 dBFS, +4: -20 dBFS
    }
    tgt = juce::jlimit (0.0f, 1.8f, tgt);
    const float prev = vuPos;
    tgtSm += (tgt - tgtSm) * (1.0f - std::exp (-dt / 0.012f));
    tgt = tgtSm;
    const int sub = juce::jlimit (2, 16, (int) std::ceil (dt / 0.002f));
    for (int i = 0; i < sub; ++i)
    {
        const float h = dt / (float) sub, wn = 13.0f, z = 0.8f;
        const float acc = wn * wn * (tgt - vuPos) - 2.0f * z * wn * vuVel;
        vuVel += acc * h; vuPos += vuVel * h;
        if (vuPos < -0.03f) { vuPos = -0.03f; vuVel = 0; }
        if (vuPos > 1.55f) { vuPos = 1.55f; vuVel = 0; }
    }
    if (std::abs (prev - vuPos) > 0.0002f || std::abs (lamp - lampPrev) > 0.001f) repaint (windowR.toNearestInt().expanded (2));
    if (std::abs (lamp - lampPrev) > 0.002f) { for (auto& k : ratioKeys) k.repaint(); for (auto& k : meterKeys) k.repaint(); repaint (lcdR.toNearestInt()); }
}

Top76Editor::Top76Editor (Top76Processor& p) : AudioProcessorEditor (p), content (p)
{
    setLookAndFeel (&look);
    addAndMakeVisible (content);
    content.setBounds (0, 0, W, H);
    setResizable (true, true);
    setResizeLimits (700, 180, W, H);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) W / (double) H);
    setSize (1120, 288);
    startTimerHz (60);
}
Top76Editor::~Top76Editor() { setLookAndFeel (nullptr); }
void Top76Editor::resized() { content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) W)); }
