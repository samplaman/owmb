#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_core/juce_core.h>
#endif

#include "PluginState.h"

namespace openwav
{

class SfzParser
{
public:
    static bool parseFile(const juce::File& sfzFile, SampleMapState& outState, juce::String& outError);
    static int parseMidiNote(const juce::String& text, int noteOffset = 0);
};

} // namespace openwav
