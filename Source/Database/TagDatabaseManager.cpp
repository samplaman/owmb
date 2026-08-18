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
            if (extensionFilter.equalsIgnoreCase(".aiff") || extensionFilter.equalsIgnoreCase(".aif") || extensionFilter.equalsIgnoreCase(".aifc"))
            {
                if (!item.fileExtension.equalsIgnoreCase(".aiff") &&
                    !item.fileExtension.equalsIgnoreCase(".aif") &&
                    !item.fileExtension.equalsIgnoreCase(".aifc"))
                    continue;
            }
            else if (!item.fileExtension.endsWithIgnoreCase(extensionFilter))
            {
                continue;
            }
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

void TagDatabaseManager::addItems(const std::vector<MediaItem>& items, bool saveNow, bool notifyListeners)
{
    if (items.empty()) return;

    {
        const juce::ScopedLock sl(lock);
        for (const auto& item : items)
        {
            itemsMap[item.id] = item;
        }
    }
    if (notifyListeners)
    {
        notifyIndexUpdated();
        notifyTagsUpdated();
    }
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

            // Preserve user-added custom tags (tags that don't start with '#')
            std::set<juce::String> customUserTags;
            for (const auto& t : item.tags)
            {
                if (!t.startsWith("#"))
                    customUserTags.insert(t);
            }

            item.tags = inferTagsFromPath(item.filePath, item.durationSeconds, item.numChannels);
            for (const auto& ct : customUserTags)
            {
                item.tags.insert(ct);
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

static bool isKeywordMatch(const juce::String& textLower, const char* keywordStr)
{
    juce::String kw = juce::String(keywordStr).toLowerCase();
    int kwLen = kw.length();
    if (kwLen == 0) return false;

    // Short keywords (<=4 chars or specific ambiguous words) require strict token/word boundary separation
    bool isShortAbbrev = (kwLen <= 4 || kw == "fill" || kw == "reese" || kw == "chant" || kw == "synth" || kw == "house" || kw == "drill" || kw == "lofi" || kw == "stem");

    int pos = 0;
    while ((pos = textLower.indexOf(pos, kw)) != -1)
    {
        bool startOk = false;
        if (pos == 0) {
            startOk = true;
        } else {
            juce::juce_wchar prev = textLower[pos - 1];
            if (isShortAbbrev) {
                startOk = !juce::CharacterFunctions::isLetter(prev);
            } else {
                startOk = !juce::CharacterFunctions::isLetterOrDigit(prev) || prev == '/' || prev == '\\' || prev == '_' || prev == '-';
            }
        }

        bool endOk = false;
        int endPos = pos + kwLen;
        if (endPos >= textLower.length()) {
            endOk = true;
        } else {
            juce::juce_wchar next = textLower[endPos];
            if (isShortAbbrev) {
                endOk = !juce::CharacterFunctions::isLetter(next);
            } else {
                endOk = !juce::CharacterFunctions::isLetterOrDigit(next) || next == '/' || next == '\\' || next == '_' || next == '.' || next == '-';
            }
        }

        if (startOk && endOk)
            return true;

        pos += kwLen;
    }

    return false;
}

std::set<juce::String> TagDatabaseManager::inferTagsFromPath(const juce::String& filePath, double durationSeconds, int numChannels)
{
    std::set<juce::String> tags;
    juce::File file(filePath);
    juce::String fileNameLower = file.getFileNameWithoutExtension().toLowerCase();
    juce::String parentPathLower = file.getParentDirectory().getFullPathName().toLowerCase();
    juce::String fullPathLower = filePath.toLowerCase();
    juce::String extLower = file.getFileExtension().toLowerCase();

    // 1. File extension tag
    if (extLower == ".wav") tags.insert("#Wav");
    else if (extLower == ".mp3") tags.insert("#MP3");
    else if (extLower == ".flac") tags.insert("#FLAC");
    else if (extLower == ".ogg") tags.insert("#OGG");
    else if (extLower == ".aif" || extLower == ".aiff" || extLower == ".aifc") tags.insert("#AIFF");

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
    for (int bpmVal = 60; bpmVal <= 180; ++bpmVal)
    {
        juce::String bpmStr = juce::String(bpmVal);
        if (isKeywordMatch(fileNameLower, (bpmStr + "bpm").toRawUTF8()) ||
            isKeywordMatch(fileNameLower, (bpmStr + "_bpm").toRawUTF8()) ||
            isKeywordMatch(fileNameLower, (bpmStr + " bpm").toRawUTF8()))
        {
            tags.insert("#" + bpmStr + "BPM");
            break;
        }
    }

    // 5. Comprehensive Keyword Rules Dictionary
    struct KeywordRule { const char* keyword; const char* tag; bool isCategory; };
    const KeywordRule rules[] = {
        // Drums & Percussion
        { "subkick", "#SubKick", true }, { "kick", "#Kick", true }, { "bd", "#Kick", true }, { "bassdrum", "#Kick", true },
        { "rimshot", "#Rimshot", true }, { "rim", "#Rimshot", true }, { "snare", "#Snare", true }, { "sd", "#Snare", true },
        { "clap", "#Clap", true }, { "snap", "#Snap", true },
        { "openhat", "#OpenHat", true }, { "open_hat", "#OpenHat", true }, { "open-hat", "#OpenHat", true },
        { "closedhat", "#ClosedHat", true }, { "closed_hat", "#ClosedHat", true }, { "closed-hat", "#ClosedHat", true },
        { "hihat", "#HiHat", true }, { "hat", "#HiHat", true }, { "hh", "#HiHat", true },
        { "tom", "#Tom", true }, { "crash", "#Crash", true }, { "ride", "#Ride", true }, { "cymbal", "#Cymbal", true },
        { "shaker", "#Shaker", true }, { "tambourine", "#Tambourine", true }, { "tamb", "#Tambourine", true },
        { "cowbell", "#Cowbell", true }, { "conga", "#Conga", true }, { "bongo", "#Bongo", true },
        { "percussion", "#Percussion", true }, { "perc", "#Percussion", true },
        { "808", "#808", false }, { "909", "#909", false },

        // Bass
        { "subbass", "#SubBass", true }, { "sub_bass", "#SubBass", true }, { "sub-bass", "#SubBass", true },
        { "synthbass", "#SynthBass", true }, { "synth_bass", "#SynthBass", true },
        { "reesebass", "#ReeseBass", true }, { "reese", "#ReeseBass", true },
        { "slapbass", "#SlapBass", true }, { "808bass", "#808Bass", true },
        { "sub", "#SubBass", true }, { "bass", "#Bass", true },

        // Melodic & Harmonic Instruments
        { "grandpiano", "#Piano", true }, { "piano", "#Piano", true },
        { "rhodes", "#Rhodes", true }, { "epiano", "#Rhodes", true },
        { "organ", "#Organ", true }, { "keys", "#Keys", true },
        { "acguitar", "#AcousticGuitar", true }, { "acousticguitar", "#AcousticGuitar", true }, { "nylon", "#AcousticGuitar", true },
        { "elguitar", "#ElectricGuitar", true }, { "electricguitar", "#ElectricGuitar", true },
        { "guitar", "#Guitar", true }, { "gtr", "#Guitar", true },

        // Orchestral & Classical
        { "orchestra", "#Orchestral", true }, { "orchestral", "#Orchestral", true }, { "symphonic", "#Orchestral", true }, { "symphony", "#Orchestral", true },
        { "ensemble", "#Ensemble", true }, { "tutti", "#Orchestral", true },

        // Orchestral Strings & Articulations
        { "violin", "#Violin", true }, { "violins", "#Violin", true }, { "vln", "#Violin", true },
        { "viola", "#Viola", true }, { "violas", "#Viola", true }, { "vla", "#Viola", true },
        { "cello", "#Cello", true }, { "cellos", "#Cello", true }, { "vc", "#Cello", true }, { "violoncello", "#Cello", true },
        { "doublebass", "#Contrabass", true }, { "contrabass", "#Contrabass", true }, { "upright", "#Contrabass", true }, { "cb", "#Contrabass", true },
        { "pizzicato", "#Pizzicato", false }, { "pizz", "#Pizzicato", false },
        { "staccato", "#Staccato", false }, { "stacc", "#Staccato", false },
        { "spiccato", "#Spiccato", false }, { "spicc", "#Spiccato", false },
        { "legato", "#Legato", false }, { "tremolo", "#Tremolo", false },
        { "strings", "#Strings", true }, { "string", "#Strings", true }, { "str", "#Strings", true },

        // Orchestral Brass & Woodwinds
        { "frenchhorn", "#FrenchHorn", true }, { "french_horn", "#FrenchHorn", true }, { "horn", "#FrenchHorn", true }, { "horns", "#FrenchHorn", true },
        { "trumpet", "#Trumpet", true }, { "trumpets", "#Trumpet", true }, { "tpt", "#Trumpet", true },
        { "trombone", "#Trombone", true }, { "trombones", "#Trombone", true }, { "tbn", "#Trombone", true },
        { "tuba", "#Tuba", true }, { "brass", "#Brass", true },
        { "piccolo", "#Piccolo", true }, { "flute", "#Flute", true }, { "flutes", "#Flute", true }, { "fl", "#Flute", true },
        { "oboe", "#Oboe", true }, { "englishhorn", "#EnglishHorn", true }, { "english_horn", "#EnglishHorn", true },
        { "clarinet", "#Clarinet", true }, { "clarinets", "#Clarinet", true }, { "cl", "#Clarinet", true },
        { "bassoon", "#Bassoon", true }, { "contrabassoon", "#Bassoon", true }, { "woodwind", "#Woodwinds", true }, { "woodwinds", "#Woodwinds", true },

        // Orchestral Percussion & Harp & Choir
        { "timpani", "#Timpani", true }, { "timp", "#Timpani", true }, { "timpanis", "#Timpani", true },
        { "tubularbells", "#TubularBells", true }, { "tubular_bells", "#TubularBells", true }, { "chimes", "#TubularBells", true },
        { "glockenspiel", "#Glockenspiel", true }, { "glock", "#Glockenspiel", true }, { "xylophone", "#Xylophone", true }, { "xylo", "#Xylophone", true },
        { "gong", "#Gong", true }, { "tamtam", "#Gong", true }, { "tam_tam", "#Gong", true },
        { "harp", "#Harp", true }, { "harps", "#Harp", true },
        { "choir", "#Choir", true }, { "chorus", "#Choir", true }, { "vocals_ensemble", "#Choir", true },

        { "saxophone", "#Sax", true }, { "sax", "#Sax", true },
        { "glockenspiel", "#Bell", true }, { "chime", "#Bell", true }, { "bell", "#Bell", true }, { "bells", "#Bell", true },
        { "marimba", "#Marimba", true }, { "kalimba", "#Marimba", true }, { "vibes", "#Marimba", true },
        { "lead", "#Lead", true }, { "pad", "#Pad", true }, { "pluck", "#Pluck", true }, { "arp", "#Arp", true }, { "arpeggio", "#Arp", true },
        { "synth", "#Synth", true },

        // Vocals
        { "acapella", "#Acapella", true }, { "vocalchop", "#VocalChop", true }, { "voxchop", "#VocalChop", true },
        { "chant", "#Chant", true }, { "speech", "#Speech", true }, { "spoken", "#Speech", true },
        { "vocal", "#Vocal", true }, { "vox", "#Vocal", true }, { "vocals", "#Vocal", true },

        // Sound Effects & Textures
        { "uplifter", "#Riser", true }, { "riser", "#Riser", true },
        { "downlifter", "#Downlifter", true }, { "faller", "#Downlifter", true },
        { "subdrop", "#SubDrop", true }, { "impact", "#Impact", true }, { "sweep", "#Sweep", true }, { "whoosh", "#Sweep", true },
        { "foley", "#Foley", true }, { "vinyl", "#Vinyl", true }, { "crackle", "#Vinyl", true },
        { "atmos", "#Atmosphere", true }, { "atmosphere", "#Atmosphere", true }, { "texture", "#Texture", true },
        { "glitch", "#Glitch", true }, { "sfx", "#FX", true }, { "fx", "#FX", true },

        // Genres
        { "boombap", "#BoomBap", false }, { "boom_bap", "#BoomBap", false }, { "trap", "#Trap", false }, { "hiphop", "#HipHop", false }, { "hip_hop", "#HipHop", false },
        { "techhouse", "#TechHouse", false }, { "tech_house", "#TechHouse", false }, { "deephouse", "#DeepHouse", false }, { "deep_house", "#DeepHouse", false },
        { "house", "#House", false }, { "techno", "#Techno", false }, { "trance", "#Trance", false },
        { "dnb", "#DnB", false }, { "drumandbass", "#DnB", false }, { "drum_n_bass", "#DnB", false }, { "dubstep", "#Dubstep", false },
        { "futurebass", "#FutureBass", false }, { "lofi", "#LoFi", false }, { "lo-fi", "#LoFi", false },
        { "ambient", "#Ambient", false }, { "cinematic", "#Cinematic", false }, { "drill", "#Drill", false }, { "synthwave", "#Synthwave", false },
        { "pop", "#Pop", false }, { "rock", "#Rock", false }, { "funk", "#Funk", false }, { "soul", "#Soul", false },

        // Types & Structs
        { "drumloop", "#DrumLoop", false }, { "drum_loop", "#DrumLoop", false }, { "toploop", "#TopLoop", false }, { "top_loop", "#TopLoop", false },
        { "melodicloop", "#MelodicLoop", false }, { "vocalloop", "#VocalLoop", false }, { "percloop", "#PercLoop", false }, { "bassloop", "#BassLoop", false },
        { "fill", "#Fill", false }, { "stem", "#Stem", false }, { "dry", "#Dry", false }, { "wet", "#Wet", false },
        { "loop", "#Loop", false }, { "groove", "#Loop", false }, { "break", "#Loop", false }, { "beat", "#Loop", false },
        { "oneshot", "#OneShot", false }, { "one_shot", "#OneShot", false }, { "hit", "#OneShot", false }, { "stab", "#OneShot", false },
        { "acoustic", "#Acoustic", false }, { "digital", "#Digital", false }
    };

    bool hasFilenameCategory = false;

    // Check filename first for high priority matching
    for (const auto& r : rules)
    {
        if (isKeywordMatch(fileNameLower, r.keyword))
        {
            tags.insert(r.tag);
            if (r.isCategory)
                hasFilenameCategory = true;
        }
    }

    // Only check parent folder path if no category tag matched in filename or for non-category metadata tags
    for (const auto& r : rules)
    {
        if (!r.isCategory || !hasFilenameCategory)
        {
            if (isKeywordMatch(parentPathLower, r.keyword))
            {
                tags.insert(r.tag);
            }
        }
    }

    // Conflict Disambiguation:
    if (tags.find("#Kick") != tags.end() || tags.find("#SubKick") != tags.end())
    {
        tags.erase("#Snare");
        tags.erase("#Percussion");
        tags.erase("#FX");
    }
    else if (tags.find("#Snare") != tags.end())
    {
        tags.erase("#Percussion");
        tags.erase("#FX");
    }
    else if (tags.find("#HiHat") != tags.end() || tags.find("#OpenHat") != tags.end() || tags.find("#ClosedHat") != tags.end())
    {
        tags.erase("#Percussion");
        tags.erase("#FX");
    }
    else if (tags.find("#Bass") != tags.end() || tags.find("#SubBass") != tags.end() || tags.find("#SynthBass") != tags.end())
    {
        tags.erase("#Percussion");
        tags.erase("#FX");
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

    if (obj->hasProperty("primaryColourHex"))
        primaryColourHex = obj->getProperty("primaryColourHex").toString();
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
        rootObj->setProperty("primaryColourHex", primaryColourHex);
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
    const juce::ScopedLock sl(lock);
    listeners.call([](TagDatabaseListener& l) { l.libraryIndexUpdated(); });
}

void TagDatabaseManager::notifyTagsUpdated()
{
    const juce::ScopedLock sl(lock);
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
    std::vector<juce::String> sortedFolders;
    for (const auto& f : scanFolders)
        sortedFolders.push_back(f);
        
    std::sort(sortedFolders.begin(), sortedFolders.end());
    
    std::vector<juce::String> res;
    for (const auto& folderPath : sortedFolders)
    {
        bool isSubFolder = false;
        juce::File f(folderPath);
        for (const auto& parentPath : res)
        {
            if (f.isAChildOf(juce::File(parentPath)))
            {
                isSubFolder = true;
                break;
            }
        }
        if (!isSubFolder)
            res.push_back(folderPath);
    }
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

juce::String TagDatabaseManager::getPrimaryColourHex() const
{
    const juce::ScopedLock sl(lock);
    return primaryColourHex;
}

void TagDatabaseManager::setPrimaryColourHex(const juce::String& hex)
{
    {
        const juce::ScopedLock sl(lock);
        primaryColourHex = hex;
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
