#include "SampleCloudComponent.h"
#include "OpenWavLookAndFeel.h"
#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <GL/gl.h>
#include <windows.h>
#elif JUCE_LINUX
#include <GL/gl.h>
#endif
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <unordered_map>

#if defined(_WIN32)
extern "C" {
__declspec(dllimport) void __stdcall glEnable(unsigned int cap);
__declspec(dllimport) void __stdcall glBlendFunc(unsigned int sfactor,
                                                 unsigned int dfactor);
__declspec(dllimport) void __stdcall glDrawArrays(unsigned int mode, int first,
                                                  int count);
}
#endif

namespace openwav {

juce::String SampleCloudComponent::getAtlasCategoryName(const juce::String& tag) {
    juce::String cat = TagDatabaseManager::getCanonicalCategory(tag);
    if (cat == "other" && tag.isNotEmpty() && !tag.equalsIgnoreCase("#other") &&
        !tag.equalsIgnoreCase("#oneshot") && !tag.equalsIgnoreCase("untagged")) {
        juce::String clean = tag.startsWith("#") ? tag.substring(1).toLowerCase() : tag.toLowerCase();
        if (clean.isNotEmpty())
            return clean;
    }
    return cat;
}

juce::String SampleCloudComponent::getCanonicalAtlasCategory(const MediaItem& item) {
    return TagDatabaseManager::getCanonicalCategoryForItem(item);
}

SampleCloudComponent::CloudOverlayComponent::CloudOverlayComponent(
    SampleCloudComponent &ownerComp)
    : owner(ownerComp) {
  setInterceptsMouseClicks(true, true);
  addAndMakeVisible(owner.zoomInButton);
  addAndMakeVisible(owner.zoomOutButton);
  addAndMakeVisible(owner.resetZoomButton);
  addAndMakeVisible(owner.viewModeButton);
  addAndMakeVisible(owner.autoRotateButton);
  addAndMakeVisible(owner.hoverPlayButton);
}

void SampleCloudComponent::CloudOverlayComponent::paint(juce::Graphics &g) {
  owner.paintOverlay(g);
}

void SampleCloudComponent::CloudOverlayComponent::resized() {
  auto b = getLocalBounds();
  int dockRight = b.getRight() - 15;
  int dockBottom = b.getBottom() - 15;

  owner.hoverPlayButton.setBounds(15, 15, 120, 32);

  owner.zoomInButton.setBounds(dockRight - 36, dockBottom - 145, 36, 36);
  owner.zoomOutButton.setBounds(dockRight - 36, dockBottom - 102, 36, 36);

  owner.resetZoomButton.setBounds(dockRight - 95, dockBottom - 36, 95, 32);
  owner.viewModeButton.setBounds(dockRight - 145, dockBottom - 36, 44, 32);
  owner.autoRotateButton.setBounds(dockRight - 245, dockBottom - 36, 94, 32);
}

void SampleCloudComponent::CloudOverlayComponent::mouseDown(
    const juce::MouseEvent &e) {
  owner.mouseDown(e);
}
void SampleCloudComponent::CloudOverlayComponent::mouseDrag(
    const juce::MouseEvent &e) {
  owner.mouseDrag(e);
}
void SampleCloudComponent::CloudOverlayComponent::mouseUp(
    const juce::MouseEvent &e) {
  owner.mouseUp(e);
}
void SampleCloudComponent::CloudOverlayComponent::mouseMove(
    const juce::MouseEvent &e) {
  owner.mouseMove(e);
}
void SampleCloudComponent::CloudOverlayComponent::mouseExit(
    const juce::MouseEvent &e) {
  owner.mouseExit(e);
}
void SampleCloudComponent::CloudOverlayComponent::mouseWheelMove(
    const juce::MouseEvent &e, const juce::MouseWheelDetails &wheel) {
  owner.mouseWheelMove(e, wheel);
}
void SampleCloudComponent::CloudOverlayComponent::mouseDoubleClick(
    const juce::MouseEvent &e) {
  owner.mouseDoubleClick(e);
}

SampleCloudComponent::SampleCloudComponent(TagDatabaseManager &db,
                                           AudioEngine &engine)
    : dbManager(db), audioEngine(engine), overlayComponent(*this) {
  zoomInButton.onClick = [this] {
    targetZoomScale = juce::jlimit(0.20f, 3.5f, targetZoomScale * 1.25f);
  };
  zoomOutButton.onClick = [this] {
    targetZoomScale = juce::jlimit(0.20f, 3.5f, targetZoomScale * 0.8f);
  };
  resetZoomButton.onClick = [this] { resetZoomAndPan(); };

  viewModeButton.onClick = [this] {
    is2DMode = !is2DMode;
    if (is2DMode) {
      saved3DRotX = targetRotX;
      saved3DRotY = targetRotY;
      targetRotX = 0.0f;
      targetRotY = 0.0f;
      rotX = 0.0f;
      rotY = 0.0f;
      targetCameraCenterPos.z = 0.0f;
      cameraCenterPos.z = 0.0f;
      autoRotateButton.setEnabled(false);
    } else {
      targetRotX = saved3DRotX;
      targetRotY = saved3DRotY;
      autoRotateButton.setEnabled(true);
    }
    viewModeButton.setButtonText(is2DMode ? "3D" : "2D");
    rebuildGpuBuffers();
  };

  autoRotateButton.onClick = [this] {
    autoRotate = !autoRotate;
    autoRotateButton.setButtonText(autoRotate ? "Stop Spin" : "Spin");
  };

  hoverPlayButton.setClickingTogglesState(true);
  hoverPlayButton.setToggleState(false, juce::dontSendNotification);
  hoverPlayButton.setTooltip("Automatically trigger sample playback on mouseover");
  hoverPlayButton.onClick = [this] {
    hoverPlayEnabled = hoverPlayButton.getToggleState();
    hoverPlayButton.setButtonText(hoverPlayEnabled ? "Mouseover: On" : "Mouseover: Off");
    hoveredNodeIndex = -1;
  };

#if JUCE_MAC
  metalView = std::make_unique<MetalCloudView>();
  if (metalView->initialize()) {
    metalContainer = std::make_unique<juce::NSViewComponent>();
    metalContainer->setView(metalView->getNativeView());
    metalContainer->setInterceptsMouseClicks(false, false);
    addChildComponent(metalContainer.get());
    metalContainer->toBack();
    metalContainer->setVisible(true);
  }
#else
  addAndMakeVisible(overlayComponent);
  openGLContext.setOpenGLVersionRequired(
      juce::OpenGLContext::OpenGLVersion::openGL3_2);
  openGLContext.setRenderer(this);
  openGLContext.attachTo(*this);
  openGLContext.setContinuousRepainting(true);
#endif

  startTimerHz(60);
}

SampleCloudComponent::~SampleCloudComponent() {
#if JUCE_MAC
  if (overlayComponent.isOnDesktop())
    overlayComponent.removeFromDesktop();
  if (metalView)
    metalView->setPaused(true);
  metalContainer.reset();
  metalView.reset();
#else
  openGLContext.detach();
#endif
  stopTimer();
  layoutActive = false;
  if (layoutThread.joinable())
    layoutThread.join();
}

void SampleCloudComponent::parentHierarchyChanged() {
#if JUCE_MAC
  if (auto *peer = getPeer()) {
    if (!overlayComponent.isOnDesktop()) {
      overlayComponent.addToDesktop(
          juce::ComponentPeer::windowIsSemiTransparent,
          peer->getNativeHandle());
      overlayComponent.setVisible(isShowing());
      overlayComponent.toFront(false);
      auto peerArea = peer->getAreaCoveredBy(*this);
      overlayComponent.setBounds(peerArea);
    }
  } else {
    if (overlayComponent.isOnDesktop()) {
      overlayComponent.removeFromDesktop();
    }
  }
#endif
}

void SampleCloudComponent::visibilityChanged() {
#if JUCE_MAC
  if (metalView)
    metalView->setPaused(!isShowing());
  overlayComponent.setVisible(isShowing());
  if (isShowing()) {
    overlayComponent.toFront(false);
    if (auto *peer = getPeer())
      overlayComponent.setBounds(peer->getAreaCoveredBy(*this));
  }
#else
  if (!isShowing())
    openGLContext.setContinuousRepainting(false);
  else
    openGLContext.setContinuousRepainting(true);
#endif
}

void SampleCloudComponent::setItems(const std::vector<MediaItem> &items) {
  uint64_t newHash = items.size();
  for (const auto &item : items) {
    newHash ^= static_cast<uint64_t>(item.id.hashCode());
    newHash = (newHash << 5) | (newHash >> 59);
  }
  if (newHash == currentDataHash && !nodes.empty())
    return;
  currentDataHash = newHash;

  layoutActive = false;
  if (layoutThread.joinable())
    layoutThread.join();

  if (items.empty()) {
    nodes.clear();
    clusters.clear();
    constellationEdges.clear();
    vertexBuffer.clear();
    lineVertexBuffer.clear();
#if JUCE_MAC
    if (metalView) {
      metalView->updateVertices({});
      metalView->updateLines({});
    }
#else
    vboNeedsUpdate = true;
#endif
    overlayComponent.repaint();
    repaint();
    return;
  }

  layoutPending = true;
  layoutActive = true;
  overlayComponent.repaint();
  repaint();

  runLayoutAsync(items);
}

void SampleCloudComponent::runLayoutAsync(std::vector<MediaItem> items) {
  std::vector<CloudNode> newNodes;
  newNodes.reserve(items.size());

  for (const auto &item : items) {
    CloudNode node;
    node.item = item;
    node.primaryTag = getPrimaryCategoryTag(item.tags);
    node.canonicalCategory = getCanonicalAtlasCategory(item).toLowerCase();
    node.colour = getColourForTag(node.primaryTag);
    node.radius = 4.0f;
    node.hoverScale = 1.0f;
    newNodes.push_back(std::move(node));
  }

  auto [newClusters, newEdges] = calculateClusterLayoutInternal(newNodes);

  if (!layoutActive.load())
    return;

  juce::Component::SafePointer<SampleCloudComponent> safeThis(this);
  juce::MessageManager::callAsync([safeThis, newNodes = std::move(newNodes),
                                   newClusters = std::move(newClusters),
                                   newEdges = std::move(newEdges)]() mutable {
    if (auto *comp = safeThis.getComponent()) {
      if (!comp->layoutActive.load())
        return;

      comp->nodes = std::move(newNodes);
      comp->clusters = std::move(newClusters);
      comp->constellationEdges = std::move(newEdges);
      comp->recentSelectionHistory.clear();

      comp->rebuildGpuBuffers();

      comp->layoutPending = false;
      comp->revealAlpha = 0.0f;
      if (comp->selectedNodeIndex >= 0 && comp->selectedNodeIndex < static_cast<int>(comp->nodes.size())) {
        comp->updateSimilarStack();
      }
      comp->overlayComponent.repaint();
      comp->repaint();
    }
  });
}

void SampleCloudComponent::rebuildGpuBuffers() {
  vertexBuffer.clear();
  lineVertexBuffer.clear();

  for (auto &c : clusters) {
    c.centerPos = is2DMode ? c.centerPos2D : c.centerPos3D;
  }

  vertexBuffer.reserve(nodes.size());
  for (auto &n : nodes) {
    if (is2DMode) {
      n.currentPos = n.pos2D;
      n.targetPos = n.pos2D;
      vertexBuffer.push_back({
          n.pos2D.x, n.pos2D.y, 0.0f,
          n.colour.getFloatRed(), n.colour.getFloatGreen(),
          n.colour.getFloatBlue(), n.colour.getFloatAlpha(),
          n.radius * 1.15f
      });
    } else {
      n.currentPos = n.pos3D;
      n.targetPos = n.pos3D;
      vertexBuffer.push_back({
          n.pos3D.x, n.pos3D.y, n.pos3D.z,
          n.colour.getFloatRed(), n.colour.getFloatGreen(),
          n.colour.getFloatBlue(), n.colour.getFloatAlpha(),
          n.radius
      });
    }
  }

    for (const auto &edge : constellationEdges) {
      if (edge.first < clusters.size() && edge.second < clusters.size()) {
        const auto &c1 = clusters[edge.first];
        const auto &c2 = clusters[edge.second];
        lineVertexBuffer.push_back({
            c1.centerPos3D.x, c1.centerPos3D.y, c1.centerPos3D.z,
            c1.colour.getFloatRed(), c1.colour.getFloatGreen(),
            c1.colour.getFloatBlue(), 0.55f
        });
        lineVertexBuffer.push_back({
            c2.centerPos3D.x, c2.centerPos3D.y, c2.centerPos3D.z,
            c2.colour.getFloatRed(), c2.colour.getFloatGreen(),
            c2.colour.getFloatBlue(), 0.55f
        });
      }
    }

#if JUCE_MAC
  if (metalView) {
    std::vector<MetalVertex> mVerts;
    mVerts.reserve(vertexBuffer.size());
    for (const auto &v : vertexBuffer) {
      mVerts.push_back({v.x, v.y, v.z, v.r, v.g, v.b, v.a, v.radius});
    }
    metalView->updateVertices(mVerts);

    std::vector<MetalLineVertex> mLines;
    mLines.reserve(lineVertexBuffer.size());
    for (const auto &lv : lineVertexBuffer) {
      mLines.push_back({lv.x, lv.y, lv.z, lv.r, lv.g, lv.b, lv.a});
    }
    metalView->updateLines(mLines);
  }
#else
  vboNeedsUpdate = true;
#endif

  updateMapBounds();
  overlayComponent.repaint();
  repaint();
}

std::pair<std::vector<SampleCloudComponent::TagCluster>,
          std::vector<std::pair<size_t, size_t>>>
SampleCloudComponent::calculateClusterLayoutInternal(
    std::vector<CloudNode> &nodesCopy) {
  std::vector<TagCluster> outClusters;
  if (nodesCopy.empty())
    return {outClusters, {}};

  std::unordered_map<juce::String, TagCluster> clusterMap;
  for (const auto &n : nodesCopy) {
    auto &c = clusterMap[n.primaryTag];
    c.tag = n.primaryTag;
    c.colour = n.colour;
    c.count++;
  }

  for (const auto &pair : clusterMap)
    outClusters.push_back(pair.second);

  size_t tagCount = outClusters.size();
  struct AtlasAnchorDef {
    float x;       // -1.0 to 1.0 (left to right)
    float y;       // -1.0 to 1.0 (bottom to top)
    float z;       // -1.0 to 1.0 (depth)
    float angle;   // Main elongation angle (radians)
    float stretch; // Aspect ratio / filament length (1.0 = round, 2.5 = long nebula arm)
  };

  static const std::unordered_map<juce::String, AtlasAnchorDef> atlasAnchors = {
    // Left side: Kicks, Bass, Toms, Bongos (Low end & organic punch)
    { "kick",              { -0.62f, -0.22f, -0.15f, -0.45f, 2.0f } },
    { "bass",              { -0.78f, -0.58f, -0.25f, -0.60f, 2.1f } },
    { "tom",               { -0.44f,  0.42f,  0.15f,  2.10f, 2.3f } },
    { "bongo/conga",       { -0.14f,  0.78f, -0.10f,  0.85f, 1.6f } },
    { "guitar",            { -0.70f,  0.22f, -0.12f, -0.80f, 1.8f } },
    { "keys",              { -0.88f,  0.08f,  0.15f,  1.10f, 1.7f } },
    { "strings",           { -0.38f,  0.18f,  0.22f,  0.40f, 1.6f } },
    { "brass",             { -0.10f,  0.26f, -0.08f, -0.30f, 1.5f } },
    { "woodwinds",         { -0.24f,  0.54f,  0.10f,  0.70f, 1.5f } },
    { "orchestral",        { -0.30f,  0.34f,  0.18f,  0.55f, 1.6f } },

    // Center & Top: Clave, Shakers, Cowbell, Vocals, Synths, Other (Top transients & Melodic core)
    { "clave/rim",         {  0.20f,  0.94f,  0.05f,  0.05f, 1.5f } },
    { "cowbell",           {  0.04f,  0.46f,  0.12f,  0.30f, 1.4f } },
    { "tambourine shaker", {  0.28f,  0.62f, -0.15f, -0.50f, 1.7f } },
    { "vocal",             {  0.02f,  0.08f,  0.12f,  0.00f, 1.5f } },
    { "synth",             { -0.10f, -0.18f,  0.25f,  0.90f, 2.0f } },
    { "percussion",        {  0.08f,  0.20f, -0.05f,  0.50f, 1.7f } },
    { "other",             {  0.18f, -0.42f,  0.00f, -0.20f, 1.7f } },
    { "oneshot",           {  0.22f, -0.40f,  0.00f, -0.20f, 1.7f } },
    { "loop",              { -0.38f, -0.48f,  0.08f, -0.50f, 1.8f } },

    // Right side: Snares, Claps, HiHats, Cymbals, Rides, FX (Highs, Snappy transients & Splash)
    { "hihat closed",      {  0.52f,  0.72f,  0.20f,  0.60f, 1.9f } },
    { "hihat",             {  0.50f,  0.66f,  0.18f,  0.55f, 1.9f } },
    { "snare",             {  0.62f,  0.38f,  0.05f,  1.40f, 2.4f } },
    { "clap",              {  0.75f,  0.08f, -0.18f,  1.50f, 2.2f } },
    { "snap",              {  0.70f,  0.18f, -0.10f,  1.20f, 1.6f } },
    { "hihat open",        {  0.68f, -0.26f,  0.12f, -0.40f, 1.7f } },
    { "ride",              {  1.05f,  0.05f,  0.00f,  1.55f, 2.4f } },
    { "crash",             {  0.78f, -0.72f, -0.12f,  0.20f, 1.6f } },
    { "cymbal",            {  0.88f, -0.35f,  0.00f,  0.80f, 1.8f } },
    { "fx",                {  0.42f, -0.12f, -0.22f, -1.10f, 1.7f } },
  };

  auto findAnchor = [&](const juce::String &rawTag) -> AtlasAnchorDef {
    juce::String t = rawTag.trim().toLowerCase();
    if (t.startsWith("#")) t = t.substring(1);

    // Fine-grained sub-tag anchors within their semantic region
    if (t == "subkick") return { -0.66f, -0.28f, -0.18f, -0.48f, 2.0f };
    if (t == "subbass") return { -0.82f, -0.64f, -0.28f, -0.62f, 2.1f };
    if (t == "subdrop" || t == "impact") return {  0.34f, -0.24f, -0.24f, -1.05f, 1.7f };
    if (t == "riser" || t == "sweep" || t == "downlifter") return {  0.48f, -0.06f, -0.20f, -1.15f, 1.9f };
    if (t == "atmosphere" || t == "texture" || t == "vinyl" || t == "foley" || t == "glitch")
      return {  0.40f, -0.18f, -0.18f, -0.95f, 1.7f };
    if (t == "pad")   return { -0.16f, -0.24f,  0.26f,  0.85f, 1.9f };
    if (t == "lead")  return { -0.06f, -0.12f,  0.24f,  0.95f, 2.0f };
    if (t == "pluck" || t == "arp") return { -0.08f, -0.20f,  0.22f,  0.90f, 1.8f };
    if (t == "oneshot" || t == "one_shot") return atlasAnchors.at("oneshot");

    // Resolve via Single-Source-of-Truth canonical category (guarantees every instrument/FX/loop tag maps cleanly!)
    juce::String canonical = TagDatabaseManager::getCanonicalCategory(rawTag);
    auto it = atlasAnchors.find(canonical);
    if (it != atlasAnchors.end())
      return it->second;

    auto itRaw = atlasAnchors.find(t);
    if (itRaw != atlasAnchors.end())
      return itRaw->second;

    return { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
  };

  if (tagCount > 0) {
    float baseSpread2D = std::max(520.0f, std::sqrt(static_cast<float>(nodesCopy.size())) * 16.0f);
    float baseSpread3D = std::max(480.0f, std::sqrt(static_cast<float>(nodesCopy.size())) * 14.0f);

    for (size_t i = 0; i < tagCount; ++i) {
      auto anc = findAnchor(outClusters[i].tag);
      float ax = anc.x, ay = anc.y, az = anc.z;
      if (anc.stretch <= 0.01f) {
        float ang = (2.0f * juce::MathConstants<float>::pi * static_cast<float>(i)) / static_cast<float>(tagCount);
        ax = std::cos(ang) * 0.85f;
        ay = std::sin(ang) * 0.85f;
        az = ((static_cast<int>(i) % 3) - 1) * 0.25f;
      }

      outClusters[i].centerPos2D = { ax * baseSpread2D, ay * baseSpread2D * 0.90f, 0.0f };
      outClusters[i].centerPos3D = { ax * baseSpread3D, ay * baseSpread3D * 0.85f, az * baseSpread3D };
    }

    // 3D Relaxation
    for (int iter = 0; iter < 16; ++iter) {
      if (!layoutActive.load()) return {outClusters, {}};
      for (size_t i = 0; i < tagCount; ++i) {
        for (size_t j = i + 1; j < tagCount; ++j) {
          auto delta = outClusters[j].centerPos3D - outClusters[i].centerPos3D;
          float dist = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
          float r1 = std::max(70.0f, 35.0f + std::sqrt(static_cast<float>(outClusters[i].count)) * 20.0f);
          float r2 = std::max(70.0f, 35.0f + std::sqrt(static_cast<float>(outClusters[j].count)) * 20.0f);
          float minDist = r1 + r2 + 60.0f;

          if (dist < minDist && dist > 0.01f) {
            float overlap = (minDist - dist) * 0.5f;
            auto dir = delta / dist;
            outClusters[i].centerPos3D = outClusters[i].centerPos3D - dir * overlap;
            outClusters[j].centerPos3D = outClusters[j].centerPos3D + dir * overlap;
          }
        }
      }
    }

    // 2D Relaxation (Planar Repulsion)
    for (int iter = 0; iter < 24; ++iter) {
      if (!layoutActive.load()) return {outClusters, {}};
      for (size_t i = 0; i < tagCount; ++i) {
        for (size_t j = i + 1; j < tagCount; ++j) {
          float dx = outClusters[j].centerPos2D.x - outClusters[i].centerPos2D.x;
          float dy = outClusters[j].centerPos2D.y - outClusters[i].centerPos2D.y;
          float dist = std::sqrt(dx * dx + dy * dy);

          float r1 = std::max(70.0f, 35.0f + std::sqrt(static_cast<float>(outClusters[i].count)) * 20.0f);
          float r2 = std::max(70.0f, 35.0f + std::sqrt(static_cast<float>(outClusters[j].count)) * 20.0f);
          float minDist = r1 + r2 + 60.0f;

          if (dist < minDist && dist > 0.01f) {
            float overlap = (minDist - dist) * 0.5f;
            float nx = dx / dist;
            float ny = dy / dist;
            outClusters[i].centerPos2D.x -= nx * overlap;
            outClusters[i].centerPos2D.y -= ny * overlap;
            outClusters[j].centerPos2D.x += nx * overlap;
            outClusters[j].centerPos2D.y += ny * overlap;
          }
        }
      }
    }

    for (size_t i = 0; i < tagCount; ++i) {
      outClusters[i].centerPos = is2DMode ? outClusters[i].centerPos2D : outClusters[i].centerPos3D;
    }
  }

  std::unordered_map<juce::String, Vector3D> tagCenters3D;
  std::unordered_map<juce::String, Vector3D> tagCenters2D;
  std::unordered_map<juce::String, int> tagTotalCounts;
  for (const auto &cl : outClusters) {
    tagCenters3D[cl.tag] = cl.centerPos3D;
    tagCenters2D[cl.tag] = cl.centerPos2D;
    tagTotalCounts[cl.tag] = cl.count;
  }

  std::unordered_map<juce::String, int> tagItemCounts;
  for (auto &node : nodesCopy) {
    if (!layoutActive.load()) return {outClusters, {}};
    auto cPos3D = tagCenters3D[node.primaryTag];
    auto cPos2D = tagCenters2D[node.primaryTag];
    int countIdx = tagItemCounts[node.primaryTag]++;
    int totalInTag = tagTotalCounts[node.primaryTag];

    auto anc = findAnchor(node.primaryTag);
    float axisAngle = anc.angle;
    float axisStretch = (anc.stretch > 0.01f) ? anc.stretch : 1.6f;

    uint32_t nodeHash = static_cast<uint32_t>(node.item.filePath.hashCode() ^
                                                (countIdx * 2654435761u));

    // Multi-stage hash generation for high-entropy organic variation
    uint32_t h1 = nodeHash ^ 0x6a09e667;
    uint32_t h2 = (h1 * 1664525u + 1013904223u) ^ 0xbb67ae85;
    uint32_t h3 = (h2 * 1664525u + 1013904223u) ^ 0x3c6ef372;
    uint32_t h4 = (h3 * 1664525u + 1013904223u) ^ 0xa54ff53a;

    // --- 1. Long-Axis Coordinate (Continuous logarithmic audio mapping + organic dispersion, no hard walls) ---
    float uAxis = 0.0f;
    float hashVal = ((static_cast<float>(h1 % 2001) - 1000.0f) / 1000.0f);

    if (node.item.highFreqRatio > 0.0001 || node.item.zcr > 0.0001) {
      float bright = static_cast<float>(node.item.highFreqRatio * 0.45 + node.item.zcr * 2.2);
      // Smooth logarithmic brightness curve with tanh: asymptotic falloff, ZERO hard clamps, ZERO flat walls
      float logB = std::log(std::max(0.002f, bright * 4.0f));
      float audioAxis = std::tanh((logB - 0.10f) * 0.70f);

      // 55% audio brightness alignment + 45% organic sample dispersion
      // Guarantees each sample has an individual coordinate and prevents boundary clustering
      uAxis = audioAxis * 0.55f + hashVal * 0.45f;
    } else {
      uAxis = hashVal;
    }

    // --- 2. Cross-Axis Coordinate (Gaussian Box-Muller falloff around nebula spine) ---
    float u1 = std::max(0.0001f, static_cast<float>((h2 >> 4) % 10000) / 10000.0f);
    float u2 = static_cast<float>((h3 >> 4) % 10000) / 10000.0f;
    float gaussNorm = std::sqrt(-2.0f * std::log(u1)) * std::cos(2.0f * juce::MathConstants<float>::pi * u2);

    float decayFeat = 0.0f;
    if (node.item.decayRatio > 0.001) {
      decayFeat = std::tanh((static_cast<float>(node.item.decayRatio) - 0.25f) * 2.0f) * 0.35f;
    }
    float uCross = decayFeat + gaussNorm * 0.34f;

    // Dynamic cluster scale proportional to item count
    float baseRadius = std::max(60.0f, 35.0f + std::sqrt(static_cast<float>(totalInTag)) * 16.0f);

    // Edge loosening: samples near the periphery get increased organic dispersion
    // so clusters dissolve softly into space without forming dense perimeter ridges
    float rFromCore = std::sqrt(uAxis * uAxis + uCross * uCross);
    float edgeLoosen = 1.0f + std::min(rFromCore * 0.5f, 1.0f);

    float jX = (((static_cast<float>(h3 % 1001) - 500.0f) / 500.0f) * 6.0f) * edgeLoosen;
    float jY = (((static_cast<float>(h4 % 1001) - 500.0f) / 500.0f) * 6.0f) * edgeLoosen;
    float jZ = (((static_cast<float>((h1 ^ h4) % 1001) - 500.0f) / 500.0f) * 6.0f) * edgeLoosen;

    // 2D Rotation by cluster elongation angle
    float cosA = std::cos(axisAngle);
    float sinA = std::sin(axisAngle);
    float posX_rel = (uAxis * axisStretch * cosA - uCross * sinA) * baseRadius + jX;
    float posY_rel = (uAxis * axisStretch * sinA + uCross * cosA) * baseRadius + jY;

    // 2D Position
    node.pos2D = { cPos2D.x + posX_rel, cPos2D.y + posY_rel, 0.0f };

    // 3D Position: Tilted galactic disc with organic thickness
    float zThickness = ((static_cast<float>((h2 ^ h3) % 2001) - 1000.0f) / 1000.0f) * (baseRadius * 0.35f);
    float posZ_rel = uCross * baseRadius * 0.40f + zThickness + jZ;
    node.pos3D = { cPos3D.x + posX_rel, cPos3D.y + posY_rel, cPos3D.z + posZ_rel };
  }

  // --- 3. Intra-Cluster Node Repulsion Pass (Collision Avoidance + Soft Spring Anchoring) ---
  // Guarantees that similar samples never stack on top of each other
  // and maintain a clean minimum visual distance just like Atlas 2!
  std::unordered_map<juce::String, std::vector<size_t>> clusterNodeIndices;
  for (size_t i = 0; i < nodesCopy.size(); ++i) {
    clusterNodeIndices[nodesCopy[i].primaryTag].push_back(i);
  }

  std::vector<Vector3D> origPos2D(nodesCopy.size());
  std::vector<Vector3D> origPos3D(nodesCopy.size());
  for (size_t i = 0; i < nodesCopy.size(); ++i) {
    origPos2D[i] = nodesCopy[i].pos2D;
    origPos3D[i] = nodesCopy[i].pos3D;
  }

  const float minNodeDist2D = 16.0f;
  const float minNodeDistSq2D = minNodeDist2D * minNodeDist2D;
  const float minNodeDist3D = 16.0f;
  const float minNodeDistSq3D = minNodeDist3D * minNodeDist3D;

  for (auto &pair : clusterNodeIndices) {
    auto &indices = pair.second;
    size_t count = indices.size();
    if (count <= 1) continue;

    for (int iter = 0; iter < 12; ++iter) {
      if (!layoutActive.load()) return {outClusters, {}};
      for (size_t i = 0; i < count; ++i) {
        size_t idxA = indices[i];
        for (size_t j = i + 1; j < count; ++j) {
          size_t idxB = indices[j];

          // 2D Repulsion
          float dx2 = nodesCopy[idxB].pos2D.x - nodesCopy[idxA].pos2D.x;
          float dy2 = nodesCopy[idxB].pos2D.y - nodesCopy[idxA].pos2D.y;
          float distSq2 = dx2 * dx2 + dy2 * dy2;

          if (distSq2 < minNodeDistSq2D && distSq2 > 0.0001f) {
            float dist = std::sqrt(distSq2);
            float push = (minNodeDist2D - dist) * 0.40f;
            float nx = dx2 / dist;
            float ny = dy2 / dist;
            nodesCopy[idxA].pos2D.x -= nx * push;
            nodesCopy[idxA].pos2D.y -= ny * push;
            nodesCopy[idxB].pos2D.x += nx * push;
            nodesCopy[idxB].pos2D.y += ny * push;
          } else if (distSq2 <= 0.0001f) {
            float sepAngle = static_cast<float>((idxA * 37 + idxB * 73) % 628) * 0.01f;
            float push = minNodeDist2D * 0.5f;
            nodesCopy[idxA].pos2D.x -= std::cos(sepAngle) * push;
            nodesCopy[idxA].pos2D.y -= std::sin(sepAngle) * push;
            nodesCopy[idxB].pos2D.x += std::cos(sepAngle) * push;
            nodesCopy[idxB].pos2D.y += std::sin(sepAngle) * push;
          }

          // 3D Repulsion
          float dx3 = nodesCopy[idxB].pos3D.x - nodesCopy[idxA].pos3D.x;
          float dy3 = nodesCopy[idxB].pos3D.y - nodesCopy[idxA].pos3D.y;
          float dz3 = nodesCopy[idxB].pos3D.z - nodesCopy[idxA].pos3D.z;
          float distSq3 = dx3 * dx3 + dy3 * dy3 + dz3 * dz3;

          if (distSq3 < minNodeDistSq3D && distSq3 > 0.0001f) {
            float dist = std::sqrt(distSq3);
            float push = (minNodeDist3D - dist) * 0.40f;
            float nx = dx3 / dist;
            float ny = dy3 / dist;
            float nz = dz3 / dist;
            nodesCopy[idxA].pos3D.x -= nx * push;
            nodesCopy[idxA].pos3D.y -= ny * push;
            nodesCopy[idxA].pos3D.z -= nz * push;
            nodesCopy[idxB].pos3D.x += nx * push;
            nodesCopy[idxB].pos3D.y += ny * push;
            nodesCopy[idxB].pos3D.z += nz * push;
          } else if (distSq3 <= 0.0001f) {
            float sepAngle = static_cast<float>((idxA * 37 + idxB * 73) % 628) * 0.01f;
            float push = minNodeDist3D * 0.5f;
            nodesCopy[idxA].pos3D.x -= std::cos(sepAngle) * push;
            nodesCopy[idxA].pos3D.y -= std::sin(sepAngle) * push;
            nodesCopy[idxA].pos3D.z += std::cos(sepAngle) * push;
            nodesCopy[idxA].pos3D.z -= std::sin(sepAngle) * push;
            nodesCopy[idxB].pos3D.x += std::cos(sepAngle) * push;
            nodesCopy[idxB].pos3D.y += std::sin(sepAngle) * push;
          }
        }
      }

      // Soft spring anchoring: gently pulls nodes toward their organic natural seed positions
      // to prevent repulsion from packing points into straight perimeter barriers
      for (size_t idx : indices) {
        nodesCopy[idx].pos2D.x += (origPos2D[idx].x - nodesCopy[idx].pos2D.x) * 0.08f;
        nodesCopy[idx].pos2D.y += (origPos2D[idx].y - nodesCopy[idx].pos2D.y) * 0.08f;
        nodesCopy[idx].pos3D.x += (origPos3D[idx].x - nodesCopy[idx].pos3D.x) * 0.08f;
        nodesCopy[idx].pos3D.y += (origPos3D[idx].y - nodesCopy[idx].pos3D.y) * 0.08f;
        nodesCopy[idx].pos3D.z += (origPos3D[idx].z - nodesCopy[idx].pos3D.z) * 0.08f;
      }
    }
  }

  for (auto &node : nodesCopy) {
    node.targetPos = is2DMode ? node.pos2D : node.pos3D;
    node.currentPos = node.targetPos;
  }

  // Inter-cluster constellation connections (for 3D mode)
  std::vector<std::pair<size_t, size_t>> outEdges;
  for (size_t i = 0; i < tagCount; ++i) {
    std::vector<std::pair<float, size_t>> neighbors;
    for (size_t j = 0; j < tagCount; ++j) {
      if (i == j) continue;
      auto delta = outClusters[j].centerPos3D - outClusters[i].centerPos3D;
      float d = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
      neighbors.push_back({d, j});
    }
    std::sort(neighbors.begin(), neighbors.end());
    size_t connectCount = std::min(static_cast<size_t>(2), neighbors.size());
    for (size_t k = 0; k < connectCount; ++k) {
      size_t j = neighbors[k].second;
      if (i < j) {
        outEdges.push_back({i, j});
      }
    }
  }

  return {outClusters, outEdges};
}

void SampleCloudComponent::selectItemById(const juce::String &itemId) {
  if (itemId.isEmpty())
    return;
  for (size_t i = 0; i < nodes.size(); ++i) {
    if (nodes[i].item.id == itemId) {
      selectedNodeIndex = static_cast<int>(i);
      targetCameraCenterPos = nodes[i].targetPos;
      updateSimilarStack();
      overlayComponent.repaint();
      repaint();
      break;
    }
  }
}

void SampleCloudComponent::updateSimilarStack() {
  similarAbove.clear();
  similarBelow.clear();

  if (selectedNodeIndex < 0 || selectedNodeIndex >= static_cast<int>(nodes.size()))
    return;

  // 1. Maintain selection visit history to prevent A-B-A-B looping traps when clicking forward or back
  if (recentSelectionHistory.empty() || recentSelectionHistory.back() != selectedNodeIndex) {
    recentSelectionHistory.erase(
        std::remove(recentSelectionHistory.begin(), recentSelectionHistory.end(), selectedNodeIndex),
        recentSelectionHistory.end());
    recentSelectionHistory.push_back(selectedNodeIndex);
    if (recentSelectionHistory.size() > 24) {
      recentSelectionHistory.erase(recentSelectionHistory.begin());
    }
  }

  const auto &refNode = nodes[selectedNodeIndex];
  const auto &refItem = refNode.item;
  const juce::String refCat = refNode.canonicalCategory.isNotEmpty()
                                  ? refNode.canonicalCategory
                                  : getCanonicalAtlasCategory(refItem).toLowerCase();

  // 2. Determine Category Cluster Core vs. Edge Position using pre-cached category
  Vector3D catCenter { 0.0f, 0.0f, 0.0f };
  int sameCatCount = 0;
  for (const auto &n : nodes) {
    if (TagDatabaseManager::areCategoriesCompatible(n.canonicalCategory, refCat)) {
      catCenter.x += n.targetPos.x;
      catCenter.y += n.targetPos.y;
      catCenter.z += n.targetPos.z;
      sameCatCount++;
    }
  }

  float maxDistFromCenter = 1.0f;
  float refDistFromCenter = 0.0f;

  if (sameCatCount > 0) {
    catCenter.x /= static_cast<float>(sameCatCount);
    catCenter.y /= static_cast<float>(sameCatCount);
    catCenter.z /= static_cast<float>(sameCatCount);

    float dx = refNode.targetPos.x - catCenter.x;
    float dy = refNode.targetPos.y - catCenter.y;
    float dz = is2DMode ? 0.0f : (refNode.targetPos.z - catCenter.z);
    refDistFromCenter = std::sqrt(dx * dx + dy * dy + dz * dz);

    for (const auto &n : nodes) {
      if (TagDatabaseManager::areCategoriesCompatible(n.canonicalCategory, refCat)) {
        float ndx = n.targetPos.x - catCenter.x;
        float ndy = n.targetPos.y - catCenter.y;
        float ndz = is2DMode ? 0.0f : (n.targetPos.z - catCenter.z);
        float d = std::sqrt(ndx * ndx + ndy * ndy + ndz * ndz);
        maxDistFromCenter = std::max(maxDistFromCenter, d);
      }
    }
  }

  const float normalizedRadius = (maxDistFromCenter > 1.0f) ? (refDistFromCenter / maxDistFromCenter) : 0.0f;
  const bool isEdgeSample = (normalizedRadius >= 0.68f);

  struct ScoredCandidate {
    int nodeIndex;
    double acousticDist;
    double effectiveDist;
    double diff;
    bool isSameCategory;
  };

  std::vector<ScoredCandidate> aboveCandidates;
  std::vector<ScoredCandidate> belowCandidates;
  aboveCandidates.reserve(128);
  belowCandidates.reserve(128);

  const float maxNeighborDistSq = (maxDistFromCenter * 1.6f) * (maxDistFromCenter * 1.6f);

  for (size_t i = 0; i < nodes.size(); ++i) {
    if (static_cast<int>(i) == selectedNodeIndex)
      continue;

    const auto &candNode = nodes[i];
    const bool isSameCat = TagDatabaseManager::areCategoriesCompatible(candNode.canonicalCategory, refCat);

    if (!isSameCat) {
      if (!isEdgeSample && sameCatCount >= 6) {
        continue;
      }

      float spDx = candNode.targetPos.x - refNode.targetPos.x;
      float spDy = candNode.targetPos.y - refNode.targetPos.y;
      float spDz = is2DMode ? 0.0f : (candNode.targetPos.z - refNode.targetPos.z);
      float spatialDistSq = spDx * spDx + spDy * spDy + spDz * spDz;

      if (sameCatCount >= 6 && spatialDistSq > maxNeighborDistSq) {
        continue;
      }
    }

    const auto &candItem = candNode.item;
    double acousticDist = TagDatabaseManager::calculateAcousticDistance(refItem, candItem);

    double recencyPenalty = 0.0;
    if (recentSelectionHistory.size() > 1) {
      int histLen = static_cast<int>(recentSelectionHistory.size());
      for (int h = histLen - 2; h >= 0; --h) {
        if (recentSelectionHistory[h] == static_cast<int>(i)) {
          int stepsAgo = (histLen - 1) - h;
          if (stepsAgo == 1)       recencyPenalty = 1.25;
          else if (stepsAgo == 2)  recencyPenalty = 0.85;
          else if (stepsAgo == 3)  recencyPenalty = 0.55;
          else if (stepsAgo == 4)  recencyPenalty = 0.35;
          else if (stepsAgo <= 8)  recencyPenalty = 0.20;
          else                     recencyPenalty = 0.10;
          break;
        }
      }
    }

    double effectiveDist = acousticDist + recencyPenalty + (isSameCat ? 0.0 : 0.35);

    double spectralDiff = (candItem.highFreqRatio - refItem.highFreqRatio) * 2.4
                        + (candItem.zcr - refItem.zcr) * 1.8
                        + (candItem.decayRatio - refItem.decayRatio) * 1.0;

    float dy = candNode.targetPos.y - refNode.targetPos.y;
    float dx = candNode.targetPos.x - refNode.targetPos.x;
    float spatialProgression = (maxDistFromCenter > 1.0f) ? ((dy * 0.75f + dx * 0.25f) / maxDistFromCenter) : 0.0f;

    double totalDiff = spectralDiff + spatialProgression * 0.35;
    if (std::abs(totalDiff) < 0.005) {
      totalDiff = (dy >= 0.0f) ? 0.005 : -0.005;
    }

    if (totalDiff >= 0.0) {
      aboveCandidates.push_back({ static_cast<int>(i), acousticDist, effectiveDist, totalDiff, isSameCat });
    } else {
      belowCandidates.push_back({ static_cast<int>(i), acousticDist, effectiveDist, -totalDiff, isSameCat });
    }
  }

  auto selectGraduatedStack = [this](std::vector<ScoredCandidate>& sortedPool) -> std::vector<ScoredCandidate> {
    std::vector<ScoredCandidate> selected;
    if (sortedPool.empty()) return selected;

    std::sort(sortedPool.begin(), sortedPool.end(),
              [](const ScoredCandidate& a, const ScoredCandidate& b) {
                return a.effectiveDist < b.effectiveDist;
              });

    const size_t scanLimit = std::min<size_t>(sortedPool.size(), 64);

    int idx0 = -1;
    double bestScore0 = 1e9;
    for (size_t i = 0; i < scanLimit; ++i) {
      double score = sortedPool[i].effectiveDist + std::abs(sortedPool[i].diff - 0.035) * 0.40;
      if (score < bestScore0) {
        bestScore0 = score;
        idx0 = static_cast<int>(i);
      }
    }
    if (idx0 < 0) idx0 = 0;
    selected.push_back(sortedPool[idx0]);

    if (sortedPool.size() == 1) return selected;

    int idx1 = -1;
    double bestScore1 = 1e9;
    const auto &item0 = nodes[selected[0].nodeIndex].item;
    for (size_t i = 0; i < scanLimit; ++i) {
      if (sortedPool[i].nodeIndex == selected[0].nodeIndex) continue;

      double sepDist = TagDatabaseManager::calculateAcousticDistance(item0, nodes[sortedPool[i].nodeIndex].item);
      double stepAdv = sortedPool[i].diff - selected[0].diff;

      double penalty = 0.0;
      if (stepAdv < 0.015) penalty += 0.50;
      if (sepDist < 0.035) penalty += 0.60;

      double score = sortedPool[i].effectiveDist + std::abs(sortedPool[i].diff - 0.12) * 0.70 + penalty;
      if (score < bestScore1) {
        bestScore1 = score;
        idx1 = static_cast<int>(i);
      }
    }
    if (idx1 >= 0) {
      selected.push_back(sortedPool[idx1]);
    } else {
      for (size_t i = 0; i < sortedPool.size(); ++i) {
        if (sortedPool[i].nodeIndex != selected[0].nodeIndex) {
          selected.push_back(sortedPool[i]);
          break;
        }
      }
    }

    if (sortedPool.size() == 2 || selected.size() < 2) return selected;

    int idx2 = -1;
    double bestScore2 = 1e9;
    const auto &item1 = nodes[selected[1].nodeIndex].item;
    for (size_t i = 0; i < scanLimit; ++i) {
      if (sortedPool[i].nodeIndex == selected[0].nodeIndex ||
          sortedPool[i].nodeIndex == selected[1].nodeIndex) continue;

      const auto &candItem = nodes[sortedPool[i].nodeIndex].item;
      double sepDist0 = TagDatabaseManager::calculateAcousticDistance(item0, candItem);
      double sepDist1 = TagDatabaseManager::calculateAcousticDistance(item1, candItem);
      double stepAdv = sortedPool[i].diff - selected[1].diff;

      double penalty = 0.0;
      if (stepAdv < 0.02)   penalty += 0.55;
      if (sepDist0 < 0.045) penalty += 0.50;
      if (sepDist1 < 0.035) penalty += 0.50;

      double score = sortedPool[i].effectiveDist + std::abs(sortedPool[i].diff - 0.24) * 0.60 + penalty;
      if (score < bestScore2) {
        bestScore2 = score;
        idx2 = static_cast<int>(i);
      }
    }
    if (idx2 >= 0) {
      selected.push_back(sortedPool[idx2]);
    } else {
      for (size_t i = 0; i < sortedPool.size(); ++i) {
        if (sortedPool[i].nodeIndex != selected[0].nodeIndex &&
            sortedPool[i].nodeIndex != selected[1].nodeIndex) {
          selected.push_back(sortedPool[i]);
          break;
        }
      }
    }

    return selected;
  };

  auto selectedAbove = selectGraduatedStack(aboveCandidates);
  auto selectedBelow = selectGraduatedStack(belowCandidates);

  auto isAlreadyChosen = [&](int nIdx) {
    for (const auto& s : selectedAbove) if (s.nodeIndex == nIdx) return true;
    for (const auto& s : selectedBelow) if (s.nodeIndex == nIdx) return true;
    return false;
  };

  if (selectedAbove.size() < 3) {
    for (const auto& c : belowCandidates) {
      if (!isAlreadyChosen(c.nodeIndex)) {
        selectedAbove.push_back(c);
        if (selectedAbove.size() >= 3) break;
      }
    }
  }
  if (selectedBelow.size() < 3) {
    for (const auto& c : aboveCandidates) {
      if (!isAlreadyChosen(c.nodeIndex)) {
        selectedBelow.push_back(c);
        if (selectedBelow.size() >= 3) break;
      }
    }
  }

  for (const auto& s : selectedAbove) {
    similarAbove.push_back({ s.nodeIndex, nodes[s.nodeIndex].item, s.acousticDist, true, {} });
  }
  for (const auto& s : selectedBelow) {
    similarBelow.push_back({ s.nodeIndex, nodes[s.nodeIndex].item, s.acousticDist, false, {} });
  }
}

void SampleCloudComponent::drawMiniWaveform(juce::Graphics &g, const juce::Rectangle<float> &bounds,
                                           const MediaItem &item, juce::Colour waveColour) {
  if (bounds.getWidth() < 4.0f || bounds.getHeight() < 4.0f)
    return;

  const float midY = bounds.getCentreY();
  const float halfH = (bounds.getHeight() * 0.5f) - 0.8f;
  const float w = bounds.getWidth();
  const float x0 = bounds.getX();

  // 1. Stable, unique acoustic seed per item
  uint32_t seed = 2166136261u;
  for (int i = 0; i < item.fileName.length(); ++i) {
    seed = (seed ^ static_cast<uint32_t>(item.fileName[i])) * 16777619u;
  }
  for (int i = 0; i < item.filePath.length(); ++i) {
    seed = (seed ^ static_cast<uint32_t>(item.filePath[i])) * 16777619u;
  }

  // 2. Extract acoustic profile features
  float decay = juce::jlimit(0.10f, 0.95f, static_cast<float>(item.decayRatio > 0.0 ? item.decayRatio : 0.35));
  float crest = juce::jlimit(1.1f, 8.0f, static_cast<float>(item.crestFactor > 0.0 ? item.crestFactor : 2.5));
  float hfr   = juce::jlimit(0.08f, 0.92f, static_cast<float>(item.highFreqRatio > 0.0 ? item.highFreqRatio : 0.35));
  float zcr   = juce::jlimit(0.04f, 0.90f, static_cast<float>(item.zcr > 0.0 ? item.zcr : 0.25));

  juce::String cat = getCanonicalAtlasCategory(item).toLowerCase();

  // 3. Tail Compression:
  // Instead of letting the waveform die in the first 15% and leaving a long empty line,
  // compress the time window so the active sound body spans 70% to 90% of the capsule width!
  float bodySpan = juce::jlimit(0.68f, 0.92f, 0.65f + decay * 0.27f);
  if (cat.contains("loop") || cat.contains("synth") || cat.contains("pad") ||
      cat.contains("keys") || cat.contains("vocal") || cat.contains("bass")) {
    bodySpan = 0.92f;
  } else if (cat.contains("hihat closed") || cat.contains("snap")) {
    bodySpan = 0.62f;
  }

  int numPoints = juce::roundToInt(w);
  if (numPoints < 16) numPoints = 16;
  const float step = w / static_cast<float>(numPoints);

  g.setColour(waveColour);

  for (int p = 0; p < numPoints; ++p) {
    float u = static_cast<float>(p) / static_cast<float>(numPoints); // 0.0 .. 1.0 across capsule width

    // Time-compressed normalized coordinate within the active body
    float t = u / bodySpan;

    float amp = 0.0f;
    if (t <= 1.0f) {
      // Attack envelope
      float attack = std::min(1.0f, t / 0.045f);

      // Tail compression: gentle decay that tapers nicely toward the end of bodySpan
      // rather than dying immediately into a flat line
      float decayExponent = 1.4f + (1.0f - decay) * 2.0f;
      float env = attack * std::exp(-t * decayExponent);

      // Smooth fade-out at the end of bodySpan
      float fade = 1.0f;
      if (t > 0.65f) {
        fade = 0.5f * (1.0f + std::cos((t - 0.65f) / 0.35f * 3.14159f));
      }
      env *= fade;

      // Category-specific timbral waveform shaping
      float osc = 0.0f;
      float seedPhase = static_cast<float>(seed % 100) * 0.0628f;

      if (cat.contains("kick") || cat.contains("subkick")) {
        // Punchy low-frequency downward pitch sweep
        float freq = 20.0f + (1.0f - t) * 35.0f;
        osc = std::sin(t * freq + seedPhase) * 0.70f + std::sin(t * freq * 0.5f) * 0.30f;
      } else if (cat.contains("snare")) {
        // Snap transient + dense wire crackle
        float noise = (((seed * (static_cast<uint32_t>(p) + 31u) * 48271u) % 1000u) / 1000.0f - 0.5f) * 2.0f;
        float snap = (t < 0.12f) ? std::sin(t * 85.0f) * 0.8f : 0.0f;
        osc = snap + noise * (0.45f + hfr * 0.40f) + std::sin(t * 32.0f + seedPhase) * 0.30f;
      } else if (cat.contains("clap")) {
        // Characteristic multi-clap pre-transients
        float preBurst = 0.0f;
        if (t < 0.08f) preBurst = std::sin(t * 120.0f) * 0.9f;
        else if (t < 0.16f) preBurst = std::sin((t - 0.08f) * 110.0f) * 0.75f;
        else if (t < 0.24f) preBurst = std::sin((t - 0.16f) * 95.0f) * 0.60f;
        float noise = (((seed * (static_cast<uint32_t>(p) + 47u) * 48271u) % 1000u) / 1000.0f - 0.5f) * 2.0f;
        osc = preBurst + noise * 0.60f;
      } else if (cat.contains("hihat") || cat.contains("cymbal") || cat.contains("crash") || cat.contains("ride")) {
        // High-frequency metallic shimmer and dense flutter
        float f1 = 60.0f + hfr * 55.0f;
        float f2 = 110.0f + zcr * 60.0f;
        float noise = (((seed * (static_cast<uint32_t>(p) + 19u) * 48271u) % 1000u) / 1000.0f - 0.5f) * 1.5f;
        osc = (std::sin(t * f1 + seedPhase) * 0.45f + std::sin(t * f2 * 1.33f) * 0.35f + noise * 0.40f);
      } else if (cat.contains("loop")) {
        // 4 rhythmic beat pulses across the capsule
        float beatPulse = std::pow(std::abs(std::sin(t * 3.14159f * 4.0f)), 2.5f);
        env = (0.25f + 0.75f * beatPulse) * (1.0f - t * 0.20f) * fade;
        osc = std::sin(t * 40.0f + seedPhase) * 0.6f + std::sin(t * 80.0f) * 0.4f;
      } else if (cat.contains("bass")) {
        // Fat, oscillating low-frequency wave cycles
        float f = 16.0f + (1.0f - crest * 0.12f) * 12.0f;
        osc = std::sin(t * f + seedPhase) * 0.75f + std::sin(t * f * 2.0f + seedPhase) * 0.25f;
      } else {
        // Melodic / Synth / Vocal / Acoustic instruments
        float f1 = 20.0f + hfr * 35.0f;
        float f2 = f1 * 1.5f;
        float vibrato = std::sin(t * 12.0f + seedPhase) * 0.15f;
        osc = std::sin(t * f1 + seedPhase) * 0.55f + std::sin(t * f2) * 0.35f + vibrato;
      }

      // Dynamic amplitude compression:
      // Brings up the audible body so differences stand out prominently
      float rawAmp = std::abs(env * (0.65f + osc * 0.35f));
      amp = std::pow(rawAmp, 0.70f); // Power-curve compression (amplifies body details)
      amp = juce::jlimit(0.04f, 1.0f, amp);
    } else {
      // Smooth clean taper after bodySpan: gently fades to zero
      float afterTail = (u - bodySpan) / (1.0f - bodySpan);
      amp = 0.04f * std::max(0.0f, 1.0f - afterTail * 2.0f);
    }

    float h = amp * halfH;
    float px = x0 + static_cast<float>(p) * step;

    // Draw crisp antialiased vertical waveform bar
    g.drawLine(px, midY - h, px, midY + h + 0.5f, 1.2f);
  }
}

void SampleCloudComponent::updateMapBounds() {
  float maxX = 600.0f, maxY = 500.0f, maxZ = 400.0f;
  for (const auto &n : nodes) {
    maxX = std::max(maxX, std::abs(n.targetPos.x));
    maxY = std::max(maxY, std::abs(n.targetPos.y));
    maxZ = std::max(maxZ, std::abs(n.targetPos.z));
  }
  for (const auto &c : clusters) {
    maxX = std::max(maxX, std::abs(c.centerPos2D.x));
    maxY = std::max(maxY, std::abs(c.centerPos2D.y));
    maxX = std::max(maxX, std::abs(c.centerPos3D.x));
    maxY = std::max(maxY, std::abs(c.centerPos3D.y));
    maxZ = std::max(maxZ, std::abs(c.centerPos3D.z));
  }
  maxMapPanX = maxX + 280.0f;
  maxMapPanY = maxY + 240.0f;
  maxMapPanZ = maxZ + 240.0f;
}

juce::Rectangle<float> SampleCloudComponent::getDockDeadzone() const {
  float effW = getEffectiveWidth();
  if (selectedNodeIndex < 0 || selectedNodeIndex >= static_cast<int>(nodes.size())) {
    return juce::Rectangle<float>(effW - 270.0f, 0.0f, 270.0f, 65.0f);
  }
  // Must encompass the Up/Down navigation buttons positioned at (dockLeft - 22 = effW - 291) across their full width!
  float deadzoneLeft = effW - 304.0f;
  float deadzoneWidth = 304.0f;
  float deadzoneHeight = showSimilarStack ? 270.0f : 96.0f;
  return juce::Rectangle<float>(deadzoneLeft, 0.0f, deadzoneWidth, deadzoneHeight);
}

float SampleCloudComponent::smoothDamp(float current, float target, float& currentVelocity, float smoothTime, float dt) {
  smoothTime = std::max(0.0001f, smoothTime);
  float omega = 2.0f / smoothTime;
  float x = omega * dt;
  float exp = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);
  float change = current - target;
  float originalTo = target;

  float temp = (currentVelocity + omega * change) * dt;
  currentVelocity = (currentVelocity - omega * temp) * exp;
  float output = target + (change + temp) * exp;

  if ((originalTo - current > 0.0f) == (output > originalTo)) {
    output = originalTo;
    currentVelocity = (output - originalTo) / dt;
  }
  return output;
}

void SampleCloudComponent::flyToCluster(int clusterIdx) {
  if (clusterIdx < 0 || clusterIdx >= static_cast<int>(clusters.size()))
    return;
  targetCameraCenterPos = is2DMode ? clusters[clusterIdx].centerPos2D : clusters[clusterIdx].centerPos3D;
  if (is2DMode) targetCameraCenterPos.z = 0.0f;
  targetZoomScale = juce::jlimit(0.20f, 3.5f, 1.40f);
  selectedNodeIndex = -1;
  overlayComponent.repaint();
}

float SampleCloudComponent::getEffectiveWidth() const {
#if JUCE_MAC
  if (overlayComponent.isOnDesktop())
    return static_cast<float>(overlayComponent.getWidth());
#endif
  return static_cast<float>(getWidth());
}

float SampleCloudComponent::getEffectiveHeight() const {
#if JUCE_MAC
  if (overlayComponent.isOnDesktop())
    return static_cast<float>(overlayComponent.getHeight());
#endif
  return static_cast<float>(getHeight());
}

void SampleCloudComponent::paint(juce::Graphics &g) {
  juce::ignoreUnused(g);
}

void SampleCloudComponent::paintOverlay(juce::Graphics &g) {
  if (layoutPending) {
    g.setColour(juce::Colours::white);
    g.setFont(18.0f);
    g.drawText("Layout Computing...", getLocalBounds(),
               juce::Justification::centred);
    return;
  }

  const float effW = getEffectiveWidth();
  const float effH = getEffectiveHeight();
  const float halfW = effW * 0.5f;
  const float halfH = effH * 0.5f;

#if JUCE_MAC
  const float renderScale = 1.0f;
#else
  float renderScale = 1.0f;
  if (auto *peer = getPeer())
    renderScale = std::max(1.0f, static_cast<float>(peer->getPlatformScaleFactor()));
#endif

  // 1. Selected Node Starlight Beacon
  if (selectedNodeIndex >= 0 && selectedNodeIndex < (int)nodes.size()) {
    if (lastViewProjectionMatrix.mat[15] != 0.0f) {
      auto &n = nodes[selectedNodeIndex];
      juce::Vector3D<float> p(n.currentPos.x, n.currentPos.y, is2DMode ? 0.0f : n.currentPos.z);
      float w = lastViewProjectionMatrix.mat[3] * p.x +
                lastViewProjectionMatrix.mat[7] * p.y +
                lastViewProjectionMatrix.mat[11] * p.z +
                lastViewProjectionMatrix.mat[15];
      if (w > 0.001f) {
        float spX = lastViewProjectionMatrix.mat[0] * p.x +
                    lastViewProjectionMatrix.mat[4] * p.y +
                    lastViewProjectionMatrix.mat[8] * p.z +
                    lastViewProjectionMatrix.mat[12];
        float spY = lastViewProjectionMatrix.mat[1] * p.x +
                    lastViewProjectionMatrix.mat[5] * p.y +
                    lastViewProjectionMatrix.mat[9] * p.z +
                    lastViewProjectionMatrix.mat[13];
        float spZ = lastViewProjectionMatrix.mat[2] * p.x +
                    lastViewProjectionMatrix.mat[6] * p.y +
                    lastViewProjectionMatrix.mat[10] * p.z +
                    lastViewProjectionMatrix.mat[14];
        if (spZ > 0.0f) {
          float sx = halfW + (spX / w) * halfW;
          float sy = halfH - (spY / w) * halfH;
          float effRad = is2DMode ? (n.radius * 1.15f) : n.radius;
          float pointDiam = juce::jlimit(6.0f, 22.0f, effRad * 1600.0f / std::max(60.0f, w));
          float r = juce::jlimit(3.2f, 8.5f, (pointDiam * 0.5f) / renderScale);

          g.setColour(n.colour.withAlpha(0.85f));
          g.drawEllipse(sx - r - 3.0f, sy - r - 3.0f, (r + 3.0f) * 2.0f,
                        (r + 3.0f) * 2.0f, 2.0f);
          g.setColour(n.colour.withAlpha(0.35f));
          g.drawEllipse(sx - r - 7.0f, sy - r - 7.0f, (r + 7.0f) * 2.0f,
                        (r + 7.0f) * 2.0f, 1.0f);
        }
      }
    }
  }

  // 2. Smooth Hover Growth Animation, Neon Glow Aura & Floating HUD Card
  int activeHoverIdx = (hoveredNodeIndex >= 0) ? hoveredNodeIndex : animatedHoverNodeIndex;
  if (activeHoverIdx >= 0 && activeHoverIdx < (int)nodes.size()) {
    if (lastViewProjectionMatrix.mat[15] != 0.0f) {
      auto &n = nodes[activeHoverIdx];
      juce::Vector3D<float> p(n.currentPos.x, n.currentPos.y, is2DMode ? 0.0f : n.currentPos.z);
      float w = lastViewProjectionMatrix.mat[3] * p.x +
                lastViewProjectionMatrix.mat[7] * p.y +
                lastViewProjectionMatrix.mat[11] * p.z +
                lastViewProjectionMatrix.mat[15];
      if (w > 0.001f) {
        float spX = lastViewProjectionMatrix.mat[0] * p.x +
                    lastViewProjectionMatrix.mat[4] * p.y +
                    lastViewProjectionMatrix.mat[8] * p.z +
                    lastViewProjectionMatrix.mat[12];
        float spY = lastViewProjectionMatrix.mat[1] * p.x +
                    lastViewProjectionMatrix.mat[5] * p.y +
                    lastViewProjectionMatrix.mat[9] * p.z +
                    lastViewProjectionMatrix.mat[13];
        float spZ = lastViewProjectionMatrix.mat[2] * p.x +
                    lastViewProjectionMatrix.mat[6] * p.y +
                    lastViewProjectionMatrix.mat[10] * p.z +
                    lastViewProjectionMatrix.mat[14];
        if (spZ > 0.0f) {
          float sx = halfW + (spX / w) * halfW;
          float sy = halfH - (spY / w) * halfH;
          float effRad = is2DMode ? (n.radius * 1.15f) : n.radius;
          float pointDiam = juce::jlimit(6.0f, 22.0f, effRad * 1600.0f / std::max(60.0f, w));
          float baseR = juce::jlimit(4.0f, 9.0f, ((pointDiam * 0.5f) / renderScale) * 1.12f);
          float animR = baseR * currentHoverScale;

          // A. Subtle, Semi-Transparent Neon Hover Glow & Disc (slightly larger for clearer proximity feedback)
          if (currentHoverScale > 1.02f) {
            float progress = juce::jlimit(0.0f, 1.0f, (currentHoverScale - 1.0f) / 0.62f);

            // Soft atmospheric halo (semi-transparent)
            g.setColour(n.colour.withAlpha(0.20f * progress));
            g.fillEllipse(sx - animR * 1.35f, sy - animR * 1.35f, animR * 2.70f, animR * 2.70f);

            // Refined semi-transparent sample star disc
            g.setColour(n.colour.withAlpha(0.58f + 0.15f * progress));
            g.fillEllipse(sx - animR, sy - animR, animR * 2.0f, animR * 2.0f);

            // Subtle luminous ring highlight around the star
            g.setColour(juce::Colours::white.withAlpha(0.44f * progress));
            g.drawEllipse(sx - animR, sy - animR, animR * 2.0f, animR * 2.0f, 1.0f);

            // Gentle center glint
            float coreR = animR * 0.35f;
            g.setColour(juce::Colours::white.withAlpha(0.62f * progress));
            g.fillEllipse(sx - coreR, sy - coreR, coreR * 2.0f, coreR * 2.0f);
          }

          // B. Subtle radar ring
          float radarR = animR + 2.5f + pulseRadar * 6.8f;
          float radarAlpha = (1.0f - pulseRadar) * 0.32f;
          g.setColour(n.colour.withAlpha(radarAlpha));
          g.drawEllipse(sx - radarR, sy - radarR, radarR * 2.0f, radarR * 2.0f, 1.0f);

          // C. Delicate targeting brackets
          float bDist = animR + 3.6f;
          float bArm = 4.0f;
          g.setColour(juce::Colours::white.withAlpha(0.45f));
          g.saveState();
          g.addTransform(juce::AffineTransform::rotation(reticleAngle, sx, sy));
          // Top-Left bracket
          g.drawLine(sx - bDist, sy - bDist, sx - bDist + bArm, sy - bDist, 1.0f);
          g.drawLine(sx - bDist, sy - bDist, sx - bDist, sy - bDist + bArm, 1.0f);
          // Top-Right bracket
          g.drawLine(sx + bDist, sy - bDist, sx + bDist - bArm, sy - bDist, 1.0f);
          g.drawLine(sx + bDist, sy - bDist, sx + bDist, sy - bDist + bArm, 1.0f);
          // Bottom-Left bracket
          g.drawLine(sx - bDist, sy + bDist, sx - bDist + bArm, sy + bDist, 1.0f);
          g.drawLine(sx - bDist, sy + bDist, sx - bDist, sy + bDist - bArm, 1.0f);
          // Bottom-Right bracket
          g.drawLine(sx + bDist, sy + bDist, sx + bDist - bArm, sy + bDist, 1.0f);
          g.drawLine(sx + bDist, sy + bDist, sx + bDist, sy + bDist - bArm, 1.0f);
          g.restoreState();

          // D. Floating Dark Anthracite HUD Card
          float cardW = 280.0f;
          float cardH = 54.0f;
          float cardX = sx + animR + 20.0f;
          float cardY = sy - cardH * 0.5f;

          if (cardX + cardW > effW - 15.0f) {
            cardX = sx - animR - 20.0f - cardW;
          }
          if (cardY < 15.0f) cardY = 15.0f;
          if (cardX < 148.0f && cardY < 54.0f) cardY = 54.0f;
          if (cardY + cardH > effH - 50.0f) cardY = effH - 50.0f - cardH;

          // Holographic connecting neon laser line
          float attachX = (cardX > sx) ? cardX : (cardX + cardW);
          float attachY = cardY + cardH * 0.5f;
          g.setColour(n.colour.withAlpha(0.70f));
          g.drawLine(sx + (cardX > sx ? animR + 8.0f : -animR - 8.0f), sy, attachX, attachY, 1.2f);
          g.fillEllipse(attachX - 3.0f, attachY - 3.0f, 6.0f, 6.0f);

          // Dark Anthracite Card Background
          juce::Rectangle<float> cardBounds(cardX, cardY, cardW, cardH);
          g.setColour(juce::Colour::fromRGB(22, 23, 26).withAlpha(0.96f));
          g.fillRoundedRectangle(cardBounds, 7.0f);
          g.setColour(n.colour.withAlpha(0.60f));
          g.drawRoundedRectangle(cardBounds, 7.0f, 1.2f);

          // Card Header: Audio tag + File Name
          g.setFont(juce::Font(13.5f, juce::Font::bold));
          g.setColour(n.colour);
          g.drawText("AUDIO", cardX + 12.0f, cardY + 7.0f, 44.0f, 18.0f, juce::Justification::centredLeft);
          g.setColour(OpenWavLookAndFeel::textPrimary);
          g.drawText(n.item.fileName, cardX + 60.0f, cardY + 7.0f, cardW - 72.0f, 18.0f,
                     juce::Justification::centredLeft, true);

          // Badges Row
          float badgeY = cardY + 28.0f;
          juce::Font metaFont(11.0f, juce::Font::plain);
          g.setFont(metaFont);

          // Primary Tag Badge with Neon Glow Accent
          juce::String tagText = n.primaryTag.startsWith("#") ? n.primaryTag.substring(1) : n.primaryTag;
          float tagW = metaFont.getStringWidthFloat(tagText) + 12.0f;
          juce::Rectangle<float> tagBounds(cardX + 12.0f, badgeY, tagW, 18.0f);
          g.setColour(n.colour.withAlpha(0.25f));
          g.fillRoundedRectangle(tagBounds, 4.0f);
          g.setColour(n.colour);
          g.drawRoundedRectangle(tagBounds, 4.0f, 1.0f);
          g.setColour(juce::Colours::white);
          g.drawText(tagText, tagBounds, juce::Justification::centred);

          // Format / Duration Details
          juce::String metaStr = n.item.cachedFormattedDuration + "  •  " + n.item.cachedFormattedSampleRate;
          g.setColour(OpenWavLookAndFeel::textSecondary);
          g.drawText(metaStr, cardX + 16.0f + tagW, badgeY, cardW - tagW - 28.0f, 18.0f,
                     juce::Justification::centredLeft);
        }
      }
    }
  }

  // 2b. Atlas 2 Style: Floating In-Constellation Category Labels (for both 2D and 3D modes)
  if (!clusters.empty() && lastViewProjectionMatrix.mat[15] != 0.0f) {
    for (const auto &cl : clusters) {
      float fontSize = juce::jlimit(13.5f, 19.0f, 13.0f + std::sqrt(static_cast<float>(cl.count)) * 0.40f);
      juce::Font clusterFont(fontSize, juce::Font::bold);
      g.setFont(clusterFont);

      const auto &center = is2DMode ? cl.centerPos2D : cl.centerPos3D;
      juce::Vector3D<float> p(center.x, center.y, center.z);
      float w = lastViewProjectionMatrix.mat[3] * p.x +
                lastViewProjectionMatrix.mat[7] * p.y +
                lastViewProjectionMatrix.mat[11] * p.z +
                lastViewProjectionMatrix.mat[15];
      if (w > 0.001f) {
        float spX = lastViewProjectionMatrix.mat[0] * p.x +
                    lastViewProjectionMatrix.mat[4] * p.y +
                    lastViewProjectionMatrix.mat[8] * p.z +
                    lastViewProjectionMatrix.mat[12];
        float spY = lastViewProjectionMatrix.mat[1] * p.x +
                    lastViewProjectionMatrix.mat[5] * p.y +
                    lastViewProjectionMatrix.mat[9] * p.z +
                    lastViewProjectionMatrix.mat[13];
        float spZ = lastViewProjectionMatrix.mat[2] * p.x +
                    lastViewProjectionMatrix.mat[6] * p.y +
                    lastViewProjectionMatrix.mat[10] * p.z +
                    lastViewProjectionMatrix.mat[14];
        if (spZ > 0.0f) {
          float invW = 1.0f / w;
          float sx = halfW + (spX * invW) * halfW;
          float sy = halfH - (spY * invW) * halfH;

          juce::String labelText = getAtlasCategoryName(cl.tag);
          if (labelText == "other" && cl.tag.isNotEmpty() && !cl.tag.equalsIgnoreCase("#other")) {
            labelText = cl.tag.startsWith("#") ? cl.tag.substring(1) : cl.tag;
          }
          float textW = clusterFont.getStringWidthFloat(labelText) + 24.0f;
          float textH = fontSize + 10.0f;
          juce::Rectangle<float> labelBounds(sx - textW * 0.5f, sy - textH * 0.5f, textW, textH);

          if (labelBounds.getRight() > 0 && labelBounds.getX() < effW &&
              labelBounds.getBottom() > 0 && labelBounds.getY() < effH - 60.0f) {
            // Subtle dark drop-shadow for high contrast against stars
            g.setColour(juce::Colour::fromRGB(12, 13, 15).withAlpha(0.85f * revealAlpha));
            g.drawText(labelText, labelBounds.translated(0.0f, 1.5f), juce::Justification::centred);

            // Clean Atlas 2 bold white title
            g.setColour(juce::Colours::white.withAlpha(0.95f * revealAlpha));
            g.drawText(labelText, labelBounds, juce::Justification::centred);
          }
        }
      }
    }
  }

  // 3. Bottom Category Legend Dock (Horizontally and Vertically aligned with Control Buttons)
  if (!clusters.empty()) {
    juce::Font legendFont(12.0f, juce::Font::bold);
    g.setFont(legendFont);

    float itemH = 32.0f;
    float itemY = effH - 51.0f;
    float x = 20.0f + legendScrollOffset;

    g.saveState();
    g.reduceClipRegion(15, juce::roundToInt(effH - 55.0f), juce::roundToInt(effW - 280.0f), 40);

    for (const auto &cluster : clusters) {
      juce::String fullTag = getAtlasCategoryName(cluster.tag);
      float textW = legendFont.getStringWidthFloat(fullTag);
      float itemW = 24.0f + textW + 14.0f;

      juce::Rectangle<float> itemBounds(x, itemY, itemW, itemH);

      // Clean dark anthracite pill background & border
      g.setColour(juce::Colour::fromRGB(24, 25, 28).withAlpha(0.88f * revealAlpha));
      g.fillRoundedRectangle(itemBounds, 6.0f);
      g.setColour(cluster.colour.withAlpha(0.45f * revealAlpha));
      g.drawRoundedRectangle(itemBounds, 6.0f, 1.0f);

      // Neon color indicator dot
      g.setColour(cluster.colour.withAlpha(0.95f * revealAlpha));
      g.fillEllipse(x + 10.0f, itemY + itemH * 0.5f - 3.5f, 7.0f, 7.0f);

      // Category label text
      g.setColour(juce::Colours::white.withAlpha(0.92f * revealAlpha));
      g.drawText(fullTag, x + 22.0f, itemY, textW + 8.0f, itemH, juce::Justification::centredLeft);

      x += itemW + 8.0f;
    }

    g.restoreState();
  }

  // 4. Quick-Audition & Similar Samples Inspector Dock (Modern Precision Angular Design)
  if (selectedNodeIndex >= 0 && selectedNodeIndex < static_cast<int>(nodes.size())) {
    const float dockRight = effW - 25.0f;
    const float dockW = 244.0f;
    const float dockLeft = dockRight - dockW;
    const float mainCardH = 38.0f;
    const float slotCardH = 26.0f;
    const float cardRadius = 3.5f;
    const float slotRadius = 3.0f;
    const auto &selNode = nodes[selectedNodeIndex];
    const auto &selItem = selNode.item;

    // Neutral Slate & Luminous Platinum Palette (No Cosmos teal/cyan)
    const juce::Colour bgCardTop        = juce::Colour::fromRGB(32, 35, 42); // Sleek dark slate
    const juce::Colour bgCardBottom     = juce::Colour::fromRGB(22, 24, 29); // Subtle gradient bottom
    const juce::Colour bgSlot           = juce::Colour::fromRGB(24, 26, 32); // Inactive slot card
    const juce::Colour bgSlotHover      = juce::Colour::fromRGB(38, 42, 52); // Hovered slot card

    const juce::Colour borderHairline   = juce::Colour::fromRGB(54, 58, 68); // Muted titanium border
    const juce::Colour borderActiveCard = juce::Colour::fromRGB(80, 88, 102); // Resting card border

    // Neutral Highlight Accent: Luminous Platinum & Ice Silver
    const juce::Colour neutralHighlight  = juce::Colour::fromRGB(224, 230, 240); // Platinum highlight on hover
    const juce::Colour neutralHoverWhite = juce::Colour::fromRGB(255, 255, 255);
    const juce::Colour neutralMutedText  = juce::Colour::fromRGB(140, 148, 162);
    const juce::Colour waveformColor     = juce::Colour::fromRGB(218, 226, 236);

    if (!showSimilarStack) {
      // ----------------- COLLAPSED MODE -----------------
      float centerX = dockLeft + dockW * 0.5f;
      float topChevronY = 10.0f;
      float nameY = 28.0f;
      float mainY = 46.0f;
      overallDockBounds = getDockDeadzone();

      // Expand Chevron placed above card (pointing down ∨)
      expandChevronBounds = juce::Rectangle<float>(centerX - 24.0f, topChevronY - 3.0f, 48.0f, 18.0f);
      juce::Path ch;
      ch.startNewSubPath(centerX - 6.0f, topChevronY + 3.0f);
      ch.lineTo(centerX, topChevronY + 9.0f);
      ch.lineTo(centerX + 6.0f, topChevronY + 3.0f);
      g.setColour(isHoveringExpandChevron ? neutralHoverWhite : borderActiveCard);
      g.strokePath(ch, juce::PathStrokeType(1.6f, juce::PathStrokeType::mitered, juce::PathStrokeType::square));

      // Filename above card
      juce::Rectangle<float> nameRect(dockLeft, nameY, dockW - 4.0f, 16.0f);
      g.setFont(juce::Font(12.0f, juce::Font::bold));
      g.setColour(juce::Colour::fromRGB(10, 11, 14).withAlpha(0.85f));
      g.drawText(selItem.fileName, nameRect.translated(0.0f, 1.0f), juce::Justification::centredLeft, true);
      g.setColour(neutralHoverWhite);
      g.drawText(selItem.fileName, nameRect, juce::Justification::centredLeft, true);

      // Up / Down Navigation Buttons (left of card)
      float arrowX = dockLeft - 22.0f;
      upArrowBounds = juce::Rectangle<float>(arrowX, mainY + 2.0f, 16.0f, 16.0f);
      downArrowBounds = juce::Rectangle<float>(arrowX, mainY + 20.0f, 16.0f, 16.0f);

      // Up Arrow button ▲
      g.setColour(bgSlot);
      g.fillRoundedRectangle(upArrowBounds, 2.0f);
      g.setColour(isHoveringUpArrow ? neutralHighlight : borderHairline);
      g.drawRoundedRectangle(upArrowBounds, 2.0f, 1.0f);

      juce::Path upP;
      upP.addTriangle(upArrowBounds.getCentreX(), upArrowBounds.getY() + 4.0f,
                      upArrowBounds.getX() + 3.0f, upArrowBounds.getBottom() - 4.0f,
                      upArrowBounds.getRight() - 3.0f, upArrowBounds.getBottom() - 4.0f);
      g.setColour(isHoveringUpArrow ? neutralHoverWhite : neutralMutedText);
      g.fillPath(upP);

      // Down Arrow button ▼
      g.setColour(bgSlot);
      g.fillRoundedRectangle(downArrowBounds, 2.0f);
      g.setColour(isHoveringDownArrow ? neutralHighlight : borderHairline);
      g.drawRoundedRectangle(downArrowBounds, 2.0f, 1.0f);

      juce::Path downP;
      downP.addTriangle(downArrowBounds.getX() + 3.0f, downArrowBounds.getY() + 4.0f,
                        downArrowBounds.getRight() - 3.0f, downArrowBounds.getY() + 4.0f,
                        downArrowBounds.getCentreX(), downArrowBounds.getBottom() - 4.0f);
      g.setColour(isHoveringDownArrow ? neutralHoverWhite : neutralMutedText);
      g.fillPath(downP);

      // Main Active Card (Angular with crisp 3.5px corners)
      mainPillBounds = juce::Rectangle<float>(dockLeft, mainY, dockW, mainCardH);
      juce::ColourGradient cardGrad(bgCardTop, mainPillBounds.getX(), mainPillBounds.getY(),
                                    bgCardBottom, mainPillBounds.getX(), mainPillBounds.getBottom(), false);
      g.setGradientFill(cardGrad);
      g.fillRoundedRectangle(mainPillBounds, cardRadius);

      g.setColour(isHoveringMainPill ? neutralHighlight : borderActiveCard);
      g.drawRoundedRectangle(mainPillBounds, cardRadius, isHoveringMainPill ? 1.5f : 1.0f);

      // Square Play Button Tile
      playButtonBounds = juce::Rectangle<float>(mainPillBounds.getX() + 4.0f, mainPillBounds.getY() + 4.0f, 30.0f, 30.0f);
      g.setColour(isHoveringPlayButton ? bgSlotHover : bgSlot);
      g.fillRoundedRectangle(playButtonBounds, 3.0f);
      g.setColour(isHoveringPlayButton ? neutralHighlight : borderHairline);
      g.drawRoundedRectangle(playButtonBounds, 3.0f, 1.0f);

      float pcx = playButtonBounds.getCentreX();
      float pcy = playButtonBounds.getCentreY();
      g.setColour(neutralHoverWhite);
      if (audioEngine.isPlaying() && audioEngine.getCurrentFile() == juce::File(selItem.filePath)) {
        g.fillRect(pcx - 4.5f, pcy - 5.0f, 3.0f, 10.0f);
        g.fillRect(pcx + 1.5f, pcy - 5.0f, 3.0f, 10.0f);
      } else {
        juce::Path playP;
        playP.addTriangle(pcx - 3.0f, pcy - 5.5f,
                          pcx - 3.0f, pcy + 5.5f,
                          pcx + 5.5f, pcy);
        g.fillPath(playP);
      }

      // Waveform Slot inside Main Card
      juce::Rectangle<float> waveRect(playButtonBounds.getRight() + 8.0f, mainPillBounds.getY() + 4.0f,
                                      dockW - 42.0f - 40.0f, mainCardH - 8.0f);
      g.setColour(juce::Colour::fromRGB(15, 17, 21).withAlpha(0.65f));
      g.fillRoundedRectangle(waveRect, 2.0f);
      g.setColour(borderHairline.withAlpha(0.40f));
      g.drawRoundedRectangle(waveRect, 2.0f, 0.8f);

      if (audioEngine.getThumbnail().getTotalLength() > 0.0 &&
          audioEngine.getCurrentFile() == juce::File(selItem.filePath)) {
        g.setColour(neutralHoverWhite.withAlpha(0.92f));
        double totalLen = audioEngine.getThumbnail().getTotalLength();
        double activeEnd = totalLen;
        if (totalLen > 0.35) {
          float maxPeak = 0.0f;
          const double stepSec = std::max(0.05, totalLen / 64.0);
          for (double t = 0.0; t < totalLen; t += stepSec) {
            float mn = 0.0f, mx = 0.0f;
            audioEngine.getThumbnail().getApproximateMinMax(t, std::min(totalLen, t + stepSec), 0, mn, mx);
            maxPeak = std::max(maxPeak, std::max(std::abs(mn), std::abs(mx)));
          }
          if (maxPeak > 0.01f) {
            float threshold = maxPeak * 0.04f;
            const double revStep = std::max(0.04, totalLen / 80.0);
            for (double t = totalLen; t >= 0.15; t -= revStep) {
              float mn = 0.0f, mx = 0.0f;
              audioEngine.getThumbnail().getApproximateMinMax(std::max(0.0, t - revStep), t, 0, mn, mx);
              if (std::max(std::abs(mn), std::abs(mx)) > threshold) {
                activeEnd = std::min(totalLen, t + 0.10);
                break;
              }
            }
          }
        }
        audioEngine.getThumbnail().drawChannels(g, waveRect.toNearestInt(), 0.0, activeEnd, 1.0f);
      } else {
        drawMiniWaveform(g, waveRect, selItem, waveformColor);
      }

      // Favorite Star
      favoriteStarBounds = juce::Rectangle<float>(mainPillBounds.getRight() - 26.0f, mainPillBounds.getY() + 10.0f, 18.0f, 18.0f);
      if (selItem.isFavorite) {
        g.setColour(juce::Colour::fromRGB(255, 215, 0));
        g.setFont(juce::Font(16.0f));
        g.drawText(juce::String::fromUTF8("\xe2\x98\x85"), favoriteStarBounds, juce::Justification::centred);
      } else {
        g.setColour(isHoveringFavoriteStar ? neutralHoverWhite : neutralMutedText);
        g.setFont(juce::Font(16.0f));
        g.drawText(juce::String::fromUTF8("\xe2\x98\x86"), favoriteStarBounds, juce::Justification::centred);
      }

    } else {
      // ----------------- EXPANDED MODE (Modern Modular Slot Rack) -----------------
      float centerX = dockLeft + dockW * 0.5f;
      float startY = 10.0f;
      overallDockBounds = getDockDeadzone();

      // Top Collapse Chevron (pointing up ∧)
      expandChevronBounds = juce::Rectangle<float>(centerX - 24.0f, startY - 2.0f, 48.0f, 18.0f);
      juce::Path ch;
      ch.startNewSubPath(centerX - 6.0f, startY + 9.0f);
      ch.lineTo(centerX, startY + 3.0f);
      ch.lineTo(centerX + 6.0f, startY + 9.0f);
      g.setColour(isHoveringExpandChevron ? neutralHoverWhite : borderActiveCard);
      g.strokePath(ch, juce::PathStrokeType(1.6f, juce::PathStrokeType::mitered, juce::PathStrokeType::square));

      // 3 Slots Above (rendered top to bottom: +3, +2, +1)
      const float aboveYs[3] = { 28.0f, 56.0f, 84.0f };
      const juce::String aboveTags[3] = { "+3", "+2", "+1" };

      for (int i = 0; i < 3; ++i) {
        int idx = 2 - i; // idx 2 is furthest (+3, at top), idx 0 is closest (+1, right above)
        if (idx < static_cast<int>(similarAbove.size())) {
          float y = aboveYs[i];
          similarAbove[idx].bounds = juce::Rectangle<float>(dockLeft, y, dockW, slotCardH);

          bool isHovered = (hoveredSimilarIndex == -(idx + 1));
          g.setColour(isHovered ? bgSlotHover : bgSlot);
          g.fillRoundedRectangle(similarAbove[idx].bounds, slotRadius);
          g.setColour(isHovered ? neutralHighlight : borderHairline);
          g.drawRoundedRectangle(similarAbove[idx].bounds, slotRadius, isHovered ? 1.4f : 1.0f);

          // Step Badge on the left
          juce::Rectangle<float> tagRect(dockLeft + 5.0f, y + 4.0f, 22.0f, slotCardH - 8.0f);
          g.setFont(juce::Font(10.0f, juce::Font::bold));
          g.setColour(isHovered ? neutralHoverWhite : neutralMutedText);
          g.drawText(aboveTags[i], tagRect, juce::Justification::centred);

          // Hairline separator
          g.setColour(borderHairline.withAlpha(0.60f));
          g.fillRect(dockLeft + 29.0f, y + 4.0f, 1.0f, slotCardH - 8.0f);

          // Mini waveform inside slot
          juce::Rectangle<float> slotWaveRect(dockLeft + 36.0f, y + 3.0f, dockW - 44.0f, slotCardH - 6.0f);
          drawMiniWaveform(g, slotWaveRect, similarAbove[idx].item,
                           isHovered ? neutralHoverWhite : waveformColor.withAlpha(0.85f));

          // Hover Tooltip Badge showing filename
          if (isHovered) {
            juce::Font tipFont(11.5f, juce::Font::bold);
            g.setFont(tipFont);
            float tipW = tipFont.getStringWidthFloat(similarAbove[idx].item.fileName) + 16.0f;
            juce::Rectangle<float> tipRect(similarAbove[idx].bounds.getX() - tipW - 8.0f,
                                          y + (slotCardH - 20.0f) * 0.5f, tipW, 20.0f);
            g.setColour(juce::Colour::fromRGB(18, 20, 24).withAlpha(0.96f));
            g.fillRoundedRectangle(tipRect, 3.0f);
            g.setColour(neutralHighlight);
            g.drawRoundedRectangle(tipRect, 3.0f, 1.0f);
            g.setColour(neutralHoverWhite);
            g.drawText(similarAbove[idx].item.fileName, tipRect, juce::Justification::centred);
          }
        }
      }

      // Center Active Item Header & Card
      float mainY = 132.0f;
      float nameY = 114.0f;
      juce::Rectangle<float> nameRect(dockLeft, nameY, dockW - 4.0f, 16.0f);
      g.setFont(juce::Font(12.0f, juce::Font::bold));
      g.setColour(juce::Colour::fromRGB(10, 11, 14).withAlpha(0.85f));
      g.drawText(selItem.fileName, nameRect.translated(0.0f, 1.0f), juce::Justification::centredLeft, true);
      g.setColour(neutralHoverWhite);
      g.drawText(selItem.fileName, nameRect, juce::Justification::centredLeft, true);

      // Up / Down Navigation Buttons
      float arrowX = dockLeft - 22.0f;
      upArrowBounds = juce::Rectangle<float>(arrowX, mainY + 2.0f, 16.0f, 16.0f);
      downArrowBounds = juce::Rectangle<float>(arrowX, mainY + 20.0f, 16.0f, 16.0f);

      g.setColour(bgSlot);
      g.fillRoundedRectangle(upArrowBounds, 2.0f);
      g.setColour(isHoveringUpArrow ? neutralHighlight : borderHairline);
      g.drawRoundedRectangle(upArrowBounds, 2.0f, 1.0f);

      juce::Path upP;
      upP.addTriangle(upArrowBounds.getCentreX(), upArrowBounds.getY() + 4.0f,
                      upArrowBounds.getX() + 3.0f, upArrowBounds.getBottom() - 4.0f,
                      upArrowBounds.getRight() - 3.0f, upArrowBounds.getBottom() - 4.0f);
      g.setColour(isHoveringUpArrow ? neutralHoverWhite : neutralMutedText);
      g.fillPath(upP);

      g.setColour(bgSlot);
      g.fillRoundedRectangle(downArrowBounds, 2.0f);
      g.setColour(isHoveringDownArrow ? neutralHighlight : borderHairline);
      g.drawRoundedRectangle(downArrowBounds, 2.0f, 1.0f);

      juce::Path downP;
      downP.addTriangle(downArrowBounds.getX() + 3.0f, downArrowBounds.getY() + 4.0f,
                        downArrowBounds.getRight() - 3.0f, downArrowBounds.getY() + 4.0f,
                        downArrowBounds.getCentreX(), downArrowBounds.getBottom() - 4.0f);
      g.setColour(isHoveringDownArrow ? neutralHoverWhite : neutralMutedText);
      g.fillPath(downP);

      // Main Active Card
      mainPillBounds = juce::Rectangle<float>(dockLeft, mainY, dockW, mainCardH);
      juce::ColourGradient cardGrad(bgCardTop, mainPillBounds.getX(), mainPillBounds.getY(),
                                    bgCardBottom, mainPillBounds.getX(), mainPillBounds.getBottom(), false);
      g.setGradientFill(cardGrad);
      g.fillRoundedRectangle(mainPillBounds, cardRadius);
      g.setColour(isHoveringMainPill ? neutralHighlight : borderActiveCard);
      g.drawRoundedRectangle(mainPillBounds, cardRadius, isHoveringMainPill ? 1.5f : 1.0f);

      // Square Play Button Tile
      playButtonBounds = juce::Rectangle<float>(mainPillBounds.getX() + 4.0f, mainPillBounds.getY() + 4.0f, 30.0f, 30.0f);
      g.setColour(isHoveringPlayButton ? bgSlotHover : bgSlot);
      g.fillRoundedRectangle(playButtonBounds, 3.0f);
      g.setColour(isHoveringPlayButton ? neutralHighlight : borderHairline);
      g.drawRoundedRectangle(playButtonBounds, 3.0f, 1.0f);

      float pcx = playButtonBounds.getCentreX();
      float pcy = playButtonBounds.getCentreY();
      g.setColour(neutralHoverWhite);
      if (audioEngine.isPlaying() && audioEngine.getCurrentFile() == juce::File(selItem.filePath)) {
        g.fillRect(pcx - 4.5f, pcy - 5.0f, 3.0f, 10.0f);
        g.fillRect(pcx + 1.5f, pcy - 5.0f, 3.0f, 10.0f);
      } else {
        juce::Path playP;
        playP.addTriangle(pcx - 3.0f, pcy - 5.5f,
                          pcx - 3.0f, pcy + 5.5f,
                          pcx + 5.5f, pcy);
        g.fillPath(playP);
      }

      // Waveform Slot inside Main Card
      juce::Rectangle<float> waveRect(playButtonBounds.getRight() + 8.0f, mainPillBounds.getY() + 4.0f,
                                      dockW - 42.0f - 40.0f, mainCardH - 8.0f);
      g.setColour(juce::Colour::fromRGB(15, 17, 21).withAlpha(0.65f));
      g.fillRoundedRectangle(waveRect, 2.0f);
      g.setColour(borderHairline.withAlpha(0.40f));
      g.drawRoundedRectangle(waveRect, 2.0f, 0.8f);

      if (audioEngine.getThumbnail().getTotalLength() > 0.0 &&
          audioEngine.getCurrentFile() == juce::File(selItem.filePath)) {
        g.setColour(neutralHoverWhite.withAlpha(0.92f));
        double totalLen = audioEngine.getThumbnail().getTotalLength();
        double activeEnd = totalLen;
        if (totalLen > 0.35) {
          float maxPeak = 0.0f;
          const double stepSec = std::max(0.05, totalLen / 64.0);
          for (double t = 0.0; t < totalLen; t += stepSec) {
            float mn = 0.0f, mx = 0.0f;
            audioEngine.getThumbnail().getApproximateMinMax(t, std::min(totalLen, t + stepSec), 0, mn, mx);
            maxPeak = std::max(maxPeak, std::max(std::abs(mn), std::abs(mx)));
          }
          if (maxPeak > 0.01f) {
            float threshold = maxPeak * 0.04f;
            const double revStep = std::max(0.04, totalLen / 80.0);
            for (double t = totalLen; t >= 0.15; t -= revStep) {
              float mn = 0.0f, mx = 0.0f;
              audioEngine.getThumbnail().getApproximateMinMax(std::max(0.0, t - revStep), t, 0, mn, mx);
              if (std::max(std::abs(mn), std::abs(mx)) > threshold) {
                activeEnd = std::min(totalLen, t + 0.10);
                break;
              }
            }
          }
        }
        audioEngine.getThumbnail().drawChannels(g, waveRect.toNearestInt(), 0.0, activeEnd, 1.0f);
      } else {
        drawMiniWaveform(g, waveRect, selItem, waveformColor);
      }

      // Favorite Star
      favoriteStarBounds = juce::Rectangle<float>(mainPillBounds.getRight() - 26.0f, mainPillBounds.getY() + 10.0f, 18.0f, 18.0f);
      if (selItem.isFavorite) {
        g.setColour(juce::Colour::fromRGB(255, 215, 0));
        g.setFont(juce::Font(16.0f));
        g.drawText(juce::String::fromUTF8("\xe2\x98\x85"), favoriteStarBounds, juce::Justification::centred);
      } else {
        g.setColour(isHoveringFavoriteStar ? neutralHoverWhite : neutralMutedText);
        g.setFont(juce::Font(16.0f));
        g.drawText(juce::String::fromUTF8("\xe2\x98\x86"), favoriteStarBounds, juce::Justification::centred);
      }

      // 3 Slots Below (rendered top to bottom: -1, -2, -3)
      const float belowYs[3] = { 176.0f, 204.0f, 232.0f };
      const juce::String belowTags[3] = { "-1", "-2", "-3" };

      for (int i = 0; i < 3; ++i) {
        if (i < static_cast<int>(similarBelow.size())) {
          float y = belowYs[i];
          similarBelow[i].bounds = juce::Rectangle<float>(dockLeft, y, dockW, slotCardH);

          bool isHovered = (hoveredSimilarIndex == (i + 1));
          g.setColour(isHovered ? bgSlotHover : bgSlot);
          g.fillRoundedRectangle(similarBelow[i].bounds, slotRadius);
          g.setColour(isHovered ? neutralHighlight : borderHairline);
          g.drawRoundedRectangle(similarBelow[i].bounds, slotRadius, isHovered ? 1.4f : 1.0f);

          // Step Badge on the left
          juce::Rectangle<float> tagRect(dockLeft + 5.0f, y + 4.0f, 22.0f, slotCardH - 8.0f);
          g.setFont(juce::Font(10.0f, juce::Font::bold));
          g.setColour(isHovered ? neutralHoverWhite : neutralMutedText);
          g.drawText(belowTags[i], tagRect, juce::Justification::centred);

          // Hairline separator
          g.setColour(borderHairline.withAlpha(0.60f));
          g.fillRect(dockLeft + 29.0f, y + 4.0f, 1.0f, slotCardH - 8.0f);

          // Mini waveform inside slot
          juce::Rectangle<float> slotWaveRect(dockLeft + 36.0f, y + 3.0f, dockW - 44.0f, slotCardH - 6.0f);
          drawMiniWaveform(g, slotWaveRect, similarBelow[i].item,
                           isHovered ? neutralHoverWhite : waveformColor.withAlpha(0.85f));

          if (isHovered) {
            juce::Font tipFont(11.5f, juce::Font::bold);
            g.setFont(tipFont);
            float tipW = tipFont.getStringWidthFloat(similarBelow[i].item.fileName) + 16.0f;
            juce::Rectangle<float> tipRect(similarBelow[i].bounds.getX() - tipW - 8.0f,
                                          y + (slotCardH - 20.0f) * 0.5f, tipW, 20.0f);
            g.setColour(juce::Colour::fromRGB(18, 20, 24).withAlpha(0.96f));
            g.fillRoundedRectangle(tipRect, 3.0f);
            g.setColour(neutralHighlight);
            g.drawRoundedRectangle(tipRect, 3.0f, 1.0f);
            g.setColour(neutralHoverWhite);
            g.drawText(similarBelow[i].item.fileName, tipRect, juce::Justification::centred);
          }
        }
      }
    }
  } else if (!nodes.empty()) {
    // Subtle hint pill when no sample is selected yet
    const float dockRight = effW - 25.0f;
    juce::Rectangle<float> hintRect(dockRight - 230.0f, 24.0f, 230.0f, 28.0f);
    g.setColour(juce::Colour::fromRGB(20, 22, 26).withAlpha(0.75f));
    g.fillRoundedRectangle(hintRect, 3.5f);
    g.setColour(juce::Colour::fromRGB(52, 56, 66));
    g.drawRoundedRectangle(hintRect, 3.5f, 1.0f);
    g.setFont(juce::Font(11.5f, juce::Font::plain));
    g.setColour(juce::Colour::fromRGB(200, 208, 220).withAlpha(0.85f));
    g.drawText(juce::String::fromUTF8("\xe2\x9c\xa8 Click a star to audition & drag"), hintRect, juce::Justification::centred);
  }
}

void SampleCloudComponent::timerCallback() {
  bool needsRepaint = false;

  if (revealAlpha < 1.0f && !layoutPending) {
    revealAlpha = std::min(1.0f, revealAlpha + 0.03f);
    needsRepaint = true;
  }

  // Smooth ease for hover scale expansion (150% larger)
  if (std::abs(targetHoverScale - currentHoverScale) > 0.005f) {
    currentHoverScale += (targetHoverScale - currentHoverScale) * 0.22f;
    needsRepaint = true;
  } else {
    currentHoverScale = targetHoverScale;
    if (currentHoverScale <= 1.01f && hoveredNodeIndex < 0) {
      animatedHoverNodeIndex = -1;
    }
  }

  if (std::abs(targetRotX - rotX) > 0.001f ||
      std::abs(targetRotY - rotY) > 0.001f) {
    rotX += (targetRotX - rotX) * 0.1f;
    rotY += (targetRotY - rotY) * 0.1f;
    needsRepaint = true;
  }

  if (std::abs(targetZoomScale - zoomScale) > 0.0005f) {
    zoomScale += (targetZoomScale - zoomScale) * 0.18f;
    needsRepaint = true;
  }
  targetZoomScale = juce::jlimit(0.20f, 3.5f, targetZoomScale);
  zoomScale = juce::jlimit(0.20f, 3.5f, zoomScale);

  targetCameraCenterPos.x = juce::jlimit(-maxMapPanX, maxMapPanX, targetCameraCenterPos.x);
  targetCameraCenterPos.y = juce::jlimit(-maxMapPanY, maxMapPanY, targetCameraCenterPos.y);
  if (!is2DMode) {
    targetCameraCenterPos.z = juce::jlimit(-maxMapPanZ, maxMapPanZ, targetCameraCenterPos.z);
  } else {
    targetCameraCenterPos.z = 0.0f;
  }

  if (!isPanning) {
    float dx = targetCameraCenterPos.x - cameraCenterPos.x;
    float dy = targetCameraCenterPos.y - cameraCenterPos.y;
    float dz = is2DMode ? 0.0f : (targetCameraCenterPos.z - cameraCenterPos.z);
    float distSq = dx * dx + dy * dy + dz * dz;
    float velSq = cameraVelocity.x * cameraVelocity.x + cameraVelocity.y * cameraVelocity.y + cameraVelocity.z * cameraVelocity.z;

    if (distSq > 0.02f || velSq > 0.02f) {
      const float dt = 1.0f / 60.0f;
      const float smoothTime = 0.22f; // Organic, continuous critically damped transition
      cameraCenterPos.x = smoothDamp(cameraCenterPos.x, targetCameraCenterPos.x, cameraVelocity.x, smoothTime, dt);
      cameraCenterPos.y = smoothDamp(cameraCenterPos.y, targetCameraCenterPos.y, cameraVelocity.y, smoothTime, dt);
      if (!is2DMode) {
        cameraCenterPos.z = smoothDamp(cameraCenterPos.z, targetCameraCenterPos.z, cameraVelocity.z, smoothTime, dt);
      } else {
        cameraCenterPos.z = 0.0f;
        cameraVelocity.z = 0.0f;
      }
      needsRepaint = true;
    } else {
      cameraCenterPos = targetCameraCenterPos;
      cameraVelocity = { 0.0f, 0.0f, 0.0f };
    }
  } else {
    cameraVelocity = { 0.0f, 0.0f, 0.0f };
  }

  if (!isPanning && !isRotating && !is2DMode && autoRotate) {
    targetRotY -= 0.0005f;
    needsRepaint = true;
  }

  pulsePhase += 0.05f;
  reticleAngle += 0.035f;
  pulseRadar = std::fmod(pulseRadar + 0.035f, 1.0f);

  float effW = getEffectiveWidth();
  float effH = getEffectiveHeight();
  float aspect = (effH > 0.0f) ? (effW / effH) : 1.0f;
  auto projectionMatrix = juce::Matrix3D<float>::fromFrustum(
      -aspect * 0.1f, aspect * 0.1f, -0.1f, 0.1f, 0.2f, 1000000.0f);

  juce::Matrix3D<float> viewMatrix;
  viewMatrix = viewMatrix *
               juce::Matrix3D<float>::fromTranslation(
                   {0.0f, 0.0f, -cameraDistance / zoomScale});

  float crx = std::cos(rotX), srx = std::sin(rotX);
  float cry = std::cos(rotY), sry = std::sin(rotY);
  juce::Matrix3D<float> rotMatrix(cry, 0.0f, sry, 0.0f, srx * sry, crx,
                                  -srx * cry, 0.0f, -crx * sry, srx, crx * cry,
                                  0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
  viewMatrix = viewMatrix * rotMatrix;

  viewMatrix = viewMatrix *
               juce::Matrix3D<float>::fromTranslation(
                   {-cameraCenterPos.x, -cameraCenterPos.y, -cameraCenterPos.z});

  lastViewProjectionMatrix = projectionMatrix * viewMatrix;

#if JUCE_MAC
  if (metalView && isShowing()) {
    MetalUniforms u {};
    memcpy(u.projectionMatrix, projectionMatrix.mat, sizeof(u.projectionMatrix));
    memcpy(u.viewMatrix, viewMatrix.mat, sizeof(u.viewMatrix));
    u.revealAlpha = revealAlpha;
    u.time = static_cast<float>(juce::Time::getMillisecondCounterHiRes() / 1000.0);
    bool isLightMode = OpenWavLookAndFeel::bgCard.getBrightness() > 0.5f;
    u.isLightMode = isLightMode ? 1.0f : 0.0f;
    float scale = 1.0f;
    if (auto *peer = getPeer())
      scale = (float)peer->getPlatformScaleFactor();
    if (scale < 1.0f) scale = 1.0f;
    u.scaleFactor = scale;
    const juce::Colour bgAnthracite = juce::Colour::fromRGB(22, 23, 26);
    auto activeBg = isLightMode ? OpenWavLookAndFeel::bgCard : bgAnthracite;
    u.fogColor[0] = activeBg.getFloatRed();
    u.fogColor[1] = activeBg.getFloatGreen();
    u.fogColor[2] = activeBg.getFloatBlue();
    u.viewportWidth = effW;
    u.viewportHeight = effH;
    int hoveredVtxIdx = -1;
    if (animatedHoverNodeIndex >= 0 && animatedHoverNodeIndex < static_cast<int>(nodes.size())) {
      hoveredVtxIdx = animatedHoverNodeIndex;
    }
    u.hoveredVertexIndex = hoveredVtxIdx;
    u.hoverScale = currentHoverScale;

    metalView->setClearColor(activeBg.getFloatRed(), activeBg.getFloatGreen(), activeBg.getFloatBlue(), 1.0f);
    metalView->updateUniforms(u);
    metalView->renderNow();
  }

  // Repaint overlay for reticle, radar pulse, hover animation, or reveal animation
  if (hoveredNodeIndex >= 0 || animatedHoverNodeIndex >= 0 || selectedNodeIndex >= 0 || revealAlpha < 1.0f) {
    overlayComponent.repaint();
  }
#else
  if (needsRepaint) {
    overlayComponent.repaint();
    repaint();
  }
#endif
}

void SampleCloudComponent::resetZoomAndPan() {
  selectedNodeIndex = -1;
  targetZoomScale = 1.0f;
  zoomScale = 1.0f;
  targetCameraCenterPos = {0.0f, 0.0f, 0.0f};
  cameraCenterPos = {0.0f, 0.0f, 0.0f};
  cameraVelocity = {0.0f, 0.0f, 0.0f};
  if (is2DMode) {
    targetRotX = 0.0f;
    targetRotY = 0.0f;
    rotX = 0.0f;
    rotY = 0.0f;
  } else {
    targetRotX = 0.35f;
    targetRotY = 0.45f;
    rotX = 0.35f;
    rotY = 0.45f;
  }
  overlayComponent.repaint();
  repaint();
}

void SampleCloudComponent::refreshView() {
  resized();
  overlayComponent.resized();
  overlayComponent.repaint();
  repaint();

#if JUCE_MAC
  if (auto *peer = getPeer()) {
    if (overlayComponent.isOnDesktop()) {
      overlayComponent.setBounds(peer->getAreaCoveredBy(*this));
      overlayComponent.toFront(false);
    }
  }
  if (metalView && isShowing()) {
    float effW = getEffectiveWidth();
    float effH = getEffectiveHeight();
    float aspect = (effH > 0.0f) ? (effW / effH) : 1.0f;
    auto projectionMatrix = juce::Matrix3D<float>::fromFrustum(
        -aspect * 0.1f, aspect * 0.1f, -0.1f, 0.1f, 0.2f, 1000000.0f);

    juce::Matrix3D<float> viewMatrix;
    viewMatrix = viewMatrix *
                 juce::Matrix3D<float>::fromTranslation(
                     {0.0f, 0.0f, -cameraDistance / zoomScale});

    float crx = std::cos(rotX), srx = std::sin(rotX);
    float cry = std::cos(rotY), sry = std::sin(rotY);
    juce::Matrix3D<float> rotMatrix(cry, 0.0f, sry, 0.0f, srx * sry, crx,
                                    -srx * cry, 0.0f, -crx * sry, srx, crx * cry,
                                    0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    viewMatrix = viewMatrix * rotMatrix;

    viewMatrix = viewMatrix *
                 juce::Matrix3D<float>::fromTranslation(
                     {-cameraCenterPos.x, -cameraCenterPos.y, -cameraCenterPos.z});

    lastViewProjectionMatrix = projectionMatrix * viewMatrix;

    MetalUniforms u {};
    memcpy(u.projectionMatrix, projectionMatrix.mat, sizeof(u.projectionMatrix));
    memcpy(u.viewMatrix, viewMatrix.mat, sizeof(u.viewMatrix));
    u.revealAlpha = revealAlpha;
    u.time = static_cast<float>(juce::Time::getMillisecondCounterHiRes() / 1000.0);
    bool isLightMode = OpenWavLookAndFeel::bgCard.getBrightness() > 0.5f;
    u.isLightMode = isLightMode ? 1.0f : 0.0f;
    float scale = 1.0f;
    if (auto *peer = getPeer())
      scale = (float)peer->getPlatformScaleFactor();
    if (scale < 1.0f) scale = 1.0f;
    u.scaleFactor = scale;
    const juce::Colour bgAnthracite = juce::Colour::fromRGB(22, 23, 26);
    auto activeBg = isLightMode ? OpenWavLookAndFeel::bgCard : bgAnthracite;
    u.fogColor[0] = activeBg.getFloatRed();
    u.fogColor[1] = activeBg.getFloatGreen();
    u.fogColor[2] = activeBg.getFloatBlue();
    u.viewportWidth = effW;
    u.viewportHeight = effH;
    int hoveredVtxIdx = -1;
    if (animatedHoverNodeIndex >= 0 && animatedHoverNodeIndex < static_cast<int>(nodes.size())) {
      hoveredVtxIdx = animatedHoverNodeIndex;
    }
    u.hoveredVertexIndex = hoveredVtxIdx;
    u.hoverScale = currentHoverScale;

    metalView->setClearColor(activeBg.getFloatRed(), activeBg.getFloatGreen(), activeBg.getFloatBlue(), 1.0f);
    metalView->updateUniforms(u);
    metalView->renderNow();
  }
#endif
}

void SampleCloudComponent::resized() {
  auto b = getLocalBounds();
#if JUCE_MAC
  if (metalContainer)
    metalContainer->setBounds(b);
  if (overlayComponent.isOnDesktop()) {
    if (auto *peer = getPeer())
      overlayComponent.setBounds(peer->getAreaCoveredBy(*this));
    else
      overlayComponent.setBounds(b);
  } else {
    overlayComponent.setBounds(b);
  }
#else
  overlayComponent.setBounds(b);
#endif
}

void SampleCloudComponent::mouseDown(const juce::MouseEvent &e) {
  mouseDragStartPos = e.position;
  isPanning = false;
  isRotating = false;

  if (hoverPlayButton.getBounds().toFloat().expanded(3.0f).contains(e.position)) {
    isClickConsumedByUI = true;
    return;
  }

  auto dockDeadzone = getDockDeadzone();
  if (dockDeadzone.contains(e.position)) {
    isClickConsumedByUI = true;

    if (selectedNodeIndex >= 0 && selectedNodeIndex < static_cast<int>(nodes.size())) {
      // 1. Expand / Collapse Chevron (top chevron)
      if (expandChevronBounds.expanded(8.0f).contains(e.position)) {
        showSimilarStack = !showSimilarStack;
        lastDockToggleTime = juce::Time::getMillisecondCounter();
        overlayComponent.repaint();
        repaint();
        return;
      }

      const juce::Rectangle<float> upHitBounds(
          upArrowBounds.getX() - 4.0f, upArrowBounds.getY() - 3.0f,
          upArrowBounds.getWidth() + 8.0f, upArrowBounds.getHeight() + 4.0f);
      const juce::Rectangle<float> downHitBounds(
          downArrowBounds.getX() - 4.0f, downArrowBounds.getY() - 1.0f,
          downArrowBounds.getWidth() + 8.0f, downArrowBounds.getHeight() + 4.0f);

      // 2. Up Arrow ▲ (previous / higher sample)
      if (upHitBounds.contains(e.position)) {
        int nextIdx = -1;
        if (!similarAbove.empty()) {
          nextIdx = similarAbove[0].nodeIndex;
        } else if (selectedNodeIndex > 0) {
          nextIdx = selectedNodeIndex - 1;
        }
        if (nextIdx >= 0 && nextIdx < static_cast<int>(nodes.size())) {
          selectedNodeIndex = nextIdx;
          targetCameraCenterPos = nodes[selectedNodeIndex].targetPos;
          audioEngine.auditionFile(juce::File(nodes[selectedNodeIndex].item.filePath));
          updateSimilarStack();
          listeners.call([&](SampleCloudListener &l) {
            l.cloudSampleSelected(nodes[selectedNodeIndex].item);
          });
          overlayComponent.repaint();
          repaint();
        }
        return;
      }

      // 3. Down Arrow ▼ (next / lower sample)
      if (downHitBounds.contains(e.position)) {
        int nextIdx = -1;
        if (!similarBelow.empty()) {
          nextIdx = similarBelow[0].nodeIndex;
        } else if (selectedNodeIndex + 1 < static_cast<int>(nodes.size())) {
          nextIdx = selectedNodeIndex + 1;
        }
        if (nextIdx >= 0 && nextIdx < static_cast<int>(nodes.size())) {
          selectedNodeIndex = nextIdx;
          targetCameraCenterPos = nodes[selectedNodeIndex].targetPos;
          audioEngine.auditionFile(juce::File(nodes[selectedNodeIndex].item.filePath));
          updateSimilarStack();
          listeners.call([&](SampleCloudListener &l) {
            l.cloudSampleSelected(nodes[selectedNodeIndex].item);
          });
          overlayComponent.repaint();
          repaint();
        }
        return;
      }

      // 4. Play Button Circle
      if (playButtonBounds.expanded(3.0f).contains(e.position)) {
        auto &item = nodes[selectedNodeIndex].item;
        juce::File targetFile(item.filePath);
        if (audioEngine.isPlaying() && audioEngine.getCurrentFile() == targetFile) {
          audioEngine.stop();
        } else {
          audioEngine.auditionFile(targetFile);
        }
        overlayComponent.repaint();
        return;
      }

      // 5. Favorite Star
      if (favoriteStarBounds.expanded(4.0f).contains(e.position)) {
        auto &item = nodes[selectedNodeIndex].item;
        dbManager.toggleFavorite(item.id);
        item.isFavorite = !item.isFavorite;
        overlayComponent.repaint();
        return;
      }

      // 6. Main Center Pill (audition or drag to DAW)
      if (mainPillBounds.contains(e.position)) {
        dockDragFilePath = nodes[selectedNodeIndex].item.filePath;
        dockDragNodeIndex = selectedNodeIndex;
        dockDragStartPos = e.position;
        isDraggingDockSample = true;
        return;
      }

      // 7. Similar Items in Stack (audition, select or drag to DAW)
      if (showSimilarStack) {
        for (const auto &entry : similarAbove) {
          if (entry.bounds.contains(e.position)) {
            dockDragFilePath = entry.item.filePath;
            dockDragNodeIndex = entry.nodeIndex;
            dockDragStartPos = e.position;
            isDraggingDockSample = true;
            return;
          }
        }
        for (const auto &entry : similarBelow) {
          if (entry.bounds.contains(e.position)) {
            dockDragFilePath = entry.item.filePath;
            dockDragNodeIndex = entry.nodeIndex;
            dockDragStartPos = e.position;
            isDraggingDockSample = true;
            return;
          }
        }
      }
    }

    // Swallowed by deadzone: map will never pan, drag or select stars
    return;
  }

  isClickConsumedByUI = false;

  // Debounce protection: if dock was just toggled in the last 400ms, swallow click to prevent accidental star selection
  if (juce::Time::getMillisecondCounter() - lastDockToggleTime < 400) {
    return;
  }

  // Always run fresh precision 2D-center hit test against current camera projection
  const int hitIdx = findHitNodeAt(e.position);
  hoveredNodeIndex = hitIdx;

  const uint32_t clickNowMs = juce::Time::getMillisecondCounter();
  int targetNodeIdx = hitIdx;
  // Stabilize rapid stationary re-clicks (<= 2.5px) while the camera is centering the clicked star,
  // without interfering when the user moves the cursor onto a close neighboring sample
  if (selectedNodeIndex >= 0 && selectedNodeIndex < static_cast<int>(nodes.size()) &&
      (clickNowMs - lastMapNodeClickTimeMs) < 420 &&
      (e.position - lastMapNodeClickPos).getDistanceFromOrigin() <= 2.5f) {
    targetNodeIdx = selectedNodeIndex;
  }

  if (e.mods.isRightButtonDown() || e.mods.isPopupMenu()) {
    if (targetNodeIdx >= 0 && targetNodeIdx < static_cast<int>(nodes.size())) {
      const bool selectionChanged = (selectedNodeIndex != targetNodeIdx);
      selectedNodeIndex = targetNodeIdx;
      lastMapNodeClickPos = e.position;
      lastMapNodeClickTimeMs = clickNowMs;
      targetCameraCenterPos = nodes[selectedNodeIndex].targetPos;
      audioEngine.auditionFile(juce::File(nodes[selectedNodeIndex].item.filePath));
      if (selectionChanged || (similarAbove.empty() && similarBelow.empty())) {
        updateSimilarStack();
        listeners.call([&](SampleCloudListener &l) {
          l.cloudSampleSelected(nodes[selectedNodeIndex].item);
        });
      }
      overlayComponent.repaint();
      repaint();
      showContextMenuForNode(selectedNodeIndex);
    }
    return;
  }

  // Node clicked directly: immediately cut off previous playback & start/restart from 0.0
  if (targetNodeIdx >= 0 && targetNodeIdx < static_cast<int>(nodes.size())) {
    const bool selectionChanged = (selectedNodeIndex != targetNodeIdx);
    selectedNodeIndex = targetNodeIdx;
    lastMapNodeClickPos = e.position;
    lastMapNodeClickTimeMs = clickNowMs;
    targetCameraCenterPos = nodes[selectedNodeIndex].targetPos;
    if (is2DMode) targetCameraCenterPos.z = 0.0f;

    // 1. Trigger audio FIRST with zero latency (never queued behind UI updates!)
    audioEngine.auditionFile(juce::File(nodes[selectedNodeIndex].item.filePath));

    // 2. Only recompute similar stack & sync table when selection actually changed
    if (selectionChanged || (similarAbove.empty() && similarBelow.empty())) {
      updateSimilarStack();
      listeners.call([&](SampleCloudListener &l) {
        l.cloudSampleSelected(nodes[selectedNodeIndex].item);
      });
    }
    overlayComponent.repaint();
    repaint();

    mouseDragStartPos = e.position;
    isRotating = false;
    isPanning = false;
    return;
  }

  float effW = getEffectiveWidth();
  float effH = getEffectiveHeight();

  // Check if a bottom category in legend was clicked (exact match with paintOverlay geometry)
  if (e.y >= effH - 55.0f && e.y <= effH - 15.0f && e.x < effW - 280.0f) {
    float x = 20.0f + legendScrollOffset;
    float itemY = effH - 51.0f;
    float itemH = 32.0f;
    juce::Font legendFont(12.0f, juce::Font::bold);
    for (size_t i = 0; i < clusters.size(); ++i) {
      juce::String fullTag = getAtlasCategoryName(clusters[i].tag);
      float textW = legendFont.getStringWidthFloat(fullTag);
      float itemW = 24.0f + textW + 14.0f;
      juce::Rectangle<float> itemBounds(x, itemY, itemW, itemH);
      if (itemBounds.contains(e.position)) {
        flyToCluster(static_cast<int>(i));
        return;
      }
      x += itemW + 8.0f;
    }
  }

  mouseDragStartPos = e.position;
  isRotating = false;
  isPanning = false;
}

void SampleCloudComponent::mouseDrag(const juce::MouseEvent &e) {
  // If dragging a sample from the floating dock: initiate external drag-and-drop to DAW!
  if (isDraggingDockSample && !dockDragFilePath.isEmpty()) {
    if ((e.position - dockDragStartPos).getDistanceFromOrigin() > 3.0f) {
      juce::String path = dockDragFilePath;
      dockDragFilePath.clear();
      isDraggingDockSample = false;
      isClickConsumedByUI = false;

      juce::File f(path);
      if (f.existsAsFile()) {
        juce::StringArray files;
        files.add(f.getFullPathName());
        juce::DragAndDropContainer::performExternalDragDropOfFiles(files, false);
      }
      return;
    }
  }

  // Deadzone / UI guard: if the click started in the dock or chevron deadzone,
  // absolutely NO map panning or rotating is permitted!
  if (isClickConsumedByUI) {
    return;
  }

  if (e.mods.isRightButtonDown() || e.mods.isPopupMenu())
    return;

  auto delta = e.position - mouseDragStartPos;

  // Deadzone threshold: only start panning / rotating when dragged more than 4 pixels!
  if (!isPanning && !isRotating) {
    if (delta.getDistanceFromOrigin() < 4.0f) {
      return; // Click within deadzone: do not disrupt smooth camera transition!
    }
    if (e.mods.isShiftDown() || is2DMode) {
      isPanning = true;
      dragStartCenter = cameraCenterPos;
    } else {
      isRotating = true;
      dragStartRotX = targetRotX;
      dragStartRotY = targetRotY;
    }
  }

  if (is2DMode && isRotating) {
    isRotating = false;
    isPanning = true;
    dragStartCenter = cameraCenterPos;
  }

  if (isPanning) {
    // Keep selection active while panning
    float dx = (delta.x * 2.0f / zoomScale);
    float dy = (delta.y * 2.0f / zoomScale);

    if (is2DMode) {
      targetCameraCenterPos.x = juce::jlimit(-maxMapPanX, maxMapPanX, dragStartCenter.x + dx);
      targetCameraCenterPos.y = juce::jlimit(-maxMapPanY, maxMapPanY, dragStartCenter.y - dy);
      targetCameraCenterPos.z = 0.0f;
      cameraCenterPos = targetCameraCenterPos;
    } else {
      float crx = std::cos(targetRotX), srx = std::sin(targetRotX);
      float cry = std::cos(targetRotY), sry = std::sin(targetRotY);
      targetCameraCenterPos.x = juce::jlimit(-maxMapPanX, maxMapPanX, dragStartCenter.x + (cry * dx - srx * sry * dy));
      targetCameraCenterPos.y = juce::jlimit(-maxMapPanY, maxMapPanY, dragStartCenter.y - (crx * dy));
      targetCameraCenterPos.z = juce::jlimit(-maxMapPanZ, maxMapPanZ, dragStartCenter.z + (sry * dx + srx * cry * dy));
      cameraCenterPos = targetCameraCenterPos;
    }
  } else if (isRotating) {
    targetRotY = dragStartRotY - (delta.x * 0.01f);
    targetRotX = juce::jlimit(-juce::MathConstants<float>::halfPi,
                              juce::MathConstants<float>::halfPi,
                              dragStartRotX - (delta.y * 0.01f));
  }
  overlayComponent.repaint();
  repaint();
}

void SampleCloudComponent::mouseUp(const juce::MouseEvent &e) {
  isClickConsumedByUI = false;
  if (isDraggingDockSample) {
    isDraggingDockSample = false;
    // Released without dragging: user clicked to audition / select
    if ((e.position - dockDragStartPos).getDistanceFromOrigin() <= 4.0f) {
      if (dockDragNodeIndex == selectedNodeIndex) {
        audioEngine.auditionFile(juce::File(nodes[selectedNodeIndex].item.filePath));
      } else if (dockDragNodeIndex >= 0 && dockDragNodeIndex < static_cast<int>(nodes.size())) {
        selectedNodeIndex = dockDragNodeIndex;
        targetCameraCenterPos = nodes[selectedNodeIndex].targetPos;
        audioEngine.auditionFile(juce::File(nodes[selectedNodeIndex].item.filePath));
        updateSimilarStack();
        listeners.call([&](SampleCloudListener &l) {
          l.cloudSampleSelected(nodes[selectedNodeIndex].item);
        });
      }
      overlayComponent.repaint();
      repaint();
    }
    dockDragFilePath.clear();
    dockDragNodeIndex = -1;
  }

  isRotating = false;
  isPanning = false;
}

void SampleCloudComponent::mouseMove(const juce::MouseEvent &e) {
  // Check hover over dock components
  bool onDock = false;
  hoveredSimilarIndex = -100;
  isHoveringPlayButton = false;
  isHoveringUpArrow = false;
  isHoveringDownArrow = false;
  isHoveringExpandChevron = false;
  isHoveringFavoriteStar = false;
  isHoveringMainPill = false;

  if (hoverPlayButton.getBounds().toFloat().expanded(3.0f).contains(e.position)) {
    if (hoveredNodeIndex >= 0) {
      hoveredNodeIndex = -1;
      targetHoverScale = 1.0f;
    }
    overlayComponent.setMouseCursor(juce::MouseCursor::NormalCursor);
    overlayComponent.repaint();
    return;
  }

  auto deadzone = getDockDeadzone();
  if (deadzone.contains(e.position)) {
    onDock = true;
    if (selectedNodeIndex >= 0 && selectedNodeIndex < static_cast<int>(nodes.size())) {
      const juce::Rectangle<float> upHitBounds(
          upArrowBounds.getX() - 4.0f, upArrowBounds.getY() - 3.0f,
          upArrowBounds.getWidth() + 8.0f, upArrowBounds.getHeight() + 4.0f);
      const juce::Rectangle<float> downHitBounds(
          downArrowBounds.getX() - 4.0f, downArrowBounds.getY() - 1.0f,
          downArrowBounds.getWidth() + 8.0f, downArrowBounds.getHeight() + 4.0f);

      if (expandChevronBounds.expanded(8.0f).contains(e.position)) {
        isHoveringExpandChevron = true;
        overlayComponent.setMouseCursor(juce::MouseCursor::PointingHandCursor);
      } else if (playButtonBounds.expanded(3.0f).contains(e.position)) {
        isHoveringPlayButton = true;
        overlayComponent.setMouseCursor(juce::MouseCursor::PointingHandCursor);
      } else if (upHitBounds.contains(e.position)) {
        isHoveringUpArrow = true;
        overlayComponent.setMouseCursor(juce::MouseCursor::PointingHandCursor);
      } else if (downHitBounds.contains(e.position)) {
        isHoveringDownArrow = true;
        overlayComponent.setMouseCursor(juce::MouseCursor::PointingHandCursor);
      } else if (favoriteStarBounds.expanded(3.0f).contains(e.position)) {
        isHoveringFavoriteStar = true;
        overlayComponent.setMouseCursor(juce::MouseCursor::PointingHandCursor);
      } else if (mainPillBounds.contains(e.position)) {
        isHoveringMainPill = true;
        hoveredSimilarIndex = 0;
        overlayComponent.setMouseCursor(juce::MouseCursor::DraggingHandCursor);
      } else if (showSimilarStack) {
        bool found = false;
        for (size_t i = 0; i < similarAbove.size(); ++i) {
          if (similarAbove[i].bounds.contains(e.position)) {
            hoveredSimilarIndex = -static_cast<int>(i + 1);
            overlayComponent.setMouseCursor(juce::MouseCursor::DraggingHandCursor);
            found = true;
            break;
          }
        }
        if (!found) {
          for (size_t i = 0; i < similarBelow.size(); ++i) {
            if (similarBelow[i].bounds.contains(e.position)) {
              hoveredSimilarIndex = static_cast<int>(i + 1);
              overlayComponent.setMouseCursor(juce::MouseCursor::DraggingHandCursor);
              found = true;
              break;
            }
          }
        }
        if (!found) {
          overlayComponent.setMouseCursor(juce::MouseCursor::NormalCursor);
        }
      } else {
        overlayComponent.setMouseCursor(juce::MouseCursor::NormalCursor);
      }
    } else {
      overlayComponent.setMouseCursor(juce::MouseCursor::NormalCursor);
    }
  }

  if (onDock) {
    if (hoveredNodeIndex >= 0) {
      hoveredNodeIndex = -1;
      targetHoverScale = 1.0f;
    }
    overlayComponent.repaint();
    return;
  }

  const int bestIndex = findHitNodeAt(e.position);

  if (bestIndex != hoveredNodeIndex) {
    const int prevHover = hoveredNodeIndex;
    hoveredNodeIndex = bestIndex;
    if (hoveredNodeIndex >= 0) {
      if (prevHover >= 0)
        currentHoverScale = 1.08f; // Smooth re-ease when sliding directly onto a close neighbor
      animatedHoverNodeIndex = hoveredNodeIndex;
      targetHoverScale = 1.62f; // Balanced +62% semi-transparent hover growth
      overlayComponent.setMouseCursor(juce::MouseCursor::PointingHandCursor);

      if (hoverPlayEnabled) {
        audioEngine.auditionHover(juce::File(nodes[hoveredNodeIndex].item.filePath));
      }
    } else {
      targetHoverScale = 1.0f;
      overlayComponent.setMouseCursor(juce::MouseCursor::NormalCursor);
    }
    overlayComponent.repaint();
  }
}

int SampleCloudComponent::findHitNodeAt(juce::Point<float> pos) const {
  const float effW = getEffectiveWidth();
  const float effH = getEffectiveHeight();
  const float halfW = effW * 0.5f;
  const float halfH = effH * 0.5f;
  if (nodes.empty() || halfW <= 1.0f || halfH <= 1.0f)
    return -1;

  // Compute exact instantaneous ViewProjection matrix locally (eliminates cross-thread race with renderOpenGL)
  const float aspect = effW / effH;
  const auto projectionMatrix = juce::Matrix3D<float>::fromFrustum(
      -aspect * 0.1f, aspect * 0.1f, -0.1f, 0.1f, 0.2f, 1000000.0f);

  juce::Matrix3D<float> viewMatrix;
  viewMatrix = viewMatrix *
               juce::Matrix3D<float>::fromTranslation(
                   {0.0f, 0.0f, -cameraDistance / zoomScale});

  const float crx = std::cos(rotX), srx = std::sin(rotX);
  const float cry = std::cos(rotY), sry = std::sin(rotY);
  const juce::Matrix3D<float> rotMatrix(
      cry, 0.0f, sry, 0.0f,
      srx * sry, crx, -srx * cry, 0.0f,
      -crx * sry, srx, crx * cry, 0.0f,
      0.0f, 0.0f, 0.0f, 1.0f);
  viewMatrix = viewMatrix * rotMatrix;
  viewMatrix = viewMatrix *
               juce::Matrix3D<float>::fromTranslation(
                   {-cameraCenterPos.x, -cameraCenterPos.y, -cameraCenterPos.z});

  const auto vp = projectionMatrix * viewMatrix;

  const float mx = pos.x;
  const float my = pos.y;

#if JUCE_MAC
  const float renderScale = 1.0f;
#else
  float renderScale = 1.0f;
  if (auto *peer = getPeer())
    renderScale = std::max(1.0f, static_cast<float>(peer->getPlatformScaleFactor()));
#endif

  // Balanced proximity tolerance for normal click mode (prevents empty-space misses while keeping exact center-distance precision);
  // generous radius exclusively for Mouseover: On
  const float snapMargin = juce::jlimit(3.0f, 5.4f, 4.6f / std::sqrt(zoomScale));

  const float m00 = vp.mat[0], m04 = vp.mat[4], m08 = vp.mat[8],  m12 = vp.mat[12];
  const float m01 = vp.mat[1], m05 = vp.mat[5], m09 = vp.mat[9],  m13 = vp.mat[13];
  const float m02 = vp.mat[2], m06 = vp.mat[6], m10 = vp.mat[10], m14 = vp.mat[14];
  const float m03 = vp.mat[3], m07 = vp.mat[7], m11 = vp.mat[11], m15 = vp.mat[15];

  int bestIndex = -1;
  float bestScore = std::numeric_limits<float>::max();

  const size_t numNodes = nodes.size();
  for (size_t i = 0; i < numNodes; ++i) {
    const auto &n = nodes[i];
    const float px = n.currentPos.x;
    const float py = n.currentPos.y;
    const float pz = is2DMode ? 0.0f : n.currentPos.z;

    const float w = m03 * px + m07 * py + m11 * pz + m15;
    if (w <= 0.001f)
      continue;

    const float spZ = m02 * px + m06 * py + m10 * pz + m14;
    if (spZ <= 0.0f)
      continue;

    const float effRad = is2DMode ? (n.radius * 1.15f) : n.radius;
    const float pointDiam = juce::jlimit(6.0f, 22.0f, effRad * 1600.0f / std::max(60.0f, w));
    const float renderedRadius = (pointDiam * 0.5f) / renderScale;
    float hitRadius = hoverPlayEnabled
                          ? juce::jlimit(16.0f, 24.0f, renderedRadius * 2.0f + 11.0f)
                          : juce::jlimit(7.5f, 13.5f, renderedRadius * 1.15f + snapMargin);
    if (static_cast<int>(i) == hoveredNodeIndex)
      hitRadius += 1.2f; // Prevent outer-edge tremor / click slip against empty space (-1)

    const float invW = 1.0f / w;
    const float spX = m00 * px + m04 * py + m08 * pz + m12;
    const float sx = halfW + (spX * invW) * halfW;
    const float dx = sx - mx;
    if (std::abs(dx) > hitRadius)
      continue;

    const float spY = m01 * px + m05 * py + m09 * pz + m13;
    const float sy = halfH - (spY * invW) * halfH;
    const float dy = sy - my;
    if (std::abs(dy) > hitRadius)
      continue;

    const float distSq = dx * dx + dy * dy;
    if (distSq <= hitRadius * hitRadius) {
      const float dist = std::sqrt(distSq);
      // Rank primarily by exact 2D screen distance to star center so middle/neighbor samples switch immediately!
      float score = dist + (is2DMode ? 0.0f : (spZ / (spZ + 2000.0f)) * 0.45f);
      if (!hoverPlayEnabled && static_cast<int>(i) == hoveredNodeIndex)
        score -= 0.15f;

      if (score < bestScore) {
        bestScore = score;
        bestIndex = static_cast<int>(i);
      }
    }
  }

  return bestIndex;
}

void SampleCloudComponent::mouseExit(const juce::MouseEvent &e) {
  juce::ignoreUnused(e);
  hoveredNodeIndex = -1;
  targetHoverScale = 1.0f;
  hoveredSimilarIndex = -100;
  isHoveringPlayButton = false;
  isHoveringUpArrow = false;
  isHoveringDownArrow = false;
  isHoveringExpandChevron = false;
  isHoveringFavoriteStar = false;
  isHoveringMainPill = false;
  overlayComponent.setMouseCursor(juce::MouseCursor::NormalCursor);
  overlayComponent.repaint();
#if !JUCE_MAC
  repaint();
#endif
}

void SampleCloudComponent::mouseWheelMove(
    const juce::MouseEvent &e, const juce::MouseWheelDetails &wheel) {
  float effW = getEffectiveWidth();
  float effH = getEffectiveHeight();

  if (e.y >= effH - 55.0f && e.y <= effH - 15.0f && e.x < effW - 280.0f) {
    float totalWidth = 0.0f;
    juce::Font legendFont(12.0f, juce::Font::bold);
    for (const auto &cluster : clusters) {
      juce::String fullTag = getAtlasCategoryName(cluster.tag);
      totalWidth += 24.0f +
                    legendFont.getStringWidthFloat(fullTag) +
                    14.0f + 8.0f;
    }

    float viewWidth = effW - 280.0f - 20.0f;
    float maxScroll = 0.0f;
    if (totalWidth > viewWidth)
      maxScroll = totalWidth - viewWidth;

    legendScrollOffset += wheel.deltaX * 150.0f + wheel.deltaY * 150.0f;
    legendScrollOffset = juce::jlimit(-maxScroll, 0.0f, legendScrollOffset);
    overlayComponent.repaint();
    repaint();
  } else {
#if JUCE_MAC
    // On macOS only: when using trackpad or Magic Mouse, rotate the 3D view around selected node when scrolling horizontally
    const bool isTrackpadOrMagicMouse = (wheel.isSmooth || wheel.isInertial);
    const bool isHorizontalScroll = (std::abs(wheel.deltaX) > 0.0001f) &&
                                    (std::abs(wheel.deltaX) >= std::abs(wheel.deltaY) * 0.35f);

    if (isTrackpadOrMagicMouse && !is2DMode && isHorizontalScroll) {
      // 1. Identify selected node (or hovered node if none currently selected)
      if (selectedNodeIndex >= 0 && selectedNodeIndex < static_cast<int>(nodes.size())) {
        targetCameraCenterPos = nodes[selectedNodeIndex].targetPos;
        cameraCenterPos.x += (targetCameraCenterPos.x - cameraCenterPos.x) * 0.35f;
        cameraCenterPos.y += (targetCameraCenterPos.y - cameraCenterPos.y) * 0.35f;
        cameraCenterPos.z += (targetCameraCenterPos.z - cameraCenterPos.z) * 0.35f;
      } else if (hoveredNodeIndex >= 0 && hoveredNodeIndex < static_cast<int>(nodes.size())) {
        selectedNodeIndex = hoveredNodeIndex;
        targetCameraCenterPos = nodes[selectedNodeIndex].targetPos;
        cameraCenterPos.x += (targetCameraCenterPos.x - cameraCenterPos.x) * 0.35f;
        cameraCenterPos.y += (targetCameraCenterPos.y - cameraCenterPos.y) * 0.35f;
        cameraCenterPos.z += (targetCameraCenterPos.z - cameraCenterPos.z) * 0.35f;
      }

      // 2. Rotate 3D view around selected node (azimuth / yaw orbit)
      const float rotSensitivity = 2.2f;
      const float rotDelta = wheel.deltaX * rotSensitivity;
      targetRotY -= rotDelta;
      rotY -= rotDelta * 0.4f;

      // 3. If there is also significant vertical movement, zoom simultaneously
      if (std::abs(wheel.deltaY) > std::abs(wheel.deltaX) * 0.6f) {
        targetZoomScale = juce::jlimit(
            0.20f, 3.5f, targetZoomScale * std::pow(1.15f, wheel.deltaY * 3.0f));
      }

      overlayComponent.repaint();
      repaint();
      return;
    }
#endif

    targetZoomScale = juce::jlimit(
        0.20f, 3.5f, targetZoomScale * std::pow(1.15f, wheel.deltaY * 3.0f));
    overlayComponent.repaint();
    repaint();
  }
}

void SampleCloudComponent::mouseDoubleClick(const juce::MouseEvent &e) {
  if (e.mods.isRightButtonDown() || e.mods.isPopupMenu())
    return;

  if (getDockDeadzone().contains(e.position))
    return;

  // Note: JUCE invokes mouseDoubleClick during mouseUp whenever getNumberOfClicks() >= 2.
  // Playback was ALREADY restarted on mouseDown; never re-trigger audio on button release!
  if (selectedNodeIndex >= 0 && selectedNodeIndex < static_cast<int>(nodes.size()) &&
      (hoveredNodeIndex >= 0 || (juce::Time::getMillisecondCounter() - lastMapNodeClickTimeMs) < 600)) {
    listeners.call([&](SampleCloudListener &l) {
      l.cloudSampleDoubleClicked(nodes[selectedNodeIndex].item);
    });
  }
}

void SampleCloudComponent::addListener(SampleCloudListener *listener) {
  listeners.add(listener);
}
void SampleCloudComponent::removeListener(SampleCloudListener *listener) {
  listeners.remove(listener);
}

juce::String SampleCloudComponent::getPrimaryCategoryTag(
    const std::set<juce::String> &tags) const {
  return TagDatabaseManager::getPrimaryCategoryTag(tags);
}

juce::Colour
SampleCloudComponent::getColourForTag(const juce::String &tag) const {
  juce::String cat = TagDatabaseManager::getCanonicalCategory(tag);

  if (cat == "kick")              return juce::Colour(0xffff1493); // Vivid Neon Hot Pink (#FF1493)
  if (cat == "snare" || cat == "clave/rim") return juce::Colour(0xff00d4ff); // Vivid Neon Electric Blue (#00D4FF)
  if (cat == "clap" || cat == "snap")       return juce::Colour(0xffbf00ff); // Neon Electric Violet (#BF00FF)
  if (cat == "hihat" || cat == "hihat closed" || cat == "hihat open")
                                  return juce::Colour(0xffffe600); // Radiant Neon Yellow (#FFE600)
  if (cat == "tom")               return juce::Colour(0xffff6b00); // Neon Vibrant Orange (#FF6B00)
  if (cat == "crash" || cat == "ride" || cat == "cymbal")
                                  return juce::Colour(0xff00ffcc); // Neon Mint Turquoise (#00FFCC)
  if (cat == "percussion" || cat == "tambourine shaker" || cat == "bongo/conga" || cat == "cowbell")
                                  return juce::Colour(0xff39ff14); // Electric Neon Green (#39FF14)
  if (cat == "bass")              return juce::Colour(0xff8a2be2); // Neon Blue-Violet (#8A2BE2)
  if (cat == "keys")              return juce::Colour(0xff00f0ff); // Neon Ice Cyan (#00F0FF)
  if (cat == "guitar")            return juce::Colour(0xff00ffa2); // Neon Seafoam (#00FFA2)
  if (cat == "strings")           return juce::Colour(0xfff72585); // Neon Orchid Magenta (#F72585)
  if (cat == "brass")             return juce::Colour(0xffffaa00); // Neon Amber Gold (#FFAA00)
  if (cat == "woodwinds")         return juce::Colour(0xff06d6a0); // Neon Teal Green (#06D6A0)
  if (cat == "orchestral")        return juce::Colour(0xffff007f); // Neon Rose Magenta (#FF007F)
  if (cat == "vocal")             return juce::Colour(0xffff3366); // Neon Coral Pink (#FF3366)
  if (cat == "fx")                return juce::Colour(0xff00ffff); // Electric Aqua Cyan (#00FFFF)
  if (cat == "synth")             return juce::Colour(0xffd000ff); // Neon Laser Violet (#D000FF)
  if (cat == "loop")              return juce::Colour(0xff2ec4b6); // Neon Emerald Mint (#2EC4B6)
  if (cat == "other" || tag.equalsIgnoreCase("Untagged") || tag.equalsIgnoreCase("#OneShot"))
                                  return juce::Colour(0xffff6584); // Neon Warm Coral (#FF6584)

  // Pure vibrant neon fallback
  uint32_t hash = static_cast<uint32_t>(tag.hashCode());
  float hue = (hash % 360) / 360.0f;
  return juce::Colour::fromHSV(hue, 0.95f, 1.0f, 1.0f);
}

void SampleCloudComponent::showContextMenuForNode(int nodeIndex) {
  if (nodeIndex < 0 || nodeIndex >= static_cast<int>(nodes.size()))
    return;

  const auto item = nodes[static_cast<size_t>(nodeIndex)].item;

  juce::PopupMenu menu;
  menu.addSectionHeader(item.fileName);
  menu.addItem(1, item.isFavorite ? "Remove from Favorites" : "Add to Favorites");

  juce::PopupMenu ratingMenu;
  for (int r = 0; r <= 5; ++r) {
    ratingMenu.addItem(10 + r, (r == 0) ? "No Rating" : juce::String(r) + " Stars", true, r == item.rating);
  }
  menu.addSubMenu("Set Rating", ratingMenu);

  menu.addSeparator();
  menu.addItem(2, "Add Custom Tag...");

  juce::PopupMenu removeTagMenu;
  int tagIdx = 100;
  for (const auto &tag : item.tags) {
    removeTagMenu.addItem(tagIdx++, "Remove " + tag);
  }
  if (!item.tags.empty()) {
    menu.addSubMenu("Remove Tag", removeTagMenu);
  }

  menu.addSeparator();
  menu.addItem(4, "Reveal in Finder");
  menu.addItem(6, "Add to Sample Map");
  menu.addItem(7, "Slice to Sample Map...");
  menu.addItem(8, "Edit Sample in Editor");

  menu.showMenuAsync(
      juce::PopupMenu::Options().withMousePosition(),
      [this, item, nodeIndex](int result) {
        if (result == 1) {
          if (nodeIndex < static_cast<int>(nodes.size())) {
            nodes[static_cast<size_t>(nodeIndex)].item.isFavorite = !item.isFavorite;
          }
          dbManager.toggleFavorite(item.id);
          overlayComponent.repaint();
          repaint();
        } else if (result >= 10 && result <= 15) {
          int newRating = result - 10;
          if (nodeIndex < static_cast<int>(nodes.size())) {
            nodes[static_cast<size_t>(nodeIndex)].item.rating = newRating;
          }
          dbManager.setRating(item.id, newRating);
          overlayComponent.repaint();
          repaint();
        } else if (result == 2) {
          auto alert = std::make_shared<juce::AlertWindow>(
              "Add Custom Tag", "Enter a new custom tag for " + item.fileName + ":",
              juce::AlertWindow::QuestionIcon);
          alert->addTextEditor("tagInput", "", "Tag (e.g. Kick, #Sub, Vocal)");
          if (auto *ed = alert->getTextEditor("tagInput")) {
            ed->setJustification(juce::Justification::centredLeft);
            ed->setIndents(4, 0);
          }
          alert->addButton("Add Tag", 1, juce::KeyPress(juce::KeyPress::returnKey));
          alert->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

          alert->enterModalState(
              true, juce::ModalCallbackFunction::create([this, alert, item](int btnResult) {
                if (btnResult == 1) {
                  auto newTag = alert->getTextEditorContents("tagInput").trim();
                  if (newTag.isNotEmpty()) {
                    dbManager.addTagToItem(item.id, newTag);
                  }
                }
              }));
        } else if (result >= 100) {
          int idx = result - 100;
          if (idx >= 0 && idx < static_cast<int>(item.tags.size())) {
            auto it = item.tags.begin();
            std::advance(it, idx);
            dbManager.removeTagFromItem(item.id, *it);
          }
        } else if (result == 4) {
          juce::File f(item.filePath);
          if (f.existsAsFile()) {
            f.revealToUser();
          }
        } else if (result == 6) {
          listeners.call([item](SampleCloudListener &l) {
            l.addToSampleMapRequested(item);
          });
        } else if (result == 7) {
          listeners.call([item](SampleCloudListener &l) {
            l.autoSliceToSamplerRequested(item);
          });
        } else if (result == 8) {
          listeners.call([item](SampleCloudListener &l) {
            l.editSampleRequested(item);
          });
        }
      });
}

// ==============================================================================
// RAW OPENGL RENDERING (WINDOWS / LINUX FALLBACK)
// ==============================================================================

#if !JUCE_MAC

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

void SampleCloudComponent::newOpenGLContextCreated() {
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
      "uniform float isLightMode;\n"
      "uniform int hoveredVertexIndex;\n"
      "uniform float hoverScale;\n"
      "out vec4 destColour;\n"
      "out float isHovered;\n"
      "\n"
      "void main()\n"
      "{\n"
      "    vec4 viewPos = viewMatrix * vec4(position, 1.0);\n"
      "    gl_Position = projectionMatrix * viewPos;\n"
      "    destColour = colour;\n"
      "    if (isLightMode > 0.5) { destColour.rgb *= 0.85; }\n"
      "\n"
      "    float currentRad = radius;\n"
      "    float hovered = 0.0;\n"
      "    if (gl_VertexID == hoveredVertexIndex && hoveredVertexIndex >= 0) {\n"
      "        currentRad *= hoverScale;\n"
      "        destColour.rgb = mix(destColour.rgb, vec3(1.0), 0.25);\n"
      "        hovered = 1.0;\n"
      "    } else {\n"
      "        destColour.a *= revealAlpha;\n"
      "    }\n"
      "    isHovered = hovered;\n"
      "\n"
      "    // Bounded screen-space point size (stays crisp on zoom, never turns into giant blurry blob)\n"
      "    float baseScreenSize = currentRad * 1600.0 / max(60.0, -viewPos.z);\n"
      "    gl_PointSize = clamp(baseScreenSize, 6.0, 22.0);\n"
      "    if (hovered > 0.5) {\n"
      "        gl_PointSize = clamp(baseScreenSize * hoverScale, 8.0, 27.0);\n"
      "    }\n"
      "}\n";

  juce::String fragmentShader =
      "#version 150\n"
      "in vec4 destColour;\n"
      "in float isHovered;\n"
      "out vec4 finalColour;\n"
      "uniform vec3 fogColor;\n"
      "\n"
      "void main()\n"
      "{\n"
      "    vec2 p = gl_PointCoord * 2.0 - vec2(1.0);\n"
      "    float dist = length(p);\n"
      "    if (dist > 1.0) discard;\n"
      "\n"
      "    // Crisp solid circular disc with smooth antialiased perimeter (Atlas 2 style)\n"
      "    float edgeAlpha = smoothstep(1.0, 0.82, dist);\n"
      "\n"
      "    // Solid vibrant neon color\n"
      "    vec3 col = destColour.rgb;\n"
      "\n"
      "    // Very subtle center highlight for physical 3D disc depth (NOT white blowout)\n"
      "    float highlight = smoothstep(0.55, 0.0, dist);\n"
      "    col = mix(col, vec3(1.0), highlight * 0.18);\n"
      "\n"
      "    if (isHovered > 0.5) {\n"
      "        // Subtle luminous rim for hovered point\n"
      "        float rim = smoothstep(0.70, 0.95, dist);\n"
      "        col = mix(col, vec3(1.0), rim * 0.25);\n"
      "    }\n"
      "\n"
      "    finalColour = vec4(col, destColour.a * edgeAlpha);\n"
      "}\n";

  shaderProgram.reset(new juce::OpenGLShaderProgram(openGLContext));
  if (shaderProgram->addVertexShader(vertexShader) &&
      shaderProgram->addFragmentShader(fragmentShader) &&
      shaderProgram->link()) {
    // Success
  }
}

void SampleCloudComponent::renderOpenGL() {
  bool isLightMode = OpenWavLookAndFeel::bgCard.getBrightness() > 0.5f;
  const juce::Colour bgAnthracite = juce::Colour::fromRGB(22, 23, 26);
  juce::OpenGLHelpers::clear(isLightMode ? OpenWavLookAndFeel::bgCard : bgAnthracite);

  if (layoutPending || !shaderProgram || vertexBuffer.empty())
    return;

#if !defined(_WIN32)
  juce::gl::glEnable(GL_BLEND);
  juce::gl::glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#else
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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

  float aspect = (getHeight() > 0) ? (getWidth() / (float)getHeight()) : 1.0f;
  auto projectionMatrix = juce::Matrix3D<float>::fromFrustum(
      -aspect * 0.1f, aspect * 0.1f, -0.1f, 0.1f, 0.2f, 1000000.0f);

  juce::Matrix3D<float> viewMatrix;
  viewMatrix = viewMatrix *
               juce::Matrix3D<float>::fromTranslation(
                   {0.0f, 0.0f, -cameraDistance / zoomScale});

  float crx = std::cos(rotX), srx = std::sin(rotX);
  float cry = std::cos(rotY), sry = std::sin(rotY);
  juce::Matrix3D<float> rotMatrix(cry, 0.0f, sry, 0.0f, srx * sry, crx,
                                  -srx * cry, 0.0f, -crx * sry, srx, crx * cry,
                                  0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
  viewMatrix = viewMatrix * rotMatrix;

  viewMatrix = viewMatrix *
               juce::Matrix3D<float>::fromTranslation(
                   {-cameraCenterPos.x, -cameraCenterPos.y, -cameraCenterPos.z});

  lastViewProjectionMatrix = projectionMatrix * viewMatrix;

  GLuint progID = shaderProgram->getProgramID();
  openGLContext.extensions.glUniformMatrix4fv(
      openGLContext.extensions.glGetUniformLocation(progID, "projectionMatrix"),
      1, GL_FALSE, projectionMatrix.mat);
  openGLContext.extensions.glUniformMatrix4fv(
      openGLContext.extensions.glGetUniformLocation(progID, "viewMatrix"), 1,
      GL_FALSE, viewMatrix.mat);
  openGLContext.extensions.glUniform1f(
      openGLContext.extensions.glGetUniformLocation(progID, "revealAlpha"),
      revealAlpha);
  openGLContext.extensions.glUniform1f(
      openGLContext.extensions.glGetUniformLocation(progID, "time"),
      static_cast<float>(juce::Time::getMillisecondCounterHiRes() / 1000.0));
  openGLContext.extensions.glUniform1f(
      openGLContext.extensions.glGetUniformLocation(progID, "isLightMode"),
      isLightMode ? 1.0f : 0.0f);

  int hoveredVtxIdx = -1;
  if (animatedHoverNodeIndex >= 0 && animatedHoverNodeIndex < static_cast<int>(nodes.size())) {
    hoveredVtxIdx = animatedHoverNodeIndex;
  }
  openGLContext.extensions.glUniform1i(
      openGLContext.extensions.glGetUniformLocation(progID, "hoveredVertexIndex"),
      hoveredVtxIdx);
  openGLContext.extensions.glUniform1f(
      openGLContext.extensions.glGetUniformLocation(progID, "hoverScale"),
      currentHoverScale);

  auto activeBg = isLightMode ? OpenWavLookAndFeel::bgCard : bgAnthracite;
  openGLContext.extensions.glUniform3f(
      openGLContext.extensions.glGetUniformLocation(progID, "fogColor"),
      activeBg.getFloatRed(), activeBg.getFloatGreen(), activeBg.getFloatBlue());

  openGLContext.extensions.glBindBuffer(GL_ARRAY_BUFFER, vbo);

  if (vboNeedsUpdate) {
    openGLContext.extensions.glBufferData(GL_ARRAY_BUFFER,
                                          vertexBuffer.size() * sizeof(Vertex),
                                          vertexBuffer.data(), GL_STATIC_DRAW);
    vboNeedsUpdate = false;
  }

  GLint posAttr =
      openGLContext.extensions.glGetAttribLocation(progID, "position");
  GLint colAttr =
      openGLContext.extensions.glGetAttribLocation(progID, "colour");
  GLint radAttr =
      openGLContext.extensions.glGetAttribLocation(progID, "radius");

  if (posAttr >= 0) {
    openGLContext.extensions.glVertexAttribPointer(
        posAttr, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
        (GLvoid *)offsetof(Vertex, x));
    openGLContext.extensions.glEnableVertexAttribArray(posAttr);
  }
  if (colAttr >= 0) {
    openGLContext.extensions.glVertexAttribPointer(
        colAttr, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
        (GLvoid *)offsetof(Vertex, r));
    openGLContext.extensions.glEnableVertexAttribArray(colAttr);
  }
  if (radAttr >= 0) {
    openGLContext.extensions.glVertexAttribPointer(
        radAttr, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex),
        (GLvoid *)offsetof(Vertex, radius));
    openGLContext.extensions.glEnableVertexAttribArray(radAttr);
  }
#if !defined(_WIN32)
  juce::gl::glDrawArrays(GL_POINTS, 0, (GLsizei)vertexBuffer.size());
#else
  glDrawArrays(GL_POINTS, 0, (GLsizei)vertexBuffer.size());
#endif
  if (posAttr >= 0)
    openGLContext.extensions.glDisableVertexAttribArray(posAttr);
  if (colAttr >= 0)
    openGLContext.extensions.glDisableVertexAttribArray(colAttr);
  if (radAttr >= 0)
    openGLContext.extensions.glDisableVertexAttribArray(radAttr);
}

void SampleCloudComponent::openGLContextClosing() {
  openGLContext.extensions.glDeleteBuffers(1, &vbo);
  shaderProgram.reset();
}

#endif // !JUCE_MAC

} // namespace openwav
