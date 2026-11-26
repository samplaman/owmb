#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_audio_processors/juce_audio_processors.h>
 #include <juce_audio_formats/juce_audio_formats.h>
 #include <juce_core/juce_core.h>
 #include <juce_events/juce_events.h>
#endif

#include "../Models/PluginState.h"
#include <atomic>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

namespace openwav
{

struct AutoSamplerConfig
{
    juce::String instrumentName { "AutoSampled Instrument" };
    juce::File outputDirectory;

    int minNote { 36 };           // C2
    int maxNote { 84 };           // C6
    int semitoneStride { 3 };     // 1, 2, 3, 4, 6, 12
    int velocityLayers { 1 };     // 1..8
    int roundRobinCount { 1 };    // 1..8
    int midiChannel { 1 };        // 1..16

    float sustainSeconds { 1.5f };
    float maxTailSeconds { 3.0f };
    bool autoTailDetect { true };
    bool trimLeadingSilence { true };
    int normalizeMode { 1 };      // 0 = Off, 1 = Group Peak (-1 dBFS), 2 = Per-Sample (-1 dBFS)
    double sampleRate { 44100.0 };
    bool exportSfz { true };
    bool captureReleaseTriggers { false };
};

class AutoSamplerEngine : private juce::Timer
{
public:
    AutoSamplerEngine();
    ~AutoSamplerEngine();

    // Plugin Discovery & Loading
    void loadKnownPlugins();
    void saveKnownPlugins() const;
    std::vector<juce::PluginDescription> getInstrumentPlugins() const;

    void scanPluginsAsync(std::function<void(float progress, const juce::String& pluginName)> progressCb,
                          std::function<void(int numInstrumentsFound)> completionCb);
    void cancelScan();
    bool isScanning() const { return scanning.load(std::memory_order_acquire); }

    std::unique_ptr<juce::AudioPluginInstance> createPluginInstance(const juce::PluginDescription& desc,
                                                                    double sampleRate,
                                                                    int blockSize,
                                                                    juce::String& outError);

    // Offline Auto-Sampling
    void startAutoSample(juce::AudioPluginInstance* pluginInstance,
                         const AutoSamplerConfig& config,
                         std::function<void(float progress, const juce::String& status)> progressCb,
                         std::function<void(const SampleMapState& resultState, bool success, const juce::String& error)> completionCb);

    void cancelSampling();
    void stopSamplingAndWait();
    bool isSampling() const { return sampling.load(std::memory_order_acquire); }

    static juce::File getDefaultOutputBaseDirectory();
    static juce::String midiNoteToName(int midiNote);

private:
    void timerCallback() override;
    void advanceScanFormat();
    void finishScan(bool cancelled);

    juce::File getKnownPluginsFile() const;
    juce::File getDeadMansPedalFile() const;

    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPluginList;
    mutable juce::CriticalSection pluginListLock;

    std::atomic<bool> scanning { false };
    std::unique_ptr<juce::PluginDirectoryScanner> activeScanner;
    int scanFormatIndex { 0 };
    std::function<void(float, const juce::String&)> scanProgressCb;
    std::function<void(int)> scanCompletionCb;

    std::atomic<bool> sampling { false };
    std::atomic<bool> cancelSampleRequested { false };
    std::unique_ptr<std::thread> sampleThread;

    std::shared_ptr<std::atomic<bool>> aliveToken = std::make_shared<std::atomic<bool>>(true);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AutoSamplerEngine)
};

} // namespace openwav
