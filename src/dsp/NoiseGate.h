#pragma once

#include <algorithm>
#include <cmath>

namespace hushrig
{
/**
 * Noise gate de latência zero (sem lookahead), pensado para guitarra.
 * Header-only e sem dependência do JUCE, para ser testável isoladamente.
 *
 * Detector de pico com decaimento rápido + histerese + hold, e suavização
 * do ganho (ataque/release) para evitar cliques.
 */
class NoiseGate
{
public:
    struct Params
    {
        float thresholdDb  = -55.0f; // abre acima disto
        float hysteresisDb = 4.0f;   // fecha em (threshold - hysteresis)
        float attackMs     = 1.0f;
        float holdMs       = 40.0f;
        float releaseMs    = 80.0f;
        float reductionDb  = -60.0f; // atenuação com o gate fechado
    };

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        reset();
        updateCoefficients();
    }

    void setParams (const Params& newParams)
    {
        params = newParams;
        updateCoefficients();
    }

    void reset()
    {
        envelope = 0.0f;
        gain = 1.0f;
        holdCounter = 0;
        open = false;
    }

    /** Recebe o nível absoluto (linear) da amostra e devolve o ganho a aplicar. */
    float processSample (float level)
    {
        // Detector de pico: sobe instantaneamente, decai suavemente.
        envelope = std::max (level, envelope * envelopeDecay);

        if (envelope >= openThreshold)
        {
            open = true;
            holdCounter = holdSamples;
        }
        else if (envelope < closeThreshold)
        {
            if (holdCounter > 0)
                --holdCounter;
            else
                open = false;
        }
        else if (open)
        {
            holdCounter = holdSamples; // na zona de histerese, mantém aberto
        }

        const float target = open ? 1.0f : floorGain;
        const float coeff  = target > gain ? attackCoeff : releaseCoeff;
        gain = target + coeff * (gain - target);
        return gain;
    }

    /** Processa um bloco in-place. Um único ganho (linkado) vale para todos os canais. */
    void process (float* const* channels, int numChannels, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            float level = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                level = std::max (level, std::abs (channels[ch][i]));

            const float g = processSample (level);

            for (int ch = 0; ch < numChannels; ++ch)
                channels[ch][i] *= g;
        }
    }

private:
    static float dbToGain (float db) { return std::pow (10.0f, db / 20.0f); }

    float timeToCoeff (float ms) const
    {
        const double samples = std::max (1.0e-3 * static_cast<double> (ms) * sampleRate, 1.0);
        return static_cast<float> (std::exp (-1.0 / samples));
    }

    void updateCoefficients()
    {
        openThreshold  = dbToGain (params.thresholdDb);
        closeThreshold = dbToGain (params.thresholdDb - params.hysteresisDb);
        floorGain      = dbToGain (params.reductionDb);
        attackCoeff    = timeToCoeff (params.attackMs);
        releaseCoeff   = timeToCoeff (params.releaseMs);
        envelopeDecay  = timeToCoeff (10.0f);
        holdSamples    = static_cast<int> (1.0e-3 * static_cast<double> (params.holdMs) * sampleRate);
    }

    Params params;
    double sampleRate = 48000.0;

    float openThreshold = 0.0f, closeThreshold = 0.0f, floorGain = 0.0f;
    float attackCoeff = 0.0f, releaseCoeff = 0.0f, envelopeDecay = 0.0f;
    int holdSamples = 0;

    float envelope = 0.0f;
    float gain = 1.0f;
    int holdCounter = 0;
    bool open = false;
};
} // namespace hushrig
