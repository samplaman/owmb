#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_gui_extra/juce_gui_extra.h>
#endif

#include "../Audio/AudioEngine.h"
#include "../Models/PluginState.h"
#include <functional>
#include <vector>

namespace openwav
{

class CueSheetComponent : public juce::Component,
                          public juce::TableListBoxModel,
                          private juce::TextEditor::Listener
{
public:
    explicit CueSheetComponent(AudioEngine& engine);
    ~CueSheetComponent() override;

    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

    void logUsage(const juce::String& filePath,
                  double inSeconds,
                  double outSeconds,
                  double fullDurationSeconds,
                  bool isPartialRegion,
                  const juce::String& sourceContext,
                  const juce::String& defaultNotes = {});

    void syncFromSampleMap(const SampleMapState& mapState);

    std::vector<CueSheetEntryState> getState() const { return entries; }
    void setState(const std::vector<CueSheetEntryState>& newEntries);

    std::function<void(const juce::String& filePath, double inSec, double outSec, bool isPartial)> onAuditionCue;
    std::function<void()> onSyncSampleMapRequested;

    // TableListBoxModel
    int getNumRows() override;
    void paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected) override;
    void paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected) override;
    juce::Component* refreshComponentForCell(int rowNumber, int columnId, bool isRowSelected,
                                             juce::Component* existingComponentToUpdate) override;
    void cellDoubleClicked(int rowNumber, int columnId, const juce::MouseEvent&) override;
    void selectedRowsChanged(int lastRowSelected) override;
    void sortOrderChanged(int newSortColumnId, bool isForwards) override;

private:
    enum ColumnId
    {
        ColIndex = 1,
        ColFileName,
        ColRegion,
        ColDuration,
        ColUsage,
        ColSceneTrack,
        ColNotes,
        ColFilePath,
        ColLoggedAt
    };

    void textEditorTextChanged(juce::TextEditor& editor) override;
    void rebuildFilteredIndices();
    void updateSummaryLabel();
    void updateActionStates();
    void logCurrentLoadedSample();
    void removeSelectedRows();
    void clearAllEntries();
    void exportCsv();

    static juce::String formatTimecode(double seconds);
    static juce::String escapeCsvField(const juce::String& field);

    AudioEngine& audioEngine;

    std::vector<CueSheetEntryState> entries;
    std::vector<int> filteredIndices;

    juce::Label titleLabel { {}, "Cue Sheet / Used Samples" };
    juce::Label summaryLabel;
    juce::TextEditor filterEditor;
    juce::TextButton logCurrentButton { "+ Log Current" };
    juce::TextButton syncSamplerButton { "Sync Sample Map" };
    juce::TextButton removeButton { "Remove" };
    juce::TextButton clearButton { "Clear" };
    juce::TextButton exportCsvButton { "Export CSV..." };

    juce::TableListBox table;
    int sortColumnId { ColIndex };
    bool sortForwards { true };
};

} // namespace openwav
