#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include <array>
#include <memory>

#include "../PluginProcessor.h"
#include "HushLookAndFeel.h"

/** Controles do amp (NAM): ganho de entrada, tom (graves/medios/agudos), volume de saida e normalizacao. */
class AmpPanel final : public juce::Component
{
public:
    static constexpr int kHeight = 172;

    explicit AmpPanel (HushRigProcessor& p) : processor (p)
    {
        static constexpr std::array<Spec, 5> specs { {
            { "INPUT", "ampIn" }, { "BASS", "ampBass" }, { "MID", "ampMid" }, { "TREBLE", "ampTreble" }, { "OUTPUT", "ampOut" },
        } };

        for (size_t i = 0; i < knobs.size(); ++i)
        {
            auto& k = knobs[i];
            k.caption.setText (specs[i].caption, juce::dontSendNotification);
            k.caption.setJustificationType (juce::Justification::centred);
            k.caption.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
            addAndMakeVisible (k.caption);

            k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 18);
            k.slider.setTextValueSuffix (" dB");
            k.slider.setColour (juce::Slider::textBoxTextColourId, hush::colours::text);
            k.slider.setColour (juce::Slider::textBoxBackgroundColourId, hush::colours::track);
            k.slider.setColour (juce::Slider::textBoxOutlineColourId, hush::colours::border);
            addAndMakeVisible (k.slider);

            k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, specs[i].id, k.slider);
        }

        normalizeButton.setButtonText (juce::String::fromUTF8 ("Normalizar volume"));
        normalizeButton.setTooltip (juce::String::fromUTF8 ("Iguala o volume dos modelos usando o loudness gravado no arquivo .nam"));
        addAndMakeVisible (normalizeButton);
        normalizeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, "ampNorm", normalizeButton);

        info.setJustificationType (juce::Justification::topLeft);
        info.setMinimumHorizontalScale (1.0f);
        info.setFont (juce::Font (juce::FontOptions (11.0f)));
        addAndMakeVisible (info);

        refresh();
    }

    /** Atualiza o texto do ganho de normalizacao. Chamado pelo timer do editor. */
    void refresh()
    {
        using namespace hush::colours;

        juce::String text;
        juce::Colour colour = textDim;

        if (! processor.ampLoaded.load())
        {
            text = juce::String::fromUTF8 ("Carregue um modelo no bloco AMP da cadeia.");
        }
        else if (! processor.ampHasLoudness.load())
        {
            text = juce::String::fromUTF8 ("Este modelo não informa o loudness: sem normalização automática.");
        }
        else if (processor.apvts.getRawParameterValue ("ampNorm")->load() >= 0.5f)
        {
            const float db = processor.ampNormDb.load();
            text = juce::String::fromUTF8 ("Ganho aplicado ao modelo: ") + (db >= 0.0f ? "+" : "") + juce::String (db, 1) + " dB";
            colour = text_ok();
        }
        else
        {
            text = juce::String::fromUTF8 ("Normalização desligada (volume original do modelo).");
        }

        if (text != info.getText())
            info.setText (text, juce::dontSendNotification);
        info.setColour (juce::Label::textColourId, colour);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        auto side = area.removeFromRight (190);
        area.removeFromRight (12);

        const int colW = area.getWidth() / static_cast<int> (knobs.size());
        for (size_t i = 0; i < knobs.size(); ++i)
        {
            auto col = area.withX (area.getX() + static_cast<int> (i) * colW).withWidth (colW);
            knobs[i].caption.setBounds (col.removeFromTop (18));
            knobs[i].slider.setBounds (col.reduced (6, 0));
        }

        side.removeFromTop (20);
        normalizeButton.setBounds (side.removeFromTop (24));
        side.removeFromTop (6);
        info.setBounds (side);
    }

private:
    struct Spec { const char* caption; const char* id; };
    struct Knob
    {
        juce::Label caption;
        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    static juce::Colour text_ok() { return hush::colours::good; }

    HushRigProcessor& processor;
    std::array<Knob, 5> knobs;
    juce::ToggleButton normalizeButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> normalizeAttachment;
    juce::Label info;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AmpPanel)
};
