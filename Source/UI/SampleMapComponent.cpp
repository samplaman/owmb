#include "SampleMapComponent.h"
#include "OpenWavLookAndFeel.h"
#include "LorisResynthesisDialog.h"
#include "ShortcutManager.h"
#include <regex>
// Rebuild trigger

namespace openwav
{

// ─────────────────────────────────────────────────────────
//  VelocityCurveComponent Implementation
// ─────────────────────────────────────────────────────────
VelocityCurveComponent::VelocityCurveComponent()
{
    setOpaque(false);
}

void VelocityCurveComponent::setSensitivity(float s)
{
    sensitivity = juce::jlimit(0.0f, 1.0f, s);
    repaint();
}

void VelocityCurveComponent::setCurve(float c)
{
    curve = juce::jlimit(-1.0f, 1.0f, c);
    repaint();
}

void VelocityCurveComponent::setCurveMode(int mode)
{
    curveMode = mode;
    repaint();
}

void VelocityCurveComponent::setMinFloor(int floorVal)
{
    minFloor = juce::jlimit(0, 127, floorVal);
    repaint();
}

float VelocityCurveComponent::computeResponse(float inVel) const
{
    if (inVel <= 0.0001f) return 0.0f;
    float v = juce::jlimit(0.0f, 1.0f, inVel);

    float curved = v;
    if (curveMode == 4) // Fixed
    {
        curved = 1.0f;
    }
    else if (curveMode == 3) // S-Curve
    {
        curved = v * v * (3.0f - 2.0f * v);
        if (std::abs(curve) > 0.01f)
        {
            float c = juce::jlimit(-1.0f, 1.0f, curve);
            if (c > 0.0f) curved = std::pow(curved, 1.0f + 2.5f * c);
            else curved = std::pow(curved, 1.0f / (1.0f - 2.5f * c));
        }
    }
    else
    {
        float c = juce::jlimit(-1.0f, 1.0f, curve);
        if (curveMode == 1 && c <= 0.0f) c = 0.5f;
        else if (curveMode == 2 && c >= 0.0f) c = -0.5f;

        if (c > 0.001f)
            curved = std::pow(v, 1.0f + 2.5f * c);
        else if (c < -0.001f)
            curved = std::pow(v, 1.0f / (1.0f - 2.5f * c));
        else
            curved = v;
    }

    float out = (1.0f - sensitivity) + sensitivity * curved;
    if (minFloor > 0)
    {
        float fNorm = static_cast<float>(minFloor) / 127.0f;
        out = fNorm + (1.0f - fNorm) * out;
    }
    return juce::jlimit(0.0f, 1.0f, out);
}

void VelocityCurveComponent::triggerHit(int velInt)
{
    lastHitVel = juce::jlimit(0, 127, velInt);
    float inNorm = static_cast<float>(lastHitVel) / 127.0f;
    lastHitResponse = computeResponse(inNorm);
    lastHitTimeMs = juce::Time::getMillisecondCounter();

    if (onVelocityHit)
    {
        int outVel = juce::jlimit(0, 127, static_cast<int>(lastHitResponse * 127.0f + 0.5f));
        float db = (lastHitResponse > 0.001f) ? (20.0f * std::log10(lastHitResponse)) : -60.0f;
        onVelocityHit(lastHitVel, outVel, db);
    }

    repaint();
}

void VelocityCurveComponent::updateHitFade()
{
    if (lastHitVel >= 0)
    {
        uint32_t now = juce::Time::getMillisecondCounter();
        if (now - lastHitTimeMs > 1200)
        {
            lastHitVel = -1;
        }
        repaint();
    }
}

juce::Rectangle<float> VelocityCurveComponent::getGraphArea() const
{
    return getLocalBounds().toFloat().reduced(14.0f, 12.0f);
}

juce::Point<float> VelocityCurveComponent::getNodePosition(const juce::Rectangle<float>& graphArea) const
{
    float inNorm = 0.5f;
    float outNorm = computeResponse(inNorm);
    float x = graphArea.getX() + inNorm * graphArea.getWidth();
    float y = graphArea.getBottom() - outNorm * graphArea.getHeight();
    return { x, y };
}

void VelocityCurveComponent::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    auto graphArea = getGraphArea();

    // Background card
    g.setColour(OpenWavLookAndFeel::bgDark.withAlpha(0.75f));
    g.fillRoundedRectangle(area, 6.0f);
    g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.35f));
    g.drawRoundedRectangle(area, 6.0f, 1.0f);

    // Inner graph grid background
    g.setColour(juce::Colour(0xff101318));
    g.fillRect(graphArea);

    // Grid lines at 32, 64, 96 (25%, 50%, 75%)
    g.setFont(juce::Font(8.0f));
    for (int v : { 32, 64, 96 })
    {
        float ratio = static_cast<float>(v) / 127.0f;
        float x = graphArea.getX() + ratio * graphArea.getWidth();
        float y = graphArea.getBottom() - ratio * graphArea.getHeight();

        g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.2f));
        g.drawVerticalLine(static_cast<int>(x), graphArea.getY(), graphArea.getBottom());
        g.drawHorizontalLine(static_cast<int>(y), graphArea.getX(), graphArea.getRight());

        g.setColour(OpenWavLookAndFeel::textSecondary.withAlpha(0.4f));
        g.drawText(juce::String(v), x - 10.0f, graphArea.getBottom() + 1.0f, 20.0f, 9.0f, juce::Justification::centred);
        g.drawText(juce::String(v), graphArea.getX() - 14.0f, y - 5.0f, 12.0f, 10.0f, juce::Justification::right);
    }

    // Diagonal 1:1 reference line (linear baseline)
    {
        g.setColour(OpenWavLookAndFeel::textSecondary.withAlpha(0.18f));
        juce::Line<float> diag(graphArea.getX(), graphArea.getBottom(), graphArea.getRight(), graphArea.getY());
        float dashes[] = { 3.0f, 3.0f };
        g.drawDashedLine(diag, dashes, 2, 1.0f);
    }

    // Build the curve path
    juce::Path curvePath;
    juce::Path fillPath;

    int steps = 64;
    for (int i = 0; i <= steps; ++i)
    {
        float inNorm = static_cast<float>(i) / static_cast<float>(steps);
        float outNorm = computeResponse(inNorm);
        float px = graphArea.getX() + inNorm * graphArea.getWidth();
        float py = graphArea.getBottom() - outNorm * graphArea.getHeight();

        if (i == 0)
        {
            curvePath.startNewSubPath(px, py);
            fillPath.startNewSubPath(px, graphArea.getBottom());
            fillPath.lineTo(px, py);
        }
        else
        {
            curvePath.lineTo(px, py);
            fillPath.lineTo(px, py);
        }
    }
    fillPath.lineTo(graphArea.getRight(), graphArea.getBottom());
    fillPath.closeSubPath();

    // Fill under curve with glowing cyan gradient
    juce::ColourGradient fillGrad(OpenWavLookAndFeel::accentCyan.withAlpha(0.28f), graphArea.getX(), graphArea.getY(),
                                  OpenWavLookAndFeel::accentCyan.withAlpha(0.02f), graphArea.getX(), graphArea.getBottom(), false);
    g.setGradientFill(fillGrad);
    g.fillPath(fillPath);

    // Glowing stroke for curve
    g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.35f));
    g.strokePath(curvePath, juce::PathStrokeType(4.0f));
    g.setColour(OpenWavLookAndFeel::accentCyan);
    g.strokePath(curvePath, juce::PathStrokeType(2.0f));

    // Interactive Control Node at Center
    auto nodePos = getNodePosition(graphArea);
    float nodeRadius = (isHoveringNode || isDraggingNode) ? 6.5f : 5.0f;

    // Node outer glow
    g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.4f));
    g.fillEllipse(nodePos.x - nodeRadius - 3.0f, nodePos.y - nodeRadius - 3.0f, (nodeRadius + 3.0f) * 2.0f, (nodeRadius + 3.0f) * 2.0f);
    // Node body
    g.setColour(juce::Colours::white);
    g.fillEllipse(nodePos.x - nodeRadius, nodePos.y - nodeRadius, nodeRadius * 2.0f, nodeRadius * 2.0f);
    g.setColour(OpenWavLookAndFeel::accentCyan.darker(0.3f));
    g.drawEllipse(nodePos.x - nodeRadius, nodePos.y - nodeRadius, nodeRadius * 2.0f, nodeRadius * 2.0f, 1.5f);

    // Live Audition / Hit Dot animation
    if (lastHitVel >= 0)
    {
        uint32_t now = juce::Time::getMillisecondCounter();
        float ageMs = static_cast<float>(now - lastHitTimeMs);
        float alpha = juce::jlimit(0.0f, 1.0f, 1.0f - (ageMs / 1200.0f));

        if (alpha > 0.01f)
        {
            float inNorm = static_cast<float>(lastHitVel) / 127.0f;
            float hitX = graphArea.getX() + inNorm * graphArea.getWidth();
            float hitY = graphArea.getBottom() - lastHitResponse * graphArea.getHeight();

            // Animated pulsing halo
            g.setColour(juce::Colours::white.withAlpha(0.25f * alpha));
            g.fillEllipse(hitX - 9.0f, hitY - 9.0f, 18.0f, 18.0f);

            g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.85f * alpha));
            g.fillEllipse(hitX - 5.0f, hitY - 5.0f, 10.0f, 10.0f);

            g.setColour(juce::Colours::white.withAlpha(alpha));
            g.fillEllipse(hitX - 2.5f, hitY - 2.5f, 5.0f, 5.0f);

            // Small badge showing input and output
            int outV = juce::jlimit(0, 127, static_cast<int>(lastHitResponse * 127.0f + 0.5f));
            juce::String badge = juce::String(lastHitVel) + " → " + juce::String(outV);
            g.setFont(juce::Font(8.5f, juce::Font::bold));
            g.setColour(OpenWavLookAndFeel::bgDark.withAlpha(0.85f * alpha));
            float badgeW = 38.0f;
            float badgeX = juce::jlimit(graphArea.getX(), graphArea.getRight() - badgeW, hitX - badgeW * 0.5f);
            float badgeY = (hitY - 18.0f < graphArea.getY()) ? (hitY + 8.0f) : (hitY - 17.0f);
            g.fillRoundedRectangle(badgeX, badgeY, badgeW, 12.0f, 3.0f);
            g.setColour(juce::Colours::white.withAlpha(alpha));
            g.drawText(badge, badgeX, badgeY, badgeW, 12.0f, juce::Justification::centred);
        }
    }

    // Border around graph
    g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.4f));
    g.drawRect(graphArea, 1.0f);

    // Coordinate axis tags
    g.setFont(juce::Font(7.5f));
    g.setColour(OpenWavLookAndFeel::textSecondary.withAlpha(0.5f));
    g.drawText("IN: 0", graphArea.getX(), graphArea.getBottom() + 1.0f, 24.0f, 9.0f, juce::Justification::left);
    g.drawText("127", graphArea.getRight() - 18.0f, graphArea.getBottom() + 1.0f, 18.0f, 9.0f, juce::Justification::right);
    g.drawText("OUT: 127", graphArea.getX() + 2.0f, graphArea.getY() + 2.0f, 40.0f, 8.0f, juce::Justification::left);
}

void VelocityCurveComponent::mouseDown(const juce::MouseEvent& e)
{
    auto graphArea = getGraphArea();
    auto nodePos = getNodePosition(graphArea);
    if (e.position.getDistanceFrom(nodePos) <= 12.0f)
    {
        isDraggingNode = true;
    }
    else if (graphArea.contains(e.position))
    {
        isDraggingNode = true;
        mouseDrag(e);
    }
}

void VelocityCurveComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (isDraggingNode)
    {
        auto graphArea = getGraphArea();
        float normX = juce::jlimit(0.0f, 1.0f, (e.position.x - graphArea.getX()) / graphArea.getWidth());
        float normY = juce::jlimit(0.0f, 1.0f, 1.0f - (e.position.y - graphArea.getY()) / graphArea.getHeight());

        float expectedLinear = normX;
        float diff = normY - expectedLinear;
        float newCurve = juce::jlimit(-1.0f, 1.0f, diff * 3.5f);
        curve = newCurve;
        curveMode = 0;

        if (e.mods.isShiftDown())
        {
            sensitivity = juce::jlimit(0.0f, 1.0f, normY);
        }

        repaint();
        if (onCurveChanged)
            onCurveChanged(sensitivity, curve, curveMode);
    }
}

void VelocityCurveComponent::mouseUp(const juce::MouseEvent&)
{
    isDraggingNode = false;
    repaint();
}

void VelocityCurveComponent::mouseMove(const juce::MouseEvent& e)
{
    auto graphArea = getGraphArea();
    auto nodePos = getNodePosition(graphArea);
    bool nearNode = (e.position.getDistanceFrom(nodePos) <= 10.0f);
    if (nearNode != isHoveringNode)
    {
        isHoveringNode = nearNode;
        repaint();
    }
    hoverPos = e.position;
}

void VelocityCurveComponent::mouseExit(const juce::MouseEvent&)
{
    isHoveringNode = false;
    repaint();
}

// ─────────────────────────────────────────────────────────
//  SampleMapComponent Implementation
// ─────────────────────────────────────────────────────────
SampleMapComponent::SampleMapComponent(AudioEngine& engine)
    : audioEngine(engine)
{
    setOpaque(true);
    setWantsKeyboardFocus(true);
    audioEngine.getKeyboardState().addListener(this);
    audioEngine.addListener(this);

    // ── Action Toolbar Buttons ─────────────────────────
    addAndMakeVisible(addSampleButton);
    addSampleButton.addListener(this);

    addAndMakeVisible(lorisResynthButton);
    lorisResynthButton.addListener(this);
    lorisResynthButton.setTooltip("Resynthesize a single sample across notes using Loris Additive Modeling");
    lorisResynthButton.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::accentCyan.brighter(0.2f));

    addAndMakeVisible(deleteSelectedButton);
    deleteSelectedButton.addListener(this);
    deleteSelectedButton.setTooltip("Delete selected sample zones (or press Delete/Backspace)");
    deleteSelectedButton.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::favoriteRed.brighter(0.2f));

    addAndMakeVisible(autoMapPitchButton);
    autoMapPitchButton.addListener(this);

    addAndMakeVisible(autoMapChromaticButton);
    autoMapChromaticButton.addListener(this);

    addAndMakeVisible(autoMapVelButton);
    autoMapVelButton.addListener(this);

    addAndMakeVisible(autoMapRRButton);
    autoMapRRButton.addListener(this);

    addAndMakeVisible(clearMapButton);
    clearMapButton.addListener(this);

    addAndMakeVisible(saveMapButton);
    saveMapButton.addListener(this);

    addAndMakeVisible(loadMapButton);
    loadMapButton.addListener(this);

    addAndMakeVisible(exportZipButton);
    exportZipButton.addListener(this);
    exportZipButton.setTooltip("Export complete sample map and audio samples as a .zip bundle");

    roundRobinButton.setClickingTogglesState(false);
    roundRobinButton.setButtonText(roundRobinMode == 0 ? "RR: Cycle" : (roundRobinMode == 1 ? "RR: Random" : "RR: OFF"));
    roundRobinButton.onClick = [this] {
        roundRobinMode = (roundRobinMode + 1) % 3;
        roundRobinButton.setButtonText(roundRobinMode == 0 ? "RR: Cycle" : (roundRobinMode == 1 ? "RR: Random" : "RR: OFF"));
        if (onStateChanged) onStateChanged();
    };
    addAndMakeVisible(roundRobinButton);

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

    openFxRackButton.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::accentCyan.withAlpha(0.25f));
    openFxRackButton.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::accentCyan);
    openFxRackButton.setTooltip("Open Performance FX Rack for Sample Mapped Items");
    openFxRackButton.onClick = [this] {
        if (onOpenFxRackRequested)
            onOpenFxRackRequested();
    };
    addAndMakeVisible(openFxRackButton);

    midiChannelButton.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::accentCyan.withAlpha(0.25f));
    midiChannelButton.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::accentCyan);
    midiChannelButton.setTooltip("Select incoming MIDI Channel for the Sample Mapper (Default: Channel 2)");
    updateMidiChannelButtonText();
    midiChannelButton.onClick = [this] {
        juce::PopupMenu m;
        m.addItem(100, "Omni (All Channels)", true, midiChannelSetting == 0);
        m.addSeparator();
        for (int ch = 1; ch <= 16; ++ch)
        {
            juce::String label = "Channel " + juce::String(ch);
            if (ch == 2)
                label += " (Default)";
            else if (ch == 1)
                label += " (Master Sample)";
            m.addItem(ch, label, true, midiChannelSetting == ch);
        }
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&midiChannelButton),
            [this](int result) {
                if (result == 100)
                {
                    midiChannelSetting = 0; // Omni
                    updateMidiChannelButtonText();
                    if (onStateChanged) onStateChanged();
                }
                else if (result >= 1 && result <= 16)
                {
                    midiChannelSetting = result;
                    updateMidiChannelButtonText();
                    if (onStateChanged) onStateChanged();
                }
            });
    };
    addAndMakeVisible(midiChannelButton);

    // ── Inspector Labels & Sliders ─────────────────────
    inspectorTitle.setFont(juce::Font(14.0f).boldened());
    inspectorTitle.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    addAndMakeVisible(inspectorTitle);

    sampleNameValue.setFont(juce::Font(12.0f).boldened());
    sampleNameValue.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textPrimary);
    addAndMakeVisible(sampleNameValue);

    auto setupSlider = [this](juce::Slider& s, juce::Label& lbl, const juce::String& text, double minV, double maxV, double stepV, double defV) {
        lbl.setFont(juce::Font(11.0f));
        lbl.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
        addAndMakeVisible(lbl);

        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 45, 18);
        s.setRange(minV, maxV, stepV);
        s.setValue(defV, juce::dontSendNotification);
        s.addListener(this);
        addAndMakeVisible(s);
    };

    setupSlider(rootNoteSlider, rootNoteTitle, "Root Note:", 0.0, 127.0, 1.0, 60.0);
    setupSlider(keyLowSlider, keyLowTitle, "Key Low:", 0.0, 127.0, 1.0, 36.0);
    setupSlider(keyHighSlider, keyHighTitle, "Key High:", 0.0, 127.0, 1.0, 84.0);
    setupSlider(velLowSlider, velLowTitle, "Vel Low:", 0.0, 127.0, 1.0, 0.0);
    setupSlider(velHighSlider, velHighTitle, "Vel High:", 0.0, 127.0, 1.0, 127.0);
    setupSlider(rrSlider, rrTitle, "Round Robin:", 1.0, 8.0, 1.0, 1.0);
    setupSlider(tuneSlider, tuneTitle, "Fine Tune:", -100.0, 100.0, 1.0, 0.0);
    setupSlider(gainSlider, gainTitle, "Gain (dB):", -24.0, 12.0, 0.5, 0.0);

    setupSlider(attackSlider, attackTitle, "Attack (ms):", 0.0, 2000.0, 1.0, 5.0);
    setupSlider(decaySlider, decayTitle, "Decay (ms):", 0.0, 2000.0, 1.0, 100.0);
    setupSlider(sustainSlider, sustainTitle, "Sustain (%):", 0.0, 1.0, 0.01, 1.0);
    setupSlider(releaseSlider, releaseTitle, "Release (ms):", 0.0, 5000.0, 1.0, 200.0);
    setupSlider(reverbSlider, reverbTitle, "Reverb (%):", 0.0, 100.0, 1.0, 0.0);

    inspectorDeleteButton.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::favoriteRed.darker(0.3f));
    inspectorDeleteButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    inspectorDeleteButton.onClick = [this] {
        deleteSelectedZones();
    };
    addChildComponent(inspectorDeleteButton);

    // ── ADSR Rotary Knobs (Top Bar beside Clear Map) ──
    auto setupKnob = [this](juce::Slider& s, juce::Label& lbl, const juce::String& text, double minV, double maxV, double stepV, double defV) {
        lbl.setFont(juce::Font(11.0f).boldened());
        lbl.setText(text, juce::dontSendNotification);
        lbl.setJustificationType(juce::Justification::centred);
        lbl.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
        addAndMakeVisible(lbl);

        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 48, 14);
        s.setRange(minV, maxV, stepV);
        s.setValue(defV, juce::dontSendNotification);
        s.addListener(this);
        addAndMakeVisible(s);
    };

    setupKnob(attackKnob, attackLabel, "Attack", 0.0, 2000.0, 1.0, 5.0);
    setupKnob(decayKnob, decayLabel, "Decay", 0.0, 2000.0, 1.0, 100.0);
    setupKnob(sustainKnob, sustainLabel, "Sustain", 0.0, 1.0, 0.01, 1.0);
    setupKnob(releaseKnob, releaseLabel, "Release", 0.0, 5000.0, 1.0, 200.0);

    // ── Velocity Curve Button & Studio Controls ──────────────
    velCurveButton.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::accentCyan.withAlpha(0.20f));
    velCurveButton.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::accentCyan);
    velCurveButton.setTooltip("Toggle Visual Velocity Curve & Sensitivity Editor");
    updateVelocityCurveButtonText();
    velCurveButton.onClick = [this] {
        if (inspectorActiveTab == 1)
            inspectorActiveTab = 0;
        else
            inspectorActiveTab = 1;
        inspectorTabZonesBtn.setToggleState(inspectorActiveTab == 0, juce::dontSendNotification);
        inspectorTabVelBtn.setToggleState(inspectorActiveTab == 1, juce::dontSendNotification);
        resized();
        repaint();
    };
    addAndMakeVisible(velCurveButton);

    // Inspector Tab Switchers
    inspectorTabZonesBtn.setRadioGroupId(9901);
    inspectorTabZonesBtn.setClickingTogglesState(true);
    inspectorTabZonesBtn.setToggleState(true, juce::dontSendNotification);
    inspectorTabZonesBtn.onClick = [this] {
        inspectorActiveTab = 0;
        inspectorTabZonesBtn.setToggleState(true, juce::dontSendNotification);
        inspectorTabVelBtn.setToggleState(false, juce::dontSendNotification);
        resized();
        repaint();
    };
    addAndMakeVisible(inspectorTabZonesBtn);

    inspectorTabVelBtn.setRadioGroupId(9901);
    inspectorTabVelBtn.setClickingTogglesState(true);
    inspectorTabVelBtn.onClick = [this] {
        inspectorActiveTab = 1;
        inspectorTabZonesBtn.setToggleState(false, juce::dontSendNotification);
        inspectorTabVelBtn.setToggleState(true, juce::dontSendNotification);
        resized();
        repaint();
    };
    addAndMakeVisible(inspectorTabVelBtn);

    // Visual Velocity Curve View
    addAndMakeVisible(velocityCurveView);
    velocityCurveView.onCurveChanged = [this](float newSens, float newCurve, int newMode) {
        if (velocityScopeIsZone && selectedZoneIndex >= 0 && selectedZoneIndex < (int)zones.size())
        {
            zones[selectedZoneIndex].velocitySensitivity = newSens;
        }
        else
        {
            globalVelocitySensitivity = newSens;
            globalVelocityCurve = newCurve;
            globalVelocityCurveMode = newMode;
        }
        velSensitivitySlider.setValue(newSens * 100.0, juce::dontSendNotification);
        velCurveSlider.setValue(newCurve * 100.0, juce::dontSendNotification);
        updateVelocityCurveButtonText();
        if (onStateChanged) onStateChanged();
    };

    velocityCurveView.onVelocityHit = [this](int inV, int outV, float dbGain) {
        velReadoutLabel.setText("In: " + juce::String(inV) + "  →  Out: " + juce::String(outV) +
                                " (" + (dbGain >= 0.0f ? "+" : "") + juce::String(dbGain, 1) + " dB)",
                                juce::dontSendNotification);
    };

    // Preset Buttons
    auto setupPresetBtn = [this](juce::TextButton& btn, int mode, const juce::String& tooltip) {
        btn.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::bgDark);
        btn.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::textPrimary);
        btn.setTooltip(tooltip);
        btn.onClick = [this, mode] { applyVelocityPreset(mode); };
        addAndMakeVisible(btn);
    };
    setupPresetBtn(velLinBtn, 0, "Linear 1:1 dynamic response");
    setupPresetBtn(velSoftBtn, 1, "Soft / Exponential curve (requires harder strike for loud volume)");
    setupPresetBtn(velHardBtn, 2, "Hard / Logarithmic curve (reaches loud volume easily)");
    setupPresetBtn(velSCurveBtn, 3, "S-Curve dynamic transfer response");
    setupPresetBtn(velFixedBtn, 4, "Fixed volume (0% velocity sensitivity / constant output)");

    // Sliders
    velSensitivityLabel.setFont(juce::Font(10.5f));
    velSensitivityLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    addAndMakeVisible(velSensitivityLabel);

    velSensitivitySlider.setSliderStyle(juce::Slider::LinearHorizontal);
    velSensitivitySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 42, 18);
    velSensitivitySlider.setTextValueSuffix("%");
    velSensitivitySlider.setRange(0.0, 100.0, 1.0);
    velSensitivitySlider.setValue(100.0, juce::dontSendNotification);
    velSensitivitySlider.addListener(this);
    addAndMakeVisible(velSensitivitySlider);

    velCurveLabel.setFont(juce::Font(10.5f));
    velCurveLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    addAndMakeVisible(velCurveLabel);

    velCurveSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    velCurveSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 42, 18);
    velCurveSlider.setTextValueSuffix("%");
    velCurveSlider.setRange(-100.0, 100.0, 1.0);
    velCurveSlider.setValue(0.0, juce::dontSendNotification);
    velCurveSlider.addListener(this);
    addAndMakeVisible(velCurveSlider);

    velFloorLabel.setFont(juce::Font(10.5f));
    velFloorLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    addAndMakeVisible(velFloorLabel);

    velFloorSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    velFloorSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 42, 18);
    velFloorSlider.setRange(0.0, 127.0, 1.0);
    velFloorSlider.setValue(0.0, juce::dontSendNotification);
    velFloorSlider.addListener(this);
    addAndMakeVisible(velFloorSlider);

    velScopeButton.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::bgDark);
    velScopeButton.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::accentCyan);
    velScopeButton.setTooltip("Toggle whether velocity sensitivity applies globally or to selected zone");
    velScopeButton.onClick = [this] {
        velocityScopeIsZone = !velocityScopeIsZone;
        velScopeButton.setButtonText(velocityScopeIsZone ? "Scope: Zone" : "Scope: Global");
        updateVelocityCurveUI();
    };
    addAndMakeVisible(velScopeButton);

    velReadoutLabel.setFont(juce::Font(10.0f));
    velReadoutLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    velReadoutLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(velReadoutLabel);

    velTestAuditionBtn.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::accentCyan.withAlpha(0.25f));
    velTestAuditionBtn.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::accentCyan);
    velTestAuditionBtn.setTooltip("Play a test note at velocity 100 to audition the curve response");
    velTestAuditionBtn.onClick = [this] {
        int noteToPlay = (selectedZoneIndex >= 0 && selectedZoneIndex < (int)zones.size()) ? zones[selectedZoneIndex].rootNote : 60;
        triggerKeybedNote(noteToPlay, 100.0f / 127.0f, 100);
    };
    addAndMakeVisible(velTestAuditionBtn);

    activeNoteVelocities.fill(-1);

    lookAndFeelChanged();
}

void SampleMapComponent::updateVelocityCurveButtonText()
{
    int sensPercent = juce::roundToInt(globalVelocitySensitivity * 100.0f);
    juce::String modeName;
    switch (globalVelocityCurveMode)
    {
        case 0: modeName = (std::abs(globalVelocityCurve) < 0.02f) ? "Lin" : (globalVelocityCurve > 0 ? "Exp" : "Log"); break;
        case 1: modeName = "Soft"; break;
        case 2: modeName = "Hard"; break;
        case 3: modeName = "S-Curv"; break;
        case 4: modeName = "Fixed"; break;
        default: modeName = "Custom"; break;
    }

    velCurveButton.setButtonText("Vel: " + juce::String(sensPercent) + "% (" + modeName + ")");
}

void SampleMapComponent::applyVelocityPreset(int mode)
{
    globalVelocityCurveMode = mode;
    velocityCurveView.setCurveMode(mode);

    if (mode == 0) // Linear
    {
        globalVelocityCurve = 0.0f;
        globalVelocitySensitivity = 1.0f;
        globalVelocityMinFloor = 0;
    }
    else if (mode == 1) // Soft (Exp)
    {
        globalVelocityCurve = 0.5f;
        globalVelocitySensitivity = 1.0f;
    }
    else if (mode == 2) // Hard (Log)
    {
        globalVelocityCurve = -0.5f;
        globalVelocitySensitivity = 1.0f;
    }
    else if (mode == 3) // S-Curve
    {
        globalVelocityCurve = 0.0f;
        globalVelocitySensitivity = 1.0f;
    }
    else if (mode == 4) // Fixed (Flat)
    {
        globalVelocityCurve = 0.0f;
        globalVelocitySensitivity = 0.0f;
    }

    velSensitivitySlider.setValue(globalVelocitySensitivity * 100.0, juce::dontSendNotification);
    velCurveSlider.setValue(globalVelocityCurve * 100.0, juce::dontSendNotification);
    velFloorSlider.setValue(globalVelocityMinFloor, juce::dontSendNotification);

    velocityCurveView.setSensitivity(globalVelocitySensitivity);
    velocityCurveView.setCurve(globalVelocityCurve);
    velocityCurveView.setMinFloor(globalVelocityMinFloor);

    updateVelocityCurveButtonText();
    if (onStateChanged) onStateChanged();
    repaint();
}

void SampleMapComponent::updateVelocityCurveUI()
{
    float currentSens = globalVelocitySensitivity;
    if (velocityScopeIsZone && selectedZoneIndex >= 0 && selectedZoneIndex < static_cast<int>(zones.size()))
    {
        currentSens = zones[selectedZoneIndex].velocitySensitivity;
        velScopeButton.setButtonText("Scope: Zone " + juce::String(selectedZoneIndex + 1));
    }
    else if (velocityScopeIsZone)
    {
        velScopeButton.setButtonText("Scope: Zone");
    }
    else
    {
        velScopeButton.setButtonText("Scope: Global");
    }

    velSensitivitySlider.setValue(currentSens * 100.0, juce::dontSendNotification);
    velCurveSlider.setValue(globalVelocityCurve * 100.0, juce::dontSendNotification);
    velFloorSlider.setValue(globalVelocityMinFloor, juce::dontSendNotification);

    velocityCurveView.setSensitivity(currentSens);
    velocityCurveView.setCurve(globalVelocityCurve);
    velocityCurveView.setCurveMode(globalVelocityCurveMode);
    velocityCurveView.setMinFloor(globalVelocityMinFloor);

    updateVelocityCurveButtonText();
}

float SampleMapComponent::computeEffectiveVelocity(float inVelocity, const SampleMapZone* zone) const
{
    float zoneSens = -1.0f;
    if (zone != nullptr)
        zoneSens = zone->velocitySensitivity;

    SampleMapState s;
    s.velocitySensitivity = globalVelocitySensitivity;
    s.velocityCurve = globalVelocityCurve;
    s.velocityCurveMode = globalVelocityCurveMode;
    s.velocityMinFloor = globalVelocityMinFloor;
    return s.computeVelocityResponse(inVelocity, zoneSens);
}

void SampleMapComponent::lookAndFeelChanged()
{
    // Sliders & buttons inherit LookAndFeel automatically
}

SampleMapComponent::~SampleMapComponent()
{
    stopTimer();
    audioEngine.getKeyboardState().removeListener(this);
    audioEngine.removeListener(this);
}

void SampleMapComponent::sampleLoaded(const juce::String& /*filePath*/)
{
    juce::MessageManager::callAsync([this] {
        resized();
        repaint();
    });
}

void SampleMapComponent::pitchTrackingStateChanged(bool enabled)
{
    juce::MessageManager::callAsync([this, enabled] {
        pitchTrackButton.setToggleState(enabled, juce::dontSendNotification);
        pitchTrackButton.setButtonText(enabled ? "Pitch Track: ON" : "Pitch Track: OFF");
    });
}

void SampleMapComponent::oneShotStateChanged(bool enabled)
{
    juce::MessageManager::callAsync([this, enabled] {
        oneShotButton.setToggleState(enabled, juce::dontSendNotification);
        oneShotButton.setButtonText(enabled ? "One Shot: ON" : "One Shot: OFF");
    });
}

void SampleMapComponent::loopingStateChanged(bool enabled)
{
    juce::MessageManager::callAsync([this, enabled] {
        loopButton.setToggleState(enabled, juce::dontSendNotification);
        loopButton.setButtonText(enabled ? "Loop: ON" : "Loop: OFF");
    });
}

void SampleMapComponent::updateMidiChannelButtonText()
{
    if (midiChannelSetting == 0)
        midiChannelButton.setButtonText("MIDI: Omni");
    else
        midiChannelButton.setButtonText("MIDI Ch: " + juce::String(midiChannelSetting));
}

void SampleMapComponent::handleNoteOn(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity)
{
    int targetCh = midiChannelSetting;
    if (targetCh > 0 && midiChannel > 0 && midiChannel != targetCh)
        return;

    if (midiNoteNumber >= 0 && midiNoteNumber < 128)
    {
        int velInt = juce::jlimit(0, 127, static_cast<int>(velocity * 127.0f));

        int matchingZoneIdx = -1;
        juce::File fileToLoad;
        for (size_t i = 0; i < zones.size(); ++i)
        {
            const auto& z = zones[i];
            if (midiNoteNumber >= z.keyLow && midiNoteNumber <= z.keyHigh && velInt >= z.velLow && velInt <= z.velHigh && z.filePath.isNotEmpty())
            {
                matchingZoneIdx = static_cast<int>(i);
                fileToLoad = juce::File(z.filePath);
                break;
            }
        }

        // If sample map has zones, only respond to mapped areas at this velocity!
        if (!zones.empty() && matchingZoneIdx < 0)
            return;

        activeMidiNotes[static_cast<size_t>(midiNoteNumber)] = true;
        activeNoteVelocities[static_cast<size_t>(midiNoteNumber)] = velInt;

        uint32_t now = juce::Time::getMillisecondCounter();
        recentHitDots.erase(
            std::remove_if(recentHitDots.begin(), recentHitDots.end(), [midiNoteNumber](const HitDot& d) {
                return d.note == midiNoteNumber;
            }),
            recentHitDots.end()
        );
        recentHitDots.push_back({ midiNoteNumber, velInt, now });
        velocityCurveView.triggerHit(velInt);

        if (matchingZoneIdx >= 0)
        {
            selectedZoneIndex = matchingZoneIdx;
            selectedZoneIndices.clear();
            selectedZoneIndices.insert(matchingZoneIdx);
        }

        juce::MessageManager::callAsync([this, fileToLoad] {
            if (!isTimerRunning())
                startTimerHz(30);

            if (fileToLoad.existsAsFile())
            {
                if (audioEngine.getCurrentFile() != fileToLoad)
                {
                    audioEngine.loadFile(fileToLoad, false, true);
                }
            }
            resized();
            repaint();
        });
    }
}

void SampleMapComponent::handleNoteOff(juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float /*velocity*/)
{
    int targetCh = midiChannelSetting;
    if (targetCh > 0 && midiChannel > 0 && midiChannel != targetCh)
        return;

    if (midiNoteNumber >= 0 && midiNoteNumber < 128)
    {
        activeMidiNotes[static_cast<size_t>(midiNoteNumber)] = false;
        activeNoteVelocities[static_cast<size_t>(midiNoteNumber)] = -1;
        juce::MessageManager::callAsync([this] {
            if (!isTimerRunning())
                startTimerHz(30);
            repaint();
        });
    }
}

void SampleMapComponent::timerCallback()
{
    uint32_t now = juce::Time::getMillisecondCounter();
    bool hasHeld = false;
    for (int note = 0; note < 128; ++note)
    {
        if (activeNoteVelocities[static_cast<size_t>(note)] >= 0)
        {
            hasHeld = true;
            break;
        }
    }

    recentHitDots.erase(
        std::remove_if(recentHitDots.begin(), recentHitDots.end(), [now, this](const HitDot& dot) {
            bool isHeld = (dot.note >= 0 && dot.note < 128 && activeNoteVelocities[static_cast<size_t>(dot.note)] >= 0);
            return !isHeld && (now - dot.timestampMs > 1200);
        }),
        recentHitDots.end()
    );

    velocityCurveView.updateHitFade();

    if (recentHitDots.empty() && !hasHeld)
    {
        stopTimer();
    }
    repaint();
}

bool SampleMapComponent::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& f : files)
    {
        juce::File file(f);
        if (file.hasFileExtension("wav") || file.hasFileExtension("mp3") ||
            file.hasFileExtension("flac") || file.hasFileExtension("aiff") ||
            file.hasFileExtension("aif") || file.hasFileExtension("aifc") ||
            file.hasFileExtension("ogg") || file.hasFileExtension("xml") ||
            file.hasFileExtension("samplemap") || file.hasFileExtension("owmap") ||
            file.hasFileExtension("zip"))
            return true;
    }
    return false;
}

void SampleMapComponent::filesDropped(const juce::StringArray& files, int /*x*/, int /*y*/)
{
    for (const auto& f : files)
    {
        juce::File file(f);
        if (file.existsAsFile())
        {
            if (file.hasFileExtension("xml") || file.hasFileExtension("samplemap") || file.hasFileExtension("owmap") || file.hasFileExtension("zip"))
            {
                if (loadSampleMapFile(file))
                    return;
            }
            addSampleFile(file);
        }
    }
}

bool SampleMapComponent::isInterestedInDragSource(const juce::DragAndDropTarget::SourceDetails& /*dragSourceDetails*/)
{
    return true;
}

void SampleMapComponent::itemDropped(const juce::DragAndDropTarget::SourceDetails& dragSourceDetails)
{
    juce::String description = dragSourceDetails.description.toString();
    if (description.isNotEmpty())
    {
        juce::File file(description);
        if (file.existsAsFile())
        {
            if (file.hasFileExtension("xml") || file.hasFileExtension("samplemap") || file.hasFileExtension("owmap") || file.hasFileExtension("zip"))
            {
                if (loadSampleMapFile(file))
                    return;
            }
            addSampleFile(file);
        }
    }
}

juce::Rectangle<float> SampleMapComponent::getGridBounds() const
{
    auto area = getLocalBounds().reduced(20, 16);
    area.removeFromTop(44);     // Top toolbar gap
    area.removeFromBottom(50);  // Keybed height
    int inspectorW = 240;
    area.removeFromRight(inspectorW + 16); // Inspector sidebar
    return area.toFloat();
}

juce::Rectangle<float> SampleMapComponent::getKeybedBounds() const
{
    auto grid = getGridBounds();
    return juce::Rectangle<float>(grid.getX(), grid.getBottom() + 4.0f, grid.getWidth(), 44.0f);
}

juce::Rectangle<float> SampleMapComponent::getInspectorBounds() const
{
    auto area = getLocalBounds().reduced(20, 16);
    area.removeFromTop(44);
    int inspectorW = 240;
    return area.removeFromRight(inspectorW).toFloat();
}

int SampleMapComponent::noteNumberAtX(float x, juce::Rectangle<float> gridArea) const
{
    float relX = (x - gridArea.getX()) / gridArea.getWidth();
    int note = juce::jlimit(0, 127, static_cast<int>(relX * 128.0f));
    return note;
}

float SampleMapComponent::xForNoteNumber(int noteNum, juce::Rectangle<float> gridArea) const
{
    float norm = static_cast<float>(juce::jlimit(0, 127, noteNum)) / 128.0f;
    return gridArea.getX() + norm * gridArea.getWidth();
}

int SampleMapComponent::velocityAtY(float y, juce::Rectangle<float> gridArea) const
{
    float relY = 1.0f - (y - gridArea.getY()) / gridArea.getHeight();
    return juce::jlimit(0, 127, static_cast<int>(relY * 128.0f));
}

float SampleMapComponent::yForVelocity(int vel, juce::Rectangle<float> gridArea) const
{
    float norm = 1.0f - static_cast<float>(juce::jlimit(0, 127, vel)) / 128.0f;
    return gridArea.getY() + norm * gridArea.getHeight();
}

juce::String SampleMapComponent::midiNoteToName(int noteNum) const
{
    static const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    int octave = (noteNum / 12) - 1;
    int nameIdx = noteNum % 12;
    return juce::String(noteNames[nameIdx]) + juce::String(octave);
}

int SampleMapComponent::parseRootNoteFromFilename(const juce::String& filename) const
{
    try
    {
        // Search for note patterns e.g. C3, F#2, Db4, A-1
        std::string name = filename.toStdString();
        std::regex noteRegex("([A-Ga-g])(#|b|s)?(-?[0-9]+)");
        std::smatch match;

        if (std::regex_search(name, match, noteRegex))
        {
            std::string noteStr = match[1].str();
            std::string accStr = match[2].str();
            std::string octStr = match[3].str();

            char noteChar = static_cast<char>(std::toupper(static_cast<unsigned char>(noteStr[0])));
            int baseNote = 0;
            switch (noteChar)
            {
                case 'C': baseNote = 0; break;
                case 'D': baseNote = 2; break;
                case 'E': baseNote = 4; break;
                case 'F': baseNote = 5; break;
                case 'G': baseNote = 7; break;
                case 'A': baseNote = 9; break;
                case 'B': baseNote = 11; break;
            }

            if (accStr == "#" || accStr == "s") baseNote += 1;
            else if (accStr == "b") baseNote -= 1;

            int octave = std::stoi(octStr);
            int midiNote = (octave + 1) * 12 + baseNote;
            return juce::jlimit(0, 127, midiNote);
        }
    }
    catch (...)
    {
    }

    return 60; // Default C4
}

static int parseRoundRobinFromFilename(const juce::String& filename)
{
    try
    {
        std::string name = filename.toStdString();
        std::regex rrRegex("(?:_|-|\\s)(?:rr|take|r)([1-9][0-9]?)", std::regex_constants::icase);
        std::smatch match;
        if (std::regex_search(name, match, rrRegex))
        {
            return std::stoi(match[1].str());
        }
    }
    catch (...)
    {
    }
    return 1;
}

// ─────────────────────────────────────────────────────────
//  Zone Actions & Operations
// ─────────────────────────────────────────────────────────
void SampleMapComponent::addSample(const MediaItem& item)
{
    SampleMapZone z;
    z.filePath = item.filePath;
    z.sampleName = item.fileName;
    z.rootNote = parseRootNoteFromFilename(item.fileName);
    z.keyLow = z.rootNote;
    z.keyHigh = z.rootNote;
    z.velLow = 0;
    z.velHigh = 127;
    z.roundRobinIndex = parseRoundRobinFromFilename(item.fileName);

    zones.push_back(z);
    selectedZoneIndex = static_cast<int>(zones.size()) - 1;
    selectedZoneIndices.clear();
    selectedZoneIndices.insert(selectedZoneIndex);

    juce::File itemFile(item.filePath);
    if (itemFile.existsAsFile())
    {
        audioEngine.loadFile(itemFile, false, true);
    }
    resized();
    repaint();
    if (onStateChanged) onStateChanged();
}

void SampleMapComponent::addSampleFile(const juce::File& file)
{
    if (!file.existsAsFile()) return;

    SampleMapZone z;
    z.filePath = file.getFullPathName();
    z.sampleName = file.getFileName();
    z.rootNote = parseRootNoteFromFilename(file.getFileName());
    z.keyLow = z.rootNote;
    z.keyHigh = z.rootNote;
    z.velLow = 0;
    z.velHigh = 127;
    z.roundRobinIndex = parseRoundRobinFromFilename(file.getFileName());

    zones.push_back(z);
    selectedZoneIndex = static_cast<int>(zones.size()) - 1;
    selectedZoneIndices.clear();
    selectedZoneIndices.insert(selectedZoneIndex);

    audioEngine.loadFile(file, false, true);
    resized();
    repaint();
    if (onStateChanged) onStateChanged();
}

void SampleMapComponent::sliceFileToZones(const juce::File& audioFile, const juce::AudioBuffer<float>& buffer, double sampleRate, const std::vector<double>& sliceRatios)
{
    clearAllZones();

    int numChannels = buffer.getNumChannels();
    int numSamples = buffer.getNumSamples();
    if (numSamples <= 0 || numChannels <= 0 || sampleRate <= 0.0) return;

    int numSlices = static_cast<int>(sliceRatios.size());

    juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("OWMB_Temp");
    tempDir.createDirectory();

    static int sliceCounter = 0;
    auto timestamp = juce::Time::currentTimeMillis();
    juce::String baseName = audioFile.getFileNameWithoutExtension();

    for (int i = 0; i < numSlices; ++i)
    {
        double startR = sliceRatios[i];
        double endR = (i + 1 < numSlices) ? sliceRatios[i + 1] : 1.0;

        int startSample = static_cast<int>(startR * numSamples);
        int endSample = static_cast<int>(endR * numSamples);
        int sliceLen = endSample - startSample;
        if (sliceLen <= 0) continue;

        juce::File sliceFile = tempDir.getChildFile(baseName + "_Slice_" + juce::String(i + 1) + "_" + juce::String(timestamp) + "_" + juce::String(++sliceCounter) + ".wav");
        sliceFile.deleteFile();

        auto* rawStream = sliceFile.createOutputStream().release();
        if (rawStream != nullptr)
        {
            juce::WavAudioFormat wavFormat;
            std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(rawStream, sampleRate, numChannels, 16, {}, 0));
            if (writer != nullptr)
            {
                writer->writeFromAudioSampleBuffer(buffer, startSample, sliceLen);
                writer.reset(); // Flush and close file handle immediately
            }
            else
            {
                delete rawStream;
            }
        }

        if (sliceFile.existsAsFile() && sliceFile.getSize() > 44)
        {
            juce::AudioBuffer<float> sliceBuf(numChannels, sliceLen);
            for (int ch = 0; ch < numChannels; ++ch)
                sliceBuf.copyFrom(ch, 0, buffer, ch, startSample, sliceLen);

            audioEngine.putSampleInCache(sliceFile.getFullPathName(), sampleRate, sliceBuf);

            SampleMapZone z;
            z.filePath = sliceFile.getFullPathName();
            z.sampleName = sliceFile.getFileName();

            int mappedKey = juce::jmin(127, 36 + i);
            z.rootNote = mappedKey;
            z.keyLow = mappedKey;
            z.keyHigh = mappedKey;
            z.velLow = 0;
            z.velHigh = 127;
            z.fineTuneCents = 0.0f;
            z.gainDb = 0.0f;

            zones.push_back(z);
        }
    }

    if (!zones.empty())
    {
        selectedZoneIndex = 0;
        selectedZoneIndices.insert(0);
        juce::File firstSlice(zones[0].filePath);
        if (firstSlice.existsAsFile())
        {
            audioEngine.loadFile(firstSlice, false, true);
        }
    }

    resized();
    repaint();
    if (onStateChanged)
        onStateChanged();
}

void SampleMapComponent::sliceLoadedSample(const std::vector<double>& sliceRatios)
{
    juce::AudioBuffer<float> buffer;
    double sampleRate = 44100.0;
    if (!audioEngine.getAudioBufferCopy(buffer, sampleRate))
        return;

    juce::File currentFile = audioEngine.getCurrentFile();
    if (!currentFile.existsAsFile())
    {
        juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("OWMB_Temp");
        tempDir.createDirectory();
        currentFile = tempDir.getChildFile("LoadedSample.wav");
    }

    sliceFileToZones(currentFile, buffer, sampleRate, sliceRatios);
}

void SampleMapComponent::autoSliceToSampler(const MediaItem& item, const std::vector<double>& customSliceRatios)
{
    try
    {
        if (onSliceToSamplerStarted)
            onSliceToSamplerStarted();

        juce::File audioFile(item.filePath);
        if (!audioFile.existsAsFile()) return;

        std::unique_ptr<juce::AudioFormatReader> reader(audioEngine.getFormatManager().createReaderFor(audioFile));
        if (reader == nullptr) return;

        int numChannels = static_cast<int>(reader->numChannels);
        int numSamples = static_cast<int>(reader->lengthInSamples);
        double sampleRate = reader->sampleRate;

        if (numSamples <= 0 || numChannels <= 0 || sampleRate <= 0.0) return;
        if (numSamples > 192000 * 600) return;

        juce::AudioBuffer<float> buffer(numChannels, numSamples);
        reader->read(&buffer, 0, numSamples, 0, true, true);

        if (!customSliceRatios.empty())
        {
            sliceFileToZones(audioFile, buffer, sampleRate, customSliceRatios);
            return;
        }

        int blockSize = 256;
        int numBlocks = numSamples / blockSize;

        std::vector<double> sliceRatios;
        sliceRatios.push_back(0.0);

        if (numBlocks > 4)
        {
            std::vector<float> energy(numBlocks, 0.0f);
            for (int b = 0; b < numBlocks; ++b)
            {
                float sum = 0.0f;
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    for (int s = 0; s < blockSize; ++s)
                    {
                        int idx = b * blockSize + s;
                        if (idx < numSamples)
                        {
                            float val = buffer.getSample(ch, idx);
                            sum += val * val;
                        }
                    }
                }
                energy[b] = std::sqrt(sum / static_cast<float>(blockSize * numChannels));
            }

            std::vector<float> onset(numBlocks, 0.0f);
            float sumOnset = 0.0f;
            for (int b = 1; b < numBlocks; ++b)
            {
                float diff = energy[b] - energy[b - 1];
                if (diff > 0.0f)
                {
                    onset[b] = diff;
                    sumOnset += diff;
                }
            }

            float meanOnset = sumOnset / static_cast<float>(numBlocks);
            float sqDiffSum = 0.0f;
            for (int b = 1; b < numBlocks; ++b)
            {
                float diff = onset[b] - meanOnset;
                sqDiffSum += diff * diff;
            }
            float stdOnset = std::sqrt(sqDiffSum / static_cast<float>(numBlocks));

            float threshold = std::max(0.005f, meanOnset + 0.35f * stdOnset);
            int minDistanceBlocks = static_cast<int>(0.05 * sampleRate / static_cast<double>(blockSize));
            if (minDistanceBlocks < 1) minDistanceBlocks = 1;
            int lastOnsetBlock = -minDistanceBlocks;

            struct OnsetCandidate
            {
                int blockIdx { 0 };
                float strength { 0.0f };
            };
            std::vector<OnsetCandidate> candidates;

            for (int b = 1; b < numBlocks - 1; ++b)
            {
                if (onset[b] > threshold && onset[b] >= onset[b - 1] && onset[b] >= onset[b + 1])
                {
                    if (b - lastOnsetBlock >= minDistanceBlocks)
                    {
                        candidates.push_back({ b, onset[b] });
                        lastOnsetBlock = b;
                    }
                }
            }

            const size_t maxOnsets = 39; // 39 onsets + 0.0 start = max 40 slices
            if (candidates.size() > maxOnsets)
            {
                std::sort(candidates.begin(), candidates.end(), [](const OnsetCandidate& a, const OnsetCandidate& b) {
                    return a.strength > b.strength;
                });
                candidates.resize(maxOnsets);
                std::sort(candidates.begin(), candidates.end(), [](const OnsetCandidate& a, const OnsetCandidate& b) {
                    return a.blockIdx < b.blockIdx;
                });
            }

            for (const auto& c : candidates)
            {
                double ratio = static_cast<double>(c.blockIdx * blockSize) / static_cast<double>(numSamples);
                sliceRatios.push_back(ratio);
            }
        }

        if (sliceRatios.size() <= 1)
        {
            sliceRatios.clear();
            for (int i = 0; i < 8; ++i)
            {
                sliceRatios.push_back(static_cast<double>(i) / 8.0);
            }
        }

        std::sort(sliceRatios.begin(), sliceRatios.end());
        sliceRatios.erase(std::unique(sliceRatios.begin(), sliceRatios.end()), sliceRatios.end());

        sliceFileToZones(audioFile, buffer, sampleRate, sliceRatios);
    }
    catch (...)
    {
    }
}

void SampleMapComponent::selectZone(int index, bool addToSelection)
{
    if (!addToSelection)
    {
        selectedZoneIndices.clear();
    }
    if (index >= 0 && index < static_cast<int>(zones.size()))
    {
        selectedZoneIndices.insert(index);
        selectedZoneIndex = index;

        const auto& z = zones[index];
        juce::File f(z.filePath);
        if (f.existsAsFile())
        {
            audioEngine.loadFile(f, false, true);
        }

        attackKnob.setValue(z.attackMs, juce::dontSendNotification);
        decayKnob.setValue(z.decayMs, juce::dontSendNotification);
        sustainKnob.setValue(z.sustainLevel, juce::dontSendNotification);
        releaseKnob.setValue(z.releaseMs, juce::dontSendNotification);
    }
    else
    {
        selectedZoneIndex = -1;
    }
    updateVelocityCurveUI();
    resized();
    repaint();
}

void SampleMapComponent::deselectAllZones()
{
    selectedZoneIndices.clear();
    selectedZoneIndex = -1;
    updateVelocityCurveUI();
    resized();
    repaint();
}

void SampleMapComponent::deleteSelectedZones()
{
    if (selectedZoneIndices.empty())
    {
        if (selectedZoneIndex >= 0 && selectedZoneIndex < static_cast<int>(zones.size()))
        {
            selectedZoneIndices.insert(selectedZoneIndex);
        }
        else
        {
            return;
        }
    }

    std::vector<SampleMapZone> remainingZones;
    for (int i = 0; i < static_cast<int>(zones.size()); ++i)
    {
        if (selectedZoneIndices.count(i) == 0)
        {
            remainingZones.push_back(zones[i]);
        }
    }

    zones = std::move(remainingZones);
    selectedZoneIndices.clear();

    if (zones.empty())
    {
        selectedZoneIndex = -1;
        audioEngine.clearMasterSample();
    }
    else
    {
        selectedZoneIndex = juce::jlimit(0, static_cast<int>(zones.size()) - 1, selectedZoneIndex);
        selectedZoneIndices.insert(selectedZoneIndex);
        juce::File f(zones[selectedZoneIndex].filePath);
        if (f.existsAsFile())
        {
            audioEngine.loadFile(f, false, true);
        }
    }

    resized();
    repaint();
    if (onStateChanged) onStateChanged();
}

void SampleMapComponent::clearAllZones()
{
    zones.clear();
    selectedZoneIndex = -1;
    selectedZoneIndices.clear();
    audioEngine.clearMasterSample();
    resized();
    repaint();
    if (onStateChanged) onStateChanged();
}

void SampleMapComponent::autoMapByPitch()
{
    if (zones.empty()) return;

    // Sort zones by root note
    std::sort(zones.begin(), zones.end(), [](const SampleMapZone& a, const SampleMapZone& b) {
        return a.rootNote < b.rootNote;
    });

    int numZones = static_cast<int>(zones.size());
    for (int i = 0; i < numZones; ++i)
    {
        int prevRoot = (i > 0) ? zones[i - 1].rootNote : 0;
        int nextRoot = (i + 1 < numZones) ? zones[i + 1].rootNote : 127;

        zones[i].keyLow = (i == 0) ? 0 : (zones[i].rootNote + prevRoot) / 2;
        zones[i].keyHigh = (i == numZones - 1) ? 127 : (zones[i].rootNote + nextRoot) / 2 - 1;
        zones[i].keyHigh = juce::jmax(zones[i].keyLow, zones[i].keyHigh);
    }

    if (selectedZoneIndex >= 0 && selectedZoneIndex < static_cast<int>(zones.size()))
    {
        audioEngine.getKeyboardState().noteOn(1, zones[selectedZoneIndex].rootNote, 0.8f);
    }
    else if (!zones.empty())
    {
        selectedZoneIndex = 0;
        selectedZoneIndices.clear();
        selectedZoneIndices.insert(0);
        audioEngine.getKeyboardState().noteOn(1, zones[0].rootNote, 0.8f);
    }

    resized();
    repaint();
    if (onStateChanged) onStateChanged();
}

void SampleMapComponent::autoMapChromatic()
{
    if (zones.empty()) return;

    int currentKey = 36; // C2
    for (auto& z : zones)
    {
        z.rootNote = currentKey;
        z.keyLow = currentKey;
        z.keyHigh = currentKey;
        z.velLow = 0;
        z.velHigh = 127;
        currentKey = juce::jmin(127, currentKey + 1);
    }

    if (selectedZoneIndex >= 0 && selectedZoneIndex < static_cast<int>(zones.size()))
    {
        audioEngine.getKeyboardState().noteOn(1, zones[selectedZoneIndex].rootNote, 0.8f);
    }
    else if (!zones.empty())
    {
        selectedZoneIndex = 0;
        selectedZoneIndices.clear();
        selectedZoneIndices.insert(0);
        audioEngine.getKeyboardState().noteOn(1, zones[0].rootNote, 0.8f);
    }

    resized();
    repaint();
    if (onStateChanged) onStateChanged();
}

void SampleMapComponent::autoMapVelocityLayers()
{
    if (zones.empty()) return;

    // Group zones that share the same key span or rootNote
    std::map<std::pair<int, int>, std::vector<int>> keyGroups;
    for (size_t i = 0; i < zones.size(); ++i)
    {
        keyGroups[{ zones[i].keyLow, zones[i].keyHigh }].push_back(static_cast<int>(i));
    }

    for (const auto& [keyRange, indices] : keyGroups)
    {
        if (indices.size() <= 1) continue;

        int numLayers = static_cast<int>(indices.size());
        int velStep = 128 / numLayers;
        for (int l = 0; l < numLayers; ++l)
        {
            int idx = indices[static_cast<size_t>(l)];
            zones[idx].velLow = l * velStep;
            zones[idx].velHigh = (l == numLayers - 1) ? 127 : ((l + 1) * velStep - 1);
        }
    }

    resized();
    repaint();
    if (onStateChanged) onStateChanged();
}

void SampleMapComponent::autoMapRoundRobin()
{
    if (zones.empty()) return;

    // Group zones that share the same key span (keyLow, keyHigh)
    std::map<std::pair<int, int>, std::vector<int>> keyGroups;
    for (size_t i = 0; i < zones.size(); ++i)
    {
        keyGroups[{ zones[i].keyLow, zones[i].keyHigh }].push_back(static_cast<int>(i));
    }

    for (const auto& [keyRange, indices] : keyGroups)
    {
        // Further sub-group by velocity tier
        std::map<std::pair<int, int>, std::vector<int>> velGroups;
        for (int idx : indices)
        {
            velGroups[{ zones[idx].velLow, zones[idx].velHigh }].push_back(idx);
        }

        for (const auto& [velRange, rrIndices] : velGroups)
        {
            for (size_t rr = 0; rr < rrIndices.size(); ++rr)
            {
                zones[rrIndices[rr]].roundRobinIndex = static_cast<int>(rr + 1);
            }
        }
    }

    resized();
    repaint();
    if (onStateChanged) onStateChanged();
}

void SampleMapComponent::saveSampleMapToFile()
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Save Sample Map",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("MySampleMap.xml"),
        "*.xml;*.samplemap;*.owmap");

    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                         [this, chooser](const juce::FileChooser& fc) {
        auto result = fc.getResult();
        if (result == juce::File())
            return;

        if (result.getFileExtension().isEmpty())
            result = result.withFileExtension("xml");

        auto currentState = getState();
        currentState.saveToFile(result);
    });
}

void SampleMapComponent::loadSampleMapFromFile()
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Load Sample Map",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.xml;*.samplemap;*.owmap;*.zip");

    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                         [this, chooser](const juce::FileChooser& fc) {
        auto result = fc.getResult();
        if (result.existsAsFile())
        {
            loadSampleMapFile(result);
        }
    });
}

bool SampleMapComponent::loadSampleMapFile(const juce::File& file)
{
    if (!file.existsAsFile())
        return false;

    if (file.hasFileExtension("zip"))
    {
        juce::File importBaseDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                                        .getChildFile("OpenWav")
                                        .getChildFile("ImportedMaps")
                                        .getChildFile(file.getFileNameWithoutExtension());
        importBaseDir.createDirectory();

        juce::ZipFile zip(file);
        if (zip.getNumEntries() == 0)
            return false;

        for (int i = 0; i < zip.getNumEntries(); ++i)
        {
            zip.uncompressEntry(i, importBaseDir, true);
        }

        juce::Array<juce::File> xmlFiles;
        importBaseDir.findChildFiles(xmlFiles, juce::File::findFiles, true, "*.xml;*.samplemap;*.owmap");
        if (xmlFiles.isEmpty())
            return false;

        juce::File chosenXml = xmlFiles.getFirst();
        for (const auto& xf : xmlFiles)
        {
            if (xf.getFileName().equalsIgnoreCase("SampleMap.xml") ||
                xf.getFileNameWithoutExtension().equalsIgnoreCase(file.getFileNameWithoutExtension()))
            {
                chosenXml = xf;
                break;
            }
        }

        return loadSampleMapFile(chosenXml);
    }

    auto newState = SampleMapState::loadFromFile(file);
    if (newState.zones.empty() && !file.loadFileAsString().containsIgnoreCase("<SampleMap"))
        return false;

    setState(newState);
    if (onStateChanged)
        onStateChanged();
    return true;
}

void SampleMapComponent::exportSampleMapToZip()
{
    if (zones.empty())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Export Sample Map",
            "There are no sample zones mapped to export. Please map or slice some samples first.");
        return;
    }

    juce::String defaultBundleName = "MySampleMapBundle.zip";
    if (!zones.empty() && zones[0].sampleName.isNotEmpty())
    {
        auto base = juce::File::createLegalFileName(juce::File(zones[0].sampleName).getFileNameWithoutExtension());
        if (base.isNotEmpty())
            defaultBundleName = base + "_Bundle.zip";
    }

    auto chooser = std::make_shared<juce::FileChooser>(
        "Export Sample Map Bundle (.zip)",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile(defaultBundleName),
        "*.zip");

    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                         [this, chooser](const juce::FileChooser& fc) {
        auto targetZip = fc.getResult();
        if (targetZip == juce::File())
            return;

        if (!targetZip.hasFileExtension("zip"))
            targetZip = targetZip.withFileExtension("zip");

        auto capturedZones = this->zones;
        auto capturedState = getState();
        AudioEngine* enginePtr = &this->audioEngine;
        juce::Component::SafePointer<SampleMapComponent> safeThis(this);

        juce::Thread::launch([safeThis, enginePtr, targetZip, capturedZones, capturedState]() mutable {
            juce::File tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                     .getChildFile("OWMB_ZipExport_" + juce::String(juce::Time::currentTimeMillis()));
            tempDir.createDirectory();
            juce::File samplesDir = tempDir.getChildFile("Samples");
            samplesDir.createDirectory();

            SampleMapState exportState = capturedState;
            exportState.zones.clear();

            std::set<juce::String> usedNames;

            for (size_t i = 0; i < capturedZones.size(); ++i)
            {
                const auto& z = capturedZones[i];

                juce::String rawName = juce::File(z.filePath).getFileName();
                if (rawName.isEmpty())
                    rawName = z.sampleName;
                if (rawName.isEmpty())
                    rawName = "Zone_" + juce::String(i + 1);

                if (!rawName.endsWithIgnoreCase(".wav"))
                {
                    rawName = juce::File(rawName).getFileNameWithoutExtension() + ".wav";
                }
                rawName = juce::File::createLegalFileName(rawName);

                juce::String uniqueName = rawName;
                int dup = 1;
                while (usedNames.count(uniqueName.toLowerCase()) > 0)
                {
                    uniqueName = juce::File(rawName).getFileNameWithoutExtension() + "_" + juce::String(++dup) + ".wav";
                }
                usedNames.insert(uniqueName.toLowerCase());

                juce::File targetWav = samplesDir.getChildFile(uniqueName);

                // Retrieve audio data (from engine cache, disk file, or loaded master buffer)
                juce::AudioBuffer<float> zoneBuffer;
                double sampleRate = 44100.0;
                bool gotBuffer = enginePtr->getCachedSampleCopy(z.filePath, zoneBuffer, sampleRate);

                if (!gotBuffer)
                {
                    juce::File srcFile(z.filePath);
                    if (srcFile.existsAsFile())
                    {
                        std::unique_ptr<juce::AudioFormatReader> reader(enginePtr->getFormatManager().createReaderFor(srcFile));
                        if (reader != nullptr && reader->lengthInSamples > 0 && reader->numChannels > 0)
                        {
                            sampleRate = (reader->sampleRate > 0.0) ? reader->sampleRate : 44100.0;
                            zoneBuffer.setSize(static_cast<int>(reader->numChannels), static_cast<int>(reader->lengthInSamples));
                            reader->read(&zoneBuffer, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);
                            gotBuffer = true;
                        }
                    }
                }

                if (!gotBuffer && enginePtr->getCurrentFile().getFullPathName() == z.filePath)
                {
                    gotBuffer = enginePtr->getAudioBufferCopy(zoneBuffer, sampleRate);
                }

                if (gotBuffer && zoneBuffer.getNumSamples() > 0)
                {
                    targetWav.deleteFile();
                    auto* outStream = targetWav.createOutputStream().release();
                    if (outStream != nullptr)
                    {
                        juce::WavAudioFormat wavFormat;
                        std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(outStream, sampleRate, zoneBuffer.getNumChannels(), 24, {}, 0));
                        if (writer != nullptr)
                        {
                            writer->writeFromAudioSampleBuffer(zoneBuffer, 0, zoneBuffer.getNumSamples());
                        }
                    }
                }
                else if (juce::File(z.filePath).existsAsFile())
                {
                    juce::File(z.filePath).copyFileTo(targetWav);
                }

                SampleMapZoneState zoneState;
                zoneState.filePath = "Samples/" + uniqueName;
                zoneState.sampleName = uniqueName;
                zoneState.rootNote = z.rootNote;
                zoneState.keyLow = z.keyLow;
                zoneState.keyHigh = z.keyHigh;
                zoneState.velLow = z.velLow;
                zoneState.velHigh = z.velHigh;
                zoneState.roundRobinIndex = z.roundRobinIndex;
                zoneState.fineTuneCents = z.fineTuneCents;
                zoneState.gainDb = z.gainDb;
                zoneState.attackMs = z.attackMs;
                zoneState.decayMs = z.decayMs;
                zoneState.sustainLevel = z.sustainLevel;
                zoneState.releaseMs = z.releaseMs;
                exportState.zones.push_back(zoneState);
            }

            juce::File xmlFile = tempDir.getChildFile("SampleMap.xml");
            exportState.saveToFile(xmlFile);

            // Also create a named xml corresponding to the zip name
            juce::String bundleBase = targetZip.getFileNameWithoutExtension();
            if (bundleBase != "SampleMap")
            {
                juce::File namedXml = tempDir.getChildFile(bundleBase + ".xml");
                exportState.saveToFile(namedXml);
            }

            // Build the zip archive
            juce::ZipFile::Builder zipBuilder;
            zipBuilder.addFile(xmlFile, 9, "SampleMap.xml");
            if (bundleBase != "SampleMap")
            {
                juce::File namedXml = tempDir.getChildFile(bundleBase + ".xml");
                if (namedXml.existsAsFile())
                    zipBuilder.addFile(namedXml, 9, bundleBase + ".xml");
            }

            juce::Array<juce::File> sampleFiles;
            samplesDir.findChildFiles(sampleFiles, juce::File::findFiles, false);
            for (const auto& sf : sampleFiles)
            {
                zipBuilder.addFile(sf, 9, "Samples/" + sf.getFileName());
            }

            targetZip.deleteFile();
            bool success = false;
            auto* zipOut = targetZip.createOutputStream().release();
            if (zipOut != nullptr)
            {
                success = zipBuilder.writeToStream(*zipOut, nullptr);
                delete zipOut;
            }

            tempDir.deleteRecursively();

            int numExported = static_cast<int>(capturedZones.size());
            juce::MessageManager::callAsync([safeThis, success, targetZip, numExported] {
                if (success && targetZip.existsAsFile())
                {
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::AlertWindow::InfoIcon,
                        "Export Successful",
                        "Successfully exported " + juce::String(numExported) + " sample zone(s) and map definition to:\n" + targetZip.getFullPathName());
                }
                else
                {
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::AlertWindow::WarningIcon,
                        "Export Failed",
                        "Failed to write zip archive to:\n" + targetZip.getFullPathName());
                }
            });
        });
    });
}

// ─────────────────────────────────────────────────────────
//  Paint
// ─────────────────────────────────────────────────────────
void SampleMapComponent::paint(juce::Graphics& g)
{
    g.fillAll(OpenWavLookAndFeel::bgDark);

    auto gridArea = getGridBounds();
    auto keybedArea = getKeybedBounds();
    auto inspectorArea = getInspectorBounds();

    paintZoneGrid(g, gridArea);
    paintKeybed(g, keybedArea);

    // Inspector card background
    g.setColour(OpenWavLookAndFeel::bgCard);
    g.fillRoundedRectangle(inspectorArea, 8.0f);
    g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.4f));
    g.drawRoundedRectangle(inspectorArea, 8.0f, 1.0f);
}

void SampleMapComponent::paintZoneGrid(juce::Graphics& g, juce::Rectangle<float> area) const
{
    // Background card
    g.setColour(OpenWavLookAndFeel::bgCard);
    g.fillRoundedRectangle(area, 8.0f);
    g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.4f));
    g.drawRoundedRectangle(area, 8.0f, 1.0f);

    auto inner = area.reduced(2.0f);

    // Grid octaves (every 12 notes)
    g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.15f));
    for (int note = 0; note <= 128; note += 12)
    {
        float x = xForNoteNumber(note, inner);
        g.drawVerticalLine(static_cast<int>(x), inner.getY(), inner.getBottom());
        g.setFont(juce::Font(9.0f));
        g.setColour(OpenWavLookAndFeel::textSecondary.withAlpha(0.5f));
        g.drawText("C" + juce::String((note / 12) - 1), x + 2.0f, inner.getY() + 2.0f, 30.0f, 12.0f, juce::Justification::left);
        g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.15f));
    }

    // Velocity grid lines and labels (0, 32, 64, 96, 127)
    g.setFont(juce::Font(9.0f));
    for (int vel : { 32, 64, 96 })
    {
        float y = yForVelocity(vel, inner);
        g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.2f));
        g.drawHorizontalLine(static_cast<int>(y), inner.getX(), inner.getRight());
        g.setColour(OpenWavLookAndFeel::textSecondary.withAlpha(0.45f));
        g.drawText("v" + juce::String(vel), inner.getRight() - 24.0f, y - 10.0f, 22.0f, 10.0f, juce::Justification::right);
    }

    // Draw mapped zones
    for (size_t i = 0; i < zones.size(); ++i)
    {
        const auto& z = zones[i];
        float x1 = xForNoteNumber(z.keyLow, inner);
        float x2 = xForNoteNumber(z.keyHigh + 1, inner);
        float y1 = yForVelocity(z.velHigh, inner);
        float y2 = yForVelocity(z.velLow, inner);

        juce::Rectangle<float> zRect(x1, y1, std::max(6.0f, x2 - x1), std::max(6.0f, y2 - y1));

        bool isSelected = isZoneSelected(static_cast<int>(i));
        
        // Base color with slight hue shift for different Round Robin layers
        float hueShift = (z.roundRobinIndex > 1) ? (0.07f * (z.roundRobinIndex - 1)) : 0.0f;
        juce::Colour baseAccent = OpenWavLookAndFeel::accentCyan.withRotatedHue(hueShift);
        juce::Colour zoneCol = isSelected ? baseAccent : baseAccent.withAlpha(0.65f);

        // Zone background fill
        g.setColour(zoneCol.withAlpha(isSelected ? 0.28f : 0.16f));
        g.fillRoundedRectangle(zRect, 4.0f);

        // Zone border
        g.setColour(zoneCol);
        g.drawRoundedRectangle(zRect, 4.0f, isSelected ? 2.0f : 1.0f);

        // Root note indicator line
        float rx = xForNoteNumber(z.rootNote, inner) + (xForNoteNumber(z.rootNote + 1, inner) - xForNoteNumber(z.rootNote, inner)) * 0.5f;
        if (rx >= zRect.getX() && rx <= zRect.getRight())
        {
            g.setColour(OpenWavLookAndFeel::favoriteRed.withAlpha(0.8f));
            g.drawLine(rx, zRect.getY(), rx, zRect.getBottom(), 1.5f);
        }

        // Edge resize handles for selected zone card
        if (isSelected)
        {
            g.setColour(OpenWavLookAndFeel::textPrimary);
            float handleSize = 8.0f;
            g.fillRect(juce::Rectangle<float>(zRect.getX() - 2.0f, zRect.getCentreY() - handleSize * 0.5f, 4.0f, handleSize));
            g.fillRect(juce::Rectangle<float>(zRect.getRight() - 2.0f, zRect.getCentreY() - handleSize * 0.5f, 4.0f, handleSize));
            g.fillRect(juce::Rectangle<float>(zRect.getCentreX() - handleSize * 0.5f, zRect.getY() - 2.0f, handleSize, 4.0f));
            g.fillRect(juce::Rectangle<float>(zRect.getCentreX() - handleSize * 0.5f, zRect.getBottom() - 2.0f, handleSize, 4.0f));
        }

        // Zone text label & badges
        g.setColour(OpenWavLookAndFeel::textPrimary);
        g.setFont(juce::Font(10.0f).boldened());

        juce::String labelText = z.sampleName;
        juce::String tag;
        if (z.roundRobinIndex > 1) tag += "RR" + juce::String(z.roundRobinIndex);
        if (z.velLow > 0 || z.velHigh < 127)
        {
            if (tag.isNotEmpty()) tag += " ";
            tag += "v:" + juce::String(z.velLow) + "-" + juce::String(z.velHigh);
        }
        if (tag.isNotEmpty() && zRect.getHeight() > 24.0f)
            labelText += " [" + tag + "]";

        g.drawText(labelText, zRect.reduced(4.0f, 2.0f), juce::Justification::centred, true);
    }

    // ── Velocity Recorded Hit Dots ──
    uint32_t nowTime = juce::Time::getMillisecondCounter();
    for (const auto& dot : recentHitDots)
    {
        if (dot.note < 0 || dot.note > 127) continue;

        bool isHeld = (activeNoteVelocities[static_cast<size_t>(dot.note)] >= 0);
        float alpha = 1.0f;
        if (!isHeld)
        {
            float ageMs = static_cast<float>(nowTime - dot.timestampMs);
            alpha = juce::jlimit(0.0f, 1.0f, 1.0f - (ageMs / 1200.0f));
        }

        if (alpha <= 0.01f) continue;

        float dotX = xForNoteNumber(dot.note, inner) + (xForNoteNumber(dot.note + 1, inner) - xForNoteNumber(dot.note, inner)) * 0.5f;
        float dotY = yForVelocity(dot.vel, inner);

        // Outer ambient glow
        g.setColour(juce::Colours::white.withAlpha(0.22f * alpha));
        g.fillEllipse(dotX - 9.0f, dotY - 9.0f, 18.0f, 18.0f);

        // Middle halo
        g.setColour(juce::Colours::white.withAlpha(0.55f * alpha));
        g.fillEllipse(dotX - 5.5f, dotY - 5.5f, 11.0f, 11.0f);

        // Core bright white dot
        g.setColour(juce::Colours::white.withAlpha(0.98f * alpha));
        g.fillEllipse(dotX - 3.0f, dotY - 3.0f, 6.0f, 6.0f);

        // Little velocity label badge beside dot
        g.setFont(juce::Font(9.0f).boldened());
        g.setColour(juce::Colours::white.withAlpha(0.9f * alpha));
        g.drawText("v" + juce::String(dot.vel), dotX + 6.0f, dotY - 7.0f, 32.0f, 14.0f, juce::Justification::left);
    }

    // Rubber-band lasso selection box
    if (activeDragTarget == DragTarget::BoxSelect && !lassoRect.isEmpty())
    {
        g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.2f));
        g.fillRect(lassoRect);
        g.setColour(OpenWavLookAndFeel::accentCyan);
        g.drawRect(lassoRect, 1.5f);
    }
}

void SampleMapComponent::triggerKeybedNote(int note, float velocity, int velInt)
{
    if (note < 0 || note > 127)
        return;

    // Strictly match BOTH note AND velocity within mapped zones
    std::vector<int> matchingZoneIndices;
    for (size_t i = 0; i < zones.size(); ++i)
    {
        const auto& z = zones[i];
        if (note >= z.keyLow && note <= z.keyHigh && velInt >= z.velLow && velInt <= z.velHigh && z.filePath.isNotEmpty())
        {
            matchingZoneIndices.push_back(static_cast<int>(i));
        }
    }

    // Only play mapped areas! If no note on selected map at that velocity, DO NOT play outside!
    if (matchingZoneIndices.empty())
    {
        auditionNote = -1;
        return;
    }

    int chosenIdx = -1;
    if (matchingZoneIndices.size() == 1 || roundRobinMode == 2)
    {
        chosenIdx = matchingZoneIndices[0];
    }
    else if (roundRobinMode == 1) // Random RR
    {
        chosenIdx = matchingZoneIndices[static_cast<size_t>(juce::Random::getSystemRandom().nextInt(static_cast<int>(matchingZoneIndices.size())))];
    }
    else // Cycle RR
    {
        std::sort(matchingZoneIndices.begin(), matchingZoneIndices.end(), [this](int a, int b) {
            return zones[a].roundRobinIndex < zones[b].roundRobinIndex;
        });
        static std::map<int, int> rrCounters;
        int count = rrCounters[note]++;
        chosenIdx = matchingZoneIndices[static_cast<size_t>(count) % matchingZoneIndices.size()];
    }

    if (chosenIdx >= 0 && chosenIdx < static_cast<int>(zones.size()))
    {
        selectedZoneIndex = chosenIdx;
        selectedZoneIndices.clear();
        selectedZoneIndices.insert(chosenIdx);

        const auto& z = zones[chosenIdx];
        juce::File fileToLoad(z.filePath);
        if (fileToLoad.existsAsFile())
        {
            if (audioEngine.getCurrentFile() != fileToLoad)
            {
                audioEngine.loadFile(fileToLoad, false, true);
            }

            float effectiveVelocity = computeEffectiveVelocity(velocity, &z);
            audioEngine.playZoneVoice(fileToLoad, note, z.rootNote, z.fineTuneCents, z.gainDb, effectiveVelocity,
                                      z.attackMs / 1000.0f, z.decayMs / 1000.0f, z.sustainLevel, z.releaseMs / 1000.0f,
                                      oneShotButton.getToggleState() || audioEngine.isOneShotEnabled(),
                                      loopButton.getToggleState() || audioEngine.isLooping());

            velocityCurveView.triggerHit(velInt);

            auditionNote = note;
            int ch = midiChannelSetting > 0 ? midiChannelSetting : 2;
            audioEngine.getKeyboardState().noteOn(ch, note, effectiveVelocity);
            resized();
            repaint();
        }
        else
        {
            auditionNote = -1;
        }
    }
    else
    {
        auditionNote = -1;
    }
}

void SampleMapComponent::paintKeybed(juce::Graphics& g, juce::Rectangle<float> area) const
{
    g.setColour(OpenWavLookAndFeel::bgHeader);
    g.fillRoundedRectangle(area, 4.0f);

    float noteWidth = area.getWidth() / 128.0f;

    // Draw 128 keys (white & black)
    for (int note = 0; note < 128; ++note)
    {
        float kx = area.getX() + note * noteWidth;
        int noteInOctave = note % 12;
        bool isBlackKey = (noteInOctave == 1 || noteInOctave == 3 || noteInOctave == 6 || noteInOctave == 8 || noteInOctave == 10);

        juce::Rectangle<float> keyRect(kx, area.getY(), noteWidth, area.getHeight());

        bool hasZone = false;
        for (const auto& z : zones)
        {
            if (note >= z.keyLow && note <= z.keyHigh && z.filePath.isNotEmpty())
            {
                hasZone = true;
                break;
            }
        }

        bool isMidiPressed = activeMidiNotes[static_cast<size_t>(note)];
        if (note == auditionNote || isMidiPressed)
        {
            int vel = (note == auditionNote) ? auditionVelocity : activeNoteVelocities[static_cast<size_t>(note)];
            if (vel <= 0) vel = 100;
            float velNorm = juce::jlimit(0.01f, 1.0f, static_cast<float>(vel) / 127.0f);

            float maxH = isBlackKey ? (area.getHeight() * 0.65f) : area.getHeight();
            g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.35f));
            g.fillRect(keyRect.withHeight(maxH));

            float fillH = maxH * velNorm;
            g.setColour(OpenWavLookAndFeel::accentCyan);
            g.fillRect(keyRect.withHeight(fillH));

            g.setColour(juce::Colours::white);
            g.fillRect(keyRect.getX(), keyRect.getY() + fillH - 1.5f, keyRect.getWidth(), 2.0f);
        }
        else if (isBlackKey)
        {
            g.setColour(juce::Colours::black.withAlpha(0.85f));
            g.fillRect(keyRect.withHeight(area.getHeight() * 0.65f));

            if (hasZone)
            {
                g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.6f));
                g.fillRect(keyRect.getX() + 0.5f, area.getY() + area.getHeight() * 0.65f - 2.5f, keyRect.getWidth() - 1.0f, 2.0f);
            }
        }
        else
        {
            if (hasZone)
            {
                g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.08f));
                g.fillRect(keyRect);
                g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.35f));
                g.drawRect(keyRect, 0.5f);
                g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.65f));
                g.fillRect(keyRect.getX() + 0.5f, keyRect.getBottom() - 3.0f, keyRect.getWidth() - 1.0f, 2.5f);
            }
            else
            {
                g.setColour(OpenWavLookAndFeel::textSecondary.withAlpha(0.15f));
                g.fillRect(keyRect);
                g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.3f));
                g.drawRect(keyRect, 0.5f);
            }

            if (note % 12 == 0)
            {
                g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.45f));
                g.fillRect(keyRect.getX() + keyRect.getWidth() * 0.15f, keyRect.getBottom() - 3.0f, keyRect.getWidth() * 0.7f, 2.0f);
            }
        }
    }
}

// ─────────────────────────────────────────────────────────
//  Mouse Interaction
// ─────────────────────────────────────────────────────────
void SampleMapComponent::mouseDown(const juce::MouseEvent& e)
{
    auto gridArea = getGridBounds();
    auto keybedArea = getKeybedBounds();

    if (keybedArea.contains(e.position))
    {
        int targetNote = noteNumberAtX(e.x, keybedArea);
        activeDragTarget = DragTarget::KeybedAudition;
        auditionKeyUnderMouse = targetNote;

        int noteInOctave = targetNote % 12;
        bool isBlackKey = (noteInOctave == 1 || noteInOctave == 3 || noteInOctave == 6 || noteInOctave == 8 || noteInOctave == 10);
        float keyH = isBlackKey ? (keybedArea.getHeight() * 0.65f) : keybedArea.getHeight();

        float relY = juce::jlimit(0.0f, 1.0f, (e.position.y - keybedArea.getY()) / keyH);
        // Bottom of key (relY = 1.0) is 127, top of key (relY = 0.0) is lowest velocity (1)
        int velInt = juce::jlimit(1, 127, static_cast<int>(std::round(1.0f + relY * 126.0f)));
        float floatVelocity = static_cast<float>(velInt) / 127.0f;
        auditionVelocity = velInt;

        triggerKeybedNote(targetNote, floatVelocity, velInt);
        return;
    }

    if (gridArea.contains(e.position))
    {
        auto inner = gridArea.reduced(2.0f);
        float edgeThreshold = 8.0f;
        bool isMulti = e.mods.isCommandDown() || e.mods.isCtrlDown() || e.mods.isShiftDown();

        for (int i = static_cast<int>(zones.size()) - 1; i >= 0; --i)
        {
            const auto& z = zones[i];
            float x1 = xForNoteNumber(z.keyLow, inner);
            float x2 = xForNoteNumber(z.keyHigh + 1, inner);
            float y1 = yForVelocity(z.velHigh, inner);
            float y2 = yForVelocity(z.velLow, inner);

            juce::Rectangle<float> zRect(x1, y1, std::max(8.0f, x2 - x1), std::max(8.0f, y2 - y1));

            if (zRect.expanded(edgeThreshold).contains(e.position))
            {
                if (e.mods.isPopupMenu())
                {
                    if (!isZoneSelected(i) && !isMulti)
                    {
                        selectZone(i, false);
                    }
                    juce::PopupMenu menu;
                    menu.addItem(1, "Delete Selected Zone(s)");
                    menu.addSeparator();
                    menu.addItem(2, "Auto Pitch Map");
                    menu.addItem(3, "Auto Chromatic Map");
                    menu.addItem(4, "Reveal in Finder");

                    menu.showMenuAsync(juce::PopupMenu::Options().withMousePosition(), [this, z](int result) {
                        if (result == 1)
                        {
                            deleteSelectedZones();
                        }
                        else if (result == 2)
                        {
                            autoMapByPitch();
                        }
                        else if (result == 3)
                        {
                            autoMapChromatic();
                        }
                        else if (result == 4)
                        {
                            juce::File f(z.filePath);
                            if (f.existsAsFile())
                            {
                                f.revealToUser();
                            }
                        }
                    });
                    return;
                }

                if (isMulti)
                {
                    if (isZoneSelected(i))
                        selectedZoneIndices.erase(i);
                    else
                        selectedZoneIndices.insert(i);
                    selectedZoneIndex = i;
                }
                else if (!isZoneSelected(i))
                {
                    selectZone(i, false);
                }

                activeDragZone = i;

                dragStartNote = noteNumberAtX(e.x, gridArea);
                dragStartVel = velocityAtY(e.y, gridArea);
                dragStartZones.clear();
                for (int sIdx : selectedZoneIndices)
                {
                    if (sIdx >= 0 && sIdx < static_cast<int>(zones.size()))
                        dragStartZones[sIdx] = zones[sIdx];
                }

                juce::File fileToLoad(z.filePath);
                if (fileToLoad.existsAsFile())
                {
                    audioEngine.loadFile(fileToLoad, false, true);
                    audioEngine.playZoneVoice(fileToLoad, z.rootNote, z.rootNote, z.fineTuneCents, z.gainDb, 0.8f,
                                              z.attackMs / 1000.0f, z.decayMs / 1000.0f, z.sustainLevel, z.releaseMs / 1000.0f,
                                              oneShotButton.getToggleState() || audioEngine.isOneShotEnabled(),
                                              loopButton.getToggleState() || audioEngine.isLooping());
                }
                auditionNote = z.rootNote;
                auditionVelocity = 100;
                int ch = midiChannelSetting > 0 ? midiChannelSetting : 2;
                audioEngine.getKeyboardState().noteOn(ch, auditionNote, 0.8f);

                float zoneW = zRect.getWidth();
                float effectiveEdgeThreshold = (zoneW < 24.0f) ? std::min(3.0f, zoneW * 0.25f) : edgeThreshold;

                float dLeft = std::abs(e.x - zRect.getX());
                float dRight = std::abs(e.x - zRect.getRight());
                float dTop = std::abs(e.y - zRect.getY());
                float dBottom = std::abs(e.y - zRect.getBottom());

                if (e.mods.isAltDown())
                {
                    activeDragTarget = DragTarget::MoveZone;
                }
                else if (zoneW < 24.0f && e.x > zRect.getX() + effectiveEdgeThreshold && e.x < zRect.getRight() - effectiveEdgeThreshold)
                {
                    activeDragTarget = DragTarget::MoveZone;
                }
                else if (dLeft <= effectiveEdgeThreshold)
                {
                    activeDragTarget = DragTarget::ResizeKeyLow;
                }
                else if (dRight <= effectiveEdgeThreshold)
                {
                    activeDragTarget = DragTarget::ResizeKeyHigh;
                }
                else if (dTop <= edgeThreshold)
                {
                    activeDragTarget = DragTarget::ResizeVelHigh;
                }
                else if (dBottom <= edgeThreshold)
                {
                    activeDragTarget = DragTarget::ResizeVelLow;
                }
                else
                {
                    activeDragTarget = DragTarget::MoveZone;
                }

                resized();
                repaint();
                return;
            }
        }

        // Click on empty grid background -> Start rubber-band lasso box-selection
        activeDragTarget = DragTarget::BoxSelect;
        boxSelectStartPos = e.position;
        lassoRect = juce::Rectangle<float>();
        if (!isMulti)
        {
            selectedZoneIndices.clear();
            selectedZoneIndex = -1;
        }
        resized();
        repaint();
    }
}

void SampleMapComponent::mouseMove(const juce::MouseEvent& e)
{
    auto keybedArea = getKeybedBounds();
    if (keybedArea.contains(e.position))
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        return;
    }

    auto gridArea = getGridBounds();
    if (!gridArea.contains(e.position))
    {
        setMouseCursor(juce::MouseCursor::NormalCursor);
        return;
    }

    auto inner = gridArea.reduced(2.0f);
    float edgeThreshold = 8.0f;

    for (int i = static_cast<int>(zones.size()) - 1; i >= 0; --i)
    {
        const auto& z = zones[i];
        float x1 = xForNoteNumber(z.keyLow, inner);
        float x2 = xForNoteNumber(z.keyHigh + 1, inner);
        float y1 = yForVelocity(z.velHigh, inner);
        float y2 = yForVelocity(z.velLow, inner);

        juce::Rectangle<float> zRect(x1, y1, std::max(8.0f, x2 - x1), std::max(8.0f, y2 - y1));

        if (zRect.expanded(edgeThreshold).contains(e.position))
        {
            float zoneW = zRect.getWidth();
            float effectiveEdgeThreshold = (zoneW < 24.0f) ? std::min(3.0f, zoneW * 0.25f) : edgeThreshold;

            float dLeft = std::abs(e.x - zRect.getX());
            float dRight = std::abs(e.x - zRect.getRight());
            float dTop = std::abs(e.y - zRect.getY());
            float dBottom = std::abs(e.y - zRect.getBottom());

            if (dLeft <= effectiveEdgeThreshold || dRight <= effectiveEdgeThreshold)
            {
                setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
                return;
            }
            if (dTop <= edgeThreshold || dBottom <= edgeThreshold)
            {
                setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
                return;
            }
            setMouseCursor(juce::MouseCursor::DraggingHandCursor);
            return;
        }
    }

    setMouseCursor(juce::MouseCursor::NormalCursor);
}

void SampleMapComponent::mouseDrag(const juce::MouseEvent& e)
{
    auto keybedArea = getKeybedBounds();
    if (activeDragTarget == DragTarget::KeybedAudition)
    {
        int newNote = noteNumberAtX(e.x, keybedArea);
        int noteInOctave = newNote % 12;
        bool isBlackKey = (noteInOctave == 1 || noteInOctave == 3 || noteInOctave == 6 || noteInOctave == 8 || noteInOctave == 10);
        float keyH = isBlackKey ? (keybedArea.getHeight() * 0.65f) : keybedArea.getHeight();

        float relY = juce::jlimit(0.0f, 1.0f, (e.position.y - keybedArea.getY()) / keyH);
        int newVelInt = juce::jlimit(1, 127, static_cast<int>(std::round(1.0f + relY * 126.0f)));
        float newVelocity = static_cast<float>(newVelInt) / 127.0f;

        if (newNote != auditionKeyUnderMouse)
        {
            if (auditionNote >= 0)
            {
                audioEngine.stopZoneVoice(auditionNote);
                int ch = midiChannelSetting > 0 ? midiChannelSetting : 2;
                audioEngine.getKeyboardState().noteOff(ch, auditionNote, 0.0f);
                auditionNote = -1;
            }
            auditionKeyUnderMouse = newNote;
            auditionVelocity = newVelInt;
            triggerKeybedNote(newNote, newVelocity, newVelInt);
        }
        return;
    }

    auto gridArea = getGridBounds();

    if (activeDragTarget == DragTarget::BoxSelect)
    {
        float curX = juce::jlimit(gridArea.getX(), gridArea.getRight(), e.position.x);
        float curY = juce::jlimit(gridArea.getY(), gridArea.getBottom(), e.position.y);
        float startX = juce::jlimit(gridArea.getX(), gridArea.getRight(), boxSelectStartPos.x);
        float startY = juce::jlimit(gridArea.getY(), gridArea.getBottom(), boxSelectStartPos.y);

        float lx = std::min(startX, curX);
        float ly = std::min(startY, curY);
        float lw = std::abs(curX - startX);
        float lh = std::abs(curY - startY);
        lassoRect = juce::Rectangle<float>(lx, ly, lw, lh).getIntersection(gridArea);

        auto inner = gridArea.reduced(2.0f);
        if (!e.mods.isCommandDown() && !e.mods.isCtrlDown() && !e.mods.isShiftDown())
            selectedZoneIndices.clear();

        for (int i = 0; i < static_cast<int>(zones.size()); ++i)
        {
            const auto& z = zones[i];
            float x1 = xForNoteNumber(z.keyLow, inner);
            float x2 = xForNoteNumber(z.keyHigh + 1, inner);
            float y1 = yForVelocity(z.velHigh, inner);
            float y2 = yForVelocity(z.velLow, inner);

            juce::Rectangle<float> zRect(x1, y1, std::max(8.0f, x2 - x1), std::max(8.0f, y2 - y1));
            if (lassoRect.intersects(zRect))
            {
                selectedZoneIndices.insert(i);
                selectedZoneIndex = i;
            }
        }
        resized();
        repaint();
        return;
    }

    if (activeDragZone < 0 || activeDragZone >= static_cast<int>(zones.size())) return;

    int currentNote = noteNumberAtX(e.x, gridArea);
    int currentVel = velocityAtY(e.y, gridArea);
    int noteDelta = currentNote - dragStartNote;
    int velDelta = currentVel - dragStartVel;

    if (activeDragTarget == DragTarget::MoveZone)
    {
        for (int sIdx : selectedZoneIndices)
        {
            if (sIdx >= 0 && sIdx < static_cast<int>(zones.size()) && dragStartZones.count(sIdx) > 0)
            {
                const auto& startZ = dragStartZones[sIdx];
                int span = startZ.keyHigh - startZ.keyLow;
                int newLow = juce::jlimit(0, 127 - span, startZ.keyLow + noteDelta);

                auto& z = zones[sIdx];
                z.keyLow = newLow;
                z.keyHigh = newLow + span;
                z.rootNote = juce::jlimit(z.keyLow, z.keyHigh, startZ.rootNote + noteDelta);

                int vSpan = startZ.velHigh - startZ.velLow;
                int newVelLow = juce::jlimit(0, 127 - vSpan, startZ.velLow + velDelta);
                z.velLow = newVelLow;
                z.velHigh = newVelLow + vSpan;
            }
        }
    }
    else if (activeDragTarget == DragTarget::ResizeKeyLow)
    {
        for (int sIdx : selectedZoneIndices)
        {
            if (sIdx >= 0 && sIdx < static_cast<int>(zones.size()) && dragStartZones.count(sIdx) > 0)
            {
                const auto& startZ = dragStartZones[sIdx];
                auto& z = zones[sIdx];
                z.keyLow = juce::jmin(z.keyHigh, juce::jlimit(0, 127, startZ.keyLow + noteDelta));
                z.rootNote = juce::jlimit(z.keyLow, z.keyHigh, z.rootNote);
            }
        }
    }
    else if (activeDragTarget == DragTarget::ResizeKeyHigh)
    {
        for (int sIdx : selectedZoneIndices)
        {
            if (sIdx >= 0 && sIdx < static_cast<int>(zones.size()) && dragStartZones.count(sIdx) > 0)
            {
                const auto& startZ = dragStartZones[sIdx];
                auto& z = zones[sIdx];
                z.keyHigh = juce::jmax(z.keyLow, juce::jlimit(0, 127, startZ.keyHigh + noteDelta));
                z.rootNote = juce::jlimit(z.keyLow, z.keyHigh, z.rootNote);
            }
        }
    }
    else if (activeDragTarget == DragTarget::ResizeVelLow)
    {
        for (int sIdx : selectedZoneIndices)
        {
            if (sIdx >= 0 && sIdx < static_cast<int>(zones.size()) && dragStartZones.count(sIdx) > 0)
            {
                const auto& startZ = dragStartZones[sIdx];
                auto& z = zones[sIdx];
                z.velLow = juce::jmin(z.velHigh, juce::jlimit(0, 127, startZ.velLow + velDelta));
            }
        }
    }
    else if (activeDragTarget == DragTarget::ResizeVelHigh)
    {
        for (int sIdx : selectedZoneIndices)
        {
            if (sIdx >= 0 && sIdx < static_cast<int>(zones.size()) && dragStartZones.count(sIdx) > 0)
            {
                const auto& startZ = dragStartZones[sIdx];
                auto& z = zones[sIdx];
                z.velHigh = juce::jmax(z.velLow, juce::jlimit(0, 127, startZ.velHigh + velDelta));
            }
        }
    }

    resized();
    repaint();
}

void SampleMapComponent::mouseUp(const juce::MouseEvent& /*e*/)
{
    if (auditionNote >= 0)
    {
        int ch = midiChannelSetting > 0 ? midiChannelSetting : 2;
        audioEngine.stopZoneVoice(auditionNote);
        audioEngine.getKeyboardState().noteOff(ch, auditionNote, 0.0f);
        audioEngine.triggerNoteOff(auditionNote);
    }

    bool wasDraggingZone = (activeDragTarget == DragTarget::MoveZone ||
                            activeDragTarget == DragTarget::ResizeKeyLow ||
                            activeDragTarget == DragTarget::ResizeKeyHigh ||
                            activeDragTarget == DragTarget::ResizeVelLow ||
                            activeDragTarget == DragTarget::ResizeVelHigh);

    activeDragTarget = DragTarget::None;
    activeDragZone = -1;
    auditionNote = -1;
    auditionKeyUnderMouse = -1;
    auditionVelocity = 100;
    lassoRect = juce::Rectangle<float>();
    dragStartZones.clear();
    repaint();

    if (wasDraggingZone && onStateChanged)
    {
        onStateChanged();
    }
}

// ─────────────────────────────────────────────────────────
//  Layout
// ─────────────────────────────────────────────────────────
void SampleMapComponent::resized()
{
    auto area = getLocalBounds().reduced(20, 16);

    // ── Single Top Toolbar ──
    auto topRow = area.removeFromTop(28);
    int gap = 5;

    midiChannelButton.setBounds(topRow.removeFromRight(88));
    topRow.removeFromRight(gap);

    velCurveButton.setBounds(topRow.removeFromRight(96));
    topRow.removeFromRight(gap);

    addSampleButton.setBounds(topRow.removeFromLeft(78));
    topRow.removeFromLeft(gap);
    lorisResynthButton.setBounds(topRow.removeFromLeft(90));
    topRow.removeFromLeft(gap);
    deleteSelectedButton.setBounds(topRow.removeFromLeft(96));
    topRow.removeFromLeft(gap);
    autoMapPitchButton.setBounds(topRow.removeFromLeft(70));
    topRow.removeFromLeft(gap);
    autoMapChromaticButton.setBounds(topRow.removeFromLeft(84));
    topRow.removeFromLeft(gap);
    autoMapVelButton.setBounds(topRow.removeFromLeft(78));
    topRow.removeFromLeft(gap);
    autoMapRRButton.setBounds(topRow.removeFromLeft(64));
    topRow.removeFromLeft(gap);
    clearMapButton.setBounds(topRow.removeFromLeft(64));
    topRow.removeFromLeft(gap);
    saveMapButton.setBounds(topRow.removeFromLeft(66));
    topRow.removeFromLeft(gap);
    loadMapButton.setBounds(topRow.removeFromLeft(66));
    topRow.removeFromLeft(gap);
    exportZipButton.setBounds(topRow.removeFromLeft(78));
    topRow.removeFromLeft(gap);
    roundRobinButton.setBounds(topRow.removeFromLeft(78));
    topRow.removeFromLeft(gap);
    pitchTrackButton.setBounds(topRow.removeFromLeft(88));
    topRow.removeFromLeft(gap);
    oneShotButton.setBounds(topRow.removeFromLeft(84));
    topRow.removeFromLeft(gap);
    loopButton.setBounds(topRow.removeFromLeft(66));
    topRow.removeFromLeft(gap);
    openFxRackButton.setBounds(topRow.removeFromLeft(82));

    attackKnob.setVisible(false);
    attackLabel.setVisible(false);
    decayKnob.setVisible(false);
    decayLabel.setVisible(false);
    sustainKnob.setVisible(false);
    sustainLabel.setVisible(false);
    releaseKnob.setVisible(false);
    releaseLabel.setVisible(false);

    // ── Inspector Panel Layout ─────────────────────────
    auto inspectorArea = getInspectorBounds().reduced(12, 10);
    int rowH = 20;
    int labelW = 85;
    int rowGap = 4;

    // Inspector Tab Switchers: [ Zones ] [ Vel Curve ]
    auto tabRow = inspectorArea.removeFromTop(22);
    int tabW = (tabRow.getWidth() - 6) / 2;
    inspectorTabZonesBtn.setBounds(tabRow.removeFromLeft(tabW).toNearestInt());
    tabRow.removeFromLeft(6);
    inspectorTabVelBtn.setBounds(tabRow.toNearestInt());
    inspectorArea.removeFromTop(8);

    if (inspectorActiveTab == 1)
    {
        // ── VELOCITY RESPONSE CURVE TAB ──
        rootNoteTitle.setVisible(false); rootNoteSlider.setVisible(false);
        keyLowTitle.setVisible(false); keyLowSlider.setVisible(false);
        keyHighTitle.setVisible(false); keyHighSlider.setVisible(false);
        velLowTitle.setVisible(false); velLowSlider.setVisible(false);
        velHighTitle.setVisible(false); velHighSlider.setVisible(false);
        rrTitle.setVisible(false); rrSlider.setVisible(false);
        tuneTitle.setVisible(false); tuneSlider.setVisible(false);
        gainTitle.setVisible(false); gainSlider.setVisible(false);
        attackTitle.setVisible(false); attackSlider.setVisible(false);
        decayTitle.setVisible(false); decaySlider.setVisible(false);
        sustainTitle.setVisible(false); sustainSlider.setVisible(false);
        releaseTitle.setVisible(false); releaseSlider.setVisible(false);
        reverbTitle.setVisible(false); reverbSlider.setVisible(false);
        inspectorDeleteButton.setVisible(false);
        sampleNameValue.setVisible(false);

        inspectorTitle.setVisible(true);
        inspectorTitle.setText("Velocity Response Curve", juce::dontSendNotification);
        inspectorTitle.setBounds(inspectorArea.removeFromTop(20).toNearestInt());
        inspectorArea.removeFromTop(4);

        // Scope & Audition row
        auto scopeRow = inspectorArea.removeFromTop(22);
        velScopeButton.setVisible(true);
        velScopeButton.setBounds(scopeRow.removeFromLeft(105).toNearestInt());
        scopeRow.removeFromLeft(6);
        velTestAuditionBtn.setVisible(true);
        velTestAuditionBtn.setBounds(scopeRow.toNearestInt());
        inspectorArea.removeFromTop(8);

        // Visual Curve Component
        velocityCurveView.setVisible(true);
        int curveH = std::min(130, static_cast<int>(inspectorArea.getHeight()) - 150);
        if (curveH < 90) curveH = 90;
        velocityCurveView.setBounds(inspectorArea.removeFromTop(curveH).toNearestInt());
        inspectorArea.removeFromTop(8);

        // Preset buttons row: Lin | Soft | Hard | S-Curv | Fixed
        auto presetRow = inspectorArea.removeFromTop(20);
        int btnW = (presetRow.getWidth() - 16) / 5;
        velLinBtn.setVisible(true); velLinBtn.setBounds(presetRow.removeFromLeft(btnW).toNearestInt()); presetRow.removeFromLeft(4);
        velSoftBtn.setVisible(true); velSoftBtn.setBounds(presetRow.removeFromLeft(btnW).toNearestInt()); presetRow.removeFromLeft(4);
        velHardBtn.setVisible(true); velHardBtn.setBounds(presetRow.removeFromLeft(btnW).toNearestInt()); presetRow.removeFromLeft(4);
        velSCurveBtn.setVisible(true); velSCurveBtn.setBounds(presetRow.removeFromLeft(btnW).toNearestInt()); presetRow.removeFromLeft(4);
        velFixedBtn.setVisible(true); velFixedBtn.setBounds(presetRow.toNearestInt());
        inspectorArea.removeFromTop(8);

        // Sliders
        auto setupSliderRow = [&](juce::Label& lbl, juce::Slider& sld) {
            lbl.setVisible(true);
            sld.setVisible(true);
            auto r = inspectorArea.removeFromTop(20);
            inspectorArea.removeFromTop(4);
            lbl.setBounds(r.removeFromLeft(70).toNearestInt());
            r.removeFromLeft(4);
            sld.setBounds(r.toNearestInt());
        };

        setupSliderRow(velSensitivityLabel, velSensitivitySlider);
        setupSliderRow(velCurveLabel, velCurveSlider);
        setupSliderRow(velFloorLabel, velFloorSlider);

        inspectorArea.removeFromTop(4);
        velReadoutLabel.setVisible(true);
        velReadoutLabel.setBounds(inspectorArea.removeFromTop(18).toNearestInt());
    }
    else
    {
        // ── ZONE PARAMETERS TAB ──
        velLinBtn.setVisible(false);
        velSoftBtn.setVisible(false);
        velHardBtn.setVisible(false);
        velSCurveBtn.setVisible(false);
        velFixedBtn.setVisible(false);
        velSensitivityLabel.setVisible(false); velSensitivitySlider.setVisible(false);
        velCurveLabel.setVisible(false); velCurveSlider.setVisible(false);
        velFloorLabel.setVisible(false); velFloorSlider.setVisible(false);
        velScopeButton.setVisible(false);
        velTestAuditionBtn.setVisible(false);
        velReadoutLabel.setVisible(false);

        inspectorTitle.setVisible(true);
        inspectorTitle.setText("Zone Parameters", juce::dontSendNotification);
        inspectorTitle.setBounds(inspectorArea.removeFromTop(20).toNearestInt());
        inspectorArea.removeFromTop(4);

        if (selectedZoneIndex >= 0 && selectedZoneIndex < static_cast<int>(zones.size()))
        {
            sampleNameValue.setVisible(true);
            const auto& z = zones[selectedZoneIndex];
            sampleNameValue.setText(z.sampleName, juce::dontSendNotification);
            inspectorArea.removeFromTop(rowH);

            auto setupRow = [&](juce::Label& lbl, juce::Slider& slider, double val) {
                lbl.setVisible(true);
                slider.setVisible(true);
                auto r = inspectorArea.removeFromTop(rowH);
                inspectorArea.removeFromTop(rowGap);
                lbl.setBounds(r.removeFromLeft(labelW).toNearestInt());
                r.removeFromLeft(4);
                slider.setBounds(r.toNearestInt());
                slider.setValue(val, juce::dontSendNotification);
            };

            setupRow(rootNoteTitle, rootNoteSlider, z.rootNote);
            setupRow(keyLowTitle, keyLowSlider, z.keyLow);
            setupRow(keyHighTitle, keyHighSlider, z.keyHigh);
            setupRow(velLowTitle, velLowSlider, z.velLow);
            setupRow(velHighTitle, velHighSlider, z.velHigh);
            setupRow(rrTitle, rrSlider, z.roundRobinIndex);
            setupRow(tuneTitle, tuneSlider, z.fineTuneCents);
            setupRow(gainTitle, gainSlider, z.gainDb);

            inspectorArea.removeFromTop(4);
            setupRow(attackTitle, attackSlider, z.attackMs);
            setupRow(decayTitle, decaySlider, z.decayMs);
            setupRow(sustainTitle, sustainSlider, z.sustainLevel);
            setupRow(releaseTitle, releaseSlider, z.releaseMs);
            setupRow(reverbTitle, reverbSlider, audioEngine.getSamplerReverbAmount() * 100.0f);

            inspectorArea.removeFromTop(6);
            inspectorDeleteButton.setVisible(true);
            inspectorDeleteButton.setBounds(inspectorArea.removeFromTop(22).toNearestInt());

            if (inspectorArea.getHeight() >= 80)
            {
                inspectorArea.removeFromTop(6);
                velocityCurveView.setVisible(true);
                velocityCurveView.setBounds(inspectorArea.removeFromTop(std::min(90, static_cast<int>(inspectorArea.getHeight()))).toNearestInt());
            }
            else
            {
                velocityCurveView.setVisible(false);
            }
        }
        else
        {
            rootNoteTitle.setVisible(false); rootNoteSlider.setVisible(false);
            keyLowTitle.setVisible(false); keyLowSlider.setVisible(false);
            keyHighTitle.setVisible(false); keyHighSlider.setVisible(false);
            velLowTitle.setVisible(false); velLowSlider.setVisible(false);
            velHighTitle.setVisible(false); velHighSlider.setVisible(false);
            rrTitle.setVisible(false); rrSlider.setVisible(false);
            tuneTitle.setVisible(false); tuneSlider.setVisible(false);
            gainTitle.setVisible(false); gainSlider.setVisible(false);
            attackTitle.setVisible(false); attackSlider.setVisible(false);
            decayTitle.setVisible(false); decaySlider.setVisible(false);
            sustainTitle.setVisible(false); sustainSlider.setVisible(false);
            releaseTitle.setVisible(false); releaseSlider.setVisible(false);
            reverbTitle.setVisible(false); reverbSlider.setVisible(false);
            inspectorDeleteButton.setVisible(false);

            sampleNameValue.setVisible(true);
            sampleNameValue.setText("No Zone Selected", juce::dontSendNotification);
            sampleNameValue.setBounds(inspectorArea.removeFromTop(rowH).toNearestInt());
            inspectorArea.removeFromTop(10);

            velocityCurveView.setVisible(true);
            int curveH = std::min(120, static_cast<int>(inspectorArea.getHeight()) - 40);
            if (curveH > 60)
            {
                velocityCurveView.setBounds(inspectorArea.removeFromTop(curveH).toNearestInt());
            }
        }
    }
}

// ─────────────────────────────────────────────────────────
//  Listeners
// ─────────────────────────────────────────────────────────
void SampleMapComponent::sliderValueChanged(juce::Slider* slider)
{
    if (slider == &velSensitivitySlider)
    {
        float val = static_cast<float>(slider->getValue() / 100.0);
        if (velocityScopeIsZone && selectedZoneIndex >= 0 && selectedZoneIndex < static_cast<int>(zones.size()))
            zones[selectedZoneIndex].velocitySensitivity = val;
        else
            globalVelocitySensitivity = val;

        velocityCurveView.setSensitivity(val);
        updateVelocityCurveButtonText();
        if (onStateChanged) onStateChanged();
        repaint();
        return;
    }
    else if (slider == &velCurveSlider)
    {
        float val = static_cast<float>(slider->getValue() / 100.0);
        globalVelocityCurve = val;
        globalVelocityCurveMode = 0;
        velocityCurveView.setCurve(val);
        velocityCurveView.setCurveMode(0);
        updateVelocityCurveButtonText();
        if (onStateChanged) onStateChanged();
        repaint();
        return;
    }
    else if (slider == &velFloorSlider)
    {
        int val = static_cast<int>(slider->getValue());
        globalVelocityMinFloor = val;
        velocityCurveView.setMinFloor(val);
        if (onStateChanged) onStateChanged();
        repaint();
        return;
    }
    else if (slider == &attackKnob || slider == &attackSlider)
    {
        float val = static_cast<float>(slider->getValue());
        globalAttackMs = val;
        attackKnob.setValue(val, juce::dontSendNotification);
        attackSlider.setValue(val, juce::dontSendNotification);
        for (int sIdx : selectedZoneIndices)
        {
            if (sIdx >= 0 && sIdx < static_cast<int>(zones.size()))
                zones[sIdx].attackMs = val;
        }
        repaint();
        return;
    }
    else if (slider == &decayKnob || slider == &decaySlider)
    {
        float val = static_cast<float>(slider->getValue());
        globalDecayMs = val;
        decayKnob.setValue(val, juce::dontSendNotification);
        decaySlider.setValue(val, juce::dontSendNotification);
        for (int sIdx : selectedZoneIndices)
        {
            if (sIdx >= 0 && sIdx < static_cast<int>(zones.size()))
                zones[sIdx].decayMs = val;
        }
        repaint();
        return;
    }
    else if (slider == &sustainKnob || slider == &sustainSlider)
    {
        float val = static_cast<float>(slider->getValue());
        globalSustainLevel = val;
        sustainKnob.setValue(val, juce::dontSendNotification);
        sustainSlider.setValue(val, juce::dontSendNotification);
        for (int sIdx : selectedZoneIndices)
        {
            if (sIdx >= 0 && sIdx < static_cast<int>(zones.size()))
                zones[sIdx].sustainLevel = val;
        }
        repaint();
        return;
    }
    else if (slider == &releaseKnob || slider == &releaseSlider)
    {
        float val = static_cast<float>(slider->getValue());
        globalReleaseMs = val;
        releaseKnob.setValue(val, juce::dontSendNotification);
        releaseSlider.setValue(val, juce::dontSendNotification);
        for (int sIdx : selectedZoneIndices)
        {
            if (sIdx >= 0 && sIdx < static_cast<int>(zones.size()))
                zones[sIdx].releaseMs = val;
        }
        repaint();
        return;
    }

    if (selectedZoneIndex < 0 || selectedZoneIndex >= static_cast<int>(zones.size()))
        return;

    auto& z = zones[selectedZoneIndex];

    if (slider == &rootNoteSlider) z.rootNote = static_cast<int>(rootNoteSlider.getValue());
    else if (slider == &keyLowSlider) z.keyLow = juce::jmin(z.keyHigh, static_cast<int>(keyLowSlider.getValue()));
    else if (slider == &keyHighSlider) z.keyHigh = juce::jmax(z.keyLow, static_cast<int>(keyHighSlider.getValue()));
    else if (slider == &velLowSlider) z.velLow = juce::jmin(z.velHigh, static_cast<int>(velLowSlider.getValue()));
    else if (slider == &velHighSlider) z.velHigh = juce::jmax(z.velLow, static_cast<int>(velHighSlider.getValue()));
    else if (slider == &rrSlider) z.roundRobinIndex = static_cast<int>(rrSlider.getValue());
    else if (slider == &tuneSlider) z.fineTuneCents = static_cast<float>(tuneSlider.getValue());
    else if (slider == &gainSlider) z.gainDb = static_cast<float>(gainSlider.getValue());
    else if (slider == &reverbSlider)
    {
        float val = static_cast<float>(reverbSlider.getValue() / 100.0f);
        audioEngine.setSamplerReverbAmount(val);
        repaint();
        return;
    }

    repaint();
    if (onStateChanged) onStateChanged();
}

void SampleMapComponent::buttonClicked(juce::Button* button)
{
    if (button == &addSampleButton)
    {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Select Audio Samples to Map",
            juce::File::getSpecialLocation(juce::File::userDesktopDirectory),
            "*.wav;*.mp3;*.flac;*.ogg;*.aif;*.aiff;*.aifc;*.WAV;*.MP3;*.FLAC;*.OGG;*.AIF;*.AIFF;*.AIFC",
            true);

        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems, [this, chooser](const juce::FileChooser& fc) {
            auto results = fc.getResults();
            if (results.isEmpty())
            {
                auto single = fc.getResult();
                if (single.existsAsFile())
                    results.add(single);
            }
            if (results.isEmpty()) return;

            for (const auto& file : results)
            {
                addSampleFile(file);
            }
        });
    }
    else if (button == &lorisResynthButton) openLorisResynthesisDialog();
    else if (button == &deleteSelectedButton) deleteSelectedZones();
    else if (button == &autoMapPitchButton) autoMapByPitch();
    else if (button == &autoMapChromaticButton) autoMapChromatic();
    else if (button == &autoMapVelButton) autoMapVelocityLayers();
    else if (button == &autoMapRRButton) autoMapRoundRobin();
    else if (button == &clearMapButton) clearAllZones();
    else if (button == &saveMapButton) saveSampleMapToFile();
    else if (button == &loadMapButton) loadSampleMapFromFile();
    else if (button == &exportZipButton) exportSampleMapToZip();
    else if (button == &velLinBtn) applyVelocityPreset(0);
    else if (button == &velSoftBtn) applyVelocityPreset(1);
    else if (button == &velHardBtn) applyVelocityPreset(2);
    else if (button == &velSCurveBtn) applyVelocityPreset(3);
    else if (button == &velFixedBtn) applyVelocityPreset(4);
    else if (button == &velScopeButton)
    {
        velocityScopeIsZone = !velocityScopeIsZone;
        velScopeButton.setButtonText(velocityScopeIsZone ? "Scope: Zone" : "Scope: Global");
        updateVelocityCurveUI();
    }
    else if (button == &velTestAuditionBtn)
    {
        int noteToPlay = (selectedZoneIndex >= 0 && selectedZoneIndex < static_cast<int>(zones.size())) ? zones[selectedZoneIndex].rootNote : 60;
        triggerKeybedNote(noteToPlay, 100.0f / 127.0f, 100);
    }
}

bool SampleMapComponent::keyPressed(const juce::KeyPress& key)
{
    auto& sm = ShortcutManager::getInstance();

    if (sm.matches("map.delete_zone", key))
    {
        if (!selectedZoneIndices.empty() || selectedZoneIndex >= 0)
        {
            deleteSelectedZones();
            return true;
        }
    }

    if (selectedZoneIndices.empty())
        return false;

    int deltaNote = 0;
    int deltaVel = 0;

    if (sm.matches("map.nudge_down", key)) deltaNote = -1;
    else if (sm.matches("map.nudge_up", key)) deltaNote = 1;
    else if (sm.matches("map.velocity_up", key)) deltaVel = 4;
    else if (sm.matches("map.velocity_down", key)) deltaVel = -4;

    if (deltaNote != 0 || deltaVel != 0)
    {
        for (int idx : selectedZoneIndices)
        {
            if (idx >= 0 && idx < static_cast<int>(zones.size()))
            {
                if (deltaNote != 0)
                {
                    int span = zones[idx].keyHigh - zones[idx].keyLow;
                    int newLow = juce::jlimit(0, 127 - span, zones[idx].keyLow + deltaNote);
                    zones[idx].keyLow = newLow;
                    zones[idx].keyHigh = newLow + span;
                    zones[idx].rootNote = juce::jlimit(zones[idx].keyLow, zones[idx].keyHigh, zones[idx].rootNote + deltaNote);
                }
                if (deltaVel != 0)
                {
                    int span = zones[idx].velHigh - zones[idx].velLow;
                    int newLow = juce::jlimit(0, 127 - span, zones[idx].velLow + deltaVel);
                    zones[idx].velLow = newLow;
                    zones[idx].velHigh = newLow + span;
                }
            }
        }
        resized();
        repaint();
        if (onStateChanged) onStateChanged();
        return true;
    }

    return false;
}

SampleMapState SampleMapComponent::getState() const
{
    SampleMapState s;
    for (const auto& z : zones)
    {
        SampleMapZoneState zs;
        zs.filePath = z.filePath;
        zs.sampleName = z.sampleName;
        zs.rootNote = z.rootNote;
        zs.keyLow = z.keyLow;
        zs.keyHigh = z.keyHigh;
        zs.velLow = z.velLow;
        zs.velHigh = z.velHigh;
        zs.roundRobinIndex = z.roundRobinIndex;
        zs.fineTuneCents = z.fineTuneCents;
        zs.gainDb = z.gainDb;
        zs.attackMs = z.attackMs;
        zs.decayMs = z.decayMs;
        zs.sustainLevel = z.sustainLevel;
        zs.releaseMs = z.releaseMs;
        zs.velocitySensitivity = z.velocitySensitivity;
        s.zones.push_back(zs);
    }
    s.globalAttackMs = globalAttackMs;
    s.globalDecayMs = globalDecayMs;
    s.globalSustainLevel = globalSustainLevel;
    s.globalReleaseMs = globalReleaseMs;
    s.samplerReverbAmount = audioEngine.getSamplerReverbAmount();
    s.pitchTrackingEnabled = audioEngine.isPitchTrackingEnabled();
    s.roundRobinMode = roundRobinMode;
    s.midiChannel = midiChannelSetting;
    s.velocitySensitivity = globalVelocitySensitivity;
    s.velocityCurve = globalVelocityCurve;
    s.velocityCurveMode = globalVelocityCurveMode;
    s.velocityMinFloor = globalVelocityMinFloor;
    return s;
}

void SampleMapComponent::setState(const SampleMapState& state)
{
    zones.clear();
    selectedZoneIndex = -1;
    selectedZoneIndices.clear();

    for (const auto& zs : state.zones)
    {
        SampleMapZone z;
        z.filePath = zs.filePath;
        z.sampleName = zs.sampleName;
        z.rootNote = zs.rootNote;
        z.keyLow = zs.keyLow;
        z.keyHigh = zs.keyHigh;
        z.velLow = zs.velLow;
        z.velHigh = zs.velHigh;
        z.roundRobinIndex = zs.roundRobinIndex;
        z.fineTuneCents = zs.fineTuneCents;
        z.gainDb = zs.gainDb;
        z.attackMs = zs.attackMs;
        z.decayMs = zs.decayMs;
        z.sustainLevel = zs.sustainLevel;
        z.releaseMs = zs.releaseMs;
        z.velocitySensitivity = zs.velocitySensitivity;
        z.isSelected = false;
        zones.push_back(z);
    }

    globalAttackMs = state.globalAttackMs;
    globalDecayMs = state.globalDecayMs;
    globalSustainLevel = state.globalSustainLevel;
    globalReleaseMs = state.globalReleaseMs;

    attackKnob.setValue(globalAttackMs, juce::dontSendNotification);
    decayKnob.setValue(globalDecayMs, juce::dontSendNotification);
    sustainKnob.setValue(globalSustainLevel, juce::dontSendNotification);
    releaseKnob.setValue(globalReleaseMs, juce::dontSendNotification);

    reverbSlider.setValue(state.samplerReverbAmount, juce::dontSendNotification);
    audioEngine.setSamplerReverbAmount(state.samplerReverbAmount);

    audioEngine.setPitchTrackingEnabled(state.pitchTrackingEnabled);
    pitchTrackButton.setToggleState(state.pitchTrackingEnabled, juce::dontSendNotification);
    pitchTrackButton.setButtonText(state.pitchTrackingEnabled ? "Pitch Track: ON" : "Pitch Track: OFF");

    roundRobinMode = state.roundRobinMode;
    roundRobinButton.setButtonText(roundRobinMode == 0 ? "RR: Cycle" : (roundRobinMode == 1 ? "RR: Random" : "RR: OFF"));

    midiChannelSetting = state.midiChannel > 0 ? state.midiChannel : 2;
    updateMidiChannelButtonText();

    globalVelocitySensitivity = state.velocitySensitivity;
    globalVelocityCurve = state.velocityCurve;
    globalVelocityCurveMode = state.velocityCurveMode;
    globalVelocityMinFloor = state.velocityMinFloor;
    updateVelocityCurveUI();

    if (!zones.empty())
    {
        std::vector<juce::File> filesToPreload;
        for (const auto& z : zones)
        {
            if (z.filePath.isNotEmpty())
            {
                juce::File f(z.filePath);
                if (f.existsAsFile())
                    filesToPreload.push_back(f);
            }
        }
        if (!filesToPreload.empty())
        {
            audioEngine.preloadSampleFiles(filesToPreload);
        }

        selectedZoneIndex = 0;
        selectedZoneIndices.insert(0);
        juce::File firstSlice(zones[0].filePath);
        if (firstSlice.existsAsFile())
        {
            audioEngine.loadFile(firstSlice, false, true);
        }
    }

    resized();
    repaint();
}

void SampleMapComponent::openLorisResynthesisDialog()
{
    juce::File sourceFile;

    // 1. Check if a zone is currently selected
    if (selectedZoneIndex >= 0 && selectedZoneIndex < static_cast<int>(zones.size()))
    {
        sourceFile = juce::File(zones[selectedZoneIndex].filePath);
    }

    // 2. Otherwise check if AudioEngine has a loaded file
    if (!sourceFile.existsAsFile())
    {
        sourceFile = audioEngine.getCurrentFile();
    }

    if (sourceFile.existsAsFile())
    {
        openLorisResynthesisForSample(sourceFile);
    }
    else
    {
        // Prompt user to select an audio file to resynthesize
        auto chooser = std::make_shared<juce::FileChooser>(
            "Select Audio Sample for Loris Resynthesis",
            juce::File::getSpecialLocation(juce::File::userDesktopDirectory),
            "*.wav;*.mp3;*.flac;*.ogg;*.aif;*.aiff;*.aifc;*.WAV;*.MP3;*.FLAC;*.OGG;*.AIF;*.AIFF;*.AIFC",
            true);

        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this, chooser](const juce::FileChooser& fc) {
            auto result = fc.getResult();
            if (result.existsAsFile())
            {
                openLorisResynthesisForSample(result);
            }
        });
    }
}

void SampleMapComponent::openLorisResynthesisForSample(const juce::File& file)
{
    if (!file.existsAsFile()) return;

    juce::AudioFormatManager formatMgr;
    formatMgr.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formatMgr.createReaderFor(file));
    if (reader == nullptr) return;

    juce::AudioBuffer<float> buf((int)reader->numChannels, (int)reader->lengthInSamples);
    reader->read(&buf, 0, (int)reader->lengthInSamples, 0, true, true);
    double sr = reader->sampleRate;
    int detectedRoot = LorisResynthesizer::detectRootMidiNote(buf, sr);

    LorisResynthesisDialog::showDialog(this, file, buf, sr, detectedRoot, [this](const std::vector<ResynthesizedZone>& generatedZones) {
        applyResynthesizedZones(generatedZones);
    });
}

void SampleMapComponent::applyResynthesizedZones(const std::vector<ResynthesizedZone>& generatedZones)
{
    if (generatedZones.empty()) return;

    clearAllZones();

    for (const auto& gz : generatedZones)
    {
        if (gz.audioFile.existsAsFile())
        {
            audioEngine.loadFile(gz.audioFile, false, true);

            SampleMapZone z;
            z.filePath = gz.audioFile.getFullPathName();
            z.sampleName = gz.sampleName;
            z.rootNote = gz.rootNote;
            z.keyLow = gz.keyLow;
            z.keyHigh = gz.keyHigh;
            z.velLow = 0;
            z.velHigh = 127;
            z.roundRobinIndex = 1;
            z.fineTuneCents = 0.0f;
            z.gainDb = 0.0f;
            z.attackMs = 5.0f;
            z.decayMs = 100.0f;
            z.sustainLevel = 1.0f;
            z.releaseMs = 200.0f;
            z.isSelected = false;

            zones.push_back(z);
        }
    }

    if (!zones.empty())
    {
        selectedZoneIndex = 0;
        selectedZoneIndices.clear();
        selectedZoneIndices.insert(0);
        zones[0].isSelected = true;
    }

    resized();
    repaint();
    if (onStateChanged) onStateChanged();
}

} // namespace openwav


