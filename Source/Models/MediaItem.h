#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_core/juce_core.h>
#endif

#include <cstdint>
#include <set>

namespace openwav
{

struct MediaItem
{
    juce::String id;                 // Unique identifier (hash of canonical path)
    juce::String filePath;           // Absolute system file path
    juce::String fileName;           // Base filename (e.g. Kick_01.wav)
    juce::String fileExtension;      // Extension lowercase (e.g. .wav, .mp3)
    int64_t fileSizeBytes { 0 };     // Size in bytes
    double durationSeconds { 0.0 };  // Duration in seconds
    double sampleRate { 0.0 };       // Sample rate in Hz (e.g. 44100.0)
    int numChannels { 0 };           // Stereo/Mono channel count
    int bitDepth { 0 };              // Bit resolution (16, 24, 32, etc.)
    double bpm { 0.0 };              // Estimated BPM if detected or set
    bool isFavorite { false };       // Favorite status flag
    int rating { 0 };                // Rating star count (0-5)
    std::set<juce::String> tags;     // Active tags (e.g. "Kick", "120BPM", "Loop", "Wav")
    int64_t dateAddedMs { 0 };       // Timestamp when added to index

    // DSP similarity features
    double zcr { 0.0 };
    double highFreqRatio { 0.0 };
    double decayRatio { 0.0 };
    double crestFactor { 0.0 };

    juce::var toVar() const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("id", id);
        obj->setProperty("filePath", filePath);
        obj->setProperty("fileName", fileName);
        obj->setProperty("fileExtension", fileExtension);
        obj->setProperty("fileSizeBytes", static_cast<juce::int64>(fileSizeBytes));
        obj->setProperty("durationSeconds", durationSeconds);
        obj->setProperty("sampleRate", sampleRate);
        obj->setProperty("numChannels", numChannels);
        obj->setProperty("bitDepth", bitDepth);
        obj->setProperty("bpm", bpm);
        obj->setProperty("isFavorite", isFavorite);
        obj->setProperty("rating", rating);
        obj->setProperty("dateAddedMs", static_cast<juce::int64>(dateAddedMs));

        obj->setProperty("zcr", zcr);
        obj->setProperty("highFreqRatio", highFreqRatio);
        obj->setProperty("decayRatio", decayRatio);
        obj->setProperty("crestFactor", crestFactor);

        juce::Array<juce::var> tagArray;
        for (const auto& tag : tags)
            tagArray.add(tag);
        obj->setProperty("tags", tagArray);

        return juce::var(obj);
    }

    static MediaItem fromVar(const juce::var& v)
    {
        MediaItem item;
        if (!v.isObject()) return item;

        auto* obj = v.getDynamicObject();
        if (!obj) return item;

        item.id = obj->getProperty("id").toString();
        item.filePath = obj->getProperty("filePath").toString();
        item.fileName = obj->getProperty("fileName").toString();
        item.fileExtension = obj->getProperty("fileExtension").toString();
        item.fileSizeBytes = static_cast<juce::int64>(obj->getProperty("fileSizeBytes"));
        item.durationSeconds = static_cast<double>(obj->getProperty("durationSeconds"));
        item.sampleRate = static_cast<double>(obj->getProperty("sampleRate"));
        item.numChannels = static_cast<int>(obj->getProperty("numChannels"));
        item.bitDepth = static_cast<int>(obj->getProperty("bitDepth"));
        item.bpm = static_cast<double>(obj->getProperty("bpm"));
        item.isFavorite = static_cast<bool>(obj->getProperty("isFavorite"));
        item.rating = static_cast<int>(obj->getProperty("rating"));
        item.dateAddedMs = static_cast<juce::int64>(obj->getProperty("dateAddedMs"));

        item.zcr = static_cast<double>(obj->getProperty("zcr"));
        item.highFreqRatio = static_cast<double>(obj->getProperty("highFreqRatio"));
        item.decayRatio = static_cast<double>(obj->getProperty("decayRatio"));
        item.crestFactor = static_cast<double>(obj->getProperty("crestFactor"));

        if (obj->hasProperty("tags"))
        {
            auto tagVar = obj->getProperty("tags");
            if (tagVar.isArray())
            {
                for (const auto& t : *tagVar.getArray())
                    item.tags.insert(t.toString());
            }
        }

        return item;
    }
};

} // namespace openwav
