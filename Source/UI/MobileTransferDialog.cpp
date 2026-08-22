#include "MobileTransferDialog.h"
#include "OpenWavLookAndFeel.h"
#include "StudioIcons.h"

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
 #include <dwmapi.h>
 #pragma comment(lib, "dwmapi.lib")
#endif

namespace openwav
{

MobileTransferDialog::MobileTransferDialog(AudioEngine& engine, TagDatabaseManager& dbManager, LibraryScanner& scanner)
    : audioEngine(engine),
      dbManager(dbManager),
      libraryScanner(scanner),
      progressBar(currentProgress)
{
    setAlwaysOnTop(true);
    setVisible(false);

    syncManager.addListener(this);

    // 1. Header (No OWMB logo)
    titleLabel.setFont(juce::Font(juce::FontOptions(18.0f).withStyle("Bold")));
    titleLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    addAndMakeVisible(titleLabel);

    subtitleLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
    subtitleLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    addAndMakeVisible(subtitleLabel);

    // 2. Status
    statusLabel.setFont(juce::Font(juce::FontOptions(13.0f).withStyle("Bold")));
    statusLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    statusLabel.setText("Searching for Field Recorder on Wi-Fi...", juce::dontSendNotification);
    addAndMakeVisible(statusLabel);

    // 3. The Master 1-Button Sync
    syncNowButton.setCustomColours(OpenWavLookAndFeel::accentCyan,
                                  OpenWavLookAndFeel::isDarkTheme() ? juce::Colours::black : juce::Colours::white,
                                  OpenWavLookAndFeel::accentCyan);
    syncNowButton.onClick = [this] {
        startOneButtonSync();
    };
    addAndMakeVisible(syncNowButton);

    // 5. Progress & Status
    progressBar.setColour(juce::ProgressBar::foregroundColourId, OpenWavLookAndFeel::accentCyan);
    progressBar.setColour(juce::ProgressBar::backgroundColourId, OpenWavLookAndFeel::bgDark);
    addAndMakeVisible(progressBar);

    transferStatusLabel.setFont(juce::Font(juce::FontOptions(13.0f).withStyle("Bold")));
    transferStatusLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textPrimary);
    transferStatusLabel.setText("Ready to sync field recordings.", juce::dontSendNotification);
    transferStatusLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(transferStatusLabel);

    fileDetailLabel.setFont(juce::Font(juce::FontOptions(11.5f)));
    fileDetailLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    fileDetailLabel.setText("Tap the button above to import all recordings from phone.", juce::dontSendNotification);
    fileDetailLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(fileDetailLabel);

    // 6. Bottom Navigation
    openFolderButton.setCustomColours(OpenWavLookAndFeel::bgHover, OpenWavLookAndFeel::textPrimary, OpenWavLookAndFeel::borderColour);
    openFolderButton.onClick = [this] {
        auto recDir = MobileSyncManager::getRecordingsDirectory();
        recDir.revealToUser();
    };
    addAndMakeVisible(openFolderButton);

    closeButton.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::bgHover);
    closeButton.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::textPrimary);
    closeButton.onClick = [this] {
        hideDialog();
    };
    addAndMakeVisible(closeButton);

    setSize(540, 360);
    lookAndFeelChanged();
}

MobileTransferDialog::~MobileTransferDialog()
{
    stopTimer();
    syncManager.removeListener(this);
    syncManager.stopServices();
}

void MobileTransferDialog::lookAndFeelChanged()
{
    titleLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    subtitleLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);

    syncNowButton.setCustomColours(OpenWavLookAndFeel::accentCyan,
                                  OpenWavLookAndFeel::isDarkTheme() ? juce::Colours::black : juce::Colours::white,
                                  OpenWavLookAndFeel::accentCyan);

    openFolderButton.setCustomColours(OpenWavLookAndFeel::bgHover, OpenWavLookAndFeel::textPrimary, OpenWavLookAndFeel::borderColour);
    closeButton.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::bgHover);
    closeButton.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::textPrimary);

    repaint();
}

void MobileTransferDialog::showDialog()
{
    syncManager.startServices();
    syncManager.broadcastDesktopBeacon();
    syncManager.scanLocalSubnet();
    startTimer(1200);

    lookAndFeelChanged();
    setSize(540, 360);

    updateDiscoveredState();

    if (dialogWindow == nullptr)
    {
        juce::DialogWindow::LaunchOptions opts;
        opts.content.setNonOwned(this);
        opts.dialogTitle = "Field Recorder LAN Sync";
        opts.dialogBackgroundColour = OpenWavLookAndFeel::bgCard;
        opts.escapeKeyTriggersCloseButton = true;
        opts.useNativeTitleBar = true;
        opts.resizable = false;

        dialogWindow = opts.launchAsync();

#if JUCE_WINDOWS
        if (dialogWindow != nullptr)
        {
            if (auto* peer = dialogWindow->getPeer())
            {
                HWND hwnd = (HWND)peer->getNativeHandle();
                BOOL isDark = OpenWavLookAndFeel::isDarkTheme() ? TRUE : FALSE;
                DwmSetWindowAttribute(hwnd, 20, &isDark, sizeof(isDark));
                DwmSetWindowAttribute(hwnd, 19, &isDark, sizeof(isDark));
            }
        }
#endif
    }
}

void MobileTransferDialog::hideDialog()
{
    if (dialogWindow != nullptr)
        dialogWindow->exitModalState(0);
}

void MobileTransferDialog::timerCallback()
{
    updateDiscoveredState();
}

void MobileTransferDialog::discoveredDevicesChanged()
{
    updateDiscoveredState();
}

void MobileTransferDialog::updateDiscoveredState()
{
    auto devices = syncManager.getDiscoveredDevices();

    if (!devices.empty())
    {
        const auto& dev = devices[0];
        activeDeviceHost = dev.ipAddress;
        activeDevicePort = dev.port;
        activeDeviceName = dev.deviceName;
        isConnected = true;

        statusLabel.setText("Connected: " + dev.deviceName, juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);

        if (!isSyncing)
        {
            transferStatusLabel.setText("Field Recorder connected and ready to sync.", juce::dontSendNotification);
            transferStatusLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textPrimary);
        }
    }
    else
    {
        if (activeDeviceHost.isEmpty())
        {
            isConnected = false;
            statusLabel.setText("Searching for Field Recorder on Wi-Fi...", juce::dontSendNotification);
            statusLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
        }
    }
    repaint();
}

void MobileTransferDialog::startOneButtonSync()
{
    if (isSyncing) return;

    auto devices = syncManager.getDiscoveredDevices();
    if (!devices.empty())
    {
        const auto& dev = devices[0];
        syncAllFromDevice(dev.ipAddress, dev.port);
        return;
    }

    if (activeDeviceHost.isNotEmpty())
    {
        syncAllFromDevice(activeDeviceHost, activeDevicePort);
        return;
    }

    // Proactively scan subnet now
    transferStatusLabel.setText("Scanning local network for phone...", juce::dontSendNotification);
    transferStatusLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    fileDetailLabel.setText("Ensure phone app is running on the same Wi-Fi.", juce::dontSendNotification);

    syncManager.scanLocalSubnet([this](bool found) {
        if (found)
        {
            auto devs = syncManager.getDiscoveredDevices();
            if (!devs.empty())
            {
                syncAllFromDevice(devs[0].ipAddress, devs[0].port);
                return;
            }
        }

        transferStatusLabel.setText("Could not find phone automatically.", juce::dontSendNotification);
        transferStatusLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::favoriteRed);
        fileDetailLabel.setText("Make sure both phone & Mac/PC are on the same Wi-Fi with OWMB open.", juce::dontSendNotification);
    });
}

void MobileTransferDialog::syncAllFromDevice(const juce::String& host, int port)
{
    isSyncing = true;
    currentProgress = 0.0;
    progressBar.repaint();

    transferStatusLabel.setText("Connecting to " + (activeDeviceName.isNotEmpty() ? activeDeviceName : host) + "...", juce::dontSendNotification);
    transferStatusLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    fileDetailLabel.setText("Checking for new field recordings...", juce::dontSendNotification);

    syncManager.fetchRecordings(host, port,
        [this, host, port](bool success, const std::vector<MobileRecordingItem>& items, const juce::String& error) {
            if (!success)
            {
                isSyncing = false;
                transferStatusLabel.setText("Connection failed: " + error, juce::dontSendNotification);
                transferStatusLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::favoriteRed);
                fileDetailLabel.setText("Ensure the OWMB Field Recorder app is open on your phone.", juce::dontSendNotification);
                return;
            }

            pendingItemsToDownload.clear();
            for (const auto& it : items)
            {
                if (!it.isAlreadyDownloaded)
                    pendingItemsToDownload.push_back(it);
            }

            if (pendingItemsToDownload.empty())
            {
                isSyncing = false;
                currentProgress = 1.0;
                progressBar.repaint();
                transferStatusLabel.setText("All " + juce::String(items.size()) + " recordings are already in your library!", juce::dontSendNotification);
                transferStatusLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
                fileDetailLabel.setText("No new recordings to transfer.", juce::dontSendNotification);
                return;
            }

            downloadQueueIndex = 0;
            auto downloadNext = std::make_shared<std::function<void()>>();

            *downloadNext = [this, host, port, downloadNext]() {
                if (downloadQueueIndex < 0 || downloadQueueIndex >= static_cast<int>(pendingItemsToDownload.size()))
                {
                    isSyncing = false;
                    currentProgress = 1.0;
                    progressBar.repaint();
                    transferStatusLabel.setText("Successfully imported " + juce::String(pendingItemsToDownload.size()) + " new recordings!", juce::dontSendNotification);
                    transferStatusLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
                    fileDetailLabel.setText("Saved to ~/Documents/OWMB_Recordings & indexed with tags.", juce::dontSendNotification);
                    return;
                }

                const auto& item = pendingItemsToDownload[static_cast<size_t>(downloadQueueIndex)];
                auto destDir = MobileSyncManager::getRecordingsDirectory();
                auto destFile = destDir.getChildFile(item.name);

                transferStatusLabel.setText("Syncing (" + juce::String(downloadQueueIndex + 1) + "/" + juce::String(pendingItemsToDownload.size()) + "): " + item.name, juce::dontSendNotification);
                transferStatusLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
                fileDetailLabel.setText(item.sizeFormatted + " - " + item.date, juce::dontSendNotification);

                syncManager.downloadRecording(host, port, item.name, destFile,
                    [this](float progress) {
                        double overall = (static_cast<double>(downloadQueueIndex) + static_cast<double>(progress)) / static_cast<double>(pendingItemsToDownload.size());
                        currentProgress = juce::jlimit(0.0, 1.0, overall);
                        progressBar.repaint();
                    },
                    [this, downloadNext](bool success, const juce::File& downloadedFile, const juce::String& /*error*/) {
                        if (success && downloadedFile.existsAsFile())
                        {
                            importFileToDatabase(downloadedFile);
                        }

                        downloadQueueIndex++;
                        (*downloadNext)();
                    });
            };

            (*downloadNext)();
        });
}

void MobileTransferDialog::fileReceivedFromPhone(const juce::File& file)
{
    if (file.existsAsFile())
    {
        importFileToDatabase(file);
        transferStatusLabel.setText("Direct push received: " + file.getFileName(), juce::dontSendNotification);
        transferStatusLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
        fileDetailLabel.setText("Auto-saved to recordings folder & indexed in library.", juce::dontSendNotification);
    }
}

void MobileTransferDialog::importFileToDatabase(const juce::File& file)
{
    if (!file.existsAsFile()) return;

    auto recDir = MobileSyncManager::getRecordingsDirectory();
    dbManager.addScanFolder(recDir.getFullPathName());

    MediaItem item;
    item.id = juce::String::toHexString(file.getFullPathName().hashCode64());
    item.filePath = file.getFullPathName();
    item.fileName = file.getFileName();
    item.fileExtension = file.getFileExtension().toLowerCase();
    item.fileSizeBytes = file.getSize();
    item.dateAddedMs = file.getLastModificationTime().toMilliseconds();

    juce::AudioFormatManager formatMgr;
    formatMgr.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(formatMgr.createReaderFor(file));
    if (reader != nullptr)
    {
        item.sampleRate = reader->sampleRate;
        item.durationSeconds = static_cast<double>(reader->lengthInSamples) / (reader->sampleRate > 0.0 ? reader->sampleRate : 44100.0);
        item.numChannels = static_cast<int>(reader->numChannels);
        item.bitDepth = static_cast<int>(reader->bitsPerSample);
    }
    else
    {
        item.sampleRate = 44100.0;
        item.durationSeconds = 0.0;
        item.numChannels = 2;
        item.bitDepth = 24;
    }

    auto tags = TagDatabaseManager::inferTagsFromPath(file.getFullPathName(), item.durationSeconds, item.numChannels);
    tags.insert("field-recording");
    tags.insert("mobile");
    tags.insert("phone");
    tags.insert("lan-sync");
    TagDatabaseManager::sanitizeTags(tags);
    item.tags = tags;

    item.bpm = TagDatabaseManager::extractBpmFromFilename(file.getFileName());
    item.precomputeCachedStrings();

    dbManager.addOrUpdateItem(item);
    dbManager.notifyIndexUpdated();
    dbManager.notifyTagsUpdated();
}

void MobileTransferDialog::paint(juce::Graphics& g)
{
    g.fillAll(OpenWavLookAndFeel::bgDark);

    auto bounds = getLocalBounds().toFloat().reduced(14.0f);

    // 1. Top Header Card
    auto headerCard = bounds.removeFromTop(56.0f);
    g.setColour(OpenWavLookAndFeel::bgHeader);
    g.fillRoundedRectangle(headerCard, 8.0f);
    g.setColour(OpenWavLookAndFeel::borderColour);
    g.drawRoundedRectangle(headerCard, 8.0f, 1.0f);

    bounds.removeFromTop(10.0f);

    // 2. Bottom Button Card
    auto bottomCard = bounds.removeFromBottom(48.0f);
    g.setColour(OpenWavLookAndFeel::bgCard);
    g.fillRoundedRectangle(bottomCard, 8.0f);
    g.setColour(OpenWavLookAndFeel::borderColour);
    g.drawRoundedRectangle(bottomCard, 8.0f, 1.0f);

    bounds.removeFromBottom(10.0f);

    // 3. Center Sync Card
    g.setColour(OpenWavLookAndFeel::bgCard);
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(OpenWavLookAndFeel::borderColour);
    g.drawRoundedRectangle(bounds, 8.0f, 1.0f);

    // Center card accent indicator line
    auto topAccent = bounds.removeFromTop(2.0f).reduced(20.0f, 0.0f);
    g.setColour(OpenWavLookAndFeel::accentCyan);
    g.fillRect(topAccent);
}

void MobileTransferDialog::resized()
{
    auto bounds = getLocalBounds().reduced(14);

    // 1. Top Header Card (56px) - No OWMB logo
    auto headerArea = bounds.removeFromTop(56).reduced(14, 8);
    titleLabel.setBounds(headerArea.removeFromTop(22));
    subtitleLabel.setBounds(headerArea.removeFromTop(16));

    bounds.removeFromTop(10);

    // 2. Bottom Navigation Card (48px)
    auto bottomArea = bounds.removeFromBottom(48).reduced(12, 8);
    openFolderButton.setBounds(bottomArea.removeFromLeft(190).withHeight(30));
    closeButton.setBounds(bottomArea.removeFromRight(90).withHeight(30));

    bounds.removeFromBottom(10);

    // 3. Center Sync Card
    auto centerArea = bounds.reduced(20, 14);

    statusLabel.setBounds(centerArea.removeFromTop(22));
    centerArea.removeFromTop(10);

    syncNowButton.setBounds(centerArea.removeFromTop(48));
    centerArea.removeFromTop(12);

    progressBar.setBounds(centerArea.removeFromTop(20));
    centerArea.removeFromTop(10);

    transferStatusLabel.setBounds(centerArea.removeFromTop(22));
    centerArea.removeFromTop(4);
    fileDetailLabel.setBounds(centerArea.removeFromTop(18));
}

} // namespace openwav
