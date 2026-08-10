#include "SampleCloudComponent.h"
#include "OpenWavLookAndFeel.h"
#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
 #include <GL/gl.h>
#elif JUCE_MAC
 #include <OpenGL/gl.h>
#elif JUCE_LINUX
 #include <GL/gl.h>
#endif
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <limits>
#include <cstddef>

#if defined(_WIN32)
extern "C" {
    __declspec(dllimport) void __stdcall glEnable(unsigned int cap);
    __declspec(dllimport) void __stdcall glBlendFunc(unsigned int sfactor, unsigned int dfactor);
    __declspec(dllimport) void __stdcall glDrawArrays(unsigned int mode, int first, int count);
}
#endif

namespace openwav
{

SampleCloudComponent::SampleCloudComponent(TagDatabaseManager& db, AudioEngine& engine)
    : dbManager(db), audioEngine(engine)
{
    zoomInButton.onClick = [this] { zoomScale = juce::jlimit(0.3f, 4.0f, zoomScale * 1.25f); repaint(); };
    zoomOutButton.onClick = [this] { zoomScale = juce::jlimit(0.3f, 4.0f, zoomScale * 0.8f); repaint(); };
    resetZoomButton.onClick = [this] { resetZoomAndPan(); };
    
    viewModeButton.onClick = [this] {
        is2DMode = !is2DMode;
        if (is2DMode) {
            saved3DRotX = targetRotX;
            saved3DRotY = targetRotY;
            targetRotX = 0.0f;
            targetRotY = 0.0f;
        } else {
            targetRotX = saved3DRotX;
            targetRotY = saved3DRotY;
        }
        viewModeButton.setButtonText(is2DMode ? "3D" : "2D");
    };

    addAndMakeVisible(zoomInButton);
    addAndMakeVisible(zoomOutButton);
    addAndMakeVisible(resetZoomButton);
    addAndMakeVisible(viewModeButton);
    addAndMakeVisible(autoRotateButton);

    autoRotateButton.onClick = [this] {
        autoRotate = !autoRotate;
        autoRotateButton.setButtonText(autoRotate ? "Stop Spin" : "Spin");
    };

    openGLContext.setOpenGLVersionRequired(juce::OpenGLContext::OpenGLVersion::openGL3_2);
    openGLContext.setRenderer(this);
    openGLContext.attachTo(*this);
    openGLContext.setContinuousRepainting(true); // Enable continuous repainting for twinkling animation

    startTimerHz(60);
}

SampleCloudComponent::~SampleCloudComponent()
{
    openGLContext.detach();
    stopTimer();
    layoutActive = false;
    if (layoutThread.joinable()) layoutThread.join();
}

void SampleCloudComponent::setItems(const std::vector<MediaItem>& items)
{
    uint64_t newHash = items.size();
    for (const auto& item : items)
    {
        newHash ^= static_cast<uint64_t>(item.id.hashCode());
        newHash = (newHash << 5) | (newHash >> 59);
    }
    if (newHash == currentDataHash && !nodes.empty()) return;
    currentDataHash = newHash;

    layoutActive = false;
    if (layoutThread.joinable()) layoutThread.join();

    layoutActive = true;
    layoutPending = true;
    repaint();

    auto itemsCopy = items;
    layoutThread = std::thread([this, itemsCopy]() {
        runLayoutAsync(itemsCopy);
    });
}

void SampleCloudComponent::runLayoutAsync(std::vector<MediaItem> items)
{
    std::vector<CloudNode> newNodes;
    newNodes.reserve(items.size());

    for (const auto& item : items)
    {
        CloudNode node;
        node.item = item;
        node.primaryTag = getPrimaryCategoryTag(item.tags);
        node.colour = getColourForTag(node.primaryTag);
        float dur = static_cast<float>(item.durationSeconds);
        node.radius = juce::jlimit(3.0f, 9.0f, 3.0f + dur * 1.0f);
        node.hoverScale = 1.0f;
        newNodes.push_back(node);
    }

    auto [newClusters, newEdges] = calculateClusterLayoutInternal(newNodes);
    
    if (!layoutActive.load()) return;

    juce::Component::SafePointer<SampleCloudComponent> safeThis(this);
    juce::MessageManager::callAsync([safeThis, newNodes = std::move(newNodes), newClusters = std::move(newClusters), newEdges = std::move(newEdges)]() mutable {
        if (auto* comp = safeThis.getComponent())
        {
            if (!comp->layoutActive.load()) return;
            
            comp->nodes = std::move(newNodes);
            comp->clusters = std::move(newClusters);
            comp->constellationEdges = std::move(newEdges);
    
            comp->vertexBuffer.clear();
            comp->vertexBuffer.reserve(comp->nodes.size() + comp->clusters.size());
            
            // Render a large glowing planet at the center of each cluster for the label
            for (const auto& c : comp->clusters)
            {
                comp->vertexBuffer.push_back({
                    c.centerPos.x, c.centerPos.y, c.centerPos.z,
                    c.colour.getFloatRed(), c.colour.getFloatGreen(), c.colour.getFloatBlue(), c.colour.getFloatAlpha() * 0.85f,
                    38.0f // Massive planet radius
                });
            }
            
            for (const auto& n : comp->nodes)
            {
                comp->vertexBuffer.push_back({
                    n.targetPos.x, n.targetPos.y, n.targetPos.z,
                    n.colour.getFloatRed(), n.colour.getFloatGreen(), n.colour.getFloatBlue(), n.colour.getFloatAlpha(),
                    n.radius
                });
            }
            comp->vboNeedsUpdate = true;
            
            comp->layoutPending = false;
            comp->revealAlpha = 0.0f;
            comp->repaint();
        }
    });
}

std::pair<std::vector<SampleCloudComponent::TagCluster>, std::vector<std::pair<size_t, size_t>>> SampleCloudComponent::calculateClusterLayoutInternal(std::vector<CloudNode>& nodesCopy)
{
    std::vector<TagCluster> outClusters;
    if (nodesCopy.empty()) return {outClusters, {}};

    std::unordered_map<juce::String, TagCluster> clusterMap;
    for (const auto& n : nodesCopy)
    {
        auto& c = clusterMap[n.primaryTag];
        c.tag = n.primaryTag;
        c.colour = n.colour;
        c.count++;
    }

    for (const auto& pair : clusterMap)
        outClusters.push_back(pair.second);

    std::sort(outClusters.begin(), outClusters.end(), [](const TagCluster& a, const TagCluster& b) {
        return a.count > b.count;
    });

    size_t tagCount = outClusters.size();
    if (tagCount > 0)
    {
        float baseRadius = 450.0f;
        float goldenAngle = 2.39996323f;

        for (size_t i = 0; i < tagCount; ++i)
        {
            auto tagLower = outClusters[i].tag.toLowerCase();

            if (tagLower.contains("kick")) outClusters[i].centerPos = { -320.0f, -40.0f, -120.0f };
            else if (tagLower.contains("snare")) outClusters[i].centerPos = { 320.0f, -40.0f, 120.0f };
            else if (tagLower.contains("bass")) outClusters[i].centerPos = { 0.0f, 320.0f, 180.0f };
            else if (tagLower.contains("hat") || tagLower.contains("hihat")) outClusters[i].centerPos = { 0.0f, -320.0f, -180.0f };
            else if (tagLower.contains("perc")) outClusters[i].centerPos = { 450.0f, 160.0f, -220.0f };
            else if (tagLower.contains("synth") || tagLower.contains("lead")) outClusters[i].centerPos = { -450.0f, -160.0f, 220.0f };
            else
            {
                float theta = static_cast<float>(i) * goldenAngle;
                float r = baseRadius + (i * 65.0f);
                float x = r * std::cos(theta);
                float z = r * std::sin(theta);
                float y = (i % 2 == 0 ? 1.0f : -1.0f) * (60.0f + (i * 25.0f));

                outClusters[i].centerPos = { x, y, z };
            }
        }
        
        for (int iter = 0; iter < 16; ++iter)
        {
            for (size_t i = 0; i < tagCount; ++i)
            {
                for (size_t j = i + 1; j < tagCount; ++j)
                {
                    auto delta = outClusters[j].centerPos - outClusters[i].centerPos;
                    float dist = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
                    float r1 = std::max(45.0f, 30.0f + std::sqrt(static_cast<float>(outClusters[i].count)) * 14.0f);
                    float r2 = std::max(45.0f, 30.0f + std::sqrt(static_cast<float>(outClusters[j].count)) * 14.0f);
                    float minDist = r1 + r2 + 60.0f;

                    if (dist < minDist && dist > 0.01f)
                    {
                        float overlap = (minDist - dist) * 0.5f;
                        auto dir = delta / dist;
                        outClusters[i].centerPos = outClusters[i].centerPos - dir * overlap;
                        outClusters[j].centerPos = outClusters[j].centerPos + dir * overlap;
                    }
                }
            }
        }
    }

    std::unordered_map<juce::String, Vector3D> tagCenters;
    std::unordered_map<juce::String, int> tagTotalCounts;
    for (const auto& cl : outClusters)
    {
        tagCenters[cl.tag] = cl.centerPos;
        tagTotalCounts[cl.tag] = cl.count;
    }

    std::unordered_map<juce::String, int> tagItemCounts;
    for (auto& node : nodesCopy)
    {
        auto cPos = tagCenters[node.primaryTag];
        int countIdx = tagItemCounts[node.primaryTag]++;
        int totalInTag = tagTotalCounts[node.primaryTag];

        uint32_t nodeHash = static_cast<uint32_t>(node.item.filePath.hashCode() ^ (countIdx * 2654435761u));

        float phi = 2.39996323f;
        float uNorm = static_cast<float>(countIdx + 0.5f) / std::max(1.0f, static_cast<float>(totalInTag));
        float yDir = 1.0f - 2.0f * uNorm;
        float radiusAtY = std::sqrt(std::max(0.05f, 1.0f - yDir * yDir));

        float maxCloudRadius = std::max(55.0f, 30.0f + std::sqrt(static_cast<float>(totalInTag)) * 16.0f);
        float scatterFactor = std::pow(static_cast<float>(nodeHash % 1000) / 1000.0f, 0.65f);
        float rDist = maxCloudRadius * (0.12f + 0.88f * scatterFactor);

        float theta = countIdx * phi + (nodeHash % 100) * 0.01f;
        float xDir = radiusAtY * std::cos(theta);
        float zDir = radiusAtY * std::sin(theta);

        node.targetPos = { cPos.x + rDist * xDir, cPos.y + rDist * yDir, cPos.z + rDist * zDir };
        node.currentPos = node.targetPos;
    }

    // Edges calculation removed for O(N^2) performance reasons, as OpenGL doesn't render CPU edges anymore
    std::vector<std::pair<size_t, size_t>> outEdges;
    return {outClusters, outEdges};
}

void SampleCloudComponent::selectItemById(const juce::String& itemId)
{
    if (itemId.isEmpty()) return;
    for (size_t i = 0; i < nodes.size(); ++i)
    {
        if (nodes[i].item.id == itemId)
        {
            selectedNodeIndex = static_cast<int>(i);
            repaint();
            break;
        }
    }
}

void SampleCloudComponent::paint(juce::Graphics& g)
{
    if (layoutPending)
    {
        g.setColour(juce::Colours::white);
        g.setFont(18.0f);
        g.drawText("Layout Computing...", getLocalBounds(), juce::Justification::centred);
        return;
    }
    
    // Draw vignette
    bool isLightMode = OpenWavLookAndFeel::bgCard.getBrightness() > 0.5f;
    float w = (float)getWidth();
    float h = (float)getHeight();
    juce::Colour centerCol = isLightMode ? juce::Colours::transparentWhite : juce::Colours::transparentBlack;
    juce::Colour edgeCol = isLightMode ? OpenWavLookAndFeel::bgDark.withAlpha(0.65f) : juce::Colours::black.withAlpha(0.65f);
    juce::ColourGradient vignette(centerCol, 
                                  w * 0.5f, h * 0.5f,
                                  edgeCol, 
                                  0.0f, 0.0f, true);
    g.setGradientFill(vignette);
    g.fillRect(getLocalBounds());
    
    // Draw 2D ring highlight over OpenGL surface
    if (hoveredNodeIndex >= 0 || selectedNodeIndex >= 0)
    {
        if (lastViewProjectionMatrix.mat[15] != 0.0f) 
        {
            float halfW = getWidth() * 0.5f;
            float halfH = getHeight() * 0.5f;
            for (int i : {hoveredNodeIndex, selectedNodeIndex})
            {
                if (i >= 0 && i < nodes.size())
                {
                    auto& n = nodes[i];
                    juce::Vector3D<float> p(n.currentPos.x, n.currentPos.y, n.currentPos.z);
                    float w = lastViewProjectionMatrix.mat[3] * p.x + lastViewProjectionMatrix.mat[7] * p.y + lastViewProjectionMatrix.mat[11] * p.z + lastViewProjectionMatrix.mat[15]; float spX = lastViewProjectionMatrix.mat[0] * p.x + lastViewProjectionMatrix.mat[4] * p.y + lastViewProjectionMatrix.mat[8] * p.z + lastViewProjectionMatrix.mat[12]; float spY = lastViewProjectionMatrix.mat[1] * p.x + lastViewProjectionMatrix.mat[5] * p.y + lastViewProjectionMatrix.mat[9] * p.z + lastViewProjectionMatrix.mat[13]; float spZ = lastViewProjectionMatrix.mat[2] * p.x + lastViewProjectionMatrix.mat[6] * p.y + lastViewProjectionMatrix.mat[10] * p.z + lastViewProjectionMatrix.mat[14];
                    if (spZ > 0.0f)
                    {
                        float sx = halfW + (spX / w) * halfW;
                        float sy = halfH - (spY / w) * halfH;
                        float r = std::max(2.2f, n.radius * 2.0f);
                        g.setColour(i == hoveredNodeIndex ? juce::Colours::white : OpenWavLookAndFeel::accentCyan);
                        g.drawEllipse(sx - r - 2.0f, sy - r - 2.0f, (r + 2.0f) * 2.0f, (r + 2.0f) * 2.0f, 2.0f);
                    }
                }
            }
        }
    }

    if (!clusters.empty())
    {
        juce::Font legendFont(13.0f, juce::Font::bold);
        g.setFont(legendFont);
        
        float padding = 20.0f;
        float circleSize = 12.0f;
        
        float startY = getHeight() - 30.0f;
        float x = 20.0f + legendScrollOffset;
        
        g.saveState();
        g.reduceClipRegion(0, getHeight() - 40, getWidth() - 320, 40);
        
        for (const auto& cluster : clusters)
        {
            float textW = legendFont.getStringWidthFloat(cluster.tag.toUpperCase());
            
            g.setColour(cluster.colour.withAlpha(revealAlpha));
            g.fillEllipse(x, startY + 10.0f - circleSize * 0.5f, circleSize, circleSize);
            
            g.setColour(juce::Colours::white.withAlpha(0.8f * revealAlpha));
            g.drawText(cluster.tag.toUpperCase(), x + circleSize + 6.0f, startY, textW, 20.0f, juce::Justification::centredLeft);
            
            x += circleSize + 6.0f + textW + padding;
        }
        
        g.restoreState();
    }
}

void SampleCloudComponent::timerCallback()
{
    bool needsRepaint = false;

    if (revealAlpha < 1.0f && !layoutPending)
    {
        revealAlpha = std::min(1.0f, revealAlpha + 0.03f);
        needsRepaint = true;
    }

    if (std::abs(targetRotX - rotX) > 0.001f || std::abs(targetRotY - rotY) > 0.001f)
    {
        rotX += (targetRotX - rotX) * 0.1f;
        rotY += (targetRotY - rotY) * 0.1f;
        needsRepaint = true;
    }

    if (!isPanning && !isRotating && !is2DMode && autoRotate)
    {
        targetRotY -= 0.0005f; 
        needsRepaint = true;
    }

    pulsePhase += 0.05f;

    if (needsRepaint) repaint();
}

void SampleCloudComponent::resetZoomAndPan()
{
    zoomScale = 1.0f;
    panOffset = { 0.0f, 0.0f };
    targetRotX = 0.35f;
    targetRotY = 0.45f;
    is2DMode = false;
    viewModeButton.setButtonText("2D");
    repaint();
}

void SampleCloudComponent::resized()
{
    auto b = getLocalBounds();
    zoomInButton.setBounds(b.getRight() - 50, b.getBottom() - 140, 40, 40);
    zoomOutButton.setBounds(b.getRight() - 50, b.getBottom() - 90, 40, 40);
    resetZoomButton.setBounds(b.getRight() - 120, b.getBottom() - 40, 110, 30);
    viewModeButton.setBounds(b.getRight() - 170, b.getBottom() - 40, 40, 30);
    autoRotateButton.setBounds(b.getRight() - 280, b.getBottom() - 40, 100, 30);
}

void SampleCloudComponent::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown() || e.mods.isPopupMenu())
    {
        if (hoveredNodeIndex >= 0)
            showContextMenuForNode(hoveredNodeIndex);
        return;
    }

    mouseDragStartPos = e.position;
    isRotating = false;
    isPanning = false;

    if (e.mods.isShiftDown())
    {
        isPanning = true;
        dragStartPan = panOffset;
    }
    else
    {
        isRotating = true;
        dragStartRotX = targetRotX;
        dragStartRotY = targetRotY;
    }
}

void SampleCloudComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown() || e.mods.isPopupMenu()) return;

    if (is2DMode && isRotating) {
        isRotating = false;
        isPanning = true;
        dragStartPan = panOffset;
    }

    auto delta = e.position - mouseDragStartPos;
    if (isPanning)
    {
        panOffset.x = dragStartPan.x - (delta.x * 2.0f / zoomScale);
        panOffset.y = dragStartPan.y - (delta.y * 2.0f / zoomScale);
    }
    else if (isRotating)
    {
        targetRotY = dragStartRotY - (delta.x * 0.01f);
        targetRotX = juce::jlimit(-juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, dragStartRotX - (delta.y * 0.01f));
    }
    repaint();
}

void SampleCloudComponent::mouseUp(const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown() || e.mods.isPopupMenu()) return;

    if (e.getDistanceFromDragStart() < 15.0f)
    {
        if (hoveredNodeIndex >= 0)
        {
            selectedNodeIndex = hoveredNodeIndex;
            listeners.call([&](SampleCloudListener& l) { l.cloudSampleSelected(nodes[selectedNodeIndex].item); });
            repaint();
        }
    }
    isRotating = false;
    isPanning = false;
}

void SampleCloudComponent::mouseMove(const juce::MouseEvent& e)
{
    if (nodes.empty() || lastViewProjectionMatrix.mat[15] == 0.0f) return;
    
    int bestIndex = -1;
    float bestZ = std::numeric_limits<float>::max();
    float halfW = getWidth() * 0.5f;
    float halfH = getHeight() * 0.5f;

    for (size_t i = 0; i < nodes.size(); ++i)
    {
        auto& n = nodes[i];
        juce::Vector3D<float> p(n.currentPos.x, n.currentPos.y, n.currentPos.z);
        float w = lastViewProjectionMatrix.mat[3] * p.x + lastViewProjectionMatrix.mat[7] * p.y + lastViewProjectionMatrix.mat[11] * p.z + lastViewProjectionMatrix.mat[15];
        if (w > 0.0f)
        {
            float spX = lastViewProjectionMatrix.mat[0] * p.x + lastViewProjectionMatrix.mat[4] * p.y + lastViewProjectionMatrix.mat[8] * p.z + lastViewProjectionMatrix.mat[12];
            float spY = lastViewProjectionMatrix.mat[1] * p.x + lastViewProjectionMatrix.mat[5] * p.y + lastViewProjectionMatrix.mat[9] * p.z + lastViewProjectionMatrix.mat[13];
            float spZ = lastViewProjectionMatrix.mat[2] * p.x + lastViewProjectionMatrix.mat[6] * p.y + lastViewProjectionMatrix.mat[10] * p.z + lastViewProjectionMatrix.mat[14];
            float sx = halfW + (spX / w) * halfW;
            float sy = halfH - (spY / w) * halfH;
            float dx = sx - e.x;
            float dy = sy - e.y;
            if (dx*dx + dy*dy < 144.0f)
            {
                if (spZ < bestZ)
                {
                    bestZ = spZ;
                    bestIndex = static_cast<int>(i);
                }
            }
        }
    }
    
    if (bestIndex != hoveredNodeIndex)
    {
        hoveredNodeIndex = bestIndex;
        repaint();
    }
}

void SampleCloudComponent::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (e.y > getHeight() - 40 && e.x < getWidth() - 320)
    {
        float totalWidth = 0.0f;
        juce::Font legendFont(13.0f, juce::Font::bold);
        for (const auto& cluster : clusters)
        {
            totalWidth += 12.0f + 6.0f + legendFont.getStringWidthFloat(cluster.tag.toUpperCase()) + 20.0f;
        }
        
        float viewWidth = getWidth() - 320.0f - 20.0f;
        float maxScroll = 0.0f;
        if (totalWidth > viewWidth)
            maxScroll = totalWidth - viewWidth;
            
        legendScrollOffset += wheel.deltaX * 150.0f + wheel.deltaY * 150.0f;
        legendScrollOffset = juce::jlimit(-maxScroll, 0.0f, legendScrollOffset);
        repaint();
    }
    else
    {
        zoomScale = juce::jlimit(0.3f, 4.0f, zoomScale * (1.0f + wheel.deltaY * 2.0f));
        repaint();
    }
}

void SampleCloudComponent::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown() || e.mods.isPopupMenu()) return;

    if (hoveredNodeIndex >= 0)
    {
        listeners.call([&](SampleCloudListener& l) { l.cloudSampleDoubleClicked(nodes[hoveredNodeIndex].item); });
    }
}

void SampleCloudComponent::addListener(SampleCloudListener* listener) { listeners.add(listener); }
void SampleCloudComponent::removeListener(SampleCloudListener* listener) { listeners.remove(listener); }

juce::String SampleCloudComponent::getPrimaryCategoryTag(const std::set<juce::String>& tags) const
{
    if (tags.empty()) return "Untagged";
    return *tags.begin();
}

juce::Colour SampleCloudComponent::getColourForTag(const juce::String& tag) const
{
    auto t = tag.toLowerCase();
    if (t.contains("kick")) return juce::Colour(0xffe53935);
    if (t.contains("snare")) return juce::Colour(0xfff57c00);
    if (t.contains("hat") || t.contains("hihat")) return juce::Colour(0xff0288d1);
    if (t.contains("perc")) return juce::Colour(0xff2e7d32);
    if (t.contains("bass")) return juce::Colour(0xff673ab7);
    if (t.contains("synth") || t.contains("lead")) return juce::Colour(0xffffb300);
    if (t.contains("loop")) return juce::Colour(0xff00897b);
    if (t.contains("vocal")) return juce::Colour(0xffc2185b);

    uint32_t hash = static_cast<uint32_t>(tag.hashCode());
    float hue = (hash % 360) / 360.0f;
    return juce::Colour::fromHSV(hue, 0.65f, 0.85f, 1.0f);
}

void SampleCloudComponent::showContextMenuForNode(int idx)
{
    if (idx < 0 || idx >= static_cast<int>(nodes.size())) return;
    const auto& item = nodes[idx].item;

    juce::PopupMenu menu;
    menu.addSectionHeader(item.fileName);
    menu.addItem(1, item.isFavorite ? "Remove from Favorites" : "Add to Favorites");
    menu.addItem(2, "Add Custom Tag...");
    menu.addItem(3, "Reveal in Explorer");

    menu.showMenuAsync(juce::PopupMenu::Options(), [this, item](int result) {
        if (result == 1) dbManager.toggleFavorite(item.id);
        else if (result == 3) juce::File(item.filePath).revealToUser();
    });
}

// ==============================================================================
// RAW OPENGL RENDERING
// ==============================================================================

#ifndef GL_FALSE
 #define GL_FALSE 0
#endif
#ifndef GL_FLOAT
 #define GL_FLOAT 0x1406
#endif
#ifndef GL_POINTS
 #define GL_POINTS 0x0000
#endif
#ifndef GL_ARRAY_BUFFER
 #define GL_ARRAY_BUFFER 0x8892
#endif
#ifndef GL_STATIC_DRAW
 #define GL_STATIC_DRAW 0x88E4
#endif
#ifndef GL_BLEND
 #define GL_BLEND 0x0BE2
#endif
#ifndef GL_SRC_ALPHA
 #define GL_SRC_ALPHA 0x0302
#endif
#ifndef GL_ONE_MINUS_SRC_ALPHA
 #define GL_ONE_MINUS_SRC_ALPHA 0x0303
#endif

void SampleCloudComponent::newOpenGLContextCreated()
{
    openGLContext.extensions.glGenBuffers(1, &vbo);
    vboNeedsUpdate = true;

    juce::String vertexShader =
        "#version 150\n"
        "in vec3 position;\n"
        "in vec4 colour;\n"
        "in float radius;\n"
        "uniform mat4 projectionMatrix;\n"
        "uniform mat4 viewMatrix;\n"
        "uniform float revealAlpha;\n"
        "uniform float time;\n"
        "out vec4 destColour;\n"
        
        "float rand(vec2 co){ return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453); }\n"
        
        "void main()\n"
        "{\n"
        "    vec4 viewPos = viewMatrix * vec4(position, 1.0);\n"
        "    gl_Position = projectionMatrix * viewPos;\n"
        "    float phase = position.x * 0.1 + position.y * 0.15 + position.z * 0.05;\n"
        "    float noise = rand(position.xy * 0.01) * 0.5;\n"
        "    float twinkle = 0.65 + 0.35 * sin(time * (1.5 + noise) + phase) * cos(time * 0.8 + phase * 1.3);\n"
        "    float depthFade = clamp(600.0 / max(1.0, -viewPos.z), 0.15, 1.0);\n"
        "    destColour = colour;\n"
        "    destColour.rgb *= twinkle * depthFade;\n"
        "    destColour.a *= revealAlpha * depthFade;\n"
        "    gl_PointSize = max(3.0, radius * 6000.0 / max(1.0, -viewPos.z));\n"
        "}\n";

    juce::String fragmentShader =
        "#version 150\n"
        "in vec4 destColour;\n"
        "out vec4 finalColour;\n"
        "void main()\n"
        "{\n"
        "    vec2 p = gl_PointCoord * 2.0 - vec2(1.0);\n"
        "    float r = dot(p, p);\n"
        "    if (r > 1.0) discard;\n"
        "    float alpha = 1.0 - smoothstep(0.8, 1.0, r);\n"
        "    finalColour = vec4(destColour.rgb, destColour.a * alpha);\n"
        "}\n";

    shaderProgram.reset(new juce::OpenGLShaderProgram(openGLContext));
    if (shaderProgram->addVertexShader(vertexShader) &&
        shaderProgram->addFragmentShader(fragmentShader) &&
        shaderProgram->link())
    {
        // Success
    }
}

void SampleCloudComponent::renderOpenGL()
{
    bool isLightMode = OpenWavLookAndFeel::bgCard.getBrightness() > 0.5f;
    juce::OpenGLHelpers::clear(OpenWavLookAndFeel::bgCard);

    if (layoutPending || !shaderProgram || vertexBuffer.empty()) return;

    // We must manually call glEnable and glBlendFunc
    // Since we don't have glew, we can use the JUCE OpenGLHelpers or just rely on the driver
    // But glBlendFunc, glEnable, glDrawArrays are standard OpenGL 1.1 available in opengl32.dll
    #if !defined(_WIN32)
    juce::gl::glEnable(GL_BLEND);
    juce::gl::glBlendFunc(GL_SRC_ALPHA, isLightMode ? GL_ONE_MINUS_SRC_ALPHA : 1); 
    #else
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, isLightMode ? GL_ONE_MINUS_SRC_ALPHA : 1);
    #endif
    
    #ifndef GL_PROGRAM_POINT_SIZE
    #define GL_PROGRAM_POINT_SIZE 0x8642
    #endif
    
    #if !defined(_WIN32)
    juce::gl::glEnable(GL_PROGRAM_POINT_SIZE);
    #else
    glEnable(GL_PROGRAM_POINT_SIZE);
    #endif

    shaderProgram->use();

    float aspect = getWidth() / (float)getHeight();
    auto projectionMatrix = juce::Matrix3D<float>::fromFrustum(-aspect * 0.1f, aspect * 0.1f, -0.1f, 0.1f, 0.2f, 10000.0f);
    
    juce::Matrix3D<float> viewMatrix;
    viewMatrix = viewMatrix * juce::Matrix3D<float>::fromTranslation({-panOffset.x, panOffset.y, -cameraDistance / zoomScale});
    
    // Matrix3D rotation requires manual building or usage of rotation(Vector3D)
    float crx = std::cos(rotX), srx = std::sin(rotX);
    float cry = std::cos(rotY), sry = std::sin(rotY);
    juce::Matrix3D<float> rotMatrix(
        cry, 0.0f, sry, 0.0f,
        srx*sry, crx, -srx*cry, 0.0f,
        -crx*sry, srx, crx*cry, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    );
    viewMatrix = viewMatrix * rotMatrix;
    
    lastViewProjectionMatrix = projectionMatrix * viewMatrix;

    GLuint progID = shaderProgram->getProgramID();
    openGLContext.extensions.glUniformMatrix4fv(openGLContext.extensions.glGetUniformLocation(progID, "projectionMatrix"), 1, GL_FALSE, projectionMatrix.mat);
    openGLContext.extensions.glUniformMatrix4fv(openGLContext.extensions.glGetUniformLocation(progID, "viewMatrix"), 1, GL_FALSE, viewMatrix.mat);
    openGLContext.extensions.glUniform1f(openGLContext.extensions.glGetUniformLocation(progID, "revealAlpha"), revealAlpha);
    openGLContext.extensions.glUniform1f(openGLContext.extensions.glGetUniformLocation(progID, "time"), static_cast<float>(juce::Time::getMillisecondCounterHiRes() / 1000.0));
    openGLContext.extensions.glUniform1f(openGLContext.extensions.glGetUniformLocation(progID, "isLightMode"), isLightMode ? 1.0f : 0.0f);

    openGLContext.extensions.glBindBuffer(GL_ARRAY_BUFFER, vbo);

    if (vboNeedsUpdate)
    {
        openGLContext.extensions.glBufferData(GL_ARRAY_BUFFER, vertexBuffer.size() * sizeof(Vertex), vertexBuffer.data(), GL_STATIC_DRAW);
        vboNeedsUpdate = false;
    }

    GLint posAttr = openGLContext.extensions.glGetAttribLocation(progID, "position");
    GLint colAttr = openGLContext.extensions.glGetAttribLocation(progID, "colour");
    GLint radAttr = openGLContext.extensions.glGetAttribLocation(progID, "radius");

    if (posAttr >= 0)
    {
        openGLContext.extensions.glVertexAttribPointer(posAttr, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)offsetof(Vertex, x));
        openGLContext.extensions.glEnableVertexAttribArray(posAttr);
    }
    if (colAttr >= 0)
    {
        openGLContext.extensions.glVertexAttribPointer(colAttr, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)offsetof(Vertex, r));
        openGLContext.extensions.glEnableVertexAttribArray(colAttr);
    }
    if (radAttr >= 0)
    {
        openGLContext.extensions.glVertexAttribPointer(radAttr, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)offsetof(Vertex, radius));
        openGLContext.extensions.glEnableVertexAttribArray(radAttr);
    }
    #if !defined(_WIN32)
    juce::gl::glDrawArrays(GL_POINTS, 0, (GLsizei)vertexBuffer.size());
    #else
    glDrawArrays(GL_POINTS, 0, (GLsizei)vertexBuffer.size());
    #endif
    if (posAttr >= 0) openGLContext.extensions.glDisableVertexAttribArray(posAttr);
    if (colAttr >= 0) openGLContext.extensions.glDisableVertexAttribArray(colAttr);
    if (radAttr >= 0) openGLContext.extensions.glDisableVertexAttribArray(radAttr);
}

void SampleCloudComponent::openGLContextClosing()
{
    openGLContext.extensions.glDeleteBuffers(1, &vbo);
    shaderProgram.reset();
}

} // namespace openwav
