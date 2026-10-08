#pragma once

#include "Rig.h"

namespace storage
{
constexpr int kNumSlots = 4;

void init();
bool save (int slot, const hushrig::RigState& state);
/** Carrega o slot no estado. Falso se estiver vazio ou corrompido (o estado não muda). */
bool load (int slot, hushrig::RigState& state);

int lastSlot();
void setLastSlot (int slot);
} // namespace storage
