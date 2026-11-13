#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_gui_basics/juce_gui_basics.h>
#endif

#include "../Audio/AudioEngine.h"
#include "../Models/PluginState.h"
#include <functional>
#include <vector>

namespace openwav
{

class InstrumentKnobWidget : public juce::Component
{
public:
    InstrumentKnobWidget(InstrumentKnobState state);
    ~InstrumentKnobWidget() override = default;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;

    void setEditMode(bool editing);
    void setSelected(bool sel);
    void setState(const InstrumentKnobState& newState);
    const InstrumentKnobState& getState() const { return knobState; }
    InstrumentKnobState& getMutableState() { return knobState; }

    std::function<void(InstrumentKnobWidget*)> onSelected;
    std::function<void(InstrumentKnobWidget*, float newValue)> onValueChanged;
    std::function<void(InstrumentKnobWidget*, juce::Point<int> parentDelta)> onMovedInEdit;
    std::function<void(InstrumentKnobWidget*, int newSizePx)> onResizedInEdit;
    std::function<void()> onInteractionFinished;

private:
    InstrumentKnobState knobState;
    bool editMode { false };
    bool selected { false };
    bool hovering { false };
    bool draggingResizeHandle { false };
    bool draggingMove { false };
    bool draggingValue { false };
    float dragStartValue { 0.5f };
    int dragStartSize { 84 };
    juce::Point<int> dragStartScreenPos;

    juce::Rectangle<float> getResizeHandleBounds() const;
};

class InstrumentViewComponent : public juce::Component
{
public:
    explicit InstrumentViewComponent(AudioEngine& engine);
    ~InstrumentViewComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    InstrumentPanelState getState() const;
    void setState(const InstrumentPanelState& state);
    void applyAllMacros();

    std::function<void()> onStateChanged;
    std::function<void()> onOpenFxRackRequested;
    std::function<void(const juce::String& paramId, float normalizedValue)> onSamplerParamMacroChanged;

private:
    class MacroTargetRow : public juce::Component
    {
    public:
        MacroTargetRow(const InstrumentMacroTarget& target, int index,
                       std::function<void(int, float, float, bool)> onEdit,
                       std::function<void(int)> onRemove);
        void resized() override;
        void paint(juce::Graphics& g) override;

    private:
        int rowIndex { 0 };
        juce::Label nameLabel;
        juce::Slider minSlider;
        juce::Slider maxSlider;
        juce::TextButton invertButton { "Inv" };
        juce::TextButton removeButton { "X" };
    };

    void rebuildKnobsFromState();
    void layoutKnobs();
    void selectKnob(int index);
    void addNewKnob();
    void deleteSelectedKnob();
    void applyKnobMacros(const InstrumentKnobState& knob);
    void showAddMacroMenu();
    void chooseBackgroundImage();
    void clearBackgroundImage();
    void loadBackgroundImageFromPath(const juce::String& path);
    void syncInspectorFromSelectedKnob();
    void rebuildMacroRows();
    void updateToolbarVisibility();
    juce::Rectangle<int> getCanvasBounds() const;
    juce::Rectangle<int> getInspectorBounds() const;

    AudioEngine& audioEngine;
    InstrumentPanelState panelState;
    juce::OwnedArray<InstrumentKnobWidget> knobWidgets;
    int selectedKnobIndex { -1 };
    bool editMode { false };

    juce::Image bgImage;

    // Top control strip
    juce::Label instrumentTitleLabel;
    juce::TextButton editLayoutButton { "Edit Layout" };
    juce::TextButton addKnobButton { "+ Add Knob" };
    juce::TextButton setBgButton { "BG Image" };
    juce::TextButton clearBgButton { "Clear BG" };
    juce::Label bgDimLabel { {}, "Dim:" };
    juce::Slider bgDimSlider;
    juce::ComboBox bgFitCombo;
    juce::TextButton fxRackButton { "FX Rack" };

    // Right-side Knob & Macro Editor (visible in Edit Mode)
    juce::Label inspectorHeaderLabel { {}, "Knob & Macro Editor" };
    juce::Label noSelectionLabel { {}, "Select a knob on the canvas\nor click '+ Add Knob' to start." };

    juce::Label knobNameLabel { {}, "Label:" };
    juce::TextEditor knobNameEditor;

    juce::Label knobStyleLabel { {}, "Style:" };
    juce::ComboBox knobStyleCombo;

    juce::Label knobSizeLabel { {}, "Size:" };
    juce::Slider knobSizeSlider;

    juce::Label knobDefaultLabel { {}, "Default:" };
    juce::Slider knobDefaultSlider;

    juce::Label knobColourLabel { {}, "Colour:" };
    juce::OwnedArray<juce::TextButton> colourSwatches;
    std::vector<juce::String> swatchHexes;

    juce::TextButton deleteKnobButton { "Delete Knob" };

    juce::Label macrosHeaderLabel { {}, "Assigned Macros (Sampler & Rack FX)" };
    juce::TextButton addMacroButton { "+ Add Macro Target" };
    juce::Viewport macroListViewport;
    juce::Component macroListContainer;
    juce::OwnedArray<MacroTargetRow> macroRows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InstrumentViewComponent)
};

} // namespace openwav
