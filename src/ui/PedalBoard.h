#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include <array>
#include <functional>
#include <memory>
#include <vector>

#include "../PluginProcessor.h"
#include "HushLookAndFeel.h"

/**
 * Faixa de pedais: seletor de presets em cima e, embaixo, um bloco por pedal
 * (liga/desliga + knobs). Arrastar o titulo de um pedal muda a ordem da cadeia.
 */
class PedalBoard final : public juce::Component
{
public:
    static constexpr int kHeight = 252;

    explicit PedalBoard (HushRigProcessor& p) : processor (p), order (p.getChainOrder()), shown (order)
    {
        for (size_t i = 0; i < strips.size(); ++i)
        {
            strips[i] = std::make_unique<Strip> (processor, specs()[i], static_cast<int> (i));
            strips[i]->onDrag = [this] (int id, float x) { dragMoved (id, x); };
            strips[i]->onDrop = [this] { dragEnded(); };
            strips[i]->onLoadModel = [this] { chooseAmpModel(); };
            addAndMakeVisible (*strips[i]);
        }

        presetBox.setTextWhenNothingSelected (pt ("Escolher preset..."));
        presetBox.onChange = [this] { presetChosen(); };
        addAndMakeVisible (presetBox);

        saveButton.setButtonText ("Salvar");
        saveButton.onClick = [this] { askPresetName(); };
        addAndMakeVisible (saveButton);

        deleteButton.setButtonText ("Excluir");
        deleteButton.setEnabled (false);
        deleteButton.onClick = [this] { askDeletePreset(); };
        addAndMakeVisible (deleteButton);

        rebuildPresetList();
    }

    ~PedalBoard() override
    {
        if (dialog != nullptr)
        {
            dialog->setLookAndFeel (nullptr);
            dialog->exitModalState (0);
        }
    }

    /** Sincroniza com mudancas externas (preset carregado, estado restaurado). Chamado pelo timer do editor. */
    void refresh()
    {
        if (dragId >= 0)
            return;

        const auto current = processor.getChainOrder();

        if (current.slots != order.slots)
        {
            order = shown = current;
            layoutStrips();
        }

        for (auto& s : strips)
            s->syncState();
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (hush::colours::textDim);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText ("PEDAIS", getLocalBounds().removeFromTop (kBarH).removeFromLeft (70), juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto bar = getLocalBounds().removeFromTop (kBarH);
        bar.removeFromLeft (70);
        deleteButton.setBounds (bar.removeFromRight (80));
        bar.removeFromRight (8);
        saveButton.setBounds (bar.removeFromRight (80));
        bar.removeFromRight (8);
        presetBox.setBounds (bar.removeFromLeft (230));

        layoutStrips();
    }

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    static constexpr int kBarH = 28, kBarGap = 10, kGap = 8, kHeaderH = 28, kKnobRowH = 70;

    static juce::String pt (const char* utf8) { return juce::String::fromUTF8 (utf8); }

    struct KnobSpec { const char* caption; const char* id; };
    struct PedalSpec { const char* title; const char* onId; std::vector<KnobSpec> knobs; bool amp = false; };

    // Mesma ordem do enum hushrig::Pedal.
    static const std::array<PedalSpec, hushrig::kNumPedals>& specs()
    {
        static const std::array<PedalSpec, hushrig::kNumPedals> s { {
            { "OVERDRIVE", "odOn", { { "DRIVE", "odDrive" }, { "TONE", "odTone" }, { "LEVEL", "odLevel" } } },
            { "EQ",        "eqOn", { { "LOW", "eqLow" }, { "MID", "eqMid" }, { "HIGH", "eqHigh" } } },
            { "CHORUS",    "chOn", { { "RATE", "chRate" }, { "DEPTH", "chDepth" }, { "MIX", "chMix" } } },
            { "DELAY",     "dlOn", { { "TIME", "dlTime" }, { "FDBK", "dlFeedback" }, { "MIX", "dlMix" }, { "TONE", "dlTone" } } },
            { "REVERB",    "rvOn", { { "ROOM", "rvRoom" }, { "DAMP", "rvDamp" }, { "MIX", "rvMix" } } },
            { "AMP (NAM)", "ampOn", {}, true }, // os knobs ficam no AmpPanel
        } };
        return s;
    }

    class Strip final : public juce::Component
    {
    public:
        std::function<void (int, float)> onDrag;
        std::function<void()> onDrop;
        std::function<void()> onLoadModel; // so no bloco do amp

        Strip (HushRigProcessor& p, const PedalSpec& s, int pedalId)
            : processor (p), spec (s), id (pedalId), onParam (p.apvts.getRawParameterValue (s.onId))
        {
            toggle.setButtonText ({});
            toggle.setTooltip ("Liga/desliga");
            toggle.onClick = [this] { syncState(); };
            addAndMakeVisible (toggle);
            toggleAttachment = std::make_unique<ButtonAttachment> (p.apvts, s.onId, toggle);

            for (const auto& k : s.knobs)
            {
                auto knob = std::make_unique<Knob>();
                knob->caption.setText (k.caption, juce::dontSendNotification);
                knob->caption.setJustificationType (juce::Justification::centred);
                knob->caption.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
                knob->caption.setInterceptsMouseClicks (false, false);
                addAndMakeVisible (knob->caption);

                knob->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
                knob->slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
                knob->slider.setPopupDisplayEnabled (true, true, nullptr);
                addAndMakeVisible (knob->slider);

                knob->attachment = std::make_unique<SliderAttachment> (p.apvts, k.id, knob->slider);
                knobs.push_back (std::move (knob));
            }

            if (spec.amp)
            {
                loadButton.setButtonText (pt ("Carregar modelo..."));
                loadButton.onClick = [this] { if (onLoadModel) onLoadModel(); };
                addAndMakeVisible (loadButton);

                status.setJustificationType (juce::Justification::topLeft);
                status.setMinimumHorizontalScale (1.0f);
                status.setFont (juce::Font (juce::FontOptions (11.0f)));
                status.setInterceptsMouseClicks (false, false);
                addAndMakeVisible (status);
            }

            syncState();
        }

        void syncState()
        {
            const bool on = onParam->load() >= 0.5f;

            if (on != shownOn)
            {
                shownOn = on;
                repaint();
            }

            if (spec.amp)
                updateAmpStatus();
        }

        void paint (juce::Graphics& g) override
        {
            using namespace hush::colours;

            const auto r = getLocalBounds().toFloat();
            g.setColour (shownOn ? track.brighter (0.15f) : track.darker (0.4f));
            g.fillRoundedRectangle (r, 8.0f);
            g.setColour (shownOn ? accent : border);
            g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, dragging ? 2.0f : 1.0f);

            g.setColour (shownOn ? accentBright : textDim);
            g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
            g.drawText (spec.title, getLocalBounds().removeFromTop (kHeaderH).withTrimmedLeft (8).withTrimmedRight (30),
                        juce::Justification::centredLeft);
        }

        void resized() override
        {
            auto area = getLocalBounds();
            toggle.setBounds (area.removeFromTop (kHeaderH).removeFromRight (28).reduced (2, 3));
            area.reduce (4, 2);

            const int colW = area.getWidth() / 2;

            for (size_t i = 0; i < knobs.size(); ++i)
            {
                auto cell = juce::Rectangle<int> (area.getX() + static_cast<int> (i % 2) * colW,
                                                  area.getY() + static_cast<int> (i / 2) * kKnobRowH, colW, kKnobRowH);
                knobs[i]->caption.setBounds (cell.removeFromBottom (14));
                knobs[i]->slider.setBounds (cell.reduced (2, 0));
            }

            if (spec.amp)
            {
                area.removeFromTop (6);
                loadButton.setBounds (area.removeFromTop (28).reduced (2, 0));
                area.removeFromTop (8);
                status.setBounds (area.reduced (4, 0));
            }
        }

        void setDragging (bool d)
        {
            dragging = d;
            repaint();
        }

        void mouseDown (const juce::MouseEvent& e) override { dragStarted = e.y < kHeaderH; }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (dragStarted && onDrag)
                onDrag (id, e.getEventRelativeTo (getParentComponent()).position.x);
        }

        void mouseUp (const juce::MouseEvent&) override
        {
            if (dragStarted && onDrop)
                onDrop();
            dragStarted = false;
        }

        juce::MouseCursor getMouseCursor() override { return juce::MouseCursor::DraggingHandCursor; }

    private:
        static juce::String pt (const char* utf8) { return juce::String::fromUTF8 (utf8); }

        void updateAmpStatus()
        {
            using namespace hush::colours;

            juce::String text;
            juce::Colour colour = textDim;

            if (! processor.ampLoaded.load())
            {
                text = pt ("Nenhum modelo carregado");
            }
            else
            {
                const float load = processor.ampCpuLoad.load();
                colour = load < 0.3f ? good : load < 0.5f ? okay : load < 0.8f ? warn : bad;

                text = juce::File (processor.getAmpModelPath()).getFileNameWithoutExtension()
                       + "\nCPU " + juce::String (juce::roundToInt (load * 100.0f)) + "% do buffer";

                if (processor.ampRateMismatch.load())
                {
                    text += pt ("\nTaxa do modelo difere da do dispositivo");
                    colour = warn;
                }
            }

            if (text != status.getText())
                status.setText (text, juce::dontSendNotification);
            status.setColour (juce::Label::textColourId, colour);
        }

        struct Knob
        {
            juce::Label caption;
            juce::Slider slider;
            std::unique_ptr<SliderAttachment> attachment;
        };

        HushRigProcessor& processor;
        const PedalSpec& spec;
        const int id;
        std::atomic<float>* onParam;
        juce::ToggleButton toggle;
        std::unique_ptr<ButtonAttachment> toggleAttachment;
        std::vector<std::unique_ptr<Knob>> knobs;
        juce::TextButton loadButton;
        juce::Label status;
        bool shownOn = false, dragging = false, dragStarted = false;
    };

    // --- Layout e arrastar ----------------------------------------------------
    int stripWidth() const { return (getWidth() - kGap * (hushrig::kNumPedals - 1)) / hushrig::kNumPedals; }
    int stripTop() const { return kBarH + kBarGap; }

    void layoutStrips()
    {
        const int w = stripWidth(), h = getHeight() - stripTop();

        for (size_t slot = 0; slot < shown.slots.size(); ++slot)
        {
            const int pedal = shown.slots[slot];
            if (pedal == dragId)
                continue;
            strips[static_cast<size_t> (pedal)]->setBounds (static_cast<int> (slot) * (w + kGap), stripTop(), w, h);
        }
    }

    void dragMoved (int id, float x)
    {
        const int w = stripWidth();
        auto& strip = *strips[static_cast<size_t> (id)];

        if (dragId != id)
        {
            dragId = id;
            grabOffset = x - static_cast<float> (strip.getX());
            strip.setDragging (true);
            strip.toFront (false);
        }

        const int left = juce::jlimit (0, getWidth() - w, static_cast<int> (x - grabOffset));
        strip.setBounds (left, stripTop(), w, getHeight() - stripTop());

        const int target = juce::jlimit (0, hushrig::kNumPedals - 1, juce::roundToInt (static_cast<float> (left) / static_cast<float> (w + kGap)));
        const int from = static_cast<int> (std::find (order.slots.begin(), order.slots.end(), id) - order.slots.begin());

        shown = order;
        shown.move (from, target);
        layoutStrips();
    }

    void dragEnded()
    {
        if (dragId < 0)
            return;

        strips[static_cast<size_t> (dragId)]->setDragging (false);
        dragId = -1;
        order = shown;
        processor.setChainOrder (order);
        layoutStrips();
    }

    // --- Amp (NAM) --------------------------------------------------------------
    void chooseAmpModel()
    {
        const auto current = processor.getAmpModelPath();
        const auto start = current.isNotEmpty() ? juce::File (current).getParentDirectory()
                                                : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

        chooser = std::make_unique<juce::FileChooser> (pt ("Escolher modelo NAM"), start, "*.nam;*.wav");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safe = juce::Component::SafePointer<PedalBoard> (this)] (const juce::FileChooser& fc)
                              {
                                  const auto file = fc.getResult();
                                  if (safe == nullptr || file == juce::File())
                                      return;

                                  juce::String error;
                                  if (safe->processor.loadAmpModel (file, error))
                                  {
                                      if (auto* on = safe->processor.apvts.getParameter ("ampOn")) // carregar liga o amp
                                      {
                                          on->beginChangeGesture();
                                          on->setValueNotifyingHost (1.0f);
                                          on->endChangeGesture();
                                      }
                                      return;
                                  }

                                  juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                                                    .withIconType (juce::MessageBoxIconType::WarningIcon)
                                                                    .withTitle (pt ("Não foi possível carregar o modelo"))
                                                                    .withMessage (error)
                                                                    .withButton ("OK")
                                                                    .withAssociatedComponent (safe.getComponent()),
                                                                nullptr);
                              });
    }

    // --- Presets ---------------------------------------------------------------
    static constexpr int kUserIdBase = 1000;

    void rebuildPresetList (const juce::String& select = {})
    {
        presetBox.clear (juce::dontSendNotification);

        presetBox.addSectionHeading (pt ("De fábrica"));
        const auto factory = PresetManager::getFactoryPresetNames();
        for (int i = 0; i < factory.size(); ++i)
            presetBox.addItem (factory[i], i + 1);

        userNames = processor.presets.getUserPresetNames();
        if (! userNames.isEmpty())
        {
            presetBox.addSectionHeading ("Meus presets");
            for (int i = 0; i < userNames.size(); ++i)
                presetBox.addItem (userNames[i], kUserIdBase + i);
        }

        const int idx = userNames.indexOf (select);
        presetBox.setSelectedId (idx >= 0 ? kUserIdBase + idx : 0, juce::dontSendNotification);
        deleteButton.setEnabled (idx >= 0);
    }

    void presetChosen()
    {
        const int id = presetBox.getSelectedId();

        if (id >= kUserIdBase)
            processor.presets.loadUserPreset (userNames[id - kUserIdBase]);
        else if (id > 0)
            processor.presets.loadFactoryPreset (id - 1);

        deleteButton.setEnabled (id >= kUserIdBase);
        refresh();
    }

    void askPresetName()
    {
        if (dialog != nullptr)
            return;

        auto* w = new juce::AlertWindow ("Salvar preset", pt ("Nome do preset (salva os pedais e a ordem):"),
                                         juce::MessageBoxIconType::NoIcon, this);
        w->addTextEditor ("name", presetBox.getText(), "");
        w->addButton ("Salvar", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancelar", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        w->setLookAndFeel (&getLookAndFeel());
        dialog = w;

        w->enterModalState (true, juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<PedalBoard> (this), w] (int result)
        {
            if (safe == nullptr)
                return;

            const auto name = PresetManager::sanitiseName (w->getTextEditorContents ("name"));
            w->setLookAndFeel (nullptr);

            if (result == 1 && name.isNotEmpty() && safe->processor.presets.saveUserPreset (name))
                safe->rebuildPresetList (name);
        }), true);
    }

    void askDeletePreset()
    {
        const int id = presetBox.getSelectedId();
        if (id < kUserIdBase)
            return;

        const auto name = userNames[id - kUserIdBase];

        juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                          .withIconType (juce::MessageBoxIconType::QuestionIcon)
                                          .withTitle ("Excluir preset")
                                          .withMessage ("Excluir \"" + name + "\"?")
                                          .withButton ("Excluir")
                                          .withButton ("Cancelar")
                                          .withAssociatedComponent (this),
                                      [safe = juce::Component::SafePointer<PedalBoard> (this), name] (int result)
                                      {
                                          if (safe != nullptr && result == 1 && safe->processor.presets.deleteUserPreset (name))
                                              safe->rebuildPresetList();
                                      });
    }

    HushRigProcessor& processor;
    hushrig::ChainOrder order, shown; // `shown` inclui a previa durante o arrastar
    std::array<std::unique_ptr<Strip>, hushrig::kNumPedals> strips;
    int dragId = -1;
    float grabOffset = 0.0f;

    std::unique_ptr<juce::FileChooser> chooser;
    juce::ComboBox presetBox;
    juce::TextButton saveButton, deleteButton;
    juce::StringArray userNames;
    juce::Component::SafePointer<juce::AlertWindow> dialog;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PedalBoard)
};
