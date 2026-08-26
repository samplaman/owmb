#include "PlayComponent.h"
#include "OpenWavLookAndFeel.h"

namespace openwav
{

PlayComponent::PlayComponent(AudioEngine& engine)
    : audioEngine(engine),
      customCanvas(engine)
{
    setOpaque(true);

    // ── Header & Instrument Info ───────────────────────
    instrumentTitleLabel.setFont(juce::Font(20.0f).boldened());
    instrumentTitleLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    instrumentTitleLabel.setText("Play Instrument", juce::dontSendNotification);
    addAndMakeVisible(instrumentTitleLabel);

    instrumentInfoLabel.setFont(juce::Font(12.0f).boldened());
    instrumentInfoLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan.withAlpha(0.9f));
    instrumentInfoLabel.setText("No instrument loaded", juce::dontSendNotification);
    addAndMakeVisible(instrumentInfoLabel);

    addAndMakeVisible(editUiButton);
    editUiButton.setClickingTogglesState(true);
    editUiButton.setTooltip("Toggle visual UI editor to move, resize, and style controls on the canvas");
    editUiButton.onClick = [this] {
        bool editing = editUiButton.getToggleState();
        customCanvas.setEditModeEnabled(editing);
        styleInspector.setVisible(editing);
        editUiButton.setButtonText(editing ? "Lock UI" : "Edit UI");
        if (editing)
        {
            styleInspector.setInstrumentState(currentState, customCanvas.getActiveTab());
            styleInspector.setSelectedItem(customCanvas.getSelectedItem());
        }
        resized();
    };

    addAndMakeVisible(savePresetButton);
    savePresetButton.setTooltip("Save current UI layout, styling, and sample mapping as a Decent Sampler preset (.dspreset) in the same folder as the Samples directory");
    savePresetButton.onClick = [this] {
        // Find default folder:
        juce::File defaultTarget;
        juce::File defaultDir;

        // 1. If preset was loaded from an existing file, save back to the exact same file & folder level
        if (currentState.presetFilePath.isNotEmpty())
        {
            juce::File origF(currentState.presetFilePath);
            if (origF.existsAsFile())
            {
                defaultTarget = origF;
                defaultDir = origF.getParentDirectory();
            }
            else if (origF.getParentDirectory().exists())
            {
                defaultTarget = origF;
                defaultDir = origF.getParentDirectory();
            }
        }

        // 2. Otherwise locate folder relative to Samples or background image
        if (!defaultDir.exists() && !currentState.zones.empty())
        {
            juce::File firstSample(currentState.zones[0].filePath);
            if (firstSample.existsAsFile())
            {
                auto sampleFolder = firstSample.getParentDirectory();
                if (sampleFolder.getFileName().equalsIgnoreCase("samples") ||
                    sampleFolder.getFileName().equalsIgnoreCase("audio") ||
                    sampleFolder.getFileName().equalsIgnoreCase("wavs") ||
                    sampleFolder.getFileName().equalsIgnoreCase("sample"))
                {
                    defaultDir = sampleFolder.getParentDirectory();
                }
                else
                {
                    defaultDir = sampleFolder.getParentDirectory();
                }
            }
        }
        if (!defaultDir.exists() && currentState.customUi.resolvedBgImagePath.isNotEmpty())
        {
            juce::File bgF(currentState.customUi.resolvedBgImagePath);
            if (bgF.existsAsFile())
            {
                auto bgFolder = bgF.getParentDirectory();
                if (bgFolder.getFileName().equalsIgnoreCase("resources") ||
                    bgFolder.getFileName().equalsIgnoreCase("images") ||
                    bgFolder.getFileName().equalsIgnoreCase("artwork") ||
                    bgFolder.getFileName().equalsIgnoreCase("assets"))
                    defaultDir = bgFolder.getParentDirectory();
                else
                    defaultDir = bgFolder;
            }
        }
        if (!defaultDir.exists())
            defaultDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);

        if (!defaultTarget.existsAsFile())
        {
            juce::String instName = currentState.instrumentName;
            if (instName.isEmpty() && defaultDir.exists() && defaultDir.getFileName().isNotEmpty())
                instName = defaultDir.getFileName();
            if (instName.isEmpty() && !currentState.zones.empty())
                instName = juce::File(currentState.zones[0].sampleName).getFileNameWithoutExtension();
            if (instName.isEmpty())
                instName = "Preset";

            defaultTarget = defaultDir.getChildFile(instName + ".dspreset");
        }

        auto chooser = std::make_shared<juce::FileChooser>("Save Decent Sampler Preset (.dspreset)", defaultTarget, "*.dspreset");
        chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                             [this, chooser](const juce::FileChooser&) {
            auto resultFile = chooser->getResult();
            if (resultFile != juce::File())
            {
                if (!resultFile.hasFileExtension("dspreset"))
                    resultFile = resultFile.withFileExtension("dspreset");

                currentState = customCanvas.getInstrumentState();
                bool success = currentState.saveToDecentSamplerPreset(resultFile);
                if (success)
                {
                    currentState.instrumentName = resultFile.getFileNameWithoutExtension();
                    customCanvas.setInstrumentState(currentState);
                    syncUiFromState();
                    if (onStateChanged)
                        onStateChanged(currentState);
                }
            }
        });
    };

    addAndMakeVisible(loadPresetButton);
    loadPresetButton.addListener(this);
    loadPresetButton.setTooltip("Load a .dspreset, .dslibrary, or sample map file");

    addAndMakeVisible(unloadPresetButton);
    unloadPresetButton.addListener(this);
    unloadPresetButton.setTooltip("Unload current preset and clear sample map");

    addAndMakeVisible(editMapButton);
    editMapButton.addListener(this);
    editMapButton.setTooltip("Open in Sample Map Editor");

    addAndMakeVisible(allNotesOffButton);
    allNotesOffButton.addListener(this);
    allNotesOffButton.setTooltip("Stop all active voices / Panic");

    // ── Front Decent Sampler Canvas View ───────────────
    addAndMakeVisible(customCanvas);
    customCanvas.onStateChanged = [this](const SampleMapState& updatedState) {
        currentState = updatedState;
        syncUiFromState();
        if (onStateChanged)
            onStateChanged(currentState);
    };

    // ── Right-side Style Inspector ─────────────────────
    addAndMakeVisible(styleInspector);
    styleInspector.setCanvas(&customCanvas);
    styleInspector.onStateChanged = [this](const SampleMapState& updatedState) {
        currentState = updatedState;
        customCanvas.setInstrumentState(currentState);
        syncUiFromState();
        if (onStateChanged)
            onStateChanged(currentState);
    };

    customCanvas.onItemSelected = [this](const DecentSamplerCanvasComponent::SelectedCanvasItem& item) {
        styleInspector.setInstrumentState(currentState, customCanvas.getActiveTab());
        styleInspector.setSelectedItem(item);
    };

    styleInspector.setVisible(false);

    lookAndFeelChanged();
}

void PlayComponent::lookAndFeelChanged()
{
    editUiButton.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::bgCard);
    editUiButton.setColour(juce::TextButton::buttonOnColourId, OpenWavLookAndFeel::accentCyan);
    editUiButton.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::accentCyan);
    editUiButton.setColour(juce::TextButton::textColourOnId, juce::Colours::black);

    savePresetButton.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::accentCyan.withAlpha(0.2f));
    savePresetButton.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::accentCyan);

    loadPresetButton.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::accentCyan.withAlpha(0.2f));
    loadPresetButton.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::accentCyan);

    unloadPresetButton.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::bgCard);
    unloadPresetButton.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::textSecondary);

    editMapButton.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::bgCard);
    editMapButton.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::accentCyan);

    allNotesOffButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0x33FF4444));
    allNotesOffButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xFFFF6666));

    repaint();
}

void PlayComponent::paint(juce::Graphics& g)
{
    g.fillAll(OpenWavLookAndFeel::bgDark);
}

void PlayComponent::resized()
{
    auto area = getLocalBounds().reduced(10, 8);

    // 1. Top Header Row
    auto headerRow = area.removeFromTop(36);
    editUiButton.setBounds(headerRow.removeFromRight(78).reduced(0, 3));
    headerRow.removeFromRight(6);
    savePresetButton.setBounds(headerRow.removeFromRight(95).reduced(0, 3));
    headerRow.removeFromRight(6);
    loadPresetButton.setBounds(headerRow.removeFromRight(95).reduced(0, 3));
    headerRow.removeFromRight(6);
    unloadPresetButton.setBounds(headerRow.removeFromRight(68).reduced(0, 3));
    headerRow.removeFromRight(6);
    editMapButton.setBounds(headerRow.removeFromRight(78).reduced(0, 3));
    headerRow.removeFromRight(6);
    allNotesOffButton.setBounds(headerRow.removeFromRight(56).reduced(0, 3));
    headerRow.removeFromRight(12);

    int titleW = std::min(280, headerRow.getWidth() / 2);
    instrumentTitleLabel.setBounds(headerRow.removeFromLeft(titleW));
    instrumentInfoLabel.setBounds(headerRow.reduced(0, 3));

    area.removeFromTop(8);

    // 2. Canvas & Right Style Inspector View
    if (styleInspector.isVisible())
    {
        int inspectorW = std::min(320, std::max(270, area.getWidth() / 3));
        auto inspectorArea = area.removeFromRight(inspectorW);
        area.removeFromRight(8);
        styleInspector.setBounds(inspectorArea);
    }
    customCanvas.setBounds(area);
}

void PlayComponent::buttonClicked(juce::Button* button)
{
    if (button == &loadPresetButton)
    {
        if (onLoadPresetRequested)
            onLoadPresetRequested();
    }
    else if (button == &unloadPresetButton)
    {
        audioEngine.stopAllVoices();
        audioEngine.resetAllGroups();
        audioEngine.setSamplerReverbAmount(0.0f);
        audioEngine.setSamplerDelay(350.0f, 0.3f, 0.0f);
        audioEngine.setSamplerChorus(1.0f, 0.25f, 0.0f);
        audioEngine.setSamplerLowpassCutoff(22000.0f);
        audioEngine.setSamplerHighpassCutoff(10.0f);

        currentState = SampleMapState();
        customCanvas.setInstrumentState(currentState);
        syncUiFromState();
        repaint();

        if (onUnloadPresetRequested)
            onUnloadPresetRequested();

        if (onStateChanged)
            onStateChanged(currentState);
    }
    else if (button == &editMapButton)
    {
        if (onOpenSampleMapRequested)
            onOpenSampleMapRequested();
    }
    else if (button == &allNotesOffButton)
    {
        audioEngine.stopAllVoices();
        for (int note = 0; note < 128; ++note)
        {
            audioEngine.getKeyboardState().noteOff(1, note, 0.0f);
        }
    }
}

void PlayComponent::syncUiFromState()
{
    float revAmount = (currentState.irFilePath.isNotEmpty() && currentState.irReverbWetLevel > 0.001f) ? currentState.irReverbWetLevel : currentState.samplerReverbAmount;
    audioEngine.setSamplerReverbAmount(currentState.samplerReverbAmount);

    if (currentState.irFilePath.isNotEmpty())
    {
        audioEngine.loadImpulseResponseFile(juce::File(currentState.irFilePath));
        audioEngine.setSamplerIrReverbAmount(currentState.irReverbWetLevel);
        audioEngine.setSamplerIrReverbDryLevel(currentState.irReverbDryLevel);
    }
    audioEngine.setSamplerDelay(currentState.delayTimeMs, currentState.delayFeedback, currentState.delayWetLevel);
    audioEngine.setSamplerChorus(currentState.chorusRateHz, currentState.chorusDepth, currentState.chorusWetLevel);

    for (const auto& m : currentState.modulators)
    {
        if (m.scope.equalsIgnoreCase("global") || m.scope.isEmpty())
        {
            audioEngine.setLfoFrequency(m.frequency);
            audioEngine.setLfoAmount(m.modAmount);
            audioEngine.setLfoShapeByName(m.shape);
            audioEngine.setLfoTargetByName(m.target);
            audioEngine.setLfoTargetName(m.target);
            break;
        }
    }

    for (const auto& g : currentState.groups)
    {
        audioEngine.setGroupVolumeDb(g.index, g.volumeDb);
        audioEngine.setGroupPan(g.index, g.pan);
        audioEngine.setGroupTuningCents(g.index, g.fineTuneCents);
        audioEngine.setGroupMuted(g.index, g.muted || !g.enabled);
    }

    audioEngine.setSamplerLowpassCutoff(currentState.masterFilterCutoffHz);
    audioEngine.setSamplerHighpassCutoff(currentState.masterHighpassHz);
    audioEngine.setPitchTrackingEnabled(currentState.pitchTrackingEnabled);

    // Update Title & Info
    juce::String name = currentState.instrumentName;
    if (name.isEmpty() && !currentState.zones.empty())
        name = juce::File(currentState.zones[0].sampleName).getFileNameWithoutExtension();
    if (name.isEmpty())
        name = "Default Multi-Sample Instrument";

    instrumentTitleLabel.setText(name, juce::dontSendNotification);

    int numZones = static_cast<int>(currentState.zones.size());
    int minKey = 127, maxKey = 0;
    std::set<int> velLayers;
    for (const auto& z : currentState.zones)
    {
        minKey = std::min(minKey, z.keyLow);
        maxKey = std::max(maxKey, z.keyHigh);
        velLayers.insert(z.velLow);
    }

    if (numZones > 0)
    {
        juce::String info = juce::String(numZones) + " Zones | " +
                            juce::String(velLayers.size()) + " Velocity Layer" + (velLayers.size() > 1 ? "s" : "") +
                            " | Range: " + juce::MidiMessage::getMidiNoteName(minKey, true, true, 3) + " - " +
                            juce::MidiMessage::getMidiNoteName(maxKey, true, true, 3);
        instrumentInfoLabel.setText(info, juce::dontSendNotification);
    }
    else
    {
        instrumentInfoLabel.setText("No sample zones mapped. Load a preset or map samples in Sample Map view.", juce::dontSendNotification);
    }
}

void PlayComponent::setState(const SampleMapState& state)
{
    currentState = state;
    customCanvas.setInstrumentState(currentState);
    styleInspector.setInstrumentState(currentState, customCanvas.getActiveTab());
    styleInspector.setSelectedItem(customCanvas.getSelectedItem());
    syncUiFromState();
    repaint();
}

SampleMapState PlayComponent::getState() const
{
    return currentState;
}

} // namespace openwav
