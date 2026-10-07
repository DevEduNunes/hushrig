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

    for (size_t i = 0; i < chainSlots.size(); ++i)
        chainSlots[i].store (static_cast<int> (i));

    auto p = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pedals = { p ("odOn"), p ("odDrive"), p ("odTone"), p ("odLevel"),
               p ("eqOn"), p ("eqLow"), p ("eqMid"), p ("eqHigh"),
               p ("chOn"), p ("chRate"), p ("chDepth"), p ("chMix"),
               p ("dlOn"), p ("dlTime"), p ("dlFeedback"), p ("dlMix"), p ("dlTone"),
               p ("rvOn"), p ("rvRoom"), p ("rvDamp"), p ("rvMix") };
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

    auto addBool = [&layout] (const char* id, const char* name)
    {
        layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, false));
    };
    auto addFloat = [&layout] (const char* id, const char* name, Range range, float def, const char* unit = "")
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range, def, Attrs().withLabel (unit)));
    };

    addBool ("odOn", "Overdrive On");
    addFloat ("odDrive", "Overdrive Drive", Range (0.0f, 40.0f, 0.1f), 12.0f, "dB");
    addFloat ("odTone", "Overdrive Tone", Range (500.0f, 12000.0f, 1.0f, 0.4f), 4000.0f, "Hz");
    addFloat ("odLevel", "Overdrive Level", Range (-24.0f, 6.0f, 0.1f), -6.0f, "dB");

    addBool ("eqOn", "EQ On");
    addFloat ("eqLow", "EQ Low", Range (-12.0f, 12.0f, 0.1f), 0.0f, "dB");
    addFloat ("eqMid", "EQ Mid", Range (-12.0f, 12.0f, 0.1f), 0.0f, "dB");
    addFloat ("eqHigh", "EQ High", Range (-12.0f, 12.0f, 0.1f), 0.0f, "dB");

    addBool ("chOn", "Chorus On");
    addFloat ("chRate", "Chorus Rate", Range (0.1f, 5.0f, 0.01f, 0.5f), 0.8f, "Hz");
    addFloat ("chDepth", "Chorus Depth", Range (0.0f, 1.0f, 0.01f), 0.5f);
    addFloat ("chMix", "Chorus Mix", Range (0.0f, 1.0f, 0.01f), 0.5f);

    addBool ("dlOn", "Delay On");
    addFloat ("dlTime", "Delay Time", Range (1.0f, hushrig::Delay::kMaxTimeMs, 1.0f, 0.5f), 350.0f, "ms");
    addFloat ("dlFeedback", "Delay Feedback", Range (0.0f, 0.95f, 0.01f), 0.35f);
    addFloat ("dlMix", "Delay Mix", Range (0.0f, 1.0f, 0.01f), 0.3f);
    addFloat ("dlTone", "Delay Tone", Range (500.0f, 12000.0f, 1.0f, 0.4f), 5000.0f, "Hz");

    addBool ("rvOn", "Reverb On");
    addFloat ("rvRoom", "Reverb Room", Range (0.0f, 1.0f, 0.01f), 0.5f);
    addFloat ("rvDamp", "Reverb Damping", Range (0.0f, 1.0f, 0.01f), 0.5f);
    addFloat ("rvMix", "Reverb Mix", Range (0.0f, 1.0f, 0.01f), 0.25f);

    return layout;
}

void HushRigProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    recorder.stop(); // a taxa de amostragem pode ter mudado
    sampleRateHz.store (sampleRate);
    blockSize.store (samplesPerBlock);
    gate.prepare (sampleRate);
    overdrive.prepare (sampleRate, 2);
    eq.prepare (sampleRate, 2);
    chorus.prepare (sampleRate, 2);
    delay.prepare (sampleRate, 2);
    reverb.prepare (sampleRate, 2);
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

    auto* const* data = buffer.getArrayOfWritePointers();

    for (auto& slot : chainSlots)
        runPedal (static_cast<hushrig::Pedal> (slot.load (std::memory_order_relaxed)), data, numChannels, numSamples);

    const float outGain = juce::Decibels::decibelsToGain (outputGainDb->load());
    buffer.applyGainRamp (0, numSamples, lastOutputGain, outGain);
    lastOutputGain = outGain;

    raisePeak (outputPeak, maxMagnitude (buffer));
    recorder.write (buffer);
    gateOpen.store (gateActive ? gate.isOpen() : true, std::memory_order_relaxed);
}

void HushRigProcessor::runPedal (hushrig::Pedal pedal, float* const* data, int numChannels, int numSamples)
{
    using hushrig::Pedal;

    switch (pedal)
    {
        case Pedal::overdrive:
            if (pedals.odOn->load() >= 0.5f)
            {
                overdrive.setParams ({ pedals.odDrive->load(), pedals.odTone->load(), pedals.odLevel->load() });
                overdrive.process (data, numChannels, numSamples);
            }
            break;
        case Pedal::eq:
            if (pedals.eqOn->load() >= 0.5f)
            {
                eq.setParams ({ pedals.eqLow->load(), pedals.eqMid->load(), pedals.eqHigh->load() });
                eq.process (data, numChannels, numSamples);
            }
            break;
        case Pedal::chorus:
            if (pedals.chOn->load() >= 0.5f)
            {
                chorus.setParams ({ pedals.chRate->load(), pedals.chDepth->load(), pedals.chMix->load() });
                chorus.process (data, numChannels, numSamples);
            }
            break;
        case Pedal::delay:
            if (pedals.dlOn->load() >= 0.5f)
            {
                delay.setParams ({ pedals.dlTime->load(), pedals.dlFeedback->load(), pedals.dlMix->load(), pedals.dlTone->load() });
                delay.process (data, numChannels, numSamples);
            }
            break;
        case Pedal::reverb:
            if (pedals.rvOn->load() >= 0.5f)
            {
                reverb.setParams ({ pedals.rvRoom->load(), pedals.rvDamp->load(), pedals.rvMix->load() });
                reverb.process (data, numChannels, numSamples);
            }
            break;
    }
}

hushrig::ChainOrder HushRigProcessor::getChainOrder() const
{
    hushrig::ChainOrder order;
    for (size_t i = 0; i < chainSlots.size(); ++i)
        order.slots[i] = chainSlots[i].load();
    return order;
}

void HushRigProcessor::setChainOrder (const hushrig::ChainOrder& order)
{
    if (! hushrig::ChainOrder::isValid (order.slots))
        return;

    // Valores intermediarios podem repetir um pedal por um bloco; aceitavel (so troca de ordem).
    for (size_t i = 0; i < chainSlots.size(); ++i)
        chainSlots[i].store (order.slots[i]);
    apvts.state.setProperty ("chainOrder", juce::String (order.toString()), nullptr);
}

void HushRigProcessor::loadChainOrderFromState()
{
    const auto text = apvts.state.getProperty ("chainOrder").toString().toStdString();
    setChainOrder (hushrig::ChainOrder::fromString (text));
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
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            loadChainOrderFromState();
        }
}

// Ponto de entrada exigido pelo JUCE para criar o plugin / app standalone.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HushRigProcessor();
}
