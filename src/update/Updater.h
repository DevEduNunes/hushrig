#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include <functional>

/**
 * Procura e instala atualizações a partir das releases do GitHub.
 * Todo o trabalho de rede roda em uma thread própria; os callbacks são
 * sempre chamados na message thread.
 */
class Updater final : private juce::Thread
{
public:
    struct ReleaseInfo
    {
        juce::String version;     // ex.: "0.2.0"
        juce::String assetName;   // ex.: "HushRig-Setup-0.2.0.exe"
        juce::String downloadUrl;
        juce::String sha256;      // vazio se a release não informar
    };

    enum class CheckResult { upToDate, updateAvailable, failed };

    Updater();
    ~Updater() override;

    std::function<void (CheckResult, const ReleaseInfo&, const juce::String& message)> onCheckDone;
    std::function<void (double progress)> onProgress; // negativo = indeterminado
    std::function<void (bool ok, const juce::String& message)> onInstallerLaunched;

    bool isBusy() const { return isThreadRunning(); }

    void checkForUpdates();
    void downloadAndInstall (const ReleaseInfo& release);

    static juce::String currentVersion();

private:
    enum class Job { check, install };

    void run() override;
    void runCheck();
    void runInstall();

    void postCheckDone (CheckResult result, ReleaseInfo info, juce::String message);
    void postProgress (double progress);
    void postInstallerLaunched (bool ok, juce::String message);

    Job job = Job::check;
    ReleaseInfo target;

    JUCE_DECLARE_WEAK_REFERENCEABLE (Updater)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Updater)
};
