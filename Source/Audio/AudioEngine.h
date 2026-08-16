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
#include <cmath>
#include <algorithm>
#include <cstring>

namespace openwav
{

class AudioEngineListener
{
public:
    virtual ~AudioEngineListener() = default;
    virtual void playbackStateChanged(bool /*isPlaying*/) {}
    virtual void playbackPositionChanged(double /*currentSeconds*/, double /*totalSeconds*/) {}
    virtual void sampleLoaded(const juce::String& /*filePath*/) {}
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
};

class AudioEngine : public juce::ChangeListener
{
public:
    AudioEngine();
    ~AudioEngine() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock);
    void releaseResources();
    void processNextAudioBlock(juce::AudioBuffer<float>& outputBuffer, juce::MidiBuffer& midiMessages);

    // Audio Playback Controls
    bool loadFile(const juce::File& audioFile, bool autoPlay = false);
    void preloadSampleFiles(const std::vector<juce::File>& files);
    void putSampleInCache(const juce::String& filePath, double sampleRate, const juce::AudioBuffer<float>& buf);
    void playZoneVoice(const juce::File& file, int triggerMidiNote, int rootNote, float fineTuneCents, float gainDb, float velocity = 1.0f,
                       float attackSec = 0.005f, float decaySec = 0.1f, float sustainLevel = 1.0f, float releaseSec = 0.2f, bool isOneShot = false, bool isLooping = false);
    void setSamplerReverbAmount(float amount) { samplerReverbAmount.store(juce::jlimit(0.0f, 1.0f, amount)); }
    float getSamplerReverbAmount() const { return samplerReverbAmount.load(); }
    void setPitchTrackingEnabled(bool enabled) { pitchTrackingEnabled.store(enabled); updateVoiceRatios(); }
    bool isPitchTrackingEnabled() const { return pitchTrackingEnabled.load(); }
    void setOneShotEnabled(bool enabled) { oneShotEnabled.store(enabled); }
    bool isOneShotEnabled() const { return oneShotEnabled.load(); }
    void stopZoneVoice(int triggerMidiNote);
    void play();
    void pause();
    void stop();
    void setPositionRatio(double ratio); // 0.0 - 1.0
    void setLooping(bool shouldLoop);
    bool isLooping() const { return isLoopingEnabled; }
    bool isPlaying() const;
    double getCurrentPositionSeconds() const;
    double getTotalLengthSeconds() const;
    int getNumChannels() const;
    float getGain() const { return gainLevel; }
    void setGain(float newGain);

    void getMinMaxForTimeRange(double startTimeSecs, double endTimeSecs, float& minVal, float& maxVal, int channel = -1) const;
    void getMinMaxForRatioRange(double startRatio, double endRatio, float& minVal, float& maxVal, int channel = -1) const;
    WaveformPeaks getWaveformPeaks() const;

    void setSampleRange(double startRatio, double endRatio);
    double getSampleStartRatio() const { return sampleStartRatio; }
    double getSampleEndRatio() const { return sampleEndRatio; }
    bool getAudioBufferCopy(juce::AudioBuffer<float>& destBuffer, double& sampleRate) const;
    bool cropLoadedSample(double startRatio, double endRatio);
    bool normalizeLoadedSample();
    bool silenceSelection(double startRatio, double endRatio);
    bool reverseSelection(double startRatio, double endRatio);
    bool deverbSelection(double startRatio, double endRatio, float amount = 0.6f);
    bool applyFadesToBuffer(double fadeInMs, int fadeInType, double fadeOutMs, int fadeOutType);
    void rebuildWaveformPeaks();

    bool getAutoPlay() const { return autoPlayOnSelect; }
    void setAutoPlay(bool enabled) { autoPlayOnSelect = enabled; }

    void setSampleBpm(double bpm);
    double getSampleBpm() const { return sampleBpm.load(); }

    void setHostBpm(double bpm);
    double getHostBpm() const { return hostBpm.load(); }
    
    void updateVoiceRatios();

    juce::AudioThumbnail& getThumbnail() { return thumbnail; }
    juce::AudioFormatManager& getFormatManager() { return formatManager; }
    const juce::File& getCurrentFile() const { return currentFile; }
    juce::MidiKeyboardState& getKeyboardState() { return keyboardState; }
    void setMainTransportMidiEnabled(bool enabled) { mainTransportMidiEnabled.store(enabled); }
    bool isMainTransportMidiEnabled() const { return mainTransportMidiEnabled.load(); }

    enum class RecordingChannelMode
    {
        Stereo,
        MonoLeft,
        MonoRight
    };

    // Audio Recording Subsystem
    void startRecording();
    void stopRecording();
    bool isRecording() const { return recordingActive.load(); }
    void setInputMuted(bool muted) { inputMuted.store(muted); }
    bool isInputMuted() const { return inputMuted.load(); }
    void setRecordingChannelMode(RecordingChannelMode mode) { channelMode.store(mode); }
    RecordingChannelMode getRecordingChannelMode() const { return channelMode.load(); }
    double getRecordingDurationSeconds() const;
    void getLiveInputLevels(float& leftLevel, float& rightLevel) const;
    bool getRecordedBufferCopy(juce::AudioBuffer<float>& destBuffer, double& sampleRate) const;
    juce::File saveRecordingToWav(const juce::String& baseFileName);
    void playMetronomeClick(bool isAccent = false);
    void setInputParametricEq(const std::array<float, 9>& freqs, const std::array<float, 9>& gains, bool lowCut);

    void addListener(AudioEngineListener* listener);
    void removeListener(AudioEngineListener* listener);

private:
    void replaceEditedSampleInMemoryAndDisk();
    void triggerNoteOn(int midiNoteNumber, float velocity);
    void triggerNoteOff(int midiNoteNumber);
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    juce::TimeSliceThread backgroundThread { "OpenWavBackgroundThread" };
    juce::AudioFormatManager formatManager;
    juce::AudioThumbnailCache thumbnailCache { 1000 };
    juce::AudioThumbnail thumbnail { 512, formatManager, thumbnailCache };

    juce::CriticalSection voiceLock;
    std::vector<std::shared_ptr<AudioVoice>> activeVoices;
    std::shared_ptr<AudioVoice> loadedVoice;
    WaveformPeaks cachedWaveformPeaks;
    double engineSampleRate { 44100.0 };
    double currentFileSampleRate { 44100.0 };
    double stoppedPositionSecs { 0.0 };

    juce::File currentFile;
    float gainLevel { 0.8f };
    bool isLoopingEnabled { true };
    bool autoPlayOnSelect { true };
    std::atomic<uint64_t> currentLoadId { 0 };

    std::atomic<double> sampleBpm { 0.0 };
    std::atomic<double> hostBpm { 120.0 };

    // Live Recording State
    std::atomic<bool> recordingActive { false };
    std::atomic<bool> inputMuted { true };
    std::atomic<RecordingChannelMode> channelMode { RecordingChannelMode::Stereo };
    mutable juce::CriticalSection recordingLock;
    juce::AudioBuffer<float> recordingBuffer;
    int recordingWritePosition { 0 };
    std::atomic<float> liveInputLeft { 0.0f };
    std::atomic<float> liveInputRight { 0.0f };

    juce::Reverb reverbDSP;
    juce::Reverb::Parameters reverbParams;
    std::atomic<float> samplerReverbAmount { 0.0f };
    std::atomic<bool> pitchTrackingEnabled { true };
    std::atomic<bool> oneShotEnabled { false };
    struct BiquadState
    {
        float x1 { 0.0f }, x2 { 0.0f }, y1 { 0.0f }, y2 { 0.0f };
    };

    std::array<std::atomic<float>, 9> eqFreqs;
    std::array<std::atomic<float>, 9> eqGains;
    std::atomic<bool> eqLowCutEnabled { false };

    BiquadState inputFilterStates[10][2]; // 10 filters, 2 channels

    double sampleStartRatio { 0.0 };
    double sampleEndRatio { 1.0 };

    std::atomic<bool> mainTransportMidiEnabled { true };
    juce::MidiKeyboardState keyboardState;
    std::map<juce::String, CachedSample> sampleCache;
    juce::ListenerList<AudioEngineListener> listeners;
};

} // namespace openwav
