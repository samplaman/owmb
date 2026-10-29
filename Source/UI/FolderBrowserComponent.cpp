#include "FolderBrowserComponent.h"
#include "OpenWavLookAndFeel.h"
#include <algorithm>
#include <cmath>

namespace openwav
{

FolderBrowserComponent::FolderBrowserComponent(TagDatabaseManager& db,
                                               LibraryScanner& scanner,
                                               AudioEngine& engine)
    : dbManager(db), libraryScanner(scanner), audioEngine(engine)
{
    dbManager.addListener(this);
    audioEngine.addListener(this);

    setOpaque(true);

    summaryLabel.setFont(juce::Font(juce::FontOptions(11.5f).withStyle("Bold")));
    summaryLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    summaryLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(summaryLabel);

    expandAllButton.setTooltip("Expand all scanned folders and subfolders");
    expandAllButton.onClick = [this] { expandAllFolders(); };
    addAndMakeVisible(expandAllButton);

    collapseAllButton.setTooltip("Collapse all folders back to Scanned Folders overview");
    collapseAllButton.onClick = [this] { collapseAllFolders(); };
    addAndMakeVisible(collapseAllButton);

    emptyStateAddButton.onClick = [this] {
        if (onAddFolderRequested)
            onAddFolderRequested();
    };
    addChildComponent(emptyStateAddButton);

    listBox.setModel(this);
    listBox.setRowHeight(32);
    listBox.setMultipleSelectionEnabled(false);
    listBox.setWantsKeyboardFocus(true);
    listBox.setOpaque(true);
    listBox.setOutlineThickness(0);
    listBox.setColour(juce::ListBox::backgroundColourId, OpenWavLookAndFeel::bgDark);

    listBox.onKeyNavigation = [this](const juce::KeyPress& key) {
        return handleKeyNavigation(key);
    };

    if (auto* vp = listBox.getViewport())
    {
        vp->setOpaque(true);
        vp->setScrollBarsShown(true, false);
        vp->setRepaintsOnMouseActivity(false);
    }

    addAndMakeVisible(listBox);

    cachedCurrentFilePath = audioEngine.getCurrentFile().getFullPathName();
    cachedIsPlaying = audioEngine.isPlaying();

    buildIconCache();
    currentScanFolders = dbManager.getScanFolders();
    rebuildTree();
}

FolderBrowserComponent::~FolderBrowserComponent()
{
    dbManager.removeListener(this);
    audioEngine.removeListener(this);
    listBox.setModel(nullptr);
}

void FolderBrowserComponent::lookAndFeelChanged()
{
    listBox.setColour(juce::ListBox::backgroundColourId, OpenWavLookAndFeel::bgDark);
    summaryLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    buildIconCache();
    repaint();
    listBox.repaint();
}

void FolderBrowserComponent::buildIconCache()
{
    const float scale = 2.0f;

    // Play Icon
    {
        int w = 24, h = 24;
        playIconImage = juce::Image(juce::Image::ARGB, static_cast<int>(w * scale), static_cast<int>(h * scale), true);
        juce::Graphics g(playIconImage);
        g.addTransform(juce::AffineTransform::scale(scale));
        g.setColour(OpenWavLookAndFeel::textSecondary);
        juce::Path p;
        p.addTriangle(w * 0.38f, h * 0.28f,
                      w * 0.38f, h * 0.72f,
                      w * 0.68f, h * 0.50f);
        g.fillPath(p);
    }

    // Pause Icon
    {
        int w = 24, h = 24;
        pauseIconImage = juce::Image(juce::Image::ARGB, static_cast<int>(w * scale), static_cast<int>(h * scale), true);
        juce::Graphics g(pauseIconImage);
        g.addTransform(juce::AffineTransform::scale(scale));
        g.setColour(OpenWavLookAndFeel::accentCyan);
        juce::Path p;
        p.addRectangle(w * 0.35f, h * 0.28f, 3.0f, h * 0.44f);
        p.addRectangle(w * 0.55f, h * 0.28f, 3.0f, h * 0.44f);
        g.fillPath(p);
    }

    // Favorite Heart Active
    {
        int w = 18, h = 18;
        heartActiveImage = juce::Image(juce::Image::ARGB, static_cast<int>(w * scale), static_cast<int>(h * scale), true);
        juce::Graphics g(heartActiveImage);
        g.addTransform(juce::AffineTransform::scale(scale));

        juce::Path heart;
        float x = 2.0f, y = 2.0f, hw = 14.0f, hh = 14.0f;
        heart.startNewSubPath(x + hw * 0.5f, y + hh * 0.85f);
        heart.cubicTo(x + hw * 0.1f, y + hh * 0.5f,  x,              y + hh * 0.2f,  x + hw * 0.25f, y + hh * 0.05f);
        heart.cubicTo(x + hw * 0.45f, y - hh * 0.05f, x + hw * 0.5f,  y + hh * 0.2f,  x + hw * 0.5f,  y + hh * 0.25f);
        heart.cubicTo(x + hw * 0.5f,  y + hh * 0.2f,  x + hw * 0.55f, y - hh * 0.05f, x + hw * 0.75f, y + hh * 0.05f);
        heart.cubicTo(x + hw,         y + hh * 0.2f,  x + hw * 0.9f,  y + hh * 0.5f,  x + hw * 0.5f,  y + hh * 0.85f);
        heart.closeSubPath();

        g.setColour(OpenWavLookAndFeel::favoriteRed);
        g.fillPath(heart);
    }

    // Favorite Heart Inactive
    {
        int w = 18, h = 18;
        heartInactiveImage = juce::Image(juce::Image::ARGB, static_cast<int>(w * scale), static_cast<int>(h * scale), true);
        juce::Graphics g(heartInactiveImage);
        g.addTransform(juce::AffineTransform::scale(scale));

        juce::Path heart;
        float x = 2.0f, y = 2.0f, hw = 14.0f, hh = 14.0f;
        heart.startNewSubPath(x + hw * 0.5f, y + hh * 0.85f);
        heart.cubicTo(x + hw * 0.1f, y + hh * 0.5f,  x,              y + hh * 0.2f,  x + hw * 0.25f, y + hh * 0.05f);
        heart.cubicTo(x + hw * 0.45f, y - hh * 0.05f, x + hw * 0.5f,  y + hh * 0.2f,  x + hw * 0.5f,  y + hh * 0.25f);
        heart.cubicTo(x + hw * 0.5f,  y + hh * 0.2f,  x + hw * 0.55f, y - hh * 0.05f, x + hw * 0.75f, y + hh * 0.05f);
        heart.cubicTo(x + hw,         y + hh * 0.2f,  x + hw * 0.9f,  y + hh * 0.5f,  x + hw * 0.5f,  y + hh * 0.85f);
        heart.closeSubPath();

        g.setColour(OpenWavLookAndFeel::textSecondary.withAlpha(0.35f));
        g.strokePath(heart, juce::PathStrokeType(1.2f));
    }

    // Rating Stars (0..5)
    {
        int totalW = 64;
        int totalH = 18;
        float starSize = 10.0f;
        float starSpacing = 3.5f;

        juce::Path baseStar;
        for (int i = 0; i < 5; ++i)
        {
            float outerAngle = i * juce::MathConstants<float>::twoPi / 5.0f - juce::MathConstants<float>::halfPi;
            float innerAngle = outerAngle + juce::MathConstants<float>::pi / 5.0f;
            float rOuter = starSize * 0.5f;
            float rInner = rOuter * 0.42f;

            float ox = std::cos(outerAngle) * rOuter;
            float oy = std::sin(outerAngle) * rOuter;
            float ix = std::cos(innerAngle) * rInner;
            float iy = std::sin(innerAngle) * rInner;

            if (i == 0) baseStar.startNewSubPath(ox, oy);
            else baseStar.lineTo(ox, oy);
            baseStar.lineTo(ix, iy);
        }
        baseStar.closeSubPath();

        for (int r = 0; r <= 5; ++r)
        {
            starRatingImages[r] = juce::Image(juce::Image::ARGB, static_cast<int>(totalW * scale), static_cast<int>(totalH * scale), true);
            juce::Graphics g(starRatingImages[r]);
            g.addTransform(juce::AffineTransform::scale(scale));

            float cy = totalH * 0.5f;
            float startX = 0.0f;

            for (int s = 1; s <= 5; ++s)
            {
                float sx = startX + (s - 1) * (starSize + starSpacing) + starSize * 0.5f;
                juce::Path currentStar = baseStar;
                currentStar.applyTransform(juce::AffineTransform::translation(sx, cy));

                if (s <= r)
                {
                    g.setColour(juce::Colour::fromRGB(255, 200, 45));
                    g.fillPath(currentStar);
                }
                else
                {
                    g.setColour(OpenWavLookAndFeel::textSecondary.withAlpha(0.25f));
                    g.strokePath(currentStar, juce::PathStrokeType(1.0f));
                }
            }
        }
    }
}

FolderBrowserComponent::ColumnLayout FolderBrowserComponent::computeColumns(int totalWidth)
{
    ColumnLayout cols;
    int w = std::max(420, totalWidth);

    cols.favW = 40;
    cols.ratingW = 76;
    cols.srW = juce::jlimit(88, 120, static_cast<int>(w * 0.105));
    cols.fmtW = juce::jlimit(56, 74, static_cast<int>(w * 0.062));
    cols.durW = juce::jlimit(62, 82, static_cast<int>(w * 0.068));
    cols.tagsW = juce::jlimit(130, 360, static_cast<int>(w * 0.255));

    int fixedRight = cols.tagsW + cols.durW + cols.fmtW + cols.srW + cols.ratingW + cols.favW;
    int minNameW = 240;
    if (w - fixedRight < minNameW)
    {
        int deficit = minNameW - (w - fixedRight);
        int shrinkTags = std::min(deficit, std::max(0, cols.tagsW - 90));
        cols.tagsW -= shrinkTags;
        fixedRight = cols.tagsW + cols.durW + cols.fmtW + cols.srW + cols.ratingW + cols.favW;
    }

    cols.nameX = 0;
    cols.nameW = std::max(140, w - fixedRight);
    cols.tagsX = cols.nameX + cols.nameW;
    cols.durX = cols.tagsX + cols.tagsW;
    cols.fmtX = cols.durX + cols.durW;
    cols.srX = cols.fmtX + cols.fmtW;
    cols.ratingX = cols.srX + cols.srW;
    cols.favX = cols.ratingX + cols.ratingW;

    return cols;
}

juce::String FolderBrowserComponent::normalizePathKey(const juce::String& rawPath)
{
    juce::String p = rawPath.trim().replaceCharacter('\\', '/');
    while (p.length() > 1 && p.endsWithChar('/'))
    {
        if (p.length() == 3 && p[1] == ':')
            break;
        p = p.dropLastCharacters(1);
    }
#if JUCE_WINDOWS || JUCE_MAC
    return p.toLowerCase();
#else
    return p;
#endif
}

juce::String FolderBrowserComponent::extractParentDirectory(const juce::String& filePath)
{
    int lastBackslash = filePath.lastIndexOfChar('\\');
    int lastSlash = filePath.lastIndexOfChar('/');
    int idx = std::max(lastBackslash, lastSlash);
    if (idx <= 0)
        return {};
    if (idx == 2 && filePath[1] == ':')
        return filePath.substring(0, 3);
    return filePath.substring(0, idx);
}

juce::String FolderBrowserComponent::formatDuration(double seconds)
{
    if (seconds <= 0.0) return "0:00.0";
    int mins = static_cast<int>(seconds) / 60;
    double remSecs = seconds - mins * 60.0;
    juce::String secStr = (remSecs < 10.0 ? "0" : "") + juce::String(remSecs, 1);
    return juce::String(mins) + ":" + secStr;
}

void FolderBrowserComponent::paint(juce::Graphics& g)
{
    g.fillAll(OpenWavLookAndFeel::bgDark);

    // 1. Top Toolbar Background
    auto toolbarBounds = juce::Rectangle<int>(0, 0, getWidth(), TopToolbarHeight);
    g.setColour(OpenWavLookAndFeel::bgHeader);
    g.fillRect(toolbarBounds);

    // Draw small cyan folder-tree icon on left of toolbar
    {
        auto iconRect = juce::Rectangle<float>(12.0f, 10.0f, 16.0f, 16.0f);
        juce::Path folderPath;
        folderPath.startNewSubPath(iconRect.getX(), iconRect.getY() + 3.0f);
        folderPath.lineTo(iconRect.getX() + 6.0f, iconRect.getY() + 3.0f);
        folderPath.lineTo(iconRect.getX() + 8.0f, iconRect.getY() + 5.2f);
        folderPath.lineTo(iconRect.getRight(), iconRect.getY() + 5.2f);
        folderPath.lineTo(iconRect.getRight(), iconRect.getBottom() - 1.5f);
        folderPath.lineTo(iconRect.getX(), iconRect.getBottom() - 1.5f);
        folderPath.closeSubPath();

        g.setColour(OpenWavLookAndFeel::accentCyan);
        g.strokePath(folderPath, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    g.setColour(OpenWavLookAndFeel::borderColour);
    g.fillRect(0, TopToolbarHeight - 1, getWidth(), 1);

    // 2. Column Header Bar
    auto headerBounds = juce::Rectangle<int>(0, TopToolbarHeight, getWidth(), ColumnHeaderHeight);
    g.setColour(OpenWavLookAndFeel::bgHeader.darker(0.12f));
    g.fillRect(headerBounds);

    int rowContentW = getWidth();
    if (auto* vp = listBox.getViewport())
    {
        int vw = vp->getMaximumVisibleWidth();
        if (vw > 100)
            rowContentW = vw;
    }

    auto cols = computeColumns(rowContentW);
    g.setFont(headerColFont);
    g.setColour(OpenWavLookAndFeel::textSecondary);

    auto drawHeaderCell = [&](int x, int w, const juce::String& title, juce::Justification just = juce::Justification::centredLeft) {
        g.setColour(OpenWavLookAndFeel::textSecondary);
        g.drawText(title, juce::Rectangle<int>(x + 8, TopToolbarHeight, std::max(0, w - 12), ColumnHeaderHeight), just, true);
        g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.65f));
        g.drawVerticalLine(x + w - 1, static_cast<float>(TopToolbarHeight), static_cast<float>(TopToolbarHeight + ColumnHeaderHeight));
    };

    drawHeaderCell(cols.nameX + 4, cols.nameW - 4, "SCANNED FOLDER / FILE NAME");
    drawHeaderCell(cols.tagsX, cols.tagsW, "TAGS");
    drawHeaderCell(cols.durX, cols.durW, "DURATION");
    drawHeaderCell(cols.fmtX, cols.fmtW, "FORMAT");
    drawHeaderCell(cols.srX, cols.srW, "SAMPLE RATE");
    drawHeaderCell(cols.ratingX, cols.ratingW, "RATING", juce::Justification::centred);
    g.setColour(OpenWavLookAndFeel::textSecondary);
    g.drawText("FAV", juce::Rectangle<int>(cols.favX, TopToolbarHeight, cols.favW, ColumnHeaderHeight), juce::Justification::centred, true);

    g.setColour(OpenWavLookAndFeel::borderColour);
    g.fillRect(0, TopToolbarHeight + ColumnHeaderHeight - 1, getWidth(), 1);

    // 3. Empty State Overlay (when zero scanned folders & zero items)
    if (rootFolders.empty())
    {
        auto emptyArea = getLocalBounds().withTrimmedTop(TopToolbarHeight + ColumnHeaderHeight);
        auto card = emptyArea.withSizeKeepingCentre(std::min(440, emptyArea.getWidth() - 40), 150).toFloat();

        g.setColour(OpenWavLookAndFeel::bgCard);
        g.fillRoundedRectangle(card, 8.0f);
        g.setColour(OpenWavLookAndFeel::borderColour);
        g.drawRoundedRectangle(card, 8.0f, 1.0f);

        g.setColour(OpenWavLookAndFeel::textPrimary);
        g.setFont(juce::Font(juce::FontOptions(15.0f).withStyle("Bold")));
        g.drawText("No Scanned Folders Yet",
                   card.removeFromTop(48.0f).withTrimmedTop(14.0f).toNearestInt(),
                   juce::Justification::centred, true);

        g.setColour(OpenWavLookAndFeel::textSecondary);
        g.setFont(juce::Font(juce::FontOptions(12.5f)));
        g.drawText("Add a sample folder to browse your scanned folders and subfolders here.",
                   card.removeFromTop(34.0f).reduced(20.0f, 0.0f).toNearestInt(),
                   juce::Justification::centred, true);
    }
}

void FolderBrowserComponent::resized()
{
    auto area = getLocalBounds();

    auto topBar = area.removeFromTop(TopToolbarHeight).reduced(10, 4);
    collapseAllButton.setBounds(topBar.removeFromRight(92));
    topBar.removeFromRight(6);
    expandAllButton.setBounds(topBar.removeFromRight(88));
    topBar.removeFromRight(10);
    topBar.removeFromLeft(22); // space for toolbar folder icon
    summaryLabel.setBounds(topBar);

    area.removeFromTop(ColumnHeaderHeight);

    if (rootFolders.empty())
    {
        listBox.setVisible(false);
        emptyStateAddButton.setVisible(true);
        auto card = area.withSizeKeepingCentre(std::min(440, area.getWidth() - 40), 150);
        emptyStateAddButton.setBounds(card.getCentreX() - 64, card.getBottom() - 46, 128, 30);
    }
    else
    {
        emptyStateAddButton.setVisible(false);
        listBox.setVisible(true);
        listBox.setBounds(area);
    }
}

void FolderBrowserComponent::setFilteredItems(const std::vector<MediaItem>& items,
                                              const juce::String& activeSearchQuery,
                                              bool favoritesOnly)
{
    currentItems = items;
    currentFavOnly = favoritesOnly;
    currentScanFolders = dbManager.getScanFolders();

    juce::String trimmedSearch = activeSearchQuery.trim();
    bool searchIsActive = trimmedSearch.isNotEmpty();

    if (searchIsActive && !wasInSearchAutoExpand)
    {
        preSearchExpandedKeys = expandedFolderKeys;
        wasInSearchAutoExpand = true;
    }
    else if (!searchIsActive && wasInSearchAutoExpand)
    {
        expandedFolderKeys = preSearchExpandedKeys;
        wasInSearchAutoExpand = false;
    }

    bool searchChanged = (trimmedSearch != lastSearchQuery);
    lastSearchQuery = trimmedSearch;

    rebuildTree();

    // When user types a search query with a manageable result set, auto-expand matching folders
    if (searchIsActive && searchChanged && !currentItems.empty() && currentItems.size() <= 400)
    {
        for (auto& root : rootFolders)
        {
            if (root->totalSampleCount > 0)
                setSubtreeExpanded(root.get(), true);
        }
        rebuildVisibleRows();
    }

    resized();
    repaint();
}

void FolderBrowserComponent::rebuildTree()
{
    rootFolders.clear();

    // 1. Create root nodes for every registered Scanned Folder
    for (const auto& scanPath : currentScanFolders)
    {
        auto root = std::make_unique<FolderNode>();
        root->fullPath = scanPath;
        root->normalizedKey = normalizePathKey(scanPath);

        juce::File f(scanPath);
        root->displayName = f.getFileName();
        if (root->displayName.isEmpty())
            root->displayName = scanPath;

        root->subtitlePath = scanPath;
        root->depth = 0;
        root->isScannedRoot = true;
        rootFolders.push_back(std::move(root));
    }

    std::unique_ptr<FolderNode> otherRoot;

    // 2. Bucket each filtered MediaItem into its root Scanned Folder and build subfolder tree
    for (size_t idx = 0; idx < currentItems.size(); ++idx)
    {
        const auto& item = currentItems[idx];
        juce::String parentDir = extractParentDirectory(item.filePath);
        juce::String parentKey = normalizePathKey(parentDir);

        FolderNode* matchedRoot = nullptr;
        size_t bestRootKeyLen = 0;

        for (auto& root : rootFolders)
        {
            const auto& rk = root->normalizedKey;
            if (rk.isEmpty())
                continue;

            bool isExact = (parentKey == rk);
            bool isChild = (!isExact && parentKey.length() > rk.length() &&
                            parentKey.startsWith(rk) &&
                            (rk.endsWithChar('/') || parentKey[rk.length()] == '/'));

            if ((isExact || isChild) && rk.length() > bestRootKeyLen)
            {
                matchedRoot = root.get();
                bestRootKeyLen = rk.length();
            }
        }

        if (matchedRoot != nullptr)
        {
            // Walk relative segments beneath matchedRoot
            juce::String cleanParent = parentDir.replaceCharacter('\\', '/');
            juce::String cleanRoot = matchedRoot->fullPath.trim().replaceCharacter('\\', '/');
            while (cleanRoot.length() > 1 && cleanRoot.endsWithChar('/'))
            {
                if (cleanRoot.length() == 3 && cleanRoot[1] == ':')
                    break;
                cleanRoot = cleanRoot.dropLastCharacters(1);
            }

            juce::String relPath;
            if (cleanParent.length() > cleanRoot.length())
                relPath = cleanParent.substring(cleanRoot.length());

            while (relPath.startsWithChar('/'))
                relPath = relPath.substring(1);

            FolderNode* cur = matchedRoot;
            if (relPath.isNotEmpty())
            {
                juce::StringArray segments;
                segments.addTokens(relPath, "/", "");
                segments.removeEmptyStrings();

                juce::String runningDiskPath = matchedRoot->fullPath;
                while (runningDiskPath.endsWithChar('\\') || runningDiskPath.endsWithChar('/'))
                    runningDiskPath = runningDiskPath.dropLastCharacters(1);

                const juce::String sep = juce::File::getSeparatorString();

                for (const auto& seg : segments)
                {
                    runningDiskPath += sep + seg;
                    juce::String segKey =
#if JUCE_WINDOWS || JUCE_MAC
                        seg.toLowerCase();
#else
                        seg;
#endif
                    auto it = cur->childLookup.find(segKey);
                    if (it != cur->childLookup.end())
                    {
                        cur = it->second;
                    }
                    else
                    {
                        auto child = std::make_unique<FolderNode>();
                        child->fullPath = runningDiskPath;
                        child->normalizedKey = cur->normalizedKey + "/" + segKey;
                        child->displayName = seg;
                        child->depth = cur->depth + 1;
                        child->parent = cur;

                        FolderNode* rawChild = child.get();
                        cur->childLookup[segKey] = rawChild;
                        cur->subfolders.push_back(std::move(child));
                        cur = rawChild;
                    }
                }
            }

            cur->directItemIndices.push_back(idx);
        }
        else
        {
            if (!otherRoot)
            {
                otherRoot = std::make_unique<FolderNode>();
                otherRoot->fullPath = {};
                otherRoot->normalizedKey = "__other_uncategorized_root__";
                otherRoot->displayName = "Other / Imported Files";
                otherRoot->subtitlePath = "Files outside registered Scanned Folders";
                otherRoot->depth = 0;
                otherRoot->isScannedRoot = false;
                otherRoot->isVirtualOtherRoot = true;
            }

            juce::String folderLabel = parentDir.isNotEmpty() ? juce::File(parentDir).getFileName() : "Root";
            if (folderLabel.isEmpty()) folderLabel = parentDir;

            auto it = otherRoot->childLookup.find(parentKey);
            FolderNode* sub = nullptr;
            if (it != otherRoot->childLookup.end())
            {
                sub = it->second;
            }
            else
            {
                auto child = std::make_unique<FolderNode>();
                child->fullPath = parentDir;
                child->normalizedKey = "__other__/" + parentKey;
                child->displayName = folderLabel;
                child->subtitlePath = parentDir;
                child->depth = 1;
                child->parent = otherRoot.get();
                sub = child.get();
                otherRoot->childLookup[parentKey] = sub;
                otherRoot->subfolders.push_back(std::move(child));
            }
            sub->directItemIndices.push_back(idx);
        }
    }

    if (otherRoot && (!otherRoot->subfolders.empty() || !otherRoot->directItemIndices.empty()))
        rootFolders.push_back(std::move(otherRoot));

    for (auto& root : rootFolders)
        finalizeFolderNode(root.get());

    // Update top toolbar summary text
    int scannedCount = static_cast<int>(currentScanFolders.size());
    int sampleCount = static_cast<int>(currentItems.size());
    juce::String folderWord = (scannedCount == 1) ? "SCANNED FOLDER" : "SCANNED FOLDERS";
    juce::String sampleWord = (sampleCount == 1) ? "SAMPLE" : "SAMPLES";
    summaryLabel.setText(juce::String(scannedCount) + " " + folderWord +
                         juce::String(juce::CharPointer_UTF8("   \xe2\x80\xa2   ")) +
                         juce::String(sampleCount) + " " + sampleWord,
                         juce::dontSendNotification);

    rebuildVisibleRows();
}

void FolderBrowserComponent::finalizeFolderNode(FolderNode* node)
{
    if (node == nullptr) return;

    for (auto& child : node->subfolders)
        finalizeFolderNode(child.get());

    std::sort(node->subfolders.begin(), node->subfolders.end(),
              [](const std::unique_ptr<FolderNode>& a, const std::unique_ptr<FolderNode>& b) {
                  return a->displayName.compareNatural(b->displayName) < 0;
              });

    std::sort(node->directItemIndices.begin(), node->directItemIndices.end(),
              [this](size_t a, size_t b) {
                  return currentItems[a].fileName.compareNatural(currentItems[b].fileName) < 0;
              });

    int samples = static_cast<int>(node->directItemIndices.size());
    int folders = static_cast<int>(node->subfolders.size());
    for (const auto& child : node->subfolders)
    {
        samples += child->totalSampleCount;
        folders += child->totalSubfolderCount;
    }
    node->totalSampleCount = samples;
    node->totalSubfolderCount = folders;
}

void FolderBrowserComponent::rebuildVisibleRows()
{
    visibleRows.clear();
    visibleRows.reserve(rootFolders.size() + 128);

    for (auto& root : rootFolders)
        appendNodeToVisibleRows(root.get());

    listBox.updateContent();

    // Restore selection row cleanly without triggering unwanted audio playback
    int rowToSelect = -1;
    if (currentSelectedItemId.isNotEmpty())
    {
        for (size_t i = 0; i < visibleRows.size(); ++i)
        {
            if (!visibleRows[i].isFolder &&
                visibleRows[i].itemIndex < currentItems.size() &&
                currentItems[visibleRows[i].itemIndex].id == currentSelectedItemId)
            {
                rowToSelect = static_cast<int>(i);
                break;
            }
        }
    }

    if (rowToSelect < 0 && currentSelectedFolderKey.isNotEmpty())
    {
        for (size_t i = 0; i < visibleRows.size(); ++i)
        {
            if (visibleRows[i].isFolder &&
                visibleRows[i].folder != nullptr &&
                visibleRows[i].folder->normalizedKey == currentSelectedFolderKey)
            {
                rowToSelect = static_cast<int>(i);
                break;
            }
        }
    }

    isSynchronizingSelection = true;
    if (rowToSelect >= 0)
        listBox.selectRow(rowToSelect, true, true);
    else
        listBox.deselectAllRows();
    isSynchronizingSelection = false;

    listBox.repaint();
}

void FolderBrowserComponent::appendNodeToVisibleRows(FolderNode* node)
{
    if (node == nullptr) return;

    bool expanded = (expandedFolderKeys.find(node->normalizedKey) != expandedFolderKeys.end());

    VisibleRow folderRow;
    folderRow.isFolder = true;
    folderRow.folder = node;
    folderRow.parentFolder = node->parent;
    folderRow.depth = node->depth;
    folderRow.isExpanded = expanded;
    visibleRows.push_back(folderRow);

    if (!expanded)
        return;

    for (auto& sub : node->subfolders)
        appendNodeToVisibleRows(sub.get());

    for (size_t itemIdx : node->directItemIndices)
    {
        VisibleRow itemRow;
        itemRow.isFolder = false;
        itemRow.folder = nullptr;
        itemRow.parentFolder = node;
        itemRow.itemIndex = itemIdx;
        itemRow.depth = node->depth + 1;
        itemRow.isExpanded = false;
        visibleRows.push_back(itemRow);
    }
}

void FolderBrowserComponent::setSubtreeExpanded(FolderNode* node, bool expanded)
{
    if (node == nullptr) return;

    if (expanded)
        expandedFolderKeys.insert(node->normalizedKey);
    else
        expandedFolderKeys.erase(node->normalizedKey);

    for (auto& sub : node->subfolders)
        setSubtreeExpanded(sub.get(), expanded);
}

void FolderBrowserComponent::toggleFolderRow(FolderNode* node, bool recursiveSubtree)
{
    if (node == nullptr) return;

    bool currentlyExpanded = (expandedFolderKeys.find(node->normalizedKey) != expandedFolderKeys.end());
    bool nextExpanded = !currentlyExpanded;

    currentSelectedItemId.clear();
    currentSelectedFolderKey = node->normalizedKey;

    if (recursiveSubtree)
    {
        setSubtreeExpanded(node, nextExpanded);
    }
    else if (nextExpanded)
    {
        expandedFolderKeys.insert(node->normalizedKey);

        // Auto-unfold immediate single-child empty wrapper folders so 1 click reaches actual content
        FolderNode* cursor = node;
        while (cursor->directItemIndices.empty() && cursor->subfolders.size() == 1)
        {
            cursor = cursor->subfolders[0].get();
            expandedFolderKeys.insert(cursor->normalizedKey);
        }
    }
    else
    {
        expandedFolderKeys.erase(node->normalizedKey);
    }

    rebuildVisibleRows();
}

void FolderBrowserComponent::expandAllFolders()
{
    for (auto& root : rootFolders)
        setSubtreeExpanded(root.get(), true);

    rebuildVisibleRows();
}

void FolderBrowserComponent::collapseAllFolders()
{
    expandedFolderKeys.clear();
    rebuildVisibleRows();
}

void FolderBrowserComponent::revealAndExpandRootFolder(const juce::String& folderPath)
{
    juce::String targetKey = normalizePathKey(folderPath);
    for (auto& root : rootFolders)
    {
        if (root->normalizedKey == targetKey)
        {
            expandedFolderKeys.insert(root->normalizedKey);
            FolderNode* cursor = root.get();
            while (cursor->directItemIndices.empty() && cursor->subfolders.size() == 1)
            {
                cursor = cursor->subfolders[0].get();
                expandedFolderKeys.insert(cursor->normalizedKey);
            }

            currentSelectedItemId.clear();
            currentSelectedFolderKey = root->normalizedKey;
            rebuildVisibleRows();

            for (size_t i = 0; i < visibleRows.size(); ++i)
            {
                if (visibleRows[i].isFolder && visibleRows[i].folder == root.get())
                {
                    listBox.smoothScrollToRow(static_cast<int>(i));
                    break;
                }
            }
            return;
        }
    }
}

int FolderBrowserComponent::getNumRows()
{
    return static_cast<int>(visibleRows.size());
}

void FolderBrowserComponent::paintListBoxItem(int rowNumber,
                                              juce::Graphics& g,
                                              int width,
                                              int height,
                                              bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(visibleRows.size()))
        return;

    const auto& row = visibleRows[static_cast<size_t>(rowNumber)];

    // -------------------------------------------------------------------------
    // A. FOLDER ROW (Scanned Folder Root or Collapsible Subfolder)
    // -------------------------------------------------------------------------
    if (row.isFolder && row.folder != nullptr)
    {
        const auto& folder = *row.folder;
        const bool isRoot = (row.depth == 0);

        if (rowIsSelected)
        {
            g.fillAll(OpenWavLookAndFeel::accentCyan.withAlpha(0.20f));
            g.setColour(OpenWavLookAndFeel::accentCyan);
            g.fillRect(0, 0, 3, height);
        }
        else if (isRoot)
        {
            auto baseBg = OpenWavLookAndFeel::bgHeader.interpolatedWith(
                OpenWavLookAndFeel::accentCyan, row.isExpanded ? 0.10f : 0.04f);
            g.fillAll(baseBg);
            if (row.isExpanded)
            {
                g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.85f));
                g.fillRect(0, 0, 3, height);
            }
        }
        else
        {
            g.fillAll(OpenWavLookAndFeel::bgHeader.interpolatedWith(
                OpenWavLookAndFeel::bgCard, row.isExpanded ? 0.25f : 0.50f));
        }

        // Bottom hairline divider
        g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(isRoot ? 0.90f : 0.45f));
        g.fillRect(0, height - 1, width, 1);

        // Tree guide lines for subfolders
        const int indentStep = 18;
        for (int d = 0; d < row.depth; ++d)
        {
            float gx = static_cast<float>(16 + d * indentStep);
            g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.55f));
            g.drawVerticalLine(juce::roundToInt(gx), 0.0f, static_cast<float>(height));
            if (d == row.depth - 1)
                g.drawHorizontalLine(height / 2, gx, gx + 8.0f);
        }

        int x = 10 + row.depth * indentStep;

        // 1. Disclosure Chevron (> / v)
        {
            float cx = static_cast<float>(x) + 6.0f;
            float cy = static_cast<float>(height) * 0.5f;
            juce::Path chevron;
            if (row.isExpanded)
            {
                chevron.startNewSubPath(cx - 4.0f, cy - 2.0f);
                chevron.lineTo(cx, cy + 2.5f);
                chevron.lineTo(cx + 4.0f, cy - 2.0f);
            }
            else
            {
                chevron.startNewSubPath(cx - 2.0f, cy - 4.0f);
                chevron.lineTo(cx + 2.5f, cy);
                chevron.lineTo(cx - 2.0f, cy + 4.0f);
            }

            bool hasContent = (folder.totalSampleCount > 0 || !folder.subfolders.empty());
            g.setColour(hasContent ? OpenWavLookAndFeel::accentCyan
                                   : OpenWavLookAndFeel::textSecondary.withAlpha(0.45f));
            g.strokePath(chevron, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        x += 18;

        // 2. Crisp Vector Folder Icon
        {
            auto iconArea = juce::Rectangle<float>(static_cast<float>(x), (height - 15.0f) * 0.5f, 16.0f, 15.0f);
            juce::Path fp;
            fp.startNewSubPath(iconArea.getX(), iconArea.getY() + 2.0f);
            fp.lineTo(iconArea.getX() + 5.8f, iconArea.getY() + 2.0f);
            fp.lineTo(iconArea.getX() + 7.6f, iconArea.getY() + 4.2f);
            fp.lineTo(iconArea.getRight(), iconArea.getY() + 4.2f);
            fp.lineTo(iconArea.getRight(), iconArea.getBottom() - 1.2f);
            fp.lineTo(iconArea.getX(), iconArea.getBottom() - 1.2f);
            fp.closeSubPath();

            juce::Colour folderCol = isRoot ? OpenWavLookAndFeel::accentCyan
                                            : OpenWavLookAndFeel::accentCyan.withAlpha(0.82f);
            if (row.isExpanded)
            {
                g.setColour(folderCol.withAlpha(0.22f));
                g.fillPath(fp);
            }
            g.setColour(folderCol);
            g.strokePath(fp, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        x += 22;

        // 3. Right-side Count Badge
        juce::String badgeText;
        if (isRoot)
        {
            int directSubs = static_cast<int>(folder.subfolders.size());
            if (directSubs > 0)
            {
                badgeText << directSubs << juce::String(juce::CharPointer_UTF8(
                    directSubs == 1 ? " subfolder  \xe2\x80\xa2  " : " subfolders  \xe2\x80\xa2  "));
            }
            badgeText << folder.totalSampleCount << (folder.totalSampleCount == 1 ? " sample" : " samples");
        }
        else
        {
            badgeText = juce::String(folder.totalSampleCount) + (folder.totalSampleCount == 1 ? " sample" : " samples");
        }

        int badgeW = juce::roundToInt(badgeFont.getStringWidthFloat(badgeText)) + 16;
        auto badgeBounds = juce::Rectangle<float>(static_cast<float>(width - badgeW - 12),
                                                  (height - 19.0f) * 0.5f,
                                                  static_cast<float>(badgeW),
                                                  19.0f);

        if (folder.totalSampleCount > 0)
        {
            g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(isRoot ? 0.16f : 0.10f));
            g.fillRoundedRectangle(badgeBounds, 9.5f);
            g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(isRoot ? 0.50f : 0.30f));
            g.drawRoundedRectangle(badgeBounds, 9.5f, 1.0f);
            g.setColour(isRoot ? OpenWavLookAndFeel::accentCyan : OpenWavLookAndFeel::textPrimary);
        }
        else
        {
            g.setColour(OpenWavLookAndFeel::bgDark.withAlpha(0.45f));
            g.fillRoundedRectangle(badgeBounds, 9.5f);
            g.setColour(OpenWavLookAndFeel::textSecondary.withAlpha(0.55f));
        }
        g.setFont(badgeFont);
        g.drawText(badgeText, badgeBounds.toNearestInt(), juce::Justification::centred, true);

        // 4. Folder Name + Optional Root Full Path Subtitle
        int maxTextRight = static_cast<int>(badgeBounds.getX()) - 12;
        if (maxTextRight > x)
        {
            const auto& nameFont = isRoot ? rootFolderFont : subFolderFont;
            g.setFont(nameFont);
            g.setColour(rowIsSelected ? OpenWavLookAndFeel::accentCyan : OpenWavLookAndFeel::textPrimary);

            int nameWidth = std::min(maxTextRight - x, juce::roundToInt(nameFont.getStringWidthFloat(folder.displayName)) + 6);
            g.drawText(folder.displayName, x, 0, nameWidth, height, juce::Justification::centredLeft, true);

            if (isRoot && folder.subtitlePath.isNotEmpty() && (x + nameWidth + 16) < maxTextRight)
            {
                g.setFont(pathHintFont);
                g.setColour(OpenWavLookAndFeel::textSecondary.withAlpha(0.65f));
                g.drawText(juce::String(juce::CharPointer_UTF8("\xe2\x80\x94  ")) + folder.subtitlePath,
                           x + nameWidth + 6,
                           0,
                           maxTextRight - (x + nameWidth + 6),
                           height,
                           juce::Justification::centredLeft,
                           true);
            }
        }
        return;
    }

    // -------------------------------------------------------------------------
    // B. AUDIO SAMPLE ROW (Indented inside Expanded Folder)
    // -------------------------------------------------------------------------
    if (row.itemIndex >= currentItems.size())
        return;

    const auto& item = currentItems[row.itemIndex];
    const auto cols = computeColumns(width);

    if (rowIsSelected)
    {
        g.fillAll(OpenWavLookAndFeel::accentCyan.withAlpha(0.18f));
        g.setColour(OpenWavLookAndFeel::accentCyan);
        g.fillRect(0, 0, 3, height);
    }
    else
    {
        if (rowNumber % 2 == 1)
            g.fillAll(OpenWavLookAndFeel::bgDark.withAlpha(0.35f));
        else
            g.fillAll(OpenWavLookAndFeel::bgCard);
    }

    // Subtle tree vertical guide lines
    const int indentStep = 18;
    int maxDepthPx = std::max(0, cols.tagsX - 90);
    int visualDepth = std::min(row.depth, maxDepthPx / indentStep);

    for (int d = 0; d < visualDepth; ++d)
    {
        float gx = static_cast<float>(16 + d * indentStep);
        g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.40f));
        g.drawVerticalLine(juce::roundToInt(gx), 0.0f, static_cast<float>(height));
    }

    int itemStartX = 10 + visualDepth * indentStep;

    // 1. Play / Pause Icon
    {
        bool isCurrentFile = (cachedCurrentFilePath == item.filePath);
        bool isPlaying = isCurrentFile && cachedIsPlaying;
        const auto& img = isPlaying ? pauseIconImage : playIconImage;
        if (img.isValid())
        {
            int iconW = img.getWidth() / 2;
            int iconH = img.getHeight() / 2;
            int ix = itemStartX;
            int iy = (height - iconH) / 2;
            g.drawImage(img, ix, iy, iconW, iconH, 0, 0, img.getWidth(), img.getHeight());
        }
    }

    // 2. Sample File Name
    {
        int nameTextX = itemStartX + 26;
        int nameTextW = std::max(0, cols.tagsX - nameTextX - 8);
        if (nameTextW > 10)
        {
            g.setColour(rowIsSelected ? OpenWavLookAndFeel::accentCyan : OpenWavLookAndFeel::textPrimary);
            g.setFont(rowIsSelected ? cellBoldFont : cellFont);
            g.drawText(item.fileName, nameTextX, 0, nameTextW, height, juce::Justification::centredLeft, true);
        }
    }

    // 3. Tags Pill Badges
    {
        auto tagCell = juce::Rectangle<int>(cols.tagsX, 0, cols.tagsW, height).reduced(6, 0);
        float tagX = static_cast<float>(tagCell.getX());
        float tagY = static_cast<float>(tagCell.getY() + 4);
        float tagHeight = static_cast<float>(height - 8);

        g.setFont(tagFont);
        if (!item.cachedTags.empty())
        {
            for (const auto& tagInfo : item.cachedTags)
            {
                float tagWidth = static_cast<float>(tagInfo.width);
                if (tagX + tagWidth > tagCell.getRight()) break;

                auto pillBounds = juce::Rectangle<float>(tagX, tagY, tagWidth, tagHeight);
                g.setColour(OpenWavLookAndFeel::bgCard);
                g.fillRoundedRectangle(pillBounds, 3.0f);
                g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.5f));
                g.drawRoundedRectangle(pillBounds, 3.0f, 1.0f);
                g.setColour(OpenWavLookAndFeel::textPrimary);
                g.drawText(tagInfo.tag, pillBounds, juce::Justification::centred, true);

                tagX += tagWidth + 4.0f;
            }
        }
        else
        {
            for (const auto& tag : item.tags)
            {
                float tagWidth = tagFont.getStringWidthFloat(tag) + 12.0f;
                if (tagX + tagWidth > tagCell.getRight()) break;

                auto pillBounds = juce::Rectangle<float>(tagX, tagY, tagWidth, tagHeight);
                g.setColour(OpenWavLookAndFeel::bgCard);
                g.fillRoundedRectangle(pillBounds, 3.0f);
                g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.5f));
                g.drawRoundedRectangle(pillBounds, 3.0f, 1.0f);
                g.setColour(OpenWavLookAndFeel::textPrimary);
                g.drawText(tag, pillBounds, juce::Justification::centred, true);

                tagX += tagWidth + 4.0f;
            }
        }
    }

    // 4. Duration
    g.setFont(cellFont);
    g.setColour(OpenWavLookAndFeel::textSecondary);
    g.drawText(item.cachedFormattedDuration.isNotEmpty()
                   ? item.cachedFormattedDuration
                   : formatDuration(item.durationSeconds),
               juce::Rectangle<int>(cols.durX + 6, 0, cols.durW - 10, height),
               juce::Justification::centredLeft, true);

    // 5. Format
    g.setColour(OpenWavLookAndFeel::accentCyan);
    g.drawText(item.cachedUppercaseExtension.isNotEmpty()
                   ? item.cachedUppercaseExtension
                   : item.fileExtension.toUpperCase(),
               juce::Rectangle<int>(cols.fmtX + 6, 0, cols.fmtW - 10, height),
               juce::Justification::centredLeft, true);

    // 6. Sample Rate / Bit Depth
    g.setColour(OpenWavLookAndFeel::textSecondary);
    if (item.cachedFormattedSampleRate.isNotEmpty())
    {
        g.drawText(item.cachedFormattedSampleRate,
                   juce::Rectangle<int>(cols.srX + 6, 0, cols.srW - 10, height),
                   juce::Justification::centredLeft, true);
    }
    else
    {
        juce::String srStr = juce::String(item.sampleRate / 1000.0, 1) + " kHz";
        if (item.bitDepth > 0) srStr += " / " + juce::String(item.bitDepth) + "b";
        g.drawText(srStr,
                   juce::Rectangle<int>(cols.srX + 6, 0, cols.srW - 10, height),
                   juce::Justification::centredLeft, true);
    }

    // 7. Rating Stars
    {
        int r = juce::jlimit(0, 5, item.rating);
        const auto& img = starRatingImages[r];
        if (img.isValid())
        {
            int imgW = img.getWidth() / 2;
            int imgH = img.getHeight() / 2;
            int rx = cols.ratingX + (cols.ratingW - imgW) / 2;
            int ry = (height - imgH) / 2;
            g.drawImage(img, rx, ry, imgW, imgH, 0, 0, img.getWidth(), img.getHeight());
        }
    }

    // 8. Favorite Heart
    {
        const auto& img = item.isFavorite ? heartActiveImage : heartInactiveImage;
        if (img.isValid())
        {
            int imgW = img.getWidth() / 2;
            int imgH = img.getHeight() / 2;
            int hx = cols.favX + (cols.favW - imgW) / 2;
            int hy = (height - imgH) / 2;
            g.drawImage(img, hx, hy, imgW, imgH, 0, 0, img.getWidth(), img.getHeight());
        }
    }
}

void FolderBrowserComponent::listBoxItemClicked(int rowNumber, const juce::MouseEvent& e)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(visibleRows.size()))
        return;

    listBox.grabKeyboardFocus();
    const auto row = visibleRows[static_cast<size_t>(rowNumber)];

    // Folder Row Click
    if (row.isFolder && row.folder != nullptr)
    {
        if (e.mods.isPopupMenu())
        {
            showFolderContextMenu(row.folder);
            return;
        }

        bool recursive = e.mods.isAltDown() || e.mods.isShiftDown();
        toggleFolderRow(row.folder, recursive);
        return;
    }

    // Sample Row Click
    if (row.itemIndex >= currentItems.size())
        return;

    if (e.mods.isPopupMenu())
    {
        showSampleContextMenu(row.itemIndex, rowNumber);
        return;
    }

    int rowW = getWidth();
    if (auto* vp = listBox.getViewport())
    {
        if (vp->getMaximumVisibleWidth() > 100)
            rowW = vp->getMaximumVisibleWidth();
    }
    const auto cols = computeColumns(rowW);

    auto& item = currentItems[row.itemIndex];

    // 1. Favorite Heart Click
    if (e.x >= cols.favX)
    {
        bool newFav = !item.isFavorite;
        item.isFavorite = newFav;
        dbManager.toggleFavorite(item.id);

        if (currentFavOnly && !newFav)
        {
            currentItems.erase(currentItems.begin() + static_cast<std::ptrdiff_t>(row.itemIndex));
            rebuildTree();
        }
        else
        {
            listBox.repaintRow(rowNumber);
        }
        return;
    }

    // 2. Rating Stars Click
    if (e.x >= cols.ratingX && e.x < cols.favX)
    {
        float localX = static_cast<float>(e.x - cols.ratingX);
        int imgW = 64;
        float startX = (cols.ratingW - imgW) * 0.5f;
        float relX = localX - startX;
        float step = static_cast<float>(imgW) / 5.0f;

        int clickedStar = (relX < step) ? 1 : juce::jlimit(1, 5, static_cast<int>(relX / step) + 1);
        int newRating = (item.rating == clickedStar) ? 0 : clickedStar;

        item.rating = newRating;
        item.precomputeCachedStrings();
        dbManager.setRating(item.id, newRating);
        listBox.repaintRow(rowNumber);
        return;
    }

    // 3. Play / Pause Icon or Row Audition
    const int indentStep = 18;
    int maxDepthPx = std::max(0, cols.tagsX - 90);
    int visualDepth = std::min(row.depth, maxDepthPx / indentStep);
    int playBtnStartX = 8 + visualDepth * indentStep;
    int playBtnEndX = playBtnStartX + 28;

    currentSelectedFolderKey.clear();
    currentSelectedItemId = item.id;

    bool isCurrentFile = (audioEngine.getCurrentFile().getFullPathName() == item.filePath);
    if (e.x >= playBtnStartX && e.x <= playBtnEndX && isCurrentFile && audioEngine.isPlaying())
    {
        audioEngine.stop();
        listBox.repaintRow(rowNumber);
        return;
    }

    juce::File fileToLoad(item.filePath);
    if (fileToLoad.existsAsFile())
    {
        audioEngine.stop();
        audioEngine.setSampleBpm(item.bpm);
        audioEngine.loadFile(fileToLoad, true);
    }

    const MediaItem itemCopy = item;
    listeners.call([itemCopy](SampleTableListener& l) {
        l.sampleSelected(itemCopy);
    });
}

void FolderBrowserComponent::listBoxItemDoubleClicked(int rowNumber, const juce::MouseEvent& e)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(visibleRows.size()))
        return;

    const auto& row = visibleRows[static_cast<size_t>(rowNumber)];
    if (row.isFolder)
        return; // single-click already toggled folder cleanly

    if (e.x >= computeColumns(listBox.getWidth()).ratingX)
        return;

    if (row.itemIndex < currentItems.size())
    {
        const MediaItem itemCopy = currentItems[row.itemIndex];
        listeners.call([itemCopy](SampleTableListener& l) {
            l.sampleDoubleClicked(itemCopy);
        });
    }
}

void FolderBrowserComponent::selectedRowsChanged(int lastRowSelected)
{
    if (isSynchronizingSelection || suppressSelectionPlayback)
        return;

    if (lastRowSelected < 0 || lastRowSelected >= static_cast<int>(visibleRows.size()))
        return;

    const auto& row = visibleRows[static_cast<size_t>(lastRowSelected)];
    if (row.isFolder)
    {
        if (row.folder != nullptr)
        {
            currentSelectedFolderKey = row.folder->normalizedKey;
            currentSelectedItemId.clear();
        }
        return;
    }

    // Mouse clicks are handled with column-level precision inside listBoxItemClicked
    if (juce::ModifierKeys::getCurrentModifiersRealtime().isAnyMouseButtonDown())
        return;

    if (row.itemIndex < currentItems.size())
    {
        const auto& item = currentItems[row.itemIndex];
        currentSelectedFolderKey.clear();
        currentSelectedItemId = item.id;

        juce::File fileToLoad(item.filePath);
        if (fileToLoad.existsAsFile())
        {
            audioEngine.stop();
            audioEngine.setSampleBpm(item.bpm);
            audioEngine.loadFile(fileToLoad, true);
        }

        const MediaItem itemCopy = item;
        listeners.call([itemCopy](SampleTableListener& l) {
            l.sampleSelected(itemCopy);
        });
    }
}

void FolderBrowserComponent::returnKeyPressed(int lastRowSelected)
{
    if (lastRowSelected < 0 || lastRowSelected >= static_cast<int>(visibleRows.size()))
        return;

    const auto row = visibleRows[static_cast<size_t>(lastRowSelected)];
    if (row.isFolder && row.folder != nullptr)
    {
        toggleFolderRow(row.folder, false);
    }
    else if (!row.isFolder && row.itemIndex < currentItems.size())
    {
        const auto& item = currentItems[row.itemIndex];
        juce::File f(item.filePath);
        if (f.existsAsFile())
        {
            audioEngine.stop();
            audioEngine.setSampleBpm(item.bpm);
            audioEngine.loadFile(f, true);
        }
    }
}

bool FolderBrowserComponent::handleKeyNavigation(const juce::KeyPress& key)
{
    int sel = listBox.getSelectedRow();
    if (sel < 0 || sel >= static_cast<int>(visibleRows.size()))
        return false;

    const auto row = visibleRows[static_cast<size_t>(sel)];

    if (key == juce::KeyPress::rightKey)
    {
        if (row.isFolder && row.folder != nullptr)
        {
            if (!row.isExpanded)
            {
                toggleFolderRow(row.folder, false);
            }
            else if (sel + 1 < static_cast<int>(visibleRows.size()))
            {
                listBox.selectRow(sel + 1);
                listBox.smoothScrollToRow(sel + 1);
            }
            return true;
        }
    }
    else if (key == juce::KeyPress::leftKey)
    {
        if (row.isFolder && row.folder != nullptr && row.isExpanded)
        {
            toggleFolderRow(row.folder, false);
            return true;
        }
        else if (row.parentFolder != nullptr)
        {
            for (int i = sel - 1; i >= 0; --i)
            {
                if (visibleRows[static_cast<size_t>(i)].isFolder &&
                    visibleRows[static_cast<size_t>(i)].folder == row.parentFolder)
                {
                    listBox.selectRow(i);
                    listBox.smoothScrollToRow(i);
                    return true;
                }
            }
        }
    }

    return false;
}

void FolderBrowserComponent::moveSelection(int delta)
{
    int total = static_cast<int>(visibleRows.size());
    if (total <= 0)
        return;

    int cur = listBox.getSelectedRow();
    int next = juce::jlimit(0, total - 1, (cur < 0 ? 0 : cur + delta));
    listBox.selectRow(next);
    listBox.smoothScrollToRow(next);
}

void FolderBrowserComponent::selectItemById(const juce::String& itemId, bool triggerPlayback)
{
    if (itemId.isEmpty())
        return;

    currentSelectedItemId = itemId;
    currentSelectedFolderKey.clear();

    for (size_t i = 0; i < visibleRows.size(); ++i)
    {
        if (!visibleRows[i].isFolder &&
            visibleRows[i].itemIndex < currentItems.size() &&
            currentItems[visibleRows[i].itemIndex].id == itemId)
        {
            int row = static_cast<int>(i);
            suppressSelectionPlayback = !triggerPlayback;
            listBox.selectRow(row);
            suppressSelectionPlayback = false;
            listBox.smoothScrollToRow(row);
            listBox.repaint();
            return;
        }
    }
}

juce::var FolderBrowserComponent::getDragSourceDescription(const juce::SparseSet<int>& rowsToDescribe)
{
    if (rowsToDescribe.size() <= 0)
        return {};

    int r = rowsToDescribe[0];
    if (r < 0 || r >= static_cast<int>(visibleRows.size()))
        return {};

    const auto& row = visibleRows[static_cast<size_t>(r)];
    juce::String dragPath;

    if (row.isFolder && row.folder != nullptr && !row.folder->isVirtualOtherRoot)
    {
        dragPath = row.folder->fullPath;
    }
    else if (!row.isFolder && row.itemIndex < currentItems.size())
    {
        dragPath = currentItems[row.itemIndex].filePath;
    }

    if (dragPath.isNotEmpty())
    {
        if (juce::Desktop::getInstance().getMainMouseSource().isDragging())
        {
            juce::StringArray files;
            files.add(dragPath);
            juce::MessageManager::callAsync([files] {
                juce::DragAndDropContainer::performExternalDragDropOfFiles(files, false);
            });
        }
#if JUCE_LINUX
        return {};
#else
        return dragPath;
#endif
    }

    return {};
}

juce::String FolderBrowserComponent::getTooltipForRow(int rowNumber)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(visibleRows.size()))
        return {};

    const auto& row = visibleRows[static_cast<size_t>(rowNumber)];
    if (row.isFolder && row.folder != nullptr)
        return row.folder->fullPath;

    if (!row.isFolder && row.itemIndex < currentItems.size())
        return currentItems[row.itemIndex].filePath;

    return {};
}

void FolderBrowserComponent::showFolderContextMenu(FolderNode* folder)
{
    if (folder == nullptr)
        return;

    bool isExpanded = (expandedFolderKeys.find(folder->normalizedKey) != expandedFolderKeys.end());
    juce::String folderKey = folder->normalizedKey;
    juce::String folderPath = folder->fullPath;
    juce::String folderName = folder->displayName;
    bool isScannedRoot = folder->isScannedRoot;
    bool isVirtual = folder->isVirtualOtherRoot;

    juce::PopupMenu menu;
    menu.addSectionHeader(folder->displayName);
    menu.addItem(1, isExpanded ? "Collapse Folder" : "Expand Folder");
    if (!folder->subfolders.empty())
    {
        menu.addItem(2, "Expand All Subfolders");
        menu.addItem(3, "Collapse All Subfolders");
    }

    if (!isVirtual && folderPath.isNotEmpty())
    {
        menu.addSeparator();
        menu.addItem(4, "Reveal Folder in Explorer");

        if (isScannedRoot)
        {
            menu.addItem(5, "Rescan This Folder");
            menu.addSeparator();
            menu.addItem(6, "Remove from Scanned Folders");
        }
    }

    menu.showMenuAsync(juce::PopupMenu::Options().withMousePosition(),
                       [this, folderKey, folderPath, folderName](int result) {
        FolderNode* target = nullptr;
        std::function<FolderNode*(FolderNode*)> findNode = [&](FolderNode* n) -> FolderNode* {
            if (n == nullptr) return nullptr;
            if (n->normalizedKey == folderKey) return n;
            for (auto& c : n->subfolders)
                if (auto* found = findNode(c.get()))
                    return found;
            return nullptr;
        };
        for (auto& root : rootFolders)
        {
            target = findNode(root.get());
            if (target != nullptr) break;
        }

        if (result == 1 && target != nullptr)
        {
            toggleFolderRow(target, false);
        }
        else if (result == 2 && target != nullptr)
        {
            setSubtreeExpanded(target, true);
            rebuildVisibleRows();
        }
        else if (result == 3 && target != nullptr)
        {
            setSubtreeExpanded(target, false);
            rebuildVisibleRows();
        }
        else if (result == 4 && folderPath.isNotEmpty())
        {
            juce::File f(folderPath);
            if (f.exists())
                f.revealToUser();
        }
        else if (result == 5 && folderPath.isNotEmpty())
        {
            libraryScanner.startScan({ folderPath });
        }
        else if (result == 6 && folderPath.isNotEmpty())
        {
            juce::Component::SafePointer<FolderBrowserComponent> safeThis(this);
            auto options = juce::MessageBoxOptions::makeOptionsOkCancel(
                juce::MessageBoxIconType::WarningIcon,
                "Remove Scanned Folder",
                "Are you sure you want to remove \"" + folderName + "\" from Scanned Folders?\n\n" +
                    folderPath +
                    "\n\nIts indexed samples will be removed from the OWMB library (your audio files on disk will not be deleted).",
                "Remove Folder",
                "Cancel",
                this);
            juce::AlertWindow::showAsync(options, [safeThis, folderPath](int confirmResult) {
                if (confirmResult == 1 && safeThis != nullptr)
                {
                    safeThis->dbManager.removeScanFolder(folderPath);
                    safeThis->currentScanFolders = safeThis->dbManager.getScanFolders();
                    safeThis->rebuildTree();
                    safeThis->resized();
                    safeThis->repaint();
                    if (safeThis->onFoldersChanged)
                        safeThis->onFoldersChanged();
                }
            });
        }
    });
}

void FolderBrowserComponent::showSampleContextMenu(size_t itemIdx, int rowNumber)
{
    if (itemIdx >= currentItems.size())
        return;

    const MediaItem item = currentItems[itemIdx];

    juce::PopupMenu menu;
    menu.addSectionHeader(item.fileName);
    menu.addItem(1, item.isFavorite ? "Remove from Favorites" : "Add to Favorites");

    juce::PopupMenu ratingMenu;
    for (int r = 0; r <= 5; ++r)
        ratingMenu.addItem(10 + r, (r == 0) ? "No Rating" : juce::String(r) + " Stars", true, r == item.rating);
    menu.addSubMenu("Set Rating", ratingMenu);

    menu.addSeparator();
    menu.addItem(2, "Add Custom Tag...");

    juce::PopupMenu removeTagMenu;
    int tagIdx = 100;
    for (const auto& tag : item.tags)
        removeTagMenu.addItem(tagIdx++, "Remove " + tag);
    if (!item.tags.empty())
        menu.addSubMenu("Remove Tag", removeTagMenu);

    menu.addSeparator();
    menu.addItem(4, "Reveal in Explorer");
    menu.addItem(5, "Convert / Export Sample...");
    menu.addItem(6, "Add to Sample Map");
    menu.addItem(7, "Slice to Sample Map...");
    menu.addItem(8, "Edit Sample in Editor");

    menu.showMenuAsync(juce::PopupMenu::Options().withMousePosition(),
                       [this, item, itemIdx, rowNumber](int result) {
        if (result == 1)
        {
            if (itemIdx < currentItems.size() && currentItems[itemIdx].id == item.id)
                currentItems[itemIdx].isFavorite = !item.isFavorite;
            dbManager.toggleFavorite(item.id);
            listBox.repaintRow(rowNumber);
        }
        else if (result >= 10 && result <= 15)
        {
            int newRating = result - 10;
            if (itemIdx < currentItems.size() && currentItems[itemIdx].id == item.id)
            {
                currentItems[itemIdx].rating = newRating;
                currentItems[itemIdx].precomputeCachedStrings();
            }
            dbManager.setRating(item.id, newRating);
            listBox.repaintRow(rowNumber);
        }
        else if (result == 2)
        {
            auto alert = std::make_shared<juce::AlertWindow>(
                "Add Custom Tag",
                "Enter a new custom tag for " + item.fileName + ":",
                juce::AlertWindow::QuestionIcon);
            alert->addTextEditor("tagInput", "", "Tag (e.g. Kick, #Sub, Vocal)");
            if (auto* ed = alert->getTextEditor("tagInput"))
            {
                ed->setJustification(juce::Justification::centredLeft);
                ed->setIndents(4, 0);
            }
            alert->addButton("Add Tag", 1, juce::KeyPress(juce::KeyPress::returnKey));
            alert->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

            alert->enterModalState(true, juce::ModalCallbackFunction::create([this, alert, item](int btnResult) {
                if (btnResult == 1)
                {
                    auto newTag = alert->getTextEditorContents("tagInput").trim();
                    if (newTag.isNotEmpty())
                        dbManager.addTagToItem(item.id, newTag);
                }
            }));
        }
        else if (result >= 100)
        {
            int idx = result - 100;
            if (idx >= 0 && idx < static_cast<int>(item.tags.size()))
            {
                auto it = item.tags.begin();
                std::advance(it, idx);
                dbManager.removeTagFromItem(item.id, *it);
            }
        }
        else if (result == 4)
        {
            juce::File f(item.filePath);
            if (f.existsAsFile())
                f.revealToUser();
        }
        else if (result == 5)
        {
            convertSample(item);
        }
        else if (result == 6)
        {
            listeners.call([item](SampleTableListener& l) {
                l.addToSampleMapRequested(item);
            });
        }
        else if (result == 7)
        {
            listeners.call([item](SampleTableListener& l) {
                l.autoSliceToSamplerRequested(item);
            });
        }
        else if (result == 8)
        {
            listeners.call([item](SampleTableListener& l) {
                l.editSampleRequested(item);
            });
        }
    });
}

void FolderBrowserComponent::convertSample(const MediaItem& item)
{
    juce::StringArray formatNames;
    std::vector<juce::AudioFormat*> writableFormats;
    auto& formatManager = audioEngine.getFormatManager();
    for (int i = 0; i < formatManager.getNumKnownFormats(); ++i)
    {
        auto* format = formatManager.getKnownFormat(i);
        auto name = format->getFormatName();
        if (name != "MP3 file" && name != "MP3" && !formatNames.contains(name))
        {
            formatNames.add(name);
            writableFormats.push_back(format);
        }
    }

    if (writableFormats.empty())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                               "Convert Audio",
                                               "No writable audio formats are supported by the application.");
        return;
    }

    convertDialog = std::make_unique<ConvertDialog>(item, audioEngine, dbManager, writableFormats);
    convertDialog->showDialog();
}

void FolderBrowserComponent::libraryIndexUpdated()
{
    juce::Component::SafePointer<FolderBrowserComponent> safeThis(this);
    juce::MessageManager::callAsync([safeThis] {
        if (safeThis != nullptr)
        {
            auto latestFolders = safeThis->dbManager.getScanFolders();
            if (latestFolders != safeThis->currentScanFolders)
            {
                safeThis->currentScanFolders = latestFolders;
                safeThis->rebuildTree();
                safeThis->resized();
                safeThis->repaint();
            }
        }
    });
}

void FolderBrowserComponent::tagsUpdated()
{
}

void FolderBrowserComponent::sampleLoaded(const juce::String& filePath)
{
    cachedCurrentFilePath = filePath;
    cachedIsPlaying = audioEngine.isPlaying();

    for (size_t i = 0; i < visibleRows.size(); ++i)
    {
        if (!visibleRows[i].isFolder &&
            visibleRows[i].itemIndex < currentItems.size() &&
            currentItems[visibleRows[i].itemIndex].filePath == filePath)
        {
            currentSelectedItemId = currentItems[visibleRows[i].itemIndex].id;
            currentSelectedFolderKey.clear();
            suppressSelectionPlayback = true;
            listBox.selectRow(static_cast<int>(i));
            suppressSelectionPlayback = false;
            break;
        }
    }

    listBox.repaint();
}

void FolderBrowserComponent::playbackStateChanged(bool isPlaying)
{
    cachedIsPlaying = isPlaying;
    listBox.repaint();
}

void FolderBrowserComponent::addListener(SampleTableListener* listener)
{
    listeners.add(listener);
}

void FolderBrowserComponent::removeListener(SampleTableListener* listener)
{
    listeners.remove(listener);
}

} // namespace openwav

