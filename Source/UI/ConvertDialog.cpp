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
    
    if (dialogWindow == nullptr)
    {
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
        dialogWindow->exitModalState(0);
}

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
            
        std::unique_ptr<juce::AudioFormatReader> reader(audioEngine.getFormatManager().createReaderFor(juce::File(currentItem.filePath)));
        if (reader == nullptr)
        {
            juce::NativeMessageBox::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Conversion Failed", "Could not open source file.");
            return;
        }
        
        double srcSR = reader->sampleRate;
        double dstSR = (targetSampleRate > 0.0) ? targetSampleRate : srcSR;
        int srcBits = reader->bitsPerSample;
        int dstBits = (targetBitDepth > 0) ? targetBitDepth : srcBits;
        
        juce::int64 numSamples64 = reader->lengthInSamples;
        int numChannels = reader->numChannels;
        
        if (numSamples64 <= 0 || numSamples64 > 0x7FFFFFFF || numChannels <= 0)
        {
            juce::NativeMessageBox::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Conversion Failed", "Invalid audio file properties.");
            return;
        }
        
        int numSamples = static_cast<int>(numSamples64);
        juce::AudioBuffer<float> srcBuffer(numChannels, numSamples);
        if (!reader->read(&srcBuffer, 0, numSamples, 0, true, true))
        {
            juce::NativeMessageBox::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Conversion Failed", "Failed to read audio data from source.");
            return;
        }
        
        auto outStream = std::make_unique<juce::FileOutputStream>(destFile);
        if (outStream->failedToOpen())
        {
            juce::NativeMessageBox::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Conversion Failed", "Could not open destination file for writing.");
            return;
        }
        
        outStream->setPosition(0);
        outStream->truncate();
        
        std::unique_ptr<juce::AudioFormatWriter> writer(targetFormat->createWriterFor(outStream.get(), dstSR, numChannels, dstBits, {}, 0));
        if (writer == nullptr)
        {
            juce::NativeMessageBox::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Conversion Failed", "Could not create audio writer for format.");
            return;
        }
        
        outStream.release(); // writer takes ownership
        bool success = writer->writeFromAudioSampleBuffer(srcBuffer, 0, numSamples);
        writer.reset();
        
        if (success)
        {
            MediaItem newItem;
            newItem.id = juce::Uuid().toString();
            newItem.filePath = destFile.getFullPathName();
            newItem.fileName = destFile.getFileName();
            newItem.durationSeconds = numSamples / dstSR;
            newItem.isFavorite = false;
            
            newItem.tags.insert("#Converted");
            for (const auto& tag : currentItem.tags)
                newItem.tags.insert(tag);
                
            dbManager.addOrUpdateItem(newItem);
            dbManager.saveToFile();
            
            juce::NativeMessageBox::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "Conversion Successful", "File converted successfully and added to library:\n" + destFile.getFullPathName());
        }
        else
        {
            juce::NativeMessageBox::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Conversion Failed", "Failed to write converted audio data.");
        }
    });
}

} // namespace openwav
