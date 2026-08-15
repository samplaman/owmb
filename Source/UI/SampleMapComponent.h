#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_gui_basics/juce_gui_basics.h>
#endif
#include "../Audio/AudioEngine.h"
#include "../Models/MediaItem.h"
#include <vector>

namespace openwav
{

struct SampleMapZone
{
    juce::String filePath;
    juce::String sampleName;
    int rootNote { 60 };  // MIDI Note 0-127 (60 = C4)
    int keyLow { 48 };    // C3
    int keyHigh { 72 };   // C5
    int velLow { 0 };     // 0-127
    int velHigh { 127 };
    float fineTuneCents { 0.0f }; // -100 to +100 cents
    float gainDb { 0.0f };        // -24 to +12 dB
    float attackMs { 5.0f };      // 0 to 2000 ms
    float decayMs { 100.0f };     // 0 to 2000 ms
    float sustainLevel { 1.0f };  // 0 to 1.0
    float releaseMs { 200.0f };   // 0 to 5000 ms
    bool isSelected { false };
};

class SampleMapComponent : public juce::Component,
                           public juce::Slider::Listener,
                           public juce::Button::Listener,
                           public juce::MidiKeyboardState::Listener,
                           public juce::FileDragAndDropTarget,
                           public juce::DragAndDropTarget,
                           public AudioEngineListener
{
public:
    explicit SampleMapComponent(AudioEngine& engine);
    ~SampleMapComponent() override;

    void sampleLoaded(const juce::String& filePath) override;

    void handleNoteOn(juce::MidiKeyboardState* state, int midiChannel, int midiNoteNumber, float velocity) override;
    void handleNoteOff(juce::MidiKeyboardState* state, int midiChannel, int midiNoteNumber, float velocity) override;

    // juce::FileDragAndDropTarget
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    // juce::DragAndDropTarget
    bool isInterestedInDragSource(const juce::DragAndDropTarget::SourceDetails& dragSourceDetails) override;
    void itemDropped(const juce::DragAndDropTarget::SourceDetails& dragSourceDetails) override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void lookAndFeelChanged() override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;

    void sliderValueChanged(juce::Slider* slider) override;
    void buttonClicked(juce::Button* button) override;

    void addSample(const MediaItem& item);
    void addSampleFile(const juce::File& file);
    void autoSliceToSampler(const MediaItem& item);
    void clearAllZones();
    void autoMapByPitch();
    void autoMapChromatic();
    void autoMapVelocityLayers();

    const std::vector<SampleMapZone>& getZones() const { return zones; }

private:
    juce::Rectangle<float> getGridBounds() const;
    juce::Rectangle<float> getKeybedBounds() const;
    juce::Rectangle<float> getInspectorBounds() const;

    int noteNumberAtX(float x, juce::Rectangle<float> gridArea) const;
    float xForNoteNumber(int noteNum, juce::Rectangle<float> gridArea) const;
    int velocityAtY(float y, juce::Rectangle<float> gridArea) const;
    float yForVelocity(int vel, juce::Rectangle<float> gridArea) const;

    int parseRootNoteFromFilename(const juce::String& filename) const;
    juce::String midiNoteToName(int noteNum) const;

    void paintKeybed(juce::Graphics& g, juce::Rectangle<float> area) const;
    void paintZoneGrid(juce::Graphics& g, juce::Rectangle<float> area) const;

    AudioEngine& audioEngine;
    std::vector<SampleMapZone> zones;
    int selectedZoneIndex { -1 };

    // Action Toolbar
    juce::TextButton addSampleButton { "Add Sample" };
    juce::TextButton autoMapPitchButton { "Auto Pitch" };
    juce::TextButton autoMapChromaticButton { "Auto Chromatic" };
    juce::TextButton autoMapVelButton { "Auto Velocity" };
    juce::TextButton clearMapButton { "Clear Map" };
    juce::TextButton pitchTrackButton { "Pitch Track: ON" };

    // ADSR Envelope Rotary Knobs (Top Bar beside Clear Map)
    juce::Label attackLabel { {}, "Attack" };
    juce::Slider attackKnob;
    juce::Label decayLabel { {}, "Decay" };
    juce::Slider decayKnob;
    juce::Label sustainLabel { {}, "Sustain" };
    juce::Slider sustainKnob;
    juce::Label releaseLabel { {}, "Release" };
    juce::Slider releaseKnob;

    float globalAttackMs { 5.0f };
    float globalDecayMs { 100.0f };
    float globalSustainLevel { 1.0f };
    float globalReleaseMs { 200.0f };

    // Inspector Panel Controls
    juce::Label inspectorTitle { {}, "Zone Parameters" };
    juce::Label sampleNameValue { {}, "No Zone Selected" };
    juce::Label rootNoteTitle { {}, "Root Note:" };
    juce::Slider rootNoteSlider;
    juce::Label keyLowTitle { {}, "Key Low:" };
    juce::Slider keyLowSlider;
    juce::Label keyHighTitle { {}, "Key High:" };
    juce::Slider keyHighSlider;
    juce::Label velLowTitle { {}, "Vel Low:" };
    juce::Slider velLowSlider;
    juce::Label velHighTitle { {}, "Vel High:" };
    juce::Slider velHighSlider;
    juce::Label tuneTitle { {}, "Fine Tune:" };
    juce::Slider tuneSlider;
    juce::Label gainTitle { {}, "Gain (dB):" };
    juce::Slider gainSlider;
    juce::Label attackTitle { {}, "Attack (ms):" };
    juce::Slider attackSlider;
    juce::Label decayTitle { {}, "Decay (ms):" };
    juce::Slider decaySlider;
    juce::Label sustainTitle { {}, "Sustain (%):" };
    juce::Slider sustainSlider;
    juce::Label releaseTitle { {}, "Release (ms):" };
    juce::Slider releaseSlider;
    juce::Label reverbTitle { {}, "Reverb (%):" };
    juce::Slider reverbSlider;

    bool isZoneSelected(int index) const { return selectedZoneIndices.count(index) > 0; }
    void selectZone(int index, bool addToSelection = false);
    void deselectAllZones();

    // Mouse Interaction State
    enum class DragTarget
    {
        None,
        MoveZone,
        ResizeKeyLow,
        ResizeKeyHigh,
        ResizeVelLow,
        ResizeVelHigh,
        KeybedAudition,
        BoxSelect
    };
    DragTarget activeDragTarget { DragTarget::None };
    int activeDragZone { -1 };
    int auditionNote { -1 };
    std::array<bool, 128> activeMidiNotes {};

    std::set<int> selectedZoneIndices;
    juce::Rectangle<float> lassoRect;
    juce::Point<float> boxSelectStartPos;
    int dragStartNote { -1 };
    int dragStartVel { -1 };
    std::map<int, SampleMapZone> dragStartZones;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleMapComponent)
};

} // namespace openwav
