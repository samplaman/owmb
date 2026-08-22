#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_audio_processors/juce_audio_processors.h>
#endif
#include "Database/TagDatabaseManager.h"
#include "Scanner/LibraryScanner.h"
#include "Audio/AudioEngine.h"
#include "Models/PluginState.h"

namespace openwav
{

class OpenWavAudioProcessor : public juce::AudioProcessor
{
public:
    OpenWavAudioProcessor();
    ~OpenWavAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "OpenWav Media Browser"; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return std::numeric_limits<double>::infinity(); }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    TagDatabaseManager& getDatabaseManager() { return dbManager; }
    LibraryScanner& getLibraryScanner() { return libraryScanner; }
    AudioEngine& getAudioEngine() { return audioEngine; }

    PerformanceState& getPerformanceState() { return performanceState; }
    const PerformanceState& getPerformanceState() const { return performanceState; }
    void setPerformanceState(const PerformanceState& s) { performanceState = s; }

    EditComponentState& getEditState() { return editState; }
    const EditComponentState& getEditState() const { return editState; }
    void setEditState(const EditComponentState& s) { editState = s; }

    SampleMapState& getSampleMapState() { return sampleMapState; }
    const SampleMapState& getSampleMapState() const { return sampleMapState; }
    void setSampleMapState(const SampleMapState& s) { sampleMapState = s; }

private:
    TagDatabaseManager dbManager;
    LibraryScanner libraryScanner { dbManager };
    AudioEngine audioEngine;

    PerformanceState performanceState;
    EditComponentState editState;
    SampleMapState sampleMapState;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenWavAudioProcessor)
};

} // namespace openwav
