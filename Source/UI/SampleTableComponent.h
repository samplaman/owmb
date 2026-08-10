#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_gui_basics/juce_gui_basics.h>
#endif
#include "../Database/TagDatabaseManager.h"
#include "../Audio/AudioEngine.h"
#include "../Models/MediaItem.h"
#include "ConvertDialog.h"

namespace openwav
{

class SampleTableListener
{
public:
    virtual ~SampleTableListener() = default;
    virtual void sampleSelected(const MediaItem& item) = 0;
    virtual void sampleDoubleClicked(const MediaItem& item) = 0;
    virtual void displayedItemsChanged(const std::vector<MediaItem>& /*items*/) {}
};

class SampleTableComponent : public juce::Component,
                             public juce::TableListBoxModel,
                             public TagDatabaseListener
{
public:
    SampleTableComponent(TagDatabaseManager& db, AudioEngine& engine);
    ~SampleTableComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void updateFilter(const juce::String& searchKeyword,
                      const std::set<juce::String>& selectedTags,
                      bool matchAllTags,
                      const juce::String& extensionFilter,
                      bool favoritesOnly);

    const std::vector<MediaItem>& getDisplayedItems() const { return displayedItems; }

    // juce::TableListBoxModel overrides
    int getNumRows() override;
    void paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected) override;
    void paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected) override;
    juce::String getCellTooltip(int rowNumber, int columnId) override;
    juce::Component* refreshComponentForCell(int rowNumber, int columnId, bool isRowSelected, juce::Component* existingComponentToUpdate) override;
    void cellClicked(int rowNumber, int columnId, const juce::MouseEvent& e) override;
    void cellDoubleClicked(int rowNumber, int columnId, const juce::MouseEvent& e) override;
    void selectedRowsChanged(int lastRowSelected) override;
    juce::var getDragSourceDescription(const juce::SparseSet<int>& selectedRows) override;
    bool mayDragToExternalWindows() const override;

    void moveSelection(int delta);
    void selectItemById(const juce::String& itemId);

    // TagDatabaseListener callbacks
    void libraryIndexUpdated() override;
    void tagsUpdated() override;

    void addListener(SampleTableListener* listener);
    void removeListener(SampleTableListener* listener);

private:
    void showContextMenuForRow(int rowNumber);
    void convertSample(const MediaItem& item);
    static juce::String formatDuration(double seconds);

    TagDatabaseManager& dbManager;
    AudioEngine& audioEngine;

    juce::TableListBox table;
    std::vector<MediaItem> displayedItems;

    // Filter State
    juce::String currentKeyword;
    std::set<juce::String> currentSelectedTags;
    bool currentMatchAll { false };
    juce::String currentExtFilter { "All" };
    bool currentFavOnly { false };

    std::function<void(const MediaItem*, const MediaItem*, juce::Point<int>)> onSimilarityHover;

    juce::ListenerList<SampleTableListener> listeners;
    juce::String similarityTargetId;
    juce::String similarityTargetName;
    MediaItem cachedSimilarityTargetItem;

    juce::Label similarityBannerLabel;
    juce::TextButton clearSimilarityButton { "X" };
    static double calculateDistance(const MediaItem& a, const MediaItem& b);

    std::unique_ptr<ConvertDialog> convertDialog;
};

} // namespace openwav
