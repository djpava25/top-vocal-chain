#include "TopMicEditor.h"
#include "MicArt.h"

using namespace tmicui;

namespace
{
    juce::String U (const char* s) { return juce::String::fromUTF8 (s); }
    juce::Font font (float h, bool bold = false) { return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain)); }
    constexpr float A0 = juce::MathConstants<float>::pi * 1.25f, A1 = juce::MathConstants<float>::pi * 2.75f;
    const juce::Colour bgA (0xff1d1c1f), bgB (0xff111114), ink (0xffece7df), dim (0xffa49d92), amber (0xfff2a13a), teal (0xff5fe0c0), grid (0xff5b6268);
    float pv (TopMicProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); }
    void setPv (TopMicProcessor& p, const juce::String& id, float v) { if (auto* q = p.apvts.getParameter (id)) { q->beginChangeGesture(); q->setValueNotifyingHost (q->convertTo0to1 (v)); q->endChangeGesture(); } }
    void panelBox (juce::Graphics& g, juce::Rectangle<float> r, float rad = 8.0f)
    {
        g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (r.translated (0, 2), rad);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff27262a), r.getX(), r.getY(), juce::Colour (0xff18181b), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, rad);
        g.setColour (juce::Colours::white.withAlpha (0.06f)); g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.0f);
    }
}

Look::Look()
{
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1e1d21));
    setColour (juce::PopupMenu::textColourId, ink);
    setColour (juce::PopupMenu::headerTextColourId, amber);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff4a3a28));
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff1e1d21));
    setColour (juce::TooltipWindow::textColourId, ink);
}
void Look::drawBubble (juce::Graphics& g, juce::BubbleComponent&, const juce::Point<float>&, const juce::Rectangle<float>& body)
{
    g.setColour (juce::Colour (0xf0151417)); g.fillRoundedRectangle (body, 4);
    g.setColour (amber.withAlpha (0.6f)); g.drawRoundedRectangle (body.reduced (0.5f), 4, 1.0f);
}
void Look::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&)
{
    const juce::Point<float> c ((float) x + (float) w * 0.5f, (float) y + (float) h * 0.5f);
    const float R = (float) juce::jmin (w, h) * 0.5f - 5.0f, a = a0 + pos * (a1 - a0);
    // arco de valor
    juce::Path tr; tr.addCentredArc (c.x, c.y, R + 3.5f, R + 3.5f, 0, a0, a1, true);
    g.setColour (juce::Colours::black.withAlpha (0.6f)); g.strokePath (tr, juce::PathStrokeType (2.6f));
    juce::Path vr; vr.addCentredArc (c.x, c.y, R + 3.5f, R + 3.5f, 0, a0, a, true);
    g.setColour (amber.withAlpha (0.85f)); g.strokePath (vr, juce::PathStrokeType (2.2f));
    for (int i = 5; i > 0; --i) { g.setColour (juce::Colours::black.withAlpha (0.10f)); g.fillEllipse (c.x - R - (float) i * 0.4f, c.y - R + (float) i * 1.0f, (R + (float) i * 0.4f) * 2, (R + (float) i * 0.5f) * 2); }
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a393d), c.x, c.y - R, juce::Colour (0xff060607), c.x, c.y + R, false));
    g.fillEllipse (c.x - R, c.y - R, R * 2, R * 2);
    for (int i = 0; i < 44; ++i)
    {
        const float t = (float) i / 44.0f * juce::MathConstants<float>::twoPi;
        g.setColour (juce::Colours::white.withAlpha (i % 2 ? 0.05f : 0.0f));
        g.drawLine (juce::Line<float> ({ c.x + R * std::sin (t), c.y - R * std::cos (t) }, { c.x + R * 0.84f * std::sin (t), c.y - R * 0.84f * std::cos (t) }), 1.0f);
    }
    const float rc = R * 0.66f;
    juce::ColourGradient cg (juce::Colour (0xfff7f7f6), c.x - rc * 0.4f, c.y - rc * 0.8f, juce::Colour (0xff76777a), c.x + rc * 0.5f, c.y + rc, true);
    cg.addColour (0.5, juce::Colour (0xffc4c5c8));
    g.setGradientFill (cg); g.fillEllipse (c.x - rc, c.y - rc, rc * 2, rc * 2);
    for (int i = 1; i < 6; ++i) { const float rr = rc * (float) i / 6.0f; g.setColour (juce::Colours::black.withAlpha (0.04f)); g.drawEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2, 0.6f); }
    g.setColour (juce::Colours::black.withAlpha (0.5f)); g.drawEllipse (c.x - rc, c.y - rc, rc * 2, rc * 2, 0.8f);
    g.setColour (juce::Colour (0xff111111));
    g.drawLine (juce::Line<float> ({ c.x + rc * 0.15f * std::sin (a), c.y - rc * 0.15f * std::cos (a) }, { c.x + R * 0.95f * std::sin (a), c.y - R * 0.95f * std::cos (a) }), juce::jmax (2.2f, R * 0.09f));
    g.setColour (juce::Colours::white); g.drawLine (juce::Line<float> ({ c.x + rc * 1.02f * std::sin (a), c.y - rc * 1.02f * std::cos (a) }, { c.x + R * 0.95f * std::sin (a), c.y - R * 0.95f * std::cos (a) }), 1.6f);
}

Knob::Knob (APVTS& s, const juce::String& id)
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (A0, A1, true);
    setPopupDisplayEnabled (true, true, nullptr, 1000);
    setMouseDragSensitivity (220);
    att = std::make_unique<APVTS::SliderAttachment> (s, id, *this);
    if (auto* p = s.getParameter (id)) setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
}

Toggle::Toggle (APVTS& s, const juce::String& id, const juce::String& label, int ic) : juce::Button (label), icon (ic)
{
    setClickingTogglesState (true);
    att = std::make_unique<APVTS::ButtonAttachment> (s, id, *this);
}
void Toggle::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (2);
    const bool on = getToggleState();
    g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillRoundedRectangle (r.translated (0, 1.5f), 4.0f);
    g.setGradientFill (juce::ColourGradient (on ? juce::Colour (0xff4b3a24) : juce::Colour (over ? 0xff38373c : 0xff2e2d31), r.getX(), r.getY(),
                                             on ? juce::Colour (0xff2e2316) : juce::Colour (0xff1c1b1e), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r.translated (0, down ? 1.0f : 0.0f), 4.0f);
    g.setColour (on ? amber.withAlpha (0.9f) : juce::Colours::black.withAlpha (0.6f)); g.drawRoundedRectangle (r, 4.0f, 1.0f);
    g.setColour (on ? amber : dim);
    const auto c = r.getCentre();
    if (icon == 1) { g.drawEllipse (c.x - 6, c.y - 6, 12, 12, 1.6f); g.drawLine (c.x - 8, c.y + 8, c.x + 8, c.y - 8, 1.6f); }
    else if (icon == 2)
    {
        juce::Path p; p.addCentredArc (c.x, c.y + 1, 6, 6, 0, 0.6f, juce::MathConstants<float>::twoPi - 0.6f, true);
        g.strokePath (p, juce::PathStrokeType (1.7f)); g.drawLine (c.x, c.y - 7, c.x, c.y - 1, 1.7f);
    }
    else { g.setFont (font (12.0f, true)); g.drawText (getButtonText(), r, juce::Justification::centred); }
}

void Choice::paint (juce::Graphics& g)
{
    const int n = names.size(), act = shown ? shown() : (int) pv (proc, pid.toRawUTF8());
    const float w = (float) getWidth() / (float) n;
    for (int i = 0; i < n; ++i)
    {
        auto r = juce::Rectangle<float> ((float) i * w, 0, w, (float) getHeight()).reduced (1.5f);
        const bool ok = ! enabled || enabled (i), on = i == act;
        g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (r.translated (0, 1.2f), 3.5f);
        g.setGradientFill (juce::ColourGradient (on ? juce::Colour (0xff5a4428) : juce::Colour (0xff2c2b2f), r.getX(), r.getY(), on ? juce::Colour (0xff35281a) : juce::Colour (0xff1b1a1d), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 3.5f);
        g.setColour (on ? amber : juce::Colours::black.withAlpha (0.6f)); g.drawRoundedRectangle (r, 3.5f, 1.0f);
        g.setColour (on ? amber : ok ? ink.withAlpha (0.85f) : dim.withAlpha (0.3f));
        g.setFont (font (10.5f, true)); g.drawFittedText (names[i], r.toNearestInt(), juce::Justification::centred, 1, 0.7f);
    }
}
void Choice::mouseUp (const juce::MouseEvent& e)
{
    const int i = juce::jlimit (0, names.size() - 1, (int) ((float) e.x / ((float) getWidth() / (float) names.size())));
    if (! enabled || enabled (i)) { setPv (proc, pid, (float) i); repaint(); }
}

void Display::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1);
    g.setColour (juce::Colour (0xff0b0a0c)); g.fillRoundedRectangle (r, 4);
    g.setColour (juce::Colours::black); g.drawRoundedRectangle (r, 4, 1.4f);
    g.setColour (juce::Colours::white.withAlpha (0.06f)); g.drawHorizontalLine ((int) r.getBottom() - 1, r.getX() + 3, r.getRight() - 3);
    g.setColour (isEnabled() ? amber : dim.withAlpha (0.5f)); g.setFont (font (fontSize, true));
    g.drawFittedText (text, r.reduced (6, 0).toNearestInt(), juce::Justification::centred, 1, 0.75f);
}

//============================================================================================ vitrina
namespace
{
    // dibuja un microfono en el rectangulo (con su placa "HM" y su nombre grabado)
    void drawMic (juce::Graphics& g, juce::Rectangle<float> area, int model, float alpha = 1.0f)
    {
        const auto d = micart::designFor (model);
        const float mx = 26, my = 30, unitsW = d.halfW * 2 + mx * 2, unitsH = d.height + my * 2;
        const float s = juce::jmin (area.getWidth() / unitsW, area.getHeight() / unitsH);
        const float k = s * 1.6f < 1.6f ? 1.6f : juce::jmin (2.4f, s * 1.6f);
        const auto& img = micart::micImage (model, k);
        const juce::Rectangle<float> dst (area.getCentreX() - unitsW * s * 0.5f, area.getBottom() - unitsH * s, unitsW * s, unitsH * s);
        g.setOpacity (alpha);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (img, dst);
        // placa: "HM" dorado y el modelo grabado debajo
        if (d.badgeY > 0)
        {
            const float bx = dst.getX() + (d.halfW + mx) * s, by = dst.getY() + (my + d.badgeY) * s;
            g.setColour (juce::Colour (0xffe7c57a).withMultipliedAlpha (alpha)); g.setFont (font (9.5f * s, true));
            g.drawText ("HM", juce::Rectangle<float> (bx - 12 * s, by - 7 * s, 24 * s, 14 * s), juce::Justification::centred);
            const juce::String nm = tmic::models()[(size_t) model].name;
            g.setFont (font (7.5f * s, true).withExtraKerningFactor (0.08f));
            g.setColour (juce::Colours::black.withAlpha (0.45f * alpha)); g.drawText (nm, juce::Rectangle<float> (bx - 30 * s, by + 15 * s + 0.8f, 60 * s, 10 * s), juce::Justification::centred);
            g.setColour (juce::Colours::white.withAlpha (0.55f * alpha)); g.drawText (nm, juce::Rectangle<float> (bx - 30 * s, by + 15 * s, 60 * s, 10 * s), juce::Justification::centred);
        }
        g.setOpacity (1.0f);
    }
}
void Showcase::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    // fondo de estudio: pared oscura, luz de arriba y piso que refleja
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2826), r.getCentreX(), r.getY() + 60, juce::Colour (0xff0c0b0b), r.getX(), r.getBottom(), true));
    g.fillRoundedRectangle (r, 10);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.13f), r.getCentreX(), r.getY() + 90, juce::Colours::transparentWhite, r.getCentreX() + 170, r.getY() + 260, true));
    g.fillRoundedRectangle (r, 10);
    const float floorY = r.getY() + r.getHeight() * 0.80f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0x33000000), 0, floorY, juce::Colour (0xaa000000), 0, r.getBottom(), false));
    g.fillRect (r.withTop (floorY).reduced (1, 0));
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.55f), r.getCentreX(), floorY + 4, juce::Colours::transparentBlack, r.getCentreX() + 110, floorY + 4, true));
    g.fillEllipse (r.getCentreX() - 110, floorY - 10, 220, 28);
    const int m = (int) pv (proc, "model");
    const bool dual = pv (proc, "dual") > 0.5f;
    auto micArea = juce::Rectangle<float> (r.getX() + 40, r.getY() + 14, r.getWidth() - 80, floorY - r.getY() - 2);
    if (dual)
    {
        drawMic (g, micArea.withTrimmedLeft (micArea.getWidth() * 0.45f).withTrimmedTop (micArea.getHeight() * 0.28f).translated (18, 0), (int) pv (proc, "modelB"), 0.95f);
        drawMic (g, micArea.withTrimmedRight (micArea.getWidth() * 0.30f).translated (-14, 0), m);
        g.setColour (teal); g.setFont (font (12, true)); g.drawText ("B", juce::Rectangle<float> (r.getRight() - 46, floorY - 26, 20, 16), juce::Justification::centred);
        g.setColour (amber); g.drawText ("A", juce::Rectangle<float> (r.getX() + 30, floorY - 26, 20, 16), juce::Justification::centred);
    }
    else drawMic (g, micArea, m);
    // flechas
    g.setColour (juce::Colours::white.withAlpha (0.75f));
    const float ay = r.getY() + r.getHeight() * 0.42f;
    juce::Path la; la.startNewSubPath (r.getX() + 22, ay - 12); la.lineTo (r.getX() + 12, ay); la.lineTo (r.getX() + 22, ay + 12);
    juce::Path ra; ra.startNewSubPath (r.getRight() - 22, ay - 12); ra.lineTo (r.getRight() - 12, ay); ra.lineTo (r.getRight() - 22, ay + 12);
    g.strokePath (la, juce::PathStrokeType (2.4f)); g.strokePath (ra, juce::PathStrokeType (2.4f));
    // nombre y descripcion
    const auto& mod = tmic::models()[(size_t) m];
    auto info = r.withTop (floorY + 8).reduced (18, 4);
    g.setColour (ink); g.setFont (font (26, true).withExtraKerningFactor (0.06f));
    g.drawText (mod.name, info.removeFromTop (30), juce::Justification::centredLeft);
    g.setColour (amber); g.setFont (font (11.5f, true).withExtraKerningFactor (0.12f));
    g.drawText (U (mod.kind), juce::Rectangle<float> (info.getRight() - 120, info.getY() - 26, 120, 18), juce::Justification::centredRight);
    g.setColour (dim); g.setFont (font (11.5f));
    g.drawFittedText (U (mod.desc), info.toNearestInt(), juce::Justification::topLeft, 3, 0.9f);
    g.setColour (juce::Colours::white.withAlpha (0.08f)); g.drawRoundedRectangle (r.reduced (0.5f), 10, 1.0f);
}
void Showcase::mouseUp (const juce::MouseEvent& e)
{
    const int n = (int) tmic::models().size(), cur = (int) pv (proc, "model");
    const float floorY = (float) getHeight() * 0.80f;
    if ((float) e.y > floorY && onName) { onName(); return; }
    int next = -1;
    if (e.x < 60) next = (cur - 1 + n) % n;
    else if (e.x > getWidth() - 60) next = (cur + 1) % n;
    if (next >= 0) setPv (proc, "model", (float) next);
}

//============================================================================================ grafica de respuesta
void ResponseView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    panelBox (g, r);
    auto plot = r.reduced (44, 26).withTrimmedBottom (4).withTrimmedRight (-20);
    g.setColour (juce::Colour (0xff0b0b0d)); g.fillRoundedRectangle (plot.expanded (6, 6), 4);
    const double fs = proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0;
    const float dbR = 15.0f;
    auto X = [&] (double f) { return plot.getX() + plot.getWidth() * (float) (std::log (f / 20.0) / std::log (1000.0)); };
    auto Y = [&] (double db) { return plot.getCentreY() - plot.getHeight() * 0.5f * (float) juce::jlimit (-1.2, 1.2, db / dbR); };
    g.setFont (font (9.5f));
    for (double f : { 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0 })
    {
        g.setColour (grid.withAlpha (0.35f)); g.drawVerticalLine ((int) X (f), plot.getY(), plot.getBottom());
        g.setColour (dim); g.drawText (f >= 1000 ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f), juce::Rectangle<float> (X (f) - 16, plot.getBottom() + 4, 32, 12), juce::Justification::centred);
    }
    for (int db = -12; db <= 12; db += 6)
    {
        g.setColour (grid.withAlpha (db == 0 ? 0.7f : 0.3f)); g.drawHorizontalLine ((int) Y (db), plot.getX(), plot.getRight());
        g.setColour (dim); g.drawText ((db > 0 ? "+" : "") + juce::String (db), juce::Rectangle<float> (plot.getX() - 34, Y (db) - 6, 28, 12), juce::Justification::centredRight);
    }
    const auto sA = proc.settings (false), sB = proc.settings (true);
    const auto& m = tmic::models()[(size_t) sA.model];
    const int pat = tmic::nearestPattern (m, sA.pattern);
    auto path = [&] (const std::function<double (double)>& fn)
    {
        juce::Path p;
        for (int i = 0; i <= 220; ++i) { const double f = 20.0 * std::pow (1000.0, i / 220.0); const juce::Point<float> pt (X (f), Y (fn (f))); if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt); }
        return p;
    };
    // origen (lo que se quita), destino (el micro elegido) y resultado (lo que sale)
    const auto& src = tmic::sources()[(size_t) sA.source];
    if (! src.curve.empty())
    {
        juce::Path ps = path ([&] (double f) { return tmic::sourceDb (src, f); });
        juce::Path dashed; const float dl[] { 5.0f, 4.0f };
        juce::PathStrokeType (1.4f).createDashedStroke (dashed, ps, dl, 2);
        g.setColour (juce::Colour (0xff8aa0b0)); g.fillPath (dashed);
    }
    g.setColour (ink.withAlpha (0.55f)); g.strokePath (path ([&] (double f) { return tmic::modelDb (m, pat, f); }), juce::PathStrokeType (1.4f));
    if (pv (proc, "dual") > 0.5f) { g.setColour (teal); g.strokePath (path ([&] (double f) { return tmic::responseDb (sB, fs, f); }), juce::PathStrokeType (2.0f)); }
    juce::Path res = path ([&] (double f) { return tmic::responseDb (sA, fs, f); });
    g.setColour (amber.withAlpha (0.25f)); g.strokePath (res, juce::PathStrokeType (6.0f));
    g.setColour (amber); g.strokePath (res, juce::PathStrokeType (2.2f));
    // leyenda
    auto leg = juce::Rectangle<float> (plot.getX() + 8, plot.getY() + 6, 600, 14);
    auto item = [&] (juce::Colour c, const juce::String& t, bool dash)
    {
        g.setColour (c);
        if (dash) { for (int i = 0; i < 3; ++i) g.fillRect (leg.getX() + (float) i * 6, leg.getCentreY() - 0.7f, 4.0f, 1.4f); }
        else g.fillRect (leg.getX(), leg.getCentreY() - 1.0f, 16.0f, 2.0f);
        g.setColour (dim); g.setFont (font (10.5f, true)); g.drawText (t, leg.withTrimmedLeft (22), juce::Justification::centredLeft);
        leg.removeFromLeft (22 + juce::GlyphArrangement::getStringWidth (font (10.5f, true), t) + 16);
    };
    item (amber, U ("RESULTADO"), false);
    item (ink.withAlpha (0.55f), juce::String (m.name) + U (" (medido)"), false);
    if (! src.curve.empty()) item (juce::Colour (0xff8aa0b0), U ("ORIGEN: ") + src.shortName, true);
    if (pv (proc, "dual") > 0.5f) item (teal, U ("MIC B"), false);
    g.setColour (dim); g.setFont (font (10.5f, true)); g.drawText ("dB", juce::Rectangle<float> (r.getX() + 8, r.getY() + 6, 30, 14), juce::Justification::centredLeft);
}

//============================================================================================ polar
void PolarView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    panelBox (g, r);
    auto scr = r.reduced (12);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1c2226), scr.getCentreX(), scr.getCentreY(), juce::Colour (0xff060708), scr.getX(), scr.getY(), true));
    g.fillEllipse (scr);
    const auto c = scr.getCentre(); const float R = scr.getHeight() * 0.42f;
    for (int k = 0; k <= 4; ++k)
    {
        const float rr = R * (1.0f - (float) k / 4.0f);
        g.setColour (grid.withAlpha (k == 0 ? 0.7f : 0.3f)); g.drawEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2, 0.8f);
        if (k > 0 && k < 4) { g.setFont (font (8.5f)); g.drawText (juce::String (-k * 6), juce::Rectangle<float> (c.x - 12, c.y - rr - 1, 24, 10), juce::Justification::centred); }
    }
    for (int d = 0; d < 360; d += 30)
    {
        const float a = (float) d * juce::MathConstants<float>::pi / 180.0f;
        g.setColour (grid.withAlpha (0.25f)); g.drawLine (c.x, c.y, c.x + R * std::sin (a), c.y - R * std::cos (a), 0.7f);
    }
    auto curve = [&] (bool b, juce::Colour col, float width)
    {
        const auto s = proc.settings (b);
        const auto& m = tmic::models()[(size_t) s.model];
        const int pat = tmic::nearestPattern (m, s.pattern);
        juce::Path p;
        for (int d = 0; d <= 360; d += 2)
        {
            const float gdb = 20.0f * std::log10 (std::max (1.0e-3f, std::abs (tmic::polarGain (m, pat, (float) d))));
            const float rr = R * juce::jlimit (0.0f, 1.0f, (gdb + 24.0f) / 24.0f);
            const float a = (float) d * juce::MathConstants<float>::pi / 180.0f;
            const juce::Point<float> pt (c.x + rr * std::sin (a), c.y - rr * std::cos (a));
            if (d == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        p.closeSubPath();
        g.setColour (col.withAlpha (0.12f)); g.fillPath (p);
        g.setColour (col); g.strokePath (p, juce::PathStrokeType (width));
    };
    if (pv (proc, "dual") > 0.5f) curve (true, teal, 1.5f);
    curve (false, amber, 2.0f);
    // la voz: angulo y distancia (mas cerca = punto mas adentro)
    const bool rev = pv (proc, "rev") > 0.5f;
    const float ax = pv (proc, "axis"), dist = pv (proc, "dist") * std::pow (2.0f, -pv (proc, "prox") / 50.0f);
    const float a = (rev ? 180.0f - ax : ax) * juce::MathConstants<float>::pi / 180.0f;
    const float rr = R * juce::jlimit (0.25f, 1.08f, 0.25f + 0.83f * std::log (dist / 1.0f) / std::log (100.0f));
    const juce::Point<float> sp (c.x + rr * std::sin (a), c.y - rr * std::cos (a));
    g.setColour (juce::Colours::white.withAlpha (0.3f)); g.drawLine (c.x, c.y, sp.x, sp.y, 1.0f);
    g.setColour (juce::Colours::white); g.fillEllipse (sp.x - 4.5f, sp.y - 4.5f, 9, 9);
    g.setColour (amber); g.setFont (font (10.5f, true));
    g.drawText (tmic::patternName (tmic::nearestPattern (tmic::models()[(size_t) pv (proc, "model")], (int) pv (proc, "pattern"))), r.withHeight (18).translated (0, 6), juce::Justification::centred);
    g.setColour (dim); g.setFont (font (10.0f));
    g.drawText (juce::String (dist, dist < 10 ? 1 : 0) + " cm  ·  " + juce::String ((int) ax) + U ("°"), r.withTop (r.getBottom() - 22), juce::Justification::centred);
}

//============================================================================================ editor
TopMicEditor::TopMicEditor (TopMicProcessor& p)
    : AudioProcessorEditor (&p), proc (p), show (p), resp (p), polar (p),
      pattern (p, "pattern", { "OMNI", "WIDE", "CARD", "HYPER", "FIG-8" }), filter (p, "filter", { "OFF", "60 Hz", "100 Hz", "160 Hz" }),
      dist (p.apvts, "dist"), prox (p.apvts, "prox"), axis (p.apvts, "axis"), out (p.apvts, "out"), blend (p.apvts, "blend"),
      rev (p.apvts, "rev", "REV"), phase (p.apvts, "phase", {}, 1), power (p.apvts, "power", {}, 2), dual (p.apvts, "dual", "DUAL")
{
    setLookAndFeel (&look);
    tips.setLookAndFeel (&look);
    show.setTooltip (U ("Flechas: cambiar de micrófono. Clic en el nombre: lista completa"));
    show.onName = [this] { modelMenu ("model", show); };
    pattern.enabled = [this] (int i) { return (tmic::models()[(size_t) pv (proc, "model")].patterns >> i & 1u) != 0; };
    pattern.shown = [this] { return proc.effectivePattern (false); };
    pattern.setTooltip (U ("Patrón polar (solo los que tiene el micrófono elegido; en los multipatrón cambia también el timbre)"));
    filter.setTooltip (U ("Filtro de graves del micrófono"));
    sourceBox.setTooltip (U ("¿Con qué micrófono grabaste? Su color se quita con su curva invertida antes de aplicar el modelo"));
    sourceBox.onClick = [this]
    {
        juce::PopupMenu m; m.addSectionHeader (U ("¿Con qué micrófono grabaste?"));
        const int cur = (int) pv (proc, "source");
        for (int i = 0; i < (int) tmic::sources().size(); ++i) m.addItem (i + 1, U (tmic::sources()[(size_t) i].name), true, i == cur);
        juce::Component::SafePointer<TopMicEditor> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&sourceBox), [safe] (int r) { if (safe != nullptr && r > 0) setPv (safe->proc, "source", (float) (r - 1)); });
    };
    micBBox.setTooltip (U ("Micrófono B del modo DUAL (curva verde)"));
    micBBox.onClick = [this] { modelMenu ("modelB", micBBox); };
    presetBox.fontSize = 12.5f;
    presetBox.setTooltip ("Presets");
    presetBox.onClick = [this]
    {
        juce::PopupMenu m;
        for (int i = 0; i < proc.getNumPrograms(); ++i) m.addItem (i + 1, proc.getProgramName (i), true, i == proc.getCurrentProgram());
        juce::Component::SafePointer<TopMicEditor> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetBox), [safe] (int r) { if (safe != nullptr && r > 0) safe->proc.setCurrentProgram (r - 1); });
    };
    dist.setTooltip (U ("DISTANCIA: a cuántos cm del micrófono grabaste (ajusta la proximidad del micro de origen)"));
    prox.setTooltip (U ("CERCANÍA: acerca (+) o aleja (−) el micrófono virtual; +100 = 4 veces más cerca"));
    axis.setTooltip (U ("EJE: ángulo de la voz respecto al micrófono (fuera de eje = menos agudos y, de costado, menos proximidad)"));
    out.setTooltip ("SALIDA"); blend.setTooltip (U ("Mezcla entre el micrófono A y el B"));
    rev.setTooltip (U ("REV: cantar por la parte de atrás del micrófono"));
    phase.setTooltip ("Invertir fase"); power.setTooltip ("Encender / apagar"); dual.setTooltip (U ("DUAL: dos micrófonos a la vez"));
    for (juce::Component* c : { (juce::Component*) &show, (juce::Component*) &resp, (juce::Component*) &polar, (juce::Component*) &sourceBox,
                                (juce::Component*) &micBBox, (juce::Component*) &presetBox, (juce::Component*) &pattern, (juce::Component*) &filter,
                                (juce::Component*) &dist, (juce::Component*) &prox, (juce::Component*) &axis, (juce::Component*) &out, (juce::Component*) &blend,
                                (juce::Component*) &rev, (juce::Component*) &phase, (juce::Component*) &power, (juce::Component*) &dual })
        addAndMakeVisible (c);
    setResizable (true, true);
    setResizeLimits (W / 2, H / 2, W * 3 / 2, H * 3 / 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) W / H);
    setSize (W * 9 / 10, H * 9 / 10);
    startTimerHz (30);
}
TopMicEditor::~TopMicEditor() { stopTimer(); tips.setLookAndFeel (nullptr); setLookAndFeel (nullptr); }

void TopMicEditor::modelMenu (const juce::String& id, juce::Component& target)
{
    juce::PopupMenu m;
    const int cur = (int) pv (proc, id.toRawUTF8());
    auto group = [] (int look) { return look == tmic::TubeLdc || look == tmic::GoldTube ? 0 : look == tmic::FetLdc ? 1 : look <= tmic::LongDyn ? 2 : 3; };
    const juce::StringArray gn { U ("Válvula"), "FET", U ("Dinámicos"), "Cinta" };
    for (int gi = 0; gi < 4; ++gi)
    {
        m.addSectionHeader (gn[gi]);
        for (int i = 0; i < (int) tmic::models().size(); ++i)
            if (group (tmic::models()[(size_t) i].look) == gi)
                m.addItem (i + 1, juce::String (tmic::models()[(size_t) i].name) + "  -  " + U (tmic::models()[(size_t) i].desc), true, i == cur);
    }
    juce::Component::SafePointer<TopMicEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&target), [safe, id] (int r) { if (safe != nullptr && r > 0) setPv (safe->proc, id, (float) (r - 1)); });
}

void TopMicEditor::resized()
{
    const float s = (float) getWidth() / (float) W;
    const auto T = juce::AffineTransform::scale (s);
    auto put = [&] (juce::Component& c, int x, int y, int w, int h) { c.setBounds (x, y, w, h); c.setTransform (T); };
    put (show, 16, 64, 380, 580);
    put (resp, 412, 64, 672, 240);
    put (polar, 412, 316, 228, 228);
    put (sourceBox, 662, 338, 200, 30);
    put (pattern, 874, 338, 208, 30);
    put (dist, 664, 394, 74, 74); put (prox, 768, 394, 74, 74); put (axis, 872, 394, 74, 74); put (out, 976, 394, 74, 74);
    put (filter, 662, 520, 200, 28);
    put (rev, 874, 520, 64, 28); put (phase, 946, 520, 64, 28); put (power, 1018, 520, 64, 28);
    put (dual, 412, 600, 70, 30); put (micBBox, 490, 600, 150, 30); put (blend, 662, 580, 56, 56);
    put (presetBox, 840, 16, 244, 30);
}

void TopMicEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale ((float) getWidth() / (float) W));
    g.setGradientFill (juce::ColourGradient (bgA, 0, 0, bgB, 0, (float) H, false));
    g.fillAll();
    juce::Random rng (7);
    for (int i = 0; i < 7000; ++i) { g.setColour ((rng.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (0.025f)); g.fillRect (rng.nextFloat() * W, rng.nextFloat() * H, 1.0f, 1.0f); }
    // cabecera: modelo del plugin y marca propia
    g.setColour (ink); g.setFont (font (24.0f, true).withExtraKerningFactor (0.10f));
    g.drawText ("TOP MIC", 20, 14, 140, 34, juce::Justification::centredLeft);
    g.setColour (dim); g.setFont (font (12.0f, true).withExtraKerningFactor (0.18f));
    g.drawText ("COLLECTION", 140, 22, 140, 22, juce::Justification::centredLeft);
    g.setColour (dim.withAlpha (0.5f)); g.fillRect (262.0f, 18.0f, 1.0f, 28.0f);
    g.setColour (amber); g.drawEllipse (276.0f, 18.0f, 28.0f, 28.0f, 2.0f);
    g.setFont (font (11.5f, true)); g.drawText ("HM", 276, 18, 28, 28, juce::Justification::centred);
    g.setColour (ink); g.setFont (font (15.0f, true)); g.drawText ("AnalogHM", 312, 16, 140, 18, juce::Justification::centredLeft);
    g.setColour (dim); g.setFont (font (10.5f)); g.drawText ("Dj Pava Hm Music", 312, 33, 160, 14, juce::Justification::centredLeft);
    g.setColour (dim); g.setFont (font (10.0f, true)); g.drawText ("PRESET", 770, 24, 64, 14, juce::Justification::centredRight);
    // panel de controles
    panelBox (g, { 652, 316, 432, 238 });
    auto label = [&g] (const juce::String& t, float x, float y, float w) { g.setColour (dim); g.setFont (font (10.5f, true).withExtraKerningFactor (0.06f)); g.drawText (t, juce::Rectangle<float> (x, y, w, 14), juce::Justification::centred); };
    label (U ("MIC DE ORIGEN"), 662, 322, 200); label (U ("PATRÓN"), 874, 322, 208);
    label (U ("DISTANCIA"), 651, 470, 100); label (U ("CERCANÍA"), 755, 470, 100); label ("EJE", 859, 470, 100); label ("SALIDA", 963, 470, 100);
    {
        auto val = [&g] (const juce::String& t, float x) { g.setColour (amber.withAlpha (0.9f)); g.setFont (font (10.5f)); g.drawText (t, juce::Rectangle<float> (x, 484, 100, 13), juce::Justification::centred); };
        const float pr = pv (proc, "prox"), o = pv (proc, "out");
        val (juce::String (pv (proc, "dist"), 1) + " cm", 651);
        val ((pr > 0 ? "+" : "") + juce::String ((int) pr) + U (" %"), 755);
        val (juce::String ((int) pv (proc, "axis")) + U ("°"), 859);
        val ((o > 0 ? "+" : "") + juce::String (o, 1) + " dB", 963);
        g.setColour (amber.withAlpha (0.9f)); g.setFont (font (10.5f));
        g.drawText (juce::String ((int) pv (proc, "blend")) + " % B", juce::Rectangle<float> (720, 604, 60, 14), juce::Justification::centredLeft);
    }
    label ("FILTRO", 662, 504, 200);
    // panel dual + medidor
    panelBox (g, { 412, 556, 672, 88 });
    label ("MIC B", 490, 582, 150); label ("A / B", 650, 562, 80);
    g.setColour (dim); g.setFont (font (10.0f, true)); g.drawText ("DUAL", 412, 582, 70, 14, juce::Justification::centred);
    g.setColour (dim); g.setFont (font (10.0f, true)); g.drawText ("ENTRADA", 740, 586, 70, 14, juce::Justification::centredLeft);
    const auto mr = juce::Rectangle<float> (806, 588, 262, 10);
    g.setColour (juce::Colour (0xff09090a)); g.fillRoundedRectangle (mr, 2);
    const float posm = juce::jlimit (0.0f, 1.0f, (20.0f * std::log10 (std::max (1.0e-6f, meter)) + 60.0f) / 60.0f);
    g.setGradientFill (juce::ColourGradient (teal.darker (0.3f), mr.getX(), 0, posm > 0.95f ? juce::Colour (0xffff5a4a) : teal, mr.getRight(), 0, false));
    g.fillRoundedRectangle (mr.reduced (1.5f).withWidth ((mr.getWidth() - 3) * posm), 1.5f);
    g.setColour (dim); g.setFont (font (8.5f));
    for (int mk : { 60, 48, 36, 24, 18, 12, 6, 0 }) { const float x = mr.getX() + mr.getWidth() * (1.0f - (float) mk / 60.0f); g.drawText (juce::String (-mk), juce::Rectangle<float> (x - 10, 602, 20, 10), juce::Justification::centred); }
}

void TopMicEditor::timerCallback()
{
    const auto src = tmic::sources()[(size_t) pv (proc, "source")];
    const juce::String st = U (src.name);
    if (sourceBox.text != st) { sourceBox.text = st; sourceBox.repaint(); }
    const bool dualOn = pv (proc, "dual") > 0.5f;
    const juce::String mb = juce::String (tmic::models()[(size_t) pv (proc, "modelB")].name);
    if (micBBox.text != mb) { micBBox.text = mb; micBBox.repaint(); }
    blend.setEnabled (dualOn); micBBox.setEnabled (dualOn);
    const auto pn = proc.getProgramName (proc.getCurrentProgram());
    if (presetBox.text != pn) { presetBox.text = pn; presetBox.repaint(); }
    // redibuja solo si cambio algo de lo que se ve
    juce::int64 h = 17;
    for (const char* id : { "model", "source", "pattern", "filter", "axis", "prox", "dist", "rev", "dual", "modelB", "out", "blend" }) h = h * 1000003 + (juce::int64) std::lround (pv (proc, id) * 100.0f);
    if (h != lastHash)
    {
        lastHash = h; show.repaint(); resp.repaint(); polar.repaint(); pattern.repaint(); filter.repaint();
        repaint (juce::Rectangle<int> (652, 466, 432, 34).transformedBy (juce::AffineTransform::scale ((float) getWidth() / (float) W)));
        repaint (juce::Rectangle<int> (716, 600, 70, 20).transformedBy (juce::AffineTransform::scale ((float) getWidth() / (float) W)));
    }
    const float pk = proc.inPeak.exchange (0.0f);
    const float prev = meter;
    meter = pk > meter ? pk : meter * 0.9f;
    if (std::abs (prev - meter) > 1e-5f) repaint (juce::Rectangle<int> (730, 580, 350, 36).transformedBy (juce::AffineTransform::scale ((float) getWidth() / (float) W)));
}
