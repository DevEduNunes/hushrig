#pragma once

#include "Rig.h"

namespace net
{
/** Sobe o ponto de acesso WiFi + servidor web (página de controle e WebSocket). */
void start (hushrig::RigState& state);

/** Avisa os celulares conectados que o estado mudou por fora (ex.: footswitch). */
void notifyStateChanged();
} // namespace net
