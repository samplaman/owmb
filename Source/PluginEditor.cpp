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

namespace {
class CustomSplashScreen : public juce::TopLevelWindow, private juce::Timer {
public:
    CustomSplashScreen(const juce::Image& img, int durationMs) 
        : TopLevelWindow("OWMB Splash", true), totalDuration(durationMs), elapsed(0) {
        
        imageComp.setImage(img, juce::RectanglePlacement::centred);
        addAndMakeVisible(imageComp);
        
        setAlwaysOnTop(true);
        setOpaque(false);
        setDropShadowEnabled(false);
        setSize(img.getWidth(), img.getHeight());
        
        addToDesktop(juce::ComponentPeer::windowIsTemporary |
                     juce::ComponentPeer::windowIgnoresKeyPresses);
                     
        if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
            setCentrePosition(display->userArea.getCentre());
            
        setAlpha(0.0f);
        setVisible(true);
        startTimerHz(60);
    }
    
    ~CustomSplashScreen() override {
        removeFromDesktop();
    }
    
    void resized() override {
        imageComp.setBounds(getLocalBounds());
    }
    
    void paintOverChildren(juce::Graphics& g) override {
        float thickness = 8.0f;
        
        float progress = static_cast<float>(elapsed) / totalDuration;
        float cx = getWidth() * 0.5f;
        float cy = getHeight() * 0.5f;
        float radius = (std::min(getWidth(), getHeight()) * 0.5f) - (thickness * 0.5f);
        
        juce::Path p;
        p.addCentredArc(cx, cy, radius, radius, 0.0f, 0.0f, juce::MathConstants<float>::twoPi * progress, true);
        
        g.setColour(openwav::OpenWavLookAndFeel::accentCyan);
        g.strokePath(p, juce::PathStrokeType(thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    
    void timerCallback() override {
        elapsed += 1000 / 60;
        
        float fadeDuration = 500.0f; // 500ms fade
        float alpha = 1.0f;
        if (elapsed < fadeDuration) {
            alpha = elapsed / fadeDuration;
        } else if (totalDuration - elapsed < fadeDuration) {
            alpha = (totalDuration - elapsed) / fadeDuration;
        }
        setAlpha(juce::jlimit(0.0f, 1.0f, alpha));
        
        if (elapsed >= totalDuration) {
            stopTimer();
            delete this;
        } else {
            repaint();
        }
    }
    
private:
    juce::ImageComponent imageComp;
    int totalDuration;
    int elapsed;
};
} // namespace

namespace openwav {

OpenWavAudioProcessorEditor::OpenWavAudioProcessorEditor(
    OpenWavAudioProcessor &p)
    : AudioProcessorEditor(&p), audioProcessor(p),
      headerBar(p.getDatabaseManager(), p.getLibraryScanner()),
      tagPanel(p.getDatabaseManager()), leftPanelResizer(*this),
      sampleTable(p.getDatabaseManager(), p.getAudioEngine()),
      sampleCloud(p.getDatabaseManager(), p.getAudioEngine()),
      librariesComponent(p.getDatabaseManager(), p.getLibraryScanner(),
                         p.getAudioEngine()),
      recorderComponent(p.getAudioEngine(), p.getDatabaseManager()),
      waveformTransport(p.getAudioEngine()),
      scanProgressDialog(p.getLibraryScanner()) {
  bool isDark = audioProcessor.getDatabaseManager().isDarkMode();
  juce::String savedColourHex =
      audioProcessor.getDatabaseManager().getPrimaryColourHex();
  if (savedColourHex.isNotEmpty()) {
    juce::Colour c = juce::Colour::fromString(savedColourHex);
    if (c.getAlpha() > 0)
      OpenWavLookAndFeel::setPrimaryColour(c);
  }
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

  setResizable(true, true);
  setResizeLimits(800, 650, 3840, 2160);
  setSize(1920, 1080);
  setWantsKeyboardFocus(true);

  addChildComponent(similarityGraphPopup);
  sampleTable.onSimilarityHover = [this](const MediaItem *item,
                                         const MediaItem *target,
                                         juce::Point<int> pos) {
    if (item && target)
      similarityGraphPopup.showComparison(target, item, pos);
    else
      similarityGraphPopup.hidePopup();
  };

  triggerFilterUpdate();

  // Trigger startup scan for newly added/modified audio files in existing scan
  // folders
  auto foldersToScan = audioProcessor.getDatabaseManager().getScanFolders();
  if (!foldersToScan.empty()) {
    scanProgressDialog.setSilent(true);
    audioProcessor.getLibraryScanner().startScan(foldersToScan);
  }

  auto splashImage = juce::ImageCache::getFromMemory(
      BinaryData::splashit_png, BinaryData::splashit_pngSize);
  if (splashImage.isValid()) {
    new CustomSplashScreen(splashImage, 4000);
  }

  juce::Component::SafePointer<OpenWavAudioProcessorEditor> safeThis(this);
  juce::Timer::callAfterDelay(4000, [safeThis] {
    if (safeThis != nullptr) {
      safeThis->uiReady = true;

      if (juce::JUCEApplicationBase::isStandaloneApp()) {
        if (auto *dw =
                safeThis->findParentComponentOfClass<juce::DocumentWindow>()) {
          dw->setAlpha(1.0f);
          dw->setVisible(true);

          // On Linux, the window was moved off-screen to hide it during
          // the splash (since setAlpha(0) is unreliable on X11/Wayland).
          // Restore it to its proper centered position now.
#if JUCE_LINUX || JUCE_BSD
          auto mainDisplay =
              juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
          auto userArea = (mainDisplay != nullptr)
                              ? mainDisplay->userArea
                              : juce::Rectangle<int>(0, 0, 1920, 1080);
          int targetW = dw->getWidth();
          int targetH = dw->getHeight();
          dw->centreWithSize(targetW, targetH);
#endif
          dw->toFront(true);
        }
      }

      safeThis->headerBar.setVisible(true);
      safeThis->tagPanel.setVisible(true);
      safeThis->leftPanelResizer.setVisible(true);
      safeThis->waveformTransport.setVisible(true);

      safeThis->resized();
      safeThis->repaint();
    }
  });
}

OpenWavAudioProcessorEditor::~OpenWavAudioProcessorEditor() {
  headerBar.removeListener(this);
  tagPanel.removeListener(this);
  sampleTable.removeListener(this);
  sampleCloud.removeListener(this);
  juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
  setLookAndFeel(nullptr);
}

void OpenWavAudioProcessorEditor::LeftPanelResizerBar::paint(
    juce::Graphics &g) {
  g.fillAll(OpenWavLookAndFeel::bgDark);
  g.setColour(OpenWavLookAndFeel::borderColour);
  g.drawVerticalLine(getWidth() / 2, 0.0f, static_cast<float>(getHeight()));

  // Sleek cyan grip handle in vertical center
  g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.7f));
  float cx = getWidth() * 0.5f;
  float cy = getHeight() * 0.5f;
  g.fillRoundedRectangle(cx - 1.5f, cy - 14.0f, 3.0f, 28.0f, 1.5f);
}

void OpenWavAudioProcessorEditor::LeftPanelResizerBar::mouseDown(
    const juce::MouseEvent &e) {
  dragStartX = e.getEventRelativeTo(&owner).x;
  startWidth = owner.tagPanelWidth;
}

void OpenWavAudioProcessorEditor::LeftPanelResizerBar::mouseDrag(
    const juce::MouseEvent &e) {
  int currentX = e.getEventRelativeTo(&owner).x;
  int deltaX = currentX - dragStartX;
  owner.setTagPanelWidth(startWidth + deltaX);
}

void OpenWavAudioProcessorEditor::setTagPanelWidth(int newWidth) {
  int minW = 160;
  int maxW = std::min(600, getWidth() - 300);
  int clampedW = juce::jlimit(minW, maxW, newWidth);

  if (tagPanelWidth != clampedW) {
    tagPanelWidth = clampedW;
    resized();
    repaint();
  }
}

void OpenWavAudioProcessorEditor::paint(juce::Graphics &g) {
  g.fillAll(OpenWavLookAndFeel::bgDark);
}

void OpenWavAudioProcessorEditor::resized() {
  if (!uiReady) {
    headerBar.setVisible(false);
    tagPanel.setVisible(false);
    leftPanelResizer.setVisible(false);
    sampleTable.setVisible(false);
    sampleCloud.setVisible(false);
    librariesComponent.setVisible(false);
    recorderComponent.setVisible(false);
    waveformTransport.setVisible(false);
    return;
  }

  auto area = getLocalBounds();

  headerBar.setBounds(area.removeFromTop(54));
  waveformTransport.setBounds(area.removeFromBottom(300));

  // Left Split View with Resizer Bar
  int currentTagWidth =
      juce::jlimit(160, std::max(160, area.getWidth() - 300), tagPanelWidth);
  tagPanel.setBounds(area.removeFromLeft(currentTagWidth));
  leftPanelResizer.setBounds(area.removeFromLeft(6));

  auto mode = headerBar.getCurrentViewMode();

  if (mode == ViewMode::Cloud) {
    sampleTable.setVisible(false);
    librariesComponent.setVisible(false);
    recorderComponent.setVisible(false);
    sampleCloud.setVisible(true);
    sampleCloud.setBounds(area);
  } else if (mode == ViewMode::Libraries) {
    sampleTable.setVisible(false);
    sampleCloud.setVisible(false);
    recorderComponent.setVisible(false);
    librariesComponent.setVisible(true);
    librariesComponent.setBounds(area);
  } else if (mode == ViewMode::Record) {
    sampleTable.setVisible(false);
    sampleCloud.setVisible(false);
    librariesComponent.setVisible(false);
    recorderComponent.setVisible(true);
    recorderComponent.setBounds(area);
  } else // ViewMode::List
  {
    sampleCloud.setVisible(false);
    librariesComponent.setVisible(false);
    recorderComponent.setVisible(false);
    sampleTable.setVisible(true);
    sampleTable.setBounds(area);
  }
}

void OpenWavAudioProcessorEditor::searchTextChanged(
    const juce::String & /*newText*/) {
  startTimer(200); // 200 ms debounce
}

void OpenWavAudioProcessorEditor::searchBarUpPressed() {
  sampleTable.moveSelection(-1);
}

void OpenWavAudioProcessorEditor::searchBarDownPressed() {
  sampleTable.moveSelection(1);
}

void OpenWavAudioProcessorEditor::timerCallback() {
  stopTimer();
  triggerFilterUpdate();
}

void OpenWavAudioProcessorEditor::formatFilterChanged(
    const juce::String & /*extension*/) {
  triggerFilterUpdate();
}

void OpenWavAudioProcessorEditor::addFolderRequested() {
  auto chooser = std::make_shared<juce::FileChooser>(
      "Select Audio Folder to Scan...",
      juce::File::getSpecialLocation(juce::File::userHomeDirectory), "*");

  chooser->launchAsync(
      juce::FileBrowserComponent::openMode |
          juce::FileBrowserComponent::canSelectDirectories,
      [this, chooser](const juce::FileChooser &fc) {
        auto result = fc.getResult();
        if (result.isDirectory()) {
          juce::String folderPath = result.getFullPathName();
          audioProcessor.getDatabaseManager().addScanFolder(folderPath);
          audioProcessor.getLibraryScanner().startScan({folderPath});
        }
      });
}

void OpenWavAudioProcessorEditor::rescanRequested() {
  auto folders = audioProcessor.getDatabaseManager().getScanFolders();
  if (!folders.empty()) {
    audioProcessor.getLibraryScanner().startScan(folders);
  } else {
    addFolderRequested();
  }
}

void OpenWavAudioProcessorEditor::settingsRequested() {
  juce::PopupMenu menu;
  bool isDark = audioProcessor.getDatabaseManager().isDarkMode();

  menu.addItem(1, "Dark Theme", true, isDark);
  menu.addItem(2, "Light Theme", true, !isDark);

  juce::PopupMenu colourSubMenu;
  colourSubMenu.addItem(10, "Cyan (Default #00C8DC)");
  colourSubMenu.addItem(11, "Electric Blue (#008CFF)");
  colourSubMenu.addItem(12, "Neon Purple (#A040FF)");
  colourSubMenu.addItem(13, "Emerald Green (#00E676)");
  colourSubMenu.addItem(14, "Sunset Amber (#FFAB00)");
  colourSubMenu.addItem(15, "Crimson Red (#FF3D00)");
  colourSubMenu.addSeparator();
  colourSubMenu.addItem(16, "Custom Colour Picker...");
  colourSubMenu.addItem(17, "Reset to Theme Default");

  menu.addSubMenu("Primary UI Colour", colourSubMenu);

  menu.addSeparator();
  menu.addItem(3, "Audio / MIDI Device Settings...");
  menu.addSeparator();
  menu.addItem(4, "Reset All Library Data...");
  menu.addSeparator();
  menu.addItem(5, "About OWMB...");

  menu.showMenuAsync(juce::PopupMenu::Options(), [this](int result) {
    auto applyColour = [this](juce::Colour c, const juce::String &hexStr) {
      OpenWavLookAndFeel::setPrimaryColour(c);
      audioProcessor.getDatabaseManager().setPrimaryColourHex(hexStr);
      lookAndFeel.updateColors();
      sendLookAndFeelChange();
      repaint();
    };

    if (result == 1) // Dark Theme
    {
      audioProcessor.getDatabaseManager().setDarkMode(true);
      OpenWavLookAndFeel::setDarkTheme(true);
      lookAndFeel.updateColors();
      sendLookAndFeelChange();
      updateNativeTitleBarTheme();
    } else if (result == 2) // Light Theme
    {
      audioProcessor.getDatabaseManager().setDarkMode(false);
      OpenWavLookAndFeel::setDarkTheme(false);
      lookAndFeel.updateColors();
      sendLookAndFeelChange();
      updateNativeTitleBarTheme();
    } else if (result == 10) // Cyan
      applyColour(juce::Colour::fromRGB(0, 200, 220), "#ff00c8dc");
    else if (result == 11) // Electric Blue
      applyColour(juce::Colour::fromRGB(0, 140, 255), "#ff008cff");
    else if (result == 12) // Neon Purple
      applyColour(juce::Colour::fromRGB(160, 64, 255), "#ffa040ff");
    else if (result == 13) // Emerald Green
      applyColour(juce::Colour::fromRGB(0, 230, 118), "#ff00e676");
    else if (result == 14) // Sunset Amber
      applyColour(juce::Colour::fromRGB(255, 171, 0), "#ffffab00");
    else if (result == 15) // Crimson Red
      applyColour(juce::Colour::fromRGB(255, 61, 0), "#ffff3d00");
    else if (result == 17) // Reset to Default
    {
      OpenWavLookAndFeel::resetPrimaryColour();
      audioProcessor.getDatabaseManager().setPrimaryColourHex("");
      lookAndFeel.updateColors();
      sendLookAndFeelChange();
      repaint();
    } else if (result == 16) // Custom Colour Picker...
    {
      auto *selector =
          new juce::ColourSelector(juce::ColourSelector::showColourAtTop |
                                   juce::ColourSelector::showSliders |
                                   juce::ColourSelector::showColourspace);
      selector->setCurrentColour(OpenWavLookAndFeel::accentCyan);
      selector->setSize(340, 300);

      class ColourPickerWindow : public juce::DialogWindow,
                                 public juce::ChangeListener {
      public:
        ColourPickerWindow(juce::ColourSelector *selectorComp,
                           std::function<void(juce::Colour)> onColor)
            : DialogWindow("Custom Primary UI Colour",
                           OpenWavLookAndFeel::bgDark, true, true),
              onColor(onColor), selector(selectorComp) {
          setContentOwned(selectorComp, true);
          selectorComp->addChangeListener(this);
          setUsingNativeTitleBar(true);
          setResizable(false, false);
          centreWithSize(getWidth(), getHeight());
          setVisible(true);
        }

        void closeButtonPressed() override { setVisible(false); }

        void changeListenerCallback(juce::ChangeBroadcaster *) override {
          if (selector && onColor)
            onColor(selector->getCurrentColour());
        }

      private:
        std::function<void(juce::Colour)> onColor;
        juce::ColourSelector *selector;
      };

      new ColourPickerWindow(selector, [applyColour](juce::Colour newC) {
        applyColour(newC, newC.toString());
      });
    } else if (result == 3) // Audio/MIDI Settings
    {
      if (juce::JUCEApplicationBase::isStandaloneApp()) {
        if (auto *sfw =
                findParentComponentOfClass<juce::StandaloneFilterWindow>()) {
          if (auto *holder = sfw->getPluginHolder()) {
            holder->showAudioSettingsDialog();
            return;
          }
        }
      }

      juce::AlertWindow::showMessageBoxAsync(
          juce::AlertWindow::InfoIcon, "Audio / MIDI Settings",
          "In VST3 / AU plugin mode, Audio and MIDI devices are managed by "
          "your host DAW.",
          "OK");
    } else if (result == 4) // Reset All Library Data
    {
      juce::AlertWindow::showAsync(
          juce::MessageBoxOptions()
              .withIconType(juce::MessageBoxIconType::WarningIcon)
              .withTitle("Reset All Data")
              .withMessage(
                  "Are you sure you want to reset all library items, custom "
                  "tags, and scanned folders?\nThis action cannot be undone.")
              .withButton("Reset Everything")
              .withButton("Cancel"),
          [this](int alertResult) {
            if (alertResult == 1) {
              audioProcessor.getDatabaseManager().clearAllData();
              tagPanel.clearAllFiltersAndSelection();
              triggerFilterUpdate();
            }
          });
    } else if (result == 5) // About OWMB...
    {
      aboutDialog.showDialog();
    }
  });
}

void OpenWavAudioProcessorEditor::tagFilterSelectionChanged(
    const std::set<juce::String> & /*selectedTags*/, bool /*matchAllTags*/,
    bool /*favoritesOnly*/) {
  triggerFilterUpdate();
}

void OpenWavAudioProcessorEditor::sampleSelected(const MediaItem &item) {
  sampleCloud.selectItemById(item.id);
}

void OpenWavAudioProcessorEditor::sampleDoubleClicked(
    const MediaItem & /*item*/) {}

void OpenWavAudioProcessorEditor::displayedItemsChanged(
    const std::vector<MediaItem> &items) {
  if (headerBar.getCurrentViewMode() == ViewMode::Cloud) {
    sampleCloud.setItems(items);
  }
}

bool OpenWavAudioProcessorEditor::isInterestedInFileDrag(
    const juce::StringArray &files) {
  for (const auto &f : files) {
    juce::File file(f);
    if (file.isDirectory() ||
        file.getFileExtension().containsIgnoreCase("wav") ||
        file.getFileExtension().containsIgnoreCase("mp3"))
      return true;
  }
  return false;
}

void OpenWavAudioProcessorEditor::filesDropped(const juce::StringArray &files,
                                               int /*x*/, int /*y*/) {
  std::vector<juce::String> foldersToScan;
  for (const auto &f : files) {
    juce::File file(f);
    if (file.isDirectory()) {
      foldersToScan.push_back(file.getFullPathName());
      audioProcessor.getDatabaseManager().addScanFolder(file.getFullPathName());
    } else if (file.existsAsFile()) {
      // If parent directory not scanned, scan parent
      auto parent = file.getParentDirectory().getFullPathName();
      foldersToScan.push_back(parent);
      audioProcessor.getDatabaseManager().addScanFolder(parent);
    }
  }

  if (!foldersToScan.empty()) {
    audioProcessor.getLibraryScanner().startScan(foldersToScan);
  }
}

void OpenWavAudioProcessorEditor::viewModeChanged(ViewMode mode) {
  resized();
  if (mode == ViewMode::Cloud || mode == ViewMode::List) {
    triggerFilterUpdate();
  }
}

void OpenWavAudioProcessorEditor::parentHierarchyChanged() {
  juce::AudioProcessorEditor::parentHierarchyChanged();

  if (juce::JUCEApplicationBase::isStandaloneApp()) {
    if (auto *dw = findParentComponentOfClass<juce::DocumentWindow>()) {
      dw->setLookAndFeel(&lookAndFeel);
      dw->sendLookAndFeelChange();
#if __has_include(<BinaryData.h>)
      auto appIcon = juce::ImageFileFormat::loadFrom(
          BinaryData::splashit_png,
          static_cast<size_t>(BinaryData::splashit_pngSize));
      if (appIcon.isValid()) {
        dw->setIcon(appIcon);
      }
#endif
      dw->setUsingNativeTitleBar(true);
      dw->setTitleBarButtonsRequired(juce::DocumentWindow::allButtons, false);
      dw->setResizable(true, true);
      dw->setResizeLimits(800, 650, 3840, 2160);

      auto mainDisplay =
          juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
      auto userArea = (mainDisplay != nullptr)
                          ? mainDisplay->userArea
                          : juce::Rectangle<int>(0, 0, 1920, 1080);

      int targetW = std::min(1920, std::max(800, userArea.getWidth() - 40));
      int targetH = std::min(1080, std::max(650, userArea.getHeight() - 60));

      dw->setContentComponentSize(targetW, targetH);
      dw->centreWithSize(targetW, targetH);
      updateNativeTitleBarTheme();

      if (!uiReady) {
        dw->setAlpha(0.0f);
        dw->setVisible(false);

        // On Linux, setAlpha(0) is unreliable (depends on compositor support).
        // Move the window far off-screen so it can't flash on screen.
#if JUCE_LINUX || JUCE_BSD
        dw->setTopLeftPosition(-10000, -10000);
#endif
      }
    }
  }
}

void OpenWavAudioProcessorEditor::cloudSampleSelected(const MediaItem &item) {
  sampleTable.selectItemById(item.id);
}

void OpenWavAudioProcessorEditor::cloudSampleDoubleClicked(
    const MediaItem &item) {}

void OpenWavAudioProcessorEditor::triggerFilterUpdate() {
  sampleTable.updateFilter(
      headerBar.getSearchText(), tagPanel.getSelectedTags(),
      tagPanel.getMatchAllTags(), headerBar.getSelectedFormat(),
      tagPanel.getFavoritesOnly());

  if (headerBar.getCurrentViewMode() == ViewMode::Cloud) {
    sampleCloud.setItems(sampleTable.getDisplayedItems());
  }
}

void OpenWavAudioProcessorEditor::updateNativeTitleBarTheme() {
#if JUCE_WINDOWS
  if (auto *dw = findParentComponentOfClass<juce::DocumentWindow>()) {
    if (auto *peer = dw->getPeer()) {
      if (auto hwnd = peer->getNativeHandle()) {
        BOOL useDarkMode =
            audioProcessor.getDatabaseManager().isDarkMode() ? TRUE : FALSE;

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

        typedef HRESULT(WINAPI * DwmSetWindowAttributePtr)(HWND, DWORD, LPCVOID,
                                                           DWORD);

        if (HMODULE dwmDll = LoadLibraryA("dwmapi.dll")) {
          if (auto dwmSetWindowAttribute =
                  (DwmSetWindowAttributePtr)GetProcAddress(
                      dwmDll, "DwmSetWindowAttribute")) {
            dwmSetWindowAttribute((HWND)hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE,
                                  &useDarkMode, sizeof(useDarkMode));
            SetWindowPos((HWND)hwnd, nullptr, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                             SWP_FRAMECHANGED);
          }
          FreeLibrary(dwmDll);
        }
      }
    }
  }
#endif
}

bool OpenWavAudioProcessorEditor::keyPressed(const juce::KeyPress &key) {
  if (key.getTextCharacter() == 'l' || key.getTextCharacter() == 'L') {
    waveformTransport.toggleLoop();
    return true;
  }
  if (key.getTextCharacter() == 's' || key.getTextCharacter() == 'S') {
    waveformTransport.triggerSlice();
    return true;
  }
  if (key == juce::KeyPress::spaceKey) {
    waveformTransport.togglePlay();
    return true;
  }

  return juce::AudioProcessorEditor::keyPressed(key);
}

} // namespace openwav
