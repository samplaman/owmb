#include "InstrumentViewComponent.h"
#include "OpenWavLookAndFeel.h"
#include <cmath>

namespace openwav
{

namespace
{
juce::Colour parseHexColour(const juce::String& hex, juce::Colour fallback = OpenWavLookAndFeel::accentCyan)
{
    auto clean = hex.trim().replace("#", "");
    if (clean.length() == 6)
        clean = "FF" + clean;
    if (clean.length() == 8)
        return juce::Colour::fromString(clean);
    return fallback;
}

const char* effectTypeDisplayName(PerformanceEffectType type)
{
    switch (type)
    {
        case PerformanceEffectType::Reverb:        return "Studio Reverb";
        case PerformanceEffectType::Delay:         return "Stereo Delay";
        case PerformanceEffectType::Filter:        return "Multi-Mode Filter";
        case PerformanceEffectType::Distortion:    return "Analog Saturation";
        case PerformanceEffectType::Chorus:        return "Lush Chorus";
        case PerformanceEffectType::Compressor:    return "VCA Compressor";
        case PerformanceEffectType::Bitcrusher:    return "Lo-Fi Bitcrusher";
        case PerformanceEffectType::ParametricEQ:  return "3-Band Parametric EQ";
        case PerformanceEffectType::PitchShifter:  return "Pitch Shifter";
        case PerformanceEffectType::Tremolo:       return "Stereo Tremolo";
        case PerformanceEffectType::Phaser:        return "Analog Phaser";
        case PerformanceEffectType::Flanger:       return "Jet Flanger";
        case PerformanceEffectType::StereoImager:  return "Stereo Imager";
        case PerformanceEffectType::AutoWah:       return "Envelope Auto-Wah";
        case PerformanceEffectType::RingModulator: return "Ring Modulator";
        case PerformanceEffectType::AmpCabinet:    return "Amp & Cabinet";
        case PerformanceEffectType::ADSREnvelope:  return "ADSR Envelope";
        default:                                   return "Effect";
    }
}
} // namespace

// ─────────────────────────────────────────────────────────────────────────────
//  InstrumentKnobWidget
// ─────────────────────────────────────────────────────────────────────────────
InstrumentKnobWidget::InstrumentKnobWidget(InstrumentKnobState state)
    : knobState(std::move(state))
{
    setRepaintsOnMouseActivity(true);
}

void InstrumentKnobWidget::setEditMode(bool editing)
{
    if (editMode != editing)
    {
        editMode = editing;
        setMouseCursor(editMode ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::PointingHandCursor);
        repaint();
    }
}

void InstrumentKnobWidget::setSelected(bool sel)
{
    if (selected != sel)
    {
        selected = sel;
        repaint();
    }
}

void InstrumentKnobWidget::setState(const InstrumentKnobState& newState)
{
    knobState = newState;
    repaint();
}

juce::Rectangle<float> InstrumentKnobWidget::getResizeHandleBounds() const
{
    return juce::Rectangle<float>(static_cast<float>(getWidth()) - 14.0f,
                                  static_cast<float>(getHeight()) - 14.0f,
                                  12.0f, 12.0f);
}

void InstrumentKnobWidget::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const float labelHeight = 20.0f;
    auto dialArea = bounds.withTrimmedBottom(labelHeight).reduced(4.0f);
    float diameter = std::min(dialArea.getWidth(), dialArea.getHeight());
    if (diameter < 20.0f) return;

    auto centre = dialArea.getCentre();
    float radius = diameter * 0.5f;
    juce::Colour accent = parseHexColour(knobState.colourHex);

    const float startAngle = juce::MathConstants<float>::pi * 1.25f;
    const float endAngle = juce::MathConstants<float>::pi * 2.75f;
    const float val = juce::jlimit(0.0f, 1.0f, knobState.value);
    const float angle = startAngle + val * (endAngle - startAngle);

    if (editMode && selected)
    {
        g.setColour(accent.withAlpha(0.14f));
        g.fillRoundedRectangle(bounds.reduced(1.0f), 8.0f);
        g.setColour(accent.withAlpha(0.85f));
        g.drawRoundedRectangle(bounds.reduced(1.0f), 8.0f, 1.5f);
    }
    else if (editMode)
    {
        g.setColour(juce::Colours::white.withAlpha(0.18f));
        g.drawRoundedRectangle(bounds.reduced(1.0f), 8.0f, 1.0f);
    }

    // Track arc
    {
        juce::Path trackArc;
        trackArc.addCentredArc(centre.x, centre.y, radius - 2.0f, radius - 2.0f, 0.0f, startAngle, endAngle, true);
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.strokePath(trackArc, juce::PathStrokeType(knobState.style == 3 ? 4.5f : 3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Active value arc
    if (val > 0.002f)
    {
        juce::Path valArc;
        valArc.addCentredArc(centre.x, centre.y, radius - 2.0f, radius - 2.0f, 0.0f, startAngle, angle, true);
        if (knobState.style == 3)
        {
            g.setColour(accent.withAlpha(0.30f));
            g.strokePath(valArc, juce::PathStrokeType(8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setColour(accent);
        g.strokePath(valArc, juce::PathStrokeType(knobState.style == 3 ? 4.2f : 3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Knob body
    float bodyRadius = radius - 8.0f;
    auto bodyRect = juce::Rectangle<float>(centre.x - bodyRadius, centre.y - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f);

    if (knobState.style == 1) // Vintage Bakelite
    {
        juce::ColourGradient grad(juce::Colour(0xFF3A3532), centre.x, centre.y - bodyRadius,
                                  juce::Colour(0xFF161412), centre.x, centre.y + bodyRadius, false);
        g.setGradientFill(grad);
        g.fillEllipse(bodyRect);
        g.setColour(juce::Colour(0xFF554E48));
        g.drawEllipse(bodyRect, 1.5f);

        // Inner fluted cap
        auto inner = bodyRect.reduced(bodyRadius * 0.25f);
        g.setColour(juce::Colour(0xFF24201E));
        g.fillEllipse(inner);
    }
    else if (knobState.style == 2) // Studio Minimal
    {
        g.setColour(juce::Colour(0xFF1A1E26));
        g.fillEllipse(bodyRect);
        g.setColour(hovering ? accent.withAlpha(0.8f) : juce::Colour(0xFF3B4354));
        g.drawEllipse(bodyRect, 1.5f);
    }
    else // 0 = Modern Arc, 3 = Neon Ring
    {
        juce::ColourGradient grad(juce::Colour(0xFF2B313C), centre.x, centre.y - bodyRadius,
                                  juce::Colour(0xFF14181F), centre.x, centre.y + bodyRadius, false);
        g.setGradientFill(grad);
        g.fillEllipse(bodyRect);
        g.setColour(hovering || selected ? accent.withAlpha(0.75f) : juce::Colour(0xFF424B5B));
        g.drawEllipse(bodyRect, 1.2f);
    }

    // Pointer / indicator line
    {
        float pointerLen = bodyRadius * 0.78f;
        float innerStart = (knobState.style == 1) ? 0.0f : bodyRadius * 0.30f;
        float sinA = std::sin(angle);
        float cosA = -std::cos(angle);
        juce::Point<float> p1(centre.x + sinA * innerStart, centre.y + cosA * innerStart);
        juce::Point<float> p2(centre.x + sinA * pointerLen, centre.y + cosA * pointerLen);
        g.setColour(knobState.style == 1 ? juce::Colours::ivory : accent);
        g.drawLine(p1.x, p1.y, p2.x, p2.y, 2.4f);
    }

    // Label below knob
    auto labelArea = bounds.removeFromBottom(labelHeight);
    g.setColour(juce::Colours::white.withAlpha(0.92f));
    g.setFont(juce::Font(12.0f, juce::Font::bold));
    juce::String caption = knobState.label;
    if (!editMode && (hovering || draggingValue))
        caption = juce::String(juce::roundToInt(val * 100.0f)) + "%";
    g.drawFittedText(caption, labelArea.toNearestInt(), juce::Justification::centred, 1);

    // Edit Mode badges: macro count & resize handle
    if (editMode)
    {
        int macroCount = static_cast<int>(knobState.targets.size());
        if (macroCount > 0)
        {
            juce::Rectangle<float> badge(bounds.getRight() - 22.0f, 3.0f, 19.0f, 14.0f);
            g.setColour(accent.withAlpha(0.9f));
            g.fillRoundedRectangle(badge, 4.0f);
            g.setColour(juce::Colours::black);
            g.setFont(juce::Font(10.0f, juce::Font::bold));
            g.drawText(juce::String(macroCount) + "M", badge.toNearestInt(), juce::Justification::centred);
        }

        if (selected)
        {
            auto rh = getResizeHandleBounds();
            g.setColour(accent);
            g.fillRoundedRectangle(rh, 2.0f);
            g.setColour(juce::Colours::black);
            g.drawLine(rh.getX() + 3.0f, rh.getBottom() - 3.0f, rh.getRight() - 3.0f, rh.getY() + 3.0f, 1.3f);
        }
    }
}

void InstrumentKnobWidget::mouseEnter(const juce::MouseEvent&)
{
    hovering = true;
    repaint();
}

void InstrumentKnobWidget::mouseExit(const juce::MouseEvent&)
{
    hovering = false;
    repaint();
}

void InstrumentKnobWidget::mouseDown(const juce::MouseEvent& e)
{
    if (onSelected)
        onSelected(this);

    dragStartScreenPos = e.getScreenPosition();
    draggingResizeHandle = false;
    draggingMove = false;
    draggingValue = false;

    if (editMode)
    {
        if (selected && getResizeHandleBounds().expanded(4.0f).contains(e.position))
        {
            draggingResizeHandle = true;
            dragStartSize = knobState.sizePx;
        }
        else
        {
            draggingMove = true;
        }
    }
    else
    {
        draggingValue = true;
        dragStartValue = knobState.value;
    }
}

void InstrumentKnobWidget::mouseDrag(const juce::MouseEvent& e)
{
    auto delta = e.getScreenPosition() - dragStartScreenPos;
    if (editMode)
    {
        if (draggingResizeHandle)
        {
            int d = std::max(delta.x, delta.y);
            int newSize = juce::jlimit(48, 150, dragStartSize + d);
            if (newSize != knobState.sizePx && onResizedInEdit)
                onResizedInEdit(this, newSize);
        }
        else if (draggingMove && onMovedInEdit)
        {
            onMovedInEdit(this, delta);
            dragStartScreenPos = e.getScreenPosition();
        }
    }
    else if (draggingValue)
    {
        float sensitivity = e.mods.isShiftDown() ? 0.0015f : 0.005f;
        float newVal = juce::jlimit(0.0f, 1.0f, dragStartValue - static_cast<float>(delta.y) * sensitivity);
        if (std::abs(newVal - knobState.value) > 0.0005f)
        {
            knobState.value = newVal;
            repaint();
            if (onValueChanged)
                onValueChanged(this, newVal);
        }
    }
}

void InstrumentKnobWidget::mouseUp(const juce::MouseEvent&)
{
    if ((draggingValue || draggingMove || draggingResizeHandle) && onInteractionFinished)
        onInteractionFinished();
    draggingResizeHandle = false;
    draggingMove = false;
    draggingValue = false;
}

void InstrumentKnobWidget::mouseDoubleClick(const juce::MouseEvent&)
{
    if (editMode)
        return;

    knobState.value = juce::jlimit(0.0f, 1.0f, knobState.defaultValue);
    repaint();
    if (onValueChanged)
        onValueChanged(this, knobState.value);
    if (onInteractionFinished)
        onInteractionFinished();
}

void InstrumentKnobWidget::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (editMode)
        return;

    float step = (e.mods.isShiftDown() ? 0.01f : 0.05f) * (wheel.deltaY >= 0.0f ? 1.0f : -1.0f);
    if (std::abs(wheel.deltaY) < 0.0001f)
        return;

    float newVal = juce::jlimit(0.0f, 1.0f, knobState.value + step);
    if (std::abs(newVal - knobState.value) > 0.0005f)
    {
        knobState.value = newVal;
        repaint();
        if (onValueChanged)
            onValueChanged(this, newVal);
        if (onInteractionFinished)
            onInteractionFinished();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  MacroTargetRow
// ─────────────────────────────────────────────────────────────────────────────
InstrumentViewComponent::MacroTargetRow::MacroTargetRow(
    const InstrumentMacroTarget& target, int index,
    std::function<void(int, float, float, bool)> onEdit,
    std::function<void(int)> onRemove)
    : rowIndex(index)
{
    nameLabel.setText(target.displayName.isNotEmpty() ? target.displayName : target.paramId, juce::dontSendNotification);
    nameLabel.setFont(juce::Font(11.5f, juce::Font::bold));
    nameLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textPrimary);
    addAndMakeVisible(nameLabel);

    auto setupRangeSlider = [this](juce::Slider& s, float val) {
        s.setSliderStyle(juce::Slider::LinearBar);
        s.setRange(0.0, 100.0, 1.0);
        s.setValue(val * 100.0, juce::dontSendNotification);
        s.setTextValueSuffix("%");
        s.setColour(juce::Slider::trackColourId, OpenWavLookAndFeel::accentCyan.withAlpha(0.45f));
        addAndMakeVisible(s);
    };

    setupRangeSlider(minSlider, target.minRange);
    setupRangeSlider(maxSlider, target.maxRange);

    auto emitChange = [this, onEdit] {
        if (onEdit)
            onEdit(rowIndex,
                   static_cast<float>(minSlider.getValue() / 100.0),
                   static_cast<float>(maxSlider.getValue() / 100.0),
                   invertButton.getToggleState());
    };

    minSlider.onValueChange = emitChange;
    maxSlider.onValueChange = emitChange;

    invertButton.setClickingTogglesState(true);
    invertButton.setToggleState(target.inverted, juce::dontSendNotification);
    invertButton.setTooltip("Invert macro range");
    invertButton.onClick = emitChange;
    addAndMakeVisible(invertButton);

    removeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF4B1D24));
    removeButton.setTooltip("Remove macro target");
    removeButton.onClick = [this, onRemove] {
        if (onRemove)
            onRemove(rowIndex);
    };
    addAndMakeVisible(removeButton);
}

void InstrumentViewComponent::MacroTargetRow::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(0xFF1A1F29));
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 5.0f);
    g.setColour(juce::Colour(0xFF2D3545));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 5.0f, 1.0f);
}

void InstrumentViewComponent::MacroTargetRow::resized()
{
    auto area = getLocalBounds().reduced(6, 4);
    auto top = area.removeFromTop(18);
    removeButton.setBounds(top.removeFromRight(20));
    top.removeFromRight(4);
    invertButton.setBounds(top.removeFromRight(32));
    top.removeFromRight(4);
    nameLabel.setBounds(top);

    area.removeFromTop(4);
    int halfW = (area.getWidth() - 6) / 2;
    minSlider.setBounds(area.removeFromLeft(halfW));
    area.removeFromLeft(6);
    maxSlider.setBounds(area);
}

// ─────────────────────────────────────────────────────────────────────────────
//  InstrumentViewComponent
// ─────────────────────────────────────────────────────────────────────────────
InstrumentViewComponent::InstrumentViewComponent(AudioEngine& engine)
    : audioEngine(engine)
{
    // Top strip controls
    instrumentTitleLabel.setText(panelState.instrumentName, juce::dontSendNotification);
    instrumentTitleLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    instrumentTitleLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textPrimary);
    instrumentTitleLabel.setEditable(false, true, false);
    instrumentTitleLabel.onTextChange = [this] {
        panelState.instrumentName = instrumentTitleLabel.getText().trim();
        if (panelState.instrumentName.isEmpty())
            panelState.instrumentName = "Custom Instrument";
        if (onStateChanged) onStateChanged();
    };
    addAndMakeVisible(instrumentTitleLabel);

    editLayoutButton.setClickingTogglesState(true);
    editLayoutButton.setColour(juce::TextButton::buttonOnColourId, OpenWavLookAndFeel::accentCyan.withAlpha(0.28f));
    editLayoutButton.onClick = [this] {
        editMode = editLayoutButton.getToggleState();
        for (auto* kw : knobWidgets)
            kw->setEditMode(editMode);
        if (editMode && selectedKnobIndex < 0 && !knobWidgets.isEmpty())
            selectKnob(0);
        updateToolbarVisibility();
        resized();
        repaint();
    };
    addAndMakeVisible(editLayoutButton);

    addKnobButton.onClick = [this] {
        if (!editMode)
        {
            editMode = true;
            editLayoutButton.setToggleState(true, juce::dontSendNotification);
            for (auto* kw : knobWidgets)
                kw->setEditMode(true);
        }
        addNewKnob();
    };
    addAndMakeVisible(addKnobButton);

    setBgButton.onClick = [this] { chooseBackgroundImage(); };
    addAndMakeVisible(setBgButton);

    clearBgButton.onClick = [this] { clearBackgroundImage(); };
    addAndMakeVisible(clearBgButton);

    bgDimLabel.setFont(juce::Font(11.0f));
    bgDimLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    addAndMakeVisible(bgDimLabel);

    bgDimSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    bgDimSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    bgDimSlider.setRange(0.0, 0.85, 0.01);
    bgDimSlider.setValue(panelState.backgroundDim, juce::dontSendNotification);
    bgDimSlider.onValueChange = [this] {
        panelState.backgroundDim = static_cast<float>(bgDimSlider.getValue());
        repaint();
        if (onStateChanged) onStateChanged();
    };
    addAndMakeVisible(bgDimSlider);

    bgFitCombo.addItem("Cover", 1);
    bgFitCombo.addItem("Contain", 2);
    bgFitCombo.addItem("Stretch", 3);
    bgFitCombo.setSelectedId(1, juce::dontSendNotification);
    bgFitCombo.onChange = [this] {
        panelState.backgroundFitMode = juce::jlimit(0, 2, bgFitCombo.getSelectedId() - 1);
        repaint();
        if (onStateChanged) onStateChanged();
    };
    addAndMakeVisible(bgFitCombo);

    fxRackButton.onClick = [this] {
        if (onOpenFxRackRequested)
            onOpenFxRackRequested();
    };
    addAndMakeVisible(fxRackButton);

    // Right-side Inspector Controls
    inspectorHeaderLabel.setFont(juce::Font(13.5f, juce::Font::bold));
    inspectorHeaderLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    addAndMakeVisible(inspectorHeaderLabel);

    noSelectionLabel.setFont(juce::Font(12.0f));
    noSelectionLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    noSelectionLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(noSelectionLabel);

    knobNameLabel.setFont(juce::Font(11.5f));
    addAndMakeVisible(knobNameLabel);
    knobNameEditor.onTextChange = [this] {
        if (selectedKnobIndex >= 0 && selectedKnobIndex < static_cast<int>(panelState.knobs.size()))
        {
            panelState.knobs[static_cast<size_t>(selectedKnobIndex)].label = knobNameEditor.getText();
            knobWidgets[selectedKnobIndex]->setState(panelState.knobs[static_cast<size_t>(selectedKnobIndex)]);
            if (onStateChanged) onStateChanged();
        }
    };
    addAndMakeVisible(knobNameEditor);

    knobStyleLabel.setFont(juce::Font(11.5f));
    addAndMakeVisible(knobStyleLabel);
    knobStyleCombo.addItem("Modern Arc", 1);
    knobStyleCombo.addItem("Vintage Bakelite", 2);
    knobStyleCombo.addItem("Studio Minimal", 3);
    knobStyleCombo.addItem("Neon Ring", 4);
    knobStyleCombo.onChange = [this] {
        if (selectedKnobIndex >= 0 && selectedKnobIndex < static_cast<int>(panelState.knobs.size()))
        {
            panelState.knobs[static_cast<size_t>(selectedKnobIndex)].style = juce::jlimit(0, 3, knobStyleCombo.getSelectedId() - 1);
            knobWidgets[selectedKnobIndex]->setState(panelState.knobs[static_cast<size_t>(selectedKnobIndex)]);
            if (onStateChanged) onStateChanged();
        }
    };
    addAndMakeVisible(knobStyleCombo);

    knobSizeLabel.setFont(juce::Font(11.5f));
    addAndMakeVisible(knobSizeLabel);
    knobSizeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    knobSizeSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 18);
    knobSizeSlider.setRange(48.0, 150.0, 2.0);
    knobSizeSlider.onValueChange = [this] {
        if (selectedKnobIndex >= 0 && selectedKnobIndex < static_cast<int>(panelState.knobs.size()))
        {
            panelState.knobs[static_cast<size_t>(selectedKnobIndex)].sizePx = static_cast<int>(knobSizeSlider.getValue());
            knobWidgets[selectedKnobIndex]->setState(panelState.knobs[static_cast<size_t>(selectedKnobIndex)]);
            layoutKnobs();
            if (onStateChanged) onStateChanged();
        }
    };
    addAndMakeVisible(knobSizeSlider);

    knobDefaultLabel.setFont(juce::Font(11.5f));
    addAndMakeVisible(knobDefaultLabel);
    knobDefaultSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    knobDefaultSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 18);
    knobDefaultSlider.setRange(0.0, 100.0, 1.0);
    knobDefaultSlider.setTextValueSuffix("%");
    knobDefaultSlider.onValueChange = [this] {
        if (selectedKnobIndex >= 0 && selectedKnobIndex < static_cast<int>(panelState.knobs.size()))
        {
            panelState.knobs[static_cast<size_t>(selectedKnobIndex)].defaultValue = static_cast<float>(knobDefaultSlider.getValue() / 100.0);
            knobWidgets[selectedKnobIndex]->setState(panelState.knobs[static_cast<size_t>(selectedKnobIndex)]);
            if (onStateChanged) onStateChanged();
        }
    };
    addAndMakeVisible(knobDefaultSlider);

    knobColourLabel.setFont(juce::Font(11.5f));
    addAndMakeVisible(knobColourLabel);
    swatchHexes = { "FF00D4FF", "FFFFB800", "FF2EE59D", "FFFF5A6E", "FFA855F7", "FFF59E0B", "FFE2E8F0", "FFEC4899" };
    for (int i = 0; i < static_cast<int>(swatchHexes.size()); ++i)
    {
        auto* btn = new juce::TextButton();
        juce::Colour c = parseHexColour(swatchHexes[static_cast<size_t>(i)]);
        btn->setColour(juce::TextButton::buttonColourId, c);
        btn->onClick = [this, i] {
            if (selectedKnobIndex >= 0 && selectedKnobIndex < static_cast<int>(panelState.knobs.size()))
            {
                panelState.knobs[static_cast<size_t>(selectedKnobIndex)].colourHex = swatchHexes[static_cast<size_t>(i)];
                knobWidgets[selectedKnobIndex]->setState(panelState.knobs[static_cast<size_t>(selectedKnobIndex)]);
                if (onStateChanged) onStateChanged();
            }
        };
        addAndMakeVisible(btn);
        colourSwatches.add(btn);
    }

    deleteKnobButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF4B1D24));
    deleteKnobButton.onClick = [this] { deleteSelectedKnob(); };
    addAndMakeVisible(deleteKnobButton);

    macrosHeaderLabel.setFont(juce::Font(12.0f, juce::Font::bold));
    macrosHeaderLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textPrimary);
    addAndMakeVisible(macrosHeaderLabel);

    addMacroButton.onClick = [this] { showAddMacroMenu(); };
    addAndMakeVisible(addMacroButton);

    macroListViewport.setViewedComponent(&macroListContainer, false);
    macroListViewport.setScrollBarsShown(true, false);
    addAndMakeVisible(macroListViewport);

    // Seed 4 starter knobs if none exist yet
    if (panelState.knobs.empty())
    {
        auto makeStarter = [](const juce::String& lbl, float x, float y, const juce::String& hex, const juce::String& paramId, const juce::String& disp, float defVal) {
            InstrumentKnobState k;
            k.id = juce::Uuid().toString();
            k.label = lbl;
            k.xNorm = x;
            k.yNorm = y;
            k.sizePx = 86;
            k.style = 0;
            k.colourHex = hex;
            k.value = defVal;
            k.defaultValue = defVal;
            if (paramId.isNotEmpty())
            {
                InstrumentMacroTarget t;
                t.targetKind = 0;
                t.paramId = paramId;
                t.displayName = disp;
                k.targets.push_back(t);
            }
            return k;
        };
        panelState.knobs.push_back(makeStarter("Attack", 0.20f, 0.50f, "FF00D4FF", "attackMs", "Sampler: Attack", 0.02f));
        panelState.knobs.push_back(makeStarter("Release", 0.40f, 0.50f, "FF2EE59D", "releaseMs", "Sampler: Release", 0.10f));
        panelState.knobs.push_back(makeStarter("Macro 1", 0.60f, 0.50f, "FFFFB800", "", "", 0.50f));
        panelState.knobs.push_back(makeStarter("Macro 2", 0.80f, 0.50f, "FFA855F7", "", "", 0.50f));
    }

    rebuildKnobsFromState();
    updateToolbarVisibility();
}

InstrumentViewComponent::~InstrumentViewComponent() = default;

InstrumentPanelState InstrumentViewComponent::getState() const
{
    return panelState;
}

void InstrumentViewComponent::setState(const InstrumentPanelState& state)
{
    panelState = state;
    instrumentTitleLabel.setText(panelState.instrumentName, juce::dontSendNotification);
    bgDimSlider.setValue(panelState.backgroundDim, juce::dontSendNotification);
    bgFitCombo.setSelectedId(juce::jlimit(0, 2, panelState.backgroundFitMode) + 1, juce::dontSendNotification);
    loadBackgroundImageFromPath(panelState.backgroundImagePath);
    selectedKnobIndex = -1;
    rebuildKnobsFromState();
    updateToolbarVisibility();
    resized();
    repaint();
}

void InstrumentViewComponent::applyAllMacros()
{
    for (const auto& k : panelState.knobs)
        applyKnobMacros(k);
}

juce::Rectangle<int> InstrumentViewComponent::getCanvasBounds() const
{
    auto area = getLocalBounds();
    area.removeFromTop(38);
    if (editMode)
        area.removeFromRight(285);
    return area.reduced(8);
}

juce::Rectangle<int> InstrumentViewComponent::getInspectorBounds() const
{
    if (!editMode)
        return {};
    auto area = getLocalBounds();
    area.removeFromTop(38);
    return area.removeFromRight(285).reduced(4, 8);
}

void InstrumentViewComponent::paint(juce::Graphics& g)
{
    auto full = getLocalBounds();

    // Top control strip background
    g.setColour(juce::Colour(0xFF141820));
    g.fillRect(full.removeFromTop(38));
    g.setColour(OpenWavLookAndFeel::borderColour);
    g.drawHorizontalLine(37, 0.0f, static_cast<float>(getWidth()));

    // Instrument Canvas
    auto canvas = getCanvasBounds();
    g.setColour(juce::Colour(0xFF0E1117));
    g.fillRoundedRectangle(canvas.toFloat(), 10.0f);

    if (bgImage.isValid())
    {
        juce::Graphics::ScopedSaveState ss(g);
        juce::Path clipPath;
        clipPath.addRoundedRectangle(canvas.toFloat(), 10.0f);
        g.reduceClipRegion(clipPath);

        juce::RectanglePlacement placement = juce::RectanglePlacement::centred;
        if (panelState.backgroundFitMode == 0)
            placement = juce::RectanglePlacement::centred | juce::RectanglePlacement::fillDestination;
        else if (panelState.backgroundFitMode == 1)
            placement = juce::RectanglePlacement::centred;
        else
            placement = juce::RectanglePlacement::stretchToFit;

        g.drawImageWithin(bgImage, canvas.getX(), canvas.getY(), canvas.getWidth(), canvas.getHeight(), placement, false);

        if (panelState.backgroundDim > 0.001f)
        {
            g.setColour(juce::Colours::black.withAlpha(juce::jlimit(0.0f, 0.9f, panelState.backgroundDim)));
            g.fillRect(canvas);
        }
    }
    else
    {
        // Subtle brushed studio faceplate gradient + grid in Edit Mode
        juce::ColourGradient plateGrad(juce::Colour(0xFF181D26), static_cast<float>(canvas.getX()), static_cast<float>(canvas.getY()),
                                       juce::Colour(0xFF0F131A), static_cast<float>(canvas.getX()), static_cast<float>(canvas.getBottom()), false);
        g.setGradientFill(plateGrad);
        g.fillRoundedRectangle(canvas.toFloat(), 10.0f);
    }

    if (editMode)
    {
        g.setColour(juce::Colours::white.withAlpha(0.05f));
        const int step = 28;
        for (int x = canvas.getX() + step; x < canvas.getRight(); x += step)
            g.drawVerticalLine(x, static_cast<float>(canvas.getY()), static_cast<float>(canvas.getBottom()));
        for (int y = canvas.getY() + step; y < canvas.getBottom(); y += step)
            g.drawHorizontalLine(y, static_cast<float>(canvas.getX()), static_cast<float>(canvas.getRight()));
    }

    g.setColour(OpenWavLookAndFeel::borderColour);
    g.drawRoundedRectangle(canvas.toFloat(), 10.0f, 1.2f);

    if (knobWidgets.isEmpty())
    {
        g.setColour(OpenWavLookAndFeel::textSecondary);
        g.setFont(juce::Font(14.0f));
        g.drawText("No instrument knobs yet. Click '+ Add Knob' to build your custom instrument.",
                   canvas, juce::Justification::centred);
    }

    if (editMode)
    {
        auto insp = getInspectorBounds();
        g.setColour(juce::Colour(0xFF151922));
        g.fillRoundedRectangle(insp.toFloat(), 8.0f);
        g.setColour(OpenWavLookAndFeel::borderColour);
        g.drawRoundedRectangle(insp.toFloat(), 8.0f, 1.0f);
    }
}

void InstrumentViewComponent::resized()
{
    auto top = getLocalBounds().removeFromTop(38).reduced(8, 6);
    instrumentTitleLabel.setBounds(top.removeFromLeft(180));
    top.removeFromLeft(8);
    editLayoutButton.setBounds(top.removeFromLeft(88));
    top.removeFromLeft(6);
    addKnobButton.setBounds(top.removeFromLeft(86));
    top.removeFromLeft(10);
    setBgButton.setBounds(top.removeFromLeft(78));
    top.removeFromLeft(4);
    if (clearBgButton.isVisible())
    {
        clearBgButton.setBounds(top.removeFromLeft(70));
        top.removeFromLeft(6);
        bgFitCombo.setBounds(top.removeFromLeft(76));
        top.removeFromLeft(6);
        bgDimLabel.setBounds(top.removeFromLeft(30));
        bgDimSlider.setBounds(top.removeFromLeft(75));
    }
    fxRackButton.setBounds(top.removeFromRight(76));

    layoutKnobs();

    auto insp = getInspectorBounds().reduced(10, 10);
    if (editMode && !insp.isEmpty())
    {
        inspectorHeaderLabel.setBounds(insp.removeFromTop(20));
        insp.removeFromTop(6);

        bool hasSel = (selectedKnobIndex >= 0 && selectedKnobIndex < static_cast<int>(panelState.knobs.size()));
        noSelectionLabel.setVisible(!hasSel);
        if (!hasSel)
        {
            noSelectionLabel.setBounds(insp);
            return;
        }

        auto layoutRow = [&](juce::Label& lbl, juce::Component& comp, int h = 22) {
            auto r = insp.removeFromTop(h);
            lbl.setBounds(r.removeFromLeft(56));
            r.removeFromLeft(4);
            comp.setBounds(r);
            insp.removeFromTop(5);
        };

        layoutRow(knobNameLabel, knobNameEditor);
        layoutRow(knobStyleLabel, knobStyleCombo);
        layoutRow(knobSizeLabel, knobSizeSlider);
        layoutRow(knobDefaultLabel, knobDefaultSlider);

        auto colRow = insp.removeFromTop(22);
        knobColourLabel.setBounds(colRow.removeFromLeft(56));
        colRow.removeFromLeft(4);
        int sw = std::max(14, (colRow.getWidth() - 7 * 3) / 8);
        for (int i = 0; i < colourSwatches.size(); ++i)
        {
            colourSwatches[i]->setBounds(colRow.removeFromLeft(sw).reduced(0, 2));
            colRow.removeFromLeft(3);
        }
        insp.removeFromTop(6);

        deleteKnobButton.setBounds(insp.removeFromTop(22));
        insp.removeFromTop(10);

        macrosHeaderLabel.setBounds(insp.removeFromTop(18));
        insp.removeFromTop(4);
        addMacroButton.setBounds(insp.removeFromTop(24));
        insp.removeFromTop(6);

        macroListViewport.setBounds(insp);
        int rowH = 52;
        int totalH = std::max(insp.getHeight(), macroRows.size() * rowH);
        macroListContainer.setSize(std::max(100, insp.getWidth() - 12), totalH);
        int y = 0;
        for (auto* row : macroRows)
        {
            row->setBounds(0, y, macroListContainer.getWidth(), rowH - 4);
            y += rowH;
        }
    }
}

void InstrumentViewComponent::updateToolbarVisibility()
{
    bool hasBg = bgImage.isValid();
    clearBgButton.setVisible(hasBg);
    bgFitCombo.setVisible(hasBg);
    bgDimLabel.setVisible(hasBg);
    bgDimSlider.setVisible(hasBg);

    bool hasSel = editMode && (selectedKnobIndex >= 0 && selectedKnobIndex < static_cast<int>(panelState.knobs.size()));
    inspectorHeaderLabel.setVisible(editMode);
    noSelectionLabel.setVisible(editMode && !hasSel);

    knobNameLabel.setVisible(hasSel);
    knobNameEditor.setVisible(hasSel);
    knobStyleLabel.setVisible(hasSel);
    knobStyleCombo.setVisible(hasSel);
    knobSizeLabel.setVisible(hasSel);
    knobSizeSlider.setVisible(hasSel);
    knobDefaultLabel.setVisible(hasSel);
    knobDefaultSlider.setVisible(hasSel);
    knobColourLabel.setVisible(hasSel);
    for (auto* s : colourSwatches)
        s->setVisible(hasSel);
    deleteKnobButton.setVisible(hasSel);
    macrosHeaderLabel.setVisible(hasSel);
    addMacroButton.setVisible(hasSel);
    macroListViewport.setVisible(hasSel);
}

void InstrumentViewComponent::rebuildKnobsFromState()
{
    knobWidgets.clear();
    for (int i = 0; i < static_cast<int>(panelState.knobs.size()); ++i)
    {
        auto* kw = new InstrumentKnobWidget(panelState.knobs[static_cast<size_t>(i)]);
        kw->setEditMode(editMode);
        kw->setSelected(i == selectedKnobIndex);

        kw->onSelected = [this](InstrumentKnobWidget* w) {
            int idx = knobWidgets.indexOf(w);
            if (idx >= 0)
                selectKnob(idx);
        };

        kw->onValueChanged = [this](InstrumentKnobWidget* w, float newVal) {
            int idx = knobWidgets.indexOf(w);
            if (idx >= 0 && idx < static_cast<int>(panelState.knobs.size()))
            {
                panelState.knobs[static_cast<size_t>(idx)].value = newVal;
                applyKnobMacros(panelState.knobs[static_cast<size_t>(idx)]);
            }
        };

        kw->onMovedInEdit = [this](InstrumentKnobWidget* w, juce::Point<int> delta) {
            int idx = knobWidgets.indexOf(w);
            auto canvas = getCanvasBounds();
            if (idx < 0 || idx >= static_cast<int>(panelState.knobs.size()) || canvas.getWidth() <= 0 || canvas.getHeight() <= 0)
                return;

            auto& k = panelState.knobs[static_cast<size_t>(idx)];
            k.xNorm = juce::jlimit(0.05f, 0.95f, k.xNorm + static_cast<float>(delta.x) / static_cast<float>(canvas.getWidth()));
            k.yNorm = juce::jlimit(0.08f, 0.92f, k.yNorm + static_cast<float>(delta.y) / static_cast<float>(canvas.getHeight()));
            w->setState(k);
            layoutKnobs();
        };

        kw->onResizedInEdit = [this](InstrumentKnobWidget* w, int newSizePx) {
            int idx = knobWidgets.indexOf(w);
            if (idx >= 0 && idx < static_cast<int>(panelState.knobs.size()))
            {
                panelState.knobs[static_cast<size_t>(idx)].sizePx = newSizePx;
                w->setState(panelState.knobs[static_cast<size_t>(idx)]);
                knobSizeSlider.setValue(newSizePx, juce::dontSendNotification);
                layoutKnobs();
            }
        };

        kw->onInteractionFinished = [this] {
            if (onStateChanged)
                onStateChanged();
        };

        addAndMakeVisible(kw);
        knobWidgets.add(kw);
    }
    layoutKnobs();
}

void InstrumentViewComponent::layoutKnobs()
{
    auto canvas = getCanvasBounds();
    if (canvas.getWidth() <= 0 || canvas.getHeight() <= 0)
        return;

    for (int i = 0; i < knobWidgets.size() && i < static_cast<int>(panelState.knobs.size()); ++i)
    {
        const auto& k = panelState.knobs[static_cast<size_t>(i)];
        int w = juce::jlimit(48, 150, k.sizePx);
        int h = w + 18;
        int cx = canvas.getX() + static_cast<int>(k.xNorm * static_cast<float>(canvas.getWidth()));
        int cy = canvas.getY() + static_cast<int>(k.yNorm * static_cast<float>(canvas.getHeight()));
        int x = juce::jlimit(canvas.getX(), std::max(canvas.getX(), canvas.getRight() - w), cx - w / 2);
        int y = juce::jlimit(canvas.getY(), std::max(canvas.getY(), canvas.getBottom() - h), cy - h / 2);
        knobWidgets[i]->setBounds(x, y, w, h);
    }
}

void InstrumentViewComponent::selectKnob(int index)
{
    selectedKnobIndex = index;
    for (int i = 0; i < knobWidgets.size(); ++i)
        knobWidgets[i]->setSelected(i == selectedKnobIndex);
    syncInspectorFromSelectedKnob();
    updateToolbarVisibility();
    resized();
    repaint();
}

void InstrumentViewComponent::addNewKnob()
{
    InstrumentKnobState k;
    k.id = juce::Uuid().toString();
    k.label = "Knob " + juce::String(panelState.knobs.size() + 1);
    int col = static_cast<int>(panelState.knobs.size()) % 4;
    int row = static_cast<int>(panelState.knobs.size()) / 4;
    k.xNorm = juce::jlimit(0.15f, 0.85f, 0.20f + col * 0.20f);
    k.yNorm = juce::jlimit(0.20f, 0.80f, 0.35f + (row % 3) * 0.22f);
    k.sizePx = 84;
    k.style = 0;
    k.colourHex = swatchHexes.empty() ? juce::String("FF00D4FF") : swatchHexes[panelState.knobs.size() % swatchHexes.size()];

    panelState.knobs.push_back(k);
    rebuildKnobsFromState();
    selectKnob(static_cast<int>(panelState.knobs.size()) - 1);
    if (onStateChanged) onStateChanged();
}

void InstrumentViewComponent::deleteSelectedKnob()
{
    if (selectedKnobIndex < 0 || selectedKnobIndex >= static_cast<int>(panelState.knobs.size()))
        return;

    panelState.knobs.erase(panelState.knobs.begin() + selectedKnobIndex);
    if (panelState.knobs.empty())
        selectedKnobIndex = -1;
    else
        selectedKnobIndex = std::min(selectedKnobIndex, static_cast<int>(panelState.knobs.size()) - 1);

    rebuildKnobsFromState();
    syncInspectorFromSelectedKnob();
    updateToolbarVisibility();
    resized();
    repaint();
    if (onStateChanged) onStateChanged();
}

void InstrumentViewComponent::syncInspectorFromSelectedKnob()
{
    if (selectedKnobIndex < 0 || selectedKnobIndex >= static_cast<int>(panelState.knobs.size()))
        return;

    const auto& k = panelState.knobs[static_cast<size_t>(selectedKnobIndex)];
    knobNameEditor.setText(k.label, juce::dontSendNotification);
    knobStyleCombo.setSelectedId(juce::jlimit(0, 3, k.style) + 1, juce::dontSendNotification);
    knobSizeSlider.setValue(k.sizePx, juce::dontSendNotification);
    knobDefaultSlider.setValue(k.defaultValue * 100.0, juce::dontSendNotification);
    rebuildMacroRows();
}

void InstrumentViewComponent::rebuildMacroRows()
{
    macroRows.clear();
    if (selectedKnobIndex < 0 || selectedKnobIndex >= static_cast<int>(panelState.knobs.size()))
        return;

    const auto& targets = panelState.knobs[static_cast<size_t>(selectedKnobIndex)].targets;
    for (int i = 0; i < static_cast<int>(targets.size()); ++i)
    {
        auto* row = new MacroTargetRow(
            targets[static_cast<size_t>(i)], i,
            [this](int idx, float minR, float maxR, bool inv) {
                if (selectedKnobIndex < 0 || selectedKnobIndex >= static_cast<int>(panelState.knobs.size()))
                    return;
                auto& k = panelState.knobs[static_cast<size_t>(selectedKnobIndex)];
                if (idx >= 0 && idx < static_cast<int>(k.targets.size()))
                {
                    k.targets[static_cast<size_t>(idx)].minRange = minR;
                    k.targets[static_cast<size_t>(idx)].maxRange = maxR;
                    k.targets[static_cast<size_t>(idx)].inverted = inv;
                    applyKnobMacros(k);
                    if (onStateChanged) onStateChanged();
                }
            },
            [this](int idx) {
                if (selectedKnobIndex < 0 || selectedKnobIndex >= static_cast<int>(panelState.knobs.size()))
                    return;
                auto& k = panelState.knobs[static_cast<size_t>(selectedKnobIndex)];
                if (idx >= 0 && idx < static_cast<int>(k.targets.size()))
                {
                    k.targets.erase(k.targets.begin() + idx);
                    knobWidgets[selectedKnobIndex]->setState(k);
                    rebuildMacroRows();
                    resized();
                    if (onStateChanged) onStateChanged();
                }
            });
        macroListContainer.addAndMakeVisible(row);
        macroRows.add(row);
    }
    resized();
}

void InstrumentViewComponent::applyKnobMacros(const InstrumentKnobState& knob)
{
    if (knob.targets.empty())
        return;

    auto rackEffects = audioEngine.getPerformanceRack().getEffectsSnapshot();
    const float v = juce::jlimit(0.0f, 1.0f, knob.value);

    for (const auto& t : knob.targets)
    {
        float effectiveV = t.inverted ? (1.0f - v) : v;
        float mappedNorm = juce::jlimit(0.0f, 1.0f, t.minRange + effectiveV * (t.maxRange - t.minRange));

        if (t.targetKind == 0)
        {
            if (t.paramId == "masterGain")
            {
                audioEngine.getPerformanceRack().setMasterGain(mappedNorm * 1.5f);
            }
            else if (onSamplerParamMacroChanged)
            {
                onSamplerParamMacroChanged(t.paramId, mappedNorm);
            }
        }
        else
        {
            std::shared_ptr<RackEffectBase> targetFx;
            for (const auto& fx : rackEffects)
            {
                if (fx && t.effectId.isNotEmpty() && fx->getId() == t.effectId)
                {
                    targetFx = fx;
                    break;
                }
            }
            if (targetFx == nullptr)
            {
                for (const auto& fx : rackEffects)
                {
                    if (fx && static_cast<int>(fx->getType()) == t.effectType)
                    {
                        targetFx = fx;
                        break;
                    }
                }
            }

            if (targetFx != nullptr)
            {
                for (const auto& pInfo : targetFx->getParameterInfos())
                {
                    if (pInfo.id == t.paramId)
                    {
                        float actual = pInfo.minValue + mappedNorm * (pInfo.maxValue - pInfo.minValue);
                        if (pInfo.isInt)
                            actual = std::round(actual);
                        targetFx->setParameter(pInfo.id, actual);
                        break;
                    }
                }
            }
        }
    }
}

void InstrumentViewComponent::showAddMacroMenu()
{
    if (selectedKnobIndex < 0 || selectedKnobIndex >= static_cast<int>(panelState.knobs.size()))
        return;

    juce::PopupMenu menu;
    juce::PopupMenu samplerMenu;
    samplerMenu.addItem(1001, "Master Gain");
    samplerMenu.addItem(1002, "Global Attack");
    samplerMenu.addItem(1003, "Global Decay");
    samplerMenu.addItem(1004, "Global Sustain");
    samplerMenu.addItem(1005, "Global Release");
    menu.addSubMenu("Sampler Parameters", samplerMenu);

    auto rackEffects = audioEngine.getPerformanceRack().getEffectsSnapshot();
    struct RackParamChoice
    {
        juce::String effectId;
        int effectType;
        juce::String effectName;
        EffectParamInfo param;
    };
    std::vector<RackParamChoice> rackChoices;

    if (!rackEffects.empty())
    {
        menu.addSeparator();
        int nextId = 2000;
        for (int fxIdx = 0; fxIdx < static_cast<int>(rackEffects.size()); ++fxIdx)
        {
            const auto& fx = rackEffects[static_cast<size_t>(fxIdx)];
            if (!fx) continue;

            juce::PopupMenu fxSub;
            for (const auto& p : fx->getParameterInfos())
            {
                int itemId = nextId++;
                fxSub.addItem(itemId, p.name);
                rackChoices.push_back({ fx->getId(), static_cast<int>(fx->getType()), fx->getName(), p });
            }
            menu.addSubMenu("Rack #" + juce::String(fxIdx + 1) + ": " + fx->getName(), fxSub);
        }
    }

    menu.addSeparator();
    juce::PopupMenu addFxMenu;
    const PerformanceEffectType allTypes[] = {
        PerformanceEffectType::Filter,
        PerformanceEffectType::Reverb,
        PerformanceEffectType::Delay,
        PerformanceEffectType::Distortion,
        PerformanceEffectType::Chorus,
        PerformanceEffectType::Compressor,
        PerformanceEffectType::Bitcrusher,
        PerformanceEffectType::ParametricEQ,
        PerformanceEffectType::PitchShifter,
        PerformanceEffectType::Tremolo,
        PerformanceEffectType::Phaser,
        PerformanceEffectType::Flanger,
        PerformanceEffectType::StereoImager,
        PerformanceEffectType::AutoWah,
        PerformanceEffectType::RingModulator,
        PerformanceEffectType::AmpCabinet,
        PerformanceEffectType::ADSREnvelope
    };
    for (int i = 0; i < static_cast<int>(std::size(allTypes)); ++i)
        addFxMenu.addItem(5000 + i, effectTypeDisplayName(allTypes[i]));
    menu.addSubMenu("+ Add New Rack Effect & Map", addFxMenu);

    juce::Component::SafePointer<InstrumentViewComponent> safeThis(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&addMacroButton),
                       [safeThis, rackChoices, allTypes](int result) {
        if (safeThis == nullptr || result == 0)
            return;
        if (safeThis->selectedKnobIndex < 0 || safeThis->selectedKnobIndex >= static_cast<int>(safeThis->panelState.knobs.size()))
            return;

        auto& k = safeThis->panelState.knobs[static_cast<size_t>(safeThis->selectedKnobIndex)];
        InstrumentMacroTarget target;

        if (result >= 1001 && result <= 1005)
        {
            target.targetKind = 0;
            switch (result)
            {
                case 1001: target.paramId = "masterGain"; target.displayName = "Sampler: Master Gain"; break;
                case 1002: target.paramId = "attackMs"; target.displayName = "Sampler: Attack"; break;
                case 1003: target.paramId = "decayMs"; target.displayName = "Sampler: Decay"; break;
                case 1004: target.paramId = "sustain"; target.displayName = "Sampler: Sustain"; break;
                case 1005: target.paramId = "releaseMs"; target.displayName = "Sampler: Release"; break;
                default: break;
            }
        }
        else if (result >= 2000 && result < 2000 + static_cast<int>(rackChoices.size()))
        {
            const auto& choice = rackChoices[static_cast<size_t>(result - 2000)];
            target.targetKind = 1;
            target.effectId = choice.effectId;
            target.effectType = choice.effectType;
            target.paramId = choice.param.id;
            target.displayName = choice.effectName + ": " + choice.param.name;
        }
        else if (result >= 5000 && result < 5000 + static_cast<int>(std::size(allTypes)))
        {
            auto type = allTypes[result - 5000];
            auto newFx = safeThis->audioEngine.getPerformanceRack().addEffect(type);
            if (!newFx)
                return;

            auto infos = newFx->getParameterInfos();
            if (infos.empty())
                return;

            target.targetKind = 1;
            target.effectId = newFx->getId();
            target.effectType = static_cast<int>(type);
            target.paramId = infos[0].id;
            target.displayName = newFx->getName() + ": " + infos[0].name;
        }
        else
        {
            return;
        }

        k.targets.push_back(target);
        safeThis->knobWidgets[safeThis->selectedKnobIndex]->setState(k);
        safeThis->applyKnobMacros(k);
        safeThis->rebuildMacroRows();
        if (safeThis->onStateChanged)
            safeThis->onStateChanged();
    });
}

void InstrumentViewComponent::chooseBackgroundImage()
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Select Instrument Background Image",
        juce::File::getSpecialLocation(juce::File::userPicturesDirectory),
        "*.png;*.jpg;*.jpeg;*.bmp;*.gif",
        true);

    juce::Component::SafePointer<InstrumentViewComponent> safeThis(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                         [safeThis, chooser](const juce::FileChooser& fc) {
        if (safeThis == nullptr)
            return;
        auto file = fc.getResult();
        if (file.existsAsFile())
        {
            safeThis->panelState.backgroundImagePath = file.getFullPathName();
            safeThis->loadBackgroundImageFromPath(safeThis->panelState.backgroundImagePath);
            safeThis->updateToolbarVisibility();
            safeThis->resized();
            safeThis->repaint();
            if (safeThis->onStateChanged)
                safeThis->onStateChanged();
        }
    });
}

void InstrumentViewComponent::clearBackgroundImage()
{
    panelState.backgroundImagePath.clear();
    bgImage = juce::Image();
    updateToolbarVisibility();
    resized();
    repaint();
    if (onStateChanged)
        onStateChanged();
}

void InstrumentViewComponent::loadBackgroundImageFromPath(const juce::String& path)
{
    bgImage = juce::Image();
    if (path.isNotEmpty())
    {
        juce::File f(path);
        if (f.existsAsFile())
            bgImage = juce::ImageFileFormat::loadFrom(f);
    }
}

} // namespace openwav
