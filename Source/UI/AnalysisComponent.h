#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_gui_basics/juce_gui_basics.h>
#endif
#include "../Models/MediaItem.h"

namespace openwav
{

class AnalysisComponent : public juce::Component
{
public:
    AnalysisComponent();
    ~AnalysisComponent() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setItem(const MediaItem& item);
    void clearItem();

private:
    void paintRadarChart(juce::Graphics& g, juce::Rectangle<float> area) const;
    void paintInfoSection(juce::Graphics& g, juce::Rectangle<float> area) const;
    void paintTagsSection(juce::Graphics& g, juce::Rectangle<float> area) const;

    static juce::String formatFileSize(int64_t bytes);
    static juce::String formatDateAdded(int64_t timestampMs);

    bool hasItem { false };
    MediaItem currentItem;
};

} // namespace openwav
