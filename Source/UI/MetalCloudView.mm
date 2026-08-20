#if defined(__APPLE__)

#include "MetalCloudView.h"
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <AppKit/AppKit.h>
#import <simd/simd.h>
#include <mutex>

namespace openwav {

static const char* kMetalShaderSource = R"(
#include <metal_stdlib>
using namespace metal;

struct VertexInput {
    float3 position [[attribute(0)]];
    float4 colour   [[attribute(1)]];
    float  radius   [[attribute(2)]];
};

struct Uniforms {
    float4x4 projectionMatrix;
    float4x4 viewMatrix;
    float revealAlpha;
    float time;
    float isLightMode;
    float scaleFactor;
    float3 fogColor;
    float pad1;
    float2 viewportSize;
};

struct VertexOutput {
    float4 position [[position]];
    float4 colour;
    float  pointSize [[point_size]];
    float  viewDepth;
};

float rand(float2 co) {
    return fract(sin(dot(co, float2(12.9898, 78.233))) * 43758.5453);
}

float3 filmicToneMap(float3 x) {
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0f, 1.0f);
}

vertex VertexOutput cloudVertexShader(VertexInput in [[stage_in]],
                                      constant Uniforms& uniforms [[buffer(1)]]) {
    VertexOutput out;
    float4 viewPos = uniforms.viewMatrix * float4(in.position, 1.0);
    out.position = uniforms.projectionMatrix * viewPos;
    out.position.z = 0.5f * (out.position.z + out.position.w);

    float phase = in.position.x * 0.1f + in.position.y * 0.15f + in.position.z * 0.05f;
    float noise = rand(in.position.xy * 0.01f) * 0.5f;
    float twinkle = 0.65f + 0.35f * sin(uniforms.time * (1.5f + noise) + phase) * cos(uniforms.time * 0.8f + phase * 1.3f);
    float depthFade = clamp(600.0f / max(1.0f, -viewPos.z), 0.15f, 1.0f);

    float4 col = in.colour;
    if (uniforms.isLightMode > 0.5f) {
        col.rgb *= 0.85f;
    }
    col.a *= uniforms.revealAlpha * depthFade * twinkle;
    out.colour = col;
    out.viewDepth = max(0.0f, -viewPos.z);
    
    // Scale point size by Retina scaleFactor for crisp high-resolution rasterization
    float baseSize = max(3.0f, in.radius * 7500.0f / max(1.0f, -viewPos.z));
    out.pointSize = clamp(baseSize * uniforms.scaleFactor, 2.0f, 511.0f);
    return out;
}

fragment float4 cloudFragmentShader(VertexOutput in [[stage_in]],
                                    float2 pointCoord [[point_coord]],
                                    constant Uniforms& uniforms [[buffer(1)]]) {
    float2 p = pointCoord * 2.0f - float2(1.0f);
    float r2 = dot(p, p);
    if (r2 > 1.0f) {
        discard_fragment();
    }

    // Ultra-smooth Gaussian core + exponential halo falloff + anti-aliased perimeter fade
    float core = smoothstep(0.45f, 0.0f, r2);
    float halo = exp(-r2 * 3.2f) * 0.65f;
    float edgeAntiAlias = smoothstep(1.0f, 0.80f, r2);
    float alpha = clamp((core + halo) * edgeAntiAlias, 0.0f, 1.0f);

    // Vibrant Filter: Saturation & Contrast Boost
    float3 col = in.colour.rgb;
    float lum = dot(col, float3(0.2126f, 0.7152f, 0.0722f));
    float3 vibrantCol = mix(float3(lum), col, 1.42f);
    vibrantCol = mix(float3(0.5f), vibrantCol, 1.15f);

    // Atmospheric Fog
    float fogDensity = 0.00035f;
    float fogFactor = clamp(exp(-in.viewDepth * fogDensity), 0.0f, 1.0f);
    float3 foggedRGB = mix(uniforms.fogColor, vibrantCol, fogFactor);

    // ACES Tone Mapping
    float3 filmicRGB = filmicToneMap(foggedRGB * 1.25f);
    return float4(filmicRGB, in.colour.a * alpha);
}
)";

} // namespace openwav

// PassThroughMTKView allows mouse events to pass directly through to JUCE and maintains Retina scaling
@interface PassThroughMTKView : MTKView
@end

@implementation PassThroughMTKView
- (NSView *)hitTest:(NSPoint)point {
    return nil;
}
- (BOOL)acceptsFirstMouse:(NSEvent *)event {
    return NO;
}
- (void)viewDidChangeBackingProperties {
    [super viewDidChangeBackingProperties];
    CGFloat scale = (self.window != nil && self.window.backingScaleFactor > 0)
                        ? self.window.backingScaleFactor
                        : [NSScreen mainScreen].backingScaleFactor;
    if (scale < 1.0) scale = 1.0;
    self.layer.contentsScale = scale;
    CGSize boundsSize = self.bounds.size;
    if (boundsSize.width > 0 && boundsSize.height > 0) {
        self.drawableSize = CGSizeMake(boundsSize.width * scale, boundsSize.height * scale);
    }
}
- (void)setFrame:(NSRect)frame {
    [super setFrame:frame];
    CGFloat scale = (self.window != nil && self.window.backingScaleFactor > 0)
                        ? self.window.backingScaleFactor
                        : [NSScreen mainScreen].backingScaleFactor;
    if (scale < 1.0) scale = 1.0;
    self.layer.contentsScale = scale;
    if (frame.size.width > 0 && frame.size.height > 0) {
        self.drawableSize = CGSizeMake(frame.size.width * scale, frame.size.height * scale);
    }
}
- (void)setBounds:(NSRect)bounds {
    [super setBounds:bounds];
    CGFloat scale = (self.window != nil && self.window.backingScaleFactor > 0)
                        ? self.window.backingScaleFactor
                        : [NSScreen mainScreen].backingScaleFactor;
    if (scale < 1.0) scale = 1.0;
    self.layer.contentsScale = scale;
    if (bounds.size.width > 0 && bounds.size.height > 0) {
        self.drawableSize = CGSizeMake(bounds.size.width * scale, bounds.size.height * scale);
    }
}
@end

// MTKView delegate for rendering frames
@interface CloudMetalDelegate : NSObject <MTKViewDelegate>
@property (nonatomic, assign) openwav::MetalCloudView::Impl* impl;
@end

namespace openwav {

class MetalCloudView::Impl {
public:
    id<MTLDevice> device { nil };
    id<MTLCommandQueue> commandQueue { nil };
    id<MTLRenderPipelineState> pipelineStateLight { nil };
    id<MTLRenderPipelineState> pipelineStateDark { nil };
    PassThroughMTKView* mtkView { nil };
    CloudMetalDelegate* delegate { nil };

    id<MTLBuffer> vertexBuffer { nil };
    id<MTLBuffer> uniformBuffer { nil };
    size_t vertexCount { 0 };
    MetalUniforms currentUniforms {};
    std::mutex renderMutex;
    MTLClearColor clearColor { MTLClearColorMake(0.1, 0.1, 0.1, 1.0) };
    bool isPaused { false };

    Impl() = default;

    ~Impl() {
        @autoreleasepool {
            if (mtkView != nil) {
                mtkView.delegate = nil;
                [mtkView removeFromSuperview];
                mtkView = nil;
            }
            delegate = nil;
            vertexBuffer = nil;
            uniformBuffer = nil;
            pipelineStateLight = nil;
            pipelineStateDark = nil;
            commandQueue = nil;
            device = nil;
        }
    }

    bool init() {
        @autoreleasepool {
            device = MTLCreateSystemDefaultDevice();
            if (device == nil) {
                return false;
            }

            commandQueue = [device newCommandQueue];
            if (commandQueue == nil) {
                return false;
            }

            NSError* error = nil;
            NSString* shaderSource = [NSString stringWithUTF8String:kMetalShaderSource];
            id<MTLLibrary> library = [device newLibraryWithSource:shaderSource options:nil error:&error];
            if (library == nil || error != nil) {
                NSLog(@"[MetalCloudView] Error compiling Metal shaders: %@", error.localizedDescription);
                return false;
            }

            id<MTLFunction> vertexFunc = [library newFunctionWithName:@"cloudVertexShader"];
            id<MTLFunction> fragmentFunc = [library newFunctionWithName:@"cloudFragmentShader"];

            // Vertex descriptor layout
            MTLVertexDescriptor* vertexDesc = [[MTLVertexDescriptor alloc] init];
            // Position (x, y, z)
            vertexDesc.attributes[0].format = MTLVertexFormatFloat3;
            vertexDesc.attributes[0].offset = offsetof(MetalVertex, x);
            vertexDesc.attributes[0].bufferIndex = 0;
            // Colour (r, g, b, a)
            vertexDesc.attributes[1].format = MTLVertexFormatFloat4;
            vertexDesc.attributes[1].offset = offsetof(MetalVertex, r);
            vertexDesc.attributes[1].bufferIndex = 0;
            // Radius
            vertexDesc.attributes[2].format = MTLVertexFormatFloat;
            vertexDesc.attributes[2].offset = offsetof(MetalVertex, radius);
            vertexDesc.attributes[2].bufferIndex = 0;

            vertexDesc.layouts[0].stride = sizeof(MetalVertex);
            vertexDesc.layouts[0].stepRate = 1;
            vertexDesc.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;

            // Pipeline Descriptor for Light Mode (Standard alpha blend)
            MTLRenderPipelineDescriptor* pipeDescLight = [[MTLRenderPipelineDescriptor alloc] init];
            pipeDescLight.vertexFunction = vertexFunc;
            pipeDescLight.fragmentFunction = fragmentFunc;
            pipeDescLight.vertexDescriptor = vertexDesc;
            pipeDescLight.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
            pipeDescLight.colorAttachments[0].blendingEnabled = YES;
            pipeDescLight.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
            pipeDescLight.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
            pipeDescLight.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
            pipeDescLight.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
            pipeDescLight.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
            pipeDescLight.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

            pipelineStateLight = [device newRenderPipelineStateWithDescriptor:pipeDescLight error:&error];
            if (pipelineStateLight == nil || error != nil) {
                NSLog(@"[MetalCloudView] Error creating Light pipeline state: %@", error.localizedDescription);
                return false;
            }

            // Pipeline Descriptor for Dark Mode (Luminous additive blend)
            MTLRenderPipelineDescriptor* pipeDescDark = [[MTLRenderPipelineDescriptor alloc] init];
            pipeDescDark.vertexFunction = vertexFunc;
            pipeDescDark.fragmentFunction = fragmentFunc;
            pipeDescDark.vertexDescriptor = vertexDesc;
            pipeDescDark.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
            pipeDescDark.colorAttachments[0].blendingEnabled = YES;
            pipeDescDark.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
            pipeDescDark.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
            pipeDescDark.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
            pipeDescDark.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOne;
            pipeDescDark.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
            pipeDescDark.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOne;

            pipelineStateDark = [device newRenderPipelineStateWithDescriptor:pipeDescDark error:&error];
            if (pipelineStateDark == nil || error != nil) {
                NSLog(@"[MetalCloudView] Error creating Dark pipeline state: %@", error.localizedDescription);
                return false;
            }

            // Create Uniform buffer
            uniformBuffer = [device newBufferWithLength:sizeof(MetalUniforms)
                                                options:MTLResourceStorageModeShared];

            // Setup MTKView with native Retina backing store
            CGFloat initialScale = [NSScreen mainScreen].backingScaleFactor;
            if (initialScale < 1.0) initialScale = 1.0;

            mtkView = [[PassThroughMTKView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) device:device];
            mtkView.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
            mtkView.clearColor = clearColor;
            mtkView.paused = YES;
            mtkView.enableSetNeedsDisplay = NO;
            mtkView.autoResizeDrawable = YES;
            mtkView.layer.contentsScale = initialScale;
            mtkView.drawableSize = CGSizeMake(800 * initialScale, 600 * initialScale);

            delegate = [[CloudMetalDelegate alloc] init];
            delegate.impl = this;
            mtkView.delegate = delegate;

            return true;
        }
    }

    void draw(MTKView* view) {
        if (isPaused) {
            return;
        }

        std::lock_guard<std::mutex> lock(renderMutex);
        if (vertexBuffer == nil || vertexCount == 0 || uniformBuffer == nil) {
            return;
        }

        @autoreleasepool {
            MTLRenderPassDescriptor* renderPassDesc = view.currentRenderPassDescriptor;
            if (renderPassDesc == nil) {
                return;
            }

            renderPassDesc.colorAttachments[0].clearColor = clearColor;
            renderPassDesc.colorAttachments[0].loadAction = MTLLoadActionClear;
            renderPassDesc.colorAttachments[0].storeAction = MTLStoreActionStore;

            id<CAMetalDrawable> drawable = view.currentDrawable;
            if (drawable == nil) {
                return;
            }

            id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];
            if (commandBuffer == nil) {
                return;
            }

            id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDesc];
            if (encoder == nil) {
                return;
            }

            id<MTLRenderPipelineState> pipeline = (currentUniforms.isLightMode > 0.5f) ? pipelineStateLight : pipelineStateDark;
            [encoder setRenderPipelineState:pipeline];
            [encoder setVertexBuffer:vertexBuffer offset:0 atIndex:0];
            [encoder setVertexBuffer:uniformBuffer offset:0 atIndex:1];
            [encoder setFragmentBuffer:uniformBuffer offset:0 atIndex:1];

            [encoder drawPrimitives:MTLPrimitiveTypePoint vertexStart:0 vertexCount:vertexCount];
            [encoder endEncoding];

            [commandBuffer presentDrawable:drawable];
            [commandBuffer commit];
        }
    }
};

} // namespace openwav

@implementation CloudMetalDelegate
- (void)mtkView:(nonnull MTKView *)view drawableSizeWillChange:(CGSize)size {
}

- (void)drawInMTKView:(nonnull MTKView *)view {
    if (self.impl != nullptr) {
        self.impl->draw(view);
    }
}
@end

namespace openwav {

MetalCloudView::MetalCloudView()
    : pImpl(new Impl()) {
}

MetalCloudView::~MetalCloudView() {
    delete pImpl;
    pImpl = nullptr;
}

bool MetalCloudView::initialize() {
    return pImpl ? pImpl->init() : false;
}

void* MetalCloudView::getNativeView() const {
    return (pImpl && pImpl->mtkView) ? (__bridge void*)pImpl->mtkView : nullptr;
}

void MetalCloudView::updateVertices(const std::vector<MetalVertex>& vertices) {
    if (!pImpl || !pImpl->device) return;

    std::lock_guard<std::mutex> lock(pImpl->renderMutex);
    pImpl->vertexCount = vertices.size();
    if (vertices.empty()) {
        pImpl->vertexBuffer = nil;
        return;
    }

    size_t dataSize = vertices.size() * sizeof(MetalVertex);
    if (pImpl->vertexBuffer == nil || [pImpl->vertexBuffer length] < dataSize) {
        pImpl->vertexBuffer = [pImpl->device newBufferWithLength:dataSize options:MTLResourceStorageModeShared];
    }

    if (pImpl->vertexBuffer != nil) {
        memcpy([pImpl->vertexBuffer contents], vertices.data(), dataSize);
    }
}

void MetalCloudView::updateUniforms(const MetalUniforms& uniforms) {
    if (!pImpl || !pImpl->uniformBuffer) return;

    std::lock_guard<std::mutex> lock(pImpl->renderMutex);
    pImpl->currentUniforms = uniforms;
    memcpy([pImpl->uniformBuffer contents], &uniforms, sizeof(MetalUniforms));
}

void MetalCloudView::setClearColor(float r, float g, float b, float a) {
    if (!pImpl) return;
    std::lock_guard<std::mutex> lock(pImpl->renderMutex);
    pImpl->clearColor = MTLClearColorMake(r, g, b, a);
    if (pImpl->mtkView) {
        pImpl->mtkView.clearColor = pImpl->clearColor;
    }
}

void MetalCloudView::setPaused(bool paused) {
    if (!pImpl) return;
    pImpl->isPaused = paused;
}

void MetalCloudView::renderNow() {
    if (!pImpl || !pImpl->mtkView || pImpl->isPaused) return;
    [pImpl->mtkView draw];
}

} // namespace openwav

#endif // __APPLE__
