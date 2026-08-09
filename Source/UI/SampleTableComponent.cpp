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
    similarityBannerLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    addChildComponent(similarityBannerLabel);

    clearSimilarityButton.setButtonText(juce::String::fromUTF8("\xe2\x9c\x95"));
    clearSimilarityButton.setTooltip("Clear similarity filter");
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
    
    // Fixed columns: Column 1 (36 px) + Column 8 (40 px) = 76 px
    int availableWidth = totalWidth - 76;
    if (availableWidth > 100)
    {
        // Default sum of resizable column widths is 755 px.
        double scale = static_cast<double>(availableWidth) / 755.0;
        
        header.setColumnWidth(2, static_cast<int>(220 * scale));
        header.setColumnWidth(3, static_cast<int>(230 * scale));
        header.setColumnWidth(4, static_cast<int>(70 * scale));
        header.setColumnWidth(5, static_cast<int>(65 * scale));
        header.setColumnWidth(6, static_cast<int>(95 * scale));
        header.setColumnWidth(7, static_cast<int>(75 * scale));
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
            similarityBannerLabel.setText("SHOWING ACOUSTICALLY SIMILAR SOUNDS TO: " + similarityTargetName, juce::dontSendNotification);

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
    else if (rowNumber % 2 == 1)
    {
        g.fillAll(OpenWavLookAndFeel::bgHeader.withAlpha(0.45f));
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

            if (similarityTargetId.isNotEmpty() && cachedSimilarityTargetItem.id == similarityTargetId)
            {
                if (item.id == similarityTargetId)
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
                else
                {
                    float matchPct = TagDatabaseManager::calculateMatchPercentage(cachedSimilarityTargetItem, item);
                    float matchRatio = juce::jlimit(0.0f, 1.0f, matchPct / 100.0f);

                    int barWidth = 80;
                    float barHeight = 14.0f;
                    auto barOuter = bounds.removeFromRight(barWidth).toFloat().withHeight(barHeight);
                    barOuter.setY((height - barHeight) * 0.5f);

                    // Background track
                    g.setColour(OpenWavLookAndFeel::bgDark.withAlpha(0.6f));
                    g.fillRoundedRectangle(barOuter, 3.0f);
                    g.setColour(OpenWavLookAndFeel::borderColour);
                    g.drawRoundedRectangle(barOuter, 3.0f, 1.0f);

                    // Filled match progress bar scaled 0 to 100%
                    if (matchRatio > 0.01f)
                    {
                        auto fillWidth = std::max(4.0f, (barOuter.getWidth() - 2.0f) * matchRatio);
                        auto fillBounds = juce::Rectangle<float>(barOuter.getX() + 1.0f, barOuter.getY() + 1.0f, fillWidth, barOuter.getHeight() - 2.0f);
                        g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.65f));
                        g.fillRoundedRectangle(fillBounds, 2.0f);
                    }

                    // Percentage text (e.g. "98%")
                    juce::String pctStr = juce::String(juce::roundToInt(matchPct)) + "%";
                    g.setColour(OpenWavLookAndFeel::textPrimary);
                    g.setFont(juce::Font(10.0f).boldened());
                    g.drawText(pctStr, barOuter, juce::Justification::centred, true);

                    bounds.removeFromRight(6);
                }
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

        default:
            break;
    }
}

juce::Component* SampleTableComponent::refreshComponentForCell(int /*rowNumber*/, int /*columnId*/, bool /*isRowSelected*/, juce::Component* existingComponentToUpdate)
{
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

    if (columnId == 9) // Clicking on comment cell should edit it, not trigger audio preview
    {
        return;
    }

    // If already selected, selectedRowsChanged won't trigger, so reload/play here.
    // Otherwise table.selectRow below will trigger selectedRowsChanged.
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
    if (lastRowSelected >= 0 && lastRowSelected < static_cast<int>(displayedItems.size()))
    {
        const auto& item = displayedItems[static_cast<size_t>(lastRowSelected)];
        juce::File fileToLoad(item.filePath);
        
        if (fileToLoad.existsAsFile() && audioEngine.getCurrentFile() != fileToLoad)
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
            table.selectRow(static_cast<int>(i), false, false);
            table.scrollToEnsureRowIsOnscreen(static_cast<int>(i));
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
            return displayedItems[static_cast<size_t>(r)].filePath;
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
            formatNames.add(format->getFormatName());
            writableFormats.push_back(format);
        }
    }

    if (writableFormats.empty())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                               "Convert Audio",
                                               "No writable audio formats are supported by the application.");
        return;
    }

    auto alert = std::make_shared<juce::AlertWindow>("Convert Audio File", "Configure target settings for conversion:", juce::AlertWindow::QuestionIcon);
    alert->addComboBox("format", formatNames, "Format:");
    
    juce::StringArray srOptions;
    srOptions.add("Original");
    srOptions.add("44100 Hz");
    srOptions.add("48000 Hz");
    srOptions.add("88200 Hz");
    srOptions.add("96000 Hz");
    alert->addComboBox("samplerate", srOptions, "Sample Rate:");

    juce::StringArray bdOptions;
    bdOptions.add("Original");
    bdOptions.add("16-bit");
    bdOptions.add("24-bit");
    bdOptions.add("32-bit Float");
    alert->addComboBox("bitdepth", bdOptions, "Bit Depth:");

    // Center cursor and text on alert's comboboxes and internal components
    if (auto* fCombo = alert->getComboBoxComponent("format"))
    {
        // Select matching format by default
        juce::String currentExt = juce::File(item.filePath).getFileExtension().toLowerCase();
        if (currentExt.startsWith("."))
            currentExt = currentExt.substring(1);

        for (int idx = 0; idx < formatNames.size(); ++idx)
        {
            if (writableFormats[idx]->getFileExtensions().contains(currentExt))
            {
                fCombo->setSelectedItemIndex(idx);
                break;
            }
        }
    }

    alert->addButton("Convert...", 1, juce::KeyPress(juce::KeyPress::returnKey));
    alert->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    alert->enterModalState(true, juce::ModalCallbackFunction::create([this, alert, item, writableFormats](int btnResult) {
        if (btnResult != 1)
            return;

        int formatIdx = -1;
        if (auto* cb = alert->getComboBoxComponent("format"))
            formatIdx = cb->getSelectedItemIndex();

        if (formatIdx < 0 || formatIdx >= static_cast<int>(writableFormats.size()))
            return;

        auto* targetFormat = writableFormats[static_cast<size_t>(formatIdx)];

        int srIdx = -1;
        if (auto* cb = alert->getComboBoxComponent("samplerate"))
            srIdx = cb->getSelectedItemIndex();

        double targetSampleRate = 0.0;
        if (srIdx == 1) targetSampleRate = 44100.0;
        else if (srIdx == 2) targetSampleRate = 48000.0;
        else if (srIdx == 3) targetSampleRate = 88200.0;
        else if (srIdx == 4) targetSampleRate = 96000.0;

        int bdIdx = -1;
        if (auto* cb = alert->getComboBoxComponent("bitdepth"))
            bdIdx = cb->getSelectedItemIndex();

        int targetBitDepth = 0;
        if (bdIdx == 1) targetBitDepth = 16;
        else if (bdIdx == 2) targetBitDepth = 24;
        else if (bdIdx == 3) targetBitDepth = 32;

        // Choose save destination using FileChooser
        juce::String targetExt = targetFormat->getFileExtensions()[0];
        if (!targetExt.startsWith("."))
            targetExt = "." + targetExt;

        auto defaultName = juce::File(item.filePath).getFileNameWithoutExtension() + "_converted" + targetExt;

        auto scanFolders = dbManager.getScanFolders();
        juce::File defaultLocation;
        if (!scanFolders.empty())
        {
            // If original file is within one of the scan folders, use its parent dir
            bool originalInScanFolder = false;
            juce::File originalFile(item.filePath);
            for (const auto& folder : scanFolders)
            {
                if (originalFile.isAChildOf(juce::File(folder)))
                {
                    originalInScanFolder = true;
                    break;
                }
            }

            if (originalInScanFolder)
            {
                defaultLocation = originalFile.getParentDirectory().getChildFile(defaultName);
            }
            else
            {
                // Otherwise, save it in the first scan folder
                defaultLocation = juce::File(scanFolders[0]).getChildFile(defaultName);
            }
        }
        else
        {
            defaultLocation = juce::File(item.filePath).getParentDirectory().getChildFile(defaultName);
        }

        // Keep FileChooser alive via a shared pointer member or static variable
        struct ChosenState {
            std::shared_ptr<juce::FileChooser> chooser;
        };
        auto state = std::make_shared<ChosenState>();
        state->chooser = std::make_shared<juce::FileChooser>("Save Converted File...", defaultLocation, "*" + targetExt);

        state->chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                                 [this, item, targetFormat, targetSampleRate, targetBitDepth, targetExt, state](const juce::FileChooser& chooser) {
            auto destFile = chooser.getResult();
            if (destFile == juce::File())
                return;

            if (!destFile.getFileExtension().equalsIgnoreCase(targetExt))
            {
                destFile = destFile.withFileExtension(targetExt);
            }

            // Perform conversion
            std::unique_ptr<juce::AudioFormatReader> reader(audioEngine.getFormatManager().createReaderFor(juce::File(item.filePath)));
            if (reader == nullptr)
            {
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Conversion Failed", "Could not open source file.");
                return;
            }

            double srcSR = reader->sampleRate;
            double dstSR = (targetSampleRate > 0.0) ? targetSampleRate : srcSR;
            int srcBits = reader->bitsPerSample;
            int dstBits = (targetBitDepth > 0) ? targetBitDepth : srcBits;
            
            // Handle target format bit depth restrictions (e.g. FLAC doesn't support 32-bit)
            auto possibleDepths = targetFormat->getPossibleBitDepths();
            if (!possibleDepths.isEmpty() && !possibleDepths.contains(dstBits))
            {
                int bestBits = possibleDepths[0];
                for (int depth : possibleDepths)
                {
                    if (depth <= dstBits)
                        bestBits = std::max(bestBits, depth);
                }
                dstBits = bestBits;
            }

            int numChannels = reader->numChannels;

            juce::int64 numSamples64 = reader->lengthInSamples;
            if (juce::File(item.filePath).getFileExtension().toLowerCase() == ".mp3" && srcSR < 32000.0)
            {
                numSamples64 /= 2;
            }

            if (numSamples64 <= 0 || numSamples64 > 0x7FFFFFFF || numChannels <= 0)
            {
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Conversion Failed", "Invalid audio file properties.");
                return;
            }

            int numSamples = static_cast<int>(numSamples64);
            juce::AudioBuffer<float> srcBuffer(numChannels, numSamples);
            if (!reader->read(&srcBuffer, 0, numSamples, 0, true, true))
            {
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Conversion Failed", "Failed to read audio data from source.");
                return;
            }

            if (destFile.existsAsFile())
                destFile.deleteFile();

            auto outStream = std::make_unique<juce::FileOutputStream>(destFile);
            if (outStream->failedToOpen())
            {
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Conversion Failed", "Could not open destination file for writing.");
                return;
            }

            auto* rawStream = outStream.release();
            std::unique_ptr<juce::AudioFormatWriter> writer(
                targetFormat->createWriterFor(rawStream, dstSR, numChannels, dstBits, {}, 0)
            );

            if (writer == nullptr)
            {
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Conversion Failed", "Could not create audio writer for format.");
                return;
            }

            bool success = false;
            int totalSamples = numSamples;
            if (std::abs(dstSR - srcSR) > 0.01)
            {
                double speedRatio = srcSR / dstSR;
                int dstSamples = static_cast<int>(std::round(numSamples / speedRatio));
                totalSamples = dstSamples;
                juce::AudioBuffer<float> dstBuffer(numChannels, dstSamples);

                for (int ch = 0; ch < numChannels; ++ch)
                {
                    juce::LagrangeInterpolator interpolator;
                    interpolator.process(speedRatio,
                                         srcBuffer.getReadPointer(ch),
                                         dstBuffer.getWritePointer(ch),
                                         dstSamples);
                }
                success = writer->writeFromAudioSampleBuffer(dstBuffer, 0, dstSamples);
            }
            else
            {
                success = writer->writeFromAudioSampleBuffer(srcBuffer, 0, numSamples);
            }

            if (success)
            {
                // Force close output writer/stream so that the file is not locked on disk when reading back
                writer.reset();

                // Create a MediaItem for the newly converted file
                MediaItem newItem;
                newItem.filePath = destFile.getFullPathName();
                newItem.fileName = destFile.getFileName();
                newItem.fileExtension = destFile.getFileExtension().toLowerCase();
                newItem.fileSizeBytes = destFile.getSize();
                newItem.dateAddedMs = destFile.getLastModificationTime().toMilliseconds();
                newItem.id = juce::String::toHexString(destFile.getFullPathName().hashCode64());
                
                // Get inferred tags and append #Converted
                newItem.tags = TagDatabaseManager::inferTagsFromPath(destFile.getFullPathName());
                newItem.tags.insert("#Converted");

                newItem.sampleRate = dstSR;
                newItem.numChannels = numChannels;
                newItem.bitDepth = dstBits;
                newItem.durationSeconds = static_cast<double>(totalSamples) / dstSR;

                // Add to database and save
                dbManager.addOrUpdateItem(newItem);
                dbManager.saveToFile();

                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "Conversion Successful", "File converted successfully and added to library:\n" + destFile.getFullPathName());
            }
            else
            {
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Conversion Failed", "Failed to write converted audio data.");
            }
        });
    }));
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
