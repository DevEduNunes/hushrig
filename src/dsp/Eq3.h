#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace hushrig
{
/** EQ de 3 bandas: low shelf (200 Hz), peaking (1 kHz) e high shelf (4 kHz), biquads RBJ. */
class Eq3
{
public:
    struct Params
    {
        float lowDb  = 0.0f; // -12..+12
        float midDb  = 0.0f;
        float highDb = 0.0f;
    };

    void prepare (double newSampleRate, int numChannels)
    {
        sampleRate = newSampleRate;
        channels = std::max (numChannels, 1);
        for (auto& s : states)
            s.assign (static_cast<size_t> (channels), {});
        updateCoefficients();
    }

    void setParams (const Params& p)
    {
        params = p;
        updateCoefficients();
    }

    void reset()
    {
        for (auto& s : states)
            std::fill (s.begin(), s.end(), State {});
    }

    void process (float* const* data, int numChannels, int numSamples)
    {
        const int n = std::min (numChannels, channels);
        for (int ch = 0; ch < n; ++ch)
            for (int i = 0; i < numSamples; ++i)
            {
                float x = data[ch][i];
                for (size_t b = 0; b < 3; ++b)
                    x = run (coeffs[b], states[b][static_cast<size_t> (ch)], x);
                data[ch][i] = x;
            }
    }

private:
    struct Coeffs { float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };
    struct State { float z1 = 0, z2 = 0; };

    static float run (const Coeffs& c, State& s, float x)
    {
        const float y = c.b0 * x + s.z1;
        s.z1 = c.b1 * x - c.a1 * y + s.z2;
        s.z2 = c.b2 * x - c.a2 * y;
        return y;
    }

    enum class Kind { lowShelf, peak, highShelf };

    Coeffs design (Kind kind, double freq, double gainDb, double q) const
    {
        constexpr double pi = 3.14159265358979323846;
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = 2.0 * pi * freq / sampleRate;
        const double cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * q);
        const double sq = 2.0 * std::sqrt (A) * alpha;

        double b0, b1, b2, a0, a1, a2;
        switch (kind)
        {
            case Kind::lowShelf:
                b0 = A * ((A + 1) - (A - 1) * cw + sq);
                b1 = 2 * A * ((A - 1) - (A + 1) * cw);
                b2 = A * ((A + 1) - (A - 1) * cw - sq);
                a0 = (A + 1) + (A - 1) * cw + sq;
                a1 = -2 * ((A - 1) + (A + 1) * cw);
                a2 = (A + 1) + (A - 1) * cw - sq;
                break;
            case Kind::highShelf:
                b0 = A * ((A + 1) + (A - 1) * cw + sq);
                b1 = -2 * A * ((A - 1) + (A + 1) * cw);
                b2 = A * ((A + 1) + (A - 1) * cw - sq);
                a0 = (A + 1) - (A - 1) * cw + sq;
                a1 = 2 * ((A - 1) - (A + 1) * cw);
                a2 = (A + 1) - (A - 1) * cw - sq;
                break;
            default:
                b0 = 1 + alpha * A;
                b1 = -2 * cw;
                b2 = 1 - alpha * A;
                a0 = 1 + alpha / A;
                a1 = -2 * cw;
                a2 = 1 - alpha / A;
                break;
        }

        return { static_cast<float> (b0 / a0), static_cast<float> (b1 / a0), static_cast<float> (b2 / a0),
                 static_cast<float> (a1 / a0), static_cast<float> (a2 / a0) };
    }

    void updateCoefficients()
    {
        coeffs[0] = design (Kind::lowShelf, 200.0, params.lowDb, 0.707);
        coeffs[1] = design (Kind::peak, 1000.0, params.midDb, 0.8);
        coeffs[2] = design (Kind::highShelf, 4000.0, params.highDb, 0.707);
    }

    Params params;
    double sampleRate = 48000.0;
    int channels = 2;
    Coeffs coeffs[3];
    std::vector<State> states[3];
};
} // namespace hushrig
