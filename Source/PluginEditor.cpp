#include "PluginEditor.h"
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

namespace openwav
{

OpenWavAudioProcessorEditor::OpenWavAudioProcessorEditor(OpenWavAudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      headerBar(p.getDatabaseManager(), p.getLibraryScanner()),
      tagPanel(p.getDatabaseManager()),
      sampleTable(p.getDatabaseManager(), p.getAudioEngine()),
      sampleCloud(p.getDatabaseManager(), p.getAudioEngine()),
      librariesComponent(p.getDatabaseManager(), p.getLibraryScanner(), p.getAudioEngine()),
      waveformTransport(p.getAudioEngine())
{
    setLookAndFeel(&lookAndFeel);
    juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel);

    headerBar.addListener(this);
    tagPanel.addListener(this);
    sampleTable.addListener(this);
    sampleCloud.addListener(this);

    addAndMakeVisible(headerBar);
    addAndMakeVisible(tagPanel);
    addAndMakeVisible(sampleTable);
    addChildComponent(sampleCloud);
    addChildComponent(librariesComponent);
    addAndMakeVisible(waveformTransport);

    setResizable(true, true);
    setResizeLimits(800, 650, 3840, 2160);
    setSize(1920, 1080);

    triggerFilterUpdate();
}

OpenWavAudioProcessorEditor::~OpenWavAudioProcessorEditor()
{
    headerBar.removeListener(this);
    tagPanel.removeListener(this);
    sampleTable.removeListener(this);
    sampleCloud.removeListener(this);
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
    setLookAndFeel(nullptr);
}

void OpenWavAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(OpenWavLookAndFeel::bgDark);
}

void OpenWavAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    headerBar.setBounds(area.removeFromTop(54));
    waveformTransport.setBounds(area.removeFromBottom(300));

    // Middle Split View
    tagPanel.setBounds(area.removeFromLeft(220));

    auto mode = headerBar.getCurrentViewMode();

    if (mode == ViewMode::Cloud)
    {
        sampleTable.setVisible(false);
        librariesComponent.setVisible(false);
        sampleCloud.setVisible(true);
        sampleCloud.setBounds(area);
    }
    else if (mode == ViewMode::Libraries)
    {
        sampleTable.setVisible(false);
        sampleCloud.setVisible(false);
        librariesComponent.setVisible(true);
        librariesComponent.setBounds(area);
    }
    else // ViewMode::List
    {
        sampleCloud.setVisible(false);
        librariesComponent.setVisible(false);
        sampleTable.setVisible(true);
        sampleTable.setBounds(area);
    }
}

void OpenWavAudioProcessorEditor::searchTextChanged(const juce::String& /*newText*/)
{
    triggerFilterUpdate();
}

void OpenWavAudioProcessorEditor::formatFilterChanged(const juce::String& /*extension*/)
{
    triggerFilterUpdate();
}

void OpenWavAudioProcessorEditor::addFolderRequested()
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Select Audio Folder to Scan...",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory),
        "*"
    );

    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [this, chooser](const juce::FileChooser& fc) {
            auto result = fc.getResult();
            if (result.isDirectory())
            {
                juce::String folderPath = result.getFullPathName();
                audioProcessor.getDatabaseManager().addScanFolder(folderPath);
                audioProcessor.getLibraryScanner().startScan({ folderPath });
            }
        });
}

void OpenWavAudioProcessorEditor::rescanRequested()
{
    auto folders = audioProcessor.getDatabaseManager().getScanFolders();
    if (!folders.empty())
    {
        audioProcessor.getLibraryScanner().startScan(folders);
    }
    else
    {
        addFolderRequested();
    }
}

void OpenWavAudioProcessorEditor::settingsRequested()
{
    if (juce::JUCEApplicationBase::isStandaloneApp())
    {
        if (auto* sfw = findParentComponentOfClass<juce::StandaloneFilterWindow>())
        {
            if (auto* holder = sfw->getPluginHolder())
            {
                holder->showAudioSettingsDialog();
                return;
            }
        }
    }

    juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::InfoIcon,
        "Audio / MIDI Settings",
        "In VST3 / AU plugin mode, Audio and MIDI devices are managed by your host DAW.",
        "OK"
    );
}

void OpenWavAudioProcessorEditor::tagFilterSelectionChanged(const std::set<juce::String>& /*selectedTags*/, bool /*matchAllTags*/, bool /*favoritesOnly*/)
{
    triggerFilterUpdate();
}

void OpenWavAudioProcessorEditor::sampleSelected(const MediaItem& /*item*/)
{
}

void OpenWavAudioProcessorEditor::sampleDoubleClicked(const MediaItem& /*item*/)
{
}

void OpenWavAudioProcessorEditor::displayedItemsChanged(const std::vector<MediaItem>& items)
{
    sampleCloud.setItems(items);
}

bool OpenWavAudioProcessorEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& f : files)
    {
        juce::File file(f);
        if (file.isDirectory() || file.getFileExtension().containsIgnoreCase("wav") || file.getFileExtension().containsIgnoreCase("mp3"))
            return true;
    }
    return false;
}

void OpenWavAudioProcessorEditor::filesDropped(const juce::StringArray& files, int /*x*/, int /*y*/)
{
    std::vector<juce::String> foldersToScan;
    for (const auto& f : files)
    {
        juce::File file(f);
        if (file.isDirectory())
        {
            foldersToScan.push_back(file.getFullPathName());
            audioProcessor.getDatabaseManager().addScanFolder(file.getFullPathName());
        }
        else if (file.existsAsFile())
        {
            // If parent directory not scanned, scan parent
            auto parent = file.getParentDirectory().getFullPathName();
            foldersToScan.push_back(parent);
            audioProcessor.getDatabaseManager().addScanFolder(parent);
        }
    }

    if (!foldersToScan.empty())
    {
        audioProcessor.getLibraryScanner().startScan(foldersToScan);
    }
}

void OpenWavAudioProcessorEditor::viewModeChanged(ViewMode /*mode*/)
{
    resized();
}

void OpenWavAudioProcessorEditor::parentHierarchyChanged()
{
    juce::AudioProcessorEditor::parentHierarchyChanged();

    if (juce::JUCEApplicationBase::isStandaloneApp())
    {
        if (auto* dw = findParentComponentOfClass<juce::DocumentWindow>())
        {
           #if JUCE_LINUX
            dw->setUsingNativeTitleBar(false);
            dw->setTitleBarHeight(28);
           #else
            dw->setUsingNativeTitleBar(true);
           #endif
            dw->setResizable(true, true);
            dw->setResizeLimits(800, 650, 3840, 2160);

            auto mainDisplay = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
            auto userArea = (mainDisplay != nullptr) ? mainDisplay->userArea : juce::Rectangle<int>(0, 0, 1920, 1080);

            int targetW = std::min(1920, std::max(800, userArea.getWidth() - 40));
            int targetH = std::min(1080, std::max(650, userArea.getHeight() - 60));

            dw->setContentComponentSize(targetW, targetH);
            dw->centreWithSize(targetW, targetH);
        }
    }
}

void OpenWavAudioProcessorEditor::cloudSampleSelected(const MediaItem& /*item*/)
{
}

void OpenWavAudioProcessorEditor::cloudSampleDoubleClicked(const MediaItem& /*item*/)
{
}

void OpenWavAudioProcessorEditor::triggerFilterUpdate()
{
    sampleTable.updateFilter(
        headerBar.getSearchText(),
        tagPanel.getSelectedTags(),
        tagPanel.getMatchAllTags(),
        headerBar.getSelectedFormat(),
        tagPanel.getFavoritesOnly()
    );

    sampleCloud.setItems(sampleTable.getDisplayedItems());
}

} // namespace openwav
