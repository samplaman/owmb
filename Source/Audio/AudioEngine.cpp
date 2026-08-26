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

            stream.skipNextBytes(4); // Skip RIFF size

            char waveHead[4];
            if (stream.read(waveHead, 4) < 4 || std::memcmp(waveHead, "WAVE", 4) != 0)
                return -1;

            int safetyCounter = 0;
            while (!stream.isExhausted() && ++safetyCounter < 500)
            {
                char chunkID[4];
                if (stream.read(chunkID, 4) < 4)
                    break;

                uint8_t sizeBytes[4];
                if (stream.read(sizeBytes, 4) < 4)
                    break;

                uint32_t chunkSize = static_cast<uint32_t>(sizeBytes[0]) |
                                    (static_cast<uint32_t>(sizeBytes[1]) << 8) |
                                    (static_cast<uint32_t>(sizeBytes[2]) << 16) |
                                    (static_cast<uint32_t>(sizeBytes[3]) << 24);

                if (std::memcmp(chunkID, "smpl", 4) == 0)
                {
                    if (chunkSize >= 16)
                    {
                        stream.skipNextBytes(12);
                        if (stream.read(sizeBytes, 4) == 4)
                        {
                            int32_t rootNote = static_cast<int32_t>(
                                static_cast<uint32_t>(sizeBytes[0]) |
                                (static_cast<uint32_t>(sizeBytes[1]) << 8) |
                                (static_cast<uint32_t>(sizeBytes[2]) << 16) |
                                (static_cast<uint32_t>(sizeBytes[3]) << 24));
                            if (rootNote >= 0 && rootNote <= 127)
                                return rootNote;
                        }
                    }
                    break;
                }
                else
                {
                    int64_t bytesToSkip = (static_cast<int64_t>(chunkSize) + 1) & ~1;
                    if (bytesToSkip <= 0)
                        break;
                    auto prevPos = stream.getPosition();
                    stream.skipNextBytes(bytesToSkip);
                    if (stream.getPosition() <= prevPos)
                        break;
                }
            }
        }
    }
    return -1;
}

AudioEngine::AudioEngine()
{
    formatManager.registerBasicFormats();
#if JUCE_USE_FLAC
    formatManager.registerFormat(new juce::FlacAudioFormat(), false);
#endif
#if JUCE_USE_OGGVORBIS
    formatManager.registerFormat(new juce::OggVorbisAudioFormat(), false);
#endif

    float baseFreqs[9] = { 60.0f, 120.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f };
    for (int i = 0; i < 9; ++i)
    {
        eqFreqs[i].store(baseFreqs[i], std::memory_order_relaxed);
        eqGains[i].store(0.0f, std::memory_order_relaxed);
    }

    // Pre-allocate recording buffer for 10 minutes stereo @ 44.1kHz default
    recordingBuffer.setSize(2, static_cast<int>(44100.0 * 600.0), false, true, false);
    recordingBuffer.clear();

    delayBuffer.setSize(2, 96000, false, true, false);
    delayBuffer.clear();
    chorusBuffer.setSize(2, 9600, false, true, false);
    chorusBuffer.clear();

    resetAllGroups();

    backgroundThread.startThread(juce::Thread::Priority::high);
    startTimerHz(30);
}

AudioEngine::~AudioEngine()
{
    stopTimer();
    stop();
    stopRecording();
    currentLoadId++;
    thumbnail.setSource(nullptr);
    thumbnailCache.clear();
    backgroundThread.signalThreadShouldExit();
    backgroundThread.stopThread(100);
}

void AudioEngine::timerCallback()
{
    bool currentlyPlaying = isPlayingAtomic.load(std::memory_order_relaxed);
    if (currentlyPlaying != lastNotifiedPlayingState)
    {
        lastNotifiedPlayingState = currentlyPlaying;
        listeners.call([currentlyPlaying](AudioEngineListener& l) {
            l.playbackStateChanged(currentlyPlaying);
        });
    }
}

void AudioEngine::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    if (sampleRate > 0.0)
    {
        engineSampleRateAtomic.store(sampleRate, std::memory_order_relaxed);
        // Pre-allocate recording buffer for 10 minutes at the current hardware sample rate
        recordingBuffer.setSize(2, static_cast<int>(sampleRate * 600.0), false, true, false);
        recordingBuffer.clear();

        delayBuffer.setSize(2, static_cast<int>(sampleRate * 2.5), false, true, false);
        delayBuffer.clear();
        delayBufferWritePos = 0;

        chorusBuffer.setSize(2, static_cast<int>(sampleRate * 0.1), false, true, false);
        chorusBuffer.clear();
        chorusBufferWritePos = 0;

        irProcessBuffer.setSize(2, samplesPerBlock > 0 ? samplesPerBlock : 512, false, true, false);
        irProcessBuffer.clear();

        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sampleRate;
        spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock > 0 ? samplesPerBlock : 512);
        spec.numChannels = 2;
        irConvolution.prepare(spec);
    }
}

void AudioEngine::releaseResources()
{
    irConvolution.reset();
}

bool AudioEngine::pushCommand(const EngineCommand& cmd)
{
    const juce::SpinLock::ScopedLockType sl(commandProducerLock);
    uint32_t currentWrite = commandWriteIdx.load(std::memory_order_relaxed);
    uint32_t nextWrite = (currentWrite + 1) % CommandQueueCapacity;
    if (nextWrite == commandReadIdx.load(std::memory_order_acquire))
    {
        return false; // Ring buffer is full
    }
    commandQueue[currentWrite] = cmd;
    commandWriteIdx.store(nextWrite, std::memory_order_release);
    return true;
}

void AudioEngine::drainCommandsOnAudioThread()
{
    uint32_t currentRead = commandReadIdx.load(std::memory_order_relaxed);
    uint32_t currentWrite = commandWriteIdx.load(std::memory_order_acquire);

    while (currentRead != currentWrite)
    {
        const auto& cmd = commandQueue[currentRead];
        switch (cmd.type)
        {
            case EngineCommandType::Play:
            {
                startPlaybackInternal(cmd.doubleVal1);
                break;
            }
            case EngineCommandType::Pause:
            {
                for (auto& slot : voicePool)
                {
                    if (slot.active && !slot.isZoneVoice && !slot.isMetronome && slot.sample != nullptr)
                    {
                        double sr = engineSampleRateAtomic.load(std::memory_order_relaxed);
                        if (sr > 0.0 && slot.ratio > 0.0)
                        {
                            stoppedPositionSecs.store((slot.readPosition / slot.ratio) / sr, std::memory_order_relaxed);
                        }
                        slot.active = false;
                    }
                }
                break;
            }
            case EngineCommandType::Stop:
            {
                stopPlaybackInternal();
                break;
            }
            case EngineCommandType::SeekRatio:
            {
                double clamped = juce::jlimit(sampleStartRatioAtomic.load(std::memory_order_relaxed),
                                              sampleEndRatioAtomic.load(std::memory_order_relaxed),
                                              cmd.doubleVal1);
                for (auto& slot : voicePool)
                {
                    if (slot.active && !slot.isZoneVoice && !slot.isMetronome && slot.sample != nullptr)
                    {
                        slot.readPosition = clamped * slot.sample->buffer.getNumSamples();
                    }
                }
                break;
            }
            case EngineCommandType::SetLooping:
            {
                for (auto& slot : voicePool)
                {
                    if (!slot.isZoneVoice && !slot.isMetronome)
                    {
                        slot.isLooping = cmd.boolVal1;
                    }
                }
                break;
            }
            case EngineCommandType::SetGain:
            {
                break;
            }
            case EngineCommandType::SetSampleRange:
            {
                for (auto& slot : voicePool)
                {
                    if (!slot.isZoneVoice && !slot.isMetronome && slot.sample != nullptr)
                    {
                        slot.startRatio = cmd.doubleVal1;
                        slot.endRatio = cmd.doubleVal2;
                        int numSamples = slot.sample->buffer.getNumSamples();
                        double startPos = slot.startRatio * numSamples;
                        double endPos = slot.endRatio * numSamples;
                        if (slot.readPosition < startPos || slot.readPosition > endPos)
                            slot.readPosition = startPos;
                    }
                }
                break;
            }
            case EngineCommandType::LoadPreviewSample:
            {
                activePreviewSampleOnAudioThread = cmd.sampleData;
                if (cmd.boolVal1)
                {
                    startPlaybackInternal(sampleStartRatioAtomic.load(std::memory_order_relaxed));
                }
                break;
            }
            case EngineCommandType::PlayZoneVoice:
            {
                if (cmd.sampleData != nullptr && cmd.sampleData->buffer.getNumSamples() > 0)
                {
                    int targetSlot = -1;
                    for (int i = 0; i < MaxActiveVoices; ++i)
                    {
                        if (!voicePool[i].active)
                        {
                            targetSlot = i;
                            break;
                        }
                    }
                    if (targetSlot == -1) targetSlot = 0;

                    auto& slot = voicePool[targetSlot];
                    slot.sample = cmd.sampleData;
                    slot.sampleStartOffset = cmd.int64Val1;
                    slot.sampleEndOffset = cmd.int64Val2;
                    slot.loopStartOffset = cmd.int64Val3;
                    slot.loopEndOffset = cmd.int64Val4;

                    int voiceLen = cmd.sampleData->buffer.getNumSamples();
                    int64_t startPos = cmd.int64Val1;
                    if (startPos < 0) startPos = 0;
                    if (startPos >= voiceLen) startPos = 0;

                    slot.readPosition = static_cast<double>(startPos);
                    slot.startRatio = 0.0;
                    slot.endRatio = 1.0;
                    slot.isLooping = cmd.boolVal1;
                    slot.active = true;
                    slot.rootNote = cmd.intVal2;
                    slot.triggerMidiNote = cmd.intVal1;
                    slot.fineTuneCents = cmd.floatVal1;
                    slot.bufferSampleRate = cmd.sampleData->sampleRate;
                    slot.isZoneVoice = true;
                    slot.isOneShot = cmd.boolVal2;
                    slot.isMetronome = false;
                    slot.groupIndex = cmd.intVal3;
                    slot.pan = cmd.floatVal7;

                    double engineSr = engineSampleRateAtomic.load(std::memory_order_relaxed);
                    if (engineSr <= 0.0) engineSr = 44100.0;

                    bool ptEnabled = pitchTrackingEnabled.load(std::memory_order_relaxed);
                    double semitoneDiff = (ptEnabled ? (slot.triggerMidiNote - slot.rootNote) : 0.0) + (slot.fineTuneCents / 100.0);
                    double srRatio = (slot.bufferSampleRate > 0.0 && engineSr > 0.0) ? (slot.bufferSampleRate / engineSr) : 1.0;
                    slot.ratio = srRatio * std::pow(2.0, semitoneDiff / 12.0);

                    slot.gain = cmd.floatVal2;

                    juce::ADSR::Parameters adsrParams;
                    adsrParams.attack = juce::jmax(0.001f, cmd.floatVal3);
                    adsrParams.decay = juce::jmax(0.001f, cmd.floatVal4);
                    adsrParams.sustain = juce::jlimit(0.0f, 1.0f, cmd.floatVal5);
                    adsrParams.release = juce::jmax(0.001f, cmd.floatVal6);
                    slot.adsr.setSampleRate(engineSr);
                    slot.adsr.setParameters(adsrParams);
                    slot.adsr.noteOn();
                }
                break;
            }
            case EngineCommandType::StopZoneVoice:
            {
                int triggerNote = cmd.intVal1;
                for (auto& slot : voicePool)
                {
                    if (slot.active && slot.isZoneVoice && slot.triggerMidiNote == triggerNote)
                    {
                        if (!slot.isOneShot)
                        {
                            slot.adsr.noteOff();
                            slot.isLooping = false;
                        }
                    }
                }
                break;
            }
            case EngineCommandType::PlayMetronome:
            {
                bool isAccent = (cmd.intVal1 != 0);
                double sr = engineSampleRateAtomic.load(std::memory_order_relaxed);
                if (sr <= 0.0) sr = 44100.0;
                int clickLen = static_cast<int>(sr * 0.015);
                if (clickLen > 0)
                {
                    int targetSlot = -1;
                    for (int i = MaxActiveVoices - 1; i >= 0; --i)
                    {
                        if (!voicePool[i].active)
                        {
                            targetSlot = i;
                            break;
                        }
                    }
                    if (targetSlot == -1) targetSlot = MaxActiveVoices - 1;

                    auto& slot = voicePool[targetSlot];
                    if (slot.sample == nullptr || slot.sample->buffer.getNumSamples() < clickLen)
                    {
                        slot.sample = std::make_shared<CachedSample>();
                        slot.sample->buffer.setSize(2, clickLen);
                    }
                    slot.sample->sampleRate = sr;
                    slot.sample->buffer.setSize(2, clickLen, false, false, true);

                    double freq = isAccent ? 1200.0 : 800.0;
                    float gain = isAccent ? 0.65f : 0.45f;
                    for (int s = 0; s < clickLen; ++s)
                    {
                        float env = std::exp(-static_cast<float>(s) / (sr * 0.003f));
                        float smp = static_cast<float>(std::sin(2.0 * juce::MathConstants<double>::pi * freq * (s / sr)) * env * gain);
                        slot.sample->buffer.setSample(0, s, smp);
                        slot.sample->buffer.setSample(1, s, smp);
                    }

                    slot.readPosition = 0.0;
                    slot.ratio = 1.0;
                    slot.isLooping = false;
                    slot.active = true;
                    slot.startRatio = 0.0;
                    slot.endRatio = 1.0;
                    slot.rootNote = 60;
                    slot.triggerMidiNote = -1;
                    slot.gain = 1.0f;
                    slot.fineTuneCents = 0.0f;
                    slot.bufferSampleRate = sr;
                    slot.isZoneVoice = false;
                    slot.isOneShot = true;
                    slot.isMetronome = true;
                    slot.adsr.reset();
                }
                break;
            }
            case EngineCommandType::UpdateVoiceRatios:
            {
                double engineSr = engineSampleRateAtomic.load(std::memory_order_relaxed);
                if (engineSr <= 0.0) engineSr = 44100.0;
                bool pitchTrack = pitchTrackingEnabled.load(std::memory_order_relaxed);

                for (auto& slot : voicePool)
                {
                    if (slot.active && slot.sample != nullptr)
                    {
                        double sr = (slot.bufferSampleRate > 0.0) ? slot.bufferSampleRate : slot.sample->sampleRate;
                        double baseRatio = (engineSr > 0.0 && sr > 0.0) ? (sr / engineSr) : 1.0;
                        double pitchOffsetSemis = (slot.triggerMidiNote - slot.rootNote) + (slot.fineTuneCents / 100.0);
                        slot.ratio = baseRatio * std::pow(2.0, pitchOffsetSemis / 12.0);
                    }
                }
                break;
            }
            default:
                break;
        }

        currentRead = (currentRead + 1) % CommandQueueCapacity;
    }

    commandReadIdx.store(currentRead, std::memory_order_release);
}

void AudioEngine::startPlaybackInternal(double startRatio)
{
    if (activePreviewSampleOnAudioThread == nullptr || activePreviewSampleOnAudioThread->buffer.getNumSamples() == 0)
        return;

    for (auto& slot : voicePool)
    {
        if (!slot.isZoneVoice && !slot.isMetronome)
        {
            slot.active = false;
        }
    }

    auto& slot = voicePool[0];
    slot.sample = activePreviewSampleOnAudioThread;
    slot.readPosition = juce::jlimit(0.0, 1.0, startRatio) * slot.sample->buffer.getNumSamples();
    
    double engineSr = engineSampleRateAtomic.load(std::memory_order_relaxed);
    if (engineSr <= 0.0) engineSr = 44100.0;
    double fileSr = slot.sample->sampleRate;
    slot.ratio = (engineSr > 0.0) ? (fileSr / engineSr) : 1.0;
    slot.isLooping = isLoopingAtomic.load(std::memory_order_relaxed);
    slot.active = true;
    slot.startRatio = sampleStartRatioAtomic.load(std::memory_order_relaxed);
    slot.endRatio = sampleEndRatioAtomic.load(std::memory_order_relaxed);
    slot.rootNote = slot.sample->rootNote;
    slot.triggerMidiNote = -1;
    slot.fineTuneCents = 0.0f;
    slot.bufferSampleRate = fileSr;
    slot.isZoneVoice = false;
    slot.isOneShot = oneShotEnabled.load(std::memory_order_relaxed);
    slot.isMetronome = false;
    slot.gain = 1.0f;

    juce::ADSR::Parameters defaultAdsr;
    defaultAdsr.attack = samplerAttackSec.load(std::memory_order_relaxed);
    defaultAdsr.decay = samplerDecaySec.load(std::memory_order_relaxed);
    defaultAdsr.sustain = samplerSustainLevel.load(std::memory_order_relaxed);
    defaultAdsr.release = samplerReleaseSec.load(std::memory_order_relaxed);
    slot.adsr.setSampleRate(engineSr);
    slot.adsr.setParameters(defaultAdsr);
    slot.adsr.noteOn();
}

void AudioEngine::stopPlaybackInternal()
{
    for (auto& slot : voicePool)
    {
        slot.active = false;
    }
    double totalLen = totalDurationAtomic.load(std::memory_order_relaxed);
    stoppedPositionSecs.store(sampleStartRatioAtomic.load(std::memory_order_relaxed) * totalLen, std::memory_order_relaxed);
}

void AudioEngine::processNextAudioBlock(juce::AudioBuffer<float>& outputBuffer, juce::MidiBuffer& midiMessages)
{
    // 1. Drain Lock-Free Commands
    drainCommandsOnAudioThread();

    // 2. Process MIDI Keyboard buffer (invokes handleNoteOn / handleNoteOff)
    keyboardState.processNextMidiBuffer(midiMessages, 0, outputBuffer.getNumSamples(), true);

    // 2b. Drain commands immediately so note on events start rendering in the current block
    drainCommandsOnAudioThread();

    // 3. Live Input Processing & Monitoring
    int inChannels = outputBuffer.getNumChannels();
    int numSamples = outputBuffer.getNumSamples();
    bool monitorInput = !inputMuted.load(std::memory_order_relaxed);

    juce::AudioBuffer<float> inputMonitorBuffer;

    if (inChannels > 0 && numSamples > 0)
    {
        if (monitorInput)
        {
            bool lowCut = eqLowCutEnabled.load(std::memory_order_relaxed);

            struct BiquadCoeffs
            {
                float b0 { 1.0f }, b1 { 0.0f }, b2 { 0.0f }, a1 { 0.0f }, a2 { 0.0f };
            };

            double engineSr = engineSampleRateAtomic.load(std::memory_order_relaxed);

            auto calcCoeffs = [fs = engineSr](int type, float f0, float gainDb, float Q) -> BiquadCoeffs {
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
                    c.a2 = (A + 1.0f) - (A - 1.0f) * cosW - beta * sinW;
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
                float f = eqFreqs[band].load(std::memory_order_relaxed);
                float g = eqGains[band].load(std::memory_order_relaxed);
                
                if (std::abs(g) > 0.05f)
                {
                    auto c = calcCoeffs(1 + band, f, g, 1.414f);
                    for (int ch = 0; ch < inChannels; ++ch)
                        processFilter(outputBuffer.getWritePointer(ch), c, inputFilterStates[1 + band][std::min(ch, 1)]);
                }
            }

            inputMonitorBuffer.makeCopyOf(outputBuffer);

            float maxL = outputBuffer.getMagnitude(0, 0, numSamples);
            float maxR = (inChannels >= 2) ? outputBuffer.getMagnitude(1, 0, numSamples) : maxL;
            liveInputLeft.store(maxL, std::memory_order_relaxed);
            liveInputRight.store(maxR, std::memory_order_relaxed);

            if (recordingActive.load(std::memory_order_relaxed))
            {
                int currentWrite = recordingWritePosition.load(std::memory_order_relaxed);
                int spaceLeft = recordingBuffer.getNumSamples() - currentWrite;
                int toWrite = std::min(numSamples, spaceLeft);

                if (toWrite > 0)
                {
                    auto mode = channelMode.load(std::memory_order_relaxed);
                    if (mode == RecordingChannelMode::MonoLeft)
                    {
                        recordingBuffer.copyFrom(0, currentWrite, outputBuffer, 0, 0, toWrite);
                        recordingBuffer.copyFrom(1, currentWrite, outputBuffer, 0, 0, toWrite);
                    }
                    else if (mode == RecordingChannelMode::MonoRight)
                    {
                        int srcCh = std::min(1, inChannels - 1);
                        recordingBuffer.copyFrom(0, currentWrite, outputBuffer, srcCh, 0, toWrite);
                        recordingBuffer.copyFrom(1, currentWrite, outputBuffer, srcCh, 0, toWrite);
                    }
                    else
                    {
                        for (int ch = 0; ch < recordingBuffer.getNumChannels(); ++ch)
                        {
                            int srcCh = std::min(ch, inChannels - 1);
                            recordingBuffer.copyFrom(ch, currentWrite, outputBuffer, srcCh, 0, toWrite);
                        }
                    }
                    recordingWritePosition.store(currentWrite + toWrite, std::memory_order_relaxed);
                }
                else
                {
                    recordingActive.store(false, std::memory_order_relaxed);
                }
            }
        }
        else
        {
            liveInputLeft.store(0.0f, std::memory_order_relaxed);
            liveInputRight.store(0.0f, std::memory_order_relaxed);
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

    // 4. Render Active Voices (Lock-Free!)
    int outChannels = outputBuffer.getNumChannels();
    float globalGain = gainLevelAtomic.load(std::memory_order_relaxed);
    bool hasActiveVoices = false;
    double latestPreviewPosSec = -1.0;

    double engineSr = engineSampleRateAtomic.load(std::memory_order_relaxed);
    if (engineSr <= 0.0) engineSr = 44100.0;

    // Calculate LFO Modulation for this block
    float lfoFreq = lfoFrequency.load(std::memory_order_relaxed);
    float baseLfoAmt = lfoAmount.load(std::memory_order_relaxed);
    float modWheel = midiModWheelAmount.load(std::memory_order_relaxed);
    float lfoAmt = (modWheel > 0.0001f) ? (baseLfoAmt > 0.0001f ? (baseLfoAmt * modWheel) : modWheel) : baseLfoAmt;
    int lfoShp = lfoShape.load(std::memory_order_relaxed);
    int lfoTgt = lfoTarget.load(std::memory_order_relaxed);
    float pitchBend = midiPitchBendSemis.load(std::memory_order_relaxed);
    float expression = midiExpressionAmount.load(std::memory_order_relaxed);

    float lfoVal = 0.0f;
    if (lfoAmt > 0.0001f && engineSr > 0.0)
    {
        double phaseInc = (static_cast<double>(lfoFreq) / engineSr) * numSamples;
        lfoPhase = std::fmod(lfoPhase + phaseInc, 1.0);
        if (lfoPhase < 0.0) lfoPhase += 1.0;

        if (lfoShp == 0) // Sine
        {
            lfoVal = static_cast<float>(std::sin(2.0 * juce::MathConstants<double>::pi * lfoPhase));
        }
        else if (lfoShp == 1) // Triangle
        {
            double p = std::fmod(lfoPhase + 0.25, 1.0);
            lfoVal = static_cast<float>(4.0 * std::abs(p - 0.5) - 1.0);
        }
        else if (lfoShp == 2) // Saw
        {
            lfoVal = static_cast<float>(2.0 * lfoPhase - 1.0);
        }
        else if (lfoShp == 3) // Square
        {
            lfoVal = (lfoPhase < 0.5) ? 1.0f : -1.0f;
        }
        else if (lfoShp == 4) // Random S&H
        {
            if (lfoPhase < lastRandomLfoPhase) // wrapped
            {
                lastRandomLfoVal = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;
            }
            lastRandomLfoPhase = lfoPhase;
            lfoVal = lastRandomLfoVal;
        }
    }

    float lfoOut = lfoVal * lfoAmt;
    currentLfoOutput.store(lfoOut, std::memory_order_relaxed);

    // LFO is stored in currentLfoOutput for control/parameter modulation. It does not corrupt audio playback directly.
    float lfoPitchOffsetSemis = 0.0f;
    float lfoVolMul = 1.0f;
    float lfoPanOffset = 0.0f;

    bool ptEnabled = pitchTrackingEnabled.load(std::memory_order_relaxed);

    for (auto& slot : voicePool)
    {
        if (!slot.active || slot.sample == nullptr)
            continue;

        int grpIdx = juce::jlimit(0, MaxGroups - 1, slot.groupIndex);
        if (slot.isZoneVoice && groupMutes[grpIdx].load(std::memory_order_relaxed))
            continue;

        const auto& voiceBuf = slot.sample->buffer;
        int voiceChannels = voiceBuf.getNumChannels();
        int voiceLength = voiceBuf.getNumSamples();
        if (voiceChannels == 0 || voiceLength == 0)
        {
            slot.active = false;
            continue;
        }

        hasActiveVoices = true;
        double pos = slot.readPosition;

        double baseRatio = (slot.ratio > 0.0) ? slot.ratio : 1.0;
        double ratio = baseRatio;
        if (slot.isZoneVoice && slot.triggerMidiNote >= 0)
        {
            float grpTune = groupTuningsCents[grpIdx].load(std::memory_order_relaxed);
            float netFineTune = slot.fineTuneCents + grpTune;
            double semitoneDiff = (ptEnabled ? (slot.triggerMidiNote - slot.rootNote) : 0.0) + pitchBend + (netFineTune / 100.0);
            double srRatio = (slot.bufferSampleRate > 0.0 && engineSr > 0.0) ? (slot.bufferSampleRate / engineSr) : 1.0;
            ratio = srRatio * std::pow(2.0, semitoneDiff / 12.0);
        }

        int startSample = 0;
        int endSample = voiceLength;

        if (slot.isZoneVoice && (slot.sampleStartOffset > 0 || slot.sampleEndOffset > 0))
        {
            startSample = static_cast<int>(juce::jlimit<int64_t>(0, voiceLength - 1, slot.sampleStartOffset));
            if (slot.sampleEndOffset > slot.sampleStartOffset && slot.sampleEndOffset <= voiceLength)
                endSample = static_cast<int>(slot.sampleEndOffset);
            else
                endSample = voiceLength;
        }
        else
        {
            startSample = static_cast<int>(slot.startRatio * voiceLength);
            endSample = static_cast<int>(slot.endRatio * voiceLength);
            if (endSample <= startSample) endSample = voiceLength;
        }

        if (slot.isLooping)
        {
            if (slot.loopEndOffset > slot.loopStartOffset && slot.loopEndOffset <= voiceLength)
            {
                startSample = static_cast<int>(slot.loopStartOffset);
                endSample = static_cast<int>(slot.loopEndOffset);
            }
        }

        int loopLen = endSample - startSample;

        // Smooth crossfade length (up to 50ms) to ensure seamless, click-free loop wrap-around
        int xfadeLen = 0;
        if (slot.isLooping && loopLen >= 64)
        {
            int desiredXfade = static_cast<int>(0.050 * engineSr); // 50ms
            xfadeLen = std::min(desiredXfade, loopLen / 4);
        }

        if (pos < startSample) pos = startSample;

        bool hasAdsr = !slot.isMetronome;

        float grpGainDb = slot.isZoneVoice ? groupVolumesDb[grpIdx].load(std::memory_order_relaxed) : 0.0f;
        float grpGainMul = std::pow(10.0f, grpGainDb / 20.0f);

        float grpPan = slot.isZoneVoice ? groupPans[grpIdx].load(std::memory_order_relaxed) : 0.0f;
        float netPan = juce::jlimit(-1.0f, 1.0f, slot.pan + grpPan);
        float panAngle = (netPan + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
        float leftPanGain = std::cos(panAngle);
        float rightPanGain = std::sin(panAngle);

        auto getInterpolatedSample = [&](int ch, double readPos) -> float {
            if (readPos < 0.0) readPos = 0.0;
            if (readPos >= voiceLength - 1)
                return voiceBuf.getSample(ch, voiceLength - 1);
            int i1 = static_cast<int>(readPos);
            int i2 = std::min(i1 + 1, voiceLength - 1);
            float f = static_cast<float>(readPos - i1);
            float v1 = voiceBuf.getSample(ch, i1);
            float v2 = voiceBuf.getSample(ch, i2);
            return v1 + f * (v2 - v1);
        };

        for (int i = 0; i < numSamples; ++i)
        {
            float envVal = hasAdsr ? slot.adsr.getNextSample() : 1.0f;
            float voiceVol = globalGain * slot.gain * envVal * grpGainMul * expression * lfoVolMul;
            bool adsrActive = hasAdsr ? slot.adsr.isActive() : true;

            if (pos >= endSample || !adsrActive)
            {
                if (slot.isLooping && loopLen > 0 && adsrActive)
                {
                    double overshoot = pos - endSample;
                    pos = startSample + xfadeLen + overshoot;
                    if (pos >= endSample) pos = startSample;
                }
                else
                {
                    slot.active = false;
                    break;
                }
            }

            double curPos = pos;
            if (curPos >= voiceLength) curPos = voiceLength - 1;

            if (slot.isLooping && xfadeLen > 0 && curPos >= (endSample - xfadeLen))
            {
                double delta = curPos - (endSample - xfadeLen);
                double t = juce::jlimit(0.0, 1.0, delta / static_cast<double>(xfadeLen));
                float theta = static_cast<float>(t * juce::MathConstants<double>::halfPi);
                float wOut = std::cos(theta);
                float wIn = std::sin(theta);

                double inPos = startSample + delta;
                if (inPos >= voiceLength) inPos = voiceLength - 1;

                if (outChannels >= 2)
                {
                    float sOutL = getInterpolatedSample(0, curPos);
                    float sInL = getInterpolatedSample(0, inPos);
                    float sampleValL = (sOutL * wOut + sInL * wIn) * voiceVol * leftPanGain;
                    outputBuffer.addSample(0, i, sampleValL);

                    int rightSrcCh = std::min(1, voiceChannels - 1);
                    float sOutR = getInterpolatedSample(rightSrcCh, curPos);
                    float sInR = getInterpolatedSample(rightSrcCh, inPos);
                    float sampleValR = (sOutR * wOut + sInR * wIn) * voiceVol * rightPanGain;
                    outputBuffer.addSample(1, i, sampleValR);
                }
                else if (outChannels == 1)
                {
                    float sOut = getInterpolatedSample(0, curPos);
                    float sIn = getInterpolatedSample(0, inPos);
                    float sampleVal = (sOut * wOut + sIn * wIn) * voiceVol;
                    outputBuffer.addSample(0, i, sampleVal);
                }
            }
            else
            {
                if (outChannels >= 2)
                {
                    float sampleValL = getInterpolatedSample(0, curPos) * voiceVol * leftPanGain;
                    outputBuffer.addSample(0, i, sampleValL);

                    int rightSrcCh = std::min(1, voiceChannels - 1);
                    float sampleValR = getInterpolatedSample(rightSrcCh, curPos) * voiceVol * rightPanGain;
                    outputBuffer.addSample(1, i, sampleValR);
                }
                else if (outChannels == 1)
                {
                    float sampleVal = getInterpolatedSample(0, curPos) * voiceVol;
                    outputBuffer.addSample(0, i, sampleVal);
                }
            }

            pos += ratio;
        }

        slot.readPosition = pos;

        if (!slot.isMetronome && slot.active)
        {
            if (engineSr > 0.0 && ratio > 0.0)
            {
                latestPreviewPosSec = (slot.readPosition / ratio) / engineSr;
            }
        }
    }

    // 5. Update Real-Time Telemetry Atomics
    isPlayingAtomic.store(hasActiveVoices, std::memory_order_relaxed);
    if (latestPreviewPosSec >= 0.0)
    {
        currentPositionAtomic.store(latestPreviewPosSec, std::memory_order_relaxed);
    }
    else if (!hasActiveVoices)
    {
        currentPositionAtomic.store(stoppedPositionSecs.load(std::memory_order_relaxed), std::memory_order_relaxed);
    }

    // 6. Process Sampler Lowpass & Highpass Filters
    if (hasActiveVoices && outputBuffer.getNumSamples() > 0 && engineSr > 1000.0)
    {
        int numChans = std::min(2, outputBuffer.getNumChannels());

        float lpFreq = samplerLowpassCutoff.load(std::memory_order_relaxed);
        if (lfoTgt == 3 && std::abs(lfoOut) > 0.0001f)
        {
            lpFreq = juce::jlimit(20.0f, 20000.0f, lpFreq * std::pow(2.0f, lfoOut * 3.0f));
        }

        if (lpFreq < 20000.0f)
        {
            float clampedLp = juce::jlimit(20.0f, static_cast<float>(engineSr * 0.48), lpFreq);
            float omega = 2.0f * juce::MathConstants<float>::pi * clampedLp / static_cast<float>(engineSr);
            float cosW = std::cos(omega);
            float sinW = std::sin(omega);
            float alpha = sinW / (2.0f * 0.7071f);
            float a0 = 1.0f + alpha;
            float b0 = ((1.0f - cosW) * 0.5f) / a0;
            float b1 = (1.0f - cosW) / a0;
            float b2 = ((1.0f - cosW) * 0.5f) / a0;
            float a1 = (-2.0f * cosW) / a0;
            float a2 = (1.0f - alpha) / a0;

            for (int ch = 0; ch < numChans; ++ch)
            {
                float* d = outputBuffer.getWritePointer(ch);
                float x1 = samplerLpState[ch].x1;
                float x2 = samplerLpState[ch].x2;
                float y1 = samplerLpState[ch].y1;
                float y2 = samplerLpState[ch].y2;

                for (int s = 0; s < numSamples; ++s)
                {
                    float in = d[s];
                    float out = b0 * in + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
                    if (std::abs(out) < 1e-15f) out = 0.0f;
                    x2 = x1; x1 = in;
                    y2 = y1; y1 = out;
                    d[s] = out;
                }
                samplerLpState[ch].x1 = x1;
                samplerLpState[ch].x2 = x2;
                samplerLpState[ch].y1 = y1;
                samplerLpState[ch].y2 = y2;
            }
        }

        float hpFreq = samplerHighpassCutoff.load(std::memory_order_relaxed);
        if (hpFreq > 25.0f)
        {
            float clampedHp = juce::jlimit(10.0f, static_cast<float>(engineSr * 0.48), hpFreq);
            float omega = 2.0f * juce::MathConstants<float>::pi * clampedHp / static_cast<float>(engineSr);
            float cosW = std::cos(omega);
            float sinW = std::sin(omega);
            float alpha = sinW / (2.0f * 0.7071f);
            float a0 = 1.0f + alpha;
            float b0 = ((1.0f + cosW) * 0.5f) / a0;
            float b1 = (-(1.0f + cosW)) / a0;
            float b2 = ((1.0f + cosW) * 0.5f) / a0;
            float a1 = (-2.0f * cosW) / a0;
            float a2 = (1.0f - alpha) / a0;

            for (int ch = 0; ch < numChans; ++ch)
            {
                float* d = outputBuffer.getWritePointer(ch);
                float x1 = samplerHpState[ch].x1;
                float x2 = samplerHpState[ch].x2;
                float y1 = samplerHpState[ch].y1;
                float y2 = samplerHpState[ch].y2;

                for (int s = 0; s < numSamples; ++s)
                {
                    float in = d[s];
                    float out = b0 * in + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
                    if (std::abs(out) < 1e-15f) out = 0.0f;
                    x2 = x1; x1 = in;
                    y2 = y1; y1 = out;
                    d[s] = out;
                }
                samplerHpState[ch].x1 = x1;
                samplerHpState[ch].x2 = x2;
                samplerHpState[ch].y1 = y1;
                samplerHpState[ch].y2 = y2;
            }
        }
    }

    // Manage effects tail: keep effects processing for ~4s after voices stop
    if (hasActiveVoices)
    {
        reverbTailSamplesRemaining = static_cast<int>(engineSr * 4.0);
    }
    bool processEffectsTail = hasActiveVoices || reverbTailSamplesRemaining > 0;
    if (!hasActiveVoices && reverbTailSamplesRemaining > 0)
    {
        reverbTailSamplesRemaining -= numSamples;
    }

    // 7. Process Chorus Effect
    float chWet = samplerChorusWet.load(std::memory_order_relaxed);
    float chDepth = samplerChorusDepth.load(std::memory_order_relaxed);
    if (chWet > 0.001f && processEffectsTail && chorusBuffer.getNumSamples() > 500)
    {
        float chRate = samplerChorusRate.load(std::memory_order_relaxed);
        int cLen = chorusBuffer.getNumSamples();
        double phaseInc = (static_cast<double>(chRate) / engineSr);
        int numChans = std::min(2, outputBuffer.getNumChannels());
        float effDepth = (chDepth > 0.001f) ? chDepth : 0.5f;

        for (int s = 0; s < numSamples; ++s)
        {
            chorusPhase = std::fmod(chorusPhase + phaseInc, 1.0);
            if (chorusPhase < 0.0) chorusPhase += 1.0;

            // Stereo modulation: Left uses sine, Right uses cosine (90 deg quadrature phase offset)
            float modL = static_cast<float>(std::sin(2.0 * juce::MathConstants<double>::pi * chorusPhase));
            float modR = static_cast<float>(std::cos(2.0 * juce::MathConstants<double>::pi * chorusPhase));

            // Delay time between ~12ms and ~28ms
            float delaySecL = 0.018f + 0.007f * modL * effDepth;
            float delaySecR = 0.022f + 0.007f * modR * effDepth;

            float delaySamplesL = delaySecL * static_cast<float>(engineSr);
            float delaySamplesR = delaySecR * static_cast<float>(engineSr);

            // Write dry input samples to chorus ring buffer first
            for (int ch = 0; ch < numChans; ++ch)
            {
                chorusBuffer.setSample(ch, chorusBufferWritePos, outputBuffer.getSample(ch, s));
            }
            if (numChans == 1 && chorusBuffer.getNumChannels() >= 2)
            {
                chorusBuffer.setSample(1, chorusBufferWritePos, outputBuffer.getSample(0, s));
            }

            // Read interpolated samples from chorus buffer
            auto readInterpolated = [&](int ch, float delaySmp) -> float {
                int srcCh = std::min(ch, chorusBuffer.getNumChannels() - 1);
                float rPos = static_cast<float>(chorusBufferWritePos) - delaySmp;
                while (rPos < 0.0f) rPos += static_cast<float>(cLen);
                while (rPos >= static_cast<float>(cLen)) rPos -= static_cast<float>(cLen);

                int idx0 = static_cast<int>(rPos);
                int idx1 = (idx0 + 1) % cLen;
                float frac = rPos - static_cast<float>(idx0);

                float s0 = chorusBuffer.getSample(srcCh, idx0);
                float s1 = chorusBuffer.getSample(srcCh, idx1);
                return s0 + frac * (s1 - s0);
            };

            float chSampleL = readInterpolated(0, delaySamplesL);
            float chSampleR = readInterpolated(1, delaySamplesR);

            float* outL = outputBuffer.getWritePointer(0);
            outL[s] = outL[s] * (1.0f - chWet * 0.4f) + chSampleL * chWet;

            if (numChans >= 2)
            {
                float* outR = outputBuffer.getWritePointer(1);
                outR[s] = outR[s] * (1.0f - chWet * 0.4f) + chSampleR * chWet;
            }

            chorusBufferWritePos = (chorusBufferWritePos + 1) % cLen;
        }
    }

    // 8. Process Stereo Delay Line
    float delWet = samplerDelayWetLevel.load(std::memory_order_relaxed);
    float delFb = samplerDelayFeedback.load(std::memory_order_relaxed);
    float delTime = samplerDelayTimeMs.load(std::memory_order_relaxed);
    if ((delWet > 0.001f || delFb > 0.001f) && processEffectsTail && delayBuffer.getNumSamples() > 1000)
    {
        int dLen = delayBuffer.getNumSamples();
        int delSamples = juce::jlimit(1, dLen - 1, static_cast<int>((delTime / 1000.0f) * engineSr));
        int numChans = std::min(2, outputBuffer.getNumChannels());

        for (int s = 0; s < numSamples; ++s)
        {
            int readIdx = (delayBufferWritePos - delSamples + dLen) % dLen;

            for (int ch = 0; ch < numChans; ++ch)
            {
                float inSmp = outputBuffer.getSample(ch, s);
                float delayedSmp = delayBuffer.getSample(ch, readIdx);

                // Soft clip / tame feedback to prevent explosion
                float nextBufferVal = inSmp + std::tanh(delayedSmp * delFb);
                delayBuffer.setSample(ch, delayBufferWritePos, nextBufferVal);

                if (delWet > 0.001f)
                {
                    float outSmp = inSmp * (1.0f - delWet * 0.25f) + delayedSmp * delWet;
                    outputBuffer.setSample(ch, s, outSmp);
                }
            }

            delayBufferWritePos = (delayBufferWritePos + 1) % dLen;
        }
    }

    // 9. Process Algorithmic Reverb DSP on Sampler output
    float revAmount = samplerReverbAmount.load(std::memory_order_relaxed);
    if (revAmount > 0.001f && outputBuffer.getNumChannels() >= 2 && processEffectsTail)
    {
        reverbParams.roomSize = 0.4f + revAmount * 0.5f;
        reverbParams.damping = 0.5f;
        reverbParams.wetLevel = revAmount * 0.5f;
        reverbParams.dryLevel = 1.0f - (revAmount * 0.2f);
        reverbParams.width = 1.0f;
        reverbDSP.setParameters(reverbParams);
        reverbDSP.processStereo(outputBuffer.getWritePointer(0), outputBuffer.getWritePointer(1), outputBuffer.getNumSamples());
    }

    // 10. Process IR Convolution Reverb
    float irWet = irReverbAmount.load(std::memory_order_relaxed);
    if (irWet > 0.001f && irLoaded.load(std::memory_order_relaxed) && outputBuffer.getNumChannels() >= 1 && processEffectsTail)
    {
        int irBufSamples = irProcessBuffer.getNumSamples();
        int samplesToProcess = std::min(numSamples, irBufSamples);
        if (samplesToProcess <= 0) samplesToProcess = numSamples;

        float irDry = irReverbDryLevel.load(std::memory_order_relaxed);
        for (int ch = 0; ch < 2; ++ch)
        {
            int srcCh = std::min(ch, outputBuffer.getNumChannels() - 1);
            if (ch < irProcessBuffer.getNumChannels())
                irProcessBuffer.copyFrom(ch, 0, outputBuffer, srcCh, 0, samplesToProcess);
        }

        juce::dsp::AudioBlock<float> block(irProcessBuffer.getArrayOfWritePointers(),
                                           std::min(2, irProcessBuffer.getNumChannels()), 0,
                                           static_cast<size_t>(samplesToProcess));
        juce::dsp::ProcessContextReplacing<float> context(block);
        irConvolution.process(context);

        for (int ch = 0; ch < outputBuffer.getNumChannels(); ++ch)
        {
            float* mainData = outputBuffer.getWritePointer(ch);
            const float* irData = irProcessBuffer.getReadPointer(std::min(ch, irProcessBuffer.getNumChannels() - 1));
            for (int s = 0; s < samplesToProcess; ++s)
            {
                mainData[s] = mainData[s] * irDry + irData[s] * irWet;
            }
        }
    }
}

void AudioEngine::resetAllGroups()
{
    for (int i = 0; i < MaxGroups; ++i)
    {
        groupVolumesDb[i].store(0.0f, std::memory_order_relaxed);
        groupPans[i].store(0.0f, std::memory_order_relaxed);
        groupTuningsCents[i].store(0.0f, std::memory_order_relaxed);
        groupMutes[i].store(false, std::memory_order_relaxed);
    }
}

void AudioEngine::setGroupVolumeDb(int groupIdx, float gainDb)
{
    if (groupIdx >= 0 && groupIdx < MaxGroups)
        groupVolumesDb[groupIdx].store(gainDb, std::memory_order_relaxed);
}

float AudioEngine::getGroupVolumeDb(int groupIdx) const
{
    if (groupIdx >= 0 && groupIdx < MaxGroups)
        return groupVolumesDb[groupIdx].load(std::memory_order_relaxed);
    return 0.0f;
}

void AudioEngine::setGroupPan(int groupIdx, float pan)
{
    if (groupIdx >= 0 && groupIdx < MaxGroups)
        groupPans[groupIdx].store(juce::jlimit(-1.0f, 1.0f, pan), std::memory_order_relaxed);
}

float AudioEngine::getGroupPan(int groupIdx) const
{
    if (groupIdx >= 0 && groupIdx < MaxGroups)
        return groupPans[groupIdx].load(std::memory_order_relaxed);
    return 0.0f;
}

void AudioEngine::setGroupTuningCents(int groupIdx, float cents)
{
    if (groupIdx >= 0 && groupIdx < MaxGroups)
        groupTuningsCents[groupIdx].store(cents, std::memory_order_relaxed);
}

float AudioEngine::getGroupTuningCents(int groupIdx) const
{
    if (groupIdx >= 0 && groupIdx < MaxGroups)
        return groupTuningsCents[groupIdx].load(std::memory_order_relaxed);
    return 0.0f;
}

void AudioEngine::setGroupMuted(int groupIdx, bool muted)
{
    if (groupIdx >= 0 && groupIdx < MaxGroups)
        groupMutes[groupIdx].store(muted, std::memory_order_relaxed);
}

bool AudioEngine::getGroupMuted(int groupIdx) const
{
    if (groupIdx >= 0 && groupIdx < MaxGroups)
        return groupMutes[groupIdx].load(std::memory_order_relaxed);
    return false;
}

void AudioEngine::setLfoShapeByName(const juce::String& shape)
{
    if (shape.containsIgnoreCase("triangle") || shape.containsIgnoreCase("tri")) setLfoShape(1);
    else if (shape.containsIgnoreCase("saw") || shape.containsIgnoreCase("ramp")) setLfoShape(2);
    else if (shape.containsIgnoreCase("square") || shape.containsIgnoreCase("pulse")) setLfoShape(3);
    else if (shape.containsIgnoreCase("random") || shape.containsIgnoreCase("noise") || shape.containsIgnoreCase("sh")) setLfoShape(4);
    else setLfoShape(0);
}

void AudioEngine::setLfoTargetByName(const juce::String& target)
{
    if (target.containsIgnoreCase("volume") || target.containsIgnoreCase("amp") || target.containsIgnoreCase("tremolo")) setLfoTarget(1);
    else if (target.containsIgnoreCase("pan")) setLfoTarget(2);
    else if (target.containsIgnoreCase("cutoff") || target.containsIgnoreCase("filter") || target.containsIgnoreCase("freq")) setLfoTarget(3);
    else setLfoTarget(0);
}

void AudioEngine::loadImpulseResponseFile(const juce::File& irFile)
{
    if (!irFile.existsAsFile())
        return;

    juce::String path = irFile.getFullPathName();
    if (path == currentLoadedIrPath && irLoaded.load(std::memory_order_relaxed))
        return;

    currentLoadedIrPath = path;

    irConvolution.loadImpulseResponse(irFile,
                                     juce::dsp::Convolution::Stereo::yes,
                                     juce::dsp::Convolution::Trim::yes,
                                     0,
                                     juce::dsp::Convolution::Normalise::yes);
    irLoaded.store(true, std::memory_order_relaxed);
}

void AudioEngine::preloadSampleFiles(const std::vector<juce::File>& files)
{
    std::vector<juce::File> uniqueFiles;
    for (const auto& file : files)
    {
        if (!file.existsAsFile()) continue;
        auto path = file.getFullPathName();
        if (std::find_if(uniqueFiles.begin(), uniqueFiles.end(), [&](const juce::File& u) {
            return u.getFullPathName() == path;
        }) == uniqueFiles.end())
        {
            uniqueFiles.push_back(file);
        }
    }

    if (uniqueFiles.empty()) return;

    std::thread([this, uniqueFiles]() {
        for (const auto& file : uniqueFiles)
        {
            juce::String filePath = file.getFullPathName();

            {
                const juce::ScopedLock sl(cacheLock);
                auto it = sampleCache.find(filePath);
                if (it != sampleCache.end() && it->second != nullptr && it->second->buffer.getNumSamples() > 0)
                {
                    continue;
                }
            }

            std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
            if (reader != nullptr && reader->lengthInSamples > 0 && reader->numChannels > 0)
            {
                auto cached = std::make_shared<CachedSample>();
                cached->sampleRate = (reader->sampleRate > 0.0) ? reader->sampleRate : 44100.0;
                cached->rootNote = getRootNoteFromWavSmplHeader(file);
                if (cached->rootNote < 0 || cached->rootNote > 127) cached->rootNote = 60;
                cached->buffer.setSize(static_cast<int>(reader->numChannels), static_cast<int>(reader->lengthInSamples));
                reader->read(&cached->buffer, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);

                const juce::ScopedLock sl(cacheLock);
                sampleCache[filePath] = cached;
            }
        }
    }).detach();
}

void AudioEngine::putSampleInCache(const juce::String& filePath, double sampleRate, const juce::AudioBuffer<float>& buf)
{
    const juce::ScopedLock sl(cacheLock);
    auto cached = std::make_shared<CachedSample>();
    cached->sampleRate = sampleRate;
    cached->buffer.makeCopyOf(buf);
    cached->rootNote = 60;
    sampleCache[filePath] = cached;
}

bool AudioEngine::getCachedSampleCopy(const juce::String& filePath, juce::AudioBuffer<float>& destBuffer, double& sampleRate) const
{
    const juce::ScopedLock sl(cacheLock);
    auto it = sampleCache.find(filePath);
    if (it != sampleCache.end() && it->second != nullptr && it->second->buffer.getNumSamples() > 0)
    {
        destBuffer.makeCopyOf(it->second->buffer);
        sampleRate = it->second->sampleRate;
        return true;
    }
    return false;
}

void AudioEngine::playZoneVoice(const juce::File& file, int triggerMidiNote, int rootNote, float fineTuneCents, float gainDb, float velocity,
                                float attackSec, float decaySec, float sustainLevel, float releaseSec, bool isOneShot, bool isLooping,
                                int groupIndex, float pan,
                                int64_t sampleStart, int64_t sampleEnd, int64_t loopStart, int64_t loopEnd)
{
    juce::String filePath = file.getFullPathName();
    if (filePath.isEmpty()) return;

    std::shared_ptr<CachedSample> cached;

    {
        const juce::ScopedLock sl(cacheLock);
        auto it = sampleCache.find(filePath);
        if (it != sampleCache.end())
        {
            cached = it->second;
        }
    }

    if (cached == nullptr || cached->buffer.getNumSamples() == 0)
    {
        if (!file.existsAsFile()) return;

        std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
        if (reader == nullptr || reader->lengthInSamples <= 0 || reader->numChannels <= 0) return;

        cached = std::make_shared<CachedSample>();
        cached->sampleRate = (reader->sampleRate > 0.0) ? reader->sampleRate : 44100.0;
        cached->rootNote = rootNote;
        cached->buffer.setSize(static_cast<int>(reader->numChannels), static_cast<int>(reader->lengthInSamples));
        reader->read(&cached->buffer, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);

        const juce::ScopedLock sl(cacheLock);
        sampleCache[filePath] = cached;
    }

    if (cached == nullptr || cached->buffer.getNumSamples() == 0) return;

    EngineCommand cmd;
    cmd.type = EngineCommandType::PlayZoneVoice;
    cmd.sampleData = cached;
    cmd.intVal1 = triggerMidiNote;
    cmd.intVal2 = rootNote;
    cmd.intVal3 = groupIndex;
    cmd.floatVal1 = fineTuneCents;
    cmd.floatVal2 = velocity * std::pow(10.0f, gainDb / 20.0f);
    cmd.floatVal3 = attackSec;
    cmd.floatVal4 = decaySec;
    cmd.floatVal5 = sustainLevel;
    cmd.floatVal6 = releaseSec;
    cmd.floatVal7 = pan;
    cmd.int64Val1 = sampleStart;
    cmd.int64Val2 = sampleEnd;
    cmd.int64Val3 = loopStart;
    cmd.int64Val4 = loopEnd;
    cmd.boolVal1 = isLooping;
    cmd.boolVal2 = isOneShot || oneShotEnabled.load(std::memory_order_relaxed);
    pushCommand(cmd);

    juce::MessageManager::callAsync([this, triggerMidiNote]() {
        int sliceIdx = (triggerMidiNote >= 36 && triggerMidiNote < 100) ? (triggerMidiNote - 36) : -1;
        listeners.call([sliceIdx](AudioEngineListener& l) {
            l.activeSliceTriggered(sliceIdx);
        });
    });
}

void AudioEngine::stopZoneVoice(int triggerMidiNote)
{
    EngineCommand cmd;
    cmd.type = EngineCommandType::StopZoneVoice;
    cmd.intVal1 = triggerMidiNote;
    pushCommand(cmd);
}

void AudioEngine::stopAllVoices()
{
    stop();
    for (int n = 0; n < 128; ++n)
    {
        stopZoneVoice(n);
    }
}

void AudioEngine::triggerNoteOn(int midiNoteNumber, float /*velocity*/)
{
    if (!midiInputEnabled.load(std::memory_order_relaxed))
        return;

    std::shared_ptr<CachedSample> master;
    {
        const juce::ScopedLock sl(uiDataLock);
        master = currentMasterSample;
    }

    if (master == nullptr || master->buffer.getNumSamples() == 0)
        return;

    EngineCommand cmd;
    cmd.type = EngineCommandType::PlayZoneVoice;
    cmd.sampleData = master;
    cmd.intVal1 = midiNoteNumber;
    cmd.intVal2 = master->rootNote;
    cmd.floatVal1 = 0.0f;
    cmd.floatVal2 = 1.0f;
    cmd.floatVal3 = 0.005f;
    cmd.floatVal4 = 0.1f;
    cmd.floatVal5 = 1.0f;
    cmd.floatVal6 = 0.05f;
    cmd.boolVal1 = isLoopingAtomic.load(std::memory_order_relaxed);
    cmd.boolVal2 = oneShotEnabled.load(std::memory_order_relaxed);
    pushCommand(cmd);

    juce::MessageManager::callAsync([this] {
        listeners.call([](AudioEngineListener& l) { l.playbackStateChanged(true); });
    });
}

void AudioEngine::triggerNoteOff(int midiNoteNumber)
{
    EngineCommand cmd;
    cmd.type = EngineCommandType::StopZoneVoice;
    cmd.intVal1 = midiNoteNumber;
    pushCommand(cmd);
}

bool AudioEngine::loadFile(const juce::File& audioFile, bool autoPlay, bool isSamplerSample)
{
    isLoadedInSampler.store(isSamplerSample, std::memory_order_relaxed);
    currentFile = audioFile;
    auto loadId = ++currentLoadId;

    sampleStartRatioAtomic.store(0.0, std::memory_order_relaxed);
    sampleEndRatioAtomic.store(1.0, std::memory_order_relaxed);

    std::thread([this, audioFile, autoPlay, loadId]() {
        if (loadId != currentLoadId.load(std::memory_order_relaxed))
            return;

        juce::String filePath = audioFile.getFullPathName();
        std::shared_ptr<CachedSample> sampleData;
        double fileSampleRate = 44100.0;
        int numSamples = 0;
        int numChannels = 0;

        {
            const juce::ScopedLock sl(cacheLock);
            auto it = sampleCache.find(filePath);
            if (it != sampleCache.end() && it->second != nullptr && it->second->buffer.getNumSamples() > 0)
            {
                sampleData = it->second;
                fileSampleRate = sampleData->sampleRate;
                numSamples = sampleData->buffer.getNumSamples();
                numChannels = sampleData->buffer.getNumChannels();
            }
        }

        if (sampleData == nullptr)
        {
            std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(audioFile));
            if (reader == nullptr || loadId != currentLoadId.load(std::memory_order_relaxed))
                return;

            fileSampleRate = (reader->sampleRate > 0.0) ? reader->sampleRate : 44100.0;
            int64_t numSamples64 = reader->lengthInSamples;
            numChannels = static_cast<int>(reader->numChannels);

            if (numSamples64 <= 0 || numSamples64 > 0x7FFFFFFF || numChannels <= 0)
                return;

            numSamples = static_cast<int>(numSamples64);
            int rootNoteVal = getRootNoteFromWavSmplHeader(audioFile);
            if (rootNoteVal < 0 || rootNoteVal > 127) rootNoteVal = 60;

            if (loadId != currentLoadId.load(std::memory_order_relaxed))
                return;

            sampleData = std::make_shared<CachedSample>();
            sampleData->sampleRate = fileSampleRate;
            sampleData->rootNote = rootNoteVal;
            sampleData->buffer.setSize(numChannels, numSamples);
            reader->read(&sampleData->buffer, 0, numSamples, 0, true, true);

            // Auto-normalize if loaded audio peaks exceed 1.0f
            float loadMaxPeak = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto range = sampleData->buffer.findMinMax(ch, 0, numSamples);
                float peak = std::max(std::abs(range.getStart()), std::abs(range.getEnd()));
                if (peak > loadMaxPeak)
                    loadMaxPeak = peak;
            }
            if (loadMaxPeak > 1.0f)
            {
                sampleData->buffer.applyGain(1.0f / loadMaxPeak);
            }

            {
                const juce::ScopedLock sl(cacheLock);
                if (sampleCache.size() > 200) sampleCache.clear();
                sampleCache[filePath] = sampleData;
            }
        }

        // Precompute waveform peaks in background
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

            auto rL = sampleData->buffer.findMinMax(0, sStart, numRead);
            peaks.minLeft[p] = rL.getStart();
            peaks.maxLeft[p] = rL.getEnd();

            if (numChannels >= 2)
            {
                auto rR = sampleData->buffer.findMinMax(1, sStart, numRead);
                peaks.minRight[p] = rR.getStart();
                peaks.maxRight[p] = rR.getEnd();
            }
            else
            {
                peaks.minRight[p] = peaks.minLeft[p];
                peaks.maxRight[p] = peaks.maxLeft[p];
            }
        }

        if (loadId != currentLoadId.load(std::memory_order_relaxed))
            return;

        juce::MessageManager::callAsync([this, audioFile, autoPlay, loadId, sampleData, fileSampleRate, peaks = std::move(peaks)]() mutable {
            if (loadId != currentLoadId.load(std::memory_order_relaxed))
                return;

            thumbnail.setSource(new juce::FileInputSource(audioFile));

            {
                const juce::ScopedLock sl(uiDataLock);
                currentMasterSample = sampleData;
                cachedWaveformPeaks = std::move(peaks);
                currentFileSampleRateAtomic.store(fileSampleRate, std::memory_order_relaxed);
                numChannelsAtomic.store(sampleData->buffer.getNumChannels(), std::memory_order_relaxed);

                double engineSr = engineSampleRateAtomic.load(std::memory_order_relaxed);
                if (engineSr <= 0.0) engineSr = 44100.0;
                double ratio = (engineSr > 0.0) ? (fileSampleRate / engineSr) : 1.0;
                double totalLen = (sampleData->buffer.getNumSamples() / ratio) / engineSr;
                totalDurationAtomic.store(totalLen, std::memory_order_relaxed);
                stoppedPositionSecs.store(0.0, std::memory_order_relaxed);

                loadedVoice = std::make_shared<AudioVoice>();
                loadedVoice->buffer.makeCopyOf(sampleData->buffer);
                loadedVoice->ratio = ratio;
                loadedVoice->rootNote = sampleData->rootNote;
                loadedVoice->bufferSampleRate = fileSampleRate;

                originalEditBuffer.makeCopyOf(sampleData->buffer);
                originalEditSampleRate = fileSampleRate;
                originalSnapshotValid = true;
            }

            EngineCommand cmd;
            cmd.type = EngineCommandType::LoadPreviewSample;
            cmd.sampleData = sampleData;
            cmd.boolVal1 = autoPlay;
            pushCommand(cmd);

            listeners.call([filePath = audioFile.getFullPathName(), autoPlay](AudioEngineListener& l) {
                l.sampleLoaded(filePath);
                l.playbackStateChanged(autoPlay);
            });
        });
    }).detach();

    return true;
}

bool AudioEngine::loadAudioBuffer(const juce::String& displayName, const juce::AudioBuffer<float>& buffer, double sampleRate, int rootNote, bool isSamplerSample)
{
    if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0)
        return false;

    stop();

    if (sampleRate <= 0.0)
        sampleRate = 44100.0;

    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();

    auto sampleData = std::make_shared<CachedSample>();
    sampleData->sampleRate = sampleRate;
    sampleData->rootNote = rootNote;
    sampleData->buffer.makeCopyOf(buffer);

    WaveformPeaks peaks;
    peaks.numChannels = numChannels;
    peaks.numPoints = std::min(1024, numSamples);
    peaks.minLeft.resize(static_cast<size_t>(peaks.numPoints));
    peaks.maxLeft.resize(static_cast<size_t>(peaks.numPoints));
    peaks.minRight.resize(static_cast<size_t>(peaks.numPoints));
    peaks.maxRight.resize(static_cast<size_t>(peaks.numPoints));

    int blockSize = numSamples / peaks.numPoints;
    if (blockSize <= 0) blockSize = 1;

    for (int p = 0; p < peaks.numPoints; ++p)
    {
        int sStart = p * blockSize;
        int numRead = std::min(blockSize, numSamples - sStart);
        if (numRead <= 0) break;

        auto rL = sampleData->buffer.findMinMax(0, sStart, numRead);
        peaks.minLeft[static_cast<size_t>(p)] = rL.getStart();
        peaks.maxLeft[static_cast<size_t>(p)] = rL.getEnd();

        if (numChannels >= 2)
        {
            auto rR = sampleData->buffer.findMinMax(1, sStart, numRead);
            peaks.minRight[static_cast<size_t>(p)] = rR.getStart();
            peaks.maxRight[static_cast<size_t>(p)] = rR.getEnd();
        }
        else
        {
            peaks.minRight[static_cast<size_t>(p)] = peaks.minLeft[static_cast<size_t>(p)];
            peaks.maxRight[static_cast<size_t>(p)] = peaks.maxLeft[static_cast<size_t>(p)];
        }
    }

    double engineSr = engineSampleRateAtomic.load(std::memory_order_relaxed);
    if (engineSr <= 0.0) engineSr = 44100.0;
    double ratio = (engineSr > 0.0) ? (sampleRate / engineSr) : 1.0;
    double totalLen = (numSamples / ratio) / engineSr;

    {
        const juce::ScopedLock sl(uiDataLock);
        currentMasterSample = sampleData;
        cachedWaveformPeaks = std::move(peaks);
        currentFileSampleRateAtomic.store(sampleRate, std::memory_order_relaxed);
        numChannelsAtomic.store(numChannels, std::memory_order_relaxed);
        totalDurationAtomic.store(totalLen, std::memory_order_relaxed);
        stoppedPositionSecs.store(0.0, std::memory_order_relaxed);
        sampleStartRatioAtomic.store(0.0, std::memory_order_relaxed);
        sampleEndRatioAtomic.store(1.0, std::memory_order_relaxed);

        loadedVoice = std::make_shared<AudioVoice>();
        loadedVoice->buffer.makeCopyOf(sampleData->buffer);
        loadedVoice->ratio = ratio;
        loadedVoice->rootNote = rootNote;
        loadedVoice->bufferSampleRate = sampleRate;

        // Snapshot this independent slice buffer for non-destructive revert!
        originalEditBuffer.makeCopyOf(sampleData->buffer);
        originalEditSampleRate = sampleRate;
        originalSnapshotValid = true;
        isLoadedInSampler.store(isSamplerSample, std::memory_order_relaxed);
    }

    EngineCommand cmd;
    cmd.type = EngineCommandType::LoadPreviewSample;
    cmd.sampleData = sampleData;
    cmd.boolVal1 = false;
    pushCommand(cmd);

    listeners.call([displayName](AudioEngineListener& l) {
        l.sampleLoaded(displayName);
        l.playbackStateChanged(false);
    });

    return true;
}

void AudioEngine::play()
{
    double currentRatio = sampleStartRatioAtomic.load(std::memory_order_relaxed);
    double totalLen = totalDurationAtomic.load(std::memory_order_relaxed);
    double stoppedPos = stoppedPositionSecs.load(std::memory_order_relaxed);
    if (totalLen > 0.0)
    {
        double r = stoppedPos / totalLen;
        if (r >= currentRatio && r < sampleEndRatioAtomic.load(std::memory_order_relaxed))
            currentRatio = r;
    }

    EngineCommand cmd;
    cmd.type = EngineCommandType::Play;
    cmd.doubleVal1 = currentRatio;
    pushCommand(cmd);

    listeners.call([](AudioEngineListener& l) {
        l.playbackStateChanged(true);
    });
    lastNotifiedPlayingState = true;
}

void AudioEngine::pause()
{
    EngineCommand cmd;
    cmd.type = EngineCommandType::Pause;
    pushCommand(cmd);

    lastNotifiedPlayingState = false;
    listeners.call([](AudioEngineListener& l) { l.playbackStateChanged(false); });
}

void AudioEngine::stop()
{
    stoppedPositionSecs.store(0.0, std::memory_order_relaxed);
    double totalLen = totalDurationAtomic.load(std::memory_order_relaxed);
    stoppedPositionSecs.store(sampleStartRatioAtomic.load(std::memory_order_relaxed) * totalLen, std::memory_order_relaxed);

    EngineCommand cmd;
    cmd.type = EngineCommandType::Stop;
    pushCommand(cmd);

    lastNotifiedPlayingState = false;
    listeners.call([](AudioEngineListener& l) { l.playbackStateChanged(false); });
}

void AudioEngine::clearMasterSample()
{
    {
        const juce::ScopedLock sl(uiDataLock);
        currentMasterSample = nullptr;
        loadedVoice = nullptr;
        currentFile = juce::File();
        totalDurationAtomic.store(0.0, std::memory_order_relaxed);
    }
    stop();
}

void AudioEngine::setPositionRatio(double ratio)
{
    double startR = sampleStartRatioAtomic.load(std::memory_order_relaxed);
    double endR = sampleEndRatioAtomic.load(std::memory_order_relaxed);
    double clamped = juce::jlimit(startR, endR, ratio);

    double totalLen = totalDurationAtomic.load(std::memory_order_relaxed);
    stoppedPositionSecs.store(clamped * totalLen, std::memory_order_relaxed);

    EngineCommand cmd;
    cmd.type = EngineCommandType::SeekRatio;
    cmd.doubleVal1 = clamped;
    pushCommand(cmd);
}

void AudioEngine::setPitchTrackingEnabled(bool enabled)
{
    pitchTrackingEnabled.store(enabled, std::memory_order_relaxed);
    updateVoiceRatios();
    listeners.call([enabled](AudioEngineListener& l) { l.pitchTrackingStateChanged(enabled); });
}

void AudioEngine::setOneShotEnabled(bool enabled)
{
    oneShotEnabled.store(enabled, std::memory_order_relaxed);
    listeners.call([enabled](AudioEngineListener& l) { l.oneShotStateChanged(enabled); });
}

void AudioEngine::setLooping(bool shouldLoop)
{
    isLoopingAtomic.store(shouldLoop, std::memory_order_relaxed);
    EngineCommand cmd;
    cmd.type = EngineCommandType::SetLooping;
    cmd.boolVal1 = shouldLoop;
    pushCommand(cmd);
    listeners.call([shouldLoop](AudioEngineListener& l) { l.loopingStateChanged(shouldLoop); });
}

void AudioEngine::setGain(float newGain)
{
    float g = juce::jlimit(0.0f, 1.5f, newGain);
    gainLevelAtomic.store(g, std::memory_order_relaxed);
    EngineCommand cmd;
    cmd.type = EngineCommandType::SetGain;
    cmd.floatVal1 = g;
    pushCommand(cmd);
}

double AudioEngine::getCurrentPositionSeconds() const
{
    return currentPositionAtomic.load(std::memory_order_relaxed);
}

void AudioEngine::getMinMaxForTimeRange(double startTimeSecs, double endTimeSecs, float& minVal, float& maxVal, int channel) const
{
    minVal = 0.0f;
    maxVal = 0.0f;

    const juce::ScopedLock sl(uiDataLock);
    if (loadedVoice == nullptr || loadedVoice->buffer.getNumSamples() == 0)
        return;

    const auto& buffer = loadedVoice->buffer;
    int numSamples = buffer.getNumSamples();
    if (numSamples <= 0) return;

    double totalLen = totalDurationAtomic.load(std::memory_order_relaxed);
    if (totalLen <= 0.0) return;

    int startSample = juce::jlimit(0, numSamples - 1, static_cast<int>((startTimeSecs / totalLen) * numSamples));
    int endSample = juce::jlimit(startSample + 1, numSamples, static_cast<int>((endTimeSecs / totalLen) * numSamples));
    int numToRead = endSample - startSample;
    if (numToRead <= 0) return;

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

    const juce::ScopedLock sl(uiDataLock);
    if (loadedVoice == nullptr || loadedVoice->buffer.getNumSamples() == 0)
        return;

    const auto& buffer = loadedVoice->buffer;
    int numSamples = buffer.getNumSamples();
    if (numSamples <= 0) return;

    startRatio = juce::jlimit(0.0, 1.0, startRatio);
    endRatio = juce::jlimit(0.0, 1.0, endRatio);
    if (endRatio <= startRatio) endRatio = std::min(1.0, startRatio + 0.001);

    int startSample = juce::jlimit(0, numSamples - 1, static_cast<int>(startRatio * numSamples));
    int endSample = juce::jlimit(startSample + 1, numSamples, static_cast<int>(endRatio * numSamples));
    int numToRead = endSample - startSample;
    if (numToRead <= 0) return;

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

WaveformPeaks AudioEngine::getWaveformPeaks() const
{
    const juce::ScopedLock sl(uiDataLock);
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
    double start = juce::jlimit(0.0, 1.0, startRatio);
    double end = juce::jlimit(0.0, 1.0, endRatio);
    if (end < start) std::swap(start, end);

    if (end - start < 0.0001)
    {
        if (start <= 0.9999) end = std::min(1.0, start + 0.0001);
        else start = std::max(0.0, end - 0.0001);
    }

    sampleStartRatioAtomic.store(start, std::memory_order_relaxed);
    sampleEndRatioAtomic.store(end, std::memory_order_relaxed);

    EngineCommand cmd;
    cmd.type = EngineCommandType::SetSampleRange;
    cmd.doubleVal1 = start;
    cmd.doubleVal2 = end;
    pushCommand(cmd);
}

bool AudioEngine::getAudioBufferCopy(juce::AudioBuffer<float>& destBuffer, double& sampleRate) const
{
    const juce::ScopedLock sl(uiDataLock);
    if (loadedVoice == nullptr || loadedVoice->buffer.getNumSamples() == 0)
        return false;

    destBuffer.makeCopyOf(loadedVoice->buffer);
    sampleRate = engineSampleRateAtomic.load(std::memory_order_relaxed);
    return true;
}

void AudioEngine::snapshotOriginalForEditing()
{
    const juce::ScopedLock sl(uiDataLock);
    if (loadedVoice == nullptr || loadedVoice->buffer.getNumSamples() == 0)
        return;

    originalEditBuffer.makeCopyOf(loadedVoice->buffer);
    originalEditSampleRate = currentFileSampleRateAtomic.load(std::memory_order_relaxed);
    originalSnapshotValid = true;
}

bool AudioEngine::restoreOriginal()
{
    const juce::ScopedLock sl(uiDataLock);
    if (!originalSnapshotValid || originalEditBuffer.getNumSamples() == 0)
        return false;
    if (loadedVoice == nullptr)
        return false;

    loadedVoice->buffer.makeCopyOf(originalEditBuffer);

    sampleStartRatioAtomic.store(0.0, std::memory_order_relaxed);
    sampleEndRatioAtomic.store(1.0, std::memory_order_relaxed);
    stoppedPositionSecs.store(0.0, std::memory_order_relaxed);

    rebuildWaveformPeaks();
    replaceEditedSampleInMemory();
    return true;
}

bool AudioEngine::hasOriginalSnapshot() const
{
    const juce::ScopedLock sl(uiDataLock);
    return originalSnapshotValid && originalEditBuffer.getNumSamples() > 0;
}

bool AudioEngine::cropLoadedSample(double startRatio, double endRatio)
{
    const juce::ScopedLock sl(uiDataLock);
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
    sampleStartRatioAtomic.store(0.0, std::memory_order_relaxed);
    sampleEndRatioAtomic.store(1.0, std::memory_order_relaxed);
    stoppedPositionSecs.store(0.0, std::memory_order_relaxed);

    rebuildWaveformPeaks();
    replaceEditedSampleInMemory();
    return true;
}

bool AudioEngine::normalizeLoadedSample()
{
    const juce::ScopedLock sl(uiDataLock);
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
        return false;
    }

    float scaleFactor = 1.0f / maxPeak;
    loadedVoice->buffer.applyGain(scaleFactor);
    rebuildWaveformPeaks();
    replaceEditedSampleInMemory();
    return true;
}

void AudioEngine::rebuildWaveformPeaks()
{
    if (loadedVoice == nullptr || loadedVoice->buffer.getNumSamples() == 0)
        return;

    float maxPeak = 0.0f;
    int totalSamples = loadedVoice->buffer.getNumSamples();
    int numChannels = loadedVoice->buffer.getNumChannels();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto range = loadedVoice->buffer.findMinMax(ch, 0, totalSamples);
        float peak = std::max(std::abs(range.getStart()), std::abs(range.getEnd()));
        if (peak > maxPeak)
            maxPeak = peak;
    }

    if (maxPeak > 1.0f)
    {
        float scaleFactor = 1.0f / maxPeak;
        loadedVoice->buffer.applyGain(scaleFactor);
    }

    WaveformPeaks peaks;
    peaks.numChannels = numChannels;
    peaks.numPoints = 2048;
    peaks.minLeft.resize(2048, 0.0f);
    peaks.maxLeft.resize(2048, 0.0f);
    peaks.minRight.resize(2048, 0.0f);
    peaks.maxRight.resize(2048, 0.0f);

    for (int p = 0; p < 2048; ++p)
    {
        int sStart = static_cast<int>((static_cast<double>(p) / 2048.0) * totalSamples);
        int sEnd = static_cast<int>((static_cast<double>(p + 1) / 2048.0) * totalSamples);
        sEnd = juce::jlimit(sStart + 1, totalSamples, sEnd);
        int numRead = sEnd - sStart;

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
    const juce::ScopedLock sl(uiDataLock);
    if (loadedVoice == nullptr || loadedVoice->buffer.getNumSamples() == 0)
        return false;

    startRatio = juce::jlimit(0.0, 1.0, startRatio);
    endRatio = juce::jlimit(0.0, 1.0, endRatio);
    if (endRatio <= startRatio + 0.0001)
        return false;

    int totalSamples = loadedVoice->buffer.getNumSamples();
    int startSample = static_cast<int>(startRatio * totalSamples);
    int endSample = static_cast<int>(endRatio * totalSamples);
    int numToSilence = endSample - startSample;
    if (numToSilence <= 0)
        return false;

    int numChannels = loadedVoice->buffer.getNumChannels();
    for (int ch = 0; ch < numChannels; ++ch)
    {
        loadedVoice->buffer.clear(ch, startSample, numToSilence);
    }

    rebuildWaveformPeaks();
    replaceEditedSampleInMemory();
    return true;
}

bool AudioEngine::silenceSpectralRegion(double startRatio, double endRatio, float minFreqHz, float maxFreqHz)
{
    return adjustSpectralRegionGain(startRatio, endRatio, minFreqHz, maxFreqHz, -120.0f);
}

bool AudioEngine::adjustSpectralRegionGain(double startRatio, double endRatio, float minFreqHz, float maxFreqHz, float gaindB)
{
    const juce::ScopedLock sl(uiDataLock);
    if (loadedVoice == nullptr || loadedVoice->buffer.getNumSamples() == 0)
        return false;

    startRatio = juce::jlimit(0.0, 1.0, startRatio);
    endRatio = juce::jlimit(0.0, 1.0, endRatio);
    if (endRatio <= startRatio + 0.0001)
        return false;

    int totalSamples = loadedVoice->buffer.getNumSamples();
    int startSample = static_cast<int>(startRatio * totalSamples);
    int endSample = static_cast<int>(endRatio * totalSamples);
    int numSamples = endSample - startSample;
    if (numSamples < 256)
        return false;

    double fileSr = currentFileSampleRateAtomic.load(std::memory_order_relaxed);
    if (fileSr <= 0.0) fileSr = 44100.0;
    float centerFreq = std::sqrt(std::max(20.0f, minFreqHz) * std::min(static_cast<float>(fileSr * 0.49), maxFreqHz));
    float bandwidth = std::max(20.0f, maxFreqHz - minFreqHz);
    float q = std::max(0.1f, centerFreq / bandwidth);

    int numChannels = loadedVoice->buffer.getNumChannels();
    for (int ch = 0; ch < numChannels; ++ch)
    {
        juce::dsp::IIR::Coefficients<float>::Ptr coeffs;
        if (gaindB <= -90.0f)
        {
            coeffs = juce::dsp::IIR::Coefficients<float>::makeNotch(fileSr, centerFreq, q);
        }
        else
        {
            coeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(fileSr, centerFreq, q, juce::Decibels::decibelsToGain(gaindB));
        }

        juce::dsp::IIR::Filter<float> filter;
        filter.coefficients = coeffs;
        filter.reset();

        float* channelData = loadedVoice->buffer.getWritePointer(ch, startSample);
        for (int i = 0; i < numSamples; ++i)
        {
            channelData[i] = filter.processSample(channelData[i]);
        }
    }

    rebuildWaveformPeaks();
    replaceEditedSampleInMemory();
    return true;
}

bool AudioEngine::isolateSpectralRegion(double startRatio, double endRatio, float minFreqHz, float maxFreqHz)
{
    const juce::ScopedLock sl(uiDataLock);
    if (loadedVoice == nullptr || loadedVoice->buffer.getNumSamples() == 0)
        return false;

    startRatio = juce::jlimit(0.0, 1.0, startRatio);
    endRatio = juce::jlimit(0.0, 1.0, endRatio);
    if (endRatio <= startRatio + 0.0001)
        return false;

    int totalSamples = loadedVoice->buffer.getNumSamples();
    int startSample = static_cast<int>(startRatio * totalSamples);
    int endSample = static_cast<int>(endRatio * totalSamples);
    int numSamples = endSample - startSample;
    if (numSamples < 256)
        return false;

    double fileSr = currentFileSampleRateAtomic.load(std::memory_order_relaxed);
    if (fileSr <= 0.0) fileSr = 44100.0;
    float centerFreq = std::sqrt(std::max(20.0f, minFreqHz) * std::min(static_cast<float>(fileSr * 0.49), maxFreqHz));
    float bandwidth = std::max(20.0f, maxFreqHz - minFreqHz);
    float q = std::max(0.1f, centerFreq / bandwidth);

    int numChannels = loadedVoice->buffer.getNumChannels();
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto coeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass(fileSr, centerFreq, q);
        juce::dsp::IIR::Filter<float> filter;
        filter.coefficients = coeffs;
        filter.reset();

        float* channelData = loadedVoice->buffer.getWritePointer(ch, startSample);
        for (int i = 0; i < numSamples; ++i)
        {
            channelData[i] = filter.processSample(channelData[i]);
        }
    }

    rebuildWaveformPeaks();
    replaceEditedSampleInMemory();
    return true;
}

bool AudioEngine::repairSpectralRegion(double startRatio, double endRatio, float minFreqHz, float maxFreqHz)
{
    const juce::ScopedLock sl(uiDataLock);
    if (loadedVoice == nullptr || loadedVoice->buffer.getNumSamples() == 0)
        return false;

    startRatio = juce::jlimit(0.0, 1.0, startRatio);
    endRatio = juce::jlimit(0.0, 1.0, endRatio);
    if (endRatio <= startRatio + 0.0001)
        return false;

    int totalSamples = loadedVoice->buffer.getNumSamples();
    int startSample = static_cast<int>(startRatio * totalSamples);
    int endSample = static_cast<int>(endRatio * totalSamples);
    int numSamples = endSample - startSample;
    if (numSamples < 64)
        return false;

    double fileSr = currentFileSampleRateAtomic.load(std::memory_order_relaxed);
    if (fileSr <= 0.0) fileSr = 44100.0;
    float centerFreq = std::sqrt(std::max(20.0f, minFreqHz) * std::min(static_cast<float>(fileSr * 0.49), maxFreqHz));
    float bandwidth = std::max(20.0f, maxFreqHz - minFreqHz);
    float q = std::max(0.2f, centerFreq / bandwidth);

    int numChannels = loadedVoice->buffer.getNumChannels();
    int contextSamples = std::min(numSamples, static_cast<int>(fileSr * 0.04)); // 40ms context
    if (contextSamples <= 0) contextSamples = 16;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        // 1. Isolate the target band using a bandpass filter
        auto bpCoeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass(fileSr, centerFreq, q);
        juce::dsp::IIR::Filter<float> bpFilter;
        bpFilter.coefficients = bpCoeffs;
        bpFilter.reset();

        std::vector<float> bandData(static_cast<size_t>(numSamples));
        const float* src = loadedVoice->buffer.getReadPointer(ch, startSample);
        for (int i = 0; i < numSamples; ++i)
            bandData[static_cast<size_t>(i)] = bpFilter.processSample(src[i]);

        // 2. Sample pre-context and post-context in this band
        int preStart = std::max(0, startSample - contextSamples);
        int postEnd = std::min(totalSamples, endSample + contextSamples);

        float preEnergy = 0.0f;
        int preCount = startSample - preStart;
        if (preCount > 0)
        {
            const float* preSrc = loadedVoice->buffer.getReadPointer(ch, preStart);
            for (int i = 0; i < preCount; ++i) preEnergy += std::abs(preSrc[i]);
            preEnergy /= preCount;
        }

        float postEnergy = 0.0f;
        int postCount = postEnd - endSample;
        if (postCount > 0)
        {
            const float* postSrc = loadedVoice->buffer.getReadPointer(ch, endSample);
            for (int i = 0; i < postCount; ++i) postEnergy += std::abs(postSrc[i]);
            postEnergy /= postCount;
        }

        // 3. Smooth inpaint / healing interpolation across the region
        float* dst = loadedVoice->buffer.getWritePointer(ch, startSample);
        for (int i = 0; i < numSamples; ++i)
        {
            float t = static_cast<float>(i) / static_cast<float>(numSamples);
            float targetEnergy = (1.0f - t) * preEnergy + t * postEnergy;
            float currentBandVal = bandData[static_cast<size_t>(i)];
            float currentAbs = std::abs(currentBandVal) + 1e-6f;
            float scale = std::min(1.0f, targetEnergy / currentAbs);

            // Replace corrupted band component with smoothed inpaint fill
            dst[i] = (dst[i] - currentBandVal) + currentBandVal * scale;
        }
    }

    rebuildWaveformPeaks();
    replaceEditedSampleInMemory();
    return true;
}

bool AudioEngine::removeSpectralHarmonics(double startRatio, double endRatio, float fundamentalFreqHz)
{
    const juce::ScopedLock sl(uiDataLock);
    if (loadedVoice == nullptr || loadedVoice->buffer.getNumSamples() == 0)
        return false;

    startRatio = juce::jlimit(0.0, 1.0, startRatio);
    endRatio = juce::jlimit(0.0, 1.0, endRatio);
    if (endRatio <= startRatio + 0.0001)
        return false;

    int totalSamples = loadedVoice->buffer.getNumSamples();
    int startSample = static_cast<int>(startRatio * totalSamples);
    int endSample = static_cast<int>(endRatio * totalSamples);
    int numSamples = endSample - startSample;
    if (numSamples < 64)
        return false;

    double fileSr = currentFileSampleRateAtomic.load(std::memory_order_relaxed);
    if (fileSr <= 0.0) fileSr = 44100.0;
    float maxNyquist = static_cast<float>(fileSr * 0.49);

    int numChannels = loadedVoice->buffer.getNumChannels();
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* channelData = loadedVoice->buffer.getWritePointer(ch, startSample);

        // Notch out fundamental and first 4 integer harmonics
        for (int h = 1; h <= 5; ++h)
        {
            float freq = fundamentalFreqHz * static_cast<float>(h);
            if (freq < 20.0f || freq >= maxNyquist)
                break;

            auto notchCoeffs = juce::dsp::IIR::Coefficients<float>::makeNotch(fileSr, freq, 8.0f);
            juce::dsp::IIR::Filter<float> notchFilter;
            notchFilter.coefficients = notchCoeffs;
            notchFilter.reset();

            for (int i = 0; i < numSamples; ++i)
                channelData[i] = notchFilter.processSample(channelData[i]);
        }
    }

    rebuildWaveformPeaks();
    replaceEditedSampleInMemory();
    return true;
}

bool AudioEngine::denoiseSpectralRegion(double startRatio, double endRatio, float minFreqHz, float maxFreqHz, float reductionDb)
{
    const juce::ScopedLock sl(uiDataLock);
    if (loadedVoice == nullptr || loadedVoice->buffer.getNumSamples() == 0)
        return false;

    startRatio = juce::jlimit(0.0, 1.0, startRatio);
    endRatio = juce::jlimit(0.0, 1.0, endRatio);
    if (endRatio <= startRatio + 0.0001)
        return false;

    int totalSamples = loadedVoice->buffer.getNumSamples();
    int startSample = static_cast<int>(startRatio * totalSamples);
    int endSample = static_cast<int>(endRatio * totalSamples);
    int numSamples = endSample - startSample;
    if (numSamples < 64)
        return false;

    double fileSr = currentFileSampleRateAtomic.load(std::memory_order_relaxed);
    if (fileSr <= 0.0) fileSr = 44100.0;
    float centerFreq = std::sqrt(std::max(20.0f, minFreqHz) * std::min(static_cast<float>(fileSr * 0.49), maxFreqHz));
    float bandwidth = std::max(20.0f, maxFreqHz - minFreqHz);
    float q = std::max(0.2f, centerFreq / bandwidth);

    float attenGain = juce::Decibels::decibelsToGain(-std::abs(reductionDb));

    int numChannels = loadedVoice->buffer.getNumChannels();
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto bpCoeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass(fileSr, centerFreq, q);
        juce::dsp::IIR::Filter<float> bpFilter;
        bpFilter.coefficients = bpCoeffs;
        bpFilter.reset();

        float* channelData = loadedVoice->buffer.getWritePointer(ch, startSample);
        for (int i = 0; i < numSamples; ++i)
        {
            float bandVal = bpFilter.processSample(channelData[i]);
            // Suppress noise in this band
            channelData[i] = (channelData[i] - bandVal) + bandVal * attenGain;
        }
    }

    rebuildWaveformPeaks();
    replaceEditedSampleInMemory();
    return true;
}

bool AudioEngine::widenSpectralRegion(double startRatio, double endRatio, float minFreqHz, float maxFreqHz, float widthFactor)
{
    const juce::ScopedLock sl(uiDataLock);
    if (loadedVoice == nullptr || loadedVoice->buffer.getNumSamples() == 0 || loadedVoice->buffer.getNumChannels() < 2)
        return false;

    startRatio = juce::jlimit(0.0, 1.0, startRatio);
    endRatio = juce::jlimit(0.0, 1.0, endRatio);
    if (endRatio <= startRatio + 0.0001)
        return false;

    int totalSamples = loadedVoice->buffer.getNumSamples();
    int startSample = static_cast<int>(startRatio * totalSamples);
    int endSample = static_cast<int>(endRatio * totalSamples);
    int numSamples = endSample - startSample;
    if (numSamples < 64)
        return false;

    double fileSr = currentFileSampleRateAtomic.load(std::memory_order_relaxed);
    if (fileSr <= 0.0) fileSr = 44100.0;
    float centerFreq = std::sqrt(std::max(20.0f, minFreqHz) * std::min(static_cast<float>(fileSr * 0.49), maxFreqHz));
    float bandwidth = std::max(20.0f, maxFreqHz - minFreqHz);
    float q = std::max(0.2f, centerFreq / bandwidth);

    auto bpCoeffsL = juce::dsp::IIR::Coefficients<float>::makeBandPass(fileSr, centerFreq, q);
    auto bpCoeffsR = juce::dsp::IIR::Coefficients<float>::makeBandPass(fileSr, centerFreq, q);
    juce::dsp::IIR::Filter<float> filterL, filterR;
    filterL.coefficients = bpCoeffsL; filterL.reset();
    filterR.coefficients = bpCoeffsR; filterR.reset();

    float* dataL = loadedVoice->buffer.getWritePointer(0, startSample);
    float* dataR = loadedVoice->buffer.getWritePointer(1, startSample);

    for (int i = 0; i < numSamples; ++i)
    {
        float bL = filterL.processSample(dataL[i]);
        float bR = filterR.processSample(dataR[i]);

        float mid = 0.5f * (bL + bR);
        float side = 0.5f * (bL - bR) * widthFactor;

        float newBL = mid + side;
        float newBR = mid - side;

        dataL[i] = (dataL[i] - bL) + newBL;
        dataR[i] = (dataR[i] - bR) + newBR;
    }

    rebuildWaveformPeaks();
    replaceEditedSampleInMemory();
    return true;
}

bool AudioEngine::saturateSpectralRegion(double startRatio, double endRatio, float minFreqHz, float maxFreqHz, float driveAmount)
{
    const juce::ScopedLock sl(uiDataLock);
    if (loadedVoice == nullptr || loadedVoice->buffer.getNumSamples() == 0)
        return false;

    startRatio = juce::jlimit(0.0, 1.0, startRatio);
    endRatio = juce::jlimit(0.0, 1.0, endRatio);
    if (endRatio <= startRatio + 0.0001)
        return false;

    int totalSamples = loadedVoice->buffer.getNumSamples();
    int startSample = static_cast<int>(startRatio * totalSamples);
    int endSample = static_cast<int>(endRatio * totalSamples);
    int numSamples = endSample - startSample;
    if (numSamples < 64)
        return false;

    double fileSr = currentFileSampleRateAtomic.load(std::memory_order_relaxed);
    if (fileSr <= 0.0) fileSr = 44100.0;
    float centerFreq = std::sqrt(std::max(20.0f, minFreqHz) * std::min(static_cast<float>(fileSr * 0.49), maxFreqHz));
    float bandwidth = std::max(20.0f, maxFreqHz - minFreqHz);
    float q = std::max(0.2f, centerFreq / bandwidth);

    float drive = 1.0f + juce::jlimit(0.1f, 2.0f, driveAmount) * 2.5f;

    int numChannels = loadedVoice->buffer.getNumChannels();
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto bpCoeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass(fileSr, centerFreq, q);
        juce::dsp::IIR::Filter<float> bpFilter;
        bpFilter.coefficients = bpCoeffs;
        bpFilter.reset();

        float* channelData = loadedVoice->buffer.getWritePointer(ch, startSample);
        for (int i = 0; i < numSamples; ++i)
        {
            float bandVal = bpFilter.processSample(channelData[i]);
            float satVal = std::tanh(bandVal * drive) / std::sqrt(drive);
            channelData[i] = (channelData[i] - bandVal) + satVal;
        }
    }

    rebuildWaveformPeaks();
    replaceEditedSampleInMemory();
    return true;
}

bool AudioEngine::reverseSelection(double startRatio, double endRatio)
{
    const juce::ScopedLock sl(uiDataLock);
    if (loadedVoice == nullptr || loadedVoice->buffer.getNumSamples() == 0)
        return false;

    startRatio = juce::jlimit(0.0, 1.0, startRatio);
    endRatio = juce::jlimit(0.0, 1.0, endRatio);
    if (endRatio <= startRatio + 0.0001)
        return false;

    int totalSamples = loadedVoice->buffer.getNumSamples();
    int startSample = static_cast<int>(startRatio * totalSamples);
    int endSample = static_cast<int>(endRatio * totalSamples);
    int numSamples = endSample - startSample;
    if (numSamples <= 1)
        return false;

    int numChannels = loadedVoice->buffer.getNumChannels();
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* data = loadedVoice->buffer.getWritePointer(ch, startSample);
        std::reverse(data, data + numSamples);
    }

    rebuildWaveformPeaks();
    replaceEditedSampleInMemory();
    return true;
}

bool AudioEngine::deverbSelection(double startRatio, double endRatio, float amount)
{
    const juce::ScopedLock sl(uiDataLock);
    if (loadedVoice == nullptr || loadedVoice->buffer.getNumSamples() == 0)
        return false;

    startRatio = juce::jlimit(0.0, 1.0, startRatio);
    endRatio = juce::jlimit(0.0, 1.0, endRatio);
    if (endRatio <= startRatio + 0.0001)
        return false;

    int totalSamples = loadedVoice->buffer.getNumSamples();
    int startSample = static_cast<int>(startRatio * totalSamples);
    int endSample = static_cast<int>(endRatio * totalSamples);
    int numSamples = endSample - startSample;
    if (numSamples <= 100)
        return false;

    double fileSr = currentFileSampleRateAtomic.load(std::memory_order_relaxed);
    if (fileSr <= 0.0) fileSr = 44100.0;
    float sr = static_cast<float>(fileSr);

    // Time constants for envelope tracking and anti-pop gain smoothing
    float attackTime = 0.004f;        // 4ms smooth transient attack
    float releaseFast = 0.020f;       // 20ms short-term energy follower
    float decaySlow = 0.140f;         // 140ms reverberant tail decay tracker
    float gainSmoothingTime = 0.010f; // 10ms gain smoothing to eliminate all clicks & AM pops

    float aFastAttack = std::exp(-1.0f / (sr * attackTime));
    float aFastRelease = std::exp(-1.0f / (sr * releaseFast));
    float aSlowDecay = std::exp(-1.0f / (sr * decaySlow));
    float aGainSmooth = 1.0f - std::exp(-1.0f / (sr * gainSmoothingTime));

    float suppressionStrength = juce::jlimit(0.0f, 1.0f, amount);
    float gainFloor = juce::jmax(0.15f, 1.0f - suppressionStrength * 0.82f); // Gentle gain floor (-16dB max)

    int numChannels = loadedVoice->buffer.getNumChannels();
    std::vector<float> smoothedGains(static_cast<size_t>(numSamples), 1.0f);

    // 5ms crossfade at region boundaries to prevent edge clicks
    int crossfadeLen = std::min(numSamples / 4, static_cast<int>(sr * 0.005f));

    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* data = loadedVoice->buffer.getWritePointer(ch, startSample);

        float fastEnv = 0.0f;
        float slowEnv = 0.0f;
        float currentGain = 1.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            float absVal = std::abs(data[i]);

            // Short-term envelope (smoothed to prevent wave-cycle tracking)
            if (absVal > fastEnv)
                fastEnv = aFastAttack * fastEnv + (1.0f - aFastAttack) * absVal;
            else
                fastEnv = aFastRelease * fastEnv + (1.0f - aFastRelease) * absVal;

            // Long-term reverberant tail envelope
            if (absVal > slowEnv)
                slowEnv = aFastAttack * slowEnv + (1.0f - aFastAttack) * absVal;
            else
                slowEnv = aSlowDecay * slowEnv + (1.0f - aSlowDecay) * absVal;

            // Compute target attenuation based on transient-to-tail ratio
            float targetGain = 1.0f;
            if (slowEnv > 1e-5f)
            {
                float ratio = fastEnv / slowEnv;
                if (ratio < 1.0f)
                {
                    float atten = std::pow(ratio, 1.4f * suppressionStrength);
                    targetGain = gainFloor + (1.0f - gainFloor) * atten;
                }
            }

            // Low-pass filter the gain curve (eliminates clicking, buzzing and pops)
            currentGain += aGainSmooth * (targetGain - currentGain);
            smoothedGains[static_cast<size_t>(i)] = currentGain;
        }

        // Apply smoothed gain curve with boundary cosine crossfades
        for (int i = 0; i < numSamples; ++i)
        {
            float g = smoothedGains[static_cast<size_t>(i)];

            if (crossfadeLen > 0)
            {
                if (i < crossfadeLen)
                {
                    float fade = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::pi * (static_cast<float>(i) / static_cast<float>(crossfadeLen))));
                    g = 1.0f + fade * (g - 1.0f);
                }
                else if (i >= numSamples - crossfadeLen)
                {
                    float remaining = static_cast<float>(numSamples - 1 - i);
                    float fade = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::pi * (remaining / static_cast<float>(crossfadeLen))));
                    g = 1.0f + fade * (g - 1.0f);
                }
            }

            data[i] *= g;
        }
    }

    rebuildWaveformPeaks();
    replaceEditedSampleInMemory();
    return true;
}

bool AudioEngine::adjustGainSelection(double startRatio, double endRatio, float gaindB)
{
    const juce::ScopedLock sl(uiDataLock);
    if (loadedVoice == nullptr || loadedVoice->buffer.getNumSamples() == 0)
        return false;

    startRatio = juce::jlimit(0.0, 1.0, startRatio);
    endRatio = juce::jlimit(0.0, 1.0, endRatio);
    if (endRatio <= startRatio + 0.0001)
        return false;

    int totalSamples = loadedVoice->buffer.getNumSamples();
    int startSample = static_cast<int>(startRatio * totalSamples);
    int endSample = static_cast<int>(endRatio * totalSamples);
    int numSamples = endSample - startSample;
    if (numSamples <= 0)
        return false;

    float gainLinear = juce::Decibels::decibelsToGain(gaindB);
    int numChannels = loadedVoice->buffer.getNumChannels();
    for (int ch = 0; ch < numChannels; ++ch)
    {
        loadedVoice->buffer.applyGain(ch, startSample, numSamples, gainLinear);
    }

    rebuildWaveformPeaks();
    replaceEditedSampleInMemory();
    return true;
}

bool AudioEngine::applyHighPassFilter(double startRatio, double endRatio, float cutoffHz)
{
    const juce::ScopedLock sl(uiDataLock);
    if (loadedVoice == nullptr || loadedVoice->buffer.getNumSamples() == 0)
        return false;

    startRatio = juce::jlimit(0.0, 1.0, startRatio);
    endRatio = juce::jlimit(0.0, 1.0, endRatio);
    if (endRatio <= startRatio + 0.0001)
        return false;

    int totalSamples = loadedVoice->buffer.getNumSamples();
    int startSample = static_cast<int>(startRatio * totalSamples);
    int endSample = static_cast<int>(endRatio * totalSamples);
    int numSamples = endSample - startSample;
    if (numSamples < 32)
        return false;

    double fileSr = currentFileSampleRateAtomic.load(std::memory_order_relaxed);
    if (fileSr <= 0.0) fileSr = 44100.0;
    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(fileSr, cutoffHz, 0.707f);

    int numChannels = loadedVoice->buffer.getNumChannels();
    for (int ch = 0; ch < numChannels; ++ch)
    {
        juce::dsp::IIR::Filter<float> filter;
        filter.coefficients = coeffs;
        filter.reset();

        float* channelData = loadedVoice->buffer.getWritePointer(ch, startSample);
        for (int i = 0; i < numSamples; ++i)
        {
            channelData[i] = filter.processSample(channelData[i]);
        }
    }

    rebuildWaveformPeaks();
    replaceEditedSampleInMemory();
    return true;
}

bool AudioEngine::autoTrimSilence()
{
    const juce::ScopedLock sl(uiDataLock);
    if (loadedVoice == nullptr || loadedVoice->buffer.getNumSamples() == 0)
        return false;

    int totalSamples = loadedVoice->buffer.getNumSamples();
    int numChannels = loadedVoice->buffer.getNumChannels();
    float threshold = 0.002f; // -54 dBFS

    int startSample = 0;
    for (int i = 0; i < totalSamples; ++i)
    {
        float maxSample = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float s = std::abs(loadedVoice->buffer.getSample(ch, i));
            if (s > maxSample) maxSample = s;
        }
        if (maxSample >= threshold)
        {
            startSample = std::max(0, i - 64);
            break;
        }
    }

    int endSample = totalSamples - 1;
    for (int i = totalSamples - 1; i >= startSample; --i)
    {
        float maxSample = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float s = std::abs(loadedVoice->buffer.getSample(ch, i));
            if (s > maxSample) maxSample = s;
        }
        if (maxSample >= threshold)
        {
            endSample = std::min(totalSamples, i + 64);
            break;
        }
    }

    if (endSample <= startSample) return false;
    double startR = static_cast<double>(startSample) / totalSamples;
    double endR = static_cast<double>(endSample) / totalSamples;
    return cropLoadedSample(startR, endR);
}

bool AudioEngine::invertPhaseSelection(double startRatio, double endRatio)
{
    const juce::ScopedLock sl(uiDataLock);
    if (loadedVoice == nullptr || loadedVoice->buffer.getNumSamples() == 0)
        return false;

    startRatio = juce::jlimit(0.0, 1.0, startRatio);
    endRatio = juce::jlimit(0.0, 1.0, endRatio);
    if (endRatio <= startRatio + 0.0001)
        return false;

    int totalSamples = loadedVoice->buffer.getNumSamples();
    int startSample = static_cast<int>(startRatio * totalSamples);
    int endSample = static_cast<int>(endRatio * totalSamples);
    int numSamples = endSample - startSample;
    if (numSamples <= 0)
        return false;

    int numChannels = loadedVoice->buffer.getNumChannels();
    for (int ch = 0; ch < numChannels; ++ch)
    {
        loadedVoice->buffer.applyGain(ch, startSample, numSamples, -1.0f);
    }

    rebuildWaveformPeaks();
    replaceEditedSampleInMemory();
    return true;
}

bool AudioEngine::changeSampleSpeed(double speedMultiplier)
{
    const juce::ScopedLock sl(uiDataLock);
    if (loadedVoice == nullptr || loadedVoice->buffer.getNumSamples() == 0 || speedMultiplier <= 0.05 || speedMultiplier >= 20.0)
        return false;

    int origSamples = loadedVoice->buffer.getNumSamples();
    int numChannels = loadedVoice->buffer.getNumChannels();
    int newNumSamples = static_cast<int>(origSamples / speedMultiplier);
    if (newNumSamples <= 10) return false;

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
    sampleStartRatioAtomic.store(0.0, std::memory_order_relaxed);
    sampleEndRatioAtomic.store(1.0, std::memory_order_relaxed);
    rebuildWaveformPeaks();
    replaceEditedSampleInMemory();
    return true;
}

bool AudioEngine::applyFadesToBuffer(double fadeInMs, int fadeInType, double fadeOutMs, int fadeOutType)
{
    const juce::ScopedLock sl(uiDataLock);
    if (loadedVoice == nullptr || loadedVoice->buffer.getNumSamples() == 0)
        return false;

    int numSamples = loadedVoice->buffer.getNumSamples();
    int startIdx = juce::jlimit(0, numSamples - 1, static_cast<int>(sampleStartRatioAtomic.load(std::memory_order_relaxed) * numSamples));
    int endIdx = juce::jlimit(startIdx + 1, numSamples, static_cast<int>(sampleEndRatioAtomic.load(std::memory_order_relaxed) * numSamples));
    int selSamples = endIdx - startIdx;
    if (selSamples <= 0) return false;

    double sr = currentFileSampleRateAtomic.load(std::memory_order_relaxed);
    if (sr <= 0.0) sr = 44100.0;
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
    replaceEditedSampleInMemory();
    return true;
}

void AudioEngine::replaceEditedSampleInMemory()
{
    if (loadedVoice == nullptr)
        return;

    float maxPeak = 0.0f;
    int numChannels = loadedVoice->buffer.getNumChannels();
    int totalSamples = loadedVoice->buffer.getNumSamples();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto range = loadedVoice->buffer.findMinMax(ch, 0, totalSamples);
        float peak = std::max(std::abs(range.getStart()), std::abs(range.getEnd()));
        if (peak > maxPeak)
            maxPeak = peak;
    }

    if (maxPeak > 1.0f)
    {
        float scaleFactor = 1.0f / maxPeak;
        loadedVoice->buffer.applyGain(scaleFactor);
    }

    double fileSr = currentFileSampleRateAtomic.load(std::memory_order_relaxed);
    if (fileSr <= 0.0) fileSr = 44100.0;

    auto newSampleData = std::make_shared<CachedSample>();
    newSampleData->sampleRate = fileSr;
    newSampleData->rootNote = loadedVoice->rootNote;
    newSampleData->buffer.makeCopyOf(loadedVoice->buffer);

    currentMasterSample = newSampleData;

    juce::String currentPath = currentFile.getFullPathName();
    if (currentPath.isNotEmpty())
    {
        const juce::ScopedLock sl(cacheLock);
        sampleCache[currentPath] = newSampleData;
    }

    numChannelsAtomic.store(numChannels, std::memory_order_relaxed);
    double engineSr = engineSampleRateAtomic.load(std::memory_order_relaxed);
    if (engineSr <= 0.0) engineSr = 44100.0;
    double ratio = (engineSr > 0.0) ? (fileSr / engineSr) : 1.0;
    totalDurationAtomic.store((totalSamples / ratio) / engineSr, std::memory_order_relaxed);

    rebuildWaveformPeaks();

    EngineCommand cmd;
    cmd.type = EngineCommandType::LoadPreviewSample;
    cmd.sampleData = newSampleData;
    cmd.boolVal1 = false;
    pushCommand(cmd);
}

void AudioEngine::setSampleBpm(double bpm)
{
    sampleBpm.store(bpm, std::memory_order_relaxed);
    updateVoiceRatios();
}

void AudioEngine::setHostBpm(double bpm)
{
    hostBpm.store(bpm, std::memory_order_relaxed);
    updateVoiceRatios();
}

void AudioEngine::setHostSyncEnabled(bool enabled)
{
    hostSyncEnabled.store(enabled, std::memory_order_relaxed);
    updateVoiceRatios();
    listeners.call([enabled](AudioEngineListener& l) { l.transportSyncChanged(enabled); });
}

void AudioEngine::toggleHostSync()
{
    setHostSyncEnabled(!hostSyncEnabled.load(std::memory_order_relaxed));
}

void AudioEngine::setInternalBpm(double bpm)
{
    double clamped = juce::jlimit(20.0, 300.0, bpm);
    internalBpm.store(clamped, std::memory_order_relaxed);
    updateVoiceRatios();
    listeners.call([clamped](AudioEngineListener& l) { l.bpmChanged(clamped); });
}

void AudioEngine::setHostTransportState(bool isPlaying, double bpm, double positionSec, double ppqPosition)
{
    bool prevHostPlaying = isHostPlaying.load(std::memory_order_relaxed);
    isHostPlaying.store(isPlaying, std::memory_order_relaxed);
    if (bpm > 20.0 && bpm <= 400.0)
    {
        hostBpm.store(bpm, std::memory_order_relaxed);
    }
    hostPositionSeconds.store(positionSec, std::memory_order_relaxed);
    hostPpqPosition.store(ppqPosition, std::memory_order_relaxed);

    if (hostSyncEnabled.load(std::memory_order_relaxed))
    {
        if (isPlaying && !prevHostPlaying)
        {
            play();
        }
        else if (!isPlaying && prevHostPlaying)
        {
            pause();
        }
    }
}

void AudioEngine::togglePlay()
{
    if (isPlaying())
        pause();
    else
        play();
}

void AudioEngine::toggleLoop()
{
    setLooping(!isLooping());
}

void AudioEngine::updateVoiceRatios()
{
    EngineCommand cmd;
    cmd.type = EngineCommandType::UpdateVoiceRatios;
    pushCommand(cmd);
}

void AudioEngine::startRecording()
{
    recordingWritePosition.store(0, std::memory_order_relaxed);
    recordingActive.store(true, std::memory_order_relaxed);
}

void AudioEngine::stopRecording()
{
    recordingActive.store(false, std::memory_order_relaxed);
}

double AudioEngine::getRecordingDurationSeconds() const
{
    double sr = engineSampleRateAtomic.load(std::memory_order_relaxed);
    if (sr <= 0.0) sr = 44100.0;
    return static_cast<double>(recordingWritePosition.load(std::memory_order_relaxed)) / sr;
}

void AudioEngine::getLiveInputLevels(float& leftLevel, float& rightLevel) const
{
    leftLevel = liveInputLeft.load(std::memory_order_relaxed);
    rightLevel = liveInputRight.load(std::memory_order_relaxed);
}

bool AudioEngine::getRecordedBufferCopy(juce::AudioBuffer<float>& destBuffer, double& sampleRate) const
{
    int writePos = recordingWritePosition.load(std::memory_order_relaxed);
    if (writePos <= 0)
        return false;

    destBuffer.setSize(2, writePos);
    for (int ch = 0; ch < 2; ++ch)
    {
        destBuffer.copyFrom(ch, 0, recordingBuffer, ch, 0, writePos);
    }
    sampleRate = engineSampleRateAtomic.load(std::memory_order_relaxed);
    return true;
}

juce::File AudioEngine::saveRecordingToWav(const juce::String& baseFileName)
{
    juce::AudioBuffer<float> copyBuf;
    double sr = engineSampleRateAtomic.load(std::memory_order_relaxed);
    if (sr <= 0.0) sr = 44100.0;
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
    EngineCommand cmd;
    cmd.type = EngineCommandType::PlayMetronome;
    cmd.intVal1 = isAccent ? 1 : 0;
    pushCommand(cmd);
}

void AudioEngine::setInputParametricEq(const std::array<float, 9>& freqs, const std::array<float, 9>& gains, bool lowCut)
{
    for (int i = 0; i < 9; ++i)
    {
        eqFreqs[i].store(freqs[i], std::memory_order_relaxed);
        eqGains[i].store(gains[i], std::memory_order_relaxed);
    }
    eqLowCutEnabled.store(lowCut, std::memory_order_relaxed);
}

} // namespace openwav
