#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace hushrig
{
/** Delay com feedback e filtro passa-baixas no retorno (repetições mais escuras). */
class Delay
{
public:
    struct Params
    {
        float timeMs   = 350.0f;
        float feedback = 0.35f; // 0..0.95
        float mix      = 0.3f;  // 0 = seco, 1 = molhado
        float toneHz   = 5000.0f;
    };

    static constexpr float kMaxTimeMs = 2000.0f;

    void prepare (double newSampleRate, int numChannels)
    {
        sampleRate = newSampleRate;
        channels = std::max (numChannels, 1);
        bufferSize = static_cast<int> (kMaxTimeMs * 1.0e-3 * sampleRate) + 2;
        buffers.assign (static_cast<size_t> (channels), std::vector<float> (static_cast<size_t> (bufferSize), 0.0f));
        lp.assign (static_cast<size_t> (channels), 0.0f);
        writePos = 0;
        smoothedDelay = targetDelaySamples();
        updateCoefficients();
    }

    void setParams (const Params& p)
    {
        params = p;
        params.feedback = std::clamp (params.feedback, 0.0f, 0.95f);
        params.timeMs = std::clamp (params.timeMs, 1.0f, kMaxTimeMs);
        updateCoefficients();
    }

    void reset()
    {
        for (auto& b : buffers)
            std::fill (b.begin(), b.end(), 0.0f);
        std::fill (lp.begin(), lp.end(), 0.0f);
    }

    void process (float* const* data, int numChannels, int numSamples)
    {
        const int n = std::min (numChannels, channels);
        const float target = targetDelaySamples();

        for (int i = 0; i < numSamples; ++i)
        {
            smoothedDelay += (target - smoothedDelay) * 0.0005f; // evita "pitch bend" brusco
            float readPos = static_cast<float> (writePos) - smoothedDelay;
            if (readPos < 0.0f)
                readPos += static_cast<float> (bufferSize);

            const int i0 = static_cast<int> (readPos) % bufferSize;
            const int i1 = (i0 + 1) % bufferSize;
            const float frac = readPos - std::floor (readPos);

            for (int ch = 0; ch < n; ++ch)
            {
                auto& buf = buffers[static_cast<size_t> (ch)];
                const float wet = buf[static_cast<size_t> (i0)] * (1.0f - frac) + buf[static_cast<size_t> (i1)] * frac;

                auto& s = lp[static_cast<size_t> (ch)];
                s += lpAlpha * (wet - s);

                const float dry = data[ch][i];
                buf[static_cast<size_t> (writePos)] = dry + s * params.feedback;
                data[ch][i] = dry * (1.0f - params.mix) + wet * params.mix;
            }

            writePos = (writePos + 1) % bufferSize;
        }
    }

private:
    float targetDelaySamples() const { return static_cast<float> (params.timeMs * 1.0e-3 * sampleRate); }

    void updateCoefficients()
    {
        constexpr double pi = 3.14159265358979323846;
        const double fc = std::min (static_cast<double> (params.toneHz), sampleRate * 0.45);
        lpAlpha = static_cast<float> (1.0 - std::exp (-2.0 * pi * fc / sampleRate));
    }

    Params params;
    double sampleRate = 48000.0;
    int channels = 2, bufferSize = 2, writePos = 0;
    float smoothedDelay = 1.0f, lpAlpha = 1.0f;
    std::vector<std::vector<float>> buffers;
    std::vector<float> lp;
};
} // namespace hushrig
