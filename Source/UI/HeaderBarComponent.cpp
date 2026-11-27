#include "HeaderBarComponent.h"
#include "OpenWavLookAndFeel.h"

namespace openwav
{

HeaderBarComponent::HeaderBarComponent(TagDatabaseManager& db, LibraryScanner& scanner)
    : dbManager(db), libraryScanner(scanner)
{
    libraryScanner.addListener(this);

    // Title & Logo
    addAndMakeVisible(logoComponent);
    titleLabel.setFont(juce::Font(18.0f).boldened());
    titleLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    titleLabel.setJustificationType(juce::Justification::centred);
    addChildComponent(titleLabel); // added but hidden if logo loads

    lookAndFeelChanged();

    // Search Editor
    searchEditor.setJustification(juce::Justification::centredLeft);
    searchEditor.setIndents(8, 0);
#if JUCE_MAC
    searchEditor.setTextToShowWhenEmpty("Search samples, tags, paths... (Cmd+F)", OpenWavLookAndFeel::textSecondary.withAlpha(0.65f));
#else
    searchEditor.setTextToShowWhenEmpty("Search samples, tags, paths... (Ctrl+F)", OpenWavLookAndFeel::textSecondary.withAlpha(0.65f));
#endif
    searchEditor.addListener(this);
    searchEditor.addKeyListener(this);
    addAndMakeVisible(searchEditor);

    clearSearchButton.setComponentID("iconOnlyBtn");
    clearSearchButton.setTooltip("Clear Search");
    clearSearchButton.onClick = [this] {
        searchEditor.clear();
        clearSearchButton.setVisible(false);
        listeners.call([](HeaderBarListener& l) { l.searchTextChanged({}); });
    };
    addChildComponent(clearSearchButton);

    // Add Folder Button
    addFolderButton.setComponentID("iconOnlyBtn");
    addFolderButton.setTooltip("Add Folder to Library");
    addFolderButton.onClick = [this] {
        listeners.call([](HeaderBarListener& l) { l.addFolderRequested(); });
    };
    addAndMakeVisible(addFolderButton);

    // Rescan Button
    rescanButton.setComponentID("iconOnlyBtn");
    rescanButton.setTooltip("Rescan Library Folders");
    rescanButton.onClick = [this] {
        listeners.call([](HeaderBarListener& l) { l.rescanRequested(); });
    };
    addAndMakeVisible(rescanButton);

    // Settings Button (Audio / MIDI Setup)
    settingsButton.setComponentID("iconOnlyBtn");
    settingsButton.setTooltip("Audio / MIDI & App Settings");
    settingsButton.onClick = [this] {
        listeners.call([](HeaderBarListener& l) { l.settingsRequested(); });
    };
    addAndMakeVisible(settingsButton);

    // Format Filter Dropdown
    formatFilterBox.addItem("All Formats", 1);
    formatFilterBox.addItem(".WAV", 2);
    formatFilterBox.addItem(".MP3", 3);
    formatFilterBox.addItem(".FLAC", 4);
    formatFilterBox.addItem(".OGG", 5);
    formatFilterBox.addItem(".AIFF", 6);
    formatFilterBox.addItem(".M4A / .CAF", 7);
    formatFilterBox.addItem(".MID", 8);
    formatFilterBox.setSelectedId(1, juce::dontSendNotification);
    formatFilterBox.setTooltip("Filter samples by audio or MIDI format");
    formatFilterBox.onChange = [this] {
        static const juce::String formats[] = { "All", ".wav", ".mp3", ".flac", ".ogg", ".aiff", ".m4a", ".mid" };
        int idx = formatFilterBox.getSelectedId() - 1;
        if (idx >= 0 && idx < 8)
            setFormatFilter(formats[idx]);
    };
    addAndMakeVisible(formatFilterBox);

    // Sample Type Filter Dropdown (All / Loops / One-Shots)
    typeFilterBox.addItem("All Types", 1);
    typeFilterBox.addItem("Loops", 2);
    typeFilterBox.addItem("One-Shots", 3);
    typeFilterBox.setSelectedId(1, juce::dontSendNotification);
    typeFilterBox.setTooltip("Filter samples by Loops or One-Shots");
    typeFilterBox.onChange = [this] {
        int id = typeFilterBox.getSelectedId();
        SampleTypeFilter f = SampleTypeFilter::All;
        if (id == 2) f = SampleTypeFilter::Loops;
        else if (id == 3) f = SampleTypeFilter::OneShots;
        activeSampleType = f;
        listeners.call([f](HeaderBarListener& l) { l.sampleTypeFilterChanged(f); });
    };
    addAndMakeVisible(typeFilterBox);

    // View Switcher (List vs Cloud vs Library vs Record vs Analysis vs Edit vs Sample Map vs Rack)
    auto setupViewBtn = [this](juce::TextButton& btn, const juce::String& viewName, const juce::String& tooltipText, ViewMode mode) {
        btn.setComponentID("headerViewBtn");
        btn.setName(viewName);
        btn.setButtonText(viewName);
        btn.setTooltip(tooltipText);
        btn.onClick = [this, mode] { setViewMode(mode); };
        addAndMakeVisible(btn);
    };

    setupViewBtn(btnListView, "List", "List View", ViewMode::List);
    setupViewBtn(btnFoldersView, "Folders", "Scanned Folders View", ViewMode::Folders);
    setupViewBtn(btnCloudView, "Cloud", "Cloud View", ViewMode::Cloud);
    setupViewBtn(btnLibrariesView, "Library", "Library View", ViewMode::Libraries);
    setupViewBtn(btnRecordView, "Record", "Record View", ViewMode::Record);
    setupViewBtn(btnAnalysisView, "Analysis", "Analysis View", ViewMode::Analysis);
    setupViewBtn(btnEditView, "Edit", "Edit View", ViewMode::Edit);
    setupViewBtn(btnSampleMapView, "Sample Map", "Sample Map View", ViewMode::SampleMap);
    setupViewBtn(btnSamplerRackView, "Sampler Rack", "Multi-SampleMap Sampler Rack (MIDI Channels & DAW Multi-Out)", ViewMode::SamplerRack);
    setupViewBtn(btnPerformanceView, "Rack", "FX Rack View", ViewMode::Performance);
    setupViewBtn(btnMixerView, "Mixer", "Sampler Multi-Out & Mixer (VST3 / AU / Standalone)", ViewMode::Mixer);
    setupViewBtn(btnCueSheetView, "Cue Sheet", "Cue Sheet / Used Samples", ViewMode::CueSheet);

    btnListView.setToggleState(true, juce::dontSendNotification);

    OpenWavLookAndFeel::groupButtons({ &addFolderButton, &rescanButton, &settingsButton });
    OpenWavLookAndFeel::groupButtons({ &btnListView, &btnFoldersView, &btnCloudView, &btnLibrariesView });
    OpenWavLookAndFeel::groupButtons({ &btnRecordView, &btnAnalysisView, &btnEditView, &btnSampleMapView });
}

HeaderBarComponent::~HeaderBarComponent()
{
    libraryScanner.removeListener(this);
    searchEditor.removeListener(this);
}

void HeaderBarComponent::paint(juce::Graphics& g)
{
    g.fillAll(OpenWavLookAndFeel::bgHeader);
    g.setColour(OpenWavLookAndFeel::borderColour);
    g.drawRect(getLocalBounds().removeFromBottom(1));

    if (!viewPillBounds.isEmpty())
    {
        auto pill = viewPillBounds.toFloat();
        g.setColour(OpenWavLookAndFeel::bgDark.withAlpha(0.75f));
        g.fillRoundedRectangle(pill, 8.0f);

        g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.8f));
        if (!viewDivider1.isEmpty())
            g.fillRect(viewDivider1);
        if (!viewDivider2.isEmpty())
            g.fillRect(viewDivider2);
    }
}

void HeaderBarComponent::resized()
{
    auto area = getLocalBounds().reduced(12, 11);
    int barHeight = 32;
    int innerBtnH = 28;
    int gap = 4;

    // 1. Far Right Actions: Settings, Rescan, Add Folder (28px buttons -> 32px outer group pill)
    auto utilArea = area.removeFromRight(3 * 32 + 2 * gap + 6).withHeight(barHeight);
    auto utilInner = utilArea.reduced(3, 2);
    settingsButton.setBounds(utilInner.removeFromRight(32).withHeight(innerBtnH));
    utilInner.removeFromRight(gap);
    rescanButton.setBounds(utilInner.removeFromRight(32).withHeight(innerBtnH));
    utilInner.removeFromRight(gap);
    addFolderButton.setBounds(utilInner.removeFromRight(32).withHeight(innerBtnH));

    area.removeFromRight(12);

    // 2. Far Left: Logo / Title
    if (logoComponent.isVisible())
        logoComponent.setBounds(area.removeFromLeft(135).withHeight(barHeight));
    else
        titleLabel.setBounds(area.removeFromLeft(85).withHeight(barHeight));
    area.removeFromLeft(16);

    // 3. View Switcher Segmented Pill on Right-of-Center (32px outer pill, 28px inner buttons)
    const int viewBtnWidth = 30;
    const int divGap = 8;
    const int totalViewWidth = 12 * viewBtnWidth + 9 * gap + 2 * divGap + 6;
    auto pillArea = area.removeFromRight(totalViewWidth).withHeight(barHeight);
    viewPillBounds = pillArea;
    auto vInner = pillArea.reduced(3, 2);

    btnListView.setBounds(vInner.removeFromLeft(viewBtnWidth).withHeight(innerBtnH));
    vInner.removeFromLeft(gap);
    btnFoldersView.setBounds(vInner.removeFromLeft(viewBtnWidth).withHeight(innerBtnH));
    vInner.removeFromLeft(gap);
    btnCloudView.setBounds(vInner.removeFromLeft(viewBtnWidth).withHeight(innerBtnH));
    vInner.removeFromLeft(gap);
    btnLibrariesView.setBounds(vInner.removeFromLeft(viewBtnWidth).withHeight(innerBtnH));

    viewDivider1 = juce::Rectangle<int>(vInner.getX() + 3, vInner.getY() + 4, 1, innerBtnH - 8);
    vInner.removeFromLeft(divGap);

    btnRecordView.setBounds(vInner.removeFromLeft(viewBtnWidth).withHeight(innerBtnH));
    vInner.removeFromLeft(gap);
    btnAnalysisView.setBounds(vInner.removeFromLeft(viewBtnWidth).withHeight(innerBtnH));
    vInner.removeFromLeft(gap);
    btnEditView.setBounds(vInner.removeFromLeft(viewBtnWidth).withHeight(innerBtnH));
    vInner.removeFromLeft(gap);
    btnSampleMapView.setBounds(vInner.removeFromLeft(viewBtnWidth).withHeight(innerBtnH));

    viewDivider2 = juce::Rectangle<int>(vInner.getX() + 3, vInner.getY() + 4, 1, innerBtnH - 8);
    vInner.removeFromLeft(divGap);

    btnSamplerRackView.setBounds(vInner.removeFromLeft(viewBtnWidth).withHeight(innerBtnH));
    vInner.removeFromLeft(gap);
    btnPerformanceView.setBounds(vInner.removeFromLeft(viewBtnWidth).withHeight(innerBtnH));
    vInner.removeFromLeft(gap);
    btnMixerView.setBounds(vInner.removeFromLeft(viewBtnWidth).withHeight(innerBtnH));
    vInner.removeFromLeft(gap);
    btnCueSheetView.setBounds(vInner.removeFromLeft(viewBtnWidth).withHeight(innerBtnH));

    area.removeFromRight(12);

    // 4. Format & Sample Type Filter Dropdowns + Dynamic Search Editor (never obstructs search editor on compact windows)
    if (area.getWidth() >= 220)
    {
        formatFilterBox.setVisible(true);
        formatFilterBox.setBounds(area.removeFromRight(96).withHeight(barHeight));
        area.removeFromRight(6);
    }
    else
    {
        formatFilterBox.setVisible(false);
    }

    if (area.getWidth() >= 250)
    {
        typeFilterBox.setVisible(true);
        typeFilterBox.setBounds(area.removeFromRight(96).withHeight(barHeight));
        area.removeFromRight(8);
    }
    else
    {
        typeFilterBox.setVisible(false);
    }

    searchEditor.setBounds(area.withHeight(barHeight));
    clearSearchButton.setBounds(searchEditor.getRight() - 26, searchEditor.getY() + 4, 22, 24);
    clearSearchButton.toFront(false);
}

void HeaderBarComponent::textEditorTextChanged(juce::TextEditor& editor)
{
    if (&editor == &searchEditor)
    {
        clearSearchButton.setVisible(!searchEditor.isEmpty());
        listeners.call([txt = searchEditor.getText()](HeaderBarListener& l) {
            l.searchTextChanged(txt);
        });
    }
}

void HeaderBarComponent::setFormatFilter(const juce::String& ext)
{
    activeFormat = ext;
    listeners.call([ext](HeaderBarListener& l) {
        l.formatFilterChanged(ext);
    });
}

void HeaderBarComponent::setSelectedSampleType(SampleTypeFilter type, bool sendNotification)
{
    activeSampleType = type;
    int id = 1;
    if (type == SampleTypeFilter::Loops) id = 2;
    else if (type == SampleTypeFilter::OneShots) id = 3;
    typeFilterBox.setSelectedId(id, juce::dontSendNotification);
    if (sendNotification)
    {
        listeners.call([type](HeaderBarListener& l) {
            l.sampleTypeFilterChanged(type);
        });
    }
}

void HeaderBarComponent::scanStarted()
{
}

void HeaderBarComponent::scanProgress(int /*filesProcessed*/, int /*totalFiles*/, const juce::String& /*currentFile*/)
{
}

void HeaderBarComponent::scanFinished(int /*totalFilesDiscovered*/)
{
}

void HeaderBarComponent::setViewMode(ViewMode mode)
{
    currentViewMode = mode;
    btnListView.setToggleState(mode == ViewMode::List, juce::dontSendNotification);
    btnFoldersView.setToggleState(mode == ViewMode::Folders, juce::dontSendNotification);
    btnCloudView.setToggleState(mode == ViewMode::Cloud, juce::dontSendNotification);
    btnLibrariesView.setToggleState(mode == ViewMode::Libraries, juce::dontSendNotification);
    btnRecordView.setToggleState(mode == ViewMode::Record, juce::dontSendNotification);
    btnAnalysisView.setToggleState(mode == ViewMode::Analysis, juce::dontSendNotification);
    btnEditView.setToggleState(mode == ViewMode::Edit, juce::dontSendNotification);
    btnSampleMapView.setToggleState(mode == ViewMode::SampleMap, juce::dontSendNotification);
    btnSamplerRackView.setToggleState(mode == ViewMode::SamplerRack, juce::dontSendNotification);
    btnPerformanceView.setToggleState(mode == ViewMode::Performance, juce::dontSendNotification);
    btnMixerView.setToggleState(mode == ViewMode::Mixer, juce::dontSendNotification);
    btnCueSheetView.setToggleState(mode == ViewMode::CueSheet, juce::dontSendNotification);

    listeners.call([mode](HeaderBarListener& l) {
        l.viewModeChanged(mode);
    });
}

void HeaderBarComponent::addListener(HeaderBarListener* listener)
{
    listeners.add(listener);
}

void HeaderBarComponent::removeListener(HeaderBarListener* listener)
{
    listeners.remove(listener);
}

void HeaderBarComponent::lookAndFeelChanged()
{
    juce::Image logoImage;
#if defined(JUCE_BINARYDATA_H_INCLUDED) || __has_include(<JuceHeader.h>)
    logoImage = juce::ImageFileFormat::loadFrom(BinaryData::owmblogo_png, static_cast<size_t>(BinaryData::owmblogo_pngSize));
#endif

    if (logoImage.isNull())
    {
        juce::File logoFile = juce::File::getCurrentWorkingDirectory().getChildFile("owmblogo.png");
        if (!logoFile.existsAsFile())
            logoFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile("owmblogo.png");
        if (logoFile.existsAsFile())
            logoImage = juce::ImageFileFormat::loadFrom(logoFile);
    }

    if (!logoImage.isNull())
    {
        logoComponent.setVisible(true);
        titleLabel.setVisible(false);

        if (dbManager.isDarkMode())
        {
            // Invert colors of logo for dark theme (keeping alpha channel intact)
            juce::Image inverted = logoImage.createCopy();
            juce::Image::BitmapData bd(inverted, juce::Image::BitmapData::readWrite);
            for (int y = 0; y < bd.height; ++y)
            {
                for (int x = 0; x < bd.width; ++x)
                {
                    auto c = bd.getPixelColour(x, y);
                    bd.setPixelColour(x, y, juce::Colour(static_cast<juce::uint8>(255 - c.getRed()),
                                                        static_cast<juce::uint8>(255 - c.getGreen()),
                                                        static_cast<juce::uint8>(255 - c.getBlue()),
                                                        c.getAlpha()));
                }
            }
            logoComponent.setImage(inverted, juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yMid | juce::RectanglePlacement::onlyReduceInSize);
        }
        else
        {
            logoComponent.setImage(logoImage, juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yMid | juce::RectanglePlacement::onlyReduceInSize);
        }
    }
    else
    {
        logoComponent.setVisible(false);
        titleLabel.setVisible(true);
    }

    // Update labels and text fields
    titleLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    searchEditor.setTextToShowWhenEmpty("Search by name, tag, or path...", OpenWavLookAndFeel::textSecondary);
}

void HeaderBarComponent::setSearchText(const juce::String& text)
{
    searchEditor.setText(text, juce::dontSendNotification);
}

bool HeaderBarComponent::keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent)
{
    if (originatingComponent == &searchEditor)
    {
        if (key == juce::KeyPress::upKey)
        {
            listeners.call([](HeaderBarListener& l) { l.searchBarUpPressed(); });
            return true;
        }
        else if (key == juce::KeyPress::downKey)
        {
            listeners.call([](HeaderBarListener& l) { l.searchBarDownPressed(); });
            return true;
        }
    }
    return false;
}

} // namespace openwav
