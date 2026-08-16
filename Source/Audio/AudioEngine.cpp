#include "AudioEngine.h"
#include <juce_dsp/juce_dsp.h>
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
    formatManager.registerFormat(new juce::WavAudioFormat(), true);
    formatManager.registerFormat(new juce::AiffAudioFormat(), false);
    formatManager.registerFormat(new juce::FlacAudioFormat(), false);
    formatManager.registerFormat(new juce::OggVorbisAudioFormat(), false);
    formatManager.registerBasicFormats();

    float baseFreqs[9] = { 60.0f, 120.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f };
    for (int i = 0; i < 9; ++i)
    {
        eqFreqs[i].store(baseFreqs[i]);
        eqGains[i].store(0.0f);
    }

    backgroundThread.startThread(juce::Thread::Priority::high);
}

AudioEngine::~AudioEngine()
{
    stop();
    stopRecording();
    currentLoadId++;
    thumbnail.setSource(nullptr);
    thumbnailCache.clear();
    backgroundThread.signalThreadShouldExit();
    backgroundThread.stopThread(100);
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
    keyboardState.processNextMidiBuffer(midiMessages, 0, outputBuffer.getNumSamples(), true);

    // Capture live incoming audio levels and recording data before clearing buffer
    int inChannels = outputBuffer.getNumChannels();
    int numSamples = outputBuffer.getNumSamples();
    bool monitorInput = !inputMuted.load();

    juce::AudioBuffer<float> inputMonitorBuffer;

    if (inChannels > 0 && numSamples > 0)
    {
        if (monitorInput)
        {
            // Apply Live Real-Time 9-Band Parametric EQ & 80Hz Low Cut to incoming live audio
            bool lowCut = eqLowCutEnabled.load();

            struct BiquadCoeffs
            {
                float b0 { 1.0f }, b1 { 0.0f }, b2 { 0.0f }, a1 { 0.0f }, a2 { 0.0f };
            };

            auto calcCoeffs = [fs = engineSampleRate](int type, float f0, float gainDb, float Q) -> BiquadCoeffs {
                BiquadCoeffs c;
                if (fs <= 0.0) return c;
                float w0 = 2.0f * juce::MathConstants<float>::pi * f0 / static_cast<float>(fs);
                float cosW = std::cos(w0);
                float sinW = std::sin(w0);
                float alpha = sinW / (2.0f * Q);
                float A = std::pow(10.0f, gainDb / 40.0f);

                float a0 = 1.0f;
                if (type == 0) // Low Cut (80Hz High Pass)
                {
                    c.b0 = (1.0f + cosW) * 0.5f;
                    c.b1 = -(1.0f + cosW);
                    c.b2 = (1.0f + cosW) * 0.5f;
                    a0 = 1.0f + alpha;
                    c.a1 = -2.0f * cosW;
                    c.a2 = 1.0f - alpha;
                }
                else if (type == 1) // Low Shelf
                {
                    float beta = std::sqrt(A) / Q;
                    c.b0 = A * ((A + 1.0f) - (A - 1.0f) * cosW + beta * sinW);
                    c.b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosW);
                    c.b2 = A * ((A + 1.0f) - (A - 1.0f) * cosW - beta * sinW);
                    a0 = (A + 1.0f) + (A - 1.0f) * cosW + beta * sinW;
                    c.a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cosW);
                    c.a2 = (A + 1.0f) + (A - 1.0f) * cosW - beta * sinW;
                }
                else if (type == 2) // Mid Peak/Bell
                {
                    c.b0 = 1.0f + alpha * A;
                    c.b1 = -2.0f * cosW;
                    c.b2 = 1.0f - alpha * A;
                    a0 = 1.0f + alpha / A;
                    c.a1 = -2.0f * cosW;
                    c.a2 = 1.0f - alpha / A;
                }
                else if (type == 3) // High Shelf
                {
                    float beta = std::sqrt(A) / Q;
                    c.b0 = A * ((A + 1.0f) + (A - 1.0f) * cosW + beta * sinW);
                    c.b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosW);
                    c.b2 = A * ((A + 1.0f) + (A - 1.0f) * cosW - beta * sinW);
                    a0 = (A + 1.0f) - (A - 1.0f) * cosW + beta * sinW;
                    c.a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cosW);
                    c.a2 = (A + 1.0f) - (A - 1.0f) * cosW - beta * sinW;
                }

                if (std::abs(a0) > 1e-6f)
                {
                    c.b0 /= a0;
                    c.b1 /= a0;
                    c.b2 /= a0;
                    c.a1 /= a0;
                    c.a2 /= a0;
                }
                return c;
            };

            auto processFilter = [numSamples](float* samples, const BiquadCoeffs& c, BiquadState& s) {
                for (int i = 0; i < numSamples; ++i)
                {
                    float x = samples[i];
                    float y = c.b0 * x + c.b1 * s.x1 + c.b2 * s.x2 - c.a1 * s.y1 - c.a2 * s.y2;
                    s.x2 = s.x1;
                    s.x1 = x;
                    s.y2 = s.y1;
                    s.y1 = y;
                    samples[i] = y;
                }
            };

            if (lowCut)
            {
                auto c = calcCoeffs(0, 80.0f, 0.0f, 0.707f);
                for (int ch = 0; ch < inChannels; ++ch)
                    processFilter(outputBuffer.getWritePointer(ch), c, inputFilterStates[0][std::min(ch, 1)]);
            }

            for (int band = 0; band < 9; ++band)
            {
                float f = eqFreqs[band].load();
                float g = eqGains[band].load();
                
                if (std::abs(g) > 0.05f)
                {
                    // Use a slightly sharper Q for the 9-band EQ
                    auto c = calcCoeffs(1 + band, f, g, 1.414f);
                    for (int ch = 0; ch < inChannels; ++ch)
                        processFilter(outputBuffer.getWritePointer(ch), c, inputFilterStates[1 + band][std::min(ch, 1)]);
                }
            }

            inputMonitorBuffer.makeCopyOf(outputBuffer);

            float maxL = outputBuffer.getMagnitude(0, 0, numSamples);
            float maxR = (inChannels >= 2) ? outputBuffer.getMagnitude(1, 0, numSamples) : maxL;
            liveInputLeft.store(maxL);
            liveInputRight.store(maxR);

            if (recordingActive.load())
            {
                const juce::ScopedLock sl(recordingLock);
                auto mode = channelMode.load();
                int spaceLeft = recordingBuffer.getNumSamples() - recordingWritePosition;
                int toWrite = std::min(numSamples, spaceLeft);
                if (toWrite <= 0)
                {
                    int currentLen = recordingBuffer.getNumSamples();
                    int addChunk = static_cast<int>(engineSampleRate * 30.0); // add 30s
                    if (currentLen + addChunk <= static_cast<int>(engineSampleRate * 600.0)) // Max 10 min
                    {
                        recordingBuffer.setSize(2, currentLen + addChunk, true, true, false);
                        spaceLeft = recordingBuffer.getNumSamples() - recordingWritePosition;
                        toWrite = std::min(numSamples, spaceLeft);
                    }
                    else
                    {
                        recordingActive.store(false);
                    }
                }

                if (toWrite > 0 && recordingActive.load())
                {
                    if (mode == RecordingChannelMode::MonoLeft)
                    {
                        recordingBuffer.copyFrom(0, recordingWritePosition, outputBuffer, 0, 0, toWrite);
                        recordingBuffer.copyFrom(1, recordingWritePosition, outputBuffer, 0, 0, toWrite);
                    }
                    else if (mode == RecordingChannelMode::MonoRight)
                    {
                        int srcCh = std::min(1, inChannels - 1);
                        recordingBuffer.copyFrom(0, recordingWritePosition, outputBuffer, srcCh, 0, toWrite);
                        recordingBuffer.copyFrom(1, recordingWritePosition, outputBuffer, srcCh, 0, toWrite);
                    }
                    else // Stereo
                    {
                        for (int ch = 0; ch < recordingBuffer.getNumChannels(); ++ch)
                        {
                            int srcCh = std::min(ch, inChannels - 1);
                            recordingBuffer.copyFrom(ch, recordingWritePosition, outputBuffer, srcCh, 0, toWrite);
                        }
                    }
                    recordingWritePosition += toWrite;
                }
            }
        }
        else
        {
            liveInputLeft.store(0.0f);
            liveInputRight.store(0.0f);
        }
    }

    outputBuffer.clear();

    if (monitorInput && inputMonitorBuffer.getNumChannels() > 0 && inputMonitorBuffer.getNumSamples() > 0)
    {
        for (int ch = 0; ch < outputBuffer.getNumChannels(); ++ch)
        {
            int srcCh = std::min(ch, inputMonitorBuffer.getNumChannels() - 1);
            outputBuffer.addFrom(ch, 0, inputMonitorBuffer, srcCh, 0, numSamples);
        }
    }

    const juce::ScopedLock sl(voiceLock);
    if (activeVoices.empty())
        return;

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
            float envVal = voice->adsr.getNextSample();
            float voiceVol = gainLevel * voice->gain * envVal;

            int idx = static_cast<int>(pos);
            if (idx >= endSample || idx >= voiceLength || !voice->adsr.isActive())
            {
                if (voice->isLooping && endSample > startSample && voice->adsr.isActive())
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
                float sample = (s1 + frac * (s2 - s1)) * voiceVol;

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

    // Process Reverb DSP on Sampler output if global samplerReverbAmount > 0.0f
    float revAmount = samplerReverbAmount.load();
    if (revAmount > 0.001f && outputBuffer.getNumChannels() >= 2 && !activeVoices.empty())
    {
        reverbParams.roomSize = 0.4f + revAmount * 0.5f;
        reverbParams.damping = 0.5f;
        reverbParams.wetLevel = revAmount * 0.5f;
        reverbParams.dryLevel = 1.0f - (revAmount * 0.2f);
        reverbParams.width = 1.0f;
        reverbDSP.setParameters(reverbParams);
        reverbDSP.processStereo(outputBuffer.getWritePointer(0), outputBuffer.getWritePointer(1), outputBuffer.getNumSamples());
    }
}

void AudioEngine::preloadSampleFiles(const std::vector<juce::File>& files)
{
    std::thread([this, files]() {
        for (const auto& file : files)
        {
            if (!file.existsAsFile()) continue;

            juce::String filePath = file.getFullPathName();

            {
                const juce::ScopedLock sl(voiceLock);
                if (sampleCache.find(filePath) != sampleCache.end())
                    continue;
            }

            std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
            if (reader != nullptr && reader->lengthInSamples > 0 && reader->numChannels > 0)
            {
                CachedSample cached;
                cached.sampleRate = (reader->sampleRate > 0.0) ? reader->sampleRate : 44100.0;
                cached.buffer.setSize(static_cast<int>(reader->numChannels), static_cast<int>(reader->lengthInSamples));
                reader->read(&cached.buffer, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);

                const juce::ScopedLock sl(voiceLock);
                if (sampleCache.size() > 200) sampleCache.clear();
                sampleCache[filePath] = cached;
            }
        }
    }).detach();
}

void AudioEngine::putSampleInCache(const juce::String& filePath, double sampleRate, const juce::AudioBuffer<float>& buf)
{
    const juce::ScopedLock sl(voiceLock);
    if (sampleCache.size() > 200) sampleCache.clear();
    CachedSample cached;
    cached.sampleRate = sampleRate;
    cached.buffer.makeCopyOf(buf);
    sampleCache[filePath] = cached;
}

void AudioEngine::playZoneVoice(const juce::File& file, int triggerMidiNote, int rootNote, float fineTuneCents, float gainDb, float velocity,
                               float attackSec, float decaySec, float sustainLevel, float releaseSec, bool isOneShot, bool isLooping)
{
    if (!file.existsAsFile()) return;

    juce::String filePath = file.getFullPathName();
    CachedSample cached;

    {
        const juce::ScopedLock sl(voiceLock);
        auto it = sampleCache.find(filePath);
        if (it != sampleCache.end())
        {
            cached = it->second;
        }
    }

    if (cached.buffer.getNumSamples() == 0)
    {
        std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
        if (reader == nullptr || reader->lengthInSamples <= 0 || reader->numChannels <= 0) return;

        cached.sampleRate = (reader->sampleRate > 0.0) ? reader->sampleRate : 44100.0;
        cached.buffer.setSize(static_cast<int>(reader->numChannels), static_cast<int>(reader->lengthInSamples));
        reader->read(&cached.buffer, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);

        const juce::ScopedLock sl(voiceLock);
        if (sampleCache.size() > 200) sampleCache.clear();
        sampleCache[filePath] = cached;
    }

    if (cached.buffer.getNumSamples() == 0) return;

    {
        const juce::ScopedLock sl(voiceLock);
        if (currentFile != file || loadedVoice == nullptr)
        {
            currentFile = file;
            currentFileSampleRate = cached.sampleRate;
            auto lVoice = std::make_shared<AudioVoice>();
            lVoice->buffer.makeCopyOf(cached.buffer);
            lVoice->ratio = (engineSampleRate > 0.0) ? (cached.sampleRate / engineSampleRate) : 1.0;
            lVoice->isLooping = isLooping || isLoopingEnabled;
            lVoice->startRatio = 0.0;
            lVoice->endRatio = 1.0;
            lVoice->rootNote = rootNote;
            loadedVoice = lVoice;
            rebuildWaveformPeaks();
            juce::MessageManager::callAsync([this, file] {
                thumbnail.setSource(new juce::FileInputSource(file));
            });
            listeners.call([filePath = file.getFullPathName()](AudioEngineListener& l) {
                l.sampleLoaded(filePath);
            });
        }
    }

    auto voice = std::make_shared<AudioVoice>();
    voice->buffer.makeCopyOf(cached.buffer);

    double semitoneDiff = pitchTrackingEnabled.load() ? ((triggerMidiNote - rootNote) + (fineTuneCents / 100.0)) : (fineTuneCents / 100.0);
    double srRatio = (cached.sampleRate > 0.0 && engineSampleRate > 0.0) ? (cached.sampleRate / engineSampleRate) : 1.0;
    voice->ratio = srRatio * std::pow(2.0, semitoneDiff / 12.0);

    voice->readPosition = 0.0;
    voice->startRatio = 0.0;
    voice->endRatio = 1.0;
    voice->isLooping = isLooping || isLoopingEnabled;
    voice->finished = false;
    voice->rootNote = rootNote;
    voice->triggerMidiNote = triggerMidiNote;
    voice->fineTuneCents = fineTuneCents;
    voice->bufferSampleRate = cached.sampleRate;
    voice->isZoneVoice = true;
    voice->isOneShot = isOneShot || oneShotEnabled.load();
    voice->gain = velocity * std::pow(10.0f, gainDb / 20.0f);

    juce::ADSR::Parameters adsrParams;
    adsrParams.attack = attackSec;
    adsrParams.decay = decaySec;
    adsrParams.sustain = sustainLevel;
    adsrParams.release = releaseSec;

    voice->adsr.setSampleRate(engineSampleRate > 0.0 ? engineSampleRate : 44100.0);
    voice->adsr.setParameters(adsrParams);
    voice->adsr.noteOn();

    const juce::ScopedLock sl(voiceLock);
    if (activeVoices.size() >= 32)
    {
        activeVoices.erase(activeVoices.begin());
    }
    activeVoices.push_back(voice);
}

void AudioEngine::stopZoneVoice(int triggerMidiNote)
{
    const juce::ScopedLock sl(voiceLock);
    for (auto& v : activeVoices)
    {
        if (v->triggerMidiNote == triggerMidiNote)
        {
            if (!v->isOneShot)
            {
                v->adsr.noteOff();
                v->isLooping = false;
            }
        }
    }
}

void AudioEngine::triggerNoteOn(int midiNoteNumber, float /*velocity*/)
{
    if (!midiInputEnabled.load())
        return;

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

    // Single sample playback outside of sample map does not apply keytracking
    voice->ratio = loadedVoice->ratio;

    voice->readPosition = sampleStartRatio * voice->buffer.getNumSamples();
    voice->isLooping = isLoopingEnabled;
    voice->finished = false;
    voice->startRatio = sampleStartRatio;
    voice->endRatio = sampleEndRatio;
    voice->rootNote = loadedVoice->rootNote;
    voice->triggerMidiNote = midiNoteNumber;
    voice->fineTuneCents = 0.0f;
    voice->bufferSampleRate = currentFileSampleRate;
    voice->isZoneVoice = false;
    voice->isOneShot = oneShotEnabled.load();

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
            if (!v->isOneShot)
            {
                v->finished = true;
            }
        }
    }
}

bool AudioEngine::loadFile(const juce::File& audioFile, bool autoPlay, bool isSamplerSample)
{
    isLoadedInSampler.store(isSamplerSample);
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
        peaks.numPoints = 2048;
        peaks.minLeft.resize(2048, 0.0f);
        peaks.maxLeft.resize(2048, 0.0f);
        peaks.minRight.resize(2048, 0.0f);
        peaks.maxRight.resize(2048, 0.0f);

        for (int p = 0; p < 2048; ++p)
        {
            int sStart = static_cast<int>((static_cast<double>(p) / 2048.0) * numSamples);
            int sEnd = static_cast<int>((static_cast<double>(p + 1) / 2048.0) * numSamples);
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
                
                updateVoiceRatios();
                
                if (autoPlay)
                {
                    activeVoices.clear(); // Kill previous voices so only the last selected voice plays
                    auto playVoice = std::make_shared<AudioVoice>();
                    playVoice->buffer = juce::AudioBuffer<float>(loadedVoice->buffer.getArrayOfWritePointers(), loadedVoice->buffer.getNumChannels(), loadedVoice->buffer.getNumSamples());
                    playVoice->ratio = loadedVoice->ratio;
                    playVoice->readPosition = sampleStartRatio * playVoice->buffer.getNumSamples();
                    playVoice->isLooping = isLoopingEnabled;
                    playVoice->finished = false;
                    playVoice->startRatio = sampleStartRatio;
                    playVoice->endRatio = sampleEndRatio;
                    playVoice->rootNote = loadedVoice->rootNote;

                    juce::ADSR::Parameters defaultAdsr;
                    defaultAdsr.attack = 0.005f;
                    defaultAdsr.decay = 0.1f;
                    defaultAdsr.sustain = 1.0f;
                    defaultAdsr.release = 0.05f;
                    playVoice->adsr.setSampleRate(engineSampleRate > 0.0 ? engineSampleRate : 44100.0);
                    playVoice->adsr.setParameters(defaultAdsr);
                    playVoice->adsr.noteOn();

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
        voice->triggerMidiNote = -1;
        voice->fineTuneCents = 0.0f;
        voice->bufferSampleRate = currentFileSampleRate;
        
        juce::ADSR::Parameters defaultAdsr;
        defaultAdsr.attack = 0.005f;
        defaultAdsr.decay = 0.1f;
        defaultAdsr.sustain = 1.0f;
        defaultAdsr.release = 0.05f;
        voice->adsr.setSampleRate(engineSampleRate > 0.0 ? engineSampleRate : 44100.0);
        voice->adsr.setParameters(defaultAdsr);
        voice->adsr.noteOn();

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

void AudioEngine::setPitchTrackingEnabled(bool enabled)
{
    pitchTrackingEnabled.store(enabled);
    updateVoiceRatios();
    listeners.call([enabled](AudioEngineListener& l) { l.pitchTrackingStateChanged(enabled); });
}

void AudioEngine::setOneShotEnabled(bool enabled)
{
    oneShotEnabled.store(enabled);
    listeners.call([enabled](AudioEngineListener& l) { l.oneShotStateChanged(enabled); });
}

void AudioEngine::setLooping(bool shouldLoop)
{
    isLoopingEnabled = shouldLoop;
    {
        const juce::ScopedLock sl(voiceLock);
        for (auto& v : activeVoices)
        {
            v->isLooping = isLoopingEnabled;
        }
    }
    listeners.call([shouldLoop](AudioEngineListener& l) { l.loopingStateChanged(shouldLoop); });
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

bool AudioEngine::cropLoadedSample(double startRatio, double endRatio)
{
    const juce::ScopedLock sl(voiceLock);
    if (loadedVoice == nullptr || loadedVoice->buffer.getNumSamples() == 0)
        return false;

    startRatio = juce::jlimit(0.0, 1.0, startRatio);
    endRatio = juce::jlimit(0.0, 1.0, endRatio);
    if (endRatio <= startRatio + 0.0001)
        return false;

    int totalSamples = loadedVoice->buffer.getNumSamples();
    int startSample = static_cast<int>(startRatio * totalSamples);
    int endSample = static_cast<int>(endRatio * totalSamples);
    int newNumSamples = endSample - startSample;
    if (newNumSamples <= 0)
        return false;

    juce::AudioBuffer<float> croppedBuffer(loadedVoice->buffer.getNumChannels(), newNumSamples);
    for (int ch = 0; ch < loadedVoice->buffer.getNumChannels(); ++ch)
    {
        croppedBuffer.copyFrom(ch, 0, loadedVoice->buffer, ch, startSample, newNumSamples);
    }

    loadedVoice->buffer = std::move(croppedBuffer);
    sampleStartRatio = 0.0;
    sampleEndRatio = 1.0;
    stoppedPositionSecs = 0.0;

    for (auto& v : activeVoices)
    {
        v->buffer = juce::AudioBuffer<float>(loadedVoice->buffer.getArrayOfWritePointers(), loadedVoice->buffer.getNumChannels(), loadedVoice->buffer.getNumSamples());
        v->startRatio = 0.0;
        v->endRatio = 1.0;
        v->readPosition = 0;
    }

    rebuildWaveformPeaks();
    replaceEditedSampleInMemoryAndDisk();
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
    rebuildWaveformPeaks();
    replaceEditedSampleInMemoryAndDisk();
    return true;
}

void AudioEngine::rebuildWaveformPeaks()
{
    if (loadedVoice == nullptr || loadedVoice->buffer.getNumSamples() == 0)
        return;

    const int targetNumPoints = 2000;
    int totalSamples = loadedVoice->buffer.getNumSamples();
    int numChannels = loadedVoice->buffer.getNumChannels();

    WaveformPeaks peaks;
    peaks.numChannels = numChannels;
    peaks.numPoints = targetNumPoints;
    peaks.minLeft.resize(targetNumPoints, 0.0f);
    peaks.maxLeft.resize(targetNumPoints, 0.0f);
    peaks.minRight.resize(targetNumPoints, 0.0f);
    peaks.maxRight.resize(targetNumPoints, 0.0f);

    double samplesPerPoint = static_cast<double>(totalSamples) / targetNumPoints;

    for (int p = 0; p < targetNumPoints; ++p)
    {
        int sStart = static_cast<int>(p * samplesPerPoint);
        int sEnd = std::min(totalSamples, static_cast<int>((p + 1) * samplesPerPoint));
        int numRead = std::max(1, sEnd - sStart);

        auto rL = loadedVoice->buffer.findMinMax(0, sStart, numRead);
        peaks.minLeft[p] = rL.getStart();
        peaks.maxLeft[p] = rL.getEnd();

        if (numChannels >= 2)
        {
            auto rR = loadedVoice->buffer.findMinMax(1, sStart, numRead);
            peaks.minRight[p] = rR.getStart();
            peaks.maxRight[p] = rR.getEnd();
        }
        else
        {
            peaks.minRight[p] = peaks.minLeft[p];
            peaks.maxRight[p] = peaks.maxLeft[p];
        }
    }

    cachedWaveformPeaks = std::move(peaks);
}

bool AudioEngine::silenceSelection(double startRatio, double endRatio)
{
    const juce::ScopedLock sl(voiceLock);
    if (loadedVoice == nullptr || loadedVoice->buffer.getNumSamples() == 0)
        return false;

    int numSamples = loadedVoice->buffer.getNumSamples();
    int startIdx = juce::jlimit(0, numSamples - 1, static_cast<int>(startRatio * numSamples));
    int endIdx = juce::jlimit(startIdx + 1, numSamples, static_cast<int>(endRatio * numSamples));

    for (int ch = 0; ch < loadedVoice->buffer.getNumChannels(); ++ch)
    {
        loadedVoice->buffer.clear(ch, startIdx, endIdx - startIdx);
    }
    rebuildWaveformPeaks();
    replaceEditedSampleInMemoryAndDisk();
    return true;
}

bool AudioEngine::silenceSpectralRegion(double startRatio, double endRatio, float minFreqHz, float maxFreqHz)
{
    const juce::ScopedLock sl(voiceLock);
    if (loadedVoice == nullptr || loadedVoice->buffer.getNumSamples() == 0)
        return false;

    int totalSamples = loadedVoice->buffer.getNumSamples();
    int startIdx = juce::jlimit(0, totalSamples - 1, static_cast<int>(startRatio * totalSamples));
    int endIdx = juce::jlimit(startIdx + 1, totalSamples, static_cast<int>(endRatio * totalSamples));
    int count = endIdx - startIdx;
    if (count <= 64) return false;

    double sr = (currentFileSampleRate > 0.0) ? currentFileSampleRate : 44100.0;
    int numChannels = loadedVoice->buffer.getNumChannels();

    constexpr int fftOrder = 10; // 1024 points
    constexpr int fftSize = 1 << fftOrder;
    constexpr int hopSize = 256;

    juce::dsp::FFT fft(fftOrder);
    juce::dsp::WindowingFunction<float> window(fftSize, juce::dsp::WindowingFunction<float>::hann);

    int binMin = juce::jlimit(0, fftSize / 2, static_cast<int>((minFreqHz / (sr * 0.5f)) * (fftSize / 2)));
    int binMax = juce::jlimit(binMin, fftSize / 2, static_cast<int>((maxFreqHz / (sr * 0.5f)) * (fftSize / 2)));

    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* samples = loadedVoice->buffer.getWritePointer(ch, startIdx);
        std::vector<float> outputBuffer(count, 0.0f);
        std::vector<float> windowSum(count, 0.0f);

        std::vector<float> fftBuffer(fftSize * 2, 0.0f);

        for (int pos = 0; pos + fftSize <= count; pos += hopSize)
        {
            std::fill(fftBuffer.begin(), fftBuffer.end(), 0.0f);
            std::copy(samples + pos, samples + pos + fftSize, fftBuffer.begin());
            window.multiplyWithWindowingTable(fftBuffer.data(), fftSize);

            fft.performRealOnlyForwardTransform(fftBuffer.data());

            // Zero out frequency bins within [binMin, binMax]
            for (int b = binMin; b <= binMax; ++b)
            {
                fftBuffer[2 * b] = 0.0f;     // Real part
                fftBuffer[2 * b + 1] = 0.0f; // Imaginary part
            }

            fft.performRealOnlyInverseTransform(fftBuffer.data());
            window.multiplyWithWindowingTable(fftBuffer.data(), fftSize);

            for (int i = 0; i < fftSize; ++i)
            {
                outputBuffer[pos + i] += fftBuffer[i];
                float w = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * i / (fftSize - 1)));
                windowSum[pos + i] += w * w;
            }
        }

        for (int i = 0; i < count; ++i)
        {
            if (windowSum[i] > 0.0001f)
                samples[i] = outputBuffer[i] / (windowSum[i] * (fftSize / (2.0f * hopSize)));
        }
    }

    rebuildWaveformPeaks();
    replaceEditedSampleInMemoryAndDisk();
    return true;
}

bool AudioEngine::adjustSpectralRegionGain(double startRatio, double endRatio, float minFreqHz, float maxFreqHz, float gaindB)
{
    const juce::ScopedLock sl(voiceLock);
    if (loadedVoice == nullptr || loadedVoice->buffer.getNumSamples() == 0)
        return false;

    int totalSamples = loadedVoice->buffer.getNumSamples();
    int startIdx = juce::jlimit(0, totalSamples - 1, static_cast<int>(startRatio * totalSamples));
    int endIdx = juce::jlimit(startIdx + 1, totalSamples, static_cast<int>(endRatio * totalSamples));
    int count = endIdx - startIdx;
    if (count <= 64) return false;

    double sr = (currentFileSampleRate > 0.0) ? currentFileSampleRate : 44100.0;
    int numChannels = loadedVoice->buffer.getNumChannels();

    constexpr int fftOrder = 10; // 1024 points
    constexpr int fftSize = 1 << fftOrder;
    constexpr int hopSize = 256;

    juce::dsp::FFT fft(fftOrder);
    juce::dsp::WindowingFunction<float> window(fftSize, juce::dsp::WindowingFunction<float>::hann);

    int binMin = juce::jlimit(0, fftSize / 2, static_cast<int>((minFreqHz / (sr * 0.5f)) * (fftSize / 2)));
    int binMax = juce::jlimit(binMin, fftSize / 2, static_cast<int>((maxFreqHz / (sr * 0.5f)) * (fftSize / 2)));

    float gainScale = juce::Decibels::decibelsToGain(gaindB);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* samples = loadedVoice->buffer.getWritePointer(ch, startIdx);
        std::vector<float> outputBuffer(count, 0.0f);
        std::vector<float> windowSum(count, 0.0f);
        std::vector<float> fftBuffer(fftSize * 2, 0.0f);

        for (int pos = 0; pos + fftSize <= count; pos += hopSize)
        {
            std::fill(fftBuffer.begin(), fftBuffer.end(), 0.0f);
            std::copy(samples + pos, samples + pos + fftSize, fftBuffer.begin());
            window.multiplyWithWindowingTable(fftBuffer.data(), fftSize);

            fft.performRealOnlyForwardTransform(fftBuffer.data());

            for (int b = binMin; b <= binMax; ++b)
            {
                fftBuffer[2 * b] *= gainScale;
                fftBuffer[2 * b + 1] *= gainScale;
            }

            fft.performRealOnlyInverseTransform(fftBuffer.data());
            window.multiplyWithWindowingTable(fftBuffer.data(), fftSize);

            for (int i = 0; i < fftSize; ++i)
            {
                outputBuffer[pos + i] += fftBuffer[i];
                float w = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * i / (fftSize - 1)));
                windowSum[pos + i] += w * w;
            }
        }

        for (int i = 0; i < count; ++i)
        {
            if (windowSum[i] > 0.0001f)
                samples[i] = outputBuffer[i] / (windowSum[i] * (fftSize / (2.0f * hopSize)));
        }
    }

    rebuildWaveformPeaks();
    replaceEditedSampleInMemoryAndDisk();
    return true;
}

bool AudioEngine::isolateSpectralRegion(double startRatio, double endRatio, float minFreqHz, float maxFreqHz)
{
    const juce::ScopedLock sl(voiceLock);
    if (loadedVoice == nullptr || loadedVoice->buffer.getNumSamples() == 0)
        return false;

    int totalSamples = loadedVoice->buffer.getNumSamples();
    int startIdx = juce::jlimit(0, totalSamples - 1, static_cast<int>(startRatio * totalSamples));
    int endIdx = juce::jlimit(startIdx + 1, totalSamples, static_cast<int>(endRatio * totalSamples));
    int count = endIdx - startIdx;
    if (count <= 64) return false;

    double sr = (currentFileSampleRate > 0.0) ? currentFileSampleRate : 44100.0;
    int numChannels = loadedVoice->buffer.getNumChannels();

    constexpr int fftOrder = 10;
    constexpr int fftSize = 1 << fftOrder;
    constexpr int hopSize = 256;

    juce::dsp::FFT fft(fftOrder);
    juce::dsp::WindowingFunction<float> window(fftSize, juce::dsp::WindowingFunction<float>::hann);

    int binMin = juce::jlimit(0, fftSize / 2, static_cast<int>((minFreqHz / (sr * 0.5f)) * (fftSize / 2)));
    int binMax = juce::jlimit(binMin, fftSize / 2, static_cast<int>((maxFreqHz / (sr * 0.5f)) * (fftSize / 2)));

    for (int ch = 0; ch < numChannels; ++ch)
    {
        // Silence before selection start and after selection end
        if (startIdx > 0)
            loadedVoice->buffer.clear(ch, 0, startIdx);
        if (endIdx < totalSamples)
            loadedVoice->buffer.clear(ch, endIdx, totalSamples - endIdx);

        float* samples = loadedVoice->buffer.getWritePointer(ch, startIdx);
        std::vector<float> outputBuffer(count, 0.0f);
        std::vector<float> windowSum(count, 0.0f);
        std::vector<float> fftBuffer(fftSize * 2, 0.0f);

        for (int pos = 0; pos + fftSize <= count; pos += hopSize)
        {
            std::fill(fftBuffer.begin(), fftBuffer.end(), 0.0f);
            std::copy(samples + pos, samples + pos + fftSize, fftBuffer.begin());
            window.multiplyWithWindowingTable(fftBuffer.data(), fftSize);

            fft.performRealOnlyForwardTransform(fftBuffer.data());

            // Zero out all bins EXCEPT binMin ... binMax
            for (int b = 0; b < fftSize / 2; ++b)
            {
                if (b < binMin || b > binMax)
                {
                    fftBuffer[2 * b] = 0.0f;
                    fftBuffer[2 * b + 1] = 0.0f;
                }
            }

            fft.performRealOnlyInverseTransform(fftBuffer.data());
            window.multiplyWithWindowingTable(fftBuffer.data(), fftSize);

            for (int i = 0; i < fftSize; ++i)
            {
                outputBuffer[pos + i] += fftBuffer[i];
                float w = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * i / (fftSize - 1)));
                windowSum[pos + i] += w * w;
            }
        }

        for (int i = 0; i < count; ++i)
        {
            if (windowSum[i] > 0.0001f)
                samples[i] = outputBuffer[i] / (windowSum[i] * (fftSize / (2.0f * hopSize)));
        }
    }

    rebuildWaveformPeaks();
    replaceEditedSampleInMemoryAndDisk();
    return true;
}

bool AudioEngine::reverseSelection(double startRatio, double endRatio)
{
    const juce::ScopedLock sl(voiceLock);
    if (loadedVoice == nullptr || loadedVoice->buffer.getNumSamples() == 0)
        return false;

    int numSamples = loadedVoice->buffer.getNumSamples();
    int startIdx = juce::jlimit(0, numSamples - 1, static_cast<int>(startRatio * numSamples));
    int endIdx = juce::jlimit(startIdx + 1, numSamples, static_cast<int>(endRatio * numSamples));
    int count = endIdx - startIdx;
    if (count <= 1) return false;

    for (int ch = 0; ch < loadedVoice->buffer.getNumChannels(); ++ch)
    {
        float* data = loadedVoice->buffer.getWritePointer(ch, startIdx);
        std::reverse(data, data + count);
    }
    rebuildWaveformPeaks();
    replaceEditedSampleInMemoryAndDisk();
    return true;
}

bool AudioEngine::deverbSelection(double startRatio, double endRatio, float amount)
{
    const juce::ScopedLock sl(voiceLock);
    if (loadedVoice == nullptr || loadedVoice->buffer.getNumSamples() == 0)
        return false;

    int numSamples = loadedVoice->buffer.getNumSamples();
    int startIdx = juce::jlimit(0, numSamples - 1, static_cast<int>(startRatio * numSamples));
    int endIdx = juce::jlimit(startIdx + 1, numSamples, static_cast<int>(endRatio * numSamples));
    int count = endIdx - startIdx;
    if (count <= 1) return false;

    double sr = (currentFileSampleRate > 0.0) ? currentFileSampleRate : 44100.0;
    float amountClamped = juce::jlimit(0.1f, 1.0f, amount);

    // ── 3-Band Envelope Follower Coefficients ───────────
    // Band 0 (Low: <300Hz), Band 1 (Mid: 300Hz-3.5kHz), Band 2 (High: >3.5kHz)
    float attCoeffs[3] = {
        std::exp(-1.0f / static_cast<float>(sr * 0.004f)), // Low attack 4ms
        std::exp(-1.0f / static_cast<float>(sr * 0.0015f)),// Mid attack 1.5ms
        std::exp(-1.0f / static_cast<float>(sr * 0.0010f)) // High attack 1ms
    };
    float relCoeffs[3] = {
        std::exp(-1.0f / static_cast<float>(sr * 0.080f)), // Low release 80ms
        std::exp(-1.0f / static_cast<float>(sr * 0.040f)), // Mid release 40ms (aggressive)
        std::exp(-1.0f / static_cast<float>(sr * 0.030f))  // High release 30ms
    };
    float tailCoeffs[3] = {
        std::exp(-1.0f / static_cast<float>(sr * 0.400f)),
        std::exp(-1.0f / static_cast<float>(sr * 0.250f)),
        std::exp(-1.0f / static_cast<float>(sr * 0.200f))
    };

    // Minimum gain floors per band
    float floors[3] = {
        juce::jlimit(0.01f, 0.5f, std::pow(1.0f - amountClamped * 0.7f, 2.0f)),
        juce::jlimit(0.0005f, 0.3f, std::pow(1.0f - amountClamped, 3.0f)), // Deep mid squelch
        juce::jlimit(0.001f, 0.35f, std::pow(1.0f - amountClamped * 0.9f, 2.5f))
    };

    // Expansion exponents per band
    float powers[3] = {
        1.5f + amountClamped * 1.0f,
        2.0f + amountClamped * 2.0f, // 2.0 to 4.0 expansion power for mid room tail
        1.8f + amountClamped * 1.5f
    };

    int numChannels = loadedVoice->buffer.getNumChannels();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* samples = loadedVoice->buffer.getWritePointer(ch, startIdx);

        // 3-Band IIR Crossover Filters (300 Hz & 3500 Hz)
        juce::IIRFilter lp300, hp300, lp3500, hp3500;
        lp300.setCoefficients(juce::IIRCoefficients::makeLowPass(sr, 300.0));
        hp300.setCoefficients(juce::IIRCoefficients::makeHighPass(sr, 300.0));
        lp3500.setCoefficients(juce::IIRCoefficients::makeLowPass(sr, 3500.0));
        hp3500.setCoefficients(juce::IIRCoefficients::makeHighPass(sr, 3500.0));

        float fastEnv[3] = { 0.0f, 0.0f, 0.0f };
        float tailEnv[3] = { 0.0f, 0.0f, 0.0f };

        for (int i = 0; i < count; ++i)
        {
            float inputSample = samples[i];

            // Split into 3 sub-bands
            float xLow = lp300.processSingleSampleRaw(inputSample);
            float hpSample = hp300.processSingleSampleRaw(inputSample);
            float xMid = lp3500.processSingleSampleRaw(hpSample);
            float xHigh = hp3500.processSingleSampleRaw(hpSample);

            float subBands[3] = { xLow, xMid, xHigh };
            float outBands[3] = { 0.0f, 0.0f, 0.0f };

            // Process each band independently
            for (int b = 0; b < 3; ++b)
            {
                float mag = std::abs(subBands[b]);

                // Track transient envelope vs background diffuse tail
                if (mag > fastEnv[b])
                    fastEnv[b] = attCoeffs[b] * fastEnv[b] + (1.0f - attCoeffs[b]) * mag;
                else
                    fastEnv[b] = relCoeffs[b] * fastEnv[b] + (1.0f - relCoeffs[b]) * mag;

                tailEnv[b] = tailCoeffs[b] * tailEnv[b] + (1.0f - tailCoeffs[b]) * mag;

                float ratio = (tailEnv[b] > 0.000001f) ? (fastEnv[b] / (tailEnv[b] + 0.000001f)) : 1.0f;
                float normRatio = juce::jlimit(0.0f, 1.0f, ratio);

                float bandGain = std::pow(normRatio, powers[b]);
                bandGain = juce::jlimit(floors[b], 1.0f, bandGain);

                outBands[b] = subBands[b] * bandGain;
            }

            // Re-combine sub-band signals
            samples[i] = outBands[0] + outBands[1] + outBands[2];
        }
    }

    for (auto& v : activeVoices)
    {
        v->buffer = juce::AudioBuffer<float>(loadedVoice->buffer.getArrayOfWritePointers(), loadedVoice->buffer.getNumChannels(), loadedVoice->buffer.getNumSamples());
    }

    rebuildWaveformPeaks();
    replaceEditedSampleInMemoryAndDisk();
    return true;
}

bool AudioEngine::adjustGainSelection(double startRatio, double endRatio, float gaindB)
{
    const juce::ScopedLock sl(voiceLock);
    if (loadedVoice == nullptr || loadedVoice->buffer.getNumSamples() == 0)
        return false;

    int numSamples = loadedVoice->buffer.getNumSamples();
    int startIdx = juce::jlimit(0, numSamples - 1, static_cast<int>(startRatio * numSamples));
    int endIdx = juce::jlimit(startIdx + 1, numSamples, static_cast<int>(endRatio * numSamples));
    int count = endIdx - startIdx;
    if (count <= 0) return false;

    float gainFactor = juce::Decibels::decibelsToGain(gaindB);
    for (int ch = 0; ch < loadedVoice->buffer.getNumChannels(); ++ch)
    {
        loadedVoice->buffer.applyGain(ch, startIdx, count, gainFactor);
    }
    rebuildWaveformPeaks();
    replaceEditedSampleInMemoryAndDisk();
    return true;
}

bool AudioEngine::applyHighPassFilter(double startRatio, double endRatio, float cutoffHz)
{
    const juce::ScopedLock sl(voiceLock);
    if (loadedVoice == nullptr || loadedVoice->buffer.getNumSamples() == 0)
        return false;

    int numSamples = loadedVoice->buffer.getNumSamples();
    int startIdx = juce::jlimit(0, numSamples - 1, static_cast<int>(startRatio * numSamples));
    int endIdx = juce::jlimit(startIdx + 1, numSamples, static_cast<int>(endRatio * numSamples));
    int count = endIdx - startIdx;
    if (count <= 0) return false;

    double sr = (currentFileSampleRate > 0.0) ? currentFileSampleRate : 44100.0;
    juce::IIRFilter hpFilter;
    hpFilter.setCoefficients(juce::IIRCoefficients::makeHighPass(sr, cutoffHz));

    for (int ch = 0; ch < loadedVoice->buffer.getNumChannels(); ++ch)
    {
        hpFilter.reset();
        hpFilter.processSamples(loadedVoice->buffer.getWritePointer(ch, startIdx), count);
    }
    rebuildWaveformPeaks();
    replaceEditedSampleInMemoryAndDisk();
    return true;
}

bool AudioEngine::autoTrimSilence()
{
    const juce::ScopedLock sl(voiceLock);
    if (loadedVoice == nullptr || loadedVoice->buffer.getNumSamples() == 0)
        return false;

    int numSamples = loadedVoice->buffer.getNumSamples();
    int numChannels = loadedVoice->buffer.getNumChannels();
    const float silenceThreshold = 0.003f; // ~ -50dB

    int startSample = 0;
    for (int i = 0; i < numSamples; ++i)
    {
        bool loud = false;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            if (std::abs(loadedVoice->buffer.getSample(ch, i)) > silenceThreshold)
            {
                loud = true;
                break;
            }
        }
        if (loud) { startSample = i; break; }
    }

    int endSample = numSamples - 1;
    for (int i = numSamples - 1; i >= startSample; --i)
    {
        bool loud = false;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            if (std::abs(loadedVoice->buffer.getSample(ch, i)) > silenceThreshold)
            {
                loud = true;
                break;
            }
        }
        if (loud) { endSample = i; break; }
    }

    if (startSample >= endSample)
        return false;

    double newStartRatio = static_cast<double>(startSample) / numSamples;
    double newEndRatio = static_cast<double>(endSample + 1) / numSamples;
    return cropLoadedSample(newStartRatio, newEndRatio);
}

bool AudioEngine::invertPhaseSelection(double startRatio, double endRatio)
{
    const juce::ScopedLock sl(voiceLock);
    if (loadedVoice == nullptr || loadedVoice->buffer.getNumSamples() == 0)
        return false;

    int numSamples = loadedVoice->buffer.getNumSamples();
    int startIdx = juce::jlimit(0, numSamples - 1, static_cast<int>(startRatio * numSamples));
    int endIdx = juce::jlimit(startIdx + 1, numSamples, static_cast<int>(endRatio * numSamples));
    int count = endIdx - startIdx;
    if (count <= 0) return false;

    for (int ch = 0; ch < loadedVoice->buffer.getNumChannels(); ++ch)
    {
        loadedVoice->buffer.applyGain(ch, startIdx, count, -1.0f);
    }
    rebuildWaveformPeaks();
    replaceEditedSampleInMemoryAndDisk();
    return true;
}

bool AudioEngine::changeSampleSpeed(double speedMultiplier)
{
    const juce::ScopedLock sl(voiceLock);
    if (loadedVoice == nullptr || loadedVoice->buffer.getNumSamples() == 0 || speedMultiplier <= 0.05)
        return false;

    int origSamples = loadedVoice->buffer.getNumSamples();
    int newNumSamples = static_cast<int>(origSamples / speedMultiplier);
    if (newNumSamples <= 10) return false;

    int numChannels = loadedVoice->buffer.getNumChannels();
    juce::AudioBuffer<float> resampledBuffer(numChannels, newNumSamples);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* src = loadedVoice->buffer.getReadPointer(ch);
        float* dst = resampledBuffer.getWritePointer(ch);
        for (int i = 0; i < newNumSamples; ++i)
        {
            double srcPos = i * speedMultiplier;
            int srcIdx = static_cast<int>(srcPos);
            double frac = srcPos - srcIdx;
            if (srcIdx >= origSamples - 1)
            {
                dst[i] = src[origSamples - 1];
            }
            else
            {
                dst[i] = static_cast<float>((1.0 - frac) * src[srcIdx] + frac * src[srcIdx + 1]);
            }
        }
    }

    loadedVoice->buffer = std::move(resampledBuffer);
    sampleStartRatio = 0.0;
    sampleEndRatio = 1.0;
    rebuildWaveformPeaks();
    replaceEditedSampleInMemoryAndDisk();
    return true;
}

bool AudioEngine::applyFadesToBuffer(double fadeInMs, int fadeInType, double fadeOutMs, int fadeOutType)
{
    const juce::ScopedLock sl(voiceLock);
    if (loadedVoice == nullptr || loadedVoice->buffer.getNumSamples() == 0)
        return false;

    int numSamples = loadedVoice->buffer.getNumSamples();
    int startIdx = juce::jlimit(0, numSamples - 1, static_cast<int>(sampleStartRatio * numSamples));
    int endIdx = juce::jlimit(startIdx + 1, numSamples, static_cast<int>(sampleEndRatio * numSamples));
    int selSamples = endIdx - startIdx;
    if (selSamples <= 0) return false;

    double sr = (currentFileSampleRate > 0.0) ? currentFileSampleRate : 44100.0;
    int fadeInSamples = std::min(selSamples, static_cast<int>((fadeInMs / 1000.0) * sr));
    int fadeOutSamples = std::min(selSamples, static_cast<int>((fadeOutMs / 1000.0) * sr));

    auto evalCurve = [](float t, int type) -> float {
        t = juce::jlimit(0.0f, 1.0f, t);
        if (type == 1) return std::sin(t * juce::MathConstants<float>::halfPi);
        if (type == 2) return t * t;
        return t;
    };

    int numChannels = loadedVoice->buffer.getNumChannels();

    if (fadeInSamples > 0)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = loadedVoice->buffer.getWritePointer(ch, startIdx);
            for (int i = 0; i < fadeInSamples; ++i)
            {
                float gain = evalCurve(static_cast<float>(i) / fadeInSamples, fadeInType);
                data[i] *= gain;
            }
        }
    }

    if (fadeOutSamples > 0)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = loadedVoice->buffer.getWritePointer(ch, endIdx - fadeOutSamples);
            for (int i = 0; i < fadeOutSamples; ++i)
            {
                float t = static_cast<float>(i) / fadeOutSamples;
                float gain = 1.0f - evalCurve(t, fadeOutType);
                data[i] *= gain;
            }
        }
    }

    rebuildWaveformPeaks();
    replaceEditedSampleInMemoryAndDisk();
    return true;
}

void AudioEngine::replaceEditedSampleInMemoryAndDisk()
{
    if (loadedVoice == nullptr)
        return;

    juce::String filePath = currentFile.getFullPathName();
    if (filePath.isNotEmpty() && currentFile.existsAsFile())
    {
        thumbnail.setSource(nullptr);

        juce::TemporaryFile tempFile(currentFile);
        {
            juce::WavAudioFormat wavFormat;
            std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(
                tempFile.getFile().createOutputStream().release(),
                currentFileSampleRate > 0.0 ? currentFileSampleRate : 44100.0,
                loadedVoice->buffer.getNumChannels(),
                16,
                {},
                0
            ));
            if (writer != nullptr)
            {
                writer->writeFromAudioSampleBuffer(loadedVoice->buffer, 0, loadedVoice->buffer.getNumSamples());
            }
        }
        tempFile.overwriteTargetFileWithTemporary();

        putSampleInCache(filePath, currentFileSampleRate, loadedVoice->buffer);

        thumbnail.setSource(new juce::FileInputSource(currentFile));
    }
    else if (filePath.isNotEmpty())
    {
        putSampleInCache(filePath, currentFileSampleRate, loadedVoice->buffer);
    }

    rebuildWaveformPeaks();

    for (auto& v : activeVoices)
    {
        v->buffer = juce::AudioBuffer<float>(loadedVoice->buffer.getArrayOfWritePointers(), loadedVoice->buffer.getNumChannels(), loadedVoice->buffer.getNumSamples());
    }

    listeners.call([filePath](AudioEngineListener& l) {
        l.sampleLoaded(filePath);
    });
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
    
    double defaultBaseRatio = (engineSampleRate > 0.0) ? (currentFileSampleRate / engineSampleRate) : 1.0;
    
    if (loadedVoice != nullptr)
    {
        loadedVoice->ratio = defaultBaseRatio;
    }
    
    bool pitchTrack = pitchTrackingEnabled.load();
    
    for (auto& voice : activeVoices)
    {
        double sr = (voice->bufferSampleRate > 0.0) ? voice->bufferSampleRate : currentFileSampleRate;
        double baseRatio = (engineSampleRate > 0.0 && sr > 0.0) ? (sr / engineSampleRate) : 1.0;
        
        double pitchOffsetSemis = 0.0;
        if (voice->isZoneVoice && pitchTrack && voice->triggerMidiNote != -1)
        {
            pitchOffsetSemis = static_cast<double>(voice->triggerMidiNote - voice->rootNote);
        }
        pitchOffsetSemis += (voice->fineTuneCents / 100.0);
        
        voice->ratio = baseRatio * std::pow(2.0, pitchOffsetSemis / 12.0);
    }
}

void AudioEngine::startRecording()
{
    const juce::ScopedLock sl(recordingLock);
    int initialAlloc = static_cast<int>(engineSampleRate * 60.0); // 60s initial buffer
    recordingBuffer.setSize(2, initialAlloc, false, true, false);
    recordingBuffer.clear();
    recordingWritePosition = 0;
    recordingActive.store(true);
}

void AudioEngine::stopRecording()
{
    recordingActive.store(false);
}

double AudioEngine::getRecordingDurationSeconds() const
{
    const juce::ScopedLock sl(recordingLock);
    double sr = (engineSampleRate > 0.0) ? engineSampleRate : 44100.0;
    return static_cast<double>(recordingWritePosition) / sr;
}

void AudioEngine::getLiveInputLevels(float& leftLevel, float& rightLevel) const
{
    leftLevel = liveInputLeft.load();
    rightLevel = liveInputRight.load();
}

bool AudioEngine::getRecordedBufferCopy(juce::AudioBuffer<float>& destBuffer, double& sampleRate) const
{
    const juce::ScopedLock sl(recordingLock);
    if (recordingWritePosition <= 0)
        return false;

    destBuffer.setSize(2, recordingWritePosition);
    for (int ch = 0; ch < 2; ++ch)
    {
        destBuffer.copyFrom(ch, 0, recordingBuffer, ch, 0, recordingWritePosition);
    }
    sampleRate = engineSampleRate;
    return true;
}

juce::File AudioEngine::saveRecordingToWav(const juce::String& baseFileName)
{
    juce::AudioBuffer<float> copyBuf;
    double sr = engineSampleRate;
    if (!getRecordedBufferCopy(copyBuf, sr) || copyBuf.getNumSamples() == 0)
        return {};

    juce::File recDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("OWMB_Recordings");
    recDir.createDirectory();

    juce::String cleanName = juce::File::createLegalFileName(baseFileName);
    if (cleanName.isEmpty())
        cleanName = "Rec_" + juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S");
    if (!cleanName.endsWithIgnoreCase(".wav"))
        cleanName += ".wav";

    juce::File destFile = recDir.getChildFile(cleanName);
    if (destFile.existsAsFile())
        destFile.deleteFile();

    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::AudioFormatWriter> writer(
        wavFormat.createWriterFor(new juce::FileOutputStream(destFile),
                                  sr,
                                  copyBuf.getNumChannels(),
                                  24, // 24-bit PCM WAV
                                  {},
                                  0)
    );

    if (writer != nullptr)
    {
        writer->writeFromAudioSampleBuffer(copyBuf, 0, copyBuf.getNumSamples());
        writer.reset();
        return destFile;
    }

    return {};
}

void AudioEngine::playMetronomeClick(bool isAccent)
{
    const juce::ScopedLock sl(voiceLock);
    double sr = (engineSampleRate > 0.0) ? engineSampleRate : 44100.0;
    int clickLength = static_cast<int>(sr * 0.015); // 15ms tick

    if (clickLength <= 0) return;

    auto voice = std::make_shared<AudioVoice>();
    voice->buffer.setSize(2, clickLength);
    voice->buffer.clear();

    double freq = isAccent ? 1200.0 : 800.0;
    float gain = isAccent ? 0.65f : 0.45f;

    for (int s = 0; s < clickLength; ++s)
    {
        float envelope = std::exp(-static_cast<float>(s) / (sr * 0.003f));
        float sample = static_cast<float>(std::sin(2.0 * juce::MathConstants<double>::pi * freq * (s / sr)) * envelope * gain);
        voice->buffer.setSample(0, s, sample);
        voice->buffer.setSample(1, s, sample);
    }

    voice->ratio = 1.0;
    voice->readPosition = 0.0;
    voice->isLooping = false;
    voice->finished = false;
    voice->startRatio = 0.0;
    voice->endRatio = 1.0;
    voice->rootNote = 60;
    voice->triggerMidiNote = -1;

    activeVoices.push_back(voice);
}

void AudioEngine::setInputParametricEq(const std::array<float, 9>& freqs, const std::array<float, 9>& gains, bool lowCut)
{
    for (int i = 0; i < 9; ++i)
    {
        eqFreqs[i].store(freqs[i]);
        eqGains[i].store(gains[i]);
    }
    eqLowCutEnabled.store(lowCut);
}

} // namespace openwav
