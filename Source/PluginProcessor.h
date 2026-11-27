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
#include <atomic>
#include <functional>
#include <vector>

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

    SamplerRackState getSamplerRackState() const;
    void setSamplerRackState(const SamplerRackState& r, bool preloadSamples = true);
    void triggerRackSlotNoteOn(int slotIndex, int midiNoteNumber, float velocity = 0.85f);
    void triggerRackSlotNoteOff(int slotIndex, int midiNoteNumber);

    MultiOutMixerState getMixerState() const;
    void setMixerState(const MultiOutMixerState& m);

    bool isPluginWrapper() const;
    juce::String getWrapperTypeDescription() const;
    int getActiveOutputBusCount() const;

    PluginFullState getFullPluginState() const;
    void setFullPluginState(const PluginFullState& s, bool preloadSamples = false);

    // MIDI File Playback
    struct MidiDisplayNote
    {
        int noteNumber { 60 };
        double startSec { 0.0 };
        double endSec { 0.0 };
        float velocity { 0.8f };
    };
    bool loadAndPlayMidiFile(const juce::File& midiFile);
    void stopMidiFilePlayback(bool stopAudioVoices = true);
    void pauseMidiFilePlayback();
    void toggleMidiFilePlayback();
    void seekMidiFilePlayback(double normalizedRatio);
    void clearLoadedMidiFile();
    bool isMidiFilePlaying() const { return midiPlaybackActive.load(std::memory_order_relaxed); }
    bool hasLoadedMidiFile() const { return midiSelectedForTransport.load(std::memory_order_relaxed); }
    double getMidiPlaybackTimeSeconds() const { return midiPlayTimeAtomic.load(std::memory_order_relaxed); }
    double getMidiFileDurationSeconds() const { return midiFileDurationAtomic.load(std::memory_order_relaxed); }
    juce::String getLoadedMidiFileName() const;
    juce::String getLoadedMidiFilePath() const;
    std::vector<MidiDisplayNote> getMidiDisplayNotes() const;

    // MIDI Learn (CC Mapping for Performance Rack)
    struct MidiCcBinding
    {
        int ccNumber { -1 };
        int effectIndex { -1 };
        juce::String paramId;
        float minVal { 0.0f };
        float maxVal { 1.0f };
    };
    void startMidiLearn(int effectIndex, const juce::String& paramId, float minVal, float maxVal);
    void clearMidiLearnForParam(int effectIndex, const juce::String& paramId);
    int getLearnedCcForParam(int effectIndex, const juce::String& paramId) const;
    std::function<void(int cc, int effectIndex, const juce::String& paramId)> onMidiLearned;

    // MIDI Output Device
    void setMidiOutputDevice(const juce::String& deviceId);
    juce::String getMidiOutputDeviceId() const { return midiOutputDeviceId; }

private:
    TagDatabaseManager dbManager;
    LibraryScanner libraryScanner { dbManager };
    AudioEngine audioEngine;

    mutable juce::CriticalSection stateLock;
    PluginFullState fullPluginState;
    EditComponentState editState;
    SampleMapState sampleMapState;

    std::map<int, int> noteRoundRobinCounters;
    std::map<int, int> lastRandomZoneIndex;
    std::map<std::pair<int, int>, int> rackNoteRoundRobinCounters;
    std::map<std::pair<int, int>, int> rackLastRandomZoneIndex;

    void processIncomingMidiMessage(const juce::MidiMessage& message, bool isInternalMidiFile = false);
    void triggerNoteOnInternal(int midiChannel, int midiNoteNumber, float velocity, bool isInternalMidiFile);
    void triggerNoteOffInternal(int midiChannel, int midiNoteNumber, float velocity, bool isInternalMidiFile);
    bool triggerSingleRackSlotNoteOn(int slotIndex, const SamplerRackSlotState& slot, int midiNoteNumber, float velocity, bool isInternalMidiFile);
    void triggerSingleRackSlotNoteOff(int slotIndex, const SamplerRackSlotState& slot, int midiNoteNumber, float relVel);
    void clearAllHeldMidiState();
    std::array<std::array<bool, 128>, 16> physicalNotesHeld {};
    std::array<std::array<bool, 128>, 16> sustainedNotes {};
    std::array<std::array<float, 128>, 16> lastNoteOnVelocities {};
    std::array<bool, 16> sustainPedalDown {};

    // MIDI File Playback State
    mutable juce::CriticalSection midiSeqLock;
    juce::MidiMessageSequence midiPlaySequence;
    int midiPlayEventIndex { 0 };
    double midiPlayTimeSec { 0.0 };
    std::atomic<double> midiPlayTimeAtomic { 0.0 };
    std::atomic<double> midiFileDurationAtomic { 0.0 };
    juce::String loadedMidiFileName;
    juce::String loadedMidiFilePath;
    std::vector<MidiDisplayNote> midiDisplayNotes;
    std::atomic<bool> midiPlaybackActive { false };
    std::atomic<bool> midiSelectedForTransport { false };

    // MIDI Learn State
    mutable juce::CriticalSection midiCcLock;
    std::vector<MidiCcBinding> midiCcBindings;
    bool midiLearnPending { false };
    MidiCcBinding pendingLearnTarget;

    // MIDI Output
    juce::CriticalSection midiOutLock;
    std::unique_ptr<juce::MidiOutput> activeMidiOutput;
    juce::String midiOutputDeviceId;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenWavAudioProcessor)
};

} // namespace openwav
