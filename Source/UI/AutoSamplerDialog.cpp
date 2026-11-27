#include "AutoSamplerDialog.h"
#include "OpenWavLookAndFeel.h"

namespace openwav
{

AutoSamplerDialog::AutoSamplerDialog(std::function<void(const SampleMapState& resultState)> onComplete,
                                     const juce::String& targetLabel)
    : onCompleteCallback(std::move(onComplete))
{
    setSize(560, 495);

    titleLabel.setFont(juce::Font(17.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    if (targetLabel.isNotEmpty())
        titleLabel.setText("VST3 / AU Auto-Sampler - " + targetLabel, juce::dontSendNotification);
    addAndMakeVisible(titleLabel);

    auto styleLabel = [this](juce::Label& l) {
        l.setFont(juce::Font(12.0f));
        l.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
        addAndMakeVisible(l);
    };

    styleLabel(pluginLabel);
    pluginCombo.setTextWhenNothingSelected("Select VST3 / AU Instrument...");
    pluginCombo.onChange = [this] { loadSelectedPlugin(); };
    addAndMakeVisible(pluginCombo);

    scanPluginsBtn.onClick = [this] {
        scanPluginsBtn.setEnabled(false);
        startBtn.setEnabled(false);
        statusLabel.setText("Scanning VST3 / AU plugins...", juce::dontSendNotification);
        juce::Component::SafePointer<AutoSamplerDialog> safeThis(this);
        engine.scanPluginsAsync(
            [safeThis](float prog, const juce::String& name) {
                if (safeThis == nullptr) return;
                safeThis->progressValue = static_cast<double>(prog);
                safeThis->statusLabel.setText("Scanning: " + name, juce::dontSendNotification);
            },
            [safeThis](int count) {
                if (safeThis == nullptr) return;
                safeThis->progressValue = 0.0;
                safeThis->scanPluginsBtn.setEnabled(true);
                safeThis->refreshPluginCombo();
                safeThis->startBtn.setEnabled(safeThis->loadedPlugin != nullptr);
                safeThis->statusLabel.setText("Found " + juce::String(count) + " instrument plugin(s).", juce::dontSendNotification);
            });
    };
    addAndMakeVisible(scanPluginsBtn);

    openEditorBtn.setEnabled(false);
    openEditorBtn.onClick = [this] { openPluginEditor(); };
    addAndMakeVisible(openEditorBtn);

    styleLabel(nameLabel);
    nameEditor.setText("AutoSampled Instrument", juce::dontSendNotification);
    addAndMakeVisible(nameEditor);

    styleLabel(templateLabel);
    templateCombo.addItem("Custom", 1);
    templateCombo.addItem("Piano (Keys + Release)", 2);
    templateCombo.addItem("Drum Kit (Chromatic)", 3);
    templateCombo.addItem("Synth / Lead / Bass", 4);
    templateCombo.addItem("Lush Pad / Strings", 5);
    templateCombo.setSelectedId(1, juce::dontSendNotification);
    templateCombo.onChange = [this] {
        const int id = templateCombo.getSelectedId();
        if (id == 2) { // Piano
            minNoteCombo.setSelectedId(36 + 1, juce::dontSendNotification);
            maxNoteCombo.setSelectedId(84 + 1, juce::dontSendNotification);
            strideCombo.setSelectedId(3, juce::dontSendNotification);
            velLayersCombo.setSelectedId(3, juce::dontSendNotification);
            rrCombo.setSelectedId(2, juce::dontSendNotification);
            sustainSlider.setValue(1.8, juce::dontSendNotification);
            tailSlider.setValue(3.5, juce::dontSendNotification);
            autoTailToggle.setToggleState(true, juce::dontSendNotification);
            trimSilenceToggle.setToggleState(true, juce::dontSendNotification);
            captureReleaseToggle.setToggleState(true, juce::dontSendNotification);
        } else if (id == 3) { // Drum Kit
            minNoteCombo.setSelectedId(36 + 1, juce::dontSendNotification);
            maxNoteCombo.setSelectedId(60 + 1, juce::dontSendNotification);
            strideCombo.setSelectedId(1, juce::dontSendNotification);
            velLayersCombo.setSelectedId(3, juce::dontSendNotification);
            rrCombo.setSelectedId(3, juce::dontSendNotification);
            sustainSlider.setValue(0.4, juce::dontSendNotification);
            tailSlider.setValue(1.5, juce::dontSendNotification);
            autoTailToggle.setToggleState(true, juce::dontSendNotification);
            trimSilenceToggle.setToggleState(true, juce::dontSendNotification);
            captureReleaseToggle.setToggleState(false, juce::dontSendNotification);
        } else if (id == 4) { // Synth / Bass
            minNoteCombo.setSelectedId(36 + 1, juce::dontSendNotification);
            maxNoteCombo.setSelectedId(84 + 1, juce::dontSendNotification);
            strideCombo.setSelectedId(4, juce::dontSendNotification);
            velLayersCombo.setSelectedId(2, juce::dontSendNotification);
            rrCombo.setSelectedId(1, juce::dontSendNotification);
            sustainSlider.setValue(2.0, juce::dontSendNotification);
            tailSlider.setValue(2.5, juce::dontSendNotification);
            autoTailToggle.setToggleState(true, juce::dontSendNotification);
            trimSilenceToggle.setToggleState(true, juce::dontSendNotification);
            captureReleaseToggle.setToggleState(false, juce::dontSendNotification);
        } else if (id == 5) { // Pad / Strings
            minNoteCombo.setSelectedId(36 + 1, juce::dontSendNotification);
            maxNoteCombo.setSelectedId(72 + 1, juce::dontSendNotification);
            strideCombo.setSelectedId(6, juce::dontSendNotification);
            velLayersCombo.setSelectedId(1, juce::dontSendNotification);
            rrCombo.setSelectedId(1, juce::dontSendNotification);
            sustainSlider.setValue(3.0, juce::dontSendNotification);
            tailSlider.setValue(4.5, juce::dontSendNotification);
            autoTailToggle.setToggleState(true, juce::dontSendNotification);
            trimSilenceToggle.setToggleState(true, juce::dontSendNotification);
            captureReleaseToggle.setToggleState(false, juce::dontSendNotification);
        }
        updateEstimateLabel();
    };
    addAndMakeVisible(templateCombo);

    auto populateNoteCombo = [](juce::ComboBox& cb, int defaultNote) {
        for (int n = 0; n <= 127; ++n)
            cb.addItem(AutoSamplerEngine::midiNoteToName(n) + " (" + juce::String(n) + ")", n + 1);
        cb.setSelectedId(defaultNote + 1, juce::dontSendNotification);
    };

    styleLabel(minNoteLabel);
    populateNoteCombo(minNoteCombo, 36); // C2
    minNoteCombo.onChange = [this] { updateEstimateLabel(); };
    addAndMakeVisible(minNoteCombo);

    styleLabel(maxNoteLabel);
    populateNoteCombo(maxNoteCombo, 84); // C6
    maxNoteCombo.onChange = [this] { updateEstimateLabel(); };
    addAndMakeVisible(maxNoteCombo);

    styleLabel(strideLabel);
    strideCombo.addItem("1 (Every Semitone)", 1);
    strideCombo.addItem("2 (Whole Tone)", 2);
    strideCombo.addItem("3 (Minor 3rd)", 3);
    strideCombo.addItem("4 (Major 3rd)", 4);
    strideCombo.addItem("6 (Tritone)", 6);
    strideCombo.addItem("12 (Octaves)", 12);
    strideCombo.setSelectedId(3, juce::dontSendNotification);
    strideCombo.onChange = [this] { updateEstimateLabel(); };
    addAndMakeVisible(strideCombo);

    styleLabel(velLayersLabel);
    for (int v = 1; v <= 8; ++v)
        velLayersCombo.addItem(juce::String(v) + (v == 1 ? " Layer" : " Layers"), v);
    velLayersCombo.setSelectedId(1, juce::dontSendNotification);
    velLayersCombo.onChange = [this] { updateEstimateLabel(); };
    addAndMakeVisible(velLayersCombo);

    styleLabel(rrLabel);
    for (int r = 1; r <= 8; ++r)
        rrCombo.addItem(juce::String(r) + (r == 1 ? " Take (Off)" : " RR"), r);
    rrCombo.setSelectedId(1, juce::dontSendNotification);
    rrCombo.onChange = [this] { updateEstimateLabel(); };
    addAndMakeVisible(rrCombo);

    styleLabel(sustainLabel);
    sustainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    sustainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 46, 20);
    sustainSlider.setRange(0.2, 10.0, 0.1);
    sustainSlider.setValue(1.5, juce::dontSendNotification);
    sustainSlider.setTextValueSuffix("s");
    addAndMakeVisible(sustainSlider);

    styleLabel(tailLabel);
    tailSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    tailSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 46, 20);
    tailSlider.setRange(0.2, 15.0, 0.1);
    tailSlider.setValue(3.0, juce::dontSendNotification);
    tailSlider.setTextValueSuffix("s");
    addAndMakeVisible(tailSlider);

    styleLabel(srLabel);
    srCombo.addItem("44.1 kHz", 44100);
    srCombo.addItem("48 kHz", 48000);
    srCombo.addItem("96 kHz", 96000);
    srCombo.setSelectedId(44100, juce::dontSendNotification);
    addAndMakeVisible(srCombo);

    styleLabel(normLabel);
    normCombo.addItem("Off", 1);
    normCombo.addItem("Group Peak (-1 dB)", 2);
    normCombo.addItem("Per-Sample (-1 dB)", 3);
    normCombo.setSelectedId(2, juce::dontSendNotification);
    addAndMakeVisible(normCombo);

    autoTailToggle.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(autoTailToggle);

    trimSilenceToggle.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(trimSilenceToggle);

    exportSfzToggle.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(exportSfzToggle);

    captureReleaseToggle.setToggleState(false, juce::dontSendNotification);
    captureReleaseToggle.setTooltip("Record the post-Note-Off release sound of the VST as separate Release Trigger zones");
    addAndMakeVisible(captureReleaseToggle);

    estimateLabel.setFont(juce::Font(12.0f, juce::Font::bold));
    estimateLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    addAndMakeVisible(estimateLabel);

    addAndMakeVisible(progressBar);

    statusLabel.setFont(juce::Font(11.5f));
    statusLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    addAndMakeVisible(statusLabel);

    startBtn.setEnabled(false);
    startBtn.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::accentCyan.withAlpha(0.28f));
    startBtn.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::accentCyan);
    startBtn.onClick = [this] { startSampling(); };
    addAndMakeVisible(startBtn);

    cancelBtn.onClick = [this] {
        if (engine.isSampling())
        {
            engine.cancelSampling();
            statusLabel.setText("Cancelling...", juce::dontSendNotification);
        }
        else if (engine.isScanning())
        {
            engine.cancelScan();
            progressValue = 0.0;
            scanPluginsBtn.setEnabled(true);
            refreshPluginCombo();
            startBtn.setEnabled(loadedPlugin != nullptr);
            statusLabel.setText("Scan cancelled.", juce::dontSendNotification);
        }
        else
        {
            requestClose();
        }
    };
    addAndMakeVisible(cancelBtn);

    refreshPluginCombo();
    updateEstimateLabel();
}

AutoSamplerDialog::~AutoSamplerDialog()
{
    engine.cancelScan();
    engine.stopSamplingAndWait();
    pluginEditorWindow.reset();
    loadedPlugin.reset();
}

void AutoSamplerDialog::refreshPluginCombo()
{
    instrumentPlugins = engine.getInstrumentPlugins();
    pluginCombo.clear(juce::dontSendNotification);
    for (int i = 0; i < static_cast<int>(instrumentPlugins.size()); ++i)
    {
        const auto& d = instrumentPlugins[static_cast<size_t>(i)];
        pluginCombo.addItem(d.name + " (" + d.pluginFormatName + ")", i + 1);
    }
    if (!instrumentPlugins.empty())
        statusLabel.setText("Select an instrument plugin or click Scan to refresh.", juce::dontSendNotification);
}

void AutoSamplerDialog::loadSelectedPlugin()
{
    int idx = pluginCombo.getSelectedId() - 1;
    if (idx < 0 || idx >= static_cast<int>(instrumentPlugins.size()))
        return;

    pluginEditorWindow.reset();
    loadedPlugin.reset();
    openEditorBtn.setEnabled(false);
    startBtn.setEnabled(false);

    const auto& desc = instrumentPlugins[static_cast<size_t>(idx)];
    double sr = static_cast<double>(srCombo.getSelectedId() > 0 ? srCombo.getSelectedId() : 44100);
    juce::String err;
    loadedPlugin = engine.createPluginInstance(desc, sr, 512, err);

    if (loadedPlugin != nullptr)
    {
        nameEditor.setText(desc.name, juce::dontSendNotification);
        openEditorBtn.setEnabled(loadedPlugin->hasEditor());
        startBtn.setEnabled(true);
        statusLabel.setText("Loaded: " + desc.name + " (" + desc.pluginFormatName + ")", juce::dontSendNotification);
    }
    else
    {
        statusLabel.setText("Failed to load " + desc.name + ": " + err, juce::dontSendNotification);
    }
}

void AutoSamplerDialog::openPluginEditor()
{
    if (loadedPlugin == nullptr || !loadedPlugin->hasEditor())
        return;

    if (pluginEditorWindow != nullptr)
    {
        pluginEditorWindow->setVisible(true);
        pluginEditorWindow->toFront(true);
        return;
    }

    if (auto* ed = loadedPlugin->createEditorIfNeeded())
    {
        pluginEditorWindow = std::make_unique<PluginEditorWindow>(loadedPlugin->getName(), ed);
    }
}

void AutoSamplerDialog::updateEstimateLabel()
{
    int lo = std::max(0, minNoteCombo.getSelectedId() - 1);
    int hi = std::max(0, maxNoteCombo.getSelectedId() - 1);
    if (hi < lo) std::swap(lo, hi);
    int stride = std::max(1, strideCombo.getSelectedId());
    int noteCount = ((hi - lo) / stride) + 1;
    int vels = std::max(1, velLayersCombo.getSelectedId());
    int rrs = std::max(1, rrCombo.getSelectedId());
    int total = noteCount * vels * rrs;

    estimateLabel.setText("Capture Plan: " + juce::String(noteCount) + " notes x "
                              + juce::String(vels) + " vel x " + juce::String(rrs)
                              + " RR = " + juce::String(total) + " WAV samples",
                          juce::dontSendNotification);
}

void AutoSamplerDialog::setControlsEnabled(bool enabled)
{
    pluginCombo.setEnabled(enabled);
    scanPluginsBtn.setEnabled(enabled);
    openEditorBtn.setEnabled(enabled && loadedPlugin != nullptr && loadedPlugin->hasEditor());
    nameEditor.setEnabled(enabled);
    minNoteCombo.setEnabled(enabled);
    maxNoteCombo.setEnabled(enabled);
    strideCombo.setEnabled(enabled);
    velLayersCombo.setEnabled(enabled);
    rrCombo.setEnabled(enabled);
    sustainSlider.setEnabled(enabled);
    tailSlider.setEnabled(enabled);
    srCombo.setEnabled(enabled);
    normCombo.setEnabled(enabled);
    autoTailToggle.setEnabled(enabled);
    trimSilenceToggle.setEnabled(enabled);
    exportSfzToggle.setEnabled(enabled);
    captureReleaseToggle.setEnabled(enabled);
    startBtn.setEnabled(enabled && loadedPlugin != nullptr);
    cancelBtn.setButtonText(enabled ? "Close" : "Cancel");
}

void AutoSamplerDialog::startSampling()
{
    if (loadedPlugin == nullptr || engine.isSampling())
        return;

    AutoSamplerConfig cfg;
    cfg.instrumentName = nameEditor.getText().trim();
    if (cfg.instrumentName.isEmpty())
        cfg.instrumentName = loadedPlugin->getName();

    cfg.minNote = std::max(0, minNoteCombo.getSelectedId() - 1);
    cfg.maxNote = std::max(0, maxNoteCombo.getSelectedId() - 1);
    cfg.semitoneStride = std::max(1, strideCombo.getSelectedId());
    cfg.velocityLayers = std::max(1, velLayersCombo.getSelectedId());
    cfg.roundRobinCount = std::max(1, rrCombo.getSelectedId());
    cfg.sustainSeconds = static_cast<float>(sustainSlider.getValue());
    cfg.maxTailSeconds = static_cast<float>(tailSlider.getValue());
    cfg.sampleRate = static_cast<double>(srCombo.getSelectedId() > 0 ? srCombo.getSelectedId() : 44100);
    cfg.normalizeMode = std::max(0, normCombo.getSelectedId() - 1);
    cfg.autoTailDetect = autoTailToggle.getToggleState();
    cfg.trimLeadingSilence = trimSilenceToggle.getToggleState();
    cfg.exportSfz = exportSfzToggle.getToggleState();
    cfg.captureReleaseTriggers = captureReleaseToggle.getToggleState();

    setControlsEnabled(false);
    progressValue = 0.0;

    juce::Component::SafePointer<AutoSamplerDialog> safeThis(this);
    engine.startAutoSample(
        loadedPlugin.get(),
        cfg,
        [safeThis](float prog, const juce::String& status) {
            if (safeThis == nullptr) return;
            safeThis->progressValue = static_cast<double>(prog);
            safeThis->statusLabel.setText(status, juce::dontSendNotification);
        },
        [safeThis](const SampleMapState& resultState, bool success, const juce::String& err) {
            if (safeThis == nullptr) return;
            safeThis->setControlsEnabled(true);
            safeThis->progressValue = success ? 1.0 : 0.0;

            if (success)
            {
                safeThis->statusLabel.setText(
                    "Captured " + juce::String(resultState.zones.size()) + " zones into Sampler!",
                    juce::dontSendNotification);
                if (safeThis->onCompleteCallback)
                    safeThis->onCompleteCallback(resultState);
                safeThis->requestClose();
            }
            else
            {
                safeThis->statusLabel.setText(err, juce::dontSendNotification);
            }
        });
}

void AutoSamplerDialog::paint(juce::Graphics& g)
{
    g.fillAll(OpenWavLookAndFeel::bgDark);
}

void AutoSamplerDialog::resized()
{
    auto area = getLocalBounds().reduced(18, 16);
    titleLabel.setBounds(area.removeFromTop(26));
    area.removeFromTop(10);

    auto rowH = 26;
    auto gap = 8;

    // Plugin row
    {
        auto r = area.removeFromTop(rowH);
        pluginLabel.setBounds(r.removeFromLeft(88));
        openEditorBtn.setBounds(r.removeFromRight(76));
        r.removeFromRight(6);
        scanPluginsBtn.setBounds(r.removeFromRight(64));
        r.removeFromRight(6);
        pluginCombo.setBounds(r);
    }
    area.removeFromTop(gap);

    // Name & Template row
    {
        auto r = area.removeFromTop(rowH);
        nameLabel.setBounds(r.removeFromLeft(88));
        templateCombo.setBounds(r.removeFromRight(175));
        r.removeFromRight(6);
        templateLabel.setBounds(r.removeFromRight(68));
        r.removeFromRight(8);
        nameEditor.setBounds(r);
    }
    area.removeFromTop(gap);

    // Key range & stride row
    {
        auto r = area.removeFromTop(rowH);
        minNoteLabel.setBounds(r.removeFromLeft(62));
        minNoteCombo.setBounds(r.removeFromLeft(105));
        r.removeFromLeft(10);
        maxNoteLabel.setBounds(r.removeFromLeft(64));
        maxNoteCombo.setBounds(r.removeFromLeft(105));
        r.removeFromLeft(10);
        strideLabel.setBounds(r.removeFromLeft(48));
        strideCombo.setBounds(r);
    }
    area.removeFromTop(gap);

    // Velocity & RR row
    {
        auto r = area.removeFromTop(rowH);
        velLayersLabel.setBounds(r.removeFromLeft(88));
        velLayersCombo.setBounds(r.removeFromLeft(160));
        r.removeFromLeft(16);
        rrLabel.setBounds(r.removeFromLeft(88));
        rrCombo.setBounds(r);
    }
    area.removeFromTop(gap);

    // Sustain & Tail row
    {
        auto r = area.removeFromTop(rowH);
        sustainLabel.setBounds(r.removeFromLeft(88));
        sustainSlider.setBounds(r.removeFromLeft(165));
        r.removeFromLeft(16);
        tailLabel.setBounds(r.removeFromLeft(84));
        tailSlider.setBounds(r);
    }
    area.removeFromTop(gap);

    // Sample rate & Normalize row
    {
        auto r = area.removeFromTop(rowH);
        srLabel.setBounds(r.removeFromLeft(88));
        srCombo.setBounds(r.removeFromLeft(160));
        r.removeFromLeft(16);
        normLabel.setBounds(r.removeFromLeft(78));
        normCombo.setBounds(r);
    }
    area.removeFromTop(gap + 2);

    // Checkboxes row
    {
        auto r = area.removeFromTop(24);
        autoTailToggle.setBounds(r.removeFromLeft(175));
        trimSilenceToggle.setBounds(r.removeFromLeft(160));
        exportSfzToggle.setBounds(r.removeFromLeft(95));
        captureReleaseToggle.setBounds(r);
    }
    area.removeFromTop(10);

    estimateLabel.setBounds(area.removeFromTop(20));
    area.removeFromTop(6);
    progressBar.setBounds(area.removeFromTop(18));
    area.removeFromTop(6);
    statusLabel.setBounds(area.removeFromTop(20));

    auto bottom = area.removeFromBottom(32);
    cancelBtn.setBounds(bottom.removeFromRight(90));
    bottom.removeFromRight(10);
    startBtn.setBounds(bottom.removeFromRight(150));
}

namespace
{
// Non-modal top-level window for AutoSamplerDialog.
// Using DialogWindow::LaunchOptions::launchAsync() makes the dialog modal, which
// causes JUCE's Windows HWNDComponentPeer to reject clicks (MA_NOACTIVATEANDEAT)
// on any separate top-level VST3 plugin editor window (e.g. Plogue sforzando).
class AutoSamplerWindow : public juce::DocumentWindow
{
public:
    AutoSamplerWindow(AutoSamplerDialog* content, juce::Component* centreAround)
        : DocumentWindow("OWMB Auto-Sampler (VST3 / AU)",
                         OpenWavLookAndFeel::bgDark,
                         DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar(true);
        setResizable(false, false);
        setContentOwned(content, true);

        if (centreAround != nullptr)
            centreAroundComponent(centreAround, getWidth(), getHeight());
        else
            centreWithSize(getWidth(), getHeight());

        setVisible(true);
        toFront(true);
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey)
        {
            closeButtonPressed();
            return true;
        }
        return DocumentWindow::keyPressed(key);
    }

    void closeButtonPressed() override
    {
        if (closing)
            return;
        closing = true;
        setVisible(false);

        juce::Component::SafePointer<AutoSamplerWindow> safeThis(this);
        juce::MessageManager::callAsync([safeThis]() {
            if (safeThis != nullptr)
                delete safeThis.getComponent();
        });
    }

private:
    bool closing { false };
};

juce::Component::SafePointer<AutoSamplerWindow> activeAutoSamplerWindow;
} // namespace

void AutoSamplerDialog::requestClose()
{
    if (auto* dw = findParentComponentOfClass<juce::DocumentWindow>())
        dw->closeButtonPressed();
}

void AutoSamplerDialog::closeActiveWindow()
{
    if (activeAutoSamplerWindow != nullptr)
    {
        auto* win = activeAutoSamplerWindow.getComponent();
        activeAutoSamplerWindow = nullptr;
        delete win;
    }
}

void AutoSamplerDialog::setTargetContext(std::function<void(const SampleMapState& resultState)> onComplete,
                                         const juce::String& targetLabel)
{
    if (engine.isSampling())
        return;

    onCompleteCallback = std::move(onComplete);
    titleLabel.setText(targetLabel.isNotEmpty()
                           ? ("VST3 / AU Auto-Sampler - " + targetLabel)
                           : "VST3 / AU Auto-Sampler",
                       juce::dontSendNotification);
    if (auto* dw = findParentComponentOfClass<juce::DocumentWindow>())
    {
        dw->setName(targetLabel.isNotEmpty()
                        ? ("OWMB Auto-Sampler - " + targetLabel)
                        : "OWMB Auto-Sampler (VST3 / AU)");
    }
}

void AutoSamplerDialog::showAsync(juce::Component* parent,
                                  std::function<void(const SampleMapState& resultState)> onComplete,
                                  const juce::String& targetLabel)
{
    if (activeAutoSamplerWindow != nullptr && activeAutoSamplerWindow->isVisible())
    {
        if (auto* dlg = dynamic_cast<AutoSamplerDialog*>(activeAutoSamplerWindow->getContentComponent()))
            dlg->setTargetContext(std::move(onComplete), targetLabel);
        activeAutoSamplerWindow->toFront(true);
        return;
    }

    closeActiveWindow();
    auto* content = new AutoSamplerDialog(std::move(onComplete), targetLabel);
    activeAutoSamplerWindow = new AutoSamplerWindow(content, parent);
    if (targetLabel.isNotEmpty() && activeAutoSamplerWindow != nullptr)
        activeAutoSamplerWindow->setName("OWMB Auto-Sampler - " + targetLabel);
}

} // namespace openwav
