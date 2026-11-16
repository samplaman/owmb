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
#include <vector>

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
    void mouseDoubleClick(const juce::MouseEvent& e) override;

    void timerCallback() override;

    std::function<void(const MediaItem&)> onRecordingSaved;
    std::function<void(const MediaItem&)> onSendToEditor;
    std::function<void(const MediaItem&)> onSendToSampleMap;
    std::function<void()> onOpenAudioSettings;

private:
    struct RecordedTake
    {
        juce::String name;
        juce::AudioBuffer<float> buffer;
        double sampleRate { 44100.0 };
    };

    void toggleRecording();
    void togglePauseRecording();
    void clearCurrentTake();
    void trimCurrentTake();
    void saveAndAddToLibrary();
    bool saveCurrentTakeToDiskAndDb(MediaItem& outItem, bool autoPreview);
    void playPreview();
    void updateInputState();
    void beginCountdownOrRecord();
    void storeTakeSnapshot();
    void loadTakeIntoView(int takeIndex);
    void syncGlobalWaveformToCurrentTake();
    void resetEq();

    juce::Point<float> getEqNodeScreenPos(float freq, float gainDb, juce::Rectangle<float> eqBounds) const;
    void updateEqNodeFromScreenPos(int nodeIdx, juce::Point<float> pos, juce::Rectangle<float> eqBounds);

    AudioEngine& audioEngine;
    TagDatabaseManager& dbManager;

    // DAW Transport Controls
    juce::TextButton recordButton { "REC" };
    juce::TextButton pauseButton { "PAUSE" };
    juce::TextButton clearButton { "CLEAR" };
    juce::TextButton previewButton { "PLAY" };
    juce::TextButton trimButton { "TRIM" };
    juce::TextButton saveButton { "Save to Library" };
    juce::TextButton toEditorButton { "To Editor" };
    juce::TextButton toSamplerButton { "To Sampler" };
    juce::TextButton audioSetupButton { "Audio Setup" };

    juce::ToggleButton monitorButton { "Monitor" };
    juce::ToggleButton metronomeButton { "Click" };
    juce::ToggleButton waitSignalButton { "Wait Signal" };
    juce::Slider bpmSlider;
    juce::Label bpmLabel { {}, "120 BPM" };

    juce::Slider inputGainSlider;
    juce::Label inputGainLabel { {}, "Gain: 0.0 dB" };

    juce::Label channelLabel { {}, "Input:" };
    juce::ComboBox channelSelector;

    juce::Label countInLabel { {}, "Count-In:" };
    juce::ComboBox countInSelector;

    juce::Label nameLabel { {}, "Name:" };
    juce::TextEditor nameEditor;

    juce::Label tagsLabel { {}, "Tags:" };
    juce::TextEditor tagsEditor;

    juce::ToggleButton lowCutButton { "80Hz Low Cut" };
    juce::ToggleButton normalizeButton { "Auto-Normalize" };
    juce::TextButton resetEqButton { "Reset EQ" };

    juce::ComboBox takeSelector;
    juce::Label takeLabel { {}, "Takes:" };

    juce::Label timeLabel;
    juce::Label formatInfoLabel;
    juce::Label statusLabel;

    // Metering & Clip Hold
    float smoothLeftLevel { 0.0f };
    float smoothRightLevel { 0.0f };
    float peakHoldLeft { 0.0f };
    float peakHoldRight { 0.0f };
    uint32_t peakHoldTimeL { 0 };
    uint32_t peakHoldTimeR { 0 };
    bool clipLeft { false };
    bool clipRight { false };

    // Countdown / Metronome / Auto-Threshold State
    bool isCountingDown { false };
    bool isPausedRecording { false };
    bool isArmedWaitSignal { false };
    int countdownValue { 3 };
    int beatCounter { 0 };
    uint32_t lastBeatMs { 0 };
    float flashAlpha { 0.0f };

    // Recorded Takes & Trim Handles
    std::vector<RecordedTake> takes;
    int activeTakeIndex { -1 };
    double trimStartRatio { 0.0 };
    double trimEndRatio { 1.0 };
    bool hasRecordedBuffer { false };
    juce::File lastSavedFile;
    juce::File lastPreviewTempFile;
    int previewTempCounter { 0 };

    // Interactive Waveform / Trim / Meter Hitboxes
    juce::Rectangle<float> cachedScopeArea;
    juce::Rectangle<float> cachedMeterArea;
    enum class DragMode { None, EqNode, TrimStart, TrimEnd };
    DragMode dragMode { DragMode::None };

    // Interactive 9-Band Parametric EQ Parameters
    std::array<float, 9> eqFreqs { 60.0f, 120.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f };
    std::array<float, 9> eqGains { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };

    juce::Rectangle<float> cachedEqArea;
    int draggedNodeIndex { -1 };
    int hoveredNodeIndex { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RecorderComponent)
};

} // namespace openwav
