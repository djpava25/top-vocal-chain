#include "VbxEditor.h"
#include "VbxParts.h"

using namespace vbui;
using juce::Colour; namespace Colours = juce::Colours; using juce::Graphics; using juce::Path; using juce::Rectangle; using juce::ColourGradient;
using Pt = juce::Point<float>;

namespace
{
    juce::String U (const char* s) { return juce::String::fromUTF8 (s); }
    juce::Font font (float h, bool bold = false) { return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain)); }
    constexpr float pi = juce::MathConstants<float>::pi, deg = pi / 180.0f;
    float param (VbxProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); }
    void setParam (VbxProcessor& p, const char* id, float v)
    { if (auto* q = p.apvts.getParameter (id)) { q->beginChangeGesture(); q->setValueNotifyingHost (q->convertTo0to1 (v)); q->endChangeGesture(); } }
    Pt polar (Pt c, float r, float a) { return { c.x + r * std::sin (a), c.y - r * std::cos (a) }; }

    // todas las medidas estan tomadas de la foto frontal del original (2576 px de ancho) y se escalan
    constexpr float S = (float) Content::strip;
    constexpr float sc = (float) Content::W / 2576.0f;
    Pt P (float x, float y) { return { x * sc, S + y * sc }; }
    float L (float v) { return v * sc; }

    // perillas: centro y radio en la foto
    struct KP { float x, y, r; };
    const KP kInput { 336, 580, 44 }, kGain { 605, 590, 46 }, kMeter { 805, 585, 33 }, kThr { 1287, 385, 44 }, kAtt { 1010, 592, 46 }, kRel { 1288, 592, 46 },
             kDsF { 1697, 155, 46 }, kDsT { 1937, 160, 44 }, kLow { 1695, 385, 34 }, kMid { 1935, 385, 34 }, kHi { 2170, 385, 34 },
             kLowF { 1695, 595, 30 }, kMidF { 1932, 595, 30 }, kHiF { 2175, 595, 30 };
    constexpr float kComp = 1.45f;                             // componente mas grande que la perilla (sombra)
    std::vector<float> stepsFrom (float a0, float step, int n) { std::vector<float> v; for (int i = 0; i < n; ++i) v.push_back (a0 + step * (float) i); return v; }
    const std::vector<float> aGain = stepsFrom (-150, 30, 5), aAtt = stepsFrom (30, 30, 5), aRel = stepsFrom (-30, -30, 5),
                             aMeter { -78, -48, 0, 48, 78 }, aDs = stepsFrom (-64, 32, 5), aFreq = stepsFrom (-150, 30, 11);

    // VU: angulos de la escala medidos en la foto (dB -> grados), pivote de la aguja
    const float vuDb[] { -20, -10, -7, -5, -3, -2, -1, 0, 1, 2, 3 };
    const float vuAng[] { -39, -27.5f, -19.4f, -12.3f, -3.4f, 2.2f, 8.6f, 16.9f, 24.3f, 34.8f, 43 };
    float vuAngle (float db)
    {
        if (db <= vuDb[0]) return vuAng[0] - (vuDb[0] - db) * 0.6f;
        for (int i = 1; i < 11; ++i) if (db <= vuDb[i]) { const float t = (db - vuDb[i - 1]) / (vuDb[i] - vuDb[i - 1]); return vuAng[i - 1] + t * (vuAng[i] - vuAng[i - 1]); }
        return vuAng[10] + (db - 3.0f) * 4.0f;
    }
    const Pt vuPivot { 810, 330 };
    Path vuWindow()     // en coordenadas de la foto
    {
        Path p;
        auto top = [] (float x) { return 543.0f - std::sqrt (448.0f * 448.0f - (x - 810.0f) * (x - 810.0f)); };
        auto bot = [] (float x) { return 815.0f - std::sqrt (600.0f * 600.0f - (x - 810.0f) * (x - 810.0f)); };
        p.startNewSubPath (628, top (628));
        for (float x = 640; x <= 992; x += 12) p.lineTo (x, top (x));
        p.lineTo (992, top (992)); p.lineTo (975, bot (975));
        for (float x = 963; x >= 645; x -= 12) p.lineTo (x, bot (x));
        p.lineTo (645, bot (645)); p.closeSubPath();
        return p.createPathWithRoundedCorners (16.0f);
    }
    juce::AffineTransform photoToContent() { return juce::AffineTransform::scale (sc).translated (0, S); }

    // ------------------------------------------------------------------ serigrafia blanca (placas y panel)
    void drawSilk (Graphics& g)
    {
        auto txt = [&g] (const juce::String& t, float x, float y, float h, juce::Justification j = juce::Justification::centred)
        {
            g.setFont (font (L (h), true).withHorizontalScale (0.86f));
            const float w = 400;
            Rectangle<float> r (j == juce::Justification::centredRight ? P (x, y).x - w : j == juce::Justification::centredLeft ? P (x, y).x : P (x, y).x - w * 0.5f, P (x, y).y - L (h), w, L (h) * 2);
            g.drawText (t, r, j);
        };
        auto line = [&g] (Pt a, Pt b, float w = 2.4f) { g.drawLine ({ P (a.x, a.y), P (b.x, b.y) }, L (w)); };
        g.setColour (Colours::white);
        // placa izquierda
        txt ("48V ON", 340, 67, 22); txt ("PHANTOM", 340, 160, 22);
        line ({ 340, 185 }, { 340, 236 });
        line ({ 405, 208 }, { 425, 190 }); line ({ 425, 190 }, { 458, 190 });
        txt ("120", 470, 222, 24); txt ("80", 470, 255, 24); txt ("FLAT", 470, 289, 24);
        txt ("INSTR", 432, 384, 19); txt (U ("0°"), 547, 350, 24); txt ("LINE", 547, 384, 24); txt (U ("180°"), 547, 418, 24);
        txt ("INPUT", 336, 497, 24); txt ("dB", 518, 497, 24); txt ("GAIN", 605, 497, 24);
        // escalas de abanico (arco + 7 rayas hacia adentro)
        auto fan = [&g] (const KP& k, float ra, float ri)
        {
            Path arc; arc.addCentredArc (P (k.x, k.y).x, P (k.x, k.y).y, L (ra), L (ra), 0, -135.0f * deg, 135.0f * deg, true);
            g.strokePath (arc, juce::PathStrokeType (L (2.4f)));
            for (int i = 0; i < 7; ++i) { const float a = (-135.0f + 45.0f * (float) i) * deg; g.drawLine ({ polar (P (k.x, k.y), L (ri), a), polar (P (k.x, k.y), L (ra), a) }, L (2.4f)); }
        };
        fan (kInput, 68, 46); fan (kThr, 68, 46); fan (kDsT, 68, 46); fan (kLow, 56, 37); fan (kMid, 56, 37); fan (kHi, 56, 37);
        // lineas guia de los selectores: de la etiqueta al codo y del codo al borde de la perilla
        auto guide = [&g] (const KP& k, float angDeg, Pt anchor)
        {
            const Pt c = P (k.x, k.y), e1 = polar (c, L (k.r * 1.12f), angDeg * deg), a = P (anchor.x, anchor.y);
            const float dirx = e1.x > a.x ? 1.0f : -1.0f;
            const Pt e2 (a.x + dirx * L (20.0f), a.y);                     // tramo horizontal y luego diagonal
            g.drawLine ({ a, e2 }, L (2.2f)); g.drawLine ({ e2, e1 }, L (2.2f));
        };
        const char* gl[] { "40", "45", "50", "55", "60" }; const float gy[] { 655, 622, 589, 556, 523 };
        for (int i = 0; i < 5; ++i) { txt (gl[i], 548, gy[i], 24, juce::Justification::centredRight); guide (kGain, aGain[(size_t) i], { 553, gy[i] }); }
        // medidor
        txt ("LINE IN", 776, 546, 17, juce::Justification::centredRight); txt ("G-R", 762, 566, 17, juce::Justification::centredRight);
        txt ("PREOUT", 810, 527, 17); txt ("EQ OUT", 856, 546, 17, juce::Justification::centredLeft); txt ("D-S", 866, 566, 17, juce::Justification::centredLeft);
        guide (kMeter, aMeter[0], { 766, 566 }); guide (kMeter, aMeter[1], { 780, 546 }); g.drawLine ({ P (810, 537), polar (P (kMeter.x, kMeter.y), L (kMeter.r * 1.12f), 0) }, L (2.2f));
        guide (kMeter, aMeter[3], { 852, 546 }); guide (kMeter, aMeter[4], { 862, 566 });
        // placa central
        txt ("LINK", 1288, 72, 22); txt ("SEPARATE", 1288, 162, 22);
        txt ("COMPRESS 3:1", 1150, 347, 21); txt ("BYPASS", 1135, 432, 22);
        txt ("THRESHOLD", 1290, 300, 24); txt ("ATTACK", 1030, 502, 24); txt ("RELEASE", 1288, 502, 24);
        const char* ar[] { "FAST", "MED FAST", "MEDIUM", "MED SLOW", "SLOW" }; const float ay[] { 523, 557, 591, 625, 659 };
        for (int i = 0; i < 5; ++i)
        {
            txt (ar[i], 1153, ay[i], 23);
            const float half = juce::GlyphArrangement::getStringWidth (font (L (23), true).withHorizontalScale (0.86f), ar[i]) * 0.5f / sc;
            guide (kAtt, aAtt[(size_t) i], { 1153 - half - 12, ay[i] }); guide (kRel, aRel[(size_t) i], { 1153 + half + 12, ay[i] });
        }
        // de-esser
        txt ("DE ESS", 1520, 113, 22); txt ("BYPASS", 1520, 198, 22); txt ("THRESHOLD", 1930, 67, 24);
        const char* dl[] { "3K", "6K", "9K", "12K" }; const Pt dp[] { { 1603, 113 }, { 1632, 75 }, { 1693, 55 }, { 1752, 75 } };
        for (int i = 0; i < 4; ++i) txt (dl[i], dp[i].x, dp[i].y, 22);
        txt ("LIMIT", 1795, 108, 19); txt ("10:1", 1795, 128, 19);
        for (int i = 0; i < 5; ++i) { const Pt c = P (kDsF.x, kDsF.y); const float a = aDs[(size_t) i] * deg; g.drawLine ({ polar (c, L (kDsF.r * 1.10f), a), polar (c, L (kDsF.r * 1.55f), a) }, L (2.4f)); }
        // EQ
        txt ("EQ IN", 1525, 346, 21); txt ("BYPASS", 1525, 432, 22);
        txt ("LOW PEAK", 1693, 322, 24); txt ("MID DIP", 1932, 322, 24); txt ("HI PEAK", 2150, 322, 24);
        txt ("0dB", 1652, 460, 21); txt ("+10", 1735, 460, 21); txt ("0dB", 1892, 460, 21); txt ("-10", 1972, 460, 21); txt ("0dB", 2129, 460, 21); txt ("+10", 2210, 460, 21);
        txt ("EQ INPUT", 1522, 528, 21); txt ("INSERT", 1522, 551, 21); txt ("PRE", 1473, 580, 21); txt ("O/P", 1473, 603, 21); txt ("LINE IN", 1522, 636, 19);
        auto freqs = [&] (const KP& k, float leftEdge, float rightEdge, std::initializer_list<const char*> labels)
        {
            const float rows[] { 645, 620, 592, 568, 545 };
            int i = 0;
            for (auto* l : labels)
            {
                if (i < 5) { txt (l, leftEdge, rows[i], 22, juce::Justification::centredRight); guide (k, aFreq[(size_t) i], { leftEdge + 5, rows[i] }); }
                else if (i == 5) { txt (l, k.x, 532, 22); }
                else { const float y = rows[10 - i]; txt (l, rightEdge, y, 22, juce::Justification::centredLeft); guide (k, aFreq[(size_t) i], { rightEdge - 5, y }); }
                ++i;
            }
        };
        freqs (kLowF, 1640, 1748, { "20", "35", "50", "70", "100", "150", "200", "300", "500", "700", "1000" });
        freqs (kMidF, 1876, 1985, { "200", "300", "500", "700", "1000", "1K5", "2K", "3K", "4K", "5K", "7K" });
        freqs (kHiF, 2113, 2222, { "1K5", "2K", "3K", "4K", "5K", "6K4", "8K", "10K", "12K", "16K", "20K" });
    }
    // logotipo grabado (contorno de letras con serifa, como un grabado a buril)
    void drawEngrave (Graphics& g)
    {
        auto word = [&g] (const juce::String& t, float cx, float cy, float h)
        {
            juce::GlyphArrangement ga;
            const auto f = juce::Font (juce::FontOptions (juce::Font::getDefaultSerifFontName(), L (h), juce::Font::plain)).withHorizontalScale (0.78f).withExtraKerningFactor (0.12f);
            ga.addLineOfText (f, t, 0, 0);
            Path p; ga.createPath (p);
            const auto b = p.getBounds();
            p.applyTransform (juce::AffineTransform::translation (P (cx, cy).x - b.getCentreX(), P (cx, cy).y - b.getCentreY()));
            g.strokePath (p, juce::PathStrokeType (L (1.25f)));
        };
        g.setColour (Colours::white);
        word ("TOP", 812, 314, 112);
        word ("6961P", 812, 406, 112);
    }

    // ------------------------------------------------------------------ placa de aluminio (se calcula una vez)
    std::vector<float> blur (const juce::Image& m, int w, int h, int r)
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
    juce::Image renderPlate()
    {
        static juce::Image cached; static juce::SpinLock lock;
        const juce::SpinLock::ScopedLockType sl (lock);
        if (cached.isValid()) return cached;
        constexpr float k = 1.5f;
        const int iw = (int) ((float) Content::W * k), ih = (int) ((float) (Content::H - Content::strip) * k);
        const auto toImg = juce::AffineTransform::translation (0, -S).scaled (k);
        auto newMask = [iw, ih] { return juce::Image (juce::Image::SingleChannel, iw, ih, true); };
        juce::Image plates = newMask(), silk = newMask(), eng = newMask(), win = newMask(), holes = newMask();
        {
            Graphics g (plates); g.addTransform (photoToContent().followedBy (toImg)); g.setColour (Colours::white);
            Path a; a.startNewSubPath (240, 30); a.lineTo (382, 30); a.lineTo (742, 672); a.lineTo (240, 672); a.closeSubPath(); g.fillPath (a.createPathWithRoundedCorners (18));
            Path b; b.startNewSubPath (885, 672); b.lineTo (1232, 30); b.lineTo (1390, 30); b.lineTo (1390, 672); b.closeSubPath(); g.fillPath (b.createPathWithRoundedCorners (18));
            g.fillRoundedRectangle (1440, 32, 520, 238, 18); g.fillEllipse (1940 - 119, 151 - 119, 238, 238);
            g.fillRoundedRectangle (1440, 305, 760, 370, 18); g.fillRoundedRectangle (2150, 455, 190, 220, 18); g.fillEllipse (2180 - 160, 465 - 160, 320, 320);
        }
        { Graphics g (silk); g.addTransform (toImg); drawSilk (g); }
        { Graphics g (eng); g.addTransform (toImg); drawEngrave (g); }
        { Graphics g (win); g.addTransform (photoToContent().followedBy (toImg)); g.setColour (Colours::white); g.fillPath (vuWindow()); }
        {
            Graphics g (holes); g.addTransform (photoToContent().followedBy (toImg)); g.setColour (Colours::white);
            for (Pt c : { Pt { 40, 32 }, Pt { 40, 680 }, Pt { 2536, 32 }, Pt { 2536, 680 } }) g.fillRoundedRectangle (c.x - 30, c.y - 17, 60, 34, 17);
        }
        const auto plateB = blur (plates, iw, ih, 2), plateSharp = blur (plates, iw, ih, 0), winB = blur (win, iw, ih, 5), silkA = blur (silk, iw, ih, 0),
                   engA = blur (eng, iw, ih, 0), holeA = blur (holes, iw, ih, 0);
        struct Scr { float x, y, r; };
        std::vector<Scr> screws;
        for (float y : { 72.0f, 160.0f, 302.0f, 393.0f, 535.0f, 627.0f }) screws.push_back ({ 185, y, 13 });
        for (float y : { 78.0f, 168.0f, 310.0f, 400.0f, 545.0f, 632.0f }) screws.push_back ({ 2390, y, 13 });
        for (Pt c : { Pt { 255, 47 }, Pt { 253, 657 }, Pt { 720, 657 }, Pt { 1368, 52 }, Pt { 895, 657 }, Pt { 1368, 657 }, Pt { 1460, 47 }, Pt { 1460, 243 }, Pt { 1810, 52 },
                      Pt { 1810, 245 }, Pt { 1460, 323 }, Pt { 1460, 657 }, Pt { 2050, 325 }, Pt { 2050, 657 } }) screws.push_back ({ c.x, c.y, 10 });
        const auto blue = r3d::srgb (122.0f / 255.0f, 144.0f / 255.0f, 172.0f / 255.0f, 0, 0.55f);
        auto black = r3d::srgb (0.15f, 0.145f, 0.15f, 0, 0.45f); black.f0 = 0.04f;
        const auto ink = r3d::srgb (0.93f, 0.93f, 0.92f, 0, 0.4f);
        const float pk = sc * k;                                            // pixeles de imagen por pixel de la foto
        const float cxI = (float) iw * 0.5f, cyI = (float) ih * 0.5f;
        auto fn = [&] (float rx, float ry)
        {
            const float px = rx + cxI, py = ry + cyI;
            const int ix = juce::jlimit (0, iw - 1, (int) px), iy = juce::jlimit (0, ih - 1, (int) py);
            const size_t i = (size_t) (iy * iw + ix);
            r3d::Sample s; s.m = blue;
            // aluminio cepillado en horizontal (rayas largas y finas)
            const float streak = r3d::fbm (px * 0.004f, py * 0.9f, 1, 3), fine = r3d::fbm (px * 0.015f, py * 3.0f, 2, 2);
            const float hair = r3d::vnoise (px * 0.02f, py * 4.0f, 9);                     // lineas finas del cepillado
            float tone = 0.84f + 0.14f * streak + 0.12f * fine + 0.18f * hair;
            tone *= 1.08f - 0.10f * (py / (float) ih);
            float h = 0.35f * streak + 0.15f * fine;
            // placas negras anodizadas en relieve con canto biselado
            const float pb = plateB[i], ps = plateSharp[i];
            if (ps > 0.0f)
            {
                s.m = vbparts::mixM (s.m, black, ps);
                tone = 0.90f + 0.16f * r3d::fbm (px * 0.003f, py * 1.1f, 3, 3);
            }
            h += 5.0f * pb;
            s.m.r *= tone; s.m.g *= tone; s.m.b *= tone;
            // ventana del VU: rebaje con bisel
            h -= 12.0f * winB[i];
            // serigrafia y grabado
            if (silkA[i] > 0) { s.m = vbparts::mixM (s.m, ink, silkA[i] * (0.9f + 0.1f * r3d::fbm (px * 0.4f, py * 0.4f, 5, 2))); h += 0.5f * silkA[i]; }
            if (engA[i] > 0) { s.m = vbparts::mixM (s.m, r3d::srgb (0.88f, 0.91f, 0.95f, 0, 0.7f), 0.85f * engA[i]); h -= 0.5f * engA[i]; }
            // borde de la chapa y ranuras de las orejas
            const float dEdge = std::min (py, (float) ih - 1 - py);
            h -= 3.0f * (1.0f - r3d::fillet (dEdge, 4.0f));
            if (holeA[i] > 0) s.m.alpha = 1.0f - holeA[i];
            // tornillos y jack
            const float X = px / pk, Y = py / pk;                          // coordenadas de la foto
            for (auto& sc2 : screws)
            {
                const float dx = (X - sc2.x) * pk, dy = (Y - sc2.y) * pk, R = sc2.r * pk, rr = std::sqrt (dx * dx + dy * dy);
                if (rr < R + 1) { const auto q = vbparts::hexScrew (dx, dy, R); s.m = vbparts::mixM (s.m, q.m, q.m.alpha); h = h * (1 - q.m.alpha) + (h + 2.0f + q.h) * q.m.alpha; }
                else if (rr < R * 1.8f) { const float ao = 1.0f - 0.45f * std::exp (-std::pow ((rr - R) / (0.35f * R), 2.0f)); s.m.r *= ao; s.m.g *= ao; s.m.b *= ao; }
            }
            {
                const float dx = (X - 332) * pk, dy = (Y - 383) * pk, R = 37 * pk, rr = std::sqrt (dx * dx + dy * dy);
                if (rr < R + 1) { const auto q = vbparts::jack (dx, dy, R); s.m = vbparts::mixM (s.m, q.m, q.m.alpha); h = h * (1 - q.m.alpha) + (h + q.h) * q.m.alpha; }
            }
            s.h = h; return s;
        };
        const std::function<r3d::V3 (float, float)> view = [iw, ih] (float x, float y) { return r3d::norm ({ -x, -0.1f * (float) ih - y, 1.3f * (float) iw }); };
        cached = r3d::render (iw, ih, fn, {}, &view);
        return cached;
    }
}


Look::Look()
{
    setColour (juce::PopupMenu::backgroundColourId, Colour (0xff1b1d20));
    setColour (juce::PopupMenu::textColourId, Colour (0xffe8ecef));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colour (0xff4d6178));
    setColour (juce::PopupMenu::headerTextColourId, Colour (0xff9fb3c0));
    setColour (juce::TooltipWindow::backgroundColourId, Colour (0xff1b1d20));
    setColour (juce::TooltipWindow::textColourId, Colour (0xffe8ecef));
}
void Look::drawBubble (Graphics& g, juce::BubbleComponent&, const juce::Point<float>&, const Rectangle<float>& body)
{
    g.setColour (Colour (0xf01b1d20)); g.fillRoundedRectangle (body, 4);
    g.setColour (Colour (0x80e8ecef)); g.drawRoundedRectangle (body.reduced (0.5f), 4, 1.0f);
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
    if (k->mini)
    {
        const Pt c ((float) x + (float) w * 0.5f, (float) y + (float) h * 0.5f); const float R = (float) std::min (w, h) * 0.5f - 2.0f;
        g.setGradientFill (ColourGradient (Colour (0xff3c3e42), c.x - R * 0.5f, c.y - R * 0.7f, Colour (0xff0b0c0d), c.x + R * 0.5f, c.y + R * 0.8f, false));
        g.fillEllipse (c.x - R, c.y - R, R * 2, R * 2);
        g.setColour (Colours::black); g.drawEllipse (c.x - R, c.y - R, R * 2, R * 2, 1.0f);
        g.setColour (Colour (0xffe8e4da)); g.drawLine ({ polar (c, R * 0.25f, ang), polar (c, R * 0.9f, ang) }, 2.2f);
        return;
    }
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (k->image (ang, w, h), Rectangle<float> ((float) x, (float) y, (float) w, (float) h));
}

Knob::Knob (APVTS& s, const juce::String& id, float a0, float a1, std::vector<float> st, bool m) : steps (std::move (st)), mini (m)
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (a0, a1, true);
    setMouseDragSensitivity (steps.empty() ? 260 : 130);
    setPopupDisplayEnabled (steps.empty(), true, nullptr, 900);
    if (auto* p = s.getParameter (id)) setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    att = std::make_unique<APVTS::SliderAttachment> (s, id, *this);
}
const juce::Image& Knob::image (float ang, int w, int h)
{
    if (std::abs (ang - imgAng) < 1e-4f && imgW == w && img.isValid()) return img;
    const int iw = w * 2, ih = h * 2;
    const float R = (float) std::min (iw, ih) * 0.5f / kComp;
    img = r3d::render (iw, ih, [R, ang] (float px, float py) { return vbparts::finKnob (px, py, R, ang); }, { 0.04f * R, 0.20f * R, 0.20f * R, 0.65f });
    imgAng = ang; imgW = w;
    return img;
}

int Toggle::position() const
{
    if (decorative) return decoOn ? -1 : 1;
    const float v = proc.apvts.getRawParameterValue (id)->load();
    if (n == 3) return v > 1.5f ? -1 : (v > 0.5f ? 0 : 1);
    const bool hi = v > 0.5f;
    return (hi == upHigh) ? -1 : 1;
}
void Toggle::paint (Graphics& g)
{
    const int pos = position();
    auto& im = cache[pos + 1];
    if (! im.isValid())
    {
        const int w = getWidth() * 2, h = getHeight() * 2; const float R = L (15.0f) * 2.0f;
        im = r3d::render (w, h, [R, pos] (float x, float y) { return vbparts::miniToggle (x, y, R, pos); }, { 0.05f * R, 0.25f * R, 0.25f * R, 0.6f });
    }
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (im, getLocalBounds().toFloat());
}
void Toggle::mouseUp (const juce::MouseEvent& e)
{
    if (decorative) { decoOn = ! decoOn; repaint(); if (auto* p = getParentComponent()) p->repaint(); return; }
    if (n == 3)
    {
        const float v = proc.apvts.getRawParameterValue (id)->load();
        setParam (proc, id, juce::jlimit (0.0f, 2.0f, v + (e.position.y < (float) getHeight() * 0.5f ? 1.0f : -1.0f)));
    }
    else setParam (proc, id, proc.apvts.getRawParameterValue (id)->load() > 0.5f ? 0.0f : 1.0f);
    repaint();
}

void Lamp::paint (Graphics& g)
{
    const bool on = param (proc, "power") > 0.5f;
    auto& im = cache[on ? 1 : 0];
    if (! im.isValid())
    {
        const int w = getWidth() * 2, h = getHeight() * 2; const float R = L (54.0f) * 2.0f;
        im = r3d::render (w, h, [R, on] (float x, float y) { return vbparts::lamp (x, y, R, on); });
    }
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (im, getLocalBounds().toFloat());
    if (on)
    {
        const auto c = getLocalBounds().toFloat().getCentre();
        g.setGradientFill (ColourGradient (Colour (0x40ff3020), c.x, c.y, Colour (0x00ff3020), c.x + L (40), c.y, true));
        g.fillEllipse (c.x - L (40), c.y - L (40), L (80), L (80));
    }
}
void Lamp::mouseUp (const juce::MouseEvent&) { setParam (proc, "power", param (proc, "power") > 0.5f ? 0.0f : 1.0f); repaint(); }

Content::Content (VbxProcessor& p)
    : proc (p),
      input (p.apvts, "input", -135 * deg, 135 * deg), gain (p.apvts, "gain", 0, 1, aGain), meterSel (p.apvts, "meter", 0, 1, aMeter),
      thr (p.apvts, "thr", -135 * deg, 135 * deg), attack (p.apvts, "attack", 0, 1, aAtt), release (p.apvts, "release", 0, 1, aRel),
      dsFreq (p.apvts, "dsF", 0, 1, aDs), dsThr (p.apvts, "dsThr", -135 * deg, 135 * deg),
      low (p.apvts, "low", -135 * deg, 135 * deg), lowF (p.apvts, "lowF", 0, 1, aFreq), mid (p.apvts, "mid", -135 * deg, 135 * deg), midF (p.apvts, "midF", 0, 1, aFreq),
      hi (p.apvts, "hi", -135 * deg, 135 * deg), hiF (p.apvts, "hiF", 0, 1, aFreq), out (p.apvts, "out", -pi * 0.75f, pi * 0.75f, {}, true),
      phantom (p, "power", 2), hpf (p, "hpf", 3), phase (p, "phase", 2, false), link (p, "link", 2), comp (p, "comp", 2), deess (p, "deess", 2), eq (p, "eq", 2), eqLine (p, "eqLine", 2, false),
      lamp (p)
{
    phantom.decorative = true;
    for (juce::Component* c : std::initializer_list<juce::Component*> { &input, &gain, &meterSel, &thr, &attack, &release, &dsFreq, &dsThr, &low, &lowF, &mid, &midF, &hi, &hiF, &out,
                                                                         &phantom, &hpf, &phase, &link, &comp, &deess, &eq, &eqLine, &lamp })
        addAndMakeVisible (c);
    input.setTooltip (U ("INPUT: nivel antes del compresor y del preamp (-20 a +20 dB). Es el control \"limpio\" de nivel"));
    gain.setTooltip (U ("GAIN: realimentación de la válvula, 40-60 dB. 40 = limpio, 50 = normal, 60 = caliente (sube 5 dB por paso)"));
    meterSel.setTooltip (U ("Medidor: G-R (compresión), LINE IN, PRE OUT, EQ OUT o D-S (de-esser)"));
    thr.setTooltip (U ("THRESHOLD del compresor óptico 3:1"));
    attack.setTooltip (U ("ATTACK: de FAST (4 ms) a SLOW (70 ms)")); release.setTooltip (U ("RELEASE: de FAST (0.3 s) a SLOW (5 s)"));
    dsFreq.setTooltip (U ("Frecuencia del de-esser (3K-12K) o LIMIT 10:1")); dsThr.setTooltip (U ("THRESHOLD del de-esser / limitador"));
    low.setTooltip (U ("LOW PEAK: realce de graves (0 a +10 dB)")); mid.setTooltip (U ("MID DIP: corte de medios (0 a -10 dB)")); hi.setTooltip (U ("HI PEAK: realce de agudos (0 a +10 dB)"));
    lowF.setTooltip (U ("Frecuencia de LOW PEAK")); midF.setTooltip (U ("Frecuencia de MID DIP")); hiF.setTooltip (U ("Frecuencia de HI PEAK"));
    out.setTooltip (U ("OUTPUT: volumen de salida (-20 a +10 dB)"));
    phantom.setTooltip (U ("48V: solo decorativo (en el plugin no hace falta)"));
    hpf.setTooltip (U ("Filtro de graves: 120, 80 o FLAT")); phase.setTooltip (U ("Fase: 0° arriba, 180° abajo"));
    link.setTooltip (U ("LINK: el compresor actúa igual en los dos canales")); comp.setTooltip (U ("COMPRESS 3:1 / BYPASS"));
    deess.setTooltip (U ("DE ESS / BYPASS")); eq.setTooltip (U ("EQ IN / BYPASS"));
    eqLine.setTooltip (U ("EQ INPUT: PRE O/P = el EQ recibe el preamp; LINE IN = el EQ recibe la señal directa (sin preamp ni compresor)"));
    lamp.setTooltip (U ("POWER: enciende o apaga (señal original)"));
    plate = renderPlate();
    // barra superior
    stripImg = juce::Image (juce::Image::ARGB, W, strip, true);
    Graphics g (stripImg);
    g.setGradientFill (ColourGradient (Colour (0xff1e2124), 0, 0, Colour (0xff111315), 0, S, false)); g.fillAll();
    g.setColour (Colours::black); g.drawHorizontalLine (strip - 1, 0, (float) W);
    g.setFont (font (14, true).withExtraKerningFactor (0.08f)); g.setColour (Colour (0xffcfd8de));
    g.drawText ("Dj Pava Hm Music", Rectangle<float> (20, 0, 300, S), juce::Justification::centredLeft);
    g.setFont (font (11, true)); g.setColour (Colour (0xff9fb0bb));
    g.drawText ("OUTPUT", Rectangle<float> (1352, 4, 80, 38), juce::Justification::centredRight);
}

void Content::resized()
{
    auto place = [] (juce::Component& c, const KP& k)
    { const Pt p = P (k.x, k.y); const float d = L (k.r) * kComp * 2.0f; c.setBounds (juce::roundToInt (p.x - d * 0.5f), juce::roundToInt (p.y - d * 0.5f), juce::roundToInt (d), juce::roundToInt (d)); };
    place (input, kInput); place (gain, kGain); place (meterSel, kMeter); place (thr, kThr); place (attack, kAtt); place (release, kRel);
    place (dsFreq, kDsF); place (dsThr, kDsT); place (low, kLow); place (mid, kMid); place (hi, kHi); place (lowF, kLowF); place (midF, kMidF); place (hiF, kHiF);
    auto tog = [] (juce::Component& c, float x, float y) { const Pt p = P (x, y); const float d = L (16.0f) * 6.0f; c.setBounds (juce::roundToInt (p.x - d * 0.5f), juce::roundToInt (p.y - d * 0.5f), juce::roundToInt (d), juce::roundToInt (d)); };
    tog (phantom, 335, 113); tog (hpf, 415, 258); tog (phase, 487, 384); tog (link, 1285, 118); tog (comp, 1130, 388);
    tog (deess, 1520, 152); tog (eq, 1525, 388); tog (eqLine, 1525, 590);
    { const Pt p = P (2170, 157); const float d = L (54) * 2.2f; lamp.setBounds (juce::roundToInt (p.x - d * 0.5f), juce::roundToInt (p.y - d * 0.5f), juce::roundToInt (d), juce::roundToInt (d)); }
    out.setBounds (1442, 7, 32, 32);
}

void Content::paintVu (Graphics& g)
{
    const auto xf = photoToContent();
    Path win = vuWindow(); win.applyTransform (xf);
    juce::Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (win);
    const bool on = param (proc, "power") > 0.5f;
    // cara retroiluminada (crema al centro, naranja en los bordes)
    const Pt cc = P (810, 200);
    g.setGradientFill (ColourGradient (on ? Colour (0xfffde3b8) : Colour (0xffd9c6a6), cc.x, cc.y, on ? Colour (0xffe7a15a) : Colour (0xffb08a62), cc.x + L (230), cc.y, true));
    g.fillPath (win);
    const Pt pv = P (vuPivot.x, vuPivot.y);
    // escala
    g.setColour (Colour (0xff141210));
    for (int i = 0; i < 11; ++i)
    {
        const float a = vuAng[i] * deg;
        const bool red = vuDb[i] > 0;
        g.setColour (red ? Colour (0xffc8161b) : Colour (0xff141210));
        g.setFont (font (L (34)).withHorizontalScale (0.92f));
        const auto np = polar (pv, L (214), a);
        juce::String t = juce::String ((int) std::abs (vuDb[i]));
        g.drawText (t, Rectangle<float> (np.x - L (30), np.y - L (20), L (60), L (40)), juce::Justification::centred);
        if (! red) { g.setColour (Colour (0xff141210)); g.drawLine ({ polar (pv, L (166), a), polar (pv, L (190), a) }, L (4.5f)); }
    }
    // rayas menores y puntos
    for (float db : { -15.0f, -8.0f, -6.0f, -4.0f })
    {
        const float a = vuAngle (db) * deg;
        g.setColour (Colour (0xff141210)); g.drawLine ({ polar (pv, L (172), a), polar (pv, L (188), a) }, L (3.5f));
    }
    const float pct[] { 0, 20, 40, 60, 80, 100 };
    for (int i = 0; i < 6; ++i)
    {
        const float db = pct[i] <= 0 ? -24.0f : 20.0f * std::log10 (pct[i] / 100.0f);
        const float a = vuAngle (db) * deg;
        const auto d = polar (pv, L (152), a);
        g.fillEllipse (d.x - L (4.5f), d.y - L (4.5f), L (9), L (9));
        const auto q = polar (pv, L (133), a);
        g.setFont (font (L (16)));
        g.drawText (juce::String ((int) pct[i]), Rectangle<float> (q.x - L (20), q.y - L (10), L (40), L (20)), juce::Justification::centred);
    }
    // zona roja
    {
        Path red; red.addCentredArc (pv.x, pv.y, L (176), L (176), 0, vuAngle (0) * deg, vuAngle (3) * deg, true);
        g.setColour (Colour (0xffcf1a1f)); g.strokePath (red, juce::PathStrokeType (L (8)));
        for (float db : { 1.0f, 2.0f, 3.0f }) { const float a = vuAngle (db) * deg; g.drawLine ({ polar (pv, L (172), a), polar (pv, L (192), a) }, L (4)); }
    }
    // aguja
    const float a = vuAngle (vuShown) * deg;
    g.setColour (Colours::black.withAlpha (0.25f)); g.drawLine ({ polar (pv, L (95), a).translated (L (4), L (3)), polar (pv, L (240), a).translated (L (4), L (3)) }, L (3));
    g.setColour (Colour (0xff111111)); g.drawLine ({ polar (pv, L (95), a), polar (pv, L (240), a) }, L (2.6f));
    // sombra interior del bisel y reflejo del vidrio
    g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.45f), 0, P (0, 95).y, Colours::transparentBlack, 0, P (0, 140).y, false)); g.fillPath (win);
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.16f), P (660, 100).x, P (660, 100).y, Colours::transparentWhite, P (760, 200).x, P (760, 200).y, false)); g.fillPath (win);
}

void Content::paint (Graphics& g)
{
    g.drawImageAt (stripImg, 0, 0);
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (plate, Rectangle<float> (0, S, (float) W, (float) H - S));
    paintVu (g);
    // LED del 48V
    {
        const Pt c = P (340, 258); const float r = L (8);
        if (phantom.decoOn)
        {
            g.setGradientFill (ColourGradient (Colour (0x90ff2010), c.x, c.y, Colour (0x00ff2010), c.x + r * 3.5f, c.y, true)); g.fillEllipse (c.x - r * 3.5f, c.y - r * 3.5f, r * 7, r * 7);
            g.setGradientFill (ColourGradient (Colour (0xffffd0c0), c.x - r * 0.3f, c.y - r * 0.3f, Colour (0xffd01008), c.x + r, c.y + r, true));
        }
        else g.setGradientFill (ColourGradient (Colour (0xff5a2020), c.x - r * 0.3f, c.y - r * 0.3f, Colour (0xff200606), c.x + r, c.y + r, true));
        g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    }
    // barra superior: presets, XFMR y OUTPUT
    for (auto [r, t] : { std::pair<Rectangle<float>, const char*> { prevArea, "<" }, { nextArea, ">" } })
    {
        g.setGradientFill (ColourGradient (Colour (0xff2e3337), 0, r.getY(), Colour (0xff1b1e21), 0, r.getBottom(), false)); g.fillRoundedRectangle (r, 4);
        g.setColour (Colours::black); g.drawRoundedRectangle (r, 4, 1.0f);
        Path tri; const auto c = r.getCentre(); const float s = t[0] == '<' ? -1.0f : 1.0f;
        tri.addTriangle (c.x - 4 * s, c.y - 6, c.x - 4 * s, c.y + 6, c.x + 5 * s, c.y);
        g.setColour (Colour (0xffcfd8de)); g.fillPath (tri);
    }
    g.setGradientFill (ColourGradient (Colour (0xff0a0b0c), 0, presetArea.getY(), Colour (0xff181a1c), 0, presetArea.getBottom(), false)); g.fillRoundedRectangle (presetArea, 4);
    g.setColour (Colours::black); g.drawRoundedRectangle (presetArea, 4, 1.0f);
    g.setFont (font (14, true)); g.setColour (Colour (0xffe8edf0));
    g.drawText (proc.getProgramName (proc.getCurrentProgram()), presetArea, juce::Justification::centred);
    const bool x = param (proc, "xfmr") > 0.5f;
    g.setColour (x ? Colour (0xff3d5a74) : Colour (0xff23272b)); g.fillRoundedRectangle (xfmrArea, 13);
    g.setColour (Colours::black); g.drawRoundedRectangle (xfmrArea, 13, 1.0f);
    g.setFont (font (11, true)); g.setColour (x ? Colour (0xffe8edf0) : Colour (0xff7f8a92));
    g.drawText ("XFMR", xfmrArea, juce::Justification::centred);
    g.setFont (font (11)); g.setColour (Colour (0xff9fb0bb));
    g.drawText (juce::String (param (proc, "out"), 1) + " dB", Rectangle<float> (1480, 4, 60, 38), juce::Justification::centredLeft);
}

void Content::mouseUp (const juce::MouseEvent& e)
{
    const int n = proc.getNumPrograms();
    if (prevArea.contains (e.position)) { proc.setCurrentProgram ((proc.getCurrentProgram() + n - 1) % n); repaint(); return; }
    if (nextArea.contains (e.position)) { proc.setCurrentProgram ((proc.getCurrentProgram() + 1) % n); repaint(); return; }
    if (xfmrArea.contains (e.position)) { setParam (proc, "xfmr", param (proc, "xfmr") > 0.5f ? 0.0f : 1.0f); repaint(); return; }
    if (! presetArea.contains (e.position)) return;
    juce::PopupMenu m; m.addSectionHeader ("PRESETS");
    for (int i = 0; i < n; ++i) m.addItem (i + 1, proc.getProgramName (i), true, i == proc.getCurrentProgram());
    juce::Component::SafePointer<Content> sp (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (presetArea.toNearestInt())), [sp] (int r)
    { if (sp != nullptr && r > 0) { sp->proc.setCurrentProgram (r - 1); sp->repaint(); } });
}

void Content::tick()
{
    // VU: 0 VU = -18 dBFS; en G-R y D-S la aguja baja desde 0
    const int mode = (int) param (proc, "meter");
    const float line = proc.mLine.exchange (0), pre = proc.mPre.exchange (0), eqv = proc.mEq.exchange (0), gc = proc.mComp.exchange (0), gd = proc.mDs.exchange (0);
    float target;
    if (mode == 0) target = -gc;
    else if (mode == 4) target = -gd;
    else target = juce::Decibels::gainToDecibels ((mode == 1 ? line : mode == 2 ? pre : eqv) * 0.7071f, -60.0f) + 18.0f;
    if (param (proc, "power") < 0.5f) target = -26.0f;
    vu = juce::jlimit (-26.0f, 4.0f, target);
    const float prev = vuShown;
    vuShown += (vu - vuShown) * 0.25f;     // balistica ~300 ms
    if (std::abs (prev - vuShown) > 0.01f) repaint (getLocalArea (nullptr, Rectangle<int> ((int) P (600, 60).x, (int) P (600, 60).y, (int) L (420), (int) L (200))));
    const int prog = proc.getCurrentProgram(); const float o = param (proc, "out"), x = param (proc, "xfmr");
    if (prog != lastProgram || o != lastOut || x != lastX) { lastProgram = prog; lastOut = o; lastX = x; repaint (0, 0, W, strip); }
    for (auto* t : { &hpf, &phase, &link, &comp, &deess, &eq, &eqLine }) t->repaint();
    lamp.repaint();
}

VbxEditor::VbxEditor (VbxProcessor& p) : AudioProcessorEditor (p), content (p)
{
    setLookAndFeel (&look);
    addAndMakeVisible (content);
    content.setBounds (0, 0, W, H);
    setResizable (true, true);
    setResizeLimits (1000, (int) (1000.0 * H / W), 2400, (int) (2400.0 * H / W));
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) W / (double) H);
    setSize (1280, (int) (1280.0 * H / W));
    startTimerHz (30);
}
VbxEditor::~VbxEditor() { setLookAndFeel (nullptr); }
void VbxEditor::resized() { content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) W)); }
