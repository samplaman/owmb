#include "TagDatabaseManager.h"
#include <algorithm>
#include <cmath>
#include <cstring>

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

void TagDatabaseManager::reTagAllItems(std::function<void(float progress, int processed, int total)> progressCallback,
                                       std::function<void()> completionCallback)
{
    juce::Thread::launch([this, progressCallback, completionCallback]() {
        std::vector<juce::String> itemIds;
        {
            const juce::ScopedLock sl(lock);
            itemIds.reserve(itemsMap.size());
            for (const auto& kv : itemsMap)
            {
                itemIds.push_back(kv.first);
            }
        }

        const size_t total = itemIds.size();
        if (total == 0)
        {
            juce::MessageManager::callAsync([completionCallback] {
                if (completionCallback)
                    completionCallback();
            });
            return;
        }

        size_t processed = 0;
        const size_t chunkSize = 50;

        for (size_t i = 0; i < total; ++i)
        {
            {
                const juce::ScopedLock sl(lock);
                auto it = itemsMap.find(itemIds[i]);
                if (it != itemsMap.end())
                {
                    auto& item = it->second;

                    // Preserve user-added custom tags (without '#') and audio-analysis structural/character tags
                    std::set<juce::String> preservedTags;
                    juce::String priorDspCategory;
                    bool hadLoopTag = (item.tags.find("#Loop") != item.tags.end());
                    bool hadOneShotTag = (item.tags.find("#OneShot") != item.tags.end());

                    for (const auto& t : item.tags)
                    {
                        if (!t.startsWith("#") ||
                            t == "#OneShot" || t == "#Loop" || t == "#Silent" ||
                            t == "#Punchy" || t == "#Bright" || t == "#Warm" || t == "#Converted")
                        {
                            preservedTags.insert(t);
                        }
                    }

                    if (hasSemanticCategoryTag(item.tags))
                    {
                        priorDspCategory = getPrimaryCategoryTag(item.tags);
                    }

                    juce::File f(item.filePath);
                    double fnBpm = extractBpmFromFilename(f.getFileNameWithoutExtension());
                    if (fnBpm <= 0.0)
                    {
                        fnBpm = extractBpmFromFilename(f.getParentDirectory().getFileName());
                    }

                    if (fnBpm >= 40.0 && fnBpm <= 260.0)
                    {
                        item.bpm = fnBpm;
                    }
                    else if (!hadLoopTag && (hadOneShotTag || (item.durationSeconds > 0.0 && item.durationSeconds < 1.35)))
                    {
                        item.bpm = 0.0;
                    }

                    item.tags = inferTagsFromPath(item.filePath, item.durationSeconds, item.numChannels);
                    for (const auto& pt : preservedTags)
                    {
                        item.tags.insert(pt);
                    }

                    // Re-apply audio-feature classification if path/filename gave no semantic category
                    if (!hasSemanticCategoryTag(item.tags))
                    {
                        if (item.crestFactor > 0.0 || item.highFreqRatio > 0.0 || item.zcr > 0.0)
                        {
                            classifyFromAudioFeatures(item);
                        }
                        else if (priorDspCategory.isNotEmpty() && priorDspCategory.startsWith("#"))
                        {
                            item.tags.insert(priorDspCategory);
                        }
                    }

                    if (item.bpm >= 40.0 && item.bpm <= 260.0)
                    {
                        item.tags.insert("#" + juce::String(juce::roundToInt(item.bpm)) + "BPM");
                    }

                    sanitizeItem(item);
                    item.precomputeCachedStrings();
                }
            }

            processed++;

            if (progressCallback && (processed % chunkSize == 0 || processed == total))
            {
                float prog = static_cast<float>(processed) / static_cast<float>(total);
                juce::MessageManager::callAsync([progressCallback, prog, processed, total] {
                    progressCallback(prog, static_cast<int>(processed), static_cast<int>(total));
                });
            }
        }

        juce::MessageManager::callAsync([this, completionCallback] {
            notifyIndexUpdated();
            notifyTagsUpdated();
            saveToFile();
            if (completionCallback)
                completionCallback();
        });
    });
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

static juce::String normalizePitchRoot(const juce::String& raw)
{
    if (raw.isEmpty() || raw.length() > 2)
        return {};

    juce::juce_wchar c0 = juce::CharacterFunctions::toUpperCase(raw[0]);
    if (c0 < 'A' || c0 > 'G')
        return {};

    if (raw.length() == 1)
        return juce::String::charToString(c0);

    juce::juce_wchar c1 = raw[1];
    if (c1 == '#')
    {
        if (c0 == 'E' || c0 == 'B') return {};
        return juce::String::charToString(c0) + "#";
    }
    if (c1 == 'b' || c1 == 'B')
    {
        if (c0 == 'C' || c0 == 'F') return {};
        return juce::String::charToString(c0) + "b";
    }
    return {};
}

static bool isTokenLikelyBpm(const juce::String& tok)
{
    juce::String low = tok.toLowerCase();
    if (low.endsWith("bpm"))
        low = low.dropLastCharacters(3);
    else if (low.startsWith("bpm"))
        low = low.substring(3);

    if (low.isEmpty() || (low.startsWith("0") && low.length() > 1))
        return false;

    for (int i = 0; i < low.length(); ++i)
    {
        if (!juce::CharacterFunctions::isDigit(low[i]) && low[i] != '.')
            return false;
    }

    double v = low.getDoubleValue();
    return v >= 60.0 && v <= 220.0;
}

static bool isFmSynthesisWord(const juce::String& lowTok)
{
    return lowTok == "synth" || lowTok == "synths" || lowTok == "bass" || lowTok == "sub" ||
           lowTok == "lead" || lowTok == "leads" || lowTok == "pad" || lowTok == "pads" ||
           lowTok == "pluck" || lowTok == "plucks" || lowTok == "arp" || lowTok == "bell" ||
           lowTok == "bells" || lowTok == "keys" || lowTok == "piano" || lowTok == "epiano" ||
           lowTok == "rhodes" || lowTok == "organ" || lowTok == "kick" || lowTok == "snare" ||
           lowTok == "hat" || lowTok == "hihat" || lowTok == "perc" || lowTok == "drum" ||
           lowTok == "drums" || lowTok == "tom" || lowTok == "clap" || lowTok == "stab" ||
           lowTok == "hit" || lowTok == "fx" || lowTok == "808" || lowTok == "operator";
}

static juce::String extractKeyFromFilename(const juce::String& text)
{
    if (text.isEmpty())
        return {};

    // Normalize common delimiters while preserving original letter casing for FM/dB disambiguation
    juce::String clean = text;
    const char delims[] = { '-', '.', ' ', '(', ')', '[', ']', '{', '}' };
    for (char d : delims)
        clean = clean.replaceCharacter(d, '_');

    juce::StringArray rawTokens;
    rawTokens.addTokens(clean, "_", "\"'");

    std::vector<juce::String> tokens;
    tokens.reserve(static_cast<size_t>(rawTokens.size()));
    for (const auto& rt : rawTokens)
    {
        auto trimmed = rt.trim();
        if (trimmed.isNotEmpty())
            tokens.push_back(trimmed);
    }

    if (tokens.empty())
        return {};

    const int n = static_cast<int>(tokens.size());
    juce::String fullLower = text.toLowerCase();

    // Pass 1: Explicit two-token combinations (e.g. "Key_C", "Key_Am", "F_maj", "D_min", "C#_minor")
    for (int i = 0; i < n - 1; ++i)
    {
        juce::String t0Low = tokens[i].toLowerCase();
        juce::String t1 = tokens[i + 1];
        juce::String t1Low = t1.toLowerCase();

        // "Key C", "Key Am", "Root F#"
        if (t0Low == "key" || t0Low == "root" || t0Low == "pitch")
        {
            if (i + 2 < n)
            {
                juce::String r2 = normalizePitchRoot(t1);
                juce::String t2Low = tokens[i + 2].toLowerCase();
                if (r2.isNotEmpty() && (t2Low == "maj" || t2Low == "major"))
                    return "#Key_" + r2 + "_Major";
                if (r2.isNotEmpty() && (t2Low == "min" || t2Low == "minor" || t2Low == "m"))
                    return "#Key_" + r2 + "_Minor";
            }

            if (t1Low.endsWith("maj") || t1Low.endsWith("major"))
            {
                int drop = t1Low.endsWith("major") ? 5 : 3;
                juce::String r = normalizePitchRoot(t1.dropLastCharacters(drop));
                if (r.isNotEmpty()) return "#Key_" + r + "_Major";
            }
            if (t1Low.endsWith("min") || t1Low.endsWith("minor"))
            {
                int drop = t1Low.endsWith("minor") ? 5 : 3;
                juce::String r = normalizePitchRoot(t1.dropLastCharacters(drop));
                if (r.isNotEmpty()) return "#Key_" + r + "_Minor";
            }
            if (t1Low.endsWith("m") && t1.length() >= 2 && t1.length() <= 3)
            {
                juce::String r = normalizePitchRoot(t1.dropLastCharacters(1));
                if (r.isNotEmpty()) return "#Key_" + r + "_Minor";
            }
            juce::String r = normalizePitchRoot(t1);
            if (r.isNotEmpty())
                return "#Key_" + r;
        }

        // "F_maj", "D_min", "F#_minor", "Bb_major"
        juce::String r0 = normalizePitchRoot(tokens[i]);
        if (r0.isNotEmpty())
        {
            if (t1Low == "maj" || t1Low == "major")
                return "#Key_" + r0 + "_Major";
            if (t1Low == "min" || t1Low == "minor")
                return "#Key_" + r0 + "_Minor";
        }
    }

    // Pass 2: Unambiguous single-token keys (maj/min suffixes, minor 'm', accidentals C#/Bb, or tuned root+octave F#1/G1)
    for (int i = 0; i < n; ++i)
    {
        const juce::String& tok = tokens[i];
        juce::String low = tok.toLowerCase();

        if (low.endsWith("major") || low.endsWith("maj"))
        {
            int drop = low.endsWith("major") ? 5 : 3;
            juce::String r = normalizePitchRoot(tok.dropLastCharacters(drop));
            if (r.isNotEmpty()) return "#Key_" + r + "_Major";
        }

        if (low.endsWith("minor") || low.endsWith("min"))
        {
            int drop = low.endsWith("minor") ? 5 : 3;
            juce::String r = normalizePitchRoot(tok.dropLastCharacters(drop));
            if (r.isNotEmpty()) return "#Key_" + r + "_Minor";
        }

        // Minor shorthand: Cm, C#m, Dbm, Fm, Am, Gm...
        if (low.endsWith("m") && tok.length() >= 2 && tok.length() <= 3)
        {
            // Disambiguate "FM" synthesis (e.g. FM_Bass, FM_Pluck, FM_Synth) from F Minor
            if (low == "fm")
            {
                bool nextIsSynthWord = (i + 1 < n) && isFmSynthesisWord(tokens[i + 1].toLowerCase());
                if (nextIsSynthWord && (tok == "FM" || i == 0))
                    continue;
            }
            // Disambiguate English "I am" or "AM/FM"
            if (low == "am")
            {
                if ((i > 0 && tokens[i - 1].equalsIgnoreCase("i")) ||
                    (i + 1 < n && tokens[i + 1].equalsIgnoreCase("fm")) ||
                    (i > 0 && tokens[i - 1].equalsIgnoreCase("fm")))
                    continue;
            }

            juce::String r = normalizePitchRoot(tok.dropLastCharacters(1));
            if (r.isNotEmpty())
                return "#Key_" + r + "_Minor";
        }

        // Sharp / Flat root without suffix: C#, Db, F#, Gm, Bb, Eb, Ab
        if (tok.length() == 2 && (tok[1] == '#' || tok[1] == 'b' || tok[1] == 'B'))
        {
            // Disambiguate decibels ("dB", "-6_db", "0_db") from D-flat ("Db")
            if (low == "db")
            {
                if (tok == "dB" || tok == "db")
                    continue;
                if (i > 0 && juce::CharacterFunctions::isDigit(tokens[i - 1].getLastCharacter()))
                    continue;
            }
            juce::String r = normalizePitchRoot(tok);
            if (r.isNotEmpty())
                return "#Key_" + r;
        }

        // Root note with octave number: e.g. F#1, C#2, Bb1, G1, F1, C3
        if (tok.length() >= 2 && tok.length() <= 3 && juce::CharacterFunctions::isDigit(tok.getLastCharacter()))
        {
            int oct = tok.getLastCharacter() - '0';
            if (oct >= 0 && oct <= 6)
            {
                juce::String notePart = tok.dropLastCharacters(1);
                if (notePart.length() == 2) // Accidental + octave (e.g. F#1, Bb2) is unambiguous
                {
                    if (!notePart.equalsIgnoreCase("db") || notePart == "Db")
                    {
                        juce::String r = normalizePitchRoot(notePart);
                        if (r.isNotEmpty())
                            return "#Key_" + r;
                    }
                }
                else if (notePart.length() == 1 && oct >= 1 && oct <= 5 && i > 0)
                {
                    // Natural + octave (e.g. Kick_G1, Bass_E1, Piano_C4), avoiding vinyl track prefixes A1/B2 at i==0
                    bool hasTunedContext = fullLower.contains("kick") || fullLower.contains("808") ||
                                           fullLower.contains("bass") || fullLower.contains("sub") ||
                                           fullLower.contains("piano") || fullLower.contains("keys") ||
                                           fullLower.contains("synth") || fullLower.contains("pluck") ||
                                           fullLower.contains("pad") || fullLower.contains("lead") ||
                                           fullLower.contains("tom") || fullLower.contains("bell") ||
                                           fullLower.contains("harp") || fullLower.contains("guitar");
                    if (hasTunedContext)
                    {
                        juce::String r = normalizePitchRoot(notePart);
                        if (r.isNotEmpty())
                            return "#Key_" + r;
                    }
                }
            }
        }
    }

    // Pass 3: Bare single-letter keys (A..G) ONLY when directly adjacent to a BPM token in non-unpitched context
    // Prevents false positives like "Kick_A.wav", "Snare_B.wav", "A_Warm_Pad.wav", "G_Funk.wav"
    bool isUnpitchedPerc = fullLower.contains("snare") || fullLower.contains("clap") || fullLower.contains("snap") ||
                           fullLower.contains("hat") || fullLower.contains("hihat") || fullLower.contains("shaker") ||
                           fullLower.contains("tamb") || fullLower.contains("crash") || fullLower.contains("ride") ||
                           fullLower.contains("cymbal") || fullLower.contains("rim") || fullLower.contains("cowbell") ||
                           fullLower.contains("conga") || fullLower.contains("bongo") || fullLower.contains("foley") ||
                           fullLower.contains("impact") || fullLower.contains("riser") || fullLower.contains("sweep");

    if (!isUnpitchedPerc)
    {
        for (int i = 0; i < n; ++i)
        {
            if (tokens[i].length() == 1)
            {
                juce::String r = normalizePitchRoot(tokens[i]);
                if (r.isNotEmpty())
                {
                    bool prevIsBpm = (i > 0) && isTokenLikelyBpm(tokens[i - 1]);
                    bool nextIsBpm = (i + 1 < n) && isTokenLikelyBpm(tokens[i + 1]);
                    if (prevIsBpm || nextIsBpm)
                        return "#Key_" + r;
                }
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
    bool isShortAbbrev = (kwLen <= 4 || kw == "fill" || kw == "reese" || kw == "chant" ||
                          kw == "synth" || kw == "house" || kw == "drill" || kw == "lofi" ||
                          kw == "stem" || kw == "clave" || kw == "conga" || kw == "bongo");

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

        pos += 1;
    }

    return false;
}

bool TagDatabaseManager::isTechnicalOrModifierTag(const juce::String& tag)
{
    if (tag.isEmpty()) return true;
    if (tag.startsWithIgnoreCase("#Key_") || (tag.startsWith("#") && tag.endsWithIgnoreCase("BPM") && tag.length() > 4))
        return true;

    static const std::set<juce::String> nonCategoryTags = {
        "#Wav", "#MP3", "#FLAC", "#OGG", "#AIFF",
        "#Stereo", "#Mono", "#Short", "#Long",
        "#OneShot", "#Loop", "#Silent", "#Converted",
        "#Punchy", "#Bright", "#Warm",
        "#Dry", "#Wet", "#Acoustic", "#Digital", "#Fill", "#Stem",
        "#Pizzicato", "#Staccato", "#Spiccato", "#Legato", "#Tremolo",
        "#808", "#909",
        "#BoomBap", "#Trap", "#HipHop", "#TechHouse", "#DeepHouse", "#House",
        "#Techno", "#Trance", "#DnB", "#Dubstep", "#FutureBass", "#LoFi",
        "#Ambient", "#Cinematic", "#Drill", "#Synthwave", "#Pop", "#Rock", "#Funk", "#Soul"
    };
    return nonCategoryTags.find(tag) != nonCategoryTags.end();
}

bool TagDatabaseManager::hasSemanticCategoryTag(const std::set<juce::String>& tags)
{
    for (const auto& t : tags)
    {
        if (t.startsWith("#") && !isTechnicalOrModifierTag(t))
            return true;
    }
    return false;
}

juce::String TagDatabaseManager::getPrimaryCategoryTag(const std::set<juce::String>& tags)
{
    if (tags.empty())
        return "Untagged";

    // Single Source of Truth priority order across OWMB (Database, Cloud Map, Similarity, Table)
    static const std::vector<juce::String> priorityList = {
        // 1. Drums & Percussion (Specific before general; always beats #808/#909 modifier!)
        "#SubKick", "#Kick",
        "#Rimshot", "#Snare", "#Clap", "#Snap",
        "#OpenHat", "#ClosedHat", "#HiHat",
        "#Tom", "#Crash", "#Ride", "#Cymbal",
        "#Cowbell", "#Conga", "#Bongo", "#Shaker", "#Tambourine", "#Percussion",

        // 2. Bass (Specific before general)
        "#SubBass", "#ReeseBass", "#SlapBass", "#SynthBass", "#808Bass", "#Bass",

        // 3. Keys & Guitars
        "#Piano", "#Rhodes", "#Organ", "#Keys",
        "#AcousticGuitar", "#ElectricGuitar", "#Guitar",

        // 4. Orchestral, Strings, Brass, Woodwinds, Mallets & Choir
        "#Violin", "#Viola", "#Cello", "#Contrabass", "#Strings",
        "#FrenchHorn", "#Trumpet", "#Trombone", "#Tuba", "#Brass",
        "#Piccolo", "#Flute", "#Oboe", "#EnglishHorn", "#Clarinet", "#Bassoon", "#Sax", "#Woodwinds",
        "#Timpani", "#TubularBells", "#Glockenspiel", "#Xylophone", "#Marimba", "#Bell", "#Gong", "#Harp", "#Choir",
        "#Ensemble", "#Orchestral",

        // 5. Vocals
        "#Acapella", "#VocalChop", "#Chant", "#Speech", "#Vocal",

        // 6. Synths
        "#Lead", "#Pluck", "#Arp", "#Pad", "#Synth",

        // 7. FX & Textures
        "#SubDrop", "#Impact", "#Riser", "#Downlifter", "#Sweep", "#Glitch", "#Vinyl", "#Foley", "#Atmosphere", "#Texture", "#FX",

        // 8. Typed Loops & Generic Loop
        "#DrumLoop", "#TopLoop", "#PercLoop", "#BassLoop", "#VocalLoop", "#MelodicLoop", "#Loop",

        // 9. Drum Machine / Structural Fallbacks
        "#808", "#909", "#OneShot"
    };

    for (const auto& prio : priorityList)
    {
        if (tags.find(prio) != tags.end())
            return prio;
    }

    for (const auto& t : tags)
    {
        if (!t.endsWithIgnoreCase("BPM") && !t.startsWithIgnoreCase("#Key_") &&
            t != "#Wav" && t != "#MP3" && t != "#FLAC" && t != "#OGG" && t != "#AIFF" &&
            t != "#Stereo" && t != "#Mono" && t != "#Short" && t != "#Long" &&
            t != "#Punchy" && t != "#Bright" && t != "#Warm" &&
            t != "#Dry" && t != "#Wet" && t != "#Silent" && t != "#Converted")
        {
            return t;
        }
    }

    return *tags.begin();
}

juce::String TagDatabaseManager::getCanonicalCategory(const juce::String& tag)
{
    juce::String t = tag.trim().toLowerCase();
    if (t.startsWith("#"))
        t = t.substring(1);

    if (t.isEmpty() || t == "untagged")
        return "other";

    if (t == "subkick" || t == "kick") return "kick";
    if (t == "snare") return "snare";
    if (t == "rimshot" || t == "rim" || t == "clave") return "clave/rim";
    if (t == "clap") return "clap";
    if (t == "snap") return "snap";
    if (t == "closedhat" || t == "closed_hat" || t == "closed-hat") return "hihat closed";
    if (t == "openhat" || t == "open_hat" || t == "open-hat") return "hihat open";
    if (t == "hihat" || t == "hat") return "hihat";
    if (t == "tom") return "tom";
    if (t == "crash") return "crash";
    if (t == "ride") return "ride";
    if (t == "cymbal") return "cymbal";
    if (t == "shaker" || t == "tambourine" || t == "tamb") return "tambourine shaker";
    if (t == "cowbell") return "cowbell";
    if (t == "conga" || t == "bongo") return "bongo/conga";
    if (t == "percussion" || t == "perc" || t == "percloop" || t == "toploop") return "percussion";

    if (t == "subbass" || t == "synthbass" || t == "reesebass" || t == "slapbass" ||
        t == "808bass" || t == "bass" || t == "bassloop" || t == "808")
        return "bass";

    if (t == "piano" || t == "rhodes" || t == "organ" || t == "keys") return "keys";
    if (t == "acousticguitar" || t == "electricguitar" || t == "guitar" || t == "gtr") return "guitar";

    if (t == "violin" || t == "viola" || t == "cello" || t == "contrabass" || t == "strings" || t == "string")
        return "strings";
    if (t == "frenchhorn" || t == "trumpet" || t == "trombone" || t == "tuba" || t == "brass" || t == "horn")
        return "brass";
    if (t == "piccolo" || t == "flute" || t == "oboe" || t == "englishhorn" ||
        t == "clarinet" || t == "bassoon" || t == "sax" || t == "woodwinds" || t == "woodwind")
        return "woodwinds";
    if (t == "timpani" || t == "tubularbells" || t == "glockenspiel" || t == "xylophone" ||
        t == "marimba" || t == "bell" || t == "gong" || t == "harp" || t == "ensemble" || t == "orchestral")
        return "orchestral";

    if (t == "acapella" || t == "vocalchop" || t == "chant" || t == "speech" ||
        t == "vocal" || t == "vox" || t == "vocalloop" || t == "choir")
        return "vocal";

    if (t == "lead" || t == "pad" || t == "pluck" || t == "arp" || t == "synth" || t == "melodicloop")
        return "synth";

    if (t == "subdrop" || t == "impact" || t == "riser" || t == "downlifter" || t == "sweep" ||
        t == "glitch" || t == "vinyl" || t == "foley" || t == "atmosphere" || t == "texture" ||
        t == "fx" || t == "sfx")
        return "fx";

    if (t == "drumloop" || t == "loop" || t == "909") return "loop";
    if (t == "oneshot" || t == "one_shot") return "other";

    // Substring fallback for custom user tags (e.g. "my_kick", "warm pad")
    if (t.contains("kick")) return "kick";
    if (t.contains("snare")) return "snare";
    if (t.contains("clap")) return "clap";
    if (t.contains("closed") && t.contains("hat")) return "hihat closed";
    if (t.contains("open") && t.contains("hat")) return "hihat open";
    if (t.contains("hihat") || isKeywordMatch(t, "hat")) return "hihat";
    if (isKeywordMatch(t, "tom")) return "tom";
    if (isKeywordMatch(t, "rim") || t.contains("rimshot") || t.contains("clave")) return "clave/rim";
    if (t.contains("crash")) return "crash";
    if (t.contains("ride")) return "ride";
    if (t.contains("cymbal")) return "cymbal";
    if (t.contains("shaker") || t.contains("tamb")) return "tambourine shaker";
    if (t.contains("cowbell")) return "cowbell";
    if (t.contains("conga") || t.contains("bongo")) return "bongo/conga";
    if (t.contains("perc")) return "percussion";
    if (t.contains("bass")) return "bass";
    if (t.contains("piano") || t.contains("rhodes") || t.contains("organ") || t.contains("keys")) return "keys";
    if (t.contains("guitar") || t.contains("gtr")) return "guitar";
    if (t.contains("string") || t.contains("violin") || t.contains("cello") || t.contains("viola")) return "strings";
    if (t.contains("brass") || t.contains("trumpet") || t.contains("trombone") || t.contains("horn")) return "brass";
    if (t.contains("flute") || t.contains("clarinet") || t.contains("oboe") || t.contains("sax") || t.contains("woodwind")) return "woodwinds";
    if (t.contains("vocal") || t.contains("vox") || t.contains("choir") || t.contains("acapella")) return "vocal";
    if (t.contains("synth") || t.contains("lead") || t.contains("pad") || t.contains("pluck") || t.contains("arp")) return "synth";
    if (t.contains("riser") || t.contains("sweep") || t.contains("impact") || t.contains("glitch") || t.contains("foley") || isKeywordMatch(t, "fx")) return "fx";
    if (t.contains("loop")) return "loop";

    return t;
}

juce::String TagDatabaseManager::getCanonicalCategoryForItem(const MediaItem& item)
{
    // 1. Evaluate using strict priority order (never alphabetical std::set iteration!)
    if (!item.tags.empty())
    {
        juce::String primary = getPrimaryCategoryTag(item.tags);
        if (primary != "Untagged" && primary != "#OneShot" && !isTechnicalOrModifierTag(primary))
        {
            juce::String cat = getCanonicalCategory(primary);
            if (cat != "other")
                return cat;
        }
        // Handle standalone #808 or #Loop when no specific instrument tag is present
        if (primary == "#808") return "bass";
        if (primary == "#Loop") return "loop";
    }

    // 2. Word-boundary-safe path inference fallback if item had not been tagged yet
    if (item.filePath.isNotEmpty())
    {
        auto inferred = inferTagsFromPath(item.filePath, item.durationSeconds, item.numChannels);
        juce::String primary = getPrimaryCategoryTag(inferred);
        if (primary != "Untagged" && primary != "#OneShot")
        {
            juce::String cat = getCanonicalCategory(primary);
            if (cat != "other")
                return cat;
        }
    }

    // 3. Sample-rate-normalized DSP feature fallback
    if (item.durationSeconds > 0.0 && (item.crestFactor > 0.0 || item.highFreqRatio > 0.0 || item.zcr > 0.0))
    {
        MediaItem temp = item;
        classifyFromAudioFeatures(temp);
        juce::String primary = getPrimaryCategoryTag(temp.tags);
        if (primary != "Untagged" && primary != "#OneShot")
        {
            juce::String cat = getCanonicalCategory(primary);
            if (cat != "other")
                return cat;
        }
    }

    return "other";
}

bool TagDatabaseManager::areCategoriesCompatible(const juce::String& catA, const juce::String& catB)
{
    if (catA == catB)
        return true;

    // Treat generic "hihat" as compatible with closed/open hi-hats
    if ((catA == "hihat" && (catB == "hihat closed" || catB == "hihat open")) ||
        (catB == "hihat" && (catA == "hihat closed" || catA == "hihat open")))
        return true;

    // Treat crash/ride/cymbal family as compatible when searching metallic cymbals
    if ((catA == "cymbal" && (catB == "crash" || catB == "ride")) ||
        (catB == "cymbal" && (catA == "crash" || catA == "ride")))
        return true;

    return false;
}

void TagDatabaseManager::classifyFromAudioFeatures(MediaItem& item)
{
    if (hasSemanticCategoryTag(item.tags))
        return;

    const bool isLoop = (item.tags.find("#Loop") != item.tags.end()) ||
                        (item.durationSeconds >= 1.35 && item.decayRatio >= 0.26);

    if (!isLoop)
    {
        if (item.durationSeconds <= 1.10 && item.decayRatio < 0.28 &&
            item.highFreqRatio < 0.22 && item.zcr < 0.060 && item.crestFactor > 2.2)
        {
            item.tags.insert("#Kick");
        }
        else if (item.durationSeconds > 0.45 && item.highFreqRatio < 0.20 &&
                 item.zcr < 0.055 && item.decayRatio >= 0.20)
        {
            item.tags.insert("#Bass");
        }
        else if (item.highFreqRatio >= 1.05 || item.zcr >= 0.20)
        {
            if (item.durationSeconds <= 0.28 && item.decayRatio < 0.20)
                item.tags.insert("#ClosedHat");
            else if (item.durationSeconds <= 1.10)
                item.tags.insert("#OpenHat");
            else
                item.tags.insert("#Cymbal");
        }
        else if (item.highFreqRatio >= 0.42 && item.highFreqRatio < 1.05 && item.zcr >= 0.075)
        {
            if (item.decayRatio < 0.16 && item.durationSeconds < 0.55)
                item.tags.insert("#Clap");
            else
                item.tags.insert("#Snare");
        }
        else if (item.highFreqRatio < 0.35 && item.zcr < 0.085 &&
                 item.crestFactor > 3.0 && item.durationSeconds <= 1.10 && item.decayRatio < 0.30)
        {
            item.tags.insert("#Tom");
        }
        else if (item.crestFactor > 3.5 && item.durationSeconds <= 0.90)
        {
            item.tags.insert("#Percussion");
        }
        else if (item.durationSeconds >= 0.85 && item.decayRatio >= 0.45)
        {
            if (item.highFreqRatio >= 0.55)
                item.tags.insert("#FX");
            else
                item.tags.insert("#Pad");
        }
        else if (item.decayRatio < 0.24)
        {
            item.tags.insert("#Pluck");
        }
        else
        {
            item.tags.insert("#Synth");
        }
    }
    else
    {
        if (item.highFreqRatio < 0.14 && item.zcr < 0.045)
        {
            item.tags.insert("#BassLoop");
            item.tags.insert("#Bass");
        }
        else if (item.crestFactor > 3.4 && item.highFreqRatio > 0.30)
        {
            item.tags.insert("#DrumLoop");
        }
        else
        {
            item.tags.insert("#MelodicLoop");
            item.tags.insert("#Synth");
        }
    }
}

void TagDatabaseManager::sanitizeTags(std::set<juce::String>& tags)
{
    // 1. Format / Channel / Length hygiene
    if (tags.find("#Stereo") != tags.end() && tags.find("#Mono") != tags.end())
        tags.erase("#Mono");
    if (tags.find("#Short") != tags.end() && tags.find("#Long") != tags.end())
        tags.erase("#Long");

    // 2. Typed Loops imply #Loop and exclude #OneShot
    bool hasTypedLoop = (tags.find("#DrumLoop") != tags.end() || tags.find("#TopLoop") != tags.end() ||
                         tags.find("#PercLoop") != tags.end() || tags.find("#BassLoop") != tags.end() ||
                         tags.find("#VocalLoop") != tags.end() || tags.find("#MelodicLoop") != tags.end());
    if (hasTypedLoop)
    {
        tags.insert("#Loop");
        tags.erase("#OneShot");
    }
    else if (tags.find("#Loop") != tags.end() && tags.find("#OneShot") != tags.end())
    {
        if (tags.find("#Short") != tags.end())
            tags.erase("#Loop");
        else
            tags.erase("#OneShot");
    }

    // 3.SubKick beats falsely inferred #SubBass (e.g. "Sub_Kick_01.wav")
    if (tags.find("#SubKick") != tags.end())
    {
        tags.erase("#SubBass");
    }

    bool hasSpecificBass = (tags.find("#SubBass") != tags.end() || tags.find("#SynthBass") != tags.end() ||
                            tags.find("#ReeseBass") != tags.end() || tags.find("#SlapBass") != tags.end() ||
                            tags.find("#808Bass") != tags.end());
    bool hasBass = hasSpecificBass || (tags.find("#Bass") != tags.end());

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

    bool hasSpecificSynth = (tags.find("#Lead") != tags.end() || tags.find("#Pad") != tags.end() ||
                             tags.find("#Pluck") != tags.end() || tags.find("#Arp") != tags.end());

    bool hasSpecificFX = (tags.find("#Riser") != tags.end() || tags.find("#Downlifter") != tags.end() ||
                          tags.find("#SubDrop") != tags.end() || tags.find("#Impact") != tags.end() ||
                          tags.find("#Sweep") != tags.end() || tags.find("#Foley") != tags.end() ||
                          tags.find("#Vinyl") != tags.end() || tags.find("#Atmosphere") != tags.end() ||
                          tags.find("#Texture") != tags.end() || tags.find("#Glitch") != tags.end());

    bool hasOtherDrums = (tags.find("#Snare") != tags.end() || tags.find("#Rimshot") != tags.end() ||
                          tags.find("#Clap") != tags.end() || tags.find("#Snap") != tags.end() ||
                          tags.find("#HiHat") != tags.end() || tags.find("#OpenHat") != tags.end() ||
                          tags.find("#ClosedHat") != tags.end() || tags.find("#Tom") != tags.end() ||
                          tags.find("#Crash") != tags.end() || tags.find("#Ride") != tags.end() ||
                          tags.find("#Cymbal") != tags.end() || tags.find("#Shaker") != tags.end() ||
                          tags.find("#Tambourine") != tags.end() || tags.find("#Cowbell") != tags.end() ||
                          tags.find("#Conga") != tags.end() || tags.find("#Bongo") != tags.end());

    bool hasKick = (tags.find("#Kick") != tags.end() || tags.find("#SubKick") != tags.end());
    bool isLoopSample = (tags.find("#Loop") != tags.end() || hasTypedLoop);

    // 4. Multi-drum loops (e.g. "Kick_Snare_Loop.wav") become #DrumLoop rather than a single drum one-shot
    if (isLoopSample && hasKick && hasOtherDrums)
    {
        tags.insert("#DrumLoop");
        tags.erase("#Kick");
        tags.erase("#SubKick");
        tags.erase("#Snare");
        tags.erase("#Rimshot");
        tags.erase("#Clap");
        tags.erase("#Snap");
        tags.erase("#HiHat");
        tags.erase("#OpenHat");
        tags.erase("#ClosedHat");
        hasKick = false;
        hasOtherDrums = false;
    }

    // 5. Note: Tuned kicks (#Key_* + #Kick) MUST KEEP #Kick!
    // Only remove #Kick when an incompatible melodic/vocal/specific-FX/bass/other-drum instrument is present.
    // Generic #Synth or #FX on a kick (e.g. "Synth_Kick.wav", "FX_Kick.wav") yields to #Kick!
    bool hasIncompatibleWithKick = hasBass || hasKeys || hasGuitar || hasOrchestral ||
                                   hasVocal || hasSpecificSynth || hasSpecificFX || hasOtherDrums;
    if (hasIncompatibleWithKick)
    {
        tags.erase("#Kick");
        tags.erase("#SubKick");
        hasKick = false;
    }

    // 6. Hi-Hat specificity cleanup
    if (tags.find("#OpenHat") != tags.end() && tags.find("#ClosedHat") != tags.end())
    {
        tags.erase("#OpenHat");
        tags.erase("#ClosedHat");
        tags.insert("#HiHat");
    }
    else if (tags.find("#OpenHat") != tags.end() || tags.find("#ClosedHat") != tags.end())
    {
        tags.erase("#HiHat");
    }

    // 7. Drum & Instrument mutual exclusivity
    if (hasKick)
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
        tags.erase("#Synth");
    }
    else if (tags.find("#Snare") != tags.end() || tags.find("#Rimshot") != tags.end() ||
             tags.find("#Clap") != tags.end() || tags.find("#Snap") != tags.end())
    {
        tags.erase("#Percussion");
        tags.erase("#FX");
        tags.erase("#Synth");
        tags.erase("#HiHat");
        tags.erase("#OpenHat");
        tags.erase("#ClosedHat");
        tags.erase("#Tom");
    }
    else if (tags.find("#HiHat") != tags.end() || tags.find("#OpenHat") != tags.end() || tags.find("#ClosedHat") != tags.end())
    {
        tags.erase("#Percussion");
        tags.erase("#FX");
        tags.erase("#Synth");
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

    // 8. Ensure only one #*BPM tag exists and remove BPM tags from pure one-shots
    bool isOneShotSample = (tags.find("#OneShot") != tags.end() || tags.find("#Short") != tags.end() ||
                            tags.find("#Kick") != tags.end() || tags.find("#SubKick") != tags.end() ||
                            tags.find("#Snare") != tags.end() || tags.find("#Clap") != tags.end() ||
                            tags.find("#HiHat") != tags.end() || tags.find("#ClosedHat") != tags.end() ||
                            tags.find("#OpenHat") != tags.end() || tags.find("#Tom") != tags.end() ||
                            tags.find("#Crash") != tags.end() || tags.find("#Ride") != tags.end() ||
                            tags.find("#Rimshot") != tags.end() || tags.find("#Snap") != tags.end() ||
                            tags.find("#Cowbell") != tags.end() || tags.find("#Impact") != tags.end() ||
                            tags.find("#Foley") != tags.end());

    bool isExplicitLoopSample = (tags.find("#Loop") != tags.end() || tags.find("#Arp") != tags.end() || hasTypedLoop);

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

void TagDatabaseManager::sanitizeItem(MediaItem& item)
{
    sanitizeTags(item.tags);

    // Keep item.bpm 100% synchronized with sanitized #*BPM tags
    double tagBpm = 0.0;
    for (const auto& t : item.tags)
    {
        if (t.startsWith("#") && t.endsWithIgnoreCase("BPM") && t.length() > 4)
        {
            tagBpm = t.substring(1, t.length() - 3).getDoubleValue();
            break;
        }
    }

    if (tagBpm <= 0.0)
    {
        // BPM tag was stripped (e.g. one-shot sample) or absent -> clear stale numeric BPM
        item.bpm = 0.0;
    }
    else if (item.bpm <= 0.0)
    {
        item.bpm = tagBpm;
    }
}

static bool hasKickNegation(const juce::String& s)
{
    return s.contains("nokick") || s.contains("no_kick") || s.contains("no-kick") ||
           s.contains("no kick") || s.contains("kickless") || s.contains("without_kick") ||
           s.contains("without kick") || s.contains("kick_free");
}

static bool hasStandaloneBassKeyword(const juce::String& textLower)
{
    // Remove compound non-bass-instrument phrases before testing "bass"
    juce::String cleaned = textLower;
    const char* compounds[] = {
        "bassdrum", "bassdrums", "bass_drum", "bass_drums", "bass-drum", "bass-drums", "bass drum", "bass drums",
        "drumandbass", "drum_and_bass", "drum-and-bass", "drum_n_bass", "drum-n-bass", "drum n bass",
        "futurebass", "future_bass", "future-bass", "future bass",
        "doublebass", "double_bass", "double-bass", "contrabass"
    };
    for (const char* c : compounds)
        cleaned = cleaned.replace(c, "_");

    return isKeywordMatch(cleaned, "bass") || isKeywordMatch(cleaned, "basses");
}

static bool hasStandaloneSubKeyword(const juce::String& textLower)
{
    juce::String cleaned = textLower;
    const char* compounds[] = {
        "subkick", "subkicks", "sub_kick", "sub_kicks", "sub-kick", "sub-kicks", "sub kick", "sub kicks",
        "subdrop", "subdrops", "sub_drop", "sub_drops", "sub-drop", "sub-drops", "sub drop", "sub drops"
    };
    for (const char* c : compounds)
        cleaned = cleaned.replace(c, "_");

    return isKeywordMatch(cleaned, "sub");
}

std::set<juce::String> TagDatabaseManager::inferTagsFromPath(const juce::String& filePath, double durationSeconds, int numChannels)
{
    std::set<juce::String> tags;
    juce::File file(filePath);
    juce::String fileNameNoExt = file.getFileNameWithoutExtension();
    juce::String fileNameLower = fileNameNoExt.toLowerCase();
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
    juce::String keyTag = extractKeyFromFilename(fileNameNoExt);
    if (keyTag.isNotEmpty())
    {
        tags.insert(keyTag);
    }

    // 4. BPM Tag Extraction from Filename (e.g. 128BPM, 120_bpm, 95bpm, 174_DNB, BPM125)
    double extractedBpm = extractBpmFromFilename(fileNameNoExt);
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
        // Drums & Percussion (Compound forms + Singular & Plural)
        { "subkick", "#SubKick", true }, { "subkicks", "#SubKick", true },
        { "sub_kick", "#SubKick", true }, { "sub_kicks", "#SubKick", true },
        { "sub-kick", "#SubKick", true }, { "sub-kicks", "#SubKick", true },
        { "sub kick", "#SubKick", true }, { "sub kicks", "#SubKick", true },
        { "bassdrum", "#Kick", true }, { "bassdrums", "#Kick", true },
        { "bass_drum", "#Kick", true }, { "bass_drums", "#Kick", true },
        { "bass-drum", "#Kick", true }, { "bass-drums", "#Kick", true },
        { "bass drum", "#Kick", true }, { "bass drums", "#Kick", true },
        { "kick", "#Kick", true }, { "kicks", "#Kick", true },
        { "bd", "#Kick", true },

        { "rimshot", "#Rimshot", true }, { "rimshots", "#Rimshot", true },
        { "rim_shot", "#Rimshot", true }, { "rim-shot", "#Rimshot", true },
        { "sidestick", "#Rimshot", true }, { "side_stick", "#Rimshot", true }, { "side-stick", "#Rimshot", true },
        { "clave", "#Rimshot", true }, { "claves", "#Rimshot", true },
        { "woodblock", "#Rimshot", true }, { "wood_block", "#Rimshot", true },
        { "rim", "#Rimshot", true }, { "rims", "#Rimshot", true },

        { "snare", "#Snare", true }, { "snares", "#Snare", true },
        { "sd", "#Snare", true },
        { "clap", "#Clap", true }, { "claps", "#Clap", true },
        { "snap", "#Snap", true }, { "snaps", "#Snap", true },

        { "openhat", "#OpenHat", true }, { "openhats", "#OpenHat", true },
        { "open_hat", "#OpenHat", true }, { "open_hats", "#OpenHat", true },
        { "open-hat", "#OpenHat", true }, { "open-hats", "#OpenHat", true },
        { "openhihat", "#OpenHat", true }, { "openhihats", "#OpenHat", true },
        { "open_hihat", "#OpenHat", true }, { "open_hihats", "#OpenHat", true },
        { "open-hihat", "#OpenHat", true }, { "open-hihats", "#OpenHat", true },
        { "open_hi_hat", "#OpenHat", true }, { "open-hi-hat", "#OpenHat", true },
        { "open_hh", "#OpenHat", true }, { "open-hh", "#OpenHat", true },
        { "ophat", "#OpenHat", true }, { "op_hat", "#OpenHat", true }, { "op-hat", "#OpenHat", true },

        { "closedhat", "#ClosedHat", true }, { "closedhats", "#ClosedHat", true },
        { "closed_hat", "#ClosedHat", true }, { "closed_hats", "#ClosedHat", true },
        { "closed-hat", "#ClosedHat", true }, { "closed-hats", "#ClosedHat", true },
        { "closedhihat", "#ClosedHat", true }, { "closedhihats", "#ClosedHat", true },
        { "closed_hihat", "#ClosedHat", true }, { "closed_hihats", "#ClosedHat", true },
        { "closed-hihat", "#ClosedHat", true }, { "closed-hihats", "#ClosedHat", true },
        { "closed_hi_hat", "#ClosedHat", true }, { "closed-hi-hat", "#ClosedHat", true },
        { "closed_hh", "#ClosedHat", true }, { "closed-hh", "#ClosedHat", true },
        { "clhat", "#ClosedHat", true }, { "cl_hat", "#ClosedHat", true }, { "cl-hat", "#ClosedHat", true },

        { "hihat", "#HiHat", true }, { "hihats", "#HiHat", true },
        { "hi_hat", "#HiHat", true }, { "hi_hats", "#HiHat", true },
        { "hi-hat", "#HiHat", true }, { "hi-hats", "#HiHat", true },
        { "hat", "#HiHat", true }, { "hats", "#HiHat", true },
        { "hh", "#HiHat", true },

        { "tom", "#Tom", true }, { "toms", "#Tom", true },
        { "floortom", "#Tom", true }, { "floortoms", "#Tom", true },
        { "floor_tom", "#Tom", true }, { "rack_tom", "#Tom", true },
        { "crash", "#Crash", true }, { "crashes", "#Crash", true },
        { "ride", "#Ride", true }, { "rides", "#Ride", true },
        { "cymbal", "#Cymbal", true }, { "cymbals", "#Cymbal", true },
        { "shaker", "#Shaker", true }, { "shakers", "#Shaker", true },
        { "tambourine", "#Tambourine", true }, { "tambourines", "#Tambourine", true },
        { "tamb", "#Tambourine", true },
        { "cowbell", "#Cowbell", true }, { "cowbells", "#Cowbell", true }, { "cow_bell", "#Cowbell", true },
        { "conga", "#Conga", true }, { "congas", "#Conga", true },
        { "bongo", "#Bongo", true }, { "bongos", "#Bongo", true },
        { "triangle", "#Percussion", true }, { "cabasa", "#Percussion", true },
        { "agogo", "#Percussion", true }, { "guiro", "#Percussion", true },
        { "djembe", "#Percussion", true }, { "cajon", "#Percussion", true },
        { "tabla", "#Percussion", true }, { "timbale", "#Percussion", true }, { "timbales", "#Percussion", true },
        { "percussion", "#Percussion", true }, { "percussions", "#Percussion", true },
        { "perc", "#Percussion", true }, { "percs", "#Percussion", true },
        { "808", "#808", false }, { "909", "#909", false },

        // Bass (Note: standalone "sub" and "bass" checked via compound-safe guards below)
        { "subbass", "#SubBass", true }, { "sub_bass", "#SubBass", true }, { "sub-bass", "#SubBass", true },
        { "synthbass", "#SynthBass", true }, { "synth_bass", "#SynthBass", true }, { "synth-bass", "#SynthBass", true },
        { "reesebass", "#ReeseBass", true }, { "reese_bass", "#ReeseBass", true }, { "reese", "#ReeseBass", true },
        { "slapbass", "#SlapBass", true }, { "slap_bass", "#SlapBass", true }, { "slap-bass", "#SlapBass", true },
        { "808bass", "#808Bass", true }, { "808_bass", "#808Bass", true }, { "808-bass", "#808Bass", true }, { "808 bass", "#808Bass", true },

        // Melodic & Harmonic Instruments
        { "grandpiano", "#Piano", true }, { "grand_piano", "#Piano", true }, { "piano", "#Piano", true }, { "pianos", "#Piano", true },
        { "rhodes", "#Rhodes", true }, { "epiano", "#Rhodes", true }, { "e_piano", "#Rhodes", true }, { "wurlitzer", "#Rhodes", true },
        { "organ", "#Organ", true }, { "organs", "#Organ", true },
        { "keys", "#Keys", true }, { "keyboard", "#Keys", true }, { "keyboards", "#Keys", true },
        { "acguitar", "#AcousticGuitar", true }, { "acousticguitar", "#AcousticGuitar", true }, { "acoustic_guitar", "#AcousticGuitar", true }, { "nylon", "#AcousticGuitar", true },
        { "elguitar", "#ElectricGuitar", true }, { "electricguitar", "#ElectricGuitar", true }, { "electric_guitar", "#ElectricGuitar", true },
        { "guitar", "#Guitar", true }, { "guitars", "#Guitar", true }, { "gtr", "#Guitar", true },

        // Orchestral & Classical (Unsafe 2-letter "fl", "cl", "cb" removed to prevent FL_Studio / CL_Hat / 808_CB collisions)
        { "orchestra", "#Orchestral", true }, { "orchestral", "#Orchestral", true },
        { "symphonic", "#Orchestral", true }, { "symphony", "#Orchestral", true },
        { "ensemble", "#Ensemble", true }, { "ensembles", "#Ensemble", true }, { "tutti", "#Orchestral", true },

        // Orchestral Strings & Articulations
        { "violin", "#Violin", true }, { "violins", "#Violin", true }, { "vln", "#Violin", true },
        { "viola", "#Viola", true }, { "violas", "#Viola", true }, { "vla", "#Viola", true },
        { "cello", "#Cello", true }, { "cellos", "#Cello", true }, { "vc", "#Cello", true }, { "violoncello", "#Cello", true },
        { "doublebass", "#Contrabass", true }, { "double_bass", "#Contrabass", true },
        { "contrabass", "#Contrabass", true }, { "upright", "#Contrabass", true }, { "upright_bass", "#Contrabass", true },
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
        { "piccolo", "#Piccolo", true }, { "flute", "#Flute", true }, { "flutes", "#Flute", true }, { "flt", "#Flute", true },
        { "oboe", "#Oboe", true }, { "oboes", "#Oboe", true },
        { "englishhorn", "#EnglishHorn", true }, { "english_horn", "#EnglishHorn", true },
        { "clarinet", "#Clarinet", true }, { "clarinets", "#Clarinet", true }, { "clar", "#Clarinet", true },
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
        { "vocal_chop", "#VocalChop", true }, { "vocal_chops", "#VocalChop", true },
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
        { "sub_drop", "#SubDrop", true }, { "sub-drop", "#SubDrop", true },
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
        { "futurebass", "#FutureBass", false }, { "future_bass", "#FutureBass", false },
        { "lofi", "#LoFi", false }, { "lo-fi", "#LoFi", false },
        { "ambient", "#Ambient", false },
        { "cinematic", "#Cinematic", false },
        { "drill", "#Drill", false },
        { "synthwave", "#Synthwave", false },
        { "pop", "#Pop", false },
        { "rock", "#Rock", false },
        { "funk", "#Funk", false },
        { "soul", "#Soul", false },

        // Typed Loops (Count as semantic categories so folder traversal won't overwrite e.g. Drum_Loop_128.wav)
        { "drumloop", "#DrumLoop", true }, { "drumloops", "#DrumLoop", true },
        { "drum_loop", "#DrumLoop", true }, { "drum_loops", "#DrumLoop", true },
        { "drum-loop", "#DrumLoop", true }, { "drum-loops", "#DrumLoop", true },
        { "toploop", "#TopLoop", true }, { "toploops", "#TopLoop", true },
        { "top_loop", "#TopLoop", true }, { "top_loops", "#TopLoop", true },
        { "top-loop", "#TopLoop", true }, { "top-loops", "#TopLoop", true },
        { "melodicloop", "#MelodicLoop", true }, { "melodicloops", "#MelodicLoop", true },
        { "melodic_loop", "#MelodicLoop", true }, { "melodic_loops", "#MelodicLoop", true },
        { "vocalloop", "#VocalLoop", true }, { "vocalloops", "#VocalLoop", true },
        { "vocal_loop", "#VocalLoop", true }, { "vocal_loops", "#VocalLoop", true },
        { "percloop", "#PercLoop", true }, { "percloops", "#PercLoop", true },
        { "perc_loop", "#PercLoop", true }, { "perc_loops", "#PercLoop", true },
        { "bassloop", "#BassLoop", true }, { "bassloops", "#BassLoop", true },
        { "bass_loop", "#BassLoop", true }, { "bass_loops", "#BassLoop", true },

        // Modifiers & Structural Tags
        { "fill", "#Fill", false }, { "fills", "#Fill", false },
        { "stem", "#Stem", false }, { "stems", "#Stem", false },
        { "dry", "#Dry", false }, { "wet", "#Wet", false },
        { "loop", "#Loop", false }, { "loops", "#Loop", false },
        { "groove", "#Loop", false }, { "grooves", "#Loop", false },
        { "break", "#Loop", false }, { "breaks", "#Loop", false },
        { "beat", "#Loop", false }, { "beats", "#Loop", false },
        { "oneshot", "#OneShot", false }, { "oneshots", "#OneShot", false },
        { "one_shot", "#OneShot", false }, { "one_shots", "#OneShot", false },
        { "one-shot", "#OneShot", false }, { "one-shots", "#OneShot", false },
        { "hit", "#OneShot", false }, { "hits", "#OneShot", false },
        { "stab", "#OneShot", false }, { "stabs", "#OneShot", false },
        { "acoustic", "#Acoustic", false }, { "digital", "#Digital", false }
    };

    bool hasFilenameCategory = false;
    const bool kickNegatedInName = hasKickNegation(fileNameLower);

    // Check filename first for high-priority matching
    for (const auto& r : rules)
    {
        if (kickNegatedInName && (std::strcmp(r.tag, "#Kick") == 0 || std::strcmp(r.tag, "#SubKick") == 0))
            continue;

        // For short abbreviation "bd", allow tuned kicks (e.g. BD_Fm.wav) while skipping melodic contexts
        if (std::strcmp(r.keyword, "bd") == 0)
        {
            if (fileNameLower.contains("chord") || fileNameLower.contains("pad") ||
                fileNameLower.contains("lead") || fileNameLower.contains("piano") ||
                fileNameLower.contains("guitar") || fileNameLower.contains("vocal") ||
                hasStandaloneBassKeyword(fileNameLower) || fileNameLower.contains("breakdown"))
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

    // Compound-safe checks for "bass", "sub", and drum-machine abbreviations ("cb", "oh", "ch")
    const bool fnHasKick = (tags.find("#Kick") != tags.end() || tags.find("#SubKick") != tags.end());
    if (!fnHasKick && hasStandaloneBassKeyword(fileNameLower))
    {
        tags.insert("#Bass");
        hasFilenameCategory = true;
    }
    if (!fnHasKick && tags.find("#SubDrop") == tags.end() && hasStandaloneSubKeyword(fileNameLower))
    {
        tags.insert("#SubBass");
        hasFilenameCategory = true;
    }

    // Drum-pack shorthand "CB" (808_CB / 909_CB), "OH" (Open Hat), "CH" (Closed Hat) with context guards
    bool hasDrumMachineCtx = fileNameLower.contains("808") || fileNameLower.contains("909") ||
                             fileNameLower.contains("707") || fileNameLower.contains("606") ||
                             fileNameLower.contains("drum") || fileNameLower.contains("kit");
    if (hasDrumMachineCtx && isKeywordMatch(fileNameLower, "cb"))
    {
        tags.insert("#Cowbell");
        hasFilenameCategory = true;
    }
    if ((hasDrumMachineCtx || fileNameLower.contains("hat") || fileNameLower.contains("hh")) &&
        !fileNameLower.contains("vocal") && !fileNameLower.contains("vox") && !fileNameLower.contains("choir"))
    {
        if (isKeywordMatch(fileNameLower, "oh"))
        {
            tags.insert("#OpenHat");
            hasFilenameCategory = true;
        }
        else if (isKeywordMatch(fileNameLower, "ch"))
        {
            tags.insert("#ClosedHat");
            hasFilenameCategory = true;
        }
    }

    // Inspect directory hierarchy level-by-level starting from the immediate parent
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
                if (isCompoundFolder)
                    continue;

                if (kickNegatedInName && (std::strcmp(r.tag, "#Kick") == 0 || std::strcmp(r.tag, "#SubKick") == 0))
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

        if (!hasFilenameCategory && !isCompoundFolder)
        {
            bool folderHasKick = (tags.find("#Kick") != tags.end() || tags.find("#SubKick") != tags.end());
            if (!folderHasKick && hasStandaloneBassKeyword(dirNameLower))
            {
                tags.insert("#Bass");
                folderMatchedCategory = true;
            }
            else if (!folderHasKick && tags.find("#SubDrop") == tags.end() && hasStandaloneSubKeyword(dirNameLower))
            {
                tags.insert("#SubBass");
                folderMatchedCategory = true;
            }
            else if (dirNameLower == "oh" || dirNameLower == "open hats" || dirNameLower == "open hihats")
            {
                tags.insert("#OpenHat");
                folderMatchedCategory = true;
            }
            else if (dirNameLower == "ch" || dirNameLower == "closed hats" || dirNameLower == "closed hihats")
            {
                tags.insert("#ClosedHat");
                folderMatchedCategory = true;
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
                    sanitizeItem(item);
                    item.precomputeCachedStrings();
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
    juce::File dbFile;

    {
        const juce::ScopedLock sl(lock);
        itemsArray.ensureStorageAllocated(static_cast<int>(itemsMap.size()));
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
        dbFile = getDatabaseFile();
    }

    rootObj->setProperty("items", itemsArray);
    rootObj->setProperty("scanFolders", foldersArray);

    juce::Thread::launch([rootVar = juce::var(rootObj), dbFile]() {
        auto jsonString = juce::JSON::toString(rootVar, false);
        auto tempFile = dbFile.getSiblingFile(dbFile.getFileName() + ".tmp");
        if (tempFile.replaceWithText(jsonString))
        {
            tempFile.moveFileTo(dbFile);
        }
        else
        {
            dbFile.replaceWithText(jsonString);
        }
    });
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
    notifyIndexUpdated();
    saveToFile();
}

void TagDatabaseManager::removeScanFolder(const juce::String& folderPath)
{
    {
        const juce::ScopedLock sl(lock);
        scanFolders.erase(folderPath);

        juce::String normFolder = folderPath.trim().replaceCharacter('\\', '/');
        while (normFolder.length() > 1 && normFolder.endsWithChar('/'))
        {
            if (normFolder.length() == 3 && normFolder[1] == ':')
                break;
            normFolder = normFolder.dropLastCharacters(1);
        }

        for (auto it = itemsMap.begin(); it != itemsMap.end(); )
        {
            juce::String normItem = it->second.filePath.replaceCharacter('\\', '/');
            bool inFolder = normItem.equalsIgnoreCase(normFolder) ||
                            (normItem.length() > normFolder.length() &&
                             normItem.startsWithIgnoreCase(normFolder) &&
                             (normFolder.endsWithChar('/') || normItem[normFolder.length()] == '/'));
            if (inFolder)
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
    // Normalized sample-rate-invariant feature differences
    double d_zcr = (a.zcr - b.zcr) * 2.4;

    // Log-compressed highFreqRatio (spans [0, 4.0], keeps kicks crisp while preventing noise-hat outlier blowup)
    double hfrA = std::log1p(juce::jlimit(0.0, 4.0, a.highFreqRatio));
    double hfrB = std::log1p(juce::jlimit(0.0, 4.0, b.highFreqRatio));
    double d_hfr = (hfrA - hfrB) * 1.9;

    double drA = juce::jlimit(0.0, 1.25, a.decayRatio);
    double drB = juce::jlimit(0.0, 1.25, b.decayRatio);
    double d_dr = (drA - drB) * 1.75;

    double cf_a = juce::jlimit(1.0, 14.0, a.crestFactor);
    double cf_b = juce::jlimit(1.0, 14.0, b.crestFactor);
    double d_cf = (cf_a - cf_b) * 0.16;

    // Logarithmic duration ratio difference
    double logDurA = std::log10(std::max(0.02, a.durationSeconds) + 0.05);
    double logDurB = std::log10(std::max(0.02, b.durationSeconds) + 0.05);
    double d_dur = (logDurA - logDurB) * 1.75;

    // Mild channel mismatch penalty (mono vs dual-mono drum samples remain close)
    double d_ch = (a.numChannels > 0 && b.numChannels > 0 && a.numChannels != b.numChannels) ? 0.10 : 0.0;

    double baseDist = std::sqrt(d_zcr * d_zcr + d_hfr * d_hfr + d_dr * d_dr + d_cf * d_cf + d_dur * d_dur + d_ch * d_ch);

    // Semantic Category & Acoustic Character Overlap Bonus (excludes #Wav, #Stereo, #OneShot format tags!)
    double tagDiscount = 0.0;
    juce::String catA = getCanonicalCategoryForItem(a);
    juce::String catB = getCanonicalCategoryForItem(b);
    if (catA != "other" && areCategoriesCompatible(catA, catB))
    {
        tagDiscount += (catA == catB) ? 0.20 : 0.12;
    }

    int sharedSemanticTags = 0;
    for (const auto& t : a.tags)
    {
        if (b.tags.find(t) == b.tags.end())
            continue;

        // Ignore pure container/channel/length tags that carry zero timbral similarity
        if (t == "#Wav" || t == "#MP3" || t == "#FLAC" || t == "#OGG" || t == "#AIFF" ||
            t == "#Stereo" || t == "#Mono" || t == "#Short" || t == "#Long" ||
            t == "#OneShot" || t == "#Loop" || t == "#Converted" ||
            t.endsWithIgnoreCase("BPM") || t.startsWithIgnoreCase("#Key_"))
        {
            continue;
        }
        sharedSemanticTags++;
    }

    tagDiscount = std::min(0.38, tagDiscount + sharedSemanticTags * 0.07);
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
