#include "TopNoteEditorEditor.h"

using namespace tne;

namespace
{
    juce::String U (const char* s) { return juce::String::fromUTF8 (s); }
    const juce::Colour bg (0xff101217), panel (0xff181b22), panel2 (0xff1f232c), line (0xff2b303b),
                       text (0xffe7eaf0), dim (0xff8c94a5), amber (0xffffa23e), cyan (0xff43d8ff),
                       red (0xffff4d5a), green (0xff4be3a4), violet (0xff9b7bff);
    juce::Font font (float h, bool bold = false) { return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain)); }
    juce::String noteName (int n)
    {
        static const char* nm[12] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return juce::String (nm[((n % 12) + 12) % 12]) + juce::String (n / 12 - 1);
    }
    bool isBlack (int m) { const int k = ((m % 12) + 12) % 12; return k == 1 || k == 3 || k == 6 || k == 8 || k == 10; }
    const std::array<int, 12>& maskOf (int scale)
    {
        static const std::array<std::array<int, 12>, 7> m {{
            {{ 1,1,1,1,1,1,1,1,1,1,1,1 }}, {{ 1,0,1,0,1,1,0,1,0,1,0,1 }}, {{ 1,0,1,1,0,1,0,1,1,0,1,0 }},
            {{ 1,0,1,1,0,1,0,1,1,0,0,1 }}, {{ 1,0,1,0,1,0,0,1,0,1,0,0 }}, {{ 1,0,0,1,0,1,0,1,0,0,1,0 }},
            {{ 1,0,0,1,0,1,1,1,0,0,1,0 }} }};
        return m[(size_t) juce::jlimit (0, 6, scale)];
    }
}

//==============================================================================
Look::Look()
{
    setColour (juce::ComboBox::backgroundColourId, panel2);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::outlineColourId, line);
    setColour (juce::ComboBox::arrowColourId, dim);
    setColour (juce::PopupMenu::backgroundColourId, panel2);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, amber.withAlpha (0.3f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::TooltipWindow::backgroundColourId, panel2);
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TooltipWindow::outlineColourId, line);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::buttonOnColourId, amber);
    setColour (juce::ToggleButton::textColourId, text);
    setColour (juce::BubbleComponent::backgroundColourId, panel2);
    setColour (juce::BubbleComponent::outlineColourId, line);
    setColour (juce::Slider::textBoxTextColourId, text);
}
void Look::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const auto accent = b.findColour (juce::TextButton::buttonOnColourId);
    if (b.getToggleState()) { g.setColour (accent); g.fillRoundedRectangle (r, 7); return; }
    g.setColour (down ? panel2.brighter (0.25f) : over ? panel2.brighter (0.1f) : panel2);
    g.fillRoundedRectangle (r, 7);
    g.setColour (over ? accent.withAlpha (0.7f) : line); g.drawRoundedRectangle (r, 7, 1.2f);
}
void Look::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (b.getToggleState() ? bg : (b.isEnabled() ? text : dim.withAlpha (0.5f)));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 2), juce::Justification::centred, 2, 0.8f);
}
juce::Font Look::getTextButtonFont (juce::TextButton&, int h) { return font (juce::jmin (13.0f, (float) h * 0.42f), true); }
juce::Font Look::getComboBoxFont (juce::ComboBox&) { return font (13.5f); }
void Look::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (panel2); g.fillRoundedRectangle (r, 7);
    g.setColour (line); g.drawRoundedRectangle (r, 7, 1.2f);
    juce::Path a; const float ax = (float) w - 15, ay = (float) h * 0.5f;
    a.addTriangle (ax - 4.5f, ay - 2.0f, ax + 4.5f, ay - 2.0f, ax, ay + 3.5f);
    g.setColour (dim); g.fillPath (a);
}
void Look::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool)
{
    auto r = b.getLocalBounds().toFloat();
    auto box = juce::Rectangle<float> (r.getX() + 2, r.getCentreY() - 9, 18, 18);
    g.setColour (b.getToggleState() ? amber : panel2); g.fillRoundedRectangle (box, 4);
    g.setColour (b.getToggleState() ? amber : line); g.drawRoundedRectangle (box, 4, 1.2f);
    if (b.getToggleState())
    {
        juce::Path t; t.startNewSubPath (box.getX() + 4, box.getCentreY()); t.lineTo (box.getX() + 8, box.getBottom() - 4); t.lineTo (box.getRight() - 4, box.getY() + 4);
        g.setColour (bg); g.strokePath (t, juce::PathStrokeType (2.2f));
    }
    g.setColour (text); g.setFont (font (13.5f, true));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (28), juce::Justification::centredLeft);
}
void Look::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider& s)
{
    auto r = juce::Rectangle<float> ((float) x, (float) y + (float) h * 0.5f - 3, (float) w, 6);
    g.setColour (line); g.fillRoundedRectangle (r, 3);
    g.setColour (s.findColour (juce::Slider::trackColourId)); g.fillRoundedRectangle (r.withRight (pos), 3);
    g.setColour (text); g.fillEllipse (pos - 7, r.getCentreY() - 7, 14, 14);
}

//==============================================================================
NoteView::NoteView (TopNoteEditorProcessor& p) : proc (p)
{
    setWantsKeyboardFocus (true);
    addKeyListener (this);
    for (auto* b : { &hbar, &vbar })
    {
        b->addListener (this);
        b->setAutoHide (false);
        b->setColour (juce::ScrollBar::thumbColourId, juce::Colour (0xff59606e));
        b->setColour (juce::ScrollBar::trackColourId, juce::Colour (0xff12151b));
        addAndMakeVisible (b);
    }
}

void NoteView::resized()
{
    hbar.setBounds (keyW, getHeight() - barW, getWidth() - keyW - barW, barW);
    vbar.setBounds (getWidth() - barW, rulerH, barW, getHeight() - rulerH - barW);
    syncBars();
}

double NoteView::totalSeconds() const
{
    int end = proc.sourceLength();
    for (const auto& nt : proc.notes) end = juce::jmax (end, nt.t1);
    return juce::jmax (1.0, (double) end / proc.getRate());
}

void NoteView::setViewStart (double s)
{
    viewStart = juce::jlimit (0.0, juce::jmax (0.0, totalSeconds() - visibleSeconds() * 0.5), s);
}

void NoteView::syncBars()
{
    const double vis = visibleSeconds();
    hbar.setRangeLimits (0.0, juce::jmax (totalSeconds(), viewStart + vis), juce::dontSendNotification);
    hbar.setCurrentRange (viewStart, vis, juce::dontSendNotification);
    vbar.setRangeLimits (0.0, 100.0, juce::dontSendNotification);          // 0 = MIDI 120 arriba, 100 = MIDI 20
    vbar.setCurrentRange (120.0 - topMidi, visibleSemis(), juce::dontSendNotification);
}

void NoteView::scrollBarMoved (juce::ScrollBar* b, double start)
{
    if (b == &hbar) viewStart = juce::jmax (0.0, start);
    else topMidi = juce::jlimit (20.0f, 120.0f, 120.0f - (float) start);
    repaint();
}

void NoteView::followPlayhead()
{
    const double pp = proc.playPos.load();
    if (pp < 0.0 || ! (proc.previewing.load() || proc.hostPlaying.load()) || drag != None) return;
    const double t = pp / proc.getRate(), vis = visibleSeconds();
    if (t > viewStart + vis * 0.92 || t < viewStart) { setViewStart (t - vis * 0.08); repaint(); }
}
NoteView::~NoteView() { removeKeyListener (this); }

float NoteView::xOf (double s) const { return (float) keyW + (float) ((s / proc.getRate() - viewStart) * pxPerSec); }
double NoteView::sampleOf (float x) const { return ((x - (float) keyW) / pxPerSec + viewStart) * proc.getRate(); }
float NoteView::yOf (float m) const { return (float) rulerH + (topMidi - m) * pxPerSemi; }
float NoteView::midiOf (float y) const { return topMidi - (y - (float) rulerH) / pxPerSemi; }

juce::Rectangle<float> NoteView::noteRect (const ned::Note& nt) const
{
    const float x0 = xOf (nt.t0), x1 = xOf (nt.t1), yc = yOf (nt.target());
    return { x0, yc - pxPerSemi * 0.6f, juce::jmax (2.0f, x1 - x0), pxPerSemi * 1.2f };
}

int NoteView::hit (juce::Point<float> p, int& edge) const
{
    edge = 0;
    for (int i = (int) proc.notes.size(); --i >= 0;)
    {
        const auto& nt = proc.notes[(size_t) i];
        if (! nt.visible) continue;
        const auto r = noteRect (nt).expanded (0, 2);
        if (! r.contains (p)) continue;
        const float e = juce::jmin (7.0f, r.getWidth() * 0.25f);
        if (p.x - r.getX() < e) edge = -1;
        else if (r.getRight() - p.x < e) edge = 1;
        return i;
    }
    return -1;
}

double NoteView::gridSamples() const { return proc.getRate() * 60.0 / juce::jmax (20.0, proc.bpm.load()) / 4.0; }
double NoteView::snapTime (double s) const
{
    const double g = gridSamples();
    const double host = (double) proc.getOrigin() + s;
    return std::round (host / g) * g - (double) proc.getOrigin();
}

void NoteView::validateSelection()
{
    for (auto it = sel.begin(); it != sel.end();)
    {
        if (*it >= (int) proc.notes.size() || ! proc.notes[(size_t) *it].visible) it = sel.erase (it); else ++it;
    }
}

void NoteView::selectAll()
{
    sel.clear();
    for (int i = 0; i < (int) proc.notes.size(); ++i) if (proc.notes[(size_t) i].visible) sel.insert (i);
    repaint();
    if (onChange) onChange();
}

void NoteView::fitAll()
{
    const int len = proc.sourceLength();
    if (len <= 0 || getWidth() <= keyW) return;
    pxPerSec = juce::jlimit (5.0f, 3000.0f, (float) (getWidth() - keyW - barW - 10) / (float) (len / proc.getRate()));
    viewStart = 0.0;
    float mn = 200, mx = -1;
    for (const auto& nt : proc.notes) if (nt.visible && nt.pitched) { mn = juce::jmin (mn, nt.target()); mx = juce::jmax (mx, nt.target()); }
    const float range = mx > 0 ? mx - mn : 12.0f;
    pxPerSemi = juce::jlimit (12.0f, 34.0f, (float) (getHeight() - rulerH) / (range + 14.0f));
    const float semis = (float) (getHeight() - rulerH) / pxPerSemi;
    const float mid = mx > 0 ? (mn + mx) * 0.5f : 60.0f;
    topMidi = std::round (mid + semis * 0.5f);
    repaint();
}

void NoteView::zoom (float factor, float aroundX)
{
    const double at = viewStart + (aroundX - keyW) / pxPerSec;
    pxPerSec = juce::jlimit (5.0f, 3000.0f, pxPerSec * factor);
    setViewStart (at - (aroundX - keyW) / pxPerSec);
    repaint();
}

void NoteView::liveChange()
{
    const auto now = juce::Time::getMillisecondCounter();
    if (now - lastLive > 70) { proc.notesChanged(); lastLive = now; }
    repaint();
    if (onChange) onChange();
}

void NoteView::split (int idx, double ts)
{
    auto& A = proc.notes[(size_t) idx];
    const int minLen = (int) (proc.getRate() * 0.03);
    if (ts <= A.t0 + minLen || ts >= A.t1 - minLen) return;
    proc.beginEdit();
    const auto c = proc.getCurve();
    ned::Note a = proc.notes[(size_t) idx], b = a;
    const int ss = a.s0 + (int) std::round ((ts - a.t0) * (double) (a.s1 - a.s0) / (double) (a.t1 - a.t0));
    a.s1 = ss; a.t1 = (int) ts; b.s0 = ss; b.t0 = (int) ts;
    const float tgt = proc.notes[(size_t) idx].target();
    const bool shifted = proc.notes[(size_t) idx].shift != 0.0f;
    if (c != nullptr) { ned::measure (a, *c); ned::measure (b, *c); }
    if (shifted) { a.shift = tgt - a.center; b.shift = tgt - b.center; }
    proc.notes[(size_t) idx] = a;
    proc.notes.insert (proc.notes.begin() + idx + 1, b);
    sel = { idx, idx + 1 };
    proc.notesChanged();
    repaint();
    if (onChange) onChange();
}

bool NoteView::mergeAt (juce::Point<float> p)
{
    for (int i = 0; i + 1 < (int) proc.notes.size(); ++i)
    {
        const auto& A = proc.notes[(size_t) i];
        const auto& B = proc.notes[(size_t) i + 1];
        if (! A.visible || ! B.visible || A.t1 != B.t0 || A.s1 != B.s0) continue;
        if (std::abs (xOf (A.t1) - p.x) > 6.0f) continue;
        const float y1 = yOf (A.target()), y2 = yOf (B.target());
        if (p.y < juce::jmin (y1, y2) - pxPerSemi * 2.0f || p.y > juce::jmax (y1, y2) + pxPerSemi * 1.0f) continue;
        proc.beginEdit();
        ned::Note m = A;
        m.s1 = B.s1; m.t1 = B.t1;
        const float tgt = A.target();
        if (auto c = proc.getCurve()) ned::measure (m, *c);
        m.shift = A.shift != 0.0f ? tgt - m.center : 0.0f;
        proc.notes[(size_t) i] = m;
        proc.notes.erase (proc.notes.begin() + i + 1);
        sel = { i };
        proc.notesChanged();
        repaint();
        if (onChange) onChange();
        return true;
    }
    return false;
}

//==============================================================================
void NoteView::paint (juce::Graphics& g)
{
    validateSelection();
    syncBars();
    const float W = (float) getWidth(), H = (float) getHeight();
    g.fillAll (juce::Colour (0xff0d0f14));
    const int scale = (int) proc.apvts.getRawParameterValue ("scale")->load();
    const int key = (int) proc.apvts.getRawParameterValue ("key")->load();
    const auto& mask = maskOf (scale);

    // filas de semitonos + teclado
    const int mLo = (int) std::floor (midiOf (H)) - 1, mHi = (int) std::ceil (topMidi) + 1;
    for (int m = mLo; m <= mHi; ++m)
    {
        const float y0 = yOf ((float) m + 0.5f), y1 = yOf ((float) m - 0.5f);
        juce::Colour row = isBlack (m) ? juce::Colour (0xff0f1218) : juce::Colour (0xff151922);
        if (scale != 0 && mask[(size_t) (((m - key) % 12 + 12) % 12)]) row = row.interpolatedWith (amber, 0.06f);
        g.setColour (row); g.fillRect ((float) keyW, y0, W - keyW, y1 - y0);
        g.setColour (line.withAlpha (((m % 12) + 12) % 12 == 0 ? 0.9f : 0.35f)); g.drawHorizontalLine ((int) y1, (float) keyW, W);
        g.setColour (isBlack (m) ? juce::Colour (0xff22262f) : juce::Colour (0xffd9dde5));
        g.fillRect (0.0f, y0 + 0.5f, (float) keyW - 3.0f, y1 - y0 - 1.0f);
        if (((m % 12) + 12) % 12 == 0 || pxPerSemi >= 15.0f)
        {
            g.setColour (isBlack (m) ? dim : juce::Colour (0xff20242c));
            g.setFont (font (juce::jmin (11.0f, pxPerSemi * 0.75f), ((m % 12) + 12) % 12 == 0));
            g.drawText (noteName (m), 4, (int) y0, keyW - 10, (int) (y1 - y0), juce::Justification::centredRight);
        }
    }

    // regla con compases (tempo del DAW)
    g.setColour (panel); g.fillRect ((float) keyW, 0.0f, W - keyW, (float) rulerH);
    {
        const double spb = proc.getRate() * 60.0 / juce::jmax (20.0, proc.bpm.load());
        const int sig = juce::jmax (1, proc.sigNum.load());
        const double hostA = (double) proc.getOrigin() + sampleOf ((float) keyW), hostB = (double) proc.getOrigin() + sampleOf (W);
        const float beatPx = (float) (spb / proc.getRate() * pxPerSec);
        const int step = beatPx < 6 ? sig * 4 : beatPx < 14 ? sig : 1;
        for (juce::int64 k = (juce::int64) std::floor (hostA / spb); k <= (juce::int64) std::ceil (hostB / spb); ++k)
        {
            if (k % step != 0) continue;
            const float x = xOf ((double) k * spb - (double) proc.getOrigin());
            const bool bar = k % sig == 0;
            g.setColour (line.withAlpha (bar ? 0.9f : 0.35f));
            g.drawVerticalLine ((int) x, (float) rulerH, H);
            g.setColour (bar ? text : dim);
            g.drawVerticalLine ((int) x, bar ? 4.0f : 14.0f, (float) rulerH);
            if (bar) { g.setFont (font (11.0f, true)); g.drawText (juce::String (k / sig + 1), (int) x + 4, 2, 40, 14, juce::Justification::centredLeft); }
        }
        // cursor de reproduccion previa
        const float cx = xOf (cursorSample);
        juce::Path tri; tri.addTriangle (cx - 6, (float) rulerH - 10, cx + 6, (float) rulerH - 10, cx, (float) rulerH);
        g.setColour (green); g.fillPath (tri);
    }

    // notas
    g.saveState();
    g.reduceClipRegion (keyW, rulerH, (int) W - keyW, (int) H - rulerH);
    const auto c = proc.getCurve();
    float peak = -120.0f;
    if (c != nullptr) for (float d : c->db) peak = juce::jmax (peak, d);
    const float floorDb = peak - 45.0f;
    const auto tool = this->tool;
    for (int i = 0; i < (int) proc.notes.size(); ++i)
    {
        const auto& nt = proc.notes[(size_t) i];
        if (! nt.visible) continue;
        const float x0 = xOf (nt.t0), x1 = xOf (nt.t1);
        if (x1 < keyW || x0 > W) continue;
        const bool selected = sel.count (i) > 0;
        const float yc = yOf (nt.target());
        // contorno original (si se movio)
        if (nt.shift != 0.0f || nt.t0 != nt.s0 || nt.t1 != nt.s1)
        {
            const float ox0 = xOf (nt.s0), ox1 = xOf (nt.s1), oy = yOf (nt.center);
            juce::Path ghost; ghost.addRoundedRectangle (ox0, oy - pxPerSemi * 0.3f, juce::jmax (2.0f, ox1 - ox0), pxPerSemi * 0.6f, 3.0f);
            juce::Path dash; const float pat[] { 3, 3 };
            juce::PathStrokeType (1.0f).createDashedStroke (dash, ghost, pat, 2);
            g.setColour (dim.withAlpha (0.55f)); g.fillPath (dash);
        }
        // blob: grosor segun el volumen
        juce::Path blob;
        std::vector<juce::Point<float>> lower;
        const float step = 2.0f;
        const double srcPerTgt = (double) (nt.s1 - nt.s0) / juce::jmax (1, nt.t1 - nt.t0);
        const float gainLift = nt.gainDb / 40.0f;
        bool first = true;
        for (float x = x0; ; x += step)
        {
            const float xx = juce::jmin (x, x1);
            const double t = sampleOf (xx);
            const double s = nt.s0 + (t - nt.t0) * srcPerTgt;
            float a = c != nullptr ? juce::jlimit (0.0f, 1.0f, (c->dbAt (s) - floorDb) / 45.0f + gainLift) : 0.5f;
            const float edgeFade = juce::jmin (1.0f, juce::jmin (xx - x0, x1 - xx) / 4.0f + 0.15f);
            const float hh = juce::jmax (1.5f, pxPerSemi * (0.08f + 0.5f * std::pow (a, 1.4f)) * edgeFade);
            if (first) { blob.startNewSubPath (xx, yc - hh); first = false; } else blob.lineTo (xx, yc - hh);
            lower.push_back ({ xx, yc + hh });
            if (xx >= x1) break;
        }
        for (auto it = lower.rbegin(); it != lower.rend(); ++it) blob.lineTo (*it);
        blob.closeSubPath();
        const juce::Colour base = ! nt.pitched ? juce::Colour (0xff8d939f) : selected ? cyan : amber;
        if (nt.mute)
        {
            juce::Path dash; const float pat[] { 4, 3 };
            juce::PathStrokeType (1.4f).createDashedStroke (dash, blob, pat, 2);
            g.setColour (base.withAlpha (0.8f)); g.fillPath (dash);
        }
        else
        {
            g.setGradientFill (juce::ColourGradient (base.brighter (0.25f), 0, yc - pxPerSemi * 0.5f, base.darker (0.35f), 0, yc + pxPerSemi * 0.5f, false));
            g.fillPath (blob);
            g.setColour (base.darker (0.6f)); g.strokePath (blob, juce::PathStrokeType (1.0f));
        }
        // curva de tono editada
        if (c != nullptr && nt.pitched)
        {
            juce::Path pc; bool pen = false;
            for (float x = juce::jmax (x0, (float) keyW); x <= juce::jmin (x1, W); x += 2.0f)
            {
                const double t = sampleOf (x);
                const double s = nt.s0 + (t - nt.t0) * srcPerTgt;
                const float p = c->midiAt (s);
                if (p <= 0) { pen = false; continue; }
                const float sm = c->smoothAt (s) > 0 ? c->smoothAt (s) : p;
                const float tg = nt.center + nt.shift + nt.drift * (sm - nt.center) + nt.mod * (p - sm);
                if (! pen) { pc.startNewSubPath (x, yOf (tg)); pen = true; } else pc.lineTo (x, yOf (tg));
            }
            g.setColour (juce::Colours::white.withAlpha (selected ? 0.95f : 0.65f));
            g.strokePath (pc, juce::PathStrokeType (1.3f));
        }
        // datos de la herramienta activa
        if ((selected || i == hover) && x1 - x0 > 24)
        {
            juce::String info;
            if (tool == Modulation) info = "Vib " + juce::String (juce::roundToInt (nt.mod * 100)) + "%";
            else if (tool == Drift) info = "Der " + juce::String (juce::roundToInt (nt.drift * 100)) + "%";
            else if (tool == Formant) info = "F " + juce::String (nt.formant >= 0 ? "+" : "") + juce::String (nt.formant, 1);
            else if (tool == Amplitude) info = nt.mute ? "MUTE" : juce::String (nt.gainDb >= 0 ? "+" : "") + juce::String (nt.gainDb, 1) + " dB";
            if (info.isNotEmpty())
            {
                g.setColour (text); g.setFont (font (11.0f, true));
                g.drawText (info, (int) x0, (int) (yc - pxPerSemi * 1.8f), (int) (x1 - x0), 14, juce::Justification::centred);
            }
            if (tool == Formant && nt.formant != 0.0f)
            {
                g.setColour (violet);
                const float fy = yc - nt.formant * pxPerSemi * 0.5f;
                g.drawLine (x0 + 3, fy, x1 - 3, fy, 2.0f);
            }
        }
    }
    // linea de reproduccion
    const double pp = proc.playPos.load();
    if (pp >= 0.0 && (proc.hostPlaying.load() || proc.previewing.load() || proc.isTransferring()))
    {
        const float x = xOf (pp);
        g.setColour (proc.isTransferring() ? red : green); g.drawLine (x, 0, x, H, 1.6f);
    }
    if (drag == Band && ! band.isEmpty())
    {
        g.setColour (cyan.withAlpha (0.12f)); g.fillRect (band);
        g.setColour (cyan.withAlpha (0.7f)); g.drawRect (band, 1.0f);
    }
    g.restoreState();

    // mensajes de estado
    const int st = proc.status.load();
    auto banner = [&] (const juce::String& t, juce::Colour col)
    {
        auto r = juce::Rectangle<float> (W * 0.5f - 330, H * 0.5f - 70, 660, 140);
        g.setColour (panel.withAlpha (0.95f)); g.fillRoundedRectangle (r, 14);
        g.setColour (col); g.drawRoundedRectangle (r, 14, 1.5f);
        g.setColour (text); g.setFont (font (15.0f, true));
        g.drawFittedText (t, r.reduced (24).toNearestInt(), juce::Justification::centred, 6);
    };
    if (st == TopNoteEditorProcessor::Empty)
        banner (U ("1) Pulsa TRANSFERIR y dale PLAY en tu DAW a la pista de voz (desde antes de que empiece a cantar).\n"
                   "2) Cuando termine, pulsa TRANSFERIR otra vez.\n\n"
                   "O arrastra aquí un archivo de audio (WAV, AIFF, FLAC, MP3) / pulsa IMPORTAR."), amber);
    else if (st == TopNoteEditorProcessor::Recording)
        banner (U ("TRANSFIRIENDO…  ") + juce::String (proc.recordedSeconds(), 1) + U (" s\n\nDale PLAY en tu DAW. Pulsa TRANSFERIR otra vez para terminar."), red);
    else if (st == TopNoteEditorProcessor::Analysing)
        banner (U ("Analizando notas…  ") + juce::String (juce::roundToInt (proc.progress.load() * 100)) + " %", amber);
}

//==============================================================================
void NoteView::mouseMove (const juce::MouseEvent& e)
{
    int edge = 0;
    const int h = hit (e.position, edge);
    if (h != hover || edge != hoverEdge) { hover = h; hoverEdge = edge; repaint(); if (onChange) onChange(); }
    if (tool == Separate) setMouseCursor (h >= 0 ? juce::MouseCursor::IBeamCursor : juce::MouseCursor::NormalCursor);
    else if (h >= 0 && tool == Main) setMouseCursor (edge != 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::DraggingHandCursor);
    else if (h >= 0) setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    else setMouseCursor (juce::MouseCursor::NormalCursor);
}

void NoteView::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    const auto p = e.position;
    drag = None; editBegun = false; axis = 0;
    if (e.mods.isMiddleButtonDown() || (e.mods.isAltDown() && p.x > keyW))
    {
        // mano: arrastrar para moverse por la toma (izquierda/derecha y arriba/abajo)
        drag = Pan; panStartView = viewStart; panStartTop = topMidi;
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        return;
    }
    if (p.y < rulerH && p.x > keyW)
    {
        cursorSample = juce::jmax (0.0, sampleOf (p.x));
        drag = Scrub;
        if (proc.previewing.load()) proc.startPreview (cursorSample);
        repaint();
        return;
    }
    if (p.x < keyW) return;
    int edge = 0;
    const int h = hit (p, edge);
    primary = h;
    dragStart = proc.notes;
    if (tool == Separate)
    {
        if (! mergeAt (p) && h >= 0) split (h, sampleOf (p.x));
        return;
    }
    if (h >= 0)
    {
        if (e.mods.isShiftDown()) sel.insert (h);
        else if (e.mods.isCommandDown()) { if (sel.count (h)) sel.erase (h); else sel.insert (h); }
        else if (! sel.count (h)) { sel.clear(); sel.insert (h); }
        if (tool == Main) drag = edge < 0 ? ResizeL : edge > 0 ? ResizeR : Move;
        else drag = Param;
    }
    else
    {
        if (! e.mods.isShiftDown()) sel.clear();
        drag = Band;
        band = { p, p };
    }
    repaint();
    if (onChange) onChange();
}

void NoteView::mouseDrag (const juce::MouseEvent& e)
{
    const float dx = (float) e.getDistanceFromDragStartX(), dy = (float) e.getDistanceFromDragStartY();
    if (drag == Scrub) { cursorSample = juce::jmax (0.0, sampleOf (e.position.x)); repaint(); return; }
    if (drag == Pan)
    {
        setViewStart (panStartView - dx / pxPerSec);
        topMidi = juce::jlimit (20.0f, 120.0f, panStartTop + dy / pxPerSemi);
        repaint();
        return;
    }
    if (drag == Band)
    {
        band = juce::Rectangle<float> (e.mouseDownPosition, e.position);
        if (! e.mods.isShiftDown()) sel.clear();
        for (int i = 0; i < (int) proc.notes.size(); ++i)
            if (proc.notes[(size_t) i].visible && noteRect (proc.notes[(size_t) i]).intersects (band)) sel.insert (i);
        repaint();
        if (onChange) onChange();
        return;
    }
    if (drag == None || primary < 0 || primary >= (int) proc.notes.size() || dragStart.size() != proc.notes.size()) return;
    if (! editBegun)
    {
        if (std::abs (dx) < 3 && std::abs (dy) < 3) return;
        proc.beginEdit();
        editBegun = true;
    }
    auto& N = proc.notes;
    const auto& S = dragStart;
    const bool fine = e.mods.isAltDown();
    const double dS = dx / pxPerSec * proc.getRate();
    const int minLen = (int) (proc.getRate() * 0.02);
    switch (drag)
    {
        case Move:
        {
            if (axis == 0) axis = std::abs (dy) > std::abs (dx) ? 1 : 2;
            if (axis == 1)
            {
                const float dSemi = -dy / pxPerSemi;
                const auto& P = S[(size_t) primary];
                const float tgt0 = P.center + P.shift;
                const float delta = fine ? std::round (dSemi * 100.0f) / 100.0f : (float) proc.snapPitch (tgt0 + dSemi) - tgt0;
                for (int i : sel) N[(size_t) i].shift = juce::jlimit (-36.0f, 36.0f, S[(size_t) i].shift + delta);
            }
            else
            {
                const auto& P = S[(size_t) primary];
                double nt0 = fine ? P.t0 + dS : snapTime (P.t0 + dS);
                double delta = nt0 - P.t0;
                for (int i : sel) delta = juce::jmax (delta, (double) -S[(size_t) i].t0);
                for (int i : sel) { N[(size_t) i].t0 = S[(size_t) i].t0 + (int) delta; N[(size_t) i].t1 = S[(size_t) i].t1 + (int) delta; }
            }
            break;
        }
        case ResizeL:
        case ResizeR:
        {
            const auto& P = S[(size_t) primary];
            auto& Q = N[(size_t) primary];
            const int srcLen = P.s1 - P.s0;
            if (drag == ResizeL)
            {
                double v = fine ? P.t0 + dS : snapTime (P.t0 + dS);
                v = juce::jlimit ((double) juce::jmax (0, P.t1 - srcLen * 4), (double) (P.t1 - juce::jmax (minLen, srcLen / 4)), v);
                Q.t0 = (int) v;
            }
            else
            {
                double v = fine ? P.t1 + dS : snapTime (P.t1 + dS);
                v = juce::jlimit ((double) (P.t0 + juce::jmax (minLen, srcLen / 4)), (double) (P.t0 + srcLen * 4), v);
                Q.t1 = (int) v;
            }
            break;
        }
        case Param:
            for (int i : sel)
            {
                auto& n = N[(size_t) i];
                const auto& s = S[(size_t) i];
                if (tool == Modulation) n.mod = juce::jlimit (0.0f, 2.0f, std::round ((s.mod - dy / 120.0f) * 100.0f) / 100.0f);
                else if (tool == Drift) n.drift = juce::jlimit (0.0f, 2.0f, std::round ((s.drift - dy / 120.0f) * 100.0f) / 100.0f);
                else if (tool == Formant) n.formant = juce::jlimit (-5.0f, 5.0f, std::round ((s.formant - dy / 30.0f) * 10.0f) / 10.0f);
                else if (tool == Amplitude) n.gainDb = juce::jlimit (-36.0f, 12.0f, std::round ((s.gainDb - dy / 5.0f) * 10.0f) / 10.0f);
            }
            break;
        default: break;
    }
    liveChange();
}

void NoteView::mouseUp (const juce::MouseEvent&)
{
    if (drag == Pan) setMouseCursor (juce::MouseCursor::NormalCursor);
    if (editBegun) proc.notesChanged();
    editBegun = false;
    drag = None;
    band = {};
    repaint();
    if (onChange) onChange();
}

void NoteView::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto p = e.position;
    if (p.y < rulerH || p.x < keyW || tool == Separate) return;
    if (tool == Main)
    {
        // doble clic justo encima de una nota: dividirla (o unir si es en la union de dos notas)
        for (int i = 0; i < (int) proc.notes.size(); ++i)
        {
            const auto& nt = proc.notes[(size_t) i];
            if (! nt.visible) continue;
            const auto r = noteRect (nt);
            const auto zone = juce::Rectangle<float> (r.getX() - 4, r.getY() - pxPerSemi * 1.1f, r.getWidth() + 8, pxPerSemi * 1.1f);
            if (zone.contains (p))
            {
                if (! mergeAt (p)) split (i, sampleOf (p.x));
                return;
            }
        }
    }
    int edge = 0;
    const int h = hit (p, edge);
    if (h < 0) return;
    proc.beginEdit();
    std::set<int> t = sel; t.insert (h);
    for (int i : t)
    {
        auto& nt = proc.notes[(size_t) i];
        if (tool == Main && nt.pitched) nt.shift = (float) proc.snapPitch (nt.center + nt.shift) - nt.center;
        else if (tool == Amplitude) nt.mute = ! nt.mute;
        else if (tool == Modulation) nt.mod = 1.0f;
        else if (tool == Drift) nt.drift = 1.0f;
        else if (tool == Formant) nt.formant = 0.0f;
    }
    proc.notesChanged();
    repaint();
    if (onChange) onChange();
}

void NoteView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (e.mods.isCommandDown()) { zoom (1.0f + w.deltaY * 0.8f, e.position.x); return; }
    if (e.mods.isShiftDown() || std::abs (w.deltaX) > std::abs (w.deltaY) || e.position.y < rulerH || e.position.y > getHeight() - barW)
    {
        // rueda sobre la regla de tiempo / barra de abajo, Shift + rueda, o trackpad lateral: izquierda / derecha
        const float d = std::abs (w.deltaX) > std::abs (w.deltaY) ? w.deltaX : w.deltaY;
        setViewStart (viewStart - d * visibleSeconds() * 0.35);
    }
    else topMidi = juce::jlimit (20.0f, 120.0f, topMidi + w.deltaY * 8.0f);
    repaint();
}

bool NoteView::keyPressed (const juce::KeyPress& k, juce::Component*)
{
    const bool cmd = k.getModifiers().isCommandDown();
    if (cmd && k.getKeyCode() == 'Z') { if (k.getModifiers().isShiftDown()) proc.redo(); else proc.undo(); repaint(); if (onChange) onChange(); return true; }
    if (cmd && k.getKeyCode() == 'Y') { proc.redo(); repaint(); if (onChange) onChange(); return true; }
    if (cmd && k.getKeyCode() == 'A') { selectAll(); return true; }
    if (k.getKeyCode() == juce::KeyPress::spaceKey)
    {
        if (proc.previewing.load()) proc.stopPreview(); else proc.startPreview (cursorSample);
        return true;
    }
    if (k.getKeyCode() == juce::KeyPress::leftKey || k.getKeyCode() == juce::KeyPress::rightKey)
    {
        const double d = (k.getKeyCode() == juce::KeyPress::leftKey ? -1.0 : 1.0) * visibleSeconds() * (k.getModifiers().isShiftDown() ? 0.9 : 0.25);
        setViewStart (viewStart + d); repaint(); return true;
    }
    if (k.getKeyCode() == juce::KeyPress::homeKey) { setViewStart (0.0); repaint(); return true; }
    if (k.getKeyCode() == juce::KeyPress::endKey) { setViewStart (totalSeconds() - visibleSeconds() * 0.9); repaint(); return true; }
    if (sel.empty()) return false;
    if (k.getKeyCode() == juce::KeyPress::deleteKey || k.getKeyCode() == juce::KeyPress::backspaceKey)
    {
        proc.beginEdit(); for (int i : sel) proc.notes[(size_t) i].mute = ! proc.notes[(size_t) i].mute;
        proc.notesChanged(); repaint(); if (onChange) onChange(); return true;
    }
    if (k.getKeyCode() == juce::KeyPress::upKey || k.getKeyCode() == juce::KeyPress::downKey)
    {
        const float d = (k.getKeyCode() == juce::KeyPress::upKey ? 1.0f : -1.0f) * (k.getModifiers().isAltDown() ? 0.1f : 1.0f);
        proc.beginEdit(); for (int i : sel) proc.notes[(size_t) i].shift += d;
        proc.notesChanged(); repaint(); if (onChange) onChange(); return true;
    }
    if (k.getKeyCode() == juce::KeyPress::leftKey || k.getKeyCode() == juce::KeyPress::rightKey)
    {
        const int d = (int) (gridSamples() * (k.getKeyCode() == juce::KeyPress::rightKey ? 1 : -1));
        proc.beginEdit();
        for (int i : sel) { auto& n = proc.notes[(size_t) i]; if (n.t0 + d >= 0) { n.t0 += d; n.t1 += d; } }
        proc.notesChanged(); repaint(); if (onChange) onChange(); return true;
    }
    return false;
}

//==============================================================================
MacroPanel::MacroPanel (TopNoteEditorProcessor& p, NoteView& v, int t) : proc (p), view (v), type (t)
{
    for (auto* s : { &a, &b })
    {
        s->setSliderStyle (juce::Slider::LinearHorizontal);
        s->setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 22);
        s->setRange (0, 100, 1);
        s->setTextValueSuffix (" %");
        s->setColour (juce::Slider::trackColourId, amber);
        s->setColour (juce::Slider::textBoxOutlineColourId, line);
        addAndMakeVisible (s);
    }
    if (type == 0) { la = U ("Afinación del centro"); lb = U ("Deriva (fluctuación dentro de la nota)"); a.setValue (100); b.setValue (0); }
    else if (type == 1)
    {
        la = "Cantidad"; a.setValue (100); b.setVisible (false);
        grid.addItemList ({ "1/4", "1/8", "1/16", "1/32" }, 1); grid.setSelectedId (3);
        addAndMakeVisible (grid);
    }
    else { la = U ("Bajar las más fuertes"); lb = U ("Subir las más suaves"); a.setValue (50); b.setValue (50); }
    apply.setColour (juce::TextButton::buttonOnColourId, amber);
    apply.onClick = [this]
    {
        const auto s = view.selection();
        if (type == 0) proc.macroCorrectPitch (s, (float) a.getValue() / 100.0f, (float) b.getValue() / 100.0f);
        else if (type == 1) { const double g[] { 1.0, 0.5, 0.25, 0.125 }; proc.macroQuantizeTime (s, g[juce::jlimit (0, 3, grid.getSelectedId() - 1)], (float) a.getValue() / 100.0f); }
        else proc.macroLevel (s, (float) a.getValue() / 100.0f, (float) b.getValue() / 100.0f);
        view.repaint();
        if (view.onChange) view.onChange();
        if (auto* box = findParentComponentOfClass<juce::CallOutBox>()) box->dismiss();
    };
    addAndMakeVisible (apply);
    setSize (380, 190);
}
void MacroPanel::resized()
{
    a.setBounds (16, 56, getWidth() - 32, 26);
    if (type == 1) grid.setBounds (16, 108, 120, 28); else b.setBounds (16, 108, getWidth() - 32, 26);
    apply.setBounds (getWidth() - 136, getHeight() - 44, 120, 32);
}
void MacroPanel::paint (juce::Graphics& g)
{
    g.fillAll (panel);
    g.setColour (text); g.setFont (font (14.0f, true));
    const juce::String title = type == 0 ? U ("CORREGIR AFINACIÓN") : type == 1 ? "CUANTIZAR TIEMPO" : "NIVELAR NOTAS";
    g.drawText (title, 16, 8, getWidth() - 32, 20, juce::Justification::centredLeft);
    g.setColour (dim); g.setFont (font (12.5f));
    g.drawText (la, 16, 36, getWidth() - 32, 18, juce::Justification::centredLeft);
    g.drawText (type == 1 ? U ("Cuadrícula") : lb, 16, 88, getWidth() - 32, 18, juce::Justification::centredLeft);
    g.drawText (view.selection().empty() ? U ("Se aplica a todas las notas") : juce::String ((int) view.selection().size()) + U (" notas seleccionadas"),
                16, getHeight() - 40, 200, 24, juce::Justification::centredLeft);
}

//==============================================================================
void DragTile::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2);
    const bool ready = proc.hasSource() && proc.status.load() == TopNoteEditorProcessor::Ready;
    if (ready)
    {
        g.setColour (amber.withAlpha (hover ? 0.35f : 0.18f)); g.fillRoundedRectangle (r.expanded (2), 11);
        g.setGradientFill (juce::ColourGradient (amber.brighter (hover ? 0.3f : 0.1f), r.getX(), r.getY(), amber.darker (0.35f), r.getX(), r.getBottom(), false));
    }
    else g.setColour (panel2);
    g.fillRoundedRectangle (r, 10);
    g.setColour (ready ? juce::Colours::white.withAlpha (0.35f) : line); g.drawRoundedRectangle (r, 10, 1.2f);
    // icono: onda + flecha hacia fuera
    const auto ic = r.removeFromLeft (r.getHeight()).reduced (9);
    const auto ink = ready ? juce::Colour (0xff1a1206) : dim;
    g.setColour (ink);
    const float hs[] { 0.35f, 0.75f, 1.0f, 0.6f, 0.85f, 0.4f };
    for (int i = 0; i < 6; ++i)
    {
        const float h = ic.getHeight() * 0.7f * hs[i];
        g.fillRoundedRectangle (ic.getX() + (float) i * ic.getWidth() * 0.13f, ic.getCentreY() - h * 0.5f, ic.getWidth() * 0.08f, h, 1.0f);
    }
    juce::Path ar;
    const float ax = ic.getRight() - 2, ay = ic.getY() + 4;
    ar.startNewSubPath (ax - 11, ay + 11); ar.lineTo (ax, ay);
    ar.startNewSubPath (ax - 7, ay); ar.lineTo (ax, ay); ar.lineTo (ax, ay + 7);
    g.strokePath (ar, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    // texto
    juce::String l1, l2;
    if (busy) { l1 = U ("PREPARANDO LA VOZ…"); l2 = U ("un momento"); }
    else if (! ready) { l1 = U ("VOZ AFINADA"); l2 = proc.hasSource() ? U ("procesando…") : U ("primero transfiere o importa una voz"); }
    else
    {
        l1 = U ("ARRASTRA LA VOZ AFINADA");
        l2 = proc.alignToSong && proc.getOrigin() > 0 ? U ("al DAW · suéltala en el compás 1") : U ("al DAW · suéltala donde empieza la voz");
    }
    g.setColour (ink); g.setFont (font (13.5f, true));
    g.drawFittedText (l1, r.withTrimmedBottom (r.getHeight() * 0.45f).toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
    g.setColour (ink.withAlpha (0.8f)); g.setFont (font (11.0f, true));
    g.drawFittedText (l2, r.withTrimmedTop (r.getHeight() * 0.52f).toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
}
void DragTile::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 6) return;
    if (! proc.hasSource() || proc.status.load() != TopNoteEditorProcessor::Ready) return;
    dragging = true;
    busy = true; repaint();
    file = proc.dragFile();
    busy = false; repaint();
    if (file.existsAsFile())
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this);
}
void DragTile::mouseUp (const juce::MouseEvent& e)
{
    // clic sin arrastrar: crea el archivo y lo muestra en su carpeta
    if (! dragging && e.getDistanceFromDragStart() < 6 && proc.hasSource() && proc.status.load() == TopNoteEditorProcessor::Ready)
    {
        file = proc.dragFile();
        if (file.existsAsFile()) file.revealToUser();
    }
    dragging = false;
}

//==============================================================================
TopNoteEditorEditor::TopNoteEditorEditor (TopNoteEditorProcessor& p) : AudioProcessorEditor (&p), proc (p), view (p), tile (p)
{
    setLookAndFeel (&look);
    tips.setLookAndFeel (&look);
    addAndMakeVisible (view);
    view.onChange = [this] { repaint (0, H - 62, W, 62); };

    transferBtn.setColour (juce::TextButton::buttonOnColourId, red);
    transferBtn.setTooltip (U ("Graba la pista mientras tu DAW reproduce. Pulsa otra vez para terminar."));
    transferBtn.onClick = [this] { proc.setTransfer (! proc.isTransferring()); };
    importBtn.setTooltip (U ("Abrir un archivo de audio (WAV, AIFF, FLAC, MP3, OGG)"));
    importBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Importar audio", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc) { if (fc.getResult().existsAsFile()) proc.importFile (fc.getResult()); });
    };
    algoBox.addItem (U ("Melódico (voz)"), 1);
    algoBox.addItem ("Percusivo", 2);
    algoBox.setSelectedId (proc.getAlgorithm() + 1, juce::dontSendNotification);
    algoBox.setTooltip (U ("Cambiar el algoritmo vuelve a analizar y borra las ediciones"));
    algoBox.onChange = [this] { if (proc.hasSource() && algoBox.getSelectedId() - 1 != proc.getAlgorithm()) proc.reanalyse (algoBox.getSelectedId() - 1); };
    for (auto* c : { (juce::Component*) &transferBtn, (juce::Component*) &importBtn, (juce::Component*) &algoBox }) addAndMakeVisible (c);

    const juce::StringArray tn { "PRINCIPAL", U ("MODULACIÓN"), "DERIVA", "FORMANTES", "AMPLITUD", "SEPARAR" };
    const juce::StringArray tt {
        U ("Arrastra arriba/abajo = tono (Alt: centésimas). Izquierda/derecha = tiempo. Bordes = duración. Doble clic = afinar. Doble clic justo encima = dividir."),
        U ("Arrastra arriba/abajo para subir o bajar el vibrato (0-200 %). Doble clic = 100 %"),
        U ("Arrastra arriba/abajo la deriva de tono dentro de la nota. Doble clic = 100 %"),
        U ("Arrastra arriba/abajo para cambiar el timbre (formantes) sin cambiar la nota"),
        U ("Arrastra arriba/abajo el volumen de la nota. Doble clic = silenciar"),
        U ("Clic dentro de una nota = dividir. Clic en la unión de dos notas = unir") };
    for (int i = 0; i < tn.size(); ++i)
    {
        auto* b = tools.add (new juce::TextButton (tn[i]));
        b->setTooltip (tt[i]);
        b->setColour (juce::TextButton::buttonOnColourId, i == 0 ? amber : i == 3 ? violet : cyan);
        b->onClick = [this, i] { setTool (i); };
        addAndMakeVisible (b);
    }
    setTool (0);

    keyBox.addItemList ({ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 1);
    scaleBox.addItemList (TopNoteEditorProcessor::scaleNames(), 1);
    keyAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "key", keyBox);
    scaleAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "scale", scaleBox);
    snapAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "snap", snapBtn);
    keyBox.onChange = scaleBox.onChange = [this] { view.repaint(); };
    snapBtn.setTooltip (U ("Al mover o corregir, las notas caen en la escala elegida"));
    for (auto* c : { (juce::Component*) &keyBox, (juce::Component*) &scaleBox, (juce::Component*) &snapBtn }) addAndMakeVisible (c);

    macroPitch.setTooltip (U ("Lleva las notas (seleccionadas o todas) a su nota correcta"));
    macroTime.setTooltip (U ("Acerca el inicio de las notas a la cuadrícula del tempo del DAW"));
    macroLevel.setTooltip (U ("Iguala el volumen entre notas (prepara para el compresor)"));
    macroPitch.onClick = [this] { openMacro (0, macroPitch); };
    macroTime.onClick = [this] { openMacro (1, macroTime); };
    macroLevel.onClick = [this] { openMacro (2, macroLevel); };
    undoBtn.onClick = [this] { proc.undo(); view.repaint(); };
    redoBtn.onClick = [this] { proc.redo(); view.repaint(); };
    fitBtn.onClick = [this] { view.fitAll(); };
    playBtn.setColour (juce::TextButton::buttonOnColourId, green);
    playBtn.setTooltip (U ("Escucha dentro del plugin desde el cursor verde (barra espaciadora)"));
    playBtn.onClick = [this] { if (proc.previewing.load()) proc.stopPreview(); else proc.startPreview (view.cursorSample); };
    exportBtn.setTooltip (U ("Guarda la toma editada como WAV en Documentos/Top Note Editor"));
    exportBtn.onClick = [this] { tile.file = proc.exportWav(); tile.repaint(); };
    for (auto* c : { (juce::Component*) &macroPitch, (juce::Component*) &macroTime, (juce::Component*) &macroLevel,
                     (juce::Component*) &undoBtn, (juce::Component*) &redoBtn, (juce::Component*) &fitBtn, (juce::Component*) &playBtn,
                     (juce::Component*) &exportBtn, (juce::Component*) &tile })
        addAndMakeVisible (c);
    tile.file = proc.lastExport;
    tile.setTooltip (U ("Agarra aquí y suelta en la lista de reproducción de FL Studio.\nLuego silencia o borra la pista de la voz original."));
    alignBtn.setToggleState (proc.alignToSong, juce::dontSendNotification);
    alignBtn.setTooltip (U ("Activado: el WAV empieza en el compás 1 de tu canción, así queda alineado con la voz original\nsi lo sueltas al inicio de la lista de reproducción."));
    alignBtn.onClick = [this] { proc.alignToSong = alignBtn.getToggleState(); tile.repaint(); };
    addAndMakeVisible (alignBtn);

    zoomSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    zoomSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    zoomSlider.setRange (0.0, 1.0);
    zoomSlider.setValue (0.4, juce::dontSendNotification);
    zoomSlider.setColour (juce::Slider::trackColourId, amber);
    zoomSlider.setTooltip ("Zoom horizontal (Ctrl + rueda sobre las notas)");
    zoomSlider.onValueChange = [this]
    {
        view.zoom ((float) std::pow (40.0, zoomSlider.getValue() - lastZoom), (float) tne::NoteView::keyW + 300.0f);
        lastZoom = zoomSlider.getValue();
    };
    addAndMakeVisible (zoomSlider);

    setResizable (true, true);
    setResizeLimits (W * 2 / 3, H * 2 / 3, W * 3 / 2, H * 3 / 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) W / H);
    setSize (W, H);
    startTimerHz (30);
}

TopNoteEditorEditor::~TopNoteEditorEditor()
{
    stopTimer();
    tips.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void TopNoteEditorEditor::setTool (int t)
{
    view.tool = (tne::Tool) t;
    for (int i = 0; i < tools.size(); ++i) tools[i]->setToggleState (i == t, juce::dontSendNotification);
    view.repaint();
    repaint (0, H - 62, W, 62);
    tile.repaint();
}

void TopNoteEditorEditor::openMacro (int type, juce::Component& anchor)
{
    if (proc.notes.empty()) return;
    juce::CallOutBox::launchAsynchronously (std::make_unique<tne::MacroPanel> (proc, view, type), anchor.getScreenBounds(), nullptr);
}

void TopNoteEditorEditor::filesDropped (const juce::StringArray& files, int, int)
{
    if (! files.isEmpty()) proc.importFile (juce::File (files[0]));
}

void TopNoteEditorEditor::resized()
{
    // la interfaz se dibuja a W x H y se escala
    const float s = (float) getWidth() / (float) W;
    setTransform ({});
    for (auto* c : getChildren()) c->setTransform (juce::AffineTransform::scale (s));
    transferBtn.setBounds (262, 12, 126, 32);
    importBtn.setBounds (394, 12, 104, 32);
    algoBox.setBounds (504, 12, 150, 32);
    for (int i = 0; i < tools.size(); ++i) tools[i]->setBounds (668 + i * 100, 12, 96, 32);
    keyBox.setBounds (16, 56, 64, 30);
    scaleBox.setBounds (86, 56, 170, 30);
    snapBtn.setBounds (264, 56, 160, 30);
    macroPitch.setBounds (430, 56, 172, 30);
    macroTime.setBounds (608, 56, 146, 30);
    macroLevel.setBounds (760, 56, 130, 30);
    undoBtn.setBounds (904, 56, 94, 30);
    redoBtn.setBounds (1002, 56, 86, 30);
    fitBtn.setBounds (1094, 56, 84, 30);
    playBtn.setBounds (1184, 56, 80, 30);
    view.setBounds (0, 96, W, H - 96 - 62);
    zoomSlider.setBounds (596, H - 28, 150, 24);
    alignBtn.setBounds (596, H - 56, 180, 24);
    exportBtn.setBounds (780, H - 50, 116, 38);
    tile.setBounds (904, H - 58, 364, 54);
}

void TopNoteEditorEditor::paint (juce::Graphics& g)
{
    const float s = (float) getWidth() / (float) W;
    g.addTransform (juce::AffineTransform::scale (s));
    g.fillAll (bg);
    g.setGradientFill (juce::ColourGradient (panel, 0, 0, bg, 0, 96, false));
    g.fillRect (0, 0, W, 96);
    g.setColour (line); g.drawHorizontalLine (95, 0, (float) W); g.drawHorizontalLine (H - 62, 0, (float) W);
    // logo: un blob con su curva
    {
        juce::Path blob; blob.addRoundedRectangle (18.0f, 20.0f, 34.0f, 14.0f, 7.0f);
        g.setColour (amber); g.fillPath (blob);
        juce::Path wv; wv.startNewSubPath (20, 27);
        for (int i = 0; i <= 30; ++i) wv.lineTo (20.0f + i, 27.0f + std::sin (i * 0.6f) * 2.5f);
        g.setColour (juce::Colours::white.withAlpha (0.9f)); g.strokePath (wv, juce::PathStrokeType (1.4f));
    }
    g.setColour (text); g.setFont (font (20.0f, true));
    g.drawText ("TOP NOTE EDITOR", 60, 8, 200, 24, juce::Justification::centredLeft);
    g.setColour (dim); g.setFont (font (11.5f));
    g.drawText ("by Dj Pava Hm Music", 61, 30, 200, 14, juce::Justification::centredLeft);

    // pie: informacion de la nota
    juce::String info;
    const auto selIdx = view.selection();
    const int idx = view.hoverNote() >= 0 ? view.hoverNote() : (selIdx.size() == 1 ? selIdx[0] : -1);
    if (idx >= 0 && idx < (int) proc.notes.size())
    {
        const auto& nt = proc.notes[(size_t) idx];
        const float tg = nt.target();
        const int nn = (int) std::round (tg);
        const int cents = juce::roundToInt ((tg - nn) * 100);
        const int oc = juce::roundToInt ((nt.center - std::round (nt.center)) * 100);
        info << (nt.pitched ? noteName (nn) + "  " + (cents >= 0 ? "+" : "") + juce::String (cents) + " ct" : U ("Sin tono"))
             << U ("   ·   original ") << (nt.pitched ? noteName ((int) std::round (nt.center)) + " " + (oc >= 0 ? "+" : "") + juce::String (oc) + " ct" : "-")
             << U ("   ·   ") << juce::String ((nt.t1 - nt.t0) / proc.getRate(), 2) << " s"
             << U ("   ·   vib ") << juce::roundToInt (nt.mod * 100) << U ("%   der ") << juce::roundToInt (nt.drift * 100)
             << U ("%   form ") << juce::String (nt.formant, 1) << U ("   vol ") << juce::String (nt.gainDb, 1) << " dB" << (nt.mute ? "  MUTE" : "");
    }
    else if (! selIdx.empty()) info << (int) selIdx.size() << U (" notas seleccionadas");
    else if (proc.status.load() == TopNoteEditorProcessor::Ready)
        info << (int) std::count_if (proc.notes.begin(), proc.notes.end(), [] (const ned::Note& n) { return n.visible; })
             << U (" notas  ·  ") << proc.getSourceName() << U ("  ·  muévete con la barra de abajo, ← →, Shift+rueda o Alt+arrastrar  ·  Ctrl+rueda = zoom");
    g.setColour (text); g.setFont (font (13.0f, true));
    g.drawFittedText (info, 16, H - 54, 570, 20, juce::Justification::centredLeft, 1, 0.8f);
    g.setColour (dim); g.setFont (font (12.0f));
    const juce::String st = proc.rendering.load() ? U ("Procesando cambios… ") + juce::String (juce::roundToInt (proc.renderProgress.load() * 100)) + " %"
                          : proc.hostPlaying.load() ? U ("Sonando desde el DAW con tus ediciones") : U ("Listo  ·  BPM ") + juce::String (proc.bpm.load(), 1);
    g.drawText (st, 16, H - 32, 570, 18, juce::Justification::centredLeft);
}

void TopNoteEditorEditor::timerCallback()
{
    const bool tr = proc.isTransferring();
    transferBtn.setToggleState (tr, juce::dontSendNotification);
    transferBtn.setButtonText (tr ? U ("PARAR (") + juce::String (proc.recordedSeconds(), 0) + " s)" : "TRANSFERIR");
    undoBtn.setEnabled (proc.canUndo());
    redoBtn.setEnabled (proc.canRedo());
    playBtn.setToggleState (proc.previewing.load(), juce::dontSendNotification);
    playBtn.setButtonText (proc.previewing.load() ? "STOP" : "PLAY");
    algoBox.setSelectedId (proc.getAlgorithm() + 1, juce::dontSendNotification);
    const int st = proc.status.load();
    if (st == TopNoteEditorProcessor::Ready && lastStatus != TopNoteEditorProcessor::Ready) view.fitAll();
    lastStatus = st;
    view.followPlayhead();
    view.repaint();
    repaint (0, H - 62, W, 62);
    tile.repaint();
}
