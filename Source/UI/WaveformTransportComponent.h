#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_gui_basics/juce_gui_basics.h>
#endif
#include "../Audio/AudioEngine.h"

namespace openwav
{

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

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

    // juce::Timer callback for UI playhead updates
    void timerCallback() override;

    // juce::ChangeListener for AudioThumbnail
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    // AudioEngineListener callbacks
    void playbackStateChanged(bool isPlaying) override;
    void playbackPositionChanged(double currentSeconds, double totalSeconds) override {}
    void sampleLoaded(const juce::String& filePath) override;

    // juce::Slider::Listener callback
    void sliderValueChanged(juce::Slider* slider) override;

private:
    void seekToMousePosition(float mouseX);

    AudioEngine& audioEngine;

    juce::TextButton playPauseButton { "Play" };
    juce::TextButton stopButton { "Stop" };
    juce::TextButton loopButton { "Loop" };
    juce::TextButton autoPlayButton { "Auto" };

    juce::Slider volumeSlider;
    juce::Label timeLabel;
    juce::Label sampleNameLabel;

    double currentPositionSecs { 0.0 };
    double totalDurationSecs { 0.0 };
};

} // namespace openwav
