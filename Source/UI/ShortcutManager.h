#pragma once

#if __has_include(<JuceHeader.h>)
#include <JuceHeader.h>
#else
#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#endif

#include <vector>

namespace openwav {

struct ShortcutItem {
    juce::String id;
    juce::String name;
    juce::String category;
    juce::String description;
    juce::KeyPress defaultKey;
    juce::KeyPress secondaryDefaultKey;
    juce::KeyPress customKey;
    bool isExplicitlyUnassigned { false };

    juce::KeyPress getEffectiveKey() const {
        if (isExplicitlyUnassigned)
            return juce::KeyPress();
        return customKey.isValid() ? customKey : defaultKey;
    }

    bool isCustomized() const {
        if (isExplicitlyUnassigned)
            return true;
        return customKey.isValid() && !(customKey == defaultKey);
    }
};

class ShortcutListener {
public:
    virtual ~ShortcutListener() = default;
    virtual void shortcutsChanged() = 0;
};

class ShortcutManager {
public:
    static ShortcutManager& getInstance();

    bool matches(const juce::String& actionId, const juce::KeyPress& key) const;

    const std::vector<ShortcutItem>& getAllShortcuts() const { return shortcuts; }
    ShortcutItem* getShortcut(const juce::String& actionId);
    const ShortcutItem* getShortcut(const juce::String& actionId) const;

    bool isKeyAssigned(const juce::KeyPress& key,
                       const juce::String& excludingActionId,
                       juce::String* outConflictingActionName = nullptr,
                       juce::String* outConflictingActionId = nullptr) const;

    void setShortcutKey(const juce::String& actionId, const juce::KeyPress& key);
    void clearShortcut(const juce::String& actionId);
    void resetShortcutToDefault(const juce::String& actionId);
    void resetAllToDefaults();

    static juce::String formatKeyPress(const juce::KeyPress& key);

    void addListener(ShortcutListener* listener);
    void removeListener(ShortcutListener* listener);

    void loadSettings();
    void saveSettings();

private:
    ShortcutManager();
    ~ShortcutManager() = default;

    void initDefaultShortcuts();
    void notifyChanged();
    juce::File getConfigFile() const;
    static bool compareKeyPress(const juce::KeyPress& candidate, const juce::KeyPress& target);

    std::vector<ShortcutItem> shortcuts;
    juce::ListenerList<ShortcutListener> listeners;

    JUCE_DECLARE_NON_COPYABLE(ShortcutManager)
};

} // namespace openwav
