#include "PresetManager.h"

#include "PluginProcessor.h"

namespace
{
constexpr const char* kExtension = ".hushpreset";

struct Value { const char* id; float value; };
struct Factory { const char* name; std::vector<Value> values; };

// Parametros nao citados voltam ao padrao antes de aplicar o preset.
const std::vector<Factory>& factoryPresets()
{
    static const std::vector<Factory> presets {
        { "Limpo", {} },
        { "Crunch", { { "odOn", 1 }, { "odDrive", 14 }, { "odTone", 4500 }, { "odLevel", -6 } } },
        { "Ambiente", { { "dlOn", 1 }, { "dlTime", 420 }, { "dlFeedback", 0.3f }, { "dlMix", 0.25f },
                        { "rvOn", 1 }, { "rvRoom", 0.6f }, { "rvMix", 0.2f } } },
        { "Solo", { { "odOn", 1 }, { "odDrive", 24 }, { "odTone", 5000 }, { "odLevel", -8 },
                    { "eqOn", 1 }, { "eqMid", 3 }, { "eqHigh", 1.5f },
                    { "dlOn", 1 }, { "dlTime", 380 }, { "dlFeedback", 0.35f }, { "dlMix", 0.22f },
                    { "rvOn", 1 }, { "rvRoom", 0.45f }, { "rvMix", 0.15f } } },
        { "Chorus Cristalino", { { "chOn", 1 }, { "chRate", 0.9f }, { "chDepth", 0.6f }, { "chMix", 0.5f },
                                 { "rvOn", 1 }, { "rvRoom", 0.5f }, { "rvMix", 0.2f } } },
    };
    return presets;
}

// Ganho/gate sao do "setup" do usuario, nao do preset: ficam como estao.
bool isPedalParameter (const juce::String& id)
{
    return id.startsWith ("od") || id.startsWith ("amp") || id.startsWith ("eq") || id.startsWith ("ch") || id.startsWith ("dl") || id.startsWith ("rv");
}
} // namespace

juce::File PresetManager::getPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("HushRig").getChildFile ("Presets");
}

juce::String PresetManager::sanitiseName (const juce::String& name)
{
    return juce::File::createLegalFileName (name.trim()).trim();
}

juce::File PresetManager::fileFor (const juce::String& name) const
{
    return getPresetFolder().getChildFile (sanitiseName (name) + kExtension);
}

juce::StringArray PresetManager::getFactoryPresetNames()
{
    juce::StringArray names;
    for (const auto& p : factoryPresets())
        names.add (p.name);
    return names;
}

bool PresetManager::loadFactoryPreset (int index)
{
    const auto& presets = factoryPresets();
    if (index < 0 || index >= static_cast<int> (presets.size()))
        return false;

    for (auto* param : processor.getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (param))
            if (isPedalParameter (withId->paramID))
            {
                withId->beginChangeGesture();
                withId->setValueNotifyingHost (withId->getDefaultValue());
                withId->endChangeGesture();
            }

    for (const auto& v : presets[static_cast<size_t> (index)].values)
        if (auto* param = processor.apvts.getParameter (v.id))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 (v.value));
            param->endChangeGesture();
        }

    processor.setChainOrder ({});
    return true;
}

juce::StringArray PresetManager::getUserPresetNames() const
{
    juce::StringArray names;
    for (const auto& f : getPresetFolder().findChildFiles (juce::File::findFiles, false, "*" + juce::String (kExtension)))
        names.add (f.getFileNameWithoutExtension());
    names.sort (true);
    return names;
}

bool PresetManager::saveUserPreset (const juce::String& name)
{
    if (sanitiseName (name).isEmpty())
        return false;

    const auto file = fileFor (name);
    if (! file.getParentDirectory().createDirectory())
        return false;

    // Salva so os pedais e a ordem; ganhos e gate pertencem ao setup, nao ao preset.
    auto state = processor.apvts.copyState();
    for (int i = state.getNumChildren(); --i >= 0;)
        if (! isPedalParameter (state.getChild (i).getProperty ("id").toString()))
            state.removeChild (i, nullptr);

    state.setProperty ("ampModel", processor.getAmpModelPath(), nullptr);

    if (auto xml = state.createXml())
        return xml->writeTo (file);
    return false;
}

bool PresetManager::loadUserPreset (const juce::String& name)
{
    const auto file = fileFor (name);
    if (! file.existsAsFile())
        return false;

    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName (processor.apvts.state.getType()))
        return false;

    const auto saved = juce::ValueTree::fromXml (*xml);

    for (auto* param : processor.getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (param))
            if (isPedalParameter (withId->paramID))
            {
                const auto child = saved.getChildWithProperty ("id", withId->paramID);
                const float value = child.isValid() ? static_cast<float> (child.getProperty ("value")) : 0.0f;
                if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (withId))
                {
                    ranged->beginChangeGesture();
                    ranged->setValueNotifyingHost (child.isValid() ? ranged->convertTo0to1 (value) : ranged->getDefaultValue());
                    ranged->endChangeGesture();
                }
            }

    processor.setChainOrder (hushrig::ChainOrder::fromString (saved.getProperty ("chainOrder").toString().toStdString()));

    // O modelo do amp vai junto (caminho absoluto); se o arquivo sumiu, mantem o atual.
    const juce::File model (saved.getProperty ("ampModel").toString());
    juce::String ignored;
    if (model.existsAsFile() && model.getFullPathName() != processor.getAmpModelPath())
        processor.loadAmpModel (model, ignored);

    return true;
}

bool PresetManager::deleteUserPreset (const juce::String& name)
{
    return fileFor (name).deleteFile();
}
