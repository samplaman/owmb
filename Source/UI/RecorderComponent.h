#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_gui_basics/juce_gui_basics.h>
#endif

#include "../Audio/AudioEngine.h"
#include "../Database/TagDatabaseManager.h"
#include <cmath>
#include <algorithm>
#include <cstring>

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
    void visibilityChanged() override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

    void timerCallback() override;

private:
    void toggleRecording();
    void saveAndAddToLibrary();
    void playPreview();
    void updateInputMuteState();

    juce::Point<float> getEqNodeScreenPos(float freq, float gainDb, juce::Rectangle<float> eqBounds) const;
    void updateEqNodeFromScreenPos(int nodeIdx, juce::Point<float> pos, juce::Rectangle<float> eqBounds);

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

    juce::ToggleButton lowCutButton { "80Hz Low Cut" };
    juce::ToggleButton normalizeButton { "Auto-Normalize" };

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

    // Interactive 9-Band Parametric EQ Parameters
    std::array<float, 9> eqFreqs { 60.0f, 120.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f };
    std::array<float, 9> eqGains { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };

    juce::Rectangle<float> cachedEqArea;
    int draggedNodeIndex { -1 };
    int hoveredNodeIndex { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RecorderComponent)
};

} // namespace openwav
