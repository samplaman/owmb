#include "SampleCloudComponent.h"
#include "OpenWavLookAndFeel.h"
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <limits>

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

juce::Point<float> SampleCloudComponent::project3DToScreen(Vector3D pos, Vector3D& outTransformed, float& outScale) const
{
    auto center = getLocalBounds().getCentre().toFloat();

    // 1. Rotate around Y axis (Yaw rotY)
    float cosY = std::cos(rotY);
    float sinY = std::sin(rotY);

    float x1 = pos.x * cosY + pos.z * sinY;
    float z1 = -pos.x * sinY + pos.z * cosY;

    // 2. Rotate around X axis (Pitch rotX)
    float cosX = std::cos(rotX);
    float sinX = std::sin(rotX);

    float y2 = pos.y * cosX - z1 * sinX;
    float z2 = pos.y * sinX + z1 * cosX;

    outTransformed = { x1, y2, z2 };

    // 3. Orthographic 3D Projection (No FOV distortion)
    outScale = zoomScale;

    float screenX = center.x + x1 * outScale + panOffset.x;
    float screenY = center.y + y2 * outScale + panOffset.y;

    return { screenX, screenY };
}

void SampleCloudComponent::update3DTransforms()
{
    for (auto& cl : clusters)
    {
        cl.screenPos = project3DToScreen(cl.centerPos, cl.transformedPos, cl.projectedScale);
    }

    sortedNodePointers.clear();
    sortedNodePointers.reserve(nodes.size());

    for (size_t i = 0; i < nodes.size(); ++i)
    {
        auto& node = nodes[i];
        node.originalIndex = i;
        node.screenPos = project3DToScreen(node.currentPos, node.transformedPos, node.projectedScale);
        sortedNodePointers.push_back(&node);
    }

    // Sort by transformed Z (Back to front: Painter's Algorithm)
    std::sort(sortedNodePointers.begin(), sortedNodePointers.end(), [](const CloudNode* a, const CloudNode* b) {
        return a->transformedPos.z > b->transformedPos.z;
    });
}

void SampleCloudComponent::resetZoomAndPan()
{
    zoomScale = 1.0f;
    panOffset = { 0.0f, 0.0f };
    targetRotX = 0.35f;
    targetRotY = 0.45f;
    cameraDistance = 850.0f;
    repaint();
}

void SampleCloudComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    juce::Colour centerColour, outerColour;

    if (OpenWavLookAndFeel::bgDark.getPerceivedBrightness() < 0.5f)
    {
        // Dark Mode: deep celestial radial glow towards the center
        centerColour = OpenWavLookAndFeel::bgDark.interpolatedWith(OpenWavLookAndFeel::accentCyan, 0.08f);
        outerColour = OpenWavLookAndFeel::bgDark;
    }
    else
    {
        // Light Mode: clean soft depth
        centerColour = OpenWavLookAndFeel::bgDark;
        outerColour = OpenWavLookAndFeel::bgDark.darker(0.04f);
    }

    juce::ColourGradient cg (centerColour, bounds.getCentreX(), bounds.getCentreY(),
                             outerColour, 0.0f, 0.0f,
                             true); // true = radial gradient

    g.setGradientFill (cg);
    g.fillAll();

    if (nodes.empty())
    {
        g.setFont(juce::Font(14.0f));
        g.setColour(OpenWavLookAndFeel::textSecondary);
        g.drawText("No samples found. Add a folder or adjust filters.", getLocalBounds(), juce::Justification::centred, true);
        return;
    }

    update3DTransforms();

    // 1. Draw Clean Cluster Territory Halos (Perfect Circles)
    for (const auto& cluster : clusters)
    {
        float baseRadius = std::max(45.0f, 30.0f + std::sqrt(static_cast<float>(cluster.count)) * 14.0f);
        float r = baseRadius * cluster.projectedScale;
        float depthAlpha = juce::jlimit(0.35f, 1.0f, 1.0f - (cluster.transformedPos.z + 300.0f) / 900.0f);

        // Soft island land mass fill
        g.setColour(cluster.colour.withAlpha(0.08f * depthAlpha));
        g.fillEllipse(cluster.screenPos.x - r, cluster.screenPos.y - r, r * 2.0f, r * 2.0f);

        // Outer territory ring
        g.setColour(cluster.colour.withAlpha(0.28f * depthAlpha));
        g.drawEllipse(cluster.screenPos.x - r, cluster.screenPos.y - r, r * 2.0f, r * 2.0f, 1.2f);
    }

    // 3. Draw Z-Sorted 3D Sample Nodes (Back to Front)
    for (const auto* nodePtr : sortedNodePointers)
    {
        const auto& node = *nodePtr;
        int origIdx = static_cast<int>(node.originalIndex);
        bool isHovered = (origIdx == hoveredNodeIndex);
        bool isSelected = (origIdx == selectedNodeIndex);

        float depthAlpha = juce::jlimit(0.45f, 1.0f, 1.0f - (node.transformedPos.z + 300.0f) / 900.0f);

        float baseR = node.radius * node.hoverScale;
        float r = std::max(2.2f, baseR * node.projectedScale);

        // Soft outer glow halo
        float glowR = r * (isSelected ? 2.0f : (isHovered ? 1.8f : 1.4f));
        float alpha = isSelected ? 0.45f : (isHovered ? 0.40f : 0.22f);
        g.setColour(node.colour.withAlpha(alpha * depthAlpha));
        g.fillEllipse(node.screenPos.x - glowR, node.screenPos.y - glowR, glowR * 2.0f, glowR * 2.0f);

        // Vibrant Color Fill
        juce::Colour nodeColor = node.colour;
        if (isHovered)
            nodeColor = juce::Colours::white;
        else if (isSelected)
            nodeColor = OpenWavLookAndFeel::accentCyan;

        g.setColour(nodeColor.withAlpha(depthAlpha));
        g.fillEllipse(node.screenPos.x - r, node.screenPos.y - r, r * 2.0f, r * 2.0f);

        // Ring highlight if hovered or selected
        if (isHovered || isSelected)
        {
            g.setColour(isHovered ? juce::Colours::white : OpenWavLookAndFeel::accentCyan);
            g.drawEllipse(node.screenPos.x - r, node.screenPos.y - r, r * 2.0f, r * 2.0f, 1.4f);
        }
    }

    // 4. Draw 3D Category Badges over Cluster Centroids
    for (const auto& cluster : clusters)
    {
        juce::String badgeText = cluster.tag.toUpperCase() + " (" + juce::String(cluster.count) + ")";
        juce::Font badgeFont(11.0f, juce::Font::bold);
        int textWidth = badgeFont.getStringWidth(badgeText) + 20;

        float depthAlpha = juce::jlimit(0.25f, 1.0f, 1.0f - (cluster.transformedPos.z + 300.0f) / 900.0f);

        juce::Rectangle<float> badgeBounds(cluster.screenPos.x - textWidth * 0.5f, cluster.screenPos.y - 28.0f * cluster.projectedScale, static_cast<float>(textWidth), 22.0f);

        g.setColour(OpenWavLookAndFeel::bgCard.withAlpha(0.92f * depthAlpha));
        g.fillRoundedRectangle(badgeBounds, 11.0f);
        g.setColour(cluster.colour.withAlpha(depthAlpha));
        g.drawRoundedRectangle(badgeBounds, 11.0f, 1.4f);

        g.setColour(OpenWavLookAndFeel::textPrimary.withAlpha(depthAlpha));
        g.setFont(badgeFont);
        g.drawText(badgeText, badgeBounds, juce::Justification::centred, true);
    }

    // 5. Sticky Top Header Badge
    if (!clusters.empty())
    {
        juce::String mapHeaderText = "3D CONSTELLATION MAP (" + juce::String(nodes.size()) + " SAMPLES • " + juce::String(clusters.size()) + " SECTORS)";
        juce::Font mapHeaderFont(11.0f, juce::Font::bold);
        float badgeWidth = static_cast<float>(mapHeaderFont.getStringWidth(mapHeaderText) + 24);

        juce::Rectangle<float> headerBadgeBounds(12.0f, 8.0f, badgeWidth, 28.0f);

        g.setColour(OpenWavLookAndFeel::bgCard.withAlpha(0.92f));
        g.fillRoundedRectangle(headerBadgeBounds, 6.0f);
        g.setColour(OpenWavLookAndFeel::borderColour);
        g.drawRoundedRectangle(headerBadgeBounds, 6.0f, 1.0f);

        g.setFont(mapHeaderFont);
        g.setColour(OpenWavLookAndFeel::accentCyan);
        g.drawText(mapHeaderText, headerBadgeBounds, juce::Justification::centred, true);
    }

    // 6. Floating Hover Card with Mini Waveform Preview
    if (hoveredNodeIndex >= 0 && hoveredNodeIndex < static_cast<int>(nodes.size()))
    {
        const auto& item = nodes[static_cast<size_t>(hoveredNodeIndex)].item;
        const auto screenPos = nodes[static_cast<size_t>(hoveredNodeIndex)].screenPos;

        juce::Rectangle<float> cardBounds(screenPos.x + 14.0f, screenPos.y - 55.0f, 210.0f, 85.0f);

        if (cardBounds.getRight() > getWidth() - 10) cardBounds.setX(screenPos.x - 224.0f);
        if (cardBounds.getY() < 10) cardBounds.setY(screenPos.y + 14.0f);

        g.setColour(OpenWavLookAndFeel::bgCard.withAlpha(0.96f));
        g.fillRoundedRectangle(cardBounds, 8.0f);
        g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.7f));
        g.drawRoundedRectangle(cardBounds, 8.0f, 1.2f);

        auto contentArea = cardBounds.reduced(8.0f);

        g.setColour(OpenWavLookAndFeel::textPrimary);
        g.setFont(juce::Font(12.0f).boldened());
        g.drawText(item.fileName, contentArea.removeFromTop(18.0f), juce::Justification::left, true);

        g.setColour(OpenWavLookAndFeel::textSecondary);
        g.setFont(juce::Font(11.0f));
        juce::String metaStr = juce::String(item.durationSeconds, 2) + "s  |  " +
                              juce::String(item.sampleRate / 1000.0, 1) + "kHz / " +
                              juce::String(item.bitDepth) + "b";
        g.drawText(metaStr, contentArea.removeFromTop(16.0f), juce::Justification::left, true);

        juce::String tagList;
        for (const auto& t : item.tags) tagList += "#" + t + " ";
        g.setColour(OpenWavLookAndFeel::accentCyan);
        g.drawText(tagList, contentArea.removeFromTop(16.0f), juce::Justification::left, true);

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

    // 7. Bottom HUD Category Legend Bar
    if (!clusters.empty())
    {
        float legendX = 14.0f;
        float legendY = static_cast<float>(getHeight() - 32);

        g.setColour(OpenWavLookAndFeel::bgCard.withAlpha(0.85f));
        g.fillRoundedRectangle(10.0f, legendY - 4.0f, static_cast<float>(getWidth() - 20), 26.0f, 6.0f);

        g.setFont(juce::Font(11.0f).boldened());

        for (const auto& cl : clusters)
        {
            if (legendX + 80.0f > getWidth() - 330)
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

    // Bottom-Right 3D HUD Control Helper
    g.setFont(juce::Font(10.0f));
    g.setColour(OpenWavLookAndFeel::textSecondary);
    g.drawText("Drag to rotate 3D view | Wheel to zoom | Mid-click to pan", getWidth() - 320, getHeight() - 28, 300, 18, juce::Justification::right, true);
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

    // Smooth 3D camera rotation interpolation
    float drotX = targetRotX - rotX;
    float drotY = targetRotY - rotY;

    if (std::abs(drotX) > 0.001f || std::abs(drotY) > 0.001f)
    {
        rotX += drotX * 0.15f;
        rotY += drotY * 0.15f;
        needsRepaint = true;
    }

    for (size_t i = 0; i < nodes.size(); ++i)
    {
        auto& node = nodes[i];
        float targetScale = (static_cast<int>(i) == hoveredNodeIndex) ? 2.2f : 1.0f;
        float ds = targetScale - node.hoverScale;
        if (std::abs(ds) > 0.01f)
        {
            node.hoverScale += ds * 0.25f;
            needsRepaint = true;
        }

        Vector3D dPos = node.targetPos - node.currentPos;
        if (std::abs(dPos.x) > 0.5f || std::abs(dPos.y) > 0.5f || std::abs(dPos.z) > 0.5f)
        {
            node.currentPos = node.currentPos + dPos * 0.12f;
            needsRepaint = true;
        }
    }

    if (needsRepaint || isPanning || isRotating)
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
        isRotating = false;
        hasStartedDrag = false;
        selectedNodeIndex = idx;
        const auto& item = nodes[static_cast<size_t>(idx)].item;

        audioEngine.loadFile(juce::File(item.filePath), true);

        listeners.call([item](SampleCloudListener& l) {
            l.cloudSampleSelected(item);
        });

        repaint();
    }
    else if (e.mods.isMiddleButtonDown() || e.mods.isShiftDown())
    {
        isPanning = true;
        isRotating = false;
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
        mouseDragStartPos = e.position;
        dragStartPan = panOffset;
    }
    else
    {
        // Dragging left mouse button on background rotates 3D camera
        isRotating = true;
        isPanning = false;
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
        mouseDragStartPos = e.position;
        dragStartRotX = targetRotX;
        dragStartRotY = targetRotY;
    }
}

void SampleCloudComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (isRotating)
    {
        auto delta = e.position - mouseDragStartPos;
        targetRotY = dragStartRotY + delta.x * 0.008f;
        targetRotX = juce::jlimit(-1.4f, 1.4f, dragStartRotX + delta.y * 0.008f);
        repaint();
    }
    else if (isPanning)
    {
        panOffset = dragStartPan + (e.position - mouseDragStartPos);
        repaint();
    }
    else if (e.mouseWasDraggedSinceMouseDown() && !hasStartedDrag && selectedNodeIndex >= 0 && selectedNodeIndex < static_cast<int>(nodes.size()))
    {
        hasStartedDrag = true;
        const auto& item = nodes[static_cast<size_t>(selectedNodeIndex)].item;
        juce::StringArray filesToDrag;
        filesToDrag.add(item.filePath);
        
        juce::MessageManager::callAsync([filesToDrag] {
            juce::DragAndDropContainer::performExternalDragDropOfFiles(filesToDrag, false);
        });
    }
}

void SampleCloudComponent::mouseUp(const juce::MouseEvent& /*e*/)
{
    isPanning = false;
    isRotating = false;
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

        if (!item.tags.empty())
            node.primaryTag = *item.tags.begin();
        else
            node.primaryTag = "General";

        node.colour = getColourForTag(node.primaryTag);

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

    std::sort(clusters.begin(), clusters.end(), [](const TagCluster& a, const TagCluster& b) {
        return a.count > b.count;
    });

    size_t tagCount = clusters.size();
    if (tagCount > 0)
    {
        float baseRadius = 450.0f;
        float goldenAngle = 2.39996323f; // Golden ratio angle (~137.5 deg)

        for (size_t i = 0; i < tagCount; ++i)
        {
            auto tagLower = clusters[i].tag.toLowerCase();

            if (tagLower.contains("kick"))
            {
                clusters[i].centerPos = { -320.0f, -40.0f, -120.0f };
            }
            else if (tagLower.contains("snare"))
            {
                clusters[i].centerPos = { 320.0f, -40.0f, 120.0f };
            }
            else if (tagLower.contains("bass"))
            {
                clusters[i].centerPos = { 0.0f, 320.0f, 180.0f };
            }
            else if (tagLower.contains("hat") || tagLower.contains("hihat"))
            {
                clusters[i].centerPos = { 0.0f, -320.0f, -180.0f };
            }
            else if (tagLower.contains("perc"))
            {
                clusters[i].centerPos = { 450.0f, 160.0f, -220.0f };
            }
            else if (tagLower.contains("synth") || tagLower.contains("lead"))
            {
                clusters[i].centerPos = { -450.0f, -160.0f, 220.0f };
            }
            else
            {
                float theta = static_cast<float>(i) * goldenAngle;
                float r = baseRadius + (i * 65.0f);
                float x = r * std::cos(theta);
                float z = r * std::sin(theta);
                float y = (i % 2 == 0 ? 1.0f : -1.0f) * (60.0f + (i * 25.0f));

                clusters[i].centerPos = { x, y, z };
            }
        }

        // Cluster-cluster 3D repulsion pass to ensure zero overlap between tag spheres
        for (int iter = 0; iter < 16; ++iter)
        {
            for (size_t i = 0; i < tagCount; ++i)
            {
                for (size_t j = i + 1; j < tagCount; ++j)
                {
                    auto delta = clusters[j].centerPos - clusters[i].centerPos;
                    float dist = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
                    float r1 = std::max(45.0f, 30.0f + std::sqrt(static_cast<float>(clusters[i].count)) * 14.0f);
                    float r2 = std::max(45.0f, 30.0f + std::sqrt(static_cast<float>(clusters[j].count)) * 14.0f);
                    float minDist = r1 + r2 + 60.0f; // Generous 60px clearance buffer

                    if (dist < minDist && dist > 0.01f)
                    {
                        float overlap = (minDist - dist) * 0.5f;
                        auto dir = delta / dist;
                        clusters[i].centerPos = clusters[i].centerPos - dir * overlap;
                        clusters[j].centerPos = clusters[j].centerPos + dir * overlap;
                    }
                }
            }
        }
    }

    std::unordered_map<juce::String, Vector3D> tagCenters;
    std::unordered_map<juce::String, int> tagTotalCounts;
    for (const auto& cl : clusters)
    {
        tagCenters[cl.tag] = cl.centerPos;
        tagTotalCounts[cl.tag] = cl.count;
    }

    std::unordered_map<juce::String, int> tagItemCounts;

    for (auto& node : nodes)
    {
        auto cPos = tagCenters[node.primaryTag];
        int countIdx = tagItemCounts[node.primaryTag]++;
        int totalInTag = tagTotalCounts[node.primaryTag];

        // 3D Fibonacci Sphere Constellation Packing
        float phi = 2.39996323f;
        float y = 1.0f - (static_cast<float>(countIdx) / std::max(1.0f, static_cast<float>(totalInTag - 1))) * 2.0f;
        float radiusAtY = std::sqrt(std::max(0.0f, 1.0f - y * y));

        float clusterSphereRadius = std::max(40.0f, 25.0f + std::sqrt(static_cast<float>(totalInTag)) * 14.0f);

        float theta = countIdx * phi;
        float xOffset = clusterSphereRadius * radiusAtY * std::cos(theta);
        float zOffset = clusterSphereRadius * radiusAtY * std::sin(theta);
        float yOffset = clusterSphereRadius * y;

        node.targetPos = { cPos.x + xOffset, cPos.y + yOffset, cPos.z + zOffset };

        if (std::abs(node.currentPos.x) < 0.1f && std::abs(node.currentPos.y) < 0.1f && std::abs(node.currentPos.z) < 0.1f)
        {
            node.currentPos = node.targetPos;
        }
    }

    applyForceDirectedPhysics();
}

void SampleCloudComponent::applyForceDirectedPhysics()
{
    for (int iter = 0; iter < 16; ++iter)
    {
        for (size_t i = 0; i < nodes.size(); ++i)
        {
            for (size_t j = i + 1; j < nodes.size(); ++j)
            {
                auto delta = nodes[j].targetPos - nodes[i].targetPos;
                float dist = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
                float minDist = nodes[i].radius + nodes[j].radius + 14.0f;

                if (dist < minDist && dist > 0.01f)
                {
                    float overlap = (minDist - dist) * 0.5f;
                    auto dir = delta / dist;
                    nodes[i].targetPos = nodes[i].targetPos - dir * overlap;
                    nodes[j].targetPos = nodes[j].targetPos + dir * overlap;
                }
            }
        }
    }
}

int SampleCloudComponent::findNodeAtPosition(juce::Point<float> screenPos) const
{
    int bestIndex = -1;
    float bestZ = std::numeric_limits<float>::max();

    for (size_t i = 0; i < nodes.size(); ++i)
    {
        const auto& node = nodes[i];
        float hitR = (node.radius + 4.0f) * node.projectedScale;
        float maxHit = std::max(12.0f, hitR);
        float dist = node.screenPos.getDistanceFrom(screenPos);
        if (dist <= maxHit)
        {
            if (node.transformedPos.z < bestZ)
            {
                bestZ = node.transformedPos.z;
                bestIndex = static_cast<int>(i);
            }
        }
    }
    return bestIndex;
}

juce::Colour SampleCloudComponent::getColourForTag(const juce::String& tag) const
{
    auto t = tag.toLowerCase();
    if (t.contains("kick"))  return juce::Colour(0xffe53935); // Rich Crimson
    if (t.contains("snare")) return juce::Colour(0xfff57c00); // Warm Terracotta Amber
    if (t.contains("hat") || t.contains("hihat")) return juce::Colour(0xff0288d1); // Oceanic Azure Blue
    if (t.contains("perc"))  return juce::Colour(0xff2e7d32); // Rich Emerald Green
    if (t.contains("bass"))  return juce::Colour(0xff673ab7); // Royal Indigo Violet
    if (t.contains("synth") || t.contains("lead")) return juce::Colour(0xffffb300); // Warm Golden Amber
    if (t.contains("loop"))  return juce::Colour(0xff00897b); // Deep Teal
    if (t.contains("vocal")) return juce::Colour(0xffc2185b); // Rich Plum Berry

    // Tailored non-neon HSL mapping for custom tags
    uint32_t hash = static_cast<uint32_t>(tag.hashCode());
    float hue = (hash % 360) / 360.0f;
    return juce::Colour::fromHSV(hue, 0.65f, 0.85f, 1.0f);
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
