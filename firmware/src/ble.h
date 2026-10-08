#pragma once

#include <string>

namespace ble
{
/** Sobe o NimBLE e começa a anunciar "HushRig" (serviço GATT estilo UART: RX escreve, TX notifica). */
void start();

/** Envia uma linha de JSON ao celular conectado. `droppable`: pode ser descartada se o rádio estiver ocupado (medidores). */
void send (const std::string& json, bool droppable);
} // namespace ble
