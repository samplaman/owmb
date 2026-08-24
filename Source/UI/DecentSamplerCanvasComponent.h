#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_gui_basics/juce_gui_basics.h>
#endif
#include "../Audio/AudioEngine.h"
#include "../Models/PluginState.h"

namespace openwav
{

// ── 1:1 Decent Sampler Interactive Knob & Slider Component ──
class DecentSamplerControlComponent : public juce::Component,
                                      public juce::SettableTooltipClient
{
public:
    explicit DecentSamplerControlComponent(const DecentSamplerUiControl& model);
    ~DecentSamplerControlComponent() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;

    void setValue(double val, bool notify = true);
    double getValue() const { return model.currentValue; }
    void setVisualModulationOffset(double offset);
    double getVisualModulationOffset() const { return visualModOffset; }
    void setModel(const DecentSamplerUiControl& m);
    const DecentSamplerUiControl& getModel() const { return model; }
    void setScale(float s);

    std::function<void(double)> onValueChanged;

    static juce::Colour parseDecentSamplerColor(const juce::String& hexStr, juce::Colour defaultColor);

    struct CachedFilmstrip
    {
        int numFrames { 0 };
        int frameW { 0 };
        int frameH { 0 };
        bool isVertical { true };
        std::vector<juce::Image> frames;
    };

    static std::shared_ptr<CachedFilmstrip> getOrCreateFilmstrip(const juce::String& filePath, int numFramesHint);

private:
    DecentSamplerUiControl model;
    float scale { 1.0f };
    double visualModOffset { 0.0 };
    double dragStartVal { 0.0 };
    int dragStartY { 0 };
    int dragStartX { 0 };
    bool isHovered { false };
    bool isDragging { false };

    juce::Colour fgColor;
    juce::Colour bgColor;
    juce::Colour textColor;
    std::shared_ptr<CachedFilmstrip> filmstrip;

    void updateColors();
    void loadFilmstripImage();
    juce::String getFormattedValueString() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DecentSamplerControlComponent)
};

// ── Decent Sampler Canvas Component ─────────────────────────
class DecentSamplerCanvasComponent : public juce::Component,
                                     public juce::Button::Listener,
                                     public juce::ComboBox::Listener,
                                     public juce::Timer
{
public:
    explicit DecentSamplerCanvasComponent(AudioEngine& engine);
    ~DecentSamplerCanvasComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    void setInstrumentState(const SampleMapState& state);
    SampleMapState getInstrumentState() const;

    void buttonClicked(juce::Button* button) override;
    void comboBoxChanged(juce::ComboBox* comboBox) override;

    std::function<void(const SampleMapState&)> onStateChanged;

private:
    AudioEngine& audioEngine;
    SampleMapState currentState;
    int currentTab { 0 };

    juce::Image bgImage;
    juce::Colour parsedBgColor { juce::Colours::transparentBlack };

    // Tab buttons
    juce::OwnedArray<juce::TextButton> tabButtons;

    // Active UI Components
    float topNavPaddingY { 36.0f }; // Decent Sampler top nav bar space (in base coordinates)

    struct ControlItem
    {
        std::unique_ptr<DecentSamplerControlComponent> control;
        DecentSamplerUiControl model;
    };
    std::vector<ControlItem> controls;

    struct LabelItem
    {
        std::unique_ptr<juce::Label> label;
        DecentSamplerUiLabel model;
    };
    std::vector<LabelItem> labels;

    struct ImageItem
    {
        std::unique_ptr<juce::ImageComponent> imageComp;
        DecentSamplerUiImage model;
    };
    std::vector<ImageItem> images;

    struct ButtonItem
    {
        std::unique_ptr<juce::TextButton> button;
        DecentSamplerUiButton model;
    };
    std::vector<ButtonItem> buttons;

    struct MenuItem
    {
        std::unique_ptr<juce::ComboBox> combo;
        DecentSamplerUiMenu model;
    };
    std::vector<MenuItem> menus;

    void rebuildActiveTabUi();
    void applyBinding(const DecentSamplerBinding& binding, double value);
    void applyControlBindings(const DecentSamplerUiControl& ctrl, double value);
    juce::Rectangle<float> getCanvasBounds() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DecentSamplerCanvasComponent)
};

} // namespace openwav
