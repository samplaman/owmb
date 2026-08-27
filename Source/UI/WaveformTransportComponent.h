#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_gui_basics/juce_gui_basics.h>
#endif
#include "../Audio/AudioEngine.h"
#include "../Audio/LorisResynthesizer.h"

namespace openwav
{

class SlicesGridComponent : public juce::Component
{
public:
    SlicesGridComponent(AudioEngine& engine, std::function<void(int)> onSliceDragged);
    ~SlicesGridComponent() override = default;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

    void updateSlices(const std::vector<double>& ratios, double duration, int viewportWidth, int viewportHeight);
    void setSelectedSliceIndex(int index);
    int getSelectedSliceIndex() const { return selectedSliceIndex; }

    juce::Rectangle<float> getSliceCardBounds(int index) const;
    int getSliceIndexAt(juce::Point<float> pos) const;

    std::function<void(int, double, double)> onSliceSelected;

private:
    AudioEngine& audioEngine;
    std::function<void(int)> onSliceDragged;
    std::vector<double> sliceRatios;
    int clickedSliceIndex { -1 };
    int hoveredSliceIndex { -1 };
    int selectedSliceIndex { -1 };
    double totalDurationSecs { 0.0 };
};

class BpmControlComponent : public juce::Component,
                            private juce::Label::Listener
{
public:
    BpmControlComponent(AudioEngine& engine);
    ~BpmControlComponent() override = default;

    void setBpm(double bpm, bool sendNotification = false);
    double getBpm() const { return currentBpm; }
    void setHostSynced(bool synced);

    std::function<void(double)> onBpmChanged;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void lookAndFeelChanged() override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

private:
    void labelTextChanged(juce::Label* label) override;
    void editorShown(juce::Label* label, juce::TextEditor& editor) override;

    AudioEngine& audioEngine;
    double currentBpm { 120.0 };
    bool isSynced { false };
    double dragStartBpm { 120.0 };
    juce::Point<int> dragStartPos;

    juce::Label tempoLabel;
    juce::TextButton minusBtn { "-" };
    juce::TextButton plusBtn { "+" };
};

class WaveformTransportComponent : public juce::Component,
                                    public juce::ChangeListener,
                                    public AudioEngineListener,
                                    public juce::Slider::Listener,
                                    public juce::Timer
{
public:
    explicit WaveformTransportComponent(AudioEngine& engine);
    ~WaveformTransportComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void lookAndFeelChanged() override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

    // juce::Timer callback for UI playhead updates
    void timerCallback() override;

    // juce::ChangeListener for AudioThumbnail
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    // AudioEngineListener callbacks
    void playbackStateChanged(bool isPlaying) override;
    void playbackPositionChanged(double currentSeconds, double totalSeconds) override {}
    void sampleLoaded(const juce::String& filePath) override;
    void transportSyncChanged(bool isSynced) override;
    void bpmChanged(double newBpm) override;
    void loopingStateChanged(bool enabled) override;
    void activeSliceTriggered(int sliceIndex) override { setActiveSliceIndex(sliceIndex); }

    // juce::Slider::Listener callback
    void sliderValueChanged(juce::Slider* slider) override;

    void togglePlay();
    void toggleLoop();
    void toggleSync();
    void triggerSlice();
    void setSliceRatios(const std::vector<double>& ratios);
    const std::vector<double>& getSliceRatios() const { return sliceRatios; }
    void setActiveSliceIndex(int sliceIndex);
    void setActiveSliceRange(double startRatio, double endRatio, int sliceIndex = -1);

    std::function<void(const std::vector<double>&)> onSlicesGenerated;
    std::function<void(const std::vector<ResynthesizedZone>&)> onLorisResynthCompleted;

    void openLorisResynthesis();

private:
    void runAutoSlice();
    void openSliceConfigWindow();
    void exportAndDragSlice(int sliceIndex);

    AudioEngine& audioEngine;

    juce::TextButton playPauseButton { "Play" };
    juce::TextButton stopButton { "Stop" };
    juce::TextButton loopButton { "Loop" };
    juce::TextButton autoPlayButton { "Auto" };
    juce::TextButton autoSliceButton { "Slice" };
    juce::TextButton lorisResynthButton { "Resynth" };
    juce::TextButton syncButton { "SYNC" };
    BpmControlComponent bpmControl;

    std::vector<double> sliceRatios;

    juce::Slider volumeSlider;
    juce::Label timeLabel;
    juce::Label sampleNameLabel;

    double currentPositionSecs { 0.0 };
    double totalDurationSecs { 0.0 };

    int activeSliceIndex { -1 };
    double activeSliceStartRatio { 0.0 };
    double activeSliceEndRatio { 1.0 };

    enum class DragMode
    {
        None,
        DraggingStart,
        DraggingEnd,
        SelectingRange
    };
    DragMode dragMode { DragMode::None };
    double dragStartRatio { 0.0 };

    // Slices Scrollable Grid
    juce::Viewport slicesViewport;
    SlicesGridComponent slicesGrid;
};

} // namespace openwav
