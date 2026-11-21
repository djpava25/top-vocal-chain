#include "TopMasterEditor.h"

using namespace tmui;
using namespace tmaster;

namespace
{
    juce::String U (const char* s) { return juce::String::fromUTF8 (s); }
    const juce::Colour bg (0xff0e1015), panel (0xff15181f), panel2 (0xff1c2029), line (0xff2a2f3b),
                       text (0xffe9ecf2), dim (0xff8b93a5), cyan (0xff3fd3e6), gold (0xffffbf47), coral (0xffff6b6b),
                       green (0xff59e39b), violet (0xffa489ff);
    juce::Font font (float h, bool bold = false) { return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain)); }
    // mayusculas que respetan los acentos
    juce::String upper (const juce::String& s)
    {
        return s.toUpperCase().replace (U ("á"), U ("Á")).replace (U ("é"), U ("É")).replace (U ("í"), U ("Í"))
                .replace (U ("ó"), U ("Ó")).replace (U ("ú"), U ("Ú")).replace (U ("ñ"), U ("Ñ"));
    }
    juce::Colour moduleColour (int m)
    {
        static const juce::Colour c[] { cyan, cyan, violet, gold, green, gold, coral, cyan, gold, coral, violet, green, dim };
        return c[m];
    }
    void setParam (APVTS& s, const juce::String& id, float v) { if (auto* p = s.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v)); }
    float getParam (APVTS& s, const juce::String& id) { return s.getRawParameterValue (id)->load(); }
    // respuesta de un biquad en una frecuencia (dB)
    float biquadDb (const tvc::Biquad& b, double sr, float hz)
    {
        const double w = 2.0 * juce::MathConstants<double>::pi * hz / sr;
        const std::complex<double> z1 = std::polar (1.0, -w), z2 = z1 * z1;
        const auto num = (double) b.b0 + (double) b.b1 * z1 + (double) b.b2 * z2;
        const auto den = 1.0 + (double) b.a1 * z1 + (double) b.a2 * z2;
        return (float) (20.0 * std::log10 (std::abs (num) / std::abs (den) + 1e-12));
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
    setColour (juce::PopupMenu::highlightedBackgroundColourId, cyan.withAlpha (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::TooltipWindow::backgroundColourId, panel2);
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TooltipWindow::outlineColourId, line);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::BubbleComponent::backgroundColourId, panel2);
    setColour (juce::BubbleComponent::outlineColourId, line);
}
void Look::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);
    const float r = (float) juce::jmin (w, h) * 0.5f - 5.0f;
    const juce::Point<float> c ((float) x + (float) w * 0.5f, (float) y + (float) h * 0.5f);
    const float a = a0 + pos * (a1 - a0);
    juce::Path tr; tr.addCentredArc (c.x, c.y, r, r, 0, a0, a1, true);
    g.setColour (line); g.strokePath (tr, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const bool bip = s.getMinimum() < 0 && s.getMaximum() > 0;
    const float from = bip ? (a0 + a1) * 0.5f : a0;
    juce::Path v; v.addCentredArc (c.x, c.y, r, r, 0, juce::jmin (from, a), juce::jmax (from, a), true);
    g.setColour (accent); g.strokePath (v, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const float ri = r - 8.0f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c313d), c.x, c.y - ri, juce::Colour (0xff15181f), c.x, c.y + ri, false));
    g.fillEllipse (c.x - ri, c.y - ri, ri * 2, ri * 2);
    g.setColour (text);
    g.drawLine (juce::Line<float> ({ c.x + ri * 0.3f * std::sin (a), c.y - ri * 0.3f * std::cos (a) }, { c.x + (ri - 2) * std::sin (a), c.y - (ri - 2) * std::cos (a) }), 2.2f);
}
void Look::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1);
    const auto accent = b.findColour (juce::TextButton::buttonOnColourId);
    if (b.getToggleState()) { g.setColour (accent); g.fillRoundedRectangle (r, 7); return; }
    g.setColour (down ? panel2.brighter (0.2f) : over ? panel2.brighter (0.08f) : panel2); g.fillRoundedRectangle (r, 7);
    g.setColour (over ? accent.withAlpha (0.7f) : line); g.drawRoundedRectangle (r, 7, 1.2f);
}
void Look::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (b.getToggleState() ? bg : text);
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 2), juce::Justification::centred, 1);
}
juce::Font Look::getTextButtonFont (juce::TextButton&, int h) { return font (juce::jmin (13.0f, (float) h * 0.42f), true); }
juce::Font Look::getComboBoxFont (juce::ComboBox&) { return font (13.0f); }
void Look::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (panel2); g.fillRoundedRectangle (r, 7);
    g.setColour (line); g.drawRoundedRectangle (r, 7, 1.2f);
    juce::Path a; const float ax = (float) w - 14, ay = (float) h * 0.5f;
    a.addTriangle (ax - 4.5f, ay - 2.0f, ax + 4.5f, ay - 2.0f, ax, ay + 3.5f);
    g.setColour (dim); g.fillPath (a);
}
void Look::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool)
{
    auto r = b.getLocalBounds().toFloat();
    auto sw = juce::Rectangle<float> (r.getX() + 2, r.getCentreY() - 9, 34, 18);
    const bool on = b.getToggleState();
    g.setColour (on ? b.findColour (juce::ToggleButton::tickColourId) : line); g.fillRoundedRectangle (sw, 9);
    g.setColour (on ? bg : text.withAlpha (0.8f)); g.fillEllipse (on ? sw.getRight() - 16 : sw.getX() + 2, sw.getY() + 2, 14, 14);
    g.setColour (text); g.setFont (font (12.5f, true));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (42), juce::Justification::centredLeft);
}

//==============================================================================
Control::Control (APVTS& s, const juce::String& pid, const juce::String& lbl, juce::Colour accent) : id (pid), label (lbl)
{
    auto* prm = s.getParameter (pid);
    if (dynamic_cast<juce::AudioParameterChoice*> (prm) != nullptr) kind = 1;
    else if (dynamic_cast<juce::AudioParameterBool*> (prm) != nullptr) kind = 2;
    if (kind == 0)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.22f, juce::MathConstants<float>::pi * 2.78f, true);
        slider.setPopupDisplayEnabled (true, true, nullptr);
        slider.onValueChange = [this] { repaint(); };
        addAndMakeVisible (slider);
        sa = std::make_unique<APVTS::SliderAttachment> (s, pid, slider);
    }
    else if (kind == 1)
    {
        combo.addItemList (dynamic_cast<juce::AudioParameterChoice*> (prm)->choices, 1);
        addAndMakeVisible (combo);
        ca = std::make_unique<APVTS::ComboBoxAttachment> (s, pid, combo);
    }
    else
    {
        toggle.setButtonText ("");
        toggle.setColour (juce::ToggleButton::tickColourId, accent);
        addAndMakeVisible (toggle);
        ba = std::make_unique<APVTS::ButtonAttachment> (s, pid, toggle);
    }
}
void Control::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (18);
    if (kind == 0) { slider.setBounds (r.withTrimmedBottom (16)); }
    else if (kind == 1) combo.setBounds (r.withSizeKeepingCentre (r.getWidth() - 4, 28));
    else toggle.setBounds (r.withSizeKeepingCentre (44, 28));
}
void Control::paint (juce::Graphics& g)
{
    g.setColour (dim); g.setFont (font (11.0f, true));
    g.drawFittedText (upper (label), 0, 0, getWidth(), 16, juce::Justification::centred, 1, 0.7f);
    if (kind == 0)
    {
        g.setColour (text); g.setFont (font (12.0f, true));
        g.drawText (slider.getTextFromValue (slider.getValue()), 0, getHeight() - 16, getWidth(), 15, juce::Justification::centred);
    }
}

//==============================================================================
ModuleRow::ModuleRow (TopMasterProcessor& p, int m) : module (m), proc (p) {}
void ModuleRow::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1, 2);
    const bool on = proc.apvts.getRawParameterValue (juce::String (modulePrefix (module)) + "_on")->load() > 0.5f;
    const auto col = moduleColour (module);
    g.setColour (selected ? panel2.brighter (0.08f) : panel2); g.fillRoundedRectangle (r, 8);
    if (selected) { g.setColour (col); g.drawRoundedRectangle (r, 8, 1.5f); }
    else { g.setColour (line); g.drawRoundedRectangle (r, 8, 1.0f); }
    // interruptor
    const auto pw = juce::Rectangle<float> (r.getX() + 10, r.getCentreY() - 9, 18, 18);
    g.setColour (on ? col : line); g.fillEllipse (pw);
    g.setColour (on ? bg : dim);
    juce::Path ic; ic.addCentredArc (pw.getCentreX(), pw.getCentreY(), 5, 5, 0, 0.6f, 5.7f, true);
    g.strokePath (ic, juce::PathStrokeType (1.6f));
    g.drawLine (pw.getCentreX(), pw.getCentreY() - 6, pw.getCentreX(), pw.getCentreY() - 1, 1.6f);
    g.setColour (on ? text : dim); g.setFont (font (13.0f, true));
    g.drawText (moduleName (module), (int) r.getX() + 36, (int) r.getY(), (int) r.getWidth() - 80, (int) r.getHeight(), juce::Justification::centredLeft);
    // reduccion en vivo
    float gr = 0;
    if (on)
    {
        if (module == DYN) for (auto& v : proc.chain->dyn.grLive) gr = juce::jmax (gr, v.load());
        else if (module == MAXIMIZER) gr = proc.chain->max.grLive.load();
        else if (module == DEQ) for (auto& v : proc.chain->deq.grLive) gr = juce::jmax (gr, std::abs (v.load()));
    }
    if (gr > 0.05f)
    {
        const auto m = juce::Rectangle<float> (r.getX() + 36, r.getBottom() - 7, juce::jmin (r.getWidth() - 80, gr * 12.0f), 3);
        g.setColour (coral); g.fillRoundedRectangle (m, 1.5f);
    }
    if (module < numReorderable)
    {
        g.setColour (dim);
        juce::Path up, dn;
        const float ax = r.getRight() - 30;
        up.addTriangle (ax - 6, r.getCentreY() - 3, ax + 6, r.getCentreY() - 3, ax, r.getCentreY() - 11);
        dn.addTriangle (ax + 14, r.getCentreY() + 3, ax + 26, r.getCentreY() + 3, ax + 20, r.getCentreY() + 11);
        g.fillPath (up); g.fillPath (dn);
    }
}
void ModuleRow::mouseDown (const juce::MouseEvent& e)
{
    const auto r = getLocalBounds().toFloat();
    if (e.position.x < 40)
    {
        const auto id = juce::String (modulePrefix (module)) + "_on";
        setParam (proc.apvts, id, proc.apvts.getRawParameterValue (id)->load() > 0.5f ? 0.0f : 1.0f);
        repaint();
    }
    else if (module < numReorderable && e.position.x > r.getRight() - 40)
    {
        proc.moveModule (module, e.position.x < r.getRight() - 20 ? -1 : 1);
        if (onReorder) onReorder();
        return;
    }
    if (onSelect) onSelect (module);
}

//==============================================================================
Display::Display (TopMasterProcessor& p) : proc (p)
{
    fft = std::make_unique<juce::dsp::FFT> (12);
    win.resize (4096); buf.resize (8192);
    for (int i = 0; i < 4096; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (6.2831853f * i / 4096);
    specIn.assign (300, -120.0f); specOut.assign (300, -120.0f);
}
float Display::xOf (float hz) const { return (float) getWidth() * std::log (hz / 20.0f) / std::log (1000.0f); }
float Display::hzOf (float x) const { return 20.0f * std::pow (1000.0f, x / (float) getWidth()); }
float Display::yDb (float db) const { return 20.0f + (float) (getHeight() - 40) * juce::jlimit (0.0f, 1.0f, -db / 90.0f); }
float Display::yEq (float db) const { return (float) getHeight() * 0.5f - db / 12.0f * (float) getHeight() * 0.38f; }

void Display::updateSpectrum()
{
    auto one = [this] (const Tap& t, std::vector<float>& spec)
    {
        t.read (buf.data(), 4096);
        for (int i = 0; i < 4096; ++i) buf[(size_t) i] *= win[(size_t) i];
        std::fill (buf.begin() + 4096, buf.end(), 0.0f);
        fft->performFrequencyOnlyForwardTransform (buf.data(), true);
        const double sr = proc.getSampleRate() > 0 ? proc.getSampleRate() : 44100.0;
        for (int i = 0; i < 300; ++i)
        {
            const float f = 20.0f * std::pow (1000.0f, (float) i / 299.0f);
            const int a = juce::jlimit (1, 2047, (int) (f / 1.03f * 4096 / sr)), b = juce::jlimit (a, 2047, (int) (f * 1.03f * 4096 / sr));
            float m = 0; for (int k = a; k <= b; ++k) m = juce::jmax (m, buf[(size_t) k]);
            const float db = 20.0f * std::log10 (m * 2.0f / 2048.0f + 1e-9f) + 3.0f * std::log2 (f / 1000.0f);
            spec[(size_t) i] = db > spec[(size_t) i] ? db : spec[(size_t) i] - 1.5f;
        }
    };
    one (proc.chain->inTap, specIn);
    one (proc.chain->outTap, specOut);
}

juce::StringArray Display::crossoverIds() const
{
    switch (module)
    {
        case DYN: return { "dyn_x1", "dyn_x2" };
        case IMPACT: return { "imp_x1", "imp_x2", "imp_x3" };
        case IMAGER: return { "img_x1", "img_x2", "img_x3" };
        case EXCITER: return { "exc_x1", "exc_x2" };
        case BASS: return { "bc_freq" };
        case SHAPER: return { "ss_low", "ss_high" };
        default: return {};
    }
}

float Display::eqResponse (float hz) const
{
    const double sr = proc.getSampleRate() > 0 ? proc.getSampleRate() : 44100.0;
    float db = 0;
    for (int b = 0; b < 8; ++b)
    {
        const juce::String p = "eq_b" + juce::String (b + 1);
        if (getParam (proc.apvts, p + "_on") < 0.5f) continue;
        tvc::Biquad bq;
        EqModule::design (bq, b, sr, getParam (proc.apvts, p + "_f"), getParam (proc.apvts, p + "_g"), getParam (proc.apvts, p + "_q"));
        db += biquadDb (bq, sr, hz);
    }
    return db;
}

int Display::hitTest (juce::Point<float> p, juce::String& id) const
{
    id = {};
    if (module == EQ || module == DEQ)
    {
        const int nb = module == EQ ? 8 : 4;
        for (int b = 0; b < nb; ++b)
        {
            const juce::String pre = (module == EQ ? "eq_b" : "deq_b") + juce::String (b + 1);
            const float gdb = module == EQ ? ((b == 0 || b == 7) ? 0.0f : getParam (proc.apvts, pre + "_g")) : getParam (proc.apvts, pre + "_range");
            if (p.getDistanceFrom ({ xOf (getParam (proc.apvts, pre + "_f")), yEq (gdb) }) < 12.0f) return b;
        }
        return -1;
    }
    for (const auto& x : crossoverIds())
        if (std::abs (p.x - xOf (getParam (proc.apvts, x))) < 7.0f) { id = x; return 100; }
    return -1;
}
void Display::setP (const juce::String& id, float v) { setParam (proc.apvts, id, v); }

void Display::mouseMove (const juce::MouseEvent& e)
{
    juce::String id;
    const int h = hitTest (e.position, id);
    if (h != hover) { hover = h; repaint(); }
    setMouseCursor (h == 100 ? juce::MouseCursor::LeftRightResizeCursor : h >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
}
void Display::mouseDown (const juce::MouseEvent& e)
{
    dragBand = hitTest (e.position, dragId);
    if (dragBand >= 0 && dragBand < 100) { band = dragBand; if (onBand) onBand (band); }
}
void Display::mouseDrag (const juce::MouseEvent& e)
{
    const float hz = juce::jlimit (20.0f, 20000.0f, hzOf (e.position.x));
    if (dragBand == 100 && dragId.isNotEmpty()) { setP (dragId, hz); repaint(); return; }
    if (dragBand < 0) return;
    const juce::String pre = (module == EQ ? "eq_b" : "deq_b") + juce::String (dragBand + 1);
    setP (pre + "_f", hz);
    const float db = juce::jlimit (-12.0f, 12.0f, ((float) getHeight() * 0.5f - e.position.y) / ((float) getHeight() * 0.38f) * 12.0f);
    if (module == EQ && dragBand != 0 && dragBand != 7) setP (pre + "_g", db);
    if (module == DEQ) setP (pre + "_range", db);
    repaint();
}
void Display::mouseUp (const juce::MouseEvent&) { dragBand = -1; dragId = {}; }
void Display::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    juce::String id;
    int b = hitTest (e.position, id);
    if (b < 0 || b >= 100) b = band;
    if (module != EQ && module != DEQ) return;
    const juce::String q = (module == EQ ? "eq_b" : "deq_b") + juce::String (b + 1) + "_q";
    setP (q, juce::jlimit (0.3f, 8.0f, getParam (proc.apvts, q) * (1.0f + w.deltaY)));
}

void Display::paint (juce::Graphics& g)
{
    const float W = (float) getWidth(), H = (float) getHeight();
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0b0d12)); g.fillRoundedRectangle (r, 10);
    for (float f : { 30.f, 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f, 20000.f })
    {
        g.setColour (line.withAlpha (0.5f)); g.drawVerticalLine ((int) xOf (f), 0, H);
        g.setColour (dim); g.setFont (font (10.0f));
        g.drawText (f >= 1000 ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f), (int) xOf (f) - 18, (int) H - 16, 36, 12, juce::Justification::centred);
    }
    // espectros
    juce::Path pin, pout;
    pout.startNewSubPath (0, H);
    for (int i = 0; i < 300; ++i)
    {
        const float x = W * (float) i / 299.0f;
        if (i == 0) pin.startNewSubPath (x, yDb (specIn[(size_t) i])); else pin.lineTo (x, yDb (specIn[(size_t) i]));
        pout.lineTo (x, yDb (specOut[(size_t) i]));
    }
    pout.lineTo (W, H); pout.closeSubPath();
    g.setGradientFill (juce::ColourGradient (cyan.withAlpha (0.35f), 0, 0, cyan.withAlpha (0.03f), 0, H, false));
    g.fillPath (pout);
    g.setColour (dim.withAlpha (0.7f)); g.strokePath (pin, juce::PathStrokeType (1.0f));
    const auto col = moduleColour (module);

    if (module == EQ)
    {
        g.setColour (line); g.drawHorizontalLine ((int) yEq (0), 0, W);
        juce::Path c;
        for (int i = 0; i <= 240; ++i)
        {
            const float f = 20.0f * std::pow (1000.0f, (float) i / 240.0f);
            const float y = yEq (eqResponse (f));
            if (i == 0) c.startNewSubPath (xOf (f), y); else c.lineTo (xOf (f), y);
        }
        g.setColour (gold.withAlpha (0.25f)); g.strokePath (c, juce::PathStrokeType (6.0f));
        g.setColour (gold); g.strokePath (c, juce::PathStrokeType (2.0f));
    }
    if (module == EQ || module == DEQ)
    {
        const int nb = module == EQ ? 8 : 4;
        for (int b = 0; b < nb; ++b)
        {
            const juce::String pre = (module == EQ ? "eq_b" : "deq_b") + juce::String (b + 1);
            const bool on = getParam (proc.apvts, pre + "_on") > 0.5f;
            const float gdb = module == EQ ? ((b == 0 || b == 7) ? 0.0f : getParam (proc.apvts, pre + "_g")) : getParam (proc.apvts, pre + "_range");
            const juce::Point<float> pt (xOf (getParam (proc.apvts, pre + "_f")), yEq (gdb));
            if (module == DEQ && on)
            {
                const float live = proc.chain->deq.grLive[(size_t) b].load();
                g.setColour (coral); g.drawLine (pt.x, yEq (0), pt.x, yEq (live), 4.0f);
            }
            const bool sel = b == band || b == hover;
            g.setColour ((on ? col : dim).withAlpha (sel ? 0.35f : 0.12f)); g.fillEllipse (pt.x - 14, pt.y - 14, 28, 28);
            g.setColour (on ? (sel ? col : bg) : panel2); g.fillEllipse (pt.x - 10, pt.y - 10, 20, 20);
            g.setColour (on ? col : dim); g.drawEllipse (pt.x - 10, pt.y - 10, 20, 20, 2.0f);
            g.setColour (sel && on ? bg : text); g.setFont (font (11.0f, true));
            g.drawText (juce::String (b + 1), juce::Rectangle<float> (20, 20).withCentre (pt), juce::Justification::centred);
        }
    }
    const auto xs = crossoverIds();
    if (! xs.isEmpty())
    {
        std::vector<float> xp;
        for (const auto& id : xs) xp.push_back (xOf (getParam (proc.apvts, id)));
        if (module == SHAPER)
        {
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.fillRect (0.0f, 0.0f, xp[0], H); g.fillRect (xp[1], 0.0f, W - xp[1], H);
        }
        for (float x : xp)
        {
            g.setColour (col); g.drawLine (x, 0, x, H, 2.0f);
            g.fillRoundedRectangle (x - 4, H * 0.5f - 16, 8, 32, 3);
        }
        // valores por banda
        std::vector<float> edges { 0.0f }; for (float x : xp) edges.push_back (x); edges.push_back (W);
        g.setFont (font (13.0f, true));
        for (size_t b = 0; b + 1 < edges.size(); ++b)
        {
            juce::String t;
            if (module == DYN) t = "GR " + juce::String (proc.chain->dyn.grLive[b].load(), 1) + " dB";
            else if (module == IMAGER) t = "Ancho " + juce::String ((int) getParam (proc.apvts, "img_w" + juce::String ((int) b + 1))) + " %";
            else if (module == IMPACT) t = "Impacto " + juce::String ((int) getParam (proc.apvts, "imp_a" + juce::String ((int) b + 1))) + " %";
            else if (module == EXCITER) t = juce::String ((int) getParam (proc.apvts, "exc_a" + juce::String ((int) b + 1))) + " %";
            else if (module == BASS) t = b == 0 ? U ("Graves ") + juce::String (getParam (proc.apvts, "bc_gain"), 1) + " dB" : juce::String();
            g.setColour (text);
            g.drawText (t, (int) edges[b], 26, (int) (edges[b + 1] - edges[b]), 18, juce::Justification::centred);
        }
    }
    if (module == MAXIMIZER)
    {
        const float gr = proc.chain->max.grLive.load();
        auto m = juce::Rectangle<float> (W - 70, 30, 24, H - 70);
        g.setColour (panel2); g.fillRoundedRectangle (m, 6);
        g.setColour (coral); g.fillRoundedRectangle (m.withHeight (juce::jmin (m.getHeight(), gr / 12.0f * m.getHeight())), 6);
        g.setColour (text); g.setFont (font (12.0f, true));
        g.drawText ("GR " + juce::String (gr, 1), (int) W - 110, 8, 100, 16, juce::Justification::centredRight);
        const float ceil = getParam (proc.apvts, "max_ceil");
        g.setColour (gold); g.drawHorizontalLine ((int) yDb (ceil), 0, W - 80);
        g.drawText ("techo " + juce::String (ceil, 1) + " dBTP", 10, (int) yDb (ceil) - 18, 160, 16, juce::Justification::centredLeft);
    }
    g.setColour (line); g.drawRoundedRectangle (r.reduced (0.5f), 10, 1.0f);
    g.setColour (dim); g.setFont (font (11.0f));
    g.drawText (U ("gris = entrada · azul = salida"), 10, 6, 260, 14, juce::Justification::centredLeft);
}

//==============================================================================
AssistantPanel::AssistantPanel (TopMasterProcessor& p) : proc (p)
{
    genre.addItemList (TopMasterProcessor::genreNames(), 1); genre.setSelectedId (proc.genre + 1, juce::dontSendNotification);
    target.addItemList (TopMasterProcessor::targetNames(), 1); target.setSelectedId (proc.target + 1, juce::dontSendNotification);
    intensity.addItemList (TopMasterProcessor::intensityNames(), 1); intensity.setSelectedId (proc.intensity + 1, juce::dontSendNotification);
    genre.onChange = [this] { proc.genre = genre.getSelectedId() - 1; };
    target.onChange = [this] { proc.target = target.getSelectedId() - 1; };
    intensity.onChange = [this] { proc.intensity = intensity.getSelectedId() - 1; };
    useRef.setToggleState (proc.useReference, juce::dontSendNotification);
    useRef.setColour (juce::ToggleButton::tickColourId, gold);
    useRef.onClick = [this] { proc.useReference = useRef.getToggleState(); };
    listen.setColour (juce::TextButton::buttonOnColourId, gold);
    listen.onClick = [this]
    {
        if (proc.assistantBusy.load()) return;
        if (proc.listening.load()) proc.stopListeningAndRun();
        else { proc.assistantReport = {}; proc.startListening(); }
    };
    close.onClick = [this] { if (onClose) onClose(); };
    for (auto* c : { (juce::Component*) &genre, (juce::Component*) &target, (juce::Component*) &intensity, (juce::Component*) &useRef,
                     (juce::Component*) &listen, (juce::Component*) &close })
        addAndMakeVisible (c);
    startTimerHz (10);
}
void AssistantPanel::resized()
{
    genre.setBounds (30, 92, 230, 30);
    target.setBounds (270, 92, 190, 30);
    intensity.setBounds (470, 92, 100, 30);
    useRef.setBounds (30, 134, 320, 28);
    listen.setBounds (30, 178, 200, 44);
    close.setBounds (getWidth() - 130, getHeight() - 50, 110, 34);
}
void AssistantPanel::timerCallback()
{
    useRef.setEnabled (proc.hasReference());
    listen.setButtonText (proc.assistantBusy.load() ? U ("ANALIZANDO…") : proc.listening.load() ? U ("ANALIZAR AHORA") : U ("ESCUCHAR"));
    listen.setToggleState (proc.listening.load(), juce::dontSendNotification);
    repaint();
}
void AssistantPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (panel.withAlpha (0.98f)); g.fillRoundedRectangle (r, 14);
    g.setColour (gold); g.drawRoundedRectangle (r.reduced (0.75f), 14, 1.5f);
    g.setColour (text); g.setFont (font (18.0f, true));
    g.drawText (U ("ASISTENTE DE MASTERING"), 30, 18, 400, 26, juce::Justification::centredLeft);
    g.setColour (dim); g.setFont (font (12.5f));
    g.drawText (U ("Género / estilo"), 30, 72, 230, 16, juce::Justification::centredLeft);
    g.drawText (U ("Volumen final"), 270, 72, 190, 16, juce::Justification::centredLeft);
    g.drawText ("Intensidad", 470, 72, 100, 16, juce::Justification::centredLeft);
    g.drawFittedText (U ("Pulsa ESCUCHAR y dale play a la parte más fuerte de tu canción (coro). A los 30 s analiza solo, o pulsa ANALIZAR AHORA."),
                      30, 44, getWidth() - 60, 22, juce::Justification::centredLeft, 2);
    if (proc.listening.load())
    {
        const float s = proc.listenSeconds.load();
        auto bar = juce::Rectangle<float> (250, 192, (float) getWidth() - 280, 14);
        g.setColour (panel2); g.fillRoundedRectangle (bar, 7);
        g.setColour (gold); g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jmin (1.0f, s / 30.0f)), 7);
        g.setColour (text); g.setFont (font (12.5f, true));
        g.drawText (U ("Escuchando… ") + juce::String (s, 1) + " s" + (s < 0.1f ? U ("  (dale play en tu DAW)") : juce::String()), 250, 168, 360, 18, juce::Justification::centredLeft);
    }
    else if (proc.assistantBusy.load())
    {
        auto bar = juce::Rectangle<float> (250, 192, (float) getWidth() - 280, 14);
        g.setColour (panel2); g.fillRoundedRectangle (bar, 7);
        g.setColour (cyan); g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * proc.assistantProgress.load()), 7);
    }
    if (proc.assistantReport.isNotEmpty())
    {
        g.setColour (panel2); g.fillRoundedRectangle (30.0f, 240.0f, (float) getWidth() - 60, (float) getHeight() - 300, 10);
        g.setColour (text); g.setFont (font (13.5f));
        g.drawFittedText (proc.assistantReport, 46, 252, getWidth() - 92, getHeight() - 324, juce::Justification::topLeft, 10);
    }
}

//==============================================================================
TopMasterEditor::TopMasterEditor (TopMasterProcessor& p) : AudioProcessorEditor (&p), proc (p), display (p)
{
    setLookAndFeel (&look);
    tips.setLookAndFeel (&look);
    addAndMakeVisible (display);
    display.onBand = [this] (int b) { selectBand (b); };
    for (int m = 0; m < NumModules; ++m)
    {
        auto* r = rows.add (new tmui::ModuleRow (proc, m));
        r->onSelect = [this] (int mm) { selectModule (mm); };
        r->onReorder = [this] { layoutRows(); };
        r->setTooltip (moduleHelp (m));
        addAndMakeVisible (r);
    }
    assistantBtn.setColour (juce::TextButton::buttonOnColourId, gold);
    assistantBtn.setTooltip (U ("Analiza tu canción y arma la cadena de mastering según el género y el volumen que elijas"));
    assistantBtn.onClick = [this] { showAssistant (assistant == nullptr); };
    refBtn.setClickingTogglesState (true);
    refBtn.setColour (juce::TextButton::buttonOnColourId, violet);
    refBtn.setTooltip (U ("Escucha tu canción de referencia (al mismo volumen si 'Igualar volumen' está activo)"));
    refBtn.onClick = [this] { proc.referenceOn.store (refBtn.getToggleState() && proc.hasReference()); };
    loadRefBtn.setTooltip (U ("Carga una canción de referencia (o arrástrala sobre el plugin)"));
    loadRefBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Canción de referencia", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc) { if (fc.getResult().existsAsFile()) proc.loadReference (fc.getResult()); repaint(); });
    };
    bypassBtn.setClickingTogglesState (true);
    bypassBtn.setColour (juce::TextButton::buttonOnColourId, coral);
    bypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "bypass", bypassBtn);
    matchBtn.setColour (juce::ToggleButton::tickColourId, gold);
    matchBtn.setTooltip (U ("Al comparar (bypass o referencia) iguala el volumen para que solo escuches la diferencia de sonido"));
    matchAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "gainmatch", matchBtn);
    resetBtn.onClick = [this] { proc.chain->outMeter.resetFlag = true; proc.chain->inMeter.resetFlag = true; };
    resetBtn.setTooltip ("Reinicia el loudness integrado");
    inGain = std::make_unique<tmui::Control> (proc.apvts, "in_gain", "Entrada", cyan);
    for (auto* c : { (juce::Component*) &assistantBtn, (juce::Component*) &refBtn, (juce::Component*) &loadRefBtn, (juce::Component*) &bypassBtn,
                     (juce::Component*) &matchBtn, (juce::Component*) &resetBtn, (juce::Component*) inGain.get() })
        addAndMakeVisible (c);
    for (int b = 0; b < 8; ++b)
    {
        auto* bb = bandBtns.add (new juce::TextButton (juce::String (b + 1)));
        bb->setColour (juce::TextButton::buttonOnColourId, gold);
        bb->onClick = [this, b] { selectBand (b); };
        addChildComponent (bb);
    }
    setResizable (true, true);
    setResizeLimits (W * 2 / 3, H * 2 / 3, W * 3 / 2, H * 3 / 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) W / H);
    setSize (W, H);
    selectModule (EQ);
    startTimerHz (30);
}

TopMasterEditor::~TopMasterEditor()
{
    stopTimer();
    tips.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void TopMasterEditor::showAssistant (bool show)
{
    if (! show) { assistant.reset(); assistantBtn.setToggleState (false, juce::dontSendNotification); return; }
    assistant = std::make_unique<tmui::AssistantPanel> (proc);
    juce::Component::SafePointer<TopMasterEditor> safeThis (this);
    assistant->onClose = [safeThis] { juce::MessageManager::callAsync ([safeThis] { if (safeThis != nullptr) safeThis->showAssistant (false); }); };
    addAndMakeVisible (*assistant);
    assistantBtn.setToggleState (true, juce::dontSendNotification);
    resized();
}

void TopMasterEditor::filesDropped (const juce::StringArray& f, int, int)
{
    if (! f.isEmpty()) proc.loadReference (juce::File (f[0]));
    repaint();
}

void TopMasterEditor::selectModule (int m)
{
    module = m;
    display.module = m;
    for (auto* r : rows) { r->selected = r->module == m; r->repaint(); }
    if (m == EQ || m == DEQ) band = m == EQ ? 3 : 0;
    display.band = band;
    buildControls();
    repaint();
}

void TopMasterEditor::selectBand (int b)
{
    band = b;
    display.band = b;
    buildControls();
    display.repaint();
}

void TopMasterEditor::buildControls()
{
    controls.clear();
    const auto col = moduleColour (module);
    juce::StringArray ids, labels;
    auto add = [&] (const juce::String& id, const juce::String& l) { ids.add (id); labels.add (l); };
    if (module == EQ || module == DEQ)
    {
        const juce::String pre = (module == EQ ? "eq_b" : "deq_b") + juce::String (band + 1);
        add (pre + "_on", "Banda activa");
        add (pre + "_f", "Frecuencia");
        if (module == EQ && band != 0 && band != 7) add (pre + "_g", "Ganancia");
        if (! (module == EQ && (band == 1 || band == 6))) add (pre + "_q", "Q / ancho");
        if (module == EQ) add (pre + "_ch", "Canal");
        else { add (pre + "_thr", "Umbral"); add (pre + "_range", "Rango"); add ("deq_att", "Ataque"); add ("deq_rel", "Release"); }
    }
    else
    {
        for (const auto& f : table().floats) if (f.module == module) add (f.id, f.name);
        for (const auto& c : table().choices) if (c.module == module) add (c.id, c.name);
        for (const auto& b : table().bools) if (b.module == module && ! b.id.endsWith ("_on")) add (b.id, b.name);
    }
    for (int i = 0; i < ids.size(); ++i)
    {
        auto* c = controls.add (new tmui::Control (proc.apvts, ids[i], labels[i], col));
        addAndMakeVisible (c);
    }
    const int nb = module == EQ ? 8 : module == DEQ ? 4 : 0;
    for (int b = 0; b < 8; ++b) { bandBtns[b]->setVisible (b < nb); bandBtns[b]->setToggleState (b == band, juce::dontSendNotification); }
    resized();
}

void TopMasterEditor::layoutRows()
{
    const auto o = proc.getOrder();
    const float s = (float) getWidth() / (float) W;
    int y = 96;
    auto place = [&] (int m) { rows[m]->setBounds (16, y, 236, 46); rows[m]->setTransform (juce::AffineTransform::scale (s)); y += 48; };
    for (int i = 0; i < numReorderable; ++i) place (o[(size_t) i]);
    y += 14;
    place (MAXIMIZER); place (DITHER);
    for (auto* r : rows) r->repaint();
}

void TopMasterEditor::resized()
{
    const float s = (float) getWidth() / (float) W;
    const auto T = juce::AffineTransform::scale (s);
    auto put = [&] (juce::Component& c, int x, int y, int w, int h) { c.setBounds (x, y, w, h); c.setTransform (T); };
    put (assistantBtn, 300, 12, 150, 34);
    put (loadRefBtn, 470, 12, 116, 34);
    put (refBtn, 592, 12, 140, 34);
    put (matchBtn, 746, 12, 170, 34);
    put (bypassBtn, 1158, 12, 106, 34);
    put (display, 268, 66, 732, 330);
    layoutRows();
    // controles del modulo
    const int nb = module == EQ ? 8 : module == DEQ ? 4 : 0;
    for (int b = 0; b < 8; ++b) put (*bandBtns[b], 280 + b * 44, 446, 40, 28);
    const int cw = 80, perRow = 9;
    const int y0 = nb > 0 ? 488 : 450;
    for (int i = 0; i < controls.size(); ++i)
        put (*controls[i], 280 + (i % perRow) * cw, y0 + (i / perRow) * 112, cw - 4, 104);
    put (*inGain, 1030, 640, 90, 104);
    put (resetBtn, 1140, 676, 110, 30);
    if (assistant != nullptr) put (*assistant, 340, 120, 600, 470);
}

void TopMasterEditor::paint (juce::Graphics& g)
{
    const float sc = (float) getWidth() / (float) W;
    g.addTransform (juce::AffineTransform::scale (sc));
    g.fillAll (bg);
    g.setGradientFill (juce::ColourGradient (panel, 0, 0, bg, 0, 58, false));
    g.fillRect (0, 0, W, 58);
    g.setColour (line); g.drawHorizontalLine (57, 0, (float) W);
    // logo
    {
        juce::Path p;
        p.addTriangle (18, 42, 34, 14, 50, 42);
        g.setGradientFill (juce::ColourGradient (gold, 18, 14, cyan, 50, 42, false));
        g.fillPath (p);
        g.setColour (bg); g.fillRect (24.0f, 34.0f, 20.0f, 3.0f);
    }
    g.setColour (text); g.setFont (font (21.0f, true));
    g.drawText ("TOP MASTER", 60, 8, 220, 24, juce::Justification::centredLeft);
    g.setColour (dim); g.setFont (font (11.0f));
    g.drawText (U ("suite de mastering · by Dj Pava Hm Music"), 61, 31, 230, 14, juce::Justification::centredLeft);
    if (proc.hasReference())
    {
        g.setColour (violet); g.setFont (font (11.0f, true));
        g.drawText ("ref: " + proc.getReferenceName() + "  (" + juce::String (proc.getReferenceLufs(), 1) + " LUFS)", 930, 18, 220, 22, juce::Justification::centredLeft);
    }
    // cadena
    g.setColour (dim); g.setFont (font (11.5f, true));
    g.drawText (U ("CADENA  (▲▼ para reordenar)"), 18, 70, 236, 18, juce::Justification::centredLeft);
    // panel del modulo
    g.setColour (panel); g.fillRoundedRectangle (268.0f, 404.0f, 732.0f, 360.0f, 10.0f);
    g.setColour (moduleColour (module)); g.fillRoundedRectangle (280.0f, 418.0f, 4.0f, 16.0f, 2.0f);
    g.setColour (text); g.setFont (font (15.0f, true));
    g.drawText (upper (moduleName (module)), 292, 412, 260, 26, juce::Justification::centredLeft);
    g.setColour (dim); g.setFont (font (12.0f));
    g.drawFittedText (moduleHelp (module), 560, 410, 428, 34, juce::Justification::centredLeft, 2);
    if (module == EQ || module == DEQ)
    {
        g.drawText ("Banda", 280 + (module == EQ ? 8 : 4) * 44 + 8, 446, 200, 28, juce::Justification::centredLeft);
    }
    // medidores
    const float mx = 1012, mw = 252;
    g.setColour (panel); g.fillRoundedRectangle (mx, 66, mw, 698, 10);
    auto& om = proc.chain->outMeter;
    auto& im = proc.chain->inMeter;
    g.setColour (dim); g.setFont (font (11.5f, true));
    g.drawText ("LOUDNESS (SALIDA)", (int) mx + 14, 76, 200, 16, juce::Justification::centredLeft);
    auto big = [&] (const juce::String& lab, float v, int y, juce::Colour c, float size)
    {
        g.setColour (dim); g.setFont (font (11.0f, true));
        g.drawText (lab, (int) mx + 14, y, 120, 16, juce::Justification::centredLeft);
        g.setColour (c); g.setFont (font (size, true));
        g.drawText (v > -99 ? juce::String (v, 1) : juce::String ("-inf"), (int) mx + 110, y - 6, (int) mw - 124, (int) size + 6, juce::Justification::centredRight);
    };
    const float integ = om.integrated.load();
    const float goal = TopMasterProcessor::targetLufs (proc.target);
    big ("INTEGRADO", integ, 104, std::abs (integ - goal) < 1.0f ? green : text, 30.0f);
    big ("CORTO (3 s)", om.shortTerm.load(), 148, text, 20.0f);
    big (U ("MOMENTÁNEO"), om.momentary.load(), 180, text, 20.0f);
    big ("TRUE PEAK", om.truePeak.load(), 212, om.truePeak.load() > -0.1f ? coral : text, 20.0f);
    big ("ENTRADA (INT.)", im.integrated.load(), 244, dim, 16.0f);
    g.setColour (dim); g.setFont (font (11.0f));
    g.drawText ("objetivo " + juce::String (goal, 1) + " LUFS", (int) mx + 14, 270, 220, 14, juce::Justification::centredLeft);
    // barra de loudness corto con el objetivo
    {
        auto bar = juce::Rectangle<float> (mx + 14, 294, mw - 28, 14);
        g.setColour (panel2); g.fillRoundedRectangle (bar, 7);
        auto toX = [&] (float l) { return bar.getX() + juce::jlimit (0.0f, 1.0f, (l + 30.0f) / 26.0f) * bar.getWidth(); };
        g.setGradientFill (juce::ColourGradient (cyan, bar.getX(), 0, gold, bar.getRight(), 0, false));
        g.fillRoundedRectangle (bar.withRight (juce::jmax (bar.getX() + 4, toX (om.shortTerm.load()))), 7);
        g.setColour (text); g.fillRect (toX (goal) - 1.0f, bar.getY() - 4, 2.0f, bar.getHeight() + 8);
    }
    // reduccion del maximizador y correlacion
    big ("MAXIMIZADOR GR", -proc.chain->max.grLive.load(), 330, coral, 18.0f);
    big (U ("CORRELACIÓN"), proc.chain->img.correlation.load(), 362, text, 18.0f);
    if (proc.apvts.getRawParameterValue ("bypass")->load() > 0.5f || proc.referenceOn.load())
    {
        g.setColour (gold); g.setFont (font (12.0f, true));
        g.drawText (U ("Comparando · igualado ") + juce::String (proc.matchGainDb.load(), 1) + " dB", (int) mx + 14, 400, 230, 16, juce::Justification::centredLeft);
    }
    g.setColour (dim); g.setFont (font (11.0f));
    g.drawFittedText (U ("Streaming (Spotify, YouTube, Apple): -14 LUFS y techo -1 dBTP.\nUrbano / club: -9 a -7 LUFS."), (int) mx + 14, 560, (int) mw - 28, 60, juce::Justification::topLeft, 4);
}

void TopMasterEditor::timerCallback()
{
    display.updateSpectrum();
    display.repaint();
    for (auto* r : rows) r->repaint();
    refBtn.setToggleState (proc.referenceOn.load(), juce::dontSendNotification);
    const float s = (float) getWidth() / (float) W;
    repaint (juce::Rectangle<int> (1012, 66, 252, 360).transformedBy (juce::AffineTransform::scale (s)));
}
