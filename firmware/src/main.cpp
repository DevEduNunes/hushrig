#include "audio.h"
#include "battery.h"
#include "config.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "net.h"
#include "storage.h"

namespace
{
hushrig::RigState gState;

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
                net::notifyStateChanged();
            }
        }
        gpio_set_level (cfg::kLed, audio::isEngaged() ? 1 : 0);
    }
}
} // namespace

extern "C" void app_main()
{
    storage::init();
    storage::load (storage::lastSlot(), gState); // sem slot salvo, ficam os padrões

    battery::start();
    audio::start (gState);
    net::start (gState);
    xTaskCreatePinnedToCore (controlsTask, "controls", 3072, nullptr, 2, nullptr, 0);
}
