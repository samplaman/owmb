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
#include <array>
#include <functional>

namespace openwav
{

class MultiOutMixerComponent : public juce::Component,
                               public juce::ListBoxModel,
                               private juce::Timer
{
public:
    MultiOutMixerComponent(OpenWavAudioProcessor& processor, AudioEngine& engine);
    ~MultiOutMixerComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // ListBoxModel (Per-Instrument Output Routing Rows)
    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
    juce::Component* refreshComponentForRow(int rowNumber, bool isRowSelected, juce::Component* existingComponentToUpdate) override;

    MultiOutMixerState getState() const;
    void setState(const MultiOutMixerState& state);
    void refreshFromSampleMap();

    std::function<SampleMapState()> getSampleMapStateCallback;
    std::function<void(const SampleMapState&)> onSampleMapModified;
    std::function<SamplerRackState()> getSamplerRackStateCallback;
    std::function<void(const SamplerRackState&)> onSamplerRackModified;
    std::function<void()> onStateChanged;
    std::function<void()> onOpenSampleMapRequested;
    std::function<void()> onOpenSamplerRackRequested;
    std::function<void()> onOpenFxRackRequested;

    struct RoutableInstrumentRow
    {
        enum class Kind { SingleSampler, MainSampleMap, RackSlot };
        Kind kind { Kind::MainSampleMap };
        int rackSlotIndex { -1 };
        juce::String name;
        juce::String detailBadge;
        int midiChannel { 0 };
        int outputBus { 0 };
        bool canAudition { false };
        bool hasMidiChannel { true };
    };

    juce::String getBusDisplayLabel(int busIdx) const;
    void auditionInstrumentRow(int rowIndex);
    void setInstrumentMidiChannel(int rowIndex, int newMidiCh);
    void setInstrumentOutputBus(int rowIndex, int newBusIdx);

private:
    void timerCallback() override;
    void rebuildInstrumentRows();
    void syncAllControlsFromState();
    void showAssignMenuForBus(int busIdx, juce::Component& targetBtn);
    void autoRouteInstrumentsAcrossBuses();
    void resetAllInstrumentsToMain();
    void resetMixerToUnity();
    static juce::String formatDbText(float db);
    static juce::String formatPanText(float pan);

    class ConsoleBayComponent : public juce::Component
    {
    public:
        explicit ConsoleBayComponent(MultiOutMixerComponent& ownerComp) : owner(ownerComp) {}
        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent& e) override;
    private:
        MultiOutMixerComponent& owner;
    };
    friend class ConsoleBayComponent;

    struct ChannelStripWidgets
    {
        RackKnobLookAndFeel panKnobLnf;
        juce::Label nameEditor;
        juce::TextButton assignBtn { "+ Route" };
        juce::Slider panKnob;
        juce::Label panValueLabel;
        juce::TextButton muteBtn { "M" };
        juce::TextButton soloBtn { "S" };
        juce::TextButton fxBtn { "FX" };
        juce::Slider faderSlider;
        juce::Label dbReadoutLabel;
        juce::Rectangle<int> cardBounds;
        juce::Rectangle<int> scaleBounds;
        juce::Rectangle<int> meterBounds;
        float meterL { 0.0f };
        float meterR { 0.0f };
        bool clipL { false };
        bool clipR { false };
    };

    OpenWavAudioProcessor& audioProcessor;
    AudioEngine& audioEngine;

    MultiOutMixerState mixerState;
    SampleMapState cachedSampleMap;
    SamplerRackState cachedSamplerRack;
    std::vector<RoutableInstrumentRow> instrumentRows;

    // Clean 52px Top Master Toolbar (consistent with Effects Rack & Sampler Rack)
    juce::Label titleLabel;
    juce::Label statusSubtitleLabel;

    juce::TextButton modeConsoleBtn { "Mixer Console" };
    juce::TextButton modeRoutingBtn { "Instrument Outputs" };

    juce::TextButton foldToMainBtn { "Sum Inactive" };
    juce::TextButton autoRouteBtn { "Auto-Route" };
    juce::TextButton resetRoutingBtn { "All -> 1-2" };
    juce::TextButton resetMixerBtn { "Reset Mixer" };
    juce::TextButton openRackBtn { "Sampler Rack" };

    // Console Viewport & Bay (prevents horizontal squishing on compact windows)
    juce::Viewport consoleViewport;
    ConsoleBayComponent consoleBay;

    // Instrument Routing View
    juce::ListBox instrumentList { "InstrumentOutputRoutingList", nullptr };
    juce::Rectangle<int> routingCardBounds;

    std::array<ChannelStripWidgets, AudioEngine::MaxOutputBuses> busStrips;
    ChannelStripWidgets masterStrip;

    bool isUpdatingControls { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MultiOutMixerComponent)
};

} // namespace openwav
