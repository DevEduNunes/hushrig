#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Recorder.h"
#include "dsp/Chorus.h"
#include "dsp/Delay.h"
#include "dsp/Eq3.h"
#include "dsp/NoiseGate.h"
#include "dsp/Overdrive.h"
#include "dsp/Reverb.h"
#include "update/Updater.h"

class HushRigProcessor final : public juce::AudioProcessor
{
public:
    HushRigProcessor();
    ~HushRigProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "HushRig"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; } // cauda do delay/reverb

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    hushrig::Recorder recorder; // gravação em WAV (acionada pela interface)
    Updater updater; // vive com o processador; o editor só liga/desliga os callbacks

    // Telemetria para a interface (escrita na thread de áudio, lida pela UI).
    // Os picos guardam o maior valor desde a última leitura (a UI faz exchange(0)).
    std::atomic<float> inputPeak { 0.0f };
    std::atomic<float> outputPeak { 0.0f };
    std::atomic<bool> gateOpen { false };
    std::atomic<double> sampleRateHz { 0.0 };
    std::atomic<int> blockSize { 0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    hushrig::NoiseGate gate;
    hushrig::Overdrive overdrive;
    hushrig::Eq3 eq;
    hushrig::Chorus chorus;
    hushrig::Delay delay;
    hushrig::Reverb reverb;

    // Parametros dos pedais (ids em createLayout). Ordem fixa por enquanto: OD > EQ > Chorus > Delay > Reverb.
    struct PedalParams
    {
        std::atomic<float> *odOn, *odDrive, *odTone, *odLevel;
        std::atomic<float> *eqOn, *eqLow, *eqMid, *eqHigh;
        std::atomic<float> *chOn, *chRate, *chDepth, *chMix;
        std::atomic<float> *dlOn, *dlTime, *dlFeedback, *dlMix, *dlTone;
        std::atomic<float> *rvOn, *rvRoom, *rvDamp, *rvMix;
    } pedals {};

    std::atomic<float>* inputGainDb = nullptr;
    std::atomic<float>* gateThresholdDb = nullptr;
    std::atomic<float>* gateHoldMs = nullptr;
    std::atomic<float>* gateReleaseMs = nullptr;
    std::atomic<float>* gateBypass = nullptr;
    std::atomic<float>* outputGainDb = nullptr;

    float lastInputGain = 1.0f;
    float lastOutputGain = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HushRigProcessor)
};
