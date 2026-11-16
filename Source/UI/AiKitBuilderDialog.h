#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_gui_basics/juce_gui_basics.h>
#endif

#include "../AI/AiKitBuilder.h"
#include "OpenWavLookAndFeel.h"
#include <functional>

namespace openwav
{

class AiKitBuilderDialog : public juce::Component
{
public:
    AiKitBuilderDialog(TagDatabaseManager& db,
                       std::function<void(const AiKitBuildResult&)> onApplyCallback);
    ~AiKitBuilderDialog() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void runBuild();

    TagDatabaseManager& dbManager;
    std::function<void(const AiKitBuildResult&)> onApply;

    juce::Label titleLabel { {}, "AI KIT & PRESET BUILDER" };
    juce::Label subtitleLabel { {}, "Describe the drum kit or instrument + FX rack you want. Works offline via semantic audio embeddings or live with a Gemini API key." };

    juce::TextEditor promptEditor;
    juce::TextButton presetBtn1 { "Lo-Fi Boom Bap Kit" };
    juce::TextButton presetBtn2 { "808 Trap Kit + Sat" };
    juce::TextButton presetBtn3 { "Ambient Lush Pad" };
    juce::TextButton presetBtn4 { "House Groove + Delay" };

    juce::Label apiKeyLabel { {}, "Gemini API Key (Optional):" };
    juce::TextEditor apiKeyEditor;
    juce::Label statusLabel;

    juce::TextButton buildButton { "Generate Kit & FX" };
    juce::TextButton cancelButton { "Close" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AiKitBuilderDialog)
};

} // namespace openwav
