#include "AudioEngine.h"
#include <algorithm>

namespace openwav
{

AudioEngine::AudioEngine()
{
    formatManager.registerBasicFormats();
    backgroundThread.startThread(juce::Thread::Priority::high);
}

AudioEngine::~AudioEngine()
{
    backgroundThread.stopThread(1000);
}

void AudioEngine::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    if (sampleRate > 0.0)
        engineSampleRate = sampleRate;
}

void AudioEngine::releaseResources()
{
}

void AudioEngine::processNextAudioBlock(juce::AudioBuffer<float>& outputBuffer)
{
    outputBuffer.clear();

    const juce::ScopedLock sl(voiceLock);
    if (activeVoices.empty())
        return;

    int numSamples = outputBuffer.getNumSamples();
    int outChannels = outputBuffer.getNumChannels();

    for (auto it = activeVoices.begin(); it != activeVoices.end(); )
    {
        auto& voice = *it;
        if (voice->finished)
        {
            it = activeVoices.erase(it);
            continue;
        }

        int voiceChannels = voice->buffer.getNumChannels();
        int voiceLength = voice->buffer.getNumSamples();
        double pos = voice->readPosition;
        double ratio = (voice->ratio > 0.0) ? voice->ratio : 1.0;

        for (int i = 0; i < numSamples; ++i)
        {
            int idx = static_cast<int>(pos);
            if (idx >= voiceLength)
            {
                if (voice->isLooping && voiceLength > 0)
                {
                    pos = 0.0;
                    idx = 0;
                }
                else
                {
                    voice->finished = true;
                    break;
                }
            }

            int nextIdx = std::min(idx + 1, voiceLength - 1);
            float frac = static_cast<float>(pos - idx);

            for (int ch = 0; ch < outChannels; ++ch)
            {
                int srcCh = std::min(ch, voiceChannels - 1);
                float s1 = voice->buffer.getSample(srcCh, idx);
                float s2 = voice->buffer.getSample(srcCh, nextIdx);
                float sample = (s1 + frac * (s2 - s1)) * gainLevel;

                outputBuffer.addSample(ch, i, sample);
            }

            pos += ratio;
        }

        voice->readPosition = pos;

        if (voice->finished)
        {
            it = activeVoices.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

bool AudioEngine::loadFile(const juce::File& audioFile, bool autoPlay)
{
    if (!audioFile.existsAsFile())
        return false;

    currentFile = audioFile;
    thumbnail.setSource(new juce::FileInputSource(audioFile));
    auto loadId = ++currentLoadId;

    std::thread([this, audioFile, autoPlay, loadId]() {
        std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(audioFile));
        if (reader == nullptr)
            return;

        double fileSampleRate = reader->sampleRate;
        int64_t numSamples64 = reader->lengthInSamples;
        int numChannels = static_cast<int>(reader->numChannels);

        if (numSamples64 <= 0)
            return;

        int numSamples = static_cast<int>(numSamples64);
        auto voice = std::make_shared<AudioVoice>();
        voice->buffer.setSize(numChannels, numSamples);
        reader->read(&voice->buffer, 0, numSamples, 0, true, true);
        voice->ratio = (engineSampleRate > 0.0) ? (fileSampleRate / engineSampleRate) : 1.0;
        voice->isLooping = isLoopingEnabled;

        juce::MessageManager::callAsync([this, audioFile, autoPlay, loadId, voice]() {
            {
                const juce::ScopedLock sl(voiceLock);
                activeVoices.clear(); // Kill previous voices so only the last selected voice plays
                activeVoices.push_back(voice);
            }

            listeners.call([filePath = audioFile.getFullPathName()](AudioEngineListener& l) {
                l.sampleLoaded(filePath);
                l.playbackStateChanged(true);
            });
        });
    }).detach();

    return true;
}

void AudioEngine::play()
{
    // Re-trigger playback if currentFile exists and no active voices
    if (activeVoices.empty() && currentFile.existsAsFile())
    {
        loadFile(currentFile, true);
    }
}

void AudioEngine::pause()
{
    const juce::ScopedLock sl(voiceLock);
    activeVoices.clear();
    listeners.call([](AudioEngineListener& l) { l.playbackStateChanged(false); });
}

void AudioEngine::stop()
{
    const juce::ScopedLock sl(voiceLock);
    activeVoices.clear();
    listeners.call([](AudioEngineListener& l) { l.playbackStateChanged(false); });
}

void AudioEngine::setPositionRatio(double ratio)
{
    const juce::ScopedLock sl(voiceLock);
    double clamped = juce::jlimit(0.0, 1.0, ratio);
    for (auto& v : activeVoices)
    {
        v->readPosition = clamped * v->buffer.getNumSamples();
    }
}

void AudioEngine::setLooping(bool shouldLoop)
{
    isLoopingEnabled = shouldLoop;
    const juce::ScopedLock sl(voiceLock);
    for (auto& v : activeVoices)
    {
        v->isLooping = isLoopingEnabled;
    }
}

void AudioEngine::setGain(float newGain)
{
    gainLevel = juce::jlimit(0.0f, 1.5f, newGain);
}

bool AudioEngine::isPlaying() const
{
    const juce::ScopedLock sl(voiceLock);
    return !activeVoices.empty();
}

double AudioEngine::getCurrentPositionSeconds() const
{
    const juce::ScopedLock sl(voiceLock);
    if (!activeVoices.empty())
    {
        auto& v = activeVoices.back();
        return (v->readPosition / v->ratio) / engineSampleRate;
    }
    return 0.0;
}

double AudioEngine::getTotalLengthSeconds() const
{
    const juce::ScopedLock sl(voiceLock);
    if (!activeVoices.empty())
    {
        auto& v = activeVoices.back();
        return (v->buffer.getNumSamples() / v->ratio) / engineSampleRate;
    }
    return 0.0;
}

void AudioEngine::getMinMaxForTimeRange(double startTimeSecs, double endTimeSecs, float& minVal, float& maxVal) const
{
    minVal = 0.0f;
    maxVal = 0.0f;

    const juce::ScopedLock sl(voiceLock);
    if (activeVoices.empty())
        return;

    const auto& buffer = activeVoices.back()->buffer;
    int numSamples = buffer.getNumSamples();
    if (numSamples <= 0)
        return;

    double sr = (engineSampleRate > 0.0) ? engineSampleRate : 44100.0;
    double fileLengthSecs = (numSamples / activeVoices.back()->ratio) / sr;
    if (fileLengthSecs <= 0.0)
        return;

    int startSample = juce::jlimit(0, numSamples - 1, static_cast<int>((startTimeSecs / fileLengthSecs) * numSamples));
    int endSample = juce::jlimit(startSample + 1, numSamples, static_cast<int>((endTimeSecs / fileLengthSecs) * numSamples));
    int numToRead = endSample - startSample;

    if (numToRead <= 0)
        return;

    int numChannels = buffer.getNumChannels();
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto range = buffer.findMinMax(ch, startSample, numToRead);
        if (range.getStart() < minVal) minVal = range.getStart();
        if (range.getEnd() > maxVal) maxVal = range.getEnd();
    }
}

void AudioEngine::changeListenerCallback(juce::ChangeBroadcaster* /*source*/)
{
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
