#include "SpqEditor.h"

using namespace spqui;
using juce::Colour; namespace Colours = juce::Colours; using juce::Graphics; using juce::Path; using juce::Rectangle;
using Pt = juce::Point<float>;

namespace
{
    juce::String U (const char* s) { return juce::String::fromUTF8 (s); }
    const Colour bg (0xff101216), panel (0xff171a20), panel2 (0xff1e222a), text (0xffd9dde4), dim (0xff7d8593), accent (0xff35d0ff), warm (0xffffa23a);
    constexpr double fMin = 10.0, fMax = 30000.0;
    juce::Font ui (float h, bool bold = false) { return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain)); }
    juce::String fStr (double f) { return f >= 1000 ? juce::String (f / 1000.0, f >= 10000 ? 1 : 2) + " kHz" : juce::String (f, f < 100 ? 1 : 0) + " Hz"; }
    juce::String noteStr (double f)
    {
        static const char* nm[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const double m = spq::noteOf (f); const int r = (int) std::lround (m); const int cents = (int) std::lround ((m - r) * 100);
        return juce::String (nm[((r % 12) + 12) % 12]) + juce::String (r / 12 - 1) + (cents >= 0 ? " +" : " ") + juce::String (cents) + "c";
    }
}

Colour spqui::bandColour (int i)
{
    static const uint32_t c[] { 0xffff5a5a, 0xffff9a3c, 0xffffd23c, 0xffa6e23c, 0xff3ce283, 0xff3ce2d6, 0xff3cb4ff, 0xff6a7bff, 0xffb06aff, 0xffff6ad8,
                                0xffff7f7f, 0xffffb46b, 0xfff0e06b, 0xffbde86b, 0xff6be8a2, 0xff6be8e0, 0xff6bc8ff, 0xff8f9bff, 0xffc79bff, 0xffff9be4,
                                0xffe05050, 0xffe09040, 0xff50e0a0, 0xff50a0e0 };
    return Colour (c[i % 24]);
}

// =====================================================================================================
Look::Look()
{
    setColour (juce::PopupMenu::backgroundColourId, Colour (0xff1b1f26));
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colour (0xff2a6f8a));
    setColour (juce::PopupMenu::headerTextColourId, dim);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::backgroundColourId, panel2);
    setColour (juce::ComboBox::outlineColourId, Colour (0xff2c323c));
    setColour (juce::ComboBox::arrowColourId, dim);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, Colours::black);
    setColour (juce::TextButton::buttonColourId, panel2);
    setColour (juce::TextButton::buttonOnColourId, accent);
    setColour (juce::TooltipWindow::backgroundColourId, Colour (0xff1b1f26));
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (juce::Label::textColourId, text);
}
void Look::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const auto r = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const float rad = std::min (r.getWidth(), r.getHeight()) * 0.5f, cx = r.getCentreX(), cy = r.getCentreY();
    const Colour ac = s.findColour (juce::Slider::rotarySliderFillColourId);
    Path track; track.addCentredArc (cx, cy, rad - 2, rad - 2, 0, a0, a1, true);
    g.setColour (Colour (0xff2b313b)); g.strokePath (track, juce::PathStrokeType (3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const bool bip = s.getMinimum() < 0 && s.getMaximum() > 0;
    const float aMid = bip ? a0 + (a1 - a0) * (float) s.valueToProportionOfLength (0.0) : a0, aNow = a0 + pos * (a1 - a0);
    Path val; val.addCentredArc (cx, cy, rad - 2, rad - 2, 0, std::min (aMid, aNow), std::max (aMid, aNow), true);
    g.setColour (ac); g.strokePath (val, juce::PathStrokeType (3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setGradientFill (juce::ColourGradient (Colour (0xff3a414d), cx, cy - rad, Colour (0xff1c2027), cx, cy + rad, false));
    g.fillEllipse (cx - rad + 6, cy - rad + 6, (rad - 6) * 2, (rad - 6) * 2);
    g.setColour (Colours::white.withAlpha (0.85f));
    g.drawLine ({ { cx + (rad - 11) * std::sin (aNow), cy - (rad - 11) * std::cos (aNow) }, { cx + (rad - 6.5f) * std::sin (aNow), cy - (rad - 6.5f) * std::cos (aNow) } }, 2.0f);
}
void Look::drawButtonBackground (Graphics& g, juce::Button& b, const Colour&, bool over, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    Colour c = on ? b.findColour (juce::TextButton::buttonOnColourId) : panel2;
    if (over && ! on) c = c.brighter (0.15f);
    if (down) c = c.darker (0.2f);
    g.setColour (c); g.fillRoundedRectangle (r, 4.0f);
    g.setColour (Colour (0xff2c323c)); g.drawRoundedRectangle (r, 4.0f, 1.0f);
}
void Look::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& c)
{
    const auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (c.findColour (juce::ComboBox::backgroundColourId)); g.fillRoundedRectangle (r, 4.0f);
    g.setColour (Colour (0xff2c323c)); g.drawRoundedRectangle (r, 4.0f, 1.0f);
    Path a; const float ax = (float) w - 12, ay = (float) h * 0.5f; a.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (dim); g.fillPath (a);
}
juce::Font Look::getComboBoxFont (juce::ComboBox&) { return ui (13.0f); }
juce::Font Look::getTextButtonFont (juce::TextButton&, int) { return ui (12.0f, true); }
juce::Font Look::getPopupMenuFont() { return ui (14.0f); }
void Look::drawLabel (Graphics& g, juce::Label& l)
{
    g.setColour (l.findColour (juce::Label::textColourId)); g.setFont (l.getFont());
    g.drawFittedText (l.getText(), l.getLocalBounds(), l.getJustificationType(), 1, 0.9f);
}

// =====================================================================================================
Graph::Graph (SpqProcessor& p) : proc (p)
{
    fftBuf.assign (2 * 8192, 0.0f); win.resize (8192);
    for (int i = 0; i < 8192; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi * i / 8192.0f);
    specPre.assign (400, -150.0f); specPost.assign (400, -150.0f);
    setWantsKeyboardFocus (false);
}
float Graph::xOfF (double f) const { return (float) (std::log (f / fMin) / std::log (fMax / fMin)) * (float) getWidth(); }
double Graph::fOfX (float x) const { return fMin * std::pow (fMax / fMin, juce::jlimit (0.0, 1.0, (double) x / getWidth())); }
float Graph::yOfDb (double db) const { const float h = (float) getHeight() - 28.0f; return 14.0f + h * 0.5f - (float) (db / (dbRange * 1.1)) * h * 0.5f; }
double Graph::dbOfY (float y) const { const float h = (float) getHeight() - 28.0f; return (double) ((14.0f + h * 0.5f - y) / (h * 0.5f)) * dbRange * 1.1; }
bool Graph::hasGain (int t) const { return t == spq::Bell || t == spq::LowShelf || t == spq::HighShelf || t == spq::TiltShelf || t == spq::FlatTilt; }

double Graph::bandDb (int i, double f, double extra) const
{
    auto b = proc.bandParams (i);
    spq::ASec as[spq::maxSec]; const int n = spq::analogSections (b, b.g + extra, as);
    return 20.0 * std::log10 (std::max (1e-9, spq::analogMag (as, n, f)));
}
Pt Graph::nodePos (int i) const
{
    const auto b = proc.bandParams (i);
    double y = 0;
    if (hasGain (b.type)) y = b.type == spq::FlatTilt || b.type == spq::TiltShelf ? b.g * 0.5 : (b.type == spq::Bell ? b.g : b.g * 0.5);
    else if (b.type == spq::LowCut || b.type == spq::HighCut) y = 0;
    return { xOfF (b.f), yOfDb (juce::jlimit ((double) -dbRange * 1.05, (double) dbRange * 1.05, y)) };
}
int Graph::hitNode (Pt p) const
{
    int best = -1; float bd = 12.0f * 12.0f;
    for (int i = 0; i < SpqProcessor::numBands; ++i)
    {
        if (! proc.bandParams (i).on) continue;
        const float d = nodePos (i).getDistanceSquaredFrom (p);
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}
void Graph::setP (int band, const char* what, float v)
{
    if (auto* q = proc.apvts.getParameter (SpqProcessor::pid (band, what))) q->setValueNotifyingHost (q->convertTo0to1 (v));
}
int Graph::createBand (double f, double g, int type)
{
    for (int i = 0; i < SpqProcessor::numBands; ++i)
        if (! proc.bandParams (i).on)
        {
            proc.undo.beginNewTransaction ("Nueva banda");
            for (auto* prm : proc.getParameters())
                if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
                    if (r->getParameterID().startsWith ("b" + juce::String (i) + "_")) r->setValueNotifyingHost (r->getDefaultValue());
            setP (i, "type", (float) type); setP (i, "freq", (float) f); setP (i, "gain", hasGain (type) ? (float) g : 0.0f);
            setP (i, "q", (type == spq::LowCut || type == spq::HighCut || (type >= spq::LowShelf && type <= spq::HighShelf)) ? 0.71f : 1.0f);
            setP (i, "slope", type == spq::LowCut || type == spq::HighCut ? 3.0f : 1.0f);
            setP (i, "on", 1.0f);
            return i;
        }
    return -1;
}
double Graph::peakNear (double f) const
{
    // busca el pico del analizador (salida) a +-1/3 de octava
    const int n = (int) specPost.size();
    auto idxOf = [&] (double fr) { return juce::jlimit (0, n - 1, (int) std::lround (std::log (fr / fMin) / std::log (fMax / fMin) * (n - 1))); };
    int a = idxOf (f / 1.26), b = idxOf (f * 1.26), best = idxOf (f);
    for (int i = a; i <= b; ++i) if (specPost[(size_t) i] > specPost[(size_t) best]) best = i;
    return fMin * std::pow (fMax / fMin, best / (double) (n - 1));
}

void Graph::updateAnalyzer()
{
    if (anaMode == 0) return;
    const int w = proc.anaW.load();
    for (int pass = 0; pass < 2; ++pass)
    {
        if ((pass == 0 && ! (anaMode & 1)) || (pass == 1 && ! (anaMode & 2))) continue;
        const auto& src = pass == 0 ? proc.anaPre : proc.anaPost;
        auto& dst = pass == 0 ? specPre : specPost;
        for (int i = 0; i < 8192; ++i) fftBuf[(size_t) i] = src[(size_t) ((w - 8192 + i + SpqProcessor::anaSize) & (SpqProcessor::anaSize - 1))] * win[(size_t) i];
        std::fill (fftBuf.begin() + 8192, fftBuf.end(), 0.0f);
        fft.performFrequencyOnlyForwardTransform (fftBuf.data(), true);
        const double fs = proc.fsNow(), binHz = fs / 8192.0;
        const int n = (int) dst.size();
        for (int p = 0; p < n; ++p)
        {
            const double f0 = fMin * std::pow (fMax / fMin, p / (double) n), f1 = fMin * std::pow (fMax / fMin, (p + 1) / (double) n);
            int k0 = (int) std::floor (f0 / binHz), k1 = std::max (k0, (int) std::floor (f1 / binHz));
            float m = 0;
            if (k0 >= 4096) m = 0;
            else for (int k = k0; k <= std::min (k1, 4095); ++k) m = std::max (m, fftBuf[(size_t) k]);
            float db = 20.0f * std::log10 (m * 4.0f / 8192.0f + 1e-9f) + 4.5f * (float) std::log2 (std::max (f0, 1.0) / 1000.0);   // +4.5 dB/oct
            float& o = dst[(size_t) p];
            o = db > o ? db : std::max (db, o - 1.1f);
        }
    }
    repaint();
}

void Graph::paint (Graphics& g)
{
    const float W = (float) getWidth(), H = (float) getHeight();
    g.setGradientFill (juce::ColourGradient (Colour (0xff141820), 0, 0, Colour (0xff0c0e12), 0, H, false));
    g.fillAll();
    // ---- rejilla
    g.setFont (ui (10.5f));
    for (double f : { 20.0, 30.0, 40.0, 50.0, 60.0, 80.0, 100.0, 200.0, 300.0, 400.0, 500.0, 600.0, 800.0, 1000.0, 2000.0, 3000.0, 4000.0, 5000.0, 6000.0, 8000.0, 10000.0, 20000.0 })
    {
        const bool major = f == 100 || f == 1000 || f == 10000;
        g.setColour (Colours::white.withAlpha (major ? 0.10f : 0.045f));
        g.drawVerticalLine ((int) xOfF (f), 0, H);
        if (f == 20 || f == 50 || f == 100 || f == 200 || f == 500 || f == 1000 || f == 2000 || f == 5000 || f == 10000 || f == 20000)
        { g.setColour (dim); g.drawText (f >= 1000 ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f), (int) xOfF (f) + 3, (int) H - 15, 40, 12, juce::Justification::left); }
    }
    const float step = dbRange <= 3 ? 1.0f : dbRange <= 6 ? 2.0f : dbRange <= 12 ? 3.0f : 6.0f;
    for (float d = -dbRange; d <= dbRange + 0.01f; d += step)
    {
        g.setColour (Colours::white.withAlpha (d == 0 ? 0.16f : 0.05f));
        g.drawHorizontalLine ((int) yOfDb (d), 0, W);
        g.setColour (dim); g.drawText ((d > 0 ? "+" : "") + juce::String ((int) d), (int) W - 34, (int) yOfDb (d) - 6, 30, 12, juce::Justification::right);
    }
    // ---- analizador (escala propia: 0 a -90 dB)
    auto anaY = [H] (float db) { return juce::jmap (juce::jlimit (-100.0f, 6.0f, db), -96.0f, 6.0f, H, 0.0f); };
    if (anaMode & 1)
    {
        Path p; p.startNewSubPath (0, H);
        for (int i = 0; i < (int) specPre.size(); ++i) p.lineTo ((float) i / (float) specPre.size() * W, anaY (specPre[(size_t) i]));
        p.lineTo (W, H); p.closeSubPath();
        g.setColour (Colour (0xff4a5263).withAlpha (0.35f)); g.fillPath (p);
    }
    if (anaMode & 2)
    {
        Path p; p.startNewSubPath (0, H);
        for (int i = 0; i < (int) specPost.size(); ++i) p.lineTo ((float) i / (float) specPost.size() * W, anaY (specPost[(size_t) i]));
        p.lineTo (W, H); p.closeSubPath();
        g.setGradientFill (juce::ColourGradient (accent.withAlpha (0.20f), 0, H * 0.3f, accent.withAlpha (0.03f), 0, H, false)); g.fillPath (p);
        Path l; for (int i = 0; i < (int) specPost.size(); ++i) { const Pt q ((float) i / (float) specPost.size() * W, anaY (specPost[(size_t) i])); if (i == 0) l.startNewSubPath (q); else l.lineTo (q); }
        g.setColour (accent.withAlpha (0.45f)); g.strokePath (l, juce::PathStrokeType (1.0f));
    }
    // ---- curvas de cada banda (relleno) + curva total
    constexpr int NP = 360;
    std::vector<double> total (NP, 0.0), side (NP, 0.0); bool anySide = false, any = false;
    for (int b = 0; b < SpqProcessor::numBands; ++b)
    {
        const auto bp = proc.bandParams (b);
        if (! bp.on) continue;
        any = true;
        const double dyn = proc.dynNow[(size_t) b].load();
        Path fill; fill.startNewSubPath (0, yOfDb (0));
        Path dline; bool hasDyn = bp.dyn && std::abs (dyn) > 0.05;
        for (int i = 0; i < NP; ++i)
        {
            const double f = fMin * std::pow (fMax / fMin, i / (double) (NP - 1));
            const double d = bandDb (b, f, 0.0);
            const float x = (float) i / (NP - 1) * W;
            fill.lineTo (x, yOfDb (juce::jlimit (-60.0, 60.0, d)));
            const double dd = hasDyn ? bandDb (b, f, dyn) : d;
            if (hasDyn) { if (i == 0) dline.startNewSubPath (x, yOfDb (dd)); else dline.lineTo (x, yOfDb (juce::jlimit (-60.0, 60.0, dd))); }
            if (bp.place == spq::SideCh) { side[(size_t) i] += dd; anySide = true; } else total[(size_t) i] += dd;
        }
        fill.lineTo (W, yOfDb (0)); fill.closeSubPath();
        const auto c = bandColour (b);
        const bool hot = b == selected || b == hover;
        const bool cutT = bp.type == spq::LowCut || bp.type == spq::HighCut || bp.type == spq::BandPass || bp.type == spq::Notch;
        g.setColour (c.withAlpha (hot ? 0.20f : (cutT ? 0.045f : 0.09f))); g.fillPath (fill);
        if (hot) { g.setColour (c.withAlpha (0.8f)); g.strokePath (fill, juce::PathStrokeType (1.0f)); }
        if (hasDyn) { g.setColour (c.withAlpha (0.95f)); g.strokePath (dline, juce::PathStrokeType (1.6f)); }
    }
    // reduccion espectral en vivo (rojo) sobre la curva total
    {
        bool sp = false; for (int k = 0; k < SpqProcessor::dispPts; ++k) if (std::abs (proc.specNow[(size_t) k].load()) > 0.05f) { sp = true; break; }
        if (sp)
        {
            Path a, top;
            for (int i = 0; i < NP; ++i)
            {
                const double f = fMin * std::pow (fMax / fMin, i / (double) (NP - 1));
                const double pos = std::log (std::max (20.0, std::min (20000.0, f)) / 20.0) / std::log (1000.0) * (SpqProcessor::dispPts - 1);
                const float s = proc.specNow[(size_t) juce::jlimit (0, SpqProcessor::dispPts - 1, (int) std::lround (pos))].load();
                const float x = (float) i / (NP - 1) * W;
                if (i == 0) { a.startNewSubPath (x, yOfDb (total[0])); top.startNewSubPath (x, yOfDb (total[0] + s)); }
                a.lineTo (x, yOfDb (total[(size_t) i] + s)); top.lineTo (x, yOfDb (total[(size_t) i] + s));
                total[(size_t) i] += s;
            }
            for (int i = NP - 1; i >= 0; --i) a.lineTo ((float) i / (NP - 1) * W, yOfDb (total[(size_t) i] - proc.specNow[(size_t) juce::jlimit (0, SpqProcessor::dispPts - 1, (int) std::lround (std::log (std::max (20.0, std::min (20000.0, fMin * std::pow (fMax / fMin, i / (double) (NP - 1)))) / 20.0) / std::log (1000.0) * (SpqProcessor::dispPts - 1)))].load()));
            a.closeSubPath();
            g.setColour (Colour (0xffff4040).withAlpha (0.35f)); g.fillPath (a);
        }
    }
    auto strokeTotal = [&] (const std::vector<double>& t, Colour c, float w)
    {
        Path p; for (int i = 0; i < NP; ++i) { const Pt q ((float) i / (NP - 1) * W, yOfDb (juce::jlimit (-60.0, 60.0, t[(size_t) i]))); if (i == 0) p.startNewSubPath (q); else p.lineTo (q); }
        g.setColour (c.withAlpha (0.25f)); g.strokePath (p, juce::PathStrokeType (w + 3.0f));
        g.setColour (c); g.strokePath (p, juce::PathStrokeType (w));
    };
    if (anySide) strokeTotal (side, Colour (0xffb08cff), 1.6f);
    strokeTotal (total, Colour (0xfff2f4f8), 2.0f);
    // ---- nodos
    for (int b = 0; b < SpqProcessor::numBands; ++b)
    {
        const auto bp = proc.bandParams (b);
        if (! bp.on) continue;
        const Pt p = nodePos (b);
        const auto c = bandColour (b);
        if (bp.dyn || bp.spectral)                                              // rango dinamico: barra vertical
        {
            const float y0 = p.y, y1 = yOfDb (juce::jlimit (-60.0, 60.0, (bp.type == spq::Bell ? bp.g : bp.g * 0.5) + bp.range));
            g.setColour (c.withAlpha (0.5f)); g.drawLine (p.x, y0, p.x, y1, 2.0f);
            g.fillEllipse (p.x - 3, y1 - 3, 6, 6);
        }
        const float r = b == selected ? 9.0f : 7.5f;
        g.setColour (Colours::black.withAlpha (0.5f)); g.fillEllipse (p.x - r - 1.5f, p.y - r - 0.5f, (r + 1.5f) * 2, (r + 1.5f) * 2);
        g.setColour (c); g.fillEllipse (p.x - r, p.y - r, r * 2, r * 2);
        if (b == selected) { g.setColour (Colours::white); g.drawEllipse (p.x - r - 3, p.y - r - 3, (r + 3) * 2, (r + 3) * 2, 1.6f); }
        if (bp.spectral) { g.setColour (Colour (0xffff4040)); g.drawEllipse (p.x - r - 1, p.y - r - 1, (r + 1) * 2, (r + 1) * 2, 1.4f); }
        g.setColour (Colours::black); g.setFont (ui (10.0f, true));
        g.drawText (juce::String (b + 1), Rectangle<float> (p.x - r, p.y - r, r * 2, r * 2), juce::Justification::centred);
        const char* pl[] { "", "L", "R", "M", "S" };
        if (bp.place != 0) { g.setColour (c); g.drawText (pl[bp.place], Rectangle<float> (p.x + r, p.y - r - 10, 14, 12), juce::Justification::left); }
    }
    // ---- ayuda / lectura
    if (! any)
    {
        g.setColour (dim); g.setFont (ui (15.0f));
        g.drawText (U ("Doble clic en la curva para crear una banda  ·  Shift + arrastrar: atrapa un pico del espectro"), getLocalBounds().withTrimmedTop (getHeight() / 2 - 40).withHeight (30), juce::Justification::centred);
    }
    const int show = dragging >= 0 ? dragging : hover;
    if (show >= 0 || grab)
    {
        juce::String s;
        if (show >= 0)
        {
            const auto bp = proc.bandParams (show);
            s = "Banda " + juce::String (show + 1) + "   " + fStr (bp.f) + "  (" + noteStr (bp.f) + ")";
            if (hasGain (bp.type)) s += "   " + juce::String (bp.g, 1) + " dB";
            s += "   Q " + juce::String (bp.q, 2);
            if (bp.dyn || bp.spectral) s += "   din " + juce::String (proc.dynNow[(size_t) show].load(), 1) + " dB";
        }
        else s = "Pico " + fStr (peakNear (fOfX (mousePos.x)));
        g.setFont (ui (12.5f));
        const int tw = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), s) + 16;
        Rectangle<float> box (juce::jlimit (4.0f, W - (float) tw - 4, mousePos.x + 14), juce::jlimit (4.0f, H - 26, mousePos.y - 30), (float) tw, 22);
        g.setColour (Colour (0xe0181c23)); g.fillRoundedRectangle (box, 4);
        g.setColour (text); g.drawText (s, box, juce::Justification::centred);
    }
}

void Graph::mouseMove (const juce::MouseEvent& e)
{
    mousePos = e.position;
    const int h = hitNode (e.position);
    if (h != hover) { hover = h; }
    repaint();
}
void Graph::mouseDown (const juce::MouseEvent& e)
{
    mousePos = downPos = e.position;
    dragging = hitNode (e.position);
    if (dragging >= 0)
    {
        if (e.mods.isPopupMenu()) { bandMenu (dragging); dragging = -1; return; }
        proc.undo.beginNewTransaction ("Mover banda");
        const auto bp = proc.bandParams (dragging);
        downF = bp.f; downG = proc.apvts.getRawParameterValue (SpqProcessor::pid (dragging, "gain"))->load(); downQ = bp.q;
        for (auto* w : { "freq", "gain", "q" }) if (auto* q = proc.apvts.getParameter (SpqProcessor::pid (dragging, w))) q->beginChangeGesture();
        selected = dragging; if (onSelect) onSelect (selected);
    }
    else if (e.mods.isShiftDown())                                              // atrapar un pico del espectro
    {
        grab = true;
        const double f = peakNear (fOfX (e.position.x));
        const int b = createBand (f, 0.0, spq::Bell);
        if (b >= 0) { setP (b, "q", 4.0f); dragging = b; selected = b; downF = f; downG = 0; downQ = 4; if (onSelect) onSelect (b); }
    }
    else { selected = -1; if (onSelect) onSelect (-1); }
    repaint();
}
void Graph::mouseDrag (const juce::MouseEvent& e)
{
    mousePos = e.position;
    if (dragging < 0) return;
    const auto bp = proc.bandParams (dragging);
    const float fine = e.mods.isShiftDown() && ! grab ? 0.15f : 1.0f;
    const Pt d = (e.position - downPos) * fine;
    if (! grab)
    {
        const double f = juce::jlimit (10.0, 30000.0, std::exp (std::log (downF) + (double) d.x / getWidth() * std::log (fMax / fMin)));
        setP (dragging, "freq", (float) f);
    }
    if (hasGain (bp.type))
    {
        const double k = (bp.type == spq::Bell ? 1.0 : 2.0) / (proc.apvts.getRawParameterValue ("scale")->load() * 0.01 + 1e-6);
        setP (dragging, "gain", (float) juce::jlimit (-30.0, 30.0, downG + (dbOfY (downPos.y + d.y) - dbOfY (downPos.y)) * k));
    }
    else setP (dragging, "q", (float) juce::jlimit (0.025, 40.0, downQ * std::pow (2.0, -(double) d.y / 60.0)));
    repaint();
}
void Graph::mouseUp (const juce::MouseEvent&)
{
    if (dragging >= 0)
        for (auto* w : { "freq", "gain", "q" }) if (auto* q = proc.apvts.getParameter (SpqProcessor::pid (dragging, w))) q->endChangeGesture();
    dragging = -1; grab = false; repaint();
}
void Graph::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int h = hitNode (e.position);
    if (h >= 0) { proc.undo.beginNewTransaction ("Borrar banda"); setP (h, "on", 0.0f); selected = -1; if (onSelect) onSelect (-1); repaint(); return; }
    const double f = fOfX (e.position.x);
    const int type = f < 30 ? spq::LowCut : f > 16000 ? spq::HighCut : spq::Bell;
    const int b = createBand (f, juce::jlimit (-30.0, 30.0, dbOfY (e.position.y)), type);
    if (b >= 0) { selected = b; if (onSelect) onSelect (b); }
    repaint();
}
void Graph::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const int h = hitNode (e.position) >= 0 ? hitNode (e.position) : selected;
    if (h < 0) return;
    const auto bp = proc.bandParams (h);
    const float dlt = (w.deltaY != 0 ? w.deltaY : w.deltaX) * (w.isReversed ? -1.0f : 1.0f);
    if (e.mods.isCommandDown() || e.mods.isCtrlDown())                           // rango dinamico
    {
        const float r = proc.apvts.getRawParameterValue (SpqProcessor::pid (h, "range"))->load();
        setP (h, "range", juce::jlimit (-30.0f, 30.0f, r + dlt * 6.0f));
    }
    else if ((bp.type == spq::LowCut || bp.type == spq::HighCut) && e.mods.isAltDown())
        setP (h, "slope", (float) juce::jlimit (0, spq::numSlopes - 1, bp.slope + (dlt > 0 ? 1 : -1)));
    else setP (h, "q", (float) juce::jlimit (0.025, 40.0, bp.q * std::pow (2.0, (double) dlt * 1.5)));
    repaint();
}
void Graph::bandMenu (int i)
{
    juce::PopupMenu m, t, s, p, d;
    const auto bp = proc.bandParams (i);
    const char* tn[] { "Bell", "Low Shelf", "High Shelf", "Low Cut", "High Cut", "Notch", "Band Pass", "Tilt Shelf", "Flat Tilt", "All Pass" };
    for (int k = 0; k < spq::NumTypes; ++k) t.addItem (100 + k, tn[k], true, bp.type == k);
    for (int k = 0; k < spq::numSlopes; ++k) s.addItem (200 + k, juce::String (spq::slopeDb[k]) + " dB/oct", true, bp.slope == k);
    const char* pn[] { "Stereo", "Left", "Right", "Mid", "Side" };
    for (int k = 0; k < 5; ++k) p.addItem (300 + k, pn[k], true, bp.place == k);
    const int dm = proc.dynModeOf (i);
    d.addItem (400, "Off", true, dm == 0); d.addItem (401, "Dynamic EQ", true, dm == 1); d.addItem (402, "Spectral", true, dm == 2);
    m.addSectionHeader ("Banda " + juce::String (i + 1));
    m.addSubMenu ("Tipo", t); m.addSubMenu ("Pendiente", s); m.addSubMenu (U ("Estéreo"), p); m.addSubMenu (U ("Dinámica"), d);
    m.addSeparator(); m.addItem (500, "Borrar banda");
    juce::Component::SafePointer<Graph> sp (this);
    m.showMenuAsync ({}, [sp, i] (int r)
    {
        if (sp == nullptr || r == 0) return;
        sp->proc.undo.beginNewTransaction ("Banda");
        if (r >= 100 && r < 200) sp->setP (i, "type", (float) (r - 100));
        else if (r >= 200 && r < 300) sp->setP (i, "slope", (float) (r - 200));
        else if (r >= 300 && r < 400) sp->setP (i, "place", (float) (r - 300));
        else if (r >= 400 && r < 500) sp->setP (i, "dmode", (float) (r - 400));
        else if (r == 500) { sp->setP (i, "on", 0.0f); sp->selected = -1; if (sp->onSelect) sp->onSelect (-1); }
        if (sp->onSelect && sp->selected >= 0) sp->onSelect (sp->selected);
        sp->repaint();
    });
}

// =====================================================================================================
Content::Content (SpqProcessor& p) : proc (p), graph (p)
{
    addAndMakeVisible (graph);
    graph.onSelect = [this] (int b) { select (b); };
    auto combo = [this] (juce::ComboBox& c, juce::StringArray items) { c.addItemList (items, 1); addAndMakeVisible (c); };
    combo (mode, { "Zero Latency", "Linear Phase Low", "Linear Phase High" });
    combo (character, { "Clean", "Subtle", "Warm" });
    combo (ana, { "Analyzer Off", "Analyzer Pre", "Analyzer Post", "Analyzer Pre+Post" });
    combo (range, { U ("±3 dB"), U ("±6 dB"), U ("±12 dB"), U ("±30 dB") });
    modeAtt = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, "mode", mode);
    charAtt = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, "character", character);
    ana.setSelectedId (4, juce::dontSendNotification); range.setSelectedId (3, juce::dontSendNotification);
    ana.onChange = [this] { graph.anaMode = ana.getSelectedId() - 1; graph.repaint(); };
    range.onChange = [this] { const float r[] { 3, 6, 12, 30 }; graph.dbRange = r[juce::jlimit (0, 3, range.getSelectedId() - 1)]; graph.repaint(); };
    autoGain.setClickingTogglesState (true); addAndMakeVisible (autoGain);
    autoAtt = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, "autogain", autoGain);
    for (auto* b : { &undoB, &redoB, &prevP, &nextP, &presetName }) addAndMakeVisible (*b);
    undoB.onClick = [this] { proc.undo.undo(); select (graph.selected); };
    redoB.onClick = [this] { proc.undo.redo(); select (graph.selected); };
    prevP.onClick = [this] { const int n = proc.getNumPrograms(); proc.setCurrentProgram ((proc.getCurrentProgram() + n - 1) % n); select (-1); };
    nextP.onClick = [this] { const int n = proc.getNumPrograms(); proc.setCurrentProgram ((proc.getCurrentProgram() + 1) % n); select (-1); };
    presetName.onClick = [this]
    {
        juce::PopupMenu m; for (int i = 0; i < proc.getNumPrograms(); ++i) m.addItem (i + 1, proc.getProgramName (i), true, i == proc.getCurrentProgram());
        juce::Component::SafePointer<Content> sp (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetName), [sp] (int r) { if (sp != nullptr && r > 0) { sp->proc.setCurrentProgram (r - 1); sp->select (-1); } });
    };
    auto knob = [this] (juce::Slider& s, Colour c)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag); s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 16);
        s.setColour (juce::Slider::rotarySliderFillColourId, c); s.setRotaryParameters (-2.4f + 6.2832f, 2.4f + 6.2832f, true);
        s.setMouseDragSensitivity (220); addAndMakeVisible (s);
    };
    knob (scale, dim); knob (output, accent);
    scaleAtt = std::make_unique<APVTS::SliderAttachment> (proc.apvts, "scale", scale);
    outAtt = std::make_unique<APVTS::SliderAttachment> (proc.apvts, "output", output);
    for (auto* s : { &freq, &gain, &q, &thr, &rangeK, &att, &rel }) knob (*s, accent);
    combo (type, { "Bell", "Low Shelf", "High Shelf", "Low Cut", "High Cut", "Notch", "Band Pass", "Tilt Shelf", "Flat Tilt", "All Pass" });
    { juce::StringArray sl; for (int d : spq::slopeDb) sl.add (juce::String (d) + " dB/oct"); combo (slope, sl); }
    combo (place, { "Stereo", "Left", "Right", "Mid", "Side" });
    for (auto* b : { &dOff, &dDyn, &dSpec }) { b->setClickingTogglesState (false); addAndMakeVisible (*b); }
    dOff.onClick = [this] { if (sel >= 0) { proc.undo.beginNewTransaction(); if (auto* q = proc.apvts.getParameter (SpqProcessor::pid (sel, "dmode"))) q->setValueNotifyingHost (q->convertTo0to1 (0)); } };
    dDyn.onClick = [this] { if (sel >= 0) { proc.undo.beginNewTransaction(); if (auto* q = proc.apvts.getParameter (SpqProcessor::pid (sel, "dmode"))) q->setValueNotifyingHost (q->convertTo0to1 (1)); } };
    dSpec.onClick = [this] { if (sel >= 0) { proc.undo.beginNewTransaction(); if (auto* q = proc.apvts.getParameter (SpqProcessor::pid (sel, "dmode"))) q->setValueNotifyingHost (q->convertTo0to1 (2)); } };
    below.setClickingTogglesState (true); addAndMakeVisible (below);
    addAndMakeVisible (soloB); addAndMakeVisible (del);
    soloB.onStateChange = [this] { proc.solo.store (soloB.isDown() && sel >= 0 ? sel : -1); };
    del.onClick = [this] { if (sel >= 0) { proc.undo.beginNewTransaction ("Borrar"); if (auto* q = proc.apvts.getParameter (SpqProcessor::pid (sel, "on"))) q->setValueNotifyingHost (0.0f); graph.selected = -1; select (-1); } };
    mode.setTooltip (U ("Zero Latency: filtros analógicos calcados sin latencia. Linear Phase: sin giro de fase (Low ~53 ms, High ~213 ms de latencia)"));
    character.setTooltip (U ("Clean: transparente. Subtle: trafo (los graves saturan un poco). Warm: válvula (más armónicos pares)"));
    autoGain.setTooltip (U ("Compensa el volumen de la curva para comparar a igual nivel"));
    scale.setTooltip (U ("Gain Scale: escala todas las ganancias a la vez (0-200 %)")); output.setTooltip ("Output");
    dSpec.setTooltip (U ("Spectral: dentro de la banda baja solo las frecuencias que sobresalen (resonancias, dureza, sibilancia)"));
    dDyn.setTooltip (U ("Dynamic: la ganancia de la banda se mueve con el nivel (umbral, rango, ataque, liberación)"));
    below.setTooltip (U ("BELOW: actúa cuando la señal está POR DEBAJO del umbral"));
    soloB.setTooltip (U ("Mantén apretado para escuchar solo la región de la banda"));
    graph.setTooltip ({});
    setWantsKeyboardFocus (true);
    select (-1);
    startTimerHz (30);
}
Content::~Content() { stopTimer(); }

void Content::select (int b)
{
    sel = b; graph.selected = b;
    typeAtt.reset(); slopeAtt.reset(); placeAtt.reset(); fAtt.reset(); gAtt.reset(); qAtt.reset(); thrAtt.reset(); rgAtt.reset(); atAtt.reset(); rlAtt.reset(); belowAtt.reset();
    if (b >= 0 && proc.bandParams (b).on)
    {
        auto& s = proc.apvts;
        typeAtt = std::make_unique<APVTS::ComboBoxAttachment> (s, SpqProcessor::pid (b, "type"), type);
        slopeAtt = std::make_unique<APVTS::ComboBoxAttachment> (s, SpqProcessor::pid (b, "slope"), slope);
        placeAtt = std::make_unique<APVTS::ComboBoxAttachment> (s, SpqProcessor::pid (b, "place"), place);
        fAtt = std::make_unique<APVTS::SliderAttachment> (s, SpqProcessor::pid (b, "freq"), freq);
        gAtt = std::make_unique<APVTS::SliderAttachment> (s, SpqProcessor::pid (b, "gain"), gain);
        qAtt = std::make_unique<APVTS::SliderAttachment> (s, SpqProcessor::pid (b, "q"), q);
        thrAtt = std::make_unique<APVTS::SliderAttachment> (s, SpqProcessor::pid (b, "thr"), thr);
        rgAtt = std::make_unique<APVTS::SliderAttachment> (s, SpqProcessor::pid (b, "range"), rangeK);
        atAtt = std::make_unique<APVTS::SliderAttachment> (s, SpqProcessor::pid (b, "att"), att);
        rlAtt = std::make_unique<APVTS::SliderAttachment> (s, SpqProcessor::pid (b, "rel"), rel);
        belowAtt = std::make_unique<APVTS::ButtonAttachment> (s, SpqProcessor::pid (b, "below"), below);
        const auto c = bandColour (b);
        for (auto* k : { &freq, &gain, &q, &thr, &rangeK, &att, &rel }) k->setColour (juce::Slider::rotarySliderFillColourId, c);
    }
    else { sel = -1; graph.selected = -1; }
    lastDm = -1;
    refreshPanel();
    graph.repaint(); repaint();
}

void Content::refreshPanel()
{
    const bool has = sel >= 0;
    for (juce::Component* c : std::initializer_list<juce::Component*> { &type, &slope, &place, &freq, &gain, &q, &dOff, &dDyn, &dSpec, &soloB, &del })
        c->setVisible (has);
    const int dm = has ? proc.dynModeOf (sel) : 0;
    for (juce::Component* c : std::initializer_list<juce::Component*> { &thr, &rangeK, &att, &rel, &below }) c->setVisible (has && dm != 0);
    dOff.setToggleState (dm == 0, juce::dontSendNotification); dDyn.setToggleState (dm == 1, juce::dontSendNotification); dSpec.setToggleState (dm == 2, juce::dontSendNotification);
    if (has) { const int t = proc.bandParams (sel).type; slope.setEnabled (t == spq::LowCut || t == spq::HighCut || t == spq::LowShelf || t == spq::HighShelf || t == spq::BandPass || t == spq::TiltShelf);
               gain.setEnabled (t == spq::Bell || t == spq::LowShelf || t == spq::HighShelf || t == spq::TiltShelf || t == spq::FlatTilt); }
    lastDm = dm;
    presetName.setButtonText (proc.getProgramName (proc.getCurrentProgram()));
    repaint();
}

void Content::timerCallback()
{
    graph.updateAnalyzer();
    if (sel >= 0 && ! proc.bandParams (sel).on) select (-1);
    const int dm = sel >= 0 ? proc.dynModeOf (sel) : 0;
    if (dm != lastDm) refreshPanel();
    if (presetName.getButtonText() != proc.getProgramName (proc.getCurrentProgram())) refreshPanel();
}

bool Content::keyPressed (const juce::KeyPress& k)
{
    if ((k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey) && sel >= 0) { del.triggerClick(); return true; }
    if (k.getModifiers().isCommandDown() && k.getKeyCode() == 'Z') { if (k.getModifiers().isShiftDown()) proc.undo.redo(); else proc.undo.undo(); select (graph.selected); return true; }
    if (k.getModifiers().isCommandDown() && k.getKeyCode() == 'Y') { proc.undo.redo(); select (graph.selected); return true; }
    return false;
}

void Content::paint (Graphics& g)
{
    g.fillAll (bg);
    // barra superior
    g.setGradientFill (juce::ColourGradient (Colour (0xff1c2028), 0, 0, Colour (0xff15181e), 0, 46, false)); g.fillRect (0, 0, W, 46);
    g.setColour (Colour (0xff262b34)); g.drawHorizontalLine (46, 0, (float) W);
    g.setColour (text); g.drawEllipse (14, 9, 28, 28, 2.0f); g.setFont (ui (11.0f, true)); g.drawText ("HM", 14, 9, 28, 28, juce::Justification::centred);
    g.setFont (ui (16.0f, true)); g.drawText ("TOP SPECTRA EQ", 52, 6, 200, 20, juce::Justification::left);
    g.setColour (dim); g.setFont (ui (10.5f, true)); g.drawText (U ("ANALOG HM  ·  DJ PAVA HM MUSIC"), 52, 25, 220, 14, juce::Justification::left);
    g.setFont (ui (10.0f)); g.drawText ("SCALE", 1080, 3, 50, 10, juce::Justification::centred); g.drawText ("OUTPUT", 1136, 3, 56, 10, juce::Justification::centred);
    // panel inferior
    const Rectangle<int> pr (0, 500, W, H - 500);
    g.setColour (panel); g.fillRect (pr);
    g.setColour (Colour (0xff262b34)); g.drawHorizontalLine (500, 0, (float) W);
    if (sel < 0)
    {
        g.setColour (dim); g.setFont (ui (14.0f));
        g.drawFittedText (U ("Doble clic: nueva banda (en los extremos crea cortes)  ·  Arrastrar un nodo: frecuencia y ganancia (Shift = fino)  ·  Rueda: Q\n"
                               "Ctrl + rueda: rango dinámico  ·  Alt + rueda: pendiente  ·  Doble clic en un nodo: borrar  ·  Clic derecho: menú  ·  Shift + arrastrar en el vacío: atrapar un pico  ·  Ctrl+Z: deshacer"),
                          pr.reduced (20), juce::Justification::centred, 2);
        return;
    }
    const auto c = bandColour (sel);
    g.setColour (c); g.fillRoundedRectangle (16, 516, 6, 148, 3);
    g.setColour (text); g.setFont (ui (18.0f, true)); g.drawText ("BANDA " + juce::String (sel + 1), 32, 512, 120, 24, juce::Justification::left);
    g.setColour (dim); g.setFont (ui (10.5f, true));
    auto lab = [&] (const juce::String& t, int x, int y, int w) { g.drawText (t, x, y, w, 12, juce::Justification::centred); };
    lab ("FREQ", 430, 514, 76); lab ("GAIN", 514, 514, 76); lab ("Q", 598, 514, 76);
    if (proc.dynModeOf (sel) != 0) { lab ("THRESHOLD", 850, 514, 76); lab ("RANGE", 934, 514, 76); lab ("ATTACK", 1018, 514, 76); lab ("RELEASE", 1102, 514, 76); }
    lab (U ("DINÁMICA"), 700, 514, 130);
    g.setColour (Colour (0xff262b34)); g.drawVerticalLine (690, 512, 668); g.drawVerticalLine (418, 512, 668);
}

void Content::resized()
{
    graph.setBounds (0, 47, W, 452);
    prevP.setBounds (290, 10, 26, 26); presetName.setBounds (318, 10, 230, 26); nextP.setBounds (550, 10, 26, 26);
    undoB.setBounds (588, 10, 56, 26); redoB.setBounds (646, 10, 56, 26);
    mode.setBounds (716, 10, 140, 26); character.setBounds (862, 10, 74, 26); ana.setBounds (942, 10, 128, 26);
    range.setBounds (1076, 30, 0, 0); range.setVisible (false);
    autoGain.setBounds (720, 0, 0, 0); autoGain.setVisible (false);
    scale.setBounds (1080, 10, 50, 36); output.setBounds (1136, 10, 56, 36);
    scale.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0); output.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    // la escala del grafico y auto gain viven en la esquina del grafico
    range.setVisible (true); range.setBounds (W - 214, 56, 98, 22); range.toFront (false);
    autoGain.setVisible (true); autoGain.setBounds (W - 110, 56, 98, 22); autoGain.toFront (false);
    // panel de banda
    type.setBounds (32, 544, 170, 26); slope.setBounds (32, 578, 170, 26); place.setBounds (32, 612, 170, 26);
    soloB.setBounds (214, 544, 90, 26); del.setBounds (214, 578, 90, 26);
    freq.setBounds (430, 530, 76, 92); gain.setBounds (514, 530, 76, 92); q.setBounds (598, 530, 76, 92);
    dOff.setBounds (700, 534, 130, 26); dDyn.setBounds (700, 566, 130, 26); dSpec.setBounds (700, 598, 130, 26); below.setBounds (700, 630, 130, 26);
    thr.setBounds (850, 530, 76, 92); rangeK.setBounds (934, 530, 76, 92); att.setBounds (1018, 530, 76, 92); rel.setBounds (1102, 530, 76, 92);
}

SpqEditor::SpqEditor (SpqProcessor& p) : AudioProcessorEditor (p), content (p)
{
    setLookAndFeel (&look);
    addAndMakeVisible (content);
    content.setBounds (0, 0, W, H);
    setResizable (true, true);
    setResizeLimits (720, 408, 2400, 1360);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) W / (double) H);
    setSize (1080, 612);
}
SpqEditor::~SpqEditor() { setLookAndFeel (nullptr); }
void SpqEditor::resized() { content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) W)); }
