#include "LibrariesComponent.h"
#include "OpenWavLookAndFeel.h"

namespace openwav
{

class DownloadProgressCellComponent : public juce::Component,
                                      public juce::SettableTooltipClient
{
public:
    DownloadProgressCellComponent(std::function<void()> onActionClick)
        : actionCallback(onActionClick)
    {
        btnAction.onClick = [this] { if (actionCallback) actionCallback(); };
        addAndMakeVisible(btnAction);
    }

    void updateCallbacks(std::function<void()> onActionClick)
    {
        actionCallback = onActionClick;
    }

    void updateState(const PixeldrainFile& item)
    {
        isDownloaded = item.isDownloaded;
        isDownloading = item.isDownloading;
        isQueued = item.isQueued;
        isFailed = item.isFailed;
        isPreviewing = item.isPreviewing;
        progress = isPreviewing ? item.previewProgress : item.downloadProgress;
        fileName = item.name;
        failReason = item.failReason;

        if (isDownloading || isPreviewing)
        {
            btnAction.setVisible(false);
            setTooltip(isPreviewing ? "Streaming preview: " + juce::String(juce::roundToInt(progress * 100.0)) + "%"
                                    : "Downloading: " + juce::String(juce::roundToInt(progress * 100.0)) + "%");
        }
        else if (isQueued)
        {
            btnAction.setVisible(true);
            btnAction.setButtonText("Queued");
            btnAction.setEnabled(false);
            btnAction.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::accentBlue.withAlpha(0.2f));
            btnAction.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::accentBlue.withMultipliedBrightness(1.4f));
            btnAction.setTooltip("Waiting in sequential download queue");
            setTooltip("Waiting in sequential download queue");
        }
        else if (isFailed)
        {
            btnAction.setVisible(true);
            btnAction.setButtonText("Retry");
            btnAction.setEnabled(true);
            btnAction.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::favoriteRed.withAlpha(0.25f));
            btnAction.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::favoriteRed);
            juce::String tip = failReason.isNotEmpty() ? "Failed: " + failReason + " (Click to retry)" : "Click to retry download";
            btnAction.setTooltip(tip);
            setTooltip(tip);
        }
        else if (isDownloaded)
        {
            btnAction.setVisible(true);
            btnAction.setButtonText(item.isZip ? "Extracted" : "Downloaded");
            btnAction.setEnabled(false);
            btnAction.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(40, 167, 69).withAlpha(0.2f));
            btnAction.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(60, 200, 90));
            btnAction.setTooltip("File is present in local library");
            setTooltip("File is present in local library");
        }
        else
        {
            btnAction.setVisible(true);
            btnAction.setButtonText("Download");
            btnAction.setEnabled(true);
            btnAction.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::bgHover);
            btnAction.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::textPrimary);
            btnAction.setTooltip("Download file from Pixeldrain");
            setTooltip("Download file from Pixeldrain");
        }

        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        if (isDownloading || isPreviewing)
        {
            auto bounds = getLocalBounds().reduced(2, 4).toFloat();
            float cornerSize = 4.0f;

            // Background track
            g.setColour(OpenWavLookAndFeel::bgDark.withMultipliedBrightness(0.7f));
            g.fillRoundedRectangle(bounds, cornerSize);

            g.setColour(OpenWavLookAndFeel::borderColour);
            g.drawRoundedRectangle(bounds, cornerSize, 1.0f);

            // Progress bar
            float clampedProgress = juce::jlimit(0.0f, 1.0f, static_cast<float>(progress));
            if (clampedProgress > 0.005f)
            {
                auto progressBounds = bounds;
                progressBounds.setWidth(bounds.getWidth() * clampedProgress);

                juce::Colour startColour = isPreviewing ? OpenWavLookAndFeel::accentCyan.withMultipliedBrightness(0.85f)
                                                       : OpenWavLookAndFeel::accentCyan;
                juce::Colour endColour = isPreviewing ? OpenWavLookAndFeel::accentBlue
                                                     : OpenWavLookAndFeel::accentBlue.withMultipliedBrightness(1.2f);

                juce::ColourGradient grad(startColour, progressBounds.getTopLeft(),
                                          endColour, progressBounds.getBottomRight(), false);
                g.setGradientFill(grad);
                g.fillRoundedRectangle(progressBounds, cornerSize);

                // Subtle glowing top highlight
                g.setColour(juce::Colours::white.withAlpha(0.18f));
                auto highlightBounds = progressBounds.removeFromTop(progressBounds.getHeight() * 0.45f);
                g.fillRoundedRectangle(highlightBounds, cornerSize);
            }

            // Outline highlight
            g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.6f));
            g.drawRoundedRectangle(bounds, cornerSize, 1.0f);

            // Centered Percentage Text
            int pct = juce::roundToInt(progress * 100.0);
            juce::String text = isPreviewing ? "Stream " + juce::String(pct) + "%" : juce::String(pct) + "%";

            g.setFont(juce::Font(juce::FontOptions(11.5f).withStyle("Bold")));

            // Text shadow for high readability
            g.setColour(juce::Colours::black.withAlpha(0.7f));
            g.drawText(text, bounds.translated(1, 1), juce::Justification::centred, false);

            g.setColour(juce::Colours::white);
            g.drawText(text, bounds, juce::Justification::centred, false);
        }
    }

    void resized() override
    {
        btnAction.setBounds(getLocalBounds().reduced(2, 2));
    }

private:
    juce::TextButton btnAction { "Download" };
    std::function<void()> actionCallback;

    bool isDownloaded { false };
    bool isDownloading { false };
    bool isQueued { false };
    bool isFailed { false };
    bool isPreviewing { false };
    double progress { 0.0 };
    juce::String fileName;
    juce::String failReason;
};

class PixeldrainFolderTreeItem : public juce::TreeViewItem
{
public:
    PixeldrainFolderTreeItem(std::shared_ptr<PixeldrainFolderNode> node, LibrariesComponent& owner)
        : folderNode(node), owner(owner)
    {
        setDrawsInLeftMargin(true);
    }

    ~PixeldrainFolderTreeItem() override
    {
        clearSubItems();
    }

    int getItemHeight() const override
    {
        return 28;
    }

    bool mightContainSubItems() override
    {
        return folderNode != nullptr && !folderNode->subFolders.empty();
    }

    juce::String getUniqueName() const override
    {
        return folderNode != nullptr ? folderNode->fullPath : juce::String();
    }

    void itemOpennessChanged(bool isNowOpen) override
    {
        if (isNowOpen)
        {
            if (getNumSubItems() == 0 && folderNode != nullptr)
            {
                for (const auto& sub : folderNode->subFolders)
                {
                    addSubItem(new PixeldrainFolderTreeItem(sub, owner));
                }
            }
        }
    }

    void paintOpenCloseButton(juce::Graphics& g, const juce::Rectangle<float>& area, juce::Colour, bool isItemOpen) override
    {
        if (!mightContainSubItems()) return;

        auto center = area.getCentre();
        float s = 5.0f;
        juce::Path p;

        if (isItemOpen)
        {
            // Crisp down chevron
            p.startNewSubPath(center.x - s, center.y - s * 0.4f);
            p.lineTo(center.x, center.y + s * 0.6f);
            p.lineTo(center.x + s, center.y - s * 0.4f);
            g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.95f));
            g.strokePath(p, juce::PathStrokeType(1.8f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
        }
        else
        {
            // Crisp right chevron
            p.startNewSubPath(center.x - s * 0.4f, center.y - s);
            p.lineTo(center.x + s * 0.6f, center.y);
            p.lineTo(center.x - s * 0.4f, center.y + s);
            g.setColour(OpenWavLookAndFeel::textSecondary.withAlpha(0.75f));
            g.strokePath(p, juce::PathStrokeType(1.8f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
        }
    }

    void paintItem(juce::Graphics& g, int width, int height) override
    {
        if (folderNode == nullptr || width <= 0 || height <= 0) return;

        bool isSel = isSelected();
        bool isRoot = folderNode->isRoot;
        auto bounds = juce::Rectangle<float>(0.0f, 1.0f, static_cast<float>(width), static_cast<float>(height - 2));

        // 1. Selection Highlight Bar with cyber gradient & left neon indicator
        if (isSel)
        {
            juce::ColourGradient grad(OpenWavLookAndFeel::accentCyan.withAlpha(0.20f), 0.0f, 0.0f,
                                      OpenWavLookAndFeel::accentBlue.withAlpha(0.06f), static_cast<float>(width), 0.0f, false);
            g.setGradientFill(grad);
            g.fillRoundedRectangle(bounds, 4.0f);

            // Left Neon Active Bar
            g.setColour(OpenWavLookAndFeel::accentCyan);
            g.fillRoundedRectangle(1.0f, 3.0f, 3.0f, static_cast<float>(height - 6), 1.5f);

            // Subtle border glow
            g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.40f));
            g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
        }

        // 2. Vector Folder Icon
        float iconX = 6.0f;
        float iconY = static_cast<float>((height - 15) / 2);
        float iconW = 16.0f;
        float iconH = 13.0f;

        juce::Colour mainFolderColour = isSel ? OpenWavLookAndFeel::accentCyan
                                             : (isRoot ? OpenWavLookAndFeel::accentCyan.withMultipliedBrightness(0.9f)
                                                       : OpenWavLookAndFeel::accentBlue.withMultipliedBrightness(1.3f));

        // Folder back tab
        g.setColour(mainFolderColour.withAlpha(isSel ? 0.95f : 0.75f));
        g.fillRoundedRectangle(iconX, iconY, 7.0f, 5.0f, 1.5f);

        // Folder main body with 3D gradient
        juce::Colour bodyTop = isSel ? OpenWavLookAndFeel::accentCyan.withMultipliedBrightness(1.15f)
                                     : mainFolderColour;
        juce::Colour bodyBottom = isSel ? OpenWavLookAndFeel::accentBlue
                                        : mainFolderColour.withMultipliedBrightness(0.7f);

        juce::ColourGradient folderGrad(bodyTop, iconX, iconY + 3.0f,
                                        bodyBottom, iconX, iconY + iconH, false);
        g.setGradientFill(folderGrad);
        g.fillRoundedRectangle(iconX, iconY + 3.0f, iconW, iconH - 3.0f, 2.0f);

        // Top edge glossy reflection line
        g.setColour(juce::Colours::white.withAlpha(isSel ? 0.35f : 0.18f));
        g.drawLine(iconX + 1.0f, iconY + 4.0f, iconX + iconW - 1.0f, iconY + 4.0f, 1.0f);

        // Folder outline
        g.setColour(isSel ? OpenWavLookAndFeel::accentCyan.withAlpha(0.85f) : mainFolderColour.withAlpha(0.5f));
        g.drawRoundedRectangle(iconX, iconY + 3.0f, iconW, iconH - 3.0f, 2.0f, 1.0f);

        // 3. Folder Name Label
        int textX = static_cast<int>(iconX + iconW + 8.0f);
        int totalFiles = folderNode->getTotalFileCount();
        juce::String countStr = juce::String(totalFiles);

        int badgeWidth = 0;
        if (width > 80)
        {
            badgeWidth = juce::jmax(26, countStr.length() * 8 + 12);
        }

        int textWidth = juce::jmax(10, width - textX - badgeWidth - 6);

        g.setFont(juce::Font(juce::FontOptions(12.5f).withStyle(isSel ? "Bold" : (isRoot ? "Bold" : "Regular"))));
        g.setColour(isSel ? OpenWavLookAndFeel::accentCyan.withMultipliedBrightness(1.3f)
                          : (isRoot ? OpenWavLookAndFeel::accentCyan : OpenWavLookAndFeel::textPrimary));
        g.drawText(folderNode->name, textX, 0, textWidth, height, juce::Justification::centredLeft, true);

        // 4. Sleek Count Badge
        if (badgeWidth > 0)
        {
            auto badgeArea = juce::Rectangle<float>(static_cast<float>(width - badgeWidth - 4),
                                                    static_cast<float>((height - 18) / 2),
                                                    static_cast<float>(badgeWidth),
                                                    18.0f);

            // Badge Background
            juce::Colour badgeBg = isSel ? OpenWavLookAndFeel::accentCyan.withAlpha(0.22f)
                                         : OpenWavLookAndFeel::bgDark.withMultipliedBrightness(1.6f);
            g.setColour(badgeBg);
            g.fillRoundedRectangle(badgeArea, 9.0f);

            // Badge Border
            juce::Colour badgeBorder = isSel ? OpenWavLookAndFeel::accentCyan.withAlpha(0.6f)
                                             : OpenWavLookAndFeel::borderColour.withAlpha(0.65f);
            g.setColour(badgeBorder);
            g.drawRoundedRectangle(badgeArea, 9.0f, 1.0f);

            // Badge Text
            g.setFont(juce::Font(juce::FontOptions(10.5f).withStyle("Bold")));
            g.setColour(isSel ? OpenWavLookAndFeel::accentCyan.withMultipliedBrightness(1.3f)
                              : (totalFiles > 0 ? OpenWavLookAndFeel::textSecondary.withMultipliedBrightness(1.3f)
                                                : OpenWavLookAndFeel::textSecondary.withAlpha(0.5f)));
            g.drawText(countStr, badgeArea.toNearestInt(), juce::Justification::centred, false);
        }
    }

    void itemSelectionChanged(bool isNowSelected) override
    {
        if (isNowSelected && folderNode != nullptr)
        {
            juce::Component::SafePointer<LibrariesComponent> safeOwner(&owner);
            auto node = folderNode;
            juce::MessageManager::callAsync([safeOwner, node] {
                if (safeOwner != nullptr && node != nullptr)
                    safeOwner->selectFolder(node);
            });
        }
    }

    void itemClicked(const juce::MouseEvent& e) override
    {
        if (folderNode != nullptr)
        {
            if (e.mods.isPopupMenu())
            {
                juce::Component::SafePointer<LibrariesComponent> safeOwner(&owner);
                auto node = folderNode;
                juce::PopupMenu m;
                m.addItem(1, "Download Folder '" + node->name + "' (" + juce::String(node->getTotalFileCount()) + " audio files)");
                m.addSeparator();
                m.addItem(2, "Expand All Subfolders");
                m.addItem(3, "Collapse All Subfolders");

                m.showMenuAsync(juce::PopupMenu::Options(), [this, safeOwner, node](int result) {
                    if (result == 1)
                    {
                        if (safeOwner != nullptr && node != nullptr)
                            safeOwner->downloadFolder(node);
                    }
                    else if (result == 2)
                    {
                        setOpen(true);
                        for (int i = 0; i < getNumSubItems(); ++i)
                        {
                            if (auto* sub = getSubItem(i))
                                sub->setOpen(true);
                        }
                    }
                    else if (result == 3)
                    {
                        setOpen(false);
                    }
                });
            }
        }
    }

private:
    std::shared_ptr<PixeldrainFolderNode> folderNode;
    LibrariesComponent& owner;
};

static bool isSupportedAudioFile(const juce::String& name, const juce::String& mime)
{
    juce::String lower = name.toLowerCase().trim();
    juce::String base = juce::File(lower).getFileName();

    if (base.startsWith(".") || lower.contains(".search_index") || lower.contains("search_index.gz") ||
        lower.endsWith(".gz") || lower.contains("__macosx") || base.startsWith("._"))
        return false;

    juce::String ext = juce::File(name).getFileExtension().toLowerCase();
    if (ext == ".wav" || ext == ".mp3" || ext == ".flac" || ext == ".ogg" || ext == ".aiff" || ext == ".aif" || ext == ".aifc")
        return true;

    juce::String mimeLower = mime.toLowerCase();
    if (mimeLower.contains("wav") || mimeLower.contains("mpeg") || mimeLower.contains("mp3") ||
        mimeLower.contains("flac") || mimeLower.contains("ogg") || mimeLower.contains("aiff") || mimeLower.contains("aif"))
        return true;

    return false;
}

static bool isZipFile(const juce::String& name, const juce::String& mime)
{
    juce::String lower = name.toLowerCase().trim();
    juce::String base = juce::File(lower).getFileName();

    // Never consider gzip, search_index, tar, or hidden files as zip archives
    if (base.startsWith(".") || lower.contains(".search_index") || lower.contains("search_index.gz") ||
        lower.endsWith(".gz") || lower.endsWith(".tar") || lower.endsWith(".tgz") ||
        lower.endsWith(".7z") || lower.endsWith(".rar") || lower.contains("__macosx") || base.startsWith("._"))
        return false;

    juce::String ext = juce::File(name).getFileExtension().toLowerCase();
    if (ext == ".zip")
        return true;

    juce::String mimeLower = mime.toLowerCase();
    if (mimeLower == "application/zip" || mimeLower == "application/x-zip-compressed")
        return true;

    return false;
}

static bool isSupportedRemoteFile(const juce::String& name, const juce::String& mime)
{
    juce::String lower = name.toLowerCase().trim();
    juce::String base = juce::File(lower).getFileName();

    // Explicitly reject index files, hidden files, metadata, and non-supported archives
    if (base.startsWith(".") ||
        lower.contains(".search_index") ||
        lower.contains("search_index.gz") ||
        lower.endsWith(".gz") ||
        lower.endsWith(".tar") ||
        lower.endsWith(".7z") ||
        lower.endsWith(".rar") ||
        lower.endsWith(".json") ||
        lower.endsWith(".txt") ||
        lower.endsWith(".md") ||
        lower.endsWith(".xml") ||
        lower.endsWith(".ds_store") ||
        lower.startsWith("__macosx") ||
        base.startsWith("._"))
    {
        return false;
    }

    return isSupportedAudioFile(name, mime) || isZipFile(name, mime);
}



LibrariesComponent::LibrariesComponent(TagDatabaseManager& db, LibraryScanner& scanner, AudioEngine& audio)
    : dbManager(db), libraryScanner(scanner), audioEngine(audio)
{
    // Pixeldrain Logo on top right
    addAndMakeVisible(pixeldrainLogoComponent);

    // Load API Key or Hotlink from settings
    apiKeyEditor.setText(dbManager.getPixeldrainApiKey(), juce::dontSendNotification);
    apiKeyEditor.setJustification(juce::Justification::centredLeft);
    apiKeyEditor.setIndents(6, 0);
    apiKeyEditor.setTextToShowWhenEmpty("Enter API Key or Public Hotlink (e.g. /u/id, /l/id, /d/id)...", OpenWavLookAndFeel::textSecondary);
    apiKeyEditor.addListener(this);
    addAndMakeVisible(apiKeyEditor);

    apiKeyLabel.setFont(juce::Font(juce::FontOptions(13.0f).withStyle("Bold")));
    apiKeyLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textPrimary);
    addAndMakeVisible(apiKeyLabel);

    connectButton.onClick = [this] { fetchUserFiles(); };
    addAndMakeVisible(connectButton);

    statusLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
    statusLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    statusLabel.setText(statusText, juce::dontSendNotification);
    addAndMakeVisible(statusLabel);

    searchLabel.setFont(juce::Font(juce::FontOptions(13.0f).withStyle("Bold")));
    searchLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textPrimary);
    addAndMakeVisible(searchLabel);

    searchEditor.setJustification(juce::Justification::centredLeft);
    searchEditor.setIndents(6, 0);
    searchEditor.setTextToShowWhenEmpty("Filter remote files by name...", OpenWavLookAndFeel::textSecondary);
    searchEditor.addListener(this);
    addAndMakeVisible(searchEditor);

    juce::File downloadDir(dbManager.getDownloadFolder());
    saveDirLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
    saveDirLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    saveDirLabel.setText("Save to: " + downloadDir.getFullPathName(), juce::dontSendNotification);
    addAndMakeVisible(saveDirLabel);

    chooseDirButton.onClick = [this] {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Select Download Directory...",
            juce::File(dbManager.getDownloadFolder()),
            "*",
            true
        );
        chooser->launchAsync(juce::FileBrowserComponent::openMode |
                             juce::FileBrowserComponent::canSelectDirectories |
                             juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc) {
                auto result = fc.getResult();
                if (result == juce::File() && !fc.getResults().isEmpty())
                    result = fc.getResults().getFirst();

                juce::File dir = result.isDirectory() ? result : result.getParentDirectory();
                if (dir.exists() && dir.isDirectory())
                {
                    dbManager.setDownloadFolder(dir.getFullPathName());
                    saveDirLabel.setText("Save to: " + dir.getFullPathName(), juce::dontSendNotification);
                    updateDownloadStatuses();
                }
            });
    };
    addAndMakeVisible(chooseDirButton);

    downloadFolderButton.setButtonText("Download Folder");
    downloadFolderButton.onClick = [this] {
        if (selectedFolderNode != nullptr)
            downloadFolder(selectedFolderNode);
        else if (rootFolderNode != nullptr)
            downloadFolder(rootFolderNode);
    };
    addAndMakeVisible(downloadFolderButton);

    downloadAllWavsButton.setButtonText("Download All");
    downloadAllWavsButton.onClick = [this] { downloadAllWavs(); };
    addAndMakeVisible(downloadAllWavsButton);

    // Online Folders Tree View
    foldersHeaderLabel.setFont(juce::Font(juce::FontOptions(11.0f).withStyle("Bold")));
    foldersHeaderLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    foldersHeaderLabel.setColour(juce::Label::backgroundColourId, OpenWavLookAndFeel::bgDark.withMultipliedBrightness(0.7f));
    foldersHeaderLabel.setColour(juce::Label::outlineColourId, OpenWavLookAndFeel::borderColour);
    foldersHeaderLabel.setJustificationType(juce::Justification::centredLeft);
    foldersHeaderLabel.setText("  ONLINE FOLDERS", juce::dontSendNotification);
    addAndMakeVisible(foldersHeaderLabel);

    folderTreeView.setRootItemVisible(true);
    folderTreeView.setDefaultOpenness(true);
    folderTreeView.setOpenCloseButtonsVisible(true);
    folderTreeView.setIndentSize(14);
    folderTreeView.setColour(juce::TreeView::backgroundColourId, OpenWavLookAndFeel::bgDark.withMultipliedBrightness(0.6f));
    folderTreeView.setColour(juce::TreeView::linesColourId, OpenWavLookAndFeel::borderColour.withAlpha(0.35f));
    addAndMakeVisible(folderTreeView);

    // Breadcrumbs & Toggle
    breadcrumbLabel.setFont(juce::Font(juce::FontOptions(12.0f).withStyle("Bold")));
    breadcrumbLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textPrimary);
    breadcrumbLabel.setColour(juce::Label::backgroundColourId, OpenWavLookAndFeel::bgDark.withMultipliedBrightness(0.7f));
    breadcrumbLabel.setColour(juce::Label::outlineColourId, OpenWavLookAndFeel::borderColour);
    breadcrumbLabel.setJustificationType(juce::Justification::centredLeft);
    breadcrumbLabel.setText("All Files", juce::dontSendNotification);
    addAndMakeVisible(breadcrumbLabel);

    includeSubfoldersToggle.setButtonText("Include Subfolders");
    includeSubfoldersToggle.setToggleState(true, juce::dontSendNotification);
    includeSubfoldersToggle.onClick = [this] { filterRemoteFiles(); };
    addAndMakeVisible(includeSubfoldersToggle);

    // Setup Table Box
    auto& header = tableBox.getHeader();
    header.addColumn("#", 1, 40, 30, 60, juce::TableHeaderComponent::notSortable);
    header.addColumn("Name", 2, 280, 150, 600);
    header.addColumn("Type", 3, 70, 50, 100);
    header.addColumn("Size", 4, 80, 60, 120);
    header.addColumn("Uploaded", 5, 110, 90, 160);
    header.addColumn("Status", 6, 110, 80, 160);
    header.addColumn("Action", 7, 110, 80, 160);

    tableBox.setModel(this);
    tableBox.setRowHeight(36);
    addAndMakeVisible(tableBox);

    lookAndFeelChanged();

    if (apiKeyEditor.getText().isNotEmpty())
    {
        fetchUserFiles();
    }
}

LibrariesComponent::~LibrariesComponent()
{
    folderTreeView.setRootItem(nullptr);
    rootTreeItem.reset();
    tableBox.setModel(nullptr);
    apiKeyEditor.removeListener(this);
    searchEditor.removeListener(this);
}

void LibrariesComponent::paint(juce::Graphics& g)
{
    g.fillAll(OpenWavLookAndFeel::bgDark);

    if (folderTreeView.isVisible())
    {
        auto treeBounds = folderTreeView.getBounds().toFloat();
        g.setColour(OpenWavLookAndFeel::borderColour);
        g.drawRoundedRectangle(treeBounds, 3.0f, 1.0f);
    }
}

void LibrariesComponent::resized()
{
    auto area = getLocalBounds().reduced(16);

    // Header Controls Block
    auto topRow = area.removeFromTop(32);

    if (pixeldrainLogoComponent.isVisible())
    {
        pixeldrainLogoComponent.setBounds(topRow.removeFromRight(90));
        topRow.removeFromRight(12);
    }

    apiKeyLabel.setBounds(topRow.removeFromLeft(125));
    topRow.removeFromLeft(6);
    apiKeyEditor.setBounds(topRow.removeFromLeft(280));
    topRow.removeFromLeft(8);
    connectButton.setBounds(topRow.removeFromLeft(110));
    topRow.removeFromLeft(12);
    statusLabel.setBounds(topRow);

    area.removeFromTop(12);

    auto secondRow = area.removeFromTop(32);
    searchLabel.setBounds(secondRow.removeFromLeft(55));
    secondRow.removeFromLeft(6);
    searchEditor.setBounds(secondRow.removeFromLeft(220));
    secondRow.removeFromLeft(14);
    saveDirLabel.setBounds(secondRow.removeFromLeft(260));
    secondRow.removeFromLeft(8);
    chooseDirButton.setBounds(secondRow.removeFromLeft(120));
    secondRow.removeFromLeft(10);
    downloadFolderButton.setBounds(secondRow.removeFromLeft(140));
    secondRow.removeFromLeft(8);
    downloadAllWavsButton.setBounds(secondRow.removeFromLeft(130));

    area.removeFromTop(12);

    auto mainArea = area;
    int treeWidth = juce::jlimit(180, 320, juce::roundToInt(mainArea.getWidth() * 0.25f));

    auto leftArea = mainArea.removeFromLeft(treeWidth);
    mainArea.removeFromLeft(10); // gap between tree and table

    // Left Pane: Folders Tree
    auto leftHeaderArea = leftArea.removeFromTop(24);
    foldersHeaderLabel.setBounds(leftHeaderArea);
    leftArea.removeFromTop(4);
    folderTreeView.setBounds(leftArea);

    // Right Pane: Breadcrumbs + Table
    auto rightTopRow = mainArea.removeFromTop(24);
    includeSubfoldersToggle.setBounds(rightTopRow.removeFromRight(140));
    breadcrumbLabel.setBounds(rightTopRow);

    mainArea.removeFromTop(4);
    tableBox.setBounds(mainArea);

    auto& header = tableBox.getHeader();
    int tableWidth = mainArea.getWidth();
    int availableWidth = tableWidth - 40;
    if (availableWidth > 100)
    {
        double scale = static_cast<double>(availableWidth) / 760.0;
        header.setColumnWidth(2, static_cast<int>(280 * scale));
        header.setColumnWidth(3, static_cast<int>(70 * scale));
        header.setColumnWidth(4, static_cast<int>(80 * scale));
        header.setColumnWidth(5, static_cast<int>(110 * scale));
        header.setColumnWidth(6, static_cast<int>(110 * scale));
        header.setColumnWidth(7, static_cast<int>(110 * scale));
    }
}

int LibrariesComponent::getNumRows()
{
    return static_cast<int>(displayedFiles.size());
}

void LibrariesComponent::paintRowBackground(juce::Graphics& g, int rowNumber, int /*width*/, int /*height*/, bool rowIsSelected)
{
    if (rowIsSelected)
    {
        g.fillAll(OpenWavLookAndFeel::bgHover);
    }
    else if (rowNumber % 2 == 1)
    {
        g.fillAll(OpenWavLookAndFeel::bgDark.withMultipliedBrightness(1.05f));
    }
    else
    {
        g.fillAll(OpenWavLookAndFeel::bgDark);
    }
}

void LibrariesComponent::paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool /*rowIsSelected*/)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(displayedFiles.size()))
        return;

    const auto& item = displayedFiles[static_cast<size_t>(rowNumber)];

    g.setFont(juce::Font(juce::FontOptions(13.0f)));
    g.setColour(OpenWavLookAndFeel::textPrimary);

    juce::Rectangle<int> cellBounds(4, 0, width - 8, height);

    if (columnId == 1) // #
    {
        g.setColour(OpenWavLookAndFeel::textSecondary);
        g.drawText(juce::String(rowNumber + 1), cellBounds, juce::Justification::centredLeft);
    }
    else if (columnId == 2) // Name
    {
        if (item.isWav)
            g.setColour(OpenWavLookAndFeel::accentCyan);
        else if (item.isZip)
            g.setColour(OpenWavLookAndFeel::accentBlue.withMultipliedBrightness(1.25f));
        else
            g.setColour(OpenWavLookAndFeel::textPrimary);

        g.drawText(item.name, cellBounds, juce::Justification::centredLeft, true);
    }
    else if (columnId == 3) // Type
    {
        g.setColour(OpenWavLookAndFeel::textSecondary);
        if (item.isZip)
        {
            g.drawText("ZIP", cellBounds, juce::Justification::centredLeft);
        }
        else
        {
            juce::String ext = juce::File(item.name).getFileExtension().toUpperCase();
            if (ext.isEmpty()) ext = item.mimeType;
            g.drawText(ext, cellBounds, juce::Justification::centredLeft);
        }
    }
    else if (columnId == 4) // Size
    {
        g.setColour(OpenWavLookAndFeel::textSecondary);
        double mb = static_cast<double>(item.sizeBytes) / (1024.0 * 1024.0);
        juce::String sizeStr = (mb >= 1.0) ? juce::String(mb, 2) + " MB" : juce::String(item.sizeBytes / 1024) + " KB";
        g.drawText(sizeStr, cellBounds, juce::Justification::centredLeft);
    }
    else if (columnId == 5) // Uploaded
    {
        g.setColour(OpenWavLookAndFeel::textSecondary);
        juce::String dateStr = item.dateUpload;
        if (dateStr.contains("T"))
            dateStr = dateStr.upToFirstOccurrenceOf("T", false, false);
        g.drawText(dateStr, cellBounds, juce::Justification::centredLeft);
    }
    else if (columnId == 6) // Status
    {
        if (item.isDownloading)
        {
            g.setColour(OpenWavLookAndFeel::accentCyan);
            int pct = juce::roundToInt(item.downloadProgress * 100.0);
            g.drawText("Downloading " + juce::String(pct) + "%", cellBounds, juce::Justification::centredLeft);
        }
        else if (item.isQueued)
        {
            g.setColour(OpenWavLookAndFeel::accentBlue.withMultipliedBrightness(1.35f));
            g.drawText("Queued in Batch", cellBounds, juce::Justification::centredLeft);
        }
        else if (item.isPreviewing)
        {
            g.setColour(OpenWavLookAndFeel::accentCyan);
            int pct = juce::roundToInt(item.previewProgress * 100.0);
            g.drawText("Streaming " + juce::String(pct) + "%", cellBounds, juce::Justification::centredLeft);
        }
        else if (item.isDownloaded)
        {
            g.setColour(juce::Colour::fromRGB(40, 167, 69));
            g.drawText(item.isZip ? "Extracted" : "Local Library", cellBounds, juce::Justification::centredLeft);
        }
        else if (item.isFailed)
        {
            g.setColour(OpenWavLookAndFeel::favoriteRed);
            juce::String err = item.failReason.isNotEmpty() ? item.failReason : "Download Failed";
            g.drawText(err, cellBounds, juce::Justification::centredLeft);
        }
        else
        {
            g.setColour(OpenWavLookAndFeel::textSecondary);
            g.drawText("Cloud Only", cellBounds, juce::Justification::centredLeft);
        }
    }
}

juce::Component* LibrariesComponent::refreshComponentForCell(int rowNumber, int columnId, bool /*isRowSelected*/, juce::Component* existingComponentToUpdate)
{
    if (columnId != 7)
    {
        delete existingComponentToUpdate;
        return nullptr;
    }

    if (rowNumber < 0 || rowNumber >= static_cast<int>(displayedFiles.size()))
    {
        delete existingComponentToUpdate;
        return nullptr;
    }

    const auto& item = displayedFiles[static_cast<size_t>(rowNumber)];

    auto* actionComp = dynamic_cast<DownloadProgressCellComponent*>(existingComponentToUpdate);
    if (actionComp == nullptr)
    {
        actionComp = new DownloadProgressCellComponent(
            [this, rowNumber] { downloadFile(rowNumber); }
        );
    }
    else
    {
        actionComp->updateCallbacks(
            [this, rowNumber] { downloadFile(rowNumber); }
        );
    }

    actionComp->updateState(item);
    return actionComp;
}

void LibrariesComponent::cellDoubleClicked(int rowNumber, int /*columnId*/, const juce::MouseEvent& /*e*/)
{
    if (rowNumber >= 0 && rowNumber < static_cast<int>(displayedFiles.size()))
    {
        const auto& item = displayedFiles[static_cast<size_t>(rowNumber)];
        if (item.isZip)
        {
            if (item.isDownloaded)
            {
                statusLabel.setText("ZIP archive '" + item.name + "' is already downloaded & extracted in library.", juce::dontSendNotification);
            }
            else
            {
                downloadFile(rowNumber);
            }
            return;
        }

        if (item.isDownloaded && juce::File(item.localPath).existsAsFile())
        {
            audioEngine.loadFile(juce::File(item.localPath), true);
        }
        else
        {
            previewFile(rowNumber);
        }
    }
}

void LibrariesComponent::cellClicked(int rowNumber, int /*columnId*/, const juce::MouseEvent& /*e*/)
{
    if (rowNumber >= 0 && rowNumber < static_cast<int>(displayedFiles.size()))
    {
        tableBox.grabKeyboardFocus();
        if (tableBox.getSelectedRow() == rowNumber)
        {
            previewFile(rowNumber);
        }
        else
        {
            tableBox.selectRow(rowNumber);
        }
    }
}

void LibrariesComponent::selectedRowsChanged(int lastRowSelected)
{
    if (lastRowSelected >= 0 && lastRowSelected < static_cast<int>(displayedFiles.size()))
    {
        previewFile(lastRowSelected);
    }
}

void LibrariesComponent::textEditorTextChanged(juce::TextEditor& editor)
{
    if (&editor == &apiKeyEditor)
    {
        juce::String text = apiKeyEditor.getText().trim();
        dbManager.setPixeldrainApiKey(text);
        if (text.isEmpty())
        {
            allRemoteFiles.clear();
            displayedFiles.clear();
            rootFolderNode = nullptr;
            selectedFolderNode = nullptr;
            rebuildFolderTree();
            breadcrumbLabel.setText("All Files", juce::dontSendNotification);
            downloadFolderButton.setButtonText("Download Folder");
            tableBox.updateContent();
            tableBox.repaint();
            statusLabel.setText("Ready. Enter your Pixeldrain API key or hotlink to fetch account files.", juce::dontSendNotification);
        }
    }
    else if (&editor == &searchEditor)
    {
        filterRemoteFiles();
    }
}

static void extractFileObj(const juce::var& itemVar, std::vector<PixeldrainFile>& outFiles)
{
    if (!itemVar.isObject()) return;
    auto* fileObj = itemVar.getDynamicObject();
    if (!fileObj) return;

    if (fileObj->hasProperty("detail"))
    {
        auto detailVar = fileObj->getProperty("detail");
        if (detailVar.isObject())
            fileObj = detailVar.getDynamicObject();
    }

    juce::String id = fileObj->getProperty("id").toString();
    if (id.isEmpty()) id = fileObj->getProperty("file_id").toString();
    if (id.isEmpty()) id = fileObj->getProperty("path").toString();
    if (id.isEmpty()) return;

    juce::String name = fileObj->getProperty("name").toString();
    juce::String mime = fileObj->getProperty("mime_type").toString();
    if (mime.isEmpty()) mime = fileObj->getProperty("file_type").toString();

    // STRICT AUDIO + ZIP FILTER: .wav, .mp3, .flac, .ogg, .aiff (.aif) and .zip
    if (!isSupportedRemoteFile(name, mime))
        return;

    PixeldrainFile f;
    f.id = id;
    f.name = name;
    if (fileObj->hasProperty("size"))
        f.sizeBytes = static_cast<int64_t>(static_cast<juce::int64>(fileObj->getProperty("size")));
    else if (fileObj->hasProperty("file_size"))
        f.sizeBytes = static_cast<int64_t>(static_cast<juce::int64>(fileObj->getProperty("file_size")));

    f.dateUpload = fileObj->getProperty("date_upload").toString();
    if (f.dateUpload.isEmpty()) f.dateUpload = fileObj->getProperty("date_created").toString();
    if (f.dateUpload.isEmpty()) f.dateUpload = fileObj->getProperty("created").toString();
    if (f.dateUpload.isEmpty()) f.dateUpload = fileObj->getProperty("modified").toString();

    f.mimeType = mime;
    f.isWav = juce::File(name).getFileExtension().equalsIgnoreCase(".wav") || mime.containsIgnoreCase("wav");
    f.isZip = isZipFile(name, mime);
    f.folderPath = "/";
    f.relativePath = name;

    outFiles.push_back(f);
}

static juce::String urlEncodePath(const juce::String& path)
{
    juce::String encoded;
    auto utf8 = path.toRawUTF8();
    for (size_t i = 0; utf8[i] != 0; ++i)
    {
        unsigned char c = static_cast<unsigned char>(utf8[i]);
        if (c == '/' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~')
        {
            encoded << static_cast<char>(c);
        }
        else
        {
            encoded << "%" + juce::String::toHexString(static_cast<int>(c)).paddedLeft('0', 2).toUpperCase();
        }
    }
    return encoded;
}

static juce::URL::InputStreamOptions makeHttpOptions(const juce::String& authHeader = {}, int timeoutMs = 10000, bool acceptJson = true)
{
    auto opts = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                    .withConnectionTimeoutMs(timeoutMs)
                    .withNumRedirectsToFollow(5);

    juce::String extraHeaders = "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) OpenWav/1.0";
    if (acceptJson)
        extraHeaders += "\r\nAccept: application/json";
    else
        extraHeaders += "\r\nAccept: */*";

    if (authHeader.isNotEmpty())
        extraHeaders += "\r\n" + authHeader;

    return opts.withExtraHeaders(extraHeaders);
}

static void extractFilesFromFilesystemNode(const juce::var& nodeVar,
                                         std::vector<PixeldrainFile>& outFiles,
                                         const juce::String& authHeader,
                                         const juce::String& currentFolderPath,
                                         std::shared_ptr<PixeldrainFolderNode> currentFolderNode)
{
    if (!nodeVar.isObject() || currentFolderNode == nullptr) return;
    auto* obj = nodeVar.getDynamicObject();
    if (!obj) return;

    juce::String type = obj->getProperty("type").toString().toLowerCase();
    bool isDir = (type == "dir" || type == "directory" || static_cast<bool>(obj->getProperty("is_directory")));

    if (isDir)
    {
        juce::String dirName = obj->getProperty("name").toString();
        if (dirName.isEmpty()) dirName = obj->getProperty("path").toString();
        if (dirName.isEmpty()) dirName = "Folder";

        // Ignore hidden directories and search index directories
        if (dirName.startsWith(".") || dirName.startsWithIgnoreCase("__MACOSX") || dirName.containsIgnoreCase("search_index"))
            return;

        juce::String dirPath = currentFolderPath.isEmpty() ? ("/" + dirName) : (currentFolderPath + "/" + dirName);

        auto subFolderNode = std::make_shared<PixeldrainFolderNode>();
        subFolderNode->id = obj->getProperty("id").toString();
        if (subFolderNode->id.isEmpty()) subFolderNode->id = obj->getProperty("path").toString();
        subFolderNode->name = dirName;
        subFolderNode->fullPath = dirPath;
        subFolderNode->parentFolder = currentFolderNode;

        currentFolderNode->subFolders.push_back(subFolderNode);

        // Process inline children if present
        if (obj->hasProperty("children") && obj->getProperty("children").isArray())
        {
            auto cVar = obj->getProperty("children");
            for (const auto& child : *cVar.getArray())
            {
                extractFilesFromFilesystemNode(child, outFiles, authHeader, dirPath, subFolderNode);
            }
        }
        else
        {
            // Fetch children dynamically from filesystem API
            juce::String subDirId = subFolderNode->id;
            if (subDirId.isNotEmpty())
            {
                juce::URL subUrl("https://pixeldrain.com/api/filesystem/" + subDirId);
                std::unique_ptr<juce::InputStream> subStream(subUrl.createInputStream(makeHttpOptions(authHeader, 10000)));
                if (subStream != nullptr)
                {
                    auto subText = subStream->readEntireStreamAsString();
                    auto subParsed = juce::JSON::parse(subText);
                    if (subParsed.isObject())
                    {
                        auto* subObj = subParsed.getDynamicObject();
                        if (subObj && subObj->hasProperty("children"))
                        {
                            auto cVar = subObj->getProperty("children");
                            if (cVar.isArray())
                            {
                                for (const auto& child : *cVar.getArray())
                                {
                                    extractFilesFromFilesystemNode(child, outFiles, authHeader, dirPath, subFolderNode);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    else
    {
        juce::String name = obj->getProperty("name").toString();
        juce::String mime = obj->getProperty("mime_type").toString();
        if (mime.isEmpty()) mime = obj->getProperty("file_type").toString();

        if (!isSupportedRemoteFile(name, mime))
            return;

        PixeldrainFile f;
        f.id = obj->getProperty("id").toString();
        if (f.id.isEmpty()) f.id = obj->getProperty("file_id").toString();
        if (f.id.isEmpty()) f.id = obj->getProperty("path").toString();
        if (f.id.isEmpty()) f.id = name;

        f.name = name;
        f.mimeType = mime;
        if (obj->hasProperty("size"))
            f.sizeBytes = static_cast<int64_t>(static_cast<juce::int64>(obj->getProperty("size")));
        else if (obj->hasProperty("file_size"))
            f.sizeBytes = static_cast<int64_t>(static_cast<juce::int64>(obj->getProperty("file_size")));

        f.dateUpload = obj->getProperty("date_upload").toString();
        if (f.dateUpload.isEmpty()) f.dateUpload = obj->getProperty("date_created").toString();

        f.isWav = juce::File(f.name).getFileExtension().equalsIgnoreCase(".wav") || f.mimeType.containsIgnoreCase("wav");
        f.isZip = isZipFile(f.name, f.mimeType);

        f.folderPath = currentFolderPath;
        juce::String cleanFolder = currentFolderPath.trim().trimCharactersAtStart("/\\");
        f.relativePath = cleanFolder.isNotEmpty() ? (cleanFolder + "/" + f.name) : f.name;

        currentFolderNode->files.push_back(f);
        outFiles.push_back(f);
    }
}

static void buildTreeFromFlatFiles(std::vector<PixeldrainFile>& files, std::shared_ptr<PixeldrainFolderNode> rootNode)
{
    if (rootNode == nullptr) return;

    for (auto& f : files)
    {
        juce::String name = f.name;
        if (name.contains("/") || name.contains("\\"))
        {
            juce::String normalized = name.replace("\\", "/");
            juce::String folderPart = normalized.upToLastOccurrenceOf("/", false, false);
            juce::String baseName = normalized.fromLastOccurrenceOf("/", false, false);

            f.name = baseName;
            f.folderPath = "/" + folderPart;
            f.relativePath = normalized;

            auto current = rootNode;
            juce::StringArray tokens;
            tokens.addTokens(folderPart, "/", "");
            juce::String currentPath = "";

            for (const auto& token : tokens)
            {
                currentPath += "/" + token;
                std::shared_ptr<PixeldrainFolderNode> nextNode = nullptr;
                for (const auto& sub : current->subFolders)
                {
                    if (sub->name.equalsIgnoreCase(token))
                    {
                        nextNode = sub;
                        break;
                    }
                }
                if (nextNode == nullptr)
                {
                    nextNode = std::make_shared<PixeldrainFolderNode>();
                    nextNode->name = token;
                    nextNode->fullPath = currentPath;
                    nextNode->parentFolder = current;
                    current->subFolders.push_back(nextNode);
                }
                current = nextNode;
            }
            current->files.push_back(f);
        }
        else
        {
            f.folderPath = "/";
            f.relativePath = f.name;
            rootNode->files.push_back(f);
        }
    }
}

struct PixeldrainTarget
{
    enum Kind { UserAccount, List, SingleFile, Directory } kind { UserAccount };
    juce::String idOrKey;
};

static PixeldrainTarget parsePixeldrainInput(const juce::String& rawInput)
{
    PixeldrainTarget target;
    juce::String s = rawInput.trim();

    while (s.startsWith("\"") || s.startsWith("'")) s = s.substring(1);
    while (s.endsWith("\"") || s.endsWith("'")) s = s.dropLastCharacters(1);
    s = s.trim();

    juce::String lower = s.toLowerCase();

    // Check Shared Directory / Filesystem Link (e.g. /d/ or /filesystem/ or /dir/)
    if (lower.contains("/d/") || lower.contains("/filesystem/") || lower.contains("/dir/"))
    {
        target.kind = PixeldrainTarget::Directory;
        juce::String dirId = s;

        if (lower.contains("/filesystem/"))
            dirId = dirId.substring(lower.indexOf("/filesystem/") + 12);
        else if (lower.contains("/dir/"))
            dirId = dirId.substring(lower.indexOf("/dir/") + 5);
        else if (lower.contains("/d/"))
            dirId = dirId.substring(lower.indexOf("/d/") + 3);

        if (dirId.contains("?")) dirId = dirId.upToFirstOccurrenceOf("?", false, false);
        if (dirId.contains("#")) dirId = dirId.upToFirstOccurrenceOf("#", false, false);
        if (dirId.contains("/")) dirId = dirId.upToFirstOccurrenceOf("/", false, false);

        target.idOrKey = dirId.trim();
        return target;
    }

    if (lower.contains("/list/") || lower.contains("/l/"))
    {
        target.kind = PixeldrainTarget::List;
        juce::String listId = s;

        if (lower.contains("/list/"))
            listId = listId.substring(lower.indexOf("/list/") + 6);
        else if (lower.contains("/l/"))
            listId = listId.substring(lower.indexOf("/l/") + 3);

        if (listId.contains("?")) listId = listId.upToFirstOccurrenceOf("?", false, false);
        if (listId.contains("#")) listId = listId.upToFirstOccurrenceOf("#", false, false);
        if (listId.contains("/")) listId = listId.upToFirstOccurrenceOf("/", false, false);

        target.idOrKey = listId.trim();
        return target;
    }

    if (lower.contains("/file/") || lower.contains("/u/"))
    {
        target.kind = PixeldrainTarget::SingleFile;
        juce::String fileId = s;

        if (lower.contains("/file/"))
            fileId = fileId.substring(lower.indexOf("/file/") + 6);
        else if (lower.contains("/u/"))
            fileId = fileId.substring(lower.indexOf("/u/") + 3);

        if (fileId.endsWith("/info")) fileId = fileId.dropLastCharacters(5);
        if (fileId.contains("?")) fileId = fileId.upToFirstOccurrenceOf("?", false, false);
        if (fileId.contains("#")) fileId = fileId.upToFirstOccurrenceOf("#", false, false);
        if (fileId.contains("/")) fileId = fileId.upToFirstOccurrenceOf("/", false, false);

        target.idOrKey = fileId.trim();
        return target;
    }

    if (lower.contains("pixeldrain.com") || lower.contains("http://") || lower.contains("https://"))
    {
        juce::URL u(s);
        juce::String path = u.getSubPath();
        if (path.isEmpty()) path = s.fromLastOccurrenceOf("/", false, false);

        path = path.trim();
        if (path.startsWith("/")) path = path.substring(1);

        if (path.startsWithIgnoreCase("d/"))
        {
            target.kind = PixeldrainTarget::Directory;
            target.idOrKey = path.substring(2);
        }
        else if (path.startsWithIgnoreCase("l/"))
        {
            target.kind = PixeldrainTarget::List;
            target.idOrKey = path.substring(2);
        }
        else if (path.startsWithIgnoreCase("u/"))
        {
            target.kind = PixeldrainTarget::SingleFile;
            target.idOrKey = path.substring(2);
        }
        else if (path.startsWithIgnoreCase("list/"))
        {
            target.kind = PixeldrainTarget::List;
            target.idOrKey = path.substring(5);
        }
        else if (path.startsWithIgnoreCase("file/"))
        {
            target.kind = PixeldrainTarget::SingleFile;
            target.idOrKey = path.substring(5);
        }
        else if (path.startsWithIgnoreCase("filesystem/"))
        {
            target.kind = PixeldrainTarget::Directory;
            target.idOrKey = path.substring(11);
        }
        else
        {
            target.kind = PixeldrainTarget::Directory;
            target.idOrKey = path;
        }

        if (target.idOrKey.contains("?")) target.idOrKey = target.idOrKey.upToFirstOccurrenceOf("?", false, false);
        if (target.idOrKey.contains("/")) target.idOrKey = target.idOrKey.upToFirstOccurrenceOf("/", false, false);
        return target;
    }

    target.kind = PixeldrainTarget::UserAccount;
    target.idOrKey = s;
    return target;
}

void LibrariesComponent::fetchUserFiles()
{
    juce::String inputStr = apiKeyEditor.getText().trim();
    if (inputStr.isEmpty())
    {
        allRemoteFiles.clear();
        displayedFiles.clear();
        rootFolderNode = nullptr;
        selectedFolderNode = nullptr;
        rebuildFolderTree();
        breadcrumbLabel.setText("All Files", juce::dontSendNotification);
        downloadFolderButton.setButtonText("Download Folder");
        tableBox.updateContent();
        tableBox.repaint();
        statusLabel.setText("Please enter an API Key or Public Hotlink (e.g. /d/id, /u/id, or /l/id).", juce::dontSendNotification);
        return;
    }

    dbManager.setPixeldrainApiKey(inputStr);
    statusLabel.setText("Fetching from Pixeldrain...", juce::dontSendNotification);
    isFetching = true;

    auto target = parsePixeldrainInput(inputStr);

    juce::Component::SafePointer<LibrariesComponent> safeThis(this);

    juce::Thread::launch([safeThis, target] {
        if (safeThis == nullptr) return;

        std::vector<PixeldrainFile> fetchedFiles;
        auto rootNode = std::make_shared<PixeldrainFolderNode>();
        rootNode->id = target.idOrKey;
        rootNode->name = (target.kind == PixeldrainTarget::Directory) ? "Cloud Folder" : (target.kind == PixeldrainTarget::List ? "Cloud List" : "Cloud Library");
        rootNode->fullPath = "/";
        rootNode->isRoot = true;

        juce::String errorMsg;

        if (target.kind == PixeldrainTarget::Directory)
        {
            juce::URL url("https://pixeldrain.com/api/filesystem/" + target.idOrKey);
            std::unique_ptr<juce::InputStream> stream(url.createInputStream(makeHttpOptions("", 10000)));

            if (stream != nullptr)
            {
                auto responseText = stream->readEntireStreamAsString();
                auto parsed = juce::JSON::parse(responseText);
                if (parsed.isObject())
                {
                    auto* obj = parsed.getDynamicObject();
                    if (obj && obj->hasProperty("name"))
                    {
                        juce::String folderTitle = obj->getProperty("name").toString();
                        if (folderTitle.isNotEmpty())
                            rootNode->name = folderTitle;
                    }

                    if (obj && obj->hasProperty("children"))
                    {
                        auto cVar = obj->getProperty("children");
                        if (cVar.isArray())
                        {
                            for (const auto& child : *cVar.getArray())
                            {
                                extractFilesFromFilesystemNode(child, fetchedFiles, "", "", rootNode);
                            }
                        }
                    }
                }
            }

            // Fallback: If filesystem endpoint returned no files, try list endpoint!
            if (fetchedFiles.empty())
            {
                juce::URL listUrl("https://pixeldrain.com/api/list/" + target.idOrKey);
                std::unique_ptr<juce::InputStream> listStream(listUrl.createInputStream(makeHttpOptions("", 10000)));
                if (listStream != nullptr)
                {
                    auto listText = listStream->readEntireStreamAsString();
                    auto listParsed = juce::JSON::parse(listText);
                    if (listParsed.isObject())
                    {
                        auto* obj = listParsed.getDynamicObject();
                        if (obj && obj->hasProperty("title"))
                        {
                            juce::String t = obj->getProperty("title").toString();
                            if (t.isNotEmpty()) rootNode->name = t;
                        }
                        if (obj && obj->hasProperty("files"))
                        {
                            auto fVar = obj->getProperty("files");
                            if (fVar.isArray())
                            {
                                for (const auto& itemVar : *fVar.getArray())
                                    extractFileObj(itemVar, fetchedFiles);
                            }
                        }
                    }
                }
                buildTreeFromFlatFiles(fetchedFiles, rootNode);
            }

            if (fetchedFiles.empty())
            {
                errorMsg = "Failed to fetch shared folder: " + target.idOrKey;
            }
        }
        else if (target.kind == PixeldrainTarget::SingleFile)
        {
            juce::URL url("https://pixeldrain.com/api/file/" + target.idOrKey + "/info");
            std::unique_ptr<juce::InputStream> stream(url.createInputStream(makeHttpOptions("", 10000)));

            if (stream != nullptr)
            {
                auto responseText = stream->readEntireStreamAsString();
                auto parsed = juce::JSON::parse(responseText);
                extractFileObj(parsed, fetchedFiles);
                buildTreeFromFlatFiles(fetchedFiles, rootNode);

                // Fallback 1: Try list endpoint
                if (fetchedFiles.empty())
                {
                    juce::URL listUrl("https://pixeldrain.com/api/list/" + target.idOrKey);
                    std::unique_ptr<juce::InputStream> listStream(listUrl.createInputStream(makeHttpOptions("", 10000)));
                    if (listStream != nullptr)
                    {
                        auto listText = listStream->readEntireStreamAsString();
                        auto listParsed = juce::JSON::parse(listText);
                        if (listParsed.isObject())
                        {
                            auto* obj = listParsed.getDynamicObject();
                            if (obj && obj->hasProperty("files"))
                            {
                                auto fVar = obj->getProperty("files");
                                if (fVar.isArray())
                                {
                                    for (const auto& itemVar : *fVar.getArray())
                                        extractFileObj(itemVar, fetchedFiles);
                                }
                            }
                        }
                    }
                    buildTreeFromFlatFiles(fetchedFiles, rootNode);
                }

                // Fallback 2: Try filesystem endpoint
                if (fetchedFiles.empty())
                {
                    juce::URL fsUrl("https://pixeldrain.com/api/filesystem/" + target.idOrKey);
                    std::unique_ptr<juce::InputStream> fsStream(fsUrl.createInputStream(makeHttpOptions("", 10000)));
                    if (fsStream != nullptr)
                    {
                        auto fsText = fsStream->readEntireStreamAsString();
                        auto fsParsed = juce::JSON::parse(fsText);
                        if (fsParsed.isObject())
                        {
                            auto* obj = fsParsed.getDynamicObject();
                            if (obj && obj->hasProperty("children"))
                            {
                                auto cVar = obj->getProperty("children");
                                if (cVar.isArray())
                                {
                                    for (const auto& child : *cVar.getArray())
                                        extractFilesFromFilesystemNode(child, fetchedFiles, "", "", rootNode);
                                }
                            }
                        }
                    }
                }
            }
            else
            {
                errorMsg = "Failed to fetch public file: " + target.idOrKey;
            }
        }
        else if (target.kind == PixeldrainTarget::List)
        {
            juce::URL url("https://pixeldrain.com/api/list/" + target.idOrKey);
            std::unique_ptr<juce::InputStream> stream(url.createInputStream(makeHttpOptions("", 10000)));

            if (stream != nullptr)
            {
                auto responseText = stream->readEntireStreamAsString();
                auto parsed = juce::JSON::parse(responseText);
                if (parsed.isObject())
                {
                    auto* obj = parsed.getDynamicObject();
                    if (obj && obj->hasProperty("title"))
                    {
                        juce::String t = obj->getProperty("title").toString();
                        if (t.isNotEmpty()) rootNode->name = t;
                    }
                    if (obj && obj->hasProperty("files"))
                    {
                        auto fVar = obj->getProperty("files");
                        if (fVar.isArray())
                        {
                            for (const auto& itemVar : *fVar.getArray())
                            {
                                extractFileObj(itemVar, fetchedFiles);
                            }
                        }
                    }
                }
                buildTreeFromFlatFiles(fetchedFiles, rootNode);
            }

            // Fallback to filesystem endpoint
            if (fetchedFiles.empty())
            {
                juce::URL fsUrl("https://pixeldrain.com/api/filesystem/" + target.idOrKey);
                std::unique_ptr<juce::InputStream> fsStream(fsUrl.createInputStream(makeHttpOptions("", 10000)));
                if (fsStream != nullptr)
                {
                    auto fsText = fsStream->readEntireStreamAsString();
                    auto fsParsed = juce::JSON::parse(fsText);
                    if (fsParsed.isObject())
                    {
                        auto* obj = fsParsed.getDynamicObject();
                        if (obj && obj->hasProperty("children"))
                        {
                            auto cVar = obj->getProperty("children");
                            if (cVar.isArray())
                            {
                                for (const auto& child : *cVar.getArray())
                                    extractFilesFromFilesystemNode(child, fetchedFiles, "", "", rootNode);
                            }
                        }
                    }
                }
            }

            if (fetchedFiles.empty())
            {
                errorMsg = "Failed to fetch public list: " + target.idOrKey;
            }
        }
        else // UserAccount
        {
            juce::URL url("https://pixeldrain.com/api/user/files");
            juce::String authHeader = "Authorization: Basic " + juce::Base64::toBase64(":" + target.idOrKey);

            std::unique_ptr<juce::InputStream> stream(url.createInputStream(makeHttpOptions(authHeader, 10000)));
            if (stream != nullptr)
            {
                auto responseText = stream->readEntireStreamAsString();
                auto parsed = juce::JSON::parse(responseText);

                juce::Array<juce::var>* filesArray = nullptr;

                if (parsed.isObject())
                {
                    auto* obj = parsed.getDynamicObject();
                    if (obj && obj->hasProperty("files"))
                    {
                        auto fVar = obj->getProperty("files");
                        if (fVar.isArray())
                            filesArray = fVar.getArray();
                    }
                }
                else if (parsed.isArray())
                {
                    filesArray = parsed.getArray();
                }

                if (filesArray != nullptr)
                {
                    for (const auto& itemVar : *filesArray)
                    {
                        extractFileObj(itemVar, fetchedFiles);
                    }
                }
                buildTreeFromFlatFiles(fetchedFiles, rootNode);
            }
            else
            {
                errorMsg = "Failed to connect to Pixeldrain API. Check API key.";
            }
        }

        juce::MessageManager::callAsync([safeThis, fetchedFiles, rootNode, errorMsg] {
            if (safeThis == nullptr) return;

            safeThis->isFetching = false;
            if (fetchedFiles.empty())
            {
                safeThis->allRemoteFiles.clear();
                safeThis->displayedFiles.clear();
                safeThis->rootFolderNode = nullptr;
                safeThis->rebuildFolderTree();
                safeThis->tableBox.updateContent();

                if (errorMsg.isNotEmpty())
                    safeThis->statusLabel.setText("Notice: " + errorMsg, juce::dontSendNotification);
                else
                    safeThis->statusLabel.setText("No supported files found (.wav, .mp3, .flac, .ogg, .aiff, .zip).", juce::dontSendNotification);
            }
            else
            {
                safeThis->allRemoteFiles = fetchedFiles;
                safeThis->rootFolderNode = rootNode;
                safeThis->rebuildFolderTree();
                safeThis->updateDownloadStatuses();
                safeThis->filterRemoteFiles();

                safeThis->statusLabel.setText("Loaded " + juce::String(safeThis->allRemoteFiles.size()) + " file(s) in " +
                                   juce::String(rootNode->subFolders.size()) + " folder(s).", juce::dontSendNotification);
            }
        });
    });
}

void LibrariesComponent::updateDownloadStatuses()
{
    juce::File targetDir(dbManager.getDownloadFolder());

    for (auto& f : allRemoteFiles)
    {
        juce::String relPath = f.relativePath.isNotEmpty() ? f.relativePath : f.name;
        juce::File checkFile = targetDir.getChildFile(relPath);
        if (!checkFile.existsAsFile())
            checkFile = targetDir.getChildFile(f.name);

        if (checkFile.existsAsFile() && checkFile.getSize() > 0)
        {
            f.isDownloaded = true;
            f.isQueued = false;
            f.isDownloading = false;
            f.isFailed = false;
            f.downloadProgress = 1.0;
            f.localPath = checkFile.getFullPathName();
        }
    }
}

void LibrariesComponent::updateBatchProgressLabel()
{
    if (totalBatchCount > 0 && sequentialDownloader != nullptr && sequentialDownloader->isThreadRunning())
    {
        int activeIdx = juce::jmin(totalBatchCount, completedBatchCount + 1);
        statusLabel.setText("Batch downloading (" + juce::String(activeIdx) + "/" + juce::String(totalBatchCount) + ")...", juce::dontSendNotification);
    }
}

void LibrariesComponent::selectFolder(std::shared_ptr<PixeldrainFolderNode> folder)
{
    selectedFolderNode = folder;
    if (folder != nullptr)
    {
        juce::String pathStr = folder->fullPath;
        if (pathStr.isEmpty() || pathStr == "/") pathStr = "All Files (" + folder->name + ")";
        breadcrumbLabel.setText(pathStr + "   |   " + juce::String(folder->getTotalFileCount()) + " audio file(s)", juce::dontSendNotification);
        downloadFolderButton.setButtonText("Download Folder (" + juce::String(folder->getTotalFileCount()) + ")");
    }
    else
    {
        breadcrumbLabel.setText("All Files", juce::dontSendNotification);
        downloadFolderButton.setButtonText("Download Folder");
    }
    filterRemoteFiles();
}

void LibrariesComponent::rebuildFolderTree()
{
    folderTreeView.setRootItem(nullptr);
    rootTreeItem.reset();

    if (rootFolderNode == nullptr)
        return;

    rootTreeItem = std::make_unique<PixeldrainFolderTreeItem>(rootFolderNode, *this);
    folderTreeView.setRootItem(rootTreeItem.get());
    rootTreeItem->setOpen(true);

    selectedFolderNode = rootFolderNode;
    selectFolder(rootFolderNode);
}

void LibrariesComponent::filterRemoteFiles()
{
    juce::String kw = searchEditor.getText().trim().toLowerCase();
    bool includeSubs = includeSubfoldersToggle.getToggleState();
    displayedFiles.clear();

    std::vector<PixeldrainFile> candidateFiles;
    if (selectedFolderNode == nullptr || selectedFolderNode->isRoot)
    {
        candidateFiles = allRemoteFiles;
    }
    else
    {
        if (includeSubs)
            selectedFolderNode->getAllFilesRecursive(candidateFiles);
        else
            candidateFiles = selectedFolderNode->files;
    }

    // Sync latest status from allRemoteFiles into candidateFiles
    for (auto& cand : candidateFiles)
    {
        for (const auto& master : allRemoteFiles)
        {
            if (master.id == cand.id)
            {
                cand = master;
                break;
            }
        }
    }

    for (const auto& f : candidateFiles)
    {
        if (kw.isEmpty() || f.name.toLowerCase().contains(kw) || f.mimeType.toLowerCase().contains(kw) || f.relativePath.toLowerCase().contains(kw))
        {
            displayedFiles.push_back(f);
        }
    }

    tableBox.updateContent();
    tableBox.repaint();
}

void LibrariesComponent::downloadFolder(std::shared_ptr<PixeldrainFolderNode> folder)
{
    if (folder == nullptr) return;

    std::vector<PixeldrainFile> folderFiles;
    folder->getAllFilesRecursive(folderFiles);

    queueDownloads(folderFiles, "folder '" + folder->name + "'");
}

void LibrariesComponent::downloadAllWavs()
{
    if (sequentialDownloader != nullptr && sequentialDownloader->isThreadRunning())
    {
        cancelAllDownloads();
        return;
    }

    auto& sourceList = searchEditor.getText().trim().isEmpty() ? allRemoteFiles : displayedFiles;
    queueDownloads(sourceList, "all remote files");
}

void LibrariesComponent::queueDownloads(const std::vector<PixeldrainFile>& filesToQueue, const juce::String& batchTitle)
{
    if (sequentialDownloader != nullptr && sequentialDownloader->isThreadRunning())
    {
        cancelAllDownloads();
        return;
    }

    cancelRequested = false;
    std::vector<QueuedDownload> newJobs;

    for (const auto& item : filesToQueue)
    {
        if (!item.isDownloaded && !item.isDownloading && !item.isQueued)
        {
            for (auto& f : allRemoteFiles)
            {
                if (f.id == item.id)
                {
                    f.isQueued = true;
                    f.isFailed = false;
                    f.failReason = "";
                    f.downloadProgress = 0.0;
                }
            }

            QueuedDownload job;
            job.fileId = item.id;
            job.fileName = item.name;
            job.relativePath = item.relativePath.isNotEmpty() ? item.relativePath : item.name;
            job.sizeBytes = item.sizeBytes;
            job.isZip = item.isZip || juce::File(item.name).getFileExtension().equalsIgnoreCase(".zip");
            newJobs.push_back(job);
        }
    }

    if (newJobs.empty())
    {
        statusLabel.setText("No new files to download in " + batchTitle + ".", juce::dontSendNotification);
        return;
    }

    totalBatchCount = static_cast<int>(newJobs.size());
    completedBatchCount = 0;

    downloadAllWavsButton.setButtonText("Cancel All");
    downloadAllWavsButton.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::favoriteRed.withAlpha(0.3f));
    downloadAllWavsButton.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::favoriteRed);

    statusLabel.setText("Queued " + juce::String(newJobs.size()) + " file(s) from " + batchTitle + "...", juce::dontSendNotification);
    filterRemoteFiles();

    {
        const juce::ScopedLock sl (downloadQueueLock);
        for (const auto& job : newJobs)
        {
            downloadQueue.push_back(job);
            activeDownloadCount++;
        }
    }

    if (sequentialDownloader == nullptr || !sequentialDownloader->isThreadRunning())
    {
        sequentialDownloader = std::make_unique<SequentialDownloader>(*this);
        sequentialDownloader->startThread();
    }
}

void LibrariesComponent::downloadFile(int displayedIndex)
{
    if (displayedIndex < 0 || displayedIndex >= static_cast<int>(displayedFiles.size()))
        return;

    auto& targetItem = displayedFiles[static_cast<size_t>(displayedIndex)];
    if (targetItem.isDownloading || targetItem.isDownloaded)
        return;

    targetItem.isDownloading = true;
    targetItem.isQueued = false;
    targetItem.isFailed = false;
    targetItem.failReason = "";
    targetItem.downloadProgress = 0.0;

    for (auto& f : allRemoteFiles)
    {
        if (f.id == targetItem.id)
        {
            f.isDownloading = true;
            f.isQueued = false;
            f.isFailed = false;
            f.failReason = "";
            f.downloadProgress = 0.0;
        }
    }

    activeDownloadCount++;
    tableBox.repaint();

    juce::String fileId = targetItem.id;
    juce::String fileName = targetItem.name;
    juce::String relPath = targetItem.relativePath.isNotEmpty() ? targetItem.relativePath : targetItem.name;
    int64_t sizeBytes = targetItem.sizeBytes;
    bool isZip = targetItem.isZip || juce::File(fileName).getFileExtension().equalsIgnoreCase(".zip");
    juce::String inputStr = apiKeyEditor.getText().trim();
    juce::File destFolder(dbManager.getDownloadFolder());
    if (!destFolder.exists())
        destFolder.createDirectory();

    juce::File destFile = destFolder.getChildFile(relPath);
    destFile.getParentDirectory().createDirectory();

    statusLabel.setText("Downloading: " + fileName + "...", juce::dontSendNotification);

    juce::Component::SafePointer<LibrariesComponent> safeThis(this);

    juce::Thread::launch([safeThis, fileId, fileName, sizeBytes, inputStr, destFile, isZip] {
        if (safeThis == nullptr) return;

        auto shouldExit = [safeThis] { return safeThis == nullptr; };
        bool success = false;

        // Try download with automatic retry
        for (int attempt = 1; attempt <= 3; ++attempt)
        {
            if (safeThis == nullptr) return;
            if (attempt > 1) juce::Thread::sleep(500 * attempt);

            success = downloadFileSync(fileId, fileName, sizeBytes, inputStr, destFile, shouldExit, safeThis);
            if (success) break;
        }

        int extractedCount = 0;
        if (success && isZip && destFile.existsAsFile())
        {
            juce::MessageManager::callAsync([safeThis, fileName] {
                if (safeThis != nullptr)
                    safeThis->statusLabel.setText("Extracting audio files from " + fileName + "...", juce::dontSendNotification);
            });

            juce::String statusMsg;
            extractedCount = extractAudioFilesFromZip(destFile, destFile.getParentDirectory(), statusMsg);
        }

        juce::String failReason = success ? "" : "Download Failed";

        juce::MessageManager::callAsync([safeThis, fileId, destFile, success, isZip, extractedCount, failReason] {
            if (safeThis != nullptr)
            {
                safeThis->handleDownloadFinished(fileId, destFile, success, isZip, extractedCount, failReason);
            }
        });
    });
}

void LibrariesComponent::cancelAllDownloads()
{
    cancelRequested = true;

    {
        const juce::ScopedLock sl (downloadQueueLock);
        downloadQueue.clear();
    }

    if (sequentialDownloader != nullptr && sequentialDownloader->isThreadRunning())
    {
        sequentialDownloader->signalThreadShouldExit();
        sequentialDownloader->stopThread(250);
    }

    activeDownloadCount = 0;

    for (auto& f : allRemoteFiles)
    {
        if (f.isQueued || f.isDownloading)
        {
            f.isQueued = false;
            f.isDownloading = false;
            f.downloadProgress = 0.0;
        }
    }

    for (auto& f : displayedFiles)
    {
        if (f.isQueued || f.isDownloading)
        {
            f.isQueued = false;
            f.isDownloading = false;
            f.downloadProgress = 0.0;
        }
    }

    downloadAllWavsButton.setButtonText("Download All");
    downloadAllWavsButton.removeColour(juce::TextButton::buttonColourId);
    downloadAllWavsButton.removeColour(juce::TextButton::textColourOffId);

    statusLabel.setText("Batch download cancelled.", juce::dontSendNotification);
    tableBox.updateContent();
    tableBox.repaint();
}

bool LibrariesComponent::downloadFileSync(const juce::String& fileId,
                                         const juce::String& fileName,
                                         int64_t expectedSizeBytes,
                                         const juce::String& apiKey,
                                         const juce::File& destFile,
                                         std::function<bool()> shouldExit,
                                         juce::Component::SafePointer<LibrariesComponent> safeThis,
                                         bool isPreview)
{
    auto target = parsePixeldrainInput(apiKey);
    juce::String rootDirId = target.idOrKey;

    juce::String cleanId = fileId.trim();
    juce::String cleanName = fileName.trim().trimCharactersAtStart("/\\");
    juce::String cleanRel = cleanName;

    if (safeThis != nullptr)
    {
        for (const auto& f : safeThis->allRemoteFiles)
        {
            if (f.id == fileId || f.name == fileName)
            {
                if (f.relativePath.isNotEmpty())
                    cleanRel = f.relativePath.trim().trimCharactersAtStart("/\\");
                break;
            }
        }
    }

    juce::StringArray candidateUrls;

    if (cleanId.startsWithIgnoreCase("http://") || cleanId.startsWithIgnoreCase("https://"))
    {
        juce::String fullUrl = cleanId;
        if (fullUrl.contains("/u/"))
        {
            juce::String idPart = fullUrl.substring(fullUrl.indexOf("/u/") + 3);
            if (idPart.contains("?")) idPart = idPart.upToFirstOccurrenceOf("?", false, false);
            candidateUrls.add("https://pixeldrain.com/api/file/" + idPart + "?download");
            candidateUrls.add("https://pixeldrain.com/api/file/" + idPart);
            candidateUrls.add("https://pixeldrain.com/api/filesystem/" + idPart + "?download");
        }
        else
        {
            if (!fullUrl.contains("?download") && !fullUrl.contains("&download"))
                candidateUrls.add(fullUrl + (fullUrl.contains("?") ? "&download" : "?download"));
            candidateUrls.add(fullUrl);
        }
    }
    else
    {
        // 1. Filesystem directory path endpoints (e.g. /d/<rootId>/<path>)
        if (target.kind == PixeldrainTarget::Directory && rootDirId.isNotEmpty())
        {
            if (cleanRel.isNotEmpty())
                candidateUrls.add("https://pixeldrain.com/api/filesystem/" + urlEncodePath(rootDirId + "/" + cleanRel) + "?download");
            if (cleanName.isNotEmpty() && cleanName != cleanRel)
                candidateUrls.add("https://pixeldrain.com/api/filesystem/" + urlEncodePath(rootDirId + "/" + cleanName) + "?download");
            if (cleanId != cleanRel && cleanId != cleanName && !cleanId.contains("://"))
                candidateUrls.add("https://pixeldrain.com/api/filesystem/" + urlEncodePath(rootDirId + "/" + cleanId) + "?download");
        }

        // 2. Direct File ID endpoints (if cleanId is 8-char or standard ID)
        juce::String bareId = cleanId;
        if (bareId.startsWithIgnoreCase("file/"))
            bareId = bareId.substring(5);

        if (!bareId.contains("/") && !bareId.contains(" ") && bareId.isNotEmpty())
        {
            candidateUrls.add("https://pixeldrain.com/api/file/" + bareId + "?download");
            candidateUrls.add("https://pixeldrain.com/api/file/" + bareId);
            candidateUrls.add("https://pixeldrain.com/u/" + bareId + "?download");
        }

        // 3. Shared List endpoint
        if (target.kind == PixeldrainTarget::List && rootDirId.isNotEmpty())
        {
            candidateUrls.add("https://pixeldrain.com/api/list/" + rootDirId + "/" + urlEncodePath(cleanName) + "?download");
        }

        // 4. Standalone filesystem path endpoints
        if (cleanId.startsWith("/"))
        {
            candidateUrls.add("https://pixeldrain.com/api/filesystem" + urlEncodePath(cleanId) + "?download");
            candidateUrls.add("https://pixeldrain.com/api/filesystem" + urlEncodePath(cleanId));
        }
        else
        {
            candidateUrls.add("https://pixeldrain.com/api/filesystem/" + urlEncodePath(cleanId) + "?download");
            candidateUrls.add("https://pixeldrain.com/api/filesystem/" + urlEncodePath(cleanId));
        }

        if (cleanRel.isNotEmpty() && cleanRel != cleanId)
        {
            candidateUrls.add("https://pixeldrain.com/api/filesystem/" + urlEncodePath(cleanRel) + "?download");
        }
    }

    juce::String authHeader;
    if (target.kind == PixeldrainTarget::UserAccount && target.idOrKey.isNotEmpty())
    {
        authHeader = "Authorization: Basic " + juce::Base64::toBase64(":" + target.idOrKey);
    }

    // Ensure parent directory exists before writing
    destFile.getParentDirectory().createDirectory();

    bool success = false;

    for (const auto& downloadUrlStr : candidateUrls)
    {
        if (shouldExit() || safeThis == nullptr) break;

        juce::URL url(downloadUrlStr);
        auto stream = url.createInputStream(makeHttpOptions(authHeader, 20000, false));

        if (stream == nullptr && authHeader.isNotEmpty())
        {
            // Fallback retry without auth header if private account header failed
            stream = url.createInputStream(makeHttpOptions("", 20000, false));
        }

        if (stream != nullptr)
        {
            destFile.deleteFile();
            auto outStream = destFile.createOutputStream();
            if (outStream != nullptr)
            {
                int64_t streamLen = stream->getTotalLength();
                int64_t totalBytes = (streamLen > 0) ? streamLen : expectedSizeBytes;

                int64_t bytesWritten = 0;
                char buffer[16384];
                auto lastUiUpdateTime = juce::Time::getMillisecondCounter();
                double lastReportedProgress = -1.0;
                bool readError = false;

                while (!stream->isExhausted())
                {
                    if (shouldExit() || safeThis == nullptr)
                    {
                        readError = true;
                        break;
                    }

                    int bytesRead = stream->read(buffer, sizeof(buffer));
                    if (bytesRead < 0)
                    {
                        readError = true;
                        break;
                    }
                    if (bytesRead == 0)
                    {
                        if (stream->isExhausted())
                            break;
                        juce::Thread::sleep(5);
                        continue;
                    }

                    if (!outStream->write(buffer, static_cast<size_t>(bytesRead)))
                    {
                        readError = true;
                        break;
                    }
                    bytesWritten += bytesRead;

                    auto now = juce::Time::getMillisecondCounter();
                    double progress = (totalBytes > 0) ? juce::jlimit(0.0, 1.0, static_cast<double>(bytesWritten) / totalBytes) : 0.0;

                    // Throttle UI updates to at most once every 40ms or 1% progress change
                    if ((now - lastUiUpdateTime > 40 || (progress - lastReportedProgress) >= 0.01) && totalBytes > 0)
                    {
                        lastUiUpdateTime = now;
                        lastReportedProgress = progress;

                        juce::MessageManager::callAsync([safeThis, fileId, progress, bytesWritten, isPreview, fileName] {
                            if (safeThis != nullptr)
                            {
                                for (auto& f : safeThis->allRemoteFiles)
                                {
                                    if (f.id == fileId)
                                    {
                                        if (isPreview)
                                            f.previewProgress = progress;
                                        else
                                        {
                                            f.downloadProgress = progress;
                                            f.bytesDownloaded = bytesWritten;
                                        }
                                    }
                                }
                                for (auto& f : safeThis->displayedFiles)
                                {
                                    if (f.id == fileId)
                                    {
                                        if (isPreview)
                                            f.previewProgress = progress;
                                        else
                                        {
                                            f.downloadProgress = progress;
                                            f.bytesDownloaded = bytesWritten;
                                        }
                                    }
                                }
                                safeThis->tableBox.repaint();

                                if (isPreview)
                                {
                                    safeThis->statusLabel.setText("Streaming preview: " + fileName + " (" + juce::String(juce::roundToInt(progress * 100.0)) + "%)", juce::dontSendNotification);
                                }
                            }
                        });
                    }
                }
                outStream->flush();
                outStream.reset(); // close file before reading to check

                // Check for completion & integrity
                if (!readError && destFile.existsAsFile() && destFile.getSize() > 0)
                {
                    bool sizeOk = true;
                    if (streamLen > 0 && bytesWritten < streamLen * 0.95)
                    {
                        // Severed stream!
                        sizeOk = false;
                    }

                    if (sizeOk)
                    {
                        juce::FileInputStream checkStream(destFile);
                        if (checkStream.openedOk())
                        {
                            char firstChars[64] = {0};
                            int readBytes = checkStream.read(firstChars, 63);
                            juce::String startStr(firstChars, static_cast<size_t>(readBytes));
                            startStr = startStr.trim();

                            // Reject JSON / HTML error responses from Pixeldrain
                            if (!startStr.startsWith("{\"success\":false") &&
                                !startStr.startsWithIgnoreCase("<!DOCTYPE") &&
                                !startStr.startsWithIgnoreCase("<html") &&
                                !(startStr.startsWith("{") && (startStr.containsIgnoreCase("error") || startStr.containsIgnoreCase("message"))))
                            {
                                success = true;
                            }
                        }
                    }
                }

                if (success)
                {
                    // Final 100% progress update
                    juce::MessageManager::callAsync([safeThis, fileId, isPreview] {
                        if (safeThis != nullptr)
                        {
                            for (auto& f : safeThis->allRemoteFiles)
                            {
                                if (f.id == fileId)
                                {
                                    if (isPreview) f.previewProgress = 1.0;
                                    else f.downloadProgress = 1.0;
                                }
                            }
                            for (auto& f : safeThis->displayedFiles)
                            {
                                if (f.id == fileId)
                                {
                                    if (isPreview) f.previewProgress = 1.0;
                                    else f.downloadProgress = 1.0;
                                }
                            }
                            safeThis->tableBox.repaint();
                        }
                    });
                    break;
                }
                else
                {
                    destFile.deleteFile();
                    // Small polite delay between candidate URL attempts if an error was encountered
                    juce::Thread::sleep(100);
                }
            }
        }
    }

    if (!success && !shouldExit())
    {
        juce::MessageManager::callAsync([safeThis, fileName, isPreview] {
            if (safeThis != nullptr)
            {
                if (isPreview)
                    safeThis->statusLabel.setText("Failed to connect or stream preview: " + fileName, juce::dontSendNotification);
            }
        });
    }
    return success;
}

int LibrariesComponent::extractAudioFilesFromZip(const juce::File& zipFile,
                                               const juce::File& destinationFolder,
                                               juce::String& outStatus)
{
    if (!zipFile.existsAsFile())
    {
        outStatus = "Zip file not found: " + zipFile.getFullPathName();
        return 0;
    }

    juce::ZipFile zip(zipFile);
    int numEntries = zip.getNumEntries();
    if (numEntries <= 0)
    {
        outStatus = "Zip archive is empty: " + zipFile.getFileName();
        return 0;
    }

    bool allEntriesShareSingleFolder = true;
    juce::String rootDirName;
    for (int i = 0; i < numEntries; ++i)
    {
        const auto* entry = zip.getEntry(i);
        if (entry == nullptr) continue;
        juce::String fn = entry->filename.replace("\\", "/");
        if (fn.isEmpty() || fn.startsWith("/") || fn.contains("__MACOSX") || fn.startsWith("."))
            continue;

        if (!fn.contains("/"))
        {
            allEntriesShareSingleFolder = false;
            break;
        }
        juce::String topFolder = fn.upToFirstOccurrenceOf("/", false, false);
        if (rootDirName.isEmpty())
            rootDirName = topFolder;
        else if (rootDirName != topFolder)
        {
            allEntriesShareSingleFolder = false;
            break;
        }
    }

    juce::File extractBaseDir = (allEntriesShareSingleFolder && rootDirName.isNotEmpty())
                                ? destinationFolder
                                : destinationFolder.getChildFile(zipFile.getFileNameWithoutExtension());

    if (!extractBaseDir.exists())
        extractBaseDir.createDirectory();

    int extractedAudioCount = 0;
    for (int i = 0; i < numEntries; ++i)
    {
        const auto* entry = zip.getEntry(i);
        if (entry == nullptr) continue;

        juce::String entryPath = entry->filename.replace("\\", "/");

        // Skip macOS metadata and hidden files
        if (entryPath.contains("__MACOSX") || entryPath.startsWith(".") ||
            juce::File(entryPath).getFileName().startsWith("._") ||
            juce::File(entryPath).getFileName().startsWithIgnoreCase(".ds_store"))
            continue;

        if (isSupportedAudioFile(entryPath, ""))
        {
            auto result = zip.uncompressEntry(i, extractBaseDir, true);
            if (result.wasOk())
            {
                extractedAudioCount++;
            }
        }
    }

    outStatus = "Extracted " + juce::String(extractedAudioCount) + " audio file(s) from " + zipFile.getFileName();
    return extractedAudioCount;
}

void LibrariesComponent::handleDownloadFinished(const juce::String& fileId, const juce::File& destFile, bool success, bool isZip, int extractedCount, const juce::String& failReason)
{
    activeDownloadCount--;
    if (activeDownloadCount.load() < 0)
        activeDownloadCount.store(0);

    for (auto& f : allRemoteFiles)
    {
        if (f.id == fileId)
        {
            f.isDownloading = false;
            f.isQueued = false;
            if (success)
            {
                f.isDownloaded = true;
                f.isFailed = false;
                f.failReason = "";
                f.downloadProgress = 1.0;
                f.localPath = destFile.getFullPathName();
            }
            else
            {
                f.isFailed = true;
                f.failReason = failReason.isNotEmpty() ? failReason : "Failed";
                f.downloadProgress = 0.0;
            }
        }
    }

    for (auto& f : displayedFiles)
    {
        if (f.id == fileId)
        {
            f.isDownloading = false;
            f.isQueued = false;
            if (success)
            {
                f.isDownloaded = true;
                f.isFailed = false;
                f.failReason = "";
                f.downloadProgress = 1.0;
                f.localPath = destFile.getFullPathName();
            }
            else
            {
                f.isFailed = true;
                f.failReason = failReason.isNotEmpty() ? failReason : "Failed";
                f.downloadProgress = 0.0;
            }
        }
    }

    tableBox.repaint();

    if (success && isZip)
    {
        statusLabel.setText("Downloaded " + destFile.getFileName() + " & auto-extracted " + juce::String(extractedCount) + " audio file(s).", juce::dontSendNotification);
    }
    else if (success)
    {
        statusLabel.setText("Downloaded " + destFile.getFileName() + " to library.", juce::dontSendNotification);
    }
    else
    {
        statusLabel.setText("Failed to download " + destFile.getFileName() + ". Click Retry to try again.", juce::dontSendNotification);
    }

    checkAndTriggerBatchScan();
}

void LibrariesComponent::checkAndTriggerBatchScan()
{
    bool anyDownloading = false;
    bool anyQueued = false;
    for (const auto& f : allRemoteFiles)
    {
        if (f.isDownloading) anyDownloading = true;
        if (f.isQueued) anyQueued = true;
    }

    bool queueEmpty = true;
    {
        const juce::ScopedLock sl(downloadQueueLock);
        queueEmpty = downloadQueue.empty();
    }

    if (!anyDownloading && !anyQueued && queueEmpty && activeDownloadCount.load() <= 0)
    {
        downloadAllWavsButton.setButtonText("Download All");
        downloadAllWavsButton.removeColour(juce::TextButton::buttonColourId);
        downloadAllWavsButton.removeColour(juce::TextButton::textColourOffId);

        int failedCount = 0;
        for (const auto& f : allRemoteFiles)
        {
            if (f.isFailed) failedCount++;
        }

        juce::File downloadFolder(dbManager.getDownloadFolder());
        if (downloadFolder.exists())
        {
            dbManager.addScanFolder(downloadFolder.getFullPathName());
            libraryScanner.startScan({ downloadFolder.getFullPathName() });

            if (failedCount > 0)
                statusLabel.setText("Downloads finished with " + juce::String(failedCount) + " error(s). Library rescan started.", juce::dontSendNotification);
            else
                statusLabel.setText("All downloads finished. Library rescan started.", juce::dontSendNotification);
        }
        else
        {
            statusLabel.setText("All downloads finished.", juce::dontSendNotification);
        }
    }
}

void LibrariesComponent::previewFile(int displayedIndex)
{
    if (displayedIndex < 0 || displayedIndex >= static_cast<int>(displayedFiles.size()))
        return;

    auto& targetItem = displayedFiles[static_cast<size_t>(displayedIndex)];
    if (targetItem.isDownloading || targetItem.isPreviewing)
        return;

    if (targetItem.isZip)
    {
        statusLabel.setText("ZIP archive: '" + targetItem.name + "'. Click Download to auto-extract audio files.", juce::dontSendNotification);
        return;
    }

    if (targetItem.isDownloaded && juce::File(targetItem.localPath).existsAsFile())
    {
        audioEngine.loadFile(juce::File(targetItem.localPath), true);
        return;
    }

    // Replace path separators in the file ID to prevent creating directories or invalid filenames in the temp folder
    juce::String safeId = targetItem.id.replace("/", "_").replace("\\", "_");
    juce::String ext = juce::File(targetItem.name).getFileExtension();
    if (ext.isEmpty()) ext = ".wav";

    juce::File previewFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                .getChildFile("owmb_preview_" + safeId + ext);

    if (previewFile.existsAsFile() && previewFile.getSize() > 128)
    {
        std::unique_ptr<juce::AudioFormatReader> reader(audioEngine.getFormatManager().createReaderFor(previewFile));
        if (reader != nullptr && reader->lengthInSamples > 0)
        {
            targetItem.previewPath = previewFile.getFullPathName();
            audioEngine.loadFile(previewFile, true);
            statusLabel.setText("Playing preview: " + targetItem.name, juce::dontSendNotification);
            return;
        }
        else
        {
            // Delete corrupt or incomplete cached preview file so it can be cleanly re-downloaded
            previewFile.deleteFile();
        }
    }

    statusLabel.setText("Streaming preview: " + targetItem.name + "...", juce::dontSendNotification);

    targetItem.isPreviewing = true;
    targetItem.previewProgress = 0.0;

    for (auto& f : allRemoteFiles)
    {
        if (f.id == targetItem.id)
        {
            f.isPreviewing = true;
            f.previewProgress = 0.0;
        }
    }

    tableBox.repaint();

    juce::String fileId = targetItem.id;
    juce::String fileName = targetItem.name;
    int64_t sizeBytes = targetItem.sizeBytes;
    juce::String inputStr = apiKeyEditor.getText().trim();

    juce::Component::SafePointer<LibrariesComponent> safeThis(this);

    juce::Thread::launch([safeThis, fileId, fileName, sizeBytes, inputStr, previewFile] {
        if (safeThis == nullptr) return;

        auto shouldExit = [safeThis] { return safeThis == nullptr; };
        bool success = downloadFileSync(fileId, fileName, sizeBytes, inputStr, previewFile, shouldExit, safeThis, true);

        juce::MessageManager::callAsync([safeThis, fileId, previewFile, success] {
            if (safeThis != nullptr)
            {
                safeThis->handlePreviewFinished(fileId, previewFile, success);
            }
        });
    });
}

void LibrariesComponent::handlePreviewFinished(const juce::String& fileId, const juce::File& previewFile, bool success)
{
    for (auto& f : allRemoteFiles)
    {
        if (f.id == fileId)
        {
            f.isPreviewing = false;
            if (success && previewFile.existsAsFile() && previewFile.getSize() > 128)
            {
                std::unique_ptr<juce::AudioFormatReader> reader(audioEngine.getFormatManager().createReaderFor(previewFile));
                if (reader != nullptr && reader->lengthInSamples > 0)
                {
                    f.previewPath = previewFile.getFullPathName();
                    audioEngine.loadFile(previewFile, true);
                    statusLabel.setText("Playing preview: " + f.name, juce::dontSendNotification);
                }
                else
                {
                    previewFile.deleteFile();
                    statusLabel.setText("Preview file format error: " + f.name, juce::dontSendNotification);
                }
            }
            else
            {
                if (previewFile.existsAsFile())
                    previewFile.deleteFile();

                statusLabel.setText("Preview stream failed for: " + f.name, juce::dontSendNotification);
            }
        }
    }
    for (auto& f : displayedFiles)
    {
        if (f.id == fileId)
        {
            f.isPreviewing = false;
            if (success && previewFile.existsAsFile() && previewFile.getSize() > 128)
            {
                f.previewPath = previewFile.getFullPathName();
            }
        }
    }
    tableBox.repaint();
}

LibrariesComponent::SequentialDownloader::SequentialDownloader(LibrariesComponent& owner)
    : juce::Thread("SequentialDownloader"), owner(owner)
{
}

LibrariesComponent::SequentialDownloader::~SequentialDownloader()
{
    stopThread(250);
}

void LibrariesComponent::SequentialDownloader::run()
{
    juce::Component::SafePointer<LibrariesComponent> safeOwner(&owner);

    while (!threadShouldExit())
    {
        if (safeOwner == nullptr || safeOwner->cancelRequested.load())
            return;

        QueuedDownload nextJob;
        {
            const juce::ScopedLock sl (safeOwner->downloadQueueLock);
            if (safeOwner->downloadQueue.empty())
            {
                juce::MessageManager::callAsync([safeOwner] {
                    if (safeOwner != nullptr)
                    {
                        safeOwner->checkAndTriggerBatchScan();
                    }
                });
                break;
            }
            nextJob = safeOwner->downloadQueue.front();
            safeOwner->downloadQueue.erase(safeOwner->downloadQueue.begin());
        }

        if (threadShouldExit() || safeOwner == nullptr || safeOwner->cancelRequested.load())
            return;

        // Transition file from Queued to Actively Downloading
        juce::MessageManager::callAsync([safeOwner, nextJob] {
            if (safeOwner != nullptr)
            {
                for (auto& f : safeOwner->allRemoteFiles)
                {
                    if (f.id == nextJob.fileId)
                    {
                        f.isQueued = false;
                        f.isDownloading = true;
                        f.downloadProgress = 0.0;
                    }
                }
                for (auto& f : safeOwner->displayedFiles)
                {
                    if (f.id == nextJob.fileId)
                    {
                        f.isQueued = false;
                        f.isDownloading = true;
                        f.downloadProgress = 0.0;
                    }
                }
                safeOwner->tableBox.repaint();

                int currentFileIndex = safeOwner->completedBatchCount + 1;
                safeOwner->statusLabel.setText("Downloading (" + juce::String(currentFileIndex) + "/" +
                                               juce::String(safeOwner->totalBatchCount) + "): " +
                                               nextJob.fileName, juce::dontSendNotification);
            }
        });

        // Perform the download with retries
        juce::File destFolder(safeOwner->dbManager.getDownloadFolder());
        if (!destFolder.exists())
            destFolder.createDirectory();

        juce::String relPath = nextJob.relativePath.isNotEmpty() ? nextJob.relativePath : nextJob.fileName;
        juce::File destFile = destFolder.getChildFile(relPath);
        destFile.getParentDirectory().createDirectory();

        juce::String apiKey = safeOwner->dbManager.getPixeldrainApiKey();

        bool success = false;
        juce::String errorReason;

        // Retry loop (up to 3 attempts with exponential backoff)
        const int maxAttempts = 3;
        for (int attempt = 1; attempt <= maxAttempts; ++attempt)
        {
            if (threadShouldExit() || safeOwner == nullptr || safeOwner->cancelRequested.load())
                break;

            if (attempt > 1)
            {
                juce::MessageManager::callAsync([safeOwner, nextJob, attempt] {
                    if (safeOwner != nullptr)
                        safeOwner->statusLabel.setText("Retrying (" + juce::String(attempt) + "/" + juce::String(maxAttempts) + "): " + nextJob.fileName, juce::dontSendNotification);
                });
                juce::Thread::sleep(500 * attempt);
            }

            success = downloadFileSync(nextJob.fileId, nextJob.fileName, nextJob.sizeBytes, apiKey, destFile,
                                       [this, safeOwner] {
                                            return threadShouldExit() || safeOwner == nullptr || safeOwner->cancelRequested.load();
                                       },
                                       safeOwner);

            if (success)
                break;
        }

        if (threadShouldExit() || safeOwner == nullptr || safeOwner->cancelRequested.load())
            return;

        int extractedCount = 0;
        bool isZip = nextJob.isZip || destFile.getFileExtension().equalsIgnoreCase(".zip");
        if (success && isZip && destFile.existsAsFile())
        {
            juce::MessageManager::callAsync([safeOwner, nextJob] {
                if (safeOwner != nullptr)
                    safeOwner->statusLabel.setText("Extracting audio files from " + nextJob.fileName + "...", juce::dontSendNotification);
            });

            juce::String statusMsg;
            extractedCount = extractAudioFilesFromZip(destFile, destFile.getParentDirectory(), statusMsg);
        }

        if (!success)
            errorReason = "Download Failed";

        juce::MessageManager::callAsync([safeOwner, nextJob, destFile, success, isZip, extractedCount, errorReason] {
            if (safeOwner != nullptr)
            {
                safeOwner->completedBatchCount++;
                safeOwner->handleDownloadFinished(nextJob.fileId, destFile, success, isZip, extractedCount, errorReason);
            }
        });

        // Polite delay between sequential downloads to prevent Pixeldrain API 429 rate limiting
        juce::Thread::sleep(200);
    }
}


void LibrariesComponent::lookAndFeelChanged()
{
    juce::Image logoImage;
#if defined(JUCE_BINARYDATA_H_INCLUDED) || __has_include(<JuceHeader.h>)
    logoImage = juce::ImageFileFormat::loadFrom(BinaryData::mainpixeldrainlogo_cropped_png, static_cast<size_t>(BinaryData::mainpixeldrainlogo_cropped_pngSize));
#endif

    if (logoImage.isNull())
    {
        juce::File logoFile = juce::File::getCurrentWorkingDirectory().getChildFile("mainpixeldrainlogo_cropped.png");
        if (!logoFile.existsAsFile())
            logoFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile("mainpixeldrainlogo_cropped.png");
        if (!logoFile.existsAsFile())
            logoFile = juce::File::getCurrentWorkingDirectory().getChildFile("mainpixeldrainlogo.png");
        if (logoFile.existsAsFile())
            logoImage = juce::ImageFileFormat::loadFrom(logoFile);
    }

    if (!logoImage.isNull())
    {
        pixeldrainLogoComponent.setVisible(true);
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
            pixeldrainLogoComponent.setImage(inverted, juce::RectanglePlacement::xRight | juce::RectanglePlacement::yMid | juce::RectanglePlacement::onlyReduceInSize);
        }
        else
        {
            pixeldrainLogoComponent.setImage(logoImage, juce::RectanglePlacement::xRight | juce::RectanglePlacement::yMid | juce::RectanglePlacement::onlyReduceInSize);
        }
    }
    else
    {
        pixeldrainLogoComponent.setVisible(false);
    }

    // Refresh table and tree colours when LookAndFeel changes
    tableBox.setColour(juce::ListBox::backgroundColourId, OpenWavLookAndFeel::bgDark);
    tableBox.setOutlineThickness(1);
    tableBox.setColour(juce::ListBox::outlineColourId, OpenWavLookAndFeel::borderColour);
    tableBox.repaint();

    folderTreeView.setColour(juce::TreeView::backgroundColourId, OpenWavLookAndFeel::bgDark.withMultipliedBrightness(0.6f));
    folderTreeView.setColour(juce::TreeView::linesColourId, OpenWavLookAndFeel::borderColour.withAlpha(0.35f));
    folderTreeView.repaint();

    foldersHeaderLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    foldersHeaderLabel.setColour(juce::Label::backgroundColourId, OpenWavLookAndFeel::bgDark.withMultipliedBrightness(0.7f));
    foldersHeaderLabel.setColour(juce::Label::outlineColourId, OpenWavLookAndFeel::borderColour);

    breadcrumbLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textPrimary);
    breadcrumbLabel.setColour(juce::Label::backgroundColourId, OpenWavLookAndFeel::bgDark.withMultipliedBrightness(0.7f));
    breadcrumbLabel.setColour(juce::Label::outlineColourId, OpenWavLookAndFeel::borderColour);

    includeSubfoldersToggle.setColour(juce::ToggleButton::textColourId, OpenWavLookAndFeel::textSecondary);
    includeSubfoldersToggle.setColour(juce::ToggleButton::tickColourId, OpenWavLookAndFeel::accentCyan);

    // Update labels and text fields with dynamic colors
    apiKeyLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textPrimary);
    apiKeyEditor.setTextToShowWhenEmpty("Enter API Key or Public Hotlink (e.g. /u/id or /l/id)...", OpenWavLookAndFeel::textSecondary);
    statusLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    searchLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textPrimary);
    searchEditor.setTextToShowWhenEmpty("Filter remote files by name...", OpenWavLookAndFeel::textSecondary);
    saveDirLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
}

bool LibrariesComponent::mayDragToExternalWindows() const
{
    return true;
}

} // namespace openwav
