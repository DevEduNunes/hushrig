#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace hushrig
{
/**
 * Overdrive com saturação tanh. Filtro passa-altas antes do clipping (tira o
 * "lamaçal" dos graves), controle de tom (passa-baixas) depois, e nível de saída.
 * Sem dependência do JUCE; um estado por canal.
 */
class Overdrive
{
public:
    struct Params
    {
        float driveDb  = 12.0f;   // ganho antes do clipping
        float toneHz   = 4000.0f; // corte do passa-baixas pós-clipping
        float levelDb  = -6.0f;   // volume de saída
    };

    void prepare (double newSampleRate, int numChannels)
    {
        sampleRate = newSampleRate;
        channels = std::max (numChannels, 1);
        hpState.assign (static_cast<size_t> (channels), 0.0f);
        hpPrevIn.assign (static_cast<size_t> (channels), 0.0f);
        lpState.assign (static_cast<size_t> (channels), 0.0f);
        updateCoefficients();
    }

    void setParams (const Params& p)
    {
        params = p;
        updateCoefficients();
    }

    void reset()
    {
        std::fill (hpState.begin(), hpState.end(), 0.0f);
        std::fill (hpPrevIn.begin(), hpPrevIn.end(), 0.0f);
        std::fill (lpState.begin(), lpState.end(), 0.0f);
    }

    float processSample (int ch, float x)
    {
        const auto c = static_cast<size_t> (ch);

        // passa-altas de 1ª ordem (~720 Hz) antes do clipping
        const float hp = hpCoeff * (hpState[c] + x - hpPrevIn[c]);
        hpPrevIn[c] = x;
        hpState[c] = hp;
        const float pre = x * 0.5f + hp * 0.5f; // mistura: preserva um pouco de grave

        const float clipped = std::tanh (pre * driveGain) / std::tanh (driveGain);

        lpState[c] += lpAlpha * (clipped - lpState[c]);
        return lpState[c] * levelGain;
    }

    void process (float* const* data, int numChannels, int numSamples)
    {
        for (int ch = 0; ch < numChannels && ch < channels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                data[ch][i] = processSample (ch, data[ch][i]);
    }

private:
    void updateCoefficients()
    {
        constexpr double pi = 3.14159265358979323846;
        driveGain = std::pow (10.0f, params.driveDb / 20.0f);
        levelGain = std::pow (10.0f, params.levelDb / 20.0f);

        const double hpFc = 720.0;
        const double rc = 1.0 / (2.0 * pi * hpFc);
        const double dt = 1.0 / sampleRate;
        hpCoeff = static_cast<float> (rc / (rc + dt));

        const double lpFc = std::min (static_cast<double> (params.toneHz), sampleRate * 0.45);
        lpAlpha = static_cast<float> (1.0 - std::exp (-2.0 * pi * lpFc / sampleRate));
    }

    Params params;
    double sampleRate = 48000.0;
    int channels = 2;
    float driveGain = 1.0f, levelGain = 1.0f, hpCoeff = 0.0f, lpAlpha = 1.0f;
    std::vector<float> hpState, hpPrevIn, lpState;
};
} // namespace hushrig
