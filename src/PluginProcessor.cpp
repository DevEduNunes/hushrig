#include "PluginProcessor.h"

#include "PluginEditor.h"

namespace
{
using Range = juce::NormalisableRange<float>;
using Attrs = juce::AudioParameterFloatAttributes;

void raisePeak (std::atomic<float>& target, float value)
{
    auto current = target.load (std::memory_order_relaxed);

    while (value > current && ! target.compare_exchange_weak (current, value, std::memory_order_relaxed))
    {
    }
}

float maxMagnitude (const juce::AudioBuffer<float>& buffer)
{
    float peak = 0.0f;

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        peak = std::max (peak, buffer.getMagnitude (ch, 0, buffer.getNumSamples()));

    return peak;
}
} // namespace

HushRigProcessor::HushRigProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    inputGainDb     = apvts.getRawParameterValue ("inputGain");
    gateThresholdDb = apvts.getRawParameterValue ("gateThreshold");
    gateHoldMs      = apvts.getRawParameterValue ("gateHold");
    gateReleaseMs   = apvts.getRawParameterValue ("gateRelease");
    gateBypass      = apvts.getRawParameterValue ("gateBypass");
    outputGainDb    = apvts.getRawParameterValue ("outputGain");
}

juce::AudioProcessorValueTreeState::ParameterLayout HushRigProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "inputGain", 1 }, "Input Gain",
        Range (-24.0f, 24.0f, 0.1f), 0.0f, Attrs().withLabel ("dB")));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "gateBypass", 1 }, "Gate Bypass", false));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "gateThreshold", 1 }, "Gate Threshold",
        Range (-90.0f, -20.0f, 0.1f), -60.0f, Attrs().withLabel ("dB")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "gateHold", 1 }, "Gate Hold",
        Range (0.0f, 500.0f, 1.0f, 0.5f), 40.0f, Attrs().withLabel ("ms")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "gateRelease", 1 }, "Gate Release",
        Range (5.0f, 1000.0f, 1.0f, 0.5f), 80.0f, Attrs().withLabel ("ms")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "outputGain", 1 }, "Output Gain",
        Range (-24.0f, 12.0f, 0.1f), 0.0f, Attrs().withLabel ("dB")));

    return layout;
}

void HushRigProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sampleRateHz.store (sampleRate);
    blockSize.store (samplesPerBlock);
    gate.prepare (sampleRate);
    lastInputGain  = juce::Decibels::decibelsToGain (inputGainDb->load());
    lastOutputGain = juce::Decibels::decibelsToGain (outputGainDb->load());
}

bool HushRigProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::stereo())
        return false;

    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void HushRigProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // Guitarra costuma chegar em mono: espelha o canal 0 nos demais.
    if (getTotalNumInputChannels() == 1)
        for (int ch = 1; ch < numChannels; ++ch)
            buffer.copyFrom (ch, 0, buffer, 0, 0, numSamples);

    const float inGain = juce::Decibels::decibelsToGain (inputGainDb->load());
    buffer.applyGainRamp (0, numSamples, lastInputGain, inGain);
    lastInputGain = inGain;

    raisePeak (inputPeak, maxMagnitude (buffer));

    const bool gateActive = gateBypass->load() < 0.5f;

    if (gateActive)
    {
        hushrig::NoiseGate::Params p;
        p.thresholdDb = gateThresholdDb->load();
        p.holdMs      = gateHoldMs->load();
        p.releaseMs   = gateReleaseMs->load();
        gate.setParams (p);
        gate.process (buffer.getArrayOfWritePointers(), numChannels, numSamples);
    }

    const float outGain = juce::Decibels::decibelsToGain (outputGainDb->load());
    buffer.applyGainRamp (0, numSamples, lastOutputGain, outGain);
    lastOutputGain = outGain;

    raisePeak (outputPeak, maxMagnitude (buffer));
    gateOpen.store (gateActive ? gate.isOpen() : true, std::memory_order_relaxed);
}

juce::AudioProcessorEditor* HushRigProcessor::createEditor()
{
    return new HushRigEditor (*this);
}

void HushRigProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void HushRigProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

// Ponto de entrada exigido pelo JUCE para criar o plugin / app standalone.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HushRigProcessor();
}
