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

    similarityBannerLabel.setFont(juce::Font(12.0f).boldened());
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
    updateFilter("", {}, false, "All", false);
}

SampleTableComponent::~SampleTableComponent()
{
    dbManager.removeListener(this);
    table.setModel(nullptr);
}

void SampleTableComponent::paint(juce::Graphics& g)
{
    g.fillAll(OpenWavLookAndFeel::bgDark);

    if (similarityTargetId.isNotEmpty())
    {
        auto bannerArea = getLocalBounds().removeFromTop(30).reduced(4, 2).toFloat();
        
        // Glassmorphic / glowing backdrop
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
    // Clear similarity target if any filter inputs change
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

    displayedItems = dbManager.getFilteredItems(currentKeyword, currentSelectedTags, currentMatchAll, currentExtFilter, currentFavOnly);

    if (similarityTargetId.isNotEmpty())
    {
        // Find target item to compute distance against
        MediaItem targetItem;
        bool found = false;
        for (const auto& item : displayedItems)
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
            for (const auto& item : displayedItems)
            {
                if (item.id == targetItem.id || TagDatabaseManager::calculateMatchPercentage(targetItem, item) >= 60.0f)
                {
                    thresholdedItems.push_back(item);
                }
            }
            displayedItems = thresholdedItems;

            std::sort(displayedItems.begin(), displayedItems.end(), [targetItem](const MediaItem& a, const MediaItem& b) {
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
            std::sort(displayedItems.begin(), displayedItems.end(), [](const MediaItem& a, const MediaItem& b) {
                return a.fileName.compareIgnoreCase(b.fileName) < 0;
            });
        }
    }
    else
    {
        similarityTargetName = "";
        std::sort(displayedItems.begin(), displayedItems.end(), [](const MediaItem& a, const MediaItem& b) {
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

    resized();

    table.updateContent();
    table.repaint();

    if (similarityTargetId.isNotEmpty())
    {
        table.scrollToEnsureRowIsOnscreen(0);
    }

    listeners.call([this](SampleTableListener& l) {
        l.displayedItemsChanged(displayedItems);
    });
}

int SampleTableComponent::getNumRows()
{
    return static_cast<int>(displayedItems.size());
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
        g.fillAll(OpenWavLookAndFeel::bgCard);
    }
}

void SampleTableComponent::paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(displayedItems.size()))
        return;

    const auto& item = displayedItems[static_cast<size_t>(rowNumber)];
    auto bounds = juce::Rectangle<int>(0, 0, width, height).reduced(6, 0);

    g.setFont(juce::Font(13.0f));

    switch (columnId)
    {
        case 1: // Play status / Icon button
        {
            bool isCurrentFile = (audioEngine.getCurrentFile().getFullPathName() == item.filePath);
            bool isPlaying = isCurrentFile && audioEngine.isPlaying();

            g.setColour(isPlaying ? OpenWavLookAndFeel::accentCyan : OpenWavLookAndFeel::textSecondary);
            juce::Path icon;
            if (isPlaying)
            {
                // Pause icon
                icon.addRectangle(width * 0.35f, height * 0.28f, 3.0f, height * 0.44f);
                icon.addRectangle(width * 0.55f, height * 0.28f, 3.0f, height * 0.44f);
            }
            else
            {
                // Play triangle
                icon.addTriangle(width * 0.38f, height * 0.28f,
                                 width * 0.38f, height * 0.72f,
                                 width * 0.68f, height * 0.50f);
            }
            g.fillPath(icon);
            break;
        }

        case 2: // File Name
        {
            g.setColour(rowIsSelected ? OpenWavLookAndFeel::accentCyan : OpenWavLookAndFeel::textPrimary);
            g.setFont(rowIsSelected ? juce::Font(13.0f).boldened() : juce::Font(13.0f));

            if (similarityTargetId.isNotEmpty() && cachedSimilarityTargetItem.id == similarityTargetId && item.id == similarityTargetId)
            {
                juce::Font badgeFont(10.0f, juce::Font::bold);
                juce::String badgeText = "TARGET";
                int badgeW = badgeFont.getStringWidth(badgeText) + 10;
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
            int tagX = bounds.getX();
            int tagY = bounds.getY() + 4;
            int tagHeight = height - 8;

            juce::Font tagFont(11.0f);
            g.setFont(tagFont);
            for (const auto& tag : item.tags)
            {
                int tagWidth = tagFont.getStringWidth(tag) + 12;
                if (tagX + tagWidth > bounds.getRight()) break;

                auto pillBounds = juce::Rectangle<float>(static_cast<float>(tagX), static_cast<float>(tagY),
                                                         static_cast<float>(tagWidth), static_cast<float>(tagHeight));

                g.setColour(OpenWavLookAndFeel::bgCard);
                g.fillRoundedRectangle(pillBounds, 4.0f);

                g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.6f));
                g.drawRoundedRectangle(pillBounds, 4.0f, 1.0f);

                g.setColour(OpenWavLookAndFeel::textPrimary);
                g.drawText(tag, pillBounds, juce::Justification::centred, true);

                tagX += tagWidth + 4;
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

        case 7: // Rating Stars
        {
            g.setColour(juce::Colours::gold);
            if (item.cachedStarRating.isNotEmpty())
            {
                g.drawText(item.cachedStarRating, bounds, juce::Justification::centredLeft, true);
            }
            else
            {
                juce::String stars;
                for (int s = 0; s < item.rating; ++s) stars += juce::String::fromUTF8("\xe2\x98\x85");
                for (int s = item.rating; s < 5; ++s) stars += juce::String::fromUTF8("\xe2\x98\x86");
                g.drawText(stars, bounds, juce::Justification::centredLeft, true);
            }
            break;
        }

        case 8: // Favorite Heart Vector Drawing
        {
            juce::Path heart;
            float iconSize = 14.0f;
            float cx = bounds.getCentreX();
            float cy = bounds.getCentreY();
            float x = cx - iconSize * 0.5f;
            float y = cy - iconSize * 0.5f;
            float w = iconSize;
            float h = iconSize;

            heart.startNewSubPath(x + w * 0.5f, y + h * 0.85f);
            heart.cubicTo(x + w * 0.1f, y + h * 0.5f,  x,             y + h * 0.2f,  x + w * 0.25f, y + h * 0.05f);
            heart.cubicTo(x + w * 0.45f, y - h * 0.05f, x + w * 0.5f,  y + h * 0.2f,  x + w * 0.5f,  y + h * 0.25f);
            heart.cubicTo(x + w * 0.5f,  y + h * 0.2f,  x + w * 0.55f, y - h * 0.05f, x + w * 0.75f, y + h * 0.05f);
            heart.cubicTo(x + w,         y + h * 0.2f,  x + w * 0.9f,  y + h * 0.5f,  x + w * 0.5f,  y + h * 0.85f);
            heart.closeSubPath();

            if (item.isFavorite)
            {
                g.setColour(OpenWavLookAndFeel::favoriteRed);
                g.fillPath(heart);
            }
            else
            {
                g.setColour(OpenWavLookAndFeel::textSecondary.withAlpha(0.35f));
                g.strokePath(heart, juce::PathStrokeType(1.2f));
            }
            break;
        }

        case 9: // Match %
        {
            // Now handled by MatchCellComponent via refreshComponentForCell
            break;
        }

        default:
            break;
    }
}

juce::String SampleTableComponent::getCellTooltip(int rowNumber, int columnId)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(displayedItems.size()))
        return {};

    if (similarityTargetId.isNotEmpty() && cachedSimilarityTargetItem.id == similarityTargetId)
    {
        const auto& item = displayedItems[static_cast<size_t>(rowNumber)];
        if (item.id == similarityTargetId)
            return "This is the target sample.";

        // We only want to show the tooltip if hovering over the Match % column (column 9) or the File Name column (column 2)
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

class MatchCellComponent : public juce::Component
{
public:
    MatchCellComponent(SampleTableComponent& owner, const MediaItem& i, const MediaItem& target)
        : table(owner), item(i), targetItem(target)
    {
    }

    void paint(juce::Graphics& g) override
    {
        if (item.id == targetItem.id) return;

        auto bounds = getLocalBounds().reduced(6, 0);
        float matchPct = TagDatabaseManager::calculateMatchPercentage(targetItem, item);
        float matchRatio = juce::jlimit(0.0f, 1.0f, matchPct / 100.0f);

        float size = std::min(getWidth(), getHeight()) - 8.0f;
        auto ringBounds = juce::Rectangle<float>(0, 0, size, size).withCentre(bounds.getCentre().toFloat());
        
        g.setColour(OpenWavLookAndFeel::bgDark.withAlpha(0.6f));
        g.drawEllipse(ringBounds, 3.0f);
        
        if (matchRatio > 0.01f)
        {
            juce::Path arc;
            arc.addCentredArc(ringBounds.getCentreX(), ringBounds.getCentreY(), ringBounds.getWidth() * 0.5f, ringBounds.getHeight() * 0.5f, 0.0f, 0.0f, juce::MathConstants<float>::twoPi * matchRatio, true);
            g.setColour(OpenWavLookAndFeel::accentCyan);
            g.strokePath(arc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        
        juce::String pctStr = juce::String(juce::roundToInt(matchPct));
        g.setColour(OpenWavLookAndFeel::textPrimary);
        g.setFont(juce::Font(9.0f).boldened());
        g.drawText(pctStr, ringBounds.toNearestInt(), juce::Justification::centred, false);
    }

    void mouseEnter(const juce::MouseEvent&) override
    {
        if (table.onSimilarityHover)
            table.onSimilarityHover(&item, &targetItem, getScreenPosition().translated(getWidth(), 0));
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        if (table.onSimilarityHover)
            table.onSimilarityHover(nullptr, nullptr, {});
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        forwardClick(e);
    }

    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        forwardClick(e);
    }

private:
    void forwardClick(const juce::MouseEvent& e)
    {
        int rowNumber = -1;
        for (int i = 0; i < (int)table.displayedItems.size(); ++i)
        {
            if (table.displayedItems[i].id == item.id)
            {
                rowNumber = i;
                break;
            }
        }
        if (rowNumber >= 0)
        {
            table.cellClicked(rowNumber, 9, e.getEventRelativeTo(&table));
        }
    }

private:
    SampleTableComponent& table;
    MediaItem item;
    MediaItem targetItem;
};

juce::Component* SampleTableComponent::refreshComponentForCell(int rowNumber, int columnId, bool /*isRowSelected*/, juce::Component* existingComponentToUpdate)
{
    if (columnId == 9 && similarityTargetId.isNotEmpty())
    {
        if (rowNumber >= 0 && rowNumber < static_cast<int>(displayedItems.size()))
        {
            const auto& item = displayedItems[static_cast<size_t>(rowNumber)];
            if (item.id != similarityTargetId)
            {
                auto* comp = new MatchCellComponent(*this, item, cachedSimilarityTargetItem);
                delete existingComponentToUpdate;
                return comp;
            }
        }
    }

    delete existingComponentToUpdate;
    return nullptr;
}

void SampleTableComponent::cellClicked(int rowNumber, int columnId, const juce::MouseEvent& e)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(displayedItems.size()))
        return;

    table.grabKeyboardFocus();

    const auto& item = displayedItems[static_cast<size_t>(rowNumber)];

    if (e.mods.isPopupMenu())
    {
        showContextMenuForRow(rowNumber);
        return;
    }

    if (columnId == 8) // Favorite Heart Toggle
    {
        dbManager.toggleFavorite(item.id);
        return;
    }



    // If already selected, selectedRowsChanged won't trigger, so reload/play here.
    // Otherwise table.selectRow below will trigger selectedRowsChanged.
    juce::File fileToLoad(item.filePath);
    if (fileToLoad.existsAsFile())
    {
        audioEngine.setSampleBpm(item.bpm);
        audioEngine.loadFile(fileToLoad, true);
    }

    table.selectRow(rowNumber);

    listeners.call([item](SampleTableListener& l) {
        l.sampleSelected(item);
    });
}

void SampleTableComponent::selectedRowsChanged(int lastRowSelected)
{
    if (lastRowSelected >= 0 && lastRowSelected < static_cast<int>(displayedItems.size()))
    {
        const auto& item = displayedItems[static_cast<size_t>(lastRowSelected)];
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
    int numRows = getNumRows();
    if (numRows <= 0)
        return;

    int nextRow = current + delta;
    if (nextRow < 0) nextRow = 0;
    if (nextRow >= numRows) nextRow = numRows - 1;

    table.selectRow(nextRow);
    table.scrollToEnsureRowIsOnscreen(nextRow);
}

void SampleTableComponent::selectItemById(const juce::String& itemId)
{
    if (itemId.isEmpty())
        return;

    for (size_t i = 0; i < displayedItems.size(); ++i)
    {
        if (displayedItems[i].id == itemId)
        {
            table.selectRow(static_cast<int>(i));
            table.scrollToEnsureRowIsOnscreen(static_cast<int>(i));

            juce::File fileToLoad(displayedItems[i].filePath);
            if (fileToLoad.existsAsFile())
            {
                audioEngine.setSampleBpm(displayedItems[i].bpm);
                audioEngine.loadFile(fileToLoad, true);
            }
            break;
        }
    }
}

void SampleTableComponent::cellDoubleClicked(int rowNumber, int /*columnId*/, const juce::MouseEvent& /*e*/)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(displayedItems.size()))
        return;

    const auto& item = displayedItems[static_cast<size_t>(rowNumber)];
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
        if (r >= 0 && r < static_cast<int>(displayedItems.size()))
        {
            if (juce::Desktop::getInstance().getMainMouseSource().isDragging())
            {
                juce::StringArray files;
                files.add(displayedItems[static_cast<size_t>(r)].filePath);

                juce::MessageManager::callAsync([files] {
                    juce::DragAndDropContainer::performExternalDragDropOfFiles(files, false);
                });
            }
#if JUCE_LINUX
            return {};
#else
            return displayedItems[static_cast<size_t>(r)].filePath;
#endif
        }
    }
    return {};
}

void SampleTableComponent::showContextMenuForRow(int rowNumber)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(displayedItems.size()))
        return;

    const auto& item = displayedItems[static_cast<size_t>(rowNumber)];

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
    menu.addItem(3, "Reveal in File Explorer / Finder");
    menu.addItem(4, "Find Similar Sounds");
    menu.addItem(5, "Convert Format / Sample Rate...");
    menu.addItem(6, "Auto Slice to Sampler");

    menu.showMenuAsync(juce::PopupMenu::Options(), [this, item](int result) {
        if (result == 1)
        {
            dbManager.toggleFavorite(item.id);
        }
        else if (result >= 10 && result <= 15)
        {
            dbManager.setRating(item.id, result - 10);
        }
        else if (result == 2)
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
        else if (result >= 100)
        {
            int idx = result - 100;
            if (idx >= 0 && idx < static_cast<int>(item.tags.size()))
            {
                auto it = item.tags.begin();
                std::advance(it, idx);
                dbManager.removeTagFromItem(item.id, *it);
            }
        }
        else if (result == 3)
        {
            juce::File(item.filePath).revealToUser();
        }
        else if (result == 4)
        {
            similarityTargetId = item.id;
            updateFilter(currentKeyword, currentSelectedTags, currentMatchAll, currentExtFilter, currentFavOnly);
            table.scrollToEnsureRowIsOnscreen(0);
        }
        else if (result == 5)
        {
            convertSample(item);
        }
        else if (result == 6)
        {
            listeners.call([item](SampleTableListener& l) {
                l.autoSliceToSamplerRequested(item);
            });
        }
    });
}

void SampleTableComponent::convertSample(const MediaItem& item)
{
    // 1. Get writable formats
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
