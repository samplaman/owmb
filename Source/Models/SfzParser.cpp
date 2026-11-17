#include "SfzParser.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <set>
#include <vector>

namespace openwav
{

namespace
{

using OpcodeMap = std::map<juce::String, juce::String>;

juce::String stripComments(const juce::String& raw)
{
    juce::String out;
    out.preallocateBytes(static_cast<size_t>(raw.getNumBytesAsUTF8()) + 1);

    const int len = raw.length();
    bool inBlockComment = false;
    bool inLineComment = false;
    bool inQuotes = false;

    for (int i = 0; i < len; ++i)
    {
        juce::juce_wchar c = raw[i];
        juce::juce_wchar next = (i + 1 < len) ? raw[i + 1] : 0;

        if (inBlockComment)
        {
            if (c == '*' && next == '/')
            {
                inBlockComment = false;
                ++i;
            }
            else if (c == '\n')
            {
                out << '\n';
            }
            continue;
        }

        if (inLineComment)
        {
            if (c == '\n' || c == '\r')
            {
                inLineComment = false;
                out << c;
            }
            continue;
        }

        if (c == '"')
        {
            inQuotes = !inQuotes;
            out << c;
            continue;
        }

        if (!inQuotes)
        {
            if (c == '/' && next == '*')
            {
                inBlockComment = true;
                ++i;
                continue;
            }
            if (c == '/' && next == '/')
            {
                inLineComment = true;
                ++i;
                continue;
            }
        }

        out << c;
    }

    return out;
}

juce::String normalizePathSlashes(juce::String p)
{
    p = p.trim();
    if (p.length() >= 2 && p.startsWithChar('"') && p.endsWithChar('"'))
        p = p.substring(1, p.length() - 1);
    return p.replace("\\", "/");
}

juce::String preprocessSfzRecursive(const juce::File& file,
                                    std::map<juce::String, juce::String>& defines,
                                    std::set<juce::String>& includeStack,
                                    int depth)
{
    if (depth > 16 || !file.existsAsFile())
        return {};

    auto canonical = file.getFullPathName();
    if (includeStack.count(canonical) > 0)
        return {};

    includeStack.insert(canonical);
    juce::String raw = stripComments(file.loadFileAsString());
    juce::StringArray lines = juce::StringArray::fromLines(raw);
    juce::String result;
    auto baseDir = file.getParentDirectory();

    for (const auto& line : lines)
    {
        auto trimmed = line.trim();
        if (trimmed.startsWith("#define"))
        {
            auto rest = trimmed.substring(7).trimStart();
            int spaceIdx = rest.indexOfAnyOf(" \t");
            if (spaceIdx > 0)
            {
                auto varName = rest.substring(0, spaceIdx).trim();
                auto varVal = rest.substring(spaceIdx + 1).trim();
                if (varName.isNotEmpty())
                    defines[varName] = varVal;
            }
            continue;
        }

        if (trimmed.startsWith("#include"))
        {
            auto incPath = normalizePathSlashes(trimmed.substring(8));
            for (const auto& kv : defines)
                incPath = incPath.replace(kv.first, kv.second);

            auto incFile = baseDir.getChildFile(incPath);
            if (incFile.existsAsFile())
                result << preprocessSfzRecursive(incFile, defines, includeStack, depth + 1) << "\n";
            continue;
        }

        juce::String expanded = line;
        if (!defines.empty() && expanded.containsChar('$'))
        {
            // Expand longest variable names first to avoid prefix collisions
            std::vector<std::pair<juce::String, juce::String>> sorted(defines.begin(), defines.end());
            std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
                return a.first.length() > b.first.length();
            });
            for (const auto& kv : sorted)
                expanded = expanded.replace(kv.first, kv.second);
        }

        result << expanded << "\n";
    }

    includeStack.erase(canonical);
    return result;
}

bool isOpcodeIdentChar(juce::juce_wchar c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}

struct SfzToken
{
    bool isHeader { false };
    juce::String key;
    juce::String value;
};

std::vector<SfzToken> tokenizeSfz(const juce::String& text)
{
    std::vector<SfzToken> tokens;
    const int len = text.length();
    int i = 0;

    auto skipWs = [&]() {
        while (i < len && juce::CharacterFunctions::isWhitespace(text[i]))
            ++i;
    };

    while (i < len)
    {
        skipWs();
        if (i >= len) break;

        if (text[i] == '<')
        {
            int closeIdx = text.indexOfChar(i + 1, '>');
            if (closeIdx > i)
            {
                SfzToken tok;
                tok.isHeader = true;
                tok.key = text.substring(i + 1, closeIdx).trim().toLowerCase();
                tokens.push_back(std::move(tok));
                i = closeIdx + 1;
                continue;
            }
        }

        // Scan for opcode=value
        int eqIdx = -1;
        int scan = i;
        while (scan < len && isOpcodeIdentChar(text[scan]))
            ++scan;

        if (scan > i && scan < len && text[scan] == '=')
            eqIdx = scan;

        if (eqIdx < 0)
        {
            ++i;
            continue;
        }

        juce::String opcode = text.substring(i, eqIdx).trim().toLowerCase();
        int valStart = eqIdx + 1;
        int valEnd = valStart;
        bool inQuotes = false;

        while (valEnd < len)
        {
            juce::juce_wchar c = text[valEnd];
            if (c == '"')
            {
                inQuotes = !inQuotes;
                ++valEnd;
                continue;
            }

            if (!inQuotes)
            {
                if (c == '<')
                {
                    int closeIdx = text.indexOfChar(valEnd + 1, '>');
                    if (closeIdx > valEnd)
                        break;
                }

                if (juce::CharacterFunctions::isWhitespace(c))
                {
                    // Look ahead past whitespace to see if another opcode= follows
                    int ahead = valEnd;
                    while (ahead < len && juce::CharacterFunctions::isWhitespace(text[ahead]))
                        ++ahead;

                    if (ahead >= len || text[ahead] == '<')
                        break;

                    int identEnd = ahead;
                    while (identEnd < len && isOpcodeIdentChar(text[identEnd]))
                        ++identEnd;

                    if (identEnd > ahead && identEnd < len && text[identEnd] == '=')
                        break;
                }
            }
            ++valEnd;
        }

        juce::String val = text.substring(valStart, valEnd).trim();
        if (val.length() >= 2 && val.startsWithChar('"') && val.endsWithChar('"'))
            val = val.substring(1, val.length() - 1);

        SfzToken tok;
        tok.isHeader = false;
        tok.key = std::move(opcode);
        tok.value = std::move(val);
        tokens.push_back(std::move(tok));

        i = valEnd;
    }

    return tokens;
}

juce::File resolveSampleFile(const juce::File& sfzDir, const juce::String& defaultPath, const juce::String& sampleValue)
{
    auto cleanSample = normalizePathSlashes(sampleValue);
    if (cleanSample.isEmpty() || cleanSample.startsWithChar('*'))
        return {};

    auto cleanDefault = normalizePathSlashes(defaultPath);
    if (cleanDefault.isNotEmpty() && !cleanDefault.endsWithChar('/'))
        cleanDefault += "/";

    // 1. default_path + sample relative to SFZ directory
    if (cleanDefault.isNotEmpty())
    {
        auto candidate = sfzDir.getChildFile(cleanDefault + cleanSample);
        if (candidate.existsAsFile())
            return candidate;
    }

    // 2. sample directly relative to SFZ directory
    {
        auto candidate = sfzDir.getChildFile(cleanSample);
        if (candidate.existsAsFile())
            return candidate;
    }

    // 3. Absolute path fallback
    if (juce::File::isAbsolutePath(cleanSample))
    {
        juce::File abs(cleanSample);
        if (abs.existsAsFile())
            return abs;
    }

    // Return best-guess path even if missing on disk so zone path is preserved
    return cleanDefault.isNotEmpty() ? sfzDir.getChildFile(cleanDefault + cleanSample)
                                     : sfzDir.getChildFile(cleanSample);
}

void applyOpcodesToZone(const OpcodeMap& opcodes,
                        const juce::File& sfzDir,
                        const juce::String& defaultPath,
                        int noteOffset,
                        SampleMapZoneState& zone,
                        bool& hasPitchTrackZero,
                        bool& hasSeqRoundRobin,
                        bool& hasRandRoundRobin)
{
    auto getStr = [&](const char* k) -> juce::String {
        auto it = opcodes.find(k);
        return (it != opcodes.end()) ? it->second : juce::String();
    };
    auto hasKey = [&](const char* k) -> bool {
        return opcodes.find(k) != opcodes.end();
    };

    juce::File sampleFile = resolveSampleFile(sfzDir, defaultPath, getStr("sample"));
    zone.filePath = sampleFile.getFullPathName();
    zone.sampleName = sampleFile.getFileName();

    int keyLow = 0;
    int keyHigh = 127;
    int rootNote = 60;
    bool rootExplicit = false;

    if (hasKey("key"))
    {
        int k = SfzParser::parseMidiNote(getStr("key"), noteOffset);
        keyLow = k;
        keyHigh = k;
        rootNote = k;
        rootExplicit = true;
    }

    if (hasKey("lokey"))
        keyLow = SfzParser::parseMidiNote(getStr("lokey"), noteOffset);
    if (hasKey("hikey"))
        keyHigh = SfzParser::parseMidiNote(getStr("hikey"), noteOffset);

    if (keyHigh < keyLow)
        std::swap(keyLow, keyHigh);

    if (hasKey("pitch_keycenter"))
    {
        rootNote = SfzParser::parseMidiNote(getStr("pitch_keycenter"), noteOffset);
        rootExplicit = true;
    }
    else if (!rootExplicit)
    {
        rootNote = (keyLow == keyHigh) ? keyLow : juce::jlimit(0, 127, (keyLow + keyHigh) / 2);
    }

    zone.keyLow = juce::jlimit(0, 127, keyLow);
    zone.keyHigh = juce::jlimit(zone.keyLow, 127, keyHigh);
    zone.rootNote = juce::jlimit(0, 127, rootNote);

    int velLow = hasKey("lovel") ? getStr("lovel").getIntValue() : 0;
    int velHigh = hasKey("hivel") ? getStr("hivel").getIntValue() : 127;
    if (velHigh < velLow)
        std::swap(velLow, velHigh);
    zone.velLow = juce::jlimit(0, 127, velLow);
    zone.velHigh = juce::jlimit(zone.velLow, 127, velHigh);

    if (hasKey("amp_veltrack"))
    {
        float velTrack = getStr("amp_veltrack").getFloatValue();
        zone.velocitySensitivity = juce::jlimit(0.0f, 1.0f, std::abs(velTrack) / 100.0f);
    }

    float cents = 0.0f;
    if (hasKey("tune"))
        cents += getStr("tune").getFloatValue();
    if (hasKey("transpose"))
        cents += getStr("transpose").getFloatValue() * 100.0f;
    zone.fineTuneCents = juce::jlimit(-100.0f, 100.0f, cents);

    if (hasKey("volume"))
        zone.gainDb = juce::jlimit(-24.0f, 12.0f, getStr("volume").getFloatValue());

    if (hasKey("ampeg_attack"))
        zone.attackMs = juce::jlimit(0.0f, 2000.0f, getStr("ampeg_attack").getFloatValue() * 1000.0f);
    if (hasKey("ampeg_decay"))
        zone.decayMs = juce::jlimit(0.0f, 2000.0f, getStr("ampeg_decay").getFloatValue() * 1000.0f);
    if (hasKey("ampeg_sustain"))
        zone.sustainLevel = juce::jlimit(0.0f, 1.0f, getStr("ampeg_sustain").getFloatValue() / 100.0f);
    if (hasKey("ampeg_release"))
        zone.releaseMs = juce::jlimit(0.0f, 5000.0f, getStr("ampeg_release").getFloatValue() * 1000.0f);

    if (hasKey("seq_position"))
    {
        zone.roundRobinIndex = juce::jlimit(1, 32, getStr("seq_position").getIntValue());
        hasSeqRoundRobin = true;
    }
    else if (hasKey("lorand") || hasKey("hirand"))
    {
        hasRandRoundRobin = true;
    }

    if (hasKey("pitch_keytrack") && getStr("pitch_keytrack").getIntValue() == 0)
        hasPitchTrackZero = true;

    if (hasKey("trigger") && getStr("trigger").trim().equalsIgnoreCase("release"))
        zone.isReleaseTrigger = true;
}

} // namespace

int SfzParser::parseMidiNote(const juce::String& text, int noteOffset)
{
    auto t = text.trim().toLowerCase();
    if (t.isEmpty())
        return 60;

    // Pure integer MIDI note number
    bool isNumeric = true;
    for (int i = 0; i < t.length(); ++i)
    {
        juce::juce_wchar c = t[i];
        if (i == 0 && (c == '-' || c == '+')) continue;
        if (c < '0' || c > '9') { isNumeric = false; break; }
    }
    if (isNumeric)
        return juce::jlimit(0, 127, t.getIntValue() + noteOffset);

    // Note name: c4, f#3, bb2, c-1
    static const int baseOffsets[7] = { 9, 11, 0, 2, 4, 5, 7 }; // A B C D E F G
    juce::juce_wchar letter = t[0];
    if (letter < 'a' || letter > 'g')
        return 60;

    int semitone = baseOffsets[letter - 'a'];
    int pos = 1;
    if (pos < t.length())
    {
        if (t[pos] == '#') { semitone += 1; ++pos; }
        else if (t[pos] == 'b') { semitone -= 1; ++pos; }
    }

    int octave = t.substring(pos).getIntValue();
    // Standard SFZ / MIDI: C4 = 60 -> (octave + 1) * 12 + semitone
    int midi = (octave + 1) * 12 + semitone + noteOffset;
    return juce::jlimit(0, 127, midi);
}

bool SfzParser::parseFile(const juce::File& sfzFile, SampleMapState& outState, juce::String& outError)
{
    outError.clear();
    if (!sfzFile.existsAsFile())
    {
        outError = "SFZ file does not exist.";
        return false;
    }

    std::map<juce::String, juce::String> defines;
    std::set<juce::String> includeStack;
    juce::String expanded = preprocessSfzRecursive(sfzFile, defines, includeStack, 0);
    auto tokens = tokenizeSfz(expanded);

    if (tokens.empty())
    {
        outError = "SFZ file is empty or contains no valid headers/opcodes.";
        return false;
    }

    enum class Section
    {
        None,
        Control,
        Global,
        Master,
        Group,
        Region,
        Other
    };

    Section currentSection = Section::None;
    OpcodeMap globalOps;
    OpcodeMap masterOps;
    OpcodeMap groupOps;
    OpcodeMap regionOps;
    juce::String defaultPath;
    int noteOffset = 0;

    SampleMapState parsed;
    parsed.instrument.instrumentName = sfzFile.getFileNameWithoutExtension();

    bool anyPitchTrackZero = false;
    bool allPitchTrackZero = true;
    bool hasSeqRR = false;
    bool hasRandRR = false;

    struct RandRegionInfo
    {
        size_t zoneIndex;
        float lorand;
        float hirand;
    };
    std::vector<RandRegionInfo> randRegions;

    auto flushRegion = [&]() {
        if (currentSection != Section::Region)
            return;

        OpcodeMap merged = globalOps;
        for (const auto& kv : masterOps) merged[kv.first] = kv.second;
        for (const auto& kv : groupOps) merged[kv.first] = kv.second;
        for (const auto& kv : regionOps) merged[kv.first] = kv.second;

        auto sampleIt = merged.find("sample");
        if (sampleIt == merged.end() || sampleIt->second.trim().isEmpty() || sampleIt->second.trim().startsWithChar('*'))
            return;

        SampleMapZoneState zone;
        bool regionPitchZero = false;
        applyOpcodesToZone(merged, sfzFile.getParentDirectory(), defaultPath, noteOffset,
                           zone, regionPitchZero, hasSeqRR, hasRandRR);

        if (regionPitchZero)
            anyPitchTrackZero = true;
        else
            allPitchTrackZero = false;

        if (merged.find("seq_position") == merged.end() &&
            (merged.find("lorand") != merged.end() || merged.find("hirand") != merged.end()))
        {
            float loR = (merged.find("lorand") != merged.end()) ? merged["lorand"].getFloatValue() : 0.0f;
            float hiR = (merged.find("hirand") != merged.end()) ? merged["hirand"].getFloatValue() : 1.0f;
            randRegions.push_back({ parsed.zones.size(), loR, hiR });
        }

        parsed.zones.push_back(std::move(zone));
    };

    for (const auto& tok : tokens)
    {
        if (tok.isHeader)
        {
            if (currentSection == Section::Region)
                flushRegion();

            regionOps.clear();

            if (tok.key == "control")
            {
                currentSection = Section::Control;
            }
            else if (tok.key == "global")
            {
                currentSection = Section::Global;
                globalOps.clear();
                masterOps.clear();
                groupOps.clear();
            }
            else if (tok.key == "master")
            {
                currentSection = Section::Master;
                masterOps.clear();
                groupOps.clear();
            }
            else if (tok.key == "group")
            {
                currentSection = Section::Group;
                groupOps.clear();
            }
            else if (tok.key == "region")
            {
                currentSection = Section::Region;
            }
            else
            {
                currentSection = Section::Other;
            }
            continue;
        }

        switch (currentSection)
        {
            case Section::Control:
                if (tok.key == "default_path")
                    defaultPath = tok.value;
                else if (tok.key == "note_offset")
                    noteOffset += tok.value.getIntValue();
                else if (tok.key == "octave_offset")
                    noteOffset += tok.value.getIntValue() * 12;
                break;
            case Section::Global:
                globalOps[tok.key] = tok.value;
                break;
            case Section::Master:
                masterOps[tok.key] = tok.value;
                break;
            case Section::Group:
                groupOps[tok.key] = tok.value;
                break;
            case Section::Region:
                regionOps[tok.key] = tok.value;
                break;
            case Section::None:
            case Section::Other:
                break;
        }
    }

    if (currentSection == Section::Region)
        flushRegion();

    if (parsed.zones.empty())
    {
        outError = "No valid <region> entries with sample paths were found in the SFZ file.";
        return false;
    }

    // Assign sequential roundRobinIndex values for lorand/hirand groups sharing key+velocity bounds
    if (!randRegions.empty())
    {
        std::map<std::tuple<int, int, int, int>, std::vector<RandRegionInfo>> buckets;
        for (const auto& rr : randRegions)
        {
            const auto& z = parsed.zones[rr.zoneIndex];
            buckets[{ z.keyLow, z.keyHigh, z.velLow, z.velHigh }].push_back(rr);
        }
        for (auto& kv : buckets)
        {
            auto& vec = kv.second;
            std::sort(vec.begin(), vec.end(), [](const RandRegionInfo& a, const RandRegionInfo& b) {
                if (std::abs(a.lorand - b.lorand) > 1e-5f)
                    return a.lorand < b.lorand;
                return a.hirand < b.hirand;
            });
            for (size_t idx = 0; idx < vec.size(); ++idx)
                parsed.zones[vec[idx].zoneIndex].roundRobinIndex = juce::jlimit(1, 32, static_cast<int>(idx) + 1);
        }
    }

    if (anyPitchTrackZero && allPitchTrackZero)
        parsed.pitchTrackingEnabled = false;

    if (hasSeqRR)
        parsed.roundRobinMode = 0; // Cycle
    else if (hasRandRR)
        parsed.roundRobinMode = 1; // Random

    outState = std::move(parsed);
    return true;
}

} // namespace openwav
