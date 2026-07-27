#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_gui_basics/juce_gui_basics.h>
#endif
#include "../Database/TagDatabaseManager.h"
#include <set>

namespace openwav
{

class TagPanelListener
{
public:
    virtual ~TagPanelListener() = default;
    virtual void tagFilterSelectionChanged(const std::set<juce::String>& selectedTags, bool matchAllTags, bool favoritesOnly) = 0;
};

class TagPanelComponent : public juce::Component,
                          public TagDatabaseListener
{
public:
    explicit TagPanelComponent(TagDatabaseManager& db);
    ~TagPanelComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void refreshTags();
    void refreshFolders();
    const std::set<juce::String>& getSelectedTags() const { return selectedTags; }
    bool getMatchAllTags() const { return matchAll; }
    bool getFavoritesOnly() const { return favoritesOnly; }

    void addListener(TagPanelListener* listener);
    void removeListener(TagPanelListener* listener);

    // TagDatabaseListener callbacks
    void libraryIndexUpdated() override;
    void tagsUpdated() override;

private:
    void notifySelectionChanged();

    TagDatabaseManager& dbManager;

    juce::TextButton favoritesButton { "Favorites" };
    juce::ToggleButton matchModeToggle { "Match All Tags (AND)" };
    juce::TextButton clearFiltersButton { "Clear Tags" };
    juce::TextButton addCustomTagButton { "+ Add Custom Tag..." };
    juce::TextButton resetAllButton { "Reset All Data" };

    juce::Label foldersHeaderLabel { {}, "SCANNED FOLDERS" };
    juce::OwnedArray<juce::Label> folderLabels;
    juce::OwnedArray<juce::TextButton> folderRemoveButtons;

    juce::OwnedArray<juce::TextButton> tagButtons;
    std::set<juce::String> selectedTags;

    bool favoritesOnly { false };
    bool matchAll { false };

    juce::ListenerList<TagPanelListener> listeners;
};

} // namespace openwav
