#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_core/juce_core.h>
 #include <juce_graphics/juce_graphics.h>
#endif
#include "MediaItem.h"
#include <array>
#include <vector>
#include <algorithm>
#include <cmath>
#include <functional>

namespace openwav
{

struct EditComponentState
{
    juce::String filePath;
    double sampleStartRatio { 0.0 };
    double sampleEndRatio { 1.0 };
    double loopInRatio { 0.0 };
    double loopOutRatio { 1.0 };
    bool loopMarkersSet { false };
    double fadeInMs { 0.0 };
    double fadeOutMs { 0.0 };
    int fadeInCurveType { 0 };
    int fadeOutCurveType { 0 };
    double crossfadeMs { 0.0 };
    double zoomLevel { 1.0 };
    double scrollOffset { 0.0 };
    bool snapToZeroCrossing { false };
    bool isSpectralView { false };
    bool hasSpectralBoxSelection { false };
    double spectralTimeStart { 0.0 };
    double spectralTimeEnd { 1.0 };
    float spectralFreqLow { 20.0f };
    float spectralFreqHigh { 20000.0f };

    juce::var toVar() const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("filePath", filePath);
        obj->setProperty("sampleStartRatio", sampleStartRatio);
        obj->setProperty("sampleEndRatio", sampleEndRatio);
        obj->setProperty("loopInRatio", loopInRatio);
        obj->setProperty("loopOutRatio", loopOutRatio);
        obj->setProperty("loopMarkersSet", loopMarkersSet);
        obj->setProperty("fadeInMs", fadeInMs);
        obj->setProperty("fadeOutMs", fadeOutMs);
        obj->setProperty("fadeInCurveType", fadeInCurveType);
        obj->setProperty("fadeOutCurveType", fadeOutCurveType);
        obj->setProperty("crossfadeMs", crossfadeMs);
        obj->setProperty("zoomLevel", zoomLevel);
        obj->setProperty("scrollOffset", scrollOffset);
        obj->setProperty("snapToZeroCrossing", snapToZeroCrossing);
        obj->setProperty("isSpectralView", isSpectralView);
        obj->setProperty("hasSpectralBoxSelection", hasSpectralBoxSelection);
        obj->setProperty("spectralTimeStart", spectralTimeStart);
        obj->setProperty("spectralTimeEnd", spectralTimeEnd);
        obj->setProperty("spectralFreqLow", spectralFreqLow);
        obj->setProperty("spectralFreqHigh", spectralFreqHigh);
        return juce::var(obj);
    }

    static EditComponentState fromVar(const juce::var& v)
    {
        EditComponentState s;
        if (!v.isObject()) return s;
        auto* obj = v.getDynamicObject();
        if (!obj) return s;

        s.filePath = obj->getProperty("filePath").toString();
        if (obj->hasProperty("sampleStartRatio")) s.sampleStartRatio = static_cast<double>(obj->getProperty("sampleStartRatio"));
        if (obj->hasProperty("sampleEndRatio")) s.sampleEndRatio = static_cast<double>(obj->getProperty("sampleEndRatio"));
        if (obj->hasProperty("loopInRatio")) s.loopInRatio = static_cast<double>(obj->getProperty("loopInRatio"));
        if (obj->hasProperty("loopOutRatio")) s.loopOutRatio = static_cast<double>(obj->getProperty("loopOutRatio"));
        if (obj->hasProperty("loopMarkersSet")) s.loopMarkersSet = static_cast<bool>(obj->getProperty("loopMarkersSet"));
        if (obj->hasProperty("fadeInMs")) s.fadeInMs = static_cast<double>(obj->getProperty("fadeInMs"));
        if (obj->hasProperty("fadeOutMs")) s.fadeOutMs = static_cast<double>(obj->getProperty("fadeOutMs"));
        if (obj->hasProperty("fadeInCurveType")) s.fadeInCurveType = static_cast<int>(obj->getProperty("fadeInCurveType"));
        if (obj->hasProperty("fadeOutCurveType")) s.fadeOutCurveType = static_cast<int>(obj->getProperty("fadeOutCurveType"));
        if (obj->hasProperty("crossfadeMs")) s.crossfadeMs = static_cast<double>(obj->getProperty("crossfadeMs"));
        if (obj->hasProperty("zoomLevel")) s.zoomLevel = static_cast<double>(obj->getProperty("zoomLevel"));
        if (obj->hasProperty("scrollOffset")) s.scrollOffset = static_cast<double>(obj->getProperty("scrollOffset"));
        if (obj->hasProperty("snapToZeroCrossing")) s.snapToZeroCrossing = static_cast<bool>(obj->getProperty("snapToZeroCrossing"));
        if (obj->hasProperty("isSpectralView")) s.isSpectralView = static_cast<bool>(obj->getProperty("isSpectralView"));
        if (obj->hasProperty("hasSpectralBoxSelection")) s.hasSpectralBoxSelection = static_cast<bool>(obj->getProperty("hasSpectralBoxSelection"));
        if (obj->hasProperty("spectralTimeStart")) s.spectralTimeStart = static_cast<double>(obj->getProperty("spectralTimeStart"));
        if (obj->hasProperty("spectralTimeEnd")) s.spectralTimeEnd = static_cast<double>(obj->getProperty("spectralTimeEnd"));
        if (obj->hasProperty("spectralFreqLow")) s.spectralFreqLow = static_cast<float>(obj->getProperty("spectralFreqLow"));
        if (obj->hasProperty("spectralFreqHigh")) s.spectralFreqHigh = static_cast<float>(obj->getProperty("spectralFreqHigh"));
        return s;
    }
};

struct SampleMapZoneState
{
    juce::String filePath;
    juce::String sampleName;
    int rootNote { 60 };
    int keyLow { 48 };
    int keyHigh { 72 };
    int velLow { 0 };
    int velHigh { 127 };
    int roundRobinIndex { 1 };
    float fineTuneCents { 0.0f };
    float gainDb { 0.0f };
    float attackMs { 5.0f };
    float decayMs { 100.0f };
    float sustainLevel { 1.0f };
    float releaseMs { 200.0f };

    int groupIndex { 0 };
    float pan { 0.0f };
    juce::String trigger { "attack" }; // "attack", "release"
    int64_t sampleStart { 0 };
    int64_t sampleEnd { 0 };
    int64_t loopStart { 0 };
    int64_t loopEnd { 0 };
    bool loopEnabled { false };

    juce::var toVar() const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("filePath", filePath);
        obj->setProperty("sampleName", sampleName);
        obj->setProperty("rootNote", rootNote);
        obj->setProperty("keyLow", keyLow);
        obj->setProperty("keyHigh", keyHigh);
        obj->setProperty("velLow", velLow);
        obj->setProperty("velHigh", velHigh);
        obj->setProperty("roundRobinIndex", roundRobinIndex);
        obj->setProperty("fineTuneCents", fineTuneCents);
        obj->setProperty("gainDb", gainDb);
        obj->setProperty("attackMs", attackMs);
        obj->setProperty("decayMs", decayMs);
        obj->setProperty("sustainLevel", sustainLevel);
        obj->setProperty("releaseMs", releaseMs);
        obj->setProperty("groupIndex", groupIndex);
        obj->setProperty("pan", pan);
        obj->setProperty("trigger", trigger);
        obj->setProperty("sampleStart", static_cast<double>(sampleStart));
        obj->setProperty("sampleEnd", static_cast<double>(sampleEnd));
        obj->setProperty("loopStart", static_cast<double>(loopStart));
        obj->setProperty("loopEnd", static_cast<double>(loopEnd));
        obj->setProperty("loopEnabled", loopEnabled);
        return juce::var(obj);
    }

    static SampleMapZoneState fromVar(const juce::var& v)
    {
        SampleMapZoneState z;
        if (!v.isObject()) return z;
        auto* obj = v.getDynamicObject();
        if (!obj) return z;
        z.filePath = obj->getProperty("filePath").toString();
        z.sampleName = obj->getProperty("sampleName").toString();
        z.rootNote = static_cast<int>(obj->getProperty("rootNote"));
        z.keyLow = static_cast<int>(obj->getProperty("keyLow"));
        z.keyHigh = static_cast<int>(obj->getProperty("keyHigh"));
        z.velLow = static_cast<int>(obj->getProperty("velLow"));
        z.velHigh = static_cast<int>(obj->getProperty("velHigh"));
        if (obj->hasProperty("roundRobinIndex"))
            z.roundRobinIndex = static_cast<int>(obj->getProperty("roundRobinIndex"));
        else
            z.roundRobinIndex = 1;
        z.fineTuneCents = static_cast<float>(obj->getProperty("fineTuneCents"));
        z.gainDb = static_cast<float>(obj->getProperty("gainDb"));
        z.attackMs = static_cast<float>(obj->getProperty("attackMs"));
        z.decayMs = static_cast<float>(obj->getProperty("decayMs"));
        z.sustainLevel = static_cast<float>(obj->getProperty("sustainLevel"));
        z.releaseMs = static_cast<float>(obj->getProperty("releaseMs"));
        if (obj->hasProperty("groupIndex"))
            z.groupIndex = static_cast<int>(obj->getProperty("groupIndex"));
        if (obj->hasProperty("pan"))
            z.pan = static_cast<float>(obj->getProperty("pan"));
        if (obj->hasProperty("trigger"))
            z.trigger = obj->getProperty("trigger").toString();
        if (obj->hasProperty("sampleStart"))
            z.sampleStart = static_cast<int64_t>(static_cast<double>(obj->getProperty("sampleStart")));
        if (obj->hasProperty("sampleEnd"))
            z.sampleEnd = static_cast<int64_t>(static_cast<double>(obj->getProperty("sampleEnd")));
        if (obj->hasProperty("loopStart"))
            z.loopStart = static_cast<int64_t>(static_cast<double>(obj->getProperty("loopStart")));
        if (obj->hasProperty("loopEnd"))
            z.loopEnd = static_cast<int64_t>(static_cast<double>(obj->getProperty("loopEnd")));
        if (obj->hasProperty("loopEnabled"))
            z.loopEnabled = static_cast<bool>(obj->getProperty("loopEnabled"));
        return z;
    }

    std::unique_ptr<juce::XmlElement> toXml() const
    {
        auto xml = std::make_unique<juce::XmlElement>("Zone");
        xml->setAttribute("filePath", filePath);
        xml->setAttribute("sampleName", sampleName);
        xml->setAttribute("rootNote", rootNote);
        xml->setAttribute("keyLow", keyLow);
        xml->setAttribute("keyHigh", keyHigh);
        xml->setAttribute("velLow", velLow);
        xml->setAttribute("velHigh", velHigh);
        xml->setAttribute("roundRobinIndex", roundRobinIndex);
        xml->setAttribute("fineTuneCents", static_cast<double>(fineTuneCents));
        xml->setAttribute("gainDb", static_cast<double>(gainDb));
        xml->setAttribute("attackMs", static_cast<double>(attackMs));
        xml->setAttribute("decayMs", static_cast<double>(decayMs));
        xml->setAttribute("sustainLevel", static_cast<double>(sustainLevel));
        xml->setAttribute("releaseMs", static_cast<double>(releaseMs));
        xml->setAttribute("groupIndex", groupIndex);
        xml->setAttribute("pan", static_cast<double>(pan));
        xml->setAttribute("trigger", trigger);
        xml->setAttribute("sampleStart", static_cast<double>(sampleStart));
        xml->setAttribute("sampleEnd", static_cast<double>(sampleEnd));
        xml->setAttribute("loopStart", static_cast<double>(loopStart));
        xml->setAttribute("loopEnd", static_cast<double>(loopEnd));
        xml->setAttribute("loopEnabled", loopEnabled);
        return xml;
    }

    static SampleMapZoneState fromXml(const juce::XmlElement& xml, const juce::File& baseDir = {})
    {
        SampleMapZoneState z;
        z.filePath = xml.getStringAttribute("filePath");
        juce::String relPath = xml.getStringAttribute("relativePath");
        juce::String sampleName = xml.getStringAttribute("sampleName");

        if (baseDir.exists())
        {
            if (relPath.isNotEmpty() && baseDir.getChildFile(relPath).existsAsFile())
            {
                z.filePath = baseDir.getChildFile(relPath).getFullPathName();
            }
            else if (z.filePath.isNotEmpty() && baseDir.getChildFile(z.filePath).existsAsFile())
            {
                z.filePath = baseDir.getChildFile(z.filePath).getFullPathName();
            }
            else if (z.filePath.isNotEmpty() && baseDir.getChildFile("Samples").getChildFile(juce::File(z.filePath).getFileName()).existsAsFile())
            {
                z.filePath = baseDir.getChildFile("Samples").getChildFile(juce::File(z.filePath).getFileName()).getFullPathName();
            }
            else if (z.filePath.isNotEmpty() && baseDir.getChildFile(juce::File(z.filePath).getFileName()).existsAsFile())
            {
                z.filePath = baseDir.getChildFile(juce::File(z.filePath).getFileName()).getFullPathName();
            }
            else if (sampleName.isNotEmpty() && baseDir.getChildFile("Samples").getChildFile(sampleName).existsAsFile())
            {
                z.filePath = baseDir.getChildFile("Samples").getChildFile(sampleName).getFullPathName();
            }
            else if (sampleName.isNotEmpty() && baseDir.getChildFile(sampleName).existsAsFile())
            {
                z.filePath = baseDir.getChildFile(sampleName).getFullPathName();
            }
        }

        z.sampleName = sampleName.isNotEmpty() ? sampleName : juce::File(z.filePath).getFileName();
        z.rootNote = xml.getIntAttribute("rootNote", 60);
        z.keyLow = xml.getIntAttribute("keyLow", 48);
        z.keyHigh = xml.getIntAttribute("keyHigh", 72);
        z.velLow = xml.getIntAttribute("velLow", 0);
        z.velHigh = xml.getIntAttribute("velHigh", 127);
        z.roundRobinIndex = xml.getIntAttribute("roundRobinIndex", 1);
        z.fineTuneCents = static_cast<float>(xml.getDoubleAttribute("fineTuneCents", 0.0));
        z.gainDb = static_cast<float>(xml.getDoubleAttribute("gainDb", 0.0));
        z.attackMs = static_cast<float>(xml.getDoubleAttribute("attackMs", 5.0));
        z.decayMs = static_cast<float>(xml.getDoubleAttribute("decayMs", 100.0));
        z.sustainLevel = static_cast<float>(xml.getDoubleAttribute("sustainLevel", 1.0));
        z.releaseMs = static_cast<float>(xml.getDoubleAttribute("releaseMs", 200.0));
        z.groupIndex = xml.getIntAttribute("groupIndex", 0);
        z.pan = static_cast<float>(xml.getDoubleAttribute("pan", 0.0));
        z.trigger = xml.getStringAttribute("trigger", "attack");
        z.sampleStart = static_cast<int64_t>(xml.getDoubleAttribute("sampleStart", 0.0));
        z.sampleEnd = static_cast<int64_t>(xml.getDoubleAttribute("sampleEnd", 0.0));
        z.loopStart = static_cast<int64_t>(xml.getDoubleAttribute("loopStart", 0.0));
        z.loopEnd = static_cast<int64_t>(xml.getDoubleAttribute("loopEnd", 0.0));
        z.loopEnabled = xml.getBoolAttribute("loopEnabled", false);
        return z;
    }
};

struct DecentSamplerGroupState
{
    int index { 0 };
    juce::String name;
    float volumeDb { 0.0f };
    float pan { 0.0f };
    float fineTuneCents { 0.0f };
    float attackMs { 5.0f };
    float decayMs { 100.0f };
    float sustainLevel { 1.0f };
    float releaseMs { 200.0f };
    bool enabled { true };
    bool muted { false };
    int seqPosition { 1 };
    juce::String seqMode;
    juce::String trigger { "attack" };

    juce::var toVar() const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("index", index);
        obj->setProperty("name", name);
        obj->setProperty("volumeDb", volumeDb);
        obj->setProperty("pan", pan);
        obj->setProperty("fineTuneCents", fineTuneCents);
        obj->setProperty("attackMs", attackMs);
        obj->setProperty("decayMs", decayMs);
        obj->setProperty("sustainLevel", sustainLevel);
        obj->setProperty("releaseMs", releaseMs);
        obj->setProperty("enabled", enabled);
        obj->setProperty("muted", muted);
        obj->setProperty("seqPosition", seqPosition);
        obj->setProperty("seqMode", seqMode);
        obj->setProperty("trigger", trigger);
        return juce::var(obj);
    }

    static DecentSamplerGroupState fromVar(const juce::var& v)
    {
        DecentSamplerGroupState g;
        if (!v.isObject()) return g;
        auto* obj = v.getDynamicObject();
        if (!obj) return g;
        g.index = static_cast<int>(obj->getProperty("index"));
        g.name = obj->getProperty("name").toString();
        g.volumeDb = static_cast<float>(obj->getProperty("volumeDb"));
        g.pan = static_cast<float>(obj->getProperty("pan"));
        g.fineTuneCents = static_cast<float>(obj->getProperty("fineTuneCents"));
        g.attackMs = static_cast<float>(obj->getProperty("attackMs"));
        g.decayMs = static_cast<float>(obj->getProperty("decayMs"));
        g.sustainLevel = static_cast<float>(obj->getProperty("sustainLevel"));
        g.releaseMs = static_cast<float>(obj->getProperty("releaseMs"));
        g.enabled = static_cast<bool>(obj->getProperty("enabled"));
        g.muted = static_cast<bool>(obj->getProperty("muted"));
        g.seqPosition = static_cast<int>(obj->getProperty("seqPosition"));
        g.seqMode = obj->getProperty("seqMode").toString();
        g.trigger = obj->getProperty("trigger").toString();
        return g;
    }
};

struct DecentSamplerModulatorState
{
    juce::String id;
    juce::String type { "lfo" };
    juce::String shape { "sine" }; // "sine", "triangle", "saw", "square", "random"
    float frequency { 1.0f };
    float modAmount { 0.0f };
    juce::String target { "pitch" }; // "pitch", "volume", "pan", "cutoff"
    juce::String scope { "global" }; // "global", "voice", "group"
    int groupIndex { -1 };

    juce::var toVar() const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("id", id);
        obj->setProperty("type", type);
        obj->setProperty("shape", shape);
        obj->setProperty("frequency", frequency);
        obj->setProperty("modAmount", modAmount);
        obj->setProperty("target", target);
        obj->setProperty("scope", scope);
        obj->setProperty("groupIndex", groupIndex);
        return juce::var(obj);
    }

    static DecentSamplerModulatorState fromVar(const juce::var& v)
    {
        DecentSamplerModulatorState m;
        if (!v.isObject()) return m;
        auto* obj = v.getDynamicObject();
        if (!obj) return m;
        m.id = obj->getProperty("id").toString();
        m.type = obj->getProperty("type").toString();
        m.shape = obj->getProperty("shape").toString();
        m.frequency = static_cast<float>(obj->getProperty("frequency"));
        m.modAmount = static_cast<float>(obj->getProperty("modAmount"));
        m.target = obj->getProperty("target").toString();
        m.scope = obj->getProperty("scope").toString();
        m.groupIndex = static_cast<int>(obj->getProperty("groupIndex"));
        return m;
    }
};

struct DecentSamplerEffectState
{
    juce::String type;
    juce::String path;
    juce::String resolvedPath;
    float wetLevel { 0.0f };
    float dryLevel { 1.0f };
    float frequency { 20000.0f };
    float resonance { 0.707f };
    float delayTimeMs { 250.0f };
    float feedback { 0.3f };
    float roomSize { 0.5f };
    float damping { 0.5f };
    int groupIndex { -1 };

    juce::var toVar() const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("type", type);
        obj->setProperty("path", path);
        obj->setProperty("resolvedPath", resolvedPath);
        obj->setProperty("wetLevel", wetLevel);
        obj->setProperty("dryLevel", dryLevel);
        obj->setProperty("frequency", frequency);
        obj->setProperty("resonance", resonance);
        obj->setProperty("delayTimeMs", delayTimeMs);
        obj->setProperty("feedback", feedback);
        obj->setProperty("roomSize", roomSize);
        obj->setProperty("damping", damping);
        obj->setProperty("groupIndex", groupIndex);
        return juce::var(obj);
    }

    static DecentSamplerEffectState fromVar(const juce::var& v)
    {
        DecentSamplerEffectState e;
        if (!v.isObject()) return e;
        auto* obj = v.getDynamicObject();
        if (!obj) return e;
        e.type = obj->getProperty("type").toString();
        e.path = obj->getProperty("path").toString();
        e.resolvedPath = obj->getProperty("resolvedPath").toString();
        e.wetLevel = static_cast<float>(obj->getProperty("wetLevel"));
        e.dryLevel = static_cast<float>(obj->getProperty("dryLevel"));
        e.frequency = static_cast<float>(obj->getProperty("frequency"));
        e.resonance = static_cast<float>(obj->getProperty("resonance"));
        e.delayTimeMs = static_cast<float>(obj->getProperty("delayTimeMs"));
        e.feedback = static_cast<float>(obj->getProperty("feedback"));
        e.roomSize = static_cast<float>(obj->getProperty("roomSize"));
        e.damping = static_cast<float>(obj->getProperty("damping"));
        e.groupIndex = static_cast<int>(obj->getProperty("groupIndex"));
        return e;
    }
};

struct DecentSamplerBinding
{
    juce::String type;
    juce::String level;
    int position { 0 };
    juce::String parameter;
    juce::String translation;

    juce::var toVar() const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("type", type);
        obj->setProperty("level", level);
        obj->setProperty("position", position);
        obj->setProperty("parameter", parameter);
        obj->setProperty("translation", translation);
        return juce::var(obj);
    }
    static DecentSamplerBinding fromVar(const juce::var& v)
    {
        DecentSamplerBinding b;
        if (!v.isObject()) return b;
        auto* obj = v.getDynamicObject();
        if (!obj) return b;
        b.type = obj->getProperty("type").toString();
        b.level = obj->getProperty("level").toString();
        b.position = static_cast<int>(obj->getProperty("position"));
        b.parameter = obj->getProperty("parameter").toString();
        b.translation = obj->getProperty("translation").toString();
        return b;
    }
};

struct DecentSamplerMidiCcMapping
{
    int ccNumber { 1 };
    std::vector<DecentSamplerBinding> bindings;

    juce::var toVar() const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("ccNumber", ccNumber);
        juce::Array<juce::var> bindArray;
        for (const auto& b : bindings)
            bindArray.add(b.toVar());
        obj->setProperty("bindings", bindArray);
        return juce::var(obj);
    }

    static DecentSamplerMidiCcMapping fromVar(const juce::var& v)
    {
        DecentSamplerMidiCcMapping m;
        if (!v.isObject()) return m;
        auto* obj = v.getDynamicObject();
        if (!obj) return m;
        m.ccNumber = static_cast<int>(obj->getProperty("ccNumber"));
        if (obj->hasProperty("bindings") && obj->getProperty("bindings").isArray())
        {
            for (const auto& bv : *obj->getProperty("bindings").getArray())
                m.bindings.push_back(DecentSamplerBinding::fromVar(bv));
        }
        return m;
    }
};

struct DecentSamplerUiLabel
{
    int x { 0 };
    int y { 0 };
    int width { 120 };
    int height { 30 };
    juce::String text;
    float textSize { 14.0f };
    juce::String textColorHex;
    juce::String textAlignment { "center" };

    juce::var toVar() const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("x", x);
        obj->setProperty("y", y);
        obj->setProperty("width", width);
        obj->setProperty("height", height);
        obj->setProperty("text", text);
        obj->setProperty("textSize", textSize);
        obj->setProperty("textColorHex", textColorHex);
        obj->setProperty("textAlignment", textAlignment);
        return juce::var(obj);
    }
    static DecentSamplerUiLabel fromVar(const juce::var& v)
    {
        DecentSamplerUiLabel l;
        if (!v.isObject()) return l;
        auto* obj = v.getDynamicObject();
        if (!obj) return l;
        l.x = static_cast<int>(obj->getProperty("x"));
        l.y = static_cast<int>(obj->getProperty("y"));
        l.width = static_cast<int>(obj->getProperty("width"));
        l.height = static_cast<int>(obj->getProperty("height"));
        l.text = obj->getProperty("text").toString();
        l.textSize = static_cast<float>(obj->getProperty("textSize"));
        l.textColorHex = obj->getProperty("textColorHex").toString();
        l.textAlignment = obj->getProperty("textAlignment").toString();
        return l;
    }
};

struct DecentSamplerUiImage
{
    int x { 0 };
    int y { 0 };
    int width { 100 };
    int height { 100 };
    juce::String path;
    juce::String resolvedFilePath;

    juce::var toVar() const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("x", x);
        obj->setProperty("y", y);
        obj->setProperty("width", width);
        obj->setProperty("height", height);
        obj->setProperty("path", path);
        obj->setProperty("resolvedFilePath", resolvedFilePath);
        return juce::var(obj);
    }
    static DecentSamplerUiImage fromVar(const juce::var& v)
    {
        DecentSamplerUiImage img;
        if (!v.isObject()) return img;
        auto* obj = v.getDynamicObject();
        if (!obj) return img;
        img.x = static_cast<int>(obj->getProperty("x"));
        img.y = static_cast<int>(obj->getProperty("y"));
        img.width = static_cast<int>(obj->getProperty("width"));
        img.height = static_cast<int>(obj->getProperty("height"));
        img.path = obj->getProperty("path").toString();
        img.resolvedFilePath = obj->getProperty("resolvedFilePath").toString();
        return img;
    }
};

struct DecentSamplerUiButton
{
    int x { 0 };
    int y { 0 };
    int width { 80 };
    int height { 30 };
    juce::String text;
    juce::String style { "toggle" };
    bool state { false };
    std::vector<DecentSamplerBinding> bindings;

    juce::var toVar() const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("x", x);
        obj->setProperty("y", y);
        obj->setProperty("width", width);
        obj->setProperty("height", height);
        obj->setProperty("text", text);
        obj->setProperty("style", style);
        obj->setProperty("state", state);
        juce::Array<juce::var> bArray;
        for (const auto& b : bindings) bArray.add(b.toVar());
        obj->setProperty("bindings", bArray);
        return juce::var(obj);
    }
    static DecentSamplerUiButton fromVar(const juce::var& v)
    {
        DecentSamplerUiButton btn;
        if (!v.isObject()) return btn;
        auto* obj = v.getDynamicObject();
        if (!obj) return btn;
        btn.x = static_cast<int>(obj->getProperty("x"));
        btn.y = static_cast<int>(obj->getProperty("y"));
        btn.width = static_cast<int>(obj->getProperty("width"));
        btn.height = static_cast<int>(obj->getProperty("height"));
        btn.text = obj->getProperty("text").toString();
        btn.style = obj->getProperty("style").toString();
        btn.state = static_cast<bool>(obj->getProperty("state"));
        if (obj->hasProperty("bindings") && obj->getProperty("bindings").isArray())
        {
            for (const auto& bv : *obj->getProperty("bindings").getArray())
                btn.bindings.push_back(DecentSamplerBinding::fromVar(bv));
        }
        return btn;
    }
};

struct DecentSamplerUiMenu
{
    int x { 0 };
    int y { 0 };
    int width { 120 };
    int height { 30 };
    juce::StringArray options;
    int selectedIndex { 0 };
    std::vector<DecentSamplerBinding> bindings;

    juce::var toVar() const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("x", x);
        obj->setProperty("y", y);
        obj->setProperty("width", width);
        obj->setProperty("height", height);
        juce::Array<juce::var> optArray;
        for (const auto& o : options) optArray.add(o);
        obj->setProperty("options", optArray);
        obj->setProperty("selectedIndex", selectedIndex);
        juce::Array<juce::var> bArray;
        for (const auto& b : bindings) bArray.add(b.toVar());
        obj->setProperty("bindings", bArray);
        return juce::var(obj);
    }
    static DecentSamplerUiMenu fromVar(const juce::var& v)
    {
        DecentSamplerUiMenu m;
        if (!v.isObject()) return m;
        auto* obj = v.getDynamicObject();
        if (!obj) return m;
        m.x = static_cast<int>(obj->getProperty("x"));
        m.y = static_cast<int>(obj->getProperty("y"));
        m.width = static_cast<int>(obj->getProperty("width"));
        m.height = static_cast<int>(obj->getProperty("height"));
        m.selectedIndex = static_cast<int>(obj->getProperty("selectedIndex"));
        if (obj->hasProperty("options") && obj->getProperty("options").isArray())
        {
            for (const auto& ov : *obj->getProperty("options").getArray())
                m.options.add(ov.toString());
        }
        if (obj->hasProperty("bindings") && obj->getProperty("bindings").isArray())
        {
            for (const auto& bv : *obj->getProperty("bindings").getArray())
                m.bindings.push_back(DecentSamplerBinding::fromVar(bv));
        }
        return m;
    }
};

struct DecentSamplerUiControl
{
    int x { -1 };
    int y { -1 };
    int width { 80 };
    int height { 80 };
    juce::String id;
    juce::String label;
    juce::String parameterName;
    juce::String type;
    juce::String style;
    juce::String units;
    float textSize { 14.0f };
    double minValue { 0.0 };
    double maxValue { 1.0 };
    double defaultValue { 0.0 };
    double currentValue { 0.0 };
    juce::String textColorHex;
    juce::String trackColorHex;
    juce::String trackBackgroundColorHex;
    juce::String customSkinImagePath;
    juce::String resolvedCustomSkinImagePath;
    int customSkinNumFrames { 0 };
    juce::String bindingType;
    juce::String bindingParam;
    std::vector<DecentSamplerBinding> bindings;

    juce::var toVar() const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("x", x);
        obj->setProperty("y", y);
        obj->setProperty("width", width);
        obj->setProperty("height", height);
        obj->setProperty("id", id);
        obj->setProperty("label", label);
        obj->setProperty("parameterName", parameterName);
        obj->setProperty("type", type);
        obj->setProperty("style", style);
        obj->setProperty("units", units);
        obj->setProperty("textSize", textSize);
        obj->setProperty("minValue", minValue);
        obj->setProperty("maxValue", maxValue);
        obj->setProperty("defaultValue", defaultValue);
        obj->setProperty("currentValue", currentValue);
        obj->setProperty("textColorHex", textColorHex);
        obj->setProperty("trackColorHex", trackColorHex);
        obj->setProperty("trackBackgroundColorHex", trackBackgroundColorHex);
        obj->setProperty("customSkinImagePath", customSkinImagePath);
        obj->setProperty("resolvedCustomSkinImagePath", resolvedCustomSkinImagePath);
        obj->setProperty("customSkinNumFrames", customSkinNumFrames);
        obj->setProperty("bindingType", bindingType);
        obj->setProperty("bindingParam", bindingParam);
        juce::Array<juce::var> bArray;
        for (const auto& b : bindings) bArray.add(b.toVar());
        obj->setProperty("bindings", bArray);
        return juce::var(obj);
    }

    static DecentSamplerUiControl fromVar(const juce::var& v)
    {
        DecentSamplerUiControl c;
        if (!v.isObject()) return c;
        auto* obj = v.getDynamicObject();
        if (!obj) return c;
        if (obj->hasProperty("x")) c.x = static_cast<int>(obj->getProperty("x"));
        if (obj->hasProperty("y")) c.y = static_cast<int>(obj->getProperty("y"));
        if (obj->hasProperty("width")) c.width = static_cast<int>(obj->getProperty("width"));
        if (obj->hasProperty("height")) c.height = static_cast<int>(obj->getProperty("height"));
        c.id = obj->getProperty("id").toString();
        c.label = obj->getProperty("label").toString();
        c.parameterName = obj->getProperty("parameterName").toString();
        c.type = obj->getProperty("type").toString();
        c.style = obj->getProperty("style").toString();
        c.units = obj->getProperty("units").toString();
        if (obj->hasProperty("textSize")) c.textSize = static_cast<float>(obj->getProperty("textSize"));
        if (obj->hasProperty("minValue")) c.minValue = static_cast<double>(obj->getProperty("minValue"));
        if (obj->hasProperty("maxValue")) c.maxValue = static_cast<double>(obj->getProperty("maxValue"));
        if (obj->hasProperty("defaultValue")) c.defaultValue = static_cast<double>(obj->getProperty("defaultValue"));
        if (obj->hasProperty("currentValue")) c.currentValue = static_cast<double>(obj->getProperty("currentValue"));
        c.textColorHex = obj->getProperty("textColorHex").toString();
        c.trackColorHex = obj->getProperty("trackColorHex").toString();
        c.trackBackgroundColorHex = obj->getProperty("trackBackgroundColorHex").toString();
        c.customSkinImagePath = obj->getProperty("customSkinImagePath").toString();
        c.resolvedCustomSkinImagePath = obj->getProperty("resolvedCustomSkinImagePath").toString();
        if (obj->hasProperty("customSkinNumFrames")) c.customSkinNumFrames = static_cast<int>(obj->getProperty("customSkinNumFrames"));
        c.bindingType = obj->getProperty("bindingType").toString();
        c.bindingParam = obj->getProperty("bindingParam").toString();
        if (obj->hasProperty("bindings") && obj->getProperty("bindings").isArray())
        {
            for (const auto& bv : *obj->getProperty("bindings").getArray())
                c.bindings.push_back(DecentSamplerBinding::fromVar(bv));
        }
        return c;
    }
};

struct DecentSamplerTabState
{
    juce::String name { "main" };
    std::vector<DecentSamplerUiLabel> labels;
    std::vector<DecentSamplerUiImage> images;
    std::vector<DecentSamplerUiControl> controls;
    std::vector<DecentSamplerUiButton> buttons;
    std::vector<DecentSamplerUiMenu> menus;

    juce::var toVar() const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("name", name);
        juce::Array<juce::var> lArray, iArray, cArray, bArray, mArray;
        for (const auto& l : labels) lArray.add(l.toVar());
        for (const auto& i : images) iArray.add(i.toVar());
        for (const auto& c : controls) cArray.add(c.toVar());
        for (const auto& b : buttons) bArray.add(b.toVar());
        for (const auto& m : menus) mArray.add(m.toVar());
        obj->setProperty("labels", lArray);
        obj->setProperty("images", iArray);
        obj->setProperty("controls", cArray);
        obj->setProperty("buttons", bArray);
        obj->setProperty("menus", mArray);
        return juce::var(obj);
    }
    static DecentSamplerTabState fromVar(const juce::var& v)
    {
        DecentSamplerTabState t;
        if (!v.isObject()) return t;
        auto* obj = v.getDynamicObject();
        if (!obj) return t;
        t.name = obj->getProperty("name").toString();
        if (obj->hasProperty("labels") && obj->getProperty("labels").isArray())
            for (const auto& item : *obj->getProperty("labels").getArray()) t.labels.push_back(DecentSamplerUiLabel::fromVar(item));
        if (obj->hasProperty("images") && obj->getProperty("images").isArray())
            for (const auto& item : *obj->getProperty("images").getArray()) t.images.push_back(DecentSamplerUiImage::fromVar(item));
        if (obj->hasProperty("controls") && obj->getProperty("controls").isArray())
            for (const auto& item : *obj->getProperty("controls").getArray()) t.controls.push_back(DecentSamplerUiControl::fromVar(item));
        if (obj->hasProperty("buttons") && obj->getProperty("buttons").isArray())
            for (const auto& item : *obj->getProperty("buttons").getArray()) t.buttons.push_back(DecentSamplerUiButton::fromVar(item));
        if (obj->hasProperty("menus") && obj->getProperty("menus").isArray())
            for (const auto& item : *obj->getProperty("menus").getArray()) t.menus.push_back(DecentSamplerUiMenu::fromVar(item));
        return t;
    }
};

struct DecentSamplerUiState
{
    int width { 812 };
    int height { 375 };
    juce::String bgImagePath;
    juce::String resolvedBgImagePath;
    juce::String bgColorHex;
    std::vector<DecentSamplerTabState> tabs;

    bool hasCustomUi() const
    {
        if (resolvedBgImagePath.isNotEmpty()) return true;
        for (const auto& tab : tabs)
        {
            if (!tab.labels.empty() || !tab.images.empty() || !tab.controls.empty() || !tab.buttons.empty() || !tab.menus.empty())
                return true;
        }
        return false;
    }

    juce::var toVar() const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("width", width);
        obj->setProperty("height", height);
        obj->setProperty("bgImagePath", bgImagePath);
        obj->setProperty("resolvedBgImagePath", resolvedBgImagePath);
        obj->setProperty("bgColorHex", bgColorHex);
        juce::Array<juce::var> tabArr;
        for (const auto& t : tabs) tabArr.add(t.toVar());
        obj->setProperty("tabs", tabArr);
        return juce::var(obj);
    }
    static DecentSamplerUiState fromVar(const juce::var& v)
    {
        DecentSamplerUiState u;
        if (!v.isObject()) return u;
        auto* obj = v.getDynamicObject();
        if (!obj) return u;
        if (obj->hasProperty("width")) u.width = static_cast<int>(obj->getProperty("width"));
        if (obj->hasProperty("height")) u.height = static_cast<int>(obj->getProperty("height"));
        u.bgImagePath = obj->getProperty("bgImagePath").toString();
        u.resolvedBgImagePath = obj->getProperty("resolvedBgImagePath").toString();
        u.bgColorHex = obj->getProperty("bgColorHex").toString();
        if (obj->hasProperty("tabs") && obj->getProperty("tabs").isArray())
        {
            for (const auto& tv : *obj->getProperty("tabs").getArray())
                u.tabs.push_back(DecentSamplerTabState::fromVar(tv));
        }
        return u;
    }
};

struct SampleMapState
{
    std::vector<SampleMapZoneState> zones;
    std::vector<DecentSamplerGroupState> groups;
    std::vector<DecentSamplerModulatorState> modulators;
    std::vector<DecentSamplerEffectState> effects;
    std::vector<DecentSamplerUiControl> uiControls;
    std::vector<DecentSamplerMidiCcMapping> midiCcMappings;
    DecentSamplerUiState customUi;
    juce::String instrumentName;
    juce::String irFilePath;
    float irReverbWetLevel { 0.0f };
    float irReverbDryLevel { 1.0f };
    float delayTimeMs { 250.0f };
    float delayFeedback { 0.0f };
    float delayWetLevel { 0.0f };
    float chorusRateHz { 1.0f };
    float chorusDepth { 0.0f };
    float chorusWetLevel { 0.0f };
    float globalAttackMs { 5.0f };
    float globalDecayMs { 100.0f };
    float globalSustainLevel { 1.0f };
    float globalReleaseMs { 200.0f };
    float samplerReverbAmount { 0.0f };
    float masterFilterCutoffHz { 20000.0f };
    float masterHighpassHz { 20.0f };
    float masterTone { 1.0f };
    float masterGainDb { 0.0f };
    float masterFineTuneCents { 0.0f };
    bool pitchTrackingEnabled { true };
    int roundRobinMode { 0 }; // 0 = Cycle, 1 = Random, 2 = Off

    juce::var toVar() const
    {
        auto* obj = new juce::DynamicObject();
        juce::Array<juce::var> zoneArray;
        for (const auto& z : zones)
            zoneArray.add(z.toVar());
        obj->setProperty("zones", zoneArray);

        juce::Array<juce::var> grpArray;
        for (const auto& g : groups)
            grpArray.add(g.toVar());
        obj->setProperty("groups", grpArray);

        juce::Array<juce::var> modArray;
        for (const auto& m : modulators)
            modArray.add(m.toVar());
        obj->setProperty("modulators", modArray);

        juce::Array<juce::var> fxArray;
        for (const auto& f : effects)
            fxArray.add(f.toVar());
        obj->setProperty("effects", fxArray);

        juce::Array<juce::var> ctrlArray;
        for (const auto& c : uiControls)
            ctrlArray.add(c.toVar());
        obj->setProperty("uiControls", ctrlArray);

        juce::Array<juce::var> ccArray;
        for (const auto& m : midiCcMappings)
            ccArray.add(m.toVar());
        obj->setProperty("midiCcMappings", ccArray);

        obj->setProperty("customUi", customUi.toVar());
        obj->setProperty("instrumentName", instrumentName);
        obj->setProperty("irFilePath", irFilePath);
        obj->setProperty("irReverbWetLevel", irReverbWetLevel);
        obj->setProperty("irReverbDryLevel", irReverbDryLevel);
        obj->setProperty("delayTimeMs", delayTimeMs);
        obj->setProperty("delayFeedback", delayFeedback);
        obj->setProperty("delayWetLevel", delayWetLevel);
        obj->setProperty("chorusRateHz", chorusRateHz);
        obj->setProperty("chorusDepth", chorusDepth);
        obj->setProperty("chorusWetLevel", chorusWetLevel);
        obj->setProperty("globalAttackMs", globalAttackMs);
        obj->setProperty("globalDecayMs", globalDecayMs);
        obj->setProperty("globalSustainLevel", globalSustainLevel);
        obj->setProperty("globalReleaseMs", globalReleaseMs);
        obj->setProperty("samplerReverbAmount", samplerReverbAmount);
        obj->setProperty("masterFilterCutoffHz", masterFilterCutoffHz);
        obj->setProperty("masterHighpassHz", masterHighpassHz);
        obj->setProperty("masterTone", masterTone);
        obj->setProperty("masterGainDb", masterGainDb);
        obj->setProperty("masterFineTuneCents", masterFineTuneCents);
        obj->setProperty("pitchTrackingEnabled", pitchTrackingEnabled);
        obj->setProperty("roundRobinMode", roundRobinMode);
        return juce::var(obj);
    }

    static SampleMapState fromVar(const juce::var& v)
    {
        SampleMapState s;
        if (!v.isObject()) return s;
        auto* obj = v.getDynamicObject();
        if (!obj) return s;

        if (obj->hasProperty("zones") && obj->getProperty("zones").isArray())
        {
            for (const auto& zv : *obj->getProperty("zones").getArray())
                s.zones.push_back(SampleMapZoneState::fromVar(zv));
        }
        if (obj->hasProperty("groups") && obj->getProperty("groups").isArray())
        {
            for (const auto& gv : *obj->getProperty("groups").getArray())
                s.groups.push_back(DecentSamplerGroupState::fromVar(gv));
        }
        if (obj->hasProperty("modulators") && obj->getProperty("modulators").isArray())
        {
            for (const auto& mv : *obj->getProperty("modulators").getArray())
                s.modulators.push_back(DecentSamplerModulatorState::fromVar(mv));
        }
        if (obj->hasProperty("effects") && obj->getProperty("effects").isArray())
        {
            for (const auto& fv : *obj->getProperty("effects").getArray())
                s.effects.push_back(DecentSamplerEffectState::fromVar(fv));
        }
        if (obj->hasProperty("uiControls") && obj->getProperty("uiControls").isArray())
        {
            for (const auto& cv : *obj->getProperty("uiControls").getArray())
                s.uiControls.push_back(DecentSamplerUiControl::fromVar(cv));
        }
        if (obj->hasProperty("midiCcMappings") && obj->getProperty("midiCcMappings").isArray())
        {
            for (const auto& ccv : *obj->getProperty("midiCcMappings").getArray())
                s.midiCcMappings.push_back(DecentSamplerMidiCcMapping::fromVar(ccv));
        }
        if (obj->hasProperty("customUi")) s.customUi = DecentSamplerUiState::fromVar(obj->getProperty("customUi"));
        if (obj->hasProperty("instrumentName")) s.instrumentName = obj->getProperty("instrumentName").toString();
        if (obj->hasProperty("irFilePath")) s.irFilePath = obj->getProperty("irFilePath").toString();
        if (obj->hasProperty("irReverbWetLevel")) s.irReverbWetLevel = static_cast<float>(obj->getProperty("irReverbWetLevel"));
        if (obj->hasProperty("irReverbDryLevel")) s.irReverbDryLevel = static_cast<float>(obj->getProperty("irReverbDryLevel"));
        if (obj->hasProperty("delayTimeMs")) s.delayTimeMs = static_cast<float>(obj->getProperty("delayTimeMs"));
        if (obj->hasProperty("delayFeedback")) s.delayFeedback = static_cast<float>(obj->getProperty("delayFeedback"));
        if (obj->hasProperty("delayWetLevel")) s.delayWetLevel = static_cast<float>(obj->getProperty("delayWetLevel"));
        if (obj->hasProperty("chorusRateHz")) s.chorusRateHz = static_cast<float>(obj->getProperty("chorusRateHz"));
        if (obj->hasProperty("chorusDepth")) s.chorusDepth = static_cast<float>(obj->getProperty("chorusDepth"));
        if (obj->hasProperty("chorusWetLevel")) s.chorusWetLevel = static_cast<float>(obj->getProperty("chorusWetLevel"));
        if (obj->hasProperty("globalAttackMs")) s.globalAttackMs = static_cast<float>(obj->getProperty("globalAttackMs"));
        if (obj->hasProperty("globalDecayMs")) s.globalDecayMs = static_cast<float>(obj->getProperty("globalDecayMs"));
        if (obj->hasProperty("globalSustainLevel")) s.globalSustainLevel = static_cast<float>(obj->getProperty("globalSustainLevel"));
        if (obj->hasProperty("globalReleaseMs")) s.globalReleaseMs = static_cast<float>(obj->getProperty("globalReleaseMs"));
        if (obj->hasProperty("samplerReverbAmount")) s.samplerReverbAmount = static_cast<float>(obj->getProperty("samplerReverbAmount"));
        if (obj->hasProperty("masterFilterCutoffHz")) s.masterFilterCutoffHz = static_cast<float>(obj->getProperty("masterFilterCutoffHz"));
        if (obj->hasProperty("masterHighpassHz")) s.masterHighpassHz = static_cast<float>(obj->getProperty("masterHighpassHz"));
        if (obj->hasProperty("masterTone")) s.masterTone = static_cast<float>(obj->getProperty("masterTone"));
        if (obj->hasProperty("masterGainDb")) s.masterGainDb = static_cast<float>(obj->getProperty("masterGainDb"));
        if (obj->hasProperty("masterFineTuneCents")) s.masterFineTuneCents = static_cast<float>(obj->getProperty("masterFineTuneCents"));
        if (obj->hasProperty("pitchTrackingEnabled")) s.pitchTrackingEnabled = static_cast<bool>(obj->getProperty("pitchTrackingEnabled"));
        if (obj->hasProperty("roundRobinMode")) s.roundRobinMode = static_cast<int>(obj->getProperty("roundRobinMode"));
        return s;
    }

    std::unique_ptr<juce::XmlElement> toXml(const juce::File& targetFile = {}) const
    {
        auto xml = std::make_unique<juce::XmlElement>("SampleMap");
        xml->setAttribute("version", "1.0");
        xml->setAttribute("globalAttackMs", static_cast<double>(globalAttackMs));
        xml->setAttribute("globalDecayMs", static_cast<double>(globalDecayMs));
        xml->setAttribute("globalSustainLevel", static_cast<double>(globalSustainLevel));
        xml->setAttribute("globalReleaseMs", static_cast<double>(globalReleaseMs));
        xml->setAttribute("samplerReverbAmount", static_cast<double>(samplerReverbAmount));
        xml->setAttribute("pitchTrackingEnabled", pitchTrackingEnabled ? 1 : 0);
        xml->setAttribute("roundRobinMode", roundRobinMode);

        auto baseDir = targetFile.getParentDirectory();
        for (const auto& z : zones)
        {
            auto zXml = z.toXml();
            if (baseDir.exists())
            {
                juce::File zFile(z.filePath);
                zXml->setAttribute("relativePath", zFile.getRelativePathFrom(baseDir));
            }
            xml->addChildElement(zXml.release());
        }
        return xml;
    }

    static juce::String getXmlAttrCaseInsensitive(const juce::XmlElement& elem, const std::initializer_list<const char*>& attrNames)
    {
        for (const char* name : attrNames)
        {
            for (int i = 0; i < elem.getNumAttributes(); ++i)
            {
                if (elem.getAttributeName(i).equalsIgnoreCase(name))
                    return elem.getAttributeValue(i);
            }
        }
        return {};
    }

    static int parseDecentSamplerNote(const juce::String& text, int defaultVal = 60)
    {
        auto s = text.trim();
        if (s.isEmpty()) return defaultVal;

        if (s.containsOnly("0123456789+-"))
            return juce::jlimit(0, 127, s.getIntValue());

        juce::String upper = s.toUpperCase();
        juce::juce_wchar noteChar = upper[0];
        int baseNote = 0;
        switch (noteChar)
        {
            case 'C': baseNote = 0; break;
            case 'D': baseNote = 2; break;
            case 'E': baseNote = 4; break;
            case 'F': baseNote = 5; break;
            case 'G': baseNote = 7; break;
            case 'A': baseNote = 9; break;
            case 'B': baseNote = 11; break;
            default: return defaultVal;
        }

        int idx = 1;
        if (idx < upper.length())
        {
            juce::juce_wchar acc = upper[idx];
            if (acc == '#' || acc == 'S')
            {
                baseNote += 1;
                idx++;
            }
            else if (acc == 'B' || acc == 'F')
            {
                baseNote -= 1;
                idx++;
            }
        }

        juce::String octaveStr = upper.substring(idx).trim();
        int octave = 3; // Decent Sampler standard: Middle C is C3 (MIDI 60)
        if (octaveStr.isNotEmpty())
            octave = octaveStr.getIntValue();

        // In Decent Sampler (Ableton/Kontakt standard): C3 = 60, C-2 = 0, C4 = 72, A3 = 69
        int midiNote = (octave + 2) * 12 + baseNote;
        return juce::jlimit(0, 127, midiNote);
    }

    static int parseDecentSamplerVelocity(const juce::String& text, int defaultVal)
    {
        auto s = text.trim();
        if (s.isEmpty()) return defaultVal;

        // If explicitly formatted with a decimal point and between 0.0 and 1.0 (e.g. "0.5")
        if (s.contains(".") && !s.containsOnly("0123456789+-"))
        {
            double d = s.getDoubleValue();
            if (d >= 0.0 && d <= 1.0)
                return juce::jlimit(0, 127, static_cast<int>(std::round(d * 127.0)));
        }

        int val = s.getIntValue();
        return juce::jlimit(0, 127, val);
    }

    static float parseDecentSamplerVolume(const juce::String& text, float defaultDb = 0.0f)
    {
        auto s = text.trim();
        if (s.isEmpty()) return defaultDb;

        if (s.endsWithIgnoreCase("db"))
            return s.dropLastCharacters(2).trim().getFloatValue();

        float val = s.getFloatValue();
        if (val < 0.0f || s.startsWith("+"))
            return val;

        if (val > 0.0f && val <= 2.0f && !s.containsIgnoreCase("db"))
            return 20.0f * std::log10(std::max(val, 0.00001f));

        return val;
    }

    static float parseDecentSamplerTimeMs(const juce::String& text, float defaultMs = 5.0f)
    {
        auto s = text.trim();
        if (s.isEmpty()) return defaultMs;

        if (s.endsWithIgnoreCase("ms"))
            return s.dropLastCharacters(2).trim().getFloatValue();
        if (s.endsWithIgnoreCase("s"))
            return s.dropLastCharacters(1).trim().getFloatValue() * 1000.0f;

        float val = s.getFloatValue();
        if (val > 0.0f && val <= 20.0f)
            return val * 1000.0f;

        return val;
    }

    static juce::File resolveDecentSamplerSamplePath(const juce::String& rawPath, const juce::File& sourceFile)
    {
        if (rawPath.isEmpty())
            return {};

        juce::String cleanPath = rawPath.replace("\\", "/").trim();
        if (cleanPath.startsWith("./"))
            cleanPath = cleanPath.substring(2);

        juce::File direct(cleanPath);
        if (direct.existsAsFile())
            return direct;

        juce::File baseDir = sourceFile.isDirectory() ? sourceFile : sourceFile.getParentDirectory();
        if (!baseDir.exists())
            return direct;

        std::vector<juce::File> searchRoots = {
            baseDir,
            baseDir.getParentDirectory(),
            baseDir.getParentDirectory().getParentDirectory()
        };

        for (const auto& root : searchRoots)
        {
            if (root.exists())
            {
                juce::File c1 = root.getChildFile(cleanPath);
                if (c1.existsAsFile()) return c1;

                juce::File c2 = root.getChildFile("Samples").getChildFile(cleanPath);
                if (c2.existsAsFile()) return c2;

                juce::File c3 = root.getChildFile("samples").getChildFile(cleanPath);
                if (c3.existsAsFile()) return c3;

                juce::File c4 = root.getChildFile("Artwork").getChildFile(cleanPath);
                if (c4.existsAsFile()) return c4;

                juce::File c5 = root.getChildFile("Resources").getChildFile(cleanPath);
                if (c5.existsAsFile()) return c5;

                juce::File c6 = root.getChildFile("Images").getChildFile(cleanPath);
                if (c6.existsAsFile()) return c6;

                juce::File c7 = root.getChildFile("Graphics").getChildFile(cleanPath);
                if (c7.existsAsFile()) return c7;

                juce::File c8 = root.getChildFile("ir").getChildFile(cleanPath);
                if (c8.existsAsFile()) return c8;

                juce::File c9 = root.getChildFile("IR").getChildFile(cleanPath);
                if (c9.existsAsFile()) return c9;

                juce::File c10 = root.getChildFile("IRs").getChildFile(cleanPath);
                if (c10.existsAsFile()) return c10;

                juce::File c11 = root.getChildFile("Impulse Responses").getChildFile(cleanPath);
                if (c11.existsAsFile()) return c11;

                juce::File c12 = root.getChildFile("reverb").getChildFile(cleanPath);
                if (c12.existsAsFile()) return c12;
            }
        }

        juce::String filename = juce::File(cleanPath).getFileName();
        for (const auto& root : searchRoots)
        {
            if (root.exists())
            {
                juce::File c1 = root.getChildFile(filename);
                if (c1.existsAsFile()) return c1;

                juce::File c2 = root.getChildFile("Samples").getChildFile(filename);
                if (c2.existsAsFile()) return c2;

                juce::File c3 = root.getChildFile("samples").getChildFile(filename);
                if (c3.existsAsFile()) return c3;

                juce::File c4 = root.getChildFile("Artwork").getChildFile(filename);
                if (c4.existsAsFile()) return c4;

                juce::File c5 = root.getChildFile("Resources").getChildFile(filename);
                if (c5.existsAsFile()) return c5;

                juce::File c6 = root.getChildFile("Images").getChildFile(filename);
                if (c6.existsAsFile()) return c6;

                juce::File c7 = root.getChildFile("ir").getChildFile(filename);
                if (c7.existsAsFile()) return c7;

                juce::File c8 = root.getChildFile("IR").getChildFile(filename);
                if (c8.existsAsFile()) return c8;

                juce::File c9 = root.getChildFile("IRs").getChildFile(filename);
                if (c9.existsAsFile()) return c9;

                juce::File c10 = root.getChildFile("Impulse Responses").getChildFile(filename);
                if (c10.existsAsFile()) return c10;
            }
        }

        juce::Array<juce::File> found;
        baseDir.findChildFiles(found, juce::File::findFiles, true, filename);
        if (!found.isEmpty())
            return found.getFirst();

        if (baseDir.getParentDirectory().exists())
        {
            baseDir.getParentDirectory().findChildFiles(found, juce::File::findFiles, true, filename);
            if (!found.isEmpty())
                return found.getFirst();
        }

        return baseDir.getChildFile(cleanPath);
    }

    static SampleMapState fromDecentSamplerXml(const juce::XmlElement& xml, const juce::File& sourceFile = {})
    {
        SampleMapState s;
        auto baseDir = sourceFile.existsAsFile() ? sourceFile.getParentDirectory() : sourceFile;

        bool hasRandomSeq = false;
        int maxRRIndex = 1;

        struct GroupDefaults
        {
            int groupIndex { 0 };
            juce::String name;
            int seqPosition { 1 };
            juce::String seqMode;
            juce::String trigger { "attack" };
            float gainDb { 0.0f };
            float pan { 0.0f };
            float fineTuneCents { 0.0f };
            float attackMs { 5.0f };
            float decayMs { 100.0f };
            float sustainLevel { 1.0f };
            float releaseMs { 200.0f };
            int rootNote { -1 };
            int keyLow { -1 };
            int keyHigh { -1 };
            int velLow { 0 };
            int velHigh { 127 };
            bool enabled { true };
        };

        int currentGroupCounter = 0;

        auto parseGroupDefaults = [&](const juce::XmlElement& groupElem, int groupIdx) -> GroupDefaults
        {
            GroupDefaults gd;
            gd.groupIndex = groupIdx;
            gd.name = getXmlAttrCaseInsensitive(groupElem, { "name", "label", "title", "id" });
            if (gd.name.isEmpty())
                gd.name = "Group " + juce::String(groupIdx + 1);

            juce::String seqPosStr = getXmlAttrCaseInsensitive(groupElem, { "seqPosition", "seqposition", "seqPos", "roundRobin", "rrIndex" });
            if (seqPosStr.isNotEmpty())
                gd.seqPosition = std::max(1, seqPosStr.getIntValue());

            juce::String seqModeStr = getXmlAttrCaseInsensitive(groupElem, { "seqMode", "seqmode", "sequenceMode", "rrMode" });
            if (seqModeStr.isNotEmpty())
                gd.seqMode = seqModeStr;

            juce::String trigStr = getXmlAttrCaseInsensitive(groupElem, { "trigger", "trig" });
            if (trigStr.isNotEmpty())
                gd.trigger = trigStr;

            juce::String volStr = getXmlAttrCaseInsensitive(groupElem, { "volume", "gain", "amp", "level" });
            if (volStr.isNotEmpty())
                gd.gainDb = parseDecentSamplerVolume(volStr);

            juce::String panStr = getXmlAttrCaseInsensitive(groupElem, { "pan", "panning" });
            if (panStr.isNotEmpty())
                gd.pan = juce::jlimit(-1.0f, 1.0f, panStr.getFloatValue());

            juce::String tuneStr = getXmlAttrCaseInsensitive(groupElem, { "tuning", "tune", "pitch" });
            if (tuneStr.isNotEmpty())
                gd.fineTuneCents += static_cast<float>(tuneStr.getDoubleValue() * 100.0);

            juce::String fineTuneStr = getXmlAttrCaseInsensitive(groupElem, { "fineTune", "finetune", "fine" });
            if (fineTuneStr.isNotEmpty())
                gd.fineTuneCents += static_cast<float>(fineTuneStr.getDoubleValue());

            juce::String attStr = getXmlAttrCaseInsensitive(groupElem, { "attack", "attackMs" });
            if (attStr.isNotEmpty())
                gd.attackMs = parseDecentSamplerTimeMs(attStr, 5.0f);

            juce::String decStr = getXmlAttrCaseInsensitive(groupElem, { "decay", "decayMs" });
            if (decStr.isNotEmpty())
                gd.decayMs = parseDecentSamplerTimeMs(decStr, 100.0f);

            juce::String susStr = getXmlAttrCaseInsensitive(groupElem, { "sustain", "sustainLevel" });
            if (susStr.isNotEmpty())
                gd.sustainLevel = juce::jlimit(0.0f, 1.0f, static_cast<float>(susStr.getDoubleValue()));

            juce::String relStr = getXmlAttrCaseInsensitive(groupElem, { "release", "releaseMs" });
            if (relStr.isNotEmpty())
                gd.releaseMs = parseDecentSamplerTimeMs(relStr, 200.0f);

            for (auto* envChild : groupElem.getChildIterator())
            {
                if (envChild->getTagName().equalsIgnoreCase("envelope") || envChild->getTagName().equalsIgnoreCase("adsr"))
                {
                    juce::String eAtt = getXmlAttrCaseInsensitive(*envChild, { "attack", "attackMs" });
                    if (eAtt.isNotEmpty()) gd.attackMs = parseDecentSamplerTimeMs(eAtt, gd.attackMs);

                    juce::String eDec = getXmlAttrCaseInsensitive(*envChild, { "decay", "decayMs" });
                    if (eDec.isNotEmpty()) gd.decayMs = parseDecentSamplerTimeMs(eDec, gd.decayMs);

                    juce::String eSus = getXmlAttrCaseInsensitive(*envChild, { "sustain", "sustainLevel" });
                    if (eSus.isNotEmpty()) gd.sustainLevel = juce::jlimit(0.0f, 1.0f, static_cast<float>(eSus.getDoubleValue()));

                    juce::String eRel = getXmlAttrCaseInsensitive(*envChild, { "release", "releaseMs" });
                    if (eRel.isNotEmpty()) gd.releaseMs = parseDecentSamplerTimeMs(eRel, gd.releaseMs);
                }
            }

            juce::String rootStr = getXmlAttrCaseInsensitive(groupElem, { "rootNote", "rootnote", "root", "centerNote", "pitchKeycenter" });
            if (rootStr.isNotEmpty())
                gd.rootNote = parseDecentSamplerNote(rootStr, 60);

            juce::String loNoteStr = getXmlAttrCaseInsensitive(groupElem, { "loNote", "lonote", "lowNote", "lownote", "keyLow", "minNote" });
            if (loNoteStr.isNotEmpty())
                gd.keyLow = parseDecentSamplerNote(loNoteStr, 0);

            juce::String hiNoteStr = getXmlAttrCaseInsensitive(groupElem, { "hiNote", "hinote", "highNote", "highnote", "keyHigh", "maxNote" });
            if (hiNoteStr.isNotEmpty())
                gd.keyHigh = parseDecentSamplerNote(hiNoteStr, 127);

            juce::String loVelStr = getXmlAttrCaseInsensitive(groupElem, { "loVel", "lovel", "lowVel", "lowvel", "velLow", "minVelocity", "minVel", "lo_vel", "low_vel" });
            if (loVelStr.isNotEmpty())
                gd.velLow = parseDecentSamplerVelocity(loVelStr, 0);

            juce::String hiVelStr = getXmlAttrCaseInsensitive(groupElem, { "hiVel", "hivel", "highVel", "highvel", "velHigh", "maxVelocity", "maxVel", "hi_vel", "high_vel" });
            if (hiVelStr.isNotEmpty())
                gd.velHigh = parseDecentSamplerVelocity(hiVelStr, 127);

            DecentSamplerGroupState gs;
            gs.index = gd.groupIndex;
            gs.name = gd.name;
            gs.volumeDb = gd.gainDb;
            gs.pan = gd.pan;
            gs.fineTuneCents = gd.fineTuneCents;
            gs.attackMs = gd.attackMs;
            gs.decayMs = gd.decayMs;
            gs.sustainLevel = gd.sustainLevel;
            gs.releaseMs = gd.releaseMs;
            gs.enabled = true;
            gs.muted = false;
            gs.seqPosition = gd.seqPosition;
            gs.seqMode = gd.seqMode;
            gs.trigger = gd.trigger;
            s.groups.push_back(gs);

            return gd;
        };

        auto processSampleElement = [&](const juce::XmlElement& sampleElem, const GroupDefaults& gd)
        {
            juce::String trig = getXmlAttrCaseInsensitive(sampleElem, { "trigger", "trig" });
            if (trig.isEmpty())
                trig = gd.trigger;

            // Filter out continuous noise generators
            if (trig.equalsIgnoreCase("continuous"))
                return;

            SampleMapZoneState z;
            juce::String rawPath = getXmlAttrCaseInsensitive(sampleElem, { "path", "sample", "file" });
            if (rawPath.isEmpty())
                return;

            juce::File resolvedFile = resolveDecentSamplerSamplePath(rawPath, baseDir);
            z.filePath = resolvedFile.getFullPathName();
            z.sampleName = resolvedFile.getFileName();
            z.groupIndex = gd.groupIndex;
            z.pan = gd.pan;
            z.trigger = trig.isNotEmpty() ? trig : "attack";

            juce::String startStr = getXmlAttrCaseInsensitive(sampleElem, { "start", "sampleStart" });
            if (startStr.isNotEmpty()) z.sampleStart = startStr.getLargeIntValue();
            juce::String endStr = getXmlAttrCaseInsensitive(sampleElem, { "end", "sampleEnd" });
            if (endStr.isNotEmpty()) z.sampleEnd = endStr.getLargeIntValue();
            juce::String lStartStr = getXmlAttrCaseInsensitive(sampleElem, { "loopStart", "loop_start" });
            if (lStartStr.isNotEmpty()) z.loopStart = lStartStr.getLargeIntValue();
            juce::String lEndStr = getXmlAttrCaseInsensitive(sampleElem, { "loopEnd", "loop_end" });
            if (lEndStr.isNotEmpty()) z.loopEnd = lEndStr.getLargeIntValue();
            juce::String lEnabledStr = getXmlAttrCaseInsensitive(sampleElem, { "loopEnabled", "loop", "isLooping" });
            if (lEnabledStr.isNotEmpty()) z.loopEnabled = (lEnabledStr.equalsIgnoreCase("true") || lEnabledStr.getIntValue() == 1);

            // Root Note
            juce::String rootStr = getXmlAttrCaseInsensitive(sampleElem, { "rootNote", "rootnote", "root", "centerNote", "pitchKeycenter" });
            if (rootStr.isNotEmpty())
                z.rootNote = parseDecentSamplerNote(rootStr, 60);
            else if (gd.rootNote >= 0)
                z.rootNote = gd.rootNote;
            else
                z.rootNote = 60;

            // Key Low & High
            juce::String loStr = getXmlAttrCaseInsensitive(sampleElem, { "loNote", "lonote", "lowNote", "lownote", "keyLow", "minNote" });
            if (loStr.isNotEmpty())
                z.keyLow = parseDecentSamplerNote(loStr, z.rootNote);
            else if (gd.keyLow >= 0)
                z.keyLow = gd.keyLow;
            else
                z.keyLow = -1; // Flag for auto-span

            juce::String hiStr = getXmlAttrCaseInsensitive(sampleElem, { "hiNote", "hinote", "highNote", "highnote", "keyHigh", "maxNote" });
            if (hiStr.isNotEmpty())
                z.keyHigh = parseDecentSamplerNote(hiStr, z.rootNote);
            else if (gd.keyHigh >= 0)
                z.keyHigh = gd.keyHigh;
            else
                z.keyHigh = -1; // Flag for auto-span

            // Velocity Low & High
            juce::String loVelStr = getXmlAttrCaseInsensitive(sampleElem, { "loVel", "lovel", "lowVel", "lowvel", "velLow", "minVelocity", "minVel", "lo_vel", "low_vel" });
            if (loVelStr.isNotEmpty())
                z.velLow = parseDecentSamplerVelocity(loVelStr, gd.velLow);
            else
                z.velLow = gd.velLow;

            juce::String hiVelStr = getXmlAttrCaseInsensitive(sampleElem, { "hiVel", "hivel", "highVel", "highvel", "velHigh", "maxVelocity", "maxVel", "hi_vel", "high_vel" });
            if (hiVelStr.isNotEmpty())
                z.velHigh = parseDecentSamplerVelocity(hiVelStr, gd.velHigh);
            else
                z.velHigh = gd.velHigh;

            if (z.velLow == 1)
                z.velLow = 0;
            if (z.velHigh > 127)
                z.velHigh = 127;

            // Round Robin
            int seqPos = gd.seqPosition;
            juce::String sPosStr = getXmlAttrCaseInsensitive(sampleElem, { "seqPosition", "seqposition", "seqPos", "roundRobin", "rrIndex" });
            if (sPosStr.isNotEmpty())
                seqPos = std::max(1, sPosStr.getIntValue());
            z.roundRobinIndex = seqPos;
            maxRRIndex = std::max(maxRRIndex, z.roundRobinIndex);

            juce::String sMode = getXmlAttrCaseInsensitive(sampleElem, { "seqMode", "seqmode", "sequenceMode", "rrMode" });
            if (sMode.isEmpty()) sMode = gd.seqMode;
            if (sMode.containsIgnoreCase("random"))
                hasRandomSeq = true;

            // Tuning & Volume
            z.fineTuneCents = gd.fineTuneCents;
            juce::String tuneStr = getXmlAttrCaseInsensitive(sampleElem, { "tuning", "tune", "pitch" });
            if (tuneStr.isNotEmpty())
                z.fineTuneCents += static_cast<float>(tuneStr.getDoubleValue() * 100.0);

            juce::String fineTuneStr = getXmlAttrCaseInsensitive(sampleElem, { "fineTune", "finetune", "fine" });
            if (fineTuneStr.isNotEmpty())
                z.fineTuneCents += static_cast<float>(fineTuneStr.getDoubleValue());

            z.gainDb = gd.gainDb;
            juce::String volStr = getXmlAttrCaseInsensitive(sampleElem, { "volume", "gain", "amp", "level" });
            if (volStr.isNotEmpty())
                z.gainDb = parseDecentSamplerVolume(volStr, gd.gainDb);

            juce::String sPan = getXmlAttrCaseInsensitive(sampleElem, { "pan", "panning" });
            if (sPan.isNotEmpty())
                z.pan = juce::jlimit(-1.0f, 1.0f, sPan.getFloatValue());

            // ADSR
            z.attackMs = gd.attackMs;
            z.decayMs = gd.decayMs;
            z.sustainLevel = gd.sustainLevel;
            z.releaseMs = gd.releaseMs;

            juce::String attStr = getXmlAttrCaseInsensitive(sampleElem, { "attack", "attackMs" });
            if (attStr.isNotEmpty())
                z.attackMs = parseDecentSamplerTimeMs(attStr, gd.attackMs);

            juce::String decStr = getXmlAttrCaseInsensitive(sampleElem, { "decay", "decayMs" });
            if (decStr.isNotEmpty())
                z.decayMs = parseDecentSamplerTimeMs(decStr, gd.decayMs);

            juce::String susStr = getXmlAttrCaseInsensitive(sampleElem, { "sustain", "sustainLevel" });
            if (susStr.isNotEmpty())
                z.sustainLevel = juce::jlimit(0.0f, 1.0f, static_cast<float>(susStr.getDoubleValue()));

            juce::String relStr = getXmlAttrCaseInsensitive(sampleElem, { "release", "releaseMs" });
            if (relStr.isNotEmpty())
                z.releaseMs = parseDecentSamplerTimeMs(relStr, gd.releaseMs);

            for (auto* c : sampleElem.getChildIterator())
            {
                if (c->getTagName().equalsIgnoreCase("envelope") || c->getTagName().equalsIgnoreCase("adsr"))
                {
                    juce::String cAtt = getXmlAttrCaseInsensitive(*c, { "attack", "attackMs" });
                    if (cAtt.isNotEmpty()) z.attackMs = parseDecentSamplerTimeMs(cAtt, z.attackMs);

                    juce::String cDec = getXmlAttrCaseInsensitive(*c, { "decay", "decayMs" });
                    if (cDec.isNotEmpty()) z.decayMs = parseDecentSamplerTimeMs(cDec, z.decayMs);

                    juce::String cSus = getXmlAttrCaseInsensitive(*c, { "sustain", "sustainLevel" });
                    if (cSus.isNotEmpty()) z.sustainLevel = juce::jlimit(0.0f, 1.0f, static_cast<float>(cSus.getDoubleValue()));

                    juce::String cRel = getXmlAttrCaseInsensitive(*c, { "release", "releaseMs" });
                    if (cRel.isNotEmpty()) z.releaseMs = parseDecentSamplerTimeMs(cRel, z.releaseMs);
                }
            }

            s.zones.push_back(z);
        };

        GroupDefaults rootDefaults;

        std::function<void(const juce::XmlElement&, const GroupDefaults&)> traverseXml;
        traverseXml = [&](const juce::XmlElement& element, const GroupDefaults& parentDefaults)
        {
            for (auto* child : element.getChildIterator())
            {
                juce::String tag = child->getTagName();
                if (tag.equalsIgnoreCase("groups"))
                {
                    GroupDefaults groupsDefaults = parentDefaults;
                    juce::String sMode = getXmlAttrCaseInsensitive(*child, { "seqMode", "seqmode" });
                    if (sMode.isNotEmpty())
                        groupsDefaults.seqMode = sMode;
                    traverseXml(*child, groupsDefaults);
                }
                else if (tag.equalsIgnoreCase("group"))
                {
                    int grpIdx = currentGroupCounter++;
                    GroupDefaults groupDefaults = parseGroupDefaults(*child, grpIdx);
                    if (groupDefaults.seqMode.isEmpty())
                        groupDefaults.seqMode = parentDefaults.seqMode;
                    if (groupDefaults.trigger.isEmpty() || groupDefaults.trigger.equalsIgnoreCase("attack"))
                        groupDefaults.trigger = parentDefaults.trigger;

                    for (auto* groupChild : child->getChildIterator())
                    {
                        juce::String gTag = groupChild->getTagName();
                        if (gTag.equalsIgnoreCase("sample") || gTag.equalsIgnoreCase("zone"))
                        {
                            processSampleElement(*groupChild, groupDefaults);
                        }
                        else if (gTag.equalsIgnoreCase("group"))
                        {
                            traverseXml(*groupChild, groupDefaults);
                        }
                        else if (gTag.equalsIgnoreCase("modulators") || gTag.equalsIgnoreCase("modulator") || gTag.equalsIgnoreCase("lfo"))
                        {
                            auto parseMod = [&](const juce::XmlElement& modElem) {
                                DecentSamplerModulatorState m;
                                m.id = getXmlAttrCaseInsensitive(modElem, { "id", "name" });
                                m.type = getXmlAttrCaseInsensitive(modElem, { "type" });
                                if (m.type.isEmpty()) m.type = "lfo";
                                m.shape = getXmlAttrCaseInsensitive(modElem, { "shape", "waveform", "wave" });
                                if (m.shape.isEmpty()) m.shape = "sine";
                                juce::String freqStr = getXmlAttrCaseInsensitive(modElem, { "frequency", "freq", "rate", "speed" });
                                if (freqStr.isNotEmpty()) m.frequency = std::max(0.01f, freqStr.getFloatValue());
                                juce::String amtStr = getXmlAttrCaseInsensitive(modElem, { "modAmount", "amount", "intensity", "depth" });
                                if (amtStr.isNotEmpty()) m.modAmount = amtStr.getFloatValue();
                                m.target = getXmlAttrCaseInsensitive(modElem, { "target", "destination", "param" });
                                if (m.target.isEmpty()) m.target = "pitch";
                                m.scope = "group";
                                m.groupIndex = grpIdx;
                                s.modulators.push_back(m);
                            };

                            if (groupChild->getTagName().equalsIgnoreCase("modulators"))
                            {
                                for (auto* mChild : groupChild->getChildIterator())
                                    parseMod(*mChild);
                            }
                            else
                            {
                                parseMod(*groupChild);
                            }
                        }
                    }
                }
                else if (tag.equalsIgnoreCase("sample") || tag.equalsIgnoreCase("zone"))
                {
                    processSampleElement(*child, parentDefaults);
                }
            }
        };

        traverseXml(xml, rootDefaults);

        // Parse global modulators (<modulators> -> <modulator>)
        for (auto* child : xml.getChildIterator())
        {
            if (child->getTagName().equalsIgnoreCase("modulators"))
            {
                for (auto* mod : child->getChildIterator())
                {
                    if (mod->getTagName().equalsIgnoreCase("modulator") || mod->getTagName().equalsIgnoreCase("lfo"))
                    {
                        DecentSamplerModulatorState m;
                        m.id = getXmlAttrCaseInsensitive(*mod, { "id", "name" });
                        m.type = getXmlAttrCaseInsensitive(*mod, { "type" });
                        if (m.type.isEmpty()) m.type = "lfo";
                        m.shape = getXmlAttrCaseInsensitive(*mod, { "shape", "waveform", "wave" });
                        if (m.shape.isEmpty()) m.shape = "sine";

                        juce::String freqStr = getXmlAttrCaseInsensitive(*mod, { "frequency", "freq", "rate", "speed" });
                        if (freqStr.isNotEmpty()) m.frequency = std::max(0.01f, freqStr.getFloatValue());

                        juce::String amtStr = getXmlAttrCaseInsensitive(*mod, { "modAmount", "amount", "intensity", "depth" });
                        if (amtStr.isNotEmpty()) m.modAmount = amtStr.getFloatValue();

                        m.target = getXmlAttrCaseInsensitive(*mod, { "target", "destination", "param" });
                        if (m.target.isEmpty()) m.target = "pitch";
                        m.scope = getXmlAttrCaseInsensitive(*mod, { "scope" });
                        if (m.scope.isEmpty()) m.scope = "global";

                        s.modulators.push_back(m);
                    }
                }
            }
            else if (child->getTagName().equalsIgnoreCase("midi"))
            {
                for (auto* ccElem : child->getChildIterator())
                {
                    if (ccElem->getTagName().equalsIgnoreCase("cc"))
                    {
                        DecentSamplerMidiCcMapping ccMap;
                        ccMap.ccNumber = ccElem->getIntAttribute("number", 1);
                        for (auto* bElem : ccElem->getChildIterator())
                        {
                            if (bElem->getTagName().equalsIgnoreCase("binding"))
                            {
                                DecentSamplerBinding b;
                                b.type = getXmlAttrCaseInsensitive(*bElem, { "type" });
                                b.level = getXmlAttrCaseInsensitive(*bElem, { "level" });
                                b.position = bElem->getIntAttribute("position", 0);
                                b.parameter = getXmlAttrCaseInsensitive(*bElem, { "parameter", "param" });
                                b.translation = getXmlAttrCaseInsensitive(*bElem, { "translation" });
                                ccMap.bindings.push_back(b);
                            }
                        }
                        s.midiCcMappings.push_back(ccMap);
                    }
                }
            }
        }

        // Check global effects (<effects> -> <effect>)
        for (auto* child : xml.getChildIterator())
        {
            if (child->getTagName().equalsIgnoreCase("effects"))
            {
                for (auto* fx : child->getChildIterator())
                {
                    if (fx->getTagName().equalsIgnoreCase("effect"))
                    {
                        DecentSamplerEffectState eff;
                        eff.type = fx->getStringAttribute("type");
                        eff.path = getXmlAttrCaseInsensitive(*fx, { "path", "file", "irFile", "sample", "ir" });
                        if (eff.path.isNotEmpty() && sourceFile.existsAsFile())
                        {
                            eff.resolvedPath = resolveDecentSamplerSamplePath(eff.path, sourceFile).getFullPathName();
                        }

                        juce::String wetStr = getXmlAttrCaseInsensitive(*fx, { "wetLevel", "wet", "mix", "amount" });
                        if (wetStr.isNotEmpty()) eff.wetLevel = juce::jlimit(0.0f, 1.0f, wetStr.getFloatValue());

                        juce::String dryStr = getXmlAttrCaseInsensitive(*fx, { "dryLevel", "dry" });
                        if (dryStr.isNotEmpty()) eff.dryLevel = juce::jlimit(0.0f, 1.0f, dryStr.getFloatValue());

                        if (eff.type.containsIgnoreCase("convolution") || eff.type.containsIgnoreCase("ir") || eff.path.isNotEmpty())
                        {
                            if (eff.resolvedPath.isNotEmpty())
                            {
                                s.irFilePath = eff.resolvedPath;
                                s.irReverbWetLevel = (eff.wetLevel > 0.001f) ? eff.wetLevel : 0.4f;
                                s.irReverbDryLevel = (eff.dryLevel > 0.001f) ? eff.dryLevel : 1.0f;
                            }
                        }
                        else if (eff.type.containsIgnoreCase("reverb"))
                        {
                            s.samplerReverbAmount = juce::jlimit(0.0f, 1.0f, eff.wetLevel);
                            eff.roomSize = static_cast<float>(fx->getDoubleAttribute("roomSize", 0.5));
                            eff.damping = static_cast<float>(fx->getDoubleAttribute("damping", 0.5));
                        }
                        else if (eff.type.containsIgnoreCase("delay") || eff.type.containsIgnoreCase("echo"))
                        {
                            juce::String dTime = getXmlAttrCaseInsensitive(*fx, { "delayTime", "time", "delayTimeMs" });
                            if (dTime.isNotEmpty()) s.delayTimeMs = parseDecentSamplerTimeMs(dTime, 250.0f);
                            juce::String dFb = getXmlAttrCaseInsensitive(*fx, { "feedback", "fb" });
                            if (dFb.isNotEmpty()) s.delayFeedback = juce::jlimit(0.0f, 0.95f, dFb.getFloatValue());
                            s.delayWetLevel = eff.wetLevel;
                            eff.delayTimeMs = s.delayTimeMs;
                            eff.feedback = s.delayFeedback;
                        }
                        else if (eff.type.containsIgnoreCase("chorus"))
                        {
                            juce::String cRate = getXmlAttrCaseInsensitive(*fx, { "rate", "frequency", "freq" });
                            if (cRate.isNotEmpty()) s.chorusRateHz = std::max(0.1f, cRate.getFloatValue());
                            juce::String cDepth = getXmlAttrCaseInsensitive(*fx, { "depth", "amount" });
                            if (cDepth.isNotEmpty()) s.chorusDepth = juce::jlimit(0.0f, 1.0f, cDepth.getFloatValue());
                            s.chorusWetLevel = eff.wetLevel;
                        }
                        else if (eff.type.containsIgnoreCase("lowpass"))
                        {
                            juce::String fStr = getXmlAttrCaseInsensitive(*fx, { "frequency", "freq", "cutoff" });
                            if (fStr.isNotEmpty())
                            {
                                float f = fStr.getFloatValue();
                                if (f > 0.0f) s.masterFilterCutoffHz = f;
                            }
                            eff.frequency = s.masterFilterCutoffHz;
                        }
                        else if (eff.type.containsIgnoreCase("highpass"))
                        {
                            juce::String fStr = getXmlAttrCaseInsensitive(*fx, { "frequency", "freq", "cutoff" });
                            if (fStr.isNotEmpty())
                            {
                                float f = fStr.getFloatValue();
                                if (f > 0.0f) s.masterHighpassHz = f;
                            }
                            eff.frequency = s.masterHighpassHz;
                        }

                        s.effects.push_back(eff);
                    }
                }
            }
        }

        // Auto-span key ranges if loNote / hiNote were omitted
        if (!s.zones.empty())
        {
            if (s.zones.size() == 1)
            {
                if (s.zones[0].keyLow < 0) s.zones[0].keyLow = 0;
                if (s.zones[0].keyHigh < 0) s.zones[0].keyHigh = 127;
            }
            else
            {
                // Check if any zones have unspecified ranges
                bool needsSpan = false;
                for (const auto& z : s.zones)
                {
                    if (z.keyLow < 0 || z.keyHigh < 0)
                    {
                        needsSpan = true;
                        break;
                    }
                }

                if (needsSpan)
                {
                    // Group by (velLow, velHigh, roundRobinIndex) to calculate midpoints
                    std::vector<int> sortedIndices(s.zones.size());
                    for (size_t i = 0; i < sortedIndices.size(); ++i) sortedIndices[i] = static_cast<int>(i);

                    std::sort(sortedIndices.begin(), sortedIndices.end(), [&](int a, int b) {
                        if (s.zones[a].roundRobinIndex != s.zones[b].roundRobinIndex)
                            return s.zones[a].roundRobinIndex < s.zones[b].roundRobinIndex;
                        if (s.zones[a].velLow != s.zones[b].velLow)
                            return s.zones[a].velLow < s.zones[b].velLow;
                        return s.zones[a].rootNote < s.zones[b].rootNote;
                    });

                    for (size_t k = 0; k < sortedIndices.size(); ++k)
                    {
                        int currIdx = sortedIndices[k];
                        auto& curr = s.zones[currIdx];

                        int prevRoot = -1;
                        if (k > 0)
                        {
                            int prevIdx = sortedIndices[k - 1];
                            if (s.zones[prevIdx].roundRobinIndex == curr.roundRobinIndex &&
                                s.zones[prevIdx].velLow == curr.velLow)
                            {
                                prevRoot = s.zones[prevIdx].rootNote;
                            }
                        }

                        int nextRoot = -1;
                        if (k + 1 < sortedIndices.size())
                        {
                            int nextIdx = sortedIndices[k + 1];
                            if (s.zones[nextIdx].roundRobinIndex == curr.roundRobinIndex &&
                                s.zones[nextIdx].velLow == curr.velLow)
                            {
                                nextRoot = s.zones[nextIdx].rootNote;
                            }
                        }

                        if (curr.keyLow < 0)
                        {
                            if (prevRoot >= 0 && prevRoot < curr.rootNote)
                                curr.keyLow = (prevRoot + curr.rootNote) / 2 + 1;
                            else
                                curr.keyLow = 0;
                        }

                        if (curr.keyHigh < 0)
                        {
                            if (nextRoot >= 0 && nextRoot > curr.rootNote)
                                curr.keyHigh = (curr.rootNote + nextRoot) / 2;
                            else
                                curr.keyHigh = 127;
                        }

                        curr.keyLow = juce::jlimit(0, 127, curr.keyLow);
                        curr.keyHigh = juce::jlimit(curr.keyLow, 127, curr.keyHigh);
                    }
                }
            }
        }

        // Parse UI controls and Custom UI elements from <ui>
        for (auto* child : xml.getChildIterator())
        {
            if (child->getTagName().equalsIgnoreCase("ui"))
            {
                s.customUi.width = child->getIntAttribute("width", 812);
                s.customUi.height = child->getIntAttribute("height", 375);
                s.customUi.bgImagePath = getXmlAttrCaseInsensitive(*child, { "bgImage", "bgimage", "background", "backgroundImage" });
                s.customUi.bgColorHex = getXmlAttrCaseInsensitive(*child, { "bgColor", "bgcolor", "backgroundColor" });
                if (s.customUi.bgImagePath.isNotEmpty() && sourceFile.existsAsFile())
                {
                    s.customUi.resolvedBgImagePath = resolveDecentSamplerSamplePath(s.customUi.bgImagePath, sourceFile).getFullPathName();
                }

                auto parseTabContents = [&](const juce::XmlElement& container, DecentSamplerTabState& tabState)
                {
                    for (auto* uiChild : container.getChildIterator())
                    {
                        juce::String tag = uiChild->getTagName();
                        if (tag.equalsIgnoreCase("labeled-knob") || tag.equalsIgnoreCase("control") ||
                            tag.equalsIgnoreCase("slider") || tag.equalsIgnoreCase("labeled-auto-slider") ||
                            tag.equalsIgnoreCase("knob"))
                        {
                            DecentSamplerUiControl ctrl;
                            ctrl.x = uiChild->getIntAttribute("x", -1);
                            ctrl.y = uiChild->getIntAttribute("y", -1);
                            ctrl.width = uiChild->getIntAttribute("width", 80);
                            ctrl.height = uiChild->getIntAttribute("height", 80);
                            ctrl.id = getXmlAttrCaseInsensitive(*uiChild, { "id", "parameterName", "name" });
                            ctrl.label = getXmlAttrCaseInsensitive(*uiChild, { "label", "name", "title" });
                            if (ctrl.label.isEmpty())
                                ctrl.label = ctrl.id;
                            ctrl.parameterName = getXmlAttrCaseInsensitive(*uiChild, { "parameterName", "id", "name" });
                            ctrl.type = getXmlAttrCaseInsensitive(*uiChild, { "type" });
                            ctrl.style = getXmlAttrCaseInsensitive(*uiChild, { "style" });
                            ctrl.units = getXmlAttrCaseInsensitive(*uiChild, { "units", "unit" });
                            ctrl.textSize = static_cast<float>(uiChild->getDoubleAttribute("textSize", 14.0));
                            ctrl.textColorHex = getXmlAttrCaseInsensitive(*uiChild, { "textColor", "textcolor", "color" });
                            ctrl.trackColorHex = getXmlAttrCaseInsensitive(*uiChild, { "trackForegroundColor", "trackColor", "fgColor" });
                            ctrl.trackBackgroundColorHex = getXmlAttrCaseInsensitive(*uiChild, { "trackBackgroundColor", "trackBgColor", "bgColor" });

                            ctrl.customSkinImagePath = getXmlAttrCaseInsensitive(*uiChild, { "customSkinImage", "customSkinImagePath", "customSkin", "skinImage", "skin", "filmstripImage", "filmstrip", "image", "imagePath", "stripImage", "strip", "bitmap", "src", "path", "file", "knobImage" });
                            
                            // Also check child elements for customSkinImage / skin / image / filmstrip
                            for (auto* c : uiChild->getChildIterator())
                            {
                                juce::String cTag = c->getTagName();
                                if (cTag.equalsIgnoreCase("customSkinImage") || cTag.equalsIgnoreCase("skin") ||
                                    cTag.equalsIgnoreCase("image") || cTag.equalsIgnoreCase("filmstrip") ||
                                    cTag.equalsIgnoreCase("multiFrameImage") || cTag.equalsIgnoreCase("bitmap") ||
                                    cTag.equalsIgnoreCase("knobImage") || cTag.equalsIgnoreCase("frame"))
                                {
                                    if (ctrl.customSkinImagePath.isEmpty())
                                        ctrl.customSkinImagePath = getXmlAttrCaseInsensitive(*c, { "path", "src", "file", "image", "customSkinImage", "name", "filename", "url" });
                                    if (ctrl.customSkinNumFrames <= 0)
                                    {
                                        juce::String childFrames = getXmlAttrCaseInsensitive(*c, { "numFrames", "frames", "customSkinNumFrames", "frameCount", "totalFrames" });
                                        if (childFrames.isNotEmpty()) ctrl.customSkinNumFrames = childFrames.getIntValue();
                                    }
                                }
                            }

                            if (ctrl.customSkinImagePath.isNotEmpty() && sourceFile.exists())
                            {
                                ctrl.resolvedCustomSkinImagePath = resolveDecentSamplerSamplePath(ctrl.customSkinImagePath, sourceFile).getFullPathName();
                            }

                            juce::String framesStr = getXmlAttrCaseInsensitive(*uiChild, { "customSkinNumFrames", "numFrames", "frames", "frameCount", "totalFrames" });
                            if (framesStr.isNotEmpty() && ctrl.customSkinNumFrames <= 0)
                                ctrl.customSkinNumFrames = framesStr.getIntValue();

                            juce::String minStr = getXmlAttrCaseInsensitive(*uiChild, { "minValue", "min", "minimum" });
                            ctrl.minValue = minStr.isNotEmpty() ? minStr.getDoubleValue() : 0.0;

                            juce::String maxStr = getXmlAttrCaseInsensitive(*uiChild, { "maxValue", "max", "maximum" });
                            ctrl.maxValue = maxStr.isNotEmpty() ? maxStr.getDoubleValue() : 1.0;

                            juce::String valStr = getXmlAttrCaseInsensitive(*uiChild, { "value", "defaultValue", "default" });
                            ctrl.defaultValue = valStr.isNotEmpty() ? valStr.getDoubleValue() : ctrl.minValue;
                            ctrl.currentValue = ctrl.defaultValue;

                            for (auto* b : uiChild->getChildIterator())
                            {
                                if (b->getTagName().equalsIgnoreCase("binding"))
                                {
                                    DecentSamplerBinding binding;
                                    binding.type = getXmlAttrCaseInsensitive(*b, { "type", "target" });
                                    binding.level = getXmlAttrCaseInsensitive(*b, { "level" });
                                    binding.position = b->getIntAttribute("position", 0);
                                    binding.parameter = getXmlAttrCaseInsensitive(*b, { "parameter", "param", "property" });
                                    binding.translation = getXmlAttrCaseInsensitive(*b, { "translation" });

                                    ctrl.bindings.push_back(binding);
                                    if (ctrl.bindingType.isEmpty()) ctrl.bindingType = binding.type;
                                    if (ctrl.bindingParam.isEmpty()) ctrl.bindingParam = binding.parameter;
                                }
                            }

                            if (ctrl.id.isEmpty())
                            {
                                if (ctrl.bindingParam.isNotEmpty()) ctrl.id = ctrl.bindingParam.toLowerCase();
                                else ctrl.id = "ctrl_" + juce::String(tabState.controls.size() + 1);
                            }
                            if (ctrl.label.isEmpty() && ctrl.customSkinImagePath.isEmpty())
                            {
                                ctrl.label = ctrl.id;
                            }

                            tabState.controls.push_back(ctrl);
                            s.uiControls.push_back(ctrl);
                        }
                        else if (tag.equalsIgnoreCase("label"))
                        {
                            DecentSamplerUiLabel lbl;
                            lbl.x = uiChild->getIntAttribute("x", 0);
                            lbl.y = uiChild->getIntAttribute("y", 0);
                            lbl.width = uiChild->getIntAttribute("width", 120);
                            lbl.height = uiChild->getIntAttribute("height", 30);
                            lbl.text = getXmlAttrCaseInsensitive(*uiChild, { "text", "value", "label" });
                            lbl.textSize = static_cast<float>(uiChild->getDoubleAttribute("textSize", 14.0));
                            lbl.textColorHex = getXmlAttrCaseInsensitive(*uiChild, { "textColor", "textcolor", "color" });
                            lbl.textAlignment = getXmlAttrCaseInsensitive(*uiChild, { "textAlignment", "align", "justification" });
                            if (lbl.textAlignment.isEmpty()) lbl.textAlignment = "center";
                            tabState.labels.push_back(lbl);
                        }
                        else if (tag.equalsIgnoreCase("image"))
                        {
                            DecentSamplerUiImage img;
                            img.x = uiChild->getIntAttribute("x", 0);
                            img.y = uiChild->getIntAttribute("y", 0);
                            img.width = uiChild->getIntAttribute("width", 100);
                            img.height = uiChild->getIntAttribute("height", 100);
                            img.path = getXmlAttrCaseInsensitive(*uiChild, { "path", "src", "file" });
                            if (img.path.isNotEmpty() && sourceFile.existsAsFile())
                            {
                                img.resolvedFilePath = resolveDecentSamplerSamplePath(img.path, sourceFile).getFullPathName();
                            }
                            tabState.images.push_back(img);
                        }
                        else if (tag.equalsIgnoreCase("button"))
                        {
                            DecentSamplerUiButton btn;
                            btn.x = uiChild->getIntAttribute("x", 0);
                            btn.y = uiChild->getIntAttribute("y", 0);
                            btn.width = uiChild->getIntAttribute("width", 80);
                            btn.height = uiChild->getIntAttribute("height", 30);
                            btn.text = getXmlAttrCaseInsensitive(*uiChild, { "text", "label" });
                            btn.style = getXmlAttrCaseInsensitive(*uiChild, { "style", "type" });
                            btn.state = uiChild->getBoolAttribute("value", false);

                            for (auto* b : uiChild->getChildIterator())
                            {
                                if (b->getTagName().equalsIgnoreCase("binding"))
                                {
                                    DecentSamplerBinding binding;
                                    binding.type = getXmlAttrCaseInsensitive(*b, { "type", "target" });
                                    binding.level = getXmlAttrCaseInsensitive(*b, { "level" });
                                    binding.position = b->getIntAttribute("position", 0);
                                    binding.parameter = getXmlAttrCaseInsensitive(*b, { "parameter", "param", "property" });
                                    binding.translation = getXmlAttrCaseInsensitive(*b, { "translation" });
                                    btn.bindings.push_back(binding);
                                }
                            }
                            tabState.buttons.push_back(btn);
                        }
                        else if (tag.equalsIgnoreCase("menu"))
                        {
                            DecentSamplerUiMenu menu;
                            menu.x = uiChild->getIntAttribute("x", 0);
                            menu.y = uiChild->getIntAttribute("y", 0);
                            menu.width = uiChild->getIntAttribute("width", 120);
                            menu.height = uiChild->getIntAttribute("height", 30);
                            menu.selectedIndex = uiChild->getIntAttribute("value", 0);

                            for (auto* mChild : uiChild->getChildIterator())
                            {
                                if (mChild->getTagName().equalsIgnoreCase("option"))
                                {
                                    menu.options.add(getXmlAttrCaseInsensitive(*mChild, { "name", "text", "value" }));
                                }
                                else if (mChild->getTagName().equalsIgnoreCase("binding"))
                                {
                                    DecentSamplerBinding binding;
                                    binding.type = getXmlAttrCaseInsensitive(*mChild, { "type", "target" });
                                    binding.level = getXmlAttrCaseInsensitive(*mChild, { "level" });
                                    binding.position = mChild->getIntAttribute("position", 0);
                                    binding.parameter = getXmlAttrCaseInsensitive(*mChild, { "parameter", "param", "property" });
                                    binding.translation = getXmlAttrCaseInsensitive(*mChild, { "translation" });
                                    menu.bindings.push_back(binding);
                                }
                            }
                            tabState.menus.push_back(menu);
                        }
                    }
                };

                bool hasTabs = false;
                for (auto* uiChild : child->getChildIterator())
                {
                    if (uiChild->getTagName().equalsIgnoreCase("tab"))
                    {
                        hasTabs = true;
                        DecentSamplerTabState tab;
                        tab.name = getXmlAttrCaseInsensitive(*uiChild, { "name", "title", "label" });
                        if (tab.name.isEmpty()) tab.name = "Tab " + juce::String(s.customUi.tabs.size() + 1);
                        parseTabContents(*uiChild, tab);
                        s.customUi.tabs.push_back(tab);
                    }
                }

                if (!hasTabs)
                {
                    DecentSamplerTabState mainTab;
                    mainTab.name = "Main";
                    parseTabContents(*child, mainTab);
                    if (!mainTab.controls.empty() || !mainTab.labels.empty() || !mainTab.images.empty() || !mainTab.buttons.empty() || !mainTab.menus.empty())
                    {
                        s.customUi.tabs.push_back(mainTab);
                    }
                }
            }
        }

        if (sourceFile.existsAsFile())
            s.instrumentName = sourceFile.getFileNameWithoutExtension();

        if (hasRandomSeq)
            s.roundRobinMode = 1;
        else if (maxRRIndex > 1)
            s.roundRobinMode = 0;
        else
            s.roundRobinMode = 0;

        if (!s.zones.empty())
        {
            s.globalAttackMs = s.zones[0].attackMs;
            s.globalDecayMs = s.zones[0].decayMs;
            s.globalSustainLevel = s.zones[0].sustainLevel;
            s.globalReleaseMs = s.zones[0].releaseMs;
        }

        s.pitchTrackingEnabled = true;
        return s;
    }

    static SampleMapState fromXml(const juce::XmlElement& xml, const juce::File& sourceFile = {})
    {
        if (xml.hasTagName("DecentSampler") || xml.hasTagName("decentSampler") || xml.hasTagName("Decentsampler"))
        {
            return fromDecentSamplerXml(xml, sourceFile);
        }

        SampleMapState s;
        if (!xml.hasTagName("SampleMap") && !xml.hasTagName("samplemap") && !xml.hasTagName("OpenWavSampleMap"))
            return s;

        s.globalAttackMs = static_cast<float>(xml.getDoubleAttribute("globalAttackMs", 5.0));
        s.globalDecayMs = static_cast<float>(xml.getDoubleAttribute("decayMs", 100.0));
        s.globalSustainLevel = static_cast<float>(xml.getDoubleAttribute("globalSustainLevel", 1.0));
        s.globalReleaseMs = static_cast<float>(xml.getDoubleAttribute("globalReleaseMs", 200.0));
        s.samplerReverbAmount = static_cast<float>(xml.getDoubleAttribute("samplerReverbAmount", 0.0));
        s.pitchTrackingEnabled = xml.getBoolAttribute("pitchTrackingEnabled", true);
        s.roundRobinMode = xml.getIntAttribute("roundRobinMode", 0);

        auto baseDir = sourceFile.getParentDirectory();
        for (auto* child : xml.getChildIterator())
        {
            if (child->hasTagName("Zone") || child->hasTagName("zone"))
            {
                s.zones.push_back(SampleMapZoneState::fromXml(*child, baseDir));
            }
        }
        return s;
    }

    bool saveToFile(const juce::File& file) const
    {
        auto xml = toXml(file);
        if (xml == nullptr) return false;
        return xml->writeTo(file);
    }

    static SampleMapState loadFromFile(const juce::File& file)
    {
        if (!file.existsAsFile()) return {};
        auto xml = juce::XmlDocument::parse(file);
        if (xml == nullptr) return {};
        return fromXml(*xml, file);
    }
};

struct PluginFullState
{
    int version { 2 };
    int currentViewMode { 0 };
    juce::String currentLoadedFilePath;
    std::vector<double> transportSliceRatios;
    juce::String searchText;
    std::vector<juce::String> selectedTags;
    int tagPanelWidth { 220 };

    EditComponentState edit;
    SampleMapState sampleMap;

    juce::var toVar() const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("version", version);
        obj->setProperty("currentViewMode", currentViewMode);
        obj->setProperty("currentLoadedFilePath", currentLoadedFilePath);

        juce::Array<juce::var> sliceArray;
        for (double r : transportSliceRatios)
            sliceArray.add(r);
        obj->setProperty("transportSliceRatios", sliceArray);

        obj->setProperty("searchText", searchText);

        juce::Array<juce::var> tagArray;
        for (const auto& t : selectedTags)
            tagArray.add(t);
        obj->setProperty("selectedTags", tagArray);

        obj->setProperty("tagPanelWidth", tagPanelWidth);

        obj->setProperty("edit", edit.toVar());
        obj->setProperty("sampleMap", sampleMap.toVar());
        return juce::var(obj);
    }

    static PluginFullState fromVar(const juce::var& v)
    {
        PluginFullState s;
        if (!v.isObject()) return s;
        auto* obj = v.getDynamicObject();
        if (!obj) return s;

        s.version = static_cast<int>(obj->getProperty("version"));
        if (obj->hasProperty("currentViewMode"))
            s.currentViewMode = static_cast<int>(obj->getProperty("currentViewMode"));
        if (obj->hasProperty("currentLoadedFilePath"))
            s.currentLoadedFilePath = obj->getProperty("currentLoadedFilePath").toString();

        if (obj->hasProperty("transportSliceRatios") && obj->getProperty("transportSliceRatios").isArray())
        {
            for (const auto& r : *obj->getProperty("transportSliceRatios").getArray())
                s.transportSliceRatios.push_back(static_cast<double>(r));
        }

        if (obj->hasProperty("searchText"))
            s.searchText = obj->getProperty("searchText").toString();

        if (obj->hasProperty("selectedTags") && obj->getProperty("selectedTags").isArray())
        {
            for (const auto& t : *obj->getProperty("selectedTags").getArray())
                s.selectedTags.push_back(t.toString());
        }

        if (obj->hasProperty("tagPanelWidth"))
            s.tagPanelWidth = static_cast<int>(obj->getProperty("tagPanelWidth"));

        if (obj->hasProperty("edit")) s.edit = EditComponentState::fromVar(obj->getProperty("edit"));
        if (obj->hasProperty("sampleMap")) s.sampleMap = SampleMapState::fromVar(obj->getProperty("sampleMap"));
        return s;
    }
};

} // namespace openwav
