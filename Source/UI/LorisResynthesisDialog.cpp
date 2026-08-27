#include "LorisResynthesisDialog.h"
#include "OpenWavLookAndFeel.h"

namespace openwav
{

LorisResynthesisDialog::LorisResynthesisDialog(std::function<void(const std::vector<ResynthesizedZone>&)> onFinishedCallback)
    : onFinished(std::move(onFinishedCallback)),
      progressBar(progressValue)
{
    setSize(480, 440);

    titleLabel.setText("Loris Additive Resynthesis", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel);

    sampleInfoLabel.setText("No sample loaded", juce::dontSendNotification);
    sampleInfoLabel.setFont(juce::FontOptions(12.0f));
    sampleInfoLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.6f));
    addAndMakeVisible(sampleInfoLabel);

    // Root Note
    rootNoteLabel.setText("Root Note:", juce::dontSendNotification);
    rootNoteLabel.setFont(juce::FontOptions(13.0f));
    rootNoteLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.85f));
    addAndMakeVisible(rootNoteLabel);

    populateNoteComboBox(rootNoteBox, 60);
    rootNoteBox.addListener(this);
    addAndMakeVisible(rootNoteBox);

    autoDetectBtn.addListener(this);
    addAndMakeVisible(autoDetectBtn);

    // Min Note
    minNoteLabel.setText("Min Note:", juce::dontSendNotification);
    minNoteLabel.setFont(juce::FontOptions(13.0f));
    minNoteLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.85f));
    addAndMakeVisible(minNoteLabel);

    populateNoteComboBox(minNoteBox, 36); // C2
    minNoteBox.addListener(this);
    addAndMakeVisible(minNoteBox);

    // Max Note
    maxNoteLabel.setText("Max Note:", juce::dontSendNotification);
    maxNoteLabel.setFont(juce::FontOptions(13.0f));
    maxNoteLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.85f));
    addAndMakeVisible(maxNoteLabel);

    populateNoteComboBox(maxNoteBox, 84); // C6
    maxNoteBox.addListener(this);
    addAndMakeVisible(maxNoteBox);

    // Density / Stride
    strideLabel.setText("Note Step / Density:", juce::dontSendNotification);
    strideLabel.setFont(juce::FontOptions(13.0f));
    strideLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.85f));
    addAndMakeVisible(strideLabel);

    strideBox.addItem("Every Semitone (Chromatic - Dense)", 1);
    strideBox.addItem("Every 2 Semitones (Whole Tone)", 2);
    strideBox.addItem("Every 3 Semitones (Minor 3rd - Recommended)", 3);
    strideBox.addItem("Every 4 Semitones (Major 3rd)", 4);
    strideBox.addItem("Every 6 Semitones (Tritone)", 6);
    strideBox.addItem("Every Octave (Fast)", 12);
    strideBox.setSelectedId(3); // Minor 3rd
    strideBox.addListener(this);
    addAndMakeVisible(strideBox);

    // Formant Toggle
    formantLockToggle.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(formantLockToggle);

    // Quality Preset
    qualityLabel.setText("Quality / Preset:", juce::dontSendNotification);
    qualityLabel.setFont(juce::FontOptions(13.0f));
    qualityLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.85f));
    addAndMakeVisible(qualityLabel);

    qualityBox.addItem("Adaptive f0 / Standard", 1);
    qualityBox.addItem("Vocal / Solo Instrument (High Res)", 2);
    qualityBox.addItem("Bass / Low Frequency Focus", 3);
    qualityBox.addItem("Percussive / Fast Transients", 4);
    qualityBox.setSelectedId(1);
    addAndMakeVisible(qualityBox);

    // Progress Bar & Status
    progressBar.setPercentageDisplay(false);
    addAndMakeVisible(progressBar);

    statusLabel.setText("Ready to resynthesize.", juce::dontSendNotification);
    statusLabel.setFont(juce::FontOptions(12.0f));
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.7f));
    addAndMakeVisible(statusLabel);

    // Action Buttons
    startBtn.addListener(this);
    addAndMakeVisible(startBtn);

    cancelBtn.addListener(this);
    addAndMakeVisible(cancelBtn);
}

LorisResynthesisDialog::~LorisResynthesisDialog()
{
    resynthesizer.cancel();
}

void LorisResynthesisDialog::populateNoteComboBox(juce::ComboBox& box, int defaultNote)
{
    box.clear();
    for (int n = 12; n <= 108; ++n)
    {
        juce::String name = LorisResynthesizer::getMidiNoteName(n) + " (" + juce::String(n) + ")";
        box.addItem(name, n + 1); // ID must be > 0
    }
    box.setSelectedId(defaultNote + 1, juce::dontSendNotification);
}

void LorisResynthesisDialog::setSourceSample(const juce::File& file, const juce::AudioBuffer<float>& buffer, double sampleRate, int initialRootNote)
{
    config.sourceFile = file;
    config.sourceBuffer.makeCopyOf(buffer);
    config.sampleRate = sampleRate;
    config.baseSampleName = file.existsAsFile() ? file.getFileNameWithoutExtension() : "Sample";

    sampleInfoLabel.setText("Source: " + config.baseSampleName + " (" + juce::String(sampleRate / 1000.0, 1) + " kHz, " +
                            juce::String(buffer.getNumSamples() / sampleRate, 2) + "s)", juce::dontSendNotification);

    int detectedRoot = initialRootNote;
    if (buffer.getNumSamples() > 512)
    {
        detectedRoot = LorisResynthesizer::detectRootMidiNote(buffer, sampleRate);
    }
    rootNoteBox.setSelectedId(detectedRoot + 1, juce::dontSendNotification);

    // Default range around root note
    int minN = std::max(24, detectedRoot - 24);
    int maxN = std::min(96, detectedRoot + 24);
    minNoteBox.setSelectedId(minN + 1, juce::dontSendNotification);
    maxNoteBox.setSelectedId(maxN + 1, juce::dontSendNotification);
}

void LorisResynthesisDialog::setSourceMediaItem(const MediaItem& item, const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    setSourceSample(juce::File(item.filePath), buffer, sampleRate, 60);
}

void LorisResynthesisDialog::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff18181b)); // Dark background

    g.setColour(juce::Colour(0xff27272a));
    g.drawRect(getLocalBounds(), 1);

    // Divider line
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.drawLine(16.0f, 52.0f, (float)getWidth() - 16.0f, 52.0f, 1.0f);
}

void LorisResynthesisDialog::resized()
{
    auto bounds = getLocalBounds().reduced(20, 16);

    titleLabel.setBounds(bounds.removeFromTop(24));
    sampleInfoLabel.setBounds(bounds.removeFromTop(18));
    bounds.removeFromTop(12);

    // Root Note Row
    auto rootRow = bounds.removeFromTop(28);
    rootNoteLabel.setBounds(rootRow.removeFromLeft(120));
    autoDetectBtn.setBounds(rootRow.removeFromRight(100));
    rootRow.removeFromRight(8);
    rootNoteBox.setBounds(rootRow);
    bounds.removeFromTop(10);

    // Min / Max Note Row
    auto rangeRow = bounds.removeFromTop(28);
    minNoteLabel.setBounds(rangeRow.removeFromLeft(70));
    minNoteBox.setBounds(rangeRow.removeFromLeft(130));
    rangeRow.removeFromLeft(20);
    maxNoteLabel.setBounds(rangeRow.removeFromLeft(70));
    maxNoteBox.setBounds(rangeRow);
    bounds.removeFromTop(10);

    // Density / Stride Row
    auto strideRow = bounds.removeFromTop(28);
    strideLabel.setBounds(strideRow.removeFromLeft(150));
    strideBox.setBounds(strideRow);
    bounds.removeFromTop(10);

    // Quality Preset Row
    auto qualityRow = bounds.removeFromTop(28);
    qualityLabel.setBounds(qualityRow.removeFromLeft(150));
    qualityBox.setBounds(qualityRow);
    bounds.removeFromTop(10);

    // Formant Lock Toggle
    formantLockToggle.setBounds(bounds.removeFromTop(26));
    bounds.removeFromTop(12);

    // Progress Bar & Status
    progressBar.setBounds(bounds.removeFromTop(14));
    bounds.removeFromTop(6);
    statusLabel.setBounds(bounds.removeFromTop(18));
    bounds.removeFromTop(14);

    // Bottom Action Buttons
    auto btnRow = bounds.removeFromTop(32);
    cancelBtn.setBounds(btnRow.removeFromRight(90));
    btnRow.removeFromRight(10);
    startBtn.setBounds(btnRow);
}

void LorisResynthesisDialog::buttonClicked(juce::Button* button)
{
    if (button == &autoDetectBtn)
    {
        if (config.sourceBuffer.getNumSamples() > 0)
        {
            int detected = LorisResynthesizer::detectRootMidiNote(config.sourceBuffer, config.sampleRate);
            rootNoteBox.setSelectedId(detected + 1, juce::sendNotificationSync);
            statusLabel.setText("Detected root note: " + LorisResynthesizer::getMidiNoteName(detected), juce::dontSendNotification);
        }
    }
    else if (button == &startBtn)
    {
        startResynthesis();
    }
    else if (button == &cancelBtn)
    {
        if (resynthesizer.isRunning())
        {
            resynthesizer.cancel();
            statusLabel.setText("Cancelling...", juce::dontSendNotification);
        }
        else
        {
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            {
                dw->exitModalState(0);
            }
        }
    }
}

void LorisResynthesisDialog::comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged)
{
    if (comboBoxThatHasChanged == &minNoteBox || comboBoxThatHasChanged == &maxNoteBox)
    {
        int minN = minNoteBox.getSelectedId() - 1;
        int maxN = maxNoteBox.getSelectedId() - 1;
        if (minN > maxN)
        {
            maxNoteBox.setSelectedId(minN + 1, juce::dontSendNotification);
        }
    }
}

void LorisResynthesisDialog::startResynthesis()
{
    if (resynthesizer.isRunning())
        return;

    config.rootNote = rootNoteBox.getSelectedId() - 1;
    config.autoDetectRoot = false;
    config.minNote = minNoteBox.getSelectedId() - 1;
    config.maxNote = maxNoteBox.getSelectedId() - 1;
    config.noteStride = strideBox.getSelectedId();
    config.preserveFormants = formantLockToggle.getToggleState();

    int qPreset = qualityBox.getSelectedId();
    double fundamentalHz = 440.0 * std::pow(2.0, (config.rootNote - 69) / 12.0);

    if (qPreset == 2) // Vocal / Lead
    {
        config.freqResolutionHz = fundamentalHz * 0.6;
        config.windowWidthHz = config.freqResolutionHz * 2.5;
        config.freqDriftHz = config.freqResolutionHz * 0.4;
    }
    else if (qPreset == 3) // Bass
    {
        config.freqResolutionHz = std::max(20.0, fundamentalHz * 0.9);
        config.windowWidthHz = config.freqResolutionHz * 3.0;
        config.freqDriftHz = config.freqResolutionHz * 0.3;
    }
    else if (qPreset == 4) // Percussive
    {
        config.freqResolutionHz = fundamentalHz * 1.0;
        config.windowWidthHz = config.freqResolutionHz * 1.8;
        config.freqDriftHz = config.freqResolutionHz * 0.7;
    }
    else
    {
        config.freqResolutionHz = 0.0; // Auto
        config.windowWidthHz = 0.0;
        config.freqDriftHz = 0.0;
    }

    startBtn.setEnabled(false);
    autoDetectBtn.setEnabled(false);
    rootNoteBox.setEnabled(false);
    minNoteBox.setEnabled(false);
    maxNoteBox.setEnabled(false);
    strideBox.setEnabled(false);
    qualityBox.setEnabled(false);
    formantLockToggle.setEnabled(false);
    cancelBtn.setButtonText("Cancel");

    updateStatus(0.02f, "Starting Loris partial analysis...");

    resynthesizer.startResynthesis(
        config,
        [this](float prog, const juce::String& text) {
            updateStatus(prog, text);
        },
        [this](const std::vector<ResynthesizedZone>& zones, bool success, const juce::String& error) {
            startBtn.setEnabled(true);
            autoDetectBtn.setEnabled(true);
            rootNoteBox.setEnabled(true);
            minNoteBox.setEnabled(true);
            maxNoteBox.setEnabled(true);
            strideBox.setEnabled(true);
            qualityBox.setEnabled(true);
            formantLockToggle.setEnabled(true);
            cancelBtn.setButtonText("Close");

            if (success)
            {
                updateStatus(1.0f, "Generated " + juce::String((int)zones.size()) + " zones successfully!");

                if (onFinished)
                {
                    onFinished(zones);
                }

                juce::Timer::callAfterDelay(700, [this]() {
                    if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                    {
                        dw->exitModalState(1);
                    }
                });
            }
            else
            {
                updateStatus(0.0f, "Error: " + error);
            }
        });
}

void LorisResynthesisDialog::updateStatus(float progress, const juce::String& text)
{
    progressValue = std::clamp((double)progress, 0.0, 1.0);
    statusLabel.setText(text, juce::dontSendNotification);
    repaint();
}

void LorisResynthesisDialog::showDialog(
    juce::Component* parentComponent,
    const juce::File& sourceFile,
    const juce::AudioBuffer<float>& buffer,
    double sampleRate,
    int rootNote,
    std::function<void(const std::vector<ResynthesizedZone>&)> onFinished)
{
    auto* dialog = new LorisResynthesisDialog(std::move(onFinished));
    dialog->setSourceSample(sourceFile, buffer, sampleRate, rootNote);

    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "Loris Additive Resynthesis";
    options.dialogBackgroundColour = juce::Colour(0xff18181b);
    options.content.setOwned(dialog);
    options.useNativeTitleBar = true;
    options.resizable = false;

    if (parentComponent != nullptr)
    {
        options.componentToCentreAround = parentComponent;
    }

    options.launchAsync();
}

} // namespace openwav
