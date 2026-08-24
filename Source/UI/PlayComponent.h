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

namespace openwav
{

class PlayComponent : public juce::Component,
                      public juce::Slider::Listener,
                      public juce::Button::Listener,
                      public juce::Timer
{
public:
    explicit PlayComponent(AudioEngine& engine);
    ~PlayComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void lookAndFeelChanged() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void timerCallback() override;

    void sliderValueChanged(juce::Slider* slider) override;
    void buttonClicked(juce::Button* button) override;

    void setState(const SampleMapState& state);
    SampleMapState getState() const;

    std::function<void(const SampleMapState&)> onStateChanged;
    std::function<void()> onOpenSampleMapRequested;
    std::function<void()> onLoadPresetRequested;

private:
    AudioEngine& audioEngine;
    SampleMapState currentState;
    bool showCustomUiCanvas { false };

    // Header info & Actions
    juce::Label instrumentTitleLabel;
    juce::Label instrumentInfoLabel;
    juce::TextButton viewModeToggleButton { "Custom UI" };
    juce::TextButton loadPresetButton { "Load Preset" };
    juce::TextButton editMapButton { "Edit Map" };
    juce::TextButton allNotesOffButton { "Panic" };

    // Custom Decent Sampler Canvas View
    DecentSamplerCanvasComponent customCanvas;

    // Standard ADSR Envelope Bank
    juce::GroupComponent adsrGroup { "adsrGroup", "AMPLITUDE ENVELOPE" };
    juce::Slider attackSlider;
    juce::Label attackLabel { {}, "Attack" };
    juce::Slider decaySlider;
    juce::Label decayLabel { {}, "Decay" };
    juce::Slider sustainSlider;
    juce::Label sustainLabel { {}, "Sustain" };
    juce::Slider releaseSlider;
    juce::Label releaseLabel { {}, "Release" };

    // Master Sound & Playback Controls
    juce::GroupComponent soundGroup { "soundGroup", "SOUND & PERFORMANCE" };
    juce::Slider volumeSlider;
    juce::Label volumeLabel { {}, "Volume" };
    juce::Slider reverbSlider;
    juce::Label reverbLabel { {}, "Reverb" };
    juce::Slider toneSlider;
    juce::Label toneLabel { {}, "Tone / Cutoff" };
    juce::Slider tuneSlider;
    juce::Label tuneLabel { {}, "Tune" };

    juce::TextButton roundRobinButton { "RR: Cycle" };
    juce::TextButton pitchTrackButton { "Pitch Track: ON" };
    juce::TextButton oneShotButton { "One Shot: OFF" };
    juce::TextButton loopButton { "Loop: OFF" };

    // Custom Decent Sampler Controls Bank
    juce::GroupComponent customGroup { "customGroup", "PRESET CONTROLS" };
    juce::TextButton addCustomControlButton { "+ Add Control" };

    struct CustomSliderControl
    {
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<DecentSamplerControlComponent> decentControl;
        DecentSamplerUiControl model;
        int controlIndex { -1 };
    };
    std::vector<CustomSliderControl> customSliders;

    void drawAdsrCurve(juce::Graphics& g, juce::Rectangle<float> bounds);
    void rebuildCustomSliders();
    void syncUiFromState();
    void applyCustomControlBinding(const DecentSamplerUiControl& ctrl, double value);
    void showEditCustomControlDialog(int index);
    void showAddCustomControlDialog();
    void updateViewModeVisibility();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlayComponent)
};

} // namespace openwav
