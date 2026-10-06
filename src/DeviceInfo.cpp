#include "DeviceInfo.h"

#include <juce_audio_utils/juce_audio_utils.h>

#if ! HUSHRIG_NO_STANDALONE_HOLDER
 #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

DeviceStats queryDeviceStats (double fallbackSampleRate, int fallbackBufferSamples)
{
    if (const auto& forced = deviceStatsOverride())
        return *forced;

    DeviceStats stats;
    stats.sampleRate = fallbackSampleRate;
    stats.bufferSamples = fallbackBufferSamples;

   #if ! HUSHRIG_NO_STANDALONE_HOLDER
    if (auto* holder = juce::StandalonePluginHolder::getInstance())
    {
        auto& manager = holder->deviceManager;

        if (auto* device = manager.getCurrentAudioDevice())
        {
            stats.fromDevice = true;
            stats.deviceName = device->getName();
            stats.apiName = manager.getCurrentAudioDeviceType();
            stats.sampleRate = device->getCurrentSampleRate();
            stats.bufferSamples = device->getCurrentBufferSizeSamples();
            stats.inputLatencySamples = device->getInputLatencyInSamples();
            stats.outputLatencySamples = device->getOutputLatencyInSamples();
            stats.cpuUsage = manager.getCpuUsage();
        }
    }
   #endif

    return stats;
}
