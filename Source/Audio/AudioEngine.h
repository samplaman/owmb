#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_audio_basics/juce_audio_basics.h>
 #include <juce_audio_formats/juce_audio_formats.h>
 #include <juce_audio_utils/juce_audio_utils.h>
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
#include "PerformanceRackDSP.h"

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
    virtual void randomLoopStateChanged(bool /*enabled*/) {}
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
    bool pitchTrackingEnabled { true };
    bool randomLoopEnabled { false };
    bool hasCustomLoopPoints { false };
    bool isMetronome { false };
    bool matchesEditorSample { false };
    double loopInRatio { 0.0 };
    double loopOutRatio { 1.0 };
    double loopCrossfadeMs { 0.0 };
    double fadeInMs { 0.0 };
    double fadeOutMs { 0.0 };
    int fadeInCurveType { 0 };
    int fadeOutCurveType { 0 };
    int activeLoopStartSample { -1 };
    int nextLoopStartSample { -1 };
    int outputBus { 0 };
    int rackSlotIndex { -1 };
    float voicePan { 0.0f };
    uint32_t rngState { 0x9E3779B9u };
    uint64_t startOrder { 0 };
};

enum class EngineCommandType : int
{
    None = 0,
    Play,
    Pause,
    Stop,
    RestartPreview,
    SeekRatio,
    SetLooping,
    SetGain,
    SetSampleRange,
    SetLoopPoints,
    SetLoopCrossfadeMs,
    SetFades,
    SetPitchTracking,
    SetOneShot,
    LoadPreviewSample,
    PlayZoneVoice,
    StopZoneVoice,
    StopAllZoneVoices,
    PlayMetronome,
    UpdateVoiceRatios
};

struct SampleEditSettings
{
    double sampleStartRatio { 0.0 };
    double sampleEndRatio { 1.0 };
    double loopInRatio { 0.0 };
    double loopOutRatio { 1.0 };
    double loopCrossfadeMs { 0.0 };
    double fadeInMs { 0.0 };
    double fadeOutMs { 0.0 };
    int fadeInCurveType { 0 };
    int fadeOutCurveType { 0 };
    bool loopMarkersSet { false };
    bool isLooping { false };
};

struct EngineCommand
{
    EngineCommandType type { EngineCommandType::None };
    int intVal1 { 0 };
    int intVal2 { 0 };
    float floatVal1 { 0.0f };
    float floatVal2 { 0.0f };
    float floatVal3 { 0.0f };
    float floatVal4 { 0.0f };
    float floatVal5 { 0.0f };
    float floatVal6 { 0.0f };
    double doubleVal1 { 0.0 };
    double doubleVal2 { 1.0 };
    bool boolVal1 { false };
    bool boolVal2 { false };
    bool boolVal3 { false };
    bool pitchTrackingEnabled { true };
    bool randomLoopEnabled { false };
    int outputBus { 0 };
    int rackSlotIndex { -1 };
    float voicePan { 0.0f };
    SampleEditSettings editSettings;
    std::shared_ptr<CachedSample> sampleData;
};

class AudioEngine : public juce::ChangeListener, public juce::Timer
{
public:
    static constexpr int MaxActiveVoices = 32;
    static constexpr int CommandQueueCapacity = 1024;
    static constexpr int MaxOutputBuses = 8;
    static constexpr int MaxSamplerRackSlots = 16;

    static std::unique_ptr<juce::AudioFormatReader> createAudioReaderForFile(const juce::File& file);
    static juce::String getDefaultOutputBusName(int busIdx);

    AudioEngine();
    ~AudioEngine() override;

    void timerCallback() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock);
    void releaseResources();
    void processNextAudioBlock(juce::AudioBuffer<float>& outputBuffer, juce::MidiBuffer& midiMessages);
    void processNextAudioBlock(juce::AudioBuffer<float>& outputBuffer,
                               juce::AudioBuffer<float>* auxBusBuffers,
                               int numAuxBuses,
                               juce::MidiBuffer& midiMessages);

    // Audio Playback Controls
    bool loadFile(const juce::File& audioFile, bool autoPlay = false, bool isSamplerSample = false);
    bool loadAudioBuffer(const juce::String& displayName, const juce::AudioBuffer<float>& buffer, double sampleRate, int rootNote = 60, bool isSamplerSample = true);
    bool isCurrentSampleInSampler() const { return isLoadedInSampler.load(std::memory_order_relaxed); }
    void setLoadedInSampler(bool inSampler) { isLoadedInSampler.store(inSampler, std::memory_order_relaxed); }
    void preloadSampleFiles(const std::vector<juce::File>& files);
    void putSampleInCache(const juce::String& filePath, double sampleRate, const juce::AudioBuffer<float>& buf);
    bool getCachedSampleCopy(const juce::String& filePath, juce::AudioBuffer<float>& destBuffer, double& sampleRate) const;
    void playZoneVoice(const juce::File& file, int triggerMidiNote, int rootNote, float fineTuneCents, float gainDb, float velocity = 1.0f,
                       float attackSec = 0.005f, float decaySec = 0.1f, float sustainLevel = 1.0f, float releaseSec = 0.2f,
                       bool isOneShot = false, bool isLooping = false, bool updateEditorUi = true, int outputBus = 0,
                       int rackSlotIndex = -1, float voicePan = 0.0f,
                       const SampleEditSettings* explicitEditSettings = nullptr,
                       int pitchTrackingOverride = -1,
                       int randomLoopOverride = -1,
                       bool useExplicitOneShot = false);

    // Multi-Output Routing & 8-Bus Mixer Controls (Lock-Free)
    void setDefaultSamplerOutputBus(int busIdx) { defaultSamplerBusAtomic.store(juce::jlimit(0, MaxOutputBuses - 1, busIdx), std::memory_order_relaxed); }
    int getDefaultSamplerOutputBus() const { return defaultSamplerBusAtomic.load(std::memory_order_relaxed); }
    void setFoldInactiveBusesToMain(bool enabled) { foldInactiveBusesToMainAtomic.store(enabled, std::memory_order_relaxed); }
    bool getFoldInactiveBusesToMain() const { return foldInactiveBusesToMainAtomic.load(std::memory_order_relaxed); }

    void setBusGainDb(int busIdx, float db);
    float getBusGainDb(int busIdx) const;
    void setBusPan(int busIdx, float pan);
    float getBusPan(int busIdx) const;
    void setBusMute(int busIdx, bool muted);
    bool getBusMute(int busIdx) const;
    void setBusSolo(int busIdx, bool soloed);
    bool getBusSolo(int busIdx) const;
    void setBusSendToFxRack(int busIdx, bool enabled);
    bool getBusSendToFxRack(int busIdx) const;

    void setMasterMixerGainDb(float db) { masterMixerGainDbAtomic.store(juce::jlimit(-60.0f, 6.0f, db), std::memory_order_relaxed); }
    float getMasterMixerGainDb() const { return masterMixerGainDbAtomic.load(std::memory_order_relaxed); }
    void setMasterMixerPan(float pan) { masterMixerPanAtomic.store(juce::jlimit(-1.0f, 1.0f, pan), std::memory_order_relaxed); }
    float getMasterMixerPan() const { return masterMixerPanAtomic.load(std::memory_order_relaxed); }
    void setMasterMixerMute(bool muted) { masterMixerMuteAtomic.store(muted, std::memory_order_relaxed); }
    bool getMasterMixerMute() const { return masterMixerMuteAtomic.load(std::memory_order_relaxed); }

    void setHostBusActive(int busIdx, bool active);
    bool isHostBusActive(int busIdx) const;
    void consumeBusPeaks(int busIdx, float& outLeft, float& outRight);
    void consumeMasterPeaks(float& outLeft, float& outRight);
    void setSamplerReverbAmount(float amount) { samplerReverbAmount.store(juce::jlimit(0.0f, 1.0f, amount), std::memory_order_relaxed); }
    float getSamplerReverbAmount() const { return samplerReverbAmount.load(std::memory_order_relaxed); }
    void setPitchTrackingEnabled(bool enabled);
    bool isPitchTrackingEnabled() const { return pitchTrackingEnabled.load(std::memory_order_relaxed); }
    void setPitchBend(float semitones) { pitchBendMultiplierAtomic.store(std::pow(2.0, static_cast<double>(semitones) / 12.0), std::memory_order_relaxed); }
    void setOneShotEnabled(bool enabled);
    bool isOneShotEnabled() const { return oneShotEnabled.load(std::memory_order_relaxed); }
    void stopZoneVoice(int triggerMidiNote, int rackSlotIndex = -1);
    float consumeRackSlotPeak(int slotIdx);
    void stopAllZoneVoices(bool immediateCut = false);
    void stopAllVoices();
    void drainPendingCommandsNow() { drainCommandsOnAudioThread(); }
    void play();
    void pause();
    void stop();
    void restartPlayback();
    void auditionFile(const juce::File& audioFile);
    void auditionHover(const juce::File& audioFile);
    void clearMasterSample();
    void setPositionRatio(double ratio); // 0.0 - 1.0
    void setLooping(bool shouldLoop);
    bool isLooping() const { return isLoopingAtomic.load(std::memory_order_relaxed); }
    void setRandomLoopPositionEnabled(bool enabled);
    bool isRandomLoopPositionEnabled() const { return randomLoopEnabledAtomic.load(std::memory_order_relaxed); }
    bool getActiveRandomLoopWindow(double& outStartRatio, double& outEndRatio) const;
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
    void setLoopPoints(double inRatio, double outRatio);
    double getLoopInRatio() const { return loopInRatioAtomic.load(std::memory_order_relaxed); }
    double getLoopOutRatio() const { return loopOutRatioAtomic.load(std::memory_order_relaxed); }
    void setLoopMarkersSet(bool active);
    bool isLoopMarkersSet() const { return loopMarkersSetAtomic.load(std::memory_order_relaxed); }
    void setLoopCrossfadeMs(double ms);
    double getLoopCrossfadeMs() const { return loopCrossfadeMsAtomic.load(std::memory_order_relaxed); }
    void setFades(double inMs, int inCurve, double outMs, int outCurve);
    double getFadeInMs() const { return fadeInMsAtomic.load(std::memory_order_relaxed); }
    double getFadeOutMs() const { return fadeOutMsAtomic.load(std::memory_order_relaxed); }
    int getFadeInCurve() const { return fadeInCurveAtomic.load(std::memory_order_relaxed); }
    int getFadeOutCurve() const { return fadeOutCurveAtomic.load(std::memory_order_relaxed); }
    void setSampleEditSettings(const juce::String& filePath, const SampleEditSettings& settings);
    bool getSampleEditSettings(const juce::String& filePath, SampleEditSettings& outSettings) const;
    static bool extractMetadataLoopAndRoot(const juce::File& file,
                                           const juce::AudioFormatReader* existingReader,
                                           double& outLoopInRatio,
                                           double& outLoopOutRatio,
                                           double& outLoopCrossfadeMs,
                                           bool& outHasLoopPoints,
                                           int& outRootNote);
    std::map<juce::String, SampleEditSettings> getAllSampleEditSettings() const;
    void setAllSampleEditSettings(const std::map<juce::String, SampleEditSettings>& settingsMap);
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
    bool applyLowPassFilter(double startRatio, double endRatio, float cutoffHz = 8000.0f);
    bool removeDcOffsetSelection(double startRatio, double endRatio);
    bool monoSumSelection(double startRatio, double endRatio);
    bool autoTrimSilence();
    bool invertPhaseSelection(double startRatio, double endRatio);
    bool changeSampleSpeed(double speedMultiplier);
    bool applyFadesToBuffer(double fadeInMs, int fadeInType, double fadeOutMs, int fadeOutType);
    void rebuildWaveformPeaks();

    // Non-destructive editing: snapshot original before edits, restore to revert
    void snapshotOriginalForEditing();
    bool restoreOriginal();
    bool restoreBufferSnapshot(const juce::AudioBuffer<float>& buf, double sr);
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
    std::function<void()> onStopped;
    std::function<void(const juce::File&)> onMidiFileAudition;
    std::function<bool(const juce::File&)> isMidiFileCurrentlyPlaying;
    std::function<double()> getMidiAuditionProgressRatio;
    
    void updateVoiceRatios();

    juce::AudioThumbnail& getThumbnail() { return thumbnail; }
    juce::AudioFormatManager& getFormatManager() { return formatManager; }
    const juce::File& getCurrentFile() const { return currentFile; }
    juce::String getLastPlayedSampleId() const;
    juce::MidiKeyboardState& getKeyboardState() { return keyboardState; }
    void setMainTransportMidiEnabled(bool enabled) { mainTransportMidiEnabled.store(enabled, std::memory_order_relaxed); }
    bool isMainTransportMidiEnabled() const { return mainTransportMidiEnabled.load(std::memory_order_relaxed); }
    void setMidiInputEnabled(bool enabled) { midiInputEnabled.store(enabled, std::memory_order_relaxed); }
    bool isMidiInputEnabled() const { return midiInputEnabled.load(std::memory_order_relaxed); }

    PerformanceRackDSP& getPerformanceRack() { return performanceRack; }
    const PerformanceRackDSP& getPerformanceRack() const { return performanceRack; }

    enum class RecordingChannelMode
    {
        Stereo,
        MonoLeft,
        MonoRight
    };

    // Audio Recording Subsystem
    void startRecording(bool resetPosition = true);
    void pauseRecording();
    void stopRecording();
    void clearRecording();
    bool trimRecording(double startRatio, double endRatio);
    bool isRecording() const { return recordingActive.load(std::memory_order_relaxed); }
    void setInputListeningEnabled(bool enabled) { inputListeningEnabled.store(enabled, std::memory_order_relaxed); }
    bool isInputListeningEnabled() const { return inputListeningEnabled.load(std::memory_order_relaxed); }
    void setInputMuted(bool muted) { inputMuted.store(muted, std::memory_order_relaxed); }
    bool isInputMuted() const { return inputMuted.load(std::memory_order_relaxed); }
    void setInputGainDb(float db) { inputGainDbAtomic.store(juce::jlimit(-24.0f, 24.0f, db), std::memory_order_relaxed); }
    float getInputGainDb() const { return inputGainDbAtomic.load(std::memory_order_relaxed); }
    void setRecordingChannelMode(RecordingChannelMode mode) { channelMode.store(mode, std::memory_order_relaxed); }
    RecordingChannelMode getRecordingChannelMode() const { return channelMode.load(std::memory_order_relaxed); }
    double getRecordingDurationSeconds() const;
    void getLiveInputLevels(float& leftLevel, float& rightLevel) const;
    bool getRecordedBufferCopy(juce::AudioBuffer<float>& destBuffer, double& sampleRate) const;
    juce::File saveRecordingToWav(const juce::String& baseFileName);
    void playMetronomeClick(bool isAccent = false);
    void setInputParametricEq(const std::array<float, 9>& freqs, const std::array<float, 9>& gains, bool lowCut);

    void triggerNoteOn(int midiNoteNumber, float velocity = 1.0f);
    void triggerMidiAuditionNoteOn(int midiNoteNumber, float velocity = 1.0f);
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
    std::shared_ptr<CachedSample> defaultMidiSynthSample;
    void initDefaultMidiSynthSample();
    std::shared_ptr<AudioVoice> loadedVoice;
    WaveformPeaks cachedWaveformPeaks;
    std::map<juce::String, WaveformPeaks> waveformPeaksCache;
    juce::String lastNotifiedSampleId;
    std::atomic<const CachedSample*> activeEditorSamplePtr { nullptr };

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
    std::atomic<bool> randomLoopEnabledAtomic { false };
    std::atomic<double> activeRandomLoopStartAtomic { -1.0 };
    std::atomic<double> activeRandomLoopEndAtomic { -1.0 };
    std::atomic<double> sampleStartRatioAtomic { 0.0 };
    std::atomic<double> sampleEndRatioAtomic { 1.0 };
    std::atomic<double> loopInRatioAtomic { 0.0 };
    std::atomic<double> loopOutRatioAtomic { 1.0 };
    std::atomic<bool> loopMarkersSetAtomic { false };
    std::atomic<double> loopCrossfadeMsAtomic { 0.0 };
    std::atomic<double> fadeInMsAtomic { 0.0 };
    std::atomic<double> fadeOutMsAtomic { 0.0 };
    std::atomic<int> fadeInCurveAtomic { 0 };
    std::atomic<int> fadeOutCurveAtomic { 0 };
    std::atomic<double> stoppedPositionSecs { 0.0 };
    std::atomic<double> engineSampleRateAtomic { 44100.0 };
    std::atomic<double> currentFileSampleRateAtomic { 44100.0 };
    std::atomic<double> pitchBendMultiplierAtomic { 1.0 };

    juce::File currentFile;
    juce::File loadedMasterFile;
    std::atomic<bool> loadInProgress { false };
    std::atomic<bool> pendingAutoPlay { false };
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
    std::atomic<bool> inputListeningEnabled { false };
    std::atomic<bool> inputMuted { true };
    std::atomic<float> inputGainDbAtomic { 0.0f };
    std::atomic<RecordingChannelMode> channelMode { RecordingChannelMode::Stereo };
    juce::AudioBuffer<float> recordingBuffer;
    std::atomic<int> recordingWritePosition { 0 };
    std::atomic<float> liveInputLeft { 0.0f };
    std::atomic<float> liveInputRight { 0.0f };

    juce::Reverb reverbDSP;
    juce::Reverb::Parameters reverbParams;
    std::atomic<float> samplerReverbAmount { 0.0f };
    std::atomic<bool> pitchTrackingEnabled { true };
    std::atomic<bool> oneShotEnabled { false };
    std::atomic<bool> isLoadedInSampler { false };

    struct BiquadState
    {
        float x1 { 0.0f }, x2 { 0.0f }, y1 { 0.0f }, y2 { 0.0f };
    };

    std::array<std::atomic<float>, 9> eqFreqs;
    std::array<std::atomic<float>, 9> eqGains;
    std::atomic<bool> eqLowCutEnabled { false };

    BiquadState inputFilterStates[10][2]; // 10 filters, 2 channels

    std::atomic<bool> mainTransportMidiEnabled { true };
    std::atomic<bool> midiInputEnabled { true };
    juce::MidiKeyboardState keyboardState;

    mutable juce::CriticalSection cacheLock;
    std::map<juce::String, std::shared_ptr<CachedSample>> sampleCache;
    std::map<juce::String, SampleEditSettings> perSampleEditSettings;
    void updateCurrentSampleEditSettings();
    void applyEditSettingsAtomicsForFile(const juce::String& filePath);
    bool seedLoopSettingsFromMetadataIfUnset(const juce::String& filePath,
                                             const juce::AudioFormatReader* reader,
                                             int* outRootNote = nullptr);
    void syncUiToPlayedSample(const juce::File& audioFile, const std::shared_ptr<CachedSample>& cached);
    void syncUiToSynthSample(const juce::String& displayLabel, const std::shared_ptr<CachedSample>& sample);

    PerformanceRackDSP performanceRack;
    juce::AudioBuffer<float> zoneBuffer;
    std::array<juce::AudioBuffer<float>, MaxOutputBuses> busBuffers;

    std::atomic<int> defaultSamplerBusAtomic { 0 };
    std::atomic<bool> foldInactiveBusesToMainAtomic { true };
    std::array<std::atomic<float>, MaxOutputBuses> busGainDbAtomic {};
    std::array<std::atomic<float>, MaxOutputBuses> busPanAtomic {};
    std::array<std::atomic<bool>, MaxOutputBuses> busMuteAtomic {};
    std::array<std::atomic<bool>, MaxOutputBuses> busSoloAtomic {};
    std::array<std::atomic<bool>, MaxOutputBuses> busSendToFxRackAtomic {};
    std::array<std::atomic<float>, MaxOutputBuses> busPeakLeftAtomic {};
    std::array<std::atomic<float>, MaxOutputBuses> busPeakRightAtomic {};
    std::array<std::atomic<bool>, MaxOutputBuses> hostBusActiveAtomic {};
    std::atomic<float> masterMixerGainDbAtomic { 0.0f };
    std::atomic<float> masterMixerPanAtomic { 0.0f };
    std::atomic<bool> masterMixerMuteAtomic { false };
    std::atomic<float> masterPeakLeftAtomic { 0.0f };
    std::atomic<float> masterPeakRightAtomic { 0.0f };
    std::array<std::atomic<float>, MaxSamplerRackSlots> rackSlotPeakAtomic {};

    juce::ListenerList<AudioEngineListener> listeners;
    bool lastNotifiedPlayingState { false };

    class HoverAuditionWorker : public juce::Thread
    {
    public:
        explicit HoverAuditionWorker(AudioEngine& ownerEngine)
            : juce::Thread("OpenWavHoverAudition"), owner(ownerEngine) {}
        void run() override { owner.runHoverAuditionLoop(); }
    private:
        AudioEngine& owner;
    };

    void runHoverAuditionLoop();
    void cancelPendingHoverAudition();

    juce::AudioFormatManager hoverFormatManager;
    juce::CriticalSection hoverLock;
    juce::WaitableEvent hoverWakeEvent;
    juce::File pendingHoverFile;
    uint64_t hoverRequestSeq { 0 };
    uint64_t hoverProcessedSeq { 0 };
    std::atomic<uint64_t> hoverCancelGeneration { 0 };
    HoverAuditionWorker hoverWorker { *this };

    std::atomic<bool> isShuttingDown { false };
    std::shared_ptr<std::atomic<bool>> engineAliveToken = std::make_shared<std::atomic<bool>>(true);
    std::atomic<int> activePreloadThreads { 0 };
};

} // namespace openwav
