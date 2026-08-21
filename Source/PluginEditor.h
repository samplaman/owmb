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
#include "UI/RecorderComponent.h"
#include "UI/ScanProgressDialog.h"
#include "UI/AboutDialog.h"
#include "UI/SimilarityGraphPopup.h"
#include "UI/AnalysisComponent.h"
#include "UI/EditComponent.h"
#include "UI/SampleMapComponent.h"
#include "UI/PerformanceComponent.h"

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
    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;
    void triggerExitSequence();

    // HeaderBarListener Callbacks
    void searchTextChanged(const juce::String& newText) override;
    void formatFilterChanged(const juce::String& extension) override;
    void addFolderRequested() override;
    void rescanRequested() override;
    void settingsRequested() override;
    void viewModeChanged(ViewMode mode) override;
    void searchBarUpPressed() override;
    void searchBarDownPressed() override;

    // TagPanelListener Callbacks
    void tagFilterSelectionChanged(const std::set<juce::String>& selectedTags, bool matchAllTags, bool favoritesOnly) override;

    // SampleTableListener Callbacks
    void sampleSelected(const MediaItem& item) override;
    void sampleDoubleClicked(const MediaItem& item) override;
    void displayedItemsChanged(const std::vector<MediaItem>& items) override;
    void addToSampleMapRequested(const MediaItem& item) override;
    void autoSliceToSamplerRequested(const MediaItem& item) override;

    // SampleCloudListener Callbacks
    void cloudSampleSelected(const MediaItem& item) override;
    void cloudSampleDoubleClicked(const MediaItem& item) override;

    // juce::FileDragAndDropTarget Callbacks
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    void parentHierarchyChanged() override;
    bool keyPressed(const juce::KeyPress& key) override;
    
    void setTagPanelWidth(int newWidth);
    void applyUiScale(float scale);

private:
    void triggerFilterUpdate();
    void updateNativeTitleBarTheme();
    void timerCallback() override;

    bool uiReady = false;
    bool isExiting = false;

    OpenWavAudioProcessor& audioProcessor;
    OpenWavLookAndFeel lookAndFeel;

    HeaderBarComponent headerBar;
    TagPanelComponent tagPanel;

    class LeftPanelResizerBar : public juce::Component
    {
    public:
        explicit LeftPanelResizerBar(OpenWavAudioProcessorEditor& ownerComponent)
            : owner(ownerComponent)
        {
            setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
        }

        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseDrag(const juce::MouseEvent& e) override;

    private:
        OpenWavAudioProcessorEditor& owner;
        int dragStartX { 0 };
        int startWidth { 348 };
    };

    LeftPanelResizerBar leftPanelResizer;
    int tagPanelWidth { 348 };

    SampleTableComponent sampleTable;
    SampleCloudComponent sampleCloud;
    LibrariesComponent librariesComponent;
    RecorderComponent recorderComponent;
    WaveformTransportComponent waveformTransport;
    ScanProgressDialog scanProgressDialog;
    AboutDialog aboutDialog;
    SimilarityGraphPopup similarityGraphPopup;
    AnalysisComponent analysisComponent;
    EditComponent editComponent;
    SampleMapComponent sampleMapComponent;
    PerformanceComponent performanceComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenWavAudioProcessorEditor)
};

} // namespace openwav
