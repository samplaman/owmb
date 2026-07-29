#include "WaveformTransportComponent.h"
#include "OpenWavLookAndFeel.h"

namespace openwav
{

WaveformTransportComponent::WaveformTransportComponent(AudioEngine& engine)
    : audioEngine(engine)
{
    audioEngine.getThumbnail().addChangeListener(this);
    audioEngine.addListener(this);

    // Play/Pause Button
    playPauseButton.onClick = [this] {
        if (audioEngine.isPlaying())
            audioEngine.pause();
        else
            audioEngine.play();
    };
    addAndMakeVisible(playPauseButton);

    // Stop Button
    stopButton.onClick = [this] { audioEngine.stop(); };
    addAndMakeVisible(stopButton);

    // Loop Button
    loopButton.setClickingTogglesState(true);
    loopButton.onClick = [this] {
        audioEngine.setLooping(loopButton.getToggleState());
    };
    addAndMakeVisible(loopButton);

    // AutoPlay Button
    autoPlayButton.setClickingTogglesState(true);
    autoPlayButton.setToggleState(audioEngine.getAutoPlay(), juce::dontSendNotification);
    autoPlayButton.onClick = [this] {
        audioEngine.setAutoPlay(autoPlayButton.getToggleState());
    };
    addAndMakeVisible(autoPlayButton);

    // Volume Slider
    volumeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    volumeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    volumeSlider.setRange(0.0, 1.0, 0.01);
    volumeSlider.setValue(audioEngine.getGain());
    volumeSlider.addListener(this);
    addAndMakeVisible(volumeSlider);

    // Time Label
    timeLabel.setFont(juce::Font(12.0f).boldened());
    timeLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    timeLabel.setJustificationType(juce::Justification::centredRight);
    timeLabel.setText("00:00 / 00:00", juce::dontSendNotification);
    addAndMakeVisible(timeLabel);

    // Sample Name Label
    sampleNameLabel.setFont(juce::Font(13.0f).boldened());
    sampleNameLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textPrimary);
    sampleNameLabel.setText("No sample loaded", juce::dontSendNotification);
    addAndMakeVisible(sampleNameLabel);

    startTimerHz(30);
}

WaveformTransportComponent::~WaveformTransportComponent()
{
    stopTimer();
    audioEngine.getThumbnail().removeChangeListener(this);
    audioEngine.removeListener(this);
    volumeSlider.removeListener(this);
}

void WaveformTransportComponent::paint(juce::Graphics& g)
{
    g.fillAll(OpenWavLookAndFeel::bgHeader);

    // Border line at top
    g.setColour(OpenWavLookAndFeel::borderColour);
    g.drawRect(getLocalBounds().removeFromTop(1));

    // Calculate Transport Track Bounds
    auto area = getLocalBounds().reduced(12, 8);
    area.removeFromTop(32); // Space for top buttons and sample name

    auto trackBounds = area.removeFromTop(44).toFloat();

    // Track Background Card
    g.setColour(OpenWavLookAndFeel::bgDark);
    g.fillRoundedRectangle(trackBounds, 6.0f);
    g.setColour(OpenWavLookAndFeel::borderColour);
    g.drawRoundedRectangle(trackBounds, 6.0f, 1.0f);

    if (totalDurationSecs > 0.0)
    {
        auto waveformRect = trackBounds.reduced(8.0f, 4.0f);
        float totalWidth = waveformRect.getWidth();
        float barWidth = 3.0f;
        float gap = 2.0f;
        float step = barWidth + gap;
        int numBars = std::max(1, static_cast<int>(totalWidth / step));
        float startX = waveformRect.getX() + (totalWidth - (numBars * step - gap)) * 0.5f;

        float progressRatio = juce::jlimit(0.0f, 1.0f, static_cast<float>(currentPositionSecs / totalDurationSecs));
        float playheadX = waveformRect.getX() + totalWidth * progressRatio;

        auto& thumbnail = audioEngine.getThumbnail();
        int numChannels = thumbnail.getNumChannels();
        float halfHeight = (waveformRect.getHeight() - 4.0f) * 0.5f;

        for (int i = 0; i < numBars; ++i)
        {
            double startTime = (static_cast<double>(i) / static_cast<double>(numBars)) * totalDurationSecs;
            double endTime = (static_cast<double>(i + 1) / static_cast<double>(numBars)) * totalDurationSecs;

            float minVal = 0.0f;
            float maxVal = 0.0f;

            audioEngine.getMinMaxForTimeRange(startTime, endTime, minVal, maxVal);

            if (minVal == 0.0f && maxVal == 0.0f && numChannels > 0)
            {
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    float chMin = 0.0f, chMax = 0.0f;
                    thumbnail.getApproximateMinMax(startTime, endTime, ch, chMin, chMax);
                    if (chMin < minVal) minVal = chMin;
                    if (chMax > maxVal) maxVal = chMax;
                }
            }

            // Apply dynamic range curve so waveform contours and amplitude variations are vividly clear
            float peak = std::max(std::abs(minVal), std::abs(maxVal));
            if (peak > 0.0001f)
            {
                float boost = std::pow(peak, 0.65f) / peak;
                minVal *= boost;
                maxVal *= boost;
            }
            else
            {
                minVal = -0.05f;
                maxVal = 0.05f;
            }

            minVal = juce::jlimit(-1.0f, 0.0f, minVal);
            maxVal = juce::jlimit(0.0f, 1.0f, maxVal);

            float topY = waveformRect.getCentreY() - maxVal * halfHeight;
            float bottomY = waveformRect.getCentreY() - minVal * halfHeight;
            float barHeight = std::max(3.0f, bottomY - topY);
            float y = topY;

            float x = startX + i * step;
            juce::Rectangle<float> barRect(x, y, barWidth, barHeight);

            bool isPlayed = (x + barWidth <= playheadX);
            if (isPlayed)
                g.setColour(OpenWavLookAndFeel::accentCyan);
            else
                g.setColour(OpenWavLookAndFeel::textSecondary.withAlpha(0.40f));

            g.fillRoundedRectangle(barRect, 1.5f);
        }

        // Draw Scrubber Line & Playhead Knob
        g.setColour(OpenWavLookAndFeel::accentCyan);
        g.drawLine(playheadX, trackBounds.getY() + 3.0f, playheadX, trackBounds.getBottom() - 3.0f, 2.0f);
        g.setColour(juce::Colours::white);
        g.fillEllipse(playheadX - 5.0f, trackBounds.getCentreY() - 5.0f, 10.0f, 10.0f);
    }
    else
    {
        g.setFont(juce::Font(13.0f));
        g.setColour(OpenWavLookAndFeel::textSecondary);
        g.drawText("Select a sample to preview playback", trackBounds, juce::Justification::centred, true);
    }
}

void WaveformTransportComponent::resized()
{
    auto area = getLocalBounds().reduced(12, 8);
    auto topRow = area.removeFromTop(32);

    playPauseButton.setBounds(topRow.removeFromLeft(70).withHeight(28));
    topRow.removeFromLeft(6);

    stopButton.setBounds(topRow.removeFromLeft(65).withHeight(28));
    topRow.removeFromLeft(6);

    loopButton.setBounds(topRow.removeFromLeft(65).withHeight(28));
    topRow.removeFromLeft(6);

    autoPlayButton.setBounds(topRow.removeFromLeft(65).withHeight(28));
    topRow.removeFromLeft(12);

    sampleNameLabel.setBounds(topRow.removeFromLeft(220).withHeight(28));

    timeLabel.setBounds(topRow.removeFromRight(110).withHeight(28));
    topRow.removeFromRight(10);

    volumeSlider.setBounds(topRow.removeFromRight(100).withHeight(28));
}

void WaveformTransportComponent::mouseDown(const juce::MouseEvent& e)
{
    seekToMousePosition(static_cast<float>(e.x));
}

void WaveformTransportComponent::mouseDrag(const juce::MouseEvent& e)
{
    seekToMousePosition(static_cast<float>(e.x));
}

void WaveformTransportComponent::seekToMousePosition(float mouseX)
{
    auto area = getLocalBounds().reduced(12, 8);
    area.removeFromTop(32);
    auto waveformBounds = area.removeFromTop(56).toFloat();

    if (waveformBounds.contains(mouseX, waveformBounds.getCentreY()))
    {
        double ratio = (mouseX - waveformBounds.getX()) / waveformBounds.getWidth();
        audioEngine.setPositionRatio(ratio);
        repaint();
    }
}

void WaveformTransportComponent::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &audioEngine.getThumbnail())
    {
        repaint();
    }
}

void WaveformTransportComponent::playbackStateChanged(bool isPlaying)
{
    playPauseButton.setButtonText(isPlaying ? "Pause" : "Play");
    repaint();
}

void WaveformTransportComponent::timerCallback()
{
    if (audioEngine.isPlaying())
    {
        currentPositionSecs = audioEngine.getCurrentPositionSeconds();
        totalDurationSecs = audioEngine.getTotalLengthSeconds();

        int curMins = static_cast<int>(currentPositionSecs) / 60;
        int curSecs = static_cast<int>(currentPositionSecs) % 60;
        int totMins = static_cast<int>(totalDurationSecs) / 60;
        int totSecs = static_cast<int>(totalDurationSecs) % 60;

        juce::String timeStr = juce::String::formatted("%02d:%02d / %02d:%02d", curMins, curSecs, totMins, totSecs);
        timeLabel.setText(timeStr, juce::dontSendNotification);

        repaint();
    }
}

void WaveformTransportComponent::sampleLoaded(const juce::String& filePath)
{
    juce::File f(filePath);
    sampleNameLabel.setText(f.getFileName(), juce::dontSendNotification);
    totalDurationSecs = audioEngine.getTotalLengthSeconds();
    currentPositionSecs = 0.0;
    repaint();
}

void WaveformTransportComponent::sliderValueChanged(juce::Slider* slider)
{
    if (slider == &volumeSlider)
    {
        audioEngine.setGain(static_cast<float>(volumeSlider.getValue()));
    }
}

} // namespace openwav
