#include "battery.h"

#include <algorithm>
#include <iterator>

#include "config.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

namespace
{
const char* TAG = "battery";
adc_oneshot_unit_handle_t adc = nullptr;
adc_cali_handle_t cali = nullptr;
} // namespace

namespace battery
{
void start()
{
    adc_oneshot_unit_init_cfg_t unit = {};
    unit.unit_id = ADC_UNIT_1;
    if (adc_oneshot_new_unit (&unit, &adc) != ESP_OK)
    {
        ESP_LOGW (TAG, "ADC indisponível; bateria não será exibida");
        adc = nullptr;
        return;
    }

    adc_oneshot_chan_cfg_t chan = {};
    chan.atten = ADC_ATTEN_DB_12; // até ~3,1 V no pino (a célula chega a 4,2 V /2 = 2,1 V)
    chan.bitwidth = ADC_BITWIDTH_DEFAULT;
    adc_oneshot_config_channel (adc, cfg::kBatteryChannel, &chan);

    adc_cali_curve_fitting_config_t calCfg = {};
    calCfg.unit_id = ADC_UNIT_1;
    calCfg.chan = cfg::kBatteryChannel;
    calCfg.atten = ADC_ATTEN_DB_12;
    calCfg.bitwidth = ADC_BITWIDTH_DEFAULT;
    if (adc_cali_create_scheme_curve_fitting (&calCfg, &cali) != ESP_OK)
        cali = nullptr; // sem calibração de fábrica: cai na estimativa linear abaixo
}

float volts()
{
    if (adc == nullptr)
        return 0.0f;

    int sum = 0, count = 0;
    for (int i = 0; i < 16; ++i)
    {
        int raw = 0;
        if (adc_oneshot_read (adc, cfg::kBatteryChannel, &raw) == ESP_OK)
        {
            int mv = raw * 3100 / 4095; // estimativa sem calibração
            if (cali != nullptr)
                adc_cali_raw_to_voltage (cali, raw, &mv);
            sum += mv;
            ++count;
        }
    }
    return count == 0 ? 0.0f : (static_cast<float> (sum) / count) * 1.0e-3f * cfg::kBatteryDivider;
}

int percent()
{
    struct Point { float v; int pct; };
    static constexpr Point curve[] = { { 3.30f, 0 }, { 3.60f, 10 }, { 3.70f, 25 }, { 3.80f, 45 },
                                       { 3.90f, 65 }, { 4.00f, 80 }, { 4.10f, 92 }, { 4.20f, 100 } };
    const float v = volts();
    if (v <= curve[0].v)
        return 0;
    for (size_t i = 1; i < std::size (curve); ++i)
        if (v <= curve[i].v)
        {
            const float t = (v - curve[i - 1].v) / (curve[i].v - curve[i - 1].v);
            return curve[i - 1].pct + static_cast<int> (t * static_cast<float> (curve[i].pct - curve[i - 1].pct));
        }
    return 100;
}
} // namespace battery
