#include "PerformanceComponent.h"
#include <cmath>
#include <algorithm>

#if __has_include(<BinaryData.h>)
 #include <BinaryData.h>
#endif

namespace openwav
{

// ─────────────────────────────────────────────────────────────────────────────
//  SampleSonics Brand Logo Resource Loader
// ─────────────────────────────────────────────────────────────────────────────
static juce::Image getSampleSonicsLogo()
{
    static juce::Image cachedLogo;
    static bool attemptedLoad = false;

    if (attemptedLoad)
        return cachedLogo;

    attemptedLoad = true;
    juce::Image rawImage;

#if defined(JUCE_BINARYDATA_H_INCLUDED) || __has_include(<BinaryData.h>)
    int dataSize = 0;
    const char* data = BinaryData::getNamedResource("samplesonics_com_png", dataSize);
    if (data == nullptr)
        data = BinaryData::getNamedResource("samplesonics.com.png", dataSize);

    if (data != nullptr && dataSize > 0)
        rawImage = juce::ImageFileFormat::loadFrom(data, static_cast<size_t>(dataSize));
#endif

    if (!rawImage.isValid())
    {
        const juce::String filename = "samplesonics.com.png";
        const juce::File searchPaths[] = {
            juce::File::getCurrentWorkingDirectory().getChildFile(filename),
            juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile(filename),
            juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getParentDirectory().getChildFile("Resources").getChildFile(filename),
            juce::File("/Users/eoinodowd/Desktop/owmb latest").getChildFile(filename)
        };

        for (const auto& f : searchPaths)
        {
            if (f.existsAsFile())
            {
                rawImage = juce::ImageFileFormat::loadFrom(f);
                if (rawImage.isValid())
                    break;
            }
        }
    }

    if (!rawImage.isValid())
        return {};

    // Convert to ARGB with transparent background so the white soundwave mark
    // floats cleanly onto any metallic chassis gradient with crisp anti-aliasing
    int w = rawImage.getWidth();
    int h = rawImage.getHeight();
    cachedLogo = juce::Image(juce::Image::ARGB, w, h, true);

    juce::Image::BitmapData srcData(rawImage, juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData dstData(cachedLogo, juce::Image::BitmapData::writeOnly);

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            auto c = srcData.getPixelColour(x, y);
            float lum = c.getBrightness();
            // Background is ~54/255 (0.21), foreground white soundwave is ~241/255 (0.945)
            float alpha = juce::jlimit(0.0f, 1.0f, (lum - 0.21f) / (0.94f - 0.21f));
            dstData.setPixelColour(x, y, juce::Colours::white.withAlpha(alpha));
        }
    }

    return cachedLogo;
}

// ─────────────────────────────────────────────────────────────────────────────
//  RackKnobLookAndFeel Implementation
// ─────────────────────────────────────────────────────────────────────────────
RackKnobLookAndFeel::RackKnobLookAndFeel()
{
}

void RackKnobLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPosProportional, float rotaryStartAngle,
                                           float rotaryEndAngle, juce::Slider& /*slider*/)
{
    auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(4.0f);
    float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
    float centreX = bounds.getCentreX();
    float centreY = bounds.getCentreY();
    float rx = centreX - radius;
    float ry = centreY - radius;
    float rw = radius * 2.0f;
    float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    // 1. Drop shadow / recess
    g.setColour(juce::Colour(10, 12, 14).withAlpha(0.7f));
    g.fillEllipse(rx + 1.0f, ry + 2.0f, rw, rw);

    // 2. Dial body metallic gradient
    juce::ColourGradient dialGrad(juce::Colour(46, 50, 58), centreX, ry,
                                 juce::Colour(24, 26, 30), centreX, ry + rw, false);
    g.setGradientFill(dialGrad);
    g.fillEllipse(rx, ry, rw, rw);

    // 3. Dial outer metallic rim with vintage knurling/weathering
    g.setColour(juce::Colour(72, 78, 90));
    g.drawEllipse(rx, ry, rw, rw, 1.2f);
    g.setColour(juce::Colour(22, 24, 28));
    g.drawEllipse(rx + 1.0f, ry + 1.0f, rw - 2.0f, rw - 2.0f, 0.8f);

    // 4. Background Track Arc
    juce::Path bgArc;
    bgArc.addCentredArc(centreX, centreY, radius - 4.5f, radius - 4.5f, 0.0f,
                        rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(juce::Colour(36, 40, 48));
    g.strokePath(bgArc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // 5. Active Value Track Arc (accent coloured)
    if (sliderPosProportional > 0.001f)
    {
        juce::Path valArc;
        valArc.addCentredArc(centreX, centreY, radius - 4.5f, radius - 4.5f, 0.0f,
                            rotaryStartAngle, angle, true);
        g.setColour(accent);
        g.strokePath(valArc, juce::PathStrokeType(3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // 6. Inner dial cap
    float innerR = radius * 0.72f;
    juce::ColourGradient capGrad(juce::Colour(34, 38, 44), centreX, centreY - innerR,
                                juce::Colour(18, 20, 24), centreX, centreY + innerR, false);
    g.setGradientFill(capGrad);
    g.fillEllipse(centreX - innerR, centreY - innerR, innerR * 2.0f, innerR * 2.0f);
    g.setColour(juce::Colour(55, 60, 70));
    g.drawEllipse(centreX - innerR, centreY - innerR, innerR * 2.0f, innerR * 2.0f, 1.0f);
    // Subtle inner aged groove
    g.setColour(juce::Colour(16, 18, 22).withAlpha(0.85f));
    g.drawEllipse(centreX - innerR + 1.0f, centreY - innerR + 1.0f, (innerR - 1.0f) * 2.0f, (innerR - 1.0f) * 2.0f, 0.7f);

    // 7. Rotary Pointer needle
    juce::Path p;
    float pointerLength = radius * 0.65f;
    float pointerThickness = 2.2f;
    p.addRoundedRectangle(-pointerThickness * 0.5f, -pointerLength, pointerThickness, pointerLength * 0.75f, 1.0f);
    p.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));
    g.setColour(juce::Colours::white);
    g.fillPath(p);
}

// ─────────────────────────────────────────────────────────────────────────────
//  RackKnobControl Implementation
// ─────────────────────────────────────────────────────────────────────────────
RackKnobControl::RackKnobControl(const EffectParamInfo& info,
                                 float initialValue,
                                 juce::Colour accentColour,
                                 std::function<void(float)> onParamChange)
    : paramInfo(info), onChange(std::move(onParamChange))
{
    knobLnf.setAccentColour(accentColour);

    // Title
    titleLabel.setText(info.name.toUpperCase(), juce::dontSendNotification);
    titleLabel.setFont(juce::Font(10.0f).boldened());
    titleLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);

    if (!info.choiceLabels.isEmpty())
    {
        isChoice = true;
        choiceCombo = std::make_unique<juce::ComboBox>();
        for (int i = 0; i < info.choiceLabels.size(); ++i)
            choiceCombo->addItem(info.choiceLabels[i], i + 1);

        int selectedId = juce::jlimit(1, info.choiceLabels.size(), static_cast<int>(std::round(initialValue)) + 1);
        choiceCombo->setSelectedId(selectedId, juce::dontSendNotification);
        choiceCombo->addListener(this);
        addAndMakeVisible(*choiceCombo);
    }
    else
    {
        isChoice = false;
        knobSlider.setLookAndFeel(&knobLnf);
        knobSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        knobSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);

        if (info.isInt)
            knobSlider.setRange(info.minValue, info.maxValue, 1.0);
        else
            knobSlider.setRange(info.minValue, info.maxValue, (info.maxValue - info.minValue) / 200.0);

        knobSlider.setValue(initialValue, juce::dontSendNotification);
        knobSlider.addListener(this);
        addAndMakeVisible(knobSlider);

        // Value readout label
        valueLabel.setFont(juce::Font(11.0f));
        valueLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textPrimary);
        valueLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(valueLabel);

        setValue(initialValue, false);
    }
}

RackKnobControl::~RackKnobControl()
{
    knobSlider.setLookAndFeel(nullptr);
}

void RackKnobControl::resized()
{
    auto area = getLocalBounds();
    titleLabel.setBounds(area.removeFromTop(16));

    if (isChoice && choiceCombo)
    {
        area.reduce(4, 8);
        choiceCombo->setBounds(area.withHeight(24).withY(area.getCentreY() - 12));
    }
    else
    {
        valueLabel.setBounds(area.removeFromBottom(16));
        knobSlider.setBounds(area.reduced(2));
    }
}

void RackKnobControl::sliderValueChanged(juce::Slider* slider)
{
    if (slider == &knobSlider)
    {
        float val = static_cast<float>(knobSlider.getValue());
        setValue(val, false);
        if (onChange)
            onChange(val);
    }
}

void RackKnobControl::comboBoxChanged(juce::ComboBox* comboBox)
{
    if (comboBox == choiceCombo.get())
    {
        float val = static_cast<float>(choiceCombo->getSelectedId() - 1);
        if (onChange)
            onChange(val);
    }
}

void RackKnobControl::setValue(float val, bool sendNotification)
{
    if (isChoice && choiceCombo)
    {
        int id = static_cast<int>(std::round(val)) + 1;
        choiceCombo->setSelectedId(id, sendNotification ? juce::sendNotificationAsync : juce::dontSendNotification);
    }
    else
    {
        knobSlider.setValue(val, sendNotification ? juce::sendNotificationAsync : juce::dontSendNotification);

        juce::String str;
        if (paramInfo.isInt)
        {
            str = juce::String(static_cast<int>(std::round(val)));
        }
        else
        {
            if (std::abs(val) >= 100.0f)
                str = juce::String(static_cast<int>(std::round(val)));
            else if (std::abs(val) >= 10.0f)
                str = juce::String(val, 1);
            else
                str = juce::String(val, 2);
        }

        if (paramInfo.suffix.isNotEmpty())
            str += " " + paramInfo.suffix;

        valueLabel.setText(str, juce::dontSendNotification);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  RackUnitComponent Implementation
// ─────────────────────────────────────────────────────────────────────────────
RackUnitComponent::RackUnitComponent(std::shared_ptr<RackEffectBase> fx,
                                     int unitIndex,
                                     int totalUnits,
                                     std::function<void(int)> onMoveUp,
                                     std::function<void(int)> onMoveDown,
                                     std::function<void(int)> onDelete,
                                     std::function<void()> onParamsChanged)
    : effect(std::move(fx)),
      index(unitIndex),
      totalCount(totalUnits),
      moveUpCallback(std::move(onMoveUp)),
      moveDownCallback(std::move(onMoveDown)),
      deleteCallback(std::move(onDelete)),
      paramsChangedCallback(std::move(onParamsChanged))
{
    // Slot label
    slotLabel.setText(juce::String::formatted("#%02d", index + 1), juce::dontSendNotification);
    slotLabel.setFont(juce::Font(11.0f).boldened());
    slotLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    slotLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(slotLabel);

    // Power / Bypass toggle button
    bool bypassed = effect ? effect->getBypassed() : false;
    powerButton.setButtonText(bypassed ? "BYPASS" : "ACTIVE");
    powerButton.setClickingTogglesState(true);
    powerButton.setToggleState(!bypassed, juce::dontSendNotification);
    powerButton.addListener(this);
    addAndMakeVisible(powerButton);

    // Effect title badge
    titleBadge.setText(effect ? effect->getName().toUpperCase() : "MODULE", juce::dontSendNotification);
    titleBadge.setFont(juce::Font(12.0f).boldened());
    titleBadge.setColour(juce::Label::textColourId, juce::Colours::white);
    titleBadge.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleBadge);

    // Presets combo
    if (effect)
    {
        auto presets = effect->getPresetNames();
        for (int i = 0; i < presets.size(); ++i)
            presetCombo.addItem("Preset: " + presets[i], i + 1);

        if (!presets.isEmpty())
            presetCombo.setSelectedId(1, juce::dontSendNotification);

        presetCombo.addListener(this);
        addAndMakeVisible(presetCombo);
    }

    // Reorder and Delete buttons
    btnMoveUp.addListener(this);
    btnMoveDown.addListener(this);
    btnDelete.addListener(this);

    btnMoveUp.setEnabled(index > 0);
    btnMoveDown.setEnabled(index < totalCount - 1);

    addAndMakeVisible(btnMoveUp);
    addAndMakeVisible(btnMoveDown);
    addAndMakeVisible(btnDelete);

    // Create Parameter Knobs
    if (effect)
    {
        auto paramInfos = effect->getParameterInfos();
        auto accent = effect->getAccentColour();

        for (const auto& info : paramInfos)
        {
            float initVal = effect->getParameter(info.id);
            auto* ctrl = new RackKnobControl(info, initVal, accent, [this, id = info.id](float val) {
                if (effect)
                    effect->setParameter(id, val);
                if (paramsChangedCallback)
                    paramsChangedCallback();
            });
            controls.add(ctrl);
            addAndMakeVisible(ctrl);
        }
    }
}

RackUnitComponent::~RackUnitComponent()
{
}

void RackUnitComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    bool isBypassed = effect ? effect->getBypassed() : false;

    // 1. Clean Sleek Chassis Metallic Gradient
    juce::ColourGradient chassisGrad(juce::Colour(34, 38, 44), 0.0f, bounds.getY(),
                                     juce::Colour(22, 24, 28), 0.0f, bounds.getBottom(), false);
    g.setGradientFill(chassisGrad);
    g.fillRoundedRectangle(bounds, 5.0f);

    // Subtle 1px inner chamfer highlight at the top for clean hardware depth
    g.setColour(juce::Colours::white.withAlpha(0.06f));
    g.drawRoundedRectangle(bounds.reduced(1.0f), 4.5f, 1.0f);

    // 2. 19" Rack Mounting Ears (Left & Right 38px)
    float earWidth = 38.0f;
    juce::Rectangle<float> leftEar(bounds.getX(), bounds.getY(), earWidth, bounds.getHeight());
    juce::Rectangle<float> rightEar(bounds.getRight() - earWidth, bounds.getY(), earWidth, bounds.getHeight());

    juce::ColourGradient earGrad(juce::Colour(22, 24, 28), 0.0f, bounds.getY(),
                                 juce::Colour(16, 18, 20), 0.0f, bounds.getBottom(), false);
    g.setGradientFill(earGrad);
    g.fillRect(leftEar);
    g.fillRect(rightEar);

    // 3. Clean Seams separating rack ears from module faceplate
    g.setColour(juce::Colour(45, 50, 58));
    g.drawVerticalLine(static_cast<int>(leftEar.getRight()), bounds.getY(), bounds.getBottom());
    g.drawVerticalLine(static_cast<int>(rightEar.getX()), bounds.getY(), bounds.getBottom());

    // 4. Precision Rack Mount Screws
    auto drawScrew = [&g](float cx, float cy) {
        float r = 5.5f;
        // Screw recess
        g.setColour(juce::Colour(12, 14, 16));
        g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);
        // Polished metal rim
        g.setColour(juce::Colour(75, 82, 92));
        g.drawEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f, 1.0f);
        // Precision screw slot cross
        g.setColour(juce::Colour(32, 36, 42));
        g.drawLine(cx - 3.0f, cy - 3.0f, cx + 3.0f, cy + 3.0f, 1.2f);
        g.drawLine(cx - 3.0f, cy + 3.0f, cx + 3.0f, cy - 3.0f, 1.2f);
    };

    drawScrew(leftEar.getCentreX(), bounds.getY() + 18.0f);
    drawScrew(leftEar.getCentreX(), bounds.getBottom() - 18.0f);
    drawScrew(rightEar.getCentreX(), bounds.getY() + 18.0f);
    drawScrew(rightEar.getCentreX(), bounds.getBottom() - 18.0f);

    // 5. Clean Top Accent Color Strip
    if (effect)
    {
        auto accent = effect->getAccentColour();
        if (isBypassed) accent = accent.withAlpha(0.35f);
        g.setColour(accent);
        g.fillRoundedRectangle(bounds.getX() + earWidth, bounds.getY(), bounds.getWidth() - earWidth * 2.0f, 3.0f, 1.5f);
    }

    // 6. SampleSonics Brand Logo
    drawSampleSonicsLogo(g, bounds, earWidth);

    // 7. Outer Bevel / Border
    g.setColour(juce::Colour(52, 58, 68));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 5.0f, 1.0f);

    // 8. If bypassed, draw subtle darkening overlay
    if (isBypassed)
    {
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.fillRoundedRectangle(bounds.reduced(earWidth, 0.0f), 0.0f);
    }
}

void RackUnitComponent::drawSampleSonicsLogo(juce::Graphics& g, const juce::Rectangle<float>& bounds, float earWidth)
{
    auto logoImg = getSampleSonicsLogo();
    if (!logoImg.isValid())
        return;

    bool isBypassed = effect ? effect->getBypassed() : false;
    float alpha = isBypassed ? 0.35f : 0.90f;

    // 1. Header Bar Brand Badge (between preset combo and action buttons)
    float headerY = bounds.getY() + 9.0f;
    float headerH = 24.0f;
    float rightActionX = bounds.getRight() - earWidth - 96.0f;
    float presetRightX = presetCombo.isVisible() ? presetCombo.getBounds().toFloat().getRight() + 14.0f
                                                 : titleBadge.getBounds().toFloat().getRight() + 14.0f;

    float availableWidth = rightActionX - presetRightX;
    if (availableWidth >= 110.0f)
    {
        // Draw Logo Icon + SAMPLESONICS text
        float iconSize = 20.0f;
        float iconX = rightActionX - 116.0f;
        float iconY = headerY + (headerH - iconSize) * 0.5f;

        g.saveState();
        g.setOpacity(alpha);
        g.drawImage(logoImg,
                    iconX, iconY, iconSize, iconSize,
                    0, 0, logoImg.getWidth(), logoImg.getHeight());
        g.restoreState();

        g.setFont(juce::Font(juce::Font::getDefaultSansSerifFontName(), 10.0f, juce::Font::bold));
        g.setColour(juce::Colour(215, 220, 230).withAlpha(alpha));
        g.drawText("SAMPLESONICS", iconX + iconSize + 6.0f, headerY, 90.0f, headerH,
                   juce::Justification::centredLeft, false);
    }
    else if (availableWidth >= 30.0f)
    {
        // Just the logo icon if space is tight
        float iconSize = 20.0f;
        float iconX = rightActionX - iconSize - 8.0f;
        float iconY = headerY + (headerH - iconSize) * 0.5f;

        g.saveState();
        g.setOpacity(alpha);
        g.drawImage(logoImg,
                    iconX, iconY, iconSize, iconSize,
                    0, 0, logoImg.getWidth(), logoImg.getHeight());
        g.restoreState();
    }

    // 2. Subtle Precision Studio Hardware Emblem in Bottom-Right Corner
    float cornerSize = 20.0f;
    float cornerX = bounds.getRight() - earWidth - cornerSize - 14.0f;
    float cornerY = bounds.getBottom() - cornerSize - 10.0f;

    g.saveState();
    g.setOpacity(isBypassed ? 0.20f : 0.65f);
    g.drawImage(logoImg,
                cornerX, cornerY, cornerSize, cornerSize,
                0, 0, logoImg.getWidth(), logoImg.getHeight());
    g.restoreState();
}

void RackUnitComponent::resized()
{
    auto area = getLocalBounds().reduced(38, 6); // Exclude rack ears

    // Top Header Bar
    auto headerArea = area.removeFromTop(30);

    // Left items: Slot badge, Power LED button, Title badge
    slotLabel.setBounds(headerArea.removeFromLeft(36));
    headerArea.removeFromLeft(4);

    powerButton.setBounds(headerArea.removeFromLeft(68).reduced(0, 3));
    headerArea.removeFromLeft(8);

    titleBadge.setBounds(headerArea.removeFromLeft(150).reduced(0, 2));
    headerArea.removeFromLeft(12);

    // Right items: Delete, Down, Up
    btnDelete.setBounds(headerArea.removeFromRight(26).reduced(0, 3));
    headerArea.removeFromRight(4);
    btnMoveDown.setBounds(headerArea.removeFromRight(26).reduced(0, 3));
    headerArea.removeFromRight(4);
    btnMoveUp.setBounds(headerArea.removeFromRight(26).reduced(0, 3));
    headerArea.removeFromRight(12);

    // Center preset combo
    int presetWidth = juce::jlimit(130, 210, headerArea.getWidth() - 20);
    presetCombo.setBounds(headerArea.removeFromLeft(presetWidth).reduced(0, 3));

    // Parameter Knobs Row
    area.removeFromTop(6);
    int numKnobs = controls.size();
    if (numKnobs > 0)
    {
        int knobWidth = juce::jlimit(54, 96, area.getWidth() / numKnobs);
        int totalWidth = knobWidth * numKnobs;
        int startX = area.getX() + (area.getWidth() - totalWidth) / 2;

        for (int i = 0; i < numKnobs; ++i)
        {
            controls[i]->setBounds(startX + i * knobWidth, area.getY(), knobWidth, area.getHeight());
        }
    }
}

void RackUnitComponent::buttonClicked(juce::Button* button)
{
    if (button == &powerButton)
    {
        bool active = powerButton.getToggleState();
        if (effect)
            effect->setBypassed(!active);

        powerButton.setButtonText(active ? "ACTIVE" : "BYPASS");
        repaint();
        if (paramsChangedCallback)
            paramsChangedCallback();
    }
    else if (button == &btnMoveUp)
    {
        if (moveUpCallback)
            moveUpCallback(index);
    }
    else if (button == &btnMoveDown)
    {
        if (moveDownCallback)
            moveDownCallback(index);
    }
    else if (button == &btnDelete)
    {
        if (deleteCallback)
            deleteCallback(index);
    }
}

void RackUnitComponent::comboBoxChanged(juce::ComboBox* comboBox)
{
    if (comboBox == &presetCombo && effect)
    {
        int presetIdx = presetCombo.getSelectedId() - 1;
        if (presetIdx >= 0)
        {
            effect->loadPreset(presetIdx);
            syncKnobValues();
            if (paramsChangedCallback)
                paramsChangedCallback();
        }
    }
}

void RackUnitComponent::syncKnobValues()
{
    if (!effect) return;
    auto pInfos = effect->getParameterInfos();
    for (int i = 0; i < controls.size() && i < static_cast<int>(pInfos.size()); ++i)
    {
        float val = effect->getParameter(pInfos[i].id);
        controls[i]->setValue(val, false);
    }
}

void RackUnitComponent::updateTelemetry()
{
    if (effect)
    {
        float t = effect->getTelemetryMeter();
        if (std::abs(t - telemetryValue) > 0.05f)
        {
            telemetryValue = t;
            repaint();
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  PerformanceComponent Implementation
// ─────────────────────────────────────────────────────────────────────────────
PerformanceComponent::PerformanceComponent(AudioEngine& engine)
    : audioEngine(engine), rackBay(*this)
{
    // Rack Title & Count
    rackTitleLabel.setText("SAMPLE MAP FX RACK", juce::dontSendNotification);
    rackTitleLabel.setFont(juce::Font(14.0f).boldened());
    rackTitleLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    addAndMakeVisible(rackTitleLabel);

    targetSubtitleLabel.setText("Zones & MIDI Instrument", juce::dontSendNotification);
    targetSubtitleLabel.setFont(juce::Font(10.5f).italicised());
    targetSubtitleLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    addAndMakeVisible(targetSubtitleLabel);

    unitCountLabel.setText("0 Units Active", juce::dontSendNotification);
    unitCountLabel.setFont(juce::Font(11.0f));
    unitCountLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    addAndMakeVisible(unitCountLabel);

    // Add Effect Button
    btnAddEffect.onClick = [this] { showAddEffectMenu(); };
    addAndMakeVisible(btnAddEffect);

    // Rack Presets Button
    btnPresets.onClick = [this] { showPresetsMenu(); };
    addAndMakeVisible(btnPresets);

    // Clear All Button
    btnClearAll.onClick = [this] {
        audioEngine.getPerformanceRack().clearEffects();
        refreshRack();
    };
    addAndMakeVisible(btnClearAll);

    // Master Bypass Button
    btnMasterBypass.setClickingTogglesState(true);
    btnMasterBypass.setToggleState(false, juce::dontSendNotification);
    btnMasterBypass.onClick = [this] {
        bool bypass = btnMasterBypass.getToggleState();
        audioEngine.getPerformanceRack().setMasterBypassed(bypass);
        btnMasterBypass.setButtonText(bypass ? "Bypassed" : "Master Active");
        repaint();
    };
    btnMasterBypass.setButtonText("Master Active");
    addAndMakeVisible(btnMasterBypass);

    // Master Gain
    masterGainLabel.setFont(juce::Font(11.0f).boldened());
    masterGainLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    masterGainLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(masterGainLabel);

    masterGainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    masterGainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 18);
    masterGainSlider.setRange(0.0, 2.0, 0.01);
    masterGainSlider.setValue(1.0, juce::dontSendNotification);
    masterGainSlider.textFromValueFunction = [](double val) {
        if (val <= 0.0001) return juce::String("-inf dB");
        double db = 20.0 * std::log10(val);
        return juce::String(db, 1) + " dB";
    };
    masterGainSlider.addListener(this);
    addAndMakeVisible(masterGainSlider);

    // Viewport & Rack Bay
    rackViewport.setViewedComponent(&rackBay, false);
    rackViewport.setScrollBarsShown(true, false);
    addAndMakeVisible(rackViewport);

    startTimerHz(25);
    refreshRack();
}

PerformanceComponent::~PerformanceComponent()
{
    stopTimer();
}

void PerformanceComponent::paint(juce::Graphics& g)
{
    g.fillAll(OpenWavLookAndFeel::bgDark);

    // Toolbar bottom divider
    g.setColour(OpenWavLookAndFeel::borderColour);
    g.drawHorizontalLine(52, 0.0f, static_cast<float>(getWidth()));
}

void PerformanceComponent::resized()
{
    auto area = getLocalBounds();

    // Top Toolbar (height 52px)
    auto topBar = area.removeFromTop(52).reduced(12, 8);
    int btnH = 32;

    auto titleArea = topBar.removeFromLeft(175);
    rackTitleLabel.setBounds(titleArea.removeFromTop(18));
    targetSubtitleLabel.setBounds(titleArea.removeFromTop(14));

    unitCountLabel.setBounds(topBar.removeFromLeft(95).withHeight(btnH));
    topBar.removeFromLeft(6);

    btnAddEffect.setBounds(topBar.removeFromLeft(105).withHeight(btnH));
    topBar.removeFromLeft(6);
    btnPresets.setBounds(topBar.removeFromLeft(105).withHeight(btnH));
    topBar.removeFromLeft(6);
    btnClearAll.setBounds(topBar.removeFromLeft(78).withHeight(btnH));

    // Right-aligned master controls
    masterGainSlider.setBounds(topBar.removeFromRight(150).withHeight(btnH));
    masterGainLabel.setBounds(topBar.removeFromRight(50).withHeight(btnH));
    topBar.removeFromRight(12);
    btnMasterBypass.setBounds(topBar.removeFromRight(110).withHeight(btnH));

    // Remaining area for rack bay viewport
    rackViewport.setBounds(area);

    // Trigger rack bay resized to calculate required height
    rackBay.resized();
}

void PerformanceComponent::buttonClicked(juce::Button* /*button*/)
{
}

void PerformanceComponent::sliderValueChanged(juce::Slider* slider)
{
    if (slider == &masterGainSlider)
    {
        float g = static_cast<float>(masterGainSlider.getValue());
        audioEngine.getPerformanceRack().setMasterGain(g);
    }
}

void PerformanceComponent::comboBoxChanged(juce::ComboBox* /*comboBox*/)
{
}

void PerformanceComponent::timerCallback()
{
    for (auto* unit : rackUnits)
    {
        if (unit)
            unit->updateTelemetry();
    }
}

void PerformanceComponent::showAddEffectMenu()
{
    juce::PopupMenu m;

    juce::PopupMenu spatialMenu;
    spatialMenu.addItem(1, "Studio Reverb");
    spatialMenu.addItem(2, "Stereo Delay");
    spatialMenu.addItem(11, "Tape & BBD Flanger");
    spatialMenu.addItem(12, "Stereo Width & Imager");
    m.addSubMenu("Spatial & Time", spatialMenu);

    juce::PopupMenu dynamicsMenu;
    dynamicsMenu.addItem(17, "Global ADSR Envelope");
    dynamicsMenu.addItem(3, "Analog Saturation");
    dynamicsMenu.addItem(4, "VCA Compressor");
    dynamicsMenu.addItem(5, "3-Band Parametric EQ");
    dynamicsMenu.addItem(6, "Multi-Mode SVF Filter");
    dynamicsMenu.addItem(13, "Dynamic Auto-Wah");
    dynamicsMenu.addItem(14, "Amp & Cabinet Simulator");
    m.addSubMenu("Dynamics & Tone", dynamicsMenu);

    juce::PopupMenu modMenu;
    modMenu.addItem(7, "Stereo Chorus");
    modMenu.addItem(15, "Analog Phaser");
    modMenu.addItem(8, "Pitch Harmonizer");
    modMenu.addItem(9, "Tremolo & Auto-Panner");
    modMenu.addItem(16, "Metallic Ring Modulator");
    m.addSubMenu("Modulation & Pitch", modMenu);

    juce::PopupMenu lofiMenu;
    lofiMenu.addItem(10, "Vintage Bitcrusher");
    m.addSubMenu("Lo-Fi & Creative", lofiMenu);

    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&btnAddEffect),
                    [this](int result) {
        if (result <= 0) return;

        PerformanceEffectType type = PerformanceEffectType::Reverb;
        switch (result)
        {
            case 1: type = PerformanceEffectType::Reverb; break;
            case 2: type = PerformanceEffectType::Delay; break;
            case 3: type = PerformanceEffectType::Distortion; break;
            case 4: type = PerformanceEffectType::Compressor; break;
            case 5: type = PerformanceEffectType::ParametricEQ; break;
            case 6: type = PerformanceEffectType::Filter; break;
            case 7: type = PerformanceEffectType::Chorus; break;
            case 8: type = PerformanceEffectType::PitchShifter; break;
            case 9: type = PerformanceEffectType::Tremolo; break;
            case 10: type = PerformanceEffectType::Bitcrusher; break;
            case 11: type = PerformanceEffectType::Flanger; break;
            case 12: type = PerformanceEffectType::StereoImager; break;
            case 13: type = PerformanceEffectType::AutoWah; break;
            case 14: type = PerformanceEffectType::AmpCabinet; break;
            case 15: type = PerformanceEffectType::Phaser; break;
            case 16: type = PerformanceEffectType::RingModulator; break;
            case 17: type = PerformanceEffectType::ADSREnvelope; break;
            default: return;
        }

        audioEngine.getPerformanceRack().addEffect(type);
        refreshRack();
    });
}

void PerformanceComponent::showPresetsMenu()
{
    juce::PopupMenu m;
    auto templates = audioEngine.getPerformanceRack().getRackTemplateNames();
    for (int i = 0; i < templates.size(); ++i)
    {
        if (i == 1) m.addSeparator();
        m.addItem(i + 1, templates[i]);
    }

    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&btnPresets),
                    [this](int result) {
        if (result <= 0) return;
        audioEngine.getPerformanceRack().loadRackTemplate(result - 1);
        refreshRack();
    });
}

void PerformanceComponent::refreshRack()
{
    rackUnits.clear();

    auto snapshot = audioEngine.getPerformanceRack().getEffectsSnapshot();
    int total = static_cast<int>(snapshot.size());

    for (int i = 0; i < total; ++i)
    {
        auto* unit = new RackUnitComponent(
            snapshot[i],
            i,
            total,
            [this](int idx) {
                audioEngine.getPerformanceRack().moveEffect(idx, idx - 1);
                refreshRack();
            },
            [this](int idx) {
                audioEngine.getPerformanceRack().moveEffect(idx, idx + 1);
                refreshRack();
            },
            [this](int idx) {
                audioEngine.getPerformanceRack().removeEffect(idx);
                refreshRack();
            },
            [this]() {
                // Params tweaked
            }
        );
        rackUnits.add(unit);
        rackBay.addAndMakeVisible(unit);
    }

    unitCountLabel.setText(juce::String(total) + (total == 1 ? " Unit Active" : " Units Active"),
                           juce::dontSendNotification);

    // Sync master bypass toggle
    bool bypass = audioEngine.getPerformanceRack().isMasterBypassed();
    btnMasterBypass.setToggleState(bypass, juce::dontSendNotification);
    btnMasterBypass.setButtonText(bypass ? "Bypassed" : "Master Active");

    // Sync master gain
    masterGainSlider.setValue(audioEngine.getPerformanceRack().getMasterGain(), juce::dontSendNotification);

    rackBay.resized();
    rackBay.repaint();
}

PerformanceRackState PerformanceComponent::getState() const
{
    PerformanceRackState state;
    state.masterBypass = audioEngine.getPerformanceRack().isMasterBypassed();
    state.masterGain = audioEngine.getPerformanceRack().getMasterGain();

    auto snapshot = audioEngine.getPerformanceRack().getEffectsSnapshot();
    for (const auto& fx : snapshot)
    {
        if (!fx) continue;
        RackEffectState res;
        res.effectType = static_cast<int>(fx->getType());
        res.enabled = !fx->getBypassed();
        for (const auto& p : fx->getParameterInfos())
        {
            float val = fx->getParameter(p.id);
            res.parameters.push_back(val);
            if (p.id == "mix")
                res.wetDry = val;
        }
        state.effects.push_back(res);
    }
    return state;
}

void PerformanceComponent::setState(const PerformanceRackState& state)
{
    auto& rack = audioEngine.getPerformanceRack();
    rack.setMasterBypassed(state.masterBypass);
    rack.setMasterGain(state.masterGain);
    rack.clearEffects();

    for (const auto& es : state.effects)
    {
        auto fx = rack.addEffect(static_cast<PerformanceEffectType>(es.effectType));
        if (fx)
        {
            fx->setBypassed(!es.enabled);
            auto pInfos = fx->getParameterInfos();
            for (size_t i = 0; i < pInfos.size() && i < es.parameters.size(); ++i)
            {
                fx->setParameter(pInfos[i].id, es.parameters[i]);
            }
        }
    }

    refreshRack();
}

// ─────────────────────────────────────────────────────────────────────────────
//  RackBayComponent Implementation
// ─────────────────────────────────────────────────────────────────────────────
void PerformanceComponent::RackBayComponent::paint(juce::Graphics& g)
{
    auto area = getLocalBounds();
    g.fillAll(juce::Colour(14, 16, 18));

    // Draw full-height 19" rack rails on left and right
    int railWidth = 38;
    auto leftRail = area.removeFromLeft(railWidth);
    auto rightRail = area.removeFromRight(railWidth);

    g.setColour(juce::Colour(22, 25, 29));
    g.fillRect(leftRail);
    g.fillRect(rightRail);

    // Inner rail borders
    g.setColour(juce::Colour(42, 46, 54));
    g.drawVerticalLine(leftRail.getRight(), 0.0f, static_cast<float>(getHeight()));
    g.drawVerticalLine(rightRail.getX(), 0.0f, static_cast<float>(getHeight()));

    // Draw regular rack screw hole guides every 44px (1U height)
    int uHeight = 44;
    g.setColour(juce::Colour(34, 38, 44));
    for (int y = 22; y < getHeight(); y += uHeight)
    {
        g.fillEllipse(leftRail.getCentreX() - 3.5f, static_cast<float>(y) - 3.5f, 7.0f, 7.0f);
        g.fillEllipse(rightRail.getCentreX() - 3.5f, static_cast<float>(y) - 3.5f, 7.0f, 7.0f);
    }

    // If rack is empty, draw empty state placeholder
    if (ownerComp.rackUnits.isEmpty())
    {
        ownerComp.drawEmptyRackPlaceholder(g, getLocalBounds().reduced(railWidth + 20, 20));
    }
}

void PerformanceComponent::RackBayComponent::resized()
{
    int total = ownerComp.rackUnits.size();
    int unitH = 132;
    int gap = 6;
    int topPad = 12;

    int requiredHeight = topPad + total * (unitH + gap) + 24;
    int viewportHeight = ownerComp.rackViewport.getHeight();
    int targetHeight = std::max(requiredHeight, viewportHeight);

    int targetWidth = ownerComp.rackViewport.getWidth();
    if (targetWidth > 0 && (getWidth() != targetWidth || getHeight() != targetHeight))
    {
        setSize(targetWidth, targetHeight);
        return;
    }

    int y = topPad;
    for (auto* u : ownerComp.rackUnits)
    {
        u->setBounds(12, y, getWidth() - 24, unitH);
        y += unitH + gap;
    }
}

void PerformanceComponent::drawEmptyRackPlaceholder(juce::Graphics& g, const juce::Rectangle<int>& area)
{
    auto center = area.toFloat();
    float boxW = juce::jmin(520.0f, center.getWidth());
    float boxH = 200.0f;
    auto box = juce::Rectangle<float>(center.getCentreX() - boxW * 0.5f,
                                      center.getCentreY() - boxH * 0.5f,
                                      boxW, boxH);

    // Dashed outline
    g.setColour(juce::Colour(45, 50, 60));
    float dashPattern[] = { 6.0f, 4.0f };
    g.drawDashedLine(juce::Line<float>(box.getTopLeft(), box.getTopRight()), dashPattern, 2, 1.5f);
    g.drawDashedLine(juce::Line<float>(box.getTopRight(), box.getBottomRight()), dashPattern, 2, 1.5f);
    g.drawDashedLine(juce::Line<float>(box.getBottomRight(), box.getBottomLeft()), dashPattern, 2, 1.5f);
    g.drawDashedLine(juce::Line<float>(box.getBottomLeft(), box.getTopLeft()), dashPattern, 2, 1.5f);

    // Text instructions
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(16.0f).boldened());
    g.drawText("Effects Rack is Empty", box.removeFromTop(70), juce::Justification::centred);

    g.setColour(OpenWavLookAndFeel::textSecondary);
    g.setFont(juce::Font(13.0f));
    g.drawText("Click '+ Add Effect' above or select a Rack Preset to insert studio DSP modules.",
               box.removeFromTop(40), juce::Justification::centred);

    g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.8f));
    g.setFont(juce::Font(12.0f).italicised());
    g.drawText("Supports up to 16 real-time modules with zero latency.",
               box.removeFromTop(30), juce::Justification::centred);
}

} // namespace openwav
