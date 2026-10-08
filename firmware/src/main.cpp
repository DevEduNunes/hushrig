#include "app.h"
#include "audio.h"
#include "battery.h"
#include "config.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "protocol.h"
#include "storage.h"

#if HUSHRIG_WIFI
#include "net.h"
#endif
#if HUSHRIG_BLE
#include "ble.h"
#endif

namespace
{
hushrig::RigState gState;

void sendToAll (const std::string& json, bool droppable)
{
#if HUSHRIG_WIFI
    net::broadcastText (json);
#endif
#if HUSHRIG_BLE
    ble::send (json, droppable);
#endif
    (void) json;
    (void) droppable;
}

// Medidores 10×/s para quem estiver conectado.
void meterTask (void*)
{
    for (;;)
    {
        vTaskDelay (pdMS_TO_TICKS (100));
        sendToAll (protocol::metersJson(), true);
    }
}

// Footswitch: toque curto liga/desliga o efeito. Debounce de 30 ms.
void controlsTask (void*)
{
    gpio_config_t sw = {};
    sw.pin_bit_mask = 1ULL << cfg::kFootswitch;
    sw.mode = GPIO_MODE_INPUT;
    sw.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config (&sw);

    gpio_config_t led = {};
    led.pin_bit_mask = 1ULL << cfg::kLed;
    led.mode = GPIO_MODE_OUTPUT;
    gpio_config (&led);

    bool wasDown = false;
    int stable = 0;
    for (;;)
    {
        vTaskDelay (pdMS_TO_TICKS (10));
        const bool down = gpio_get_level (cfg::kFootswitch) == 0;
        stable = down == wasDown ? 0 : stable + 1;
        if (stable >= 3)
        {
            wasDown = down;
            stable = 0;
            if (down)
            {
                audio::setEngaged (! audio::isEngaged());
                app::broadcastState();
            }
        }
        gpio_set_level (cfg::kLed, audio::isEngaged() ? 1 : 0);
    }
}
} // namespace

namespace app
{
void broadcastState() { sendToAll (protocol::stateJson(), false); }
} // namespace app

extern "C" void app_main()
{
    storage::init();
    storage::load (storage::lastSlot(), gState); // sem slot salvo, ficam os padrões
    protocol::init (gState);

    battery::start();
    audio::start (gState);
#if HUSHRIG_WIFI
    net::start();
#endif
#if HUSHRIG_BLE
    ble::start();
#endif
    xTaskCreatePinnedToCore (meterTask, "meters", 4096, nullptr, 3, nullptr, 0);
    xTaskCreatePinnedToCore (controlsTask, "controls", 3072, nullptr, 2, nullptr, 0);
}
