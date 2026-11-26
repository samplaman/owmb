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
openwav::OpenWavAudioProcessorEditor *activeEditorPtr = nullptr;

LRESULT CALLBACK NativeCloseSubclassProc(HWND hwnd, UINT msg, WPARAM wParam,
                                         LPARAM lParam) {
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
} // namespace
#endif

namespace openwav {

OpenWavAudioProcessorEditor::OpenWavAudioProcessorEditor(
    OpenWavAudioProcessor &p)
    : AudioProcessorEditor(&p), audioProcessor(p),
      headerBar(p.getDatabaseManager(), p.getLibraryScanner()),
      tagPanel(p.getDatabaseManager()), leftPanelResizer(*this),
      sampleTable(p.getDatabaseManager(), p.getAudioEngine()),
      folderBrowser(p.getDatabaseManager(), p.getLibraryScanner(), p.getAudioEngine()),
      sampleCloud(p.getDatabaseManager(), p.getAudioEngine()),
      librariesComponent(p.getDatabaseManager(), p.getLibraryScanner(),
                         p.getAudioEngine()),
      recorderComponent(p.getAudioEngine(), p.getDatabaseManager()),
      waveformTransport(p.getAudioEngine()),
      scanProgressDialog(p.getLibraryScanner()),
      mobileTransferDialog(p.getAudioEngine(), p.getDatabaseManager(),
                           p.getLibraryScanner()),
      editComponent(p.getAudioEngine()), sampleMapComponent(p.getAudioEngine()),
      performanceComponent(p.getAudioEngine()),
      cueSheetComponent(p.getAudioEngine()),
      commandPalette(p.getDatabaseManager()) {
  juce::String savedColourHex =
      audioProcessor.getDatabaseManager().getPrimaryColourHex();
  if (savedColourHex.isNotEmpty()) {
    juce::Colour c = juce::Colour::fromString(savedColourHex);
    if (c.getAlpha() > 0)
      OpenWavLookAndFeel::setPrimaryColour(c);
  }
  OpenWavLookAndFeel::setDarkTheme(true);
  lookAndFeel.updateColors();

  setLookAndFeel(&lookAndFeel);
  juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel);

  headerBar.addListener(this);
  tagPanel.addListener(this);
  sampleTable.addListener(this);
  folderBrowser.addListener(this);
  sampleCloud.addListener(this);

  addAndMakeVisible(headerBar);
  addAndMakeVisible(tagPanel);
  addAndMakeVisible(leftPanelResizer);
  addAndMakeVisible(sampleTable);
  addChildComponent(folderBrowser);
  addChildComponent(sampleCloud);
  addChildComponent(librariesComponent);
  addChildComponent(recorderComponent);
  addChildComponent(analysisComponent);
  addChildComponent(editComponent);
  addChildComponent(sampleMapComponent);
  addChildComponent(performanceComponent);
  addChildComponent(cueSheetComponent);
  addAndMakeVisible(waveformTransport);

  tooltipWindow = std::make_unique<juce::TooltipWindow>(this, 280);
  commandPalette.setStaticCommands({
      { "VIEW",   "List View",            "Browse samples in table",             "1", [this] { headerBar.setViewMode(ViewMode::List); } },
      { "VIEW",   "Folders View",         "Browse scanned folder hierarchy",     "2", [this] { headerBar.setViewMode(ViewMode::Folders); } },
      { "VIEW",   "3D Cloud View",        "Explore semantic 3D sample cloud",    "3", [this] { headerBar.setViewMode(ViewMode::Cloud); } },
      { "VIEW",   "Online Libraries",     "Browse and download cloud packs",     "4", [this] { headerBar.setViewMode(ViewMode::Libraries); } },
      { "VIEW",   "Recorder",             "Record live audio & punch-in takes",  "5", [this] { headerBar.setViewMode(ViewMode::Record); } },
      { "VIEW",   "Analysis",             "Inspect waveform metrics & metadata", "6", [this] { headerBar.setViewMode(ViewMode::Analysis); } },
      { "VIEW",   "Audio Editor",         "Trim, normalize & spectral edit",     "7", [this] { headerBar.setViewMode(ViewMode::Edit); } },
      { "VIEW",   "Sample Map",           "Build multi-zone instruments & kits", "8", [this] { headerBar.setViewMode(ViewMode::SampleMap); } },
      { "VIEW",   "Performance Rack",     "Chain real-time audio FX",            "9", [this] { headerBar.setViewMode(ViewMode::Performance); } },
      { "VIEW",   "Cue Sheet",            "Spreadsheet of all used samples & regions", "", [this] { headerBar.setViewMode(ViewMode::CueSheet); } },
      { "ACTION", "Add Sample Folder...", "Scan a new folder into library",      "",  [this] { addFolderRequested(); } },
      { "ACTION", "Rescan Library",       "Refresh all scanned sample folders",  "",  [this] { rescanRequested(); } },
      { "ACTION", "Play / Pause",         "Toggle transport playback",           "Space", [this] { waveformTransport.togglePlay(); } },
      { "ACTION", "Toggle Loop",          "Enable or disable sample looping",    "L", [this] { waveformTransport.toggleLoop(); } },
      { "ACTION", "Auto-Slice Sample",    "Open slice configuration",            "",  [this] { waveformTransport.triggerSlice(); } },
      { "ACTION", "Export Sample Map as .mid", "Export mapped zones as MIDI pattern", "", [this] { sampleMapComponent.exportZonesAsMidiPattern(); } },
      { "ACTION", "Stop MIDI Playback",   "Stop previewing any playing .mid file", "", [this] { audioProcessor.stopMidiFilePlayback(); } },
      { "FILTER", "Show Loops Only",      "Filter library to loop samples",       "",  [this] { sampleTypeFilterChanged(SampleTypeFilter::Loops); } },
      { "FILTER", "Show One-Shots Only",  "Filter library to one-shot samples",   "",  [this] { sampleTypeFilterChanged(SampleTypeFilter::OneShots); } },
      { "FILTER", "Show All Sample Types","Show both loops and one-shots",        "",  [this] { sampleTypeFilterChanged(SampleTypeFilter::All); } },
      { "ACTION", "Keyboard Shortcuts...", "View and customize shortcuts",       "",  [this] { shortcutsDialog.showDialog(); } },
      { "ACTION", "Reset UI Scale (100%)", "Restore default 1.0x scaling",       "Cmd+0", [this] { applyUiScale(1.0f); } },
  });
  commandPalette.onSampleChosen = [this](const MediaItem& item) {
    juce::File f(item.filePath);
    if (f.existsAsFile()) {
      if (f.hasFileExtension("mid;midi")) {
        sampleSelected(item);
      } else {
        audioProcessor.getAudioEngine().loadFile(f);
        sampleSelected(item);
        if (audioProcessor.getAudioEngine().getAutoPlay())
          audioProcessor.getAudioEngine().play();
      }
    }
  };

  performanceComponent.onMidiLearnRequested = [this](int effectIdx, const EffectParamInfo& info) {
    audioProcessor.startMidiLearn(effectIdx, info.id, info.minValue, info.maxValue);
    ToastOverlay::show("Move a MIDI CC knob/fader to bind " + info.name + "...");
  };
  performanceComponent.onClearMidiLearnRequested = [this](int effectIdx, const juce::String& paramId) {
    audioProcessor.clearMidiLearnForParam(effectIdx, paramId);
    ToastOverlay::show("Cleared MIDI CC binding for " + paramId);
  };
  performanceComponent.getLearnedCcCallback = [this](int effectIdx, const juce::String& paramId) {
    return audioProcessor.getLearnedCcForParam(effectIdx, paramId);
  };
  audioProcessor.onMidiLearned = [this](int cc, int, const juce::String& paramId) {
    ToastOverlay::show("Bound CC " + juce::String(cc) + " to " + paramId);
    performanceComponent.refreshRack();
  };

  waveformTransport.getMidiPreviewData = [this]() {
    WaveformTransportComponent::MidiPreviewState state;
    state.selected = audioProcessor.hasLoadedMidiFile();
    state.active = audioProcessor.isMidiFilePlaying();
    if (state.selected || state.active) {
      state.fileName = audioProcessor.getLoadedMidiFileName();
      state.positionSec = audioProcessor.getMidiPlaybackTimeSeconds();
      state.durationSec = audioProcessor.getMidiFileDurationSeconds();
      auto notes = audioProcessor.getMidiDisplayNotes();
      state.notes.reserve(notes.size());
      for (const auto& n : notes) {
        state.notes.push_back({ n.noteNumber, n.startSec, n.endSec, n.velocity });
      }
    }
    return state;
  };
  waveformTransport.onMidiTogglePlay = [this]() {
    audioProcessor.toggleMidiFilePlayback();
  };
  waveformTransport.onMidiStop = [this]() {
    audioProcessor.stopMidiFilePlayback();
  };
  waveformTransport.onMidiSeekRatio = [this](double ratio) {
    audioProcessor.seekMidiFilePlayback(ratio);
  };
  addChildComponent(commandPalette);
  addChildComponent(toastOverlay);

  std::function<void(juce::Component&)> applyButtonCursors = [&](juce::Component& c) {
    if (auto* b = dynamic_cast<juce::Button*>(&c))
      b->setMouseCursor(juce::MouseCursor::PointingHandCursor);
    for (int i = 0; i < c.getNumChildComponents(); ++i)
      if (auto* child = c.getChildComponent(i))
        applyButtonCursors(*child);
  };
  applyButtonCursors(*this);

  setResizable(true, true);
  if (juce::JUCEApplicationBase::isStandaloneApp()) {
    auto* mainDisplay = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
    auto userArea = (mainDisplay != nullptr) ? mainDisplay->userArea : juce::Rectangle<int>(0, 0, 1920, 1080);
    setResizeLimits(800, 650, std::max(7680, userArea.getWidth()), std::max(4320, userArea.getHeight()));
    setSize(std::max(800, userArea.getWidth()), std::max(650, userArea.getHeight()));
  } else {
    setResizeLimits(800, 650, 3840, 2160);
    setSize(1920, 1080);
  }
  setWantsKeyboardFocus(true);

  float savedScale = audioProcessor.getDatabaseManager().getUiScale();
  if (savedScale >= 0.70f && savedScale <= 2.0f &&
      std::abs(savedScale - 1.0f) > 0.001f) {
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

  folderBrowser.onAddFolderRequested = [this] {
    addFolderRequested();
  };

  folderBrowser.onFoldersChanged = [this] {
    tagPanel.refreshFolders();
    triggerFilterUpdate();
  };

  sampleMapComponent.onStateChanged = [this] {
    saveStateToProcessor();
  };

  sampleMapComponent.onOpenFxRackRequested = [this] {
    headerBar.setViewMode(ViewMode::Performance);
  };

  sampleMapComponent.onOpenAiKitBuilderRequested = [this] {
    auto dlg = std::make_unique<AiKitBuilderDialog>(
        audioProcessor.getDatabaseManager(),
        [this](const AiKitBuildResult& res) {
          sampleMapComponent.setState(res.sampleMap);

          auto& rack = audioProcessor.getAudioEngine().getPerformanceRack();
          rack.clearEffects();
          for (const auto& fxSpec : res.effects)
          {
            if (auto fx = rack.addEffect(fxSpec.type))
            {
              for (const auto& [pid, val] : fxSpec.params)
                fx->setParameter(pid, val);
            }
          }
          performanceComponent.refreshRack();
          saveStateToProcessor();
        });

    juce::DialogWindow::LaunchOptions opts;
    opts.dialogTitle = "AI Kit & Preset Builder";
    opts.dialogBackgroundColour = OpenWavLookAndFeel::bgDark;
    opts.content.setOwned(dlg.release());
    opts.content->setSize(640, 320);
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar = true;
    opts.resizable = false;
    opts.componentToCentreAround = this;
    opts.launchAsync();
  };

  recorderComponent.onRecordingSaved = [this](const MediaItem&) {
    tagPanel.refreshTags();
    triggerFilterUpdate();
  };

  recorderComponent.onSendToEditor = [this](const MediaItem& item) {
    tagPanel.refreshTags();
    triggerFilterUpdate();
    editSampleRequested(item);
  };

  recorderComponent.onSendToSampleMap = [this](const MediaItem& item) {
    tagPanel.refreshTags();
    triggerFilterUpdate();
    addToSampleMapRequested(item);
  };

  recorderComponent.onOpenAudioSettings = [this] {
    if (juce::JUCEApplicationBase::isStandaloneApp()) {
      if (auto* sfw = findParentComponentOfClass<juce::StandaloneFilterWindow>()) {
        if (auto* holder = sfw->getPluginHolder()) {
          holder->showAudioSettingsDialog();
          return;
        }
      }
    }
    juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::InfoIcon, "Audio / Input Settings",
        "In VST3 / AU plugin mode, Audio inputs are routed by your host DAW.",
        "OK");
  };

  waveformTransport.onSlicesGenerated = [this](const std::vector<double>& /*ratios*/) {
    // Slices displayed on main transport bar; auto-mapping to sampler occurs via right-click Auto-Slice
  };

  waveformTransport.onLorisResynthCompleted = [this](const std::vector<ResynthesizedZone>& zones) {
    sampleMapComponent.applyResynthesizedZones(zones);
    headerBar.setViewMode(ViewMode::SampleMap);
  };

  auto indexEditedFile = [this](const juce::File& file) -> MediaItem {
    MediaItem item;
    item.id = juce::String(file.getFullPathName().hashCode64());
    item.filePath = file.getFullPathName();
    item.fileName = file.getFileName();
    item.fileExtension = file.getFileExtension().toLowerCase();
    item.fileSizeBytes = file.getSize();
    item.dateAddedMs = juce::Time::currentTimeMillis();

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    if (std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(file)); reader != nullptr)
    {
      item.sampleRate = reader->sampleRate;
      item.numChannels = static_cast<int>(reader->numChannels);
      item.bitDepth = static_cast<int>(reader->bitsPerSample);
      if (reader->sampleRate > 0.0)
        item.durationSeconds = static_cast<double>(reader->lengthInSamples) / reader->sampleRate;
    }
    item.precomputeCachedStrings();
    audioProcessor.getDatabaseManager().addOrUpdateItem(item);
    tagPanel.refreshTags();
    triggerFilterUpdate();
    return item;
  };

  editComponent.onSavedToLibrary = [this, indexEditedFile](const juce::File& file) {
    MediaItem item = indexEditedFile(file);
    auto& eng = audioProcessor.getAudioEngine();
    const double total = eng.getTotalLengthSeconds();
    const double startR = eng.getSampleStartRatio();
    const double endR = eng.getSampleEndRatio();
    const bool isPartial = (startR > 0.001 || endR < 0.999);
    cueSheetComponent.logUsage(item.filePath, 0.0, item.durationSeconds, item.durationSeconds,
                               isPartial, "Edit Copy",
                               isPartial ? ("Trimmed from " + juce::String(startR * total, 2) + "s - " + juce::String(endR * total, 2) + "s") : "");
    ToastOverlay::show("Saved edited copy to library: " + file.getFileName());
  };

  editComponent.onSendToSampleMap = [this, indexEditedFile](const juce::File& file) {
    MediaItem item = indexEditedFile(file);
    addToSampleMapRequested(item);
    ToastOverlay::show("Sent to Sample Map: " + file.getFileName());
  };

  cueSheetComponent.onSyncSampleMapRequested = [this] {
    cueSheetComponent.syncFromSampleMap(sampleMapComponent.getState());
  };

  cueSheetComponent.onAuditionCue = [this](const juce::String& filePath, double inSec, double outSec, bool isPartial) {
    juce::File f(filePath);
    if (!f.existsAsFile()) {
      ToastOverlay::show("File not found on disk: " + f.getFileName(), true);
      return;
    }
    auto& eng = audioProcessor.getAudioEngine();
    eng.loadFile(f, false, false);
    const double total = eng.getTotalLengthSeconds();
    if (isPartial && total > 0.0) {
      eng.setSampleRange(juce::jlimit(0.0, 1.0, inSec / total),
                         juce::jlimit(0.0, 1.0, outSec / total));
      eng.setPositionRatio(juce::jlimit(0.0, 1.0, inSec / total));
    }
    eng.play();
  };

  // Broadcast current LookAndFeel theme and colours to all sub-components
  sendLookAndFeelChange();

  triggerFilterUpdate();

  // Restore persisted state for Edit, SampleMap, and layout
  restoreStateFromProcessor();

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

  if (juce::JUCEApplicationBase::isStandaloneApp()) {
    autoEnableNewMidiInputs();
    midiDevicePollTimer.startTimer(1000);
  }

  juce::Component::SafePointer<OpenWavAudioProcessorEditor> safeThis(this);
  juce::MessageManager::callAsync([safeThis] {
    if (safeThis != nullptr)
      safeThis->grabKeyboardFocus();
  });
}

OpenWavAudioProcessorEditor::~OpenWavAudioProcessorEditor() {
  midiDevicePollTimer.stopTimer();
  saveStateToProcessor();
#if JUCE_WINDOWS
  activeEditorPtr = nullptr;
#endif
  audioProcessor.getLibraryScanner().cancelScan();
  audioProcessor.getAudioEngine().stop();
  headerBar.removeListener(this);
  tagPanel.removeListener(this);
  sampleTable.removeListener(this);
  folderBrowser.removeListener(this);
  sampleCloud.removeListener(this);
  tooltipWindow.reset();
  if (auto *dw = findParentComponentOfClass<juce::DocumentWindow>()) {
    dw->setLookAndFeel(nullptr);
  }
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
  int maxW = std::max(minW, std::min(600, getWidth() - 300));
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
    g.setFont(OpenWavLookAndFeel::getTitleFont(22.0f));
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
    folderBrowser.setVisible(false);
    sampleCloud.setVisible(false);
    librariesComponent.setVisible(false);
    recorderComponent.setVisible(false);
    analysisComponent.setVisible(false);
    editComponent.setVisible(false);
    sampleMapComponent.setVisible(false);
    waveformTransport.setVisible(false);
    return;
  }

  auto area = getLocalBounds();

  headerBar.setBounds(area.removeFromTop(54));
  int transportH = juce::jlimit(200, 300, static_cast<int>(area.getHeight() * 0.28f));
  waveformTransport.setBounds(area.removeFromBottom(transportH));

  // Left Split View with Resizer Bar
  int currentTagWidth =
      juce::jlimit(160, std::max(160, area.getWidth() - 300), tagPanelWidth);
  tagPanel.setBounds(area.removeFromLeft(currentTagWidth));
  leftPanelResizer.setBounds(area.removeFromLeft(6));

  auto mode = headerBar.getCurrentViewMode();

  sampleTable.setVisible(false);
  folderBrowser.setVisible(false);
  sampleCloud.setVisible(false);
  librariesComponent.setVisible(false);
  recorderComponent.setVisible(false);
  analysisComponent.setVisible(false);
  editComponent.setVisible(false);
  sampleMapComponent.setVisible(false);
  performanceComponent.setVisible(false);
  cueSheetComponent.setVisible(false);

  if (mode == ViewMode::Cloud) {
    sampleCloud.setVisible(true);
    sampleCloud.setTransform(juce::AffineTransform());
    sampleCloud.setBounds(area);
  } else if (mode == ViewMode::Folders) {
    folderBrowser.setVisible(true);
    folderBrowser.setBounds(area);
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
  } else if (mode == ViewMode::CueSheet) {
    cueSheetComponent.setVisible(true);
    cueSheetComponent.setBounds(area);
  } else // ViewMode::List
  {
    sampleTable.setVisible(true);
    sampleTable.setBounds(area);
  }

  waveformTransport.toFront(false);

  commandPalette.setBounds(getLocalBounds());
  if (commandPalette.isVisible())
    commandPalette.toFront(false);
  toastOverlay.setBounds(getLocalBounds());
  toastOverlay.toFront(false);
}

void OpenWavAudioProcessorEditor::searchTextChanged(
    const juce::String & /*newText*/) {
  startTimer(200); // 200 ms debounce
}

void OpenWavAudioProcessorEditor::searchBarUpPressed() {
  if (headerBar.getCurrentViewMode() == ViewMode::Folders)
    folderBrowser.moveSelection(-1);
  else
    sampleTable.moveSelection(-1);
}

void OpenWavAudioProcessorEditor::searchBarDownPressed() {
  if (headerBar.getCurrentViewMode() == ViewMode::Folders)
    folderBrowser.moveSelection(1);
  else
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

void OpenWavAudioProcessorEditor::sampleTypeFilterChanged(
    SampleTypeFilter filter) {
  headerBar.setSelectedSampleType(filter, false);
  tagPanel.setSampleTypeFilter(filter, false);
  triggerFilterUpdate();
}

void OpenWavAudioProcessorEditor::addFolderRequested() {
  auto chooser = std::make_shared<juce::FileChooser>(
      "Select Audio Folder to Scan...",
      juce::File::getSpecialLocation(juce::File::userHomeDirectory), "*", true);

  juce::Component::SafePointer<OpenWavAudioProcessorEditor> safeThis(this);
  chooser->launchAsync(
      juce::FileBrowserComponent::openMode |
          juce::FileBrowserComponent::canSelectDirectories |
          juce::FileBrowserComponent::canSelectMultipleItems,
      [safeThis, chooser](const juce::FileChooser &fc) {
        if (safeThis == nullptr)
          return;
        auto results = fc.getResults();
        if (results.isEmpty()) {
          auto single = fc.getResult();
          if (single != juce::File())
            results.add(single);
        }

        std::vector<juce::String> foldersToScan;

        for (const auto &fileOrDir : results) {
          juce::File dir = fileOrDir.isDirectory()
                                ? fileOrDir
                                : fileOrDir.getParentDirectory();
          if (dir.exists() && dir.isDirectory()) {
            juce::String folderPath = dir.getFullPathName();
            if (std::find(foldersToScan.begin(), foldersToScan.end(),
                          folderPath) == foldersToScan.end()) {
              foldersToScan.push_back(folderPath);
              safeThis->audioProcessor.getDatabaseManager().addScanFolder(folderPath);
            }
          }
        }

        safeThis->tagPanel.refreshFolders();
        if (safeThis->headerBar.getCurrentViewMode() == ViewMode::Folders) {
          safeThis->folderBrowser.setFilteredItems(
              safeThis->sampleTable.getDisplayedItems(),
              safeThis->headerBar.getSearchText(),
              safeThis->tagPanel.getFavoritesOnly());
        }

        if (!foldersToScan.empty()) {
          safeThis->scanProgressDialog.setSilent(false);
          safeThis->audioProcessor.getLibraryScanner().startScan(foldersToScan);
        }
      });
}

void OpenWavAudioProcessorEditor::rescanRequested() {
  auto folders = audioProcessor.getDatabaseManager().getScanFolders();
  if (!folders.empty()) {
    scanProgressDialog.setSilent(false);
    audioProcessor.getLibraryScanner().startScan(folders);
  } else {
    addFolderRequested();
  }
}

void OpenWavAudioProcessorEditor::settingsRequested() {
  juce::PopupMenu menu;

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

  scaleSubMenu.addItem(30, "Increase Size (+10%)    [Cmd/Ctrl +]",
                       curScale < 2.0f);
  scaleSubMenu.addItem(31, "Decrease Size (-10%)    [Cmd/Ctrl -]",
                       curScale > 0.70f);
  scaleSubMenu.addItem(32, "Reset Size (100%)       [Cmd/Ctrl 0]",
                       std::abs(curScale - 1.0f) >= 0.02f);
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
  menu.addItem(22, "Quick Command Palette... (Cmd/Ctrl+K)");
  menu.addItem(21, "Keyboard Shortcuts...");
  menu.addSeparator();
  menu.addItem(20, "Phone Field Recorder Sync (LAN)...");
  menu.addSeparator();
  menu.addItem(3, "Audio / MIDI Device Settings...");

  const auto midiInputs = juce::MidiInput::getAvailableDevices();
  const auto midiOutputs = juce::MidiOutput::getAvailableDevices();
  if (juce::JUCEApplicationBase::isStandaloneApp()) {
    if (auto *sfw = findParentComponentOfClass<juce::StandaloneFilterWindow>()) {
      if (auto *holder = sfw->getPluginHolder()) {
        juce::PopupMenu midiInMenu;
        if (midiInputs.isEmpty()) {
          midiInMenu.addItem(0, "(No MIDI inputs found)", false);
        } else {
          for (int i = 0; i < midiInputs.size(); ++i) {
            bool enabled = holder->deviceManager.isMidiInputDeviceEnabled(midiInputs[i].identifier);
            midiInMenu.addItem(1000 + i, midiInputs[i].name, true, enabled);
          }
        }
        menu.addSubMenu("MIDI Input Devices", midiInMenu);

        juce::PopupMenu midiOutMenu;
        const auto currentOutId = audioProcessor.getMidiOutputDeviceId();
        midiOutMenu.addItem(2000, "None", true, currentOutId.isEmpty());
        for (int i = 0; i < midiOutputs.size(); ++i) {
          midiOutMenu.addItem(2001 + i, midiOutputs[i].name, true, currentOutId == midiOutputs[i].identifier);
        }
        menu.addSubMenu("MIDI Output Device", midiOutMenu);
      }
    }
  }

  menu.addSeparator();
  menu.addItem(4, "Reset All Library Data...");
  menu.addSeparator();
  menu.addItem(5, "About OWMB...");

  menu.showMenuAsync(
      juce::PopupMenu::Options()
          .withTargetComponent(&headerBar.getSettingsButton())
          .withPreferredPopupDirection(juce::PopupMenu::Options::PopupDirection::downwards)
          .withStandardItemHeight(28),
      [this, midiInputs, midiOutputs](int result) {
    auto applyColour = [this](juce::Colour c, const juce::String &hexStr) {
      OpenWavLookAndFeel::setPrimaryColour(c);
      audioProcessor.getDatabaseManager().setPrimaryColourHex(hexStr);
      lookAndFeel.updateColors();
      sendLookAndFeelChange();
      repaint();
    };

    if (result == 22) {
      commandPalette.open();
      return;
    }
    if (result == 10) // Cyan
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
    } else if (result >= 1000 && result < 2000) {
      int idx = result - 1000;
      if (idx >= 0 && idx < midiInputs.size()) {
        if (auto *sfw = findParentComponentOfClass<juce::StandaloneFilterWindow>()) {
          if (auto *holder = sfw->getPluginHolder()) {
            const auto &dev = midiInputs[idx];
            bool wasEnabled = holder->deviceManager.isMidiInputDeviceEnabled(dev.identifier);
            holder->deviceManager.setMidiInputDeviceEnabled(dev.identifier, !wasEnabled);
            ToastOverlay::show(juce::String(!wasEnabled ? "Enabled MIDI In: " : "Disabled MIDI In: ") + dev.name);
          }
        }
      }
    } else if (result == 2000) {
      audioProcessor.setMidiOutputDevice({});
      ToastOverlay::show("MIDI Output disabled");
    } else if (result >= 2001 && result < 3000) {
      int idx = result - 2001;
      if (idx >= 0 && idx < midiOutputs.size()) {
        audioProcessor.setMidiOutputDevice(midiOutputs[idx].identifier);
        ToastOverlay::show("MIDI Output: " + midiOutputs[idx].name);
      }
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
    } else if (result == 21) // Keyboard Shortcuts...
    {
      shortcutsDialog.showDialog();
    } else if (result == 20) // Phone Field Recorder Sync (LAN)
    {
      mobileTransferDialog.showDialog();
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

void OpenWavAudioProcessorEditor::sampleTypeQuickFilterChanged(
    SampleTypeFilter typeFilter) {
  headerBar.setSelectedSampleType(typeFilter, false);
  triggerFilterUpdate();
}

void OpenWavAudioProcessorEditor::scannedFolderClicked(
    const juce::String &folderPath) {
  headerBar.setViewMode(ViewMode::Folders);
  folderBrowser.revealAndExpandRootFolder(folderPath);
}

void OpenWavAudioProcessorEditor::sampleSelected(const MediaItem &item) {
  sampleTable.selectItemById(item.id, false);
  folderBrowser.selectItemById(item.id, false);
  sampleCloud.selectItemById(item.id);
  analysisComponent.setItem(item);

  juce::File f(item.filePath);
  if (f.existsAsFile() && f.hasFileExtension("mid;midi")) {
    if (!audioProcessor.isMidiFilePlaying() || audioProcessor.getLoadedMidiFilePath() != f.getFullPathName()) {
      audioProcessor.loadAndPlayMidiFile(f);
    }
    waveformTransport.syncMidiPreviewNow();
    ToastOverlay::show("Playing MIDI: " + f.getFileName());
  } else {
    audioProcessor.clearLoadedMidiFile();
    waveformTransport.syncMidiPreviewNow();
  }
}

void OpenWavAudioProcessorEditor::sampleDoubleClicked(
    const MediaItem & /*item*/) {}

void OpenWavAudioProcessorEditor::displayedItemsChanged(
    const std::vector<MediaItem> &items) {
  if (headerBar.getCurrentViewMode() == ViewMode::Cloud) {
    sampleCloud.setItems(items);
  } else if (headerBar.getCurrentViewMode() == ViewMode::Folders) {
    folderBrowser.setFilteredItems(items, headerBar.getSearchText(),
                                   tagPanel.getFavoritesOnly());
  }
}

void OpenWavAudioProcessorEditor::addToSampleMapRequested(
    const MediaItem &item) {
  audioProcessor.getAudioEngine().stop();
  sampleMapComponent.addSample(item);
  cueSheetComponent.logUsage(item.filePath, 0.0, item.durationSeconds,
                             item.durationSeconds, false, "Sample Map");
  headerBar.setViewMode(ViewMode::SampleMap);
}

void OpenWavAudioProcessorEditor::autoSliceToSamplerRequested(
    const MediaItem &item) {
  audioProcessor.getAudioEngine().stop();
  juce::File sampleFile(item.filePath);
  if (!sampleFile.existsAsFile())
    return;

  // Load the sample into the audio engine for slicing preview
  audioProcessor.getAudioEngine().loadFile(sampleFile, false, true);

  auto sliceComp = std::make_unique<SliceConfigComponent>(
      audioProcessor.getAudioEngine(),
      [this, item](const std::vector<double>& ratios) {
        if (!ratios.empty()) {
          waveformTransport.setSliceRatios(ratios);
          sampleMapComponent.autoSliceToSampler(item, ratios);
          for (size_t i = 0; i < ratios.size(); ++i)
          {
            const double startR = ratios[i];
            const double endR = (i + 1 < ratios.size()) ? ratios[i + 1] : 1.0;
            cueSheetComponent.logUsage(item.filePath,
                                       startR * item.durationSeconds,
                                       endR * item.durationSeconds,
                                       item.durationSeconds,
                                       true,
                                       "Auto-Slice",
                                       "Slice " + juce::String(i + 1));
          }
          headerBar.setViewMode(ViewMode::SampleMap);
        }
      });

  juce::DialogWindow::LaunchOptions opts;
  opts.dialogTitle = "Slice to Sample Map";
  opts.dialogBackgroundColour = OpenWavLookAndFeel::bgDark;
  opts.content.setOwned(sliceComp.release());
  opts.content->setSize(620, 380);
  opts.escapeKeyTriggersCloseButton = true;
  opts.useNativeTitleBar = true;
  opts.resizable = false;
  opts.componentToCentreAround = this;

  opts.launchAsync();
}

void OpenWavAudioProcessorEditor::editSampleRequested(
    const MediaItem &item) {
  audioProcessor.getAudioEngine().loadFile(juce::File(item.filePath), false, true);
  audioProcessor.getAudioEngine().snapshotOriginalForEditing();
  editComponent.sampleLoaded(item.filePath);
  headerBar.setViewMode(ViewMode::Edit);
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
        file.getFileExtension().containsIgnoreCase("aifc") ||
        file.getFileExtension().containsIgnoreCase("m4a") ||
        file.getFileExtension().containsIgnoreCase("caf") ||
        file.getFileExtension().containsIgnoreCase("aac") ||
        file.getFileExtension().containsIgnoreCase("wma") ||
        file.getFileExtension().containsIgnoreCase("mid") ||
        file.getFileExtension().containsIgnoreCase("midi"))
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

  tagPanel.refreshFolders();
  if (headerBar.getCurrentViewMode() == ViewMode::Folders) {
    folderBrowser.setFilteredItems(sampleTable.getDisplayedItems(),
                                   headerBar.getSearchText(),
                                   tagPanel.getFavoritesOnly());
  }

  if (!foldersToScan.empty()) {
    scanProgressDialog.setSilent(false);
    audioProcessor.getLibraryScanner().startScan(foldersToScan);
  }
}

void OpenWavAudioProcessorEditor::viewModeChanged(ViewMode mode) {
  resized();
  if (mode == ViewMode::Cloud || mode == ViewMode::List || mode == ViewMode::Folders) {
    triggerFilterUpdate();
  }
  if (mode == ViewMode::Edit) {
    auto currentFile = audioProcessor.getAudioEngine().getCurrentFile();
    if (currentFile.existsAsFile() && !currentFile.hasFileExtension("mid;midi")) {
      editComponent.sampleLoaded(currentFile.getFullPathName());
    } else {
      auto lastId = audioProcessor.getAudioEngine().getLastPlayedSampleId();
      if (lastId.isNotEmpty())
        editComponent.sampleLoaded(lastId);
    }
    // Snapshot the original buffer when entering edit view so edits are non-destructive
    if (!audioProcessor.getAudioEngine().hasOriginalSnapshot())
      audioProcessor.getAudioEngine().snapshotOriginalForEditing();
  }
  if (mode == ViewMode::Performance) {
    performanceComponent.refreshRack();
  }
}

void OpenWavAudioProcessorEditor::parentHierarchyChanged() {
  juce::AudioProcessorEditor::parentHierarchyChanged();

  if (juce::JUCEApplicationBase::isStandaloneApp()) {
    autoEnableNewMidiInputs();
    ensureStandaloneAudioInputEnabled();
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

      if (!initialStandaloneSized) {
        initialStandaloneSized = true;
        auto& displays = juce::Desktop::getInstance().getDisplays();
        const auto* targetDisplay = displays.getDisplayForRect(dw->getScreenBounds(), true);
        if (targetDisplay == nullptr)
          targetDisplay = displays.getPrimaryDisplay();
        auto userArea = (targetDisplay != nullptr)
                            ? targetDisplay->userArea
                            : juce::Rectangle<int>(0, 0, 1920, 1080);

        dw->setResizeLimits(800, 650, std::max(7680, userArea.getWidth()), std::max(4320, userArea.getHeight()));
        auto border = dw->getBorderThickness();
        int targetW = std::max(800, userArea.getWidth() - border.getLeftAndRight());
        int targetH = std::max(650, userArea.getHeight() - border.getTopAndBottom());
        dw->setContentComponentSize(targetW, targetH);
        dw->setTopLeftPosition(userArea.getPosition());
        dw->resized();
        resized();
      }
      dw->setAlpha(1.0f);
      dw->setVisible(true);
#if JUCE_WINDOWS
      if (auto *peer = dw->getPeer()) {
        if (auto hwnd = (HWND)peer->getNativeHandle()) {
          activeEditorPtr = this;
          if (originalWndProc == nullptr) {
            originalWndProc = (WNDPROC)SetWindowLongPtr(
                hwnd, GWLP_WNDPROC, (LONG_PTR)NativeCloseSubclassProc);
          }
        }
      }
#endif
      updateNativeTitleBarTheme();
    }
  }

  if (isVisible()) {
    grabKeyboardFocus();
  }
}

void OpenWavAudioProcessorEditor::visibilityChanged() {
  if (isVisible()) {
    grabKeyboardFocus();
  }
}

void OpenWavAudioProcessorEditor::mouseDown(const juce::MouseEvent &e) {
  grabKeyboardFocus();
  juce::AudioProcessorEditor::mouseDown(e);
}

void OpenWavAudioProcessorEditor::cloudSampleSelected(const MediaItem &item) {
  sampleTable.selectItemById(item.id, false);
  folderBrowser.selectItemById(item.id, false);
  analysisComponent.setItem(item);
  juce::File f(item.filePath);
  if (!f.hasFileExtension("mid;midi")) {
    audioProcessor.clearLoadedMidiFile();
  }
  waveformTransport.syncMidiPreviewNow();
}

void OpenWavAudioProcessorEditor::cloudSampleDoubleClicked(
    const MediaItem &item) {
  sampleTable.selectItemById(item.id, false);
  analysisComponent.setItem(item);
}

void OpenWavAudioProcessorEditor::triggerFilterUpdate() {
  sampleTable.updateFilter(
      headerBar.getSearchText(), tagPanel.getSelectedTags(),
      tagPanel.getMatchAllTags(), headerBar.getSelectedFormat(),
      tagPanel.getFavoritesOnly(), headerBar.getSelectedSampleType());

  if (headerBar.getCurrentViewMode() == ViewMode::Cloud) {
    sampleCloud.setItems(sampleTable.getDisplayedItems());
  } else if (headerBar.getCurrentViewMode() == ViewMode::Folders) {
    folderBrowser.setFilteredItems(sampleTable.getDisplayedItems(),
                                   headerBar.getSearchText(),
                                   tagPanel.getFavoritesOnly());
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
  if (key.getModifiers().isCommandDown() &&
      (key.getKeyCode() == 'K' || key.getKeyCode() == 'k')) {
    if (commandPalette.isVisible())
      commandPalette.close();
    else
      commandPalette.open();
    return true;
  }

  auto& sm = ShortcutManager::getInstance();

  if (sm.matches("ui.zoom_in", key)) {
    applyUiScale(audioProcessor.getDatabaseManager().getUiScale() + 0.10f);
    return true;
  }
  if (sm.matches("ui.zoom_out", key)) {
    applyUiScale(audioProcessor.getDatabaseManager().getUiScale() - 0.10f);
    return true;
  }
  if (sm.matches("ui.zoom_reset", key)) {
    applyUiScale(1.0f);
    return true;
  }

  if (sm.matches("transport.loop", key)) {
    waveformTransport.toggleLoop();
    return true;
  }
  if (sm.matches("transport.slice", key)) {
    waveformTransport.triggerSlice();
    return true;
  }
  if (sm.matches("transport.play_pause", key)) {
    waveformTransport.togglePlay();
    return true;
  }

  if (sm.matches("view.list", key)) {
    headerBar.setViewMode(ViewMode::List);
    return true;
  }
  if (sm.matches("view.galaxy", key)) {
    headerBar.setViewMode(ViewMode::Cloud);
    return true;
  }
  if (sm.matches("view.edit", key)) {
    headerBar.setViewMode(ViewMode::Edit);
    return true;
  }
  if (sm.matches("view.sample_map", key)) {
    headerBar.setViewMode(ViewMode::SampleMap);
    return true;
  }
  if (sm.matches("view.performance", key)) {
    headerBar.setViewMode(ViewMode::Performance);
    return true;
  }

  return juce::AudioProcessorEditor::keyPressed(key);
}

void OpenWavAudioProcessorEditor::ensureStandaloneAudioInputEnabled() {
  if (!juce::JUCEApplicationBase::isStandaloneApp() || standaloneAudioInputChecked)
    return;

  auto *sfw = findParentComponentOfClass<juce::StandaloneFilterWindow>();
  if (sfw == nullptr)
    return;

  auto *holder = sfw->getPluginHolder();
  if (holder == nullptr)
    return;

  standaloneAudioInputChecked = true;
  auto setup = holder->deviceManager.getAudioDeviceSetup();
  if (setup.inputDeviceName.isEmpty() || (!setup.useDefaultInputChannels && setup.inputChannels.isZero())) {
    if (auto *devType = holder->deviceManager.getCurrentDeviceTypeObject()) {
      auto inputNames = devType->getDeviceNames(true);
      int defaultIdx = devType->getDefaultDeviceIndex(true);
      if (setup.inputDeviceName.isEmpty() && defaultIdx >= 0 && defaultIdx < inputNames.size())
        setup.inputDeviceName = inputNames[defaultIdx];
    }
    setup.useDefaultInputChannels = true;
    holder->deviceManager.setAudioDeviceSetup(setup, true);
  }
}

void OpenWavAudioProcessorEditor::autoEnableNewMidiInputs() {
  if (!juce::JUCEApplicationBase::isStandaloneApp())
    return;

  auto *sfw = findParentComponentOfClass<juce::StandaloneFilterWindow>();
  if (sfw == nullptr)
    return;

  auto *holder = sfw->getPluginHolder();
  if (holder == nullptr)
    return;

  const auto availableDevices = juce::MidiInput::getAvailableDevices();
  std::set<juce::String> currentIds;
  for (const auto &dev : availableDevices) {
    currentIds.insert(dev.identifier);
    const bool isDawControlPort = dev.name.containsIgnoreCase("DAW");
    if (knownMidiInputIds.find(dev.identifier) == knownMidiInputIds.end()) {
      knownMidiInputIds.insert(dev.identifier);
      if (isDawControlPort) {
        if (holder->deviceManager.isMidiInputDeviceEnabled(dev.identifier)) {
          holder->deviceManager.setMidiInputDeviceEnabled(dev.identifier, false);
        }
      } else if (!holder->deviceManager.isMidiInputDeviceEnabled(dev.identifier)) {
        holder->deviceManager.setMidiInputDeviceEnabled(dev.identifier, true);
      }
    }
  }

  // Prune disconnected devices so re-plugging them re-enables automatically
  for (auto it = knownMidiInputIds.begin(); it != knownMidiInputIds.end();) {
    if (currentIds.find(*it) == currentIds.end())
      it = knownMidiInputIds.erase(it);
    else
      ++it;
  }
}

void OpenWavAudioProcessorEditor::saveStateToProcessor() {
  PluginFullState s;
  s.currentViewMode = static_cast<int>(headerBar.getCurrentViewMode());
  s.currentLoadedFilePath = audioProcessor.getAudioEngine().getCurrentFile().getFullPathName();
  s.transportSliceRatios = waveformTransport.getSliceRatios();
  s.searchText = headerBar.getSearchText();
  for (const auto& t : tagPanel.getSelectedTags())
    s.selectedTags.push_back(t);
  s.tagPanelWidth = tagPanelWidth;
  s.edit = editComponent.getState();
  s.sampleMap = sampleMapComponent.getState();
  s.performance = performanceComponent.getState();
  s.cueSheetEntries = cueSheetComponent.getState();
  for (const auto& [fp, es] : audioProcessor.getAudioEngine().getAllSampleEditSettings())
  {
    SampleEditSettingsState ss;
    ss.filePath = fp;
    ss.sampleStartRatio = es.sampleStartRatio;
    ss.sampleEndRatio = es.sampleEndRatio;
    ss.loopInRatio = es.loopInRatio;
    ss.loopOutRatio = es.loopOutRatio;
    ss.loopCrossfadeMs = es.loopCrossfadeMs;
    ss.fadeInMs = es.fadeInMs;
    ss.fadeOutMs = es.fadeOutMs;
    ss.fadeInCurveType = es.fadeInCurveType;
    ss.fadeOutCurveType = es.fadeOutCurveType;
    ss.loopMarkersSet = es.loopMarkersSet;
    ss.isLooping = es.isLooping;
    s.perSampleEditSettings.push_back(std::move(ss));
  }

  audioProcessor.setFullPluginState(s);
}

void OpenWavAudioProcessorEditor::restoreStateFromProcessor() {
  auto s = audioProcessor.getFullPluginState();

  if (!s.perSampleEditSettings.empty())
  {
    std::map<juce::String, SampleEditSettings> restoredSettings;
    for (const auto& ss : s.perSampleEditSettings)
    {
      if (ss.filePath.isEmpty()) continue;
      SampleEditSettings es;
      es.sampleStartRatio = ss.sampleStartRatio;
      es.sampleEndRatio = ss.sampleEndRatio;
      es.loopInRatio = ss.loopInRatio;
      es.loopOutRatio = ss.loopOutRatio;
      es.loopCrossfadeMs = ss.loopCrossfadeMs;
      es.fadeInMs = ss.fadeInMs;
      es.fadeOutMs = ss.fadeOutMs;
      es.fadeInCurveType = ss.fadeInCurveType;
      es.fadeOutCurveType = ss.fadeOutCurveType;
      es.loopMarkersSet = ss.loopMarkersSet;
      es.isLooping = ss.isLooping;
      restoredSettings[ss.filePath] = es;
    }
    audioProcessor.getAudioEngine().setAllSampleEditSettings(restoredSettings);
  }

  // 1. Restore Tag Panel width
  if (s.tagPanelWidth >= 120 && s.tagPanelWidth <= 500) {
    tagPanelWidth = s.tagPanelWidth;
  }

  // 2. Restore Search text and Tag filters
  if (s.searchText.isNotEmpty() || !s.selectedTags.empty()) {
    headerBar.setSearchText(s.searchText);
    tagPanel.setSelectedTags(s.selectedTags);
  }

  // 3. Restore Loaded Audio File in Transport
  if (s.currentLoadedFilePath.isNotEmpty()) {
    juce::File f(s.currentLoadedFilePath);
    if (f.existsAsFile()) {
      if (audioProcessor.getAudioEngine().getCurrentFile() != f || !audioProcessor.getAudioEngine().isCurrentSampleInSampler()) {
        audioProcessor.getAudioEngine().loadFile(f, false, true);
      }
    }
  }

  // 4. Restore Transport Slices
  if (!s.transportSliceRatios.empty()) {
    waveformTransport.setSliceRatios(s.transportSliceRatios);
  }

  // 5. Restore Sub-components state
  editComponent.setState(s.edit);
  sampleMapComponent.setState(s.sampleMap);
  performanceComponent.setState(s.performance);
  cueSheetComponent.setState(s.cueSheetEntries);

  // 6. Restore View Mode (never open directly in Cloud view)
  if (s.currentViewMode >= 0 && s.currentViewMode <= 9) {
    auto mode = static_cast<ViewMode>(s.currentViewMode);
    if (mode == ViewMode::Cloud)
      mode = ViewMode::List;
    headerBar.setViewMode(mode);
  } else {
    headerBar.setViewMode(ViewMode::List);
  }

  resized();
  repaint();
}

} // namespace openwav

