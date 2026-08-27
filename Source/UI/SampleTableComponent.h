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
    virtual void addToSampleMapRequested(const MediaItem& /*item*/) {}
    virtual void autoSliceToSamplerRequested(const MediaItem& /*item*/) {}
    virtual void editSampleRequested(const MediaItem& /*item*/) {}
};

class SmoothTableListBox : public juce::TableListBox, private juce::Timer
{
public:
    SmoothTableListBox() = default;
    ~SmoothTableListBox() override { stopTimer(); }

    std::function<void()> onScrolled;

    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        if (wheel.deltaY == 0.0f && wheel.deltaX == 0.0f)
            return;

        auto* vp = getViewport();
        if (vp == nullptr)
        {
            juce::TableListBox::mouseWheelMove(e, wheel);
            return;
        }

        auto& sb = vp->getVerticalScrollBar();
        double maxScroll = sb.getMaximumRangeLimit() - sb.getCurrentRangeSize();
        if (maxScroll <= 0.0)
            return;

        if (wheel.isSmooth)
        {
            // High-precision smooth trackpad panning (preserves subpixel response and macOS native inertia)
            double currentY = static_cast<double>(vp->getViewPositionY());
            double deltaPixels = static_cast<double>(wheel.deltaY) * 240.0;
            double newY = juce::jlimit(0.0, maxScroll, currentY - deltaPixels);

            targetScrollY = newY;
            currentScrollY = newY;
            velocity = 0.0;

            vp->setViewPosition(0, juce::roundToInt(newY));
            if (onScrolled) onScrolled();
        }
        else
        {
            // Notched Mouse Wheel: Smooth kinetic spring glide with momentum
            double currentY = static_cast<double>(vp->getViewPositionY());
            if (!isTimerRunning())
            {
                currentScrollY = currentY;
                targetScrollY = currentY;
                velocity = 0.0;
            }

            // Continuous velocity boost & target interpolation
            double step = static_cast<double>(wheel.deltaY) * (getRowHeight() * 3.5);
            targetScrollY = juce::jlimit(0.0, maxScroll, targetScrollY - step);
            velocity -= step * 0.4;

            startTimerHz(60);
        }
    }

    void smoothScrollToRow(int rowNumber)
    {
        auto* vp = getViewport();
        if (vp == nullptr) return;

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
            targetScrollY = targetY + rowH - viewH;
            currentScrollY = currentY;
            startTimerHz(60);
        }
    }

    void syncScrollPosition()
    {
        if (auto* vp = getViewport())
        {
            currentScrollY = vp->getViewPositionY();
            targetScrollY = currentScrollY;
            velocity = 0.0;
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
            if (onScrolled) onScrolled();
            return;
        }

        // Apply smooth critically-damped spring interpolation
        currentScrollY += diff * 0.32;
        currentScrollY = juce::jlimit(0.0, maxScroll, currentScrollY);

        vp->setViewPosition(0, juce::roundToInt(currentScrollY));
        if (onScrolled) onScrolled();
    }

    double currentScrollY { 0.0 };
    double targetScrollY { 0.0 };
    double velocity { 0.0 };
};

class SampleTableComponent : public juce::Component,
                             public juce::TableListBoxModel,
                             public TagDatabaseListener,
                             public AudioEngineListener,
                             public juce::ScrollBar::Listener
{
public:
    static constexpr int InitialRenderChunk = 150;
    static constexpr int RenderChunkIncrement = 150;

    SampleTableComponent(TagDatabaseManager& db, AudioEngine& engine);
    ~SampleTableComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void updateFilter(const juce::String& searchKeyword,
                      const std::set<juce::String>& selectedTags,
                      bool matchAllTags,
                      const juce::String& extensionFilter,
                      bool favoritesOnly);

    const std::vector<MediaItem>& getDisplayedItems() const { return allFilteredItems; }

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
    void selectItemById(const juce::String& itemId, bool triggerPlayback = true);
    void lookAndFeelChanged() override;

    // TagDatabaseListener callbacks
    void libraryIndexUpdated() override;
    void tagsUpdated() override;

    // AudioEngineListener callbacks
    void sampleLoaded(const juce::String& filePath) override;
    void playbackStateChanged(bool isPlaying) override;

    // ScrollBar::Listener callback
    void scrollBarMoved(juce::ScrollBar* scrollBar, double newRangeStart) override;
    void checkAndLoadMoreRows();

    std::function<void(const MediaItem*, const MediaItem*, juce::Point<int>)> onSimilarityHover;

    void addListener(SampleTableListener* listener);
    void removeListener(SampleTableListener* listener);

private:
    void showContextMenuForRow(int rowNumber);
    void convertSample(const MediaItem& item);
    static juce::String formatDuration(double seconds);
    void buildIconCache();

    TagDatabaseManager& dbManager;
    AudioEngine& audioEngine;

    SmoothTableListBox table;
    std::vector<MediaItem> allFilteredItems;
    int renderedItemCount { InitialRenderChunk };
    juce::String currentSelectedItemId;
    bool shouldAutoPlayOnSelection { true };

    // Fast cached audio playback status for instant, zero-lookup paintCell
    juce::String cachedCurrentFilePath;
    bool cachedIsPlaying { false };

    // Fast pre-rendered cached Retina icons for zero CPU drawing overhead
    juce::Image playIconImage;
    juce::Image pauseIconImage;
    juce::Image heartActiveImage;
    juce::Image heartInactiveImage;
    juce::Image starRatingImages[6];

    // Pre-cached fonts for zero-allocation table row painting
    juce::Font cellFont { juce::FontOptions(13.0f) };
    juce::Font cellBoldFont { juce::FontOptions(13.0f).withStyle("Bold") };
    juce::Font tagFont { juce::FontOptions(11.0f) };
    juce::Font badgeFont { juce::FontOptions(10.0f).withStyle("Bold") };

    // Filter State
    juce::String currentKeyword;
    std::set<juce::String> currentSelectedTags;
    bool currentMatchAll { false };
    juce::String currentExtFilter { "All" };
    bool currentFavOnly { false };

    juce::ListenerList<SampleTableListener> listeners;
    bool isSynchronizingSelection { false };
    juce::String similarityTargetId;
    juce::String similarityTargetName;
    MediaItem cachedSimilarityTargetItem;

    juce::Label similarityBannerLabel;
    juce::TextButton clearSimilarityButton { "X" };
    static double calculateDistance(const MediaItem& a, const MediaItem& b);

    std::unique_ptr<ConvertDialog> convertDialog;
};

} // namespace openwav
