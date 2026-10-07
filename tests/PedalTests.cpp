#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

#include "dsp/Chorus.h"
#include "dsp/Delay.h"
#include "dsp/Eq3.h"
#include "dsp/Overdrive.h"
#include "dsp/Reverb.h"

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr double kPi = 3.14159265358979323846;

std::vector<float> sine (double freq, float amplitude, int numSamples)
{
    std::vector<float> v (static_cast<size_t> (numSamples));
    for (int n = 0; n < numSamples; ++n)
        v[static_cast<size_t> (n)] = amplitude * static_cast<float> (std::sin (2.0 * kPi * freq * n / kSampleRate));
    return v;
}

template <typename Effect>
std::vector<float> run (Effect& fx, std::vector<float> in)
{
    float* ch[1] = { in.data() };
    fx.process (ch, 1, static_cast<int> (in.size()));
    return in;
}

float peak (const std::vector<float>& v, size_t from = 0)
{
    float p = 0.0f;
    for (size_t i = from; i < v.size(); ++i)
        p = std::max (p, std::abs (v[i]));
    return p;
}

bool allFinite (const std::vector<float>& v)
{
    for (float x : v)
        if (! std::isfinite (x))
            return false;
    return true;
}
} // namespace

TEST_CASE ("Overdrive limita o nivel e comprime a dinamica")
{
    hushrig::Overdrive od;
    od.prepare (kSampleRate, 1);
    od.setParams ({ 24.0f, 6000.0f, 0.0f });

    const auto loud = run (od, sine (440.0, 1.0f, 4800));
    od.reset();
    const auto quiet = run (od, sine (440.0, 0.1f, 4800));

    REQUIRE (allFinite (loud));
    REQUIRE (peak (loud) <= 1.05f);
    // 20 dB de diferença na entrada vira bem menos na saída (saturou)
    REQUIRE (peak (loud, 2400) / peak (quiet, 2400) < 5.0f);
}

TEST_CASE ("Delay repete o sinal no tempo configurado")
{
    hushrig::Delay d;
    d.prepare (kSampleRate, 1);
    d.setParams ({ 100.0f, 0.0f, 1.0f, 20000.0f });

    std::vector<float> in (static_cast<size_t> (kSampleRate * 0.3), 0.0f);
    in[0] = 1.0f;

    // deixa o tempo suavizado assentar no alvo antes do impulso
    std::vector<float> warm (static_cast<size_t> (kSampleRate * 2.0), 0.0f);
    run (d, warm);

    const auto out = run (d, in);
    size_t maxIdx = 0;
    for (size_t i = 1; i < out.size(); ++i)
        if (std::abs (out[i]) > std::abs (out[maxIdx]))
            maxIdx = i;

    REQUIRE (std::abs (static_cast<double> (maxIdx) - 0.1 * kSampleRate) <= 2.0);
    REQUIRE (allFinite (out));
}

TEST_CASE ("Delay com feedback maximo continua estavel")
{
    hushrig::Delay d;
    d.prepare (kSampleRate, 1);
    d.setParams ({ 50.0f, 5.0f, 1.0f, 5000.0f }); // feedback sera limitado

    auto out = run (d, sine (220.0, 0.5f, 48000));
    for (int i = 0; i < 10; ++i)
        out = run (d, std::vector<float> (48000, 0.0f));
    REQUIRE (allFinite (out));
    REQUIRE (peak (out) < 1.0f);
}

TEST_CASE ("Chorus com mix 0 e transparente e com mix 1 fica limitado")
{
    hushrig::Chorus c;
    c.prepare (kSampleRate, 1);
    c.setParams ({ 1.0f, 0.5f, 0.0f });
    const auto in = sine (330.0, 0.5f, 4800);
    const auto dry = run (c, in);
    for (size_t i = 0; i < in.size(); ++i)
        REQUIRE (dry[i] == in[i]);

    c.reset();
    c.setParams ({ 1.0f, 1.0f, 1.0f });
    const auto wet = run (c, in);
    REQUIRE (allFinite (wet));
    REQUIRE (peak (wet) <= 0.51f);
}

TEST_CASE ("Reverb gera cauda depois do impulso e decai")
{
    hushrig::Reverb r;
    r.prepare (kSampleRate, 1);
    r.setParams ({ 0.7f, 0.5f, 1.0f });

    std::vector<float> in (static_cast<size_t> (kSampleRate * 4), 0.0f);
    in[0] = 1.0f;
    const auto out = run (r, in);

    REQUIRE (allFinite (out));
    const size_t half = static_cast<size_t> (kSampleRate * 0.5);
    float early = 0.0f, late = 0.0f;
    for (size_t i = 2000; i < half; ++i)
        early = std::max (early, std::abs (out[i]));
    for (size_t i = out.size() - 4800; i < out.size(); ++i)
        late = std::max (late, std::abs (out[i]));
    REQUIRE (early > 0.0f);
    REQUIRE (late < early);
}

TEST_CASE ("EQ aplica o ganho esperado em cada banda")
{
    auto gainAt = [] (double freq, hushrig::Eq3::Params p)
    {
        hushrig::Eq3 eq;
        eq.prepare (kSampleRate, 1);
        eq.setParams (p);
        const auto out = run (eq, sine (freq, 0.1f, 48000));
        return 20.0f * std::log10 (peak (out, 24000) / 0.1f);
    };

    REQUIRE (std::abs (gainAt (1000.0, { 0.0f, 0.0f, 0.0f })) < 0.1f);
    REQUIRE (std::abs (gainAt (1000.0, { 0.0f, 9.0f, 0.0f }) - 9.0f) < 0.5f);
    REQUIRE (std::abs (gainAt (60.0, { 9.0f, 0.0f, 0.0f }) - 9.0f) < 1.0f);
    REQUIRE (std::abs (gainAt (12000.0, { 0.0f, 0.0f, -9.0f }) + 9.0f) < 1.0f);
}
