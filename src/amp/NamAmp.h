#pragma once

#include <memory>
#include <string>
#include <vector>

namespace hushrig
{
/**
 * Amp sim baseado no NeuralAmpModelerCore (modelos .nam).
 * Os headers do NAM ficam so no .cpp (pimpl), para o resto do app nao depender deles.
 *
 * Latencia: os modelos NAM sao causais e processados amostra a amostra, entao este
 * bloco adiciona 0 amostras de latencia. Se o modelo foi treinado em outra taxa de
 * amostragem que a do dispositivo, `sampleRateMismatch()` avisa (o som muda de timbre).
 *
 * `load` e `prepare` alocam memoria: chame fora da thread de audio. `process` nao aloca.
 */
class NamAmp
{
public:
    NamAmp();
    ~NamAmp();

    NamAmp (const NamAmp&) = delete;
    NamAmp& operator= (const NamAmp&) = delete;

    /** Carrega um .nam (ou .wav de IR). Em caso de erro devolve false e preenche `error`; o modelo anterior e mantido. */
    bool load (const std::string& path, std::string& error);

    /** Ajusta taxa e tamanho maximo de bloco. Pode ser chamado antes ou depois de `load`. */
    void prepare (double sampleRate, int maxBlockSize);

    bool isLoaded() const;

    /** Processa um canal in-place. Sem modelo carregado, nao altera o sinal. */
    void process (float* samples, int numSamples);

    double expectedSampleRate() const; // taxa do treino; -1 se desconhecida
    bool sampleRateMismatch() const;
    bool hasLoudness() const;
    double loudnessDb() const;
    int latencySamples() const { return 0; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
} // namespace hushrig
