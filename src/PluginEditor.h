#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include <array>
#include <memory>

#include "PluginProcessor.h"
#include "ui/HushLookAndFeel.h"
#include "ui/LevelMeter.h"

class HushRigEditor final : public juce::AudioProcessorEditor,
                            private juce::Timer
{
public:
    explicit HushRigEditor (HushRigProcessor&);
    ~HushRigEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Atualiza medidores (e, se pedido, a latência). Chamado pelo timer; público para a ferramenta de screenshots. */
    void tick (bool refreshStats = false);

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        juce::Label caption;
        juce::Slider slider;
        std::unique_ptr<SliderAttachment> attachment;
    };

    struct LatencyView
    {
        juce::String big, badge, device, details, tip;
        juce::Colour colour;
    };

    void timerCallback() override { tick(); }

    void addKnob (Knob& knob, const juce::String& caption, const char* paramId, const juce::String& suffix);
    void setupUpdateSection();
    void setupRecordSection();
    void toggleRecording();
    void updateRecordView();
    void setStatus (const juce::String& text, bool isError = false);
    void updateLatencyView();
    bool isGateBypassed() const;

    HushRigProcessor& processor;
    const bool showUpdateSection;

    HushLookAndFeel lnf;

    // Controles (0 = input, 1 = threshold, 2 = hold, 3 = release, 4 = output)
    std::array<Knob, 5> knobs;
    juce::ToggleButton bypassButton;
    std::unique_ptr<ButtonAttachment> bypassAttachment;

    // Medidores
    LevelMeter inputMeter, outputMeter;
    float shownInputDb = -100.0f, shownOutputDb = -100.0f;
    bool shownGateOpen = false;
    int tickCount = 0;

    LatencyView latency;

    // Gravação em WAV
    juce::TextButton recordButton, folderButton;
    juce::Label recordLabel;
    bool wasRecording = false;

    // Atualização (apenas standalone)
    juce::Label statusLabel;
    juce::TextButton checkButton, installButton;
    double progress = 0.0;
    juce::ProgressBar progressBar { progress };
    Updater::ReleaseInfo pendingRelease;

    // Áreas calculadas em resized()
    juce::Rectangle<int> headerArea, latencyCard, meterCard, knobCard, recordCard, updateCard;
    juce::Rectangle<int> inputRow, outputRow, knobInner;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HushRigEditor)
};
