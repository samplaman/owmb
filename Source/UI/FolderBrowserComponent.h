#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_gui_basics/juce_gui_basics.h>
#endif

#include "../Database/TagDatabaseManager.h"
#include "../Scanner/LibraryScanner.h"
#include "../Audio/AudioEngine.h"
#include "../Models/MediaItem.h"
#include "SampleTableComponent.h"
#include "ConvertDialog.h"

#include <memory>
#include <vector>
#include <set>
#include <map>

namespace openwav
{

class SmoothFolderListBox : public juce::ListBox, private juce::Timer
{
public:
    SmoothFolderListBox() = default;
    ~SmoothFolderListBox() override { stopTimer(); }

    std::function<bool(const juce::KeyPress&)> onKeyNavigation;

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (onKeyNavigation && onKeyNavigation(key))
            return true;
        return juce::ListBox::keyPressed(key);
    }

    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        if (wheel.deltaY == 0.0f && wheel.deltaX == 0.0f)
            return;

        auto* vp = getViewport();
        if (vp == nullptr)
        {
            juce::ListBox::mouseWheelMove(e, wheel);
            return;
        }

        auto& sb = vp->getVerticalScrollBar();
        double maxScroll = sb.getMaximumRangeLimit() - sb.getCurrentRangeSize();
        if (maxScroll <= 0.0)
            return;

        if (wheel.isSmooth)
        {
            double currentY = static_cast<double>(vp->getViewPositionY());
            double deltaPixels = static_cast<double>(wheel.deltaY) * 240.0;
            double newY = juce::jlimit(0.0, maxScroll, currentY - deltaPixels);

            targetScrollY = newY;
            currentScrollY = newY;
            velocity = 0.0;

            vp->setViewPosition(0, juce::roundToInt(newY));
        }
        else
        {
            double currentY = static_cast<double>(vp->getViewPositionY());
            if (!isTimerRunning())
            {
                currentScrollY = currentY;
                targetScrollY = currentY;
                velocity = 0.0;
            }

            double step = static_cast<double>(wheel.deltaY) * (getRowHeight() * 3.5);
            targetScrollY = juce::jlimit(0.0, maxScroll, targetScrollY - step);
            velocity -= step * 0.4;

            startTimerHz(60);
        }
    }

    void smoothScrollToRow(int rowNumber)
    {
        auto* vp = getViewport();
        if (vp == nullptr || rowNumber < 0) return;

        int rowH = getRowHeight();
        int targetY = rowNumber * rowH;
        int viewH = vp->getViewHeight();
        int currentY = vp->getViewPositionY();

        if (targetY < currentY)
        {
            targetScrollY = targetY;
            currentScrollY = currentY;
            startTimerHz(60);
        }
        else if (targetY + rowH > currentY + viewH)
        {
            targetScrollY = std::max(0, targetY + rowH - viewH);
            currentScrollY = currentY;
            startTimerHz(60);
        }
    }

private:
    void timerCallback() override
    {
        auto* vp = getViewport();
        if (vp == nullptr)
        {
            stopTimer();
            return;
        }

        auto& sb = vp->getVerticalScrollBar();
        double maxScroll = sb.getMaximumRangeLimit() - sb.getCurrentRangeSize();
        if (maxScroll <= 0.0)
        {
            stopTimer();
            return;
        }

        targetScrollY = juce::jlimit(0.0, maxScroll, targetScrollY);

        double diff = targetScrollY - currentScrollY;
        if (std::abs(diff) < 0.5 && std::abs(velocity) < 0.1)
        {
            currentScrollY = targetScrollY;
            velocity = 0.0;
            vp->setViewPosition(0, juce::roundToInt(currentScrollY));
            stopTimer();
            return;
        }

        currentScrollY += diff * 0.32;
        currentScrollY = juce::jlimit(0.0, maxScroll, currentScrollY);
        vp->setViewPosition(0, juce::roundToInt(currentScrollY));
    }

    double currentScrollY { 0.0 };
    double targetScrollY { 0.0 };
    double velocity { 0.0 };
};

class FolderBrowserComponent : public juce::Component,
                               public juce::ListBoxModel,
                               public TagDatabaseListener,
                               public AudioEngineListener
{
public:
    FolderBrowserComponent(TagDatabaseManager& db, LibraryScanner& scanner, AudioEngine& engine);
    ~FolderBrowserComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void lookAndFeelChanged() override;

    void setFilteredItems(const std::vector<MediaItem>& items,
                          const juce::String& activeSearchQuery,
                          bool favoritesOnly);

    void expandAllFolders();
    void collapseAllFolders();
    void revealAndExpandRootFolder(const juce::String& folderPath);

    void moveSelection(int delta);
    void selectItemById(const juce::String& itemId, bool triggerPlayback = false);

    // juce::ListBoxModel overrides
    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
    void listBoxItemClicked(int row, const juce::MouseEvent& e) override;
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent& e) override;
    void selectedRowsChanged(int lastRowSelected) override;
    void returnKeyPressed(int lastRowSelected) override;
    juce::var getDragSourceDescription(const juce::SparseSet<int>& rowsToDescribe) override;
    juce::String getTooltipForRow(int row) override;

    // TagDatabaseListener overrides
    void libraryIndexUpdated() override;
    void tagsUpdated() override;

    // AudioEngineListener overrides
    void sampleLoaded(const juce::String& filePath) override;
    void playbackStateChanged(bool isPlaying) override;

    void addListener(SampleTableListener* listener);
    void removeListener(SampleTableListener* listener);

    std::function<void()> onAddFolderRequested;
    std::function<void()> onFoldersChanged;

private:
    struct FolderNode
    {
        juce::String fullPath;
        juce::String normalizedKey;
        juce::String displayName;
        juce::String subtitlePath;
        int depth { 0 };
        bool isScannedRoot { false };
        bool isVirtualOtherRoot { false };
        int totalSampleCount { 0 };
        int totalSubfolderCount { 0 };

        FolderNode* parent { nullptr };
        std::vector<std::unique_ptr<FolderNode>> subfolders;
        std::map<juce::String, FolderNode*> childLookup;
        std::vector<size_t> directItemIndices;
    };

    struct VisibleRow
    {
        bool isFolder { false };
        FolderNode* folder { nullptr };
        FolderNode* parentFolder { nullptr };
        size_t itemIndex { 0 };
        int depth { 0 };
        bool isExpanded { false };
    };

    struct ColumnLayout
    {
        int nameX { 0 };
        int nameW { 260 };
        int tagsX { 260 };
        int tagsW { 220 };
        int durX { 480 };
        int durW { 70 };
        int fmtX { 550 };
        int fmtW { 64 };
        int srX { 614 };
        int srW { 100 };
        int ratingX { 714 };
        int ratingW { 76 };
        int favX { 790 };
        int favW { 40 };
    };

    static ColumnLayout computeColumns(int totalWidth);
    static juce::String normalizePathKey(const juce::String& rawPath);
    static juce::String extractParentDirectory(const juce::String& filePath);
    static juce::String formatDuration(double seconds);

    void buildIconCache();
    void rebuildTree();
    void rebuildVisibleRows();
    void appendNodeToVisibleRows(FolderNode* node);
    void finalizeFolderNode(FolderNode* node);
    void setSubtreeExpanded(FolderNode* node, bool expanded);
    void toggleFolderRow(FolderNode* node, bool recursiveSubtree);

    bool handleKeyNavigation(const juce::KeyPress& key);
    void showFolderContextMenu(FolderNode* folder);
    void showSampleContextMenu(size_t itemIdx, int rowNumber);
    void convertSample(const MediaItem& item);

    TagDatabaseManager& dbManager;
    LibraryScanner& libraryScanner;
    AudioEngine& audioEngine;

    static constexpr int TopToolbarHeight = 36;
    static constexpr int ColumnHeaderHeight = 26;

    juce::Label summaryLabel;
    juce::TextButton expandAllButton { "Expand All" };
    juce::TextButton collapseAllButton { "Collapse All" };
    juce::TextButton emptyStateAddButton { "+ Add Folder" };

    SmoothFolderListBox listBox;

    std::vector<MediaItem> currentItems;
    std::vector<juce::String> currentScanFolders;
    juce::String lastSearchQuery;
    bool currentFavOnly { false };

    std::vector<std::unique_ptr<FolderNode>> rootFolders;
    std::vector<VisibleRow> visibleRows;

    std::set<juce::String> expandedFolderKeys;
    std::set<juce::String> preSearchExpandedKeys;
    bool wasInSearchAutoExpand { false };

    juce::String currentSelectedItemId;
    juce::String currentSelectedFolderKey;
    bool isSynchronizingSelection { false };
    bool suppressSelectionPlayback { false };

    juce::String cachedCurrentFilePath;
    bool cachedIsPlaying { false };

    juce::Image playIconImage;
    juce::Image pauseIconImage;
    juce::Image heartActiveImage;
    juce::Image heartInactiveImage;
    juce::Image starRatingImages[6];

    juce::Font rootFolderFont { juce::FontOptions(13.5f).withStyle("Bold") };
    juce::Font subFolderFont { juce::FontOptions(12.5f).withStyle("Bold") };
    juce::Font pathHintFont { juce::FontOptions(11.5f) };
    juce::Font cellFont { juce::FontOptions(13.0f) };
    juce::Font cellBoldFont { juce::FontOptions(13.0f).withStyle("Bold") };
    juce::Font tagFont { juce::FontOptions(11.0f) };
    juce::Font badgeFont { juce::FontOptions(10.5f).withStyle("Bold") };
    juce::Font headerColFont { juce::FontOptions(11.0f).withStyle("Bold") };

    juce::ListenerList<SampleTableListener> listeners;
    std::unique_ptr<ConvertDialog> convertDialog;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FolderBrowserComponent)
};

} // namespace openwav

