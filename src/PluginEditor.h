#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include <array>
#include <memory>

#include "PluginProcessor.h"
#include "ui/AmpPanel.h"
#include "ui/CardLayout.h"
#include "ui/HushLookAndFeel.h"
#include "ui/LevelMeter.h"
#include "ui/PedalBoard.h"
#include "ui/UpdateBadge.h"

class HushRigEditor final : public juce::AudioProcessorEditor,
                            private juce::Timer,
                            private juce::ComponentListener
{
public:
    explicit HushRigEditor (HushRigProcessor&);
    ~HushRigEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void paintOverChildren (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void parentHierarchyChanged() override;

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
    void setupUpdater();
    void attachUpdateBadge();
    void layoutUpdateBadge();
    void componentMovedOrResized (juce::Component&, bool, bool) override { layoutUpdateBadge(); }
    void setupRecordSection();
    void toggleRecording();
    void updateRecordView();
    void updateLatencyView();
    bool isGateBypassed() const;
    int cardAt (juce::Point<int> p) const;
    void updateDropTarget (juce::Point<int> p);
    void applyCardDrop();

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

    // Pedais e presets
    PedalBoard pedalBoard;
    AmpPanel ampPanel;

    // Gravação em WAV
    juce::TextButton recordButton, folderButton;
    juce::Label recordLabel;
    bool wasRecording = false;

    // Atualização (apenas standalone): o botão fica na barra de título da janela
    UpdateBadge updateBadge;
    juce::Component::SafePointer<juce::DocumentWindow> badgeWindow;
    Updater::ReleaseInfo pendingRelease;
    double progress = 0.0;

    // Áreas calculadas em resized()
    juce::Rectangle<int> headerArea, latencyCard, meterCard, knobCard, pedalCard, ampCard, recordCard;
    juce::Rectangle<int> inputRow, outputRow, knobInner;

    // Ordem dos cards (arrastáveis) e estado do arrasto
    std::vector<int> cardOrder = hushrig::CardLayout::defaultOrder();
    std::array<juce::Rectangle<int>, hushrig::kNumCards> cardRects;
    int dragCard = -1;        // card sendo arrastado (-1 = nenhum)
    bool isDraggingCard = false;
    int dropTarget = -1;      // card ao lado do qual o arrastado vai cair
    bool dropBefore = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HushRigEditor)
};
