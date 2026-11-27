#include "MultiOutMixerComponent.h"
#include <cmath>
#include <algorithm>

namespace openwav
{

namespace
{
class InstrumentRoutingRowComponent : public juce::Component
{
public:
    explicit InstrumentRoutingRowComponent(MultiOutMixerComponent& ownerComp)
        : owner(ownerComp)
    {
        auditionBtn.setComponentID("iconOnlyBtn");
        auditionBtn.setButtonText("Play");
        auditionBtn.setTooltip("Audition this instrument through its assigned output track");
        auditionBtn.onClick = [this] {
            if (rowIdx >= 0)
                owner.auditionInstrumentRow(rowIdx);
        };
        addAndMakeVisible(auditionBtn);

        midiCombo.addItem("MIDI: Omni", 1);
        for (int ch = 1; ch <= 16; ++ch)
            midiCombo.addItem("MIDI: Ch " + juce::String(ch), ch + 1);
        midiCombo.setTooltip("Incoming MIDI channel that triggers this instrument");
        midiCombo.onChange = [this] {
            if (updating || rowIdx < 0) return;
            const int id = midiCombo.getSelectedId();
            if (id >= 1 && id <= 17)
                owner.setInstrumentMidiChannel(rowIdx, id - 1);
        };
        addAndMakeVisible(midiCombo);

        busCombo.setTooltip("Select the stereo output track for this entire instrument");
        busCombo.onChange = [this] {
            if (updating || rowIdx < 0) return;
            const int id = busCombo.getSelectedId();
            if (id >= 1 && id <= AudioEngine::MaxOutputBuses)
                owner.setInstrumentOutputBus(rowIdx, id - 1);
        };
        addAndMakeVisible(busCombo);
    }

    void updateFromRow(int index, const MultiOutMixerComponent::RoutableInstrumentRow& row)
    {
        updating = true;
        rowIdx = index;
        currentRow = row;

        auditionBtn.setEnabled(row.canAudition);
        midiCombo.setVisible(row.hasMidiChannel);
        if (row.hasMidiChannel)
            midiCombo.setSelectedId(juce::jlimit(0, 16, row.midiChannel) + 1, juce::dontSendNotification);

        busCombo.clear(juce::dontSendNotification);
        for (int b = 0; b < AudioEngine::MaxOutputBuses; ++b)
            busCombo.addItem(owner.getBusDisplayLabel(b), b + 1);
        busCombo.setSelectedId(juce::jlimit(0, AudioEngine::MaxOutputBuses - 1, row.outputBus) + 1,
                               juce::dontSendNotification);

        updating = false;
        resized();
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced(2.0f, 2.0f);
        g.setColour((rowIdx % 2 == 0) ? OpenWavLookAndFeel::bgCard.withAlpha(0.82f)
                                      : OpenWavLookAndFeel::bgHover.withAlpha(0.42f));
        g.fillRoundedRectangle(b, 6.0f);

        g.setColour(currentRow.outputBus > 0
                        ? OpenWavLookAndFeel::accentCyan.withAlpha(0.42f)
                        : OpenWavLookAndFeel::borderColour.withAlpha(0.55f));
        g.drawRoundedRectangle(b, 6.0f, 1.0f);

        // Kind / Slot pill
        juce::String tagStr = "MAIN";
        if (currentRow.kind == MultiOutMixerComponent::RoutableInstrumentRow::Kind::SingleSampler)
            tagStr = "PREVIEW";
        else if (currentRow.kind == MultiOutMixerComponent::RoutableInstrumentRow::Kind::RackSlot)
            tagStr = juce::String::formatted("RACK #%02d", currentRow.rackSlotIndex + 1);

        auto tagRect = juce::Rectangle<float>(42.0f, b.getY() + 8.0f, 68.0f, b.getHeight() - 16.0f);
        g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.15f));
        g.fillRoundedRectangle(tagRect, 4.0f);
        g.setColour(OpenWavLookAndFeel::accentCyan);
        g.setFont(OpenWavLookAndFeel::getMonoFont(10.0f));
        g.drawText(tagStr, tagRect, juce::Justification::centred, false);

        const float rightControlsX = static_cast<float>(currentRow.hasMidiChannel ? midiCombo.getX() : busCombo.getX());
        auto detailRect = juce::Rectangle<float>(rightControlsX - 96.0f, b.getY() + 8.0f, 86.0f, b.getHeight() - 16.0f);
        if (detailRect.getX() > tagRect.getRight() + 80.0f)
        {
            g.setColour(OpenWavLookAndFeel::bgDark.withAlpha(0.65f));
            g.fillRoundedRectangle(detailRect, 4.0f);
            g.setColour(OpenWavLookAndFeel::textSecondary);
            g.setFont(OpenWavLookAndFeel::getMonoFont(10.0f));
            g.drawText(currentRow.detailBadge, detailRect, juce::Justification::centred, true);
        }
        else
        {
            detailRect.setX(rightControlsX - 6.0f);
        }

        const float nameLeft = tagRect.getRight() + 10.0f;
        const float nameRight = std::max(nameLeft + 40.0f, detailRect.getX() - 10.0f);
        g.setColour(OpenWavLookAndFeel::textPrimary);
        g.setFont(OpenWavLookAndFeel::getBodyFont(13.0f, true));
        g.drawText(currentRow.name,
                   juce::Rectangle<float>(nameLeft, b.getY(), nameRight - nameLeft, b.getHeight()),
                   juce::Justification::centredLeft, true);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(10, 6);
        auditionBtn.setBounds(r.removeFromLeft(26).withSizeKeepingCentre(26, 26));

        const int busW = juce::jlimit(150, 210, r.getWidth() / 4 + 30);
        busCombo.setBounds(r.removeFromRight(busW).reduced(0, 2));
        r.removeFromRight(8);

        if (currentRow.hasMidiChannel)
        {
            const int midiW = juce::jlimit(112, 138, r.getWidth() / 5 + 24);
            midiCombo.setBounds(r.removeFromRight(midiW).reduced(0, 2));
        }
    }

private:
    MultiOutMixerComponent& owner;
    juce::TextButton auditionBtn;
    juce::ComboBox midiCombo;
    juce::ComboBox busCombo;
    MultiOutMixerComponent::RoutableInstrumentRow currentRow;
    int rowIdx { -1 };
    bool updating { false };
};
} // namespace

MultiOutMixerComponent::MultiOutMixerComponent(OpenWavAudioProcessor& processor, AudioEngine& engine)
    : audioProcessor(processor), audioEngine(engine), consoleBay(*this)
{
    // Top Toolbar Header Labels (Matches Effects Rack & Sampler Rack)
    titleLabel.setText("MULTI-OUT & MIXER", juce::dontSendNotification);
    titleLabel.setFont(OpenWavLookAndFeel::getTitleFont(14.0f));
    titleLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    addAndMakeVisible(titleLabel);

    statusSubtitleLabel.setFont(OpenWavLookAndFeel::getBodyFont(10.5f, false));
    statusSubtitleLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    addAndMakeVisible(statusSubtitleLabel);

    // Clean 2-Tab Switcher: [ Mixer Console ] [ Instrument Outputs ]
    auto setupModeBtn = [this](juce::TextButton& btn, int mode, const juce::String& tip) {
        btn.setClickingTogglesState(false);
        btn.setTooltip(tip);
        btn.onClick = [this, mode] {
            mixerState.viewLayoutMode = mode;
            modeConsoleBtn.setToggleState(mode == 0, juce::dontSendNotification);
            modeRoutingBtn.setToggleState(mode == 1, juce::dontSendNotification);
            audioProcessor.setMixerState(mixerState);
            if (onStateChanged) onStateChanged();
            resized();
            repaint();
        };
        addAndMakeVisible(btn);
    };

    setupModeBtn(modeConsoleBtn, 0, "Show spacious 8-Stereo-Bus + Master Console Mixer (click any channel's instrument pill to route instruments)");
    setupModeBtn(modeRoutingBtn, 1, "Show per-instrument MIDI Channel and DAW Multi-Output routing table");
    OpenWavLookAndFeel::groupButtons({ &modeConsoleBtn, &modeRoutingBtn });

    foldToMainBtn.setClickingTogglesState(true);
    foldToMainBtn.setToggleState(true, juce::dontSendNotification);
    foldToMainBtn.setTooltip("When enabled, any Aux output (Out 3-16) not yet activated in your DAW is summed into Out 1-2 so it remains audible. Turn off for strict discrete stem isolation.");
    foldToMainBtn.onClick = [this] {
        mixerState.foldInactiveBusesToMain = foldToMainBtn.getToggleState();
        audioEngine.setFoldInactiveBusesToMain(mixerState.foldInactiveBusesToMain);
        audioProcessor.setMixerState(mixerState);
        if (onStateChanged) onStateChanged();
        consoleBay.repaint();
        repaint();
    };
    addAndMakeVisible(foldToMainBtn);

    autoRouteBtn.setTooltip("Automatically assign loaded Sampler Rack instruments sequentially across Out 1-2 through Out 15-16");
    autoRouteBtn.onClick = [this] { autoRouteInstrumentsAcrossBuses(); };
    addAndMakeVisible(autoRouteBtn);

    resetRoutingBtn.setTooltip("Route all instruments back to Out 1-2 (Main)");
    resetRoutingBtn.onClick = [this] { resetAllInstrumentsToMain(); };
    addAndMakeVisible(resetRoutingBtn);

    resetMixerBtn.setTooltip("Reset all faders to 0.0 dB, pans to Center, and clear Mute / Solo");
    resetMixerBtn.onClick = [this] { resetMixerToUnity(); };
    addAndMakeVisible(resetMixerBtn);

    OpenWavLookAndFeel::groupButtons({ &autoRouteBtn, &resetRoutingBtn, &resetMixerBtn });

    openRackBtn.setTooltip("Open the Multi-SampleMap Sampler Rack");
    openRackBtn.onClick = [this] {
        if (onOpenSamplerRackRequested)
            onOpenSamplerRackRequested();
    };
    addAndMakeVisible(openRackBtn);

    // Console Viewport (smooth horizontal scroll if window is resized very narrow; expands to full width otherwise)
    consoleViewport.setViewedComponent(&consoleBay, false);
    consoleViewport.setScrollBarsShown(false, true);
    addAndMakeVisible(consoleViewport);

    // Instrument Routing List
    instrumentList.setModel(this);
    instrumentList.setRowHeight(44);
    instrumentList.setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    instrumentList.setColour(juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
    addChildComponent(instrumentList);

    // Initialize 8 Stereo Channel Strips inside ConsoleBayComponent
    for (int i = 0; i < AudioEngine::MaxOutputBuses; ++i)
    {
        auto& st = busStrips[static_cast<size_t>(i)];
        st.panKnobLnf.setAccentColour(OpenWavLookAndFeel::accentCyan);

        st.nameEditor.setText(mixerState.strips[static_cast<size_t>(i)].name, juce::dontSendNotification);
        st.nameEditor.setEditable(false, true, false);
        st.nameEditor.setJustificationType(juce::Justification::centred);
        st.nameEditor.setFont(OpenWavLookAndFeel::getBodyFont(11.5f, true));
        st.nameEditor.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textPrimary);
        st.nameEditor.setColour(juce::Label::backgroundColourId, OpenWavLookAndFeel::bgDark.withAlpha(0.70f));
        st.nameEditor.setColour(juce::Label::outlineColourId, OpenWavLookAndFeel::borderColour.withAlpha(0.55f));
        st.nameEditor.setTooltip("Double-click to rename this output track (e.g. Drums, Bass, Keys, Pads)");
        st.nameEditor.onTextChange = [this, i] {
            if (isUpdatingControls) return;
            juce::String txt = busStrips[static_cast<size_t>(i)].nameEditor.getText().trim();
            if (txt.isEmpty())
                txt = (i == 0) ? "Main 1-2" : AudioEngine::getDefaultOutputBusName(i);
            mixerState.strips[static_cast<size_t>(i)].name = txt;
            busStrips[static_cast<size_t>(i)].nameEditor.setText(txt, juce::dontSendNotification);
            instrumentList.updateContent();
            instrumentList.repaint();
            audioProcessor.setMixerState(mixerState);
            if (onStateChanged) onStateChanged();
        };
        consoleBay.addAndMakeVisible(st.nameEditor);

        st.assignBtn.setTooltip("Click to route any instrument to " + AudioEngine::getDefaultOutputBusName(i));
        st.assignBtn.onClick = [this, i] {
            showAssignMenuForBus(i, busStrips[static_cast<size_t>(i)].assignBtn);
        };
        consoleBay.addAndMakeVisible(st.assignBtn);

        st.panKnob.setLookAndFeel(&st.panKnobLnf);
        st.panKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        st.panKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        st.panKnob.setRange(-1.0, 1.0, 0.01);
        st.panKnob.setDoubleClickReturnValue(true, 0.0);
        st.panKnob.setTooltip("Stereo Balance Pan (Double-click for Center)");
        st.panKnob.onValueChange = [this, i] {
            if (isUpdatingControls) return;
            const float p = static_cast<float>(busStrips[static_cast<size_t>(i)].panKnob.getValue());
            mixerState.strips[static_cast<size_t>(i)].pan = p;
            busStrips[static_cast<size_t>(i)].panValueLabel.setText(formatPanText(p), juce::dontSendNotification);
            audioEngine.setBusPan(i, p);
            audioProcessor.setMixerState(mixerState);
            if (onStateChanged) onStateChanged();
        };
        consoleBay.addAndMakeVisible(st.panKnob);

        st.panValueLabel.setText("C", juce::dontSendNotification);
        st.panValueLabel.setJustificationType(juce::Justification::centred);
        st.panValueLabel.setFont(OpenWavLookAndFeel::getMonoFont(10.0f));
        st.panValueLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
        consoleBay.addAndMakeVisible(st.panValueLabel);

        st.muteBtn.setClickingTogglesState(true);
        st.muteBtn.setTooltip("Mute " + AudioEngine::getDefaultOutputBusName(i));
        st.muteBtn.setColour(juce::TextButton::buttonOnColourId, OpenWavLookAndFeel::favoriteRed.withAlpha(0.85f));
        st.muteBtn.onClick = [this, i] {
            if (isUpdatingControls) return;
            const bool m = busStrips[static_cast<size_t>(i)].muteBtn.getToggleState();
            mixerState.strips[static_cast<size_t>(i)].mute = m;
            audioEngine.setBusMute(i, m);
            audioProcessor.setMixerState(mixerState);
            if (onStateChanged) onStateChanged();
            consoleBay.repaint();
        };
        consoleBay.addAndMakeVisible(st.muteBtn);

        st.soloBtn.setClickingTogglesState(true);
        st.soloBtn.setTooltip("Solo " + AudioEngine::getDefaultOutputBusName(i));
        st.soloBtn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xFFD97706));
        st.soloBtn.onClick = [this, i] {
            if (isUpdatingControls) return;
            const bool s = busStrips[static_cast<size_t>(i)].soloBtn.getToggleState();
            mixerState.strips[static_cast<size_t>(i)].solo = s;
            audioEngine.setBusSolo(i, s);
            audioProcessor.setMixerState(mixerState);
            if (onStateChanged) onStateChanged();
            consoleBay.repaint();
        };
        consoleBay.addAndMakeVisible(st.soloBtn);

        if (i == 0)
        {
            st.fxBtn.setClickingTogglesState(true);
            st.fxBtn.setToggleState(true, juce::dontSendNotification);
            st.fxBtn.setTooltip("Enable or bypass the 21-Effect Performance Rack on Out 1-2 (Main)");
            st.fxBtn.onClick = [this] {
                if (isUpdatingControls) return;
                const bool fx = busStrips[0].fxBtn.getToggleState();
                mixerState.strips[0].sendToFxRack = fx;
                audioEngine.setBusSendToFxRack(0, fx);
                audioProcessor.setMixerState(mixerState);
                if (onStateChanged) onStateChanged();
                consoleBay.repaint();
            };
            consoleBay.addAndMakeVisible(st.fxBtn);
        }

        st.faderSlider.setSliderStyle(juce::Slider::LinearVertical);
        st.faderSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        st.faderSlider.setRange(-60.0, 6.0, 0.1);
        st.faderSlider.setSkewFactorFromMidPoint(-12.0);
        st.faderSlider.setDoubleClickReturnValue(true, 0.0);
        st.faderSlider.setValue(0.0, juce::dontSendNotification);
        st.faderSlider.setTooltip("Output Track Volume Fader (-60 dB to +6 dB, double-click for 0.0 dB)");
        st.faderSlider.onValueChange = [this, i] {
            if (isUpdatingControls) return;
            const float db = static_cast<float>(busStrips[static_cast<size_t>(i)].faderSlider.getValue());
            mixerState.strips[static_cast<size_t>(i)].gainDb = db;
            busStrips[static_cast<size_t>(i)].dbReadoutLabel.setText(formatDbText(db), juce::dontSendNotification);
            audioEngine.setBusGainDb(i, db);
            audioProcessor.setMixerState(mixerState);
            if (onStateChanged) onStateChanged();
        };
        consoleBay.addAndMakeVisible(st.faderSlider);

        st.dbReadoutLabel.setText("0.0 dB", juce::dontSendNotification);
        st.dbReadoutLabel.setJustificationType(juce::Justification::centred);
        st.dbReadoutLabel.setFont(OpenWavLookAndFeel::getMonoFont(10.5f));
        st.dbReadoutLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textPrimary);
        consoleBay.addAndMakeVisible(st.dbReadoutLabel);
    }

    // Master Channel Strip
    masterStrip.panKnobLnf.setAccentColour(juce::Colour(0xFF10B981));

    masterStrip.nameEditor.setText("Main Trim", juce::dontSendNotification);
    masterStrip.nameEditor.setJustificationType(juce::Justification::centred);
    masterStrip.nameEditor.setFont(OpenWavLookAndFeel::getBodyFont(11.5f, true));
    masterStrip.nameEditor.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    consoleBay.addAndMakeVisible(masterStrip.nameEditor);

    masterStrip.assignBtn.setButtonText("FX Rack");
    masterStrip.assignBtn.setTooltip("Open the 21-Effect Performance Rack view");
    masterStrip.assignBtn.onClick = [this] {
        if (onOpenFxRackRequested)
            onOpenFxRackRequested();
    };
    consoleBay.addAndMakeVisible(masterStrip.assignBtn);

    masterStrip.panKnob.setLookAndFeel(&masterStrip.panKnobLnf);
    masterStrip.panKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    masterStrip.panKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    masterStrip.panKnob.setRange(-1.0, 1.0, 0.01);
    masterStrip.panKnob.setDoubleClickReturnValue(true, 0.0);
    masterStrip.panKnob.setTooltip("Master Stereo Balance (Double-click for Center)");
    masterStrip.panKnob.onValueChange = [this] {
        if (isUpdatingControls) return;
        const float p = static_cast<float>(masterStrip.panKnob.getValue());
        mixerState.masterPan = p;
        masterStrip.panValueLabel.setText(formatPanText(p), juce::dontSendNotification);
        audioEngine.setMasterMixerPan(p);
        audioProcessor.setMixerState(mixerState);
        if (onStateChanged) onStateChanged();
    };
    consoleBay.addAndMakeVisible(masterStrip.panKnob);

    masterStrip.panValueLabel.setText("C", juce::dontSendNotification);
    masterStrip.panValueLabel.setJustificationType(juce::Justification::centred);
    masterStrip.panValueLabel.setFont(OpenWavLookAndFeel::getMonoFont(10.0f));
    masterStrip.panValueLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    consoleBay.addAndMakeVisible(masterStrip.panValueLabel);

    masterStrip.muteBtn.setButtonText("MUTE");
    masterStrip.muteBtn.setClickingTogglesState(true);
    masterStrip.muteBtn.setTooltip("Mute Master Output");
    masterStrip.muteBtn.setColour(juce::TextButton::buttonOnColourId, OpenWavLookAndFeel::favoriteRed.withAlpha(0.85f));
    masterStrip.muteBtn.onClick = [this] {
        if (isUpdatingControls) return;
        const bool m = masterStrip.muteBtn.getToggleState();
        mixerState.masterMute = m;
        audioEngine.setMasterMixerMute(m);
        audioProcessor.setMixerState(mixerState);
        if (onStateChanged) onStateChanged();
        consoleBay.repaint();
    };
    consoleBay.addAndMakeVisible(masterStrip.muteBtn);

    masterStrip.faderSlider.setSliderStyle(juce::Slider::LinearVertical);
    masterStrip.faderSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    masterStrip.faderSlider.setRange(-60.0, 6.0, 0.1);
    masterStrip.faderSlider.setSkewFactorFromMidPoint(-12.0);
    masterStrip.faderSlider.setDoubleClickReturnValue(true, 0.0);
    masterStrip.faderSlider.setValue(0.0, juce::dontSendNotification);
    masterStrip.faderSlider.setTooltip("Master Output Trim Fader (-60 dB to +6 dB)");
    masterStrip.faderSlider.onValueChange = [this] {
        if (isUpdatingControls) return;
        const float db = static_cast<float>(masterStrip.faderSlider.getValue());
        mixerState.masterGainDb = db;
        masterStrip.dbReadoutLabel.setText(formatDbText(db), juce::dontSendNotification);
        audioEngine.setMasterMixerGainDb(db);
        audioProcessor.setMixerState(mixerState);
        if (onStateChanged) onStateChanged();
    };
    consoleBay.addAndMakeVisible(masterStrip.faderSlider);

    masterStrip.dbReadoutLabel.setText("0.0 dB", juce::dontSendNotification);
    masterStrip.dbReadoutLabel.setJustificationType(juce::Justification::centred);
    masterStrip.dbReadoutLabel.setFont(OpenWavLookAndFeel::getMonoFont(10.5f));
    masterStrip.dbReadoutLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    consoleBay.addAndMakeVisible(masterStrip.dbReadoutLabel);

    syncAllControlsFromState();
    refreshFromSampleMap();
    startTimerHz(30);
}

MultiOutMixerComponent::~MultiOutMixerComponent()
{
    stopTimer();
    instrumentList.setModel(nullptr);
    for (auto& st : busStrips)
        st.panKnob.setLookAndFeel(nullptr);
    masterStrip.panKnob.setLookAndFeel(nullptr);
}

juce::String MultiOutMixerComponent::formatDbText(float db)
{
    if (db <= -59.5f)
        return "-inf";
    return (db >= 0.05f ? "+" : "") + juce::String(db, 1) + " dB";
}

juce::String MultiOutMixerComponent::formatPanText(float pan)
{
    const int pct = juce::roundToInt(pan * 100.0f);
    if (std::abs(pct) <= 1)
        return "C";
    return (pct < 0) ? ("L" + juce::String(-pct)) : ("R" + juce::String(pct));
}

juce::String MultiOutMixerComponent::getBusDisplayLabel(int busIdx) const
{
    const int idx = juce::jlimit(0, AudioEngine::MaxOutputBuses - 1, busIdx);
    const juce::String hwName = (idx == 0) ? "Out 1-2" : AudioEngine::getDefaultOutputBusName(idx);
    const juce::String customName = mixerState.strips[static_cast<size_t>(idx)].name.trim();
    const juce::String defaultName = (idx == 0) ? "Main 1-2" : AudioEngine::getDefaultOutputBusName(idx);
    if (customName.isNotEmpty() && !customName.equalsIgnoreCase(hwName) && !customName.equalsIgnoreCase(defaultName))
        return hwName + " (" + customName + ")";
    return (idx == 0) ? "Out 1-2 (Main)" : hwName;
}

void MultiOutMixerComponent::rebuildInstrumentRows()
{
    if (getSampleMapStateCallback)
        cachedSampleMap = getSampleMapStateCallback();
    else
        cachedSampleMap = audioProcessor.getSampleMapState();

    if (getSamplerRackStateCallback)
        cachedSamplerRack = getSamplerRackStateCallback();
    else
        cachedSamplerRack = audioProcessor.getSamplerRackState();

    instrumentRows.clear();

    if (!cachedSamplerRack.slots.empty())
    {
        for (size_t i = 0; i < cachedSamplerRack.slots.size(); ++i)
        {
            const auto& sl = cachedSamplerRack.slots[i];
            RoutableInstrumentRow row;
            row.kind = RoutableInstrumentRow::Kind::RackSlot;
            row.rackSlotIndex = static_cast<int>(i);
            row.name = sl.name.isNotEmpty() ? sl.name : ("Sampler " + juce::String(i + 1));
            row.detailBadge = juce::String(sl.sampleMap.zones.size()) + (sl.sampleMap.zones.size() == 1 ? " zone" : " zones");
            row.midiChannel = juce::jlimit(0, 16, sl.midiChannel);
            row.outputBus = juce::jlimit(0, AudioEngine::MaxOutputBuses - 1, sl.outputBus);
            row.canAudition = !sl.sampleMap.zones.empty();
            row.hasMidiChannel = true;
            instrumentRows.push_back(std::move(row));
        }
    }
    else
    {
        RoutableInstrumentRow mapRow;
        mapRow.kind = RoutableInstrumentRow::Kind::MainSampleMap;
        mapRow.name = (cachedSampleMap.instrument.instrumentName.isNotEmpty()
                       && cachedSampleMap.instrument.instrumentName != "Custom Instrument")
                          ? cachedSampleMap.instrument.instrumentName
                          : juce::String("Sample Map Instrument");
        mapRow.detailBadge = juce::String(cachedSampleMap.zones.size()) + (cachedSampleMap.zones.size() == 1 ? " zone" : " zones");
        mapRow.midiChannel = juce::jlimit(0, 16, cachedSampleMap.midiChannel);
        mapRow.outputBus = juce::jlimit(0, AudioEngine::MaxOutputBuses - 1, cachedSampleMap.outputBus);
        mapRow.canAudition = !cachedSampleMap.zones.empty();
        mapRow.hasMidiChannel = true;
        instrumentRows.push_back(std::move(mapRow));
    }

    // Also expose Single / Unmapped Browser Sampler output track
    {
        RoutableInstrumentRow singleRow;
        singleRow.kind = RoutableInstrumentRow::Kind::SingleSampler;
        auto curFile = audioEngine.getCurrentFile();
        singleRow.name = curFile.existsAsFile()
            ? ("Single Sample (" + curFile.getFileName() + ")")
            : juce::String("Single / Unmapped Sampler");
        singleRow.detailBadge = "Keyboard";
        singleRow.outputBus = juce::jlimit(0, AudioEngine::MaxOutputBuses - 1, mixerState.defaultSamplerOutputBus);
        singleRow.canAudition = curFile.existsAsFile();
        singleRow.hasMidiChannel = false;
        instrumentRows.push_back(std::move(singleRow));
    }

    // Update each Mixer Strip's instrument assignment pill button text
    for (int b = 0; b < AudioEngine::MaxOutputBuses; ++b)
    {
        juce::StringArray assignedNames;
        for (const auto& r : instrumentRows)
        {
            if (r.kind == RoutableInstrumentRow::Kind::SingleSampler && instrumentRows.size() > 1 && b == 0 && r.outputBus == 0)
                continue; // Keep Main 1-2 pill clean unless explicitly only single sample
            if (r.outputBus == b)
                assignedNames.add(r.name);
        }

        auto& btn = busStrips[static_cast<size_t>(b)].assignBtn;
        if (assignedNames.isEmpty())
            btn.setButtonText("+ Route Inst");
        else if (assignedNames.size() == 1)
            btn.setButtonText(assignedNames[0]);
        else
            btn.setButtonText(juce::String(assignedNames.size()) + " Instruments");
    }
}

void MultiOutMixerComponent::syncAllControlsFromState()
{
    isUpdatingControls = true;

    if (mixerState.viewLayoutMode > 1)
        mixerState.viewLayoutMode = 0;

    modeConsoleBtn.setToggleState(mixerState.viewLayoutMode == 0, juce::dontSendNotification);
    modeRoutingBtn.setToggleState(mixerState.viewLayoutMode == 1, juce::dontSendNotification);
    foldToMainBtn.setToggleState(mixerState.foldInactiveBusesToMain, juce::dontSendNotification);

    for (int i = 0; i < AudioEngine::MaxOutputBuses; ++i)
    {
        auto& st = busStrips[static_cast<size_t>(i)];
        const auto& model = mixerState.strips[static_cast<size_t>(i)];
        st.nameEditor.setText(model.name, juce::dontSendNotification);
        st.panKnob.setValue(model.pan, juce::dontSendNotification);
        st.panValueLabel.setText(formatPanText(model.pan), juce::dontSendNotification);
        st.muteBtn.setToggleState(model.mute, juce::dontSendNotification);
        st.soloBtn.setToggleState(model.solo, juce::dontSendNotification);
        if (i == 0)
            st.fxBtn.setToggleState(model.sendToFxRack, juce::dontSendNotification);
        st.faderSlider.setValue(model.gainDb, juce::dontSendNotification);
        st.dbReadoutLabel.setText(formatDbText(model.gainDb), juce::dontSendNotification);
    }

    masterStrip.panKnob.setValue(mixerState.masterPan, juce::dontSendNotification);
    masterStrip.panValueLabel.setText(formatPanText(mixerState.masterPan), juce::dontSendNotification);
    masterStrip.muteBtn.setToggleState(mixerState.masterMute, juce::dontSendNotification);
    masterStrip.faderSlider.setValue(mixerState.masterGainDb, juce::dontSendNotification);
    masterStrip.dbReadoutLabel.setText(formatDbText(mixerState.masterGainDb), juce::dontSendNotification);

    isUpdatingControls = false;
}

MultiOutMixerState MultiOutMixerComponent::getState() const
{
    return mixerState;
}

void MultiOutMixerComponent::setState(const MultiOutMixerState& state)
{
    mixerState = state;
    if (mixerState.viewLayoutMode > 1)
        mixerState.viewLayoutMode = 0;
    audioProcessor.setMixerState(mixerState);
    syncAllControlsFromState();
    refreshFromSampleMap();
    resized();
    repaint();
}

void MultiOutMixerComponent::refreshFromSampleMap()
{
    rebuildInstrumentRows();

    const bool isPlugin = audioProcessor.isPluginWrapper();
    const int activeBuses = audioProcessor.getActiveOutputBusCount();
    if (isPlugin)
    {
        statusSubtitleLabel.setText(audioProcessor.getWrapperTypeDescription()
                                    + " • Active Host Buses: " + juce::String(activeBuses) + "/8",
                                    juce::dontSendNotification);
    }
    else
    {
        statusSubtitleLabel.setText("Standalone • 8 Stereo Buses Ready for VST3 / AU",
                                    juce::dontSendNotification);
    }

    instrumentList.updateContent();
    instrumentList.repaint();
    consoleBay.repaint();
    resized();
    repaint();
}

int MultiOutMixerComponent::getNumRows()
{
    return static_cast<int>(instrumentRows.size());
}

void MultiOutMixerComponent::paintListBoxItem(int, juce::Graphics&, int, int, bool)
{
}

juce::Component* MultiOutMixerComponent::refreshComponentForRow(int rowNumber, bool, juce::Component* existingComponentToUpdate)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(instrumentRows.size()))
    {
        delete existingComponentToUpdate;
        return nullptr;
    }

    auto* rowComp = dynamic_cast<InstrumentRoutingRowComponent*>(existingComponentToUpdate);
    if (rowComp == nullptr)
    {
        delete existingComponentToUpdate;
        rowComp = new InstrumentRoutingRowComponent(*this);
    }

    rowComp->updateFromRow(rowNumber, instrumentRows[static_cast<size_t>(rowNumber)]);
    return rowComp;
}

void MultiOutMixerComponent::auditionInstrumentRow(int rowIndex)
{
    if (rowIndex < 0 || rowIndex >= static_cast<int>(instrumentRows.size()))
        return;

    const auto& r = instrumentRows[static_cast<size_t>(rowIndex)];
    if (r.kind == RoutableInstrumentRow::Kind::RackSlot && r.rackSlotIndex >= 0
        && r.rackSlotIndex < static_cast<int>(cachedSamplerRack.slots.size()))
    {
        const auto& sl = cachedSamplerRack.slots[static_cast<size_t>(r.rackSlotIndex)];
        if (!sl.sampleMap.zones.empty())
        {
            const auto& z0 = sl.sampleMap.zones.front();
            const int note = juce::jlimit(0, 127, (z0.rootNote >= z0.keyLow && z0.rootNote <= z0.keyHigh) ? z0.rootNote : z0.keyLow);
            audioProcessor.triggerRackSlotNoteOn(r.rackSlotIndex, note - sl.transposeSemitones, 0.85f);
            juce::Timer::callAfterDelay(260, [proc = &audioProcessor, idx = r.rackSlotIndex, n = note - sl.transposeSemitones] {
                proc->triggerRackSlotNoteOff(idx, n);
            });
        }
    }
    else if (r.kind == RoutableInstrumentRow::Kind::MainSampleMap)
    {
        if (!cachedSampleMap.zones.empty())
        {
            const auto& z0 = cachedSampleMap.zones.front();
            juce::File f(z0.filePath);
            if (f.existsAsFile())
            {
                const int note = juce::jlimit(0, 127, (z0.rootNote >= z0.keyLow && z0.rootNote <= z0.keyHigh) ? z0.rootNote : z0.keyLow);
                audioEngine.playZoneVoice(f, note, z0.rootNote, z0.fineTuneCents, z0.gainDb, 0.85f,
                                          z0.attackMs / 1000.0f, z0.decayMs / 1000.0f, z0.sustainLevel, z0.releaseMs / 1000.0f,
                                          true, false, true, cachedSampleMap.outputBus);
            }
        }
    }
    else if (r.kind == RoutableInstrumentRow::Kind::SingleSampler)
    {
        audioEngine.triggerNoteOn(60, 0.85f);
        juce::Timer::callAfterDelay(260, [eng = &audioEngine] {
            eng->triggerNoteOff(60);
        });
    }
}

void MultiOutMixerComponent::setInstrumentMidiChannel(int rowIndex, int newMidiCh)
{
    if (rowIndex < 0 || rowIndex >= static_cast<int>(instrumentRows.size()))
        return;

    const auto kind = instrumentRows[static_cast<size_t>(rowIndex)].kind;
    const int slotIdx = instrumentRows[static_cast<size_t>(rowIndex)].rackSlotIndex;

    if (kind == RoutableInstrumentRow::Kind::RackSlot && slotIdx >= 0 && slotIdx < static_cast<int>(cachedSamplerRack.slots.size()))
    {
        cachedSamplerRack.slots[static_cast<size_t>(slotIdx)].midiChannel = juce::jlimit(0, 16, newMidiCh);
        cachedSamplerRack.slots[static_cast<size_t>(slotIdx)].sampleMap.midiChannel = juce::jlimit(0, 16, newMidiCh);
        if (onSamplerRackModified)
            onSamplerRackModified(cachedSamplerRack);
        else
            audioProcessor.setSamplerRackState(cachedSamplerRack, false);
    }
    else if (kind == RoutableInstrumentRow::Kind::MainSampleMap)
    {
        cachedSampleMap.midiChannel = juce::jlimit(0, 16, newMidiCh);
        if (onSampleMapModified)
            onSampleMapModified(cachedSampleMap);
        else
            audioProcessor.setSampleMapState(cachedSampleMap);
    }

    if (onStateChanged) onStateChanged();
    rebuildInstrumentRows();
    instrumentList.repaintRow(rowIndex);
}

void MultiOutMixerComponent::setInstrumentOutputBus(int rowIndex, int newBusIdx)
{
    if (rowIndex < 0 || rowIndex >= static_cast<int>(instrumentRows.size()))
        return;

    const int clampedBus = juce::jlimit(0, AudioEngine::MaxOutputBuses - 1, newBusIdx);
    const auto kind = instrumentRows[static_cast<size_t>(rowIndex)].kind;
    const int slotIdx = instrumentRows[static_cast<size_t>(rowIndex)].rackSlotIndex;

    if (kind == RoutableInstrumentRow::Kind::RackSlot && slotIdx >= 0 && slotIdx < static_cast<int>(cachedSamplerRack.slots.size()))
    {
        cachedSamplerRack.slots[static_cast<size_t>(slotIdx)].outputBus = clampedBus;
        cachedSamplerRack.slots[static_cast<size_t>(slotIdx)].sampleMap.outputBus = clampedBus;
        if (onSamplerRackModified)
            onSamplerRackModified(cachedSamplerRack);
        else
            audioProcessor.setSamplerRackState(cachedSamplerRack, false);
    }
    else if (kind == RoutableInstrumentRow::Kind::MainSampleMap)
    {
        cachedSampleMap.outputBus = clampedBus;
        if (onSampleMapModified)
            onSampleMapModified(cachedSampleMap);
        else
            audioProcessor.setSampleMapState(cachedSampleMap);
    }
    else if (kind == RoutableInstrumentRow::Kind::SingleSampler)
    {
        mixerState.defaultSamplerOutputBus = clampedBus;
        audioEngine.setDefaultSamplerOutputBus(clampedBus);
        audioProcessor.setMixerState(mixerState);
    }

    if (onStateChanged) onStateChanged();
    rebuildInstrumentRows();
    instrumentList.updateContent();
    instrumentList.repaint();
    consoleBay.repaint();
}

void MultiOutMixerComponent::showAssignMenuForBus(int busIdx, juce::Component& targetBtn)
{
    rebuildInstrumentRows();

    juce::PopupMenu m;
    m.addSectionHeader("Route Entire Instrument to " + getBusDisplayLabel(busIdx));
    for (size_t i = 0; i < instrumentRows.size(); ++i)
    {
        const auto& r = instrumentRows[i];
        juce::String label = r.name + "  [" + AudioEngine::getDefaultOutputBusName(r.outputBus) + "]";
        m.addItem(static_cast<int>(i) + 1, label, true, r.outputBus == busIdx);
    }
    m.addSeparator();
    m.addItem(100, "Open Instrument Outputs Table...");
    m.addItem(101, "Open Multi-SampleMap Sampler Rack...");

    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&targetBtn),
                    [this, busIdx](int res) {
        if (res >= 1 && res <= static_cast<int>(instrumentRows.size()))
        {
            setInstrumentOutputBus(res - 1, busIdx);
        }
        else if (res == 100)
        {
            mixerState.viewLayoutMode = 1;
            syncAllControlsFromState();
            resized();
            repaint();
        }
        else if (res == 101 && onOpenSamplerRackRequested)
        {
            onOpenSamplerRackRequested();
        }
    });
}

void MultiOutMixerComponent::autoRouteInstrumentsAcrossBuses()
{
    rebuildInstrumentRows();

    if (!cachedSamplerRack.slots.empty())
    {
        for (size_t i = 0; i < cachedSamplerRack.slots.size(); ++i)
        {
            const int bus = static_cast<int>(i) % AudioEngine::MaxOutputBuses;
            cachedSamplerRack.slots[i].outputBus = bus;
            cachedSamplerRack.slots[i].sampleMap.outputBus = bus;
        }
        if (onSamplerRackModified)
            onSamplerRackModified(cachedSamplerRack);
        else
            audioProcessor.setSamplerRackState(cachedSamplerRack, false);
    }
    else
    {
        cachedSampleMap.outputBus = 0;
        if (onSampleMapModified)
            onSampleMapModified(cachedSampleMap);
    }

    if (onStateChanged) onStateChanged();
    refreshFromSampleMap();
}

void MultiOutMixerComponent::resetAllInstrumentsToMain()
{
    rebuildInstrumentRows();

    if (!cachedSamplerRack.slots.empty())
    {
        for (auto& sl : cachedSamplerRack.slots)
        {
            sl.outputBus = 0;
            sl.sampleMap.outputBus = 0;
        }
        if (onSamplerRackModified)
            onSamplerRackModified(cachedSamplerRack);
        else
            audioProcessor.setSamplerRackState(cachedSamplerRack, false);
    }

    cachedSampleMap.outputBus = 0;
    if (onSampleMapModified)
        onSampleMapModified(cachedSampleMap);
    else
        audioProcessor.setSampleMapState(cachedSampleMap);

    mixerState.defaultSamplerOutputBus = 0;
    audioEngine.setDefaultSamplerOutputBus(0);
    audioProcessor.setMixerState(mixerState);

    if (onStateChanged) onStateChanged();
    refreshFromSampleMap();
}

void MultiOutMixerComponent::resetMixerToUnity()
{
    for (int i = 0; i < AudioEngine::MaxOutputBuses; ++i)
    {
        auto& st = mixerState.strips[static_cast<size_t>(i)];
        st.gainDb = 0.0f;
        st.pan = 0.0f;
        st.mute = false;
        st.solo = false;
        busStrips[static_cast<size_t>(i)].clipL = false;
        busStrips[static_cast<size_t>(i)].clipR = false;
    }
    mixerState.masterGainDb = 0.0f;
    mixerState.masterPan = 0.0f;
    mixerState.masterMute = false;
    masterStrip.clipL = false;
    masterStrip.clipR = false;

    audioProcessor.setMixerState(mixerState);
    syncAllControlsFromState();
    if (onStateChanged) onStateChanged();
    consoleBay.repaint();
}

void MultiOutMixerComponent::timerCallback()
{
    if (!isShowing() || mixerState.viewLayoutMode != 0)
        return;

    bool needsMeterRepaint = false;
    constexpr float kDecay = 0.82f;

    for (int i = 0; i < AudioEngine::MaxOutputBuses; ++i)
    {
        float pkL = 0.0f, pkR = 0.0f;
        audioEngine.consumeBusPeaks(i, pkL, pkR);
        auto& st = busStrips[static_cast<size_t>(i)];

        const float nextL = std::max(pkL, st.meterL * kDecay);
        const float nextR = std::max(pkR, st.meterR * kDecay);
        if (pkL >= 1.0f) st.clipL = true;
        if (pkR >= 1.0f) st.clipR = true;

        if (std::abs(nextL - st.meterL) > 0.002f || std::abs(nextR - st.meterR) > 0.002f)
        {
            st.meterL = (nextL < 0.001f) ? 0.0f : nextL;
            st.meterR = (nextR < 0.001f) ? 0.0f : nextR;
            needsMeterRepaint = true;
        }
    }

    {
        float mL = 0.0f, mR = 0.0f;
        audioEngine.consumeMasterPeaks(mL, mR);
        const float nextL = std::max(mL, masterStrip.meterL * kDecay);
        const float nextR = std::max(mR, masterStrip.meterR * kDecay);
        if (mL >= 1.0f) masterStrip.clipL = true;
        if (mR >= 1.0f) masterStrip.clipR = true;

        if (std::abs(nextL - masterStrip.meterL) > 0.002f || std::abs(nextR - masterStrip.meterR) > 0.002f)
        {
            masterStrip.meterL = (nextL < 0.001f) ? 0.0f : nextL;
            masterStrip.meterR = (nextR < 0.001f) ? 0.0f : nextR;
            needsMeterRepaint = true;
        }
    }

    if (needsMeterRepaint)
        consoleBay.repaint();
}

void MultiOutMixerComponent::paint(juce::Graphics& g)
{
    g.fillAll(OpenWavLookAndFeel::bgDark);

    // Top Toolbar Divider (identical to PerformanceComponent & SamplerRackComponent)
    g.setColour(OpenWavLookAndFeel::borderColour);
    g.drawHorizontalLine(52, 0.0f, static_cast<float>(getWidth()));

    OpenWavLookAndFeel::drawButtonGroup(g, { &modeConsoleBtn, &modeRoutingBtn });
    if (autoRouteBtn.isVisible())
        OpenWavLookAndFeel::drawButtonGroup(g, { &autoRouteBtn, &resetRoutingBtn, &resetMixerBtn });

    if (mixerState.viewLayoutMode == 1 && !routingCardBounds.isEmpty())
    {
        OpenWavLookAndFeel::drawPanelCard(g, routingCardBounds.toFloat(), 8.0f, false);

        auto hdr = routingCardBounds.withHeight(38).reduced(16, 0);
        g.setColour(OpenWavLookAndFeel::accentCyan);
        g.setFont(OpenWavLookAndFeel::getTitleFont(12.5f));
        g.drawText("INSTRUMENT -> DAW OUTPUT TRACK ROUTING", hdr, juce::Justification::centredLeft, true);

        g.setColour(OpenWavLookAndFeel::textSecondary);
        g.setFont(OpenWavLookAndFeel::getBodyFont(11.5f, false));
        g.drawText("Each instrument routes as a complete stereo stem to Out 1-2 through Out 15-16",
                   hdr, juce::Justification::centredRight, true);

        g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.6f));
        g.drawHorizontalLine(routingCardBounds.getY() + 38,
                             static_cast<float>(routingCardBounds.getX() + 12),
                             static_cast<float>(routingCardBounds.getRight() - 12));
    }
}

void MultiOutMixerComponent::resized()
{
    auto area = getLocalBounds();
    if (area.getWidth() < 80 || area.getHeight() < 80)
        return;

    // 1. Clean Non-Overlapping 52px Top Toolbar
    auto topBar = area.removeFromTop(52).reduced(12, 10);
    const int btnH = 30;

    // Right-side utility buttons (hide secondary buttons gracefully if window is very compact)
    if (topBar.getWidth() >= 760)
    {
        openRackBtn.setVisible(true);
        openRackBtn.setBounds(topBar.removeFromRight(98).withHeight(btnH));
        topBar.removeFromRight(6);
    }
    else
    {
        openRackBtn.setVisible(false);
    }

    if (topBar.getWidth() >= 580)
    {
        auto actionGroup = topBar.removeFromRight(246);
        const int aw = (actionGroup.getWidth() - 8) / 3;
        autoRouteBtn.setVisible(true);
        resetRoutingBtn.setVisible(true);
        resetMixerBtn.setVisible(true);
        autoRouteBtn.setBounds(actionGroup.removeFromLeft(aw).withHeight(btnH));
        actionGroup.removeFromLeft(4);
        resetRoutingBtn.setBounds(actionGroup.removeFromLeft(aw).withHeight(btnH));
        actionGroup.removeFromLeft(4);
        resetMixerBtn.setBounds(actionGroup.withHeight(btnH));
        topBar.removeFromRight(8);
    }
    else
    {
        autoRouteBtn.setVisible(false);
        resetRoutingBtn.setVisible(false);
        resetMixerBtn.setVisible(false);
    }

    if (topBar.getWidth() >= 410)
    {
        foldToMainBtn.setVisible(true);
        foldToMainBtn.setBounds(topBar.removeFromRight(102).withHeight(btnH));
        topBar.removeFromRight(8);
    }
    else
    {
        foldToMainBtn.setVisible(false);
    }

    // Left Title & Subtitle (clamped to remaining space before Mode Switcher)
    const int modeSwitcherW = 248;
    const int titleW = juce::jlimit(120, 235, topBar.getWidth() - modeSwitcherW - 10);
    auto titleArea = topBar.removeFromLeft(titleW);
    titleLabel.setBounds(titleArea.removeFromTop(18));
    statusSubtitleLabel.setBounds(titleArea);
    topBar.removeFromLeft(8);

    auto switcherArea = topBar.removeFromLeft(std::min(modeSwitcherW, topBar.getWidth()));
    const int mw = (switcherArea.getWidth() - 4) / 2;
    modeConsoleBtn.setBounds(switcherArea.removeFromLeft(mw).withHeight(btnH));
    switcherArea.removeFromLeft(4);
    modeRoutingBtn.setBounds(switcherArea.withHeight(btnH));

    // 2. Main Content Area
    const bool showConsole = (mixerState.viewLayoutMode == 0);
    consoleViewport.setVisible(showConsole);
    instrumentList.setVisible(!showConsole);

    if (showConsole)
    {
        routingCardBounds = {};
        consoleViewport.setBounds(area.reduced(8, 8));
        consoleBay.resized();
    }
    else
    {
        routingCardBounds = area.reduced(12, 10);
        instrumentList.setBounds(routingCardBounds.withTrimmedTop(44).reduced(10, 6));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  ConsoleBayComponent (Spacious 8 Stereo Output Strips + Master Strip)
// ─────────────────────────────────────────────────────────────────────────────
void MultiOutMixerComponent::ConsoleBayComponent::mouseDown(const juce::MouseEvent& e)
{
    for (auto& st : owner.busStrips)
    {
        if (st.meterBounds.contains(e.getPosition()))
        {
            st.clipL = false;
            st.clipR = false;
            repaint();
            return;
        }
    }
    if (owner.masterStrip.meterBounds.contains(e.getPosition()))
    {
        owner.masterStrip.clipL = false;
        owner.masterStrip.clipR = false;
        repaint();
    }
}

void MultiOutMixerComponent::ConsoleBayComponent::resized()
{
    constexpr int numStrips = AudioEngine::MaxOutputBuses + 1;
    constexpr int minStripW = 104;
    constexpr int stripGap = 8;
    constexpr int masterDividerGap = 10;
    constexpr int padX = 6;
    constexpr int padY = 4;

    const int vpW = owner.consoleViewport.getWidth();
    const int vpH = owner.consoleViewport.getHeight();
    if (vpW <= 0 || vpH <= 0)
        return;

    const int minRequiredW = padX * 2 + numStrips * minStripW + (numStrips - 1) * stripGap + masterDividerGap;
    const int targetW = std::max(vpW, minRequiredW);
    const int targetH = std::max(300, vpH - (targetW > vpW ? 12 : 0));

    if (getWidth() != targetW || getHeight() != targetH)
    {
        setSize(targetW, targetH);
        return;
    }

    const int availW = targetW - padX * 2 - (numStrips - 1) * stripGap - masterDividerGap;
    const int stripW = std::max(minStripW, availW / numStrips);
    const int stripH = targetH - padY * 2;

    auto layoutStrip = [](ChannelStripWidgets& st, juce::Rectangle<int> bounds, bool hasSolo, bool hasFxToggle) {
        st.cardBounds = bounds;
        auto inner = bounds.reduced(8, 8);

        inner.removeFromTop(34); // Bus title ("OUT 1-2") + status pill ("DAW ACTIVE")

        st.nameEditor.setBounds(inner.removeFromTop(22));
        inner.removeFromTop(5);

        st.assignBtn.setBounds(inner.removeFromTop(22));
        inner.removeFromTop(6);

        // Rotary Pan Knob + Readout
        const int knobH = std::min(42, std::max(32, inner.getHeight() / 6));
        st.panKnob.setBounds(inner.removeFromTop(knobH).withSizeKeepingCentre(knobH, knobH));
        st.panValueLabel.setBounds(inner.removeFromTop(14));
        inner.removeFromTop(5);

        // Mute / Solo (/ FX) Row - Spacious & Unobstructed
        auto btnRow = inner.removeFromTop(24);
        if (hasSolo && hasFxToggle)
        {
            const int bw = (btnRow.getWidth() - 8) / 3;
            st.muteBtn.setBounds(btnRow.removeFromLeft(bw));
            btnRow.removeFromLeft(4);
            st.soloBtn.setBounds(btnRow.removeFromLeft(bw));
            btnRow.removeFromLeft(4);
            st.fxBtn.setBounds(btnRow);
            st.fxBtn.setVisible(true);
        }
        else if (hasSolo)
        {
            const int bw = (btnRow.getWidth() - 6) / 2;
            st.muteBtn.setBounds(btnRow.removeFromLeft(bw));
            btnRow.removeFromLeft(6);
            st.soloBtn.setBounds(btnRow);
            st.fxBtn.setVisible(false);
        }
        else
        {
            st.muteBtn.setBounds(btnRow);
            st.soloBtn.setVisible(false);
            st.fxBtn.setVisible(false);
        }

        inner.removeFromTop(8);
        st.dbReadoutLabel.setBounds(inner.removeFromBottom(20));
        inner.removeFromBottom(4);

        // Fader + dB Scale + Stereo L/R Peak Meter
        const int meterW = 18;
        const int scaleW = 20;
        st.meterBounds = inner.removeFromRight(meterW);
        st.scaleBounds = inner.removeFromRight(scaleW);
        st.faderSlider.setBounds(inner);
    };

    int x = padX;
    for (int i = 0; i < AudioEngine::MaxOutputBuses; ++i)
    {
        layoutStrip(owner.busStrips[static_cast<size_t>(i)],
                    juce::Rectangle<int>(x, padY, stripW, stripH),
                    true,
                    i == 0);
        x += stripW + stripGap;
    }

    x += masterDividerGap;
    const int masterW = std::max(stripW, targetW - padX - x);
    layoutStrip(owner.masterStrip,
                juce::Rectangle<int>(x, padY, masterW, stripH),
                false,
                false);
}

void MultiOutMixerComponent::ConsoleBayComponent::paint(juce::Graphics& g)
{
    g.fillAll(OpenWavLookAndFeel::bgDark);

    auto drawScaleAndMeters = [&](const ChannelStripWidgets& st, bool muted) {
        if (!st.scaleBounds.isEmpty())
        {
            static const float kTickDbs[] = { 6.0f, 0.0f, -6.0f, -12.0f, -24.0f, -48.0f };
            g.setFont(OpenWavLookAndFeel::getMonoFont(8.5f));
            const auto range = st.faderSlider.getNormalisableRange();
            for (float db : kTickDbs)
            {
                const double prop = range.convertTo0to1(static_cast<double>(db));
                const float y = static_cast<float>(st.scaleBounds.getBottom())
                              - static_cast<float>(prop) * static_cast<float>(st.scaleBounds.getHeight());
                g.setColour(db == 0.0f ? OpenWavLookAndFeel::accentCyan.withAlpha(0.75f)
                                       : OpenWavLookAndFeel::borderColour.withAlpha(0.7f));
                g.drawHorizontalLine(juce::roundToInt(y),
                                     static_cast<float>(st.scaleBounds.getX() + 1),
                                     static_cast<float>(st.scaleBounds.getX() + 4));
                g.setColour(db == 0.0f ? OpenWavLookAndFeel::textPrimary.withAlpha(0.85f)
                                       : OpenWavLookAndFeel::textSecondary.withAlpha(0.65f));
                juce::String lbl = (db > 0.0f) ? "+6" : juce::String(static_cast<int>(db));
                g.drawText(lbl,
                           juce::Rectangle<int>(st.scaleBounds.getX() + 4, juce::roundToInt(y) - 6, st.scaleBounds.getWidth() - 4, 12),
                           juce::Justification::centredLeft, false);
            }
        }

        if (st.meterBounds.isEmpty())
            return;

        auto rf = st.meterBounds.toFloat();
        g.setColour(juce::Colour(0xFF0A0D12));
        g.fillRoundedRectangle(rf, 3.0f);
        g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.6f));
        g.drawRoundedRectangle(rf, 3.0f, 1.0f);

        auto inner = rf.reduced(2.5f, 2.5f);
        const float barW = std::max(3.0f, (inner.getWidth() - 2.0f) * 0.5f);
        auto leftTrack  = juce::Rectangle<float>(inner.getX(), inner.getY() + 5.0f, barW, inner.getHeight() - 5.0f);
        auto rightTrack = juce::Rectangle<float>(inner.getRight() - barW, inner.getY() + 5.0f, barW, inner.getHeight() - 5.0f);

        auto levelToNorm = [](float lin) -> float {
            if (lin <= 0.0005f) return 0.0f;
            const float db = juce::Decibels::gainToDecibels(lin, -60.0f);
            return juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 66.0f);
        };

        auto drawBar = [&](juce::Rectangle<float> track, float lin, bool clipped) {
            g.setColour(clipped ? OpenWavLookAndFeel::favoriteRed : juce::Colour(0xFF1E2430));
            g.fillRoundedRectangle(track.getX(), inner.getY(), track.getWidth(), 3.5f, 1.0f);

            const float norm = levelToNorm(lin);
            if (norm <= 0.002f) return;
            const float fillH = track.getHeight() * norm;
            auto fillRect = juce::Rectangle<float>(track.getX(), track.getBottom() - fillH, track.getWidth(), fillH);

            if (muted)
            {
                g.setColour(OpenWavLookAndFeel::textSecondary.withAlpha(0.42f));
                g.fillRoundedRectangle(fillRect, 1.5f);
            }
            else
            {
                juce::ColourGradient grad(OpenWavLookAndFeel::favoriteRed, fillRect.getX(), track.getY(),
                                          juce::Colour(0xFF10B981), fillRect.getX(), track.getBottom(), false);
                grad.addColour(0.20, juce::Colour(0xFFF59E0B));
                grad.addColour(0.55, OpenWavLookAndFeel::accentCyan);
                g.setGradientFill(grad);
                g.fillRoundedRectangle(fillRect, 1.5f);
            }
        };

        drawBar(leftTrack, st.meterL, st.clipL);
        drawBar(rightTrack, st.meterR, st.clipR);
    };

    bool anySolo = false;
    for (const auto& s : owner.mixerState.strips)
        if (s.solo) anySolo = true;

    for (int i = 0; i < AudioEngine::MaxOutputBuses; ++i)
    {
        const auto& st = owner.busStrips[static_cast<size_t>(i)];
        if (st.cardBounds.isEmpty()) continue;

        const auto& model = owner.mixerState.strips[static_cast<size_t>(i)];
        const bool hostActive = owner.audioEngine.isHostBusActive(i);
        const bool isEffectiveMuted = owner.mixerState.masterMute || model.mute || (anySolo && !model.solo);

        // Hardware Chassis Card Gradient (matches PerformanceComponent / SamplerRackComponent)
        auto cf = st.cardBounds.toFloat();
        juce::ColourGradient cardGrad(juce::Colour(32, 36, 42), 0.0f, cf.getY(),
                                      juce::Colour(20, 22, 26), 0.0f, cf.getBottom(), false);
        g.setGradientFill(cardGrad);
        g.fillRoundedRectangle(cf, 7.0f);

        // Top Accent Line
        juce::Colour topAccent = (i == 0) ? OpenWavLookAndFeel::accentCyan
                                          : (hostActive ? juce::Colour(0xFF10B981) : OpenWavLookAndFeel::accentCyan.withAlpha(0.55f));
        g.setColour(isEffectiveMuted ? topAccent.withAlpha(0.28f) : topAccent);
        g.fillRoundedRectangle(cf.getX() + 6.0f, cf.getY(), cf.getWidth() - 12.0f, 2.5f, 1.2f);

        g.setColour(model.solo ? juce::Colour(0xFFD97706) : juce::Colour(50, 56, 66));
        g.drawRoundedRectangle(cf.reduced(0.5f), 7.0f, model.solo ? 1.4f : 1.0f);

        // Bus Title ("OUT 1-2", "OUT 3-4"...)
        const juce::String busTitle = (i == 0) ? "OUT 1-2" : AudioEngine::getDefaultOutputBusName(i).toUpperCase();
        g.setColour(OpenWavLookAndFeel::textPrimary);
        g.setFont(OpenWavLookAndFeel::getTitleFont(11.0f));
        g.drawText(busTitle, st.cardBounds.withHeight(16).translated(0, 7), juce::Justification::centred, false);

        // Status Pill ("MAIN BUS", "DAW OUT", "-> OUT 1-2", "INACTIVE")
        juce::String statusTxt;
        juce::Colour statusCol;
        if (i == 0)
        {
            statusTxt = "MAIN BUS";
            statusCol = OpenWavLookAndFeel::accentCyan;
        }
        else if (hostActive)
        {
            statusTxt = "DAW ACTIVE";
            statusCol = juce::Colour(0xFF34D399);
        }
        else if (owner.mixerState.foldInactiveBusesToMain)
        {
            statusTxt = "-> OUT 1-2";
            statusCol = OpenWavLookAndFeel::accentCyan.withAlpha(0.85f);
        }
        else
        {
            statusTxt = "HOST OFF";
            statusCol = OpenWavLookAndFeel::textSecondary.withAlpha(0.55f);
        }

        auto pillRect = juce::Rectangle<float>(cf.getX() + 10.0f, cf.getY() + 23.0f, cf.getWidth() - 20.0f, 14.0f);
        g.setColour(statusCol.withAlpha(0.13f));
        g.fillRoundedRectangle(pillRect, 3.5f);
        g.setColour(statusCol);
        g.setFont(OpenWavLookAndFeel::getMonoFont(8.8f));
        g.drawText(statusTxt, pillRect, juce::Justification::centred, false);

        // Readout pill background behind dB label
        g.setColour(OpenWavLookAndFeel::bgDark.withAlpha(0.75f));
        g.fillRoundedRectangle(st.dbReadoutLabel.getBounds().toFloat(), 4.0f);

        drawScaleAndMeters(st, isEffectiveMuted);
    }

    // Master Strip Divider Line
    if (!owner.masterStrip.cardBounds.isEmpty())
    {
        const float divX = static_cast<float>(owner.masterStrip.cardBounds.getX() - 5);
        g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.8f));
        g.drawVerticalLine(juce::roundToInt(divX), 10.0f, static_cast<float>(getHeight() - 10));

        auto mf = owner.masterStrip.cardBounds.toFloat();
        juce::ColourGradient mGrad(juce::Colour(36, 41, 49), 0.0f, mf.getY(),
                                   juce::Colour(22, 25, 30), 0.0f, mf.getBottom(), false);
        g.setGradientFill(mGrad);
        g.fillRoundedRectangle(mf, 7.0f);

        g.setColour(juce::Colour(0xFF10B981));
        g.fillRoundedRectangle(mf.getX() + 6.0f, mf.getY(), mf.getWidth() - 12.0f, 2.5f, 1.2f);
        g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.65f));
        g.drawRoundedRectangle(mf.reduced(0.5f), 7.0f, 1.25f);

        g.setColour(OpenWavLookAndFeel::accentCyan);
        g.setFont(OpenWavLookAndFeel::getTitleFont(11.0f));
        g.drawText("MASTER", owner.masterStrip.cardBounds.withHeight(16).translated(0, 7), juce::Justification::centred, false);

        auto pillRect = juce::Rectangle<float>(mf.getX() + 10.0f, mf.getY() + 23.0f, mf.getWidth() - 20.0f, 14.0f);
        g.setColour(juce::Colour(0x2210B981));
        g.fillRoundedRectangle(pillRect, 3.5f);
        g.setColour(juce::Colour(0xFF34D399));
        g.setFont(OpenWavLookAndFeel::getMonoFont(8.8f));
        g.drawText("GLOBAL OUT", pillRect, juce::Justification::centred, false);

        g.setColour(OpenWavLookAndFeel::bgDark.withAlpha(0.75f));
        g.fillRoundedRectangle(owner.masterStrip.dbReadoutLabel.getBounds().toFloat(), 4.0f);

        drawScaleAndMeters(owner.masterStrip, owner.mixerState.masterMute);
    }
}

} // namespace openwav
