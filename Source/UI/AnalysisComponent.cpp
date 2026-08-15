#include "AnalysisComponent.h"
#include "OpenWavLookAndFeel.h"

namespace openwav
{

AnalysisComponent::AnalysisComponent()
{
    setOpaque(true);
}

void AnalysisComponent::setItem(const MediaItem& item)
{
    currentItem = item;
    hasItem = true;
    repaint();
}

void AnalysisComponent::clearItem()
{
    hasItem = false;
    repaint();
}

// ─────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────
juce::String AnalysisComponent::formatFileSize(int64_t bytes)
{
    if (bytes < 1024)
        return juce::String(bytes) + " B";
    if (bytes < 1024 * 1024)
        return juce::String(bytes / 1024.0, 1) + " KB";
    if (bytes < 1024 * 1024 * 1024)
        return juce::String(bytes / (1024.0 * 1024.0), 2) + " MB";
    return juce::String(bytes / (1024.0 * 1024.0 * 1024.0), 2) + " GB";
}

juce::String AnalysisComponent::formatDateAdded(int64_t timestampMs)
{
    if (timestampMs <= 0)
        return "Unknown";
    auto t = juce::Time(timestampMs);
    return t.formatted("%Y-%m-%d %H:%M");
}

// ─────────────────────────────────────────────────────────
//  Paint
// ─────────────────────────────────────────────────────────
void AnalysisComponent::paint(juce::Graphics& g)
{
    g.fillAll(OpenWavLookAndFeel::bgDark);

    if (!hasItem)
    {
        g.setColour(OpenWavLookAndFeel::textSecondary.withAlpha(0.5f));
        g.setFont(juce::Font(20.0f));
        g.drawText("No file selected", getLocalBounds(), juce::Justification::centred);
        return;
    }

    auto bounds = getLocalBounds().toFloat().reduced(20.0f, 16.0f);

    // ── Title bar ──────────────────────────────────────
    auto titleArea = bounds.removeFromTop(48.0f);
    g.setColour(OpenWavLookAndFeel::accentCyan);
    g.setFont(juce::Font(22.0f).boldened());
    g.drawText(currentItem.fileName, titleArea, juce::Justification::centredLeft);

    g.setColour(OpenWavLookAndFeel::textSecondary);
    g.setFont(juce::Font(12.0f));
    g.drawText(currentItem.cachedUppercaseExtension + "  |  " + formatFileSize(currentItem.fileSizeBytes),
               titleArea, juce::Justification::centredRight);

    // Separator
    bounds.removeFromTop(4.0f);
    g.setColour(OpenWavLookAndFeel::borderColour);
    g.fillRect(bounds.removeFromTop(1.0f));
    bounds.removeFromTop(16.0f);

    // ── Two-column layout ──────────────────────────────
    float columnGap = 32.0f;
    float leftWidth = bounds.getWidth() * 0.55f - columnGap * 0.5f;
    float rightWidth = bounds.getWidth() * 0.45f - columnGap * 0.5f;

    auto leftCol = bounds.removeFromLeft(leftWidth);
    bounds.removeFromLeft(columnGap);
    auto rightCol = bounds;

    paintInfoSection(g, leftCol);
    paintRadarChart(g, rightCol);
}

// ─────────────────────────────────────────────────────────
//  Left column: info cards
// ─────────────────────────────────────────────────────────
void AnalysisComponent::paintInfoSection(juce::Graphics& g, juce::Rectangle<float> area) const
{
    auto paintCard = [&](const juce::String& title, const std::vector<std::pair<juce::String, juce::String>>& rows,
                         juce::Rectangle<float>& cardArea)
    {
        float cardHeight = 28.0f + rows.size() * 24.0f + 16.0f;
        auto card = cardArea.removeFromTop(cardHeight);
        cardArea.removeFromTop(12.0f);

        // Card background
        g.setColour(OpenWavLookAndFeel::bgCard);
        g.fillRoundedRectangle(card, 8.0f);
        g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.4f));
        g.drawRoundedRectangle(card, 8.0f, 1.0f);

        auto inner = card.reduced(16.0f, 8.0f);

        // Card title
        g.setColour(OpenWavLookAndFeel::accentCyan);
        g.setFont(juce::Font(14.0f).boldened());
        g.drawText(title, inner.removeFromTop(24.0f), juce::Justification::centredLeft);

        // Rows
        g.setFont(juce::Font(13.0f));
        for (const auto& [label, value] : rows)
        {
            auto rowArea = inner.removeFromTop(24.0f);
            g.setColour(OpenWavLookAndFeel::textSecondary);
            g.drawText(label, rowArea.removeFromLeft(160.0f), juce::Justification::centredLeft);
            g.setColour(OpenWavLookAndFeel::textPrimary);
            g.drawText(value, rowArea, juce::Justification::centredLeft);
        }
    };

    // ── File Info Card ──
    {
        std::vector<std::pair<juce::String, juce::String>> rows;
        rows.push_back({ "File Name", currentItem.fileName });
        rows.push_back({ "Path", currentItem.filePath });
        rows.push_back({ "Format", currentItem.cachedUppercaseExtension });
        rows.push_back({ "File Size", formatFileSize(currentItem.fileSizeBytes) });
        rows.push_back({ "Date Added", formatDateAdded(currentItem.dateAddedMs) });
        paintCard("File Information", rows, area);
    }

    // ── Audio Properties Card ──
    {
        std::vector<std::pair<juce::String, juce::String>> rows;
        rows.push_back({ "Duration", currentItem.cachedFormattedDuration });
        rows.push_back({ "Sample Rate", currentItem.cachedFormattedSampleRate });
        rows.push_back({ "Bit Depth", currentItem.bitDepth > 0 ? juce::String(currentItem.bitDepth) + "-bit" : "N/A" });
        rows.push_back({ "Channels", currentItem.numChannels == 1 ? "Mono" : (currentItem.numChannels == 2 ? "Stereo" : juce::String(currentItem.numChannels) + " ch") });
        rows.push_back({ "BPM", currentItem.bpm > 0.0 ? juce::String(currentItem.bpm, 1) : "N/A" });
        paintCard("Audio Properties", rows, area);
    }

    // ── Rating & Favourite Card ──
    {
        std::vector<std::pair<juce::String, juce::String>> rows;
        rows.push_back({ "Rating", currentItem.cachedStarRating.isEmpty() ? "Not rated" : currentItem.cachedStarRating });
        rows.push_back({ "Favourite", currentItem.isFavorite ? juce::String::fromUTF8("\xe2\x9d\xa4 Yes") : "No" });
        if (currentItem.comment.isNotEmpty())
            rows.push_back({ "Comment", currentItem.comment });
        paintCard("Rating & Notes", rows, area);
    }

    // ── Tags Card ──
    if (!currentItem.tags.empty())
    {
        float tagRowHeight = 30.0f;
        float tagCardHeight = 28.0f + tagRowHeight + 20.0f;
        // Estimate tag rows: ~80px per tag, wrap
        int tagsPerRow = juce::jmax(1, static_cast<int>(area.getWidth() - 32.0f) / 90);
        int numTagRows = (static_cast<int>(currentItem.tags.size()) + tagsPerRow - 1) / tagsPerRow;
        tagCardHeight = 28.0f + numTagRows * (tagRowHeight + 4.0f) + 20.0f;

        auto card = area.removeFromTop(tagCardHeight);
        area.removeFromTop(12.0f);

        g.setColour(OpenWavLookAndFeel::bgCard);
        g.fillRoundedRectangle(card, 8.0f);
        g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.4f));
        g.drawRoundedRectangle(card, 8.0f, 1.0f);

        auto inner = card.reduced(16.0f, 8.0f);

        g.setColour(OpenWavLookAndFeel::accentCyan);
        g.setFont(juce::Font(14.0f).boldened());
        g.drawText("Tags", inner.removeFromTop(24.0f), juce::Justification::centredLeft);
        inner.removeFromTop(4.0f);

        paintTagsSection(g, inner);
    }
}

// ─────────────────────────────────────────────────────────
//  Tags as pill badges
// ─────────────────────────────────────────────────────────
void AnalysisComponent::paintTagsSection(juce::Graphics& g, juce::Rectangle<float> area) const
{
    g.setFont(juce::Font(12.0f));
    float x = area.getX();
    float y = area.getY();
    float maxX = area.getRight();
    float pillHeight = 24.0f;
    float hGap = 6.0f;
    float vGap = 6.0f;

    for (const auto& tag : currentItem.tags)
    {
        float textWidth = juce::Font(12.0f).getStringWidthFloat(tag);
        float pillWidth = textWidth + 20.0f;

        if (x + pillWidth > maxX && x > area.getX())
        {
            x = area.getX();
            y += pillHeight + vGap;
        }

        auto pill = juce::Rectangle<float>(x, y, pillWidth, pillHeight);

        g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.15f));
        g.fillRoundedRectangle(pill, pillHeight * 0.5f);
        g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.6f));
        g.drawRoundedRectangle(pill, pillHeight * 0.5f, 1.0f);

        g.setColour(OpenWavLookAndFeel::accentCyan);
        g.drawText(tag, pill, juce::Justification::centred);

        x += pillWidth + hGap;
    }
}

// ─────────────────────────────────────────────────────────
//  Right column: Radar chart + descriptor bars
// ─────────────────────────────────────────────────────────
void AnalysisComponent::paintRadarChart(juce::Graphics& g, juce::Rectangle<float> area) const
{
    // Card background
    auto cardArea = area.withHeight(juce::jmin(area.getHeight(), 500.0f));
    g.setColour(OpenWavLookAndFeel::bgCard);
    g.fillRoundedRectangle(cardArea, 8.0f);
    g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.4f));
    g.drawRoundedRectangle(cardArea, 8.0f, 1.0f);

    auto inner = cardArea.reduced(16.0f, 12.0f);

    // Title
    g.setColour(OpenWavLookAndFeel::accentCyan);
    g.setFont(juce::Font(14.0f).boldened());
    g.drawText("Acoustic Fingerprint", inner.removeFromTop(24.0f), juce::Justification::centredLeft);
    inner.removeFromTop(8.0f);

    // ── Radar Chart ──────────────────────────────────
    struct Descriptor
    {
        juce::String name;
        double value;
        double maxVal;
    };

    std::vector<Descriptor> descriptors = {
        { "ZCR",       currentItem.zcr,           0.5  },
        { "HF Ratio",  currentItem.highFreqRatio,  1.0  },
        { "Decay",     currentItem.decayRatio,     1.0  },
        { "Crest",     currentItem.crestFactor,    30.0 }
    };

    int numAxes = static_cast<int>(descriptors.size());
    float chartSize = juce::jmin(inner.getWidth(), inner.getHeight() - 120.0f);
    chartSize = juce::jmax(chartSize, 120.0f);
    float radius = chartSize * 0.40f;
    float cx = inner.getCentreX();
    float cy = inner.getY() + chartSize * 0.5f + 8.0f;
    float angleStep = juce::MathConstants<float>::twoPi / numAxes;

    // Grid rings
    for (int ring = 1; ring <= 4; ++ring)
    {
        float r = radius * ring / 4.0f;
        juce::Path ringPath;
        for (int i = 0; i <= numAxes; ++i)
        {
            float angle = i * angleStep - juce::MathConstants<float>::halfPi;
            float px = cx + std::cos(angle) * r;
            float py = cy + std::sin(angle) * r;
            if (i == 0)
                ringPath.startNewSubPath(px, py);
            else
                ringPath.lineTo(px, py);
        }
        ringPath.closeSubPath();
        g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.25f));
        g.strokePath(ringPath, juce::PathStrokeType(0.5f));
    }

    // Axis lines + labels
    g.setFont(juce::Font(11.0f));
    for (int i = 0; i < numAxes; ++i)
    {
        float angle = i * angleStep - juce::MathConstants<float>::halfPi;
        float axX = cx + std::cos(angle) * radius;
        float axY = cy + std::sin(angle) * radius;

        g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.4f));
        g.drawLine(cx, cy, axX, axY, 0.5f);

        // Label
        float labelDist = radius + 18.0f;
        float lx = cx + std::cos(angle) * labelDist;
        float ly = cy + std::sin(angle) * labelDist;
        g.setColour(OpenWavLookAndFeel::textSecondary);
        g.drawText(descriptors[i].name,
                   juce::Rectangle<float>(lx - 40.0f, ly - 8.0f, 80.0f, 16.0f),
                   juce::Justification::centred);
    }

    // Data polygon
    juce::Path dataPath;
    for (int i = 0; i <= numAxes; ++i)
    {
        int idx = i % numAxes;
        float norm = static_cast<float>(juce::jlimit(0.0, 1.0, descriptors[idx].value / descriptors[idx].maxVal));
        float r = norm * radius;
        float angle = idx * angleStep - juce::MathConstants<float>::halfPi;
        float px = cx + std::cos(angle) * r;
        float py = cy + std::sin(angle) * r;
        if (i == 0)
            dataPath.startNewSubPath(px, py);
        else
            dataPath.lineTo(px, py);
    }
    dataPath.closeSubPath();

    // Filled polygon
    g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.15f));
    g.fillPath(dataPath);
    g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.8f));
    g.strokePath(dataPath, juce::PathStrokeType(2.0f));

    // Data points (dots)
    for (int i = 0; i < numAxes; ++i)
    {
        float norm = static_cast<float>(juce::jlimit(0.0, 1.0, descriptors[i].value / descriptors[i].maxVal));
        float r = norm * radius;
        float angle = i * angleStep - juce::MathConstants<float>::halfPi;
        float px = cx + std::cos(angle) * r;
        float py = cy + std::sin(angle) * r;
        g.setColour(OpenWavLookAndFeel::accentCyan);
        g.fillEllipse(px - 4.0f, py - 4.0f, 8.0f, 8.0f);
    }

    // ── Descriptor Bars below the chart ──────────────
    float barAreaTop = cy + chartSize * 0.5f + 32.0f;
    auto barArea = inner.withTop(barAreaTop);
    float barHeight = 18.0f;
    float barGap = 8.0f;

    g.setFont(juce::Font(12.0f));
    for (const auto& desc : descriptors)
    {
        if (barArea.getHeight() < barHeight + barGap)
            break;

        auto rowArea = barArea.removeFromTop(barHeight);
        barArea.removeFromTop(barGap);

        // Label
        auto labelRect = rowArea.removeFromLeft(80.0f);
        g.setColour(OpenWavLookAndFeel::textSecondary);
        g.drawText(desc.name, labelRect, juce::Justification::centredLeft);

        // Value text
        auto valueRect = rowArea.removeFromRight(70.0f);
        g.setColour(OpenWavLookAndFeel::textPrimary);
        g.drawText(juce::String(desc.value, 4), valueRect, juce::Justification::centredRight);

        // Bar track
        auto trackRect = rowArea.reduced(4.0f, 3.0f);
        g.setColour(OpenWavLookAndFeel::bgDark);
        g.fillRoundedRectangle(trackRect, 4.0f);

        // Bar fill
        float norm = static_cast<float>(juce::jlimit(0.0, 1.0, desc.value / desc.maxVal));
        auto fillRect = trackRect.withWidth(trackRect.getWidth() * norm);
        g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.7f));
        g.fillRoundedRectangle(fillRect, 4.0f);
    }
}

void AnalysisComponent::resized()
{
    // All painting is done in paint(), nothing to lay out.
}

} // namespace openwav
