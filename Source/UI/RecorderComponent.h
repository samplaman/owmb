#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_gui_basics/juce_gui_basics.h>
#endif

#include "../Audio/AudioEngine.h"
#include "../Database/TagDatabaseManager.h"

namespace openwav
{

class RecorderComponent : public juce::Component,
                          public juce::Timer
{
public:
    RecorderComponent(AudioEngine& engine, TagDatabaseManager& db);
    ~RecorderComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void lookAndFeelChanged() override;

    void timerCallback() override;

private:
    void toggleRecording();
    void saveAndAddToLibrary();
    void playPreview();

    AudioEngine& audioEngine;
    TagDatabaseManager& dbManager;

    juce::TextButton recordButton { "RECORD" };
    juce::TextButton previewButton { "Preview" };
    juce::TextButton saveButton { "Save & Add to Library" };

    juce::Label channelLabel { {}, "Input Channel:" };
    juce::ComboBox channelSelector;

    juce::Label countInLabel { {}, "Count-In:" };
    juce::ComboBox countInSelector;

    juce::Label nameLabel { {}, "Sample Name:" };
    juce::TextEditor nameEditor;

    juce::Label tagsLabel { {}, "Tags (comma separated):" };
    juce::TextEditor tagsEditor;

    juce::Label timeLabel;
    juce::Label statusLabel;

    float currentLeftLevel { 0.0f };
    float currentRightLevel { 0.0f };
    float smoothLeftLevel { 0.0f };
    float smoothRightLevel { 0.0f };

    bool isCountingDown { false };
    int countdownValue { 3 };
    uint32_t lastBeatMs { 0 };
    float flashAlpha { 0.0f };

    juce::File lastSavedFile;
    bool hasRecordedBuffer { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RecorderComponent)
};

} // namespace openwav
