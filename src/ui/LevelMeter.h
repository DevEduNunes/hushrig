#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/HushLookAndFeel.h"

/** Medidor horizontal em dBFS, com marcador opcional de threshold. */
class LevelMeter final : public juce::Component
{
public:
    static constexpr float minDb = -80.0f;

    void setLevelDb (float db)
    {
        if (db != levelDb)
        {
            levelDb = db;
            repaint();
        }
    }

    void setThreshold (float db, bool visible)
    {
        if (db != thresholdDb || visible != thresholdVisible)
        {
            thresholdDb = db;
            thresholdVisible = visible;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        using namespace hush::colours;

        auto area = getLocalBounds().toFloat();
        const auto bar = area.removeFromTop (14.0f);

        const auto xFor = [&bar] (float db)
        {
            const float t = (juce::jlimit (minDb, 0.0f, db) - minDb) / (0.0f - minDb);
            return bar.getX() + bar.getWidth() * t;
        };

        g.setColour (track);
        g.fillRoundedRectangle (bar, 4.0f);

        if (levelDb > minDb)
        {
            const auto fill = bar.withRight (xFor (levelDb));
            g.setGradientFill (juce::ColourGradient (textDim, bar.getX(), 0.0f, accentBright, bar.getRight(), 0.0f, false));
            g.fillRoundedRectangle (fill, 4.0f);

            if (levelDb > -1.0f)
            {
                g.setColour (bad);
                g.fillRoundedRectangle (bar.withLeft (xFor (-3.0f)).withRight (fill.getRight()), 4.0f);
            }
        }

        g.setFont (juce::FontOptions (10.0f));

        for (const float db : { -60.0f, -40.0f, -20.0f, -6.0f, 0.0f })
        {
            const float tx = xFor (db);
            g.setColour (textDim);
            g.fillRect (tx - 0.5f, bar.getBottom() + 1.0f, 1.0f, 3.0f);
            g.drawText (juce::String (static_cast<int> (db)),
                        juce::Rectangle<int> (static_cast<int> (tx) - 14, static_cast<int> (bar.getBottom()) + 4, 28, 12),
                        juce::Justification::centred);
        }

        if (thresholdVisible)
        {
            const float tx = xFor (thresholdDb);
            g.setColour (warn);
            g.fillRect (tx - 1.0f, bar.getY() - 2.0f, 2.0f, bar.getHeight() + 4.0f);
        }
    }

private:
    float levelDb = -100.0f;
    float thresholdDb = -60.0f;
    bool thresholdVisible = false;
};
