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

    lookAndFeelChanged();

    startTimerHz(30);
}

RecorderComponent::~RecorderComponent()
{
    stopTimer();
}

void RecorderComponent::paint(juce::Graphics& g)
{
    g.fillAll(OpenWavLookAndFeel::bgDark);

    auto bounds = getLocalBounds().reduced(16);

    // Title HUD Header
    g.setFont(juce::Font(16.0f).boldened());
    g.setColour(OpenWavLookAndFeel::textPrimary);
    g.drawText("AUDIO SAMPLE RECORDER", bounds.removeFromTop(24), juce::Justification::left, true);

    bounds.removeFromTop(12);

    // Scope & Meter Box Area
    auto mainCard = bounds.removeFromTop(bounds.getHeight() - 110).toFloat();
    g.setColour(OpenWavLookAndFeel::bgHeader);
    g.fillRoundedRectangle(mainCard, 8.0f);
    g.setColour(OpenWavLookAndFeel::borderColour);
    g.drawRoundedRectangle(mainCard, 8.0f, 1.0f);

    auto scopeArea = mainCard.reduced(16.0f);

    // Reserve right side for Stereo VU Level Meters
    auto meterArea = scopeArea.removeFromRight(36.0f);
    scopeArea.removeFromRight(12.0f);

    // Draw Live Oscilloscope Waveform View
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
}

void RecorderComponent::resized()
{
    auto area = getLocalBounds().reduced(16);
    area.removeFromTop(36); // Space for HUD header

    auto mainCard = area.removeFromTop(area.getHeight() - 110);
    juce::ignoreUnused(mainCard);
    area.removeFromTop(12);

    // Form Controls Row (Sample Name, Tags, Buttons)
    auto formRow = area.removeFromTop(34);

    recordButton.setBounds(formRow.removeFromLeft(110));
    formRow.removeFromLeft(12);

    timeLabel.setBounds(formRow.removeFromLeft(120));
    formRow.removeFromLeft(16);

    nameLabel.setBounds(formRow.removeFromLeft(90));
    nameEditor.setBounds(formRow.removeFromLeft(180));
    formRow.removeFromLeft(16);

    tagsLabel.setBounds(formRow.removeFromLeft(140));
    tagsEditor.setBounds(formRow.removeFromLeft(200));
    formRow.removeFromLeft(16);

    saveButton.setBounds(formRow.removeFromLeft(160));
    formRow.removeFromLeft(10);

    previewButton.setBounds(formRow.removeFromLeft(90));

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

void RecorderComponent::timerCallback()
{
    float rawL = 0.0f, rawR = 0.0f;
    audioEngine.getLiveInputLevels(rawL, rawR);

    // Smooth VU meter movement
    smoothLeftLevel += (rawL - smoothLeftLevel) * 0.35f;
    smoothRightLevel += (rawR - smoothRightLevel) * 0.35f;

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

        audioEngine.startRecording();
        recordButton.setButtonText("STOP");
        recordButton.setToggleState(true, juce::dontSendNotification);
    }
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

    juce::File savedFile = audioEngine.saveRecordingToWav(sampleName);
    if (!savedFile.existsAsFile())
    {
        statusLabel.setText("Error saving recording file to disk.", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::favoriteRed);
        return;
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

    juce::AudioBuffer<float> buf;
    double sr = 44100.0;
    if (audioEngine.getRecordedBufferCopy(buf, sr) && sr > 0.0)
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
    if (item.tags.empty())
        item.tags.insert("Recorded");

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
