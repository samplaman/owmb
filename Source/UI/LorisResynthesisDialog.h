#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_gui_basics/juce_gui_basics.h>
#endif

#include "../Audio/LorisResynthesizer.h"
#include "../Models/MediaItem.h"

namespace openwav
{

class LorisResynthesisDialog : public juce::Component,
                               public juce::Button::Listener,
                               public juce::ComboBox::Listener
{
public:
    explicit LorisResynthesisDialog(std::function<void(const std::vector<ResynthesizedZone>&)> onFinishedCallback);
    ~LorisResynthesisDialog() override;

    void setSourceSample(const juce::File& file, const juce::AudioBuffer<float>& buffer, double sampleRate, int initialRootNote = 60);
    void setSourceMediaItem(const MediaItem& item, const juce::AudioBuffer<float>& buffer, double sampleRate);

    void paint(juce::Graphics& g) override;
    void resized() override;

    void buttonClicked(juce::Button* button) override;
    void comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) override;

    static void showDialog(juce::Component* parentComponent,
                           const juce::File& sourceFile,
                           const juce::AudioBuffer<float>& buffer,
                           double sampleRate,
                           int rootNote,
                           std::function<void(const std::vector<ResynthesizedZone>&)> onFinished);

private:
    void populateNoteComboBox(juce::ComboBox& box, int defaultNote);
    void startResynthesis();
    void updateStatus(float progress, const juce::String& text);

    std::function<void(const std::vector<ResynthesizedZone>&)> onFinished;

    LorisResynthesizer resynthesizer;
    LorisResynthesisConfig config;

    // UI Components
    juce::Label titleLabel;
    juce::Label sampleInfoLabel;

    // Root Note
    juce::Label rootNoteLabel;
    juce::ComboBox rootNoteBox;
    juce::TextButton autoDetectBtn { "Auto Detect" };

    // Range & Stride
    juce::Label minNoteLabel;
    juce::ComboBox minNoteBox;
    juce::Label maxNoteLabel;
    juce::ComboBox maxNoteBox;
    juce::Label strideLabel;
    juce::ComboBox strideBox;

    // Settings
    juce::ToggleButton formantLockToggle { "Preserve Acoustic Formants (Natural Sound)" };
    juce::Label qualityLabel;
    juce::ComboBox qualityBox;

    // Progress
    juce::ProgressBar progressBar;
    double progressValue { 0.0 };
    juce::Label statusLabel;

    // Buttons
    juce::TextButton startBtn { "Generate Resynthesized Bank" };
    juce::TextButton cancelBtn { "Cancel" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LorisResynthesisDialog)
};

} // namespace openwav
