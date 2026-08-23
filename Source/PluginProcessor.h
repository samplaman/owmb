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

class OpenWavAudioProcessor : public juce::AudioProcessor,
                               public juce::MidiKeyboardState::Listener
{
public:
    OpenWavAudioProcessor();
    ~OpenWavAudioProcessor() override;

    void handleNoteOn(juce::MidiKeyboardState* state, int midiChannel, int midiNoteNumber, float velocity) override;
    void handleNoteOff(juce::MidiKeyboardState* state, int midiChannel, int midiNoteNumber, float velocity) override;

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

    EditComponentState getEditState() const;
    void setEditState(const EditComponentState& s);

    SampleMapState getSampleMapState() const;
    void setSampleMapState(const SampleMapState& s);

    PluginFullState getFullPluginState() const;
    void setFullPluginState(const PluginFullState& s);

private:
    TagDatabaseManager dbManager;
    LibraryScanner libraryScanner { dbManager };
    AudioEngine audioEngine;

    mutable juce::CriticalSection stateLock;
    PluginFullState fullPluginState;
    EditComponentState editState;
    SampleMapState sampleMapState;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenWavAudioProcessor)
};

} // namespace openwav
