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

struct AudioVoice
{
    juce::AudioBuffer<float> buffer;
    double readPosition { 0.0 };
    double ratio { 1.0 };
    bool isLooping { false };
    bool finished { false };
};

class AudioEngine : public juce::ChangeListener
{
public:
    AudioEngine();
    ~AudioEngine() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock);
    void releaseResources();
    void processNextAudioBlock(juce::AudioBuffer<float>& outputBuffer);

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
    float getGain() const { return gainLevel; }
    void setGain(float newGain);

    bool getAutoPlay() const { return autoPlayOnSelect; }
    void setAutoPlay(bool enabled) { autoPlayOnSelect = enabled; }

    juce::AudioThumbnail& getThumbnail() { return thumbnail; }
    const juce::File& getCurrentFile() const { return currentFile; }

    void addListener(AudioEngineListener* listener);
    void removeListener(AudioEngineListener* listener);

private:
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    juce::TimeSliceThread backgroundThread { "OpenWavBackgroundThread" };
    juce::AudioFormatManager formatManager;
    juce::AudioThumbnailCache thumbnailCache { 1000 };
    juce::AudioThumbnail thumbnail { 512, formatManager, thumbnailCache };

    juce::CriticalSection voiceLock;
    std::vector<std::shared_ptr<AudioVoice>> activeVoices;
    double engineSampleRate { 44100.0 };

    juce::File currentFile;
    float gainLevel { 0.8f };
    bool isLoopingEnabled { false };
    bool autoPlayOnSelect { true };
    std::atomic<uint64_t> currentLoadId { 0 };

    juce::ListenerList<AudioEngineListener> listeners;
};

} // namespace openwav
