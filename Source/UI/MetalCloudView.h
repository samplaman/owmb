#pragma once

#if JUCE_MAC || defined(__APPLE__)

#include <vector>
#include <cstdint>

namespace openwav {

struct MetalVertex {
    float x, y, z;
    float r, g, b, a;
    float radius;
};

struct MetalLineVertex {
    float x, y, z;
    float r, g, b, a;
};

struct MetalUniforms {
    float projectionMatrix[16];
    float viewMatrix[16];
    float revealAlpha;
    float time;
    float isLightMode;
    float scaleFactor;
    float fogColor[3];
    float pad1;
    float viewportWidth;
    float viewportHeight;
};

class MetalCloudView {
public:
    class Impl;

    MetalCloudView();
    ~MetalCloudView();

    bool initialize();
    void* getNativeView() const;

    void updateVertices(const std::vector<MetalVertex>& vertices);
    void updateLines(const std::vector<MetalLineVertex>& lines);
    void updateUniforms(const MetalUniforms& uniforms);
    void setClearColor(float r, float g, float b, float a);
    void setPaused(bool paused);
    void renderNow();

private:
    Impl* pImpl { nullptr };
};

} // namespace openwav

#endif // JUCE_MAC
