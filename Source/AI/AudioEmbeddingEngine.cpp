#include "AudioEmbeddingEngine.h"
#include <cmath>
#include <algorithm>
#include <unordered_map>

namespace openwav
{

namespace
{
    juce::File gExternalModelFile;

    struct SemanticConcept
    {
        const char* keyword;
        int dim;
        float weight;
    };

    // Dimensions 0..15: Acoustic / spectral / temporal descriptors
    //   0: brightness (highFreqRatio)        1: darkness (1 - highFreqRatio)
    //   2: noisiness / zcr                   3: tonal / clean (1 - zcr)
    //   4: punch / transient (crestFactor)   5: sustained / smooth (1 - crest)
    //   6: short / one-shot                  7: long / pad / loop
    //   8: slow decay                        9: fast decay
    //  10: fast tempo (>=120 BPM)           11: slow tempo (<105 BPM)
    //  12: stereo width                     13: mono focus
    //  14: sub/low-end focus                15: high-end/air focus
    // Dimensions 16..63: Semantic category, instrument, character & genre axes
    const SemanticConcept kConcepts[] = {
        // Instruments / Categories (16..37)
        { "kick", 16, 1.4f }, { "bd", 16, 1.3f }, { "bassdrum", 16, 1.4f },
        { "808", 17, 1.3f }, { "sub", 17, 1.3f }, { "bass", 18, 1.3f }, { "reese", 18, 1.2f },
        { "snare", 19, 1.4f }, { "sd", 19, 1.3f }, { "rim", 19, 0.9f },
        { "clap", 20, 1.4f }, { "snap", 20, 1.1f },
        { "hihat", 21, 1.4f }, { "hat", 21, 1.4f }, { "hh", 21, 1.3f }, { "cymbal", 22, 1.2f }, { "crash", 22, 1.2f }, { "ride", 22, 1.2f },
        { "perc", 23, 1.3f }, { "shaker", 23, 1.2f }, { "tom", 24, 1.3f }, { "conga", 23, 1.1f }, { "bongo", 23, 1.1f },
        { "pad", 25, 1.4f }, { "atmos", 25, 1.3f }, { "texture", 25, 1.2f }, { "drone", 25, 1.2f },
        { "lead", 26, 1.3f }, { "synth", 27, 1.2f }, { "arp", 27, 1.1f },
        { "pluck", 28, 1.3f }, { "keys", 29, 1.3f }, { "piano", 29, 1.3f }, { "rhodes", 29, 1.3f }, { "organ", 29, 1.2f },
        { "chord", 30, 1.3f }, { "guitar", 31, 1.3f }, { "strings", 32, 1.3f }, { "brass", 33, 1.3f },
        { "vocal", 34, 1.4f }, { "vox", 34, 1.4f }, { "choir", 34, 1.2f },
        { "fx", 35, 1.3f }, { "riser", 35, 1.2f }, { "impact", 35, 1.2f }, { "sweep", 35, 1.2f }, { "foley", 35, 1.1f },
        { "loop", 36, 1.2f }, { "oneshot", 37, 1.2f }, { "one-shot", 37, 1.2f },

        // Timbral & Character descriptors (38..53)
        { "warm", 38, 1.3f }, { "analog", 38, 1.2f }, { "tape", 39, 1.3f }, { "vintage", 39, 1.2f },
        { "lofi", 40, 1.4f }, { "lo-fi", 40, 1.4f }, { "dusty", 40, 1.2f }, { "vinyl", 40, 1.2f },
        { "dark", 1, 1.2f }, { "muffled", 1, 1.1f }, { "deep", 14, 1.3f }, { "low", 14, 1.1f },
        { "bright", 0, 1.2f }, { "airy", 15, 1.3f }, { "crisp", 0, 1.1f }, { "crispy", 0, 1.1f }, { "sparkle", 15, 1.2f },
        { "punchy", 4, 1.3f }, { "hard", 4, 1.1f }, { "tight", 9, 1.2f }, { "short", 6, 1.2f }, { "snappy", 4, 1.2f },
        { "soft", 5, 1.2f }, { "smooth", 5, 1.2f }, { "lush", 41, 1.3f }, { "wide", 12, 1.2f },
        { "gritty", 42, 1.3f }, { "dirty", 42, 1.3f }, { "saturated", 42, 1.2f }, { "distorted", 42, 1.3f }, { "crunchy", 42, 1.2f },
        { "clean", 3, 1.2f }, { "pure", 3, 1.1f }, { "dry", 43, 1.2f }, { "wet", 44, 1.2f }, { "reverb", 44, 1.3f }, { "delay", 44, 1.2f },
        { "heavy", 45, 1.2f }, { "fat", 45, 1.2f }, { "thick", 45, 1.2f }, { "boomy", 14, 1.2f },

        // Genre / Vibe (54..63)
        { "trap", 54, 1.2f }, { "hiphop", 55, 1.2f }, { "boom", 55, 1.1f }, { "bap", 55, 1.1f },
        { "house", 56, 1.2f }, { "techno", 57, 1.2f }, { "edm", 56, 1.0f },
        { "dnb", 58, 1.2f }, { "jungle", 58, 1.2f },
        { "ambient", 59, 1.3f }, { "chill", 59, 1.1f }, { "cinematic", 60, 1.2f },
        { "acoustic", 61, 1.2f }, { "live", 61, 1.0f }, { "electronic", 62, 1.1f }, { "industrial", 63, 1.2f }
    };

    void accumulateTextTokens(AudioEmbeddingEngine::Embedding& vec, const juce::String& rawText, float scale)
    {
        juce::String lower = rawText.toLowerCase();
        juce::StringArray tokens;
        tokens.addTokens(lower, " ,._-/\\#|()[]", "\"'");

        for (const auto& tok : tokens)
        {
            if (tok.isEmpty()) continue;
            for (const auto& c : kConcepts)
            {
                if (tok == c.keyword || tok.contains(c.keyword))
                {
                    vec[static_cast<size_t>(c.dim)] += c.weight * scale;
                }
            }
        }
    }
}

void AudioEmbeddingEngine::normalize(Embedding& vec)
{
    double sumSq = 0.0;
    for (float v : vec)
        sumSq += static_cast<double>(v) * static_cast<double>(v);

    if (sumSq > 1e-12)
    {
        float invNorm = static_cast<float>(1.0 / std::sqrt(sumSq));
        for (float& v : vec)
            v *= invNorm;
    }
}

AudioEmbeddingEngine::Embedding AudioEmbeddingEngine::computeEmbedding(const MediaItem& item)
{
    Embedding vec {};

    // Acoustic descriptors (0..15)
    float hf = juce::jlimit(0.0f, 1.0f, static_cast<float>(item.highFreqRatio));
    float zcrNorm = juce::jlimit(0.0f, 1.0f, static_cast<float>(item.zcr * 2.5));
    float crestNorm = juce::jlimit(0.0f, 1.0f, static_cast<float>((item.crestFactor - 1.0) / 10.0));
    float decayNorm = juce::jlimit(0.0f, 1.0f, static_cast<float>(item.decayRatio));
    float durShort = (item.durationSeconds > 0.0 && item.durationSeconds < 1.2) ? 1.0f : 0.1f;
    float durLong = (item.durationSeconds >= 2.5) ? juce::jlimit(0.0f, 1.0f, static_cast<float>(item.durationSeconds / 8.0)) : 0.0f;

    vec[0] = hf * 1.1f;
    vec[1] = (1.0f - hf) * 1.1f;
    vec[2] = zcrNorm * 0.9f;
    vec[3] = (1.0f - zcrNorm) * 0.9f;
    vec[4] = crestNorm * 1.2f;
    vec[5] = (1.0f - crestNorm) * 0.9f;
    vec[6] = durShort;
    vec[7] = durLong;
    vec[8] = decayNorm;
    vec[9] = 1.0f - decayNorm;

    if (item.bpm >= 120.0) vec[10] = 0.8f;
    else if (item.bpm > 0.0 && item.bpm < 105.0) vec[11] = 0.8f;

    vec[12] = (item.numChannels >= 2) ? 0.6f : 0.0f;
    vec[13] = (item.numChannels == 1) ? 0.6f : 0.0f;
    vec[14] = (1.0f - hf) * (1.0f - zcrNorm * 0.5f);
    vec[15] = hf * 0.9f;

    // Semantic projections from tags, filename, and comment
    for (const auto& t : item.tags)
        accumulateTextTokens(vec, t, 1.0f);

    accumulateTextTokens(vec, item.fileName, 0.85f);
    if (item.comment.isNotEmpty())
        accumulateTextTokens(vec, item.comment, 0.6f);

    normalize(vec);
    return vec;
}

AudioEmbeddingEngine::Embedding AudioEmbeddingEngine::encodeTextQuery(const juce::String& query)
{
    Embedding vec {};
    accumulateTextTokens(vec, query, 1.2f);
    normalize(vec);
    return vec;
}

float AudioEmbeddingEngine::cosineSimilarity(const Embedding& a, const Embedding& b)
{
    float dot = 0.0f;
    for (size_t i = 0; i < EmbeddingDim; ++i)
        dot += a[i] * b[i];
    return dot;
}

float AudioEmbeddingEngine::cosineSimilarity(const std::vector<float>& a, const Embedding& b)
{
    if (a.size() != EmbeddingDim) return 0.0f;
    float dot = 0.0f;
    for (size_t i = 0; i < EmbeddingDim; ++i)
        dot += a[i] * b[i];
    return dot;
}

float AudioEmbeddingEngine::cosineSimilarity(const std::vector<float>& a, const std::vector<float>& b)
{
    if (a.size() != EmbeddingDim || b.size() != EmbeddingDim) return 0.0f;
    float dot = 0.0f;
    for (size_t i = 0; i < EmbeddingDim; ++i)
        dot += a[i] * b[i];
    return dot;
}

bool AudioEmbeddingEngine::isNaturalLanguageQuery(const juce::String& query)
{
    juce::String trimmed = query.trim();
    if (trimmed.isEmpty()) return false;
    if (trimmed.containsChar(' ')) return true;

    Embedding qVec = encodeTextQuery(trimmed);
    float mag = 0.0f;
    for (float v : qVec) mag += v * v;
    return mag > 0.1f;
}

void AudioEmbeddingEngine::setExternalModelPath(const juce::File& modelFile)
{
    gExternalModelFile = modelFile;
}

juce::File AudioEmbeddingEngine::getExternalModelPath()
{
    return gExternalModelFile;
}

bool AudioEmbeddingEngine::hasExternalModel()
{
    return gExternalModelFile.existsAsFile();
}

} // namespace openwav
