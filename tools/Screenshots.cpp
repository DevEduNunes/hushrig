// Gera os screenshots do README renderizando o editor sem abrir janela.
// Uso: HushRigShots <pasta-de-saida>

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdio>

#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
void savePng (const juce::Image& image, const juce::File& file)
{
    file.deleteFile();
    juce::FileOutputStream out (file);
    juce::PNGImageFormat png;

    if (out.failedToOpen() || ! png.writeImageToStream (image, out))
        std::fprintf (stderr, "Falha ao gravar %s\n", file.getFullPathName().toRawUTF8());
}

void setParam (HushRigProcessor& processor, const char* id, float value)
{
    if (auto* parameter = processor.apvts.getParameter (id))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}
} // namespace

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;

    const auto outDir = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "shots");
    outDir.createDirectory();

    // Faz o editor se comportar como no app standalone (mostra a seção de atualização).
    juce::AudioProcessor::setTypeOfNextNewPlugin (juce::AudioProcessor::wrapperType_Standalone);
    HushRigProcessor processor;
    juce::AudioProcessor::setTypeOfNextNewPlugin (juce::AudioProcessor::wrapperType_Undefined);

    setParam (processor, "inputGain", 6.0f);
    setParam (processor, "gateThreshold", -58.0f);
    setParam (processor, "gateHold", 60.0f);
    setParam (processor, "gateRelease", 120.0f);
    setParam (processor, "outputGain", -2.0f);

    HushRigEditor editor (processor);

    savePng (editor.createComponentSnapshot (editor.getLocalBounds(), true, 2.0f), outDir.getChildFile ("app-main.png"));

    Updater::ReleaseInfo info;
    info.version = "0.2.0";
    info.assetName = "HushRig-Setup-0.2.0.exe";

    if (processor.updater.onCheckDone)
        processor.updater.onCheckDone (Updater::CheckResult::updateAvailable, info,
                                       juce::String::fromUTF8 ("Nova versão disponível: 0.2.0"));

    savePng (editor.createComponentSnapshot (editor.getLocalBounds(), true, 2.0f), outDir.getChildFile ("app-update.png"));

    return 0;
}
