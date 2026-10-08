#include "ble.h"
#include "config.h"

#if HUSHRIG_BLE

#include <algorithm>
#include <cstring>

#include "app.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "protocol.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

// UUIDs (os bytes vão em ordem inversa). Mantenha iguais aos de firmware/src/web/index.html.
//   serviço a7c9e0f1-0b3d-4e5a-9c1f-4d6b2e8a1001   RX ...1002 (celular escreve)   TX ...1003 (pedal notifica)
#define HUSHRIG_UUID(last) BLE_UUID128_INIT (last, 0x10, 0x8a, 0x2e, 0x6b, 0x4d, 0x1f, 0x9c, 0x5a, 0x4e, 0x3d, 0x0b, 0xf1, 0xe0, 0xc9, 0xa7)

namespace
{
const char* TAG = "ble";

const ble_uuid128_t kSvcUuid = HUSHRIG_UUID (0x01);
const ble_uuid128_t kRxUuid  = HUSHRIG_UUID (0x02);
const ble_uuid128_t kTxUuid  = HUSHRIG_UUID (0x03);

uint8_t gOwnAddrType = 0;
uint16_t gConn = BLE_HS_CONN_HANDLE_NONE;
uint16_t gTxHandle = 0;
volatile bool gNotify = false;
QueueHandle_t gTxQueue = nullptr;
std::string gRxLine;

void advertise();

// Cada mensagem é um JSON terminado em '\n'; o BLE a fatia em pedaços do tamanho do MTU.
void feedRx (const uint8_t* data, uint16_t len)
{
    for (uint16_t i = 0; i < len; ++i)
    {
        const char c = static_cast<char> (data[i]);
        if (c == '\n')
        {
            const auto result = protocol::handle (gRxLine.c_str());
            gRxLine.clear();
            if (result.broadcastState)
                app::broadcastState();
            else if (result.replyState)
                ble::send (protocol::stateJson(), false);
        }
        else if (gRxLine.size() < 512)
            gRxLine += c;
        else
            gRxLine.clear(); // lixo: descarta e ressincroniza no próximo '\n'
    }
}

int rxAccess (uint16_t, uint16_t, ble_gatt_access_ctxt* ctxt, void*)
{
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR)
        return BLE_ATT_ERR_UNLIKELY;
    uint8_t buf[256];
    uint16_t len = 0;
    if (ble_hs_mbuf_to_flat (ctxt->om, buf, sizeof buf, &len) != 0)
        return BLE_ATT_ERR_UNLIKELY;
    feedRx (buf, len);
    return 0;
}

int txAccess (uint16_t, uint16_t, ble_gatt_access_ctxt*, void*)
{
    return BLE_ATT_ERR_READ_NOT_PERMITTED; // só notificações
}

const ble_gatt_chr_def kChars[] = {
    { .uuid = &kRxUuid.u, .access_cb = rxAccess, .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP },
    { .uuid = &kTxUuid.u, .access_cb = txAccess, .flags = BLE_GATT_CHR_F_NOTIFY, .val_handle = &gTxHandle },
    { 0 },
};

const ble_gatt_svc_def kServices[] = {
    { .type = BLE_GATT_SVC_TYPE_PRIMARY, .uuid = &kSvcUuid.u, .characteristics = kChars },
    { 0 },
};

int gapEvent (ble_gap_event* e, void*)
{
    switch (e->type)
    {
        case BLE_GAP_EVENT_CONNECT:
            if (e->connect.status == 0)
            {
                gConn = e->connect.conn_handle;
                // Pede intervalo curto (15–30 ms) para os sliders responderem rápido.
                ble_gap_upd_params p = {};
                p.itvl_min = 12;
                p.itvl_max = 24;
                p.supervision_timeout = 400;
                ble_gap_update_params (gConn, &p);
                ESP_LOGI (TAG, "conectado");
            }
            else
                advertise();
            break;
        case BLE_GAP_EVENT_DISCONNECT:
            gConn = BLE_HS_CONN_HANDLE_NONE;
            gNotify = false;
            gRxLine.clear();
            ESP_LOGI (TAG, "desconectado");
            advertise();
            break;
        case BLE_GAP_EVENT_SUBSCRIBE:
            if (e->subscribe.attr_handle == gTxHandle)
                gNotify = e->subscribe.cur_notify != 0;
            break;
        case BLE_GAP_EVENT_ADV_COMPLETE:
            advertise();
            break;
        default:
            break;
    }
    return 0;
}

void advertise()
{
    ble_hs_adv_fields fields = {};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    const char* name = ble_svc_gap_device_name();
    fields.name = reinterpret_cast<const uint8_t*> (name);
    fields.name_len = static_cast<uint8_t> (std::strlen (name));
    fields.name_is_complete = 1;
    ble_gap_adv_set_fields (&fields);

    ble_hs_adv_fields rsp = {};
    rsp.uuids128 = &kSvcUuid;
    rsp.num_uuids128 = 1;
    rsp.uuids128_is_complete = 1;
    ble_gap_adv_rsp_set_fields (&rsp);

    ble_gap_adv_params params = {};
    params.conn_mode = BLE_GAP_CONN_MODE_UND;
    params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    const int rc = ble_gap_adv_start (gOwnAddrType, nullptr, BLE_HS_FOREVER, &params, gapEvent, nullptr);
    if (rc != 0 && rc != BLE_HS_EALREADY)
        ESP_LOGW (TAG, "adv_start falhou: %d", rc);
}

void onSync()
{
    ble_hs_util_ensure_addr (0);
    ble_hs_id_infer_auto (0, &gOwnAddrType);
    advertise();
}

void onReset (int reason) { ESP_LOGW (TAG, "reset do host BLE: %d", reason); }

void hostTask (void*)
{
    nimble_port_run(); // só volta em nimble_port_stop()
    nimble_port_freertos_deinit();
}

// Fatia e envia as mensagens da fila. Roda fora do host do NimBLE para poder esperar sem travá-lo.
void txTask (void*)
{
    for (;;)
    {
        std::string* msg = nullptr;
        if (xQueueReceive (gTxQueue, &msg, portMAX_DELAY) != pdTRUE || msg == nullptr)
            continue;

        size_t offset = 0;
        while (offset < msg->size() && gNotify && gConn != BLE_HS_CONN_HANDLE_NONE)
        {
            const size_t chunk = std::max<size_t> (20, ble_att_mtu (gConn) - 3);
            const size_t n = std::min (chunk, msg->size() - offset);

            int rc = BLE_HS_ENOMEM;
            for (int attempt = 0; attempt < 10 && rc == BLE_HS_ENOMEM; ++attempt)
            {
                os_mbuf* om = ble_hs_mbuf_from_flat (msg->data() + offset, static_cast<uint16_t> (n));
                if (om == nullptr)
                {
                    vTaskDelay (pdMS_TO_TICKS (5));
                    continue;
                }
                rc = ble_gatts_notify_custom (gConn, gTxHandle, om); // consome o mbuf
                if (rc == BLE_HS_ENOMEM)
                    vTaskDelay (pdMS_TO_TICKS (5));
            }
            if (rc != 0)
                break; // desiste desta mensagem; o cliente ressincroniza no próximo '\n'
            offset += n;
            vTaskDelay (1);
        }
        delete msg;
    }
}
} // namespace

namespace ble
{
void start()
{
    gTxQueue = xQueueCreate (8, sizeof (std::string*));
    ESP_ERROR_CHECK (nimble_port_init());

    ble_hs_cfg.sync_cb = onSync;
    ble_hs_cfg.reset_cb = onReset;
    ble_svc_gap_init();
    ble_svc_gatt_init();
    ESP_ERROR_CHECK (ble_gatts_count_cfg (kServices));
    ESP_ERROR_CHECK (ble_gatts_add_svcs (kServices));
    ble_svc_gap_device_name_set (cfg::kBleName);
    ble_att_set_preferred_mtu (247);

    nimble_port_freertos_init (hostTask);
    xTaskCreatePinnedToCore (txTask, "ble_tx", 4096, nullptr, 3, nullptr, 0);
}

void send (const std::string& json, bool droppable)
{
    if (! gNotify || gTxQueue == nullptr)
        return;
    if (droppable && uxQueueMessagesWaiting (gTxQueue) > 2)
        return; // rádio ocupado: pula este medidor
    auto* msg = new std::string (json);
    *msg += '\n';
    if (xQueueSend (gTxQueue, &msg, 0) != pdTRUE)
        delete msg;
}
} // namespace ble

#endif // HUSHRIG_BLE
