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

// Resolution presets
enum class ResolutionPreset {
    Resolution360p,  // 640x360
    Resolution480p,  // 640x480
    Resolution540p,  // 960x540
    Resolution720p,  // 1280x720
    Resolution1080p, // 1920x1080
    Resolution4K     // 3840x2160
};

// Frame rate presets
enum class FrameRatePreset {
    FrameRate10fps, // 10 fps
    FrameRate15fps, // 15 fps
    FrameRate30fps, // 30 fps
    FrameRate60fps, // 60 fps
    FrameRate120fps // 120 fps
};

struct ResolutionInfo {
    int width;
    int height;
    const char* name;
};

struct FrameRateInfo {
    int fps;
    const char* name;
};

static const ResolutionInfo kResolutions[] = {
    { 640, 360, "360p" },
    { 640, 480, "480p" },
    { 960, 540, "540p" },
    { 1280, 720, "720p" },
    { 1920, 1080, "1080p" },
    { 3840, 2160, "4K" }
};

static const FrameRateInfo kFrameRates[] = {
    { 10, "10fps" },
    { 15, "15fps" },
    { 30, "30fps" },
    { 60, "60fps" },
    { 120, "120fps" }
};

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
@property (weak, nonatomic) IBOutlet UIScrollView *scrollView;

@property (strong, nonatomic) UIButton* resolutionBtn;
@property (strong, nonatomic) UIButton* frameRateBtn;
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

    ResolutionPreset _currentResolution;
    FrameRatePreset _currentFrameRate;
}

- (BOOL)configureProvider:(ccap::Provider&)provider
           withResolution:(ResolutionPreset)resolution
                frameRate:(FrameRatePreset)frameRate {
    const ResolutionInfo& resInfo = kResolutions[(int)resolution];
    const FrameRateInfo& fpsInfo = kFrameRates[(int)frameRate];

    provider.set(ccap::PropertyName::PixelFormatInternal, ccap::PixelFormat::NV12f);
    provider.set(ccap::PropertyName::PixelFormatOutput, ccap::PixelFormat::NV12f);
    provider.set(ccap::PropertyName::Width, resInfo.width);
    provider.set(ccap::PropertyName::Height, resInfo.height);

    // Set frame rate
    provider.set(ccap::PropertyName::FrameRate, fpsInfo.fps);

    NSLog(@"Configured provider for %s (%dx%d) at %s",
          resInfo.name, resInfo.width, resInfo.height, fpsInfo.name);

    return YES;
}

- (BOOL)openCamera:(int)cameraIndex withResolution:(ResolutionPreset)resolution frameRate:(FrameRatePreset)frameRate {
    [self configureProvider:_provider withResolution:resolution frameRate:frameRate];

    // Try to open with desired resolution and frame rate
    if (_provider.open(cameraIndex, true)) {
        return YES;
    }

    return NO;
}

- (void)updateResolutionButtonTitle {
    [self updateResolutionButtonTitle:NO];
}

- (void)updateResolutionButtonTitle:(BOOL)showError {
    if (_resolutionBtn) {
        const ResolutionInfo& resInfo = kResolutions[(int)_currentResolution];
        NSString* title = [NSString stringWithFormat:@"%s", resInfo.name];

        // 根据传入的参数决定是否显示错误状态
        if (showError) {
            title = [NSString stringWithFormat:@"❌%s", resInfo.name];
        }

        [_resolutionBtn setTitle:title forState:UIControlStateNormal];
    }
}

- (void)updateFrameRateButtonTitle {
    [self updateFrameRateButtonTitle:NO];
}

- (void)updateFrameRateButtonTitle:(BOOL)showError {
    if (_frameRateBtn) {
        const FrameRateInfo& fpsInfo = kFrameRates[(int)_currentFrameRate];
        NSString* title = [NSString stringWithFormat:@"%s", fpsInfo.name];

        // 根据传入的参数决定是否显示错误状态
        if (showError) {
            title = [NSString stringWithFormat:@"❌%s", fpsInfo.name];
        }

        [_frameRateBtn setTitle:title forState:UIControlStateNormal];
    }
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
    _currentResolution = ResolutionPreset::Resolution1080p;
    _currentFrameRate = FrameRatePreset::FrameRate30fps;

    // Setup resolution button
    [self setupResolutionButton];

    // Setup frame rate button
    [self setupFrameRateButton];

    __weak __typeof(self) weakSelf = self;
    _provider.setNewFrameCallback([=](const std::shared_ptr<ccap::VideoFrame>& newFrame) {
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        dispatch_async(dispatch_get_main_queue(), ^{
            [strongSelf.mtkView setNeedsDisplay];
        });
        return false;
    });

    // Open camera with configured resolution and frame rate
    BOOL cameraOpenSuccess = NO;
    if ([self openCamera:_cameraIndex withResolution:_currentResolution frameRate:_currentFrameRate]) {
        cameraOpenSuccess = YES;
    } else {
        // 如果初始设置失败，尝试 720p@30fps 作为默认
        _currentResolution = ResolutionPreset::Resolution720p;
        _currentFrameRate = FrameRatePreset::FrameRate30fps;
        if ([self openCamera:_cameraIndex withResolution:_currentResolution frameRate:_currentFrameRate]) {
            cameraOpenSuccess = YES;
        } else {
            NSLog(@"Failed to open camera with any resolution and frame rate");
        }
    }
    // 更新按钮标题以反映实际状态
    [self updateResolutionButtonTitle:!cameraOpenSuccess];
    [self updateFrameRateButtonTitle:!cameraOpenSuccess];
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

- (void)setupResolutionButton {
    // Create resolution button programmatically
    _resolutionBtn = [UIButton buttonWithType:UIButtonTypeSystem];
    _resolutionBtn.translatesAutoresizingMaskIntoConstraints = NO;
    [_resolutionBtn setTitle:@"1080p" forState:UIControlStateNormal];
    [_resolutionBtn setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    _resolutionBtn.backgroundColor = [[UIColor blackColor] colorWithAlphaComponent:0.5];
    _resolutionBtn.layer.cornerRadius = 8;
    _resolutionBtn.titleLabel.font = [UIFont boldSystemFontOfSize:16];
    [_resolutionBtn addTarget:self action:@selector(switchResolution:) forControlEvents:UIControlEventTouchUpInside];

    [self.scrollView addSubview:_resolutionBtn];

    // 先临时设置一个简单的位置，稍后在 viewDidLayoutSubviews 中更新
    [NSLayoutConstraint activateConstraints:@[
        [_resolutionBtn.leadingAnchor constraintEqualToAnchor:self.scrollView.leadingAnchor constant:20],
        [_resolutionBtn.topAnchor constraintEqualToAnchor:self.scrollView.topAnchor constant:20],
        [_resolutionBtn.widthAnchor constraintEqualToConstant:80],
        [_resolutionBtn.heightAnchor constraintEqualToConstant:40]
    ]];
}

- (void)setupFrameRateButton {
    // Create frame rate button programmatically
    _frameRateBtn = [UIButton buttonWithType:UIButtonTypeSystem];
    _frameRateBtn.translatesAutoresizingMaskIntoConstraints = NO;
    [_frameRateBtn setTitle:@"30fps" forState:UIControlStateNormal];
    [_frameRateBtn setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
    _frameRateBtn.backgroundColor = [[UIColor blackColor] colorWithAlphaComponent:0.5];
    _frameRateBtn.layer.cornerRadius = 8;
    _frameRateBtn.titleLabel.font = [UIFont boldSystemFontOfSize:16];
    [_frameRateBtn addTarget:self action:@selector(switchFrameRate:) forControlEvents:UIControlEventTouchUpInside];

    [self.scrollView addSubview:_frameRateBtn];

    // 先临时设置一个简单的位置，稍后在 viewDidLayoutSubviews 中更新
    [NSLayoutConstraint activateConstraints:@[
        [_frameRateBtn.leadingAnchor constraintEqualToAnchor:self.scrollView.leadingAnchor constant:110], // 在分辨率按钮右边
        [_frameRateBtn.topAnchor constraintEqualToAnchor:self.scrollView.topAnchor constant:20],
        [_frameRateBtn.widthAnchor constraintEqualToConstant:80],
        [_frameRateBtn.heightAnchor constraintEqualToConstant:40]
    ]];
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

    // 保存当前状态，以便失败时回退
    int previousCameraIndex = _cameraIndex;
    ResolutionPreset previousResolution = _currentResolution;

    _cameraIndex = (_cameraIndex + 1) % _allCameraNames.size();
    _provider.close();

    auto name = _allCameraNames[_cameraIndex];
    if (![self openCamera:_cameraIndex withResolution:_currentResolution frameRate:_currentFrameRate]) {
        // 切换失败，尝试回退到之前的摄像头
        _cameraIndex = previousCameraIndex;
        if (![self openCamera:_cameraIndex withResolution:_currentResolution frameRate:_currentFrameRate]) {
            // 连之前的摄像头都无法打开
            [self updateResolutionButtonTitle:YES]; // 显示错误状态，表示切换失败
            [self updateFrameRateButtonTitle:YES];
            NSLog(@"Failed to switch camera and failed to revert to previous camera");
            return;
        }
        [self updateResolutionButtonTitle:YES];
        [self updateFrameRateButtonTitle:YES];
        NSLog(@"Failed to switch to camera: %s, reverted to previous camera", name.c_str());
        return;
    }

    // 成功切换，更新按钮标题
    [self updateResolutionButtonTitle:NO]; // 正常状态
    [self updateFrameRateButtonTitle:NO]; // 正常状态

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

- (IBAction)switchResolution:(id)sender {
    // 保存当前状态，以便失败时回退
    ResolutionPreset previousResolution = _currentResolution;

    // Cycle through resolutions
    int currentIndex = (int)_currentResolution;
    int nextIndex = (currentIndex + 1) % (sizeof(kResolutions) / sizeof(kResolutions[0]));
    ResolutionPreset newResolution = (ResolutionPreset)nextIndex;

    // Close current provider
    _provider.close();

    // Try to open with new resolution
    if ([self openCamera:_cameraIndex withResolution:newResolution frameRate:_currentFrameRate]) {
        _currentResolution = newResolution;
        [self updateResolutionButtonTitle:NO]; // 成功切换，正常状态

        const ResolutionInfo& resInfo = kResolutions[(int)_currentResolution];
        NSLog(@"Switched to resolution: %s (%dx%d)", resInfo.name, resInfo.width, resInfo.height);

        // Reset texture objects to accommodate new resolution
        if (_yTexture != nil) {
            _yTexture = nil;
            _uvTexture = nil;
            _textureWidth = 0;
            _textureHeight = 0;
        }
    } else {
        // 切换失败，回退到之前的分辨率
        _currentResolution = previousResolution;
        if ([self openCamera:_cameraIndex withResolution:_currentResolution frameRate:_currentFrameRate]) {
            // 成功恢复到之前的分辨率，但显示错误状态表示切换失败
            [self updateResolutionButtonTitle:YES];
            const ResolutionInfo& failedResInfo = kResolutions[(int)newResolution];
            const ResolutionInfo& currentResInfo = kResolutions[(int)_currentResolution];
            NSLog(@"Failed to switch to %s, reverted to %s", failedResInfo.name, currentResInfo.name);
        } else {
            // 连之前的分辨率都无法打开，显示错误状态
            [self updateResolutionButtonTitle:YES];
            NSLog(@"Failed to switch resolution and failed to revert to previous setting");
        }
    }
}

- (IBAction)switchFrameRate:(id)sender {
    // 保存当前状态，以便失败时回退
    FrameRatePreset previousFrameRate = _currentFrameRate;

    // Cycle through frame rates
    int currentIndex = (int)_currentFrameRate;
    int nextIndex = (currentIndex + 1) % (sizeof(kFrameRates) / sizeof(kFrameRates[0]));
    FrameRatePreset newFrameRate = (FrameRatePreset)nextIndex;

    // Close current provider
    _provider.close();

    // Try to open with new frame rate
    if ([self openCamera:_cameraIndex withResolution:_currentResolution frameRate:newFrameRate]) {
        _currentFrameRate = newFrameRate;
        [self updateFrameRateButtonTitle:NO]; // 成功切换，正常状态

        const FrameRateInfo& frameRateInfo = kFrameRates[(int)_currentFrameRate];
        NSLog(@"Switched to frame rate: %s (%d fps)", frameRateInfo.name, frameRateInfo.fps);

        // Reset texture objects to accommodate new frame rate
        if (_yTexture != nil) {
            _yTexture = nil;
            _uvTexture = nil;
            _textureWidth = 0;
            _textureHeight = 0;
        }
    } else {
        // 切换失败，回退到之前的帧率
        _currentFrameRate = previousFrameRate;
        if ([self openCamera:_cameraIndex withResolution:_currentResolution frameRate:_currentFrameRate]) {
            // 成功恢复到之前的帧率，但显示错误状态表示切换失败
            [self updateFrameRateButtonTitle:YES];
            const FrameRateInfo& failedFrameRateInfo = kFrameRates[(int)newFrameRate];
            const FrameRateInfo& currentFrameRateInfo = kFrameRates[(int)_currentFrameRate];
            NSLog(@"Failed to switch to %s, reverted to %s", failedFrameRateInfo.name, currentFrameRateInfo.name);
        } else {
            // 连之前的帧率都无法打开，显示错误状态
            [self updateFrameRateButtonTitle:YES];
            NSLog(@"Failed to switch frame rate and failed to revert to previous setting");
        }
    }
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

    // 清理按钮
    if (_resolutionBtn) {
        [_resolutionBtn removeFromSuperview];
        _resolutionBtn = nil;
    }
    
    if (_frameRateBtn) {
        [_frameRateBtn removeFromSuperview];
        _frameRateBtn = nil;
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

    float scaling = std::max(drawableSize.width / srcWidth, drawableSize.height / srcHeight);

    int viewportWidth = scaling * srcWidth;
    int viewportHeight = scaling * srcHeight;

    id<MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];
    id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];

    // 设置视口 - 居中并填满屏幕
    MTLViewport viewport = {
        .originX = (drawableSize.width - viewportWidth) / 2.0,
        .originY = (drawableSize.height - viewportHeight) / 2.0,
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

    // Update scrollView contentSize and button positions after layout
    if (self.scrollView && _resolutionBtn && _frameRateBtn) {
        CGSize scrollViewSize = self.scrollView.bounds.size;
        NSLog(@"ScrollView size: %@", NSStringFromCGSize(scrollViewSize));

        if (scrollViewSize.width > 0 && scrollViewSize.height > 0) {
            // 设置 contentSize，留出足够的空间容纳两个按钮
            self.scrollView.contentSize = CGSizeMake(200, scrollViewSize.height);

            // 设置分辨率按钮和帧率按钮位置
            _resolutionBtn.frame = CGRectMake(10, 0, 80, scrollViewSize.height);
            _resolutionBtn.translatesAutoresizingMaskIntoConstraints = YES;
            
            _frameRateBtn.frame = CGRectMake(100, 0, 80, scrollViewSize.height);
            _frameRateBtn.translatesAutoresizingMaskIntoConstraints = YES;

            NSLog(@"Resolution button frame: %@", NSStringFromCGRect(_resolutionBtn.frame));
            NSLog(@"Frame rate button frame: %@", NSStringFromCGRect(_frameRateBtn.frame));
        }
    }
}
@end
