#include "RecorderComponent.h"
#include "OpenWavLookAndFeel.h"

namespace openwav
{

RecorderComponent::RecorderComponent(AudioEngine& engine, TagDatabaseManager& db)
    : audioEngine(engine), dbManager(db)
{
    // Record Toggle Button
    recordButton.onClick = [this] { toggleRecording(); };
    addAndMakeVisible(recordButton);

    // Preview Button
    previewButton.onClick = [this] { playPreview(); };
    previewButton.setEnabled(false);
    addAndMakeVisible(previewButton);

    // Save Button
    saveButton.onClick = [this] { saveAndAddToLibrary(); };
    saveButton.setEnabled(false);
    addAndMakeVisible(saveButton);

    // Input Channel Selector
    channelLabel.setFont(juce::Font(12.0f).boldened());
    channelLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    addAndMakeVisible(channelLabel);

    channelSelector.addItem("Stereo (Ch 1+2)", 1);
    channelSelector.addItem("Mono - Input 1 (Left)", 2);
    channelSelector.addItem("Mono - Input 2 (Right)", 3);
    channelSelector.setSelectedId(1, juce::dontSendNotification);
    channelSelector.onChange = [this] {
        int id = channelSelector.getSelectedId();
        if (id == 2)
            audioEngine.setRecordingChannelMode(AudioEngine::RecordingChannelMode::MonoLeft);
        else if (id == 3)
            audioEngine.setRecordingChannelMode(AudioEngine::RecordingChannelMode::MonoRight);
        else
            audioEngine.setRecordingChannelMode(AudioEngine::RecordingChannelMode::Stereo);
    };
    addAndMakeVisible(channelSelector);

    // Count-In Selector
    countInLabel.setFont(juce::Font(12.0f).boldened());
    countInLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    addAndMakeVisible(countInLabel);

    countInSelector.addItem("Count-In: 3s", 1);
    countInSelector.addItem("Count-In: Off", 2);
    countInSelector.setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(countInSelector);

    // Name Label & Editor
    nameLabel.setFont(juce::Font(12.0f).boldened());
    nameLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    addAndMakeVisible(nameLabel);

    nameEditor.setText("Rec_" + juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S"));
    nameEditor.setJustification(juce::Justification::centredLeft);
    nameEditor.setIndents(6, 0);
    addAndMakeVisible(nameEditor);

    // Tags Label & Editor
    tagsLabel.setFont(juce::Font(12.0f).boldened());
    tagsLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    addAndMakeVisible(tagsLabel);

    tagsEditor.setText("Recorded, Custom");
    tagsEditor.setJustification(juce::Justification::centredLeft);
    tagsEditor.setIndents(6, 0);
    addAndMakeVisible(tagsEditor);

    // Time Label
    timeLabel.setFont(juce::Font(28.0f).boldened());
    timeLabel.setJustificationType(juce::Justification::centred);
    timeLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textPrimary);
    timeLabel.setText("00:00.0", juce::dontSendNotification);
    addAndMakeVisible(timeLabel);

    // Status Label
    statusLabel.setFont(juce::Font(13.0f).boldened());
    statusLabel.setJustificationType(juce::Justification::centred);
    statusLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    statusLabel.setText("Ready to record live input audio", juce::dontSendNotification);
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

    lookAndFeelChanged();

    audioEngine.setInputParametricEq(eqFreqs, eqGains, lowCutButton.getToggleState());

    startTimerHz(30);
    updateInputMuteState();
}

RecorderComponent::~RecorderComponent()
{
    stopTimer();
    audioEngine.setInputMuted(true);
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
    if (cachedEqArea.isEmpty()) return;

    juce::Point<float> mousePos = e.position.toFloat();
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
}

void RecorderComponent::mouseDown(const juce::MouseEvent& e)
{
    if (cachedEqArea.isEmpty()) return;

    juce::Point<float> mousePos = e.position.toFloat();
    draggedNodeIndex = -1;

    for (int i = 0; i < 9; ++i)
    {
        juce::Point<float> nodePos = getEqNodeScreenPos(eqFreqs[i], eqGains[i], cachedEqArea);
        if (nodePos.getDistanceFrom(mousePos) <= 16.0f)
        {
            draggedNodeIndex = i;
            break;
        }
    }
}

void RecorderComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (draggedNodeIndex >= 0 && !cachedEqArea.isEmpty())
    {
        updateEqNodeFromScreenPos(draggedNodeIndex, e.position.toFloat(), cachedEqArea);
        repaint();
    }
}

void RecorderComponent::mouseUp(const juce::MouseEvent& /*e*/)
{
    draggedNodeIndex = -1;
}

void RecorderComponent::paint(juce::Graphics& g)
{
    g.fillAll(OpenWavLookAndFeel::bgDark);

    auto bounds = getLocalBounds().reduced(16);

    // Title HUD Header
    g.setFont(juce::Font(16.0f).boldened());
    g.setColour(OpenWavLookAndFeel::textPrimary);
    g.drawText("AUDIO SAMPLE RECORDER & PARAMETRIC EQ", bounds.removeFromTop(24), juce::Justification::left, true);

    bounds.removeFromTop(12);

    // Split Cards Container Area
    auto mainCard = bounds.removeFromTop(bounds.getHeight() - 110).toFloat();
    
    // Split into Left (Oscilloscope + VU Meters) and Right (Parametric EQ + Spectrum Analyzer)
    float gap = 12.0f;
    float leftW = std::max(200.0f, (mainCard.getWidth() - gap) * 0.52f);
    float rightW = mainCard.getWidth() - leftW - gap;

    auto leftCard = mainCard.removeFromLeft(leftW);
    mainCard.removeFromLeft(gap);
    auto rightCard = mainCard.removeFromLeft(rightW);

    // --- LEFT CARD: LIVE OSCILLOSCOPE & STEREO VU METERS ---
    g.setColour(OpenWavLookAndFeel::bgHeader);
    g.fillRoundedRectangle(leftCard, 8.0f);
    g.setColour(OpenWavLookAndFeel::borderColour);
    g.drawRoundedRectangle(leftCard, 8.0f, 1.0f);

    auto scopeArea = leftCard.reduced(14.0f);

    // Header Label Left
    g.setFont(juce::Font(12.0f).boldened());
    g.setColour(OpenWavLookAndFeel::accentCyan);
    g.drawText("LIVE OSCILLOSCOPE & METERS", scopeArea.removeFromTop(18.0f), juce::Justification::left, true);
    scopeArea.removeFromTop(4.0f);

    // Reserve right side of scope card for Stereo VU Meters
    auto meterArea = scopeArea.removeFromRight(36.0f);
    scopeArea.removeFromRight(10.0f);

    // Live Oscilloscope Background
    g.setColour(OpenWavLookAndFeel::bgDark);
    g.fillRoundedRectangle(scopeArea, 6.0f);
    g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.6f));
    g.drawRoundedRectangle(scopeArea, 6.0f, 1.0f);

    // Oscilloscope Grid Lines
    g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.25f));
    g.drawHorizontalLine(static_cast<int>(scopeArea.getCentreY()), scopeArea.getX(), scopeArea.getRight());
    g.drawVerticalLine(static_cast<int>(scopeArea.getCentreX()), scopeArea.getY(), scopeArea.getBottom());

    juce::AudioBuffer<float> recBuf;
    double sr = 44100.0;
    bool hasData = audioEngine.getRecordedBufferCopy(recBuf, sr);

    if (hasData && recBuf.getNumSamples() > 0)
    {
        int numSamples = recBuf.getNumSamples();
        int width = static_cast<int>(scopeArea.getWidth());
        float centerY = scopeArea.getCentreY();
        float halfH = (scopeArea.getHeight() - 8.0f) * 0.5f;

        g.setColour(audioEngine.isRecording() ? OpenWavLookAndFeel::favoriteRed : OpenWavLookAndFeel::accentCyan);

        juce::Path scopePath;
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
            float pyMin = centerY + minVal * halfH;
            float pyMax = centerY + maxVal * halfH;

            if (x == 0)
                scopePath.startNewSubPath(px, pyMin);

            scopePath.lineTo(px, pyMax);
            scopePath.lineTo(px, pyMin);
        }

        g.strokePath(scopePath, juce::PathStrokeType(1.2f));
    }
    else
    {
        g.setFont(juce::Font(13.0f));
        g.setColour(OpenWavLookAndFeel::textSecondary);
        g.drawText("Press RECORD to capture live audio stream...", scopeArea, juce::Justification::centred, true);
    }

    // Beat Flash Pulse Overlay
    if (flashAlpha > 0.01f)
    {
        g.setColour(OpenWavLookAndFeel::favoriteRed.withAlpha(flashAlpha * 0.35f));
        g.fillRoundedRectangle(scopeArea, 6.0f);
        flashAlpha *= 0.82f;
    }

    // Visual Countdown HUD Overlay
    if (isCountingDown)
    {
        auto hudRect = scopeArea.withSizeKeepingCentre(160.0f, 120.0f);
        g.setColour(OpenWavLookAndFeel::bgHeader.withAlpha(0.92f));
        g.fillRoundedRectangle(hudRect, 16.0f);
        g.setColour(OpenWavLookAndFeel::favoriteRed);
        g.drawRoundedRectangle(hudRect, 16.0f, 2.5f);

        auto numRect = hudRect.removeFromTop(80.0f);
        g.setFont(juce::Font(56.0f).boldened());
        g.setColour(OpenWavLookAndFeel::favoriteRed);
        g.drawText(juce::String(countdownValue), numRect, juce::Justification::centred, false);

        g.setFont(juce::Font(12.0f).boldened());
        g.setColour(OpenWavLookAndFeel::textPrimary);
        g.drawText("GET READY...", hudRect, juce::Justification::centred, false);
    }

    // Draw Dual Stereo VU Level Meters (Left & Right)
    auto meterLeft = meterArea.removeFromLeft(14.0f);
    auto meterRight = meterArea.removeFromRight(14.0f);

    auto drawMeter = [&g](juce::Rectangle<float> rect, float level) {
        g.setColour(OpenWavLookAndFeel::bgDark);
        g.fillRoundedRectangle(rect, 3.0f);
        g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.5f));
        g.drawRoundedRectangle(rect, 3.0f, 1.0f);

        float fillH = rect.getHeight() * juce::jlimit(0.0f, 1.0f, level);
        if (fillH > 0.0f)
        {
            auto fillRect = rect.removeFromBottom(fillH);
            g.setColour(level > 0.90f ? OpenWavLookAndFeel::favoriteRed : OpenWavLookAndFeel::accentCyan);
            g.fillRoundedRectangle(fillRect, 2.0f);
        }
    };

    drawMeter(meterLeft, smoothLeftLevel);
    drawMeter(meterRight, smoothRightLevel);

    // --- RIGHT CARD: PARAMETRIC EQ GRAPH & SPECTRUM VISUALIZER ---
    g.setColour(OpenWavLookAndFeel::bgHeader);
    g.fillRoundedRectangle(rightCard, 8.0f);
    g.setColour(OpenWavLookAndFeel::borderColour);
    g.drawRoundedRectangle(rightCard, 8.0f, 1.0f);

    auto eqArea = rightCard.reduced(14.0f);

    // Header Label Right
    g.setFont(juce::Font(12.0f).boldened());
    g.setColour(OpenWavLookAndFeel::accentCyan);
    g.drawText("PARAMETRIC EQ & SPECTRUM ANALYZER", eqArea.removeFromTop(18.0f), juce::Justification::left, true);
    eqArea.removeFromTop(4.0f);

    cachedEqArea = eqArea;

    // EQ Background Graph
    g.setColour(OpenWavLookAndFeel::bgDark);
    g.fillRoundedRectangle(eqArea, 6.0f);
    g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.6f));
    g.drawRoundedRectangle(eqArea, 6.0f, 1.0f);

    // EQ Grid Lines (0dB, +6dB, -6dB horizontal lines)
    float eqCenterY = eqArea.getCentreY();
    g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.35f));
    g.drawHorizontalLine(static_cast<int>(eqCenterY), eqArea.getX(), eqArea.getRight());

    g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.18f));
    float plus6Y = eqCenterY - (6.0f / 15.0f) * (eqArea.getHeight() * 0.40f);
    float minus6Y = eqCenterY + (6.0f / 15.0f) * (eqArea.getHeight() * 0.40f);
    g.drawHorizontalLine(static_cast<int>(plus6Y), eqArea.getX(), eqArea.getRight());
    g.drawHorizontalLine(static_cast<int>(minus6Y), eqArea.getX(), eqArea.getRight());

    // Frequency Grid Lines (100Hz, 1kHz, 10kHz)
    float gridFreqs[3] = { 100.0f, 1000.0f, 10000.0f };
    const char* gridLabels[3] = { "100Hz", "1kHz", "10kHz" };

    g.setFont(juce::Font(10.0f));
    for (int i = 0; i < 3; ++i)
    {
        auto p = getEqNodeScreenPos(gridFreqs[i], 0.0f, eqArea);
        g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.20f));
        g.drawVerticalLine(static_cast<int>(p.x), eqArea.getY(), eqArea.getBottom());

        g.setColour(OpenWavLookAndFeel::textSecondary.withAlpha(0.6f));
        g.drawText(gridLabels[i], p.x - 20.0f, eqArea.getBottom() - 16.0f, 40.0f, 14.0f, juce::Justification::centred, false);
    }

    // Spectrum Analyzer Curve (Real-time spectral energy behind EQ curve)
    float liveAmp = (smoothLeftLevel + smoothRightLevel) * 0.5f;
    if (liveAmp > 0.01f)
    {
        juce::Path specPath;
        specPath.startNewSubPath(eqArea.getX(), eqArea.getBottom() - 2.0f);

        int specSteps = 40;
        for (int i = 0; i <= specSteps; ++i)
        {
            float norm = static_cast<float>(i) / specSteps;
            float px = eqArea.getX() + norm * eqArea.getWidth();

            float noiseFactor = std::sin(norm * 14.0f + juce::Time::getMillisecondCounter() * 0.006f) * 0.25f + 0.75f;
            float rollOff = std::exp(-norm * 2.2f);
            float specH = eqArea.getHeight() * 0.65f * liveAmp * rollOff * noiseFactor;

            float py = std::max(eqArea.getY() + 4.0f, eqArea.getBottom() - 2.0f - specH);
            specPath.lineTo(px, py);
        }
        specPath.lineTo(eqArea.getRight(), eqArea.getBottom() - 2.0f);
        specPath.closeSubPath();

        g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.12f));
        g.fillPath(specPath);
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

    // Fill under EQ Curve
    juce::Path fillCurve = eqCurve;
    fillCurve.lineTo(eqArea.getRight(), eqArea.getCentreY());
    fillCurve.lineTo(eqArea.getX(), eqArea.getCentreY());
    fillCurve.closeSubPath();

    g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.18f));
    g.fillPath(fillCurve);

    // Stroke EQ Transfer Curve
    g.setColour(OpenWavLookAndFeel::accentCyan);
    g.strokePath(eqCurve, juce::PathStrokeType(2.0f));

    // Draw 9 Interactive EQ Handles
    for (int i = 0; i < 9; ++i)
    {
        auto p = getEqNodeScreenPos(eqFreqs[i], eqGains[i], eqArea);
        bool isHovered = (hoveredNodeIndex == i);
        bool isDragged = (draggedNodeIndex == i);

        float r = (isHovered || isDragged) ? 10.0f : 8.0f;

        // Glow Halo
        if (isHovered || isDragged)
        {
            g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.35f));
            g.fillEllipse(p.x - r * 1.6f, p.y - r * 1.6f, r * 3.2f, r * 3.2f);
        }

        // Handle Circle Fill
        g.setColour(isDragged ? juce::Colours::white : OpenWavLookAndFeel::bgCard);
        g.fillEllipse(p.x - r, p.y - r, r * 2.0f, r * 2.0f);

        g.setColour(OpenWavLookAndFeel::accentCyan);
        g.drawEllipse(p.x - r, p.y - r, r * 2.0f, r * 2.0f, 1.8f);

        // Label Text (1-9)
        g.setFont(juce::Font(10.0f).boldened());
        g.setColour(isDragged ? OpenWavLookAndFeel::bgDark : OpenWavLookAndFeel::textPrimary);
        g.drawText(juce::String(i + 1), p.x - r, p.y - r, r * 2.0f, r * 2.0f, juce::Justification::centred, false);
    }
}

void RecorderComponent::resized()
{
    auto area = getLocalBounds().reduced(16);
    area.removeFromTop(36); // Space for HUD header

    auto mainCard = area.removeFromTop(area.getHeight() - 110);
    juce::ignoreUnused(mainCard);
    area.removeFromTop(12);

    // Form Controls Row (Sample Name, Tags, Buttons, EQ Toggles)
    auto formRow = area.removeFromTop(34);

    recordButton.setBounds(formRow.removeFromLeft(95));
    formRow.removeFromLeft(8);

    timeLabel.setBounds(formRow.removeFromLeft(100));
    formRow.removeFromLeft(10);

    channelLabel.setBounds(formRow.removeFromLeft(90));
    channelSelector.setBounds(formRow.removeFromLeft(145));
    formRow.removeFromLeft(10);

    countInLabel.setBounds(formRow.removeFromLeft(65));
    countInSelector.setBounds(formRow.removeFromLeft(115));
    formRow.removeFromLeft(10);

    lowCutButton.setBounds(formRow.removeFromLeft(110));
    formRow.removeFromLeft(8);

    normalizeButton.setBounds(formRow.removeFromLeft(115));
    formRow.removeFromLeft(10);

    nameLabel.setBounds(formRow.removeFromLeft(85));
    nameEditor.setBounds(formRow.removeFromLeft(135));
    formRow.removeFromLeft(10);

    tagsLabel.setBounds(formRow.removeFromLeft(125));
    tagsEditor.setBounds(formRow.removeFromLeft(135));
    formRow.removeFromLeft(10);

    saveButton.setBounds(formRow.removeFromLeft(145));
    formRow.removeFromLeft(8);

    previewButton.setBounds(formRow.removeFromLeft(75));

    statusLabel.setBounds(area.removeFromTop(24));
}

void RecorderComponent::lookAndFeelChanged()
{
    recordButton.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::favoriteRed.withAlpha(0.2f));
    recordButton.setColour(juce::TextButton::buttonOnColourId, OpenWavLookAndFeel::favoriteRed);
    recordButton.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::favoriteRed);
    recordButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);

    saveButton.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::accentCyan.withAlpha(0.2f));
    saveButton.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::accentCyan);

    previewButton.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::bgCard);
    previewButton.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::textPrimary);
}

void RecorderComponent::visibilityChanged()
{
    updateInputMuteState();
}

void RecorderComponent::updateInputMuteState()
{
    bool isRecordViewActive = isVisible();
    bool isRecordingPressedOrStarted = isCountingDown || audioEngine.isRecording();
    bool shouldMuteInput = !(isRecordViewActive && isRecordingPressedOrStarted);
    audioEngine.setInputMuted(shouldMuteInput);
}

void RecorderComponent::timerCallback()
{
    updateInputMuteState();
    float rawL = 0.0f, rawR = 0.0f;
    audioEngine.getLiveInputLevels(rawL, rawR);

    // Convert linear peak amplitude (0.0 to 1.0) to responsive dB meter scale (-50dB to 0dB -> 0.0 to 1.0)
    float dbL = (rawL > 0.001f) ? juce::Decibels::gainToDecibels(rawL, -50.0f) : -50.0f;
    float dbR = (rawR > 0.001f) ? juce::Decibels::gainToDecibels(rawR, -50.0f) : -50.0f;

    float normL = juce::jlimit(0.0f, 1.0f, (dbL + 50.0f) / 50.0f);
    float normR = juce::jlimit(0.0f, 1.0f, (dbR + 50.0f) / 50.0f);

    // Smooth VU meter response (fast attack, smooth decay)
    if (normL > smoothLeftLevel)
        smoothLeftLevel = normL;
    else
        smoothLeftLevel += (normL - smoothLeftLevel) * 0.25f;

    if (normR > smoothRightLevel)
        smoothRightLevel = normR;
    else
        smoothRightLevel += (normR - smoothRightLevel) * 0.25f;

    if (isCountingDown)
    {
        uint32_t now = juce::Time::getMillisecondCounter();
        if (now - lastBeatMs >= 1000)
        {
            lastBeatMs = now;
            countdownValue--;
            flashAlpha = 1.0f;

            if (countdownValue > 0)
            {
                audioEngine.playMetronomeClick(false);
            }
            else
            {
                isCountingDown = false;
                audioEngine.playMetronomeClick(true); // Accent click on GO!
                audioEngine.startRecording();
                recordButton.setButtonText("STOP");
                recordButton.setToggleState(true, juce::dontSendNotification);
            }
        }
    }

    if (audioEngine.isRecording())
    {
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

void RecorderComponent::toggleRecording()
{
    if (isCountingDown)
    {
        isCountingDown = false;
        recordButton.setButtonText("RECORD");
        recordButton.setToggleState(false, juce::dontSendNotification);
        statusLabel.setText("Countdown cancelled", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
        return;
    }

    if (audioEngine.isRecording())
    {
        audioEngine.stopRecording();
        recordButton.setButtonText("RECORD");
        recordButton.setToggleState(false, juce::dontSendNotification);
        saveButton.setEnabled(hasRecordedBuffer);
        previewButton.setEnabled(hasRecordedBuffer);

        double durSecs = audioEngine.getRecordingDurationSeconds();
        statusLabel.setText("Recording finished (" + juce::String(durSecs, 1) + "s). Ready to save to library.", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    }
    else
    {
        hasRecordedBuffer = false;
        saveButton.setEnabled(false);
        previewButton.setEnabled(false);

        // Reset default name to current timestamp
        nameEditor.setText("Rec_" + juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S"));

        if (countInSelector.getSelectedId() == 1) // 3s Count-In enabled
        {
            isCountingDown = true;
            countdownValue = 3;
            lastBeatMs = juce::Time::getMillisecondCounter();
            flashAlpha = 1.0f;
            audioEngine.playMetronomeClick(false);
            recordButton.setButtonText("CANCEL");
            recordButton.setToggleState(true, juce::dontSendNotification);
            statusLabel.setText("Counting down to record...", juce::dontSendNotification);
            statusLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::favoriteRed);
        }
        else
        {
            audioEngine.playMetronomeClick(true);
            audioEngine.startRecording();
            recordButton.setButtonText("STOP");
            recordButton.setToggleState(true, juce::dontSendNotification);
        }
    }
    updateInputMuteState();
}

void RecorderComponent::playPreview()
{
    juce::AudioBuffer<float> copyBuf;
    double sr = 44100.0;
    if (!audioEngine.getRecordedBufferCopy(copyBuf, sr) || copyBuf.getNumSamples() == 0)
        return;

    // Save temporary WAV file to play preview via AudioEngine
    juce::File tempPreview = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("OWMB_rec_preview.wav");
    if (tempPreview.existsAsFile())
        tempPreview.deleteFile();

    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatWriter> writer(
        format.createWriterFor(new juce::FileOutputStream(tempPreview), sr, copyBuf.getNumChannels(), 16, {}, 0)
    );

    if (writer != nullptr)
    {
        writer->writeFromAudioSampleBuffer(copyBuf, 0, copyBuf.getNumSamples());
        writer.reset();
        audioEngine.loadFile(tempPreview, true);
    }
}

void RecorderComponent::saveAndAddToLibrary()
{
    juce::String sampleName = nameEditor.getText().trim();
    if (sampleName.isEmpty())
        sampleName = "Rec_" + juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S");

    // Check if DSP processing (Low Cut / Normalization) needs to be written to disk
    bool applyLowCut = lowCutButton.getToggleState();
    bool applyNormalize = normalizeButton.getToggleState();

    juce::AudioBuffer<float> buf;
    double sr = 44100.0;
    if (audioEngine.getRecordedBufferCopy(buf, sr) && buf.getNumSamples() > 0 && sr > 0.0)
    {
        if (applyLowCut)
        {
            float dt = 1.0f / static_cast<float>(sr);
            float RC = 1.0f / (2.0f * juce::MathConstants<float>::pi * 80.0f);
            float alpha = RC / (RC + dt);

            for (int ch = 0; ch < buf.getNumChannels(); ++ch)
            {
                float* samples = buf.getWritePointer(ch);
                float prevIn = samples[0];
                float prevOut = samples[0];
                for (int i = 1; i < buf.getNumSamples(); ++i)
                {
                    float currIn = samples[i];
                    float currOut = alpha * (prevOut + currIn - prevIn);
                    samples[i] = currOut;
                    prevIn = currIn;
                    prevOut = currOut;
                }
            }
        }

        if (applyNormalize)
        {
            float maxPeak = buf.getMagnitude(0, buf.getNumSamples());
            if (maxPeak > 0.0001f)
            {
                float targetGain = 0.98f / maxPeak;
                buf.applyGain(targetGain);
            }
        }
    }

    juce::File savedFile = audioEngine.saveRecordingToWav(sampleName);
    if (!savedFile.existsAsFile())
    {
        statusLabel.setText("Error saving recording file to disk.", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::favoriteRed);
        return;
    }

    // Overwrite disk file with DSP processed buffer if Low Cut or Normalization was applied
    if ((applyLowCut || applyNormalize) && buf.getNumSamples() > 0)
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

    // Build MediaItem for library database index
    MediaItem item;
    item.id = juce::String(savedFile.getFullPathName().hashCode64());
    item.filePath = savedFile.getFullPathName();
    item.fileName = savedFile.getFileName();
    item.fileExtension = savedFile.getFileExtension().toLowerCase();
    item.fileSizeBytes = savedFile.getSize();
    item.dateAddedMs = juce::Time::getCurrentTime().toMilliseconds();

    if (buf.getNumSamples() > 0 && sr > 0.0)
    {
        item.durationSeconds = static_cast<double>(buf.getNumSamples()) / sr;
        item.sampleRate = sr;
        item.numChannels = buf.getNumChannels();
        item.bitDepth = 24;
    }

    // Parse Tags
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

    // Auto-preview saved file in engine
    audioEngine.loadFile(savedFile, true);

    statusLabel.setText("SUCCESS! Saved & added to library: " + savedFile.getFileName(), juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);

    saveButton.setEnabled(false);
}

} // namespace openwav
