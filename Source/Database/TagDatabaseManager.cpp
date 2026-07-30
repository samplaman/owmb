#include "TagDatabaseManager.h"
#include <algorithm>

namespace openwav
{

TagDatabaseManager::TagDatabaseManager()
{
    loadFromFile();
}

TagDatabaseManager::~TagDatabaseManager()
{
    saveToFile();
}

juce::File TagDatabaseManager::getDatabaseFile() const
{
    auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    auto openWavDir = appData.getChildFile("OpenWav");
    if (!openWavDir.exists())
        openWavDir.createDirectory();
    return openWavDir.getChildFile("library_index.json");
}

std::vector<MediaItem> TagDatabaseManager::getAllItems() const
{
    const juce::ScopedLock sl(lock);
    std::vector<MediaItem> res;
    res.reserve(itemsMap.size());
    for (const auto& kv : itemsMap)
    {
        if (!kv.second.fileName.startsWith(".") && !kv.second.fileName.startsWithIgnoreCase("._"))
            res.push_back(kv.second);
    }
    return res;
}

std::vector<MediaItem> TagDatabaseManager::getFilteredItems(const juce::String& searchKeyword,
                                                             const std::set<juce::String>& selectedTags,
                                                             bool matchAllTags,
                                                             const juce::String& extensionFilter,
                                                             bool favoritesOnly) const
{
    const juce::ScopedLock sl(lock);
    std::vector<MediaItem> filtered;

    juce::String keywordLower = searchKeyword.toLowerCase().trim();
    juce::String extFilterLower = extensionFilter.toLowerCase().trim();

    for (const auto& kv : itemsMap)
    {
        const auto& item = kv.second;

        // 0. Skip hidden files (starting with '.')
        if (item.fileName.startsWith(".") || item.fileName.startsWithIgnoreCase("._"))
            continue;

        // 1. Favorites check
        if (favoritesOnly && !item.isFavorite)
            continue;

        // 2. Extension check
        if (extFilterLower.isNotEmpty() && extFilterLower != "all")
        {
            if (!item.fileExtension.toLowerCase().endsWith(extFilterLower))
                continue;
        }

        // 3. Keyword check (matches filename, path, or tags)
        if (keywordLower.isNotEmpty())
        {
            bool keywordMatched = item.fileName.toLowerCase().contains(keywordLower) ||
                                  item.filePath.toLowerCase().contains(keywordLower);

            if (!keywordMatched)
            {
                for (const auto& tag : item.tags)
                {
                    if (tag.toLowerCase().contains(keywordLower))
                    {
                        keywordMatched = true;
                        break;
                    }
                }
            }

            if (!keywordMatched)
                continue;
        }

        // 4. Selected tags filter
        if (!selectedTags.empty())
        {
            if (matchAllTags)
            {
                // AND mode: item must have all selected tags
                bool hasAll = true;
                for (const auto& reqTag : selectedTags)
                {
                    if (item.tags.find(reqTag) == item.tags.end())
                    {
                        hasAll = false;
                        break;
                    }
                }
                if (!hasAll)
                    continue;
            }
            else
            {
                // OR mode: item must have at least one selected tag
                bool hasAny = false;
                for (const auto& reqTag : selectedTags)
                {
                    if (item.tags.find(reqTag) != item.tags.end())
                    {
                        hasAny = true;
                        break;
                    }
                }
                if (!hasAny)
                    continue;
            }
        }

        filtered.push_back(item);
    }

    return filtered;
}

std::set<juce::String> TagDatabaseManager::getAllKnownTags() const
{
    const juce::ScopedLock sl(lock);
    std::set<juce::String> allTags;
    for (const auto& kv : itemsMap)
    {
        for (const auto& tag : kv.second.tags)
            allTags.insert(tag);
    }
    return allTags;
}

std::map<juce::String, int> TagDatabaseManager::getTagFrequencies() const
{
    const juce::ScopedLock sl(lock);
    std::map<juce::String, int> freqs;
    for (const auto& kv : itemsMap)
    {
        for (const auto& tag : kv.second.tags)
            freqs[tag]++;
    }
    return freqs;
}

void TagDatabaseManager::addOrUpdateItem(const MediaItem& item)
{
    {
        const juce::ScopedLock sl(lock);
        itemsMap[item.id] = item;
    }
    notifyIndexUpdated();
    notifyTagsUpdated();
}

void TagDatabaseManager::addItems(const std::vector<MediaItem>& items)
{
    if (items.empty()) return;

    {
        const juce::ScopedLock sl(lock);
        for (const auto& item : items)
        {
            itemsMap[item.id] = item;
        }
    }
    notifyIndexUpdated();
    notifyTagsUpdated();
    saveToFile();
}

void TagDatabaseManager::addTagToItem(const juce::String& itemId, const juce::String& tag)
{
    auto cleanedTag = tag.trim();
    if (cleanedTag.isEmpty()) return;

    if (!cleanedTag.startsWith("#"))
        cleanedTag = "#" + cleanedTag;

    {
        const juce::ScopedLock sl(lock);
        auto it = itemsMap.find(itemId);
        if (it != itemsMap.end())
        {
            it->second.tags.insert(cleanedTag);
        }
    }
    notifyIndexUpdated();
    notifyTagsUpdated();
    saveToFile();
}

void TagDatabaseManager::removeTagFromItem(const juce::String& itemId, const juce::String& tag)
{
    {
        const juce::ScopedLock sl(lock);
        auto it = itemsMap.find(itemId);
        if (it != itemsMap.end())
        {
            it->second.tags.erase(tag);
        }
    }
    notifyIndexUpdated();
    notifyTagsUpdated();
    saveToFile();
}

void TagDatabaseManager::toggleFavorite(const juce::String& itemId)
{
    {
        const juce::ScopedLock sl(lock);
        auto it = itemsMap.find(itemId);
        if (it != itemsMap.end())
        {
            it->second.isFavorite = !it->second.isFavorite;
        }
    }
    notifyIndexUpdated();
    saveToFile();
}

void TagDatabaseManager::setRating(const juce::String& itemId, int rating)
{
    {
        const juce::ScopedLock sl(lock);
        auto it = itemsMap.find(itemId);
        if (it != itemsMap.end())
        {
            it->second.rating = juce::jlimit(0, 5, rating);
        }
    }
    notifyIndexUpdated();
    saveToFile();
}

void TagDatabaseManager::removeMissingFiles()
{
    bool changed = false;
    {
        const juce::ScopedLock sl(lock);
        for (auto it = itemsMap.begin(); it != itemsMap.end(); )
        {
            if (!juce::File(it->second.filePath).existsAsFile())
            {
                it = itemsMap.erase(it);
                changed = true;
            }
            else
            {
                ++it;
            }
        }
    }

    if (changed)
    {
        notifyIndexUpdated();
        notifyTagsUpdated();
        saveToFile();
    }
}

void TagDatabaseManager::clearLibrary()
{
    {
        const juce::ScopedLock sl(lock);
        itemsMap.clear();
    }
    notifyIndexUpdated();
    notifyTagsUpdated();
    saveToFile();
}

void TagDatabaseManager::clearAllData()
{
    {
        const juce::ScopedLock sl(lock);
        itemsMap.clear();
        scanFolders.clear();
    }

    auto dbFile = getDatabaseFile();
    if (dbFile.existsAsFile())
    {
        dbFile.deleteFile();
    }

    notifyIndexUpdated();
    notifyTagsUpdated();
}

std::set<juce::String> TagDatabaseManager::inferTagsFromPath(const juce::String& filePath)
{
    std::set<juce::String> tags;
    juce::File file(filePath);
    juce::String fullPathLower = filePath.toLowerCase();
    juce::String extLower = file.getFileExtension().toLowerCase();

    // 1. File extension tag
    if (extLower == ".wav") tags.insert("#Wav");
    else if (extLower == ".mp3") tags.insert("#MP3");
    else if (extLower == ".flac") tags.insert("#FLAC");
    else if (extLower == ".ogg") tags.insert("#OGG");
    else if (extLower == ".aif" || extLower == ".aiff") tags.insert("#AIFF");

    // 2. Keyword rules mapping
    struct KeywordRule { const char* keyword; const char* tag; };
    const KeywordRule rules[] = {
        { "kick", "#Kick" }, { "bd", "#Kick" }, { "bassdrum", "#Kick" },
        { "snare", "#Snare" }, { "sd", "#Snare" }, { "rim", "#Snare" },
        { "clap", "#Clap" }, { "snap", "#Clap" },
        { "hihat", "#HiHat" }, { "hat", "#HiHat" }, { "hh", "#HiHat" }, { "openhat", "#HiHat" }, { "closedhat", "#HiHat" },
        { "perc", "#Percussion" }, { "percussion", "#Percussion" }, { "conga", "#Percussion" }, { "bongo", "#Percussion" }, { "tom", "#Percussion" },
        { "cymbal", "#Cymbal" }, { "crash", "#Cymbal" }, { "ride", "#Cymbal" }, { "shaker", "#Percussion" }, { "tamb", "#Percussion" },
        { "loop", "#Loop" }, { "groove", "#Loop" }, { "break", "#Loop" }, { "beat", "#Loop" },
        { "oneshot", "#OneShot" }, { "one_shot", "#OneShot" }, { "hit", "#OneShot" }, { "stab", "#OneShot" },
        { "sub", "#Sub" }, { "bass", "#Bass" }, { "synthbass", "#Bass" },
        { "synth", "#Synth" }, { "lead", "#Lead" }, { "pad", "#Pad" }, { "pluck", "#Synth" }, { "chord", "#Synth" }, { "keys", "#Synth" },
        { "vocal", "#Vocal" }, { "vox", "#Vocal" }, { "chant", "#Vocal" }, { "acapella", "#Vocal" },
        { "fx", "#FX" }, { "sfx", "#FX" }, { "riser", "#FX" }, { "downlifter", "#FX" }, { "impact", "#FX" }, { "sweep", "#FX" },
        { "808", "#808" }, { "acoustic", "#Acoustic" }, { "digital", "#Digital" }, { "ambient", "#Ambient" }
    };

    for (const auto& r : rules)
    {
        if (fullPathLower.contains(r.keyword))
        {
            tags.insert(r.tag);
        }
    }

    return tags;
}

void TagDatabaseManager::loadFromFile()
{
    auto dbFile = getDatabaseFile();
    if (!dbFile.existsAsFile())
        return;

    auto jsonText = dbFile.loadFileAsString();
    auto parsed = juce::JSON::parse(jsonText);
    if (!parsed.isObject())
        return;

    auto* obj = parsed.getDynamicObject();
    if (!obj) return;

    const juce::ScopedLock sl(lock);
    itemsMap.clear();

    if (obj->hasProperty("items"))
    {
        auto itemsVar = obj->getProperty("items");
        if (itemsVar.isArray())
        {
            for (const auto& itemVar : *itemsVar.getArray())
            {
                auto item = MediaItem::fromVar(itemVar);
                if (item.id.isNotEmpty())
                {
                    itemsMap[item.id] = item;
                }
            }
        }
    }

    if (obj->hasProperty("scanFolders"))
    {
        auto foldersVar = obj->getProperty("scanFolders");
        if (foldersVar.isArray())
        {
            for (const auto& fVar : *foldersVar.getArray())
            {
                scanFolders.insert(fVar.toString());
            }
        }
    }

    if (obj->hasProperty("pixeldrainApiKey"))
        pixeldrainApiKey = obj->getProperty("pixeldrainApiKey").toString();

    if (obj->hasProperty("downloadFolder"))
        downloadFolder = obj->getProperty("downloadFolder").toString();
}

void TagDatabaseManager::saveToFile()
{
    auto* rootObj = new juce::DynamicObject();

    juce::Array<juce::var> itemsArray;
    juce::Array<juce::var> foldersArray;

    {
        const juce::ScopedLock sl(lock);
        for (const auto& kv : itemsMap)
        {
            itemsArray.add(kv.second.toVar());
        }
        for (const auto& folder : scanFolders)
        {
            foldersArray.add(folder);
        }
        rootObj->setProperty("pixeldrainApiKey", pixeldrainApiKey);
        rootObj->setProperty("downloadFolder", downloadFolder);
    }

    rootObj->setProperty("items", itemsArray);
    rootObj->setProperty("scanFolders", foldersArray);

    auto jsonString = juce::JSON::toString(juce::var(rootObj), false);
    auto dbFile = getDatabaseFile();
    dbFile.replaceWithText(jsonString);
}

void TagDatabaseManager::addListener(TagDatabaseListener* listener)
{
    listeners.add(listener);
}

void TagDatabaseManager::removeListener(TagDatabaseListener* listener)
{
    listeners.remove(listener);
}

void TagDatabaseManager::notifyIndexUpdated()
{
    listeners.call([](TagDatabaseListener& l) { l.libraryIndexUpdated(); });
}

void TagDatabaseManager::notifyTagsUpdated()
{
    listeners.call([](TagDatabaseListener& l) { l.tagsUpdated(); });
}

void TagDatabaseManager::addScanFolder(const juce::String& folderPath)
{
    {
        const juce::ScopedLock sl(lock);
        scanFolders.insert(folderPath);
    }
    saveToFile();
}

void TagDatabaseManager::removeScanFolder(const juce::String& folderPath)
{
    {
        const juce::ScopedLock sl(lock);
        scanFolders.erase(folderPath);

        for (auto it = itemsMap.begin(); it != itemsMap.end(); )
        {
            if (it->second.filePath.startsWithIgnoreCase(folderPath))
                it = itemsMap.erase(it);
            else
                ++it;
        }
    }
    notifyIndexUpdated();
    notifyTagsUpdated();
    saveToFile();
}

std::vector<juce::String> TagDatabaseManager::getScanFolders() const
{
    const juce::ScopedLock sl(lock);
    std::vector<juce::String> res;
    for (const auto& f : scanFolders)
        res.push_back(f);
    return res;
}

juce::String TagDatabaseManager::getPixeldrainApiKey() const
{
    const juce::ScopedLock sl(lock);
    return pixeldrainApiKey;
}

void TagDatabaseManager::setPixeldrainApiKey(const juce::String& apiKey)
{
    {
        const juce::ScopedLock sl(lock);
        pixeldrainApiKey = apiKey.trim();
    }
    saveToFile();
}

juce::String TagDatabaseManager::getDownloadFolder() const
{
    const juce::ScopedLock sl(lock);
    if (downloadFolder.isEmpty())
    {
        auto userAudio = juce::File::getSpecialLocation(juce::File::userMusicDirectory);
        if (!userAudio.exists())
            userAudio = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
        return userAudio.getChildFile("OWMB Downloads").getFullPathName();
    }
    return downloadFolder;
}

void TagDatabaseManager::setDownloadFolder(const juce::String& folderPath)
{
    {
        const juce::ScopedLock sl(lock);
        downloadFolder = folderPath.trim();
    }
    saveToFile();
}

} // namespace openwav
