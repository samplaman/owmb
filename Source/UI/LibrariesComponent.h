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
    bool isQueued { false };
    bool isFailed { false };
    juce::String failReason;
    double downloadProgress { 0.0 };
    int64_t bytesDownloaded { 0 };
    juce::String localPath;
    juce::String folderPath;    // e.g. "/Drums/Kicks"
    juce::String relativePath;  // e.g. "Drums/Kicks/kick_01.wav"

    // Preview/Streaming Support
    bool isPreviewing { false };
    double previewProgress { 0.0 };
    juce::String previewPath;
};

struct PixeldrainFolderNode : public std::enable_shared_from_this<PixeldrainFolderNode>
{
    juce::String id;
    juce::String name;
    juce::String fullPath;
    bool isRoot { false };
    std::weak_ptr<PixeldrainFolderNode> parentFolder;
    std::vector<std::shared_ptr<PixeldrainFolderNode>> subFolders;
    std::vector<PixeldrainFile> files;

    int getDirectFileCount() const { return static_cast<int>(files.size()); }
    int getTotalFileCount() const
    {
        int count = static_cast<int>(files.size());
        for (const auto& sub : subFolders)
            count += sub->getTotalFileCount();
        return count;
    }

    void getAllFilesRecursive(std::vector<PixeldrainFile>& out) const
    {
        for (const auto& f : files)
            out.push_back(f);
        for (const auto& sub : subFolders)
            sub->getAllFilesRecursive(out);
    }
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
    void downloadFolder(std::shared_ptr<PixeldrainFolderNode> folder);
    void cancelAllDownloads();
    void previewFile(int displayedIndex);
    void handlePreviewFinished(const juce::String& fileId, const juce::File& previewFile, bool success);

    // Folder Tree Operations
    void selectFolder(std::shared_ptr<PixeldrainFolderNode> folder);
    void rebuildFolderTree();

private:
    void textEditorTextChanged(juce::TextEditor& editor) override;
    void filterRemoteFiles();
    void updateDownloadStatuses();
    void updateBatchProgressLabel();
    void queueDownloads(const std::vector<PixeldrainFile>& filesToQueue, const juce::String& batchTitle);

    TagDatabaseManager& dbManager;
    LibraryScanner& libraryScanner;
    AudioEngine& audioEngine;

    // Header Controls
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
    juce::TextButton downloadFolderButton { "Download Folder" };
    juce::TextButton downloadAllWavsButton { "Download All" };

    // Tree View and Breadcrumb Controls
    juce::Label foldersHeaderLabel { {}, "ONLINE FOLDERS" };
    juce::TreeView folderTreeView;
    std::unique_ptr<juce::TreeViewItem> rootTreeItem;
    std::shared_ptr<PixeldrainFolderNode> rootFolderNode;
    std::shared_ptr<PixeldrainFolderNode> selectedFolderNode;

    juce::Label breadcrumbLabel;
    juce::ToggleButton includeSubfoldersToggle { "Include Subfolders" };

    juce::TableListBox tableBox;

    std::vector<PixeldrainFile> allRemoteFiles;
    std::vector<PixeldrainFile> displayedFiles;

    struct QueuedDownload
    {
        juce::String fileId;
        juce::String fileName;
        juce::String relativePath;
        int64_t sizeBytes { 0 };
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
                                 int64_t expectedSizeBytes,
                                 const juce::String& apiKey,
                                 const juce::File& destFile,
                                 std::function<bool()> shouldExit,
                                 juce::Component::SafePointer<LibrariesComponent> safeThis,
                                 bool isPreview = false);

    static int extractAudioFilesFromZip(const juce::File& zipFile,
                                       const juce::File& destinationFolder,
                                       juce::String& outStatus);

    void handleDownloadFinished(const juce::String& fileId, const juce::File& destFile, bool success, bool isZip = false, int extractedCount = 0, const juce::String& failReason = {});
    void checkAndTriggerBatchScan();

    std::atomic<int> activeDownloadCount { 0 };
    std::atomic<bool> cancelRequested { false };
    int totalBatchCount { 0 };
    int completedBatchCount { 0 };
    std::atomic<bool> isFetching { false };
    juce::String statusText { "Ready. Enter your Pixeldrain API key or hotlink to fetch account files." };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LibrariesComponent)
};

} // namespace openwav
