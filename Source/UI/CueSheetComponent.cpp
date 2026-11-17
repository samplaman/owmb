#include "CueSheetComponent.h"
#include "OpenWavLookAndFeel.h"
#include "ToastOverlay.h"
#include <algorithm>
#include <cmath>

namespace openwav
{

namespace
{
class EditableCueCell : public juce::Label
{
public:
    EditableCueCell()
    {
        setEditable(false, true, false);
        setJustificationType(juce::Justification::centredLeft);
        setMouseCursor(juce::MouseCursor::IBeamCursor);
        setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
        setColour(juce::Label::textWhenEditingColourId, OpenWavLookAndFeel::textPrimary);
        setColour(juce::Label::backgroundWhenEditingColourId, OpenWavLookAndFeel::bgDark);
        setColour(juce::Label::outlineWhenEditingColourId, OpenWavLookAndFeel::accentCyan);
    }

    std::function<void(const juce::String&)> onCommitted;

    void textWasEdited() override
    {
        if (onCommitted)
            onCommitted(getText());
    }
};
} // namespace

CueSheetComponent::CueSheetComponent(AudioEngine& engine)
    : audioEngine(engine)
{
    setOpaque(true);
    setWantsKeyboardFocus(true);

    titleLabel.setFont(OpenWavLookAndFeel::getTitleFont(14.5f));
    titleLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textPrimary);
    addAndMakeVisible(titleLabel);

    summaryLabel.setFont(OpenWavLookAndFeel::getCaptionFont(11.5f));
    summaryLabel.setColour(juce::Label::textColourId, OpenWavLookAndFeel::textSecondary);
    summaryLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(summaryLabel);

    filterEditor.setJustification(juce::Justification::centredLeft);
    filterEditor.setIndents(8, 0);
    filterEditor.setTextToShowWhenEmpty("Filter cues, scenes, notes...", OpenWavLookAndFeel::textSecondary.withAlpha(0.6f));
    filterEditor.setFont(OpenWavLookAndFeel::getBodyFont(12.0f, false));
    filterEditor.setColour(juce::TextEditor::backgroundColourId, OpenWavLookAndFeel::bgDark);
    filterEditor.setColour(juce::TextEditor::textColourId, OpenWavLookAndFeel::textPrimary);
    filterEditor.setColour(juce::TextEditor::outlineColourId, OpenWavLookAndFeel::borderColour);
    filterEditor.setColour(juce::TextEditor::focusedOutlineColourId, OpenWavLookAndFeel::accentCyan);
    filterEditor.addListener(this);
    addAndMakeVisible(filterEditor);

    logCurrentButton.setTooltip("Log currently loaded sample (and active Start/End region) to Cue Sheet");
    logCurrentButton.onClick = [this] { logCurrentLoadedSample(); };
    addAndMakeVisible(logCurrentButton);

    syncSamplerButton.setTooltip("Import all zones currently mapped in Sample Map");
    syncSamplerButton.onClick = [this] {
        if (onSyncSampleMapRequested)
            onSyncSampleMapRequested();
    };
    addAndMakeVisible(syncSamplerButton);

    removeButton.setTooltip("Remove selected cue row(s)");
    removeButton.onClick = [this] { removeSelectedRows(); };
    addAndMakeVisible(removeButton);

    clearButton.setTooltip("Clear all cue sheet entries");
    clearButton.onClick = [this] { clearAllEntries(); };
    addAndMakeVisible(clearButton);

    exportCsvButton.setComponentID("accentBtn");
    exportCsvButton.setTooltip("Export Cue Sheet as CSV for Excel / Google Sheets / DAW documentation");
    exportCsvButton.onClick = [this] { exportCsv(); };
    addAndMakeVisible(exportCsvButton);

    OpenWavLookAndFeel::groupButtons({ &logCurrentButton, &syncSamplerButton, &removeButton, &clearButton });

    table.setHeader(std::make_unique<juce::TableHeaderComponent>());
    table.setModel(this);
    table.setMultipleSelectionEnabled(true);
    table.setRowHeight(32);
    table.setHeaderHeight(28);
    table.setOpaque(true);
    table.setColour(juce::ListBox::backgroundColourId, OpenWavLookAndFeel::bgCard);
    table.setColour(juce::ListBox::outlineColourId, OpenWavLookAndFeel::borderColour);
    table.setOutlineThickness(1);
    table.getHeader().setOpaque(true);

    auto& hdr = table.getHeader();
    hdr.addColumn("#",             ColIndex,      42,  36,  60,  juce::TableHeaderComponent::defaultFlags);
    hdr.addColumn("Sample / File", ColFileName,   190, 110, 400, juce::TableHeaderComponent::defaultFlags);
    hdr.addColumn("Part / Region", ColRegion,     145, 95,  260, juce::TableHeaderComponent::defaultFlags);
    hdr.addColumn("Duration",      ColDuration,   76,  60,  130, juce::TableHeaderComponent::defaultFlags);
    hdr.addColumn("Usage",         ColUsage,      115, 80,  200, juce::TableHeaderComponent::defaultFlags);
    hdr.addColumn("Scene / Track", ColSceneTrack, 145, 90,  280, juce::TableHeaderComponent::defaultFlags);
    hdr.addColumn("Notes / Cue",   ColNotes,      210, 100, 450, juce::TableHeaderComponent::defaultFlags);
    hdr.addColumn("File Path",     ColFilePath,   250, 120, 600, juce::TableHeaderComponent::defaultFlags);
    hdr.addColumn("Logged",        ColLoggedAt,   125, 95,  180, juce::TableHeaderComponent::defaultFlags);

    addAndMakeVisible(table);
    updateSummaryLabel();
    updateActionStates();
}

CueSheetComponent::~CueSheetComponent()
{
    filterEditor.removeListener(this);
    table.setModel(nullptr);
}

void CueSheetComponent::paint(juce::Graphics& g)
{
    g.fillAll(OpenWavLookAndFeel::bgDark);

    auto area = getLocalBounds().reduced(12, 10);
    auto topBar = area.removeFromTop(36);
    g.setColour(OpenWavLookAndFeel::bgCard);
    g.fillRoundedRectangle(topBar.toFloat(), 6.0f);
    g.setColour(OpenWavLookAndFeel::borderColour);
    g.drawRoundedRectangle(topBar.toFloat().reduced(0.5f), 6.0f, 1.0f);

    OpenWavLookAndFeel::drawButtonGroup(g, { &logCurrentButton, &syncSamplerButton, &removeButton, &clearButton });
}

void CueSheetComponent::paintOverChildren(juce::Graphics& g)
{
    if (entries.empty())
    {
        auto tableArea = table.getBounds().withTrimmedTop(table.getHeaderHeight());
        if (tableArea.getHeight() > 140)
        {
            OpenWavLookAndFeel::drawEmptyState(
                g, tableArea,
                "No Used Samples Logged Yet",
                "Samples added to Sample Map, edited, or logged with '+ Log Current' appear here for post-production cue sheets.");
        }
    }
}

void CueSheetComponent::resized()
{
    auto area = getLocalBounds().reduced(12, 10);
    auto topBar = area.removeFromTop(36).reduced(8, 4);

    titleLabel.setBounds(topBar.removeFromLeft(185));
    summaryLabel.setBounds(topBar.removeFromLeft(165));
    topBar.removeFromLeft(8);

    exportCsvButton.setBounds(topBar.removeFromRight(105));
    topBar.removeFromRight(8);

    clearButton.setBounds(topBar.removeFromRight(58));
    removeButton.setBounds(topBar.removeFromRight(68));
    syncSamplerButton.setBounds(topBar.removeFromRight(120));
    logCurrentButton.setBounds(topBar.removeFromRight(104));
    topBar.removeFromRight(8);

    filterEditor.setBounds(topBar);

    area.removeFromTop(8);
    table.setBounds(area);
}

bool CueSheetComponent::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::backspaceKey || key == juce::KeyPress::deleteKey)
    {
        if (table.getNumSelectedRows() > 0)
        {
            removeSelectedRows();
            return true;
        }
    }
    return Component::keyPressed(key);
}

void CueSheetComponent::logUsage(const juce::String& filePath,
                                 double inSeconds,
                                 double outSeconds,
                                 double fullDurationSeconds,
                                 bool isPartialRegion,
                                 const juce::String& sourceContext,
                                 const juce::String& defaultNotes)
{
    if (filePath.isEmpty())
        return;

    juce::File f(filePath);
    const double clampedIn = std::max(0.0, inSeconds);
    const double clampedOut = std::max(clampedIn, outSeconds);

    // Avoid exact duplicate consecutive/identical cue entries for the same file + region + source
    for (const auto& existing : entries)
    {
        if (existing.filePath == filePath
            && std::abs(existing.inSeconds - clampedIn) < 0.01
            && std::abs(existing.outSeconds - clampedOut) < 0.01
            && existing.sourceContext == sourceContext)
        {
            return;
        }
    }

    CueSheetEntryState entry;
    entry.filePath = filePath;
    entry.fileName = f.getFileName();
    entry.inSeconds = clampedIn;
    entry.outSeconds = clampedOut;
    entry.fullDurationSeconds = std::max(clampedOut, fullDurationSeconds);
    entry.isPartialRegion = isPartialRegion;
    entry.sourceContext = sourceContext;
    entry.notes = defaultNotes;
    entry.loggedAt = juce::Time::getCurrentTime().formatted("%Y-%m-%d %H:%M");

    entries.push_back(std::move(entry));
    rebuildFilteredIndices();
    updateSummaryLabel();
    updateActionStates();
    repaint();
}

void CueSheetComponent::syncFromSampleMap(const SampleMapState& mapState)
{
    int added = 0;
    for (const auto& z : mapState.zones)
    {
        if (z.filePath.isEmpty())
            continue;

        juce::String note = "MIDI " + juce::MidiMessage::getMidiNoteName(z.keyLow, true, true, 3)
                            + "-" + juce::MidiMessage::getMidiNoteName(z.keyHigh, true, true, 3)
                            + " (Root " + juce::MidiMessage::getMidiNoteName(z.rootNote, true, true, 3) + ")";

        double durSec = 0.0;
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        if (std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(juce::File(z.filePath))); reader != nullptr)
        {
            if (reader->sampleRate > 0.0)
                durSec = static_cast<double>(reader->lengthInSamples) / reader->sampleRate;
        }
        const double inSec = z.sampleStartRatio * durSec;
        const double outSec = (z.sampleEndRatio > z.sampleStartRatio ? z.sampleEndRatio : 1.0) * durSec;
        const bool isPartial = (z.sampleStartRatio > 0.001 || z.sampleEndRatio < 0.999);

        const size_t prevCount = entries.size();
        logUsage(z.filePath, inSec, outSec, durSec, isPartial, "Sample Map", note);
        if (entries.size() > prevCount)
            ++added;
    }

    if (added > 0)
        ToastOverlay::show("Synced " + juce::String(added) + " sample(s) from Sample Map");
    else
        ToastOverlay::show("Sample Map is already synced with Cue Sheet");
}

void CueSheetComponent::setState(const std::vector<CueSheetEntryState>& newEntries)
{
    entries = newEntries;
    rebuildFilteredIndices();
    updateSummaryLabel();
    updateActionStates();
    repaint();
}

int CueSheetComponent::getNumRows()
{
    return static_cast<int>(filteredIndices.size());
}

void CueSheetComponent::paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected)
{
    if (rowIsSelected)
    {
        g.fillAll(OpenWavLookAndFeel::accentCyan.withAlpha(0.18f));
        g.setColour(OpenWavLookAndFeel::accentCyan);
        g.fillRect(0, 0, 3, height);
    }
    else if (rowNumber % 2 == 1)
    {
        g.fillAll(OpenWavLookAndFeel::bgDark.withAlpha(0.35f));
    }
    else
    {
        g.fillAll(OpenWavLookAndFeel::bgCard);
    }

    g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.35f));
    g.drawHorizontalLine(height - 1, 0.0f, static_cast<float>(width));
}

void CueSheetComponent::paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(filteredIndices.size()))
        return;

    const int idx = filteredIndices[static_cast<size_t>(rowNumber)];
    const auto& entry = entries[static_cast<size_t>(idx)];

    g.setFont(OpenWavLookAndFeel::getBodyFont(12.0f, columnId == ColFileName));
    g.setColour(rowIsSelected ? juce::Colours::white : OpenWavLookAndFeel::textPrimary);

    juce::String text;
    juce::Justification just = juce::Justification::centredLeft;

    switch (columnId)
    {
        case ColIndex:
            text = juce::String(idx + 1);
            just = juce::Justification::centred;
            g.setColour(OpenWavLookAndFeel::textSecondary);
            break;
        case ColFileName:
            text = entry.fileName;
            break;
        case ColRegion:
            text = entry.isPartialRegion
                ? (formatTimecode(entry.inSeconds) + " - " + formatTimecode(entry.outSeconds))
                : "Full";
            g.setColour(entry.isPartialRegion ? OpenWavLookAndFeel::accentCyan : OpenWavLookAndFeel::textSecondary);
            break;
        case ColDuration:
            text = formatTimecode(std::max(0.0, entry.outSeconds - entry.inSeconds));
            g.setFont(OpenWavLookAndFeel::getMonoFont(11.5f, false));
            break;
        case ColUsage:
        {
            const juce::Colour badgeCol = entry.sourceContext.containsIgnoreCase("Edit")
                ? OpenWavLookAndFeel::accentBlue
                : (entry.sourceContext.containsIgnoreCase("Sample Map") || entry.sourceContext.containsIgnoreCase("Slice")
                    ? OpenWavLookAndFeel::accentCyan
                    : OpenWavLookAndFeel::textSecondary);
            auto pill = juce::Rectangle<float>(6.0f, 5.0f, std::max(20.0f, static_cast<float>(width) - 12.0f), static_cast<float>(height) - 10.0f);
            g.setColour(badgeCol.withAlpha(0.16f));
            g.fillRoundedRectangle(pill, 4.0f);
            g.setColour(badgeCol);
            g.setFont(OpenWavLookAndFeel::getSectionHeaderFont(10.0f));
            g.drawText(entry.sourceContext, pill.toNearestInt(), juce::Justification::centred, true);
            return;
        }
        case ColFilePath:
            text = entry.filePath;
            g.setColour(OpenWavLookAndFeel::textSecondary);
            break;
        case ColLoggedAt:
            text = entry.loggedAt;
            g.setColour(OpenWavLookAndFeel::textSecondary);
            break;
        default:
            return;
    }

    g.drawText(text, 6, 0, std::max(0, width - 12), height, just, true);
}

juce::Component* CueSheetComponent::refreshComponentForCell(int rowNumber, int columnId, bool /*isRowSelected*/,
                                                            juce::Component* existingComponentToUpdate)
{
    if (columnId != ColSceneTrack && columnId != ColNotes)
    {
        delete existingComponentToUpdate;
        return nullptr;
    }

    if (rowNumber < 0 || rowNumber >= static_cast<int>(filteredIndices.size()))
    {
        delete existingComponentToUpdate;
        return nullptr;
    }

    auto* cell = dynamic_cast<EditableCueCell*>(existingComponentToUpdate);
    if (cell == nullptr)
    {
        delete existingComponentToUpdate;
        cell = new EditableCueCell();
    }

    const int idx = filteredIndices[static_cast<size_t>(rowNumber)];
    auto& entry = entries[static_cast<size_t>(idx)];

    cell->setFont(OpenWavLookAndFeel::getBodyFont(12.0f, false));
    cell->setColour(juce::Label::textColourId, OpenWavLookAndFeel::textPrimary);
    cell->setText(columnId == ColSceneTrack ? entry.sceneTrack : entry.notes, juce::dontSendNotification);
    cell->setTooltip("Double-click to edit");

    cell->onCommitted = [this, idx, columnId](const juce::String& newVal) {
        if (idx < 0 || idx >= static_cast<int>(entries.size()))
            return;
        if (columnId == ColSceneTrack)
            entries[static_cast<size_t>(idx)].sceneTrack = newVal;
        else if (columnId == ColNotes)
            entries[static_cast<size_t>(idx)].notes = newVal;
    };

    return cell;
}

void CueSheetComponent::cellDoubleClicked(int rowNumber, int columnId, const juce::MouseEvent&)
{
    if (columnId == ColSceneTrack || columnId == ColNotes)
        return;
    if (rowNumber < 0 || rowNumber >= static_cast<int>(filteredIndices.size()))
        return;

    const auto& entry = entries[static_cast<size_t>(filteredIndices[static_cast<size_t>(rowNumber)])];
    if (onAuditionCue)
        onAuditionCue(entry.filePath, entry.inSeconds, entry.outSeconds, entry.isPartialRegion);
}

void CueSheetComponent::selectedRowsChanged(int /*lastRowSelected*/)
{
    updateActionStates();
}

void CueSheetComponent::sortOrderChanged(int newSortColumnId, bool isForwards)
{
    sortColumnId = newSortColumnId;
    sortForwards = isForwards;
    rebuildFilteredIndices();
}

void CueSheetComponent::textEditorTextChanged(juce::TextEditor& editor)
{
    if (&editor == &filterEditor)
        rebuildFilteredIndices();
}

void CueSheetComponent::rebuildFilteredIndices()
{
    filteredIndices.clear();
    const juce::String q = filterEditor.getText().trim().toLowerCase();

    for (int i = 0; i < static_cast<int>(entries.size()); ++i)
    {
        const auto& e = entries[static_cast<size_t>(i)];
        if (q.isEmpty()
            || e.fileName.toLowerCase().contains(q)
            || e.sourceContext.toLowerCase().contains(q)
            || e.sceneTrack.toLowerCase().contains(q)
            || e.notes.toLowerCase().contains(q)
            || e.filePath.toLowerCase().contains(q))
        {
            filteredIndices.push_back(i);
        }
    }

    if (sortColumnId != ColIndex)
    {
        std::sort(filteredIndices.begin(), filteredIndices.end(), [this](int a, int b) {
            const auto& ea = entries[static_cast<size_t>(a)];
            const auto& eb = entries[static_cast<size_t>(b)];
            int cmp = 0;
            switch (sortColumnId)
            {
                case ColFileName:   cmp = ea.fileName.compareIgnoreCase(eb.fileName); break;
                case ColRegion:     cmp = (ea.inSeconds < eb.inSeconds ? -1 : (ea.inSeconds > eb.inSeconds ? 1 : 0)); break;
                case ColDuration:
                {
                    double da = ea.outSeconds - ea.inSeconds;
                    double db = eb.outSeconds - eb.inSeconds;
                    cmp = (da < db ? -1 : (da > db ? 1 : 0));
                    break;
                }
                case ColUsage:      cmp = ea.sourceContext.compareIgnoreCase(eb.sourceContext); break;
                case ColSceneTrack: cmp = ea.sceneTrack.compareIgnoreCase(eb.sceneTrack); break;
                case ColNotes:      cmp = ea.notes.compareIgnoreCase(eb.notes); break;
                case ColFilePath:   cmp = ea.filePath.compareIgnoreCase(eb.filePath); break;
                case ColLoggedAt:   cmp = ea.loggedAt.compareIgnoreCase(eb.loggedAt); break;
                default:            cmp = (a < b ? -1 : (a > b ? 1 : 0)); break;
            }
            return sortForwards ? (cmp < 0) : (cmp > 0);
        });
    }
    else if (!sortForwards)
    {
        std::reverse(filteredIndices.begin(), filteredIndices.end());
    }

    table.updateContent();
    table.repaint();
}

void CueSheetComponent::updateSummaryLabel()
{
    double totalSecs = 0.0;
    for (const auto& e : entries)
        totalSecs += std::max(0.0, e.outSeconds - e.inSeconds);

    summaryLabel.setText(juce::String(entries.size()) + (entries.size() == 1 ? " cue  |  " : " cues  |  ")
                             + formatTimecode(totalSecs),
                         juce::dontSendNotification);
}

void CueSheetComponent::updateActionStates()
{
    const bool hasEntries = !entries.empty();
    removeButton.setEnabled(table.getNumSelectedRows() > 0);
    clearButton.setEnabled(hasEntries);
    exportCsvButton.setEnabled(hasEntries);
}

void CueSheetComponent::logCurrentLoadedSample()
{
    const auto& f = audioEngine.getCurrentFile();
    const double totalDur = audioEngine.getTotalLengthSeconds();
    if (!f.existsAsFile() || totalDur <= 0.0)
    {
        ToastOverlay::show("Load a sample first to log it to the Cue Sheet", true);
        return;
    }

    const double startR = audioEngine.getSampleStartRatio();
    const double endR = audioEngine.getSampleEndRatio();
    const double inSec = startR * totalDur;
    const double outSec = endR * totalDur;
    const bool isPartial = (startR > 0.001 || endR < 0.999);

    logUsage(f.getFullPathName(), inSec, outSec, totalDur, isPartial, "Manual Log");
    ToastOverlay::show("Logged to Cue Sheet: " + f.getFileName());
}

void CueSheetComponent::removeSelectedRows()
{
    auto sel = table.getSelectedRows();
    if (sel.isEmpty())
        return;

    std::vector<int> toErase;
    for (int i = 0; i < sel.size(); ++i)
    {
        int row = sel[i];
        if (row >= 0 && row < static_cast<int>(filteredIndices.size()))
            toErase.push_back(filteredIndices[static_cast<size_t>(row)]);
    }

    std::sort(toErase.rbegin(), toErase.rend());
    toErase.erase(std::unique(toErase.begin(), toErase.end()), toErase.end());
    for (int idx : toErase)
    {
        if (idx >= 0 && idx < static_cast<int>(entries.size()))
            entries.erase(entries.begin() + idx);
    }

    table.deselectAllRows();
    rebuildFilteredIndices();
    updateSummaryLabel();
    updateActionStates();
    repaint();
}

void CueSheetComponent::clearAllEntries()
{
    if (entries.empty())
        return;

    entries.clear();
    table.deselectAllRows();
    rebuildFilteredIndices();
    updateSummaryLabel();
    updateActionStates();
    repaint();
}

void CueSheetComponent::exportCsv()
{
    if (entries.empty())
    {
        ToastOverlay::show("Cue Sheet is empty", true);
        return;
    }

    auto chooser = std::make_shared<juce::FileChooser>(
        "Export Cue Sheet CSV",
        juce::File::getSpecialLocation(juce::File::userDesktopDirectory).getChildFile("project_cue_sheet.csv"),
        "*.csv");

    chooser->launchAsync(
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser](const juce::FileChooser& fc)
        {
            auto target = fc.getResult();
            if (target == juce::File())
                return;

            if (!target.hasFileExtension("csv"))
                target = target.withFileExtension("csv");

            juce::String csv;
            csv << "Cue,Sample,Part/Region,In (s),Out (s),Duration (s),Usage,Scene/Track,Notes,File Path,Logged\n";

            for (size_t i = 0; i < entries.size(); ++i)
            {
                const auto& e = entries[i];
                const juce::String regionStr = e.isPartialRegion
                    ? (formatTimecode(e.inSeconds) + " - " + formatTimecode(e.outSeconds))
                    : "Full";
                const double dur = std::max(0.0, e.outSeconds - e.inSeconds);

                csv << juce::String(i + 1) << ","
                    << escapeCsvField(e.fileName) << ","
                    << escapeCsvField(regionStr) << ","
                    << juce::String(e.inSeconds, 3) << ","
                    << juce::String(e.outSeconds, 3) << ","
                    << juce::String(dur, 3) << ","
                    << escapeCsvField(e.sourceContext) << ","
                    << escapeCsvField(e.sceneTrack) << ","
                    << escapeCsvField(e.notes) << ","
                    << escapeCsvField(e.filePath) << ","
                    << escapeCsvField(e.loggedAt) << "\n";
            }

            if (target.replaceWithText(csv))
                ToastOverlay::show("Exported Cue Sheet CSV: " + target.getFileName());
            else
                ToastOverlay::show("Failed to write CSV file", true);
        });
}

juce::String CueSheetComponent::formatTimecode(double seconds)
{
    const double clamped = std::max(0.0, seconds);
    const int mins = static_cast<int>(clamped) / 60;
    const double rem = clamped - static_cast<double>(mins * 60);
    return juce::String::formatted("%d:%05.2f", mins, rem);
}

juce::String CueSheetComponent::escapeCsvField(const juce::String& field)
{
    if (field.containsAnyOf(",\"\r\n"))
        return "\"" + field.replace("\"", "\"\"") + "\"";
    return field;
}

} // namespace openwav
