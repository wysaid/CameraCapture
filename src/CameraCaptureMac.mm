/**
 * @file CameraCaptureMac.mm
 * @author wysaid (this@wysaid.org)
 * @brief Source file for ProviderMac class.
 * @date 2025-04
 *
 */

#if __APPLE__

#include "CameraCaptureMac.h"

#import <AVFoundation/AVFoundation.h>
#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>
#include <cmath>

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

static NSString* getCVPixelFormatName(OSType format)
{
    switch (format)
    {
    case kCVPixelFormatType_420YpCbCr8BiPlanarFullRange:
        return @"kCVPixelFormatType_420YpCbCr8BiPlanarFullRange";
    case kCVPixelFormatType_32BGRA:
        return @"kCVPixelFormatType_32BGRA";
    case kCVPixelFormatType_24BGR:
        return @"kCVPixelFormatType_24BGR";
    case kCVPixelFormatType_24RGB:
        return @"kCVPixelFormatType_24RGB";
    case kCVPixelFormatType_32ARGB:
        return @"kCVPixelFormatType_32ARGB";
    case kCVPixelFormatType_32ABGR:
        return @"kCVPixelFormatType_32ABGR";
    case kCVPixelFormatType_422YpCbCr8:
        return @"kCVPixelFormatType_422YpCbCr8";
    case kCVPixelFormatType_422YpCbCr8_yuvs:
        return @"kCVPixelFormatType_422YpCbCr8_yuvs";
    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
        return @"kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange";

    default:
        return [NSString stringWithFormat:@"Unknown(0x%08x)", (unsigned int)format];
    }
}

@interface CameraCaptureObjc : NSObject<AVCaptureVideoDataOutputSampleBufferDelegate>
{
    ccap::ProviderMac* _provider;

    /// for verbose log
    uint64_t _frameCounter;
    uint64_t _lastFrameTime;
    double _duration;
    double _fps;
    ccap::PixelFormat _pixelFormat;
}

@property (nonatomic, strong) AVCaptureSession* session;
@property (nonatomic, strong) AVCaptureDevice* device;
@property (nonatomic, strong) NSString* cameraName;
@property (nonatomic, strong) AVCaptureDeviceInput* videoInput;
@property (nonatomic, strong) AVCaptureVideoDataOutput* videoOutput;
@property (nonatomic, strong) dispatch_queue_t captureQueue;
@property (nonatomic) CGSize resolution;
@property (nonatomic) OSType cvPixelFormat;

- (instancetype)initWithProvider:(ccap::ProviderMac*)provider captureSize:(CGSize)sz;
- (BOOL)start;
- (void)stop;

@end

@implementation CameraCaptureObjc

- (instancetype)initWithProvider:(ccap::ProviderMac*)provider captureSize:(CGSize)sz
{
    self = [super init];
    if (self)
    {
        _duration = 0;
        _fps = 0;
        _frameCounter = 0;

        _provider = provider;
        _resolution = sz;
        _session = [[AVCaptureSession alloc] init];
        [_session beginConfiguration];

        if (AVCaptureSessionPreset preset = AVCaptureSessionPresetHigh; _resolution.width > 0 && _resolution.height > 0)
        { /// Handle resolution
            NSArray* resolutions = @[
                @[ @320, @240, AVCaptureSessionPreset320x240 ],
                @[ @352, @288, AVCaptureSessionPreset352x288 ],
                @[ @640, @480, AVCaptureSessionPreset640x480 ],
                @[ @960, @540, AVCaptureSessionPreset960x540 ],
                @[ @1280, @720, AVCaptureSessionPreset1280x720 ],
                @[ @1920, @1080, AVCaptureSessionPreset1920x1080 ],
                @[ @3840, @2160, AVCaptureSessionPreset3840x2160 ],
            ];

            CGSize inputSize = _resolution;

            for (NSArray* res in resolutions)
            {
                if ([res[0] intValue] >= _resolution.width && [res[1] intValue] >= _resolution.height)
                {
                    _resolution.width = [res[0] intValue];
                    _resolution.height = [res[1] intValue];
                    preset = res[2];
                    break;
                }
            }

            if (![_session canSetSessionPreset:preset])
            {
                if (ccap::errorLogEnabled())
                {
                    NSLog(@"ccap: CameraCaptureObjc init - session preset not supported, using AVCaptureSessionPresetHigh");
                }
                preset = AVCaptureSessionPresetHigh;
            }

            [_session setSessionPreset:preset];

            if (ccap::infoLogEnabled())
            {
                NSLog(@"ccap: Expected camera resolution: (%gx%g), actual matched camera resolution: (%gx%g)",
                      inputSize.width, inputSize.height, _resolution.width, _resolution.height);
            }
        }

        if (_cameraName != nil && _cameraName.length > 0)
        { /// Find preferred device
            NSArray<AVCaptureDevice*>* devices = [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];
            for (AVCaptureDevice* d in devices)
            {
                if ([d.localizedName caseInsensitiveCompare:_cameraName] == NSOrderedSame)
                {
                    _device = d;
                    break;
                }
            }
        }

        if (!_device)
        { /// Fallback to default device
            _device = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];
        }

        if (![_device hasMediaType:AVMediaTypeVideo])
        { /// No video device found
            if (ccap::errorLogEnabled())
                NSLog(@"ccap: No video device found");
            return nil;
        }

        /// Configure device

        NSError* error = nil;
        [_device lockForConfiguration:&error];
        if ([_device isFocusModeSupported:AVCaptureFocusModeContinuousAutoFocus])
        {
            [_device setFocusMode:AVCaptureFocusModeContinuousAutoFocus];
            if (ccap::verboseLogEnabled())
            {
                NSLog(@"ccap: Set focus mode to continuous auto focus");
            }
        }
        [_device unlockForConfiguration];

        // Create input device
        _videoInput = [[AVCaptureDeviceInput alloc] initWithDevice:_device error:&error];
        if (error)
        {
            [AVCaptureDeviceInput deviceInputWithDevice:self.device error:&error];
            if (error)
            {
                if (ccap::errorLogEnabled())
                {
                    NSLog(@"ccap: Open camera failed: %@", error);
                }
                return nil;
            }
        }

        // Add input device to session
        if ([_session canAddInput:_videoInput])
        {
            [_session addInput:_videoInput];
            if (ccap::verboseLogEnabled())
            {
                NSLog(@"ccap: Add input to session");
            }
        }

        // Create output device
        _videoOutput = [[AVCaptureVideoDataOutput alloc] init];
        [_videoOutput setAlwaysDiscardsLateVideoFrames:YES]; // better performance

        { /// Handle pixel format
            _pixelFormat = _provider->getFrameProperty().pixelFormat;
            if (_pixelFormat == ccap::PixelFormat::Unknown)
            { /// Default to BGRA8888 if not set
                _pixelFormat = ccap::PixelFormat::BGRA8888;
            }

            [self fixPixelFormat];

            NSArray* supportedFormats = [_videoOutput availableVideoCVPixelFormatTypes];
            if (ccap::verboseLogEnabled())
            {
                NSMutableArray<NSString*>* arr = [NSMutableArray new];
                for (NSNumber* format in supportedFormats)
                {
                    [arr addObject:getCVPixelFormatName([format unsignedIntValue])];
                }

                NSLog(@"ccap: Supported pixel format: %@", arr);
            }

            OSType preferredFormat = _cvPixelFormat;
            if (![supportedFormats containsObject:@(preferredFormat)])
            {
                _cvPixelFormat = 0;
                if (bool hasYUV = _pixelFormat & ccap::kPixelFormatYUVColorBit)
                { /// Handle YUV formats, fallback to NV12f
                    auto hasFullRange = _pixelFormat & ccap::kPixelFormatYUVColorFullRangeBit;
                    auto supportFullRange = [supportedFormats containsObject:@(kCVPixelFormatType_420YpCbCr8BiPlanarFullRange)];
                    auto supportVideoRange = [supportedFormats containsObject:@(kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange)];

                    if (supportFullRange || supportVideoRange)
                    {
                        if (!hasFullRange && supportVideoRange)
                        {
                            _pixelFormat = ccap::PixelFormat::NV12v;
                            _cvPixelFormat = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
                        }
                        else
                        {
                            _pixelFormat = ccap::PixelFormat::NV12f;
                            _cvPixelFormat = kCVPixelFormatType_420YpCbCr8BiPlanarFullRange;
                        }
                    }
                }

                if (_cvPixelFormat == 0)
                {
                    auto hasOnlyRGB = ccap::pixelFormatInclude(_pixelFormat, ccap::kPixelFormatRGBColorBit);
                    auto supportRGB = [supportedFormats containsObject:@(kCVPixelFormatType_24RGB)];
                    auto supportBGR = [supportedFormats containsObject:@(kCVPixelFormatType_24BGR)];
                    auto supportBGRA = [supportedFormats containsObject:@(kCVPixelFormatType_32BGRA)];

                    if (hasOnlyRGB && (supportRGB || supportBGR))
                    {
                        if (supportBGR)
                        {
                            _pixelFormat = ccap::PixelFormat::BGR888;
                            _cvPixelFormat = kCVPixelFormatType_24BGR;
                        }
                        else
                        {
                            _pixelFormat = ccap::PixelFormat::RGB888;
                            _cvPixelFormat = kCVPixelFormatType_24RGB;
                        }
                    }
                    else
                    {
                        if (supportBGRA)
                        {
                            _pixelFormat = ccap::PixelFormat::BGRA8888;
                            _cvPixelFormat = kCVPixelFormatType_32BGRA;
                        }
                        else
                        {
                            _pixelFormat = ccap::PixelFormat::RGBA8888;
                            _cvPixelFormat = kCVPixelFormatType_32RGBA;
                        }
                    }
                }

                if (_cvPixelFormat == 0)
                { /// last fall back.
                    _cvPixelFormat = kCVPixelFormatType_32BGRA;
                    _pixelFormat = ccap::PixelFormat::BGRA8888;
                }

                if (ccap::errorLogEnabled())
                {
                    NSLog(@"ccap: Preferred pixel format (%@) not supported, fallback to: (%@)", getCVPixelFormatName(preferredFormat), getCVPixelFormatName(_cvPixelFormat));
                }
            }

            _videoOutput.videoSettings = @{(id)kCVPixelBufferPixelFormatTypeKey : @(_cvPixelFormat)};
        }

        // Set output queue
        _captureQueue = dispatch_queue_create("ccap.queue", DISPATCH_QUEUE_SERIAL);
        [_videoOutput setSampleBufferDelegate:self queue:_captureQueue];

        // Add output device to session
        if ([_session canAddOutput:_videoOutput])
        {
            [_session addOutput:_videoOutput];
            if (ccap::verboseLogEnabled())
            {
                NSLog(@"ccap: Add output to session");
            }
        }

        [_session commitConfiguration];
        if (auto fps = _provider->getFrameProperty().fps; fps > 0.0)
        {
            [self setFrameRate:_provider->getFrameProperty().fps];
        }
        [self flushResolution];
    }
    return self;
}

- (void)setFrameRate:(double)fps
{
    if (_device)
    {
        NSError* error;
        [_device lockForConfiguration:&error];

        if ([_device respondsToSelector:@selector(setActiveVideoMinFrameDuration:)] &&
            [_device respondsToSelector:@selector(setActiveVideoMaxFrameDuration:)])
        {
            if (error == nil)
            {
                double desiredFps = fps;
                double distance = 1e9;
                CMTime maxFrameDuration = kCMTimeInvalid;
                CMTime minFrameDuration = kCMTimeInvalid;
                for (AVFrameRateRange* r in _device.activeFormat.videoSupportedFrameRateRanges)
                {
                    auto newDis = std::abs(r.minFrameRate - desiredFps) + std::abs(r.maxFrameRate - desiredFps);
                    if (newDis < distance)
                    {
                        maxFrameDuration = r.maxFrameDuration;
                        minFrameDuration = r.minFrameDuration;
                        distance = newDis;
                        fps = r.minFrameRate;
                        break;
                    }
                }

                if (CMTIME_IS_VALID(maxFrameDuration) && CMTIME_IS_VALID(minFrameDuration))
                {
                    [_device setActiveVideoMinFrameDuration:maxFrameDuration];
                    [_device setActiveVideoMaxFrameDuration:minFrameDuration];
                    _provider->getFrameProperty().fps = fps;

                    if (ccap::infoLogEnabled())
                    {
                        if (std::abs(fps - desiredFps) > 0.01)
                        {
                            NSLog(@"ccap: Set fps to %g, but actual fps is %g", desiredFps, fps);
                        }
                        else
                        {
                            NSLog(@"ccap: Set fps to %g", fps);
                        }
                    }
                }
                else
                {
                    if (ccap::errorLogEnabled())
                    {
                        NSLog(@"ccap: Desired fps (%g) not supported, skipping", fps);
                    }
                }
            }
        }
        else
        {
            for (AVCaptureConnection* connection in _videoOutput.connections)
            {
                for (AVCaptureInputPort* port in connection.inputPorts)
                {
                    if ([port.mediaType isEqualToString:AVMediaTypeVideo])
                    {
                        auto tm = CMTimeMake(1, fps);
                        connection.videoMinFrameDuration = tm;
                        connection.videoMaxFrameDuration = tm;
                    }
                }
            }
        }

        [_device unlockForConfiguration];
    }
}

- (void)fixPixelFormat
{
    switch (_pixelFormat)
    {
    case ccap::PixelFormat::NV21v: /// MacOS does not support NV21, fallback to NV12
    case ccap::PixelFormat::I420v:
        if (ccap::errorLogEnabled())
        {
            NSLog(@"ccap: NV21v/I420v is not supported on macOS, fallback to NV12v");
        }
    case ccap::PixelFormat::NV12v:
        _cvPixelFormat = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
        _pixelFormat = ccap::PixelFormat::NV12v;
        break;
    case ccap::PixelFormat::NV21f: /// MacOS does not support NV21, fallback to NV12
    case ccap::PixelFormat::I420f:
        if (ccap::errorLogEnabled())
        {
            NSLog(@"ccap: NV21f/I420f is not supported on macOS, fallback to NV12f");
        }
    case ccap::PixelFormat::NV12f:
        _cvPixelFormat = kCVPixelFormatType_420YpCbCr8BiPlanarFullRange;
        _pixelFormat = ccap::PixelFormat::NV12f;
        break;
    case ccap::PixelFormat::BGRA8888:
        _cvPixelFormat = kCVPixelFormatType_32BGRA;
        break;
    case ccap::PixelFormat::RGBA8888:
        _cvPixelFormat = kCVPixelFormatType_32RGBA;
        break;
    case ccap::PixelFormat::BGR888:
        _cvPixelFormat = kCVPixelFormatType_24BGR;
        break;
    default: /// I420 is not supported on macOS
    case ccap::PixelFormat::RGB888:
        _cvPixelFormat = kCVPixelFormatType_24RGB;
        _pixelFormat = ccap::PixelFormat::RGB888;
        break;
    }
}

- (void)flushResolution
{
    if (_videoOutput && _videoOutput.connections.count > 0)
    {
        AVCaptureConnection* connection = [_videoOutput connections][0];
        if (connection)
        {
            CMFormatDescriptionRef formatDescription = connection.supportsVideoMinFrameDuration ? connection.inputPorts[0].formatDescription : nil;
            if (formatDescription)
            {
                CMVideoDimensions dimensions = CMVideoFormatDescriptionGetDimensions(formatDescription);
                if (dimensions.width != _resolution.width || dimensions.height != _resolution.height)
                {
                    if (ccap::infoLogEnabled())
                    {
                        NSLog(@"ccap: Actual camera resolution: %dx%d", dimensions.width, dimensions.height);
                    }

                    _resolution.width = dimensions.width;
                    _resolution.height = dimensions.height;
                    if (_provider)
                    {
                        auto& prop = _provider->getFrameProperty();
                        prop.width = dimensions.width;
                        prop.height = dimensions.height;
                    }
                }
            }
            return;
        }
    }

    if (ccap::errorLogEnabled())
    {
        NSLog(@"ccap: No connections available");
    }
}

- (BOOL)start
{
    if (_session && ![_session isRunning])
    {
        if (ccap::verboseLogEnabled())
        {
            NSLog(@"ccap: CameraCaptureObjc start");
        }
        [_session startRunning];
    }
    return [_session isRunning];
}

- (void)stop
{
    if (_session && [_session isRunning])
    {
        if (ccap::verboseLogEnabled())
        {
            NSLog(@"ccap: CameraCaptureObjc stop");
        }
        [_session stopRunning];
    }
}

- (BOOL)isRunning
{
    return [_session isRunning];
}

- (void)destroy
{
    if (_session)
    {
        if (ccap::verboseLogEnabled())
        {
            NSLog(@"ccap: CameraCaptureObjc destroy");
        }

        [_session stopRunning];
        [_videoOutput setSampleBufferDelegate:nil queue:dispatch_get_main_queue()];

        [_session beginConfiguration];

        if (_videoInput)
        {
            [_session removeInput:_videoInput];
            [_session removeOutput:_videoOutput];
            _videoInput = nil;
            _videoOutput = nil;
        }

        [_session commitConfiguration];
        _session = nil;
    }
}

- (void)dealloc
{
    if (ccap::verboseLogEnabled())
    {
        NSLog(@"ccap: CameraCaptureObjc dealloc - begin");
    }

    [self destroy];

    if (ccap::verboseLogEnabled())
    {
        NSLog(@"ccap: CameraCaptureObjc dealloc - end");
    }
}

// 处理每一帧的数据
- (void)captureOutput:(AVCaptureOutput*)output
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
           fromConnection:(AVCaptureConnection*)connection
{
    if (!_provider)
    {
        if (ccap::errorLogEnabled())
            NSLog(@"ccap: CameraCaptureObjc captureOutput - provider is nil");
        return;
    }

    // 获取图像缓冲区
    CVImageBufferRef imageBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);

    // 锁定图像缓冲区以访问其内容
    CVPixelBufferLockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);

    auto newFrame = _provider->getFreeFrame();

    // 获取分辨率
    size_t width = CVPixelBufferGetWidth(imageBuffer);
    size_t height = CVPixelBufferGetHeight(imageBuffer);
    size_t bytes{};

    CMTime timestamp = CMSampleBufferGetPresentationTimeStamp(sampleBuffer);

    newFrame->timestamp = (uint64_t)(CMTimeGetSeconds(timestamp) * 1e9);
    newFrame->width = width;
    newFrame->height = height;
    newFrame->pixelFormat = _pixelFormat;

    if (_pixelFormat & ccap::kPixelFormatYUVColorBit)
    {
        auto yBytesPerRow = CVPixelBufferGetBytesPerRowOfPlane(imageBuffer, 0);
        auto uvBytesPerRow = CVPixelBufferGetBytesPerRowOfPlane(imageBuffer, 1);
        size_t yBytes = CVPixelBufferGetHeightOfPlane(imageBuffer, 0) * yBytesPerRow;
        size_t uvBytes = CVPixelBufferGetHeightOfPlane(imageBuffer, 1) * uvBytesPerRow;
        bytes = yBytes + uvBytes;

        newFrame->data[2] = nullptr;

        newFrame->stride[0] = yBytesPerRow;
        newFrame->stride[1] = uvBytesPerRow;
        newFrame->stride[2] = 0;
        newFrame->sizeInBytes = bytes;

        CFRetain(imageBuffer);
        auto manager = std::make_shared<ccap::FakeFrame>([imageBuffer, newFrame]() mutable {
            CVPixelBufferUnlockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);
            CFRelease(imageBuffer);
            if (ccap::verboseLogEnabled())
            {
                NSLog(@"ccap: recycled YUV frame, width: %d, height: %d", (int)newFrame->width, (int)newFrame->height);
            }

            /// Make ref count + 1
            newFrame = nullptr;
        });

        newFrame->data[0] = (uint8_t*)CVPixelBufferGetBaseAddressOfPlane(imageBuffer, 0);
        newFrame->data[1] = (uint8_t*)CVPixelBufferGetBaseAddressOfPlane(imageBuffer, 1);
        auto fakeFrame = std::shared_ptr<ccap::Frame>(manager, newFrame.get());
        newFrame = fakeFrame;
    }
    else
    {
        bytes = CVPixelBufferGetDataSize(imageBuffer);
        newFrame->sizeInBytes = bytes;

        newFrame->data[1] = nullptr;
        newFrame->data[2] = nullptr;
        newFrame->stride[0] = CVPixelBufferGetBytesPerRow(imageBuffer);
        newFrame->stride[1] = 0;
        newFrame->stride[2] = 0;

        CFRetain(imageBuffer);
        auto manager = std::make_shared<ccap::FakeFrame>([imageBuffer, newFrame]() mutable {
            CVPixelBufferUnlockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);
            CFRelease(imageBuffer);
            if (ccap::verboseLogEnabled())
            {
                NSLog(@"ccap: recycled RGBA frame, width: %d, height: %d", (int)newFrame->width, (int)newFrame->height);
            }
            /// Make ref count + 1
            newFrame = nullptr;
        });

        newFrame->data[0] = (uint8_t*)CVPixelBufferGetBaseAddress(imageBuffer);
        auto fakeFrame = std::shared_ptr<ccap::Frame>(manager, newFrame.get());
        newFrame = fakeFrame;
    }

    newFrame->frameIndex = _provider->frameIndex()++;

    if (ccap::verboseLogEnabled())
    {
        if (_lastFrameTime != 0)
        {
            _duration += (newFrame->timestamp - _lastFrameTime) / 1.e9;
        }
        _lastFrameTime = newFrame->timestamp;
        ++_frameCounter;

        if (_duration > 0.5 || _frameCounter >= 30)
        {
            auto newFps = _frameCounter / _duration;
            constexpr double alpha = 0.8; // Smoothing factor, smaller value means smoother
            _fps = alpha * newFps + (1.0 - alpha) * _fps;
            _frameCounter = 0;
            _duration = 0;
        }

        double roundedFps = std::round(_fps * 10.0) / 10.0;
        NSLog(@"ccap: New frame available: %lux%lu, bytes %lu, Data address: %p, fps: %g", width, height, bytes, newFrame->data[0], roundedFps);
    }

    _provider->newFrameAvailable(std::move(newFrame));
}

@end

namespace ccap
{
ProviderMac::~ProviderMac()
{
    if (m_imp)
    {
        [m_imp destroy];
        m_imp = nil;
    }
}

std::vector<std::string> ProviderMac::findDeviceNames()
{
    @autoreleasepool
    {
        NSArray<AVCaptureDevice*>* devices = [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];
        std::vector<std::string> names;
        if (devices.count != 0)
        {
            names.reserve(devices.count);
            for (AVCaptureDevice* d in devices)
            {
                names.emplace_back([d.localizedName UTF8String]);
            }
        }
        return names;
    }
}

bool ProviderMac::open(std::string_view deviceName)
{
    if (m_imp != nil)
    {
        if (ccap::errorLogEnabled())
        {
            NSLog(@"ccap: Camera is already opened");
        }
        return false;
    }

    @autoreleasepool
    {
        m_imp = [[CameraCaptureObjc alloc] initWithProvider:this captureSize:CGSizeMake(m_frameProp.width, m_frameProp.height)];
    }
    return isOpened();
}

bool ProviderMac::isOpened() const
{
    return m_imp && m_imp.session;
}

void ProviderMac::close()
{
    m_imp = nil;
}

bool ProviderMac::start()
{
    if (!isOpened())
    {
        if (warningLogEnabled())
        {
            NSLog(@"ccap: camera start called with no device opened");
        }
        return false;
    }

    @autoreleasepool
    {
        return [m_imp start];
    }
}

void ProviderMac::stop()
{
    if (m_imp)
    {
        @autoreleasepool
        {
            [m_imp stop];
        }
    }
}

bool ProviderMac::isStarted() const
{
    return m_imp && [m_imp isRunning];
}

} // namespace ccap

#endif