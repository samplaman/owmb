#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_core/juce_core.h>
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

class TagDatabaseManager
{
public:
    TagDatabaseManager();
    ~TagDatabaseManager();

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
    void addOrUpdateItem(const MediaItem& item);
    void addItems(const std::vector<MediaItem>& items);
    void addTagToItem(const juce::String& itemId, const juce::String& tag);
    void removeTagFromItem(const juce::String& itemId, const juce::String& tag);
    void toggleFavorite(const juce::String& itemId);
    void setRating(const juce::String& itemId, int rating);
    void setComment(const juce::String& itemId, const juce::String& comment);
    void removeMissingFiles();
    void clearLibrary();
    void clearAllData();

    // Tag Auto-Inference Helper
    static std::set<juce::String> inferTagsFromPath(const juce::String& filePath);

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

    // Theme Settings Persistence
    bool isDarkMode() const;
    void setDarkMode(bool useDark);

private:
    juce::File getDatabaseFile() const;
    void notifyIndexUpdated();
    void notifyTagsUpdated();

    mutable juce::CriticalSection lock;
    std::map<juce::String, MediaItem> itemsMap; // ID -> MediaItem
    std::set<juce::String> scanFolders;
    juce::String pixeldrainApiKey;
    juce::String downloadFolder;
    bool darkThemeActive = false;
    juce::ListenerList<TagDatabaseListener> listeners;
};

} // namespace openwav
