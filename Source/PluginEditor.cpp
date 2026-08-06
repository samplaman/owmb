#include "PluginEditor.h"
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#if __has_include(<BinaryData.h>)
 #include <BinaryData.h>
#endif

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
#endif

namespace openwav
{

OpenWavAudioProcessorEditor::OpenWavAudioProcessorEditor(OpenWavAudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      headerBar(p.getDatabaseManager(), p.getLibraryScanner()),
      tagPanel(p.getDatabaseManager()),
      leftPanelResizer(*this),
      sampleTable(p.getDatabaseManager(), p.getAudioEngine()),
      sampleCloud(p.getDatabaseManager(), p.getAudioEngine()),
      librariesComponent(p.getDatabaseManager(), p.getLibraryScanner(), p.getAudioEngine()),
      recorderComponent(p.getAudioEngine(), p.getDatabaseManager()),
      waveformTransport(p.getAudioEngine()),
      scanProgressDialog(p.getLibraryScanner())
{
    bool isDark = audioProcessor.getDatabaseManager().isDarkMode();
    OpenWavLookAndFeel::setDarkTheme(isDark);
    lookAndFeel.updateColors();

    setLookAndFeel(&lookAndFeel);
    juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel);

    headerBar.addListener(this);
    tagPanel.addListener(this);
    sampleTable.addListener(this);
    sampleCloud.addListener(this);

    addAndMakeVisible(headerBar);
    addAndMakeVisible(tagPanel);
    addAndMakeVisible(leftPanelResizer);
    addAndMakeVisible(sampleTable);
    addChildComponent(sampleCloud);
    addChildComponent(librariesComponent);
    addChildComponent(recorderComponent);
    addAndMakeVisible(waveformTransport);
    addChildComponent(scanProgressDialog);

    setResizable(true, true);
    setResizeLimits(800, 650, 3840, 2160);
    setSize(1920, 1080);

    triggerFilterUpdate();

    // Trigger startup scan for newly added/modified audio files in existing scan folders
    auto foldersToScan = audioProcessor.getDatabaseManager().getScanFolders();
    if (!foldersToScan.empty())
    {
        audioProcessor.getLibraryScanner().startScan(foldersToScan);
    }
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

void OpenWavAudioProcessorEditor::LeftPanelResizerBar::paint(juce::Graphics& g)
{
    g.fillAll(OpenWavLookAndFeel::bgDark);
    g.setColour(OpenWavLookAndFeel::borderColour);
    g.drawVerticalLine(getWidth() / 2, 0.0f, static_cast<float>(getHeight()));

    // Sleek cyan grip handle in vertical center
    g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.7f));
    float cx = getWidth() * 0.5f;
    float cy = getHeight() * 0.5f;
    g.fillRoundedRectangle(cx - 1.5f, cy - 14.0f, 3.0f, 28.0f, 1.5f);
}

void OpenWavAudioProcessorEditor::LeftPanelResizerBar::mouseDown(const juce::MouseEvent& e)
{
    dragStartX = e.getEventRelativeTo(&owner).x;
    startWidth = owner.tagPanelWidth;
}

void OpenWavAudioProcessorEditor::LeftPanelResizerBar::mouseDrag(const juce::MouseEvent& e)
{
    int currentX = e.getEventRelativeTo(&owner).x;
    int deltaX = currentX - dragStartX;
    owner.setTagPanelWidth(startWidth + deltaX);
}

void OpenWavAudioProcessorEditor::setTagPanelWidth(int newWidth)
{
    int minW = 160;
    int maxW = std::min(600, getWidth() - 300);
    int clampedW = juce::jlimit(minW, maxW, newWidth);

    if (tagPanelWidth != clampedW)
    {
        tagPanelWidth = clampedW;
        resized();
        repaint();
    }
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

    // Left Split View with Resizer Bar
    int currentTagWidth = juce::jlimit(160, std::max(160, area.getWidth() - 300), tagPanelWidth);
    tagPanel.setBounds(area.removeFromLeft(currentTagWidth));
    leftPanelResizer.setBounds(area.removeFromLeft(6));

    auto mode = headerBar.getCurrentViewMode();

    if (mode == ViewMode::Cloud)
    {
        sampleTable.setVisible(false);
        librariesComponent.setVisible(false);
        recorderComponent.setVisible(false);
        sampleCloud.setVisible(true);
        sampleCloud.setBounds(area);
    }
    else if (mode == ViewMode::Libraries)
    {
        sampleTable.setVisible(false);
        sampleCloud.setVisible(false);
        recorderComponent.setVisible(false);
        librariesComponent.setVisible(true);
        librariesComponent.setBounds(area);
    }
    else if (mode == ViewMode::Record)
    {
        sampleTable.setVisible(false);
        sampleCloud.setVisible(false);
        librariesComponent.setVisible(false);
        recorderComponent.setVisible(true);
        recorderComponent.setBounds(area);
    }
    else // ViewMode::List
    {
        sampleCloud.setVisible(false);
        librariesComponent.setVisible(false);
        recorderComponent.setVisible(false);
        sampleTable.setVisible(true);
        sampleTable.setBounds(area);
    }

    scanProgressDialog.setBounds(getLocalBounds());
    scanProgressDialog.toFront(true);
}

void OpenWavAudioProcessorEditor::searchTextChanged(const juce::String& /*newText*/)
{
    startTimer(200); // 200 ms debounce
}

void OpenWavAudioProcessorEditor::searchBarUpPressed()
{
    sampleTable.moveSelection(-1);
}

void OpenWavAudioProcessorEditor::searchBarDownPressed()
{
    sampleTable.moveSelection(1);
}

void OpenWavAudioProcessorEditor::timerCallback()
{
    stopTimer();
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
    juce::PopupMenu menu;
    bool isDark = audioProcessor.getDatabaseManager().isDarkMode();

    menu.addItem(1, "Dark Theme", true, isDark);
    menu.addItem(2, "Light Theme", true, !isDark);
    menu.addSeparator();
    menu.addItem(3, "Audio / MIDI Device Settings...");
    menu.addSeparator();
    menu.addItem(4, "Reset All Library Data...");

    menu.showMenuAsync(juce::PopupMenu::Options(), [this](int result) {
        if (result == 1) // Dark Theme
        {
            audioProcessor.getDatabaseManager().setDarkMode(true);
            OpenWavLookAndFeel::setDarkTheme(true);
            lookAndFeel.updateColors();
            sendLookAndFeelChange();
            updateNativeTitleBarTheme();
        }
        else if (result == 2) // Light Theme
        {
            audioProcessor.getDatabaseManager().setDarkMode(false);
            OpenWavLookAndFeel::setDarkTheme(false);
            lookAndFeel.updateColors();
            sendLookAndFeelChange();
            updateNativeTitleBarTheme();
        }
        else if (result == 3) // Audio/MIDI Settings
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
        else if (result == 4) // Reset All Library Data
        {
            juce::AlertWindow::showAsync(
                juce::MessageBoxOptions()
                    .withIconType(juce::MessageBoxIconType::WarningIcon)
                    .withTitle("Reset All Data")
                    .withMessage("Are you sure you want to reset all library items, custom tags, and scanned folders?\nThis action cannot be undone.")
                    .withButton("Reset Everything")
                    .withButton("Cancel"),
                [this](int alertResult) {
                    if (alertResult == 1)
                    {
                        audioProcessor.getDatabaseManager().clearAllData();
                        tagPanel.clearAllFiltersAndSelection();
                        triggerFilterUpdate();
                    }
                });
        }
    });
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
    if (headerBar.getCurrentViewMode() == ViewMode::Cloud)
    {
        sampleCloud.setItems(items);
    }
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

void OpenWavAudioProcessorEditor::viewModeChanged(ViewMode mode)
{
    resized();
    if (mode == ViewMode::Cloud || mode == ViewMode::List)
    {
        triggerFilterUpdate();
    }
}

void OpenWavAudioProcessorEditor::parentHierarchyChanged()
{
    juce::AudioProcessorEditor::parentHierarchyChanged();

    if (juce::JUCEApplicationBase::isStandaloneApp())
    {
        if (auto* dw = findParentComponentOfClass<juce::DocumentWindow>())
        {
            dw->setLookAndFeel(&lookAndFeel);
            dw->sendLookAndFeelChange();
#if __has_include(<BinaryData.h>)
            auto appIcon = juce::ImageFileFormat::loadFrom(BinaryData::owmbico_png, static_cast<size_t>(BinaryData::owmbico_pngSize));
            if (appIcon.isValid())
            {
                dw->setIcon(appIcon);
            }
#endif
            dw->setUsingNativeTitleBar(true);
            dw->setResizable(true, true);
            dw->setResizeLimits(800, 650, 3840, 2160);

            auto mainDisplay = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
            auto userArea = (mainDisplay != nullptr) ? mainDisplay->userArea : juce::Rectangle<int>(0, 0, 1920, 1080);

            int targetW = std::min(1920, std::max(800, userArea.getWidth() - 40));
            int targetH = std::min(1080, std::max(650, userArea.getHeight() - 60));

            dw->setContentComponentSize(targetW, targetH);
            dw->centreWithSize(targetW, targetH);
            updateNativeTitleBarTheme();
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

    if (headerBar.getCurrentViewMode() == ViewMode::Cloud)
    {
        sampleCloud.setItems(sampleTable.getDisplayedItems());
    }
}

void OpenWavAudioProcessorEditor::updateNativeTitleBarTheme()
{
#if JUCE_WINDOWS
    if (auto* dw = findParentComponentOfClass<juce::DocumentWindow>())
    {
        if (auto* peer = dw->getPeer())
        {
            if (auto hwnd = peer->getNativeHandle())
            {
                BOOL useDarkMode = audioProcessor.getDatabaseManager().isDarkMode() ? TRUE : FALSE;
                
                #ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
                #define DWMWA_USE_IMMERSIVE_DARK_MODE 20
                #endif
                
                typedef HRESULT (WINAPI* DwmSetWindowAttributePtr)(HWND, DWORD, LPCVOID, DWORD);
                
                if (HMODULE dwmDll = LoadLibraryA("dwmapi.dll"))
                {
                    if (auto dwmSetWindowAttribute = (DwmSetWindowAttributePtr)GetProcAddress(dwmDll, "DwmSetWindowAttribute"))
                    {
                        dwmSetWindowAttribute((HWND)hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode));
                        SetWindowPos((HWND)hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
                    }
                    FreeLibrary(dwmDll);
                }
            }
        }
    }
#endif
}

} // namespace openwav
