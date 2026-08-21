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
    void lookAndFeelChanged() override;

    void refreshTags();
    void refreshFolders();
    void clearAllFiltersAndSelection();
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

    juce::Label filterHeaderLabel { {}, "QUICK FILTERS" };
    juce::TextButton favoritesButton { "Favorites" };
    juce::TextButton clearFiltersButton { "Clear Tags" };
    juce::ToggleButton matchModeToggle { "Match All Tags (AND)" };
    juce::TextButton addCustomTagButton { "+ Add Custom Tag..." };
    juce::TextButton autoTagButton { "Auto-Tag Library" };

    juce::Label tagsHeaderLabel { {}, "TAGS" };
    juce::Viewport tagViewport;
    juce::Component tagCloudContainer;
    juce::OwnedArray<juce::TextButton> tagButtons;

    juce::Label foldersHeaderLabel { {}, "SCANNED FOLDERS" };
    juce::Viewport folderViewport;
    juce::Component folderListContainer;
    juce::OwnedArray<juce::Label> folderLabels;
    juce::OwnedArray<juce::TextButton> folderRemoveButtons;

    std::set<juce::String> selectedTags;

    bool favoritesOnly { false };
    bool matchAll { false };
    bool isAutoTagging { false };
    double autoTagProgress { 0.0 };

    juce::ListenerList<TagPanelListener> listeners;
};

} // namespace openwav
