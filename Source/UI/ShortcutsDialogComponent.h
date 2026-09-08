#pragma once

#if __has_include(<JuceHeader.h>)
#include <JuceHeader.h>
#else
#include <juce_gui_basics/juce_gui_basics.h>
#endif

#include "ShortcutManager.h"
#include <vector>
#include <memory>

namespace openwav {

class ShortcutsDialogComponent : public juce::Component,
                                 public ShortcutListener,
                                 public juce::TextEditor::Listener {
public:
    ShortcutsDialogComponent();
    ~ShortcutsDialogComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void lookAndFeelChanged() override;
    bool keyPressed(const juce::KeyPress& key) override;

    void showDialog();
    void hideDialog();

    // ShortcutListener
    void shortcutsChanged() override;

    // TextEditor::Listener
    void textEditorTextChanged(juce::TextEditor& editor) override;

private:
    class ShortcutRowComponent : public juce::Component {
    public:
        ShortcutRowComponent(const ShortcutItem& item, ShortcutsDialogComponent& owner);
        ~ShortcutRowComponent() override = default;

        void paint(juce::Graphics& g) override;
        void resized() override;
        void updateState(bool isRecording);

        const juce::String& getActionId() const { return actionId; }

    private:
        ShortcutsDialogComponent& owner;
        juce::String actionId;
        juce::String actionName;
        juce::String actionCategory;
        juce::String actionDescription;

        juce::TextButton keycapButton;
        juce::TextButton editButton { "Edit" };
        juce::TextButton clearButton { "Clear" };
        juce::TextButton resetButton { "Reset" };

        bool isRecording { false };
    };

    class ListContainerComponent : public juce::Component {
    public:
        ListContainerComponent() = default;
        ~ListContainerComponent() override = default;
        void paint(juce::Graphics& g) override;
    };

    void rebuildList();
    void selectCategory(int index);
    void startRecording(const juce::String& actionId);
    void stopRecording();

    juce::Label titleLabel { {}, "Keyboard Shortcuts" };
    juce::Label subtitleLabel { {}, "Click any key badge or 'Edit' to capture a new key combination." };
    juce::TextEditor searchEditor;

    juce::String currentCategoryFilter { "All" };
    std::vector<std::unique_ptr<juce::TextButton>> categoryButtons;

    juce::Viewport viewport;
    ListContainerComponent listContainer;
    std::vector<std::unique_ptr<ShortcutRowComponent>> rows;

    juce::TextButton resetAllButton { "Reset All to Defaults" };
    juce::TextButton closeButton { "Close" };

    juce::String recordingActionId;
    juce::Component::SafePointer<juce::DialogWindow> dialogWindow;

    friend class ShortcutRowComponent;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ShortcutsDialogComponent)
};

} // namespace openwav
