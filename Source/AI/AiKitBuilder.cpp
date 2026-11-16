#include "AiKitBuilder.h"
#include <algorithm>
#include <set>

namespace openwav
{

namespace
{
    const MediaItem* pickBestSemanticMatch(const std::vector<MediaItem>& items,
                                           const juce::String& query,
                                           std::set<juce::String>& usedPaths,
                                           float minSim = 0.05f)
    {
        auto qVec = AudioEmbeddingEngine::encodeTextQuery(query);
        const MediaItem* best = nullptr;
        float bestScore = minSim;

        for (const auto& item : items)
        {
            if (item.filePath.isEmpty() || usedPaths.count(item.filePath) > 0)
                continue;
            if (!juce::File(item.filePath).existsAsFile())
                continue;

            auto itemVec = (item.embedding.size() == AudioEmbeddingEngine::EmbeddingDim)
                ? item.embedding
                : std::vector<float>(AudioEmbeddingEngine::computeEmbedding(item).begin(),
                                     AudioEmbeddingEngine::computeEmbedding(item).end());

            float sim = AudioEmbeddingEngine::cosineSimilarity(itemVec, qVec);
            if (item.isFavorite) sim += 0.05f;
            if (sim > bestScore)
            {
                bestScore = sim;
                best = &item;
            }
        }

        if (best != nullptr)
            usedPaths.insert(best->filePath);
        return best;
    }

    SampleMapZoneState makeZoneFromItem(const MediaItem& item, int rootNote, int keyLow, int keyHigh,
                                        float atkMs = 5.0f, float decMs = 120.0f, float sus = 1.0f, float relMs = 200.0f)
    {
        SampleMapZoneState z;
        z.filePath = item.filePath;
        z.sampleName = item.fileName;
        z.rootNote = rootNote;
        z.keyLow = keyLow;
        z.keyHigh = keyHigh;
        z.velLow = 0;
        z.velHigh = 127;
        z.roundRobinIndex = 1;
        z.attackMs = atkMs;
        z.decayMs = decMs;
        z.sustainLevel = sus;
        z.releaseMs = relMs;
        return z;
    }
}

std::vector<AiEffectSpec> AiKitBuilder::inferEffectsFromPrompt(const juce::String& prompt)
{
    juce::String p = prompt.toLowerCase();
    std::vector<AiEffectSpec> fx;

    if (p.contains("warm") || p.contains("tape") || p.contains("saturat") || p.contains("gritty") || p.contains("heavy") || p.contains("distort") || p.contains("808"))
    {
        AiEffectSpec sat;
        sat.type = PerformanceEffectType::Distortion;
        sat.params["drive"] = p.contains("heavy") || p.contains("distort") ? 0.55f : 0.28f;
        sat.params["mix"] = p.contains("subtle") ? 0.30f : 0.45f;
        fx.push_back(sat);
    }

    if (p.contains("lofi") || p.contains("lo-fi") || p.contains("vinyl") || p.contains("dusty") || p.contains("retro"))
    {
        AiEffectSpec crush;
        crush.type = PerformanceEffectType::Bitcrusher;
        crush.params["bits"] = 12.0f;
        crush.params["downsample"] = 2.0f;
        crush.params["mix"] = 0.35f;
        fx.push_back(crush);

        AiEffectSpec filt;
        filt.type = PerformanceEffectType::Filter;
        filt.params["cutoff"] = 4800.0f;
        filt.params["resonance"] = 0.25f;
        fx.push_back(filt);
    }

    if (p.contains("punch") || p.contains("tight") || p.contains("compress") || p.contains("boom") || p.contains("drum") || p.contains("kit"))
    {
        AiEffectSpec comp;
        comp.type = PerformanceEffectType::Compressor;
        comp.params["threshold"] = -14.0f;
        comp.params["ratio"] = 4.0f;
        comp.params["mix"] = 0.65f;
        fx.push_back(comp);
    }

    if (p.contains("chorus") || p.contains("wide") || p.contains("lush") || p.contains("dream") || p.contains("80s"))
    {
        AiEffectSpec ch;
        ch.type = PerformanceEffectType::Chorus;
        ch.params["rate"] = 0.8f;
        ch.params["depth"] = 0.45f;
        ch.params["mix"] = 0.40f;
        fx.push_back(ch);
    }

    if (p.contains("delay") || p.contains("echo") || p.contains("dub") || p.contains("space"))
    {
        AiEffectSpec dly;
        dly.type = PerformanceEffectType::Delay;
        dly.params["time"] = 360.0f;
        dly.params["feedback"] = 0.35f;
        dly.params["mix"] = 0.28f;
        fx.push_back(dly);
    }

    if (p.contains("reverb") || p.contains("ambient") || p.contains("lush") || p.contains("hall") || p.contains("chill") || p.contains("pad"))
    {
        AiEffectSpec rev;
        rev.type = PerformanceEffectType::Reverb;
        rev.params["size"] = (p.contains("hall") || p.contains("ambient") || p.contains("lush")) ? 0.78f : 0.45f;
        rev.params["damping"] = 0.45f;
        rev.params["mix"] = p.contains("ambient") ? 0.45f : 0.25f;
        fx.push_back(rev);
    }

    return fx;
}

AiKitBuildResult AiKitBuilder::buildFromPrompt(const juce::String& prompt,
                                               const TagDatabaseManager& db,
                                               const juce::String& geminiApiKey)
{
    auto items = db.getAllItems();
    if (items.empty())
    {
        AiKitBuildResult res;
        res.success = false;
        res.summary = "Your sample library is empty. Scan a folder first so the AI Kit Builder can select samples.";
        return res;
    }

    if (geminiApiKey.trim().isNotEmpty())
    {
        AiKitBuildResult cloudRes;
        if (tryBuildWithGemini(prompt, items, geminiApiKey.trim(), cloudRes))
            return cloudRes;
    }

    return buildLocalSemantic(prompt, items);
}

AiKitBuildResult AiKitBuilder::buildLocalSemantic(const juce::String& prompt,
                                                  const std::vector<MediaItem>& items)
{
    AiKitBuildResult res;
    juce::String p = prompt.toLowerCase();

    const bool isMelodicInstrument = (p.contains("pad") || p.contains("lead") || p.contains("keys") || p.contains("piano")
                                      || p.contains("synth") || p.contains("pluck") || p.contains("bass instrument")
                                      || p.contains("ambient")) && !p.contains("kit") && !p.contains("drum");

    std::set<juce::String> usedPaths;
    res.sampleMap.midiChannel = 0; // Omni
    res.sampleMap.roundRobinMode = 0;

    if (isMelodicInstrument)
    {
        res.sampleMap.pitchTrackingEnabled = true;
        const bool isPad = p.contains("pad") || p.contains("ambient") || p.contains("lush") || p.contains("slow");
        float atk = isPad ? 280.0f : 5.0f;
        float dec = isPad ? 400.0f : 180.0f;
        float sus = isPad ? 0.90f : 0.75f;
        float rel = isPad ? 950.0f : 280.0f;

        if (const auto* primary = pickBestSemanticMatch(items, prompt, usedPaths, -0.5f))
        {
            res.sampleMap.zones.push_back(makeZoneFromItem(*primary, 60, 36, 84, atk, dec, sus, rel));
            res.summary = "Built melodic instrument with '" + primary->fileName + "' mapped across C2–C6.";
        }
    }
    else
    {
        // Drum / Beat Kit mapping (GM-style layout starting at C1 = 36)
        res.sampleMap.pitchTrackingEnabled = false;
        struct SlotSpec { const char* label; const char* query; int note; };
        const SlotSpec slots[] = {
            { "Kick",  "kick bassdrum",        36 },
            { "Rim",   "rim perc",             37 },
            { "Snare", "snare",                38 },
            { "Clap",  "clap snap",            39 },
            { "CHat",  "hihat closed hat",     42 },
            { "Perc",  "perc shaker",          43 },
            { "OHat",  "hihat open hat",       46 },
            { "Crash", "crash cymbal fx",      49 }
        };

        juce::String vibeMod;
        if (p.contains("lofi") || p.contains("lo-fi")) vibeMod = " lofi warm dusty";
        else if (p.contains("trap") || p.contains("808")) vibeMod = " trap punchy 808";
        else if (p.contains("house") || p.contains("techno")) vibeMod = " house tight";
        else if (p.contains("boom")) vibeMod = " boom bap vintage";

        int count = 0;
        for (const auto& slot : slots)
        {
            if (const auto* match = pickBestSemanticMatch(items, juce::String(slot.query) + vibeMod, usedPaths, 0.02f))
            {
                res.sampleMap.zones.push_back(makeZoneFromItem(*match, slot.note, slot.note, slot.note, 2.0f, 150.0f, 1.0f, 250.0f));
                ++count;
            }
        }

        // Add pitched bass / 808 zone on C2..B2 (48..59) if requested
        if (p.contains("808") || p.contains("bass") || p.contains("sub"))
        {
            if (const auto* bass = pickBestSemanticMatch(items, "808 sub bass" + vibeMod, usedPaths, 0.02f))
            {
                res.sampleMap.zones.push_back(makeZoneFromItem(*bass, 48, 48, 59, 4.0f, 220.0f, 0.9f, 350.0f));
                ++count;
            }
        }

        res.summary = "Built " + juce::String(count) + "-zone kit using local semantic audio embeddings.";
    }

    res.effects = inferEffectsFromPrompt(prompt);
    if (!res.effects.empty())
        res.summary += " Added " + juce::String(static_cast<int>(res.effects.size())) + " Performance Rack FX.";

    res.success = !res.sampleMap.zones.empty();
    if (!res.success)
        res.summary = "No matching audio files were found on disk for that prompt.";

    return res;
}

bool AiKitBuilder::tryBuildWithGemini(const juce::String& prompt,
                                      const std::vector<MediaItem>& items,
                                      const juce::String& apiKey,
                                      AiKitBuildResult& outResult)
{
    // Build a compact top-32 candidate list using local semantic ranking so the LLM picks from real files
    auto qVec = AudioEmbeddingEngine::encodeTextQuery(prompt);
    std::vector<std::pair<float, const MediaItem*>> ranked;
    ranked.reserve(items.size());
    for (const auto& item : items)
    {
        if (item.filePath.isEmpty() || !juce::File(item.filePath).existsAsFile())
            continue;
        auto itemVec = (item.embedding.size() == AudioEmbeddingEngine::EmbeddingDim)
            ? item.embedding
            : std::vector<float>(AudioEmbeddingEngine::computeEmbedding(item).begin(),
                                 AudioEmbeddingEngine::computeEmbedding(item).end());
        ranked.push_back({ AudioEmbeddingEngine::cosineSimilarity(itemVec, qVec), &item });
    }
    if (ranked.empty()) return false;

    std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    if (ranked.size() > 32) ranked.resize(32);

    juce::String catalogText;
    for (size_t i = 0; i < ranked.size(); ++i)
    {
        const auto* it = ranked[i].second;
        catalogText += juce::String(static_cast<int>(i)) + ": " + it->fileName + " [";
        int tCount = 0;
        for (const auto& t : it->tags)
        {
            if (tCount++ > 0) catalogText += ",";
            catalogText += t;
            if (tCount >= 4) break;
        }
        catalogText += "]\n";
    }

    juce::String sysPrompt =
        "You are a sampler kit & FX designer. Given the user prompt and numbered sample catalog, respond ONLY with valid JSON: "
        "{\"pitchTracking\":bool,\"zones\":[{\"idx\":int,\"root\":int,\"low\":int,\"high\":int}],\"effects\":[\"Reverb\"|\"Delay\"|\"Filter\"|\"Distortion\"|\"Chorus\"|\"Compressor\"|\"Bitcrusher\"]}.\n"
        "Catalog:\n" + catalogText + "\nUser prompt: " + prompt;

    auto* partObj = new juce::DynamicObject();
    partObj->setProperty("text", sysPrompt);
    juce::Array<juce::var> parts;
    parts.add(juce::var(partObj));
    auto* contentObj = new juce::DynamicObject();
    contentObj->setProperty("parts", parts);
    juce::Array<juce::var> contents;
    contents.add(juce::var(contentObj));
    auto* reqObj = new juce::DynamicObject();
    reqObj->setProperty("contents", contents);

    juce::URL url("https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent?key=" + apiKey);
    url = url.withPOSTData(juce::JSON::toString(juce::var(reqObj), true));

    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                       .withExtraHeaders("Content-Type: application/json")
                       .withConnectionTimeoutMs(8000);

    std::unique_ptr<juce::InputStream> stream(url.createInputStream(options));
    if (stream == nullptr) return false;

    juce::var resp = juce::JSON::parse(stream->readEntireStreamAsString());
    if (!resp.isObject()) return false;

    auto candidates = resp.getProperty("candidates", juce::var());
    if (!candidates.isArray() || candidates.getArray()->isEmpty()) return false;

    juce::String text = candidates.getArray()->getFirst()
                            .getProperty("content", juce::var())
                            .getProperty("parts", juce::var())[0]
                            .getProperty("text", juce::var()).toString();

    int firstBrace = text.indexOfChar('{');
    int lastBrace = text.lastIndexOfChar('}');
    if (firstBrace < 0 || lastBrace <= firstBrace) return false;

    juce::var plan = juce::JSON::parse(text.substring(firstBrace, lastBrace + 1));
    if (!plan.isObject()) return false;

    outResult.sampleMap.midiChannel = 0;
    outResult.sampleMap.pitchTrackingEnabled = static_cast<bool>(plan.getProperty("pitchTracking", false));

    auto zonesVar = plan.getProperty("zones", juce::var());
    if (zonesVar.isArray())
    {
        for (const auto& zv : *zonesVar.getArray())
        {
            int idx = static_cast<int>(zv.getProperty("idx", -1));
            if (idx < 0 || idx >= static_cast<int>(ranked.size())) continue;
            int root = juce::jlimit(0, 127, static_cast<int>(zv.getProperty("root", 60)));
            int low = juce::jlimit(0, 127, static_cast<int>(zv.getProperty("low", root)));
            int high = juce::jlimit(low, 127, static_cast<int>(zv.getProperty("high", root)));
            outResult.sampleMap.zones.push_back(makeZoneFromItem(*ranked[static_cast<size_t>(idx)].second, root, low, high));
        }
    }

    if (outResult.sampleMap.zones.empty())
        return false;

    auto fxVar = plan.getProperty("effects", juce::var());
    if (fxVar.isArray())
    {
        for (const auto& fv : *fxVar.getArray())
        {
            juce::String name = fv.toString().toLowerCase();
            AiEffectSpec spec;
            if (name.contains("reverb")) spec.type = PerformanceEffectType::Reverb;
            else if (name.contains("delay")) spec.type = PerformanceEffectType::Delay;
            else if (name.contains("filter")) spec.type = PerformanceEffectType::Filter;
            else if (name.contains("distort")) spec.type = PerformanceEffectType::Distortion;
            else if (name.contains("chorus")) spec.type = PerformanceEffectType::Chorus;
            else if (name.contains("compress")) spec.type = PerformanceEffectType::Compressor;
            else if (name.contains("bitcrush")) spec.type = PerformanceEffectType::Bitcrusher;
            else continue;
            outResult.effects.push_back(spec);
        }
    }

    outResult.usedCloudLlm = true;
    outResult.success = true;
    outResult.summary = "Built " + juce::String(static_cast<int>(outResult.sampleMap.zones.size()))
        + "-zone kit via Gemini LLM + " + juce::String(static_cast<int>(outResult.effects.size())) + " FX.";
    return true;
}

} // namespace openwav
