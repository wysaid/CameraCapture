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
@property (nonatomic, strong) GLKView* glkView;
@property (weak, nonatomic) IBOutlet UIButton* backBtn;
@end

@implementation ExampleVC_gles
{
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

    float _progress;
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    _allCameraNames = _provider.findDeviceNames();
    for (auto& name : _allCameraNames)
    {
        NSLog(@"Find Camera Device: %s", name.c_str());
    }

    _cameraIndex = 0;

    // render
    _glContext = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
    _glkView = [[GLKView alloc] initWithFrame:self.view.bounds context:_glContext];
    [_glkView setDrawableColorFormat:GLKViewDrawableColorFormatRGBA8888];
    [_glkView setEnableSetNeedsDisplay:NO];
    [_glkView setBackgroundColor:[UIColor clearColor]];
    [_glkView setDelegate:self];
    [self setupGLES];

    // camera

    _provider.set(ccap::PropertyName::PixelFormatInternal, ccap::PixelFormat::NV12f);
    _provider.set(ccap::PropertyName::PixelFormatOutput, ccap::PixelFormat::NV12f);
    _provider.set(ccap::PropertyName::Width, 1920);
    _provider.set(ccap::PropertyName::Height, 1080);

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

    if (!_provider.open(_cameraIndex, true))
    {
        _provider.close();
        _provider.set(ccap::PropertyName::Width, 1280);
        _provider.set(ccap::PropertyName::Height, 720);
        _provider.open(_cameraIndex, true);
    }

    [self.view insertSubview:_glkView atIndex:0];
}

- (GLuint)loadShader:(const char*)shaderSource type:(GLenum)type
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &shaderSource, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        GLchar infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        NSLog(@"Shader compilation failed: %s", infoLog);
        return 0;
    }
    return shader;
}

- (void)checkGLError
{
    GLenum error = glGetError();
    if (error != GL_NO_ERROR)
    {
        NSLog(@"❌OpenGL Error: %d", error);
    }
}

- (void)setupGLES
{
    [EAGLContext setCurrentContext:_glContext];

    const float vertData[8] = { -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f };
    glGenBuffers(1, &_vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, _vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertData), vertData, GL_STATIC_DRAW);

    [self checkGLError];

    GLuint vertexShader = [self loadShader:kVertexShaderSource type:GL_VERTEX_SHADER];
    GLuint fragmentShader = [self loadShader:kFragmentShaderSource type:GL_FRAGMENT_SHADER];
    if (vertexShader == 0 || fragmentShader == 0)
    {
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

    if (!success)
    {
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
    if (_progressUniformLocation == -1)
    {
        NSLog(@"Uniform 'progress' not found in shader program");
    }

    [self setFlipScale:-1.0f y:1.0f];
    [self setRotation:M_PI_2];
    [self checkGLError];
}

- (void)setFlipScale:(float)x y:(float)y
{
    [EAGLContext setCurrentContext:_glContext];
    glUseProgram(_program);
    GLint flipScaleLocation = glGetUniformLocation(_program, "flipScale");
    if (flipScaleLocation == -1)
    {
        NSLog(@"Uniform 'flipScale' not found in shader program");
        return;
    }
    glUniform2f(flipScaleLocation, x, y);
    [self checkGLError];
}

- (void)setRotation:(float)rotation
{
    [EAGLContext setCurrentContext:_glContext];
    glUseProgram(_program);
    GLint rotationLocation = glGetUniformLocation(_program, "rotation");
    if (rotationLocation == -1)
    {
        NSLog(@"Uniform 'rotation' not found in shader program");
        return;
    }
    glUniform1f(rotationLocation, rotation);
    [self checkGLError];
}

- (void)updateFrame
{
    [EAGLContext setCurrentContext:_glContext];
    auto newFrame = _provider.grab(0);
    if (!newFrame)
        return;
    if (_yTexture == 0 || _textureWidth != newFrame->width || _textureHeight != newFrame->height)
    {
        if (_yTexture != 0)
        {
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
    }
    else
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, _yTexture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _textureWidth, _textureHeight, GL_LUMINANCE, GL_UNSIGNED_BYTE, newFrame->data[0]);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, _uvTexture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _textureWidth / 2, _textureHeight / 2, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, newFrame->data[1]);
    }

    [self checkGLError];
}

- (void)glkView:(GLKView*)view drawInRect:(CGRect)rect
{
    [self updateFrame];

    [view bindDrawable];

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (_yTexture == 0 || _program == 0)
    {
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

    float scaling = std::min(canvasWidth / (float)srcWidth, canvasHeight / (float)srcHeight);
    GLint viewportWidth = scaling * srcWidth;
    GLint viewportHeight = scaling * srcHeight;

    glViewport(0, 0, viewportWidth, viewportHeight);
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
        [self setFlipScale:-1.0f y:1.0f];
        [self setRotation:M_PI_2];
    }
    else
    {
        [self setFlipScale:-1.0f y:-1.0f];
        [self setRotation:M_PI_2];
    }
    NSLog(@"Switched to camera: %s", name.c_str());
}

- (void)dealloc
{
    [self clear];
    NSLog(@"ExampleVC_gles dealloc");
}

- (void)fixViewSize
{
    if (!CGSizeEqualToSize(_glkView.bounds.size, self.view.bounds.size))
    {
        [_glkView setFrame:CGRectMake(0, 0, self.view.bounds.size.width, self.view.bounds.size.height)];
    }
}

- (void)viewDidLayoutSubviews
{
    [super viewDidLayoutSubviews];
    [self fixViewSize];
}

- (void)clear
{
    if (_provider.isOpened())
    {
        _provider.setNewFrameCallback(nullptr);
        _provider.close();
    }

    if (_glContext)
    {
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
