#include "net.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "audio.h"
#include "battery.h"
#include "cJSON.h"
#include "config.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "storage.h"

extern const uint8_t indexHtmlStart[] asm ("_binary_index_html_start");
extern const uint8_t indexHtmlEnd[] asm ("_binary_index_html_end");

namespace
{
const char* TAG = "net";
constexpr size_t kMaxWsMessage = 512;

hushrig::RigState* gState = nullptr;
httpd_handle_t gServer = nullptr;
int gSlot = 0;

// ---- envio ------------------------------------------------------------------
void broadcast (const char* text, size_t len)
{
    if (gServer == nullptr)
        return;
    int fds[cfg::kApMaxClients + 4];
    size_t n = sizeof fds / sizeof fds[0];
    if (httpd_get_client_list (gServer, &n, fds) != ESP_OK)
        return;
    for (size_t i = 0; i < n; ++i)
        if (httpd_ws_get_fd_info (gServer, fds[i]) == HTTPD_WS_CLIENT_WEBSOCKET)
        {
            httpd_ws_frame_t f = {};
            f.type = HTTPD_WS_TYPE_TEXT;
            f.payload = reinterpret_cast<uint8_t*> (const_cast<char*> (text));
            f.len = len;
            httpd_ws_send_frame_async (gServer, fds[i], &f);
        }
}

// Estado completo: valores, faixas (fonte única da verdade para a UI), ordem, bypass e slot.
char* buildState()
{
    cJSON* root = cJSON_CreateObject();
    cJSON* values = cJSON_AddObjectToObject (root, "p");
    cJSON* ranges = cJSON_AddObjectToObject (root, "r");
    for (int i = 0; i < hushrig::kNumParams; ++i)
    {
        const auto& info = hushrig::paramTable()[static_cast<size_t> (i)];
        cJSON_AddNumberToObject (values, info.id, gState->get (static_cast<hushrig::Param> (i)));
        cJSON* r = cJSON_AddArrayToObject (ranges, info.id);
        cJSON_AddItemToArray (r, cJSON_CreateNumber (info.min));
        cJSON_AddItemToArray (r, cJSON_CreateNumber (info.max));
    }
    cJSON* order = cJSON_AddArrayToObject (root, "order");
    for (int s : gState->getOrder().slots)
        cJSON_AddItemToArray (order, cJSON_CreateNumber (s));
    cJSON_AddBoolToObject (root, "on", audio::isEngaged());
    cJSON_AddNumberToObject (root, "slot", gSlot);
    cJSON_AddNumberToObject (root, "slots", storage::kNumSlots);
    char* text = cJSON_PrintUnformatted (root);
    cJSON_Delete (root);
    return text;
}

void sendStateTo (int fd)
{
    char* text = buildState();
    if (text == nullptr)
        return;
    httpd_ws_frame_t f = {};
    f.type = HTTPD_WS_TYPE_TEXT;
    f.payload = reinterpret_cast<uint8_t*> (text);
    f.len = std::strlen (text);
    httpd_ws_send_frame_async (gServer, fd, &f);
    cJSON_free (text);
}

void broadcastState()
{
    char* text = buildState();
    if (text == nullptr)
        return;
    broadcast (text, std::strlen (text));
    cJSON_free (text);
}

// ---- recebimento --------------------------------------------------------------
void handleMessage (int fd, const char* text)
{
    cJSON* msg = cJSON_Parse (text);
    if (msg == nullptr)
        return;

    const cJSON* set = cJSON_GetObjectItem (msg, "set");
    const cJSON* v = cJSON_GetObjectItem (msg, "v");
    if (cJSON_IsString (set) && cJSON_IsNumber (v))
        gState->setById (set->valuestring, static_cast<float> (v->valuedouble));

    if (const cJSON* on = cJSON_GetObjectItem (msg, "on"); cJSON_IsBool (on))
    {
        audio::setEngaged (cJSON_IsTrue (on));
        broadcastState();
    }

    if (const cJSON* order = cJSON_GetObjectItem (msg, "order"); cJSON_IsArray (order))
    {
        hushrig::ChainOrder o;
        if (cJSON_GetArraySize (order) == hushrig::kNumPedals)
        {
            for (int i = 0; i < hushrig::kNumPedals; ++i)
            {
                const cJSON* item = cJSON_GetArrayItem (order, i);
                o.slots[static_cast<size_t> (i)] = cJSON_IsNumber (item) ? item->valueint : -1;
            }
            gState->setOrder (o); // inválida = ignorada
        }
        broadcastState();
    }

    if (const cJSON* save = cJSON_GetObjectItem (msg, "save"); cJSON_IsNumber (save))
    {
        if (storage::save (save->valueint, *gState))
        {
            gSlot = save->valueint;
            storage::setLastSlot (gSlot);
        }
        broadcastState();
    }

    if (const cJSON* load = cJSON_GetObjectItem (msg, "load"); cJSON_IsNumber (load))
    {
        if (storage::load (load->valueint, *gState))
        {
            gSlot = load->valueint;
            storage::setLastSlot (gSlot);
        }
        broadcastState();
    }

    if (cJSON_GetObjectItem (msg, "reset") != nullptr)
    {
        gState->resetToDefaults();
        broadcastState();
    }

    if (cJSON_GetObjectItem (msg, "hello") != nullptr)
        sendStateTo (fd);

    cJSON_Delete (msg);
}

esp_err_t wsHandler (httpd_req_t* req)
{
    if (req->method == HTTP_GET)
        return ESP_OK; // handshake do WebSocket; o cliente manda "hello" em seguida

    httpd_ws_frame_t frame = {};
    frame.type = HTTPD_WS_TYPE_TEXT;
    esp_err_t err = httpd_ws_recv_frame (req, &frame, 0); // só o tamanho
    if (err != ESP_OK || frame.len == 0 || frame.len > kMaxWsMessage)
        return ESP_FAIL;

    char buf[kMaxWsMessage + 1];
    frame.payload = reinterpret_cast<uint8_t*> (buf);
    err = httpd_ws_recv_frame (req, &frame, frame.len);
    if (err != ESP_OK)
        return err;
    if (frame.type != HTTPD_WS_TYPE_TEXT)
        return ESP_OK;
    buf[frame.len] = '\0';

    handleMessage (httpd_req_to_sockfd (req), buf);
    return ESP_OK;
}

esp_err_t indexHandler (httpd_req_t* req)
{
    httpd_resp_set_type (req, "text/html; charset=utf-8");
    return httpd_resp_send (req, reinterpret_cast<const char*> (indexHtmlStart),
                            static_cast<ssize_t> (indexHtmlEnd - indexHtmlStart - 1)); // -1: terminador nulo do embed
}

// Android testa "generate_204" para detectar portal cativo; responder 204 evita o aviso de "sem internet".
esp_err_t noContentHandler (httpd_req_t* req)
{
    httpd_resp_set_status (req, "204 No Content");
    return httpd_resp_send (req, nullptr, 0);
}

// ---- medidores (10 Hz) --------------------------------------------------------
void meterTask (void*)
{
    int tick = 0;
    int batt = battery::percent();
    for (;;)
    {
        vTaskDelay (pdMS_TO_TICKS (100));
        if (++tick % 50 == 0) // bateria a cada 5 s
            batt = battery::percent();

        const auto m = audio::meters();
        char text[128];
        const int len = std::snprintf (text, sizeof text,
                                       "{\"m\":[%.4f,%.4f,%d,%d,%.2f,%llu]}", m.inPeak, m.outPeak,
                                       m.gateOpen ? 1 : 0, batt, m.cpuLoad, m.underruns);
        broadcast (text, static_cast<size_t> (len));
    }
}

// ---- WiFi -----------------------------------------------------------------------
void startAccessPoint()
{
    ESP_ERROR_CHECK (esp_netif_init());
    ESP_ERROR_CHECK (esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK (esp_wifi_init (&init));

    wifi_config_t ap = {};
    std::strncpy (reinterpret_cast<char*> (ap.ap.ssid), cfg::kApSsid, sizeof ap.ap.ssid - 1);
    std::strncpy (reinterpret_cast<char*> (ap.ap.password), cfg::kApPassword, sizeof ap.ap.password - 1);
    ap.ap.ssid_len = static_cast<uint8_t> (std::strlen (cfg::kApSsid));
    ap.ap.channel = cfg::kApChannel;
    ap.ap.max_connection = cfg::kApMaxClients;
    ap.ap.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK (esp_wifi_set_mode (WIFI_MODE_AP));
    ESP_ERROR_CHECK (esp_wifi_set_config (WIFI_IF_AP, &ap));
    ESP_ERROR_CHECK (esp_wifi_set_ps (WIFI_PS_NONE)); // sem economia de energia: resposta rápida dos sliders
    ESP_ERROR_CHECK (esp_wifi_start());
    ESP_LOGI (TAG, "WiFi '%s' ativo; abra http://192.168.4.1", cfg::kApSsid);
}
} // namespace

namespace net
{
void start (hushrig::RigState& state)
{
    gState = &state;
    gSlot = storage::lastSlot();
    startAccessPoint();

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = cfg::kApMaxClients + 3;
    config.lru_purge_enable = true;
    config.send_wait_timeout = 1; // cliente travado não pode segurar a tarefa de medidores
    config.stack_size = 6144;
    config.core_id = 0;           // o núcleo 1 é só do áudio
    ESP_ERROR_CHECK (httpd_start (&gServer, &config));

    httpd_uri_t index = {};
    index.uri = "/";
    index.method = HTTP_GET;
    index.handler = indexHandler;
    httpd_register_uri_handler (gServer, &index);

    httpd_uri_t gen204 = {};
    gen204.uri = "/generate_204";
    gen204.method = HTTP_GET;
    gen204.handler = noContentHandler;
    httpd_register_uri_handler (gServer, &gen204);

    httpd_uri_t ws = {};
    ws.uri = "/ws";
    ws.method = HTTP_GET;
    ws.handler = wsHandler;
    ws.is_websocket = true;
    httpd_register_uri_handler (gServer, &ws);

    xTaskCreatePinnedToCore (meterTask, "meters", 4096, nullptr, 3, nullptr, 0);
}

void notifyStateChanged() { broadcastState(); }
} // namespace net
