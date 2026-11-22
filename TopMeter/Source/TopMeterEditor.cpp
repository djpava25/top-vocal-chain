#include "TopMeterEditor.h"

using namespace tmui;

namespace
{
    juce::String U (const char* s) { return juce::String::fromUTF8 (s); }
    juce::Font font (float h, bool bold = false) { return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain)); }
    const juce::Colour bg (0xff1e1f22), panel (0xff2a2b2f), panelDark (0xff17181a), chipCol (0xff3a3c41), line (0xff404248),
                       text (0xffeceef1), dim (0xff9a9ea6), graph (0xff6f8bd6), graphHi (0xffa9bff4), lime (0xffd9f58e), limeDim (0xff8b9c5c),
                       redBox (0xff7d3440), redText (0xffffa3ad), blueBox (0xff35487c), amber (0xffffb547);
    juce::String fmt (float v, int dec = 1) { return v <= -149.0f ? juce::String ("--") : juce::String (v, dec); }
    void box (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c) { g.setColour (c); g.fillRoundedRectangle (r, 4); }
}

//==============================================================================
Chip::Chip (APVTS& s, const juce::String& id, const juce::String& pre, std::vector<float> fv, const juce::String& u)
    : param (s.getParameter (id)), prefix (pre), unit (u), values (std::move (fv))
{
    att = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v) { value = v; repaint(); }, nullptr);
    att->sendInitialUpdate();
    toggle = dynamic_cast<juce::AudioParameterBool*> (param) != nullptr;
}
void Chip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.5f, 1.0f);
    const bool on = toggle && value > 0.5f;
    box (g, r, toggle ? (on ? juce::Colour (0xff4a5d92) : (hover ? juce::Colour (0xff45474d) : chipCol)) : (hover ? juce::Colour (0xff45474d) : chipCol));
    juce::String v;
    if (toggle) v = prefix;
    else if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (param)) v = c->choices[juce::jlimit (0, c->choices.size() - 1, (int) value)];
    else v = juce::String (value, std::abs (value - std::round (value)) < 0.01f ? 1 : 1) + unit;
    g.setFont (font (12.5f, true));
    if (! toggle && prefix.isNotEmpty())
    {
        g.setColour (dim); g.drawText (prefix, r.reduced (8, 0), juce::Justification::centredLeft);
        g.setColour (text); g.drawText (v, r.reduced (8, 0), juce::Justification::centredRight);
    }
    else { g.setColour (toggle ? (on ? text : dim) : text); g.drawText (v, r, juce::Justification::centred); }
    if (! toggle)
    {
        juce::Path tri; tri.addTriangle (r.getRight() - 6, r.getBottom() - 2, r.getRight() - 2, r.getBottom() - 2, r.getRight() - 2, r.getBottom() - 6);
        g.setColour (dim); g.fillPath (tri);
    }
}
void Chip::mouseUp (const juce::MouseEvent& e)
{
    if (! getLocalBounds().contains (e.getPosition())) return;
    if (toggle) { att->setValueAsCompleteGesture (value > 0.5f ? 0.0f : 1.0f); return; }
    juce::PopupMenu m;
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (param))
        for (int i = 0; i < c->choices.size(); ++i) m.addItem (i + 1, c->choices[i], true, i == (int) value);
    else
        for (size_t i = 0; i < values.size(); ++i) m.addItem ((int) i + 1, juce::String (values[i], 1) + unit, true, std::abs (values[i] - value) < 0.01f);
    juce::Component::SafePointer<Chip> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int r)
    {
        if (safe == nullptr || r <= 0) return;
        if (dynamic_cast<juce::AudioParameterChoice*> (safe->param)) safe->att->setValueAsCompleteGesture ((float) (r - 1));
        else safe->att->setValueAsCompleteGesture (safe->values[(size_t) (r - 1)]);
    });
}

//==============================================================================
void Btn::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.5f);
    if (! flat)
    {
        g.setGradientFill (juce::ColourGradient (on ? juce::Colour (0xffcfe0ff) : juce::Colour (hover ? 0xff4b4e55 : 0xff3c3e44), r.getX(), r.getY(),
                                                 on ? juce::Colour (0xff8faee8) : juce::Colour (0xff2b2d31), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 5);
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.drawRoundedRectangle (r, 5, 1.0f);
    }
    const auto col = on ? juce::Colour (0xff15233f) : (hover ? ::text : (accentText ? graphHi : juce::Colour (0xffc9ccd2)));
    g.setColour (col);
    const auto c = r.getCentre(); const float s = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f;
    juce::PathStrokeType st (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    switch (icon)
    {
        case power:
        {
            juce::Path p; p.addCentredArc (c.x, c.y + 1, s * 0.45f, s * 0.45f, 0, 0.6f, juce::MathConstants<float>::twoPi - 0.6f, true);
            g.strokePath (p, st); g.drawLine (c.x, c.y - s * 0.55f, c.x, c.y - s * 0.05f, 1.8f); break;
        }
        case play:
            if (on) { g.fillRect (c.x - s * 0.3f, c.y - s * 0.35f, s * 0.2f, s * 0.7f); g.fillRect (c.x + s * 0.1f, c.y - s * 0.35f, s * 0.2f, s * 0.7f); }
            else { juce::Path p; p.addTriangle (c.x - s * 0.25f, c.y - s * 0.38f, c.x - s * 0.25f, c.y + s * 0.38f, c.x + s * 0.38f, c.y); g.fillPath (p); }
            break;
        case sync:
        {
            juce::Path p; p.addCentredArc (c.x, c.y, s * 0.38f, s * 0.38f, 0, 0.5f, juce::MathConstants<float>::twoPi - 0.2f, true);
            g.strokePath (p, st);
            juce::Path a; a.addTriangle (c.x + s * 0.1f, c.y - s * 0.55f, c.x + s * 0.36f, c.y - s * 0.38f, c.x + s * 0.08f, c.y - s * 0.2f); g.fillPath (a); break;
        }
        case undo: case redo:
        {
            const float d = icon == undo ? 1.0f : -1.0f;
            juce::Path p; p.startNewSubPath (c.x - d * s * 0.4f, c.y - s * 0.15f); p.lineTo (c.x + d * s * 0.15f, c.y - s * 0.15f);
            p.cubicTo (c.x + d * s * 0.55f, c.y - s * 0.15f, c.x + d * s * 0.55f, c.y + s * 0.4f, c.x + d * s * 0.05f, c.y + s * 0.4f);
            g.strokePath (p, st);
            juce::Path a; a.addTriangle (c.x - d * s * 0.55f, c.y - s * 0.15f, c.x - d * s * 0.28f, c.y - s * 0.38f, c.x - d * s * 0.28f, c.y + s * 0.08f); g.fillPath (a); break;
        }
        case left: case right:
        {
            const float d = icon == left ? -1.0f : 1.0f;
            juce::Path p; p.startNewSubPath (c.x - d * s * 0.15f, c.y - s * 0.32f); p.lineTo (c.x + d * s * 0.2f, c.y); p.lineTo (c.x - d * s * 0.15f, c.y + s * 0.32f);
            g.strokePath (p, st); break;
        }
        case help: g.setFont (font (15.0f, true)); g.drawText ("?", r, juce::Justification::centred); break;
        case resetIcon:
        {
            juce::Path p; p.addCentredArc (c.x, c.y, s * 0.42f, s * 0.42f, 0, -2.2f, 2.6f, true); g.strokePath (p, st);
            juce::Path a; const float ax = c.x + s * 0.42f * std::sin (-2.2f), ay = c.y - s * 0.42f * std::cos (-2.2f);
            a.addTriangle (ax - 4, ay - 3, ax + 3, ay - 4, ax, ay + 3); g.fillPath (a); break;
        }
        default:
            g.setFont (font (label.length() > 2 ? 14.0f : 15.0f, true));
            g.drawFittedText (label, r.reduced (4, 0).toNearestInt(), juce::Justification::centred, 1, 0.7f);
    }
}

//==============================================================================
namespace
{
    // rango vertical de la grafica de sonoridad segun la escala elegida
    void loudRange (const TopMeterProcessor& p, float& lo, float& hi)
    {
        const int sc = (int) p.apvts.getRawParameterValue ("lscale")->load();
        const float t = p.targetLufs();
        if (sc == 0) { lo = t - 18; hi = t + 9; }
        else if (sc == 1) { lo = t - 36; hi = t + 18; }
        else { lo = -60; hi = 0; }
    }
    double windowBlocks (const TopMeterProcessor& p)
    {
        const double w[] { 10, 30, 60, 180, 600 };
        return w[juce::jlimit (0, 4, (int) p.apvts.getRawParameterValue ("window")->load())] * 10.0;
    }
    juce::String clock (double sec)
    {
        const int s = (int) sec;
        return juce::String (s / 3600).paddedLeft ('0', 2) + ":" + juce::String ((s / 60) % 60).paddedLeft ('0', 2) + ":" + juce::String (s % 60).paddedLeft ('0', 2);
    }
}

void LoudnessView::paint (juce::Graphics& g)
{
    auto& m = proc.meter;
    const float Wd = (float) getWidth(), Ht = (float) getHeight();
    float lo, hi; loudRange (proc, lo, hi);
    const float gy0 = 2, gy1 = Ht - 18;
    auto yOf = [&] (float l) { return gy1 - (juce::jlimit (lo, hi, l) - lo) / (hi - lo) * (gy1 - gy0); };
    const float target = proc.targetLufs();
    // escalas
    const float stepL = (hi - lo) > 40 ? 6.0f : 3.0f;
    g.setFont (font (10.5f));
    for (float l = hi; l >= lo - 0.01f; l -= stepL)
    {
        const bool t = std::abs (l - target) < 0.01f;
        g.setColour (t ? text : dim);
        g.drawText (juce::String ((int) std::round (l)), 0, (int) yOf (l) - 6, 28, 12, juce::Justification::centredRight);
        g.drawText (juce::String ((int) std::round (l)), (int) Wd - 26, (int) yOf (l) - 6, 26, 12, juce::Justification::centredLeft);
    }
    // barra Momentary
    {
        const auto r = juce::Rectangle<float> (32, gy0, 26, gy1 - gy0);
        box (g, r, panelDark);
        const float mv = m.momentary.load();
        if (mv > lo)
        {
            const float y = yOf (mv);
            g.setGradientFill (juce::ColourGradient (graphHi, r.getX(), gy0, graph.darker (0.3f), r.getX(), gy1, false));
            g.fillRect (r.withTop (y));
        }
        const float ty = yOf (target);
        g.setColour (text); g.fillRect (r.getX(), ty - 0.5f, r.getWidth(), 1.5f);
        juce::Path tri; tri.addTriangle (r.getRight() + 2, ty, r.getRight() + 9, ty - 5, r.getRight() + 9, ty + 5);
        g.fillPath (tri);
        const float iv = m.integrated.load();
        if (iv > lo) { g.setColour (amber); g.fillRect (r.getX() - 4, yOf (iv) - 1.0f, 4.0f, 2.0f); }
    }
    // grafica de historia
    const auto gr = juce::Rectangle<float> (68, gy0, Wd - 68 - 104, gy1 - gy0);
    box (g, gr, panelDark);
    for (float l = hi; l >= lo; l -= stepL) { g.setColour (juce::Colours::white.withAlpha (std::abs (l - target) < 0.01f ? 0.0f : 0.04f)); g.drawHorizontalLine ((int) yOf (l), gr.getX(), gr.getRight()); }
    const int count = m.histCount.load();
    const double win = windowBlocks (proc);
    const double end = viewEnd < 0 ? (double) count : viewEnd;
    const double start = end - win;
    const bool shortTerm = proc.apvts.getRawParameterValue ("graph")->load() < 0.5f;
    const auto& H = shortTerm ? m.histS : m.histM;
    const int hs = tmeter::Meter::historySize;
    juce::Path area, lineM;
    const int cols = (int) gr.getWidth();
    bool started = false;
    area.startNewSubPath (gr.getX(), gr.getBottom());
    for (int x = 0; x <= cols; ++x)
    {
        const double b0 = start + win * x / cols, b1 = start + win * (x + 1) / cols;
        float v = -150.0f, vm = -150.0f;
        for (long long b = (long long) std::floor (b0); b < (long long) std::max (b0 + 1, std::ceil (b1)); ++b)
            if (b >= 0 && b < count && count - b <= hs) { v = std::max (v, H[(size_t) (b % hs)]); vm = std::max (vm, m.histM[(size_t) (b % hs)]); }
        const float px = gr.getX() + (float) x;
        area.lineTo (px, v > lo ? yOf (v) : gr.getBottom());
        if (shortTerm && vm > lo) { if (! started) { lineM.startNewSubPath (px, yOf (vm)); started = true; } else lineM.lineTo (px, yOf (vm)); }
    }
    area.lineTo (gr.getRight(), gr.getBottom()); area.closeSubPath();
    g.saveState(); g.reduceClipRegion (gr.toNearestInt());
    g.setGradientFill (juce::ColourGradient (graph.withAlpha (0.95f), 0, gr.getY(), graph.withAlpha (0.55f).darker (0.4f), 0, gr.getBottom(), false));
    g.fillPath (area);
    g.setColour (graphHi.withAlpha (0.9f)); g.strokePath (area, juce::PathStrokeType (1.2f));
    if (! lineM.isEmpty()) { g.setColour (juce::Colours::white.withAlpha (0.18f)); g.strokePath (lineM, juce::PathStrokeType (1.0f)); }
    // objetivo
    g.setColour (text.withAlpha (0.85f)); g.drawHorizontalLine ((int) yOf (target), gr.getX(), gr.getRight());
    // linea de "ahora"
    if (viewEnd < 0) { g.setColour (juce::Colours::white.withAlpha (0.9f)); g.drawVerticalLine ((int) gr.getRight() - 2, gr.getY(), gr.getBottom()); }
    g.restoreState();
    // tiempos
    g.setColour (dim); g.setFont (font (11.0f));
    for (int k = 0; k <= 3; ++k)
    {
        const double b = start + win * (0.03 + 0.31 * k);
        if (b < 0) continue;
        const float x = gr.getX() + gr.getWidth() * (float) (0.03 + 0.31 * k);
        g.drawText (clock (b / 10.0), (int) x - 40, (int) gy1 + 3, 80, 14, juce::Justification::centred);
    }
    // histograma de Short-Term
    const auto hr = juce::Rectangle<float> (gr.getRight() + 6, gy0, 70, gy1 - gy0);
    box (g, hr, panelDark);
    {
        const auto& h = m.stHist;
        const int rows = (int) hr.getHeight();
        std::vector<float> v ((size_t) rows, 0.0f); float mx = 0;
        for (int b = 0; b < tmeter::LoudHist::bins; ++b)
        {
            const int c = h.count[(size_t) b]; if (c == 0) continue;
            const float l = tmeter::LoudHist::lo + ((float) b + 0.5f) * tmeter::LoudHist::step;
            if (l < lo || l > hi) continue;
            const int row = juce::jlimit (0, rows - 1, (int) (yOf (l) - hr.getY()));
            v[(size_t) row] += (float) c; mx = std::max (mx, v[(size_t) row]);
        }
        if (mx > 0)
        {
            g.setColour (graphHi.withAlpha (0.85f));
            for (int r = 0; r < rows; ++r) if (v[(size_t) r] > 0) g.fillRect (hr.getRight() - hr.getWidth() * v[(size_t) r] / mx, hr.getY() + (float) r, hr.getWidth() * v[(size_t) r] / mx, 1.2f);
        }
        g.setColour (text.withAlpha (0.6f)); g.drawHorizontalLine ((int) yOf (target), hr.getX(), hr.getRight());
    }
}

//==============================================================================
void StatsView::paint (juce::Graphics& g)
{
    auto& m = proc.meter;
    const float target = proc.targetLufs();
    const float I = m.integrated.load(), S = m.shortTerm.load(), TP = m.tpMax.load(), SM = m.stMax.load();
    const float M = m.momentary.load(), MM = m.momMax.load(), LRA = m.lra.load();
    const float plr = (TP > -149 && I > -149) ? TP - I : -150.0f, psr = (m.tpRecent.load() > -149 && S > -149) ? m.tpRecent.load() - S : -150.0f;
    auto big = [&] (juce::Rectangle<float> r, float v, const char* unit, const char* name, juce::Colour bgc, juce::Colour vc)
    {
        box (g, r, bgc);
        const auto vs = fmt (v);
        g.setColour (vc); g.setFont (font (22.0f, true));
        g.drawText (vs, r.withTrimmedBottom (r.getHeight() * 0.36f).withTrimmedRight (38), juce::Justification::centredRight);
        g.setColour (vc.withAlpha (0.8f)); g.setFont (font (10.5f, true));
        g.drawText (unit, r.withTrimmedBottom (r.getHeight() * 0.36f).withTrimmedLeft (r.getWidth() - 36), juce::Justification::centredLeft);
        g.setColour (dim.brighter (0.2f)); g.setFont (font (11.5f));
        g.drawText (name, r.withTrimmedTop (r.getHeight() * 0.62f), juce::Justification::centred);
    };
    auto small = [&] (juce::Rectangle<float> r, float v, const char* unit, const char* name, juce::Colour bgc)
    {
        box (g, r, bgc);
        g.setColour (text); g.setFont (font (16.0f, true));
        g.drawText (fmt (v), r.withTrimmedBottom (r.getHeight() * 0.4f).withTrimmedRight (32), juce::Justification::centredRight);
        g.setColour (dim); g.setFont (font (9.5f, true));
        g.drawText (unit, r.withTrimmedBottom (r.getHeight() * 0.4f).withTrimmedLeft (r.getWidth() - 30), juce::Justification::centredLeft);
        g.setFont (font (11.0f)); g.drawText (name, r.withTrimmedTop (r.getHeight() * 0.58f), juce::Justification::centred);
    };
    const bool loud = I > target + 1.0f, ok = I > -149 && std::abs (I - target) <= 1.0f;
    const float c1 = 98, c2 = (float) getWidth() - c1 - 6;
    big ({ 0, 0, c1, 50 }, I, "LUFS", "Integrated", loud ? redBox : ok ? juce::Colour (0xff2f5a43) : panel, loud ? redText : ok ? juce::Colour (0xff9ff0c0) : text);
    big ({ 0, 54, c1, 50 }, S, "LUFS", "Short-Term", panel, text);
    big ({ 0, 108, c1, 50 }, TP, "dBTP", "True Peak Max", TP > -1.0f ? redBox : panel, TP > -1.0f ? redText : text);
    big ({ 0, 162, c1, 50 }, SM, "LUFS", "Short Term Max", panel, text);
    const float x2 = c1 + 6, h2 = 39.6f;
    small ({ x2, 0, c2, h2 }, plr, "LU", "PLR", panel);
    small ({ x2, h2 + 3.5f, c2, h2 }, psr, "LU", "PSR", blueBox);
    small ({ x2, (h2 + 3.5f) * 2, c2, h2 }, LRA, "LU", "LRA", panel);
    small ({ x2, (h2 + 3.5f) * 3, c2, h2 }, M, "LUFS", "Mom", panel);
    small ({ x2, (h2 + 3.5f) * 4, c2, h2 }, MM, "LUFS", "Mom Max", panel);
}

//==============================================================================
void OverviewView::paint (juce::Graphics& g)
{
    auto& m = proc.meter;
    auto r = getLocalBounds().toFloat();
    box (g, r, panelDark);
    const int count = m.histCount.load();
    const int hs = tmeter::Meter::historySize;
    const double first = std::max (0, count - hs);
    const double total = std::max (windowBlocks (proc), (double) count - first);
    float lo, hi; loudRange (proc, lo, hi);
    juce::Path p; p.startNewSubPath (r.getX(), r.getBottom());
    for (int x = 0; x < (int) r.getWidth(); x += 2)
    {
        const long long b = (long long) (first + total * x / r.getWidth());
        float v = b < count ? m.histS[(size_t) (b % hs)] : -150.0f;
        const float y = r.getBottom() - r.getHeight() * juce::jlimit (0.0f, 1.0f, (v - lo) / (hi - lo));
        p.lineTo (r.getX() + (float) x, y);
    }
    p.lineTo (r.getRight(), r.getBottom()); p.closeSubPath();
    g.setColour (graph.withAlpha (0.5f)); g.fillPath (p);
    const double end = lv.viewEnd < 0 ? (double) count : lv.viewEnd;
    const double win = windowBlocks (proc);
    const float x0 = r.getX() + (float) ((end - win - first) / total) * r.getWidth(), x1 = r.getX() + (float) ((end - first) / total) * r.getWidth();
    g.setColour (graphHi.withAlpha (0.15f)); g.fillRect (juce::Rectangle<float> (x0, r.getY(), x1 - x0, r.getHeight()));
    g.setColour (graphHi); g.drawRect (juce::Rectangle<float> (x0, r.getY() + 0.5f, x1 - x0, r.getHeight() - 1), 1.0f);
}
void OverviewView::mouseDrag (const juce::MouseEvent& e)
{
    auto& m = proc.meter;
    const int count = m.histCount.load();
    const double first = std::max (0, count - tmeter::Meter::historySize);
    const double total = std::max (windowBlocks (proc), (double) count - first);
    const double win = windowBlocks (proc);
    const double centreB = first + total * e.position.x / getWidth();
    const double end = juce::jlimit (std::min ((double) count, win), (double) count, centreB + win * 0.5);
    if (end >= count - 1) lv.viewEnd = -1;
    else
    {
        lv.viewEnd = end;
        if (auto* p = proc.apvts.getParameter ("autoscroll")) p->setValueNotifyingHost (0.0f);
    }
    repaint(); lv.repaint();
}

//==============================================================================
namespace
{
    float levelPos (float db, bool linear)
    {
        if (db <= -150.0f) return 0.0f;
        if (linear) return juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
        static const float d[] { 0, -5, -10, -20, -30, -40, -60, -90 }, p[] { 1.0f, 0.875f, 0.75f, 0.56f, 0.40f, 0.26f, 0.08f, 0.0f };
        if (db >= 0) return 1.0f;
        for (int i = 0; i < 7; ++i)
            if (db >= d[i + 1]) return p[i + 1] + (p[i] - p[i + 1]) * (db - d[i + 1]) / (d[i] - d[i + 1]);
        return 0.0f;
    }
}
void LevelView::tick()
{
    auto& m = proc.meter;
    const bool tp = proc.apvts.getRawParameterValue ("tp")->load() > 0.5f;
    const int hc = (int) proc.apvts.getRawParameterValue ("pkhold")->load();
    const double holdSec[] { 1, 3, 5, 1e9 };
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    for (int c = 0; c < 2; ++c)
    {
        const float v = tp ? m.tpCur[c].load() : m.peak[c].load();
        shown[c] = v > shown[c] ? v : std::max (v, shown[c] - 1.5f);
        rms[c] = m.rmsDb[c].load();
        if (v >= hold[c] || now > holdT[c]) { hold[c] = v; holdT[c] = now + holdSec[juce::jlimit (0, 3, hc)]; }
    }
    repaint();
}
void LevelView::paint (juce::Graphics& g)
{
    auto& m = proc.meter;
    const bool lin = proc.apvts.getRawParameterValue ("lvscale")->load() > 0.5f;
    const float top = 8, bot = (float) getHeight() - 6;
    auto yOf = [&] (float db) { return bot - levelPos (db, lin) * (bot - top); };
    // escalas
    g.setFont (font (10.5f));
    const float marks[] { 0, -5, -10, -20, -30, -40, -60 };
    for (float d : marks)
    {
        g.setColour (dim);
        const juce::String t = juce::String ((int) -d);
        g.drawText (t, 0, (int) yOf (d) - 6, 26, 12, juce::Justification::centredRight);
        g.drawText (t, 140, (int) yOf (d) - 6, 26, 12, juce::Justification::centredLeft);
    }
    g.setColour (dim); g.drawText ("inf", 0, (int) bot - 10, 26, 12, juce::Justification::centredRight);
    for (int c = 0; c < 2; ++c)
    {
        const auto r = juce::Rectangle<float> (34.0f + (float) c * 52.0f, top, 40, bot - top);
        g.setColour (juce::Colour (0xff121314)); g.fillRect (r);
        g.setColour (juce::Colour (0xff3b2224)); g.fillRect (r.withHeight (6));        // zona de saturacion
        const float yp = yOf (shown[c]), yr = yOf (rms[c]);
        g.setColour (limeDim); g.fillRect (r.withTop (yp).withBottom (r.getBottom()));
        g.setGradientFill (juce::ColourGradient (lime, r.getX(), r.getY(), lime.darker (0.15f), r.getX(), r.getBottom(), false));
        g.fillRect (r.withTop (yr));
        if (shown[c] > -0.1f) { g.setColour (juce::Colour (0xffff4d4d)); g.fillRect (r.withHeight (6)); }
        if (hold[c] > -149) { g.setColour (juce::Colours::white); g.fillRect (r.getX(), yOf (hold[c]) - 1.0f, r.getWidth(), 2.0f); }
    }
    // cajas de valores
    auto valBox = [&] (juce::Rectangle<float> r, const char* name, float mx, float cl, float cr)
    {
        box (g, r, panel);
        g.setColour (dim); g.setFont (font (11.5f, true));
        g.drawText (name, r.withHeight (20).reduced (8, 0), juce::Justification::centredLeft);
        g.drawText ("dBFS", r.withHeight (20).reduced (8, 0), juce::Justification::centredRight);
        g.setColour (lime); g.setFont (font (22.0f, true));
        g.drawText (fmt (mx), r.withY (r.getY() + 18).withHeight (30), juce::Justification::centred);
        g.setColour (dim); g.setFont (font (11.0f));
        g.drawText ("max", r.withY (r.getY() + 46).withHeight (14), juce::Justification::centred);
        g.setColour (juce::Colour (0xff1f2023)); g.fillRect (r.getX(), r.getY() + 64, r.getWidth(), 1.5f); g.fillRect (r.getCentreX(), r.getY() + 66, 1.5f, 30.0f);
        g.setColour (lime.withAlpha (0.9f)); g.setFont (font (17.0f, true));
        g.drawText (fmt (cl), juce::Rectangle<float> (r.getX(), r.getY() + 66, r.getWidth() * 0.5f, 30), juce::Justification::centred);
        g.drawText (fmt (cr), juce::Rectangle<float> (r.getCentreX(), r.getY() + 66, r.getWidth() * 0.5f, 30), juce::Justification::centred);
        g.setColour (dim); g.setFont (font (11.0f));
        g.drawText ("current", r.withY (r.getBottom() - 16).withHeight (14), juce::Justification::centred);
    };
    const bool tp = proc.apvts.getRawParameterValue ("tp")->load() > 0.5f;
    const float pmx = tp ? m.tpMax.load() : std::max (m.peakMax[0].load(), m.peakMax[1].load());
    valBox ({ 176, 2, 110, 114 }, tp ? "TRUE PK" : "PEAK", pmx, shown[0], shown[1]);
    valBox ({ 176, 122, 110, 114 }, "RMS", std::max (m.rmsMax[0].load(), m.rmsMax[1].load()), rms[0], rms[1]);
}

//==============================================================================
void ScopeView::paint (juce::Graphics& g)
{
    auto& m = proc.meter;
    const auto r = getLocalBounds().toFloat();
    // barra de balance (arriba) y de correlacion (abajo)
    auto segBar = [&] (float y, float v, bool corr)
    {
        const int n = 25;
        const float x0 = r.getX() + 22, w = r.getWidth() - 44, sw = w / n;
        const int idx = juce::jlimit (0, n - 1, (int) std::round ((v + 1.0f) * 0.5f * (n - 1)));
        for (int i = 0; i < n; ++i)
        {
            juce::Colour c = corr ? (i < n / 2 ? juce::Colour (0xffd9784a) : juce::Colour (0xffcfe27a)) : juce::Colour (0xff6a6d74);
            const bool lit = corr ? (v >= 0 ? (i >= n / 2 && i <= idx) : (i <= n / 2 && i >= idx)) : i == idx;
            g.setColour (lit ? (corr ? c : juce::Colours::white) : c.withAlpha (0.22f));
            g.fillRect (x0 + (float) i * sw + 1, y, sw - 2, 9.0f);
        }
        g.setColour (juce::Colours::white); g.fillRect (x0 + (float) (n / 2) * sw + sw * 0.5f - 1, y - 2, 2.0f, 13.0f);
    };
    g.setColour (dim); g.setFont (font (12.0f, true));
    g.drawText ("L", (int) r.getX() + 4, 0, 14, 14, juce::Justification::centred);
    g.drawText ("R", (int) r.getRight() - 18, 0, 14, 14, juce::Justification::centred);
    segBar (2, m.balance.load(), false);
    g.drawText (U ("−1"), (int) r.getX() + 2, (int) r.getBottom() - 14, 20, 14, juce::Justification::centred);
    g.drawText ("+1", (int) r.getRight() - 22, (int) r.getBottom() - 14, 20, 14, juce::Justification::centred);
    segBar (r.getBottom() - 12, m.correlation.load(), true);
    // goniometro
    const auto sq = juce::Rectangle<float> (r.getCentreX() - 105, r.getY() + 20, 210, 210);
    const auto c = sq.getCentre(); const float R = sq.getWidth() * 0.5f;
    juce::Path dia; dia.startNewSubPath (c.x, c.y - R); dia.lineTo (c.x + R, c.y); dia.lineTo (c.x, c.y + R); dia.lineTo (c.x - R, c.y); dia.closeSubPath();
    g.setColour (juce::Colour (0xff121314)); g.fillPath (dia);
    g.setColour (juce::Colours::white.withAlpha (0.22f)); g.strokePath (dia, juce::PathStrokeType (1.0f));
    g.drawLine (c.x - R * 0.5f, c.y - R * 0.5f, c.x + R * 0.5f, c.y + R * 0.5f, 0.8f);
    g.drawLine (c.x + R * 0.5f, c.y - R * 0.5f, c.x - R * 0.5f, c.y + R * 0.5f, 0.8f);
    g.drawLine (c.x - R * 1.0f, c.y - R * 1.0f, c.x - R * 0.5f, c.y - R * 0.5f, 0.6f);
    g.drawLine (c.x + R * 1.0f, c.y - R * 1.0f, c.x + R * 0.5f, c.y - R * 0.5f, 0.6f);
    g.drawLine (c.x - R * 1.0f, c.y + R * 1.0f, c.x - R * 0.5f, c.y + R * 0.5f, 0.6f);
    g.drawLine (c.x + R * 1.0f, c.y + R * 1.0f, c.x + R * 0.5f, c.y + R * 0.5f, 0.6f);
    const int w = proc.scopeW.load(), N = 2048;
    float pk = 1e-4f;
    for (int i = 0; i < N; ++i)
    {
        const int k = (w - N + i) & (TopMeterProcessor::scopeSize - 1);
        pk = std::max (pk, std::abs (proc.scopeL[(size_t) k]) + std::abs (proc.scopeR[(size_t) k]));
    }
    const float tg = juce::jlimit (0.7f, 40.0f, 0.95f / pk);
    gain += (tg - gain) * (tg < gain ? 0.5f : 0.05f);
    g.saveState(); g.reduceClipRegion (dia);
    g.setColour (juce::Colour (0xffe8f07a).withAlpha (0.55f));
    for (int i = 0; i < N; ++i)
    {
        const int k = (w - N + i) & (TopMeterProcessor::scopeSize - 1);
        const float L = proc.scopeL[(size_t) k], Rr = proc.scopeR[(size_t) k];
        const float x = (Rr - L) * gain * R, y = (L + Rr) * gain * R;
        g.fillRect (c.x + x, c.y - y, 1.3f, 1.3f);
    }
    g.restoreState();
}

//==============================================================================
void VuView::tick()
{
    const float v = juce::jlimit (-22.0f, 4.0f, proc.meter.vu[channel].load());
    // aguja con inercia (resorte amortiguado)
    vel += (v - needle) * 0.35f; vel *= 0.55f; needle += vel;
    repaint();
}
void VuView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto face = r.removeFromTop (r.getHeight() - 58).reduced (4);
    g.setColour (text); g.setFont (font (15.0f, true));
    g.drawText (channel == 0 ? "L" : "R", face.removeFromTop (22), juce::Justification::centred);
    // marco
    g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (face.translated (0, 3), 8);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4c52), face.getX(), face.getY(), juce::Colour (0xff17181a), face.getX(), face.getBottom(), false));
    g.fillRoundedRectangle (face, 8);
    const auto inner = face.reduced (8);
    juce::ColourGradient cg (juce::Colour (0xfff6f3ea), inner.getCentreX(), inner.getY(), juce::Colour (0xffcfcabc), inner.getCentreX(), inner.getBottom(), false);
    g.setGradientFill (cg); g.fillRoundedRectangle (inner, 4);
    const juce::Point<float> pv (inner.getCentreX(), inner.getBottom() + inner.getHeight() * 0.35f);
    const float Rr = inner.getHeight() * 1.0f;
    auto ang = [] (float vu) { // -20..+3 VU
        const float p = std::pow (10.0f, vu / 20.0f);                    // escala de tension (como un VU real)
        const float p0 = std::pow (10.0f, -20.0f / 20.0f), p1 = std::pow (10.0f, 3.0f / 20.0f);
        return -0.8f + 1.6f * (p - p0) / (p1 - p0);
    };
    const juce::Colour ink (0xff2a2724), redc (0xffc0262b);
    g.saveState(); g.reduceClipRegion (inner.toNearestInt());
    juce::Path arcBlack; arcBlack.addCentredArc (pv.x, pv.y, Rr, Rr, 0, ang (-20), ang (0), true);
    g.setColour (ink); g.strokePath (arcBlack, juce::PathStrokeType (1.4f));
    juce::Path arcRed; arcRed.addCentredArc (pv.x, pv.y, Rr + 4, Rr + 4, 0, ang (0), ang (3), true);
    g.setColour (redc); g.strokePath (arcRed, juce::PathStrokeType (8.0f));
    const float labs[] { -20, -10, -7, -5, -3, -2, -1, 0, 1, 2, 3 };
    g.setFont (font (12.0f, true));
    for (float l : labs)
    {
        const float a = ang (l);
        g.setColour (l > 0 ? redc : ink);
        g.drawLine (juce::Line<float> ({ pv.x + Rr * std::sin (a), pv.y - Rr * std::cos (a) }, { pv.x + (Rr + 9) * std::sin (a), pv.y - (Rr + 9) * std::cos (a) }), 1.4f);
        const float rl = Rr + 20;
        g.drawText (juce::String ((int) std::abs (l)), juce::Rectangle<float> (pv.x + rl * std::sin (a) - 12, pv.y - rl * std::cos (a) - 7, 24, 14), juce::Justification::centred);
    }
    // escala de porcentaje
    g.setFont (font (8.5f)); g.setColour (ink.withAlpha (0.7f));
    for (int pc : { 0, 20, 40, 60, 80, 100 })
    {
        const float vu = pc == 0 ? -20.0f : 20.0f * std::log10 ((float) pc / 100.0f);
        const float a = ang (std::max (-20.0f, vu)), rl = Rr - 14;
        g.drawLine (juce::Line<float> ({ pv.x + (Rr - 6) * std::sin (a), pv.y - (Rr - 6) * std::cos (a) }, { pv.x + Rr * std::sin (a), pv.y - Rr * std::cos (a) }), 1.0f);
        g.drawText (juce::String (pc), juce::Rectangle<float> (pv.x + rl * std::sin (a) - 10, pv.y - rl * std::cos (a) - 5, 20, 10), juce::Justification::centred);
    }
    g.setColour (ink); g.setFont (font (20.0f, true));
    g.drawText (U ("−"), juce::Rectangle<float> (inner.getX() + 6, inner.getBottom() - 30, 20, 20), juce::Justification::centred);
    g.setColour (redc); g.drawText ("+", juce::Rectangle<float> (inner.getRight() - 26, inner.getBottom() - 30, 20, 20), juce::Justification::centred);
    g.setColour (ink); g.setFont (font (10.0f, true));
    g.drawText ("TOP METER", juce::Rectangle<float> (inner.getCentreX() - 50, inner.getBottom() - 40, 100, 12), juce::Justification::centred);
    g.setFont (font (12.0f, true)); g.drawText ("VU", juce::Rectangle<float> (inner.getCentreX() - 30, inner.getBottom() - 24, 60, 14), juce::Justification::centred);
    // aguja
    const float a = ang (needle);
    const juce::Point<float> tip (pv.x + (Rr + 8) * std::sin (a), pv.y - (Rr + 8) * std::cos (a));
    g.setColour (juce::Colours::black.withAlpha (0.2f)); g.drawLine (juce::Line<float> (pv.translated (3, 2), tip.translated (3, 2)), 2.0f);
    g.setColour (juce::Colour (0xffb3141c)); g.drawLine (juce::Line<float> (pv, tip), 1.6f);
    g.restoreState();
    // tapa y cristal
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.25f), inner.getX(), inner.getY(), juce::Colours::transparentWhite, inner.getX(), inner.getCentreY(), false));
    g.fillRoundedRectangle (inner.withHeight (inner.getHeight() * 0.45f), 4);
    g.setColour (juce::Colour (0xff111111)); g.fillEllipse (inner.getRight() - 18, inner.getY() + 6, 9, 9);
    // valores
    const float cur = proc.meter.vu[channel].load(), mx = proc.meter.vuMax[channel].load();
    g.setColour (dim); g.setFont (font (11.5f));
    g.drawText ("current", juce::Rectangle<float> (r.getX(), r.getY() + 4, r.getWidth() * 0.5f, 14), juce::Justification::centred);
    g.setColour (juce::Colour (0xffff6b6b)); g.drawText ("max", juce::Rectangle<float> (r.getCentreX(), r.getY() + 4, r.getWidth() * 0.5f, 14), juce::Justification::centred);
    g.setColour (text); g.setFont (font (22.0f, true));
    g.drawText (cur < -39 ? juce::String ("--") : juce::String (cur, 1), juce::Rectangle<float> (r.getX(), r.getY() + 18, r.getWidth() * 0.5f, 28), juce::Justification::centred);
    g.drawText (mx < -39 ? juce::String ("--") : juce::String (mx, 1), juce::Rectangle<float> (r.getCentreX(), r.getY() + 18, r.getWidth() * 0.5f, 28), juce::Justification::centred);
}

//==============================================================================
SpectrumView::SpectrumView (TopMeterProcessor& p) : proc (p)
{
    data.assign ((size_t) (2 << 14), 0.0f);
    window.resize ((size_t) (1 << 14));
    for (size_t i = 0; i < window.size(); ++i) window[i] = 0.5f - 0.5f * std::cos (6.2831853f * (float) i / (float) (window.size() - 1));
    level.fill (-150); peak.fill (-150); maxv.fill (-150); pow.fill (0); peakT.fill (0);
}
void SpectrumView::resetPeaks() { peak.fill (-150); maxv.fill (-150); }
void SpectrumView::tick()
{
    const int N = 1 << 14;
    const int w = proc.specW.load();
    double wsum = 0;
    for (int i = 0; i < N; ++i)
    {
        const float x = proc.specBuf[(size_t) ((w - N + i) & (TopMeterProcessor::specSize - 1))];
        data[(size_t) i] = x * window[(size_t) i]; wsum += (double) window[(size_t) i] * window[(size_t) i];
    }
    std::fill (data.begin() + N, data.end(), 0.0f);
    fft.performFrequencyOnlyForwardTransform (data.data(), true);
    const double sr = proc.getRate(), binHz = sr / N;
    const int avg = (int) proc.apvts.getRawParameterValue ("savg")->load();
    const double a = avg == 0 ? 0.45 : avg == 1 ? 0.12 : 0.04;
    const int hc = (int) proc.apvts.getRawParameterValue ("shold")->load();
    const double holdSec[] { 1, 3, 5, 1e9 };
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    for (int b = 0; b < bands; ++b)
    {
        const double fc = centre (b), f0 = fc * std::pow (2.0, -1.0 / 6.0), f1 = fc * std::pow (2.0, 1.0 / 6.0);
        const int k0 = std::max (1, (int) std::floor (f0 / binHz)), k1 = std::max (k0, (int) std::ceil (f1 / binHz) - 1);
        double e = 0;
        for (int k = k0; k <= k1 && k < N / 2; ++k) e += (double) data[(size_t) k] * data[(size_t) k];
        // potencia del tercio de octava normalizada: un seno a 0 dBFS marca 0 dB
        const double p = 4.0 * e / (N * wsum);
        pow[(size_t) b] += (p - pow[(size_t) b]) * a;
        const float db = tmeter::toDb (pow[(size_t) b]);
        level[(size_t) b] = db;
        maxv[(size_t) b] = std::max (maxv[(size_t) b], db);
        if (db >= peak[(size_t) b] || now > peakT[(size_t) b]) { peak[(size_t) b] = db; peakT[(size_t) b] = now + holdSec[juce::jlimit (0, 3, hc)]; }
    }
    repaint();
}
void SpectrumView::mouseMove (const juce::MouseEvent& e)
{
    const float x0 = 34, w = (float) getWidth() - 68;
    hoverBand = e.position.x < x0 || e.position.x > x0 + w ? -1 : juce::jlimit (0, bands - 1, (int) ((e.position.x - x0) / (w / bands)));
}
void SpectrumView::paint (juce::Graphics& g)
{
    const float lo = proc.apvts.getRawParameterValue ("smin")->load(), hi = proc.apvts.getRawParameterValue ("smax")->load();
    const float top = 4, bot = (float) getHeight() - 20, x0 = 34, w = (float) getWidth() - 68;
    auto yOf = [&] (float db) { return bot - juce::jlimit (0.0f, 1.0f, (db - lo) / (hi - lo)) * (bot - top); };
    g.setFont (font (11.0f));
    const float step = (hi - lo) > 80 ? 12.0f : 6.0f;
    for (float d = hi; d >= lo - 0.01f; d -= step)
    {
        g.setColour (dim);
        g.drawText (juce::String ((int) -d), 0, (int) yOf (d) - 6, 28, 12, juce::Justification::centredRight);
        g.drawText (juce::String ((int) -d), (int) (x0 + w) + 6, (int) yOf (d) - 6, 28, 12, juce::Justification::centredLeft);
    }
    const float bw = w / bands;
    for (int b = 0; b < bands; ++b)
    {
        const auto col = juce::Rectangle<float> (x0 + (float) b * bw + 3, top, bw - 6, bot - top);
        const bool sel = b == selected || b == hoverBand;
        g.setColour (sel ? juce::Colour (0xff34373e) : juce::Colour (0xff26272a)); g.fillRect (col);
        const float yl = yOf (level[(size_t) b]), yp = yOf (peak[(size_t) b]);
        if (yp < bot) { g.setColour (juce::Colour (0xff5f6b8a)); g.fillRect (col.withTop (yp).withBottom (yl)); g.setColour (juce::Colour (0xff8090b6)); g.fillRect (col.getX(), yp, col.getWidth(), 2.0f); }
        g.setColour (sel ? graphHi.brighter (0.2f) : juce::Colour (0xff9db2f2)); g.fillRect (col.withTop (yl));
    }
    g.setColour (dim); g.setFont (font (11.0f, true));
    const int labelBands[] { 1, 4, 7, 10, 13, 16, 19, 22, 25, 28 };
    const char* names[] { "31.5", "63", "125", "250", "500", "1k", "2k", "4k", "8k", "16k" };
    for (int i = 0; i < 10; ++i)
        g.drawText (names[i], (int) (x0 + bw * (float) labelBands[i]) - 20, (int) bot + 4, (int) bw + 40, 14, juce::Justification::centred);
}

//==============================================================================
TopMeterEditor::TopMeterEditor (TopMeterProcessor& p)
    : AudioProcessorEditor (&p), proc (p), loud (p), stats (p), overview (p, loud), levels (p), scope (p), vuL (p, 0), vuR (p, 1), spectrum (p)
{
    powerBtn.setTooltip ("Pausar / reanudar la medicion");
    powerBtn.onClick = [this] { if (auto* q = proc.apvts.getParameter ("run")) q->setValueNotifyingHost (q->getValue() > 0.5f ? 0.0f : 1.0f); };
    playBtn.setTooltip ("Medir / congelar");
    playBtn.onClick = powerBtn.onClick;
    syncBtn.setTooltip (U ("DAW Sync: se reinicia al dar play en el DAW y solo mide mientras suena"));
    syncBtn.onClick = [this] { if (auto* q = proc.apvts.getParameter ("sync")) q->setValueNotifyingHost (q->getValue() > 0.5f ? 0.0f : 1.0f); };
    resetBtn.setTooltip ("Borrar todas las mediciones");
    resetBtn.accentText = true;
    resetBtn.onClick = [this] { proc.resetMeters(); spectrum.resetPeaks(); loud.viewEnd = -1; };
    undoBtn.flat = redoBtn.flat = aBtn.flat = bBtn.flat = prevBtn.flat = nextBtn.flat = presetBtn.flat = helpBtn.flat = specReset.flat = true;
    undoBtn.onClick = [this] { proc.undoManager.undo(); };
    redoBtn.onClick = [this] { proc.undoManager.redo(); };
    aBtn.onClick = [this] { switchAB (0); };
    bBtn.onClick = [this] { switchAB (1); };
    prevBtn.onClick = [this] { const int n = proc.getNumPrograms(); proc.setCurrentProgram ((proc.getCurrentProgram() - 1 + n) % n); };
    nextBtn.onClick = [this] { proc.setCurrentProgram ((proc.getCurrentProgram() + 1) % proc.getNumPrograms()); };
    presetBtn.accentText = true;
    presetBtn.onClick = [this]
    {
        juce::PopupMenu m;
        for (int i = 0; i < proc.getNumPrograms(); ++i) m.addItem (i + 1, proc.getProgramName (i), true, i == proc.getCurrentProgram());
        juce::Component::SafePointer<TopMeterEditor> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetBtn), [safe] (int r) { if (safe != nullptr && r > 0) safe->proc.setCurrentProgram (r - 1); });
    };
    helpBtn.onClick = []
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Top Meter",
            U ("Medidor de sonoridad (ITU-R BS.1770 / EBU R128):\n"
               "Integrated = volumen de toda la canción · Short-Term = últimos 3 s · Momentary = últimos 0.4 s\n"
               "True Peak = pico real entre muestras (no pases de -1 dBTP para streaming)\n"
               "LRA = rango dinámico · PLR = pico menos integrado · PSR = pico menos short-term\n\n"
               "Diseño propio de Dj Pava Hm Music."));
    };
    specReset.setTooltip ("Borrar picos del espectro");
    specReset.onClick = [this] { spectrum.resetPeaks(); };
    nameLabel.setText (proc.channelName, juce::dontSendNotification);
    nameLabel.setEditable (false, true);
    nameLabel.setJustificationType (juce::Justification::centred);
    nameLabel.setFont (font (25.0f));
    nameLabel.setColour (juce::Label::textColourId, text);
    nameLabel.setColour (juce::Label::backgroundColourId, juce::Colour (0xff0f1011));
    nameLabel.setColour (juce::Label::outlineColourId, juce::Colour (0xff333539));
    nameLabel.setTooltip ("Doble clic para ponerle nombre (MASTER, VOZ, BUS...)");
    nameLabel.onTextChange = [this] { proc.channelName = nameLabel.getText(); };

    auto chip = [this] (const juce::String& id, const juce::String& prefix, std::vector<float> v = {}, const juce::String& unit = {})
    { auto* c = chips.add (new tmui::Chip (proc.apvts, id, prefix, std::move (v), unit)); addAndMakeVisible (c); return c; };
    chip ("lscale", {})->setTooltip ("Escala de la grafica de sonoridad");
    chip ("sync", {})->setTooltip (U ("DAW Sync: reinicia al dar play · Libre: mide siempre"));
    chip ("window", {})->setTooltip ("Tiempo visible en la grafica");
    chip ("autoscroll", "Auto Scroll")->setTooltip ("La grafica sigue el momento actual");
    chip ("graph", {})->setTooltip ("Que se dibuja: Short-Term (3 s) o Momentary (0.4 s)");
    chip ("target", "Target")->setTooltip ("Objetivo de sonoridad (Spotify/YouTube -14, Apple -16, TV -23/-24)");
    chip ("tp", "TP")->setTooltip ("Barras con True Peak (pico real entre muestras)");
    chip ("lvscale", {})->setTooltip ("Escala de las barras");
    chip ("pkhold", "PkHold")->setTooltip ("Tiempo que se mantiene la linea de pico");
    chip ("rmswin", "RMS")->setTooltip ("Ventana del RMS");
    chip ("vuref", "Ref", { -24, -22, -20, -18, -16, -14, -12, -10, -8 }, " dBFS")->setTooltip (U ("Nivel digital que marca 0 VU"));
    chip ("smin", "Min", { -120, -100, -90, -80, -60 }, " dB")->setTooltip ("Parte baja del espectro");
    chip ("smax", "Max", { -12, -6, 0, 6, 12 }, " dB")->setTooltip ("Parte alta del espectro");
    chip ("shold", "PkHold")->setTooltip ("Tiempo de los picos del espectro");
    chip ("savg", "Avg")->setTooltip ("Suavizado del espectro");

    for (juce::Component* c : { (juce::Component*) &powerBtn, (juce::Component*) &playBtn, (juce::Component*) &syncBtn, (juce::Component*) &resetBtn,
                                (juce::Component*) &undoBtn, (juce::Component*) &redoBtn, (juce::Component*) &aBtn, (juce::Component*) &bBtn,
                                (juce::Component*) &prevBtn, (juce::Component*) &nextBtn, (juce::Component*) &presetBtn, (juce::Component*) &helpBtn,
                                (juce::Component*) &specReset, (juce::Component*) &nameLabel, (juce::Component*) &loud, (juce::Component*) &stats,
                                (juce::Component*) &overview, (juce::Component*) &levels, (juce::Component*) &scope, (juce::Component*) &vuL,
                                (juce::Component*) &vuR, (juce::Component*) &spectrum })
        addAndMakeVisible (c);
    setResizable (true, true);
    setResizeLimits (W * 6 / 10, H * 6 / 10, W * 3 / 2, H * 3 / 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) W / H);
    setSize (W * 82 / 100, H * 82 / 100);
    startTimerHz (30);
}

TopMeterEditor::~TopMeterEditor() { stopTimer(); }

void TopMeterEditor::switchAB (int s)
{
    if (s == proc.abSlot) return;
    proc.abStore[proc.abSlot] = proc.apvts.copyState();
    if (proc.abStore[s].isValid()) proc.apvts.replaceState (proc.abStore[s].createCopy());
    proc.abSlot = s;
}

void TopMeterEditor::resized()
{
    const float s = (float) getWidth() / (float) W;
    const auto T = juce::AffineTransform::scale (s);
    auto put = [&] (juce::Component& c, int x, int y, int w, int h) { c.setBounds (x, y, w, h); c.setTransform (T); };
    put (powerBtn, 12, 12, 40, 40);
    put (nameLabel, 300, 12, 330, 40);
    put (playBtn, 646, 12, 40, 40); put (syncBtn, 690, 12, 40, 40);
    put (resetBtn, 744, 12, 100, 40);
    // barra de sonoridad
    const int ly = 68, lh = 24;
    put (*chips[0], 30, ly, 112, lh); put (*chips[1], 146, ly, 104, lh); put (*chips[2], 254, ly, 96, lh); put (*chips[3], 354, ly, 110, lh);
    put (*chips[4], 468, ly, 110, lh); put (*chips[5], 740, ly, 130, lh);
    put (loud, 4, 98, 860, 214);
    put (stats, 868, 98, 188, 214);
    put (overview, 4, 318, 860, 22);
    // barra de nivel
    const int vy = 348;
    put (*chips[6], 30, vy, 32, lh); put (*chips[7], 66, vy, 104, lh); put (*chips[8], 296, vy, 112, lh); put (*chips[9], 412, vy, 120, lh);
    put (*chips[10], 790, vy, 160, lh);
    put (levels, 8, 376, 290, 240);
    put (scope, 300, 376, 236, 248);
    put (vuL, 548, 382, 232, 238);
    put (vuR, 814, 382, 232, 238);
    // barra del espectro
    const int sy = 630;
    put (specReset, 6, sy, 26, lh);
    put (*chips[11], 36, sy, 136, lh); put (*chips[12], 176, sy, 116, lh); put (*chips[13], 296, sy, 116, lh); put (*chips[14], 416, sy, 140, lh);
    put (spectrum, 4, 658, 1052, 244);
    // pie
    put (undoBtn, 8, 908, 32, 32); put (redoBtn, 44, 908, 32, 32);
    put (aBtn, 132, 908, 30, 32); put (bBtn, 166, 908, 30, 32);
    put (prevBtn, 212, 908, 32, 32); put (presetBtn, 250, 908, 380, 32); put (nextBtn, 636, 908, 32, 32);
    put (helpBtn, 1016, 908, 32, 32);
}

void TopMeterEditor::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale ((float) getWidth() / (float) W));
    g.fillAll (bg);
    // cabecera
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff35373c), 0, 0, juce::Colour (0xff232427), 0, 62, false));
    g.fillRect (0, 0, W, 62);
    g.setColour (juce::Colours::black); g.drawHorizontalLine (62, 0, (float) W);
    g.setColour (text); g.setFont (font (25.0f, true));
    g.drawText ("TOP", 62, 14, 60, 34, juce::Justification::centredLeft);
    g.setColour (dim); g.setFont (font (25.0f));
    g.drawText ("METER", 116, 14, 100, 34, juce::Justification::centredLeft);
    // numero de instancia
    g.setColour (juce::Colour (0xff0f1011)); g.fillRoundedRectangle (236, 12, 54, 40, 4);
    g.setColour (juce::Colour (0xff333539)); g.drawRoundedRectangle (236, 12, 54, 40, 4, 1.0f);
    g.setColour (text); g.setFont (font (24.0f));
    g.drawText (juce::String (proc.instanceNumber), 236, 12, 54, 40, juce::Justification::centred);
    // marca propia
    g.setColour (juce::Colour (0xffd6d8dc)); g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultSerifFontName(), 23.0f, juce::Font::plain)));
    g.drawText ("Dj Pava Hm Music", 856, 8, 196, 30, juce::Justification::centredRight);
    g.setColour (dim); g.setFont (font (10.5f, true).withExtraKerningFactor (0.3f));
    g.drawText ("LOUDNESS  METER", 856, 36, 196, 16, juce::Justification::centredRight);
    // barras de opciones
    for (int y : { 66, 346, 628 })
    {
        g.setColour (juce::Colour (0xff2f3034)); g.fillRect (0, y, W, 28);
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawHorizontalLine (y, 0, (float) W); g.drawHorizontalLine (y + 28, 0, (float) W);
    }
    g.setColour (text); g.setFont (font (13.0f, true));
    g.drawText ("LOUDNESS", 586, 66, 140, 28, juce::Justification::centredLeft);
    g.drawText ("LEVEL", 184, 346, 100, 28, juce::Justification::centredLeft);
    g.drawText ("VU", 560, 346, 60, 28, juce::Justification::centredLeft);
    g.drawText ("SPECTRUM 1/3 OCT", 570, 628, 160, 28, juce::Justification::centredLeft);
    // banda del espectro seleccionada
    {
        const int b = spectrum.hoverBand >= 0 ? spectrum.hoverBand : spectrum.selected;
        const auto r1 = juce::Rectangle<float> (740, 631, 120, 22), r2 = juce::Rectangle<float> (866, 631, 90, 22), r3 = juce::Rectangle<float> (962, 631, 92, 22);
        box (g, r1, b >= 0 ? juce::Colour (0xffcbb8ff) : juce::Colour (0xffb9a6ef));
        g.setColour (juce::Colour (0xff1b1430)); g.setFont (font (12.0f, true));
        const float fc = b >= 0 ? tmui::SpectrumView::centre (b) : 0;
        g.drawText (b < 0 ? "Sin banda" : (fc >= 1000 ? juce::String (fc / 1000.0f, fc < 10000 ? 1 : 0) + " kHz" : juce::String ((int) std::round (fc)) + " Hz"), r1, juce::Justification::centred);
        box (g, r2, juce::Colour (0xff4a4660)); box (g, r3, juce::Colour (0xff4a4660));
        g.setColour (juce::Colour (0xffcbb8ff)); g.setFont (font (12.0f, true));
        g.drawText ("now", r2.reduced (6, 0), juce::Justification::centredLeft);
        g.drawText ("max", r3.reduced (6, 0), juce::Justification::centredLeft);
        g.setColour (text);
        g.drawText (b < 0 ? juce::String ("--") : fmt (spectrum.level[(size_t) b]), r2.reduced (6, 0), juce::Justification::centredRight);
        g.drawText (b < 0 ? juce::String ("--") : fmt (spectrum.maxv[(size_t) b]), r3.reduced (6, 0), juce::Justification::centredRight);
    }
    // separadores de secciones
    g.setColour (juce::Colours::black.withAlpha (0.5f)); g.drawVerticalLine (542, 376, 624);
    // pie
    g.setColour (juce::Colour (0xff17181a)); g.fillRect (0, 904, W, H - 904);
    g.setColour (juce::Colours::black); g.drawHorizontalLine (904, 0, (float) W);
    g.setColour (juce::Colour (0xff2b2c30)); g.drawVerticalLine (204, 904, (float) H); g.drawVerticalLine (676, 904, (float) H);
    g.setColour (dim); g.setFont (font (12.0f));
    g.drawText ("v1.0.0", 940, 912, 70, 24, juce::Justification::centredRight);
    g.drawText (proc.hostPlaying.load() ? "DAW: reproduciendo" : "DAW: detenido", 690, 912, 200, 24, juce::Justification::centredLeft);
}

void TopMeterEditor::timerCallback()
{
    const bool run = proc.apvts.getRawParameterValue ("run")->load() > 0.5f;
    if (playBtn.on != run) { playBtn.on = run; playBtn.repaint(); powerBtn.on = run; powerBtn.repaint(); }
    const bool sync = proc.apvts.getRawParameterValue ("sync")->load() < 0.5f;
    if (syncBtn.on != sync) { syncBtn.on = sync; syncBtn.repaint(); }
    if (proc.apvts.getRawParameterValue ("autoscroll")->load() > 0.5f) loud.viewEnd = -1;
    aBtn.on = proc.abSlot == 0; bBtn.on = proc.abSlot == 1;
    aBtn.label = "A"; bBtn.label = "B";
    aBtn.accentText = aBtn.on; bBtn.accentText = bBtn.on; aBtn.on = bBtn.on = false;
    aBtn.repaint(); bBtn.repaint();
    const auto pn = proc.getProgramName (proc.getCurrentProgram());
    if (presetBtn.label != pn) { presetBtn.label = pn; presetBtn.repaint(); }
    loud.repaint(); stats.repaint(); overview.repaint();
    levels.tick(); scope.repaint(); vuL.tick(); vuR.tick(); spectrum.tick();
    repaint (juce::Rectangle<int> (730, 628, 330, 28).transformedBy (juce::AffineTransform::scale ((float) getWidth() / (float) W)));
    repaint (juce::Rectangle<int> (680, 904, 260, 44).transformedBy (juce::AffineTransform::scale ((float) getWidth() / (float) W)));
    if (++transactionTick >= 15)
    {
        transactionTick = 0;
        if (! juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown()) proc.undoManager.beginNewTransaction();
    }
}
