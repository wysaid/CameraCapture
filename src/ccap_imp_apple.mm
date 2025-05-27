/**
 * @file ccap_imp_macos.mm
 * @author wysaid (this@wysaid.org)
 * @brief Source file for ProviderApple class.
 * @date 2025-04
 *
 */

#if __APPLE__

#include "ccap_imp_apple.h"

#import <AVFoundation/AVFoundation.h>
#import <Accelerate/Accelerate.h>
#import <Foundation/Foundation.h>
#include <cassert>
#include <cmath>

#if _CCAP_LOG_ENABLED_
#include <deque>
#endif

using namespace ccap;

#if _CCAP_LOG_ENABLED_

#define CCAP_NSLOG(logLevel, ...) CCAP_CALL_LOG(logLevel, NSLog(__VA_ARGS__))
#define CCAP_NSLOG_E(...) CCAP_NSLOG(LogLevel::Error, __VA_ARGS__)
#define CCAP_NSLOG_W(...) CCAP_NSLOG(LogLevel::Warning, __VA_ARGS__)
#define CCAP_NSLOG_I(...) CCAP_NSLOG(LogLevel::Info, __VA_ARGS__)
#define CCAP_NSLOG_V(...) CCAP_NSLOG(LogLevel::Verbose, __VA_ARGS__)
#else
#define CCAP_NSLOG_E(...) ((void)0)
#define CCAP_NSLOG_W(...) ((void)0)
#define CCAP_NSLOG_I(...) ((void)0)
#define CCAP_NSLOG_V(...) ((void)0)
#endif

#if defined(DEBUG) && _CCAP_LOG_ENABLED_
#include <sys/sysctl.h>
#include <sys/types.h>
#include <unistd.h>

namespace ccap
{
extern bool globalLogLevelChanged;
}

[[maybe_unused]] static void optimizeLogIfNotSet()
{
    if (!globalLogLevelChanged)
    {
        int mib[4];
        struct kinfo_proc info{};
        size_t size = sizeof(info);

        mib[0] = CTL_KERN;
        mib[1] = KERN_PROC;
        mib[2] = KERN_PROC_PID;
        mib[3] = getpid();

        info.kp_proc.p_flag = 0;
        sysctl(mib, 4, &info, &size, NULL, 0);

        if ((info.kp_proc.p_flag & P_TRACED) != 0)
        { /// In debug mode, if logLevel has not been set, switch to verbose for easier troubleshooting.
            setLogLevel(LogLevel::Verbose);
            fputs("ccap: Debug mode detected, set log level to verbose.\n", stderr);
        }
    }
}
#else
#define optimizeLogIfNotSet() (void)0
#endif

namespace
{
constexpr FrameOrientation kDefaultFrameOrientation = FrameOrientation::TopToBottom;

struct PixelFormatInfo
{
    NSString* name{ nil };
    ccap::PixelFormat format{ ccap::PixelFormat::Unknown };
    std::string description;
};

#define MakeFormatInfo(format) format, #format

PixelFormatInfo getPixelFormatInfo(OSType format)
{ /// On macOS, available pixelFormats are limited, only listing the possible ones here.
    constexpr const char* unavailableMsg = "ccap unavailable for now";
    switch (format)
    {
    case kCVPixelFormatType_420YpCbCr8BiPlanarFullRange:
        return { @"kCVPixelFormatType_420YpCbCr8BiPlanarFullRange", MakeFormatInfo(PixelFormat::NV12f) };
    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
        return { @"kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange", MakeFormatInfo(PixelFormat::NV12) };
    case kCVPixelFormatType_420YpCbCr8PlanarFullRange:
        return { @"kCVPixelFormatType_420YpCbCr8PlanarFullRange", MakeFormatInfo(PixelFormat::I420f) };
    case kCVPixelFormatType_420YpCbCr8Planar:
        return { @"kCVPixelFormatType_420YpCbCr8Planar", MakeFormatInfo(PixelFormat::I420) };
    case kCVPixelFormatType_422YpCbCr8:
        return { @"kCVPixelFormatType_422YpCbCr8", PixelFormat::Unknown, unavailableMsg };
    case kCVPixelFormatType_422YpCbCr8_yuvs:
        return { @"kCVPixelFormatType_422YpCbCr8_yuvs", PixelFormat::Unknown, unavailableMsg };

        /////////////// ↓ RGB(A) ↓ ///////////////

    case kCVPixelFormatType_32BGRA:
        return { @"kCVPixelFormatType_32BGRA", MakeFormatInfo(PixelFormat::BGRA32) };
    case kCVPixelFormatType_24BGR:
        return { @"kCVPixelFormatType_24BGR", MakeFormatInfo(PixelFormat::BGR24) };
    case kCVPixelFormatType_24RGB:
        return { @"kCVPixelFormatType_24RGB", MakeFormatInfo(PixelFormat::RGB24) };
    case kCVPixelFormatType_32RGBA:
        return { @"kCVPixelFormatType_32RGBA", MakeFormatInfo(PixelFormat::RGBA32) };
    case kCVPixelFormatType_32ARGB:
        return { @"kCVPixelFormatType_32ARGB", PixelFormat::Unknown, unavailableMsg };
    default:
        return { [NSString stringWithFormat:@"Unknown(0x%08x)", (unsigned int)format], MakeFormatInfo(PixelFormat::Unknown) };
    }
}

struct ResolutionInfo
{
    AVCaptureSessionPreset preset = nil;
    DeviceInfo::Resolution resolution{};
};

std::vector<ResolutionInfo> allSupportedResolutions(AVCaptureSession* session)
{
    std::vector<ResolutionInfo> info = {
#if CCAP_MACOS
        { AVCaptureSessionPreset320x240, { 320, 240 } },
#endif
        { AVCaptureSessionPreset352x288, { 352, 288 } },
        { AVCaptureSessionPreset640x480, { 640, 480 } },
#if CCAP_MACOS
        { AVCaptureSessionPreset960x540, { 960, 540 } },
#endif
        { AVCaptureSessionPreset1280x720, { 1280, 720 } },
        { AVCaptureSessionPreset1920x1080, { 1920, 1080 } },
        { AVCaptureSessionPreset3840x2160, { 3840, 2160 } },
    };

    auto r = std::remove_if(info.begin(), info.end(), ^bool(const ResolutionInfo& i) {
        return ![session canSetSessionPreset:i.preset];
    });

    if (r != info.end())
    {
        info.erase(r, info.end());
    }
    return info;
}
} // namespace

NSArray<AVCaptureDevice*>* findAllDeviceName()
{
    NSMutableArray* allTypes = [NSMutableArray new];
    [allTypes addObject:AVCaptureDeviceTypeBuiltInWideAngleCamera];
#if CCAP_IOS
    [allTypes addObject:AVCaptureDeviceTypeBuiltInTelephotoCamera];
    [allTypes addObject:AVCaptureDeviceTypeBuiltInDualCamera];
    if (@available(iOS 13.0, *))
    {
        [allTypes addObject:AVCaptureDeviceTypeBuiltInUltraWideCamera];
        [allTypes addObject:AVCaptureDeviceTypeBuiltInDualWideCamera];
        [allTypes addObject:AVCaptureDeviceTypeBuiltInTripleCamera];
    }
#else
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
#endif

    AVCaptureDeviceDiscoverySession* discoverySession =
        [AVCaptureDeviceDiscoverySession discoverySessionWithDeviceTypes:allTypes
                                                               mediaType:AVMediaTypeVideo
                                                                position:AVCaptureDevicePositionUnspecified];
    if (infoLogEnabled())
    {
        static dispatch_once_t onceToken;
        dispatch_once(&onceToken, ^{
            NSLog(@"ccap: Available camera devices: %@", discoverySession.devices);
        });
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

void inplaceConvertFrame(VideoFrame* frame, PixelFormat toFormat, bool verticalFlip, std::vector<uint8_t>& memCache)
{
    auto* inputBytes = frame->data[0];
    auto inputLineSize = frame->stride[0];
    auto outputChannelCount = (toFormat & kPixelFormatAlphaColorBit) ? 4 : 3;

    // Ensure 16/32 byte alignment for best performance
    auto newLineSize = outputChannelCount == 3 ? ((frame->width * 3 + 31) & ~31) : (frame->width * 4);
    auto inputFormat = frame->pixelFormat;

    auto inputChannelCount = (inputFormat & kPixelFormatAlphaColorBit) ? 4 : 3;

    bool isInputRGB = inputFormat & kPixelFormatRGBBit; ///< Not RGB means BGR
    bool isOutputRGB = toFormat & kPixelFormatRGBBit;   ///< Not RGB means BGR
    bool swapRB = isInputRGB != isOutputRGB;            ///< Whether R and B channels need to be swapped
    bool flipOnly = verticalFlip && (inputChannelCount == outputChannelCount && !swapRB);

    frame->stride[0] = newLineSize;
    frame->allocator->resize(newLineSize * frame->height);
    frame->data[0] = frame->allocator->data();
    frame->pixelFormat = toFormat;

    if (flipOnly)
    { /// 只是上下翻转, 别的都不需要.

        vImage_Buffer fakeSrc = { inputBytes, frame->height, inputLineSize, inputLineSize };
        vImage_Buffer fakeDst = { frame->data[0], frame->height, newLineSize, newLineSize };
        vImageVerticalReflect_Planar8(&fakeSrc, &fakeDst, kvImageNoFlags);
        return;
    }

    vImage_Buffer src = { inputBytes, frame->height, frame->width, inputLineSize };
    vImage_Buffer dst;
    if (!verticalFlip) // 大概率是这个, 放前面
    {
        dst = { frame->data[0], frame->height, frame->width, newLineSize };
    }
    else
    { /// 当需要 flip 的时候, 先把结果写入到 memCache
        memCache.resize(newLineSize * frame->height);
        dst = { memCache.data(), frame->height, frame->width, newLineSize };
    }

    //// The cross-conversion between input and output would require many switch cases, simplifying here.

    if (inputChannelCount == outputChannelCount)
    { /// Conversion with the same number of channels, only RGB <-> BGR, RGBA <-> BGRA, swapRB must be true.
        assert(swapRB);
        if (inputChannelCount == 4)
        {
            // RGBA8888 <-> BGRA8888: Channel reordering required
            // vImagePermuteChannels_ARGB8888 needs 4 indices to implement arbitrary channel arrangement
            // RGBA8888: [R, G, B, A], BGRA8888: [B, G, R, A]
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
    { // Different number of channels, only 4 channels <-> 3 channels

        if (inputChannelCount == 4)
        { // 4 channels -> 3 channels
            if (swapRB)
            { // Possible cases: RGBA->BGR, BGRA->RGB
                vImageConvert_RGBA8888toBGR888(&src, &dst, kvImageNoFlags);
            }
            else
            { // Possible cases: RGBA->RGB, BGRA->BGR
                vImageConvert_RGBA8888toRGB888(&src, &dst, kvImageNoFlags);
            }
        }
        else
        { /// 3 channels -> 4 channels
            if (swapRB)
            { // Possible cases: BGR->RGBA, RGB->BGRA
                vImageConvert_RGB888toBGRA8888(&src, nullptr, 0xff, &dst, false, kvImageNoFlags);
            }
            else
            { // Possible cases: BGR->BGRA, RGB->RGBA
                vImageConvert_RGB888toRGBA8888(&src, nullptr, 0xff, &dst, false, kvImageNoFlags);
            }
        }
    }

    if (verticalFlip)
    { /// vImageVerticalReflect 不支持 inplace 操作, 所以只要 flip, 就需要用到这个 memCache
        if (outputChannelCount == 4)
        { /// 4 通道直接交换
            vImage_Buffer realDst = { frame->data[0], frame->height, frame->width, newLineSize };
            vImageVerticalReflect_ARGB8888(&dst, &realDst, kvImageNoFlags);
        }
        else /// 3 通道需要交换, 因为不存在 `vImageVerticalReflect_RGB888`, 所以转成单通道来实现.
        {
            vImage_Buffer fakeSrc = { memCache.data(), frame->height, newLineSize, newLineSize };
            vImage_Buffer fakeDst = { frame->data[0], frame->height, newLineSize, newLineSize };
            vImageVerticalReflect_Planar8(&fakeSrc, &fakeDst, kvImageNoFlags);
        }
    }
} // namespace

@interface CameraCaptureObjc : NSObject<AVCaptureVideoDataOutputSampleBufferDelegate>
{
    ProviderApple* _provider;

    std::vector<uint8_t> _memoryCache; ///< Memory cache used for storing temporary computation results
}

@property (nonatomic, strong) AVCaptureSession* session;
@property (nonatomic, strong) AVCaptureDevice* device;
@property (nonatomic, strong) NSString* cameraName;
@property (nonatomic, strong) AVCaptureDeviceInput* videoInput;
@property (nonatomic, strong) AVCaptureVideoDataOutput* videoOutput;
@property (nonatomic, strong) dispatch_queue_t captureQueue;
@property (nonatomic) CGSize resolution;
@property (nonatomic) OSType cvPixelFormat;
@property (nonatomic) BOOL opened;

- (instancetype)initWithProvider:(ProviderApple*)provider;
- (BOOL)start;
- (void)stop;

@end

@implementation CameraCaptureObjc

- (instancetype)initWithProvider:(ProviderApple*)provider
{
    self = [super init];
    if (self)
    {
        _provider = provider;
        _opened = NO;
    }
    return self;
}

- (BOOL)open
{
    AVAuthorizationStatus authStatus = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo];
    if (authStatus == AVAuthorizationStatusNotDetermined)
    {
        dispatch_semaphore_t sema = dispatch_semaphore_create(0);
        void (^requestAccess)(void) = ^(void) {
            [AVCaptureDevice requestAccessForMediaType:AVMediaTypeVideo completionHandler:^(BOOL granted) {
                CCAP_NSLOG_I(@"ccap: Camera access %@", granted ? @"granted" : @"denied");
                dispatch_semaphore_signal(sema);
            }];
        };

        // Permission must be requested on the main thread
        if (![NSThread isMainThread])
        {
            dispatch_async(dispatch_get_main_queue(), ^{
                requestAccess();
            });
        }
        else
        {
            requestAccess();
        }

        CCAP_NSLOG_I(@"ccap: Waiting for camera access permission...");
        dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
        authStatus = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo];
    }

    if (authStatus != AVAuthorizationStatusAuthorized)
    {
        CCAP_NSLOG_E(@"ccap: Camera access not authorized. Current status: %ld", (long)authStatus);
        return NO;
    }

    _session = [[AVCaptureSession alloc] init];
    [_session beginConfiguration];

    if (AVCaptureSessionPreset preset = AVCaptureSessionPresetHigh; _resolution.width > 0 && _resolution.height > 0)
    { /// Handle resolution

        auto allInfo = allSupportedResolutions(_session);
        CGSize inputSize = _resolution;

        for (auto& info : allInfo)
        {
            auto width = info.resolution.width;
            auto height = info.resolution.height;

            /// The info is sorted from small to large, so find the first match.
            if (width >= _resolution.width && height >= _resolution.height)
            {
                _resolution.width = width;
                _resolution.height = height;
                preset = info.preset;
                break;
            }
        }

        if (![_session canSetSessionPreset:preset])
        {
            CCAP_NSLOG_E(@"ccap: CameraCaptureObjc init - session preset not supported, using AVCaptureSessionPresetHigh");
            preset = AVCaptureSessionPresetHigh;
        }

        [_session setSessionPreset:preset];

        CCAP_NSLOG_I(@"ccap: Expected camera resolution: (%gx%g), actual matched camera resolution: (%gx%g)",
                     inputSize.width, inputSize.height, _resolution.width, _resolution.height);
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
        CCAP_NSLOG_E(@"ccap: No video device found");
        return NO;
    }

    CCAP_NSLOG_I(@"ccap: The camera to be used: %@", _device);

    /// Configure device

    NSError* error = nil;
    [_device lockForConfiguration:&error];
    if ([_device isFocusModeSupported:AVCaptureFocusModeContinuousAutoFocus])
    {
        [_device setFocusMode:AVCaptureFocusModeContinuousAutoFocus];
        CCAP_NSLOG_V(@"ccap: Set focus mode to continuous auto focus");
    }
    [_device unlockForConfiguration];

    // Create input device
    _videoInput = [[AVCaptureDeviceInput alloc] initWithDevice:_device error:&error];
    if (error)
    {
        [AVCaptureDeviceInput deviceInputWithDevice:self.device error:&error];
        if (error)
        {
            CCAP_NSLOG_E(@"ccap: Open camera failed: %@", error);
            return NO;
        }
    }

    // Add input device to session
    if ([_session canAddInput:_videoInput])
    {
        [_session addInput:_videoInput];

        CCAP_NSLOG_V(@"ccap: Add input to session");
    }
    else
    {
        CCAP_NSLOG_E(@"ccap: session can not add input");
        return NO;
    }

    // Create output device
    _videoOutput = [[AVCaptureVideoDataOutput alloc] init];
    [_videoOutput setAlwaysDiscardsLateVideoFrames:YES]; // better performance

    auto& requiredPixelFormat = _provider->getFrameProperty().outputPixelFormat;

    if (requiredPixelFormat == PixelFormat::Unknown)
    { /// Default to BGRA32 if not set
        requiredPixelFormat = PixelFormat::BGRA32;
    }

    auto& cameraPixelFormat = _provider->getFrameProperty().cameraPixelFormat;
    if (cameraPixelFormat == PixelFormat::Unknown)
    {
        cameraPixelFormat = requiredPixelFormat;
    }

    { /// Handle pixel format
        static_assert(sizeof(cameraPixelFormat) == sizeof(uint32_t), "size must match");

        [self fixPixelFormat];

        NSArray* supportedFormats = [_videoOutput availableVideoCVPixelFormatTypes];
        if (infoLogEnabled())
        {
            NSMutableArray* arr = [NSMutableArray new];
            for (NSNumber* format in supportedFormats)
            {
                auto info = getPixelFormatInfo([format unsignedIntValue]);
                [arr addObject:[NSString stringWithFormat:@"%@ (%s)", info.name, info.description.c_str()]];
            }
            NSLog(@"ccap: Supported pixel format: %@", arr);
        }

        OSType preferredFormat = _cvPixelFormat;
        if (![supportedFormats containsObject:@(preferredFormat)])
        {
            _cvPixelFormat = 0;
            if (bool hasYUV = cameraPixelFormat & kPixelFormatYUVColorBit)
            { /// Handle YUV formats, fallback to NV12f
                auto hasFullRange = cameraPixelFormat & kPixelFormatYUVColorFullRangeBit;
                auto supportFullRange = [supportedFormats containsObject:@(kCVPixelFormatType_420YpCbCr8BiPlanarFullRange)];
                auto supportVideoRange = [supportedFormats containsObject:@(kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange)];

                if (supportFullRange || supportVideoRange)
                {
                    if (!hasFullRange && supportVideoRange)
                    {
                        cameraPixelFormat = PixelFormat::NV12;
                        _cvPixelFormat = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
                    }
                    else
                    {
                        cameraPixelFormat = PixelFormat::NV12f;
                        _cvPixelFormat = kCVPixelFormatType_420YpCbCr8BiPlanarFullRange;
                    }
                }
            }

            if (_cvPixelFormat == 0)
            {
                auto hasOnlyRGB = pixelFormatInclude(cameraPixelFormat, kPixelFormatRGBColorBit);
                auto supportRGB = [supportedFormats containsObject:@(kCVPixelFormatType_24RGB)];
                auto supportBGR = [supportedFormats containsObject:@(kCVPixelFormatType_24BGR)];
                auto supportBGRA = [supportedFormats containsObject:@(kCVPixelFormatType_32BGRA)];

                if (hasOnlyRGB && (supportRGB || supportBGR))
                {
                    if (supportBGR)
                    {
                        cameraPixelFormat = PixelFormat::BGR24;
                        _cvPixelFormat = kCVPixelFormatType_24BGR;
                    }
                    else
                    {
                        cameraPixelFormat = PixelFormat::RGB24;
                        _cvPixelFormat = kCVPixelFormatType_24RGB;
                    }
                }
                else
                {
                    if (supportBGRA)
                    {
                        cameraPixelFormat = PixelFormat::BGRA32;
                        _cvPixelFormat = kCVPixelFormatType_32BGRA;
                    }
                    else
                    {
                        cameraPixelFormat = PixelFormat::RGBA32;
                        _cvPixelFormat = kCVPixelFormatType_32RGBA;
                    }
                }
            }

            if (_cvPixelFormat == 0)
            { /// last fall back.
                _cvPixelFormat = kCVPixelFormatType_32BGRA;
                cameraPixelFormat = PixelFormat::BGRA32;
            }

            if (ccap::errorLogEnabled())
            {
                if (cameraPixelFormat != requiredPixelFormat)
                {
                    if (!(cameraPixelFormat & kPixelFormatRGBColorBit))
                    { /// Currently only RGB format conversion is supported.
                        CCAP_NSLOG_E(@"ccap: CameraCaptureObjc init - convert pixel format not supported!!!");
                    }
                }

                auto preferredInfo = getPixelFormatInfo(preferredFormat);
                auto fallbackInfo = getPixelFormatInfo(_cvPixelFormat);
                CCAP_NSLOG_E(@"ccap: Preferred pixel format (%@-%s) not supported, fallback to: (%@-%s)", preferredInfo.name, preferredInfo.description.c_str(), fallbackInfo.name, fallbackInfo.description.c_str());
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
        CCAP_NSLOG_V(@"ccap: Add output to session");
    }
    else
    {
        CCAP_NSLOG_E(@"ccap: session can not add output");
        return NO;
    }

    [_session commitConfiguration];
    if (auto fps = _provider->getFrameProperty().fps; fps > 0.0)
    {
        [self setFrameRate:_provider->getFrameProperty().fps];
    }

    CCAP_NSLOG_V(@"ccap: videoOutput.connections,count = %lu", (unsigned long)_videoOutput.connections.count);
    [self flushResolution];
    _opened = YES;
    return _opened;
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
                    CCAP_NSLOG_E(@"ccap: Desired fps (%g) not supported, skipping", fps);
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
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
                        connection.videoMinFrameDuration = tm;
                        connection.videoMaxFrameDuration = tm;
#pragma clang diagnostic pop
                    }
                }
            }
        }

        [_device unlockForConfiguration];
    }
}

- (void)fixPixelFormat
{
    auto& internalFormat = _provider->getFrameProperty().cameraPixelFormat;
    switch (internalFormat)
    {
    case PixelFormat::I420:
        CCAP_NSLOG_E(@"ccap: I420 is not supported on macOS, fallback to NV12");
    case PixelFormat::NV12:
        _cvPixelFormat = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
        internalFormat = PixelFormat::NV12;
        break;
    case PixelFormat::I420f:
        CCAP_NSLOG_E(@"ccap: I420f is not supported on macOS, fallback to NV12f");
    case PixelFormat::NV12f:
        _cvPixelFormat = kCVPixelFormatType_420YpCbCr8BiPlanarFullRange;
        internalFormat = PixelFormat::NV12f;
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
        internalFormat = PixelFormat::RGB24;
        break;
    }
}

- (void)flushResolution
{
    if (@available(iOS 7.0, *))
    {
        if ([_device.activeFormat respondsToSelector:@selector(formatDescription)])
        {
            _resolution = CGSizeMake(CMVideoFormatDescriptionGetDimensions(_device.activeFormat.formatDescription).width,
                                     CMVideoFormatDescriptionGetDimensions(_device.activeFormat.formatDescription).height);
            return;
        }
    }

    if (_videoOutput && _videoOutput.connections.count > 0)
    {
        AVCaptureConnection* connection = [_videoOutput connections][0];
        if (connection)
        {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            CMFormatDescriptionRef formatDescription = connection.supportsVideoMinFrameDuration ? connection.inputPorts[0].formatDescription : nil;
#pragma clang diagnostic pop
            if (formatDescription)
            {
                CMVideoDimensions dimensions = CMVideoFormatDescriptionGetDimensions(formatDescription);
                if (dimensions.width != _resolution.width || dimensions.height != _resolution.height)
                {
                    CCAP_NSLOG_I(@"ccap: Actual camera resolution: %dx%d", dimensions.width, dimensions.height);

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

    CCAP_NSLOG_E(@"ccap: No connections available");
}

- (BOOL)start
{
    if (_session && _opened && ![_session isRunning])
    {
        CCAP_NSLOG_V(@"ccap: CameraCaptureObjc start");
        [_session startRunning];
    }
    return [_session isRunning];
}

- (void)stop
{
    if (_session && [_session isRunning])
    {
        CCAP_NSLOG_V(@"ccap: CameraCaptureObjc stop");
        [_session stopRunning];
    }
}

- (BOOL)isRunning
{
    return [_session isRunning];
}

- (void)destroy
{
    @autoreleasepool
    {
        if (_session)
        {
            CCAP_NSLOG_V(@"ccap: CameraCaptureObjc destroy");

            if ([_session isRunning])
            {
                [_session stopRunning];
            }

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
        _opened = NO;
    }
}

- (void)dealloc
{
    [self destroy];
}

// Process each frame of data
- (void)captureOutput:(AVCaptureOutput*)output didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer fromConnection:(AVCaptureConnection*)connection
{
    if (!_provider)
    {
        CCAP_NSLOG_E(@"ccap: CameraCaptureObjc captureOutput - provider is nil");
        return;
    }

    if (_provider->tooManyNewFrames())
    {
        if (!_provider->hasNewFrameCallback())
        {
            CCAP_NSLOG_I(@"ccap: VideoFrame dropped to avoid memory leak: grab() called less frequently than camera frame rate.");
            return;
        }
        else
        {
            CCAP_NSLOG_I(@"ccap: new frame callback returned false, but grab() was not called or is called less frequently than the camera frame rate");
        }
    }

    // Get the image buffer
    CVImageBufferRef imageBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);

    // Lock the image buffer to access its content
    CVPixelBufferLockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);

    auto newFrame = _provider->getFreeFrame();

    // Get resolution
    uint32_t width = (uint32_t)CVPixelBufferGetWidth(imageBuffer);
    uint32_t height = (uint32_t)CVPixelBufferGetHeight(imageBuffer);
    uint32_t bytes{};

    CMTime timestamp = CMSampleBufferGetPresentationTimeStamp(sampleBuffer);
    auto internalFormat = _provider->getFrameProperty().cameraPixelFormat;
    auto outputFormat = _provider->getFrameProperty().outputPixelFormat;

    newFrame->timestamp = (uint64_t)(CMTimeGetSeconds(timestamp) * 1e9);
    newFrame->width = width;
    newFrame->height = height;
    newFrame->pixelFormat = internalFormat;
    newFrame->nativeHandle = imageBuffer;

    if (internalFormat & kPixelFormatYUVColorBit)
    { /// 在 macOS 上， 只能是 NV12 或 NV12f 格式, 不予转换.
        newFrame->orientation = kDefaultFrameOrientation;
        uint32_t yBytesPerRow = (uint32_t)CVPixelBufferGetBytesPerRowOfPlane(imageBuffer, 0);
        uint32_t uvBytesPerRow = (uint32_t)CVPixelBufferGetBytesPerRowOfPlane(imageBuffer, 1);
        uint32_t yBytes = (uint32_t)CVPixelBufferGetHeightOfPlane(imageBuffer, 0) * yBytesPerRow;
        uint32_t uvBytes = (uint32_t)CVPixelBufferGetHeightOfPlane(imageBuffer, 1) * uvBytesPerRow;
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
            CCAP_NSLOG_V(@"ccap: recycled YUV frame, width: %d, height: %d", (int)newFrame->width, (int)newFrame->height);
            
            newFrame->nativeHandle = nullptr;
            newFrame = nullptr;
        });

        newFrame->data[0] = (uint8_t*)CVPixelBufferGetBaseAddressOfPlane(imageBuffer, 0);
        newFrame->data[1] = (uint8_t*)CVPixelBufferGetBaseAddressOfPlane(imageBuffer, 1);
        auto fakeFrame = std::shared_ptr<VideoFrame>(manager, newFrame.get());
        newFrame = fakeFrame;
    }
    else
    {
        newFrame->orientation = _provider->frameOrientation();
        bytes = (uint32_t)CVPixelBufferGetDataSize(imageBuffer);
        newFrame->sizeInBytes = bytes;

        newFrame->data[0] = (uint8_t*)CVPixelBufferGetBaseAddress(imageBuffer);
        newFrame->data[1] = nullptr;
        newFrame->data[2] = nullptr;
        newFrame->stride[0] = (uint32_t)CVPixelBufferGetBytesPerRow(imageBuffer);
        newFrame->stride[1] = 0;
        newFrame->stride[2] = 0;

        if (internalFormat == outputFormat && newFrame->orientation == kDefaultFrameOrientation)
        {
            CFRetain(imageBuffer);
            auto manager = std::make_shared<FakeFrame>([imageBuffer, newFrame]() mutable {
                CVPixelBufferUnlockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);
                CFRelease(imageBuffer);
                CCAP_NSLOG_V(@"ccap: recycled RGBA frame, width: %d, height: %d", (int)newFrame->width, (int)newFrame->height);
                
                newFrame->nativeHandle = nullptr;
                newFrame = nullptr;
            });

            auto fakeFrame = std::shared_ptr<VideoFrame>(manager, newFrame.get());
            newFrame = fakeFrame;
        }
        else
        {
            if (!newFrame->allocator)
            {
                auto&& f = _provider->getAllocatorFactory();
                newFrame->allocator = f ? f() : std::make_shared<DefaultAllocator>();
            }

            std::chrono::steady_clock::time_point startConvertTime;

            if (verboseLogEnabled())
            {
                startConvertTime = std::chrono::steady_clock::now();
            }

            inplaceConvertFrame(newFrame.get(), outputFormat, (int)(newFrame->orientation != kDefaultFrameOrientation), _memoryCache);

            CVPixelBufferUnlockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);

            if (verboseLogEnabled())
            {
#ifdef DEBUG
                constexpr const char* mode = "(using Debug mode)";
#else
                constexpr const char* mode = "(using Release mode)";
#endif

                auto endConvertTime = std::chrono::steady_clock::now();
                auto durationInUs = std::chrono::duration_cast<std::chrono::microseconds>(endConvertTime - startConvertTime);
                CCAP_NSLOG_V(@"ccap: captureOutput - perform convert, width: %d, height: %d, flip: %d, cost time(%s): %g ms", (int)newFrame->width, (int)newFrame->height, (int)(newFrame->orientation != kDefaultFrameOrientation), mode, durationInUs.count() / 1000.0);
            }
        }
    }

    newFrame->frameIndex = _provider->frameIndex()++;

    if (verboseLogEnabled())
    { /// Generally, camera interfaces are not called in multiple threads, and verbose logs are only for debugging, so no lock is needed here.
        static uint64_t s_lastFrameTime;
        static std::deque<uint64_t> s_durations;

        if (s_lastFrameTime != 0)
        {
            auto dur = newFrame->timestamp - s_lastFrameTime;
            s_durations.emplace_back(dur);
        }

        s_lastFrameTime = newFrame->timestamp;

        /// use a window of 30 frames to calculate the fps
        if (s_durations.size() > 30)
        {
            s_durations.pop_front();
        }

        double fps = 0.0;

        if (!s_durations.empty())
        {
            double sum = 0.0;
            for (auto& d : s_durations)
            {
                sum += d / 1e9f;
            }
            fps = std::round(s_durations.size() / sum * 10) / 10.0;
        }

        NSLog(@"ccap: New frame available: %ux%u, bytes %u, Data address: %p, fps: %g", width, height, bytes, newFrame->data[0], fps);
    }

    _provider->newFrameAvailable(std::move(newFrame));
}

@end

namespace ccap
{
ProviderApple::ProviderApple()
{
#ifdef CCAP_MACOS
    optimizeLogIfNotSet();
#endif
    m_frameOrientation = kDefaultFrameOrientation;
}

ProviderApple::~ProviderApple()
{
    ProviderApple::close();
}

std::vector<std::string> ProviderApple::findDeviceNames()
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

bool ProviderApple::open(std::string_view deviceName)
{
    if (m_imp != nil)
    {
        CCAP_NSLOG_E(@"ccap: Camera is already opened");
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

bool ProviderApple::isOpened() const
{
    return m_imp && m_imp.session && m_imp.opened;
}

std::optional<DeviceInfo> ProviderApple::getDeviceInfo() const
{
    std::optional<DeviceInfo> deviceInfo;
    if (m_imp && m_imp.videoOutput)
    {
        @autoreleasepool
        {
            NSString* deviceName = [m_imp.device localizedName];
            if ([deviceName length] > 0)
            {
                deviceInfo.emplace();
                deviceInfo->deviceName = [deviceName UTF8String];
                NSArray* supportedFormats = [m_imp.videoOutput availableVideoCVPixelFormatTypes];
                auto& formats = deviceInfo->supportedPixelFormats;
                formats.reserve(supportedFormats.count);
                for (NSNumber* format in supportedFormats)
                {
                    auto info = getPixelFormatInfo((OSType)[format unsignedIntValue]);
                    if (info.format != PixelFormat::Unknown)
                    {
                        formats.emplace_back(info.format);
                    }
                    else
                    {
                        CCAP_NSLOG_V(@"ccap: The OS native pixel format %@ currently not implemented", info.name);
                    }
                }

                auto allResolutions = allSupportedResolutions(m_imp.session);
                for (auto& info : allResolutions)
                {
                    deviceInfo->supportedResolutions.emplace_back(info.resolution);
                }
            }
        }
    }

    if (!deviceInfo)
    {
        CCAP_NSLOG_E(@"ccap: getDeviceInfo called with no device opened");
    }
    return deviceInfo;
}

void ProviderApple::close()
{
    if (m_imp)
    {
        [m_imp destroy];
        m_imp = nil;
    }
}

bool ProviderApple::start()
{
    if (!isOpened())
    {
        CCAP_NSLOG_W(@"ccap: camera start called with no device opened");
        return false;
    }

    @autoreleasepool
    {
        return [m_imp start];
    }
}

void ProviderApple::stop()
{
    if (m_imp)
    {
        @autoreleasepool
        {
            [m_imp stop];
        }
    }
}

bool ProviderApple::isStarted() const
{
    return m_imp && [m_imp isRunning];
}

ProviderImp* createProviderApple()
{
    return new ProviderApple();
}

} // namespace ccap

#endif
