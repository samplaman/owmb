#include "ConvertDialog.h"
#include "OpenWavLookAndFeel.h"

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

ConvertDialog::ConvertDialog(const MediaItem& item, AudioEngine& engine, TagDatabaseManager& db, const std::vector<juce::AudioFormat*>& writableFormats)
    : currentItem(item), audioEngine(engine), dbManager(db), formats(writableFormats)
{
    addAndMakeVisible(titleLabel);
    titleLabel.setText("Configure target settings for conversion:", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(formatLabel);
    formatLabel.setText("Format:", juce::dontSendNotification);
    formatLabel.setJustificationType(juce::Justification::centredRight);
    
    addAndMakeVisible(formatCombo);
    for (int i = 0; i < formats.size(); ++i)
        formatCombo.addItem(formats[i]->getFormatName(), i + 1);

    // Select matching format by default
    juce::String currentExt = juce::File(item.filePath).getFileExtension().toLowerCase();
    if (currentExt.startsWith(".")) currentExt = currentExt.substring(1);

    for (int idx = 0; idx < formats.size(); ++idx)
    {
        if (formats[idx]->getFileExtensions().contains(currentExt))
        {
            formatCombo.setSelectedId(idx + 1, juce::dontSendNotification);
            break;
        }
    }
    if (formatCombo.getSelectedId() == 0 && formats.size() > 0)
        formatCombo.setSelectedId(1, juce::dontSendNotification);

    addAndMakeVisible(sampleRateLabel);
    sampleRateLabel.setText("Sample Rate:", juce::dontSendNotification);
    sampleRateLabel.setJustificationType(juce::Justification::centredRight);

    addAndMakeVisible(sampleRateCombo);
    sampleRateCombo.addItemList({"Original", "44100 Hz", "48000 Hz", "88200 Hz", "96000 Hz"}, 1);
    sampleRateCombo.setSelectedId(1, juce::dontSendNotification);

    addAndMakeVisible(bitDepthLabel);
    bitDepthLabel.setText("Bit Depth:", juce::dontSendNotification);
    bitDepthLabel.setJustificationType(juce::Justification::centredRight);

    addAndMakeVisible(bitDepthCombo);
    bitDepthCombo.addItemList({"Original", "16-bit", "24-bit", "32-bit Float"}, 1);
    bitDepthCombo.setSelectedId(1, juce::dontSendNotification);

    addAndMakeVisible(convertButton);
    convertButton.onClick = [this] { performConversion(); };
    convertButton.addShortcut(juce::KeyPress(juce::KeyPress::returnKey));

    addAndMakeVisible(cancelButton);
    cancelButton.onClick = [this] { hideDialog(); };
    cancelButton.addShortcut(juce::KeyPress(juce::KeyPress::escapeKey));

    setSize(400, 220);
    lookAndFeelChanged();
}

ConvertDialog::~ConvertDialog()
{
}

void ConvertDialog::lookAndFeelChanged()
{
    titleLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textPrimary);
    formatLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    sampleRateLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    bitDepthLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    
    convertButton.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::accentBlue);
    convertButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    
    cancelButton.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::bgHover);
    cancelButton.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::textPrimary);
}

void ConvertDialog::paint(juce::Graphics& g)
{
    g.fillAll(OpenWavLookAndFeel::bgCard);
}

void ConvertDialog::resized()
{
    auto area = getLocalBounds().reduced(20);
    titleLabel.setBounds(area.removeFromTop(30));
    area.removeFromTop(10);
    
    auto formatArea = area.removeFromTop(24);
    formatLabel.setBounds(formatArea.removeFromLeft(100));
    formatCombo.setBounds(formatArea.reduced(2, 0));

    area.removeFromTop(10);
    auto srArea = area.removeFromTop(24);
    sampleRateLabel.setBounds(srArea.removeFromLeft(100));
    sampleRateCombo.setBounds(srArea.reduced(2, 0));

    area.removeFromTop(10);
    auto bdArea = area.removeFromTop(24);
    bitDepthLabel.setBounds(bdArea.removeFromLeft(100));
    bitDepthCombo.setBounds(bdArea.reduced(2, 0));

    area.removeFromTop(20);
    auto btnArea = area.removeFromTop(30);
    cancelButton.setBounds(btnArea.removeFromRight(100));
    btnArea.removeFromRight(10);
    convertButton.setBounds(btnArea.removeFromRight(120));
}

void ConvertDialog::showDialog()
{
    lookAndFeelChanged();
    setSize(500, 340);
    
    if (dialogWindow == nullptr)
    {
        setSize(500, 340);
        juce::DialogWindow::LaunchOptions opts;
        opts.content.setNonOwned(this);
        opts.dialogTitle = "Convert Audio File";
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

void ConvertDialog::hideDialog()
{
    if (dialogWindow != nullptr)
    {
        dialogWindow->exitModalState(0);
        dialogWindow = nullptr;
    }
    setSize(500, 340);
}

class ConversionTask : public juce::ThreadWithProgressWindow
{
public:
    ConversionTask(const juce::File& src, const juce::File& dst, double tSR, int tBits, juce::AudioFormat* fmt, TagDatabaseManager& db, AudioEngine& engine, const MediaItem& item)
        : juce::ThreadWithProgressWindow("Converting Audio...", true, true),
          sourceFile(src), destFile(dst), targetSampleRate(tSR), targetBitDepth(tBits), targetFormat(fmt), dbManager(db), audioEngine(engine), currentItem(item)
    {
    }

    void run() override
    {
        std::unique_ptr<juce::AudioFormatReader> reader(audioEngine.getFormatManager().createReaderFor(sourceFile));
        if (reader == nullptr)
        {
            showError("Could not open source file.");
            return;
        }

        double srcSR = reader->sampleRate;
        double dstSR = (targetSampleRate > 0.0) ? targetSampleRate : srcSR;
        int srcBits = reader->bitsPerSample;
        int dstBits = (targetBitDepth > 0) ? targetBitDepth : srcBits;

        auto possibleDepths = targetFormat->getPossibleBitDepths();
        if (!possibleDepths.isEmpty() && !possibleDepths.contains(dstBits))
        {
            int bestBits = possibleDepths[0];
            for (int depth : possibleDepths)
            {
                if (depth <= dstBits)
                    bestBits = std::max(bestBits, depth);
            }
            dstBits = bestBits;
        }

        juce::int64 numSamples64 = reader->lengthInSamples;
        if (sourceFile.getFileExtension().toLowerCase() == ".mp3" && srcSR < 32000.0)
            numSamples64 /= 2;

        int numChannels = reader->numChannels;
        if (numSamples64 <= 0 || numSamples64 > 0x7FFFFFFF || numChannels <= 0)
        {
            showError("Invalid audio file properties.");
            return;
        }

        if (destFile.existsAsFile())
            destFile.deleteFile();

        auto outStream = std::make_unique<juce::FileOutputStream>(destFile);
        if (outStream->failedToOpen())
        {
            showError("Could not open destination file for writing.");
            return;
        }

        auto* rawStream = outStream.release();
        std::unique_ptr<juce::AudioFormatWriter> writer(targetFormat->createWriterFor(rawStream, dstSR, numChannels, dstBits, {}, 0));

        if (writer == nullptr)
        {
            showError("Could not create audio writer for format.");
            return;
        }

        bool requiresResampling = std::abs(dstSR - srcSR) > 0.01;
        double speedRatio = srcSR / dstSR;

        std::vector<juce::LagrangeInterpolator> interpolators(static_cast<size_t>(numChannels));

        const int blockSize = 32768;
        juce::AudioBuffer<float> srcBuffer(numChannels, blockSize);

        juce::int64 samplesProcessed = 0;
        bool success = true;

        while (samplesProcessed < numSamples64)
        {
            if (threadShouldExit())
            {
                writer.reset();
                destFile.deleteFile();
                return; // Cancelled
            }

            int numToProcess = static_cast<int>(std::min(static_cast<juce::int64>(blockSize), numSamples64 - samplesProcessed));
            if (!reader->read(&srcBuffer, 0, numToProcess, samplesProcessed, true, true))
            {
                success = false;
                break;
            }

            if (requiresResampling)
            {
                int dstSamples = static_cast<int>(std::round(numToProcess / speedRatio));
                juce::AudioBuffer<float> dstBuffer(numChannels, dstSamples);

                for (int ch = 0; ch < numChannels; ++ch)
                {
                    interpolators[ch].process(speedRatio,
                                              srcBuffer.getReadPointer(ch),
                                              dstBuffer.getWritePointer(ch),
                                              dstSamples);
                }

                if (!writer->writeFromAudioSampleBuffer(dstBuffer, 0, dstSamples))
                {
                    success = false;
                    break;
                }
            }
            else
            {
                if (!writer->writeFromAudioSampleBuffer(srcBuffer, 0, numToProcess))
                {
                    success = false;
                    break;
                }
            }

            samplesProcessed += numToProcess;
            setProgress(static_cast<double>(samplesProcessed) / static_cast<double>(numSamples64));
        }

        writer.reset();

        if (!success)
        {
            destFile.deleteFile();
            showError("Failed to write converted audio data.");
            return;
        }

        // Successfully converted
        MediaItem newItem;
        newItem.filePath = destFile.getFullPathName();
        newItem.fileName = destFile.getFileName();
        newItem.fileExtension = destFile.getFileExtension().toLowerCase();
        newItem.fileSizeBytes = destFile.getSize();
        newItem.dateAddedMs = destFile.getLastModificationTime().toMilliseconds();
        newItem.id = juce::String::toHexString(destFile.getFullPathName().hashCode64());
        
        newItem.tags = TagDatabaseManager::inferTagsFromPath(destFile.getFullPathName());
        newItem.tags.insert("#Converted");
        for (const auto& tag : currentItem.tags)
            newItem.tags.insert(tag);

        newItem.sampleRate = dstSR;
        newItem.numChannels = numChannels;
        newItem.bitDepth = dstBits;
        
        double totalOutputSamples = requiresResampling ? static_cast<double>(numSamples64) / speedRatio : static_cast<double>(numSamples64);
        newItem.durationSeconds = totalOutputSamples / dstSR;

        dbManager.addOrUpdateItem(newItem);
        dbManager.saveToFile();

        juce::MessageManager::callAsync([path = destFile.getFullPathName()] {
            juce::NativeMessageBox::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "Conversion Successful", "File converted successfully and added to library:\n" + path);
        });
    }

private:
    void showError(const juce::String& message)
    {
        juce::MessageManager::callAsync([message] {
            juce::NativeMessageBox::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Conversion Failed", message);
        });
    }

    juce::File sourceFile;
    juce::File destFile;
    double targetSampleRate;
    int targetBitDepth;
    juce::AudioFormat* targetFormat;
    TagDatabaseManager& dbManager;
    AudioEngine& audioEngine;
    MediaItem currentItem;
};

void ConvertDialog::performConversion()
{
    int formatIdx = formatCombo.getSelectedId() - 1;
    if (formatIdx < 0 || formatIdx >= formats.size())
        return;
        
    auto* targetFormat = formats[formatIdx];
    
    int srIdx = sampleRateCombo.getSelectedId();
    double targetSampleRate = 0.0;
    if (srIdx == 2) targetSampleRate = 44100.0;
    else if (srIdx == 3) targetSampleRate = 48000.0;
    else if (srIdx == 4) targetSampleRate = 88200.0;
    else if (srIdx == 5) targetSampleRate = 96000.0;

    int bdIdx = bitDepthCombo.getSelectedId();
    int targetBitDepth = 0;
    if (bdIdx == 2) targetBitDepth = 16;
    else if (bdIdx == 3) targetBitDepth = 24;
    else if (bdIdx == 4) targetBitDepth = 32;
    
    juce::String targetExt = targetFormat->getFileExtensions()[0];
    if (!targetExt.startsWith("."))
        targetExt = "." + targetExt;
        
    auto defaultName = juce::File(currentItem.filePath).getFileNameWithoutExtension() + "_converted" + targetExt;
    
    auto scanFolders = dbManager.getScanFolders();
    juce::File defaultLocation;
    if (!scanFolders.empty())
    {
        bool originalInScanFolder = false;
        juce::File originalFile(currentItem.filePath);
        for (const auto& folder : scanFolders)
        {
            if (originalFile.isAChildOf(juce::File(folder)))
            {
                originalInScanFolder = true;
                break;
            }
        }
        
        if (originalInScanFolder)
            defaultLocation = originalFile.getParentDirectory().getChildFile(defaultName);
        else
            defaultLocation = juce::File(scanFolders[0]).getChildFile(defaultName);
    }
    else
    {
        defaultLocation = juce::File(currentItem.filePath).getParentDirectory().getChildFile(defaultName);
    }
    
    hideDialog();
    
    chooser = std::make_shared<juce::FileChooser>("Save Converted File...", defaultLocation, "*" + targetExt);
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                         [this, targetFormat, targetSampleRate, targetBitDepth, targetExt](const juce::FileChooser& ch) {
        auto destFile = ch.getResult();
        if (destFile == juce::File())
            return;
            
        if (!destFile.getFileExtension().equalsIgnoreCase(targetExt))
            destFile = destFile.withFileExtension(targetExt);
            
        auto task = std::make_shared<ConversionTask>(juce::File(currentItem.filePath), destFile, targetSampleRate, targetBitDepth, targetFormat, dbManager, audioEngine, currentItem);
        if (task->runThread())
        {
            // Thread completed successfully (it manages its own success/error notifications)
        }
    });
}

} // namespace openwav
