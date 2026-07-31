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
    searchEditor.setJustification(juce::Justification::centred);
    searchEditor.setTextToShowWhenEmpty("Search by name, tag, or path...", OpenWavLookAndFeel::textSecondary);
    searchEditor.addListener(this);
    addAndMakeVisible(searchEditor);

    // Add Folder Button
    addFolderButton.onClick = [this] {
        listeners.call([](HeaderBarListener& l) { l.addFolderRequested(); });
    };
    addAndMakeVisible(addFolderButton);

    // Rescan Button
    rescanButton.onClick = [this] {
        listeners.call([](HeaderBarListener& l) { l.rescanRequested(); });
    };
    addAndMakeVisible(rescanButton);

    // Settings Button (Audio / MIDI Setup)
    settingsButton.onClick = [this] {
        listeners.call([](HeaderBarListener& l) { l.settingsRequested(); });
    };
    addAndMakeVisible(settingsButton);

    // Format Filter Buttons
    auto setupFormatBtn = [this](juce::TextButton& btn, const juce::String& fmt) {
        btn.setClickingTogglesState(false);
        btn.onClick = [this, &btn, fmt] { setFormatFilter(fmt, &btn); };
        addAndMakeVisible(btn);
    };

    setupFormatBtn(btnAll, "All");
    setupFormatBtn(btnWav, ".wav");
    setupFormatBtn(btnMp3, ".mp3");
    setupFormatBtn(btnFlac, ".flac");
    setupFormatBtn(btnOgg, ".ogg");
    setupFormatBtn(btnAiff, ".aiff");

    btnAll.setToggleState(true, juce::dontSendNotification);

    // View Switcher (List vs Cloud vs Librarys)
    btnListView.onClick = [this] { setViewMode(ViewMode::List); };
    btnCloudView.onClick = [this] { setViewMode(ViewMode::Cloud); };
    btnLibrariesView.onClick = [this] { setViewMode(ViewMode::Libraries); };
    addAndMakeVisible(btnListView);
    addAndMakeVisible(btnCloudView);
    addAndMakeVisible(btnLibrariesView);
    btnListView.setToggleState(true, juce::dontSendNotification);

    // Status Label
    statusLabel.setFont(juce::Font(12.0f));
    statusLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    statusLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(statusLabel);

    // Progress Bar
    progressBar.setVisible(false);
    progressBar.setColour(juce::ProgressBar::backgroundColourId, OpenWavLookAndFeel::bgDark);
    progressBar.setColour(juce::ProgressBar::foregroundColourId, OpenWavLookAndFeel::accentCyan);
    progressBar.setTextToDisplay("");
    addAndMakeVisible(progressBar);

    updateLibraryCount(static_cast<int>(dbManager.getAllItems().size()));
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
}

void HeaderBarComponent::resized()
{
    auto area = getLocalBounds().reduced(12, 10);

    if (logoComponent.isVisible())
    {
        logoComponent.setBounds(area.removeFromLeft(150));
    }
    else
    {
        titleLabel.setBounds(area.removeFromLeft(90));
    }
    area.removeFromLeft(10);

    searchEditor.setBounds(area.removeFromLeft(220).withHeight(32));
    area.removeFromLeft(16);

    // Format buttons group with spacious padding
    int btnHeight = 32;
    int gap = 6;

    btnAll.setBounds(area.removeFromLeft(44).withHeight(btnHeight));
    area.removeFromLeft(gap);
    btnWav.setBounds(area.removeFromLeft(50).withHeight(btnHeight));
    area.removeFromLeft(gap);
    btnMp3.setBounds(area.removeFromLeft(50).withHeight(btnHeight));
    area.removeFromLeft(gap);
    btnFlac.setBounds(area.removeFromLeft(55).withHeight(btnHeight));
    area.removeFromLeft(gap);
    btnOgg.setBounds(area.removeFromLeft(50).withHeight(btnHeight));
    area.removeFromLeft(gap);
    btnAiff.setBounds(area.removeFromLeft(55).withHeight(btnHeight));

    area.removeFromLeft(16);

    // View Mode Toggle (List / Cloud / Library)
    btnListView.setBounds(area.removeFromLeft(65).withHeight(btnHeight));
    area.removeFromLeft(gap);
    btnCloudView.setBounds(area.removeFromLeft(75).withHeight(btnHeight));
    area.removeFromLeft(gap);
    btnLibrariesView.setBounds(area.removeFromLeft(85).withHeight(btnHeight));

    area.removeFromLeft(16);

    addFolderButton.setBounds(area.removeFromLeft(115).withHeight(btnHeight));
    area.removeFromLeft(gap);
    rescanButton.setBounds(area.removeFromLeft(90).withHeight(btnHeight));
    area.removeFromLeft(gap);
    settingsButton.setBounds(area.removeFromLeft(95).withHeight(btnHeight));

    statusLabel.setBounds(area.removeFromRight(150));
    progressBar.setBounds(statusLabel.getBounds().reduced(0, 4));
}

void HeaderBarComponent::textEditorTextChanged(juce::TextEditor& editor)
{
    if (&editor == &searchEditor)
    {
        listeners.call([txt = searchEditor.getText()](HeaderBarListener& l) {
            l.searchTextChanged(txt);
        });
    }
}

void HeaderBarComponent::setFormatFilter(const juce::String& ext, juce::TextButton* targetBtn)
{
    activeFormat = ext;
    btnAll.setToggleState(targetBtn == &btnAll, juce::dontSendNotification);
    btnWav.setToggleState(targetBtn == &btnWav, juce::dontSendNotification);
    btnMp3.setToggleState(targetBtn == &btnMp3, juce::dontSendNotification);
    btnFlac.setToggleState(targetBtn == &btnFlac, juce::dontSendNotification);
    btnOgg.setToggleState(targetBtn == &btnOgg, juce::dontSendNotification);
    btnAiff.setToggleState(targetBtn == &btnAiff, juce::dontSendNotification);

    listeners.call([ext](HeaderBarListener& l) {
        l.formatFilterChanged(ext);
    });
}

void HeaderBarComponent::scanStarted()
{
    juce::MessageManager::callAsync([this] {
        scanProgressValue = 0.0;
        statusLabel.setVisible(false);
        progressBar.setVisible(true);
    });
}

void HeaderBarComponent::scanProgress(int filesProcessed, int totalFiles, const juce::String& /*currentFile*/)
{
    juce::MessageManager::callAsync([this, filesProcessed, totalFiles] {
        if (totalFiles > 0)
        {
            scanProgressValue = static_cast<double>(filesProcessed) / totalFiles;
        }
    });
}

void HeaderBarComponent::scanFinished(int /*totalFilesDiscovered*/)
{
    juce::MessageManager::callAsync([this] {
        progressBar.setVisible(false);
        statusLabel.setVisible(true);
        updateLibraryCount(static_cast<int>(dbManager.getAllItems().size()));
    });
}

void HeaderBarComponent::updateLibraryCount(int count)
{
    statusLabel.setText("Library: " + juce::String(count) + " files", juce::dontSendNotification);
}

void HeaderBarComponent::setViewMode(ViewMode mode)
{
    currentViewMode = mode;
    btnListView.setToggleState(mode == ViewMode::List, juce::dontSendNotification);
    btnCloudView.setToggleState(mode == ViewMode::Cloud, juce::dontSendNotification);
    btnLibrariesView.setToggleState(mode == ViewMode::Libraries, juce::dontSendNotification);

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

    // Update labels, text fields, and progress bar colors
    titleLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    statusLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    progressBar.setColour(juce::ProgressBar::backgroundColourId, OpenWavLookAndFeel::bgDark);
    progressBar.setColour(juce::ProgressBar::foregroundColourId, OpenWavLookAndFeel::accentCyan);
    searchEditor.setTextToShowWhenEmpty("Search by name, tag, or path...", OpenWavLookAndFeel::textSecondary);
}

} // namespace openwav
