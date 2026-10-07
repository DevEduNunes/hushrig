#include "NamAmp.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <filesystem>
#include <string>

#include <cstdint>

#include "NAM/activations.h"
#include "NAM/container.h"
#include "NAM/convnet.h"
#include "NAM/dsp.h"
#include "NAM/get_dsp.h"
#include "NAM/linear.h"
#include "NAM/lstm.h"
#include "NAM/sequential.h"
#include "NAM/wavenet/model.h"

namespace hushrig
{
// O NAM registra cada arquitetura (WaveNet, LSTM...) com um objeto estatico dentro do proprio .cpp.
// Se esses .cpp vierem de uma biblioteca estatica (o JUCE empacota o codigo do plugin assim), o linker
// os descarta por nao serem referenciados e o carregamento falha com "No config parser registered".
// Esta tabela, com ligacao externa e volatile (nao pode ser removida), referencia uma funcao de
// cada arquivo e obriga o linker a mante-los.
extern volatile const std::uintptr_t namArchitectureAnchors[];
volatile const std::uintptr_t namArchitectureAnchors[] = {
    reinterpret_cast<std::uintptr_t> (&nam::wavenet::create_config),
    reinterpret_cast<std::uintptr_t> (&nam::lstm::create_config),
    reinterpret_cast<std::uintptr_t> (&nam::convnet::create_config),
    reinterpret_cast<std::uintptr_t> (&nam::linear::create_config),
    reinterpret_cast<std::uintptr_t> (&nam::sequential::create_config),
    reinterpret_cast<std::uintptr_t> (&nam::container::create_config),
};

struct NamAmp::Impl
{
    std::unique_ptr<nam::DSP> model;
    double sampleRate = 48000.0;
    int maxBlock = 512;
    std::vector<NAM_SAMPLE> in, out;
};

NamAmp::NamAmp() : impl (std::make_unique<Impl>())
{
    nam::activations::Activation::enable_fast_tanh();
    impl->in.resize (static_cast<size_t> (impl->maxBlock));
    impl->out.resize (static_cast<size_t> (impl->maxBlock));
}

NamAmp::~NamAmp() = default;

bool NamAmp::load (const std::string& path, std::string& error)
{
    try
    {
        const std::u8string utf8 (path.begin(), path.end()); // caminho em UTF-8 (acentos no Windows)
        auto model = nam::get_dsp (std::filesystem::path (utf8));

        if (model == nullptr)
        {
            error = "Nao foi possivel carregar o modelo.";
            return false;
        }

        if (model->NumInputChannels() != 1 || model->NumOutputChannels() < 1)
        {
            error = "O modelo precisa ter 1 entrada de audio.";
            return false;
        }

        model->Reset (impl->sampleRate, impl->maxBlock);
        impl->model = std::move (model);
        return true;
    }
    catch (const std::exception& e)
    {
        error = e.what();
        return false;
    }
}

void NamAmp::prepare (double sampleRate, int maxBlockSize)
{
    impl->sampleRate = sampleRate;
    impl->maxBlock = std::max (maxBlockSize, 1);
    impl->in.assign (static_cast<size_t> (impl->maxBlock), 0.0);
    impl->out.assign (static_cast<size_t> (impl->maxBlock), 0.0);

    if (impl->model != nullptr)
        impl->model->Reset (sampleRate, impl->maxBlock);
}

bool NamAmp::isLoaded() const { return impl->model != nullptr; }

void NamAmp::process (float* samples, int numSamples)
{
    if (impl->model == nullptr)
        return;

    // Hosts podem entregar blocos maiores que o anunciado: processa em fatias.
    for (int start = 0; start < numSamples; start += impl->maxBlock)
    {
        const int n = std::min (impl->maxBlock, numSamples - start);

        for (int i = 0; i < n; ++i)
            impl->in[static_cast<size_t> (i)] = static_cast<NAM_SAMPLE> (samples[start + i]);

        NAM_SAMPLE* inPtr = impl->in.data();
        NAM_SAMPLE* outPtr = impl->out.data();
        impl->model->process (&inPtr, &outPtr, n);

        for (int i = 0; i < n; ++i)
            samples[start + i] = static_cast<float> (impl->out[static_cast<size_t> (i)]);
    }
}

double NamAmp::expectedSampleRate() const
{
    return impl->model != nullptr ? impl->model->GetExpectedSampleRate() : -1.0;
}

bool NamAmp::sampleRateMismatch() const
{
    const double expected = expectedSampleRate();
    return impl->model != nullptr && expected > 0.0 && std::abs (expected - impl->sampleRate) > 1.0
           && ! impl->model->SupportsArbitrarySampleRate();
}

bool NamAmp::hasLoudness() const { return impl->model != nullptr && impl->model->HasLoudness(); }
double NamAmp::loudnessDb() const { return hasLoudness() ? impl->model->GetLoudness() : 0.0; }

double NamAmp::normalizationGainDb (double targetLoudnessDb) const
{
    // Limita para que um metadado absurdo nao gere um ganho perigoso.
    return hasLoudness() ? std::clamp (targetLoudnessDb - loudnessDb(), -24.0, 24.0) : 0.0;
}
} // namespace hushrig
