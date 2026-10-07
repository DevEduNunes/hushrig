#pragma once

#include <algorithm>
#include <array>
#include <vector>

namespace hushrig
{
/**
 * Reverb estilo Freeverb (8 combs em paralelo + 4 allpass em série) por canal.
 * O canal direito usa tamanhos levemente diferentes para decorrelacionar.
 */
class Reverb
{
public:
    struct Params
    {
        float roomSize = 0.5f; // 0..1
        float damping  = 0.5f; // 0..1
        float mix      = 0.25f;
    };

    void prepare (double sampleRate, int numChannels)
    {
        channels = std::max (numChannels, 1);
        const double scale = sampleRate / 44100.0;
        combs.assign (static_cast<size_t> (channels), {});
        allpasses.assign (static_cast<size_t> (channels), {});

        for (int ch = 0; ch < channels; ++ch)
        {
            const int spread = ch % 2 == 1 ? 23 : 0;
            for (size_t i = 0; i < kCombTunings.size(); ++i)
                combs[static_cast<size_t> (ch)][i].resize (static_cast<int> ((kCombTunings[i] + spread) * scale));
            for (size_t i = 0; i < kAllpassTunings.size(); ++i)
                allpasses[static_cast<size_t> (ch)][i].resize (static_cast<int> ((kAllpassTunings[i] + spread) * scale));
        }
    }

    void setParams (const Params& p) { params = p; }

    void reset()
    {
        for (auto& chCombs : combs)
            for (auto& c : chCombs)
                c.clear();
        for (auto& chAp : allpasses)
            for (auto& a : chAp)
                a.clear();
    }

    void process (float* const* data, int numChannels, int numSamples)
    {
        const int n = std::min (numChannels, channels);
        const float feedback = 0.7f + 0.28f * std::clamp (params.roomSize, 0.0f, 1.0f);
        const float damp = std::clamp (params.damping, 0.0f, 1.0f) * 0.4f;

        for (int ch = 0; ch < n; ++ch)
        {
            auto& chCombs = combs[static_cast<size_t> (ch)];
            auto& chAp = allpasses[static_cast<size_t> (ch)];

            for (int i = 0; i < numSamples; ++i)
            {
                const float dry = data[ch][i];
                const float input = dry * 0.015f;

                float acc = 0.0f;
                for (auto& c : chCombs)
                    acc += c.process (input, feedback, damp);
                for (auto& a : chAp)
                    acc = a.process (acc);

                data[ch][i] = dry * (1.0f - params.mix) + acc * params.mix * 3.0f;
            }
        }
    }

private:
    static constexpr std::array<int, 8> kCombTunings { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
    static constexpr std::array<int, 4> kAllpassTunings { 556, 441, 341, 225 };

    struct Comb
    {
        void resize (int size) { buffer.assign (static_cast<size_t> (std::max (size, 1)), 0.0f); pos = 0; store = 0.0f; }
        void clear() { std::fill (buffer.begin(), buffer.end(), 0.0f); store = 0.0f; }

        float process (float in, float feedback, float damp)
        {
            const float out = buffer[static_cast<size_t> (pos)];
            store = out * (1.0f - damp) + store * damp;
            buffer[static_cast<size_t> (pos)] = in + store * feedback;
            pos = (pos + 1) % static_cast<int> (buffer.size());
            return out;
        }

        std::vector<float> buffer;
        int pos = 0;
        float store = 0.0f;
    };

    struct Allpass
    {
        void resize (int size) { buffer.assign (static_cast<size_t> (std::max (size, 1)), 0.0f); pos = 0; }
        void clear() { std::fill (buffer.begin(), buffer.end(), 0.0f); }

        float process (float in)
        {
            const float buffered = buffer[static_cast<size_t> (pos)];
            const float out = buffered - in;
            buffer[static_cast<size_t> (pos)] = in + buffered * 0.5f;
            pos = (pos + 1) % static_cast<int> (buffer.size());
            return out;
        }

        std::vector<float> buffer;
        int pos = 0;
    };

    Params params;
    int channels = 2;
    std::vector<std::array<Comb, 8>> combs;
    std::vector<std::array<Allpass, 4>> allpasses;
};
} // namespace hushrig
