#include "SampleTableComponent.h"
#include "OpenWavLookAndFeel.h"
#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
#endif


namespace openwav
{

SampleTableComponent::SampleTableComponent(TagDatabaseManager& db, AudioEngine& engine)
    : dbManager(db), audioEngine(engine)
{
    dbManager.addListener(this);
    audioEngine.addListener(this);

    table.setHeader(std::make_unique<juce::TableHeaderComponent>());
    auto& header = table.getHeader();
    header.addColumn("", 1, 36, 36, 36, juce::TableHeaderComponent::notResizable);
    header.addColumn("File Name", 2, 220, 100, 600);
    header.addColumn("Tags", 3, 230, 100, 600);
    header.addColumn("Duration", 4, 70, 50, 120);
    header.addColumn("Format", 5, 65, 50, 100);
    header.addColumn("Sample Rate", 6, 95, 70, 150);
    header.addColumn("Rating", 7, 75, 60, 100);
    header.addColumn("Fav", 8, 40, 40, 40, juce::TableHeaderComponent::notResizable);

    table.setModel(this);
    table.setRowHeight(32);
    table.setMultipleSelectionEnabled(false);
    table.setWantsKeyboardFocus(true);
    setOpaque(true);
    table.setOpaque(true);
    table.setOutlineThickness(0);
    header.setOpaque(true);

    table.onScrolled = [this] {
        checkAndLoadMoreRows();
    };

    if (auto* vp = table.getViewport())
    {
        vp->setOpaque(true);
        vp->setScrollBarsShown(true, false);
        vp->setRepaintsOnMouseActivity(false);
        vp->getVerticalScrollBar().addListener(this);
    }

    similarityBannerLabel.setFont(badgeFont);
    similarityBannerLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textPrimary);
    addChildComponent(similarityBannerLabel);

    clearSimilarityButton.setButtonText("X");
    clearSimilarityButton.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::accentCyan.withAlpha(0.25f));
    clearSimilarityButton.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::accentCyan);
    clearSimilarityButton.onClick = [this] {
        similarityTargetId = "";
        similarityTargetName = "";
        updateFilter(currentKeyword, currentSelectedTags, currentMatchAll, currentExtFilter, currentFavOnly);
    };
    addChildComponent(clearSimilarityButton);

    addAndMakeVisible(table);
    buildIconCache();
    updateFilter("", {}, false, "All", false);
}

SampleTableComponent::~SampleTableComponent()
{
    dbManager.removeListener(this);
    audioEngine.removeListener(this);
    if (auto* vp = table.getViewport())
    {
        vp->getVerticalScrollBar().removeListener(this);
    }
    table.setModel(nullptr);
}

void SampleTableComponent::sampleLoaded(const juce::String& filePath)
{
    cachedCurrentFilePath = filePath;
    cachedIsPlaying = audioEngine.isPlaying();
    table.repaint();
}

void SampleTableComponent::playbackStateChanged(bool isPlaying)
{
    cachedIsPlaying = isPlaying;
    table.repaint();
}

void SampleTableComponent::scrollBarMoved(juce::ScrollBar* scrollBar, double /*newRangeStart*/)
{
    if (auto* vp = table.getViewport())
    {
        if (scrollBar == &vp->getVerticalScrollBar())
        {
            table.syncScrollPosition();
            checkAndLoadMoreRows();
        }
    }
}

void SampleTableComponent::checkAndLoadMoreRows()
{
    auto* vp = table.getViewport();
    if (vp == nullptr) return;

    auto& sb = vp->getVerticalScrollBar();
    double currentPos = sb.getCurrentRangeStart();
    double viewSize = sb.getCurrentRangeSize();
    double maxLimit = sb.getMaximumRangeLimit();
    double maxScroll = maxLimit - viewSize;

    // Load next chunk when user scrolls near the bottom (70% of currently rendered range)
    if (maxScroll <= 0.0 || currentPos >= maxScroll * 0.70)
    {
        int total = static_cast<int>(allFilteredItems.size());
        if (renderedItemCount < total)
        {
            renderedItemCount = std::min(total, renderedItemCount + RenderChunkIncrement);
            table.updateContent();
        }
    }
}

void SampleTableComponent::lookAndFeelChanged()
{
    buildIconCache();
    repaint();
}

void SampleTableComponent::buildIconCache()
{
    const float scale = 2.0f; // 2x Retina rendering for crystal clarity

    // Play Icon
    {
        int w = 24, h = 24;
        playIconImage = juce::Image(juce::Image::ARGB, static_cast<int>(w * scale), static_cast<int>(h * scale), true);
        juce::Graphics g(playIconImage);
        g.addTransform(juce::AffineTransform::scale(scale));
        g.setColour(OpenWavLookAndFeel::textSecondary);
        juce::Path p;
        p.addTriangle(w * 0.38f, h * 0.28f,
                      w * 0.38f, h * 0.72f,
                      w * 0.68f, h * 0.50f);
        g.fillPath(p);
    }

    // Pause Icon
    {
        int w = 24, h = 24;
        pauseIconImage = juce::Image(juce::Image::ARGB, static_cast<int>(w * scale), static_cast<int>(h * scale), true);
        juce::Graphics g(pauseIconImage);
        g.addTransform(juce::AffineTransform::scale(scale));
        g.setColour(OpenWavLookAndFeel::accentCyan);
        juce::Path p;
        p.addRectangle(w * 0.35f, h * 0.28f, 3.0f, h * 0.44f);
        p.addRectangle(w * 0.55f, h * 0.28f, 3.0f, h * 0.44f);
        g.fillPath(p);
    }

    // Favorite Heart Active (Solid Red)
    {
        int w = 18, h = 18;
        heartActiveImage = juce::Image(juce::Image::ARGB, static_cast<int>(w * scale), static_cast<int>(h * scale), true);
        juce::Graphics g(heartActiveImage);
        g.addTransform(juce::AffineTransform::scale(scale));

        juce::Path heart;
        float x = 2.0f, y = 2.0f, hw = 14.0f, hh = 14.0f;
        heart.startNewSubPath(x + hw * 0.5f, y + hh * 0.85f);
        heart.cubicTo(x + hw * 0.1f, y + hh * 0.5f,  x,              y + hh * 0.2f,  x + hw * 0.25f, y + hh * 0.05f);
        heart.cubicTo(x + hw * 0.45f, y - hh * 0.05f, x + hw * 0.5f,  y + hh * 0.2f,  x + hw * 0.5f,  y + hh * 0.25f);
        heart.cubicTo(x + hw * 0.5f,  y + hh * 0.2f,  x + hw * 0.55f, y - hh * 0.05f, x + hw * 0.75f, y + hh * 0.05f);
        heart.cubicTo(x + hw,         y + hh * 0.2f,  x + hw * 0.9f,  y + hh * 0.5f,  x + hw * 0.5f,  y + hh * 0.85f);
        heart.closeSubPath();

        g.setColour(OpenWavLookAndFeel::favoriteRed);
        g.fillPath(heart);
    }

    // Favorite Heart Inactive (Outlined)
    {
        int w = 18, h = 18;
        heartInactiveImage = juce::Image(juce::Image::ARGB, static_cast<int>(w * scale), static_cast<int>(h * scale), true);
        juce::Graphics g(heartInactiveImage);
        g.addTransform(juce::AffineTransform::scale(scale));

        juce::Path heart;
        float x = 2.0f, y = 2.0f, hw = 14.0f, hh = 14.0f;
        heart.startNewSubPath(x + hw * 0.5f, y + hh * 0.85f);
        heart.cubicTo(x + hw * 0.1f, y + hh * 0.5f,  x,              y + hh * 0.2f,  x + hw * 0.25f, y + hh * 0.05f);
        heart.cubicTo(x + hw * 0.45f, y - hh * 0.05f, x + hw * 0.5f,  y + hh * 0.2f,  x + hw * 0.5f,  y + hh * 0.25f);
        heart.cubicTo(x + hw * 0.5f,  y + hh * 0.2f,  x + hw * 0.55f, y - hh * 0.05f, x + hw * 0.75f, y + hh * 0.05f);
        heart.cubicTo(x + hw,         y + hh * 0.2f,  x + hw * 0.9f,  y + hh * 0.5f,  x + hw * 0.5f,  y + hh * 0.85f);
        heart.closeSubPath();

        g.setColour(OpenWavLookAndFeel::textSecondary.withAlpha(0.35f));
        g.strokePath(heart, juce::PathStrokeType(1.2f));
    }

    // Rating Stars (Pre-rendered rows of 5 stars for rating 0 to 5)
    {
        int totalW = 64;
        int totalH = 18;
        float starSize = 10.0f;
        float starSpacing = 3.5f;

        juce::Path baseStar;
        for (int i = 0; i < 5; ++i)
        {
            float outerAngle = i * juce::MathConstants<float>::twoPi / 5.0f - juce::MathConstants<float>::halfPi;
            float innerAngle = outerAngle + juce::MathConstants<float>::pi / 5.0f;
            float rOuter = starSize * 0.5f;
            float rInner = rOuter * 0.42f;

            float ox = std::cos(outerAngle) * rOuter;
            float oy = std::sin(outerAngle) * rOuter;
            float ix = std::cos(innerAngle) * rInner;
            float iy = std::sin(innerAngle) * rInner;

            if (i == 0) baseStar.startNewSubPath(ox, oy);
            else baseStar.lineTo(ox, oy);
            baseStar.lineTo(ix, iy);
        }
        baseStar.closeSubPath();

        for (int r = 0; r <= 5; ++r)
        {
            starRatingImages[r] = juce::Image(juce::Image::ARGB, static_cast<int>(totalW * scale), static_cast<int>(totalH * scale), true);
            juce::Graphics g(starRatingImages[r]);
            g.addTransform(juce::AffineTransform::scale(scale));

            float cy = totalH * 0.5f;
            float startX = 0.0f;

            for (int s = 1; s <= 5; ++s)
            {
                float sx = startX + (s - 1) * (starSize + starSpacing) + starSize * 0.5f;
                juce::Path currentStar = baseStar;
                currentStar.applyTransform(juce::AffineTransform::translation(sx, cy));

                if (s <= r)
                {
                    g.setColour(juce::Colour::fromRGB(255, 200, 45)); // Vibrant gold
                    g.fillPath(currentStar);
                }
                else
                {
                    g.setColour(OpenWavLookAndFeel::textSecondary.withAlpha(0.25f));
                    g.strokePath(currentStar, juce::PathStrokeType(1.0f));
                }
            }
        }
    }
}

void SampleTableComponent::paint(juce::Graphics& g)
{
    g.fillAll(OpenWavLookAndFeel::bgDark);

    if (similarityTargetId.isNotEmpty())
    {
        auto bannerArea = getLocalBounds().removeFromTop(30).reduced(4, 2).toFloat();
        
        juce::Colour baseColor = OpenWavLookAndFeel::accentCyan;
        juce::ColourGradient grad(baseColor.withAlpha(0.25f), bannerArea.getTopLeft(), baseColor.withAlpha(0.05f), bannerArea.getBottomRight(), false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(bannerArea, 6.0f);
        
        g.setColour(baseColor.withAlpha(0.5f));
        g.drawRoundedRectangle(bannerArea, 6.0f, 1.0f);
    }
}

void SampleTableComponent::resized()
{
    auto area = getLocalBounds();
    if (similarityTargetId.isNotEmpty())
    {
        auto bannerRow = area.removeFromTop(30).reduced(4, 2);
        similarityBannerLabel.setVisible(true);
        clearSimilarityButton.setVisible(true);
        clearSimilarityButton.setBounds(bannerRow.removeFromRight(26));
        similarityBannerLabel.setBounds(bannerRow);
    }
    else
    {
        similarityBannerLabel.setVisible(false);
        clearSimilarityButton.setVisible(false);
    }

    table.setBounds(area);
    
    auto& header = table.getHeader();
    int totalWidth = area.getWidth();
    
    int fixedWidths = 76; 
    if (similarityTargetId.isNotEmpty()) fixedWidths += 80;
    
    int availableWidth = totalWidth - fixedWidths;
    if (availableWidth > 100)
    {
        double scale = static_cast<double>(availableWidth) / 755.0;
        
        header.setColumnWidth(2, static_cast<int>(220 * scale));
        header.setColumnWidth(3, static_cast<int>(230 * scale));
        header.setColumnWidth(4, static_cast<int>(70 * scale));
        header.setColumnWidth(5, static_cast<int>(65 * scale));
        header.setColumnWidth(6, static_cast<int>(95 * scale));
        header.setColumnWidth(7, static_cast<int>(75 * scale));
        if (similarityTargetId.isNotEmpty()) header.setColumnWidth(9, 80);
    }
}

void SampleTableComponent::updateFilter(const juce::String& searchKeyword,
                                         const std::set<juce::String>& selectedTags,
                                         bool matchAllTags,
                                         const juce::String& extensionFilter,
                                         bool favoritesOnly)
{
    if (currentKeyword != searchKeyword ||
        currentSelectedTags != selectedTags ||
        currentMatchAll != matchAllTags ||
        currentExtFilter != extensionFilter ||
        currentFavOnly != favoritesOnly)
    {
        similarityTargetId = "";
    }

    currentKeyword = searchKeyword;
    currentSelectedTags = selectedTags;
    currentMatchAll = matchAllTags;
    currentExtFilter = extensionFilter;
    currentFavOnly = favoritesOnly;

    allFilteredItems = dbManager.getFilteredItems(currentKeyword, currentSelectedTags, currentMatchAll, currentExtFilter, currentFavOnly);

    if (similarityTargetId.isNotEmpty())
    {
        MediaItem targetItem;
        bool found = false;
        for (const auto& item : allFilteredItems)
        {
            if (item.id == similarityTargetId)
            {
                targetItem = item;
                found = true;
                break;
            }
        }
        if (!found)
        {
            auto allItems = dbManager.getAllItems();
            for (const auto& item : allItems)
            {
                if (item.id == similarityTargetId)
                {
                    targetItem = item;
                    found = true;
                    break;
                }
            }
        }

        if (found)
        {
            cachedSimilarityTargetItem = targetItem;
            similarityTargetName = targetItem.fileName;
            similarityBannerLabel.setText("SHOWING SIMILAR SOUNDS TO: " + similarityTargetName, juce::dontSendNotification);

            std::vector<MediaItem> thresholdedItems;
            for (const auto& item : allFilteredItems)
            {
                if (item.id == targetItem.id || TagDatabaseManager::calculateMatchPercentage(targetItem, item) >= 60.0f)
                {
                    thresholdedItems.push_back(item);
                }
            }
            allFilteredItems = thresholdedItems;

            std::sort(allFilteredItems.begin(), allFilteredItems.end(), [targetItem](const MediaItem& a, const MediaItem& b) {
                if (a.id == targetItem.id) return true;
                if (b.id == targetItem.id) return false;

                double distA = TagDatabaseManager::calculateAcousticDistance(a, targetItem);
                double distB = TagDatabaseManager::calculateAcousticDistance(b, targetItem);
                return distA < distB;
            });
        }
        else
        {
            similarityTargetId = "";
            similarityTargetName = "";
            std::sort(allFilteredItems.begin(), allFilteredItems.end(), [](const MediaItem& a, const MediaItem& b) {
                return a.fileName.compareIgnoreCase(b.fileName) < 0;
            });
        }
    }
    else
    {
        similarityTargetName = "";
        std::sort(allFilteredItems.begin(), allFilteredItems.end(), [](const MediaItem& a, const MediaItem& b) {
            return a.fileName.compareIgnoreCase(b.fileName) < 0;
        });
    }

    if (similarityTargetId.isNotEmpty())
    {
        if (table.getHeader().getColumnName(9) != "Match %")
            table.getHeader().addColumn("Match %", 9, 80, 60, 100);
    }
    else
    {
        if (table.getHeader().getColumnName(9) == "Match %")
            table.getHeader().removeColumn(9);
    }

    // Reset rendered chunk to InitialRenderChunk for fast initial rendering
    renderedItemCount = std::min(static_cast<int>(allFilteredItems.size()), InitialRenderChunk);

    resized();

    table.updateContent();
    table.repaint();

    if (similarityTargetId.isNotEmpty())
    {
        table.smoothScrollToRow(0);
    }

    listeners.call([this](SampleTableListener& l) {
        l.displayedItemsChanged(allFilteredItems);
    });
}

int SampleTableComponent::getNumRows()
{
    return std::min(renderedItemCount, static_cast<int>(allFilteredItems.size()));
}

void SampleTableComponent::paintRowBackground(juce::Graphics& g, int rowNumber, int /*width*/, int height, bool rowIsSelected)
{
    if (rowIsSelected)
    {
        g.fillAll(OpenWavLookAndFeel::accentCyan.withAlpha(0.18f));
        g.setColour(OpenWavLookAndFeel::accentCyan);
        g.fillRect(0, 0, 3, height);
    }
    else
    {
        if (rowNumber % 2 == 1)
            g.fillAll(OpenWavLookAndFeel::bgDark.withAlpha(0.35f));
        else
            g.fillAll(OpenWavLookAndFeel::bgCard);
    }
}

void SampleTableComponent::paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(allFilteredItems.size()))
        return;

    const auto& item = allFilteredItems[static_cast<size_t>(rowNumber)];
    auto bounds = juce::Rectangle<int>(0, 0, width, height).reduced(6, 0);

    // Auto-stream more rows if user scrolls near the end of the rendered batch
    if (rowNumber >= renderedItemCount - 25 && renderedItemCount < static_cast<int>(allFilteredItems.size()))
    {
        juce::MessageManager::callAsync([this]() {
            int total = static_cast<int>(allFilteredItems.size());
            if (renderedItemCount < total)
            {
                renderedItemCount = std::min(total, renderedItemCount + RenderChunkIncrement);
                table.updateContent();
            }
        });
    }

    g.setFont(cellFont);

    switch (columnId)
    {
        case 1: // Play status / Icon button
        {
            bool isCurrentFile = (cachedCurrentFilePath == item.filePath);
            bool isPlaying = isCurrentFile && cachedIsPlaying;
            const auto& img = isPlaying ? pauseIconImage : playIconImage;
            if (img.isValid())
            {
                int iconW = img.getWidth() / 2;
                int iconH = img.getHeight() / 2;
                int x = bounds.getX() + (bounds.getWidth() - iconW) / 2;
                int y = bounds.getY() + (bounds.getHeight() - iconH) / 2;
                g.drawImage(img, x, y, iconW, iconH, 0, 0, img.getWidth(), img.getHeight());
            }
            break;
        }

        case 2: // File Name
        {
            g.setColour(rowIsSelected ? OpenWavLookAndFeel::accentCyan : OpenWavLookAndFeel::textPrimary);
            g.setFont(rowIsSelected ? cellBoldFont : cellFont);

            if (similarityTargetId.isNotEmpty() && cachedSimilarityTargetItem.id == similarityTargetId && item.id == similarityTargetId)
            {
                juce::String badgeText = "TARGET";
                int badgeW = juce::roundToInt(badgeFont.getStringWidthFloat(badgeText)) + 10;
                auto targetBounds = bounds.removeFromRight(badgeW).toFloat().withHeight(16.0f);
                targetBounds.setY((height - 16.0f) * 0.5f);

                g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.25f));
                g.fillRoundedRectangle(targetBounds, 4.0f);
                g.setColour(OpenWavLookAndFeel::accentCyan);
                g.drawRoundedRectangle(targetBounds, 4.0f, 1.0f);
                g.setFont(badgeFont);
                g.drawText(badgeText, targetBounds, juce::Justification::centred, true);

                bounds.removeFromRight(6);
            }

            g.drawText(item.fileName, bounds, juce::Justification::centredLeft, true);
            break;
        }

        case 3: // Tags Pill Badges
        {
            float tagX = static_cast<float>(bounds.getX());
            float tagY = static_cast<float>(bounds.getY() + 4);
            float tagHeight = static_cast<float>(height - 8);

            g.setFont(tagFont);
            if (!item.cachedTags.empty())
            {
                for (const auto& tagInfo : item.cachedTags)
                {
                    float tagWidth = static_cast<float>(tagInfo.width);
                    if (tagX + tagWidth > bounds.getRight()) break;

                    auto pillBounds = juce::Rectangle<float>(tagX, tagY, tagWidth, tagHeight);

                    g.setColour(OpenWavLookAndFeel::bgCard);
                    g.fillRoundedRectangle(pillBounds, 3.0f);

                    g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.5f));
                    g.drawRoundedRectangle(pillBounds, 3.0f, 1.0f);

                    g.setColour(OpenWavLookAndFeel::textPrimary);
                    g.drawText(tagInfo.tag, pillBounds, juce::Justification::centred, true);

                    tagX += tagWidth + 4.0f;
                }
            }
            else
            {
                for (const auto& tag : item.tags)
                {
                    float tagWidth = tagFont.getStringWidthFloat(tag) + 12.0f;
                    if (tagX + tagWidth > bounds.getRight()) break;

                    auto pillBounds = juce::Rectangle<float>(tagX, tagY, tagWidth, tagHeight);

                    g.setColour(OpenWavLookAndFeel::bgCard);
                    g.fillRoundedRectangle(pillBounds, 3.0f);

                    g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.5f));
                    g.drawRoundedRectangle(pillBounds, 3.0f, 1.0f);

                    g.setColour(OpenWavLookAndFeel::textPrimary);
                    g.drawText(tag, pillBounds, juce::Justification::centred, true);

                    tagX += tagWidth + 4.0f;
                }
            }
            break;
        }

        case 4: // Duration
        {
            g.setColour(OpenWavLookAndFeel::textSecondary);
            g.drawText(item.cachedFormattedDuration.isNotEmpty() ? item.cachedFormattedDuration : formatDuration(item.durationSeconds), bounds, juce::Justification::centredLeft, true);
            break;
        }

        case 5: // Format
        {
            g.setColour(OpenWavLookAndFeel::accentCyan);
            g.drawText(item.cachedUppercaseExtension.isNotEmpty() ? item.cachedUppercaseExtension : item.fileExtension.toUpperCase(), bounds, juce::Justification::centredLeft, true);
            break;
        }

        case 6: // Sample Rate / Bit Depth
        {
            g.setColour(OpenWavLookAndFeel::textSecondary);
            if (item.cachedFormattedSampleRate.isNotEmpty())
            {
                g.drawText(item.cachedFormattedSampleRate, bounds, juce::Justification::centredLeft, true);
            }
            else
            {
                juce::String srStr = juce::String(item.sampleRate / 1000.0, 1) + " kHz";
                if (item.bitDepth > 0) srStr += " / " + juce::String(item.bitDepth) + "b";
                g.drawText(srStr, bounds, juce::Justification::centredLeft, true);
            }
            break;
        }

        case 7: // Rating Stars (Pre-rendered instantaneous cached blit)
        {
            int r = juce::jlimit(0, 5, item.rating);
            const auto& img = starRatingImages[r];
            if (img.isValid())
            {
                int imgW = img.getWidth() / 2;
                int imgH = img.getHeight() / 2;
                int x = bounds.getX() + (bounds.getWidth() - imgW) / 2;
                int y = bounds.getY() + (bounds.getHeight() - imgH) / 2;
                g.drawImage(img, x, y, imgW, imgH, 0, 0, img.getWidth(), img.getHeight());
            }
            break;
        }

        case 8: // Favorite Heart (Pre-rendered instantaneous cached blit)
        {
            const auto& img = item.isFavorite ? heartActiveImage : heartInactiveImage;
            if (img.isValid())
            {
                int imgW = img.getWidth() / 2;
                int imgH = img.getHeight() / 2;
                int x = bounds.getX() + (bounds.getWidth() - imgW) / 2;
                int y = bounds.getY() + (bounds.getHeight() - imgH) / 2;
                g.drawImage(img, x, y, imgW, imgH, 0, 0, img.getWidth(), img.getHeight());
            }
            break;
        }

        case 9: // Match % (Zero-allocation direct paint)
        {
            if (similarityTargetId.isNotEmpty() && item.id != similarityTargetId)
            {
                float matchPct = TagDatabaseManager::calculateMatchPercentage(cachedSimilarityTargetItem, item);
                float matchRatio = juce::jlimit(0.0f, 1.0f, matchPct / 100.0f);

                float size = std::min(width, height) - 8.0f;
                auto ringBounds = juce::Rectangle<float>(0, 0, size, size).withCentre(bounds.getCentre().toFloat());

                g.setColour(OpenWavLookAndFeel::bgDark.withAlpha(0.6f));
                g.drawEllipse(ringBounds, 2.0f);

                if (matchRatio > 0.01f)
                {
                    juce::Path arc;
                    arc.addCentredArc(ringBounds.getCentreX(), ringBounds.getCentreY(), ringBounds.getWidth() * 0.5f, ringBounds.getHeight() * 0.5f, 0.0f, 0.0f, juce::MathConstants<float>::twoPi * matchRatio, true);
                    g.setColour(OpenWavLookAndFeel::accentCyan);
                    g.strokePath(arc, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                }

                juce::String pctStr = juce::String(juce::roundToInt(matchPct));
                g.setColour(OpenWavLookAndFeel::textPrimary);
                g.setFont(badgeFont);
                g.drawText(pctStr, ringBounds.toNearestInt(), juce::Justification::centred, false);
            }
            break;
        }

        default:
            break;
    }
}

juce::String SampleTableComponent::getCellTooltip(int rowNumber, int columnId)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(allFilteredItems.size()))
        return {};

    if (similarityTargetId.isNotEmpty() && cachedSimilarityTargetItem.id == similarityTargetId)
    {
        const auto& item = allFilteredItems[static_cast<size_t>(rowNumber)];
        if (item.id == similarityTargetId)
            return "This is the target sample.";

        if (columnId == 9 || columnId == 2)
        {
            const auto& a = item;
            const auto& b = cachedSimilarityTargetItem;
            
            double d_zcr = std::abs(a.zcr - b.zcr) * 2.2;
            double d_hfr = std::abs(a.highFreqRatio - b.highFreqRatio) * 1.5;
            double d_dr = std::abs(a.decayRatio - b.decayRatio) * 1.8;
            
            juce::String tooltip = "Acoustic Similarity Breakdown:\n";
            if (d_zcr < 0.25) tooltip += "- Similar Zero-Crossing Rate (Pitch/Noise)\n";
            if (d_hfr < 0.25) tooltip += "- Similar High Frequency Content\n";
            if (d_dr < 0.25) tooltip += "- Similar Decay/Envelope Profile\n";
            
            int sharedTags = 0;
            for (const auto& t : a.tags)
                if (b.tags.find(t) != b.tags.end() && !t.endsWithIgnoreCase("BPM") && !t.startsWithIgnoreCase("#Key_"))
                    sharedTags++;
                    
            if (sharedTags > 0) tooltip += "- Shares " + juce::String(sharedTags) + " common tag(s)\n";

            if (tooltip == "Acoustic Similarity Breakdown:\n")
                tooltip += "- General acoustic profile match";

            return tooltip.trim();
        }
    }

    return {};
}

juce::Component* SampleTableComponent::refreshComponentForCell(int /*rowNumber*/, int /*columnId*/, bool /*isRowSelected*/, juce::Component* existingComponentToUpdate)
{
    delete existingComponentToUpdate;
    return nullptr;
}

void SampleTableComponent::cellClicked(int rowNumber, int columnId, const juce::MouseEvent& e)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(allFilteredItems.size()))
        return;

    table.grabKeyboardFocus();

    const auto& item = allFilteredItems[static_cast<size_t>(rowNumber)];

    if (e.mods.isPopupMenu())
    {
        showContextMenuForRow(rowNumber);
        return;
    }

    if (columnId == 8) // Favorite Heart Toggle (Instant O(1) update)
    {
        bool newFav = !allFilteredItems[static_cast<size_t>(rowNumber)].isFavorite;
        allFilteredItems[static_cast<size_t>(rowNumber)].isFavorite = newFav;
        dbManager.toggleFavorite(item.id);

        if (currentFavOnly && !newFav)
        {
            allFilteredItems.erase(allFilteredItems.begin() + rowNumber);
            table.updateContent();
        }
        else
        {
            table.repaintRow(rowNumber);
        }
        return;
    }

    if (columnId == 7) // Rating Stars Direct Click (Instant O(1) update)
    {
        int colIndex = table.getHeader().getIndexOfColumnId(7, true);
        int colX = (colIndex >= 0) ? table.getHeader().getColumnPosition(colIndex).getX() : 0;
        int colW = table.getHeader().getColumnWidth(7);
        float localX = static_cast<float>(e.x - colX);

        int imgW = 64;
        float startX = (colW - imgW) * 0.5f;
        float relX = localX - startX;
        float step = static_cast<float>(imgW) / 5.0f;

        int clickedStar = 1;
        if (relX < step)
        {
            clickedStar = 1;
        }
        else
        {
            clickedStar = static_cast<int>(relX / step) + 1;
            clickedStar = juce::jlimit(1, 5, clickedStar);
        }

        int newRating = (item.rating == clickedStar) ? 0 : clickedStar;
        allFilteredItems[static_cast<size_t>(rowNumber)].rating = newRating;
        allFilteredItems[static_cast<size_t>(rowNumber)].precomputeCachedStrings();

        dbManager.setRating(item.id, newRating);
        table.repaintRow(rowNumber);
        return;
    }

    if (table.getSelectedRow() == rowNumber)
    {
        juce::File fileToLoad(item.filePath);
        if (fileToLoad.existsAsFile())
        {
            audioEngine.setSampleBpm(item.bpm);
            audioEngine.loadFile(fileToLoad, true);
        }

        listeners.call([item](SampleTableListener& l) {
            l.sampleSelected(item);
        });
    }
    else
    {
        table.selectRow(rowNumber);
    }
}

void SampleTableComponent::selectedRowsChanged(int lastRowSelected)
{
    if (lastRowSelected >= 0 && lastRowSelected < static_cast<int>(allFilteredItems.size()))
    {
        const auto& item = allFilteredItems[static_cast<size_t>(lastRowSelected)];
        juce::File fileToLoad(item.filePath);
        
        if (fileToLoad.existsAsFile())
        {
            audioEngine.setSampleBpm(item.bpm);
            audioEngine.loadFile(fileToLoad, true);
        }

        listeners.call([item](SampleTableListener& l) {
            l.sampleSelected(item);
        });
    }
}

void SampleTableComponent::moveSelection(int delta)
{
    int current = table.getSelectedRow();
    int totalItems = static_cast<int>(allFilteredItems.size());
    if (totalItems <= 0)
        return;

    int nextRow = current + delta;
    if (nextRow < 0) nextRow = 0;
    if (nextRow >= totalItems) nextRow = totalItems - 1;

    if (nextRow >= renderedItemCount)
    {
        renderedItemCount = std::min(totalItems, nextRow + RenderChunkIncrement);
        table.updateContent();
    }

    table.selectRow(nextRow);
    table.smoothScrollToRow(nextRow);
}

void SampleTableComponent::selectItemById(const juce::String& itemId)
{
    if (itemId.isEmpty())
        return;

    for (size_t i = 0; i < allFilteredItems.size(); ++i)
    {
        if (allFilteredItems[i].id == itemId)
        {
            int row = static_cast<int>(i);
            if (row >= renderedItemCount)
            {
                renderedItemCount = std::min(static_cast<int>(allFilteredItems.size()), row + RenderChunkIncrement);
                table.updateContent();
            }

            table.selectRow(row);
            table.smoothScrollToRow(row);

            juce::File fileToLoad(allFilteredItems[i].filePath);
            if (fileToLoad.existsAsFile())
            {
                audioEngine.setSampleBpm(allFilteredItems[i].bpm);
                audioEngine.loadFile(fileToLoad, true);
            }
            break;
        }
    }
}

void SampleTableComponent::cellDoubleClicked(int rowNumber, int /*columnId*/, const juce::MouseEvent& /*e*/)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(allFilteredItems.size()))
        return;

    const auto& item = allFilteredItems[static_cast<size_t>(rowNumber)];
    if (audioEngine.getCurrentFile().getFullPathName() == item.filePath && audioEngine.isPlaying())
    {
        audioEngine.pause();
    }
    else
    {
        audioEngine.loadFile(juce::File(item.filePath), true);
    }

    listeners.call([item](SampleTableListener& l) {
        l.sampleDoubleClicked(item);
    });
}

juce::var SampleTableComponent::getDragSourceDescription(const juce::SparseSet<int>& selectedRows)
{
#if JUCE_WINDOWS
    bool isLeftCtrl = (GetKeyState(VK_LCONTROL) & 0x8000) != 0;
#else
    bool isLeftCtrl = juce::ModifierKeys::getCurrentModifiersRealtime().isCtrlDown();
#endif

    if (!isLeftCtrl)
        return {};

    if (selectedRows.size() > 0)
    {
        int r = selectedRows[0];
        if (r >= 0 && r < static_cast<int>(allFilteredItems.size()))
        {
            if (juce::Desktop::getInstance().getMainMouseSource().isDragging())
            {
                juce::StringArray files;
                files.add(allFilteredItems[static_cast<size_t>(r)].filePath);

                juce::MessageManager::callAsync([files] {
                    juce::DragAndDropContainer::performExternalDragDropOfFiles(files, false);
                });
            }
#if JUCE_LINUX
            return {};
#else
            return allFilteredItems[static_cast<size_t>(r)].filePath;
#endif
        }
    }
    return {};
}

void SampleTableComponent::showContextMenuForRow(int rowNumber)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(allFilteredItems.size()))
        return;

    const auto& item = allFilteredItems[static_cast<size_t>(rowNumber)];

    juce::PopupMenu menu;
    menu.addSectionHeader(item.fileName);
    menu.addItem(1, item.isFavorite ? "Remove from Favorites" : "Add to Favorites");
    
    juce::PopupMenu ratingMenu;
    for (int r = 0; r <= 5; ++r)
    {
        ratingMenu.addItem(10 + r, (r == 0) ? "No Rating" : juce::String(r) + " Stars", true, r == item.rating);
    }
    menu.addSubMenu("Set Rating", ratingMenu);

    menu.addSeparator();
    menu.addItem(2, "Add Custom Tag...");
    
    juce::PopupMenu removeTagMenu;
    int tagIdx = 100;
    for (const auto& tag : item.tags)
    {
        removeTagMenu.addItem(tagIdx++, "Remove " + tag);
    }
    if (!item.tags.empty())
    {
        menu.addSubMenu("Remove Tag", removeTagMenu);
    }

    menu.addSeparator();
    menu.addItem(3, "Find Similar Sounds...");

    menu.addSeparator();
    menu.addItem(4, "Reveal in Finder");
    menu.addItem(5, "Convert / Export Sample...");
    menu.addItem(6, "Add to Performance Grid (Pad)");
    menu.addItem(7, "Auto-Slice into Performance Grid");

    menu.showMenuAsync(juce::PopupMenu::Options().withMousePosition(),
                       [this, item, rowNumber](int result) {
        if (result == 1) // Toggle Favorite
        {
            allFilteredItems[static_cast<size_t>(rowNumber)].isFavorite = !item.isFavorite;
            dbManager.toggleFavorite(item.id);
            table.repaintRow(rowNumber);
        }
        else if (result >= 10 && result <= 15) // Set Rating
        {
            int newRating = result - 10;
            allFilteredItems[static_cast<size_t>(rowNumber)].rating = newRating;
            allFilteredItems[static_cast<size_t>(rowNumber)].precomputeCachedStrings();
            dbManager.setRating(item.id, newRating);
            table.repaintRow(rowNumber);
        }
        else if (result == 2) // Add Tag
        {
            auto alert = std::make_shared<juce::AlertWindow>("Add Custom Tag", "Enter a new custom tag for " + item.fileName + ":", juce::AlertWindow::QuestionIcon);
            alert->addTextEditor("tagInput", "", "Tag (e.g. Kick, #Sub, Vocal)");
            if (auto* ed = alert->getTextEditor("tagInput"))
            {
                ed->setJustification(juce::Justification::centredLeft);
                ed->setIndents(4, 0);
            }
            alert->addButton("Add Tag", 1, juce::KeyPress(juce::KeyPress::returnKey));
            alert->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

            alert->enterModalState(true, juce::ModalCallbackFunction::create([this, alert, item](int btnResult) {
                if (btnResult == 1)
                {
                    auto newTag = alert->getTextEditorContents("tagInput").trim();
                    if (newTag.isNotEmpty())
                    {
                        dbManager.addTagToItem(item.id, newTag);
                    }
                }
            }));
        }
        else if (result >= 100) // Remove Tag
        {
            int idx = result - 100;
            if (idx >= 0 && idx < static_cast<int>(item.tags.size()))
            {
                auto it = item.tags.begin();
                std::advance(it, idx);
                dbManager.removeTagFromItem(item.id, *it);
            }
        }
        else if (result == 3) // Find Similar Sounds
        {
            similarityTargetId = item.id;
            updateFilter(currentKeyword, currentSelectedTags, currentMatchAll, currentExtFilter, currentFavOnly);
        }
        else if (result == 4) // Reveal in Finder
        {
            juce::File f(item.filePath);
            if (f.existsAsFile())
            {
                f.revealToUser();
            }
        }
        else if (result == 5) // Convert / Export Sample
        {
            convertSample(item);
        }
        else if (result == 6) // Add to Performance Grid (Pad)
        {
            listeners.call([item](SampleTableListener& l) {
                l.addToSampleMapRequested(item);
            });
        }
        else if (result == 7) // Auto-Slice into Performance Grid
        {
            listeners.call([item](SampleTableListener& l) {
                l.autoSliceToSamplerRequested(item);
            });
        }
    });
}

void SampleTableComponent::convertSample(const MediaItem& item)
{
    juce::StringArray formatNames;
    std::vector<juce::AudioFormat*> writableFormats;
    auto& formatManager = audioEngine.getFormatManager();
    for (int i = 0; i < formatManager.getNumKnownFormats(); ++i)
    {
        auto* format = formatManager.getKnownFormat(i);
        auto name = format->getFormatName();
        if (name != "MP3 file" && name != "MP3")
        {
            if (!formatNames.contains(name))
            {
                formatNames.add(name);
                writableFormats.push_back(format);
            }
        }
    }

    if (writableFormats.empty())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                               "Convert Audio",
                                               "No writable audio formats are supported by the application.");
        return;
    }

    convertDialog = std::make_unique<ConvertDialog>(item, audioEngine, dbManager, writableFormats);
    convertDialog->showDialog();
}

juce::String SampleTableComponent::formatDuration(double seconds)
{
    if (seconds <= 0.0) return "0:00";
    int totalSecs = static_cast<int>(seconds);
    int mins = totalSecs / 60;
    int secs = totalSecs % 60;
    return juce::String(mins) + ":" + (secs < 10 ? "0" : "") + juce::String(secs);
}

void SampleTableComponent::libraryIndexUpdated()
{
    juce::MessageManager::callAsync([this] {
        updateFilter(currentKeyword, currentSelectedTags, currentMatchAll, currentExtFilter, currentFavOnly);
    });
}

void SampleTableComponent::tagsUpdated()
{
    juce::MessageManager::callAsync([this] {
        updateFilter(currentKeyword, currentSelectedTags, currentMatchAll, currentExtFilter, currentFavOnly);
    });
}

void SampleTableComponent::addListener(SampleTableListener* listener)
{
    listeners.add(listener);
}

void SampleTableComponent::removeListener(SampleTableListener* listener)
{
    listeners.remove(listener);
}

bool SampleTableComponent::mayDragToExternalWindows() const
{
    return true;
}

double SampleTableComponent::calculateDistance(const MediaItem& a, const MediaItem& b)
{
    return TagDatabaseManager::calculateAcousticDistance(a, b);
}

} // namespace openwav
