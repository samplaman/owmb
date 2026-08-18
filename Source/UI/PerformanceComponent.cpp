#include "PerformanceComponent.h"
#include "SampleMapComponent.h"
#include "OpenWavLookAndFeel.h"
#include <random>

namespace openwav
{

namespace
{
    const std::array<juce::Colour, 8> apcPadColors = {
        juce::Colour(0xff00c8dc), // Cyan
        juce::Colour(0xff00e676), // Emerald
        juce::Colour(0xffffab00), // Amber
        juce::Colour(0xffa040ff), // Purple
        juce::Colour(0xff008cff), // Electric Blue
        juce::Colour(0xffff3d00), // Crimson
        juce::Colour(0xffff4081), // Pink
        juce::Colour(0xffeeff41)  // Lime
    };

    juce::String padIndexToName(int row, int col)
    {
        char rowLetter = static_cast<char>('A' + row);
        return juce::String::charToString(rowLetter) + juce::String(col + 1);
    }
}

PerformanceComponent::PerformanceComponent(TagDatabaseManager& db, AudioEngine& engine)
    : dbManager(db), audioEngine(engine)
{
    audioEngine.addListener(this);
    audioEngine.getKeyboardState().addListener(this);

    // Initialize Pad Grid data
    for (int r = 0; r < numRows; ++r)
    {
        for (int c = 0; c < numCols; ++c)
        {
            int idx = r * numCols + c;
            pads[idx].id = idx;
            pads[idx].row = r;
            pads[idx].col = c;
            pads[idx].color = apcPadColors[(r + c) % apcPadColors.size()];
        }
    }

    // Preset Browser Setup
    addAndMakeVisible(newPresetButton);
    newPresetButton.addListener(this);

    addAndMakeVisible(savePresetButton);
    savePresetButton.addListener(this);

    addAndMakeVisible(loadPresetButton);
    loadPresetButton.addListener(this);

    addAndMakeVisible(presetSelector);
    refreshPresetList();
    presetSelector.onChange = [this] {
        int id = presetSelector.getSelectedId();
        if (id == 1)
        {
            clearAllPads();
        }
        else if (id > 1)
        {
            auto dir = getPresetsDirectory();
            auto files = dir.findChildFiles(juce::File::findFiles, false, "*.owperf");
            int idx = id - 2;
            if (idx >= 0 && idx < files.size())
            {
                loadPerformancePreset(files[idx]);
            }
        }
    };

    // Controls
    addAndMakeVisible(fillButton);
    fillButton.addListener(this);

    addAndMakeVisible(clearButton);
    clearButton.addListener(this);

    addAndMakeVisible(randomButton);
    randomButton.addListener(this);

    addAndMakeVisible(stopAllButton);
    stopAllButton.addListener(this);

    pitchTrackButton.setClickingTogglesState(true);
    bool ptEnabled = audioEngine.isPitchTrackingEnabled();
    pitchTrackButton.setToggleState(ptEnabled, juce::dontSendNotification);
    pitchTrackButton.setButtonText(ptEnabled ? "Pitch Track: ON" : "Pitch Track: OFF");
    pitchTrackButton.onClick = [this] {
        bool enabled = pitchTrackButton.getToggleState();
        audioEngine.setPitchTrackingEnabled(enabled);
        pitchTrackButton.setButtonText(enabled ? "Pitch Track: ON" : "Pitch Track: OFF");
    };
    addAndMakeVisible(pitchTrackButton);

    oneShotButton.setClickingTogglesState(true);
    bool osEnabled = audioEngine.isOneShotEnabled();
    oneShotButton.setToggleState(osEnabled, juce::dontSendNotification);
    oneShotButton.setButtonText(osEnabled ? "One Shot: ON" : "One Shot: OFF");
    oneShotButton.onClick = [this] {
        bool enabled = oneShotButton.getToggleState();
        audioEngine.setOneShotEnabled(enabled);
        oneShotButton.setButtonText(enabled ? "One Shot: ON" : "One Shot: OFF");
    };
    addAndMakeVisible(oneShotButton);

    // Global Performance ADSR Setup (Linear Horizontal Sliders)
    auto setupAdsrSlider = [this](juce::Slider& knob, juce::Label& label, const juce::String& text, double minVal, double maxVal, double defaultVal, const juce::String& suffix) {
        label.setText(text, juce::dontSendNotification);
        label.setFont(juce::Font(11.5f).boldened());
        label.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
        label.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(label);

        knob.setSliderStyle(juce::Slider::LinearHorizontal);
        knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        knob.setRange(minVal, maxVal, 0.001);
        knob.setValue(defaultVal, juce::dontSendNotification);
        knob.setNumDecimalPlacesToDisplay(2);
        knob.setTooltip(suffix);
        addAndMakeVisible(knob);
    };

    setupAdsrSlider(attackKnob, attackLabel, "ATT", 0.001, 2.0, 0.005, "Attack");
    setupAdsrSlider(decayKnob, decayLabel, "DEC", 0.01, 5.0, 0.1, "Decay");
    setupAdsrSlider(sustainKnob, sustainLabel, "SUS", 0.0, 1.0, 1.0, "Sustain");
    setupAdsrSlider(releaseKnob, releaseLabel, "REL", 0.01, 5.0, 0.15, "Release");

    loopButton.setClickingTogglesState(true);
    bool loopEnabled = audioEngine.isLooping();
    loopButton.setToggleState(loopEnabled, juce::dontSendNotification);
    loopButton.setButtonText(loopEnabled ? "Loop: ON" : "Loop: OFF");
    loopButton.onClick = [this] {
        bool enabled = loopButton.getToggleState();
        audioEngine.setLooping(enabled);
        loopButton.setButtonText(enabled ? "Loop: ON" : "Loop: OFF");
    };
    addAndMakeVisible(loopButton);

    colVolumeDb.fill(0.0f);
    masterVolumeDb = 0.0f;

    addAndMakeVisible(bankLabel);
    bankLabel.setFont(juce::Font(12.0f).boldened());
    bankLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);

    addAndMakeVisible(bankSelector);
    bankSelector.addItem("Bank A (Pads 1-32)", 1);
    bankSelector.addItem("Bank B (Pads 33-64)", 2);
    bankSelector.setSelectedId(1, juce::dontSendNotification);
    bankSelector.onChange = [this] {
        currentBank = bankSelector.getSelectedId() - 1;
        repaint();
    };

    connectApcHardware();

    startTimerHz(30);
    setWantsKeyboardFocus(true);
}

PerformanceComponent::~PerformanceComponent()
{
    stopTimer();
    audioEngine.getKeyboardState().removeListener(this);
    audioEngine.removeListener(this);

    disconnectApcHardware();
}

void PerformanceComponent::disconnectApcHardware()
{
    if (apcMidiOutput != nullptr)
    {
        // Turn off all physical APC Mini hardware LEDs on exit
        for (int note = 0; note < 128; ++note)
        {
            apcMidiOutput->sendMessageNow(juce::MidiMessage::noteOn(1, note, static_cast<juce::uint8>(0)));
        }
        apcMidiOutput = nullptr;
    }

    if (apcMidiInput != nullptr)
    {
        apcMidiInput->stop();
        apcMidiInput = nullptr;
    }
}

void PerformanceComponent::connectApcHardware()
{
    auto inputDevices = juce::MidiInput::getAvailableDevices();
    for (const auto& dev : inputDevices)
    {
        if (dev.name.containsIgnoreCase("APC") || dev.name.containsIgnoreCase("Akai"))
        {
            apcMidiInput = juce::MidiInput::openDevice(dev.identifier, this);
            if (apcMidiInput != nullptr)
                apcMidiInput->start();
            break;
        }
    }

    auto outputDevices = juce::MidiOutput::getAvailableDevices();
    for (const auto& dev : outputDevices)
    {
        if (dev.name.containsIgnoreCase("APC") || dev.name.containsIgnoreCase("Akai"))
        {
            apcMidiOutput = juce::MidiOutput::openDevice(dev.identifier);
            break;
        }
    }

    updateApcHardwareLeds();
}

void PerformanceComponent::sampleLoaded(const juce::String& filePath)
{
    juce::MessageManager::callAsync([this, filePath] {
        bool updated = false;
        for (auto& pad : pads)
        {
            if (pad.file.getFullPathName() == filePath && pad.file.existsAsFile())
            {
                pad.miniWaveform.clear();
                std::unique_ptr<juce::AudioFormatReader> reader(audioEngine.getFormatManager().createReaderFor(pad.file));
                if (reader != nullptr && reader->lengthInSamples > 0)
                {
                    int points = 32;
                    pad.miniWaveform.resize(points, 0.0f);
                    int totalSamples = static_cast<int>(std::min<int64_t>(reader->lengthInSamples, 44100 * 10));
                    juce::AudioBuffer<float> tempBuf(reader->numChannels, totalSamples);
                    reader->read(&tempBuf, 0, totalSamples, 0, true, true);
                    int samplesPerPoint = totalSamples / points;
                    if (samplesPerPoint > 0)
                    {
                        for (int p = 0; p < points; ++p)
                        {
                            auto range = tempBuf.findMinMax(0, p * samplesPerPoint, samplesPerPoint);
                            pad.miniWaveform[p] = std::max(std::abs(range.getStart()), std::abs(range.getEnd()));
                        }
                    }
                }
                updated = true;
            }
        }
        if (updated)
        {
            repaint();
        }
    });
}

void PerformanceComponent::visibilityChanged()
{
    if (isVisible())
    {
        refreshAllPadWaveforms();
        repaint();
    }
}

void PerformanceComponent::refreshAllPadWaveforms()
{
    for (auto& pad : pads)
    {
        if (pad.file.existsAsFile() && pad.miniWaveform.empty())
        {
            std::unique_ptr<juce::AudioFormatReader> reader(audioEngine.getFormatManager().createReaderFor(pad.file));
            if (reader != nullptr && reader->lengthInSamples > 0)
            {
                int points = 32;
                pad.miniWaveform.resize(points, 0.0f);
                int totalSamples = static_cast<int>(std::min<int64_t>(reader->lengthInSamples, 44100 * 10));
                juce::AudioBuffer<float> tempBuf(reader->numChannels, totalSamples);
                reader->read(&tempBuf, 0, totalSamples, 0, true, true);
                int samplesPerPoint = totalSamples / points;
                if (samplesPerPoint > 0)
                {
                    for (int p = 0; p < points; ++p)
                    {
                        auto range = tempBuf.findMinMax(0, p * samplesPerPoint, samplesPerPoint);
                        pad.miniWaveform[p] = std::max(std::abs(range.getStart()), std::abs(range.getEnd()));
                    }
                }
            }
        }
    }
}

void PerformanceComponent::updateApcHardwareLeds()
{
    if (apcMidiOutput == nullptr)
        return;

    for (int i = 0; i < totalPads; ++i)
    {
        const auto& pad = pads[i];
        // Convert top-left to right and down pad index (0..63) to APC Mini physical note (56..63 top, 0..7 bottom)
        int apcNote = (7 - pad.row) * 8 + pad.col;

        int ledColour = 0; // Off
        if (pad.file.existsAsFile())
        {
            if (pad.isTriggered)
                ledColour = 3; // Red / Active
            else
                ledColour = 1; // Green / Ready
        }

        apcMidiOutput->sendMessageNow(juce::MidiMessage::noteOn(1, apcNote, static_cast<juce::uint8>(ledColour)));
    }
}

void PerformanceComponent::handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& message)
{
    if (!audioEngine.isMidiInputEnabled())
        return;

    if (message.isController())
    {
        int cc = message.getControllerNumber();
        int val = message.getControllerValue(); // 0..127
        float norm = static_cast<float>(val) / 127.0f;

        bool isShift = isApcShiftPressed || juce::ModifierKeys::getCurrentModifiersRealtime().isShiftDown();

        // APC Mini 9 Physical Faders
        // Faders 1-8: CC 48 - 55
        if (cc >= 48 && cc <= 55)
        {
            int col = cc - 48;
            if (col >= 0 && col < numCols)
            {
                if (isShift)
                {
                    // Shift held down: Fader controls Column Pitch (-12.0 to +12.0 semitones)
                    float pitch = juce::jmap(norm, -12.0f, 12.0f);
                    colPitchSemi[static_cast<size_t>(col)] = pitch;

                    for (int r = 0; r < numRows; ++r)
                    {
                        int pIdx = r * numCols + col;
                        pads[pIdx].pitchSemi = pitch;
                    }
                    juce::MessageManager::callAsync([this] { repaint(); });
                }
                else
                {
                    // Normal mode: Fader controls Column Volume (-48dB to +6dB)
                    float db = (val == 0) ? -48.0f : juce::jmap(norm, -48.0f, 6.0f);
                    colVolumeDb[static_cast<size_t>(col)] = db;
                }
            }
        }
        // Fader 9 (Master): CC 56
        else if (cc == 56)
        {
            float db = (val == 0) ? -48.0f : juce::jmap(norm, -48.0f, 6.0f);
            masterVolumeDb = db;
            audioEngine.setGain(static_cast<float>(juce::Decibels::decibelsToGain(db)));
        }
    }
    else if (message.isNoteOn())
    {
        int note = message.getNoteNumber();
        if (note == 98 || note == 122) // APC Mini physical Shift button
        {
            isApcShiftPressed = true;
            return;
        }
        handleNoteOn(nullptr, message.getChannel(), note, message.getFloatVelocity());
    }
    else if (message.isNoteOff())
    {
        int note = message.getNoteNumber();
        if (note == 98 || note == 122) // APC Mini physical Shift button released
        {
            isApcShiftPressed = false;
            juce::MessageManager::callAsync([this] { repaint(); });
            return;
        }
    }
}

void PerformanceComponent::pitchTrackingStateChanged(bool enabled)
{
    juce::MessageManager::callAsync([this, enabled] {
        pitchTrackButton.setToggleState(enabled, juce::dontSendNotification);
        pitchTrackButton.setButtonText(enabled ? "Pitch Track: ON" : "Pitch Track: OFF");
    });
}

void PerformanceComponent::oneShotStateChanged(bool enabled)
{
    juce::MessageManager::callAsync([this, enabled] {
        oneShotButton.setToggleState(enabled, juce::dontSendNotification);
        oneShotButton.setButtonText(enabled ? "One Shot: ON" : "One Shot: OFF");
    });
}

void PerformanceComponent::loopingStateChanged(bool enabled)
{
    juce::MessageManager::callAsync([this, enabled] {
        loopButton.setToggleState(enabled, juce::dontSendNotification);
        loopButton.setButtonText(enabled ? "Loop: ON" : "Loop: OFF");
    });
}

void PerformanceComponent::handleNoteOn(juce::MidiKeyboardState*, int, int midiNoteNumber, float velocity)
{
    if (velocity <= 0.0f)
    {
        handleNoteOff(nullptr, 0, midiNoteNumber, 0.0f);
        return;
    }

    int padIdx = -1;

    // 1. APC Mini Native Grid Note Mapping (Notes 0 - 63 converted from top-left to right and down)
    if (midiNoteNumber >= 0 && midiNoteNumber < totalPads)
    {
        int apcRow = 7 - (midiNoteNumber / 8);
        int apcCol = midiNoteNumber % 8;
        padIdx = apcRow * numCols + apcCol;
    }
    // 2. Standard DAW / GM Drum Notes (Notes 36 - 99 sequential top-left to right and down)
    else if (midiNoteNumber >= 36 && midiNoteNumber < 36 + totalPads)
    {
        padIdx = midiNoteNumber - 36;
    }

    if (padIdx >= 0 && padIdx < totalPads)
    {
        juce::MessageManager::callAsync([this, padIdx] {
            triggerPad(padIdx);
        });
    }
}

void PerformanceComponent::handleNoteOff(juce::MidiKeyboardState*, int, int midiNoteNumber, float)
{
    int padIdx = -1;
    if (midiNoteNumber >= 0 && midiNoteNumber < totalPads)
    {
        int apcRow = 7 - (midiNoteNumber / 8);
        int apcCol = midiNoteNumber % 8;
        padIdx = apcRow * numCols + apcCol;
    }
    else if (midiNoteNumber >= 36 && midiNoteNumber < 36 + totalPads)
    {
        padIdx = midiNoteNumber - 36;
    }

    if (padIdx >= 0 && padIdx < totalPads)
    {
        juce::MessageManager::callAsync([this, padIdx] {
            stopPad(padIdx);
        });
    }
}

void PerformanceComponent::timerCallback()
{
    bool needRepaint = false;
    for (auto& pad : pads)
    {
        if (pad.isTriggered)
        {
            pad.playheadPos += 0.04f;
            if (pad.playheadPos >= 1.0f)
            {
                pad.playheadPos = 0.0f;
                pad.isTriggered = false;
            }
            needRepaint = true;
        }

        if (pad.triggerAnim > 0.0f)
        {
            pad.triggerAnim = std::max(0.0f, pad.triggerAnim - 0.08f);
            needRepaint = true;
        }
    }

    if (needRepaint)
        repaint();
}

void PerformanceComponent::paint(juce::Graphics& g)
{
    g.fillAll(OpenWavLookAndFeel::bgDark);

    auto area = getLocalBounds();
    area.removeFromTop(44); // Top Controls area
    auto gridArea = area.reduced(8);

    int startRow = (currentBank == 0) ? 0 : 4;
    int visibleRows = 4;
    float cellW = static_cast<float>(gridArea.getWidth()) / static_cast<float>(numCols);
    float cellH = static_cast<float>(gridArea.getHeight()) / static_cast<float>(visibleRows);

    for (int r = 0; r < visibleRows; ++r)
    {
        int actualRow = startRow + r;
        for (int c = 0; c < numCols; ++c)
        {
            int padIdx = actualRow * numCols + c;
            auto& pad = pads[padIdx];

            juce::Rectangle<float> padRect(
                gridArea.getX() + c * cellW + 3.0f,
                gridArea.getY() + r * cellH + 3.0f,
                cellW - 6.0f,
                cellH - 6.0f
            );

            // Base Pad Background (APC Glassmorphism)
            juce::Colour baseBg = pad.file.existsAsFile() ? OpenWavLookAndFeel::bgCard : OpenWavLookAndFeel::bgHeader;
            if (hoveredPadIndex == padIdx)
                baseBg = baseBg.interpolatedWith(OpenWavLookAndFeel::bgHover, 0.4f);

            // Active RGB Trigger Pulse
            if (pad.triggerAnim > 0.0f)
            {
                baseBg = baseBg.interpolatedWith(pad.color, pad.triggerAnim * 0.6f);
            }

            g.setColour(baseBg);
            g.fillRoundedRectangle(padRect, 6.0f);

            // Glowing APC Border
            juce::Colour borderCol = pad.file.existsAsFile() ? pad.color.withAlpha(0.7f) : OpenWavLookAndFeel::borderColour;
            if (pad.isTriggered)
                borderCol = OpenWavLookAndFeel::accentCyan;

            g.setColour(borderCol);
            g.drawRoundedRectangle(padRect, 6.0f, pad.isTriggered ? 2.5f : 1.2f);

            // Pad Name Badge (e.g. A1, B4)
            juce::String padBadge = padIndexToName(actualRow, c);
            g.setFont(juce::Font(10.0f).boldened());
            g.setColour(borderCol);
            g.drawText(padBadge, padRect.reduced(6, 4), juce::Justification::topLeft, false);

            // Tuned Pitch Badge (e.g. +3.5st or -2.0st)
            if (std::abs(pad.pitchSemi) > 0.05f)
            {
                juce::String pitchStr = (pad.pitchSemi > 0 ? "+" : "") + juce::String(pad.pitchSemi, 1) + "st";
                g.setFont(juce::Font(9.5f).boldened());
                g.setColour(OpenWavLookAndFeel::accentCyan);
                g.drawText(pitchStr, padRect.reduced(6, 4), juce::Justification::topRight, false);
            }

            // Mini Waveform peaks inside pad automatically rendered
            if (pad.file.existsAsFile())
            {
                if (pad.miniWaveform.empty())
                {
                    std::unique_ptr<juce::AudioFormatReader> reader(audioEngine.getFormatManager().createReaderFor(pad.file));
                    if (reader != nullptr && reader->lengthInSamples > 0)
                    {
                        int points = 32;
                        pad.miniWaveform.resize(points, 0.0f);
                        int totalSamples = static_cast<int>(std::min<int64_t>(reader->lengthInSamples, 44100 * 10));
                        juce::AudioBuffer<float> tempBuf(reader->numChannels, totalSamples);
                        reader->read(&tempBuf, 0, totalSamples, 0, true, true);
                        int samplesPerPoint = totalSamples / points;
                        if (samplesPerPoint > 0)
                        {
                            for (int p = 0; p < points; ++p)
                            {
                                auto range = tempBuf.findMinMax(0, p * samplesPerPoint, samplesPerPoint);
                                pad.miniWaveform[p] = std::max(std::abs(range.getStart()), std::abs(range.getEnd()));
                            }
                        }
                    }
                }

                if (!pad.miniWaveform.empty())
                {
                    auto waveArea = padRect.reduced(8, 14);
                    g.setColour(pad.color.withAlpha(0.55f));
                    float waveWidth = waveArea.getWidth();
                    float centerY = waveArea.getCentreY();
                    float step = waveWidth / static_cast<float>(pad.miniWaveform.size());

                    for (size_t i = 0; i < pad.miniWaveform.size(); ++i)
                    {
                        float x = waveArea.getX() + i * step;
                        float h = pad.miniWaveform[i] * (waveArea.getHeight() * 0.45f);
                        g.drawVerticalLine(juce::roundToInt(x), centerY - h, centerY + h);
                    }
                }
            }

            // Sample Name Text
            juce::String displayName = pad.sampleName.isNotEmpty() ? pad.sampleName : "[ Empty ]";
            g.setFont(juce::Font(11.5f).boldened());
            g.setColour(pad.file.existsAsFile() ? OpenWavLookAndFeel::textPrimary : OpenWavLookAndFeel::textSecondary);
            g.drawText(displayName, padRect.reduced(6), juce::Justification::centred, true);

            // Playhead animation
            if (pad.isTriggered && pad.playheadPos > 0.0f)
            {
                float phX = padRect.getX() + padRect.getWidth() * pad.playheadPos;
                g.setColour(OpenWavLookAndFeel::accentCyan);
                g.drawVerticalLine(juce::roundToInt(phX), padRect.getY() + 4.0f, padRect.getBottom() - 4.0f);
            }
        }
    }
}

void PerformanceComponent::resized()
{
    auto area = getLocalBounds().reduced(8);

    // Top Controls Bar
    auto topBar = area.removeFromTop(36);
    newPresetButton.setBounds(topBar.removeFromLeft(48));
    topBar.removeFromLeft(4);
    savePresetButton.setBounds(topBar.removeFromLeft(52));
    topBar.removeFromLeft(4);
    loadPresetButton.setBounds(topBar.removeFromLeft(52));
    topBar.removeFromLeft(6);

    presetSelector.setBounds(topBar.removeFromLeft(150));
    topBar.removeFromLeft(10);

    fillButton.setBounds(topBar.removeFromLeft(100));
    topBar.removeFromLeft(6);
    clearButton.setBounds(topBar.removeFromLeft(75));
    topBar.removeFromLeft(6);
    randomButton.setBounds(topBar.removeFromLeft(80));
    topBar.removeFromLeft(6);
    stopAllButton.setBounds(topBar.removeFromLeft(70));
    topBar.removeFromLeft(10);

    pitchTrackButton.setBounds(topBar.removeFromLeft(110));
    topBar.removeFromLeft(6);
    oneShotButton.setBounds(topBar.removeFromLeft(105));
    topBar.removeFromLeft(6);
    loopButton.setBounds(topBar.removeFromLeft(85));
    topBar.removeFromLeft(10);

    // Global ADSR Horizontal Sliders
    auto placeSlider = [&topBar](juce::Label& lbl, juce::Slider& knob) {
        lbl.setBounds(topBar.removeFromLeft(30));
        topBar.removeFromLeft(2);
        knob.setBounds(topBar.removeFromLeft(55));
        topBar.removeFromLeft(8);
    };

    placeSlider(attackLabel, attackKnob);
    placeSlider(decayLabel, decayKnob);
    placeSlider(sustainLabel, sustainKnob);
    placeSlider(releaseLabel, releaseKnob);
    topBar.removeFromLeft(10);

    bankLabel.setBounds(topBar.removeFromLeft(60));
    bankSelector.setBounds(topBar.removeFromLeft(140));
}

int PerformanceComponent::padIndexAtPos(juce::Point<int> pos) const
{
    auto area = getLocalBounds();
    area.removeFromTop(44);
    auto gridArea = area.reduced(8);

    if (!gridArea.contains(pos))
        return -1;

    int startRow = (currentBank == 0) ? 0 : 4;
    int visibleRows = 4;

    float cellW = static_cast<float>(gridArea.getWidth()) / static_cast<float>(numCols);
    float cellH = static_cast<float>(gridArea.getHeight()) / static_cast<float>(visibleRows);

    int col = static_cast<int>((pos.x - gridArea.getX()) / cellW);
    int rowRel = static_cast<int>((pos.y - gridArea.getY()) / cellH);

    col = juce::jlimit(0, numCols - 1, col);
    rowRel = juce::jlimit(0, visibleRows - 1, rowRel);

    int actualRow = startRow + rowRel;
    return actualRow * numCols + col;
}

void PerformanceComponent::mouseDown(const juce::MouseEvent& e)
{
    int padIdx = padIndexAtPos(e.getPosition());
    if (padIdx >= 0 && padIdx < totalPads)
    {
        if (e.mods.isPopupMenu())
        {
            mouseRightClick(e, padIdx);
        }
        else
        {
            triggerPad(padIdx);
        }
    }
}

void PerformanceComponent::mouseUp(const juce::MouseEvent& /*e*/)
{
}

void PerformanceComponent::mouseRightClick(const juce::MouseEvent& e, int padIndex)
{
    juce::PopupMenu menu;
    auto& pad = pads[padIndex];

    menu.addSectionHeader("Pad " + padIndexToName(pad.row, pad.col) + ": " + (pad.sampleName.isNotEmpty() ? pad.sampleName : "Empty"));
    menu.addItem(1, "Load Sample File...");
    if (pad.file.existsAsFile())
    {
        menu.addItem(2, "Clear Pad");
        menu.addSeparator();
        menu.addItem(3, "Pitch +1 Semitone");
        menu.addItem(4, "Pitch -1 Semitone");
        menu.addItem(5, "Reset Pitch");
        menu.addSeparator();
        menu.addItem(6, "Gain +3 dB");
        menu.addItem(7, "Gain -3 dB");
        menu.addItem(8, "Reset Gain");
    }

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea({ e.getScreenX(), e.getScreenY(), 1, 1 }),
        [this, padIndex](int result) {
            if (result == 1) // Load File
            {
                auto chooser = std::make_shared<juce::FileChooser>(
                    "Select Audio File for Pad...",
                    juce::File::getSpecialLocation(juce::File::userHomeDirectory),
                    "*.wav;*.mp3;*.flac;*.ogg;*.aif;*.aiff;*.aifc");

                chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                    [this, padIndex, chooser](const juce::FileChooser& fc) {
                        auto file = fc.getResult();
                        if (file.existsAsFile())
                        {
                            pads[padIndex].file = file;
                            pads[padIndex].sampleName = file.getFileNameWithoutExtension();
                            repaint();
                        }
                    });
            }
            else if (result == 2) // Clear
            {
                pads[padIndex].file = juce::File();
                pads[padIndex].sampleName = "";
                pads[padIndex].miniWaveform.clear();
                repaint();
            }
            else if (result == 3) { pads[padIndex].pitchSemi += 1.0f; }
            else if (result == 4) { pads[padIndex].pitchSemi -= 1.0f; }
            else if (result == 5) { pads[padIndex].pitchSemi = 0.0f; }
            else if (result == 6) { pads[padIndex].gainDb += 3.0f; }
            else if (result == 7) { pads[padIndex].gainDb -= 3.0f; }
            else if (result == 8) { pads[padIndex].gainDb = 0.0f; }
        });
}

void PerformanceComponent::triggerPad(int index)
{
    if (index < 0 || index >= totalPads)
        return;

    auto& pad = pads[index];
    pad.isTriggered = true;
    pad.triggerAnim = 1.0f;
    pad.playheadPos = 0.0f;

    if (pad.file.existsAsFile())
    {
        float masterGain = static_cast<float>(juce::Decibels::decibelsToGain(masterVolumeDb));
        float colGain = static_cast<float>(juce::Decibels::decibelsToGain(colVolumeDb[static_cast<size_t>(pad.col)]));
        float padGain = static_cast<float>(juce::Decibels::decibelsToGain(pad.gainDb)) * colGain * masterGain;

        float att = static_cast<float>(attackKnob.getValue());
        float dec = static_cast<float>(decayKnob.getValue());
        float sus = static_cast<float>(sustainKnob.getValue());
        float rel = static_cast<float>(releaseKnob.getValue());

        bool isOneShot = oneShotButton.getToggleState();
        bool isLoop = loopButton.getToggleState();

        audioEngine.playZoneVoice(pad.file, 36 + index, 60, pad.pitchSemi * 100.0f, pad.gainDb, padGain, att, dec, sus, rel, isOneShot, isLoop);
    }
    updateApcHardwareLeds();
    repaint();
}

void PerformanceComponent::stopPad(int index)
{
    if (index >= 0 && index < totalPads)
    {
        pads[index].isTriggered = false;
        pads[index].playheadPos = 0.0f;
        audioEngine.stopZoneVoice(36 + index);
        updateApcHardwareLeds();
        repaint();
    }
}

void PerformanceComponent::stopAllPads()
{
    for (int i = 0; i < totalPads; ++i)
    {
        pads[i].isTriggered = false;
        pads[i].playheadPos = 0.0f;
        audioEngine.stopZoneVoice(36 + i);
    }
    audioEngine.stop();
    updateApcHardwareLeds();
    repaint();
}

void PerformanceComponent::autoFillGridFromDatabase()
{
    auto allItems = dbManager.getAllItems();
    if (allItems.empty())
        return;

    for (size_t i = 0; i < pads.size() && i < allItems.size(); ++i)
    {
        pads[i].file = juce::File(allItems[i].filePath);
        pads[i].sampleName = allItems[i].fileName;
    }
    repaint();
}

void PerformanceComponent::clearAllPads()
{
    for (auto& pad : pads)
    {
        pad.file = juce::File();
        pad.sampleName = "";
        pad.miniWaveform.clear();
        pad.isTriggered = false;
    }
    repaint();
}

void PerformanceComponent::randomizeGrid()
{
    auto allItems = dbManager.getAllItems();
    if (allItems.empty())
        return;

    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(allItems.begin(), allItems.end(), g);

    for (size_t i = 0; i < pads.size() && i < allItems.size(); ++i)
    {
        pads[i].file = juce::File(allItems[i].filePath);
        pads[i].sampleName = allItems[i].fileName;
    }
    repaint();
}

bool PerformanceComponent::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& f : files)
    {
        juce::File file(f);
        if (file.getFileExtension().containsIgnoreCase("wav") ||
            file.getFileExtension().containsIgnoreCase("mp3") ||
            file.getFileExtension().containsIgnoreCase("flac") ||
            file.getFileExtension().containsIgnoreCase("ogg"))
            return true;
    }
    return false;
}

void PerformanceComponent::filesDropped(const juce::StringArray& files, int x, int y)
{
    int padIdx = padIndexAtPos({ x, y });
    if (padIdx >= 0 && padIdx < totalPads && !files.isEmpty())
    {
        juce::File dropped(files[0]);
        if (dropped.existsAsFile())
        {
            pads[padIdx].file = dropped;
            pads[padIdx].sampleName = dropped.getFileNameWithoutExtension();
            repaint();
        }
    }
}

bool PerformanceComponent::isInterestedInDragSource(const juce::DragAndDropTarget::SourceDetails& /*dragSourceDetails*/)
{
    return true;
}

void PerformanceComponent::itemDropped(const juce::DragAndDropTarget::SourceDetails& dragSourceDetails)
{
    int padIdx = padIndexAtPos(dragSourceDetails.localPosition);
    if (padIdx >= 0 && padIdx < totalPads)
    {
        juce::String path = dragSourceDetails.description.toString();
        juce::File file(path);
        if (file.existsAsFile())
        {
            pads[padIdx].file = file;
            pads[padIdx].sampleName = file.getFileNameWithoutExtension();
            repaint();
        }
    }
}

void PerformanceComponent::sliderValueChanged(juce::Slider* /*slider*/)
{
}

juce::File PerformanceComponent::getPresetsDirectory() const
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("OpenWav")
                   .getChildFile("PerformancePresets");
    if (!dir.exists())
        dir.createDirectory();
    return dir;
}

void PerformanceComponent::refreshPresetList()
{
    presetSelector.clear(juce::dontSendNotification);
    presetSelector.addItem("[ Empty Performance ]", 1);

    auto dir = getPresetsDirectory();
    auto files = dir.findChildFiles(juce::File::findFiles, false, "*.owperf");

    for (int i = 0; i < files.size(); ++i)
    {
        presetSelector.addItem(files[i].getFileNameWithoutExtension(), i + 2);
    }

    if (presetSelector.getSelectedId() <= 0)
        presetSelector.setSelectedId(1, juce::dontSendNotification);
}

void PerformanceComponent::savePerformancePreset(const juce::File& file)
{
    juce::DynamicObject::Ptr rootObj = new juce::DynamicObject();
    rootObj->setProperty("presetName", file.getFileNameWithoutExtension());

    juce::Array<juce::var> padsArray;
    for (int i = 0; i < totalPads; ++i)
    {
        if (pads[i].file.existsAsFile())
        {
            juce::DynamicObject::Ptr padObj = new juce::DynamicObject();
            padObj->setProperty("index", i);
            padObj->setProperty("filePath", pads[i].file.getFullPathName());
            padObj->setProperty("sampleName", pads[i].sampleName);
            padObj->setProperty("gainDb", pads[i].gainDb);
            padObj->setProperty("pitchSemi", pads[i].pitchSemi);
            padsArray.add(padObj.get());
        }
    }
    rootObj->setProperty("pads", padsArray);
    rootObj->setProperty("attack", attackKnob.getValue());
    rootObj->setProperty("decay", decayKnob.getValue());
    rootObj->setProperty("sustain", sustainKnob.getValue());
    rootObj->setProperty("release", releaseKnob.getValue());

    juce::var jsonVar(rootObj.get());
    juce::String jsonStr = juce::JSON::toString(jsonVar, true);
    file.replaceWithText(jsonStr);

    refreshPresetList();

    auto dir = getPresetsDirectory();
    auto files = dir.findChildFiles(juce::File::findFiles, false, "*.owperf");
    for (int i = 0; i < files.size(); ++i)
    {
        if (files[i].getFileNameWithoutExtension() == file.getFileNameWithoutExtension())
        {
            presetSelector.setSelectedId(i + 2, juce::dontSendNotification);
            break;
        }
    }
}

void PerformanceComponent::loadPerformancePreset(const juce::File& file)
{
    if (!file.existsAsFile())
        return;

    clearAllPads();

    juce::var jsonVar = juce::JSON::parse(file.loadFileAsString());
    if (jsonVar.isObject())
    {
        auto* rootObj = jsonVar.getDynamicObject();
        rootObj->setProperty("attack", attackKnob.getValue());
        rootObj->setProperty("decay", decayKnob.getValue());
        rootObj->setProperty("sustain", sustainKnob.getValue());
        rootObj->setProperty("release", releaseKnob.getValue());

        if (rootObj->hasProperty("pads"))
        {
            auto* padsArray = rootObj->getProperty("pads").getArray();
            if (padsArray != nullptr)
            {
                for (const auto& padVar : *padsArray)
                {
                    if (padVar.isObject())
                    {
                        auto* padObj = padVar.getDynamicObject();
                        int idx = padObj->getProperty("index");
                        juce::String path = padObj->getProperty("filePath");
                        juce::String name = padObj->getProperty("sampleName");
                        float gain = padObj->getProperty("gainDb");
                        float pitch = padObj->getProperty("pitchSemi");

                        if (idx >= 0 && idx < totalPads)
                        {
                            juce::File f(path);
                            if (f.existsAsFile())
                            {
                                pads[idx].file = f;
                                pads[idx].sampleName = name;
                                pads[idx].gainDb = gain;
                                pads[idx].pitchSemi = pitch;
                            }
                        }
                    }
                }
            }
        }
    }

    if (jsonVar.isObject())
    {
        auto* rootObj = jsonVar.getDynamicObject();
        if (rootObj->hasProperty("attack")) attackKnob.setValue(rootObj->getProperty("attack"), juce::dontSendNotification);
        if (rootObj->hasProperty("decay")) decayKnob.setValue(rootObj->getProperty("decay"), juce::dontSendNotification);
        if (rootObj->hasProperty("sustain")) sustainKnob.setValue(rootObj->getProperty("sustain"), juce::dontSendNotification);
        if (rootObj->hasProperty("release")) releaseKnob.setValue(rootObj->getProperty("release"), juce::dontSendNotification);
    }

    updateApcHardwareLeds();
    repaint();
}

void PerformanceComponent::buttonClicked(juce::Button* button)
{
    if (button == &newPresetButton)
    {
        clearAllPads();
        presetSelector.setSelectedId(1, juce::dontSendNotification);
    }
    else if (button == &savePresetButton)
    {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Save Performance Preset...",
            getPresetsDirectory(),
            "*.owperf");
        chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc) {
                auto file = fc.getResult();
                if (file.getFileName().isNotEmpty())
                {
                    if (file.getFileExtension().isEmpty())
                        file = file.withFileExtension("owperf");
                    savePerformancePreset(file);
                }
            });
    }
    else if (button == &loadPresetButton)
    {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Open Performance Preset...",
            getPresetsDirectory(),
            "*.owperf");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc) {
                auto file = fc.getResult();
                if (file.existsAsFile())
                {
                    loadPerformancePreset(file);
                }
            });
    }
    else if (button == &fillButton)
        autoFillGridFromDatabase();
    else if (button == &clearButton)
        clearAllPads();
    else if (button == &randomButton)
        randomizeGrid();
    else if (button == &stopAllButton)
        stopAllPads();
}

bool PerformanceComponent::keyPressed(const juce::KeyPress& key)
{
    // Computer Keyboard layout mapping for APC Pad performance triggering
    static const std::map<int, int> keyToPadMap = {
        { '1', 0 }, { '2', 1 }, { '3', 2 }, { '4', 3 }, { '5', 4 }, { '6', 5 }, { '7', 6 }, { '8', 7 },
        { 'q', 8 }, { 'w', 9 }, { 'e', 10 }, { 'r', 11 }, { 't', 12 }, { 'y', 13 }, { 'u', 14 }, { 'i', 15 },
        { 'a', 16 }, { 's', 17 }, { 'd', 18 }, { 'f', 19 }, { 'g', 20 }, { 'h', 21 }, { 'j', 22 }, { 'k', 23 },
        { 'z', 24 }, { 'x', 25 }, { 'c', 26 }, { 'v', 27 }, { 'b', 28 }, { 'n', 29 }, { 'm', 30 }
    };

    auto charCode = juce::CharacterFunctions::toLowerCase(key.getTextCharacter());
    auto it = keyToPadMap.find(charCode);
    if (it != keyToPadMap.end())
    {
        triggerPad(it->second);
        return true;
    }
    return false;
}

void PerformanceComponent::loadSlices(const std::vector<SampleMapZone>& zones)
{
    clearAllPads();
    for (size_t i = 0; i < zones.size(); ++i)
    {
        const auto& z = zones[i];
        juce::File file(z.filePath);
        if (file.existsAsFile())
        {
            int padIdx = (z.rootNote >= 36 && z.rootNote < 36 + totalPads)
                             ? (z.rootNote - 36)
                             : static_cast<int>(i);

            if (padIdx >= 0 && padIdx < totalPads)
            {
                pads[padIdx].file = file;
                pads[padIdx].sampleName = z.sampleName.isNotEmpty() ? z.sampleName : file.getFileNameWithoutExtension();
                pads[padIdx].gainDb = z.gainDb;
                pads[padIdx].pitchSemi = z.fineTuneCents / 100.0f;
            }
        }
    }
    updateApcHardwareLeds();
    repaint();
}

} // namespace openwav
