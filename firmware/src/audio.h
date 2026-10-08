#pragma once

#include "Rig.h"

namespace audio
{
struct Meters
{
    float inPeak, outPeak; // linear, 0..1
    bool gateOpen;
    float cpuLoad;         // 0..1: tempo gasto no DSP / duração do bloco
    unsigned long long underruns;
};

/** Inicia o I2S e a tarefa de áudio (núcleo 1). `state` precisa viver para sempre. */
void start (hushrig::RigState& state);

/** Efeito ligado (true) ou bypass digital (false). */
void setEngaged (bool engaged);
bool isEngaged();

Meters meters();
} // namespace audio
