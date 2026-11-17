#pragma once

#include "OpenWavLookAndFeel.h"
#include "../Database/TagDatabaseManager.h"
#include <algorithm>
#include <functional>
#include <vector>

namespace openwav
{

class CommandPaletteOverlay : public juce::Component,
                              private juce::TextEditor::Listener,
                              private juce::KeyListener,
                              private juce::ListBoxModel
{
public:
    struct CommandEntry
    {
        juce::String category; // "VIEW", "ACTION", "SAMPLE"
        juce::String title;
        juce::String subtitle;
        juce::String shortcut;
        std::function<void()> action;
    };

    explicit CommandPaletteOverlay(TagDatabaseManager& db)
        : dbManager(db)
    {
        setWantsKeyboardFocus(true);
        setVisible(false);

        searchBox.setJustification(juce::Justification::centredLeft);
        searchBox.setIndents(10, 0);
        searchBox.setFont(OpenWavLookAndFeel::getBodyFont(13.5f, false));
        searchBox.setColour(juce::TextEditor::backgroundColourId, OpenWavLookAndFeel::bgDark);
        searchBox.setColour(juce::TextEditor::textColourId, OpenWavLookAndFeel::textPrimary);
        searchBox.setColour(juce::TextEditor::outlineColourId, OpenWavLookAndFeel::borderColour);
        searchBox.setColour(juce::TextEditor::focusedOutlineColourId, OpenWavLookAndFeel::accentCyan);
        searchBox.setTextToShowWhenEmpty("Type a view, action, or sample name...", OpenWavLookAndFeel::textSecondary.withAlpha(0.65f));
        searchBox.addListener(this);
        searchBox.addKeyListener(this);
        addAndMakeVisible(searchBox);

        listBox.setModel(this);
        listBox.setRowHeight(36);
        listBox.setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        listBox.setOutlineThickness(0);
        addAndMakeVisible(listBox);
    }

    ~CommandPaletteOverlay() override
    {
        searchBox.removeListener(this);
        searchBox.removeKeyListener(this);
    }

    void setStaticCommands(std::vector<CommandEntry> cmds)
    {
        staticCommands = std::move(cmds);
    }

    std::function<void(const MediaItem&)> onSampleChosen;

    void open()
    {
        searchBox.setText({}, juce::dontSendNotification);
        rebuildFilter({});
        setVisible(true);
        toFront(true);
        resized();
        searchBox.grabKeyboardFocus();
    }

    void close()
    {
        setVisible(false);
        if (auto* parent = getParentComponent())
            parent->grabKeyboardFocus();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black.withAlpha(0.55f));

        auto card = getCardBounds().toFloat();
        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.fillRoundedRectangle(card.translated(0.0f, 6.0f), 10.0f);

        g.setColour(OpenWavLookAndFeel::bgHeader);
        g.fillRoundedRectangle(card, 10.0f);
        g.setColour(OpenWavLookAndFeel::borderColour);
        g.drawRoundedRectangle(card.reduced(0.5f), 10.0f, 1.0f);

        // Footer hint
        auto footer = card.removeFromBottom(24.0f).reduced(14.0f, 0.0f);
        g.setColour(OpenWavLookAndFeel::textSecondary.withAlpha(0.7f));
        g.setFont(OpenWavLookAndFeel::getCaptionFont(10.5f));
        g.drawText("Up/Down to navigate   |   Enter to run   |   Esc to close",
                   footer.toNearestInt(), juce::Justification::centredLeft, true);
    }

    void resized() override
    {
        auto card = getCardBounds();
        auto inner = card.reduced(12, 12);
        searchBox.setBounds(inner.removeFromTop(36));
        inner.removeFromTop(8);
        inner.removeFromBottom(20);
        listBox.setBounds(inner);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (!getCardBounds().contains(e.getPosition()))
            close();
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey)
        {
            close();
            return true;
        }
        return false;
    }

private:
    juce::Rectangle<int> getCardBounds() const
    {
        int w = std::min(560, std::max(320, getWidth() - 40));
        int h = std::min(400, std::max(240, getHeight() - 80));
        int x = (getWidth() - w) / 2;
        int y = std::max(32, (getHeight() - h) / 4);
        return { x, y, w, h };
    }

    void textEditorTextChanged(juce::TextEditor& editor) override
    {
        if (&editor == &searchBox)
            rebuildFilter(searchBox.getText());
    }

    bool keyPressed(const juce::KeyPress& key, juce::Component* /*originatingComponent*/) override
    {
        if (key == juce::KeyPress::escapeKey)
        {
            close();
            return true;
        }
        if (key == juce::KeyPress::downKey)
        {
            if (!filtered.empty())
            {
                int next = std::min(static_cast<int>(filtered.size()) - 1, listBox.getSelectedRow() + 1);
                listBox.selectRow(next);
            }
            return true;
        }
        if (key == juce::KeyPress::upKey)
        {
            if (!filtered.empty())
            {
                int prev = std::max(0, listBox.getSelectedRow() - 1);
                listBox.selectRow(prev);
            }
            return true;
        }
        if (key == juce::KeyPress::returnKey)
        {
            executeRow(listBox.getSelectedRow());
            return true;
        }
        return false;
    }

    void rebuildFilter(const juce::String& query)
    {
        filtered.clear();
        juce::String q = query.trim().toLowerCase();

        for (const auto& cmd : staticCommands)
        {
            if (q.isEmpty() ||
                cmd.title.toLowerCase().contains(q) ||
                cmd.subtitle.toLowerCase().contains(q) ||
                cmd.category.toLowerCase().contains(q))
            {
                filtered.push_back(cmd);
            }
        }

        if (q.length() >= 2)
        {
            auto allItems = dbManager.getAllItems();
            int addedSamples = 0;
            for (const auto& item : allItems)
            {
                if (item.cachedLowerFileName.contains(q) || item.cachedLowerFilePath.contains(q))
                {
                    CommandEntry entry;
                    entry.category = "SAMPLE";
                    entry.title = item.fileName;
                    entry.subtitle = juce::File(item.filePath).getParentDirectory().getFileName();
                    entry.action = [this, item] {
                        if (onSampleChosen)
                            onSampleChosen(item);
                    };
                    filtered.push_back(std::move(entry));
                    if (++addedSamples >= 25)
                        break;
                }
            }
        }

        listBox.updateContent();
        if (!filtered.empty())
            listBox.selectRow(0);
        repaint();
    }

    void executeRow(int row)
    {
        if (row < 0 || row >= static_cast<int>(filtered.size()))
            return;
        auto fn = filtered[static_cast<size_t>(row)].action;
        close();
        if (fn)
            fn();
    }

    int getNumRows() override
    {
        return static_cast<int>(filtered.size());
    }

    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override
    {
        if (rowNumber < 0 || rowNumber >= static_cast<int>(filtered.size()))
            return;

        const auto& item = filtered[static_cast<size_t>(rowNumber)];
        auto bounds = juce::Rectangle<float>(2.0f, 1.0f, static_cast<float>(width) - 4.0f, static_cast<float>(height) - 2.0f);

        if (rowIsSelected)
        {
            g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.18f));
            g.fillRoundedRectangle(bounds, 6.0f);
            g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.65f));
            g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);
        }

        auto row = bounds.reduced(8.0f, 4.0f);

        // Category pill
        auto badge = row.removeFromLeft(56.0f).reduced(0.0f, 3.0f);
        juce::Colour badgeCol = (item.category == "VIEW")
                                    ? OpenWavLookAndFeel::accentCyan
                                    : (item.category == "SAMPLE" ? OpenWavLookAndFeel::accentBlue : OpenWavLookAndFeel::textSecondary);
        g.setColour(badgeCol.withAlpha(0.16f));
        g.fillRoundedRectangle(badge, 4.0f);
        g.setColour(badgeCol);
        g.setFont(OpenWavLookAndFeel::getSectionHeaderFont(9.5f));
        g.drawText(item.category, badge.toNearestInt(), juce::Justification::centred, false);

        row.removeFromLeft(10.0f);

        if (item.shortcut.isNotEmpty())
        {
            auto scArea = row.removeFromRight(68.0f);
            g.setColour(OpenWavLookAndFeel::textSecondary.withAlpha(0.8f));
            g.setFont(OpenWavLookAndFeel::getMonoFont(10.5f, false));
            g.drawText(item.shortcut, scArea.toNearestInt(), juce::Justification::centredRight, true);
        }

        g.setColour(rowIsSelected ? juce::Colours::white : OpenWavLookAndFeel::textPrimary);
        g.setFont(OpenWavLookAndFeel::getBodyFont(12.5f, true));
        if (item.subtitle.isNotEmpty())
        {
            auto top = row.removeFromTop(row.getHeight() * 0.56f);
            g.drawText(item.title, top.toNearestInt(), juce::Justification::centredLeft, true);
            g.setColour(OpenWavLookAndFeel::textSecondary);
            g.setFont(OpenWavLookAndFeel::getCaptionFont(10.5f));
            g.drawText(item.subtitle, row.toNearestInt(), juce::Justification::centredLeft, true);
        }
        else
        {
            g.drawText(item.title, row.toNearestInt(), juce::Justification::centredLeft, true);
        }
    }

    void listBoxItemClicked(int row, const juce::MouseEvent&) override
    {
        executeRow(row);
    }

    TagDatabaseManager& dbManager;
    juce::TextEditor searchBox;
    juce::ListBox listBox;
    std::vector<CommandEntry> staticCommands;
    std::vector<CommandEntry> filtered;
};

} // namespace openwav
