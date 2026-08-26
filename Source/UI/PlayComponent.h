#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_gui_basics/juce_gui_basics.h>
 #include <juce_audio_utils/juce_audio_utils.h>
#endif
#include "../Audio/AudioEngine.h"
#include "../Models/PluginState.h"
#include "DecentSamplerCanvasComponent.h"
#include "DecentSamplerStyleInspectorComponent.h"

namespace openwav
{

class PlayComponent : public juce::Component,
                      public juce::Button::Listener
{
public:
    explicit PlayComponent(AudioEngine& engine);
    ~PlayComponent() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void lookAndFeelChanged() override;

    void buttonClicked(juce::Button* button) override;

    void setState(const SampleMapState& state);
    SampleMapState getState() const;

    std::function<void(const SampleMapState&)> onStateChanged;
    std::function<void()> onOpenSampleMapRequested;
    std::function<void()> onLoadPresetRequested;
    std::function<void()> onUnloadPresetRequested;

private:
    AudioEngine& audioEngine;
    SampleMapState currentState;

    // Header info & Actions
    juce::Label instrumentTitleLabel;
    juce::Label instrumentInfoLabel;
    juce::TextButton editUiButton { "Edit UI" };
    juce::TextButton savePresetButton { "Save Preset" };
    juce::TextButton loadPresetButton { "Load Preset" };
    juce::TextButton unloadPresetButton { "Unload" };
    juce::TextButton editMapButton { "Edit Map" };
    juce::TextButton allNotesOffButton { "Panic" };

    // Front Decent Sampler Canvas View
    DecentSamplerCanvasComponent customCanvas;

    // Right-side Style Inspector
    DecentSamplerStyleInspectorComponent styleInspector;

    void syncUiFromState();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlayComponent)
};

} // namespace openwav
