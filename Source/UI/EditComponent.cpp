#include "EditComponent.h"
#include "OpenWavLookAndFeel.h"

namespace openwav
{

// ─── Fade curve constants ────────────────────────────────
// curveType: 0 = Linear, 1 = Equal Power, 2 = Exponential
static const juce::StringArray curveNames = { "Linear", "Equal Power", "Exponential" };

EditComponent::EditComponent(AudioEngine& engine)
    : audioEngine(engine), hScrollBar(false)
{
    setOpaque(true);
    audioEngine.addListener(this);

    // ── Transport buttons ──────────────────────────────
    playPauseButton.onClick = [this] {
        if (audioEngine.isPlaying()) audioEngine.pause();
        else                         audioEngine.play();
    };
    addAndMakeVisible(playPauseButton);

    stopButton.onClick = [this] { audioEngine.stop(); };
    addAndMakeVisible(stopButton);

    loopToggleButton.setClickingTogglesState(true);
    loopToggleButton.setToggleState(audioEngine.isLooping(), juce::dontSendNotification);
    loopToggleButton.onClick = [this] {
        audioEngine.setLooping(loopToggleButton.getToggleState());
        repaint();
    };
    addAndMakeVisible(loopToggleButton);

    playSelButton.onClick = [this] { playSelectionOnly(); };
    addAndMakeVisible(playSelButton);

    // ── Editing buttons ────────────────────────────────
    cropButton.onClick = [this] { cropToSelection(); };
    addAndMakeVisible(cropButton);

    resetSelectionButton.onClick = [this] { resetSelection(); };
    addAndMakeVisible(resetSelectionButton);

    snapZeroCrossingButton.setClickingTogglesState(true);
    snapZeroCrossingButton.setToggleState(false, juce::dontSendNotification);
    snapZeroCrossingButton.onClick = [this] {
        snapToZeroCrossing = snapZeroCrossingButton.getToggleState();
    };
    addAndMakeVisible(snapZeroCrossingButton);

    // ── DSP Tools ──────────────────────────────────────
    silenceButton.onClick = [this] { silenceSelectedRegion(); };
    addAndMakeVisible(silenceButton);

    reverseButton.onClick = [this] { reverseSelectedRegion(); };
    addAndMakeVisible(reverseButton);

    normalizeButton.onClick = [this] { normalizeAudioPeak(); };
    addAndMakeVisible(normalizeButton);

    deverbButton.onClick = [this] { deverbSelectedRegion(); };
    addAndMakeVisible(deverbButton);

    bakeFadesButton.onClick = [this] { bakeFadesIntoBuffer(); };
    addAndMakeVisible(bakeFadesButton);

    // ── Fine Loop Nudge controls ──────────────────────
    loopInNudgeLeft.onClick = [this] {
        loopInRatio = juce::jlimit(0.0, loopOutRatio - 0.001, loopInRatio - 0.002);
        repaint();
    };
    addAndMakeVisible(loopInNudgeLeft);

    loopInNudgeRight.onClick = [this] {
        loopInRatio = juce::jlimit(0.0, loopOutRatio - 0.001, loopInRatio + 0.002);
        repaint();
    };
    addAndMakeVisible(loopInNudgeRight);

    loopOutNudgeLeft.onClick = [this] {
        loopOutRatio = juce::jlimit(loopInRatio + 0.001, 1.0, loopOutRatio - 0.002);
        repaint();
    };
    addAndMakeVisible(loopOutNudgeLeft);

    loopOutNudgeRight.onClick = [this] {
        loopOutRatio = juce::jlimit(loopInRatio + 0.001, 1.0, loopOutRatio + 0.002);
        repaint();
    };
    addAndMakeVisible(loopOutNudgeRight);

    exportButton.onClick = [this] { exportEdited(); };
    addAndMakeVisible(exportButton);

    setWantsKeyboardFocus(true);

    // ── Sample name label ──────────────────────────────
    sampleNameLabel.setFont(juce::Font(14.0f).boldened());
    sampleNameLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(sampleNameLabel);

    // ── Time labels ────────────────────────────────────
    auto setupStaticLabel = [this](juce::Label& lbl) {
        lbl.setFont(juce::Font(11.0f));
        lbl.setJustificationType(juce::Justification::centredRight);
        lbl.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
        addAndMakeVisible(lbl);
    };
    setupStaticLabel(startLabel);
    setupStaticLabel(endLabel);
    setupStaticLabel(loopInLabel);
    setupStaticLabel(loopOutLabel);

    auto setupTimeValueLabel = [this](juce::Label& lbl) {
        lbl.setFont(juce::Font(11.0f));
        lbl.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(lbl);
    };
    setupTimeValueLabel(startTimeLabel);
    setupTimeValueLabel(endTimeLabel);
    setupTimeValueLabel(loopInTimeLabel);
    setupTimeValueLabel(loopOutTimeLabel);

    // ── Fade In controls ───────────────────────────────
    fadeInSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    fadeInSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    fadeInSlider.setRange(0.0, 500.0, 1.0);
    fadeInSlider.setValue(0.0);
    fadeInSlider.onValueChange = [this] {
        fadeInMs = fadeInSlider.getValue();
        fadeInMsLabel.setText(juce::String(static_cast<int>(fadeInMs)) + " ms", juce::dontSendNotification);
        repaint();
    };
    fadeInSlider.onDragEnd = [this] {
        bakeFadesIntoBuffer();
    };
    addAndMakeVisible(fadeInSlider);

    fadeInLabel.setFont(juce::Font(11.0f));
    fadeInLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(fadeInLabel);

    fadeInMsLabel.setFont(juce::Font(11.0f));
    fadeInMsLabel.setText("0 ms", juce::dontSendNotification);
    fadeInMsLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(fadeInMsLabel);

    fadeInCurveBox.addItemList(curveNames, 1);
    fadeInCurveBox.setSelectedId(1, juce::dontSendNotification);
    fadeInCurveBox.onChange = [this] {
        fadeInCurveType = fadeInCurveBox.getSelectedId() - 1;
        repaint();
    };
    addAndMakeVisible(fadeInCurveBox);

    // ── Fade Out controls ──────────────────────────────
    fadeOutSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    fadeOutSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    fadeOutSlider.setRange(0.0, 500.0, 1.0);
    fadeOutSlider.setValue(0.0);
    fadeOutSlider.onValueChange = [this] {
        fadeOutMs = fadeOutSlider.getValue();
        fadeOutMsLabel.setText(juce::String(static_cast<int>(fadeOutMs)) + " ms", juce::dontSendNotification);
        repaint();
    };
    fadeOutSlider.onDragEnd = [this] {
        bakeFadesIntoBuffer();
    };
    addAndMakeVisible(fadeOutSlider);

    fadeOutLabel.setFont(juce::Font(11.0f));
    fadeOutLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(fadeOutLabel);

    fadeOutMsLabel.setFont(juce::Font(11.0f));
    fadeOutMsLabel.setText("0 ms", juce::dontSendNotification);
    fadeOutMsLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(fadeOutMsLabel);

    fadeOutCurveBox.addItemList(curveNames, 1);
    fadeOutCurveBox.setSelectedId(1, juce::dontSendNotification);
    fadeOutCurveBox.onChange = [this] {
        fadeOutCurveType = fadeOutCurveBox.getSelectedId() - 1;
        repaint();
    };
    addAndMakeVisible(fadeOutCurveBox);

    // ── Crossfade control ──────────────────────────────
    crossfadeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    crossfadeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    crossfadeSlider.setRange(0.0, 100.0, 1.0);
    crossfadeSlider.setValue(0.0);
    crossfadeSlider.onValueChange = [this] {
        crossfadeMs = crossfadeSlider.getValue();
        crossfadeMsLabel.setText(juce::String(static_cast<int>(crossfadeMs)) + " ms", juce::dontSendNotification);
        repaint();
    };
    addAndMakeVisible(crossfadeSlider);

    crossfadeLabel.setFont(juce::Font(11.0f));
    crossfadeLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(crossfadeLabel);

    crossfadeMsLabel.setFont(juce::Font(11.0f));
    crossfadeMsLabel.setText("0 ms", juce::dontSendNotification);
    crossfadeMsLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(crossfadeMsLabel);

    // ── Zoom controls ──────────────────────────────────
    zoomSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    zoomSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    zoomSlider.setRange(1.0, 64.0, 0.1);
    zoomSlider.setSkewFactorFromMidPoint(8.0);
    zoomSlider.setValue(1.0);
    zoomSlider.onValueChange = [this] {
        zoomLevel = zoomSlider.getValue();
        scrollOffset = juce::jlimit(0.0, std::max(0.0, 1.0 - 1.0 / zoomLevel), scrollOffset);
        repaint();
    };
    addAndMakeVisible(zoomSlider);

    zoomInButton.onClick = [this] {
        zoomSlider.setValue(juce::jmin(64.0, zoomLevel * 1.5));
    };
    addAndMakeVisible(zoomInButton);

    zoomOutButton.onClick = [this] {
        zoomSlider.setValue(juce::jmax(1.0, zoomLevel / 1.5));
    };
    addAndMakeVisible(zoomOutButton);

    zoomLabel.setFont(juce::Font(11.0f));
    zoomLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(zoomLabel);

    // ── Scroll bar ─────────────────────────────────────
    hScrollBar.setRangeLimits(0.0, 1.0);
    hScrollBar.setCurrentRange(0.0, 1.0);
    hScrollBar.setAutoHide(false);
    hScrollBar.addListener(this);

    // Defer addAndMakeVisible — only show when zoomed
    addAndMakeVisible(hScrollBar);

    // Initial look and feel colors
    lookAndFeelChanged();

    startTimerHz(30);
}

EditComponent::~EditComponent()
{
    hScrollBar.removeListener(this);
    stopTimer();
    audioEngine.removeListener(this);
}

void EditComponent::scrollBarMoved(juce::ScrollBar* scrollBarThatHasMoved, double newRangeStart)
{
    if (scrollBarThatHasMoved == &hScrollBar)
    {
        scrollOffset = newRangeStart;
        repaint();
    }
}

// ─────────────────────────────────────────────────────────
//  Look and Feel
// ─────────────────────────────────────────────────────────
void EditComponent::lookAndFeelChanged()
{
    // Future-proof: nothing needed beyond inherited behavior currently,
    // but labels pick up the LookAndFeel colours automatically.
}

// ─────────────────────────────────────────────────────────
//  Layout
// ─────────────────────────────────────────────────────────
juce::Rectangle<float> EditComponent::getWaveformBounds() const
{
    auto area = getLocalBounds().reduced(20, 16);
    area.removeFromTop(66);     // 2-row toolbar
    area.removeFromTop(8);      // Gap
    area.removeFromBottom(130); // Control panel
    area.removeFromBottom(20);  // Scrollbar + gap
    return area.toFloat();
}

juce::Rectangle<int> EditComponent::getControlPanelBounds() const
{
    auto area = getLocalBounds().reduced(20, 16);
    area.removeFromBottom(20); // Account for scrollbar (16px) + gap (4px)
    return area.removeFromBottom(130);
}

void EditComponent::updateControlVisibility()
{
    bool hasSamplerSample = (totalDurationSecs > 0.0 && audioEngine.isCurrentSampleInSampler());

    playPauseButton.setVisible(hasSamplerSample);
    stopButton.setVisible(hasSamplerSample);
    playSelButton.setVisible(hasSamplerSample);
    loopToggleButton.setVisible(hasSamplerSample);
    cropButton.setVisible(hasSamplerSample);
    resetSelectionButton.setVisible(hasSamplerSample);
    snapZeroCrossingButton.setVisible(hasSamplerSample);
    silenceButton.setVisible(hasSamplerSample);
    reverseButton.setVisible(hasSamplerSample);
    normalizeButton.setVisible(hasSamplerSample);
    deverbButton.setVisible(hasSamplerSample);
    bakeFadesButton.setVisible(hasSamplerSample);
    exportButton.setVisible(hasSamplerSample);
    loopInNudgeLeft.setVisible(hasSamplerSample);
    loopInNudgeRight.setVisible(hasSamplerSample);
    loopOutNudgeLeft.setVisible(hasSamplerSample);
    loopOutNudgeRight.setVisible(hasSamplerSample);
    startLabel.setVisible(hasSamplerSample);
    endLabel.setVisible(hasSamplerSample);
    loopInLabel.setVisible(hasSamplerSample);
    loopOutLabel.setVisible(hasSamplerSample);
    startTimeLabel.setVisible(hasSamplerSample);
    endTimeLabel.setVisible(hasSamplerSample);
    loopInTimeLabel.setVisible(hasSamplerSample);
    loopOutTimeLabel.setVisible(hasSamplerSample);
    sampleNameLabel.setVisible(hasSamplerSample);
    fadeInSlider.setVisible(hasSamplerSample);
    fadeOutSlider.setVisible(hasSamplerSample);
    fadeInCurveBox.setVisible(hasSamplerSample);
    fadeOutCurveBox.setVisible(hasSamplerSample);
    fadeInLabel.setVisible(hasSamplerSample);
    fadeOutLabel.setVisible(hasSamplerSample);
    fadeInMsLabel.setVisible(hasSamplerSample);
    fadeOutMsLabel.setVisible(hasSamplerSample);
    crossfadeSlider.setVisible(hasSamplerSample);
    crossfadeLabel.setVisible(hasSamplerSample);
    crossfadeMsLabel.setVisible(hasSamplerSample);
    zoomSlider.setVisible(hasSamplerSample);
    zoomInButton.setVisible(hasSamplerSample);
    zoomOutButton.setVisible(hasSamplerSample);
    zoomLabel.setVisible(hasSamplerSample);
    hScrollBar.setVisible(hasSamplerSample && zoomLevel > 1.0);
}

void EditComponent::resized()
{
    auto area = getLocalBounds().reduced(20, 16);

    // ── Top 2-row toolbar ──────────────────────────────
    auto topArea = area.removeFromTop(66);
    auto row1 = topArea.removeFromTop(28);
    topArea.removeFromTop(6);
    auto row2 = topArea;

    int gap = 5;

    // Row 1: Transport, Sample Name, Zoom & Export
    playPauseButton.setBounds(row1.removeFromLeft(68));
    row1.removeFromLeft(gap);
    stopButton.setBounds(row1.removeFromLeft(60));
    row1.removeFromLeft(gap);
    playSelButton.setBounds(row1.removeFromLeft(80));
    row1.removeFromLeft(gap);
    loopToggleButton.setBounds(row1.removeFromLeft(65));
    row1.removeFromLeft(12);
    sampleNameLabel.setBounds(row1.removeFromLeft(180));

    // Right side of Row 1: Zoom & Export
    exportButton.setBounds(row1.removeFromRight(90));
    row1.removeFromRight(gap);
    zoomInButton.setBounds(row1.removeFromRight(26));
    row1.removeFromRight(2);
    zoomOutButton.setBounds(row1.removeFromRight(26));
    row1.removeFromRight(gap);
    zoomSlider.setBounds(row1.removeFromRight(100));
    row1.removeFromRight(gap);
    zoomLabel.setBounds(row1.removeFromRight(40));

    // Row 2: Range & DSP Action Tools
    cropButton.setBounds(row2.removeFromLeft(60));
    row2.removeFromLeft(gap);
    resetSelectionButton.setBounds(row2.removeFromLeft(65));
    row2.removeFromLeft(gap);
    snapZeroCrossingButton.setBounds(row2.removeFromLeft(88));
    row2.removeFromLeft(14); // Gap to DSP tools

    silenceButton.setBounds(row2.removeFromLeft(72));
    row2.removeFromLeft(gap);
    reverseButton.setBounds(row2.removeFromLeft(75));
    row2.removeFromLeft(gap);
    normalizeButton.setBounds(row2.removeFromLeft(88));
    row2.removeFromLeft(gap);
    deverbButton.setBounds(row2.removeFromLeft(75));
    row2.removeFromLeft(gap);
    bakeFadesButton.setBounds(row2.removeFromLeft(92));

    area.removeFromTop(8); // Gap

    // ── Scrollbar ──────────────────────────────────────
    auto scrollArea = area.removeFromBottom(16);
    area.removeFromBottom(4);
    hScrollBar.setBounds(scrollArea);

    // ── Control panel layout ───────────────────────────
    auto controlPanel = getControlPanelBounds();
    auto innerPanel = controlPanel.reduced(14, 10);

    auto cpLeft = innerPanel.removeFromLeft(innerPanel.getWidth() / 2 - 8);
    innerPanel.removeFromLeft(16);
    auto cpRight = innerPanel;

    int rowH = 22;
    int labelW = 72;
    int valueW = 95;
    int itemGap = 6;
    int rowGap = 4;

    // Left panel: time displays + fine loop nudges
    {
        auto r = cpLeft.removeFromTop(rowH);
        cpLeft.removeFromTop(rowGap);
        startLabel.setBounds(r.removeFromLeft(labelW));
        r.removeFromLeft(itemGap);
        startTimeLabel.setBounds(r.removeFromLeft(valueW));
    }
    {
        auto r = cpLeft.removeFromTop(rowH);
        cpLeft.removeFromTop(rowGap);
        endLabel.setBounds(r.removeFromLeft(labelW));
        r.removeFromLeft(itemGap);
        endTimeLabel.setBounds(r.removeFromLeft(valueW));
    }
    {
        auto r = cpLeft.removeFromTop(rowH);
        cpLeft.removeFromTop(rowGap);
        loopInLabel.setBounds(r.removeFromLeft(labelW));
        r.removeFromLeft(itemGap);
        loopInTimeLabel.setBounds(r.removeFromLeft(valueW));
        r.removeFromLeft(4);
        loopInNudgeLeft.setBounds(r.removeFromLeft(22));
        r.removeFromLeft(2);
        loopInNudgeRight.setBounds(r.removeFromLeft(22));
    }
    {
        auto r = cpLeft.removeFromTop(rowH);
        cpLeft.removeFromTop(rowGap);
        loopOutLabel.setBounds(r.removeFromLeft(labelW));
        r.removeFromLeft(itemGap);
        loopOutTimeLabel.setBounds(r.removeFromLeft(valueW));
        r.removeFromLeft(4);
        loopOutNudgeLeft.setBounds(r.removeFromLeft(22));
        r.removeFromLeft(2);
        loopOutNudgeRight.setBounds(r.removeFromLeft(22));
    }

    // Right panel: fade controls
    int sliderW = 110;
    int msW = 45;
    int comboW = 95;

    {
        auto r = cpRight.removeFromTop(rowH);
        cpRight.removeFromTop(rowGap);
        fadeInLabel.setBounds(r.removeFromLeft(labelW));
        r.removeFromLeft(4);
        fadeInSlider.setBounds(r.removeFromLeft(sliderW));
        r.removeFromLeft(4);
        fadeInMsLabel.setBounds(r.removeFromLeft(msW));
        r.removeFromLeft(4);
        fadeInCurveBox.setBounds(r.removeFromLeft(comboW));
    }
    {
        auto r = cpRight.removeFromTop(rowH);
        cpRight.removeFromTop(rowGap);
        fadeOutLabel.setBounds(r.removeFromLeft(labelW));
        r.removeFromLeft(4);
        fadeOutSlider.setBounds(r.removeFromLeft(sliderW));
        r.removeFromLeft(4);
        fadeOutMsLabel.setBounds(r.removeFromLeft(msW));
        r.removeFromLeft(4);
        fadeOutCurveBox.setBounds(r.removeFromLeft(comboW));
    }
    {
        auto r = cpRight.removeFromTop(rowH);
        cpRight.removeFromTop(rowGap);
        crossfadeLabel.setBounds(r.removeFromLeft(labelW));
        r.removeFromLeft(4);
        crossfadeSlider.setBounds(r.removeFromLeft(sliderW));
        r.removeFromLeft(4);
        crossfadeMsLabel.setBounds(r.removeFromLeft(msW));
    }

    double visibleFraction = 1.0 / zoomLevel;
    hScrollBar.setCurrentRange(scrollOffset, visibleFraction, juce::dontSendNotification);

    updateControlVisibility();
}

// ─────────────────────────────────────────────────────────
//  Coordinate Conversions
// ─────────────────────────────────────────────────────────
float EditComponent::ratioToX(double ratio, juce::Rectangle<float> bounds) const
{
    double visibleStart = scrollOffset;
    double visibleEnd = scrollOffset + 1.0 / zoomLevel;
    double normalised = (ratio - visibleStart) / (visibleEnd - visibleStart);
    return bounds.getX() + static_cast<float>(normalised) * bounds.getWidth();
}

double EditComponent::xToRatio(float x, juce::Rectangle<float> bounds) const
{
    double visibleStart = scrollOffset;
    double visibleEnd = scrollOffset + 1.0 / zoomLevel;
    double normalised = static_cast<double>(x - bounds.getX()) / bounds.getWidth();
    return visibleStart + normalised * (visibleEnd - visibleStart);
}

// ─────────────────────────────────────────────────────────
//  Fade Curves
// ─────────────────────────────────────────────────────────
float EditComponent::evaluateFadeCurve(float t, int curveType) const
{
    t = juce::jlimit(0.0f, 1.0f, t);
    switch (curveType)
    {
        case 1: // Equal Power
            return std::sin(t * juce::MathConstants<float>::halfPi);
        case 2: // Exponential
            return t * t;
        default: // Linear
            return t;
    }
}

// ─────────────────────────────────────────────────────────
//  Time formatting helper
// ─────────────────────────────────────────────────────────
static juce::String formatTime(double seconds)
{
    if (seconds < 0.0) seconds = 0.0;
    int mins = static_cast<int>(seconds) / 60;
    int secs = static_cast<int>(seconds) % 60;
    int ms = static_cast<int>((seconds - static_cast<int>(seconds)) * 1000.0);
    return juce::String::formatted("%02d:%02d.%03d", mins, secs, ms);
}

// ─────────────────────────────────────────────────────────
//  Paint
// ─────────────────────────────────────────────────────────
void EditComponent::paint(juce::Graphics& g)
{
    g.fillAll(OpenWavLookAndFeel::bgDark);

    if (totalDurationSecs <= 0.0 || !audioEngine.isCurrentSampleInSampler())
    {
        g.setColour(OpenWavLookAndFeel::textSecondary.withAlpha(0.5f));
        g.setFont(juce::Font(20.0f));
        g.drawText("No sample loaded into sampler", getLocalBounds(), juce::Justification::centred);
        return;
    }

    auto wfBounds = getWaveformBounds();

    // Waveform track background
    g.setColour(OpenWavLookAndFeel::bgCard);
    g.fillRoundedRectangle(wfBounds, 6.0f);
    g.setColour(OpenWavLookAndFeel::borderColour);
    g.drawRoundedRectangle(wfBounds, 6.0f, 1.0f);

    auto innerWf = wfBounds.reduced(6.0f, 6.0f);

    paintTimeRuler(g, innerWf);
    paintWaveform(g, innerWf);
    paintFadeEnvelopes(g, innerWf);
    paintFadeHandles(g, innerWf);
    paintMarkers(g, innerWf);
    paintPlayhead(g, innerWf);
    paintSelectionInfoOverlay(g, innerWf);

    // ── Control panel labels (static text drawn in paint) ──
    auto cpBounds = getControlPanelBounds();

    // Control panel background
    g.setColour(OpenWavLookAndFeel::bgCard);
    g.fillRoundedRectangle(cpBounds.toFloat(), 6.0f);
    g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.4f));
    g.drawRoundedRectangle(cpBounds.toFloat(), 6.0f, 1.0f);

    // Update time label text
    double startR = audioEngine.getSampleStartRatio();
    double endR = audioEngine.getSampleEndRatio();
    startTimeLabel.setText(formatTime(startR * totalDurationSecs), juce::dontSendNotification);
    endTimeLabel.setText(formatTime(endR * totalDurationSecs), juce::dontSendNotification);
    loopInTimeLabel.setText(formatTime(loopInRatio * totalDurationSecs), juce::dontSendNotification);
    loopOutTimeLabel.setText(formatTime(loopOutRatio * totalDurationSecs), juce::dontSendNotification);
}

// ─────────────────────────────────────────────────────────
//  Waveform Rendering
// ─────────────────────────────────────────────────────────
void EditComponent::paintWaveform(juce::Graphics& g, juce::Rectangle<float> bounds) const
{
    double visibleStart = scrollOffset;
    double visibleEnd = scrollOffset + 1.0 / zoomLevel;

    double startRatio = audioEngine.getSampleStartRatio();
    double endRatio = audioEngine.getSampleEndRatio();

    float selStartX = ratioToX(startRatio, bounds);
    float selEndX = ratioToX(endRatio, bounds);

    const auto peaks = audioEngine.getWaveformPeaks();
    const int numPeakPoints = peaks.numPoints;
    int numPixels = static_cast<int>(bounds.getWidth());
    bool isStereo = (peaks.numChannels >= 2);

    if (isStereo)
    {
        float midY = bounds.getCentreY();
        float lCenterY = bounds.getY() + bounds.getHeight() * 0.25f;
        float rCenterY = bounds.getY() + bounds.getHeight() * 0.75f;
        float halfChanH = bounds.getHeight() * 0.22f;

        g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.2f));
        g.drawHorizontalLine(static_cast<int>(lCenterY), bounds.getX(), bounds.getRight());
        g.drawHorizontalLine(static_cast<int>(rCenterY), bounds.getX(), bounds.getRight());
        g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.4f));
        g.drawHorizontalLine(static_cast<int>(midY), bounds.getX(), bounds.getRight());

        for (int x = 0; x < numPixels; ++x)
        {
            float pixelX = bounds.getX() + static_cast<float>(x);
            double colStartRatio = visibleStart + (static_cast<double>(x) / numPixels) * (visibleEnd - visibleStart);
            double colEndRatio = visibleStart + (static_cast<double>(x + 1) / numPixels) * (visibleEnd - visibleStart);

            float lMin = 0.0f, lMax = 0.0f, rMin = 0.0f, rMax = 0.0f;

            if (numPeakPoints > 0)
            {
                int pStart = juce::jlimit(0, numPeakPoints - 1, static_cast<int>(colStartRatio * numPeakPoints));
                int pEnd = juce::jlimit(pStart + 1, numPeakPoints, static_cast<int>(colEndRatio * numPeakPoints));

                for (int p = pStart; p < pEnd; ++p)
                {
                    auto ps = static_cast<size_t>(p);
                    if (peaks.minLeft[ps] < lMin) lMin = peaks.minLeft[ps];
                    if (peaks.maxLeft[ps] > lMax) lMax = peaks.maxLeft[ps];
                    if (peaks.minRight[ps] < rMin) rMin = peaks.minRight[ps];
                    if (peaks.maxRight[ps] > rMax) rMax = peaks.maxRight[ps];
                }
            }

            float lAbs = std::max(std::abs(lMin), std::abs(lMax));
            float rAbs = std::max(std::abs(rMin), std::abs(rMax));

            float lH = (lAbs > 0.001f) ? std::max(1.0f, lAbs * halfChanH) : 0.0f;
            float rH = (rAbs > 0.001f) ? std::max(1.0f, rAbs * halfChanH) : 0.0f;

            bool inSelection = (pixelX >= selStartX && pixelX <= selEndX);

            if (inSelection)
                g.setColour(OpenWavLookAndFeel::textPrimary.withAlpha(0.85f));
            else
                g.setColour(OpenWavLookAndFeel::textSecondary.withAlpha(0.22f));

            if (lH > 0.0f)
                g.fillRect(juce::Rectangle<float>(pixelX, lCenterY - lH, 1.0f, lH * 2.0f));
            if (rH > 0.0f)
                g.fillRect(juce::Rectangle<float>(pixelX, rCenterY - rH, 1.0f, rH * 2.0f));
        }
    }
    else
    {
        float centerY = bounds.getCentreY();
        float halfHeight = (bounds.getHeight() - 4.0f) * 0.5f;

        g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.3f));
        g.drawHorizontalLine(static_cast<int>(centerY), bounds.getX(), bounds.getRight());

        for (int x = 0; x < numPixels; ++x)
        {
            float pixelX = bounds.getX() + static_cast<float>(x);
            double colStartRatio = visibleStart + (static_cast<double>(x) / numPixels) * (visibleEnd - visibleStart);
            double colEndRatio = visibleStart + (static_cast<double>(x + 1) / numPixels) * (visibleEnd - visibleStart);

            float lMin = 0.0f, lMax = 0.0f;
            if (numPeakPoints > 0)
            {
                int pStart = juce::jlimit(0, numPeakPoints - 1, static_cast<int>(colStartRatio * numPeakPoints));
                int pEnd = juce::jlimit(pStart + 1, numPeakPoints, static_cast<int>(colEndRatio * numPeakPoints));

                for (int p = pStart; p < pEnd; ++p)
                {
                    auto ps = static_cast<size_t>(p);
                    if (peaks.minLeft[ps] < lMin) lMin = peaks.minLeft[ps];
                    if (peaks.maxLeft[ps] > lMax) lMax = peaks.maxLeft[ps];
                }
            }

            float lAbs = std::max(std::abs(lMin), std::abs(lMax));
            float lH = (lAbs > 0.001f) ? std::max(1.0f, lAbs * halfHeight) : 0.0f;

            bool inSelection = (pixelX >= selStartX && pixelX <= selEndX);

            if (inSelection)
                g.setColour(OpenWavLookAndFeel::textPrimary.withAlpha(0.85f));
            else
                g.setColour(OpenWavLookAndFeel::textSecondary.withAlpha(0.22f));

            if (lH > 0.0f)
                g.fillRect(juce::Rectangle<float>(pixelX, centerY - lH, 1.0f, std::max(1.0f, lH * 2.0f)));
        }
    }

    // Darkened overlay for non-selected regions
    g.setColour(juce::Colours::black.withAlpha(0.35f));
    if (selStartX > bounds.getX())
        g.fillRect(bounds.getX(), bounds.getY(), selStartX - bounds.getX(), bounds.getHeight());
    if (selEndX < bounds.getRight())
        g.fillRect(selEndX, bounds.getY(), bounds.getRight() - selEndX, bounds.getHeight());
}

// ─────────────────────────────────────────────────────────
//  Markers
// ─────────────────────────────────────────────────────────
void EditComponent::paintMarkers(juce::Graphics& g, juce::Rectangle<float> bounds) const
{
    double startR = audioEngine.getSampleStartRatio();
    double endR = audioEngine.getSampleEndRatio();

    float selStartX = ratioToX(startR, bounds);
    float selEndX = ratioToX(endR, bounds);

    // ── Start/End markers (cyan) ──────────────────────
    g.setColour(OpenWavLookAndFeel::accentCyan);

    // Start marker line + handle
    g.drawVerticalLine(static_cast<int>(selStartX), bounds.getY(), bounds.getBottom());
    g.fillRoundedRectangle(selStartX - 5.0f, bounds.getY() - 2.0f, 10.0f, 12.0f, 3.0f);
    g.setColour(OpenWavLookAndFeel::bgDark);
    g.setFont(juce::Font(8.0f).boldened());
    g.drawText("S", juce::Rectangle<float>(selStartX - 5.0f, bounds.getY() - 2.0f, 10.0f, 12.0f),
               juce::Justification::centred);

    // End marker line + handle
    g.setColour(OpenWavLookAndFeel::accentCyan);
    g.drawVerticalLine(static_cast<int>(selEndX), bounds.getY(), bounds.getBottom());
    g.fillRoundedRectangle(selEndX - 5.0f, bounds.getY() - 2.0f, 10.0f, 12.0f, 3.0f);
    g.setColour(OpenWavLookAndFeel::bgDark);
    g.drawText("E", juce::Rectangle<float>(selEndX - 5.0f, bounds.getY() - 2.0f, 10.0f, 12.0f),
               juce::Justification::centred);

    // ── Loop In/Out markers (green) ───────────────────
    if (loopMarkersSet)
    {
        juce::Colour loopColour = juce::Colour::fromRGB(0, 230, 118); // Emerald green

        float loopInX = ratioToX(loopInRatio, bounds);
        float loopOutX = ratioToX(loopOutRatio, bounds);

        // Loop region tint
        if (loopInX < loopOutX)
        {
            g.setColour(loopColour.withAlpha(0.06f));
            g.fillRect(loopInX, bounds.getY(), loopOutX - loopInX, bounds.getHeight());
        }

        // Loop In marker
        g.setColour(loopColour);
        g.drawVerticalLine(static_cast<int>(loopInX), bounds.getY(), bounds.getBottom());
        // Triangle handle pointing right
        juce::Path triIn;
        triIn.addTriangle(loopInX, bounds.getBottom() - 12.0f,
                          loopInX, bounds.getBottom(),
                          loopInX + 8.0f, bounds.getBottom() - 6.0f);
        g.fillPath(triIn);

        // Loop Out marker
        g.drawVerticalLine(static_cast<int>(loopOutX), bounds.getY(), bounds.getBottom());
        // Triangle handle pointing left
        juce::Path triOut;
        triOut.addTriangle(loopOutX, bounds.getBottom() - 12.0f,
                           loopOutX, bounds.getBottom(),
                           loopOutX - 8.0f, bounds.getBottom() - 6.0f);
        g.fillPath(triOut);

        // Crossfade indicator at loop boundary
        if (crossfadeMs > 0.0 && totalDurationSecs > 0.0)
        {
            double xfadeRatio = (crossfadeMs / 1000.0) / totalDurationSecs;
            float xfStartX = ratioToX(loopOutRatio - xfadeRatio, bounds);
            float xfEndX = ratioToX(loopOutRatio, bounds);

            g.setColour(loopColour.withAlpha(0.15f));
            g.fillRect(xfStartX, bounds.getY(), xfEndX - xfStartX, bounds.getHeight());

            // Dashed line at crossfade start
            g.setColour(loopColour.withAlpha(0.5f));
            float dashLengths[] = { 3.0f, 3.0f };
            g.drawDashedLine(juce::Line<float>(xfStartX, bounds.getY(), xfStartX, bounds.getBottom()),
                             dashLengths, 2, 1.0f);
        }
    }
}

// ─────────────────────────────────────────────────────────
//  Fade Envelopes Overlay
// ─────────────────────────────────────────────────────────
void EditComponent::paintFadeEnvelopes(juce::Graphics& g, juce::Rectangle<float> bounds) const
{
    if (totalDurationSecs <= 0.0) return;

    double startR = audioEngine.getSampleStartRatio();
    double endR = audioEngine.getSampleEndRatio();
    double selDuration = (endR - startR) * totalDurationSecs;
    if (selDuration <= 0.0) return;

    // ── Fade In envelope ──────────────────────────────
    if (fadeInMs > 0.0)
    {
        double fadeInRatio = (fadeInMs / 1000.0) / totalDurationSecs;
        float fadeStartX = ratioToX(startR, bounds);
        float fadeEndX = ratioToX(startR + fadeInRatio, bounds);
        float fadeWidth = fadeEndX - fadeStartX;

        if (fadeWidth > 1.0f)
        {
            juce::Path fadePath;
            fadePath.startNewSubPath(fadeStartX, bounds.getBottom());

            int steps = juce::jmax(2, static_cast<int>(fadeWidth));
            for (int i = 0; i <= steps; ++i)
            {
                float t = static_cast<float>(i) / steps;
                float gain = evaluateFadeCurve(t, fadeInCurveType);
                float px = fadeStartX + t * fadeWidth;
                float py = bounds.getBottom() - gain * bounds.getHeight();
                fadePath.lineTo(px, py);
            }

            fadePath.lineTo(fadeEndX, bounds.getY());
            fadePath.lineTo(fadeStartX, bounds.getY());
            fadePath.closeSubPath();

            g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.12f));
            g.fillPath(fadePath);

            // Envelope curve line
            juce::Path curveLine;
            curveLine.startNewSubPath(fadeStartX, bounds.getBottom());
            for (int i = 0; i <= steps; ++i)
            {
                float t = static_cast<float>(i) / steps;
                float gain = evaluateFadeCurve(t, fadeInCurveType);
                float px = fadeStartX + t * fadeWidth;
                float py = bounds.getBottom() - gain * bounds.getHeight();
                curveLine.lineTo(px, py);
            }
            g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.6f));
            g.strokePath(curveLine, juce::PathStrokeType(1.5f));
        }
    }

    // ── Fade Out envelope ─────────────────────────────
    if (fadeOutMs > 0.0)
    {
        double fadeOutRatio = (fadeOutMs / 1000.0) / totalDurationSecs;
        float fadeStartX = ratioToX(endR - fadeOutRatio, bounds);
        float fadeEndX = ratioToX(endR, bounds);
        float fadeWidth = fadeEndX - fadeStartX;

        if (fadeWidth > 1.0f)
        {
            juce::Path fadePath;
            fadePath.startNewSubPath(fadeStartX, bounds.getY());

            int steps = juce::jmax(2, static_cast<int>(fadeWidth));
            for (int i = 0; i <= steps; ++i)
            {
                float t = static_cast<float>(i) / steps;
                float gain = 1.0f - evaluateFadeCurve(t, fadeOutCurveType);
                float px = fadeStartX + t * fadeWidth;
                float py = bounds.getBottom() - gain * bounds.getHeight();
                fadePath.lineTo(px, py);
            }

            fadePath.lineTo(fadeEndX, bounds.getBottom());
            fadePath.lineTo(fadeEndX, bounds.getY());
            fadePath.lineTo(fadeStartX, bounds.getY());
            fadePath.closeSubPath();

            g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.12f));
            g.fillPath(fadePath);

            // Envelope curve line
            juce::Path curveLine;
            for (int i = 0; i <= steps; ++i)
            {
                float t = static_cast<float>(i) / steps;
                float gain = 1.0f - evaluateFadeCurve(t, fadeOutCurveType);
                float px = fadeStartX + t * fadeWidth;
                float py = bounds.getBottom() - gain * bounds.getHeight();
                if (i == 0) curveLine.startNewSubPath(px, py);
                else        curveLine.lineTo(px, py);
            }
            g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.6f));
            g.strokePath(curveLine, juce::PathStrokeType(1.5f));
        }
    }
}

// ─────────────────────────────────────────────────────────
//  Playhead
// ─────────────────────────────────────────────────────────
void EditComponent::paintPlayhead(juce::Graphics& g, juce::Rectangle<float> bounds) const
{
    if (totalDurationSecs <= 0.0) return;

    double posRatio = currentPositionSecs / totalDurationSecs;
    float phX = ratioToX(posRatio, bounds);

    if (phX >= bounds.getX() && phX <= bounds.getRight())
    {
        g.setColour(OpenWavLookAndFeel::accentCyan.brighter(0.5f));
        g.drawLine(phX, bounds.getY(), phX, bounds.getBottom(), 2.0f);
        g.setColour(juce::Colours::white);
        g.fillEllipse(phX - 5.0f, bounds.getCentreY() - 5.0f, 10.0f, 10.0f);
        g.setColour(OpenWavLookAndFeel::accentCyan);
        g.drawEllipse(phX - 5.0f, bounds.getCentreY() - 5.0f, 10.0f, 10.0f, 1.5f);
    }
}

// ─────────────────────────────────────────────────────────
//  Zero-Crossing Search Helper
// ─────────────────────────────────────────────────────────
double EditComponent::findNearestZeroCrossing(double ratio) const
{
    juce::AudioBuffer<float> buffer;
    double sampleRate = 44100.0;
    if (!audioEngine.getAudioBufferCopy(buffer, sampleRate) || buffer.getNumSamples() == 0)
        return ratio;

    int totalSamples = buffer.getNumSamples();
    int targetIdx = juce::jlimit(0, totalSamples - 1, static_cast<int>(ratio * totalSamples));

    int searchRadius = 500;
    int startIdx = std::max(0, targetIdx - searchRadius);
    int endIdx = std::min(totalSamples - 2, targetIdx + searchRadius);

    const float* channelData = buffer.getReadPointer(0);
    int bestIdx = targetIdx;
    float minAbsVal = std::abs(channelData[targetIdx]);

    for (int i = startIdx; i <= endIdx; ++i)
    {
        if (channelData[i] * channelData[i + 1] <= 0.0f)
        {
            float absVal = std::abs(channelData[i]);
            if (absVal < minAbsVal || std::abs(i - targetIdx) < std::abs(bestIdx - targetIdx))
            {
                minAbsVal = absVal;
                bestIdx = i;
            }
        }
    }

    return static_cast<double>(bestIdx) / totalSamples;
}

// ─────────────────────────────────────────────────────────
//  Time Ruler Rendering
// ─────────────────────────────────────────────────────────
void EditComponent::paintTimeRuler(juce::Graphics& g, juce::Rectangle<float> bounds) const
{
    if (totalDurationSecs <= 0.0) return;

    auto rulerBounds = bounds.removeFromTop(18.0f);
    g.setColour(OpenWavLookAndFeel::bgDark.withAlpha(0.7f));
    g.fillRect(rulerBounds);
    g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.5f));
    g.drawHorizontalLine(static_cast<int>(rulerBounds.getBottom()), bounds.getX(), bounds.getRight());

    double visibleStartSecs = scrollOffset * totalDurationSecs;
    double visibleDurationSecs = (1.0 / zoomLevel) * totalDurationSecs;
    double visibleEndSecs = visibleStartSecs + visibleDurationSecs;

    double stepSecs = 1.0;
    if (visibleDurationSecs < 0.2)      stepSecs = 0.02;
    else if (visibleDurationSecs < 0.5) stepSecs = 0.05;
    else if (visibleDurationSecs < 1.0) stepSecs = 0.1;
    else if (visibleDurationSecs < 2.5) stepSecs = 0.25;
    else if (visibleDurationSecs < 5.0) stepSecs = 0.5;
    else if (visibleDurationSecs < 15.0) stepSecs = 1.0;
    else if (visibleDurationSecs < 30.0) stepSecs = 2.0;
    else stepSecs = 5.0;

    double firstTick = std::ceil(visibleStartSecs / stepSecs) * stepSecs;

    g.setFont(juce::Font(9.0f));
    g.setColour(OpenWavLookAndFeel::textSecondary);

    for (double t = firstTick; t <= visibleEndSecs; t += stepSecs)
    {
        double ratio = t / totalDurationSecs;
        float tickX = ratioToX(ratio, bounds);
        if (tickX >= bounds.getX() && tickX <= bounds.getRight())
        {
            g.drawVerticalLine(static_cast<int>(tickX), rulerBounds.getBottom() - 5.0f, rulerBounds.getBottom());

            juce::String timeStr;
            if (stepSecs < 0.1)      timeStr = juce::String::formatted("%.3fs", t);
            else if (stepSecs < 1.0) timeStr = juce::String::formatted("%.2fs", t);
            else                     timeStr = juce::String::formatted("%.1fs", t);

            g.drawText(timeStr, tickX - 25.0f, rulerBounds.getY(), 50.0f, 12.0f, juce::Justification::centred);
        }
    }
}

// ─────────────────────────────────────────────────────────
//  Fade Handles Rendering
// ─────────────────────────────────────────────────────────
void EditComponent::paintFadeHandles(juce::Graphics& g, juce::Rectangle<float> bounds) const
{
    if (totalDurationSecs <= 0.0) return;

    double startR = audioEngine.getSampleStartRatio();
    double endR = audioEngine.getSampleEndRatio();

    float fadeY = bounds.getY() + 8.0f;

    // Fade In Handle
    double fadeInRatio = (fadeInMs / 1000.0) / totalDurationSecs;
    float fadeEndX = ratioToX(startR + fadeInRatio, bounds);

    if (fadeEndX >= bounds.getX() && fadeEndX <= bounds.getRight())
    {
        g.setColour(OpenWavLookAndFeel::accentCyan);
        g.fillEllipse(fadeEndX - 5.0f, fadeY - 5.0f, 10.0f, 10.0f);
        g.setColour(juce::Colours::white);
        g.drawEllipse(fadeEndX - 5.0f, fadeY - 5.0f, 10.0f, 10.0f, 1.5f);
    }

    // Fade Out Handle
    double fadeOutRatio = (fadeOutMs / 1000.0) / totalDurationSecs;
    float fadeStartX = ratioToX(endR - fadeOutRatio, bounds);

    if (fadeStartX >= bounds.getX() && fadeStartX <= bounds.getRight())
    {
        g.setColour(OpenWavLookAndFeel::accentCyan);
        g.fillEllipse(fadeStartX - 5.0f, fadeY - 5.0f, 10.0f, 10.0f);
        g.setColour(juce::Colours::white);
        g.drawEllipse(fadeStartX - 5.0f, fadeY - 5.0f, 10.0f, 10.0f, 1.5f);
    }
}

// ─────────────────────────────────────────────────────────
//  Selection Info Pill Overlay
// ─────────────────────────────────────────────────────────
void EditComponent::paintSelectionInfoOverlay(juce::Graphics& g, juce::Rectangle<float> bounds) const
{
    if (totalDurationSecs <= 0.0) return;

    double startR = audioEngine.getSampleStartRatio();
    double endR = audioEngine.getSampleEndRatio();
    double selDurationSecs = (endR - startR) * totalDurationSecs;

    juce::AudioBuffer<float> buf;
    double sr = 44100.0;
    int totalSamples = 0;
    if (audioEngine.getAudioBufferCopy(buf, sr))
        totalSamples = buf.getNumSamples();

    int selSamples = static_cast<int>((endR - startR) * totalSamples);

    juce::String infoStr = juce::String::formatted("Sel: %.3fs (%d smp) | S: %.3fs | E: %.3fs",
                                                   selDurationSecs, selSamples, startR * totalDurationSecs, endR * totalDurationSecs);

    g.setFont(juce::Font(10.0f).boldened());
    int strWidth = g.getCurrentFont().getStringWidth(infoStr) + 16;
    auto pillBounds = juce::Rectangle<float>(bounds.getRight() - strWidth - 10.0f, bounds.getY() + 22.0f, strWidth, 18.0f);

    g.setColour(OpenWavLookAndFeel::bgDark.withAlpha(0.85f));
    g.fillRoundedRectangle(pillBounds, 4.0f);
    g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.6f));
    g.drawRoundedRectangle(pillBounds, 4.0f, 1.0f);

    g.setColour(OpenWavLookAndFeel::accentCyan);
    g.drawText(infoStr, pillBounds, juce::Justification::centred);
}

// ─────────────────────────────────────────────────────────
//  Crop & Reset Actions
// ─────────────────────────────────────────────────────────
void EditComponent::cropToSelection()
{
    if (totalDurationSecs <= 0.0 || !audioEngine.isCurrentSampleInSampler()) return;

    double startR = audioEngine.getSampleStartRatio();
    double endR = audioEngine.getSampleEndRatio();
    if (endR <= startR + 0.0001) return;

    if (audioEngine.cropLoadedSample(startR, endR))
    {
        audioEngine.setSampleRange(0.0, 1.0);
        loopInRatio = 0.0;
        loopOutRatio = 1.0;
        loopMarkersSet = false;
        zoomLevel = 1.0;
        scrollOffset = 0.0;
        zoomSlider.setValue(1.0, juce::dontSendNotification);
        repaint();
    }
}

void EditComponent::resetSelection()
{
    if (totalDurationSecs <= 0.0 || !audioEngine.isCurrentSampleInSampler()) return;

    audioEngine.setSampleRange(0.0, 1.0);
    loopInRatio = 0.0;
    loopOutRatio = 1.0;
    loopMarkersSet = false;
    repaint();
}

void EditComponent::silenceSelectedRegion()
{
    if (totalDurationSecs <= 0.0 || !audioEngine.isCurrentSampleInSampler()) return;

    double startR = audioEngine.getSampleStartRatio();
    double endR = audioEngine.getSampleEndRatio();
    audioEngine.silenceSelection(startR, endR);
    repaint();
}

void EditComponent::reverseSelectedRegion()
{
    if (totalDurationSecs <= 0.0 || !audioEngine.isCurrentSampleInSampler()) return;

    double startR = audioEngine.getSampleStartRatio();
    double endR = audioEngine.getSampleEndRatio();
    audioEngine.reverseSelection(startR, endR);
    repaint();
}

void EditComponent::normalizeAudioPeak()
{
    if (totalDurationSecs <= 0.0 || !audioEngine.isCurrentSampleInSampler()) return;

    audioEngine.normalizeLoadedSample();
    repaint();
}

void EditComponent::deverbSelectedRegion()
{
    if (totalDurationSecs <= 0.0 || !audioEngine.isCurrentSampleInSampler()) return;

    double startR = audioEngine.getSampleStartRatio();
    double endR = audioEngine.getSampleEndRatio();
    if (audioEngine.deverbSelection(startR, endR, 0.88f))
    {
        audioEngine.setPositionRatio(startR);
        audioEngine.play();
        repaint();
    }
}

void EditComponent::bakeFadesIntoBuffer()
{
    if (totalDurationSecs <= 0.0 || !audioEngine.isCurrentSampleInSampler()) return;

    audioEngine.applyFadesToBuffer(fadeInMs, fadeInCurveType, fadeOutMs, fadeOutCurveType);
    fadeInSlider.setValue(0.0);
    fadeOutSlider.setValue(0.0);
    repaint();
}

void EditComponent::playSelectionOnly()
{
    if (totalDurationSecs <= 0.0 || !audioEngine.isCurrentSampleInSampler()) return;

    double startR = audioEngine.getSampleStartRatio();
    audioEngine.setPositionRatio(startR);
    audioEngine.play();
}

// ─────────────────────────────────────────────────────────
//  Keyboard Shortcuts
// ─────────────────────────────────────────────────────────
bool EditComponent::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::spaceKey)
    {
        playPauseButton.triggerClick();
        return true;
    }
    if (key == juce::KeyPress::escapeKey)
    {
        resetSelection();
        return true;
    }
    if (key.getKeyCode() == 'S' || key.getKeyCode() == 's')
    {
        double currentR = currentPositionSecs / totalDurationSecs;
        double endR = audioEngine.getSampleEndRatio();
        if (snapToZeroCrossing) currentR = findNearestZeroCrossing(currentR);
        audioEngine.setSampleRange(currentR, std::max(currentR + 0.001, endR));
        repaint();
        return true;
    }
    if (key.getKeyCode() == 'E' || key.getKeyCode() == 'e')
    {
        double currentR = currentPositionSecs / totalDurationSecs;
        double startR = audioEngine.getSampleStartRatio();
        if (snapToZeroCrossing) currentR = findNearestZeroCrossing(currentR);
        audioEngine.setSampleRange(std::min(startR, currentR - 0.001), currentR);
        repaint();
        return true;
    }
    return false;
}

// ─────────────────────────────────────────────────────────
//  Mouse Hover / Cursors
// ─────────────────────────────────────────────────────────
void EditComponent::mouseMove(const juce::MouseEvent& e)
{
    if (totalDurationSecs <= 0.0 || !audioEngine.isCurrentSampleInSampler())
    {
        setMouseCursor(juce::MouseCursor::NormalCursor);
        return;
    }

    auto wfBounds = getWaveformBounds().reduced(6.0f, 6.0f);
    if (!wfBounds.contains(e.position))
    {
        setMouseCursor(juce::MouseCursor::NormalCursor);
        return;
    }

    const float threshold = 10.0f;
    double startR = audioEngine.getSampleStartRatio();
    double endR = audioEngine.getSampleEndRatio();

    float startX = ratioToX(startR, wfBounds);
    float endX = ratioToX(endR, wfBounds);

    float fadeY = wfBounds.getY() + 8.0f;
    double fadeInRatio = (fadeInMs / 1000.0) / totalDurationSecs;
    double fadeOutRatio = (fadeOutMs / 1000.0) / totalDurationSecs;

    float fadeInX = ratioToX(startR + fadeInRatio, wfBounds);
    float fadeOutX = ratioToX(endR - fadeOutRatio, wfBounds);

    if (std::abs(e.position.x - startX) <= threshold ||
        std::abs(e.position.x - endX) <= threshold ||
        (loopMarkersSet && (std::abs(e.position.x - ratioToX(loopInRatio, wfBounds)) <= threshold ||
                            std::abs(e.position.x - ratioToX(loopOutRatio, wfBounds)) <= threshold)) ||
        (std::abs(e.position.x - fadeInX) <= threshold && std::abs(e.position.y - fadeY) <= threshold) ||
        (std::abs(e.position.x - fadeOutX) <= threshold && std::abs(e.position.y - fadeY) <= threshold))
    {
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    }
    else
    {
        setMouseCursor(juce::MouseCursor::IBeamCursor);
    }
}

// ─────────────────────────────────────────────────────────
//  Mouse Interaction
// ─────────────────────────────────────────────────────────
void EditComponent::mouseDown(const juce::MouseEvent& e)
{
    if (totalDurationSecs <= 0.0 || !audioEngine.isCurrentSampleInSampler()) return;

    auto wfBounds = getWaveformBounds().reduced(6.0f, 6.0f);
    if (!wfBounds.contains(e.position))
        return;

    double clickRatio = xToRatio(e.position.x, wfBounds);
    const float threshold = 12.0f;

    // Check if clicking in time ruler area at top of waveform
    if (e.position.y <= wfBounds.getY() + 18.0f)
    {
        dragTarget = DragTarget::ScrubbingPlayhead;
        audioEngine.setPositionRatio(clickRatio);
        currentPositionSecs = clickRatio * totalDurationSecs;
        repaint();
        return;
    }

    double startR = audioEngine.getSampleStartRatio();
    double endR = audioEngine.getSampleEndRatio();

    float startX = ratioToX(startR, wfBounds);
    float endX = ratioToX(endR, wfBounds);

    // Check fade handles
    float fadeY = wfBounds.getY() + 8.0f;
    double fadeInRatio = (fadeInMs / 1000.0) / totalDurationSecs;
    double fadeOutRatio = (fadeOutMs / 1000.0) / totalDurationSecs;

    float fadeInX = ratioToX(startR + fadeInRatio, wfBounds);
    float fadeOutX = ratioToX(endR - fadeOutRatio, wfBounds);

    if (std::abs(e.position.x - fadeInX) <= threshold && std::abs(e.position.y - fadeY) <= 12.0f)
    {
        dragTarget = DragTarget::FadeInHandle;
        return;
    }
    if (std::abs(e.position.x - fadeOutX) <= threshold && std::abs(e.position.y - fadeY) <= 12.0f)
    {
        dragTarget = DragTarget::FadeOutHandle;
        return;
    }

    // Check loop markers first (they're on top visually)
    if (loopMarkersSet)
    {
        float loopInX = ratioToX(loopInRatio, wfBounds);
        float loopOutX = ratioToX(loopOutRatio, wfBounds);

        if (std::abs(e.position.x - loopInX) <= threshold)
        {
            dragTarget = DragTarget::LoopInMarker;
            return;
        }
        if (std::abs(e.position.x - loopOutX) <= threshold)
        {
            dragTarget = DragTarget::LoopOutMarker;
            return;
        }
    }

    // Start/End markers
    if (std::abs(e.position.x - startX) <= threshold)
    {
        dragTarget = DragTarget::StartMarker;
    }
    else if (std::abs(e.position.x - endX) <= threshold)
    {
        dragTarget = DragTarget::EndMarker;
    }
    else
    {
        dragTarget = DragTarget::SelectingRange;
        dragStartRatio = clickRatio;
        if (snapToZeroCrossing) clickRatio = findNearestZeroCrossing(clickRatio);
        audioEngine.setSampleRange(clickRatio, clickRatio);
    }
    repaint();
}

void EditComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (totalDurationSecs <= 0.0 || dragTarget == DragTarget::None)
        return;

    auto wfBounds = getWaveformBounds().reduced(6.0f, 6.0f);
    double currentRatio = juce::jlimit(0.0, 1.0, xToRatio(e.position.x, wfBounds));

    double startR = audioEngine.getSampleStartRatio();
    double endR = audioEngine.getSampleEndRatio();

    if (snapToZeroCrossing && dragTarget != DragTarget::FadeInHandle && dragTarget != DragTarget::FadeOutHandle)
    {
        currentRatio = findNearestZeroCrossing(currentRatio);
    }

    switch (dragTarget)
    {
        case DragTarget::ScrubbingPlayhead:
            audioEngine.setPositionRatio(currentRatio);
            currentPositionSecs = currentRatio * totalDurationSecs;
            break;
        case DragTarget::StartMarker:
            audioEngine.setSampleRange(currentRatio, endR);
            break;
        case DragTarget::EndMarker:
            audioEngine.setSampleRange(startR, currentRatio);
            break;
        case DragTarget::LoopInMarker:
            loopInRatio = juce::jlimit(0.0, loopOutRatio - 0.001, currentRatio);
            break;
        case DragTarget::LoopOutMarker:
            loopOutRatio = juce::jlimit(loopInRatio + 0.001, 1.0, currentRatio);
            break;
        case DragTarget::FadeInHandle:
        {
            double newFadeInSecs = std::max(0.0, (currentRatio - startR) * totalDurationSecs);
            fadeInSlider.setValue(newFadeInSecs * 1000.0);
            break;
        }
        case DragTarget::FadeOutHandle:
        {
            double newFadeOutSecs = std::max(0.0, (endR - currentRatio) * totalDurationSecs);
            fadeOutSlider.setValue(newFadeOutSecs * 1000.0);
            break;
        }
        case DragTarget::SelectingRange:
        {
            double newStart = std::min(dragStartRatio, currentRatio);
            double newEnd = std::max(dragStartRatio, currentRatio);
            audioEngine.setSampleRange(newStart, newEnd);
            break;
        }
        default:
            break;
    }
    repaint();
}

void EditComponent::mouseUp(const juce::MouseEvent& /*e*/)
{
    if (dragTarget == DragTarget::SelectingRange)
    {
        double startR = audioEngine.getSampleStartRatio();
        double endR = audioEngine.getSampleEndRatio();
        if (endR - startR < 0.005)
        {
            // Too small — treat as a click-to-seek
            audioEngine.setSampleRange(0.0, 1.0);
            audioEngine.setPositionRatio(startR);
        }
        else
        {
            audioEngine.setPositionRatio(startR);
            // Set initial loop markers to the selection if not yet set
            if (!loopMarkersSet)
            {
                loopInRatio = startR;
                loopOutRatio = endR;
                loopMarkersSet = true;
            }
        }
    }
    else if (dragTarget == DragTarget::StartMarker)
    {
        audioEngine.setPositionRatio(audioEngine.getSampleStartRatio());
    }
    else if (dragTarget == DragTarget::FadeInHandle || dragTarget == DragTarget::FadeOutHandle)
    {
        bakeFadesIntoBuffer();
    }

    dragTarget = DragTarget::None;
    repaint();
}

void EditComponent::mouseDoubleClick(const juce::MouseEvent& /*e*/)
{
    if (totalDurationSecs <= 0.0) return;

    // Reset selection to full sample
    audioEngine.setSampleRange(0.0, 1.0);
    audioEngine.setPositionRatio(0.0);
    loopInRatio = 0.0;
    loopOutRatio = 1.0;
    loopMarkersSet = false;
    repaint();
}

void EditComponent::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    auto wfBounds = getWaveformBounds();
    if (!wfBounds.contains(e.position))
    {
        Component::mouseWheelMove(e, wheel);
        return;
    }

    if (e.mods.isCtrlDown() || e.mods.isCommandDown() || std::abs(wheel.deltaY) > 0.001f)
    {
        double mouseRatio = xToRatio(e.position.x, wfBounds);
        double zoomFactor = (wheel.deltaY > 0) ? 1.25 : 0.8;
        double newZoom = juce::jlimit(1.0, 64.0, zoomLevel * zoomFactor);

        if (std::abs(newZoom - zoomLevel) > 0.001)
        {
            double oldVisibleStart = scrollOffset;
            double oldVisibleDuration = 1.0 / zoomLevel;
            double newVisibleDuration = 1.0 / newZoom;

            double cursorFrac = (mouseRatio - oldVisibleStart) / oldVisibleDuration;
            double newVisibleStart = mouseRatio - cursorFrac * newVisibleDuration;

            zoomLevel = newZoom;
            scrollOffset = juce::jlimit(0.0, std::max(0.0, 1.0 - newVisibleDuration), newVisibleStart);

            zoomSlider.setValue(zoomLevel, juce::dontSendNotification);
            hScrollBar.setCurrentRange(scrollOffset, newVisibleDuration, juce::dontSendNotification);
            repaint();
        }
    }
}

// ─────────────────────────────────────────────────────────
//  AudioEngine Callbacks
// ─────────────────────────────────────────────────────────
void EditComponent::playbackStateChanged(bool isPlaying)
{
    playPauseButton.setButtonText(isPlaying ? "Pause" : "Play");
    if (audioEngine.isCurrentSampleInSampler())
    {
        currentPositionSecs = audioEngine.getCurrentPositionSeconds();
        totalDurationSecs = audioEngine.getTotalLengthSeconds();
    }
    else
    {
        totalDurationSecs = 0.0;
    }
    updateControlVisibility();
    repaint();
}

void EditComponent::sampleLoaded(const juce::String& filePath)
{
    if (!audioEngine.isCurrentSampleInSampler())
    {
        sampleNameLabel.setText("", juce::dontSendNotification);
        totalDurationSecs = 0.0;
        currentPositionSecs = 0.0;
        loopInRatio = 0.0;
        loopOutRatio = 1.0;
        loopMarkersSet = false;
        zoomLevel = 1.0;
        scrollOffset = 0.0;
        zoomSlider.setValue(1.0, juce::dontSendNotification);
        updateControlVisibility();
        repaint();
        return;
    }

    juce::File f(filePath);
    sampleNameLabel.setText(f.getFileName(), juce::dontSendNotification);
    totalDurationSecs = audioEngine.getTotalLengthSeconds();
    currentPositionSecs = 0.0;
    loopInRatio = 0.0;
    loopOutRatio = 1.0;
    loopMarkersSet = false;
    zoomLevel = 1.0;
    scrollOffset = 0.0;
    zoomSlider.setValue(1.0, juce::dontSendNotification);
    updateControlVisibility();
    repaint();
}

void EditComponent::timerCallback()
{
    if (audioEngine.isPlaying() && audioEngine.isCurrentSampleInSampler())
    {
        currentPositionSecs = audioEngine.getCurrentPositionSeconds();
        totalDurationSecs = audioEngine.getTotalLengthSeconds();
        repaint();
    }
}

// ─────────────────────────────────────────────────────────
//  Export
// ─────────────────────────────────────────────────────────
void EditComponent::applyFadeToBuffer(juce::AudioBuffer<float>& buffer, double sampleRate) const
{
    int numSamples = buffer.getNumSamples();
    int numChannels = buffer.getNumChannels();

    // Fade In
    int fadeInSamples = static_cast<int>((fadeInMs / 1000.0) * sampleRate);
    fadeInSamples = juce::jmin(fadeInSamples, numSamples);
    for (int s = 0; s < fadeInSamples; ++s)
    {
        float t = static_cast<float>(s) / fadeInSamples;
        float gain = evaluateFadeCurve(t, fadeInCurveType);
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.setSample(ch, s, buffer.getSample(ch, s) * gain);
    }

    // Fade Out
    int fadeOutSamples = static_cast<int>((fadeOutMs / 1000.0) * sampleRate);
    fadeOutSamples = juce::jmin(fadeOutSamples, numSamples);
    for (int s = 0; s < fadeOutSamples; ++s)
    {
        int sampleIdx = numSamples - 1 - s;
        float t = static_cast<float>(s) / fadeOutSamples;
        float gain = evaluateFadeCurve(t, fadeOutCurveType);
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.setSample(ch, sampleIdx, buffer.getSample(ch, sampleIdx) * gain);
    }
}

void EditComponent::exportEdited()
{
    if (totalDurationSecs <= 0.0 || !audioEngine.isCurrentSampleInSampler()) return;

    juce::AudioBuffer<float> fullBuffer;
    double sampleRate = 44100.0;
    if (!audioEngine.getAudioBufferCopy(fullBuffer, sampleRate))
        return;

    double startR = audioEngine.getSampleStartRatio();
    double endR = audioEngine.getSampleEndRatio();
    int totalSamples = fullBuffer.getNumSamples();
    int startSample = static_cast<int>(startR * totalSamples);
    int endSample = static_cast<int>(endR * totalSamples);
    int regionLen = endSample - startSample;

    if (regionLen <= 0) return;

    // Create the edited region buffer
    juce::AudioBuffer<float> editedBuffer(fullBuffer.getNumChannels(), regionLen);
    for (int ch = 0; ch < fullBuffer.getNumChannels(); ++ch)
        editedBuffer.copyFrom(ch, 0, fullBuffer, ch, startSample, regionLen);

    // Apply fades
    applyFadeToBuffer(editedBuffer, sampleRate);

    // File chooser
    juce::File originalFile = audioEngine.getCurrentFile();
    juce::String suggestedName = originalFile.existsAsFile()
        ? originalFile.getFileNameWithoutExtension() + "_edited.wav"
        : "edited_sample.wav";

    auto chooser = std::make_shared<juce::FileChooser>(
        "Export Edited Sample",
        originalFile.existsAsFile() ? originalFile.getParentDirectory() : juce::File::getSpecialLocation(juce::File::userDesktopDirectory),
        "*.wav");

    chooser->launchAsync(
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser, editedBuffer = std::move(editedBuffer), sampleRate](const juce::FileChooser& fc) mutable
        {
            auto result = fc.getResult();
            if (result == juce::File()) return;

            // Ensure .wav extension
            if (!result.hasFileExtension("wav"))
                result = result.withFileExtension("wav");

            result.deleteFile();
            std::unique_ptr<juce::FileOutputStream> outStream(result.createOutputStream());
            if (outStream == nullptr) return;

            juce::WavAudioFormat wavFormat;
            std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(
                outStream.get(),
                sampleRate,
                editedBuffer.getNumChannels(),
                24, // 24-bit output
                {},
                0));

            if (writer != nullptr)
            {
                outStream.release(); // Writer takes ownership
                writer->writeFromAudioSampleBuffer(editedBuffer, 0, editedBuffer.getNumSamples());

                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::InfoIcon,
                    "Export Complete",
                    "Edited sample saved to:\n" + result.getFullPathName(),
                    "OK");
            }
        });
}

} // namespace openwav
