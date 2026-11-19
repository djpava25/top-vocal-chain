#include "PluginEditor.h"

using namespace tvcui;
using namespace cui;

static juce::String U (const char* s) { return juce::String::fromUTF8 (s); }

//==============================================================================
// BASE DE MODULO
//==============================================================================
Module::Module (TopVocalChainProcessor& p, const juce::String& t, const char* onId, int sd)
    : proc (p), title (t), onParam (onId), seed (sd)
{
    if (onParam != nullptr)
    {
        inSwitch = std::make_unique<ModuleSwitch> ([this] { return status(); });
        inSwitch->setClickingTogglesState (true);
        attach (*inSwitch, onParam);
    }
}

int Module::status() const { return onParam == nullptr ? 1 : (param (onParam) ? 1 : 0); }

void Module::attach (juce::Button& b, const char* id)
{
    addAndMakeVisible (b);
    btnAtts.push_back (std::make_unique<APVTS::ButtonAttachment> (proc.apvts, id, b));
}

void Module::setupBox (juce::ComboBox& box, const juce::StringArray& items, const char* id)
{
    box.addItemList (items, 1);
    addAndMakeVisible (box);
    boxAtts.push_back (std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, id, box));
}

void Module::resized()
{
    if (inSwitch) inSwitch->setBounds (getWidth() - 60, 7, 52, 22);
    cache = {};
    layout();
}

void Module::paint (juce::Graphics& g)
{
    // Fondo estatico cacheado a la resolucion real de pantalla (nitido a 200 %)
    const float sc = juce::jmax (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    const int cw = juce::roundToInt ((float) getWidth() * sc), ch = juce::roundToInt ((float) getHeight() * sc);
    if (cache.isNull() || cache.getWidth() != cw || cache.getHeight() != ch)
    {
        cache = juce::Image (juce::Image::ARGB, cw, ch, true);
        juce::Graphics cg (cache);
        cg.addTransform (juce::AffineTransform::scale (sc));
        moduleFrame (cg, getLocalBounds().toFloat(), title, seed);
        paintStatic (cg);
    }
    g.drawImage (cache, 0, 0, getWidth(), getHeight(), 0, 0, cw, ch);
    paintDynamic (g);
}

void Module::paintOverChildren (juce::Graphics& g)
{
    // Modulo apagado: todo el cuerpo queda atenuado (la cabecera con el boton sigue clara)
    if (onParam != nullptr && ! param (onParam))
    {
        g.setColour (juce::Colour (0xff15161a).withAlpha (0.5f));
        g.fillRect (getLocalBounds().withTrimmedTop (37).reduced (1, 0).withTrimmedBottom (1));
    }
}

void Module::tick()
{
    if (inSwitch) inSwitch->tick();
    const bool nowOn = onParam == nullptr || param (onParam);
    if (nowOn != lastOn) { lastOn = nowOn; repaint(); }
}

//==============================================================================
// MODULOS
//==============================================================================
namespace
{
const char* preText[]  { "Válvula: cálida, armónicos pares", "Cinta: suave y redonda, agudos dulces",
                         "Transistor: filo y mordida", "Transformador: cuerpo en los graves",
                         "Diodo: agresivo y asimétrico" };
const char* compText[] { "Limpio (VCA): transparente, respeta Attack y Release.",
                         "Opto (estilo LA-2A): lento y musical. Cuanto más comprime, más lento suelta.",
                         "FET (estilo 1176): atrapa los picos en ~1 ms. Agresivo y con presencia.",
                         "Vari-Mu (estilo Fairchild): el ratio sube con el nivel. Suave y pegado." };
const char* eqText[]   { "Limpio: preciso, sin color.",
                         "Estilo Neve 1073: frecuencias por pasos, curvas anchas, color de transformador.",
                         "Estilo API 550: Q proporcional (más ganancia, curva más estrecha). Punch.",
                         "Estilo Pultec: truco de graves (realce + corte) y aire sedoso." };
const char* wideText[] { "Ensancha la voz sin perder nada en mono.",
                         "Dos voces moduladas en contrafase.",
                         "Doblaje automático: dos tomas falsas a los lados.",
                         "Izquierda unos cents arriba, derecha abajo.",
                         "Ensemble de 3 voces, espacioso y sutil." };

juce::String noteName (int note)
{
    return note >= 0 ? TopVocalChainProcessor::keyNames()[note % 12] + juce::String (note / 12 - 1) : juce::String ("--");
}

//------------------------------------------------------------------------------
struct PreampModule : Module
{
    Knob drive, inGain, warmth, trim;
    juce::ComboBox type;
    int lastType = -1;

    PreampModule (TopVocalChainProcessor& p)
        : Module (p, "PREAMP", "pre_on", 28),
          drive (p.apvts, "pre_drive", "DRIVE", Col::red, "0", "12", "24"),
          inGain (p.apvts, "in_gain", "INPUT", Col::slate, "-24", "0", "+24"),
          warmth (p.apvts, "pre_warmth", "WARMTH", Col::orange, "0", "", "100"),
          trim (p.apvts, "pre_trim", "TRIM", Col::slate, "-12", "0", "+12")
    {
        for (auto* k : { &drive, &inGain, &warmth, &trim }) add (*k);
        setupBox (type, TopVocalChainProcessor::preTypeNames(), "pre_type");
    }
    int status() const override { return param ("pre_on") ? (proc.meters.preHot.load() ? 2 : 1) : 0; }
    void layout() override
    {
        type.setBounds (14, 46, 186, 24);
        drive.setBounds (37, 90, 140, 140);
        inGain.setBounds (6, 230, 100, 96);
        warmth.setBounds (108, 230, 100, 96);
        trim.setBounds (57, 326, 100, 86);
    }
    void paintDynamic (juce::Graphics& g) override
    {
        g.setFont (font (11.0f));
        g.setColour (Col::inkDim);
        g.drawFittedText (U (preText[juce::jlimit (0, 4, choice ("pre_type"))]), { 14, 73, 186, 14 }, juce::Justification::centred, 1);
    }
    void tick() override
    {
        Module::tick();
        if (choice ("pre_type") != lastType) { lastType = choice ("pre_type"); repaint (10, 70, 194, 18); }
    }
};

//------------------------------------------------------------------------------
struct AiModule : Module
{
    Knob amount;
    LitButton listen { "ESCUCHAR", Col::amber };
    juce::TextButton reset { "RESET" };
    juce::Rectangle<int> disp { 12, 220, 190, 170 };

    AiModule (TopVocalChainProcessor& p)
        : Module (p, "IA VOICE", nullptr, 45),
          amount (p.apvts, "ai_amount", "INTELIGENCIA", Col::purple, "0", "50", "100")
    {
        add (amount);
        listen.setClickingTogglesState (true);
        attach (listen, "ai_listen");
        reset.onClick = [this] { proc.requestAIReset(); };
        add (reset);
    }
    void layout() override
    {
        listen.setBounds (getWidth() - 96, 7, 88, 22);
        amount.setBounds (37, 44, 140, 140);
        reset.setBounds (70, 188, 74, 24);
    }
    void paintDynamic (juce::Graphics& g) override
    {
        const auto& m = proc.meters;
        auto r = disp.toFloat();
        lcd (g, r);
        auto t = r.reduced (8, 7);
        lcdText (g, param ("ai_listen") ? "ESCUCHANDO" : "CONGELADO", t.removeFromTop (15), 11.5f, true, juce::Justification::centredLeft);
        auto cb = t.removeFromTop (5);
        g.setColour (Col::amber.withAlpha (0.15f)); g.fillRect (cb);
        g.setColour (Col::amber); g.fillRect (cb.withWidth (cb.getWidth() * m.aiConfidence.load()));
        t.removeFromTop (4);
        const float avg = m.aiAvgPitch.load();
        lcdText (g, avg > 0 ? juce::String (avg < 165 ? "VOZ GRAVE " : avg < 300 ? "VOZ MEDIA " : "VOZ AGUDA ") + juce::String ((int) avg) + " Hz"
                            : juce::String ("VOZ --"), t.removeFromTop (15), 11.5f, true, juce::Justification::centredLeft);
        auto row = [&] (const juce::String& label, float v, const juce::String& unit, int dec = 1)
        {
            auto rr = t.removeFromTop (15.5f);
            lcdText (g, label, rr, 10.5f, false, juce::Justification::centredLeft, Col::amber.withAlpha (0.65f));
            const auto num = dec == 0 ? juce::String (juce::roundToInt (v)) : juce::String (v, dec);
            lcdText (g, (v > 0.05f ? "+" : "") + num + " " + unit, rr, 11.0f, true, juce::Justification::centredRight);
        };
        row ("GANANCIA", m.aiGain.load(), "dB");
        row ("LOW CUT", m.aiLowCut.load(), "Hz", 0);
        row ("BARRO", m.aiMud.load(), "dB");
        row ("PRESENCIA", m.aiPresence.load(), "dB");
        row ("AIRE", m.aiAir.load(), "dB");
        row ("DE-ESSER", m.aiDeEss.load(), "dB");
        row ("COMP THR", m.aiCompThr.load(), "dB");
        row ("VEL. TUNE", m.aiTuneSpeed.load(), "ms", 0);
    }
    void tick() override { Module::tick(); repaint (disp.expanded (4)); }
};

//------------------------------------------------------------------------------
struct TuneModule : Module
{
    Knob speed, amount;
    juce::ComboBox key, scale;
    juce::Rectangle<int> disp { 12, 46, 208, 98 };

    TuneModule (TopVocalChainProcessor& p)
        : Module (p, "AUTOTUNE", "tune_on", 62),
          speed (p.apvts, "tune_speed", "SPEED", Col::green, "0", "60", "400"),
          amount (p.apvts, "tune_amount", "AMOUNT", Col::green, "0", "", "100")
    {
        add (speed); add (amount);
        setupBox (key, TopVocalChainProcessor::keyNames(), "tune_key");
        setupBox (scale, TopVocalChainProcessor::scaleNames(), "tune_scale");
    }
    void layout() override
    {
        key.setBounds (12, 152, 70, 24);
        scale.setBounds (88, 152, 132, 24);
        speed.setBounds (8, 186, 108, 126);
        amount.setBounds (116, 186, 108, 126);
    }
    void paintDynamic (juce::Graphics& g) override
    {
        const auto& m = proc.meters;
        auto r = disp.toFloat();
        lcd (g, r);
        const int note = (int) m.targetNote.load();
        const float hz = m.pitchHz.load(), cents = m.correctionCents.load();
        lcdText (g, noteName (note), r.withTrimmedLeft (12).withWidth (100).withHeight (60), 38.0f, true, juce::Justification::centredLeft,
                 note >= 0 ? Col::amber : Col::amber.withAlpha (0.35f));
        auto info = r.withTrimmedLeft (106).reduced (8, 10);
        lcdText (g, hz > 0 ? juce::String (hz, 1) + " Hz" : U ("SIN VOZ"), info.removeFromTop (18), 13.0f, true, juce::Justification::centredRight);
        lcdText (g, param ("tune_on") ? "CORR " + juce::String (juce::roundToInt (cents)) + " c" : juce::String ("BYPASS"),
                 info.removeFromTop (18), 12.0f, false, juce::Justification::centredRight, Col::amber.withAlpha (0.8f));
        auto bar = r.withTrimmedTop (r.getHeight() - 24).reduced (12, 7);
        g.setColour (Col::amber.withAlpha (0.12f)); g.fillRect (bar);
        const float c = juce::jlimit (-100.0f, 100.0f, cents) / 100.0f;
        const float mid = bar.getCentreX(), x2 = mid + c * bar.getWidth() * 0.5f;
        g.setColour (Col::amber); g.fillRect (juce::jmin (mid, x2), bar.getY(), std::abs (x2 - mid) + 1.5f, bar.getHeight());
        g.setColour (Col::amber.withAlpha (0.6f)); g.fillRect (mid - 0.5f, bar.getY() - 3, 1.0f, bar.getHeight() + 6);
    }
    void tick() override { Module::tick(); repaint (disp.expanded (4)); }
};

//------------------------------------------------------------------------------
struct WideModule : Module
{
    Knob amount;
    juce::OwnedArray<LitButton> modes;
    juce::Rectangle<int> disp { 12, 292, 208, 106 };

    WideModule (TopVocalChainProcessor& p)
        : Module (p, "WIDER", "wide_on", 79),
          amount (p.apvts, "wide_amount", "WIDER", Col::purple, "0", "50", "100")
    {
        add (amount);
        const auto& names = TopVocalChainProcessor::wideModeNames();
        for (int i = 0; i < names.size(); ++i)
        {
            auto* b = modes.add (new LitButton (names[i].toUpperCase(), Col::amber));
            b->onClick = [this, i]
            {
                auto* prm = proc.apvts.getParameter ("wide_mode");
                prm->setValueNotifyingHost (prm->convertTo0to1 ((float) i));
            };
            add (*b);
        }
    }
    void layout() override
    {
        for (int i = 0; i < modes.size(); ++i)
        {
            const int row = i < 3 ? 0 : 1;
            modes[i]->setBounds (row == 0 ? 12 + i * 70 : 12 + (i - 3) * 105, 46 + row * 30, row == 0 ? 66 : 101, 26);
        }
        amount.setBounds (36, 112, 160, 170);
    }
    void paintDynamic (juce::Graphics& g) override
    {
        auto r = disp.toFloat();
        lcd (g, r);
        auto t = r.reduced (10, 8);
        const int mode = juce::jlimit (0, 4, choice ("wide_mode"));
        lcdText (g, TopVocalChainProcessor::wideModeNames()[mode].toUpperCase(), t.removeFromTop (18), 14.0f, true, juce::Justification::centredLeft);
        g.setFont (font (11.5f));
        g.setColour (Col::amber.withAlpha (0.75f));
        g.drawFittedText (U (wideText[mode]), t.removeFromTop (32).toNearestInt(), juce::Justification::topLeft, 2);
        t.removeFromTop (4);
        lcdText (g, U ("ANCHO ESTÉREO"), t.removeFromTop (14), 10.5f, false, juce::Justification::centredLeft, Col::amber.withAlpha (0.65f));
        auto bar = t.removeFromTop (12);
        g.setColour (Col::amber.withAlpha (0.12f)); g.fillRect (bar);
        const float wv = juce::jlimit (0.0f, 1.0f, proc.meters.stereoWidth.load() * 1.6f);
        g.setColour (Col::amber); g.fillRect (bar.withSizeKeepingCentre (bar.getWidth() * wv, bar.getHeight()));
    }
    void tick() override
    {
        Module::tick();
        const int mode = choice ("wide_mode");
        for (int i = 0; i < modes.size(); ++i)
            if (modes[i]->getToggleState() != (i == mode)) modes[i]->setToggleState (i == mode, juce::dontSendNotification);
        repaint (disp.expanded (4));
    }
};

//------------------------------------------------------------------------------
struct CompModule : Module
{
    Knob thr, ratio, attack, release, makeup;
    juce::ComboBox type;
    juce::Rectangle<int> grBox { 140, 556, 58, 100 }, info { 14, 690, 186, 120 };

    CompModule (TopVocalChainProcessor& p)
        : Module (p, "COMPRESSOR", "comp_on", 96),
          thr (p.apvts, "comp_thr", "THRESHOLD", Col::charcoal, "-60", "-30", "0"),
          ratio (p.apvts, "comp_ratio", "RATIO", Col::charcoal, "1:1", "4:1", "20:1"),
          attack (p.apvts, "comp_att", "ATTACK", Col::slate, "0.1", "8", "100"),
          release (p.apvts, "comp_rel", "RELEASE", Col::slate, "10", "150", "1s"),
          makeup (p.apvts, "comp_makeup", "MAKEUP", Col::red, "0", "12", "24")
    {
        for (auto* k : { &thr, &ratio, &attack, &release, &makeup }) add (*k);
        setupBox (type, TopVocalChainProcessor::compTypeNames(), "comp_type");
    }
    int status() const override { return param ("comp_on") ? (proc.meters.compGrDb.load() < -12.0f ? 2 : 1) : 0; }
    void layout() override
    {
        type.setBounds (14, 46, 186, 24);
        thr.setBounds (22, 80, 170, 168);
        ratio.setBounds (37, 252, 140, 140);
        attack.setBounds (4, 400, 104, 116);
        release.setBounds (106, 400, 104, 116);
        makeup.setBounds (14, 536, 120, 132);
    }
    void paintStatic (juce::Graphics& g) override
    {
        auto cpan = juce::Rectangle<float> (8, 528, 198, 150);
        g.setColour (Col::sand); g.fillRoundedRectangle (cpan, 4);
        g.setColour (juce::Colours::black.withAlpha (0.15f)); g.drawRoundedRectangle (cpan, 4, 1.0f);
    }
    void paintDynamic (juce::Graphics& g) override
    {
        auto r = grBox.toFloat();
        lcd (g, r);
        lcdText (g, "GR", r.withHeight (24), 11.0f, true, juce::Justification::centred, Col::amber.withAlpha (0.7f));
        lcdText (g, juce::String (proc.meters.compGrDb.load(), 1), r.withTrimmedTop (30).withHeight (30), 17.0f, true, juce::Justification::centred);
        lcdText (g, "dB", r.withTrimmedTop (62).withHeight (20), 10.5f, false, juce::Justification::centred, Col::amber.withAlpha (0.7f));
        g.setFont (font (12.0f, true));
        g.setColour (Col::ink);
        g.drawText ("MODELO", info.withHeight (16), juce::Justification::topLeft);
        g.setFont (font (11.5f));
        g.setColour (Col::inkDim);
        g.drawFittedText (U (compText[juce::jlimit (0, 3, choice ("comp_type"))]), info.withTrimmedTop (18), juce::Justification::topLeft, 5);
    }
    void tick() override { Module::tick(); repaint (grBox.expanded (4)); repaint (info); }
};

//------------------------------------------------------------------------------
struct EqModule : Module
{
    Knob air, presence, midFreq, midGain, midQ, low, lowCut;
    juce::ComboBox type;
    juce::Rectangle<int> curve { 12, 644, 208, 104 }, info { 12, 754, 208, 56 };

    EqModule (TopVocalChainProcessor& p)
        : Module (p, "EQUALIZER", "eq_on", 113),
          air (p.apvts, "eq_air_gain", "AIR", Col::green, "-12", "0", "+12"),
          presence (p.apvts, "eq_pres_gain", "PRESENCE", Col::green, "-12", "0", "+12"),
          midFreq (p.apvts, "eq_mid_freq", "MID FREQ", Col::blue, "200", "1k", "5k"),
          midGain (p.apvts, "eq_mid_gain", "MID", Col::blue, "-12", "0", "+12"),
          midQ (p.apvts, "eq_mid_q", "MID Q", Col::charcoal, "0.3", "1", "5"),
          low (p.apvts, "eq_low_gain", "LOW", Col::orange, "-12", "0", "+12"),
          lowCut (p.apvts, "eq_lowcut", "LOW CUT", Col::orange, "20", "100", "500")
    {
        for (auto* k : { &air, &presence, &midFreq, &midGain, &midQ, &low, &lowCut }) add (*k);
        setupBox (type, TopVocalChainProcessor::eqTypeNames(), "eq_type");
    }
    int status() const override { return param ("eq_on") ? (proc.meters.eqHot.load() ? 2 : 1) : 0; }
    void layout() override
    {
        type.setBounds (14, 46, 204, 26);
        air.setBounds (12, 100, 104, 116);
        presence.setBounds (116, 100, 104, 116);
        midFreq.setBounds (12, 246, 104, 116);
        midGain.setBounds (116, 246, 104, 116);
        midQ.setBounds (64, 366, 104, 116);
        low.setBounds (12, 512, 104, 116);
        lowCut.setBounds (116, 512, 104, 116);
    }
    void paintStatic (juce::Graphics& g) override
    {
        auto band = [&] (int y, int h, juce::Colour c, const char* name)
        {
            auto r = juce::Rectangle<float> (8, (float) y, 216, (float) h);
            g.setColour (c); g.fillRoundedRectangle (r, 4);
            g.setColour (juce::Colours::black.withAlpha (0.15f)); g.drawRoundedRectangle (r, 4, 1.0f);
            g.setFont (font (11.0f, true));
            g.setColour (Col::inkDim);
            g.drawText (name, r.reduced (8, 4).toNearestInt(), juce::Justification::topLeft);
        };
        band (82, 140, Col::sage, "HIGH");
        band (228, 260, Col::panelTint, "MID");
        band (494, 140, Col::sand, "LOW");
    }
    void paintDynamic (juce::Graphics& g) override
    {
        auto r = curve.toFloat();
        lcd (g, r);
        auto plot = r.reduced (6, 6);
        auto xOf = [&] (double f) { return plot.getX() + (float) (std::log10 (f / 20.0) / 3.0) * plot.getWidth(); };
        auto yOf = [&] (float db) { return plot.getCentreY() - juce::jlimit (-15.0f, 15.0f, db) / 15.0f * plot.getHeight() * 0.5f; };
        g.setColour (Col::amber.withAlpha (0.12f));
        for (double f : { 100.0, 1000.0, 10000.0 }) g.fillRect (xOf (f), plot.getY(), 1.0f, plot.getHeight());
        g.fillRect (plot.getX(), yOf (0), plot.getWidth(), 1.0f);
        juce::Path c;
        for (int i = 0; i <= 100; ++i)
        {
            const double f = 20.0 * std::pow (1000.0, i / 100.0);
            const float y = yOf (proc.getEqMagnitudeDb (f));
            if (i == 0) c.startNewSubPath (xOf (f), y); else c.lineTo (xOf (f), y);
        }
        g.setColour (Col::amber.withAlpha (0.25f)); g.strokePath (c, juce::PathStrokeType (4.0f));
        g.setColour (Col::amber); g.strokePath (c, juce::PathStrokeType (1.6f));
        g.setFont (font (9.5f));
        g.setColour (Col::amber.withAlpha (0.5f));
        for (auto [f, t] : { std::pair<double, const char*> { 100.0, "100" }, { 1000.0, "1k" }, { 10000.0, "10k" } })
            g.drawText (t, juce::Rectangle<float> (xOf (f) + 2, plot.getBottom() - 11, 30, 10), juce::Justification::left);
        g.setFont (font (11.5f));
        g.setColour (Col::inkDim);
        g.drawFittedText (U (eqText[juce::jlimit (0, 3, choice ("eq_type"))]), info, juce::Justification::topLeft, 3);
    }
    void tick() override { Module::tick(); repaint (curve.expanded (4)); repaint (info); }
};

//------------------------------------------------------------------------------
struct DelayModule : Module
{
    Knob time, feedback, mix, hiCut;
    LitButton sync { "SYNC", Col::amber }, ping { "PING PONG", Col::amber };
    juce::ComboBox div;
    juce::Rectangle<int> disp { 14, 362, 186, 76 };

    DelayModule (TopVocalChainProcessor& p)
        : Module (p, "DELAY", "dly_on", 130),
          time (p.apvts, "dly_time", "TIME", Col::mustard, "10", "", "2s"),
          feedback (p.apvts, "dly_fb", "FEEDBACK", Col::mustard, "0", "", "95"),
          mix (p.apvts, "dly_mix", "MIX", Col::charcoal, "0", "", "100"),
          hiCut (p.apvts, "dly_hicut", "HI CUT", Col::slate, "1k", "5k", "20k")
    {
        for (auto* k : { &time, &feedback, &mix, &hiCut }) add (*k);
        sync.setClickingTogglesState (true); ping.setClickingTogglesState (true);
        attach (sync, "dly_sync");
        attach (ping, "dly_pingpong");
        setupBox (div, TopVocalChainProcessor::divisionNames(), "dly_div");
    }
    void layout() override
    {
        time.setBounds (4, 44, 104, 116);
        feedback.setBounds (106, 44, 104, 116);
        mix.setBounds (4, 162, 104, 116);
        hiCut.setBounds (106, 162, 104, 116);
        sync.setBounds (14, 290, 84, 26);
        div.setBounds (106, 290, 94, 26);
        ping.setBounds (14, 324, 186, 26);
    }
    void paintDynamic (juce::Graphics& g) override
    {
        auto r = disp.toFloat();
        lcd (g, r);
        const auto t = param ("dly_sync") ? TopVocalChainProcessor::divisionNames()[choice ("dly_div")] + "  SYNC"
                                          : juce::String (juce::roundToInt (value ("dly_time"))) + " ms";
        lcdText (g, t, r.withTrimmedTop (8).withHeight (28), 20.0f, true, juce::Justification::centred);
        lcdText (g, param ("dly_pingpong") ? "PING PONG" : U ("ESTÉREO"), r.withTrimmedTop (42).withHeight (20), 11.5f, false,
                 juce::Justification::centred, Col::amber.withAlpha (0.7f));
    }
    void tick() override { Module::tick(); repaint (disp.expanded (4)); }
};

//------------------------------------------------------------------------------
struct ReverbModule : Module
{
    Knob size, damp, width, mix;
    ReverbModule (TopVocalChainProcessor& p)
        : Module (p, "REVERB", "rev_on", 147),
          size (p.apvts, "rev_size", "SIZE", Col::blue, "0", "", "100"),
          damp (p.apvts, "rev_damp", "DAMPING", Col::slate, "0", "", "100"),
          width (p.apvts, "rev_width", "WIDTH", Col::slate, "0", "", "100"),
          mix (p.apvts, "rev_mix", "MIX", Col::charcoal, "0", "", "100")
    {
        for (auto* k : { &size, &damp, &width, &mix }) add (*k);
    }
    void layout() override
    {
        size.setBounds (4, 46, 104, 116);
        damp.setBounds (106, 46, 104, 116);
        width.setBounds (4, 172, 104, 116);
        mix.setBounds (106, 172, 104, 116);
    }
};

//------------------------------------------------------------------------------
struct MetersModule : Module
{
    LedMeter in, out, gr;
    juce::Rectangle<int> top { 12, 46, 176, 48 }, bottom { 12, 694, 176, 120 };
    int clipHold = 0;

    MetersModule (TopVocalChainProcessor& p)
        : Module (p, "METERS", nullptr, 164),
          in ([&p] { return p.meters.inLevelDb.load(); }, false),
          out ([&p] { return p.meters.outLevelDb.load(); }, false),
          gr ([&p] { return p.meters.compGrDb.load(); }, true)
    {
        for (auto* m : { &in, &out, &gr }) add (*m);
    }
    void layout() override
    {
        in.setBounds (34, 104, 30, 560);
        out.setBounds (80, 104, 30, 560);
        gr.setBounds (136, 104, 30, 560);
    }
    void paintStatic (juce::Graphics& g) override
    {
        g.setFont (font (11.0f, true));
        g.setColour (Col::ink);
        const float meterTop = 104.0f, meterH = 554.0f;
        for (int db : { 6, 0, -6, -12, -18, -24, -36, -48 })
        {
            const float y = meterTop + 3 + meterH * (1.0f - LedMeter::levelToProp ((float) db));
            g.drawText ((db > 0 ? "+" : "") + juce::String (db), 6, (int) y - 7, 24, 14, juce::Justification::centredRight);
        }
        for (int v : { 0, 5, 10, 15, 20 })
        {
            const float y = meterTop + 3 + meterH * ((float) v / 20.0f);
            g.drawText (juce::String (v), 170, (int) y - 7, 24, 14, juce::Justification::centredLeft);
        }
        for (auto [x, t] : { std::pair<int, const char*> { 34, "IN" }, { 80, "OUT" }, { 132, "GR" } })
            g.drawText (t, x - 6, 668, 42, 16, juce::Justification::centred);
    }
    void paintDynamic (juce::Graphics& g) override
    {
        const auto& m = proc.meters;
        auto r = top.toFloat();
        lcd (g, r);
        auto left = r.withWidth (r.getWidth() * 0.5f), right = r.withTrimmedLeft (r.getWidth() * 0.5f);
        auto val = [] (float db) { return db < -90.0f ? juce::String ("-inf") : juce::String (db, 1); };
        lcdText (g, "IN", left.withHeight (18), 10.5f, true, juce::Justification::centred, Col::amber.withAlpha (0.7f));
        lcdText (g, val (m.inLevelDb.load()), left.withTrimmedTop (18), 16.0f, true, juce::Justification::centred);
        lcdText (g, clipHold > 0 ? "CLIP" : "OUT", right.withHeight (18), 10.5f, true, juce::Justification::centred,
                 clipHold > 0 ? juce::Colour (0xffff3b30) : Col::amber.withAlpha (0.7f));
        lcdText (g, val (m.outLevelDb.load()), right.withTrimmedTop (18), 16.0f, true, juce::Justification::centred,
                 clipHold > 0 ? juce::Colour (0xffff3b30) : Col::amber);

        auto b = bottom.toFloat();
        lcd (g, b);
        auto t = b.reduced (10, 8);
        lcdText (g, "NOTA", t.removeFromTop (14), 10.5f, false, juce::Justification::centredLeft, Col::amber.withAlpha (0.65f));
        lcdText (g, noteName ((int) m.targetNote.load()), t.removeFromTop (30), 26.0f, true, juce::Justification::centredLeft);
        lcdText (g, U ("ESTÉREO"), t.removeFromTop (14), 10.5f, false, juce::Justification::centredLeft, Col::amber.withAlpha (0.65f));
        lcdText (g, juce::String (juce::roundToInt (juce::jlimit (0.0f, 1.0f, m.stereoWidth.load() * 1.6f) * 100.0f)) + " %",
                 t.removeFromTop (24), 18.0f, true, juce::Justification::centredLeft);
    }
    void tick() override
    {
        Module::tick();
        for (auto* m : { &in, &out, &gr }) m->tick();
        if (proc.meters.outClip.load()) clipHold = 30; else if (clipHold > 0) --clipHold;
        repaint (top.expanded (4));
        repaint (bottom.expanded (4));
    }
};

//------------------------------------------------------------------------------
struct OutputModule : Module
{
    LitButton slotA { "A", Col::amber }, slotB { "B", Col::amber }, compare { "MANTENER: ORIGINAL", Col::amber };
    juce::TextButton copy { "COPIAR" };
    LitButton bypass { "BYPASS", Col::red }, limit { "LIMIT -0.3 dB", Col::amber };
    Knob dryWet;
    Fader fader;
    juce::Rectangle<int> matchBox { 12, 116, 176, 40 };

    OutputModule (TopVocalChainProcessor& p)
        : Module (p, "OUTPUT", nullptr, 181),
          dryWet (p.apvts, "global_mix", "DRY / WET", Col::charcoal, "DRY", "", "WET"),
          fader (p.apvts, "out_gain")
    {
        add (slotA); add (slotB); add (copy); add (compare); add (dryWet); add (fader);
        slotA.onClick = [this] { proc.switchToSlot (0); };
        slotB.onClick = [this] { proc.switchToSlot (1); };
        copy.setTooltip ("Copia los ajustes actuales a la otra ranura");
        copy.onClick = [this] { proc.copyToOtherSlot(); };
        compare.onStateChange = [this]
        {
            const bool down = compare.isDown();
            proc.compareActive.store (down);
            compare.setToggleState (down, juce::dontSendNotification);
        };
        bypass.setClickingTogglesState (true);
        limit.setClickingTogglesState (true);
        attach (bypass, "global_bypass");
        attach (limit, "limit_on");
    }
    void layout() override
    {
        slotA.setBounds (12, 46, 52, 26);
        slotB.setBounds (68, 46, 52, 26);
        copy.setBounds (124, 46, 64, 26);
        compare.setBounds (12, 80, 176, 30);
        bypass.setBounds (12, 166, 84, 28);
        limit.setBounds (100, 166, 88, 28);
        dryWet.setBounds (40, 202, 120, 122);
        fader.setBounds (24, 340, 150, 420);
    }
    void paintStatic (juce::Graphics& g) override
    {
        auto plate = juce::Rectangle<float> (12, 772, 176, 46);
        g.setGradientFill (juce::ColourGradient (Col::header.brighter (0.2f), 0, plate.getY(), Col::header, 0, plate.getBottom(), false));
        g.fillRoundedRectangle (plate, 4);
        drawLogo (g, plate.reduced (8, 6));
    }
    void paintDynamic (juce::Graphics& g) override
    {
        auto r = matchBox.toFloat();
        lcd (g, r);
        const float db = proc.meters.matchGainDb.load();
        const bool cmp = proc.compareActive.load();
        lcdText (g, cmp ? "ESCUCHANDO ORIGINAL" : (getSlot() == 0 ? "AJUSTES A" : "AJUSTES B"), r.withHeight (20).translated (0, 2), 11.0f, true,
                 juce::Justification::centred, cmp ? juce::Colour (0xffff7b3a) : Col::amber);
        lcdText (g, U ("volumen igualado ") + (db >= 0 ? "+" : "") + juce::String (db, 1) + " dB", r.withTrimmedTop (20),
                 10.5f, false, juce::Justification::centred, Col::amber.withAlpha (0.7f));
    }
    int getSlot() const { return proc.getCurrentSlot(); }
    void tick() override
    {
        Module::tick();
        slotA.setToggleState (getSlot() == 0, juce::dontSendNotification);
        slotB.setToggleState (getSlot() == 1, juce::dontSendNotification);
        repaint (matchBox.expanded (4));
    }
};
} // namespace

//==============================================================================
// VISTA PRINCIPAL
//==============================================================================
namespace
{
    struct Column { int width; std::vector<int> ids; };
    const std::vector<Column>& columns()
    {
        static const std::vector<Column> c {
            { 214, { mPre, mAi } }, { 232, { mTune, mWide } }, { 214, { mComp } }, { 232, { mEq } },
            { 214, { mDly, mRev } }, { 200, { mMet } }, { 200, { mOut } } };
        return c;
    }
    const int moduleHeight[numModules] { 420, 404, 404, 420, 830, 830, 460, 364, 830, 830 };
    const char* moduleNames[numModules] { "Preamp", "IA Voice", "Autotune", "Wider", "Compressor", "Equalizer",
                                          "Delay", "Reverb", "Meters", "Output" };
}

MainView::MainView (TopVocalChainProcessor& p) : proc (p)
{
    modules[mPre]  = std::make_unique<PreampModule> (p);
    modules[mAi]   = std::make_unique<AiModule> (p);
    modules[mTune] = std::make_unique<TuneModule> (p);
    modules[mWide] = std::make_unique<WideModule> (p);
    modules[mComp] = std::make_unique<CompModule> (p);
    modules[mEq]   = std::make_unique<EqModule> (p);
    modules[mDly]  = std::make_unique<DelayModule> (p);
    modules[mRev]  = std::make_unique<ReverbModule> (p);
    modules[mMet]  = std::make_unique<MetersModule> (p);
    modules[mOut]  = std::make_unique<OutputModule> (p);
    for (auto& m : modules) addChildComponent (*m);

    for (auto* b : { &modulesBtn, &undoBtn, &redoBtn, &resetBtn, &prevBtn, &nextBtn, &saveBtn, &loadBtn })
        addAndMakeVisible (b);
    addAndMakeVisible (presetBox);

    const char* scales[] { "100%", "125%", "150%", "200%" };
    for (int i = 0; i < 4; ++i)
    {
        auto* b = scaleBtns.add (new LitButton (scales[i], Col::amber));
        b->onClick = [this, i] { proc.uiScaleIndex.store (i); if (onSizeChange) onSizeChange(); };
        addAndMakeVisible (b);
    }

    modulesBtn.onClick = [this] { showModulesMenu(); };
    undoBtn.onClick = [this] { proc.undoManager.undo(); };
    redoBtn.onClick = [this] { proc.undoManager.redo(); };
    resetBtn.onClick = [this] { proc.undoManager.beginNewTransaction(); proc.resetAll(); refreshPresetBox(); };
    undoBtn.setTooltip ("Deshacer"); redoBtn.setTooltip ("Rehacer"); resetBtn.setTooltip ("Volver a los ajustes por defecto");

    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setTextWhenNothingSelected ("Preset");
    refreshPresetBox();
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id > 0) { proc.undoManager.beginNewTransaction(); proc.setCurrentProgram (id - 1); }
    };
    auto step = [this] (int d)
    {
        const int n = proc.getNumPrograms();
        proc.undoManager.beginNewTransaction();
        proc.setCurrentProgram ((proc.getCurrentProgram() + d + n) % n);
        refreshPresetBox();
    };
    prevBtn.onClick = [step] { step (-1); };
    nextBtn.onClick = [step] { step (1); };
    saveBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Guardar preset", TopVocalChainProcessor::getUserPresetFolder(), "*.tvcpreset");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f != juce::File() && proc.saveUserPreset (f))
                                      presetBox.setText (f.getFileNameWithoutExtension(), juce::dontSendNotification);
                              });
    };
    loadBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Cargar preset", TopVocalChainProcessor::getUserPresetFolder(), "*.tvcpreset");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f.existsAsFile() && proc.loadUserPreset (f))
                                      presetBox.setText (f.getFileNameWithoutExtension(), juce::dontSendNotification);
                              });
    };
}

MainView::~MainView() = default;

int MainView::getCanvasWidth() const
{
    int w = 12;
    for (const auto& col : columns())
    {
        bool any = false;
        for (int id : col.ids) any |= ! isHidden (id) || id == mOut;
        if (any) w += col.width + 6;
    }
    return juce::jmax (minW, w + 6);
}

void MainView::resized()
{
    // Barra superior
    modulesBtn.setBounds (360, 10, 104, 28);
    for (int i = 0; i < scaleBtns.size(); ++i) scaleBtns[i]->setBounds (472 + i * 50, 10, 48, 28);
    undoBtn.setBounds (682, 10, 60, 28);
    redoBtn.setBounds (746, 10, 60, 28);
    resetBtn.setBounds (810, 10, 66, 28);
    const int right = getWidth() - 12;
    loadBtn.setBounds (right - 96, 10, 96, 28);
    saveBtn.setBounds (right - 196, 10, 96, 28);
    nextBtn.setBounds (right - 232, 10, 30, 28);
    presetBox.setBounds (right - 462, 10, 226, 28);
    prevBtn.setBounds (right - 496, 10, 30, 28);
    // Si no cabe todo, los presets ocupan el espacio de la derecha de RESET
    if (prevBtn.getX() < 884)
    {
        const int shift = 884 - prevBtn.getX();
        for (auto* c : std::initializer_list<juce::Component*> { &prevBtn, &presetBox, &nextBtn, &saveBtn, &loadBtn })
            c->setBounds (c->getBounds().translated (shift, 0));
    }

    // Columnas de modulos (los ocultos no ocupan espacio)
    int used = 12;
    for (const auto& col : columns())
    {
        bool any = false;
        for (int id : col.ids) any |= ! isHidden (id) || id == mOut;
        if (any) used += col.width + 6;
    }
    int x = juce::jmax (12, (getWidth() - (used - 6) - 12) / 2 + 12);
    for (const auto& col : columns())
    {
        int y = 56;
        bool any = false;
        for (int id : col.ids)
        {
            const bool show = ! isHidden (id) || id == mOut;
            modules[(size_t) id]->setVisible (show);
            if (! show) continue;
            any = true;
            modules[(size_t) id]->setBounds (x, y, col.width, moduleHeight[id]);
            y += moduleHeight[id] + 6;
        }
        if (any) x += col.width + 6;
    }
}

void MainView::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1d1f23), 0, 0, Col::backdrop, 0, (float) H, false));
    g.fillAll();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c2e33), 0, 0, juce::Colour (0xff17181b), 0, 48, false));
    g.fillRect (0, 0, getWidth(), 48);
    g.setColour (juce::Colours::black); g.fillRect (0, 47, getWidth(), 2);
    drawLogo (g, { 14, 6, 330, 36 });
}

void MainView::showModulesMenu()
{
    juce::PopupMenu menu;
    for (int id = 0; id < numModules; ++id)
        if (id != mOut) menu.addItem (id + 1, moduleNames[id], true, ! isHidden (id));
    menu.addSeparator();
    menu.addItem (100, "Mostrar todos");
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&modulesBtn),
                        [this] (int r)
                        {
                            if (r == 0) return;
                            if (r == 100) proc.hiddenModules.store (0);
                            else proc.hiddenModules.store (proc.hiddenModules.load() ^ (1 << (r - 1)));
                            if (onSizeChange) onSizeChange();
                        });
}

void MainView::refreshPresetBox()
{
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
}

void MainView::tick()
{
    for (auto& m : modules) if (m->isVisible()) m->tick();
    for (int i = 0; i < scaleBtns.size(); ++i)
        scaleBtns[i]->setToggleState (i == proc.uiScaleIndex.load(), juce::dontSendNotification);

    // Deshacer: agrupa cada gesto (soltar el raton) en un paso
    if (proc.undoManager.getNumActionsInCurrentTransaction() > 0 && ! juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown())
        proc.undoManager.beginNewTransaction();
    undoBtn.setEnabled (proc.undoManager.canUndo());
    redoBtn.setEnabled (proc.undoManager.canRedo());

    if (presetBox.getSelectedId() > 0 && presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
        refreshPresetBox();
}

//==============================================================================
// EDITOR
//==============================================================================
float TopVocalChainEditor::scaleFactor (int index)
{
    static const float f[] { 0.75f, 0.9375f, 1.125f, 1.5f };   // 100 %, 125 %, 150 %, 200 %
    return f[juce::jlimit (0, 3, index)];
}

TopVocalChainEditor::TopVocalChainEditor (TopVocalChainProcessor& p)
    : AudioProcessorEditor (&p), proc (p), view (p)
{
    view.setLookAndFeel (&lnf);
    addAndMakeVisible (view);
    view.onSizeChange = [this] { updateSize(); };
    setResizable (false, false);
    updateSize();
    startTimerHz (30);
}

TopVocalChainEditor::~TopVocalChainEditor()
{
    stopTimer();
    view.setLookAndFeel (nullptr);
}

void TopVocalChainEditor::updateSize()
{
    const float s = scaleFactor (proc.uiScaleIndex.load());
    const int w = view.getCanvasWidth();
    view.setTransform ({});
    view.setBounds (0, 0, w, tvcui::MainView::H);
    view.setTransform (juce::AffineTransform::scale (s));
    setSize (juce::roundToInt ((float) w * s), juce::roundToInt ((float) tvcui::MainView::H * s));
    view.resized();
}

void TopVocalChainEditor::paint (juce::Graphics& g) { g.fillAll (cui::Col::backdrop); }

void TopVocalChainEditor::resized() {}

void TopVocalChainEditor::timerCallback() { view.tick(); }
