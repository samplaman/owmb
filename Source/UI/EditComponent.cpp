#include "EditComponent.h"
#include "OpenWavLookAndFeel.h"
#include "ShortcutManager.h"
#include "ToastOverlay.h"
#include <algorithm>
#include <cmath>
#include <functional>

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
    tooltipWindow = std::make_unique<juce::TooltipWindow>(this, 400);

    // ── Transport buttons ──────────────────────────────
    playPauseButton.setTooltip("Play / Pause (Space)");
    playPauseButton.onClick = [this] {
        if (audioEngine.isPlaying()) audioEngine.pause();
        else                         audioEngine.play();
    };
    addAndMakeVisible(playPauseButton);

    stopButton.setTooltip("Stop Playback");
    stopButton.onClick = [this] { audioEngine.stop(); };
    addAndMakeVisible(stopButton);

    loopToggleButton.setTooltip("Loop Playback (L)");
    loopToggleButton.setClickingTogglesState(true);
    loopToggleButton.setToggleState(audioEngine.isLooping(), juce::dontSendNotification);
    loopToggleButton.onClick = [this] {
        bool looping = loopToggleButton.getToggleState();
        audioEngine.setLooping(looping);
        if (looping && !loopMarkersSet)
        {
            loopInRatio = audioEngine.getSampleStartRatio();
            loopOutRatio = audioEngine.getSampleEndRatio();
            loopMarkersSet = true;
        }
        audioEngine.setLoopPoints(loopInRatio, loopOutRatio);
        audioEngine.setLoopMarkersSet(loopMarkersSet);
        audioEngine.setLoopCrossfadeMs(crossfadeMs);
        repaint();
    };
    addAndMakeVisible(loopToggleButton);

    playSelButton.setComponentID("editIconBtn");
    playSelButton.setTooltip("Play Selection Only");
    playSelButton.onClick = [this] { playSelectionOnly(); };
    addAndMakeVisible(playSelButton);

    // ── Editing buttons ────────────────────────────────
    selectAllButton.setComponentID("editIconBtn");
    selectAllButton.setTooltip("Select All (Cmd/Ctrl + A)");
    selectAllButton.onClick = [this] { selectAllRegion(); };
    addAndMakeVisible(selectAllButton);

    deselectAllButton.setComponentID("editIconBtn");
    deselectAllButton.setTooltip("Deselect All (Esc)");
    deselectAllButton.onClick = [this] { deselectAllRegion(); };
    addAndMakeVisible(deselectAllButton);

    cropButton.setComponentID("editIconBtn");
    cropButton.setTooltip("Crop to Selection");
    cropButton.onClick = [this] { cropToSelection(); };
    addAndMakeVisible(cropButton);

    resetSelectionButton.setComponentID("editIconBtn");
    resetSelectionButton.setTooltip("Reset Selection");
    resetSelectionButton.onClick = [this] { resetSelection(); };
    addAndMakeVisible(resetSelectionButton);

    snapZeroCrossingButton.setComponentID("editIconBtn");
    snapZeroCrossingButton.setTooltip("Snap Selection to Zero-Crossing");
    snapZeroCrossingButton.setClickingTogglesState(true);
    snapZeroCrossingButton.setToggleState(false, juce::dontSendNotification);
    snapZeroCrossingButton.onClick = [this] {
        snapToZeroCrossing = snapZeroCrossingButton.getToggleState();
    };
    addAndMakeVisible(snapZeroCrossingButton);

    // ── Spectral View & Removal Buttons ───────────────
    spectralToggleButton.setComponentID("editIconBtn");
    spectralToggleButton.setTooltip("Toggle Spectral / Time-Frequency View");
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

    auto setupShapeBtn = [this](juce::TextButton& btn, const juce::String& tip, SpectralSelectionShape shape) {
        btn.setComponentID("editIconBtn");
        btn.setTooltip(tip);
        btn.setClickingTogglesState(true);
        btn.setRadioGroupId(9001);
        btn.onClick = [this, shape] {
            currentSpectralShape = shape;
            updateShapeButtonStates();
            repaint();
        };
        addAndMakeVisible(btn);
    };

    setupShapeBtn(spectralBoxSelectBtn, "Spectral Box Select: Draw a 2D time-frequency bounding box", SpectralSelectionShape::Box);
    setupShapeBtn(spectralTimeSelectBtn, "Spectral Time Select: Select entire frequency spectrum over time slice", SpectralSelectionShape::Time);
    setupShapeBtn(spectralFreqSelectBtn, "Spectral Frequency Select: Select full time across specific frequency band", SpectralSelectionShape::Frequency);
    setupShapeBtn(spectralLassoSelectBtn, "Spectral Lasso Select: Freehand freeform polygon selection", SpectralSelectionShape::Lasso);
    setupShapeBtn(spectralEllipseSelectBtn, "Spectral Oval Select: Elliptical time-frequency selection", SpectralSelectionShape::Ellipse);

    updateShapeButtonStates();
    initSpectralCursors();

    repairSpectralButton.setComponentID("editIconBtn");
    repairSpectralButton.setTooltip("Spectral Heal / Inpaint: Interpolate corrupted spectral frames");
    repairSpectralButton.onClick = [this] { repairSpectralSelection(); };
    addAndMakeVisible(repairSpectralButton);

    deHarmonicButton.setComponentID("editIconBtn");
    deHarmonicButton.setTooltip("De-Harmonic: Notch fundamental frequency & harmonics (De-Hum)");
    deHarmonicButton.onClick = [this] { deHarmonicSelection(); };
    addAndMakeVisible(deHarmonicButton);

    denoiseSpectralButton.setComponentID("editIconBtn");
    denoiseSpectralButton.setTooltip("Spectral Denoise: Suppress background noise floor in selection");
    denoiseSpectralButton.onClick = [this] { denoiseSpectralSelection(); };
    addAndMakeVisible(denoiseSpectralButton);

    widenSpectralButton.setComponentID("editIconBtn");
    widenSpectralButton.setTooltip("Stereo Spread: Enhance side-channel spatial width in selection");
    widenSpectralButton.onClick = [this] { widenSpectralSelection(); };
    addAndMakeVisible(widenSpectralButton);

    warmthSpectralButton.setComponentID("editIconBtn");
    warmthSpectralButton.setTooltip("Spectral Warmth: Add analog harmonic warmth to selection");
    warmthSpectralButton.onClick = [this] { warmthSpectralSelection(); };
    addAndMakeVisible(warmthSpectralButton);

    removeSpectralElementButton.setComponentID("editIconBtn");
    removeSpectralElementButton.setTooltip("Remove: Silence / notch out selected spectral box");
    removeSpectralElementButton.onClick = [this] { removeSpectralSelection(); };
    addAndMakeVisible(removeSpectralElementButton);

    boostSpectralButton.setComponentID("editIconBtn");
    boostSpectralButton.setTooltip("Boost Spectral Box (+6dB)");
    boostSpectralButton.onClick = [this] { boostSpectralSelection(); };
    addAndMakeVisible(boostSpectralButton);

    attenuateSpectralButton.setComponentID("editIconBtn");
    attenuateSpectralButton.setTooltip("Attenuate Spectral Box (-6dB)");
    attenuateSpectralButton.onClick = [this] { attenuateSpectralSelection(); };
    addAndMakeVisible(attenuateSpectralButton);

    isolateSpectralButton.setComponentID("editIconBtn");
    isolateSpectralButton.setTooltip("Isolate: Bandpass filter around selected spectral region");
    isolateSpectralButton.onClick = [this] { isolateSpectralSelection(); };
    addAndMakeVisible(isolateSpectralButton);

    // ── DSP Tools ──────────────────────────────────────
    silenceButton.setComponentID("editIconBtn");
    silenceButton.setTooltip("Silence Selection");
    silenceButton.onClick = [this] { silenceSelectedRegion(); };
    addAndMakeVisible(silenceButton);

    reverseButton.setComponentID("editIconBtn");
    reverseButton.setTooltip("Reverse Selection");
    reverseButton.onClick = [this] { reverseSelectedRegion(); };
    addAndMakeVisible(reverseButton);

    normalizeButton.setComponentID("editIconBtn");
    normalizeButton.setTooltip("Normalize Peak to 0dB");
    normalizeButton.onClick = [this] { normalizeAudioPeak(); };
    addAndMakeVisible(normalizeButton);

    gainBoostButton.setComponentID("editIconBtn");
    gainBoostButton.setTooltip("Gain Boost (+3dB)");
    gainBoostButton.onClick = [this] {
        pushUndoState();
        audioEngine.adjustGainSelection(audioEngine.getSampleStartRatio(), audioEngine.getSampleEndRatio(), 3.0f);
        restartPlaybackFromStart();
    };
    addAndMakeVisible(gainBoostButton);

    gainCutButton.setComponentID("editIconBtn");
    gainCutButton.setTooltip("Gain Cut (-3dB)");
    gainCutButton.onClick = [this] {
        pushUndoState();
        audioEngine.adjustGainSelection(audioEngine.getSampleStartRatio(), audioEngine.getSampleEndRatio(), -3.0f);
        restartPlaybackFromStart();
    };
    addAndMakeVisible(gainCutButton);

    autoTrimButton.setComponentID("editIconBtn");
    autoTrimButton.setTooltip("Auto-Trim Silence");
    autoTrimButton.onClick = [this] {
        pushUndoState();
        audioEngine.autoTrimSilence();
        restartPlaybackFromStart();
    };
    addAndMakeVisible(autoTrimButton);

    hpFilterButton.setComponentID("editIconBtn");
    hpFilterButton.setTooltip("Low Cut Filter (80Hz High-Pass)");
    hpFilterButton.onClick = [this] {
        pushUndoState();
        audioEngine.applyHighPassFilter(audioEngine.getSampleStartRatio(), audioEngine.getSampleEndRatio(), 80.0f);
        restartPlaybackFromStart();
    };
    addChildComponent(hpFilterButton);

    invertPhaseButton.setComponentID("editIconBtn");
    invertPhaseButton.setTooltip("Invert Phase (Polarity Flip)");
    invertPhaseButton.onClick = [this] {
        pushUndoState();
        audioEngine.invertPhaseSelection(audioEngine.getSampleStartRatio(), audioEngine.getSampleEndRatio());
        restartPlaybackFromStart();
    };
    addChildComponent(invertPhaseButton);

    speed2xButton.setComponentID("editIconBtn");
    speed2xButton.setTooltip("Double Speed (2x)");
    speed2xButton.onClick = [this] {
        pushUndoState();
        audioEngine.changeSampleSpeed(2.0);
        totalDurationSecs = audioEngine.getTotalLengthSeconds();
        restartPlaybackFromStart();
    };
    addChildComponent(speed2xButton);

    speedHalfButton.setComponentID("editIconBtn");
    speedHalfButton.setTooltip("Half Speed (0.5x)");
    speedHalfButton.onClick = [this] {
        pushUndoState();
        audioEngine.changeSampleSpeed(0.5);
        totalDurationSecs = audioEngine.getTotalLengthSeconds();
        restartPlaybackFromStart();
    };
    addChildComponent(speedHalfButton);

    deverbButton.setComponentID("editIconBtn");
    deverbButton.setTooltip("Deverb: Suppress reverberant tail & room reflections");
    deverbButton.onClick = [this] { deverbSelectedRegion(); };
    addChildComponent(deverbButton);

    bakeFadesButton.setComponentID("editIconBtn");
    bakeFadesButton.setTooltip("Bake Fades into Audio");
    bakeFadesButton.onClick = [this] { bakeFadesIntoBuffer(); };
    addAndMakeVisible(bakeFadesButton);

    loopToSelectionButton.setComponentID("editIconBtn");
    loopToSelectionButton.setTooltip("Set Loop Points to Current Selection & Enable Loop");
    loopToSelectionButton.onClick = [this] {
        double startR = audioEngine.getSampleStartRatio();
        double endR = audioEngine.getSampleEndRatio();
        if (endR - startR < 0.002)
        {
            startR = 0.0;
            endR = 1.0;
        }
        loopInRatio = startR;
        loopOutRatio = endR;
        loopMarkersSet = true;
        audioEngine.setLoopPoints(loopInRatio, loopOutRatio);
        audioEngine.setLoopMarkersSet(true);
        audioEngine.setLooping(true);
        loopToggleButton.setToggleState(true, juce::dontSendNotification);
        repaint();
    };
    addAndMakeVisible(loopToSelectionButton);

    snapLoopZeroButton.setComponentID("editIconBtn");
    snapLoopZeroButton.setTooltip("Snap Loop In & Loop Out to Nearest Zero-Crossings");
    snapLoopZeroButton.onClick = [this] {
        if (!loopMarkersSet)
        {
            loopInRatio = audioEngine.getSampleStartRatio();
            loopOutRatio = audioEngine.getSampleEndRatio();
            loopMarkersSet = true;
        }
        double zIn = findNearestZeroCrossing(loopInRatio);
        double zOut = findNearestZeroCrossing(loopOutRatio);
        if (zOut > zIn + 0.0005)
        {
            loopInRatio = zIn;
            loopOutRatio = zOut;
            audioEngine.setLoopPoints(loopInRatio, loopOutRatio);
            audioEngine.setLoopMarkersSet(loopMarkersSet);
            repaint();
        }
    };
    addAndMakeVisible(snapLoopZeroButton);

    dcOffsetButton.setComponentID("editIconBtn");
    dcOffsetButton.setTooltip("Remove DC Offset in Selection");
    dcOffsetButton.onClick = [this] {
        pushUndoState();
        if (audioEngine.removeDcOffsetSelection(audioEngine.getSampleStartRatio(), audioEngine.getSampleEndRatio()))
        {
            updateSelectionStats();
            repaint();
        }
    };
    addChildComponent(dcOffsetButton);

    lpFilterButton.setComponentID("editIconBtn");
    lpFilterButton.setTooltip("Low Pass / High Cut Filter (8 kHz) on Selection");
    lpFilterButton.onClick = [this] {
        pushUndoState();
        if (audioEngine.applyLowPassFilter(audioEngine.getSampleStartRatio(), audioEngine.getSampleEndRatio(), 8000.0f))
        {
            updateSelectionStats();
            repaint();
        }
    };
    addChildComponent(lpFilterButton);

    monoSumButton.setComponentID("editIconBtn");
    monoSumButton.setTooltip("Sum Stereo Selection to Mono (L+R)/2");
    monoSumButton.onClick = [this] {
        pushUndoState();
        if (audioEngine.monoSumSelection(audioEngine.getSampleStartRatio(), audioEngine.getSampleEndRatio()))
        {
            updateSelectionStats();
            repaint();
        }
    };
    addChildComponent(monoSumButton);

    // ── Fine Loop Nudge controls ──────────────────────
    loopInNudgeLeft.setComponentID("editIconBtn");
    loopInNudgeLeft.setTooltip("Nudge Loop Start Left");
    loopInNudgeLeft.onClick = [this] {
        loopInRatio = juce::jlimit(0.0, loopOutRatio - 0.001, loopInRatio - 0.002);
        loopMarkersSet = true;
        audioEngine.setLoopPoints(loopInRatio, loopOutRatio);
        audioEngine.setLoopMarkersSet(true);
        repaint();
    };
    addAndMakeVisible(loopInNudgeLeft);

    loopInNudgeRight.setComponentID("editIconBtn");
    loopInNudgeRight.setTooltip("Nudge Loop Start Right");
    loopInNudgeRight.onClick = [this] {
        loopInRatio = juce::jlimit(0.0, loopOutRatio - 0.001, loopInRatio + 0.002);
        loopMarkersSet = true;
        audioEngine.setLoopPoints(loopInRatio, loopOutRatio);
        audioEngine.setLoopMarkersSet(true);
        repaint();
    };
    addAndMakeVisible(loopInNudgeRight);

    loopOutNudgeLeft.setComponentID("editIconBtn");
    loopOutNudgeLeft.setTooltip("Nudge Loop End Left");
    loopOutNudgeLeft.onClick = [this] {
        loopOutRatio = juce::jlimit(loopInRatio + 0.001, 1.0, loopOutRatio - 0.002);
        loopMarkersSet = true;
        audioEngine.setLoopPoints(loopInRatio, loopOutRatio);
        audioEngine.setLoopMarkersSet(true);
        repaint();
    };
    addAndMakeVisible(loopOutNudgeLeft);

    loopOutNudgeRight.setComponentID("editIconBtn");
    loopOutNudgeRight.setTooltip("Nudge Loop End Right");
    loopOutNudgeRight.onClick = [this] {
        loopOutRatio = juce::jlimit(loopInRatio + 0.001, 1.0, loopOutRatio + 0.002);
        loopMarkersSet = true;
        audioEngine.setLoopPoints(loopInRatio, loopOutRatio);
        audioEngine.setLoopMarkersSet(true);
        repaint();
    };
    addAndMakeVisible(loopOutNudgeRight);

    undoButton.setComponentID("editIconBtn");
    undoButton.setTooltip("Undo Edit (Cmd/Ctrl+Z)");
    undoButton.onClick = [this] { performUndo(); };
    undoButton.setEnabled(false);
    addAndMakeVisible(undoButton);

    redoButton.setComponentID("editIconBtn");
    redoButton.setTooltip("Redo Edit (Cmd/Ctrl+Shift+Z)");
    redoButton.onClick = [this] { performRedo(); };
    redoButton.setEnabled(false);
    addAndMakeVisible(redoButton);

    saveToLibraryButton.setComponentID("editIconBtn");
    saveToLibraryButton.setTooltip("Save Edited Copy to Library");
    saveToLibraryButton.onClick = [this] { saveEditedCopyToLibrary(); };
    addAndMakeVisible(saveToLibraryButton);

    sendToSamplerButton.setComponentID("editIconBtn");
    sendToSamplerButton.setTooltip("Send Edited Audio to Sample Map");
    sendToSamplerButton.onClick = [this] { sendEditedToSampleMap(); };
    addAndMakeVisible(sendToSamplerButton);

    moreDspButton.setComponentID("editIconBtn");
    moreDspButton.setTooltip("More DSP & Cleanup Tools (Low/Hi Cut, DC Fix, Invert, 2x/0.5x, Deverb, Mono)");
    moreDspButton.onClick = [this] { showMoreDspMenu(); };
    addAndMakeVisible(moreDspButton);

    previewLoopSeamButton.setComponentID("editIconBtn");
    previewLoopSeamButton.setTooltip("Preview Loop Seam (audition last 0.5s across loop boundary)");
    previewLoopSeamButton.onClick = [this] { previewLoopSeam(); };
    addAndMakeVisible(previewLoopSeamButton);

    selectionStatsLabel.setFont(OpenWavLookAndFeel::getMonoFont(10.5f, true));
    selectionStatsLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    selectionStatsLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(selectionStatsLabel);

    exportButton.setComponentID("editIconBtn");
    exportButton.setTooltip("Export Edited Audio");
    exportButton.onClick = [this] { exportEdited(); };
    addAndMakeVisible(exportButton);

    revertOriginalButton.setComponentID("editIconBtn");
    revertOriginalButton.setTooltip("Revert to Original Audio");
    revertOriginalButton.onClick = [this] {
        pushUndoState();
        if (audioEngine.restoreOriginal())
        {
            currentPositionSecs = 0.0;
            loopInRatio = 0.0;
            loopOutRatio = 1.0;
            loopMarkersSet = false;
            fadeInMs = 0.0;
            fadeOutMs = 0.0;
            fadeInSlider.setValue(0.0, juce::dontSendNotification);
            fadeOutSlider.setValue(0.0, juce::dontSendNotification);
            fadeInMsLabel.setText("0 ms", juce::dontSendNotification);
            fadeOutMsLabel.setText("0 ms", juce::dontSendNotification);
            audioEngine.setFades(0.0, fadeInCurveType, 0.0, fadeOutCurveType);
            audioEngine.setLoopPoints(0.0, 1.0);
            audioEngine.setLoopMarkersSet(false);
            zoomLevel = 1.0;
            scrollOffset = 0.0;
            zoomSlider.setValue(1.0, juce::dontSendNotification);
            totalDurationSecs = audioEngine.getTotalLengthSeconds();
            hasSpectralBoxSelection = false;
            lassoScreenPoints.clear();
            spectrogramGenerated = false;
            if (isSpectralView)
                generateSpectrogram();
            updateControlVisibility();
            updateSelectionStats();
            repaint();
            restartPlaybackFromStart();
        }
    };
    addAndMakeVisible(revertOriginalButton);

    OpenWavLookAndFeel::groupButtons({ &playPauseButton, &stopButton, &playSelButton, &loopToggleButton });
    OpenWavLookAndFeel::groupButtons({ &zoomOutButton, &zoomInButton });
    OpenWavLookAndFeel::groupButtons({ &undoButton, &redoButton, &revertOriginalButton, &saveToLibraryButton, &sendToSamplerButton, &exportButton });
    OpenWavLookAndFeel::groupButtons({ &spectralBoxSelectBtn, &spectralTimeSelectBtn, &spectralFreqSelectBtn, &spectralLassoSelectBtn, &spectralEllipseSelectBtn });
    OpenWavLookAndFeel::groupButtons({ &repairSpectralButton, &deHarmonicButton, &denoiseSpectralButton, &widenSpectralButton, &warmthSpectralButton, &removeSpectralElementButton, &boostSpectralButton, &attenuateSpectralButton, &isolateSpectralButton });
    OpenWavLookAndFeel::groupButtons({ &selectAllButton, &deselectAllButton, &cropButton, &resetSelectionButton, &snapZeroCrossingButton });
    OpenWavLookAndFeel::groupButtons({ &silenceButton, &reverseButton, &normalizeButton, &gainBoostButton, &gainCutButton, &autoTrimButton, &bakeFadesButton, &loopToSelectionButton, &snapLoopZeroButton, &moreDspButton });

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

    auto formatFadeLabel = [](double ms) -> juce::String {
        if (ms >= 1000.0)
            return juce::String(ms / 1000.0, 2) + " s";
        return juce::String(static_cast<int>(std::round(ms))) + " ms";
    };

    // ── Fade In controls ───────────────────────────────
    fadeInSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    fadeInSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    fadeInSlider.setRange(0.0, 5000.0, 1.0);
    fadeInSlider.setSkewFactorFromMidPoint(400.0);
    fadeInSlider.setValue(0.0);
    fadeInSlider.onValueChange = [this, formatFadeLabel] {
        double maxSelMs = std::max(1.0, (audioEngine.getSampleEndRatio() - audioEngine.getSampleStartRatio()) * totalDurationSecs * 1000.0);
        double clamped = (totalDurationSecs > 0.0) ? std::min(fadeInSlider.getValue(), std::max(0.0, maxSelMs - fadeOutMs)) : fadeInSlider.getValue();
        fadeInMs = std::max(0.0, clamped);
        if (std::abs(fadeInSlider.getValue() - fadeInMs) > 0.5)
            fadeInSlider.setValue(fadeInMs, juce::dontSendNotification);
        fadeInMsLabel.setText(formatFadeLabel(fadeInMs), juce::dontSendNotification);
        audioEngine.setFades(fadeInMs, fadeInCurveType, fadeOutMs, fadeOutCurveType);
        repaint();
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
        audioEngine.setFades(fadeInMs, fadeInCurveType, fadeOutMs, fadeOutCurveType);
        repaint();
    };
    addAndMakeVisible(fadeInCurveBox);

    // ── Fade Out controls ──────────────────────────────
    fadeOutSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    fadeOutSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    fadeOutSlider.setRange(0.0, 5000.0, 1.0);
    fadeOutSlider.setSkewFactorFromMidPoint(400.0);
    fadeOutSlider.setValue(0.0);
    fadeOutSlider.onValueChange = [this, formatFadeLabel] {
        double maxSelMs = std::max(1.0, (audioEngine.getSampleEndRatio() - audioEngine.getSampleStartRatio()) * totalDurationSecs * 1000.0);
        double clamped = (totalDurationSecs > 0.0) ? std::min(fadeOutSlider.getValue(), std::max(0.0, maxSelMs - fadeInMs)) : fadeOutSlider.getValue();
        fadeOutMs = std::max(0.0, clamped);
        if (std::abs(fadeOutSlider.getValue() - fadeOutMs) > 0.5)
            fadeOutSlider.setValue(fadeOutMs, juce::dontSendNotification);
        fadeOutMsLabel.setText(formatFadeLabel(fadeOutMs), juce::dontSendNotification);
        audioEngine.setFades(fadeInMs, fadeInCurveType, fadeOutMs, fadeOutCurveType);
        repaint();
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
        audioEngine.setFades(fadeInMs, fadeInCurveType, fadeOutMs, fadeOutCurveType);
        repaint();
    };
    addAndMakeVisible(fadeOutCurveBox);

    // ── Crossfade control ──────────────────────────────
    crossfadeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    crossfadeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    crossfadeSlider.setRange(0.0, 500.0, 1.0);
    crossfadeSlider.setSkewFactorFromMidPoint(60.0);
    crossfadeSlider.setValue(0.0);
    crossfadeSlider.onValueChange = [this, formatFadeLabel] {
        double maxLoopMs = (totalDurationSecs > 0.0) ? std::max(0.0, (loopOutRatio - loopInRatio) * totalDurationSecs * 500.0) : 500.0;
        double clamped = (totalDurationSecs > 0.0 && loopMarkersSet) ? std::min(crossfadeSlider.getValue(), maxLoopMs) : crossfadeSlider.getValue();
        crossfadeMs = std::max(0.0, clamped);
        if (std::abs(crossfadeSlider.getValue() - crossfadeMs) > 0.5)
            crossfadeSlider.setValue(crossfadeMs, juce::dontSendNotification);
        crossfadeMsLabel.setText(formatFadeLabel(crossfadeMs), juce::dontSendNotification);
        audioEngine.setLoopCrossfadeMs(crossfadeMs);
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

    zoomInButton.setComponentID("editIconBtn");
    zoomInButton.setTooltip("Zoom In (+)");
    zoomInButton.onClick = [this] {
        zoomSlider.setValue(juce::jmin(64.0, zoomLevel * 1.5));
    };
    addAndMakeVisible(zoomInButton);

    zoomOutButton.setComponentID("editIconBtn");
    zoomOutButton.setTooltip("Zoom Out (-)");
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
    hpFilterButton.setVisible(false);
    invertPhaseButton.setVisible(false);
    speed2xButton.setVisible(false);
    speedHalfButton.setVisible(false);
    deverbButton.setVisible(false);
    bakeFadesButton.setVisible(hasSamplerSample);
    loopToSelectionButton.setVisible(showWaveformTools);
    snapLoopZeroButton.setVisible(showWaveformTools);
    dcOffsetButton.setVisible(false);
    lpFilterButton.setVisible(false);
    monoSumButton.setVisible(false);
    moreDspButton.setVisible(showWaveformTools);

    // Spectral Tools (visible whenever in Spectral mode)
    spectralBoxSelectBtn.setVisible(showSpectralTools);
    spectralTimeSelectBtn.setVisible(showSpectralTools);
    spectralFreqSelectBtn.setVisible(showSpectralTools);
    spectralLassoSelectBtn.setVisible(showSpectralTools);
    spectralEllipseSelectBtn.setVisible(showSpectralTools);
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
    saveToLibraryButton.setVisible(hasSamplerSample);
    sendToSamplerButton.setVisible(hasSamplerSample);
    undoButton.setVisible(hasSamplerSample);
    redoButton.setVisible(hasSamplerSample);
    previewLoopSeamButton.setVisible(hasSamplerSample);
    selectionStatsLabel.setVisible(hasSamplerSample);
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
    playSelButton.setBounds(row1.removeFromLeft(30));
    row1.removeFromLeft(gap);
    loopToggleButton.setBounds(row1.removeFromLeft(65));
    row1.removeFromLeft(gap);
    spectralToggleButton.setBounds(row1.removeFromLeft(95));
    row1.removeFromLeft(12);

    // Right side of Row 1: Zoom & Export
    exportButton.setBounds(row1.removeFromRight(30));
    row1.removeFromRight(gap);
    sendToSamplerButton.setBounds(row1.removeFromRight(30));
    row1.removeFromRight(gap);
    saveToLibraryButton.setBounds(row1.removeFromRight(30));
    row1.removeFromRight(gap);
    revertOriginalButton.setBounds(row1.removeFromRight(30));
    row1.removeFromRight(gap);
    redoButton.setBounds(row1.removeFromRight(30));
    row1.removeFromRight(gap);
    undoButton.setBounds(row1.removeFromRight(30));
    row1.removeFromRight(gap);
    zoomInButton.setBounds(row1.removeFromRight(26));
    row1.removeFromRight(2);
    zoomOutButton.setBounds(row1.removeFromRight(26));
    row1.removeFromRight(gap);
    zoomSlider.setBounds(row1.removeFromRight(90));
    row1.removeFromRight(gap);
    zoomLabel.setBounds(row1.removeFromRight(40));

    // Dynamic space for sample name label
    sampleNameLabel.setBounds(row1);

    const int iconBtnWidth = 30;
    const int iconBtnGap = 4;

    // Row 2: Action Tools (Spectral suite when in spectral mode, DSP suite in waveform mode)
    if (isSpectralView)
    {
        const int shapeBtnWidth = 38;
        spectralBoxSelectBtn.setBounds(row2.removeFromLeft(shapeBtnWidth));
        row2.removeFromLeft(iconBtnGap);
        spectralTimeSelectBtn.setBounds(row2.removeFromLeft(shapeBtnWidth));
        row2.removeFromLeft(iconBtnGap);
        spectralFreqSelectBtn.setBounds(row2.removeFromLeft(shapeBtnWidth));
        row2.removeFromLeft(iconBtnGap);
        spectralLassoSelectBtn.setBounds(row2.removeFromLeft(shapeBtnWidth));
        row2.removeFromLeft(iconBtnGap);
        spectralEllipseSelectBtn.setBounds(row2.removeFromLeft(shapeBtnWidth));
        row2.removeFromLeft(12); // Gap between shape tools and action tools

        repairSpectralButton.setBounds(row2.removeFromLeft(iconBtnWidth));
        row2.removeFromLeft(iconBtnGap);
        deHarmonicButton.setBounds(row2.removeFromLeft(iconBtnWidth));
        row2.removeFromLeft(iconBtnGap);
        denoiseSpectralButton.setBounds(row2.removeFromLeft(iconBtnWidth));
        row2.removeFromLeft(iconBtnGap);
        if (audioEngine.getNumChannels() >= 2)
        {
            widenSpectralButton.setBounds(row2.removeFromLeft(iconBtnWidth));
            row2.removeFromLeft(iconBtnGap);
        }
        warmthSpectralButton.setBounds(row2.removeFromLeft(iconBtnWidth));
        row2.removeFromLeft(iconBtnGap);
        removeSpectralElementButton.setBounds(row2.removeFromLeft(iconBtnWidth));
        row2.removeFromLeft(iconBtnGap);
        boostSpectralButton.setBounds(row2.removeFromLeft(iconBtnWidth));
        row2.removeFromLeft(iconBtnGap);
        attenuateSpectralButton.setBounds(row2.removeFromLeft(iconBtnWidth));
        row2.removeFromLeft(iconBtnGap);
        isolateSpectralButton.setBounds(row2.removeFromLeft(iconBtnWidth));
        row2.removeFromLeft(10);
        bakeFadesButton.setBounds(row2.removeFromLeft(iconBtnWidth));
    }
    else
    {
        selectAllButton.setBounds(row2.removeFromLeft(iconBtnWidth));
        row2.removeFromLeft(iconBtnGap);
        deselectAllButton.setBounds(row2.removeFromLeft(iconBtnWidth));
        row2.removeFromLeft(iconBtnGap);
        cropButton.setBounds(row2.removeFromLeft(iconBtnWidth));
        row2.removeFromLeft(iconBtnGap);
        resetSelectionButton.setBounds(row2.removeFromLeft(iconBtnWidth));
        row2.removeFromLeft(iconBtnGap);
        snapZeroCrossingButton.setBounds(row2.removeFromLeft(iconBtnWidth));
        row2.removeFromLeft(12); // Gap to DSP tools

        silenceButton.setBounds(row2.removeFromLeft(iconBtnWidth));
        row2.removeFromLeft(iconBtnGap);
        reverseButton.setBounds(row2.removeFromLeft(iconBtnWidth));
        row2.removeFromLeft(iconBtnGap);
        normalizeButton.setBounds(row2.removeFromLeft(iconBtnWidth));
        row2.removeFromLeft(iconBtnGap);
        gainBoostButton.setBounds(row2.removeFromLeft(iconBtnWidth));
        row2.removeFromLeft(iconBtnGap);
        gainCutButton.setBounds(row2.removeFromLeft(iconBtnWidth));
        row2.removeFromLeft(iconBtnGap);
        autoTrimButton.setBounds(row2.removeFromLeft(iconBtnWidth));
        row2.removeFromLeft(iconBtnGap);
        bakeFadesButton.setBounds(row2.removeFromLeft(iconBtnWidth));
        row2.removeFromLeft(iconBtnGap);
        loopToSelectionButton.setBounds(row2.removeFromLeft(iconBtnWidth));
        row2.removeFromLeft(iconBtnGap);
        snapLoopZeroButton.setBounds(row2.removeFromLeft(iconBtnWidth));
        row2.removeFromLeft(iconBtnGap);
        moreDspButton.setBounds(row2.removeFromLeft(iconBtnWidth));
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
    int labelW = 68;
    int valueW = 148;
    int itemGap = 6;
    int rowGap = 4;

    // Left panel: time displays + fine loop nudges
    {
        auto r = cpLeft.removeFromTop(rowH);
        cpLeft.removeFromTop(rowGap);
        startLabel.setBounds(r.removeFromLeft(labelW));
        r.removeFromLeft(itemGap);
        startTimeLabel.setBounds(r.removeFromLeft(valueW));
        r.removeFromLeft(8);
        selectionStatsLabel.setBounds(r);
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
        r.removeFromLeft(4);
        previewLoopSeamButton.setBounds(r.removeFromLeft(24));
    }

    // Right panel: fade controls
    int sliderW = 110;
    int msW = 54;
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
        OpenWavLookAndFeel::drawEmptyState(g, getLocalBounds(),
            "No Sample Loaded",
            "Select a sample in List/Cloud or drop an audio file here to edit.");
        return;
    }

    auto wfBounds = getWaveformBounds();

    // Top Toolbar Card Background (matches Recorder / SampleMap 8px rounded cards)
    auto topBarRect = juce::Rectangle<float>(10.0f, 8.0f, static_cast<float>(getWidth() - 20), std::max(32.0f, wfBounds.getY() - 14.0f));
    OpenWavLookAndFeel::drawPanelCard(g, topBarRect, 8.0f, false);

    OpenWavLookAndFeel::drawButtonGroup(g, { &playPauseButton, &stopButton, &playSelButton, &loopToggleButton });
    OpenWavLookAndFeel::drawButtonGroup(g, { &zoomOutButton, &zoomInButton });
    OpenWavLookAndFeel::drawButtonGroup(g, { &revertOriginalButton, &exportButton });
    OpenWavLookAndFeel::drawButtonGroup(g, { &spectralBoxSelectBtn, &spectralTimeSelectBtn, &spectralFreqSelectBtn, &spectralLassoSelectBtn, &spectralEllipseSelectBtn });
    OpenWavLookAndFeel::drawButtonGroup(g, { &repairSpectralButton, &deHarmonicButton, &denoiseSpectralButton, &widenSpectralButton, &warmthSpectralButton, &removeSpectralElementButton, &boostSpectralButton, &attenuateSpectralButton, &isolateSpectralButton });
    OpenWavLookAndFeel::drawButtonGroup(g, { &selectAllButton, &deselectAllButton, &cropButton, &resetSelectionButton, &snapZeroCrossingButton });
    OpenWavLookAndFeel::drawButtonGroup(g, { &silenceButton, &reverseButton, &normalizeButton, &gainBoostButton, &gainCutButton, &autoTrimButton, &hpFilterButton, &invertPhaseButton, &speed2xButton, &speedHalfButton, &deverbButton, &bakeFadesButton, &loopToSelectionButton });

    // Waveform track background
    OpenWavLookAndFeel::drawPanelCard(g, wfBounds, 8.0f, true);

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
    OpenWavLookAndFeel::drawPanelCard(g, cpBounds.toFloat(), 8.0f, false);

    // Update time label text
    double startR = audioEngine.getSampleStartRatio();
    double endR = audioEngine.getSampleEndRatio();
    double selLen = std::max(0.0, (endR - startR) * totalDurationSecs);
    double loopLen = std::max(0.0, (loopOutRatio - loopInRatio) * totalDurationSecs);
    startTimeLabel.setText(formatTime(startR * totalDurationSecs), juce::dontSendNotification);
    endTimeLabel.setText(formatTime(endR * totalDurationSecs) + "  (Len: " + formatTime(selLen) + ")", juce::dontSendNotification);
    loopInTimeLabel.setText(formatTime(loopInRatio * totalDurationSecs), juce::dontSendNotification);
    loopOutTimeLabel.setText(formatTime(loopOutRatio * totalDurationSecs) + (loopMarkersSet ? "  (Len: " + formatTime(loopLen) + ")" : ""), juce::dontSendNotification);
}

void EditComponent::paintOverChildren(juce::Graphics& g)
{
    if (isFileDragOver)
        OpenWavLookAndFeel::drawDropOverlay(g, getLocalBounds(), "Drop audio file to open in Editor");
}

bool EditComponent::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& f : files)
    {
        juce::File file(f);
        if (file.hasFileExtension("wav") || file.hasFileExtension("mp3") ||
            file.hasFileExtension("flac") || file.hasFileExtension("aiff") ||
            file.hasFileExtension("aif") || file.hasFileExtension("ogg"))
            return true;
    }
    return false;
}

void EditComponent::fileDragEnter(const juce::StringArray&, int, int)
{
    isFileDragOver = true;
    repaint();
}

void EditComponent::fileDragExit(const juce::StringArray&)
{
    isFileDragOver = false;
    repaint();
}

void EditComponent::filesDropped(const juce::StringArray& files, int, int)
{
    isFileDragOver = false;
    for (const auto& f : files)
    {
        juce::File file(f);
        if (file.existsAsFile())
        {
            audioEngine.loadFile(file);
            sampleLoaded(file.getFullPathName());
            audioEngine.snapshotOriginalForEditing();
            break;
        }
    }
    repaint();
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

    // Draw visible selection overlay on waveform
    if (endRatio > startRatio && (startRatio > 0.0001 || endRatio < 0.9999 || dragTarget == DragTarget::SelectingRange))
    {
        float leftX = std::max(bounds.getX(), std::min(selStartX, selEndX));
        float rightX = std::min(bounds.getRight(), std::max(selStartX, selEndX));
        float selW = rightX - leftX;
        if (selW > 1.0f)
        {
            // Shaded selection box overlay
            g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.18f));
            g.fillRect(leftX, bounds.getY(), selW, bounds.getHeight());

            // Left and right boundary lines with shadow
            g.setColour(juce::Colours::black.withAlpha(0.85f));
            g.drawVerticalLine(static_cast<int>(leftX) - 1, bounds.getY(), bounds.getBottom());
            g.drawVerticalLine(static_cast<int>(rightX) + 1, bounds.getY(), bounds.getBottom());

            g.setColour(OpenWavLookAndFeel::accentCyan);
            g.drawVerticalLine(static_cast<int>(leftX), bounds.getY(), bounds.getBottom());
            g.drawVerticalLine(static_cast<int>(rightX), bounds.getY(), bounds.getBottom());
        }
    }

    g.saveState();
    g.reduceClipRegion(bounds.toNearestInt());

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

            float lH = (lAbs > 0.001f) ? std::min(halfChanH, std::max(1.0f, lAbs * halfChanH)) : 0.0f;
            float rH = (rAbs > 0.001f) ? std::min(halfChanH, std::max(1.0f, rAbs * halfChanH)) : 0.0f;

            bool inSelection = (pixelX >= selStartX && pixelX <= selEndX);

            if (inSelection)
                g.setColour(OpenWavLookAndFeel::textPrimary.withAlpha(0.85f));
            else
                g.setColour(OpenWavLookAndFeel::textSecondary.withAlpha(0.22f));

            if (lH > 0.0f)
            {
                float topY = std::max(bounds.getY(), lCenterY - lH);
                float botY = std::min(midY, lCenterY + lH);
                g.fillRect(juce::Rectangle<float>(pixelX, topY, 1.0f, std::max(1.0f, botY - topY)));
            }
            if (rH > 0.0f)
            {
                float topY = std::max(midY, rCenterY - rH);
                float botY = std::min(bounds.getBottom(), rCenterY + rH);
                g.fillRect(juce::Rectangle<float>(pixelX, topY, 1.0f, std::max(1.0f, botY - topY)));
            }
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

    g.restoreState();

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
juce::Rectangle<float> EditComponent::getSelectionDragPillBounds(juce::Rectangle<float> bounds) const
{
    double startR = audioEngine.getSampleStartRatio();
    double endR = audioEngine.getSampleEndRatio();
    float selStartX = ratioToX(startR, bounds);
    float selEndX = ratioToX(endR, bounds);
    float selW = selEndX - selStartX;
    if (selW < 46.0f)
        return {};
    float pillW = std::min(56.0f, selW - 24.0f);
    float cx = (selStartX + selEndX) * 0.5f;
    return juce::Rectangle<float>(cx - pillW * 0.5f, bounds.getY() + 1.0f, pillW, 14.0f);
}

void EditComponent::paintMarkers(juce::Graphics& g, juce::Rectangle<float> bounds) const
{
    double startR = audioEngine.getSampleStartRatio();
    double endR = audioEngine.getSampleEndRatio();

    float selStartX = ratioToX(startR, bounds);
    float selEndX = ratioToX(endR, bounds);

    auto drawFloatingPill = [&](float anchorX, float anchorY, const juce::String& text, juce::Colour col) {
        float w = static_cast<float>(std::max(54, text.length() * 6 + 14));
        float h = 16.0f;
        float px = juce::jlimit(bounds.getX() + 2.0f, bounds.getRight() - w - 2.0f, anchorX - w * 0.5f);
        float py = juce::jlimit(bounds.getY() + 2.0f, bounds.getBottom() - h - 2.0f, anchorY);
        g.setColour(OpenWavLookAndFeel::bgDark.withAlpha(0.9f));
        g.fillRoundedRectangle(px, py, w, h, 3.0f);
        g.setColour(col.withAlpha(0.8f));
        g.drawRoundedRectangle(px, py, w, h, 3.0f, 1.0f);
        g.setColour(col);
        g.setFont(juce::Font(9.5f).boldened());
        g.drawText(text, juce::Rectangle<float>(px, py, w, h), juce::Justification::centred);
    };

    // ── Start/End markers (cyan) ──────────────────────
    g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.55f));
    if (selEndX > selStartX)
        g.fillRect(selStartX, bounds.getY(), selEndX - selStartX, 2.0f);

    auto dragPill = getSelectionDragPillBounds(bounds);
    if (!dragPill.isEmpty())
    {
        bool pillHover = (hoverTarget == DragTarget::DragSelectionOut || dragTarget == DragTarget::DragSelectionOut);
        g.setColour(OpenWavLookAndFeel::bgDark.withAlpha(pillHover ? 0.92f : 0.72f));
        g.fillRoundedRectangle(dragPill, 3.0f);
        g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(pillHover ? 1.0f : 0.6f));
        g.drawRoundedRectangle(dragPill, 3.0f, 1.0f);
        g.setFont(juce::Font(8.5f).boldened());
        g.drawText("DRAG", dragPill, juce::Justification::centred);
    }

    bool startActive = (hoverTarget == DragTarget::StartMarker || dragTarget == DragTarget::StartMarker);
    bool endActive = (hoverTarget == DragTarget::EndMarker || dragTarget == DragTarget::EndMarker);

    // Start marker line + handle
    g.setColour(startActive ? juce::Colours::white : OpenWavLookAndFeel::accentCyan);
    g.drawVerticalLine(static_cast<int>(selStartX), bounds.getY(), bounds.getBottom());
    g.fillRoundedRectangle(selStartX - 5.0f, bounds.getY() - 2.0f, 10.0f, 12.0f, 3.0f);
    g.setColour(OpenWavLookAndFeel::bgDark);
    g.setFont(juce::Font(8.0f).boldened());
    g.drawText("S", juce::Rectangle<float>(selStartX - 5.0f, bounds.getY() - 2.0f, 10.0f, 12.0f),
               juce::Justification::centred);

    // End marker line + handle
    g.setColour(endActive ? juce::Colours::white : OpenWavLookAndFeel::accentCyan);
    g.drawVerticalLine(static_cast<int>(selEndX), bounds.getY(), bounds.getBottom());
    g.fillRoundedRectangle(selEndX - 5.0f, bounds.getY() - 2.0f, 10.0f, 12.0f, 3.0f);
    g.setColour(OpenWavLookAndFeel::bgDark);
    g.drawText("E", juce::Rectangle<float>(selEndX - 5.0f, bounds.getY() - 2.0f, 10.0f, 12.0f),
               juce::Justification::centred);

    if (startActive && totalDurationSecs > 0.0)
        drawFloatingPill(selStartX + 34.0f, bounds.getY() + 18.0f, "S: " + formatTime(startR * totalDurationSecs), OpenWavLookAndFeel::accentCyan);
    if (endActive && totalDurationSecs > 0.0)
        drawFloatingPill(selEndX - 34.0f, bounds.getY() + 18.0f, "E: " + formatTime(endR * totalDurationSecs), OpenWavLookAndFeel::accentCyan);

    // ── Loop In/Out markers (green) ───────────────────
    if (loopMarkersSet)
    {
        juce::Colour loopColour = juce::Colour::fromRGB(0, 230, 118); // Emerald green

        float loopInX = ratioToX(loopInRatio, bounds);
        float loopOutX = ratioToX(loopOutRatio, bounds);

        // Loop region tint & bottom draggable loop bar
        if (loopInX < loopOutX)
        {
            g.setColour(loopColour.withAlpha(0.06f));
            g.fillRect(loopInX, bounds.getY(), loopOutX - loopInX, bounds.getHeight());

            bool barActive = (hoverTarget == DragTarget::LoopRegion || dragTarget == DragTarget::LoopRegion);
            g.setColour(loopColour.withAlpha(barActive ? 0.35f : 0.20f));
            g.fillRect(loopInX, bounds.getBottom() - 12.0f, loopOutX - loopInX, 12.0f);
            g.setColour(loopColour.withAlpha(0.6f));
            g.drawHorizontalLine(static_cast<int>(bounds.getBottom() - 12.0f), loopInX, loopOutX);

            if (loopOutX - loopInX > 92.0f && totalDurationSecs > 0.0)
            {
                double loopMs = (loopOutRatio - loopInRatio) * totalDurationSecs * 1000.0;
                juce::String loopText = (loopMs >= 1000.0)
                                            ? ("Loop " + juce::String(loopMs / 1000.0, 2) + "s")
                                            : ("Loop " + juce::String(static_cast<int>(std::round(loopMs))) + "ms");
                g.setColour(OpenWavLookAndFeel::textPrimary.withAlpha(0.9f));
                g.setFont(juce::Font(8.5f).boldened());
                g.drawText(loopText, juce::Rectangle<float>(loopInX + 12.0f, bounds.getBottom() - 12.0f, (loopOutX - loopInX) - 24.0f, 12.0f),
                           juce::Justification::centred, true);
            }
        }

        // Crossfade indicator & equal-power X-Fade curves at loop boundaries
        double maxXfadeRatio = std::max(0.0, (loopOutRatio - loopInRatio) * 0.5);
        double xfadeRatio = (totalDurationSecs > 0.0 && crossfadeMs > 0.0)
                                ? std::min((crossfadeMs / 1000.0) / totalDurationSecs, maxXfadeRatio)
                                : 0.0;

        if (xfadeRatio > 0.0)
        {
            float xfOutStartX = ratioToX(loopOutRatio - xfadeRatio, bounds);
            float xfOutEndX = loopOutX;
            float xfOutW = xfOutEndX - xfOutStartX;

            float xfInStartX = loopInX;
            float xfInEndX = ratioToX(loopInRatio + xfadeRatio, bounds);
            float xfInW = xfInEndX - xfInStartX;

            if (xfOutW > 1.0f)
            {
                g.setColour(loopColour.withAlpha(0.12f));
                g.fillRect(xfOutStartX, bounds.getY(), xfOutW, bounds.getHeight());

                juce::Path outCurve;
                int steps = juce::jmax(2, static_cast<int>(xfOutW));
                for (int s = 0; s <= steps; ++s)
                {
                    float t = static_cast<float>(s) / steps;
                    float wOut = std::cos(t * juce::MathConstants<float>::halfPi);
                    float px = xfOutStartX + t * xfOutW;
                    float py = bounds.getBottom() - wOut * bounds.getHeight();
                    if (s == 0) outCurve.startNewSubPath(px, py);
                    else        outCurve.lineTo(px, py);
                }
                g.setColour(loopColour.withAlpha(0.75f));
                g.strokePath(outCurve, juce::PathStrokeType(1.5f));

                float dashLengths[] = { 3.0f, 3.0f };
                g.setColour(loopColour.withAlpha(0.55f));
                g.drawDashedLine(juce::Line<float>(xfOutStartX, bounds.getY(), xfOutStartX, bounds.getBottom()),
                                 dashLengths, 2, 1.0f);
            }

            if (xfInW > 1.0f)
            {
                g.setColour(loopColour.withAlpha(0.08f));
                g.fillRect(xfInStartX, bounds.getY(), xfInW, bounds.getHeight());

                juce::Path inCurve;
                int steps = juce::jmax(2, static_cast<int>(xfInW));
                for (int s = 0; s <= steps; ++s)
                {
                    float t = static_cast<float>(s) / steps;
                    float wIn = std::sin(t * juce::MathConstants<float>::halfPi);
                    float px = xfInStartX + t * xfInW;
                    float py = bounds.getBottom() - wIn * bounds.getHeight();
                    if (s == 0) inCurve.startNewSubPath(px, py);
                    else        inCurve.lineTo(px, py);
                }
                g.setColour(loopColour.withAlpha(0.65f));
                g.strokePath(inCurve, juce::PathStrokeType(1.5f));
            }
        }

        // Loop In marker
        g.setColour(loopColour);
        g.drawVerticalLine(static_cast<int>(loopInX), bounds.getY(), bounds.getBottom());
        g.fillRoundedRectangle(loopInX - 1.0f, bounds.getBottom() - 14.0f, 12.0f, 14.0f, 2.0f);
        g.setColour(OpenWavLookAndFeel::bgDark);
        g.setFont(juce::Font(8.0f).boldened());
        g.drawText("L", juce::Rectangle<float>(loopInX - 1.0f, bounds.getBottom() - 14.0f, 12.0f, 14.0f),
                   juce::Justification::centred);

        // Loop Out marker
        g.setColour(loopColour);
        g.drawVerticalLine(static_cast<int>(loopOutX), bounds.getY(), bounds.getBottom());
        g.fillRoundedRectangle(loopOutX - 11.0f, bounds.getBottom() - 14.0f, 12.0f, 14.0f, 2.0f);
        g.setColour(OpenWavLookAndFeel::bgDark);
        g.drawText("R", juce::Rectangle<float>(loopOutX - 11.0f, bounds.getBottom() - 14.0f, 12.0f, 14.0f),
                   juce::Justification::centred);

        // X-Fade handle diamond near bottom-right of loop
        float xfHandleX = ratioToX(loopOutRatio - xfadeRatio, bounds);
        float xfHandleY = bounds.getBottom() - 22.0f;
        if (xfHandleX >= bounds.getX() && xfHandleX <= bounds.getRight())
        {
            bool xfActive = (hoverTarget == DragTarget::CrossfadeHandle || dragTarget == DragTarget::CrossfadeHandle);
            juce::Path diamond;
            float r = xfActive ? 5.5f : 4.5f;
            diamond.addTriangle(xfHandleX, xfHandleY - r, xfHandleX + r, xfHandleY, xfHandleX, xfHandleY + r);
            diamond.addTriangle(xfHandleX, xfHandleY - r, xfHandleX - r, xfHandleY, xfHandleX, xfHandleY + r);
            g.setColour(loopColour);
            g.fillPath(diamond);
            g.setColour(juce::Colours::white);
            g.strokePath(diamond, juce::PathStrokeType(1.0f));

            if (xfActive)
            {
                juce::String xfTxt = "X-Fade: " + juce::String(static_cast<int>(std::round(crossfadeMs))) + " ms";
                drawFloatingPill(xfHandleX, xfHandleY - 24.0f, xfTxt, loopColour);
            }
        }

        if ((hoverTarget == DragTarget::LoopInMarker || dragTarget == DragTarget::LoopInMarker) && totalDurationSecs > 0.0)
            drawFloatingPill(loopInX + 34.0f, bounds.getBottom() - 36.0f, "L: " + formatTime(loopInRatio * totalDurationSecs), loopColour);
        if ((hoverTarget == DragTarget::LoopOutMarker || dragTarget == DragTarget::LoopOutMarker) && totalDurationSecs > 0.0)
            drawFloatingPill(loopOutX - 34.0f, bounds.getBottom() - 36.0f, "R: " + formatTime(loopOutRatio * totalDurationSecs), loopColour);
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
                float gain = evaluateFadeCurve(1.0f - t, fadeOutCurveType);
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
                float gain = evaluateFadeCurve(1.0f - t, fadeOutCurveType);
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

    float fadeY = bounds.getY() + 20.0f;

    // Fade In Handle
    double fadeInRatio = (fadeInMs / 1000.0) / totalDurationSecs;
    float fadeEndX = ratioToX(startR + fadeInRatio, bounds);

    if (fadeEndX >= bounds.getX() - 6.0f && fadeEndX <= bounds.getRight() + 6.0f)
    {
        bool active = (hoverTarget == DragTarget::FadeInHandle || dragTarget == DragTarget::FadeInHandle);
        if (active)
        {
            float dashLengths[] = { 3.0f, 3.0f };
            g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.45f));
            g.drawDashedLine(juce::Line<float>(fadeEndX, fadeY, fadeEndX, bounds.getBottom()), dashLengths, 2, 1.0f);
        }
        float r = active ? 6.0f : 5.0f;
        g.setColour(OpenWavLookAndFeel::accentCyan);
        g.fillEllipse(fadeEndX - r, fadeY - r, r * 2.0f, r * 2.0f);
        g.setColour(juce::Colours::white);
        g.drawEllipse(fadeEndX - r, fadeY - r, r * 2.0f, r * 2.0f, 1.5f);

        if (active)
        {
            juce::String txt = "In: " + juce::String(static_cast<int>(std::round(fadeInMs))) + " ms";
            float pillX = juce::jlimit(bounds.getX() + 2.0f, bounds.getRight() - 62.0f, fadeEndX + 8.0f);
            g.setColour(OpenWavLookAndFeel::bgDark.withAlpha(0.88f));
            g.fillRoundedRectangle(pillX, fadeY - 8.0f, 60.0f, 16.0f, 3.0f);
            g.setColour(OpenWavLookAndFeel::accentCyan);
            g.setFont(juce::Font(9.5f).boldened());
            g.drawText(txt, juce::Rectangle<float>(pillX, fadeY - 8.0f, 60.0f, 16.0f), juce::Justification::centred);
        }
    }

    // Fade Out Handle
    double fadeOutRatio = (fadeOutMs / 1000.0) / totalDurationSecs;
    float fadeStartX = ratioToX(endR - fadeOutRatio, bounds);

    if (fadeStartX >= bounds.getX() - 6.0f && fadeStartX <= bounds.getRight() + 6.0f)
    {
        bool active = (hoverTarget == DragTarget::FadeOutHandle || dragTarget == DragTarget::FadeOutHandle);
        if (active)
        {
            float dashLengths[] = { 3.0f, 3.0f };
            g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.45f));
            g.drawDashedLine(juce::Line<float>(fadeStartX, fadeY, fadeStartX, bounds.getBottom()), dashLengths, 2, 1.0f);
        }
        float r = active ? 6.0f : 5.0f;
        g.setColour(OpenWavLookAndFeel::accentCyan);
        g.fillEllipse(fadeStartX - r, fadeY - r, r * 2.0f, r * 2.0f);
        g.setColour(juce::Colours::white);
        g.drawEllipse(fadeStartX - r, fadeY - r, r * 2.0f, r * 2.0f, 1.5f);

        if (active)
        {
            juce::String txt = "Out: " + juce::String(static_cast<int>(std::round(fadeOutMs))) + " ms";
            float pillX = juce::jlimit(bounds.getX() + 2.0f, bounds.getRight() - 64.0f, fadeStartX - 68.0f);
            g.setColour(OpenWavLookAndFeel::bgDark.withAlpha(0.88f));
            g.fillRoundedRectangle(pillX, fadeY - 8.0f, 62.0f, 16.0f, 3.0f);
            g.setColour(OpenWavLookAndFeel::accentCyan);
            g.setFont(juce::Font(9.5f).boldened());
            g.drawText(txt, juce::Rectangle<float>(pillX, fadeY - 8.0f, 62.0f, 16.0f), juce::Justification::centred);
        }
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

juce::Rectangle<float> EditComponent::getSpectralSelectionRect(juce::Rectangle<float> bounds) const
{
    float startX = ratioToX(spectralTimeStart, bounds);
    float endX = ratioToX(spectralTimeEnd, bounds);
    float boxLeft = std::min(startX, endX);
    float boxRight = std::max(startX, endX);
    float boxW = std::max(4.0f, boxRight - boxLeft);

    const double minF = 20.0;
    const double maxF = 20000.0;
    double normLow = juce::jlimit(0.0, 1.0, std::log(std::max(minF, static_cast<double>(spectralFreqLow)) / minF) / std::log(maxF / minF));
    double normHigh = juce::jlimit(0.0, 1.0, std::log(std::max(minF, static_cast<double>(spectralFreqHigh)) / minF) / std::log(maxF / minF));

    float topY = bounds.getBottom() - static_cast<float>(normHigh) * bounds.getHeight();
    float bottomY = bounds.getBottom() - static_cast<float>(normLow) * bounds.getHeight();
    float boxTop = std::min(topY, bottomY);
    float boxBottom = std::max(topY, bottomY);
    float boxH = std::max(4.0f, boxBottom - boxTop);

    if (currentSpectralShape == SpectralSelectionShape::Time)
    {
        boxTop = bounds.getY();
        boxH = bounds.getHeight();
    }
    else if (currentSpectralShape == SpectralSelectionShape::Frequency)
    {
        boxLeft = bounds.getX();
        boxW = bounds.getWidth();
    }

    return juce::Rectangle<float>(boxLeft, boxTop, boxW, boxH).getIntersection(bounds);
}

void EditComponent::paintSpectrogram(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    if (!spectrogramGenerated || spectrogramImage.isNull())
    {
        generateSpectrogram();
    }

    if (spectrogramGenerated && !spectrogramImage.isNull())
    {
        const int imgW = spectrogramImage.getWidth();
        const int imgH = spectrogramImage.getHeight();
        const double visSpan = 1.0 / std::max(1.0, zoomLevel);
        const int srcX = juce::jlimit(0, std::max(0, imgW - 1), static_cast<int>(std::floor(scrollOffset * imgW)));
        const int srcW = juce::jlimit(1, imgW - srcX, static_cast<int>(std::ceil(visSpan * imgW)));
        const auto dest = bounds.toNearestInt();
        g.drawImage(spectrogramImage,
                    dest.getX(), dest.getY(), dest.getWidth(), dest.getHeight(),
                    srcX, 0, srcW, imgH);
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

        juce::Rectangle<float> boxRect = getSpectralSelectionRect(bounds);

        const juce::Colour fillCol = juce::Colour(0, 220, 255).withAlpha(0.28f);
        const juce::Colour neonBorderCol = juce::Colour(0, 245, 255);
        const juce::Colour shadowCol = juce::Colours::black.withAlpha(0.85f);
        const juce::Colour handleCol = juce::Colours::white;

        if (currentSpectralShape == SpectralSelectionShape::Ellipse)
        {
            if (!boxRect.isEmpty())
            {
                g.setColour(fillCol);
                g.fillEllipse(boxRect);

                g.setColour(shadowCol);
                g.drawEllipse(boxRect.expanded(1.0f), 3.0f);

                g.setColour(neonBorderCol);
                g.drawEllipse(boxRect, 2.0f);

                g.setColour(neonBorderCol.withAlpha(0.5f));
                float dashLengths[] = { 4.0f, 4.0f };
                g.drawDashedLine(juce::Line<float>(boxRect.getTopLeft(), boxRect.getTopRight()), dashLengths, 2, 1.0f);
                g.drawDashedLine(juce::Line<float>(boxRect.getTopRight(), boxRect.getBottomRight()), dashLengths, 2, 1.0f);
                g.drawDashedLine(juce::Line<float>(boxRect.getBottomRight(), boxRect.getBottomLeft()), dashLengths, 2, 1.0f);
                g.drawDashedLine(juce::Line<float>(boxRect.getBottomLeft(), boxRect.getTopLeft()), dashLengths, 2, 1.0f);
            }
        }
        else if (currentSpectralShape == SpectralSelectionShape::Lasso)
        {
            if (lassoScreenPoints.size() >= 2)
            {
                auto toScreen = [&](const juce::Point<float>& pt) -> juce::Point<float> {
                    return { ratioToX(pt.x, bounds), bounds.getBottom() - pt.y * bounds.getHeight() };
                };

                juce::Path lassoPath;
                lassoPath.startNewSubPath(toScreen(lassoScreenPoints[0]));
                for (size_t i = 1; i < lassoScreenPoints.size(); ++i)
                {
                    lassoPath.lineTo(toScreen(lassoScreenPoints[i]));
                }
                lassoPath.closeSubPath();

                g.setColour(fillCol);
                g.fillPath(lassoPath);

                g.setColour(shadowCol);
                g.strokePath(lassoPath, juce::PathStrokeType(3.0f));

                g.setColour(neonBorderCol);
                g.strokePath(lassoPath, juce::PathStrokeType(2.0f));

                if (dragTarget == DragTarget::SelectingSpectralBox)
                {
                    g.setColour(neonBorderCol.withAlpha(0.7f));
                    float dashLengths[] = { 3.0f, 3.0f };
                    g.drawDashedLine(juce::Line<float>(toScreen(lassoScreenPoints.back()), toScreen(lassoScreenPoints.front())), dashLengths, 2, 1.2f);
                }

                if (!boxRect.isEmpty())
                {
                    g.setColour(neonBorderCol.withAlpha(0.4f));
                    float dashLengths[] = { 4.0f, 4.0f };
                    g.drawDashedLine(juce::Line<float>(boxRect.getTopLeft(), boxRect.getTopRight()), dashLengths, 2, 1.0f);
                    g.drawDashedLine(juce::Line<float>(boxRect.getTopRight(), boxRect.getBottomRight()), dashLengths, 2, 1.0f);
                    g.drawDashedLine(juce::Line<float>(boxRect.getBottomRight(), boxRect.getBottomLeft()), dashLengths, 2, 1.0f);
                    g.drawDashedLine(juce::Line<float>(boxRect.getBottomLeft(), boxRect.getTopLeft()), dashLengths, 2, 1.0f);
                }
            }
            else if (lassoScreenPoints.size() == 1)
            {
                float px = ratioToX(lassoScreenPoints[0].x, bounds);
                float py = bounds.getBottom() - lassoScreenPoints[0].y * bounds.getHeight();
                g.setColour(neonBorderCol);
                g.fillEllipse(px - 4.0f, py - 4.0f, 8.0f, 8.0f);
            }
        }
        else if (currentSpectralShape == SpectralSelectionShape::Time)
        {
            if (!boxRect.isEmpty())
            {
                g.setColour(fillCol);
                g.fillRect(boxRect);

                g.setColour(shadowCol);
                g.drawVerticalLine(static_cast<int>(boxRect.getX()) - 1, bounds.getY(), bounds.getBottom());
                g.drawVerticalLine(static_cast<int>(boxRect.getRight()) + 1, bounds.getY(), bounds.getBottom());

                g.setColour(neonBorderCol);
                g.drawVerticalLine(static_cast<int>(boxRect.getX()), bounds.getY(), bounds.getBottom());
                g.drawVerticalLine(static_cast<int>(boxRect.getRight()), bounds.getY(), bounds.getBottom());

                g.drawLine(boxRect.getX(), bounds.getY(), boxRect.getRight(), bounds.getY(), 2.5f);
                g.drawLine(boxRect.getX(), bounds.getBottom(), boxRect.getRight(), bounds.getBottom(), 2.5f);
            }
        }
        else if (currentSpectralShape == SpectralSelectionShape::Frequency)
        {
            if (!boxRect.isEmpty())
            {
                g.setColour(fillCol);
                g.fillRect(boxRect);

                g.setColour(shadowCol);
                g.drawHorizontalLine(static_cast<int>(boxRect.getY()) - 1, bounds.getX(), bounds.getRight());
                g.drawHorizontalLine(static_cast<int>(boxRect.getBottom()) + 1, bounds.getX(), bounds.getRight());

                g.setColour(neonBorderCol);
                g.drawHorizontalLine(static_cast<int>(boxRect.getY()), bounds.getX(), bounds.getRight());
                g.drawHorizontalLine(static_cast<int>(boxRect.getBottom()), bounds.getX(), bounds.getRight());

                g.drawLine(bounds.getX(), boxRect.getY(), bounds.getX(), boxRect.getBottom(), 2.5f);
                g.drawLine(bounds.getRight(), boxRect.getY(), bounds.getRight(), boxRect.getBottom(), 2.5f);
            }
        }
        else // Box selection
        {
            if (!boxRect.isEmpty())
            {
                g.setColour(fillCol);
                g.fillRect(boxRect);

                g.setColour(shadowCol);
                g.drawRect(boxRect.expanded(1.0f), 3.0f);

                g.setColour(neonBorderCol);
                g.drawRect(boxRect, 2.0f);

                const float bracketLen = std::min(10.0f, std::min(boxRect.getWidth(), boxRect.getHeight()) * 0.35f);
                if (bracketLen > 3.0f)
                {
                    // Top-Left
                    g.setColour(shadowCol);
                    g.drawLine(boxRect.getX() - 1.0f, boxRect.getY(), boxRect.getX() + bracketLen, boxRect.getY(), 3.5f);
                    g.drawLine(boxRect.getX(), boxRect.getY() - 1.0f, boxRect.getX(), boxRect.getY() + bracketLen, 3.5f);
                    g.setColour(handleCol);
                    g.drawLine(boxRect.getX(), boxRect.getY(), boxRect.getX() + bracketLen, boxRect.getY(), 2.0f);
                    g.drawLine(boxRect.getX(), boxRect.getY(), boxRect.getX(), boxRect.getY() + bracketLen, 2.0f);

                    // Top-Right
                    g.setColour(shadowCol);
                    g.drawLine(boxRect.getRight() - bracketLen, boxRect.getY(), boxRect.getRight() + 1.0f, boxRect.getY(), 3.5f);
                    g.drawLine(boxRect.getRight(), boxRect.getY() - 1.0f, boxRect.getRight(), boxRect.getY() + bracketLen, 3.5f);
                    g.setColour(handleCol);
                    g.drawLine(boxRect.getRight() - bracketLen, boxRect.getY(), boxRect.getRight(), boxRect.getY(), 2.0f);
                    g.drawLine(boxRect.getRight(), boxRect.getY(), boxRect.getRight(), boxRect.getY() + bracketLen, 2.0f);

                    // Bottom-Left
                    g.setColour(shadowCol);
                    g.drawLine(boxRect.getX() - 1.0f, boxRect.getBottom(), boxRect.getX() + bracketLen, boxRect.getBottom(), 3.5f);
                    g.drawLine(boxRect.getX(), boxRect.getBottom() - bracketLen, boxRect.getX(), boxRect.getBottom() + 1.0f, 3.5f);
                    g.setColour(handleCol);
                    g.drawLine(boxRect.getX(), boxRect.getBottom(), boxRect.getX() + bracketLen, boxRect.getBottom(), 2.0f);
                    g.drawLine(boxRect.getX(), boxRect.getBottom() - bracketLen, boxRect.getX(), boxRect.getBottom(), 2.0f);

                    // Bottom-Right
                    g.setColour(shadowCol);
                    g.drawLine(boxRect.getRight() - bracketLen, boxRect.getBottom(), boxRect.getRight() + 1.0f, boxRect.getBottom(), 3.5f);
                    g.drawLine(boxRect.getRight(), boxRect.getBottom() - bracketLen, boxRect.getRight(), boxRect.getBottom() + 1.0f, 3.5f);
                    g.setColour(handleCol);
                    g.drawLine(boxRect.getRight() - bracketLen, boxRect.getBottom(), boxRect.getRight(), boxRect.getBottom(), 2.0f);
                    g.drawLine(boxRect.getRight(), boxRect.getBottom() - bracketLen, boxRect.getRight(), boxRect.getBottom(), 2.0f);
                }

                // Midpoint handle dots
                if (boxRect.getWidth() > 20.0f && boxRect.getHeight() > 20.0f)
                {
                    auto drawMidHandle = [&](float cx, float cy) {
                        g.setColour(shadowCol);
                        g.fillRect(cx - 3.5f, cy - 3.5f, 7.0f, 7.0f);
                        g.setColour(handleCol);
                        g.fillRect(cx - 2.0f, cy - 2.0f, 4.0f, 4.0f);
                    };
                    drawMidHandle(boxRect.getCentreX(), boxRect.getY());
                    drawMidHandle(boxRect.getCentreX(), boxRect.getBottom());
                    drawMidHandle(boxRect.getX(), boxRect.getCentreY());
                    drawMidHandle(boxRect.getRight(), boxRect.getCentreY());
                }
            }
        }

        juce::String shapeName = "Box";
        if (currentSpectralShape == SpectralSelectionShape::Time) shapeName = "Time";
        else if (currentSpectralShape == SpectralSelectionShape::Frequency) shapeName = "Freq";
        else if (currentSpectralShape == SpectralSelectionShape::Lasso) shapeName = "Lasso";
        else if (currentSpectralShape == SpectralSelectionShape::Ellipse) shapeName = "Oval";

        juce::String specStr = juce::String::formatted("Spectral [%s]: %.2fs - %.2fs | %.0fHz - %.0fHz",
                                                       shapeName.toRawUTF8(),
                                                       spectralTimeStart * totalDurationSecs,
                                                       spectralTimeEnd * totalDurationSecs,
                                                       spectralFreqLow, spectralFreqHigh);

        g.setFont(juce::Font(10.0f).boldened());
        int w = g.getCurrentFont().getStringWidth(specStr) + 16;
        auto pillRect = juce::Rectangle<float>(bounds.getX() + 10.0f, bounds.getY() + 8.0f, static_cast<float>(w), 18.0f);
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

    pushUndoState();
    if (audioEngine.repairSpectralRegion(startR, endR, lowF, highF))
    {
        hasSpectralBoxSelection = false;
        lassoScreenPoints.clear();
        spectrogramGenerated = false;
        if (isSpectralView)
            generateSpectrogram();
        updateSelectionStats();
        updateControlVisibility();
        repaint();
        restartPlaybackFromStart();
    }
    else if (!undoStack.empty())
    {
        undoStack.pop_back();
        updateUndoRedoButtons();
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

    pushUndoState();
    if (audioEngine.removeSpectralHarmonics(startR, endR, f0))
    {
        hasSpectralBoxSelection = false;
        lassoScreenPoints.clear();
        spectrogramGenerated = false;
        if (isSpectralView)
            generateSpectrogram();
        updateSelectionStats();
        updateControlVisibility();
        repaint();
        restartPlaybackFromStart();
    }
    else if (!undoStack.empty())
    {
        undoStack.pop_back();
        updateUndoRedoButtons();
    }
}

void EditComponent::denoiseSpectralSelection()
{
    if (totalDurationSecs <= 0.0) return;

    double startR = hasSpectralBoxSelection ? spectralTimeStart : audioEngine.getSampleStartRatio();
    double endR = hasSpectralBoxSelection ? spectralTimeEnd : audioEngine.getSampleEndRatio();
    float lowF = hasSpectralBoxSelection ? spectralFreqLow : 20.0f;
    float highF = hasSpectralBoxSelection ? spectralFreqHigh : 20000.0f;

    pushUndoState();
    if (audioEngine.denoiseSpectralRegion(startR, endR, lowF, highF, 18.0f))
    {
        spectrogramGenerated = false;
        if (isSpectralView)
            generateSpectrogram();
        updateSelectionStats();
        updateControlVisibility();
        repaint();
        restartPlaybackFromStart();
    }
    else if (!undoStack.empty())
    {
        undoStack.pop_back();
        updateUndoRedoButtons();
    }
}

void EditComponent::widenSpectralSelection()
{
    if (totalDurationSecs <= 0.0) return;

    double startR = hasSpectralBoxSelection ? spectralTimeStart : audioEngine.getSampleStartRatio();
    double endR = hasSpectralBoxSelection ? spectralTimeEnd : audioEngine.getSampleEndRatio();
    float lowF = hasSpectralBoxSelection ? spectralFreqLow : 200.0f;
    float highF = hasSpectralBoxSelection ? spectralFreqHigh : 16000.0f;

    pushUndoState();
    if (audioEngine.widenSpectralRegion(startR, endR, lowF, highF, 2.0f))
    {
        spectrogramGenerated = false;
        if (isSpectralView)
            generateSpectrogram();
        updateSelectionStats();
        updateControlVisibility();
        repaint();
        restartPlaybackFromStart();
    }
    else if (!undoStack.empty())
    {
        undoStack.pop_back();
        updateUndoRedoButtons();
    }
}

void EditComponent::warmthSpectralSelection()
{
    if (totalDurationSecs <= 0.0) return;

    double startR = hasSpectralBoxSelection ? spectralTimeStart : audioEngine.getSampleStartRatio();
    double endR = hasSpectralBoxSelection ? spectralTimeEnd : audioEngine.getSampleEndRatio();
    float lowF = hasSpectralBoxSelection ? spectralFreqLow : 100.0f;
    float highF = hasSpectralBoxSelection ? spectralFreqHigh : 12000.0f;

    pushUndoState();
    if (audioEngine.saturateSpectralRegion(startR, endR, lowF, highF, 0.6f))
    {
        spectrogramGenerated = false;
        if (isSpectralView)
            generateSpectrogram();
        updateSelectionStats();
        updateControlVisibility();
        repaint();
        restartPlaybackFromStart();
    }
    else if (!undoStack.empty())
    {
        undoStack.pop_back();
        updateUndoRedoButtons();
    }
}

void EditComponent::removeSpectralSelection()
{
    if (totalDurationSecs <= 0.0) return;

    double startR = hasSpectralBoxSelection ? spectralTimeStart : audioEngine.getSampleStartRatio();
    double endR = hasSpectralBoxSelection ? spectralTimeEnd : audioEngine.getSampleEndRatio();
    float lowF = hasSpectralBoxSelection ? spectralFreqLow : 20.0f;
    float highF = hasSpectralBoxSelection ? spectralFreqHigh : 20000.0f;

    pushUndoState();
    if (audioEngine.silenceSpectralRegion(startR, endR, lowF, highF))
    {
        hasSpectralBoxSelection = false;
        lassoScreenPoints.clear();
        spectrogramGenerated = false;
        if (isSpectralView)
            generateSpectrogram();
        updateSelectionStats();
        updateControlVisibility();
        repaint();
        restartPlaybackFromStart();
    }
    else if (!undoStack.empty())
    {
        undoStack.pop_back();
        updateUndoRedoButtons();
    }
}

void EditComponent::boostSpectralSelection()
{
    if (totalDurationSecs <= 0.0) return;

    double startR = hasSpectralBoxSelection ? spectralTimeStart : audioEngine.getSampleStartRatio();
    double endR = hasSpectralBoxSelection ? spectralTimeEnd : audioEngine.getSampleEndRatio();
    float lowF = hasSpectralBoxSelection ? spectralFreqLow : 20.0f;
    float highF = hasSpectralBoxSelection ? spectralFreqHigh : 20000.0f;

    pushUndoState();
    if (audioEngine.adjustSpectralRegionGain(startR, endR, lowF, highF, 6.0f))
    {
        spectrogramGenerated = false;
        if (isSpectralView)
            generateSpectrogram();
        updateSelectionStats();
        updateControlVisibility();
        repaint();
        restartPlaybackFromStart();
    }
    else if (!undoStack.empty())
    {
        undoStack.pop_back();
        updateUndoRedoButtons();
    }
}

void EditComponent::attenuateSpectralSelection()
{
    if (totalDurationSecs <= 0.0) return;

    double startR = hasSpectralBoxSelection ? spectralTimeStart : audioEngine.getSampleStartRatio();
    double endR = hasSpectralBoxSelection ? spectralTimeEnd : audioEngine.getSampleEndRatio();
    float lowF = hasSpectralBoxSelection ? spectralFreqLow : 20.0f;
    float highF = hasSpectralBoxSelection ? spectralFreqHigh : 20000.0f;

    pushUndoState();
    if (audioEngine.adjustSpectralRegionGain(startR, endR, lowF, highF, -6.0f))
    {
        spectrogramGenerated = false;
        if (isSpectralView)
            generateSpectrogram();
        updateSelectionStats();
        updateControlVisibility();
        repaint();
        restartPlaybackFromStart();
    }
    else if (!undoStack.empty())
    {
        undoStack.pop_back();
        updateUndoRedoButtons();
    }
}

void EditComponent::isolateSpectralSelection()
{
    if (totalDurationSecs <= 0.0) return;

    double startR = hasSpectralBoxSelection ? spectralTimeStart : audioEngine.getSampleStartRatio();
    double endR = hasSpectralBoxSelection ? spectralTimeEnd : audioEngine.getSampleEndRatio();
    float lowF = hasSpectralBoxSelection ? spectralFreqLow : 20.0f;
    float highF = hasSpectralBoxSelection ? spectralFreqHigh : 20000.0f;

    pushUndoState();
    if (audioEngine.isolateSpectralRegion(startR, endR, lowF, highF))
    {
        hasSpectralBoxSelection = false;
        lassoScreenPoints.clear();
        spectrogramGenerated = false;
        if (isSpectralView)
            generateSpectrogram();
        updateSelectionStats();
        updateControlVisibility();
        repaint();
        restartPlaybackFromStart();
    }
    else if (!undoStack.empty())
    {
        undoStack.pop_back();
        updateUndoRedoButtons();
    }
}

// ─────────────────────────────────────────────────────────
//  Selection Info Pill Overlay
// ─────────────────────────────────────────────────────────
void EditComponent::paintSelectionInfoOverlay(juce::Graphics& g, juce::Rectangle<float> bounds) const
{
    if (totalDurationSecs <= 0.0) return;

    juce::String infoStr;
    if (isSpectralView && (hasSpectralBoxSelection || dragTarget == DragTarget::SelectingSpectralBox))
    {
        double selDur = (spectralTimeEnd - spectralTimeStart) * totalDurationSecs;
        infoStr = juce::String::formatted("Spectral Sel: %.3fs - %.3fs (%.3fs) | %.0fHz - %.0fHz",
                                          spectralTimeStart * totalDurationSecs,
                                          spectralTimeEnd * totalDurationSecs,
                                          selDur,
                                          spectralFreqLow,
                                          spectralFreqHigh);
    }
    else
    {
        double startR = audioEngine.getSampleStartRatio();
        double endR = audioEngine.getSampleEndRatio();
        double selDurationSecs = (endR - startR) * totalDurationSecs;

        juce::AudioBuffer<float> buf;
        double sr = 44100.0;
        int totalSamples = 0;
        if (audioEngine.getAudioBufferCopy(buf, sr))
            totalSamples = buf.getNumSamples();

        int selSamples = static_cast<int>((endR - startR) * totalSamples);

        infoStr = juce::String::formatted("Sel: %.3fs (%d smp) | S: %.3fs | E: %.3fs",
                                          selDurationSecs, selSamples, startR * totalDurationSecs, endR * totalDurationSecs);
    }

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
    updateSelectionStats();
    repaint();
}

void EditComponent::deselectAllRegion()
{
    if (totalDurationSecs <= 0.0) return;
    audioEngine.setSampleRange(0.0, 1.0);
    loopInRatio = 0.0;
    loopOutRatio = 1.0;
    loopMarkersSet = false;
    hasSpectralBoxSelection = false;
    lassoScreenPoints.clear();
    updateSelectionStats();
    repaint();
}

void EditComponent::cropToSelection()
{
    if (!hasAudioToEdit()) return;

    double startR = audioEngine.getSampleStartRatio();
    double endR = audioEngine.getSampleEndRatio();
    if (endR <= startR + 0.0001) return;

    pushUndoState();
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
        updateSelectionStats();
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
    updateSelectionStats();
    repaint();
}

void EditComponent::silenceSelectedRegion()
{
    if (!hasAudioToEdit()) return;

    pushUndoState();
    double startR = audioEngine.getSampleStartRatio();
    double endR = audioEngine.getSampleEndRatio();
    audioEngine.silenceSelection(startR, endR);
    updateSelectionStats();
    repaint();
    restartPlaybackFromStart();
}

void EditComponent::reverseSelectedRegion()
{
    if (!hasAudioToEdit()) return;

    pushUndoState();
    double startR = audioEngine.getSampleStartRatio();
    double endR = audioEngine.getSampleEndRatio();
    audioEngine.reverseSelection(startR, endR);
    updateSelectionStats();
    repaint();
    restartPlaybackFromStart();
}

void EditComponent::normalizeAudioPeak()
{
    if (!hasAudioToEdit()) return;

    pushUndoState();
    audioEngine.normalizeLoadedSample();
    updateSelectionStats();
    repaint();
    restartPlaybackFromStart();
}

void EditComponent::deverbSelectedRegion()
{
    if (!hasAudioToEdit()) return;

    pushUndoState();
    double startR = audioEngine.getSampleStartRatio();
    double endR = audioEngine.getSampleEndRatio();
    if (audioEngine.deverbSelection(startR, endR, 0.75f))
    {
        updateSelectionStats();
        repaint();
        restartPlaybackFromStart();
    }
}

void EditComponent::bakeFadesIntoBuffer()
{
    if (!hasAudioToEdit()) return;

    pushUndoState();
    audioEngine.applyFadesToBuffer(fadeInMs, fadeInCurveType, fadeOutMs, fadeOutCurveType);
    fadeInMs = 0.0;
    fadeOutMs = 0.0;
    fadeInSlider.setValue(0.0, juce::dontSendNotification);
    fadeOutSlider.setValue(0.0, juce::dontSendNotification);
    fadeInMsLabel.setText("0 ms", juce::dontSendNotification);
    fadeOutMsLabel.setText("0 ms", juce::dontSendNotification);
    audioEngine.setFades(0.0, fadeInCurveType, 0.0, fadeOutCurveType);
    updateSelectionStats();
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
    auto& sm = ShortcutManager::getInstance();
    const bool cmd = key.getModifiers().isCommandDown();
    const bool shift = key.getModifiers().isShiftDown();
    const bool noMods = !cmd && !key.getModifiers().isAltDown() && !key.getModifiers().isCtrlDown();

    if (cmd && shift && (key.getKeyCode() == 'Z' || key.getKeyCode() == 'z'))
    {
        performRedo();
        return true;
    }
    if (cmd && (key.getKeyCode() == 'Y' || key.getKeyCode() == 'y'))
    {
        performRedo();
        return true;
    }
    if (cmd && !shift && (key.getKeyCode() == 'Z' || key.getKeyCode() == 'z'))
    {
        performUndo();
        return true;
    }
    if (shift && key.getKeyCode() == juce::KeyPress::spaceKey)
    {
        playSelectionOnly();
        return true;
    }
    if (noMods && !shift)
    {
        const int kc = key.getKeyCode();
        if (kc == 'Z' || kc == 'z') { snapZeroCrossingButton.triggerClick(); return true; }
        if (kc == 'N' || kc == 'n') { normalizeAudioPeak(); return true; }
        if (kc == 'R' || kc == 'r') { reverseSelectedRegion(); return true; }
        if (kc == 'C' || kc == 'c') { cropToSelection(); return true; }
        if (kc == '+' || kc == '=') { zoomInButton.triggerClick(); return true; }
        if (kc == '-' || kc == '_') { zoomOutButton.triggerClick(); return true; }
    }

    if (sm.matches("edit.select_all", key))
    {
        selectAllRegion();
        return true;
    }
    if (sm.matches("edit.deselect_all", key) || sm.matches("edit.escape", key))
    {
        deselectAllRegion();
        return true;
    }
    if (sm.matches("transport.play_pause", key))
    {
        playPauseButton.triggerClick();
        return true;
    }
    if (sm.matches("edit.silence_selection", key))
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
    if (sm.matches("edit.set_start_cursor", key))
    {
        double currentR = currentPositionSecs / totalDurationSecs;
        double endR = audioEngine.getSampleEndRatio();
        if (snapToZeroCrossing) currentR = findNearestZeroCrossing(currentR);
        audioEngine.setSampleRange(currentR, std::max(currentR + 0.001, endR));
        repaint();
        return true;
    }
    if (sm.matches("edit.set_end_cursor", key))
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
//  Spectral Cursors & Selection Shapes
// ─────────────────────────────────────────────────────────
juce::MouseCursor EditComponent::createSpectralCursor(SpectralSelectionShape shape)
{
    const int w = 24;
    const int h = 24;
    juce::Image img(juce::Image::ARGB, w, h, true);
    juce::Graphics g(img);

    // Crosshair center at (7, 7)
    const float cx = 7.0f;
    const float cy = 7.0f;

    // Outer shadow / outline for high contrast on both dark & bright spectral areas
    g.setColour(juce::Colours::black.withAlpha(0.9f));
    g.drawLine(cx, 1.0f, cx, 13.0f, 3.0f);
    g.drawLine(1.0f, cy, 13.0f, cy, 3.0f);

    // Precise 1px white crosshair
    g.setColour(juce::Colours::white);
    g.drawLine(cx, 1.0f, cx, 13.0f, 1.0f);
    g.drawLine(1.0f, cy, 13.0f, cy, 1.0f);

    // Center pinpoint pixel in cyan
    g.setColour(OpenWavLookAndFeel::accentCyan);
    g.fillRect(cx - 0.5f, cy - 0.5f, 1.0f, 1.0f);

    // Shape glyph container at bottom-right (x: 12..23, y: 12..23)
    g.setColour(juce::Colours::black.withAlpha(0.75f));
    g.fillRoundedRectangle(12.0f, 12.0f, 11.0f, 11.0f, 2.0f);
    g.setColour(juce::Colours::black);
    g.drawRoundedRectangle(12.0f, 12.0f, 11.0f, 11.0f, 2.0f, 1.0f);

    // Draw mini shape glyph in bright cyan
    g.setColour(OpenWavLookAndFeel::accentCyan);
    switch (shape)
    {
        case SpectralSelectionShape::Box:
            g.drawRect(14.0f, 14.0f, 7.0f, 7.0f, 1.2f);
            break;
        case SpectralSelectionShape::Time:
            g.drawLine(15.5f, 14.0f, 15.5f, 21.0f, 1.2f);
            g.drawLine(19.5f, 14.0f, 19.5f, 21.0f, 1.2f);
            break;
        case SpectralSelectionShape::Frequency:
            g.drawLine(14.0f, 15.5f, 21.0f, 15.5f, 1.2f);
            g.drawLine(14.0f, 19.5f, 21.0f, 19.5f, 1.2f);
            break;
        case SpectralSelectionShape::Lasso:
        {
            juce::Path p;
            p.startNewSubPath(15.0f, 15.0f);
            p.lineTo(20.0f, 15.0f);
            p.lineTo(20.5f, 18.5f);
            p.lineTo(17.5f, 21.0f);
            p.lineTo(14.5f, 18.5f);
            p.closeSubPath();
            g.strokePath(p, juce::PathStrokeType(1.1f));
            break;
        }
        case SpectralSelectionShape::Ellipse:
            g.drawEllipse(14.0f, 14.0f, 7.0f, 7.0f, 1.2f);
            break;
    }

    return juce::MouseCursor(img, static_cast<int>(cx), static_cast<int>(cy));
}

void EditComponent::initSpectralCursors()
{
    spectralBoxCursor = createSpectralCursor(SpectralSelectionShape::Box);
    spectralTimeCursor = createSpectralCursor(SpectralSelectionShape::Time);
    spectralFreqCursor = createSpectralCursor(SpectralSelectionShape::Frequency);
    spectralLassoCursor = createSpectralCursor(SpectralSelectionShape::Lasso);
    spectralEllipseCursor = createSpectralCursor(SpectralSelectionShape::Ellipse);
    spectralCursorsInitialized = true;
}

juce::MouseCursor EditComponent::getSpectralCursor(SpectralSelectionShape shape)
{
    if (!spectralCursorsInitialized)
        initSpectralCursors();

    switch (shape)
    {
        case SpectralSelectionShape::Box:       return spectralBoxCursor;
        case SpectralSelectionShape::Time:      return spectralTimeCursor;
        case SpectralSelectionShape::Frequency: return spectralFreqCursor;
        case SpectralSelectionShape::Lasso:     return spectralLassoCursor;
        case SpectralSelectionShape::Ellipse:   return spectralEllipseCursor;
    }
    return spectralBoxCursor;
}

void EditComponent::updateShapeButtonStates()
{
    spectralBoxSelectBtn.setToggleState(currentSpectralShape == SpectralSelectionShape::Box, juce::dontSendNotification);
    spectralTimeSelectBtn.setToggleState(currentSpectralShape == SpectralSelectionShape::Time, juce::dontSendNotification);
    spectralFreqSelectBtn.setToggleState(currentSpectralShape == SpectralSelectionShape::Frequency, juce::dontSendNotification);
    spectralLassoSelectBtn.setToggleState(currentSpectralShape == SpectralSelectionShape::Lasso, juce::dontSendNotification);
    spectralEllipseSelectBtn.setToggleState(currentSpectralShape == SpectralSelectionShape::Ellipse, juce::dontSendNotification);
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

    if (isSpectralView)
    {
        if (hasSpectralBoxSelection && currentSpectralShape != SpectralSelectionShape::Lasso)
        {
            auto boxRect = getSpectralSelectionRect(wfBounds);
            const float edgeTol = 8.0f;
            const bool canH = (currentSpectralShape != SpectralSelectionShape::Frequency);
            const bool canV = (currentSpectralShape != SpectralSelectionShape::Time);
            const bool nearL = canH && std::abs(e.position.x - boxRect.getX()) <= edgeTol && e.position.y >= boxRect.getY() - edgeTol && e.position.y <= boxRect.getBottom() + edgeTol;
            const bool nearR = canH && std::abs(e.position.x - boxRect.getRight()) <= edgeTol && e.position.y >= boxRect.getY() - edgeTol && e.position.y <= boxRect.getBottom() + edgeTol;
            const bool nearT = canV && std::abs(e.position.y - boxRect.getY()) <= edgeTol && e.position.x >= boxRect.getX() - edgeTol && e.position.x <= boxRect.getRight() + edgeTol;
            const bool nearB = canV && std::abs(e.position.y - boxRect.getBottom()) <= edgeTol && e.position.x >= boxRect.getX() - edgeTol && e.position.x <= boxRect.getRight() + edgeTol;

            if (nearL || nearR)
                setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
            else if (nearT || nearB)
                setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
            else if (boxRect.contains(e.position))
                setMouseCursor(juce::MouseCursor::DraggingHandCursor);
            else
                setMouseCursor(getSpectralCursor(currentSpectralShape));
        }
        else
        {
            setMouseCursor(getSpectralCursor(currentSpectralShape));
        }
        return;
    }

    const float threshold = 10.0f;
    double startR = audioEngine.getSampleStartRatio();
    double endR = audioEngine.getSampleEndRatio();

    float startX = ratioToX(startR, wfBounds);
    float endX = ratioToX(endR, wfBounds);

    float fadeY = wfBounds.getY() + 20.0f;
    double fadeInRatio = (fadeInMs / 1000.0) / totalDurationSecs;
    double fadeOutRatio = (fadeOutMs / 1000.0) / totalDurationSecs;

    float fadeInX = ratioToX(startR + fadeInRatio, wfBounds);
    float fadeOutX = ratioToX(endR - fadeOutRatio, wfBounds);

    DragTarget newHover = DragTarget::None;

    auto dragPill = getSelectionDragPillBounds(wfBounds);
    if (!dragPill.isEmpty() && dragPill.expanded(4.0f, 4.0f).contains(e.position))
        newHover = DragTarget::DragSelectionOut;
    else if (e.mods.isAltDown() && e.position.x >= startX + threshold && e.position.x <= endX - threshold)
        newHover = DragTarget::DragSelectionOut;
    else if (std::abs(e.position.x - fadeInX) <= threshold && std::abs(e.position.y - fadeY) <= 12.0f)
        newHover = DragTarget::FadeInHandle;
    else if (std::abs(e.position.x - fadeOutX) <= threshold && std::abs(e.position.y - fadeY) <= 12.0f)
        newHover = DragTarget::FadeOutHandle;
    else if (loopMarkersSet)
    {
        float loopInX = ratioToX(loopInRatio, wfBounds);
        float loopOutX = ratioToX(loopOutRatio, wfBounds);
        double maxXfadeRatio = std::max(0.0, (loopOutRatio - loopInRatio) * 0.5);
        double xfadeRatio = (crossfadeMs > 0.0) ? std::min((crossfadeMs / 1000.0) / totalDurationSecs, maxXfadeRatio) : 0.0;
        float xfHandleX = ratioToX(loopOutRatio - xfadeRatio, wfBounds);
        float xfHandleY = wfBounds.getBottom() - 22.0f;

        if (std::abs(e.position.x - xfHandleX) <= threshold && std::abs(e.position.y - xfHandleY) <= 10.0f)
            newHover = DragTarget::CrossfadeHandle;
        else if (std::abs(e.position.x - loopInX) <= threshold && e.position.y >= wfBounds.getBottom() - 24.0f)
            newHover = DragTarget::LoopInMarker;
        else if (std::abs(e.position.x - loopOutX) <= threshold && e.position.y >= wfBounds.getBottom() - 24.0f)
            newHover = DragTarget::LoopOutMarker;
        else if (e.position.y >= wfBounds.getBottom() - 14.0f && e.position.x > loopInX + threshold && e.position.x < loopOutX - threshold)
            newHover = DragTarget::LoopRegion;
    }

    if (newHover == DragTarget::None)
    {
        if (std::abs(e.position.x - startX) <= threshold)
            newHover = DragTarget::StartMarker;
        else if (std::abs(e.position.x - endX) <= threshold)
            newHover = DragTarget::EndMarker;
        else if (loopMarkersSet && std::abs(e.position.x - ratioToX(loopInRatio, wfBounds)) <= threshold)
            newHover = DragTarget::LoopInMarker;
        else if (loopMarkersSet && std::abs(e.position.x - ratioToX(loopOutRatio, wfBounds)) <= threshold)
            newHover = DragTarget::LoopOutMarker;
    }

    if (newHover != hoverTarget)
    {
        hoverTarget = newHover;
        repaint();
    }

    if (newHover == DragTarget::LoopRegion || newHover == DragTarget::DragSelectionOut)
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    else if (newHover != DragTarget::None)
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    else
        setMouseCursor(juce::MouseCursor::IBeamCursor);
}

void EditComponent::mouseExit(const juce::MouseEvent& /*e*/)
{
    if (hoverTarget != DragTarget::None)
    {
        hoverTarget = DragTarget::None;
        repaint();
    }
    setMouseCursor(juce::MouseCursor::NormalCursor);
}

// ─────────────────────────────────────────────────────────
//  Mouse Interaction
// ─────────────────────────────────────────────────────────
void EditComponent::mouseDown(const juce::MouseEvent& e)
{
    if (totalDurationSecs <= 0.0 || !audioEngine.isCurrentSampleInSampler()) return;

    auto outerWf = getWaveformBounds();
    if (!outerWf.contains(e.position))
        return;

    auto wfBounds = outerWf.reduced(6.0f, 6.0f);
    juce::Point<float> pos = wfBounds.getConstrainedPoint(e.position);

    double clickRatio = juce::jlimit(0.0, 1.0, xToRatio(pos.x, wfBounds));
    const float threshold = 12.0f;

    if (isSpectralView)
    {
        if (hasSpectralBoxSelection && currentSpectralShape != SpectralSelectionShape::Lasso)
        {
            auto boxRect = getSpectralSelectionRect(wfBounds);
            const float edgeTol = 8.0f;
            const bool canH = (currentSpectralShape != SpectralSelectionShape::Frequency);
            const bool canV = (currentSpectralShape != SpectralSelectionShape::Time);
            const bool nearL = canH && std::abs(pos.x - boxRect.getX()) <= edgeTol && pos.y >= boxRect.getY() - edgeTol && pos.y <= boxRect.getBottom() + edgeTol;
            const bool nearR = canH && std::abs(pos.x - boxRect.getRight()) <= edgeTol && pos.y >= boxRect.getY() - edgeTol && pos.y <= boxRect.getBottom() + edgeTol;
            const bool nearT = canV && std::abs(pos.y - boxRect.getY()) <= edgeTol && pos.x >= boxRect.getX() - edgeTol && pos.x <= boxRect.getRight() + edgeTol;
            const bool nearB = canV && std::abs(pos.y - boxRect.getBottom()) <= edgeTol && pos.x >= boxRect.getX() - edgeTol && pos.x <= boxRect.getRight() + edgeTol;

            if (nearL)      { dragTarget = DragTarget::SpectralResizeLeft; return; }
            if (nearR)      { dragTarget = DragTarget::SpectralResizeRight; return; }
            if (nearT)      { dragTarget = DragTarget::SpectralResizeTop; return; }
            if (nearB)      { dragTarget = DragTarget::SpectralResizeBottom; return; }
            if (boxRect.contains(pos))
            {
                dragTarget = DragTarget::SpectralMove;
                spectralDragStartPos = pos;
                spectralMoveStartT0 = spectralTimeStart;
                spectralMoveStartT1 = spectralTimeEnd;
                const double minF = 20.0, maxF = 20000.0;
                spectralMoveStartNormY0 = static_cast<float>(juce::jlimit(0.0, 1.0, std::log(std::max(minF, static_cast<double>(spectralFreqLow)) / minF) / std::log(maxF / minF)));
                spectralMoveStartNormY1 = static_cast<float>(juce::jlimit(0.0, 1.0, std::log(std::max(minF, static_cast<double>(spectralFreqHigh)) / minF) / std::log(maxF / minF)));
                return;
            }
        }

        dragTarget = DragTarget::SelectingSpectralBox;
        spectralDragStartPos = pos;
        lastLassoPixelPos = pos;
        spectralTimeStart = clickRatio;
        spectralTimeEnd = clickRatio;

        float normY = juce::jlimit(0.0f, 1.0f, (wfBounds.getBottom() - pos.y) / wfBounds.getHeight());
        spectralFreqLow = static_cast<float>(20.0 * std::pow(20000.0 / 20.0, normY));
        spectralFreqHigh = spectralFreqLow;

        lassoScreenPoints.clear();
        if (currentSpectralShape == SpectralSelectionShape::Lasso)
        {
            lassoScreenPoints.push_back({ static_cast<float>(clickRatio), normY });
        }
        else if (currentSpectralShape == SpectralSelectionShape::Time)
        {
            spectralFreqLow = 20.0f;
            spectralFreqHigh = 20000.0f;
        }
        else if (currentSpectralShape == SpectralSelectionShape::Frequency)
        {
            spectralTimeStart = 0.0;
            spectralTimeEnd = 1.0;
        }

        hasSpectralBoxSelection = true;
        updateControlVisibility();
        repaint();
        return;
    }

    double startR = audioEngine.getSampleStartRatio();
    double endR = audioEngine.getSampleEndRatio();

    float startX = ratioToX(startR, wfBounds);
    float endX = ratioToX(endR, wfBounds);

    // Check selection drag-out pill or Alt+Click inside selection
    auto dragPill = getSelectionDragPillBounds(wfBounds);
    if ((!dragPill.isEmpty() && dragPill.expanded(4.0f, 4.0f).contains(e.position))
        || (e.mods.isAltDown() && e.position.x >= startX + threshold && e.position.x <= endX - threshold))
    {
        dragTarget = DragTarget::DragSelectionOut;
        exportSelectionAndStartDrag();
        return;
    }

    // Check fade handles FIRST (so they are never shadowed by the top ruler scrub zone)
    float fadeY = wfBounds.getY() + 20.0f;
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

    // Check if clicking in time ruler area at top of waveform
    if (e.position.y <= wfBounds.getY() + 14.0f && std::abs(e.position.x - startX) > threshold && std::abs(e.position.x - endX) > threshold)
    {
        dragTarget = DragTarget::ScrubbingPlayhead;
        audioEngine.setPositionRatio(clickRatio);
        currentPositionSecs = clickRatio * totalDurationSecs;
        repaint();
        return;
    }

    // Check loop bottom handles, crossfade diamond, and draggable loop bar
    if (loopMarkersSet)
    {
        float loopInX = ratioToX(loopInRatio, wfBounds);
        float loopOutX = ratioToX(loopOutRatio, wfBounds);
        double maxXfadeRatio = std::max(0.0, (loopOutRatio - loopInRatio) * 0.5);
        double xfadeRatio = (crossfadeMs > 0.0) ? std::min((crossfadeMs / 1000.0) / totalDurationSecs, maxXfadeRatio) : 0.0;
        float xfHandleX = ratioToX(loopOutRatio - xfadeRatio, wfBounds);
        float xfHandleY = wfBounds.getBottom() - 22.0f;

        if (std::abs(e.position.x - xfHandleX) <= threshold && std::abs(e.position.y - xfHandleY) <= 10.0f)
        {
            dragTarget = DragTarget::CrossfadeHandle;
            return;
        }
        if (std::abs(e.position.x - loopInX) <= threshold && e.position.y >= wfBounds.getBottom() - 24.0f)
        {
            dragTarget = DragTarget::LoopInMarker;
            return;
        }
        if (std::abs(e.position.x - loopOutX) <= threshold && e.position.y >= wfBounds.getBottom() - 24.0f)
        {
            dragTarget = DragTarget::LoopOutMarker;
            return;
        }
        if (e.position.y >= wfBounds.getBottom() - 14.0f && e.position.x > loopInX + threshold && e.position.x < loopOutX - threshold)
        {
            dragTarget = DragTarget::LoopRegion;
            dragLoopSpan = loopOutRatio - loopInRatio;
            dragLoopGrabOffset = clickRatio - loopInRatio;
            return;
        }
    }

    // Start/End markers (prioritized in upper/mid waveform even if aligned with Loop In/Out)
    if (std::abs(e.position.x - startX) <= threshold)
    {
        dragTarget = DragTarget::StartMarker;
        return;
    }
    if (std::abs(e.position.x - endX) <= threshold)
    {
        dragTarget = DragTarget::EndMarker;
        return;
    }

    // Loop vertical lines in mid-waveform
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
    dragTarget = DragTarget::SelectingRange;
    if (snapToZeroCrossing) clickRatio = findNearestZeroCrossing(clickRatio);
    dragStartRatio = clickRatio;
    repaint();
}

void EditComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (totalDurationSecs <= 0.0 || dragTarget == DragTarget::None)
        return;

    auto wfBounds = getWaveformBounds().reduced(6.0f, 6.0f);
    juce::Point<float> pos = wfBounds.getConstrainedPoint(e.position);
    double currentRatio = juce::jlimit(0.0, 1.0, xToRatio(pos.x, wfBounds));
    float currentNormY = juce::jlimit(0.0f, 1.0f, (wfBounds.getBottom() - pos.y) / wfBounds.getHeight());
    float currentFreq = static_cast<float>(20.0 * std::pow(20000.0 / 20.0, currentNormY));

    if (dragTarget == DragTarget::SpectralResizeLeft)
    {
        spectralTimeStart = juce::jlimit(0.0, std::max(0.0, spectralTimeEnd - 0.001), currentRatio);
        repaint();
        return;
    }
    if (dragTarget == DragTarget::SpectralResizeRight)
    {
        spectralTimeEnd = juce::jlimit(std::min(1.0, spectralTimeStart + 0.001), 1.0, currentRatio);
        repaint();
        return;
    }
    if (dragTarget == DragTarget::SpectralResizeBottom)
    {
        spectralFreqLow = juce::jlimit(20.0f, std::max(20.0f, spectralFreqHigh - 5.0f), currentFreq);
        repaint();
        return;
    }
    if (dragTarget == DragTarget::SpectralResizeTop)
    {
        spectralFreqHigh = juce::jlimit(std::min(20000.0f, spectralFreqLow + 5.0f), 20000.0f, currentFreq);
        repaint();
        return;
    }
    if (dragTarget == DragTarget::SpectralMove)
    {
        if (currentSpectralShape != SpectralSelectionShape::Frequency)
        {
            double dt = currentRatio - xToRatio(spectralDragStartPos.x, wfBounds);
            double spanT = spectralMoveStartT1 - spectralMoveStartT0;
            double newT0 = juce::jlimit(0.0, std::max(0.0, 1.0 - spanT), spectralMoveStartT0 + dt);
            spectralTimeStart = newT0;
            spectralTimeEnd = newT0 + spanT;
        }
        if (currentSpectralShape != SpectralSelectionShape::Time)
        {
            float startNormY = juce::jlimit(0.0f, 1.0f, (wfBounds.getBottom() - spectralDragStartPos.y) / wfBounds.getHeight());
            float dyNorm = currentNormY - startNormY;
            float spanY = spectralMoveStartNormY1 - spectralMoveStartNormY0;
            float newY0 = juce::jlimit(0.0f, std::max(0.0f, 1.0f - spanY), spectralMoveStartNormY0 + dyNorm);
            float newY1 = newY0 + spanY;
            spectralFreqLow = static_cast<float>(20.0 * std::pow(20000.0 / 20.0, newY0));
            spectralFreqHigh = static_cast<float>(20.0 * std::pow(20000.0 / 20.0, newY1));
        }
        repaint();
        return;
    }

    if (dragTarget == DragTarget::SelectingSpectralBox)
    {
        double startR = juce::jlimit(0.0, 1.0, xToRatio(spectralDragStartPos.x, wfBounds));
        float normY1 = juce::jlimit(0.0f, 1.0f, (wfBounds.getBottom() - spectralDragStartPos.y) / wfBounds.getHeight());

        float f1 = static_cast<float>(20.0 * std::pow(20000.0 / 20.0, normY1));
        float f2 = currentFreq;

        if (currentSpectralShape == SpectralSelectionShape::Time)
        {
            spectralTimeStart = std::min(startR, currentRatio);
            spectralTimeEnd = std::max(startR, currentRatio);
            spectralFreqLow = 20.0f;
            spectralFreqHigh = 20000.0f;
        }
        else if (currentSpectralShape == SpectralSelectionShape::Frequency)
        {
            spectralTimeStart = 0.0;
            spectralTimeEnd = 1.0;
            spectralFreqLow = std::min(f1, f2);
            spectralFreqHigh = std::max(f1, f2);
        }
        else if (currentSpectralShape == SpectralSelectionShape::Lasso)
        {
            juce::Point<float> normPt(static_cast<float>(currentRatio), currentNormY);
            if (lassoScreenPoints.empty() || pos.getDistanceFrom(lastLassoPixelPos) >= 3.0f)
            {
                lassoScreenPoints.push_back(normPt);
                lastLassoPixelPos = pos;
            }

            float minR = lassoScreenPoints.front().x;
            float maxR = lassoScreenPoints.front().x;
            float minNormY = lassoScreenPoints.front().y;
            float maxNormY = lassoScreenPoints.front().y;
            for (const auto& pt : lassoScreenPoints)
            {
                minR = std::min(minR, pt.x);
                maxR = std::max(maxR, pt.x);
                minNormY = std::min(minNormY, pt.y);
                maxNormY = std::max(maxNormY, pt.y);
            }

            spectralTimeStart = juce::jlimit(0.0, 1.0, static_cast<double>(minR));
            spectralTimeEnd = juce::jlimit(0.0, 1.0, static_cast<double>(maxR));
            spectralFreqLow = static_cast<float>(20.0 * std::pow(20000.0 / 20.0, minNormY));
            spectralFreqHigh = static_cast<float>(20.0 * std::pow(20000.0 / 20.0, maxNormY));
        }
        else // Box and Ellipse
        {
            spectralTimeStart = std::min(startR, currentRatio);
            spectralTimeEnd = std::max(startR, currentRatio);
            spectralFreqLow = std::min(f1, f2);
            spectralFreqHigh = std::max(f1, f2);
        }

        hasSpectralBoxSelection = true;
        updateControlVisibility();
        repaint();
        return;
    }

    double startR = audioEngine.getSampleStartRatio();
    double endR = audioEngine.getSampleEndRatio();

    if (snapToZeroCrossing && dragTarget != DragTarget::FadeInHandle && dragTarget != DragTarget::FadeOutHandle
        && dragTarget != DragTarget::CrossfadeHandle && dragTarget != DragTarget::LoopRegion)
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
            loopMarkersSet = true;
            audioEngine.setLoopPoints(loopInRatio, loopOutRatio);
            audioEngine.setLoopMarkersSet(true);
            loopInTimeLabel.setText(formatTime(loopInRatio * totalDurationSecs), juce::dontSendNotification);
            break;
        case DragTarget::LoopOutMarker:
            loopOutRatio = juce::jlimit(loopInRatio + 0.001, 1.0, currentRatio);
            loopMarkersSet = true;
            audioEngine.setLoopPoints(loopInRatio, loopOutRatio);
            audioEngine.setLoopMarkersSet(true);
            loopOutTimeLabel.setText(formatTime(loopOutRatio * totalDurationSecs), juce::dontSendNotification);
            break;
        case DragTarget::LoopRegion:
        {
            double newIn = juce::jlimit(0.0, 1.0 - dragLoopSpan, currentRatio - dragLoopGrabOffset);
            loopInRatio = newIn;
            loopOutRatio = newIn + dragLoopSpan;
            loopMarkersSet = true;
            audioEngine.setLoopPoints(loopInRatio, loopOutRatio);
            audioEngine.setLoopMarkersSet(true);
            loopInTimeLabel.setText(formatTime(loopInRatio * totalDurationSecs), juce::dontSendNotification);
            loopOutTimeLabel.setText(formatTime(loopOutRatio * totalDurationSecs), juce::dontSendNotification);
            break;
        }
        case DragTarget::CrossfadeHandle:
        {
            double maxXfadeSecs = std::max(0.0, (loopOutRatio - loopInRatio) * totalDurationSecs * 0.5);
            double newXfadeSecs = juce::jlimit(0.0, maxXfadeSecs, (loopOutRatio - currentRatio) * totalDurationSecs);
            crossfadeSlider.setValue(newXfadeSecs * 1000.0);
            break;
        }
        case DragTarget::FadeInHandle:
        {
            double maxFadeInMs = std::max(0.0, (endR - startR) * totalDurationSecs * 1000.0 - fadeOutMs);
            double newFadeInMs = juce::jlimit(0.0, maxFadeInMs, (currentRatio - startR) * totalDurationSecs * 1000.0);
            fadeInSlider.setValue(newFadeInMs);
            break;
        }
        case DragTarget::FadeOutHandle:
        {
            double maxFadeOutMs = std::max(0.0, (endR - startR) * totalDurationSecs * 1000.0 - fadeInMs);
            double newFadeOutMs = juce::jlimit(0.0, maxFadeOutMs, (endR - currentRatio) * totalDurationSecs * 1000.0);
            fadeOutSlider.setValue(newFadeOutMs);
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

void EditComponent::mouseUp(const juce::MouseEvent& e)
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
                audioEngine.setLoopPoints(loopInRatio, loopOutRatio);
                audioEngine.setLoopMarkersSet(true);
            }
        }
    }
    else if (dragTarget == DragTarget::StartMarker)
    {
        audioEngine.setPositionRatio(audioEngine.getSampleStartRatio());
    }
    else if (dragTarget == DragTarget::FadeInHandle || dragTarget == DragTarget::FadeOutHandle)
    {
        // Non-destructive: do not bake on release, fades remain active and adjustable!
    }
    else if (dragTarget == DragTarget::SpectralMove || dragTarget == DragTarget::SpectralResizeLeft
             || dragTarget == DragTarget::SpectralResizeRight || dragTarget == DragTarget::SpectralResizeTop
             || dragTarget == DragTarget::SpectralResizeBottom)
    {
        hasSpectralBoxSelection = true;
        updateControlVisibility();
    }
    else if (dragTarget == DragTarget::SelectingSpectralBox)
    {
        auto wfBounds = getWaveformBounds().reduced(6.0f, 6.0f);
        const auto pos = wfBounds.getConstrainedPoint(e.position);
        const float dx = std::abs(pos.x - spectralDragStartPos.x);
        const float dy = std::abs(pos.y - spectralDragStartPos.y);

        if (currentSpectralShape == SpectralSelectionShape::Lasso)
        {
            hasSpectralBoxSelection = (lassoScreenPoints.size() >= 3 && (dx >= 4.0f || dy >= 4.0f));
            if (!hasSpectralBoxSelection)
                lassoScreenPoints.clear();
        }
        else if (currentSpectralShape == SpectralSelectionShape::Frequency)
        {
            hasSpectralBoxSelection = (dy >= 4.0f);
        }
        else if (currentSpectralShape == SpectralSelectionShape::Time)
        {
            hasSpectralBoxSelection = (dx >= 4.0f);
        }
        else // Box and Ellipse
        {
            hasSpectralBoxSelection = (dx >= 4.0f || dy >= 4.0f);
        }

        if (!hasSpectralBoxSelection)
        {
            const double clickRatio = juce::jlimit(0.0, 1.0, xToRatio(pos.x, wfBounds));
            audioEngine.setPositionRatio(clickRatio);
            currentPositionSecs = clickRatio * totalDurationSecs;
        }
        updateControlVisibility();
    }

    if (dragTarget == DragTarget::SelectingRange || dragTarget == DragTarget::StartMarker || dragTarget == DragTarget::EndMarker)
        updateSelectionStats();

    dragTarget = DragTarget::None;
    repaint();
}

void EditComponent::exportSelectionAndStartDrag()
{
    juce::AudioBuffer<float> buf;
    double sr = 44100.0;
    if (!audioEngine.getAudioBufferCopy(buf, sr) || buf.getNumSamples() <= 0 || sr <= 0.0)
        return;

    int totalSamples = buf.getNumSamples();
    int startIdx = juce::jlimit(0, totalSamples - 1, static_cast<int>(audioEngine.getSampleStartRatio() * totalSamples));
    int endIdx = juce::jlimit(startIdx + 1, totalSamples, static_cast<int>(audioEngine.getSampleEndRatio() * totalSamples));
    int len = endIdx - startIdx;
    if (len <= 0)
        return;

    juce::AudioBuffer<float> selBuf(buf.getNumChannels(), len);
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        selBuf.copyFrom(ch, 0, buf, ch, startIdx, len);

    // Bake current fades into dragged selection so it sounds identical in Sampler / DAW
    int fadeInSamples = std::min(len, static_cast<int>((fadeInMs / 1000.0) * sr));
    int fadeOutSamples = std::min(len, static_cast<int>((fadeOutMs / 1000.0) * sr));
    if (fadeInSamples > 0)
    {
        for (int ch = 0; ch < selBuf.getNumChannels(); ++ch)
        {
            float* data = selBuf.getWritePointer(ch);
            for (int i = 0; i < fadeInSamples; ++i)
                data[i] *= evaluateFadeCurve(static_cast<float>(i) / static_cast<float>(fadeInSamples), fadeInCurveType);
        }
    }
    if (fadeOutSamples > 0)
    {
        for (int ch = 0; ch < selBuf.getNumChannels(); ++ch)
        {
            float* data = selBuf.getWritePointer(ch, len - fadeOutSamples);
            for (int i = 0; i < fadeOutSamples; ++i)
            {
                float t = static_cast<float>(i) / static_cast<float>(fadeOutSamples);
                data[i] *= evaluateFadeCurve(1.0f - t, fadeOutCurveType);
            }
        }
    }

    auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("OpenWavSelections");
    tempDir.createDirectory();

    juce::String baseName = audioEngine.getCurrentFile().getFileNameWithoutExtension();
    if (baseName.isEmpty()) baseName = "Selection";
    juce::File outFile = tempDir.getChildFile(baseName + "_sel.wav");
    if (outFile.existsAsFile())
        outFile.deleteFile();

    if (auto outStream = outFile.createOutputStream())
    {
        juce::WavAudioFormat wavFormat;
        std::unique_ptr<juce::AudioFormatWriter> writer(
            wavFormat.createWriterFor(outStream.get(), sr, static_cast<unsigned int>(selBuf.getNumChannels()), 24, {}, 0));
        if (writer != nullptr)
        {
            outStream.release();
            writer->writeFromAudioSampleBuffer(selBuf, 0, len);
            writer.reset();

            juce::StringArray filesToDrag;
            filesToDrag.add(outFile.getFullPathName());
            juce::MessageManager::callAsync([filesToDrag] {
                juce::DragAndDropContainer::performExternalDragDropOfFiles(filesToDrag, false);
            });
        }
    }
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
    audioEngine.setLoopPoints(0.0, 1.0);
    audioEngine.setLoopMarkersSet(false);
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
        lassoScreenPoints.clear();
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
        lastLoadedFilePath.clear();
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

    const juce::String fullPath = f.getFullPathName();
    bool isSameFile = (fullPath == lastLoadedFilePath);
    lastLoadedFilePath = fullPath;

    isSliceEditingActive = false;
    activeEditingSliceIndex = -1;

    sampleNameLabel.setText(f.getFileName(), juce::dontSendNotification);
    totalDurationSecs = audioEngine.getTotalLengthSeconds();

    if (!isSameFile || !audioEngine.hasOriginalSnapshot())
    {
        currentPositionSecs = 0.0;
        SampleEditSettings saved;
        if (audioEngine.getSampleEditSettings(f.getFullPathName(), saved))
        {
            loopInRatio = saved.loopInRatio;
            loopOutRatio = saved.loopOutRatio;
            loopMarkersSet = saved.loopMarkersSet;
            fadeInMs = saved.fadeInMs;
            fadeOutMs = saved.fadeOutMs;
            fadeInCurveType = juce::jlimit(0, 2, saved.fadeInCurveType);
            fadeOutCurveType = juce::jlimit(0, 2, saved.fadeOutCurveType);
            crossfadeMs = saved.loopCrossfadeMs;

            fadeInSlider.setValue(fadeInMs, juce::dontSendNotification);
            fadeOutSlider.setValue(fadeOutMs, juce::dontSendNotification);
            crossfadeSlider.setValue(crossfadeMs, juce::dontSendNotification);
            fadeInCurveBox.setSelectedId(fadeInCurveType + 1, juce::dontSendNotification);
            fadeOutCurveBox.setSelectedId(fadeOutCurveType + 1, juce::dontSendNotification);
            fadeInMsLabel.setText(juce::String(static_cast<int>(fadeInMs)) + " ms", juce::dontSendNotification);
            fadeOutMsLabel.setText(juce::String(static_cast<int>(fadeOutMs)) + " ms", juce::dontSendNotification);
            crossfadeMsLabel.setText(juce::String(static_cast<int>(crossfadeMs)) + " ms", juce::dontSendNotification);
            loopToggleButton.setToggleState(saved.isLooping, juce::dontSendNotification);

            audioEngine.setSampleRange(saved.sampleStartRatio, saved.sampleEndRatio);
            audioEngine.setLoopPoints(loopInRatio, loopOutRatio);
            audioEngine.setLoopMarkersSet(loopMarkersSet);
            audioEngine.setLoopCrossfadeMs(crossfadeMs);
            audioEngine.setFades(fadeInMs, fadeInCurveType, fadeOutMs, fadeOutCurveType);
            audioEngine.setLooping(saved.isLooping);
        }
        else
        {
            loopInRatio = 0.0;
            loopOutRatio = 1.0;
            loopMarkersSet = false;
            fadeInMs = 0.0;
            fadeOutMs = 0.0;
            crossfadeMs = 0.0;
            fadeInSlider.setValue(0.0, juce::dontSendNotification);
            fadeOutSlider.setValue(0.0, juce::dontSendNotification);
            crossfadeSlider.setValue(0.0, juce::dontSendNotification);
            fadeInMsLabel.setText("0 ms", juce::dontSendNotification);
            fadeOutMsLabel.setText("0 ms", juce::dontSendNotification);
            crossfadeMsLabel.setText("0 ms", juce::dontSendNotification);
            loopToggleButton.setToggleState(audioEngine.isLooping(), juce::dontSendNotification);
            audioEngine.setSampleRange(0.0, 1.0);
            audioEngine.setLoopPoints(0.0, 1.0);
            audioEngine.setLoopMarkersSet(false);
            audioEngine.setLoopCrossfadeMs(0.0);
            audioEngine.setFades(0.0, fadeInCurveType, 0.0, fadeOutCurveType);
        }
        zoomLevel = 1.0;
        scrollOffset = 0.0;
        zoomSlider.setValue(1.0, juce::dontSendNotification);

        // Snapshot the original buffer for non-destructive editing
        audioEngine.snapshotOriginalForEditing();
        undoStack.clear();
        redoStack.clear();
    }

    hasSpectralBoxSelection = false;
    lassoScreenPoints.clear();
    spectrogramGenerated = false;
    if (isSpectralView)
    {
        generateSpectrogram();
    }
    updateUndoRedoButtons();
    updateSelectionStats();
    updateControlVisibility();
    repaint();
}

void EditComponent::timerCallback()
{
    if (hasAudioToEdit())
    {
        bool isPlaying = audioEngine.isPlaying();
        juce::String expectedPlayBtnText = isPlaying ? "Pause" : "Play";
        if (playPauseButton.getButtonText() != expectedPlayBtnText)
        {
            playPauseButton.setButtonText(expectedPlayBtnText);
            repaint();
        }

        bool isLooping = audioEngine.isLooping();
        if (loopToggleButton.getToggleState() != isLooping)
        {
            loopToggleButton.setToggleState(isLooping, juce::dontSendNotification);
            repaint();
        }

        double newPos = audioEngine.getCurrentPositionSeconds();
        double newDur = audioEngine.getTotalLengthSeconds();
        bool needsRepaint = isPlaying || std::abs(newPos - currentPositionSecs) > 0.0001;

        currentPositionSecs = newPos;
        totalDurationSecs = newDur;

        if (needsRepaint)
            repaint();
    }
}

void EditComponent::loopingStateChanged(bool enabled)
{
    loopToggleButton.setToggleState(enabled, juce::dontSendNotification);
    repaint();
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
        s.spectralSelectionShape = static_cast<int>(currentSpectralShape);
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

            audioEngine.setLoopPoints(loopInRatio, loopOutRatio);
            audioEngine.setLoopMarkersSet(loopMarkersSet);
            audioEngine.setLoopCrossfadeMs(crossfadeMs);
            audioEngine.setFades(fadeInMs, fadeInCurveType, fadeOutMs, fadeOutCurveType);

            zoomLevel = juce::jlimit(1.0, 64.0, state.zoomLevel);
            zoomSlider.setValue(zoomLevel, juce::dontSendNotification);
            scrollOffset = state.scrollOffset;

            snapToZeroCrossing = state.snapToZeroCrossing;
            snapZeroCrossingButton.setToggleState(snapToZeroCrossing, juce::dontSendNotification);

            isSpectralView = state.isSpectralView;
            spectralToggleButton.setToggleState(isSpectralView, juce::dontSendNotification);
            spectralToggleButton.setButtonText(isSpectralView ? "Spectral: ON" : "Spectral: OFF");

            currentSpectralShape = static_cast<SpectralSelectionShape>(juce::jlimit(0, 4, state.spectralSelectionShape));
            updateShapeButtonStates();

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

// ─────────────────────────────────────────────────────────
//  Workflow Refinements: Undo/Redo, Stats, Library/Sampler, Seam, More DSP
// ─────────────────────────────────────────────────────────
void EditComponent::pushUndoState()
{
    if (!hasAudioToEdit()) return;

    EditUndoEntry entry;
    if (!audioEngine.getAudioBufferCopy(entry.buffer, entry.sampleRate))
        return;

    entry.startRatio = audioEngine.getSampleStartRatio();
    entry.endRatio = audioEngine.getSampleEndRatio();
    entry.loopInRatio = loopInRatio;
    entry.loopOutRatio = loopOutRatio;
    entry.loopMarkersSet = loopMarkersSet;

    undoStack.push_back(std::move(entry));
    if (undoStack.size() > 15)
        undoStack.erase(undoStack.begin());

    redoStack.clear();
    updateUndoRedoButtons();
}

void EditComponent::performUndo()
{
    if (undoStack.empty()) return;

    EditUndoEntry current;
    if (audioEngine.getAudioBufferCopy(current.buffer, current.sampleRate))
    {
        current.startRatio = audioEngine.getSampleStartRatio();
        current.endRatio = audioEngine.getSampleEndRatio();
        current.loopInRatio = loopInRatio;
        current.loopOutRatio = loopOutRatio;
        current.loopMarkersSet = loopMarkersSet;
        redoStack.push_back(std::move(current));
    }

    EditUndoEntry prev = std::move(undoStack.back());
    undoStack.pop_back();

    if (audioEngine.restoreBufferSnapshot(prev.buffer, prev.sampleRate))
    {
        audioEngine.setSampleRange(prev.startRatio, prev.endRatio);
        loopInRatio = prev.loopInRatio;
        loopOutRatio = prev.loopOutRatio;
        loopMarkersSet = prev.loopMarkersSet;
        audioEngine.setLoopPoints(loopInRatio, loopOutRatio);
        audioEngine.setLoopMarkersSet(loopMarkersSet);
        totalDurationSecs = audioEngine.getTotalLengthSeconds();
        spectrogramGenerated = false;
        if (isSpectralView && totalDurationSecs > 0.0)
            generateSpectrogram();
        updateUndoRedoButtons();
        updateSelectionStats();
        repaint();
    }
}

void EditComponent::performRedo()
{
    if (redoStack.empty()) return;

    EditUndoEntry current;
    if (audioEngine.getAudioBufferCopy(current.buffer, current.sampleRate))
    {
        current.startRatio = audioEngine.getSampleStartRatio();
        current.endRatio = audioEngine.getSampleEndRatio();
        current.loopInRatio = loopInRatio;
        current.loopOutRatio = loopOutRatio;
        current.loopMarkersSet = loopMarkersSet;
        undoStack.push_back(std::move(current));
    }

    EditUndoEntry next = std::move(redoStack.back());
    redoStack.pop_back();

    if (audioEngine.restoreBufferSnapshot(next.buffer, next.sampleRate))
    {
        audioEngine.setSampleRange(next.startRatio, next.endRatio);
        loopInRatio = next.loopInRatio;
        loopOutRatio = next.loopOutRatio;
        loopMarkersSet = next.loopMarkersSet;
        audioEngine.setLoopPoints(loopInRatio, loopOutRatio);
        audioEngine.setLoopMarkersSet(loopMarkersSet);
        totalDurationSecs = audioEngine.getTotalLengthSeconds();
        spectrogramGenerated = false;
        if (isSpectralView && totalDurationSecs > 0.0)
            generateSpectrogram();
        updateUndoRedoButtons();
        updateSelectionStats();
        repaint();
    }
}

void EditComponent::updateUndoRedoButtons()
{
    undoButton.setEnabled(!undoStack.empty());
    redoButton.setEnabled(!redoStack.empty());
}

void EditComponent::updateSelectionStats()
{
    if (!hasAudioToEdit())
    {
        selectionStatsLabel.setText("Peak: --  |  RMS: --", juce::dontSendNotification);
        selectionStatsLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
        return;
    }

    juce::AudioBuffer<float> buf;
    double sr = 44100.0;
    if (!audioEngine.getAudioBufferCopy(buf, sr) || buf.getNumSamples() <= 0)
        return;

    const int totalSamples = buf.getNumSamples();
    const int numChannels = buf.getNumChannels();
    const int startSample = juce::jlimit(0, totalSamples - 1, static_cast<int>(audioEngine.getSampleStartRatio() * totalSamples));
    const int endSample = juce::jlimit(startSample + 1, totalSamples, static_cast<int>(audioEngine.getSampleEndRatio() * totalSamples));
    const int len = endSample - startSample;

    float peak = 0.0f;
    double sumSq = 0.0;
    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* data = buf.getReadPointer(ch);
        for (int i = startSample; i < endSample; ++i)
        {
            const float s = data[i];
            const float a = std::abs(s);
            if (a > peak) peak = a;
            sumSq += static_cast<double>(s) * static_cast<double>(s);
        }
    }

    const float rms = static_cast<float>(std::sqrt(sumSq / static_cast<double>(len * std::max(1, numChannels))));
    const float peakDb = juce::Decibels::gainToDecibels(peak, -100.0f);
    const float rmsDb = juce::Decibels::gainToDecibels(rms, -100.0f);

    const juce::String peakStr = (peakDb <= -99.5f) ? "-inf" : juce::String(peakDb, 1);
    const juce::String rmsStr = (rmsDb <= -99.5f) ? "-inf" : juce::String(rmsDb, 1);
    selectionStatsLabel.setText("Peak: " + peakStr + " dB | RMS: " + rmsStr + " dB", juce::dontSendNotification);
    selectionStatsLabel.setColour(juce::Label::textColourId,
        peakDb > 0.0f ? juce::Colour(0xFFFF5555) : OpenWavLookAndFeel::textSecondary);
}

static juce::File writeEditedSelectionWav(AudioEngine& engine,
                                          std::function<void(juce::AudioBuffer<float>&, double)> applyFadesFn,
                                          const juce::String& suffix)
{
    juce::AudioBuffer<float> fullBuffer;
    double sampleRate = 44100.0;
    if (!engine.getAudioBufferCopy(fullBuffer, sampleRate))
        return {};

    const int totalSamples = fullBuffer.getNumSamples();
    const int startSample = juce::jlimit(0, std::max(0, totalSamples - 1),
                                         static_cast<int>(engine.getSampleStartRatio() * totalSamples));
    const int endSample = juce::jlimit(startSample, totalSamples,
                                       static_cast<int>(engine.getSampleEndRatio() * totalSamples));
    const int regionLen = endSample - startSample;
    if (regionLen <= 0)
        return {};

    juce::AudioBuffer<float> editedBuffer(fullBuffer.getNumChannels(), regionLen);
    for (int ch = 0; ch < fullBuffer.getNumChannels(); ++ch)
        editedBuffer.copyFrom(ch, 0, fullBuffer, ch, startSample, regionLen);

    if (applyFadesFn)
        applyFadesFn(editedBuffer, sampleRate);

    juce::File originalFile = engine.getCurrentFile();
    juce::File targetDir = originalFile.existsAsFile()
        ? originalFile.getParentDirectory()
        : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("OWMB Edits");
    targetDir.createDirectory();

    juce::String baseName = originalFile.existsAsFile()
        ? originalFile.getFileNameWithoutExtension()
        : "edited_sample";
    juce::File outFile = targetDir.getChildFile(baseName + suffix + ".wav").getNonexistentSibling();

    std::unique_ptr<juce::FileOutputStream> outStream(outFile.createOutputStream());
    if (outStream == nullptr)
        return {};

    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(
        outStream.get(), sampleRate, static_cast<unsigned int>(editedBuffer.getNumChannels()), 24, {}, 0));
    if (writer == nullptr)
        return {};

    outStream.release();
    writer->writeFromAudioSampleBuffer(editedBuffer, 0, editedBuffer.getNumSamples());
    return outFile;
}

void EditComponent::saveEditedCopyToLibrary()
{
    if (!hasAudioToEdit()) return;

    juce::File outFile = writeEditedSelectionWav(
        audioEngine,
        [this](juce::AudioBuffer<float>& buf, double sr) { applyFadeToBuffer(buf, sr); },
        "_edit");
    if (outFile.existsAsFile() && onSavedToLibrary)
        onSavedToLibrary(outFile);
}

void EditComponent::sendEditedToSampleMap()
{
    if (!hasAudioToEdit()) return;

    juce::File outFile = writeEditedSelectionWav(
        audioEngine,
        [this](juce::AudioBuffer<float>& buf, double sr) { applyFadeToBuffer(buf, sr); },
        "_sampler");
    if (outFile.existsAsFile() && onSendToSampleMap)
        onSendToSampleMap(outFile);
}

void EditComponent::previewLoopSeam()
{
    if (!hasAudioToEdit() || totalDurationSecs <= 0.0) return;

    audioEngine.setLoopPoints(loopInRatio, loopOutRatio);
    audioEngine.setLoopMarkersSet(true);
    audioEngine.setLooping(true);
    loopMarkersSet = true;
    loopToggleButton.setToggleState(true, juce::dontSendNotification);

    const double preRollRatio = 0.5 / totalDurationSecs;
    const double span = std::max(0.002, loopOutRatio - loopInRatio);
    const double startRatio = juce::jlimit(loopInRatio, loopOutRatio - 0.001,
                                           loopOutRatio - std::min(preRollRatio, span * 0.5));
    audioEngine.setPositionRatio(startRatio);
    audioEngine.play();
    repaint();
}

void EditComponent::showMoreDspMenu()
{
    juce::PopupMenu menu;
    menu.addItem(1, "Low Cut (High-Pass 80 Hz)", hasAudioToEdit());
    menu.addItem(2, "Hi Cut (Low-Pass 8 kHz)", hasAudioToEdit());
    menu.addItem(3, "Remove DC Offset", hasAudioToEdit());
    menu.addSeparator();
    menu.addItem(4, "Invert Phase", hasAudioToEdit());
    menu.addItem(5, "Sum Stereo to Mono (L+R)/2", hasAudioToEdit());
    menu.addSeparator();
    menu.addItem(6, "2x Speed (Octave Up)", hasAudioToEdit());
    menu.addItem(7, "0.5x Speed (Octave Down)", hasAudioToEdit());
    menu.addItem(8, "Deverb / Transient Clean", hasAudioToEdit());

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&moreDspButton),
        [this](int result)
        {
            switch (result)
            {
                case 1: hpFilterButton.triggerClick(); break;
                case 2: lpFilterButton.triggerClick(); break;
                case 3: dcOffsetButton.triggerClick(); break;
                case 4: invertPhaseButton.triggerClick(); break;
                case 5: monoSumButton.triggerClick(); break;
                case 6: speed2xButton.triggerClick(); break;
                case 7: speedHalfButton.triggerClick(); break;
                case 8: deverbButton.triggerClick(); break;
                default: break;
            }
        });
}

} // namespace openwav

