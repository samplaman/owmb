#include "DecentSamplerCanvasComponent.h"
#include "OpenWavLookAndFeel.h"

namespace openwav
{

// ── Decent Sampler Font Helper (Clean Geometric Modern Typography, Fast Cached) ──
static const juce::String& getDecentSamplerFontFamily()
{
    static const juce::String chosen = []() {
        static const juce::StringArray preferredFamilies = {
            "Helvetica Neue", "Helvetica", "Arial", "Inter", "Roboto", "sans-serif"
        };
        auto allTypefaces = juce::Font::findAllTypefaceNames();
        for (const auto& fam : preferredFamilies)
        {
            if (allTypefaces.contains(fam, true))
                return fam;
        }
        return juce::String("sans-serif");
    }();
    return chosen;
}

static juce::Font getDecentSamplerFont(float height, bool isBold = false)
{
    auto options = juce::FontOptions(getDecentSamplerFontFamily(), height, juce::Font::plain)
                       .withStyle(isBold ? "Medium" : "Light")
                       .withKerningFactor(0.02f);
    return juce::Font(options);
}

// ── ADSR Binding Helper Functions ───────────────────────────
static bool isAttackBindingParam(const juce::String& p)
{
    juce::String s = p.trim();
    return s.equalsIgnoreCase("A") || s.equalsIgnoreCase("att") || s.equalsIgnoreCase("attack") ||
           s.containsIgnoreCase("attack") || s.containsIgnoreCase("ENV_ATTACK") || s.containsIgnoreCase("AMP_ATTACK");
}

static bool isDecayBindingParam(const juce::String& p)
{
    juce::String s = p.trim();
    return s.equalsIgnoreCase("D") || s.equalsIgnoreCase("dec") || s.equalsIgnoreCase("decay") ||
           s.containsIgnoreCase("decay") || s.containsIgnoreCase("ENV_DECAY") || s.containsIgnoreCase("AMP_DECAY");
}

static bool isSustainBindingParam(const juce::String& p)
{
    juce::String s = p.trim();
    return s.equalsIgnoreCase("S") || s.equalsIgnoreCase("sus") || s.equalsIgnoreCase("sustain") ||
           s.containsIgnoreCase("sustain") || s.containsIgnoreCase("ENV_SUSTAIN") || s.containsIgnoreCase("AMP_SUSTAIN");
}

static bool isReleaseBindingParam(const juce::String& p)
{
    juce::String s = p.trim();
    return s.equalsIgnoreCase("R") || s.equalsIgnoreCase("rel") || s.equalsIgnoreCase("release") ||
           s.containsIgnoreCase("release") || s.containsIgnoreCase("ENV_RELEASE") || s.containsIgnoreCase("AMP_RELEASE");
}

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

// ── DecentSamplerCanvasLookAndFeel Implementation ───────────
DecentSamplerCanvasLookAndFeel::DecentSamplerCanvasLookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xFF1B1D22));
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xFF1E2128));
    setColour(juce::PopupMenu::textColourId, juce::Colour(0xFFE8E8E8));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xFF333844));
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFF242730));
    setColour(juce::ComboBox::textColourId, juce::Colours::white);
    setColour(juce::ComboBox::outlineColourId, juce::Colour(0xFF3E4350));
    setColour(juce::ComboBox::arrowColourId, juce::Colour(0xFFAAAAAA));
    setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF2A2D35));
    setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFF4A90E2));
    setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    setColour(juce::TextButton::textColourOnId, juce::Colours::white);
}

void DecentSamplerCanvasLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                                                          bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    auto baseCol = backgroundColour;

    if (shouldDrawButtonAsDown)
        baseCol = baseCol.darker(0.15f);
    else if (shouldDrawButtonAsHighlighted)
        baseCol = baseCol.brighter(0.12f);

    g.setColour(baseCol);
    g.fillRoundedRectangle(bounds, 3.0f);

    g.setColour(baseCol.brighter(0.2f).withAlpha(0.6f));
    g.drawRoundedRectangle(bounds, 3.0f, 1.0f);
}

void DecentSamplerCanvasLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                                    bool /*shouldDrawButtonAsHighlighted*/, bool /*shouldDrawButtonAsDown*/)
{
    juce::Font font(getTextButtonFont(button, button.getHeight()));
    g.setFont(font);
    g.setColour(button.findColour(button.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId)
                      .withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.5f));

    const int yIndent = juce::jmin(4, button.proportionOfHeight(0.3f));
    const int cornerSize = juce::jmin(button.getHeight(), button.getWidth()) / 2;

    const int fontHeight = juce::roundToInt(font.getHeight() * 0.6f);
    const int leftIndent = juce::jmin(fontHeight, 2 + cornerSize / (button.isConnectedOnLeft() ? 4 : 2));
    const int rightIndent = juce::jmin(fontHeight, 2 + cornerSize / (button.isConnectedOnRight() ? 4 : 2));
    const int textWidth = button.getWidth() - leftIndent - rightIndent;

    if (textWidth > 0)
        g.drawFittedText(button.getButtonText(),
                         leftIndent, yIndent, textWidth, button.getHeight() - yIndent * 2,
                         juce::Justification::centred, 2);
}

void DecentSamplerCanvasLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                                                  int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox& box)
{
    auto cornerSize = 3.0f;
    auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat().reduced(0.5f);

    auto bg = box.findColour(juce::ComboBox::backgroundColourId);
    if (isButtonDown) bg = bg.darker(0.1f);
    g.setColour(bg);
    g.fillRoundedRectangle(bounds, cornerSize);

    g.setColour(box.findColour(juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle(bounds, cornerSize, 1.0f);

    // Arrow
    auto arrowZone = juce::Rectangle<float>(buttonX, buttonY, buttonW, buttonH).reduced(buttonW * 0.3f, buttonH * 0.35f);
    juce::Path path;
    path.startNewSubPath(arrowZone.getX(), arrowZone.getY());
    path.lineTo(arrowZone.getCentreX(), arrowZone.getBottom());
    path.lineTo(arrowZone.getRight(), arrowZone.getY());
    path.closeSubPath();

    g.setColour(box.findColour(juce::ComboBox::arrowColourId));
    g.fillPath(path);
}

void DecentSamplerCanvasLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
    g.fillAll(findColour(juce::PopupMenu::backgroundColourId));
    g.setColour(juce::Colour(0x35FFFFFF));
    g.drawRect(0, 0, width, height, 1);
}

void DecentSamplerCanvasLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                                                       bool isSeparator, bool isActive, bool isHighlighted, bool isTicked,
                                                       bool hasSubMenu, const juce::String& text, const juce::String& shortcutKeyText,
                                                       const juce::Drawable* icon, const juce::Colour* textColour)
{
    if (isSeparator)
    {
        auto r = area.reduced(5, 0);
        r.removeFromTop(juce::roundToInt(((float) r.getHeight() * 0.5f) - 0.5f));
        g.setColour(findColour(juce::PopupMenu::textColourId).withAlpha(0.2f));
        g.fillRect(r.removeFromTop(1));
    }
    else
    {
        auto textCol = (textColour != nullptr) ? *textColour : findColour(juce::PopupMenu::textColourId);

        if (isHighlighted && isActive)
        {
            g.setColour(findColour(juce::PopupMenu::highlightedBackgroundColourId));
            g.fillRect(area.reduced(2, 1));
            textCol = findColour(juce::PopupMenu::highlightedTextColourId);
        }

        g.setColour(textCol.withMultipliedAlpha(isActive ? 1.0f : 0.45f));
        auto font = getPopupMenuFont();
        g.setFont(font);

        auto r = area.reduced(8, 0);
        if (icon != nullptr)
        {
            icon->drawWithin(g, r.removeFromLeft(font.getHeight()).toFloat(), juce::RectanglePlacement::centred, 1.0f);
            r.removeFromLeft(4);
        }

        g.drawFittedText(text, r, juce::Justification::centredLeft, 1);
    }
}

juce::Font DecentSamplerCanvasLookAndFeel::getTextButtonFont(juce::TextButton&, int buttonHeight)
{
    return getDecentSamplerFont(std::max(10.0f, buttonHeight * 0.42f), true);
}

juce::Font DecentSamplerCanvasLookAndFeel::getComboBoxFont(juce::ComboBox&)
{
    return getDecentSamplerFont(12.0f, false);
}

juce::Font DecentSamplerCanvasLookAndFeel::getLabelFont(juce::Label&)
{
    return getDecentSamplerFont(12.0f, false);
}

juce::Font DecentSamplerCanvasLookAndFeel::getPopupMenuFont()
{
    return getDecentSamplerFont(13.0f, false);
}

// ── DecentSamplerButtonComponent Implementation ─────────────
DecentSamplerButtonComponent::DecentSamplerButtonComponent(const DecentSamplerUiButton& m, std::function<juce::File(const juce::String&)> resolver)
    : juce::Button(m.text), model(m), fileResolver(std::move(resolver))
{
    setRepaintsOnMouseActivity(true);
    loadImages();
}

void DecentSamplerButtonComponent::setFileResolver(std::function<juce::File(const juce::String&)> resolver)
{
    fileResolver = std::move(resolver);
    loadImages();
    repaint();
}

void DecentSamplerButtonComponent::setModel(const DecentSamplerUiButton& m)
{
    model = m;
    setButtonText(model.text);
    loadImages();
    repaint();
}

juce::Image DecentSamplerButtonComponent::loadImageFromPath(const juce::String& rawPath, const juce::String& resolvedPath)
{
    if (resolvedPath.isNotEmpty())
    {
        juce::File f(resolvedPath);
        if (f.existsAsFile())
        {
            auto img = juce::ImageFileFormat::loadFrom(f);
            if (img.isValid()) return img;
        }
    }

    if (rawPath.isNotEmpty())
    {
        juce::File direct(rawPath);
        if (direct.existsAsFile())
        {
            auto img = juce::ImageFileFormat::loadFrom(direct);
            if (img.isValid()) return img;
        }

        if (fileResolver)
        {
            juce::File rf = fileResolver(rawPath);
            if (rf.existsAsFile())
            {
                auto img = juce::ImageFileFormat::loadFrom(rf);
                if (img.isValid()) return img;
            }
        }
    }
    return {};
}

void DecentSamplerButtonComponent::loadImages()
{
    defaultMainImg = loadImageFromPath(model.mainImage, model.resolvedMainImagePath);
    defaultHoverImg = loadImageFromPath(model.hoverImage, model.resolvedHoverImagePath);
    defaultClickImg = loadImageFromPath(model.clickImage, model.resolvedClickImagePath);

    stateImages.clear();
    stateImages.resize(model.states.size());
    for (size_t i = 0; i < model.states.size(); ++i)
    {
        const auto& st = model.states[i];
        stateImages[i].mainImg = loadImageFromPath(st.mainImage, st.resolvedMainImagePath);
        stateImages[i].hoverImg = loadImageFromPath(st.hoverImage, st.resolvedHoverImagePath);
        stateImages[i].clickImg = loadImageFromPath(st.clickImage, st.resolvedClickImagePath);
    }
}

bool DecentSamplerButtonComponent::hasImages() const
{
    if (defaultMainImg.isValid() || defaultHoverImg.isValid() || defaultClickImg.isValid())
        return true;
    for (const auto& s : stateImages)
    {
        if (s.mainImg.isValid() || s.hoverImg.isValid() || s.clickImg.isValid())
            return true;
    }
    return false;
}

void DecentSamplerButtonComponent::paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    auto bounds = getLocalBounds().toFloat();
    if (bounds.isEmpty()) return;

    bool isToggled = getToggleState();
    int activeStateIdx = -1;
    if (!model.states.empty())
    {
        activeStateIdx = isToggled ? std::min(1, static_cast<int>(model.states.size() - 1)) : 0;
    }

    juce::Image imgToDraw;
    if (activeStateIdx >= 0 && activeStateIdx < static_cast<int>(stateImages.size()))
    {
        const auto& sImgs = stateImages[activeStateIdx];
        if (shouldDrawButtonAsDown && sImgs.clickImg.isValid())
            imgToDraw = sImgs.clickImg;
        else if (shouldDrawButtonAsHighlighted && sImgs.hoverImg.isValid())
            imgToDraw = sImgs.hoverImg;
        else if (sImgs.mainImg.isValid())
            imgToDraw = sImgs.mainImg;
    }

    if (!imgToDraw.isValid())
    {
        if (shouldDrawButtonAsDown && defaultClickImg.isValid())
            imgToDraw = defaultClickImg;
        else if (shouldDrawButtonAsHighlighted && defaultHoverImg.isValid())
            imgToDraw = defaultHoverImg;
        else if (defaultMainImg.isValid())
            imgToDraw = defaultMainImg;
    }

    juce::Colour btnTextCol = DecentSamplerControlComponent::parseDecentSamplerColor(model.textColorHex, juce::Colours::white);
    juce::Colour btnBgCol = DecentSamplerControlComponent::parseDecentSamplerColor(model.bgColorHex, juce::Colour(0xFF262930));
    juce::Colour btnActiveCol = DecentSamplerControlComponent::parseDecentSamplerColor(model.trackForegroundColorHex, juce::Colour(0xFF4A90E2));

    if (imgToDraw.isValid())
    {
        g.drawImage(imgToDraw, bounds, juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);

        bool hasSpecificClickImg = (defaultClickImg.isValid() || (activeStateIdx >= 0 && stateImages[activeStateIdx].clickImg.isValid()));
        bool hasSpecificHoverImg = (defaultHoverImg.isValid() || (activeStateIdx >= 0 && stateImages[activeStateIdx].hoverImg.isValid()));

        if (shouldDrawButtonAsDown && !hasSpecificClickImg)
        {
            g.setColour(juce::Colours::black.withAlpha(0.25f));
            g.fillRect(bounds);
        }
        else if (shouldDrawButtonAsHighlighted && !hasSpecificHoverImg)
        {
            g.setColour(juce::Colours::white.withAlpha(0.12f));
            g.fillRect(bounds);
        }

        if (model.text.isNotEmpty())
        {
            g.setColour(btnTextCol);
            g.setFont(getDecentSamplerFont(model.textSize > 0 ? model.textSize : 11.0f, true));
            g.drawFittedText(model.text, bounds.toNearestInt(), juce::Justification::centred, 1);
        }
    }
    else
    {
        auto bg = isToggled ? btnActiveCol : (shouldDrawButtonAsHighlighted ? btnBgCol.brighter(0.1f) : btnBgCol);
        if (shouldDrawButtonAsDown)
            bg = bg.darker(0.15f);

        g.setColour(bg);
        g.fillRoundedRectangle(bounds, 4.0f);

        g.setColour(isToggled ? juce::Colours::white.withAlpha(0.4f) : juce::Colours::white.withAlpha(0.15f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.0f);

        juce::String textToDraw = model.text;
        if (textToDraw.isEmpty() && activeStateIdx >= 0 && activeStateIdx < static_cast<int>(model.states.size()))
            textToDraw = model.states[activeStateIdx].name;
        if (textToDraw.isEmpty())
            textToDraw = "Button";

        g.setColour(isToggled ? juce::Colours::white : btnTextCol);
        g.setFont(getDecentSamplerFont(model.textSize > 0 ? model.textSize : 11.0f, false));
        g.drawFittedText(textToDraw, bounds.toNearestInt().reduced(4, 2), juce::Justification::centred, 1);
    }
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

    // 1. Try explicit resolved path if valid
    juce::String path = model.resolvedCustomSkinImagePath;
    if (path.isNotEmpty())
    {
        filmstrip = getOrCreateFilmstrip(path, model.customSkinNumFrames);
        if (filmstrip != nullptr) return;
    }

    // 2. Try raw skin path directly if valid file
    if (model.customSkinImagePath.isNotEmpty())
    {
        juce::File directF(model.customSkinImagePath);
        if (directF.existsAsFile())
        {
            filmstrip = getOrCreateFilmstrip(directF.getFullPathName(), model.customSkinNumFrames);
            if (filmstrip != nullptr) return;
        }

        // 3. Search relative to resolvedCustomSkinImagePath's directory
        if (model.resolvedCustomSkinImagePath.isNotEmpty())
        {
            juce::File parent = juce::File(model.resolvedCustomSkinImagePath).getParentDirectory();
            juce::File alt = SampleMapState::resolveDecentSamplerSamplePath(model.customSkinImagePath, parent);
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
    // Default to Decent Sampler native cyan/blue (#3B92F2), subtle translucent track (#35FFFFFF), and clean white text
    fgColor = parseDecentSamplerColor(model.trackColorHex, juce::Colour(0xFF3B92F2));
    bgColor = parseDecentSamplerColor(model.trackBackgroundColorHex, juce::Colour(0x38FFFFFF));
    textColor = parseDecentSamplerColor(model.textColorHex, juce::Colour(0xFFFFFFFF));
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
    if (onDragStateChanged)
        onDragStateChanged(this, true);
    repaint();
}

void DecentSamplerControlComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (!isDragging)
    {
        isDragging = true;
        if (onDragStateChanged)
            onDragStateChanged(this, true);
    }

    bool isHorizontal = model.type.containsIgnoreCase("horizontal") || model.style.containsIgnoreCase("horizontal");
    int delta = isHorizontal ? (e.getPosition().getX() - dragStartX) : (dragStartY - e.getPosition().getY());

    double range = std::max(0.0001, model.maxValue - model.minValue);
    double sensitivity = 150.0;
    if (e.mods.isShiftDown())
        sensitivity = 600.0;

    double deltaVal = (delta / sensitivity) * range;
    setValue(dragStartVal + deltaVal, true);
}

void DecentSamplerControlComponent::mouseUp(const juce::MouseEvent&)
{
    if (isDragging)
    {
        isDragging = false;
        if (onDragStateChanged)
            onDragStateChanged(this, false);
        repaint();
    }
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
    if (isDragging)
    {
        isDragging = false;
        if (onDragStateChanged)
            onDragStateChanged(this, false);
    }
    repaint();
}

void DecentSamplerControlComponent::resized()
{
}

void DecentSamplerControlComponent::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    if (area.getWidth() < 4.0f || area.getHeight() < 4.0f)
        return;

    bool isLinear = model.type.containsIgnoreCase("linear") || model.style.containsIgnoreCase("linear") ||
                    model.type.containsIgnoreCase("vertical") || model.type.containsIgnoreCase("horizontal") ||
                    model.type.containsIgnoreCase("slider");
    bool isHorizontal = model.type.containsIgnoreCase("horizontal") || model.style.containsIgnoreCase("horizontal");

    bool hasLabel = model.label.isNotEmpty();
    float fontSize = (model.textSize > 0.0f ? model.textSize : 10.0f);
    float labelH_pt = hasLabel ? (fontSize * 1.30f) : 0.0f;
    float labelH = labelH_pt * scale;

    // 1. Draw Top Label (Font scaled strictly by font size property)
    if (hasLabel)
    {
        auto labelRect = area.removeFromTop(labelH);
        area.removeFromTop(2.0f * scale);
        g.setColour(textColor.withAlpha(0.90f));
        g.setFont(getDecentSamplerFont(fontSize * scale, false));
        g.drawText(model.label, labelRect, juce::Justification::centred, true);
    }

    double range = std::max(0.0001, model.maxValue - model.minValue);
    double effectiveVal = juce::jlimit(model.minValue, model.maxValue, model.currentValue + visualModOffset);
    float normVal = static_cast<float>(juce::jlimit(0.0, 1.0, (effectiveVal - model.minValue) / range));

    if (isLinear)
    {
        // ── Linear Slider Rendering ────────────────────────
        if (isHorizontal)
        {
            float trackH = std::max(3.5f * scale, area.getHeight() * 0.12f);
            float trackY = area.getCentreY() - trackH * 0.5f;
            float trackX = area.getX() + 4.0f * scale;
            float trackW = std::max(10.0f * scale, area.getWidth() - 8.0f * scale);
            auto trackRect = juce::Rectangle<float>(trackX, trackY, trackW, trackH);

            // Background Track
            g.setColour(bgColor);
            g.fillRoundedRectangle(trackRect, trackH * 0.5f);

            // Active Track
            auto activeRect = trackRect.withWidth(trackRect.getWidth() * normVal);
            g.setColour(fgColor);
            g.fillRoundedRectangle(activeRect, trackH * 0.5f);

            // Thumb
            float thumbW = std::max(8.0f * scale, std::min(area.getWidth() * 0.18f, 15.0f * scale));
            float thumbH = std::max(11.0f * scale, area.getHeight() * 0.62f);
            float thumbX = trackRect.getX() + normVal * trackRect.getWidth() - thumbW * 0.5f;
            auto thumbRect = juce::Rectangle<float>(thumbX, trackRect.getCentreY() - thumbH * 0.5f, thumbW, thumbH);

            juce::ColourGradient thumbGrad(juce::Colour(0xFF383C44), thumbRect.getX(), thumbRect.getY(),
                                           juce::Colour(0xFF1E2024), thumbRect.getX(), thumbRect.getBottom(), false);
            g.setGradientFill(thumbGrad);
            g.fillRoundedRectangle(thumbRect, 2.5f * scale);
            g.setColour(fgColor);
            g.drawRoundedRectangle(thumbRect, 2.5f * scale, 1.0f * scale);
            g.drawLine(thumbRect.getCentreX(), thumbRect.getY() + 3.0f * scale, thumbRect.getCentreX(), thumbRect.getBottom() - 3.0f * scale, 1.2f * scale);
        }
        else
        {
            // Vertical Slider
            float trackW = std::max(3.5f * scale, area.getWidth() * 0.12f);
            float trackX = area.getCentreX() - trackW * 0.5f;
            float trackY = area.getY() + 4.0f * scale;
            float trackH = std::max(10.0f * scale, area.getHeight() - 8.0f * scale);
            auto trackRect = juce::Rectangle<float>(trackX, trackY, trackW, trackH);

            // Background Track
            g.setColour(bgColor);
            g.fillRoundedRectangle(trackRect, trackW * 0.5f);

            // Active Track
            float fillH = trackRect.getHeight() * normVal;
            auto activeRect = juce::Rectangle<float>(trackRect.getX(), trackRect.getBottom() - fillH, trackW, fillH);
            g.setColour(fgColor);
            g.fillRoundedRectangle(activeRect, trackW * 0.5f);

            // Thumb
            float thumbW = std::max(11.0f * scale, area.getWidth() * 0.62f);
            float thumbH = std::max(8.0f * scale, std::min(area.getHeight() * 0.18f, 15.0f * scale));
            float thumbY = trackRect.getBottom() - normVal * trackRect.getHeight() - thumbH * 0.5f;
            auto thumbRect = juce::Rectangle<float>(trackRect.getCentreX() - thumbW * 0.5f, thumbY, thumbW, thumbH);

            juce::ColourGradient thumbGrad(juce::Colour(0xFF383C44), thumbRect.getX(), thumbRect.getY(),
                                           juce::Colour(0xFF1E2024), thumbRect.getX(), thumbRect.getBottom(), false);
            g.setGradientFill(thumbGrad);
            g.fillRoundedRectangle(thumbRect, 2.5f * scale);
            g.setColour(fgColor);
            g.drawRoundedRectangle(thumbRect, 2.5f * scale, 1.0f * scale);
            g.drawLine(thumbRect.getX() + 3.0f * scale, thumbRect.getCentreY(), thumbRect.getRight() - 3.0f * scale, thumbRect.getCentreY(), 1.2f * scale);
        }
    }
    else
    {
        // ── Rotary Knob / Custom Filmstrip Rendering ───────
        float dialD = std::min(area.getWidth(), area.getHeight());
        if (dialD < 6.0f) return;

        // Inner dial margin padding within knob bounds (6pt in points)
        float knobPadding = 6.0f * scale;
        auto dialBounds = area.withSizeKeepingCentre(dialD, dialD).reduced(knobPadding);
        if (dialBounds.getWidth() < 4.0f || dialBounds.getHeight() < 4.0f)
            dialBounds = area.withSizeKeepingCentre(dialD, dialD).reduced(2.0f * scale);

        if (filmstrip != nullptr && !filmstrip->frames.empty())
        {
            // ── Pre-cached Fast Filmstrip Frame Blitting ──
            int numFrames = static_cast<int>(filmstrip->frames.size());
            int frameIdx = juce::jlimit(0, numFrames - 1, static_cast<int>(std::round(normVal * (numFrames - 1))));

            const auto& frameImg = filmstrip->frames[frameIdx];

            float frameAspect = static_cast<float>(filmstrip->frameW) / static_cast<float>(std::max(1, filmstrip->frameH));
            float curAspect = dialBounds.getWidth() / std::max(1.0f, dialBounds.getHeight());
            juce::Rectangle<float> drawBounds;
            if (curAspect > frameAspect)
            {
                float w = dialBounds.getHeight() * frameAspect;
                drawBounds = dialBounds.withSizeKeepingCentre(w, dialBounds.getHeight());
            }
            else
            {
                float h = dialBounds.getWidth() / frameAspect;
                drawBounds = dialBounds.withSizeKeepingCentre(dialBounds.getWidth(), h);
            }

            g.drawImage(frameImg, drawBounds, juce::RectanglePlacement::fillDestination);
        }
        else
        {
            // ── Authentic Decent Sampler 270-Degree Arc Ring Knob ──
            float cx = dialBounds.getCentreX();
            float cy = dialBounds.getCentreY();

            // Distinct balanced stroke thickness (~9.8% of dial diameter or ~5.0pt)
            float trackThickness = std::max(3.5f * scale, dialBounds.getWidth() * 0.098f);

            // Decent Sampler 270-degree sweep (from -135 deg / 7:30 bottom-left to +135 deg / 4:30 bottom-right)
            float startAngle = -juce::MathConstants<float>::pi * 0.75f; // -135 deg (bottom-left at 7:30)
            float endAngle   =  juce::MathConstants<float>::pi * 0.75f; // +135 deg (bottom-right at 4:30)
            float currentAngle = startAngle + normVal * (endAngle - startAngle);

            float arcRadius = (dialBounds.getWidth() - trackThickness) * 0.5f;
            if (arcRadius < 1.0f) arcRadius = 1.0f;

            // 1. Background Arc Track (Translucent ring with rounded end caps)
            juce::Path bgArc;
            bgArc.addCentredArc(cx, cy, arcRadius, arcRadius,
                                0.0f, startAngle, endAngle, true);
            g.setColour(bgColor);
            g.strokePath(bgArc, juce::PathStrokeType(trackThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // 2. Active Foreground Arc Track (Vibrant accent ring with rounded end caps)
            if (normVal > 0.002f)
            {
                juce::Path fgArc;
                fgArc.addCentredArc(cx, cy, arcRadius, arcRadius,
                                    0.0f, startAngle, currentAngle, true);
                g.setColour(isHovered || isDragging ? fgColor.brighter(0.15f) : fgColor);
                g.strokePath(fgArc, juce::PathStrokeType(trackThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
        }
    }
}

// ── DecentSamplerCanvasComponent Implementation ─────────────
DecentSamplerCanvasComponent::DecentSamplerCanvasComponent(AudioEngine& engine)
    : audioEngine(engine)
{
    setOpaque(true);
    setLookAndFeel(&canvasLookAndFeel);
    audioEngine.getKeyboardState().addListener(this);
    startTimerHz(30);
}

DecentSamplerCanvasComponent::~DecentSamplerCanvasComponent()
{
    stopTimer();
    audioEngine.getKeyboardState().removeListener(this);
    setLookAndFeel(nullptr);
}

float DecentSamplerCanvasComponent::getBaseWidth() const
{
    if (currentState.customUi.width > 0)
        return static_cast<float>(currentState.customUi.width);
    return 812.0f;
}

float DecentSamplerCanvasComponent::getBaseHeight() const
{
    if (currentState.customUi.height > 0)
        return static_cast<float>(currentState.customUi.height);
    return 375.0f;
}

juce::Rectangle<float> DecentSamplerCanvasComponent::getTotalFrameBounds() const
{
    // Upper canvas area above the fixed 105px keyboard sizer
    auto area = getLocalBounds().toFloat();
    area.removeFromBottom(kKeyboardHeight);
    area = area.reduced(4.0f);

    float baseW = getBaseWidth();
    float baseH = getBaseHeight();
    float targetRatio = baseW / std::max(1.0f, baseH);
    float currentRatio = area.getWidth() / std::max(1.0f, area.getHeight());

    if (currentRatio > targetRatio)
        return area.withSizeKeepingCentre(area.getHeight() * targetRatio, area.getHeight());
    else
        return area.withSizeKeepingCentre(area.getWidth(), area.getWidth() / targetRatio);
}

float DecentSamplerCanvasComponent::getCanvasScale() const
{
    auto frame = getTotalFrameBounds();
    float baseW = getBaseWidth();
    return baseW > 0.0f ? (frame.getWidth() / baseW) : 1.0f;
}

juce::Rectangle<float> DecentSamplerCanvasComponent::getHeaderBounds() const
{
    auto frame = getTotalFrameBounds();
    float scale = getCanvasScale();
    return frame.removeFromTop(kHeaderHeight * scale);
}

juce::Rectangle<float> DecentSamplerCanvasComponent::getKeyboardBounds() const
{
    // Fixed sizer at bottom of component: 100% width, constant 105.0f height, never scales
    return getLocalBounds().toFloat().removeFromBottom(kKeyboardHeight);
}

juce::Rectangle<float> DecentSamplerCanvasComponent::getCanvasBounds() const
{
    auto frame = getTotalFrameBounds();
    float scale = getCanvasScale();
    frame.removeFromTop(kHeaderHeight * scale);
    return frame;
}

// All 75 white keys from MIDI 0 to 127
static const int kAllWhiteKeys[75] = {
    0, 2, 4, 5, 7, 9, 11,       // C-2 (0..11) - idx 0..6
    12, 14, 16, 17, 19, 21, 23, // C-1 (12..23) - idx 7..13 (note 21=A0, 23=B0)
    24, 26, 28, 29, 31, 33, 35, // C0 (24..35) - idx 14..20
    36, 38, 40, 41, 43, 45, 47, // C1 (36..47) - idx 21..27
    48, 50, 52, 53, 55, 57, 59, // C2 (48..59) - idx 28..34
    60, 62, 64, 65, 67, 69, 71, // C3 (60..71) - idx 35..41 (Middle C)
    72, 74, 76, 77, 79, 81, 83, // C4 (72..83) - idx 42..48
    84, 86, 88, 89, 91, 93, 95, // C5 (84..95) - idx 49..55
    96, 98, 100, 101, 103, 105, 107, // C6 (96..107) - idx 56..62
    108, 110, 112, 113, 115, 117, 119, // C7 (108..119) - idx 63..69
    120, 122, 124, 125, 127     // C8 (120..127) - idx 70..74
};

int DecentSamplerCanvasComponent::noteNumberAtKeyboardPos(juce::Point<float> pos, juce::Rectangle<float> kbRect) const
{
    auto keybedArea = kbRect;
    if (!keybedArea.contains(pos)) return -1;

    float wkW = keybedArea.getWidth() / static_cast<float>(kNumVisibleWhiteKeys);
    float bkW = wkW * 0.65f;
    float bkH = keybedArea.getHeight() * 0.60f;

    // 1. Check black keys first (top 60% of keybed)
    if (pos.y < keybedArea.getY() + bkH)
    {
        for (int i = 0; i < kNumVisibleWhiteKeys - 1; ++i)
        {
            int note1 = kAllWhiteKeys[startWhiteKeyIndex + i];
            int note2 = kAllWhiteKeys[startWhiteKeyIndex + i + 1];
            if (note2 - note1 == 2) // There is a black key in between
            {
                int blackNote = note1 + 1;
                float dividerX = keybedArea.getX() + (i + 1) * wkW;
                if (pos.x >= dividerX - bkW * 0.5f && pos.x <= dividerX + bkW * 0.5f)
                    return blackNote;
            }
        }
    }

    // 2. White keys
    int wkIdx = static_cast<int>((pos.x - keybedArea.getX()) / wkW);
    wkIdx = juce::jlimit(0, kNumVisibleWhiteKeys - 1, wkIdx);
    return kAllWhiteKeys[startWhiteKeyIndex + wkIdx];
}

void DecentSamplerCanvasComponent::paintHeader(juce::Graphics& g, juce::Rectangle<float> headerRect)
{
    if (headerRect.isEmpty()) return;

    // Sleek display header bar overlay
    juce::ColourGradient grad(juce::Colour(0xCC14171E), headerRect.getX(), headerRect.getY(),
                              juce::Colour(0x990C0E12), headerRect.getX(), headerRect.getBottom(), false);
    g.setGradientFill(grad);
    g.fillRect(headerRect);

    // Subtle bottom divider line
    g.setColour(juce::Colour(0x30FFFFFF));
    g.drawHorizontalLine(static_cast<int>(headerRect.getBottom()), headerRect.getX(), headerRect.getRight());
}

void DecentSamplerCanvasComponent::paintFooter(juce::Graphics& g, juce::Rectangle<float> footerRect)
{
    if (footerRect.isEmpty()) return;

    // Sleek transparent footer bar overlay in exact same style as header
    juce::ColourGradient grad(juce::Colour(0x990C0E12), footerRect.getX(), footerRect.getY(),
                              juce::Colour(0xCC14171E), footerRect.getX(), footerRect.getBottom(), false);
    g.setGradientFill(grad);
    g.fillRect(footerRect);

    // Subtle top divider line matching header divider
    g.setColour(juce::Colour(0x30FFFFFF));
    g.drawHorizontalLine(static_cast<int>(footerRect.getY()), footerRect.getX(), footerRect.getRight());

    // Paint transparent interactive keyboard layout spanning full width
    paintKeyboard(g, footerRect);
}

void DecentSamplerCanvasComponent::paintKeyboard(juce::Graphics& g, juce::Rectangle<float> kbRect)
{
    auto keybedArea = kbRect;
    float wkW = keybedArea.getWidth() / static_cast<float>(kNumVisibleWhiteKeys);
    float bkW = wkW * 0.65f;
    float bkH = keybedArea.getHeight() * 0.60f;

    auto getKeyColorForNote = [&](int noteNum) -> juce::Colour {
        auto it = currentState.keyColorsByNote.find(noteNum);
        if (it != currentState.keyColorsByNote.end() && it->second.isNotEmpty())
            return DecentSamplerControlComponent::parseDecentSamplerColor(it->second, juce::Colour());

        for (const auto& r : currentState.keyboardColorRanges)
        {
            if (noteNum >= r.startNote && noteNum <= r.endNote && r.colorHex.isNotEmpty())
                return DecentSamplerControlComponent::parseDecentSamplerColor(r.colorHex, juce::Colour());
        }

        if (currentState.keyboardDefaultKeyColorHex.isNotEmpty())
            return DecentSamplerControlComponent::parseDecentSamplerColor(currentState.keyboardDefaultKeyColorHex, juce::Colour());

        return juce::Colour();
    };

    // 1. White Keys (Full Width Semi-transparent Frosted Glass with Key Colors & Range & Active MIDI glow)
    for (int i = 0; i < kNumVisibleWhiteKeys; ++i)
    {
        int note = kAllWhiteKeys[startWhiteKeyIndex + i];
        float kx = keybedArea.getX() + i * wkW;
        juce::Rectangle<float> wkRect(kx, keybedArea.getY(), wkW, keybedArea.getHeight());

        bool isPressed = (note == keyboardAuditionNote) || (note >= 0 && note < 128 && audioEngine.getKeyboardState().isNoteOnForChannels(0xFFFF, note));
        bool inRange = (note >= currentState.keyboardLowPlayableNote && note <= currentState.keyboardHighPlayableNote);
        auto keyColor = getKeyColorForNote(note);
        bool hasCustomColor = keyColor.isOpaque() || keyColor.getAlpha() > 0;

        if (isPressed)
        {
            // Illuminated active key with color
            juce::Colour activeBase = hasCustomColor ? keyColor : juce::Colour(0xFF3B92F2);
            juce::ColourGradient pressGrad(activeBase.brighter(0.35f), wkRect.getX(), wkRect.getY(),
                                           activeBase.withAlpha(0.92f), wkRect.getX(), wkRect.getBottom(), false);
            g.setGradientFill(pressGrad);
            g.fillRect(wkRect);

            // Bright top reflection highlight
            g.setColour(juce::Colours::white.withAlpha(0.7f));
            g.fillRect(juce::Rectangle<float>(wkRect.getX() + 0.5f, wkRect.getY(), wkRect.getWidth() - 1.0f, 2.5f));
        }
        else
        {
            if (hasCustomColor)
            {
                // Key tinted with preset color
                g.setColour(keyColor.withAlpha(0.26f));
                g.fillRect(wkRect);

                // Distinct colored bottom indicator strip
                g.setColour(keyColor.withAlpha(0.88f));
                g.fillRect(juce::Rectangle<float>(wkRect.getX() + 0.5f, wkRect.getBottom() - 3.5f, wkRect.getWidth() - 1.0f, 3.0f));
            }
            else if (inRange)
            {
                // Playable key frosted white glass
                g.setColour(juce::Colour(0x18FFFFFF));
                g.fillRect(wkRect);
            }
            else
            {
                // Outside playable range: muted translucent
                g.setColour(juce::Colour(0x06FFFFFF));
                g.fillRect(wkRect);
            }
        }

        // Subtle divider line between white keys
        g.setColour(isPressed ? juce::Colour(0x60FFFFFF) : (inRange ? juce::Colour(0x20FFFFFF) : juce::Colour(0x10FFFFFF)));
        g.drawRect(wkRect, 0.5f);

        // Draw Octave label on bottom of C keys (C3 = Middle C / MIDI 60)
        if (note % 12 == 0)
        {
            g.setColour(isPressed ? juce::Colours::white : (inRange ? juce::Colour(0x90FFFFFF) : juce::Colour(0x40FFFFFF)));
            g.setFont(getDecentSamplerFont(10.0f, isPressed));
            g.drawText("C" + juce::String((note / 12) - 2), wkRect.removeFromBottom(18.0f), juce::Justification::centred, false);
        }
    }

    // 2. Black Keys (Full Width Semi-transparent Dark Glass with Key Colors & Range & Active MIDI glow)
    for (int i = 0; i < kNumVisibleWhiteKeys - 1; ++i)
    {
        int note1 = kAllWhiteKeys[startWhiteKeyIndex + i];
        int note2 = kAllWhiteKeys[startWhiteKeyIndex + i + 1];
        if (note2 - note1 == 2)
        {
            int blackNote = note1 + 1;
            float dividerX = keybedArea.getX() + (i + 1) * wkW;
            auto bkRect = juce::Rectangle<float>(dividerX - bkW * 0.5f, keybedArea.getY(), bkW, bkH);

            bool isPressed = (blackNote == keyboardAuditionNote) || (blackNote >= 0 && blackNote < 128 && audioEngine.getKeyboardState().isNoteOnForChannels(0xFFFF, blackNote));
            bool inRange = (blackNote >= currentState.keyboardLowPlayableNote && blackNote <= currentState.keyboardHighPlayableNote);
            auto keyColor = getKeyColorForNote(blackNote);
            bool hasCustomColor = keyColor.isOpaque() || keyColor.getAlpha() > 0;

            if (isPressed)
            {
                juce::Colour activeBase = hasCustomColor ? keyColor : juce::Colour(0xFF2A7EE0);
                juce::ColourGradient pressGrad(activeBase.brighter(0.45f), bkRect.getX(), bkRect.getY(),
                                               activeBase.withAlpha(0.96f), bkRect.getX(), bkRect.getBottom(), false);
                g.setGradientFill(pressGrad);
                g.fillRect(bkRect);

                g.setColour(juce::Colours::white.withAlpha(0.8f));
                g.fillRect(juce::Rectangle<float>(bkRect.getX() + 0.5f, bkRect.getY(), bkRect.getWidth() - 1.0f, 2.0f));
            }
            else
            {
                if (hasCustomColor)
                {
                    juce::ColourGradient bkGrad(keyColor.withAlpha(0.55f), bkRect.getX(), bkRect.getY(),
                                               juce::Colour(0xD50A0C10), bkRect.getX(), bkRect.getBottom(), false);
                    g.setGradientFill(bkGrad);
                    g.fillRect(bkRect);
                }
                else if (inRange)
                {
                    juce::ColourGradient bkGrad(juce::Colour(0xB01C2028), bkRect.getX(), bkRect.getY(),
                                               juce::Colour(0xD50A0C10), bkRect.getX(), bkRect.getBottom(), false);
                    g.setGradientFill(bkGrad);
                    g.fillRect(bkRect);
                }
                else
                {
                    g.setColour(juce::Colour(0x50050709));
                    g.fillRect(bkRect);
                }

                g.setColour(juce::Colour(0x35000000));
                g.drawRect(bkRect, 0.5f);
                g.setColour(inRange ? juce::Colour(0x20FFFFFF) : juce::Colour(0x0CFFFFFF));
                g.drawHorizontalLine(static_cast<int>(bkRect.getY()), bkRect.getX(), bkRect.getRight());
            }
        }
    }

    // 3. Interactive Octave Scroll Buttons (Left & Right)
    float arrowBtnW = 22.0f;
    float arrowBtnH = 26.0f;
    float arrowY = keybedArea.getY() + (keybedArea.getHeight() - arrowBtnH) * 0.5f;

    leftArrowRect = juce::Rectangle<float>(keybedArea.getX() + 4.0f, arrowY, arrowBtnW, arrowBtnH);
    rightArrowRect = juce::Rectangle<float>(keybedArea.getRight() - arrowBtnW - 4.0f, arrowY, arrowBtnW, arrowBtnH);

    bool canScrollLeft = (startWhiteKeyIndex > 0);
    bool canScrollRight = (startWhiteKeyIndex < 75 - kNumVisibleWhiteKeys);

    // Left Arrow Button
    if (canScrollLeft || isHoveringLeftArrow)
    {
        g.setColour(isHoveringLeftArrow ? juce::Colour(0xD0181B22) : juce::Colour(0x80101216));
        g.fillRoundedRectangle(leftArrowRect, 3.0f);
        g.setColour(isHoveringLeftArrow ? OpenWavLookAndFeel::accentCyan : juce::Colour(0x40FFFFFF));
        g.drawRoundedRectangle(leftArrowRect, 3.0f, 1.0f);

        g.setColour(isHoveringLeftArrow ? juce::Colours::white : juce::Colour(0xB0FFFFFF));
        g.setFont(getDecentSamplerFont(11.0f, true));
        g.drawText(juce::String::charToString(0x25C0), leftArrowRect, juce::Justification::centred, false);
    }

    // Right Arrow Button
    if (canScrollRight || isHoveringRightArrow)
    {
        g.setColour(isHoveringRightArrow ? juce::Colour(0xD0181B22) : juce::Colour(0x80101216));
        g.fillRoundedRectangle(rightArrowRect, 3.0f);
        g.setColour(isHoveringRightArrow ? OpenWavLookAndFeel::accentCyan : juce::Colour(0x40FFFFFF));
        g.drawRoundedRectangle(rightArrowRect, 3.0f, 1.0f);

        g.setColour(isHoveringRightArrow ? juce::Colours::white : juce::Colour(0xB0FFFFFF));
        g.setFont(getDecentSamplerFont(11.0f, true));
        g.drawText(juce::String::charToString(0x25B6), rightArrowRect, juce::Justification::centred, false);
    }
}

void DecentSamplerCanvasComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF14161A));

    auto frameBounds = getTotalFrameBounds();
    if (!frameBounds.isEmpty())
    {
        // 1. Draw Main UI Canvas Background & Wallpaper spanning entire frame (behind the header)
        {
            juce::Graphics::ScopedSaveState sss(g);
            juce::Path frameClip;
            frameClip.addRectangle(frameBounds);
            g.reduceClipRegion(frameClip);

            if (parsedBgColor.isOpaque() || parsedBgColor.getAlpha() > 0)
            {
                g.setColour(parsedBgColor);
                g.fillRect(frameBounds);
            }
            else
            {
                g.setColour(juce::Colour(0xFF16181D));
                g.fillRect(frameBounds);
            }

            // Draw Background Wallpaper Image spanning full frame (behind header)
            if (bgImage.isValid())
            {
                g.drawImage(bgImage, frameBounds, juce::RectanglePlacement::fillDestination);
            }
        }

        // 2. Draw Top Header Bar for display (on top of background wallpaper)
        paintHeader(g, getHeaderBounds());

        // 3. Optional Tab Bar overlay if multi-tab
        if (tabButtons.size() > 1)
        {
            auto canvasBounds = getCanvasBounds();
            auto tabStripRect = canvasBounds.withHeight(30.0f * getCanvasScale());
            g.setColour(juce::Colour(0xCC101216));
            g.fillRect(tabStripRect);
            g.setColour(juce::Colour(0x35FFFFFF));
            g.drawHorizontalLine(static_cast<int>(tabStripRect.getBottom()), tabStripRect.getX(), tabStripRect.getRight());
        }

        // 4. Outer Framing Border
        g.setColour(juce::Colour(0x30FFFFFF));
        g.drawRect(frameBounds, 1.0f);
    }

    // 5. Draw Fixed-Size Keyboard Footer anchored at the absolute bottom
    paintFooter(g, getFooterBounds());
}

void DecentSamplerCanvasComponent::paintOverChildren(juce::Graphics& g)
{
    auto canvasBounds = getCanvasBounds();
    if (canvasBounds.isEmpty()) return;

    float scale = getCanvasScale();

    // 1. If a control is actively being moved, draw Decent Sampler's authentic right-side popup bubble
    if (activeDraggingControl != nullptr)
    {
        auto ctrlBounds = activeDraggingControl->getBounds().toFloat();
        juce::String valStr = activeDraggingControl->getFormattedValueString();
        if (valStr.isNotEmpty())
        {
            float popW = std::max(48.0f * scale, static_cast<float>(valStr.length() * 8.0f + 16.0f) * scale);
            float popH = 22.0f * scale;
            float popX = ctrlBounds.getRight() + 6.0f * scale;
            float popY = ctrlBounds.getCentreY() - popH * 0.5f;

            if (popX + popW > canvasBounds.getRight() - 4.0f * scale)
                popX = ctrlBounds.getX() - popW - 6.0f * scale;

            juce::Rectangle<float> popRect(popX, popY, popW, popH);

            // Sleek solid dark bubble with subtle border
            g.setColour(juce::Colour(0xF414171E));
            g.fillRoundedRectangle(popRect, 4.0f * scale);
            g.setColour(juce::Colour(0x60FFFFFF));
            g.drawRoundedRectangle(popRect, 4.0f * scale, 1.0f * scale);

            g.setColour(juce::Colours::white);
            g.setFont(getDecentSamplerFont(12.0f * scale, true));
            g.drawText(valStr, popRect, juce::Justification::centred, true);
        }
    }

    if (!editMode) return;

    g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.06f));
    float gridStep = 20.0f * scale;
    float gridStartY = canvasBounds.getY();
    for (float gx = canvasBounds.getX() + gridStep; gx < canvasBounds.getRight(); gx += gridStep)
    {
        g.drawVerticalLine(static_cast<int>(gx), gridStartY, canvasBounds.getBottom());
    }
    for (float gy = gridStartY + gridStep; gy < canvasBounds.getBottom(); gy += gridStep)
    {
        g.drawHorizontalLine(static_cast<int>(gy), canvasBounds.getX(), canvasBounds.getRight());
    }

    // 2. Draw Clean Outlines around all canvas components (Fast Rect drawing)
    g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.25f));
    auto drawItemOutline = [&](const SelectedCanvasItem& item) {
        if (item == selectedItem) return;
        auto r = getItemScreenBounds(item);
        if (!r.isEmpty())
            g.drawRect(r, 1.0f);
    };

    for (int i = 0; i < static_cast<int>(controls.size()); ++i)
        drawItemOutline({ CanvasComponentType::Control, i });
    for (int i = 0; i < static_cast<int>(labels.size()); ++i)
        drawItemOutline({ CanvasComponentType::Label, i });
    for (int i = 0; i < static_cast<int>(images.size()); ++i)
        drawItemOutline({ CanvasComponentType::Image, i });
    for (int i = 0; i < static_cast<int>(buttons.size()); ++i)
        drawItemOutline({ CanvasComponentType::Button, i });
    for (int i = 0; i < static_cast<int>(menus.size()); ++i)
        drawItemOutline({ CanvasComponentType::Menu, i });

    // 3. Draw Active Selection Box & Handles
    if (selectedItem.isValid())
    {
        auto selRect = getItemScreenBounds(selectedItem);
        if (!selRect.isEmpty())
        {
            // Glowing Accent Border
            g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.12f));
            g.fillRect(selRect);

            g.setColour(OpenWavLookAndFeel::accentCyan);
            g.drawRect(selRect, 2.0f);

            // Draw 8 Handles
            auto drawHandle = [&](float hx, float hy) {
                juce::Rectangle<float> hr(hx - 4.5f, hy - 4.5f, 9.0f, 9.0f);
                g.setColour(juce::Colours::white);
                g.fillRect(hr);
                g.setColour(OpenWavLookAndFeel::accentCyan);
                g.drawRect(hr, 1.5f);
            };

            drawHandle(selRect.getX(), selRect.getY());
            drawHandle(selRect.getRight(), selRect.getY());
            drawHandle(selRect.getRight(), selRect.getBottom());
            drawHandle(selRect.getX(), selRect.getBottom());

            drawHandle(selRect.getCentreX(), selRect.getY());
            drawHandle(selRect.getRight(), selRect.getCentreY());
            drawHandle(selRect.getCentreX(), selRect.getBottom());
            drawHandle(selRect.getX(), selRect.getCentreY());

            // Draw HUD Info Pill above/below component
            auto baseR = getItemBaseRect(selectedItem);
            juce::String hudText = getItemDisplayName(selectedItem) + "  |  X: " + juce::String(baseR.getX()) + "pt  Y: " + juce::String(baseR.getY()) + "pt  W: " + juce::String(baseR.getWidth()) + "pt  H: " + juce::String(baseR.getHeight()) + "pt";

            float hudW = std::min(400.0f, std::max(200.0f, static_cast<float>(hudText.length() * 6.5f + 24.0f)));
            float hudH = 22.0f;
            float hudX = juce::jlimit(canvasBounds.getX() + 4.0f, canvasBounds.getRight() - hudW - 4.0f, selRect.getCentreX() - hudW * 0.5f);
            float hudY = (selRect.getY() - hudH - 6.0f >= canvasBounds.getY()) ? (selRect.getY() - hudH - 6.0f) : (selRect.getBottom() + 6.0f);

            juce::Rectangle<float> hudRect(hudX, hudY, hudW, hudH);
            g.setColour(juce::Colour(0xEE141820));
            g.fillRoundedRectangle(hudRect, 4.0f);
            g.setColour(OpenWavLookAndFeel::accentCyan);
            g.drawRoundedRectangle(hudRect, 4.0f, 1.0f);

            g.setColour(OpenWavLookAndFeel::textPrimary);
            g.setFont(getDecentSamplerFont(11.0f, true));
            g.drawText(hudText, hudRect, juce::Justification::centred, true);
        }
    }

    // 4. Top Status Header Pill
    float bannerW = std::min(canvasBounds.getWidth() - 20.0f, 520.0f);
    juce::Rectangle<float> bannerRect(canvasBounds.getX() + 10.0f, canvasBounds.getY() + (tabButtons.size() > 1 ? 40.0f : 12.0f), bannerW, 24.0f);
    g.setColour(juce::Colour(0xDD0E1118));
    g.fillRoundedRectangle(bannerRect, 4.0f);
    g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.7f));
    g.drawRoundedRectangle(bannerRect, 4.0f, 1.0f);

    g.setColour(OpenWavLookAndFeel::accentCyan);
    g.setFont(getDecentSamplerFont(11.0f, true));
    g.drawText("CANVAS EDITOR: Drag to Move | Handles to Resize | Right-Click to Add | Del to Remove", bannerRect, juce::Justification::centred, true);
}

void DecentSamplerCanvasComponent::setEditModeEnabled(bool enabled)
{
    if (editMode == enabled) return;
    editMode = enabled;
    if (!editMode)
    {
        selectedItem.clear();
        isDragging = false;
        activeDragHandle = DragHandle::None;
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }
    else
    {
        setWantsKeyboardFocus(true);
        grabKeyboardFocus();
    }
    updateChildrenMouseInterception();
    if (onItemSelected) onItemSelected(selectedItem);
    repaint();
}

void DecentSamplerCanvasComponent::setSelectedItem(const SelectedCanvasItem& item)
{
    selectedItem = item;
    if (onItemSelected) onItemSelected(selectedItem);
    repaint();
}

void DecentSamplerCanvasComponent::updateChildrenMouseInterception()
{
    bool allowChildren = !editMode;
    for (auto& item : controls)
        if (item.control) item.control->setInterceptsMouseClicks(allowChildren, allowChildren);
    for (auto& item : labels)
        if (item.label) item.label->setInterceptsMouseClicks(allowChildren, allowChildren);
    for (auto& item : images)
        if (item.imageComp) item.imageComp->setInterceptsMouseClicks(allowChildren, allowChildren);
    for (auto& item : buttons)
        if (item.button) item.button->setInterceptsMouseClicks(allowChildren, allowChildren);
    for (auto& item : menus)
        if (item.combo) item.combo->setInterceptsMouseClicks(allowChildren, allowChildren);
}

DecentSamplerCanvasComponent::SelectedCanvasItem DecentSamplerCanvasComponent::hitTestCanvasItem(juce::Point<float> pos) const
{
    // Hit test top to bottom z-order: controls, buttons, menus, labels, images
    for (int i = static_cast<int>(controls.size()) - 1; i >= 0; --i)
    {
        auto r = getItemScreenBounds({ CanvasComponentType::Control, i });
        if (r.contains(pos)) return { CanvasComponentType::Control, i };
    }
    for (int i = static_cast<int>(buttons.size()) - 1; i >= 0; --i)
    {
        auto r = getItemScreenBounds({ CanvasComponentType::Button, i });
        if (r.contains(pos)) return { CanvasComponentType::Button, i };
    }
    for (int i = static_cast<int>(menus.size()) - 1; i >= 0; --i)
    {
        auto r = getItemScreenBounds({ CanvasComponentType::Menu, i });
        if (r.contains(pos)) return { CanvasComponentType::Menu, i };
    }
    for (int i = static_cast<int>(labels.size()) - 1; i >= 0; --i)
    {
        auto r = getItemScreenBounds({ CanvasComponentType::Label, i });
        if (r.contains(pos)) return { CanvasComponentType::Label, i };
    }
    for (int i = static_cast<int>(images.size()) - 1; i >= 0; --i)
    {
        auto r = getItemScreenBounds({ CanvasComponentType::Image, i });
        if (r.contains(pos)) return { CanvasComponentType::Image, i };
    }

    return {};
}

DecentSamplerCanvasComponent::DragHandle DecentSamplerCanvasComponent::hitTestHandles(juce::Rectangle<float> r, juce::Point<float> pos) const
{
    float hTol = 8.0f;
    auto testPoint = [&](float hx, float hy) {
        return pos.getDistanceFrom(juce::Point<float>(hx, hy)) <= hTol;
    };

    if (testPoint(r.getX(), r.getY())) return DragHandle::TopLeft;
    if (testPoint(r.getRight(), r.getY())) return DragHandle::TopRight;
    if (testPoint(r.getRight(), r.getBottom())) return DragHandle::BottomRight;
    if (testPoint(r.getX(), r.getBottom())) return DragHandle::BottomLeft;

    if (testPoint(r.getCentreX(), r.getY())) return DragHandle::Top;
    if (testPoint(r.getRight(), r.getCentreY())) return DragHandle::Right;
    if (testPoint(r.getCentreX(), r.getBottom())) return DragHandle::Bottom;
    if (testPoint(r.getX(), r.getCentreY())) return DragHandle::Left;

    if (r.contains(pos)) return DragHandle::Move;

    return DragHandle::None;
}

juce::Rectangle<int> DecentSamplerCanvasComponent::getItemBaseRect(const SelectedCanvasItem& item) const
{
    if (!item.isValid()) return {};
    switch (item.type)
    {
        case CanvasComponentType::Control:
            if (item.index >= 0 && item.index < static_cast<int>(controls.size()))
                return { controls[item.index].model.x, controls[item.index].model.y, controls[item.index].model.width > 0 ? controls[item.index].model.width : 80, controls[item.index].model.height > 0 ? controls[item.index].model.height : 80 };
            break;
        case CanvasComponentType::Label:
            if (item.index >= 0 && item.index < static_cast<int>(labels.size()))
                return { labels[item.index].model.x, labels[item.index].model.y, labels[item.index].model.width > 0 ? labels[item.index].model.width : 120, labels[item.index].model.height > 0 ? labels[item.index].model.height : 24 };
            break;
        case CanvasComponentType::Image:
            if (item.index >= 0 && item.index < static_cast<int>(images.size()))
                return { images[item.index].model.x, images[item.index].model.y, images[item.index].model.width > 0 ? images[item.index].model.width : 100, images[item.index].model.height > 0 ? images[item.index].model.height : 100 };
            break;
        case CanvasComponentType::Button:
            if (item.index >= 0 && item.index < static_cast<int>(buttons.size()))
                return { buttons[item.index].model.x, buttons[item.index].model.y, buttons[item.index].model.width > 0 ? buttons[item.index].model.width : 90, buttons[item.index].model.height > 0 ? buttons[item.index].model.height : 30 };
            break;
        case CanvasComponentType::Menu:
            if (item.index >= 0 && item.index < static_cast<int>(menus.size()))
                return { menus[item.index].model.x, menus[item.index].model.y, menus[item.index].model.width > 0 ? menus[item.index].model.width : 130, menus[item.index].model.height > 0 ? menus[item.index].model.height : 30 };
            break;
        default: break;
    }
    return {};
}

void DecentSamplerCanvasComponent::setItemBaseRect(const SelectedCanvasItem& item, juce::Rectangle<int> baseRect)
{
    if (!item.isValid()) return;

    auto canvasBounds = getCanvasBounds();
    float scale = getCanvasScale();
    float offX = canvasBounds.getX();
    float offY = canvasBounds.getY();

    auto screenRect = juce::Rectangle<int>(
        static_cast<int>(offX + baseRect.getX() * scale),
        static_cast<int>(offY + baseRect.getY() * scale),
        static_cast<int>(baseRect.getWidth() * scale),
        static_cast<int>(baseRect.getHeight() * scale)
    );

    int tabIdx = juce::jlimit(0, std::max(0, static_cast<int>(currentState.customUi.tabs.size()) - 1), currentTab);

    switch (item.type)
    {
        case CanvasComponentType::Control:
            if (item.index >= 0 && item.index < static_cast<int>(controls.size()))
            {
                controls[item.index].model.x = baseRect.getX();
                controls[item.index].model.y = baseRect.getY();
                controls[item.index].model.width = baseRect.getWidth();
                controls[item.index].model.height = baseRect.getHeight();
                if (controls[item.index].control)
                {
                    controls[item.index].control->setModel(controls[item.index].model);
                    controls[item.index].control->setBounds(screenRect);
                    controls[item.index].control->setScale(scale);
                }
                if (!currentState.customUi.tabs.empty() && item.index < static_cast<int>(currentState.customUi.tabs[tabIdx].controls.size()))
                {
                    auto& c = currentState.customUi.tabs[tabIdx].controls[item.index];
                    c.x = baseRect.getX(); c.y = baseRect.getY();
                    c.width = baseRect.getWidth(); c.height = baseRect.getHeight();
                }
                for (auto& uc : currentState.uiControls)
                {
                    if (uc.id == controls[item.index].model.id || uc.label == controls[item.index].model.label)
                    {
                        uc.x = baseRect.getX(); uc.y = baseRect.getY();
                        uc.width = baseRect.getWidth(); uc.height = baseRect.getHeight();
                    }
                }
            }
            break;
        case CanvasComponentType::Label:
            if (item.index >= 0 && item.index < static_cast<int>(labels.size()))
            {
                labels[item.index].model.x = baseRect.getX();
                labels[item.index].model.y = baseRect.getY();
                labels[item.index].model.width = baseRect.getWidth();
                labels[item.index].model.height = baseRect.getHeight();
                if (labels[item.index].label)
                    labels[item.index].label->setBounds(screenRect);
                if (!currentState.customUi.tabs.empty() && item.index < static_cast<int>(currentState.customUi.tabs[tabIdx].labels.size()))
                {
                    auto& l = currentState.customUi.tabs[tabIdx].labels[item.index];
                    l.x = baseRect.getX(); l.y = baseRect.getY();
                    l.width = baseRect.getWidth(); l.height = baseRect.getHeight();
                }
            }
            break;
        case CanvasComponentType::Image:
            if (item.index >= 0 && item.index < static_cast<int>(images.size()))
            {
                images[item.index].model.x = baseRect.getX();
                images[item.index].model.y = baseRect.getY();
                images[item.index].model.width = baseRect.getWidth();
                images[item.index].model.height = baseRect.getHeight();
                if (images[item.index].imageComp)
                    images[item.index].imageComp->setBounds(screenRect);
                if (!currentState.customUi.tabs.empty() && item.index < static_cast<int>(currentState.customUi.tabs[tabIdx].images.size()))
                {
                    auto& img = currentState.customUi.tabs[tabIdx].images[item.index];
                    img.x = baseRect.getX(); img.y = baseRect.getY();
                    img.width = baseRect.getWidth(); img.height = baseRect.getHeight();
                }
            }
            break;
        case CanvasComponentType::Button:
            if (item.index >= 0 && item.index < static_cast<int>(buttons.size()))
            {
                buttons[item.index].model.x = baseRect.getX();
                buttons[item.index].model.y = baseRect.getY();
                buttons[item.index].model.width = baseRect.getWidth();
                buttons[item.index].model.height = baseRect.getHeight();
                if (buttons[item.index].button)
                    buttons[item.index].button->setBounds(screenRect);
                if (!currentState.customUi.tabs.empty() && item.index < static_cast<int>(currentState.customUi.tabs[tabIdx].buttons.size()))
                {
                    auto& btn = currentState.customUi.tabs[tabIdx].buttons[item.index];
                    btn.x = baseRect.getX(); btn.y = baseRect.getY();
                    btn.width = baseRect.getWidth(); btn.height = baseRect.getHeight();
                }
            }
            break;
        case CanvasComponentType::Menu:
            if (item.index >= 0 && item.index < static_cast<int>(menus.size()))
            {
                menus[item.index].model.x = baseRect.getX();
                menus[item.index].model.y = baseRect.getY();
                menus[item.index].model.width = baseRect.getWidth();
                menus[item.index].model.height = baseRect.getHeight();
                if (menus[item.index].combo)
                    menus[item.index].combo->setBounds(screenRect);
                if (!currentState.customUi.tabs.empty() && item.index < static_cast<int>(currentState.customUi.tabs[tabIdx].menus.size()))
                {
                    auto& m = currentState.customUi.tabs[tabIdx].menus[item.index];
                    m.x = baseRect.getX(); m.y = baseRect.getY();
                    m.width = baseRect.getWidth(); m.height = baseRect.getHeight();
                }
            }
            break;
        default: break;
    }
}

juce::Rectangle<float> DecentSamplerCanvasComponent::getItemScreenBounds(const SelectedCanvasItem& item) const
{
    if (!item.isValid()) return {};
    auto r = getItemBaseRect(item);
    auto canvasBounds = getCanvasBounds();
    float scale = getCanvasScale();
    float offX = canvasBounds.getX();
    float offY = canvasBounds.getY();

    return juce::Rectangle<float>(offX + r.getX() * scale, offY + r.getY() * scale, r.getWidth() * scale, r.getHeight() * scale);
}

juce::String DecentSamplerCanvasComponent::getItemDisplayName(const SelectedCanvasItem& item) const
{
    if (!item.isValid()) return {};
    switch (item.type)
    {
        case CanvasComponentType::Control:
            if (item.index >= 0 && item.index < static_cast<int>(controls.size()))
                return "[Control] " + (controls[item.index].model.label.isNotEmpty() ? controls[item.index].model.label : controls[item.index].model.id);
            break;
        case CanvasComponentType::Label:
            if (item.index >= 0 && item.index < static_cast<int>(labels.size()))
                return "[Label] \"" + labels[item.index].model.text + "\"";
            break;
        case CanvasComponentType::Image:
            if (item.index >= 0 && item.index < static_cast<int>(images.size()))
                return "[Image] " + juce::File(images[item.index].model.path).getFileName();
            break;
        case CanvasComponentType::Button:
            if (item.index >= 0 && item.index < static_cast<int>(buttons.size()))
                return "[Button] " + buttons[item.index].model.text;
            break;
        case CanvasComponentType::Menu:
            if (item.index >= 0 && item.index < static_cast<int>(menus.size()))
                return "[Menu] (" + juce::String(menus[item.index].model.options.size()) + " options)";
            break;
        default: break;
    }
    return {};
}

void DecentSamplerCanvasComponent::showEditContextMenu(juce::Point<int> pos)
{
    juce::PopupMenu menu;
    auto canvasBounds = getCanvasBounds();
    float scale = getCanvasScale();
    int baseX = static_cast<int>((pos.x - canvasBounds.getX()) / scale);
    int baseY = static_cast<int>((pos.y - canvasBounds.getY()) / scale);

    if (selectedItem.isValid())
    {
        menu.addItem(1, "Delete Selected Component");
        menu.addItem(2, "Duplicate Component");
        menu.addSeparator();
        menu.addItem(3, "Snap to 10pt Grid");
        if (selectedItem.type == CanvasComponentType::Image)
            menu.addItem(4, "Change Image File... (Select File)");
        else if (selectedItem.type == CanvasComponentType::Control)
            menu.addItem(5, "Set Knob Skin Filmstrip... (Select File)");
        menu.addSeparator();
    }

    menu.addItem(10, "Add Knob / Slider");
    menu.addItem(11, "Add Text Label");
    menu.addItem(12, "Add Button");
    menu.addItem(13, "Add Dropdown Menu");
    menu.addItem(14, "Add Image Component... (Select File)");
    menu.addSeparator();
    menu.addItem(15, "Set Canvas Background Image... (Select File)");
    if (currentState.customUi.bgImagePath.isNotEmpty() || currentState.customUi.resolvedBgImagePath.isNotEmpty())
        menu.addItem(16, "Clear Canvas Background Image");

    auto screenPos = localPointToGlobal(pos);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(juce::Rectangle<int>(screenPos.x, screenPos.y, 1, 1)),
                       [this, baseX, baseY](int result)
    {
        int tabIdx = juce::jlimit(0, std::max(0, static_cast<int>(currentState.customUi.tabs.size()) - 1), currentTab);
        if (currentState.customUi.tabs.empty())
        {
            DecentSamplerTabState newTab;
            newTab.name = "Main";
            currentState.customUi.tabs.push_back(newTab);
        }

        if (result == 1) // Delete
        {
            deleteSelectedItem();
        }
        else if (result == 2) // Duplicate
        {
            duplicateSelectedItem();
        }
        else if (result == 3) // Snap
        {
            auto r = getItemBaseRect(selectedItem);
            r.setX((r.getX() / 10) * 10);
            r.setY((r.getY() / 10) * 10);
            r.setWidth(std::max(20, (r.getWidth() / 10) * 10));
            r.setHeight(std::max(20, (r.getHeight() / 10) * 10));
            setItemBaseRect(selectedItem, r);
            if (onStateChanged) onStateChanged(currentState);
            repaint();
        }
        else if (result == 4) // Change Image File
        {
            auto chooser = std::make_shared<juce::FileChooser>("Select Image File", juce::File(), "*.png;*.jpg;*.jpeg;*.webp;*.svg;*.bmp;*.gif;*.tif;*.tiff");
            chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                 [this, tabIdx, chooser](const juce::FileChooser&) {
                auto file = chooser->getResult();
                if (file.existsAsFile() && selectedItem.type == CanvasComponentType::Image &&
                    selectedItem.index >= 0 && selectedItem.index < static_cast<int>(currentState.customUi.tabs[tabIdx].images.size()))
                {
                    auto& imgModel = currentState.customUi.tabs[tabIdx].images[selectedItem.index];
                    imgModel.path = file.getFullPathName();
                    imgModel.resolvedFilePath = file.getFullPathName();
                    rebuildActiveTabUi();
                    if (onStateChanged) onStateChanged(currentState);
                }
            });
        }
        else if (result == 5) // Set Knob Skin Filmstrip
        {
            auto chooser = std::make_shared<juce::FileChooser>("Select Knob Filmstrip Image", juce::File(), "*.png;*.jpg;*.jpeg;*.webp;*.bmp");
            chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                 [this, tabIdx, chooser](const juce::FileChooser&) {
                auto file = chooser->getResult();
                if (file.existsAsFile() && selectedItem.type == CanvasComponentType::Control &&
                    selectedItem.index >= 0 && selectedItem.index < static_cast<int>(currentState.customUi.tabs[tabIdx].controls.size()))
                {
                    auto& ctrlModel = currentState.customUi.tabs[tabIdx].controls[selectedItem.index];
                    ctrlModel.customSkinImagePath = file.getFullPathName();
                    ctrlModel.resolvedCustomSkinImagePath = file.getFullPathName();
                    for (auto& uc : currentState.uiControls)
                    {
                        if (uc.id == ctrlModel.id || uc.label == ctrlModel.label)
                        {
                            uc.customSkinImagePath = file.getFullPathName();
                            uc.resolvedCustomSkinImagePath = file.getFullPathName();
                        }
                    }
                    rebuildActiveTabUi();
                    if (onStateChanged) onStateChanged(currentState);
                }
            });
        }
        else if (result == 10) // Add Control
        {
            DecentSamplerUiControl c;
            c.id = "ctrl_" + juce::String(juce::Random::getSystemRandom().nextInt(9999));
            c.label = "Control " + juce::String(controls.size() + 1);
            c.type = "knob";
            c.x = std::max(10, baseX);
            c.y = std::max(10, baseY);
            c.width = 80;
            c.height = 80;
            c.minValue = 0.0;
            c.maxValue = 1.0;
            c.currentValue = 0.5;
            currentState.customUi.tabs[tabIdx].controls.push_back(c);
            currentState.uiControls.push_back(c);
            rebuildActiveTabUi();
            selectedItem = { CanvasComponentType::Control, static_cast<int>(controls.size()) - 1 };
            if (onStateChanged) onStateChanged(currentState);
        }
        else if (result == 11) // Add Label
        {
            DecentSamplerUiLabel l;
            l.text = "Label Text";
            l.x = std::max(10, baseX);
            l.y = std::max(10, baseY);
            l.width = 120;
            l.height = 24;
            l.textSize = 10.0f;
            currentState.customUi.tabs[tabIdx].labels.push_back(l);
            rebuildActiveTabUi();
            selectedItem = { CanvasComponentType::Label, static_cast<int>(labels.size()) - 1 };
            if (onStateChanged) onStateChanged(currentState);
        }
        else if (result == 12) // Add Button
        {
            DecentSamplerUiButton b;
            b.text = "Button";
            b.x = std::max(10, baseX);
            b.y = std::max(10, baseY);
            b.width = 90;
            b.height = 30;
            currentState.customUi.tabs[tabIdx].buttons.push_back(b);
            rebuildActiveTabUi();
            selectedItem = { CanvasComponentType::Button, static_cast<int>(buttons.size()) - 1 };
            if (onStateChanged) onStateChanged(currentState);
        }
        else if (result == 13) // Add Menu
        {
            DecentSamplerUiMenu m;
            m.x = std::max(10, baseX);
            m.y = std::max(10, baseY);
            m.width = 130;
            m.height = 30;
            m.options.add("Option 1");
            m.options.add("Option 2");
            currentState.customUi.tabs[tabIdx].menus.push_back(m);
            rebuildActiveTabUi();
            selectedItem = { CanvasComponentType::Menu, static_cast<int>(menus.size()) - 1 };
            if (onStateChanged) onStateChanged(currentState);
        }
        else if (result == 14) // Add Image Component via File Dialog
        {
            auto chooser = std::make_shared<juce::FileChooser>("Select Image File", juce::File(), "*.png;*.jpg;*.jpeg;*.webp;*.svg;*.bmp;*.gif;*.tif;*.tiff");
            chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                 [this, baseX, baseY, tabIdx, chooser](const juce::FileChooser&) {
                auto file = chooser->getResult();
                if (file.existsAsFile())
                {
                    auto img = juce::ImageFileFormat::loadFrom(file);
                    int imgW = img.isValid() ? img.getWidth() : 120;
                    int imgH = img.isValid() ? img.getHeight() : 120;
                    if (imgW > 280)
                    {
                        imgH = static_cast<int>(imgH * (280.0f / imgW));
                        imgW = 280;
                    }

                    DecentSamplerUiImage newImg;
                    newImg.path = file.getFullPathName();
                    newImg.resolvedFilePath = file.getFullPathName();
                    newImg.x = std::max(10, baseX);
                    newImg.y = std::max(10, baseY);
                    newImg.width = std::max(20, imgW);
                    newImg.height = std::max(20, imgH);

                    currentState.customUi.tabs[tabIdx].images.push_back(newImg);
                    rebuildActiveTabUi();
                    selectedItem = { CanvasComponentType::Image, static_cast<int>(images.size()) - 1 };
                    if (onStateChanged) onStateChanged(currentState);
                }
            });
        }
        else if (result == 15) // Set Canvas Background Image
        {
            auto chooser = std::make_shared<juce::FileChooser>("Select Background Image", juce::File(), "*.png;*.jpg;*.jpeg;*.webp;*.svg;*.bmp;*.gif;*.tif;*.tiff");
            chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                 [this, chooser](const juce::FileChooser&) {
                auto file = chooser->getResult();
                if (file.existsAsFile())
                {
                    currentState.customUi.bgImagePath = file.getFullPathName();
                    currentState.customUi.resolvedBgImagePath = file.getFullPathName();
                    setInstrumentState(currentState);
                    if (onStateChanged) onStateChanged(currentState);
                }
            });
        }
        else if (result == 16) // Clear Canvas Background Image
        {
            currentState.customUi.bgImagePath = "";
            currentState.customUi.resolvedBgImagePath = "";
            setInstrumentState(currentState);
            if (onStateChanged) onStateChanged(currentState);
        }
    });
}

void DecentSamplerCanvasComponent::deleteSelectedItem()
{
    if (!selectedItem.isValid()) return;
    int tabIdx = juce::jlimit(0, std::max(0, static_cast<int>(currentState.customUi.tabs.size()) - 1), currentTab);
    if (tabIdx >= static_cast<int>(currentState.customUi.tabs.size())) return;

    auto& tab = currentState.customUi.tabs[tabIdx];
    switch (selectedItem.type)
    {
        case CanvasComponentType::Control:
            if (selectedItem.index >= 0 && selectedItem.index < static_cast<int>(tab.controls.size()))
            {
                juce::String cid = tab.controls[selectedItem.index].id;
                tab.controls.erase(tab.controls.begin() + selectedItem.index);
                for (auto it = currentState.uiControls.begin(); it != currentState.uiControls.end(); ++it)
                {
                    if (it->id == cid) { currentState.uiControls.erase(it); break; }
                }
            }
            break;
        case CanvasComponentType::Label:
            if (selectedItem.index >= 0 && selectedItem.index < static_cast<int>(tab.labels.size()))
                tab.labels.erase(tab.labels.begin() + selectedItem.index);
            break;
        case CanvasComponentType::Image:
            if (selectedItem.index >= 0 && selectedItem.index < static_cast<int>(tab.images.size()))
                tab.images.erase(tab.images.begin() + selectedItem.index);
            break;
        case CanvasComponentType::Button:
            if (selectedItem.index >= 0 && selectedItem.index < static_cast<int>(tab.buttons.size()))
                tab.buttons.erase(tab.buttons.begin() + selectedItem.index);
            break;
        case CanvasComponentType::Menu:
            if (selectedItem.index >= 0 && selectedItem.index < static_cast<int>(tab.menus.size()))
                tab.menus.erase(tab.menus.begin() + selectedItem.index);
            break;
        default: break;
    }

    selectedItem.clear();
    rebuildActiveTabUi();
    if (onStateChanged) onStateChanged(currentState);
}

void DecentSamplerCanvasComponent::duplicateSelectedItem()
{
    if (!selectedItem.isValid()) return;
    int tabIdx = juce::jlimit(0, std::max(0, static_cast<int>(currentState.customUi.tabs.size()) - 1), currentTab);
    if (tabIdx >= static_cast<int>(currentState.customUi.tabs.size())) return;

    auto& tab = currentState.customUi.tabs[tabIdx];
    switch (selectedItem.type)
    {
        case CanvasComponentType::Control:
            if (selectedItem.index >= 0 && selectedItem.index < static_cast<int>(tab.controls.size()))
            {
                auto c = tab.controls[selectedItem.index];
                c.id = c.id + "_copy";
                c.label = c.label + " Copy";
                c.x += 20; c.y += 20;
                tab.controls.push_back(c);
                currentState.uiControls.push_back(c);
                rebuildActiveTabUi();
                selectedItem = { CanvasComponentType::Control, static_cast<int>(controls.size()) - 1 };
            }
            break;
        case CanvasComponentType::Label:
            if (selectedItem.index >= 0 && selectedItem.index < static_cast<int>(tab.labels.size()))
            {
                auto l = tab.labels[selectedItem.index];
                l.x += 20; l.y += 20;
                tab.labels.push_back(l);
                rebuildActiveTabUi();
                selectedItem = { CanvasComponentType::Label, static_cast<int>(labels.size()) - 1 };
            }
            break;
        case CanvasComponentType::Image:
            if (selectedItem.index >= 0 && selectedItem.index < static_cast<int>(tab.images.size()))
            {
                auto img = tab.images[selectedItem.index];
                img.x += 20; img.y += 20;
                tab.images.push_back(img);
                rebuildActiveTabUi();
                selectedItem = { CanvasComponentType::Image, static_cast<int>(images.size()) - 1 };
            }
            break;
        case CanvasComponentType::Button:
            if (selectedItem.index >= 0 && selectedItem.index < static_cast<int>(tab.buttons.size()))
                {
                auto b = tab.buttons[selectedItem.index];
                b.x += 20; b.y += 20;
                tab.buttons.push_back(b);
                rebuildActiveTabUi();
                selectedItem = { CanvasComponentType::Button, static_cast<int>(buttons.size()) - 1 };
            }
            break;
        case CanvasComponentType::Menu:
            if (selectedItem.index >= 0 && selectedItem.index < static_cast<int>(tab.menus.size()))
            {
                auto m = tab.menus[selectedItem.index];
                m.x += 20; m.y += 20;
                tab.menus.push_back(m);
                rebuildActiveTabUi();
                selectedItem = { CanvasComponentType::Menu, static_cast<int>(menus.size()) - 1 };
            }
            break;
        default: break;
    }
    if (onStateChanged) onStateChanged(currentState);
}

void DecentSamplerCanvasComponent::mouseDown(const juce::MouseEvent& e)
{
    grabKeyboardFocus();

    auto kbRect = getKeyboardBounds();
    if (kbRect.contains(e.position))
    {
        if (leftArrowRect.contains(e.position) && startWhiteKeyIndex > 0)
        {
            scrollKeyboardOctave(-1);
            return;
        }
        if (rightArrowRect.contains(e.position) && startWhiteKeyIndex < 75 - kNumVisibleWhiteKeys)
        {
            scrollKeyboardOctave(1);
            return;
        }

        int note = noteNumberAtKeyboardPos(e.position, kbRect);
        if (note >= 0 && note <= 127)
        {
            keyboardAuditionNote = note;
            audioEngine.getKeyboardState().noteOn(1, note, 0.8f);
            repaint(kbRect.toNearestInt());
        }
        return;
    }

    if (!editMode) return;

    if (e.mods.isPopupMenu() || e.mods.isRightButtonDown())
    {
        auto hit = hitTestCanvasItem(e.position);
        if (hit.isValid())
            selectedItem = hit;
        if (onItemSelected) onItemSelected(selectedItem);
        showEditContextMenu(e.getPosition());
        repaint();
        return;
    }

    // Check if clicked a handle on the current selected item
    if (selectedItem.isValid())
    {
        auto r = getItemScreenBounds(selectedItem);
        auto handle = hitTestHandles(r, e.position);
        if (handle != DragHandle::None)
        {
            activeDragHandle = handle;
            isDragging = true;
            dragStartMousePos = e.getPosition();
            dragStartBaseRect = getItemBaseRect(selectedItem);
            if (onItemSelected) onItemSelected(selectedItem);
            repaint();
            return;
        }
    }

    // Hit test item under mouse
    auto hit = hitTestCanvasItem(e.position);
    if (hit.isValid())
    {
        selectedItem = hit;
        activeDragHandle = DragHandle::Move;
        isDragging = true;
        dragStartMousePos = e.getPosition();
        dragStartBaseRect = getItemBaseRect(selectedItem);
    }
    else
    {
        selectedItem.clear();
        isDragging = false;
        activeDragHandle = DragHandle::None;
    }

    if (onItemSelected) onItemSelected(selectedItem);
    repaint();
}

void DecentSamplerCanvasComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (keyboardAuditionNote >= 0)
    {
        int newNote = noteNumberAtKeyboardPos(e.position, getKeyboardBounds());
        if (newNote >= 0 && newNote <= 127 && newNote != keyboardAuditionNote)
        {
            audioEngine.getKeyboardState().noteOff(1, keyboardAuditionNote, 0.0f);
            keyboardAuditionNote = newNote;
            audioEngine.getKeyboardState().noteOn(1, newNote, 0.8f);
            repaint();
        }
        return;
    }

    if (!editMode || !isDragging || !selectedItem.isValid()) return;

    auto canvasBounds = getCanvasBounds();
    float baseW = static_cast<float>(currentState.customUi.width > 0 ? currentState.customUi.width : 812);
    if (baseW <= 0) baseW = 812.0f;
    float scale = canvasBounds.getWidth() / baseW;

    int dx = e.getPosition().x - dragStartMousePos.x;
    int dy = e.getPosition().y - dragStartMousePos.y;

    int baseDx = static_cast<int>(std::round(dx / scale));
    int baseDy = static_cast<int>(std::round(dy / scale));

    auto newR = dragStartBaseRect;

    bool isRotaryKnob = false;
    if (selectedItem.type == CanvasComponentType::Control && selectedItem.index >= 0 && selectedItem.index < static_cast<int>(controls.size()))
    {
        const auto& cType = controls[selectedItem.index].model.type;
        const auto& cStyle = controls[selectedItem.index].model.style;
        bool isLinear = cType.containsIgnoreCase("linear") || cStyle.containsIgnoreCase("linear") ||
                        cType.containsIgnoreCase("vertical") || cType.containsIgnoreCase("horizontal") ||
                        cType.containsIgnoreCase("slider");
        if (!isLinear) isRotaryKnob = true;
    }

    if (isRotaryKnob)
    {
        switch (activeDragHandle)
        {
            case DragHandle::Move:
                newR.setX(dragStartBaseRect.getX() + baseDx);
                newR.setY(dragStartBaseRect.getY() + baseDy);
                break;
            case DragHandle::BottomRight:
            {
                int delta = (std::abs(baseDx) > std::abs(baseDy)) ? baseDx : baseDy;
                int newSize = std::max(20, dragStartBaseRect.getWidth() + delta);
                newR.setWidth(newSize);
                newR.setHeight(newSize);
                break;
            }
            case DragHandle::Right:
            case DragHandle::Bottom:
            {
                int delta = (activeDragHandle == DragHandle::Right) ? baseDx : baseDy;
                int newSize = std::max(20, dragStartBaseRect.getWidth() + delta);
                newR.setWidth(newSize);
                newR.setHeight(newSize);
                break;
            }
            case DragHandle::TopLeft:
            {
                int delta = (std::abs(baseDx) > std::abs(baseDy)) ? baseDx : baseDy;
                int newSize = std::max(20, dragStartBaseRect.getWidth() - delta);
                newR.setX(dragStartBaseRect.getRight() - newSize);
                newR.setY(dragStartBaseRect.getBottom() - newSize);
                newR.setWidth(newSize);
                newR.setHeight(newSize);
                break;
            }
            case DragHandle::TopRight:
            {
                int delta = (std::abs(baseDx) > std::abs(baseDy)) ? baseDx : -baseDy;
                int newSize = std::max(20, dragStartBaseRect.getWidth() + delta);
                newR.setY(dragStartBaseRect.getBottom() - newSize);
                newR.setWidth(newSize);
                newR.setHeight(newSize);
                break;
            }
            case DragHandle::BottomLeft:
            {
                int delta = (std::abs(baseDx) > std::abs(baseDy)) ? -baseDx : baseDy;
                int newSize = std::max(20, dragStartBaseRect.getWidth() + delta);
                newR.setX(dragStartBaseRect.getRight() - newSize);
                newR.setWidth(newSize);
                newR.setHeight(newSize);
                break;
            }
            case DragHandle::Top:
            {
                int newSize = std::max(20, dragStartBaseRect.getHeight() - baseDy);
                newR.setY(dragStartBaseRect.getBottom() - newSize);
                newR.setWidth(newSize);
                newR.setHeight(newSize);
                break;
            }
            case DragHandle::Left:
            {
                int newSize = std::max(20, dragStartBaseRect.getWidth() - baseDx);
                newR.setX(dragStartBaseRect.getRight() - newSize);
                newR.setWidth(newSize);
                newR.setHeight(newSize);
                break;
            }
            default: break;
        }
    }
    else
    {
        switch (activeDragHandle)
        {
            case DragHandle::Move:
                newR.setX(dragStartBaseRect.getX() + baseDx);
                newR.setY(dragStartBaseRect.getY() + baseDy);
                break;
            case DragHandle::BottomRight:
                newR.setWidth(std::max(16, dragStartBaseRect.getWidth() + baseDx));
                newR.setHeight(std::max(16, dragStartBaseRect.getHeight() + baseDy));
                break;
            case DragHandle::Right:
                newR.setWidth(std::max(16, dragStartBaseRect.getWidth() + baseDx));
                break;
            case DragHandle::Bottom:
                newR.setHeight(std::max(16, dragStartBaseRect.getHeight() + baseDy));
                break;
            case DragHandle::TopLeft:
                newR.setX(dragStartBaseRect.getX() + baseDx);
                newR.setY(dragStartBaseRect.getY() + baseDy);
                newR.setWidth(std::max(16, dragStartBaseRect.getWidth() - baseDx));
                newR.setHeight(std::max(16, dragStartBaseRect.getHeight() - baseDy));
                break;
            case DragHandle::TopRight:
                newR.setY(dragStartBaseRect.getY() + baseDy);
                newR.setWidth(std::max(16, dragStartBaseRect.getWidth() + baseDx));
                newR.setHeight(std::max(16, dragStartBaseRect.getHeight() - baseDy));
                break;
            case DragHandle::BottomLeft:
                newR.setX(dragStartBaseRect.getX() + baseDx);
                newR.setWidth(std::max(16, dragStartBaseRect.getWidth() - baseDx));
                newR.setHeight(std::max(16, dragStartBaseRect.getHeight() + baseDy));
                break;
            case DragHandle::Top:
                newR.setY(dragStartBaseRect.getY() + baseDy);
                newR.setHeight(std::max(16, dragStartBaseRect.getHeight() - baseDy));
                break;
            case DragHandle::Left:
                newR.setX(dragStartBaseRect.getX() + baseDx);
                newR.setWidth(std::max(16, dragStartBaseRect.getWidth() - baseDx));
                break;
            default: break;
        }
    }

    setItemBaseRect(selectedItem, newR);
    if (onItemSelected) onItemSelected(selectedItem);
    repaint();
}

void DecentSamplerCanvasComponent::mouseUp(const juce::MouseEvent&)
{
    if (isDraggingPitchWheel)
    {
        isDraggingPitchWheel = false;
        pitchWheelValue = 0.5f;
        audioEngine.getKeyboardState().processNextMidiEvent(juce::MidiMessage::pitchWheel(1, 8192));
        repaint();
    }
    if (isDraggingModWheel)
    {
        isDraggingModWheel = false;
        repaint();
    }
    if (keyboardAuditionNote >= 0)
    {
        audioEngine.getKeyboardState().noteOff(1, keyboardAuditionNote, 0.0f);
        keyboardAuditionNote = -1;
        repaint();
    }

    if (!editMode) return;
    isDragging = false;
    activeDragHandle = DragHandle::None;
    if (onItemSelected) onItemSelected(selectedItem);
    if (onStateChanged) onStateChanged(currentState);
    repaint();
}

void DecentSamplerCanvasComponent::mouseMove(const juce::MouseEvent& e)
{
    auto kbRect = getKeyboardBounds();
    if (kbRect.contains(e.position))
    {
        bool hLeft = leftArrowRect.contains(e.position) && (startWhiteKeyIndex > 0);
        bool hRight = rightArrowRect.contains(e.position) && (startWhiteKeyIndex < 75 - kNumVisibleWhiteKeys);
        if (hLeft != isHoveringLeftArrow || hRight != isHoveringRightArrow)
        {
            isHoveringLeftArrow = hLeft;
            isHoveringRightArrow = hRight;
            repaint(kbRect.toNearestInt());
        }
        if (hLeft || hRight)
        {
            setMouseCursor(juce::MouseCursor::PointingHandCursor);
            return;
        }
    }
    else if (isHoveringLeftArrow || isHoveringRightArrow)
    {
        isHoveringLeftArrow = false;
        isHoveringRightArrow = false;
        repaint(kbRect.toNearestInt());
    }

    if (!editMode)
    {
        setMouseCursor(juce::MouseCursor::NormalCursor);
        return;
    }

    auto pos = e.getPosition().toFloat();
    if (selectedItem.isValid())
    {
        auto r = getItemScreenBounds(selectedItem);
        if (!r.isEmpty())
        {
            auto handle = hitTestHandles(r, pos);
            switch (handle)
            {
                case DragHandle::TopLeft:
                case DragHandle::BottomRight:
                    setMouseCursor(juce::MouseCursor::TopLeftCornerResizeCursor);
                    return;
                case DragHandle::TopRight:
                case DragHandle::BottomLeft:
                    setMouseCursor(juce::MouseCursor::TopRightCornerResizeCursor);
                    return;
                case DragHandle::Top:
                case DragHandle::Bottom:
                    setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
                    return;
                case DragHandle::Left:
                case DragHandle::Right:
                    setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
                    return;
                case DragHandle::Move:
                    setMouseCursor(juce::MouseCursor::DraggingHandCursor);
                    return;
                default: break;
            }
        }
    }

    auto hit = hitTestCanvasItem(pos);
    if (hit.isValid())
    {
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    }
    else
    {
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }
}

bool DecentSamplerCanvasComponent::keyPressed(const juce::KeyPress& key)
{
    if (!editMode || !selectedItem.isValid()) return false;

    int step = key.getModifiers().isShiftDown() ? 10 : 1;
    auto r = getItemBaseRect(selectedItem);

    if (key.isKeyCode(juce::KeyPress::leftKey))
    {
        r.setX(r.getX() - step);
        setItemBaseRect(selectedItem, r);
        if (onItemSelected) onItemSelected(selectedItem);
        if (onStateChanged) onStateChanged(currentState);
        repaint();
        return true;
    }
    if (key.isKeyCode(juce::KeyPress::rightKey))
    {
        r.setX(r.getX() + step);
        setItemBaseRect(selectedItem, r);
        if (onItemSelected) onItemSelected(selectedItem);
        if (onStateChanged) onStateChanged(currentState);
        repaint();
        return true;
    }
    if (key.isKeyCode(juce::KeyPress::upKey))
    {
        r.setY(r.getY() - step);
        setItemBaseRect(selectedItem, r);
        if (onItemSelected) onItemSelected(selectedItem);
        if (onStateChanged) onStateChanged(currentState);
        repaint();
        return true;
    }
    if (key.isKeyCode(juce::KeyPress::downKey))
    {
        r.setY(r.getY() + step);
        setItemBaseRect(selectedItem, r);
        if (onItemSelected) onItemSelected(selectedItem);
        if (onStateChanged) onStateChanged(currentState);
        repaint();
        return true;
    }
    if (key.isKeyCode(juce::KeyPress::deleteKey) || key.isKeyCode(juce::KeyPress::backspaceKey))
    {
        deleteSelectedItem();
        if (onItemSelected) onItemSelected(selectedItem);
        return true;
    }
    if (key.isKeyCode(juce::KeyPress::escapeKey))
    {
        selectedItem.clear();
        if (onItemSelected) onItemSelected(selectedItem);
        repaint();
        return true;
    }

    return false;
}

void DecentSamplerCanvasComponent::resized()
{
    auto canvasBounds = getCanvasBounds();
    float scale = getCanvasScale();
    float offX = canvasBounds.getX();
    float offY = canvasBounds.getY();

    // Layout Tab buttons inside the Header (Fixed 30px bar)
    if (tabButtons.size() > 1)
    {
        float navH = 24.0f;
        auto tabArea = juce::Rectangle<float>(canvasBounds.getX() + 10.0f,
                                              canvasBounds.getY() + (30.0f - navH) * 0.5f,
                                              canvasBounds.getWidth() - 20.0f, navH);
        int tabW = std::min(120, static_cast<int>(tabArea.getWidth() / tabButtons.size()));
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
            int pw = static_cast<int>((item.model.width > 0 ? item.model.width : 100) * scale);
            int ph = static_cast<int>((item.model.height > 0 ? item.model.height : 100) * scale);
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
            int pw = static_cast<int>((item.model.width > 0 ? item.model.width : 120) * scale);
            int ph = static_cast<int>((item.model.height > 0 ? item.model.height : 24) * scale);
            item.label->setBounds(px, py, pw, ph);
            float fSize = std::max(6.0f, (item.model.textSize > 0.0f ? item.model.textSize : 10.0f) * scale);
            item.label->setFont(getDecentSamplerFont(fSize, false));
        }
    }

    // Layout Controls (Knobs & Sliders)
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
                int pw = static_cast<int>((item.model.width > 0 ? item.model.width : 80) * scale);
                int ph = static_cast<int>((item.model.height > 0 ? item.model.height : 80) * scale);
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
        int itemH = static_cast<int>(std::min(110.0f, canvasBounds.getHeight() / 2.5f));
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

    // Layout Buttons
    for (auto& item : buttons)
    {
        if (item.button != nullptr)
        {
            int px = static_cast<int>(offX + item.model.x * scale);
            int py = static_cast<int>(offY + item.model.y * scale);
            int pw = static_cast<int>((item.model.width > 0 ? item.model.width : 90) * scale);
            int ph = static_cast<int>((item.model.height > 0 ? item.model.height : 30) * scale);
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
            int pw = static_cast<int>((item.model.width > 0 ? item.model.width : 130) * scale);
            int ph = static_cast<int>((item.model.height > 0 ? item.model.height : 30) * scale);
            item.combo->setBounds(px, py, pw, ph);
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

    // Auto-resolve custom skin images across all controls, buttons, and images if relative
    juce::File baseLookupDir = juce::File(currentState.customUi.resolvedBgImagePath.isNotEmpty() ? currentState.customUi.resolvedBgImagePath : currentState.customUi.bgImagePath);
    for (auto& tab : currentState.customUi.tabs)
    {
        for (auto& ctrl : tab.controls)
        {
            if (ctrl.customSkinImagePath.isNotEmpty() && ctrl.resolvedCustomSkinImagePath.isEmpty())
            {
                ctrl.resolvedCustomSkinImagePath = SampleMapState::resolveDecentSamplerSamplePath(ctrl.customSkinImagePath, baseLookupDir).getFullPathName();
            }
        }
        for (auto& img : tab.images)
        {
            if (img.path.isNotEmpty() && img.resolvedFilePath.isEmpty())
            {
                img.resolvedFilePath = SampleMapState::resolveDecentSamplerSamplePath(img.path, baseLookupDir).getFullPathName();
            }
        }
        for (auto& btn : tab.buttons)
        {
            if (btn.mainImage.isNotEmpty() && btn.resolvedMainImagePath.isEmpty())
                btn.resolvedMainImagePath = SampleMapState::resolveDecentSamplerSamplePath(btn.mainImage, baseLookupDir).getFullPathName();
            if (btn.hoverImage.isNotEmpty() && btn.resolvedHoverImagePath.isEmpty())
                btn.resolvedHoverImagePath = SampleMapState::resolveDecentSamplerSamplePath(btn.hoverImage, baseLookupDir).getFullPathName();
            if (btn.clickImage.isNotEmpty() && btn.resolvedClickImagePath.isEmpty())
                btn.resolvedClickImagePath = SampleMapState::resolveDecentSamplerSamplePath(btn.clickImage, baseLookupDir).getFullPathName();
            for (auto& st : btn.states)
            {
                if (st.mainImage.isNotEmpty() && st.resolvedMainImagePath.isEmpty())
                    st.resolvedMainImagePath = SampleMapState::resolveDecentSamplerSamplePath(st.mainImage, baseLookupDir).getFullPathName();
                if (st.hoverImage.isNotEmpty() && st.resolvedHoverImagePath.isEmpty())
                    st.resolvedHoverImagePath = SampleMapState::resolveDecentSamplerSamplePath(st.hoverImage, baseLookupDir).getFullPathName();
                if (st.clickImage.isNotEmpty() && st.resolvedClickImagePath.isEmpty())
                    st.resolvedClickImagePath = SampleMapState::resolveDecentSamplerSamplePath(st.clickImage, baseLookupDir).getFullPathName();
            }
        }
    }
    for (auto& ctrl : currentState.uiControls)
    {
        if (ctrl.customSkinImagePath.isNotEmpty() && ctrl.resolvedCustomSkinImagePath.isEmpty())
        {
            ctrl.resolvedCustomSkinImagePath = SampleMapState::resolveDecentSamplerSamplePath(ctrl.customSkinImagePath, baseLookupDir).getFullPathName();
        }
    }

    // Background color
    parsedBgColor = DecentSamplerControlComponent::parseDecentSamplerColor(currentState.customUi.bgColorHex, juce::Colours::transparentBlack);

    if (currentState.customUi.width <= 0)
        currentState.customUi.width = 812;
    if (currentState.customUi.height <= 0)
        currentState.customUi.height = 375;

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

    // Apply Delay, Chorus, Filters, and ADSR Envelopes
    audioEngine.setSamplerDelay(currentState.delayTimeMs, currentState.delayFeedback, currentState.delayWetLevel);
    audioEngine.setSamplerChorus(currentState.chorusRateHz, currentState.chorusDepth, currentState.chorusWetLevel);
    audioEngine.setSamplerReverbAmount(currentState.samplerReverbAmount);
    audioEngine.setSamplerLowpassCutoff(currentState.masterFilterCutoffHz);
    audioEngine.setSamplerHighpassCutoff(currentState.masterHighpassHz);
    audioEngine.setSamplerAdsr(currentState.globalAttackMs / 1000.0f, currentState.globalDecayMs / 1000.0f, currentState.globalSustainLevel, currentState.globalReleaseMs / 1000.0f);

    // Apply control bindings for all UI controls
    for (const auto& ctrl : currentState.uiControls)
    {
        applyControlBindings(ctrl, ctrl.currentValue);
    }

    // If controls already exist for the current tab, update models, colors, and values without full teardown
    if (!controls.empty() && currentTab < static_cast<int>(currentState.customUi.tabs.size()))
    {
        const auto& tab = currentState.customUi.tabs[currentTab];
        if (controls.size() == tab.controls.size() &&
            labels.size() == tab.labels.size() &&
            buttons.size() == tab.buttons.size() &&
            menus.size() == tab.menus.size() &&
            images.size() == tab.images.size())
        {
            for (size_t i = 0; i < controls.size(); ++i)
            {
                controls[i].model = tab.controls[i];
                if (controls[i].control != nullptr)
                {
                    controls[i].control->setModel(tab.controls[i]);
                    controls[i].control->setValue(tab.controls[i].currentValue, false);
                    controls[i].control->onDragStateChanged = [this](DecentSamplerControlComponent* c, bool dragging) {
                        activeDraggingControl = dragging ? c : nullptr;
                        repaint();
                    };
                }
                applyControlBindings(controls[i].model, tab.controls[i].currentValue);
            }

            for (size_t i = 0; i < labels.size(); ++i)
            {
                labels[i].model = tab.labels[i];
                if (labels[i].label != nullptr)
                {
                    labels[i].label->setText(tab.labels[i].text, juce::dontSendNotification);
                    labels[i].label->setColour(juce::Label::textColourId,
                        DecentSamplerControlComponent::parseDecentSamplerColor(tab.labels[i].textColorHex, juce::Colours::white));
                    labels[i].label->repaint();
                }
            }

            for (size_t i = 0; i < buttons.size(); ++i)
            {
                buttons[i].model = tab.buttons[i];
                if (buttons[i].button != nullptr)
                {
                    buttons[i].button->setModel(tab.buttons[i]);
                    buttons[i].button->repaint();
                }
            }

            for (size_t i = 0; i < menus.size(); ++i)
            {
                menus[i].model = tab.menus[i];
                if (menus[i].combo != nullptr)
                {
                    menus[i].combo->setColour(juce::ComboBox::backgroundColourId,
                        DecentSamplerControlComponent::parseDecentSamplerColor(tab.menus[i].bgColorHex, juce::Colour(0xFF242730)));
                    menus[i].combo->setColour(juce::ComboBox::textColourId,
                        DecentSamplerControlComponent::parseDecentSamplerColor(tab.menus[i].textColorHex, juce::Colours::white));
                    menus[i].combo->repaint();
                }
            }

            for (size_t i = 0; i < images.size(); ++i)
            {
                images[i].model = tab.images[i];
                if (images[i].imageComp != nullptr)
                {
                    images[i].imageComp->repaint();
                }
            }

            resized();
            repaint();
            return;
        }
    }

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

        juce::Colour txtCol = DecentSamplerControlComponent::parseDecentSamplerColor(lblModel.textColorHex, juce::Colour(0xFFE8E8E8));
        item.label->setColour(juce::Label::textColourId, txtCol);

        if (lblModel.textAlignment.containsIgnoreCase("left"))
            item.label->setJustificationType(juce::Justification::centredLeft);
        else if (lblModel.textAlignment.containsIgnoreCase("right"))
            item.label->setJustificationType(juce::Justification::centredRight);
        else
            item.label->setJustificationType(juce::Justification::centred);

        float fSize = lblModel.textSize > 0.0f ? lblModel.textSize : 10.0f;
        item.label->setFont(getDecentSamplerFont(fSize, false));

        addAndMakeVisible(*item.label);
        labels.push_back(std::move(item));
    }

    // 3. Build 1:1 Decent Sampler Knobs & Sliders
    for (const auto& ctrlModel : tab.controls)
    {
        ControlItem item;
        item.model = ctrlModel;

        item.control = std::make_unique<DecentSamplerControlComponent>(ctrlModel);
        item.control->onDragStateChanged = [this](DecentSamplerControlComponent* c, bool dragging) {
            activeDraggingControl = dragging ? c : nullptr;
            repaint();
        };
        item.control->onValueChanged = [this, ctrlModel](double val) {
            // Update in global currentState
            for (auto& c : currentState.uiControls)
            {
                if (c.id == ctrlModel.id || c.label == ctrlModel.label)
                {
                    c.currentValue = val;
                }
            }
            for (auto& t : currentState.customUi.tabs)
            {
                for (auto& c : t.controls)
                {
                    if (c.id == ctrlModel.id || c.label == ctrlModel.label)
                    {
                        c.currentValue = val;
                    }
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

        item.button = std::make_unique<DecentSamplerButtonComponent>(btnModel, [this](const juce::String& path) {
            return findImageFile(path);
        });

        bool isToggle = btnModel.style.isEmpty() || btnModel.style.containsIgnoreCase("toggle") || !btnModel.states.empty();
        if (isToggle)
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

        juce::Colour menuTextCol = DecentSamplerControlComponent::parseDecentSamplerColor(menuModel.textColorHex, juce::Colours::white);
        juce::Colour menuBgCol = DecentSamplerControlComponent::parseDecentSamplerColor(menuModel.bgColorHex, juce::Colour(0xFF20232A));
        juce::Colour menuActiveCol = DecentSamplerControlComponent::parseDecentSamplerColor(menuModel.trackForegroundColorHex, juce::Colour(0xFF4A90E2));

        item.combo->setColour(juce::ComboBox::backgroundColourId, menuBgCol);
        item.combo->setColour(juce::ComboBox::textColourId, menuTextCol);
        item.combo->setColour(juce::ComboBox::outlineColourId, juce::Colour(0xFF3E4450));
        item.combo->setColour(juce::ComboBox::arrowColourId, menuActiveCol);

        for (int i = 0; i < menuModel.options.size(); ++i)
        {
            item.combo->addItem(menuModel.options[i], i + 1);
        }
        item.combo->setSelectedId(menuModel.selectedIndex + 1, juce::dontSendNotification);
        item.combo->addListener(this);
        addAndMakeVisible(*item.combo);
        menus.push_back(std::move(item));

        // Apply initial menu option selection
        applyMenuOptionSelection(menuModel, menuModel.selectedIndex);
    }

    updateChildrenMouseInterception();
    resized();
    repaint();
}

void DecentSamplerCanvasComponent::applyBinding(const DecentSamplerBinding& binding, double rawValue)
{
    double value = rawValue;

    if (binding.translation.equalsIgnoreCase("table") && binding.translationTable.isNotEmpty())
    {
        juce::StringArray tokens;
        tokens.addTokens(binding.translationTable, ", ", "\"");
        std::vector<float> tableValues;
        for (const auto& tok : tokens)
        {
            if (tok.trim().isNotEmpty())
                tableValues.push_back(tok.trim().getFloatValue());
        }
        if (!tableValues.empty())
        {
            float norm = static_cast<float>(rawValue);
            if (norm > 1.0f) norm /= 100.0f;
            norm = juce::jlimit(0.0f, 1.0f, norm);
            float indexFloat = norm * static_cast<float>(tableValues.size() - 1);
            int idx0 = juce::jlimit(0, static_cast<int>(tableValues.size()) - 1, static_cast<int>(indexFloat));
            int idx1 = juce::jlimit(0, static_cast<int>(tableValues.size()) - 1, idx0 + 1);
            float frac = indexFloat - static_cast<float>(idx0);
            value = tableValues[idx0] + frac * (tableValues[idx1] - tableValues[idx0]);
        }
    }
    else if (binding.translation.equalsIgnoreCase("fixed_value"))
    {
        value = binding.translationValue;
    }
    else if (binding.factor != 1.0f && binding.factor > 0.0f)
    {
        value *= binding.factor;
    }
    else if (binding.translation.equalsIgnoreCase("linear") && (binding.translationOutputMin != 0.0f || binding.translationOutputMax != 1.0f || value > 1.0))
    {
        if (value > 1.0) value /= 100.0;
        value = binding.translationOutputMin + value * (binding.translationOutputMax - binding.translationOutputMin);
    }
    else if (value > 1.0 && (binding.parameter.containsIgnoreCase("volume") || binding.parameter.containsIgnoreCase("gain") ||
                             (binding.type.containsIgnoreCase("amp") && !isAttackBindingParam(binding.parameter) &&
                              !isDecayBindingParam(binding.parameter) && !isSustainBindingParam(binding.parameter) &&
                              !isReleaseBindingParam(binding.parameter))))
    {
        value /= 100.0;
    }

    juce::String param = binding.parameter;
    juce::String level = binding.level;
    juce::String type = binding.type;
    int pos = binding.position;

    // 1a. Tag Level Bindings (level="tag")
    if (level.equalsIgnoreCase("tag") && binding.identifier.isNotEmpty())
    {
        for (const auto& g : currentState.groups)
        {
            if (g.tags.containsIgnoreCase(binding.identifier))
            {
                DecentSamplerBinding grpBinding = binding;
                grpBinding.level = "group";
                grpBinding.position = g.index;
                applyBinding(grpBinding, rawValue);
            }
        }
        return;
    }

    // 1b. UI Level Bindings (level="ui" or type="control")
    if (level.equalsIgnoreCase("ui") || type.equalsIgnoreCase("control"))
    {
        for (size_t i = 0; i < currentState.uiControls.size(); ++i)
        {
            if (static_cast<int>(i) == pos ||
                (binding.identifier.isNotEmpty() && (currentState.uiControls[i].id.equalsIgnoreCase(binding.identifier) || currentState.uiControls[i].label.equalsIgnoreCase(binding.identifier))))
            {
                currentState.uiControls[i].currentValue = value;
                if (i < controls.size() && controls[i].control != nullptr)
                    controls[i].control->setValue(value, false);
            }
        }
    }

    // 1c. Group Level Bindings
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
        else if (isAttackBindingParam(param))
        {
            float attMs = static_cast<float>(value);
            if (attMs <= 30.0f) attMs *= 1000.0f;
            if (pos >= 0 && pos < static_cast<int>(currentState.groups.size()))
                currentState.groups[pos].attackMs = attMs;
            for (auto& z : currentState.zones)
                if (pos < 0 || z.groupIndex == pos) z.attackMs = attMs;
            if (pos == 0 || currentState.groups.size() <= 1)
            {
                currentState.globalAttackMs = attMs;
                audioEngine.setSamplerAttackSec(attMs / 1000.0f);
            }
        }
        else if (isDecayBindingParam(param))
        {
            float decMs = static_cast<float>(value);
            if (decMs <= 30.0f) decMs *= 1000.0f;
            if (pos >= 0 && pos < static_cast<int>(currentState.groups.size()))
                currentState.groups[pos].decayMs = decMs;
            for (auto& z : currentState.zones)
                if (pos < 0 || z.groupIndex == pos) z.decayMs = decMs;
            if (pos == 0 || currentState.groups.size() <= 1)
            {
                currentState.globalDecayMs = decMs;
                audioEngine.setSamplerDecaySec(decMs / 1000.0f);
            }
        }
        else if (isSustainBindingParam(param))
        {
            float sus = static_cast<float>(value);
            if (sus > 1.0f) sus /= 100.0f;
            if (pos >= 0 && pos < static_cast<int>(currentState.groups.size()))
                currentState.groups[pos].sustainLevel = sus;
            for (auto& z : currentState.zones)
                if (pos < 0 || z.groupIndex == pos) z.sustainLevel = sus;
            if (pos == 0 || currentState.groups.size() <= 1)
            {
                currentState.globalSustainLevel = sus;
                audioEngine.setSamplerSustainLevel(sus);
            }
        }
        else if (isReleaseBindingParam(param))
        {
            float relMs = static_cast<float>(value);
            if (relMs <= 30.0f) relMs *= 1000.0f;
            if (pos >= 0 && pos < static_cast<int>(currentState.groups.size()))
                currentState.groups[pos].releaseMs = relMs;
            for (auto& z : currentState.zones)
                if (pos < 0 || z.groupIndex == pos) z.releaseMs = relMs;
            if (pos == 0 || currentState.groups.size() <= 1)
            {
                currentState.globalReleaseMs = relMs;
                audioEngine.setSamplerReleaseSec(relMs / 1000.0f);
            }
        }
        return;
    }

    // 1b. Effect Level Bindings (level="effect" or type="effect" or parameter starts with FX_)
    if (level.equalsIgnoreCase("effect") || type.equalsIgnoreCase("effect") || param.startsWithIgnoreCase("FX_"))
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
                if (eff.type.containsIgnoreCase("reverb") || eff.type.containsIgnoreCase("convolution") || eff.type.containsIgnoreCase("ir") || param.containsIgnoreCase("reverb"))
                {
                    currentState.samplerReverbAmount = wet;
                    currentState.irReverbWetLevel = wet;
                    audioEngine.setSamplerReverbAmount(wet);
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
    else if (isAttackBindingParam(param))
    {
        float attMs = static_cast<float>(value);
        if (attMs <= 30.0f) attMs *= 1000.0f;
        currentState.globalAttackMs = attMs;
        for (auto& g : currentState.groups) g.attackMs = attMs;
        for (auto& z : currentState.zones) z.attackMs = attMs;
        audioEngine.setSamplerAttackSec(attMs / 1000.0f);
    }
    else if (isDecayBindingParam(param))
    {
        float decMs = static_cast<float>(value);
        if (decMs <= 30.0f) decMs *= 1000.0f;
        currentState.globalDecayMs = decMs;
        for (auto& g : currentState.groups) g.decayMs = decMs;
        for (auto& z : currentState.zones) z.decayMs = decMs;
        audioEngine.setSamplerDecaySec(decMs / 1000.0f);
    }
    else if (isSustainBindingParam(param))
    {
        float sus = static_cast<float>(value);
        if (sus > 1.0f) sus /= 100.0f;
        currentState.globalSustainLevel = sus;
        for (auto& g : currentState.groups) g.sustainLevel = sus;
        for (auto& z : currentState.zones) z.sustainLevel = sus;
        audioEngine.setSamplerSustainLevel(sus);
    }
    else if (isReleaseBindingParam(param))
    {
        float relMs = static_cast<float>(value);
        if (relMs <= 30.0f) relMs *= 1000.0f;
        currentState.globalReleaseMs = relMs;
        for (auto& g : currentState.groups) g.releaseMs = relMs;
        for (auto& z : currentState.zones) z.releaseMs = relMs;
        audioEngine.setSamplerReleaseSec(relMs / 1000.0f);
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
        // Fallback by parameterName, label, id, or bindingParam
        DecentSamplerBinding b;
        b.parameter = ctrl.bindingParam.isNotEmpty() ? ctrl.bindingParam
                    : (ctrl.parameterName.isNotEmpty() ? ctrl.parameterName
                    : (ctrl.label.isNotEmpty() ? ctrl.label : ctrl.id));
        applyBinding(b, value);
    }
}

void DecentSamplerCanvasComponent::buttonClicked(juce::Button* button)
{
    for (auto& item : buttons)
    {
        if (item.button.get() == button)
        {
            bool isToggled = button->getToggleState();
            item.model.state = isToggled;

            if (!item.model.states.empty())
            {
                int stateIdx = isToggled ? std::min(1, static_cast<int>(item.model.states.size() - 1)) : 0;
                const auto& activeState = item.model.states[stateIdx];
                if (activeState.name.isNotEmpty())
                    item.button->setButtonText(activeState.name);

                for (const auto& b : activeState.bindings)
                {
                    applyBinding(b, 1.0);
                }
            }

            for (const auto& b : item.model.bindings)
            {
                applyBinding(b, isToggled ? 1.0 : 0.0);
            }
            break;
        }
    }

    if (onStateChanged)
        onStateChanged(currentState);
}

juce::File DecentSamplerCanvasComponent::findIrFile(const juce::String& irPathOrName) const
{
    if (irPathOrName.trim().isEmpty())
        return {};

    juce::String clean = irPathOrName.replace("\\", "/").trim();
    if (clean.startsWith("./"))
        clean = clean.substring(2);

    juce::File direct(clean);
    if (direct.existsAsFile())
        return direct;

    // Collect candidate base search directories
    std::vector<juce::File> baseDirs;
    auto addDir = [&](const juce::File& dir) {
        if (dir.exists() && dir.isDirectory())
        {
            if (std::find(baseDirs.begin(), baseDirs.end(), dir) == baseDirs.end())
                baseDirs.push_back(dir);
        }
    };

    if (currentState.irFilePath.isNotEmpty())
    {
        juce::File irF(currentState.irFilePath);
        addDir(irF.getParentDirectory());
        addDir(irF.getParentDirectory().getParentDirectory());
    }

    if (currentState.customUi.resolvedBgImagePath.isNotEmpty())
    {
        juce::File bgF(currentState.customUi.resolvedBgImagePath);
        addDir(bgF.getParentDirectory());
        addDir(bgF.getParentDirectory().getParentDirectory());
    }

    for (const auto& z : currentState.zones)
    {
        if (z.filePath.isNotEmpty())
        {
            juce::File zf(z.filePath);
            addDir(zf.getParentDirectory());
            addDir(zf.getParentDirectory().getParentDirectory());
            if (baseDirs.size() > 6) break;
        }
    }

    addDir(juce::File::getCurrentWorkingDirectory());

    // Search common subdirectories in each candidate root
    for (const auto& base : baseDirs)
    {
        juce::File c1 = base.getChildFile(clean);
        if (c1.existsAsFile()) return c1;

        juce::File c2 = base.getChildFile("ir").getChildFile(clean);
        if (c2.existsAsFile()) return c2;

        juce::File c3 = base.getChildFile("IR").getChildFile(clean);
        if (c3.existsAsFile()) return c3;

        juce::File c4 = base.getChildFile("IRs").getChildFile(clean);
        if (c4.existsAsFile()) return c4;

        juce::File c5 = base.getChildFile("Impulse Responses").getChildFile(clean);
        if (c5.existsAsFile()) return c5;

        juce::File c6 = base.getChildFile("Samples").getChildFile(clean);
        if (c6.existsAsFile()) return c6;

        juce::File c7 = base.getChildFile("samples").getChildFile(clean);
        if (c7.existsAsFile()) return c7;

        juce::File c8 = base.getChildFile("Resources").getChildFile(clean);
        if (c8.existsAsFile()) return c8;

        // Try appending audio extensions if missing
        if (!clean.endsWithIgnoreCase(".wav") && !clean.endsWithIgnoreCase(".aif") && !clean.endsWithIgnoreCase(".flac"))
        {
            juce::File w1 = base.getChildFile(clean + ".wav");
            if (w1.existsAsFile()) return w1;
            juce::File w2 = base.getChildFile("ir").getChildFile(clean + ".wav");
            if (w2.existsAsFile()) return w2;
            juce::File w3 = base.getChildFile("IR").getChildFile(clean + ".wav");
            if (w3.existsAsFile()) return w3;
            juce::File w4 = base.getChildFile("IRs").getChildFile(clean + ".wav");
            if (w4.existsAsFile()) return w4;
            juce::File w5 = base.getChildFile("Impulse Responses").getChildFile(clean + ".wav");
            if (w5.existsAsFile()) return w5;
        }

        // Recursive search for exact filename match
        juce::String fileName = juce::File(clean).getFileName();
        if (fileName.isNotEmpty())
        {
            juce::Array<juce::File> matches;
            base.findChildFiles(matches, juce::File::findFiles, true, "*" + fileName + "*");
            if (!matches.isEmpty())
                return matches[0];
        }
    }

    return {};
}

juce::File DecentSamplerCanvasComponent::findImageFile(const juce::String& imgPathOrName) const
{
    if (imgPathOrName.trim().isEmpty())
        return {};

    juce::String clean = imgPathOrName.replace("\\", "/").trim();
    if (clean.startsWith("./"))
        clean = clean.substring(2);

    juce::File direct(clean);
    if (direct.existsAsFile())
        return direct;

    juce::File baseDir = juce::File(currentState.customUi.resolvedBgImagePath.isNotEmpty() ? currentState.customUi.resolvedBgImagePath : currentState.customUi.bgImagePath);
    if (baseDir.exists())
    {
        juce::File res = SampleMapState::resolveDecentSamplerSamplePath(clean, baseDir);
        if (res.existsAsFile()) return res;
    }

    if (currentState.irFilePath.isNotEmpty())
    {
        juce::File res = SampleMapState::resolveDecentSamplerSamplePath(clean, juce::File(currentState.irFilePath));
        if (res.existsAsFile()) return res;
    }

    for (const auto& z : currentState.zones)
    {
        if (z.filePath.isNotEmpty())
        {
            juce::File res = SampleMapState::resolveDecentSamplerSamplePath(clean, juce::File(z.filePath));
            if (res.existsAsFile()) return res;
        }
    }

    return {};
}

void DecentSamplerCanvasComponent::applyMenuOptionSelection(const DecentSamplerUiMenu& menu, int selIdx)
{
    if (selIdx < 0) return;

    // 1. Process option-level bindings and IR file loading
    if (selIdx < static_cast<int>(menu.menuOptions.size()))
    {
        const auto& opt = menu.menuOptions[selIdx];

        // Check for IR file in option value, name, or bindings
        juce::String candidateIrPath;
        if (opt.value.endsWithIgnoreCase(".wav") || opt.value.endsWithIgnoreCase(".aif") || opt.value.endsWithIgnoreCase(".flac"))
            candidateIrPath = opt.value;

        for (const auto& b : opt.bindings)
        {
            if (b.parameter.equalsIgnoreCase("FX_IR_FILE") || b.parameter.containsIgnoreCase("ir") ||
                b.parameter.containsIgnoreCase("convolution") || b.parameter.containsIgnoreCase("file") ||
                b.parameter.containsIgnoreCase("path"))
            {
                juce::String irRelPath = b.translationValueStr.isNotEmpty() ? b.translationValueStr : b.identifier;
                if (irRelPath.isEmpty()) irRelPath = opt.value;
                if (irRelPath.isNotEmpty())
                    candidateIrPath = irRelPath;
            }
            else
            {
                applyBinding(b, 1.0);
            }
        }

        // Check if menu-level bindings indicate an IR selector
        if (candidateIrPath.isEmpty())
        {
            for (const auto& mb : menu.bindings)
            {
                if (mb.parameter.equalsIgnoreCase("FX_IR_FILE") || mb.parameter.containsIgnoreCase("ir") || mb.parameter.containsIgnoreCase("convolution"))
                {
                    candidateIrPath = opt.value.isNotEmpty() ? opt.value : opt.name;
                    break;
                }
            }
        }

        // Load new IR WAV into convolver
        if (candidateIrPath.isNotEmpty())
        {
            juce::File irFile = findIrFile(candidateIrPath);
            if (irFile.existsAsFile())
            {
                currentState.irFilePath = irFile.getFullPathName();
                audioEngine.loadImpulseResponseFile(irFile);

                float currentWet = (currentState.irReverbWetLevel > 0.001f) ? currentState.irReverbWetLevel : currentState.samplerReverbAmount;
                if (currentWet <= 0.001f) currentWet = 0.4f;

                currentState.irReverbWetLevel = currentWet;
                currentState.samplerReverbAmount = currentWet;
                audioEngine.setSamplerIrReverbAmount(currentWet);
                audioEngine.setSamplerReverbAmount(currentWet);
                audioEngine.setSamplerIrReverbDryLevel(currentState.irReverbDryLevel > 0.001f ? currentState.irReverbDryLevel : 1.0f);
            }
        }
    }

    // 2. Process menu-level bindings
    for (const auto& b : menu.bindings)
    {
        applyBinding(b, static_cast<double>(selIdx));
    }
}

void DecentSamplerCanvasComponent::comboBoxChanged(juce::ComboBox* comboBox)
{
    for (auto& item : menus)
    {
        if (item.combo.get() == comboBox)
        {
            item.model.selectedIndex = comboBox->getSelectedId() - 1;
            int selIdx = item.model.selectedIndex;

            // Sync into currentState tabs
            for (auto& t : currentState.customUi.tabs)
            {
                for (auto& m : t.menus)
                {
                    if (m.options == item.model.options)
                    {
                        m.selectedIndex = selIdx;
                    }
                }
            }

            applyMenuOptionSelection(item.model, selIdx);
            break;
        }
    }

    if (onStateChanged)
        onStateChanged(currentState);
}

void DecentSamplerCanvasComponent::timerCallback()
{
    if (editMode) return;

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

void DecentSamplerCanvasComponent::handleNoteOn(juce::MidiKeyboardState*, int, int, float)
{
    juce::MessageManager::callAsync([this]() {
        repaint(getKeyboardBounds().toNearestInt());
    });
}

void DecentSamplerCanvasComponent::handleNoteOff(juce::MidiKeyboardState*, int, int, float)
{
    juce::MessageManager::callAsync([this]() {
        repaint(getKeyboardBounds().toNearestInt());
    });
}

void DecentSamplerCanvasComponent::scrollKeyboardOctave(int deltaOctaves)
{
    int newIndex = startWhiteKeyIndex + deltaOctaves * 7;
    int maxIndex = 75 - kNumVisibleWhiteKeys;
    newIndex = juce::jlimit(0, maxIndex, newIndex);
    if (newIndex != startWhiteKeyIndex)
    {
        startWhiteKeyIndex = newIndex;
        repaint(getKeyboardBounds().toNearestInt());
    }
}

void DecentSamplerCanvasComponent::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    auto kbRect = getKeyboardBounds();
    if (kbRect.contains(e.position))
    {
        if (wheel.deltaY > 0.05f || wheel.deltaX < -0.05f)
        {
            scrollKeyboardOctave(-1);
            return;
        }
        else if (wheel.deltaY < -0.05f || wheel.deltaX > 0.05f)
        {
            scrollKeyboardOctave(1);
            return;
        }
    }
    juce::Component::mouseWheelMove(e, wheel);
}

void DecentSamplerCanvasComponent::mouseExit(const juce::MouseEvent&)
{
    if (isHoveringLeftArrow || isHoveringRightArrow)
    {
        isHoveringLeftArrow = false;
        isHoveringRightArrow = false;
        repaint(getKeyboardBounds().toNearestInt());
    }
}

} // namespace openwav
