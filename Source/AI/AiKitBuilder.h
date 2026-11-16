#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_core/juce_core.h>
#endif

#include "../Database/TagDatabaseManager.h"
#include "../Models/PluginState.h"
#include "../Audio/PerformanceRackDSP.h"
#include "AudioEmbeddingEngine.h"
#include <vector>
#include <map>

namespace openwav
{

struct AiEffectSpec
{
    PerformanceEffectType type { PerformanceEffectType::Reverb };
    std::map<juce::String, float> params;
};

struct AiKitBuildResult
{
    bool success { false };
    bool usedCloudLlm { false };
    juce::String summary;
    SampleMapState sampleMap;
    std::vector<AiEffectSpec> effects;
};

class AiKitBuilder
{
public:
    static AiKitBuildResult buildFromPrompt(const juce::String& prompt,
                                            const TagDatabaseManager& db,
                                            const juce::String& geminiApiKey = {});

private:
    static AiKitBuildResult buildLocalSemantic(const juce::String& prompt,
                                               const std::vector<MediaItem>& items);
    static bool tryBuildWithGemini(const juce::String& prompt,
                                   const std::vector<MediaItem>& items,
                                   const juce::String& apiKey,
                                   AiKitBuildResult& outResult);
    static std::vector<AiEffectSpec> inferEffectsFromPrompt(const juce::String& prompt);
};

} // namespace openwav
