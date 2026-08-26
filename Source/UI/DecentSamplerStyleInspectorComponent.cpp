#include "DecentSamplerStyleInspectorComponent.h"
#include "OpenWavLookAndFeel.h"

namespace openwav
{

static const char* paletteHexes[] = {
    "#00E5FF", // Cyan
    "#00FF88", // Mint
    "#FFB300", // Amber
    "#FF5252", // Coral
    "#BD00FF", // Violet
    "#2979FF", // Sky
    "#FFFFFF", // White
    "#888888", // Gray
    "#1E222B"  // Dark
};

DecentSamplerStyleInspectorComponent::DecentSamplerStyleInspectorComponent()
{
    setOpaque(true);

    // ── Header Title & Actions ─────────────────────────
    headerTitleLabel.setText("STYLE INSPECTOR", juce::dontSendNotification);
    headerTitleLabel.setFont(juce::Font(13.0f, juce::Font::bold));
    headerTitleLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    addAndMakeVisible(headerTitleLabel);

    typePillLabel.setFont(juce::Font(10.0f, juce::Font::bold));
    typePillLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    typePillLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0x3300E5FF));
    typePillLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(typePillLabel);

    duplicateBtn.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::bgCard);
    duplicateBtn.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::accentCyan);
    duplicateBtn.onClick = [this] {
        if (canvasRef)
        {
            canvasRef->duplicateSelectedItem();
            refreshFieldsFromSelection();
        }
    };
    addAndMakeVisible(duplicateBtn);

    deleteBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0x33FF4444));
    deleteBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFFFF6666));
    deleteBtn.onClick = [this] {
        if (canvasRef)
        {
            canvasRef->deleteSelectedItem();
            refreshFieldsFromSelection();
        }
    };
    addAndMakeVisible(deleteBtn);

    // ── Viewport Setup ─────────────────────────────────
    viewport.setViewedComponent(&contentComp, false);
    viewport.setScrollBarsShown(true, false, true, false);
    addAndMakeVisible(viewport);

    // Helper for labels
    auto setupSection = [&](juce::Label& lbl, const juce::String& text) {
        lbl.setText(text, juce::dontSendNotification);
        lbl.setFont(juce::Font(10.5f, juce::Font::bold));
        lbl.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan.withAlpha(0.85f));
        contentComp.addAndMakeVisible(lbl);
    };

    auto setupFieldLabel = [&](juce::Label& lbl) {
        lbl.setFont(juce::Font(10.5f));
        lbl.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
        contentComp.addAndMakeVisible(lbl);
    };

    auto setupEditor = [&](juce::TextEditor& ed) {
        ed.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF1B1E26));
        ed.setColour(juce::TextEditor::textColourId, OpenWavLookAndFeel::textPrimary);
        ed.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xFF2E333F));
        ed.setColour(juce::TextEditor::focusedOutlineColourId, OpenWavLookAndFeel::accentCyan);
        ed.addListener(this);
        contentComp.addAndMakeVisible(ed);
    };

    // 1. Geometry Section
    setupSection(geoSectionLabel, "POSITION & SIZE (PT)");
    setupFieldLabel(xLabel); setupEditor(xEditor);
    setupFieldLabel(yLabel); setupEditor(yEditor);
    setupFieldLabel(wLabel); setupEditor(wEditor);
    setupFieldLabel(hLabel); setupEditor(hEditor);

    snapGridBtn.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::bgCard);
    snapGridBtn.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::textPrimary);
    snapGridBtn.onClick = [this] {
        int x = xEditor.getText().getIntValue();
        int y = yEditor.getText().getIntValue();
        int w = wEditor.getText().getIntValue();
        int h = hEditor.getText().getIntValue();
        xEditor.setText(juce::String((x / 10) * 10), juce::dontSendNotification);
        yEditor.setText(juce::String((y / 10) * 10), juce::dontSendNotification);
        wEditor.setText(juce::String(std::max(10, (w / 10) * 10)), juce::dontSendNotification);
        hEditor.setText(juce::String(std::max(10, (h / 10) * 10)), juce::dontSendNotification);
        applyPropertyChanges();
    };
    contentComp.addAndMakeVisible(snapGridBtn);

    // 2. Text & Identity Section
    setupSection(textSectionLabel, "TEXT & IDENTITY");
    setupFieldLabel(labelTextLabel); setupEditor(labelTextEditor);
    setupFieldLabel(idLabel); setupEditor(idEditor);
    setupFieldLabel(textSizeLabel);
    textSizeSlider.setRange(6, 48, 1);
    textSizeSlider.setValue(10.0, juce::dontSendNotification);
    textSizeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    textSizeSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 18);
    textSizeSlider.setColour(juce::Slider::thumbColourId, OpenWavLookAndFeel::accentCyan);
    textSizeSlider.setColour(juce::Slider::trackColourId, OpenWavLookAndFeel::accentCyan.withAlpha(0.4f));
    textSizeSlider.onValueChange = [this] { applyPropertyChanges(); };
    contentComp.addAndMakeVisible(textSizeSlider);

    auto setupAlignBtn = [&](juce::TextButton& btn) {
        btn.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::bgCard);
        btn.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::textSecondary);
        contentComp.addAndMakeVisible(btn);
    };
    setupAlignBtn(alignLeftBtn);
    setupAlignBtn(alignCenterBtn);
    setupAlignBtn(alignRightBtn);
    alignLeftBtn.onClick = [this] { alignLeftBtn.setToggleState(true, juce::dontSendNotification); alignCenterBtn.setToggleState(false, juce::dontSendNotification); alignRightBtn.setToggleState(false, juce::dontSendNotification); applyPropertyChanges(); };
    alignCenterBtn.onClick = [this] { alignCenterBtn.setToggleState(true, juce::dontSendNotification); alignLeftBtn.setToggleState(false, juce::dontSendNotification); alignRightBtn.setToggleState(false, juce::dontSendNotification); applyPropertyChanges(); };
    alignRightBtn.onClick = [this] { alignRightBtn.setToggleState(true, juce::dontSendNotification); alignLeftBtn.setToggleState(false, juce::dontSendNotification); alignCenterBtn.setToggleState(false, juce::dontSendNotification); applyPropertyChanges(); };

    // 3. Control & Range Section
    setupSection(controlSectionLabel, "CONTROL SETTINGS");
    setupFieldLabel(ctrlTypeLabel);
    ctrlTypeCombo.addItem("Knob (Rotary)", 1);
    ctrlTypeCombo.addItem("Vertical Slider", 2);
    ctrlTypeCombo.addItem("Horizontal Slider", 3);
    ctrlTypeCombo.addListener(this);
    contentComp.addAndMakeVisible(ctrlTypeCombo);

    setupFieldLabel(unitsLabel); setupEditor(unitsEditor);
    setupFieldLabel(minValLabel); setupEditor(minValEditor);
    setupFieldLabel(maxValLabel); setupEditor(maxValEditor);
    setupFieldLabel(defValLabel); setupEditor(defValEditor);

    setupFieldLabel(bindingLabel);
    bindingCombo.addItem("None", 1);
    bindingCombo.addItem("FX: Reverb Wet Level (FX_REVERB_WET_LEVEL)", 2);
    bindingCombo.addItem("FX: Reverb Room Size (FX_REVERB_ROOM_SIZE)", 3);
    bindingCombo.addItem("FX: Delay Time (FX_DELAY_TIME)", 4);
    bindingCombo.addItem("FX: Delay Feedback (FX_DELAY_FEEDBACK)", 5);
    bindingCombo.addItem("FX: Delay Wet (FX_DELAY_WET_LEVEL)", 6);
    bindingCombo.addItem("FX: Lowpass Cutoff (FX_LOWPASS_CUTOFF)", 7);
    bindingCombo.addItem("FX: Highpass Cutoff (FX_HIGHPASS_CUTOFF)", 8);
    bindingCombo.addItem("FX: Chorus Amount (FX_CHORUS)", 9);
    bindingCombo.addItem("FX: Tone EQ (FX_TONE)", 10);
    bindingCombo.addItem("Gain / Volume (gain)", 11);
    bindingCombo.addItem("Stereo Pan (pan)", 12);
    bindingCombo.addItem("Master Pitch / Tuning (tuning)", 13);
    bindingCombo.addItem("LFO Rate (LFO_FREQ)", 14);
    bindingCombo.addItem("LFO Depth (LFO_DEPTH)", 15);
    bindingCombo.addListener(this);
    contentComp.addAndMakeVisible(bindingCombo);

    // 4. Menu Options
    setupSection(menuSectionLabel, "DROPDOWN OPTIONS");
    setupFieldLabel(optionsLabel);
    optionsEditor.setMultiLine(true);
    optionsEditor.setReturnKeyStartsNewLine(true);
    setupEditor(optionsEditor);

    // 5. Custom Skin / Image
    setupSection(skinSectionLabel, "IMAGE & SKIN FILMSTRIP");
    setupFieldLabel(skinPathLabel); setupEditor(skinPathEditor);
    browseSkinBtn.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::bgCard);
    browseSkinBtn.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::accentCyan);
    browseSkinBtn.onClick = [this] {
        auto chooser = std::make_shared<juce::FileChooser>("Select Skin or Image file", juce::File(), "*.png;*.jpg;*.jpeg;*.webp;*.svg;*.bmp;*.gif;*.tif;*.tiff");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this, chooser](const juce::FileChooser&) {
            auto r = chooser->getResult();
            if (r.existsAsFile())
            {
                skinPathEditor.setText(r.getFullPathName(), juce::sendNotification);
            }
        });
    };
    contentComp.addAndMakeVisible(browseSkinBtn);

    clearSkinBtn.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::bgCard);
    clearSkinBtn.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::textSecondary);
    clearSkinBtn.onClick = [this] {
        skinPathEditor.setText({}, juce::sendNotification);
    };
    contentComp.addAndMakeVisible(clearSkinBtn);

    setupFieldLabel(skinFramesLabel); setupEditor(skinFramesEditor);

    // 6. Color Appearance Section
    setupSection(colorSectionLabel, "COLORS & APPEARANCE");

    setupFieldLabel(trackColorLabel); setupEditor(trackColorEditor);
    setupSwatches(trackColorSwatches, [this](const juce::String& hex) {
        trackColorEditor.setText(hex, juce::sendNotification);
    });

    setupFieldLabel(bgColorLabel); setupEditor(bgColorEditor);
    setupSwatches(bgColorSwatches, [this](const juce::String& hex) {
        bgColorEditor.setText(hex, juce::sendNotification);
    });

    setupFieldLabel(textColorLabel); setupEditor(textColorEditor);
    setupSwatches(textColorSwatches, [this](const juce::String& hex) {
        textColorEditor.setText(hex, juce::sendNotification);
    });

    // 7. Canvas Background Section
    setupSection(canvasSectionLabel, "CANVAS PROPERTIES");
    setupFieldLabel(canvasWidthLabel); setupEditor(canvasWidthEditor);
    setupFieldLabel(canvasHeightLabel); setupEditor(canvasHeightEditor);
    setupFieldLabel(tabNameLabel); setupEditor(tabNameEditor);

    setupFieldLabel(canvasBgLabel); setupEditor(canvasBgEditor);
    setupSwatches(canvasBgSwatches, [this](const juce::String& hex) {
        canvasBgEditor.setText(hex, juce::sendNotification);
    });

    setupFieldLabel(canvasBgImgLabel); setupEditor(canvasBgImgEditor);
    browseCanvasBgImgBtn.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::bgCard);
    browseCanvasBgImgBtn.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::accentCyan);
    browseCanvasBgImgBtn.onClick = [this] {
        auto chooser = std::make_shared<juce::FileChooser>("Select Background Image", juce::File(), "*.png;*.jpg;*.jpeg;*.webp;*.svg;*.bmp;*.gif;*.tif;*.tiff");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this, chooser](const juce::FileChooser&) {
            auto r = chooser->getResult();
            if (r.existsAsFile())
            {
                canvasBgImgEditor.setText(r.getFullPathName(), juce::sendNotification);
            }
        });
    };
    contentComp.addAndMakeVisible(browseCanvasBgImgBtn);

    clearCanvasBgImgBtn.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::bgCard);
    clearCanvasBgImgBtn.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::textSecondary);
    clearCanvasBgImgBtn.onClick = [this] {
        canvasBgImgEditor.setText({}, juce::sendNotification);
    };
    contentComp.addAndMakeVisible(clearCanvasBgImgBtn);

    canvasSizeInfoLabel.setFont(juce::Font(11.0f));
    canvasSizeInfoLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan.withAlpha(0.8f));
    canvasSizeInfoLabel.setText("Decent Sampler Canvas Viewport", juce::dontSendNotification);
    contentComp.addAndMakeVisible(canvasSizeInfoLabel);

    refreshFieldsFromSelection();
}

void DecentSamplerStyleInspectorComponent::setupSwatches(juce::OwnedArray<juce::TextButton>& swatches, std::function<void(const juce::String&)> onColorPicked)
{
    swatches.clear();
    for (int i = 0; i < 9; ++i)
    {
        auto* btn = swatches.add(new juce::TextButton());
        juce::String hex = paletteHexes[i];
        juce::Colour c = DecentSamplerControlComponent::parseDecentSamplerColor(hex, juce::Colours::white);
        btn->setColour(juce::TextButton::buttonColourId, c);
        btn->setColour(juce::TextButton::buttonOnColourId, c);
        btn->setTooltip(hex);
        btn->onClick = [onColorPicked, hex] {
            if (onColorPicked) onColorPicked(hex);
        };
        contentComp.addAndMakeVisible(btn);
    }
}

void DecentSamplerStyleInspectorComponent::setCanvas(DecentSamplerCanvasComponent* canvas)
{
    canvasRef = canvas;
}

void DecentSamplerStyleInspectorComponent::setInstrumentState(const SampleMapState& state, int activeTab)
{
    currentState = state;
    currentTab = activeTab;
    refreshFieldsFromSelection();
}

void DecentSamplerStyleInspectorComponent::setSelectedItem(const DecentSamplerCanvasComponent::SelectedCanvasItem& item)
{
    selectedItem = item;
    refreshFieldsFromSelection();
}

void DecentSamplerStyleInspectorComponent::paint(juce::Graphics& g)
{
    g.fillAll(OpenWavLookAndFeel::bgCard.darker(0.2f));

    // Left separator border
    g.setColour(juce::Colour(0x3500E5FF));
    g.drawVerticalLine(0, 0.0f, static_cast<float>(getHeight()));
}

void DecentSamplerStyleInspectorComponent::resized()
{
    auto area = getLocalBounds().reduced(8, 6);

    // Header Area
    auto headerArea = area.removeFromTop(28);
    duplicateBtn.setBounds(headerArea.removeFromRight(64).reduced(2, 2));
    deleteBtn.setBounds(headerArea.removeFromRight(50).reduced(2, 2));
    headerTitleLabel.setBounds(headerArea.removeFromLeft(110));
    typePillLabel.setBounds(headerArea.reduced(2, 4));

    area.removeFromTop(4);
    viewport.setBounds(area);

    layoutContent();
}

void DecentSamplerStyleInspectorComponent::layoutContent()
{
    int w = std::max(220, viewport.getWidth() - 10);
    int y = 4;

    auto row = [&](juce::Component& c, int h = 22) {
        c.setBounds(4, y, w - 8, h);
        y += h + 4;
    };

    auto row2 = [&](juce::Component& lbl, juce::Component& ed, int labelW = 75, int h = 22) {
        lbl.setBounds(4, y, labelW, h);
        ed.setBounds(4 + labelW, y, w - 8 - labelW, h);
        y += h + 4;
    };

    auto row4 = [&](juce::Component& l1, juce::Component& e1, juce::Component& l2, juce::Component& e2, int h = 22) {
        int halfW = (w - 8) / 2;
        l1.setBounds(4, y, 24, h);
        e1.setBounds(28, y, halfW - 28, h);
        l2.setBounds(4 + halfW, y, 24, h);
        e2.setBounds(4 + halfW + 24, y, halfW - 28, h);
        y += h + 4;
    };

    auto row2Buttons = [&](juce::Component& b1, juce::Component& b2, int h = 22) {
        int halfW = (w - 8) / 2;
        b1.setBounds(4, y, halfW - 2, h);
        b2.setBounds(4 + halfW + 2, y, halfW - 2, h);
        y += h + 4;
    };

    auto rowSwatches = [&](juce::OwnedArray<juce::TextButton>& swatches) {
        int swW = (w - 8) / 9;
        for (int i = 0; i < swatches.size(); ++i)
        {
            swatches[i]->setBounds(4 + i * swW, y, swW - 2, 16);
        }
        y += 16 + 6;
    };

    // If item selected, show geometry, text, controls, colors
    if (selectedItem.isValid())
    {
        geoSectionLabel.setVisible(true);
        xLabel.setVisible(true); xEditor.setVisible(true);
        yLabel.setVisible(true); yEditor.setVisible(true);
        wLabel.setVisible(true); wEditor.setVisible(true);
        hLabel.setVisible(true); hEditor.setVisible(true);
        snapGridBtn.setVisible(true);

        row(geoSectionLabel, 18);
        row4(xLabel, xEditor, yLabel, yEditor);
        row4(wLabel, wEditor, hLabel, hEditor);
        row(snapGridBtn, 20);

        // Text Section
        textSectionLabel.setVisible(true);
        labelTextLabel.setVisible(true); labelTextEditor.setVisible(true);
        idLabel.setVisible(true); idEditor.setVisible(true);
        textSizeLabel.setVisible(true); textSizeSlider.setVisible(true);

        row(textSectionLabel, 18);
        row2(labelTextLabel, labelTextEditor);
        row2(idLabel, idEditor);
        row2(textSizeLabel, textSizeSlider);

        if (selectedItem.type == DecentSamplerCanvasComponent::CanvasComponentType::Label)
        {
            alignLeftBtn.setVisible(true); alignCenterBtn.setVisible(true); alignRightBtn.setVisible(true);
            int btnW = (w - 8) / 3;
            alignLeftBtn.setBounds(4, y, btnW - 2, 20);
            alignCenterBtn.setBounds(4 + btnW, y, btnW - 2, 20);
            alignRightBtn.setBounds(4 + btnW * 2, y, btnW - 2, 20);
            y += 24;
        }
        else
        {
            alignLeftBtn.setVisible(false); alignCenterBtn.setVisible(false); alignRightBtn.setVisible(false);
        }

        // Control Section (Knobs / Sliders)
        bool isCtrl = (selectedItem.type == DecentSamplerCanvasComponent::CanvasComponentType::Control);
        controlSectionLabel.setVisible(isCtrl);
        ctrlTypeLabel.setVisible(isCtrl); ctrlTypeCombo.setVisible(isCtrl);
        unitsLabel.setVisible(isCtrl); unitsEditor.setVisible(isCtrl);
        minValLabel.setVisible(isCtrl); minValEditor.setVisible(isCtrl);
        maxValLabel.setVisible(isCtrl); maxValEditor.setVisible(isCtrl);
        defValLabel.setVisible(isCtrl); defValEditor.setVisible(isCtrl);
        bindingLabel.setVisible(isCtrl); bindingCombo.setVisible(isCtrl);

        if (isCtrl)
        {
            row(controlSectionLabel, 18);
            row2(ctrlTypeLabel, ctrlTypeCombo);
            row2(unitsLabel, unitsEditor);
            row4(minValLabel, minValEditor, maxValLabel, maxValEditor);
            row2(defValLabel, defValEditor);
            row2(bindingLabel, bindingCombo);
        }

        // Menu Section
        bool isMenu = (selectedItem.type == DecentSamplerCanvasComponent::CanvasComponentType::Menu);
        menuSectionLabel.setVisible(isMenu);
        optionsLabel.setVisible(isMenu); optionsEditor.setVisible(isMenu);
        if (isMenu)
        {
            row(menuSectionLabel, 18);
            row(optionsLabel, 16);
            row(optionsEditor, 60);
        }

        // Skin & Image Section
        bool isImageOrSkin = (selectedItem.type == DecentSamplerCanvasComponent::CanvasComponentType::Image || isCtrl || selectedItem.type == DecentSamplerCanvasComponent::CanvasComponentType::Button);
        skinSectionLabel.setVisible(isImageOrSkin);
        skinPathLabel.setVisible(isImageOrSkin); skinPathEditor.setVisible(isImageOrSkin);
        browseSkinBtn.setVisible(isImageOrSkin);
        clearSkinBtn.setVisible(isImageOrSkin);
        skinFramesLabel.setVisible(isCtrl); skinFramesEditor.setVisible(isCtrl);

        if (isImageOrSkin)
        {
            row(skinSectionLabel, 18);
            row2(skinPathLabel, skinPathEditor);
            row2Buttons(browseSkinBtn, clearSkinBtn);
            if (isCtrl)
                row2(skinFramesLabel, skinFramesEditor);
        }

        // Colors Section
        colorSectionLabel.setVisible(true);
        trackColorLabel.setVisible(true); trackColorEditor.setVisible(true);
        bgColorLabel.setVisible(true); bgColorEditor.setVisible(true);
        textColorLabel.setVisible(true); textColorEditor.setVisible(true);
        for (auto* b : trackColorSwatches) b->setVisible(true);
        for (auto* b : bgColorSwatches) b->setVisible(true);
        for (auto* b : textColorSwatches) b->setVisible(true);

        row(colorSectionLabel, 18);
        row2(trackColorLabel, trackColorEditor);
        rowSwatches(trackColorSwatches);

        row2(bgColorLabel, bgColorEditor);
        rowSwatches(bgColorSwatches);

        row2(textColorLabel, textColorEditor);
        rowSwatches(textColorSwatches);

        // Hide Canvas background section when an item is selected
        canvasSectionLabel.setVisible(false);
        canvasWidthLabel.setVisible(false); canvasWidthEditor.setVisible(false);
        canvasHeightLabel.setVisible(false); canvasHeightEditor.setVisible(false);
        tabNameLabel.setVisible(false); tabNameEditor.setVisible(false);
        canvasBgLabel.setVisible(false); canvasBgEditor.setVisible(false);
        for (auto* b : canvasBgSwatches) b->setVisible(false);
        canvasBgImgLabel.setVisible(false); canvasBgImgEditor.setVisible(false);
        browseCanvasBgImgBtn.setVisible(false);
        canvasSizeInfoLabel.setVisible(false);
    }
    else
    {
        // No item selected: Show Canvas & Background settings
        geoSectionLabel.setVisible(false);
        xLabel.setVisible(false); xEditor.setVisible(false);
        yLabel.setVisible(false); yEditor.setVisible(false);
        wLabel.setVisible(false); wEditor.setVisible(false);
        hLabel.setVisible(false); hEditor.setVisible(false);
        snapGridBtn.setVisible(false);

        textSectionLabel.setVisible(false);
        labelTextLabel.setVisible(false); labelTextEditor.setVisible(false);
        idLabel.setVisible(false); idEditor.setVisible(false);
        textSizeLabel.setVisible(false); textSizeSlider.setVisible(false);
        alignLeftBtn.setVisible(false); alignCenterBtn.setVisible(false); alignRightBtn.setVisible(false);

        controlSectionLabel.setVisible(false);
        ctrlTypeLabel.setVisible(false); ctrlTypeCombo.setVisible(false);
        unitsLabel.setVisible(false); unitsEditor.setVisible(false);
        minValLabel.setVisible(false); minValEditor.setVisible(false);
        maxValLabel.setVisible(false); maxValEditor.setVisible(false);
        defValLabel.setVisible(false); defValEditor.setVisible(false);
        bindingLabel.setVisible(false); bindingCombo.setVisible(false);

        menuSectionLabel.setVisible(false);
        optionsLabel.setVisible(false); optionsEditor.setVisible(false);

        skinSectionLabel.setVisible(false);
        skinPathLabel.setVisible(false); skinPathEditor.setVisible(false);
        browseSkinBtn.setVisible(false);
        clearSkinBtn.setVisible(false);
        skinFramesLabel.setVisible(false); skinFramesEditor.setVisible(false);

        colorSectionLabel.setVisible(false);
        trackColorLabel.setVisible(false); trackColorEditor.setVisible(false);
        bgColorLabel.setVisible(false); bgColorEditor.setVisible(false);
        textColorLabel.setVisible(false); textColorEditor.setVisible(false);
        for (auto* b : trackColorSwatches) b->setVisible(false);
        for (auto* b : bgColorSwatches) b->setVisible(false);
        for (auto* b : textColorSwatches) b->setVisible(false);

        // Show Canvas Section
        canvasSectionLabel.setVisible(true);
        canvasWidthLabel.setVisible(true); canvasWidthEditor.setVisible(true);
        canvasHeightLabel.setVisible(true); canvasHeightEditor.setVisible(true);
        tabNameLabel.setVisible(true); tabNameEditor.setVisible(true);
        canvasBgLabel.setVisible(true); canvasBgEditor.setVisible(true);
        for (auto* b : canvasBgSwatches) b->setVisible(true);
        canvasBgImgLabel.setVisible(true); canvasBgImgEditor.setVisible(true);
        browseCanvasBgImgBtn.setVisible(true);
        clearCanvasBgImgBtn.setVisible(true);
        canvasSizeInfoLabel.setVisible(true);

        row(canvasSectionLabel, 18);
        row4(canvasWidthLabel, canvasWidthEditor, canvasHeightLabel, canvasHeightEditor);
        row2(tabNameLabel, tabNameEditor);
        row2(canvasBgLabel, canvasBgEditor);
        rowSwatches(canvasBgSwatches);
        row2(canvasBgImgLabel, canvasBgImgEditor);
        row2Buttons(browseCanvasBgImgBtn, clearCanvasBgImgBtn);
        row(canvasSizeInfoLabel, 18);
    }

    contentComp.setSize(w, y + 20);
}

void DecentSamplerStyleInspectorComponent::refreshFieldsFromSelection()
{
    isUpdatingUi = true;

    int tabIdx = juce::jlimit(0, std::max(0, static_cast<int>(currentState.customUi.tabs.size()) - 1), currentTab);

    duplicateBtn.setEnabled(selectedItem.isValid());
    deleteBtn.setEnabled(selectedItem.isValid());

    if (!selectedItem.isValid())
    {
        typePillLabel.setText("CANVAS", juce::dontSendNotification);
        canvasWidthEditor.setText(juce::String(currentState.customUi.width > 0 ? currentState.customUi.width : 812), juce::dontSendNotification);
        canvasHeightEditor.setText(juce::String(currentState.customUi.height > 0 ? currentState.customUi.height : 375), juce::dontSendNotification);
        if (tabIdx < static_cast<int>(currentState.customUi.tabs.size()))
            tabNameEditor.setText(currentState.customUi.tabs[tabIdx].name, juce::dontSendNotification);
        canvasBgEditor.setText(currentState.customUi.bgColorHex, juce::dontSendNotification);
        canvasBgImgEditor.setText(currentState.customUi.bgImagePath, juce::dontSendNotification);
    }
    else
    {
        switch (selectedItem.type)
        {
            case DecentSamplerCanvasComponent::CanvasComponentType::Control:
            {
                typePillLabel.setText("KNOB / SLIDER", juce::dontSendNotification);
                xLabel.setText("X (pt):", juce::dontSendNotification);
                yLabel.setText("Y (pt):", juce::dontSendNotification);
                if (tabIdx < static_cast<int>(currentState.customUi.tabs.size()) && selectedItem.index < static_cast<int>(currentState.customUi.tabs[tabIdx].controls.size()))
                {
                    const auto& c = currentState.customUi.tabs[tabIdx].controls[selectedItem.index];
                    xEditor.setText(juce::String(c.x), juce::dontSendNotification);
                    yEditor.setText(juce::String(c.y), juce::dontSendNotification);
                    wEditor.setText(juce::String(c.width), juce::dontSendNotification);
                    hEditor.setText(juce::String(c.height), juce::dontSendNotification);

                    labelTextEditor.setText(c.label, juce::dontSendNotification);
                    idEditor.setText(c.id, juce::dontSendNotification);
                    textSizeSlider.setValue(c.textSize > 0 ? c.textSize : 10.0f, juce::dontSendNotification);

                    if (c.type.containsIgnoreCase("vert")) ctrlTypeCombo.setSelectedId(2, juce::dontSendNotification);
                    else if (c.type.containsIgnoreCase("horiz")) ctrlTypeCombo.setSelectedId(3, juce::dontSendNotification);
                    else ctrlTypeCombo.setSelectedId(1, juce::dontSendNotification);

                    unitsEditor.setText(c.units, juce::dontSendNotification);
                    minValEditor.setText(juce::String(c.minValue), juce::dontSendNotification);
                    maxValEditor.setText(juce::String(c.maxValue), juce::dontSendNotification);
                    defValEditor.setText(juce::String(c.defaultValue), juce::dontSendNotification);

                    // Map binding
                    juce::String bp = c.bindingParam;
                    if (bp.containsIgnoreCase("REVERB_WET")) bindingCombo.setSelectedId(2, juce::dontSendNotification);
                    else if (bp.containsIgnoreCase("ROOM_SIZE")) bindingCombo.setSelectedId(3, juce::dontSendNotification);
                    else if (bp.containsIgnoreCase("DELAY_TIME")) bindingCombo.setSelectedId(4, juce::dontSendNotification);
                    else if (bp.containsIgnoreCase("DELAY_FEEDBACK")) bindingCombo.setSelectedId(5, juce::dontSendNotification);
                    else if (bp.containsIgnoreCase("DELAY_WET")) bindingCombo.setSelectedId(6, juce::dontSendNotification);
                    else if (bp.containsIgnoreCase("LOWPASS") || bp.containsIgnoreCase("CUTOFF")) bindingCombo.setSelectedId(7, juce::dontSendNotification);
                    else if (bp.containsIgnoreCase("HIGHPASS")) bindingCombo.setSelectedId(8, juce::dontSendNotification);
                    else if (bp.containsIgnoreCase("CHORUS")) bindingCombo.setSelectedId(9, juce::dontSendNotification);
                    else if (bp.containsIgnoreCase("TONE")) bindingCombo.setSelectedId(10, juce::dontSendNotification);
                    else if (bp.containsIgnoreCase("gain") || bp.containsIgnoreCase("volume")) bindingCombo.setSelectedId(11, juce::dontSendNotification);
                    else if (bp.containsIgnoreCase("pan")) bindingCombo.setSelectedId(12, juce::dontSendNotification);
                    else if (bp.containsIgnoreCase("tune") || bp.containsIgnoreCase("pitch")) bindingCombo.setSelectedId(13, juce::dontSendNotification);
                    else if (bp.containsIgnoreCase("LFO_FREQ")) bindingCombo.setSelectedId(14, juce::dontSendNotification);
                    else if (bp.containsIgnoreCase("LFO_DEPTH")) bindingCombo.setSelectedId(15, juce::dontSendNotification);
                    else bindingCombo.setSelectedId(1, juce::dontSendNotification);

                    skinPathEditor.setText(c.customSkinImagePath, juce::dontSendNotification);
                    skinFramesEditor.setText(juce::String(c.customSkinNumFrames), juce::dontSendNotification);

                    trackColorEditor.setText(c.trackColorHex, juce::dontSendNotification);
                    bgColorEditor.setText(c.trackBackgroundColorHex, juce::dontSendNotification);
                    textColorEditor.setText(c.textColorHex, juce::dontSendNotification);
                }
                break;
            }
            case DecentSamplerCanvasComponent::CanvasComponentType::Label:
            {
                typePillLabel.setText("LABEL", juce::dontSendNotification);
                xLabel.setText("X (pt):", juce::dontSendNotification);
                yLabel.setText("Y (pt):", juce::dontSendNotification);
                if (tabIdx < static_cast<int>(currentState.customUi.tabs.size()) && selectedItem.index < static_cast<int>(currentState.customUi.tabs[tabIdx].labels.size()))
                {
                    const auto& l = currentState.customUi.tabs[tabIdx].labels[selectedItem.index];
                    xEditor.setText(juce::String(l.x), juce::dontSendNotification);
                    yEditor.setText(juce::String(l.y), juce::dontSendNotification);
                    wEditor.setText(juce::String(l.width), juce::dontSendNotification);
                    hEditor.setText(juce::String(l.height), juce::dontSendNotification);

                    labelTextEditor.setText(l.text, juce::dontSendNotification);
                    idEditor.setText("", juce::dontSendNotification);
                    textSizeSlider.setValue(l.textSize > 0 ? l.textSize : 10.0f, juce::dontSendNotification);

                    alignLeftBtn.setToggleState(l.textAlignment.containsIgnoreCase("left"), juce::dontSendNotification);
                    alignCenterBtn.setToggleState(l.textAlignment.containsIgnoreCase("center") || l.textAlignment.isEmpty(), juce::dontSendNotification);
                    alignRightBtn.setToggleState(l.textAlignment.containsIgnoreCase("right"), juce::dontSendNotification);

                    trackColorEditor.setText("", juce::dontSendNotification);
                    bgColorEditor.setText("", juce::dontSendNotification);
                    textColorEditor.setText(l.textColorHex, juce::dontSendNotification);
                }
                break;
            }
            case DecentSamplerCanvasComponent::CanvasComponentType::Button:
            {
                typePillLabel.setText("BUTTON", juce::dontSendNotification);
                xLabel.setText("X (pt):", juce::dontSendNotification);
                yLabel.setText("Y (pt):", juce::dontSendNotification);
                if (tabIdx < static_cast<int>(currentState.customUi.tabs.size()) && selectedItem.index < static_cast<int>(currentState.customUi.tabs[tabIdx].buttons.size()))
                {
                    const auto& b = currentState.customUi.tabs[tabIdx].buttons[selectedItem.index];
                    xEditor.setText(juce::String(b.x), juce::dontSendNotification);
                    yEditor.setText(juce::String(b.y), juce::dontSendNotification);
                    wEditor.setText(juce::String(b.width), juce::dontSendNotification);
                    hEditor.setText(juce::String(b.height), juce::dontSendNotification);

                    labelTextEditor.setText(b.text, juce::dontSendNotification);
                    idEditor.setText(b.style, juce::dontSendNotification);
                    textSizeSlider.setValue(b.textSize > 0 ? b.textSize : 10.0f, juce::dontSendNotification);
                    skinPathEditor.setText(b.mainImage.isNotEmpty() ? b.mainImage : (!b.states.empty() ? b.states[0].mainImage : ""), juce::dontSendNotification);

                    trackColorEditor.setText(b.trackForegroundColorHex, juce::dontSendNotification);
                    bgColorEditor.setText(b.bgColorHex, juce::dontSendNotification);
                    textColorEditor.setText(b.textColorHex, juce::dontSendNotification);
                }
                break;
            }
            case DecentSamplerCanvasComponent::CanvasComponentType::Menu:
            {
                typePillLabel.setText("MENU", juce::dontSendNotification);
                xLabel.setText("X (pt):", juce::dontSendNotification);
                yLabel.setText("Y (pt):", juce::dontSendNotification);
                if (tabIdx < static_cast<int>(currentState.customUi.tabs.size()) && selectedItem.index < static_cast<int>(currentState.customUi.tabs[tabIdx].menus.size()))
                {
                    const auto& m = currentState.customUi.tabs[tabIdx].menus[selectedItem.index];
                    xEditor.setText(juce::String(m.x), juce::dontSendNotification);
                    yEditor.setText(juce::String(m.y), juce::dontSendNotification);
                    wEditor.setText(juce::String(m.width), juce::dontSendNotification);
                    hEditor.setText(juce::String(m.height), juce::dontSendNotification);

                    labelTextEditor.setText("", juce::dontSendNotification);
                    idEditor.setText("", juce::dontSendNotification);
                    textSizeSlider.setValue(m.textSize > 0 ? m.textSize : 10.0f, juce::dontSendNotification);

                    optionsEditor.setText(m.options.joinIntoString("\n"), juce::dontSendNotification);

                    trackColorEditor.setText(m.trackForegroundColorHex, juce::dontSendNotification);
                    bgColorEditor.setText(m.bgColorHex, juce::dontSendNotification);
                    textColorEditor.setText(m.textColorHex, juce::dontSendNotification);
                }
                break;
            }
            case DecentSamplerCanvasComponent::CanvasComponentType::Image:
            {
                typePillLabel.setText("IMAGE", juce::dontSendNotification);
                xLabel.setText("X (pt):", juce::dontSendNotification);
                yLabel.setText("Y (pt):", juce::dontSendNotification);
                if (tabIdx < static_cast<int>(currentState.customUi.tabs.size()) && selectedItem.index < static_cast<int>(currentState.customUi.tabs[tabIdx].images.size()))
                {
                    const auto& img = currentState.customUi.tabs[tabIdx].images[selectedItem.index];
                    xEditor.setText(juce::String(img.x), juce::dontSendNotification);
                    yEditor.setText(juce::String(img.y), juce::dontSendNotification);
                    wEditor.setText(juce::String(img.width), juce::dontSendNotification);
                    hEditor.setText(juce::String(img.height), juce::dontSendNotification);

                    skinPathEditor.setText(img.path, juce::dontSendNotification);
                }
                break;
            }
            default: break;
        }
    }

    isUpdatingUi = false;
    layoutContent();
}

void DecentSamplerStyleInspectorComponent::applyPropertyChanges()
{
    if (isUpdatingUi) return;

    int tabIdx = juce::jlimit(0, std::max(0, static_cast<int>(currentState.customUi.tabs.size()) - 1), currentTab);

    if (selectedItem.isValid())
    {
        int x = xEditor.getText().getIntValue();
        int y = yEditor.getText().getIntValue();
        int w = std::max(10, wEditor.getText().getIntValue());
        int h = std::max(10, hEditor.getText().getIntValue());

        switch (selectedItem.type)
        {
            case DecentSamplerCanvasComponent::CanvasComponentType::Control:
            {
                if (tabIdx < static_cast<int>(currentState.customUi.tabs.size()) && selectedItem.index < static_cast<int>(currentState.customUi.tabs[tabIdx].controls.size()))
                {
                    auto& c = currentState.customUi.tabs[tabIdx].controls[selectedItem.index];
                    c.x = x; c.y = y; c.width = w; c.height = h;
                    c.label = labelTextEditor.getText();
                    c.id = idEditor.getText();
                    c.textSize = static_cast<float>(textSizeSlider.getValue());

                    int typeId = ctrlTypeCombo.getSelectedId();
                    if (typeId == 2) c.type = "vertical_slider";
                    else if (typeId == 3) c.type = "horizontal_slider";
                    else c.type = "knob";

                    c.units = unitsEditor.getText();
                    c.minValue = minValEditor.getText().getDoubleValue();
                    c.maxValue = maxValEditor.getText().getDoubleValue();
                    c.defaultValue = defValEditor.getText().getDoubleValue();

                    int bId = bindingCombo.getSelectedId();
                    if (bId == 2) c.bindingParam = "FX_REVERB_WET_LEVEL";
                    else if (bId == 3) c.bindingParam = "FX_REVERB_ROOM_SIZE";
                    else if (bId == 4) c.bindingParam = "FX_DELAY_TIME";
                    else if (bId == 5) c.bindingParam = "FX_DELAY_FEEDBACK";
                    else if (bId == 6) c.bindingParam = "FX_DELAY_WET_LEVEL";
                    else if (bId == 7) c.bindingParam = "FX_LOWPASS_CUTOFF";
                    else if (bId == 8) c.bindingParam = "FX_HIGHPASS_CUTOFF";
                    else if (bId == 9) c.bindingParam = "FX_CHORUS";
                    else if (bId == 10) c.bindingParam = "FX_TONE";
                    else if (bId == 11) c.bindingParam = "gain";
                    else if (bId == 12) c.bindingParam = "pan";
                    else if (bId == 13) c.bindingParam = "tuning";
                    else if (bId == 14) c.bindingParam = "LFO_FREQ";
                    else if (bId == 15) c.bindingParam = "LFO_DEPTH";

                    c.customSkinImagePath = skinPathEditor.getText();
                    c.customSkinNumFrames = skinFramesEditor.getText().getIntValue();
                    if (c.customSkinImagePath.isNotEmpty())
                    {
                        juce::File baseDir = juce::File(currentState.customUi.resolvedBgImagePath.isNotEmpty() ? currentState.customUi.resolvedBgImagePath : currentState.customUi.bgImagePath);
                        c.resolvedCustomSkinImagePath = SampleMapState::resolveDecentSamplerSamplePath(c.customSkinImagePath, baseDir).getFullPathName();
                    }
                    else
                    {
                        c.resolvedCustomSkinImagePath = "";
                    }

                    c.trackColorHex = trackColorEditor.getText();
                    c.trackBackgroundColorHex = bgColorEditor.getText();
                    c.textColorHex = textColorEditor.getText();

                    for (auto& uc : currentState.uiControls)
                    {
                        if (uc.id == c.id || uc.label == c.label)
                            uc = c;
                    }
                }
                break;
            }
            case DecentSamplerCanvasComponent::CanvasComponentType::Label:
            {
                if (tabIdx < static_cast<int>(currentState.customUi.tabs.size()) && selectedItem.index < static_cast<int>(currentState.customUi.tabs[tabIdx].labels.size()))
                {
                    auto& l = currentState.customUi.tabs[tabIdx].labels[selectedItem.index];
                    l.x = x; l.y = y; l.width = w; l.height = h;
                    l.text = labelTextEditor.getText();
                    l.textSize = static_cast<float>(textSizeSlider.getValue());
                    if (alignLeftBtn.getToggleState()) l.textAlignment = "left";
                    else if (alignRightBtn.getToggleState()) l.textAlignment = "right";
                    else l.textAlignment = "center";
                    l.textColorHex = textColorEditor.getText();
                }
                break;
            }
            case DecentSamplerCanvasComponent::CanvasComponentType::Button:
            {
                if (tabIdx < static_cast<int>(currentState.customUi.tabs.size()) && selectedItem.index < static_cast<int>(currentState.customUi.tabs[tabIdx].buttons.size()))
                {
                    auto& b = currentState.customUi.tabs[tabIdx].buttons[selectedItem.index];
                    b.x = x; b.y = y; b.width = w; b.height = h;
                    b.text = labelTextEditor.getText();
                    b.style = idEditor.getText();
                    b.textSize = static_cast<float>(textSizeSlider.getValue());
                    b.trackForegroundColorHex = trackColorEditor.getText();
                    b.bgColorHex = bgColorEditor.getText();
                    b.textColorHex = textColorEditor.getText();
                    b.mainImage = skinPathEditor.getText();
                    if (b.mainImage.isNotEmpty())
                    {
                        juce::File baseDir = juce::File(currentState.customUi.resolvedBgImagePath.isNotEmpty() ? currentState.customUi.resolvedBgImagePath : currentState.customUi.bgImagePath);
                        b.resolvedMainImagePath = SampleMapState::resolveDecentSamplerSamplePath(b.mainImage, baseDir).getFullPathName();
                    }
                    else
                    {
                        b.resolvedMainImagePath = "";
                    }
                }
                break;
            }
            case DecentSamplerCanvasComponent::CanvasComponentType::Menu:
            {
                if (tabIdx < static_cast<int>(currentState.customUi.tabs.size()) && selectedItem.index < static_cast<int>(currentState.customUi.tabs[tabIdx].menus.size()))
                {
                    auto& m = currentState.customUi.tabs[tabIdx].menus[selectedItem.index];
                    m.x = x; m.y = y; m.width = w; m.height = h;
                    m.textSize = static_cast<float>(textSizeSlider.getValue());
                    m.options.clear();
                    m.options.addLines(optionsEditor.getText());
                    m.trackForegroundColorHex = trackColorEditor.getText();
                    m.bgColorHex = bgColorEditor.getText();
                    m.textColorHex = textColorEditor.getText();
                }
                break;
            }
            case DecentSamplerCanvasComponent::CanvasComponentType::Image:
            {
                if (tabIdx < static_cast<int>(currentState.customUi.tabs.size()) && selectedItem.index < static_cast<int>(currentState.customUi.tabs[tabIdx].images.size()))
                {
                    auto& img = currentState.customUi.tabs[tabIdx].images[selectedItem.index];
                    img.x = x; img.y = y; img.width = w; img.height = h;
                    img.path = skinPathEditor.getText();
                }
                break;
            }
            default: break;
        }

        if (canvasRef)
        {
            canvasRef->setInstrumentState(currentState);
        }
        if (onStateChanged)
            onStateChanged(currentState);
    }
    else
    {
        // Canvas background image, color, dimensions & tab name
        int cw = canvasWidthEditor.getText().getIntValue();
        int ch = canvasHeightEditor.getText().getIntValue();
        if (cw > 0) currentState.customUi.width = cw;
        if (ch > 0) currentState.customUi.height = ch;

        if (tabIdx < static_cast<int>(currentState.customUi.tabs.size()) && tabNameEditor.getText().isNotEmpty())
            currentState.customUi.tabs[tabIdx].name = tabNameEditor.getText();

        currentState.customUi.bgImagePath = canvasBgImgEditor.getText();
        currentState.customUi.bgColorHex = canvasBgEditor.getText();

        if (canvasRef)
            canvasRef->setInstrumentState(currentState);
        if (onStateChanged)
            onStateChanged(currentState);
    }
}

void DecentSamplerStyleInspectorComponent::textEditorTextChanged(juce::TextEditor&)
{
    applyPropertyChanges();
}

void DecentSamplerStyleInspectorComponent::comboBoxChanged(juce::ComboBox*)
{
    applyPropertyChanges();
}

} // namespace openwav
