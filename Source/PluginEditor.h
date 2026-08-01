#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_audio_processors/juce_audio_processors.h>
#endif
#include "PluginProcessor.h"
#include "UI/OpenWavLookAndFeel.h"
#include "UI/HeaderBarComponent.h"
#include "UI/TagPanelComponent.h"
#include "UI/SampleTableComponent.h"
#include "UI/SampleCloudComponent.h"
#include "UI/LibrariesComponent.h"
#include "UI/WaveformTransportComponent.h"

namespace openwav
{

class OpenWavAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     public HeaderBarListener,
                                     public TagPanelListener,
                                     public SampleTableListener,
                                     public SampleCloudListener,
                                     public juce::FileDragAndDropTarget,
                                     private juce::Timer
{
public:
    explicit OpenWavAudioProcessorEditor(OpenWavAudioProcessor& p);
    ~OpenWavAudioProcessorEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // HeaderBarListener Callbacks
    void searchTextChanged(const juce::String& newText) override;
    void formatFilterChanged(const juce::String& extension) override;
    void addFolderRequested() override;
    void rescanRequested() override;
    void settingsRequested() override;
    void viewModeChanged(ViewMode mode) override;

    // TagPanelListener Callbacks
    void tagFilterSelectionChanged(const std::set<juce::String>& selectedTags, bool matchAllTags, bool favoritesOnly) override;

    // SampleTableListener Callbacks
    void sampleSelected(const MediaItem& item) override;
    void sampleDoubleClicked(const MediaItem& item) override;
    void displayedItemsChanged(const std::vector<MediaItem>& items) override;

    // SampleCloudListener Callbacks
    void cloudSampleSelected(const MediaItem& item) override;
    void cloudSampleDoubleClicked(const MediaItem& item) override;

    // juce::FileDragAndDropTarget Callbacks
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    void parentHierarchyChanged() override;

private:
    void triggerFilterUpdate();
    void updateNativeTitleBarTheme();
    void timerCallback() override;

    OpenWavAudioProcessor& audioProcessor;
    OpenWavLookAndFeel lookAndFeel;

    HeaderBarComponent headerBar;
    TagPanelComponent tagPanel;
    SampleTableComponent sampleTable;
    SampleCloudComponent sampleCloud;
    LibrariesComponent librariesComponent;
    WaveformTransportComponent waveformTransport;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenWavAudioProcessorEditor)
};

} // namespace openwav
