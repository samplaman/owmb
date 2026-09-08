#include "ShortcutManager.h"

namespace openwav {

ShortcutManager& ShortcutManager::getInstance() {
    static ShortcutManager instance;
    return instance;
}

ShortcutManager::ShortcutManager() {
    initDefaultShortcuts();
    loadSettings();
}

void ShortcutManager::initDefaultShortcuts() {
    shortcuts.clear();

    const auto cmdMod = juce::ModifierKeys::commandModifier;

    // ── Global & Transport ──
    shortcuts.push_back({
        "transport.play_pause",
        "Play / Pause Audio Preview",
        "Global & Transport",
        "Toggle preview playback for current sample",
        juce::KeyPress(juce::KeyPress::spaceKey),
        juce::KeyPress(),
        juce::KeyPress(),
        false
    });

    shortcuts.push_back({
        "transport.loop",
        "Toggle Loop Playback",
        "Global & Transport",
        "Turn loop preview playback on or off",
        juce::KeyPress('l'),
        juce::KeyPress(),
        juce::KeyPress(),
        false
    });

    shortcuts.push_back({
        "transport.slice",
        "Trigger Slice",
        "Global & Transport",
        "Trigger slice chopping on active waveform",
        juce::KeyPress('s'),
        juce::KeyPress(),
        juce::KeyPress(),
        false
    });

    // ── UI & Scaling ──
    shortcuts.push_back({
        "ui.zoom_in",
        "Increase UI Scale (+10%)",
        "UI & Scaling",
        "Scale up font and interface size",
        juce::KeyPress('=', cmdMod, '='),
        juce::KeyPress(juce::KeyPress::numberPadAdd, cmdMod, '+'),
        juce::KeyPress(),
        false
    });

    shortcuts.push_back({
        "ui.zoom_out",
        "Decrease UI Scale (-10%)",
        "UI & Scaling",
        "Scale down font and interface size",
        juce::KeyPress('-', cmdMod, '-'),
        juce::KeyPress(juce::KeyPress::numberPadSubtract, cmdMod, '-'),
        juce::KeyPress(),
        false
    });

    shortcuts.push_back({
        "ui.zoom_reset",
        "Reset UI Scale (100%)",
        "UI & Scaling",
        "Reset interface zoom to default 100%",
        juce::KeyPress('0', cmdMod, '0'),
        juce::KeyPress(juce::KeyPress::numberPad0, cmdMod, '0'),
        juce::KeyPress(),
        false
    });

    // ── Audio Editor ──
    shortcuts.push_back({
        "edit.select_all",
        "Select All Region",
        "Audio Editor",
        "Select the entire audio waveform",
        juce::KeyPress('a', cmdMod, 'a'),
        juce::KeyPress(),
        juce::KeyPress(),
        false
    });

    shortcuts.push_back({
        "edit.deselect_all",
        "Deselect Region",
        "Audio Editor",
        "Clear selection region in waveform",
        juce::KeyPress('d', cmdMod, 'd'),
        juce::KeyPress(juce::KeyPress::escapeKey),
        juce::KeyPress(),
        false
    });

    shortcuts.push_back({
        "edit.escape",
        "Cancel / Deselect",
        "Audio Editor",
        "Cancel selection or spectral box",
        juce::KeyPress(juce::KeyPress::escapeKey),
        juce::KeyPress(),
        juce::KeyPress(),
        false
    });

    shortcuts.push_back({
        "edit.silence_selection",
        "Silence / Remove Selection",
        "Audio Editor",
        "Silence selected range or remove spectral box",
        juce::KeyPress(juce::KeyPress::deleteKey),
        juce::KeyPress(juce::KeyPress::backspaceKey),
        juce::KeyPress(),
        false
    });

    shortcuts.push_back({
        "edit.set_start_cursor",
        "Set Sample Start to Playhead",
        "Audio Editor",
        "Move sample start boundary to current playhead cursor",
        juce::KeyPress('s'),
        juce::KeyPress(),
        juce::KeyPress(),
        false
    });

    shortcuts.push_back({
        "edit.set_end_cursor",
        "Set Sample End to Playhead",
        "Audio Editor",
        "Move sample end boundary to current playhead cursor",
        juce::KeyPress('e'),
        juce::KeyPress(),
        juce::KeyPress(),
        false
    });

    // ── Sample Map ──
    shortcuts.push_back({
        "map.delete_zone",
        "Delete Selected Zones",
        "Sample Map",
        "Remove selected sample zones from keyboard map",
        juce::KeyPress(juce::KeyPress::deleteKey),
        juce::KeyPress(juce::KeyPress::backspaceKey),
        juce::KeyPress(),
        false
    });

    shortcuts.push_back({
        "map.nudge_down",
        "Nudge Root Note Down (-1)",
        "Sample Map",
        "Shift root note of selected zones down 1 semitone",
        juce::KeyPress(juce::KeyPress::leftKey),
        juce::KeyPress(),
        juce::KeyPress(),
        false
    });

    shortcuts.push_back({
        "map.nudge_up",
        "Nudge Root Note Up (+1)",
        "Sample Map",
        "Shift root note of selected zones up 1 semitone",
        juce::KeyPress(juce::KeyPress::rightKey),
        juce::KeyPress(),
        juce::KeyPress(),
        false
    });

    shortcuts.push_back({
        "map.velocity_up",
        "Adjust Velocity (+4)",
        "Sample Map",
        "Increase velocity threshold of selected zones",
        juce::KeyPress(juce::KeyPress::upKey),
        juce::KeyPress(),
        juce::KeyPress(),
        false
    });

    shortcuts.push_back({
        "map.velocity_down",
        "Adjust Velocity (-4)",
        "Sample Map",
        "Decrease velocity threshold of selected zones",
        juce::KeyPress(juce::KeyPress::downKey),
        juce::KeyPress(),
        juce::KeyPress(),
        false
    });

    // ── View Navigation ──
    shortcuts.push_back({
        "view.list",
        "Switch to List View",
        "View Navigation",
        "Navigate to sample list browser",
        juce::KeyPress('1', cmdMod, '1'),
        juce::KeyPress(),
        juce::KeyPress(),
        false
    });

    shortcuts.push_back({
        "view.galaxy",
        "Switch to 3D Galaxy View",
        "View Navigation",
        "Navigate to 3D constellation audio cloud",
        juce::KeyPress('2', cmdMod, '2'),
        juce::KeyPress(),
        juce::KeyPress(),
        false
    });

    shortcuts.push_back({
        "view.edit",
        "Switch to Audio Editor View",
        "View Navigation",
        "Navigate to waveform audio editor",
        juce::KeyPress('3', cmdMod, '3'),
        juce::KeyPress(),
        juce::KeyPress(),
        false
    });

    shortcuts.push_back({
        "view.sample_map",
        "Switch to Sample Map View",
        "View Navigation",
        "Navigate to multi-sample instrument map",
        juce::KeyPress('4', cmdMod, '4'),
        juce::KeyPress(),
        juce::KeyPress(),
        false
    });
}

ShortcutItem* ShortcutManager::getShortcut(const juce::String& actionId) {
    for (auto& item : shortcuts) {
        if (item.id == actionId)
            return &item;
    }
    return nullptr;
}

const ShortcutItem* ShortcutManager::getShortcut(const juce::String& actionId) const {
    for (const auto& item : shortcuts) {
        if (item.id == actionId)
            return &item;
    }
    return nullptr;
}

bool ShortcutManager::compareKeyPress(const juce::KeyPress& candidate, const juce::KeyPress& target) {
    if (!candidate.isValid() || !target.isValid())
        return false;

    auto candMods = candidate.getModifiers();
    auto targMods = target.getModifiers();

    if (candMods.isCommandDown() != targMods.isCommandDown())
        return false;
    if (candMods.isAltDown() != targMods.isAltDown())
        return false;
    if (candMods.isCtrlDown() != targMods.isCtrlDown())
        return false;

    int candCode = candidate.getKeyCode();
    int targCode = target.getKeyCode();
    juce_wchar candChar = candidate.getTextCharacter();
    juce_wchar targChar = target.getTextCharacter();

    // Plus '=' and keypad '+' equivalence (Shift tolerant)
    if ((candCode == '+' || candCode == '=' || candCode == juce::KeyPress::numberPadAdd || candChar == '+' || candChar == '=') &&
        (targCode == '+' || targCode == '=' || targCode == juce::KeyPress::numberPadAdd || targChar == '+' || targChar == '='))
        return true;

    // Minus '_' and keypad '-' equivalence (Shift tolerant)
    if ((candCode == '-' || candCode == '_' || candCode == juce::KeyPress::numberPadSubtract || candChar == '-' || candChar == '_') &&
        (targCode == '-' || targCode == '_' || targCode == juce::KeyPress::numberPadSubtract || targChar == '-' || targChar == '_'))
        return true;

    if (candMods.isShiftDown() != targMods.isShiftDown())
        return false;

    if (candCode == targCode)
        return true;

    // Case-insensitive letter comparison
    if (candCode >= 'a' && candCode <= 'z') candCode -= 32;
    if (targCode >= 'a' && targCode <= 'z') targCode -= 32;
    if (candCode == targCode)
        return true;

    if (candChar != 0 && targChar != 0) {
        if (candChar == targChar)
            return true;
        if (juce::CharacterFunctions::toUpperCase(candChar) == juce::CharacterFunctions::toUpperCase(targChar))
            return true;
    }

    // Delete vs Backspace equivalence
    if ((candCode == juce::KeyPress::deleteKey || candCode == juce::KeyPress::backspaceKey) &&
        (targCode == juce::KeyPress::deleteKey || targCode == juce::KeyPress::backspaceKey))
        return true;

    // 0 and keypad 0 equivalence
    if ((candCode == '0' || candCode == juce::KeyPress::numberPad0 || candChar == '0') &&
        (targCode == '0' || targCode == juce::KeyPress::numberPad0 || targChar == '0'))
        return true;

    return false;
}

bool ShortcutManager::matches(const juce::String& actionId, const juce::KeyPress& key) const {
    const auto* item = getShortcut(actionId);
    if (!item || item->isExplicitlyUnassigned)
        return false;

    auto effKey = item->getEffectiveKey();
    if (compareKeyPress(key, effKey))
        return true;

    if (!item->isCustomized() && item->secondaryDefaultKey.isValid()) {
        if (compareKeyPress(key, item->secondaryDefaultKey))
            return true;
    }

    return false;
}

bool ShortcutManager::isKeyAssigned(const juce::KeyPress& key,
                                   const juce::String& excludingActionId,
                                   juce::String* outConflictingActionName,
                                   juce::String* outConflictingActionId) const {
    if (!key.isValid())
        return false;

    for (const auto& item : shortcuts) {
        if (item.id == excludingActionId || item.isExplicitlyUnassigned)
            continue;

        auto effKey = item.getEffectiveKey();
        if (compareKeyPress(key, effKey)) {
            if (outConflictingActionName)
                *outConflictingActionName = item.name;
            if (outConflictingActionId)
                *outConflictingActionId = item.id;
            return true;
        }
    }
    return false;
}

void ShortcutManager::setShortcutKey(const juce::String& actionId, const juce::KeyPress& key) {
    auto* item = getShortcut(actionId);
    if (!item)
        return;

    item->customKey = key;
    item->isExplicitlyUnassigned = !key.isValid();
    saveSettings();
    notifyChanged();
}

void ShortcutManager::clearShortcut(const juce::String& actionId) {
    auto* item = getShortcut(actionId);
    if (!item)
        return;

    item->customKey = juce::KeyPress();
    item->isExplicitlyUnassigned = true;
    saveSettings();
    notifyChanged();
}

void ShortcutManager::resetShortcutToDefault(const juce::String& actionId) {
    auto* item = getShortcut(actionId);
    if (!item)
        return;

    item->customKey = juce::KeyPress();
    item->isExplicitlyUnassigned = false;
    saveSettings();
    notifyChanged();
}

void ShortcutManager::resetAllToDefaults() {
    for (auto& item : shortcuts) {
        item.customKey = juce::KeyPress();
        item.isExplicitlyUnassigned = false;
    }
    saveSettings();
    notifyChanged();
}

juce::String ShortcutManager::formatKeyPress(const juce::KeyPress& key) {
    if (!key.isValid())
        return "Unassigned";

    juce::String s;
    auto mods = key.getModifiers();

#if JUCE_MAC
    if (mods.isCommandDown()) s += "Cmd + ";
    if (mods.isCtrlDown())    s += "Ctrl + ";
    if (mods.isAltDown())     s += "Option + ";
    if (mods.isShiftDown())   s += "Shift + ";
#else
    if (mods.isCtrlDown() || mods.isCommandDown()) s += "Ctrl + ";
    if (mods.isAltDown())                          s += "Alt + ";
    if (mods.isShiftDown())                        s += "Shift + ";
#endif

    int code = key.getKeyCode();
    if (code == juce::KeyPress::spaceKey) s += "Space";
    else if (code == juce::KeyPress::escapeKey) s += "Esc";
    else if (code == juce::KeyPress::returnKey) s += "Return";
    else if (code == juce::KeyPress::tabKey) s += "Tab";
    else if (code == juce::KeyPress::deleteKey) s += "Delete";
    else if (code == juce::KeyPress::backspaceKey) s += "Backspace";
    else if (code == juce::KeyPress::leftKey) s += "Left Arrow";
    else if (code == juce::KeyPress::rightKey) s += "Right Arrow";
    else if (code == juce::KeyPress::upKey) s += "Up Arrow";
    else if (code == juce::KeyPress::downKey) s += "Down Arrow";
    else if (code == juce::KeyPress::pageUpKey) s += "Page Up";
    else if (code == juce::KeyPress::pageDownKey) s += "Page Down";
    else if (code == juce::KeyPress::homeKey) s += "Home";
    else if (code == juce::KeyPress::endKey) s += "End";
    else if (code == juce::KeyPress::numberPadAdd) s += "NumPad +";
    else if (code == juce::KeyPress::numberPadSubtract) s += "NumPad -";
    else if (code == juce::KeyPress::numberPadMultiply) s += "NumPad *";
    else if (code == juce::KeyPress::numberPadDivide) s += "NumPad /";
    else if (code == juce::KeyPress::numberPad0) s += "NumPad 0";
    else if (code >= juce::KeyPress::numberPad1 && code <= juce::KeyPress::numberPad9)
        s += "NumPad " + juce::String(code - juce::KeyPress::numberPad1 + 1);
    else if (code >= 'a' && code <= 'z')
        s += juce::String::charToString(static_cast<juce_wchar>(code - 32));
    else if (code >= 'A' && code <= 'Z')
        s += juce::String::charToString(static_cast<juce_wchar>(code));
    else if (code >= '0' && code <= '9')
        s += juce::String::charToString(static_cast<juce_wchar>(code));
    else if (key.getTextCharacter() != 0 && key.getTextCharacter() > 32)
        s += juce::String::charToString(key.getTextCharacter());
    else if (code > 0)
        s += juce::KeyPress::createFromDescription(juce::String(code)).getTextDescription();
    else
        s += "Key";

    return s;
}

void ShortcutManager::addListener(ShortcutListener* listener) {
    listeners.add(listener);
}

void ShortcutManager::removeListener(ShortcutListener* listener) {
    listeners.remove(listener);
}

void ShortcutManager::notifyChanged() {
    listeners.call([](ShortcutListener& l) { l.shortcutsChanged(); });
}

juce::File ShortcutManager::getConfigFile() const {
    auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    auto openWavDir = appData.getChildFile("OpenWav");
    if (!openWavDir.exists())
        openWavDir.createDirectory();
    return openWavDir.getChildFile("shortcuts.json");
}

void ShortcutManager::loadSettings() {
    auto file = getConfigFile();
    if (!file.existsAsFile())
        return;

    auto parsed = juce::JSON::parse(file.loadFileAsString());
    if (!parsed.isObject())
        return;

    auto* obj = parsed.getDynamicObject();
    if (!obj)
        return;

    for (auto& item : shortcuts) {
        if (obj->hasProperty(item.id)) {
            auto val = obj->getProperty(item.id);
            if (val.isObject()) {
                auto* entry = val.getDynamicObject();
                if (entry) {
                    bool unassigned = entry->getProperty("unassigned");
                    if (unassigned) {
                        item.customKey = juce::KeyPress();
                        item.isExplicitlyUnassigned = true;
                    } else {
                        int code = entry->getProperty("keyCode");
                        int mods = entry->getProperty("modifiers");
                        int textChar = entry->getProperty("textChar");
                        item.customKey = juce::KeyPress(code, juce::ModifierKeys(mods), static_cast<juce_wchar>(textChar));
                        item.isExplicitlyUnassigned = false;
                    }
                }
            }
        }
    }
}

void ShortcutManager::saveSettings() {
    auto file = getConfigFile();
    auto* rootObj = new juce::DynamicObject();

    for (const auto& item : shortcuts) {
        if (item.isCustomized()) {
            auto* entry = new juce::DynamicObject();
            entry->setProperty("unassigned", item.isExplicitlyUnassigned);
            entry->setProperty("keyCode", item.customKey.getKeyCode());
            entry->setProperty("modifiers", item.customKey.getModifiers().getRawFlags());
            entry->setProperty("textChar", static_cast<int>(item.customKey.getTextCharacter()));
            rootObj->setProperty(item.id, juce::var(entry));
        }
    }

    auto jsonString = juce::JSON::toString(juce::var(rootObj), false);
    auto tempFile = file.getSiblingFile(file.getFileName() + ".tmp");
    if (tempFile.replaceWithText(jsonString)) {
        tempFile.moveFileTo(file);
    } else {
        file.replaceWithText(jsonString);
    }
}

} // namespace openwav
