#include "AudioEngine.h"

namespace openwav
{

AudioEngine::AudioEngine()
{
    formatManager.registerBasicFormats();
    transportSource.addChangeListener(this);
    backgroundThread.startThread(juce::Thread::Priority::normal);
}

AudioEngine::~AudioEngine()
{
    transportSource.removeChangeListener(this);
    transportSource.setSource(nullptr);
    backgroundThread.stopThread(1000);
}

void AudioEngine::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    transportSource.prepareToPlay(samplesPerBlock, sampleRate);
}

void AudioEngine::releaseResources()
{
    transportSource.releaseResources();
}

void AudioEngine::processNextAudioBlock(juce::AudioBuffer<float>& outputBuffer)
{
    if (!transportSource.isPlaying())
        return;

    juce::AudioSourceChannelInfo channelInfo(outputBuffer);
    transportSource.getNextAudioBlock(channelInfo);

    // Apply volume / gain
    outputBuffer.applyGain(gainLevel);
}

bool AudioEngine::loadFile(const juce::File& audioFile, bool autoPlay)
{
    if (!audioFile.existsAsFile())
        return false;

    currentFile = audioFile;

    auto* reader = formatManager.createReaderFor(audioFile);
    if (reader == nullptr)
        return false;

    transportSource.stop();
    transportSource.setSource(nullptr);

    readerSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true);
    transportSource.setSource(readerSource.get(), 32768, &backgroundThread, reader->sampleRate);
    readerSource->setLooping(isLoopingEnabled);

    // Update thumbnail source
    thumbnail.setSource(new juce::FileInputSource(audioFile));

    listeners.call([filePath = audioFile.getFullPathName()](AudioEngineListener& l) {
        l.sampleLoaded(filePath);
    });

    if (autoPlay || autoPlayOnSelect)
    {
        play();
    }

    return true;
}

void AudioEngine::play()
{
    if (readerSource != nullptr)
    {
        transportSource.start();
        listeners.call([](AudioEngineListener& l) { l.playbackStateChanged(true); });
    }
}

void AudioEngine::pause()
{
    transportSource.stop();
    listeners.call([](AudioEngineListener& l) { l.playbackStateChanged(false); });
}

void AudioEngine::stop()
{
    transportSource.stop();
    transportSource.setPosition(0.0);
    listeners.call([](AudioEngineListener& l) { l.playbackStateChanged(false); });
}

void AudioEngine::setPositionRatio(double ratio)
{
    double clamped = juce::jlimit(0.0, 1.0, ratio);
    double targetSecs = clamped * getTotalLengthSeconds();
    transportSource.setPosition(targetSecs);
}

void AudioEngine::setLooping(bool shouldLoop)
{
    isLoopingEnabled = shouldLoop;
    if (readerSource != nullptr)
    {
        readerSource->setLooping(isLoopingEnabled);
    }
}

void AudioEngine::setGain(float newGain)
{
    gainLevel = juce::jlimit(0.0f, 1.5f, newGain);
}

void AudioEngine::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &transportSource)
    {
        bool playing = transportSource.isPlaying();
        listeners.call([playing](AudioEngineListener& l) { l.playbackStateChanged(playing); });
    }
}

void AudioEngine::addListener(AudioEngineListener* listener)
{
    listeners.add(listener);
}

void AudioEngine::removeListener(AudioEngineListener* listener)
{
    listeners.remove(listener);
}

} // namespace openwav
