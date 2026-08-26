#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_gui_basics/juce_gui_basics.h>
#endif
#include "../Models/PluginState.h"
#include "DecentSamplerCanvasComponent.h"

namespace openwav
{

class DecentSamplerStyleInspectorComponent : public juce::Component,
                                            public juce::TextEditor::Listener,
                                            public juce::ComboBox::Listener
{
public:
    DecentSamplerStyleInspectorComponent();
    ~DecentSamplerStyleInspectorComponent() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setCanvas(DecentSamplerCanvasComponent* canvas);
    void setInstrumentState(const SampleMapState& state, int activeTab);
    void setSelectedItem(const DecentSamplerCanvasComponent::SelectedCanvasItem& item);

    void textEditorTextChanged(juce::TextEditor& editor) override;
    void comboBoxChanged(juce::ComboBox* combo) override;

    std::function<void(const SampleMapState&)> onStateChanged;

private:
    DecentSamplerCanvasComponent* canvasRef { nullptr };
    SampleMapState currentState;
    int currentTab { 0 };
    DecentSamplerCanvasComponent::SelectedCanvasItem selectedItem;

    bool isUpdatingUi { false };

    // Viewport & Content Container
    juce::Viewport viewport;
    juce::Component contentComp;

    // Header Title
    juce::Label headerTitleLabel;
    juce::Label typePillLabel;
    juce::TextButton duplicateBtn { "Duplicate" };
    juce::TextButton deleteBtn { "Delete" };

    // Geometry Controls
    juce::Label geoSectionLabel;
    juce::Label xLabel { {}, "X (pt):" };
    juce::TextEditor xEditor;
    juce::Label yLabel { {}, "Y (pt):" };
    juce::TextEditor yEditor;
    juce::Label wLabel { {}, "W (pt):" };
    juce::TextEditor wEditor;
    juce::Label hLabel { {}, "H (pt):" };
    juce::TextEditor hEditor;
    juce::TextButton snapGridBtn { "Snap (10pt)" };

    // Text & Identity
    juce::Label textSectionLabel;
    juce::Label labelTextLabel { {}, "Label / Text:" };
    juce::TextEditor labelTextEditor;
    juce::Label idLabel { {}, "ID / Param:" };
    juce::TextEditor idEditor;
    juce::Label textSizeLabel { {}, "Font Size:" };
    juce::Slider textSizeSlider;
    juce::TextButton alignLeftBtn { "L" };
    juce::TextButton alignCenterBtn { "C" };
    juce::TextButton alignRightBtn { "R" };

    // Control Type & Range
    juce::Label controlSectionLabel;
    juce::Label ctrlTypeLabel { {}, "Control Type:" };
    juce::ComboBox ctrlTypeCombo;
    juce::Label unitsLabel { {}, "Units:" };
    juce::TextEditor unitsEditor;
    juce::Label minValLabel { {}, "Min:" };
    juce::TextEditor minValEditor;
    juce::Label maxValLabel { {}, "Max:" };
    juce::TextEditor maxValEditor;
    juce::Label defValLabel { {}, "Default:" };
    juce::TextEditor defValEditor;
    juce::Label bindingLabel { {}, "Target Parameter:" };
    juce::ComboBox bindingCombo;

    // Menu Options
    juce::Label menuSectionLabel;
    juce::Label optionsLabel { {}, "Options (one per line):" };
    juce::TextEditor optionsEditor;

    // Image / Custom Skin
    juce::Label skinSectionLabel;
    juce::Label skinPathLabel { {}, "Image / Skin File:" };
    juce::TextEditor skinPathEditor;
    juce::TextButton browseSkinBtn { "Browse Image..." };
    juce::TextButton clearSkinBtn { "Clear" };
    juce::Label skinFramesLabel { {}, "Num Frames (Knob):" };
    juce::TextEditor skinFramesEditor;

    // Color Controls
    juce::Label colorSectionLabel;
    
    // Track / Foreground Color
    juce::Label trackColorLabel { {}, "Accent / Track Color:" };
    juce::TextEditor trackColorEditor;
    juce::OwnedArray<juce::TextButton> trackColorSwatches;

    // Background Color
    juce::Label bgColorLabel { {}, "Background Color:" };
    juce::TextEditor bgColorEditor;
    juce::OwnedArray<juce::TextButton> bgColorSwatches;

    // Text Color
    juce::Label textColorLabel { {}, "Text Color:" };
    juce::TextEditor textColorEditor;
    juce::OwnedArray<juce::TextButton> textColorSwatches;

    // Global / Canvas Background Section (when nothing selected)
    juce::Label canvasSectionLabel;
    juce::Label canvasWidthLabel { {}, "Width (pt):" };
    juce::TextEditor canvasWidthEditor;
    juce::Label canvasHeightLabel { {}, "Height (pt):" };
    juce::TextEditor canvasHeightEditor;
    juce::Label tabNameLabel { {}, "Active Tab Name:" };
    juce::TextEditor tabNameEditor;
    juce::Label canvasBgLabel { {}, "Canvas Background Hex:" };
    juce::TextEditor canvasBgEditor;
    juce::OwnedArray<juce::TextButton> canvasBgSwatches;
    juce::Label canvasBgImgLabel { {}, "Background Image:" };
    juce::TextEditor canvasBgImgEditor;
    juce::TextButton browseCanvasBgImgBtn { "Browse Image..." };
    juce::TextButton clearCanvasBgImgBtn { "Clear Image" };
    juce::Label canvasSizeInfoLabel;

    void setupSwatches(juce::OwnedArray<juce::TextButton>& swatches, std::function<void(const juce::String&)> onColorPicked);
    void refreshFieldsFromSelection();
    void applyPropertyChanges();
    void layoutContent();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DecentSamplerStyleInspectorComponent)
};

} // namespace openwav
