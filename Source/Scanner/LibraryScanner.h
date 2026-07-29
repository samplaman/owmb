#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_audio_formats/juce_audio_formats.h>
 #include <juce_core/juce_core.h>
#endif

#include "../Database/TagDatabaseManager.h"
#include "../Models/MediaItem.h"
#include <atomic>

namespace openwav
{

class ScannerListener
{
public:
    virtual ~ScannerListener() = default;
    virtual void scanStarted() = 0;
    virtual void scanProgress(int filesProcessed, int totalFiles, const juce::String& currentFile) = 0;
    virtual void scanFinished(int totalFilesDiscovered) = 0;
};

class LibraryScanner : private juce::Thread
{
public:
    explicit LibraryScanner(TagDatabaseManager& dbManager);
    ~LibraryScanner() override;

    void startScan(const std::vector<juce::String>& folderPaths);
    void cancelScan();
    bool isScanning() const { return isThreadRunning(); }

    void addListener(ScannerListener* listener);
    void removeListener(ScannerListener* listener);

private:
    void run() override;
    MediaItem processAudioFile(const juce::File& file);

    TagDatabaseManager& db;
    juce::AudioFormatManager formatManager;
    std::vector<juce::String> targetFolders;
    std::atomic<bool> cancelRequested { false };
    juce::ListenerList<ScannerListener> listeners;
};

} // namespace openwav
