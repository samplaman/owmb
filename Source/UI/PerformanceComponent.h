#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_gui_basics/juce_gui_basics.h>
#endif
#include "../Audio/AudioEngine.h"
#include "../Database/TagDatabaseManager.h"
#include "../Models/MediaItem.h"
#include <vector>
#include <array>

namespace openwav
{

struct APCPad
{
    int id { 0 };
    int row { 0 };
    int col { 0 };
    juce::File file;
    juce::String sampleName;
    juce::Colour color { juce::Colours::cyan };
    float pitchSemi { 0.0f };   // -12 to +12 semitones
    float gainDb { 0.0f };      // -24 to +12 dB
    bool isTriggered { false };
    float triggerAnim { 0.0f }; // 1.0 to 0.0 fadeout for pad RGB pulse
    float playheadPos { 0.0f }; // 0.0 to 1.0
    bool isLooping { false };
    bool isOneShot { true };
    std::vector<float> miniWaveform;
};

struct SampleMapZone;

class PerformanceComponent : public juce::Component,
                             public juce::Timer,
                             public juce::FileDragAndDropTarget,
                             public juce::DragAndDropTarget,
                             public AudioEngineListener,
                             public juce::Slider::Listener,
                             public juce::Button::Listener,
                             public juce::MidiKeyboardState::Listener,
                             public juce::MidiInputCallback
{
public:
    PerformanceComponent(TagDatabaseManager& db, AudioEngine& engine);
    ~PerformanceComponent() override;

    void handleNoteOn(juce::MidiKeyboardState* state, int midiChannel, int midiNoteNumber, float velocity) override;
    void handleNoteOff(juce::MidiKeyboardState* state, int midiChannel, int midiNoteNumber, float velocity) override;
    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override;

    void sampleLoaded(const juce::String& filePath) override;
    void pitchTrackingStateChanged(bool enabled) override;
    void oneShotStateChanged(bool enabled) override;
    void loopingStateChanged(bool enabled) override;

    void connectApcHardware();
    void disconnectApcHardware();
    void updateApcHardwareLeds();

    void timerCallback() override;
    void paint(juce::Graphics& g) override;
    void resized() override;
    void visibilityChanged() override;
    void refreshAllPadWaveforms();

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

    // juce::FileDragAndDropTarget
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    // juce::DragAndDropTarget
    bool isInterestedInDragSource(const juce::DragAndDropTarget::SourceDetails& dragSourceDetails) override;
    void itemDropped(const juce::DragAndDropTarget::SourceDetails& dragSourceDetails) override;

    void sliderValueChanged(juce::Slider* slider) override;
    void buttonClicked(juce::Button* button) override;

    void triggerPad(int index);
    void stopPad(int index);
    void stopAllPads();
    void autoFillGridFromDatabase();
    void clearAllPads();
    void randomizeGrid();
    void loadSlices(const std::vector<SampleMapZone>& zones);

    bool keyPressed(const juce::KeyPress& key) override;

private:
    void mouseRightClick(const juce::MouseEvent& e, int padIndex);
    int padIndexAtPos(juce::Point<int> pos) const;
    juce::Rectangle<int> getPadBounds(int index) const;

    TagDatabaseManager& dbManager;
    AudioEngine& audioEngine;

    static constexpr int numRows = 8;
    static constexpr int numCols = 8;
    static constexpr int totalPads = numRows * numCols; // 64 pads

    std::array<APCPad, totalPads> pads;

    // Preset Browser Controls
    juce::ComboBox presetSelector;
    juce::TextButton newPresetButton { "New" };
    juce::TextButton savePresetButton { "Save" };
    juce::TextButton loadPresetButton { "Open" };

    void refreshPresetList();
    void savePerformancePreset(const juce::File& file);
    void loadPerformancePreset(const juce::File& file);
    juce::File getPresetsDirectory() const;

    // Global ADSR Envelope Controls (Anti-Pop / Anti-Click & Envelope Shaping)
    juce::Label attackLabel { {}, "A" };
    juce::Slider attackKnob;
    juce::Label decayLabel { {}, "D" };
    juce::Slider decayKnob;
    juce::Label sustainLabel { {}, "S" };
    juce::Slider sustainKnob;
    juce::Label releaseLabel { {}, "R" };
    juce::Slider releaseKnob;

    // Top Controls
    juce::TextButton fillButton { "Auto-Fill Grid" };
    juce::TextButton clearButton { "Clear Grid" };
    juce::TextButton randomButton { "Randomize" };
    juce::TextButton stopAllButton { "Stop All" };
    juce::TextButton pitchTrackButton { "Pitch Track: ON" };
    juce::TextButton oneShotButton { "One Shot: OFF" };
    juce::TextButton loopButton { "Loop: OFF" };
    
    // Hardware Fader State (APC Mini 9 Physical Faders mapped in background)
    std::array<float, numCols> colVolumeDb {};
    std::array<float, numCols> colPitchSemi {};
    float masterVolumeDb { 0.0f };
    bool isApcShiftPressed { false };

    juce::Label bankLabel { {}, "Pad Bank:" };
    juce::ComboBox bankSelector;
    int currentBank { 0 }; // 0 = Bank A (1-32), 1 = Bank B (33-64)

    int hoveredPadIndex { -1 };
    int activeDragPadIndex { -1 };

    std::unique_ptr<juce::MidiInput> apcMidiInput;
    std::unique_ptr<juce::MidiOutput> apcMidiOutput;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PerformanceComponent)
};

} // namespace openwav
