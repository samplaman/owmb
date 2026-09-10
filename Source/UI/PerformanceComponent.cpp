#include "PerformanceComponent.h"
#include <cmath>
#include <algorithm>

#if __has_include(<BinaryData.h>)
 #include <BinaryData.h>
#endif

namespace openwav
{

// ─────────────────────────────────────────────────────────────────────────────
//  Scratched Metal Texture Resource Loader
// ─────────────────────────────────────────────────────────────────────────────
static juce::Image getScratchedMetalAlbedoTexture()
{
    static juce::Image cachedTexture;
    static bool attemptedLoad = false;

    if (attemptedLoad)
        return cachedTexture;

    attemptedLoad = true;

#if defined(JUCE_BINARYDATA_H_INCLUDED) || __has_include(<BinaryData.h>)
    int dataSize = 0;
    const char* data = BinaryData::getNamedResource("scratched_metal_albedo_png", dataSize);
    if (data == nullptr)
        data = BinaryData::getNamedResource("scratched-metal_albedo.png", dataSize);
    if (data == nullptr)
        data = BinaryData::getNamedResource("scratchedmetal_albedo_png", dataSize);

    if (data != nullptr && dataSize > 0)
    {
        cachedTexture = juce::ImageFileFormat::loadFrom(data, static_cast<size_t>(dataSize));
        if (cachedTexture.isValid())
            return cachedTexture;
    }
#endif

    // Fallback file paths for standalone run, test harnesses, or development
    const juce::String filename = "scratched-metal_albedo.png";
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
            cachedTexture = juce::ImageFileFormat::loadFrom(f);
            if (cachedTexture.isValid())
                return cachedTexture;
        }
    }

    return cachedTexture;
}

static juce::Image getScaledScratchedMetalTile(int targetHeight)
{
    static juce::Image cachedScaledTile;
    static int cachedHeight = 0;

    if (cachedHeight == targetHeight && cachedScaledTile.isValid())
        return cachedScaledTile;

    auto fullImg = getScratchedMetalAlbedoTexture();
    if (!fullImg.isValid())
        return {};

    if (targetHeight <= 0)
        targetHeight = 132;

    cachedScaledTile = fullImg.rescaled(targetHeight, targetHeight, juce::Graphics::ResamplingQuality::highResamplingQuality);
    cachedHeight = targetHeight;
    return cachedScaledTile;
}

static juce::Image getAgedMetalMaskTexture()
{
    static juce::Image cachedMask;
    static bool attemptedLoad = false;

    if (attemptedLoad)
        return cachedMask;

    attemptedLoad = true;

#if defined(JUCE_BINARYDATA_H_INCLUDED) || __has_include(<BinaryData.h>)
    int dataSize = 0;
    const char* data = BinaryData::getNamedResource("export_mask_png", dataSize);
    if (data == nullptr)
        data = BinaryData::getNamedResource("export mask.png", dataSize);
    if (data == nullptr)
        data = BinaryData::getNamedResource("exportmask_png", dataSize);

    if (data != nullptr && dataSize > 0)
    {
        cachedMask = juce::ImageFileFormat::loadFrom(data, static_cast<size_t>(dataSize));
        if (cachedMask.isValid())
            return cachedMask;
    }
#endif

    const juce::String filenames[] = { "export mask.png", "export_mask.png" };
    for (const auto& fn : filenames)
    {
        const juce::File searchPaths[] = {
            juce::File::getCurrentWorkingDirectory().getChildFile(fn),
            juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile(fn),
            juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getParentDirectory().getChildFile("Resources").getChildFile(fn),
            juce::File("/Users/eoinodowd/Desktop/owmb latest").getChildFile(fn)
        };

        for (const auto& f : searchPaths)
        {
            if (f.existsAsFile())
            {
                cachedMask = juce::ImageFileFormat::loadFrom(f);
                if (cachedMask.isValid())
                    return cachedMask;
            }
        }
    }

    // Graceful fallback to scratched metal albedo texture if export mask is not present
    cachedMask = getScratchedMetalAlbedoTexture();
    return cachedMask;
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

    // 1. Outer Chassis Gradient
    juce::ColourGradient chassisGrad(juce::Colour(34, 38, 44), 0.0f, bounds.getY(),
                                     juce::Colour(22, 24, 28), 0.0f, bounds.getBottom(), false);
    g.setGradientFill(chassisGrad);
    g.fillRoundedRectangle(bounds, 5.0f);

    // 2. 19" Rack Mounting Ears (Left & Right 38px)
    float earWidth = 38.0f;
    juce::Rectangle<float> leftEar(bounds.getX(), bounds.getY(), earWidth, bounds.getHeight());
    juce::Rectangle<float> rightEar(bounds.getRight() - earWidth, bounds.getY(), earWidth, bounds.getHeight());

    g.setColour(juce::Colour(18, 20, 24));
    g.fillRect(leftEar);
    g.fillRect(rightEar);

    // 3. Aged Brushed Metal Texture & Micro-Grain & Grime Patina
    drawAgedMetalTexture(g, bounds);

    // 4. Seams separating rack ears from module faceplate
    g.setColour(juce::Colour(45, 49, 57));
    g.drawVerticalLine(static_cast<int>(leftEar.getRight()), bounds.getY(), bounds.getBottom());
    g.drawVerticalLine(static_cast<int>(rightEar.getX()), bounds.getY(), bounds.getBottom());

    // 5. Rack Mount Screw Rivets
    auto drawScrew = [&g](float cx, float cy) {
        float r = 5.5f;
        g.setColour(juce::Colour(12, 14, 16));
        g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);
        g.setColour(juce::Colour(80, 86, 96));
        g.drawEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f, 1.0f);
        // Screw slot
        g.setColour(juce::Colour(30, 34, 40));
        g.drawLine(cx - 3.0f, cy - 3.0f, cx + 3.0f, cy + 3.0f, 1.2f);
        g.drawLine(cx - 3.0f, cy + 3.0f, cx + 3.0f, cy - 3.0f, 1.2f);
    };

    drawScrew(leftEar.getCentreX(), bounds.getY() + 18.0f);
    drawScrew(leftEar.getCentreX(), bounds.getBottom() - 18.0f);
    drawScrew(rightEar.getCentreX(), bounds.getY() + 18.0f);
    drawScrew(rightEar.getCentreX(), bounds.getBottom() - 18.0f);

    // 6. Top Accent Color Strip
    if (effect)
    {
        auto accent = effect->getAccentColour();
        if (isBypassed) accent = accent.withAlpha(0.35f);
        g.setColour(accent);
        g.fillRoundedRectangle(bounds.getX() + earWidth, bounds.getY(), bounds.getWidth() - earWidth * 2.0f, 3.0f, 1.5f);
    }

    // 7. Worn Paint, Screw Washer Grinds, Edge Chipping & Scratches
    drawWornPaintAndScuffs(g, bounds, earWidth);

    // 8. Authentic Vintage Studio Stickers (Dymo Tape, Masking Tape, QC Stamp)
    drawStudioStickers(g, bounds, earWidth);

    // 9. Outer Bevel / Border
    g.setColour(juce::Colour(52, 58, 68));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 5.0f, 1.0f);

    // 10. If bypassed, draw subtle darkening overlay
    if (isBypassed)
    {
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.fillRoundedRectangle(bounds.reduced(earWidth, 0.0f), 0.0f);
    }
}

void RackUnitComponent::drawAgedMetalTexture(juce::Graphics& g, const juce::Rectangle<float>& bounds)
{
    float earWidth = 38.0f;
    auto faceplate = bounds.reduced(earWidth, 0.0f);

    // 1. Exported Aged Metal Scratch Mask (Pre-resized 2588x230 asset ready to go)
    auto maskImg = getAgedMetalMaskTexture();
    if (maskImg.isValid())
    {
        int mw = maskImg.getWidth();
        int mh = maskImg.getHeight();

        // Check if it's the wide pre-resized rack mask (e.g. 2588x230)
        if (mw > mh * 2)
        {
            g.saveState();
            juce::Path chassisClip;
            chassisClip.addRoundedRectangle(bounds, 5.0f);
            g.reduceClipRegion(chassisClip);
            g.setOpacity(1.0f);

            // Aspect-ratio-preserving horizontal mapping across the 19" chassis:
            float scale = static_cast<float>(mh) / std::max(1.0f, bounds.getHeight());
            int srcW = std::min(mw, juce::roundToInt(bounds.getWidth() * scale));
            int srcH = mh;
            int maxShift = std::max(0, mw - srcW);
            // Deterministic offset per rack unit slot so each unit has a distinct scratch section
            int srcX = maxShift > 0 ? (index * 211) % maxShift : 0;

            g.drawImage(maskImg,
                        juce::roundToInt(bounds.getX()), juce::roundToInt(bounds.getY()),
                        juce::roundToInt(bounds.getWidth()), juce::roundToInt(bounds.getHeight()),
                        srcX, 0, srcW, srcH);
            g.restoreState();
        }
        else
        {
            // Square tilable texture fallback: scale and tile on X
            int targetH = juce::roundToInt(faceplate.getHeight());
            if (targetH <= 0) targetH = 132;

            auto tileImg = getScaledScratchedMetalTile(targetH);
            if (tileImg.isValid())
            {
                float tileW = static_cast<float>(tileImg.getWidth());
                float tileH = static_cast<float>(tileImg.getHeight());

                g.saveState();
                g.reduceClipRegion(faceplate.toNearestInt());
                g.setOpacity(0.65f);

                float xOffset = fmodf(static_cast<float>(index * 79), tileW);
                float startX = faceplate.getX() - xOffset;

                for (float y = faceplate.getY(); y < faceplate.getBottom(); y += tileH)
                {
                    for (float x = startX; x < faceplate.getRight(); x += tileW)
                    {
                        g.drawImage(tileImg,
                                    juce::roundToInt(x), juce::roundToInt(y),
                                    juce::roundToInt(tileW), juce::roundToInt(tileH),
                                    0, 0, tileImg.getWidth(), tileImg.getHeight());
                    }
                }
                g.restoreState();

                auto drawEarTiles = [&](const juce::Rectangle<float>& earRect, int earSeed) {
                    g.saveState();
                    juce::Path earClip;
                    earClip.addRoundedRectangle(bounds, 5.0f);
                    g.reduceClipRegion(earClip);
                    g.reduceClipRegion(earRect.toNearestInt());
                    g.setOpacity(0.48f);

                    float earOffset = fmodf(static_cast<float>(index * 53 + earSeed * 37), tileW);
                    float earStartX = earRect.getX() - earOffset;
                    for (float y = earRect.getY(); y < earRect.getBottom(); y += tileH)
                    {
                        for (float x = earStartX; x < earRect.getRight(); x += tileW)
                        {
                            g.drawImage(tileImg,
                                        juce::roundToInt(x), juce::roundToInt(y),
                                        juce::roundToInt(tileW), juce::roundToInt(tileH),
                                        0, 0, tileImg.getWidth(), tileImg.getHeight());
                        }
                    }
                    g.restoreState();
                };

                drawEarTiles(juce::Rectangle<float>(bounds.getX(), bounds.getY(), earWidth, bounds.getHeight()), 1);
                drawEarTiles(juce::Rectangle<float>(bounds.getRight() - earWidth, bounds.getY(), earWidth, bounds.getHeight()), 2);
            }
        }
    }

    // 2. Brushed Aluminum Horizontal Grain Lines (deterministic micro-grain)
    int numLines = static_cast<int>(faceplate.getHeight());
    for (int y = 0; y < numLines; y += 2)
    {
        uint32_t hash = static_cast<uint32_t>((y * 1103515245 + index * 12345 + 101) & 0x7FFFFFFF);
        float alpha = (hash % 100) / 100.0f;

        if (alpha > 0.62f)
        {
            float brightAlpha = (alpha - 0.62f) * 0.08f;
            g.setColour(juce::Colours::white.withAlpha(brightAlpha));
            g.drawHorizontalLine(static_cast<int>(faceplate.getY() + y), faceplate.getX(), faceplate.getRight());
        }
        else if (alpha < 0.38f)
        {
            float darkAlpha = (0.38f - alpha) * 0.09f;
            g.setColour(juce::Colours::black.withAlpha(darkAlpha));
            g.drawHorizontalLine(static_cast<int>(faceplate.getY() + y), faceplate.getX(), faceplate.getRight());
        }
    }

    // 3. Patina & Grime Vignette along Edges and Seams
    juce::ColourGradient topEdgeGrad(juce::Colour(8, 10, 12).withAlpha(0.65f), 0.0f, faceplate.getY(),
                                     juce::Colour(8, 10, 12).withAlpha(0.0f), 0.0f, faceplate.getY() + 14.0f, false);
    g.setGradientFill(topEdgeGrad);
    g.fillRect(faceplate.withHeight(14.0f));

    juce::ColourGradient botEdgeGrad(juce::Colour(6, 8, 10).withAlpha(0.70f), 0.0f, faceplate.getBottom(),
                                     juce::Colour(6, 8, 10).withAlpha(0.0f), 0.0f, faceplate.getBottom() - 16.0f, false);
    g.setGradientFill(botEdgeGrad);
    g.fillRect(faceplate.withY(faceplate.getBottom() - 16.0f).withHeight(16.0f));

    // Corner grime accumulation
    auto drawCornerGrime = [&](float cx, float cy, float radius) {
        juce::ColourGradient cg(juce::Colour(10, 12, 14).withAlpha(0.55f), cx, cy,
                                juce::Colour(10, 12, 14).withAlpha(0.0f), cx + radius, cy + radius, true);
        g.setGradientFill(cg);
        g.fillEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
    };

    drawCornerGrime(faceplate.getX(), faceplate.getY(), 32.0f);
    drawCornerGrime(faceplate.getRight(), faceplate.getY(), 32.0f);
    drawCornerGrime(faceplate.getX(), faceplate.getBottom(), 36.0f);
    drawCornerGrime(faceplate.getRight(), faceplate.getBottom(), 36.0f);
}

void RackUnitComponent::drawWornPaintAndScuffs(juce::Graphics& g, const juce::Rectangle<float>& bounds, float earWidth)
{
    juce::Rectangle<float> leftEar(bounds.getX(), bounds.getY(), earWidth, bounds.getHeight());
    juce::Rectangle<float> rightEar(bounds.getRight() - earWidth, bounds.getY(), earWidth, bounds.getHeight());

    // 1. Screw Washer Circular Scrape Marks (Grinded raw metal where screw washers rotated)
    auto drawWasherScuff = [&](float cx, float cy, int seed) {
        float r = 8.5f;
        // Inner bare aluminum washer ring
        g.setColour(juce::Colour(170, 175, 185).withAlpha(0.50f));
        g.drawEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f, 1.3f);

        // Circular grind arc marks
        juce::Path arc1;
        float startAngle = (seed % 10) * 0.6f;
        arc1.addCentredArc(cx, cy, r - 1.0f, r - 1.0f, 0.0f, startAngle, startAngle + 2.2f, true);
        g.setColour(juce::Colour(215, 220, 230).withAlpha(0.65f));
        g.strokePath(arc1, juce::PathStrokeType(1.1f));

        // Dark oxidation edge outside the washer ring
        g.setColour(juce::Colour(12, 14, 16).withAlpha(0.70f));
        g.drawEllipse(cx - (r + 1.2f), cy - (r + 1.2f), (r + 1.2f) * 2.0f, (r + 1.2f) * 2.0f, 0.8f);

        // A tiny chipped paint fleck near the screw hole
        float chipAngle = (seed % 7) * 0.9f;
        float chipX = cx + (r + 1.5f) * std::cos(chipAngle);
        float chipY = cy + (r + 1.5f) * std::sin(chipAngle);
        g.setColour(juce::Colour(155, 160, 170).withAlpha(0.75f));
        g.fillEllipse(chipX - 1.2f, chipY - 1.0f, 2.4f, 2.0f);
        g.setColour(juce::Colour(220, 225, 235).withAlpha(0.85f));
        g.fillEllipse(chipX - 0.6f, chipY - 0.5f, 1.2f, 1.0f);
    };

    drawWasherScuff(leftEar.getCentreX(), bounds.getY() + 18.0f, index * 7 + 1);
    drawWasherScuff(leftEar.getCentreX(), bounds.getBottom() - 18.0f, index * 7 + 2);
    drawWasherScuff(rightEar.getCentreX(), bounds.getY() + 18.0f, index * 7 + 3);
    drawWasherScuff(rightEar.getCentreX(), bounds.getBottom() - 18.0f, index * 7 + 4);

    // 2. Chipped Paint along Ear Seams & Outer Edges
    auto drawPaintChip = [&](float x, float y, float w, float h) {
        // Dark chipped crater
        g.setColour(juce::Colour(8, 10, 12).withAlpha(0.85f));
        g.fillRoundedRectangle(x - 0.5f, y - 0.5f, w + 1.0f, h + 1.0f, 1.0f);
        // Raw metallic aluminum exposed
        g.setColour(juce::Colour(150, 155, 165));
        g.fillRoundedRectangle(x, y, w, h, 0.8f);
        // Specular highlight on upper/left edge of chip
        g.setColour(juce::Colour(225, 230, 240).withAlpha(0.75f));
        g.drawLine(x, y, x + w * 0.7f, y, 0.9f);
        g.drawLine(x, y, x, y + h * 0.7f, 0.9f);
    };

    // Vertical seam chipping (left seam)
    float seamLeftX = leftEar.getRight();
    drawPaintChip(seamLeftX - 1.2f, bounds.getY() + 24.0f + (index * 13 % 35), 2.8f, 5.0f);
    drawPaintChip(seamLeftX - 0.8f, bounds.getBottom() - 40.0f - (index * 9 % 25), 2.2f, 4.0f);

    // Vertical seam chipping (right seam)
    float seamRightX = rightEar.getX();
    drawPaintChip(seamRightX - 1.5f, bounds.getY() + 38.0f + (index * 11 % 30), 2.7f, 4.5f);
    drawPaintChip(seamRightX - 1.0f, bounds.getBottom() - 32.0f - (index * 17 % 28), 2.4f, 5.5f);

    // Outer perimeter edge wear
    drawPaintChip(bounds.getX() + 12.0f + (index * 19 % 20), bounds.getY() + 0.5f, 6.5f, 1.8f);
    drawPaintChip(bounds.getRight() - 35.0f - (index * 23 % 20), bounds.getBottom() - 2.2f, 7.0f, 1.8f);

    // 3. Hairline Faceplate Scratches
    auto drawScratch = [&](float x1, float y1, float x2, float y2) {
        // Shadow line
        g.setColour(juce::Colour(10, 12, 14).withAlpha(0.55f));
        g.drawLine(x1, y1 + 0.8f, x2, y2 + 0.8f, 0.9f);
        // Bright metallic scratch
        g.setColour(juce::Colour(180, 185, 195).withAlpha(0.40f));
        g.drawLine(x1, y1, x2, y2, 0.8f);
    };

    // Deterministic scratches across the faceplate
    float faceW = bounds.getWidth() - earWidth * 2.0f;
    float scX1 = bounds.getX() + earWidth + 20.0f + ((index * 79) % static_cast<int>(faceW * 0.4f));
    float scY1 = bounds.getY() + 35.0f + ((index * 37) % 30);
    drawScratch(scX1, scY1, scX1 + 28.0f, scY1 + 9.0f);

    float scX2 = bounds.getX() + earWidth + faceW * 0.55f + ((index * 53) % static_cast<int>(faceW * 0.35f));
    float scY2 = bounds.getY() + 75.0f + ((index * 41) % 35);
    drawScratch(scX2, scY2, scX2 + 35.0f, scY2 - 7.0f);

    // 4. Weathered Accent Strip Distress
    if (effect)
    {
        float accentY = bounds.getY();
        float chipAx1 = bounds.getX() + earWidth + 60.0f + ((index * 47) % 120);
        float chipAx2 = bounds.getRight() - earWidth - 80.0f - ((index * 31) % 100);

        g.setColour(juce::Colour(24, 26, 30));
        g.fillRect(chipAx1, accentY, 3.5f, 3.0f);
        g.fillRect(chipAx2, accentY, 4.0f, 3.0f);

        // Bare aluminum fleck in the chip
        g.setColour(juce::Colour(160, 165, 175));
        g.fillRect(chipAx1 + 0.8f, accentY + 0.5f, 1.8f, 2.0f);
        g.fillRect(chipAx2 + 1.0f, accentY + 0.5f, 2.0f, 2.0f);
    }
}

void RackUnitComponent::drawStudioStickers(juce::Graphics& g, const juce::Rectangle<float>& bounds, float earWidth)
{
    int stickerSeed = (index + 1) * 17;
    int style = index % 3; // 0: Dymo Embossed Tape, 1: Masking Tape, 2: QC Inspection Stamp

    bool placeOnLeft = (index % 2 == 0);
    float stickerW = 80.0f;
    float stickerH = 16.0f;
    float sx = placeOnLeft ? (bounds.getX() + earWidth + 10.0f)
                           : (bounds.getRight() - earWidth - stickerW - 10.0f);
    float sy = bounds.getBottom() - stickerH - 8.0f;

    if (style == 0) // Dymo Embossed Tape
    {
        stickerW = 76.0f;
        stickerH = 15.0f;
        sx = placeOnLeft ? (bounds.getX() + earWidth + 12.0f) : (bounds.getRight() - earWidth - stickerW - 12.0f);
        sy = bounds.getBottom() - stickerH - 7.0f;

        float rot = (placeOnLeft ? -0.022f : 0.018f) * ((stickerSeed % 5) - 2);

        juce::Graphics::ScopedSaveState ss(g);
        g.addTransform(juce::AffineTransform::rotation(rot, sx + stickerW * 0.5f, sy + stickerH * 0.5f));

        // Drop shadow
        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.fillRoundedRectangle(sx + 1.2f, sy + 1.8f, stickerW, stickerH, 1.5f);

        // Dymo plastic tape color (alternates between black, vintage red, retro blue)
        juce::Colour tapeColor = (index % 4 == 0) ? juce::Colour(24, 26, 28)
                               : ((index % 4 == 1) ? juce::Colour(160, 32, 32)
                                                   : juce::Colour(28, 64, 130));

        // 45-degree clipped ends
        juce::Path tapePath;
        float cut = 2.5f;
        tapePath.startNewSubPath(sx + cut, sy);
        tapePath.lineTo(sx + stickerW - cut, sy);
        tapePath.lineTo(sx + stickerW, sy + cut);
        tapePath.lineTo(sx + stickerW, sy + stickerH - cut);
        tapePath.lineTo(sx + stickerW - cut, sy + stickerH);
        tapePath.lineTo(sx + cut, sy + stickerH);
        tapePath.lineTo(sx, sy + stickerH - cut);
        tapePath.lineTo(sx, sy + cut);
        tapePath.closeSubPath();

        g.setColour(tapeColor);
        g.fillPath(tapePath);

        // Gloss highlight reflection on top half
        g.setColour(juce::Colours::white.withAlpha(0.18f));
        g.fillRect(sx + cut, sy + 1.0f, stickerW - cut * 2.0f, stickerH * 0.45f);

        // Tape subtle border rim
        g.setColour(tapeColor.brighter(0.25f));
        g.strokePath(tapePath, juce::PathStrokeType(0.8f));

        // Embossed White Letters (Raised 3D appearance)
        juce::String dymoText;
        switch (index % 6)
        {
            case 0: dymoText = "HOT TUBE"; break;
            case 1: dymoText = "CALIBRATED"; break;
            case 2: dymoText = "DO NOT TOUCH"; break;
            case 3: dymoText = "REC LEVEL"; break;
            case 4: dymoText = "VINTAGE SPEC"; break;
            default: dymoText = "UNIT #0" + juce::String(index + 1); break;
        }

        g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 9.5f, juce::Font::bold));
        // Letter bevel shadow
        g.setColour(juce::Colour(10, 12, 14).withAlpha(0.6f));
        g.drawText(dymoText, juce::Rectangle<float>(sx + 1.0f, sy + 1.0f, stickerW, stickerH), juce::Justification::centred, false);
        // Letter white embossed face
        g.setColour(juce::Colour(248, 250, 252));
        g.drawText(dymoText, juce::Rectangle<float>(sx, sy, stickerW, stickerH), juce::Justification::centred, false);
    }
    else if (style == 1) // Torn Studio Masking Tape
    {
        stickerW = 86.0f;
        stickerH = 17.0f;
        sx = placeOnLeft ? (bounds.getX() + earWidth + 14.0f) : (bounds.getRight() - earWidth - stickerW - 14.0f);
        sy = bounds.getBottom() - stickerH - 7.0f;

        float rot = (placeOnLeft ? 0.020f : -0.024f) * ((stickerSeed % 5) - 2);

        juce::Graphics::ScopedSaveState ss(g);
        g.addTransform(juce::AffineTransform::rotation(rot, sx + stickerW * 0.5f, sy + stickerH * 0.5f));

        // Ragged torn ends path
        juce::Path tapePath;
        tapePath.startNewSubPath(sx + 3.0f, sy);
        tapePath.lineTo(sx + stickerW - 3.0f, sy);
        // Torn right end
        tapePath.lineTo(sx + stickerW, sy + 4.0f);
        tapePath.lineTo(sx + stickerW - 2.0f, sy + 8.0f);
        tapePath.lineTo(sx + stickerW + 1.0f, sy + 13.0f);
        tapePath.lineTo(sx + stickerW - 3.0f, sy + stickerH);
        // Bottom edge
        tapePath.lineTo(sx + 3.0f, sy + stickerH);
        // Torn left end
        tapePath.lineTo(sx, sy + 12.0f);
        tapePath.lineTo(sx + 2.0f, sy + 7.0f);
        tapePath.lineTo(sx - 1.0f, sy + 3.0f);
        tapePath.closeSubPath();

        // Drop shadow
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.fillPath(tapePath, juce::AffineTransform::translation(1.0f, 1.5f));

        // Aged semi-translucent cream masking tape
        g.setColour(juce::Colour(236, 227, 203).withAlpha(0.92f));
        g.fillPath(tapePath);

        // Subtle fiber texture
        g.setColour(juce::Colour(215, 203, 175).withAlpha(0.40f));
        for (float fx = sx + 8.0f; fx < sx + stickerW - 8.0f; fx += 5.0f)
        {
            g.drawVerticalLine(static_cast<int>(fx), sy + 2.0f, sy + stickerH - 2.0f);
        }

        // Handwritten marker / pen text
        juce::String markerText;
        switch (index % 6)
        {
            case 0: markerText = "CH 1-2 INSERT"; break;
            case 1: markerText = "FAT TONE!"; break;
            case 2: markerText = "SET & FORGET"; break;
            case 3: markerText = "KEEP UNDER +3"; break;
            case 4: markerText = "DRUM BUS"; break;
            default: markerText = "OWMB 1984"; break;
        }

        g.setFont(juce::Font(10.0f).italicised().boldened());
        g.setColour(juce::Colour(25, 32, 50).withAlpha(0.85f));
        g.drawText(markerText, juce::Rectangle<float>(sx, sy, stickerW, stickerH), juce::Justification::centred, false);
    }
    else // Vintage QC / Inspection Stamp Sticker
    {
        stickerW = 62.0f;
        stickerH = 22.0f;
        sx = placeOnLeft ? (bounds.getX() + earWidth + 14.0f) : (bounds.getRight() - earWidth - stickerW - 14.0f);
        sy = bounds.getBottom() - stickerH - 6.0f;

        float rot = (placeOnLeft ? -0.030f : 0.026f);

        juce::Graphics::ScopedSaveState ss(g);
        g.addTransform(juce::AffineTransform::rotation(rot, sx + stickerW * 0.5f, sy + stickerH * 0.5f));

        // Drop shadow
        g.setColour(juce::Colours::black.withAlpha(0.40f));
        g.fillRoundedRectangle(sx + 1.2f, sy + 1.5f, stickerW, stickerH, 3.0f);

        // Yellowed aged paper background
        g.setColour(juce::Colour(244, 237, 210));
        g.fillRoundedRectangle(sx, sy, stickerW, stickerH, 3.0f);

        // Faded red stamp border
        g.setColour(juce::Colour(175, 45, 45).withAlpha(0.70f));
        g.drawRoundedRectangle(sx + 2.0f, sy + 2.0f, stickerW - 4.0f, stickerH - 4.0f, 2.0f, 1.0f);

        // Stamped QC text
        g.setFont(juce::Font(juce::Font::getDefaultSansSerifFontName(), 8.5f, juce::Font::bold));
        g.setColour(juce::Colour(175, 45, 45).withAlpha(0.85f));
        g.drawText("PASSED QC #" + juce::String(12 + (index * 7) % 80),
                   juce::Rectangle<float>(sx, sy + 2.0f, stickerW, 10.0f), juce::Justification::centred, false);

        g.setFont(juce::Font(juce::Font::getDefaultSansSerifFontName(), 7.5f, juce::Font::plain));
        g.setColour(juce::Colour(55, 60, 70).withAlpha(0.80f));
        g.drawText("CALIBRATED '84",
                   juce::Rectangle<float>(sx, sy + 11.0f, stickerW, 9.0f), juce::Justification::centred, false);

        // Dog-eared folded top-right corner
        juce::Path fold;
        float foldSize = 5.0f;
        fold.startNewSubPath(sx + stickerW - foldSize, sy);
        fold.lineTo(sx + stickerW, sy + foldSize);
        fold.lineTo(sx + stickerW - foldSize, sy + foldSize);
        fold.closeSubPath();
        g.setColour(juce::Colour(218, 210, 185));
        g.fillPath(fold);
        g.setColour(juce::Colour(150, 140, 120));
        g.strokePath(fold, juce::PathStrokeType(0.6f));
    }
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
