#pragma once

namespace battery
{
void start();
/** Tensão da célula em volts (média), ou 0 se a leitura falhou. */
float volts();
/** Carga aproximada, 0..100 (curva típica de Li-ion; não é um medidor de verdade). */
int percent();
} // namespace battery
