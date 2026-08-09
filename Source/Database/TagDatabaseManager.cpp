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
    filtered.reserve(itemsMap.size());

    juce::String keyword = searchKeyword.trim();

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
        if (extensionFilter.isNotEmpty() && !extensionFilter.equalsIgnoreCase("all"))
        {
            if (!item.fileExtension.endsWithIgnoreCase(extensionFilter))
                continue;
        }

        // 3. Keyword check (matches filename, path, or tags)
        if (keyword.isNotEmpty())
        {
            bool keywordMatched = item.fileName.containsIgnoreCase(keyword) ||
                                  item.filePath.containsIgnoreCase(keyword);

            if (!keywordMatched)
            {
                for (const auto& tag : item.tags)
                {
                    if (tag.containsIgnoreCase(keyword))
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

bool TagDatabaseManager::getItemById(const juce::String& itemId, MediaItem& item) const
{
    const juce::ScopedLock sl(lock);
    auto it = itemsMap.find(itemId);
    if (it != itemsMap.end())
    {
        item = it->second;
        return true;
    }
    return false;
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

void TagDatabaseManager::addItems(const std::vector<MediaItem>& items, bool saveNow)
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
    if (saveNow)
    {
        saveToFile();
    }
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

void TagDatabaseManager::setComment(const juce::String& itemId, const juce::String& comment)
{
    {
        const juce::ScopedLock sl(lock);
        auto it = itemsMap.find(itemId);
        if (it != itemsMap.end())
        {
            it->second.comment = comment.trim();
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

void TagDatabaseManager::reTagAllItems()
{
    {
        const juce::ScopedLock sl(lock);
        for (auto& kv : itemsMap)
        {
            auto& item = kv.second;
            auto inferred = inferTagsFromPath(item.filePath, item.durationSeconds, item.numChannels);
            for (const auto& t : inferred)
            {
                item.tags.insert(t);
            }
            if (item.bpm >= 50.0 && item.bpm <= 200.0)
            {
                item.tags.insert("#" + juce::String(juce::roundToInt(item.bpm)) + "BPM");
            }
        }
    }
    notifyIndexUpdated();
    notifyTagsUpdated();
    saveToFile();
}

static juce::String extractKeyFromFilename(const juce::String& text)
{
    // Search for patterns like _C#m_, _Am_, _F_maj_, _D_min_, Key C, Key-Am, 128_Fm
    juce::String clean = text;
    clean = clean.replaceCharacter('-', '_').replaceCharacter('.', '_').replaceCharacter(' ', '_');

    juce::StringArray tokens;
    tokens.addTokens(clean, "_", "\"'");

    const char* keys[] = {
        "Cmaj", "Cmin", "C#maj", "C#min", "Dbmaj", "Dbmin", "Dmaj", "Dmin",
        "D#maj", "D#min", "Ebmaj", "Ebmin", "Emaj", "Emin", "Fmaj", "Fmin",
        "F#maj", "F#min", "Gbmaj", "Gbmin", "Gmaj", "Gmin", "G#maj", "G#min",
        "Abmaj", "Abmin", "Amaj", "Amin", "A#maj", "A#min", "Bbmaj", "Bbmin",
        "Bmaj", "Bmin",
        "C", "Cm", "C#", "C#m", "Db", "Dbm", "D", "Dm", "D#", "D#m",
        "Eb", "Ebm", "E", "Em", "F", "Fm", "F#", "F#m", "Gb", "Gbm",
        "G", "Gm", "G#", "G#m", "Ab", "Abm", "A", "Am", "A#", "A#m",
        "Bb", "Bbm", "B", "Bm"
    };

    for (const auto& tok : tokens)
    {
        juce::String t = tok.trim();
        if (t.isEmpty()) continue;

        for (const char* k : keys)
        {
            if (t.equalsIgnoreCase(k))
            {
                juce::String keyTag = juce::String(k);
                if (keyTag.endsWithIgnoreCase("maj"))
                    keyTag = keyTag.dropLastCharacters(3) + "_Major";
                else if (keyTag.endsWithIgnoreCase("min"))
                    keyTag = keyTag.dropLastCharacters(3) + "_Minor";
                else if (keyTag.endsWith("m") && keyTag.length() <= 3)
                    keyTag = keyTag.dropLastCharacters(1) + "_Minor";

                return "#Key_" + keyTag;
            }
        }
    }
    return {};
}

std::set<juce::String> TagDatabaseManager::inferTagsFromPath(const juce::String& filePath, double durationSeconds, int numChannels)
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

    // 2. Channel & Duration Tags
    if (numChannels >= 2) tags.insert("#Stereo");
    else if (numChannels == 1) tags.insert("#Mono");

    if (durationSeconds > 0.0)
    {
        if (durationSeconds < 0.4) tags.insert("#Short");
        else if (durationSeconds > 5.0) tags.insert("#Long");
    }

    // 3. Musical Key Detection
    juce::String keyTag = extractKeyFromFilename(file.getFileNameWithoutExtension());
    if (keyTag.isNotEmpty())
    {
        tags.insert(keyTag);
    }

    // 4. BPM Tag Extraction from Filename (e.g. 128BPM, 120_bpm, 95bpm)
    juce::String nameLower = file.getFileNameWithoutExtension().toLowerCase();
    for (int bpmVal = 60; bpmVal <= 180; ++bpmVal)
    {
        juce::String bpmStr = juce::String(bpmVal);
        if (nameLower.contains(bpmStr + "bpm") || nameLower.contains(bpmStr + "_bpm") || nameLower.contains(bpmStr + " bpm"))
        {
            tags.insert("#" + bpmStr + "BPM");
            break;
        }
    }

    // 5. Comprehensive Keyword Rules Dictionary
    struct KeywordRule { const char* keyword; const char* tag; };
    const KeywordRule rules[] = {
        // Drums & Percussion
        { "subkick", "#SubKick" }, { "kick", "#Kick" }, { "bd", "#Kick" }, { "bassdrum", "#Kick" },
        { "rimshot", "#Rimshot" }, { "rim", "#Rimshot" }, { "snare", "#Snare" }, { "sd", "#Snare" },
        { "clap", "#Clap" }, { "snap", "#Snap" },
        { "openhat", "#OpenHat" }, { "open_hat", "#OpenHat" }, { "open-hat", "#OpenHat" },
        { "closedhat", "#ClosedHat" }, { "closed_hat", "#ClosedHat" }, { "closed-hat", "#ClosedHat" },
        { "hihat", "#HiHat" }, { "hat", "#HiHat" }, { "hh", "#HiHat" },
        { "tom", "#Tom" }, { "crash", "#Crash" }, { "ride", "#Ride" }, { "cymbal", "#Cymbal" },
        { "shaker", "#Shaker" }, { "tambourine", "#Tambourine" }, { "tamb", "#Tambourine" },
        { "cowbell", "#Cowbell" }, { "conga", "#Conga" }, { "bongo", "#Bongo" },
        { "percussion", "#Percussion" }, { "perc", "#Percussion" },
        { "808", "#808" }, { "909", "#909" },

        // Bass
        { "subbass", "#SubBass" }, { "sub_bass", "#SubBass" }, { "sub-bass", "#SubBass" },
        { "synthbass", "#SynthBass" }, { "synth_bass", "#SynthBass" },
        { "reesebass", "#ReeseBass" }, { "reese", "#ReeseBass" },
        { "slapbass", "#SlapBass" }, { "808bass", "#808Bass" },
        { "sub", "#SubBass" }, { "bass", "#Bass" },

        // Melodic & Harmonic Instruments
        { "grandpiano", "#Piano" }, { "piano", "#Piano" },
        { "rhodes", "#Rhodes" }, { "epiano", "#Rhodes" },
        { "organ", "#Organ" }, { "keys", "#Keys" },
        { "acguitar", "#AcousticGuitar" }, { "acousticguitar", "#AcousticGuitar" }, { "nylon", "#AcousticGuitar" },
        { "elguitar", "#ElectricGuitar" }, { "electricguitar", "#ElectricGuitar" },
        { "guitar", "#Guitar" }, { "gtr", "#Guitar" },
        { "violin", "#Strings" }, { "cello", "#Strings" }, { "viola", "#Strings" }, { "strings", "#Strings" }, { "orchestral", "#Strings" },
        { "trumpet", "#Brass" }, { "trombone", "#Brass" }, { "horn", "#Brass" }, { "brass", "#Brass" },
        { "saxophone", "#Sax" }, { "sax", "#Sax" },
        { "flute", "#Flute" }, { "clarinet", "#Flute" }, { "woodwind", "#Flute" },
        { "glockenspiel", "#Bell" }, { "chime", "#Bell" }, { "bell", "#Bell" }, { "bells", "#Bell" },
        { "marimba", "#Marimba" }, { "kalimba", "#Marimba" }, { "vibes", "#Marimba" },
        { "lead", "#Lead" }, { "pad", "#Pad" }, { "pluck", "#Pluck" }, { "arp", "#Arp" }, { "arpeggio", "#Arp" },
        { "synth", "#Synth" },

        // Vocals
        { "acapella", "#Acapella" }, { "vocalchop", "#VocalChop" }, { "voxchop", "#VocalChop" },
        { "chant", "#Chant" }, { "speech", "#Speech" }, { "spoken", "#Speech" },
        { "vocal", "#Vocal" }, { "vox", "#Vocal" }, { "vocals", "#Vocal" },

        // Sound Effects & Textures
        { "uplifter", "#Riser" }, { "riser", "#Riser" },
        { "downlifter", "#Downlifter" }, { "faller", "#Downlifter" },
        { "subdrop", "#SubDrop" }, { "impact", "#Impact" }, { "sweep", "#Sweep" }, { "whoosh", "#Sweep" },
        { "foley", "#Foley" }, { "vinyl", "#Vinyl" }, { "crackle", "#Vinyl" },
        { "atmos", "#Atmosphere" }, { "atmosphere", "#Atmosphere" }, { "texture", "#Texture" },
        { "glitch", "#Glitch" }, { "sfx", "#FX" }, { "fx", "#FX" },

        // Genres
        { "boombap", "#BoomBap" }, { "boom_bap", "#BoomBap" }, { "trap", "#Trap" }, { "hiphop", "#HipHop" }, { "hip_hop", "#HipHop" },
        { "techhouse", "#TechHouse" }, { "tech_house", "#TechHouse" }, { "deephouse", "#DeepHouse" }, { "deep_house", "#DeepHouse" },
        { "house", "#House" }, { "techno", "#Techno" }, { "trance", "#Trance" },
        { "dnb", "#DnB" }, { "drumandbass", "#DnB" }, { "drum_n_bass", "#DnB" }, { "dubstep", "#Dubstep" },
        { "futurebass", "#FutureBass" }, { "lofi", "#LoFi" }, { "lo-fi", "#LoFi" },
        { "ambient", "#Ambient" }, { "cinematic", "#Cinematic" }, { "drill", "#Drill" }, { "synthwave", "#Synthwave" },
        { "pop", "#Pop" }, { "rock", "#Rock" }, { "funk", "#Funk" }, { "soul", "#Soul" },

        // Types & Structs
        { "drumloop", "#DrumLoop" }, { "drum_loop", "#DrumLoop" }, { "toploop", "#TopLoop" }, { "top_loop", "#TopLoop" },
        { "melodicloop", "#MelodicLoop" }, { "vocalloop", "#VocalLoop" }, { "percloop", "#PercLoop" }, { "bassloop", "#BassLoop" },
        { "fill", "#Fill" }, { "stem", "#Stem" }, { "dry", "#Dry" }, { "wet", "#Wet" },
        { "loop", "#Loop" }, { "groove", "#Loop" }, { "break", "#Loop" }, { "beat", "#Loop" },
        { "oneshot", "#OneShot" }, { "one_shot", "#OneShot" }, { "hit", "#OneShot" }, { "stab", "#OneShot" },
        { "acoustic", "#Acoustic" }, { "digital", "#Digital" }
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

    if (obj->hasProperty("isDarkMode"))
        darkThemeActive = static_cast<bool>(obj->getProperty("isDarkMode"));
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
        rootObj->setProperty("isDarkMode", darkThemeActive);
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

bool TagDatabaseManager::isDarkMode() const
{
    const juce::ScopedLock sl(lock);
    return darkThemeActive;
}

void TagDatabaseManager::setDarkMode(bool useDark)
{
    {
        const juce::ScopedLock sl(lock);
        darkThemeActive = useDark;
    }
    saveToFile();
}

double TagDatabaseManager::calculateAcousticDistance(const MediaItem& a, const MediaItem& b)
{
    // Normalized feature differences
    double d_zcr = (a.zcr - b.zcr) * 2.2;
    double d_hfr = (a.highFreqRatio - b.highFreqRatio) * 1.5;
    double d_dr = (a.decayRatio - b.decayRatio) * 1.8;
    
    double cf_a = juce::jlimit(1.0, 10.0, a.crestFactor);
    double cf_b = juce::jlimit(1.0, 10.0, b.crestFactor);
    double d_cf = (cf_a - cf_b) * 0.20;

    // Logarithmic duration ratio difference
    double logDurA = std::log10(std::max(0.02, a.durationSeconds) + 0.05);
    double logDurB = std::log10(std::max(0.02, b.durationSeconds) + 0.05);
    double d_dur = (logDurA - logDurB) * 1.8;

    // Channel mismatch penalty
    double d_ch = (a.numChannels != b.numChannels) ? 0.35 : 0.0;

    double baseDist = std::sqrt(d_zcr * d_zcr + d_hfr * d_hfr + d_dr * d_dr + d_cf * d_cf + d_dur * d_dur + d_ch * d_ch);

    // Tag & Category Overlap Weighting Bonus
    int sharedTags = 0;
    for (const auto& t : a.tags)
    {
        if (b.tags.find(t) != b.tags.end() && !t.endsWithIgnoreCase("BPM") && !t.startsWithIgnoreCase("#Key_"))
        {
            sharedTags++;
        }
    }

    double tagDiscount = std::min(0.40, sharedTags * 0.12);
    return std::max(0.0, baseDist - tagDiscount);
}

float TagDatabaseManager::calculateMatchPercentage(const MediaItem& a, const MediaItem& b)
{
    if (a.id == b.id) return 100.0f;
    double dist = calculateAcousticDistance(a, b);
    float match = static_cast<float>(std::max(0.0, (1.0 - (dist / 2.5)) * 100.0));
    return juce::jlimit(5.0f, 99.0f, match);
}

} // namespace openwav
