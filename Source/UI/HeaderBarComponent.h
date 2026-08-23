#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_gui_extra/juce_gui_extra.h>
#endif
#include "../Database/TagDatabaseManager.h"
#include "../Scanner/LibraryScanner.h"

namespace openwav
{

enum class ViewMode
{
    List,
    Cloud,
    Libraries,
    Record,
    Analysis,
    Edit,
    SampleMap
};

class HeaderBarListener
{
public:
    virtual ~HeaderBarListener() = default;
    virtual void searchTextChanged(const juce::String& newText) = 0;
    virtual void formatFilterChanged(const juce::String& extension) = 0;
    virtual void addFolderRequested() = 0;
    virtual void rescanRequested() = 0;
    virtual void settingsRequested() = 0;
    virtual void viewModeChanged(ViewMode mode) = 0;
    virtual void searchBarUpPressed() {}
    virtual void searchBarDownPressed() {}
};

class HeaderBarComponent : public juce::Component,
                           public juce::TextEditor::Listener,
                           public ScannerListener,
                           public juce::KeyListener
{
public:
    HeaderBarComponent(TagDatabaseManager& db, LibraryScanner& scanner);
    ~HeaderBarComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void lookAndFeelChanged() override;

    juce::String getSearchText() const { return searchEditor.getText(); }
    void setSearchText(const juce::String& text);
    juce::String getSelectedFormat() const { return activeFormat; }
    ViewMode getCurrentViewMode() const { return currentViewMode; }
    bool isCloudViewActive() const { return currentViewMode == ViewMode::Cloud; }
    int getSearchEditorRight() const { return searchEditor.getRight(); }

    void addListener(HeaderBarListener* listener);
    void removeListener(HeaderBarListener* listener);

    // ScannerListener Callbacks
    void scanStarted() override;
    void scanProgress(int filesProcessed, int totalFiles, const juce::String& currentFile) override;
    void scanFinished(int totalFilesDiscovered) override;
    void setViewMode(ViewMode mode);

private:
    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) override;
    void textEditorTextChanged(juce::TextEditor& editor) override;
    void setFormatFilter(const juce::String& ext, juce::TextButton* targetBtn);

    TagDatabaseManager& dbManager;
    LibraryScanner& libraryScanner;

    juce::ImageComponent logoComponent;
    juce::Label titleLabel { {}, "OWMB" };
    juce::TextEditor searchEditor;
    juce::TextButton addFolderButton { "+ Add Folder" };
    juce::TextButton rescanButton { "Rescan" };
    juce::TextButton settingsButton { "Settings" };

    juce::TextButton btnAll { "All" };
    juce::TextButton btnWav { ".WAV" };
    juce::TextButton btnMp3 { ".MP3" };
    juce::TextButton btnFlac { ".FLAC" };
    juce::TextButton btnOgg { ".OGG" };
    juce::TextButton btnAiff { ".AIFF" };

    juce::TextButton btnListView { "List" };
    juce::TextButton btnCloudView { "Cloud" };
    juce::TextButton btnLibrariesView { "Library" };
    juce::TextButton btnRecordView { "Record" };
    juce::TextButton btnAnalysisView { "Analysis" };
    juce::TextButton btnEditView { "Edit" };
    juce::TextButton btnSampleMapView { "Sample Map" };

    juce::String activeFormat { "All" };
    ViewMode currentViewMode { ViewMode::List };
    juce::ListenerList<HeaderBarListener> listeners;
};

} // namespace openwav
