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

    // 1. Draw background subtle radar rings
    g.setColour(OpenWavLookAndFeel::borderColour.withAlpha(0.12f));
    auto screenCenter = getLocalBounds().getCentre().toFloat() + panOffset;

    for (int r = 100; r <= 500; r += 120)
    {
        float scaledR = r * zoomScale;
        g.drawEllipse(screenCenter.x - scaledR, screenCenter.y - scaledR, scaledR * 2.0f, scaledR * 2.0f, 1.0f);
    }

    // 2. Draw Cluster Nebula Glow Halos
    for (const auto& cluster : clusters)
    {
        auto screenClusterPos = cloudToScreen(cluster.centerPos);
        float haloRadius = std::max(60.0f, (40.0f + cluster.count * 6.0f) * zoomScale);

        // Nebula glow
        juce::ColourGradient nebula(
            cluster.colour.withAlpha(0.12f),
            screenClusterPos.x, screenClusterPos.y,
            cluster.colour.withAlpha(0.0f),
            screenClusterPos.x + haloRadius, screenClusterPos.y + haloRadius,
            true
        );
        g.setGradientFill(nebula);
        g.fillEllipse(screenClusterPos.x - haloRadius, screenClusterPos.y - haloRadius, haloRadius * 2.0f, haloRadius * 2.0f);
    }

    // 4. Draw all sample nodes with 3D radial gradients
    for (size_t i = 0; i < nodes.size(); ++i)
    {
        const auto& node = nodes[i];
        bool isHovered = (static_cast<int>(i) == hoveredNodeIndex);
        bool isSelected = (static_cast<int>(i) == selectedNodeIndex);

        auto screenPos = cloudToScreen(node.currentPos);
        float baseR = isHovered ? (node.radius + 3.5f) : node.radius;
        float r = std::max(3.0f, baseR * zoomScale);

        // Soft outer glow halo
        if (isHovered || isSelected)
        {
            float glowR = r * 1.8f;
            juce::ColourGradient haloGrad(
                node.colour.withAlpha(isHovered ? 0.45f : 0.3f),
                screenPos.x, screenPos.y,
                node.colour.withAlpha(0.0f),
                screenPos.x + glowR, screenPos.y + glowR,
                true
            );
            g.setGradientFill(haloGrad);
            g.fillEllipse(screenPos.x - glowR, screenPos.y - glowR, glowR * 2.0f, glowR * 2.0f);
        }

        // 3D Radial Gradient Fill
        juce::Colour centerColour = isHovered ? node.colour.brighter(0.75f) : node.colour.brighter(0.4f);
        juce::Colour edgeColour   = isHovered ? node.colour.brighter(0.15f) : node.colour.darker(0.35f);

        juce::ColourGradient nodeGrad(
            centerColour,
            screenPos.x - r * 0.35f, screenPos.y - r * 0.35f,
            edgeColour,
            screenPos.x + r * 0.9f, screenPos.y + r * 0.9f,
            true
        );
        g.setGradientFill(nodeGrad);
        g.fillEllipse(screenPos.x - r, screenPos.y - r, r * 2.0f, r * 2.0f);

        // Specular highlight dot
        g.setColour(juce::Colours::white.withAlpha(isHovered ? 0.65f : 0.35f));
        g.fillEllipse(screenPos.x - r * 0.45f, screenPos.y - r * 0.45f, r * 0.55f, r * 0.55f);

        // Audio playing pulse animation ring
        if (isSelected)
        {
            float pulseR = r + 4.0f + std::sin(pulsePhase) * 3.0f;
            g.setColour(OpenWavLookAndFeel::accentCyan.withAlpha(0.9f));
            g.drawEllipse(screenPos.x - pulseR, screenPos.y - pulseR, pulseR * 2.0f, pulseR * 2.0f, 2.0f);
        }
    }

    // 5. Draw Category Badge Labels ALWAYS ON TOP OF NODES
    for (const auto& cluster : clusters)
    {
        auto screenClusterPos = cloudToScreen(cluster.centerPos);

        juce::String badgeText = cluster.tag.toUpperCase() + " (" + juce::String(cluster.count) + ")";
        juce::Font badgeFont(11.0f, juce::Font::bold);
        int textWidth = badgeFont.getStringWidth(badgeText) + 20;

        juce::Rectangle<float> badgeBounds(screenClusterPos.x - textWidth * 0.5f, screenClusterPos.y - 14.0f, static_cast<float>(textWidth), 22.0f);

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
        g.drawText("TAG CLUSTERS (" + juce::String(nodes.size()) + " SAMPLES)", 22, 12, 200, 20, juce::Justification::left, true);
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

    for (auto& node : nodes)
    {
        float dx = node.targetPos.x - node.currentPos.x;
        float dy = node.targetPos.y - node.currentPos.y;

        if (std::abs(dx) > 0.5f || std::abs(dy) > 0.5f)
        {
            node.currentPos.x += dx * 0.12f;
            node.currentPos.y += dy * 0.12f;
            needsRepaint = true;
        }
    }

    if (needsRepaint || selectedNodeIndex >= 0 || isPanning)
        repaint();
}

void SampleCloudComponent::mouseMove(const juce::MouseEvent& e)
{
    int prevHover = hoveredNodeIndex;
    hoveredNodeIndex = findNodeAtPosition(e.position);

    if (prevHover != hoveredNodeIndex)
        repaint();
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

        // Calculate node size based on duration
        float dur = static_cast<float>(item.durationSeconds);
        node.radius = juce::jlimit(5.5f, 14.0f, 5.5f + dur * 1.5f);

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
    float maxRadius = std::min(bounds.getWidth(), bounds.getHeight()) * 0.38f;

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

    // 2. Assign cluster centroid positions around center
    size_t tagCount = clusters.size();
    for (size_t i = 0; i < tagCount; ++i)
    {
        float angle = (static_cast<float>(i) / static_cast<float>(tagCount)) * juce::MathConstants<float>::twoPi - juce::MathConstants<float>::halfPi;
        float clusterDist = maxRadius * 0.70f;
        clusters[i].centerPos = { centerX + std::cos(angle) * clusterDist, centerY + std::sin(angle) * clusterDist };
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

        // Golden ratio spiral placement per cluster center
        float phi = 2.39996323f; // Golden angle in radians
        float r = 18.0f * std::sqrt(static_cast<float>(countIndex) + 1.0f);
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
    for (size_t i = 0; i < nodes.size(); ++i)
    {
        auto nodeScreenPos = cloudToScreen(nodes[i].currentPos);
        float hitR = (nodes[i].radius + 3.0f) * zoomScale;
        if (nodeScreenPos.getDistanceFrom(screenPos) <= std::max(4.5f, hitR))
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

juce::Colour SampleCloudComponent::getColourForTag(const juce::String& tag) const
{
    auto t = tag.toLowerCase();
    if (t.contains("kick")) return juce::Colour(0xffe91e63);      // Pink/Magenta
    if (t.contains("snare")) return juce::Colour(0xffff9800);     // Amber
    if (t.contains("hat") || t.contains("hihat")) return juce::Colour(0xff00e5ff); // Cyan
    if (t.contains("perc")) return juce::Colour(0xff4caf50);      // Green
    if (t.contains("bass")) return juce::Colour(0xff7c4dff);      // Deep Purple
    if (t.contains("synth") || t.contains("lead")) return juce::Colour(0xffffea00); // Yellow
    if (t.contains("loop")) return juce::Colour(0xff1de9b6);       // Turquoise
    if (t.contains("vocal")) return juce::Colour(0xffff4081);      // Bright Pink

    // Deterministic hue for other tags
    uint32_t hash = static_cast<uint32_t>(tag.hashCode());
    float hue = (hash % 360) / 360.0f;
    return juce::Colour::fromHSV(hue, 0.7f, 0.9f, 1.0f);
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
