// Mede o custo de CPU de um modelo .nam em tempo real, sem JUCE.
// Uso: hushrig_ampbench <modelo.nam> [taxa=48000] [segundos=10]
//
// Para cada tamanho de buffer, processa audio simulando o callback do driver e
// reporta o tempo por bloco em % do tempo disponivel (buffer / taxa). Acima de
// 100% o audio falha; o limite pratico confortavel e ~50%.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "amp/NamAmp.h"

int main (int argc, char* argv[])
{
    if (argc < 2)
    {
        std::fprintf (stderr, "Uso: %s <modelo.nam> [taxa=48000] [segundos=10]\n", argv[0]);
        return 2;
    }

    const double rate = argc > 2 ? std::atof (argv[2]) : 48000.0;
    const double seconds = argc > 3 ? std::atof (argv[3]) : 10.0;

    hushrig::NamAmp amp;
    std::string error;
    amp.prepare (rate, 1024);

    if (! amp.load (argv[1], error))
    {
        std::fprintf (stderr, "Falha ao carregar: %s\n", error.c_str());
        return 1;
    }

    std::printf ("Modelo: %s\nTaxa do dispositivo: %.0f Hz | taxa do treino: %.0f Hz%s\n\n", argv[1], rate,
                 amp.expectedSampleRate(), amp.sampleRateMismatch() ? "  (DIFERENTE: o timbre muda)" : "");
    std::printf ("%8s %10s %10s %10s %10s\n", "buffer", "janela", "medio", "p99", "pior");

    bool anyOverBudget = false;

    for (const int block : { 64, 128, 256, 512 })
    {
        amp.prepare (rate, block);

        const int numBlocks = static_cast<int> (seconds * rate / block);
        std::vector<float> buf (static_cast<size_t> (block));
        std::vector<double> times;
        times.reserve (static_cast<size_t> (numBlocks));
        const double windowUs = 1.0e6 * block / rate;

        unsigned seed = 12345;
        long long n = 0;

        for (int b = 0; b < numBlocks; ++b)
        {
            for (auto& s : buf)
            {
                seed = seed * 1664525u + 1013904223u;
                const float noise = static_cast<float> (seed >> 8) / 8388608.0f - 1.0f;
                s = 0.25f * static_cast<float> (std::sin (2.0 * 3.14159265358979 * 110.0 * static_cast<double> (n++) / rate)) + 0.02f * noise;
            }

            const auto t0 = std::chrono::steady_clock::now();
            amp.process (buf.data(), block);
            const auto t1 = std::chrono::steady_clock::now();
            times.push_back (std::chrono::duration<double, std::micro> (t1 - t0).count());
        }

        std::sort (times.begin(), times.end());
        double sum = 0.0;
        for (double t : times)
            sum += t;

        const double mean = sum / static_cast<double> (times.size());
        const double p99 = times[static_cast<size_t> (static_cast<double> (times.size() - 1) * 0.99)];
        const double worst = times.back();

        std::printf ("%8d %8.2fms %9.1f%% %9.1f%% %9.1f%%\n", block, windowUs / 1000.0, 100.0 * mean / windowUs,
                     100.0 * p99 / windowUs, 100.0 * worst / windowUs);

        if (p99 > 0.5 * windowUs)
            anyOverBudget = true;
    }

    std::printf ("\n%s\n", anyOverBudget ? "ATENCAO: p99 acima de 50% da janela em algum buffer. Prefira um modelo mais leve ou um buffer maior."
                                         : "OK: p99 abaixo de 50% da janela em todos os buffers testados.");
    return 0;
}
