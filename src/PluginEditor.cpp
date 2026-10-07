#include "PluginEditor.h"

#include "DeviceInfo.h"

namespace
{
using namespace hush::colours;

juce::String pt (const char* utf8) { return juce::String::fromUTF8 (utf8); }

juce::String dbText (float db)
{
    return db <= -99.0f ? pt ("-∞ dB") : juce::String (db, 1) + " dB";
}

constexpr int kWidth = 720;
constexpr int kPad = 20;
constexpr int kHeaderH = 56;
constexpr int kLatencyH = 118;
constexpr int kMeterH = 108;
constexpr int kKnobH = 196;
constexpr int kPedalH = PedalBoard::kHeight + 24;
constexpr int kRecordH = 84;
constexpr int kUpdateH = 124;
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
}

void drawSectionTitle (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area,
                       juce::Justification just = juce::Justification::centredLeft)
{
    g.setColour (textDim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText (text, area, just);
}
} // namespace

HushRigEditor::HushRigEditor (HushRigProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      showUpdateSection (p.wrapperType == juce::AudioProcessor::wrapperType_Standalone),
      pedalBoard (p)
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
    addAndMakeVisible (inputMeter);
    addAndMakeVisible (outputMeter);

    setupRecordSection();

    if (showUpdateSection)
        setupUpdateSection();

    const int height = kPad * 2 + kHeaderH + kGap * 5 + kLatencyH + kMeterH + kKnobH + kPedalH + kRecordH
                       + (showUpdateSection ? kGap + kUpdateH : 0);
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

void HushRigEditor::setupUpdateSection()
{
    checkButton.setButtonText (pt ("Procurar atualizações"));
    addAndMakeVisible (checkButton);

    installButton.setButtonText (pt ("Baixar e instalar"));
    installButton.setVisible (false);
    addChildComponent (installButton);

    progressBar.setVisible (false);
    addChildComponent (progressBar);

    statusLabel.setJustificationType (juce::Justification::topLeft);
    statusLabel.setMinimumHorizontalScale (1.0f);
    addAndMakeVisible (statusLabel);

    auto& updater = processor.updater;

    checkButton.onClick = [this]
    {
        installButton.setVisible (false);
        progressBar.setVisible (false);
        checkButton.setEnabled (false);
        setStatus (pt ("Procurando atualizações..."));
        processor.updater.checkForUpdates();
    };

    installButton.onClick = [this]
    {
        installButton.setEnabled (false);
        checkButton.setEnabled (false);
        progress = -1.0;
        progressBar.setVisible (true);
        setStatus (pt ("Baixando a atualização..."));
        processor.updater.downloadAndInstall (pendingRelease);
    };

    updater.onCheckDone = [this] (Updater::CheckResult result, const Updater::ReleaseInfo& info, const juce::String& message)
    {
        checkButton.setEnabled (true);
        setStatus (message, result == Updater::CheckResult::failed);

        if (result == Updater::CheckResult::updateAvailable)
        {
            pendingRelease = info;
            installButton.setButtonText (pt ("Baixar e instalar v") + info.version);
            installButton.setEnabled (true);
            installButton.setVisible (true);
        }
    };

    updater.onProgress = [this] (double p) { progress = p; };

    updater.onInstallerLaunched = [this] (bool ok, const juce::String& message)
    {
        setStatus (message, ! ok);

        if (ok)
        {
            // O instalador precisa substituir o HushRig.exe, então este app se encerra.
            juce::JUCEApplicationBase::quit();
            return;
        }

        progressBar.setVisible (false);
        checkButton.setEnabled (true);
        installButton.setEnabled (true);
    };
}

void HushRigEditor::setStatus (const juce::String& text, bool isError)
{
    statusLabel.setColour (juce::Label::textColourId, isError ? bad : text.isEmpty() ? textDim : hush::colours::text);
    statusLabel.setText (text, juce::dontSendNotification);
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

    // --- Gravação ----------------------------------------------------------
    drawCard (g, recordCard);
    drawSectionTitle (g, pt ("GRAVAÇÃO"), recordCard.reduced (16, 12).removeFromTop (16));

    // --- Atualizações ------------------------------------------------------
    if (showUpdateSection)
    {
        drawCard (g, updateCard);
        drawSectionTitle (g, pt ("ATUALIZAÇÕES"), updateCard.reduced (16, 12).removeFromTop (16));
    }
}

void HushRigEditor::resized()
{
    auto area = getLocalBounds().reduced (kPad);

    headerArea = area.removeFromTop (kHeaderH);
    area.removeFromTop (kGap);
    latencyCard = area.removeFromTop (kLatencyH);
    area.removeFromTop (kGap);
    meterCard = area.removeFromTop (kMeterH);
    area.removeFromTop (kGap);
    knobCard = area.removeFromTop (kKnobH);
    area.removeFromTop (kGap);
    pedalCard = area.removeFromTop (kPedalH);
    area.removeFromTop (kGap);
    recordCard = area.removeFromTop (kRecordH);

    if (showUpdateSection)
    {
        area.removeFromTop (kGap);
        updateCard = area.removeFromTop (kUpdateH);
    }

    // Medidores
    auto m = meterCard.reduced (16, 12);
    m.removeFromTop (22);
    inputRow = m.removeFromTop (34);
    outputRow = m.removeFromTop (34);
    inputMeter.setBounds (inputRow.withTrimmedLeft (kLabelW).withTrimmedRight (kReadoutW + 6));
    outputMeter.setBounds (outputRow.withTrimmedLeft (kLabelW).withTrimmedRight (kReadoutW + 6));

    pedalBoard.setBounds (pedalCard.reduced (16, 12));

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

    if (! showUpdateSection)
        return;

    auto u = updateCard.reduced (16, 12);
    u.removeFromTop (22);

    auto buttons = u.removeFromTop (32);
    checkButton.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2 - 4));
    buttons.removeFromLeft (8);
    installButton.setBounds (buttons);

    u.removeFromTop (8);
    progressBar.setBounds (u.removeFromTop (14));
    u.removeFromTop (4);
    statusLabel.setBounds (u);
}
