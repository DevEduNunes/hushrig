#include "audio.h"

#include <atomic>
#include <cmath>
#include <cstdint>

#include "config.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace
{
const char* TAG = "audio";

i2s_chan_handle_t txChan = nullptr;
i2s_chan_handle_t rxChan = nullptr;

std::atomic<float> gInPeak { 0.0f }, gOutPeak { 0.0f }, gLoad { 0.0f };
std::atomic<bool> gGateOpen { false };
std::atomic<bool> gEngaged { true };
std::atomic<unsigned long long> gUnderruns { 0 };

constexpr float kToFloat = 1.0f / 2147483648.0f;

void initI2s()
{
    i2s_chan_config_t chanCfg = I2S_CHANNEL_DEFAULT_CONFIG (I2S_NUM_0, I2S_ROLE_MASTER);
    chanCfg.dma_desc_num  = 3;
    chanCfg.dma_frame_num = cfg::kBlockFrames;
    chanCfg.auto_clear    = true; // se faltar dado, toca silêncio em vez de repetir o último bloco
    ESP_ERROR_CHECK (i2s_new_channel (&chanCfg, &txChan, &rxChan));

    i2s_std_config_t stdCfg = {};
    stdCfg.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG (cfg::kSampleRate);
    stdCfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256; // PCM1808 aceita 256/384/512 fs
    stdCfg.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG (I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO);
    stdCfg.gpio_cfg.mclk = cfg::kI2sMclk;
    stdCfg.gpio_cfg.bclk = cfg::kI2sBclk;
    stdCfg.gpio_cfg.ws   = cfg::kI2sWs;
    stdCfg.gpio_cfg.dout = cfg::kI2sDout;
    stdCfg.gpio_cfg.din  = cfg::kI2sDin;

    ESP_ERROR_CHECK (i2s_channel_init_std_mode (txChan, &stdCfg));
    ESP_ERROR_CHECK (i2s_channel_init_std_mode (rxChan, &stdCfg));
    ESP_ERROR_CHECK (i2s_channel_enable (txChan));
    ESP_ERROR_CHECK (i2s_channel_enable (rxChan));
}

void audioTask (void* arg)
{
    auto& state = *static_cast<hushrig::RigState*> (arg);
    auto* rig = new hushrig::Rig (state);
    rig->prepare (cfg::kSampleRate);

    constexpr int N = cfg::kBlockFrames;
    static int32_t frames[N * 2];
    static float dry[N], wet[N];

    const float blockUs = 1.0e6f * N / cfg::kSampleRate;
    float mix = 1.0f;             // 1 = efeito, 0 = bypass; muda em ~5 ms para não estalar
    const float mixStep = 1.0f / (0.005f * cfg::kSampleRate);
    float load = 0.0f;

    for (;;)
    {
        size_t bytes = 0;
        if (i2s_channel_read (rxChan, frames, sizeof frames, &bytes, portMAX_DELAY) != ESP_OK || bytes != sizeof frames)
            continue;

        const int64_t t0 = esp_timer_get_time();

        for (int i = 0; i < N; ++i) // guitarra no canal esquerdo do ADC
            dry[i] = wet[i] = static_cast<float> (frames[2 * i]) * kToFloat;

        rig->process (wet, N);

        const float target = gEngaged.load (std::memory_order_relaxed) ? 1.0f : 0.0f;
        for (int i = 0; i < N; ++i)
        {
            mix += mix < target ? mixStep : (mix > target ? -mixStep : 0.0f);
            mix = std::fmin (std::fmax (mix, 0.0f), 1.0f);
            float out = dry[i] + (wet[i] - dry[i]) * mix;
            out = std::fmin (std::fmax (out, -1.0f), 0.999999f); // sem overflow no conversor
            const int32_t s = static_cast<int32_t> (out * 2147483647.0f);
            frames[2 * i] = frames[2 * i + 1] = s;
        }

        size_t written = 0;
        i2s_channel_write (txChan, frames, sizeof frames, &written, portMAX_DELAY);

        const float used = static_cast<float> (esp_timer_get_time() - t0) / blockUs;
        load += 0.05f * (used - load);
        gLoad.store (load, std::memory_order_relaxed);
        gInPeak.store (rig->inputPeak(), std::memory_order_relaxed);
        gOutPeak.store (rig->outputPeak(), std::memory_order_relaxed);
        gGateOpen.store (rig->gateOpen(), std::memory_order_relaxed);
        if (used > 1.0f)
            gUnderruns.fetch_add (1, std::memory_order_relaxed);
    }
}
} // namespace

namespace audio
{
void start (hushrig::RigState& state)
{
    initI2s();
    // Núcleo 1 só para áudio; o WiFi e o servidor web ficam no núcleo 0.
    xTaskCreatePinnedToCore (audioTask, "audio", 8192, &state, configMAX_PRIORITIES - 2, nullptr, 1);
    ESP_LOGI (TAG, "I2S %d Hz, bloco de %d amostras (~%.2f ms)", cfg::kSampleRate, cfg::kBlockFrames,
              1000.0f * cfg::kBlockFrames / cfg::kSampleRate);
}

void setEngaged (bool engaged) { gEngaged.store (engaged, std::memory_order_relaxed); }
bool isEngaged() { return gEngaged.load (std::memory_order_relaxed); }

Meters meters()
{
    return { gInPeak.load (std::memory_order_relaxed), gOutPeak.load (std::memory_order_relaxed),
             gGateOpen.load (std::memory_order_relaxed), gLoad.load (std::memory_order_relaxed),
             gUnderruns.load (std::memory_order_relaxed) };
}
} // namespace audio
