#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PresetManager.h"
#include "Recorder.h"
#include "dsp/ChainOrder.h"
#include "dsp/Chorus.h"
#include "dsp/Delay.h"
#include "dsp/Eq3.h"
#include "dsp/NoiseGate.h"
#include "dsp/Overdrive.h"
#include "amp/NamAmp.h"
#include "dsp/Reverb.h"
#include "update/Updater.h"

class HushRigProcessor final : public juce::AudioProcessor
{
public:
    HushRigProcessor();
    ~HushRigProcessor() override;

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

    // Ordem dos pedais (thread da UI escreve; a de audio le). Persistida no estado do plugin.
    hushrig::ChainOrder getChainOrder() const;
    void setChainOrder (const hushrig::ChainOrder& order);

    // Amp sim (NAM). `loadAmpModel` roda na thread da UI: carrega, prepara e publica o modelo
    // para a thread de audio sem travar (troca atomica). Devolve false e preenche `error` se falhar.
    bool loadAmpModel (const juce::File& file, juce::String& error);
    juce::String getAmpModelPath() const { return apvts.state.getProperty ("ampModel").toString(); }

    std::atomic<bool> ampLoaded { false };
    std::atomic<bool> ampRateMismatch { false };
    std::atomic<float> ampNormDb { 0.0f };       // ganho de normalizacao do modelo atual (dB)
    std::atomic<bool> ampHasLoudness { false };
    std::atomic<float> ampCpuLoad { 0.0f }; // tempo de processamento / tempo do buffer (0..1+), suavizado

    juce::AudioProcessorValueTreeState apvts;
    hushrig::Recorder recorder; // gravação em WAV (acionada pela interface)
    PresetManager presets { *this }; // presets de fabrica e do usuario
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
    hushrig::Eq3 ampTone; // tom do amp (graves/medios/agudos), depois do modelo
    hushrig::Chorus chorus;
    hushrig::Delay delay;
    hushrig::Reverb reverb;

    // Parametros dos pedais (ids em createLayout).
    void runPedal (hushrig::Pedal pedal, float* const* data, int numChannels, int numSamples);
    void loadChainOrderFromState();
    void loadAmpFromState();
    void adoptPendingAmp();
    void collectRetiredAmp();
    void processAmp (float* const* data, int numChannels, int numSamples);

    // Troca sem lock: a UI publica em `pendingAmp`; a thread de audio adota no inicio do bloco
    // e devolve o antigo em `retiredAmp`, que a UI destroi (nunca se libera memoria no audio).
    std::unique_ptr<hushrig::NamAmp> activeAmp;
    std::atomic<hushrig::NamAmp*> pendingAmp { nullptr };
    std::atomic<hushrig::NamAmp*> retiredAmp { nullptr };
    float lastAmpIn = 1.0f, lastAmpOut = 1.0f;

    std::array<std::atomic<int>, hushrig::kNumPedals> chainSlots;

    struct PedalParams
    {
        std::atomic<float> *odOn, *odDrive, *odTone, *odLevel;
        std::atomic<float> *eqOn, *eqLow, *eqMid, *eqHigh;
        std::atomic<float> *chOn, *chRate, *chDepth, *chMix;
        std::atomic<float> *dlOn, *dlTime, *dlFeedback, *dlMix, *dlTone;
        std::atomic<float> *rvOn, *rvRoom, *rvDamp, *rvMix;
        std::atomic<float> *ampOn, *ampIn, *ampOut, *ampBass, *ampMid, *ampTreble, *ampNorm;
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
