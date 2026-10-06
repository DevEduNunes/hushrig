#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace hush::colours
{
inline const juce::Colour background   { 0xff000000 };
inline const juce::Colour card         { 0xff0c0913 };
inline const juce::Colour border       { 0xff2b1b47 };
inline const juce::Colour track        { 0xff1c1230 };
inline const juce::Colour text         { 0xffc084fc };
inline const juce::Colour textDim      { 0xff8b5fbf };
inline const juce::Colour accent       { 0xffa855f7 };
inline const juce::Colour accentBright { 0xffe9d5ff };
inline const juce::Colour good         { 0xff4ade80 };
inline const juce::Colour okay         { 0xffbef264 };
inline const juce::Colour warn         { 0xfffacc15 };
inline const juce::Colour bad          { 0xfff87171 };
} // namespace hush::colours

/** Tema escuro: fundo preto, textos e destaques em roxo. */
class HushLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    HushLookAndFeel()
    {
        using namespace hush::colours;

        setColour (juce::ResizableWindow::backgroundColourId, background);
        setColour (juce::Label::textColourId, text);
        setColour (juce::Slider::textBoxTextColourId, accentBright);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxHighlightColourId, accent.withAlpha (0.4f));
        setColour (juce::TextButton::buttonColourId, track);
        setColour (juce::TextButton::buttonOnColourId, accent);
        setColour (juce::TextButton::textColourOffId, accentBright);
        setColour (juce::TextButton::textColourOnId, accentBright);
        setColour (juce::ToggleButton::textColourId, text);
        setColour (juce::ProgressBar::foregroundColourId, accent);
        setColour (juce::ProgressBar::backgroundColourId, track);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider&) override
    {
        using namespace hush::colours;

        const auto bounds = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                                    static_cast<float> (width), static_cast<float> (height)).reduced (6.0f);
        const float radius = std::min (bounds.getWidth(), bounds.getHeight()) / 2.0f;
        const auto centre = bounds.getCentre();

        constexpr float lineWidth = 5.0f;
        const float arcRadius = radius - lineWidth * 0.5f;
        const juce::PathStrokeType stroke (lineWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

        juce::Path trackPath;
        trackPath.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
        g.setColour (track);
        g.strokePath (trackPath, stroke);

        const float angle = startAngle + sliderPos * (endAngle - startAngle);

        juce::Path valuePath;
        valuePath.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, angle, true);
        g.setColour (accent);
        g.strokePath (valuePath, stroke);

        const float bodyRadius = radius - 12.0f;
        g.setColour (card.brighter (0.12f));
        g.fillEllipse (centre.x - bodyRadius, centre.y - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f);
        g.setColour (border);
        g.drawEllipse (centre.x - bodyRadius, centre.y - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f, 1.0f);

        juce::Path pointer;
        pointer.addRoundedRectangle (-1.5f, -bodyRadius, 3.0f, bodyRadius * 0.55f, 1.5f);
        g.setColour (accentBright);
        g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool, bool) override
    {
        using namespace hush::colours;

        const auto area = button.getLocalBounds().toFloat();
        const auto sw = juce::Rectangle<float> (area.getX(), area.getCentreY() - 9.0f, 34.0f, 18.0f);
        const bool on = button.getToggleState();

        g.setColour (on ? accent : track);
        g.fillRoundedRectangle (sw, 9.0f);

        const float knobX = on ? sw.getRight() - 16.0f : sw.getX() + 2.0f;
        g.setColour (accentBright);
        g.fillEllipse (knobX, sw.getY() + 2.0f, 14.0f, 14.0f);

        g.setColour (text);
        g.setFont (juce::FontOptions (13.0f));
        g.drawText (button.getButtonText(), area.withTrimmedLeft (42.0f).toNearestInt(), juce::Justification::centredLeft);
    }
};
