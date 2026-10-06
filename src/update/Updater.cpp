#include "update/Updater.h"

#include "update/Sha256.h"
#include "update/VersionCompare.h"

namespace
{
constexpr const char* kOwner = "DevEduNunes";
constexpr const char* kRepo = "hushrig";
constexpr const char* kAssetPattern = "HushRig-Setup-*.exe";

juce::String pt (const char* utf8) { return juce::String::fromUTF8 (utf8); }

juce::URL::InputStreamOptions makeOptions (int* statusCode, bool forApi)
{
    auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                       .withConnectionTimeoutMs (15000)
                       .withNumRedirectsToFollow (5)
                       .withStatusCode (statusCode);

    return options.withExtraHeaders (forApi ? "Accept: application/vnd.github+json\r\nUser-Agent: HushRig-Updater\r\n"
                                            : "User-Agent: HushRig-Updater\r\n");
}
} // namespace

Updater::Updater() : juce::Thread ("HushRig Updater") {}

Updater::~Updater()
{
    stopThread (10000);
}

juce::String Updater::currentVersion()
{
    return JucePlugin_VersionString;
}

void Updater::checkForUpdates()
{
    if (isThreadRunning())
        return;
    job = Job::check;
    startThread();
}

void Updater::downloadAndInstall (const ReleaseInfo& release)
{
    if (isThreadRunning())
        return;
    target = release;
    job = Job::install;
    startThread();
}

void Updater::run()
{
    if (job == Job::check)
        runCheck();
    else
        runInstall();
}

void Updater::runCheck()
{
    int status = 0;
    const juce::URL url (juce::String ("https://api.github.com/repos/") + kOwner + "/" + kRepo + "/releases/latest");

    auto stream = url.createInputStream (makeOptions (&status, true));

    if (stream == nullptr)
        return postCheckDone (CheckResult::failed, {}, pt ("Sem conexão com o GitHub."));

    if (status == 404)
        return postCheckDone (CheckResult::failed, {},
                              pt ("Nenhuma versão publicada ainda (ou o repositório ainda é privado)."));

    if (status != 200)
        return postCheckDone (CheckResult::failed, {}, pt ("O GitHub respondeu com erro ") + juce::String (status) + ".");

    const auto json = juce::JSON::parse (stream->readEntireStreamAsString());

    if (! json.isObject())
        return postCheckDone (CheckResult::failed, {}, pt ("Resposta inválida do GitHub."));

    const auto tag = json["tag_name"].toString().trim();

    if (! hushrig::isNewerVersion (tag.toStdString(), currentVersion().toStdString()))
        return postCheckDone (CheckResult::upToDate, {}, pt ("Você já está na versão mais recente (") + currentVersion() + ").");

    ReleaseInfo info;
    info.version = tag.startsWithIgnoreCase ("v") ? tag.substring (1) : tag;

    if (auto* assets = json["assets"].getArray())
    {
        for (const auto& asset : *assets)
        {
            const auto name = asset["name"].toString();

            if (! name.matchesWildcard (kAssetPattern, true))
                continue;

            info.assetName = name;
            info.downloadUrl = asset["browser_download_url"].toString();

            const auto digest = asset["digest"].toString();
            if (digest.startsWithIgnoreCase ("sha256:"))
                info.sha256 = digest.fromFirstOccurrenceOf (":", false, false).toLowerCase();
            break;
        }
    }

    if (info.downloadUrl.isEmpty())
        return postCheckDone (CheckResult::failed, {}, pt ("A versão ") + info.version + pt (" não tem instalador anexado."));

    postCheckDone (CheckResult::updateAvailable, info, pt ("Nova versão disponível: ") + info.version);
}

void Updater::runInstall()
{
    if (! target.downloadUrl.startsWith ("https://github.com/"))
        return postInstallerLaunched (false, pt ("Endereço de download não confiável."));

    const auto destFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                              .getChildFile (juce::File::createLegalFileName (target.assetName));
    destFile.deleteFile();

    int status = 0;
    auto stream = juce::URL (target.downloadUrl).createInputStream (makeOptions (&status, false));

    if (stream == nullptr || status >= 400)
        return postInstallerLaunched (false, pt ("Não foi possível baixar o instalador."));

    const auto total = stream->getTotalLength();

    {
        juce::FileOutputStream out (destFile);

        if (out.failedToOpen())
            return postInstallerLaunched (false, pt ("Não foi possível gravar o instalador em disco."));

        constexpr int chunk = 64 * 1024;
        juce::HeapBlock<char> buffer (chunk);
        juce::int64 done = 0;

        while (! stream->isExhausted())
        {
            if (threadShouldExit())
            {
                out.flush();
                destFile.deleteFile();
                return;
            }

            const int n = stream->read (buffer, chunk);

            if (n < 0)
            {
                out.flush();
                destFile.deleteFile();
                return postInstallerLaunched (false, pt ("A conexão caiu durante o download."));
            }

            if (n == 0)
                break;

            out.write (buffer, static_cast<size_t> (n));
            done += n;
            postProgress (total > 0 ? static_cast<double> (done) / static_cast<double> (total) : -1.0);
        }

        out.flush();
    }

    if (total > 0 && destFile.getSize() != total)
    {
        destFile.deleteFile();
        return postInstallerLaunched (false, pt ("Download incompleto. Tente novamente."));
    }

    if (target.sha256.isNotEmpty())
    {
        hushrig::Sha256 hasher;
        juce::FileInputStream in (destFile);

        if (in.failedToOpen())
            return postInstallerLaunched (false, pt ("Não foi possível ler o instalador baixado."));

        juce::HeapBlock<char> chunk (64 * 1024);

        while (! in.isExhausted())
        {
            const int n = in.read (chunk, 64 * 1024);
            if (n <= 0)
                break;
            hasher.update (chunk, static_cast<size_t> (n));
        }

        if (juce::String (hasher.finishHex()) != target.sha256)
        {
            destFile.deleteFile();
            return postInstallerLaunched (false, pt ("Falha na verificação de integridade do instalador."));
        }
    }

    // /SILENT mostra só o progresso; /CLOSEAPPLICATIONS fecha este app; o instalador reabre depois.
    const bool started = destFile.startAsProcess ("/SILENT /CLOSEAPPLICATIONS /RESTARTAPPLICATIONS /COMPONENTS=\"app,vst3\"");

    postInstallerLaunched (started, started ? pt ("Instalador iniciado. O HushRig vai reiniciar.")
                                            : pt ("Não foi possível iniciar o instalador."));
}

void Updater::postCheckDone (CheckResult result, ReleaseInfo info, juce::String message)
{
    juce::MessageManager::callAsync ([weak = juce::WeakReference<Updater> (this), result, info, message]
    {
        if (auto* self = weak.get())
            if (self->onCheckDone)
                self->onCheckDone (result, info, message);
    });
}

void Updater::postProgress (double progress)
{
    juce::MessageManager::callAsync ([weak = juce::WeakReference<Updater> (this), progress]
    {
        if (auto* self = weak.get())
            if (self->onProgress)
                self->onProgress (progress);
    });
}

void Updater::postInstallerLaunched (bool ok, juce::String message)
{
    juce::MessageManager::callAsync ([weak = juce::WeakReference<Updater> (this), ok, message]
    {
        if (auto* self = weak.get())
            if (self->onInstallerLaunched)
                self->onInstallerLaunched (ok, message);
    });
}
