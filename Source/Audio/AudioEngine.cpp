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

        // Custom selection start and end samples
        int startSample = static_cast<int>(voice->startRatio * voiceLength);
        int endSample = static_cast<int>(voice->endRatio * voiceLength);
        if (endSample <= startSample) endSample = voiceLength;

        // Ensure position is within the selection range
        if (pos < startSample) pos = startSample;

        for (int i = 0; i < numSamples; ++i)
        {
            int idx = static_cast<int>(pos);
            if (idx >= endSample || idx >= voiceLength)
            {
                if (voice->isLooping && endSample > startSample)
                {
                    pos = startSample;
                    idx = startSample;
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

    // Reset range selection on new file load
    sampleStartRatio = 0.0;
    sampleEndRatio = 1.0;

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
        voice->startRatio = 0.0;
        voice->endRatio = 1.0;

        juce::MessageManager::callAsync([this, audioFile, autoPlay, loadId, voice]() {
            {
                const juce::ScopedLock sl(voiceLock);
                loadedVoice = voice;
                stoppedPositionSecs = 0.0;
                activeVoices.clear(); // Kill previous voices so only the last selected voice plays
                if (autoPlay)
                {
                    auto playVoice = std::make_shared<AudioVoice>();
                    playVoice->buffer.makeCopyOf(loadedVoice->buffer);
                    playVoice->ratio = loadedVoice->ratio;
                    playVoice->readPosition = sampleStartRatio * playVoice->buffer.getNumSamples();
                    playVoice->isLooping = isLoopingEnabled;
                    playVoice->finished = false;
                    playVoice->startRatio = sampleStartRatio;
                    playVoice->endRatio = sampleEndRatio;
                    activeVoices.push_back(playVoice);
                }
            }

            listeners.call([filePath = audioFile.getFullPathName(), autoPlay](AudioEngineListener& l) {
                l.sampleLoaded(filePath);
                l.playbackStateChanged(autoPlay);
            });
        });
    }).detach();

    return true;
}

void AudioEngine::play()
{
    const juce::ScopedLock sl(voiceLock);
    if (loadedVoice != nullptr && activeVoices.empty())
    {
        auto voice = std::make_shared<AudioVoice>();
        voice->buffer.makeCopyOf(loadedVoice->buffer);
        voice->ratio = loadedVoice->ratio;
        
        double sr = (engineSampleRate > 0.0) ? engineSampleRate : 44100.0;
        double startRatio = sampleStartRatio;
        
        // Resume from stoppedPositionSecs if valid and within selection bounds
        double currentRatio = startRatio;
        double totalLen = (loadedVoice->buffer.getNumSamples() / loadedVoice->ratio) / sr;
        if (totalLen > 0.0)
        {
            currentRatio = stoppedPositionSecs / totalLen;
            if (currentRatio < startRatio || currentRatio > sampleEndRatio - 0.01)
                currentRatio = startRatio;
        }

        voice->readPosition = currentRatio * voice->buffer.getNumSamples();
        voice->isLooping = isLoopingEnabled;
        voice->finished = false;
        voice->startRatio = sampleStartRatio;
        voice->endRatio = sampleEndRatio;
        
        activeVoices.clear();
        activeVoices.push_back(voice);
        
        listeners.call([](AudioEngineListener& l) {
            l.playbackStateChanged(true);
        });
    }
}

void AudioEngine::pause()
{
    const juce::ScopedLock sl(voiceLock);
    if (!activeVoices.empty())
    {
        auto& v = activeVoices.back();
        double sr = (engineSampleRate > 0.0) ? engineSampleRate : 44100.0;
        stoppedPositionSecs = (v->readPosition / v->ratio) / sr;
    }
    activeVoices.clear();
    listeners.call([](AudioEngineListener& l) { l.playbackStateChanged(false); });
}

void AudioEngine::stop()
{
    const juce::ScopedLock sl(voiceLock);
    stoppedPositionSecs = 0.0;
    if (loadedVoice != nullptr)
    {
        double sr = (engineSampleRate > 0.0) ? engineSampleRate : 44100.0;
        double totalLen = (loadedVoice->buffer.getNumSamples() / loadedVoice->ratio) / sr;
        stoppedPositionSecs = sampleStartRatio * totalLen;
    }
    activeVoices.clear();
    listeners.call([](AudioEngineListener& l) { l.playbackStateChanged(false); });
}

void AudioEngine::setPositionRatio(double ratio)
{
    const juce::ScopedLock sl(voiceLock);
    double clamped = juce::jlimit(sampleStartRatio, sampleEndRatio, ratio);
    
    if (loadedVoice != nullptr)
    {
        double sr = (engineSampleRate > 0.0) ? engineSampleRate : 44100.0;
        double totalLen = (loadedVoice->buffer.getNumSamples() / loadedVoice->ratio) / sr;
        stoppedPositionSecs = clamped * totalLen;
    }

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
        double sr = (engineSampleRate > 0.0) ? engineSampleRate : 44100.0;
        return (v->readPosition / v->ratio) / sr;
    }
    return stoppedPositionSecs;
}

double AudioEngine::getTotalLengthSeconds() const
{
    const juce::ScopedLock sl(voiceLock);
    if (loadedVoice != nullptr)
    {
        double sr = (engineSampleRate > 0.0) ? engineSampleRate : 44100.0;
        return (loadedVoice->buffer.getNumSamples() / loadedVoice->ratio) / sr;
    }
    return 0.0;
}

void AudioEngine::getMinMaxForTimeRange(double startTimeSecs, double endTimeSecs, float& minVal, float& maxVal, int channel) const
{
    minVal = 0.0f;
    maxVal = 0.0f;

    const juce::ScopedLock sl(voiceLock);
    if (loadedVoice == nullptr)
        return;

    const auto& buffer = loadedVoice->buffer;
    int numSamples = buffer.getNumSamples();
    if (numSamples <= 0)
        return;

    double sr = (engineSampleRate > 0.0) ? engineSampleRate : 44100.0;
    double fileLengthSecs = (numSamples / loadedVoice->ratio) / sr;
    if (fileLengthSecs <= 0.0)
        return;

    int startSample = juce::jlimit(0, numSamples - 1, static_cast<int>((startTimeSecs / fileLengthSecs) * numSamples));
    int endSample = juce::jlimit(startSample + 1, numSamples, static_cast<int>((endTimeSecs / fileLengthSecs) * numSamples));
    int numToRead = endSample - startSample;

    if (numToRead <= 0)
        return;

    int numChannels = buffer.getNumChannels();
    if (channel >= 0)
    {
        int ch = std::min(channel, numChannels - 1);
        auto range = buffer.findMinMax(ch, startSample, numToRead);
        minVal = range.getStart();
        maxVal = range.getEnd();
    }
    else
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto range = buffer.findMinMax(ch, startSample, numToRead);
            if (range.getStart() < minVal) minVal = range.getStart();
            if (range.getEnd() > maxVal) maxVal = range.getEnd();
        }
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

void AudioEngine::setSampleRange(double startRatio, double endRatio)
{
    const juce::ScopedLock sl(voiceLock);
    sampleStartRatio = juce::jlimit(0.0, 1.0, startRatio);
    sampleEndRatio = juce::jlimit(0.0, 1.0, endRatio);
    if (sampleEndRatio < sampleStartRatio + 0.01)
        sampleEndRatio = sampleStartRatio + 0.01;

    for (auto& v : activeVoices)
    {
        v->startRatio = sampleStartRatio;
        v->endRatio = sampleEndRatio;

        // Reset playback readPosition if it's out of range
        int numSamples = v->buffer.getNumSamples();
        double startPos = v->startRatio * numSamples;
        double endPos = v->endRatio * numSamples;
        if (v->readPosition < startPos || v->readPosition > endPos)
            v->readPosition = startPos;
    }
}

bool AudioEngine::getAudioBufferCopy(juce::AudioBuffer<float>& destBuffer, double& sampleRate) const
{
    const juce::ScopedLock sl(voiceLock);
    if (loadedVoice == nullptr)
        return false;

    destBuffer.makeCopyOf(loadedVoice->buffer);
    sampleRate = engineSampleRate;
    return true;
}

} // namespace openwav
