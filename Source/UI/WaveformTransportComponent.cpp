#include "WaveformTransportComponent.h"
#include "OpenWavLookAndFeel.h"
#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
#endif


namespace openwav
{

SlicesGridComponent::SlicesGridComponent(AudioEngine& engine, std::function<void(int)> onSliceDragged)
    : audioEngine(engine), onSliceDragged(onSliceDragged)
{}

void SlicesGridComponent::paint(juce::Graphics& g)
{
    g.fillAll(OpenWavLookAndFeel::bgDark);

    int numSlices = static_cast<int>(sliceRatios.size());
    if (numSlices == 0)
    {
        g.setFont(juce::Font(13.0f));
        g.setColour(OpenWavLookAndFeel::textSecondary);
        g.drawText("No slices. Click the 'Slice' button to automatically chop this sample.", getLocalBounds(), juce::Justification::centred, true);
        return;
    }

    sliceBadgeBounds.clear();
    float cardWidth = 100.0f;
    float cardGap = 8.0f;
    float startY = 4.0f;
    float cardHeight = getHeight() - 8.0f;

    juce::AudioBuffer<float> buffer;
    double sampleRate = 44100.0;
    bool hasBuffer = audioEngine.getAudioBufferCopy(buffer, sampleRate);

    for (int i = 0; i < numSlices; ++i)
    {
        float cardX = i * (cardWidth + cardGap) + 4.0f;
        juce::Rectangle<float> cardRect(cardX, startY, cardWidth, cardHeight);
        sliceBadgeBounds.push_back(cardRect);

        // Draw card background
        g.setColour(OpenWavLookAndFeel::bgHeader);
        g.fillRoundedRectangle(cardRect, 4.0f);

        double startR = sliceRatios[i];
        double endR = (i + 1 < numSlices) ? sliceRatios[i + 1] : 1.0;

        double currentStart = audioEngine.getSampleStartRatio();
        double currentEnd = audioEngine.getSampleEndRatio();
        bool isSelectedSlice = (std::abs(currentStart - startR) < 0.001 && std::abs(currentEnd - endR) < 0.001);

        if (isSelectedSlice)
        {
            g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.15f));
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
        auto miniWaveBounds = cardRect.reduced(6.0f, 6.0f);
        auto labelRect = miniWaveBounds.removeFromBottom(15.0f);
        miniWaveBounds.removeFromBottom(2.0f); // Spacing

        if (hasBuffer && buffer.getNumSamples() > 0 && totalDurationSecs > 0.0)
        {
            g.setColour(isSelectedSlice ? OpenWavLookAndFeel::accentCyan : OpenWavLookAndFeel::textSecondary.withAlpha(0.6f));
            
            int sliceStartSample = static_cast<int>(startR * buffer.getNumSamples());
            int sliceEndSample = static_cast<int>(endR * buffer.getNumSamples());
            int sliceLen = sliceEndSample - sliceStartSample;
            
            if (sliceLen > 0)
            {
                juce::Path wavePath;
                int miniWidth = static_cast<int>(miniWaveBounds.getWidth());
                float midY = miniWaveBounds.getCentreY();
                float halfH = miniWaveBounds.getHeight() * 0.5f;

                for (int x = 0; x < miniWidth; ++x)
                {
                    double xRatioStart = static_cast<double>(x) / miniWidth;
                    double xRatioEnd = static_cast<double>(x + 1) / miniWidth;

                    int smpStart = sliceStartSample + static_cast<int>(xRatioStart * sliceLen);
                    int smpEnd = sliceStartSample + static_cast<int>(xRatioEnd * sliceLen);
                    smpEnd = juce::jlimit(smpStart + 1, sliceEndSample, smpEnd);

                    float minVal = 0.0f;
                    float maxVal = 0.0f;
                    
                    int numChannels = buffer.getNumChannels();
                    for (int ch = 0; ch < numChannels; ++ch)
                    {
                        auto range = buffer.findMinMax(ch, smpStart, smpEnd - smpStart);
                        if (range.getStart() < minVal) minVal = range.getStart();
                        if (range.getEnd() > maxVal) maxVal = range.getEnd();
                    }

                    float pyMin = midY + minVal * halfH;
                    float pyMax = midY + maxVal * halfH;
                    
                    if (x == 0)
                    {
                        wavePath.startNewSubPath(miniWaveBounds.getX() + x, pyMin);
                        wavePath.lineTo(miniWaveBounds.getX() + x, pyMax);
                    }
                    else
                    {
                        wavePath.lineTo(miniWaveBounds.getX() + x, pyMin);
                        wavePath.lineTo(miniWaveBounds.getX() + x, pyMax);
                    }
                }
                g.strokePath(wavePath, juce::PathStrokeType(1.0f));
            }
        }

        // Draw Card Bottom Label Area
        g.setFont(juce::Font(10.0f).boldened());
        g.setColour(OpenWavLookAndFeel::textPrimary);
        g.drawText("S" + juce::String(i + 1), labelRect.removeFromLeft(20.0f), juce::Justification::centredLeft, true);

        g.setFont(juce::Font(9.0f));
        g.setColour(OpenWavLookAndFeel::textSecondary);
        g.drawText("DRAG", labelRect, juce::Justification::centredRight, true);
    }
}

void SlicesGridComponent::mouseDown(const juce::MouseEvent& e)
{
    clickedSliceIndex = -1;
    for (int i = 0; i < static_cast<int>(sliceBadgeBounds.size()); ++i)
    {
        if (sliceBadgeBounds[i].contains(e.position))
        {
            clickedSliceIndex = i;
            
            // Set playback selection range to this slice
            double startR = sliceRatios[i];
            double endR = (i + 1 < static_cast<int>(sliceRatios.size())) ? sliceRatios[i + 1] : 1.0;
            audioEngine.setSampleRange(startR, endR);
            audioEngine.setPositionRatio(startR);
            if (!audioEngine.isPlaying())
                audioEngine.play();
            
            // Trigger parent component (WaveformTransportComponent) to repaint
            if (auto* vp = getParentComponent())
            {
                if (auto* parentComp = vp->getParentComponent())
                {
                    parentComp->repaint();
                }
            }
            repaint();
            return;
        }
    }
}

void SlicesGridComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (clickedSliceIndex >= 0 && e.mouseWasDraggedSinceMouseDown())
    {
        int sliceIdx = clickedSliceIndex;
        clickedSliceIndex = -1; // Reset to prevent multiple triggerings
        if (onSliceDragged != nullptr)
            onSliceDragged(sliceIdx);
    }
}

void SlicesGridComponent::updateSlices(const std::vector<double>& ratios, double duration, int viewportWidth, int viewportHeight)
{
    sliceRatios = ratios;
    totalDurationSecs = duration;
    sliceBadgeBounds.clear();
    
    int numSlices = static_cast<int>(sliceRatios.size());
    if (numSlices > 0)
    {
        float cardWidth = 100.0f;
        float cardGap = 8.0f;
        float totalWidth = numSlices * cardWidth + (numSlices - 1) * cardGap + 8.0f;
        setSize(std::max(viewportWidth, static_cast<int>(totalWidth)), viewportHeight);
    }
    else
    {
        setSize(viewportWidth, viewportHeight);
    }
    repaint();
}

WaveformTransportComponent::WaveformTransportComponent(AudioEngine& engine)
    : audioEngine(engine),
      slicesGrid(engine, [this](int idx) { exportAndDragSlice(idx); })
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

    // Normalize Button
    normalizeButton.onClick = [this] {
        if (audioEngine.normalizeLoadedSample())
        {
            int vpHeight = slicesViewport.getHeight() - slicesViewport.getScrollBarThickness();
            slicesGrid.updateSlices(sliceRatios, totalDurationSecs, slicesViewport.getWidth(), vpHeight);
            repaint();
        }
    };
    addAndMakeVisible(normalizeButton);

    // One Shot Button
    oneShotButton.setClickingTogglesState(true);
    bool osEnabled = audioEngine.isOneShotEnabled();
    oneShotButton.setToggleState(osEnabled, juce::dontSendNotification);
    oneShotButton.setButtonText(osEnabled ? "1-Shot: ON" : "1-Shot: OFF");
    oneShotButton.onClick = [this] {
        bool enabled = oneShotButton.getToggleState();
        audioEngine.setOneShotEnabled(enabled);
        oneShotButton.setButtonText(enabled ? "1-Shot: ON" : "1-Shot: OFF");
    };
    addAndMakeVisible(oneShotButton);

    // Volume Slider
    volumeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    volumeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    volumeSlider.setRange(0.0, 1.0, 0.01);
    volumeSlider.setValue(audioEngine.getGain());
    volumeSlider.addListener(this);
    addAndMakeVisible(volumeSlider);

    // Time Label
    timeLabel.setFont(juce::Font(12.0f).boldened());
    timeLabel.setJustificationType(juce::Justification::centredRight);
    timeLabel.setText("00:00 / 00:00", juce::dontSendNotification);
    addAndMakeVisible(timeLabel);

    // Sample Name Label
    sampleNameLabel.setFont(juce::Font(13.0f).boldened());
    sampleNameLabel.setText("No sample loaded", juce::dontSendNotification);
    addAndMakeVisible(sampleNameLabel);

    // Viewport Setup
    slicesViewport.setScrollBarsShown(false, true, false, false);
    slicesViewport.setViewedComponent(&slicesGrid, false);
    addAndMakeVisible(slicesViewport);

    lookAndFeelChanged();

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

    // Reserve 92px for Slices section at bottom
    auto slicesBounds = area.removeFromBottom(92);
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

        int numPixels = static_cast<int>(totalWidth);
        float startX = waveformRect.getX();

        float progressRatio = juce::jlimit(0.0f, 1.0f, static_cast<float>(currentPositionSecs / totalDurationSecs));
        float playheadX = waveformRect.getX() + totalWidth * progressRatio;

        double startRatio = audioEngine.getSampleStartRatio();
        double endRatio = audioEngine.getSampleEndRatio();

        float selStartX = waveformRect.getX() + totalWidth * startRatio;
        float selEndX = waveformRect.getX() + totalWidth * endRatio;

        auto& thumbnail = audioEngine.getThumbnail();
        int numChannels = audioEngine.getNumChannels();
        if (numChannels <= 0)
            numChannels = thumbnail.getNumChannels();

        float halfHeight = (waveformRect.getHeight() - 4.0f) * 0.5f;
        float centerY = waveformRect.getCentreY();

        // Baseline zero line
        g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.3f));
        g.drawHorizontalLine(static_cast<int>(centerY), waveformRect.getX(), waveformRect.getRight());

        // Zero-lock peak snapshot retrieval
        const auto peaks = audioEngine.getWaveformPeaks();
        const int numPeakPoints = peaks.numPoints;

        for (int x = 0; x < numPixels; ++x)
        {
            float pixelX = startX + static_cast<float>(x);
            double startRatioCol = static_cast<double>(x) / static_cast<double>(numPixels);
            double endRatioCol = static_cast<double>(x + 1) / static_cast<double>(numPixels);

            float lMin = 0.0f, lMax = 0.0f;
            float rMin = 0.0f, rMax = 0.0f;

            if (numPeakPoints > 0 && peaks.minLeft.size() == static_cast<size_t>(numPeakPoints)
                                  && peaks.maxLeft.size() == static_cast<size_t>(numPeakPoints)
                                  && peaks.minRight.size() == static_cast<size_t>(numPeakPoints)
                                  && peaks.maxRight.size() == static_cast<size_t>(numPeakPoints))
            {
                int pStart = juce::jlimit(0, numPeakPoints - 1, static_cast<int>(startRatioCol * numPeakPoints));
                int pEnd = juce::jlimit(pStart + 1, numPeakPoints, static_cast<int>(endRatioCol * numPeakPoints));

                for (int p = pStart; p < pEnd; ++p)
                {
                    size_t idx = static_cast<size_t>(p);
                    if (idx < peaks.minLeft.size() && idx < peaks.maxLeft.size() && idx < peaks.minRight.size() && idx < peaks.maxRight.size())
                    {
                        if (peaks.minLeft[idx] < lMin) lMin = peaks.minLeft[idx];
                        if (peaks.maxLeft[idx] > lMax) lMax = peaks.maxLeft[idx];
                        if (peaks.minRight[idx] < rMin) rMin = peaks.minRight[idx];
                        if (peaks.maxRight[idx] > rMax) rMax = peaks.maxRight[idx];
                    }
                }
            }
            else if (numChannels > 0)
            {
                double unscaledDuration = thumbnail.getTotalLength();
                if (unscaledDuration > 0.0)
                {
                    double startTime = startRatioCol * unscaledDuration;
                    double endTime = endRatioCol * unscaledDuration;
                    thumbnail.getApproximateMinMax(startTime, endTime, 0, lMin, lMax);
                    if (numChannels >= 2)
                        thumbnail.getApproximateMinMax(startTime, endTime, 1, rMin, rMax);
                    else
                    {
                        rMin = lMin;
                        rMax = lMax;
                    }
                }
            }

            // High-precision linear peak dynamic height calculation
            float lAbs = std::max(std::abs(lMin), std::abs(lMax));
            float rAbs = std::max(std::abs(rMin), std::abs(rMax));

            float lHeight = (lAbs > 0.001f) ? std::max(1.0f, lAbs * halfHeight) : (lMax == 0.0f && lMin == 0.0f ? 0.0f : 1.0f);
            float rHeight = (rAbs > 0.001f) ? std::max(1.0f, rAbs * halfHeight) : (rMax == 0.0f && rMin == 0.0f ? 0.0f : 1.0f);

            float lTopY = centerY - lHeight;
            float rBottomY = centerY + rHeight;

            bool inSelection = (pixelX >= selStartX && pixelX <= selEndX);
            bool isPlayed = (pixelX <= playheadX);

            if (inSelection)
            {
                if (isPlayed)
                    g.setColour(OpenWavLookAndFeel::accentCyan);
                else
                    g.setColour(OpenWavLookAndFeel::textPrimary.withAlpha(0.85f));
            }
            else
            {
                g.setColour(OpenWavLookAndFeel::textSecondary.withAlpha(0.22f));
            }

            if (lHeight > 0.0f)
                g.fillRect(juce::Rectangle<float>(pixelX, lTopY, 1.0f, std::max(1.0f, centerY - lTopY)));
            if (rHeight > 0.0f)
                g.fillRect(juce::Rectangle<float>(pixelX, centerY, 1.0f, std::max(1.0f, rBottomY - centerY)));
        }

        // Draw darker overlay for the non-selected sections
        g.setColour(juce::Colours::black.withAlpha(0.40f));
        if (selStartX > waveformRect.getX())
        {
            g.fillRect(waveformRect.getX(), waveformRect.getY(), selStartX - waveformRect.getX(), waveformRect.getHeight());
        }
        if (selEndX < waveformRect.getRight())
        {
            g.fillRect(selEndX, waveformRect.getY(), waveformRect.getRight() - selEndX, waveformRect.getHeight());
        }

        // Draw Selection Range Boundary Handles & Lines
        g.setColour(OpenWavLookAndFeel::accentCyan);

        // Start handle line & top badge
        g.drawVerticalLine(static_cast<int>(selStartX), waveformRect.getY(), waveformRect.getBottom());
        g.fillRoundedRectangle(selStartX - 4.0f, waveformRect.getY() - 2.0f, 8.0f, 8.0f, 2.0f);

        // End handle line & top badge
        g.drawVerticalLine(static_cast<int>(selEndX), waveformRect.getY(), waveformRect.getBottom());
        g.fillRoundedRectangle(selEndX - 4.0f, waveformRect.getY() - 2.0f, 8.0f, 8.0f, 2.0f);

        // Draw slice divider lines on main waveform
        for (size_t i = 1; i < sliceRatios.size(); ++i)
        {
            float sliceStartX = waveformRect.getX() + totalWidth * sliceRatios[i];
            g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.7f));
            float dashLengths[] = { 4.0f, 4.0f };
            g.drawDashedLine(juce::Line<float>(sliceStartX, waveformRect.getY(), sliceStartX, waveformRect.getBottom()), dashLengths, 2, 1.0f);
        }

        // Draw Playhead Line & Glowing Playhead Knob
        g.setColour(OpenWavLookAndFeel::accentCyan.brighter(0.5f));
        g.drawLine(playheadX, trackBounds.getY() + 2.0f, playheadX, trackBounds.getBottom() - 2.0f, 2.0f);
        g.setColour(juce::Colours::white);
        g.fillEllipse(playheadX - 6.0f, trackBounds.getCentreY() - 6.0f, 12.0f, 12.0f);
        g.setColour(OpenWavLookAndFeel::accentCyan);
        g.drawEllipse(playheadX - 6.0f, trackBounds.getCentreY() - 6.0f, 12.0f, 12.0f, 1.5f);
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

    playPauseButton.setBounds(topRow.removeFromLeft(88).withHeight(28));
    topRow.removeFromLeft(6);

    stopButton.setBounds(topRow.removeFromLeft(82).withHeight(28));
    topRow.removeFromLeft(6);

    loopButton.setBounds(topRow.removeFromLeft(82).withHeight(28));
    topRow.removeFromLeft(6);

    autoPlayButton.setBounds(topRow.removeFromLeft(82).withHeight(28));
    topRow.removeFromLeft(6);

    autoSliceButton.setBounds(topRow.removeFromLeft(88).withHeight(28));
    topRow.removeFromLeft(6);

    normalizeButton.setBounds(topRow.removeFromLeft(95).withHeight(28));
    topRow.removeFromLeft(6);

    oneShotButton.setBounds(topRow.removeFromLeft(82).withHeight(28));
    topRow.removeFromLeft(12);

    volumeSlider.setBounds(topRow.removeFromRight(100).withHeight(28));
    topRow.removeFromRight(10);

    timeLabel.setBounds(topRow.removeFromRight(110).withHeight(28));
    topRow.removeFromRight(12);

    sampleNameLabel.setBounds(topRow.withHeight(28));

    area.removeFromTop(8); // Spacing gap
    auto slicesBounds = area.removeFromBottom(92);
    slicesViewport.setBounds(slicesBounds);

    int vpHeight = slicesBounds.getHeight() - slicesViewport.getScrollBarThickness();
    slicesGrid.updateSlices(sliceRatios, totalDurationSecs, slicesViewport.getWidth(), vpHeight);
}

void WaveformTransportComponent::mouseDown(const juce::MouseEvent& e)
{
    if (totalDurationSecs <= 0.0)
        return;

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
    repaint();
}

void WaveformTransportComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (totalDurationSecs <= 0.0)
        return;

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
    int vpHeight = slicesViewport.getHeight() - slicesViewport.getScrollBarThickness();
    slicesGrid.updateSlices(sliceRatios, totalDurationSecs, slicesViewport.getWidth(), vpHeight);
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
        int vpHeight = slicesViewport.getHeight() - slicesViewport.getScrollBarThickness();
        slicesGrid.updateSlices(sliceRatios, totalDurationSecs, slicesViewport.getWidth(), vpHeight);
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

    int vpHeight = slicesViewport.getHeight() - slicesViewport.getScrollBarThickness();
    slicesGrid.updateSlices(sliceRatios, totalDurationSecs, slicesViewport.getWidth(), vpHeight);
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
        juce::MessageManager::callAsync([filesToDrag] {
            juce::DragAndDropContainer::performExternalDragDropOfFiles(filesToDrag, false);
        });
    }
}

void WaveformTransportComponent::lookAndFeelChanged()
{
    timeLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    sampleNameLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textPrimary);
}

void WaveformTransportComponent::togglePlay()
{
    playPauseButton.triggerClick();
}

void WaveformTransportComponent::toggleLoop()
{
    loopButton.triggerClick();
}

void WaveformTransportComponent::triggerSlice()
{
    autoSliceButton.triggerClick();
}

} // namespace openwav
