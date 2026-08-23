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
    float fineTuneCents { 0.0f };
    float gainDb { 0.0f };
    float attackMs { 5.0f };
    float decayMs { 100.0f };
    float sustainLevel { 1.0f };
    float releaseMs { 200.0f };

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
        obj->setProperty("fineTuneCents", fineTuneCents);
        obj->setProperty("gainDb", gainDb);
        obj->setProperty("attackMs", attackMs);
        obj->setProperty("decayMs", decayMs);
        obj->setProperty("sustainLevel", sustainLevel);
        obj->setProperty("releaseMs", releaseMs);
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
        z.fineTuneCents = static_cast<float>(obj->getProperty("fineTuneCents"));
        z.gainDb = static_cast<float>(obj->getProperty("gainDb"));
        z.attackMs = static_cast<float>(obj->getProperty("attackMs"));
        z.decayMs = static_cast<float>(obj->getProperty("decayMs"));
        z.sustainLevel = static_cast<float>(obj->getProperty("sustainLevel"));
        z.releaseMs = static_cast<float>(obj->getProperty("releaseMs"));
        return z;
    }
};

struct SampleMapState
{
    std::vector<SampleMapZoneState> zones;
    float globalAttackMs { 5.0f };
    float globalDecayMs { 100.0f };
    float globalSustainLevel { 1.0f };
    float globalReleaseMs { 200.0f };
    float samplerReverbAmount { 0.0f };
    bool pitchTrackingEnabled { true };

    juce::var toVar() const
    {
        auto* obj = new juce::DynamicObject();
        juce::Array<juce::var> zoneArray;
        for (const auto& z : zones)
            zoneArray.add(z.toVar());
        obj->setProperty("zones", zoneArray);
        obj->setProperty("globalAttackMs", globalAttackMs);
        obj->setProperty("globalDecayMs", globalDecayMs);
        obj->setProperty("globalSustainLevel", globalSustainLevel);
        obj->setProperty("globalReleaseMs", globalReleaseMs);
        obj->setProperty("samplerReverbAmount", samplerReverbAmount);
        obj->setProperty("pitchTrackingEnabled", pitchTrackingEnabled);
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
        if (obj->hasProperty("globalAttackMs")) s.globalAttackMs = static_cast<float>(obj->getProperty("globalAttackMs"));
        if (obj->hasProperty("globalDecayMs")) s.globalDecayMs = static_cast<float>(obj->getProperty("globalDecayMs"));
        if (obj->hasProperty("globalSustainLevel")) s.globalSustainLevel = static_cast<float>(obj->getProperty("globalSustainLevel"));
        if (obj->hasProperty("globalReleaseMs")) s.globalReleaseMs = static_cast<float>(obj->getProperty("globalReleaseMs"));
        if (obj->hasProperty("samplerReverbAmount")) s.samplerReverbAmount = static_cast<float>(obj->getProperty("samplerReverbAmount"));
        if (obj->hasProperty("pitchTrackingEnabled")) s.pitchTrackingEnabled = static_cast<bool>(obj->getProperty("pitchTrackingEnabled"));
        return s;
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
