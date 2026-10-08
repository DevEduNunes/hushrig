#pragma once

// Núcleo do HushRig em hardware: parâmetros + cadeia de áudio mono.
// Sem dependência do ESP-IDF, para rodar (e ser testado) também no PC.
// Reaproveita o DSP do app (src/dsp) sem alterações.

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "dsp/Chorus.h"
#include "dsp/Delay.h"
#include "dsp/Eq3.h"
#include "dsp/NoiseGate.h"
#include "dsp/Overdrive.h"
#include "dsp/Reverb.h"
#include "dsp/ChainOrder.h"

namespace hushrig
{
// Mesmos ids do plugin (src/PluginProcessor.cpp), menos o bloco do amp NAM.
enum Param : int
{
    pInputGain, pGateBypass, pGateThreshold, pGateHold, pGateRelease, pOutputGain,
    pOdOn, pOdDrive, pOdTone, pOdLevel,
    pEqOn, pEqLow, pEqMid, pEqHigh,
    pChOn, pChRate, pChDepth, pChMix,
    pDlOn, pDlTime, pDlFeedback, pDlMix, pDlTone,
    pRvOn, pRvRoom, pRvDamp, pRvMix,
    kNumParams
};

struct ParamInfo
{
    const char* id;
    float min, max, def;
};

inline const std::array<ParamInfo, kNumParams>& paramTable()
{
    static const std::array<ParamInfo, kNumParams> table { {
        { "inputGain",     -24.0f,  24.0f,   0.0f },
        { "gateBypass",      0.0f,   1.0f,   0.0f },
        { "gateThreshold", -90.0f, -20.0f, -60.0f },
        { "gateHold",        0.0f, 500.0f,  40.0f },
        { "gateRelease",     5.0f, 1000.0f, 80.0f },
        { "outputGain",    -24.0f,  12.0f, -12.0f }, // line-out é bem mais quente que uma guitarra
        { "odOn",            0.0f,   1.0f,   0.0f },
        { "odDrive",         0.0f,  40.0f,  12.0f },
        { "odTone",        500.0f, 12000.0f, 4000.0f },
        { "odLevel",       -24.0f,   6.0f,  -6.0f },
        { "eqOn",            0.0f,   1.0f,   0.0f },
        { "eqLow",         -12.0f,  12.0f,   0.0f },
        { "eqMid",         -12.0f,  12.0f,   0.0f },
        { "eqHigh",        -12.0f,  12.0f,   0.0f },
        { "chOn",            0.0f,   1.0f,   0.0f },
        { "chRate",          0.1f,   5.0f,   0.8f },
        { "chDepth",         0.0f,   1.0f,   0.5f },
        { "chMix",           0.0f,   1.0f,   0.5f },
        { "dlOn",            0.0f,   1.0f,   0.0f },
        { "dlTime",          1.0f, 1000.0f, 350.0f },
        { "dlFeedback",      0.0f,   0.95f,  0.35f },
        { "dlMix",           0.0f,   1.0f,   0.3f },
        { "dlTone",        500.0f, 12000.0f, 5000.0f },
        { "rvOn",            0.0f,   1.0f,   0.0f },
        { "rvRoom",          0.0f,   1.0f,   0.5f },
        { "rvDamp",          0.0f,   1.0f,   0.5f },
        { "rvMix",           0.0f,   1.0f,   0.25f },
    } };
    return table;
}

/** Valores compartilhados entre a tarefa de rede (escreve) e a de áudio (lê), sem locks. */
class RigState
{
public:
    RigState()
    {
        for (int i = 0; i < kNumParams; ++i)
            values[static_cast<size_t> (i)].store (paramTable()[static_cast<size_t> (i)].def, std::memory_order_relaxed);
        for (size_t i = 0; i < order.size(); ++i)
            order[i].store (ChainOrder().slots[i], std::memory_order_relaxed);
    }

    /** Procura o parâmetro pelo id; valor fora da faixa é limitado. Devolve false se o id não existe. */
    bool setById (const char* id, float value)
    {
        for (int i = 0; i < kNumParams; ++i)
            if (std::strcmp (paramTable()[static_cast<size_t> (i)].id, id) == 0)
            {
                set (static_cast<Param> (i), value);
                return true;
            }
        return false;
    }

    void set (Param p, float value)
    {
        const auto& info = paramTable()[static_cast<size_t> (p)];
        if (! std::isfinite (value))
            return;
        values[static_cast<size_t> (p)].store (value < info.min ? info.min : (value > info.max ? info.max : value),
                                               std::memory_order_relaxed);
    }

    float get (Param p) const { return values[static_cast<size_t> (p)].load (std::memory_order_relaxed); }

    void setOrder (const ChainOrder& o)
    {
        if (! ChainOrder::isValid (o.slots))
            return;
        for (size_t i = 0; i < order.size(); ++i)
            order[i].store (o.slots[i], std::memory_order_relaxed);
    }

    ChainOrder getOrder() const
    {
        ChainOrder o;
        for (size_t i = 0; i < order.size(); ++i)
            o.slots[i] = order[i].load (std::memory_order_relaxed);
        return o;
    }

    void resetToDefaults()
    {
        for (int i = 0; i < kNumParams; ++i)
            set (static_cast<Param> (i), paramTable()[static_cast<size_t> (i)].def);
        setOrder (ChainOrder {});
    }

private:
    std::array<std::atomic<float>, kNumParams> values;
    std::array<std::atomic<int>, kNumPedals> order;
};

/** Cadeia mono: ganho de entrada > gate > pedais (na ordem escolhida) > ganho de saída. */
class Rig
{
public:
    explicit Rig (const RigState& s) : state (s) {}

    void prepare (double sampleRate)
    {
        gate.prepare (sampleRate);
        overdrive.prepare (sampleRate, 1);
        eq.prepare (sampleRate, 1);
        chorus.prepare (sampleRate, 1);
        delay.prepare (sampleRate, 1);
        reverb.prepare (sampleRate, 1);
        lastIn  = dbToGain (state.get (pInputGain));
        lastOut = dbToGain (state.get (pOutputGain));
        cacheValid = false;
    }

    /** Processa `n` amostras no lugar. Chamar só da tarefa de áudio. */
    void process (float* buf, int n)
    {
        refreshParams();
        float* ch[1] = { buf };

        const float inG = dbToGain (state.get (pInputGain));
        ramp (buf, n, lastIn, inG);
        lastIn = inG;

        peakIn = peakOf (buf, n);

        const bool gateActive = state.get (pGateBypass) < 0.5f;
        if (gateActive)
            gate.process (ch, 1, n);
        gateIsOpen = gateActive ? gate.isOpen() : true;

        const auto order = state.getOrder();
        for (int slot : order.slots)
            runPedal (static_cast<Pedal> (slot), ch, n);

        const float outG = dbToGain (state.get (pOutputGain));
        ramp (buf, n, lastOut, outG);
        lastOut = outG;

        peakOut = peakOf (buf, n);
    }

    float inputPeak() const { return peakIn; }
    float outputPeak() const { return peakOut; }
    bool gateOpen() const { return gateIsOpen; }

private:
    static float dbToGain (float db) { return std::pow (10.0f, db / 20.0f); }

    static float peakOf (const float* b, int n)
    {
        float m = 0.0f;
        for (int i = 0; i < n; ++i)
            m = std::max (m, std::abs (b[i]));
        return m;
    }

    static void ramp (float* b, int n, float from, float to)
    {
        if (from == to)
        {
            for (int i = 0; i < n; ++i)
                b[i] *= to;
            return;
        }
        const float step = (to - from) / static_cast<float> (n);
        float g = from;
        for (int i = 0; i < n; ++i, g += step)
            b[i] *= g;
    }

    bool on (Param p) const { return state.get (p) >= 0.5f; }

    // Recalcula os coeficientes só quando um parâmetro muda (evita pow/sin/exp a cada bloco).
    void refreshParams()
    {
        std::array<float, kNumParams> now;
        bool changed = ! cacheValid;
        for (int i = 0; i < kNumParams; ++i)
        {
            now[static_cast<size_t> (i)] = state.get (static_cast<Param> (i));
            changed = changed || now[static_cast<size_t> (i)] != cache[static_cast<size_t> (i)];
        }
        if (! changed)
            return;
        cache = now;
        cacheValid = true;

        NoiseGate::Params g;
        g.thresholdDb = now[pGateThreshold];
        g.holdMs      = now[pGateHold];
        g.releaseMs   = now[pGateRelease];
        gate.setParams (g);
        overdrive.setParams ({ now[pOdDrive], now[pOdTone], now[pOdLevel] });
        eq.setParams ({ now[pEqLow], now[pEqMid], now[pEqHigh] });
        chorus.setParams ({ now[pChRate], now[pChDepth], now[pChMix] });
        delay.setParams ({ now[pDlTime], now[pDlFeedback], now[pDlMix], now[pDlTone] });
        reverb.setParams ({ now[pRvRoom], now[pRvDamp], now[pRvMix] });
    }

    void runPedal (Pedal pedal, float* const* ch, int n)
    {
        switch (pedal)
        {
            case Pedal::overdrive: if (on (pOdOn)) overdrive.process (ch, 1, n); break;
            case Pedal::eq:        if (on (pEqOn)) eq.process (ch, 1, n);        break;
            case Pedal::chorus:    if (on (pChOn)) chorus.process (ch, 1, n);    break;
            case Pedal::delay:     if (on (pDlOn)) delay.process (ch, 1, n);     break;
            case Pedal::reverb:    if (on (pRvOn)) reverb.process (ch, 1, n);    break;
            case Pedal::amp:       break; // o amplificador de verdade já faz esse papel
        }
    }

    const RigState& state;
    NoiseGate gate;
    Overdrive overdrive;
    Eq3 eq;
    Chorus chorus;
    Delay delay;
    Reverb reverb;

    std::array<float, kNumParams> cache {};
    bool cacheValid = false;
    float lastIn = 1.0f, lastOut = 1.0f;
    float peakIn = 0.0f, peakOut = 0.0f;
    bool gateIsOpen = false;
};
} // namespace hushrig
