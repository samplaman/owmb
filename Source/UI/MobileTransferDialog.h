#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_gui_basics/juce_gui_basics.h>
 #include <juce_audio_basics/juce_audio_basics.h>
#endif

#include "../Network/MobileSyncManager.h"
#include "../Database/TagDatabaseManager.h"
#include "../Scanner/LibraryScanner.h"
#include "../Audio/AudioEngine.h"
#include "StudioIcons.h"

namespace openwav
{

class MobileTransferDialog : public juce::Component,
                             public MobileSyncListener,
                             public juce::Timer
{
public:
    MobileTransferDialog(AudioEngine& engine, TagDatabaseManager& dbManager, LibraryScanner& scanner);
    ~MobileTransferDialog() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void lookAndFeelChanged() override;

    void showDialog();
    void hideDialog();

    // MobileSyncListener callbacks
    void discoveredDevicesChanged() override;
    void fileReceivedFromPhone(const juce::File& file) override;

    void timerCallback() override;

private:
    void updateDiscoveredState();
    void startOneButtonSync();
    void syncAllFromDevice(const juce::String& host, int port);
    void importFileToDatabase(const juce::File& file);

    AudioEngine& audioEngine;
    TagDatabaseManager& dbManager;
    LibraryScanner& libraryScanner;

    MobileSyncManager syncManager;

    // Header UI (No OWMB Logo as requested)
    juce::Label titleLabel { {}, "FIELD RECORDER SYNC" };
    juce::Label subtitleLabel { {}, "1-Click Wireless Library Transfer & Direct Import" };

    // Device Status
    juce::Label statusLabel;

    // The Master 1-Button Sync
    SvgIconButton syncNowButton { "SYNC RECORDINGS FROM PHONE", StudioIcons::getDownloadSvg() };

    // Progress & Summary
    juce::ProgressBar progressBar;
    double currentProgress { 0.0 };
    juce::Label transferStatusLabel;
    juce::Label fileDetailLabel;

    // Bottom Navigation
    SvgIconButton openFolderButton { "Open Recordings Folder", StudioIcons::getFolderSvg() };
    juce::TextButton closeButton { "Close" };

    // Internal State
    juce::String activeDeviceHost;
    int activeDevicePort { 7777 };
    juce::String activeDeviceName;
    bool isConnected { false };
    bool isSyncing { false };
    int downloadQueueIndex { -1 };
    std::vector<MobileRecordingItem> pendingItemsToDownload;

    juce::Component::SafePointer<juce::DialogWindow> dialogWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MobileTransferDialog)
};

} // namespace openwav
