#include "Top73Editor.h"
#include "T73Parts.h"
#include <map>

using namespace t73;
using juce::Colour; namespace Colours = juce::Colours; using juce::Graphics; using juce::Path; using juce::Rectangle; using juce::ColourGradient;
using Pt = juce::Point<float>;

namespace
{
    juce::String U (const char* s) { return juce::String::fromUTF8 (s); }
    constexpr float pi = juce::MathConstants<float>::pi, deg = pi / 180.0f;
    float param (Top73Processor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); }
    void setParam (Top73Processor& p, const char* id, float v)
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

    // ------------------------------------------------------------------ geometria (pixeles de la foto frontal del modulo)
    const Pt cGain { 167.9f, 250.7f }, cHf { 167.9f, 512.0f }, cMid { 167.9f, 772.6f }, cLow { 167.9f, 1033.2f }, cHpf { 167.9f, 1292.0f }, cLevel { 433.0f, 800.0f };
    constexpr float dotR = 6.6f, scaleR = 110.6f;
    struct WingSpec { float Rc, Rw, hw; };
    const WingSpec redSpec { 66.0f, 95.0f, 21.0f }, blueSpec { 60.0f, 93.0f, 20.0f };
    constexpr float greyBigR = 68.0f, greyR = 57.5f, greySmallR = 44.0f, ringRi = 62.0f, ringRo = 90.0f;
    float boxHalf (KnobType t)
    {
        switch (t) { case KnobType::Red: return 114.0f; case KnobType::Blue: return 112.0f; case KnobType::GreyBig: return 88.0f; case KnobType::Grey: return 75.0f; case KnobType::GreySmall: return 58.0f; case KnobType::Ring: return 98.0f; }
        return 75.0f;
    }
    const Rectangle<float> moduleR { 2, 6, 334, 1651 }, trayR { 344, 6, 178, 1651 };
    const Rectangle<float> keyEql { 62, 1445, 105, 62 }, keyPhase { 172, 1445, 108, 62 }, keyBezel { 57, 1440, 228, 72 };
    const Rectangle<float> keyPrev { 359, 240, 73, 32 }, keyNext { 434, 240, 73, 32 }, prevBezel { 355, 236, 156, 40 };
    const Rectangle<float> keyIp { 359, 610, 73, 38 }, keyOp { 434, 610, 73, 38 }, ipBezel { 355, 606, 156, 46 };
    const Rectangle<float> keyAuto { 373, 1010, 120, 52 }, autoBezel { 368, 1005, 130, 62 };
    const Rectangle<float> keyPower { 373, 1445, 120, 62 }, powerBezel { 368, 1440, 130, 72 };
    const Rectangle<float> key3d { 373, 1215, 120, 52 }, bezel3d { 368, 1210, 130, 62 };
    const Pt ledAuto { 433.0f, 987.0f }, ledPower { 433.0f, 1421.0f }, led3d { 433.0f, 1192.0f };
    float meterY (int i) { return 338.0f + 38.0f * (float) i; }           // 0 = +24 arriba ... 6 = -30 abajo
    const float meterDbu[] { 24, 18, 14, 5, 0, -10, -30 };
    const char* meterTxt[] { "+24", "+18", "+14", "+5", "0", "-10", "-30" };
    juce::String ch (int c) { return juce::String::charToString ((juce::juce_wchar) c); }

    // ------------------------------------------------------------------ serigrafia blanca
    void cross (Graphics& g, Pt c, float s, float w) { g.fillRect (c.x - s * 0.5f, c.y - w * 0.5f, s, w); g.fillRect (c.x - w * 0.5f, c.y - s * 0.5f, w, s); }
    void dash (Graphics& g, Pt c, float s, float w) { g.fillRect (c.x - s * 0.5f, c.y - w * 0.5f, s, w); }
    void drawSilk (Graphics& g)
    {
        g.setColour (Colours::white);
        auto dot = [&g] (Pt c, float r = dotR) { g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2); };
        const juce::String en = ch (0x2013), md = ch (0xb7);
        auto T = [&g] (const juce::String& t, float x, float y, float cap = 15.0f, bool bold = false, float hs = 1.0f)
        { textAt (g, t, x, y, sans (cap, bold), hs); if (! bold) { textAt (g, t, x + 0.5f, y, sans (cap, bold), hs); textAt (g, t, x + 0.25f, y + 0.3f, sans (cap, bold), hs); } };   // peso medio
        // ---- logotipo propio: circulo con "HM"
        g.drawEllipse (50 - 26.5f, 108 - 26.5f, 53, 53, 3.2f);
        T ("HM", 50, 108, 15.5f, true, 0.92f);
        // ---- perilla roja: 22 posiciones cada 15 grados (MIC 80..55, OFF, 50..20, OFF, LINE +10..-20)
        for (int i = 0; i < 24; ++i) if (i != 1 && i != 2) dot (polar (cGain, 110.8f, (float) i * 15.0f * deg));
        T ("dB", 168, 81); T (en + "80", 163, 121); T ("70", 104, 135); T ("60", 46, 191); T ("OFF", 31, 250, 13.5f); T ("50", 36, 283);
        T ("40", 64, 334); T ("MIC.", 46, 375); T ("30", 132, 376); T (en + "20", 199, 377); T ("OFF", 256, 360, 13.5f);
        T ("+10", 275, 331); T ("0", 292, 282); T ("10", 296, 218); T (en + "20", 272, 160); T ("LINE", 269, 120);
        // ---- agudos, medios y graves: puntos de +-60 a +-165 grados (el 0 va abajo)
        for (Pt c : { cHf, cMid, cLow })
            for (int i = 4; i <= 11; ++i) { dot (polar (c, scaleR, (float) i * 15.0f * deg)); dot (polar (c, scaleR, -(float) i * 15.0f * deg)); }
        for (Pt c : { cHf, cMid, cLow }) { cross (g, { 71, c.y - 79 }, 15, 3.2f); dash (g, { 267, c.y - 80 }, 15, 3.2f); }
        T ("0", 168, 623);
        T ("7" + md + "2", 34, 773); T ("OFF", 306, 773, 13.5f); T ("4" + md + "8", 47, 830); T ("0" + md + "36", 300, 831);
        T ("KHz", 34, 884); T ("3" + md + "2", 100, 887); T ("0", 169, 883); T ("07", 235, 887); T ("1" + md + "6", 170, 903);
        T ("OFF", 306, 1034, 13.5f); T ("35", 288, 1094); T ("Hz", 25, 1145); T ("220", 105, 1149); T ("0", 168, 1144); T ("110", 168, 1164); T ("60", 230, 1147);
        // ---- filtro azul
        for (float a : { 90.0f, 120.0f, 150.0f, 180.0f, 210.0f }) dot (polar (cHpf, scaleR, a * deg));
        T ("OFF", 305, 1291, 13.5f); T ("50", 287, 1354); T ("80", 230, 1407); T ("160", 170, 1422); T ("300", 105, 1407); T ("Hz", 25, 1402);
        // ---- simbolos de las bandas
        const juce::PathStrokeType st (3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::butt);
        { Path p; p.startNewSubPath (269, 623); p.lineTo (284, 623); p.quadraticTo (292, 623, 299, 612); p.lineTo (317, 611);
          p.startNewSubPath (284, 623); p.quadraticTo (292, 623, 299, 634); p.lineTo (317, 635); g.strokePath (p, st); }
        { Path p; p.startNewSubPath (269, 883); p.lineTo (281, 883); p.addEllipse (281, 875, 24, 16); p.startNewSubPath (305, 883); p.lineTo (318, 883); g.strokePath (p, st); }
        { Path p; p.startNewSubPath (268, 1131); p.lineTo (284, 1131); p.quadraticTo (291, 1131, 298, 1142); p.lineTo (317, 1142);
          p.startNewSubPath (268, 1153); p.lineTo (284, 1153); p.quadraticTo (291, 1153, 298, 1142); g.strokePath (p, st); }
        { Path p; p.startNewSubPath (270, 1418); p.lineTo (282, 1395); p.lineTo (317, 1395); g.strokePath (p, st); }
        // ---- bandeja: nombre, presets, medidor, LEVEL
        T ("TOP 73", 433, 92, 22.0f, true, 0.95f);
        T ("Dj Pava Hm Music", 433, 124, 9.5f, false);
        T ("PRESET", 433, 153, 10.0f);
        T ("METER", 433, 306, 10.0f);
        for (int i = 0; i < 7; ++i) T (juce::String (meterTxt[i]).replace ("-", en), 466, meterY (i), 10.5f);
        for (int i = 0; i <= 12; ++i) dot (polar (cLevel, 74.0f, (-135.0f + 22.5f * (float) i) * deg), 4.4f);
        T (en + "20", 365, 868, 11.0f); T (en + "10", 367, 731, 11.0f); T ("0", 499, 731, 11.0f); T ("+10", 501, 868, 11.0f);
        T ("LEVEL", 433, 905, 11.0f);
    }

    // ------------------------------------------------------------------ placa (se calcula una vez)
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
    juce::Image renderPlate()
    {
        static juce::Image cached;
        if (cached.isValid()) return cached;
        constexpr float k = 1.5f;
        const int iw = (int) ((float) Content::W * k), ih = (int) ((float) Content::H * k);
        const auto panels = maskOf (iw, ih, k, [] (Graphics& g) { g.fillRoundedRectangle (moduleR, 3.0f); g.fillRoundedRectangle (trayR, 3.0f); }, 1);
        const auto silk = maskOf (iw, ih, k, [] (Graphics& g) { drawSilk (g); }, 0);
        const auto bezel = maskOf (iw, ih, k, [] (Graphics& g)
        {
            for (auto r : { keyBezel, prevBezel, ipBezel, autoBezel, bezel3d, powerBezel }) g.fillRoundedRectangle (r, 3.0f);
        }, 1);
        const auto glass = maskOf (iw, ih, k, [] (Graphics& g) { g.fillRoundedRectangle (358, 168, 150, 62, 4.0f); }, 1);
        const auto holes = maskOf (iw, ih, k, [] (Graphics& g)
        {
            for (int i = 0; i < 7; ++i) g.fillEllipse (408 - 9, meterY (i) - 9, 18, 18);
            for (Pt c : { ledAuto, led3d, ledPower }) g.fillEllipse (c.x - 9, c.y - 9, 18, 18);
        }, 1);
        struct Sk { Pt c; float R; };
        const Sk skirts[] { { cGain, 96.0f }, { cHf, 94.0f }, { cHpf, 96.0f }, { cLevel, 62.0f } };
        const Pt screws[] { { 56.5f, 41 }, { 282.5f, 41 }, { 59, 1622.5f }, { 282.5f, 1622.5f }, { 370, 41 }, { 496, 41 }, { 370, 1622.5f }, { 496, 1622.5f } };
        const auto paintM = t73p::paint(), inkM = t73p::ink();
        auto chassis = r3d::srgb (0.035f, 0.038f, 0.042f, 0, 0.3f);
        const float cx = (float) iw * 0.5f, cy = (float) ih * 0.5f;
        auto fn = [&] (float rx, float ry)
        {
            const float px = rx + cx, py = ry + cy;
            const int ix = juce::jlimit (0, iw - 1, (int) px), iy = juce::jlimit (0, ih - 1, (int) py);
            const size_t i = (size_t) (iy * iw + ix);
            const float X = px / k, Y = py / k;
            r3d::Sample s; s.m = chassis;
            const float pm = panels[i];
            float h = 4.0f * pm;
            if (pm > 0)
            {
                // pintura en polvo: grano fino y nubes muy suaves
                const float tone = 0.95f + 0.05f * r3d::fbm (px * 0.012f, py * 0.012f, 3, 3) + 0.035f * (r3d::vnoise (px * 0.7f, py * 0.7f, 5) - 0.5f);
                r3d::Mat m = paintM; m.r *= tone; m.g *= tone; m.b *= tone;
                s.m = t73p::mixM (s.m, m, pm);
                h += 0.15f * r3d::vnoise (px * 0.5f, py * 0.5f, 9);
            }
            if (silk[i] > 0) { s.m = t73p::mixM (s.m, inkM, silk[i]); h += 0.45f * silk[i]; }
            if (bezel[i] > 0) { s.m = t73p::mixM (s.m, r3d::srgb (0.02f, 0.022f, 0.025f, 0, 0.35f), bezel[i]); h -= 3.0f * bezel[i]; }
            if (glass[i] > 0) { auto gm = r3d::srgb (0.010f, 0.012f, 0.012f, 0, 0.95f); gm.f0 = 0.06f; s.m = t73p::mixM (s.m, gm, glass[i]); h -= 3.0f * glass[i]; }
            if (holes[i] > 0) { s.m = t73p::mixM (s.m, t73p::chrome (0.6f, 0.45f), holes[i]); h -= 1.0f * holes[i]; }
            for (const auto& sk : skirts)
            {
                const float dx = (X - sk.c.x) * k, dy = (Y - sk.c.y) * k, R = sk.R * k;
                if (dx * dx + dy * dy < (R + 2) * (R + 2)) { const auto q = t73p::skirt (dx, dy, R); s.m = t73p::mixM (s.m, q.m, q.m.alpha); h += q.h * q.m.alpha; }
            }
            for (Pt c : screws)
            {
                const float dx = (X - c.x) * k, dy = (Y - c.y) * k, R = 19.0f * k, rr = std::sqrt (dx * dx + dy * dy);
                if (rr < R + 1) { const auto q = t73p::socketScrew (dx, dy, R); s.m = t73p::mixM (s.m, q.m, q.m.alpha); h += q.h * q.m.alpha; }
                else if (rr < R * 1.7f) { const float ao = 1.0f - 0.4f * std::exp (-std::pow ((rr - R) / (0.3f * R), 2.0f)); s.m.r *= ao; s.m.g *= ao; s.m.b *= ao; }
            }
            {
                const float dx = (X - 174.0f) * k, dy = (Y - 1628.0f) * k, R = 30.0f * k, rr = std::sqrt (dx * dx + dy * dy);
                if (rr < R + 1) { const auto q = t73p::thumbScrew (dx, dy, R); s.m = t73p::mixM (s.m, q.m, q.m.alpha); h += (6.0f + q.h) * q.m.alpha; }
                else if (rr < R * 1.6f) { const float ao = 1.0f - 0.5f * std::exp (-std::pow ((rr - R) / (0.3f * R), 2.0f)); s.m.r *= ao; s.m.g *= ao; s.m.b *= ao; }
            }
            s.h = h; return s;
        };
        cached = r3d::render (iw, ih, fn);
        return cached;
    }

    // ------------------------------------------------------------------ imagenes de piezas (cache)
    std::map<juce::String, juce::Image>& cache() { static std::map<juce::String, juce::Image> c; return c; }
    const juce::Image& wingImage (bool red, int idx, float ang, int px)
    {
        const juce::String key = juce::String (red ? "R" : "B") + juce::String (idx) + "_" + juce::String (px);
        auto& c = cache();
        auto it = c.find (key);
        if (it != c.end()) return it->second;
        const auto spec = red ? redSpec : blueSpec;
        const float sc = (float) px / (2.0f * boxHalf (red ? KnobType::Red : KnobType::Blue));
        const auto body = red ? t73p::plastic (0.66f, 0.13f, 0.09f, 0.78f) : t73p::plastic (0.07f, 0.30f, 0.62f, 0.78f);
        const float Rw = spec.Rw * sc;
        auto img = r3d::render (px, px, [=] (float x, float y) { return t73p::wingKnob (x, y, spec.Rc * sc, Rw, spec.hw * sc, ang, body, true); },
                                { 0.05f * Rw, 0.09f * Rw, 0.10f * Rw, 0.55f, 0.04f * Rw, 0.45f });
        return c[key] = img;
    }
    const juce::Image& greyImage (float R, int px, float half)
    {
        const juce::String key = "G" + juce::String (R) + "_" + juce::String (px);
        auto& c = cache();
        auto it = c.find (key);
        if (it != c.end()) return it->second;
        const float sc = (float) px / (2.0f * half), Rp = R * sc;
        auto img = r3d::render (px, px, [=] (float x, float y) { return t73p::knurlKnob (x, y, Rp, R > 60 ? 28 : 24); }, { 0.05f * Rp, 0.10f * Rp, 0.12f * Rp, 0.6f, 0.05f * Rp, 0.5f });
        return c[key] = img;
    }
    const juce::Image& ringImage (int px)
    {
        const juce::String key = "W" + juce::String (px);
        auto& c = cache();
        auto it = c.find (key);
        if (it != c.end()) return it->second;
        const float sc = (float) px / (2.0f * boxHalf (KnobType::Ring));
        auto img = r3d::render (px, px, [=] (float x, float y) { return t73p::whiteRing (x, y, ringRi * sc, ringRo * sc); }, { 0.04f * ringRo * sc, 0.07f * ringRo * sc, 0.08f * ringRo * sc, 0.5f, 0.03f * ringRo * sc, 0.4f });
        return c[key] = img;
    }
    const juce::Image& keyImage (int w, int h, bool pressed)
    {
        const juce::String key = "K" + juce::String (w) + "x" + juce::String (h) + (pressed ? "d" : "u");
        auto& c = cache();
        auto it = c.find (key);
        if (it != c.end()) return it->second;
        auto img = r3d::render (w, h, [=] (float x, float y) { return t73p::capKey (x, y, (float) w * 0.5f - 2.0f, (float) h * 0.5f - 2.0f, pressed); }, { 0, 1.5f, 2.5f, 0.5f, 2.0f, 0.4f });
        return c[key] = img;
    }
}

// =====================================================================================================
Look::Look()
{
    setColour (juce::PopupMenu::backgroundColourId, Colour (0xff1d242c));
    setColour (juce::PopupMenu::textColourId, Colour (0xffe4e8ee));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colour (0xff8a2a22));
    setColour (juce::PopupMenu::headerTextColourId, Colour (0xffaab3bf));
    setColour (juce::TooltipWindow::backgroundColourId, Colour (0xff1d242c));
    setColour (juce::TooltipWindow::textColourId, Colour (0xffe4e8ee));
}
void Look::drawBubble (Graphics& g, juce::BubbleComponent&, const juce::Point<float>&, const Rectangle<float>& body)
{
    g.setColour (Colour (0xf01d242c)); g.fillRoundedRectangle (body, 4);
    g.setColour (Colour (0x80e4e8ee)); g.drawRoundedRectangle (body.reduced (0.5f), 4, 1.0f);
}

Knob::Knob (APVTS& s, const juce::String& id, KnobType t, std::vector<float> st, float a0, float a1) : type (t), steps (std::move (st))
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (a0 * deg, a1 * deg, true);
    setMouseDragSensitivity (steps.empty() ? 240 : 120);
    setPopupDisplayEnabled (steps.empty(), true, nullptr, 900);
    if (auto* p = s.getParameter (id)) setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    att = std::make_unique<APVTS::SliderAttachment> (s, id, *this);
}
float Knob::angleRad() const
{
    if (! steps.empty())
    {
        const int idx = juce::jlimit (0, (int) steps.size() - 1, (int) std::round (getValue() - getMinimum()));
        return steps[(size_t) idx] * deg;
    }
    const auto rp = getRotaryParameters();
    const float pos = (float) ((getValue() - getMinimum()) / (getMaximum() - getMinimum()));
    return rp.startAngleRadians + pos * (rp.endAngleRadians - rp.startAngleRadians);
}
bool Knob::hitTest (int x, int y)
{
    const float half = (float) getWidth() * 0.5f, dx = (float) x - half, dy = (float) y - half, r = std::sqrt (dx * dx + dy * dy);
    switch (type)
    {
        case KnobType::Red: return r < 97.0f;
        case KnobType::Blue: return r < 95.0f;
        case KnobType::GreyBig: return r < greyBigR + 2.0f;
        case KnobType::Grey: return r < greyR + 2.0f;
        case KnobType::GreySmall: return r < greySmallR + 2.0f;
        case KnobType::Ring: return r > greyR + 2.0f && r < ringRo + 3.0f;
    }
    return false;
}
void Knob::paint (Graphics& g)
{
    const float S = (float) getWidth(), half = S * 0.5f;
    const int px = juce::roundToInt (S * 2.0f);
    const float a = angleRad();
    const Pt c (half, half);
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    if (type == KnobType::Red || type == KnobType::Blue)
    {
        const int idx = steps.empty() ? 0 : juce::jlimit (0, (int) steps.size() - 1, (int) std::round (getValue() - getMinimum()));
        g.drawImage (wingImage (type == KnobType::Red, idx, a, px), Rectangle<float> (0, 0, S, S));
        return;
    }
    if (type == KnobType::Ring)
    {
        g.drawImage (ringImage (px), Rectangle<float> (0, 0, S, S));
        g.setColour (Colour (0xff6a6352));                                 // raya del selector sobre el aro
        g.drawLine ({ polar (c, ringRi + 3.0f, a), polar (c, ringRi + 15.0f, a) }, 3.0f);
        return;
    }
    const float R = type == KnobType::GreyBig ? greyBigR : type == KnobType::Grey ? greyR : greySmallR;
    g.drawImage (greyImage (R, px, half), Rectangle<float> (0, 0, S, S));
    g.setColour (Colours::black.withAlpha (0.35f));
    g.drawLine ({ polar (c, R * 0.74f, a).translated (0.6f, 0.9f), polar (c, R * 0.90f, a).translated (0.6f, 0.9f) }, R * 0.05f);
    g.setColour (Colour (0xfff4f4f0));
    g.drawLine ({ polar (c, R * 0.74f, a), polar (c, R * 0.90f, a) }, R * 0.045f);
}

void Key::paint (Graphics& g)
{
    const bool on = (isOn != nullptr && isOn()) || held;
    const int w = getWidth(), h = getHeight();
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (keyImage (w * 2, h * 2, on), getLocalBounds().toFloat());
    g.setColour (Colour (0xff111111));
    textAt (g, text, (float) w * 0.5f, (float) h * 0.5f + (on ? 1.5f : 0.0f), sans (textCap, true), 0.88f);
}

// =====================================================================================================
namespace
{
    std::vector<float> gainAngles() { return { 165, 180, 195, 210, 225, 240, 255, 285, 300, 315, 330, 345, 360, 405, 420, 435, 450, 465, 480, 495 }; }
}
Content::Content (Top73Processor& p)
    : proc (p),
      gain (p.apvts, "gain", KnobType::Red, gainAngles(), 165, 495),
      hf (p.apvts, "hf", KnobType::GreyBig, {}, 60, 300),
      midFreq (p.apvts, "midFreq", KnobType::Ring, { 90, 120, 150, 180, 210, 240, 270 }, 90, 270),
      midGain (p.apvts, "midGain", KnobType::Grey, {}, 60, 300),
      lowFreq (p.apvts, "lowFreq", KnobType::Ring, { 90, 120, 150, 180, 210 }, 90, 210),
      lowGain (p.apvts, "lowGain", KnobType::Grey, {}, 60, 300),
      hpf (p.apvts, "hpf", KnobType::Blue, { 90, 120, 150, 180, 210 }, 90, 210),
      level (p.apvts, "out", KnobType::GreySmall, {}, 225, 495)
{
    plate = renderPlate();
    for (juce::Component* c : std::initializer_list<juce::Component*> { &gain, &hf, &midFreq, &midGain, &lowFreq, &lowGain, &hpf, &level,
                                                                         &eqKey, &phaseKey, &prevKey, &nextKey, &ipKey, &opKey, &autoKey, &sat3dKey, &powerKey })
        addAndMakeVisible (c);
    auto toggle = [this] (const char* id) { setParam (proc, id, param (proc, id) > 0.5f ? 0.0f : 1.0f); repaint(); };
    eqKey.text = "EQL"; eqKey.textCap = 17.0f; eqKey.isOn = [this] { return param (proc, "eq") > 0.5f; }; eqKey.onClick = [toggle] { toggle ("eq"); };
    phaseKey.text = "PHASE"; phaseKey.textCap = 17.0f; phaseKey.isOn = [this] { return param (proc, "phase") > 0.5f; }; phaseKey.onClick = [toggle] { toggle ("phase"); };
    autoKey.text = "AUTO"; autoKey.isOn = [this] { return param (proc, "auto") > 0.5f; }; autoKey.onClick = [toggle] { toggle ("auto"); };
    sat3dKey.text = "3D"; sat3dKey.textCap = 17.0f; sat3dKey.isOn = [this] { return param (proc, "sat3d") > 0.5f; }; sat3dKey.onClick = [toggle] { toggle ("sat3d"); };
    powerKey.text = "POWER"; powerKey.isOn = [this] { return param (proc, "power") > 0.5f; }; powerKey.onClick = [toggle] { toggle ("power"); };
    ipKey.text = "I/P"; ipKey.textCap = 11.0f; ipKey.isOn = [this] { return param (proc, "meter") < 0.5f; }; ipKey.onClick = [this] { setParam (proc, "meter", 0); repaint(); };
    opKey.text = "O/P"; opKey.textCap = 11.0f; opKey.isOn = [this] { return param (proc, "meter") > 0.5f; }; opKey.onClick = [this] { setParam (proc, "meter", 1); repaint(); };
    prevKey.text = "<"; prevKey.textCap = 12.0f; nextKey.text = ">"; nextKey.textCap = 12.0f;
    prevKey.onClick = [this] { const int n = proc.getNumPrograms(); proc.setCurrentProgram ((proc.getCurrentProgram() + n - 1) % n); repaint(); };
    nextKey.onClick = [this] { const int n = proc.getNumPrograms(); proc.setCurrentProgram ((proc.getCurrentProgram() + 1) % n); repaint(); };
    gain.setTooltip (U ("GAIN (perilla roja): MIC 20-80 / LINE -20 a +10. Más ganancia = más armónicos (con 3D crecen paso a paso; con AUTO el volumen no cambia)"));
    hf.setTooltip (U ("Agudos: shelf de 12 kHz ±16 dB (+ a la izquierda)"));
    midFreq.setTooltip (U ("Aro blanco: frecuencia de los medios (0.36, 0.7, 1.6, 3.2, 4.8, 7.2 kHz u OFF)"));
    midGain.setTooltip (U ("Ganancia de los medios ±18 dB"));
    lowFreq.setTooltip (U ("Aro blanco: frecuencia de los graves (35, 60, 110, 220 Hz u OFF)"));
    lowGain.setTooltip (U ("Ganancia de los graves ±16 dB"));
    hpf.setTooltip (U ("Filtro pasa altos 18 dB/oct: 50, 80, 160, 300 Hz u OFF"));
    level.setTooltip (U ("LEVEL: volumen de salida (-20 a +10 dB)"));
    eqKey.setTooltip (U ("EQL: encender/apagar el EQ")); phaseKey.setTooltip (U ("PHASE: invertir la fase"));
    autoKey.setTooltip (U ("AUTO: compensa la ganancia de la perilla roja para que solo cambie el color"));
    sat3dKey.setTooltip (U ("3D: saturación 3D. La perilla roja agrega armónicos paso a paso (no solo volumen), los graves saturan más que los agudos y la izquierda y la derecha saturan un poco distinto. Apagada = circuito original"));
    powerKey.setTooltip (U ("POWER: encender/apagar el plugin"));
    ipKey.setTooltip (U ("Medidor en la entrada")); opKey.setTooltip (U ("Medidor en la salida"));
}

void Content::resized()
{
    auto place = [] (juce::Component& c, Pt ctr, KnobType t)
    { const float hb = boxHalf (t); c.setBounds (juce::roundToInt (ctr.x - hb), juce::roundToInt (ctr.y - hb), juce::roundToInt (hb * 2), juce::roundToInt (hb * 2)); };
    place (gain, cGain, KnobType::Red); place (hf, cHf, KnobType::GreyBig);
    place (midFreq, cMid, KnobType::Ring); place (midGain, cMid, KnobType::Grey);
    place (lowFreq, cLow, KnobType::Ring); place (lowGain, cLow, KnobType::Grey);
    place (hpf, cHpf, KnobType::Blue); place (level, cLevel, KnobType::GreySmall);
    for (auto [k, r] : { std::pair<Key*, Rectangle<float>> { &eqKey, keyEql }, { &phaseKey, keyPhase }, { &prevKey, keyPrev }, { &nextKey, keyNext },
                         { &ipKey, keyIp }, { &opKey, keyOp }, { &autoKey, keyAuto }, { &sat3dKey, key3d }, { &powerKey, keyPower } })
        k->setBounds (r.toNearestInt());
}

void Content::paint (Graphics& g)
{
    g.fillAll (Colour (0xff0b0d0f));
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (plate, Rectangle<float> (0, 0, (float) W, (float) H));
    auto led = [&g] (Pt c, float r, Colour col, bool on)
    {
        if (on) { g.setGradientFill (ColourGradient (col.withAlpha (0.55f), c.x, c.y, col.withAlpha (0.0f), c.x + r * 3.2f, c.y, true)); g.fillEllipse (c.x - r * 3.2f, c.y - r * 3.2f, r * 6.4f, r * 6.4f); }
        g.setGradientFill (ColourGradient (on ? col.brighter (0.8f) : col.darker (2.4f), c.x - r * 0.35f, c.y - r * 0.35f, on ? col : col.darker (3.2f), c.x + r, c.y + r, true));
        g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
        g.setColour (Colours::white.withAlpha (on ? 0.55f : 0.18f)); g.fillEllipse (c.x - r * 0.45f, c.y - r * 0.55f, r * 0.5f, r * 0.4f);
    };
    // medidor (dBu: 0 dBFS = +24 dBu)
    const bool power = param (proc, "power") > 0.5f;
    const float lv = (param (proc, "meter") > 0.5f ? outDb : inDb) + 24.0f;
    for (int i = 0; i < 7; ++i)
    {
        const Colour col = i < 2 ? Colour (0xffff3020) : i < 4 ? Colour (0xffffc021) : Colour (0xff35e05a);
        led ({ 408, meterY (i) }, 6.0f, col, power && lv >= meterDbu[i]);
    }
    led (ledAuto, 6.0f, Colour (0xffffa020), param (proc, "auto") > 0.5f);
    led (led3d, 6.0f, Colour (0xff3cc8ff), param (proc, "sat3d") > 0.5f);
    led (ledPower, 6.5f, Colour (0xffff2a18), power);
    // pantalla del preset
    g.setColour (Colour (0xffffb347));
    const auto name = proc.getProgramName (proc.getCurrentProgram());
    g.setFont (sans (10.5f, true));
    g.drawFittedText (name, lcd.reduced (8, 6).toNearestInt(), juce::Justification::centred, 2, 0.9f);
}

void Content::mouseUp (const juce::MouseEvent& e)
{
    if (lcd.contains (e.position)) presetMenu();
}
void Content::presetMenu()
{
    juce::PopupMenu m; m.addSectionHeader ("PRESETS");
    for (int i = 0; i < proc.getNumPrograms(); ++i) m.addItem (i + 1, proc.getProgramName (i), true, i == proc.getCurrentProgram());
    juce::Component::SafePointer<Content> sp (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (lcd.toNearestInt())), [sp] (int r) { if (sp != nullptr && r > 0) { sp->proc.setCurrentProgram (r - 1); sp->repaint(); } });
}

void Content::tick()
{
    auto toDb = [] (float v) { return juce::Decibels::gainToDecibels (v, -80.0f); };
    const float i = toDb (proc.inPeak.exchange (0.0f)), o = toDb (proc.outPeak.exchange (0.0f));
    const float ni = i > inDb ? i : std::max (i, inDb - 0.8f), no = o > outDb ? o : std::max (o, outDb - 0.8f);
    const bool meterChanged = std::abs (ni - inDb) > 0.2f || std::abs (no - outDb) > 0.2f;
    inDb = ni; outDb = no;
    if (meterChanged) repaint (390, 320, 40, 260);
    const auto s = proc.settings();
    const float bits = param (proc, "auto") + 2 * param (proc, "phase") + 4 * param (proc, "power") + 8 * param (proc, "meter") + 16 * param (proc, "eq") + 32 * (float) proc.getCurrentProgram();
    if (! (s == last) || bits != lastBits) { last = s; lastBits = bits; repaint(); }
}

Top73Editor::Top73Editor (Top73Processor& p) : AudioProcessorEditor (p), content (p)
{
    setLookAndFeel (&look);
    addAndMakeVisible (content);
    content.setBounds (0, 0, W, H);
    setResizable (true, true);
    setResizeLimits (200, (int) (200.0 * H / W), W, H);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) W / (double) H);
    setSize (290, (int) std::round (290.0 * H / W));
    startTimerHz (30);
}
Top73Editor::~Top73Editor() { setLookAndFeel (nullptr); }
void Top73Editor::resized() { content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) W)); }
