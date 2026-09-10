#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_gui_basics/juce_gui_basics.h>
#endif

#include "../Audio/AudioEngine.h"
#include "../Audio/PerformanceRackDSP.h"
#include "../Models/PluginState.h"
#include "OpenWavLookAndFeel.h"
#include <vector>
#include <memory>
#include <functional>

namespace openwav
{

// ─────────────────────────────────────────────────────────────────────────────
//  Custom Hardware Rotary Knob LookAndFeel
// ─────────────────────────────────────────────────────────────────────────────
class RackKnobLookAndFeel : public OpenWavLookAndFeel
{
public:
    RackKnobLookAndFeel();
    ~RackKnobLookAndFeel() override = default;

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override;

    void setAccentColour(juce::Colour c) { accent = c; }

private:
    juce::Colour accent { juce::Colour(0, 212, 255) };
};

// ─────────────────────────────────────────────────────────────────────────────
//  Individual Parameter Knob Component
// ─────────────────────────────────────────────────────────────────────────────
class RackKnobControl : public juce::Component,
                        public juce::Slider::Listener,
                        public juce::ComboBox::Listener
{
public:
    RackKnobControl(const EffectParamInfo& info,
                    float initialValue,
                    juce::Colour accentColour,
                    std::function<void(float)> onParamChange);
    ~RackKnobControl() override;

    void resized() override;
    void sliderValueChanged(juce::Slider* slider) override;
    void comboBoxChanged(juce::ComboBox* comboBox) override;
    void setValue(float val, bool sendNotification = false);

private:
    EffectParamInfo paramInfo;
    std::function<void(float)> onChange;
    RackKnobLookAndFeel knobLnf;

    juce::Label titleLabel;
    juce::Slider knobSlider;
    juce::Label valueLabel;
    std::unique_ptr<juce::ComboBox> choiceCombo;
    bool isChoice { false };
};

// ─────────────────────────────────────────────────────────────────────────────
//  Rack Unit Component: Individual 19" Hardware Effect Module
// ─────────────────────────────────────────────────────────────────────────────
class RackUnitComponent : public juce::Component,
                          public juce::Button::Listener,
                          public juce::ComboBox::Listener
{
public:
    RackUnitComponent(std::shared_ptr<RackEffectBase> effect,
                      int unitIndex,
                      int totalUnits,
                      std::function<void(int)> onMoveUp,
                      std::function<void(int)> onMoveDown,
                      std::function<void(int)> onDelete,
                      std::function<void()> onParamsChanged);
    ~RackUnitComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void buttonClicked(juce::Button* button) override;
    void comboBoxChanged(juce::ComboBox* comboBox) override;

    void updateTelemetry();
    void syncKnobValues();

private:
    void drawSampleSonicsLogo(juce::Graphics& g, const juce::Rectangle<float>& bounds, float earWidth);

    std::shared_ptr<RackEffectBase> effect;
    int index { 0 };
    int totalCount { 0 };

    std::function<void(int)> moveUpCallback;
    std::function<void(int)> moveDownCallback;
    std::function<void(int)> deleteCallback;
    std::function<void()> paramsChangedCallback;

    // Header Controls
    juce::Label slotLabel;
    juce::TextButton powerButton;
    juce::Label titleBadge;
    juce::ComboBox presetCombo;
    juce::TextButton btnMoveUp { juce::CharPointer_UTF8("\xe2\x96\xb2") };   // ▲
    juce::TextButton btnMoveDown { juce::CharPointer_UTF8("\xe2\x96\xbc") }; // ▼
    juce::TextButton btnDelete { juce::CharPointer_UTF8("\xe2\x9c\x95") };   // ✕

    // Parameter Knobs Container
    juce::OwnedArray<RackKnobControl> controls;

    float telemetryValue { 0.0f };
};

// ─────────────────────────────────────────────────────────────────────────────
//  Main Performance Component (Effects Rack Bay + Master Controls)
// ─────────────────────────────────────────────────────────────────────────────
class PerformanceComponent : public juce::Component,
                             public juce::Button::Listener,
                             public juce::Slider::Listener,
                             public juce::ComboBox::Listener,
                             public juce::Timer
{
public:
    explicit PerformanceComponent(AudioEngine& engine);
    ~PerformanceComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void buttonClicked(juce::Button* button) override;
    void sliderValueChanged(juce::Slider* slider) override;
    void comboBoxChanged(juce::ComboBox* comboBox) override;
    void timerCallback() override;

    // State Serialization
    PerformanceRackState getState() const;
    void setState(const PerformanceRackState& state);

    void refreshRack();

private:
    void showAddEffectMenu();
    void showPresetsMenu();
    void drawEmptyRackPlaceholder(juce::Graphics& g, const juce::Rectangle<int>& area);

    AudioEngine& audioEngine;

    // Top Master Toolbar
    juce::Label rackTitleLabel;
    juce::Label targetSubtitleLabel;
    juce::Label unitCountLabel;
    juce::TextButton btnAddEffect { "+ Add Effect" };
    juce::TextButton btnPresets { "Rack Presets" };
    juce::TextButton btnClearAll { "Clear All" };

    juce::TextButton btnMasterBypass { "Master Bypass" };
    juce::Label masterGainLabel { {}, "Output" };
    juce::Slider masterGainSlider;

    // Rack Bay Viewport & Container
    juce::Viewport rackViewport;
    class RackBayComponent : public juce::Component
    {
    public:
        RackBayComponent(PerformanceComponent& owner) : ownerComp(owner) {}
        void paint(juce::Graphics& g) override;
        void resized() override;
    private:
        PerformanceComponent& ownerComp;
    };
    RackBayComponent rackBay;

    juce::OwnedArray<RackUnitComponent> rackUnits;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PerformanceComponent)
};

} // namespace openwav

