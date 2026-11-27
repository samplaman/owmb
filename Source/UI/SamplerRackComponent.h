#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_gui_basics/juce_gui_basics.h>
#endif

#include "../PluginProcessor.h"
#include "../Audio/AudioEngine.h"
#include "../Models/PluginState.h"
#include "OpenWavLookAndFeel.h"
#include "PerformanceComponent.h"
#include <vector>
#include <array>
#include <functional>

namespace openwav
{

class SamplerRackComponent;

// ─────────────────────────────────────────────────────────────────────────────
//  Individual Collapsible 19" Hardware Sampler Map Unit
// ─────────────────────────────────────────────────────────────────────────────
class SamplerRackUnitComponent : public juce::Component
{
public:
    SamplerRackUnitComponent(SamplerRackComponent& ownerRack,
                             OpenWavAudioProcessor& processor,
                             AudioEngine& engine,
                             int unitIndex,
                             int totalUnits,
                             bool isEditingActiveSlot);
    ~SamplerRackUnitComponent() override;

    void syncFromSlot(const SamplerRackSlotState& slotState, int unitIndex, int totalUnits, bool isEditingActiveSlot, int layeredCount = 1);
    void updatePeakMeter(float newPeak);

    int getDesiredHeight() const { return currentSlot.collapsed ? 48 : 158; }
    bool isCollapsed() const { return currentSlot.collapsed; }

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

private:
    int noteAtMiniMapX(float x) const;
    juce::Colour getSlotAccent() const;

    SamplerRackComponent& owner;
    OpenWavAudioProcessor& audioProcessor;
    AudioEngine& audioEngine;

    SamplerRackSlotState currentSlot;
    RackKnobLookAndFeel unitKnobLnf;
    int slotIdx { 0 };
    int totalCount { 1 };
    int layeredPeerCount { 1 };
    bool activeInEditor { false };
    bool isSyncing { false };
    float activityLevel { 0.0f };
    int previewNoteHeld { -1 };

    // 1U Header Controls (Visible when both Expanded & Folded)
    juce::TextButton foldBtn { "Collapse Unit" };
    juce::Label slotIndexLabel;
    juce::Label nameEditor;
    juce::Rectangle<int> zoneBadgeBounds;
    juce::ComboBox midiChannelCombo;
    juce::ComboBox outputBusCombo;
    juce::TextButton muteBtn { "M" };
    juce::TextButton soloBtn { "S" };
    juce::TextButton noteOffBtn { "N-Off" };
    juce::TextButton loadMapBtn { "Load" };
    juce::TextButton editMapBtn { "Edit Map" };
    juce::TextButton moveUpBtn { "Move Up" };
    juce::TextButton moveDownBtn { "Move Down" };
    juce::TextButton deleteBtn { "X" };

    // Expanded 3U Rotary Knob Controls (Consistent with Effects Rack)
    struct MiniRotaryControl
    {
        juce::Label title;
        juce::Slider knob;
        juce::Label readout;
    };
    std::array<MiniRotaryControl, 5> paramKnobs; // Gain, Pan, Transpose, Attack, Release

    juce::Rectangle<float> miniMapBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SamplerRackUnitComponent)
};

// ─────────────────────────────────────────────────────────────────────────────
//  Main Multi-SampleMap Sampler Rack Component
// ─────────────────────────────────────────────────────────────────────────────
class SamplerRackComponent : public juce::Component,
                             public juce::FileDragAndDropTarget,
                             private juce::Timer
{
public:
    SamplerRackComponent(OpenWavAudioProcessor& processor, AudioEngine& engine);
    ~SamplerRackComponent() override;

    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;

    // FileDragAndDropTarget
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragEnter(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    SamplerRackState getState() const;
    void setState(const SamplerRackState& state);
    void refreshUnits();

    // Fold / Sync current SampleMapComponent into active (or new) rack slot
    int foldSampleMapIntoRack(const SampleMapState& mapState, const juce::String& preferredName = {});
    int createNewSlotAndOpenForEditing(const SampleMapState& initialMap = {}, const juce::String& customName = {});
    void updateActiveSlotSampleMap(const SampleMapState& mapState, const juce::String& expectedSlotId = {});
    bool updateSlotSampleMapById(const juce::String& slotId, const SampleMapState& mapState);
    int getActiveEditSlotIndex() const { return rackState.activeEditSlotIndex; }
    const SamplerRackSlotState* getActiveEditSlot() const;

    // Slot callbacks used by SamplerRackUnitComponent
    void updateSlotState(int index, const SamplerRackSlotState& updated, bool rebuildLayout = false);
    void moveSlot(int fromIdx, int toIdx);
    void removeSlot(int index);
    void loadMapFileIntoSlot(int index);
    void openSlotInSampleMapEditor(int index);
    int countLayeredPeersForSlot(int index) const;
    juce::String getOutputBusLabel(int busIdx) const;

    std::function<SampleMapState()> getCurrentEditorSampleMapCallback;
    std::function<void(int slotIndex, const SamplerRackSlotState& slot)> onEditSlotInSampleMapRequested;
    std::function<void(int slotIndex, const SamplerRackSlotState& slot)> onActiveSlotSyncedFromRack;
    std::function<void(int activeSlotIndex, const SamplerRackSlotState* activeSlot)> onActiveSlotContextChanged;
    std::function<void()> onOpenMixerRequested;
    std::function<void()> onStateChanged;

private:
    void timerCallback() override;
    void showAddSamplerMenu();
    void promptLoadMultipleMapFiles();
    void addEmptySlot(const SampleMapState& initialMap = {}, const juce::String& customName = {});
    bool createSlotFromFile(const juce::File& file);
    void setAllCollapsed(bool shouldCollapse);
    void drawEmptyRackPlaceholder(juce::Graphics& g, const juce::Rectangle<int>& area);

    OpenWavAudioProcessor& audioProcessor;
    AudioEngine& audioEngine;

    SamplerRackState rackState;
    bool isFileDragOver { false };

    // Top Master Toolbar
    juce::Label rackTitleLabel;
    juce::Label rackSubtitleLabel;
    juce::Label slotCountLabel;

    juce::TextButton addSamplerBtn { "+ Add Sampler" };
    juce::TextButton loadMapFilesBtn { "Load Map" };
    juce::TextButton expandAllBtn { "Expand All" };
    juce::TextButton foldAllBtn { "Collapse All" };
    juce::TextButton openMixerBtn { "Mixer" };
    juce::TextButton clearAllBtn { "Clear All" };

    juce::Viewport rackViewport;
    class RackBayComponent : public juce::Component
    {
    public:
        explicit RackBayComponent(SamplerRackComponent& ownerComp) : owner(ownerComp) {}
        void paint(juce::Graphics& g) override;
        void resized() override;
    private:
        SamplerRackComponent& owner;
    };
    RackBayComponent rackBay;

    juce::OwnedArray<SamplerRackUnitComponent> unitComponents;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SamplerRackComponent)
};

} // namespace openwav
