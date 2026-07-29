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
    loopButton.setToggleState(audioEngine.isLooping(), juce::dontSendNotification);
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

    // AutoSlice Button
    autoSliceButton.onClick = [this] { runAutoSlice(); };
    addAndMakeVisible(autoSliceButton);

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

    // Calculate Layout Areas
    auto area = getLocalBounds().reduced(12, 8);
    area.removeFromTop(32); // Space for top buttons and sample name
    area.removeFromTop(8);  // Spacing gap

    // Reserve 80px for Slices section at bottom
    auto slicesBounds = area.removeFromBottom(80);
    area.removeFromBottom(8); // Gap between waveform and slices section

    auto trackBounds = area.toFloat();

    // Track Background Card
    g.setColour(OpenWavLookAndFeel::bgDark);
    g.fillRoundedRectangle(trackBounds, 6.0f);
    g.setColour(OpenWavLookAndFeel::borderColour);
    g.drawRoundedRectangle(trackBounds, 6.0f, 1.0f);

    if (totalDurationSecs > 0.0)
    {
        auto waveformRect = trackBounds.reduced(8.0f, 8.0f);
        float totalWidth = waveformRect.getWidth();

        // 1. Draw detailed waveform per pixel column
        int numPixels = static_cast<int>(totalWidth);
        float startX = waveformRect.getX();

        float progressRatio = juce::jlimit(0.0f, 1.0f, static_cast<float>(currentPositionSecs / totalDurationSecs));
        float playheadX = waveformRect.getX() + totalWidth * progressRatio;

        double startRatio = audioEngine.getSampleStartRatio();
        double endRatio = audioEngine.getSampleEndRatio();

        float selStartX = waveformRect.getX() + totalWidth * startRatio;
        float selEndX = waveformRect.getX() + totalWidth * endRatio;

        auto& thumbnail = audioEngine.getThumbnail();
        int numChannels = thumbnail.getNumChannels();
        float halfHeight = (waveformRect.getHeight() - 6.0f) * 0.5f;
        float centerY = waveformRect.getCentreY();

        for (int x = 0; x < numPixels; ++x)
        {
            float pixelX = startX + x;
            double startTime = (static_cast<double>(x) / static_cast<double>(numPixels)) * totalDurationSecs;
            double endTime = (static_cast<double>(x + 1) / static_cast<double>(numPixels)) * totalDurationSecs;

            float lMin = 0.0f, lMax = 0.0f;
            float rMin = 0.0f, rMax = 0.0f;

            audioEngine.getMinMaxForTimeRange(startTime, endTime, lMin, lMax, 0);
            if (numChannels >= 2)
            {
                audioEngine.getMinMaxForTimeRange(startTime, endTime, rMin, rMax, 1);
            }
            else
            {
                rMin = lMin;
                rMax = lMax;
            }

            if (lMin == 0.0f && lMax == 0.0f && numChannels > 0)
            {
                thumbnail.getApproximateMinMax(startTime, endTime, 0, lMin, lMax);
                if (numChannels >= 2)
                    thumbnail.getApproximateMinMax(startTime, endTime, 1, rMin, rMax);
                else
                {
                    rMin = lMin;
                    rMax = lMax;
                }
            }

            auto boostPeak = [](float& minVal, float& maxVal) {
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
            };

            boostPeak(lMin, lMax);
            boostPeak(rMin, rMax);

            float lHeight = std::max(1.5f, lMax * halfHeight);
            float lTopY = centerY - lHeight - 1.0f;

            float rHeight = std::max(1.5f, std::abs(rMin) * halfHeight);
            float rBottomY = centerY + 1.0f + rHeight;

            bool inSelection = (pixelX >= selStartX && pixelX <= selEndX);
            bool isPlayed = (pixelX <= playheadX);

            if (inSelection)
            {
                if (isPlayed)
                    g.setColour(OpenWavLookAndFeel::accentCyan);
                else
                    g.setColour(OpenWavLookAndFeel::textPrimary.withAlpha(0.65f));
            }
            else
            {
                g.setColour(OpenWavLookAndFeel::textSecondary.withAlpha(0.20f));
            }

            g.drawVerticalLine(static_cast<int>(pixelX), lTopY, centerY - 1.0f);
            g.drawVerticalLine(static_cast<int>(pixelX), centerY + 1.0f, rBottomY);
        }

        // Draw darker overlay for the non-selected sections
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        if (selStartX > waveformRect.getX())
        {
            g.fillRect(waveformRect.getX(), waveformRect.getY(), selStartX - waveformRect.getX(), waveformRect.getHeight());
        }
        if (selEndX < waveformRect.getRight())
        {
            g.fillRect(selEndX, waveformRect.getY(), waveformRect.getRight() - selEndX, waveformRect.getHeight());
        }

        // Draw Selection Range Boundary Handles & Top Tabs
        g.setColour(OpenWavLookAndFeel::accentCyan);

        // Start handle line
        g.drawVerticalLine(static_cast<int>(selStartX), waveformRect.getY(), waveformRect.getBottom());
        g.fillRoundedRectangle(selStartX - 4.0f, waveformRect.getY() - 4.0f, 8.0f, 8.0f, 2.0f);

        // End handle line
        g.drawVerticalLine(static_cast<int>(selEndX), waveformRect.getY(), waveformRect.getBottom());
        g.fillRoundedRectangle(selEndX - 4.0f, waveformRect.getY() - 4.0f, 8.0f, 8.0f, 2.0f);

        // Draw slice divider lines on main waveform
        for (size_t i = 1; i < sliceRatios.size(); ++i)
        {
            float sliceStartX = waveformRect.getX() + totalWidth * sliceRatios[i];
            g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.6f));
            float dashLengths[] = { 4.0f, 4.0f };
            g.drawDashedLine(juce::Line<float>(sliceStartX, waveformRect.getY(), sliceStartX, waveformRect.getBottom()), dashLengths, 2, 1.0f);
        }

        // Draw Playhead Line & Playhead Knob
        g.setColour(OpenWavLookAndFeel::accentCyan.brighter(0.4f));
        g.drawLine(playheadX, trackBounds.getY() + 3.0f, playheadX, trackBounds.getBottom() - 3.0f, 2.0f);
        g.setColour(juce::Colours::white);
        g.fillEllipse(playheadX - 6.0f, trackBounds.getCentreY() - 6.0f, 12.0f, 12.0f);

        // 2. Draw Slices Section
        sliceBadgeBounds.clear();
        if (sliceRatios.empty())
        {
            g.setColour(OpenWavLookAndFeel::bgDark);
            g.fillRoundedRectangle(slicesBounds.toFloat(), 4.0f);
            g.setColour(OpenWavLookAndFeel::borderColour);
            g.drawRoundedRectangle(slicesBounds.toFloat(), 4.0f, 1.0f);

            g.setFont(juce::Font(11.0f));
            g.setColour(OpenWavLookAndFeel::textSecondary);
            g.drawText("No slices. Click the 'Slice' button to automatically chop this sample.", slicesBounds, juce::Justification::centred, true);
        }
        else
        {
            int numSlices = static_cast<int>(sliceRatios.size());
            float cardGap = 6.0f;
            float totalSlicesWidth = slicesBounds.getWidth();
            float cardWidth = std::min(110.0f, (totalSlicesWidth - (numSlices - 1) * cardGap) / numSlices);
            cardWidth = std::max(60.0f, cardWidth);

            for (int i = 0; i < numSlices; ++i)
            {
                float cardX = slicesBounds.getX() + i * (cardWidth + cardGap);
                juce::Rectangle<float> cardRect(cardX, slicesBounds.getY(), cardWidth, slicesBounds.getHeight());
                sliceBadgeBounds.push_back(cardRect);

                // Draw card background
                g.setColour(OpenWavLookAndFeel::bgDark);
                g.fillRoundedRectangle(cardRect, 4.0f);

                double startR = sliceRatios[i];
                double endR = (i + 1 < numSlices) ? sliceRatios[i + 1] : 1.0;

                double currentStart = audioEngine.getSampleStartRatio();
                double currentEnd = audioEngine.getSampleEndRatio();
                bool isSelectedSlice = (std::abs(currentStart - startR) < 0.001 && std::abs(currentEnd - endR) < 0.001);

                if (isSelectedSlice)
                {
                    g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.2f));
                    g.fillRoundedRectangle(cardRect, 4.0f);
                    g.setColour(OpenWavLookAndFeel::accentCyan);
                    g.drawRoundedRectangle(cardRect, 4.0f, 1.5f);
                }
                else
                {
                    g.setColour(OpenWavLookAndFeel::borderColour);
                    g.drawRoundedRectangle(cardRect, 4.0f, 1.0f);
                }

                // Draw a mini waveform inside the card
                auto miniWaveformRect = cardRect.reduced(6.0f, 4.0f);
                auto labelRect = miniWaveformRect.removeFromBottom(16.0f);
                miniWaveformRect.removeFromBottom(2.0f); // spacing

                g.setColour(OpenWavLookAndFeel::bgHeader);
                g.fillRoundedRectangle(miniWaveformRect, 2.0f);

                if (totalDurationSecs > 0.0)
                {
                    float miniWidth = miniWaveformRect.getWidth();
                    int numMiniBars = std::max(5, static_cast<int>(miniWidth / 3.0f));
                    float miniStep = miniWidth / numMiniBars;
                    float miniCenterY = miniWaveformRect.getCentreY();
                    float miniHalfH = (miniWaveformRect.getHeight() - 2.0f) * 0.5f;

                    g.setColour(isSelectedSlice ? OpenWavLookAndFeel::accentCyan : OpenWavLookAndFeel::textSecondary.withAlpha(0.7f));
                    for (int b = 0; b < numMiniBars; ++b)
                    {
                        double bStart = startR + (static_cast<double>(b) / numMiniBars) * (endR - startR);
                        double bEnd = startR + (static_cast<double>(b + 1) / numMiniBars) * (endR - startR);

                        float lMin = 0.0f, lMax = 0.0f;
                        audioEngine.getMinMaxForTimeRange(bStart * totalDurationSecs, bEnd * totalDurationSecs, lMin, lMax, 0);

                        float peak = std::max(std::abs(lMin), std::abs(lMax));
                        float barH = std::max(1.0f, std::pow(peak, 0.5f) * miniHalfH);

                        g.fillRect(miniWaveformRect.getX() + b * miniStep + 0.5f, miniCenterY - barH, miniStep - 0.5f, barH * 2.0f);
                    }
                }

                // Draw Slice Label and Drag indicator
                g.setFont(juce::Font(10.0f).boldened());
                g.setColour(OpenWavLookAndFeel::textPrimary);
                g.drawText("S" + juce::String(i + 1), labelRect.removeFromLeft(20.0f), juce::Justification::centredLeft, true);

                g.setFont(juce::Font(9.0f));
                g.setColour(OpenWavLookAndFeel::textSecondary);
                g.drawText("DRAG", labelRect, juce::Justification::centredRight, true);
            }
        }
    }
    else
    {
        g.setFont(juce::Font(13.0f));
        g.setColour(OpenWavLookAndFeel::textSecondary);
        g.drawText("Select a sample to preview playback", trackBounds, juce::Justification::centred, true);

        // Draw empty slices bounds anyway
        g.setColour(OpenWavLookAndFeel::bgDark);
        g.fillRoundedRectangle(slicesBounds.toFloat(), 4.0f);
        g.setColour(OpenWavLookAndFeel::borderColour);
        g.drawRoundedRectangle(slicesBounds.toFloat(), 4.0f, 1.0f);
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
    topRow.removeFromLeft(6);

    autoSliceButton.setBounds(topRow.removeFromLeft(80).withHeight(28));
    topRow.removeFromLeft(12);

    sampleNameLabel.setBounds(topRow.removeFromLeft(220).withHeight(28));

    timeLabel.setBounds(topRow.removeFromRight(110).withHeight(28));
    topRow.removeFromRight(10);

    volumeSlider.setBounds(topRow.removeFromRight(100).withHeight(28));
}

void WaveformTransportComponent::mouseDown(const juce::MouseEvent& e)
{
    if (totalDurationSecs <= 0.0)
        return;

    // Check if clicked on a slice badge first
    for (size_t i = 0; i < sliceBadgeBounds.size(); ++i)
    {
        if (sliceBadgeBounds[i].contains(e.position.x, e.position.y))
        {
            // Set selection range to this slice and trigger playback
            double startR = sliceRatios[i];
            double endR = (i + 1 < sliceRatios.size()) ? sliceRatios[i + 1] : 1.0;
            audioEngine.setSampleRange(startR, endR);
            audioEngine.setPositionRatio(startR);
            if (!audioEngine.isPlaying())
                audioEngine.play();

            dragMode = DragMode::None;
            clickedSliceIndex = static_cast<int>(i);
            repaint();
            return;
        }
    }

    auto area = getLocalBounds().reduced(12, 8);
    area.removeFromTop(32);
    area.removeFromTop(8);
    auto waveformBounds = area.toFloat();

    if (!waveformBounds.contains(e.position.x, e.position.y))
        return;

    float mouseXRatio = (e.position.x - waveformBounds.getX()) / waveformBounds.getWidth();
    double currentRatio = juce::jlimit(0.0, 1.0, static_cast<double>(mouseXRatio));

    double startRatio = audioEngine.getSampleStartRatio();
    double endRatio = audioEngine.getSampleEndRatio();

    float startX = waveformBounds.getX() + waveformBounds.getWidth() * startRatio;
    float endX = waveformBounds.getX() + waveformBounds.getWidth() * endRatio;

    const float clickThreshold = 12.0f; // 12 pixel click zone for handles

    if (std::abs(e.position.x - startX) <= clickThreshold)
    {
        dragMode = DragMode::DraggingStart;
    }
    else if (std::abs(e.position.x - endX) <= clickThreshold)
    {
        dragMode = DragMode::DraggingEnd;
    }
    else
    {
        dragMode = DragMode::SelectingRange;
        dragStartRatio = currentRatio;
        audioEngine.setSampleRange(currentRatio, currentRatio);
    }
    clickedSliceIndex = -1;
    repaint();
}

void WaveformTransportComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (totalDurationSecs <= 0.0)
        return;

    if (clickedSliceIndex >= 0 && e.mouseWasDraggedSinceMouseDown())
    {
        int sliceIdx = clickedSliceIndex;
        clickedSliceIndex = -1; // Reset to prevent multiple triggerings
        exportAndDragSlice(sliceIdx);
        return;
    }

    if (dragMode == DragMode::None)
        return;

    auto area = getLocalBounds().reduced(12, 8);
    area.removeFromTop(32);
    area.removeFromTop(8);
    auto waveformBounds = area.toFloat();

    float mouseXRatio = (e.position.x - waveformBounds.getX()) / waveformBounds.getWidth();
    double currentRatio = juce::jlimit(0.0, 1.0, static_cast<double>(mouseXRatio));

    double startRatio = audioEngine.getSampleStartRatio();
    double endRatio = audioEngine.getSampleEndRatio();

    if (dragMode == DragMode::DraggingStart)
    {
        audioEngine.setSampleRange(currentRatio, endRatio);
    }
    else if (dragMode == DragMode::DraggingEnd)
    {
        audioEngine.setSampleRange(startRatio, currentRatio);
    }
    else if (dragMode == DragMode::SelectingRange)
    {
        double newStart = std::min(dragStartRatio, currentRatio);
        double newEnd = std::max(dragStartRatio, currentRatio);
        audioEngine.setSampleRange(newStart, newEnd);
    }
    repaint();
}

void WaveformTransportComponent::mouseUp(const juce::MouseEvent& /*e*/)
{
    clickedSliceIndex = -1;

    if (dragMode == DragMode::SelectingRange)
    {
        double startRatio = audioEngine.getSampleStartRatio();
        double endRatio = audioEngine.getSampleEndRatio();
        if (endRatio - startRatio < 0.01)
        {
            // Reset selection to full, and seek to click position
            audioEngine.setSampleRange(0.0, 1.0);
            audioEngine.setPositionRatio(startRatio);
        }
        else
        {
            // Snap playhead to the selection start
            audioEngine.setPositionRatio(startRatio);
        }
    }
    else if (dragMode == DragMode::DraggingStart)
    {
        audioEngine.setPositionRatio(audioEngine.getSampleStartRatio());
    }

    dragMode = DragMode::None;
    repaint();
}

void WaveformTransportComponent::mouseDoubleClick(const juce::MouseEvent& /*e*/)
{
    if (totalDurationSecs <= 0.0)
        return;

    // Reset selection to full sample
    audioEngine.setSampleRange(0.0, 1.0);
    audioEngine.setPositionRatio(0.0);
    repaint();
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
    sliceRatios.clear();
    sliceBadgeBounds.clear();
    clickedSliceIndex = -1;
    repaint();
}

void WaveformTransportComponent::sliderValueChanged(juce::Slider* slider)
{
    if (slider == &volumeSlider)
    {
        audioEngine.setGain(static_cast<float>(volumeSlider.getValue()));
    }
}

void WaveformTransportComponent::runAutoSlice()
{
    juce::AudioBuffer<float> buffer;
    double sampleRate = 44100.0;
    if (!audioEngine.getAudioBufferCopy(buffer, sampleRate))
        return;

    sliceRatios.clear();
    sliceRatios.push_back(0.0); // Always start at 0.0

    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();
    if (numSamples <= 0 || numChannels <= 0)
    {
        repaint();
        return;
    }

    // 512 sample blocks (~11.6ms)
    int blockSize = 512;
    int numBlocks = numSamples / blockSize;

    std::vector<float> energy(numBlocks, 0.0f);
    for (int b = 0; b < numBlocks; ++b)
    {
        float sum = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            for (int s = 0; s < blockSize; ++s)
            {
                float val = buffer.getSample(ch, b * blockSize + s);
                sum += val * val;
            }
        }
        energy[b] = std::sqrt(sum / (blockSize * numChannels));
    }

    // Onset detection using a moving threshold
    int windowSize = 15;
    float multiplier = 1.6f;
    int minDistanceBlocks = static_cast<int>(0.08 * sampleRate / blockSize);
    int lastOnsetBlock = -minDistanceBlocks;

    for (int i = windowSize; i < numBlocks - 1; ++i)
    {
        float sum = 0.0f;
        for (int w = i - windowSize; w < i; ++w)
            sum += energy[w];
        float avg = sum / windowSize;

        if (energy[i] > avg * multiplier && energy[i] > energy[i - 1] && energy[i] > energy[i + 1])
        {
            if (i - lastOnsetBlock >= minDistanceBlocks)
            {
                double ratio = static_cast<double>(i * blockSize) / static_cast<double>(numSamples);
                sliceRatios.push_back(ratio);
                lastOnsetBlock = i;
            }
        }
    }

    // Fallback if no transients found (equal division to 8 slices)
    if (sliceRatios.size() <= 1)
    {
        sliceRatios.clear();
        for (int i = 0; i < 8; ++i)
        {
            sliceRatios.push_back(static_cast<double>(i) / 8.0);
        }
    }

    // Sort and clean duplicates
    std::sort(sliceRatios.begin(), sliceRatios.end());
    sliceRatios.erase(std::unique(sliceRatios.begin(), sliceRatios.end()), sliceRatios.end());

    repaint();
}

void WaveformTransportComponent::exportAndDragSlice(int sliceIndex)
{
    if (sliceIndex < 0 || sliceIndex >= static_cast<int>(sliceRatios.size()))
        return;

    juce::AudioBuffer<float> buffer;
    double sampleRate = 44100.0;
    if (!audioEngine.getAudioBufferCopy(buffer, sampleRate))
        return;

    int numSamples = buffer.getNumSamples();
    double startR = sliceRatios[sliceIndex];
    double endR = (sliceIndex + 1 < static_cast<int>(sliceRatios.size())) ? sliceRatios[sliceIndex + 1] : 1.0;

    int startSample = static_cast<int>(startR * numSamples);
    int endSample = static_cast<int>(endR * numSamples);
    int numSliceSamples = endSample - startSample;

    if (numSliceSamples <= 0)
        return;

    juce::File originalFile = audioEngine.getCurrentFile();
    juce::String sampleName = originalFile.existsAsFile() ? originalFile.getFileNameWithoutExtension() : "sample";

    static int dragCounter = 0;
    juce::File tempDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("OWMB_Temp");
    tempDir.createDirectory();
    juce::File sliceFile = tempDir.getChildFile(sampleName + "_Slice_" + juce::String(sliceIndex + 1) + "_" + juce::String(++dragCounter) + ".wav");

    bool success = false;
    {
        sliceFile.deleteFile();
        std::unique_ptr<juce::FileOutputStream> outStream(sliceFile.createOutputStream());
        if (outStream != nullptr)
        {
            juce::WavAudioFormat wavFormat;
            std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(
                outStream.get(),
                sampleRate,
                buffer.getNumChannels(),
                16,
                {},
                0
            ));

            if (writer != nullptr)
            {
                outStream.release(); // Writer took ownership
                writer->writeFromAudioSampleBuffer(buffer, startSample, numSliceSamples);
                success = true;
            }
        }
    } // File is written, flushed, closed, and unlocked here!

    if (success)
    {
        juce::StringArray filesToDrag;
        filesToDrag.add(sliceFile.getFullPathName());
        juce::DragAndDropContainer::performExternalDragDropOfFiles(filesToDrag, false);
    }
}

} // namespace openwav
