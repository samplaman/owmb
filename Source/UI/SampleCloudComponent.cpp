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

static juce::String getFullCategoryName(const juce::String& tag) {
    auto t = tag.toLowerCase();
    if (t.contains("subkick")) return "Sub Kick";
    if (t.contains("kick")) return "Kick Drum";
    if (t.contains("snare")) return "Snare Drum";
    if (t.contains("rimshot") || t.contains("rim")) return "Rimshot";
    if (t.contains("clap")) return "Clap";
    if (t.contains("snap")) return "Snap";
    if (t.contains("openhat") || t.contains("open_hat")) return "Open Hi-Hat";
    if (t.contains("closedhat") || t.contains("closed_hat")) return "Closed Hi-Hat";
    if (t.contains("hat") || t.contains("hihat")) return "Hi-Hat";
    if (t.contains("tom")) return "Tom Drum";
    if (t.contains("crash")) return "Crash Cymbal";
    if (t.contains("ride")) return "Ride Cymbal";
    if (t.contains("cymbal")) return "Cymbal";
    if (t.contains("shaker")) return "Shaker";
    if (t.contains("tamb")) return "Tambourine";
    if (t.contains("cowbell")) return "Cowbell";
    if (t.contains("conga") || t.contains("bongo")) return "Latin Percussion";
    if (t.contains("perc")) return "Percussion";
    if (t.contains("subbass") || t.contains("sub_bass")) return "Sub Bass";
    if (t.contains("synthbass") || t.contains("synth_bass")) return "Synth Bass";
    if (t.contains("reesebass") || t.contains("reese")) return "Reese Bass";
    if (t.contains("808bass") || t.contains("808")) return "808 Bass";
    if (t.contains("bass")) return "Bass";
    if (t.contains("piano") || t.contains("rhodes") || t.contains("organ") || t.contains("keys")) return "Keys & Piano";
    if (t.contains("guitar") || t.contains("gtr")) return "Guitar";
    if (t.contains("violin") || t.contains("viola") || t.contains("cello") || t.contains("contrabass") || t.contains("string")) return "Strings";
    if (t.contains("frenchhorn") || t.contains("trumpet") || t.contains("trombone") || t.contains("tuba") || t.contains("brass")) return "Brass";
    if (t.contains("flute") || t.contains("piccolo") || t.contains("oboe") || t.contains("clarinet") || t.contains("bassoon") || t.contains("woodwind")) return "Woodwinds";
    if (t.contains("timpani") || t.contains("glock") || t.contains("xylophone") || t.contains("marimba") || t.contains("harp")) return "Orchestral Percussion";
    if (t.contains("choir") || t.contains("chorus")) return "Choir";
    if (t.contains("sax")) return "Saxophone";
    if (t.contains("lead")) return "Lead Synth";
    if (t.contains("pad")) return "Synth Pad";
    if (t.contains("pluck")) return "Synth Pluck";
    if (t.contains("arp")) return "Arpeggio";
    if (t.contains("synth")) return "Synthesizer";
    if (t.contains("acapella")) return "Acapella";
    if (t.contains("vocalchop") || t.contains("voxchop")) return "Vocal Chop";
    if (t.contains("vocal") || t.contains("vox") || t.contains("chant") || t.contains("speech")) return "Vocal";
    if (t.contains("riser") || t.contains("uplifter")) return "Riser FX";
    if (t.contains("downlifter") || t.contains("faller")) return "Downlifter FX";
    if (t.contains("subdrop")) return "Sub Drop FX";
    if (t.contains("impact")) return "Impact FX";
    if (t.contains("sweep") || t.contains("whoosh")) return "Sweep FX";
    if (t.contains("foley")) return "Foley";
    if (t.contains("vinyl")) return "Vinyl & Texture";
    if (t.contains("atmos")) return "Atmosphere";
    if (t.contains("glitch")) return "Glitch FX";
    if (t.contains("fx") || t.contains("sfx")) return "Sound FX";
    if (t.contains("drumloop") || t.contains("drum_loop")) return "Drum Loop";
    if (t.contains("toploop") || t.contains("top_loop")) return "Top Loop";
    if (t.contains("percloop")) return "Percussion Loop";
    if (t.contains("bassloop")) return "Bass Loop";
    if (t.contains("melodicloop")) return "Melodic Loop";
    if (t.contains("vocalloop")) return "Vocal Loop";
    if (t.contains("loop")) return "Loop";
    if (t.contains("oneshot") || t.contains("one_shot")) return "One-Shot";
    return tag.startsWith("#") ? tag.substring(1) : tag;
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
}

void SampleCloudComponent::CloudOverlayComponent::paint(juce::Graphics &g) {
  owner.paintOverlay(g);
}

void SampleCloudComponent::CloudOverlayComponent::resized() {
  auto b = getLocalBounds();
  owner.zoomInButton.setBounds(b.getRight() - 50, b.getBottom() - 140, 40, 40);
  owner.zoomOutButton.setBounds(b.getRight() - 50, b.getBottom() - 90, 40, 40);
  owner.resetZoomButton.setBounds(b.getRight() - 120, b.getBottom() - 40, 110, 30);
  owner.viewModeButton.setBounds(b.getRight() - 170, b.getBottom() - 40, 40, 30);
  owner.autoRotateButton.setBounds(b.getRight() - 280, b.getBottom() - 40, 100, 30);
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
    targetZoomScale = juce::jlimit(0.05f, 10.0f, targetZoomScale * 1.25f);
  };
  zoomOutButton.onClick = [this] {
    targetZoomScale = juce::jlimit(0.05f, 10.0f, targetZoomScale * 0.8f);
  };
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

  autoRotateButton.onClick = [this] {
    autoRotate = !autoRotate;
    autoRotateButton.setButtonText(autoRotate ? "Stop Spin" : "Spin");
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
#if JUCE_MAC
    if (metalView)
      metalView->updateVertices({});
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

      comp->vertexBuffer.clear();
      comp->vertexBuffer.reserve(comp->nodes.size() + comp->clusters.size());

      // Render a large glowing planet at the center of each cluster for the label
      for (const auto &c : comp->clusters) {
        comp->vertexBuffer.push_back({
            c.centerPos.x, c.centerPos.y, c.centerPos.z, c.colour.getFloatRed(),
            c.colour.getFloatGreen(), c.colour.getFloatBlue(),
            c.colour.getFloatAlpha() * 0.85f,
            38.0f // Massive planet radius
        });
      }

      for (const auto &n : comp->nodes) {
        comp->vertexBuffer.push_back(
            {n.targetPos.x, n.targetPos.y, n.targetPos.z,
             n.colour.getFloatRed(), n.colour.getFloatGreen(),
             n.colour.getFloatBlue(), n.colour.getFloatAlpha(), n.radius});
      }

#if JUCE_MAC
      if (comp->metalView) {
        std::vector<MetalVertex> mVerts;
        mVerts.reserve(comp->vertexBuffer.size());
        for (const auto& v : comp->vertexBuffer) {
          mVerts.push_back({v.x, v.y, v.z, v.r, v.g, v.b, v.a, v.radius});
        }
        comp->metalView->updateVertices(mVerts);
      }
#else
      comp->vboNeedsUpdate = true;
#endif

      comp->layoutPending = false;
      comp->revealAlpha = 0.0f;
      comp->overlayComponent.repaint();
      comp->repaint();
    }
  });
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
  if (tagCount > 0) {
    float goldenRatio = (1.0f + std::sqrt(5.0f)) / 2.0f;
    float baseRadius =
        std::max(350.0f, std::sqrt(static_cast<float>(tagCount)) * 140.0f);

    for (size_t i = 0; i < tagCount; ++i) {
      float theta = 2.0f * juce::MathConstants<float>::pi * i / goldenRatio;
      float phi = std::acos(1.0f - 2.0f * (i + 0.5f) / tagCount);

      outClusters[i].centerPos = {
          baseRadius * std::sin(phi) * std::cos(theta),
          baseRadius * std::cos(phi) * 0.75f,
          baseRadius * std::sin(phi) * std::sin(theta)};
    }

    for (int iter = 0; iter < 16; ++iter) {
      if (!layoutActive.load()) return {outClusters, {}};
      for (size_t i = 0; i < tagCount; ++i) {
        for (size_t j = i + 1; j < tagCount; ++j) {
          auto delta = outClusters[j].centerPos - outClusters[i].centerPos;
          float dist = std::sqrt(delta.x * delta.x + delta.y * delta.y +
                                 delta.z * delta.z);
          float r1 = std::max(65.0f, 40.0f + std::sqrt(static_cast<float>(
                                                 outClusters[i].count)) *
                                                 18.0f);
          float r2 = std::max(65.0f, 40.0f + std::sqrt(static_cast<float>(
                                                 outClusters[j].count)) *
                                                 18.0f);
          float minDist = r1 + r2 + 90.0f;

          if (dist < minDist && dist > 0.01f) {
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
  for (const auto &cl : outClusters) {
    tagCenters[cl.tag] = cl.centerPos;
    tagTotalCounts[cl.tag] = cl.count;
  }

  std::unordered_map<juce::String, int> tagItemCounts;
  for (auto &node : nodesCopy) {
    if (!layoutActive.load()) return {outClusters, {}};
    auto cPos = tagCenters[node.primaryTag];
    int countIdx = tagItemCounts[node.primaryTag]++;
    int totalInTag = tagTotalCounts[node.primaryTag];

    uint32_t nodeHash = static_cast<uint32_t>(node.item.filePath.hashCode() ^
                                               (countIdx * 2654435761u));

    float phi = 2.39996323f;
    float uNorm = static_cast<float>(countIdx + 0.5f) /
                  std::max(1.0f, static_cast<float>(totalInTag));
    float yDir = 1.0f - 2.0f * uNorm;
    float radiusAtY = std::sqrt(std::max(0.05f, 1.0f - yDir * yDir));

    float maxCloudRadius = std::max(
        120.0f, 60.0f + std::sqrt(static_cast<float>(totalInTag)) * 30.0f);
    float scatterFactor =
        std::pow(static_cast<float>(nodeHash % 1000) / 1000.0f, 0.45f);
    float rDist = maxCloudRadius * (0.25f + 0.75f * scatterFactor);

    float theta = countIdx * phi + (nodeHash % 100) * 0.05f;
    float jitterX = (static_cast<float>((nodeHash ^ 0xa5a5a5a5) % 100) - 50.0f) * 0.8f;
    float jitterY = (static_cast<float>((nodeHash ^ 0x5a5a5a5a) % 100) - 50.0f) * 0.8f;
    float jitterZ = (static_cast<float>((nodeHash ^ 0x3c3c3c3c) % 100) - 50.0f) * 0.8f;

    float xDir = radiusAtY * std::cos(theta);
    float zDir = radiusAtY * std::sin(theta);

    node.targetPos = {cPos.x + rDist * xDir + jitterX,
                      cPos.y + rDist * yDir + jitterY,
                      cPos.z + rDist * zDir + jitterZ};
    node.currentPos = node.targetPos;
  }

  std::vector<std::pair<size_t, size_t>> outEdges;
  return {outClusters, outEdges};
}

void SampleCloudComponent::selectItemById(const juce::String &itemId) {
  if (itemId.isEmpty())
    return;
  for (size_t i = 0; i < nodes.size(); ++i) {
    if (nodes[i].item.id == itemId) {
      selectedNodeIndex = static_cast<int>(i);
      targetCameraCenterPos = nodes[i].targetPos;
      overlayComponent.repaint();
      repaint();
      break;
    }
  }
}

void SampleCloudComponent::paint(juce::Graphics &g) {
#if !JUCE_MAC
  paintOverlay(g);
#endif
}

void SampleCloudComponent::paintOverlay(juce::Graphics &g) {
  if (layoutPending) {
    g.setColour(juce::Colours::white);
    g.setFont(18.0f);
    g.drawText("Layout Computing...", getLocalBounds(),
               juce::Justification::centred);
    return;
  }

  // Draw clean high-contrast vignette (matches OpenGL look exactly)
  bool isLightMode = OpenWavLookAndFeel::bgCard.getBrightness() > 0.5f;
  float w = (float)getWidth();
  float h = (float)getHeight();
  juce::Colour centerCol = isLightMode ? juce::Colours::transparentWhite
                                       : juce::Colours::transparentBlack;
  juce::Colour edgeCol = isLightMode
                             ? OpenWavLookAndFeel::bgDark.withAlpha(0.85f)
                             : juce::Colours::black.withAlpha(0.85f);
  juce::ColourGradient vignette(centerCol, w * 0.5f, h * 0.5f, edgeCol, 0.0f,
                                0.0f, true);
  g.setGradientFill(vignette);
  g.fillRect(getLocalBounds());

  // Draw 2D ring highlight over 3D point cloud surface
  if (hoveredNodeIndex >= 0 || selectedNodeIndex >= 0) {
    if (lastViewProjectionMatrix.mat[15] != 0.0f) {
      float halfW = getWidth() * 0.5f;
      float halfH = getHeight() * 0.5f;
      for (int i : {hoveredNodeIndex, selectedNodeIndex}) {
        if (i >= 0 && i < (int)nodes.size()) {
          auto &n = nodes[i];
          juce::Vector3D<float> p(n.currentPos.x, n.currentPos.y,
                                  n.currentPos.z);
          float w = lastViewProjectionMatrix.mat[3] * p.x +
                    lastViewProjectionMatrix.mat[7] * p.y +
                    lastViewProjectionMatrix.mat[11] * p.z +
                    lastViewProjectionMatrix.mat[15];
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
          if (spZ > 0.0f && w > 0.0f) {
            float sx = halfW + (spX / w) * halfW;
            float sy = halfH - (spY / w) * halfH;
            float r = std::max(2.2f, n.radius * 2.0f);
            g.setColour(i == hoveredNodeIndex ? juce::Colours::white
                                              : OpenWavLookAndFeel::accentCyan);
            g.drawEllipse(sx - r - 2.0f, sy - r - 2.0f, (r + 2.0f) * 2.0f,
                          (r + 2.0f) * 2.0f, 2.0f);
            
            if (i == hoveredNodeIndex) {
              juce::Font tooltipFont(14.0f);
              g.setFont(tooltipFont);
              juce::String name = n.item.fileName;
              juce::String details = n.item.cachedFormattedDuration + " | " + n.item.cachedFormattedSampleRate;
              
              float nameW = tooltipFont.getStringWidthFloat(name);
              float detW = tooltipFont.getStringWidthFloat(details);
              float tooltipW = std::max(nameW, detW) + 20.0f;
              float tooltipH = 50.0f;
              
              float tx = sx + r + 10.0f;
              float ty = sy - tooltipH * 0.5f;
              
              if (tx + tooltipW > getWidth() - 10.0f) {
                  tx = sx - r - 10.0f - tooltipW;
              }
              if (ty < 10.0f) ty = 10.0f;
              if (ty + tooltipH > getHeight() - 10.0f) ty = getHeight() - 10.0f - tooltipH;

              g.setColour(OpenWavLookAndFeel::bgCard.withAlpha(0.9f));
              g.fillRoundedRectangle(tx, ty, tooltipW, tooltipH, 6.0f);
              g.setColour(OpenWavLookAndFeel::bgDark);
              g.drawRoundedRectangle(tx, ty, tooltipW, tooltipH, 6.0f, 1.5f);

              g.setColour(OpenWavLookAndFeel::textPrimary);
              g.drawText(name, tx + 10.0f, ty + 5.0f, tooltipW - 20.0f, 20.0f, juce::Justification::centredLeft);
              g.setColour(OpenWavLookAndFeel::textSecondary);
              g.drawText(details, tx + 10.0f, ty + 25.0f, tooltipW - 20.0f, 20.0f, juce::Justification::centredLeft);
            }
          }
        }
      }
    }
  }

  if (!clusters.empty()) {
    juce::Font legendFont(13.0f, juce::Font::bold);
    g.setFont(legendFont);

    float padding = 20.0f;
    float circleSize = 12.0f;

    float startY = getHeight() - 30.0f;
    float x = 20.0f + legendScrollOffset;

    g.saveState();
    g.reduceClipRegion(0, getHeight() - 40, getWidth() - 320, 40);

    for (const auto &cluster : clusters) {
      juce::String fullTag = getFullCategoryName(cluster.tag).toUpperCase();
      float textW = legendFont.getStringWidthFloat(fullTag);

      g.setColour(cluster.colour.withAlpha(revealAlpha));
      g.fillEllipse(x, startY + 10.0f - circleSize * 0.5f, circleSize,
                    circleSize);

      g.setColour(
          OpenWavLookAndFeel::textPrimary.withAlpha(0.8f * revealAlpha));
      g.drawText(fullTag, x + circleSize + 6.0f, startY,
                 textW, 20.0f, juce::Justification::centredLeft);

      x += circleSize + 6.0f + textW + padding;
    }

    g.restoreState();
  }
}

void SampleCloudComponent::timerCallback() {
  bool needsRepaint = false;

  if (revealAlpha < 1.0f && !layoutPending) {
    revealAlpha = std::min(1.0f, revealAlpha + 0.03f);
    needsRepaint = true;
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

  if (selectedNodeIndex >= 0 && selectedNodeIndex < (int)nodes.size()) {
    targetCameraCenterPos = nodes[selectedNodeIndex].targetPos;
  }

  if (std::abs(targetCameraCenterPos.x - cameraCenterPos.x) > 0.1f ||
      std::abs(targetCameraCenterPos.y - cameraCenterPos.y) > 0.1f ||
      std::abs(targetCameraCenterPos.z - cameraCenterPos.z) > 0.1f) {
    cameraCenterPos.x += (targetCameraCenterPos.x - cameraCenterPos.x) * 0.15f;
    cameraCenterPos.y += (targetCameraCenterPos.y - cameraCenterPos.y) * 0.15f;
    cameraCenterPos.z += (targetCameraCenterPos.z - cameraCenterPos.z) * 0.15f;
    needsRepaint = true;
  }

  if (!isPanning && !isRotating && !is2DMode && autoRotate) {
    targetRotY -= 0.0005f;
    needsRepaint = true;
  }

  pulsePhase += 0.05f;

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
    auto bgCard = OpenWavLookAndFeel::bgCard;
    u.fogColor[0] = bgCard.getFloatRed();
    u.fogColor[1] = bgCard.getFloatGreen();
    u.fogColor[2] = bgCard.getFloatBlue();
    u.viewportWidth = (float)getWidth();
    u.viewportHeight = (float)getHeight();

    metalView->setClearColor(bgCard.getFloatRed(), bgCard.getFloatGreen(), bgCard.getFloatBlue(), 1.0f);
    metalView->updateUniforms(u);
    metalView->renderNow();
  }
#endif

  if (needsRepaint) {
    overlayComponent.repaint();
    repaint();
  }
}

void SampleCloudComponent::resetZoomAndPan() {
  selectedNodeIndex = -1;
  targetZoomScale = 1.0f;
  zoomScale = 1.0f;
  targetCameraCenterPos = {0.0f, 0.0f, 0.0f};
  cameraCenterPos = {0.0f, 0.0f, 0.0f};
  targetRotX = 0.35f;
  targetRotY = 0.45f;
  is2DMode = false;
  viewModeButton.setButtonText("2D");
  overlayComponent.repaint();
  repaint();
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
  if (e.mods.isRightButtonDown() || e.mods.isPopupMenu()) {
    if (hoveredNodeIndex >= 0)
      showContextMenuForNode(hoveredNodeIndex);
    return;
  }

  mouseDragStartPos = e.position;
  isRotating = false;
  isPanning = false;

  if (e.mods.isShiftDown()) {
    isPanning = true;
    dragStartCenter = cameraCenterPos;
  } else {
    isRotating = true;
    dragStartRotX = targetRotX;
    dragStartRotY = targetRotY;
  }
}

void SampleCloudComponent::mouseDrag(const juce::MouseEvent &e) {
  if (e.mods.isRightButtonDown() || e.mods.isPopupMenu())
    return;

  if (is2DMode && isRotating) {
    isRotating = false;
    isPanning = true;
    dragStartCenter = cameraCenterPos;
  }

  auto delta = e.position - mouseDragStartPos;
  if (isPanning) {
    selectedNodeIndex = -1;
    float crx = std::cos(targetRotX), srx = std::sin(targetRotX);
    float cry = std::cos(targetRotY), sry = std::sin(targetRotY);
    float dx = (delta.x * 2.0f / zoomScale);
    float dy = (delta.y * 2.0f / zoomScale);

    targetCameraCenterPos.x = dragStartCenter.x - (cry * dx + srx * sry * dy);
    targetCameraCenterPos.y = dragStartCenter.y - (crx * dy);
    targetCameraCenterPos.z = dragStartCenter.z - (sry * dx - srx * cry * dy);
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
  if (e.mods.isRightButtonDown() || e.mods.isPopupMenu())
    return;

  if (e.getDistanceFromDragStart() < 15.0f) {
    if (hoveredNodeIndex >= 0) {
      selectedNodeIndex = hoveredNodeIndex;
      targetCameraCenterPos = nodes[selectedNodeIndex].targetPos;
      listeners.call([&](SampleCloudListener &l) {
        l.cloudSampleSelected(nodes[selectedNodeIndex].item);
      });
      overlayComponent.repaint();
      repaint();
    }
  }
  isRotating = false;
  isPanning = false;
}

void SampleCloudComponent::mouseMove(const juce::MouseEvent &e) {
  if (nodes.empty() || lastViewProjectionMatrix.mat[15] == 0.0f)
    return;

  int bestIndex = -1;
  float bestZ = std::numeric_limits<float>::max();
  float halfW = getWidth() * 0.5f;
  float halfH = getHeight() * 0.5f;

  for (size_t i = 0; i < nodes.size(); ++i) {
    auto &n = nodes[i];
    juce::Vector3D<float> p(n.currentPos.x, n.currentPos.y, n.currentPos.z);
    float w = lastViewProjectionMatrix.mat[3] * p.x +
              lastViewProjectionMatrix.mat[7] * p.y +
              lastViewProjectionMatrix.mat[11] * p.z +
              lastViewProjectionMatrix.mat[15];
    if (w > 0.0f) {
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
      float sx = halfW + (spX / w) * halfW;
      float sy = halfH - (spY / w) * halfH;
      float dx = sx - e.position.x;
      float dy = sy - e.position.y;
      float visualRadius = std::max(2.2f, n.radius * 2.0f) + 4.0f;
      if (dx * dx + dy * dy < visualRadius * visualRadius) {
        if (spZ < bestZ) {
          bestZ = spZ;
          bestIndex = static_cast<int>(i);
        }
      }
    }
  }

  if (bestIndex != hoveredNodeIndex) {
    hoveredNodeIndex = bestIndex;
    overlayComponent.repaint();
    repaint();
  }
}

void SampleCloudComponent::mouseWheelMove(
    const juce::MouseEvent &e, const juce::MouseWheelDetails &wheel) {
  if (e.y > getHeight() - 40 && e.x < getWidth() - 320) {
    float totalWidth = 0.0f;
    juce::Font legendFont(13.0f, juce::Font::bold);
    for (const auto &cluster : clusters) {
      juce::String fullTag = getFullCategoryName(cluster.tag).toUpperCase();
      totalWidth += 12.0f + 6.0f +
                    legendFont.getStringWidthFloat(fullTag) +
                    20.0f;
    }

    float viewWidth = getWidth() - 320.0f - 20.0f;
    float maxScroll = 0.0f;
    if (totalWidth > viewWidth)
      maxScroll = totalWidth - viewWidth;

    legendScrollOffset += wheel.deltaX * 150.0f + wheel.deltaY * 150.0f;
    legendScrollOffset = juce::jlimit(-maxScroll, 0.0f, legendScrollOffset);
    overlayComponent.repaint();
    repaint();
  } else {
    targetZoomScale = juce::jlimit(
        0.05f, 10.0f, targetZoomScale * std::pow(1.15f, wheel.deltaY * 3.0f));
    overlayComponent.repaint();
    repaint();
  }
}

void SampleCloudComponent::mouseDoubleClick(const juce::MouseEvent &e) {
  if (e.mods.isRightButtonDown() || e.mods.isPopupMenu())
    return;

  if (hoveredNodeIndex >= 0) {
    listeners.call([&](SampleCloudListener &l) {
      l.cloudSampleDoubleClicked(nodes[hoveredNodeIndex].item);
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
  if (tags.empty())
    return "Untagged";

  static const std::vector<juce::String> priorityList = {
      "#Kick", "#SubKick", "#Snare", "#Rimshot", "#Clap", "#Snap",
      "#OpenHat", "#ClosedHat", "#HiHat", "#Tom", "#Crash", "#Ride", "#Cymbal",
      "#Shaker", "#Tambourine", "#Percussion",
      "#SubBass", "#SynthBass", "#ReeseBass", "#808Bass", "#808", "#Bass",
      "#Piano", "#Rhodes", "#Organ", "#Keys", "#AcousticGuitar", "#ElectricGuitar", "#Guitar",
      "#Orchestral", "#Ensemble", "#Strings", "#Violin", "#Cello", "#Viola", "#Contrabass",
      "#Brass", "#FrenchHorn", "#Trumpet", "#Trombone", "#Tuba",
      "#Woodwinds", "#Flute", "#Piccolo", "#Oboe", "#Clarinet", "#Bassoon",
      "#Timpani", "#TubularBells", "#Glockenspiel", "#Xylophone", "#Gong", "#Harp", "#Choir",
      "#Sax", "#Marimba", "#Bell",
      "#Acapella", "#VocalChop", "#Chant", "#Speech", "#Vocal",
      "#SubDrop", "#Impact", "#Sweep", "#Downlifter", "#Riser", "#FX", "#Glitch", "#Atmosphere", "#Texture", "#Foley",
      "#Synth", "#Lead", "#Pad", "#Pluck", "#Arp",
      "#DrumLoop", "#MelodicLoop", "#VocalLoop", "#BassLoop", "#PercLoop", "#TopLoop", "#Loop",
      "#OneShot"
  };

  for (const auto& prio : priorityList) {
    if (tags.find(prio) != tags.end())
      return prio;
  }

  for (const auto& t : tags) {
    if (!t.endsWithIgnoreCase("BPM") && !t.startsWithIgnoreCase("#Key_") &&
        t != "#Wav" && t != "#MP3" && t != "#FLAC" && t != "#Stereo" && t != "#Mono" &&
        t != "#Short" && t != "#Long" && t != "#Punchy" && t != "#Bright" && t != "#Warm") {
      return t;
    }
  }

  return *tags.begin();
}

juce::Colour
SampleCloudComponent::getColourForTag(const juce::String &tag) const {
  auto t = tag.toLowerCase();
  if (t.contains("kick"))
    return juce::Colour(0xffe53935); // Crimson Red
  if (t.contains("snare") || t.contains("rimshot") || t.contains("rim"))
    return juce::Colour(0xfff57c00); // Bright Orange
  if (t.contains("clap") || t.contains("snap"))
    return juce::Colour(0xffff9800); // Amber
  if (t.contains("hat") || t.contains("hihat"))
    return juce::Colour(0xff0288d1); // Light Blue
  if (t.contains("tom"))
    return juce::Colour(0xffd84315); // Rust Red
  if (t.contains("crash") || t.contains("ride") || t.contains("cymbal"))
    return juce::Colour(0xff00acc1); // Cyan
  if (t.contains("perc") || t.contains("shaker") || t.contains("tamb") || t.contains("conga") || t.contains("bongo") || t.contains("cowbell"))
    return juce::Colour(0xff2e7d32); // Forest Green
  if (t.contains("bass"))
    return juce::Colour(0xff673ab7); // Deep Violet
  if (t.contains("piano") || t.contains("rhodes") || t.contains("organ") || t.contains("keys"))
    return juce::Colour(0xff3f51b5); // Indigo
  if (t.contains("guitar") || t.contains("gtr"))
    return juce::Colour(0xff00897b); // Sea Green
  if (t.contains("string") || t.contains("violin") || t.contains("cello") || t.contains("viola") || t.contains("contrabass"))
    return juce::Colour(0xffab47bc); // Orchid Purple
  if (t.contains("brass") || t.contains("horn") || t.contains("trumpet") || t.contains("trombone") || t.contains("tuba"))
    return juce::Colour(0xffff7043); // Bronze Orange
  if (t.contains("woodwind") || t.contains("flute") || t.contains("oboe") || t.contains("clarinet") || t.contains("bassoon"))
    return juce::Colour(0xff26a69a); // Teal
  if (t.contains("orchestral") || t.contains("symphon") || t.contains("ensemble"))
    return juce::Colour(0xff8d6e63); // Mahogany
  if (t.contains("timpani") || t.contains("glock") || t.contains("xylophone") || t.contains("marimba") || t.contains("harp"))
    return juce::Colour(0xffec407a); // Magenta
  if (t.contains("choir") || t.contains("chorus"))
    return juce::Colour(0xffe91e63); // Pink
  if (t.contains("vocal") || t.contains("vox") || t.contains("acapella") || t.contains("chant") || t.contains("speech"))
    return juce::Colour(0xffc2185b); // Deep Rose
  if (t.contains("lead") || t.contains("pluck") || t.contains("arp"))
    return juce::Colour(0xffffb300); // Gold
  if (t.contains("pad") || t.contains("synth"))
    return juce::Colour(0xff7e57c2); // Lavender Blue
  if (t.contains("riser") || t.contains("downlifter") || t.contains("subdrop") || t.contains("impact") || t.contains("sweep") || t.contains("fx") || t.contains("sfx") || t.contains("foley") || t.contains("glitch"))
    return juce::Colour(0xff00b0ff); // Electric Blue
  if (t.contains("loop"))
    return juce::Colour(0xff009688); // Teal Green

  uint32_t hash = static_cast<uint32_t>(tag.hashCode());
  float hue = (hash % 360) / 360.0f;
  return juce::Colour::fromHSV(hue, 0.65f, 0.85f, 1.0f);
}

void SampleCloudComponent::showContextMenuForNode(int idx) {
  if (idx < 0 || idx >= static_cast<int>(nodes.size()))
    return;
  const auto &item = nodes[idx].item;

  juce::PopupMenu menu;
  menu.addSectionHeader(item.fileName);
  menu.addItem(1,
               item.isFavorite ? "Remove from Favorites" : "Add to Favorites");
  menu.addItem(2, "Add Custom Tag...");
  menu.addItem(3, "Reveal in Explorer");

  menu.showMenuAsync(juce::PopupMenu::Options(), [this, item](int result) {
    if (result == 1)
      dbManager.toggleFavorite(item.id);
    else if (result == 3)
      juce::File(item.filePath).revealToUser();
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
      "out vec4 destColour;\n"
      "out float viewDepth;\n"

      "float rand(vec2 co){ return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) "
      "* 43758.5453); }\n"

      "void main()\n"
      "{\n"
      "    vec4 viewPos = viewMatrix * vec4(position, 1.0);\n"
      "    gl_Position = projectionMatrix * viewPos;\n"
      "    float phase = position.x * 0.1 + position.y * 0.15 + position.z * 0.05;\n"
      "    float noise = rand(position.xy * 0.01) * 0.5;\n"
      "    float twinkle = 0.65 + 0.35 * sin(time * (1.5 + noise) + phase) * cos(time * 0.8 + phase * 1.3);\n"
      "    float depthFade = clamp(600.0 / max(1.0, -viewPos.z), 0.15, 1.0);\n"
      "    destColour = colour;\n"
      "    if (isLightMode > 0.5) { destColour.rgb *= 0.85; }\n"
      "    destColour.a *= revealAlpha * depthFade * twinkle;\n"
      "    viewDepth = max(0.0, -viewPos.z);\n"
      "    gl_PointSize = max(3.0, radius * 7500.0 / max(1.0, -viewPos.z));\n"
      "}\n";

  juce::String fragmentShader =
      "#version 150\n"
      "in vec4 destColour;\n"
      "in float viewDepth;\n"
      "out vec4 finalColour;\n"
      "uniform vec3 fogColor;\n"

      "vec3 filmicToneMap(vec3 x) {\n"
      "    float a = 2.51;\n"
      "    float b = 0.03;\n"
      "    float c = 2.43;\n"
      "    float d = 0.59;\n"
      "    float e = 0.14;\n"
      "    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);\n"
      "}\n"

      "void main()\n"
      "{\n"
      "    vec2 p = gl_PointCoord * 2.0 - vec2(1.0);\n"
      "    float r = dot(p, p);\n"
      "    if (r > 1.0) discard;\n"

      "    // Soft Gaussian core + radial halo falloff for seamless overlapping node blending\n"
      "    float core = smoothstep(0.45, 0.0, r);\n"
      "    float halo = exp(-r * 3.2) * 0.65;\n"
      "    float alpha = clamp(core + halo, 0.0, 1.0);\n"

      "    // Vibrant Filter: Saturation & Contrast Boost\n"
      "    vec3 col = destColour.rgb;\n"
      "    float lum = dot(col, vec3(0.2126, 0.7152, 0.0722));\n"
      "    vec3 vibrantCol = mix(vec3(lum), col, 1.42);\n"
      "    vibrantCol = mix(vec3(0.5), vibrantCol, 1.15);\n"

      "    // Atmospheric Flight Fog (Distance fog into background color)\n"
      "    float fogDensity = 0.00035;\n"
      "    float fogFactor = clamp(exp(-viewDepth * fogDensity), 0.0, 1.0);\n"
      "    vec3 foggedRGB = mix(fogColor, vibrantCol, fogFactor);\n"

      "    // ACES Tone Mapping for smooth vibrant highlights\n"
      "    vec3 filmicRGB = filmicToneMap(foggedRGB * 1.25);\n"
      "    finalColour = vec4(filmicRGB, destColour.a * alpha);\n"
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
  juce::OpenGLHelpers::clear(OpenWavLookAndFeel::bgCard);

  if (layoutPending || !shaderProgram || vertexBuffer.empty())
    return;

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
  auto bgCard = OpenWavLookAndFeel::bgCard;
  openGLContext.extensions.glUniform3f(
      openGLContext.extensions.glGetUniformLocation(progID, "fogColor"),
      bgCard.getFloatRed(), bgCard.getFloatGreen(), bgCard.getFloatBlue());

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
