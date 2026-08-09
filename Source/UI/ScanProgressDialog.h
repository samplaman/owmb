#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_gui_basics/juce_gui_basics.h>
#endif

#include "../Scanner/LibraryScanner.h"
#include <atomic>

namespace openwav
{

class ScanProgressDialog : public juce::Component,
                           public ScannerListener,
                           private juce::Timer
{
public:
    explicit ScanProgressDialog(LibraryScanner& scanner);
    ~ScanProgressDialog() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void lookAndFeelChanged() override;

    // ScannerListener Callbacks
    void scanStarted() override;
    void scanProgress(int filesProcessed, int totalFiles, const juce::String& currentFile) override;
    void scanFinished(int totalFilesDiscovered) override;

    void showDialog();
    void hideDialog();

private:
    void timerCallback() override;
    static juce::String formatTime(double seconds);

    LibraryScanner& scanner;

    std::atomic<int> filesProcessed { 0 };
    std::atomic<int> totalFiles { 0 };
    juce::CriticalSection fileLabelLock;
    juce::String currentFileName { "Scanning..." };
    std::atomic<bool> isScanning { false };
    std::atomic<bool> isFinished { false };

    double startTimeMs { 0.0 };
    double elapsedTimeSec { 0.0 };
    double estimatedTimeRemainingSec { -1.0 };

    juce::Label titleLabel { {}, "Scanning Audio Folder" };
    juce::Label statusLabel;
    juce::Label currentFileLabel;

    juce::Label elapsedTimeTitleLabel { {}, "ELAPSED TIME" };
    juce::Label elapsedTimeValueLabel { {}, "00:00" };

    juce::Label estimatedTimeTitleLabel { {}, "ESTIMATED REMAINING" };
    juce::Label estimatedTimeValueLabel { {}, "Calculating..." };

    juce::TextButton actionButton { "Cancel Scan" };

    juce::Component::SafePointer<juce::DialogWindow> dialogWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScanProgressDialog)
};

} // namespace openwav
