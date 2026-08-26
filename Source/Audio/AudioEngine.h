#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_audio_basics/juce_audio_basics.h>
 #include <juce_audio_formats/juce_audio_formats.h>
 #include <juce_audio_utils/juce_audio_utils.h>
 #include <juce_dsp/juce_dsp.h>
#endif
#include <atomic>
#include <future>
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <memory>
#include <map>

namespace openwav
{

class AudioEngineListener
{
public:
    virtual ~AudioEngineListener() = default;
    virtual void playbackStateChanged(bool /*isPlaying*/) {}
    virtual void playbackPositionChanged(double /*currentSeconds*/, double /*totalSeconds*/) {}
    virtual void sampleLoaded(const juce::String& /*filePath*/) {}
    virtual void pitchTrackingStateChanged(bool /*enabled*/) {}
    virtual void oneShotStateChanged(bool /*enabled*/) {}
    virtual void loopingStateChanged(bool /*enabled*/) {}
    virtual void transportSyncChanged(bool /*isSynced*/) {}
    virtual void bpmChanged(double /*newBpm*/) {}
    virtual void activeSliceTriggered(int /*sliceIndex*/) {}
};

struct WaveformPeaks
{
    std::vector<float> minLeft;
    std::vector<float> maxLeft;
    std::vector<float> minRight;
    std::vector<float> maxRight;
    int numChannels { 0 };
    int numPoints { 0 };
};

struct AudioVoice
{
    juce::AudioBuffer<float> buffer;
    double readPosition { 0.0 };
    double ratio { 1.0 };
    bool isLooping { false };
    bool finished { false };
    juce::ADSR adsr;
    double startRatio { 0.0 };
    double endRatio { 1.0 };
    int rootNote { 60 };
    int triggerMidiNote { -1 };
    float gain { 1.0f };
    float fineTuneCents { 0.0f };
    double bufferSampleRate { 0.0 };
    bool isZoneVoice { false };
    bool isOneShot { false };
    int groupIndex { 0 };
    float pan { 0.0f };
};

struct CachedSample
{
    juce::AudioBuffer<float> buffer;
    double sampleRate { 44100.0 };
    int rootNote { 60 };
};

// Fixed real-time voice slot for the audio thread (100% allocation-free)
struct RealtimeVoiceSlot
{
    std::shared_ptr<CachedSample> sample;
    double readPosition { 0.0 };
    double ratio { 1.0 };
    bool isLooping { false };
    bool active { false };
    juce::ADSR adsr;
    double startRatio { 0.0 };
    double endRatio { 1.0 };
    int rootNote { 60 };
    int triggerMidiNote { -1 };
    float gain { 1.0f };
    float fineTuneCents { 0.0f };
    double bufferSampleRate { 44100.0 };
    bool isZoneVoice { false };
    bool isOneShot { false };
    bool isMetronome { false };
    int groupIndex { 0 };
    float pan { 0.0f };
    int64_t sampleStartOffset { 0 };
    int64_t sampleEndOffset { 0 };
    int64_t loopStartOffset { 0 };
    int64_t loopEndOffset { 0 };
};

enum class EngineCommandType : int
{
    None = 0,
    Play,
    Pause,
    Stop,
    SeekRatio,
    SetLooping,
    SetGain,
    SetSampleRange,
    SetPitchTracking,
    SetOneShot,
    LoadPreviewSample,
    PlayZoneVoice,
    StopZoneVoice,
    PlayMetronome,
    UpdateVoiceRatios
};

struct EngineCommand
{
    EngineCommandType type { EngineCommandType::None };
    int intVal1 { 0 };
    int intVal2 { 0 };
    int intVal3 { 0 };
    float floatVal1 { 0.0f };
    float floatVal2 { 0.0f };
    float floatVal3 { 0.0f };
    float floatVal4 { 0.0f };
    float floatVal5 { 0.0f };
    float floatVal6 { 0.0f };
    float floatVal7 { 0.0f };
    double doubleVal1 { 0.0 };
    double doubleVal2 { 1.0 };
    int64_t int64Val1 { 0 };
    int64_t int64Val2 { 0 };
    int64_t int64Val3 { 0 };
    int64_t int64Val4 { 0 };
    bool boolVal1 { false };
    bool boolVal2 { false };
    std::shared_ptr<CachedSample> sampleData;
};

class AudioEngine : public juce::ChangeListener, public juce::Timer
{
public:
    static constexpr int MaxActiveVoices = 32;
    static constexpr int CommandQueueCapacity = 256;
    static constexpr int MaxGroups = 256;

    AudioEngine();
    ~AudioEngine() override;

    void timerCallback() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock);
    void releaseResources();
    void processNextAudioBlock(juce::AudioBuffer<float>& outputBuffer, juce::MidiBuffer& midiMessages);

    // Audio Playback Controls
    bool loadFile(const juce::File& audioFile, bool autoPlay = false, bool isSamplerSample = false);
    bool loadAudioBuffer(const juce::String& displayName, const juce::AudioBuffer<float>& buffer, double sampleRate, int rootNote = 60, bool isSamplerSample = true);
    bool isCurrentSampleInSampler() const { return isLoadedInSampler.load(std::memory_order_relaxed); }
    void setLoadedInSampler(bool inSampler) { isLoadedInSampler.store(inSampler, std::memory_order_relaxed); }
    void preloadSampleFiles(const std::vector<juce::File>& files);
    void putSampleInCache(const juce::String& filePath, double sampleRate, const juce::AudioBuffer<float>& buf);
    bool getCachedSampleCopy(const juce::String& filePath, juce::AudioBuffer<float>& destBuffer, double& sampleRate) const;
    void playZoneVoice(const juce::File& file, int triggerMidiNote, int rootNote, float fineTuneCents, float gainDb, float velocity = 1.0f,
                       float attackSec = 0.005f, float decaySec = 0.1f, float sustainLevel = 1.0f, float releaseSec = 0.2f, bool isOneShot = false, bool isLooping = false,
                       int groupIndex = 0, float pan = 0.0f,
                       int64_t sampleStart = 0, int64_t sampleEnd = 0, int64_t loopStart = 0, int64_t loopEnd = 0);

    // Sampler ADSR Envelope Controls
    void setSamplerAdsr(float attackSec, float decaySec, float sustainLevel, float releaseSec)
    {
        samplerAttackSec.store(juce::jmax(0.001f, attackSec), std::memory_order_relaxed);
        samplerDecaySec.store(juce::jmax(0.001f, decaySec), std::memory_order_relaxed);
        samplerSustainLevel.store(juce::jlimit(0.0f, 1.0f, sustainLevel), std::memory_order_relaxed);
        samplerReleaseSec.store(juce::jmax(0.001f, releaseSec), std::memory_order_relaxed);
    }
    void setSamplerAttackSec(float sec) { samplerAttackSec.store(juce::jmax(0.001f, sec), std::memory_order_relaxed); }
    float getSamplerAttackSec() const { return samplerAttackSec.load(std::memory_order_relaxed); }
    void setSamplerDecaySec(float sec) { samplerDecaySec.store(juce::jmax(0.001f, sec), std::memory_order_relaxed); }
    float getSamplerDecaySec() const { return samplerDecaySec.load(std::memory_order_relaxed); }
    void setSamplerSustainLevel(float sus) { samplerSustainLevel.store(juce::jlimit(0.0f, 1.0f, sus), std::memory_order_relaxed); }
    float getSamplerSustainLevel() const { return samplerSustainLevel.load(std::memory_order_relaxed); }
    void setSamplerReleaseSec(float sec) { samplerReleaseSec.store(juce::jmax(0.001f, sec), std::memory_order_relaxed); }
    float getSamplerReleaseSec() const { return samplerReleaseSec.load(std::memory_order_relaxed); }

    // Algorithmic Reverb
    void setSamplerReverbAmount(float amount) { samplerReverbAmount.store(juce::jlimit(0.0f, 1.0f, amount), std::memory_order_relaxed); }
    float getSamplerReverbAmount() const { return samplerReverbAmount.load(std::memory_order_relaxed); }

    // Impulse Response (IR) Convolution Reverb
    void loadImpulseResponseFile(const juce::File& irFile);
    void setSamplerIrReverbAmount(float wet) { irReverbAmount.store(juce::jlimit(0.0f, 1.0f, wet), std::memory_order_relaxed); }
    float getSamplerIrReverbAmount() const { return irReverbAmount.load(std::memory_order_relaxed); }
    void setSamplerIrReverbDryLevel(float dry) { irReverbDryLevel.store(juce::jlimit(0.0f, 1.0f, dry), std::memory_order_relaxed); }
    float getSamplerIrReverbDryLevel() const { return irReverbDryLevel.load(std::memory_order_relaxed); }

    // Delay
    void setSamplerDelay(float timeMs, float feedback, float wet)
    {
        samplerDelayTimeMs.store(juce::jlimit(1.0f, 2000.0f, timeMs), std::memory_order_relaxed);
        samplerDelayFeedback.store(juce::jlimit(0.0f, 0.95f, feedback), std::memory_order_relaxed);
        samplerDelayWetLevel.store(juce::jlimit(0.0f, 1.0f, wet), std::memory_order_relaxed);
    }
    void setSamplerDelayTimeMs(float ms) { samplerDelayTimeMs.store(juce::jlimit(1.0f, 2000.0f, ms), std::memory_order_relaxed); }
    float getSamplerDelayTimeMs() const { return samplerDelayTimeMs.load(std::memory_order_relaxed); }
    void setSamplerDelayFeedback(float fb) { samplerDelayFeedback.store(juce::jlimit(0.0f, 0.95f, fb), std::memory_order_relaxed); }
    float getSamplerDelayFeedback() const { return samplerDelayFeedback.load(std::memory_order_relaxed); }
    void setSamplerDelayWetLevel(float wet) { samplerDelayWetLevel.store(juce::jlimit(0.0f, 1.0f, wet), std::memory_order_relaxed); }
    float getSamplerDelayWetLevel() const { return samplerDelayWetLevel.load(std::memory_order_relaxed); }

    // Chorus
    void setSamplerChorus(float rateHz, float depth, float wet)
    {
        samplerChorusRate.store(juce::jlimit(0.1f, 20.0f, rateHz), std::memory_order_relaxed);
        samplerChorusDepth.store(juce::jlimit(0.0f, 1.0f, depth), std::memory_order_relaxed);
        samplerChorusWet.store(juce::jlimit(0.0f, 1.0f, wet), std::memory_order_relaxed);
    }
    void setSamplerChorusRate(float rateHz) { samplerChorusRate.store(juce::jlimit(0.1f, 20.0f, rateHz), std::memory_order_relaxed); }
    float getSamplerChorusRate() const { return samplerChorusRate.load(std::memory_order_relaxed); }
    void setSamplerChorusDepth(float depth) { samplerChorusDepth.store(juce::jlimit(0.0f, 1.0f, depth), std::memory_order_relaxed); }
    float getSamplerChorusDepth() const { return samplerChorusDepth.load(std::memory_order_relaxed); }
    void setSamplerChorusWet(float wet) { samplerChorusWet.store(juce::jlimit(0.0f, 1.0f, wet), std::memory_order_relaxed); }
    float getSamplerChorusWet() const { return samplerChorusWet.load(std::memory_order_relaxed); }

    // LFO Modulation Controls
    void setLfoFrequency(float hz) { lfoFrequency.store(juce::jlimit(0.01f, 50.0f, hz), std::memory_order_relaxed); }
    float getLfoFrequency() const { return lfoFrequency.load(std::memory_order_relaxed); }
    void setLfoAmount(float amt) { lfoAmount.store(juce::jlimit(0.0f, 1.0f, amt), std::memory_order_relaxed); }
    float getLfoAmount() const { return lfoAmount.load(std::memory_order_relaxed); }
    void setLfoShape(int shape) { lfoShape.store(juce::jlimit(0, 4, shape), std::memory_order_relaxed); }
    int getLfoShape() const { return lfoShape.load(std::memory_order_relaxed); }
    void setLfoTarget(int target) { lfoTarget.store(juce::jlimit(0, 3, target), std::memory_order_relaxed); }
    int getLfoTarget() const { return lfoTarget.load(std::memory_order_relaxed); }
    void setLfoShapeByName(const juce::String& shape);
    void setLfoTargetByName(const juce::String& target);
    void setLfoTargetName(const juce::String& name) { lfoTargetName = name; }
    juce::String getLfoTargetName() const { return lfoTargetName; }
    float getCurrentLfoOutput() const { return currentLfoOutput.load(std::memory_order_relaxed); }

    // MIDI CC & Real-time Mod Controls
    void setMidiModWheel(float amount) { midiModWheelAmount.store(juce::jlimit(0.0f, 1.0f, amount), std::memory_order_relaxed); }
    float getMidiModWheel() const { return midiModWheelAmount.load(std::memory_order_relaxed); }
    void setMidiExpression(float amount) { midiExpressionAmount.store(juce::jlimit(0.0f, 1.0f, amount), std::memory_order_relaxed); }
    float getMidiExpression() const { return midiExpressionAmount.load(std::memory_order_relaxed); }
    void setPitchBendSemis(float semitones) { midiPitchBendSemis.store(juce::jlimit(-24.0f, 24.0f, semitones), std::memory_order_relaxed); }
    float getPitchBendSemis() const { return midiPitchBendSemis.load(std::memory_order_relaxed); }

    // Group Controls
    void setGroupVolumeDb(int groupIdx, float gainDb);
    float getGroupVolumeDb(int groupIdx) const;
    void setGroupPan(int groupIdx, float pan);
    float getGroupPan(int groupIdx) const;
    void setGroupTuningCents(int groupIdx, float cents);
    float getGroupTuningCents(int groupIdx) const;
    void setGroupMuted(int groupIdx, bool muted);
    bool getGroupMuted(int groupIdx) const;
    void resetAllGroups();

    void setSamplerLowpassCutoff(float cutoffHz) { samplerLowpassCutoff.store(juce::jlimit(20.0f, 22000.0f, cutoffHz), std::memory_order_relaxed); }
    float getSamplerLowpassCutoff() const { return samplerLowpassCutoff.load(std::memory_order_relaxed); }

    void setSamplerHighpassCutoff(float cutoffHz) { samplerHighpassCutoff.store(juce::jlimit(10.0f, 20000.0f, cutoffHz), std::memory_order_relaxed); }
    float getSamplerHighpassCutoff() const { return samplerHighpassCutoff.load(std::memory_order_relaxed); }

    void setSamplerTone(float tone) {
        float tNorm = juce::jlimit(0.0f, 1.0f, tone);
        samplerTone.store(tNorm, std::memory_order_relaxed);
        float cutoff = 300.0f * std::pow(22000.0f / 300.0f, tNorm);
        setSamplerLowpassCutoff(cutoff);
    }
    float getSamplerTone() const { return samplerTone.load(std::memory_order_relaxed); }

    void setPitchTrackingEnabled(bool enabled);
    bool isPitchTrackingEnabled() const { return pitchTrackingEnabled.load(std::memory_order_relaxed); }
    void setOneShotEnabled(bool enabled);
    bool isOneShotEnabled() const { return oneShotEnabled.load(std::memory_order_relaxed); }
    void stopZoneVoice(int triggerMidiNote);
    void stopAllVoices();
    void play();
    void pause();
    void stop();
    void clearMasterSample();
    void setPositionRatio(double ratio); // 0.0 - 1.0
    void setLooping(bool shouldLoop);
    bool isLooping() const { return isLoopingAtomic.load(std::memory_order_relaxed); }
    bool isPlaying() const { return isPlayingAtomic.load(std::memory_order_relaxed); }
    double getCurrentPositionSeconds() const;
    double getTotalLengthSeconds() const { return totalDurationAtomic.load(std::memory_order_relaxed); }
    int getNumChannels() const { return numChannelsAtomic.load(std::memory_order_relaxed); }
    float getGain() const { return gainLevelAtomic.load(std::memory_order_relaxed); }
    void setGain(float newGain);

    void getMinMaxForTimeRange(double startTimeSecs, double endTimeSecs, float& minVal, float& maxVal, int channel = -1) const;
    void getMinMaxForRatioRange(double startRatio, double endRatio, float& minVal, float& maxVal, int channel = -1) const;
    WaveformPeaks getWaveformPeaks() const;

    void setSampleRange(double startRatio, double endRatio);
    double getSampleStartRatio() const { return sampleStartRatioAtomic.load(std::memory_order_relaxed); }
    double getSampleEndRatio() const { return sampleEndRatioAtomic.load(std::memory_order_relaxed); }
    bool getAudioBufferCopy(juce::AudioBuffer<float>& destBuffer, double& sampleRate) const;
    bool cropLoadedSample(double startRatio, double endRatio);
    bool normalizeLoadedSample();
    bool silenceSelection(double startRatio, double endRatio);
    bool silenceSpectralRegion(double startRatio, double endRatio, float minFreqHz, float maxFreqHz);
    bool adjustSpectralRegionGain(double startRatio, double endRatio, float minFreqHz, float maxFreqHz, float gaindB);
    bool isolateSpectralRegion(double startRatio, double endRatio, float minFreqHz, float maxFreqHz);
    bool repairSpectralRegion(double startRatio, double endRatio, float minFreqHz, float maxFreqHz);
    bool removeSpectralHarmonics(double startRatio, double endRatio, float fundamentalFreqHz);
    bool denoiseSpectralRegion(double startRatio, double endRatio, float minFreqHz, float maxFreqHz, float reductionDb = 18.0f);
    bool widenSpectralRegion(double startRatio, double endRatio, float minFreqHz, float maxFreqHz, float widthFactor = 2.0f);
    bool saturateSpectralRegion(double startRatio, double endRatio, float minFreqHz, float maxFreqHz, float driveAmount = 0.5f);
    bool reverseSelection(double startRatio, double endRatio);
    bool deverbSelection(double startRatio, double endRatio, float amount = 0.6f);
    bool adjustGainSelection(double startRatio, double endRatio, float gaindB);
    bool applyHighPassFilter(double startRatio, double endRatio, float cutoffHz = 80.0f);
    bool autoTrimSilence();
    bool invertPhaseSelection(double startRatio, double endRatio);
    bool changeSampleSpeed(double speedMultiplier);
    bool applyFadesToBuffer(double fadeInMs, int fadeInType, double fadeOutMs, int fadeOutType);
    void rebuildWaveformPeaks();

    // Non-destructive editing: snapshot original before edits, restore to revert
    void snapshotOriginalForEditing();
    bool restoreOriginal();
    bool hasOriginalSnapshot() const;

    bool getAutoPlay() const { return autoPlayOnSelect; }
    void setAutoPlay(bool enabled) { autoPlayOnSelect = enabled; }

    void setSampleBpm(double bpm);
    double getSampleBpm() const { return sampleBpm.load(std::memory_order_relaxed); }

    void setHostBpm(double bpm);
    double getHostBpm() const { return hostBpm.load(std::memory_order_relaxed); }

    // Independent Transport & Host Sync
    void setHostSyncEnabled(bool enabled);
    bool isHostSyncEnabled() const { return hostSyncEnabled.load(std::memory_order_relaxed); }
    void toggleHostSync();

    void setInternalBpm(double bpm);
    double getInternalBpm() const { return internalBpm.load(std::memory_order_relaxed); }
    double getEffectiveBpm() const { return hostSyncEnabled.load(std::memory_order_relaxed) ? hostBpm.load(std::memory_order_relaxed) : internalBpm.load(std::memory_order_relaxed); }

    void setHostTransportState(bool isPlaying, double bpm, double positionSec, double ppqPosition);
    bool getIsHostPlaying() const { return isHostPlaying.load(std::memory_order_relaxed); }
    double getHostPositionSeconds() const { return hostPositionSeconds.load(std::memory_order_relaxed); }
    double getHostPpqPosition() const { return hostPpqPosition.load(std::memory_order_relaxed); }

    void togglePlay();
    void toggleLoop();
    
    void updateVoiceRatios();

    juce::AudioThumbnail& getThumbnail() { return thumbnail; }
    juce::AudioFormatManager& getFormatManager() { return formatManager; }
    const juce::File& getCurrentFile() const { return currentFile; }
    juce::MidiKeyboardState& getKeyboardState() { return keyboardState; }
    void setMainTransportMidiEnabled(bool enabled) { mainTransportMidiEnabled.store(enabled, std::memory_order_relaxed); }
    bool isMainTransportMidiEnabled() const { return mainTransportMidiEnabled.load(std::memory_order_relaxed); }
    void setMidiInputEnabled(bool enabled) { midiInputEnabled.store(enabled, std::memory_order_relaxed); }
    bool isMidiInputEnabled() const { return midiInputEnabled.load(std::memory_order_relaxed); }

    enum class RecordingChannelMode
    {
        Stereo,
        MonoLeft,
        MonoRight
    };

    // Audio Recording Subsystem
    void startRecording();
    void stopRecording();
    bool isRecording() const { return recordingActive.load(std::memory_order_relaxed); }
    void setInputMuted(bool muted) { inputMuted.store(muted, std::memory_order_relaxed); }
    bool isInputMuted() const { return inputMuted.load(std::memory_order_relaxed); }
    void setRecordingChannelMode(RecordingChannelMode mode) { channelMode.store(mode, std::memory_order_relaxed); }
    RecordingChannelMode getRecordingChannelMode() const { return channelMode.load(std::memory_order_relaxed); }
    double getRecordingDurationSeconds() const;
    void getLiveInputLevels(float& leftLevel, float& rightLevel) const;
    bool getRecordedBufferCopy(juce::AudioBuffer<float>& destBuffer, double& sampleRate) const;
    juce::File saveRecordingToWav(const juce::String& baseFileName);
    void playMetronomeClick(bool isAccent = false);
    void setInputParametricEq(const std::array<float, 9>& freqs, const std::array<float, 9>& gains, bool lowCut);

    void triggerNoteOn(int midiNoteNumber, float velocity);
    void triggerNoteOff(int midiNoteNumber);

    void addListener(AudioEngineListener* listener);
    void removeListener(AudioEngineListener* listener);

private:
    bool pushCommand(const EngineCommand& cmd);
    void drainCommandsOnAudioThread();
    void startPlaybackInternal(double startRatio);
    void stopPlaybackInternal();
    void replaceEditedSampleInMemory();
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    juce::TimeSliceThread backgroundThread { "OpenWavBackgroundThread" };
    juce::AudioFormatManager formatManager;
    juce::AudioThumbnailCache thumbnailCache { 1000 };
    juce::AudioThumbnail thumbnail { 512, formatManager, thumbnailCache };

    // Master sample and UI data lock (NEVER ACQUIRED BY AUDIO THREAD)
    mutable juce::CriticalSection uiDataLock;
    std::shared_ptr<CachedSample> currentMasterSample;
    std::shared_ptr<AudioVoice> loadedVoice;
    WaveformPeaks cachedWaveformPeaks;

    // Non-destructive editing: backup of original buffer before edits
    juce::AudioBuffer<float> originalEditBuffer;
    double originalEditSampleRate { 0.0 };
    bool originalSnapshotValid { false };

    // Real-Time Audio Thread State (strictly lock-free, zero heap allocations)
    std::array<RealtimeVoiceSlot, MaxActiveVoices> voicePool;
    std::shared_ptr<CachedSample> activePreviewSampleOnAudioThread;

    // Lock-Free Command FIFO
    std::array<EngineCommand, CommandQueueCapacity> commandQueue;
    std::atomic<uint32_t> commandWriteIdx { 0 };
    std::atomic<uint32_t> commandReadIdx { 0 };
    juce::SpinLock commandProducerLock;

    // Lock-Free Telemetry Atomics
    std::atomic<bool> isPlayingAtomic { false };
    std::atomic<double> currentPositionAtomic { 0.0 };
    std::atomic<double> totalDurationAtomic { 0.0 };
    std::atomic<int> numChannelsAtomic { 0 };
    std::atomic<float> gainLevelAtomic { 0.8f };
    std::atomic<bool> isLoopingAtomic { false };
    std::atomic<double> sampleStartRatioAtomic { 0.0 };
    std::atomic<double> sampleEndRatioAtomic { 1.0 };
    std::atomic<double> stoppedPositionSecs { 0.0 };
    std::atomic<double> engineSampleRateAtomic { 44100.0 };
    std::atomic<double> currentFileSampleRateAtomic { 44100.0 };

    juce::File currentFile;
    bool autoPlayOnSelect { true };
    std::atomic<uint64_t> currentLoadId { 0 };

    std::atomic<double> sampleBpm { 0.0 };
    std::atomic<double> hostBpm { 120.0 };
    std::atomic<double> internalBpm { 120.0 };
    std::atomic<bool> hostSyncEnabled { false };
    std::atomic<bool> isHostPlaying { false };
    std::atomic<double> hostPositionSeconds { 0.0 };
    std::atomic<double> hostPpqPosition { 0.0 };
    bool wasHostPlaying { false };

    // Live Recording State (Pre-allocated, lock-free on audio thread)
    std::atomic<bool> recordingActive { false };
    std::atomic<bool> inputMuted { true };
    std::atomic<RecordingChannelMode> channelMode { RecordingChannelMode::Stereo };
    juce::AudioBuffer<float> recordingBuffer;
    std::atomic<int> recordingWritePosition { 0 };
    std::atomic<float> liveInputLeft { 0.0f };
    std::atomic<float> liveInputRight { 0.0f };

    juce::Reverb reverbDSP;
    juce::Reverb::Parameters reverbParams;
    std::atomic<float> samplerReverbAmount { 0.0f };
    std::atomic<float> samplerLowpassCutoff { 22000.0f };
    std::atomic<float> samplerHighpassCutoff { 10.0f };
    std::atomic<float> samplerTone { 1.0f };
    std::atomic<bool> pitchTrackingEnabled { true };
    std::atomic<bool> oneShotEnabled { false };
    std::atomic<bool> isLoadedInSampler { false };
    std::atomic<float> samplerAttackSec { 0.005f };
    std::atomic<float> samplerDecaySec { 0.1f };
    std::atomic<float> samplerSustainLevel { 1.0f };
    std::atomic<float> samplerReleaseSec { 0.2f };

    // IR Convolution Reverb
    juce::dsp::Convolution irConvolution;
    juce::AudioBuffer<float> irProcessBuffer;
    std::atomic<float> irReverbAmount { 0.0f };
    std::atomic<float> irReverbDryLevel { 1.0f };
    std::atomic<bool> irLoaded { false };
    juce::String currentLoadedIrPath;
    int reverbTailSamplesRemaining { 0 };

    // Delay & Chorus DSP Buffers
    juce::AudioBuffer<float> delayBuffer;
    int delayBufferWritePos { 0 };
    std::atomic<float> samplerDelayTimeMs { 250.0f };
    std::atomic<float> samplerDelayFeedback { 0.0f };
    std::atomic<float> samplerDelayWetLevel { 0.0f };

    juce::AudioBuffer<float> chorusBuffer;
    int chorusBufferWritePos { 0 };
    std::atomic<float> samplerChorusRate { 1.0f };
    std::atomic<float> samplerChorusDepth { 0.0f };
    std::atomic<float> samplerChorusWet { 0.0f };
    double chorusPhase { 0.0 };

    // LFO Modulation State
    std::atomic<float> lfoFrequency { 1.0f };
    std::atomic<float> lfoAmount { 0.0f };
    std::atomic<int> lfoShape { 0 };
    std::atomic<int> lfoTarget { 0 };
    std::atomic<float> currentLfoOutput { 0.0f };
    juce::String lfoTargetName { "cutoff" };
    std::atomic<float> midiModWheelAmount { 0.0f };
    std::atomic<float> midiExpressionAmount { 1.0f };
    std::atomic<float> midiPitchBendSemis { 0.0f };
    double lfoPhase { 0.0 };
    float lastRandomLfoVal { 0.0f };
    double lastRandomLfoPhase { 0.0 };

    // Group Real-Time States
    std::array<std::atomic<float>, MaxGroups> groupVolumesDb;
    std::array<std::atomic<float>, MaxGroups> groupPans;
    std::array<std::atomic<float>, MaxGroups> groupTuningsCents;
    std::array<std::atomic<bool>, MaxGroups> groupMutes;

    struct BiquadState
    {
        float x1 { 0.0f }, x2 { 0.0f }, y1 { 0.0f }, y2 { 0.0f };
    };

    BiquadState samplerLpState[2];
    BiquadState samplerHpState[2];

    std::array<std::atomic<float>, 9> eqFreqs;
    std::array<std::atomic<float>, 9> eqGains;
    std::atomic<bool> eqLowCutEnabled { false };

    BiquadState inputFilterStates[10][2]; // 10 filters, 2 channels

    std::atomic<bool> mainTransportMidiEnabled { true };
    std::atomic<bool> midiInputEnabled { true };
    juce::MidiKeyboardState keyboardState;

    mutable juce::CriticalSection cacheLock;
    std::map<juce::String, std::shared_ptr<CachedSample>> sampleCache;

    juce::ListenerList<AudioEngineListener> listeners;
    bool lastNotifiedPlayingState { false };
};

} // namespace openwav
