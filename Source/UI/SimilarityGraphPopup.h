#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_gui_basics/juce_gui_basics.h>
#endif

#include "../Database/TagDatabaseManager.h"

namespace openwav
{

class SimilarityGraphPopup : public juce::Component
{
public:
    SimilarityGraphPopup();
    ~SimilarityGraphPopup() override = default;

    void paint(juce::Graphics& g) override;
    
    // Shows the graph comparing the two items at the given screen position
    void showComparison(const MediaItem* targetItem, const MediaItem* hoveredItem, juce::Point<int> screenPos);
    void hidePopup();

private:
    MediaItem currentTarget;
    MediaItem currentHovered;
    bool hasData { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SimilarityGraphPopup)
};

} // namespace openwav
