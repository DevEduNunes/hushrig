#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class HushRigProcessor;

/** Presets: de fabrica (em codigo) e do usuario (XML em Documentos/HushRig/Presets). */
class PresetManager
{
public:
    explicit PresetManager (HushRigProcessor& owner) : processor (owner) {}

    static juce::File getPresetFolder();

    static juce::StringArray getFactoryPresetNames();
    bool loadFactoryPreset (int index);

    juce::StringArray getUserPresetNames() const;
    bool saveUserPreset (const juce::String& name); // sobrescreve se ja existir
    bool loadUserPreset (const juce::String& name);
    bool deleteUserPreset (const juce::String& name);

    /** Nome seguro para arquivo (sem caracteres invalidos); vazio se nada sobrar. */
    static juce::String sanitiseName (const juce::String& name);

private:
    juce::File fileFor (const juce::String& name) const;

    HushRigProcessor& processor;
};
