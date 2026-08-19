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
    bool isZip { false };
    bool isDownloaded { false };
    bool isDownloading { false };
    double downloadProgress { 0.0 };
    juce::String localPath;

    // Preview/Streaming Support
    bool isPreviewing { false };
    double previewProgress { 0.0 };
    juce::String previewPath;
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
    void cellClicked(int rowNumber, int columnId, const juce::MouseEvent& e) override;
    void selectedRowsChanged(int lastRowSelected) override;
    bool mayDragToExternalWindows() const override;

    // Pixeldrain API Operations
    void fetchUserFiles();
    void downloadFile(int displayedIndex);
    void downloadAllWavs();
    void previewFile(int displayedIndex);
    void handlePreviewFinished(const juce::String& fileId, const juce::File& previewFile, bool success);

private:
    void textEditorTextChanged(juce::TextEditor& editor) override;
    void filterRemoteFiles();
    void updateDownloadStatuses();

    TagDatabaseManager& dbManager;
    LibraryScanner& libraryScanner;
    AudioEngine& audioEngine;

    // Controls
    struct ClickableImageComponent : public juce::ImageComponent
    {
        ClickableImageComponent()
        {
            setMouseCursor(juce::MouseCursor::PointingHandCursor);
        }

        void mouseDown(const juce::MouseEvent& event) override
        {
            juce::URL("https://pixeldrain.com/").launchInDefaultBrowser();
        }
    };
    ClickableImageComponent pixeldrainLogoComponent;
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

    struct QueuedDownload
    {
        juce::String fileId;
        juce::String fileName;
        bool isZip { false };
    };
    juce::CriticalSection downloadQueueLock;
    std::vector<QueuedDownload> downloadQueue;

    class SequentialDownloader : public juce::Thread
    {
    public:
        SequentialDownloader(LibrariesComponent& owner);
        ~SequentialDownloader() override;
        void run() override;
    private:
        LibrariesComponent& owner;
    };
    std::unique_ptr<SequentialDownloader> sequentialDownloader;

    static bool downloadFileSync(const juce::String& fileId,
                                 const juce::String& fileName,
                                 const juce::String& apiKey,
                                 const juce::File& destFile,
                                 std::function<bool()> shouldExit,
                                 juce::Component::SafePointer<LibrariesComponent> safeThis,
                                 bool isPreview = false);

    static int extractAudioFilesFromZip(const juce::File& zipFile,
                                       const juce::File& destinationFolder,
                                       juce::String& outStatus);

    void handleDownloadFinished(const juce::String& fileId, const juce::File& destFile, bool success, bool isZip = false, int extractedCount = 0);
    void checkAndTriggerBatchScan();

    std::atomic<int> activeDownloadCount { 0 };
    std::atomic<bool> isFetching { false };
    juce::String statusText { "Ready. Enter your Pixeldrain API key to fetch account files." };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LibrariesComponent)
};

} // namespace openwav
