#include "PluginEditor.h"

#include <algorithm>

#include "DeviceInfo.h"

namespace
{
using namespace hush::colours;

juce::String pt (const char* utf8) { return juce::String::fromUTF8 (utf8); }

juce::String dbText (float db)
{
    return db <= -99.0f ? pt ("-∞ dB") : juce::String (db, 1) + " dB";
}

constexpr int kPad = 20;
constexpr int kColW = 680;     // largura útil de cada coluna na janela inicial
constexpr int kMinColW = 640;  // abaixo disto uma coluna não cabe mais: o layout reduz o número de colunas
constexpr int kMaxCols = 3;
constexpr int kColGap = 20;    // espaço horizontal entre colunas
constexpr int kWidth = kPad * 2 + kColW * 2 + kColGap;
constexpr int kMinWidth = kPad * 2 + kMinColW;
constexpr int kMinHeight = 420;
constexpr int kDragStartPx = 6;
constexpr int kHeaderH = 56;
constexpr int kLatencyH = 118;
constexpr int kMeterH = 108;
constexpr int kKnobH = 196;
constexpr int kPedalH = PedalBoard::kHeight + 24;
constexpr int kAmpH = AmpPanel::kHeight + 36;
constexpr int kRecordH = 84;
constexpr int kGap = 12;
constexpr int kLabelW = 78;
constexpr int kReadoutW = 78;

void drawCard (juce::Graphics& g, juce::Rectangle<int> area)
{
    const auto r = area.toFloat();
    g.setColour (card);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (border);
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);

    // Pontinhos no topo: indicam que o card pode ser arrastado.
    g.setColour (border.brighter (0.5f));
    for (int i = -2; i <= 2; ++i)
        g.fillEllipse (r.getCentreX() + static_cast<float> (i) * 7.0f - 1.5f, r.getY() + 4.0f, 3.0f, 3.0f);
}

void drawSectionTitle (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area,
                       juce::Justification just = juce::Justification::centredLeft)
{
    g.setColour (textDim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText (text, area, just);
}

const std::array<int, hushrig::kNumCards>& cardHeights()
{
    static const std::array<int, hushrig::kNumCards> heights { kLatencyH, kMeterH, kKnobH, kRecordH, kPedalH, kAmpH };
    return heights;
}

hushrig::CardLayout::Result flowCards (const std::vector<int>& order, int areaWidth)
{
    return hushrig::CardLayout::flow (order, cardHeights(), areaWidth, kColGap, kGap, kMinColW, kMaxCols);
}
} // namespace

HushRigEditor::HushRigEditor (HushRigProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      showUpdateSection (p.wrapperType == juce::AudioProcessor::wrapperType_Standalone),
      pedalBoard (p),
      ampPanel (p)
{
    setLookAndFeel (&lnf);

    addKnob (knobs[0], "INPUT",     "inputGain",     " dB");
    addKnob (knobs[1], "THRESHOLD", "gateThreshold", " dB");
    addKnob (knobs[2], "HOLD",      "gateHold",      " ms");
    addKnob (knobs[3], "RELEASE",   "gateRelease",   " ms");
    addKnob (knobs[4], "OUTPUT",    "outputGain",    " dB");

    bypassButton.setButtonText ("Bypass");
    addAndMakeVisible (bypassButton);
    bypassAttachment = std::make_unique<ButtonAttachment> (processor.apvts, "gateBypass", bypassButton);

    addAndMakeVisible (pedalBoard);
    addAndMakeVisible (ampPanel);
    addAndMakeVisible (inputMeter);
    addAndMakeVisible (outputMeter);

    setupRecordSection();

    if (showUpdateSection)
        setupUpdater();

    cardOrder = hushrig::CardLayout::fromString (processor.apvts.state.getProperty ("cardOrder").toString().toStdString());

    // Janela redimensionável: os cards se reorganizam em 1, 2 ou 3 colunas conforme a largura.
    setResizable (true, ! showUpdateSection); // no standalone a própria janela já tem bordas redimensionáveis
    setResizeLimits (kMinWidth, kMinHeight, 4000, 3200);

    const int height = kPad * 2 + kHeaderH + kGap + flowCards (cardOrder, kWidth - kPad * 2).contentHeight;
    setSize (kWidth, height);

    updateLatencyView();
    tick (true);
    startTimerHz (30);
}

HushRigEditor::~HushRigEditor()
{
    stopTimer();

    processor.recorder.stop();

    processor.updater.onCheckDone = nullptr;
    processor.updater.onProgress = nullptr;
    processor.updater.onInstallerLaunched = nullptr;

    if (badgeWindow != nullptr)
    {
        badgeWindow->removeComponentListener (this);
        badgeWindow->removeChildComponent (&updateBadge);
    }

    setLookAndFeel (nullptr);
}

bool HushRigEditor::isGateBypassed() const
{
    return processor.apvts.getRawParameterValue ("gateBypass")->load() >= 0.5f;
}

void HushRigEditor::addKnob (Knob& knob, const juce::String& caption, const char* paramId, const juce::String& suffix)
{
    knob.caption.setText (caption, juce::dontSendNotification);
    knob.caption.setJustificationType (juce::Justification::centred);
    knob.caption.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
    addAndMakeVisible (knob.caption);

    knob.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 84, 20);
    knob.slider.setTextValueSuffix (suffix);
    // Definido por slider para garantir que a caixa de valor siga o tema.
    knob.slider.setColour (juce::Slider::textBoxTextColourId, hush::colours::text);
    knob.slider.setColour (juce::Slider::textBoxBackgroundColourId, track);
    knob.slider.setColour (juce::Slider::textBoxOutlineColourId, border);
    addAndMakeVisible (knob.slider);

    knob.attachment = std::make_unique<SliderAttachment> (processor.apvts, paramId, knob.slider);
}

void HushRigEditor::setupRecordSection()
{
    recordButton.setButtonText ("Gravar");
    recordButton.onClick = [this] { toggleRecording(); };
    addAndMakeVisible (recordButton);

    folderButton.setButtonText ("Abrir pasta");
    folderButton.onClick = [this]
    {
        auto folder = hushrig::Recorder::recordingsFolder();
        folder.createDirectory();
        folder.startAsProcess();
    };
    addAndMakeVisible (folderButton);

    recordLabel.setJustificationType (juce::Justification::centredLeft);
    recordLabel.setMinimumHorizontalScale (1.0f);
    addAndMakeVisible (recordLabel);

    updateRecordView();
}

void HushRigEditor::toggleRecording()
{
    if (processor.recorder.isRecording())
    {
        processor.recorder.stop();
    }
    else
    {
        juce::String error;

        if (! processor.recorder.start (processor.sampleRateHz.load(), error))
        {
            recordLabel.setColour (juce::Label::textColourId, bad);
            recordLabel.setText (error, juce::dontSendNotification);
            return;
        }
    }

    updateRecordView();
}

void HushRigEditor::updateRecordView()
{
    const bool recording = processor.recorder.isRecording();

    if (recording)
    {
        const double sr = std::max (1.0, processor.sampleRateHz.load());
        const int secs = static_cast<int> (static_cast<double> (processor.recorder.getSamplesWritten()) / sr);

        recordButton.setButtonText ("Parar");
        recordButton.setColour (juce::TextButton::buttonColourId, bad.darker (0.3f));
        recordLabel.setColour (juce::Label::textColourId, bad);
        recordLabel.setText (pt ("● Gravando  ") + juce::String::formatted ("%02d:%02d", secs / 60, secs % 60)
                                 + "  -  " + processor.recorder.getFile().getFileName(),
                             juce::dontSendNotification);
    }
    else
    {
        recordButton.setButtonText ("Gravar");
        recordButton.removeColour (juce::TextButton::buttonColourId);

        if (wasRecording) // acabou de parar
        {
            recordLabel.setColour (juce::Label::textColourId, good);
            recordLabel.setText (pt ("Salvo em ") + processor.recorder.getFile().getFullPathName(),
                                 juce::dontSendNotification);
        }
        else if (recordLabel.getText().isEmpty())
        {
            recordLabel.setColour (juce::Label::textColourId, textDim);
            recordLabel.setText (pt ("Grava o áudio já processado (WAV, 24 bits) em Documentos\\HushRig."),
                                 juce::dontSendNotification);
        }
    }

    wasRecording = recording;
}

void HushRigEditor::setupUpdater()
{
    updateBadge.setVisible (false);

    updateBadge.onClick = [this]
    {
        updateBadge.setDownloading (true);
        progress = -1.0;
        updateBadge.setProgress (progress);
        processor.updater.downloadAndInstall (pendingRelease);
    };

    auto& updater = processor.updater;

    updater.onCheckDone = [this] (Updater::CheckResult result, const Updater::ReleaseInfo& info, const juce::String&)
    {
        // Falhas na busca automática são silenciosas: só aparece algo quando há versão nova.
        if (result != Updater::CheckResult::updateAvailable)
            return;

        pendingRelease = info;
        updateBadge.setAvailable (info.version);
        layoutUpdateBadge();
    };

    updater.onProgress = [this] (double p)
    {
        progress = p;
        updateBadge.setProgress (p);
    };

    updater.onInstallerLaunched = [this] (bool ok, const juce::String& message)
    {
        if (ok)
        {
            // O instalador precisa substituir o HushRig.exe, então este app se encerra.
            juce::JUCEApplicationBase::quit();
            return;
        }

        updateBadge.setDownloading (false);
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, pt ("Atualização"), message);
    };

   #if ! HUSHRIG_NO_STANDALONE_HOLDER
    // Procura atualizações uma vez a cada abertura do app.
    static bool checkedThisRun = false;

    if (! checkedThisRun)
    {
        checkedThisRun = true;
        updater.checkForUpdates();
    }
   #endif
}

void HushRigEditor::parentHierarchyChanged()
{
    attachUpdateBadge();
}

// O botão vive na barra de título da janela standalone (ao lado de minimizar/fechar), não dentro do editor.
void HushRigEditor::attachUpdateBadge()
{
    if (! showUpdateSection)
        return;

    auto* window = findParentComponentOfClass<juce::DocumentWindow>();

    if (window == badgeWindow.getComponent())
        return;

    if (badgeWindow != nullptr)
    {
        badgeWindow->removeComponentListener (this);
        badgeWindow->removeChildComponent (&updateBadge);
    }

    badgeWindow = window;

    if (window != nullptr)
    {
        // O standalone só traz minimizar e fechar; o botão quadrado maximiza/restaura a janela.
        window->setTitleBarButtonsRequired (juce::DocumentWindow::allButtons, false);
        window->addChildComponent (updateBadge);
        window->addComponentListener (this);
        layoutUpdateBadge();
    }
}

void HushRigEditor::layoutUpdateBadge()
{
    if (badgeWindow == nullptr)
        return;

    const auto title = badgeWindow->getLocalBounds().removeFromTop (badgeWindow->getTitleBarHeight());
    const int w = updateBadge.getIdealWidth();
    int right = title.getRight() - 8;

    if (auto* minimise = badgeWindow->getMinimiseButton())
        right = minimise->getX() - 10;
    else if (auto* close = badgeWindow->getCloseButton())
        right = close->getX() - 10;

    updateBadge.setBounds (right - w, title.getCentreY() - UpdateBadge::kHeight / 2, w, UpdateBadge::kHeight);
    updateBadge.toFront (false);
}

void HushRigEditor::updateLatencyView()
{
    const auto stats = queryDeviceStats (processor.sampleRateHz.load(), processor.blockSize.load());
    const double sr = stats.sampleRate > 0.0 ? stats.sampleRate : 48000.0;
    const auto toMs = [sr] (int samples) { return 1000.0 * static_cast<double> (samples) / sr; };

    const double bufferMs = toMs (stats.bufferSamples);
    const auto kHz = juce::String (sr / 1000.0, 1) + " kHz";

    if (! stats.fromDevice)
    {
        latency.big = "0.0 ms";
        latency.badge = pt ("Adicionado pelo HushRig");
        latency.colour = good;
        latency.device = pt ("Rodando como plugin (ou sem dispositivo de áudio)");
        latency.details = stats.bufferSamples > 0
                              ? pt ("Buffer do host: ") + juce::String (stats.bufferSamples) + pt (" amostras (")
                                    + juce::String (bufferMs, 1) + " ms) · " + kHz
                              : pt ("Aguardando o áudio iniciar...");
        latency.tip = pt ("O gate não usa lookahead. A latência total depende do seu DAW e da interface de áudio.");
        return;
    }

    const double inMs = toMs (stats.inputLatencySamples);
    const double outMs = toMs (stats.outputLatencySamples);

    bool estimated = false;
    double total = inMs + outMs;

    if (total < bufferMs) // o driver não informou as latências: assume buffer de entrada + de saída
    {
        total = 2.0 * bufferMs;
        estimated = true;
    }

    latency.big = (estimated ? "~" : "") + juce::String (total, 1) + " ms";

    const auto slowTip = pt ("Reduza o buffer em Options (ícone de engrenagem) ou escolha um driver ASIO / FlexASIO.");

    if (total <= 10.0)      { latency.badge = "Excelente";             latency.colour = good;  latency.tip = pt ("Imperceptível ao tocar."); }
    else if (total <= 20.0) { latency.badge = "Boa";                   latency.colour = okay;  latency.tip = pt ("Ainda confortável para a maioria dos casos."); }
    else if (total <= 30.0) { latency.badge = pt ("Perceptível");      latency.colour = warn;  latency.tip = slowTip; }
    else                    { latency.badge = "Alta";                  latency.colour = bad;   latency.tip = slowTip; }

    latency.device = stats.deviceName + " (" + stats.apiName + ")";
    latency.details = pt ("Entrada ") + juce::String (inMs, 1) + pt (" ms · Saída ") + juce::String (outMs, 1)
                      + pt (" ms · Buffer ") + juce::String (stats.bufferSamples) + " (" + juce::String (bufferMs, 1)
                      + " ms) · " + kHz + " · CPU " + juce::String (static_cast<int> (stats.cpuUsage * 100.0)) + "%"
                      + pt (" · HushRig adiciona 0 ms");
}

void HushRigEditor::tick (bool refreshStats)
{
    constexpr float fallDbPerTick = 40.0f / 30.0f;

    const float inDb  = juce::Decibels::gainToDecibels (processor.inputPeak.exchange (0.0f), -100.0f);
    const float outDb = juce::Decibels::gainToDecibels (processor.outputPeak.exchange (0.0f), -100.0f);

    shownInputDb  = inDb  > shownInputDb  ? inDb  : std::max (inDb,  shownInputDb  - fallDbPerTick);
    shownOutputDb = outDb > shownOutputDb ? outDb : std::max (outDb, shownOutputDb - fallDbPerTick);

    inputMeter.setLevelDb (shownInputDb);
    outputMeter.setLevelDb (shownOutputDb);
    inputMeter.setThreshold (processor.apvts.getRawParameterValue ("gateThreshold")->load(), ! isGateBypassed());

    shownGateOpen = processor.gateOpen.load (std::memory_order_relaxed);
    repaint (meterCard);
    updateRecordView();
    pedalBoard.refresh();
    ampPanel.refresh();

    if (refreshStats || ++tickCount % 15 == 0)
    {
        updateLatencyView();
        repaint (latencyCard);
    }
}

void HushRigEditor::paint (juce::Graphics& g)
{
    g.fillAll (background);

    // --- Cabeçalho -------------------------------------------------------
    g.setColour (accentBright);
    g.setFont (juce::FontOptions (32.0f, juce::Font::bold));
    g.drawText ("HushRig", headerArea, juce::Justification::centredLeft);

    g.setColour (textDim);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText (pt ("v") + Updater::currentVersion() + pt (" · rig de guitarra open source"),
                headerArea, juce::Justification::centredRight);

    // --- Latência ----------------------------------------------------------
    drawCard (g, latencyCard);
    {
        auto in = latencyCard.reduced (18, 14);
        auto left = in.removeFromLeft (240);
        in.removeFromLeft (16);

        drawSectionTitle (g, pt ("LATÊNCIA"), left.removeFromTop (14));

        g.setColour (latency.colour);
        g.setFont (juce::FontOptions (46.0f, juce::Font::bold));
        g.drawText (latency.big, left.removeFromTop (56), juce::Justification::centredLeft);

        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        g.drawText (latency.badge, left.removeFromTop (20), juce::Justification::centredLeft);

        g.setColour (accentBright);
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        g.drawText (latency.device, in.removeFromTop (22), juce::Justification::centredLeft);

        g.setColour (textDim);
        g.setFont (juce::FontOptions (12.0f));
        g.drawFittedText (latency.details, in.removeFromTop (38), juce::Justification::topLeft, 2);

        g.setColour (hush::colours::text);
        g.drawFittedText (latency.tip, in, juce::Justification::topLeft, 2);
    }

    // --- Níveis ------------------------------------------------------------
    drawCard (g, meterCard);
    {
        auto titleRow = meterCard.reduced (16, 12).removeFromTop (20);
        drawSectionTitle (g, pt ("NÍVEIS"), titleRow);

        const bool bypassed = isGateBypassed();
        auto gateRect = titleRow.removeFromRight (140);

        g.setColour (bypassed ? textDim : (shownGateOpen ? good : border.brighter (0.4f)));
        g.fillEllipse (static_cast<float> (gateRect.getX()), static_cast<float> (gateRect.getCentreY()) - 5.0f, 10.0f, 10.0f);

        g.setColour (bypassed ? textDim : (shownGateOpen ? good : textDim));
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (bypassed ? "GATE DESLIGADO" : (shownGateOpen ? "GATE ABERTO" : "GATE FECHADO"),
                    gateRect.withTrimmedLeft (16), juce::Justification::centredLeft);

        const auto drawRow = [&g] (juce::Rectangle<int> row, const juce::String& name, float db)
        {
            g.setColour (hush::colours::text);
            g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
            g.drawText (name, row.withWidth (kLabelW), juce::Justification::topLeft);

            g.setColour (accentBright);
            g.drawText (dbText (db), row.removeFromRight (kReadoutW), juce::Justification::topRight);
        };

        drawRow (inputRow, "ENTRADA", shownInputDb);
        drawRow (outputRow, pt ("SAÍDA"), shownOutputDb);
    }

    // --- Controles ---------------------------------------------------------
    drawCard (g, knobCard);
    {
        const int colW = knobInner.getWidth() / 5;
        auto titleRow = knobInner.withHeight (18);

        drawSectionTitle (g, "ENTRADA", titleRow.withWidth (colW), juce::Justification::centred);
        drawSectionTitle (g, "NOISE GATE", titleRow.withX (knobInner.getX() + colW).withWidth (colW * 3 - 100),
                          juce::Justification::centred);
        drawSectionTitle (g, pt ("SAÍDA"), titleRow.withX (knobInner.getX() + colW * 4).withWidth (colW),
                          juce::Justification::centred);

        g.setColour (border);
        for (const int col : { 1, 4 })
            g.fillRect (knobInner.getX() + colW * col, knobInner.getY(), 1, knobInner.getHeight());
    }

    // --- Pedais ------------------------------------------------------------
    drawCard (g, pedalCard);

    // --- Amp ---------------------------------------------------------------
    drawCard (g, ampCard);
    drawSectionTitle (g, pt ("AMP · GANHO E TOM"), ampCard.reduced (16, 12).removeFromTop (16));

    // --- Gravação ----------------------------------------------------------
    drawCard (g, recordCard);
    drawSectionTitle (g, pt ("GRAVAÇÃO"), recordCard.reduced (16, 12).removeFromTop (16));
}

void HushRigEditor::resized()
{
    auto area = getLocalBounds().reduced (kPad);

    headerArea = area.removeFromTop (kHeaderH);
    area.removeFromTop (kGap);

    for (const auto& p : flowCards (cardOrder, area.getWidth()).cards)
    {
        const juce::Rectangle<int> rect (area.getX() + p.x, area.getY() + p.y, p.w, p.h);
        cardRects[static_cast<size_t> (p.card)] = rect;

        switch (static_cast<hushrig::Card> (p.card))
        {
            case hushrig::Card::latency: latencyCard = rect; break;
            case hushrig::Card::meters:  meterCard = rect;   break;
            case hushrig::Card::knobs:   knobCard = rect;    break;
            case hushrig::Card::record:  recordCard = rect;  break;
            case hushrig::Card::pedals:  pedalCard = rect;   break;
            case hushrig::Card::amp:     ampCard = rect;     break;
        }
    }

    // Medidores
    auto m = meterCard.reduced (16, 12);
    m.removeFromTop (22);
    inputRow = m.removeFromTop (34);
    outputRow = m.removeFromTop (34);
    inputMeter.setBounds (inputRow.withTrimmedLeft (kLabelW).withTrimmedRight (kReadoutW + 6));
    outputMeter.setBounds (outputRow.withTrimmedLeft (kLabelW).withTrimmedRight (kReadoutW + 6));

    pedalBoard.setBounds (pedalCard.reduced (16, 12));
    ampPanel.setBounds (ampCard.reduced (16, 12).withTrimmedTop (22));

    // Knobs
    knobInner = knobCard.reduced (16, 14);
    auto k = knobInner;
    k.removeFromTop (22);
    const int colW = knobInner.getWidth() / 5;

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto col = k.withX (k.getX() + static_cast<int> (i) * colW).withWidth (colW);
        knobs[i].caption.setBounds (col.removeFromTop (18));
        knobs[i].slider.setBounds (col.removeFromTop (124).reduced (6, 0));
    }

    bypassButton.setBounds (juce::Rectangle<int> (knobInner.getX() + colW * 4 - 92, knobInner.getY() - 1, 90, 20));

    auto r = recordCard.reduced (16, 12);
    r.removeFromTop (22);
    auto rbuttons = r.removeFromTop (32);
    recordButton.setBounds (rbuttons.removeFromLeft (150));
    rbuttons.removeFromLeft (8);
    folderButton.setBounds (rbuttons.removeFromLeft (120));
    rbuttons.removeFromLeft (14);
    recordLabel.setBounds (rbuttons);
}

// --- Arrastar cards ----------------------------------------------------------
// Pegue um card por qualquer área livre dele (fora dos controles) e solte em outra posição.

int HushRigEditor::cardAt (juce::Point<int> p) const
{
    for (int i = 0; i < hushrig::kNumCards; ++i)
        if (cardRects[static_cast<size_t> (i)].contains (p))
            return i;

    return -1;
}

void HushRigEditor::mouseDown (const juce::MouseEvent& e)
{
    dragCard = cardAt (e.getPosition());
    isDraggingCard = false;
    dropTarget = -1;
}

void HushRigEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (dragCard < 0)
        return;

    if (! isDraggingCard && e.getDistanceFromDragStart() > kDragStartPx)
    {
        isDraggingCard = true;
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    }

    if (isDraggingCard)
    {
        updateDropTarget (e.getPosition());
        repaint();
    }
}

void HushRigEditor::updateDropTarget (juce::Point<int> p)
{
    // O card mais próximo do ponteiro (pela distância ao centro) é a referência; acima do centro = antes dele.
    int best = -1;
    float bestDistance = 0.0f;

    for (int i = 0; i < hushrig::kNumCards; ++i)
    {
        if (i == dragCard)
            continue;

        const auto rect = cardRects[static_cast<size_t> (i)];
        const float d = rect.getCentre().toFloat().getDistanceFrom (p.toFloat());

        if (rect.contains (p) || best < 0 || d < bestDistance)
        {
            best = i;
            bestDistance = rect.contains (p) ? -1.0f : d;
        }
    }

    dropTarget = best;
    dropBefore = best < 0 || p.y < cardRects[static_cast<size_t> (best)].getCentreY();
}

void HushRigEditor::applyCardDrop()
{
    if (dragCard < 0 || dropTarget < 0)
        return;

    auto order = cardOrder;
    order.erase (std::remove (order.begin(), order.end(), dragCard), order.end());

    const auto at = std::find (order.begin(), order.end(), dropTarget);
    order.insert (dropBefore ? at : at + 1, dragCard);

    if (! hushrig::CardLayout::isValid (order) || order == cardOrder)
        return;

    cardOrder = order;
    processor.apvts.state.setProperty ("cardOrder", juce::String (hushrig::CardLayout::toString (order)), nullptr);
    resized();
}

void HushRigEditor::mouseUp (const juce::MouseEvent&)
{
    if (isDraggingCard)
        applyCardDrop();

    dragCard = -1;
    isDraggingCard = false;
    dropTarget = -1;
    setMouseCursor (juce::MouseCursor::NormalCursor);
    repaint();
}

void HushRigEditor::paintOverChildren (juce::Graphics& g)
{
    if (! isDraggingCard || dragCard < 0)
        return;

    // Card arrastado fica esmaecido, com o contorno em destaque.
    const auto dragged = cardRects[static_cast<size_t> (dragCard)].toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (dragged, 10.0f);
    g.setColour (accent);
    g.drawRoundedRectangle (dragged.reduced (0.5f), 10.0f, 2.0f);

    // Barra mostrando onde ele vai cair.
    if (dropTarget >= 0)
    {
        const auto target = cardRects[static_cast<size_t> (dropTarget)];
        const int y = dropBefore ? target.getY() - kGap / 2 - 1 : target.getBottom() + kGap / 2 - 1;
        g.setColour (accentBright);
        g.fillRoundedRectangle (static_cast<float> (target.getX()), static_cast<float> (y), static_cast<float> (target.getWidth()), 3.0f, 1.5f);
    }
}
