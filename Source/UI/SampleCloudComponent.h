#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_gui_basics/juce_gui_basics.h>
 #include <juce_opengl/juce_opengl.h>
#endif
#include <thread>
#include <atomic>
#include <vector>
#include <set>
#include <utility>
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
                             public juce::Timer,
                             public juce::OpenGLRenderer
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
        float radius { 4.0f };
        juce::Colour colour;
        juce::String primaryTag;
        float hoverScale { 1.0f };
    };

    struct TagCluster
    {
        juce::String tag;
        Vector3D centerPos;
        juce::Colour colour;
        int count { 0 };
    };

    SampleCloudComponent(TagDatabaseManager& db, AudioEngine& engine);
    ~SampleCloudComponent() override;

    void setItems(const std::vector<MediaItem>& items);
    void selectItemById(const juce::String& itemId);
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

    void addListener(SampleCloudListener* listener);
    void removeListener(SampleCloudListener* listener);

    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;

private:
    void timerCallback() override;
    void resetZoomAndPan();
    juce::Colour getColourForTag(const juce::String& tag) const;
    juce::String getPrimaryCategoryTag(const std::set<juce::String>& tags) const;
    void showContextMenuForNode(int idx);

    void runLayoutAsync(std::vector<MediaItem> items);
    std::pair<std::vector<TagCluster>, std::vector<std::pair<size_t, size_t>>> calculateClusterLayoutInternal(std::vector<CloudNode>& nodesCopy);

    TagDatabaseManager& dbManager;
    AudioEngine& audioEngine;

    std::vector<CloudNode> nodes;
    std::vector<TagCluster> clusters;
    std::vector<std::pair<size_t, size_t>> constellationEdges;

    int hoveredNodeIndex { -1 };
    int selectedNodeIndex { -1 };

    float rotX { 0.35f };
    float rotY { 0.45f };
    float targetRotX { 0.35f };
    float targetRotY { 0.45f };
    float cameraDistance { 850.0f };
    float zoomScale { 1.0f };
    juce::Point<float> panOffset { 0.0f, 0.0f };
    
    juce::Point<float> dragStartPan { 0.0f, 0.0f };
    juce::Point<float> mouseDragStartPos { 0.0f, 0.0f };
    float dragStartRotX { 0.0f };
    float dragStartRotY { 0.0f };
    bool isPanning { false };
    bool isRotating { false };

    juce::TextButton zoomInButton    { "+" };
    juce::TextButton zoomOutButton   { "-" };
    juce::TextButton resetZoomButton { "Reset View" };
    juce::TextButton viewModeButton  { "2D" };
    juce::TextButton autoRotateButton { "Stop Spin" };
    bool autoRotate { true };

    juce::ListenerList<SampleCloudListener> listeners;
    float revealAlpha { 0.0f };
    bool is2DMode { false };
    float saved3DRotX { 0.35f };
    float saved3DRotY { 0.45f };
    uint64_t currentDataHash { 0 };

    std::thread layoutThread;
    std::atomic<bool> layoutActive { false };
    bool layoutPending { false };
    float pulsePhase { 0.0f };

    juce::OpenGLContext openGLContext;
    std::unique_ptr<juce::OpenGLShaderProgram> shaderProgram;
    GLuint vbo { 0 };
    bool vboNeedsUpdate { false };

    struct Vertex {
        float x, y, z;
        float r, g, b, a;
        float radius;
    };
    std::vector<Vertex> vertexBuffer;

    juce::Matrix3D<float> lastViewProjectionMatrix;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleCloudComponent)
};

} // namespace openwav
