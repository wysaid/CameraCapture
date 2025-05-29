//
//  ExampleVC_metal.mm
//  ccapDemo
//
//  Created by wy on 2025/05
//

#import "ExampleVC_metal.h"

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#include <ccap/ccap.h>

#pragma GCC diagnostic ignored "-Wimplicit-retain-self"

constexpr static const char* kMetalShaderSource = R"(
#include <metal_stdlib>
using namespace metal;

struct VertexIn {
    float2 position [[attribute(0)]];
};

struct VertexOut {
    float4 position [[position]];
    float2 texCoord;
};

struct VertexUniforms {
    float2 flipScale;
    float rotation;
};

constant float angle = 10.0;

float2x2 mat2ZRotation(float rad) {
    float cosRad = cos(rad);
    float sinRad = sin(rad);
    return float2x2(cosRad, sinRad, -sinRad, cosRad);
}

vertex VertexOut vertex_main(VertexIn in [[stage_in]],
                            constant VertexUniforms& uniforms [[buffer(1)]]) {
    VertexOut out;
    out.position = float4(in.position, 0.0, 1.0);
    out.texCoord = uniforms.flipScale * ((in.position / 2.0) * mat2ZRotation(uniforms.rotation)) + 0.5;
    return out;
}

fragment float4 fragment_main(VertexOut in [[stage_in]],
                             texture2d<float> yTexture [[texture(0)]],
                             texture2d<float> uvTexture [[texture(1)]],
                             constant float& progress [[buffer(0)]]) {
    constexpr sampler textureSampler(mag_filter::linear,
                                   min_filter::linear);
    
    /// Apply a wavy distortion to the texture coordinates based on the progress
    float2 newCoord;
    newCoord.x = in.texCoord.x + 0.01 * sin(progress +  in.texCoord.x * angle);
    newCoord.y = in.texCoord.y + 0.01 * sin(progress +  in.texCoord.y * angle);
    /// Avoid sampling too close to the edges
    float edge1 = min(in.texCoord.x, in.texCoord.y);
    float edge2 = max(in.texCoord.x, in.texCoord.y);
    if (edge1 < 0.05 || edge2 > 0.95)
    {
        float lenthToEdge = min(edge1, 1.0 - edge2) / 0.05;
        newCoord = mix(in.texCoord, newCoord, lenthToEdge);
    }

    float3 yuv;
    yuv.x = yTexture.sample(textureSampler, newCoord).r;
    yuv.yz = uvTexture.sample(textureSampler, newCoord).rg - float2(0.5, 0.5);
    
    // BT.601 Full Range
    float3x3 yuvToRgbMatrix = float3x3(
        float3(1.0,     1.0,     1.0),
        float3(0.0,    -0.34414, 1.772),
        float3(1.402,  -0.71414, 0.0)
    );
    
    float3 rgb = yuvToRgbMatrix * yuv;
    return float4(rgb, 1.0);
}
)";

struct alignas(16) VertexShaderUniforms
{
    simd_float2 flipScale;
    float rotation;
};

@interface ExampleVC_metal ()<MTKViewDelegate>
@property (nonatomic, strong) MTKView* mtkView;
@property (nonatomic, strong) id<MTLCommandQueue> commandQueue;
@property (nonatomic, strong) id<MTLDevice> device;

@property (weak, nonatomic) IBOutlet UIButton* backBtn;
@end

@implementation ExampleVC_metal
{
    ccap::Provider _provider;

    id<MTLTexture> _yTexture, _uvTexture;
    id<MTLBuffer> _vertexBuffer;
    MTLVertexDescriptor* _vertexDescriptor;
    id<MTLRenderPipelineState> _pipeline;

    int _textureWidth;
    int _textureHeight;

    std::vector<std::string> _allCameraNames;
    int _cameraIndex;

    VertexShaderUniforms _uniforms;
    float _progress;
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    // ccap::setLogLevel(ccap::LogLevel::Verbose);

    _allCameraNames = _provider.findDeviceNames();
    for (auto& name : _allCameraNames)
    {
        NSLog(@"Find Camera Device: %s", name.c_str());
    }

    [self setupMetal];

    _cameraIndex = 0;

    _provider.set(ccap::PropertyName::PixelFormatInternal, ccap::PixelFormat::NV12f);
    _provider.set(ccap::PropertyName::PixelFormatOutput, ccap::PixelFormat::NV12f);
    _provider.set(ccap::PropertyName::Width, 1920);
    _provider.set(ccap::PropertyName::Height, 1080);

    __weak __typeof(self) weakSelf = self;
    _provider.setNewFrameCallback([=](const std::shared_ptr<ccap::VideoFrame>& newFrame) {
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        dispatch_async(dispatch_get_main_queue(), ^{
            [strongSelf.mtkView setNeedsDisplay];
        });
        return false;
    });

    if (!_provider.open(_cameraIndex, true))
    {
        _provider.close();
        _provider.set(ccap::PropertyName::Width, 1280);
        _provider.set(ccap::PropertyName::Height, 720);
        _provider.open(_cameraIndex, true);
    }
}

- (void)setupMetal
{
    if (_device)
        return;

    _device = MTLCreateSystemDefaultDevice();
    _commandQueue = [_device newCommandQueue];

    const float vertData[8] = { -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f };
    /// StaticDraw, no need to track.
    MTLResourceOptions options{};
#if CCAP_IOS
    options = MTLResourceStorageModeShared | MTLResourceHazardTrackingModeUntracked;
#else
    options = MTLResourceCPUCacheModeDefaultCache | MTLResourceStorageModeManaged | MTLResourceHazardTrackingModeUntracked;
#endif

    /// Vertex setup
    _vertexBuffer = [_device newBufferWithBytes:vertData
                                         length:sizeof(vertData)
                                        options:options];
    _vertexDescriptor = [[MTLVertexDescriptor alloc] init];
    auto* attrib0 = _vertexDescriptor.attributes[0];
    attrib0.bufferIndex = 0;
    attrib0.format = MTLVertexFormatFloat2;
    attrib0.offset = 0;

    // 添加顶点布局描述
    auto* layout0 = _vertexDescriptor.layouts[0];
    layout0.stepFunction = MTLVertexStepFunctionPerVertex;
    layout0.stepRate = 1;
    layout0.stride = sizeof(float) * 2;

    /// Pipeline setup
    NSError* error = nil;
    id<MTLLibrary> library = [_device newLibraryWithSource:[NSString stringWithUTF8String:kMetalShaderSource]
                                                   options:nil
                                                     error:&error];
    if (error)
    {
        NSLog(@"Error creating library: %@", error.localizedDescription);
        return;
    }

    id<MTLFunction> vertexFunction = [library newFunctionWithName:@"vertex_main"];
    id<MTLFunction> fragmentFunction = [library newFunctionWithName:@"fragment_main"];

    MTLRenderPipelineDescriptor* pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
    pipelineDescriptor.vertexFunction = vertexFunction;
    pipelineDescriptor.fragmentFunction = fragmentFunction;
    pipelineDescriptor.vertexDescriptor = _vertexDescriptor;
    pipelineDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;

    _pipeline = [_device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error];
    if (error)
    {
        NSLog(@"Error creating pipeline: %@", error.localizedDescription);
        return;
    }

    /// MTLView setup
    _mtkView = [[MTKView alloc] initWithFrame:self.view.bounds device:_device];
    [_mtkView setBackgroundColor:[UIColor clearColor]];
    [_mtkView setDelegate:self];
    [_mtkView setEnableSetNeedsDisplay:YES];  // 启用手动渲染模式
    [_mtkView setAutoResizeDrawable:YES];

    _uniforms.flipScale = simd_make_float2(-1.0f, 1.0f); // Y轴翻转
    _uniforms.rotation = M_PI_2;
    _progress = 0.0f;

    /// 手动刷新模式，暂停自动渲染
    [_mtkView setPaused:YES];
    [self.view insertSubview:_mtkView atIndex:0];
}

- (IBAction)backClicked:(id)sender
{
    [self clear];
    [self dismissViewControllerAnimated:YES completion:nil];
}

- (IBAction)switchCamera:(id)sender
{
    if (_allCameraNames.size() <= 1)
        return;

    _cameraIndex = (_cameraIndex + 1) % _allCameraNames.size();
    _provider.close();

    auto name = _allCameraNames[_cameraIndex];
    if (!_provider.open(name, true))
    {
        _provider.close();
        _provider.set(ccap::PropertyName::Width, 1280);
        _provider.set(ccap::PropertyName::Height, 720);
        _provider.open(name, true);
    }

    // name 里面包含 back 或者 fron 代表前后摄像头 (忽略大小写比较)
    std::transform(name.begin(), name.end(), name.begin(), ::tolower);
    if (name.find("back") != std::string::npos || name.find("rear") != std::string::npos)
    {
        _uniforms.flipScale = simd_make_float2(-1.0f, 1.0f);
        _uniforms.rotation = M_PI_2;
    }
    else
    {
        _uniforms.flipScale = simd_make_float2(-1.0f, -1.0f);
        _uniforms.rotation = M_PI_2;
    }
    NSLog(@"Switched to camera: %s", name.c_str());
}

- (void)clear
{
    if (_device == nil)
        return;

    if (_provider.isOpened())
    {
        _provider.setNewFrameCallback(nullptr);
        _provider.close();
    }

    // 清理 Metal 资源
    _yTexture = nil;
    _uvTexture = nil;
    _vertexBuffer = nil;
    _vertexDescriptor = nil;
    _pipeline = nil;
    _commandQueue = nil;

    // 清理 MTKView
    if (_mtkView)
    {
        [_mtkView setDelegate:nil];
        [_mtkView removeFromSuperview];
        _mtkView = nil;
    }

    _device = nil;
}

- (void)updateFrame
{
    auto newFrame = _provider.grab(30);
    if (!newFrame)
        return;

    if (_yTexture == nil || _textureWidth != newFrame->width || _textureHeight != newFrame->height)
    {
        // 重新创建 texture
        _yTexture = nil;
        _uvTexture = nil;

        _textureWidth = newFrame->width;
        _textureHeight = newFrame->height;

        // 创建Y纹理描述符
        MTLTextureDescriptor* yTextureDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm
                                                                                                      width:_textureWidth
                                                                                                     height:_textureHeight
                                                                                                  mipmapped:NO];
        yTextureDescriptor.usage = MTLTextureUsageShaderRead;
        _yTexture = [_device newTextureWithDescriptor:yTextureDescriptor];

        // 创建UV纹理描述符
        MTLTextureDescriptor* uvTextureDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRG8Unorm
                                                                                                       width:_textureWidth / 2
                                                                                                      height:_textureHeight / 2
                                                                                                   mipmapped:NO];
        uvTextureDescriptor.usage = MTLTextureUsageShaderRead;
        _uvTexture = [_device newTextureWithDescriptor:uvTextureDescriptor];

        // 上传Y平面数据
        MTLRegion yRegion = MTLRegionMake2D(0, 0, _textureWidth, _textureHeight);
        [_yTexture replaceRegion:yRegion
                     mipmapLevel:0
                       withBytes:newFrame->data[0]
                     bytesPerRow:_textureWidth];

        // 上传UV平面数据
        MTLRegion uvRegion = MTLRegionMake2D(0, 0, _textureWidth / 2, _textureHeight / 2);
        [_uvTexture replaceRegion:uvRegion
                      mipmapLevel:0
                        withBytes:newFrame->data[1]
                      bytesPerRow:_textureWidth];
    }
    else
    {
        // 更新已存在的纹理
        MTLRegion yRegion = MTLRegionMake2D(0, 0, _textureWidth, _textureHeight);
        [_yTexture replaceRegion:yRegion
                     mipmapLevel:0
                       withBytes:newFrame->data[0]
                     bytesPerRow:_textureWidth];

        MTLRegion uvRegion = MTLRegionMake2D(0, 0, _textureWidth / 2, _textureHeight / 2);
        [_uvTexture replaceRegion:uvRegion
                      mipmapLevel:0
                        withBytes:newFrame->data[1]
                      bytesPerRow:_textureWidth];
    }
}

- (void)mtkView:(MTKView*)mtkView drawableSizeWillChange:(CGSize)size
{
    /// resize, nothing to do here.
}

- (void)drawInMTKView:(MTKView*)mtkView
{
    [self updateFrame];

    // 检查必要的资源是否存在
    if (_yTexture == nil || _pipeline == nil)
    {
        NSLog(@"Init failed, skip render...");
        return;
    }

    id<MTLDrawable> drawable = [mtkView currentDrawable];
    if (!drawable)
        return;

    MTLRenderPassDescriptor* renderPassDescriptor = [mtkView currentRenderPassDescriptor];
    if (!renderPassDescriptor)
        return;

    // 清屏为黑色.
    renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);
    renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;

    CGSize drawableSize = mtkView.drawableSize;
#ifdef CCAP_IOS
    auto srcWidth = _textureHeight;
    auto srcHeight = _textureWidth;
#else
    auto srcWidth = _textureWidth;
    auto srcHeight = _textureHeight;
#endif

    CGFloat scaling = std::min(drawableSize.width / srcWidth, drawableSize.height / srcHeight);

    int viewportWidth = scaling * srcWidth;
    int viewportHeight = scaling * srcHeight;

    id<MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];
    id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];

    // 设置视口
    MTLViewport viewport = {
        .originX = 0.0,
        .originY = 0.0,
        .width = (double)viewportWidth,
        .height = (double)viewportHeight,
        .znear = 0.0,
        .zfar = 1.0
    };
    [renderEncoder setViewport:viewport];

    [renderEncoder setRenderPipelineState:_pipeline];
    [renderEncoder setVertexBuffer:_vertexBuffer offset:0 atIndex:0];

    [renderEncoder setVertexBytes:&_uniforms length:sizeof(_uniforms) atIndex:1];

    _progress = fmod(CFAbsoluteTimeGetCurrent(), M_PI * 2.0) * 3.0;
    [renderEncoder setFragmentBytes:&_progress length:sizeof(_progress) atIndex:0];

    [renderEncoder setFragmentTexture:_yTexture atIndex:0];
    [renderEncoder setFragmentTexture:_uvTexture atIndex:1];

    [renderEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip
                      vertexStart:0
                      vertexCount:4];

    [renderEncoder endEncoding];
    [commandBuffer presentDrawable:drawable];
    [commandBuffer commit];
}

- (void)fixViewSize
{
    if (!CGSizeEqualToSize(_mtkView.bounds.size, self.view.bounds.size))
    {
        [_mtkView setFrame:CGRectMake(0, 0, self.view.bounds.size.width, self.view.bounds.size.height)];
    }
}

- (void)viewDidLayoutSubviews
{
    [super viewDidLayoutSubviews];
    [self fixViewSize];
}
@end
