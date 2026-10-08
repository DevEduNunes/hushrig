#pragma once

#include "driver/gpio.h"
#include "hal/adc_types.h"

// Ligações e ajustes da placa. Veja docs/hardware/README.md para o esquema completo.
namespace cfg
{
// --- Áudio (I2S full-duplex, o ESP32 é o mestre do clock) ---------------------
constexpr int kSampleRate  = 48000;
constexpr int kBlockFrames = 32; // 32 = ~0,7 ms por bloco. Se estalar, suba para 64.

constexpr gpio_num_t kI2sMclk = GPIO_NUM_4; // -> SCK/SCKI do PCM1808 (o PCM5102A usa PLL interno)
constexpr gpio_num_t kI2sBclk = GPIO_NUM_5; // -> BCK de PCM1808 e PCM5102A
constexpr gpio_num_t kI2sWs   = GPIO_NUM_6; // -> LRCK de PCM1808 e PCM5102A
constexpr gpio_num_t kI2sDout = GPIO_NUM_7; // -> DIN do PCM5102A
constexpr gpio_num_t kI2sDin  = GPIO_NUM_8; // <- OUT do PCM1808

// --- Controles --------------------------------------------------------------
constexpr gpio_num_t kFootswitch = GPIO_NUM_9;  // botão para GND (pull-up interno): liga/desliga o efeito
constexpr gpio_num_t kLed        = GPIO_NUM_10; // LED (com resistor) aceso = efeito ligado

// --- Bateria: divisor 100k/100k do + da bateria até este pino (GPIO1 = ADC1_CH0) ---
constexpr adc_channel_t kBatteryChannel = ADC_CHANNEL_0;
constexpr float kBatteryDivider = 2.0f;

// --- WiFi (ponto de acesso do próprio pedal) ----------------------------------
// TROQUE a senha antes de usar em público. Mínimo de 8 caracteres.
constexpr const char* kApSsid     = "HushRig";
constexpr const char* kApPassword = "hushrig123";
constexpr int kApChannel = 6;
constexpr int kApMaxClients = 2;
} // namespace cfg
