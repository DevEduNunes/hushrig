#pragma once

#include <string>

#include "Rig.h"

namespace net
{
/** Sobe o ponto de acesso WiFi + servidor web (página de controle e WebSocket). */
void start();

/** Envia um texto a todos os WebSockets conectados. */
void broadcastText (const std::string& text);
} // namespace net
