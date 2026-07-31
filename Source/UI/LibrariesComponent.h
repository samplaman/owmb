#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_gui_basics/juce_gui_basics.h>
 #include <juce_gui_extra/juce_gui_extra.h>
#endif

#include "../Database/TagDatabaseManager.h"
#include "../Scanner/LibraryScanner.h"
#include "../Audio/AudioEngine.h"
#include <vector>
#include <map>
#include <set>
#include <atomic>
#include <functional>
#include <cstdint>
#include <memory>

namespace openwav
{

struct PixeldrainFile
{
    juce::String id;
    juce::String name;
    int64_t sizeBytes { 0 };
    juce::String dateUpload;
    juce::String mimeType;
    bool isWav { false };
    bool isDownloaded { false };
    bool isDownloading { false };
    double downloadProgress { 0.0 };
    juce::String localPath;
};

class LibrariesComponent : public juce::Component,
                           public juce::TableListBoxModel,
                           public juce::TextEditor::Listener
{
public:
    LibrariesComponent(TagDatabaseManager& db, LibraryScanner& scanner, AudioEngine& audio);
    ~LibrariesComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void lookAndFeelChanged() override;

    // juce::TableListBoxModel overrides
    int getNumRows() override;
    void paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected) override;
    void paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected) override;
    juce::Component* refreshComponentForCell(int rowNumber, int columnId, bool isRowSelected, juce::Component* existingComponentToUpdate) override;
    void cellDoubleClicked(int rowNumber, int columnId, const juce::MouseEvent& e) override;
    bool mayDragToExternalWindows() const override;

    // Pixeldrain API Operations
    void fetchUserFiles();
    void downloadFile(int displayedIndex);
    void downloadAllWavs();

private:
    void textEditorTextChanged(juce::TextEditor& editor) override;
    void filterRemoteFiles();
    void updateDownloadStatuses();

    TagDatabaseManager& dbManager;
    LibraryScanner& libraryScanner;
    AudioEngine& audioEngine;

    // Controls
    juce::ImageComponent pixeldrainLogoComponent;
    juce::Label apiKeyLabel { {}, "Key or Hotlink:" };
    juce::TextEditor apiKeyEditor;
    juce::TextButton connectButton { "Fetch Files" };
    juce::Label statusLabel;

    juce::Label searchLabel { {}, "Search:" };
    juce::TextEditor searchEditor;

    juce::Label saveDirLabel;
    juce::TextButton chooseDirButton { "Change Folder..." };
    juce::TextButton downloadAllWavsButton { "Download All Audio" };

    juce::TableListBox tableBox;

    std::vector<PixeldrainFile> allRemoteFiles;
    std::vector<PixeldrainFile> displayedFiles;

    std::atomic<bool> isFetching { false };
    juce::String statusText { "Ready. Enter your Pixeldrain API key to fetch account files." };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LibrariesComponent)
};

} // namespace openwav
