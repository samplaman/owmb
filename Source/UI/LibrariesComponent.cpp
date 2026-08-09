#include "LibrariesComponent.h"
#include "OpenWavLookAndFeel.h"

namespace openwav
{

class TableActionButtonComponent : public juce::Component
{
public:
    TableActionButtonComponent(std::function<void()> onDownloadClick)
        : downloadAction(onDownloadClick)
    {
        btnDownload.onClick = [this] { if (downloadAction) downloadAction(); };
        addAndMakeVisible(btnDownload);
    }

    void updateCallbacks(std::function<void()> onDownloadClick)
    {
        downloadAction = onDownloadClick;
    }

    void updateState(bool isDownloaded, bool isDownloading, double progress)
    {
        if (isDownloading)
        {
            btnDownload.setButtonText("Downloading " + juce::String(juce::roundToInt(progress * 100.0)) + "%");
            btnDownload.setEnabled(false);
        }
        else if (isDownloaded)
        {
            btnDownload.setButtonText("Downloaded");
            btnDownload.setEnabled(false);
        }
        else
        {
            btnDownload.setButtonText("Download");
            btnDownload.setEnabled(true);
        }
    }

    void resized() override
    {
        btnDownload.setBounds(getLocalBounds().reduced(2, 2));
    }

private:
    juce::TextButton btnDownload { "Download" };
    std::function<void()> downloadAction;
};

static bool isSupportedAudioFile(const juce::String& name, const juce::String& mime)
{
    juce::String ext = juce::File(name).getFileExtension().toLowerCase();
    if (ext == ".wav" || ext == ".mp3" || ext == ".flac" || ext == ".ogg" || ext == ".aiff" || ext == ".aif")
        return true;

    juce::String mimeLower = mime.toLowerCase();
    if (mimeLower.contains("wav") || mimeLower.contains("mpeg") || mimeLower.contains("mp3") ||
        mimeLower.contains("flac") || mimeLower.contains("ogg") || mimeLower.contains("aiff"))
        return true;

    return false;
}



LibrariesComponent::LibrariesComponent(TagDatabaseManager& db, LibraryScanner& scanner, AudioEngine& audio)
    : dbManager(db), libraryScanner(scanner), audioEngine(audio)
{
    // Pixeldrain Logo on top right
    addAndMakeVisible(pixeldrainLogoComponent);

    // Load API Key or Hotlink from settings
    apiKeyEditor.setText(dbManager.getPixeldrainApiKey(), juce::dontSendNotification);
    apiKeyEditor.setJustification(juce::Justification::centredLeft);
    apiKeyEditor.setIndents(6, 0);
    apiKeyEditor.setTextToShowWhenEmpty("Enter API Key or Public Hotlink (e.g. /u/id or /l/id)...", OpenWavLookAndFeel::textSecondary);
    apiKeyEditor.addListener(this);
    addAndMakeVisible(apiKeyEditor);

    apiKeyLabel.setFont(juce::Font(13.0f).boldened());
    apiKeyLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textPrimary);
    addAndMakeVisible(apiKeyLabel);

    connectButton.onClick = [this] { fetchUserFiles(); };
    addAndMakeVisible(connectButton);

    statusLabel.setFont(juce::Font(12.0f));
    statusLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    statusLabel.setText(statusText, juce::dontSendNotification);
    addAndMakeVisible(statusLabel);

    searchLabel.setFont(juce::Font(13.0f).boldened());
    searchLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textPrimary);
    addAndMakeVisible(searchLabel);

    searchEditor.setJustification(juce::Justification::centredLeft);
    searchEditor.setIndents(6, 0);
    searchEditor.setTextToShowWhenEmpty("Filter remote files by name...", OpenWavLookAndFeel::textSecondary);
    searchEditor.addListener(this);
    addAndMakeVisible(searchEditor);

    juce::File downloadDir(dbManager.getDownloadFolder());
    saveDirLabel.setFont(juce::Font(12.0f));
    saveDirLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    saveDirLabel.setText("Save to: " + downloadDir.getFullPathName(), juce::dontSendNotification);
    addAndMakeVisible(saveDirLabel);

    chooseDirButton.onClick = [this] {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Select Download Directory...",
            juce::File(dbManager.getDownloadFolder()),
            "*"
        );
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
            [this, chooser](const juce::FileChooser& fc) {
                auto result = fc.getResult();
                if (result.isDirectory())
                {
                    dbManager.setDownloadFolder(result.getFullPathName());
                    saveDirLabel.setText("Save to: " + result.getFullPathName(), juce::dontSendNotification);
                    updateDownloadStatuses();
                }
            });
    };
    addAndMakeVisible(chooseDirButton);

    downloadAllWavsButton.onClick = [this] { downloadAllWavs(); };
    addAndMakeVisible(downloadAllWavsButton);

    // Setup Table Box
    auto& header = tableBox.getHeader();
    header.addColumn("#", 1, 40, 30, 60, juce::TableHeaderComponent::notSortable);
    header.addColumn("Name", 2, 320, 150, 600);
    header.addColumn("Type", 3, 90, 60, 120);
    header.addColumn("Size", 4, 90, 60, 120);
    header.addColumn("Uploaded", 5, 140, 100, 200);
    header.addColumn("Status", 6, 120, 80, 180);
    header.addColumn("Action", 7, 120, 80, 180);

    tableBox.setModel(this);
    tableBox.setRowHeight(36);
    addAndMakeVisible(tableBox);

    lookAndFeelChanged();

    if (apiKeyEditor.getText().isNotEmpty())
    {
        fetchUserFiles();
    }
}

LibrariesComponent::~LibrariesComponent()
{
    tableBox.setModel(nullptr);
    apiKeyEditor.removeListener(this);
    searchEditor.removeListener(this);
}

void LibrariesComponent::paint(juce::Graphics& g)
{
    g.fillAll(OpenWavLookAndFeel::bgDark);
}

void LibrariesComponent::resized()
{
    auto area = getLocalBounds().reduced(16);

    // Header Controls Block
    auto topRow = area.removeFromTop(32);

    if (pixeldrainLogoComponent.isVisible())
    {
        pixeldrainLogoComponent.setBounds(topRow.removeFromRight(90));
        topRow.removeFromRight(12);
    }

    apiKeyLabel.setBounds(topRow.removeFromLeft(125));
    topRow.removeFromLeft(6);
    apiKeyEditor.setBounds(topRow.removeFromLeft(280));
    topRow.removeFromLeft(8);
    connectButton.setBounds(topRow.removeFromLeft(110));
    topRow.removeFromLeft(12);
    statusLabel.setBounds(topRow);

    area.removeFromTop(12);

    auto secondRow = area.removeFromTop(32);
    searchLabel.setBounds(secondRow.removeFromLeft(55));
    secondRow.removeFromLeft(6);
    searchEditor.setBounds(secondRow.removeFromLeft(240));
    secondRow.removeFromLeft(16);
    saveDirLabel.setBounds(secondRow.removeFromLeft(360));
    secondRow.removeFromLeft(8);
    chooseDirButton.setBounds(secondRow.removeFromLeft(130));
    secondRow.removeFromLeft(12);
    downloadAllWavsButton.setBounds(secondRow.removeFromLeft(150));

    area.removeFromTop(12);

    tableBox.setBounds(area);

    auto& header = tableBox.getHeader();
    int tableWidth = area.getWidth();
    // Fixed columns: Column 1 (#) = 40 px
    int availableWidth = tableWidth - 40;
    if (availableWidth > 100)
    {
        // Default sum of resizable column widths is 980 px.
        double scale = static_cast<double>(availableWidth) / 980.0;
        
        header.setColumnWidth(2, static_cast<int>(320 * scale));
        header.setColumnWidth(3, static_cast<int>(90 * scale));
        header.setColumnWidth(4, static_cast<int>(90 * scale));
        header.setColumnWidth(5, static_cast<int>(140 * scale));
        header.setColumnWidth(6, static_cast<int>(120 * scale));
        header.setColumnWidth(7, static_cast<int>(120 * scale));
    }
}

int LibrariesComponent::getNumRows()
{
    return static_cast<int>(displayedFiles.size());
}

void LibrariesComponent::paintRowBackground(juce::Graphics& g, int rowNumber, int /*width*/, int /*height*/, bool rowIsSelected)
{
    if (rowIsSelected)
    {
        g.fillAll(OpenWavLookAndFeel::bgHover);
    }
    else if (rowNumber % 2 == 1)
    {
        g.fillAll(OpenWavLookAndFeel::bgDark.withMultipliedBrightness(1.05f));
    }
    else
    {
        g.fillAll(OpenWavLookAndFeel::bgDark);
    }
}

void LibrariesComponent::paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool /*rowIsSelected*/)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(displayedFiles.size()))
        return;

    const auto& item = displayedFiles[static_cast<size_t>(rowNumber)];

    g.setFont(juce::Font(13.0f));
    g.setColour(OpenWavLookAndFeel::textPrimary);

    juce::Rectangle<int> cellBounds(4, 0, width - 8, height);

    if (columnId == 1) // #
    {
        g.setColour(OpenWavLookAndFeel::textSecondary);
        g.drawText(juce::String(rowNumber + 1), cellBounds, juce::Justification::centredLeft);
    }
    else if (columnId == 2) // Name
    {
        if (item.isWav)
            g.setColour(OpenWavLookAndFeel::accentCyan);
        else
            g.setColour(OpenWavLookAndFeel::textPrimary);

        g.drawText(item.name, cellBounds, juce::Justification::centredLeft, true);
    }
    else if (columnId == 3) // Type
    {
        g.setColour(OpenWavLookAndFeel::textSecondary);
        juce::String ext = juce::File(item.name).getFileExtension().toUpperCase();
        if (ext.isEmpty()) ext = item.mimeType;
        g.drawText(ext, cellBounds, juce::Justification::centredLeft);
    }
    else if (columnId == 4) // Size
    {
        g.setColour(OpenWavLookAndFeel::textSecondary);
        double mb = static_cast<double>(item.sizeBytes) / (1024.0 * 1024.0);
        juce::String sizeStr = (mb >= 1.0) ? juce::String(mb, 2) + " MB" : juce::String(item.sizeBytes / 1024) + " KB";
        g.drawText(sizeStr, cellBounds, juce::Justification::centredLeft);
    }
    else if (columnId == 5) // Uploaded
    {
        g.setColour(OpenWavLookAndFeel::textSecondary);
        juce::String dateStr = item.dateUpload;
        if (dateStr.contains("T"))
            dateStr = dateStr.upToFirstOccurrenceOf("T", false, false);
        g.drawText(dateStr, cellBounds, juce::Justification::centredLeft);
    }
    else if (columnId == 6) // Status
    {
        if (item.isDownloading)
        {
            g.setColour(OpenWavLookAndFeel::accentCyan);
            g.drawText("Downloading...", cellBounds, juce::Justification::centredLeft);
        }
        else if (item.isDownloaded)
        {
            g.setColour(juce::Colour::fromRGB(40, 167, 69));
            g.drawText("Local Library", cellBounds, juce::Justification::centredLeft);
        }
        else
        {
            g.setColour(OpenWavLookAndFeel::textSecondary);
            g.drawText("Cloud Only", cellBounds, juce::Justification::centredLeft);
        }
    }
}

juce::Component* LibrariesComponent::refreshComponentForCell(int rowNumber, int columnId, bool /*isRowSelected*/, juce::Component* existingComponentToUpdate)
{
    if (columnId != 7)
    {
        delete existingComponentToUpdate;
        return nullptr;
    }

    if (rowNumber < 0 || rowNumber >= static_cast<int>(displayedFiles.size()))
    {
        delete existingComponentToUpdate;
        return nullptr;
    }

    const auto& item = displayedFiles[static_cast<size_t>(rowNumber)];

    auto* actionComp = dynamic_cast<TableActionButtonComponent*>(existingComponentToUpdate);
    if (actionComp == nullptr)
    {
        actionComp = new TableActionButtonComponent(
            [this, rowNumber] { downloadFile(rowNumber); }
        );
    }
    else
    {
        actionComp->updateCallbacks(
            [this, rowNumber] { downloadFile(rowNumber); }
        );
    }

    actionComp->updateState(item.isDownloaded, item.isDownloading, item.downloadProgress);
    return actionComp;
}

void LibrariesComponent::cellDoubleClicked(int rowNumber, int /*columnId*/, const juce::MouseEvent& /*e*/)
{
    if (rowNumber >= 0 && rowNumber < static_cast<int>(displayedFiles.size()))
    {
        const auto& item = displayedFiles[static_cast<size_t>(rowNumber)];
        if (item.isDownloaded && juce::File(item.localPath).existsAsFile())
        {
            audioEngine.loadFile(juce::File(item.localPath), true);
        }
        else
        {
            previewFile(rowNumber);
        }
    }
}

void LibrariesComponent::cellClicked(int rowNumber, int /*columnId*/, const juce::MouseEvent& /*e*/)
{
    if (rowNumber >= 0 && rowNumber < static_cast<int>(displayedFiles.size()))
    {
        tableBox.grabKeyboardFocus();
        if (tableBox.getSelectedRow() == rowNumber)
        {
            previewFile(rowNumber);
        }
        else
        {
            tableBox.selectRow(rowNumber);
        }
    }
}

void LibrariesComponent::selectedRowsChanged(int lastRowSelected)
{
    if (lastRowSelected >= 0 && lastRowSelected < static_cast<int>(displayedFiles.size()))
    {
        previewFile(lastRowSelected);
    }
}

void LibrariesComponent::textEditorTextChanged(juce::TextEditor& editor)
{
    if (&editor == &apiKeyEditor)
    {
        juce::String text = apiKeyEditor.getText().trim();
        dbManager.setPixeldrainApiKey(text);
        if (text.isEmpty())
        {
            allRemoteFiles.clear();
            displayedFiles.clear();
            tableBox.updateContent();
            statusLabel.setText("Ready. Enter your Pixeldrain API key or hotlink to fetch account files.", juce::dontSendNotification);
        }
    }
    else if (&editor == &searchEditor)
    {
        filterRemoteFiles();
    }
}

static void extractFileObj(const juce::var& itemVar, std::vector<PixeldrainFile>& outFiles)
{
    if (!itemVar.isObject()) return;
    auto* fileObj = itemVar.getDynamicObject();
    if (!fileObj) return;

    if (fileObj->hasProperty("detail"))
    {
        auto detailVar = fileObj->getProperty("detail");
        if (detailVar.isObject())
            fileObj = detailVar.getDynamicObject();
    }

    juce::String id = fileObj->getProperty("id").toString();
    if (id.isEmpty()) id = fileObj->getProperty("file_id").toString();
    if (id.isEmpty()) id = fileObj->getProperty("path").toString();
    if (id.isEmpty()) return;

    juce::String name = fileObj->getProperty("name").toString();
    juce::String mime = fileObj->getProperty("mime_type").toString();
    if (mime.isEmpty()) mime = fileObj->getProperty("file_type").toString();

    // STRICT AUDIO FILTER: Only .wav, .mp3, .flac, .ogg, .aiff (.aif)
    if (!isSupportedAudioFile(name, mime))
        return;

    PixeldrainFile f;
    f.id = id;
    f.name = name;
    if (fileObj->hasProperty("size"))
        f.sizeBytes = static_cast<int64_t>(static_cast<juce::int64>(fileObj->getProperty("size")));
    else if (fileObj->hasProperty("file_size"))
        f.sizeBytes = static_cast<int64_t>(static_cast<juce::int64>(fileObj->getProperty("file_size")));

    f.dateUpload = fileObj->getProperty("date_upload").toString();
    if (f.dateUpload.isEmpty()) f.dateUpload = fileObj->getProperty("date_created").toString();
    if (f.dateUpload.isEmpty()) f.dateUpload = fileObj->getProperty("created").toString();
    if (f.dateUpload.isEmpty()) f.dateUpload = fileObj->getProperty("modified").toString();

    f.mimeType = mime;
    f.isWav = juce::File(name).getFileExtension().equalsIgnoreCase(".wav") || mime.containsIgnoreCase("wav");

    outFiles.push_back(f);
}

static juce::URL::InputStreamOptions makeHttpOptions(const juce::String& authHeader = {}, int timeoutMs = 10000, bool acceptJson = true)
{
    auto opts = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                    .withConnectionTimeoutMs(timeoutMs)
                    .withNumRedirectsToFollow(5);

    juce::String extraHeaders = "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) OpenWav/1.0";
    if (acceptJson)
        extraHeaders += "\r\nAccept: application/json";
    else
        extraHeaders += "\r\nAccept: */*";

    if (authHeader.isNotEmpty())
        extraHeaders += "\r\n" + authHeader;

    return opts.withExtraHeaders(extraHeaders);
}

static void extractFilesFromFilesystemNode(const juce::var& nodeVar, std::vector<PixeldrainFile>& outFiles, const juce::String& authHeader)
{
    if (!nodeVar.isObject()) return;
    auto* obj = nodeVar.getDynamicObject();
    if (!obj) return;

    juce::String type = obj->getProperty("type").toString().toLowerCase();
    bool isDir = (type == "dir" || type == "directory" || static_cast<bool>(obj->getProperty("is_directory")));

    if (isDir)
    {
        juce::String subDirId = obj->getProperty("id").toString();
        if (subDirId.isEmpty()) subDirId = obj->getProperty("path").toString();
        if (subDirId.isEmpty()) subDirId = obj->getProperty("name").toString();

        if (subDirId.isNotEmpty())
        {
            juce::URL subUrl("https://pixeldrain.com/api/filesystem/" + subDirId);
            std::unique_ptr<juce::InputStream> subStream(subUrl.createInputStream(makeHttpOptions(authHeader, 10000)));
            if (subStream != nullptr)
            {
                auto subText = subStream->readEntireStreamAsString();
                auto subParsed = juce::JSON::parse(subText);
                if (subParsed.isObject())
                {
                    auto* subObj = subParsed.getDynamicObject();
                    if (subObj && subObj->hasProperty("children"))
                    {
                        auto cVar = subObj->getProperty("children");
                        if (cVar.isArray())
                        {
                            for (const auto& child : *cVar.getArray())
                            {
                                extractFilesFromFilesystemNode(child, outFiles, authHeader);
                            }
                        }
                    }
                }
            }
        }
    }
    else
    {
        extractFileObj(nodeVar, outFiles);
    }
}

struct PixeldrainTarget
{
    enum Kind { UserAccount, List, SingleFile, Directory } kind { UserAccount };
    juce::String idOrKey;
};

static PixeldrainTarget parsePixeldrainInput(const juce::String& rawInput)
{
    PixeldrainTarget target;
    juce::String s = rawInput.trim();

    while (s.startsWith("\"") || s.startsWith("'")) s = s.substring(1);
    while (s.endsWith("\"") || s.endsWith("'")) s = s.dropLastCharacters(1);
    s = s.trim();

    juce::String lower = s.toLowerCase();

    // Check Shared Directory / Filesystem Link (e.g. /d/ or /filesystem/ or /dir/)
    if (lower.contains("/d/") || lower.contains("/filesystem/") || lower.contains("/dir/"))
    {
        target.kind = PixeldrainTarget::Directory;
        juce::String dirId = s;

        if (lower.contains("/filesystem/"))
            dirId = dirId.substring(lower.indexOf("/filesystem/") + 12);
        else if (lower.contains("/dir/"))
            dirId = dirId.substring(lower.indexOf("/dir/") + 5);
        else if (lower.contains("/d/"))
            dirId = dirId.substring(lower.indexOf("/d/") + 3);

        if (dirId.contains("?")) dirId = dirId.upToFirstOccurrenceOf("?", false, false);
        if (dirId.contains("#")) dirId = dirId.upToFirstOccurrenceOf("#", false, false);
        if (dirId.contains("/")) dirId = dirId.upToFirstOccurrenceOf("/", false, false);

        target.idOrKey = dirId.trim();
        return target;
    }

    if (lower.contains("/list/") || lower.contains("/l/"))
    {
        target.kind = PixeldrainTarget::List;
        juce::String listId = s;

        if (lower.contains("/list/"))
            listId = listId.substring(lower.indexOf("/list/") + 6);
        else if (lower.contains("/l/"))
            listId = listId.substring(lower.indexOf("/l/") + 3);

        if (listId.contains("?")) listId = listId.upToFirstOccurrenceOf("?", false, false);
        if (listId.contains("#")) listId = listId.upToFirstOccurrenceOf("#", false, false);
        if (listId.contains("/")) listId = listId.upToFirstOccurrenceOf("/", false, false);

        target.idOrKey = listId.trim();
        return target;
    }

    if (lower.contains("/file/") || lower.contains("/u/"))
    {
        target.kind = PixeldrainTarget::SingleFile;
        juce::String fileId = s;

        if (lower.contains("/file/"))
            fileId = fileId.substring(lower.indexOf("/file/") + 6);
        else if (lower.contains("/u/"))
            fileId = fileId.substring(lower.indexOf("/u/") + 3);

        if (fileId.endsWith("/info")) fileId = fileId.dropLastCharacters(5);
        if (fileId.contains("?")) fileId = fileId.upToFirstOccurrenceOf("?", false, false);
        if (fileId.contains("#")) fileId = fileId.upToFirstOccurrenceOf("#", false, false);
        if (fileId.contains("/")) fileId = fileId.upToFirstOccurrenceOf("/", false, false);

        target.idOrKey = fileId.trim();
        return target;
    }

    if (lower.contains("pixeldrain.com") || lower.contains("http://") || lower.contains("https://"))
    {
        juce::URL u(s);
        juce::String path = u.getSubPath();
        if (path.isEmpty()) path = s.fromLastOccurrenceOf("/", false, false);

        path = path.trim();
        if (path.startsWith("/")) path = path.substring(1);

        if (path.startsWithIgnoreCase("d/"))
        {
            target.kind = PixeldrainTarget::Directory;
            target.idOrKey = path.substring(2);
        }
        else if (path.startsWithIgnoreCase("l/"))
        {
            target.kind = PixeldrainTarget::List;
            target.idOrKey = path.substring(2);
        }
        else if (path.startsWithIgnoreCase("u/"))
        {
            target.kind = PixeldrainTarget::SingleFile;
            target.idOrKey = path.substring(2);
        }
        else if (path.startsWithIgnoreCase("list/"))
        {
            target.kind = PixeldrainTarget::List;
            target.idOrKey = path.substring(5);
        }
        else if (path.startsWithIgnoreCase("file/"))
        {
            target.kind = PixeldrainTarget::SingleFile;
            target.idOrKey = path.substring(5);
        }
        else if (path.startsWithIgnoreCase("filesystem/"))
        {
            target.kind = PixeldrainTarget::Directory;
            target.idOrKey = path.substring(11);
        }
        else
        {
            target.kind = PixeldrainTarget::Directory;
            target.idOrKey = path;
        }

        if (target.idOrKey.contains("?")) target.idOrKey = target.idOrKey.upToFirstOccurrenceOf("?", false, false);
        if (target.idOrKey.contains("/")) target.idOrKey = target.idOrKey.upToFirstOccurrenceOf("/", false, false);
        return target;
    }

    target.kind = PixeldrainTarget::UserAccount;
    target.idOrKey = s;
    return target;
}

void LibrariesComponent::fetchUserFiles()
{
    juce::String inputStr = apiKeyEditor.getText().trim();
    if (inputStr.isEmpty())
    {
        statusLabel.setText("Please enter an API Key or Public Hotlink (e.g. /d/id, /u/id, or /l/id).", juce::dontSendNotification);
        return;
    }

    dbManager.setPixeldrainApiKey(inputStr);
    statusLabel.setText("Fetching from Pixeldrain...", juce::dontSendNotification);
    isFetching = true;

    auto target = parsePixeldrainInput(inputStr);

    juce::Thread::launch([this, target] {
        std::vector<PixeldrainFile> fetchedFiles;
        juce::String errorMsg;

        if (target.kind == PixeldrainTarget::Directory)
        {
            juce::URL url("https://pixeldrain.com/api/filesystem/" + target.idOrKey);
            std::unique_ptr<juce::InputStream> stream(url.createInputStream(makeHttpOptions("", 10000)));

            if (stream != nullptr)
            {
                auto responseText = stream->readEntireStreamAsString();
                auto parsed = juce::JSON::parse(responseText);
                if (parsed.isObject())
                {
                    auto* obj = parsed.getDynamicObject();
                    if (obj && obj->hasProperty("children"))
                    {
                        auto cVar = obj->getProperty("children");
                        if (cVar.isArray())
                        {
                            for (const auto& child : *cVar.getArray())
                            {
                                extractFilesFromFilesystemNode(child, fetchedFiles, "");
                            }
                        }
                    }
                }
            }

            // Fallback: If filesystem endpoint returned no files, try list endpoint!
            if (fetchedFiles.empty())
            {
                juce::URL listUrl("https://pixeldrain.com/api/list/" + target.idOrKey);
                std::unique_ptr<juce::InputStream> listStream(listUrl.createInputStream(makeHttpOptions("", 10000)));
                if (listStream != nullptr)
                {
                    auto listText = listStream->readEntireStreamAsString();
                    auto listParsed = juce::JSON::parse(listText);
                    if (listParsed.isObject())
                    {
                        auto* obj = listParsed.getDynamicObject();
                        if (obj && obj->hasProperty("files"))
                        {
                            auto fVar = obj->getProperty("files");
                            if (fVar.isArray())
                            {
                                for (const auto& itemVar : *fVar.getArray())
                                    extractFileObj(itemVar, fetchedFiles);
                            }
                        }
                    }
                }
            }

            if (fetchedFiles.empty())
            {
                errorMsg = "Failed to fetch shared folder: " + target.idOrKey;
            }
        }
        else if (target.kind == PixeldrainTarget::SingleFile)
        {
            juce::URL url("https://pixeldrain.com/api/file/" + target.idOrKey + "/info");
            std::unique_ptr<juce::InputStream> stream(url.createInputStream(makeHttpOptions("", 10000)));

            if (stream != nullptr)
            {
                auto responseText = stream->readEntireStreamAsString();
                auto parsed = juce::JSON::parse(responseText);
                extractFileObj(parsed, fetchedFiles);

                // Fallback 1: Try list endpoint
                if (fetchedFiles.empty())
                {
                    juce::URL listUrl("https://pixeldrain.com/api/list/" + target.idOrKey);
                    std::unique_ptr<juce::InputStream> listStream(listUrl.createInputStream(makeHttpOptions("", 10000)));
                    if (listStream != nullptr)
                    {
                        auto listText = listStream->readEntireStreamAsString();
                        auto listParsed = juce::JSON::parse(listText);
                        if (listParsed.isObject())
                        {
                            auto* obj = listParsed.getDynamicObject();
                            if (obj && obj->hasProperty("files"))
                            {
                                auto fVar = obj->getProperty("files");
                                if (fVar.isArray())
                                {
                                    for (const auto& itemVar : *fVar.getArray())
                                        extractFileObj(itemVar, fetchedFiles);
                                }
                            }
                        }
                    }
                }

                // Fallback 2: Try filesystem endpoint
                if (fetchedFiles.empty())
                {
                    juce::URL fsUrl("https://pixeldrain.com/api/filesystem/" + target.idOrKey);
                    std::unique_ptr<juce::InputStream> fsStream(fsUrl.createInputStream(makeHttpOptions("", 10000)));
                    if (fsStream != nullptr)
                    {
                        auto fsText = fsStream->readEntireStreamAsString();
                        auto fsParsed = juce::JSON::parse(fsText);
                        if (fsParsed.isObject())
                        {
                            auto* obj = fsParsed.getDynamicObject();
                            if (obj && obj->hasProperty("children"))
                            {
                                auto cVar = obj->getProperty("children");
                                if (cVar.isArray())
                                {
                                    for (const auto& child : *cVar.getArray())
                                        extractFilesFromFilesystemNode(child, fetchedFiles, "");
                                }
                            }
                        }
                    }
                }
            }
            else
            {
                errorMsg = "Failed to fetch public file: " + target.idOrKey;
            }
        }
        else if (target.kind == PixeldrainTarget::List)
        {
            juce::URL url("https://pixeldrain.com/api/list/" + target.idOrKey);
            std::unique_ptr<juce::InputStream> stream(url.createInputStream(makeHttpOptions("", 10000)));

            if (stream != nullptr)
            {
                auto responseText = stream->readEntireStreamAsString();
                auto parsed = juce::JSON::parse(responseText);
                if (parsed.isObject())
                {
                    auto* obj = parsed.getDynamicObject();
                    if (obj && obj->hasProperty("files"))
                    {
                        auto fVar = obj->getProperty("files");
                        if (fVar.isArray())
                        {
                            for (const auto& itemVar : *fVar.getArray())
                            {
                                extractFileObj(itemVar, fetchedFiles);
                            }
                        }
                    }
                }
            }

            // Fallback to filesystem endpoint
            if (fetchedFiles.empty())
            {
                juce::URL fsUrl("https://pixeldrain.com/api/filesystem/" + target.idOrKey);
                std::unique_ptr<juce::InputStream> fsStream(fsUrl.createInputStream(makeHttpOptions("", 10000)));
                if (fsStream != nullptr)
                {
                    auto fsText = fsStream->readEntireStreamAsString();
                    auto fsParsed = juce::JSON::parse(fsText);
                    if (fsParsed.isObject())
                    {
                        auto* obj = fsParsed.getDynamicObject();
                        if (obj && obj->hasProperty("children"))
                        {
                            auto cVar = obj->getProperty("children");
                            if (cVar.isArray())
                            {
                                for (const auto& child : *cVar.getArray())
                                    extractFilesFromFilesystemNode(child, fetchedFiles, "");
                            }
                        }
                    }
                }
            }

            if (fetchedFiles.empty())
            {
                errorMsg = "Failed to fetch public list: " + target.idOrKey;
            }
        }
        else // UserAccount
        {
            juce::URL url("https://pixeldrain.com/api/user/files");
            juce::String authHeader = "Authorization: Basic " + juce::Base64::toBase64(":" + target.idOrKey);

            std::unique_ptr<juce::InputStream> stream(url.createInputStream(makeHttpOptions(authHeader, 10000)));
            if (stream != nullptr)
            {
                auto responseText = stream->readEntireStreamAsString();
                auto parsed = juce::JSON::parse(responseText);

                juce::Array<juce::var>* filesArray = nullptr;

                if (parsed.isObject())
                {
                    auto* obj = parsed.getDynamicObject();
                    if (obj && obj->hasProperty("files"))
                    {
                        auto fVar = obj->getProperty("files");
                        if (fVar.isArray())
                            filesArray = fVar.getArray();
                    }
                }
                else if (parsed.isArray())
                {
                    filesArray = parsed.getArray();
                }

                if (filesArray != nullptr)
                {
                    for (const auto& itemVar : *filesArray)
                    {
                        extractFileObj(itemVar, fetchedFiles);
                    }
                }
            }
            else
            {
                errorMsg = "Failed to connect to Pixeldrain API. Check API key.";
            }
        }

        juce::MessageManager::callAsync([this, fetchedFiles, errorMsg] {
            isFetching = false;
            if (fetchedFiles.empty())
            {
                allRemoteFiles.clear();
                displayedFiles.clear();
                tableBox.updateContent();

                if (errorMsg.isNotEmpty())
                    statusLabel.setText("Notice: " + errorMsg, juce::dontSendNotification);
                else
                    statusLabel.setText("No audio files found (.wav, .mp3, .flac, .ogg, .aiff).", juce::dontSendNotification);
            }
            else
            {
                allRemoteFiles = fetchedFiles;
                updateDownloadStatuses();
                filterRemoteFiles();

                statusLabel.setText("Loaded " + juce::String(allRemoteFiles.size()) + " audio file(s) (.wav, .mp3, .flac, .ogg, .aiff)", juce::dontSendNotification);
            }
        });
    });
}

void LibrariesComponent::updateDownloadStatuses()
{
    juce::File targetDir(dbManager.getDownloadFolder());

    for (auto& f : allRemoteFiles)
    {
        juce::File checkFile = targetDir.getChildFile(f.name);
        if (checkFile.existsAsFile())
        {
            f.isDownloaded = true;
            f.localPath = checkFile.getFullPathName();
        }
    }
}

void LibrariesComponent::filterRemoteFiles()
{
    juce::String kw = searchEditor.getText().trim().toLowerCase();
    displayedFiles.clear();

    for (const auto& f : allRemoteFiles)
    {
        if (kw.isEmpty() || f.name.toLowerCase().contains(kw) || f.mimeType.toLowerCase().contains(kw))
        {
            displayedFiles.push_back(f);
        }
    }

    tableBox.updateContent();
    tableBox.repaint();
}

void LibrariesComponent::downloadFile(int displayedIndex)
{
    if (displayedIndex < 0 || displayedIndex >= static_cast<int>(displayedFiles.size()))
        return;

    auto& targetItem = displayedFiles[static_cast<size_t>(displayedIndex)];
    if (targetItem.isDownloading || targetItem.isDownloaded)
        return;

    targetItem.isDownloading = true;
    targetItem.downloadProgress = 0.0;
    tableBox.updateContent();

    juce::String fileId = targetItem.id;
    juce::String fileName = targetItem.name;
    juce::String inputStr = apiKeyEditor.getText().trim();
    juce::File destFolder(dbManager.getDownloadFolder());
    if (!destFolder.exists())
        destFolder.createDirectory();

    juce::File destFile = destFolder.getChildFile(fileName);

    juce::Component::SafePointer<LibrariesComponent> safeThis(this);

    juce::Thread::launch([safeThis, fileId, fileName, inputStr, destFile] {
        if (safeThis == nullptr) return;

        auto shouldExit = [safeThis] { return safeThis == nullptr; };
        bool success = downloadFileSync(fileId, fileName, inputStr, destFile, shouldExit, safeThis);

        juce::MessageManager::callAsync([safeThis, fileId, destFile, success] {
            if (safeThis != nullptr)
            {
                safeThis->handleDownloadFinished(fileId, destFile, success);
            }
        });
    });
}

void LibrariesComponent::downloadAllWavs()
{
    std::vector<QueuedDownload> newJobs;
    for (auto& item : displayedFiles)
    {
        if (!item.isDownloaded && !item.isDownloading)
        {
            item.isDownloading = true;
            item.downloadProgress = 0.0;

            for (auto& f : allRemoteFiles)
            {
                if (f.id == item.id)
                {
                    f.isDownloading = true;
                    f.downloadProgress = 0.0;
                }
            }

            QueuedDownload job;
            job.fileId = item.id;
            job.fileName = item.name;
            newJobs.push_back(job);
        }
    }

    if (newJobs.empty())
    {
        statusLabel.setText("No new audio files to download.", juce::dontSendNotification);
        return;
    }

    statusLabel.setText("Starting download of " + juce::String(newJobs.size()) + " audio files...", juce::dontSendNotification);
    tableBox.updateContent();

    {
        const juce::ScopedLock sl (downloadQueueLock);
        for (const auto& job : newJobs)
        {
            downloadQueue.push_back(job);
        }
    }

    if (sequentialDownloader == nullptr || !sequentialDownloader->isThreadRunning())
    {
        sequentialDownloader = std::make_unique<SequentialDownloader>(*this);
        sequentialDownloader->startThread();
    }
}

bool LibrariesComponent::downloadFileSync(const juce::String& fileId,
                                         const juce::String& fileName,
                                         const juce::String& apiKey,
                                         const juce::File& destFile,
                                         std::function<bool()> shouldExit,
                                         juce::Component::SafePointer<LibrariesComponent> safeThis,
                                         bool isPreview)
{
    juce::String cleanId = fileId.trim();
    juce::StringArray candidateUrls;

    if (cleanId.startsWithIgnoreCase("http://") || cleanId.startsWithIgnoreCase("https://"))
    {
        juce::String fullUrl = cleanId;
        if (fullUrl.contains("/u/"))
        {
            juce::String idPart = fullUrl.substring(fullUrl.indexOf("/u/") + 3);
            if (idPart.contains("?")) idPart = idPart.upToFirstOccurrenceOf("?", false, false);
            candidateUrls.add("https://pixeldrain.com/api/file/" + idPart + "?download");
            candidateUrls.add("https://pixeldrain.com/api/file/" + idPart);
        }
        else
        {
            if (!fullUrl.contains("?download") && !fullUrl.contains("&download"))
                candidateUrls.add(fullUrl + (fullUrl.contains("?") ? "&download" : "?download"));
            candidateUrls.add(fullUrl);
        }
    }
    else if (cleanId.startsWith("/"))
    {
        candidateUrls.add("https://pixeldrain.com/api/filesystem" + cleanId + "?download");
        candidateUrls.add("https://pixeldrain.com/api/filesystem" + cleanId);
    }
    else if (cleanId.startsWithIgnoreCase("filesystem/"))
    {
        juce::String pathPart = cleanId.substring(11);
        candidateUrls.add("https://pixeldrain.com/api/filesystem/" + pathPart + "?download");
        candidateUrls.add("https://pixeldrain.com/api/filesystem/" + pathPart);
    }
    else
    {
        if (cleanId.startsWithIgnoreCase("file/"))
            cleanId = cleanId.substring(5);

        candidateUrls.add("https://pixeldrain.com/api/file/" + cleanId + "?download");
        candidateUrls.add("https://pixeldrain.com/api/file/" + cleanId);
    }

    auto target = parsePixeldrainInput(apiKey);
    juce::String authHeader;
    if (target.kind == PixeldrainTarget::UserAccount && target.idOrKey.isNotEmpty())
    {
        authHeader = "Authorization: Basic " + juce::Base64::toBase64(":" + target.idOrKey);
    }

    bool success = false;

    for (const auto& downloadUrlStr : candidateUrls)
    {
        if (shouldExit() || safeThis == nullptr) break;

        juce::URL url(downloadUrlStr);
        auto stream = url.createInputStream(makeHttpOptions(authHeader, 15000, false));

        if (stream == nullptr && authHeader.isNotEmpty())
        {
            // Fallback retry without auth header if private account header failed
            stream = url.createInputStream(makeHttpOptions("", 15000, false));
        }

        if (stream != nullptr)
        {
            destFile.deleteFile();
            auto outStream = destFile.createOutputStream();
            if (outStream != nullptr)
            {
                int64_t totalBytes = stream->getTotalLength();
                int64_t bytesWritten = 0;
                char buffer[8192];

                while (!stream->isExhausted())
                {
                    if (shouldExit() || safeThis == nullptr)
                        break;

                    int bytesRead = stream->read(buffer, sizeof(buffer));
                    if (bytesRead <= 0) break;
                    outStream->write(buffer, static_cast<size_t>(bytesRead));
                    bytesWritten += bytesRead;

                    if (totalBytes > 0)
                    {
                        double progress = static_cast<double>(bytesWritten) / totalBytes;
                        juce::MessageManager::callAsync([safeThis, fileId, progress, isPreview, fileName] {
                            if (safeThis != nullptr)
                            {
                                for (auto& f : safeThis->allRemoteFiles)
                                {
                                    if (f.id == fileId)
                                    {
                                        if (isPreview)
                                            f.previewProgress = progress;
                                        else
                                            f.downloadProgress = progress;
                                    }
                                }
                                safeThis->filterRemoteFiles();

                                if (isPreview)
                                {
                                    safeThis->statusLabel.setText("Streaming preview: " + fileName + " (" + juce::String(juce::roundToInt(progress * 100.0)) + "%)", juce::dontSendNotification);
                                }
                            }
                        });
                    }
                }
                outStream->flush();

                if (destFile.existsAsFile() && destFile.getSize() > 128)
                {
                    juce::FileInputStream checkStream(destFile);
                    if (checkStream.openedOk())
                    {
                        char firstChars[32] = {0};
                        int readBytes = checkStream.read(firstChars, 31);
                        juce::String startStr(firstChars, static_cast<size_t>(readBytes));
                        startStr = startStr.trim();

                        // Reject JSON / HTML error responses from Pixeldrain
                        if (!startStr.startsWith("{") && !startStr.startsWith("<") &&
                            !startStr.containsIgnoreCase("error") && !startStr.containsIgnoreCase("404"))
                        {
                            success = true;
                        }
                    }
                }

                if (success)
                    break;
                else
                    destFile.deleteFile();
            }
        }
    }

    if (!success)
    {
        juce::MessageManager::callAsync([safeThis, fileName] {
            if (safeThis != nullptr)
            {
                safeThis->statusLabel.setText("Failed to connect or stream preview: " + fileName, juce::dontSendNotification);
            }
        });
    }
    return success;
}

void LibrariesComponent::handleDownloadFinished(const juce::String& fileId, const juce::File& destFile, bool success)
{
    for (auto& f : allRemoteFiles)
    {
        if (f.id == fileId)
        {
            f.isDownloading = false;
            if (success)
            {
                f.isDownloaded = true;
                f.localPath = destFile.getFullPathName();

                // Index downloaded file into OWMB library
                dbManager.addScanFolder(destFile.getParentDirectory().getFullPathName());
                libraryScanner.startScan({ destFile.getParentDirectory().getFullPathName() });
            }
        }
    }
    filterRemoteFiles();
}

void LibrariesComponent::previewFile(int displayedIndex)
{
    if (displayedIndex < 0 || displayedIndex >= static_cast<int>(displayedFiles.size()))
        return;

    auto& targetItem = displayedFiles[static_cast<size_t>(displayedIndex)];
    if (targetItem.isDownloading || targetItem.isPreviewing)
        return;

    if (targetItem.isDownloaded && juce::File(targetItem.localPath).existsAsFile())
    {
        audioEngine.loadFile(juce::File(targetItem.localPath), true);
        return;
    }

    // Replace path separators in the file ID to prevent creating directories or invalid filenames in the temp folder
    juce::String safeId = targetItem.id.replace("/", "_").replace("\\", "_");
    juce::String ext = juce::File(targetItem.name).getFileExtension();
    if (ext.isEmpty()) ext = ".wav";

    juce::File previewFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                .getChildFile("owmb_preview_" + safeId + ext);

    if (previewFile.existsAsFile() && previewFile.getSize() > 128)
    {
        std::unique_ptr<juce::AudioFormatReader> reader(audioEngine.getFormatManager().createReaderFor(previewFile));
        if (reader != nullptr && reader->lengthInSamples > 0)
        {
            targetItem.previewPath = previewFile.getFullPathName();
            audioEngine.loadFile(previewFile, true);
            statusLabel.setText("Playing preview: " + targetItem.name, juce::dontSendNotification);
            return;
        }
        else
        {
            // Delete corrupt or incomplete cached preview file so it can be cleanly re-downloaded
            previewFile.deleteFile();
        }
    }

    statusLabel.setText("Streaming preview: " + targetItem.name + "...", juce::dontSendNotification);

    targetItem.isPreviewing = true;
    targetItem.previewProgress = 0.0;

    for (auto& f : allRemoteFiles)
    {
        if (f.id == targetItem.id)
        {
            f.isPreviewing = true;
            f.previewProgress = 0.0;
        }
    }

    tableBox.updateContent();

    juce::String fileId = targetItem.id;
    juce::String fileName = targetItem.name;
    juce::String inputStr = apiKeyEditor.getText().trim();

    juce::Component::SafePointer<LibrariesComponent> safeThis(this);

    juce::Thread::launch([safeThis, fileId, fileName, inputStr, previewFile] {
        if (safeThis == nullptr) return;

        auto shouldExit = [safeThis] { return safeThis == nullptr; };
        bool success = downloadFileSync(fileId, fileName, inputStr, previewFile, shouldExit, safeThis, true);

        juce::MessageManager::callAsync([safeThis, fileId, previewFile, success] {
            if (safeThis != nullptr)
            {
                safeThis->handlePreviewFinished(fileId, previewFile, success);
            }
        });
    });
}

void LibrariesComponent::handlePreviewFinished(const juce::String& fileId, const juce::File& previewFile, bool success)
{
    for (auto& f : allRemoteFiles)
    {
        if (f.id == fileId)
        {
            f.isPreviewing = false;
            if (success && previewFile.existsAsFile() && previewFile.getSize() > 128)
            {
                std::unique_ptr<juce::AudioFormatReader> reader(audioEngine.getFormatManager().createReaderFor(previewFile));
                if (reader != nullptr && reader->lengthInSamples > 0)
                {
                    f.previewPath = previewFile.getFullPathName();
                    audioEngine.loadFile(previewFile, true);
                    statusLabel.setText("Playing preview: " + f.name, juce::dontSendNotification);
                }
                else
                {
                    previewFile.deleteFile();
                    statusLabel.setText("Preview file format error: " + f.name, juce::dontSendNotification);
                }
            }
            else
            {
                if (previewFile.existsAsFile())
                    previewFile.deleteFile();

                statusLabel.setText("Preview stream failed for: " + f.name, juce::dontSendNotification);
            }
        }
    }
    filterRemoteFiles();
}

LibrariesComponent::SequentialDownloader::SequentialDownloader(LibrariesComponent& owner)
    : juce::Thread("SequentialDownloader"), owner(owner)
{
}

LibrariesComponent::SequentialDownloader::~SequentialDownloader()
{
    stopThread(3000);
}

void LibrariesComponent::SequentialDownloader::run()
{
    juce::Component::SafePointer<LibrariesComponent> safeOwner(&owner);

    while (!threadShouldExit())
    {
        if (safeOwner == nullptr)
            return;

        QueuedDownload nextJob;
        {
            const juce::ScopedLock sl (safeOwner->downloadQueueLock);
            if (safeOwner->downloadQueue.empty())
            {
                juce::MessageManager::callAsync([safeOwner] {
                    if (safeOwner != nullptr)
                        safeOwner->statusLabel.setText("All downloads finished.", juce::dontSendNotification);
                });
                break;
            }
            nextJob = safeOwner->downloadQueue.front();
            safeOwner->downloadQueue.erase(safeOwner->downloadQueue.begin());
        }

        // Perform the download
        juce::File destFolder(safeOwner->dbManager.getDownloadFolder());
        if (!destFolder.exists())
            destFolder.createDirectory();

        juce::File destFile = destFolder.getChildFile(nextJob.fileName);
        juce::String apiKey = safeOwner->dbManager.getPixeldrainApiKey();

        juce::MessageManager::callAsync([safeOwner, nextJob] {
            if (safeOwner != nullptr)
                safeOwner->statusLabel.setText("Downloading: " + nextJob.fileName, juce::dontSendNotification);
        });

        bool success = downloadFileSync(nextJob.fileId, nextJob.fileName, apiKey, destFile,
                                        [this] { return threadShouldExit(); }, safeOwner);

        juce::MessageManager::callAsync([safeOwner, nextJob, destFile, success] {
            if (safeOwner != nullptr)
            {
                safeOwner->handleDownloadFinished(nextJob.fileId, destFile, success);
            }
        });
    }
}


void LibrariesComponent::lookAndFeelChanged()
{
    juce::Image logoImage;
#if defined(JUCE_BINARYDATA_H_INCLUDED) || __has_include(<JuceHeader.h>)
    logoImage = juce::ImageFileFormat::loadFrom(BinaryData::mainpixeldrainlogo_cropped_png, static_cast<size_t>(BinaryData::mainpixeldrainlogo_cropped_pngSize));
#endif

    if (logoImage.isNull())
    {
        juce::File logoFile = juce::File::getCurrentWorkingDirectory().getChildFile("mainpixeldrainlogo_cropped.png");
        if (!logoFile.existsAsFile())
            logoFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile("mainpixeldrainlogo_cropped.png");
        if (!logoFile.existsAsFile())
            logoFile = juce::File::getCurrentWorkingDirectory().getChildFile("mainpixeldrainlogo.png");
        if (logoFile.existsAsFile())
            logoImage = juce::ImageFileFormat::loadFrom(logoFile);
    }

    if (!logoImage.isNull())
    {
        pixeldrainLogoComponent.setVisible(true);
        if (dbManager.isDarkMode())
        {
            // Invert colors of logo for dark theme (keeping alpha channel intact)
            juce::Image inverted = logoImage.createCopy();
            juce::Image::BitmapData bd(inverted, juce::Image::BitmapData::readWrite);
            for (int y = 0; y < bd.height; ++y)
            {
                for (int x = 0; x < bd.width; ++x)
                {
                    auto c = bd.getPixelColour(x, y);
                    bd.setPixelColour(x, y, juce::Colour(static_cast<juce::uint8>(255 - c.getRed()),
                                                        static_cast<juce::uint8>(255 - c.getGreen()),
                                                        static_cast<juce::uint8>(255 - c.getBlue()),
                                                        c.getAlpha()));
                }
            }
            pixeldrainLogoComponent.setImage(inverted, juce::RectanglePlacement::xRight | juce::RectanglePlacement::yMid | juce::RectanglePlacement::onlyReduceInSize);
        }
        else
        {
            pixeldrainLogoComponent.setImage(logoImage, juce::RectanglePlacement::xRight | juce::RectanglePlacement::yMid | juce::RectanglePlacement::onlyReduceInSize);
        }
    }
    else
    {
        pixeldrainLogoComponent.setVisible(false);
    }

    // Refresh table colours when LookAndFeel changes
    tableBox.setColour(juce::ListBox::backgroundColourId, OpenWavLookAndFeel::bgDark);
    tableBox.setOutlineThickness(1);
    tableBox.setColour(juce::ListBox::outlineColourId, OpenWavLookAndFeel::borderColour);
    tableBox.repaint();

    // Update labels and text fields with dynamic colors
    apiKeyLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textPrimary);
    apiKeyEditor.setTextToShowWhenEmpty("Enter API Key or Public Hotlink (e.g. /u/id or /l/id)...", OpenWavLookAndFeel::textSecondary);
    statusLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    searchLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textPrimary);
    searchEditor.setTextToShowWhenEmpty("Filter remote files by name...", OpenWavLookAndFeel::textSecondary);
    saveDirLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
}

bool LibrariesComponent::mayDragToExternalWindows() const
{
    return true;
}

} // namespace openwav
