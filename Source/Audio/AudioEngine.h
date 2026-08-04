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

namespace openwav
{

class AudioEngineListener
{
public:
    virtual ~AudioEngineListener() = default;
    virtual void playbackStateChanged(bool isPlaying) = 0;
    virtual void playbackPositionChanged(double currentSeconds, double totalSeconds) = 0;
    virtual void sampleLoaded(const juce::String& filePath) = 0;
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
    double startRatio { 0.0 };
    double endRatio { 1.0 };
    int rootNote { 60 };
    int triggerMidiNote { -1 };
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
    bool normalizeLoadedSample();

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

    void addListener(AudioEngineListener* listener);
    void removeListener(AudioEngineListener* listener);

private:
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

    double sampleStartRatio { 0.0 };
    double sampleEndRatio { 1.0 };

    juce::ListenerList<AudioEngineListener> listeners;
};

} // namespace openwav
