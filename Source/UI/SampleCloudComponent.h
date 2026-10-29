#pragma once

#if __has_include(<JuceHeader.h>)
 #include <JuceHeader.h>
#else
 #include <juce_gui_basics/juce_gui_basics.h>
 #include <juce_gui_extra/juce_gui_extra.h>
 #if !JUCE_MAC
  #include <juce_opengl/juce_opengl.h>
 #endif
#endif

#if JUCE_MAC
 #include "MetalCloudView.h"
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
    virtual void editSampleRequested(const MediaItem& /*item*/) {}
    virtual void addToSampleMapRequested(const MediaItem& /*item*/) {}
    virtual void autoSliceToSamplerRequested(const MediaItem& /*item*/) {}
};

class SampleCloudComponent : public juce::Component,
                             public juce::Timer
#if !JUCE_MAC
                           , public juce::OpenGLRenderer
#endif
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
        Vector3D pos3D;
        Vector3D pos2D;
        float radius { 4.0f };
        juce::Colour colour;
        juce::String primaryTag;
        juce::String canonicalCategory;
        float hoverScale { 1.0f };
    };

    struct TagCluster
    {
        juce::String tag;
        Vector3D centerPos;
        Vector3D centerPos3D;
        Vector3D centerPos2D;
        juce::Colour colour;
        int count { 0 };
    };

    SampleCloudComponent(TagDatabaseManager& db, AudioEngine& engine);
    ~SampleCloudComponent() override;

    void setItems(const std::vector<MediaItem>& items);
    void selectItemById(const juce::String& itemId);
    void refreshView();
    float getEffectiveWidth() const;
    float getEffectiveHeight() const;
    
    void paint(juce::Graphics& g) override;
    void paintOverlay(juce::Graphics& g);
    void resized() override;
    void visibilityChanged() override;
    void parentHierarchyChanged() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

    void addListener(SampleCloudListener* listener);
    void removeListener(SampleCloudListener* listener);

#if !JUCE_MAC
    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;
#endif

private:
    class CloudOverlayComponent : public juce::Component
    {
    public:
        CloudOverlayComponent(SampleCloudComponent& ownerComp);
        ~CloudOverlayComponent() override = default;

        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseDrag(const juce::MouseEvent& e) override;
        void mouseUp(const juce::MouseEvent& e) override;
        void mouseMove(const juce::MouseEvent& e) override;
        void mouseExit(const juce::MouseEvent& e) override;
        void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
        void mouseDoubleClick(const juce::MouseEvent& e) override;

    private:
        SampleCloudComponent& owner;
    };

    void timerCallback() override;
    void resetZoomAndPan();
    juce::Colour getColourForTag(const juce::String& tag) const;
    juce::String getPrimaryCategoryTag(const std::set<juce::String>& tags) const;
    static juce::String getAtlasCategoryName(const juce::String& tag);
    static juce::String getCanonicalAtlasCategory(const MediaItem& item);

    void runLayoutAsync(std::vector<MediaItem> items);
    std::pair<std::vector<TagCluster>, std::vector<std::pair<size_t, size_t>>> calculateClusterLayoutInternal(std::vector<CloudNode>& nodesCopy);
    void rebuildGpuBuffers();
    void showContextMenuForNode(int nodeIndex);
    int findHitNodeAt(juce::Point<float> pos) const;

    TagDatabaseManager& dbManager;
    AudioEngine& audioEngine;

    std::vector<CloudNode> nodes;
    std::vector<TagCluster> clusters;
    std::vector<std::pair<size_t, size_t>> constellationEdges;

    int hoveredNodeIndex { -1 };
    int selectedNodeIndex { -1 };
    int animatedHoverNodeIndex { -1 };
    float currentHoverScale { 1.0f };
    float targetHoverScale { 1.0f };

    float rotX { 0.35f };
    float rotY { 0.45f };
    float targetRotX { 0.35f };
    float targetRotY { 0.45f };
    float cameraDistance { 850.0f };
    float zoomScale { 1.0f };
    float targetZoomScale { 1.0f };
    Vector3D cameraCenterPos { 0.0f, 0.0f, 0.0f };
    Vector3D targetCameraCenterPos { 0.0f, 0.0f, 0.0f };
    Vector3D cameraVelocity { 0.0f, 0.0f, 0.0f };
    Vector3D dragStartCenter { 0.0f, 0.0f, 0.0f };

    static float smoothDamp(float current, float target, float& currentVelocity, float smoothTime, float dt);
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
    juce::TextButton hoverPlayButton { "Mouseover: Off" };
    bool autoRotate { true };
    bool hoverPlayEnabled { false };

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
    float legendScrollOffset { 0.0f };

#if JUCE_MAC
    std::unique_ptr<MetalCloudView> metalView;
    std::unique_ptr<juce::NSViewComponent> metalContainer;
#else
    juce::OpenGLContext openGLContext;
    std::unique_ptr<juce::OpenGLShaderProgram> shaderProgram;
    GLuint vbo { 0 };
    bool vboNeedsUpdate { false };
#endif

    CloudOverlayComponent overlayComponent;

    struct Vertex {
        float x, y, z;
        float r, g, b, a;
        float radius;
    };
    std::vector<Vertex> vertexBuffer;

    struct LineVertex {
        float x, y, z;
        float r, g, b, a;
    };
    std::vector<LineVertex> lineVertexBuffer;

    void flyToCluster(int clusterIdx);
    float reticleAngle { 0.0f };
    float pulseRadar { 0.0f };

    // Waves Cosmos Style Quick-Audition & Similar Samples Dock
    struct SimilarItemEntry {
        int nodeIndex { -1 };
        MediaItem item;
        double similarity { 0.0 };
        bool isBrighter { false };
        juce::Rectangle<float> bounds;
    };

    void updateSimilarStack();
    void drawMiniWaveform(juce::Graphics& g, const juce::Rectangle<float>& bounds,
                          const MediaItem& item, juce::Colour waveColour);

    bool showSimilarStack { false };
    std::vector<SimilarItemEntry> similarAbove;
    std::vector<SimilarItemEntry> similarBelow;
    int hoveredSimilarIndex { -100 }; // -3..-1: above, 0: center, 1..3: below
    bool isHoveringPlayButton { false };
    bool isHoveringUpArrow { false };
    bool isHoveringDownArrow { false };
    bool isHoveringExpandChevron { false };
    bool isHoveringFavoriteStar { false };
    bool isHoveringMainPill { false };

    juce::Rectangle<float> mainPillBounds;
    juce::Rectangle<float> playButtonBounds;
    juce::Rectangle<float> upArrowBounds;
    juce::Rectangle<float> downArrowBounds;
    juce::Rectangle<float> expandChevronBounds;
    juce::Rectangle<float> favoriteStarBounds;
    juce::Rectangle<float> overallDockBounds;
    juce::Rectangle<float> getDockDeadzone() const;
    bool isClickConsumedByUI { false };

    float maxMapPanX { 800.0f };
    float maxMapPanY { 700.0f };
    float maxMapPanZ { 500.0f };
    uint32_t lastDockToggleTime { 0 };
    void updateMapBounds();

    std::vector<int> recentSelectionHistory;

    bool isDraggingDockSample { false };
    juce::Point<float> dockDragStartPos;
    juce::String dockDragFilePath;
    int dockDragNodeIndex { -1 };

    juce::Point<float> lastMapNodeClickPos { -1000.0f, -1000.0f };
    uint32_t lastMapNodeClickTimeMs { 0 };

    juce::Matrix3D<float> lastViewProjectionMatrix;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleCloudComponent)
};

} // namespace openwav
