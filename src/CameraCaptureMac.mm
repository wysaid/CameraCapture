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
#import <Accelerate/Accelerate.h>
#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>
#include <cassert>
#include <cmath>

using namespace ccap;

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

static NSArray<AVCaptureDevice*>* findAllDeviceName()
{
    // 按类型顺序排序
    NSMutableArray* allTypes = [NSMutableArray new];
    [allTypes addObject:AVCaptureDeviceTypeBuiltInWideAngleCamera];
    if (@available(macOS 14.0, *))
    {
        [allTypes addObject:AVCaptureDeviceTypeExternal];
    }
    else
    {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        [allTypes addObject:AVCaptureDeviceTypeExternalUnknown];
#pragma clang diagnostic pop
    }

    AVCaptureDeviceDiscoverySession* discoverySession =
        [AVCaptureDeviceDiscoverySession discoverySessionWithDeviceTypes:allTypes
                                                               mediaType:AVMediaTypeVideo
                                                                position:AVCaptureDevicePositionUnspecified];
    if (verboseLogEnabled())
    {
        NSLog(@"ccap: Available camera devices: %@", discoverySession.devices);
    }

    std::vector<std::string_view> virtualDevicePatterns = {
        "obs",
        "virtual",
    };

    auto countIndex = [&](AVCaptureDevice* device) {
        NSUInteger idx = [allTypes indexOfObject:device.deviceType] * 100 + 1000;
        if (idx == NSNotFound)
            return allTypes.count * 100;

        // Here, check if it is a virtual camera like OBS, and if so, add 10 to the index.
        std::string name = [[device.localizedName lowercaseString] UTF8String];

        for (auto& pattern : virtualDevicePatterns)
        {
            if (name.find(pattern) != std::string::npos)
            {
                idx += 10;
                break;
            }
        }

        // Typically, real cameras support multiple formats, but virtual cameras usually support only one format.
        // If the device supports multiple formats, subtract the count of formats from the index.
        if (int c = (int)device.formats.count; c > 1)
        {
            idx -= device.formats.count;
        }

        return idx;
    };

    return [discoverySession.devices sortedArrayUsingComparator:^NSComparisonResult(AVCaptureDevice* d1, AVCaptureDevice* d2) {
        NSUInteger idx1 = countIndex(d1);
        NSUInteger idx2 = countIndex(d2);

        if (idx1 < idx2) return NSOrderedAscending;
        if (idx1 > idx2) return NSOrderedDescending;
        return NSOrderedSame;
    }];
}

static void inplaceConvertFrame(Frame* frame, PixelFormat toFormat)
{
    auto* inputBytes = frame->data[0];
    auto inputLineSize = frame->stride[0];
    auto outputChannelCount = (toFormat & kPixelFormatAlphaColorBit) ? 4 : 3;

    // Ensure 16/32 byte alignment for best performance
    auto newLineSize = outputChannelCount == 3 ? ((frame->width * 3 + 31) & ~31) : (frame->width * 4);
    auto inputFormat = frame->pixelFormat;
    frame->stride[0] = newLineSize;
    frame->allocator->resize(newLineSize * frame->height);
    frame->data[0] = frame->allocator->data();
    frame->pixelFormat = toFormat;

    vImage_Buffer src = { inputBytes, frame->height, frame->width, inputLineSize };
    vImage_Buffer dst = { frame->data[0], frame->height, frame->width, newLineSize };
    auto inputChannelCount = (inputFormat & kPixelFormatAlphaColorBit) ? 4 : 3;

    bool isInputRGB = inputFormat & kPixelFormatRGBBit; ///< 不是 RGB 就是 BGR
    bool isOutputRGB = toFormat & kPixelFormatRGBBit;   ///< 不是 RGB 就是 BGR
    bool swapRB = isInputRGB != isOutputRGB;            ///< 是否需要交换 R 和 B 通道

    //// 这里的 input 和 output 的交叉转换要写很多 switch case, 之类简化一下.

    if (inputChannelCount == outputChannelCount)
    { /// 通道数相同的转换, 只能是 RGB <-> BGR, RGBA <-> BGRA, swapRB 肯定为 true.
        assert(swapRB);
        if (inputChannelCount == 4)
        {
            // RGBA8888 <-> BGRA8888: 需要通道重排
            // vImagePermuteChannels_ARGB8888 需要4个索引，来实现任意通道的排列
            // RGBA8888: [R, G, B, A]，BGRA8888: [B, G, R, A]
            uint8_t permuteMap[4] = { 2, 1, 0, 3 };
            vImagePermuteChannels_ARGB8888(&src, &dst, permuteMap, kvImageNoFlags);
        }
        else
        {
            // same as above
            uint8_t permuteMap[3] = { 2, 1, 0 }; // R,G,B -> B,G,R
            vImagePermuteChannels_RGB888(&src, &dst, permuteMap, kvImageNoFlags);
        }
    }
    else
    { // 数据通道不相同, 只能是 4通道 <-> 3通道

        if (inputChannelCount == 4)
        { // 4通道 -> 3通道
            if (swapRB)
            { // 可能情况:  RGBA->BGR, BGRA->RGB
                vImageConvert_RGBA8888toBGR888(&src, &dst, kvImageNoFlags);
            }
            else
            { // 可能情况: RGBA->RGB, BGRA->BGR
                vImageConvert_RGBA8888toRGB888(&src, &dst, kvImageNoFlags);
            }
        }
        else
        { /// 3通道 -> 4通道
            if (swapRB)
            { // 可能情况: BGR->RGBA, RGB->BGRA
                vImageConvert_RGB888toBGRA8888(&src, nullptr, 0xff, &dst, false, kvImageNoFlags);
            }
            else
            { // 可能情况: BGR->BGRA, RGB->RGBA
                vImageConvert_RGB888toRGBA8888(&src, nullptr, 0xff, &dst, false, kvImageNoFlags);
            }
        }
    }
}

@interface CameraCaptureObjc : NSObject<AVCaptureVideoDataOutputSampleBufferDelegate>
{
    ProviderMac* _provider;

    /// for verbose log
    uint64_t _frameCounter;
    uint64_t _lastFrameTime;
    double _duration;
    double _fps;
    PixelFormat _cameraPixelFormat;
    PixelFormat _convertPixelFormat;
}

@property (nonatomic, strong) AVCaptureSession* session;
@property (nonatomic, strong) AVCaptureDevice* device;
@property (nonatomic, strong) NSString* cameraName;
@property (nonatomic, strong) AVCaptureDeviceInput* videoInput;
@property (nonatomic, strong) AVCaptureVideoDataOutput* videoOutput;
@property (nonatomic, strong) dispatch_queue_t captureQueue;
@property (nonatomic) CGSize resolution;
@property (nonatomic) OSType cvPixelFormat;

- (instancetype)initWithProvider:(ProviderMac*)provider;
- (BOOL)start;
- (void)stop;

@end

@implementation CameraCaptureObjc

- (instancetype)initWithProvider:(ProviderMac*)provider
{
    self = [super init];
    if (self)
    {
        _duration = 0;
        _fps = 0;
        _frameCounter = 0;
        _provider = provider;
    }
    return self;
}

- (BOOL)open
{
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
            if (errorLogEnabled())
            {
                NSLog(@"ccap: CameraCaptureObjc init - session preset not supported, using AVCaptureSessionPresetHigh");
            }
            preset = AVCaptureSessionPresetHigh;
        }

        [_session setSessionPreset:preset];

        if (infoLogEnabled())
        {
            NSLog(@"ccap: Expected camera resolution: (%gx%g), actual matched camera resolution: (%gx%g)",
                  inputSize.width, inputSize.height, _resolution.width, _resolution.height);
        }
    }

    if (_cameraName != nil && _cameraName.length > 0)
    { /// Find preferred device
        NSArray<AVCaptureDevice*>* devices = findAllDeviceName();
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
        if (errorLogEnabled())
            NSLog(@"ccap: No video device found");
        return NO;
    }

    if (verboseLogEnabled())
    {
        NSLog(@"ccap: CameraCaptureObjc init - camera name: %@", _device.localizedName);
    }

    /// Configure device

    NSError* error = nil;
    [_device lockForConfiguration:&error];
    if ([_device isFocusModeSupported:AVCaptureFocusModeContinuousAutoFocus])
    {
        [_device setFocusMode:AVCaptureFocusModeContinuousAutoFocus];
        if (verboseLogEnabled())
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
            if (errorLogEnabled())
            {
                NSLog(@"ccap: Open camera failed: %@", error);
            }
            return NO;
        }
    }

    // Add input device to session
    if ([_session canAddInput:_videoInput])
    {
        [_session addInput:_videoInput];
        if (verboseLogEnabled())
        {
            NSLog(@"ccap: Add input to session");
        }
    }

    // Create output device
    _videoOutput = [[AVCaptureVideoDataOutput alloc] init];
    [_videoOutput setAlwaysDiscardsLateVideoFrames:YES]; // better performance

    { // set portrait orientation
        AVCaptureConnection* conn = [_videoOutput connectionWithMediaType:AVMediaTypeVideo];
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        if ([conn isVideoOrientationSupported])
        {
            [conn setVideoOrientation:AVCaptureVideoOrientationPortrait];
        }
#pragma clang diagnostic pop
        else
        {
            if (verboseLogEnabled())
            {
                NSLog(@"ccap: Video orientation not supported");
            }
        }
    }

    auto requiredPixelFormat = _provider->getFrameProperty().pixelFormat;

    { /// Handle pixel format
        _cameraPixelFormat = requiredPixelFormat;

        static_assert(sizeof(_cameraPixelFormat) == sizeof(uint32_t), "size must match");
        (uint32_t&)_cameraPixelFormat &= ~(uint32_t)kPixelFormatForceToSetBit; // remove the force set bit

        if (_cameraPixelFormat == PixelFormat::Unknown)
        { /// Default to BGRA32 if not set
            _cameraPixelFormat = PixelFormat::BGRA32;
        }

        [self fixPixelFormat];
        _convertPixelFormat = _cameraPixelFormat;

        NSArray* supportedFormats = [_videoOutput availableVideoCVPixelFormatTypes];
        if (verboseLogEnabled())
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
            if (bool hasYUV = _cameraPixelFormat & kPixelFormatYUVColorBit)
            { /// Handle YUV formats, fallback to NV12f
                auto hasFullRange = _cameraPixelFormat & kPixelFormatYUVColorFullRangeBit;
                auto supportFullRange = [supportedFormats containsObject:@(kCVPixelFormatType_420YpCbCr8BiPlanarFullRange)];
                auto supportVideoRange = [supportedFormats containsObject:@(kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange)];

                if (supportFullRange || supportVideoRange)
                {
                    if (!hasFullRange && supportVideoRange)
                    {
                        _cameraPixelFormat = PixelFormat::NV12v;
                        _cvPixelFormat = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
                    }
                    else
                    {
                        _cameraPixelFormat = PixelFormat::NV12f;
                        _cvPixelFormat = kCVPixelFormatType_420YpCbCr8BiPlanarFullRange;
                    }
                }
            }

            if (_cvPixelFormat == 0)
            {
                auto hasOnlyRGB = pixelFormatInclude(_cameraPixelFormat, kPixelFormatRGBColorBit);
                auto supportRGB = [supportedFormats containsObject:@(kCVPixelFormatType_24RGB)];
                auto supportBGR = [supportedFormats containsObject:@(kCVPixelFormatType_24BGR)];
                auto supportBGRA = [supportedFormats containsObject:@(kCVPixelFormatType_32BGRA)];

                if (hasOnlyRGB && (supportRGB || supportBGR))
                {
                    if (supportBGR)
                    {
                        _cameraPixelFormat = PixelFormat::BGR24;
                        _cvPixelFormat = kCVPixelFormatType_24BGR;
                    }
                    else
                    {
                        _cameraPixelFormat = PixelFormat::RGB24;
                        _cvPixelFormat = kCVPixelFormatType_24RGB;
                    }
                }
                else
                {
                    if (supportBGRA)
                    {
                        _cameraPixelFormat = PixelFormat::BGRA32;
                        _cvPixelFormat = kCVPixelFormatType_32BGRA;
                    }
                    else
                    {
                        _cameraPixelFormat = PixelFormat::RGBA32;
                        _cvPixelFormat = kCVPixelFormatType_32RGBA;
                    }
                }
            }

            if (_cvPixelFormat == 0)
            { /// last fall back.
                _cvPixelFormat = kCVPixelFormatType_32BGRA;
                _cameraPixelFormat = PixelFormat::BGRA32;
            }

            if (_cameraPixelFormat != _convertPixelFormat)
            {
                if (!((requiredPixelFormat & kPixelFormatForceToSetBit) && (_cameraPixelFormat & kPixelFormatRGBColorBit)))
                { /// 暂时只支持 RGB 格式的 convert.
                    if (errorLogEnabled())
                    {
                        NSLog(@"ccap: CameraCaptureObjc init - convert pixel format not supported!!!");
                    }
                    _convertPixelFormat = _cameraPixelFormat;
                }
            }

            if (errorLogEnabled())
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
        if (verboseLogEnabled())
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
    return YES;
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

                    if (infoLogEnabled())
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
                    if (errorLogEnabled())
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
    switch (_cameraPixelFormat)
    {
    case PixelFormat::NV21v: /// MacOS does not support NV21, fallback to NV12
    case PixelFormat::I420v:
        if (errorLogEnabled())
        {
            NSLog(@"ccap: NV21v/I420v is not supported on macOS, fallback to NV12v");
        }
    case PixelFormat::NV12v:
        _cvPixelFormat = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
        _cameraPixelFormat = PixelFormat::NV12v;
        break;
    case PixelFormat::NV21f: /// MacOS does not support NV21, fallback to NV12
    case PixelFormat::I420f:
        if (errorLogEnabled())
        {
            NSLog(@"ccap: NV21f/I420f is not supported on macOS, fallback to NV12f");
        }
    case PixelFormat::NV12f:
        _cvPixelFormat = kCVPixelFormatType_420YpCbCr8BiPlanarFullRange;
        _cameraPixelFormat = PixelFormat::NV12f;
        break;
    case PixelFormat::BGRA32:
        _cvPixelFormat = kCVPixelFormatType_32BGRA;
        break;
    case PixelFormat::RGBA32:
        _cvPixelFormat = kCVPixelFormatType_32RGBA;
        break;
    case PixelFormat::BGR24:
        _cvPixelFormat = kCVPixelFormatType_24BGR;
        break;
    default: /// I420 is not supported on macOS
    case PixelFormat::RGB24:
        _cvPixelFormat = kCVPixelFormatType_24RGB;
        _cameraPixelFormat = PixelFormat::RGB24;
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
                    if (infoLogEnabled())
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

    if (errorLogEnabled())
    {
        NSLog(@"ccap: No connections available");
    }
}

- (BOOL)start
{
    if (_session && ![_session isRunning])
    {
        if (verboseLogEnabled())
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
        if (verboseLogEnabled())
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
        if (verboseLogEnabled())
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
    if (verboseLogEnabled())
    {
        NSLog(@"ccap: CameraCaptureObjc dealloc - begin");
    }

    [self destroy];

    if (verboseLogEnabled())
    {
        NSLog(@"ccap: CameraCaptureObjc dealloc - end");
    }
}

// 处理每一帧的数据
- (void)captureOutput:(AVCaptureOutput*)output didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection*)connection
{
    if (!_provider)
    {
        if (errorLogEnabled())
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
    newFrame->pixelFormat = _cameraPixelFormat;

    if (_cameraPixelFormat & kPixelFormatYUVColorBit)
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
        auto manager = std::make_shared<FakeFrame>([imageBuffer, newFrame]() mutable {
            CVPixelBufferUnlockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);
            CFRelease(imageBuffer);
            if (verboseLogEnabled())
            {
                NSLog(@"ccap: recycled YUV frame, width: %d, height: %d", (int)newFrame->width, (int)newFrame->height);
            }

            /// Make ref count + 1
            newFrame = nullptr;
        });

        newFrame->data[0] = (uint8_t*)CVPixelBufferGetBaseAddressOfPlane(imageBuffer, 0);
        newFrame->data[1] = (uint8_t*)CVPixelBufferGetBaseAddressOfPlane(imageBuffer, 1);
        auto fakeFrame = std::shared_ptr<Frame>(manager, newFrame.get());
        newFrame = fakeFrame;
    }
    else
    {
        bytes = CVPixelBufferGetDataSize(imageBuffer);
        newFrame->sizeInBytes = bytes;

        newFrame->data[0] = (uint8_t*)CVPixelBufferGetBaseAddress(imageBuffer);
        newFrame->data[1] = nullptr;
        newFrame->data[2] = nullptr;
        newFrame->stride[0] = CVPixelBufferGetBytesPerRow(imageBuffer);
        newFrame->stride[1] = 0;
        newFrame->stride[2] = 0;

        if (_cameraPixelFormat == _convertPixelFormat)
        {
            CFRetain(imageBuffer);
            auto manager = std::make_shared<FakeFrame>([imageBuffer, newFrame]() mutable {
                CVPixelBufferUnlockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);
                CFRelease(imageBuffer);
                if (verboseLogEnabled())
                {
                    NSLog(@"ccap: recycled RGBA frame, width: %d, height: %d", (int)newFrame->width, (int)newFrame->height);
                }
                /// Make ref count + 1
                newFrame = nullptr;
            });

            auto fakeFrame = std::shared_ptr<Frame>(manager, newFrame.get());
            newFrame = fakeFrame;
        }
        else
        {
            if (verboseLogEnabled())
            {
                NSLog(@"ccap: captureOutput - perform convert, width: %d, height: %d", (int)newFrame->width, (int)newFrame->height);
            }
            if (!newFrame->allocator)
            {
                auto&& f = _provider->getAllocatorFactory();
                newFrame->allocator = f ? f() : std::make_shared<DefaultAllocator>();
            }

            inplaceConvertFrame(newFrame.get(), _convertPixelFormat);
        }
    }

    newFrame->frameIndex = _provider->frameIndex()++;

    if (verboseLogEnabled())
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
        NSArray<AVCaptureDevice*>* devices = findAllDeviceName();
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
        if (errorLogEnabled())
        {
            NSLog(@"ccap: Camera is already opened");
        }
        return false;
    }

    @autoreleasepool
    {
        m_imp = [[CameraCaptureObjc alloc] initWithProvider:this];
        if (!deviceName.empty())
        {
            [m_imp setCameraName:@(deviceName.data())];
        }
        [m_imp setResolution:CGSizeMake(m_frameProp.width, m_frameProp.height)];
        return [m_imp open];
    }
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

ProviderImp* createProviderMac()
{
    return new ProviderMac();
}

} // namespace ccap

#endif