#pragma once

#include <string>

#include "Rig.h"

// Protocolo de controle (JSON, igual em WiFi/WebSocket e em BLE).
//   celular -> pedal: {"set":"odDrive","v":20} {"on":false} {"order":[0,5,1,2,3,4]}
//                     {"save":1} {"load":2} {"reset":1} {"hello":1}
//   pedal -> celular: {"v":1,"p":[...],"order":[...],"on":1,"slot":0,"slots":4}   (estado; "p" na ordem de Rig.h)
//                     {"m":[entrada,saida,gate,bateria%,cargaDSP,falhas]}          (medidores)
namespace protocol
{
struct Result
{
    bool replyState = false;     // mandar o estado só para quem perguntou
    bool broadcastState = false; // mandar o estado para todos
};

void init (hushrig::RigState& state);
Result handle (const char* json);
std::string stateJson();
std::string metersJson();
} // namespace protocol
