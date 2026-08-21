#include "PluginEditor.h"
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#if __has_include(<BinaryData.h>)
#include <BinaryData.h>
#endif

#if JUCE_MAC
#include <unistd.h>
#endif

#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace {
    WNDPROC originalWndProc = nullptr;
    openwav::OpenWavAudioProcessorEditor* activeEditorPtr = nullptr;

    LRESULT CALLBACK NativeCloseSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (msg == WM_CLOSE) {
            if (activeEditorPtr != nullptr) {
                activeEditorPtr->triggerExitSequence();
                return 0;
            }
        }
        if (originalWndProc != nullptr)
            return CallWindowProc(originalWndProc, hwnd, msg, wParam, lParam);
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}
#endif



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
      scanProgressDialog(p.getLibraryScanner()),
      editComponent(p.getAudioEngine()),
      sampleMapComponent(p.getAudioEngine()),
      performanceComponent(p.getDatabaseManager(), p.getAudioEngine()) {
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
  addChildComponent(analysisComponent);
  addChildComponent(editComponent);
  addChildComponent(sampleMapComponent);
  addChildComponent(performanceComponent);
  addAndMakeVisible(waveformTransport);

  setResizable(true, true);
  setResizeLimits(800, 650, 3840, 2160);
  setSize(1920, 1080);
  setWantsKeyboardFocus(true);

  float savedScale = audioProcessor.getDatabaseManager().getUiScale();
  if (savedScale >= 0.70f && savedScale <= 2.0f && std::abs(savedScale - 1.0f) > 0.001f) {
    setScaleFactor(savedScale);
  }

  addChildComponent(similarityGraphPopup);
  sampleTable.onSimilarityHover = [this](const MediaItem *item,
                                         const MediaItem *target,
                                         juce::Point<int> pos) {
    if (item && target)
      similarityGraphPopup.showComparison(target, item, pos);
    else
      similarityGraphPopup.hidePopup();
  };

  sampleMapComponent.onLoadToPerformance = [this](const std::vector<SampleMapZone>& zones) {
    performanceComponent.loadSlices(zones);
    headerBar.setViewMode(ViewMode::Performance);
  };

  sampleMapComponent.onSliceToSamplerStarted = [this] {
    performanceComponent.clearAllPads();
  };

  // Broadcast current LookAndFeel theme and colours to all sub-components
  sendLookAndFeelChange();

  triggerFilterUpdate();

  // Trigger startup scan for newly added/modified audio files in existing scan
  // folders
  auto foldersToScan = audioProcessor.getDatabaseManager().getScanFolders();
  if (!foldersToScan.empty()) {
    scanProgressDialog.setSilent(true);
    audioProcessor.getLibraryScanner().startScan(foldersToScan);
  }

  uiReady = true;

  if (juce::JUCEApplicationBase::isStandaloneApp()) {
    if (auto *dw = findParentComponentOfClass<juce::DocumentWindow>()) {
      dw->setAlpha(1.0f);
      dw->setVisible(true);
      dw->toFront(true);
    }
  }

  headerBar.setVisible(true);
  tagPanel.setVisible(true);
  leftPanelResizer.setVisible(true);
  waveformTransport.setVisible(true);

  resized();
  repaint();
}

OpenWavAudioProcessorEditor::~OpenWavAudioProcessorEditor() {
#if JUCE_WINDOWS
  activeEditorPtr = nullptr;
#endif
  audioProcessor.getLibraryScanner().cancelScan();
  audioProcessor.getAudioEngine().stop();
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

void OpenWavAudioProcessorEditor::triggerExitSequence() {
  if (isExiting)
    return;
  isExiting = true;
  audioProcessor.getAudioEngine().stop();
  audioProcessor.getLibraryScanner().cancelScan();
  repaint();

  juce::MessageManager::callAsync([] {
    if (auto *app = juce::JUCEApplicationBase::getInstance())
      app->systemRequestedQuit();
  });
}

void OpenWavAudioProcessorEditor::paint(juce::Graphics &g) {
  g.fillAll(OpenWavLookAndFeel::bgDark);
}

void OpenWavAudioProcessorEditor::paintOverChildren(juce::Graphics &g) {
  if (isExiting) {
    g.fillAll(juce::Colours::darkgrey.withAlpha(0.85f));
    g.setColour(OpenWavLookAndFeel::textPrimary);
    g.setFont(juce::Font(22.0f, juce::Font::bold));
    g.drawText("Closing OpenWav Media Browser...", getLocalBounds(),
               juce::Justification::centred, true);
  }
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
    analysisComponent.setVisible(false);
    editComponent.setVisible(false);
    sampleMapComponent.setVisible(false);
    performanceComponent.setVisible(false);
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
  bool midiAllowed = (mode == ViewMode::Edit || mode == ViewMode::SampleMap || mode == ViewMode::Performance);
  audioProcessor.getAudioEngine().setMidiInputEnabled(midiAllowed);

  sampleTable.setVisible(false);
  sampleCloud.setVisible(false);
  librariesComponent.setVisible(false);
  recorderComponent.setVisible(false);
  analysisComponent.setVisible(false);
  editComponent.setVisible(false);
  sampleMapComponent.setVisible(false);
  performanceComponent.setVisible(false);

  if (mode == ViewMode::Cloud) {
    sampleCloud.setVisible(true);
    float s = audioProcessor.getDatabaseManager().getUiScale();
    if (s <= 0.01f) s = 1.0f;
    if (std::abs(s - 1.0f) > 0.001f) {
      sampleCloud.setTransform(juce::AffineTransform::scale(1.0f / s));
      sampleCloud.setBounds(juce::Rectangle<int>(
          area.getX(),
          area.getY(),
          juce::roundToInt(area.getWidth() * s),
          juce::roundToInt(area.getHeight() * s)));
    } else {
      sampleCloud.setTransform(juce::AffineTransform());
      sampleCloud.setBounds(area);
    }
  } else if (mode == ViewMode::Libraries) {
    librariesComponent.setVisible(true);
    librariesComponent.setBounds(area);
  } else if (mode == ViewMode::Record) {
    recorderComponent.setVisible(true);
    recorderComponent.setBounds(area);
  } else if (mode == ViewMode::Analysis) {
    analysisComponent.setVisible(true);
    analysisComponent.setBounds(area);
  } else if (mode == ViewMode::Edit) {
    editComponent.setVisible(true);
    editComponent.setBounds(area);
  } else if (mode == ViewMode::SampleMap) {
    sampleMapComponent.setVisible(true);
    sampleMapComponent.setBounds(area);
  } else if (mode == ViewMode::Performance) {
    performanceComponent.setVisible(true);
    performanceComponent.setBounds(area);
  } else // ViewMode::List
  {
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
      juce::File::getSpecialLocation(juce::File::userHomeDirectory),
      "*",
      true);

  chooser->launchAsync(
      juce::FileBrowserComponent::openMode |
          juce::FileBrowserComponent::canSelectDirectories |
          juce::FileBrowserComponent::canSelectFiles |
          juce::FileBrowserComponent::canSelectMultipleItems,
      [this, chooser](const juce::FileChooser &fc) {
        auto results = fc.getResults();
        if (results.isEmpty()) {
          auto single = fc.getResult();
          if (single != juce::File())
            results.add(single);
        }

        std::vector<juce::String> foldersToScan;

        for (const auto &fileOrDir : results) {
          juce::File dir = fileOrDir.isDirectory() ? fileOrDir : fileOrDir.getParentDirectory();
          if (dir.exists() && dir.isDirectory()) {
            juce::String folderPath = dir.getFullPathName();
#if JUCE_MAC
            if (access(folderPath.toRawUTF8(), R_OK | X_OK) != 0) {
              juce::String escapedPath = folderPath.replace("'", "'\\''");
              juce::String script = "osascript -e 'do shell script \"chmod -R a+rX \\\"" + escapedPath + "\\\"\" with administrator privileges'";
              std::system(script.toRawUTF8());
            }
#endif
            if (std::find(foldersToScan.begin(), foldersToScan.end(), folderPath) == foldersToScan.end()) {
              foldersToScan.push_back(folderPath);
              audioProcessor.getDatabaseManager().addScanFolder(folderPath);
            }
          }
        }

        if (!foldersToScan.empty()) {
          audioProcessor.getLibraryScanner().startScan(foldersToScan);
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

  juce::PopupMenu scaleSubMenu;
  float curScale = audioProcessor.getDatabaseManager().getUiScale();
  int curPercent = juce::roundToInt(curScale * 100.0f);

  scaleSubMenu.addItem(30, "Increase Size (+10%)    [Cmd/Ctrl +]", curScale < 2.0f);
  scaleSubMenu.addItem(31, "Decrease Size (-10%)    [Cmd/Ctrl -]", curScale > 0.70f);
  scaleSubMenu.addItem(32, "Reset Size (100%)       [Cmd/Ctrl 0]", std::abs(curScale - 1.0f) >= 0.02f);
  scaleSubMenu.addSeparator();

  scaleSubMenu.addItem(40, "80% (Compact)", true, curPercent == 80);
  scaleSubMenu.addItem(41, "90%", true, curPercent == 90);
  scaleSubMenu.addItem(42, "100% (Default)", true, curPercent == 100);
  scaleSubMenu.addItem(43, "110%", true, curPercent == 110);
  scaleSubMenu.addItem(44, "125% (Large)", true, curPercent == 125);
  scaleSubMenu.addItem(45, "150% (Extra Large)", true, curPercent == 150);
  scaleSubMenu.addItem(46, "175%", true, curPercent == 175);
  scaleSubMenu.addItem(47, "200% (Maximum)", true, curPercent == 200);

  menu.addSubMenu("Text & UI Scaling", scaleSubMenu);

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
    } else if (result == 30) // Increase (+10%)
    {
      applyUiScale(audioProcessor.getDatabaseManager().getUiScale() + 0.10f);
    } else if (result == 31) // Decrease (-10%)
    {
      applyUiScale(audioProcessor.getDatabaseManager().getUiScale() - 0.10f);
    } else if (result == 32) // Reset (100%)
    {
      applyUiScale(1.0f);
    } else if (result == 40) // 80%
    {
      applyUiScale(0.80f);
    } else if (result == 41) // 90%
    {
      applyUiScale(0.90f);
    } else if (result == 42) // 100%
    {
      applyUiScale(1.00f);
    } else if (result == 43) // 110%
    {
      applyUiScale(1.10f);
    } else if (result == 44) // 125%
    {
      applyUiScale(1.25f);
    } else if (result == 45) // 150%
    {
      applyUiScale(1.50f);
    } else if (result == 46) // 175%
    {
      applyUiScale(1.75f);
    } else if (result == 47) // 200%
    {
      applyUiScale(2.00f);
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
  analysisComponent.setItem(item);
}

void OpenWavAudioProcessorEditor::sampleDoubleClicked(
    const MediaItem & /*item*/) {}

void OpenWavAudioProcessorEditor::displayedItemsChanged(
    const std::vector<MediaItem> &items) {
  if (headerBar.getCurrentViewMode() == ViewMode::Cloud) {
    sampleCloud.setItems(items);
  }
}

void OpenWavAudioProcessorEditor::addToSampleMapRequested(const MediaItem &item) {
  audioProcessor.getAudioEngine().stop();
  sampleMapComponent.addSample(item);
  headerBar.setViewMode(ViewMode::SampleMap);
}

void OpenWavAudioProcessorEditor::autoSliceToSamplerRequested(const MediaItem &item) {
  audioProcessor.getAudioEngine().stop();
  sampleMapComponent.autoSliceToSampler(item);
  headerBar.setViewMode(ViewMode::SampleMap);
}

bool OpenWavAudioProcessorEditor::isInterestedInFileDrag(
    const juce::StringArray &files) {
  for (const auto &f : files) {
    juce::File file(f);
    if (file.isDirectory() ||
        file.getFileExtension().containsIgnoreCase("wav") ||
        file.getFileExtension().containsIgnoreCase("mp3") ||
        file.getFileExtension().containsIgnoreCase("flac") ||
        file.getFileExtension().containsIgnoreCase("ogg") ||
        file.getFileExtension().containsIgnoreCase("aif") ||
        file.getFileExtension().containsIgnoreCase("aiff") ||
        file.getFileExtension().containsIgnoreCase("aifc"))
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
#if JUCE_MAC
      if (access(file.getFullPathName().toRawUTF8(), R_OK | X_OK) != 0) {
        juce::String escapedPath = file.getFullPathName().replace("'", "'\\''");
        juce::String script = "osascript -e 'do shell script \"chmod -R a+rX \\\"" + escapedPath + "\\\"\" with administrator privileges'";
        std::system(script.toRawUTF8());
      }
#endif
      foldersToScan.push_back(file.getFullPathName());
      audioProcessor.getDatabaseManager().addScanFolder(file.getFullPathName());
    } else if (file.existsAsFile()) {
      // If parent directory not scanned, scan parent
      auto parent = file.getParentDirectory().getFullPathName();
#if JUCE_MAC
      if (access(parent.toRawUTF8(), R_OK | X_OK) != 0) {
        juce::String escapedPath = parent.replace("'", "'\\''");
        juce::String script = "osascript -e 'do shell script \"chmod -R a+rX \\\"" + escapedPath + "\\\"\" with administrator privileges'";
        std::system(script.toRawUTF8());
      }
#endif
      foldersToScan.push_back(parent);
      audioProcessor.getDatabaseManager().addScanFolder(parent);
    }
  }

  if (!foldersToScan.empty()) {
    audioProcessor.getLibraryScanner().startScan(foldersToScan);
  }
}

void OpenWavAudioProcessorEditor::viewModeChanged(ViewMode mode) {
  audioProcessor.getAudioEngine().stop();
  bool midiAllowed = (mode == ViewMode::Edit || mode == ViewMode::SampleMap || mode == ViewMode::Performance);
  audioProcessor.getAudioEngine().setMidiInputEnabled(midiAllowed);
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
      dw->setAlpha(1.0f);
      dw->setVisible(true);
#if JUCE_WINDOWS
      if (auto *peer = dw->getPeer()) {
        if (auto hwnd = (HWND)peer->getNativeHandle()) {
          activeEditorPtr = this;
          if (originalWndProc == nullptr) {
            originalWndProc = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)NativeCloseSubclassProc);
          }
        }
      }
#endif
      updateNativeTitleBarTheme();
    }
  }
}

void OpenWavAudioProcessorEditor::cloudSampleSelected(const MediaItem &item) {
  sampleTable.selectItemById(item.id);
  analysisComponent.setItem(item);
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

void OpenWavAudioProcessorEditor::applyUiScale(float scale) {
  float clampedScale = juce::jlimit(0.70f, 2.0f, scale);
  clampedScale = std::round(clampedScale * 100.0f) / 100.0f;
  audioProcessor.getDatabaseManager().setUiScale(clampedScale);
  setScaleFactor(clampedScale);
  resized();
  sampleCloud.refreshView();
  repaint();
}

bool OpenWavAudioProcessorEditor::keyPressed(const juce::KeyPress &key) {
  if (key.getModifiers().isCommandDown()) {
    if (key.getTextCharacter() == '+' || key.getTextCharacter() == '=' || key.getKeyCode() == juce::KeyPress::numberPadAdd) {
      applyUiScale(audioProcessor.getDatabaseManager().getUiScale() + 0.10f);
      return true;
    }
    if (key.getTextCharacter() == '-' || key.getTextCharacter() == '_' || key.getKeyCode() == juce::KeyPress::numberPadSubtract) {
      applyUiScale(audioProcessor.getDatabaseManager().getUiScale() - 0.10f);
      return true;
    }
    if (key.getTextCharacter() == '0' || key.getKeyCode() == '0' || key.getKeyCode() == juce::KeyPress::numberPad0) {
      applyUiScale(1.0f);
      return true;
    }
  }

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
