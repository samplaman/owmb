#include "RecorderComponent.h"
#include "OpenWavLookAndFeel.h"
#include "ToastOverlay.h"

namespace openwav
{

RecorderComponent::RecorderComponent(AudioEngine& engine, TagDatabaseManager& db)
    : audioEngine(engine), dbManager(db)
{
    setWantsKeyboardFocus(true);
    // Record / Stop Toggle Button
    recordButton.setComponentID("iconOnlyBtn");
    recordButton.setTooltip("Record / Stop (Arm or capture live audio)");
    recordButton.onClick = [this] { toggleRecording(); };
    addAndMakeVisible(recordButton);

    // Pause / Resume Button
    pauseButton.setComponentID("iconOnlyBtn");
    pauseButton.setTooltip("Pause / Resume (Punch-In at current position)");
    pauseButton.onClick = [this] { togglePauseRecording(); };
    pauseButton.setEnabled(false);
    addAndMakeVisible(pauseButton);

    // Clear Button
    clearButton.setComponentID("iconOnlyBtn");
    clearButton.setTooltip("Clear current recorded take");
    clearButton.onClick = [this] { clearCurrentTake(); };
    clearButton.setEnabled(false);
    addAndMakeVisible(clearButton);

    // Preview / Play Button
    previewButton.setComponentID("iconOnlyBtn");
    previewButton.setTooltip("Play / Audition recorded take (respects Trim range)");
    previewButton.onClick = [this] { playPreview(); };
    previewButton.setEnabled(false);
    addAndMakeVisible(previewButton);

    // Trim Button
    trimButton.setComponentID("iconOnlyBtn");
    trimButton.setTooltip("Crop recorded take to Trim Start / End markers");
    trimButton.onClick = [this] { trimCurrentTake(); };
    trimButton.setEnabled(false);
    addAndMakeVisible(trimButton);

    // Save Button
    saveButton.setComponentID("iconOnlyBtn");
    saveButton.setTooltip("Save recording to WAV and add to Library");
    saveButton.onClick = [this] { saveAndAddToLibrary(); };
    saveButton.setEnabled(false);
    addAndMakeVisible(saveButton);

    // Send to Editor Button
    toEditorButton.setComponentID("iconOnlyBtn");
    toEditorButton.setTooltip("Save recording and open directly in Audio Editor");
    toEditorButton.onClick = [this] {
        MediaItem item;
        if (saveCurrentTakeToDiskAndDb(item, false) && onSendToEditor)
            onSendToEditor(item);
    };
    toEditorButton.setEnabled(false);
    addAndMakeVisible(toEditorButton);

    // Send to Sampler Button
    toSamplerButton.setComponentID("iconOnlyBtn");
    toSamplerButton.setTooltip("Save recording and add directly to Sample Map");
    toSamplerButton.onClick = [this] {
        MediaItem item;
        if (saveCurrentTakeToDiskAndDb(item, false) && onSendToSampleMap)
            onSendToSampleMap(item);
    };
    toSamplerButton.setEnabled(false);
    addAndMakeVisible(toSamplerButton);

    OpenWavLookAndFeel::groupButtons({ &recordButton, &pauseButton, &clearButton, &previewButton });
    OpenWavLookAndFeel::groupButtons({ &saveButton, &toEditorButton, &toSamplerButton });

    // Audio Setup Button
    audioSetupButton.setComponentID("iconOnlyBtn");
    audioSetupButton.setTooltip("Open Audio / Microphone Input Device Settings");
    audioSetupButton.onClick = [this] {
        if (onOpenAudioSettings)
            onOpenAudioSettings();
    };
    addAndMakeVisible(audioSetupButton);

    // Input Monitoring Toggle (Default OFF to prevent mic feedback)
    monitorButton.setToggleState(false, juce::dontSendNotification);
    monitorButton.setTooltip("Live Input Monitoring (listen to input through speakers/headphones)");
    monitorButton.onClick = [this] { updateInputState(); };
    addAndMakeVisible(monitorButton);

    // Metronome Click & BPM
    metronomeButton.setToggleState(false, juce::dontSendNotification);
    metronomeButton.setTooltip("Play metronome click during recording");
    addAndMakeVisible(metronomeButton);

    bpmSlider.setRange(60.0, 200.0, 1.0);
    bpmSlider.setValue(120.0, juce::dontSendNotification);
    bpmSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    bpmSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    bpmSlider.setTooltip("Recording Metronome Tempo (BPM)");
    bpmSlider.onValueChange = [this] {
        int bpm = juce::roundToInt(bpmSlider.getValue());
        bpmLabel.setText(juce::String(bpm) + " BPM", juce::dontSendNotification);
    };
    addAndMakeVisible(bpmSlider);

    bpmLabel.setFont(OpenWavLookAndFeel::getMonoFont(11.5f, true));
    bpmLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    bpmLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(bpmLabel);

    // Wait Signal (Auto-Threshold Arm)
    waitSignalButton.setToggleState(false, juce::dontSendNotification);
    waitSignalButton.setTooltip("Threshold Recording: waits for audio signal above -36 dB before starting");
    addAndMakeVisible(waitSignalButton);

    // Input Gain Slider
    inputGainSlider.setRange(-24.0, 24.0, 0.5);
    inputGainSlider.setValue(0.0, juce::dontSendNotification);
    inputGainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    inputGainSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    inputGainSlider.setTooltip("Input Pre-Gain (-24 dB to +24 dB)");
    inputGainSlider.onValueChange = [this] {
        float db = static_cast<float>(inputGainSlider.getValue());
        audioEngine.setInputGainDb(db);
        inputGainLabel.setText("Gain: " + juce::String(db, 1) + " dB", juce::dontSendNotification);
    };
    addAndMakeVisible(inputGainSlider);

    inputGainLabel.setFont(OpenWavLookAndFeel::getMonoFont(11.5f, true));
    inputGainLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    addAndMakeVisible(inputGainLabel);

    // Input Channel Selector
    channelLabel.setFont(juce::Font(11.5f).boldened());
    channelLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    addAndMakeVisible(channelLabel);

    channelSelector.addItem("Stereo (Ch 1+2)", 1);
    channelSelector.addItem("Mono - Input 1 (L)", 2);
    channelSelector.addItem("Mono - Input 2 (R)", 3);
    channelSelector.setSelectedId(1, juce::dontSendNotification);
    channelSelector.onChange = [this] {
        int id = channelSelector.getSelectedId();
        if (id == 2)
        {
            audioEngine.setRecordingChannelMode(AudioEngine::RecordingChannelMode::MonoLeft);
            formatInfoLabel.setText("24-bit WAV  •  Mono (L)", juce::dontSendNotification);
        }
        else if (id == 3)
        {
            audioEngine.setRecordingChannelMode(AudioEngine::RecordingChannelMode::MonoRight);
            formatInfoLabel.setText("24-bit WAV  •  Mono (R)", juce::dontSendNotification);
        }
        else
        {
            audioEngine.setRecordingChannelMode(AudioEngine::RecordingChannelMode::Stereo);
            formatInfoLabel.setText("24-bit WAV  •  Stereo", juce::dontSendNotification);
        }
    };
    addAndMakeVisible(channelSelector);

    // Count-In Selector
    countInLabel.setFont(juce::Font(11.5f).boldened());
    countInLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    addAndMakeVisible(countInLabel);

    countInSelector.addItem("Off", 1);
    countInSelector.addItem("1 Bar (4 Beats)", 2);
    countInSelector.addItem("3s Count-In", 3);
    countInSelector.setSelectedId(2, juce::dontSendNotification);
    addAndMakeVisible(countInSelector);

    // Take Switcher
    takeLabel.setFont(juce::Font(11.5f).boldened());
    takeLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    addAndMakeVisible(takeLabel);

    takeSelector.setTextWhenNoChoicesAvailable("No Takes");
    takeSelector.onChange = [this] {
        int id = takeSelector.getSelectedId();
        if (id >= 1)
            loadTakeIntoView(id - 1);
    };
    addAndMakeVisible(takeSelector);

    // Name Label & Editor
    nameLabel.setFont(juce::Font(11.5f).boldened());
    nameLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    addAndMakeVisible(nameLabel);

    nameEditor.setText("Rec_" + juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S"));
    nameEditor.setJustification(juce::Justification::centredLeft);
    nameEditor.setIndents(6, 0);
    addAndMakeVisible(nameEditor);

    // Tags Label & Editor
    tagsLabel.setFont(juce::Font(11.5f).boldened());
    tagsLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    addAndMakeVisible(tagsLabel);

    tagsEditor.setText("Recorded, Custom");
    tagsEditor.setJustification(juce::Justification::centredLeft);
    tagsEditor.setIndents(6, 0);
    addAndMakeVisible(tagsEditor);

    // Time Label
    timeLabel.setFont(OpenWavLookAndFeel::getMonoFont(22.0f, true));
    timeLabel.setJustificationType(juce::Justification::centredLeft);
    timeLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textPrimary);
    timeLabel.setText("00:00.0", juce::dontSendNotification);
    addAndMakeVisible(timeLabel);

    formatInfoLabel.setFont(juce::Font(11.0f).boldened());
    formatInfoLabel.setJustificationType(juce::Justification::centredLeft);
    formatInfoLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    formatInfoLabel.setText("24-bit WAV  •  Stereo", juce::dontSendNotification);
    addAndMakeVisible(formatInfoLabel);

    // Status Label
    statusLabel.setFont(juce::Font(12.0f).boldened());
    statusLabel.setJustificationType(juce::Justification::centredLeft);
    statusLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    statusLabel.setText("Ready — live input meters active. Press REC or arm Wait Signal.", juce::dontSendNotification);
    addAndMakeVisible(statusLabel);

    // DSP Filter & Normalization Toggles
    lowCutButton.setToggleState(false, juce::dontSendNotification);
    lowCutButton.onClick = [this] {
        audioEngine.setInputParametricEq(eqFreqs, eqGains, lowCutButton.getToggleState());
        repaint();
    };
    addAndMakeVisible(lowCutButton);

    normalizeButton.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(normalizeButton);

    resetEqButton.setComponentID("iconOnlyBtn");
    resetEqButton.setTooltip("Reset all 9 parametric EQ bands to 0 dB");
    resetEqButton.onClick = [this] { resetEq(); };
    addAndMakeVisible(resetEqButton);

    lookAndFeelChanged();

    audioEngine.setInputParametricEq(eqFreqs, eqGains, lowCutButton.getToggleState());
    audioEngine.setInputGainDb(0.0f);

    startTimerHz(30);
    updateInputState();
}

RecorderComponent::~RecorderComponent()
{
    stopTimer();
    audioEngine.setInputListeningEnabled(false);
    audioEngine.setInputMuted(true);
}

void RecorderComponent::resetEq()
{
    eqFreqs = { 60.0f, 120.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f };
    eqGains.fill(0.0f);
    lowCutButton.setToggleState(false, juce::dontSendNotification);
    audioEngine.setInputParametricEq(eqFreqs, eqGains, false);
    repaint();
}

juce::Point<float> RecorderComponent::getEqNodeScreenPos(float freq, float gainDb, juce::Rectangle<float> eqBounds) const
{
    if (eqBounds.isEmpty()) return { 0.0f, 0.0f };

    float normX = std::log10(juce::jlimit(20.0f, 20000.0f, freq) / 20.0f) / std::log10(1000.0f);
    float x = eqBounds.getX() + eqBounds.getWidth() * juce::jlimit(0.0f, 1.0f, normX);

    float normY = juce::jlimit(-15.0f, 15.0f, gainDb) / 15.0f;
    float y = eqBounds.getCentreY() - normY * (eqBounds.getHeight() * 0.40f);

    return { x, y };
}

void RecorderComponent::updateEqNodeFromScreenPos(int nodeIdx, juce::Point<float> pos, juce::Rectangle<float> eqBounds)
{
    if (eqBounds.isEmpty()) return;

    float normX = juce::jlimit(0.0f, 1.0f, (pos.x - eqBounds.getX()) / eqBounds.getWidth());
    float freq = 20.0f * std::pow(1000.0f, normX);

    float normY = (eqBounds.getCentreY() - pos.y) / (eqBounds.getHeight() * 0.40f);
    float gainDb = juce::jlimit(-12.0f, 12.0f, normY * 15.0f);

    if (nodeIdx >= 0 && nodeIdx < 9)
    {
        eqFreqs[nodeIdx] = juce::jlimit(20.0f, 20000.0f, freq);
        eqGains[nodeIdx] = gainDb;
    }

    audioEngine.setInputParametricEq(eqFreqs, eqGains, lowCutButton.getToggleState());
}

void RecorderComponent::mouseMove(const juce::MouseEvent& e)
{
    juce::Point<float> mousePos = e.position.toFloat();

    if (!cachedScopeArea.isEmpty() && hasRecordedBuffer && !audioEngine.isRecording())
    {
        float startX = cachedScopeArea.getX() + static_cast<float>(trimStartRatio) * cachedScopeArea.getWidth();
        float endX = cachedScopeArea.getX() + static_cast<float>(trimEndRatio) * cachedScopeArea.getWidth();
        if (cachedScopeArea.contains(mousePos) && (std::abs(mousePos.x - startX) <= 7.0f || std::abs(mousePos.x - endX) <= 7.0f))
        {
            setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
            return;
        }
    }

    if (cachedEqArea.isEmpty())
    {
        setMouseCursor(juce::MouseCursor::NormalCursor);
        return;
    }

    int prevHovered = hoveredNodeIndex;
    hoveredNodeIndex = -1;

    for (int i = 0; i < 9; ++i)
    {
        juce::Point<float> nodePos = getEqNodeScreenPos(eqFreqs[i], eqGains[i], cachedEqArea);
        if (nodePos.getDistanceFrom(mousePos) <= 14.0f)
        {
            hoveredNodeIndex = i;
            break;
        }
    }

    if (hoveredNodeIndex != prevHovered)
    {
        setMouseCursor(hoveredNodeIndex >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
    else if (hoveredNodeIndex < 0)
    {
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }
}

void RecorderComponent::mouseDown(const juce::MouseEvent& e)
{
    juce::Point<float> mousePos = e.position.toFloat();
    dragMode = DragMode::None;
    draggedNodeIndex = -1;

    // Click on VU Meters resets clip LEDs
    if (!cachedMeterArea.isEmpty() && cachedMeterArea.expanded(4.0f).contains(mousePos))
    {
        clipLeft = false;
        clipRight = false;
        peakHoldLeft = 0.0f;
        peakHoldRight = 0.0f;
        repaint();
        return;
    }

    // Check Trim handles on recorded waveform
    if (!cachedScopeArea.isEmpty() && hasRecordedBuffer && !audioEngine.isRecording() && cachedScopeArea.contains(mousePos))
    {
        float startX = cachedScopeArea.getX() + static_cast<float>(trimStartRatio) * cachedScopeArea.getWidth();
        float endX = cachedScopeArea.getX() + static_cast<float>(trimEndRatio) * cachedScopeArea.getWidth();
        if (std::abs(mousePos.x - startX) <= 8.0f)
        {
            dragMode = DragMode::TrimStart;
            return;
        }
        if (std::abs(mousePos.x - endX) <= 8.0f)
        {
            dragMode = DragMode::TrimEnd;
            return;
        }
        dragMode = DragMode::WaveDragOut;
        externalDragStarted = false;
        return;
    }

    if (cachedEqArea.isEmpty()) return;

    for (int i = 0; i < 9; ++i)
    {
        juce::Point<float> nodePos = getEqNodeScreenPos(eqFreqs[i], eqGains[i], cachedEqArea);
        if (nodePos.getDistanceFrom(mousePos) <= 16.0f)
        {
            dragMode = DragMode::EqNode;
            draggedNodeIndex = i;
            break;
        }
    }
}

void RecorderComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (dragMode == DragMode::TrimStart && !cachedScopeArea.isEmpty())
    {
        double ratio = juce::jlimit(0.0, trimEndRatio - 0.005, static_cast<double>((e.position.x - cachedScopeArea.getX()) / cachedScopeArea.getWidth()));
        trimStartRatio = ratio;
        repaint();
    }
    else if (dragMode == DragMode::TrimEnd && !cachedScopeArea.isEmpty())
    {
        double ratio = juce::jlimit(trimStartRatio + 0.005, 1.0, static_cast<double>((e.position.x - cachedScopeArea.getX()) / cachedScopeArea.getWidth()));
        trimEndRatio = ratio;
        repaint();
    }
    else if (dragMode == DragMode::WaveDragOut && !externalDragStarted && e.getDistanceFromDragStart() > 6)
    {
        externalDragStarted = true;
        juce::File dragFile = lastSavedFile.existsAsFile() ? lastSavedFile : lastPreviewTempFile;
        if (!dragFile.existsAsFile())
        {
            syncGlobalWaveformToCurrentTake();
            dragFile = lastPreviewTempFile;
        }
        if (dragFile.existsAsFile())
        {
            juce::DragAndDropContainer::performExternalDragDropOfFiles({ dragFile.getFullPathName() }, false, this);
        }
    }
    else if (dragMode == DragMode::EqNode && draggedNodeIndex >= 0 && !cachedEqArea.isEmpty())
    {
        updateEqNodeFromScreenPos(draggedNodeIndex, e.position.toFloat(), cachedEqArea);
        repaint();
    }
}

void RecorderComponent::mouseUp(const juce::MouseEvent& /*e*/)
{
    dragMode = DragMode::None;
    draggedNodeIndex = -1;
}

void RecorderComponent::mouseDoubleClick(const juce::MouseEvent& e)
{
    juce::Point<float> mousePos = e.position.toFloat();
    if (!cachedEqArea.isEmpty() && cachedEqArea.contains(mousePos))
    {
        for (int i = 0; i < 9; ++i)
        {
            juce::Point<float> nodePos = getEqNodeScreenPos(eqFreqs[i], eqGains[i], cachedEqArea);
            if (nodePos.getDistanceFrom(mousePos) <= 16.0f)
            {
                eqGains[i] = 0.0f;
                audioEngine.setInputParametricEq(eqFreqs, eqGains, lowCutButton.getToggleState());
                repaint();
                return;
            }
        }
    }
    if (!cachedScopeArea.isEmpty() && cachedScopeArea.contains(mousePos) && hasRecordedBuffer && !audioEngine.isRecording())
    {
        trimStartRatio = 0.0;
        trimEndRatio = 1.0;
        repaint();
    }
}

bool RecorderComponent::keyPressed(const juce::KeyPress& key)
{
    if (key.getKeyCode() == juce::KeyPress::escapeKey && (isCountingDown || isArmedWaitSignal))
    {
        toggleRecording();
        return true;
    }
    if (key.getTextCharacter() == 'r' || key.getTextCharacter() == 'R')
    {
        toggleRecording();
        return true;
    }
    if (key.getKeyCode() == juce::KeyPress::spaceKey)
    {
        if (audioEngine.isRecording() || isPausedRecording)
            togglePauseRecording();
        else if (hasRecordedBuffer)
            playPreview();
        return true;
    }
    return false;
}

void RecorderComponent::paint(juce::Graphics& g)
{
    g.fillAll(OpenWavLookAndFeel::bgDark);

    auto bounds = getLocalBounds().reduced(14);

    // Top Transport Bar Background
    auto transportBar = bounds.removeFromTop(54).toFloat();
    OpenWavLookAndFeel::drawPanelCard(g, transportBar, 8.0f, false);
    OpenWavLookAndFeel::drawButtonGroup(g, { &recordButton, &pauseButton, &clearButton, &previewButton });

    bounds.removeFromTop(10);

    // Bottom Controls Deck Reserve (2 rows inside rounded DAW card = 86px)
    auto mainCard = bounds.removeFromTop(std::max(160, bounds.getHeight() - 96)).toFloat();
    auto bottomDeck = bounds.withTrimmedTop(10).toFloat();
    OpenWavLookAndFeel::drawPanelCard(g, bottomDeck, 8.0f, false);
    OpenWavLookAndFeel::drawButtonGroup(g, { &saveButton, &toEditorButton, &toSamplerButton });
    g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.35f));
    g.drawHorizontalLine(static_cast<int>(bottomDeck.getY() + 42.0f), bottomDeck.getX() + 10.0f, bottomDeck.getRight() - 10.0f);

    // Split into Left (DAW Waveform Timeline + Stereo VU Meters) and Right (Parametric EQ)
    float gap = 12.0f;
    float leftW = std::max(260.0f, (mainCard.getWidth() - gap) * 0.56f);
    float rightW = mainCard.getWidth() - leftW - gap;

    auto leftCard = mainCard.removeFromLeft(leftW);
    mainCard.removeFromLeft(gap);
    auto rightCard = mainCard.removeFromLeft(rightW);

    // --- LEFT CARD: DAW WAVEFORM TIMELINE & STEREO VU METERS ---
    g.setColour(OpenWavLookAndFeel::bgHeader);
    g.fillRoundedRectangle(leftCard, 8.0f);
    g.setColour(OpenWavLookAndFeel::borderColour);
    g.drawRoundedRectangle(leftCard, 8.0f, 1.0f);

    auto scopeOuter = leftCard.reduced(12.0f);

    // Header Row inside Left Card
    auto leftHdr = scopeOuter.removeFromTop(18.0f);
    g.setFont(juce::Font(11.5f).boldened());
    if (audioEngine.isRecording())
    {
        float pulse = 0.55f + 0.45f * std::sin(juce::Time::getMillisecondCounter() * 0.01f);
        g.setColour(OpenWavLookAndFeel::favoriteRed.withAlpha(pulse));
        g.fillEllipse(leftHdr.getX(), leftHdr.getCentreY() - 4.0f, 8.0f, 8.0f);
        g.setColour(OpenWavLookAndFeel::favoriteRed);
        g.drawText("LIVE RECORDING TIMELINE", leftHdr.withTrimmedLeft(14.0f), juce::Justification::left, true);
    }
    else
    {
        g.setColour(isArmedWaitSignal ? juce::Colours::orange : OpenWavLookAndFeel::accentCyan);
        juce::String timelineTitle = isArmedWaitSignal
            ? "ARMED — WAITING FOR AUDIO SIGNAL"
            : "RECORDING TIMELINE & TRIM";
        g.drawText(timelineTitle, leftHdr, juce::Justification::left, true);
    }
    scopeOuter.removeFromTop(4.0f);

    // Reserve right side of Left Card for Stereo VU Meters + dB Scale
    auto meterCol = scopeOuter.removeFromRight(62.0f);
    scopeOuter.removeFromRight(10.0f);
    cachedMeterArea = meterCol;

    // Time Ruler above waveform
    auto rulerArea = scopeOuter.removeFromTop(18.0f);
    g.setColour(OpenWavLookAndFeel::bgDark.brighter(0.05f));
    g.fillRoundedRectangle(rulerArea, 4.0f);

    // Waveform Area
    auto scopeArea = scopeOuter;
    cachedScopeArea = scopeArea;

    g.setColour(OpenWavLookAndFeel::bgDark);
    g.fillRoundedRectangle(scopeArea, 6.0f);
    g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.6f));
    g.drawRoundedRectangle(scopeArea, 6.0f, 1.0f);

    // Amplitude Grid Lines (0dB center, +/-50%)
    g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.28f));
    g.drawHorizontalLine(static_cast<int>(scopeArea.getCentreY()), scopeArea.getX(), scopeArea.getRight());
    g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.14f));
    g.drawHorizontalLine(static_cast<int>(scopeArea.getY() + scopeArea.getHeight() * 0.25f), scopeArea.getX(), scopeArea.getRight());
    g.drawHorizontalLine(static_cast<int>(scopeArea.getY() + scopeArea.getHeight() * 0.75f), scopeArea.getX(), scopeArea.getRight());

    // Resolve waveform buffer (live recording buffer or selected take)
    juce::AudioBuffer<float> recBuf;
    double sr = 44100.0;
    bool hasData = audioEngine.getRecordedBufferCopy(recBuf, sr);
    if (!hasData && activeTakeIndex >= 0 && activeTakeIndex < static_cast<int>(takes.size()))
    {
        recBuf.makeCopyOf(takes[static_cast<size_t>(activeTakeIndex)].buffer);
        sr = takes[static_cast<size_t>(activeTakeIndex)].sampleRate;
        hasData = (recBuf.getNumSamples() > 0);
    }

    // Draw Time Ruler Ticks
    double totalDurSecs = (hasData && sr > 0.0) ? (static_cast<double>(recBuf.getNumSamples()) / sr) : 10.0;
    double tickInterval = (totalDurSecs <= 5.0) ? 0.5 : (totalDurSecs <= 20.0 ? 2.0 : (totalDurSecs <= 60.0 ? 5.0 : 15.0));
    g.setFont(juce::Font(9.5f));
    for (double t = 0.0; t <= totalDurSecs + 0.001; t += tickInterval)
    {
        float normT = static_cast<float>(t / std::max(0.1, totalDurSecs));
        float tx = rulerArea.getX() + normT * rulerArea.getWidth();
        if (tx > rulerArea.getRight() - 4.0f) break;
        g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.5f));
        g.drawVerticalLine(static_cast<int>(tx), rulerArea.getBottom() - 5.0f, rulerArea.getBottom());
        g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.12f));
        g.drawVerticalLine(static_cast<int>(tx), scopeArea.getY(), scopeArea.getBottom());
        g.setColour(OpenWavLookAndFeel::textSecondary);
        g.drawText(juce::String(t, tickInterval < 1.0 ? 1 : 0) + "s", tx + 2.0f, rulerArea.getY(), 36.0f, rulerArea.getHeight(), juce::Justification::centredLeft, false);
    }

    if (hasData && recBuf.getNumSamples() > 0)
    {
        int numSamples = recBuf.getNumSamples();
        int width = std::max(1, static_cast<int>(scopeArea.getWidth()));
        float centerY = scopeArea.getCentreY();
        float halfH = (scopeArea.getHeight() - 8.0f) * 0.5f;

        juce::Colour waveCol = audioEngine.isRecording() ? OpenWavLookAndFeel::favoriteRed : OpenWavLookAndFeel::accentCyan;

        juce::Path scopePath;
        juce::Path fillTop, fillBot;
        int step = std::max(1, numSamples / width);

        for (int x = 0; x < width; ++x)
        {
            int sStart = x * step;
            if (sStart >= numSamples) break;
            int sEnd = std::min(numSamples, sStart + step);

            float minVal = 0.0f, maxVal = 0.0f;
            for (int ch = 0; ch < recBuf.getNumChannels(); ++ch)
            {
                auto r = recBuf.findMinMax(ch, sStart, sEnd - sStart);
                if (r.getStart() < minVal) minVal = r.getStart();
                if (r.getEnd() > maxVal) maxVal = r.getEnd();
            }

            float px = scopeArea.getX() + x;
            float clampedMax = juce::jlimit(-1.0f, 1.0f, maxVal);
            float clampedMin = juce::jlimit(-1.0f, 1.0f, minVal);
            float pyMin = juce::jlimit(scopeArea.getY() + 1.0f, scopeArea.getBottom() - 1.0f, centerY - clampedMax * halfH);
            float pyMax = juce::jlimit(scopeArea.getY() + 1.0f, scopeArea.getBottom() - 1.0f, centerY - clampedMin * halfH);

            if (x == 0)
            {
                scopePath.startNewSubPath(px, pyMin);
                fillTop.startNewSubPath(px, centerY);
                fillBot.startNewSubPath(px, centerY);
            }

            scopePath.lineTo(px, pyMax);
            scopePath.lineTo(px, pyMin);
            fillTop.lineTo(px, pyMin);
            fillBot.lineTo(px, pyMax);
        }

        g.saveState();
        g.reduceClipRegion(scopeArea.reduced(1.0f).toNearestInt());

        if (width > 1)
        {
            float lastX = scopeArea.getX() + static_cast<float>(width - 1);
            fillTop.lineTo(lastX, centerY);
            fillTop.closeSubPath();
            fillBot.lineTo(lastX, centerY);
            fillBot.closeSubPath();
            g.setColour(waveCol.withAlpha(0.16f));
            g.fillPath(fillTop);
            g.fillPath(fillBot);
        }

        g.setColour(waveCol);
        g.strokePath(scopePath, juce::PathStrokeType(1.25f));
        g.restoreState();

        // Draw Trim Regions & Handles when not actively recording
        if (!audioEngine.isRecording() && (trimStartRatio > 0.0 || trimEndRatio < 1.0 || hasRecordedBuffer))
        {
            float sx = scopeArea.getX() + static_cast<float>(trimStartRatio) * scopeArea.getWidth();
            float ex = scopeArea.getX() + static_cast<float>(trimEndRatio) * scopeArea.getWidth();

            if (trimStartRatio > 0.001)
            {
                g.setColour(juce::Colours::black.withAlpha(0.45f));
                g.fillRect(scopeArea.getX(), scopeArea.getY(), sx - scopeArea.getX(), scopeArea.getHeight());
            }
            if (trimEndRatio < 0.999)
            {
                g.setColour(juce::Colours::black.withAlpha(0.45f));
                g.fillRect(ex, scopeArea.getY(), scopeArea.getRight() - ex, scopeArea.getHeight());
            }

            g.setColour(juce::Colours::orange);
            g.drawLine(sx, scopeArea.getY(), sx, scopeArea.getBottom(), 2.0f);
            g.drawLine(ex, scopeArea.getY(), ex, scopeArea.getBottom(), 2.0f);

            // Handle flags
            g.fillRoundedRectangle(sx - 1.0f, scopeArea.getY(), 14.0f, 14.0f, 2.0f);
            g.fillRoundedRectangle(ex - 13.0f, scopeArea.getY(), 14.0f, 14.0f, 2.0f);
            g.setFont(juce::Font(9.0f).boldened());
            g.setColour(juce::Colours::black);
            g.drawText("S", sx - 1.0f, scopeArea.getY(), 14.0f, 14.0f, juce::Justification::centred, false);
            g.drawText("E", ex - 13.0f, scopeArea.getY(), 14.0f, 14.0f, juce::Justification::centred, false);

            if (trimStartRatio > 0.001 || trimEndRatio < 0.999)
            {
                double selDur = (trimEndRatio - trimStartRatio) * totalDurSecs;
                juce::String selText = "Sel: " + juce::String(selDur, 2) + "s";
                auto badgeRect = juce::Rectangle<float>(scopeArea.getRight() - 82.0f, scopeArea.getY() + 4.0f, 76.0f, 18.0f);
                g.setColour(OpenWavLookAndFeel::bgHeader.withAlpha(0.9f));
                g.fillRoundedRectangle(badgeRect, 4.0f);
                g.setColour(juce::Colours::orange);
                g.setFont(juce::Font(9.5f).boldened());
                g.drawText(selText, badgeRect, juce::Justification::centred, false);
            }
        }

        // Live preview playhead when auditioning this take
        if (!audioEngine.isRecording() && lastPreviewTempFile.existsAsFile()
            && audioEngine.getCurrentFile() == lastPreviewTempFile)
        {
            double totalLen = audioEngine.getTotalLengthSeconds();
            if (totalLen > 0.0)
            {
                double normPos = juce::jlimit(0.0, 1.0, audioEngine.getCurrentPositionSeconds() / totalLen);
                float playX = scopeArea.getX() + static_cast<float>(normPos) * scopeArea.getWidth();
                g.setColour(juce::Colours::white.withAlpha(0.9f));
                g.drawLine(playX, scopeArea.getY(), playX, scopeArea.getBottom(), 1.5f);
            }
        }
    }
    else
    {
        g.setFont(juce::Font(13.0f));
        g.setColour(OpenWavLookAndFeel::textSecondary);
        juce::String emptyMsg = isArmedWaitSignal
            ? "Armed: play or speak above -36 dB to start recording..."
            : "Live input meters are active. Press REC to record.";
        g.drawText(emptyMsg, scopeArea, juce::Justification::centred, true);
    }

    // Beat Flash Pulse Overlay
    if (flashAlpha > 0.01f)
    {
        g.setColour(OpenWavLookAndFeel::favoriteRed.withAlpha(flashAlpha * 0.30f));
        g.fillRoundedRectangle(scopeArea, 6.0f);
        flashAlpha *= 0.82f;
    }

    // Visual Countdown HUD Overlay
    if (isCountingDown)
    {
        auto hudRect = scopeArea.withSizeKeepingCentre(160.0f, 110.0f);
        g.setColour(OpenWavLookAndFeel::bgHeader.withAlpha(0.94f));
        g.fillRoundedRectangle(hudRect, 14.0f);
        g.setColour(OpenWavLookAndFeel::favoriteRed);
        g.drawRoundedRectangle(hudRect, 14.0f, 2.5f);

        auto numRect = hudRect.removeFromTop(72.0f);
        g.setFont(juce::Font(48.0f).boldened());
        g.setColour(OpenWavLookAndFeel::favoriteRed);
        g.drawText(juce::String(countdownValue), numRect, juce::Justification::centred, false);

        g.setFont(juce::Font(11.5f).boldened());
        g.setColour(OpenWavLookAndFeel::textPrimary);
        g.drawText("COUNT-IN...", hudRect, juce::Justification::centred, false);
    }

    // Draw Dual Stereo VU Level Meters with Clip LED, Peak Hold, and dB Scale
    auto clipRow = meterCol.removeFromTop(14.0f);
    meterCol.removeFromTop(3.0f);
    auto dbScaleArea = meterCol.removeFromLeft(22.0f);
    auto meterBarsArea = meterCol;
    auto meterLeft = meterBarsArea.removeFromLeft(16.0f);
    auto meterRight = meterBarsArea.removeFromRight(16.0f);

    // Clip LEDs
    auto clipLRect = clipRow.removeFromRight(16.0f);
    clipRow.removeFromRight(4.0f);
    auto clipRRect = clipRow.removeFromRight(16.0f);
    g.setColour(clipLeft ? OpenWavLookAndFeel::favoriteRed : OpenWavLookAndFeel::bgDark);
    g.fillRoundedRectangle(clipRRect, 2.0f);
    g.setColour(clipRight ? OpenWavLookAndFeel::favoriteRed : OpenWavLookAndFeel::bgDark);
    g.fillRoundedRectangle(clipLRect, 2.0f);
    g.setFont(juce::Font(8.5f).boldened());
    g.setColour(OpenWavLookAndFeel::textSecondary);
    g.drawText("CLIP", clipRow, juce::Justification::centredLeft, false);

    // dB scale ticks
    const float dbMarks[4] = { 0.0f, -6.0f, -12.0f, -24.0f };
    g.setFont(juce::Font(8.5f));
    g.setColour(OpenWavLookAndFeel::textSecondary.withAlpha(0.75f));
    for (float db : dbMarks)
    {
        float norm = juce::jlimit(0.0f, 1.0f, (db + 50.0f) / 50.0f);
        float y = meterBarsArea.getBottom() - norm * meterBarsArea.getHeight();
        g.drawText(juce::String(static_cast<int>(db)), dbScaleArea.getX(), y - 6.0f, dbScaleArea.getWidth() - 2.0f, 12.0f, juce::Justification::right, false);
    }

    auto drawMeter = [&g](juce::Rectangle<float> rect, float level, float peakHold) {
        g.setColour(OpenWavLookAndFeel::bgDark);
        g.fillRoundedRectangle(rect, 3.0f);
        g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.5f));
        g.drawRoundedRectangle(rect, 3.0f, 1.0f);

        float fillH = rect.getHeight() * juce::jlimit(0.0f, 1.0f, level);
        if (fillH > 0.0f)
        {
            auto fillRect = rect.withTop(rect.getBottom() - fillH);
            juce::Colour barCol = (level > 0.92f) ? OpenWavLookAndFeel::favoriteRed
                                : (level > 0.75f ? juce::Colours::orange : OpenWavLookAndFeel::accentCyan);
            g.setColour(barCol);
            g.fillRoundedRectangle(fillRect, 2.0f);
        }

        if (peakHold > 0.02f)
        {
            float py = rect.getBottom() - rect.getHeight() * juce::jlimit(0.0f, 1.0f, peakHold);
            g.setColour(peakHold > 0.96f ? OpenWavLookAndFeel::favoriteRed : juce::Colours::white);
            g.fillRect(rect.getX() + 1.0f, py, rect.getWidth() - 2.0f, 2.0f);
        }
    };

    drawMeter(meterLeft, smoothLeftLevel, peakHoldLeft);
    drawMeter(meterRight, smoothRightLevel, peakHoldRight);

    // --- RIGHT CARD: PARAMETRIC EQ GRAPH ---
    g.setColour(OpenWavLookAndFeel::bgHeader);
    g.fillRoundedRectangle(rightCard, 8.0f);
    g.setColour(OpenWavLookAndFeel::borderColour);
    g.drawRoundedRectangle(rightCard, 8.0f, 1.0f);

    auto eqArea = rightCard.reduced(12.0f);

    g.setFont(juce::Font(11.5f).boldened());
    g.setColour(OpenWavLookAndFeel::accentCyan);
    g.drawText("INPUT PARAMETRIC EQ (9-BAND)", eqArea.removeFromTop(18.0f), juce::Justification::left, true);
    eqArea.removeFromTop(4.0f);

    cachedEqArea = eqArea;

    g.setColour(OpenWavLookAndFeel::bgDark);
    g.fillRoundedRectangle(eqArea, 6.0f);
    g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.6f));
    g.drawRoundedRectangle(eqArea, 6.0f, 1.0f);

    // EQ Grid Lines (0dB, +6dB, -6dB)
    float eqCenterY = eqArea.getCentreY();
    g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.35f));
    g.drawHorizontalLine(static_cast<int>(eqCenterY), eqArea.getX(), eqArea.getRight());

    g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.18f));
    float plus6Y = eqCenterY - (6.0f / 15.0f) * (eqArea.getHeight() * 0.40f);
    float minus6Y = eqCenterY + (6.0f / 15.0f) * (eqArea.getHeight() * 0.40f);
    g.drawHorizontalLine(static_cast<int>(plus6Y), eqArea.getX(), eqArea.getRight());
    g.drawHorizontalLine(static_cast<int>(minus6Y), eqArea.getX(), eqArea.getRight());

    // Frequency Grid Lines
    float gridFreqs[3] = { 100.0f, 1000.0f, 10000.0f };
    const char* gridLabels[3] = { "100Hz", "1kHz", "10kHz" };

    g.setFont(juce::Font(9.5f));
    for (int i = 0; i < 3; ++i)
    {
        auto p = getEqNodeScreenPos(gridFreqs[i], 0.0f, eqArea);
        g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.20f));
        g.drawVerticalLine(static_cast<int>(p.x), eqArea.getY(), eqArea.getBottom());

        g.setColour(OpenWavLookAndFeel::textSecondary.withAlpha(0.6f));
        g.drawText(gridLabels[i], p.x - 20.0f, eqArea.getBottom() - 16.0f, 40.0f, 14.0f, juce::Justification::centred, false);
    }

    // Composite 9-Band Parametric EQ Transfer Function Curve
    juce::Path eqCurve;
    int points = 120;
    bool isFirst = true;

    for (int i = 0; i <= points; ++i)
    {
        float normX = static_cast<float>(i) / points;
        float f = 20.0f * std::pow(1000.0f, normX);

        float totalGain = 0.0f;
        for (int b = 0; b < 9; ++b)
        {
            totalGain += eqGains[b] * std::exp(-std::pow(std::log(f / eqFreqs[b]), 2.0f) / 0.35f);
        }

        if (lowCutButton.getToggleState())
        {
            totalGain -= 12.0f / (1.0f + std::pow(f / 80.0f, 2.0f));
        }

        auto p = getEqNodeScreenPos(f, totalGain, eqArea);
        if (isFirst)
        {
            eqCurve.startNewSubPath(p);
            isFirst = false;
        }
        else
        {
            eqCurve.lineTo(p);
        }
    }

    juce::Path fillCurve = eqCurve;
    fillCurve.lineTo(eqArea.getRight(), eqArea.getCentreY());
    fillCurve.lineTo(eqArea.getX(), eqArea.getCentreY());
    fillCurve.closeSubPath();

    g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.18f));
    g.fillPath(fillCurve);

    g.setColour(OpenWavLookAndFeel::accentCyan);
    g.strokePath(eqCurve, juce::PathStrokeType(2.0f));

    // Draw 9 Interactive EQ Handles
    for (int i = 0; i < 9; ++i)
    {
        auto p = getEqNodeScreenPos(eqFreqs[i], eqGains[i], eqArea);
        bool isHovered = (hoveredNodeIndex == i);
        bool isDragged = (draggedNodeIndex == i);

        float r = (isHovered || isDragged) ? 10.0f : 8.0f;

        if (isHovered || isDragged)
        {
            g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.35f));
            g.fillEllipse(p.x - r * 1.6f, p.y - r * 1.6f, r * 3.2f, r * 3.2f);
        }

        g.setColour(isDragged ? juce::Colours::white : OpenWavLookAndFeel::bgCard);
        g.fillEllipse(p.x - r, p.y - r, r * 2.0f, r * 2.0f);

        g.setColour(OpenWavLookAndFeel::accentCyan);
        g.drawEllipse(p.x - r, p.y - r, r * 2.0f, r * 2.0f, 1.8f);

        g.setFont(juce::Font(9.5f).boldened());
        g.setColour(isDragged ? OpenWavLookAndFeel::bgDark : OpenWavLookAndFeel::textPrimary);
        g.drawText(juce::String(i + 1), p.x - r, p.y - r, r * 2.0f, r * 2.0f, juce::Justification::centred, false);
    }

    // Active Band Frequency / Gain Readout Pill
    int activeBand = (draggedNodeIndex >= 0) ? draggedNodeIndex : hoveredNodeIndex;
    if (activeBand >= 0 && activeBand < 9)
    {
        float f = eqFreqs[activeBand];
        juce::String fStr = (f >= 1000.0f) ? (juce::String(f / 1000.0f, 2) + " kHz") : (juce::String(static_cast<int>(f)) + " Hz");
        juce::String readout = "Band " + juce::String(activeBand + 1) + ": " + fStr + "  " + (eqGains[activeBand] >= 0.0f ? "+" : "") + juce::String(eqGains[activeBand], 1) + " dB";
        auto pill = juce::Rectangle<float>(eqArea.getRight() - 190.0f, eqArea.getY() + 6.0f, 182.0f, 20.0f);
        g.setColour(OpenWavLookAndFeel::bgHeader.withAlpha(0.9f));
        g.fillRoundedRectangle(pill, 4.0f);
        g.setColour(OpenWavLookAndFeel::accentCyan);
        g.setFont(juce::Font(10.0f).boldened());
        g.drawText(readout, pill, juce::Justification::centred, false);
    }
}

void RecorderComponent::resized()
{
    auto area = getLocalBounds().reduced(14);

    // 1. Top DAW Transport Bar (54px, responsive FlexBox)
    auto transport = area.removeFromTop(54).reduced(10, 9);
    auto timeCol = juce::Rectangle<int>();
    {
        juce::FlexBox fb;
        fb.flexDirection = juce::FlexBox::Direction::row;
        fb.alignItems = juce::FlexBox::AlignItems::center;

        auto addFixed = [&](juce::Component& c, float w, float mr = 6.0f) {
            fb.items.add(juce::FlexItem(c).withWidth(w).withHeight(static_cast<float>(transport.getHeight())).withMargin({ 0.0f, mr, 0.0f, 0.0f }));
        };

        addFixed(recordButton, 36.0f);
        addFixed(pauseButton, 36.0f);
        addFixed(clearButton, 36.0f);
        addFixed(previewButton, 36.0f, 10.0f);

        // Timecode column placeholder
        fb.items.add(juce::FlexItem().withWidth(124.0f).withHeight(static_cast<float>(transport.getHeight())).withMargin({ 0.0f, 8.0f, 0.0f, 0.0f }));

        addFixed(monitorButton, 78.0f);
        addFixed(waitSignalButton, 94.0f);
        addFixed(metronomeButton, 62.0f, 4.0f);
        fb.items.add(juce::FlexItem(bpmSlider).withMinWidth(56.0f).withMaxWidth(100.0f).withFlex(0.4f).withHeight(28.0f).withMargin({ 0.0f, 4.0f, 0.0f, 0.0f }));
        addFixed(bpmLabel, 58.0f, 8.0f);

        fb.items.add(juce::FlexItem().withFlex(1.0f)); // Spacer pushes Count-In neatly right

        addFixed(countInLabel, 56.0f, 4.0f);
        addFixed(countInSelector, 114.0f, 0.0f);

        fb.performLayout(transport);
        timeCol = fb.items[4].currentBounds.toNearestInt();
        timeLabel.setBounds(timeCol.removeFromTop(21));
        formatInfoLabel.setBounds(timeCol);
    }

    area.removeFromTop(10);

    // Skip Main Split Cards area (reserved dynamically, leaving 96px at bottom)
    area.removeFromTop(std::max(160, area.getHeight() - 96));
    area.removeFromTop(10);

    auto deck = area.reduced(10, 6);

    // 2. Bottom Row 1: Input, Gain, Takes, DSP (responsive FlexBox)
    {
        auto row1 = deck.removeFromTop(28);
        juce::FlexBox fb;
        fb.flexDirection = juce::FlexBox::Direction::row;
        fb.alignItems = juce::FlexBox::AlignItems::center;

        auto addItem = [&](juce::Component& c, float w, float mr = 6.0f) {
            fb.items.add(juce::FlexItem(c).withWidth(w).withHeight(26.0f).withMargin({ 0.0f, mr, 0.0f, 0.0f }));
        };

        addItem(audioSetupButton, 32.0f);
        addItem(channelLabel, 40.0f, 2.0f);
        fb.items.add(juce::FlexItem(channelSelector).withMinWidth(105.0f).withMaxWidth(150.0f).withFlex(0.4f).withHeight(26.0f).withMargin({ 0.0f, 8.0f, 0.0f, 0.0f }));
        addItem(inputGainLabel, 82.0f, 2.0f);
        fb.items.add(juce::FlexItem(inputGainSlider).withMinWidth(70.0f).withMaxWidth(120.0f).withFlex(0.4f).withHeight(26.0f).withMargin({ 0.0f, 8.0f, 0.0f, 0.0f }));
        addItem(takeLabel, 40.0f, 2.0f);
        fb.items.add(juce::FlexItem(takeSelector).withMinWidth(95.0f).withMaxWidth(145.0f).withFlex(0.4f).withHeight(26.0f).withMargin({ 0.0f, 8.0f, 0.0f, 0.0f }));

        fb.items.add(juce::FlexItem().withFlex(1.0f));

        addItem(trimButton, 32.0f);
        addItem(lowCutButton, 100.0f, 4.0f);
        addItem(normalizeButton, 110.0f, 4.0f);
        addItem(resetEqButton, 32.0f, 0.0f);

        fb.performLayout(row1);
    }

    deck.removeFromTop(10);

    // 3. Bottom Row 2: Metadata, Save, Send To, Status (responsive FlexBox)
    {
        auto row2 = deck.removeFromTop(28);
        juce::FlexBox fb;
        fb.flexDirection = juce::FlexBox::Direction::row;
        fb.alignItems = juce::FlexBox::AlignItems::center;

        fb.items.add(juce::FlexItem(nameLabel).withWidth(44.0f).withHeight(26.0f).withMargin({ 0.0f, 4.0f, 0.0f, 0.0f }));
        fb.items.add(juce::FlexItem(nameEditor).withMinWidth(100.0f).withMaxWidth(210.0f).withFlex(0.5f).withHeight(26.0f).withMargin({ 0.0f, 8.0f, 0.0f, 0.0f }));
        fb.items.add(juce::FlexItem(tagsLabel).withWidth(38.0f).withHeight(26.0f).withMargin({ 0.0f, 4.0f, 0.0f, 0.0f }));
        fb.items.add(juce::FlexItem(tagsEditor).withMinWidth(100.0f).withMaxWidth(200.0f).withFlex(0.5f).withHeight(26.0f).withMargin({ 0.0f, 8.0f, 0.0f, 0.0f }));
        fb.items.add(juce::FlexItem(saveButton).withWidth(32.0f).withHeight(26.0f).withMargin({ 0.0f, 6.0f, 0.0f, 0.0f }));
        fb.items.add(juce::FlexItem(toEditorButton).withWidth(32.0f).withHeight(26.0f).withMargin({ 0.0f, 6.0f, 0.0f, 0.0f }));
        fb.items.add(juce::FlexItem(toSamplerButton).withWidth(32.0f).withHeight(26.0f).withMargin({ 0.0f, 10.0f, 0.0f, 0.0f }));
        fb.items.add(juce::FlexItem(statusLabel).withMinWidth(120.0f).withFlex(1.0f).withHeight(26.0f));

        fb.performLayout(row2);
    }
}

void RecorderComponent::lookAndFeelChanged()
{
    recordButton.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::favoriteRed.withAlpha(0.22f));
    recordButton.setColour(juce::TextButton::buttonOnColourId, OpenWavLookAndFeel::favoriteRed);
    recordButton.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::favoriteRed);
    recordButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);

    pauseButton.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::bgCard);
    pauseButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::orange);
    pauseButton.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::textPrimary);
    pauseButton.setColour(juce::TextButton::textColourOnId, juce::Colours::black);

    clearButton.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::bgCard);
    clearButton.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::textSecondary);

    trimButton.setColour(juce::TextButton::buttonColourId, juce::Colours::orange.withAlpha(0.18f));
    trimButton.setColour(juce::TextButton::textColourOffId, juce::Colours::orange);

    saveButton.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::accentCyan.withAlpha(0.22f));
    saveButton.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::accentCyan);

    toEditorButton.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::bgCard);
    toEditorButton.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::accentCyan);

    toSamplerButton.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::bgCard);
    toSamplerButton.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::accentCyan);

    audioSetupButton.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::bgCard);
    audioSetupButton.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::textPrimary);

    previewButton.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::bgCard);
    previewButton.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::textPrimary);

    resetEqButton.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::bgCard);
    resetEqButton.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::textSecondary);
}

void RecorderComponent::visibilityChanged()
{
    updateInputState();
}

void RecorderComponent::updateInputState()
{
    bool isRecordViewActive = isVisible();
    audioEngine.setInputListeningEnabled(isRecordViewActive);
    bool shouldMonitor = isRecordViewActive && monitorButton.getToggleState();
    audioEngine.setInputMuted(!shouldMonitor);
}

void RecorderComponent::storeTakeSnapshot()
{
    juce::AudioBuffer<float> buf;
    double sr = 44100.0;
    if (!audioEngine.getRecordedBufferCopy(buf, sr) || buf.getNumSamples() <= 0)
        return;

    RecordedTake take;
    take.name = "Take " + juce::String(static_cast<int>(takes.size()) + 1)
        + " (" + juce::String(static_cast<double>(buf.getNumSamples()) / std::max(1.0, sr), 1) + "s)";
    take.buffer.makeCopyOf(buf);
    take.sampleRate = sr;

    if (takes.size() >= 8)
        takes.erase(takes.begin());
    takes.push_back(std::move(take));
    activeTakeIndex = static_cast<int>(takes.size()) - 1;

    takeSelector.clear(juce::dontSendNotification);
    for (size_t i = 0; i < takes.size(); ++i)
        takeSelector.addItem(takes[i].name, static_cast<int>(i) + 1);
    takeSelector.setSelectedId(activeTakeIndex + 1, juce::dontSendNotification);
}

void RecorderComponent::loadTakeIntoView(int takeIndex)
{
    if (audioEngine.isRecording() || takeIndex < 0 || takeIndex >= static_cast<int>(takes.size()))
        return;

    activeTakeIndex = takeIndex;
    trimStartRatio = 0.0;
    trimEndRatio = 1.0;
    hasRecordedBuffer = (takes[static_cast<size_t>(takeIndex)].buffer.getNumSamples() > 0);
    saveButton.setEnabled(hasRecordedBuffer);
    toEditorButton.setEnabled(hasRecordedBuffer);
    toSamplerButton.setEnabled(hasRecordedBuffer);
    previewButton.setEnabled(hasRecordedBuffer);
    trimButton.setEnabled(hasRecordedBuffer);
    clearButton.setEnabled(hasRecordedBuffer);
    if (hasRecordedBuffer)
        syncGlobalWaveformToCurrentTake();
    repaint();
}

void RecorderComponent::timerCallback()
{
    updateInputState();
    float rawL = 0.0f, rawR = 0.0f;
    audioEngine.getLiveInputLevels(rawL, rawR);

    if (rawL >= 0.99f) clipLeft = true;
    if (rawR >= 0.99f) clipRight = true;

    // Convert linear peak amplitude to responsive dB meter scale (-50dB to 0dB -> 0.0 to 1.0)
    float dbL = (rawL > 0.001f) ? juce::Decibels::gainToDecibels(rawL, -50.0f) : -50.0f;
    float dbR = (rawR > 0.001f) ? juce::Decibels::gainToDecibels(rawR, -50.0f) : -50.0f;

    float normL = juce::jlimit(0.0f, 1.0f, (dbL + 50.0f) / 50.0f);
    float normR = juce::jlimit(0.0f, 1.0f, (dbR + 50.0f) / 50.0f);

    if (normL > smoothLeftLevel)
        smoothLeftLevel = normL;
    else
        smoothLeftLevel += (normL - smoothLeftLevel) * 0.25f;

    if (normR > smoothRightLevel)
        smoothRightLevel = normR;
    else
        smoothRightLevel += (normR - smoothRightLevel) * 0.25f;

    uint32_t nowMs = juce::Time::getMillisecondCounter();
    if (normL >= peakHoldLeft || nowMs - peakHoldTimeL > 1500)
    {
        peakHoldLeft = normL;
        peakHoldTimeL = nowMs;
    }
    if (normR >= peakHoldRight || nowMs - peakHoldTimeR > 1500)
    {
        peakHoldRight = normR;
        peakHoldTimeR = nowMs;
    }

    // Threshold / Wait-Signal Auto-Start (-36 dB = ~0.0158 linear)
    if (isArmedWaitSignal && !audioEngine.isRecording())
    {
        if (std::max(rawL, rawR) >= 0.016f)
        {
            isArmedWaitSignal = false;
            isPausedRecording = false;
            audioEngine.startRecording(true);
            recordButton.setButtonText("STOP");
            recordButton.setToggleState(true, juce::dontSendNotification);
            pauseButton.setEnabled(true);
            statusLabel.setText("Signal detected — RECORDING!", juce::dontSendNotification);
            statusLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::favoriteRed);
        }
    }

    const uint32_t beatIntervalMs = static_cast<uint32_t>(std::max(250.0, 60000.0 / bpmSlider.getValue()));

    if (isCountingDown)
    {
        uint32_t interval = (countInSelector.getSelectedId() == 3) ? 1000u : beatIntervalMs;
        if (nowMs - lastBeatMs >= interval)
        {
            lastBeatMs = nowMs;
            countdownValue--;
            flashAlpha = 1.0f;

            if (countdownValue > 0)
            {
                audioEngine.playMetronomeClick(false);
            }
            else
            {
                isCountingDown = false;
                isPausedRecording = false;
                beatCounter = 0;
                audioEngine.playMetronomeClick(true);
                audioEngine.startRecording(true);
                recordButton.setButtonText("STOP");
                recordButton.setToggleState(true, juce::dontSendNotification);
                pauseButton.setEnabled(true);
            }
        }
    }

    if (audioEngine.isRecording())
    {
        if (metronomeButton.getToggleState() && (nowMs - lastBeatMs >= beatIntervalMs))
        {
            lastBeatMs = nowMs;
            beatCounter = (beatCounter + 1) % 4;
            audioEngine.playMetronomeClick(beatCounter == 0);
            flashAlpha = (beatCounter == 0) ? 0.55f : 0.25f;
        }

        double durSecs = audioEngine.getRecordingDurationSeconds();
        int mins = static_cast<int>(durSecs) / 60;
        int secs = static_cast<int>(durSecs) % 60;
        int tenths = static_cast<int>((durSecs - static_cast<int>(durSecs)) * 10.0);

        timeLabel.setText(juce::String::formatted("%02d:%02d.%d", mins, secs, tenths), juce::dontSendNotification);
        statusLabel.setText("RECORDING LIVE AUDIO...", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::favoriteRed);
        hasRecordedBuffer = true;
    }

    repaint();
}

void RecorderComponent::beginCountdownOrRecord()
{
    hasRecordedBuffer = false;
    if (lastPreviewTempFile.existsAsFile())
        lastPreviewTempFile.deleteFile();
    lastPreviewTempFile = juce::File();

    isPausedRecording = false;
    trimStartRatio = 0.0;
    trimEndRatio = 1.0;
    saveButton.setEnabled(false);
    toEditorButton.setEnabled(false);
    toSamplerButton.setEnabled(false);
    previewButton.setEnabled(false);
    trimButton.setEnabled(false);
    clearButton.setEnabled(false);
    pauseButton.setEnabled(false);
    pauseButton.setToggleState(false, juce::dontSendNotification);
    pauseButton.setButtonText("PAUSE");

    nameEditor.setText("Rec_" + juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S"));

    if (waitSignalButton.getToggleState())
    {
        isArmedWaitSignal = true;
        recordButton.setButtonText("CANCEL");
        recordButton.setToggleState(true, juce::dontSendNotification);
        statusLabel.setText("Armed: waiting for input signal above -36 dB...", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
        return;
    }

    int countId = countInSelector.getSelectedId();
    if (countId == 2 || countId == 3)
    {
        isCountingDown = true;
        countdownValue = (countId == 2) ? 4 : 3;
        lastBeatMs = juce::Time::getMillisecondCounter();
        flashAlpha = 1.0f;
        audioEngine.playMetronomeClick(true);
        recordButton.setButtonText("CANCEL");
        recordButton.setToggleState(true, juce::dontSendNotification);
        statusLabel.setText("Count-in...", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::favoriteRed);
    }
    else
    {
        lastBeatMs = juce::Time::getMillisecondCounter();
        beatCounter = 0;
        if (metronomeButton.getToggleState())
            audioEngine.playMetronomeClick(true);
        audioEngine.startRecording(true);
        recordButton.setButtonText("STOP");
        recordButton.setToggleState(true, juce::dontSendNotification);
        pauseButton.setEnabled(true);
    }
}

void RecorderComponent::toggleRecording()
{
    if (isCountingDown || isArmedWaitSignal)
    {
        isCountingDown = false;
        isArmedWaitSignal = false;
        recordButton.setButtonText("REC");
        recordButton.setToggleState(false, juce::dontSendNotification);
        statusLabel.setText("Recording cancelled.", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
        return;
    }

    if (audioEngine.isRecording() || isPausedRecording)
    {
        audioEngine.stopRecording();
        isPausedRecording = false;
        recordButton.setButtonText("REC");
        recordButton.setToggleState(false, juce::dontSendNotification);
        pauseButton.setEnabled(false);
        pauseButton.setToggleState(false, juce::dontSendNotification);
        pauseButton.setButtonText("PAUSE");

        if (hasRecordedBuffer)
        {
            storeTakeSnapshot();
            syncGlobalWaveformToCurrentTake();
        }

        saveButton.setEnabled(hasRecordedBuffer);
        toEditorButton.setEnabled(hasRecordedBuffer);
        toSamplerButton.setEnabled(hasRecordedBuffer);
        previewButton.setEnabled(hasRecordedBuffer);
        trimButton.setEnabled(hasRecordedBuffer);
        clearButton.setEnabled(hasRecordedBuffer);

        double durSecs = audioEngine.getRecordingDurationSeconds();
        statusLabel.setText("Recorded " + juce::String(durSecs, 1) + "s. Drag S/E handles to trim or Save to Library.", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    }
    else
    {
        beginCountdownOrRecord();
    }
    updateInputState();
}

void RecorderComponent::togglePauseRecording()
{
    if (audioEngine.isRecording())
    {
        audioEngine.pauseRecording();
        isPausedRecording = true;
        pauseButton.setButtonText("RESUME");
        pauseButton.setToggleState(true, juce::dontSendNotification);
        statusLabel.setText("Recording PAUSED. Click RESUME to punch-in or STOP to finish.", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
    }
    else if (isPausedRecording)
    {
        isPausedRecording = false;
        lastBeatMs = juce::Time::getMillisecondCounter();
        audioEngine.startRecording(false); // Punch-in at current write position
        pauseButton.setButtonText("PAUSE");
        pauseButton.setToggleState(false, juce::dontSendNotification);
        statusLabel.setText("RECORDING LIVE AUDIO...", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::favoriteRed);
    }
}

void RecorderComponent::clearCurrentTake()
{
    if (audioEngine.isRecording())
        audioEngine.stopRecording();

    audioEngine.clearRecording();
    audioEngine.clearMasterSample();
    if (activeTakeIndex >= 0 && activeTakeIndex < static_cast<int>(takes.size()))
    {
        takes.erase(takes.begin() + activeTakeIndex);
    }
    activeTakeIndex = takes.empty() ? -1 : static_cast<int>(takes.size()) - 1;
    takeSelector.clear(juce::dontSendNotification);
    for (size_t i = 0; i < takes.size(); ++i)
        takeSelector.addItem(takes[i].name, static_cast<int>(i) + 1);
    if (activeTakeIndex >= 0)
        takeSelector.setSelectedId(activeTakeIndex + 1, juce::dontSendNotification);

    if (lastPreviewTempFile.existsAsFile())
        lastPreviewTempFile.deleteFile();
    lastPreviewTempFile = juce::File();

    isPausedRecording = false;
    hasRecordedBuffer = (activeTakeIndex >= 0);
    trimStartRatio = 0.0;
    trimEndRatio = 1.0;
    timeLabel.setText("00:00.0", juce::dontSendNotification);
    recordButton.setButtonText("REC");
    recordButton.setToggleState(false, juce::dontSendNotification);
    pauseButton.setEnabled(false);
    pauseButton.setToggleState(false, juce::dontSendNotification);
    pauseButton.setButtonText("PAUSE");
    saveButton.setEnabled(hasRecordedBuffer);
    toEditorButton.setEnabled(hasRecordedBuffer);
    toSamplerButton.setEnabled(hasRecordedBuffer);
    previewButton.setEnabled(hasRecordedBuffer);
    trimButton.setEnabled(hasRecordedBuffer);
    clearButton.setEnabled(hasRecordedBuffer);
    statusLabel.setText(hasRecordedBuffer ? "Take removed; switched to previous take." : "Take cleared. Ready to record.", juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    repaint();
}

void RecorderComponent::trimCurrentTake()
{
    if (audioEngine.isRecording())
        return;

    if (audioEngine.trimRecording(trimStartRatio, trimEndRatio))
    {
        trimStartRatio = 0.0;
        trimEndRatio = 1.0;
        double durSecs = audioEngine.getRecordingDurationSeconds();
        int mins = static_cast<int>(durSecs) / 60;
        int secs = static_cast<int>(durSecs) % 60;
        int tenths = static_cast<int>((durSecs - static_cast<int>(durSecs)) * 10.0);
        timeLabel.setText(juce::String::formatted("%02d:%02d.%d", mins, secs, tenths), juce::dontSendNotification);
        statusLabel.setText("Trimmed take to " + juce::String(durSecs, 1) + "s.", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
        syncGlobalWaveformToCurrentTake();
        repaint();
    }
}

void RecorderComponent::syncGlobalWaveformToCurrentTake()
{
    juce::AudioBuffer<float> copyBuf;
    double sr = 44100.0;
    if (!audioEngine.getRecordedBufferCopy(copyBuf, sr) || copyBuf.getNumSamples() == 0)
    {
        if (activeTakeIndex >= 0 && activeTakeIndex < static_cast<int>(takes.size()))
        {
            copyBuf.makeCopyOf(takes[static_cast<size_t>(activeTakeIndex)].buffer);
            sr = takes[static_cast<size_t>(activeTakeIndex)].sampleRate;
        }
    }
    if (copyBuf.getNumSamples() == 0)
        return;

    if (lastPreviewTempFile.existsAsFile())
        lastPreviewTempFile.deleteFile();
    lastPreviewTempFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("OWMB_rec_preview_" + juce::String(++previewTempCounter) + ".wav");

    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatWriter> writer(
        format.createWriterFor(new juce::FileOutputStream(lastPreviewTempFile), sr, copyBuf.getNumChannels(), 16, {}, 0)
    );
    if (writer != nullptr)
    {
        writer->writeFromAudioSampleBuffer(copyBuf, 0, copyBuf.getNumSamples());
        writer.reset();
        audioEngine.loadFile(lastPreviewTempFile, false);
    }
}

void RecorderComponent::playPreview()
{
    juce::AudioBuffer<float> copyBuf;
    double sr = 44100.0;
    if (!audioEngine.getRecordedBufferCopy(copyBuf, sr) || copyBuf.getNumSamples() == 0)
    {
        if (activeTakeIndex >= 0 && activeTakeIndex < static_cast<int>(takes.size()))
        {
            copyBuf.makeCopyOf(takes[static_cast<size_t>(activeTakeIndex)].buffer);
            sr = takes[static_cast<size_t>(activeTakeIndex)].sampleRate;
        }
    }
    if (copyBuf.getNumSamples() == 0)
        return;

    int total = copyBuf.getNumSamples();
    int sIdx = juce::jlimit(0, total - 1, static_cast<int>(std::round(trimStartRatio * total)));
    int eIdx = juce::jlimit(sIdx + 1, total, static_cast<int>(std::round(trimEndRatio * total)));
    int len = eIdx - sIdx;
    if (len <= 0) return;

    if (lastPreviewTempFile.existsAsFile())
        lastPreviewTempFile.deleteFile();
    lastPreviewTempFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("OWMB_rec_preview_" + juce::String(++previewTempCounter) + ".wav");

    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatWriter> writer(
        format.createWriterFor(new juce::FileOutputStream(lastPreviewTempFile), sr, copyBuf.getNumChannels(), 16, {}, 0)
    );

    if (writer != nullptr)
    {
        writer->writeFromAudioSampleBuffer(copyBuf, sIdx, len);
        writer.reset();
        audioEngine.loadFile(lastPreviewTempFile, true);
    }
}

void RecorderComponent::saveAndAddToLibrary()
{
    MediaItem item;
    if (saveCurrentTakeToDiskAndDb(item, true))
        ToastOverlay::show("Saved \"" + item.fileName + "\" to Library");
}

bool RecorderComponent::saveCurrentTakeToDiskAndDb(MediaItem& outItem, bool autoPreview)
{
    juce::String sampleName = nameEditor.getText().trim();
    if (sampleName.isEmpty())
        sampleName = "Rec_" + juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S");

    // Apply any pending trim before saving
    if (trimStartRatio > 0.001 || trimEndRatio < 0.999)
    {
        audioEngine.trimRecording(trimStartRatio, trimEndRatio);
        trimStartRatio = 0.0;
        trimEndRatio = 1.0;
    }

    bool applyLowCut = lowCutButton.getToggleState();
    bool applyNormalize = normalizeButton.getToggleState();

    juce::AudioBuffer<float> buf;
    double sr = 44100.0;
    if (!audioEngine.getRecordedBufferCopy(buf, sr) || buf.getNumSamples() == 0)
    {
        if (activeTakeIndex >= 0 && activeTakeIndex < static_cast<int>(takes.size()))
        {
            buf.makeCopyOf(takes[static_cast<size_t>(activeTakeIndex)].buffer);
            sr = takes[static_cast<size_t>(activeTakeIndex)].sampleRate;
        }
    }

    if (buf.getNumSamples() == 0)
        return false;

    if (sr > 0.0 && applyNormalize)
    {
        float maxPeak = buf.getMagnitude(0, buf.getNumSamples());
        if (maxPeak > 0.0001f)
            buf.applyGain(0.98f / maxPeak);
    }

    juce::File savedFile = audioEngine.saveRecordingToWav(sampleName);
    if (!savedFile.existsAsFile())
    {
        statusLabel.setText("Error saving recording file to disk.", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::favoriteRed);
        return false;
    }

    {
        juce::WavAudioFormat format;
        std::unique_ptr<juce::AudioFormatWriter> writer(
            format.createWriterFor(new juce::FileOutputStream(savedFile), sr, buf.getNumChannels(), 24, {}, 0)
        );
        if (writer != nullptr)
        {
            writer->writeFromAudioSampleBuffer(buf, 0, buf.getNumSamples());
            writer.reset();
        }
    }

    lastSavedFile = savedFile;

    MediaItem item;
    item.id = juce::String(savedFile.getFullPathName().hashCode64());
    item.filePath = savedFile.getFullPathName();
    item.fileName = savedFile.getFileName();
    item.fileExtension = savedFile.getFileExtension().toLowerCase();
    item.fileSizeBytes = savedFile.getSize();
    item.dateAddedMs = juce::Time::getCurrentTime().toMilliseconds();

    if (sr > 0.0)
    {
        item.durationSeconds = static_cast<double>(buf.getNumSamples()) / sr;
        item.sampleRate = sr;
        item.numChannels = buf.getNumChannels();
        item.bitDepth = 24;
    }

    juce::StringArray tagList;
    tagList.addTokens(tagsEditor.getText(), ",", "\"'");
    for (const auto& t : tagList)
    {
        juce::String cleanTag = t.trim();
        if (cleanTag.isNotEmpty())
            item.tags.insert(cleanTag);
    }
    if (applyLowCut) item.tags.insert("#LowCut");
    if (applyNormalize) item.tags.insert("#Normalized");
    if (item.tags.empty()) item.tags.insert("Recorded");

    item.precomputeCachedStrings();

    dbManager.addOrUpdateItem(item);
    dbManager.saveToFile();

    if (autoPreview)
        audioEngine.loadFile(savedFile, true);

    statusLabel.setText("Saved & added to library: " + savedFile.getFileName(), juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);

    saveButton.setEnabled(false);
    outItem = item;
    if (onRecordingSaved)
        onRecordingSaved(item);
    return true;
}

} // namespace openwav
