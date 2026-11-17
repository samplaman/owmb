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
    formatFilterBox.setSelectedId(1, juce::dontSendNotification);
    formatFilterBox.setTooltip("Filter samples by audio format");
    formatFilterBox.onChange = [this] {
        static const juce::String formats[] = { "All", ".wav", ".mp3", ".flac", ".ogg", ".aiff" };
        int idx = formatFilterBox.getSelectedId() - 1;
        if (idx >= 0 && idx < 6)
            setFormatFilter(formats[idx]);
    };
    addAndMakeVisible(formatFilterBox);

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
    setupViewBtn(btnPerformanceView, "Rack", "Rack View", ViewMode::Performance);

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
        g.setColour(OpenWavLookAndFeel::borderColour);
        g.drawRoundedRectangle(pill.reduced(0.5f), 8.0f, 1.0f);

        g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.8f));
        if (!viewDivider1.isEmpty())
            g.fillRect(viewDivider1);
        if (!viewDivider2.isEmpty())
            g.fillRect(viewDivider2);
    }

    OpenWavLookAndFeel::drawButtonGroup(g, { &addFolderButton, &rescanButton, &settingsButton });
}

void HeaderBarComponent::resized()
{
    auto area = getLocalBounds().reduced(12, 10);
    int btnHeight = 32;
    int gap = 4;

    // 1. Far Right Actions: Settings, Rescan, Add Folder
    settingsButton.setBounds(area.removeFromRight(34).withHeight(btnHeight));
    area.removeFromRight(gap);
    rescanButton.setBounds(area.removeFromRight(34).withHeight(btnHeight));
    area.removeFromRight(gap);
    addFolderButton.setBounds(area.removeFromRight(34).withHeight(btnHeight));

    area.removeFromRight(12);

    // 2. Far Left: Logo / Title
    if (logoComponent.isVisible())
        logoComponent.setBounds(area.removeFromLeft(135));
    else
        titleLabel.setBounds(area.removeFromLeft(85));
    area.removeFromLeft(16);

    // 3. View Switcher Segmented Pill on Right-of-Center
    const int viewBtnWidth = 32;
    const int viewBtnH = 26;
    const int divGap = 9;
    const int totalViewWidth = 9 * viewBtnWidth + 6 * gap + 2 * divGap + 8; // 8px pill padding
    auto pillArea = area.removeFromRight(totalViewWidth).withHeight(btnHeight);
    viewPillBounds = pillArea;
    auto vInner = pillArea.reduced(4, 3);

    btnListView.setBounds(vInner.removeFromLeft(viewBtnWidth).withHeight(viewBtnH));
    vInner.removeFromLeft(gap);
    btnFoldersView.setBounds(vInner.removeFromLeft(viewBtnWidth).withHeight(viewBtnH));
    vInner.removeFromLeft(gap);
    btnCloudView.setBounds(vInner.removeFromLeft(viewBtnWidth).withHeight(viewBtnH));
    vInner.removeFromLeft(gap);
    btnLibrariesView.setBounds(vInner.removeFromLeft(viewBtnWidth).withHeight(viewBtnH));

    viewDivider1 = juce::Rectangle<int>(vInner.getX() + 4, vInner.getY() + 4, 1, viewBtnH - 8);
    vInner.removeFromLeft(divGap);

    btnRecordView.setBounds(vInner.removeFromLeft(viewBtnWidth).withHeight(viewBtnH));
    vInner.removeFromLeft(gap);
    btnAnalysisView.setBounds(vInner.removeFromLeft(viewBtnWidth).withHeight(viewBtnH));
    vInner.removeFromLeft(gap);
    btnEditView.setBounds(vInner.removeFromLeft(viewBtnWidth).withHeight(viewBtnH));
    vInner.removeFromLeft(gap);
    btnSampleMapView.setBounds(vInner.removeFromLeft(viewBtnWidth).withHeight(viewBtnH));

    viewDivider2 = juce::Rectangle<int>(vInner.getX() + 4, vInner.getY() + 4, 1, viewBtnH - 8);
    vInner.removeFromLeft(divGap);

    btnPerformanceView.setBounds(vInner.removeFromLeft(viewBtnWidth).withHeight(viewBtnH));

    area.removeFromRight(12);

    // 4. Format Filter Dropdown + Dynamic Search Editor
    formatFilterBox.setBounds(area.removeFromRight(106).withHeight(btnHeight));
    area.removeFromRight(8);

    searchEditor.setBounds(area.withHeight(btnHeight));
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
    btnPerformanceView.setToggleState(mode == ViewMode::Performance, juce::dontSendNotification);

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
