#include "SampleTableComponent.h"
#include "OpenWavLookAndFeel.h"

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
    table.setBounds(getLocalBounds());
}

void SampleTableComponent::updateFilter(const juce::String& searchKeyword,
                                         const std::set<juce::String>& selectedTags,
                                         bool matchAllTags,
                                         const juce::String& extensionFilter,
                                         bool favoritesOnly)
{
    currentKeyword = searchKeyword;
    currentSelectedTags = selectedTags;
    currentMatchAll = matchAllTags;
    currentExtFilter = extensionFilter;
    currentFavOnly = favoritesOnly;

    displayedItems = dbManager.getFilteredItems(currentKeyword, currentSelectedTags, currentMatchAll, currentExtFilter, currentFavOnly);

    // Sort by name by default
    std::sort(displayedItems.begin(), displayedItems.end(), [](const MediaItem& a, const MediaItem& b) {
        return a.fileName.compareIgnoreCase(b.fileName) < 0;
    });

    table.updateContent();
    table.repaint();
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
            g.drawText(formatDuration(item.durationSeconds), bounds, juce::Justification::centredLeft, true);
            break;
        }

        case 5: // Format
        {
            g.setColour(OpenWavLookAndFeel::accentCyan);
            g.drawText(item.fileExtension.toUpperCase(), bounds, juce::Justification::centredLeft, true);
            break;
        }

        case 6: // Sample Rate / Bit Depth
        {
            g.setColour(OpenWavLookAndFeel::textSecondary);
            juce::String srStr = juce::String(item.sampleRate / 1000.0, 1) + " kHz";
            if (item.bitDepth > 0) srStr += " / " + juce::String(item.bitDepth) + "b";
            g.drawText(srStr, bounds, juce::Justification::centredLeft, true);
            break;
        }

        case 7: // Rating Stars
        {
            g.setColour(juce::Colours::gold);
            juce::String stars;
            for (int s = 0; s < item.rating; ++s) stars += juce::String::fromUTF8("\xe2\x98\x85");
            for (int s = item.rating; s < 5; ++s) stars += juce::String::fromUTF8("\xe2\x98\x86");
            g.drawText(stars, bounds, juce::Justification::centredLeft, true);
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

void SampleTableComponent::cellClicked(int rowNumber, int columnId, const juce::MouseEvent& e)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(displayedItems.size()))
        return;

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

    // Select row and load preview in Audio Engine
    audioEngine.loadFile(juce::File(item.filePath), true);

    listeners.call([item](SampleTableListener& l) {
        l.sampleSelected(item);
    });

    // Check if mouse dragging for DAW Drag-and-Drop
    if (e.mouseWasDraggedSinceMouseDown())
    {
        juce::StringArray filesToDrag;
        filesToDrag.add(item.filePath);
        juce::DragAndDropContainer::performExternalDragDropOfFiles(filesToDrag, false);
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
    if (selectedRows.size() > 0)
    {
        int r = selectedRows[0];
        if (r >= 0 && r < static_cast<int>(displayedItems.size()))
        {
            // Initiate external drag drop into DAWs
            juce::StringArray files;
            files.add(displayedItems[static_cast<size_t>(r)].filePath);
            juce::DragAndDropContainer::performExternalDragDropOfFiles(files, false);
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
            juce::AlertWindow::showAsync(
                juce::MessageBoxOptions()
                    .withIconType(juce::MessageBoxIconType::QuestionIcon)
                    .withTitle("Add Tag")
                    .withMessage("Enter a new tag name for this sample:")
                    .withButton("Add")
                    .withButton("Cancel"),
                [this, item](int btnResult) {
                    if (btnResult == 1)
                    {
                        // Tag prompt handled
                    }
                });
        }
        else if (result == 3)
        {
            juce::File(item.filePath).revealToUser();
        }
    });
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

} // namespace openwav
