#include "AudioEngine.h"
#include <algorithm>

namespace openwav
{

static int getRootNoteFromFilename(const juce::String& filename)
{
    return -1;
}

static int getRootNoteFromWavSmplHeader(const juce::File& file)
{
    if (file.getFileExtension().equalsIgnoreCase(".wav"))
    {
        juce::FileInputStream stream(file);
        if (stream.openedOk())
        {
            char riffHead[4];
            if (stream.read(riffHead, 4) < 4 || std::memcmp(riffHead, "RIFF", 4) != 0)
                return -1;

            stream.skipNextBytes(4); // Skip size

            char waveHead[4];
            if (stream.read(waveHead, 4) < 4 || std::memcmp(waveHead, "WAVE", 4) != 0)
                return -1;

            while (!stream.isExhausted())
            {
                char chunkID[4];
                if (stream.read(chunkID, 4) < 4)
                    break;

                int64_t chunkSize = (uint32_t)stream.readInt();

                if (std::memcmp(chunkID, "smpl", 4) == 0)
                {
                    if (chunkSize >= 16)
                    {
                        stream.skipNextBytes(12);
                        int32_t rootNote = stream.readInt();
                        if (rootNote >= 0 && rootNote <= 127)
                            return rootNote;
                    }
                    break;
                }
                else
                {
                    int64_t bytesToSkip = (chunkSize + 1) & ~1;
                    stream.skipNextBytes(bytesToSkip);
                }
            }
        }
    }
    return -1;
}

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

void AudioEngine::processNextAudioBlock(juce::AudioBuffer<float>& outputBuffer, juce::MidiBuffer& midiMessages)
{
    {
        const juce::ScopedLock sl(voiceLock);
        for (const auto metadata : midiMessages)
        {
            auto msg = metadata.getMessage();
            if (msg.isNoteOn())
            {
                triggerNoteOn(msg.getNoteNumber(), msg.getFloatVelocity());
            }
            else if (msg.isNoteOff())
            {
                triggerNoteOff(msg.getNoteNumber());
            }
            else if (msg.isAllNotesOff())
            {
                activeVoices.clear();
            }
        }
    }

    midiMessages.clear();

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

void AudioEngine::triggerNoteOn(int midiNoteNumber, float /*velocity*/)
{
    const juce::ScopedLock sl(voiceLock);
    if (loadedVoice == nullptr)
        return;

    // Kill any existing voice with the same note trigger to avoid duplicate notes sounding forever
    for (auto& v : activeVoices)
    {
        if (v->triggerMidiNote == midiNoteNumber)
        {
            v->finished = true;
        }
    }

    auto voice = std::make_shared<AudioVoice>();
    voice->buffer = juce::AudioBuffer<float>(loadedVoice->buffer.getArrayOfWritePointers(), loadedVoice->buffer.getNumChannels(), loadedVoice->buffer.getNumSamples());

    // Pitch shift based on triggered midi note relative to root note
    double semitoneDiff = midiNoteNumber - loadedVoice->rootNote;
    voice->ratio = loadedVoice->ratio * std::pow(2.0, semitoneDiff / 12.0);

    voice->readPosition = sampleStartRatio * voice->buffer.getNumSamples();
    voice->isLooping = isLoopingEnabled;
    voice->finished = false;
    voice->startRatio = sampleStartRatio;
    voice->endRatio = sampleEndRatio;
    voice->rootNote = loadedVoice->rootNote;
    voice->triggerMidiNote = midiNoteNumber;

    activeVoices.push_back(voice);

    juce::MessageManager::callAsync([this] {
        listeners.call([](AudioEngineListener& l) { l.playbackStateChanged(true); });
    });
}

void AudioEngine::triggerNoteOff(int midiNoteNumber)
{
    const juce::ScopedLock sl(voiceLock);
    for (auto& v : activeVoices)
    {
        if (v->triggerMidiNote == midiNoteNumber)
        {
            v->finished = true;
        }
    }
}

bool AudioEngine::loadFile(const juce::File& audioFile, bool autoPlay)
{
    if (!audioFile.existsAsFile())
        return false;

    currentFile = audioFile;
    auto loadId = ++currentLoadId;

    // Reset range selection on new file load
    sampleStartRatio = 0.0;
    sampleEndRatio = 1.0;

    std::thread([this, audioFile, autoPlay, loadId]() {
        if (loadId != currentLoadId)
            return;

        std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(audioFile));
        if (reader == nullptr || loadId != currentLoadId)
            return;

        double fileSampleRate = reader->sampleRate;
        int64_t numSamples64 = reader->lengthInSamples;
        if (audioFile.getFileExtension().toLowerCase() == ".mp3" && fileSampleRate < 32000.0)
        {
            numSamples64 /= 2;
        }
        int numChannels = static_cast<int>(reader->numChannels);

        if (numSamples64 <= 0 || numSamples64 > 0x7FFFFFFF || numChannels <= 0)
            return;

        int numSamples = static_cast<int>(numSamples64);

        int rootNoteVal = getRootNoteFromWavSmplHeader(audioFile);
        if (rootNoteVal < 0 || rootNoteVal > 127)
            rootNoteVal = getRootNoteFromFilename(audioFile.getFileName());
        if (rootNoteVal < 0 || rootNoteVal > 127)
            rootNoteVal = 60; // Default to Middle C

        if (loadId != currentLoadId)
            return;

        auto voice = std::make_shared<AudioVoice>();
        voice->buffer.setSize(numChannels, numSamples);
        reader->read(&voice->buffer, 0, numSamples, 0, true, true);

        if (loadId != currentLoadId)
            return;

        voice->ratio = (engineSampleRate > 0.0) ? (fileSampleRate / engineSampleRate) : 1.0;
        voice->isLooping = isLoopingEnabled;
        voice->startRatio = 0.0;
        voice->endRatio = 1.0;
        voice->rootNote = rootNoteVal;

        // Precompute waveform peaks for fast, zero-lock UI rendering
        WaveformPeaks peaks;
        peaks.numChannels = numChannels;
        peaks.numPoints = 512;
        peaks.minLeft.resize(512, 0.0f);
        peaks.maxLeft.resize(512, 0.0f);
        peaks.minRight.resize(512, 0.0f);
        peaks.maxRight.resize(512, 0.0f);

        for (int p = 0; p < 512; ++p)
        {
            int sStart = static_cast<int>((static_cast<double>(p) / 512.0) * numSamples);
            int sEnd = static_cast<int>((static_cast<double>(p + 1) / 512.0) * numSamples);
            sEnd = juce::jlimit(sStart + 1, numSamples, sEnd);
            int numRead = sEnd - sStart;

            auto rL = voice->buffer.findMinMax(0, sStart, numRead);
            peaks.minLeft[p] = rL.getStart();
            peaks.maxLeft[p] = rL.getEnd();

            if (numChannels >= 2)
            {
                auto rR = voice->buffer.findMinMax(1, sStart, numRead);
                peaks.minRight[p] = rR.getStart();
                peaks.maxRight[p] = rR.getEnd();
            }
            else
            {
                peaks.minRight[p] = peaks.minLeft[p];
                peaks.maxRight[p] = peaks.maxLeft[p];
            }
        }

        if (loadId != currentLoadId)
            return;

        juce::MessageManager::callAsync([this, audioFile, autoPlay, loadId, voice, fileSampleRate, peaks = std::move(peaks)]() mutable {
            if (loadId != currentLoadId)
                return;

            thumbnail.setSource(new juce::FileInputSource(audioFile));

            {
                const juce::ScopedLock sl(voiceLock);
                currentFileSampleRate = fileSampleRate;
                loadedVoice = voice;
                cachedWaveformPeaks = std::move(peaks);
                stoppedPositionSecs = 0.0;
                activeVoices.clear(); // Kill previous voices so only the last selected voice plays
                
                updateVoiceRatios();
                
                if (autoPlay)
                {
                    auto playVoice = std::make_shared<AudioVoice>();
                    playVoice->buffer = juce::AudioBuffer<float>(loadedVoice->buffer.getArrayOfWritePointers(), loadedVoice->buffer.getNumChannels(), loadedVoice->buffer.getNumSamples());
                    playVoice->ratio = loadedVoice->ratio;
                    playVoice->readPosition = sampleStartRatio * playVoice->buffer.getNumSamples();
                    playVoice->isLooping = isLoopingEnabled;
                    playVoice->finished = false;
                    playVoice->startRatio = sampleStartRatio;
                    playVoice->endRatio = sampleEndRatio;
                    playVoice->rootNote = loadedVoice->rootNote;
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
        voice->buffer = juce::AudioBuffer<float>(loadedVoice->buffer.getArrayOfWritePointers(), loadedVoice->buffer.getNumChannels(), loadedVoice->buffer.getNumSamples());
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
        voice->rootNote = loadedVoice->rootNote;
        
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

int AudioEngine::getNumChannels() const
{
    const juce::ScopedLock sl(voiceLock);
    if (loadedVoice != nullptr)
        return loadedVoice->buffer.getNumChannels();
    return 0;
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

void AudioEngine::getMinMaxForRatioRange(double startRatio, double endRatio, float& minVal, float& maxVal, int channel) const
{
    minVal = 0.0f;
    maxVal = 0.0f;

    const juce::ScopedLock sl(voiceLock);
    if (loadedVoice == nullptr)
        return;

    const auto& buffer = loadedVoice->buffer;
    int numSamples = buffer.getNumSamples();
    int numChannels = buffer.getNumChannels();
    if (numSamples <= 0 || numChannels <= 0)
        return;

    double clampedStart = juce::jlimit(0.0, 1.0, startRatio);
    double clampedEnd = juce::jlimit(0.0, 1.0, endRatio);
    if (clampedStart > clampedEnd)
        std::swap(clampedStart, clampedEnd);

    int startSample = juce::jlimit(0, numSamples - 1, static_cast<int>(clampedStart * numSamples));
    int endSample = juce::jlimit(startSample + 1, numSamples, static_cast<int>(clampedEnd * numSamples));
    int numToRead = endSample - startSample;

    if (numToRead <= 0)
        return;

    if (channel >= 0)
    {
        if (channel < numChannels)
        {
            auto range = buffer.findMinMax(channel, startSample, numToRead);
            minVal = range.getStart();
            maxVal = range.getEnd();
        }
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

WaveformPeaks AudioEngine::getWaveformPeaks() const
{
    const juce::ScopedLock sl(voiceLock);
    return cachedWaveformPeaks;
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

bool AudioEngine::normalizeLoadedSample()
{
    const juce::ScopedLock sl(voiceLock);
    if (loadedVoice == nullptr || loadedVoice->buffer.getNumSamples() == 0)
        return false;

    float maxPeak = 0.0f;
    int numChannels = loadedVoice->buffer.getNumChannels();
    int numSamples = loadedVoice->buffer.getNumSamples();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto range = loadedVoice->buffer.findMinMax(ch, 0, numSamples);
        float peak = std::max(std::abs(range.getStart()), std::abs(range.getEnd()));
        if (peak > maxPeak)
            maxPeak = peak;
    }

    if (maxPeak <= 0.00001f || std::abs(maxPeak - 1.0f) < 0.001f)
    {
        return false; // Already normalized or silent
    }

    float scaleFactor = 1.0f / maxPeak;
    loadedVoice->buffer.applyGain(scaleFactor);

    for (auto& voice : activeVoices)
    {
        voice->buffer.applyGain(scaleFactor);
    }

    return true;
}

void AudioEngine::setSampleBpm(double bpm)
{
    sampleBpm.store(bpm);
    updateVoiceRatios();
}

void AudioEngine::setHostBpm(double bpm)
{
    hostBpm.store(bpm);
    updateVoiceRatios();
}

void AudioEngine::updateVoiceRatios()
{
    const juce::ScopedLock sl(voiceLock);
    
    double baseRatio = (engineSampleRate > 0.0) ? (currentFileSampleRate / engineSampleRate) : 1.0;
    double newBaseRatio = baseRatio;
    
    if (loadedVoice != nullptr)
    {
        loadedVoice->ratio = newBaseRatio;
    }
    
    for (auto& voice : activeVoices)
    {
        double midiFactor = 1.0;
        if (voice->triggerMidiNote != -1 && loadedVoice != nullptr)
        {
            double semitoneDiff = voice->triggerMidiNote - loadedVoice->rootNote;
            midiFactor = std::pow(2.0, semitoneDiff / 12.0);
        }
        voice->ratio = newBaseRatio * midiFactor;
    }
}

} // namespace openwav
