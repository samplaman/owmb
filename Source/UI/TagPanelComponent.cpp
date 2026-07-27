#include "TagPanelComponent.h"
#include "OpenWavLookAndFeel.h"

namespace openwav
{

TagPanelComponent::TagPanelComponent(TagDatabaseManager& db)
    : dbManager(db)
{
    dbManager.addListener(this);

    // Favorites filter button
    favoritesButton.onClick = [this] {
        favoritesOnly = !favoritesOnly;
        favoritesButton.setToggleState(favoritesOnly, juce::dontSendNotification);
        notifySelectionChanged();
    };
    addAndMakeVisible(favoritesButton);

    // Match mode toggle (AND / OR)
    matchModeToggle.onClick = [this] {
        matchAll = matchModeToggle.getToggleState();
        notifySelectionChanged();
    };
    addAndMakeVisible(matchModeToggle);

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

    // Add Custom Tag button
    addCustomTagButton.onClick = [this] {
        auto alert = std::make_shared<juce::AlertWindow>("Add Custom Tag", "Enter tag name to add or filter by:", juce::AlertWindow::QuestionIcon);
        alert->addTextEditor("tagInput", "", "Tag (e.g. #Warm, Bass)");
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

    // Reset All Data button
    resetAllButton.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::favoriteRed.withAlpha(0.2f));
    resetAllButton.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::favoriteRed.brighter(0.5f));
    resetAllButton.onClick = [this] {
        juce::AlertWindow::showAsync(
            juce::MessageBoxOptions()
                .withIconType(juce::MessageBoxIconType::WarningIcon)
                .withTitle("Reset All Data")
                .withMessage("Are you sure you want to reset all library items, custom tags, and scanned folders?\nThis action cannot be undone.")
                .withButton("Reset Everything")
                .withButton("Cancel"),
            [this](int result) {
                if (result == 1)
                {
                    selectedTags.clear();
                    favoritesOnly = false;
                    favoritesButton.setToggleState(false, juce::dontSendNotification);
                    dbManager.clearAllData();
                }
            });
    };
    addAndMakeVisible(resetAllButton);

    // Folders Header
    foldersHeaderLabel.setFont(juce::Font(11.0f).boldened());
    foldersHeaderLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::accentCyan);
    addAndMakeVisible(foldersHeaderLabel);

    refreshTags();
    refreshFolders();
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

    auto freqs = dbManager.getTagFrequencies();
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
        addAndMakeVisible(btn);
    }

    resized();
    repaint();
}

void TagPanelComponent::refreshFolders()
{
    folderLabels.clear();
    folderRemoveButtons.clear();

    auto folders = dbManager.getScanFolders();
    foldersHeaderLabel.setText("SCANNED FOLDERS (" + juce::String(folders.size()) + ")", juce::dontSendNotification);

    for (const auto& folderPath : folders)
    {
        juce::File f(folderPath);
        juce::String displayName = f.getFileName();
        if (displayName.isEmpty()) displayName = folderPath;

        auto* lbl = new juce::Label({}, "Folder: " + displayName);
        lbl->setFont(juce::Font(11.0f).boldened());
        lbl->setColour(juce::Label::textColourId, OpenWavLookAndFeel::textPrimary);
        lbl->setTooltip(folderPath);
        folderLabels.add(lbl);
        addAndMakeVisible(lbl);

        auto* removeBtn = new juce::TextButton("X");
        removeBtn->setTooltip("Remove folder: " + folderPath);
        removeBtn->onClick = [this, folderPath] {
            dbManager.removeScanFolder(folderPath);
        };
        folderRemoveButtons.add(removeBtn);
        addAndMakeVisible(removeBtn);
    }

    resized();
    repaint();
}

void TagPanelComponent::resized()
{
    auto area = getLocalBounds().reduced(8, 8);

    // Bottom-most Reset All Data button
    resetAllButton.setBounds(area.removeFromBottom(28));
    area.removeFromBottom(8);

    // Top control bar
    favoritesButton.setBounds(area.removeFromTop(28));
    area.removeFromTop(6);

    matchModeToggle.setBounds(area.removeFromTop(22));
    area.removeFromTop(6);

    auto btnRow = area.removeFromTop(26);
    clearFiltersButton.setBounds(btnRow.removeFromLeft(btnRow.getWidth() / 2 - 2));
    addCustomTagButton.setBounds(btnRow.removeFromRight(btnRow.getWidth()));
    area.removeFromTop(10);

    // Reserve bottom portion for Scanned Folders list
    int foldersHeight = std::max(60, folderLabels.size() * 26 + 30);
    auto foldersArea = area.removeFromBottom(foldersHeight);
    area.removeFromBottom(8);

    // Tag cloud area (wrapping pills)
    int x = area.getX();
    int y = area.getY();
    int pillHeight = 24;

    for (auto* btn : tagButtons)
    {
        int fontWidth = juce::Font(11.0f).getStringWidth(btn->getButtonText());
        int btnWidth = fontWidth + 18;

        if (x + btnWidth > area.getRight() && x > area.getX())
        {
            x = area.getX();
            y += pillHeight + 6;
        }

        if (y + pillHeight <= area.getBottom())
        {
            btn->setBounds(x, y, btnWidth, pillHeight);
            btn->setVisible(true);
        }
        else
        {
            btn->setVisible(false);
        }

        x += btnWidth + 6;
    }

    // Scanned Folders area layout
    foldersHeaderLabel.setBounds(foldersArea.removeFromTop(20));
    foldersArea.removeFromTop(4);

    for (int i = 0; i < folderLabels.size(); ++i)
    {
        auto row = foldersArea.removeFromTop(24);
        folderRemoveButtons[i]->setBounds(row.removeFromRight(22).withHeight(20));
        row.removeFromRight(4);
        folderLabels[i]->setBounds(row.withHeight(20));
    }
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
