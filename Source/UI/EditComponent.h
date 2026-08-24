#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_gui_basics/juce_gui_basics.h>
 #include <juce_audio_formats/juce_audio_formats.h>
#endif
#include "../Audio/AudioEngine.h"
#include "../Models/PluginState.h"

namespace openwav
{

class EditComponent : public juce::Component,
                      public AudioEngineListener,
                      public juce::Timer,
                      public juce::ScrollBar::Listener
{
public:
    explicit EditComponent(AudioEngine& engine);
    ~EditComponent() override;

    EditComponentState getState() const;
    void setState(const EditComponentState& state);

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseMove(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    bool keyPressed(const juce::KeyPress& key) override;
    void lookAndFeelChanged() override;

    void visibilityChanged() override;

    // AudioEngineListener
    void playbackStateChanged(bool isPlaying) override;
    void playbackPositionChanged(double currentSeconds, double totalSeconds) override {}
    void sampleLoaded(const juce::String& filePath) override;
    void loopingStateChanged(bool enabled) override;

    // Timer
    void timerCallback() override;

    // ScrollBar::Listener
    void scrollBarMoved(juce::ScrollBar* scrollBarThatHasMoved, double newRangeStart) override;

    void loadSliceForEditing(int sliceIndex, double startRatio, double endRatio);
    void saveChangesAndUpdateSlice();
    bool hasAudioToEdit() const;

private:
    int activeEditingSliceIndex { -1 };
    double activeEditingStartRatio { 0.0 };
    double activeEditingEndRatio { 1.0 };
    bool isSliceEditingActive { false };
    // Layout helpers
    juce::Rectangle<float> getWaveformBounds() const;
    juce::Rectangle<int> getControlPanelBounds() const;
    void updateControlVisibility();

    // Painting sub-routines
    void paintTimeRuler(juce::Graphics& g, juce::Rectangle<float> bounds) const;
    void paintWaveform(juce::Graphics& g, juce::Rectangle<float> bounds) const;
    void paintMarkers(juce::Graphics& g, juce::Rectangle<float> bounds) const;
    void paintFadeEnvelopes(juce::Graphics& g, juce::Rectangle<float> bounds) const;
    void paintFadeHandles(juce::Graphics& g, juce::Rectangle<float> bounds) const;
    void paintPlayhead(juce::Graphics& g, juce::Rectangle<float> bounds) const;
    void paintSelectionInfoOverlay(juce::Graphics& g, juce::Rectangle<float> bounds) const;

    // Zero-crossing search helper
    double findNearestZeroCrossing(double ratio) const;

    // Coordinate conversions
    float ratioToX(double ratio, juce::Rectangle<float> bounds) const;
    double xToRatio(float x, juce::Rectangle<float> bounds) const;

    // Export & Editing Actions
    void exportEdited();
    void selectAllRegion();
    void deselectAllRegion();
    void cropToSelection();
    void resetSelection();
    void silenceSelectedRegion();
    void reverseSelectedRegion();
    void normalizeAudioPeak();
    void deverbSelectedRegion();
    void bakeFadesIntoBuffer();
    void playSelectionOnly();
    void applyFadeToBuffer(juce::AudioBuffer<float>& buffer, double sampleRate) const;

    // Fade curve evaluation
    float evaluateFadeCurve(float t, int curveType) const;

    AudioEngine& audioEngine;

    // Transport & editing buttons
    juce::TextButton playPauseButton { "Play" };
    juce::TextButton stopButton { "Stop" };
    juce::TextButton playSelButton { "Play Sel" };
    juce::TextButton loopToggleButton { "Loop" };
    juce::TextButton selectAllButton { "Select All" };
    juce::TextButton deselectAllButton { "Deselect All" };
    juce::TextButton cropButton { "Crop" };
    juce::TextButton resetSelectionButton { "Reset" };
    juce::TextButton snapZeroCrossingButton { "Snap 0-X" };

    // DSP Actions bar buttons
    juce::TextButton silenceButton { "Silence" };
    juce::TextButton reverseButton { "Reverse" };
    juce::TextButton normalizeButton { "Normalize" };
    juce::TextButton gainBoostButton { "+3dB" };
    juce::TextButton gainCutButton { "-3dB" };
    juce::TextButton autoTrimButton { "Auto-Trim" };
    juce::TextButton hpFilterButton { "Low Cut" };
    juce::TextButton invertPhaseButton { "Invert Phase" };
    juce::TextButton speed2xButton { "2x Speed" };
    juce::TextButton speedHalfButton { "0.5x Speed" };
    juce::TextButton deverbButton { "Deverb" };
    juce::TextButton bakeFadesButton { "Bake Fades" };
    juce::TextButton exportButton { "Export" };
    juce::TextButton revertOriginalButton { "Revert" };

    // Fine loop nudge controls
    juce::TextButton loopInNudgeLeft { "<" };
    juce::TextButton loopInNudgeRight { ">" };
    juce::TextButton loopOutNudgeLeft { "<" };
    juce::TextButton loopOutNudgeRight { ">" };

    // Time display labels
    juce::Label startLabel { {}, "Start:" };
    juce::Label endLabel { {}, "End:" };
    juce::Label loopInLabel { {}, "Loop In:" };
    juce::Label loopOutLabel { {}, "Loop Out:" };
    juce::Label startTimeLabel;
    juce::Label endTimeLabel;
    juce::Label loopInTimeLabel;
    juce::Label loopOutTimeLabel;
    juce::Label sampleNameLabel;

    // Fade controls
    juce::Slider fadeInSlider;
    juce::Slider fadeOutSlider;
    juce::ComboBox fadeInCurveBox;
    juce::ComboBox fadeOutCurveBox;
    juce::Label fadeInLabel { {}, "Fade In" };
    juce::Label fadeOutLabel { {}, "Fade Out" };
    juce::Label fadeInMsLabel;
    juce::Label fadeOutMsLabel;

    // Loop crossfade
    juce::Slider crossfadeSlider;
    juce::Label crossfadeLabel { {}, "X-Fade" };
    juce::Label crossfadeMsLabel;

    // Zoom
    juce::Slider zoomSlider;
    juce::TextButton zoomInButton { "+" };
    juce::TextButton zoomOutButton { "-" };
    juce::Label zoomLabel { {}, "Zoom" };

    // Scroll
    juce::ScrollBar hScrollBar { false };

    // State
    double zoomLevel { 1.0 };      // 1.0 = full view, higher = zoomed in
    double scrollOffset { 0.0 };   // 0.0–1.0 visible start ratio when zoomed
    double totalDurationSecs { 0.0 };
    double currentPositionSecs { 0.0 };
    bool snapToZeroCrossing { false };

    // Loop markers (ratios 0–1)
    double loopInRatio { 0.0 };
    double loopOutRatio { 1.0 };
    bool loopMarkersSet { false };

    // Fade durations in milliseconds
    double fadeInMs { 0.0 };
    double fadeOutMs { 0.0 };
    int fadeInCurveType { 0 };   // 0=Linear, 1=EqualPower, 2=Exponential
    int fadeOutCurveType { 0 };
    double crossfadeMs { 0.0 };

    // Drag state
    enum class DragTarget
    {
        None,
        StartMarker,
        EndMarker,
        LoopInMarker,
        LoopOutMarker,
        FadeInHandle,
        FadeOutHandle,
        SelectingRange,
        SelectingSpectralBox,
        ScrubbingPlayhead
    };
    DragTarget dragTarget { DragTarget::None };
    double dragStartRatio { 0.0 };

    // Spectrogram State & Controls
    juce::TextButton spectralToggleButton { "Spectral: OFF" };
    juce::TextButton repairSpectralButton { "Heal / Inpaint" };
    juce::TextButton deHarmonicButton { "De-Harmonic" };
    juce::TextButton denoiseSpectralButton { "Spectral Denoise" };
    juce::TextButton widenSpectralButton { "Stereo Spread" };
    juce::TextButton warmthSpectralButton { "Warmth" };
    juce::TextButton removeSpectralElementButton { "Remove" };
    juce::TextButton boostSpectralButton { "+6dB" };
    juce::TextButton attenuateSpectralButton { "-6dB" };
    juce::TextButton isolateSpectralButton { "Isolate" };

    bool isSpectralView { false };
    juce::Image spectrogramImage;
    bool spectrogramGenerated { false };

    double spectralTimeStart { 0.0 };
    double spectralTimeEnd { 1.0 };
    float spectralFreqLow { 20.0f };
    float spectralFreqHigh { 20000.0f };
    bool hasSpectralBoxSelection { false };
    juce::Point<float> spectralDragStartPos;
    juce::Rectangle<float> spectralDragRect;

    void generateSpectrogram();
    void paintSpectrogram(juce::Graphics& g, juce::Rectangle<float> bounds);
    void repairSpectralSelection();
    void deHarmonicSelection();
    void denoiseSpectralSelection();
    void widenSpectralSelection();
    void warmthSpectralSelection();
    void removeSpectralSelection();
    void boostSpectralSelection();
    void attenuateSpectralSelection();
    void isolateSpectralSelection();
    void restartPlaybackFromStart();
};

} // namespace openwav
