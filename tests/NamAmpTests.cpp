#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cmath>
#include <string>
#include <vector>

#include "amp/NamAmp.h"

#ifndef HUSHRIG_TEST_DATA_DIR
#error "HUSHRIG_TEST_DATA_DIR deve apontar para tests/data"
#endif

namespace
{
const std::string kDir = HUSHRIG_TEST_DATA_DIR;

std::vector<float> guitarLike (int n)
{
    std::vector<float> v (static_cast<size_t> (n));
    for (int i = 0; i < n; ++i)
        v[static_cast<size_t> (i)] = 0.3f * static_cast<float> (std::sin (2.0 * 3.14159265358979 * 110.0 * i / 48000.0)
                                                                + 0.5 * std::sin (2.0 * 3.14159265358979 * 220.0 * i / 48000.0));
    return v;
}

bool allFinite (const std::vector<float>& v)
{
    for (float x : v)
        if (! std::isfinite (x))
            return false;
    return true;
}
} // namespace

TEST_CASE ("NamAmp sem modelo nao altera o sinal")
{
    hushrig::NamAmp amp;
    amp.prepare (48000.0, 128);
    auto in = guitarLike (256);
    auto out = in;
    amp.process (out.data(), static_cast<int> (out.size()));
    REQUIRE (out == in);
    REQUIRE_FALSE (amp.isLoaded());
}

TEST_CASE ("NamAmp rejeita arquivo invalido e preserva o modelo anterior")
{
    hushrig::NamAmp amp;
    amp.prepare (48000.0, 128);

    std::string error;
    const bool loaded = amp.load (kDir + "/wavenet.nam", error);
    INFO ("erro: " << error << " | dir: " << kDir);
    REQUIRE (loaded);
    REQUIRE (amp.isLoaded());

    REQUIRE_FALSE (amp.load (kDir + "/nao-existe.nam", error));
    REQUIRE_FALSE (error.empty());
    REQUIRE (amp.isLoaded());
}

TEST_CASE ("NamAmp processa WaveNet e LSTM com saida finita e diferente da entrada")
{
    for (const char* file : { "wavenet.nam", "lstm.nam" })
    {
        DYNAMIC_SECTION (file)
        {
            hushrig::NamAmp amp;
            amp.prepare (48000.0, 128);

            std::string error;
            const bool loaded = amp.load (kDir + "/" + file, error);
            INFO ("erro: " << error << " | dir: " << kDir);
            REQUIRE (loaded);
            REQUIRE (amp.latencySamples() == 0);

            const auto in = guitarLike (4800);
            auto out = in;
            // blocos de 128 mais um bloco maior que o anunciado (fatiado internamente)
            for (int pos = 0; pos < 4096; pos += 128)
                amp.process (out.data() + pos, 128);
            amp.process (out.data() + 4096, 704);

            REQUIRE (allFinite (out));
            REQUIRE (out != in);
        }
    }
}

TEST_CASE ("NamAmp e deterministico apos prepare")
{
    hushrig::NamAmp a, b;
    a.prepare (48000.0, 64);
    b.prepare (48000.0, 64);
    std::string error;
    const bool loadedA = a.load (kDir + "/wavenet.nam", error);
    INFO ("erro: " << error << " | dir: " << kDir);
    REQUIRE (loadedA);
    REQUIRE (b.load (kDir + "/wavenet.nam", error));

    auto x = guitarLike (640), y = x;
    a.process (x.data(), 640);
    b.process (y.data(), 640);
    REQUIRE (x == y);
}

TEST_CASE ("NamAmp calcula o ganho de normalizacao a partir do loudness do modelo")
{
    hushrig::NamAmp amp;
    REQUIRE (amp.normalizationGainDb() == 0.0); // sem modelo

    amp.prepare (48000.0, 128);
    std::string error;
    REQUIRE (amp.load (kDir + "/wavenet.nam", error));

    REQUIRE (amp.hasLoudness());
    REQUIRE (amp.normalizationGainDb() == Catch::Approx (-18.0 - amp.loudnessDb()));
    REQUIRE (amp.normalizationGainDb (-20.0) == Catch::Approx (-20.0 - amp.loudnessDb()));
}
