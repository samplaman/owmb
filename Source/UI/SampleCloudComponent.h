#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_gui_basics/juce_gui_basics.h>
#endif
#include "../Models/MediaItem.h"
#include "../Database/TagDatabaseManager.h"
#include "../Audio/AudioEngine.h"

namespace openwav
{

class SampleCloudListener
{
public:
    virtual ~SampleCloudListener() = default;
    virtual void cloudSampleSelected(const MediaItem& item) = 0;
    virtual void cloudSampleDoubleClicked(const MediaItem& item) = 0;
};

class SampleCloudComponent : public juce::Component,
                             public juce::Timer
{
public:
    struct Vector3D
    {
        float x { 0.0f };
        float y { 0.0f };
        float z { 0.0f };

        Vector3D operator+(const Vector3D& other) const { return { x + other.x, y + other.y, z + other.z }; }
        Vector3D operator-(const Vector3D& other) const { return { x - other.x, y - other.y, z - other.z }; }
        Vector3D operator*(float scalar) const { return { x * scalar, y * scalar, z * scalar }; }
        Vector3D operator/(float scalar) const { return { x / scalar, y / scalar, z / scalar }; }
    };

    struct CloudNode
    {
        MediaItem item;
        Vector3D currentPos;
        Vector3D targetPos;
        Vector3D transformedPos;
        float radius { 4.0f };
        juce::Colour colour;
        juce::String primaryTag;
        float hoverScale { 1.0f };
        float projectedScale { 1.0f };
        juce::Point<float> screenPos;
        size_t originalIndex { 0 };
    };

    struct TagCluster
    {
        juce::String tag;
        Vector3D centerPos;
        Vector3D transformedPos;
        juce::Point<float> screenPos;
        float projectedScale { 1.0f };
        juce::Colour colour;
        int count { 0 };
    };

    SampleCloudComponent(TagDatabaseManager& dbManager, AudioEngine& audioEngine);
    ~SampleCloudComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    void mouseMove(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    void setItems(const std::vector<MediaItem>& items);
    void resetZoomAndPan();

    void addListener(SampleCloudListener* listener);
    void removeListener(SampleCloudListener* listener);

private:
    void calculateClusterLayout();
    void applyForceDirectedPhysics();
    int findNodeAtPosition(juce::Point<float> screenPos) const;
    juce::Colour getColourForTag(const juce::String& tag) const;
    void showContextMenuForNode(int idx);

    juce::Point<float> project3DToScreen(Vector3D pos, Vector3D& outTransformed, float& outScale) const;
    juce::Point<float> project3DToScreen(Vector3D pos, Vector3D& outTransformed, float& outScale, float cosX, float sinX, float cosY, float sinY) const;
    void update3DTransforms();

    TagDatabaseManager& dbManager;
    AudioEngine& audioEngine;

    std::vector<CloudNode> nodes;
    std::vector<CloudNode*> sortedNodePointers;
    std::vector<TagCluster> clusters;

    int hoveredNodeIndex { -1 };
    int selectedNodeIndex { -1 };
    float pulsePhase { 0.0f };

    // 3D Camera and Orbit State
    float rotX { 0.35f };
    float rotY { 0.45f };
    float targetRotX { 0.35f };
    float targetRotY { 0.45f };
    float cameraDistance { 650.0f };
    float focalLength { 550.0f };

    // Zoom and Pan State
    float zoomScale { 1.0f };
    juce::Point<float> panOffset { 0.0f, 0.0f };
    juce::Point<float> dragStartPan { 0.0f, 0.0f };
    juce::Point<float> mouseDragStartPos { 0.0f, 0.0f };
    float dragStartRotX { 0.0f };
    float dragStartRotY { 0.0f };
    bool isPanning { false };
    bool isRotating { false };
    bool hasStartedDrag { false };

    // Zoom HUD Buttons
    juce::TextButton zoomInButton { "+" };
    juce::TextButton zoomOutButton { "-" };
    juce::TextButton resetZoomButton { "Reset View" };

    juce::ListenerList<SampleCloudListener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleCloudComponent)
};

} // namespace openwav
