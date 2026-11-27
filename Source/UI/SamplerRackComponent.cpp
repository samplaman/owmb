#include "SamplerRackComponent.h"
#include "SampleMapComponent.h"
#include <cmath>
#include <algorithm>

namespace openwav
{

namespace
{
static const char* kSlotAccentPalette[8] = {
    "FF00D4FF", "FF10B981", "FFF59E0B", "FFA855F7",
    "FFEC4899", "FF3B82F6", "FF14B8A6", "FFF97316"
};

juce::String makeUniqueSlotId()
{
    return "slot_" + juce::Uuid().toString().substring(0, 8);
}

juce::String inferMapDisplayName(const SampleMapState& map, const juce::String& fallback)
{
    if (map.instrument.instrumentName.isNotEmpty()
        && map.instrument.instrumentName != "Custom Instrument")
    {
        return map.instrument.instrumentName;
    }
    if (!map.zones.empty())
    {
        const auto& z0 = map.zones.front();
        juce::String base = z0.sampleName.isNotEmpty()
            ? juce::File(z0.sampleName).getFileNameWithoutExtension()
            : juce::File(z0.filePath).getFileNameWithoutExtension();
        if (base.isNotEmpty())
            return (map.zones.size() > 1) ? (base + " (" + juce::String(map.zones.size()) + "z)") : base;
    }
    return fallback;
}
} // namespace

// ─────────────────────────────────────────────────────────────────────────────
//  SamplerRackUnitComponent
// ─────────────────────────────────────────────────────────────────────────────
SamplerRackUnitComponent::SamplerRackUnitComponent(SamplerRackComponent& ownerRack,
                                                   OpenWavAudioProcessor& processor,
                                                   AudioEngine& engine,
                                                   int unitIndex,
                                                   int totalUnits,
                                                   bool isEditingActiveSlot)
    : owner(ownerRack),
      audioProcessor(processor),
      audioEngine(engine),
      slotIdx(unitIndex),
      totalCount(totalUnits),
      activeInEditor(isEditingActiveSlot)
{
    foldBtn.setComponentID("iconOnlyBtn");
    foldBtn.setTooltip("Fold / Expand this SampleMap instrument in the Sampler Rack");
    foldBtn.onClick = [this] {
        if (isSyncing) return;
        currentSlot.collapsed = !currentSlot.collapsed;
        foldBtn.setButtonText(currentSlot.collapsed ? "Expand Unit" : "Collapse Unit");
        owner.updateSlotState(slotIdx, currentSlot, true);
    };
    addAndMakeVisible(foldBtn);

    slotIndexLabel.setJustificationType(juce::Justification::centred);
    slotIndexLabel.setFont(OpenWavLookAndFeel::getMonoFont(11.0f));
    slotIndexLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    addAndMakeVisible(slotIndexLabel);

    nameEditor.setEditable(false, true, false);
    nameEditor.setJustificationType(juce::Justification::centredLeft);
    nameEditor.setFont(OpenWavLookAndFeel::getBodyFont(12.5f, true));
    nameEditor.setColour(juce::Label::textColourId, juce::Colours::white);
    nameEditor.setColour(juce::Label::backgroundColourId, OpenWavLookAndFeel::bgDark.withAlpha(0.55f));
    nameEditor.setTooltip("Double-click to rename this Sampler Rack instrument");
    nameEditor.onTextChange = [this] {
        if (isSyncing) return;
        juce::String txt = nameEditor.getText().trim();
        if (txt.isEmpty())
            txt = "Sampler " + juce::String(slotIdx + 1);
        currentSlot.name = txt;
        nameEditor.setText(txt, juce::dontSendNotification);
        owner.updateSlotState(slotIdx, currentSlot, false);
    };
    addAndMakeVisible(nameEditor);

    // MIDI Input Channel Selector (Omni or Ch 1..16)
    midiChannelCombo.addItem("MIDI: Omni", 1);
    for (int ch = 1; ch <= 16; ++ch)
        midiChannelCombo.addItem("MIDI: Ch " + juce::String(ch), ch + 1);
    midiChannelCombo.setTooltip("Select which incoming MIDI Channel triggers this instrument");
    midiChannelCombo.onChange = [this] {
        if (isSyncing) return;
        const int id = midiChannelCombo.getSelectedId();
        if (id >= 1 && id <= 17)
        {
            currentSlot.midiChannel = id - 1;
            currentSlot.sampleMap.midiChannel = currentSlot.midiChannel;
            owner.updateSlotState(slotIdx, currentSlot, false);
        }
    };
    addAndMakeVisible(midiChannelCombo);

    // Per-Instrument DAW Stereo Output Track Selector (Out 1-2 .. Out 15-16)
    outputBusCombo.setTooltip("Route this entire instrument to a stereo output track (Out 1-2 through Out 15-16)");
    outputBusCombo.onChange = [this] {
        if (isSyncing) return;
        const int id = outputBusCombo.getSelectedId();
        if (id >= 1 && id <= AudioEngine::MaxOutputBuses)
        {
            currentSlot.outputBus = id - 1;
            currentSlot.sampleMap.outputBus = currentSlot.outputBus;
            owner.updateSlotState(slotIdx, currentSlot, false);
        }
    };
    addAndMakeVisible(outputBusCombo);

    muteBtn.setClickingTogglesState(true);
    muteBtn.setTooltip("Mute this instrument");
    muteBtn.setColour(juce::TextButton::buttonOnColourId, OpenWavLookAndFeel::favoriteRed.withAlpha(0.85f));
    muteBtn.onClick = [this] {
        if (isSyncing) return;
        currentSlot.muted = muteBtn.getToggleState();
        owner.updateSlotState(slotIdx, currentSlot, false);
        repaint();
    };
    addAndMakeVisible(muteBtn);

    soloBtn.setClickingTogglesState(true);
    soloBtn.setTooltip("Solo this instrument");
    soloBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFFD97706));
    soloBtn.onClick = [this] {
        if (isSyncing) return;
        currentSlot.soloed = soloBtn.getToggleState();
        owner.updateSlotState(slotIdx, currentSlot, false);
        repaint();
    };
    addAndMakeVisible(soloBtn);

    loadMapBtn.setTooltip("Load a .owmap, .samplemap, .xml, .sfz, or .zip instrument into this slot");
    loadMapBtn.onClick = [this] { owner.loadMapFileIntoSlot(slotIdx); };
    addAndMakeVisible(loadMapBtn);

    editMapBtn.setTooltip("Unfold into the Sample Map Editor to edit keyzones, velocity layers, and round-robins");
    editMapBtn.onClick = [this] { owner.openSlotInSampleMapEditor(slotIdx); };
    addAndMakeVisible(editMapBtn);

    moveUpBtn.setComponentID("iconOnlyBtn");
    moveUpBtn.setTooltip("Move Instrument Up");
    moveUpBtn.onClick = [this] { owner.moveSlot(slotIdx, slotIdx - 1); };
    addAndMakeVisible(moveUpBtn);

    moveDownBtn.setComponentID("iconOnlyBtn");
    moveDownBtn.setTooltip("Move Instrument Down");
    moveDownBtn.onClick = [this] { owner.moveSlot(slotIdx, slotIdx + 1); };
    addAndMakeVisible(moveDownBtn);

    deleteBtn.setComponentID("iconOnlyBtn");
    deleteBtn.setTooltip("Remove Instrument from Rack");
    deleteBtn.onClick = [this] { owner.removeSlot(slotIdx); };
    addAndMakeVisible(deleteBtn);

    OpenWavLookAndFeel::groupButtons({ &moveUpBtn, &moveDownBtn, &deleteBtn });

    // Setup 5 Studio Rotary Knobs (matching PerformanceComponent Effects Rack)
    auto initRotary = [this](int idx, const juce::String& name, double minV, double maxV, double stepV, double defV) {
        auto& k = paramKnobs[static_cast<size_t>(idx)];
        k.title.setText(name, juce::dontSendNotification);
        k.title.setJustificationType(juce::Justification::centred);
        k.title.setFont(OpenWavLookAndFeel::getBodyFont(9.5f, true));
        k.title.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
        addAndMakeVisible(k.title);

        k.knob.setLookAndFeel(&unitKnobLnf);
        k.knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        k.knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        k.knob.setRange(minV, maxV, stepV);
        k.knob.setDoubleClickReturnValue(true, defV);
        addAndMakeVisible(k.knob);

        k.readout.setJustificationType(juce::Justification::centred);
        k.readout.setFont(OpenWavLookAndFeel::getMonoFont(10.0f));
        k.readout.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textPrimary);
        addAndMakeVisible(k.readout);
    };

    initRotary(0, "GAIN", -36.0, 12.0, 0.5, 0.0);
    paramKnobs[0].knob.onValueChange = [this] {
        if (isSyncing) return;
        const float v = static_cast<float>(paramKnobs[0].knob.getValue());
        currentSlot.gainDb = v;
        paramKnobs[0].readout.setText((v >= 0.05f ? "+" : "") + juce::String(v, 1) + " dB", juce::dontSendNotification);
        owner.updateSlotState(slotIdx, currentSlot, false);
    };

    initRotary(1, "PAN", -100.0, 100.0, 1.0, 0.0);
    paramKnobs[1].knob.onValueChange = [this] {
        if (isSyncing) return;
        const int pct = juce::roundToInt(paramKnobs[1].knob.getValue());
        currentSlot.pan = juce::jlimit(-1.0f, 1.0f, static_cast<float>(pct) / 100.0f);
        paramKnobs[1].readout.setText(pct == 0 ? "C" : (pct < 0 ? "L" + juce::String(-pct) : "R" + juce::String(pct)),
                                      juce::dontSendNotification);
        owner.updateSlotState(slotIdx, currentSlot, false);
    };

    initRotary(2, "TRANSPOSE", -24.0, 24.0, 1.0, 0.0);
    paramKnobs[2].knob.onValueChange = [this] {
        if (isSyncing) return;
        const int st = juce::roundToInt(paramKnobs[2].knob.getValue());
        currentSlot.transposeSemitones = st;
        paramKnobs[2].readout.setText((st > 0 ? "+" : "") + juce::String(st) + " st", juce::dontSendNotification);
        owner.updateSlotState(slotIdx, currentSlot, false);
    };

    initRotary(3, "ATTACK", 0.0, 2000.0, 1.0, 5.0);
    paramKnobs[3].knob.setSkewFactorFromMidPoint(120.0);
    paramKnobs[3].knob.onValueChange = [this] {
        if (isSyncing) return;
        const float atk = static_cast<float>(paramKnobs[3].knob.getValue());
        currentSlot.sampleMap.globalAttackMs = atk;
        for (auto& z : currentSlot.sampleMap.zones)
            if (!z.isReleaseTrigger) z.attackMs = atk;
        paramKnobs[3].readout.setText(juce::String(juce::roundToInt(atk)) + " ms", juce::dontSendNotification);
        owner.updateSlotState(slotIdx, currentSlot, false);
    };

    initRotary(4, "RELEASE", 5.0, 5000.0, 5.0, 200.0);
    paramKnobs[4].knob.setSkewFactorFromMidPoint(400.0);
    paramKnobs[4].knob.onValueChange = [this] {
        if (isSyncing) return;
        const float rel = static_cast<float>(paramKnobs[4].knob.getValue());
        currentSlot.sampleMap.globalReleaseMs = rel;
        for (auto& z : currentSlot.sampleMap.zones)
            if (!z.isReleaseTrigger) z.releaseMs = rel;
        paramKnobs[4].readout.setText(juce::String(juce::roundToInt(rel)) + " ms", juce::dontSendNotification);
        owner.updateSlotState(slotIdx, currentSlot, false);
    };
}

SamplerRackUnitComponent::~SamplerRackUnitComponent()
{
    for (auto& k : paramKnobs)
        k.knob.setLookAndFeel(nullptr);
}

juce::Colour SamplerRackUnitComponent::getSlotAccent() const
{
    if (currentSlot.accentHex.isNotEmpty())
    {
        auto c = juce::Colour::fromString(currentSlot.accentHex);
        if (c.getAlpha() > 0)
            return c;
    }
    return OpenWavLookAndFeel::accentCyan;
}

void SamplerRackUnitComponent::syncFromSlot(const SamplerRackSlotState& slotState,
                                            int unitIndex,
                                            int totalUnits,
                                            bool isEditingActiveSlot)
{
    isSyncing = true;
    currentSlot = slotState;
    currentSlot.outputBus = juce::jlimit(0, AudioEngine::MaxOutputBuses - 1, currentSlot.outputBus);
    slotIdx = unitIndex;
    totalCount = totalUnits;
    activeInEditor = isEditingActiveSlot;

    unitKnobLnf.setAccentColour(getSlotAccent());

    foldBtn.setButtonText(currentSlot.collapsed ? "Expand Unit" : "Collapse Unit");
    slotIndexLabel.setText(juce::String::formatted("#%02d", slotIdx + 1), juce::dontSendNotification);
    nameEditor.setText(currentSlot.name, juce::dontSendNotification);

    midiChannelCombo.setSelectedId(juce::jlimit(0, 16, currentSlot.midiChannel) + 1, juce::dontSendNotification);

    outputBusCombo.clear(juce::dontSendNotification);
    for (int b = 0; b < AudioEngine::MaxOutputBuses; ++b)
        outputBusCombo.addItem(owner.getOutputBusLabel(b), b + 1);
    outputBusCombo.setSelectedId(currentSlot.outputBus + 1, juce::dontSendNotification);

    muteBtn.setToggleState(currentSlot.muted, juce::dontSendNotification);
    soloBtn.setToggleState(currentSlot.soloed, juce::dontSendNotification);
    editMapBtn.setToggleState(activeInEditor, juce::dontSendNotification);

    moveUpBtn.setEnabled(slotIdx > 0);
    moveDownBtn.setEnabled(slotIdx < totalCount - 1);

    paramKnobs[0].knob.setValue(currentSlot.gainDb, juce::dontSendNotification);
    paramKnobs[0].readout.setText((currentSlot.gainDb >= 0.05f ? "+" : "") + juce::String(currentSlot.gainDb, 1) + " dB", juce::dontSendNotification);

    const int panPct = juce::roundToInt(currentSlot.pan * 100.0f);
    paramKnobs[1].knob.setValue(panPct, juce::dontSendNotification);
    paramKnobs[1].readout.setText(panPct == 0 ? "C" : (panPct < 0 ? "L" + juce::String(-panPct) : "R" + juce::String(panPct)), juce::dontSendNotification);

    paramKnobs[2].knob.setValue(currentSlot.transposeSemitones, juce::dontSendNotification);
    paramKnobs[2].readout.setText((currentSlot.transposeSemitones > 0 ? "+" : "") + juce::String(currentSlot.transposeSemitones) + " st", juce::dontSendNotification);

    paramKnobs[3].knob.setValue(currentSlot.sampleMap.globalAttackMs, juce::dontSendNotification);
    paramKnobs[3].readout.setText(juce::String(juce::roundToInt(currentSlot.sampleMap.globalAttackMs)) + " ms", juce::dontSendNotification);

    paramKnobs[4].knob.setValue(currentSlot.sampleMap.globalReleaseMs, juce::dontSendNotification);
    paramKnobs[4].readout.setText(juce::String(juce::roundToInt(currentSlot.sampleMap.globalReleaseMs)) + " ms", juce::dontSendNotification);

    const bool showBody = !currentSlot.collapsed;
    for (auto& k : paramKnobs)
    {
        k.title.setVisible(showBody);
        k.knob.setVisible(showBody);
        k.readout.setVisible(showBody);
    }

    isSyncing = false;
    resized();
    repaint();
}

void SamplerRackUnitComponent::updatePeakMeter(float newPeak)
{
    const float next = std::max(newPeak, activityLevel * 0.80f);
    const float clamped = (next < 0.002f) ? 0.0f : juce::jlimit(0.0f, 1.2f, next);
    if (std::abs(clamped - activityLevel) > 0.004f)
    {
        activityLevel = clamped;
        repaint();
    }
}

int SamplerRackUnitComponent::noteAtMiniMapX(float x) const
{
    if (miniMapBounds.isEmpty())
        return 60;
    const float norm = juce::jlimit(0.0f, 1.0f, (x - miniMapBounds.getX()) / std::max(1.0f, miniMapBounds.getWidth()));
    return juce::jlimit(24, 96, 24 + juce::roundToInt(norm * 72.0f));
}

void SamplerRackUnitComponent::mouseDown(const juce::MouseEvent& e)
{
    if (!currentSlot.collapsed && miniMapBounds.contains(e.position))
    {
        if (currentSlot.sampleMap.zones.empty())
        {
            owner.loadMapFileIntoSlot(slotIdx);
            return;
        }
        previewNoteHeld = noteAtMiniMapX(e.position.x);
        audioProcessor.triggerRackSlotNoteOn(slotIdx, previewNoteHeld, 0.85f);
        repaint();
    }
}

void SamplerRackUnitComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (previewNoteHeld >= 0 && !currentSlot.collapsed && miniMapBounds.contains(e.position))
    {
        const int newNote = noteAtMiniMapX(e.position.x);
        if (newNote != previewNoteHeld)
        {
            audioProcessor.triggerRackSlotNoteOff(slotIdx, previewNoteHeld);
            previewNoteHeld = newNote;
            audioProcessor.triggerRackSlotNoteOn(slotIdx, previewNoteHeld, 0.85f);
            repaint();
        }
    }
}

void SamplerRackUnitComponent::mouseUp(const juce::MouseEvent&)
{
    if (previewNoteHeld >= 0)
    {
        audioProcessor.triggerRackSlotNoteOff(slotIdx, previewNoteHeld);
        previewNoteHeld = -1;
        repaint();
    }
}

void SamplerRackUnitComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const auto accent = getSlotAccent();
    const bool isMuted = currentSlot.muted || !currentSlot.enabled;

    // 1. 19" Hardware Chassis Metallic Gradient
    juce::ColourGradient chassisGrad(juce::Colour(34, 38, 44), 0.0f, bounds.getY(),
                                     juce::Colour(22, 24, 28), 0.0f, bounds.getBottom(), false);
    g.setGradientFill(chassisGrad);
    g.fillRoundedRectangle(bounds, 5.0f);

    g.setColour(juce::Colours::white.withAlpha(0.05f));
    g.drawRoundedRectangle(bounds.reduced(1.0f), 4.5f, 1.0f);

    // 2. 19" Rack Mounting Ears (Left & Right 38px)
    constexpr float earWidth = 38.0f;
    juce::Rectangle<float> leftEar(bounds.getX(), bounds.getY(), earWidth, bounds.getHeight());
    juce::Rectangle<float> rightEar(bounds.getRight() - earWidth, bounds.getY(), earWidth, bounds.getHeight());

    juce::ColourGradient earGrad(juce::Colour(22, 24, 28), 0.0f, bounds.getY(),
                                 juce::Colour(16, 18, 20), 0.0f, bounds.getBottom(), false);
    g.setGradientFill(earGrad);
    g.fillRect(leftEar);
    g.fillRect(rightEar);

    g.setColour(juce::Colour(45, 50, 58));
    g.drawVerticalLine(static_cast<int>(leftEar.getRight()), bounds.getY(), bounds.getBottom());
    g.drawVerticalLine(static_cast<int>(rightEar.getX()), bounds.getY(), bounds.getBottom());

    auto drawScrew = [&g](float cx, float cy) {
        const float r = 5.0f;
        g.setColour(juce::Colour(12, 14, 16));
        g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);
        g.setColour(juce::Colour(75, 82, 92));
        g.drawEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f, 1.0f);
        g.setColour(juce::Colour(32, 36, 42));
        g.drawLine(cx - 2.6f, cy - 2.6f, cx + 2.6f, cy + 2.6f, 1.1f);
        g.drawLine(cx - 2.6f, cy + 2.6f, cx + 2.6f, cy - 2.6f, 1.1f);
    };

    if (currentSlot.collapsed)
    {
        drawScrew(leftEar.getCentreX(), bounds.getCentreY());
        drawScrew(rightEar.getCentreX(), bounds.getCentreY());
    }
    else
    {
        drawScrew(leftEar.getCentreX(), bounds.getY() + 18.0f);
        drawScrew(leftEar.getCentreX(), bounds.getBottom() - 18.0f);
        drawScrew(rightEar.getCentreX(), bounds.getY() + 18.0f);
        drawScrew(rightEar.getCentreX(), bounds.getBottom() - 18.0f);
    }

    // 3. Top Accent Strip
    g.setColour(isMuted ? accent.withAlpha(0.30f) : accent);
    g.fillRoundedRectangle(bounds.getX() + earWidth, bounds.getY(), bounds.getWidth() - earWidth * 2.0f, 3.0f, 1.5f);

    // 4. MIDI / Voice Activity LED + Dedicated Non-Overlapping Zone Badge
    const float ledX = static_cast<float>(slotIndexLabel.getRight() + 3);
    const float ledY = static_cast<float>(slotIndexLabel.getBounds().getCentreY()) - 4.5f;
    const float ledIntensity = juce::jlimit(0.0f, 1.0f, activityLevel * 1.4f);
    g.setColour(juce::Colour(14, 18, 24));
    g.fillEllipse(ledX - 1.0f, ledY - 1.0f, 11.0f, 11.0f);
    g.setColour(ledIntensity > 0.02f ? juce::Colour(0xFF34D399).interpolatedWith(accent, 0.35f)
                                     : OpenWavLookAndFeel::borderColour.withAlpha(0.6f));
    g.fillEllipse(ledX, ledY, 9.0f, 9.0f);

    if (!zoneBadgeBounds.isEmpty())
    {
        auto zp = zoneBadgeBounds.toFloat().reduced(0.0f, 3.0f);
        g.setColour(accent.withAlpha(0.14f));
        g.fillRoundedRectangle(zp, 4.0f);
        g.setColour(accent);
        g.setFont(OpenWavLookAndFeel::getMonoFont(10.0f));
        g.drawText(juce::String(currentSlot.sampleMap.zones.size()) + "z", zp, juce::Justification::centred, false);
    }

    // 5. Expanded Mini Zone Map & Keybed Visualization
    if (!currentSlot.collapsed && !miniMapBounds.isEmpty())
    {
        g.setColour(juce::Colour(0xFF0E1218));
        g.fillRoundedRectangle(miniMapBounds, 5.0f);
        g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.75f));
        g.drawRoundedRectangle(miniMapBounds, 5.0f, 1.0f);

        auto keyStrip = miniMapBounds.withHeight(15.0f).withBottomY(miniMapBounds.getBottom());
        auto zoneArea = miniMapBounds.withTrimmedBottom(15.0f).reduced(2.0f, 2.0f);

        constexpr int kMinNote = 24;
        constexpr int kMaxNote = 96;
        constexpr int kSpan = kMaxNote - kMinNote + 1;
        const float noteW = keyStrip.getWidth() / static_cast<float>(kSpan);

        for (int n = kMinNote; n <= kMaxNote; ++n)
        {
            const float nx = keyStrip.getX() + static_cast<float>(n - kMinNote) * noteW;
            const int pc = n % 12;
            const bool isBlack = (pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10);

            if (pc == 0)
            {
                g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.35f));
                g.drawVerticalLine(juce::roundToInt(nx), zoneArea.getY(), zoneArea.getBottom());
            }

            if (n == previewNoteHeld)
                g.setColour(accent);
            else
                g.setColour(isBlack ? juce::Colour(0xFF1A202C) : juce::Colour(0xFFCBD5E1).withAlpha(0.72f));
            g.fillRect(nx, keyStrip.getY(), std::max(1.0f, noteW - 0.5f), keyStrip.getHeight() - 1.0f);
        }

        if (currentSlot.sampleMap.zones.empty())
        {
            g.setColour(OpenWavLookAndFeel::textSecondary);
            g.setFont(OpenWavLookAndFeel::getBodyFont(11.0f, false));
            g.drawText("Empty Instrument - Click to Load (.owmap / .sfz / .zip) or click Edit Map",
                       zoneArea.reduced(8.0f, 0.0f), juce::Justification::centred, true);
        }
        else
        {
            for (const auto& z : currentSlot.sampleMap.zones)
            {
                const int lo = juce::jlimit(kMinNote, kMaxNote, z.keyLow);
                const int hi = juce::jlimit(kMinNote, kMaxNote, std::max(z.keyLow, z.keyHigh));
                const float x1 = zoneArea.getX() + (static_cast<float>(lo - kMinNote) / kSpan) * zoneArea.getWidth();
                const float x2 = zoneArea.getX() + (static_cast<float>(hi - kMinNote + 1) / kSpan) * zoneArea.getWidth();

                const float yTop = zoneArea.getY() + (1.0f - static_cast<float>(juce::jlimit(0, 127, z.velHigh)) / 127.0f) * zoneArea.getHeight();
                const float yBot = zoneArea.getY() + (1.0f - static_cast<float>(juce::jlimit(0, 127, z.velLow)) / 127.0f) * zoneArea.getHeight();

                juce::Rectangle<float> zr(x1, yTop, std::max(2.0f, x2 - x1 - 0.5f), std::max(4.0f, yBot - yTop));
                juce::Colour zCol = z.isReleaseTrigger ? juce::Colour(0xFFF59E0B) : accent;
                g.setColour(zCol.withAlpha(0.32f));
                g.fillRoundedRectangle(zr, 2.0f);
                g.setColour(zCol.withAlpha(0.85f));
                g.drawRoundedRectangle(zr, 2.0f, 0.9f);
            }
        }
    }

    OpenWavLookAndFeel::drawButtonGroup(g, { &moveUpBtn, &moveDownBtn, &deleteBtn });
    g.setColour(activeInEditor ? accent.withAlpha(0.85f) : juce::Colour(52, 58, 68));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 5.0f, activeInEditor ? 1.4f : 1.0f);
}

void SamplerRackUnitComponent::resized()
{
    auto area = getLocalBounds().reduced(38, 7);
    auto header = area.removeFromTop(32);

    foldBtn.setBounds(header.removeFromLeft(26).reduced(0, 3));
    header.removeFromLeft(2);
    slotIndexLabel.setBounds(header.removeFromLeft(30));
    header.removeFromLeft(15); // Space for MIDI Activity LED

    // Right-side controls in header (adaptively sized so nothing ever overlaps!)
    deleteBtn.setBounds(header.removeFromRight(26).reduced(0, 3));
    header.removeFromRight(3);
    moveDownBtn.setBounds(header.removeFromRight(26).reduced(0, 3));
    header.removeFromRight(3);
    moveUpBtn.setBounds(header.removeFromRight(26).reduced(0, 3));
    header.removeFromRight(6);

    editMapBtn.setBounds(header.removeFromRight(64).reduced(0, 3));
    header.removeFromRight(4);

    if (header.getWidth() >= 360)
    {
        loadMapBtn.setVisible(true);
        loadMapBtn.setBounds(header.removeFromRight(44).reduced(0, 3));
        header.removeFromRight(5);
    }
    else
    {
        loadMapBtn.setVisible(false);
    }

    soloBtn.setBounds(header.removeFromRight(26).reduced(0, 3));
    header.removeFromRight(3);
    muteBtn.setBounds(header.removeFromRight(26).reduced(0, 3));
    header.removeFromRight(6);

    const int outW = juce::jlimit(100, 146, header.getWidth() / 3);
    outputBusCombo.setBounds(header.removeFromRight(outW).reduced(0, 3));
    header.removeFromRight(5);

    const int midiW = juce::jlimit(88, 114, header.getWidth() / 3);
    midiChannelCombo.setBounds(header.removeFromRight(midiW).reduced(0, 3));
    header.removeFromRight(5);

    if (header.getWidth() >= 110)
    {
        zoneBadgeBounds = header.removeFromRight(42);
        header.removeFromRight(5);
    }
    else
    {
        zoneBadgeBounds = {};
    }

    nameEditor.setBounds(header.reduced(0, 4));

    if (currentSlot.collapsed)
    {
        miniMapBounds = {};
        return;
    }

    area.removeFromTop(8);
    auto body = area;

    // Right side: 5 clean Hardware Rotary Knobs (matching Effects Rack!)
    const int paramsW = juce::jlimit(290, 410, static_cast<int>(body.getWidth() * 0.48f));
    auto paramsArea = body.removeFromRight(paramsW);
    body.removeFromRight(10);

    miniMapBounds = body.toFloat();

    constexpr int numKnobs = 5;
    const int knobColW = paramsArea.getWidth() / numKnobs;

    for (int i = 0; i < numKnobs; ++i)
    {
        auto col = juce::Rectangle<int>(paramsArea.getX() + i * knobColW,
                                        paramsArea.getY(),
                                        knobColW,
                                        paramsArea.getHeight());
        auto& k = paramKnobs[static_cast<size_t>(i)];
        k.title.setBounds(col.removeFromTop(15));
        k.readout.setBounds(col.removeFromBottom(15));
        k.knob.setBounds(col.reduced(2));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  SamplerRackComponent
// ─────────────────────────────────────────────────────────────────────────────
SamplerRackComponent::SamplerRackComponent(OpenWavAudioProcessor& processor, AudioEngine& engine)
    : audioProcessor(processor), audioEngine(engine), rackBay(*this)
{
    rackTitleLabel.setText("SAMPLER RACK", juce::dontSendNotification);
    rackTitleLabel.setFont(OpenWavLookAndFeel::getTitleFont(14.0f));
    rackTitleLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    addAndMakeVisible(rackTitleLabel);

    rackSubtitleLabel.setText("Multi-Instrument MIDI & DAW Stem Router", juce::dontSendNotification);
    rackSubtitleLabel.setFont(OpenWavLookAndFeel::getBodyFont(10.5f, false));
    rackSubtitleLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    addAndMakeVisible(rackSubtitleLabel);

    slotCountLabel.setText("0 Instruments", juce::dontSendNotification);
    slotCountLabel.setFont(OpenWavLookAndFeel::getBodyFont(11.0f, true));
    slotCountLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    addAndMakeVisible(slotCountLabel);

    addSamplerBtn.setTooltip("Add a new SampleMap instrument to the Sampler Rack");
    addSamplerBtn.onClick = [this] { showAddSamplerMenu(); };
    addAndMakeVisible(addSamplerBtn);

    loadMapFilesBtn.setComponentID("iconOnlyBtn");
    loadMapFilesBtn.setTooltip("Load one or more .owmap, .samplemap, .xml, .sfz, or .zip instruments into the Sampler Rack");
    loadMapFilesBtn.onClick = [this] { promptLoadMultipleMapFiles(); };
    addAndMakeVisible(loadMapFilesBtn);

    expandAllBtn.setComponentID("iconOnlyBtn");
    expandAllBtn.setTooltip("Expand all Sampler Rack instruments");
    expandAllBtn.onClick = [this] { setAllCollapsed(false); };
    addAndMakeVisible(expandAllBtn);

    foldAllBtn.setComponentID("iconOnlyBtn");
    foldAllBtn.setTooltip("Fold all SampleMaps into compact 1U rack strips");
    foldAllBtn.onClick = [this] { setAllCollapsed(true); };
    addAndMakeVisible(foldAllBtn);

    openMixerBtn.setComponentID("iconOnlyBtn");
    openMixerBtn.setTooltip("Open the Multi-Out & Console Mixer");
    openMixerBtn.onClick = [this] {
        if (onOpenMixerRequested)
            onOpenMixerRequested();
    };
    addAndMakeVisible(openMixerBtn);

    clearAllBtn.setComponentID("iconOnlyBtn");
    clearAllBtn.setTooltip("Clear all Sampler Rack instruments");
    clearAllBtn.onClick = [this] {
        rackState.slots.clear();
        rackState.activeEditSlotIndex = 0;
        audioProcessor.setSamplerRackState(rackState, false);
        refreshUnits();
        if (onStateChanged) onStateChanged();
    };
    addAndMakeVisible(clearAllBtn);

    OpenWavLookAndFeel::groupButtons({ &loadMapFilesBtn, &expandAllBtn, &foldAllBtn, &openMixerBtn, &clearAllBtn });

    rackViewport.setViewedComponent(&rackBay, false);
    rackViewport.setScrollBarsShown(true, false);
    addAndMakeVisible(rackViewport);

    startTimerHz(25);
    refreshUnits();
}

SamplerRackComponent::~SamplerRackComponent()
{
    stopTimer();
}

juce::String SamplerRackComponent::getOutputBusLabel(int busIdx) const
{
    const auto mixer = audioProcessor.getMixerState();
    const int idx = juce::jlimit(0, AudioEngine::MaxOutputBuses - 1, busIdx);
    const juce::String hwName = (idx == 0) ? "Out 1-2" : AudioEngine::getDefaultOutputBusName(idx);
    const juce::String custom = mixer.strips[static_cast<size_t>(idx)].name.trim();
    const juce::String defName = (idx == 0) ? "Main 1-2" : AudioEngine::getDefaultOutputBusName(idx);
    if (custom.isNotEmpty() && !custom.equalsIgnoreCase(hwName) && !custom.equalsIgnoreCase(defName))
        return hwName + " (" + custom + ")";
    return (idx == 0) ? "Out 1-2 (Main)" : hwName;
}

SamplerRackState SamplerRackComponent::getState() const
{
    return rackState;
}

void SamplerRackComponent::setState(const SamplerRackState& state)
{
    rackState = state;
    for (auto& sl : rackState.slots)
    {
        sl.outputBus = juce::jlimit(0, AudioEngine::MaxOutputBuses - 1, sl.outputBus);
        sl.sampleMap.outputBus = sl.outputBus;
    }
    audioProcessor.setSamplerRackState(rackState, true);
    refreshUnits();
}

int SamplerRackComponent::foldSampleMapIntoRack(const SampleMapState& mapState, const juce::String& preferredName)
{
    if (rackState.slots.empty())
    {
        SamplerRackSlotState sl;
        sl.id = makeUniqueSlotId();
        sl.name = preferredName.isNotEmpty() ? preferredName : inferMapDisplayName(mapState, "Sampler 1");
        sl.midiChannel = mapState.midiChannel;
        sl.outputBus = juce::jlimit(0, AudioEngine::MaxOutputBuses - 1, mapState.outputBus);
        sl.collapsed = true;
        sl.accentHex = kSlotAccentPalette[0];
        sl.sampleMap = mapState;
        sl.sampleMap.outputBus = sl.outputBus;
        rackState.slots.push_back(std::move(sl));
        rackState.activeEditSlotIndex = 0;
    }
    else
    {
        const int idx = juce::jlimit(0, static_cast<int>(rackState.slots.size()) - 1, rackState.activeEditSlotIndex);
        auto& sl = rackState.slots[static_cast<size_t>(idx)];
        sl.sampleMap = mapState;
        sl.outputBus = juce::jlimit(0, AudioEngine::MaxOutputBuses - 1, mapState.outputBus);
        if (preferredName.isNotEmpty())
            sl.name = preferredName;
        else if (sl.name.startsWith("Sampler ") && !mapState.zones.empty())
            sl.name = inferMapDisplayName(mapState, sl.name);
        sl.collapsed = true;
        rackState.activeEditSlotIndex = idx;
    }

    audioProcessor.setSamplerRackState(rackState, true);
    refreshUnits();
    if (onStateChanged) onStateChanged();
    return rackState.activeEditSlotIndex;
}

void SamplerRackComponent::updateActiveSlotSampleMap(const SampleMapState& mapState)
{
    if (rackState.slots.empty())
        return;

    const int idx = juce::jlimit(0, static_cast<int>(rackState.slots.size()) - 1, rackState.activeEditSlotIndex);
    auto& sl = rackState.slots[static_cast<size_t>(idx)];
    sl.sampleMap = mapState;
    sl.outputBus = juce::jlimit(0, AudioEngine::MaxOutputBuses - 1, mapState.outputBus);
    audioProcessor.setSamplerRackState(rackState, false);
    if (isShowing())
        refreshUnits();
}

void SamplerRackComponent::addEmptySlot(const SampleMapState& initialMap, const juce::String& customName)
{
    if (static_cast<int>(rackState.slots.size()) >= AudioEngine::MaxSamplerRackSlots)
        return;

    const int newIdx = static_cast<int>(rackState.slots.size());
    SamplerRackSlotState sl;
    sl.id = makeUniqueSlotId();
    sl.name = customName.isNotEmpty() ? customName : inferMapDisplayName(initialMap, "Sampler " + juce::String(newIdx + 1));
    sl.midiChannel = (newIdx == 0 && initialMap.midiChannel == 0) ? 0 : ((newIdx < 16) ? (newIdx + 1) : 0);
    sl.outputBus = (newIdx < AudioEngine::MaxOutputBuses) ? newIdx : 0;
    sl.accentHex = kSlotAccentPalette[newIdx % 8];
    sl.sampleMap = initialMap;
    sl.sampleMap.outputBus = sl.outputBus;

    rackState.slots.push_back(std::move(sl));
    rackState.activeEditSlotIndex = newIdx;
    audioProcessor.setSamplerRackState(rackState, true);
    refreshUnits();
    if (onStateChanged) onStateChanged();
}

bool SamplerRackComponent::createSlotFromFile(const juce::File& file)
{
    if (!file.existsAsFile() || static_cast<int>(rackState.slots.size()) >= AudioEngine::MaxSamplerRackSlots)
        return false;

    if (file.hasFileExtension("owmap;samplemap;xml;sfz;zip"))
    {
        SampleMapState parsed;
        juce::String err;
        if (!SampleMapComponent::parseSampleMapFromFile(file, parsed, err))
            return false;

        addEmptySlot(parsed, file.getFileNameWithoutExtension());
        return true;
    }

    if (file.hasFileExtension("wav;mp3;flac;aif;aiff;aifc;ogg;m4a;caf;aac"))
    {
        SampleMapState singleMap;
        SampleMapZoneState z;
        z.filePath = file.getFullPathName();
        z.sampleName = file.getFileName();
        z.rootNote = 60;
        z.keyLow = 24;
        z.keyHigh = 96;
        z.velLow = 0;
        z.velHigh = 127;
        singleMap.zones.push_back(z);
        addEmptySlot(singleMap, file.getFileNameWithoutExtension());
        return true;
    }

    return false;
}

void SamplerRackComponent::showAddSamplerMenu()
{
    juce::PopupMenu m;
    m.addItem(1, "New Empty Sampler Instrument");
    m.addItem(2, "Load SampleMap / SFZ / ZIP File...");
    m.addItem(3, "Import Current Sample Map Editor");
    m.addItem(4, "Create Instrument from Loaded Audio File", audioEngine.getCurrentFile().existsAsFile());

    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&addSamplerBtn),
                    [this](int res) {
        if (res == 1)
        {
            addEmptySlot();
        }
        else if (res == 2)
        {
            promptLoadMultipleMapFiles();
        }
        else if (res == 3)
        {
            SampleMapState cur = getCurrentEditorSampleMapCallback ? getCurrentEditorSampleMapCallback()
                                                                   : audioProcessor.getSampleMapState();
            addEmptySlot(cur, inferMapDisplayName(cur, "Sample Map " + juce::String(rackState.slots.size() + 1)));
        }
        else if (res == 4)
        {
            auto curFile = audioEngine.getCurrentFile();
            if (curFile.existsAsFile())
                createSlotFromFile(curFile);
        }
    });
}

void SamplerRackComponent::promptLoadMultipleMapFiles()
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Load Sample Map / SFZ / ZIP Instruments into Sampler Rack",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.owmap;*.samplemap;*.xml;*.sfz;*.zip;*.wav;*.flac;*.aiff;*.mp3");

    juce::Component::SafePointer<SamplerRackComponent> safeThis(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode
                             | juce::FileBrowserComponent::canSelectFiles
                             | juce::FileBrowserComponent::canSelectMultipleItems,
                         [safeThis, chooser](const juce::FileChooser& fc) {
        if (safeThis == nullptr) return;
        for (const auto& f : fc.getResults())
            safeThis->createSlotFromFile(f);
    });
}

void SamplerRackComponent::loadMapFileIntoSlot(int index)
{
    if (index < 0 || index >= static_cast<int>(rackState.slots.size()))
        return;

    auto chooser = std::make_shared<juce::FileChooser>(
        "Load Sample Map / SFZ / ZIP into Slot #" + juce::String(index + 1),
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.owmap;*.samplemap;*.xml;*.sfz;*.zip;*.wav;*.flac;*.aiff;*.mp3");

    juce::Component::SafePointer<SamplerRackComponent> safeThis(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                         [safeThis, chooser, index](const juce::FileChooser& fc) {
        if (safeThis == nullptr) return;
        auto file = fc.getResult();
        if (!file.existsAsFile() || index >= static_cast<int>(safeThis->rackState.slots.size()))
            return;

        auto& sl = safeThis->rackState.slots[static_cast<size_t>(index)];
        if (file.hasFileExtension("owmap;samplemap;xml;sfz;zip"))
        {
            SampleMapState parsed;
            juce::String err;
            if (SampleMapComponent::parseSampleMapFromFile(file, parsed, err))
            {
                parsed.outputBus = sl.outputBus;
                sl.sampleMap = parsed;
                sl.name = file.getFileNameWithoutExtension();
                safeThis->audioProcessor.setSamplerRackState(safeThis->rackState, true);
                safeThis->refreshUnits();
                if (safeThis->onStateChanged) safeThis->onStateChanged();
            }
        }
        else
        {
            SampleMapState singleMap;
            singleMap.outputBus = sl.outputBus;
            SampleMapZoneState z;
            z.filePath = file.getFullPathName();
            z.sampleName = file.getFileName();
            z.rootNote = 60;
            z.keyLow = 24;
            z.keyHigh = 96;
            singleMap.zones.push_back(z);
            sl.sampleMap = singleMap;
            sl.name = file.getFileNameWithoutExtension();
            safeThis->audioProcessor.setSamplerRackState(safeThis->rackState, true);
            safeThis->refreshUnits();
            if (safeThis->onStateChanged) safeThis->onStateChanged();
        }
    });
}

void SamplerRackComponent::openSlotInSampleMapEditor(int index)
{
    if (index < 0 || index >= static_cast<int>(rackState.slots.size()))
        return;

    rackState.activeEditSlotIndex = index;
    audioProcessor.setSamplerRackState(rackState, false);
    refreshUnits();
    if (onEditSlotInSampleMapRequested)
        onEditSlotInSampleMapRequested(index, rackState.slots[static_cast<size_t>(index)]);
}

void SamplerRackComponent::updateSlotState(int index, const SamplerRackSlotState& updated, bool rebuildLayout)
{
    if (index < 0 || index >= static_cast<int>(rackState.slots.size()))
        return;

    rackState.slots[static_cast<size_t>(index)] = updated;
    audioProcessor.setSamplerRackState(rackState, false);

    if (rebuildLayout)
    {
        if (index < unitComponents.size())
            unitComponents[index]->syncFromSlot(updated, index, static_cast<int>(rackState.slots.size()),
                                                index == rackState.activeEditSlotIndex);
        rackBay.resized();
        rackBay.repaint();
    }

    if (onStateChanged)
        onStateChanged();
}

void SamplerRackComponent::moveSlot(int fromIdx, int toIdx)
{
    const int n = static_cast<int>(rackState.slots.size());
    if (fromIdx < 0 || fromIdx >= n || toIdx < 0 || toIdx >= n || fromIdx == toIdx)
        return;

    std::swap(rackState.slots[static_cast<size_t>(fromIdx)], rackState.slots[static_cast<size_t>(toIdx)]);
    if (rackState.activeEditSlotIndex == fromIdx)
        rackState.activeEditSlotIndex = toIdx;
    else if (rackState.activeEditSlotIndex == toIdx)
        rackState.activeEditSlotIndex = fromIdx;

    audioProcessor.setSamplerRackState(rackState, false);
    refreshUnits();
    if (onStateChanged) onStateChanged();
}

void SamplerRackComponent::removeSlot(int index)
{
    const int n = static_cast<int>(rackState.slots.size());
    if (index < 0 || index >= n)
        return;

    audioEngine.stopAllZoneVoices(true);
    rackState.slots.erase(rackState.slots.begin() + index);
    if (rackState.activeEditSlotIndex >= static_cast<int>(rackState.slots.size()))
        rackState.activeEditSlotIndex = std::max(0, static_cast<int>(rackState.slots.size()) - 1);

    audioProcessor.setSamplerRackState(rackState, false);
    refreshUnits();
    if (onStateChanged) onStateChanged();
}

void SamplerRackComponent::setAllCollapsed(bool shouldCollapse)
{
    for (auto& sl : rackState.slots)
        sl.collapsed = shouldCollapse;
    audioProcessor.setSamplerRackState(rackState, false);
    refreshUnits();
    if (onStateChanged) onStateChanged();
}

void SamplerRackComponent::refreshUnits()
{
    const int total = static_cast<int>(rackState.slots.size());
    while (unitComponents.size() > total)
        unitComponents.removeLast();

    while (unitComponents.size() < total)
    {
        const int idx = unitComponents.size();
        auto* u = new SamplerRackUnitComponent(*this, audioProcessor, audioEngine, idx, total, idx == rackState.activeEditSlotIndex);
        unitComponents.add(u);
        rackBay.addAndMakeVisible(u);
    }

    for (int i = 0; i < total; ++i)
    {
        unitComponents[i]->syncFromSlot(rackState.slots[static_cast<size_t>(i)],
                                        i,
                                        total,
                                        i == rackState.activeEditSlotIndex);
    }

    slotCountLabel.setText(juce::String(total) + (total == 1 ? " Instrument" : " Instruments"),
                           juce::dontSendNotification);

    rackBay.resized();
    rackBay.repaint();
    repaint();
}

void SamplerRackComponent::timerCallback()
{
    if (!isShowing())
        return;

    const int n = std::min(unitComponents.size(), AudioEngine::MaxSamplerRackSlots);
    for (int i = 0; i < n; ++i)
    {
        const float pk = audioEngine.consumeRackSlotPeak(i);
        unitComponents[i]->updatePeakMeter(pk);
    }
}

bool SamplerRackComponent::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& f : files)
    {
        juce::File file(f);
        if (file.hasFileExtension("owmap;samplemap;xml;sfz;zip;wav;mp3;flac;aif;aiff;aifc;ogg;m4a;caf;aac"))
            return true;
    }
    return false;
}

void SamplerRackComponent::fileDragEnter(const juce::StringArray&, int, int)
{
    isFileDragOver = true;
    repaint();
}

void SamplerRackComponent::fileDragExit(const juce::StringArray&)
{
    isFileDragOver = false;
    repaint();
}

void SamplerRackComponent::filesDropped(const juce::StringArray& files, int, int)
{
    isFileDragOver = false;
    for (const auto& f : files)
        createSlotFromFile(juce::File(f));
    repaint();
}

void SamplerRackComponent::paint(juce::Graphics& g)
{
    g.fillAll(OpenWavLookAndFeel::bgDark);
    g.setColour(OpenWavLookAndFeel::borderColour);
    g.drawHorizontalLine(52, 0.0f, static_cast<float>(getWidth()));
    OpenWavLookAndFeel::drawButtonGroup(g, { &loadMapFilesBtn, &expandAllBtn, &foldAllBtn, &openMixerBtn, &clearAllBtn });
}

void SamplerRackComponent::paintOverChildren(juce::Graphics& g)
{
    if (isFileDragOver)
        OpenWavLookAndFeel::drawDropOverlay(g, getLocalBounds(), "Drop SampleMap (.owmap / .sfz / .zip) or Audio Files to Add Instrument");
}

void SamplerRackComponent::resized()
{
    auto area = getLocalBounds();

    auto topBar = area.removeFromTop(52).reduced(12, 10);
    const int btnH = 30;

    auto titleArea = topBar.removeFromLeft(210);
    rackTitleLabel.setBounds(titleArea.removeFromTop(18));
    rackSubtitleLabel.setBounds(titleArea);

    slotCountLabel.setBounds(topBar.removeFromLeft(96).withHeight(btnH));
    topBar.removeFromLeft(6);

    addSamplerBtn.setBounds(topBar.removeFromLeft(118).withHeight(btnH));
    topBar.removeFromLeft(8);

    loadMapFilesBtn.setBounds(topBar.removeFromLeft(32).withHeight(btnH));
    topBar.removeFromLeft(4);
    expandAllBtn.setBounds(topBar.removeFromLeft(32).withHeight(btnH));
    topBar.removeFromLeft(4);
    foldAllBtn.setBounds(topBar.removeFromLeft(32).withHeight(btnH));
    topBar.removeFromLeft(4);
    openMixerBtn.setBounds(topBar.removeFromLeft(32).withHeight(btnH));
    topBar.removeFromLeft(4);
    clearAllBtn.setBounds(topBar.removeFromLeft(32).withHeight(btnH));

    rackViewport.setBounds(area);
    rackBay.resized();
}

void SamplerRackComponent::RackBayComponent::paint(juce::Graphics& g)
{
    auto area = getLocalBounds();
    g.fillAll(juce::Colour(14, 16, 18));

    constexpr int railWidth = 38;
    auto leftRail = area.removeFromLeft(railWidth);
    auto rightRail = area.removeFromRight(railWidth);

    g.setColour(juce::Colour(22, 25, 29));
    g.fillRect(leftRail);
    g.fillRect(rightRail);

    g.setColour(juce::Colour(42, 46, 54));
    g.drawVerticalLine(leftRail.getRight(), 0.0f, static_cast<float>(getHeight()));
    g.drawVerticalLine(rightRail.getX(), 0.0f, static_cast<float>(getHeight()));

    constexpr int uHeight = 44;
    g.setColour(juce::Colour(34, 38, 44));
    for (int y = 22; y < getHeight(); y += uHeight)
    {
        g.fillEllipse(leftRail.getCentreX() - 3.5f, static_cast<float>(y) - 3.5f, 7.0f, 7.0f);
        g.fillEllipse(rightRail.getCentreX() - 3.5f, static_cast<float>(y) - 3.5f, 7.0f, 7.0f);
    }

    if (owner.unitComponents.isEmpty())
        owner.drawEmptyRackPlaceholder(g, getLocalBounds().reduced(railWidth + 20, 20));
}

void SamplerRackComponent::RackBayComponent::resized()
{
    constexpr int gap = 6;
    constexpr int topPad = 12;

    int requiredHeight = topPad + 24;
    for (auto* u : owner.unitComponents)
        requiredHeight += u->getDesiredHeight() + gap;

    const int viewportHeight = owner.rackViewport.getHeight();
    const int targetHeight = std::max(requiredHeight, viewportHeight);
    const int targetWidth = owner.rackViewport.getWidth();

    if (targetWidth > 0 && (getWidth() != targetWidth || getHeight() != targetHeight))
    {
        setSize(targetWidth, targetHeight);
        return;
    }

    int y = topPad;
    for (auto* u : owner.unitComponents)
    {
        const int h = u->getDesiredHeight();
        u->setBounds(12, y, getWidth() - 24, h);
        y += h + gap;
    }
}

void SamplerRackComponent::drawEmptyRackPlaceholder(juce::Graphics& g, const juce::Rectangle<int>& area)
{
    auto center = area.toFloat();
    const float boxW = juce::jmin(560.0f, center.getWidth());
    const float boxH = 205.0f;
    auto box = juce::Rectangle<float>(center.getCentreX() - boxW * 0.5f,
                                      center.getCentreY() - boxH * 0.5f,
                                      boxW, boxH);

    g.setColour(juce::Colour(45, 50, 60));
    float dashPattern[] = { 6.0f, 4.0f };
    g.drawDashedLine(juce::Line<float>(box.getTopLeft(), box.getTopRight()), dashPattern, 2, 1.5f);
    g.drawDashedLine(juce::Line<float>(box.getTopRight(), box.getBottomRight()), dashPattern, 2, 1.5f);
    g.drawDashedLine(juce::Line<float>(box.getBottomRight(), box.getBottomLeft()), dashPattern, 2, 1.5f);
    g.drawDashedLine(juce::Line<float>(box.getBottomLeft(), box.getTopLeft()), dashPattern, 2, 1.5f);

    g.setColour(juce::Colours::white);
    g.setFont(OpenWavLookAndFeel::getTitleFont(16.0f));
    g.drawText("Sampler Rack is Empty", box.removeFromTop(66), juce::Justification::centred);

    g.setColour(OpenWavLookAndFeel::textSecondary);
    g.setFont(OpenWavLookAndFeel::getBodyFont(12.5f, false));
    g.drawFittedText("Click '+ Add Sampler', drop .owmap / .sfz / .zip instruments here, or click 'Fold to Rack' inside the Sample Map Editor.",
                     box.removeFromTop(46).reduced(24.0f, 0.0f).toNearestInt(),
                     juce::Justification::centred, 2);

    g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.85f));
    g.setFont(OpenWavLookAndFeel::getBodyFont(12.0f, false));
    g.drawText("Each loaded instrument can be folded into the rack and routed by MIDI Channel (1-16) to its own DAW stereo output (Out 1-16).",
               box.removeFromTop(34), juce::Justification::centred);
}

} // namespace openwav
