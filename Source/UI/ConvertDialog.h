#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_gui_basics/juce_gui_basics.h>
#endif
#include "../Models/MediaItem.h"
#include "../Database/TagDatabaseManager.h"
#include "../Audio/AudioEngine.h"

namespace openwav
{

class ConvertDialog : public juce::Component
{
public:
    ConvertDialog(const MediaItem& item, AudioEngine& engine, TagDatabaseManager& dbManager, const std::vector<juce::AudioFormat*>& writableFormats);
    ~ConvertDialog() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void lookAndFeelChanged() override;

    void showDialog();
    void hideDialog();

private:
    void performConversion();

    MediaItem currentItem;
    AudioEngine& audioEngine;
    TagDatabaseManager& dbManager;
    std::vector<juce::AudioFormat*> formats;

    juce::Label titleLabel;
    juce::Label formatLabel;
    juce::ComboBox formatCombo;
    juce::Label sampleRateLabel;
    juce::ComboBox sampleRateCombo;
    juce::Label bitDepthLabel;
    juce::ComboBox bitDepthCombo;

    juce::TextButton convertButton{"Convert..."};
    juce::TextButton cancelButton{"Cancel"};

    juce::Component::SafePointer<juce::DialogWindow> dialogWindow;
    std::shared_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ConvertDialog)
};

} // namespace openwav
