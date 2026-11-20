#include "PluginEditor.h"
#include <map>

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
const char* compText[] { "Clean VCA — Preciso y transparente|Control exacto que respeta Attack y Release. Para nivelar sin cambiar el color.",
                         "Vintage Opto — Lento y musical|Cuanto más comprime, más lento suelta. Ideal para baladas y voces suaves.",
                         "FET — Rápido y agresivo|Atrapa los picos en ~1 ms y añade presencia. Ideal para voces urbanas, rap y transientes con mucha energía.",
                         "Vari-Mu Glue — Suave y pegado|El ratio sube con el nivel. Une la voz con la mezcla.",
                         "LA Vocal — Voz al frente|Opto más rápido y denso. El clásico para voces de pop y reggaetón.",
                         "Fast — Control de picos|Ataque y release rapidísimos. Para controlar golpes y consonantes.",
                         "Smooth — Muy suave|Detección lenta, compresión casi invisible.",
                         "Tube — Cálido|Compresión de válvula con armónicos pares. Engorda y redondea la voz.",
                         "Modern Digital — Limpio con auto-release|Release automático: rápido en picos, lento en frases largas. Sin color." };
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
    LitButton analog { "ANALOG", Col::amber };
    int lastType = -1;
    float satShown = 0.0f;
    juce::Rectangle<int> satBox { 160, 340, 44, 66 };

    PreampModule (TopVocalChainProcessor& p)
        : Module (p, "PREAMP", "pre_on", 28),
          drive (p.apvts, "pre_drive", "DRIVE", Col::red, "0", "12", "24"),
          inGain (p.apvts, "in_gain", "INPUT", Col::slate, "-24", "0", "+24"),
          warmth (p.apvts, "pre_warmth", "WARMTH", Col::orange, "0", "", "100"),
          trim (p.apvts, "pre_trim", "TRIM", Col::slate, "-12", "0", "+12")
    {
        for (auto* k : { &drive, &inGain, &warmth, &trim }) add (*k);
        setupBox (type, TopVocalChainProcessor::preTypeNames(), "pre_type");
        analog.setClickingTogglesState (true);
        analog.setTooltip (U ("ANALOG\nCada canal con tolerancias de componentes distintas y un ruido de fondo muy bajo, como un equipo real"));
        attach (analog, "pre_analog");
    }
    int status() const override { return param ("pre_on") ? (proc.meters.preHot.load() ? 2 : 1) : 0; }
    void layout() override
    {
        type.setBounds (14, 46, 186, 24);
        drive.setBounds (37, 90, 140, 140);
        inGain.setBounds (6, 230, 100, 96);
        warmth.setBounds (108, 230, 100, 96);
        trim.setBounds (57, 326, 100, 86);
        analog.setBounds (8, 352, 52, 24);
    }
    void paintDynamic (juce::Graphics& g) override
    {
        g.setFont (font (11.0f));
        g.setColour (Col::inkDim);
        g.drawFittedText (U (preText[juce::jlimit (0, 7, choice ("pre_type"))]), { 14, 73, 186, 14 }, juce::Justification::centred, 1);
        // Indicador de saturacion: verde normal, amarillo moderada, rojo fuerte
        const juce::Colour cols[3] { Col::ledGreen, juce::Colour (0xffffd23a), juce::Colour (0xffff3b30) };
        const float th[3] { 0.03f, 0.3f, 0.65f };
        for (int k = 0; k < 3; ++k)
        {
            const bool lit = satShown > th[k];
            const float y = (float) satBox.getY() + 2 + (float) (2 - k) * 17.0f, x = (float) satBox.getCentreX() - 6;
            g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillEllipse (x - 1, y - 1, 14, 14);
            g.setColour (lit ? cols[k] : cols[k].withAlpha (0.18f)); g.fillEllipse (x, y, 12, 12);
            if (lit) { g.setColour (cols[k].withAlpha (0.3f)); g.fillEllipse (x - 4, y - 4, 20, 20); }
        }
        g.setFont (font (9.0f, true)); g.setColour (Col::ink);
        g.drawText ("SAT", satBox.withY (satBox.getBottom() - 12).withHeight (12), juce::Justification::centred);
    }
    void tick() override
    {
        Module::tick();
        if (choice ("pre_type") != lastType) { lastType = choice ("pre_type"); repaint (10, 70, 194, 18); }
        const float s = proc.meters.preSat.load();
        satShown = s > satShown ? s : satShown * 0.9f + s * 0.1f;   // sube rapido, baja suave (animacion del LED)
        repaint (satBox.expanded (6));
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
    static constexpr int histLen = 150;   // ~5 s a 30 cuadros/s
    std::array<float, histLen> sungHist {}, corrHist {};
    int histPos = 0;
    float viewCentre = 60.0f;

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

        // Fila superior: nota objetivo, nota cantada, Hz y cents
        const bool inTune = voiced && std::abs (dev) < 10.0f;
        lcdText (g, U ("OBJETIVO"), { r.getX() + 10, r.getY() + 5, 70, 11 }, 9.5f, false, juce::Justification::centredLeft, Col::amber.withAlpha (0.6f));
        lcdText (g, voiced ? noteName (target) : juce::String ("--"), { r.getX() + 9, r.getY() + 14, 80, 30 }, 27.0f, true,
                 juce::Justification::centredLeft, voiced ? Col::amber : Col::amber.withAlpha (0.35f));
        lcdText (g, U ("CANTADA ") + (voiced ? noteName ((int) std::round (sung)) : juce::String ("--")), { r.getX() + 86, r.getY() + 6, 132, 14 }, 11.0f, true,
                 juce::Justification::centredRight, Col::amber.withAlpha (0.85f));
        lcdText (g, voiced ? juce::String (m.pitchHz.load(), 1) + " Hz" : juce::String ("--- Hz"), { r.getX() + 86, r.getY() + 20, 132, 12 }, 10.0f, false,
                 juce::Justification::centredRight, Col::amber.withAlpha (0.7f));
        lcdText (g, voiced ? juce::String ((dev >= 0 ? "+" : "") + juce::String (juce::roundToInt (dev)) + " c  ") + (inTune ? U ("AFINADO") : U ("CORRIGIENDO"))
                           : juce::String (U ("SIN VOZ")),
                 { r.getX() + 86, r.getY() + 32, 132, 13 }, 10.0f, true, juce::Justification::centredRight, inTune ? Col::ledGreen : Col::amber);

        // Grafico de afinacion en vivo: filas = notas (las de la escala resaltadas),
        // linea ambar = voz que entra, linea verde = voz corregida
        auto gr = juce::Rectangle<float> (r.getX() + 6, r.getY() + 50, r.getWidth() - 12, r.getHeight() - 56);
        g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillRect (gr);
        const float span = 7.0f, rowH = gr.getHeight() / (2.0f * span + 1.0f);
        const int keyIdx = choice ("tune_key"), scl = choice ("tune_scale");
        auto yOfM = [&] (float midi) { return gr.getCentreY() - (midi - viewCentre) * rowH; };
        for (int n = (int) std::floor (viewCentre - span - 1); n <= (int) std::ceil (viewCentre + span + 1); ++n)
        {
            const float y = yOfM ((float) n);
            if (y < gr.getY() - rowH || y > gr.getBottom() + rowH) continue;
            const bool inScale = tvc::Scales::allowed (n, keyIdx, scl);
            auto row = juce::Rectangle<float> (gr.getX(), y - rowH * 0.5f, gr.getWidth(), rowH).getIntersection (gr);
            if (inScale) { g.setColour (Col::amber.withAlpha (n == target ? 0.22f : 0.08f)); g.fillRect (row); }
            g.setColour (Col::amber.withAlpha (0.08f)); g.fillRect (row.getX(), row.getY(), row.getWidth(), 0.6f);
            if (inScale && rowH > 7.0f)
                lcdText (g, noteName (n), row.withWidth (30).translated (2, 0), juce::jmin (9.0f, rowH * 0.9f), false,
                         juce::Justification::centredLeft, Col::amber.withAlpha (n == target ? 0.9f : 0.5f));
        }
        auto trace = [&] (const std::array<float, histLen>& h, juce::Colour col, float w)
        {
            juce::Path p; bool pen = false;
            const float x0 = gr.getX() + 30, dx = (gr.getWidth() - 32) / (float) (histLen - 1);
            for (int i = 0; i < histLen; ++i)
            {
                const float v = h[(size_t) ((histPos + i) % histLen)];
                if (v <= 0.0f) { pen = false; continue; }
                const float x = x0 + (float) i * dx, y = juce::jlimit (gr.getY(), gr.getBottom(), yOfM (v));
                if (! pen) { p.startNewSubPath (x, y); pen = true; } else p.lineTo (x, y);
            }
            g.setColour (col.withAlpha (0.25f)); g.strokePath (p, juce::PathStrokeType (w * 2.6f));
            g.setColour (col); g.strokePath (p, juce::PathStrokeType (w));
        };
        g.saveState(); g.reduceClipRegion (gr.toNearestInt());
        trace (sungHist, Col::amber.withAlpha (0.85f), 1.4f);
        trace (corrHist, Col::ledGreen, 1.8f);
        g.restoreState();
        g.setColour (Col::amber.withAlpha (0.25f)); g.drawRect (gr, 1.0f);
        // velocidad de correccion: barrita en la esquina (mas llena = mas rapido)
        const float spd = 1.0f - juce::jlimit (0.0f, 1.0f, value ("tune_speed") / 400.0f);
        auto sb = juce::Rectangle<float> (gr.getRight() - 54, gr.getY() + 4, 48, 5);
        g.setColour (Col::amber.withAlpha (0.15f)); g.fillRect (sb);
        g.setColour (Col::ledGreen.withAlpha (0.8f)); g.fillRect (sb.withWidth (sb.getWidth() * spd));
        juce::ignoreUnused (corr);

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

        // Historial para el grafico de afinacion
        {
            const auto& m = proc.meters;
            const bool v = m.voiced.load();
            const float sung = m.sungMidi.load();
            sungHist[(size_t) histPos] = v ? sung : 0.0f;
            corrHist[(size_t) histPos] = v ? sung + m.correctionCents.load() / 100.0f : 0.0f;
            histPos = (histPos + 1) % histLen;
            if (v) viewCentre += ((float) std::round (sung) - viewCentre) * 0.08f;
        }

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
    LitButton mono { "MONO CHECK", Col::amber }, safe { "SAFE VOCAL", Col::ledGreen };
    juce::Rectangle<int> disp { 12, 352, 190, 40 }, vis { 12, 104, 190, 112 };
    std::vector<float> visL = std::vector<float> (1024), visR = std::vector<float> (1024);
    float widthEst = 0.0f;

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
        safe.setTooltip (U ("SAFE VOCAL / WIDTH AUTOMÁTICO\nGraves en mono y corrección automática de fase: cierra el ancho solo si hay problema de fase"));
        mono.setTooltip (U ("MONO CHECK\nEscucha la mezcla en mono para comprobar que la voz no se pierde en un celular"));
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
        amount.setBounds (0, 220, 72, 96);
        width.setBounds (71, 220, 72, 96);
        mix.setBounds (142, 220, 72, 96);
        mono.setBounds (12, 320, 92, 26);
        safe.setBounds (110, 320, 92, 26);
    }
    void paintDynamic (juce::Graphics& g) override
    {
        const juce::Colour warn (0xffff5a3c), teal (0xff6fd3e8);
        // ---- Imagen estereo: semicirculo L..R, centro = mono
        auto v = vis.toFloat();
        lcd (g, v);
        const float cx = v.getCentreX(), base = v.getBottom() - 14, R = std::min (v.getWidth() * 0.5f - 10, v.getHeight() - 22);
        g.setColour (teal.withAlpha (0.18f));
        for (float k : { 0.33f, 0.66f, 1.0f })
        {
            juce::Path a; a.addCentredArc (cx, base, R * k, R * k, 0, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
            g.strokePath (a, juce::PathStrokeType (0.8f));
        }
        for (int i = -3; i <= 3; ++i)
        {
            const float a = (float) i / 3.0f * juce::MathConstants<float>::halfPi;
            g.drawLine (cx, base, cx + std::sin (a) * R, base - std::cos (a) * R, 0.7f);
        }
        // nube de puntos: angulo = balance lado/centro, radio = nivel
        const float peak = std::max (1.0e-4f, juce::FloatVectorOperations::findMaximum (visL.data(), (int) visL.size())
                                              + juce::FloatVectorOperations::findMaximum (visR.data(), (int) visR.size()));
        for (size_t i = 0; i < visL.size(); i += 3)
        {
            const float l = visL[i], r = visR[i];
            const float mid = (l + r) * 0.5f, side = (l - r) * 0.5f;
            const float mag = std::sqrt (mid * mid + side * side);
            if (mag < 1.0e-5f) continue;
            float a = std::atan2 (side, std::abs (mid));               // -pi/2..pi/2
            if (mid < 0 && std::abs (side) < std::abs (mid) * 0.05f) a = 0;
            const float rad = juce::jlimit (0.0f, 1.0f, mag / peak * 1.6f) * R;
            const float px = cx - std::sin (a) * rad, py = base - std::cos (a) * rad;
            g.setColour ((std::abs (a) > 1.2f ? warn : teal).withAlpha (0.55f));
            g.fillRect (px - 0.8f, py - 0.8f, 1.6f, 1.6f);
        }
        // abanico del ancho medido
        const float wa = juce::jlimit (0.0f, 1.0f, widthEst) * juce::MathConstants<float>::halfPi;
        juce::Path fan; fan.startNewSubPath (cx, base);
        fan.lineTo (cx - std::sin (wa) * R, base - std::cos (wa) * R);
        fan.addCentredArc (cx, base, R, R, 0, -wa, wa, false);
        fan.closeSubPath();
        g.setColour (teal.withAlpha (0.10f)); g.fillPath (fan);
        g.setColour (teal.withAlpha (0.7f)); g.strokePath (fan, juce::PathStrokeType (1.0f));
        g.setFont (font (9.5f, true)); g.setColour (teal.withAlpha (0.8f));
        g.drawText ("L", juce::Rectangle<float> (v.getX() + 5, base - 2, 12, 12), juce::Justification::centred);
        g.drawText ("R", juce::Rectangle<float> (v.getRight() - 17, base - 2, 12, 12), juce::Justification::centred);
        g.drawText ("WIDTH " + juce::String (juce::roundToInt (widthEst * 100)) + "%", juce::Rectangle<float> (v.getX() + 6, v.getY() + 4, 90, 11),
                    juce::Justification::centredLeft);

        // ---- Correlacion y compatibilidad mono
        auto r = disp.toFloat();
        lcd (g, r);
        auto t = r.reduced (8, 4);
        const float corr = proc.meters.labCorr.load();
        const bool problem = corr < 0.0f;
        const float guard = proc.meters.phaseGuard.load();
        lcdText (g, (corr >= 0 ? "+" : "") + juce::String (corr, 2), t.withHeight (13), 10.5f, true, juce::Justification::centredLeft);
        lcdText (g, param ("wide_mono") ? U ("ESCUCHANDO MONO") : guard < 0.97f ? U ("AUTO FASE -") + juce::String (juce::roundToInt ((1.0f - guard) * 100)) + "%"
                                         : problem ? U ("PROBLEMA DE FASE") : U ("MONO COMPATIBLE"),
                 t.withHeight (13), 10.5f, true, juce::Justification::centredRight, problem ? warn : guard < 0.97f ? Col::amber : Col::ledGreen);
        auto bar = t.withTrimmedTop (18).withHeight (9);
        g.setColour (Col::amber.withAlpha (0.12f)); g.fillRect (bar);
        g.setColour (warn.withAlpha (0.25f)); g.fillRect (bar.withWidth (bar.getWidth() * 0.5f));
        const float x = bar.getCentreX() + juce::jlimit (-1.0f, 1.0f, corr) * bar.getWidth() * 0.5f;
        g.setColour (problem ? warn : Col::amber); g.fillRect (x - 2.0f, bar.getY() - 2.0f, 4.0f, bar.getHeight() + 4.0f);
    }
    void tick() override
    {
        Module::tick();
        const int mode = choice ("wide_mode");
        for (int i = 0; i < modes.size(); ++i)
            if (modes[i]->getToggleState() != (i == mode)) modes[i]->setToggleState (i == mode, juce::dontSendNotification);
        proc.tapPost.read (visL.data(), visR.data(), (int) visL.size());
        double mm = 0, ss = 0;
        for (size_t i = 0; i < visL.size(); ++i) { const double m = visL[i] + visR[i], s = visL[i] - visR[i]; mm += m * m; ss += s * s; }
        const float target = mm + ss > 1.0e-9 ? (float) std::sqrt (ss / (mm + ss)) * 1.41f : 0.0f;   // 0 = mono, 1 = muy abierto
        widthEst += (juce::jlimit (0.0f, 1.0f, target) - widthEst) * 0.2f;
        repaint (disp.expanded (4));
        repaint (vis.expanded (4));
    }
};

//------------------------------------------------------------------------------
struct CompModule : Module
{
    Knob thr, ratio, attack, release, knee, makeup, mix, hpf;
    juce::ComboBox type;
    LitButton scExt { "SIDECHAIN EXT", Col::amber };
    LedMeter inM, grM, outM;
    int lastType = -1;
    juce::Rectangle<int> curve { 12, 78, 148, 150 }, info { 14, 752, 202, 70 };
    static constexpr int grLen = 120;
    std::array<float, grLen> grHist {};
    int grPos = 0;

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

        // Knee: zona sombreada alrededor del threshold
        {
            const float kn = value ("comp_knee");
            g.setColour (Col::amber.withAlpha (0.08f));
            g.fillRect (juce::Rectangle<float> (px (thrDb - kn * 0.5f), plot.getY(), std::max (1.0f, px (thrDb + kn * 0.5f) - px (thrDb - kn * 0.5f)), plot.getHeight()));
        }
        // Nombre del modo y su caracter
        const auto txt = juce::StringArray::fromTokens (U (compText[juce::jlimit (0, 8, choice ("comp_type"))]), "|", "");
        g.setFont (font (11.0f, true));
        g.setColour (Col::ink);
        g.drawText (txt[0], info.withHeight (14), juce::Justification::centredLeft, true);
        // Reduccion de ganancia en el tiempo: se ve como actuan el Attack y el Release
        auto gh = info.toFloat().withTrimmedTop (17);
        lcd (g, gh);
        auto ghp = gh.reduced (4, 4);
        g.setColour (Col::amber.withAlpha (0.12f));
        for (int db : { 6, 12 }) g.fillRect (ghp.getX(), ghp.getY() + ghp.getHeight() * (float) db / 18.0f, ghp.getWidth(), 1.0f);
        juce::Path gp; gp.startNewSubPath (ghp.getX(), ghp.getY());
        for (int i = 0; i < grLen; ++i)
        {
            const float v = juce::jlimit (0.0f, 18.0f, -grHist[(size_t) ((grPos + i) % grLen)]);
            gp.lineTo (ghp.getX() + ghp.getWidth() * (float) i / (grLen - 1), ghp.getY() + ghp.getHeight() * v / 18.0f);
        }
        gp.lineTo (ghp.getRight(), ghp.getY()); gp.closeSubPath();
        g.setColour (Col::amber.withAlpha (0.25f)); g.fillPath (gp);
        g.setColour (Col::amber); g.strokePath (gp, juce::PathStrokeType (1.2f));
        lcdText (g, "GR", ghp.withWidth (20).withHeight (10), 8.5f, true, juce::Justification::centredLeft, Col::amber.withAlpha (0.6f));
    }
    void tick() override
    {
        Module::tick();
        for (auto* mm : { &inM, &grM, &outM }) mm->tick();
        grHist[(size_t) grPos] = param ("comp_on") ? proc.meters.compGrDb.load() : 0.0f;
        grPos = (grPos + 1) % grLen;
        if (choice ("comp_type") != lastType)
        {
            lastType = choice ("comp_type");
            type.setTooltip (juce::StringArray::fromTokens (U (compText[juce::jlimit (0, 8, lastType)]), "|", "").joinIntoString ("\n"));
        }
        repaint (curve.expanded (4));
        repaint (info.expanded (2));
    }
};

//------------------------------------------------------------------------------
// Analizador de espectro (FFT 4096, Hann, suavizado). Lee la copia de audio del procesador.
struct SpectrumAnalyzer
{
    static constexpr int order = 12, N = 1 << order, points = 200;
    juce::dsp::FFT fft { order };
    std::vector<float> win, buf, tmp;
    std::array<float, points> smooth {};
    SpectrumAnalyzer() : win ((size_t) N), buf ((size_t) N * 2), tmp ((size_t) N)
    {
        for (int i = 0; i < N; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (N - 1));
        smooth.fill (-120.0f);
    }
    // Actualiza 'smooth' (dB por punto, eje log 20 Hz..20 kHz, inclinacion +4.5 dB/oct como los analizadores de mezcla)
    void update (const AudioTap& tap, double sr)
    {
        std::vector<float> r ((size_t) N);
        tap.read (tmp.data(), r.data(), N);
        for (int i = 0; i < N; ++i) buf[(size_t) i] = 0.5f * (tmp[(size_t) i] + r[(size_t) i]) * win[(size_t) i];
        std::fill (buf.begin() + N, buf.end(), 0.0f);
        fft.performFrequencyOnlyForwardTransform (buf.data(), true);
        const float norm = 4.0f / (float) N;
        for (int p = 0; p < points; ++p)
        {
            const double f0 = 20.0 * std::pow (1000.0, (double) p / points), f1 = 20.0 * std::pow (1000.0, (double) (p + 1) / points);
            int b0 = (int) (f0 / sr * N), b1 = std::max (b0 + 1, (int) (f1 / sr * N));
            b0 = juce::jlimit (1, N / 2 - 1, b0); b1 = juce::jlimit (b0 + 1, N / 2, b1);
            float m = 0.0f;
            for (int b = b0; b < b1; ++b) m = std::max (m, buf[(size_t) b]);
            const double fc = std::sqrt (f0 * f1);
            float db = juce::Decibels::gainToDecibels (m * norm, -120.0f) + 4.5f * (float) std::log2 (fc / 1000.0);
            auto& s = smooth[(size_t) p];
            s = db > s ? s + (db - s) * 0.6f : s + (db - s) * 0.15f;   // sube rapido, cae suave
        }
    }
};

struct EqGraph : juce::Component
{
    enum Node { nLC, nLow, nMid, nPres, nAir, nHC, numNodes };
    TopVocalChainProcessor& proc;
    int hover = -1, drag = -1;
    juce::Point<float> mouse;

    SpectrumAnalyzer pre, post;
    int analyzerMode = 1;   // 0 off, 1 post, 2 pre + post
    explicit EqGraph (TopVocalChainProcessor& p) : proc (p) {}

    void updateSpectrum()
    {
        if (analyzerMode == 0) return;
        const double sr = proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0;
        post.update (proc.tapPost, sr);
        if (analyzerMode == 2) pre.update (proc.tapPre, sr);
    }
    void drawSpectrum (juce::Graphics& g, const SpectrumAnalyzer& s, juce::Colour col, bool filled)
    {
        auto p = plot();
        auto yS = [&] (float db) { return p.getBottom() - juce::jlimit (0.0f, 1.0f, (db + 84.0f) / 78.0f) * p.getHeight(); };
        juce::Path path;
        for (int i = 0; i < SpectrumAnalyzer::points; ++i)
        {
            const double f = 20.0 * std::pow (1000.0, (i + 0.5) / SpectrumAnalyzer::points);
            const float x = xOf (f), y = yS (s.smooth[(size_t) i]);
            if (i == 0) { path.startNewSubPath (p.getX(), p.getBottom()); path.lineTo (x, y); } else path.lineTo (x, y);
        }
        path.lineTo (p.getRight(), p.getBottom());
        if (filled)
        {
            g.setGradientFill (juce::ColourGradient (col.withAlpha (0.38f), 0, p.getY(), col.withAlpha (0.05f), 0, p.getBottom(), false));
            g.fillPath (path);
        }
        g.setColour (col.withAlpha (filled ? 0.7f : 0.55f));
        g.strokePath (path, juce::PathStrokeType (1.0f));
    }

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

        // Espectro en tiempo real: gris = entrada (PRE), azul = salida (POST)
        if (analyzerMode == 2) drawSpectrum (g, pre, juce::Colour (0xffb8bcc6), false);
        if (analyzerMode >= 1) drawSpectrum (g, post, juce::Colour (0xff5fb4ff), true);

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
    LitButton analyzer { "ANALYZER: POST", Col::amber };
    juce::ComboBox midType, msMode;
    LitButton solo { "SOLO MID", Col::amber };
    Knob dyn, dynThr;
    juce::Rectangle<int> dynBox { 218, 750, 188, 72 };
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
          graph (p),
          dyn (p.apvts, "eq_dyn", "DYNAMIC", Col::blue, "0", "", "100"),
          dynThr (p.apvts, "eq_dyn_thr", "DYN THR", Col::charcoal, "-50", "", "0")
    {
        for (auto* k : { &lowCut, &low, &midFreq, &midGain, &midQ, &presence, &air, &hiCut, &dyn, &dynThr }) add (*k);
        setupBox (midType, juce::StringArray { "Bell", "Notch" }, "eq_mid_type");
        setupBox (msMode, juce::StringArray { "Stereo", "Mid", "Side" }, "eq_ms");
        midType.setTooltip (U ("Tipo de la banda MID\nBell = campana normal · Notch = corte muy estrecho para quitar una resonancia"));
        msMode.setTooltip (U ("STEREO / MID / SIDE\nMid = ecualiza solo el centro (la voz) · Side = solo los lados (efectos, coros)"));
        solo.setClickingTogglesState (true);
        solo.setTooltip (U ("SOLO MID\nEscucha solo la zona de la banda MID para encontrar resonancias"));
        attach (solo, "eq_solo");
        dyn.setDescription (U ("EQ dinámico en la banda MID: baja esa zona solo cuando suena fuerte"));
        dynThr.setDescription (U ("A partir de qué nivel actúa el EQ dinámico"));
        add (graph);
        setupBox (type, TopVocalChainProcessor::eqTypeNames(), "eq_type");
        setupBox (hpSlope, TopVocalChainProcessor::slopeNames(), "eq_hp_slope");
        setupBox (lpSlope, TopVocalChainProcessor::slopeNames(), "eq_lp_slope");
        analyzer.setTooltip (U ("Analizador de espectro\nPOST = salida, PRE+POST = compara entrada (gris) y salida (azul)"));
        analyzer.setToggleState (true, juce::dontSendNotification);
        analyzer.onClick = [this]
        {
            graph.analyzerMode = (graph.analyzerMode + 1) % 3;
            analyzer.setButtonText (graph.analyzerMode == 0 ? "ANALYZER: OFF" : graph.analyzerMode == 1 ? "ANALYZER: POST" : "PRE + POST");
            analyzer.setToggleState (graph.analyzerMode != 0, juce::dontSendNotification);
        };
        add (analyzer);
    }
    int status() const override { return param ("eq_on") ? (proc.meters.eqHot.load() ? 2 : 1) : 0; }
    void layout() override
    {
        type.setBounds (14, 46, 270, 26);
        type.setTooltip (U (eqText[juce::jlimit (0, 3, choice ("eq_type"))]));
        analyzer.setBounds (290, 46, 116, 26);
        graph.setBounds (12, 80, 396, 300);
        Knob* ks[] { &lowCut, &low, &midFreq, &midGain, &midQ, &presence, &air, &hiCut };
        for (int i = 0; i < 8; ++i) ks[i]->setBounds (10 + (i % 4) * 100, 390 + (i / 4) * 130, 100, 126);
        hpSlope.setBounds (14, 672, 190, 26);
        lpSlope.setBounds (216, 672, 190, 26);
        midType.setBounds (14, 716, 100, 26);
        msMode.setBounds (120, 716, 100, 26);
        solo.setBounds (226, 716, 180, 26);
        dyn.setBounds (8, 748, 100, 78);
        dynThr.setBounds (110, 748, 100, 78);
    }
    void paintStatic (juce::Graphics& g) override
    {
        g.setFont (font (10.5f, true));
        g.setColour (Col::inkDim);
        g.drawText ("LOW CUT SLOPE", 14, 656, 190, 14, juce::Justification::centredLeft);
        g.drawText ("HIGH CUT SLOPE", 216, 656, 190, 14, juce::Justification::centredLeft);
        g.drawText ("MID: TIPO", 14, 702, 100, 13, juce::Justification::centredLeft);
        g.drawText ("MODO", 120, 702, 100, 13, juce::Justification::centredLeft);
    }
    void paintDynamic (juce::Graphics& g) override
    {
        // EQ dinamico: cuanto esta bajando ahora la banda MID
        auto r = dynBox.toFloat();
        lcd (g, r);
        auto t = r.reduced (8, 6);
        const float dg = proc.meters.eqDynDb.load();
        lcdText (g, U ("EQ DINÁMICO (MID)"), t.removeFromTop (14), 10.0f, true, juce::Justification::centredLeft, Col::amber.withAlpha (0.7f));
        lcdText (g, value ("eq_dyn") > 0.5f ? juce::String (dg, 1) + " dB" : juce::String ("OFF"), t.removeFromTop (22), 17.0f, true, juce::Justification::centredLeft);
        auto bar = t.removeFromTop (10);
        g.setColour (Col::amber.withAlpha (0.12f)); g.fillRect (bar);
        g.setColour (Col::amber); g.fillRect (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, -dg / 12.0f)));
    }
    void tick() override { Module::tick(); graph.updateSpectrum(); graph.repaint(); repaint (dynBox.expanded (3)); }
};

//------------------------------------------------------------------------------
struct SpaceModule : Module
{
    LitButton tabDly { "DELAY", Col::amber }, tabRev { "REVERB", Col::amber };
    ModuleSwitch dlySw { [this] { return param ("dly_on") ? 1 : 0; } }, revSw { [this] { return param ("rev_on") ? 1 : 0; } };
    // Delay
    Knob dTime, dFb, dMix, dWidth, dLoCut, dHiCut, dSat, dMod, dDuck;
    LitButton sync { "SYNC", Col::amber }, ping { "PING PONG", Col::amber };
    juce::TextButton tap { "TAP" };
    juce::ComboBox div, dType;
    std::vector<double> taps;
    juce::Rectangle<int> rDisp { 12, 712, 190, 106 };
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
        setupBox (dType, TopVocalChainProcessor::delayTypeNames(), "dly_type");
        dType.setTooltip (U ("Tipo de delay\nDigital = limpio · Tape = cinta con wow/flutter · Analog = oscuro y cálido · Slap = eco corto de rockabilly/rap"));
        tap.setTooltip (U ("TAP TEMPO\nToca al ritmo de la canción (2 o más veces) para fijar el tiempo del delay"));
        tap.onClick = [this]
        {
            const double now = juce::Time::getMillisecondCounterHiRes();
            if (! taps.empty() && now - taps.back() > 2000.0) taps.clear();
            taps.push_back (now);
            if (taps.size() > 4) taps.erase (taps.begin());
            if (taps.size() >= 2)
            {
                const double ms = (taps.back() - taps.front()) / (double) (taps.size() - 1);
                auto set = [this] (const char* id, float v) { auto* prm = proc.apvts.getParameter (id); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); };
                set ("dly_sync", 0.0f);
                set ("dly_time", (float) juce::jlimit (10.0, 2000.0, ms));
            }
        };
        add (tap);
        updateTab();
    }
    std::vector<Knob*> delayKnobs() { return { &dTime, &dFb, &dMix, &dWidth, &dLoCut, &dHiCut, &dSat, &dMod, &dDuck }; }
    std::vector<Knob*> reverbKnobs() { return { &rPre, &rDecay, &rSize, &rDamp, &rEr, &rDiff, &rLoCut, &rHiCut, &rDuck, &rWidth, &rMix }; }

    void updateTab()
    {
        for (auto* k : delayKnobs()) k->setVisible (tab == 0);
        for (auto* c : std::initializer_list<juce::Component*> { &sync, &ping, &div, &dlySw, &dType, &tap }) c->setVisible (tab == 0);
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
        dType.setBounds (12, 77, 132, 24);
        auto dk = delayKnobs();
        for (int i = 0; i < (int) dk.size(); ++i)
            dk[(size_t) i]->setBounds (i == 8 ? 55 : (i % 2 == 0 ? 4 : 106), 106 + (i / 2) * 118, 104, 116);
        sync.setBounds (12, 702, 90, 26);
        div.setBounds (108, 702, 94, 26);
        ping.setBounds (12, 734, 120, 26);
        tap.setBounds (138, 734, 64, 26);
        auto rk = reverbKnobs();
        for (int i = 0; i < (int) rk.size(); ++i)
            rk[(size_t) i]->setBounds (i == 10 ? 55 : (i % 2 == 0 ? 4 : 106), 104 + (i / 2) * 101, 104, 100);
    }
    void paintDynamic (juce::Graphics& g) override
    {
        if (tab == 0)
        {
            // Repeticiones del delay: cada barra es un eco (altura = volumen, posicion = tiempo; L/R en ping pong)
            auto r = dDisp.toFloat();
            lcd (g, r);
            const bool pp = param ("dly_pingpong");
            const double ms = proc.getDelayMs();
            const auto t = (param ("dly_sync") ? TopVocalChainProcessor::divisionNames()[choice ("dly_div")] + "  " : juce::String())
                           + juce::String (juce::roundToInt (ms)) + " ms";
            lcdText (g, t, r.reduced (6, 3).withHeight (13), 10.5f, true, juce::Justification::centredLeft);
            lcdText (g, TopVocalChainProcessor::delayTypeNames()[choice ("dly_type")].toUpperCase(), r.reduced (6, 3).withHeight (13), 9.5f, false,
                     juce::Justification::centredRight, Col::amber.withAlpha (0.7f));
            auto plot = r.reduced (6, 3).withTrimmedTop (15);
            const float fb = value ("dly_fb") * 0.01f * (choice ("dly_type") == 3 ? 0.2f / std::max (0.2f, value ("dly_fb") * 0.01f) : 1.0f);
            const double window = 2000.0;
            float amp = 0.85f;
            for (int k = 1; k <= 16 && amp > 0.03f; ++k)
            {
                const float x = plot.getX() + plot.getWidth() * (float) std::min (1.0, ms * k / window);
                if (x >= plot.getRight()) break;
                const bool right = pp && (k % 2 == 0);
                const float h = plot.getHeight() * amp;
                g.setColour ((right ? juce::Colour (0xff6fd3e8) : Col::amber).withAlpha (0.85f));
                g.fillRect (x - 1.5f, plot.getBottom() - h, 3.0f, h);
                amp *= std::max (0.0f, fb);
            }
            g.setColour (Col::amber.withAlpha (0.9f));
            g.fillRect (plot.getX(), plot.getY(), 2.0f, plot.getHeight());   // sonido original
        }
        else
        {
            // Reverb: reflexiones tempranas (barras) y cola de decay (curva)
            auto r = rDisp.toFloat();
            lcd (g, r);
            auto plot = r.reduced (8, 6).withTrimmedTop (14);
            lcdText (g, TopVocalChainProcessor::reverbTypeNames()[choice ("rev_type")].toUpperCase() + "  " + juce::String (value ("rev_decay"), 1) + " s",
                     r.reduced (8, 4).withHeight (13), 10.5f, true, juce::Justification::centredLeft);
            const float pre = value ("rev_predelay"), decay = std::max (0.2f, value ("rev_decay"));
            const float window = 200.0f + decay * 1000.0f * 0.9f;
            auto xOfMs = [&] (float msv) { return plot.getX() + plot.getWidth() * juce::jlimit (0.0f, 1.0f, msv / window); };
            g.setColour (Col::amber.withAlpha (0.9f));
            g.fillRect (plot.getX(), plot.getY(), 2.0f, plot.getHeight());
            const float er = value ("rev_er") * 0.01f, size = value ("rev_size") * 0.01f;
            juce::Random rnd (choice ("rev_type") * 31 + 7);
            for (int k = 0; k < 10; ++k)
            {
                const float tms = pre + (4.0f + (float) k * (6.0f + 10.0f * size)) * (0.8f + 0.4f * rnd.nextFloat());
                const float h = plot.getHeight() * (0.25f + 0.65f * er) * (1.0f - (float) k * 0.08f);
                g.setColour (juce::Colour (0xff6fd3e8).withAlpha (0.8f));
                g.fillRect (xOfMs (tms) - 1.0f, plot.getBottom() - h, 2.0f, h);
            }
            juce::Path tail;
            const float start = pre + 10.0f;
            tail.startNewSubPath (xOfMs (start), plot.getBottom());
            for (int i = 0; i <= 60; ++i)
            {
                const float tms = start + (window - start) * (float) i / 60.0f;
                const float lvl = std::pow (10.0f, -3.0f * (tms - start) / (decay * 1000.0f));   // -60 dB en 'decay'
                tail.lineTo (xOfMs (tms), plot.getBottom() - plot.getHeight() * 0.7f * lvl);
            }
            tail.lineTo (plot.getRight(), plot.getBottom()); tail.closeSubPath();
            g.setColour (Col::amber.withAlpha (0.2f)); g.fillPath (tail);
            g.setColour (Col::amber.withAlpha (0.8f)); g.strokePath (tail, juce::PathStrokeType (1.0f));
            lcdText (g, "PRE " + juce::String (juce::roundToInt (pre)) + " ms", plot.withHeight (12).withTrimmedLeft (4), 9.0f, false,
                     juce::Justification::centredRight, Col::amber.withAlpha (0.6f));
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
        repaint ((tab == 0 ? dDisp : rDisp).expanded (4));
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
// NAVEGADOR DE PRESETS: por genero, artista, tipo de voz y estilo de mezcla (+ presets del usuario)
struct PresetBrowser : juce::Component
{
    struct Entry { juce::String name; int factory = -1; juce::File file; juce::StringArray tags; };
    TopVocalChainProcessor& proc;
    std::vector<Entry> all;
    juce::StringArray groups { "TODOS", U ("GÉNERO"), "ARTISTA", "TIPO DE VOZ", "MEZCLA", "MIS PRESETS" };
    std::map<juce::String, juce::StringArray> groupTags;
    int group = 0; juce::String tag;
    int hoverRow = -1, hoverTag = -1, scroll = 0;
    juce::TextButton close { "CERRAR" };
    std::function<void()> onClose, onLoaded;

    static juce::StringArray tagsFor (const juce::String& n)
    {
        auto t = [] (std::initializer_list<const char*> l) { juce::StringArray s; for (auto* x : l) s.add (U (x)); return s; };
        if (n == "Default")                   return t ({ "Neutro", "Voz media", "Limpia" });
        if (n == "Dominican Dembow")          return t ({ "Urbano", "Dembow", "Voz media", "Agresiva" });
        if (n == "Trap Aggressive")           return t ({ "Urbano", "Trap", "Voz grave", "Agresiva" });
        if (n == "Modern Reggaeton")          return t ({ "Urbano", "Reggaetón", "Voz media", "Espaciosa" });
        if (n == "Bad Bunny Style")           return t ({ "Urbano", "Reggaetón", "Bad Bunny", "Voz grave", "Espaciosa" });
        if (n == "El Alfa Style")             return t ({ "Urbano", "Dembow", "El Alfa", "Voz media", "Agresiva" });
        if (n == "Melodic Trap")              return t ({ "Urbano", "Trap", "Voz aguda", "Espaciosa" });
        if (n == "Clean Pop Vocal")           return t ({ "Pop", "Voz aguda", "Limpia" });
        if (n == "R&B Smooth")                return t ({ "R&B", "Voz aguda", "Cálida" });
        if (n == "Rap Punch")                 return t ({ "Urbano", "Rap", "Voz grave", "Agresiva" });
        if (n == "Drill Vocal")               return t ({ "Urbano", "Drill", "Voz grave", "Agresiva" });
        if (n == "Bachata Modern")            return t ({ "Tropical", "Bachata", "Voz aguda", "Cálida" });
        if (n == "Studio Clean")              return t ({ "Neutro", "Voz media", "Limpia" });
        if (n.startsWith ("Clasico Neve"))    return t ({ "Rock / Pop", "Voz media", "Vintage" });
        if (n.startsWith ("Pultec"))          return t ({ "Balada", "Voz aguda", "Cálida", "Vintage" });
        if (n.startsWith ("API"))             return t ({ "Rock / Pop", "Voz grave", "Vintage", "Agresiva" });
        if (n.startsWith ("Lo-Fi"))           return t ({ "Lo-Fi", "Voz media", "Vintage" });
        if (n.startsWith ("Podcast"))         return t ({ "Hablada", "Voz hablada", "Limpia" });
        return {};
    }

    explicit PresetBrowser (TopVocalChainProcessor& p) : proc (p)
    {
        for (int i = 0; i < proc.getNumPrograms(); ++i) all.push_back ({ proc.getProgramName (i), i, {}, tagsFor (proc.getProgramName (i)) });
        for (auto& f : TopVocalChainProcessor::getUserPresetFolder().findChildFiles (juce::File::findFiles, false, "*.tvcpreset"))
            all.push_back ({ f.getFileNameWithoutExtension(), -1, f, { "Mis presets" } });
        groupTags[U ("GÉNERO")] = juce::StringArray { "Urbano", "Dembow", U ("Reggaetón"), "Trap", "Rap", "Drill", "Pop", "R&B", "Tropical", "Bachata",
                                                       "Rock / Pop", "Balada", "Lo-Fi", "Hablada", "Neutro" };
        groupTags["ARTISTA"]     = juce::StringArray { "Bad Bunny", "El Alfa" };
        groupTags["TIPO DE VOZ"] = juce::StringArray { "Voz grave", "Voz media", "Voz aguda", "Voz hablada" };
        groupTags["MEZCLA"]      = juce::StringArray { "Limpia", U ("Cálida"), "Agresiva", "Espaciosa", "Vintage" };
        addAndMakeVisible (close);
        close.onClick = [this] { if (onClose) onClose(); };
    }
    std::vector<const Entry*> visible() const
    {
        std::vector<const Entry*> v;
        for (auto& e : all)
        {
            if (group == 5) { if (e.factory < 0) v.push_back (&e); continue; }
            if (group == 0 || tag.isEmpty() || e.tags.contains (tag)) v.push_back (&e);
        }
        return v;
    }
    juce::Rectangle<int> groupArea() const { return { 20, 60, 150, getHeight() - 80 }; }
    juce::Rectangle<int> tagArea() const   { return { 180, 60, 170, getHeight() - 80 }; }
    juce::Rectangle<int> listArea() const  { return { 360, 60, getWidth() - 380, getHeight() - 80 }; }
    static constexpr int rowH = 30;

    void resized() override { close.setBounds (getWidth() - 120, 14, 100, 30); }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillRoundedRectangle (b.translated (0, 6), 10);
        g.setColour (Col::header); g.fillRoundedRectangle (b, 10);
        g.setColour (Col::amber.withAlpha (0.5f)); g.drawRoundedRectangle (b.reduced (0.5f), 10, 1.5f);
        g.setFont (font (20.0f, true)); g.setColour (Col::amber);
        g.drawText ("PRESET BROWSER", 20, 14, 300, 30, juce::Justification::centredLeft);

        auto row = [&] (juce::Rectangle<int> r, const juce::String& t, bool sel, bool hov, float fs)
        {
            if (sel) { g.setColour (Col::amber.withAlpha (0.85f)); g.fillRoundedRectangle (r.toFloat().reduced (1), 4); }
            else if (hov) { g.setColour (juce::Colours::white.withAlpha (0.07f)); g.fillRoundedRectangle (r.toFloat().reduced (1), 4); }
            g.setFont (font (fs, sel)); g.setColour (sel ? Col::header : Col::cream);
            g.drawText (t, r.reduced (10, 0), juce::Justification::centredLeft, true);
        };
        auto ga = groupArea();
        for (int i = 0; i < groups.size(); ++i) row (ga.withHeight (rowH).translated (0, i * rowH), groups[i], i == group, false, 13.5f);
        auto ta = tagArea();
        g.setColour (juce::Colours::black.withAlpha (0.25f)); g.fillRoundedRectangle (ta.toFloat(), 6);
        if (groupTags.count (groups[group]))
        {
            const auto& tags = groupTags[groups[group]];
            for (int i = 0; i < tags.size(); ++i) row (ta.withHeight (26).translated (0, i * 26), tags[i], tags[i] == tag, i == hoverTag, 12.5f);
        }
        auto la = listArea();
        g.setColour (juce::Colours::black.withAlpha (0.25f)); g.fillRoundedRectangle (la.toFloat(), 6);
        const auto v = visible();
        g.saveState(); g.reduceClipRegion (la);
        for (int i = 0; i < (int) v.size(); ++i)
        {
            auto r = la.withHeight (rowH).translated (0, (i - scroll) * rowH);
            const bool cur = v[(size_t) i]->factory >= 0 && v[(size_t) i]->factory == proc.getCurrentProgram();
            row (r, v[(size_t) i]->name, cur, i == hoverRow, 14.0f);
            g.setFont (font (10.5f)); g.setColour ((cur ? Col::header : Col::cream).withAlpha (0.6f));
            g.drawText (v[(size_t) i]->tags.joinIntoString (U (" · ")), r.reduced (12, 0), juce::Justification::centredRight, true);
        }
        g.restoreState();
        if (v.empty())
        {
            g.setFont (font (13.0f)); g.setColour (Col::cream.withAlpha (0.6f));
            g.drawText (group == 5 ? U ("Aún no tienes presets guardados (botón GUARDAR)") : U ("Sin presets en esta categoría"), la, juce::Justification::centred);
        }
        g.setFont (font (11.0f)); g.setColour (Col::cream.withAlpha (0.5f));
        g.drawText (U ("clic = cargar y seguir probando · doble clic = cargar y cerrar"), 360, getHeight() - 18, getWidth() - 380, 14, juce::Justification::centredLeft);
    }
    int rowAt (juce::Point<int> p) const { const auto la = listArea(); return la.contains (p) ? (p.y - la.getY()) / rowH + scroll : -1; }
    void mouseMove (const juce::MouseEvent& e) override
    {
        hoverRow = rowAt (e.getPosition());
        const auto ta = tagArea(); hoverTag = ta.contains (e.getPosition()) ? (e.y - ta.getY()) / 26 : -1;
        repaint();
    }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        const int maxS = std::max (0, (int) visible().size() - listArea().getHeight() / rowH);
        scroll = juce::jlimit (0, maxS, scroll + (w.deltaY < 0 ? 1 : -1)); repaint();
    }
    void load (int i, bool closeAfter)
    {
        const auto v = visible();
        if (i < 0 || i >= (int) v.size()) return;
        proc.undoManager.beginNewTransaction();
        if (v[(size_t) i]->factory >= 0) proc.setCurrentProgram (v[(size_t) i]->factory);
        else proc.loadUserPreset (v[(size_t) i]->file);
        if (onLoaded) onLoaded();
        if (closeAfter && onClose) onClose();
        repaint();
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto p = e.getPosition();
        if (groupArea().contains (p)) { group = juce::jlimit (0, groups.size() - 1, (p.y - groupArea().getY()) / rowH); tag = {}; scroll = 0; repaint(); return; }
        if (tagArea().contains (p) && groupTags.count (groups[group]))
        {
            const auto& tags = groupTags[groups[group]];
            const int i = (p.y - tagArea().getY()) / 26;
            if (i < tags.size()) { tag = tags[i] == tag ? juce::String() : tags[i]; scroll = 0; repaint(); }
            return;
        }
        load (rowAt (p), false);
    }
    void mouseDoubleClick (const juce::MouseEvent& e) override { load (rowAt (e.getPosition()), true); }
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

    for (auto* b : { &modulesBtn, &undoBtn, &redoBtn, &resetBtn, &prevBtn, &nextBtn, &saveBtn, &loadBtn, &optionsBtn, &browserBtn })
        addAndMakeVisible (b);
    optionsBtn.onClick = [this] { showOptionsMenu(); };
    optionsBtn.setTooltip (U ("OPCIONES\nCalidad (Eco / Normal / High), tema, MIDI y exportar / importar configuración"));
    browserBtn.onClick = [this] { showBrowser(); };
    browserBtn.setTooltip (U ("PRESET BROWSER\nPresets por género, artista, tipo de voz y estilo de mezcla"));
    modulesBtn.setTooltip (U ("Mostrar u ocultar módulos"));
    saveBtn.setTooltip (U ("Guardar tus ajustes como preset de usuario"));
    loadBtn.setTooltip (U ("Cargar un preset desde un archivo"));


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
    modulesBtn.setBounds (360, 10, 100, 28);
    for (int i = 0; i < scaleBtns.size(); ++i) scaleBtns[i]->setBounds (466 + i * 45, 10, 43, 28);
    optionsBtn.setBounds (648, 10, 92, 28);
    undoBtn.setBounds (746, 10, 56, 28);
    redoBtn.setBounds (806, 10, 56, 28);
    resetBtn.setBounds (866, 10, 62, 28);
    const int right = getWidth() - 12;
    loadBtn.setBounds (right - 96, 10, 96, 28);
    saveBtn.setBounds (right - 196, 10, 96, 28);
    nextBtn.setBounds (right - 232, 10, 30, 28);
    browserBtn.setBounds (right - 316, 10, 80, 28);
    presetBox.setBounds (right - 486, 10, 166, 28);
    prevBtn.setBounds (right - 520, 10, 30, 28);
    // Si no cabe todo, los presets ocupan el espacio de la derecha de RESET
    if (prevBtn.getX() < 936)
    {
        const int shift = 936 - prevBtn.getX();
        for (auto* c : std::initializer_list<juce::Component*> { &prevBtn, &presetBox, &browserBtn, &nextBtn, &saveBtn, &loadBtn })
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
    drawLogo (g, { 14, 6, 236, 36 });
    // Latencia y calidad
    const double ms = proc.getSampleRate() > 0 ? 1000.0 * proc.getLatencySamples() / proc.getSampleRate() : 0.0;
    g.setFont (font (10.5f, true)); g.setColour (Col::amber.withAlpha (0.85f));
    g.drawText (U ("LATENCIA ") + juce::String (ms, 1) + " ms", 262, 9, 94, 14, juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xffb9b6ad));
    g.drawText (U ("CALIDAD ") + TopVocalChainProcessor::qualityNames()[(int) proc.apvts.getRawParameterValue ("quality")->load()].toUpperCase(),
                262, 25, 94, 14, juce::Justification::centredLeft);
}

// MIDI Learn: clic derecho en cualquier perilla
void MainView::showKnobMenu (cui::Knob& k)
{
    const auto id = k.getParamId();
    const int cc = proc.ccForParam (id);
    juce::PopupMenu m;
    m.addSectionHeader ("MIDI");
    m.addItem (1, cc >= 0 ? U ("MIDI Learn (ahora en CC ") + juce::String (cc) + ")" : juce::String ("MIDI Learn"));
    m.addItem (2, "Quitar MIDI", cc >= 0);
    m.addSeparator();
    m.addItem (3, "Valor por defecto");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&k),
                     [this, id, sp = juce::Component::SafePointer<cui::Knob> (&k)] (int r)
                     {
                         if (r == 1) proc.startMidiLearn (id);
                         if (r == 2) { proc.clearMidi (id); if (sp) sp->repaint(); }
                         if (r == 3) if (auto* prm = proc.apvts.getParameter (id)) prm->setValueNotifyingHost (prm->getDefaultValue());
                     });
}

void MainView::applyTheme()
{
    cui::setTheme (proc.uiTheme.load());
    for (auto& m : modules) m->invalidateCache();
    repaint();
}

void MainView::showOptionsMenu()
{
    juce::PopupMenu q, theme, midi, m;
    const int cq = (int) proc.apvts.getRawParameterValue ("quality")->load();
    q.addItem (10, U ("Eco (menos CPU, sin oversampling)"), true, cq == 0);
    q.addItem (11, U ("Normal (oversampling 2x)"), true, cq == 1);
    q.addItem (12, U ("High (oversampling 4x, saturación más limpia)"), true, cq == 2);
    theme.addItem (20, U ("Hardware (claro)"), true, proc.uiTheme.load() == 0);
    theme.addItem (21, U ("Oscuro"), true, proc.uiTheme.load() == 1);
    midi.addItem (30, U ("Cómo usar: clic derecho en una perilla → MIDI Learn → mueve un control de tu teclado"), false);
    midi.addItem (31, U ("Quitar todas las asignaciones MIDI"));
    m.addSubMenu ("Calidad", q);
    m.addSubMenu ("Tema", theme);
    m.addSubMenu ("MIDI Learn", midi);
    m.addSeparator();
    m.addItem (40, U ("Exportar configuración..."));
    m.addItem (41, U ("Importar configuración..."));
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&optionsBtn), [this] (int r)
    {
        auto set = [this] (const char* id, float v) { auto* prm = proc.apvts.getParameter (id); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); };
        if (r >= 10 && r <= 12) set ("quality", (float) (r - 10));
        if (r == 20 || r == 21) { proc.uiTheme.store (r - 20); applyTheme(); }
        if (r == 31) { for (auto& mm : proc.midiMap) mm.store (-1); repaint(); for (auto& mod : modules) mod->repaint(); }
        if (r == 40)
        {
            chooser = std::make_unique<juce::FileChooser> (U ("Exportar configuración"), juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                                               .getChildFile ("Top Vocal Chain.tvcconfig"), "*.tvcconfig");
            chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                      | juce::FileBrowserComponent::warnAboutOverwriting,
                                  [this] (const juce::FileChooser& fc)
                                  {
                                      const auto f = fc.getResult();
                                      if (f != juce::File())
                                          if (auto xml = proc.exportConfig().createXml()) xml->writeTo (f.withFileExtension ("tvcconfig"));
                                  });
        }
        if (r == 41)
        {
            chooser = std::make_unique<juce::FileChooser> (U ("Importar configuración"), juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.tvcconfig");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [this] (const juce::FileChooser& fc)
                                  {
                                      if (auto xml = juce::XmlDocument::parse (fc.getResult()))
                                          if (proc.importConfig (juce::ValueTree::fromXml (*xml)))
                                          { applyTheme(); refreshPresetBox(); if (onSizeChange) onSizeChange(); }
                                  });
        }
    });
}

void MainView::showBrowser()
{
    auto panel = std::make_unique<PresetBrowser> (proc);
    juce::Component::SafePointer<MainView> safeThis (this);
    panel->onClose = [safeThis] { juce::MessageManager::callAsync ([safeThis] { if (safeThis != nullptr) safeThis->browserPanel.reset(); }); };
    panel->onLoaded = [safeThis] { if (safeThis != nullptr) safeThis->refreshPresetBox(); };
    panel->setBounds (getLocalBounds().withSizeKeepingCentre (820, 560));
    addAndMakeVisible (*panel);
    browserPanel = std::move (panel);
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
    if (proc.uiTheme.load() != lastTheme) { lastTheme = proc.uiTheme.load(); applyTheme(); }
    if (proc.getLatencySamples() != lastLatency) { lastLatency = proc.getLatencySamples(); repaint (250, 0, 110, 48); }
    if (proc.lastLearnedCc.exchange (-1) >= 0) for (auto& mod : modules) mod->repaint();
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
    tooltips.setLookAndFeel (&lnf);
    addAndMakeVisible (view);
    view.onSizeChange = [this] { updateSize(); };
    setResizable (false, false);
    updateSize();
    startTimerHz (30);
}

TopVocalChainEditor::~TopVocalChainEditor()
{
    stopTimer();
    tooltips.setLookAndFeel (nullptr);
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
