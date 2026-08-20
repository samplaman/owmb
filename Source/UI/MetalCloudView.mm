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

struct LineVertexInput {
    float3 position [[attribute(0)]];
    float4 colour   [[attribute(1)]];
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
};

struct LineVertexOutput {
    float4 position [[position]];
    float4 colour;
};

struct BgVertexOut {
    float4 position [[position]];
    float2 uv;
};

// Procedural Hash for star dust & twinkle
float hash21(float2 p) {
    p = fract(p * float2(234.34, 435.345));
    p += dot(p, p + 34.23);
    return fract(p.x * p.y);
}

// Fullscreen Deep Space Nebula & Cosmic Dust Vertex Shader
vertex BgVertexOut bgVertexShader(uint vertexID [[vertex_id]]) {
    BgVertexOut out;
    out.uv = float2((vertexID << 1) & 2, vertexID & 2);
    out.position = float4(out.uv * 2.0f - 1.0f, 0.0f, 1.0f);
    out.uv.y = 1.0f - out.uv.y;
    return out;
}

// Fullscreen Deep Space Nebula & Cosmic Dust Fragment Shader
fragment float4 bgFragmentShader(BgVertexOut in [[stage_in]],
                                constant Uniforms& uniforms [[buffer(1)]]) {
    float2 p = in.uv * 2.0f - 1.0f;
    float aspect = (uniforms.viewportSize.y > 0.0f) ? (uniforms.viewportSize.x / uniforms.viewportSize.y) : 1.0f;
    float2 screenP = p;
    if (aspect > 1.0f) {
        p.x *= (aspect * 0.75f);
    } else {
        p.y /= (aspect * 0.75f);
    }
    float dist2 = dot(p, p);
    
    // Dynamic organic cosmic nebula swirls
    float t = uniforms.time * 0.06f;
    float n1 = sin(p.x * 2.4f + t * 1.2f) * cos(p.y * 2.1f + t * 0.9f);
    float n2 = sin(p.x * 4.8f - p.y * 3.6f + t * 1.5f) * 0.5f;
    float n3 = sin(p.x * 7.5f + p.y * 6.8f - t * 2.0f) * 0.25f;
    float nebula = clamp((n1 + n2 + n3) * 0.5f + 0.5f, 0.0f, 1.0f);
    
    float3 baseBg = uniforms.fogColor;
    float3 cyanNebula   = float3(0.015f, 0.065f, 0.120f);
    float3 purpleNebula = float3(0.060f, 0.015f, 0.095f);
    
    float3 cosmicColor = baseBg;
    if (uniforms.isLightMode < 0.5f) {
        // Dark Mode: Deep Cosmic Nebula glow
        float3 nebulaMix = mix(cyanNebula, purpleNebula, sin(p.x * 3.0f + uniforms.time * 0.1f) * 0.5f + 0.5f);
        cosmicColor = baseBg + nebulaMix * (nebula * 0.75f);
        
        // Ambient Micro Star Dust (Subtle high-tech twinkling dust)
        float2 gridCoord = screenP * 35.0f;
        float2 gridId = floor(gridCoord);
        float starRnd = hash21(gridId);
        if (starRnd > 0.965f) {
            float2 cellCenter = fract(gridCoord) - 0.5f;
            float starDist = length(cellCenter);
            float starTwinkle = 0.5f + 0.5f * sin(uniforms.time * (2.0f + starRnd * 4.0f) + starRnd * 6.28f);
            float starGlow = smoothstep(0.12f, 0.0f, starDist) * starTwinkle * 0.45f;
            cosmicColor += float3(0.6f, 0.8f, 1.0f) * starGlow;
        }
    } else {
        // Light Mode: Clean subtle pearl-azure atmospheric gradient
        float3 pearlGlow = float3(0.04f, 0.06f, 0.09f);
        cosmicColor = baseBg - pearlGlow * (1.0f - nebula * 0.5f);
    }
    
    // Cinematic Radial Vignette
    float vignette = clamp(1.0f - dist2 * 0.48f, 0.0f, 1.0f);
    vignette = smoothstep(0.0f, 1.0f, vignette);
    
    float3 edgeTint = (uniforms.isLightMode > 0.5f) ? baseBg * 0.70f : baseBg * 0.12f;
    float3 finalCol = mix(edgeTint, cosmicColor, vignette);
    
    return float4(finalCol, 1.0f);
}

// ACES Tone Mapping for rich HDR highlight roll-off
float3 filmicToneMap(float3 x) {
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0f, 1.0f);
}

// Constellation Lines Vertex Shader
vertex LineVertexOutput lineVertexShader(LineVertexInput in [[stage_in]],
                                         constant Uniforms& uniforms [[buffer(1)]]) {
    LineVertexOutput out;
    float4 viewPos = uniforms.viewMatrix * float4(in.position, 1.0f);
    out.position = uniforms.projectionMatrix * viewPos;
    out.position.z = 0.5f * (out.position.z + out.position.w);

    float viewDepth = max(0.0f, -viewPos.z);
    float depthFade = clamp(750.0f / max(1.0f, viewDepth), 0.08f, 1.0f);

    float pulse = 0.75f + 0.25f * sin(uniforms.time * 2.2f + (in.position.x + in.position.y) * 0.03f);
    float4 col = in.colour;
    col.a *= uniforms.revealAlpha * depthFade * pulse * 0.50f;
    out.colour = col;
    return out;
}

// Constellation Lines Fragment Shader
fragment float4 lineFragmentShader(LineVertexOutput in [[stage_in]]) {
    return in.colour;
}

// Point Cloud Vertex Shader (Triple-Layer Optical Bloom)
vertex VertexOutput cloudVertexShader(VertexInput in [[stage_in]],
                                      constant Uniforms& uniforms [[buffer(1)]]) {
    VertexOutput out;
    float4 viewPos = uniforms.viewMatrix * float4(in.position, 1.0f);
    out.position = uniforms.projectionMatrix * viewPos;
    out.position.z = 0.5f * (out.position.z + out.position.w);

    float viewDepth = max(0.0f, -viewPos.z);
    float depthFade = clamp(650.0f / max(1.0f, viewDepth), 0.18f, 1.0f);

    float phase = in.position.x * 0.1f + in.position.y * 0.15f + in.position.z * 0.05f;
    float noise = hash21(in.position.xy * 0.01f) * 0.5f;
    float twinkle = 0.70f + 0.30f * sin(uniforms.time * (1.6f + noise) + phase) * cos(uniforms.time * 0.85f + phase * 1.25f);

    float4 col = in.colour;
    if (uniforms.isLightMode > 0.5f) {
        col.rgb *= 0.85f;
    }

    // High Vibrancy & Emissive Saturation Boost
    float lum = dot(col.rgb, float3(0.2126f, 0.7152f, 0.0722f));
    float3 vibrantCol = mix(float3(lum), col.rgb, 1.48f);
    vibrantCol = mix(float3(0.5f), vibrantCol, 1.18f);

    // Atmospheric Flight Fog
    float fogDensity = 0.00032f;
    float fogFactor = clamp(exp(-viewDepth * fogDensity), 0.0f, 1.0f);
    float3 foggedRGB = mix(uniforms.fogColor, vibrantCol, fogFactor);

    // ACES Filmic Tone Mapping with vivid highlight luminance
    float3 filmicRGB = filmicToneMap(foggedRGB * 1.35f);

    col.a *= uniforms.revealAlpha * depthFade * twinkle;
    out.colour = float4(filmicRGB, col.a);

    // Scale point size for high-res Retina display
    float baseSize = max(3.2f, in.radius * 7500.0f / max(1.0f, viewDepth));
    out.pointSize = clamp(baseSize * uniforms.scaleFactor, 2.0f, 511.0f);
    return out;
}

// Point Cloud Fragment Shader (Triple-Layer Optical Bloom)
fragment float4 cloudFragmentShader(VertexOutput in [[stage_in]],
                                    float2 pointCoord [[point_coord]]) {
    float2 p = pointCoord * 2.0f - float2(1.0f);
    float r2 = dot(p, p);
    if (r2 >= 1.0f) {
        return float4(0.0f);
    }

    // Layer 1: Intense Diamond Specular Core
    float core = smoothstep(0.22f, 0.0f, r2);

    // Layer 2: Vibrant Chromatic Inner Glow
    float innerGlow = exp(-r2 * 2.8f) * 0.80f;

    // Layer 3: Wide Atmospheric Outer Halo
    float outerHalo = exp(-r2 * 0.85f) * 0.35f;

    // Perimeter Anti-Aliasing
    float edgeAntiAlias = smoothstep(1.0f, 0.82f, r2);

    float alpha = (core * 1.4f + innerGlow + outerHalo) * edgeAntiAlias;
    float3 emissiveCol = in.colour.rgb * (1.0f + core * 0.85f);

    return float4(emissiveCol, in.colour.a * clamp(alpha, 0.0f, 1.0f));
}
)";

} // namespace openwav

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

@interface CloudMetalDelegate : NSObject <MTKViewDelegate>
@property (nonatomic, assign) openwav::MetalCloudView::Impl* impl;
@end

namespace openwav {

class MetalCloudView::Impl {
public:
    id<MTLDevice> device { nil };
    id<MTLCommandQueue> commandQueue { nil };
    id<MTLRenderPipelineState> bgPipeline { nil };
    id<MTLRenderPipelineState> linePipeline { nil };
    id<MTLRenderPipelineState> pipelineStateLight { nil };
    id<MTLRenderPipelineState> pipelineStateDark { nil };
    PassThroughMTKView* mtkView { nil };
    CloudMetalDelegate* delegate { nil };

    id<MTLBuffer> vertexBuffer { nil };
    size_t vertexCount { 0 };
    id<MTLBuffer> lineVertexBuffer { nil };
    size_t lineVertexCount { 0 };

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
            lineVertexBuffer = nil;
            bgPipeline = nil;
            linePipeline = nil;
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

            // 1. Background Pipeline (Deep Space Nebula & Cosmic Dust)
            id<MTLFunction> bgVertexFunc = [library newFunctionWithName:@"bgVertexShader"];
            id<MTLFunction> bgFragmentFunc = [library newFunctionWithName:@"bgFragmentShader"];

            MTLRenderPipelineDescriptor* bgDesc = [[MTLRenderPipelineDescriptor alloc] init];
            bgDesc.vertexFunction = bgVertexFunc;
            bgDesc.fragmentFunction = bgFragmentFunc;
            bgDesc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
            bgDesc.colorAttachments[0].blendingEnabled = NO;

            bgPipeline = [device newRenderPipelineStateWithDescriptor:bgDesc error:&error];
            if (bgPipeline == nil || error != nil) {
                NSLog(@"[MetalCloudView] Error creating Background pipeline: %@", error.localizedDescription);
                return false;
            }

            // 2. Constellation Lines Pipeline (Additive Blending)
            id<MTLFunction> lineVertexFunc = [library newFunctionWithName:@"lineVertexShader"];
            id<MTLFunction> lineFragmentFunc = [library newFunctionWithName:@"lineFragmentShader"];

            MTLVertexDescriptor* lineVertexDesc = [[MTLVertexDescriptor alloc] init];
            lineVertexDesc.attributes[0].format = MTLVertexFormatFloat3;
            lineVertexDesc.attributes[0].offset = offsetof(MetalLineVertex, x);
            lineVertexDesc.attributes[0].bufferIndex = 0;
            lineVertexDesc.attributes[1].format = MTLVertexFormatFloat4;
            lineVertexDesc.attributes[1].offset = offsetof(MetalLineVertex, r);
            lineVertexDesc.attributes[1].bufferIndex = 0;
            lineVertexDesc.layouts[0].stride = sizeof(MetalLineVertex);
            lineVertexDesc.layouts[0].stepRate = 1;
            lineVertexDesc.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;

            MTLRenderPipelineDescriptor* lineDesc = [[MTLRenderPipelineDescriptor alloc] init];
            lineDesc.vertexFunction = lineVertexFunc;
            lineDesc.fragmentFunction = lineFragmentFunc;
            lineDesc.vertexDescriptor = lineVertexDesc;
            lineDesc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
            lineDesc.colorAttachments[0].blendingEnabled = YES;
            lineDesc.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
            lineDesc.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
            lineDesc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
            lineDesc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOne;
            lineDesc.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
            lineDesc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOne;

            linePipeline = [device newRenderPipelineStateWithDescriptor:lineDesc error:&error];
            if (linePipeline == nil || error != nil) {
                NSLog(@"[MetalCloudView] Error creating Line pipeline: %@", error.localizedDescription);
                return false;
            }

            // 3. Cloud Points Pipelines (Triple-Layer Optical Bloom)
            id<MTLFunction> vertexFunc = [library newFunctionWithName:@"cloudVertexShader"];
            id<MTLFunction> fragmentFunc = [library newFunctionWithName:@"cloudFragmentShader"];

            MTLVertexDescriptor* vertexDesc = [[MTLVertexDescriptor alloc] init];
            vertexDesc.attributes[0].format = MTLVertexFormatFloat3;
            vertexDesc.attributes[0].offset = offsetof(MetalVertex, x);
            vertexDesc.attributes[0].bufferIndex = 0;
            vertexDesc.attributes[1].format = MTLVertexFormatFloat4;
            vertexDesc.attributes[1].offset = offsetof(MetalVertex, r);
            vertexDesc.attributes[1].bufferIndex = 0;
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
                NSLog(@"[MetalCloudView] Error creating Light pipeline: %@", error.localizedDescription);
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
                NSLog(@"[MetalCloudView] Error creating Dark pipeline: %@", error.localizedDescription);
                return false;
            }

            // Setup MTKView with native Retina backing store
            CGFloat initialScale = [NSScreen mainScreen].backingScaleFactor;
            if (initialScale < 1.0) initialScale = 1.0;

            mtkView = [[PassThroughMTKView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) device:device];
            mtkView.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
            mtkView.clearColor = clearColor;
            mtkView.framebufferOnly = YES;
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

        MetalUniforms uniforms;
        id<MTLBuffer> vBuf = nil;
        size_t vCount = 0;
        id<MTLBuffer> lBuf = nil;
        size_t lCount = 0;
        {
            std::lock_guard<std::mutex> lock(renderMutex);
            if (vertexBuffer == nil || vertexCount == 0) {
                return;
            }
            uniforms = currentUniforms;
            vBuf = vertexBuffer;
            vCount = vertexCount;
            lBuf = lineVertexBuffer;
            lCount = lineVertexCount;
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

            // 1. Draw GPU Native Deep Space Nebula & Cosmic Dust (0.01ms on GPU)
            if (bgPipeline != nil) {
                [encoder setRenderPipelineState:bgPipeline];
                [encoder setFragmentBytes:&uniforms length:sizeof(uniforms) atIndex:1];
                [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
            }

            // 2. Draw Constellation Energy Filaments
            if (linePipeline != nil && lBuf != nil && lCount > 0) {
                [encoder setRenderPipelineState:linePipeline];
                [encoder setVertexBuffer:lBuf offset:0 atIndex:0];
                [encoder setVertexBytes:&uniforms length:sizeof(uniforms) atIndex:1];
                [encoder drawPrimitives:MTLPrimitiveTypeLine vertexStart:0 vertexCount:lCount];
            }

            // 3. Draw Point Cloud Particles (Triple-Layer Optical Bloom)
            id<MTLRenderPipelineState> pipeline = (uniforms.isLightMode > 0.5f) ? pipelineStateLight : pipelineStateDark;
            [encoder setRenderPipelineState:pipeline];
            [encoder setVertexBuffer:vBuf offset:0 atIndex:0];
            [encoder setVertexBytes:&uniforms length:sizeof(uniforms) atIndex:1];
            [encoder drawPrimitives:MTLPrimitiveTypePoint vertexStart:0 vertexCount:vCount];

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
        pImpl->vertexBuffer = [pImpl->device newBufferWithLength:dataSize
                                                         options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined];
    }

    if (pImpl->vertexBuffer != nil) {
        memcpy([pImpl->vertexBuffer contents], vertices.data(), dataSize);
    }
}

void MetalCloudView::updateLines(const std::vector<MetalLineVertex>& lines) {
    if (!pImpl || !pImpl->device) return;

    std::lock_guard<std::mutex> lock(pImpl->renderMutex);
    pImpl->lineVertexCount = lines.size();
    if (lines.empty()) {
        pImpl->lineVertexBuffer = nil;
        return;
    }

    size_t dataSize = lines.size() * sizeof(MetalLineVertex);
    if (pImpl->lineVertexBuffer == nil || [pImpl->lineVertexBuffer length] < dataSize) {
        pImpl->lineVertexBuffer = [pImpl->device newBufferWithLength:dataSize
                                                             options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined];
    }

    if (pImpl->lineVertexBuffer != nil) {
        memcpy([pImpl->lineVertexBuffer contents], lines.data(), dataSize);
    }
}

void MetalCloudView::updateUniforms(const MetalUniforms& uniforms) {
    if (!pImpl) return;
    std::lock_guard<std::mutex> lock(pImpl->renderMutex);
    pImpl->currentUniforms = uniforms;
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
