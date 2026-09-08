#include "ShortcutsDialogComponent.h"
#include "OpenWavLookAndFeel.h"

#if JUCE_WINDOWS
#include <windows.h>
#include <dwmapi.h>
#endif

namespace openwav {

// ─────────────────────────────────────────────────────────
// ShortcutRowComponent
// ─────────────────────────────────────────────────────────

ShortcutsDialogComponent::ShortcutRowComponent::ShortcutRowComponent(
    const ShortcutItem& item, ShortcutsDialogComponent& ownerComp)
    : owner(ownerComp),
      actionId(item.id),
      actionName(item.name),
      actionCategory(item.category),
      actionDescription(item.description) {
    addAndMakeVisible(keycapButton);
    addAndMakeVisible(editButton);
    addAndMakeVisible(clearButton);
    addAndMakeVisible(resetButton);

    keycapButton.onClick = [this] {
        if (isRecording)
            owner.stopRecording();
        else
            owner.startRecording(actionId);
    };

    editButton.onClick = [this] {
        if (isRecording)
            owner.stopRecording();
        else
            owner.startRecording(actionId);
    };

    clearButton.onClick = [this] {
        ShortcutManager::getInstance().clearShortcut(actionId);
    };

    resetButton.onClick = [this] {
        ShortcutManager::getInstance().resetShortcutToDefault(actionId);
    };

    updateState(false);
}

void ShortcutsDialogComponent::ShortcutRowComponent::updateState(bool recording) {
    isRecording = recording;

    const auto* item = ShortcutManager::getInstance().getShortcut(actionId);
    auto effKey = item ? item->getEffectiveKey() : juce::KeyPress();
    bool isCustom = item ? item->isCustomized() : false;

    if (isRecording) {
        keycapButton.setButtonText("Press a key... (Esc)");
        keycapButton.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::accentCyan.withAlpha(0.25f));
        keycapButton.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::accentCyan);
        editButton.setButtonText("Cancel");
    } else {
        juce::String keyText = ShortcutManager::formatKeyPress(effKey);
        keycapButton.setButtonText(keyText);
        keycapButton.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::bgHover);
        keycapButton.setColour(juce::TextButton::textColourOffId,
                               effKey.isValid() ? OpenWavLookAndFeel::textPrimary
                                                : OpenWavLookAndFeel::textSecondary.withAlpha(0.6f));
        editButton.setButtonText("Edit");
    }

    clearButton.setVisible(!isRecording && effKey.isValid());
    resetButton.setVisible(!isRecording && isCustom);

    clearButton.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::bgHover.withAlpha(0.6f));
    clearButton.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::textSecondary);

    resetButton.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::bgHover.withAlpha(0.6f));
    resetButton.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::accentBlue);

    editButton.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::bgHover);
    editButton.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::textPrimary);

    repaint();
}

void ShortcutsDialogComponent::ShortcutRowComponent::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();

    // Subtle hover / recording background
    if (isRecording) {
        g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.08f));
        g.fillRoundedRectangle(bounds.reduced(2.0f), 6.0f);
        g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.5f));
        g.drawRoundedRectangle(bounds.reduced(2.0f), 6.0f, 1.5f);
    } else {
        g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.25f));
        g.drawHorizontalLine(getHeight() - 1, 8.0f, static_cast<float>(getWidth() - 8));
    }

    // Category Pill
    float catWidth = 100.0f;
    auto catArea = bounds.removeFromLeft(catWidth + 16.0f).reduced(8.0f, 12.0f);
    g.setColour(OpenWavLookAndFeel::accentBlue.withAlpha(0.18f));
    g.fillRoundedRectangle(catArea, 4.0f);
    g.setColour(OpenWavLookAndFeel::accentBlue);
    g.drawRoundedRectangle(catArea, 4.0f, 1.0f);
    g.setFont(juce::Font(11.0f, juce::Font::bold));
    g.drawText(actionCategory, catArea, juce::Justification::centred, true);

    // Action Name & Description
    int textX = static_cast<int>(catArea.getRight()) + 14;
    int availableWidth = keycapButton.getX() - textX - 10;
    if (availableWidth > 0) {
        g.setColour(OpenWavLookAndFeel::textPrimary);
        g.setFont(juce::Font(13.5f, juce::Font::bold));
        g.drawText(actionName, textX, 7, availableWidth, 18, juce::Justification::centredLeft, true);

        g.setColour(OpenWavLookAndFeel::textSecondary);
        g.setFont(juce::Font(11.0f, juce::Font::plain));
        g.drawText(actionDescription, textX, 26, availableWidth, 16, juce::Justification::centredLeft, true);
    }
}

void ShortcutsDialogComponent::ShortcutRowComponent::resized() {
    auto area = getLocalBounds().reduced(8, 8);

    int btnWidth = 56;
    int keycapWidth = 145;

    resetButton.setBounds(area.removeFromRight(btnWidth));
    area.removeFromRight(6);

    clearButton.setBounds(area.removeFromRight(btnWidth));
    area.removeFromRight(6);

    editButton.setBounds(area.removeFromRight(btnWidth));
    area.removeFromRight(8);

    keycapButton.setBounds(area.removeFromRight(keycapWidth));
}

// ─────────────────────────────────────────────────────────
// ListContainerComponent
// ─────────────────────────────────────────────────────────

void ShortcutsDialogComponent::ListContainerComponent::paint(juce::Graphics& g) {
    g.fillAll(OpenWavLookAndFeel::bgCard.withAlpha(0.4f));
}

// ─────────────────────────────────────────────────────────
// ShortcutsDialogComponent
// ─────────────────────────────────────────────────────────

ShortcutsDialogComponent::ShortcutsDialogComponent() {
    setWantsKeyboardFocus(true);

    addAndMakeVisible(titleLabel);
    titleLabel.setFont(juce::Font(20.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textPrimary);

    addAndMakeVisible(subtitleLabel);
    subtitleLabel.setFont(juce::Font(12.5f, juce::Font::plain));
    subtitleLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);

    addAndMakeVisible(searchEditor);
    searchEditor.setTextToShowWhenEmpty("Search shortcuts by name, action, or key...",
                                       OpenWavLookAndFeel::textSecondary.withAlpha(0.6f));
    searchEditor.addListener(this);

    // Setup category filter pills
    const juce::StringArray catNames = {
        "All", "Global & Transport", "UI & Scaling", "Audio Editor", "Sample Map", "View Navigation"
    };
    const juce::StringArray catLabels = {
        "All", "Transport", "UI & Scaling", "Audio Editor", "Sample Map", "Views"
    };

    for (int i = 0; i < catNames.size(); ++i) {
        auto btn = std::make_unique<juce::TextButton>(catLabels[i]);
        btn->setClickingTogglesState(false);
        int idx = i;
        btn->onClick = [this, idx] { selectCategory(idx); };
        addAndMakeVisible(*btn);
        categoryButtons.push_back(std::move(btn));
    }

    viewport.setViewedComponent(&listContainer, false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);

    addAndMakeVisible(resetAllButton);
    resetAllButton.onClick = [this] {
        juce::AlertWindow::showAsync(
            juce::MessageBoxOptions()
                .withIconType(juce::MessageBoxIconType::WarningIcon)
                .withTitle("Reset All Shortcuts")
                .withMessage("Are you sure you want to reset all keyboard shortcuts to their default factory settings?")
                .withButton("Reset to Defaults")
                .withButton("Cancel"),
            [](int result) {
                if (result == 1) {
                    ShortcutManager::getInstance().resetAllToDefaults();
                }
            });
    };

    addAndMakeVisible(closeButton);
    closeButton.onClick = [this] { hideDialog(); };

    ShortcutManager::getInstance().addListener(this);
    selectCategory(0);
}

ShortcutsDialogComponent::~ShortcutsDialogComponent() {
    ShortcutManager::getInstance().removeListener(this);
}

void ShortcutsDialogComponent::selectCategory(int index) {
    const juce::StringArray catNames = {
        "All", "Global & Transport", "UI & Scaling", "Audio Editor", "Sample Map", "View Navigation"
    };

    if (index >= 0 && index < catNames.size()) {
        currentCategoryFilter = catNames[index];
    }

    for (size_t i = 0; i < categoryButtons.size(); ++i) {
        bool isActive = (static_cast<int>(i) == index);
        categoryButtons[i]->setColour(juce::TextButton::buttonColourId,
                                      isActive ? OpenWavLookAndFeel::accentCyan.withAlpha(0.3f)
                                               : OpenWavLookAndFeel::bgHover);
        categoryButtons[i]->setColour(juce::TextButton::textColourOffId,
                                      isActive ? OpenWavLookAndFeel::accentCyan
                                               : OpenWavLookAndFeel::textSecondary);
    }

    rebuildList();
}

void ShortcutsDialogComponent::textEditorTextChanged(juce::TextEditor& /*editor*/) {
    rebuildList();
}

void ShortcutsDialogComponent::shortcutsChanged() {
    rebuildList();
}

void ShortcutsDialogComponent::rebuildList() {
    rows.clear();

    juce::String filterText = searchEditor.getText().trim();
    const auto& allShortcuts = ShortcutManager::getInstance().getAllShortcuts();

    for (const auto& item : allShortcuts) {
        // Category filtering
        if (currentCategoryFilter != "All" && item.category != currentCategoryFilter)
            continue;

        // Search text filtering
        if (filterText.isNotEmpty()) {
            juce::String effKeyText = ShortcutManager::formatKeyPress(item.getEffectiveKey());
            bool match = item.name.containsIgnoreCase(filterText) ||
                         item.description.containsIgnoreCase(filterText) ||
                         item.category.containsIgnoreCase(filterText) ||
                         effKeyText.containsIgnoreCase(filterText);
            if (!match)
                continue;
        }

        auto row = std::make_unique<ShortcutRowComponent>(item, *this);
        bool isThisRecording = (recordingActionId == item.id);
        row->updateState(isThisRecording);
        listContainer.addAndMakeVisible(*row);
        rows.push_back(std::move(row));
    }

    resized();
}

void ShortcutsDialogComponent::startRecording(const juce::String& actionId) {
    recordingActionId = actionId;
    grabKeyboardFocus();

    for (auto& r : rows) {
        r->updateState(r->getActionId() == recordingActionId);
    }
}

void ShortcutsDialogComponent::stopRecording() {
    recordingActionId = {};

    for (auto& r : rows) {
        r->updateState(false);
    }
}

bool ShortcutsDialogComponent::keyPressed(const juce::KeyPress& key) {
    if (recordingActionId.isNotEmpty()) {
        // Cancel on Escape key
        if (key == juce::KeyPress::escapeKey) {
            stopRecording();
            return true;
        }

        if (key.isValid()) {
            juce::String conflictingName, conflictingId;
            if (ShortcutManager::getInstance().isKeyAssigned(key, recordingActionId, &conflictingName, &conflictingId)) {
                juce::String targetActionId = recordingActionId;
                juce::AlertWindow::showAsync(
                    juce::MessageBoxOptions()
                        .withIconType(juce::MessageBoxIconType::QuestionIcon)
                        .withTitle("Shortcut Conflict")
                        .withMessage("'" + ShortcutManager::formatKeyPress(key) +
                                     "' is already assigned to:\n\"" + conflictingName +
                                     "\"\n\nDo you want to reassign it? (This will clear the key from \"" + conflictingName + "\")")
                        .withButton("Reassign")
                        .withButton("Cancel"),
                    [this, key, targetActionId, conflictingId](int result) {
                        if (result == 1) {
                            ShortcutManager::getInstance().clearShortcut(conflictingId);
                            ShortcutManager::getInstance().setShortcutKey(targetActionId, key);
                        }
                        stopRecording();
                    });
                return true;
            }

            ShortcutManager::getInstance().setShortcutKey(recordingActionId, key);
            stopRecording();
            return true;
        }
    }

    if (key == juce::KeyPress::escapeKey) {
        hideDialog();
        return true;
    }

    return Component::keyPressed(key);
}

void ShortcutsDialogComponent::showDialog() {
    lookAndFeelChanged();
    stopRecording();
    searchEditor.clear();
    selectCategory(0);
    rebuildList();

    if (dialogWindow == nullptr) {
        setSize(700, 520);
        juce::DialogWindow::LaunchOptions opts;
        opts.content.setNonOwned(this);
        opts.dialogTitle = "Keyboard Shortcuts";
        opts.dialogBackgroundColour = OpenWavLookAndFeel::bgCard;
        opts.escapeKeyTriggersCloseButton = false;
        opts.useNativeTitleBar = true;
        opts.resizable = true;

        dialogWindow = opts.launchAsync();

        if (dialogWindow != nullptr) {
            dialogWindow->setResizeLimits(580, 420, 1024, 768);
            dialogWindow->centreWithSize(700, 520);

#if JUCE_WINDOWS
            if (auto* peer = dialogWindow->getPeer()) {
                HWND hwnd = (HWND)peer->getNativeHandle();
                BOOL isDark = OpenWavLookAndFeel::isDarkTheme() ? TRUE : FALSE;
                DwmSetWindowAttribute(hwnd, 20, &isDark, sizeof(isDark));
                DwmSetWindowAttribute(hwnd, 19, &isDark, sizeof(isDark));
            }
#endif
        }
    } else {
        dialogWindow->toFront(true);
    }
}

void ShortcutsDialogComponent::hideDialog() {
    stopRecording();
    if (dialogWindow != nullptr) {
        dialogWindow->exitModalState(0);
    }
}

void ShortcutsDialogComponent::paint(juce::Graphics& g) {
    g.fillAll(OpenWavLookAndFeel::bgDark);

    // Separator above footer
    g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.3f));
    g.drawHorizontalLine(getHeight() - 56, 16.0f, static_cast<float>(getWidth() - 16));
}

void ShortcutsDialogComponent::lookAndFeelChanged() {
    titleLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textPrimary);
    subtitleLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);

    searchEditor.setColour(juce::TextEditor::backgroundColourId, OpenWavLookAndFeel::bgCard);
    searchEditor.setColour(juce::TextEditor::textColourId, OpenWavLookAndFeel::textPrimary);
    searchEditor.setColour(juce::TextEditor::outlineColourId, OpenWavLookAndFeel::borderColour);
    searchEditor.setColour(juce::TextEditor::focusedOutlineColourId, OpenWavLookAndFeel::accentCyan);

    resetAllButton.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::bgHover);
    resetAllButton.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::favoriteRed.withAlpha(0.85f));

    closeButton.setColour(juce::TextButton::buttonColourId, OpenWavLookAndFeel::accentCyan.withAlpha(0.25f));
    closeButton.setColour(juce::TextButton::textColourOffId, OpenWavLookAndFeel::accentCyan);

    repaint();
}

void ShortcutsDialogComponent::resized() {
    auto area = getLocalBounds().reduced(20, 16);

    // Header
    titleLabel.setBounds(area.removeFromTop(26));
    subtitleLabel.setBounds(area.removeFromTop(18));
    area.removeFromTop(12);

    // Search bar
    searchEditor.setBounds(area.removeFromTop(32));
    area.removeFromTop(10);

    // Category pills
    auto catArea = area.removeFromTop(28);
    int catBtnWidth = catArea.getWidth() / static_cast<int>(categoryButtons.size());
    for (auto& btn : categoryButtons) {
        btn->setBounds(catArea.removeFromLeft(catBtnWidth).reduced(2, 0));
    }
    area.removeFromTop(12);

    // Footer
    auto footerArea = area.removeFromBottom(42);
    resetAllButton.setBounds(footerArea.removeFromLeft(160).reduced(0, 5));
    closeButton.setBounds(footerArea.removeFromRight(90).reduced(0, 5));
    area.removeFromBottom(8);

    // Scrollable Viewport
    viewport.setBounds(area);

    const int rowHeight = 48;
    int totalHeight = static_cast<int>(rows.size()) * rowHeight;
    int containerWidth = std::max(200, viewport.getMaximumVisibleWidth());

    listContainer.setBounds(0, 0, containerWidth, std::max(totalHeight, viewport.getHeight()));

    int y = 0;
    for (auto& row : rows) {
        row->setBounds(0, y, containerWidth, rowHeight);
        y += rowHeight;
    }
}

} // namespace openwav
