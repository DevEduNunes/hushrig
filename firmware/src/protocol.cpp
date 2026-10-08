#include "protocol.h"

#include <cstdio>

#include "audio.h"
#include "battery.h"
#include "cJSON.h"
#include "esp_timer.h"
#include "storage.h"

namespace
{
hushrig::RigState* gState = nullptr;
int gSlot = 0;
} // namespace

namespace protocol
{
void init (hushrig::RigState& state)
{
    gState = &state;
    gSlot = storage::lastSlot();
}

std::string stateJson()
{
    std::string s = "{\"v\":1,\"p\":[";
    char b[32];
    for (int i = 0; i < hushrig::kNumParams; ++i)
    {
        std::snprintf (b, sizeof b, "%s%.5g", i ? "," : "", static_cast<double> (gState->get (static_cast<hushrig::Param> (i))));
        s += b;
    }
    s += "],\"order\":[";
    const auto order = gState->getOrder();
    for (size_t i = 0; i < order.slots.size(); ++i)
    {
        std::snprintf (b, sizeof b, "%s%d", i ? "," : "", order.slots[i]);
        s += b;
    }
    std::snprintf (b, sizeof b, "],\"on\":%d,\"slot\":%d,\"slots\":%d}", audio::isEngaged() ? 1 : 0, gSlot, storage::kNumSlots);
    s += b;
    return s;
}

std::string metersJson()
{
    static int batt = -1;
    static int64_t lastBattUs = 0;
    const int64_t now = esp_timer_get_time();
    if (batt < 0 || now - lastBattUs > 5'000'000) // a bateria muda devagar: lê a cada 5 s
    {
        batt = battery::percent();
        lastBattUs = now;
    }

    const auto m = audio::meters();
    char text[128];
    std::snprintf (text, sizeof text, "{\"m\":[%.4f,%.4f,%d,%d,%.2f,%llu]}", static_cast<double> (m.inPeak),
                   static_cast<double> (m.outPeak), m.gateOpen ? 1 : 0, batt, static_cast<double> (m.cpuLoad), m.underruns);
    return text;
}

Result handle (const char* json)
{
    Result result;
    cJSON* msg = cJSON_Parse (json);
    if (msg == nullptr)
        return result;

    const cJSON* set = cJSON_GetObjectItem (msg, "set");
    const cJSON* v = cJSON_GetObjectItem (msg, "v");
    if (cJSON_IsString (set) && cJSON_IsNumber (v))
        gState->setById (set->valuestring, static_cast<float> (v->valuedouble));

    if (const cJSON* on = cJSON_GetObjectItem (msg, "on"); cJSON_IsBool (on))
    {
        audio::setEngaged (cJSON_IsTrue (on));
        result.broadcastState = true;
    }

    if (const cJSON* order = cJSON_GetObjectItem (msg, "order"); cJSON_IsArray (order))
    {
        if (cJSON_GetArraySize (order) == hushrig::kNumPedals)
        {
            hushrig::ChainOrder o;
            for (int i = 0; i < hushrig::kNumPedals; ++i)
            {
                const cJSON* item = cJSON_GetArrayItem (order, i);
                o.slots[static_cast<size_t> (i)] = cJSON_IsNumber (item) ? item->valueint : -1;
            }
            gState->setOrder (o); // inválida = ignorada
        }
        result.broadcastState = true;
    }

    if (const cJSON* save = cJSON_GetObjectItem (msg, "save"); cJSON_IsNumber (save))
    {
        if (storage::save (save->valueint, *gState))
        {
            gSlot = save->valueint;
            storage::setLastSlot (gSlot);
        }
        result.broadcastState = true;
    }

    if (const cJSON* load = cJSON_GetObjectItem (msg, "load"); cJSON_IsNumber (load))
    {
        if (storage::load (load->valueint, *gState))
        {
            gSlot = load->valueint;
            storage::setLastSlot (gSlot);
        }
        result.broadcastState = true;
    }

    if (cJSON_GetObjectItem (msg, "reset") != nullptr)
    {
        gState->resetToDefaults();
        result.broadcastState = true;
    }

    if (cJSON_GetObjectItem (msg, "hello") != nullptr)
        result.replyState = true;

    cJSON_Delete (msg);
    return result;
}
} // namespace protocol
