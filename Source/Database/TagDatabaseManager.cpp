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
    stopTimer();
    saveToFile();
}

void TagDatabaseManager::triggerAsyncSave()
{
    startTimer(500); // 500ms debounce
}

void TagDatabaseManager::timerCallback()
{
    stopTimer();
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
    triggerAsyncSave();
}

void TagDatabaseManager::setRating(const juce::String& itemId, int rating)
{
    {
        const juce::ScopedLock sl(lock);
        auto it = itemsMap.find(itemId);
        if (it != itemsMap.end())
        {
            it->second.rating = juce::jlimit(0, 5, rating);
            it->second.precomputeCachedStrings();
        }
    }
    triggerAsyncSave();
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

            // Extract BPM from filename if not already set or if explicitly specified in filename
            double fnBpm = extractBpmFromFilename(juce::File(item.filePath).getFileNameWithoutExtension());
            if (fnBpm > 0.0)
            {
                item.bpm = fnBpm;
            }

            item.tags = inferTagsFromPath(item.filePath, item.durationSeconds, item.numChannels);
            for (const auto& ct : customUserTags)
            {
                item.tags.insert(ct);
            }

            if (item.bpm >= 40.0 && item.bpm <= 260.0)
            {
                item.tags.insert("#" + juce::String(juce::roundToInt(item.bpm)) + "BPM");
            }

            sanitizeTags(item.tags);
        }
    }
    notifyIndexUpdated();
    notifyTagsUpdated();
    saveToFile();
}

static juce::String extractKeyFromFilename(const juce::String& text);

double TagDatabaseManager::extractBpmFromFilename(const juce::String& text)
{
    if (text.isEmpty()) return 0.0;

    juce::String lower = text.toLowerCase();

    // Strategy 1: Explicit BPM indicator (e.g. 128bpm, 128_bpm, 128.5bpm, bpm128, bpm_128, 128 bpm)
    int bpmIdx = 0;
    while ((bpmIdx = lower.indexOf(bpmIdx, "bpm")) != -1)
    {
        // Check number before "bpm"
        int numEnd = bpmIdx;
        while (numEnd > 0 && (lower[numEnd - 1] == '_' || lower[numEnd - 1] == '-' || lower[numEnd - 1] == ' '))
            numEnd--;

        int numStart = numEnd;
        while (numStart > 0 && (juce::CharacterFunctions::isDigit(lower[numStart - 1]) || lower[numStart - 1] == '.' || lower[numStart - 1] == ','))
            numStart--;

        if (numEnd > numStart)
        {
            juce::String numStr = lower.substring(numStart, numEnd).replaceCharacter(',', '.');
            double val = numStr.getDoubleValue();
            if (val >= 40.0 && val <= 260.0)
                return val;
        }

        // Check number after "bpm"
        int afterBpm = bpmIdx + 3;
        while (afterBpm < lower.length() && (lower[afterBpm] == '_' || lower[afterBpm] == '-' || lower[afterBpm] == ' ' || lower[afterBpm] == ':'))
            afterBpm++;

        int afterEnd = afterBpm;
        while (afterEnd < lower.length() && (juce::CharacterFunctions::isDigit(lower[afterEnd]) || lower[afterEnd] == '.' || lower[afterEnd] == ','))
            afterEnd++;

        if (afterEnd > afterBpm)
        {
            juce::String numStr = lower.substring(afterBpm, afterEnd).replaceCharacter(',', '.');
            double val = numStr.getDoubleValue();
            if (val >= 40.0 && val <= 260.0)
                return val;
        }

        bpmIdx += 3;
    }

    // Strategy 2: Standalone numeric tokens in standard tempo range (e.g. "Loop_128_Cm", "140_Dry", "Arp_174_D")
    bool hasLoopIndicator = lower.contains("loop") || lower.contains("arp") || lower.contains("synth") ||
                            lower.contains("stem") || lower.contains("beat") || lower.contains("groove") ||
                            lower.contains("break") || lower.contains("lead") || lower.contains("melody") ||
                            lower.contains("pad") || lower.contains("chord") || lower.contains("riff") ||
                            lower.contains("bassline");

    bool hasKeyIndicator = (extractKeyFromFilename(text).isNotEmpty());

    bool hasOneShotIndicator = lower.contains("kick") || lower.contains("snare") || lower.contains("clap") ||
                               lower.contains("hat") || lower.contains("hihat") || lower.contains("crash") ||
                               lower.contains("ride") || lower.contains("tom") || lower.contains("rim") ||
                               lower.contains("snap") || lower.contains("foley") || lower.contains("shot") ||
                               lower.contains("hit") || lower.contains("stab") || lower.contains("impact") ||
                               lower.contains("sweep") || lower.contains("riser") || lower.contains("fx") ||
                               lower.contains("one_shot") || lower.contains("oneshot");

    // Pure one-shot drum/FX sounds without loop or musical key context must NOT have sample indices (e.g. Kick_075, Snare_103) mistaken for BPM!
    if (hasOneShotIndicator && !hasLoopIndicator && !hasKeyIndicator)
        return 0.0;

    juce::String clean = text;
    for (int i = 0; i < clean.length(); ++i)
    {
        auto c = clean[i];
        if (!juce::CharacterFunctions::isLetterOrDigit(c) && c != '.')
            clean = clean.replaceSection(i, 1, "_");
    }

    juce::StringArray tokens;
    tokens.addTokens(clean, "_", "");

    for (int i = 0; i < tokens.size(); ++i)
    {
        juce::String t = tokens[i].trim();
        if (t.isEmpty()) continue;

        // Reject numbers with leading zeros (e.g. "075", "085", "001") - these are track/sample sequence numbers
        if (t.startsWith("0") && t.length() > 1 && !t.contains("."))
            continue;

        bool isNumeric = true;
        int dotCount = 0;
        for (int j = 0; j < t.length(); ++j)
        {
            if (t[j] == '.')
            {
                dotCount++;
                if (dotCount > 1) { isNumeric = false; break; }
            }
            else if (!juce::CharacterFunctions::isDigit(t[j]))
            {
                isNumeric = false;
                break;
            }
        }

        if (isNumeric && dotCount <= 1)
        {
            double val = t.getDoubleValue();
            if (val >= 60.0 && val <= 220.0)
            {
                // Discard standard audio sample rates: 44.1, 48, 88.2, 96, 192
                if (val == 96.0 || val == 44.1 || val == 48.0 || val == 88.2 || val == 192.0)
                    continue;

                // Avoid version/take/part/sample numbers
                if (i > 0)
                {
                    juce::String prev = tokens[i - 1].toLowerCase();
                    if (prev == "v" || prev == "ver" || prev == "version" || prev == "take" ||
                        prev == "part" || prev == "pt" || prev == "sample" || prev == "track" ||
                        prev == "tr" || prev == "no" || prev == "num")
                        continue;
                }

                // If no loop or key indicator in filename, trailing number is likely a sample number
                if (!hasLoopIndicator && !hasKeyIndicator)
                {
                    if (i > 0 && i == tokens.size() - 1)
                        continue;
                }

                return val;
            }
        }
    }

    return 0.0;
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

void TagDatabaseManager::sanitizeTags(std::set<juce::String>& tags)
{
    bool hasBass = (tags.find("#Bass") != tags.end() || tags.find("#SubBass") != tags.end() ||
                    tags.find("#SynthBass") != tags.end() || tags.find("#ReeseBass") != tags.end() ||
                    tags.find("#SlapBass") != tags.end() || tags.find("#808Bass") != tags.end());

    bool hasKeys = (tags.find("#Piano") != tags.end() || tags.find("#Rhodes") != tags.end() ||
                    tags.find("#Organ") != tags.end() || tags.find("#Keys") != tags.end());

    bool hasGuitar = (tags.find("#Guitar") != tags.end() || tags.find("#AcousticGuitar") != tags.end() ||
                      tags.find("#ElectricGuitar") != tags.end());

    bool hasOrchestral = (tags.find("#Orchestral") != tags.end() || tags.find("#Ensemble") != tags.end() ||
                          tags.find("#Strings") != tags.end() || tags.find("#Violin") != tags.end() ||
                          tags.find("#Viola") != tags.end() || tags.find("#Cello") != tags.end() ||
                          tags.find("#Contrabass") != tags.end() || tags.find("#Brass") != tags.end() ||
                          tags.find("#FrenchHorn") != tags.end() || tags.find("#Trumpet") != tags.end() ||
                          tags.find("#Trombone") != tags.end() || tags.find("#Tuba") != tags.end() ||
                          tags.find("#Woodwinds") != tags.end() || tags.find("#Flute") != tags.end() ||
                          tags.find("#Piccolo") != tags.end() || tags.find("#Oboe") != tags.end() ||
                          tags.find("#EnglishHorn") != tags.end() || tags.find("#Clarinet") != tags.end() ||
                          tags.find("#Bassoon") != tags.end() || tags.find("#Timpani") != tags.end() ||
                          tags.find("#TubularBells") != tags.end() || tags.find("#Glockenspiel") != tags.end() ||
                          tags.find("#Xylophone") != tags.end() || tags.find("#Gong") != tags.end() ||
                          tags.find("#Harp") != tags.end() || tags.find("#Choir") != tags.end() ||
                          tags.find("#Sax") != tags.end() || tags.find("#Marimba") != tags.end() ||
                          tags.find("#Bell") != tags.end());

    bool hasVocal = (tags.find("#Vocal") != tags.end() || tags.find("#Acapella") != tags.end() ||
                     tags.find("#VocalChop") != tags.end() || tags.find("#Chant") != tags.end() ||
                     tags.find("#Speech") != tags.end());

    bool hasSynth = (tags.find("#Synth") != tags.end() || tags.find("#Lead") != tags.end() ||
                     tags.find("#Pad") != tags.end() || tags.find("#Pluck") != tags.end() ||
                     tags.find("#Arp") != tags.end());

    bool hasFX = (tags.find("#FX") != tags.end() || tags.find("#Riser") != tags.end() ||
                  tags.find("#Downlifter") != tags.end() || tags.find("#SubDrop") != tags.end() ||
                  tags.find("#Impact") != tags.end() || tags.find("#Sweep") != tags.end() ||
                  tags.find("#Foley") != tags.end() || tags.find("#Vinyl") != tags.end() ||
                  tags.find("#Atmosphere") != tags.end() || tags.find("#Texture") != tags.end() ||
                  tags.find("#Glitch") != tags.end());

    bool hasOtherDrums = (tags.find("#Snare") != tags.end() || tags.find("#Rimshot") != tags.end() ||
                          tags.find("#Clap") != tags.end() || tags.find("#Snap") != tags.end() ||
                          tags.find("#HiHat") != tags.end() || tags.find("#OpenHat") != tags.end() ||
                          tags.find("#ClosedHat") != tags.end() || tags.find("#Tom") != tags.end() ||
                          tags.find("#Crash") != tags.end() || tags.find("#Ride") != tags.end() ||
                          tags.find("#Cymbal") != tags.end() || tags.find("#Shaker") != tags.end() ||
                          tags.find("#Tambourine") != tags.end() || tags.find("#Cowbell") != tags.end() ||
                          tags.find("#Conga") != tags.end() || tags.find("#Bongo") != tags.end());

    bool hasMusicalKey = false;
    for (const auto& t : tags)
    {
        if (t.startsWith("#Key_"))
        {
            hasMusicalKey = true;
            break;
        }
    }

    bool hasNonKickInstrumentOrFX = hasBass || hasKeys || hasGuitar || hasOrchestral || hasVocal || hasSynth || hasFX || hasOtherDrums;

    if (hasNonKickInstrumentOrFX)
    {
        tags.erase("#Kick");
        tags.erase("#SubKick");
    }
    else if (hasMusicalKey)
    {
        if (tags.find("#808") == tags.end())
        {
            tags.erase("#Kick");
            tags.erase("#SubKick");
        }
    }

    // Drum sub-category mutual exclusivity
    if (tags.find("#Kick") != tags.end() || tags.find("#SubKick") != tags.end())
    {
        tags.erase("#Snare");
        tags.erase("#Rimshot");
        tags.erase("#Clap");
        tags.erase("#Snap");
        tags.erase("#HiHat");
        tags.erase("#OpenHat");
        tags.erase("#ClosedHat");
        tags.erase("#Tom");
        tags.erase("#Percussion");
        tags.erase("#FX");
    }
    else if (tags.find("#Snare") != tags.end() || tags.find("#Rimshot") != tags.end() ||
             tags.find("#Clap") != tags.end() || tags.find("#Snap") != tags.end())
    {
        tags.erase("#Percussion");
        tags.erase("#FX");
        tags.erase("#HiHat");
        tags.erase("#OpenHat");
        tags.erase("#ClosedHat");
        tags.erase("#Tom");
    }
    else if (tags.find("#HiHat") != tags.end() || tags.find("#OpenHat") != tags.end() || tags.find("#ClosedHat") != tags.end())
    {
        tags.erase("#Percussion");
        tags.erase("#FX");
        tags.erase("#Tom");
    }
    else if (tags.find("#Tom") != tags.end())
    {
        tags.erase("#Percussion");
        tags.erase("#FX");
    }
    else if (hasBass)
    {
        tags.erase("#Percussion");
        tags.erase("#FX");
    }

    // Ensure only one #*BPM tag exists and remove BPM tags from one-shots
    bool isOneShotSample = (tags.find("#OneShot") != tags.end() || tags.find("#Short") != tags.end() ||
                            tags.find("#Kick") != tags.end() || tags.find("#SubKick") != tags.end() ||
                            tags.find("#Snare") != tags.end() || tags.find("#Clap") != tags.end() ||
                            tags.find("#HiHat") != tags.end() || tags.find("#ClosedHat") != tags.end() ||
                            tags.find("#OpenHat") != tags.end() || tags.find("#Tom") != tags.end() ||
                            tags.find("#Crash") != tags.end() || tags.find("#Ride") != tags.end() ||
                            tags.find("#Rimshot") != tags.end() || tags.find("#Snap") != tags.end() ||
                            tags.find("#Impact") != tags.end() || tags.find("#Foley") != tags.end());

    bool isExplicitLoopSample = (tags.find("#Loop") != tags.end() || tags.find("#Arp") != tags.end() ||
                                 tags.find("#DrumLoop") != tags.end());

    juce::String chosenBpmTag;
    std::vector<juce::String> bpmTagsToRemove;
    for (const auto& t : tags)
    {
        if (t.startsWith("#") && t.endsWithIgnoreCase("BPM") && t.length() > 4)
        {
            if (isOneShotSample && !isExplicitLoopSample)
            {
                bpmTagsToRemove.push_back(t);
            }
            else
            {
                if (chosenBpmTag.isEmpty())
                    chosenBpmTag = t;
                else
                    bpmTagsToRemove.push_back(t);
            }
        }
    }
    for (const auto& bt : bpmTagsToRemove)
    {
        tags.erase(bt);
    }
}

std::set<juce::String> TagDatabaseManager::inferTagsFromPath(const juce::String& filePath, double durationSeconds, int numChannels)
{
    std::set<juce::String> tags;
    juce::File file(filePath);
    juce::String fileNameLower = file.getFileNameWithoutExtension().toLowerCase();
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

    // 4. BPM Tag Extraction from Filename (e.g. 128BPM, 120_bpm, 95bpm, 174_DNB, BPM125)
    double extractedBpm = extractBpmFromFilename(file.getFileNameWithoutExtension());
    if (extractedBpm <= 0.0)
    {
        // Also check immediate parent folder if filename has no BPM
        extractedBpm = extractBpmFromFilename(file.getParentDirectory().getFileName());
    }

    if (extractedBpm >= 40.0 && extractedBpm <= 260.0)
    {
        tags.insert("#" + juce::String(juce::roundToInt(extractedBpm)) + "BPM");
    }

    // 5. Comprehensive Keyword Rules Dictionary
    struct KeywordRule { const char* keyword; const char* tag; bool isCategory; };
    const KeywordRule rules[] = {
        // Drums & Percussion (Singular & Plural)
        { "subkick", "#SubKick", true }, { "subkicks", "#SubKick", true },
        { "kick", "#Kick", true }, { "kicks", "#Kick", true },
        { "bassdrum", "#Kick", true }, { "bassdrums", "#Kick", true },
        { "bd", "#Kick", true },
        { "rimshot", "#Rimshot", true }, { "rimshots", "#Rimshot", true },
        { "rim", "#Rimshot", true }, { "rims", "#Rimshot", true },
        { "snare", "#Snare", true }, { "snares", "#Snare", true },
        { "sd", "#Snare", true },
        { "clap", "#Clap", true }, { "claps", "#Clap", true },
        { "snap", "#Snap", true }, { "snaps", "#Snap", true },
        { "openhat", "#OpenHat", true }, { "openhats", "#OpenHat", true },
        { "open_hat", "#OpenHat", true }, { "open_hats", "#OpenHat", true },
        { "open-hat", "#OpenHat", true }, { "open-hats", "#OpenHat", true },
        { "closedhat", "#ClosedHat", true }, { "closedhats", "#ClosedHat", true },
        { "closed_hat", "#ClosedHat", true }, { "closed_hats", "#ClosedHat", true },
        { "closed-hat", "#ClosedHat", true }, { "closed-hats", "#ClosedHat", true },
        { "hihat", "#HiHat", true }, { "hihats", "#HiHat", true },
        { "hat", "#HiHat", true }, { "hats", "#HiHat", true },
        { "hh", "#HiHat", true },
        { "tom", "#Tom", true }, { "toms", "#Tom", true },
        { "floortom", "#Tom", true }, { "floortoms", "#Tom", true },
        { "crash", "#Crash", true }, { "crashes", "#Crash", true },
        { "ride", "#Ride", true }, { "rides", "#Ride", true },
        { "cymbal", "#Cymbal", true }, { "cymbals", "#Cymbal", true },
        { "shaker", "#Shaker", true }, { "shakers", "#Shaker", true },
        { "tambourine", "#Tambourine", true }, { "tambourines", "#Tambourine", true },
        { "tamb", "#Tambourine", true },
        { "cowbell", "#Cowbell", true }, { "cowbells", "#Cowbell", true },
        { "conga", "#Conga", true }, { "congas", "#Conga", true },
        { "bongo", "#Bongo", true }, { "bongos", "#Bongo", true },
        { "percussion", "#Percussion", true }, { "percussions", "#Percussion", true },
        { "perc", "#Percussion", true }, { "percs", "#Percussion", true },
        { "808", "#808", false }, { "909", "#909", false },

        // Bass
        { "subbass", "#SubBass", true }, { "sub_bass", "#SubBass", true }, { "sub-bass", "#SubBass", true },
        { "synthbass", "#SynthBass", true }, { "synth_bass", "#SynthBass", true },
        { "reesebass", "#ReeseBass", true }, { "reese", "#ReeseBass", true },
        { "slapbass", "#SlapBass", true },
        { "808bass", "#808Bass", true }, { "808_bass", "#808Bass", true }, { "808-bass", "#808Bass", true }, { "808 bass", "#808Bass", true },
        { "sub", "#SubBass", true }, { "bass", "#Bass", true }, { "basses", "#Bass", true },

        // Melodic & Harmonic Instruments
        { "grandpiano", "#Piano", true }, { "piano", "#Piano", true }, { "pianos", "#Piano", true },
        { "rhodes", "#Rhodes", true }, { "epiano", "#Rhodes", true },
        { "organ", "#Organ", true }, { "organs", "#Organ", true },
        { "keys", "#Keys", true }, { "keyboard", "#Keys", true }, { "keyboards", "#Keys", true },
        { "acguitar", "#AcousticGuitar", true }, { "acousticguitar", "#AcousticGuitar", true }, { "nylon", "#AcousticGuitar", true },
        { "elguitar", "#ElectricGuitar", true }, { "electricguitar", "#ElectricGuitar", true },
        { "guitar", "#Guitar", true }, { "guitars", "#Guitar", true }, { "gtr", "#Guitar", true },

        // Orchestral & Classical
        { "orchestra", "#Orchestral", true }, { "orchestral", "#Orchestral", true },
        { "symphonic", "#Orchestral", true }, { "symphony", "#Orchestral", true },
        { "ensemble", "#Ensemble", true }, { "ensembles", "#Ensemble", true }, { "tutti", "#Orchestral", true },

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
        { "frenchhorn", "#FrenchHorn", true }, { "french_horn", "#FrenchHorn", true },
        { "horn", "#FrenchHorn", true }, { "horns", "#FrenchHorn", true },
        { "trumpet", "#Trumpet", true }, { "trumpets", "#Trumpet", true }, { "tpt", "#Trumpet", true },
        { "trombone", "#Trombone", true }, { "trombones", "#Trombone", true }, { "tbn", "#Trombone", true },
        { "tuba", "#Tuba", true }, { "tubas", "#Tuba", true },
        { "brass", "#Brass", true },
        { "piccolo", "#Piccolo", true }, { "flute", "#Flute", true }, { "flutes", "#Flute", true }, { "fl", "#Flute", true },
        { "oboe", "#Oboe", true }, { "oboes", "#Oboe", true },
        { "englishhorn", "#EnglishHorn", true }, { "english_horn", "#EnglishHorn", true },
        { "clarinet", "#Clarinet", true }, { "clarinets", "#Clarinet", true }, { "cl", "#Clarinet", true },
        { "bassoon", "#Bassoon", true }, { "bassoons", "#Bassoon", true }, { "contrabassoon", "#Bassoon", true },
        { "woodwind", "#Woodwinds", true }, { "woodwinds", "#Woodwinds", true },

        // Orchestral Percussion & Harp & Choir
        { "timpani", "#Timpani", true }, { "timp", "#Timpani", true }, { "timpanis", "#Timpani", true },
        { "tubularbells", "#TubularBells", true }, { "tubular_bells", "#TubularBells", true }, { "chimes", "#TubularBells", true },
        { "glockenspiel", "#Glockenspiel", true }, { "glock", "#Glockenspiel", true },
        { "xylophone", "#Xylophone", true }, { "xylo", "#Xylophone", true },
        { "gong", "#Gong", true }, { "gongs", "#Gong", true }, { "tamtam", "#Gong", true }, { "tam_tam", "#Gong", true },
        { "harp", "#Harp", true }, { "harps", "#Harp", true },
        { "choir", "#Choir", true }, { "choirs", "#Choir", true }, { "chorus", "#Choir", true }, { "vocals_ensemble", "#Choir", true },

        { "saxophone", "#Sax", true }, { "sax", "#Sax", true }, { "saxophones", "#Sax", true },
        { "chime", "#Bell", true }, { "bell", "#Bell", true }, { "bells", "#Bell", true },
        { "marimba", "#Marimba", true }, { "kalimba", "#Marimba", true }, { "vibes", "#Marimba", true },
        { "lead", "#Lead", true }, { "leads", "#Lead", true },
        { "pad", "#Pad", true }, { "pads", "#Pad", true },
        { "pluck", "#Pluck", true }, { "plucks", "#Pluck", true },
        { "arp", "#Arp", true }, { "arpeggio", "#Arp", true }, { "arpeggios", "#Arp", true },
        { "synth", "#Synth", true }, { "synths", "#Synth", true },

        // Vocals
        { "acapella", "#Acapella", true }, { "acapellas", "#Acapella", true },
        { "vocalchop", "#VocalChop", true }, { "vocalchops", "#VocalChop", true },
        { "voxchop", "#VocalChop", true }, { "voxchops", "#VocalChop", true },
        { "chant", "#Chant", true }, { "chants", "#Chant", true },
        { "speech", "#Speech", true }, { "spoken", "#Speech", true },
        { "vocal", "#Vocal", true }, { "vocals", "#Vocal", true }, { "vox", "#Vocal", true },

        // Sound Effects & Textures
        { "uplifter", "#Riser", true }, { "uplifters", "#Riser", true },
        { "riser", "#Riser", true }, { "risers", "#Riser", true },
        { "downlifter", "#Downlifter", true }, { "downlifters", "#Downlifter", true },
        { "faller", "#Downlifter", true }, { "fallers", "#Downlifter", true },
        { "subdrop", "#SubDrop", true }, { "subdrops", "#SubDrop", true },
        { "impact", "#Impact", true }, { "impacts", "#Impact", true },
        { "sweep", "#Sweep", true }, { "sweeps", "#Sweep", true },
        { "whoosh", "#Sweep", true }, { "whooshes", "#Sweep", true },
        { "foley", "#Foley", true },
        { "vinyl", "#Vinyl", true }, { "crackle", "#Vinyl", true },
        { "atmos", "#Atmosphere", true }, { "atmosphere", "#Atmosphere", true }, { "atmospheres", "#Atmosphere", true },
        { "texture", "#Texture", true }, { "textures", "#Texture", true },
        { "glitch", "#Glitch", true }, { "glitches", "#Glitch", true },
        { "sfx", "#FX", true }, { "fx", "#FX", true },

        // Genres
        { "boombap", "#BoomBap", false }, { "boom_bap", "#BoomBap", false },
        { "trap", "#Trap", false },
        { "hiphop", "#HipHop", false }, { "hip_hop", "#HipHop", false },
        { "techhouse", "#TechHouse", false }, { "tech_house", "#TechHouse", false },
        { "deephouse", "#DeepHouse", false }, { "deep_house", "#DeepHouse", false },
        { "house", "#House", false },
        { "techno", "#Techno", false },
        { "trance", "#Trance", false },
        { "dnb", "#DnB", false }, { "drumandbass", "#DnB", false }, { "drum_n_bass", "#DnB", false },
        { "dubstep", "#Dubstep", false },
        { "futurebass", "#FutureBass", false },
        { "lofi", "#LoFi", false }, { "lo-fi", "#LoFi", false },
        { "ambient", "#Ambient", false },
        { "cinematic", "#Cinematic", false },
        { "drill", "#Drill", false },
        { "synthwave", "#Synthwave", false },
        { "pop", "#Pop", false },
        { "rock", "#Rock", false },
        { "funk", "#Funk", false },
        { "soul", "#Soul", false },

        // Types & Structs
        { "drumloop", "#DrumLoop", false }, { "drumloops", "#DrumLoop", false },
        { "drum_loop", "#DrumLoop", false }, { "drum_loops", "#DrumLoop", false },
        { "toploop", "#TopLoop", false }, { "toploops", "#TopLoop", false },
        { "top_loop", "#TopLoop", false }, { "top_loops", "#TopLoop", false },
        { "melodicloop", "#MelodicLoop", false }, { "melodicloops", "#MelodicLoop", false },
        { "vocalloop", "#VocalLoop", false }, { "vocalloops", "#VocalLoop", false },
        { "percloop", "#PercLoop", false }, { "percloops", "#PercLoop", false },
        { "bassloop", "#BassLoop", false }, { "bassloops", "#BassLoop", false },
        { "fill", "#Fill", false }, { "fills", "#Fill", false },
        { "stem", "#Stem", false }, { "stems", "#Stem", false },
        { "dry", "#Dry", false }, { "wet", "#Wet", false },
        { "loop", "#Loop", false }, { "loops", "#Loop", false },
        { "groove", "#Loop", false }, { "grooves", "#Loop", false },
        { "break", "#Loop", false }, { "breaks", "#Loop", false },
        { "beat", "#Loop", false }, { "beats", "#Loop", false },
        { "oneshot", "#OneShot", false }, { "oneshots", "#OneShot", false },
        { "one_shot", "#OneShot", false }, { "one_shots", "#OneShot", false },
        { "hit", "#OneShot", false }, { "hits", "#OneShot", false },
        { "stab", "#OneShot", false }, { "stabs", "#OneShot", false },
        { "acoustic", "#Acoustic", false }, { "digital", "#Digital", false }
    };

    bool hasFilenameCategory = false;

    // Check filename first for high priority matching
    for (const auto& r : rules)
    {
        // For short abbreviation "bd", only match if filename does not have melodic indicators/keys
        if (std::strcmp(r.keyword, "bd") == 0)
        {
            if (keyTag.isNotEmpty() || fileNameLower.contains("chord") || fileNameLower.contains("pad") ||
                fileNameLower.contains("lead") || fileNameLower.contains("piano") || fileNameLower.contains("synth") ||
                fileNameLower.contains("guitar") || fileNameLower.contains("vocal") || fileNameLower.contains("bass") ||
                fileNameLower.contains("breakdown"))
            {
                continue;
            }
        }

        if (isKeywordMatch(fileNameLower, r.keyword))
        {
            tags.insert(r.tag);
            if (r.isCategory)
                hasFilenameCategory = true;
        }
    }

    // Inspect directory hierarchy level-by-level starting from the immediate parent
    // Level 0: Immediate parent folder
    // Level 1: Grandparent folder
    // Level 2: Great-grandparent folder
    juce::File currentDir = file.getParentDirectory();
    for (int depth = 0; depth < 3 && currentDir != juce::File() && currentDir.getFullPathName().isNotEmpty(); ++depth)
    {
        juce::String dirNameLower = currentDir.getFileName().toLowerCase();
        if (dirNameLower.isEmpty() || dirNameLower == "desktop" || dirNameLower == "documents" ||
            dirNameLower == "downloads" || dirNameLower == "users" || dirNameLower == "volumes" ||
            currentDir.isRoot())
            break;

        bool isCompoundFolder = dirNameLower.contains("&") || dirNameLower.contains(" and ") ||
                                dirNameLower.contains(" + ") || dirNameLower.contains(" vs ") ||
                                dirNameLower.contains(" with ");

        bool folderMatchedCategory = false;

        for (const auto& r : rules)
        {
            if (r.isCategory && !hasFilenameCategory)
            {
                // In compound folders, don't blindly assign single categories
                if (isCompoundFolder)
                    continue;

                if (std::strcmp(r.keyword, "bd") == 0)
                {
                    if (dirNameLower != "bd" && dirNameLower != "bds")
                        continue;
                }

                if (isKeywordMatch(dirNameLower, r.keyword))
                {
                    tags.insert(r.tag);
                    folderMatchedCategory = true;
                }
            }
            else if (!r.isCategory)
            {
                if (isKeywordMatch(dirNameLower, r.keyword))
                {
                    tags.insert(r.tag);
                }
            }
        }

        if (folderMatchedCategory)
        {
            hasFilenameCategory = true;
            break;
        }

        currentDir = currentDir.getParentDirectory();
    }

    sanitizeTags(tags);
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
                    if (item.bpm <= 0.0)
                    {
                        item.bpm = extractBpmFromFilename(item.fileName);
                        if (item.bpm <= 0.0)
                        {
                            item.bpm = extractBpmFromFilename(juce::File(item.filePath).getParentDirectory().getFileName());
                        }
                    }
                    if (item.bpm >= 40.0 && item.bpm <= 260.0)
                    {
                        item.tags.insert("#" + juce::String(juce::roundToInt(item.bpm)) + "BPM");
                    }
                    sanitizeTags(item.tags);
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
    {
        pixeldrainApiKey = obj->getProperty("pixeldrainApiKey").toString().trim();
        if (pixeldrainApiKey.isEmpty())
            pixeldrainApiKey = "https://pixeldrain.com/d/BCLFaT9q";
    }

    if (obj->hasProperty("downloadFolder"))
        downloadFolder = obj->getProperty("downloadFolder").toString();

    if (obj->hasProperty("isDarkMode"))
        darkThemeActive = static_cast<bool>(obj->getProperty("isDarkMode"));

    if (obj->hasProperty("primaryColourHex"))
        primaryColourHex = obj->getProperty("primaryColourHex").toString();

    if (obj->hasProperty("uiScale"))
    {
        double s = obj->getProperty("uiScale");
        if (s >= 0.70 && s <= 2.0)
            uiScale = static_cast<float>(s);
    }
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
        rootObj->setProperty("uiScale", static_cast<double>(uiScale));
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
    if (pixeldrainApiKey.trim().isEmpty())
        return "https://pixeldrain.com/d/BCLFaT9q";
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

float TagDatabaseManager::getUiScale() const
{
    const juce::ScopedLock sl(lock);
    return uiScale;
}

void TagDatabaseManager::setUiScale(float scale)
{
    {
        const juce::ScopedLock sl(lock);
        uiScale = juce::jlimit(0.70f, 2.0f, scale);
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
