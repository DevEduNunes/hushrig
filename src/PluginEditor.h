#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include <array>
#include <memory>

#include "PluginProcessor.h"

class HushRigEditor final : public juce::AudioProcessorEditor
{
public:
    explicit HushRigEditor (HushRigProcessor&);
    ~HushRigEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Row
    {
        juce::Label label;
        juce::Slider slider;
        std::unique_ptr<SliderAttachment> attachment;
    };

    void addRow (Row& row, const juce::String& text, const char* paramId, const juce::String& suffix);
    void setupUpdateSection();
    void setStatus (const juce::String& text, bool isError = false);

    HushRigProcessor& processor;
    const bool showUpdateSection;

    std::array<Row, 5> rows;
    juce::ToggleButton bypassButton;
    std::unique_ptr<ButtonAttachment> bypassAttachment;

    juce::Label versionLabel, statusLabel;
    juce::TextButton checkButton, installButton;
    double progress = 0.0;
    juce::ProgressBar progressBar { progress };
    Updater::ReleaseInfo pendingRelease;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HushRigEditor)
};
