#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace hushrig
{
/** Chorus: linha de atraso curta modulada por LFO (fase invertida entre canais para dar largura). */
class Chorus
{
public:
    struct Params
    {
        float rateHz  = 0.8f;
        float depth   = 0.5f;  // 0..1
        float mix     = 0.5f;
    };

    void prepare (double newSampleRate, int numChannels)
    {
        sampleRate = newSampleRate;
        channels = std::max (numChannels, 1);
        bufferSize = static_cast<int> (0.05 * sampleRate) + 2;
        buffers.assign (static_cast<size_t> (channels), std::vector<float> (static_cast<size_t> (bufferSize), 0.0f));
        writePos = 0;
        phase = 0.0;
    }

    void setParams (const Params& p) { params = p; }

    void reset()
    {
        for (auto& b : buffers)
            std::fill (b.begin(), b.end(), 0.0f);
        phase = 0.0;
    }

    void process (float* const* data, int numChannels, int numSamples)
    {
        constexpr double twoPi = 6.28318530717958647692;
        const int n = std::min (numChannels, channels);
        const double inc = params.rateHz / sampleRate;
        const float centreMs = 12.0f;
        const float swingMs = 8.0f * std::clamp (params.depth, 0.0f, 1.0f);

        for (int i = 0; i < numSamples; ++i)
        {
            for (int ch = 0; ch < n; ++ch)
            {
                auto& buf = buffers[static_cast<size_t> (ch)];
                const double p = phase + (ch % 2 == 1 ? 0.5 : 0.0);
                const float lfo = static_cast<float> (std::sin (twoPi * p));
                const float delaySamples = (centreMs + swingMs * lfo) * 1.0e-3f * static_cast<float> (sampleRate);

                float readPos = static_cast<float> (writePos) - delaySamples;
                if (readPos < 0.0f)
                    readPos += static_cast<float> (bufferSize);

                const int i0 = static_cast<int> (readPos) % bufferSize;
                const int i1 = (i0 + 1) % bufferSize;
                const float frac = readPos - std::floor (readPos);
                const float wet = buf[static_cast<size_t> (i0)] * (1.0f - frac) + buf[static_cast<size_t> (i1)] * frac;

                const float dry = data[ch][i];
                buf[static_cast<size_t> (writePos)] = dry;
                data[ch][i] = dry * (1.0f - params.mix) + wet * params.mix;
            }

            writePos = (writePos + 1) % bufferSize;
            phase += inc;
            if (phase >= 1.0)
                phase -= 1.0;
        }
    }

private:
    Params params;
    double sampleRate = 48000.0, phase = 0.0;
    int channels = 2, bufferSize = 2, writePos = 0;
    std::vector<std::vector<float>> buffers;
};
} // namespace hushrig
