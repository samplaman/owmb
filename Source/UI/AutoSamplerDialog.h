#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_gui_basics/juce_gui_basics.h>
#endif

#include "../Audio/AutoSamplerEngine.h"
#include "../Models/PluginState.h"
#include <functional>
#include <memory>
#include <vector>

namespace openwav
{

class AutoSamplerDialog : public juce::Component
{
public:
    AutoSamplerDialog(std::function<void(const SampleMapState& resultState)> onComplete);
    ~AutoSamplerDialog() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    static void showAsync(juce::Component* parent,
                          std::function<void(const SampleMapState& resultState)> onComplete);

private:
    class PluginEditorWindow : public juce::DocumentWindow
    {
    public:
        PluginEditorWindow(const juce::String& name, juce::AudioProcessorEditor* editor)
            : DocumentWindow(name, juce::Colour(0xFF141820), DocumentWindow::closeButton)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(editor, true);
            setResizable(editor->isResizable(), false);
            centreWithSize(getWidth(), getHeight());
            setVisible(true);
        }

        void closeButtonPressed() override
        {
            setVisible(false);
        }
    };

    void refreshPluginCombo();
    void loadSelectedPlugin();
    void openPluginEditor();
    void updateEstimateLabel();
    void setControlsEnabled(bool enabled);
    void startSampling();

    AutoSamplerEngine engine;
    std::vector<juce::PluginDescription> instrumentPlugins;
    std::unique_ptr<juce::AudioPluginInstance> loadedPlugin;
    std::unique_ptr<PluginEditorWindow> pluginEditorWindow;
    std::function<void(const SampleMapState&)> onCompleteCallback;

    juce::Label titleLabel { {}, "VST3 / AU Auto-Sampler" };

    // Plugin selection row
    juce::Label pluginLabel { {}, "Instrument:" };
    juce::ComboBox pluginCombo;
    juce::TextButton scanPluginsBtn { "Scan" };
    juce::TextButton openEditorBtn { "Open UI" };

    // Instrument name & template
    juce::Label nameLabel { {}, "Name:" };
    juce::TextEditor nameEditor;
    juce::Label templateLabel { {}, "Template:" };
    juce::ComboBox templateCombo;

    // Key range & stride
    juce::Label minNoteLabel { {}, "Key Low:" };
    juce::ComboBox minNoteCombo;
    juce::Label maxNoteLabel { {}, "Key High:" };
    juce::ComboBox maxNoteCombo;
    juce::Label strideLabel { {}, "Stride:" };
    juce::ComboBox strideCombo;

    // Velocity & RR
    juce::Label velLayersLabel { {}, "Vel Layers:" };
    juce::ComboBox velLayersCombo;
    juce::Label rrLabel { {}, "Round Robin:" };
    juce::ComboBox rrCombo;

    // Timing & audio
    juce::Label sustainLabel { {}, "Sustain (s):" };
    juce::Slider sustainSlider;
    juce::Label tailLabel { {}, "Max Tail (s):" };
    juce::Slider tailSlider;
    juce::Label srLabel { {}, "Sample Rate:" };
    juce::ComboBox srCombo;
    juce::Label normLabel { {}, "Normalize:" };
    juce::ComboBox normCombo;

    juce::ToggleButton autoTailToggle { "Auto-Detect Tail Silence" };
    juce::ToggleButton trimSilenceToggle { "Trim Leading Silence" };
    juce::ToggleButton exportSfzToggle { "Export .sfz" };
    juce::ToggleButton captureReleaseToggle { "Capture Note-Off (Release)" };

    // Estimate & progress
    juce::Label estimateLabel;
    double progressValue { 0.0 };
    juce::ProgressBar progressBar { progressValue };
    juce::Label statusLabel { {}, "Select a VST3 or AU instrument plugin to begin." };

    juce::TextButton startBtn { "Start Auto-Sample" };
    juce::TextButton cancelBtn { "Close" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AutoSamplerDialog)
};

} // namespace openwav
