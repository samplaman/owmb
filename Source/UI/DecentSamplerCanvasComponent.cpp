#include "DecentSamplerCanvasComponent.h"
#include "OpenWavLookAndFeel.h"

namespace openwav
{

// ── Color Parser (Handles AARRGGBB and RRGGBB) ──────────────
juce::Colour DecentSamplerControlComponent::parseDecentSamplerColor(const juce::String& hexStr, juce::Colour defaultColor)
{
    if (hexStr.isEmpty())
        return defaultColor;

    juce::String clean = hexStr.trim().trimCharactersAtStart("#");
    if (clean.length() == 6)
    {
        int r = clean.substring(0, 2).getHexValue32();
        int g = clean.substring(2, 4).getHexValue32();
        int b = clean.substring(4, 6).getHexValue32();
        return juce::Colour(static_cast<juce::uint8>(r), static_cast<juce::uint8>(g), static_cast<juce::uint8>(b));
    }
    else if (clean.length() == 8)
    {
        // Decent Sampler format is AARRGGBB
        int a = clean.substring(0, 2).getHexValue32();
        int r = clean.substring(2, 4).getHexValue32();
        int g = clean.substring(4, 6).getHexValue32();
        int b = clean.substring(6, 8).getHexValue32();
        return juce::Colour(static_cast<juce::uint8>(r), static_cast<juce::uint8>(g), static_cast<juce::uint8>(b), static_cast<juce::uint8>(a));
    }

    return defaultColor;
}

// ── DecentSamplerControlComponent Implementation ────────────
DecentSamplerControlComponent::DecentSamplerControlComponent(const DecentSamplerUiControl& m)
    : model(m)
{
    setRepaintsOnMouseActivity(true);
    updateColors();
    loadFilmstripImage();
}

void DecentSamplerControlComponent::setModel(const DecentSamplerUiControl& m)
{
    model = m;
    updateColors();
    loadFilmstripImage();
    repaint();
}

void DecentSamplerControlComponent::setScale(float s)
{
    scale = std::max(0.2f, s);
    repaint();
}

static std::map<juce::String, std::shared_ptr<DecentSamplerControlComponent::CachedFilmstrip>> sGlobalFilmstripCache;

std::shared_ptr<DecentSamplerControlComponent::CachedFilmstrip> DecentSamplerControlComponent::getOrCreateFilmstrip(const juce::String& filePath, int numFramesHint)
{
    if (filePath.isEmpty()) return nullptr;

    auto it = sGlobalFilmstripCache.find(filePath);
    if (it != sGlobalFilmstripCache.end() && it->second != nullptr)
        return it->second;

    juce::File file(filePath);
    if (!file.existsAsFile())
        return nullptr;

    auto masterImage = juce::ImageFileFormat::loadFrom(file);
    if (!masterImage.isValid())
        return nullptr;

    auto entry = std::make_shared<CachedFilmstrip>();
    int masterW = masterImage.getWidth();
    int masterH = masterImage.getHeight();

    int numFrames = numFramesHint > 0 ? numFramesHint : 0;
    if (numFrames <= 0)
    {
        if (masterH > masterW)
            numFrames = masterH / std::max(1, masterW);
        else
            numFrames = masterW / std::max(1, masterH);
    }
    entry->numFrames = std::max(1, numFrames);
    entry->isVertical = (masterH >= masterW);

    if (entry->isVertical)
    {
        entry->frameW = masterW;
        entry->frameH = masterH / entry->numFrames;
        entry->frames.reserve(entry->numFrames);

        for (int i = 0; i < entry->numFrames; ++i)
        {
            int sy = i * entry->frameH;
            auto slice = masterImage.getClippedImage(juce::Rectangle<int>(0, sy, entry->frameW, entry->frameH));
            juce::Image compactFrame(juce::Image::ARGB, entry->frameW, entry->frameH, true);
            {
                juce::Graphics g(compactFrame);
                g.drawImageAt(slice, 0, 0);
            }
            entry->frames.push_back(std::move(compactFrame));
        }
    }
    else
    {
        entry->frameW = masterW / entry->numFrames;
        entry->frameH = masterH;
        entry->frames.reserve(entry->numFrames);

        for (int i = 0; i < entry->numFrames; ++i)
        {
            int sx = i * entry->frameW;
            auto slice = masterImage.getClippedImage(juce::Rectangle<int>(sx, 0, entry->frameW, entry->frameH));
            juce::Image compactFrame(juce::Image::ARGB, entry->frameW, entry->frameH, true);
            {
                juce::Graphics g(compactFrame);
                g.drawImageAt(slice, 0, 0);
            }
            entry->frames.push_back(std::move(compactFrame));
        }
    }

    sGlobalFilmstripCache[filePath] = entry;
    return entry;
}

void DecentSamplerControlComponent::loadFilmstripImage()
{
    filmstrip = nullptr;

    juce::String path = model.resolvedCustomSkinImagePath;
    if (path.isNotEmpty())
    {
        filmstrip = getOrCreateFilmstrip(path, model.customSkinNumFrames);
        if (filmstrip != nullptr) return;
    }

    if (model.customSkinImagePath.isNotEmpty())
    {
        filmstrip = getOrCreateFilmstrip(model.customSkinImagePath, model.customSkinNumFrames);
        if (filmstrip != nullptr) return;

        // Try searching relative to resolvedCustomSkinImagePath's directory
        if (model.resolvedCustomSkinImagePath.isNotEmpty())
        {
            juce::File parent = juce::File(model.resolvedCustomSkinImagePath).getParentDirectory();
            juce::File alt = parent.getChildFile(juce::File(model.customSkinImagePath).getFileName());
            if (alt.existsAsFile())
            {
                filmstrip = getOrCreateFilmstrip(alt.getFullPathName(), model.customSkinNumFrames);
                if (filmstrip != nullptr) return;
            }
        }
    }
}

void DecentSamplerControlComponent::updateColors()
{
    fgColor = parseDecentSamplerColor(model.trackColorHex, OpenWavLookAndFeel::accentCyan);
    bgColor = parseDecentSamplerColor(model.trackBackgroundColorHex, juce::Colour(0x55555555));
    textColor = parseDecentSamplerColor(model.textColorHex, OpenWavLookAndFeel::textPrimary);
}

juce::String DecentSamplerControlComponent::getFormattedValueString() const
{
    double val = model.currentValue;
    juce::String s;
    if (std::abs(val) < 0.0001)
        s = "0.0";
    else if (std::abs(val - std::round(val)) < 0.001)
        s = juce::String(static_cast<int>(std::round(val)));
    else if (std::abs(val) < 10.0)
        s = juce::String(val, 2);
    else
        s = juce::String(val, 1);

    if (model.units.isNotEmpty())
        s += " " + model.units;

    return s;
}

void DecentSamplerControlComponent::setValue(double val, bool notify)
{
    val = juce::jlimit(model.minValue, model.maxValue, val);
    if (std::abs(model.currentValue - val) > 0.00001)
    {
        model.currentValue = val;
        repaint();
        if (notify && onValueChanged)
            onValueChanged(model.currentValue);
    }
}

void DecentSamplerControlComponent::setVisualModulationOffset(double offset)
{
    if (std::abs(visualModOffset - offset) > 0.0001)
    {
        visualModOffset = offset;
        repaint();
    }
}

void DecentSamplerControlComponent::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown())
        return;

    isDragging = true;
    dragStartVal = model.currentValue;
    dragStartY = e.getPosition().getY();
    dragStartX = e.getPosition().getX();
    repaint();
}

void DecentSamplerControlComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (!isDragging)
        return;

    bool isHorizontal = model.type.containsIgnoreCase("horizontal") || model.style.containsIgnoreCase("horizontal");
    int delta = isHorizontal ? (e.getPosition().getX() - dragStartX) : (dragStartY - e.getPosition().getY());

    double range = std::max(0.0001, model.maxValue - model.minValue);
    double sensitivity = 150.0;
    if (e.mods.isShiftDown())
        sensitivity = 600.0;

    double deltaVal = (delta / sensitivity) * range;
    setValue(dragStartVal + deltaVal, true);
}

void DecentSamplerControlComponent::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    double range = std::max(0.0001, model.maxValue - model.minValue);
    double step = (wheel.deltaY > 0 ? 1.0 : -1.0) * (range / 50.0);
    setValue(model.currentValue + step, true);
}

void DecentSamplerControlComponent::mouseDoubleClick(const juce::MouseEvent&)
{
    setValue(model.defaultValue, true);
}

void DecentSamplerControlComponent::mouseEnter(const juce::MouseEvent&)
{
    isHovered = true;
    repaint();
}

void DecentSamplerControlComponent::mouseExit(const juce::MouseEvent&)
{
    isHovered = false;
    isDragging = false;
    repaint();
}

void DecentSamplerControlComponent::resized()
{
}

void DecentSamplerControlComponent::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced(2.0f);
    if (area.getWidth() < 8 || area.getHeight() < 8)
        return;

    bool isLinear = model.type.containsIgnoreCase("linear") || model.style.containsIgnoreCase("linear") ||
                    model.type.containsIgnoreCase("vertical") || model.type.containsIgnoreCase("horizontal");
    bool isHorizontal = model.type.containsIgnoreCase("horizontal") || model.style.containsIgnoreCase("horizontal");

    bool hasLabel = model.label.isNotEmpty();
    float labelH = hasLabel ? std::min(18.0f * scale, area.getHeight() * 0.22f) : 0.0f;

    // 1. Draw Top Label
    if (hasLabel)
    {
        auto labelRect = area.removeFromTop(labelH);
        g.setColour(textColor);
        g.setFont(juce::Font(std::max(9.0f, model.textSize * scale)).boldened());
        g.drawText(model.label, labelRect, juce::Justification::centred, true);
    }

    area = area.reduced(1.0f);

    double range = std::max(0.0001, model.maxValue - model.minValue);
    double effectiveVal = juce::jlimit(model.minValue, model.maxValue, model.currentValue + visualModOffset);
    float normVal = static_cast<float>(juce::jlimit(0.0, 1.0, (effectiveVal - model.minValue) / range));

    if (isLinear)
    {
        // ── Linear Slider Rendering ────────────────────────
        if (isHorizontal)
        {
            float trackH = std::max(4.0f, 6.0f * scale);
            auto trackRect = area.withSizeKeepingCentre(area.getWidth() - 8.0f, trackH);

            // Background Track
            g.setColour(bgColor);
            g.fillRoundedRectangle(trackRect, trackH * 0.5f);

            // Active Track
            auto activeRect = trackRect.withWidth(trackRect.getWidth() * normVal);
            g.setColour(fgColor);
            g.fillRoundedRectangle(activeRect, trackH * 0.5f);

            // Thumb
            float thumbW = std::max(10.0f, 14.0f * scale);
            float thumbH = std::max(16.0f, 22.0f * scale);
            float thumbX = trackRect.getX() + normVal * trackRect.getWidth() - thumbW * 0.5f;
            auto thumbRect = juce::Rectangle<float>(thumbX, trackRect.getCentreY() - thumbH * 0.5f, thumbW, thumbH);

            juce::ColourGradient thumbGrad(juce::Colour(0xFF383C44), thumbRect.getX(), thumbRect.getY(),
                                           juce::Colour(0xFF1E2024), thumbRect.getX(), thumbRect.getBottom(), false);
            g.setGradientFill(thumbGrad);
            g.fillRoundedRectangle(thumbRect, 3.0f);
            g.setColour(fgColor);
            g.drawRoundedRectangle(thumbRect, 3.0f, 1.2f);
            g.drawLine(thumbRect.getCentreX(), thumbRect.getY() + 4, thumbRect.getCentreX(), thumbRect.getBottom() - 4, 1.5f);
        }
        else
        {
            // Vertical Slider
            float trackW = std::max(4.0f, 6.0f * scale);
            auto trackRect = area.withSizeKeepingCentre(trackW, area.getHeight() - 8.0f);

            // Background Track
            g.setColour(bgColor);
            g.fillRoundedRectangle(trackRect, trackW * 0.5f);

            // Active Track
            float fillH = trackRect.getHeight() * normVal;
            auto activeRect = juce::Rectangle<float>(trackRect.getX(), trackRect.getBottom() - fillH, trackW, fillH);
            g.setColour(fgColor);
            g.fillRoundedRectangle(activeRect, trackW * 0.5f);

            // Thumb
            float thumbW = std::max(18.0f, 24.0f * scale);
            float thumbH = std::max(10.0f, 14.0f * scale);
            float thumbY = trackRect.getBottom() - normVal * trackRect.getHeight() - thumbH * 0.5f;
            auto thumbRect = juce::Rectangle<float>(trackRect.getCentreX() - thumbW * 0.5f, thumbY, thumbW, thumbH);

            juce::ColourGradient thumbGrad(juce::Colour(0xFF383C44), thumbRect.getX(), thumbRect.getY(),
                                           juce::Colour(0xFF1E2024), thumbRect.getX(), thumbRect.getBottom(), false);
            g.setGradientFill(thumbGrad);
            g.fillRoundedRectangle(thumbRect, 3.0f);
            g.setColour(fgColor);
            g.drawRoundedRectangle(thumbRect, 3.0f, 1.2f);
            g.drawLine(thumbRect.getX() + 4, thumbRect.getCentreY(), thumbRect.getRight() - 4, thumbRect.getCentreY(), 1.5f);
        }

        // Value text overlay on hover/drag
        if (isHovered || isDragging)
        {
            auto valRect = juce::Rectangle<float>(area.getCentreX() - 35.0f * scale, area.getBottom() - 16.0f * scale, 70.0f * scale, 15.0f * scale);
            g.setColour(juce::Colour(0xDD121417));
            g.fillRoundedRectangle(valRect, 3.0f);
            g.setColour(fgColor);
            g.setFont(juce::Font(std::max(8.0f, 10.0f * scale)).boldened());
            g.drawText(getFormattedValueString(), valRect, juce::Justification::centred, true);
        }
    }
    else
    {
        // ── Rotary Knob / Custom Filmstrip Rendering ───────
        float dialD = std::min(area.getWidth(), area.getHeight());
        if (dialD < 8) return;

        auto dialBounds = area.withSizeKeepingCentre(dialD, dialD).reduced(2.0f);

        if (filmstrip != nullptr && !filmstrip->frames.empty())
        {
            // ── Pre-cached Fast Filmstrip Frame Blitting ──
            int numFrames = static_cast<int>(filmstrip->frames.size());
            int frameIdx = juce::jlimit(0, numFrames - 1, static_cast<int>(std::round(normVal * (numFrames - 1))));

            const auto& frameImg = filmstrip->frames[frameIdx];

            // Preserve frame aspect ratio within area
            float frameAspect = static_cast<float>(filmstrip->frameW) / static_cast<float>(std::max(1, filmstrip->frameH));
            float curAspect = area.getWidth() / std::max(1.0f, area.getHeight());
            juce::Rectangle<float> drawBounds;
            if (curAspect > frameAspect)
            {
                float w = area.getHeight() * frameAspect;
                drawBounds = area.withSizeKeepingCentre(w, area.getHeight());
            }
            else
            {
                float h = area.getWidth() / frameAspect;
                drawBounds = area.withSizeKeepingCentre(area.getWidth(), h);
            }

            g.drawImage(frameImg, drawBounds, juce::RectanglePlacement::fillDestination);

            // Value overlay on hover/drag
            if (isHovered || isDragging)
            {
                auto valRect = juce::Rectangle<float>(drawBounds.getCentreX() - 30.0f * scale,
                                                      drawBounds.getBottom() - 16.0f * scale,
                                                      60.0f * scale, 14.0f * scale);
                g.setColour(juce::Colour(0xEE101214));
                g.fillRoundedRectangle(valRect, 3.0f);
                g.setColour(fgColor);
                g.setFont(juce::Font(std::max(7.5f, 9.5f * scale)).boldened());
                g.drawText(getFormattedValueString(), valRect, juce::Justification::centred, true);
            }
        }
        else
        {
            // ── Native Procedural Decent Sampler 270-degree Arc Knob ──
            float cx = dialBounds.getCentreX();
            float cy = dialBounds.getCentreY();
            float radius = dialBounds.getWidth() * 0.5f;

            float trackThickness = std::max(2.8f, std::min(7.0f, 4.0f * scale));

            // Decent Sampler 270-degree sweep (from 135 deg to 405 deg)
            float startAngle = 2.35619449f; // 135 deg in radians
            float endAngle   = 7.06858347f; // 405 deg in radians
            float currentAngle = startAngle + normVal * (endAngle - startAngle);

            // 1. Background Arc Track
            juce::Path bgArc;
            bgArc.addCentredArc(cx, cy, radius - trackThickness * 0.5f, radius - trackThickness * 0.5f,
                                0.0f, startAngle, endAngle, true);
            g.setColour(bgColor);
            g.strokePath(bgArc, juce::PathStrokeType(trackThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // 2. Active Foreground Arc Track
            if (normVal > 0.001f)
            {
                juce::Path fgArc;
                fgArc.addCentredArc(cx, cy, radius - trackThickness * 0.5f, radius - trackThickness * 0.5f,
                                    0.0f, startAngle, currentAngle, true);
                g.setColour(fgColor);
                g.strokePath(fgArc, juce::PathStrokeType(trackThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }

            // 3. Central Knob Dial Body
            float bodyRadius = std::max(4.0f, radius - trackThickness - 2.5f * scale);
            auto bodyBounds = juce::Rectangle<float>(cx - bodyRadius, cy - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f);

            // Radial Metallic Gradient
            juce::ColourGradient bodyGrad(juce::Colour(0xFF2B2E34), cx, cy - bodyRadius * 0.35f,
                                          juce::Colour(0xFF131517), cx, cy + bodyRadius, false);
            g.setGradientFill(bodyGrad);
            g.fillEllipse(bodyBounds);

            // Outer Bevel Ring
            g.setColour(juce::Colour(0x33FFFFFF));
            g.drawEllipse(bodyBounds, 1.0f);

            // 4. Indicator Needle / Pip Pointer
            float needleInnerR = bodyRadius * 0.38f;
            float needleOuterR = bodyRadius - 1.5f;

            float sinA = std::sin(currentAngle);
            float cosA = -std::cos(currentAngle); // Screen Y goes downwards

            float x1 = cx + needleInnerR * sinA;
            float y1 = cy + needleInnerR * cosA;
            float x2 = cx + needleOuterR * sinA;
            float y2 = cy + needleOuterR * cosA;

            g.setColour(isHovered || isDragging ? fgColor : juce::Colours::white);
            g.drawLine(x1, y1, x2, y2, std::max(1.8f, 2.2f * scale));

            // 5. Value overlay on hover/drag
            if (isHovered || isDragging)
            {
                auto valRect = juce::Rectangle<float>(cx - 30.0f * scale, cy + bodyRadius * 0.35f, 60.0f * scale, 14.0f * scale);
                g.setColour(juce::Colour(0xEE101214));
                g.fillRoundedRectangle(valRect, 3.0f);
                g.setColour(fgColor);
                g.setFont(juce::Font(std::max(7.5f, 9.5f * scale)).boldened());
                g.drawText(getFormattedValueString(), valRect, juce::Justification::centred, true);
            }
        }
    }
}

// ── DecentSamplerCanvasComponent Implementation ─────────────
DecentSamplerCanvasComponent::DecentSamplerCanvasComponent(AudioEngine& engine)
    : audioEngine(engine)
{
    setOpaque(true);
    startTimerHz(30);
}

DecentSamplerCanvasComponent::~DecentSamplerCanvasComponent()
{
    stopTimer();
}

juce::Rectangle<float> DecentSamplerCanvasComponent::getCanvasBounds() const
{
    auto area = getLocalBounds().toFloat().reduced(8.0f);

    float baseW = static_cast<float>(currentState.customUi.width > 0 ? currentState.customUi.width : (bgImage.isValid() ? bgImage.getWidth() : 812));
    float baseH = static_cast<float>(currentState.customUi.height > 0 ? currentState.customUi.height : (bgImage.isValid() ? bgImage.getHeight() : 375));
    if (baseW <= 0) baseW = 812.0f;
    if (baseH <= 0) baseH = 375.0f;

    float targetRatio = baseW / baseH;
    float currentRatio = area.getWidth() / std::max(1.0f, area.getHeight());

    if (currentRatio > targetRatio)
    {
        float newW = area.getHeight() * targetRatio;
        return area.withSizeKeepingCentre(newW, area.getHeight());
    }
    else
    {
        float newH = area.getWidth() / targetRatio;
        return area.withSizeKeepingCentre(area.getWidth(), newH);
    }
}

void DecentSamplerCanvasComponent::paint(juce::Graphics& g)
{
    g.fillAll(OpenWavLookAndFeel::bgDark);

    auto canvasBounds = getCanvasBounds();
    if (canvasBounds.isEmpty()) return;

    float baseW = static_cast<float>(currentState.customUi.width > 0 ? currentState.customUi.width : (bgImage.isValid() ? bgImage.getWidth() : 812));
    float baseH = static_cast<float>(currentState.customUi.height > 0 ? currentState.customUi.height : (bgImage.isValid() ? bgImage.getHeight() : 375));
    if (baseW <= 0) baseW = 812.0f;
    if (baseH <= 0) baseH = 375.0f;
    float scale = canvasBounds.getWidth() / baseW;
    float navH = topNavPaddingY * scale;

    // Outer subtle drop-shadow glow around entire image canvas
    g.setColour(juce::Colour(0x44000000));
    g.fillRoundedRectangle(canvasBounds.expanded(3.0f), 8.0f);

    // Save graphics state for smooth clipped canvas content
    {
        juce::Graphics::ScopedSaveState sss(g);
        juce::Path canvasClip;
        canvasClip.addRoundedRectangle(canvasBounds, 6.0f);
        g.reduceClipRegion(canvasClip);

        // 1. Draw Background color spanning entire canvas
        if (parsedBgColor.isOpaque() || parsedBgColor.getAlpha() > 0)
        {
            g.setColour(parsedBgColor);
            g.fillRect(canvasBounds);
        }
        else
        {
            // Dark textured background card
            juce::ColourGradient grad(OpenWavLookAndFeel::bgCard, canvasBounds.getX(), canvasBounds.getY(),
                                      OpenWavLookAndFeel::bgCard.darker(0.35f), canvasBounds.getX(), canvasBounds.getBottom(), false);
            g.setGradientFill(grad);
            g.fillRect(canvasBounds);
        }

        // 2. Draw Background Image across entire canvas (image falls behind top nav bar)
        if (bgImage.isValid())
        {
            g.drawImage(bgImage, canvasBounds, juce::RectanglePlacement::fillDestination);
        }

        // 3. Top Glass Header / Navigation Bar
        auto navRect = canvasBounds.withHeight(navH);
        juce::ColourGradient navGrad(juce::Colour(0xCC0E1116), navRect.getX(), navRect.getY(),
                                     juce::Colour(0x880E1116), navRect.getX(), navRect.getBottom(), false);
        g.setGradientFill(navGrad);
        g.fillRect(navRect);

        // Subtle bottom divider line for header bar
        g.setColour(juce::Colour(0x40FFFFFF));
        g.drawHorizontalLine(static_cast<int>(navRect.getBottom()), canvasBounds.getX(), canvasBounds.getRight());

        // Preset Title in Header
        juce::String title = currentState.instrumentName.isNotEmpty() ? currentState.instrumentName : "Decent Sampler Preset";
        g.setColour(OpenWavLookAndFeel::accentCyan);
        g.setFont(juce::Font(std::max(11.0f, 13.5f * scale)).boldened());
        g.drawText(title, navRect.reduced(14.0f * scale, 0.0f).removeFromLeft(280.0f * scale), juce::Justification::centredLeft, true);
    }

    // 4. Crisp High-Visibility Bounding Box Frame around entire image
    g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.45f));
    g.drawRoundedRectangle(canvasBounds, 6.0f, 1.5f);
}

void DecentSamplerCanvasComponent::resized()
{
    auto canvasBounds = getCanvasBounds();
    float baseW = static_cast<float>(currentState.customUi.width > 0 ? currentState.customUi.width : (bgImage.isValid() ? bgImage.getWidth() : 812));
    float baseH = static_cast<float>(currentState.customUi.height > 0 ? currentState.customUi.height : (bgImage.isValid() ? bgImage.getHeight() : 375));
    if (baseW <= 0) baseW = 812.0f;
    if (baseH <= 0) baseH = 375.0f;

    float scale = canvasBounds.getWidth() / baseW;
    float navH = topNavPaddingY * scale;

    float offX = canvasBounds.getX();
    float offY = canvasBounds.getY() + navH; // Controls start below the top nav bar!

    // Layout Tab buttons inside the top nav bar on the right
    if (tabButtons.size() > 1)
    {
        auto tabArea = juce::Rectangle<float>(canvasBounds.getX() + 290.0f * scale, canvasBounds.getY(),
                                              canvasBounds.getWidth() - 300.0f * scale, navH).reduced(4.0f, 3.0f);
        int tabW = std::min(static_cast<int>(120.0f * scale), static_cast<int>(tabArea.getWidth() / tabButtons.size()));
        for (int i = 0; i < tabButtons.size(); ++i)
        {
            tabButtons[i]->setBounds(tabArea.removeFromLeft(tabW).toNearestInt().reduced(2, 0));
        }
    }

    // Layout Images
    for (auto& item : images)
    {
        if (item.imageComp != nullptr)
        {
            int px = static_cast<int>(offX + item.model.x * scale);
            int py = static_cast<int>(offY + item.model.y * scale);
            int pw = static_cast<int>(item.model.width * scale);
            int ph = static_cast<int>(item.model.height * scale);
            item.imageComp->setBounds(px, py, pw, ph);
        }
    }

    // Layout Labels
    for (auto& item : labels)
    {
        if (item.label != nullptr)
        {
            int px = static_cast<int>(offX + item.model.x * scale);
            int py = static_cast<int>(offY + item.model.y * scale);
            int pw = static_cast<int>(item.model.width * scale);
            int ph = static_cast<int>(item.model.height * scale);
            item.label->setBounds(px, py, pw, ph);
            item.label->setFont(juce::Font(std::max(10.0f, item.model.textSize * scale)).boldened());
        }
    }

    // Layout Buttons
    for (auto& item : buttons)
    {
        if (item.button != nullptr)
        {
            int px = static_cast<int>(offX + item.model.x * scale);
            int py = static_cast<int>(offY + item.model.y * scale);
            int pw = static_cast<int>(item.model.width * scale);
            int ph = static_cast<int>(item.model.height * scale);
            item.button->setBounds(px, py, pw, ph);
        }
    }

    // Layout Menus
    for (auto& item : menus)
    {
        if (item.combo != nullptr)
        {
            int px = static_cast<int>(offX + item.model.x * scale);
            int py = static_cast<int>(offY + item.model.y * scale);
            int pw = static_cast<int>(item.model.width * scale);
            int ph = static_cast<int>(item.model.height * scale);
            item.combo->setBounds(px, py, pw, ph);
        }
    }

    // Layout Controls / Knobs
    std::vector<ControlItem*> unpositioned;
    for (auto& item : controls)
    {
        if (item.control != nullptr)
        {
            item.control->setScale(scale);
            if (item.model.x >= 0 && item.model.y >= 0)
            {
                int px = static_cast<int>(offX + item.model.x * scale);
                int py = static_cast<int>(offY + item.model.y * scale);
                int pw = static_cast<int>(item.model.width * scale);
                int ph = static_cast<int>(item.model.height * scale);
                item.control->setBounds(px, py, pw, ph);
            }
            else
            {
                unpositioned.push_back(&item);
            }
        }
    }

    // Auto-arrange any controls without explicit (x, y) coordinates
    if (!unpositioned.empty())
    {
        int numUnpos = static_cast<int>(unpositioned.size());
        int cols = std::min(numUnpos, 4);
        int itemW = static_cast<int>(canvasBounds.getWidth() / cols);
        int itemH = static_cast<int>(std::min(110.0f, (canvasBounds.getHeight() - navH) / 2.5f));
        int startY = static_cast<int>(canvasBounds.getBottom() - itemH * ((numUnpos + cols - 1) / cols) - 16);

        for (int i = 0; i < numUnpos; ++i)
        {
            int col = i % cols;
            int row = i / cols;
            auto cell = juce::Rectangle<int>(static_cast<int>(offX + col * itemW),
                                             startY + row * itemH,
                                             itemW, itemH).reduced(6);
            if (unpositioned[i]->control != nullptr)
                unpositioned[i]->control->setBounds(cell);
        }
    }
}

void DecentSamplerCanvasComponent::setInstrumentState(const SampleMapState& state)
{
    currentState = state;

    // Load background image
    bgImage = {};
    if (currentState.customUi.resolvedBgImagePath.isNotEmpty())
    {
        juce::File imgFile(currentState.customUi.resolvedBgImagePath);
        if (imgFile.existsAsFile())
        {
            bgImage = juce::ImageFileFormat::loadFrom(imgFile);
        }
    }

    // Background color
    parsedBgColor = DecentSamplerControlComponent::parseDecentSamplerColor(currentState.customUi.bgColorHex, juce::Colours::transparentBlack);

    if (bgImage.isValid())
    {
        if (currentState.customUi.width <= 0)
            currentState.customUi.width = bgImage.getWidth();
        if (currentState.customUi.height <= 0)
            currentState.customUi.height = bgImage.getHeight();
    }

    // Rebuild Tab Buttons
    tabButtons.clear();
    int numTabs = static_cast<int>(currentState.customUi.tabs.size());
    if (numTabs > 1)
    {
        for (int i = 0; i < numTabs; ++i)
        {
            auto* btn = new juce::TextButton(currentState.customUi.tabs[i].name);
            btn->setClickingTogglesState(true);
            btn->setRadioGroupId(9901);
            btn->setToggleState(i == currentTab, juce::dontSendNotification);
            btn->onClick = [this, i] {
                currentTab = i;
                rebuildActiveTabUi();
            };
            addAndMakeVisible(btn);
            tabButtons.add(btn);
        }
    }

    if (currentTab >= numTabs)
        currentTab = 0;

    // Apply real-time group states to audio engine
    audioEngine.resetAllGroups();
    for (const auto& g : currentState.groups)
    {
        audioEngine.setGroupVolumeDb(g.index, g.volumeDb);
        audioEngine.setGroupPan(g.index, g.pan);
        audioEngine.setGroupTuningCents(g.index, g.fineTuneCents);
        audioEngine.setGroupMuted(g.index, g.muted || !g.enabled);
    }

    // Apply LFO modulation states to audio engine
    for (const auto& m : currentState.modulators)
    {
        audioEngine.setLfoFrequency(m.frequency);
        audioEngine.setLfoAmount(m.modAmount);
        audioEngine.setLfoShapeByName(m.shape);
        audioEngine.setLfoTargetByName(m.target);
    }

    // Apply IR Convolution Reverb if present
    if (currentState.irFilePath.isNotEmpty())
    {
        audioEngine.loadImpulseResponseFile(juce::File(currentState.irFilePath));
        audioEngine.setSamplerIrReverbAmount(currentState.irReverbWetLevel);
        audioEngine.setSamplerIrReverbDryLevel(currentState.irReverbDryLevel);
    }

    // Apply Delay, Chorus, and Filters
    audioEngine.setSamplerDelay(currentState.delayTimeMs, currentState.delayFeedback, currentState.delayWetLevel);
    audioEngine.setSamplerChorus(currentState.chorusRateHz, currentState.chorusDepth, currentState.chorusWetLevel);
    audioEngine.setSamplerReverbAmount(currentState.samplerReverbAmount);
    audioEngine.setSamplerLowpassCutoff(currentState.masterFilterCutoffHz);
    audioEngine.setSamplerHighpassCutoff(currentState.masterHighpassHz);

    rebuildActiveTabUi();
}

SampleMapState DecentSamplerCanvasComponent::getInstrumentState() const
{
    return currentState;
}

void DecentSamplerCanvasComponent::rebuildActiveTabUi()
{
    controls.clear();
    labels.clear();
    images.clear();
    buttons.clear();
    menus.clear();

    if (currentState.customUi.tabs.empty())
    {
        if (!currentState.uiControls.empty())
        {
            DecentSamplerTabState fallbackTab;
            fallbackTab.name = "Main";
            fallbackTab.controls = currentState.uiControls;
            currentState.customUi.tabs.push_back(fallbackTab);
        }
        else
        {
            repaint();
            return;
        }
    }

    const auto& tab = currentState.customUi.tabs[juce::jlimit(0, static_cast<int>(currentState.customUi.tabs.size()) - 1, currentTab)];

    // 1. Build Images
    for (const auto& imgModel : tab.images)
    {
        ImageItem item;
        item.model = imgModel;
        item.imageComp = std::make_unique<juce::ImageComponent>();

        if (imgModel.resolvedFilePath.isNotEmpty())
        {
            juce::File imgFile(imgModel.resolvedFilePath);
            if (imgFile.existsAsFile())
            {
                auto img = juce::ImageFileFormat::loadFrom(imgFile);
                item.imageComp->setImage(img, juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
            }
        }
        addAndMakeVisible(*item.imageComp);
        images.push_back(std::move(item));
    }

    // 2. Build Labels
    for (const auto& lblModel : tab.labels)
    {
        LabelItem item;
        item.model = lblModel;
        item.label = std::make_unique<juce::Label>();
        item.label->setText(lblModel.text, juce::dontSendNotification);

        juce::Colour txtCol = DecentSamplerControlComponent::parseDecentSamplerColor(lblModel.textColorHex, OpenWavLookAndFeel::accentCyan);
        item.label->setColour(juce::Label::textColourId, txtCol);

        if (lblModel.textAlignment.containsIgnoreCase("left"))
            item.label->setJustificationType(juce::Justification::centredLeft);
        else if (lblModel.textAlignment.containsIgnoreCase("right"))
            item.label->setJustificationType(juce::Justification::centredRight);
        else
            item.label->setJustificationType(juce::Justification::centred);

        addAndMakeVisible(*item.label);
        labels.push_back(std::move(item));
    }

    // 3. Build 1:1 Decent Sampler Knobs & Sliders
    for (const auto& ctrlModel : tab.controls)
    {
        ControlItem item;
        item.model = ctrlModel;

        item.control = std::make_unique<DecentSamplerControlComponent>(ctrlModel);
        item.control->onValueChanged = [this, ctrlModel](double val) {
            // Update in global currentState
            for (auto& c : currentState.uiControls)
            {
                if (c.id == ctrlModel.id || c.label == ctrlModel.label)
                {
                    c.currentValue = val;
                }
            }

            applyControlBindings(ctrlModel, val);

            if (onStateChanged)
                onStateChanged(currentState);
        };

        addAndMakeVisible(*item.control);
        controls.push_back(std::move(item));
    }

    // 4. Build Buttons
    for (const auto& btnModel : tab.buttons)
    {
        ButtonItem item;
        item.model = btnModel;
        item.button = std::make_unique<juce::TextButton>(btnModel.text);
        if (btnModel.style.containsIgnoreCase("toggle"))
        {
            item.button->setClickingTogglesState(true);
            item.button->setToggleState(btnModel.state, juce::dontSendNotification);
        }
        item.button->addListener(this);
        addAndMakeVisible(*item.button);
        buttons.push_back(std::move(item));
    }

    // 5. Build Menus
    for (const auto& menuModel : tab.menus)
    {
        MenuItem item;
        item.model = menuModel;
        item.combo = std::make_unique<juce::ComboBox>();
        for (int i = 0; i < menuModel.options.size(); ++i)
        {
            item.combo->addItem(menuModel.options[i], i + 1);
        }
        item.combo->setSelectedId(menuModel.selectedIndex + 1, juce::dontSendNotification);
        item.combo->addListener(this);
        addAndMakeVisible(*item.combo);
        menus.push_back(std::move(item));
    }

    resized();
    repaint();
}

void DecentSamplerCanvasComponent::applyBinding(const DecentSamplerBinding& binding, double value)
{
    juce::String param = binding.parameter;
    juce::String level = binding.level;
    juce::String type = binding.type;
    int pos = binding.position;

    // 1. Group Level Bindings
    if (level.equalsIgnoreCase("group"))
    {
        if (param.containsIgnoreCase("volume") || param.containsIgnoreCase("gain") || param.containsIgnoreCase("amp") || param.containsIgnoreCase("level"))
        {
            float volDb = 0.0f;
            if (value <= 0.0001)
            {
                volDb = -96.0f;
            }
            else if (value <= 1.0 && value >= 0.0)
            {
                volDb = 20.0f * std::log10(static_cast<float>(value));
            }
            else
            {
                volDb = static_cast<float>(value);
            }
            audioEngine.setGroupVolumeDb(pos, volDb);
            if (pos >= 0 && pos < static_cast<int>(currentState.groups.size()))
                currentState.groups[pos].volumeDb = volDb;
        }
        else if (param.containsIgnoreCase("pan") || param.containsIgnoreCase("panning"))
        {
            float pan = static_cast<float>(value);
            audioEngine.setGroupPan(pos, pan);
            if (pos >= 0 && pos < static_cast<int>(currentState.groups.size()))
                currentState.groups[pos].pan = pan;
        }
        else if (param.containsIgnoreCase("tune") || param.containsIgnoreCase("pitch"))
        {
            float cents = static_cast<float>(value);
            audioEngine.setGroupTuningCents(pos, cents);
            if (pos >= 0 && pos < static_cast<int>(currentState.groups.size()))
                currentState.groups[pos].fineTuneCents = cents;
        }
        else if (param.containsIgnoreCase("mute") || param.containsIgnoreCase("enabled") || param.containsIgnoreCase("active"))
        {
            bool muted = param.containsIgnoreCase("mute") ? (value > 0.5) : (value < 0.5);
            audioEngine.setGroupMuted(pos, muted);
            if (pos >= 0 && pos < static_cast<int>(currentState.groups.size()))
            {
                currentState.groups[pos].muted = muted;
                currentState.groups[pos].enabled = !muted;
            }
        }
        else if (param.containsIgnoreCase("attack") || param.containsIgnoreCase("ENV_ATTACK"))
        {
            float attMs = static_cast<float>(value);
            if (attMs <= 10.0f) attMs *= 1000.0f;
            for (auto& z : currentState.zones)
                if (z.groupIndex == pos) z.attackMs = attMs;
        }
        else if (param.containsIgnoreCase("decay") || param.containsIgnoreCase("ENV_DECAY"))
        {
            float decMs = static_cast<float>(value);
            if (decMs <= 10.0f) decMs *= 1000.0f;
            for (auto& z : currentState.zones)
                if (z.groupIndex == pos) z.decayMs = decMs;
        }
        else if (param.containsIgnoreCase("sustain") || param.containsIgnoreCase("ENV_SUSTAIN"))
        {
            float sus = static_cast<float>(value);
            for (auto& z : currentState.zones)
                if (z.groupIndex == pos) z.sustainLevel = sus;
        }
        else if (param.containsIgnoreCase("release") || param.containsIgnoreCase("ENV_RELEASE"))
        {
            float relMs = static_cast<float>(value);
            if (relMs <= 10.0f) relMs *= 1000.0f;
            for (auto& z : currentState.zones)
                if (z.groupIndex == pos) z.releaseMs = relMs;
        }
        return;
    }

    // 1b. Effect Level Bindings (level="effect")
    if (level.equalsIgnoreCase("effect"))
    {
        int fxIdx = pos;
        if (fxIdx >= 0 && fxIdx < static_cast<int>(currentState.effects.size()))
        {
            auto& eff = currentState.effects[fxIdx];
            if (param.containsIgnoreCase("WET") || param.containsIgnoreCase("wetLevel") || param.containsIgnoreCase("mix"))
            {
                float wet = static_cast<float>(value);
                if (wet > 1.0f) wet /= 100.0f;
                eff.wetLevel = wet;
                if (eff.type.containsIgnoreCase("reverb"))
                {
                    currentState.samplerReverbAmount = wet;
                    audioEngine.setSamplerReverbAmount(wet);
                }
                else if (eff.type.containsIgnoreCase("convolution") || eff.type.containsIgnoreCase("ir"))
                {
                    currentState.irReverbWetLevel = wet;
                    audioEngine.setSamplerIrReverbAmount(wet);
                }
                else if (eff.type.containsIgnoreCase("delay") || eff.type.containsIgnoreCase("echo"))
                {
                    currentState.delayWetLevel = wet;
                    audioEngine.setSamplerDelayWetLevel(wet);
                }
                else if (eff.type.containsIgnoreCase("chorus"))
                {
                    currentState.chorusWetLevel = wet;
                    audioEngine.setSamplerChorusWet(wet);
                }
            }
            else if (param.containsIgnoreCase("DRY") || param.containsIgnoreCase("dryLevel"))
            {
                float dry = static_cast<float>(value);
                if (dry > 1.0f) dry /= 100.0f;
                eff.dryLevel = dry;
                if (eff.type.containsIgnoreCase("convolution") || eff.type.containsIgnoreCase("ir"))
                {
                    currentState.irReverbDryLevel = dry;
                    audioEngine.setSamplerIrReverbDryLevel(dry);
                }
            }
            else if (param.containsIgnoreCase("roomSize") || param.containsIgnoreCase("room"))
            {
                eff.roomSize = static_cast<float>(value);
            }
            else if (param.containsIgnoreCase("damping"))
            {
                eff.damping = static_cast<float>(value);
            }
            else if (param.containsIgnoreCase("feedback") || param.containsIgnoreCase("fb"))
            {
                float fb = juce::jlimit(0.0f, 0.95f, static_cast<float>(value));
                eff.feedback = fb;
                currentState.delayFeedback = fb;
                audioEngine.setSamplerDelayFeedback(fb);
            }
            else if (param.containsIgnoreCase("time") || param.containsIgnoreCase("delayTime"))
            {
                float ms = static_cast<float>(value);
                if (ms <= 10.0f) ms *= 1000.0f;
                eff.delayTimeMs = ms;
                currentState.delayTimeMs = ms;
                audioEngine.setSamplerDelayTimeMs(ms);
            }
            else if (param.containsIgnoreCase("frequency") || param.containsIgnoreCase("freq") || param.containsIgnoreCase("cutoff"))
            {
                float freq = static_cast<float>(value);
                if (freq <= 1.0f) freq = 100.0f * std::pow(220.0f, freq);
                eff.frequency = freq;
                if (eff.type.containsIgnoreCase("lowpass"))
                {
                    currentState.masterFilterCutoffHz = freq;
                    audioEngine.setSamplerLowpassCutoff(freq);
                }
                else if (eff.type.containsIgnoreCase("highpass"))
                {
                    currentState.masterHighpassHz = freq;
                    audioEngine.setSamplerHighpassCutoff(freq);
                }
            }
            else if (param.containsIgnoreCase("rate"))
            {
                float rate = static_cast<float>(value);
                if (eff.type.containsIgnoreCase("chorus"))
                {
                    currentState.chorusRateHz = rate;
                    audioEngine.setSamplerChorusRate(rate);
                }
            }
            else if (param.containsIgnoreCase("depth"))
            {
                float d = static_cast<float>(value);
                if (d > 1.0f) d /= 100.0f;
                if (eff.type.containsIgnoreCase("chorus"))
                {
                    currentState.chorusDepth = d;
                    audioEngine.setSamplerChorusDepth(d);
                }
            }
        }
        // Fall through to also try parameter-name matching for common reverb/effect names
    }

    // 2. Modulator (LFO) Bindings
    if (type.equalsIgnoreCase("modulator") || param.containsIgnoreCase("lfo") || param.containsIgnoreCase("mod_"))
    {
        if (param.containsIgnoreCase("amount") || param.containsIgnoreCase("depth") || param.containsIgnoreCase("intensity"))
        {
            float amt = static_cast<float>(value);
            if (amt > 1.0f) amt /= 100.0f;
            audioEngine.setLfoAmount(amt);
        }
        else if (param.containsIgnoreCase("freq") || param.containsIgnoreCase("rate") || param.containsIgnoreCase("speed"))
        {
            audioEngine.setLfoFrequency(static_cast<float>(value));
        }
        else if (param.containsIgnoreCase("shape") || param.containsIgnoreCase("waveform"))
        {
            audioEngine.setLfoShape(static_cast<int>(value));
        }
        else if (param.containsIgnoreCase("target") || param.containsIgnoreCase("dest"))
        {
            audioEngine.setLfoTarget(static_cast<int>(value));
        }
        return;
    }

    // 3. Effects and Instrument Level Bindings
    if (param.containsIgnoreCase("ir") || param.containsIgnoreCase("convolution"))
    {
        float rev = static_cast<float>(value);
        if (rev > 1.0f) rev /= 100.0f;
        currentState.irReverbWetLevel = rev;
        audioEngine.setSamplerIrReverbAmount(rev);
    }
    else if (param.containsIgnoreCase("delay_time") || param.containsIgnoreCase("delay_ms") || param.containsIgnoreCase("echo_time"))
    {
        float ms = static_cast<float>(value);
        if (ms <= 10.0f) ms *= 1000.0f;
        currentState.delayTimeMs = ms;
        audioEngine.setSamplerDelayTimeMs(ms);
    }
    else if (param.containsIgnoreCase("delay_feedback") || param.containsIgnoreCase("feedback") || param.containsIgnoreCase("echo_feedback"))
    {
        float fb = static_cast<float>(value);
        if (fb > 1.0f) fb /= 100.0f;
        currentState.delayFeedback = fb;
        audioEngine.setSamplerDelayFeedback(fb);
    }
    else if (param.containsIgnoreCase("delay_wet") || param.containsIgnoreCase("delay") || param.containsIgnoreCase("echo"))
    {
        float wet = static_cast<float>(value);
        if (wet > 1.0f) wet /= 100.0f;
        currentState.delayWetLevel = wet;
        audioEngine.setSamplerDelayWetLevel(wet);
    }
    else if (param.containsIgnoreCase("chorus_rate") || param.containsIgnoreCase("chorus_speed"))
    {
        audioEngine.setSamplerChorusRate(static_cast<float>(value));
        currentState.chorusRateHz = static_cast<float>(value);
    }
    else if (param.containsIgnoreCase("chorus_depth"))
    {
        float d = static_cast<float>(value);
        if (d > 1.0f) d /= 100.0f;
        audioEngine.setSamplerChorusDepth(d);
        currentState.chorusDepth = d;
    }
    else if (param.containsIgnoreCase("chorus_wet") || param.containsIgnoreCase("chorus"))
    {
        float wet = static_cast<float>(value);
        if (wet > 1.0f) wet /= 100.0f;
        audioEngine.setSamplerChorusWet(wet);
        currentState.chorusWetLevel = wet;
    }
    else if (param.containsIgnoreCase("attack") || param.containsIgnoreCase("ENV_ATTACK"))
    {
        float attMs = static_cast<float>(value);
        if (attMs <= 10.0f) attMs *= 1000.0f;
        currentState.globalAttackMs = attMs;
        for (auto& z : currentState.zones) z.attackMs = attMs;
    }
    else if (param.containsIgnoreCase("decay") || param.containsIgnoreCase("ENV_DECAY"))
    {
        float decMs = static_cast<float>(value);
        if (decMs <= 10.0f) decMs *= 1000.0f;
        currentState.globalDecayMs = decMs;
        for (auto& z : currentState.zones) z.decayMs = decMs;
    }
    else if (param.containsIgnoreCase("sustain") || param.containsIgnoreCase("ENV_SUSTAIN"))
    {
        float sus = static_cast<float>(value);
        currentState.globalSustainLevel = sus;
        for (auto& z : currentState.zones) z.sustainLevel = sus;
    }
    else if (param.containsIgnoreCase("release") || param.containsIgnoreCase("ENV_RELEASE"))
    {
        float relMs = static_cast<float>(value);
        if (relMs <= 10.0f) relMs *= 1000.0f;
        currentState.globalReleaseMs = relMs;
        for (auto& z : currentState.zones) z.releaseMs = relMs;
    }
    else if (param.containsIgnoreCase("volume") || param.containsIgnoreCase("AMP_VOLUME") || param.containsIgnoreCase("gain"))
    {
        float vol = static_cast<float>(value);
        if (vol <= 0.0001f)
        {
            audioEngine.setGain(0.0f);
            currentState.masterGainDb = -96.0f;
        }
        else if (vol <= 1.0f && vol >= 0.0f)
        {
            audioEngine.setGain(vol);
            currentState.masterGainDb = 20.0f * std::log10(vol);
        }
        else
        {
            audioEngine.setGain(std::pow(10.0f, vol / 20.0f));
            currentState.masterGainDb = vol;
        }
    }
    else if (param.containsIgnoreCase("reverb") || param.containsIgnoreCase("FX_REVERB_WET_LEVEL"))
    {
        float rev = static_cast<float>(value);
        if (rev > 1.0f) rev /= 100.0f;
        currentState.samplerReverbAmount = rev;
        audioEngine.setSamplerReverbAmount(rev);
    }
    else if (param.containsIgnoreCase("highpass") || param.containsIgnoreCase("high_pass") || param.containsIgnoreCase("FX_HIGHPASS") || param.containsIgnoreCase("FX_FILTER_HP"))
    {
        float hp = static_cast<float>(value);
        if (hp <= 1.0f)
            hp = 20.0f * std::pow(200.0f, hp); // 20 Hz to 4000 Hz
        currentState.masterHighpassHz = hp;
        audioEngine.setSamplerHighpassCutoff(hp);
    }
    else if (param.containsIgnoreCase("tone") || param.containsIgnoreCase("FX_TONE") || param.containsIgnoreCase("tone_control"))
    {
        float t = static_cast<float>(value);
        if (t > 1.0f) t /= 100.0f;
        currentState.masterTone = t;
        float cutoff = 300.0f * std::pow(22000.0f / 300.0f, t);
        currentState.masterFilterCutoffHz = cutoff;
        audioEngine.setSamplerTone(t);
        audioEngine.setSamplerLowpassCutoff(cutoff);
    }
    else if (param.containsIgnoreCase("cutoff") || param.containsIgnoreCase("lowpass") || param.containsIgnoreCase("low_pass") ||
             param.containsIgnoreCase("filter") || param.containsIgnoreCase("FX_FILTER_FREQUENCY") || param.containsIgnoreCase("FX_LOWPASS") ||
             param.containsIgnoreCase("frequency") || param.containsIgnoreCase("LP_FREQ"))
    {
        float cutoff = static_cast<float>(value);
        if (cutoff <= 1.0f)
            cutoff = 100.0f * std::pow(220.0f, cutoff); // 100 Hz to 22000 Hz
        currentState.masterFilterCutoffHz = cutoff;
        audioEngine.setSamplerLowpassCutoff(cutoff);
    }
    else if (param.containsIgnoreCase("tune") || param.containsIgnoreCase("pitch") || param.containsIgnoreCase("PITCH"))
    {
        float cents = static_cast<float>(value);
        currentState.masterFineTuneCents = cents;
    }
}

void DecentSamplerCanvasComponent::applyControlBindings(const DecentSamplerUiControl& ctrl, double value)
{
    if (!ctrl.bindings.empty())
    {
        for (const auto& b : ctrl.bindings)
            applyBinding(b, value);
    }
    else
    {
        // Fallback by parameterName or label
        DecentSamplerBinding b;
        b.parameter = ctrl.bindingParam.isNotEmpty() ? ctrl.bindingParam : (ctrl.parameterName.isNotEmpty() ? ctrl.parameterName : ctrl.label);
        applyBinding(b, value);
    }
}

void DecentSamplerCanvasComponent::buttonClicked(juce::Button* button)
{
    for (auto& item : buttons)
    {
        if (item.button.get() == button)
        {
            item.model.state = button->getToggleState();
            for (const auto& b : item.model.bindings)
            {
                applyBinding(b, item.model.state ? 1.0 : 0.0);
            }
            break;
        }
    }

    if (onStateChanged)
        onStateChanged(currentState);
}

void DecentSamplerCanvasComponent::comboBoxChanged(juce::ComboBox* comboBox)
{
    for (auto& item : menus)
    {
        if (item.combo.get() == comboBox)
        {
            item.model.selectedIndex = comboBox->getSelectedId() - 1;
            for (const auto& b : item.model.bindings)
            {
                applyBinding(b, static_cast<double>(item.model.selectedIndex));
            }
            break;
        }
    }

    if (onStateChanged)
        onStateChanged(currentState);
}

void DecentSamplerCanvasComponent::timerCallback()
{
    float lfoOut = audioEngine.getCurrentLfoOutput(); // -1.0 to 1.0 (frequency, shape, and mod wheel depth applied)

    if (currentState.modulators.empty() && std::abs(lfoOut) < 0.0001f)
        return;

    for (const auto& m : currentState.modulators)
    {
        juce::String tgt = m.target.toLowerCase().trim();
        if (tgt.isEmpty())
            tgt = "cutoff";

        for (auto& item : controls)
        {
            if (item.control == nullptr) continue;

            bool matches = false;
            // Direct ID, parameterName, label, or bindingParam match
            if (item.model.id.equalsIgnoreCase(m.target) ||
                item.model.parameterName.equalsIgnoreCase(m.target) ||
                item.model.label.equalsIgnoreCase(m.target) ||
                item.model.bindingParam.equalsIgnoreCase(m.target))
            {
                matches = true;
            }

            // Binding match
            if (!matches)
            {
                for (const auto& b : item.model.bindings)
                {
                    if (b.parameter.equalsIgnoreCase(m.target) || b.type.equalsIgnoreCase(m.target))
                    {
                        matches = true;
                        break;
                    }
                }
            }

            // Keyword / semantic match
            if (!matches)
            {
                if ((tgt.contains("filter") || tgt.contains("cutoff") || tgt.contains("tone")) &&
                    (item.model.id.containsIgnoreCase("tone") || item.model.id.containsIgnoreCase("cutoff") ||
                     item.model.label.containsIgnoreCase("tone") || item.model.label.containsIgnoreCase("cutoff") ||
                     item.model.bindingParam.containsIgnoreCase("cutoff") || item.model.bindingParam.containsIgnoreCase("filter")))
                {
                    matches = true;
                }
                else if ((tgt.contains("volume") || tgt.contains("gain") || tgt.contains("level") || tgt.contains("amp")) &&
                         (item.model.id.containsIgnoreCase("vol") || item.model.label.containsIgnoreCase("vol") ||
                          item.model.id.containsIgnoreCase("gain") || item.model.label.containsIgnoreCase("gain")))
                {
                    matches = true;
                }
                else if (tgt.contains("pan") && (item.model.id.containsIgnoreCase("pan") || item.model.label.containsIgnoreCase("pan")))
                {
                    matches = true;
                }
            }

            if (matches)
            {
                double range = std::max(0.0001, item.model.maxValue - item.model.minValue);
                double offset = lfoOut * range * 0.4;
                item.control->setVisualModulationOffset(offset);

                // Modulate the parameter dynamically
                double modulatedVal = juce::jlimit(item.model.minValue, item.model.maxValue, item.model.currentValue + offset);
                applyControlBindings(item.model, modulatedVal);
            }
        }
    }
}

} // namespace openwav
