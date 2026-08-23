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
    selectAllButton.onClick = [this] { selectAllRegion(); };
    addAndMakeVisible(selectAllButton);

    deselectAllButton.onClick = [this] { deselectAllRegion(); };
    addAndMakeVisible(deselectAllButton);

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

    // ── Spectral View & Removal Buttons ───────────────
    spectralToggleButton.setClickingTogglesState(true);
    spectralToggleButton.onClick = [this] {
        isSpectralView = spectralToggleButton.getToggleState();
        spectralToggleButton.setButtonText(isSpectralView ? "Spectral: ON" : "Spectral: OFF");
        if (isSpectralView && !spectrogramGenerated)
            generateSpectrogram();
        updateControlVisibility();
        resized();
        repaint();
    };
    addAndMakeVisible(spectralToggleButton);

    repairSpectralButton.setTooltip("Smoothly heal / interpolate corrupted spectral region from adjacent audio frames");
    repairSpectralButton.onClick = [this] { repairSpectralSelection(); };
    addAndMakeVisible(repairSpectralButton);

    deHarmonicButton.setTooltip("Notch out fundamental frequency and first 4 integer harmonics across the selection (De-Hum / De-Whistle)");
    deHarmonicButton.onClick = [this] { deHarmonicSelection(); };
    addAndMakeVisible(deHarmonicButton);

    denoiseSpectralButton.setTooltip("Suppress noise floor in selected time-frequency zone");
    denoiseSpectralButton.onClick = [this] { denoiseSpectralSelection(); };
    addAndMakeVisible(denoiseSpectralButton);

    widenSpectralButton.setTooltip("Enhance stereo side-channel spatial width in selected frequency band");
    widenSpectralButton.onClick = [this] { widenSpectralSelection(); };
    addAndMakeVisible(widenSpectralButton);

    warmthSpectralButton.setTooltip("Add pleasant analog harmonic warmth to selected frequency band");
    warmthSpectralButton.onClick = [this] { warmthSpectralSelection(); };
    addAndMakeVisible(warmthSpectralButton);

    removeSpectralElementButton.setTooltip("Notch out / silence the selected frequency box");
    removeSpectralElementButton.onClick = [this] { removeSpectralSelection(); };
    addAndMakeVisible(removeSpectralElementButton);

    boostSpectralButton.setTooltip("Boost selected spectral box by +6dB");
    boostSpectralButton.onClick = [this] { boostSpectralSelection(); };
    addAndMakeVisible(boostSpectralButton);

    attenuateSpectralButton.setTooltip("Attenuate selected spectral box by -6dB");
    attenuateSpectralButton.onClick = [this] { attenuateSpectralSelection(); };
    addAndMakeVisible(attenuateSpectralButton);

    isolateSpectralButton.setTooltip("Isolate only the selected spectral region via bandpass filtering");
    isolateSpectralButton.onClick = [this] { isolateSpectralSelection(); };
    addAndMakeVisible(isolateSpectralButton);

    // ── DSP Tools ──────────────────────────────────────
    silenceButton.onClick = [this] { silenceSelectedRegion(); };
    addAndMakeVisible(silenceButton);

    reverseButton.onClick = [this] { reverseSelectedRegion(); };
    addAndMakeVisible(reverseButton);

    normalizeButton.onClick = [this] { normalizeAudioPeak(); };
    addAndMakeVisible(normalizeButton);

    gainBoostButton.onClick = [this] {
        audioEngine.adjustGainSelection(audioEngine.getSampleStartRatio(), audioEngine.getSampleEndRatio(), 3.0f);
        restartPlaybackFromStart();
    };
    addAndMakeVisible(gainBoostButton);

    gainCutButton.onClick = [this] {
        audioEngine.adjustGainSelection(audioEngine.getSampleStartRatio(), audioEngine.getSampleEndRatio(), -3.0f);
        restartPlaybackFromStart();
    };
    addAndMakeVisible(gainCutButton);

    autoTrimButton.onClick = [this] {
        audioEngine.autoTrimSilence();
        restartPlaybackFromStart();
    };
    addAndMakeVisible(autoTrimButton);

    hpFilterButton.onClick = [this] {
        audioEngine.applyHighPassFilter(audioEngine.getSampleStartRatio(), audioEngine.getSampleEndRatio(), 80.0f);
        restartPlaybackFromStart();
    };
    addAndMakeVisible(hpFilterButton);

    invertPhaseButton.onClick = [this] {
        audioEngine.invertPhaseSelection(audioEngine.getSampleStartRatio(), audioEngine.getSampleEndRatio());
        restartPlaybackFromStart();
    };
    addAndMakeVisible(invertPhaseButton);

    speed2xButton.onClick = [this] {
        audioEngine.changeSampleSpeed(2.0);
        totalDurationSecs = audioEngine.getTotalLengthSeconds();
        restartPlaybackFromStart();
    };
    addAndMakeVisible(speed2xButton);

    speedHalfButton.onClick = [this] {
        audioEngine.changeSampleSpeed(0.5);
        totalDurationSecs = audioEngine.getTotalLengthSeconds();
        restartPlaybackFromStart();
    };
    addAndMakeVisible(speedHalfButton);

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

    revertOriginalButton.onClick = [this] {
        if (audioEngine.restoreOriginal())
        {
            currentPositionSecs = 0.0;
            loopInRatio = 0.0;
            loopOutRatio = 1.0;
            loopMarkersSet = false;
            zoomLevel = 1.0;
            scrollOffset = 0.0;
            zoomSlider.setValue(1.0, juce::dontSendNotification);
            totalDurationSecs = audioEngine.getTotalLengthSeconds();
            hasSpectralBoxSelection = false;
            spectrogramGenerated = false;
            if (isSpectralView)
                generateSpectrogram();
            updateControlVisibility();
            repaint();
            restartPlaybackFromStart();
        }
    };
    addAndMakeVisible(revertOriginalButton);

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
    bool hasSamplerSample = hasAudioToEdit();
    bool showWaveformTools = (hasSamplerSample && !isSpectralView);
    bool showSpectralTools = (hasSamplerSample && isSpectralView);

    playPauseButton.setVisible(hasSamplerSample);
    stopButton.setVisible(hasSamplerSample);
    playSelButton.setVisible(hasSamplerSample);
    loopToggleButton.setVisible(hasSamplerSample);
    spectralToggleButton.setVisible(hasSamplerSample);

    // Waveform / Time-Domain Tools
    cropButton.setVisible(showWaveformTools);
    resetSelectionButton.setVisible(showWaveformTools);
    selectAllButton.setVisible(showWaveformTools);
    deselectAllButton.setVisible(showWaveformTools);
    snapZeroCrossingButton.setVisible(showWaveformTools);
    silenceButton.setVisible(showWaveformTools);
    reverseButton.setVisible(showWaveformTools);
    normalizeButton.setVisible(showWaveformTools);
    gainBoostButton.setVisible(showWaveformTools);
    gainCutButton.setVisible(showWaveformTools);
    autoTrimButton.setVisible(showWaveformTools);
    hpFilterButton.setVisible(showWaveformTools);
    invertPhaseButton.setVisible(showWaveformTools);
    speed2xButton.setVisible(showWaveformTools);
    speedHalfButton.setVisible(showWaveformTools);
    deverbButton.setVisible(showWaveformTools);
    bakeFadesButton.setVisible(hasSamplerSample);

    // Spectral Tools (visible whenever in Spectral mode)
    repairSpectralButton.setVisible(showSpectralTools);
    deHarmonicButton.setVisible(showSpectralTools);
    denoiseSpectralButton.setVisible(showSpectralTools);
    widenSpectralButton.setVisible(showSpectralTools && audioEngine.getNumChannels() >= 2);
    warmthSpectralButton.setVisible(showSpectralTools);
    removeSpectralElementButton.setVisible(showSpectralTools);
    boostSpectralButton.setVisible(showSpectralTools);
    attenuateSpectralButton.setVisible(showSpectralTools);
    isolateSpectralButton.setVisible(showSpectralTools);

    exportButton.setVisible(hasSamplerSample);
    revertOriginalButton.setVisible(hasSamplerSample && audioEngine.hasOriginalSnapshot());
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

    // Row 1: Transport, Spectral Toggle, Sample Name, Zoom & Export
    playPauseButton.setBounds(row1.removeFromLeft(68));
    row1.removeFromLeft(gap);
    stopButton.setBounds(row1.removeFromLeft(60));
    row1.removeFromLeft(gap);
    playSelButton.setBounds(row1.removeFromLeft(80));
    row1.removeFromLeft(gap);
    loopToggleButton.setBounds(row1.removeFromLeft(65));
    row1.removeFromLeft(gap);
    spectralToggleButton.setBounds(row1.removeFromLeft(95));
    row1.removeFromLeft(12);

    sampleNameLabel.setBounds(row1.removeFromLeft(200));

    // Right side of Row 1: Zoom & Export
    exportButton.setBounds(row1.removeFromRight(90));
    row1.removeFromRight(gap);
    revertOriginalButton.setBounds(row1.removeFromRight(80));
    row1.removeFromRight(gap);
    zoomInButton.setBounds(row1.removeFromRight(26));
    row1.removeFromRight(2);
    zoomOutButton.setBounds(row1.removeFromRight(26));
    row1.removeFromRight(gap);
    zoomSlider.setBounds(row1.removeFromRight(100));
    row1.removeFromRight(gap);
    zoomLabel.setBounds(row1.removeFromRight(40));

    // Row 2: Action Tools (Spectral suite when in spectral mode, DSP suite in waveform mode)
    if (isSpectralView)
    {
        repairSpectralButton.setBounds(row2.removeFromLeft(96));
        row2.removeFromLeft(gap);
        deHarmonicButton.setBounds(row2.removeFromLeft(96));
        row2.removeFromLeft(gap);
        denoiseSpectralButton.setBounds(row2.removeFromLeft(110));
        row2.removeFromLeft(gap);
        if (audioEngine.getNumChannels() >= 2)
        {
            widenSpectralButton.setBounds(row2.removeFromLeft(96));
            row2.removeFromLeft(gap);
        }
        warmthSpectralButton.setBounds(row2.removeFromLeft(72));
        row2.removeFromLeft(gap);
        removeSpectralElementButton.setBounds(row2.removeFromLeft(68));
        row2.removeFromLeft(gap);
        boostSpectralButton.setBounds(row2.removeFromLeft(52));
        row2.removeFromLeft(gap);
        attenuateSpectralButton.setBounds(row2.removeFromLeft(52));
        row2.removeFromLeft(gap);
        isolateSpectralButton.setBounds(row2.removeFromLeft(64));
        row2.removeFromLeft(gap);
        bakeFadesButton.setBounds(row2.removeFromLeft(88));
    }
    else
    {
        selectAllButton.setBounds(row2.removeFromLeft(88));
        row2.removeFromLeft(gap);
        deselectAllButton.setBounds(row2.removeFromLeft(98));
        row2.removeFromLeft(gap);
        cropButton.setBounds(row2.removeFromLeft(60));
        row2.removeFromLeft(gap);
        resetSelectionButton.setBounds(row2.removeFromLeft(64));
        row2.removeFromLeft(gap);
        snapZeroCrossingButton.setBounds(row2.removeFromLeft(84));
        row2.removeFromLeft(10); // Gap to DSP tools

        silenceButton.setBounds(row2.removeFromLeft(70));
        row2.removeFromLeft(gap);
        reverseButton.setBounds(row2.removeFromLeft(74));
        row2.removeFromLeft(gap);
        normalizeButton.setBounds(row2.removeFromLeft(86));
        row2.removeFromLeft(gap);
        gainBoostButton.setBounds(row2.removeFromLeft(54));
        row2.removeFromLeft(gap);
        gainCutButton.setBounds(row2.removeFromLeft(54));
        row2.removeFromLeft(gap);
        autoTrimButton.setBounds(row2.removeFromLeft(84));
        row2.removeFromLeft(gap);
        hpFilterButton.setBounds(row2.removeFromLeft(72));
        row2.removeFromLeft(gap);
        invertPhaseButton.setBounds(row2.removeFromLeft(96));
        row2.removeFromLeft(gap);
        speed2xButton.setBounds(row2.removeFromLeft(76));
        row2.removeFromLeft(gap);
        speedHalfButton.setBounds(row2.removeFromLeft(86));
        row2.removeFromLeft(gap);
        deverbButton.setBounds(row2.removeFromLeft(72));
        row2.removeFromLeft(gap);
        bakeFadesButton.setBounds(row2.removeFromLeft(88));
    }

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
    double sliceStart = isSliceEditingActive ? activeEditingStartRatio : 0.0;
    double sliceEnd = isSliceEditingActive ? activeEditingEndRatio : 1.0;
    if (sliceEnd <= sliceStart) { sliceStart = 0.0; sliceEnd = 1.0; }

    double visibleStart = sliceStart + scrollOffset * (sliceEnd - sliceStart);
    double visibleEnd = sliceStart + (scrollOffset + 1.0 / zoomLevel) * (sliceEnd - sliceStart);

    double normalised = (visibleEnd > visibleStart) ? (ratio - visibleStart) / (visibleEnd - visibleStart) : 0.0;
    return bounds.getX() + static_cast<float>(normalised) * bounds.getWidth();
}

double EditComponent::xToRatio(float x, juce::Rectangle<float> bounds) const
{
    double sliceStart = isSliceEditingActive ? activeEditingStartRatio : 0.0;
    double sliceEnd = isSliceEditingActive ? activeEditingEndRatio : 1.0;
    if (sliceEnd <= sliceStart) { sliceStart = 0.0; sliceEnd = 1.0; }

    double visibleStart = sliceStart + scrollOffset * (sliceEnd - sliceStart);
    double visibleEnd = sliceStart + (scrollOffset + 1.0 / zoomLevel) * (sliceEnd - sliceStart);

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
bool EditComponent::hasAudioToEdit() const
{
    return isSliceEditingActive || totalDurationSecs > 0.0 || audioEngine.getTotalLengthSeconds() > 0.0 || audioEngine.isCurrentSampleInSampler();
}

// ─────────────────────────────────────────────────────────
void EditComponent::paint(juce::Graphics& g)
{
    g.fillAll(OpenWavLookAndFeel::bgDark);

    if (!hasAudioToEdit())
    {
        g.setColour(OpenWavLookAndFeel::textSecondary.withAlpha(0.5f));
        g.setFont(juce::Font(20.0f));
        g.drawText("No sample loaded into editor", getLocalBounds(), juce::Justification::centred);
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
    if (isSpectralView)
        paintSpectrogram(g, innerWf);
    else
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
    double sliceStart = isSliceEditingActive ? activeEditingStartRatio : 0.0;
    double sliceEnd = isSliceEditingActive ? activeEditingEndRatio : 1.0;
    if (sliceEnd <= sliceStart) { sliceStart = 0.0; sliceEnd = 1.0; }

    double visibleStart = sliceStart + scrollOffset * (sliceEnd - sliceStart);
    double visibleEnd = sliceStart + (scrollOffset + 1.0 / zoomLevel) * (sliceEnd - sliceStart);

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
            float lH = (lAbs > 0.001f) ? std::min(halfHeight, std::max(1.0f, lAbs * halfHeight)) : 0.0f;

            bool inSelection = (pixelX >= selStartX && pixelX <= selEndX);

            if (inSelection)
                g.setColour(OpenWavLookAndFeel::textPrimary.withAlpha(0.85f));
            else
                g.setColour(OpenWavLookAndFeel::textSecondary.withAlpha(0.22f));

            if (lH > 0.0f)
            {
                float topY = std::max(bounds.getY(), centerY - lH);
                float botY = std::min(bounds.getBottom(), centerY + lH);
                g.fillRect(juce::Rectangle<float>(pixelX, topY, 1.0f, std::max(1.0f, botY - topY)));
            }
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

void EditComponent::generateSpectrogram()
{
    juce::AudioBuffer<float> buf;
    double sr = 44100.0;
    if (!audioEngine.getAudioBufferCopy(buf, sr) || buf.getNumSamples() < 128)
    {
        spectrogramGenerated = false;
        return;
    }

    constexpr int fftOrder = 10; // 1024 points for high spectral resolution
    constexpr int fftSize = 1 << fftOrder;
    constexpr int numBins = fftSize / 2;
    int totalSamples = buf.getNumSamples();

    int imgWidth = 600;
    int imgHeight = 280;
    spectrogramImage = juce::Image(juce::Image::RGB, imgWidth, imgHeight, true);

    juce::dsp::FFT fft(fftOrder);
    juce::dsp::WindowingFunction<float> window(fftSize, juce::dsp::WindowingFunction<float>::hann);

    std::vector<std::vector<float>> binMagnitudes(imgWidth, std::vector<float>(imgHeight, 0.0f));
    float maxMag = 0.0001f;

    std::vector<float> fftBuffer(fftSize * 2, 0.0f);
    const float* readPtr = buf.getReadPointer(0);

    double nyquist = sr * 0.5;
    double minFreq = 20.0;
    double maxFreq = std::min(20000.0, nyquist);

    for (int x = 0; x < imgWidth; ++x)
    {
        int sampleIdx = static_cast<int>((static_cast<double>(x) / imgWidth) * std::max(0, totalSamples - fftSize));
        if (sampleIdx + fftSize > totalSamples)
            sampleIdx = std::max(0, totalSamples - fftSize);

        std::fill(fftBuffer.begin(), fftBuffer.end(), 0.0f);
        std::copy(readPtr + sampleIdx, readPtr + sampleIdx + fftSize, fftBuffer.begin());
        window.multiplyWithWindowingTable(fftBuffer.data(), fftSize);

        fft.performRealOnlyForwardTransform(fftBuffer.data());

        for (int y = 0; y < imgHeight; ++y)
        {
            double normY = 1.0 - (static_cast<double>(y) / imgHeight);
            double freq = minFreq * std::pow(maxFreq / minFreq, normY);
            int bin = juce::jlimit(0, numBins - 1, static_cast<int>((freq / nyquist) * numBins));

            float real = fftBuffer[2 * bin];
            float imag = fftBuffer[2 * bin + 1];
            float mag = std::sqrt(real * real + imag * imag);

            binMagnitudes[x][y] = mag;
            if (mag > maxMag) maxMag = mag;
        }
    }

    for (int x = 0; x < imgWidth; ++x)
    {
        for (int y = 0; y < imgHeight; ++y)
        {
            float mag = binMagnitudes[x][y];
            float normMag = mag / maxMag;

            float db = juce::Decibels::gainToDecibels(normMag, -60.0f);
            float intensity = juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f);

            juce::Colour pixelCol;
            if (intensity < 0.12f)
            {
                pixelCol = juce::Colour(0x0a, 0x0e, 0x1a);
            }
            else if (intensity < 0.35f)
            {
                float t = (intensity - 0.12f) / 0.23f;
                pixelCol = juce::Colour(0x0a, 0x0e, 0x1a).interpolatedWith(juce::Colour(0x00, 0x99, 0xff), t);
            }
            else if (intensity < 0.65f)
            {
                float t = (intensity - 0.35f) / 0.30f;
                pixelCol = juce::Colour(0x00, 0x99, 0xff).interpolatedWith(juce::Colour(0x00, 0xff, 0x88), t);
            }
            else if (intensity < 0.88f)
            {
                float t = (intensity - 0.65f) / 0.23f;
                pixelCol = juce::Colour(0x00, 0xff, 0x88).interpolatedWith(juce::Colour(0xff, 0xd7, 0x00), t);
            }
            else
            {
                float t = (intensity - 0.88f) / 0.12f;
                pixelCol = juce::Colour(0xff, 0xd7, 0x00).interpolatedWith(juce::Colour(0xff, 0x00, 0x7f), t);
            }

            spectrogramImage.setPixelAt(x, y, pixelCol);
        }
    }

    spectrogramGenerated = true;
}

void EditComponent::paintSpectrogram(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    if (!spectrogramGenerated || spectrogramImage.isNull())
    {
        generateSpectrogram();
    }

    if (spectrogramGenerated && !spectrogramImage.isNull())
    {
        g.drawImage(spectrogramImage, bounds, juce::RectanglePlacement::stretchToFit);
    }

    g.setFont(juce::Font(9.0f).boldened());
    g.setColour(OpenWavLookAndFeel::textSecondary.withAlpha(0.85f));
    g.drawText("20kHz", bounds.getX() + 4, bounds.getY() + 2, 45, 12, juce::Justification::left);
    g.drawText("5kHz", bounds.getX() + 4, bounds.getY() + bounds.getHeight() * 0.25f, 45, 12, juce::Justification::left);
    g.drawText("1kHz", bounds.getX() + 4, bounds.getY() + bounds.getHeight() * 0.50f, 45, 12, juce::Justification::left);
    g.drawText("100Hz", bounds.getX() + 4, bounds.getY() + bounds.getHeight() * 0.75f, 45, 12, juce::Justification::left);
    g.drawText("20Hz", bounds.getX() + 4, bounds.getBottom() - 14, 45, 12, juce::Justification::left);

    if (hasSpectralBoxSelection || dragTarget == DragTarget::SelectingSpectralBox)
    {
        juce::Graphics::ScopedSaveState saveState(g);
        g.reduceClipRegion(bounds.toNearestInt());

        float startX = ratioToX(spectralTimeStart, bounds);
        float endX = ratioToX(spectralTimeEnd, bounds);
        float boxW = std::max(4.0f, endX - startX);

        double minF = 20.0;
        double maxF = 20000.0;
        double normLow = juce::jlimit(0.0, 1.0, std::log(std::max(minF, static_cast<double>(spectralFreqLow)) / minF) / std::log(maxF / minF));
        double normHigh = juce::jlimit(0.0, 1.0, std::log(std::max(minF, static_cast<double>(spectralFreqHigh)) / minF) / std::log(maxF / minF));

        float topY = bounds.getBottom() - static_cast<float>(normHigh) * bounds.getHeight();
        float bottomY = bounds.getBottom() - static_cast<float>(normLow) * bounds.getHeight();
        float boxH = std::max(4.0f, bottomY - topY);

        juce::Rectangle<float> boxRect(startX, topY, boxW, boxH);
        boxRect = boxRect.getIntersection(bounds);

        if (!boxRect.isEmpty())
        {
            g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.25f));
            g.fillRect(boxRect);
            g.setColour(OpenWavLookAndFeel::accentCyan);
            g.drawRect(boxRect, 1.5f);
        }

        juce::String specStr = juce::String::formatted("Spectral: %.2fs - %.2fs | %.0fHz - %.0fHz",
                                                       spectralTimeStart * totalDurationSecs,
                                                       spectralTimeEnd * totalDurationSecs,
                                                       spectralFreqLow, spectralFreqHigh);

        g.setFont(juce::Font(10.0f).boldened());
        int w = g.getCurrentFont().getStringWidth(specStr) + 16;
        auto pillRect = juce::Rectangle<float>(bounds.getX() + 10.0f, bounds.getY() + 8.0f, w, 18.0f);
        g.setColour(OpenWavLookAndFeel::bgDark.withAlpha(0.85f));
        g.fillRoundedRectangle(pillRect, 4.0f);
        g.setColour(OpenWavLookAndFeel::accentCyan);
        g.drawRoundedRectangle(pillRect, 4.0f, 1.0f);
        g.drawText(specStr, pillRect, juce::Justification::centred);
    }
}

void EditComponent::repairSpectralSelection()
{
    if (totalDurationSecs <= 0.0) return;

    double startR = hasSpectralBoxSelection ? spectralTimeStart : audioEngine.getSampleStartRatio();
    double endR = hasSpectralBoxSelection ? spectralTimeEnd : audioEngine.getSampleEndRatio();
    float lowF = hasSpectralBoxSelection ? spectralFreqLow : 20.0f;
    float highF = hasSpectralBoxSelection ? spectralFreqHigh : 20000.0f;

    if (audioEngine.repairSpectralRegion(startR, endR, lowF, highF))
    {
        hasSpectralBoxSelection = false;
        spectrogramGenerated = false;
        if (isSpectralView)
            generateSpectrogram();
        updateControlVisibility();
        repaint();
        restartPlaybackFromStart();
    }
}

void EditComponent::deHarmonicSelection()
{
    if (totalDurationSecs <= 0.0) return;

    double startR = hasSpectralBoxSelection ? spectralTimeStart : audioEngine.getSampleStartRatio();
    double endR = hasSpectralBoxSelection ? spectralTimeEnd : audioEngine.getSampleEndRatio();
    float lowF = hasSpectralBoxSelection ? spectralFreqLow : 50.0f;
    float highF = hasSpectralBoxSelection ? spectralFreqHigh : 200.0f;
    float f0 = std::sqrt(lowF * highF);

    if (audioEngine.removeSpectralHarmonics(startR, endR, f0))
    {
        hasSpectralBoxSelection = false;
        spectrogramGenerated = false;
        if (isSpectralView)
            generateSpectrogram();
        updateControlVisibility();
        repaint();
        restartPlaybackFromStart();
    }
}

void EditComponent::denoiseSpectralSelection()
{
    if (totalDurationSecs <= 0.0) return;

    double startR = hasSpectralBoxSelection ? spectralTimeStart : audioEngine.getSampleStartRatio();
    double endR = hasSpectralBoxSelection ? spectralTimeEnd : audioEngine.getSampleEndRatio();
    float lowF = hasSpectralBoxSelection ? spectralFreqLow : 20.0f;
    float highF = hasSpectralBoxSelection ? spectralFreqHigh : 20000.0f;

    if (audioEngine.denoiseSpectralRegion(startR, endR, lowF, highF, 18.0f))
    {
        spectrogramGenerated = false;
        if (isSpectralView)
            generateSpectrogram();
        updateControlVisibility();
        repaint();
        restartPlaybackFromStart();
    }
}

void EditComponent::widenSpectralSelection()
{
    if (totalDurationSecs <= 0.0) return;

    double startR = hasSpectralBoxSelection ? spectralTimeStart : audioEngine.getSampleStartRatio();
    double endR = hasSpectralBoxSelection ? spectralTimeEnd : audioEngine.getSampleEndRatio();
    float lowF = hasSpectralBoxSelection ? spectralFreqLow : 200.0f;
    float highF = hasSpectralBoxSelection ? spectralFreqHigh : 16000.0f;

    if (audioEngine.widenSpectralRegion(startR, endR, lowF, highF, 2.0f))
    {
        spectrogramGenerated = false;
        if (isSpectralView)
            generateSpectrogram();
        updateControlVisibility();
        repaint();
        restartPlaybackFromStart();
    }
}

void EditComponent::warmthSpectralSelection()
{
    if (totalDurationSecs <= 0.0) return;

    double startR = hasSpectralBoxSelection ? spectralTimeStart : audioEngine.getSampleStartRatio();
    double endR = hasSpectralBoxSelection ? spectralTimeEnd : audioEngine.getSampleEndRatio();
    float lowF = hasSpectralBoxSelection ? spectralFreqLow : 100.0f;
    float highF = hasSpectralBoxSelection ? spectralFreqHigh : 12000.0f;

    if (audioEngine.saturateSpectralRegion(startR, endR, lowF, highF, 0.6f))
    {
        spectrogramGenerated = false;
        if (isSpectralView)
            generateSpectrogram();
        updateControlVisibility();
        repaint();
        restartPlaybackFromStart();
    }
}

void EditComponent::removeSpectralSelection()
{
    if (totalDurationSecs <= 0.0) return;

    double startR = hasSpectralBoxSelection ? spectralTimeStart : audioEngine.getSampleStartRatio();
    double endR = hasSpectralBoxSelection ? spectralTimeEnd : audioEngine.getSampleEndRatio();
    float lowF = hasSpectralBoxSelection ? spectralFreqLow : 20.0f;
    float highF = hasSpectralBoxSelection ? spectralFreqHigh : 20000.0f;

    if (audioEngine.silenceSpectralRegion(startR, endR, lowF, highF))
    {
        hasSpectralBoxSelection = false;
        spectrogramGenerated = false;
        if (isSpectralView)
            generateSpectrogram();
        updateControlVisibility();
        repaint();
        restartPlaybackFromStart();
    }
}

void EditComponent::boostSpectralSelection()
{
    if (totalDurationSecs <= 0.0) return;

    double startR = hasSpectralBoxSelection ? spectralTimeStart : audioEngine.getSampleStartRatio();
    double endR = hasSpectralBoxSelection ? spectralTimeEnd : audioEngine.getSampleEndRatio();
    float lowF = hasSpectralBoxSelection ? spectralFreqLow : 20.0f;
    float highF = hasSpectralBoxSelection ? spectralFreqHigh : 20000.0f;

    if (audioEngine.adjustSpectralRegionGain(startR, endR, lowF, highF, 6.0f))
    {
        spectrogramGenerated = false;
        if (isSpectralView)
            generateSpectrogram();
        updateControlVisibility();
        repaint();
        restartPlaybackFromStart();
    }
}

void EditComponent::attenuateSpectralSelection()
{
    if (totalDurationSecs <= 0.0) return;

    double startR = hasSpectralBoxSelection ? spectralTimeStart : audioEngine.getSampleStartRatio();
    double endR = hasSpectralBoxSelection ? spectralTimeEnd : audioEngine.getSampleEndRatio();
    float lowF = hasSpectralBoxSelection ? spectralFreqLow : 20.0f;
    float highF = hasSpectralBoxSelection ? spectralFreqHigh : 20000.0f;

    if (audioEngine.adjustSpectralRegionGain(startR, endR, lowF, highF, -6.0f))
    {
        spectrogramGenerated = false;
        if (isSpectralView)
            generateSpectrogram();
        updateControlVisibility();
        repaint();
        restartPlaybackFromStart();
    }
}

void EditComponent::isolateSpectralSelection()
{
    if (totalDurationSecs <= 0.0) return;

    double startR = hasSpectralBoxSelection ? spectralTimeStart : audioEngine.getSampleStartRatio();
    double endR = hasSpectralBoxSelection ? spectralTimeEnd : audioEngine.getSampleEndRatio();
    float lowF = hasSpectralBoxSelection ? spectralFreqLow : 20.0f;
    float highF = hasSpectralBoxSelection ? spectralFreqHigh : 20000.0f;

    if (audioEngine.isolateSpectralRegion(startR, endR, lowF, highF))
    {
        hasSpectralBoxSelection = false;
        spectrogramGenerated = false;
        if (isSpectralView)
            generateSpectrogram();
        updateControlVisibility();
        repaint();
        restartPlaybackFromStart();
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
void EditComponent::selectAllRegion()
{
    if (totalDurationSecs <= 0.0) return;
    audioEngine.setSampleRange(0.0, 1.0);
    loopInRatio = 0.0;
    loopOutRatio = 1.0;
    loopMarkersSet = true;
    repaint();
}

void EditComponent::deselectAllRegion()
{
    if (totalDurationSecs <= 0.0) return;
    audioEngine.setSampleRange(0.0, 1.0);
    loopInRatio = 0.0;
    loopOutRatio = 1.0;
    loopMarkersSet = false;
    repaint();
}

void EditComponent::cropToSelection()
{
    if (!hasAudioToEdit()) return;

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
        totalDurationSecs = audioEngine.getTotalLengthSeconds();
        repaint();
        restartPlaybackFromStart();
    }
}

void EditComponent::resetSelection()
{
    if (!hasAudioToEdit()) return;

    audioEngine.setSampleRange(0.0, 1.0);
    loopInRatio = 0.0;
    loopOutRatio = 1.0;
    loopMarkersSet = false;
    repaint();
}

void EditComponent::silenceSelectedRegion()
{
    if (!hasAudioToEdit()) return;

    double startR = audioEngine.getSampleStartRatio();
    double endR = audioEngine.getSampleEndRatio();
    audioEngine.silenceSelection(startR, endR);
    repaint();
    restartPlaybackFromStart();
}

void EditComponent::reverseSelectedRegion()
{
    if (!hasAudioToEdit()) return;

    double startR = audioEngine.getSampleStartRatio();
    double endR = audioEngine.getSampleEndRatio();
    audioEngine.reverseSelection(startR, endR);
    repaint();
    restartPlaybackFromStart();
}

void EditComponent::normalizeAudioPeak()
{
    if (!hasAudioToEdit()) return;

    audioEngine.normalizeLoadedSample();
    repaint();
    restartPlaybackFromStart();
}

void EditComponent::deverbSelectedRegion()
{
    if (!hasAudioToEdit()) return;

    double startR = audioEngine.getSampleStartRatio();
    double endR = audioEngine.getSampleEndRatio();
    if (audioEngine.deverbSelection(startR, endR, 0.75f))
    {
        repaint();
        restartPlaybackFromStart();
    }
}

void EditComponent::bakeFadesIntoBuffer()
{
    if (!hasAudioToEdit()) return;

    audioEngine.applyFadesToBuffer(fadeInMs, fadeInCurveType, fadeOutMs, fadeOutCurveType);
    fadeInSlider.setValue(0.0);
    fadeOutSlider.setValue(0.0);
    repaint();
    restartPlaybackFromStart();
}

void EditComponent::playSelectionOnly()
{
    if (!hasAudioToEdit()) return;

    double startR = audioEngine.getSampleStartRatio();
    audioEngine.setPositionRatio(startR);
    audioEngine.play();
}

// ─────────────────────────────────────────────────────────
//  Keyboard Shortcuts
// ─────────────────────────────────────────────────────────
bool EditComponent::keyPressed(const juce::KeyPress& key)
{
    if (key.getModifiers().isCtrlDown() || key.getModifiers().isCommandDown())
    {
        if (key.getKeyCode() == 'A' || key.getKeyCode() == 'a')
        {
            selectAllRegion();
            return true;
        }
        if (key.getKeyCode() == 'D' || key.getKeyCode() == 'd')
        {
            deselectAllRegion();
            return true;
        }
    }
    if (key == juce::KeyPress::spaceKey)
    {
        playPauseButton.triggerClick();
        return true;
    }
    if (key == juce::KeyPress::escapeKey)
    {
        deselectAllRegion();
        return true;
    }
    if (key.isKeyCode(juce::KeyPress::deleteKey) || key.isKeyCode(juce::KeyPress::backspaceKey))
    {
        if (isSpectralView && hasSpectralBoxSelection)
        {
            removeSpectralSelection();
            return true;
        }
        else if (audioEngine.isCurrentSampleInSampler())
        {
            silenceSelectedRegion();
            return true;
        }
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
    return Component::keyPressed(key);
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

    if (isSpectralView)
    {
        dragTarget = DragTarget::SelectingSpectralBox;
        spectralDragStartPos = e.position;
        spectralTimeStart = clickRatio;
        spectralTimeEnd = clickRatio;

        float normY = juce::jlimit(0.0f, 1.0f, (wfBounds.getBottom() - e.position.y) / wfBounds.getHeight());
        spectralFreqLow = static_cast<float>(20.0 * std::pow(20000.0 / 20.0, normY));
        spectralFreqHigh = spectralFreqLow;
        hasSpectralBoxSelection = true;
        updateControlVisibility();
        repaint();
        return;
    }

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

    // Left-click moves transport position ratio immediately
    audioEngine.setPositionRatio(clickRatio);
    currentPositionSecs = clickRatio * totalDurationSecs;

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
    }
    repaint();
}

void EditComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (totalDurationSecs <= 0.0 || dragTarget == DragTarget::None)
        return;

    auto wfBounds = getWaveformBounds().reduced(6.0f, 6.0f);
    double currentRatio = juce::jlimit(0.0, 1.0, xToRatio(e.position.x, wfBounds));

    if (dragTarget == DragTarget::SelectingSpectralBox)
    {
        double startR = xToRatio(spectralDragStartPos.x, wfBounds);
        spectralTimeStart = std::min(startR, currentRatio);
        spectralTimeEnd = std::max(startR, currentRatio);

        float normY1 = juce::jlimit(0.0f, 1.0f, (wfBounds.getBottom() - spectralDragStartPos.y) / wfBounds.getHeight());
        float normY2 = juce::jlimit(0.0f, 1.0f, (wfBounds.getBottom() - e.position.y) / wfBounds.getHeight());

        float f1 = static_cast<float>(20.0 * std::pow(20000.0 / 20.0, normY1));
        float f2 = static_cast<float>(20.0 * std::pow(20000.0 / 20.0, normY2));

        spectralFreqLow = std::min(f1, f2);
        spectralFreqHigh = std::max(f1, f2);

        hasSpectralBoxSelection = true;
        updateControlVisibility();
        repaint();
        return;
    }

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
    else if (dragTarget == DragTarget::SelectingSpectralBox)
    {
        hasSpectralBoxSelection = (spectralTimeEnd - spectralTimeStart > 0.002);
        updateControlVisibility();
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
void EditComponent::visibilityChanged()
{
    if (isVisible())
    {
        // Auto-load whatever sample is currently in the engine for editing
        if (audioEngine.getTotalLengthSeconds() > 0.0)
        {
            if (!audioEngine.isCurrentSampleInSampler())
                audioEngine.setLoadedInSampler(true);

            if (totalDurationSecs <= 0.0)
                totalDurationSecs = audioEngine.getTotalLengthSeconds();

            auto currentFile = audioEngine.getCurrentFile();
            if (currentFile.existsAsFile())
                sampleNameLabel.setText(currentFile.getFileName(), juce::dontSendNotification);

            // Snapshot for non-destructive editing if not already done
            if (!audioEngine.hasOriginalSnapshot())
                audioEngine.snapshotOriginalForEditing();
        }
        else if (totalDurationSecs <= 0.0)
        {
            totalDurationSecs = audioEngine.getTotalLengthSeconds();
        }

        spectrogramGenerated = false;
        hasSpectralBoxSelection = false;
        if (isSpectralView)
            generateSpectrogram();
        updateControlVisibility();
        repaint();
    }
}

void EditComponent::loadSliceForEditing(int sliceIndex, double startRatio, double endRatio)
{
    activeEditingSliceIndex = sliceIndex;
    activeEditingStartRatio = startRatio;
    activeEditingEndRatio = endRatio;
    isSliceEditingActive = true;

    juce::AudioBuffer<float> masterBuf;
    double sampleRate = 44100.0;
    if (audioEngine.getAudioBufferCopy(masterBuf, sampleRate) && masterBuf.getNumSamples() > 0)
    {
        int totalSamples = masterBuf.getNumSamples();
        int sStart = juce::jlimit(0, totalSamples - 1, static_cast<int>(startRatio * totalSamples));
        int sEnd = juce::jlimit(sStart + 1, totalSamples, static_cast<int>(endRatio * totalSamples));
        int sliceLen = sEnd - sStart;

        if (sliceLen > 0)
        {
            juce::AudioBuffer<float> sliceBuf(masterBuf.getNumChannels(), sliceLen);
            for (int ch = 0; ch < masterBuf.getNumChannels(); ++ch)
                sliceBuf.copyFrom(ch, 0, masterBuf, ch, sStart, sliceLen);

            juce::String sliceName = "Slice #" + juce::String(sliceIndex + 1);
            audioEngine.loadAudioBuffer(sliceName, sliceBuf, sampleRate, 60, true);

            totalDurationSecs = static_cast<double>(sliceLen) / sampleRate;
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
    }
}

void EditComponent::saveChangesAndUpdateSlice()
{
    isSliceEditingActive = false;
    activeEditingSliceIndex = -1;
}

void EditComponent::playbackStateChanged(bool isPlaying)
{
    playPauseButton.setButtonText(isPlaying ? "Pause" : "Play");
    if (hasAudioToEdit())
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
    juce::File f(filePath);

    if (f.getFullPathName().isEmpty() || !f.existsAsFile())
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

    bool isSameFile = (f == audioEngine.getCurrentFile());

    isSliceEditingActive = false;
    activeEditingSliceIndex = -1;

    sampleNameLabel.setText(f.getFileName(), juce::dontSendNotification);
    totalDurationSecs = audioEngine.getTotalLengthSeconds();

    if (!isSameFile || !audioEngine.hasOriginalSnapshot())
    {
        currentPositionSecs = 0.0;
        loopInRatio = 0.0;
        loopOutRatio = 1.0;
        loopMarkersSet = false;
        zoomLevel = 1.0;
        scrollOffset = 0.0;
        zoomSlider.setValue(1.0, juce::dontSendNotification);

        // Snapshot the original buffer for non-destructive editing
        audioEngine.snapshotOriginalForEditing();
    }

    hasSpectralBoxSelection = false;
    spectrogramGenerated = false;
    if (isSpectralView)
    {
        generateSpectrogram();
    }
    updateControlVisibility();
    repaint();
}

void EditComponent::timerCallback()
{
    if (hasAudioToEdit())
    {
        double newPos = audioEngine.getCurrentPositionSeconds();
        double newDur = audioEngine.getTotalLengthSeconds();
        bool needsRepaint = audioEngine.isPlaying() || std::abs(newPos - currentPositionSecs) > 0.0001;

        currentPositionSecs = newPos;
        totalDurationSecs = newDur;

        if (needsRepaint)
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

void EditComponent::restartPlaybackFromStart()
{
    audioEngine.stop();
    currentPositionSecs = 0.0;
    audioEngine.setPositionRatio(0.0);
    audioEngine.play();
    repaint();
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

EditComponentState EditComponent::getState() const
{
    EditComponentState s;
    if (audioEngine.isCurrentSampleInSampler() && audioEngine.getCurrentFile().existsAsFile())
    {
        s.filePath = audioEngine.getCurrentFile().getFullPathName();
        s.sampleStartRatio = audioEngine.getSampleStartRatio();
        s.sampleEndRatio = audioEngine.getSampleEndRatio();
        s.loopInRatio = loopInRatio;
        s.loopOutRatio = loopOutRatio;
        s.loopMarkersSet = loopMarkersSet;
        s.fadeInMs = fadeInMs;
        s.fadeOutMs = fadeOutMs;
        s.fadeInCurveType = fadeInCurveType;
        s.fadeOutCurveType = fadeOutCurveType;
        s.crossfadeMs = crossfadeMs;
        s.zoomLevel = zoomLevel;
        s.scrollOffset = scrollOffset;
        s.snapToZeroCrossing = snapToZeroCrossing;
        s.isSpectralView = isSpectralView;
        s.hasSpectralBoxSelection = hasSpectralBoxSelection;
        s.spectralTimeStart = spectralTimeStart;
        s.spectralTimeEnd = spectralTimeEnd;
        s.spectralFreqLow = spectralFreqLow;
        s.spectralFreqHigh = spectralFreqHigh;
    }
    return s;
}

void EditComponent::setState(const EditComponentState& state)
{
    if (state.filePath.isNotEmpty())
    {
        juce::File f(state.filePath);
        if (f.existsAsFile())
        {
            if (audioEngine.getCurrentFile().getFullPathName() != state.filePath || !audioEngine.isCurrentSampleInSampler())
            {
                audioEngine.loadFile(f, false, true);
            }
            sampleNameLabel.setText(f.getFileName(), juce::dontSendNotification);
            totalDurationSecs = audioEngine.getTotalLengthSeconds();
            audioEngine.setSampleRange(state.sampleStartRatio, state.sampleEndRatio);

            loopInRatio = state.loopInRatio;
            loopOutRatio = state.loopOutRatio;
            loopMarkersSet = state.loopMarkersSet;

            fadeInMs = state.fadeInMs;
            fadeInSlider.setValue(state.fadeInMs, juce::dontSendNotification);
            fadeInMsLabel.setText(juce::String(static_cast<int>(fadeInMs)) + " ms", juce::dontSendNotification);

            fadeOutMs = state.fadeOutMs;
            fadeOutSlider.setValue(state.fadeOutMs, juce::dontSendNotification);
            fadeOutMsLabel.setText(juce::String(static_cast<int>(fadeOutMs)) + " ms", juce::dontSendNotification);

            fadeInCurveType = juce::jlimit(0, 2, state.fadeInCurveType);
            fadeInCurveBox.setSelectedId(fadeInCurveType + 1, juce::dontSendNotification);

            fadeOutCurveType = juce::jlimit(0, 2, state.fadeOutCurveType);
            fadeOutCurveBox.setSelectedId(fadeOutCurveType + 1, juce::dontSendNotification);

            crossfadeMs = state.crossfadeMs;
            crossfadeSlider.setValue(state.crossfadeMs, juce::dontSendNotification);
            crossfadeMsLabel.setText(juce::String(static_cast<int>(crossfadeMs)) + " ms", juce::dontSendNotification);

            zoomLevel = juce::jlimit(1.0, 64.0, state.zoomLevel);
            zoomSlider.setValue(zoomLevel, juce::dontSendNotification);
            scrollOffset = state.scrollOffset;

            snapToZeroCrossing = state.snapToZeroCrossing;
            snapZeroCrossingButton.setToggleState(snapToZeroCrossing, juce::dontSendNotification);

            isSpectralView = state.isSpectralView;
            spectralToggleButton.setToggleState(isSpectralView, juce::dontSendNotification);
            spectralToggleButton.setButtonText(isSpectralView ? "Spectral: ON" : "Spectral: OFF");

            hasSpectralBoxSelection = state.hasSpectralBoxSelection;
            spectralTimeStart = state.spectralTimeStart;
            spectralTimeEnd = state.spectralTimeEnd;
            spectralFreqLow = state.spectralFreqLow;
            spectralFreqHigh = state.spectralFreqHigh;

            updateControlVisibility();
            spectrogramGenerated = false;
            if (isSpectralView && totalDurationSecs > 0.0)
                generateSpectrogram();

            repaint();
        }
    }
}

} // namespace openwav

