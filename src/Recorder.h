#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include <atomic>
#include <memory>

namespace hushrig
{
/** Grava o áudio já processado em WAV (24 bits). O disco é escrito por uma thread própria;
    a thread de áudio só entrega os blocos (sem alocar nem bloquear). */
class Recorder
{
public:
    Recorder() : writerThread ("HushRig gravacao") {}
    ~Recorder() { stop(); }

    /** Pasta onde as gravações são salvas: Documentos\HushRig. */
    static juce::File recordingsFolder()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("HushRig");
    }

    /** Começa uma gravação nova. Retorna false (e preenche errorMessage) se não conseguiu abrir o arquivo. */
    bool start (double sampleRate, juce::String& errorMessage)
    {
        stop();

        if (sampleRate <= 0.0)
        {
            errorMessage = juce::String::fromUTF8 ("Aguardando o áudio iniciar.");
            return false;
        }

        auto folder = recordingsFolder();

        if (! folder.createDirectory().wasOk())
        {
            errorMessage = juce::String::fromUTF8 ("Não foi possível criar a pasta ") + folder.getFullPathName();
            return false;
        }

        auto file = folder.getNonexistentChildFile (
            juce::Time::getCurrentTime().formatted ("HushRig-%Y-%m-%d_%H-%M-%S"), ".wav");

        std::unique_ptr<juce::FileOutputStream> stream (file.createOutputStream());

        if (stream == nullptr || stream->failedToOpen())
        {
            errorMessage = juce::String::fromUTF8 ("Não foi possível criar o arquivo ") + file.getFullPathName();
            return false;
        }

        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer (
            wav.createWriterFor (stream.get(), sampleRate, 2, 24, juce::StringPairArray(), 0));

        if (writer == nullptr)
        {
            errorMessage = juce::String::fromUTF8 ("Não foi possível iniciar a gravação em WAV.");
            return false;
        }

        stream.release(); // o writer passa a ser o dono do stream

        if (! writerThread.isThreadRunning())
            writerThread.startThread();

        samplesWritten.store (0);
        currentFile = file;

        auto newWriter = std::make_unique<juce::AudioFormatWriter::ThreadedWriter> (writer.release(), writerThread, 65536);

        {
            const juce::SpinLock::ScopedLockType lock (writerLock);
            threaded = std::move (newWriter);
        }

        recording.store (true);
        return true;
    }

    /** Para a gravação e fecha o arquivo. */
    void stop()
    {
        recording.store (false);

        std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> old;

        {
            const juce::SpinLock::ScopedLockType lock (writerLock);
            old = std::move (threaded);
        }

        old.reset(); // esvazia o que faltava e fecha o arquivo (fora do lock)
    }

    bool isRecording() const { return recording.load(); }
    juce::File getFile() const { return currentFile; }
    juce::int64 getSamplesWritten() const { return samplesWritten.load(); }

    /** Thread de áudio: entrega o bloco já processado. Usa só os 2 primeiros canais. */
    void write (const juce::AudioBuffer<float>& buffer)
    {
        if (! recording.load (std::memory_order_relaxed) || buffer.getNumChannels() < 2)
            return;

        const juce::SpinLock::ScopedTryLockType lock (writerLock);

        if (lock.isLocked() && threaded != nullptr
            && threaded->write (buffer.getArrayOfReadPointers(), buffer.getNumSamples()))
            samplesWritten.fetch_add (buffer.getNumSamples());
    }

private:
    juce::TimeSliceThread writerThread;
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> threaded;
    juce::SpinLock writerLock;
    juce::File currentFile;
    std::atomic<bool> recording { false };
    std::atomic<juce::int64> samplesWritten { 0 };
};
} // namespace hushrig
