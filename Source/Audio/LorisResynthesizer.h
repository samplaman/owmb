#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_audio_basics/juce_audio_basics.h>
 #include <juce_audio_formats/juce_audio_formats.h>
 #include <juce_core/juce_core.h>
#endif

#include <vector>
#include <memory>
#include <functional>
#include <atomic>
#include <thread>
#include <string>

namespace openwav
{

struct ResynthesizedZone
{
    juce::File audioFile;
    juce::String sampleName;
    int rootNote { 60 };
    int keyLow { 48 };
    int keyHigh { 72 };
    double sampleRate { 44100.0 };
    int numChannels { 1 };
    int numSamples { 0 };
};

struct LorisResynthesisConfig
{
    juce::File sourceFile;
    juce::AudioBuffer<float> sourceBuffer;
    double sampleRate { 44100.0 };
    juce::String baseSampleName { "Resynthesized" };

    int rootNote { 60 };          // MIDI 0-127
    bool autoDetectRoot { true };  // Calculate root pitch if true
    int minNote { 36 };           // C2
    int maxNote { 84 };           // C6
    int noteStride { 3 };         // 1 = chromatic, 2 = whole tone, 3 = minor 3rd, 12 = octave

    bool preserveFormants { true };// Keep spectral envelope stationary
    double freqResolutionHz { 0.0 }; // 0.0 = auto based on root note (approx 0.8 * f0)
    double windowWidthHz { 0.0 };    // 0.0 = auto based on resolution (approx 2.0 * res)
    double freqDriftHz { 0.0 };      // 0.0 = auto based on resolution (approx 0.5 * res)
    
    float timeStretchRatio { 1.0f }; // 1.0 = same duration as source
    float gainDb { 0.0f };           // Post-synthesis gain adjustment
    
    juce::File outputDirectory;      // Directory to write generated WAV files
};

class LorisResynthesizer
{
public:
    LorisResynthesizer();
    ~LorisResynthesizer();

    static int detectRootMidiNote(const juce::AudioBuffer<float>& buffer, double sampleRate);
    static juce::String getMidiNoteName(int midiNoteNumber, bool includeOctave = true);

    void cancel();
    bool isRunning() const { return processing.load(std::memory_order_relaxed); }

    // Run resynthesis asynchronously on a background thread
    void startResynthesis(
        const LorisResynthesisConfig& config,
        std::function<void(float progress, const juce::String& statusText)> progressCallback,
        std::function<void(const std::vector<ResynthesizedZone>& zones, bool success, const juce::String& errorMsg)> completionCallback);

private:
    void runProcessing(
        LorisResynthesisConfig config,
        std::function<void(float, const juce::String&)> progressCallback,
        std::function<void(const std::vector<ResynthesizedZone>&, bool, const juce::String&)> completionCallback);

    std::atomic<bool> processing { false };
    std::atomic<bool> cancelRequested { false };
    std::unique_ptr<std::thread> workerThread;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LorisResynthesizer)
};

} // namespace openwav
