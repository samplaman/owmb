#include "SampleCloudComponent.h"
#include "OpenWavLookAndFeel.h"
#include <cmath>

namespace openwav
{

SampleCloudComponent::SampleCloudComponent(TagDatabaseManager& db, AudioEngine& engine)
    : dbManager(db), audioEngine(engine)
{
    zoomInButton.onClick = [this] {
        zoomScale = juce::jlimit(0.3f, 4.0f, zoomScale * 1.25f);
        repaint();
    };
    zoomOutButton.onClick = [this] {
        zoomScale = juce::jlimit(0.3f, 4.0f, zoomScale * 0.8f);
        repaint();
    };
    resetZoomButton.onClick = [this] {
        resetZoomAndPan();
    };

    addAndMakeVisible(zoomInButton);
    addAndMakeVisible(zoomOutButton);
    addAndMakeVisible(resetZoomButton);

    startTimerHz(60);
}

SampleCloudComponent::~SampleCloudComponent()
{
    stopTimer();
}

juce::Point<float> SampleCloudComponent::cloudToScreen(juce::Point<float> cloudPos) const
{
    auto center = getLocalBounds().getCentre().toFloat();
    return (cloudPos - center) * zoomScale + center + panOffset;
}

juce::Point<float> SampleCloudComponent::screenToCloud(juce::Point<float> screenPos) const
{
    auto center = getLocalBounds().getCentre().toFloat();
    return (screenPos - center - panOffset) / zoomScale + center;
}

void SampleCloudComponent::resetZoomAndPan()
{
    zoomScale = 1.0f;
    panOffset = { 0.0f, 0.0f };
    repaint();
}

void SampleCloudComponent::paint(juce::Graphics& g)
{
    g.fillAll(OpenWavLookAndFeel::bgDark);

    if (nodes.empty())
    {
        g.setFont(juce::Font(14.0f));
        g.setColour(OpenWavLookAndFeel::textSecondary);
        g.drawText("No samples found. Add a folder or adjust filters.", getLocalBounds(), juce::Justification::centred, true);
        return;
    }

    // 1. Draw 2D Tactical Cartography Map Grid & Coordinates (instead of circular radar rings)
    float gridStep = 140.0f * zoomScale;

    float startX = std::fmod(panOffset.x, gridStep);
    if (startX < 0) startX += gridStep;
    float startY = std::fmod(panOffset.y, gridStep);
    if (startY < 0) startY += gridStep;

    g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.08f));
    for (float x = startX; x < getWidth(); x += gridStep)
    {
        g.drawLine(x, 0.0f, x, static_cast<float>(getHeight()), 1.0f);
    }
    for (float y = startY; y < getHeight(); y += gridStep)
    {
        g.drawLine(0.0f, y, static_cast<float>(getWidth()), y, 1.0f);
    }

    // Grid Intersection Crosshairs
    g.setColour(OpenWavLookAndFeel::textSecondary.withAlpha(0.18f));
    for (float x = startX; x < getWidth(); x += gridStep)
    {
        for (float y = startY; y < getHeight(); y += gridStep)
        {
            g.drawLine(x - 3.0f, y, x + 3.0f, y, 1.0f);
            g.drawLine(x, y - 3.0f, x, y + 3.0f, 1.0f);
        }
    }

    // 2. Draw Topographic Territory Islands & Elevation Contours
    for (const auto& cluster : clusters)
    {
        auto screenClusterPos = cloudToScreen(cluster.centerPos);
        float baseRadius = std::max(45.0f * zoomScale, (25.0f + std::sqrt(static_cast<float>(cluster.count)) * 14.0f) * zoomScale);

        // Soft island land mass fill
        g.setColour(cluster.colour.withAlpha(0.06f));
        g.fillEllipse(screenClusterPos.x - baseRadius, screenClusterPos.y - baseRadius, baseRadius * 2.0f, baseRadius * 2.0f);

        // Outer Contour Line 1 (Sea level elevation)
        g.setColour(cluster.colour.withAlpha(0.18f));
        g.drawEllipse(screenClusterPos.x - baseRadius, screenClusterPos.y - baseRadius, baseRadius * 2.0f, baseRadius * 2.0f, 1.2f);

        // Mid Contour Line 2
        float r2 = baseRadius * 0.68f;
        g.setColour(cluster.colour.withAlpha(0.28f));
        g.drawEllipse(screenClusterPos.x - r2, screenClusterPos.y - r2, r2 * 2.0f, r2 * 2.0f, 1.0f);

        // Center Peak Contour Line 3
        float r3 = baseRadius * 0.38f;
        g.setColour(cluster.colour.withAlpha(0.38f));
        g.drawEllipse(screenClusterPos.x - r3, screenClusterPos.y - r3, r3 * 2.0f, r3 * 2.0f, 0.8f);
    }

    // 4. Draw all sample nodes with flat colors (highly optimized, no gradients)
    for (size_t i = 0; i < nodes.size(); ++i)
    {
        const auto& node = nodes[i];
        bool isHovered = (static_cast<int>(i) == hoveredNodeIndex);
        bool isSelected = (static_cast<int>(i) == selectedNodeIndex);

        auto screenPos = cloudToScreen(node.currentPos);
        float baseR = node.radius * node.hoverScale;
        float r = std::max(1.5f, baseR * zoomScale);

        // Soft outer glow halo - flat alpha fill (extremely fast)
        if (node.hoverScale > 1.0f || isSelected)
        {
            float glowR = r * 1.5f;
            float hoverGlowAlpha = (node.hoverScale - 1.0f) / 1.2f; // scale factor
            float alpha = isSelected ? 0.15f : (0.25f * hoverGlowAlpha);
            if (alpha > 0.0f)
            {
                g.setColour(node.colour.withAlpha(alpha));
                g.fillEllipse(screenPos.x - glowR, screenPos.y - glowR, glowR * 2.0f, glowR * 2.0f);
            }
        }

        // Flat Color Fill (No gradient - extremely fast!)
        juce::Colour nodeColor = node.colour;
        if (isHovered)
            nodeColor = node.colour.brighter(0.25f);
        else if (isSelected)
            nodeColor = OpenWavLookAndFeel::accentCyan;

        g.setColour(nodeColor);
        g.fillEllipse(screenPos.x - r, screenPos.y - r, r * 2.0f, r * 2.0f);

        // Flat outer ring highlight if hovered or selected (fast outline)
        if (isHovered || isSelected)
        {
            g.setColour(isHovered ? juce::Colours::white.withAlpha(0.6f) : OpenWavLookAndFeel::accentCyan.withAlpha(0.8f));
            g.drawEllipse(screenPos.x - r, screenPos.y - r, r * 2.0f, r * 2.0f, 1.0f);
        }

        // Selected node outer highlight ring (static - no timer repaint overhead)
        if (isSelected)
        {
            float ringR = r + 4.0f;
            g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.8f));
            g.drawEllipse(screenPos.x - ringR, screenPos.y - ringR, ringR * 2.0f, ringR * 2.0f, 1.5f);
        }
    }

    // 5. Draw Category Badge Labels ALWAYS ON TOP OF NODES (Offset above the cluster to prevent overlaps)
    for (const auto& cluster : clusters)
    {
        auto screenClusterPos = cloudToScreen(cluster.centerPos);

        juce::String badgeText = cluster.tag.toUpperCase() + " (" + juce::String(cluster.count) + ")";
        juce::Font badgeFont(11.0f, juce::Font::bold);
        int textWidth = badgeFont.getStringWidth(badgeText) + 20;

        // Position above the cluster based on cluster size and current zoom scale
        float clusterRadiusEstimate = (18.0f * std::sqrt(static_cast<float>(cluster.count) + 1.0f)) * zoomScale;
        float yOffset = std::max(25.0f * zoomScale, clusterRadiusEstimate + 12.0f);

        juce::Rectangle<float> badgeBounds(screenClusterPos.x - textWidth * 0.5f, screenClusterPos.y - yOffset - 11.0f, static_cast<float>(textWidth), 22.0f);

        g.setColour(OpenWavLookAndFeel::bgCard.withAlpha(0.95f));
        g.fillRoundedRectangle(badgeBounds, 11.0f);
        g.setColour(cluster.colour);
        g.drawRoundedRectangle(badgeBounds, 11.0f, 1.4f);

        g.setColour(OpenWavLookAndFeel::textPrimary);
        g.setFont(badgeFont);
        g.drawText(badgeText, badgeBounds, juce::Justification::centred, true);
    }

    // 6. Top Sticky Header Tag Bar Pinned to Top of Cloud View
    if (!clusters.empty())
    {
        g.setColour(OpenWavLookAndFeel::bgCard.withAlpha(0.90f));
        g.fillRoundedRectangle(12.0f, 8.0f, 220.0f, 28.0f, 6.0f);
        g.setColour(OpenWavLookAndFeel::borderColour);
        g.drawRoundedRectangle(12.0f, 8.0f, 220.0f, 28.0f, 6.0f, 1.0f);

        g.setFont(juce::Font(11.0f).boldened());
        g.setColour(OpenWavLookAndFeel::accentCyan);
        g.drawText("2D AUDIO MAP (" + juce::String(nodes.size()) + " SAMPLES • " + juce::String(clusters.size()) + " SECTORS)", 22, 12, 240, 20, juce::Justification::left, true);
    }

    // 5. Floating Hover Info Card with Mini Waveform Preview
    if (hoveredNodeIndex >= 0 && hoveredNodeIndex < static_cast<int>(nodes.size()))
    {
        const auto& item = nodes[static_cast<size_t>(hoveredNodeIndex)].item;
        const auto screenPos = cloudToScreen(nodes[static_cast<size_t>(hoveredNodeIndex)].currentPos);

        juce::Rectangle<float> cardBounds(screenPos.x + 14.0f, screenPos.y - 55.0f, 210.0f, 85.0f);

        if (cardBounds.getRight() > getWidth() - 10) cardBounds.setX(screenPos.x - 224.0f);
        if (cardBounds.getY() < 10) cardBounds.setY(screenPos.y + 14.0f);

        g.setColour(OpenWavLookAndFeel::bgCard.withAlpha(0.96f));
        g.fillRoundedRectangle(cardBounds, 8.0f);
        g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.7f));
        g.drawRoundedRectangle(cardBounds, 8.0f, 1.2f);

        auto contentArea = cardBounds.reduced(8.0f);

        // Filename Title
        g.setColour(OpenWavLookAndFeel::textPrimary);
        g.setFont(juce::Font(12.0f).boldened());
        g.drawText(item.fileName, contentArea.removeFromTop(18.0f), juce::Justification::left, true);

        // Metadata Subtitle
        g.setColour(OpenWavLookAndFeel::textSecondary);
        g.setFont(juce::Font(11.0f));
        juce::String metaStr = juce::String(item.durationSeconds, 2) + "s  |  " +
                              juce::String(item.sampleRate / 1000.0, 1) + "kHz / " +
                              juce::String(item.bitDepth) + "b";
        g.drawText(metaStr, contentArea.removeFromTop(16.0f), juce::Justification::left, true);

        // Tags List
        juce::String tagList;
        for (const auto& t : item.tags) tagList += "#" + t + " ";
        g.setColour(OpenWavLookAndFeel::accentCyan);
        g.drawText(tagList, contentArea.removeFromTop(16.0f), juce::Justification::left, true);

        // Mini Stylized Waveform Preview Bar
        auto waveRect = contentArea.reduced(2.0f, 2.0f);
        g.setColour(OpenWavLookAndFeel::bgDark);
        g.fillRoundedRectangle(waveRect, 3.0f);

        g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.75f));
        int numBars = static_cast<int>(waveRect.getWidth() / 4.0f);
        uint32_t seed = static_cast<uint32_t>(item.filePath.hashCode());

        for (int b = 0; b < numBars; ++b)
        {
            seed = seed * 1664525u + 1013904223u;
            float barH = (0.25f + ((seed % 100) / 100.0f) * 0.75f) * waveRect.getHeight();
            float barX = waveRect.getX() + b * 4.0f + 1.0f;
            float barY = waveRect.getCentreY() - barH * 0.5f;
            g.fillRect(barX, barY, 2.0f, barH);
        }
    }

    // 6. Bottom HUD Category Legend Bar
    if (!clusters.empty())
    {
        float legendX = 14.0f;
        float legendY = static_cast<float>(getHeight() - 32);

        g.setColour(OpenWavLookAndFeel::bgCard.withAlpha(0.85f));
        g.fillRoundedRectangle(10.0f, legendY - 4.0f, static_cast<float>(getWidth() - 20), 26.0f, 6.0f);

        g.setFont(juce::Font(11.0f).boldened());

        for (const auto& cl : clusters)
        {
            if (legendX + 80.0f > getWidth() - 110)
                break;

            g.setColour(cl.colour);
            g.fillEllipse(legendX, legendY + 4.0f, 8.0f, 8.0f);

            g.setColour(OpenWavLookAndFeel::textSecondary);
            juce::String tagLabel = cl.tag;
            int textW = g.getCurrentFont().getStringWidth(tagLabel) + 16;
            g.drawText(tagLabel, legendX + 12.0f, legendY, textW, 16, juce::Justification::left, true);

            legendX += textW + 14.0f;
        }
    }

    // Bottom-Right Zoom HUD Readout
    juce::String zoomStr = juce::String(static_cast<int>(zoomScale * 100)) + "%";
    g.setFont(juce::Font(11.0f).boldened());
    g.setColour(OpenWavLookAndFeel::accentCyan);
    g.drawText("Zoom: " + zoomStr, getWidth() - 95, getHeight() - 28, 80, 18, juce::Justification::right, true);
}

void SampleCloudComponent::resized()
{
    auto area = getLocalBounds().reduced(12);
    auto topHud = area.removeFromTop(28);

    resetZoomButton.setBounds(topHud.removeFromRight(85).withHeight(26));
    topHud.removeFromRight(6);
    zoomOutButton.setBounds(topHud.removeFromRight(28).withHeight(26));
    topHud.removeFromRight(4);
    zoomInButton.setBounds(topHud.removeFromRight(28).withHeight(26));

    calculateClusterLayout();
}

void SampleCloudComponent::timerCallback()
{
    pulsePhase += 0.1f;
    if (pulsePhase > juce::MathConstants<float>::twoPi)
        pulsePhase -= juce::MathConstants<float>::twoPi;

    bool needsRepaint = false;

    for (size_t i = 0; i < nodes.size(); ++i)
    {
        auto& node = nodes[i];
        float targetScale = (static_cast<int>(i) == hoveredNodeIndex) ? 2.2f : 1.0f;
        float ds = targetScale - node.hoverScale;
        if (std::abs(ds) > 0.01f)
        {
            node.hoverScale += ds * 0.25f; // Smooth transition
            needsRepaint = true;
        }
        else
        {
            node.hoverScale = targetScale;
        }

        float dx = node.targetPos.x - node.currentPos.x;
        float dy = node.targetPos.y - node.currentPos.y;

        if (std::abs(dx) > 0.5f || std::abs(dy) > 0.5f)
        {
            node.currentPos.x += dx * 0.12f;
            node.currentPos.y += dy * 0.12f;
            needsRepaint = true;
        }
    }

    // Optimisation: Do not repaint if we are idle (selectedNodeIndex is static, no pulse ring animation)
    if (needsRepaint || isPanning)
        repaint();
}

void SampleCloudComponent::mouseMove(const juce::MouseEvent& e)
{
    int prevHover = hoveredNodeIndex;
    hoveredNodeIndex = findNodeAtPosition(e.position);

    if (prevHover != hoveredNodeIndex)
    {
        if (hoveredNodeIndex >= 0)
            setMouseCursor(juce::MouseCursor::PointingHandCursor);
        else
            setMouseCursor(juce::MouseCursor::NormalCursor);

        repaint();
    }
}

void SampleCloudComponent::mouseDown(const juce::MouseEvent& e)
{
    int idx = findNodeAtPosition(e.position);

    if (e.mods.isPopupMenu() && idx >= 0 && idx < static_cast<int>(nodes.size()))
    {
        showContextMenuForNode(idx);
        return;
    }

    if (idx >= 0 && idx < static_cast<int>(nodes.size()))
    {
        isPanning = false;
        selectedNodeIndex = idx;
        const auto& item = nodes[static_cast<size_t>(idx)].item;

        audioEngine.loadFile(juce::File(item.filePath), true);

        listeners.call([item](SampleCloudListener& l) {
            l.cloudSampleSelected(item);
        });

        repaint();
    }
    else
    {
        // Canvas background clicked: initiate pan
        isPanning = true;
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
        mouseDragStartPos = e.position;
        dragStartPan = panOffset;
    }
}

void SampleCloudComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (isPanning)
    {
        panOffset = dragStartPan + (e.position - mouseDragStartPos);
        repaint();
    }
    else if (e.mouseWasDraggedSinceMouseDown() && selectedNodeIndex >= 0 && selectedNodeIndex < static_cast<int>(nodes.size()))
    {
        const auto& item = nodes[static_cast<size_t>(selectedNodeIndex)].item;
        juce::StringArray filesToDrag;
        filesToDrag.add(item.filePath);
        juce::DragAndDropContainer::performExternalDragDropOfFiles(filesToDrag, false);
    }
}

void SampleCloudComponent::mouseUp(const juce::MouseEvent& /*e*/)
{
    isPanning = false;
    setMouseCursor(hoveredNodeIndex >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void SampleCloudComponent::mouseDoubleClick(const juce::MouseEvent& e)
{
    int idx = findNodeAtPosition(e.position);
    if (idx >= 0 && idx < static_cast<int>(nodes.size()))
    {
        const auto& item = nodes[static_cast<size_t>(idx)].item;

        if (audioEngine.getCurrentFile().getFullPathName() == item.filePath && audioEngine.isPlaying())
            audioEngine.pause();
        else
            audioEngine.loadFile(juce::File(item.filePath), true);

        listeners.call([item](SampleCloudListener& l) {
            l.cloudSampleDoubleClicked(item);
        });
    }
    else
    {
        // Double-click background resets zoom & pan
        resetZoomAndPan();
    }
}

void SampleCloudComponent::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    float zoomFactor = (wheel.deltaY > 0.0f) ? 1.15f : 0.87f;
    float oldZoom = zoomScale;
    zoomScale = juce::jlimit(0.3f, 4.0f, zoomScale * zoomFactor);

    auto center = getLocalBounds().getCentre().toFloat();
    auto mousePos = e.position;
    panOffset = panOffset + (mousePos - center - panOffset) * (1.0f - zoomScale / oldZoom);

    repaint();
}

void SampleCloudComponent::setItems(const std::vector<MediaItem>& items)
{
    resetZoomAndPan();
    nodes.clear();
    nodes.reserve(items.size());

    for (const auto& item : items)
    {
        CloudNode node;
        node.item = item;

        // Primary tag determination
        if (!item.tags.empty())
            node.primaryTag = *item.tags.begin();
        else
            node.primaryTag = "General";

        node.colour = getColourForTag(node.primaryTag);

        // Calculate node size based on duration - smaller default sizes
        float dur = static_cast<float>(item.durationSeconds);
        node.radius = juce::jlimit(3.0f, 9.0f, 3.0f + dur * 1.0f);
        node.hoverScale = 1.0f;

        nodes.push_back(node);
    }

    calculateClusterLayout();
    repaint();
}

void SampleCloudComponent::calculateClusterLayout()
{
    if (nodes.empty())
    {
        clusters.clear();
        return;
    }

    auto bounds = getLocalBounds().toFloat();
    float centerX = bounds.getCentreX();
    float centerY = bounds.getCentreY();
    float maxRadius = std::min(bounds.getWidth(), bounds.getHeight()) * 0.44f;

    // 1. Collect unique primary tags and build cluster objects
    std::unordered_map<juce::String, TagCluster> clusterMap;

    for (const auto& n : nodes)
    {
        auto& c = clusterMap[n.primaryTag];
        c.tag = n.primaryTag;
        c.colour = n.colour;
        c.count++;
    }

    clusters.clear();
    for (const auto& pair : clusterMap)
    {
        clusters.push_back(pair.second);
    }

    // Sort clusters by count descending for stable territory mapping
    std::sort(clusters.begin(), clusters.end(), [](const TagCluster& a, const TagCluster& b) {
        return a.count > b.count;
    });

    // 2. Map Layout: Position tag clusters in a 2D staggered map grid
    size_t tagCount = clusters.size();
    if (tagCount > 0)
    {
        int cols = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(tagCount) * 1.35f)));
        cols = std::max(2, cols);
        int rows = static_cast<int>(std::ceil(static_cast<float>(tagCount) / static_cast<float>(cols)));

        float spacingX = 340.0f;
        float spacingY = 280.0f;

        float totalW = (cols - 1) * spacingX;
        float totalH = (rows - 1) * spacingY;

        float startX = centerX - totalW * 0.5f;
        float startY = centerY - totalH * 0.5f;

        for (size_t i = 0; i < tagCount; ++i)
        {
            int r = static_cast<int>(i) / cols;
            int c = static_cast<int>(i) % cols;

            // Stagger odd rows for an organic land-map layout
            float staggerX = (r % 2 == 1) ? (spacingX * 0.35f) : 0.0f;
            float posX = startX + c * spacingX + staggerX;
            float posY = startY + r * spacingY;

            clusters[i].centerPos = { posX, posY };
        }
    }

    // 3. Map cluster centroid positions back for quick node layout lookup
    std::unordered_map<juce::String, juce::Point<float>> tagCenters;
    for (const auto& cl : clusters)
    {
        tagCenters[cl.tag] = cl.centerPos;
    }

    // 4. Calculate initial target positions per cluster
    std::unordered_map<juce::String, int> tagItemCounts;

    for (auto& node : nodes)
    {
        auto cPos = tagCenters[node.primaryTag];
        int countIndex = tagItemCounts[node.primaryTag]++;

        // Golden ratio spiral placement per cluster center - tighter pack per group since dots are smaller
        float phi = 2.39996323f; // Golden angle in radians
        float r = 12.0f * std::sqrt(static_cast<float>(countIndex) + 1.0f);
        float theta = countIndex * phi;

        float targetX = cPos.x + r * std::cos(theta);
        float targetY = cPos.y + r * std::sin(theta);

        node.targetPos = { targetX, targetY };

        if (node.currentPos.getDistanceFrom({0.0f, 0.0f}) < 1.0f)
        {
            node.currentPos = { centerX, centerY };
        }
    }

    // 5. Apply physics repulsion to prevent overlapping dots
    applyForceDirectedPhysics();
}

void SampleCloudComponent::applyForceDirectedPhysics()
{
    // Iterative repulsion pass to eliminate node overlap
    for (int iter = 0; iter < 12; ++iter)
    {
        for (size_t i = 0; i < nodes.size(); ++i)
        {
            for (size_t j = i + 1; j < nodes.size(); ++j)
            {
                auto delta = nodes[j].targetPos - nodes[i].targetPos;
                float dist = delta.getDistanceFrom({0.0f, 0.0f});
                float minDist = nodes[i].radius + nodes[j].radius + 6.0f;

                if (dist < minDist && dist > 0.01f)
                {
                    float overlap = (minDist - dist) * 0.5f;
                    auto dir = delta / dist;
                    nodes[i].targetPos -= dir * overlap;
                    nodes[j].targetPos += dir * overlap;
                }
            }
        }
    }
}

int SampleCloudComponent::findNodeAtPosition(juce::Point<float> screenPos) const
{
    int bestIndex = -1;
    float bestDistance = std::numeric_limits<float>::max();

    for (size_t i = 0; i < nodes.size(); ++i)
    {
        auto nodeScreenPos = cloudToScreen(nodes[i].currentPos);
        float hitR = (nodes[i].radius + 3.0f) * zoomScale;
        float maxHit = std::max(12.0f, hitR);
        float dist = nodeScreenPos.getDistanceFrom(screenPos);
        if (dist <= maxHit)
        {
            if (dist < bestDistance)
            {
                bestDistance = dist;
                bestIndex = static_cast<int>(i);
            }
        }
    }
    return bestIndex;
}

juce::Colour SampleCloudComponent::getColourForTag(const juce::String& tag) const
{
    auto t = tag.toLowerCase();
    if (t.contains("kick"))  return juce::Colour(0xfff0f0f0); // Bright White / Silver
    if (t.contains("snare")) return juce::Colour(0xffd8d8d8); // Light Gray
    if (t.contains("hat") || t.contains("hihat")) return juce::Colour(0xffc0c0c0); // Medium Light Gray
    if (t.contains("perc"))  return juce::Colour(0xffa8a8a8); // Medium Gray
    if (t.contains("bass"))  return juce::Colour(0xff787878); // Slate Gray
    if (t.contains("synth") || t.contains("lead")) return juce::Colour(0xffe8e8e8); // Soft White
    if (t.contains("loop"))  return juce::Colour(0xffb0b0b0); // Cool Gray
    if (t.contains("vocal")) return juce::Colour(0xffe0e0e0); // Platinum

    // Deterministic grayscale for other tags (range 0.40f to 0.95f)
    uint32_t hash = static_cast<uint32_t>(tag.hashCode());
    float brightness = 0.40f + ((hash % 100) / 100.0f) * 0.55f;
    return juce::Colour::greyLevel(brightness);
}

void SampleCloudComponent::showContextMenuForNode(int idx)
{
    if (idx < 0 || idx >= static_cast<int>(nodes.size()))
        return;

    const auto& item = nodes[static_cast<size_t>(idx)].item;

    juce::PopupMenu menu;
    menu.addSectionHeader(item.fileName);
    menu.addItem(1, item.isFavorite ? "Remove from Favorites" : "Add to Favorites");
    menu.addItem(2, "Add Custom Tag...");
    menu.addItem(3, "Reveal in File Explorer / Finder");

    menu.showMenuAsync(juce::PopupMenu::Options(), [this, item](int result) {
        if (result == 1)
        {
            dbManager.toggleFavorite(item.id);
        }
        else if (result == 2)
        {
            auto alert = std::make_shared<juce::AlertWindow>("Add Custom Tag", "Enter a new custom tag for " + item.fileName + ":", juce::AlertWindow::QuestionIcon);
            alert->addTextEditor("tagInput", "", "Tag (e.g. Kick, #Sub, Vocal)");
            alert->addButton("Add Tag", 1, juce::KeyPress(juce::KeyPress::returnKey));
            alert->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

            alert->enterModalState(true, juce::ModalCallbackFunction::create([this, alert, item](int btnResult) {
                if (btnResult == 1)
                {
                    auto newTag = alert->getTextEditorContents("tagInput").trim();
                    if (newTag.isNotEmpty())
                    {
                        dbManager.addTagToItem(item.id, newTag);
                    }
                }
            }));
        }
        else if (result == 3)
        {
            juce::File(item.filePath).revealToUser();
        }
    });
}

void SampleCloudComponent::addListener(SampleCloudListener* listener)
{
    listeners.add(listener);
}

void SampleCloudComponent::removeListener(SampleCloudListener* listener)
{
    listeners.remove(listener);
}

} // namespace openwav
