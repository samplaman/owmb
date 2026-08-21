#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_core/juce_core.h>
 #include <juce_events/juce_events.h>
#endif

#include "../Models/MediaItem.h"
#include <vector>
#include <map>
#include <set>

namespace openwav
{

class TagDatabaseListener
{
public:
    virtual ~TagDatabaseListener() = default;
    virtual void libraryIndexUpdated() = 0;
    virtual void tagsUpdated() = 0;
};

class TagDatabaseManager : private juce::Timer
{
public:
    TagDatabaseManager();
    ~TagDatabaseManager() override;

    void triggerAsyncSave();

    // Data Access & Query
    std::vector<MediaItem> getAllItems() const;
    std::vector<MediaItem> getFilteredItems(const juce::String& searchKeyword,
                                           const std::set<juce::String>& selectedTags,
                                           bool matchAllTags,
                                           const juce::String& extensionFilter,
                                           bool favoritesOnly) const;

    std::set<juce::String> getAllKnownTags() const;
    std::map<juce::String, int> getTagFrequencies() const;

    // Item Operations
    bool getItemById(const juce::String& itemId, MediaItem& item) const;
    void addOrUpdateItem(const MediaItem& item);
    void addItems(const std::vector<MediaItem>& items, bool saveNow = true, bool notifyListeners = true);
    void addTagToItem(const juce::String& itemId, const juce::String& tag);
    void removeTagFromItem(const juce::String& itemId, const juce::String& tag);
    void toggleFavorite(const juce::String& itemId);
    void setRating(const juce::String& itemId, int rating);
    void setComment(const juce::String& itemId, const juce::String& comment);
    void removeMissingFiles();
    void clearLibrary();
    void clearAllData();
    void reTagAllItems(std::function<void(float progress, int processed, int total)> progressCallback = nullptr,
                       std::function<void()> completionCallback = nullptr);

    // Listener Notifications
    void notifyIndexUpdated();
    void notifyTagsUpdated();

    // Tag & Metadata Auto-Inference Helper
    static std::set<juce::String> inferTagsFromPath(const juce::String& filePath, double durationSeconds = 0.0, int numChannels = 0);
    static void sanitizeTags(std::set<juce::String>& tags);
    static double extractBpmFromFilename(const juce::String& text);
    static double calculateAcousticDistance(const MediaItem& a, const MediaItem& b);
    static float calculateMatchPercentage(const MediaItem& a, const MediaItem& b);

    // Persistence
    void loadFromFile();
    void saveToFile();

    // Listeners
    void addListener(TagDatabaseListener* listener);
    void removeListener(TagDatabaseListener* listener);

    // Scan Folders Persistence
    void addScanFolder(const juce::String& folderPath);
    void removeScanFolder(const juce::String& folderPath);
    std::vector<juce::String> getScanFolders() const;

    // Pixeldrain Settings Persistence
    juce::String getPixeldrainApiKey() const;
    void setPixeldrainApiKey(const juce::String& apiKey);
    juce::String getDownloadFolder() const;
    void setDownloadFolder(const juce::String& folderPath);

    // Theme & Rendering Settings Persistence
    bool isDarkMode() const;
    void setDarkMode(bool useDark);
    juce::String getPrimaryColourHex() const;
    void setPrimaryColourHex(const juce::String& hex);
    float getUiScale() const;
    void setUiScale(float scale);

private:
    void timerCallback() override;
    juce::File getDatabaseFile() const;

    mutable juce::CriticalSection lock;
    std::map<juce::String, MediaItem> itemsMap; // ID -> MediaItem
    std::set<juce::String> scanFolders;
    juce::String pixeldrainApiKey { "https://pixeldrain.com/d/BCLFaT9q" };
    juce::String downloadFolder;
    bool darkThemeActive = false;
    juce::String primaryColourHex;
    float uiScale = 1.0f;
    juce::ListenerList<TagDatabaseListener> listeners;
};

} // namespace openwav
