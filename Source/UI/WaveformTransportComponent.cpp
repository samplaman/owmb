#include "WaveformTransportComponent.h"
#include "OpenWavLookAndFeel.h"
#include "SliceConfigComponent.h"
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

juce::Rectangle<float> SlicesGridComponent::getSliceCardBounds(int index) const
{
    float cardWidth = 100.0f;
    float cardGap = 8.0f;
    float startY = 4.0f;
    float cardHeight = std::max(10.0f, static_cast<float>(getHeight()) - 8.0f);
    float cardX = index * (cardWidth + cardGap) + 4.0f;
    return juce::Rectangle<float>(cardX, startY, cardWidth, cardHeight);
}

int SlicesGridComponent::getSliceIndexAt(juce::Point<float> pos) const
{
    int numSlices = static_cast<int>(sliceRatios.size());
    for (int i = 0; i < numSlices; ++i)
    {
        if (getSliceCardBounds(i).contains(pos))
            return i;
    }
    return -1;
}

void SlicesGridComponent::setSelectedSliceIndex(int index)
{
    if (selectedSliceIndex != index)
    {
        selectedSliceIndex = index;
        repaint();
    }
}

void SlicesGridComponent::paint(juce::Graphics& g)
{
    g.fillAll(OpenWavLookAndFeel::bgDark);

    int numSlices = static_cast<int>(sliceRatios.size());
    if (numSlices == 0)
        return;

    juce::AudioBuffer<float> buffer;
    double sampleRate = 44100.0;
    bool hasBuffer = audioEngine.getAudioBufferCopy(buffer, sampleRate);

    double currentStart = audioEngine.getSampleStartRatio();
    double currentEnd = audioEngine.getSampleEndRatio();

    for (int i = 0; i < numSlices; ++i)
    {
        auto cardRect = getSliceCardBounds(i);

        double startR = sliceRatios[i];
        double endR = (i + 1 < numSlices) ? sliceRatios[i + 1] : 1.0;

        bool isSelectedSlice = (selectedSliceIndex == i);
        bool isHovered = (hoveredSliceIndex == i);

        // Draw card background
        if (isSelectedSlice)
        {
            g.setColour(OpenWavLookAndFeel::bgHeader.brighter(0.08f));
            g.fillRoundedRectangle(cardRect, 5.0f);

            // Vibrant glow fill for selected state
            g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(isHovered ? 0.25f : 0.18f));
            g.fillRoundedRectangle(cardRect, 5.0f);

            // Glowing cyan border
            g.setColour(OpenWavLookAndFeel::accentCyan);
            g.drawRoundedRectangle(cardRect, 5.0f, 1.8f);
        }
        else if (isHovered)
        {
            g.setColour(OpenWavLookAndFeel::bgHeader.brighter(0.06f));
            g.fillRoundedRectangle(cardRect, 5.0f);

            // Subtle cyan tint on hover
            g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.08f));
            g.fillRoundedRectangle(cardRect, 5.0f);

            // Highlighted border on hover
            g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.65f));
            g.drawRoundedRectangle(cardRect, 5.0f, 1.2f);
        }
        else
        {
            g.setColour(OpenWavLookAndFeel::bgHeader);
            g.fillRoundedRectangle(cardRect, 5.0f);
            g.setColour(OpenWavLookAndFeel::borderColour);
            g.drawRoundedRectangle(cardRect, 5.0f, 1.0f);
        }

        // Draw a mini waveform inside the card
        auto miniWaveBounds = cardRect.reduced(6.0f, 6.0f);
        auto labelRect = miniWaveBounds.removeFromBottom(15.0f);
        miniWaveBounds.removeFromBottom(2.0f); // Spacing

        if (hasBuffer && buffer.getNumSamples() > 0 && totalDurationSecs > 0.0)
        {
            if (isSelectedSlice)
                g.setColour(OpenWavLookAndFeel::accentCyan);
            else if (isHovered)
                g.setColour(OpenWavLookAndFeel::textPrimary);
            else
                g.setColour(OpenWavLookAndFeel::textSecondary.withAlpha(0.65f));

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

                    float pyMin = juce::jlimit(miniWaveBounds.getY(), miniWaveBounds.getBottom(), midY + minVal * halfH);
                    float pyMax = juce::jlimit(miniWaveBounds.getY(), miniWaveBounds.getBottom(), midY + maxVal * halfH);

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
        if (isSelectedSlice)
            g.setColour(OpenWavLookAndFeel::accentCyan);
        else if (isHovered)
            g.setColour(OpenWavLookAndFeel::textPrimary);
        else
            g.setColour(OpenWavLookAndFeel::textSecondary);

        g.drawText("S" + juce::String(i + 1), labelRect.removeFromLeft(24.0f), juce::Justification::centredLeft, true);

        g.setFont(juce::Font(9.0f).boldened());
        if (isSelectedSlice)
            g.setColour(OpenWavLookAndFeel::accentCyan.brighter(0.2f));
        else if (isHovered)
            g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.85f));
        else
            g.setColour(OpenWavLookAndFeel::textSecondary.withAlpha(0.6f));

        g.drawText("DRAG", labelRect, juce::Justification::centredRight, true);
    }
}

void SlicesGridComponent::mouseMove(const juce::MouseEvent& e)
{
    int hoverIdx = getSliceIndexAt(e.position);
    if (hoverIdx != hoveredSliceIndex)
    {
        hoveredSliceIndex = hoverIdx;
        repaint();
    }
    if (hoverIdx >= 0)
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    else
        setMouseCursor(juce::MouseCursor::NormalCursor);
}

void SlicesGridComponent::mouseEnter(const juce::MouseEvent& e)
{
    mouseMove(e);
}

void SlicesGridComponent::mouseExit(const juce::MouseEvent& /*e*/)
{
    if (hoveredSliceIndex != -1)
    {
        hoveredSliceIndex = -1;
        repaint();
    }
    setMouseCursor(juce::MouseCursor::NormalCursor);
}

void SlicesGridComponent::mouseDown(const juce::MouseEvent& e)
{
    clickedSliceIndex = -1;
    int idx = getSliceIndexAt(e.position);
    if (idx >= 0)
    {
        clickedSliceIndex = idx;
        selectedSliceIndex = idx;

        double startR = sliceRatios[idx];
        double endR = (idx + 1 < static_cast<int>(sliceRatios.size())) ? sliceRatios[idx + 1] : 1.0;

        if (onSliceSelected != nullptr)
        {
            onSliceSelected(idx, startR, endR);
        }
        else
        {
            audioEngine.setSampleRange(startR, endR);
            audioEngine.setPositionRatio(startR);
            if (!audioEngine.isPlaying())
                audioEngine.play();
        }

        repaint();
        if (auto* vp = getParentComponent())
        {
            vp->repaint();
            if (auto* parentComp = vp->getParentComponent())
                parentComp->repaint();
        }
    }
}

void SlicesGridComponent::mouseUp(const juce::MouseEvent& /*e*/)
{
    clickedSliceIndex = -1;
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

    int numSlices = static_cast<int>(sliceRatios.size());
    if (selectedSliceIndex >= numSlices)
        selectedSliceIndex = -1;

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

//==============================================================================
BpmControlComponent::BpmControlComponent(AudioEngine& engine)
    : audioEngine(engine)
{
    minusBtn.setButtonText("-");
    minusBtn.setTooltip("Decrease Tempo (0.5 BPM)");
    minusBtn.onClick = [this] {
        if (!isSynced)
            setBpm(currentBpm - 0.5, true);
    };
    addAndMakeVisible(minusBtn);

    plusBtn.setButtonText("+");
    plusBtn.setTooltip("Increase Tempo (0.5 BPM)");
    plusBtn.onClick = [this] {
        if (!isSynced)
            setBpm(currentBpm + 0.5, true);
    };
    addAndMakeVisible(plusBtn);

    tempoLabel.setFont(juce::Font(12.5f).boldened());
    tempoLabel.setJustificationType(juce::Justification::centred);
    tempoLabel.setEditable(false, true, false);
    tempoLabel.addListener(this);
    tempoLabel.setTooltip("Double-click to type tempo or drag up/down / scroll");
    addAndMakeVisible(tempoLabel);

    setBpm(audioEngine.getEffectiveBpm(), false);
    lookAndFeelChanged();
}

void BpmControlComponent::lookAndFeelChanged()
{
    tempoLabel.setColour(juce::Label::textColourId, isSynced ? OpenWavLookAndFeel::accentCyan : OpenWavLookAndFeel::textPrimary);
    repaint();
}

void BpmControlComponent::setBpm(double bpm, bool sendNotification)
{
    double clamped = juce::jlimit(20.0, 300.0, bpm);
    currentBpm = clamped;
    tempoLabel.setText(juce::String(clamped, 1) + " BPM", juce::dontSendNotification);
    if (sendNotification && onBpmChanged)
        onBpmChanged(clamped);
}

void BpmControlComponent::setHostSynced(bool synced)
{
    isSynced = synced;
    minusBtn.setEnabled(!synced);
    plusBtn.setEnabled(!synced);
    lookAndFeelChanged();
}

void BpmControlComponent::labelTextChanged(juce::Label* label)
{
    if (label == &tempoLabel && !isSynced)
    {
        double val = tempoLabel.getText().upToFirstOccurrenceOf(" ", false, false).getDoubleValue();
        if (val >= 20.0 && val <= 300.0)
        {
            setBpm(val, true);
        }
        else
        {
            setBpm(currentBpm, false);
        }
    }
}

void BpmControlComponent::editorShown(juce::Label* label, juce::TextEditor& editor)
{
    editor.setText(juce::String(currentBpm, 1));
    editor.selectAll();
}

void BpmControlComponent::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour(OpenWavLookAndFeel::bgCard.darker(0.15f));
    g.fillRoundedRectangle(b, 5.0f);

    if (isSynced)
    {
        g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.65f));
        g.drawRoundedRectangle(b.reduced(0.5f), 5.0f, 1.2f);
    }
    else
    {
        g.setColour(OpenWavLookAndFeel::borderColour);
        g.drawRoundedRectangle(b.reduced(0.5f), 5.0f, 1.0f);
    }
}

void BpmControlComponent::resized()
{
    auto area = getLocalBounds();
    minusBtn.setBounds(area.removeFromLeft(22).reduced(1, 2));
    plusBtn.setBounds(area.removeFromRight(22).reduced(1, 2));
    tempoLabel.setBounds(area);
}

void BpmControlComponent::mouseDown(const juce::MouseEvent& e)
{
    if (isSynced) return;
    dragStartBpm = currentBpm;
    dragStartPos = e.getPosition();
}

void BpmControlComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (isSynced) return;
    int dy = dragStartPos.y - e.y;
    double delta = (e.mods.isShiftDown() ? 0.1 : 0.5) * dy;
    setBpm(dragStartBpm + delta, true);
}

void BpmControlComponent::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (isSynced) return;
    if (wheel.deltaY != 0)
    {
        double step = (e.mods.isShiftDown() ? 0.1 : 0.5);
        double delta = (wheel.deltaY > 0 ? step : -step);
        setBpm(currentBpm + delta, true);
    }
}

//==============================================================================
WaveformTransportComponent::WaveformTransportComponent(AudioEngine& engine)
    : audioEngine(engine),
      bpmControl(engine),
      slicesGrid(engine, [this](int idx) { exportAndDragSlice(idx); })
{
    slicesGrid.onSliceSelected = [this](int idx, double startR, double endR) {
        setActiveSliceIndex(idx);
        audioEngine.setSampleRange(startR, endR);
        audioEngine.setPositionRatio(startR);
        if (!audioEngine.isPlaying())
            audioEngine.play();
        repaint();
        slicesGrid.repaint();
    };

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
    autoSliceButton.onClick = [this] { openSliceConfigWindow(); };
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

    // Host Sync Button (placed after Normalize along with BPM)
    syncButton.setClickingTogglesState(true);
    syncButton.setToggleState(audioEngine.isHostSyncEnabled(), juce::dontSendNotification);
    syncButton.setTooltip("Toggle Host DAW Sync vs Independent Transport");
    syncButton.onClick = [this] {
        audioEngine.setHostSyncEnabled(syncButton.getToggleState());
    };
    addAndMakeVisible(syncButton);

    // BPM Control Component (placed after Normalize along with SYNC)
    bpmControl.setHostSynced(audioEngine.isHostSyncEnabled());
    bpmControl.setBpm(audioEngine.getEffectiveBpm(), false);
    bpmControl.onBpmChanged = [this](double bpm) {
        audioEngine.setInternalBpm(bpm);
    };
    addAndMakeVisible(bpmControl);

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

    // Only reserve space for Slices section when slices exist
    if (!sliceRatios.empty())
    {
        area.removeFromBottom(92);
        area.removeFromBottom(8); // Gap between waveform and slices section
    }

    auto trackBounds = area.toFloat();

    // Track Background Card
    g.setColour(OpenWavLookAndFeel::bgDark);
    g.fillRoundedRectangle(trackBounds, 6.0f);
    g.setColour(OpenWavLookAndFeel::borderColour);
    double dur = totalDurationSecs;
    if (dur <= 0.0)
        dur = audioEngine.getTotalLengthSeconds();
    if (dur <= 0.0 && audioEngine.getThumbnail().getTotalLength() > 0.0)
        dur = audioEngine.getThumbnail().getTotalLength();

    if (dur > 0.0)
    {
        totalDurationSecs = dur;
        auto waveformRect = trackBounds.reduced(8.0f, 8.0f);
        float totalWidth = waveformRect.getWidth();

        int numPixels = static_cast<int>(totalWidth);
        float startX = waveformRect.getX();

        float progressRatio = juce::jlimit(0.0f, 1.0f, static_cast<float>(currentPositionSecs / totalDurationSecs));
        float playheadX = waveformRect.getX() + totalWidth * progressRatio;

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

            float lHeight = (lAbs > 0.001f) ? std::min(halfHeight, std::max(1.0f, lAbs * halfHeight)) : (lMax == 0.0f && lMin == 0.0f ? 0.0f : 1.0f);
            float rHeight = (rAbs > 0.001f) ? std::min(halfHeight, std::max(1.0f, rAbs * halfHeight)) : (rMax == 0.0f && rMin == 0.0f ? 0.0f : 1.0f);

            float lTopY = std::max(waveformRect.getY(), centerY - lHeight);
            float rBottomY = std::min(waveformRect.getBottom(), centerY + rHeight);

            bool isPlayed = (pixelX <= playheadX);

            if (isPlayed)
                g.setColour(OpenWavLookAndFeel::accentCyan);
            else
                g.setColour(OpenWavLookAndFeel::textPrimary.withAlpha(0.85f));

            if (lHeight > 0.0f)
                g.fillRect(juce::Rectangle<float>(pixelX, lTopY, 1.0f, std::max(1.0f, centerY - lTopY)));
            if (rHeight > 0.0f)
                g.fillRect(juce::Rectangle<float>(pixelX, centerY, 1.0f, std::max(1.0f, rBottomY - centerY)));
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

    playPauseButton.setBounds(topRow.removeFromLeft(76).withHeight(28));
    topRow.removeFromLeft(5);

    stopButton.setBounds(topRow.removeFromLeft(70).withHeight(28));
    topRow.removeFromLeft(5);

    loopButton.setBounds(topRow.removeFromLeft(70).withHeight(28));
    topRow.removeFromLeft(5);

    autoPlayButton.setBounds(topRow.removeFromLeft(70).withHeight(28));
    topRow.removeFromLeft(5);

    autoSliceButton.setBounds(topRow.removeFromLeft(74).withHeight(28));
    topRow.removeFromLeft(5);

    normalizeButton.setBounds(topRow.removeFromLeft(84).withHeight(28));
    topRow.removeFromLeft(8);

    // Sync button and BPM control placed directly after Normalize button
    syncButton.setBounds(topRow.removeFromLeft(68).withHeight(28));
    topRow.removeFromLeft(5);

    bpmControl.setBounds(topRow.removeFromLeft(118).withHeight(28));
    topRow.removeFromLeft(12);

    volumeSlider.setBounds(topRow.removeFromRight(100).withHeight(28));
    topRow.removeFromRight(10);

    timeLabel.setBounds(topRow.removeFromRight(110).withHeight(28));
    topRow.removeFromRight(10);

    sampleNameLabel.setBounds(topRow.withHeight(28));

    area.removeFromTop(8); // Spacing gap

    bool hasSlices = !sliceRatios.empty();
    slicesViewport.setVisible(hasSlices);

    if (hasSlices)
    {
        auto slicesBounds = area.removeFromBottom(92);
        slicesViewport.setBounds(slicesBounds);

        int vpHeight = slicesBounds.getHeight() - slicesViewport.getScrollBarThickness();
        slicesGrid.updateSlices(sliceRatios, totalDurationSecs, slicesViewport.getWidth(), vpHeight);
    }
}

void WaveformTransportComponent::mouseDown(const juce::MouseEvent& e)
{
    if (totalDurationSecs <= 0.0)
        return;

    auto area = getLocalBounds().reduced(12, 8);
    area.removeFromTop(32);
    area.removeFromTop(8);
    if (!sliceRatios.empty())
    {
        area.removeFromBottom(92);
        area.removeFromBottom(8);
    }
    auto waveformBounds = area.toFloat();

    if (!waveformBounds.contains(e.position.x, e.position.y))
        return;

    float mouseXRatio = (e.position.x - waveformBounds.getX()) / waveformBounds.getWidth();
    double currentRatio = juce::jlimit(0.0, 1.0, static_cast<double>(mouseXRatio));

    audioEngine.setPositionRatio(currentRatio);
    repaint();
}

void WaveformTransportComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (totalDurationSecs <= 0.0)
        return;

    auto area = getLocalBounds().reduced(12, 8);
    area.removeFromTop(32);
    area.removeFromTop(8);
    if (!sliceRatios.empty())
    {
        area.removeFromBottom(92);
        area.removeFromBottom(8);
    }
    auto waveformBounds = area.toFloat();

    float mouseXRatio = (e.position.x - waveformBounds.getX()) / waveformBounds.getWidth();
    double currentRatio = juce::jlimit(0.0, 1.0, static_cast<double>(mouseXRatio));

    audioEngine.setPositionRatio(currentRatio);
    repaint();
}

void WaveformTransportComponent::mouseUp(const juce::MouseEvent& /*e*/)
{
    repaint();
}

void WaveformTransportComponent::mouseDoubleClick(const juce::MouseEvent& /*e*/)
{
    if (totalDurationSecs <= 0.0)
        return;

    // Reset selection to full sample
    audioEngine.setSampleRange(0.0, 1.0);
    audioEngine.setPositionRatio(0.0);
    slicesGrid.setSelectedSliceIndex(-1);
    slicesGrid.repaint();
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
    if (audioEngine.isHostSyncEnabled())
    {
        double currentBpm = audioEngine.getEffectiveBpm();
        if (std::abs(bpmControl.getBpm() - currentBpm) > 0.05)
        {
            bpmControl.setBpm(currentBpm, false);
        }
    }

    // Always sync position from audioEngine so both transports stay in sync
    double newPos = audioEngine.getCurrentPositionSeconds();
    double newDur = audioEngine.getTotalLengthSeconds();
    bool needsRepaint = audioEngine.isPlaying() || std::abs(newPos - currentPositionSecs) > 0.0001;

    currentPositionSecs = newPos;
    totalDurationSecs = newDur;

    int curMins = static_cast<int>(currentPositionSecs) / 60;
    int curSecs = static_cast<int>(currentPositionSecs) % 60;
    int totMins = static_cast<int>(totalDurationSecs) / 60;
    int totSecs = static_cast<int>(totalDurationSecs) % 60;

    juce::String timeStr = juce::String::formatted("%02d:%02d / %02d:%02d", curMins, curSecs, totMins, totSecs);
    timeLabel.setText(timeStr, juce::dontSendNotification);

    if (needsRepaint)
        repaint();
}

void WaveformTransportComponent::sampleLoaded(const juce::String& filePath)
{
    juce::File f(filePath);

    if (f.existsAsFile())
    {
        sampleNameLabel.setText(f.getFileName(), juce::dontSendNotification);
    }

    // Always clear slices on sample change
    sliceRatios.clear();
    slicesGrid.setSelectedSliceIndex(-1);
    audioEngine.setSampleRange(0.0, 1.0);

    totalDurationSecs = audioEngine.getTotalLengthSeconds();
    resized();
    repaint();
}

void WaveformTransportComponent::sliderValueChanged(juce::Slider* slider)
{
    if (slider == &volumeSlider)
    {
        audioEngine.setGain(static_cast<float>(volumeSlider.getValue()));
    }
}

void WaveformTransportComponent::setSliceRatios(const std::vector<double>& ratios)
{
    sliceRatios = ratios;
    slicesGrid.setSelectedSliceIndex(-1);
    resized();
    repaint();
}

void WaveformTransportComponent::setNormalizeEnabled(bool enabled)
{
    normalizeButton.setEnabled(enabled);
}

void WaveformTransportComponent::setActiveSliceIndex(int sliceIndex)
{
    if (sliceIndex < 0)
    {
        activeSliceIndex = -1;
        activeSliceStartRatio = 0.0;
        activeSliceEndRatio = 1.0;
    }
    else
    {
        activeSliceIndex = sliceIndex;
        if (!sliceRatios.empty() && sliceIndex < static_cast<int>(sliceRatios.size()))
        {
            activeSliceStartRatio = sliceRatios[sliceIndex];
            activeSliceEndRatio = (sliceIndex + 1 < static_cast<int>(sliceRatios.size())) ? sliceRatios[sliceIndex + 1] : 1.0;
        }
        else
        {
            activeSliceStartRatio = 0.0;
            activeSliceEndRatio = 1.0;
        }
    }
    slicesGrid.setSelectedSliceIndex(activeSliceIndex);
    repaint();
}

void WaveformTransportComponent::setActiveSliceRange(double startRatio, double endRatio, int sliceIndex)
{
    activeSliceIndex = sliceIndex;
    activeSliceStartRatio = juce::jlimit(0.0, 1.0, startRatio);
    activeSliceEndRatio = juce::jlimit(activeSliceStartRatio, 1.0, endRatio);
    slicesGrid.setSelectedSliceIndex(activeSliceIndex);
    repaint();
}

void WaveformTransportComponent::openSliceConfigWindow()
{
    auto sliceComp = std::make_unique<SliceConfigComponent>(audioEngine, [this](const std::vector<double>& ratios) {
        setSliceRatios(ratios);
        if (onSlicesGenerated)
        {
            onSlicesGenerated(ratios);
        }
    });

    juce::DialogWindow::LaunchOptions opts;
    opts.dialogTitle = "Auto-Slice Configuration";
    opts.dialogBackgroundColour = OpenWavLookAndFeel::bgDark;
    opts.content.setOwned(sliceComp.release());
    opts.content->setSize(620, 380);
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar = true;
    opts.resizable = false;

    opts.launchAsync();
}

void WaveformTransportComponent::runAutoSlice()
{
    juce::AudioBuffer<float> buffer;
    double sampleRate = 44100.0;
    if (!audioEngine.getAudioBufferCopy(buffer, sampleRate))
        return;

    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();
    if (numSamples <= 0 || numChannels <= 0)
    {
        sliceRatios.clear();
        sliceRatios.push_back(0.0);
        int vpHeight = slicesViewport.getHeight() - slicesViewport.getScrollBarThickness();
        slicesGrid.updateSlices(sliceRatios, totalDurationSecs, slicesViewport.getWidth(), vpHeight);
        repaint();
        return;
    }

    int blockSize = 256;
    int numBlocks = numSamples / blockSize;

    sliceRatios.clear();
    sliceRatios.push_back(0.0);

    if (numBlocks > 4)
    {
        std::vector<float> energy(numBlocks, 0.0f);
        for (int b = 0; b < numBlocks; ++b)
        {
            float sum = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                for (int s = 0; s < blockSize; ++s)
                {
                    int idx = b * blockSize + s;
                    if (idx < numSamples)
                    {
                        float val = buffer.getSample(ch, idx);
                        sum += val * val;
                    }
                }
            }
            energy[b] = std::sqrt(sum / static_cast<float>(blockSize * numChannels));
        }

        std::vector<float> onset(numBlocks, 0.0f);
        float sumOnset = 0.0f;
        for (int b = 1; b < numBlocks; ++b)
        {
            float diff = energy[b] - energy[b - 1];
            if (diff > 0.0f)
            {
                onset[b] = diff;
                sumOnset += diff;
            }
        }

        float meanOnset = sumOnset / static_cast<float>(numBlocks);
        float sqDiffSum = 0.0f;
        for (int b = 1; b < numBlocks; ++b)
        {
            float diff = onset[b] - meanOnset;
            sqDiffSum += diff * diff;
        }
        float stdOnset = std::sqrt(sqDiffSum / static_cast<float>(numBlocks));

        float threshold = std::max(0.005f, meanOnset + 0.35f * stdOnset);
        int minDistanceBlocks = static_cast<int>(0.05 * sampleRate / static_cast<double>(blockSize));
        if (minDistanceBlocks < 1) minDistanceBlocks = 1;
        int lastOnsetBlock = -minDistanceBlocks;

        struct OnsetCandidate
        {
            int blockIdx { 0 };
            float strength { 0.0f };
        };
        std::vector<OnsetCandidate> candidates;

        for (int b = 1; b < numBlocks - 1; ++b)
        {
            if (onset[b] > threshold && onset[b] >= onset[b - 1] && onset[b] >= onset[b + 1])
            {
                if (b - lastOnsetBlock >= minDistanceBlocks)
                {
                    candidates.push_back({ b, onset[b] });
                    lastOnsetBlock = b;
                }
            }
        }

        const size_t maxOnsets = 39; // 39 onsets + 0.0 start = max 40 slices
        if (candidates.size() > maxOnsets)
        {
            std::sort(candidates.begin(), candidates.end(), [](const OnsetCandidate& a, const OnsetCandidate& b) {
                return a.strength > b.strength;
            });
            candidates.resize(maxOnsets);
            std::sort(candidates.begin(), candidates.end(), [](const OnsetCandidate& a, const OnsetCandidate& b) {
                return a.blockIdx < b.blockIdx;
            });
        }

        for (const auto& c : candidates)
        {
            double ratio = static_cast<double>(c.blockIdx * blockSize) / static_cast<double>(numSamples);
            sliceRatios.push_back(ratio);
        }
    }

    if (sliceRatios.size() <= 1)
    {
        sliceRatios.clear();
        for (int i = 0; i < 8; ++i)
        {
            sliceRatios.push_back(static_cast<double>(i) / 8.0);
        }
    }

    std::sort(sliceRatios.begin(), sliceRatios.end());
    sliceRatios.erase(std::unique(sliceRatios.begin(), sliceRatios.end()), sliceRatios.end());

    slicesGrid.setSelectedSliceIndex(-1);
    int vpHeight = slicesViewport.getHeight() - slicesViewport.getScrollBarThickness();
    slicesGrid.updateSlices(sliceRatios, totalDurationSecs, slicesViewport.getWidth(), vpHeight);
    repaint();

    if (onSlicesGenerated)
        onSlicesGenerated(sliceRatios);
}

void WaveformTransportComponent::exportAndDragSlice(int sliceIndex)
{
    if (totalDurationSecs <= 0.0 || sliceRatios.empty())
        return;

    int numSlices = static_cast<int>(sliceRatios.size());
    if (sliceIndex < 0 || sliceIndex >= numSlices)
        return;

    double startRatio = sliceRatios[sliceIndex];
    double endRatio = (sliceIndex + 1 < numSlices) ? sliceRatios[sliceIndex + 1] : 1.0;

    juce::AudioBuffer<float> fullBuffer;
    double sampleRate = 44100.0;
    if (!audioEngine.getAudioBufferCopy(fullBuffer, sampleRate))
        return;

    int totalSamples = fullBuffer.getNumSamples();
    int startSample = static_cast<int>(startRatio * totalSamples);
    int endSample = static_cast<int>(endRatio * totalSamples);
    int sliceLength = endSample - startSample;

    if (sliceLength <= 0)
        return;

    juce::AudioBuffer<float> sliceBuffer(fullBuffer.getNumChannels(), sliceLength);
    for (int ch = 0; ch < fullBuffer.getNumChannels(); ++ch)
    {
        sliceBuffer.copyFrom(ch, 0, fullBuffer, ch, startSample, sliceLength);
    }

    auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("OpenWavSlices");
    tempDir.createDirectory();

    juce::String baseName = audioEngine.getCurrentFile().getFileNameWithoutExtension();
    if (baseName.isEmpty()) baseName = "Sample";
    auto sliceFile = tempDir.getChildFile(baseName + "_Slice_" + juce::String(sliceIndex + 1) + ".wav");
    sliceFile.deleteFile();

    if (auto outStream = sliceFile.createOutputStream())
    {
        juce::WavAudioFormat wavFormat;
        std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(
            outStream.get(), sampleRate, sliceBuffer.getNumChannels(), 16, {}, 0));

        if (writer != nullptr)
        {
            outStream.release();
            writer->writeFromAudioSampleBuffer(sliceBuffer, 0, sliceLength);
            writer.reset();
        }

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
    bpmControl.lookAndFeelChanged();
    repaint();
}

void WaveformTransportComponent::transportSyncChanged(bool isSynced)
{
    syncButton.setToggleState(isSynced, juce::dontSendNotification);
    bpmControl.setHostSynced(isSynced);
    bpmControl.setBpm(audioEngine.getEffectiveBpm(), false);
}

void WaveformTransportComponent::bpmChanged(double newBpm)
{
    if (std::abs(bpmControl.getBpm() - newBpm) > 0.05)
    {
        bpmControl.setBpm(newBpm, false);
    }
}

void WaveformTransportComponent::togglePlay()
{
    playPauseButton.triggerClick();
}

void WaveformTransportComponent::toggleLoop()
{
    loopButton.triggerClick();
}

void WaveformTransportComponent::toggleSync()
{
    syncButton.triggerClick();
}

void WaveformTransportComponent::triggerSlice()
{
    autoSliceButton.triggerClick();
}

} // namespace openwav
