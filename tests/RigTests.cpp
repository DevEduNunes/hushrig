#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <string>
#include <vector>

#include "Rig.h"

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr double kPi = 3.14159265358979323846;
constexpr int kBlock = 64;

float rms (const std::vector<float>& v, size_t from)
{
    double acc = 0.0;
    for (size_t i = from; i < v.size(); ++i)
        acc += static_cast<double> (v[i]) * v[i];
    return static_cast<float> (std::sqrt (acc / static_cast<double> (v.size() - from)));
}

// Passa `numBlocks` blocos de um seno de 220 Hz pelo rig e devolve a saída inteira.
std::vector<float> run (hushrig::Rig& rig, float amplitude, int numBlocks)
{
    std::vector<float> out;
    int n = 0;
    for (int b = 0; b < numBlocks; ++b)
    {
        std::vector<float> block (kBlock);
        for (auto& s : block)
            s = amplitude * static_cast<float> (std::sin (2.0 * kPi * 220.0 * n++ / kSampleRate));
        rig.process (block.data(), kBlock);
        out.insert (out.end(), block.begin(), block.end());
    }
    return out;
}
} // namespace

TEST_CASE ("Rig: parametros por id sao limitados a faixa e ids desconhecidos sao recusados")
{
    hushrig::RigState state;
    CHECK (state.setById ("outputGain", 99.0f));
    CHECK (state.get (hushrig::pOutputGain) == 12.0f);
    CHECK (state.setById ("gateThreshold", -500.0f));
    CHECK (state.get (hushrig::pGateThreshold) == -90.0f);
    CHECK_FALSE (state.setById ("naoExiste", 1.0f));

    state.set (hushrig::pOdDrive, NAN); // lixo da rede não pode contaminar o áudio
    CHECK (state.get (hushrig::pOdDrive) == 12.0f);
}

TEST_CASE ("Rig: tabela de parametros tem ids unicos e padroes dentro da faixa")
{
    const auto& t = hushrig::paramTable();
    for (size_t i = 0; i < t.size(); ++i)
    {
        CHECK (t[i].min <= t[i].def);
        CHECK (t[i].def <= t[i].max);
        for (size_t j = i + 1; j < t.size(); ++j)
            CHECK (std::string (t[i].id) != t[j].id);
    }
}

TEST_CASE ("Rig: ordem invalida e ignorada")
{
    hushrig::RigState state;
    hushrig::ChainOrder bad;
    bad.slots = { 0, 0, 1, 2, 3, 4 };
    state.setOrder (bad);
    CHECK (state.getOrder().slots == hushrig::ChainOrder {}.slots);

    hushrig::ChainOrder good;
    good.move (0, 5);
    state.setOrder (good);
    CHECK (state.getOrder().slots == good.slots);
}

TEST_CASE ("Rig: sem pedais e com o gate desligado o sinal passa intacto")
{
    hushrig::RigState state;
    state.set (hushrig::pGateBypass, 1.0f);
    state.set (hushrig::pOutputGain, 0.0f);
    hushrig::Rig rig (state);
    rig.prepare (kSampleRate);

    const auto out = run (rig, 0.5f, 50);
    CHECK_THAT (rms (out, 0), Catch::Matchers::WithinRel (0.5f / std::sqrt (2.0f), 0.01f));
}

TEST_CASE ("Rig: ganho de saida muda o nivel")
{
    hushrig::RigState state;
    state.set (hushrig::pGateBypass, 1.0f);
    state.set (hushrig::pOutputGain, 0.0f);
    hushrig::Rig rig (state);
    rig.prepare (kSampleRate);
    const float base = rms (run (rig, 0.3f, 50), 0);

    state.set (hushrig::pOutputGain, -6.0f);
    run (rig, 0.3f, 4); // deixa a rampa de ganho terminar
    const float quieter = rms (run (rig, 0.3f, 50), 0);
    CHECK_THAT (quieter / base, Catch::Matchers::WithinRel (0.501f, 0.02f));
}

TEST_CASE ("Rig: o gate fecha com ruido baixo e abre com a guitarra")
{
    hushrig::RigState state;
    state.set (hushrig::pOutputGain, 0.0f);
    hushrig::Rig rig (state);
    rig.prepare (kSampleRate);

    const auto hiss = run (rig, 0.0005f, 1000); // ~ -66 dBFS, abaixo do threshold padrão (1,3 s)
    CHECK (rms (hiss, hiss.size() * 3 / 4) < 0.0005f * 0.01f); // sobrou menos de -40 dB depois do release
    CHECK_FALSE (rig.gateOpen());

    const auto played = run (rig, 0.3f, 200);
    CHECK (rig.gateOpen());
    CHECK (rms (played, played.size() / 2) > 0.2f);
}

TEST_CASE ("Rig: cadeia completa fica finita e limitada sob sinal forte")
{
    hushrig::RigState state;
    state.set (hushrig::pOutputGain, 0.0f);
    for (auto p : { hushrig::pOdOn, hushrig::pEqOn, hushrig::pChOn, hushrig::pDlOn, hushrig::pRvOn })
        state.set (p, 1.0f);
    hushrig::Rig rig (state);
    rig.prepare (kSampleRate);

    const auto out = run (rig, 0.9f, 400);
    for (float s : out)
    {
        REQUIRE (std::isfinite (s));
        REQUIRE (std::abs (s) <= 1.5f);
    }
    CHECK (rms (out, out.size() / 2) > 0.01f);
}
