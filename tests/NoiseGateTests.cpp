#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

#include "dsp/NoiseGate.h"

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr double kPi = 3.14159265358979323846;

float sine (int n, float amplitude)
{
    return amplitude * static_cast<float> (std::sin (2.0 * kPi * 110.0 * n / kSampleRate));
}

// Roda o gate sobre `numSamples` amostras de seno e devolve o pico da última janela de 10 ms.
float tailPeak (hushrig::NoiseGate& gate, int startSample, int numSamples, float amplitude)
{
    const int tail = static_cast<int> (0.010 * kSampleRate);
    float peak = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        const float x = sine (startSample + i, amplitude);
        const float y = x * gate.processSample (std::abs (x));
        if (i >= numSamples - tail)
            peak = std::max (peak, std::abs (y));
    }
    return peak;
}

hushrig::NoiseGate makeGate()
{
    hushrig::NoiseGate gate;
    gate.prepare (kSampleRate);
    hushrig::NoiseGate::Params p;
    p.thresholdDb = -55.0f;
    gate.setParams (p);
    return gate;
}
} // namespace

TEST_CASE ("Ruido abaixo do threshold e atenuado")
{
    auto gate = makeGate();
    const float amplitude = 0.0005f; // ~ -66 dBFS
    const float peak = tailPeak (gate, 0, static_cast<int> (kSampleRate), amplitude);
    REQUIRE (peak < amplitude * 0.02f);
}

TEST_CASE ("Sinal acima do threshold passa intacto")
{
    auto gate = makeGate();
    const float amplitude = 0.5f;
    const float peak = tailPeak (gate, 0, static_cast<int> (0.2 * kSampleRate), amplitude);
    REQUIRE (peak > amplitude * 0.95f);
}

TEST_CASE ("Gate reabre com sinal e fecha de novo depois do release")
{
    auto gate = makeGate();
    const int second = static_cast<int> (kSampleRate);

    // 1) ruído: fechado
    REQUIRE (tailPeak (gate, 0, second, 0.0005f) < 0.00002f);

    // 2) nota: abre
    REQUIRE (tailPeak (gate, second, second / 5, 0.5f) > 0.45f);

    // 3) volta o ruído: depois de hold + release, fecha
    REQUIRE (tailPeak (gate, 2 * second, second, 0.0005f) < 0.00002f);
}

TEST_CASE ("process() aplica um ganho linkado aos canais")
{
    auto gate = makeGate();
    constexpr int n = 4800;
    std::vector<float> left (n), right (n);
    for (int i = 0; i < n; ++i)
    {
        left[static_cast<size_t> (i)]  = sine (i, 0.5f);
        right[static_cast<size_t> (i)] = 0.0f;
    }

    float* channels[] = { left.data(), right.data() };
    gate.process (channels, 2, n);

    // O canal direito é silêncio e continua silêncio; o esquerdo (nota forte) passa.
    float peakL = 0.0f;
    for (int i = n - 480; i < n; ++i)
    {
        peakL = std::max (peakL, std::abs (left[static_cast<size_t> (i)]));
        REQUIRE (right[static_cast<size_t> (i)] == 0.0f);
    }
    REQUIRE (peakL > 0.45f);
}
