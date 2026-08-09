#include "TagPanelComponent.h"
#include "OpenWavLookAndFeel.h"

namespace openwav
{

TagPanelComponent::TagPanelComponent(TagDatabaseManager& db)
    : dbManager(db)
{
    dbManager.addListener(this);

    // Section Headers styling
    auto setupSectionHeader = [](juce::Label& lbl) {
        lbl.setFont(juce::Font(10.0f).boldened());
        lbl.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    };

    setupSectionHeader(filterHeaderLabel);
    setupSectionHeader(tagsHeaderLabel);
    setupSectionHeader(foldersHeaderLabel);

    addAndMakeVisible(filterHeaderLabel);
    addAndMakeVisible(tagsHeaderLabel);
    addAndMakeVisible(foldersHeaderLabel);

    // Viewports setup
    tagViewport.setViewedComponent(&tagCloudContainer, false);
    tagViewport.setScrollBarsShown(true, false);
    addAndMakeVisible(tagViewport);

    folderViewport.setViewedComponent(&folderListContainer, false);
    folderViewport.setScrollBarsShown(true, false);
    addAndMakeVisible(folderViewport);

    // Favorites filter button
    favoritesButton.onClick = [this] {
        favoritesOnly = !favoritesOnly;
        favoritesButton.setToggleState(favoritesOnly, juce::dontSendNotification);
        notifySelectionChanged();
    };
    addAndMakeVisible(favoritesButton);

    // Clear filters button
    clearFiltersButton.onClick = [this] {
        selectedTags.clear();
        favoritesOnly = false;
        favoritesButton.setToggleState(false, juce::dontSendNotification);
        for (auto* btn : tagButtons)
        {
            btn->setToggleState(false, juce::dontSendNotification);
        }
        notifySelectionChanged();
    };
    addAndMakeVisible(clearFiltersButton);

    // Match mode toggle (AND / OR)
    matchModeToggle.onClick = [this] {
        matchAll = matchModeToggle.getToggleState();
        notifySelectionChanged();
    };
    addAndMakeVisible(matchModeToggle);

    // Add Custom Tag button
    addCustomTagButton.onClick = [this] {
        auto alert = std::make_shared<juce::AlertWindow>("Add Custom Tag", "Enter tag name to add or filter by:", juce::AlertWindow::QuestionIcon);
        alert->addTextEditor("tagInput", "", "Tag (e.g. #Warm, Bass)");
        if (auto* ed = alert->getTextEditor("tagInput"))
        {
            ed->setJustification(juce::Justification::centredLeft);
            ed->setIndents(4, 0);
        }
        alert->addButton("Add Tag", 1, juce::KeyPress(juce::KeyPress::returnKey));
        alert->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

        alert->enterModalState(true, juce::ModalCallbackFunction::create([this, alert](int result) {
            if (result == 1)
            {
                juce::String inputTag = alert->getTextEditorContents("tagInput").trim();
                if (inputTag.isNotEmpty())
                {
                    if (!inputTag.startsWith("#"))
                        inputTag = "#" + inputTag;
                    
                    selectedTags.insert(inputTag);
                    notifySelectionChanged();
                    refreshTags();
                }
            }
        }));
    };
    addAndMakeVisible(addCustomTagButton);

    autoTagButton.onClick = [this] {
        dbManager.reTagAllItems();
    };
    addAndMakeVisible(autoTagButton);

    refreshTags();
    refreshFolders();
}

void TagPanelComponent::clearAllFiltersAndSelection()
{
    selectedTags.clear();
    favoritesOnly = false;
    favoritesButton.setToggleState(false, juce::dontSendNotification);
    refreshTags();
    notifySelectionChanged();
}

TagPanelComponent::~TagPanelComponent()
{
    dbManager.removeListener(this);
}

void TagPanelComponent::paint(juce::Graphics& g)
{
    g.fillAll(OpenWavLookAndFeel::bgCard);
    g.setColour(OpenWavLookAndFeel::borderColour);
    g.drawRect(getLocalBounds().removeFromRight(1));
}

void TagPanelComponent::refreshTags()
{
    tagButtons.clear();
    tagCloudContainer.removeAllChildren();

    auto freqs = dbManager.getTagFrequencies();
    tagsHeaderLabel.setText("TAGS (" + juce::String(freqs.size()) + ")", juce::dontSendNotification);

    std::vector<std::pair<juce::String, int>> sortedTags(freqs.begin(), freqs.end());

    std::sort(sortedTags.begin(), sortedTags.end(), [](const auto& a, const auto& b) {
        if (a.second != b.second) return a.second > b.second;
        return a.first < b.first;
    });

    for (const auto& pair : sortedTags)
    {
        auto tagStr = pair.first;
        int count = pair.second;

        auto* btn = new juce::TextButton(tagStr + " (" + juce::String(count) + ")");
        btn->setClickingTogglesState(true);
        btn->setToggleState(selectedTags.find(tagStr) != selectedTags.end(), juce::dontSendNotification);

        btn->onClick = [this, tagStr, btn] {
            if (btn->getToggleState())
                selectedTags.insert(tagStr);
            else
                selectedTags.erase(tagStr);

            notifySelectionChanged();
        };

        tagButtons.add(btn);
        tagCloudContainer.addAndMakeVisible(btn);
    }

    resized();
    repaint();
}

void TagPanelComponent::refreshFolders()
{
    folderLabels.clear();
    folderRemoveButtons.clear();
    folderListContainer.removeAllChildren();

    auto folders = dbManager.getScanFolders();
    foldersHeaderLabel.setText("SCANNED FOLDERS (" + juce::String(folders.size()) + ")", juce::dontSendNotification);

    for (const auto& folderPath : folders)
    {
        juce::File f(folderPath);
        juce::String displayName = f.getFileName();
        if (displayName.isEmpty()) displayName = folderPath;

        auto* lbl = new juce::Label({}, displayName);
        lbl->setFont(juce::Font(11.0f).boldened());
        lbl->setColour(juce::Label::textColourId, juce::Colours::white);
        lbl->setTooltip(folderPath);
        folderLabels.add(lbl);
        folderListContainer.addAndMakeVisible(lbl);

        auto* removeBtn = new juce::TextButton("X");
        removeBtn->setTooltip("Remove folder: " + folderPath);
        removeBtn->onClick = [this, folderPath] {
            class FolderRemovalThread : public juce::ThreadWithProgressWindow
            {
            public:
                FolderRemovalThread(TagDatabaseManager& db, const juce::String& path)
                    : juce::ThreadWithProgressWindow("Removing Folder", true, false), dbManager(db), folderPath(path)
                {}

                void run() override
                {
                    setProgress(-1.0);
                    setStatusMessage("Removing " + folderPath + "...");
                    dbManager.removeScanFolder(folderPath);
                }
            private:
                TagDatabaseManager& dbManager;
                juce::String folderPath;
            };
            
            FolderRemovalThread removalThread(dbManager, folderPath);
            removalThread.runThread();
            
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                                                   "Folder Removed",
                                                   "Successfully removed folder:\n" + folderPath);
        };
        folderRemoveButtons.add(removeBtn);
        folderListContainer.addAndMakeVisible(removeBtn);
    }

    resized();
    repaint();
}

void TagPanelComponent::resized()
{
    auto area = getLocalBounds().reduced(10, 10);

    // Quick Filters Section Header
    filterHeaderLabel.setBounds(area.removeFromTop(16));
    area.removeFromTop(4);

    // Quick Filters Row (Favorites + Clear)
    auto filterRow = area.removeFromTop(26);
    favoritesButton.setBounds(filterRow.removeFromLeft(filterRow.getWidth() / 2 - 2));
    clearFiltersButton.setBounds(filterRow);
    area.removeFromTop(6);

    matchModeToggle.setBounds(area.removeFromTop(20));
    area.removeFromTop(6);

    auto customTagRow = area.removeFromTop(26);
    addCustomTagButton.setBounds(customTagRow.removeFromLeft(customTagRow.getWidth() / 2 - 2));
    autoTagButton.setBounds(customTagRow);
    area.removeFromTop(12);

    // Scanned Folders Section (Fixed/Flexible Height at Bottom of remaining space)
    int folderCount = folderLabels.size();
    int targetFoldersHeight = (folderCount > 0) ? std::min(130, 20 + 4 + folderCount * 26) : 36;
    auto foldersSection = area.removeFromBottom(targetFoldersHeight);
    area.removeFromBottom(10);

    foldersHeaderLabel.setBounds(foldersSection.removeFromTop(18));
    foldersSection.removeFromTop(4);
    folderViewport.setBounds(foldersSection);

    // Layout folder items inside folderListContainer
    int fContainerWidth = std::max(100, folderViewport.getMaximumVisibleWidth());
    int fY = 0;
    for (int i = 0; i < folderCount; ++i)
    {
        auto row = juce::Rectangle<int>(0, fY, fContainerWidth, 24);
        folderRemoveButtons[i]->setBounds(row.removeFromRight(20).withHeight(20));
        row.removeFromRight(4);
        folderLabels[i]->setBounds(row.withHeight(20));
        fY += 26;
    }
    folderListContainer.setBounds(0, 0, fContainerWidth, std::max(1, fY));

    // Tags Section (Takes remaining vertical space in middle)
    tagsHeaderLabel.setBounds(area.removeFromTop(18));
    area.removeFromTop(4);
    tagViewport.setBounds(area);

    // Layout tag pills inside tagCloudContainer
    int viewportWidth = std::max(100, tagViewport.getMaximumVisibleWidth());
    int x = 0;
    int y = 0;
    int pillHeight = 24;
    int gap = 6;

    for (auto* btn : tagButtons)
    {
        int fontWidth = juce::Font(11.0f).getStringWidth(btn->getButtonText());
        int btnWidth = fontWidth + 18;

        if (x + btnWidth > viewportWidth && x > 0)
        {
            x = 0;
            y += pillHeight + gap;
        }

        btn->setBounds(x, y, btnWidth, pillHeight);
        x += btnWidth + gap;
    }

    int totalTagHeight = (tagButtons.size() > 0) ? (y + pillHeight) : 0;
    tagCloudContainer.setBounds(0, 0, viewportWidth, std::max(1, totalTagHeight));
}

void TagPanelComponent::notifySelectionChanged()
{
    listeners.call([this](TagPanelListener& l) {
        l.tagFilterSelectionChanged(selectedTags, matchAll, favoritesOnly);
    });
}

void TagPanelComponent::libraryIndexUpdated()
{
    juce::MessageManager::callAsync([this] {
        refreshTags();
        refreshFolders();
    });
}

void TagPanelComponent::tagsUpdated()
{
    juce::MessageManager::callAsync([this] { refreshTags(); });
}

void TagPanelComponent::addListener(TagPanelListener* listener)
{
    listeners.add(listener);
}

void TagPanelComponent::removeListener(TagPanelListener* listener)
{
    listeners.remove(listener);
}

} // namespace openwav
