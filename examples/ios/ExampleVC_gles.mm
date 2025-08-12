//
//  ExampleVC_gles.mm
//  ccapDemo
//
//  Created by wy on 2025/05
//

#import "ExampleVC_gles.h"

#import <GLKit/GLKit.h>
#import <OpenGLES/ES3/gl.h>
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

constexpr const char* kVertexShaderSource = R"(
#version 300 es

precision highp float;

in vec2 position;
out highp vec2 texCoord;

uniform vec2 flipScale;
uniform float rotation;

mat2 mat2ZRotation(float rad) {
    float cosRad = cos(rad);
    float sinRad = sin(rad);
    return mat2(cosRad, sinRad, -sinRad, cosRad);
}

void main()
{
    gl_Position = vec4(position, 0.0, 1.0);
    texCoord = flipScale * ((position / 2.0) * mat2ZRotation(rotation)) + 0.5;
}
)";

constexpr const char* kFragmentShaderSource = R"(
#version 300 es
precision highp float;
in highp vec2 texCoord;
out vec4 fragColor;
uniform sampler2D yTexture;
uniform sampler2D uvTexture;

uniform float progress;

const float angle = 10.0;

void main()
{
    /// Apply a wavy distortion to the texture coordinates based on the progress
    vec2 newCoord;
    newCoord.x = texCoord.x + 0.01 * sin(progress +  texCoord.x * angle);
    newCoord.y = texCoord.y + 0.01 * sin(progress +  texCoord.y * angle);
    /// Avoid sampling too close to the edges
    float edge1 = min(texCoord.x, texCoord.y);
    float edge2 = max(texCoord.x, texCoord.y);
    if (edge1 < 0.05 || edge2 > 0.95)
    {
        float lenthToEdge = min(edge1, 1.0 - edge2) / 0.05;
        newCoord = mix(texCoord, newCoord, vec2(lenthToEdge));
    }

    /// Normal YUV to RGB conversion
    vec3 yuv;
    yuv.x = texture(yTexture, newCoord).r;
    yuv.yz = texture(uvTexture, newCoord).ra - vec2(0.5, 0.5);
    // BT.601 Full Range
    vec3 rgb = mat3(
                // 1.0, 1.0, 1.0,
                // 0.0, -0.18732, 1.8556,
                // 1.57481, -0.46813, 0.0
                1.0,     1.0,     1.0,
                0.0,    -0.34414, 1.772,
                1.402,  -0.71414, 0.0
                ) * yuv;
    fragColor = vec4(rgb, 1.0);
}
)";

@interface ExampleVC_gles ()<GLKViewDelegate>
@property (weak, nonatomic) IBOutlet UIButton* backBtn;
@property (weak, nonatomic) IBOutlet UIScrollView* scrollView;

@property (nonatomic, strong) GLKView* glkView;
@property (strong, nonatomic) UIButton* resolutionBtn;
@property (strong, nonatomic) UIButton* frameRateBtn;
@end

@implementation ExampleVC_gles {
    ccap::Provider _provider;

    EAGLContext* _glContext;
    GLuint _yTexture, _uvTexture;
    GLuint _vertexBuffer;
    GLuint _program;
    GLint _progressUniformLocation;

    int _textureWidth;
    int _textureHeight;

    std::vector<std::string> _allCameraNames;
    int _cameraIndex;

    ResolutionPreset _currentResolution;
    FrameRatePreset _currentFrameRate;
    float _progress;
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

- (void)viewDidLoad {
    [super viewDidLoad];
    ccap::setLogLevel(ccap::LogLevel::Verbose);

    _allCameraNames = _provider.findDeviceNames();
    for (auto& name : _allCameraNames) {
        NSLog(@"Find Camera Device: %s", name.c_str());
    }

    _cameraIndex = 0;
    _currentResolution = ResolutionPreset::Resolution1080p;
    _currentFrameRate = FrameRatePreset::FrameRate30fps;

    // render
    CGRect rt = [[UIScreen mainScreen] bounds];
    _glContext = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
    _glkView = [[GLKView alloc] initWithFrame:rt context:_glContext];
    [_glkView setDrawableColorFormat:GLKViewDrawableColorFormatRGBA8888];
    [_glkView setEnableSetNeedsDisplay:NO];
    [_glkView setBackgroundColor:[UIColor clearColor]];
    [_glkView setDelegate:self];
    [self setupGLES];

    // Setup resolution button
    [self setupResolutionButton];

    // Setup frame rate button
    [self setupFrameRateButton];

    // camera setup with callback
    __weak __typeof(self) weakSelf = self;
    _provider.setNewFrameCallback([=](const std::shared_ptr<ccap::VideoFrame>& newFrame) {
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        //        NSLog(@"New frame received, resolution: %dx%d, format: %s",
        //              newFrame->width, newFrame->height, ccap::pixelFormatToString(newFrame->pixelFormat).data());
        dispatch_async(dispatch_get_main_queue(), ^{
            [strongSelf.glkView display];
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

    [self.view insertSubview:_glkView atIndex:0];
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

- (GLuint)loadShader:(const char*)shaderSource type:(GLenum)type {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &shaderSource, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        NSLog(@"Shader compilation failed: %s", infoLog);
        return 0;
    }
    return shader;
}

- (void)checkGLError {
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        NSLog(@"❌OpenGL Error: %d", error);
    }
}

- (void)setupGLES {
    [EAGLContext setCurrentContext:_glContext];

    const float vertData[8] = { -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f };
    glGenBuffers(1, &_vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, _vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertData), vertData, GL_STATIC_DRAW);

    [self checkGLError];

    GLuint vertexShader = [self loadShader:kVertexShaderSource type:GL_VERTEX_SHADER];
    GLuint fragmentShader = [self loadShader:kFragmentShaderSource type:GL_FRAGMENT_SHADER];
    if (vertexShader == 0 || fragmentShader == 0) {
        NSLog(@"Failed to load shaders");
        return;
    }

    _program = glCreateProgram();
    glAttachShader(_program, vertexShader);
    glAttachShader(_program, fragmentShader);
    glBindAttribLocation(_program, 0, "position");
    glLinkProgram(_program);
    GLint success;
    glGetProgramiv(_program, GL_LINK_STATUS, &success);

    if (!success) {
        GLchar infoLog[512];
        glGetProgramInfoLog(_program, 512, nullptr, infoLog);
        NSLog(@"❌Program linking failed: %s", infoLog);
        glDeleteProgram(_program);
        _program = 0;
        return;
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    glUseProgram(_program);
    glUniform1i(glGetUniformLocation(_program, "yTexture"), 0);
    glUniform1i(glGetUniformLocation(_program, "uvTexture"), 1);
    _progressUniformLocation = glGetUniformLocation(_program, "progress");
    if (_progressUniformLocation == -1) {
        NSLog(@"Uniform 'progress' not found in shader program");
    }

    [self setFlipScale:-1.0f y:1.0f];
    [self setRotation:M_PI_2];
    [self checkGLError];
}

- (void)setFlipScale:(float)x y:(float)y {
    [EAGLContext setCurrentContext:_glContext];
    glUseProgram(_program);
    GLint flipScaleLocation = glGetUniformLocation(_program, "flipScale");
    if (flipScaleLocation == -1) {
        NSLog(@"Uniform 'flipScale' not found in shader program");
        return;
    }
    glUniform2f(flipScaleLocation, x, y);
    [self checkGLError];
}

- (void)setRotation:(float)rotation {
    [EAGLContext setCurrentContext:_glContext];
    glUseProgram(_program);
    GLint rotationLocation = glGetUniformLocation(_program, "rotation");
    if (rotationLocation == -1) {
        NSLog(@"Uniform 'rotation' not found in shader program");
        return;
    }
    glUniform1f(rotationLocation, rotation);
    [self checkGLError];
}

- (void)updateFrame {
    [EAGLContext setCurrentContext:_glContext];
    auto newFrame = _provider.grab(0);
    if (!newFrame)
        return;
    if (_yTexture == 0 || _textureWidth != newFrame->width || _textureHeight != newFrame->height) {
        if (_yTexture != 0) {
            GLuint textures[2] = { _yTexture, _uvTexture };
            glDeleteTextures(2, textures);
        }

        GLuint textures[2];
        glGenTextures(2, textures);
        _yTexture = textures[0];
        _uvTexture = textures[1];
        _textureWidth = newFrame->width;
        _textureHeight = newFrame->height;

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, _yTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, _textureWidth, _textureHeight, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, newFrame->data[0]);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, _uvTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, _textureWidth / 2, _textureHeight / 2, 0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, newFrame->data[1]);
    } else {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, _yTexture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _textureWidth, _textureHeight, GL_LUMINANCE, GL_UNSIGNED_BYTE, newFrame->data[0]);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, _uvTexture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _textureWidth / 2, _textureHeight / 2, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, newFrame->data[1]);
    }

    [self checkGLError];
}

- (void)glkView:(GLKView*)view drawInRect:(CGRect)rect {
    [self updateFrame];

    [view bindDrawable];

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (_yTexture == 0 || _program == 0) {
        NSLog(@"Init failed, skip render...");
        return;
    }

    auto canvasWidth = (GLint)_glkView.drawableWidth;
    auto canvasHeight = (GLint)_glkView.drawableHeight;
#ifdef CCAP_IOS
    auto srcWidth = _textureHeight;
    auto srcHeight = _textureWidth;
#else
    auto srcWidth = _textureWidth;
    auto srcHeight = _textureHeight;
#endif

    float scaling = std::max(canvasWidth / (float)srcWidth, canvasHeight / (float)srcHeight);
    GLint viewportWidth = scaling * srcWidth;
    GLint viewportHeight = scaling * srcHeight;

    glViewport((canvasWidth - viewportWidth) / 2, 0, viewportWidth, viewportHeight);
    glUseProgram(_program);

    glUniform1f(_progressUniformLocation, _progress);
    _progress = fmod(CFAbsoluteTimeGetCurrent(), M_PI * 2.0) * 3.0;

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, _yTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, _uvTexture);

    glBindBuffer(GL_ARRAY_BUFFER, _vertexBuffer);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

- (IBAction)backClicked:(id)sender {
    [self clear];
    [self dismissViewControllerAnimated:YES completion:nil];
}

- (IBAction)switchCamera:(id)sender {
    if (_allCameraNames.size() <= 1)
        return;

    // 保存当前状态，以便失败时回退
    int previousCameraIndex = _cameraIndex;
    ResolutionPreset previousResolution = _currentResolution;

    _cameraIndex = (_cameraIndex + 1) % _allCameraNames.size();
    _provider.close();

    auto name = _allCameraNames[_cameraIndex];
    if (![self openCamera:_cameraIndex withResolution:_currentResolution frameRate:_currentFrameRate]) {
        // 切换失败，回退到之前的相机和分辨率
        NSLog(@"Failed to switch to camera: %s, reverting to previous camera", name.c_str());
        _cameraIndex = previousCameraIndex;
        _currentResolution = previousResolution;

        // 尝试恢复到之前的相机设置
        auto previousName = _allCameraNames[_cameraIndex];
        if ([self openCamera:_cameraIndex withResolution:_currentResolution frameRate:_currentFrameRate]) {
            NSLog(@"Successfully reverted to camera: %s", previousName.c_str());
            [self updateResolutionButtonTitle:YES]; // 显示错误状态，表示切换失败
        } else {
            // 连之前的设置也无法打开，显示错误状态
            NSLog(@"Failed to revert to previous camera settings");
            [self updateResolutionButtonTitle:YES];
        }
        return;
    }

    // 成功切换，更新按钮标题
    [self updateResolutionButtonTitle:NO]; // 正常状态
    [self updateFrameRateButtonTitle:NO]; // 正常状态

    // name 里面包含 back 或者 fron 代表前后摄像头 (忽略大小写比较)
    std::transform(name.begin(), name.end(), name.begin(), ::tolower);
    if (name.find("back") != std::string::npos || name.find("rear") != std::string::npos) {
        [self setFlipScale:-1.0f y:1.0f];
        [self setRotation:M_PI_2];
    } else {
        [self setFlipScale:-1.0f y:-1.0f];
        [self setRotation:M_PI_2];
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
        if (_yTexture != 0) {
            [EAGLContext setCurrentContext:_glContext];
            GLuint textures[2] = { _yTexture, _uvTexture };
            glDeleteTextures(2, textures);
            _yTexture = 0;
            _uvTexture = 0;
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
        if (_yTexture != 0) {
            [EAGLContext setCurrentContext:_glContext];
            GLuint textures[2] = { _yTexture, _uvTexture };
            glDeleteTextures(2, textures);
            _yTexture = 0;
            _uvTexture = 0;
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

- (void)dealloc {
    [self clear];
    NSLog(@"ExampleVC_gles dealloc");
}

- (void)fixViewSize {
    if (!CGSizeEqualToSize(_glkView.bounds.size, self.view.bounds.size)) {
        [_glkView setFrame:CGRectMake(0, 0, self.view.bounds.size.width, self.view.bounds.size.height)];
    }
}

- (void)viewDidLayoutSubviews {
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

- (void)clear {
    if (_provider.isOpened()) {
        _provider.setNewFrameCallback(nullptr);
        _provider.close();
    }

    if (_glContext) {
        [EAGLContext setCurrentContext:_glContext];
        glDeleteProgram(_program);
        glDeleteTextures(1, &_yTexture);
        glDeleteTextures(1, &_uvTexture);
        glDeleteBuffers(1, &_vertexBuffer);
    }

    [EAGLContext setCurrentContext:nil];
    _glContext = nil;
    [_glkView setDelegate:nil];
    _glkView = nil;
    NSLog(@"ExampleVC_gles cleared");
}

@end
