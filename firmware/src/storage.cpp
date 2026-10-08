#include "storage.h"

#include <cstdio>
#include <cstring>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

namespace
{
const char* TAG = "storage";
const char* NS = "hushrig";
constexpr uint32_t kMagic = 0x48555231; // "HUR1": mude se o formato do blob mudar

struct Blob
{
    uint32_t magic;
    uint32_t numParams;
    float values[hushrig::kNumParams];
    int32_t order[hushrig::kNumPedals];
};

bool validSlot (int slot) { return slot >= 0 && slot < storage::kNumSlots; }
} // namespace

namespace storage
{
void init()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK (nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK (err);
}

bool save (int slot, const hushrig::RigState& state)
{
    if (! validSlot (slot))
        return false;

    Blob b {};
    b.magic = kMagic;
    b.numParams = hushrig::kNumParams;
    for (int i = 0; i < hushrig::kNumParams; ++i)
        b.values[i] = state.get (static_cast<hushrig::Param> (i));
    const auto order = state.getOrder();
    for (int i = 0; i < hushrig::kNumPedals; ++i)
        b.order[i] = order.slots[static_cast<size_t> (i)];

    nvs_handle_t h;
    if (nvs_open (NS, NVS_READWRITE, &h) != ESP_OK)
        return false;
    char key[8];
    std::snprintf (key, sizeof key, "slot%d", slot);
    bool ok = nvs_set_blob (h, key, &b, sizeof b) == ESP_OK && nvs_commit (h) == ESP_OK;
    nvs_close (h);
    if (! ok)
        ESP_LOGW (TAG, "falha ao salvar o slot %d", slot);
    return ok;
}

bool load (int slot, hushrig::RigState& state)
{
    if (! validSlot (slot))
        return false;

    nvs_handle_t h;
    if (nvs_open (NS, NVS_READONLY, &h) != ESP_OK)
        return false;
    char key[8];
    std::snprintf (key, sizeof key, "slot%d", slot);
    Blob b {};
    size_t len = sizeof b;
    const bool ok = nvs_get_blob (h, key, &b, &len) == ESP_OK && len == sizeof b;
    nvs_close (h);

    if (! ok || b.magic != kMagic || b.numParams != hushrig::kNumParams)
        return false;

    for (int i = 0; i < hushrig::kNumParams; ++i)
        state.set (static_cast<hushrig::Param> (i), b.values[i]); // set() limita à faixa
    hushrig::ChainOrder order;
    for (int i = 0; i < hushrig::kNumPedals; ++i)
        order.slots[static_cast<size_t> (i)] = b.order[i];
    state.setOrder (order); // ordem inválida é ignorada
    return true;
}

int lastSlot()
{
    nvs_handle_t h;
    int8_t v = 0;
    if (nvs_open (NS, NVS_READONLY, &h) == ESP_OK)
    {
        nvs_get_i8 (h, "last", &v);
        nvs_close (h);
    }
    return validSlot (v) ? v : 0;
}

void setLastSlot (int slot)
{
    nvs_handle_t h;
    if (validSlot (slot) && nvs_open (NS, NVS_READWRITE, &h) == ESP_OK)
    {
        nvs_set_i8 (h, "last", static_cast<int8_t> (slot));
        nvs_commit (h);
        nvs_close (h);
    }
}
} // namespace storage
