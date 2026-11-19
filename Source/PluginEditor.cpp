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
const char* preText[]  { "Clean: transparente, solo ganancia", "Tube: válvula cálida, armónicos pares",
                         "Vintage: saturación suave, cuerpo y agudos redondos", "Modern: brillante y limpio, graves firmes",
                         "Tape: compresión y saturación de cinta", "Transformer: color de hierro en los graves",
                         "Transistor: filo y mordida", "Diode: agresivo y asimétrico" };
const char* compText[] { "Clean VCA: control preciso, respeta Attack y Release.",
                         "Vintage Opto: lento y musical. Cuanto más comprime, más lento suelta.",
                         "FET Punch: atrapa los picos en ~1 ms. Agresivo y con presencia.",
                         "Vari-Mu Glue: el ratio sube con el nivel. Suave y pegado.",
                         "LA Vocal: opto más rápido y denso. Voz al frente.",
                         "Fast: ataque y release rapidísimos. Controla picos.",
                         "Smooth: detección lenta, compresión muy suave." };
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
        g.drawFittedText (U (preText[juce::jlimit (0, 7, choice ("pre_type"))]), { 14, 73, 186, 14 }, juce::Justification::centred, 1);
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
    LitButton analyse { "ANALIZAR VOZ", Col::amber };
    juce::TextButton reset { "RESET" }, report { "VER INFORME" };
    juce::Rectangle<int> disp { 12, 230, 190, 164 };
    std::function<void()> onShowReport;

    AiModule (TopVocalChainProcessor& p)
        : Module (p, "IA VOICE", nullptr, 45),
          amount (p.apvts, "ai_amount", "INTELIGENCIA", Col::purple, "0", "50", "100")
    {
        add (amount);
        listen.setClickingTogglesState (true);
        listen.setTooltip (U ("Análisis continuo: la perilla INTELIGENCIA decide cuánto corrige"));
        attach (listen, "ai_listen");
        analyse.setTooltip (U ("Canta ~8 segundos: detecta registro, tonalidad, resonancias, sibilancia y más"));
        analyse.onClick = [this]
        {
            if (proc.analysis.isActive()) proc.analysis.cancel();
            else proc.startAnalysis();
        };
        add (analyse);
        reset.onClick = [this] { proc.requestAIReset(); };
        add (reset);
        report.onClick = [this] { if (onShowReport) onShowReport(); };
        add (report);
    }
    void layout() override
    {
        listen.setBounds (getWidth() - 96, 7, 88, 22);
        amount.setBounds (37, 40, 140, 124);
        analyse.setBounds (12, 168, 124, 26);
        reset.setBounds (140, 168, 62, 26);
        report.setBounds (12, 198, 190, 26);
    }
    void paintDynamic (juce::Graphics& g) override
    {
        const auto& m = proc.meters;
        auto r = disp.toFloat();
        lcd (g, r);
        auto t = r.reduced (8, 7);
        if (proc.analysis.isActive())
        {
            lcdText (g, "AI VOICE ANALYSIS", t.removeFromTop (16), 12.0f, true, juce::Justification::centredLeft);
            t.removeFromTop (10);
            lcdText (g, U ("Canta normalmente..."), t.removeFromTop (16), 11.5f, false, juce::Justification::centredLeft, Col::amber.withAlpha (0.8f));
            t.removeFromTop (8);
            auto bar = t.removeFromTop (12);
            g.setColour (Col::amber.withAlpha (0.15f)); g.fillRect (bar);
            g.setColour (Col::amber); g.fillRect (bar.withWidth (bar.getWidth() * proc.analysis.progress()));
            t.removeFromTop (6);
            lcdText (g, "ANALIZANDO " + juce::String (juce::roundToInt (proc.analysis.progress() * 100)) + " %", t.removeFromTop (18), 14.0f, true,
                     juce::Justification::centredLeft);
            return;
        }
        lcdText (g, param ("ai_listen") ? "ESCUCHANDO" : "CONGELADO", t.removeFromTop (15), 11.5f, true, juce::Justification::centredLeft);
        auto cb = t.removeFromTop (5);
        g.setColour (Col::amber.withAlpha (0.15f)); g.fillRect (cb);
        g.setColour (Col::amber); g.fillRect (cb.withWidth (cb.getWidth() * m.aiConfidence.load()));
        t.removeFromTop (4);
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
    void tick() override
    {
        Module::tick();
        const bool active = proc.analysis.isActive();
        analyse.setButtonText (active ? "CANCELAR" : "ANALIZAR VOZ");
        analyse.setToggleState (active, juce::dontSendNotification);
        report.setEnabled (proc.analysis.isReady());
        repaint (disp.expanded (4));
    }
};

//------------------------------------------------------------------------------
struct TuneModule : Module
{
    Knob speed, amount, humanize, flex, formant, transpose, vibrato, tracking;
    juce::ComboBox key, scale;
    LitButton natural { "NATURAL", Col::amber }, robotic { "ROBOTIC", Col::amber }, autoKey { "AUTO KEY", Col::amber },
              lowLat { "LOW LAT", Col::amber };
    juce::Rectangle<int> disp { 12, 46, 226, 170 }, keyInfo { 116, 254, 122, 26 };
    juce::String keyText;

    TuneModule (TopVocalChainProcessor& p)
        : Module (p, "AUTOTUNE", "tune_on", 62),
          speed (p.apvts, "tune_speed", "RETUNE SPEED", Col::green, "0", "60", "400"),
          amount (p.apvts, "tune_amount", "AMOUNT", Col::green, "0", "", "100"),
          humanize (p.apvts, "tune_humanize", "HUMANIZE", Col::slate, "0", "", "100"),
          flex (p.apvts, "tune_flex", "FLEX-TUNE", Col::slate, "0", "", "100"),
          formant (p.apvts, "tune_formant", "FORMANT", Col::purple, "-5", "0", "+5"),
          transpose (p.apvts, "tune_transpose", "TRANSPOSE", Col::purple, "-12", "0", "+12"),
          vibrato (p.apvts, "tune_vibrato", "VIBRATO", Col::orange, "0", "", "100c"),
          tracking (p.apvts, "tune_tracking", "TRACKING", Col::charcoal, "STRICT", "", "RELAX")
    {
        for (auto* k : { &speed, &amount, &humanize, &flex, &formant, &transpose, &vibrato, &tracking }) add (*k);
        setupBox (key, TopVocalChainProcessor::keyNames(), "tune_key");
        setupBox (scale, TopVocalChainProcessor::scaleNames(), "tune_scale");
        auto setMode = [this] (int m) { auto* prm = proc.apvts.getParameter ("tune_mode"); prm->setValueNotifyingHost (prm->convertTo0to1 ((float) m)); };
        natural.onClick = [setMode] { setMode (0); };
        robotic.onClick = [setMode] { setMode (1); };
        add (natural); add (robotic);
        autoKey.setClickingTogglesState (true);
        autoKey.onClick = [this]
        {
            if (autoKey.getToggleState()) { proc.startKeyDetection(); keyText = U ("escuchando..."); }
            else proc.meters.keyListening.store (false);
            repaint (keyInfo);
        };
        add (autoKey);
        lowLat.setClickingTogglesState (true);
        lowLat.setTooltip ("Baja latencia (~14 ms) para grabar escuchándote. Sin control de Formant.");
        attach (lowLat, "tune_lowlat");
    }
    void layout() override
    {
        lowLat.setBounds (getWidth() - 146, 7, 80, 22);
        key.setBounds (12, 224, 70, 24);
        scale.setBounds (88, 224, 150, 24);
        autoKey.setBounds (12, 254, 100, 26);
        natural.setBounds (12, 286, 110, 26);
        robotic.setBounds (128, 286, 110, 26);
        Knob* ks[] { &speed, &amount, &humanize, &flex, &formant, &transpose, &vibrato, &tracking };
        for (int i = 0; i < 8; ++i) ks[i]->setBounds (i % 2 == 0 ? 6 : 125, 318 + (i / 2) * 122, 119, 120);
    }
    void paintDynamic (juce::Graphics& g) override
    {
        const auto& m = proc.meters;
        auto r = disp.toFloat();
        lcd (g, r);
        const bool voiced = m.voiced.load();
        const int target = (int) m.targetNote.load();
        const float sung = m.sungMidi.load();
        const float dev = voiced && target >= 0 ? (sung - (float) target) * 100.0f : 0.0f;
        const float corr = m.correctionCents.load();

        // Nota objetivo (grande) y nota cantada
        lcdText (g, U ("OBJETIVO"), { r.getX() + 12, r.getY() + 8, 100, 12 }, 10.0f, false, juce::Justification::centredLeft, Col::amber.withAlpha (0.6f));
        lcdText (g, voiced ? noteName (target) : juce::String ("--"), { r.getX() + 10, r.getY() + 20, 110, 46 }, 40.0f, true,
                 juce::Justification::centredLeft, voiced ? Col::amber : Col::amber.withAlpha (0.35f));
        lcdText (g, U ("CANTADA"), { r.getX() + 124, r.getY() + 8, 90, 12 }, 10.0f, false, juce::Justification::centredRight, Col::amber.withAlpha (0.6f));
        lcdText (g, voiced ? noteName ((int) std::round (sung)) : juce::String ("--"), { r.getX() + 124, r.getY() + 20, 90, 24 }, 20.0f, true,
                 juce::Justification::centredRight, Col::amber.withAlpha (0.85f));
        lcdText (g, voiced ? juce::String (m.pitchHz.load(), 1) + " Hz" : juce::String ("--- Hz"), { r.getX() + 124, r.getY() + 44, 90, 16 }, 12.0f, false,
                 juce::Justification::centredRight, Col::amber.withAlpha (0.75f));

        // Indicadores: entrada de voz y afinacion
        auto led = [&] (float x, float y, juce::Colour c, bool lit, const juce::String& label)
        {
            g.setColour (lit ? c : c.withAlpha (0.15f));
            g.fillEllipse (x, y, 9, 9);
            if (lit) { g.setColour (c.withAlpha (0.3f)); g.fillEllipse (x - 3, y - 3, 15, 15); }
            lcdText (g, label, { x + 13, y - 3, 90, 15 }, 10.5f, true, juce::Justification::centredLeft, lit ? c : Col::amber.withAlpha (0.4f));
        };
        const bool inTune = voiced && std::abs (dev) < 10.0f;
        led (r.getX() + 14, r.getY() + 74, Col::ledGreen, voiced, "VOZ");
        led (r.getX() + 70, r.getY() + 74, inTune ? Col::ledGreen : Col::amber, voiced, inTune ? "AFINADO" : "CORRIGIENDO");
        lcdText (g, voiced ? juce::String ((dev >= 0 ? "+" : "") + juce::String (juce::roundToInt (dev)) + " c") : juce::String(),
                 { r.getX() + 160, r.getY() + 70, 54, 16 }, 13.0f, true, juce::Justification::centredRight);

        // Escala de afinacion -50..+50 cents: triangulo = voz cantada, barra = correccion aplicada
        auto sc = juce::Rectangle<float> (r.getX() + 14, r.getY() + 104, r.getWidth() - 28, 26);
        g.setColour (Col::amber.withAlpha (0.1f)); g.fillRect (sc);
        for (int c = -50; c <= 50; c += 10)
        {
            const float x = sc.getCentreX() + (float) c / 50.0f * sc.getWidth() * 0.5f;
            g.setColour (Col::amber.withAlpha (c == 0 ? 0.8f : 0.3f));
            g.fillRect (x - 0.5f, sc.getY(), 1.0f, c == 0 ? sc.getHeight() : 6.0f);
        }
        if (voiced)
        {
            const float x = sc.getCentreX() + juce::jlimit (-50.0f, 50.0f, dev) / 50.0f * sc.getWidth() * 0.5f;
            juce::Path tri; tri.addTriangle (x - 6, sc.getBottom(), x + 6, sc.getBottom(), x, sc.getBottom() - 12);
            g.setColour (inTune ? Col::ledGreen : Col::amber); g.fillPath (tri);
        }
        auto bar = juce::Rectangle<float> (r.getX() + 14, r.getY() + 140, r.getWidth() - 28, 8);
        g.setColour (Col::amber.withAlpha (0.12f)); g.fillRect (bar);
        const float c = juce::jlimit (-100.0f, 100.0f, corr) / 100.0f;
        const float mid = bar.getCentreX(), x2 = mid + c * bar.getWidth() * 0.5f;
        g.setColour (Col::amber); g.fillRect (juce::jmin (mid, x2), bar.getY(), std::abs (x2 - mid) + 1.5f, bar.getHeight());
        lcdText (g, U ("CORRECCIÓN ") + juce::String (juce::roundToInt (corr)) + " c", { r.getX() + 14, r.getY() + 150, 180, 14 }, 10.0f, false,
                 juce::Justification::centredLeft, Col::amber.withAlpha (0.6f));

        // Resultado de AUTO KEY
        g.setFont (font (11.5f, true));
        g.setColour (Col::ink);
        g.drawFittedText (keyText, keyInfo, juce::Justification::centredLeft, 2);
    }
    void tick() override
    {
        Module::tick();
        const int mode = choice ("tune_mode");
        natural.setToggleState (mode == 0, juce::dontSendNotification);
        robotic.setToggleState (mode == 1, juce::dontSendNotification);
        for (auto* k : { &speed, &amount, &humanize, &flex }) k->setAlpha (mode == 1 ? 0.45f : 1.0f);   // Robotic los ignora
        formant.setAlpha (param ("tune_lowlat") ? 0.45f : 1.0f);

        // AUTO KEY: cuando hay suficiente voz analizada, aplica la tonalidad detectada
        if (autoKey.getToggleState() && proc.meters.keyListening.load())
        {
            float h[12]; float total = 0;
            for (int i = 0; i < 12; ++i) { h[i] = proc.meters.keyHist[i].load(); total += h[i]; }
            if (total > 4.0f)
            {
                int root; bool minor; float conf;
                toptune::KeyFinder::find (h, root, minor, conf);
                if (conf > 0.55f || total > 12.0f)
                {
                    auto* k = proc.apvts.getParameter ("tune_key");
                    auto* s = proc.apvts.getParameter ("tune_scale");
                    k->setValueNotifyingHost (k->convertTo0to1 ((float) root));
                    s->setValueNotifyingHost (s->convertTo0to1 (minor ? 2.0f : 1.0f));
                    proc.meters.keyListening.store (false);
                    autoKey.setToggleState (false, juce::dontSendNotification);
                    keyText = TopVocalChainProcessor::keyNames()[root] + (minor ? " menor" : " mayor") + " (" + juce::String (juce::roundToInt (conf * 100)) + "%)";
                }
                else keyText = U ("escuchando... ") + juce::String (juce::roundToInt (total)) + " s";
                repaint (keyInfo);
            }
        }
        repaint (disp.expanded (4));
    }
};

//------------------------------------------------------------------------------
struct WideModule : Module
{
    Knob amount, width, mix;
    juce::OwnedArray<LitButton> modes;
    LitButton mono { "MONO", Col::amber }, safe { "SAFE VOCAL", Col::ledGreen };
    juce::Rectangle<int> disp { 12, 334, 190, 56 };

    WideModule (TopVocalChainProcessor& p)
        : Module (p, "STEREO LAB", "wide_on", 79),
          amount (p.apvts, "wide_amount", "EFECTO", Col::purple, "0", "", "100"),
          width (p.apvts, "wide_width", "WIDTH", Col::purple, "0", "100", "200"),
          mix (p.apvts, "wide_mix", "MIX", Col::charcoal, "DRY", "", "WET")
    {
        add (amount); add (width); add (mix);
        const auto& names = TopVocalChainProcessor::wideModeNames();
        for (int i = 0; i < names.size(); ++i)
        {
            auto* b = modes.add (new LitButton (names[i].toUpperCase(), Col::amber));
            b->setTooltip (U (wideText[i]));
            b->onClick = [this, i]
            {
                auto* prm = proc.apvts.getParameter ("wide_mode");
                prm->setValueNotifyingHost (prm->convertTo0to1 ((float) i));
            };
            add (*b);
        }
        mono.setClickingTogglesState (true);
        safe.setClickingTogglesState (true);
        safe.setTooltip (U ("Modo seguro para voces: graves siempre en mono, ancho y efecto limitados"));
        attach (mono, "wide_mono");
        attach (safe, "wide_safe");
    }
    void layout() override
    {
        for (int i = 0; i < modes.size(); ++i)
        {
            const int row = i < 3 ? 0 : 1;
            modes[i]->setBounds (row == 0 ? 8 + i * 66 : 8 + (i - 3) * 99, 44 + row * 30, row == 0 ? 62 : 95, 26);
        }
        amount.setBounds (4, 106, 104, 112);
        width.setBounds (106, 106, 104, 112);
        mix.setBounds (4, 218, 104, 112);
        mono.setBounds (116, 236, 88, 26);
        safe.setBounds (116, 268, 88, 26);
    }
    void paintDynamic (juce::Graphics& g) override
    {
        auto r = disp.toFloat();
        lcd (g, r);
        auto t = r.reduced (9, 6);
        const float corr = proc.meters.labCorr.load();
        const bool problem = corr < 0.0f;
        const juce::Colour warn (0xffff5a3c);
        lcdText (g, U ("CORRELACIÓN ") + (corr >= 0 ? "+" : "") + juce::String (corr, 2), t.removeFromTop (14), 11.0f, true, juce::Justification::centredLeft);
        lcdText (g, param ("wide_mono") ? "MONO" : problem ? U ("PROBLEMA DE FASE") : U ("FASE OK"), t.withHeight (14).translated (0, -14), 11.0f, true,
                 juce::Justification::centredRight, problem ? warn : Col::ledGreen);
        t.removeFromTop (6);
        auto bar = t.removeFromTop (12);
        g.setColour (Col::amber.withAlpha (0.12f)); g.fillRect (bar);
        g.setColour (warn.withAlpha (0.25f)); g.fillRect (bar.withWidth (bar.getWidth() * 0.5f));
        const float x = bar.getCentreX() + juce::jlimit (-1.0f, 1.0f, corr) * bar.getWidth() * 0.5f;
        g.setColour (problem ? warn : Col::amber); g.fillRect (x - 2.0f, bar.getY() - 2.0f, 4.0f, bar.getHeight() + 4.0f);
        g.setFont (font (9.0f)); g.setColour (Col::amber.withAlpha (0.5f));
        g.drawText ("-1", bar.translated (0, 12).withHeight (10), juce::Justification::left);
        g.drawText ("0", bar.translated (0, 12).withHeight (10), juce::Justification::centred);
        g.drawText ("+1", bar.translated (0, 12).withHeight (10), juce::Justification::right);
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
    Knob thr, ratio, attack, release, knee, makeup, mix, hpf;
    juce::ComboBox type;
    LitButton scExt { "SIDECHAIN EXT", Col::amber };
    LedMeter inM, grM, outM;
    juce::Rectangle<int> curve { 12, 78, 148, 150 }, info { 14, 756, 202, 62 };

    CompModule (TopVocalChainProcessor& p)
        : Module (p, "COMPRESSOR", "comp_on", 96),
          thr (p.apvts, "comp_thr", "THRESHOLD", Col::charcoal, "-60", "-30", "0"),
          ratio (p.apvts, "comp_ratio", "RATIO", Col::charcoal, "1:1", "4:1", "20:1"),
          attack (p.apvts, "comp_att", "ATTACK", Col::slate, "0.1", "8", "100"),
          release (p.apvts, "comp_rel", "RELEASE", Col::slate, "10", "150", "1s"),
          knee (p.apvts, "comp_knee", "KNEE", Col::slate, "HARD", "", "SOFT"),
          makeup (p.apvts, "comp_makeup", "MAKEUP", Col::red, "0", "12", "24"),
          mix (p.apvts, "comp_mix", "MIX", Col::orange, "DRY", "", "WET"),
          hpf (p.apvts, "comp_sc_hpf", "DET. HPF", Col::green, "OFF", "", "500"),
          inM ([&p] { return p.meters.compInDb.load(); }, false),
          grM ([&p] { return p.meters.compGrDb.load(); }, true),
          outM ([&p] { return p.meters.compOutDb.load(); }, false)
    {
        for (auto* k : { &thr, &ratio, &attack, &release, &knee, &makeup, &mix, &hpf }) add (*k);
        for (auto* mm : { &inM, &grM, &outM }) add (*mm);
        setupBox (type, TopVocalChainProcessor::compTypeNames(), "comp_type");
        scExt.setClickingTogglesState (true);
        scExt.setTooltip ("Usa la entrada de sidechain del DAW como detector (ducking, de-essing externo)");
        attach (scExt, "comp_sc_ext");
    }
    int status() const override { return param ("comp_on") ? (proc.meters.compGrDb.load() < -12.0f ? 2 : 1) : 0; }
    void layout() override
    {
        type.setBounds (14, 46, 202, 24);
        inM.setBounds (166, 78, 16, 150);
        grM.setBounds (184, 78, 16, 150);
        outM.setBounds (202, 78, 16, 150);
        Knob* ks[] { &thr, &ratio, &attack, &release, &knee, &makeup, &mix, &hpf };
        for (int i = 0; i < 8; ++i) ks[i]->setBounds (i % 2 == 0 ? 4 : 116, 246 + (i / 2) * 118, 110, 116);
        scExt.setBounds (14, 720, 202, 26);
    }
    void paintStatic (juce::Graphics& g) override
    {
        g.setFont (font (9.0f, true));
        g.setColour (Col::ink);
        for (auto [x, t] : { std::pair<int, const char*> { 166, "IN" }, { 184, "GR" }, { 202, "OUT" } })
            g.drawText (t, x - 4, 230, 24, 12, juce::Justification::centred);
    }
    void paintDynamic (juce::Graphics& g) override
    {
        // Curva de compresion: entrada (-60..0 dB) -> salida, con el nivel actual
        auto r = curve.toFloat();
        lcd (g, r);
        auto plot = r.reduced (8);
        auto px = [&] (float db) { return plot.getX() + (db + 60.0f) / 60.0f * plot.getWidth(); };
        auto py = [&] (float db) { return plot.getBottom() - (juce::jlimit (-60.0f, 0.0f, db) + 60.0f) / 60.0f * plot.getHeight(); };
        g.setColour (Col::amber.withAlpha (0.12f));
        for (int db = -48; db < 0; db += 12) { g.fillRect (px ((float) db), plot.getY(), 1.0f, plot.getHeight()); g.fillRect (plot.getX(), py ((float) db), plot.getWidth(), 1.0f); }
        g.setColour (Col::amber.withAlpha (0.3f));
        g.drawLine (px (-60), py (-60), px (0), py (0), 1.0f);
        juce::Path cpath;
        for (int i = 0; i <= 60; ++i)
        {
            const float in = -60.0f + (float) i;
            const float out = in + proc.getCompStaticGr (in);
            if (i == 0) cpath.startNewSubPath (px (in), py (out)); else cpath.lineTo (px (in), py (out));
        }
        g.setColour (Col::amber); g.strokePath (cpath, juce::PathStrokeType (2.0f));
        const float thrDb = value ("comp_thr");
        g.setColour (Col::amber.withAlpha (0.5f));
        g.fillRect (px (thrDb) - 0.5f, plot.getY(), 1.0f, plot.getHeight());
        const float in = proc.meters.compInDb.load();
        if (in > -60.0f && param ("comp_on"))
        {
            const float out = in + proc.getCompStaticGr (in);
            g.setColour (juce::Colours::white);
            g.fillEllipse (px (in) - 4, py (out) - 4, 8, 8);
        }
        lcdText (g, "THR " + juce::String (juce::roundToInt (thrDb)), { plot.getX(), plot.getY(), 60, 12 }, 9.5f, false, juce::Justification::centredLeft,
                 Col::amber.withAlpha (0.7f));

        g.setFont (font (11.0f));
        g.setColour (Col::inkDim);
        g.drawFittedText (U (compText[juce::jlimit (0, 6, choice ("comp_type"))]), info, juce::Justification::topLeft, 4);
    }
    void tick() override
    {
        Module::tick();
        for (auto* mm : { &inM, &grM, &outM }) mm->tick();
        repaint (curve.expanded (4));
        repaint (info);
    }
};

//------------------------------------------------------------------------------
struct EqGraph : juce::Component
{
    enum Node { nLC, nLow, nMid, nPres, nAir, nHC, numNodes };
    TopVocalChainProcessor& proc;
    int hover = -1, drag = -1;
    juce::Point<float> mouse;

    explicit EqGraph (TopVocalChainProcessor& p) : proc (p) {}

    float v (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }
    void setV (const char* id, float val)
    {
        auto* prm = proc.apvts.getParameter (id);
        prm->setValueNotifyingHost (prm->convertTo0to1 (prm->getNormalisableRange().snapToLegalValue (val)));
    }
    juce::Rectangle<float> plot() const { return getLocalBounds().toFloat().reduced (8, 8); }
    float xOf (double f) const { auto p = plot(); return p.getX() + (float) (std::log10 (f / 20.0) / 3.0) * p.getWidth(); }
    double fOf (float x) const { auto p = plot(); return 20.0 * std::pow (1000.0, juce::jlimit (0.0, 1.0, (double) ((x - p.getX()) / p.getWidth()))); }
    float yOf (float db) const { auto p = plot(); return p.getCentreY() - juce::jlimit (-15.0f, 15.0f, db) / 15.0f * p.getHeight() * 0.5f; }
    float dbOf (float y) const { auto p = plot(); return (p.getCentreY() - y) / (p.getHeight() * 0.5f) * 15.0f; }

    struct Info { const char* name; const char* freqId; const char* gainId; float freq, gain; juce::Colour col; };
    Info info (int n) const
    {
        const int type = (int) v ("eq_type");
        float lowF, presF, airF;
        TopVocalChainProcessor::eqModelFreqs (type, v ("eq_air_gain"), lowF, presF, airF);
        switch (n)
        {
            case nLC:   return { "LOW CUT", "eq_lowcut", nullptr, v ("eq_lowcut"), 0.0f, Col::orange };
            case nLow:  return { "LOW", nullptr, "eq_low_gain", lowF, v ("eq_low_gain"), Col::orange };
            case nMid:  return { "MID", "eq_mid_freq", "eq_mid_gain", v ("eq_mid_freq"), v ("eq_mid_gain"), Col::blue };
            case nPres: return { "PRESENCE", nullptr, "eq_pres_gain", presF, v ("eq_pres_gain"), Col::green };
            case nAir:  return { "AIR", nullptr, "eq_air_gain", airF, v ("eq_air_gain"), Col::green };
            default:    return { "HIGH CUT", "eq_hicut", nullptr, std::min (20000.0f, v ("eq_hicut")), 0.0f, Col::slate };
        }
    }
    juce::Point<float> pos (int n) const { auto i = info (n); return { xOf (i.freq), yOf (i.gain) }; }

    int nodeAt (juce::Point<float> p) const
    {
        int best = -1; float bd = 14.0f;
        for (int n = 0; n < numNodes; ++n) { const float d = pos (n).getDistanceFrom (p); if (d < bd) { bd = d; best = n; } }
        return best;
    }

    static juce::String hz (float f) { return f >= 1000.0f ? juce::String (f / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (f)) + " Hz"; }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        lcd (g, r);
        auto p = plot();
        g.setFont (font (9.5f));
        for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
        {
            g.setColour (Col::amber.withAlpha (0.1f)); g.fillRect (xOf (f), p.getY(), 1.0f, p.getHeight());
            g.setColour (Col::amber.withAlpha (0.45f));
            g.drawText (f >= 1000 ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f), juce::Rectangle<float> (xOf (f) + 2, p.getBottom() - 12, 30, 11), juce::Justification::left);
        }
        for (int db : { -12, -6, 0, 6, 12 })
        {
            g.setColour (Col::amber.withAlpha (db == 0 ? 0.25f : 0.08f)); g.fillRect (p.getX(), yOf ((float) db), p.getWidth(), 1.0f);
            g.setColour (Col::amber.withAlpha (0.45f));
            g.drawText ((db > 0 ? "+" : "") + juce::String (db), juce::Rectangle<float> (p.getX() + 2, yOf ((float) db) - 11, 26, 10), juce::Justification::left);
        }

        // Curva real (incluye modelo, pendientes y las correcciones de la IA)
        juce::Path c, fill;
        for (int i = 0; i <= 160; ++i)
        {
            const double f = 20.0 * std::pow (1000.0, i / 160.0);
            const float y = yOf (proc.getEqMagnitudeDb (f));
            if (i == 0) { c.startNewSubPath (xOf (f), y); fill.startNewSubPath (xOf (f), yOf (0)); fill.lineTo (xOf (f), y); }
            else { c.lineTo (xOf (f), y); fill.lineTo (xOf (f), y); }
        }
        fill.lineTo (p.getRight(), yOf (0)); fill.closeSubPath();
        g.setColour (Col::amber.withAlpha (0.12f)); g.fillPath (fill);
        g.setColour (Col::amber.withAlpha (0.25f)); g.strokePath (c, juce::PathStrokeType (5.0f));
        g.setColour (Col::amber); g.strokePath (c, juce::PathStrokeType (2.0f));

        // Puntos arrastrables
        for (int n = 0; n < numNodes; ++n)
        {
            const auto i = info (n);
            const auto q = pos (n);
            const bool hi = n == hover || n == drag;
            g.setColour (i.col.withAlpha (hi ? 0.4f : 0.2f));
            g.fillEllipse (q.x - 11, q.y - 11, 22, 22);
            g.setColour (i.col.brighter (0.5f));
            g.fillEllipse (q.x - 6, q.y - 6, 12, 12);
            g.setColour (juce::Colours::white.withAlpha (hi ? 1.0f : 0.7f));
            g.drawEllipse (q.x - 6, q.y - 6, 12, 12, 1.5f);
        }

        // Valores al pasar el mouse
        const int shown = drag >= 0 ? drag : hover;
        if (shown >= 0)
        {
            const auto i = info (shown);
            juce::String t = juce::String (i.name) + "  " + hz (i.freq);
            if (i.gainId != nullptr) t << "  " << (i.gain >= 0 ? "+" : "") << juce::String (i.gain, 1) << " dB";
            if (shown == nMid) t << "  Q " << juce::String (v ("eq_mid_q"), 2);
            const auto q = pos (shown);
            auto box = juce::Rectangle<float> (0, 0, (float) t.length() * 6.6f + 16, 20)
                           .withCentre ({ q.x, q.y - 24 }).constrainedWithin (p);
            g.setColour (juce::Colour (0xee1c1d21)); g.fillRoundedRectangle (box, 4);
            g.setColour (i.col.brighter (0.5f)); g.drawRoundedRectangle (box, 4, 1.0f);
            g.setFont (font (11.0f, true)); g.setColour (juce::Colours::white);
            g.drawText (t, box, juce::Justification::centred);
        }
        else
        {
            g.setFont (font (10.0f));
            g.setColour (Col::amber.withAlpha (0.45f));
            g.drawText (U ("arrastra los puntos · rueda del mouse en MID = Q · doble clic = reset"), p.withHeight (14), juce::Justification::centredRight);
        }
    }

    void mouseMove (const juce::MouseEvent& e) override { const int h = nodeAt (e.position); if (h != hover) { hover = h; repaint(); } }
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }

    void gesture (int n, bool begin)
    {
        const auto i = info (n);
        for (auto* id : { i.freqId, i.gainId })
            if (id != nullptr) { auto* prm = proc.apvts.getParameter (id); if (begin) prm->beginChangeGesture(); else prm->endChangeGesture(); }
    }
    void mouseDown (const juce::MouseEvent& e) override { drag = nodeAt (e.position); if (drag >= 0) gesture (drag, true); }
    void mouseUp (const juce::MouseEvent&) override { if (drag >= 0) gesture (drag, false); drag = -1; repaint(); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (drag < 0) return;
        const auto i = info (drag);
        if (i.freqId != nullptr) setV (i.freqId, (float) fOf (e.position.x));
        if (i.gainId != nullptr) setV (i.gainId, juce::jlimit (-12.0f, 12.0f, dbOf (e.position.y)));
        repaint();
    }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (nodeAt (e.position) == nMid) setV ("eq_mid_q", v ("eq_mid_q") * (w.deltaY > 0 ? 1.12f : 1.0f / 1.12f));
    }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int n = nodeAt (e.position);
        if (n < 0) return;
        const auto i = info (n);
        for (auto* id : { i.freqId, i.gainId })
            if (id != nullptr) { auto* prm = proc.apvts.getParameter (id); prm->setValueNotifyingHost (prm->getDefaultValue()); }
    }
};

struct EqModule : Module
{
    Knob lowCut, low, midFreq, midGain, midQ, presence, air, hiCut;
    juce::ComboBox type, hpSlope, lpSlope;
    EqGraph graph;
    juce::Rectangle<int> info { 14, 720, 392, 40 };

    EqModule (TopVocalChainProcessor& p)
        : Module (p, "EQUALIZER", "eq_on", 113),
          lowCut (p.apvts, "eq_lowcut", "LOW CUT", Col::orange, "20", "100", "500"),
          low (p.apvts, "eq_low_gain", "LOW", Col::orange, "-12", "0", "+12"),
          midFreq (p.apvts, "eq_mid_freq", "MID FREQ", Col::blue, "200", "1k", "5k"),
          midGain (p.apvts, "eq_mid_gain", "MID", Col::blue, "-12", "0", "+12"),
          midQ (p.apvts, "eq_mid_q", "MID Q", Col::charcoal, "0.3", "1", "5"),
          presence (p.apvts, "eq_pres_gain", "PRESENCE", Col::green, "-12", "0", "+12"),
          air (p.apvts, "eq_air_gain", "AIR", Col::green, "-12", "0", "+12"),
          hiCut (p.apvts, "eq_hicut", "HIGH CUT", Col::slate, "2k", "9k", "OFF"),
          graph (p)
    {
        for (auto* k : { &lowCut, &low, &midFreq, &midGain, &midQ, &presence, &air, &hiCut }) add (*k);
        add (graph);
        setupBox (type, TopVocalChainProcessor::eqTypeNames(), "eq_type");
        setupBox (hpSlope, TopVocalChainProcessor::slopeNames(), "eq_hp_slope");
        setupBox (lpSlope, TopVocalChainProcessor::slopeNames(), "eq_lp_slope");
    }
    int status() const override { return param ("eq_on") ? (proc.meters.eqHot.load() ? 2 : 1) : 0; }
    void layout() override
    {
        type.setBounds (14, 46, 392, 26);
        graph.setBounds (12, 80, 396, 300);
        Knob* ks[] { &lowCut, &low, &midFreq, &midGain, &midQ, &presence, &air, &hiCut };
        for (int i = 0; i < 8; ++i) ks[i]->setBounds (10 + (i % 4) * 100, 390 + (i / 4) * 130, 100, 126);
        hpSlope.setBounds (14, 672, 190, 26);
        lpSlope.setBounds (216, 672, 190, 26);
    }
    void paintStatic (juce::Graphics& g) override
    {
        g.setFont (font (10.5f, true));
        g.setColour (Col::inkDim);
        g.drawText ("LOW CUT SLOPE", 14, 656, 190, 14, juce::Justification::centredLeft);
        g.drawText ("HIGH CUT SLOPE", 216, 656, 190, 14, juce::Justification::centredLeft);
    }
    void paintDynamic (juce::Graphics& g) override
    {
        g.setFont (font (11.5f));
        g.setColour (Col::inkDim);
        g.drawFittedText (U (eqText[juce::jlimit (0, 3, choice ("eq_type"))]), info, juce::Justification::topLeft, 2);
    }
    void tick() override { Module::tick(); graph.repaint(); repaint (info); }
};

//------------------------------------------------------------------------------
struct SpaceModule : Module
{
    LitButton tabDly { "DELAY", Col::amber }, tabRev { "REVERB", Col::amber };
    ModuleSwitch dlySw { [this] { return param ("dly_on") ? 1 : 0; } }, revSw { [this] { return param ("rev_on") ? 1 : 0; } };
    // Delay
    Knob dTime, dFb, dMix, dWidth, dLoCut, dHiCut, dSat, dMod, dDuck;
    LitButton sync { "SYNC", Col::amber }, ping { "PING PONG", Col::amber };
    juce::ComboBox div;
    juce::Rectangle<int> dDisp { 12, 768, 190, 50 };
    // Reverb
    Knob rPre, rDecay, rSize, rDamp, rEr, rDiff, rLoCut, rHiCut, rDuck, rWidth, rMix;
    juce::ComboBox rType;
    int tab = 0;
    bool lastDly = true, lastRev = true;

    SpaceModule (TopVocalChainProcessor& p)
        : Module (p, "DELAY / REVERB", nullptr, 130),
          dTime (p.apvts, "dly_time", "TIME", Col::mustard, "10", "", "2s"),
          dFb (p.apvts, "dly_fb", "FEEDBACK", Col::mustard, "0", "", "95"),
          dMix (p.apvts, "dly_mix", "MIX", Col::charcoal, "0", "", "100"),
          dWidth (p.apvts, "dly_width", "WIDTH", Col::purple, "0", "100", "200"),
          dLoCut (p.apvts, "dly_locut", "LO CUT", Col::orange, "20", "", "1k"),
          dHiCut (p.apvts, "dly_hicut", "HI CUT", Col::slate, "1k", "5k", "20k"),
          dSat (p.apvts, "dly_sat", "SATURATION", Col::red, "0", "", "100"),
          dMod (p.apvts, "dly_mod", "MODULATION", Col::green, "0", "", "100"),
          dDuck (p.apvts, "dly_duck", "DUCKING", Col::blue, "0", "", "100"),
          rPre (p.apvts, "rev_predelay", "PRE-DELAY", Col::slate, "0", "40", "200"),
          rDecay (p.apvts, "rev_decay", "DECAY", Col::blue, "0.2", "1.5", "8s"),
          rSize (p.apvts, "rev_size", "SIZE", Col::blue, "0", "", "100"),
          rDamp (p.apvts, "rev_damp", "DAMPING", Col::slate, "0", "", "100"),
          rEr (p.apvts, "rev_er", "EARLY REF", Col::mustard, "0", "", "100"),
          rDiff (p.apvts, "rev_diff", "DIFFUSION", Col::mustard, "0", "", "100"),
          rLoCut (p.apvts, "rev_locut", "LO CUT", Col::orange, "20", "", "1k"),
          rHiCut (p.apvts, "rev_hicut", "HI CUT", Col::slate, "1k", "6k", "20k"),
          rDuck (p.apvts, "rev_duck", "DUCKING", Col::green, "0", "", "100"),
          rWidth (p.apvts, "rev_width", "WIDTH", Col::purple, "0", "", "100"),
          rMix (p.apvts, "rev_mix", "MIX", Col::charcoal, "0", "", "100")
    {
        for (auto* k : delayKnobs()) add (*k);
        for (auto* k : reverbKnobs()) add (*k);
        for (auto* b : { &tabDly, &tabRev }) add (*b);
        tabDly.onClick = [this] { tab = 0; updateTab(); };
        tabRev.onClick = [this] { tab = 1; updateTab(); };
        dlySw.setClickingTogglesState (true); revSw.setClickingTogglesState (true);
        attach (dlySw, "dly_on");
        attach (revSw, "rev_on");
        sync.setClickingTogglesState (true); ping.setClickingTogglesState (true);
        attach (sync, "dly_sync");
        attach (ping, "dly_pingpong");
        setupBox (div, TopVocalChainProcessor::divisionNames(), "dly_div");
        setupBox (rType, TopVocalChainProcessor::reverbTypeNames(), "rev_type");
        updateTab();
    }
    std::vector<Knob*> delayKnobs() { return { &dTime, &dFb, &dMix, &dWidth, &dLoCut, &dHiCut, &dSat, &dMod, &dDuck }; }
    std::vector<Knob*> reverbKnobs() { return { &rPre, &rDecay, &rSize, &rDamp, &rEr, &rDiff, &rLoCut, &rHiCut, &rDuck, &rWidth, &rMix }; }

    void updateTab()
    {
        for (auto* k : delayKnobs()) k->setVisible (tab == 0);
        for (auto* c : std::initializer_list<juce::Component*> { &sync, &ping, &div, &dlySw }) c->setVisible (tab == 0);
        for (auto* k : reverbKnobs()) k->setVisible (tab == 1);
        for (auto* c : std::initializer_list<juce::Component*> { &rType, &revSw }) c->setVisible (tab == 1);
        tabDly.setToggleState (tab == 0, juce::dontSendNotification);
        tabRev.setToggleState (tab == 1, juce::dontSendNotification);
        repaint();
    }
    void layout() override
    {
        tabDly.setBounds (12, 44, 92, 26);
        tabRev.setBounds (110, 44, 92, 26);
        dlySw.setBounds (150, 78, 54, 22);
        revSw.setBounds (150, 78, 54, 22);
        rType.setBounds (12, 77, 132, 24);
        auto dk = delayKnobs();
        for (int i = 0; i < (int) dk.size(); ++i)
            dk[(size_t) i]->setBounds (i == 8 ? 55 : (i % 2 == 0 ? 4 : 106), 106 + (i / 2) * 118, 104, 116);
        sync.setBounds (12, 702, 90, 26);
        div.setBounds (108, 702, 94, 26);
        ping.setBounds (12, 734, 190, 26);
        auto rk = reverbKnobs();
        for (int i = 0; i < (int) rk.size(); ++i)
            rk[(size_t) i]->setBounds (i == 10 ? 55 : (i % 2 == 0 ? 4 : 106), 106 + (i / 2) * 118, 104, 116);
    }
    void paintDynamic (juce::Graphics& g) override
    {
        g.setFont (font (12.0f, true));
        g.setColour (Col::ink);
        if (tab == 0)
        {
            g.drawText ("ECO / DELAY", 12, 78, 130, 22, juce::Justification::centredLeft);
            auto r = dDisp.toFloat();
            lcd (g, r);
            const auto t = param ("dly_sync") ? TopVocalChainProcessor::divisionNames()[choice ("dly_div")] + "  SYNC"
                                              : juce::String (juce::roundToInt (value ("dly_time"))) + " ms";
            lcdText (g, t, r.withTrimmedTop (4).withHeight (26), 18.0f, true, juce::Justification::centred);
            lcdText (g, param ("dly_pingpong") ? "PING PONG" : U ("ESTÉREO"), r.withTrimmedTop (30).withHeight (16), 10.5f, false,
                     juce::Justification::centred, Col::amber.withAlpha (0.7f));
        }
    }
    void paintOverChildren (juce::Graphics& g) override
    {
        if (! param (tab == 0 ? "dly_on" : "rev_on"))
        {
            g.setColour (juce::Colour (0xff15161a).withAlpha (0.5f));
            g.fillRect (getLocalBounds().withTrimmedTop (104).reduced (1, 0).withTrimmedBottom (1));
        }
    }
    void tick() override
    {
        Module::tick();
        dlySw.tick(); revSw.tick();
        const bool d = param ("dly_on"), r = param ("rev_on");
        if (d != lastDly || r != lastRev) { lastDly = d; lastRev = r; repaint(); }
        // Indicador en las pestañas: punto verde si ese efecto esta encendido
        tabDly.setButtonText (juce::String ("DELAY") + (d ? "  \xe2\x97\x8f" : ""));
        tabRev.setButtonText (juce::String ("REVERB") + (r ? "  \xe2\x97\x8f" : ""));
        if (tab == 0) repaint (dDisp.expanded (4));
    }
};

//------------------------------------------------------------------------------
struct MetersModule : Module
{
    LedMeter in, out, gr;
    juce::Rectangle<int> top { 12, 44, 190, 118 }, corrBox { 12, 382, 190, 34 };
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
        in.setBounds (40, 172, 30, 190);
        out.setBounds (86, 172, 30, 190);
        gr.setBounds (142, 172, 30, 190);
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (top.contains (e.getPosition())) { proc.meters.resetMeters.store (true); clipHold = 0; }
    }
    void paintStatic (juce::Graphics& g) override
    {
        g.setFont (font (10.5f, true));
        g.setColour (Col::ink);
        const float meterTop = 172.0f, meterH = 184.0f;
        for (int db : { 6, 0, -12, -24, -48 })
        {
            const float y = meterTop + 3 + meterH * (1.0f - LedMeter::levelToProp ((float) db));
            g.drawText ((db > 0 ? "+" : "") + juce::String (db), 10, (int) y - 7, 26, 14, juce::Justification::centredRight);
        }
        for (int v : { 0, 10, 20 })
        {
            const float y = meterTop + 3 + meterH * ((float) v / 20.0f);
            g.drawText (juce::String (v), 176, (int) y - 7, 24, 14, juce::Justification::centredLeft);
        }
        for (auto [x, t] : { std::pair<int, const char*> { 40, "IN" }, { 86, "OUT" }, { 142, "GR" } })
            g.drawText (t, x - 6, 364, 42, 14, juce::Justification::centred);
    }
    void paintDynamic (juce::Graphics& g) override
    {
        const auto& m = proc.meters;
        auto r = top.toFloat();
        lcd (g, r);
        auto t = r.reduced (9, 6);
        const juce::Colour red (0xffff3b30);
        auto val = [] (float db, const char* unit) { return db < -90.0f ? juce::String ("-inf") : juce::String (db, 1) + " " + unit; };
        auto row = [&] (const juce::String& label, const juce::String& v, juce::Colour c = Col::amber)
        {
            auto rr = t.removeFromTop (21);
            lcdText (g, label, rr, 10.5f, false, juce::Justification::centredLeft, Col::amber.withAlpha (0.65f));
            lcdText (g, v, rr, 13.0f, true, juce::Justification::centredRight, c);
        };
        const float tp = m.truePeakDb.load();
        row (clipHold > 0 ? "PEAK  CLIP!" : "PEAK HOLD", val (m.peakHoldDb.load(), "dB"), clipHold > 0 ? red : Col::amber);
        row ("TRUE PEAK", val (tp, "dBTP"), tp > -1.0f ? red : Col::amber);
        row ("RMS", val (m.rmsDb.load(), "dB"));
        row ("LUFS M", val (m.lufsM.load(), "LU"));
        row ("LUFS INT", val (m.lufsI.load(), "LU"));

        auto c = corrBox.toFloat();
        lcd (g, c);
        const float corr = m.outCorr.load();
        lcdText (g, U ("CORRELACIÓN ") + (corr >= 0 ? "+" : "") + juce::String (corr, 2), c.reduced (8, 2).withHeight (14), 10.0f, true,
                 juce::Justification::centredLeft, corr < 0 ? red : Col::amber);
        auto bar = c.reduced (8, 0).withTrimmedTop (19).withHeight (8);
        g.setColour (Col::amber.withAlpha (0.12f)); g.fillRect (bar);
        const float x = bar.getCentreX() + juce::jlimit (-1.0f, 1.0f, corr) * bar.getWidth() * 0.5f;
        g.setColour (corr < 0 ? red : Col::amber); g.fillRect (x - 2.0f, bar.getY() - 2.0f, 4.0f, bar.getHeight() + 4.0f);
    }
    void tick() override
    {
        Module::tick();
        for (auto* mm : { &in, &out, &gr }) mm->tick();
        if (proc.meters.outClip.load()) clipHold = 60; else if (clipHold > 0) --clipHold;
        repaint (top.expanded (4));
        repaint (corrBox.expanded (4));
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

//------------------------------------------------------------------------------
// Informe de "ANALIZAR VOZ"
struct AnalysisPanel : juce::Component
{
    TopVocalChainProcessor& proc;
    tvc::VoiceAnalysis::Report rep;
    juce::TextButton apply { "APLICAR RECOMENDACIONES" }, close { "CERRAR" };
    std::function<void()> onClose;

    explicit AnalysisPanel (TopVocalChainProcessor& p) : proc (p)
    {
        rep = proc.analysis.computeReport();
        addAndMakeVisible (apply);
        addAndMakeVisible (close);
        apply.setEnabled (rep.valid);
        apply.onClick = [this] { proc.applyReport (rep); if (onClose) onClose(); };
        close.onClick = [this] { if (onClose) onClose(); };
    }
    void resized() override
    {
        apply.setBounds (getWidth() - 360, getHeight() - 52, 230, 34);
        close.setBounds (getWidth() - 120, getHeight() - 52, 100, 34);
    }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillRoundedRectangle (b.translated (0, 6), 10);
        g.setColour (Col::header); g.fillRoundedRectangle (b, 10);
        g.setColour (Col::amber.withAlpha (0.5f)); g.drawRoundedRectangle (b.reduced (0.5f), 10, 1.5f);
        auto r = b.reduced (22, 16);
        g.setFont (font (22.0f, true)); g.setColour (Col::amber);
        g.drawText ("AI VOICE ANALYSIS", r.removeFromTop (30), juce::Justification::centredLeft);
        if (! rep.valid)
        {
            g.setFont (font (15.0f)); g.setColour (Col::cream);
            g.drawFittedText (U ("No se detectó suficiente voz. Pulsa ANALIZAR VOZ y canta al menos 8 segundos."), r.removeFromTop (60).toNearestInt(),
                              juce::Justification::centredLeft, 3);
            return;
        }
        r.removeFromTop (8);
        auto left = r.removeFromLeft (r.getWidth() * 0.46f);
        auto hz = [] (float f) { return f >= 1000.0f ? juce::String (f / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (f)) + " Hz"; };
        auto item = [&] (juce::Rectangle<float>& area, const juce::String& label, const juce::String& v)
        {
            auto rr = area.removeFromTop (34);
            g.setFont (font (11.5f)); g.setColour (Col::cream.withAlpha (0.6f));
            g.drawText (label, rr.removeFromTop (14), juce::Justification::centredLeft);
            g.setFont (font (16.0f, true)); g.setColour (Col::cream);
            g.drawText (v, rr, juce::Justification::centredLeft);
        };
        item (left, U ("REGISTRO"), rep.registro);
        item (left, U ("FRECUENCIA MEDIA"), hz (rep.f0));
        item (left, U ("TONALIDAD"), TopVocalChainProcessor::keyNames()[rep.keyRoot] + (rep.minor ? " menor" : " mayor") + "  (" + juce::String (juce::roundToInt (rep.keyConf * 100)) + "%)");
        item (left, U ("NIVEL DE ENTRADA"), juce::String (rep.rmsDb, 1) + " dBFS");
        item (left, U ("DINÁMICA"), rep.dynLabel + "  (" + juce::String (rep.dynRange, 1) + " dB)");
        item (left, U ("RESONANCIA"), rep.cutResonance ? hz (rep.resonanceHz) + "  (+" + juce::String (rep.resonanceDb, 1) + " dB)" : juce::String ("No relevante"));
        item (left, U ("SIBILANCIA"), rep.sibLabel + "  (" + hz (rep.sibHz) + ")");
        item (left, U ("PRESENCIA"), rep.presenceLabel);
        item (left, U ("GRAVES"), rep.lowLabel);
        item (left, U ("RUIDO DE FONDO"), rep.noiseLabel);

        auto right = r.withTrimmedBottom (60);
        g.setFont (font (15.0f, true)); g.setColour (Col::amber);
        g.drawText (U ("RECOMENDACIÓN"), right.removeFromTop (24), juce::Justification::centredLeft);
        for (auto& line : rep.recommendations)
        {
            auto rr = right.removeFromTop (40);
            g.setColour (Col::amber); g.fillEllipse (rr.getX(), rr.getY() + 7, 6, 6);
            g.setFont (font (13.5f)); g.setColour (Col::cream);
            g.drawFittedText (line, rr.withTrimmedLeft (14).toNearestInt(), juce::Justification::topLeft, 2);
        }
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
            { 214, { mPre, mAi } }, { 250, { mTune } }, { 230, { mComp } }, { 420, { mEq } },
            { 214, { mWide, mMet } }, { 214, { mSpace } }, { 200, { mOut } } };
        return c;
    }
    const int moduleHeight[numModules] { 420, 404, 830, 400, 830, 830, 830, 424, 830 };
    const char* moduleNames[numModules] { "Preamp", "IA Voice", "Autotune", "Stereo Lab", "Compressor", "Equalizer",
                                          "Delay / Reverb", "Meters", "Output" };
}

MainView::MainView (TopVocalChainProcessor& p) : proc (p)
{
    modules[mPre]  = std::make_unique<PreampModule> (p);
    modules[mAi]   = std::make_unique<AiModule> (p);
    modules[mTune] = std::make_unique<TuneModule> (p);
    modules[mWide] = std::make_unique<WideModule> (p);
    modules[mComp] = std::make_unique<CompModule> (p);
    modules[mEq]   = std::make_unique<EqModule> (p);
    modules[mSpace] = std::make_unique<SpaceModule> (p);
    modules[mMet]  = std::make_unique<MetersModule> (p);
    modules[mOut]  = std::make_unique<OutputModule> (p);
    for (auto& m : modules) addChildComponent (*m);
    if (auto* ai = dynamic_cast<AiModule*> (modules[mAi].get()))
        ai->onShowReport = [this] { showReport(); };

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

void MainView::showReport()
{
    auto panel = std::make_unique<AnalysisPanel> (proc);
    juce::Component::SafePointer<MainView> safeThis (this);
    panel->onClose = [safeThis]
    {
        juce::MessageManager::callAsync ([safeThis] { if (safeThis != nullptr) safeThis->reportPanel.reset(); });
    };
    panel->setBounds (getLocalBounds().withSizeKeepingCentre (760, 560));
    addAndMakeVisible (*panel);
    reportPanel = std::move (panel);
}

void MainView::refreshPresetBox()
{
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
}

void MainView::tick()
{
    // Al terminar ANALIZAR VOZ se abre el informe automaticamente
    const bool ready = proc.analysis.isReady();
    if (ready && ! reportShownForThisAnalysis) showReport();
    reportShownForThisAnalysis = ready;
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
