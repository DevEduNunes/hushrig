#pragma once

#include "driver/gpio.h"
#include "hal/adc_types.h"
#include "sdkconfig.h"

// Ligações e ajustes da placa. Veja docs/hardware/README.md para o esquema completo.

// Transportes de controle (platformio.ini define por ambiente). O ESP32 barato usa só BLE.
#ifndef HUSHRIG_BLE
#define HUSHRIG_BLE 1
#endif
#ifndef HUSHRIG_WIFI
#define HUSHRIG_WIFI 0
#endif

namespace cfg
{
// --- Áudio (I2S full-duplex, o ESP32 é o mestre do clock) ---------------------
constexpr int kSampleRate  = 48000;
constexpr int kBlockFrames = 32; // 32 = ~0,7 ms por bloco. Se estalar, suba para 64.

#if CONFIG_IDF_TARGET_ESP32S3
// ESP32-S3
constexpr gpio_num_t kI2sMclk = GPIO_NUM_4; // -> SCK/SCKI do PCM1808 (o PCM5102A usa PLL interno)
constexpr gpio_num_t kI2sBclk = GPIO_NUM_5; // -> BCK de PCM1808 e PCM5102A
constexpr gpio_num_t kI2sWs   = GPIO_NUM_6; // -> LRCK de PCM1808 e PCM5102A
constexpr gpio_num_t kI2sDout = GPIO_NUM_7; // -> DIN do PCM5102A
constexpr gpio_num_t kI2sDin  = GPIO_NUM_8; // <- OUT do PCM1808
constexpr gpio_num_t kFootswitch = GPIO_NUM_9;  // botão para GND (pull-up interno)
constexpr gpio_num_t kLed        = GPIO_NUM_10; // LED (com resistor 1k) aceso = efeito ligado
constexpr adc_channel_t kBatteryChannel = ADC_CHANNEL_0; // GPIO1
#else
// ESP32 clássico, DevKitC de 38 pinos (o de 30 pinos NÃO expõe o GPIO0). No ESP32 o MCLK do I2S só sai nos GPIO0, 1 ou 3.
constexpr gpio_num_t kI2sMclk = GPIO_NUM_0;
constexpr gpio_num_t kI2sBclk = GPIO_NUM_27;
constexpr gpio_num_t kI2sWs   = GPIO_NUM_26;
constexpr gpio_num_t kI2sDout = GPIO_NUM_25;
constexpr gpio_num_t kI2sDin  = GPIO_NUM_33;
constexpr gpio_num_t kFootswitch = GPIO_NUM_32;
constexpr gpio_num_t kLed        = GPIO_NUM_2; // LED azul da própria placa na maioria dos DevKits
constexpr adc_channel_t kBatteryChannel = ADC_CHANNEL_6; // GPIO34 (só entrada)
#endif

// Divisor 100k/100k do + da bateria até o pino acima. Sem bateria (power bank), a leitura fica ~0 e a UI mostra 0%.
constexpr float kBatteryDivider = 2.0f;

// --- BLE ----------------------------------------------------------------------
constexpr const char* kBleName = "HushRig";

// --- WiFi (só se HUSHRIG_WIFI=1): ponto de acesso do próprio pedal ---------------
// TROQUE a senha antes de usar em público. Mínimo de 8 caracteres.
constexpr const char* kApSsid     = "HushRig";
constexpr const char* kApPassword = "hushrig123";
constexpr int kApChannel = 6;
constexpr int kApMaxClients = 2;
} // namespace cfg
