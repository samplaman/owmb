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

struct PerformancePadState
{
    int id { 0 };
    juce::String filePath;
    juce::String sampleName;
    juce::uint32 colorRgba { 0xff00c8dc };
    float pitchSemi { 0.0f };
    float gainDb { 0.0f };
    bool isLooping { false };
    bool isOneShot { true };

    juce::var toVar() const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("id", id);
        obj->setProperty("filePath", filePath);
        obj->setProperty("sampleName", sampleName);
        obj->setProperty("colorRgba", static_cast<juce::int64>(colorRgba));
        obj->setProperty("pitchSemi", pitchSemi);
        obj->setProperty("gainDb", gainDb);
        obj->setProperty("isLooping", isLooping);
        obj->setProperty("isOneShot", isOneShot);
        return juce::var(obj);
    }

    static PerformancePadState fromVar(const juce::var& v)
    {
        PerformancePadState p;
        if (!v.isObject()) return p;
        auto* obj = v.getDynamicObject();
        if (!obj) return p;
        p.id = static_cast<int>(obj->getProperty("id"));
        p.filePath = obj->getProperty("filePath").toString();
        p.sampleName = obj->getProperty("sampleName").toString();
        if (obj->hasProperty("colorRgba"))
            p.colorRgba = static_cast<juce::uint32>(static_cast<juce::int64>(obj->getProperty("colorRgba")));
        p.pitchSemi = static_cast<float>(obj->getProperty("pitchSemi"));
        p.gainDb = static_cast<float>(obj->getProperty("gainDb"));
        p.isLooping = static_cast<bool>(obj->getProperty("isLooping"));
        p.isOneShot = obj->hasProperty("isOneShot") ? static_cast<bool>(obj->getProperty("isOneShot")) : true;
        return p;
    }
};

struct PerformanceState
{
    std::array<PerformancePadState, 64> pads;
    float attack { 0.005f };
    float decay { 0.1f };
    float sustain { 1.0f };
    float release { 0.15f };
    bool pitchTrack { true };
    bool oneShot { false };
    bool loop { false };
    int currentBank { 0 };
    std::array<float, 8> colVolumeDb {};
    std::array<float, 8> colPitchSemi {};
    float masterVolumeDb { 0.0f };
    juce::String currentPresetName;

    PerformanceState()
    {
        for (int i = 0; i < 64; ++i)
        {
            pads[i].id = i;
        }
        colVolumeDb.fill(0.0f);
        colPitchSemi.fill(0.0f);
    }

    juce::var toVar() const
    {
        auto* obj = new juce::DynamicObject();
        juce::Array<juce::var> padArray;
        for (int i = 0; i < 64; ++i)
        {
            if (pads[i].filePath.isNotEmpty())
            {
                padArray.add(pads[i].toVar());
            }
        }
        obj->setProperty("pads", padArray);
        obj->setProperty("attack", attack);
        obj->setProperty("decay", decay);
        obj->setProperty("sustain", sustain);
        obj->setProperty("release", release);
        obj->setProperty("pitchTrack", pitchTrack);
        obj->setProperty("oneShot", oneShot);
        obj->setProperty("loop", loop);
        obj->setProperty("currentBank", currentBank);
        obj->setProperty("masterVolumeDb", masterVolumeDb);
        obj->setProperty("currentPresetName", currentPresetName);

        juce::Array<juce::var> colVolArray;
        for (float v : colVolumeDb) colVolArray.add(v);
        obj->setProperty("colVolumeDb", colVolArray);

        juce::Array<juce::var> colPitchArray;
        for (float p : colPitchSemi) colPitchArray.add(p);
        obj->setProperty("colPitchSemi", colPitchArray);

        return juce::var(obj);
    }

    static PerformanceState fromVar(const juce::var& v)
    {
        PerformanceState s;
        if (!v.isObject()) return s;
        auto* obj = v.getDynamicObject();
        if (!obj) return s;

        if (obj->hasProperty("pads") && obj->getProperty("pads").isArray())
        {
            for (const auto& pv : *obj->getProperty("pads").getArray())
            {
                auto pad = PerformancePadState::fromVar(pv);
                if (pad.id >= 0 && pad.id < 64)
                    s.pads[pad.id] = pad;
            }
        }

        if (obj->hasProperty("attack")) s.attack = static_cast<float>(obj->getProperty("attack"));
        if (obj->hasProperty("decay")) s.decay = static_cast<float>(obj->getProperty("decay"));
        if (obj->hasProperty("sustain")) s.sustain = static_cast<float>(obj->getProperty("sustain"));
        if (obj->hasProperty("release")) s.release = static_cast<float>(obj->getProperty("release"));
        if (obj->hasProperty("pitchTrack")) s.pitchTrack = static_cast<bool>(obj->getProperty("pitchTrack"));
        if (obj->hasProperty("oneShot")) s.oneShot = static_cast<bool>(obj->getProperty("oneShot"));
        if (obj->hasProperty("loop")) s.loop = static_cast<bool>(obj->getProperty("loop"));
        if (obj->hasProperty("currentBank")) s.currentBank = static_cast<int>(obj->getProperty("currentBank"));
        if (obj->hasProperty("masterVolumeDb")) s.masterVolumeDb = static_cast<float>(obj->getProperty("masterVolumeDb"));
        if (obj->hasProperty("currentPresetName")) s.currentPresetName = obj->getProperty("currentPresetName").toString();

        if (obj->hasProperty("colVolumeDb") && obj->getProperty("colVolumeDb").isArray())
        {
            const auto& arr = *obj->getProperty("colVolumeDb").getArray();
            for (int i = 0; i < std::min<int>(8, arr.size()); ++i)
                s.colVolumeDb[i] = static_cast<float>(arr[i]);
        }
        if (obj->hasProperty("colPitchSemi") && obj->getProperty("colPitchSemi").isArray())
        {
            const auto& arr = *obj->getProperty("colPitchSemi").getArray();
            for (int i = 0; i < std::min<int>(8, arr.size()); ++i)
                s.colPitchSemi[i] = static_cast<float>(arr[i]);
        }
        return s;
    }
};

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
        return s;
    }
};

struct PluginFullState
{
    int version { 1 };
    PerformanceState performance;
    EditComponentState edit;
    SampleMapState sampleMap;

    juce::var toVar() const
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("version", version);
        obj->setProperty("performance", performance.toVar());
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
        if (obj->hasProperty("performance")) s.performance = PerformanceState::fromVar(obj->getProperty("performance"));
        if (obj->hasProperty("edit")) s.edit = EditComponentState::fromVar(obj->getProperty("edit"));
        if (obj->hasProperty("sampleMap")) s.sampleMap = SampleMapState::fromVar(obj->getProperty("sampleMap"));
        return s;
    }
};

} // namespace openwav
