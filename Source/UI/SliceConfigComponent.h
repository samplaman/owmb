#pragma once

#include <JuceHeader.h>
#include "../Audio/AudioEngine.h"
#include "OpenWavLookAndFeel.h"

namespace openwav
{

class SliceConfigComponent : public juce::Component
{
public:
    SliceConfigComponent(AudioEngine& engine, std::function<void(const std::vector<double>&)> onApplySlices);
    ~SliceConfigComponent() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;

private:
    void recomputeSlices();
    void closeWindow();

    juce::Rectangle<float> getWaveformRect() const;
    int findSliceNearX(float x, float tolerance = 10.0f) const;

    AudioEngine& audioEngine;
    std::function<void(const std::vector<double>&)> onApplyCallback;

    juce::Label titleLabel { "title", "Auto & Manual Slice Configuration" };
    juce::Label sampleNameLabel { "sampleName", "" };

    juce::Label sliceCountLabel { "sliceLabel", "Slices:" };
    juce::Slider sliceCountSlider;

    juce::TextButton preset4Button { "4" };
    juce::TextButton preset8Button { "8" };
    juce::TextButton preset16Button { "16" };
    juce::TextButton preset32Button { "32" };
    juce::TextButton preset40Button { "40" };

    juce::Label modeLabel { "modeLabel", "Mode:" };
    juce::ComboBox modeSelector;
    juce::Label helperLabel { "helper", "Click waveform to add slice • Drag line to move • Right-click line to remove" };

    juce::TextButton applyButton { "Apply Slices" };
    juce::TextButton cancelButton { "Cancel" };

    std::vector<double> previewSliceRatios;
    int draggingSliceIndex { -1 };
    int hoveredSliceIndex { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SliceConfigComponent)
};

} // namespace openwav
