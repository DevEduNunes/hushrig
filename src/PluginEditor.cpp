#include "PluginEditor.h"

namespace
{
juce::String pt (const char* utf8) { return juce::String::fromUTF8 (utf8); }

constexpr int kWidth = 480;
constexpr int kRowHeight = 32;
constexpr int kPad = 16;
constexpr int kTitleHeight = 40;
constexpr int kUpdateHeight = 150;
} // namespace

HushRigEditor::HushRigEditor (HushRigProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      showUpdateSection (p.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
{
    addRow (rows[0], "Input Gain",     "inputGain",     " dB");
    addRow (rows[1], "Gate Threshold", "gateThreshold", " dB");
    addRow (rows[2], "Gate Hold",      "gateHold",      " ms");
    addRow (rows[3], "Gate Release",   "gateRelease",   " ms");
    addRow (rows[4], "Output Gain",    "outputGain",    " dB");

    bypassButton.setButtonText ("Gate bypass");
    addAndMakeVisible (bypassButton);
    bypassAttachment = std::make_unique<ButtonAttachment> (processor.apvts, "gateBypass", bypassButton);

    if (showUpdateSection)
        setupUpdateSection();

    const int height = kPad * 2 + kTitleHeight + kRowHeight * 6 + (showUpdateSection ? kUpdateHeight : 0);
    setSize (kWidth, height);
}

HushRigEditor::~HushRigEditor()
{
    processor.updater.onCheckDone = nullptr;
    processor.updater.onProgress = nullptr;
    processor.updater.onInstallerLaunched = nullptr;
}

void HushRigEditor::addRow (Row& row, const juce::String& text, const char* paramId, const juce::String& suffix)
{
    row.label.setText (text, juce::dontSendNotification);
    row.label.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (row.label);

    row.slider.setSliderStyle (juce::Slider::LinearHorizontal);
    row.slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 80, 20);
    row.slider.setTextValueSuffix (suffix);
    addAndMakeVisible (row.slider);

    row.attachment = std::make_unique<SliderAttachment> (processor.apvts, paramId, row.slider);
}

void HushRigEditor::setupUpdateSection()
{
    versionLabel.setText (pt ("Versão ") + Updater::currentVersion(), juce::dontSendNotification);
    versionLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible (versionLabel);

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
    statusLabel.setColour (juce::Label::textColourId, isError ? juce::Colour (0xffff7a7a) : juce::Colours::lightgrey);
    statusLabel.setText (text, juce::dontSendNotification);
}

void HushRigEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff15171c));

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    g.drawText ("HushRig", kPad, kPad, getWidth() - 2 * kPad, kTitleHeight, juce::Justification::centredLeft);

    if (showUpdateSection)
    {
        g.setColour (juce::Colour (0xff2a2d35));
        const int y = kPad + kTitleHeight + kRowHeight * 6 + 4;
        g.fillRect (kPad, y, getWidth() - 2 * kPad, 1);
    }
}

void HushRigEditor::resized()
{
    auto area = getLocalBounds().reduced (kPad);
    area.removeFromTop (kTitleHeight);

    for (auto& row : rows)
    {
        auto r = area.removeFromTop (kRowHeight);
        row.label.setBounds (r.removeFromLeft (120));
        row.slider.setBounds (r);
    }

    bypassButton.setBounds (area.removeFromTop (kRowHeight));

    if (! showUpdateSection)
        return;

    area.removeFromTop (8);
    versionLabel.setBounds (area.removeFromTop (24));

    auto buttons = area.removeFromTop (32);
    checkButton.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2 - 4));
    buttons.removeFromLeft (8);
    installButton.setBounds (buttons);

    area.removeFromTop (8);
    progressBar.setBounds (area.removeFromTop (18));
    area.removeFromTop (6);
    statusLabel.setBounds (area);
}
