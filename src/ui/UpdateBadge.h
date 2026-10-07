#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>
#include <functional>

#include "HushLookAndFeel.h"

/**
 * Botão de atualização que fica na barra de título da janela standalone: uma seta de download
 * com a versão nova ("v0.2.1"). Aparece só quando há atualização; durante o download mostra o progresso.
 */
class UpdateBadge final : public juce::Component
{
public:
    static constexpr int kHeight = 22;

    std::function<void()> onClick;

    /** Mostra o botão para a versão informada. */
    void setAvailable (const juce::String& version)
    {
        label = "v" + version;
        downloading = false;
        progress = -1.0;
        setVisible (true);
        repaint();
    }

    void setDownloading (bool isDownloading)
    {
        downloading = isDownloading;
        repaint();
    }

    /** Progresso do download em 0..1; negativo = indeterminado. */
    void setProgress (double newProgress)
    {
        if (std::abs (newProgress - progress) > 0.004)
        {
            progress = newProgress;
            repaint();
        }
    }

    bool isDownloading() const { return downloading; }

    /** Largura necessária para o texto atual. */
    int getIdealWidth() const
    {
        return 34 + juce::GlyphArrangement::getStringWidthInt (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)), label);
    }

    void paint (juce::Graphics& g) override
    {
        using namespace hush::colours;

        auto r = getLocalBounds().toFloat().reduced (0.5f);
        const bool hot = isMouseOver() && ! downloading;

        g.setColour (hot ? accent : accent.withAlpha (0.85f));
        g.fillRoundedRectangle (r, r.getHeight() / 2.0f);

        // Seta de download.
        const auto icon = juce::Rectangle<float> (r.getX() + 8.0f, r.getCentreY() - 6.0f, 12.0f, 12.0f);
        juce::Path arrow;
        arrow.startNewSubPath (icon.getCentreX(), icon.getY());
        arrow.lineTo (icon.getCentreX(), icon.getBottom() - 4.0f);
        arrow.startNewSubPath (icon.getX() + 2.0f, icon.getBottom() - 6.0f);
        arrow.lineTo (icon.getCentreX(), icon.getBottom() - 2.5f);
        arrow.lineTo (icon.getRight() - 2.0f, icon.getBottom() - 6.0f);
        arrow.startNewSubPath (icon.getX(), icon.getBottom());
        arrow.lineTo (icon.getRight(), icon.getBottom());

        g.setColour (juce::Colours::black);
        g.strokePath (arrow, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (getText(), getLocalBounds().withTrimmedLeft (26).withTrimmedRight (8), juce::Justification::centredLeft);
    }

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! downloading && contains (e.getPosition()) && onClick)
            onClick();
    }

private:
    juce::String getText() const
    {
        if (! downloading)
            return label;

        return progress >= 0.0 ? juce::String (juce::roundToInt (progress * 100.0)) + "%" : juce::String ("...");
    }

    juce::String label;
    bool downloading = false;
    double progress = -1.0;
};
