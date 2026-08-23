#include "SliceConfigComponent.h"

namespace openwav
{

SliceConfigComponent::SliceConfigComponent(AudioEngine& engine, std::function<void(const std::vector<double>&)> onApplySlices)
    : audioEngine(engine), onApplyCallback(std::move(onApplySlices))
{
    addAndMakeVisible(titleLabel);
    titleLabel.setFont(juce::Font(16.0f).boldened());
    titleLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);

    addAndMakeVisible(sampleNameLabel);
    sampleNameLabel.setFont(juce::Font(13.0f));
    sampleNameLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    sampleNameLabel.setText("File: " + audioEngine.getCurrentFile().getFileName(), juce::dontSendNotification);

    addAndMakeVisible(sliceCountLabel);
    sliceCountLabel.setFont(juce::Font(12.5f).boldened());
    sliceCountLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textPrimary);

    addAndMakeVisible(sliceCountSlider);
    sliceCountSlider.setRange(2, 64, 1);
    sliceCountSlider.setValue(16, juce::dontSendNotification);
    sliceCountSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    sliceCountSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 45, 24);
    sliceCountSlider.onValueChange = [this] {
        if (modeSelector.getSelectedId() != 3)
        {
            recomputeSlices();
        }
    };

    auto setupPreset = [this](juce::TextButton& btn, int val) {
        addAndMakeVisible(btn);
        btn.onClick = [this, val] {
            if (modeSelector.getSelectedId() == 3)
            {
                modeSelector.setSelectedId(1, juce::dontSendNotification);
            }
            sliceCountSlider.setValue(val);
        };
    };

    setupPreset(preset4Button, 4);
    setupPreset(preset8Button, 8);
    setupPreset(preset16Button, 16);
    setupPreset(preset32Button, 32);
    setupPreset(preset40Button, 40);

    addAndMakeVisible(modeLabel);
    modeLabel.setFont(juce::Font(12.5f).boldened());
    modeLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textPrimary);

    addAndMakeVisible(modeSelector);
    modeSelector.addItem("Transient Onsets", 1);
    modeSelector.addItem("Equal Divisions", 2);
    modeSelector.addItem("Manual (Click/Drag)", 3);
    modeSelector.setSelectedId(1, juce::dontSendNotification);
    modeSelector.onChange = [this] {
        if (modeSelector.getSelectedId() != 3)
        {
            recomputeSlices();
        }
    };

    addAndMakeVisible(helperLabel);
    helperLabel.setFont(juce::Font(11.0f));
    helperLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    helperLabel.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(applyButton);
    applyButton.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::accentCyan.darker(0.3f));
    applyButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    applyButton.onClick = [this] {
        if (onApplyCallback)
        {
            onApplyCallback(previewSliceRatios);
        }
        closeWindow();
    };

    addAndMakeVisible(cancelButton);
    cancelButton.onClick = [this] {
        closeWindow();
    };

    recomputeSlices();
}

juce::Rectangle<float> SliceConfigComponent::getWaveformRect() const
{
    auto area = getLocalBounds().reduced(16);
    area.removeFromTop(100); // Header + controls space
    area.removeFromBottom(45); // Buttons space
    return area.toFloat().reduced(8.0f);
}

int SliceConfigComponent::findSliceNearX(float x, float tolerance) const
{
    auto wfRect = getWaveformRect();
    if (wfRect.isEmpty()) return -1;

    float totalW = wfRect.getWidth();
    float startX = wfRect.getX();

    for (size_t i = 0; i < previewSliceRatios.size(); ++i)
    {
        float sx = startX + totalW * static_cast<float>(previewSliceRatios[i]);
        if (std::abs(x - sx) <= tolerance)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void SliceConfigComponent::mouseDown(const juce::MouseEvent& e)
{
    auto wfRect = getWaveformRect();
    if (!wfRect.contains(e.position.x, e.position.y))
        return;

    int nearIdx = findSliceNearX(e.position.x, 10.0f);

    if (e.mods.isRightButtonDown() || e.mods.isPopupMenu())
    {
        if (nearIdx > 0 && nearIdx < static_cast<int>(previewSliceRatios.size()))
        {
            previewSliceRatios.erase(previewSliceRatios.begin() + nearIdx);
            modeSelector.setSelectedId(3, juce::dontSendNotification);
            sliceCountSlider.setValue(previewSliceRatios.size(), juce::dontSendNotification);
            repaint();
        }
        return;
    }

    if (nearIdx >= 0)
    {
        draggingSliceIndex = nearIdx;
    }
    else
    {
        double clickRatio = static_cast<double>(e.position.x - wfRect.getX()) / static_cast<double>(wfRect.getWidth());
        clickRatio = juce::jlimit(0.005, 0.995, clickRatio);

        previewSliceRatios.push_back(clickRatio);
        std::sort(previewSliceRatios.begin(), previewSliceRatios.end());
        previewSliceRatios.erase(std::unique(previewSliceRatios.begin(), previewSliceRatios.end()), previewSliceRatios.end());

        modeSelector.setSelectedId(3, juce::dontSendNotification);
        sliceCountSlider.setValue(previewSliceRatios.size(), juce::dontSendNotification);

        draggingSliceIndex = findSliceNearX(e.position.x, 12.0f);
        repaint();
    }
}

void SliceConfigComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (draggingSliceIndex <= 0 || draggingSliceIndex >= static_cast<int>(previewSliceRatios.size()))
        return;

    auto wfRect = getWaveformRect();
    if (wfRect.isEmpty()) return;

    double newRatio = static_cast<double>(e.position.x - wfRect.getX()) / static_cast<double>(wfRect.getWidth());
    newRatio = juce::jlimit(0.001, 0.999, newRatio);

    previewSliceRatios[static_cast<size_t>(draggingSliceIndex)] = newRatio;
    modeSelector.setSelectedId(3, juce::dontSendNotification);
    repaint();
}

void SliceConfigComponent::mouseUp(const juce::MouseEvent& /*e*/)
{
    if (draggingSliceIndex >= 0)
    {
        draggingSliceIndex = -1;
        std::sort(previewSliceRatios.begin(), previewSliceRatios.end());
        sliceCountSlider.setValue(previewSliceRatios.size(), juce::dontSendNotification);
        repaint();
    }
}

void SliceConfigComponent::mouseMove(const juce::MouseEvent& e)
{
    auto wfRect = getWaveformRect();
    if (wfRect.contains(e.position.x, e.position.y))
    {
        int nearIdx = findSliceNearX(e.position.x, 10.0f);
        if (nearIdx != hoveredSliceIndex)
        {
            hoveredSliceIndex = nearIdx;
            repaint();
        }

        if (nearIdx >= 0)
        {
            setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
        }
        else
        {
            setMouseCursor(juce::MouseCursor::CrosshairCursor);
        }
    }
    else
    {
        if (hoveredSliceIndex != -1)
        {
            hoveredSliceIndex = -1;
            setMouseCursor(juce::MouseCursor::NormalCursor);
            repaint();
        }
    }
}

void SliceConfigComponent::recomputeSlices()
{
    int targetCount = static_cast<int>(sliceCountSlider.getValue());
    targetCount = juce::jlimit(2, 64, targetCount);

    previewSliceRatios.clear();

    int mode = modeSelector.getSelectedId();
    if (mode == 2) // Equal Divisions
    {
        previewSliceRatios.push_back(0.0);
        for (int i = 1; i < targetCount; ++i)
        {
            previewSliceRatios.push_back(static_cast<double>(i) / static_cast<double>(targetCount));
        }
    }
    else // Transient Onsets
    {
        juce::AudioBuffer<float> buffer;
        double sampleRate = 44100.0;

        juce::File file = audioEngine.getCurrentFile();
        if (file.existsAsFile())
        {
            juce::AudioFormatManager formatManager;
            formatManager.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
            if (reader != nullptr && reader->lengthInSamples > 0)
            {
                sampleRate = reader->sampleRate;
                buffer.setSize(static_cast<int>(reader->numChannels), static_cast<int>(reader->lengthInSamples));
                reader->read(&buffer, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);
            }
        }

        int numSamples = buffer.getNumSamples();
        if (numSamples <= 0)
        {
            previewSliceRatios.push_back(0.0);
            for (int i = 1; i < targetCount; ++i)
            {
                previewSliceRatios.push_back(static_cast<double>(i) / static_cast<double>(targetCount));
            }
            repaint();
            return;
        }

        int channels = buffer.getNumChannels();
        int hopSize = static_cast<int>(sampleRate * 0.005);
        if (hopSize < 64) hopSize = 64;

        std::vector<float> energy;
        energy.reserve(static_cast<size_t>(numSamples / hopSize));

        for (int s = 0; s < numSamples; s += hopSize)
        {
            int numToRead = std::min(hopSize, numSamples - s);
            float sumSquare = 0.0f;
            for (int ch = 0; ch < channels; ++ch)
            {
                const float* ptr = buffer.getReadPointer(ch, s);
                for (int i = 0; i < numToRead; ++i)
                {
                    sumSquare += ptr[i] * ptr[i];
                }
            }
            energy.push_back(std::sqrt(sumSquare / static_cast<float>(numToRead * channels)));
        }

        struct OnsetCandidate {
            int samplePos;
            float strength;
        };

        std::vector<OnsetCandidate> candidatePeaks;
        if (energy.size() >= 3)
        {
            for (size_t i = 1; i < energy.size() - 1; ++i)
            {
                float diff = energy[i] - energy[i - 1];
                if (diff > 0.005f && energy[i] > energy[i + 1])
                {
                    candidatePeaks.push_back({ static_cast<int>(i * hopSize), diff });
                }
            }
        }

        int maxCandidatesNeeded = targetCount - 1;
        if (candidatePeaks.size() > static_cast<size_t>(maxCandidatesNeeded))
        {
            std::sort(candidatePeaks.begin(), candidatePeaks.end(), [](const OnsetCandidate& a, const OnsetCandidate& b) {
                return a.strength > b.strength;
            });
            candidatePeaks.resize(static_cast<size_t>(maxCandidatesNeeded));
        }

        std::vector<int> slicePositions;
        slicePositions.push_back(0);
        for (const auto& c : candidatePeaks)
        {
            slicePositions.push_back(c.samplePos);
        }

        std::sort(slicePositions.begin(), slicePositions.end());
        slicePositions.erase(std::unique(slicePositions.begin(), slicePositions.end()), slicePositions.end());

        if (slicePositions.size() < static_cast<size_t>(targetCount))
        {
            int missing = targetCount - static_cast<int>(slicePositions.size());
            for (int k = 1; k <= missing; ++k)
            {
                slicePositions.push_back(static_cast<int>((static_cast<double>(k) / static_cast<double>(missing + 1)) * numSamples));
            }
            std::sort(slicePositions.begin(), slicePositions.end());
            slicePositions.erase(std::unique(slicePositions.begin(), slicePositions.end()), slicePositions.end());
        }

        for (int pos : slicePositions)
        {
            previewSliceRatios.push_back(static_cast<double>(pos) / static_cast<double>(numSamples));
        }
    }

    repaint();
}

void SliceConfigComponent::closeWindow()
{
    if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
    {
        dw->exitModalState(0);
    }
}

void SliceConfigComponent::paint(juce::Graphics& g)
{
    g.fillAll(OpenWavLookAndFeel::bgDark);

    auto wfRect = getWaveformRect();

    // Waveform Preview Track Box
    g.setColour(OpenWavLookAndFeel::bgCard);
    g.fillRoundedRectangle(wfRect, 6.0f);
    g.setColour(OpenWavLookAndFeel::borderColour);
    g.drawRoundedRectangle(wfRect, 6.0f, 1.0f);

    float totalW = wfRect.getWidth();
    float centerY = wfRect.getCentreY();
    float halfH = wfRect.getHeight() * 0.45f;

    // Draw zero line
    g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.4f));
    g.drawHorizontalLine(static_cast<int>(centerY), wfRect.getX(), wfRect.getRight());

    // Draw waveform peaks
    auto peaks = audioEngine.getWaveformPeaks();
    int numPoints = peaks.numPoints;

    if (numPoints > 0 && peaks.minLeft.size() == static_cast<size_t>(numPoints))
    {
        g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.85f));
        int numPixels = static_cast<int>(totalW);
        for (int px = 0; px < numPixels; ++px)
        {
            float x = wfRect.getX() + static_cast<float>(px);
            int pIdx = juce::jlimit(0, numPoints - 1, static_cast<int>((static_cast<double>(px) / totalW) * numPoints));

            float maxL = peaks.maxLeft[static_cast<size_t>(pIdx)];
            float minL = peaks.minLeft[static_cast<size_t>(pIdx)];
            float height = (maxL - minL) * halfH;
            if (height < 1.0f) height = 1.0f;

            g.drawVerticalLine(juce::roundToInt(x), centerY - height * 0.5f, centerY + height * 0.5f);
        }
    }

    // Draw vertical slice lines & handles
    for (size_t i = 0; i < previewSliceRatios.size(); ++i)
    {
        float sx = wfRect.getX() + totalW * static_cast<float>(previewSliceRatios[i]);
        bool isHovered = (hoveredSliceIndex == static_cast<int>(i));
        bool isDragging = (draggingSliceIndex == static_cast<int>(i));

        if (isDragging)
        {
            g.setColour(juce::Colours::white);
            g.drawVerticalLine(juce::roundToInt(sx), wfRect.getY(), wfRect.getBottom());
        }
        else if (isHovered)
        {
            g.setColour(OpenWavLookAndFeel::accentCyan);
            g.drawVerticalLine(juce::roundToInt(sx), wfRect.getY(), wfRect.getBottom());
        }
        else
        {
            g.setColour(juce::Colours::yellow);
            g.drawVerticalLine(juce::roundToInt(sx), wfRect.getY(), wfRect.getBottom());
        }

        // Slice number badge
        g.setFont(juce::Font(9.0f).boldened());
        g.setColour(isHovered ? OpenWavLookAndFeel::accentCyan : juce::Colours::black);
        g.fillRect(sx, wfRect.getY(), 14.0f, 12.0f);
        g.setColour(isHovered ? juce::Colours::black : juce::Colours::yellow);
        g.drawText(juce::String(i + 1), sx, wfRect.getY(), 14, 12, juce::Justification::centred, false);
    }
}

void SliceConfigComponent::resized()
{
    auto area = getLocalBounds().reduced(16);

    auto topRow = area.removeFromTop(24);
    titleLabel.setBounds(topRow.removeFromLeft(240));
    sampleNameLabel.setBounds(topRow);

    area.removeFromTop(6);

    auto controlsRow = area.removeFromTop(32);
    sliceCountLabel.setBounds(controlsRow.removeFromLeft(48));
    sliceCountSlider.setBounds(controlsRow.removeFromLeft(160));
    controlsRow.removeFromLeft(6);

    preset4Button.setBounds(controlsRow.removeFromLeft(28));
    controlsRow.removeFromLeft(4);
    preset8Button.setBounds(controlsRow.removeFromLeft(28));
    controlsRow.removeFromLeft(4);
    preset16Button.setBounds(controlsRow.removeFromLeft(28));
    controlsRow.removeFromLeft(4);
    preset32Button.setBounds(controlsRow.removeFromLeft(28));
    controlsRow.removeFromLeft(4);
    preset40Button.setBounds(controlsRow.removeFromLeft(28));
    controlsRow.removeFromLeft(10);

    modeLabel.setBounds(controlsRow.removeFromLeft(45));
    modeSelector.setBounds(controlsRow.removeFromLeft(130));

    area.removeFromTop(4);
    helperLabel.setBounds(area.removeFromTop(18));

    auto bottomRow = area.removeFromBottom(36);
    cancelButton.setBounds(bottomRow.removeFromRight(90));
    bottomRow.removeFromRight(10);
    applyButton.setBounds(bottomRow.removeFromRight(120));
}

} // namespace openwav
