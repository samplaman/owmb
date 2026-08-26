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
    void mouseUp(const juce::MouseEvent& e) override;
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
    juce::String getFormattedValueString() const;

    std::function<void(double)> onValueChanged;
    std::function<void(DecentSamplerControlComponent*, bool isDragging)> onDragStateChanged;

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DecentSamplerControlComponent)
};

// ── Decent Sampler Button Component ─────────────────────────
class DecentSamplerButtonComponent : public juce::Button
{
public:
    explicit DecentSamplerButtonComponent(const DecentSamplerUiButton& model, std::function<juce::File(const juce::String&)> fileResolver = nullptr);
    ~DecentSamplerButtonComponent() override = default;

    void setModel(const DecentSamplerUiButton& m);
    const DecentSamplerUiButton& getModel() const { return model; }
    void setFileResolver(std::function<juce::File(const juce::String&)> resolver);

    void loadImages();
    bool hasImages() const;

    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

private:
    DecentSamplerUiButton model;
    std::function<juce::File(const juce::String&)> fileResolver;

    struct StateImageSet
    {
        juce::Image mainImg;
        juce::Image hoverImg;
        juce::Image clickImg;
    };
    std::vector<StateImageSet> stateImages;

    juce::Image defaultMainImg;
    juce::Image defaultHoverImg;
    juce::Image defaultClickImg;

    juce::Image loadImageFromPath(const juce::String& rawPath, const juce::String& resolvedPath);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DecentSamplerButtonComponent)
};

// ── Dedicated Decent Sampler Canvas Look And Feel ───────────
// Insulates all canvas controls (buttons, menus, labels, combo boxes)
// so their look and feel is completely independent of global themes.
class DecentSamplerCanvasLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DecentSamplerCanvasLookAndFeel();
    ~DecentSamplerCanvasLookAndFeel() override = default;

    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox& box) override;
    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override;
    void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                           bool isSeparator, bool isActive, bool isHighlighted, bool isTicked,
                           bool hasSubMenu, const juce::String& text, const juce::String& shortcutKeyText,
                           const juce::Drawable* icon, const juce::Colour* textColour) override;
    juce::Font getTextButtonFont(juce::TextButton& button, int buttonHeight) override;
    juce::Font getComboBoxFont(juce::ComboBox& box) override;
    juce::Font getLabelFont(juce::Label& label) override;
    juce::Font getPopupMenuFont() override;
};

// ── Decent Sampler Canvas Component ─────────────────────────
class DecentSamplerCanvasComponent : public juce::Component,
                                     public juce::Button::Listener,
                                     public juce::ComboBox::Listener,
                                     public juce::Timer,
                                     public juce::MidiKeyboardState::Listener
{
public:
    explicit DecentSamplerCanvasComponent(AudioEngine& engine);
    ~DecentSamplerCanvasComponent() override;

    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    void mouseExit(const juce::MouseEvent& e) override;

    void handleNoteOn(juce::MidiKeyboardState* state, int midiChannel, int midiNoteNumber, float velocity) override;
    void handleNoteOff(juce::MidiKeyboardState* state, int midiChannel, int midiNoteNumber, float velocity) override;
    void scrollKeyboardOctave(int deltaOctaves);
    bool keyPressed(const juce::KeyPress& key) override;

    void setEditModeEnabled(bool enabled);
    bool isEditModeEnabled() const { return editMode; }

    void setInstrumentState(const SampleMapState& state);
    SampleMapState getInstrumentState() const;

    int getCurrentTab() const { return currentTab; }
    void setCurrentTab(int tabIndex);

    void buttonClicked(juce::Button* button) override;
    void comboBoxChanged(juce::ComboBox* comboBox) override;

    std::function<void(const SampleMapState&)> onStateChanged;
    std::function<void(int)> onTabChanged;

    void resetToDefaultValues();
    void randomizeControls();

    enum class CanvasComponentType
    {
        None,
        Control,
        Label,
        Image,
        Button,
        Menu
    };

    struct SelectedCanvasItem
    {
        CanvasComponentType type { CanvasComponentType::None };
        int index { -1 };

        bool isValid() const { return type != CanvasComponentType::None && index >= 0; }
        void clear() { type = CanvasComponentType::None; index = -1; }
        bool operator==(const SelectedCanvasItem& o) const { return type == o.type && index == o.index; }
        bool operator!=(const SelectedCanvasItem& o) const { return !(*this == o); }
    };

    enum class DragHandle
    {
        None,
        Move,
        TopLeft,
        Top,
        TopRight,
        Right,
        BottomRight,
        Bottom,
        BottomLeft,
        Left,
        Body
    };

    void setSelectedItem(const SelectedCanvasItem& item);
    SelectedCanvasItem getSelectedItem() const { return selectedItem; }
    void deleteSelectedItem();
    void duplicateSelectedItem();
    int getActiveTab() const { return currentTab; }

    std::function<void(const SelectedCanvasItem&)> onItemSelected;

private:
    AudioEngine& audioEngine;
    SampleMapState currentState;
    int currentTab { 0 };

    bool editMode { false };
    bool isVisualPlaying { false };
    float visualPlayheadNorm { -1.0f };
    SelectedCanvasItem selectedItem;
    DragHandle activeDragHandle { DragHandle::None };
    bool isDragging { false };
    juce::Point<int> dragStartMousePos;
    juce::Rectangle<int> dragStartBaseRect;

    DecentSamplerControlComponent* activeDraggingControl { nullptr };

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
        std::unique_ptr<DecentSamplerButtonComponent> button;
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
    void updateChildrenMouseInterception();
    SelectedCanvasItem hitTestCanvasItem(juce::Point<float> pos) const;
    DragHandle hitTestHandles(juce::Rectangle<float> r, juce::Point<float> pos) const;
    juce::Rectangle<int> getItemBaseRect(const SelectedCanvasItem& item) const;
    void setItemBaseRect(const SelectedCanvasItem& item, juce::Rectangle<int> baseRect);
    juce::Rectangle<float> getItemScreenBounds(const SelectedCanvasItem& item) const;
    juce::String getItemDisplayName(const SelectedCanvasItem& item) const;
    void showEditContextMenu(juce::Point<int> pos);

    void applyBinding(const DecentSamplerBinding& binding, double value);
    void applyControlBindings(const DecentSamplerUiControl& ctrl, double value);
    void applyMenuOptionSelection(const DecentSamplerUiMenu& menu, int selectedIndex);
    juce::File findIrFile(const juce::String& irPathOrName) const;
    juce::File findImageFile(const juce::String& imgPathOrName) const;
    float getBaseWidth() const;
    float getBaseHeight() const;
    float getCanvasScale() const;
    juce::Rectangle<float> getCanvasBounds() const;
    juce::Rectangle<float> getTotalFrameBounds() const;
    juce::Rectangle<float> getHeaderBounds() const;
    juce::Rectangle<float> getKeyboardBounds() const;
    juce::Rectangle<float> getFooterBounds() const { return getKeyboardBounds(); }
    void paintHeader(juce::Graphics& g, juce::Rectangle<float> headerRect);
    void paintFooter(juce::Graphics& g, juce::Rectangle<float> footerRect);
    void paintKeyboard(juce::Graphics& g, juce::Rectangle<float> kbRect);
    int noteNumberAtKeyboardPos(juce::Point<float> pos, juce::Rectangle<float> kbRect) const;

    static constexpr float kHeaderHeight = 50.0f;
    static constexpr float kKeyboardHeight = 105.0f;
    float getHeaderHeight() const { return kHeaderHeight; }
    float getKeyboardHeight() const { return kKeyboardHeight; }
    float getFooterHeight() const { return kKeyboardHeight; }

    int keyboardAuditionNote { -1 };
    int startWhiteKeyIndex { 12 }; // Default A0 (note 21) in 75-key array
    static constexpr int kNumVisibleWhiteKeys = 52;
    juce::Rectangle<float> leftArrowRect;
    juce::Rectangle<float> rightArrowRect;
    bool isHoveringLeftArrow { false };
    bool isHoveringRightArrow { false };
    float pitchWheelValue { 0.5f };
    float modWheelValue { 0.0f };
    bool isDraggingPitchWheel { false };
    bool isDraggingModWheel { false };

    DecentSamplerCanvasLookAndFeel canvasLookAndFeel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DecentSamplerCanvasComponent)
};

} // namespace openwav
