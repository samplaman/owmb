#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_core/juce_core.h>
#endif

#include "../Models/MediaItem.h"
#include <array>
#include <vector>

namespace openwav
{

/**
 * Computes normalized 64-dimensional acoustic-semantic embeddings for audio samples
 * and natural-language search queries. Supports an optional external CoreML/ONNX
 * model path hook while providing a fast, zero-dependency local embedder.
 */
class AudioEmbeddingEngine
{
public:
    static constexpr int EmbeddingDim = 64;
    using Embedding = std::array<float, EmbeddingDim>;

    static Embedding computeEmbedding(const MediaItem& item);
    static Embedding encodeTextQuery(const juce::String& query);
    static float cosineSimilarity(const Embedding& a, const Embedding& b);
    static float cosineSimilarity(const std::vector<float>& a, const Embedding& b);
    static float cosineSimilarity(const std::vector<float>& a, const std::vector<float>& b);

    static bool isNaturalLanguageQuery(const juce::String& query);

    static void setExternalModelPath(const juce::File& modelFile);
    static juce::File getExternalModelPath();
    static bool hasExternalModel();

private:
    static void normalize(Embedding& vec);
};

} // namespace openwav
