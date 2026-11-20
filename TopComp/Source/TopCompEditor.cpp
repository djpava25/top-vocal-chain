#include "TopCompEditor.h"

using namespace tvcui;
using namespace topcompui;

namespace
{
    juce::String U (const char* s) { return juce::String::fromUTF8 (s); }
    const juce::Colour ink (0xfff4f6fa);                 // serigrafia blanca
    const juce::Colour blueHi (0xff2a5ea8), blueMid (0xff1f4c92), blueLo (0xff163a75);
    constexpr float pi = juce::MathConstants<float>::pi;

    juce::Font panelFont (float h, bool bold) { return sans (h, bold).withExtraKerningFactor (0.06f); }

    juce::Point<float> polar (juce::Point<float> c, float r, float a) { return { c.x + r * std::sin (a), c.y - r * std::cos (a) }; }

    void screw (juce::Graphics& g, juce::Point<float> c, float r)
    {
        g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillEllipse (c.x - r + 1, c.y - r + 2, r * 2, r * 2);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff2f3f5), c.x - r * 0.5f, c.y - r * 0.6f,
                                                 juce::Colour (0xff70757d), c.x + r * 0.7f, c.y + r * 0.8f, true));
        g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
        g.setColour (juce::Colour (0xff3a3e44)); g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 0.8f);
        g.setColour (juce::Colour (0xff4a4f57));
        const float s = r * 0.62f;
        g.drawLine (c.x - s, c.y - s * 0.25f, c.x + s, c.y + s * 0.25f, 2.0f);
        g.drawLine (c.x - s * 0.25f, c.y + s, c.x + s * 0.25f, c.y - s, 2.0f);
    }
}

//==============================================================================
void HwLook::flutedKnob (juce::Graphics& g, juce::Point<float> c, float r, float angle)
{
    // sombra suave proyectada abajo a la derecha
    for (int i = 6; i > 0; --i)
    {
        const float e = r * (0.02f + 0.035f * (float) i);
        g.setColour (juce::Colours::black.withAlpha (0.07f));
        g.fillEllipse (c.x - r - e + r * 0.10f, c.y - r - e + r * 0.18f, (r + e) * 2, (r + e) * 2);
    }
    // faldon estriado (7 lobulos, gira con la perilla)
    juce::Path skirt;
    const int n = 180;
    for (int i = 0; i <= n; ++i)
    {
        const float t = (float) i / (float) n * 2.0f * pi;
        const float lobe = 0.5f + 0.5f * std::cos (7.0f * (t - angle));
        const float rr = r * (0.90f + 0.10f * std::pow (lobe, 0.7f));
        const auto p = polar (c, rr, t);
        if (i == 0) skirt.startNewSubPath (p); else skirt.lineTo (p);
    }
    skirt.closeSubPath();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4c50), c.x - r * 0.7f, c.y - r * 0.8f,
                                             juce::Colour (0xff050506), c.x + r * 0.6f, c.y + r * 0.9f, false));
    g.fillPath (skirt);
    g.setColour (juce::Colours::black); g.strokePath (skirt, juce::PathStrokeType (1.2f));
    // brillo en el borde superior del faldon
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    juce::Path rim; rim.addCentredArc (c.x, c.y, r * 0.93f, r * 0.93f, 0, -pi * 0.55f, pi * 0.05f, true);
    g.strokePath (rim, juce::PathStrokeType (1.6f));

    // cupula superior brillante
    const float rc = r * 0.74f;
    g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillEllipse (c.x - rc - 1, c.y - rc + 1, rc * 2 + 2, rc * 2 + 2);
    juce::ColourGradient dome (juce::Colour (0xff4e5156), c.x - rc * 0.45f, c.y - rc * 0.55f,
                               juce::Colour (0xff09090a), c.x + rc * 0.9f, c.y + rc * 0.9f, true);
    dome.addColour (0.45, juce::Colour (0xff1d1e21));
    g.setGradientFill (dome); g.fillEllipse (c.x - rc, c.y - rc, rc * 2, rc * 2);
    // reflejo especular arriba a la izquierda
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.28f), c.x - rc * 0.35f, c.y - rc * 0.6f,
                                             juce::Colours::transparentWhite, c.x - rc * 0.35f, c.y, false));
    g.fillEllipse (c.x - rc * 0.75f, c.y - rc * 0.92f, rc * 1.1f, rc * 0.7f);
    g.setColour (juce::Colours::black.withAlpha (0.8f)); g.drawEllipse (c.x - rc, c.y - rc, rc * 2, rc * 2, 1.0f);

    // linea indicadora blanca + muesca en el faldon
    g.setColour (juce::Colour (0xfff7f7f2));
    g.drawLine (juce::Line<float> (polar (c, rc * 0.18f, angle), polar (c, rc * 0.94f, angle)), r * 0.075f);
    g.drawLine (juce::Line<float> (polar (c, r * 0.90f, angle), polar (c, r * 0.99f, angle)), r * 0.05f);
}

void HwLook::pointerKnob (juce::Graphics& g, juce::Point<float> c, float r, float angle)
{
    const auto rot = juce::AffineTransform::rotation (angle).translated (c);
    // aleta tipo "chicken-head"
    juce::Path fin;
    fin.startNewSubPath (-0.12f * r, -1.52f * r);
    fin.quadraticTo (0.0f, -1.62f * r, 0.12f * r, -1.52f * r);
    fin.lineTo (0.33f * r, 0.0f);
    fin.lineTo (0.40f * r, 1.02f * r);
    fin.quadraticTo (0.0f, 1.22f * r, -0.40f * r, 1.02f * r);
    fin.lineTo (-0.33f * r, 0.0f);
    fin.closeSubPath();
    fin.applyTransform (rot);

    for (int i = 5; i > 0; --i)
    {
        g.setColour (juce::Colours::black.withAlpha (0.08f));
        auto sh = fin; sh.applyTransform (juce::AffineTransform::translation (r * 0.12f, r * 0.2f));
        g.strokePath (sh, juce::PathStrokeType (r * 0.06f * (float) i)); g.fillPath (sh);
        const float e = r * 0.05f * (float) i;
        g.fillEllipse (c.x - r * 0.9f - e + r * 0.12f, c.y - r * 0.9f - e + r * 0.2f, (r * 0.9f + e) * 2, (r * 0.9f + e) * 2);
    }
    // base redonda
    const float rb = r * 0.9f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff44464a), c.x - rb * 0.6f, c.y - rb * 0.7f,
                                             juce::Colour (0xff050505), c.x + rb * 0.7f, c.y + rb * 0.8f, false));
    g.fillEllipse (c.x - rb, c.y - rb, rb * 2, rb * 2);
    g.setColour (juce::Colours::black); g.drawEllipse (c.x - rb, c.y - rb, rb * 2, rb * 2, 1.0f);
    // aleta
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3c3e42), c.x - r, c.y - r * 1.2f,
                                             juce::Colour (0xff070708), c.x + r, c.y + r, false));
    g.fillPath (fin);
    g.setColour (juce::Colours::black); g.strokePath (fin, juce::PathStrokeType (1.0f));
    // brillo lateral de la aleta
    juce::Path hl; hl.startNewSubPath (-0.22f * r, -1.3f * r); hl.lineTo (-0.30f * r, 0.6f * r);
    hl.applyTransform (rot);
    g.setColour (juce::Colours::white.withAlpha (0.16f)); g.strokePath (hl, juce::PathStrokeType (r * 0.07f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    // reflejo de la cupula
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.22f), c.x - rb * 0.3f, c.y - rb * 0.6f,
                                             juce::Colours::transparentWhite, c.x - rb * 0.3f, c.y, false));
    g.fillEllipse (c.x - rb * 0.7f, c.y - rb * 0.85f, rb * 0.9f, rb * 0.6f);
    // linea blanca
    juce::Path line; line.startNewSubPath (0.0f, -0.25f * r); line.lineTo (0.0f, -1.42f * r);
    line.applyTransform (rot);
    g.setColour (juce::Colour (0xfff7f7f2)); g.strokePath (line, juce::PathStrokeType (r * 0.09f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void HwLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                               float start, float end, juce::Slider& s)
{
    const juce::Point<float> c ((float) x + (float) w * 0.5f, (float) y + (float) h * 0.5f);
    const float angle = start + pos * (end - start);
    const float r = (float) s.getProperties().getWithDefault ("knobR", 30.0);
    if ((bool) s.getProperties().getWithDefault ("pointer", false)) pointerKnob (g, c, r, angle);
    else flutedKnob (g, c, r, angle);
}

//==============================================================================
PanelKnob::PanelKnob (APVTS& st, const juce::String& id, const juce::String& t, std::vector<Mark> m,
                      bool ptr, float r, float a0, float a1, const juce::String& sub)
    : title (t), subTitle (sub), marks (std::move (m)), pointer (ptr), radius (r)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (a0, a1, true);
    slider.setPopupDisplayEnabled (true, true, nullptr);
    slider.getProperties().set ("knobR", r);
    slider.getProperties().set ("pointer", ptr);
    slider.setMouseDragSensitivity (ptr ? 160 : 260);
    addAndMakeVisible (slider);
    attachment = std::make_unique<APVTS::SliderAttachment> (st, id, slider);
}

juce::Point<float> PanelKnob::knobCentre() const
{
    return { (float) getWidth() * 0.5f, (float) getHeight() * 0.5f - (pointer ? 8.0f : 10.0f) };
}

void PanelKnob::resized()
{
    const auto c = knobCentre();
    const float ext = pointer ? radius * 1.75f : radius * 1.45f;
    slider.setBounds (juce::Rectangle<float> (ext * 2, ext * 2).withCentre (c).toNearestInt());
}

void PanelKnob::paint (juce::Graphics& g)
{
    const auto c = knobCentre();
    const auto rp = slider.getRotaryParameters();
    const float reach = pointer ? radius * 1.6f : radius;
    for (const auto& mk : marks)
    {
        const float prop = (float) slider.valueToProportionOfLength (mk.value);
        const float a = rp.startAngleRadians + prop * (rp.endAngleRadians - rp.startAngleRadians);
        const auto d = polar (c, reach + 9.0f, a);
        g.setColour (ink); g.fillEllipse (d.x - 3.2f, d.y - 3.2f, 6.4f, 6.4f);
        if (mk.text.isNotEmpty())
        {
            const auto lp = polar (c, reach + 27.0f + (std::abs (std::sin (a)) * 8.0f), a);
            g.setFont (panelFont (13.5f, true));
            const float tw = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), mk.text) + 4.0f;
            auto box = juce::Rectangle<float> (tw, 16).withCentre (lp);
            g.setColour (juce::Colours::black.withAlpha (0.25f)); g.drawText (mk.text, box.translated (0, 1), juce::Justification::centred);
            g.setColour (ink); g.drawText (mk.text, box, juce::Justification::centred);
        }
    }
    auto tb = juce::Rectangle<float> (0, (float) getHeight() - 22.0f, (float) getWidth(), 20.0f);
    g.setFont (panelFont (15.0f, true));
    g.setColour (juce::Colours::black.withAlpha (0.25f)); g.drawText (title, tb.translated (0, 1), juce::Justification::centred);
    g.setColour (ink); g.drawText (title, tb, juce::Justification::centred);
    if (subTitle.isNotEmpty())
    {
        g.setFont (panelFont (13.0f, true));
        g.drawText (subTitle, tb.translated (0, -18), juce::Justification::centred);
    }
}

//==============================================================================
namespace
{
    float vuPos (float db)
    {
        const float v = std::pow (10.0f, db / 20.0f);
        return juce::jlimit (-0.04f, 1.05f, (v - 0.1f) / (1.41254f - 0.1f));
    }
    float vuAngle (float p) { return juce::degreesToRadians (-40.0f + 80.0f * p); }
}

void PanelVu::tick()
{
    const int src = (int) proc.apvts.getRawParameterValue ("meter")->load();
    float db;
    if (src == 1) db = juce::jlimit (-25.0f, 0.0f, proc.grDb.load());
    else          db = (src == 0 ? proc.inLevelDb.load() : proc.outLevelDb.load()) + 15.0f;
    const float target = vuPos (db);
    const float prev = needle;
    needle += (target - needle) * 0.24f;
    if (std::abs (needle - prev) > 1.0e-4f) repaint();
}

void PanelVu::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    // marco negro del instrumento
    g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (b.translated (2, 4).reduced (1), 6);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2b2e), b.getX(), b.getY(), juce::Colour (0xff0b0b0c), b.getX(), b.getBottom(), false));
    g.fillRoundedRectangle (b, 6);
    g.setColour (juce::Colour (0xff050505)); g.drawRoundedRectangle (b.reduced (0.5f), 6, 1.0f);
    auto face = b.reduced (14.0f, 13.0f);
    g.setColour (juce::Colours::black); g.fillRect (face.expanded (3));

    g.saveState();
    g.reduceClipRegion (face.toNearestInt());
    // cara crema retroiluminada
    juce::ColourGradient bg (juce::Colour (0xfff8eccb), face.getCentreX(), face.getY() + face.getHeight() * 0.35f,
                             juce::Colour (0xffd9c48f), face.getX(), face.getBottom(), true);
    g.setGradientFill (bg); g.fillRect (face);

    const juce::Point<float> pv (face.getCentreX(), face.getBottom() + face.getHeight() * 0.42f);
    const float rs = pv.y - (face.getY() + face.getHeight() * 0.36f);
    const juce::Colour black (0xff1b1a18), red (0xffd2261b);

    // arco rojo de +0 a +3
    {
        juce::Path arc; arc.addCentredArc (pv.x, pv.y, rs + 3, rs + 3, 0, vuAngle (vuPos (0)), vuAngle (vuPos (3)), true);
        g.setColour (red); g.strokePath (arc, juce::PathStrokeType (4.0f));
        juce::Path arc2; arc2.addCentredArc (pv.x, pv.y, rs, rs, 0, vuAngle (vuPos (-20)), vuAngle (vuPos (0)), true);
        g.setColour (black); g.strokePath (arc2, juce::PathStrokeType (1.0f));
    }
    const float dbs[] = { -20, -10, -7, -5, -3, -2, -1, 0, 1, 2, 3 };
    for (float d : dbs)
    {
        const float a = vuAngle (vuPos (d));
        const bool hot = d > 0;
        g.setColour (hot ? red : black);
        g.drawLine (juce::Line<float> (polar (pv, rs, a), polar (pv, rs + (hot ? 12.0f : 10.0f), a)), hot ? 2.2f : 1.6f);
        const auto lp = polar (pv, rs + 22.0f, a);
        g.setFont (sans (d == 0 || hot ? 14.0f : 13.0f, false));
        g.drawText (juce::String ((int) std::abs (d)), juce::Rectangle<float> (26, 16).withCentre (lp), juce::Justification::centred);
    }
    for (float d = -20.0f; d < 0.0f; d += 1.0f)
    {
        const float a = vuAngle (vuPos (d));
        g.setColour (black); g.drawLine (juce::Line<float> (polar (pv, rs, a), polar (pv, rs + 4.0f, a)), 0.8f);
    }
    // escala de porcentaje interior
    g.setFont (sans (7.5f, false));
    for (int pct = 20; pct <= 100; pct += 20)
    {
        const float a = vuAngle (((float) pct / 100.0f - 0.1f) / (1.41254f - 0.1f));
        g.setColour (black);
        g.fillEllipse (juce::Rectangle<float> (3, 3).withCentre (polar (pv, rs - 6.0f, a)));
        g.drawText (juce::String (pct), juce::Rectangle<float> (20, 10).withCentre (polar (pv, rs - 14.0f, a)), juce::Justification::centred);
    }
    g.setFont (sans (20.0f, true));
    g.setColour (black); g.drawText (U ("\xe2\x80\x93"), juce::Rectangle<float> (20, 20).withCentre (polar (pv, rs + 30.0f, vuAngle (-0.04f) - 0.03f)), juce::Justification::centred);
    g.setColour (red);   g.drawText ("+", juce::Rectangle<float> (20, 20).withCentre (polar (pv, rs + 30.0f, vuAngle (1.06f))), juce::Justification::centred);
    // rotulos de la cara
    g.setColour (black.withAlpha (0.85f));
    g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultSerifFontName(), 17.0f, juce::Font::italic)));
    g.drawText ("TOP BLUE", juce::Rectangle<float> (face.getX(), face.getBottom() - 34, face.getWidth(), 20), juce::Justification::centred);
    g.setFont (sans (15.0f, true));
    g.drawText ("VU", juce::Rectangle<float> (face.getRight() - 46, face.getBottom() - 26, 36, 18), juce::Justification::centredRight);
    const int src = (int) proc.apvts.getRawParameterValue ("meter")->load();
    g.setFont (sans (9.0f, true));
    g.drawText (src == 0 ? "INPUT" : src == 1 ? "GAIN REDUCTION" : "OUTPUT",
                juce::Rectangle<float> (face.getX() + 8, face.getBottom() - 22, 110, 14), juce::Justification::centredLeft);

    // aguja
    const float a = vuAngle (needle);
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.drawLine (juce::Line<float> (polar (pv, 10, a).translated (3, 3), polar (pv, rs + 14, a).translated (3, 3)), 1.6f);
    g.setColour (juce::Colour (0xff121212));
    g.drawLine (juce::Line<float> (polar (pv, 10, a), polar (pv, rs + 14, a)), 1.5f);
    // sombra interior + reflejo del vidrio
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.35f), face.getX(), face.getY(),
                                             juce::Colours::transparentBlack, face.getX(), face.getY() + 18, false));
    g.fillRect (face);
    juce::Path glass;
    glass.startNewSubPath (face.getX(), face.getY());
    glass.lineTo (face.getX() + face.getWidth() * 0.55f, face.getY());
    glass.lineTo (face.getX() + face.getWidth() * 0.30f, face.getBottom());
    glass.lineTo (face.getX(), face.getBottom());
    glass.closeSubPath();
    g.setColour (juce::Colours::white.withAlpha (0.07f)); g.fillPath (glass);
    g.restoreState();
}

//==============================================================================
void ChromeToggle::paintButton (juce::Graphics& g, bool, bool)
{
    auto b = getLocalBounds().toFloat();
    const juce::Point<float> c (b.getCentreX(), b.getCentreY());
    // tuerca hexagonal cromada
    juce::Path hex;
    for (int i = 0; i < 6; ++i)
    {
        const auto p = polar (c, 15.0f, (float) i * pi / 3.0f + pi / 6.0f);
        if (i == 0) hex.startNewSubPath (p); else hex.lineTo (p);
    }
    hex.closeSubPath();
    g.setColour (juce::Colours::black.withAlpha (0.4f)); g.fillPath (hex, juce::AffineTransform::translation (1.5f, 3.0f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffeef0f3), c.x - 10, c.y - 12, juce::Colour (0xff6c7178), c.x + 10, c.y + 12, false));
    g.fillPath (hex);
    g.setColour (juce::Colour (0xff3b3f45)); g.strokePath (hex, juce::PathStrokeType (0.8f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff9aa0a8), c.x - 8, c.y - 8, juce::Colour (0xff2c2f34), c.x + 8, c.y + 8, true));
    g.fillEllipse (c.x - 8.5f, c.y - 8.5f, 17, 17);
    // palanca
    const bool on = getToggleState();
    const float dir = on ? -1.0f : 1.0f;
    const juce::Point<float> tip (c.x + 4.0f, c.y + dir * 26.0f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawLine (c.x + 3, c.y + 3, tip.x + 4, tip.y + 4, 7.0f);
    juce::ColourGradient bat (juce::Colour (0xff50545a), c.x - 4, 0, juce::Colour (0xfff5f6f8), c.x + 1, 0, false);
    bat.addColour (1.0, juce::Colour (0xff60646a));
    bat.point2 = { c.x + 5, 0 };
    g.setGradientFill (bat);
    g.drawLine (c.x, c.y, tip.x, tip.y, 6.0f);
    g.setColour (juce::Colour (0xff1a1a1c)); g.fillEllipse (tip.x - 6, tip.y - 6, 12, 12);
    g.setColour (juce::Colours::white.withAlpha (0.35f)); g.fillEllipse (tip.x - 3.5f, tip.y - 4, 4, 3);
}

//==============================================================================
MainView::MainView (TopCompProcessor& p)
    : proc (p),
      gain (p.apvts, "gain", "GAIN", { { -20, "-20" }, { -10, "-10" }, { 0, "0" }, { 10, "+10" }, { 20, "+20" }, { 30, "+30" } },
            false, 44, pi * 1.25f, pi * 2.75f),
      ratio (p.apvts, "ratio", "RATIO", { { 2, "2:1" }, { 3, "" }, { 4, "4:1" }, { 6, "" }, { 10, "10:1" } },
             false, 44, pi * 1.25f, pi * 2.75f),
      thr (p.apvts, "thr", "THRESHOLD", { { 0, "0" }, { -10, "-10" }, { -20, "-20" }, { -30, "-30" }, { -40, "-40" } },
           false, 44, pi * 1.25f, pi * 2.75f),
      meter (p.apvts, "meter", "METER", { { 0, "input" }, { 1, "compression" }, { 2, "output" } },
             true, 24, pi * 1.75f, pi * 2.25f),
      attack (p.apvts, "attack", "ATTACK", { { 0, "fast" }, { 100, "slow" } }, false, 44, pi * 1.25f, pi * 2.75f),
      release (p.apvts, "release", "RELEASE", { { 0, "fast" }, { 100, "slow" } }, false, 44, pi * 1.25f, pi * 2.75f),
      select (p.apvts, "select", "SELECT", { { 0, "fixed" }, { 1, "fix./man." }, { 2, "manual" } },
              true, 24, pi * 1.75f, pi * 2.25f, "attack/release"),
      hpf (p.apvts, "hpf", "HPF SELECT", { { 0, "off" }, { 1, "60" }, { 2, "120" }, { 3, "180" }, { 4, "250" }, { 5, "500" } },
           true, 24, pi * 2.0f - 1.75f, pi * 2.0f + 1.75f, "sidechain"),
      mode (p.apvts, "mode", "MODE", { { 0, "LR" }, { 1, "M" }, { 2, "S" }, { 3, U ("\xce\x94") } },
            true, 18, pi * 2.0f - 1.15f, pi * 2.0f + 1.15f),
      mix (p.apvts, "mix", "DRY / WET", { { 0, "dry" }, { 100, "wet" } }, false, 24, pi * 1.25f, pi * 2.75f),
      color (p.apvts, "color", "COLOR", { { 0, "0" }, { 100, "100" }, { 200, "200" } }, false, 24, pi * 1.25f, pi * 2.75f),
      vu (p)
{
    // THRESHOLD como en el hardware: a la derecha = threshold mas bajo = mas compresion
    thr.slider.setRotaryParameters (pi * 2.75f, pi * 1.25f, true);
    for (auto* k : { &gain, &ratio, &thr, &meter, &attack, &release, &select, &hpf, &mode, &mix, &color })
    {
        k->slider.setLookAndFeel (&look);
        addAndMakeVisible (k);
    }
    gain.slider.setTooltip (U ("GAIN: volumen de salida. Subirlo empuja el transformador de salida (más armónicos)"));
    ratio.slider.setTooltip (U ("RATIO: cuánto comprime por encima del threshold (2:1 suave … 10:1 fuerte)"));
    thr.slider.setTooltip (U ("THRESHOLD: a la derecha = empieza a comprimir antes (más compresión)"));
    meter.slider.setTooltip (U ("METER: qué muestra el VU (entrada, compresión o salida)"));
    attack.slider.setTooltip (U ("ATTACK (en MANUAL): rápido = atrapa el golpe, lento = deja pasar el golpe"));
    release.slider.setTooltip (U ("RELEASE (en FIX./MAN. y MANUAL): qué tan rápido suelta"));
    select.slider.setTooltip (U ("FIXED: ataque y release automáticos según la música\nFIX./MAN.: ataque automático, release manual\nMANUAL: los dos a mano"));
    hpf.slider.setTooltip (U ("SIDECHAIN HPF: el compresor ignora los graves (no bombea con el bombo o el bajo)"));
    mode.slider.setTooltip (U ("LR: estéreo enlazado · M: solo el centro · S: solo los lados · Δ: escucha solo lo que el compresor quita"));
    mix.slider.setTooltip (U ("DRY/WET: compresión en paralelo"));
    color.slider.setTooltip (U ("COLOR: cantidad de armónicos de los transformadores y de la célula al comprimir (100 = normal)"));
    addAndMakeVisible (vu);
    addAndMakeVisible (power);
    powerAtt = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "power", power);
    power.setTooltip ("IN: encendido / bypass");
    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { if (presetBox.getSelectedId() > 0) proc.setCurrentProgram (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);
    buildBackground();
}

MainView::~MainView()
{
    for (auto* k : { &gain, &ratio, &thr, &meter, &attack, &release, &select, &hpf, &mode, &mix, &color })
        k->slider.setLookAndFeel (nullptr);
}

void MainView::buildBackground()
{
    background = juce::Image (juce::Image::ARGB, W, H, true);
    juce::Graphics g (background);
    g.fillAll (juce::Colour (0xff0d0e10));
    // barra superior
    brushed (g, { 0, 0, (float) W, (float) top - 2 }, juce::Colour (0xff202228), juce::Colour (0xff121316), 9, 0.5f);
    engraved (g, "TOP BLUE COMP", { 18, 4, 220, 28 }, 16, juce::Justification::centredLeft, true);
    engraved (g, "COLOR COMPRESSOR  -  BY DJ PAVA HM MUSIC", { 190, 4, 420, 28 }, 11, juce::Justification::centredLeft, false, Colours::engraveDim);
    engraved (g, "PRESET", { 1020, 4, 80, 28 }, 12, juce::Justification::centredRight, false, Colours::engraveDim);

    // panel frontal azul anodizado (pantalla completa, con orejas de rack)
    const juce::Rectangle<float> r (0, (float) top, (float) W, (float) (H - top));
    juce::ColourGradient grad (blueLo, r.getX(), r.getY(), blueLo.darker (0.15f), r.getRight(), r.getBottom(), false);
    grad.addColour (0.35, blueHi);
    grad.addColour (0.65, blueMid);
    g.setGradientFill (grad); g.fillRect (r);
    // cepillado muy fino horizontal
    juce::Random rng (737);
    for (float y = r.getY(); y < r.getBottom(); y += 1.0f)
    {
        g.setColour ((rng.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (0.012f + 0.02f * rng.nextFloat()));
        g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
    }
    // vineta suave en los bordes + biseles
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.07f), 0, r.getY(), juce::Colours::transparentWhite, 0, r.getY() + 70, false));
    g.fillRect (r);
    g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, 0, r.getBottom() - 80, juce::Colours::black.withAlpha (0.18f), 0, r.getBottom(), false));
    g.fillRect (r);
    g.setColour (juce::Colours::white.withAlpha (0.22f)); g.drawHorizontalLine ((int) r.getY() + 1, r.getX(), r.getRight());
    g.setColour (juce::Colours::black.withAlpha (0.55f)); g.drawHorizontalLine ((int) r.getBottom() - 1, r.getX(), r.getRight());

    // ranuras de las orejas y tornillos
    for (float x : { 34.0f, (float) W - 34.0f })
    {
        for (float y : { r.getY() + 92.0f, r.getBottom() - 92.0f })
        {
            auto slot = juce::Rectangle<float> (36, 20).withCentre ({ x, y });
            g.setColour (juce::Colours::white.withAlpha (0.12f)); g.fillRoundedRectangle (slot.translated (0, 1.5f), 10);
            g.setColour (juce::Colour (0xff0a0b0d)); g.fillRoundedRectangle (slot, 10);
        }
        screw (g, { x + (x < W / 2 ? 54.0f : -54.0f), r.getY() + 62.0f }, 9.0f);
        screw (g, { x + (x < W / 2 ? 54.0f : -54.0f), r.getBottom() - 62.0f }, 9.0f);
    }

    // serigrafia de la marca (propia)
    auto txt = [&] (const juce::String& t, juce::Rectangle<float> a, juce::Font f, juce::Justification j = juce::Justification::centredLeft)
    {
        g.setFont (f);
        g.setColour (juce::Colours::black.withAlpha (0.25f)); g.drawText (t, a.translated (0, 1), j);
        g.setColour (ink); g.drawText (t, a, j);
    };
    const float bx = 112;
    txt ("TOP-BLUE", { bx, r.getY() + 92, 300, 52 }, panelFont (50.0f, true).withExtraKerningFactor (-0.01f));
    txt ("COMPRESSOR     TBC 1", { bx + 2, r.getY() + 150, 300, 26 }, panelFont (22.0f, true));
    txt ("DJ PAVA     HM MUSIC", { bx + 2, r.getY() + 184, 300, 26 }, panelFont (22.0f, true));
    txt ("IN", { 160, (float) top + 282 - 58, 40, 18 }, panelFont (14.0f, true), juce::Justification::centred);
}

void MainView::resized()
{
    presetBox.setBounds (1110, 6, 260, 26);
    const int y1 = top + 106, y2 = top + 282;      // centros de las dos filas
    auto place = [] (PanelKnob& k, int cx, int cy, int w, int h, bool pointer)
    {
        // el centro de la perilla queda en (cx, cy)
        const int off = pointer ? 8 : 10;
        k.setBounds (cx - w / 2, cy - (h / 2 - off), w, h);
    };
    place (gain,    450, y1, 220, 186, false);
    place (ratio,   625, y1, 220, 186, false);
    place (thr,     800, y1, 220, 186, false);
    place (meter,   950, y1, 170, 186, true);
    place (attack,  450, y2, 220, 186, false);
    place (release, 625, y2, 220, 186, false);
    place (select,  800, y2, 180, 186, true);
    place (hpf,     955, y2, 180, 186, true);
    place (mode,    290, y2, 150, 186, true);
    place (color,  1078, y2, 120, 160, false);
    place (mix,    1196, y2, 120, 160, false);
    vu.setBounds (1030, top + 26, 250, 156);
    power.setBounds (160, top + 282 - 35, 40, 70);
}

void MainView::paint (juce::Graphics& g)
{
    g.drawImageAt (background, 0, 0);
    // lampara roja tipo joya con bisel cromado
    const bool on = proc.apvts.getRawParameterValue ("power")->load() > 0.5f;
    const juce::Point<float> c (1268.0f, (float) top + 282.0f);
    g.setColour (juce::Colours::black.withAlpha (0.4f)); g.fillEllipse (c.x - 19, c.y - 16, 40, 40);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff4f5f7), c.x - 12, c.y - 14, juce::Colour (0xff5d6168), c.x + 12, c.y + 16, false));
    g.fillEllipse (c.x - 19, c.y - 19, 38, 38);
    g.setColour (juce::Colour (0xff34373c)); g.drawEllipse (c.x - 19, c.y - 19, 38, 38, 1.0f);
    for (int i = 0; i < 16; ++i)  // estriado del bisel
    {
        const float a = (float) i * pi / 8.0f;
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        g.drawLine (juce::Line<float> (polar (c, 14.5f, a), polar (c, 18.5f, a)), 1.0f);
    }
    if (on) { g.setColour (juce::Colour (0xffff2a18).withAlpha (0.28f)); g.fillEllipse (c.x - 28, c.y - 28, 56, 56); }
    juce::ColourGradient jewel (on ? juce::Colour (0xffff9a80) : juce::Colour (0xff6a1d16), c.x - 4, c.y - 5,
                                on ? juce::Colour (0xffb5120a) : juce::Colour (0xff2a0806), c.x + 11, c.y + 11, true);
    g.setGradientFill (jewel); g.fillEllipse (c.x - 13, c.y - 13, 26, 26);
    // facetas
    g.setColour (juce::Colours::black.withAlpha (0.22f));
    for (int i = 0; i < 6; ++i)
        g.drawLine (juce::Line<float> (polar (c, 4.0f, (float) i * pi / 3.0f), polar (c, 12.5f, (float) i * pi / 3.0f)), 0.8f);
    g.setColour (juce::Colours::white.withAlpha (on ? 0.7f : 0.3f)); g.fillEllipse (c.x - 6, c.y - 8, 6, 4);
}

void MainView::tick()
{
    vu.tick();
    const bool on = proc.apvts.getRawParameterValue ("power")->load() > 0.5f;
    if (on != lastPower) { lastPower = on; repaint (1228, top + 252, 80, 80); }
    if (presetBox.getSelectedId() != proc.getCurrentProgram() + 1) presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
}

//==============================================================================
TopCompEditor::TopCompEditor (TopCompProcessor& p) : AudioProcessorEditor (&p), view (p)
{
    view.setLookAndFeel (&lnf);
    tips.setLookAndFeel (&lnf);
    addAndMakeVisible (view);
    setResizable (true, true);
    setResizeLimits (MainView::W / 2, MainView::H / 2, MainView::W * 3 / 2, MainView::H * 3 / 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) MainView::W / MainView::H);
    setSize (MainView::W * 8 / 10, MainView::H * 8 / 10);
    startTimerHz (30);
}
TopCompEditor::~TopCompEditor() { stopTimer(); tips.setLookAndFeel (nullptr); view.setLookAndFeel (nullptr); }
void TopCompEditor::paint (juce::Graphics& g) { g.fillAll (juce::Colour (0xff0d0e10)); }
void TopCompEditor::resized()
{
    view.setBounds (0, 0, MainView::W, MainView::H);
    view.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) MainView::W));
}
