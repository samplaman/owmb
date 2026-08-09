#include "ScanProgressDialog.h"
#include "OpenWavLookAndFeel.h"
#include <cmath>

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

ScanProgressDialog::ScanProgressDialog(LibraryScanner& libraryScanner)
    : scanner(libraryScanner)
{
    scanner.addListener(this);

    setAlwaysOnTop(true);
    setBufferedToImage(true);
    setVisible(false);

    // Title
    titleLabel.setFont(juce::Font(18.0f).boldened());
    titleLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);

    // Status Label
    statusLabel.setFont(juce::Font(14.0f).boldened());
    statusLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textPrimary);
    statusLabel.setJustificationType(juce::Justification::centred);
    statusLabel.setText("Preparing scan...", juce::dontSendNotification);
    addAndMakeVisible(statusLabel);

    // Current File Label
    currentFileLabel.setFont(juce::Font(12.0f));
    currentFileLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    currentFileLabel.setJustificationType(juce::Justification::centred);
    currentFileLabel.setText("", juce::dontSendNotification);
    addAndMakeVisible(currentFileLabel);

    // Stats Labels - Elapsed
    elapsedTimeTitleLabel.setFont(juce::Font(10.0f).boldened());
    elapsedTimeTitleLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    elapsedTimeTitleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(elapsedTimeTitleLabel);

    elapsedTimeValueLabel.setFont(juce::Font(18.0f).boldened());
    elapsedTimeValueLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    elapsedTimeValueLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(elapsedTimeValueLabel);

    // Stats Labels - Estimated
    estimatedTimeTitleLabel.setFont(juce::Font(10.0f).boldened());
    estimatedTimeTitleLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    estimatedTimeTitleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(estimatedTimeTitleLabel);

    estimatedTimeValueLabel.setFont(juce::Font(18.0f).boldened());
    estimatedTimeValueLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textPrimary);
    estimatedTimeValueLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(estimatedTimeValueLabel);

    // Action Button
    actionButton.setButtonText("Cancel Scan");
    actionButton.onClick = [this] {
        if (isScanning.load())
        {
            scanner.cancelScan();
            statusLabel.setText("Cancelling scan...", juce::dontSendNotification);
            actionButton.setEnabled(false);
        }
        else
        {
            hideDialog();
        }
    };
    addAndMakeVisible(actionButton);

    setSize(500, 280);
    lookAndFeelChanged();
}

ScanProgressDialog::~ScanProgressDialog()
{
    stopTimer();
    scanner.removeListener(this);
}

void ScanProgressDialog::lookAndFeelChanged()
{
    titleLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    statusLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textPrimary);
    currentFileLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textPrimary.withAlpha(0.9f));
    elapsedTimeTitleLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    elapsedTimeValueLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    estimatedTimeTitleLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    estimatedTimeValueLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textPrimary);

    repaint();
}

void ScanProgressDialog::showDialog()
{
    lookAndFeelChanged();
    startTimeMs = juce::Time::getMillisecondCounterHiRes();
    elapsedTimeSec = 0.0;
    estimatedTimeRemainingSec = -1.0;
    filesProcessed = 0;
    totalFiles = 0;
    {
        const juce::ScopedLock sl(fileLabelLock);
        currentFileName = "Preparing scan...";
    }
    isScanning = true;
    isFinished = false;

    titleLabel.setText("Scanning Audio Folder", juce::dontSendNotification);
    statusLabel.setText("Discovering audio files...", juce::dontSendNotification);
    currentFileLabel.setText("", juce::dontSendNotification);
    elapsedTimeValueLabel.setText("00:00", juce::dontSendNotification);
    estimatedTimeValueLabel.setText("Calculating...", juce::dontSendNotification);
    actionButton.setButtonText("Cancel Scan");
    actionButton.setEnabled(true);

    if (dialogWindow == nullptr)
    {
        juce::DialogWindow::LaunchOptions opts;
        opts.content.setNonOwned(this);
        opts.dialogTitle = "Scan Progress";
        opts.dialogBackgroundColour = OpenWavLookAndFeel::bgCard;
        opts.escapeKeyTriggersCloseButton = false;
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
    
    startTimer(100);
}

void ScanProgressDialog::hideDialog()
{
    stopTimer();
    if (dialogWindow != nullptr)
        dialogWindow->exitModalState(0);
}

void ScanProgressDialog::mouseDown(const juce::MouseEvent& /*event*/)
{
}

void ScanProgressDialog::scanStarted()
{
    juce::MessageManager::callAsync([this] {
        showDialog();
    });
}

void ScanProgressDialog::scanProgress(int processed, int total, const juce::String& currentFile)
{
    filesProcessed = processed;
    totalFiles = total;
    {
        const juce::ScopedLock sl(fileLabelLock);
        currentFileName = currentFile;
    }
}

void ScanProgressDialog::scanFinished(int totalDiscovered)
{
    isScanning = false;
    isFinished = true;

    juce::MessageManager::callAsync([this, totalDiscovered] {
        titleLabel.setText("Scan Complete", juce::dontSendNotification);
        if (totalDiscovered > 0)
        {
            statusLabel.setText("Successfully scanned " + juce::String(totalDiscovered) + " new audio file" + (totalDiscovered > 1 ? "s" : ""), juce::dontSendNotification);
        }
        else
        {
            statusLabel.setText("Library is up to date. No new files found.", juce::dontSendNotification);
        }
        currentFileLabel.setText("", juce::dontSendNotification);
        estimatedTimeValueLabel.setText("00:00", juce::dontSendNotification);
        actionButton.setButtonText("Close");
        actionButton.setEnabled(true);
        repaint();

        // Auto-close dialogue after 1 second
        juce::Component::SafePointer<ScanProgressDialog> safeThis(this);
        juce::Timer::callAfterDelay(1000, [safeThis] {
            if (safeThis != nullptr)
            {
                safeThis->hideDialog();
            }
        });
    });
}

void ScanProgressDialog::timerCallback()
{
    if (!isScanning.load())
        return;

    double nowMs = juce::Time::getMillisecondCounterHiRes();
    elapsedTimeSec = (nowMs - startTimeMs) / 1000.0;
    elapsedTimeValueLabel.setText(formatTime(elapsedTimeSec), juce::dontSendNotification);

    int proc = filesProcessed.load();
    int tot = totalFiles.load();

    if (tot > 0)
    {
        statusLabel.setText("Processing: " + juce::String(proc) + " / " + juce::String(tot) + " files", juce::dontSendNotification);
    }
    else
    {
        statusLabel.setText("Scanning audio files...", juce::dontSendNotification);
    }

    juce::String nameToDisplay;
    {
        const juce::ScopedLock sl(fileLabelLock);
        nameToDisplay = currentFileName;
    }

    juce::File f(nameToDisplay);
    juce::String displayName = f.getFileName();
    if (displayName.isEmpty())
        displayName = nameToDisplay;

    currentFileLabel.setText(displayName, juce::dontSendNotification);

    if (proc > 2 && tot > proc && elapsedTimeSec > 0.5)
    {
        double rate = static_cast<double>(proc) / elapsedTimeSec;
        if (rate > 0.001)
        {
            double remainingSec = static_cast<double>(tot - proc) / rate;
            estimatedTimeRemainingSec = remainingSec;
            estimatedTimeValueLabel.setText(formatTime(remainingSec), juce::dontSendNotification);
        }
    }
    else if (proc >= tot && tot > 0)
    {
        estimatedTimeValueLabel.setText("00:00", juce::dontSendNotification);
    }
    else
    {
        estimatedTimeValueLabel.setText("Calculating...", juce::dontSendNotification);
    }

    repaint();
}

juce::String ScanProgressDialog::formatTime(double seconds)
{
    if (seconds < 0.0 || std::isnan(seconds) || std::isinf(seconds))
        return "Calculating...";

    int totalSec = static_cast<int>(seconds + 0.5);
    int mins = totalSec / 60;
    int secs = totalSec % 60;
    int hrs = mins / 60;
    mins = mins % 60;

    if (hrs > 0)
        return juce::String::formatted("%02d:%02d:%02d", hrs, mins, secs);
    else
        return juce::String::formatted("%02d:%02d", mins, secs);
}

void ScanProgressDialog::paint(juce::Graphics& g)
{
    g.fillAll(OpenWavLookAndFeel::bgCard);
    auto bounds = getLocalBounds();
    int cw = bounds.getWidth();
    int cx = bounds.getX();
    int cy = bounds.getY();

    // 3. Progress Bar Drawing
    int proc = filesProcessed.load();
    int tot = totalFiles.load();
    float pct = (tot > 0) ? std::min(1.0f, std::max(0.0f, static_cast<float>(proc) / static_cast<float>(tot))) : (isFinished.load() ? 1.0f : 0.0f);

    juce::Rectangle<float> progressTrack(cx + 30.0f, cy + 115.0f, cw - 60.0f, 26.0f);

    // Track Background
    g.setColour(OpenWavLookAndFeel::bgDark);
    g.fillRoundedRectangle(progressTrack, 8.0f);
    g.setColour(OpenWavLookAndFeel::borderColour);
    g.drawRoundedRectangle(progressTrack, 8.0f, 1.0f);

    // Track Fill (Cyan to Blue Gradient)
    if (pct > 0.001f)
    {
        juce::Rectangle<float> progressFill = progressTrack.withWidth(progressTrack.getWidth() * pct);
        juce::ColourGradient fillGrad(OpenWavLookAndFeel::accentCyan, progressFill.getX(), progressFill.getY(),
                                     OpenWavLookAndFeel::accentBlue, progressFill.getRight(), progressFill.getY(), false);
        g.setGradientFill(fillGrad);
        g.fillRoundedRectangle(progressFill, 8.0f);
    }

    // Percentage Text on Progress Bar
    int displayPct = static_cast<int>(std::round(pct * 100.0f));
    juce::String pctStr = juce::String(displayPct) + "%";

    g.setFont(juce::Font(13.0f).boldened());
    g.setColour(juce::Colours::white);
    g.drawText(pctStr, progressTrack, juce::Justification::centred, false);

    // Stats Card Outlines (Elapsed & Estimated Boxes)
    float statsY = bounds.getY() + 152.0f;
    float boxW = (bounds.getWidth() - 70.0f) * 0.5f;
    float boxH = 50.0f;

    juce::Rectangle<float> elapsedBox(cx + 30.0f, statsY, boxW, boxH);
    juce::Rectangle<float> estimatedBox(cx + 40.0f + boxW, statsY, boxW, boxH);

    g.setColour(OpenWavLookAndFeel::bgDark.withAlpha(0.6f));
    g.fillRoundedRectangle(elapsedBox, 8.0f);
    g.fillRoundedRectangle(estimatedBox, 8.0f);

    g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.5f));
    g.drawRoundedRectangle(elapsedBox, 8.0f, 1.0f);
    g.drawRoundedRectangle(estimatedBox, 8.0f, 1.0f);
}

void ScanProgressDialog::resized()
{
    auto bounds = getLocalBounds();
    int cw = bounds.getWidth();
    int cx = bounds.getX();
    int cy = bounds.getY();

    titleLabel.setBounds(cx + 20, cy + 16, cw - 40, 24);
    statusLabel.setBounds(cx + 20, cy + 46, cw - 40, 20);
    currentFileLabel.setBounds(cx + 20, cy + 68, cw - 40, 18);

    // Stats Boxes Layout
    int statsY = cy + 154;
    int boxW = (cw - 70) / 2;

    elapsedTimeTitleLabel.setBounds(cx + 30, statsY + 4, boxW, 14);
    elapsedTimeValueLabel.setBounds(cx + 30, statsY + 20, boxW, 24);

    estimatedTimeTitleLabel.setBounds(cx + 40 + boxW, statsY + 4, boxW, 14);
    estimatedTimeValueLabel.setBounds(cx + 40 + boxW, statsY + 20, boxW, 24);

    // Action Button
    actionButton.setBounds(cx + (cw - 140) / 2, bounds.getBottom() - 48, 140, 32);
}

} // namespace openwav
