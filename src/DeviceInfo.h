#pragma once

#include <juce_core/juce_core.h>

#include <optional>

/** Informações do dispositivo de áudio em uso (disponíveis no app standalone). */
struct DeviceStats
{
    bool fromDevice = false;       // false = não foi possível ler o dispositivo (ex.: rodando como plugin)
    juce::String deviceName;
    juce::String apiName;          // ex.: "ASIO", "Windows Audio"
    double sampleRate = 0.0;
    int bufferSamples = 0;
    int inputLatencySamples = 0;
    int outputLatencySamples = 0;
    double cpuUsage = 0.0;         // 0..1
};

/** Permite à ferramenta de screenshots simular um dispositivo. */
inline std::optional<DeviceStats>& deviceStatsOverride()
{
    static std::optional<DeviceStats> value;
    return value;
}

/** Lê o dispositivo atual; se não houver, devolve só a taxa e o buffer informados pelo host. */
DeviceStats queryDeviceStats (double fallbackSampleRate, int fallbackBufferSamples);
