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

    addAndMakeVisible(viewModeToggleButton);
    viewModeToggleButton.addListener(this);
    viewModeToggleButton.setTooltip("Switch between Custom Preset UI and Studio Macro Controls");

    addAndMakeVisible(loadPresetButton);
    loadPresetButton.addListener(this);
    loadPresetButton.setTooltip("Load a .dspreset, .dslibrary, or sample map file");

    addAndMakeVisible(editMapButton);
    editMapButton.addListener(this);
    editMapButton.setTooltip("Open in Sample Map Editor");

    addAndMakeVisible(allNotesOffButton);
    allNotesOffButton.addListener(this);
    allNotesOffButton.setTooltip("Stop all active voices / Panic");

    // ── Custom Decent Sampler Canvas View ───────────────
    addChildComponent(customCanvas);
    customCanvas.onStateChanged = [this](const SampleMapState& updatedState) {
        currentState = updatedState;
        syncUiFromState();
        if (onStateChanged)
            onStateChanged(currentState);
    };

    // ── Standard ADSR Group ────────────────────────────
    addChildComponent(adsrGroup);
    adsrGroup.setColour(juce::GroupComponent::outlineColourId, OpenWavLookAndFeel::borderColour);
    adsrGroup.setColour(juce::GroupComponent::textColourId, OpenWavLookAndFeel::accentCyan);

    auto setupRotary = [this](juce::Slider& s, juce::Label& l, double minVal, double maxVal, double defVal, const juce::String& suffix) {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 65, 18);
        s.setRange(minVal, maxVal, 0.1);
        s.setValue(defVal, juce::dontSendNotification);
        s.setTextValueSuffix(suffix);
        s.addListener(this);
        addChildComponent(s);

        l.setFont(juce::Font(12.0f).boldened());
        l.setJustificationType(juce::Justification::centred);
        l.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
        addChildComponent(l);
    };

    setupRotary(attackSlider, attackLabel, 0.1, 5000.0, 5.0, " ms");
    setupRotary(decaySlider, decayLabel, 1.0, 10000.0, 100.0, " ms");
    setupRotary(sustainSlider, sustainLabel, 0.0, 1.0, 1.0, "");
    setupRotary(releaseSlider, releaseLabel, 1.0, 10000.0, 200.0, " ms");

    attackSlider.setSkewFactorFromMidPoint(500.0);
    decaySlider.setSkewFactorFromMidPoint(1000.0);
    releaseSlider.setSkewFactorFromMidPoint(1000.0);

    // ── Sound & Performance Group ──────────────────────
    addChildComponent(soundGroup);
    soundGroup.setColour(juce::GroupComponent::outlineColourId, OpenWavLookAndFeel::borderColour);
    soundGroup.setColour(juce::GroupComponent::textColourId, OpenWavLookAndFeel::accentCyan);

    setupRotary(volumeSlider, volumeLabel, -36.0, 12.0, 0.0, " dB");
    setupRotary(reverbSlider, reverbLabel, 0.0, 100.0, 0.0, " %");
    setupRotary(toneSlider, toneLabel, 20.0, 20000.0, 20000.0, " Hz");
    setupRotary(tuneSlider, tuneLabel, -100.0, 100.0, 0.0, " ct");

    toneSlider.setSkewFactorFromMidPoint(1000.0);

    // Toggles
    roundRobinButton.onClick = [this] {
        currentState.roundRobinMode = (currentState.roundRobinMode + 1) % 3;
        roundRobinButton.setButtonText(currentState.roundRobinMode == 0 ? "RR: Cycle" : (currentState.roundRobinMode == 1 ? "RR: Random" : "RR: OFF"));
        if (onStateChanged) onStateChanged(currentState);
    };
    addChildComponent(roundRobinButton);

    pitchTrackButton.setClickingTogglesState(true);
    pitchTrackButton.setToggleState(true, juce::dontSendNotification);
    pitchTrackButton.onClick = [this] {
        bool enabled = pitchTrackButton.getToggleState();
        audioEngine.setPitchTrackingEnabled(enabled);
        currentState.pitchTrackingEnabled = enabled;
        pitchTrackButton.setButtonText(enabled ? "Pitch Track: ON" : "Pitch Track: OFF");
        if (onStateChanged) onStateChanged(currentState);
    };
    addChildComponent(pitchTrackButton);

    oneShotButton.setClickingTogglesState(true);
    oneShotButton.setToggleState(audioEngine.isOneShotEnabled(), juce::dontSendNotification);
    oneShotButton.setButtonText(audioEngine.isOneShotEnabled() ? "One Shot: ON" : "One Shot: OFF");
    oneShotButton.onClick = [this] {
        bool enabled = oneShotButton.getToggleState();
        audioEngine.setOneShotEnabled(enabled);
        oneShotButton.setButtonText(enabled ? "One Shot: ON" : "One Shot: OFF");
    };
    addChildComponent(oneShotButton);

    loopButton.setClickingTogglesState(true);
    loopButton.setToggleState(audioEngine.isLooping(), juce::dontSendNotification);
    loopButton.setButtonText(audioEngine.isLooping() ? "Loop: ON" : "Loop: OFF");
    loopButton.onClick = [this] {
        bool enabled = loopButton.getToggleState();
        audioEngine.setLooping(enabled);
        loopButton.setButtonText(enabled ? "Loop: ON" : "Loop: OFF");
    };
    addChildComponent(loopButton);

    // ── Custom Preset Controls Group ───────────────────
    addChildComponent(customGroup);
    customGroup.setColour(juce::GroupComponent::outlineColourId, OpenWavLookAndFeel::borderColour);
    customGroup.setColour(juce::GroupComponent::textColourId, OpenWavLookAndFeel::accentCyan);

    addChildComponent(addCustomControlButton);
    addCustomControlButton.addListener(this);
    addCustomControlButton.setTooltip("Add a new custom control or macro slider");

    updateViewModeVisibility();
    startTimerHz(30);
}

PlayComponent::~PlayComponent()
{
    stopTimer();
}

void PlayComponent::timerCallback()
{
    float lfoOut = audioEngine.getCurrentLfoOutput(); // -1.0 to 1.0

    if (currentState.modulators.empty() && std::abs(lfoOut) < 0.0001f)
        return;

    for (const auto& m : currentState.modulators)
    {
        juce::String tgt = m.target.toLowerCase().trim();
        if (tgt.isEmpty()) tgt = "cutoff";

        for (auto& cs : customSliders)
        {
            if (cs.decentControl != nullptr)
            {
                bool matches = cs.model.id.equalsIgnoreCase(m.target) ||
                               cs.model.label.equalsIgnoreCase(m.target) ||
                               cs.model.parameterName.equalsIgnoreCase(m.target) ||
                               cs.model.bindingParam.equalsIgnoreCase(m.target);
                if (!matches)
                {
                    for (const auto& b : cs.model.bindings)
                    {
                        if (b.parameter.equalsIgnoreCase(m.target) || b.type.equalsIgnoreCase(m.target))
                        {
                            matches = true;
                            break;
                        }
                    }
                }
                if (!matches)
                {
                    if ((tgt.contains("filter") || tgt.contains("cutoff") || tgt.contains("tone")) &&
                        (cs.model.id.containsIgnoreCase("tone") || cs.model.id.containsIgnoreCase("cutoff") ||
                         cs.model.label.containsIgnoreCase("tone") || cs.model.label.containsIgnoreCase("cutoff") ||
                         cs.model.bindingParam.containsIgnoreCase("cutoff") || cs.model.bindingParam.containsIgnoreCase("filter")))
                    {
                        matches = true;
                    }
                    else if ((tgt.contains("volume") || tgt.contains("gain") || tgt.contains("level") || tgt.contains("amp")) &&
                             (cs.model.id.containsIgnoreCase("vol") || cs.model.label.containsIgnoreCase("vol") ||
                              cs.model.id.containsIgnoreCase("gain") || cs.model.label.containsIgnoreCase("gain")))
                    {
                        matches = true;
                    }
                    else if (tgt.contains("pan") && (cs.model.id.containsIgnoreCase("pan") || cs.model.label.containsIgnoreCase("pan")))
                    {
                        matches = true;
                    }
                }

                if (matches)
                {
                    double range = std::max(0.0001, cs.model.maxValue - cs.model.minValue);
                    double offset = lfoOut * range * 0.4;
                    cs.decentControl->setVisualModulationOffset(offset);
                    applyCustomControlBinding(cs.model, juce::jlimit(cs.model.minValue, cs.model.maxValue, cs.model.currentValue + offset));
                }
            }
        }
    }
}

void PlayComponent::lookAndFeelChanged()
{
    instrumentTitleLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    instrumentInfoLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan.withAlpha(0.9f));

    adsrGroup.setColour(juce::GroupComponent::outlineColourId, OpenWavLookAndFeel::borderColour);
    adsrGroup.setColour(juce::GroupComponent::textColourId, OpenWavLookAndFeel::accentCyan);

    soundGroup.setColour(juce::GroupComponent::outlineColourId, OpenWavLookAndFeel::borderColour);
    soundGroup.setColour(juce::GroupComponent::textColourId, OpenWavLookAndFeel::accentCyan);

    customGroup.setColour(juce::GroupComponent::outlineColourId, OpenWavLookAndFeel::borderColour);
    customGroup.setColour(juce::GroupComponent::textColourId, OpenWavLookAndFeel::accentCyan);

    attackLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    decayLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    sustainLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    releaseLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);

    volumeLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    reverbLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    toneLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    tuneLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);

    for (auto& cs : customSliders)
    {
        if (cs.label != nullptr)
            cs.label->setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    }

    viewModeToggleButton.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::bgCard);
    viewModeToggleButton.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::accentCyan);
    viewModeToggleButton.setColour(juce::TextButton::textColourOnId, OpenWavLookAndFeel::accentCyan);

    repaint();
}

void PlayComponent::updateViewModeVisibility()
{
    viewModeToggleButton.setVisible(true);

    if (showCustomUiCanvas)
    {
        viewModeToggleButton.setButtonText("Studio Macro");
        customCanvas.setVisible(true);

        adsrGroup.setVisible(false);
        attackSlider.setVisible(false); attackLabel.setVisible(false);
        decaySlider.setVisible(false); decayLabel.setVisible(false);
        sustainSlider.setVisible(false); sustainLabel.setVisible(false);
        releaseSlider.setVisible(false); releaseLabel.setVisible(false);

        soundGroup.setVisible(false);
        volumeSlider.setVisible(false); volumeLabel.setVisible(false);
        reverbSlider.setVisible(false); reverbLabel.setVisible(false);
        toneSlider.setVisible(false); toneLabel.setVisible(false);
        tuneSlider.setVisible(false); tuneLabel.setVisible(false);
        roundRobinButton.setVisible(false);
        pitchTrackButton.setVisible(false);
        oneShotButton.setVisible(false);
        loopButton.setVisible(false);

        customGroup.setVisible(false);
        addCustomControlButton.setVisible(false);
        for (auto& cs : customSliders)
        {
            if (cs.decentControl) cs.decentControl->setVisible(false);
            if (cs.slider) cs.slider->setVisible(false);
            if (cs.label) cs.label->setVisible(false);
        }
    }
    else
    {
        viewModeToggleButton.setButtonText("Custom UI");
        customCanvas.setVisible(false);

        adsrGroup.setVisible(true);
        attackSlider.setVisible(true); attackLabel.setVisible(true);
        decaySlider.setVisible(true); decayLabel.setVisible(true);
        sustainSlider.setVisible(true); sustainLabel.setVisible(true);
        releaseSlider.setVisible(true); releaseLabel.setVisible(true);

        soundGroup.setVisible(true);
        volumeSlider.setVisible(true); volumeLabel.setVisible(true);
        reverbSlider.setVisible(true); reverbLabel.setVisible(true);
        toneSlider.setVisible(true); toneLabel.setVisible(true);
        tuneSlider.setVisible(true); tuneLabel.setVisible(true);
        roundRobinButton.setVisible(true);
        pitchTrackButton.setVisible(true);
        oneShotButton.setVisible(true);
        loopButton.setVisible(true);

        customGroup.setVisible(true);
        addCustomControlButton.setVisible(true);
        for (auto& cs : customSliders)
        {
            if (cs.decentControl)
            {
                cs.decentControl->setVisible(true);
            }
            else
            {
                if (cs.slider) cs.slider->setVisible(true);
                if (cs.label) cs.label->setVisible(true);
            }
        }
    }

    resized();
    repaint();
}

void PlayComponent::drawAdsrCurve(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    g.setColour(OpenWavLookAndFeel::bgCard.darker(0.3f));
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(OpenWavLookAndFeel::borderColour);
    g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

    if (bounds.getWidth() < 40 || bounds.getHeight() < 20)
        return;

    auto r = bounds.reduced(12.0f, 10.0f);
    float att = static_cast<float>(attackSlider.getValue());
    float dec = static_cast<float>(decaySlider.getValue());
    float sus = static_cast<float>(sustainSlider.getValue());
    float rel = static_cast<float>(releaseSlider.getValue());

    float totalTime = std::max(10.0f, att + dec + 500.0f + rel);
    float x0 = r.getX();
    float x1 = x0 + (att / totalTime) * r.getWidth();
    float x2 = x1 + (dec / totalTime) * r.getWidth();
    float x3 = x2 + (500.0f / totalTime) * r.getWidth();
    float x4 = r.getRight();

    float yBottom = r.getBottom();
    float yTop = r.getY();
    float ySus = yBottom - sus * (r.getHeight());

    juce::Path p;
    p.startNewSubPath(x0, yBottom);
    p.lineTo(x1, yTop);
    p.lineTo(x2, ySus);
    p.lineTo(x3, ySus);
    p.lineTo(x4, yBottom);

    // Fill with gradient
    juce::ColourGradient grad(OpenWavLookAndFeel::accentCyan.withAlpha(0.35f), x0, yTop,
                              OpenWavLookAndFeel::accentCyan.withAlpha(0.02f), x0, yBottom, false);
    g.setGradientFill(grad);
    g.fillPath(p);

    // Stroke
    g.setColour(OpenWavLookAndFeel::accentCyan);
    g.strokePath(p, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void PlayComponent::paint(juce::Graphics& g)
{
    g.fillAll(OpenWavLookAndFeel::bgDark);

    if (!showCustomUiCanvas)
    {
        // ADSR visualizer rect inside adsrGroup
        auto adsrArea = adsrGroup.getBounds().toFloat();
        auto vizRect = adsrArea.removeFromBottom(adsrArea.getHeight() * 0.46f).reduced(12.0f, 10.0f);
        drawAdsrCurve(g, vizRect);
    }
}

void PlayComponent::resized()
{
    auto area = getLocalBounds().reduced(20, 16);

    // 1. Top Header Row
    auto headerRow = area.removeFromTop(38);
    loadPresetButton.setBounds(headerRow.removeFromRight(100).reduced(0, 4));
    headerRow.removeFromRight(8);
    editMapButton.setBounds(headerRow.removeFromRight(90).reduced(0, 4));
    headerRow.removeFromRight(8);
    allNotesOffButton.setBounds(headerRow.removeFromRight(65).reduced(0, 4));
    headerRow.removeFromRight(8);

    viewModeToggleButton.setBounds(headerRow.removeFromRight(120).reduced(0, 4));
    headerRow.removeFromRight(14);

    int titleW = std::min(300, headerRow.getWidth() / 2);
    instrumentTitleLabel.setBounds(headerRow.removeFromLeft(titleW));
    instrumentInfoLabel.setBounds(headerRow.reduced(0, 4));

    area.removeFromTop(12);

    // 2. Main Content Area
    if (showCustomUiCanvas)
    {
        customCanvas.setBounds(area);
        return;
    }

    // Studio Macro Controls Layout
    int totalWidth = area.getWidth();
    bool hasCustomControls = !currentState.uiControls.empty();
    int gap = 14;

    juce::Rectangle<int> adsrRect;
    juce::Rectangle<int> soundRect;
    juce::Rectangle<int> customRect;

    if (hasCustomControls)
    {
        int availW = totalWidth - gap * 2;
        int adsrW = static_cast<int>(availW * 0.32f);
        int soundW = static_cast<int>(availW * 0.36f);
        int customW = availW - adsrW - soundW;

        adsrRect = area.removeFromLeft(adsrW);
        area.removeFromLeft(gap);
        soundRect = area.removeFromLeft(soundW);
        area.removeFromLeft(gap);
        customRect = area;
    }
    else
    {
        int availW = totalWidth - gap;
        int halfW = availW / 2;
        adsrRect = area.removeFromLeft(halfW);
        area.removeFromLeft(gap);
        soundRect = area;
    }

    // Layout ADSR group
    adsrGroup.setBounds(adsrRect);
    auto innerAdsr = adsrRect.reduced(12, 24);
    auto knobsRow = innerAdsr.removeFromTop(innerAdsr.getHeight() * 0.52f);

    int knobW = knobsRow.getWidth() / 4;
    auto k1 = knobsRow.removeFromLeft(knobW);
    attackLabel.setBounds(k1.removeFromTop(20));
    attackSlider.setBounds(k1);

    auto k2 = knobsRow.removeFromLeft(knobW);
    decayLabel.setBounds(k2.removeFromTop(20));
    decaySlider.setBounds(k2);

    auto k3 = knobsRow.removeFromLeft(knobW);
    sustainLabel.setBounds(k3.removeFromTop(20));
    sustainSlider.setBounds(k3);

    auto k4 = knobsRow;
    releaseLabel.setBounds(k4.removeFromTop(20));
    releaseSlider.setBounds(k4);

    // Layout Sound group
    soundGroup.setBounds(soundRect);
    auto innerSound = soundRect.reduced(12, 24);
    auto soundKnobsRow = innerSound.removeFromTop(innerSound.getHeight() * 0.52f);

    int sKnobW = soundKnobsRow.getWidth() / 4;
    auto sk1 = soundKnobsRow.removeFromLeft(sKnobW);
    volumeLabel.setBounds(sk1.removeFromTop(20));
    volumeSlider.setBounds(sk1);

    auto sk2 = soundKnobsRow.removeFromLeft(sKnobW);
    reverbLabel.setBounds(sk2.removeFromTop(20));
    reverbSlider.setBounds(sk2);

    auto sk3 = soundKnobsRow.removeFromLeft(sKnobW);
    toneLabel.setBounds(sk3.removeFromTop(20));
    toneSlider.setBounds(sk3);

    auto sk4 = soundKnobsRow;
    tuneLabel.setBounds(sk4.removeFromTop(20));
    tuneSlider.setBounds(sk4);

    auto togglesRow = innerSound.reduced(6, 8);
    int toggleW = (togglesRow.getWidth() - 18) / 4;
    roundRobinButton.setBounds(togglesRow.removeFromLeft(toggleW));
    togglesRow.removeFromLeft(6);
    pitchTrackButton.setBounds(togglesRow.removeFromLeft(toggleW));
    togglesRow.removeFromLeft(6);
    oneShotButton.setBounds(togglesRow.removeFromLeft(toggleW));
    togglesRow.removeFromLeft(6);
    loopButton.setBounds(togglesRow);

    // Layout Custom Controls
    if (hasCustomControls)
    {
        customGroup.setBounds(customRect);
        addCustomControlButton.setBounds(customRect.getRight() - 110, customRect.getY() + 3, 95, 20);

        auto innerCustom = customRect.reduced(12, 26);
        int numCustom = static_cast<int>(customSliders.size());
        if (numCustom > 0)
        {
            int cols = std::min(numCustom, (innerCustom.getWidth() > 380) ? 4 : 3);
            int itemW = innerCustom.getWidth() / cols;
            int numRows = std::max(1, (numCustom + cols - 1) / cols);
            int rowH = innerCustom.getHeight() / numRows;

            for (int i = 0; i < numCustom; ++i)
            {
                int col = i % cols;
                int row = i / cols;
                auto cell = juce::Rectangle<int>(innerCustom.getX() + col * itemW,
                                                 innerCustom.getY() + row * rowH,
                                                 itemW, rowH).reduced(4);
                if (customSliders[i].decentControl != nullptr)
                {
                    customSliders[i].decentControl->setBounds(cell);
                    customSliders[i].decentControl->setScale(1.0f);
                }
                else
                {
                    if (customSliders[i].label != nullptr)
                        customSliders[i].label->setBounds(cell.removeFromTop(20));
                    if (customSliders[i].slider != nullptr)
                        customSliders[i].slider->setBounds(cell);
                }
            }
        }
    }
}

void PlayComponent::applyCustomControlBinding(const DecentSamplerUiControl& ctrl, double value)
{
    juce::String bp = ctrl.bindingParam;
    juce::String lbl = ctrl.label;

    if (!ctrl.bindings.empty())
    {
        for (const auto& b : ctrl.bindings)
        {
            if (b.level.equalsIgnoreCase("group"))
            {
                int grpIdx = b.position;
                if (b.parameter.containsIgnoreCase("volume") || b.parameter.containsIgnoreCase("gain") || b.parameter.containsIgnoreCase("AMP_VOLUME"))
                {
                    float volDb = 0.0f;
                    if (value <= 0.0001) volDb = -96.0f;
                    else if (value <= 1.0 && value >= 0.0) volDb = 20.0f * std::log10(static_cast<float>(value));
                    else volDb = static_cast<float>(value);
                    audioEngine.setGroupVolumeDb(grpIdx, volDb);
                    if (grpIdx >= 0 && grpIdx < static_cast<int>(currentState.groups.size()))
                        currentState.groups[grpIdx].volumeDb = volDb;
                }
                else if (b.parameter.containsIgnoreCase("pan"))
                {
                    audioEngine.setGroupPan(grpIdx, static_cast<float>(value));
                }
            }
            else if (b.level.equalsIgnoreCase("effect"))
            {
                int fxIdx = b.position;
                if (fxIdx >= 0 && fxIdx < static_cast<int>(currentState.effects.size()))
                {
                    auto& eff = currentState.effects[fxIdx];
                    if (b.parameter.containsIgnoreCase("WET") || b.parameter.containsIgnoreCase("wetLevel") || b.parameter.containsIgnoreCase("mix"))
                    {
                        float wet = static_cast<float>(value);
                        if (wet > 1.0f) wet /= 100.0f;
                        eff.wetLevel = wet;
                        if (eff.type.containsIgnoreCase("reverb"))
                        {
                            currentState.samplerReverbAmount = wet;
                            audioEngine.setSamplerReverbAmount(wet);
                            reverbSlider.setValue(wet * 100.0, juce::dontSendNotification);
                        }
                        else if (eff.type.containsIgnoreCase("convolution") || eff.type.containsIgnoreCase("ir"))
                        {
                            currentState.irReverbWetLevel = wet;
                            audioEngine.setSamplerIrReverbAmount(wet);
                        }
                        else if (eff.type.containsIgnoreCase("delay") || eff.type.containsIgnoreCase("echo"))
                        {
                            currentState.delayWetLevel = wet;
                            audioEngine.setSamplerDelayWetLevel(wet);
                        }
                        else if (eff.type.containsIgnoreCase("chorus"))
                        {
                            currentState.chorusWetLevel = wet;
                            audioEngine.setSamplerChorusWet(wet);
                        }
                    }
                    else if (b.parameter.containsIgnoreCase("DRY") || b.parameter.containsIgnoreCase("dryLevel"))
                    {
                        float dry = static_cast<float>(value);
                        if (dry > 1.0f) dry /= 100.0f;
                        eff.dryLevel = dry;
                        if (eff.type.containsIgnoreCase("convolution") || eff.type.containsIgnoreCase("ir"))
                        {
                            currentState.irReverbDryLevel = dry;
                            audioEngine.setSamplerIrReverbDryLevel(dry);
                        }
                    }
                    else if (b.parameter.containsIgnoreCase("feedback") || b.parameter.containsIgnoreCase("fb"))
                    {
                        float fb = juce::jlimit(0.0f, 0.95f, static_cast<float>(value));
                        eff.feedback = fb;
                        currentState.delayFeedback = fb;
                        audioEngine.setSamplerDelayFeedback(fb);
                    }
                    else if (b.parameter.containsIgnoreCase("time") || b.parameter.containsIgnoreCase("delayTime"))
                    {
                        float ms = static_cast<float>(value);
                        if (ms <= 10.0f) ms *= 1000.0f;
                        eff.delayTimeMs = ms;
                        currentState.delayTimeMs = ms;
                        audioEngine.setSamplerDelayTimeMs(ms);
                    }
                    else if (b.parameter.containsIgnoreCase("frequency") || b.parameter.containsIgnoreCase("freq") || b.parameter.containsIgnoreCase("cutoff"))
                    {
                        float freq = static_cast<float>(value);
                        if (freq <= 1.0f) freq = 100.0f * std::pow(220.0f, freq);
                        eff.frequency = freq;
                        if (eff.type.containsIgnoreCase("lowpass"))
                        {
                            currentState.masterFilterCutoffHz = freq;
                            audioEngine.setSamplerLowpassCutoff(freq);
                            toneSlider.setValue(freq, juce::dontSendNotification);
                        }
                        else if (eff.type.containsIgnoreCase("highpass"))
                        {
                            currentState.masterHighpassHz = freq;
                            audioEngine.setSamplerHighpassCutoff(freq);
                        }
                    }
                    else if (b.parameter.containsIgnoreCase("rate"))
                    {
                        if (eff.type.containsIgnoreCase("chorus"))
                        {
                            currentState.chorusRateHz = static_cast<float>(value);
                            audioEngine.setSamplerChorusRate(static_cast<float>(value));
                        }
                    }
                    else if (b.parameter.containsIgnoreCase("depth"))
                    {
                        float d = static_cast<float>(value);
                        if (d > 1.0f) d /= 100.0f;
                        if (eff.type.containsIgnoreCase("chorus"))
                        {
                            currentState.chorusDepth = d;
                            audioEngine.setSamplerChorusDepth(d);
                        }
                    }
                }
            }
        }
    }

    if (bp.containsIgnoreCase("attack") || lbl.containsIgnoreCase("attack"))
    {
        float attMs = static_cast<float>(value);
        if (attMs <= 10.0f) attMs *= 1000.0f;
        attackSlider.setValue(attMs, juce::dontSendNotification);
        currentState.globalAttackMs = attMs;
        for (auto& z : currentState.zones) z.attackMs = attMs;
    }
    else if (bp.containsIgnoreCase("decay") || lbl.containsIgnoreCase("decay"))
    {
        float decMs = static_cast<float>(value);
        if (decMs <= 10.0f) decMs *= 1000.0f;
        decaySlider.setValue(decMs, juce::dontSendNotification);
        currentState.globalDecayMs = decMs;
        for (auto& z : currentState.zones) z.decayMs = decMs;
    }
    else if (bp.containsIgnoreCase("sustain") || lbl.containsIgnoreCase("sustain"))
    {
        float sus = static_cast<float>(value);
        if (sus > 1.0f && ctrl.maxValue > 1.0) sus /= static_cast<float>(ctrl.maxValue);
        sustainSlider.setValue(sus, juce::dontSendNotification);
        currentState.globalSustainLevel = sus;
        for (auto& z : currentState.zones) z.sustainLevel = sus;
    }
    else if (bp.containsIgnoreCase("release") || lbl.containsIgnoreCase("release"))
    {
        float relMs = static_cast<float>(value);
        if (relMs <= 10.0f) relMs *= 1000.0f;
        releaseSlider.setValue(relMs, juce::dontSendNotification);
        currentState.globalReleaseMs = relMs;
        for (auto& z : currentState.zones) z.releaseMs = relMs;
    }
    else if (bp.containsIgnoreCase("reverb") || lbl.containsIgnoreCase("reverb") || bp.containsIgnoreCase("space") || lbl.containsIgnoreCase("space"))
    {
        float rev = static_cast<float>(value);
        if (rev > 1.0f) rev /= 100.0f;
        reverbSlider.setValue(rev * 100.0, juce::dontSendNotification);
        audioEngine.setSamplerReverbAmount(rev);
        currentState.samplerReverbAmount = rev;
    }
    else if (bp.containsIgnoreCase("highpass") || lbl.containsIgnoreCase("highpass") || bp.containsIgnoreCase("high_pass") || bp.containsIgnoreCase("FX_HIGHPASS"))
    {
        float hp = static_cast<float>(value);
        if (hp <= 1.0f) hp = 20.0f * std::pow(200.0f, hp);
        currentState.masterHighpassHz = hp;
        audioEngine.setSamplerHighpassCutoff(hp);
    }
    else if (bp.containsIgnoreCase("tone") || lbl.containsIgnoreCase("tone") || bp.containsIgnoreCase("FX_TONE"))
    {
        float t = static_cast<float>(value);
        if (t > 1.0f && ctrl.maxValue > 1.0) t /= static_cast<float>(ctrl.maxValue);
        toneSlider.setValue(t, juce::dontSendNotification);
        currentState.masterTone = t;
        audioEngine.setSamplerTone(t);
    }
    else if (bp.containsIgnoreCase("cutoff") || bp.containsIgnoreCase("lowpass") || bp.containsIgnoreCase("low_pass") ||
             bp.containsIgnoreCase("filter") || lbl.containsIgnoreCase("cutoff") || lbl.containsIgnoreCase("filter") ||
             bp.containsIgnoreCase("frequency") || bp.containsIgnoreCase("FX_FILTER_FREQUENCY"))
    {
        float cutoff = static_cast<float>(value);
        if (cutoff <= 1.0f) cutoff = 100.0f * std::pow(220.0f, cutoff);
        toneSlider.setValue(cutoff, juce::dontSendNotification);
        currentState.masterFilterCutoffHz = cutoff;
        audioEngine.setSamplerLowpassCutoff(cutoff);
    }
    else if (bp.containsIgnoreCase("ir") || lbl.containsIgnoreCase("ir") || bp.containsIgnoreCase("convolution") || lbl.containsIgnoreCase("convolution"))
    {
        float rev = static_cast<float>(value);
        if (rev > 1.0f) rev /= 100.0f;
        audioEngine.setSamplerIrReverbAmount(rev);
        currentState.irReverbWetLevel = rev;
    }
    else if (bp.containsIgnoreCase("delay_time") || lbl.containsIgnoreCase("delay_time") || bp.containsIgnoreCase("delay_ms") || lbl.containsIgnoreCase("delay_ms"))
    {
        float ms = static_cast<float>(value);
        if (ms <= 10.0f) ms *= 1000.0f;
        audioEngine.setSamplerDelayTimeMs(ms);
        currentState.delayTimeMs = ms;
    }
    else if (bp.containsIgnoreCase("delay_feedback") || lbl.containsIgnoreCase("delay_feedback") || bp.containsIgnoreCase("feedback") || lbl.containsIgnoreCase("feedback"))
    {
        float fb = static_cast<float>(value);
        if (fb > 1.0f) fb /= 100.0f;
        audioEngine.setSamplerDelayFeedback(fb);
        currentState.delayFeedback = fb;
    }
    else if (bp.containsIgnoreCase("delay_wet") || lbl.containsIgnoreCase("delay_wet") || bp.containsIgnoreCase("delay") || lbl.containsIgnoreCase("delay") || bp.containsIgnoreCase("echo") || lbl.containsIgnoreCase("echo"))
    {
        float wet = static_cast<float>(value);
        if (wet > 1.0f) wet /= 100.0f;
        audioEngine.setSamplerDelayWetLevel(wet);
        currentState.delayWetLevel = wet;
    }
    else if (bp.containsIgnoreCase("chorus_rate") || lbl.containsIgnoreCase("chorus_rate") || bp.containsIgnoreCase("chorus_speed") || lbl.containsIgnoreCase("chorus_speed"))
    {
        audioEngine.setSamplerChorusRate(static_cast<float>(value));
        currentState.chorusRateHz = static_cast<float>(value);
    }
    else if (bp.containsIgnoreCase("chorus_depth") || lbl.containsIgnoreCase("chorus_depth"))
    {
        float d = static_cast<float>(value);
        if (d > 1.0f) d /= 100.0f;
        audioEngine.setSamplerChorusDepth(d);
        currentState.chorusDepth = d;
    }
    else if (bp.containsIgnoreCase("chorus_wet") || lbl.containsIgnoreCase("chorus_wet") || bp.containsIgnoreCase("chorus") || lbl.containsIgnoreCase("chorus"))
    {
        float wet = static_cast<float>(value);
        if (wet > 1.0f) wet /= 100.0f;
        audioEngine.setSamplerChorusWet(wet);
        currentState.chorusWetLevel = wet;
    }
    else if (bp.containsIgnoreCase("lfo_depth") || bp.containsIgnoreCase("lfo_amount") || lbl.containsIgnoreCase("lfo_depth") || lbl.containsIgnoreCase("lfo_amount") || bp.containsIgnoreCase("vibrato") || lbl.containsIgnoreCase("vibrato") || bp.containsIgnoreCase("tremolo") || lbl.containsIgnoreCase("tremolo"))
    {
        float amt = static_cast<float>(value);
        if (amt > 1.0f) amt /= 100.0f;
        audioEngine.setLfoAmount(amt);
    }
    else if (bp.containsIgnoreCase("lfo_rate") || bp.containsIgnoreCase("lfo_speed") || bp.containsIgnoreCase("lfo_freq") || lbl.containsIgnoreCase("lfo_rate") || lbl.containsIgnoreCase("lfo_speed"))
    {
        audioEngine.setLfoFrequency(static_cast<float>(value));
    }
    else if (bp.containsIgnoreCase("volume") || lbl.containsIgnoreCase("volume") || bp.containsIgnoreCase("gain") || lbl.containsIgnoreCase("gain") || bp.containsIgnoreCase("AMP_VOLUME"))
    {
        float vol = static_cast<float>(value);
        if (vol <= 0.0001f)
        {
            audioEngine.setGain(0.0f);
            volumeSlider.setValue(-96.0, juce::dontSendNotification);
            currentState.masterGainDb = -96.0f;
        }
        else if (vol <= 1.0f && vol >= 0.0f)
        {
            audioEngine.setGain(vol);
            float db = 20.0f * std::log10(vol);
            volumeSlider.setValue(db, juce::dontSendNotification);
            currentState.masterGainDb = db;
        }
        else
        {
            audioEngine.setGain(std::pow(10.0f, vol / 20.0f));
            volumeSlider.setValue(vol, juce::dontSendNotification);
            currentState.masterGainDb = vol;
        }
    }
    else if (bp.containsIgnoreCase("tune") || bp.containsIgnoreCase("pitch") || lbl.containsIgnoreCase("tune") || lbl.containsIgnoreCase("pitch"))
    {
        float cents = static_cast<float>(value);
        tuneSlider.setValue(cents, juce::dontSendNotification);
        currentState.masterFineTuneCents = cents;
    }
}

void PlayComponent::sliderValueChanged(juce::Slider* slider)
{
    if (slider == &attackSlider)
    {
        currentState.globalAttackMs = static_cast<float>(attackSlider.getValue());
        for (auto& z : currentState.zones)
            z.attackMs = currentState.globalAttackMs;
    }
    else if (slider == &decaySlider)
    {
        currentState.globalDecayMs = static_cast<float>(decaySlider.getValue());
        for (auto& z : currentState.zones)
            z.decayMs = currentState.globalDecayMs;
    }
    else if (slider == &sustainSlider)
    {
        currentState.globalSustainLevel = static_cast<float>(sustainSlider.getValue());
        for (auto& z : currentState.zones)
            z.sustainLevel = currentState.globalSustainLevel;
    }
    else if (slider == &releaseSlider)
    {
        currentState.globalReleaseMs = static_cast<float>(releaseSlider.getValue());
        for (auto& z : currentState.zones)
            z.releaseMs = currentState.globalReleaseMs;
    }
    else if (slider == &reverbSlider)
    {
        float revAmount = static_cast<float>(reverbSlider.getValue()) / 100.0f;
        audioEngine.setSamplerReverbAmount(revAmount);
        currentState.samplerReverbAmount = revAmount;
    }
    else if (slider == &volumeSlider)
    {
        currentState.masterGainDb = static_cast<float>(volumeSlider.getValue());
    }
    else if (slider == &tuneSlider)
    {
        currentState.masterFineTuneCents = static_cast<float>(tuneSlider.getValue());
    }
    else if (slider == &toneSlider)
    {
        float val = static_cast<float>(toneSlider.getValue());
        currentState.masterFilterCutoffHz = val;
        if (val <= 1.0f)
            audioEngine.setSamplerTone(val);
        else
            audioEngine.setSamplerLowpassCutoff(val);
    }
    else
    {
        // Dynamic custom sliders
        for (size_t i = 0; i < customSliders.size(); ++i)
        {
            if (customSliders[i].slider.get() == slider)
            {
                double newVal = slider->getValue();
                customSliders[i].model.currentValue = newVal;
                if (i < currentState.uiControls.size())
                    currentState.uiControls[i].currentValue = newVal;

                applyCustomControlBinding(customSliders[i].model, newVal);
                break;
            }
        }
    }

    repaint();
    if (onStateChanged)
        onStateChanged(currentState);
}

void PlayComponent::buttonClicked(juce::Button* button)
{
    if (button == &viewModeToggleButton)
    {
        showCustomUiCanvas = !showCustomUiCanvas;
        updateViewModeVisibility();
    }
    else if (button == &loadPresetButton)
    {
        if (onLoadPresetRequested)
            onLoadPresetRequested();
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
    else if (button == &addCustomControlButton)
    {
        showAddCustomControlDialog();
    }
}

void PlayComponent::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        // Check if right-clicked on any custom slider or label
        for (size_t i = 0; i < customSliders.size(); ++i)
        {
            auto* s = customSliders[i].slider.get();
            auto* l = customSliders[i].label.get();
            auto* dc = customSliders[i].decentControl.get();
            if ((s != nullptr && s->getBounds().contains(e.getPosition())) ||
                (l != nullptr && l->getBounds().contains(e.getPosition())) ||
                (dc != nullptr && dc->getBounds().contains(e.getPosition())))
            {
                juce::PopupMenu menu;
                menu.addItem(1, "Edit Control Properties...");
                menu.addItem(2, "Reset to Default Value (" + juce::String(customSliders[i].model.defaultValue) + ")");
                menu.addSeparator();
                menu.addItem(3, "Delete Control");

                int idx = static_cast<int>(i);
                menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMousePosition(),
                    [this, idx](int result) {
                        if (result == 1)
                        {
                            showEditCustomControlDialog(idx);
                        }
                        else if (result == 2)
                        {
                            if (idx >= 0 && idx < static_cast<int>(customSliders.size()))
                            {
                                double defVal = customSliders[idx].model.defaultValue;
                                if (customSliders[idx].slider)
                                    customSliders[idx].slider->setValue(defVal, juce::sendNotification);
                                if (customSliders[idx].decentControl)
                                    customSliders[idx].decentControl->setValue(defVal, true);
                            }
                        }
                        else if (result == 3)
                        {
                            if (idx >= 0 && idx < static_cast<int>(currentState.uiControls.size()))
                            {
                                currentState.uiControls.erase(currentState.uiControls.begin() + idx);
                                rebuildCustomSliders();
                                if (onStateChanged)
                                    onStateChanged(currentState);
                            }
                        }
                    });
                return;
            }
        }
    }
}

void PlayComponent::showEditCustomControlDialog(int index)
{
    if (index < 0 || index >= static_cast<int>(currentState.uiControls.size()))
        return;

    auto& ctrl = currentState.uiControls[index];
    auto* aw = new juce::AlertWindow("Edit Preset Control", "Modify properties for \"" + ctrl.label + "\":", juce::AlertWindow::NoIcon);
    aw->addTextEditor("label", ctrl.label, "Label Name:");
    aw->addTextEditor("min", juce::String(ctrl.minValue), "Min Value:");
    aw->addTextEditor("max", juce::String(ctrl.maxValue), "Max Value:");
    aw->addTextEditor("units", ctrl.units, "Units (e.g. ms, dB, %, Hz):");

    juce::StringArray bindings = { "Custom Macro", "Amplitude Attack", "Amplitude Decay", "Amplitude Sustain", "Amplitude Release", "Master Volume", "Reverb Wet", "Filter Cutoff", "Fine Tune" };
    aw->addComboBox("binding", bindings, "Target Binding:");

    int comboIdx = 1;
    if (ctrl.bindingParam.containsIgnoreCase("attack") || ctrl.label.containsIgnoreCase("attack")) comboIdx = 2;
    else if (ctrl.bindingParam.containsIgnoreCase("decay") || ctrl.label.containsIgnoreCase("decay")) comboIdx = 3;
    else if (ctrl.bindingParam.containsIgnoreCase("sustain") || ctrl.label.containsIgnoreCase("sustain")) comboIdx = 4;
    else if (ctrl.bindingParam.containsIgnoreCase("release") || ctrl.label.containsIgnoreCase("release")) comboIdx = 5;
    else if (ctrl.bindingParam.containsIgnoreCase("volume") || ctrl.label.containsIgnoreCase("volume")) comboIdx = 6;
    else if (ctrl.bindingParam.containsIgnoreCase("reverb") || ctrl.label.containsIgnoreCase("reverb")) comboIdx = 7;
    else if (ctrl.bindingParam.containsIgnoreCase("cutoff") || ctrl.label.containsIgnoreCase("tone") || ctrl.label.containsIgnoreCase("filter")) comboIdx = 8;
    else if (ctrl.bindingParam.containsIgnoreCase("tune") || ctrl.label.containsIgnoreCase("tune")) comboIdx = 9;
    aw->getComboBoxComponent("binding")->setSelectedId(comboIdx);

    aw->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    aw->enterModalState(true, juce::ModalCallbackFunction::create([this, aw, index](int result) {
        if (result == 1 && index >= 0 && index < static_cast<int>(currentState.uiControls.size()))
        {
            auto& c = currentState.uiControls[index];
            juce::String newLabel = aw->getTextEditorContents("label").trim();
            if (newLabel.isNotEmpty()) c.label = newLabel;
            c.minValue = aw->getTextEditorContents("min").getDoubleValue();
            c.maxValue = aw->getTextEditorContents("max").getDoubleValue();
            if (c.maxValue <= c.minValue) c.maxValue = c.minValue + 1.0;
            c.units = aw->getTextEditorContents("units").trim();

            int selectedBinding = aw->getComboBoxComponent("binding")->getSelectedId();
            switch (selectedBinding)
            {
                case 2: c.bindingParam = "ENV_ATTACK"; break;
                case 3: c.bindingParam = "ENV_DECAY"; break;
                case 4: c.bindingParam = "ENV_SUSTAIN"; break;
                case 5: c.bindingParam = "ENV_RELEASE"; break;
                case 6: c.bindingParam = "AMP_VOLUME"; break;
                case 7: c.bindingParam = "FX_REVERB_WET_LEVEL"; break;
                case 8: c.bindingParam = "FX_FILTER_FREQUENCY"; break;
                case 9: c.bindingParam = "PITCH"; break;
                default: c.bindingParam = ""; break;
            }

            rebuildCustomSliders();
            if (onStateChanged)
                onStateChanged(currentState);
        }
        delete aw;
    }));
}

void PlayComponent::showAddCustomControlDialog()
{
    auto* aw = new juce::AlertWindow("Add Preset Control", "Create a new macro control slider:", juce::AlertWindow::NoIcon);
    aw->addTextEditor("label", "Macro " + juce::String(currentState.uiControls.size() + 1), "Label Name:");
    aw->addTextEditor("min", "0.0", "Min Value:");
    aw->addTextEditor("max", "100.0", "Max Value:");
    aw->addTextEditor("units", "%", "Units (e.g. ms, dB, %, Hz):");

    juce::StringArray bindings = { "Custom Macro", "Amplitude Attack", "Amplitude Decay", "Amplitude Sustain", "Amplitude Release", "Master Volume", "Reverb Wet", "Filter Cutoff", "Fine Tune" };
    aw->addComboBox("binding", bindings, "Target Binding:");
    aw->getComboBoxComponent("binding")->setSelectedId(1);

    aw->addButton("Add", 1, juce::KeyPress(juce::KeyPress::returnKey));
    aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    aw->enterModalState(true, juce::ModalCallbackFunction::create([this, aw](int result) {
        if (result == 1)
        {
            DecentSamplerUiControl ctrl;
            ctrl.label = aw->getTextEditorContents("label").trim();
            if (ctrl.label.isEmpty()) ctrl.label = "Macro";
            ctrl.id = ctrl.label.toLowerCase().replace(" ", "_");
            ctrl.parameterName = ctrl.id;
            ctrl.minValue = aw->getTextEditorContents("min").getDoubleValue();
            ctrl.maxValue = aw->getTextEditorContents("max").getDoubleValue();
            if (ctrl.maxValue <= ctrl.minValue) ctrl.maxValue = ctrl.minValue + 1.0;
            ctrl.defaultValue = ctrl.minValue;
            ctrl.currentValue = ctrl.defaultValue;
            ctrl.units = aw->getTextEditorContents("units").trim();

            int selectedBinding = aw->getComboBoxComponent("binding")->getSelectedId();
            switch (selectedBinding)
            {
                case 2: ctrl.bindingParam = "ENV_ATTACK"; break;
                case 3: ctrl.bindingParam = "ENV_DECAY"; break;
                case 4: ctrl.bindingParam = "ENV_SUSTAIN"; break;
                case 5: ctrl.bindingParam = "ENV_RELEASE"; break;
                case 6: ctrl.bindingParam = "AMP_VOLUME"; break;
                case 7: ctrl.bindingParam = "FX_REVERB_WET_LEVEL"; break;
                case 8: ctrl.bindingParam = "FX_FILTER_FREQUENCY"; break;
                case 9: ctrl.bindingParam = "PITCH"; break;
                default: ctrl.bindingParam = ""; break;
            }

            currentState.uiControls.push_back(ctrl);
            rebuildCustomSliders();
            if (onStateChanged)
                onStateChanged(currentState);
        }
        delete aw;
    }));
}

void PlayComponent::rebuildCustomSliders()
{
    customSliders.clear();

    for (size_t i = 0; i < currentState.uiControls.size(); ++i)
    {
        const auto& ctrl = currentState.uiControls[i];
        CustomSliderControl cs;
        cs.model = ctrl;
        cs.controlIndex = static_cast<int>(i);

        // Always create 1:1 DecentSamplerControlComponent so filmstrips and custom dials render
        cs.decentControl = std::make_unique<DecentSamplerControlComponent>(ctrl);
        cs.decentControl->onValueChanged = [this, i, ctrl](double val) {
            if (i < currentState.uiControls.size())
                currentState.uiControls[i].currentValue = val;
            applyCustomControlBinding(ctrl, val);
            if (onStateChanged) onStateChanged(currentState);
        };
        addChildComponent(*cs.decentControl);

        cs.slider = std::make_unique<juce::Slider>();
        cs.slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        cs.slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 65, 18);
        cs.slider->setRange(ctrl.minValue, ctrl.maxValue, (ctrl.maxValue - ctrl.minValue) / 100.0);
        cs.slider->setValue(ctrl.currentValue, juce::dontSendNotification);
        if (ctrl.units.isNotEmpty())
            cs.slider->setTextValueSuffix(" " + ctrl.units);
        cs.slider->addListener(this);
        cs.slider->addMouseListener(this, false);
        addChildComponent(*cs.slider);

        cs.label = std::make_unique<juce::Label>();
        cs.label->setFont(juce::Font(12.0f).boldened());
        cs.label->setJustificationType(juce::Justification::centred);
        cs.label->setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
        cs.label->setText(ctrl.label, juce::dontSendNotification);
        cs.label->addMouseListener(this, false);
        addChildComponent(*cs.label);

        customSliders.push_back(std::move(cs));
    }

    updateViewModeVisibility();
}

void PlayComponent::syncUiFromState()
{
    attackSlider.setValue(currentState.globalAttackMs, juce::dontSendNotification);
    decaySlider.setValue(currentState.globalDecayMs, juce::dontSendNotification);
    sustainSlider.setValue(currentState.globalSustainLevel, juce::dontSendNotification);
    releaseSlider.setValue(currentState.globalReleaseMs, juce::dontSendNotification);

    reverbSlider.setValue(currentState.samplerReverbAmount * 100.0, juce::dontSendNotification);
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

    volumeSlider.setValue(currentState.masterGainDb, juce::dontSendNotification);
    toneSlider.setValue(currentState.masterFilterCutoffHz, juce::dontSendNotification);
    audioEngine.setSamplerLowpassCutoff(currentState.masterFilterCutoffHz);
    audioEngine.setSamplerHighpassCutoff(currentState.masterHighpassHz);
    tuneSlider.setValue(currentState.masterFineTuneCents, juce::dontSendNotification);

    pitchTrackButton.setToggleState(currentState.pitchTrackingEnabled, juce::dontSendNotification);
    pitchTrackButton.setButtonText(currentState.pitchTrackingEnabled ? "Pitch Track: ON" : "Pitch Track: OFF");
    audioEngine.setPitchTrackingEnabled(currentState.pitchTrackingEnabled);

    roundRobinButton.setButtonText(currentState.roundRobinMode == 0 ? "RR: Cycle" : (currentState.roundRobinMode == 1 ? "RR: Random" : "RR: OFF"));

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

    rebuildCustomSliders();
}

void PlayComponent::setState(const SampleMapState& state)
{
    currentState = state;
    if (currentState.customUi.hasCustomUi())
    {
        showCustomUiCanvas = true;
    }
    customCanvas.setInstrumentState(currentState);
    syncUiFromState();
    updateViewModeVisibility();
}

SampleMapState PlayComponent::getState() const
{
    return currentState;
}

} // namespace openwav
