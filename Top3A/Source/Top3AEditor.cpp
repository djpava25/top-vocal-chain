#include "Top3AEditor.h"
#include "T3AParts.h"
#include <map>

using namespace t3a;
using juce::Colour; namespace Colours = juce::Colours; using juce::Graphics; using juce::Path; using juce::Rectangle; using juce::ColourGradient;
using Pt = juce::Point<float>;

namespace
{
    juce::String U (const char* s) { return juce::String::fromUTF8 (s); }
    constexpr float pi = juce::MathConstants<float>::pi, deg = pi / 180.0f;
    float param (Top3AProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); }
    void setParam (Top3AProcessor& p, const char* id, float v)
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
    // texto centrado en (x, y) por su contorno real
    void textAt (Graphics& g, const juce::String& t, float x, float y, const juce::Font& f, float hs = 1.0f)
    {
        juce::GlyphArrangement ga; ga.addLineOfText (f.withHorizontalScale (hs), t, 0, 0);
        Path p; ga.createPath (p);
        const auto b = p.getBounds();
        p.applyTransform (juce::AffineTransform::translation (x - b.getCentreX(), y - b.getCentreY()));
        g.fillPath (p);
    }
    juce::String ch (int c) { return juce::String::charToString ((juce::juce_wchar) c); }

    // ------------------------------------------------------------------ geometria (frente de medio rack: 988 x 388)
    const Rectangle<float> panelR { 6, 6, 988, 388 }, trayR { 6, 402, 988, 92 };
    const Pt cGain { 165, 176 }, cPeak { 835, 176 }, cEmph { 726, 448 }, cMix { 902, 448 };
    constexpr float skirtR = 64.0f, bodyR = 46.0f, capR = 32.0f, dotsR = 77.0f, numsR = 94.0f;
    constexpr float sSkirtR = 32.0f, sBodyR = 23.0f, sCapR = 16.0f, sDotsR = 39.0f;
    constexpr float knobHalf = 72.0f, sKnobHalf = 37.0f;
    const Pt nutMeter { 160, 322 }, nutPower { 830, 322 }, nutMode { 486, 448 };
    constexpr float nutR = 15.0f;
    const Pt pilotC { 712, 290 };
    const Pt screws[] { { 44, 62 }, { 956, 62 }, { 44, 320 }, { 956, 320 } };
    // VU
    const Rectangle<float> bezelR { 306, 70, 388, 192 }, windowR { 320, 84, 360, 164 }, lipR { 318, 206, 364, 44 };
    const Pt pivot { 500, 466 };
    constexpr float arcR = 330.0f, swing = 28.0f, fullP = 1.4125375f;        // +3 VU = 1.4125 x (0 VU)
    float vuAngleP (float p) { return (-swing + 2.0f * swing * p / fullP) * deg; }
    float vuAngleDb (float db) { return vuAngleP (std::pow (10.0f, db / 20.0f)); }
    // bandeja
    const Rectangle<float> lcdR { 64, 430, 212, 40 }, keyPrevR { 26, 432, 32, 36 }, keyNextR { 282, 432, 32, 36 };

    // ------------------------------------------------------------------ serigrafia blanca del frente y de la bandeja
    void drawSilk (Graphics& g)
    {
        g.setColour (Colours::white);
        auto T = [&g] (const juce::String& t, float x, float y, float cap, bool bold = true, float hs = 1.0f)
        { textAt (g, t, x, y, sans (cap, bold), hs); if (! bold) { textAt (g, t, x + 0.45f, y, sans (cap, bold), hs); } };
        auto dot = [&g] (Pt c, float r) { g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2); };
        // ---- escalas 0-10 de las perillas grandes
        for (Pt c : { cGain, cPeak })
            for (int i = 0; i <= 10; ++i)
            {
                const float a = (-135.0f + 27.0f * (float) i) * deg;
                dot (polar (c, dotsR, a), 2.5f);
                const Pt q = polar (c, numsR + (i == 10 ? 4.0f : 0.0f), a);
                T (juce::String (i), q.x, q.y, 12.0f, false, i == 10 ? 0.95f : 1.0f);
            }
        T ("GAIN", cGain.x, 46, 12.5f, true, 1.04f);
        T ("PEAK", cPeak.x, 26, 12.5f, true, 1.04f);
        T ("REDUCTION", cPeak.x, 46, 12.5f, true, 1.04f);
        // ---- palancas
        T ("GR", 105, 322, 13.5f, true); T ("OUTPUT", 236, 322, 13.5f, true, 1.04f);
        T ("POWER", 750, 322, 13.5f, true, 1.04f); T ("ON", 889, 322, 13.5f, true);
        // ---- logotipo propio arriba al centro (circulo con HM) + firma
        g.drawEllipse (500 - 19.0f, 40 - 19.0f, 38, 38, 2.6f);
        T ("HM", 500, 40, 11.0f, true, 0.92f);
        T ("DJ PAVA", 556, 50, 7.0f, true, 1.0f);
        // ---- nombre propio debajo del medidor
        T ("TOP 3A", 500, 300, 21.0f, true, 1.08f);
        T ("OPTICAL LEVELER", 500, 334, 17.0f, true, 1.10f);
        T ("DJ PAVA HM MUSIC", 744, 372, 7.2f, true); T ("TOP VOCAL CHAIN", 892, 372, 7.2f, true);
        // ---- bandeja (panel trasero)
        T ("PRESET", lcdR.getCentreX(), 418, 8.5f, true, 1.1f);
        T ("COMP", 418, 448, 11.0f, true); T ("LIMIT", 556, 448, 11.0f, true);
        T ("HF EMPH", 650, 448, 10.0f, true, 1.04f); T ("MIX", 846, 448, 10.0f, true, 1.04f);
        for (Pt c : { cEmph, cMix })
            for (int i = 0; i <= 10; ++i) dot (polar (c, sDotsR, (-135.0f + 27.0f * (float) i) * deg), 1.6f);
        T ("0", cEmph.x - 32, 486, 7.5f, false); T ("10", cEmph.x + 33, 486, 7.5f, false);
        T ("DRY", cMix.x - 34, 486, 7.0f, true); T ("WET", cMix.x + 34, 486, 7.0f, true);
    }
    // ------------------------------------------------------------------ escala del VU (tinta negra / roja sobre la cara ambar)
    void drawFace (Graphics& g, bool red)
    {
        const float dbs[] { -20, -10, -7, -5, -3, -1, 0, 1, 2, 3 };
        const juce::String en = ch (0x2013);
        auto T = [&g] (const juce::String& t, float x, float y, float cap) { textAt (g, t, x, y, sans (cap, false), 0.92f); textAt (g, t, x + 0.3f, y, sans (cap, false), 0.92f); };
        g.setColour (Colours::white);
        // arco principal y zona roja
        if (! red)
        {
            Path arc; arc.addCentredArc (pivot.x, pivot.y, arcR, arcR, 0, vuAngleDb (-20) - 0.01f, vuAngleDb (0), true);
            g.strokePath (arc, juce::PathStrokeType (1.6f));
        }
        else
        {
            Path arc; arc.addCentredArc (pivot.x, pivot.y, arcR, arcR, 0, vuAngleDb (0), vuAngleDb (3) + 0.004f, true);
            g.strokePath (arc, juce::PathStrokeType (1.6f));
            Path band;                                                       // cuna roja bajo el arco, se ensancha hacia +3
            const int N = 24;
            for (int i = 0; i <= N; ++i) { const float t = (float) i / N, a = vuAngleDb (0) + t * (vuAngleDb (3) - vuAngleDb (0)); const Pt p = polar (pivot, arcR - 2.0f, a); if (i == 0) band.startNewSubPath (p); else band.lineTo (p); }
            for (int i = N; i >= 0; --i) { const float t = (float) i / N, a = vuAngleDb (0) + t * (vuAngleDb (3) - vuAngleDb (0)); band.lineTo (polar (pivot, arcR - 2.0f - (5.0f + 11.0f * t), a)); }
            band.closeSubPath(); g.fillPath (band);
        }
        // marcas grandes con numero y chicas entre medio
        for (float db : dbs)
        {
            if ((db > 0) != red) continue;
            const float a = vuAngleDb (db);
            g.drawLine ({ polar (pivot, arcR, a), polar (pivot, arcR + 14.0f, a) }, 1.7f);
            const Pt q = polar (pivot, arcR + 27.0f, a);
            const juce::String s = db > 0 ? "+" + juce::String ((int) db) : db < 0 ? en + juce::String ((int) -db) : juce::String ("0");
            T (s, q.x, q.y, 11.0f);
        }
        if (! red)
        {
            for (float db : { -15.0f, -6.0f, -4.0f, -2.0f, -0.5f }) { const float a = vuAngleDb (db); g.drawLine ({ polar (pivot, arcR, a), polar (pivot, arcR + 8.0f, a) }, 1.3f); }
            for (int i = 0; i <= 5; ++i)                                     // escala de porcentaje
            {
                const float p = 0.2f * (float) i, a = vuAngleP (p);
                const Pt q = polar (pivot, arcR - 15.0f, a);
                T (juce::String (i * 20), q.x, q.y, 9.0f);
            }
            T ("VU", 342, 102, 11.0f);
            // emblema propio pequeno en la cara
            g.drawEllipse (500 - 10.0f, 178 - 10.0f, 20, 20, 1.3f);
            textAt (g, "HM", 500, 178, sans (6.5f, true), 0.92f);
        }
        else
        {
            T ("VU", 658, 102, 11.0f);
            for (float db : { 0.5f, 1.5f, 2.5f }) { const float a = vuAngleDb (db); g.drawLine ({ polar (pivot, arcR, a), polar (pivot, arcR + 8.0f, a) }, 1.3f); }
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
        const auto panel = maskOf (iw, ih, k, [] (Graphics& g) { g.fillRoundedRectangle (panelR, 2.5f); }, 1);
        const auto tray = maskOf (iw, ih, k, [] (Graphics& g) { g.fillRoundedRectangle (trayR, 2.5f); }, 1);
        const auto silk = maskOf (iw, ih, k, [] (Graphics& g) { drawSilk (g); }, 0);
        const auto outer = maskOf (iw, ih, k, [] (Graphics& g) { g.fillRoundedRectangle (bezelR, 7.0f); }, 3);
        const auto win = maskOf (iw, ih, k, [] (Graphics& g) { g.fillRoundedRectangle (windowR, 3.0f); }, 2);
        const auto winSh = maskOf (iw, ih, k, [] (Graphics& g) { g.fillRoundedRectangle (windowR.translated (2.0f, 9.0f).reduced (4.0f, 0.0f), 3.0f); }, 9);
        const auto inkB = maskOf (iw, ih, k, [] (Graphics& g) { drawFace (g, false); }, 0);
        const auto inkR = maskOf (iw, ih, k, [] (Graphics& g) { drawFace (g, true); }, 0);
        const auto glass = maskOf (iw, ih, k, [] (Graphics& g) { g.fillRoundedRectangle (lcdR, 4.0f); }, 1);
        const auto paintM = t3p::panelPaint(), inkM = t3p::ink();
        const auto chassis = r3d::srgb (0.025f, 0.025f, 0.028f, 0, 0.3f);
        const float cx = (float) iw * 0.5f, cy = (float) ih * 0.5f;
        struct Sk { Pt c; float R, Rb; };
        const Sk skirts[] { { cGain, skirtR, bodyR }, { cPeak, skirtR, bodyR }, { cEmph, sSkirtR, sBodyR }, { cMix, sSkirtR, sBodyR } };
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
                // pintura negra satinada con un cepillado horizontal muy fino
                const float br = r3d::vnoise (px * 0.004f, py * 0.55f, 11), tone = 0.90f + 0.10f * r3d::fbm (px * 0.01f, py * 0.01f, 3, 3) + 0.10f * (br - 0.5f);
                r3d::Mat m = paintM; m.r *= tone; m.g *= tone; m.b *= tone;
                s.m = t3p::mixM (s.m, m, panel[i]);
                h += 4.0f * panel[i] + 0.10f * br;
            }
            if (tray[i] > 0)
            {
                const float tone = 0.94f + 0.06f * r3d::fbm (px * 0.02f, py * 0.02f, 5, 3) + 0.05f * (r3d::vnoise (px * 0.003f, py * 0.7f, 13) - 0.5f);
                r3d::Mat m = r3d::srgb (0.150f * tone, 0.155f * tone, 0.165f * tone, 0, 0.42f);
                s.m = t3p::mixM (s.m, m, tray[i]);
                h += 4.0f * tray[i];
            }
            if (silk[i] > 0) { s.m = t3p::mixM (s.m, inkM, silk[i]); h += 0.40f * silk[i]; }
            // marco del medidor (plastico negro en relieve) y la cara ambar hundida
            const float o = outer[i], w = win[i];
            if (o > 0)
            {
                const float ring = juce::jlimit (0.0f, 1.0f, o - w);
                h += 9.0f * o - 15.0f * w;
                s.m = t3p::mixM (s.m, t3p::plastic (0.035f, 0.035f, 0.038f, 0.62f), juce::jlimit (0.0f, 1.0f, o * 1.4f));
                if (w > 0)
                {
                    const float lit = 0.35f + 0.65f * winSh[i];                       // sombra interior del marco (mas arriba)
                    const float gx = (X - 500.0f) / 180.0f, gy = (Y - 150.0f) / 70.0f;
                    const float glow = (1.0f - 0.22f * gx * gx - 0.10f * gy * gy) * lit;
                    r3d::Mat face = r3d::srgb (0.96f, 0.82f, 0.55f, 0, 0.22f); face.f0 = 0.03f;
                    face.r *= lit; face.g *= lit; face.b *= lit;
                    face.er = 0.46f * glow; face.eg = 0.30f * glow; face.eb = 0.10f * glow;
                    const float tex = 0.97f + 0.03f * r3d::vnoise (px * 0.3f, py * 0.3f, 21);
                    face.r *= tex; face.g *= tex; face.b *= tex;
                    if (inkB[i] > 0) { r3d::Mat in = r3d::srgb (0.06f, 0.055f, 0.05f, 0, 0.3f); in.er = face.er * 0.10f; in.eg = face.eg * 0.10f; in.eb = face.eb * 0.10f; face = t3p::mixM (face, in, inkB[i]); }
                    if (inkR[i] > 0) { r3d::Mat in = r3d::srgb (0.80f, 0.10f, 0.07f, 0, 0.3f); in.er = face.er * 0.55f; in.eg = face.eg * 0.06f; in.eb = face.eb * 0.04f; face = t3p::mixM (face, in, inkR[i]); }
                    s.m = t3p::mixM (s.m, face, w);
                }
                (void) ring;
            }
            if (glass[i] > 0) { auto gm = r3d::srgb (0.010f, 0.010f, 0.011f, 0, 0.95f); gm.f0 = 0.06f; s.m = t3p::mixM (s.m, gm, glass[i]); h -= 3.0f * glass[i]; }
            // faldas de acrilico ahumado
            for (const auto& sk : skirts)
            {
                const float dx = (X - sk.c.x) * k, dy = (Y - sk.c.y) * k, R = sk.R * k;
                if (dx * dx + dy * dy < (R + 2) * (R + 2)) { const auto q = t3p::smokeSkirt (dx, dy, R, sk.Rb * k); s.m = t3p::mixM (s.m, q.m, q.m.alpha); h += q.h * q.m.alpha; }
                else if (dx * dx + dy * dy < (R * 1.25f) * (R * 1.25f)) { const float rr = std::sqrt (dx * dx + dy * dy), ao = 1.0f - 0.35f * std::exp (-std::pow ((rr - R) / (0.10f * R), 2.0f)); s.m.r *= ao; s.m.g *= ao; s.m.b *= ao; }
            }
            // tuercas de las palancas
            for (Pt c : { nutMeter, nutPower, nutMode })
            {
                const float dx = (X - c.x) * k, dy = (Y - c.y) * k, R = nutR * k, rr = std::sqrt (dx * dx + dy * dy);
                if (rr < R + 1) { const auto q = t3p::hexNut (dx, dy, R); if (q.m.alpha > 0) { s.m = t3p::mixM (s.m, q.m, q.m.alpha); h += q.h * q.m.alpha; } }
                if (rr < R * 1.6f) { const float ao = 1.0f - 0.45f * std::exp (-std::pow ((rr - 0.95f * R) / (0.25f * R), 2.0f)); s.m.r *= ao; s.m.g *= ao; s.m.b *= ao; }
            }
            for (Pt c : screws)
            {
                const float dx = (X - c.x) * k, dy = (Y - c.y) * k, R = 11.0f * k, rr = std::sqrt (dx * dx + dy * dy);
                if (rr < R + 1) { const auto q = t3p::phillips (dx, dy, R); s.m = t3p::mixM (s.m, q.m, q.m.alpha); h += q.h * q.m.alpha; }
                else if (rr < R * 1.7f) { const float ao = 1.0f - 0.45f * std::exp (-std::pow ((rr - R) / (0.3f * R), 2.0f)); s.m.r *= ao; s.m.g *= ao; s.m.b *= ao; }
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
            const float X = (rx + cx) / k, Y = (ry + cy) / k;                  // en coordenadas del rectangulo del labio
            r3d::Sample s; s.m = t3p::plastic (0.040f, 0.040f, 0.044f, 0.55f);
            const float top = Y - 2.0f;
            s.m.alpha = juce::jlimit (0.0f, 1.0f, top * k + 0.5f) * juce::jlimit (0.0f, 1.0f, (X - 2.0f) * k + 0.5f) * juce::jlimit (0.0f, 1.0f, (lipR.getWidth() - 2.0f - X) * k + 0.5f);
            float h = 10.0f * r3d::fillet (top, 5.0f);
            const float tex = 0.92f + 0.08f * r3d::vnoise (rx * 0.2f, ry * 0.2f, 31);
            s.m.r *= tex; s.m.g *= tex; s.m.b *= tex;
            const float dx = (X - 182.0f) * k, dy = (Y - 22.0f) * k, R = 8.0f * k, rr = std::sqrt (dx * dx + dy * dy);
            if (rr < R + 1)
            {
                // tornillo de ajuste del cero (ranura)
                const float u = rr / R;
                r3d::Mat sm = t3p::plastic (0.06f, 0.06f, 0.065f, 0.7f);
                float hs = 10.0f + 0.25f * R * std::sqrt (std::max (0.0f, 1.0f - u * u));
                if (std::abs (dx * 0.5f + dy * 0.866f) < 0.12f * R && rr < 0.8f * R) { hs -= 0.18f * R; sm = t3p::plastic (0.01f, 0.01f, 0.01f, 0.2f); }
                const float a = juce::jlimit (0.0f, 1.0f, R + 0.5f - rr);
                s.m = t3p::mixM (s.m, sm, a); h = h * (1 - a) + hs * a;
            }
            s.h = h; return s;
        }, { 0, 3.0f * k, 4.0f * k, 0.45f, 0, 0 });
        return cached;
    }

    // ------------------------------------------------------------------ imagenes de piezas (cache)
    std::map<juce::String, juce::Image>& cache() { static std::map<juce::String, juce::Image> c; return c; }
    const juce::Image& bodyImage (bool small, int px)
    {
        const juce::String key = juce::String (small ? "s" : "b") + juce::String (px);
        auto& c = cache();
        auto it = c.find (key);
        if (it != c.end()) return it->second;
        const float half = small ? sKnobHalf : knobHalf, sc = (float) px / (2.0f * half);
        const float R = (small ? sBodyR : bodyR) * sc, Rc = (small ? sCapR : capR) * sc;
        auto img = r3d::render (px, px, [=] (float x, float y) { return t3p::knobBody (x, y, R, Rc, small ? 40 : 60, 0.62f * R, -0.13f * R, -0.15f * R); },
                                { 0.06f * R, 0.12f * R, 0.14f * R, 0.62f, 0.05f * R, 0.5f });
        return c[key] = img;
    }
    const juce::Image& leverImage (bool right, int px)
    {
        const juce::String key = juce::String (right ? "R" : "L") + juce::String (px);
        auto& c = cache();
        auto it = c.find (key);
        if (it != c.end()) return it->second;
        const float sc = (float) px / 64.0f, R = nutR * sc;
        auto img = r3d::render (px, px, [=] (float x, float y) { return t3p::batLever (x, y, R, right ? 1.0f : -1.0f); }, { 0.10f * R * (right ? 1.0f : -1.0f), 0.30f * R, 0.20f * R, 0.55f, 0, 0 });
        return c[key] = img;
    }
    const juce::Image& pilotImage (bool on, int px)
    {
        const juce::String key = juce::String (on ? "P1_" : "P0_") + juce::String (px);
        auto& c = cache();
        auto it = c.find (key);
        if (it != c.end()) return it->second;
        const float R = (float) px * 0.36f;
        auto img = r3d::render (px, px, [=] (float x, float y) { return t3p::pilot (x, y, R, on); }, { 0, 0.15f * R, 0.25f * R, 0.5f, 0, 0 });
        return c[key] = img;
    }
    const juce::Image& keyImage (int w, int h, bool pressed)
    {
        const juce::String key = "K" + juce::String (w) + "x" + juce::String (h) + (pressed ? "d" : "u");
        auto& c = cache();
        auto it = c.find (key);
        if (it != c.end()) return it->second;
        auto img = r3d::render (w, h, [=] (float x, float y) { return t3p::squareKey (x, y, (float) w * 0.5f - 3.0f, (float) h * 0.5f - 3.0f, pressed); }, { 0, 2.0f, 3.0f, 0.5f, 2.0f, 0.4f });
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

Knob::Knob (APVTS& s, const juce::String& id, bool sm) : small (sm)
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (-135.0f * deg, 135.0f * deg, true);
    setMouseDragSensitivity (260);
    setPopupDisplayEnabled (true, true, nullptr, 900);
    setVelocityBasedMode (false);
    if (auto* p = s.getParameter (id)) setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    att = std::make_unique<APVTS::SliderAttachment> (s, id, *this);
}
float Knob::angleRad() const
{
    const auto rp = getRotaryParameters();
    const float pos = (float) ((getValue() - getMinimum()) / (getMaximum() - getMinimum()));
    return rp.startAngleRadians + pos * (rp.endAngleRadians - rp.startAngleRadians);
}
bool Knob::hitTest (int x, int y)
{
    const float half = (float) getWidth() * 0.5f, dx = (float) x - half, dy = (float) y - half;
    const float R = small ? sSkirtR : skirtR;
    return dx * dx + dy * dy < (R + 2) * (R + 2);
}
void Knob::paint (Graphics& g)
{
    const float S = (float) getWidth(), half = S * 0.5f;
    const float a = angleRad();
    const Pt c (half, half);
    const float Rb = small ? sBodyR : bodyR, Rs = small ? sSkirtR : skirtR;
    // raya blanca del indicador, impresa dentro de la falda transparente
    const float w = small ? 2.2f : 3.4f;
    g.setColour (Colours::black.withAlpha (0.45f));
    g.drawLine ({ polar (c, Rb - 4.0f, a).translated (0.8f, 1.2f), polar (c, Rs - 2.5f, a).translated (0.8f, 1.2f) }, w + 0.6f);
    g.setColour (Colour (0xfff2f1ec));
    g.drawLine ({ polar (c, Rb - 4.0f, a), polar (c, Rs - 2.5f, a) }, w);
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (bodyImage (small, juce::roundToInt (S * 2.0f)), Rectangle<float> (0, 0, S, S));
}

void Toggle::paint (Graphics& g)
{
    const bool right = isRight != nullptr && isRight();
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (leverImage (right, getWidth() * 2), getLocalBounds().toFloat());
}

void Key::paint (Graphics& g)
{
    const int w = getWidth(), h = getHeight();
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (keyImage (w * 2, h * 2, held), getLocalBounds().toFloat());
    g.setColour (Colour (0xffd8d4cc));
    const float cx = (float) w * 0.5f, cy = (float) h * 0.5f + (held ? 1.0f : 0.0f), s = 5.5f;
    Path p;
    if (left) p.addTriangle (cx - s, cy, cx + s * 0.7f, cy - s, cx + s * 0.7f, cy + s);
    else p.addTriangle (cx + s, cy, cx - s * 0.7f, cy - s, cx - s * 0.7f, cy + s);
    g.fillPath (p);
}

// =====================================================================================================
Content::Content (Top3AProcessor& p)
    : proc (p), gain (p.apvts, "gain", false), peak (p.apvts, "peak", false), emph (p.apvts, "emph", true), mix (p.apvts, "mix", true)
{
    plate = renderPlate();
    cover = renderCover();
    for (juce::Component* c : std::initializer_list<juce::Component*> { &gain, &peak, &emph, &mix, &meterSw, &powerSw, &modeSw, &prevKey, &nextKey })
        addAndMakeVisible (c);
    auto flip = [this] (const char* id) { setParam (proc, id, param (proc, id) > 0.5f ? 0.0f : 1.0f); repaint(); };
    meterSw.isRight = [this] { return param (proc, "meter") > 0.5f; }; meterSw.onClick = [flip] { flip ("meter"); };
    powerSw.isRight = [this] { return param (proc, "power") > 0.5f; }; powerSw.onClick = [flip] { flip ("power"); };
    modeSw.isRight = [this] { return param (proc, "mode") > 0.5f; }; modeSw.onClick = [flip] { flip ("mode"); };
    prevKey.left = true; nextKey.left = false;
    prevKey.onClick = [this] { const int n = proc.getNumPrograms(); proc.setCurrentProgram ((proc.getCurrentProgram() + n - 1) % n); repaint(); };
    nextKey.onClick = [this] { const int n = proc.getNumPrograms(); proc.setCurrentProgram ((proc.getCurrentProgram() + 1) % n); repaint(); };
    gain.setTooltip (U ("GAIN: volumen de salida (hasta +30 dB, unidad cerca de 3.7). No cambia la compresión"));
    peak.setTooltip (U ("PEAK REDUCTION: cuánta señal llega a la celda óptica = cuánta compresión"));
    emph.setTooltip (U ("HF EMPH: la cadena lateral escucha más los agudos (hasta ~10 dB más de reducción en 15 kHz)"));
    mix.setTooltip (U ("MIX: mezcla en paralelo (seco / comprimido)"));
    meterSw.setTooltip (U ("Medidor: GR (reducción de ganancia) u OUTPUT (nivel de salida, 0 VU = -20 dBFS)"));
    powerSw.setTooltip (U ("POWER: encender / apagar el procesador"));
    modeSw.setTooltip (U ("COMP: compresión suave (~3:1). LIMIT: la relación sube con la compresión fuerte"));
    prevKey.setTooltip (U ("Preset anterior")); nextKey.setTooltip (U ("Preset siguiente"));
    lastT = juce::Time::getMillisecondCounterHiRes();
}

void Content::resized()
{
    auto place = [] (juce::Component& c, Pt ctr, float hb) { c.setBounds (juce::roundToInt (ctr.x - hb), juce::roundToInt (ctr.y - hb), juce::roundToInt (hb * 2), juce::roundToInt (hb * 2)); };
    place (gain, cGain, knobHalf); place (peak, cPeak, knobHalf);
    place (emph, cEmph, sKnobHalf); place (mix, cMix, sKnobHalf);
    place (meterSw, nutMeter, 32); place (powerSw, nutPower, 32); place (modeSw, nutMode, 32);
    prevKey.setBounds (keyPrevR.toNearestInt()); nextKey.setBounds (keyNextR.toNearestInt());
}

void Content::paint (Graphics& g)
{
    g.fillAll (Colour (0xff0b0b0c));
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (plate, Rectangle<float> (0, 0, (float) W, (float) H));
    const bool power = param (proc, "power") > 0.5f;
    // ---- aguja del VU (detras del labio inferior y del vidrio)
    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (windowR.toNearestInt());
        const float a = vuAngleP (vuPos);
        const Pt tip = polar (pivot, arcR + 19.0f, a), base = polar (pivot, 150.0f, a);
        for (int i = 3; i >= 1; --i)
        {
            g.setColour (Colour (0xff3a2208).withAlpha (0.045f * (float) i));
            g.drawLine ({ base.translated (2.5f, 3.5f), tip.translated (2.5f, 3.5f) }, 0.8f + 1.1f * (float) i);
        }
        Path nd;
        const Pt n (std::cos (a), std::sin (a));
        nd.startNewSubPath (base + n * 1.4f); nd.lineTo (tip + n * 0.55f); nd.lineTo (tip - n * 0.55f); nd.lineTo (base - n * 1.4f); nd.closeSubPath();
        g.setColour (Colour (0xff0d0c0b)); g.fillPath (nd);
        g.drawImage (cover, lipR);
        // vidrio: reflejo diagonal suave y canto superior
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.10f), windowR.getX(), windowR.getY(), Colours::white.withAlpha (0.0f), windowR.getX() + 170, windowR.getY() + 120, false));
        g.fillRect (windowR);
        g.setColour (Colours::white.withAlpha (0.10f)); g.fillRect (windowR.getX(), windowR.getY(), windowR.getWidth(), 1.2f);
    }
    // ---- piloto de POWER
    g.drawImage (pilotImage (power, 48), Rectangle<float> (pilotC.x - 12, pilotC.y - 12, 24, 24));
    if (power)
    {
        g.setGradientFill (ColourGradient (Colour (0x60ffb040), pilotC.x, pilotC.y, Colour (0x00ffb040), pilotC.x + 22, pilotC.y, true));
        g.fillEllipse (pilotC.x - 22, pilotC.y - 22, 44, 44);
    }
    // ---- nombre del preset (pantalla ambar)
    g.setColour (Colour (0xffffb347));
    g.setFont (sans (10.0f, true));
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
    // ---- VU: bobina movil de 2do orden (wn 13 rad/s, amortiguamiento 0.8) sobre la posicion lineal de la escala
    const double now = juce::Time::getMillisecondCounterHiRes();
    float dt = (float) juce::jlimit (0.0, 0.1, (now - lastT) * 0.001);
    if (dt <= 0.0f) dt = 1.0f / 60.0f;
    lastT = now;
    const bool power = param (proc, "power") > 0.5f;
    float tgt = 0.0f;                                                       // apagado: la aguja cae al reposo
    if (power)
    {
        if (param (proc, "meter") > 0.5f) tgt = proc.outAvg.load() * 1.5707963f / 0.1f;           // promedio rectificado, 0 VU = -20 dBFS
        else tgt = std::pow (10.0f, -proc.grDb.load() / 20.0f);                                     // GR: 0 VU sin reduccion
    }
    tgt = juce::jlimit (0.0f, 1.8f, tgt);
    const float prev = vuPos;
    const int sub = 8;
    for (int i = 0; i < sub; ++i)
    {
        const float h = dt / (float) sub, wn = 13.0f, z = 0.8f;
        const float acc = wn * wn * (tgt - vuPos) - 2.0f * z * wn * vuVel;
        vuVel += acc * h; vuPos += vuVel * h;
        if (vuPos < -0.03f) { vuPos = -0.03f; vuVel = 0; }                   // tope izquierdo
        if (vuPos > 1.55f) { vuPos = 1.55f; vuVel = 0; }                     // tope derecho
    }
    vuShown = vuPos > 0.0f ? 20.0f * std::log10 (vuPos) : -60.0f;
    if (std::abs (prev - vuPos) > 0.0004f) repaint (windowR.toNearestInt().expanded (2));
    const int bits = (param (proc, "power") > 0.5f ? 1 : 0) + (param (proc, "meter") > 0.5f ? 2 : 0) + (param (proc, "mode") > 0.5f ? 4 : 0) + 8 * proc.getCurrentProgram();
    if (bits != lastBits) { lastBits = bits; repaint(); }
}

Top3AEditor::Top3AEditor (Top3AProcessor& p) : AudioProcessorEditor (p), content (p)
{
    setLookAndFeel (&look);
    addAndMakeVisible (content);
    content.setBounds (0, 0, W, H);
    setResizable (true, true);
    setResizeLimits (500, 250, W, H);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) W / (double) H);
    setSize (780, 390);
    startTimerHz (60);
}
Top3AEditor::~Top3AEditor() { setLookAndFeel (nullptr); }
void Top3AEditor::resized() { content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) W)); }
