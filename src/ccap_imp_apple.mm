/**
 * @file ccap_imp_macos.mm
 * @author wysaid (this@wysaid.org)
 * @brief Source file for ProviderApple class.
 * @date 2025-04
 *
 */

#if __APPLE__

#include "ccap_imp_apple.h"
#include "ccap_file_reader_apple.h"

#include "ccap_convert.h"
#include "ccap_convert_frame.h"

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

namespace ccap {
extern bool globalLogLevelChanged;
}

[[maybe_unused]] static void optimizeLogIfNotSet() {
    if (!globalLogLevelChanged) {
        int mib[4];
        struct kinfo_proc info{};
        size_t size = sizeof(info);

        mib[0] = CTL_KERN;
        mib[1] = KERN_PROC;
        mib[2] = KERN_PROC_PID;
        mib[3] = getpid();

        info.kp_proc.p_flag = 0;
        sysctl(mib, 4, &info, &size, NULL, 0);

        if ((info.kp_proc.p_flag & P_TRACED) !=
            0) { /// In debug mode, if logLevel has not been set, switch to verbose for easier troubleshooting.
            setLogLevel(LogLevel::Verbose);
            fputs("ccap: Debug mode detected, set log level to verbose.\n", stderr);
        }
    }
}
#else
#define optimizeLogIfNotSet() (void)0
#endif

namespace {
constexpr FrameOrientation kDefaultFrameOrientation = FrameOrientation::TopToBottom;

struct PixelFormatInfo {
    NSString* name{ nil };
    ccap::PixelFormat format{ ccap::PixelFormat::Unknown };
    std::string description;
};

#define MakeFormatInfo(format) format, #format

PixelFormatInfo getPixelFormatInfo(OSType format) { /// On macOS, available pixelFormats are limited, only listing the possible ones here.
    constexpr const char* unavailableMsg = "ccap unavailable for now";
    switch (format) {
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

struct ResolutionInfo {
    AVCaptureSessionPreset preset = nil;
    DeviceInfo::Resolution resolution{};
};

std::vector<ResolutionInfo> allSupportedResolutions(AVCaptureSession* session) {
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

    auto r = std::remove_if(info.begin(), info.end(), ^bool(const ResolutionInfo& i) { return ![session canSetSessionPreset:i.preset]; });

    if (r != info.end()) {
        info.erase(r, info.end());
    }
    return info;
}
} // namespace

NSArray<AVCaptureDevice*>* findAllDeviceName() {
    NSMutableArray* allTypes = [NSMutableArray new];
    [allTypes addObject:AVCaptureDeviceTypeBuiltInWideAngleCamera];
#if CCAP_IOS
    [allTypes addObject:AVCaptureDeviceTypeBuiltInTelephotoCamera];
    [allTypes addObject:AVCaptureDeviceTypeBuiltInDualCamera];
    if (@available(iOS 13.0, *)) {
        [allTypes addObject:AVCaptureDeviceTypeBuiltInUltraWideCamera];
        [allTypes addObject:AVCaptureDeviceTypeBuiltInDualWideCamera];
        [allTypes addObject:AVCaptureDeviceTypeBuiltInTripleCamera];
    }
#else
    if (@available(macOS 14.0, *)) {
        [allTypes addObject:AVCaptureDeviceTypeExternal];
    } else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        [allTypes addObject:AVCaptureDeviceTypeExternalUnknown];
#pragma clang diagnostic pop
    }
#endif

    AVCaptureDeviceDiscoverySession* discoverySession =
        [AVCaptureDeviceDiscoverySession discoverySessionWithDeviceTypes:allTypes mediaType:AVMediaTypeVideo
                                                                position:AVCaptureDevicePositionUnspecified];
    if (infoLogEnabled()) {
        static dispatch_once_t onceToken;
        dispatch_once(&onceToken, ^{ CCAP_NSLOG_I(@"ccap: Available camera devices: %@", discoverySession.devices); });
    }

    std::vector<std::string_view> virtualDevicePatterns = {
        "obs",
        "virtual",
    };

    auto countIndex = [&](AVCaptureDevice* device) {
        NSUInteger idx = [allTypes indexOfObject:device.deviceType] * 100 + 1000;
        if (idx == NSNotFound) return allTypes.count * 100;

        // Here, check if it is a virtual camera like OBS, and if so, add 10 to the index.
        std::string name = [[device.localizedName lowercaseString] UTF8String];

        for (auto& pattern : virtualDevicePatterns) {
            if (name.find(pattern) != std::string::npos) {
                idx += 10;
                break;
            }
        }

        // Typically, real cameras support multiple formats, but virtual cameras usually support only one format.
        // If the device supports multiple formats, subtract the count of formats from the index.
        if (int c = (int)device.formats.count; c > 1) {
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

@interface CameraCaptureObjc : NSObject<AVCaptureVideoDataOutputSampleBufferDelegate> {
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

- (instancetype)initWithProvider:(ProviderApple*)provider {
    self = [super init];
    if (self) {
        _provider = provider;
        _opened = NO;
    }
    return self;
}

- (BOOL)open {
    AVAuthorizationStatus authStatus = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo];
    if (authStatus == AVAuthorizationStatusNotDetermined) {
        dispatch_semaphore_t sema = dispatch_semaphore_create(0);
        void (^requestAccess)(void) = ^(void) {
            [AVCaptureDevice requestAccessForMediaType:AVMediaTypeVideo completionHandler:^(BOOL granted) {
                CCAP_NSLOG_I(@"ccap: Camera access %@", granted ? @"granted" : @"denied");
                dispatch_semaphore_signal(sema);
            }];
        };

        // Permission must be requested on the main thread
        if (![NSThread isMainThread]) {
            dispatch_async(dispatch_get_main_queue(), ^{ requestAccess(); });
        } else {
            requestAccess();
        }

        CCAP_NSLOG_I(@"ccap: Waiting for camera access permission...");
        dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
        authStatus = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo];
    }

    if (authStatus != AVAuthorizationStatusAuthorized) {
        reportError(ErrorCode::DeviceOpenFailed, "Camera access not authorized");
        return NO;
    }

    _session = [[AVCaptureSession alloc] init];
    [_session beginConfiguration];

    if (AVCaptureSessionPreset preset = AVCaptureSessionPresetHigh; _resolution.width > 0 && _resolution.height > 0) { /// Handle resolution

        auto allInfo = allSupportedResolutions(_session);
        CGSize inputSize = _resolution;

        for (auto& info : allInfo) {
            auto width = info.resolution.width;
            auto height = info.resolution.height;

            /// The info is sorted from small to large, so find the first match.
            if (width >= _resolution.width && height >= _resolution.height) {
                _resolution.width = width;
                _resolution.height = height;
                preset = info.preset;
                break;
            }
        }

        if (![_session canSetSessionPreset:preset]) {
            CCAP_NSLOG_W(@"ccap: CameraCaptureObjc init - session preset not supported, using AVCaptureSessionPresetHigh");
            preset = AVCaptureSessionPresetHigh;
        }

        [_session setSessionPreset:preset];

        CCAP_NSLOG_I(@"ccap: Expected camera resolution: (%gx%g), actual matched camera resolution: (%gx%g)", inputSize.width,
                     inputSize.height, _resolution.width, _resolution.height);
    }

    if (_cameraName != nil && _cameraName.length > 0) { /// Find preferred device
        NSArray<AVCaptureDevice*>* devices = findAllDeviceName();
        for (AVCaptureDevice* d in devices) {
            if ([d.localizedName caseInsensitiveCompare:_cameraName] == NSOrderedSame) {
                _device = d;
                break;
            }
        }
    }

    if (!_device) { /// Fallback to default device
        _device = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];
    }

    if (![_device hasMediaType:AVMediaTypeVideo]) { /// No video device found
        reportError(ErrorCode::NoDeviceFound, "No video device found");
        return NO;
    }

    CCAP_NSLOG_I(@"ccap: The camera to be used: %@", _device);

    /// Configure device

    NSError* error = nil;
    if ([_device lockForConfiguration:&error]) {
        if ([_device isFocusModeSupported:AVCaptureFocusModeContinuousAutoFocus]) {
            [_device setFocusMode:AVCaptureFocusModeContinuousAutoFocus];
            CCAP_NSLOG_V(@"ccap: Set focus mode to continuous auto focus");
        }
        [_device unlockForConfiguration];
    } else {
        CCAP_NSLOG_W(@"ccap: lockForConfiguration failed: %@", error.localizedDescription);
    }

    // Create input device
    _videoInput = [AVCaptureDeviceInput deviceInputWithDevice:_device error:&error];
    if (!_videoInput || error) {
        reportError(ErrorCode::DeviceOpenFailed, "Open camera failed" + (error && error.localizedDescription ? (": " + std::string([error.localizedDescription UTF8String])) : ""));
        return NO;
    }

    // Add input device to session
    if ([_session canAddInput:_videoInput]) {
        [_session addInput:_videoInput];

        CCAP_NSLOG_V(@"ccap: Add input to session");
    } else {
        reportError(ErrorCode::DeviceOpenFailed, "Session cannot add input device");
        return NO;
    }

    // Create output device
    _videoOutput = [[AVCaptureVideoDataOutput alloc] init];
    [_videoOutput setAlwaysDiscardsLateVideoFrames:YES]; // better performance

    auto& requiredPixelFormat = _provider->getFrameProperty().outputPixelFormat;

    if (requiredPixelFormat == PixelFormat::Unknown) { /// Default to BGRA32 if not set
        requiredPixelFormat = PixelFormat::BGRA32;
    }

    auto& cameraPixelFormat = _provider->getFrameProperty().cameraPixelFormat;
    if (cameraPixelFormat == PixelFormat::Unknown) {
        cameraPixelFormat = requiredPixelFormat;
    }

    { /// Handle pixel format
        static_assert(sizeof(cameraPixelFormat) == sizeof(uint32_t), "size must match");

        [self fixPixelFormat];

        NSArray* supportedFormats = [_videoOutput availableVideoCVPixelFormatTypes];
        if (infoLogEnabled()) {
            NSMutableArray* arr = [NSMutableArray new];
            for (NSNumber* format in supportedFormats) {
                auto info = getPixelFormatInfo([format unsignedIntValue]);
                [arr addObject:[NSString stringWithFormat:@"%@ (%s)", info.name, info.description.c_str()]];
            }
            CCAP_NSLOG_I(@"ccap: Supported pixel format: %@", arr);
        }

        OSType preferredFormat = _cvPixelFormat;
        if (![supportedFormats containsObject:@(preferredFormat)]) {
            _cvPixelFormat = 0;
            if (bool hasYUV = cameraPixelFormat & kPixelFormatYUVColorBit) { /// Handle YUV formats, fallback to NV12f
                auto hasFullRange = cameraPixelFormat & kPixelFormatYUVColorFullRangeBit;
                auto supportFullRange = [supportedFormats containsObject:@(kCVPixelFormatType_420YpCbCr8BiPlanarFullRange)];
                auto supportVideoRange = [supportedFormats containsObject:@(kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange)];

                if (supportFullRange || supportVideoRange) {
                    if (!hasFullRange && supportVideoRange) {
                        cameraPixelFormat = PixelFormat::NV12;
                        _cvPixelFormat = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
                    } else {
                        cameraPixelFormat = PixelFormat::NV12f;
                        _cvPixelFormat = kCVPixelFormatType_420YpCbCr8BiPlanarFullRange;
                    }
                }
            }

            if (_cvPixelFormat == 0) {
                auto hasOnlyRGB = pixelFormatInclude(cameraPixelFormat, kPixelFormatRGBColorBit);
                auto supportRGB = [supportedFormats containsObject:@(kCVPixelFormatType_24RGB)];
                auto supportBGR = [supportedFormats containsObject:@(kCVPixelFormatType_24BGR)];
                auto supportBGRA = [supportedFormats containsObject:@(kCVPixelFormatType_32BGRA)];

                if (hasOnlyRGB && (supportRGB || supportBGR)) {
                    if (supportBGR) {
                        cameraPixelFormat = PixelFormat::BGR24;
                        _cvPixelFormat = kCVPixelFormatType_24BGR;
                    } else {
                        cameraPixelFormat = PixelFormat::RGB24;
                        _cvPixelFormat = kCVPixelFormatType_24RGB;
                    }
                } else {
                    if (supportBGRA) {
                        cameraPixelFormat = PixelFormat::BGRA32;
                        _cvPixelFormat = kCVPixelFormatType_32BGRA;
                    } else {
                        cameraPixelFormat = PixelFormat::RGBA32;
                        _cvPixelFormat = kCVPixelFormatType_32RGBA;
                    }
                }
            }

            if (_cvPixelFormat == 0) { /// last fall back.
                _cvPixelFormat = kCVPixelFormatType_32BGRA;
                cameraPixelFormat = PixelFormat::BGRA32;
            }

            if ((cameraPixelFormat & kPixelFormatRGBColorBit) &&
                (requiredPixelFormat & kPixelFormatYUVColorBit)) { /// If the camera output is not YUV, but the required format is YUV,
                                                                   /// fallback to cameraPixelFormat.
                requiredPixelFormat = cameraPixelFormat;
            }

            if (ccap::errorLogEnabled()) {
                if (cameraPixelFormat != requiredPixelFormat) {
                    if (!(cameraPixelFormat & kPixelFormatRGBColorBit)) { /// Currently only RGB format conversion is supported.
                        CCAP_NSLOG_E(@"ccap: CameraCaptureObjc init - convert pixel format not supported!!!");
                    }
                }

                auto preferredInfo = getPixelFormatInfo(preferredFormat);
                auto fallbackInfo = getPixelFormatInfo(_cvPixelFormat);
                CCAP_NSLOG_E(@"ccap: Preferred pixel format (%@-%s) not supported, fallback to: (%@-%s)", preferredInfo.name,
                             preferredInfo.description.c_str(), fallbackInfo.name, fallbackInfo.description.c_str());
                reportError(ErrorCode::UnsupportedPixelFormat, "Preferred pixel format not supported, fallback to: " + fallbackInfo.description);
            }
        }

        _videoOutput.videoSettings = @{(id)kCVPixelBufferPixelFormatTypeKey : @(_cvPixelFormat)};
    }

    // Set output queue
    _captureQueue = dispatch_queue_create("ccap.queue", DISPATCH_QUEUE_SERIAL);
    [_videoOutput setSampleBufferDelegate:self queue:_captureQueue];

    // Add output device to session
    if ([_session canAddOutput:_videoOutput]) {
        [_session addOutput:_videoOutput];
        CCAP_NSLOG_V(@"ccap: Add output to session");
    } else {
        reportError(ErrorCode::DeviceOpenFailed, "Session cannot add output device");
        return NO;
    }

    [_session commitConfiguration];

    // Log supported formats and frame rates for debugging
    if (infoLogEnabled() && _device) {
        NSMutableString* formatInfo = [NSMutableString stringWithString:@"ccap: Available formats and frame rates:\n"];
        for (AVCaptureDeviceFormat* format in _device.formats) {
            CMVideoDimensions dimensions = CMVideoFormatDescriptionGetDimensions(format.formatDescription);
            [formatInfo appendFormat:@"  %dx%d: ", dimensions.width, dimensions.height];

            for (AVFrameRateRange* range in format.videoSupportedFrameRateRanges) {
                [formatInfo appendFormat:@"%.1f-%.1f fps, ", range.minFrameRate, range.maxFrameRate];
            }
            [formatInfo appendString:@"\n"];
        }
        CCAP_NSLOG_I(@"%@", formatInfo);
    }

    if (auto fps = _provider->getFrameProperty().fps; fps > 0.0) {
        [self setFrameRate:_provider->getFrameProperty().fps];
    }

    CCAP_NSLOG_V(@"ccap: videoOutput.connections,count = %lu", (unsigned long)_videoOutput.connections.count);
    [self flushResolution];
    _opened = YES;
    return _opened;
}

- (void)setFrameRate:(double)fps {
    if (_device) {
        NSError* error;
        [_device lockForConfiguration:&error];

        if ([_device respondsToSelector:@selector(setActiveVideoMinFrameDuration:)] &&
            [_device respondsToSelector:@selector(setActiveVideoMaxFrameDuration:)]) {
            if (error == nil) {
                double desiredFps = fps;

                // First, try to find a format that supports the desired fps
                AVCaptureDeviceFormat* bestFormat = nil;
                AVFrameRateRange* bestRange = nil;
                double bestFps = desiredFps;
                double bestDistance = 1e9;

                // Get current resolution for format matching
                CGSize currentResolution = _resolution;
                if (currentResolution.width <= 0 || currentResolution.height <= 0) {
                    if (_device.activeFormat) {
                        CMVideoDimensions dimensions = CMVideoFormatDescriptionGetDimensions(_device.activeFormat.formatDescription);
                        currentResolution = CGSizeMake(dimensions.width, dimensions.height);
                    }
                }

                // Search through all formats to find one that supports the desired fps
                for (AVCaptureDeviceFormat* format in _device.formats) {
                    CMVideoDimensions dimensions = CMVideoFormatDescriptionGetDimensions(format.formatDescription);

                    // Skip formats with very different resolutions unless current resolution is unknown
                    if (currentResolution.width > 0 && currentResolution.height > 0) {
                        double widthRatio = (double)dimensions.width / currentResolution.width;
                        double heightRatio = (double)dimensions.height / currentResolution.height;
                        // Allow some tolerance for resolution matching
                        if (widthRatio < 0.8 || widthRatio > 1.25 || heightRatio < 0.8 || heightRatio > 1.25) {
                            continue;
                        }
                    }

                    for (AVFrameRateRange* range in format.videoSupportedFrameRateRanges) {
                        double actualFps;
                        if (desiredFps >= range.minFrameRate && desiredFps <= range.maxFrameRate) {
                            // Desired fps is within range, use it directly
                            actualFps = desiredFps;
                        } else {
                            // Choose the closest boundary value
                            if (desiredFps < range.minFrameRate) {
                                actualFps = range.minFrameRate;
                            } else {
                                actualFps = range.maxFrameRate;
                            }
                        }

                        double distance = std::abs(actualFps - desiredFps);
                        if (distance < bestDistance) {
                            bestFormat = format;
                            bestRange = range;
                            bestFps = actualFps;
                            bestDistance = distance;
                        }
                    }
                }

                // If we found a better format, switch to it
                if (bestFormat && bestFormat != _device.activeFormat) {
                    _device.activeFormat = bestFormat;
                    CCAP_NSLOG_I(@"ccap: Switched to format that supports %g fps", bestFps);

                    // Update resolution after format change
                    CMVideoDimensions newDimensions = CMVideoFormatDescriptionGetDimensions(bestFormat.formatDescription);
                    _resolution = CGSizeMake(newDimensions.width, newDimensions.height);
                    if (_provider) {
                        auto& prop = _provider->getFrameProperty();
                        prop.width = newDimensions.width;
                        prop.height = newDimensions.height;
                    }
                }

                fps = bestFps;

                if (bestRange) {
                    // Create precise frame duration for the selected fps
                    // Use tolerance to handle floating-point fps values and cover common NTSC fractional rates
                    CMTime frameDuration;

                    const double fpsEps = 0.01; // tighter tolerance to distinguish 30.0 vs 29.97, 24.0 vs 23.976
                    auto approx = [&](double t) { return std::abs(fps - t) < fpsEps; };

                    // Prefer fractional NTSC rates first, then integer rates
                    if (approx(239.76)) {
                        // 239.76 fps ~= 240000/1001 -> duration = 1001/240000 s
                        frameDuration = CMTimeMake(1001, 240000);
                    } else if (approx(240.0)) {
                        // 240.00 fps -> 1/240 s
                        frameDuration = CMTimeMake(1, 240);
                    } else if (approx(119.88)) {
                        // 119.88 fps ~= 120000/1001 -> duration = 1001/120000 s
                        frameDuration = CMTimeMake(1001, 120000);
                    } else if (approx(120.0)) {
                        // 120.00 fps -> 1/120 s
                        frameDuration = CMTimeMake(1, 120);
                    } else if (approx(59.94)) {
                        // 59.94 fps ~= 60000/1001 -> duration = 1001/60000 s
                        frameDuration = CMTimeMake(1001, 60000);
                    } else if (approx(60.0)) {
                        // 60.00 fps -> 1/60 s
                        frameDuration = CMTimeMake(1, 60);
                    } else if (approx(29.97)) {
                        // 29.97 fps ~= 30000/1001 -> duration = 1001/30000 s
                        frameDuration = CMTimeMake(1001, 30000);
                    } else if (approx(30.0)) {
                        // 30.00 fps -> 1/30 s
                        frameDuration = CMTimeMake(1, 30);
                    } else if (approx(23.976)) {
                        // 23.976 fps ~= 24000/1001 -> duration = 1001/24000 s
                        frameDuration = CMTimeMake(1001, 24000);
                    } else if (approx(24.0)) {
                        // 24.00 fps -> 1/24 s
                        frameDuration = CMTimeMake(1, 24);
                    } else {
                        // Fallback: use high time scale to retain precision and avoid integer truncation
                        frameDuration = CMTimeMakeWithSeconds(1.0 / fps, 60000);
                    }

                    // Clamp frameDuration to the supported range to prevent exceptions
                    if (CMTimeCompare(frameDuration, bestRange.minFrameDuration) < 0) {
                        frameDuration = bestRange.minFrameDuration;
                    } else if (CMTimeCompare(frameDuration, bestRange.maxFrameDuration) > 0) {
                        frameDuration = bestRange.maxFrameDuration;
                    }

                    [_device setActiveVideoMinFrameDuration:frameDuration];
                    [_device setActiveVideoMaxFrameDuration:frameDuration];
                    _provider->getFrameProperty().fps = fps;

                    if (infoLogEnabled()) {
                        if (std::abs(fps - desiredFps) > 0.01) {
                            CCAP_NSLOG_I(@"ccap: Set fps to %g, but actual fps is %g", desiredFps, fps);
                        } else {
                            CCAP_NSLOG_I(@"ccap: Set fps to %g", fps);
                        }
                    }
                } else {
                    reportError(ErrorCode::FrameRateSetFailed, "Desired fps (" + std::to_string(fps) + ") not supported, using fallback");
                }
            }
        } else {
            for (AVCaptureConnection* connection in _videoOutput.connections) {
                for (AVCaptureInputPort* port in connection.inputPorts) {
                    if ([port.mediaType isEqualToString:AVMediaTypeVideo]) {
                        // Use a precise frame duration for legacy API path as well
                        auto tm = CMTimeMakeWithSeconds(1.0 / fps, 60000);
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

- (void)fixPixelFormat {
    auto& internalFormat = _provider->getFrameProperty().cameraPixelFormat;
    switch (internalFormat) {
    case PixelFormat::I420:
        reportError(ErrorCode::UnsupportedPixelFormat, "I420 is not supported on macOS, fallback to NV12");
    case PixelFormat::NV12:
        _cvPixelFormat = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
        internalFormat = PixelFormat::NV12;
        break;
    case PixelFormat::I420f:
        reportError(ErrorCode::UnsupportedPixelFormat, "I420f is not supported on macOS, fallback to NV12f");
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

- (void)flushResolution {
    if (@available(iOS 7.0, *)) {
        if ([_device.activeFormat respondsToSelector:@selector(formatDescription)]) {
            _resolution = CGSizeMake(CMVideoFormatDescriptionGetDimensions(_device.activeFormat.formatDescription).width,
                                     CMVideoFormatDescriptionGetDimensions(_device.activeFormat.formatDescription).height);
            return;
        }
    }

    if (_videoOutput && _videoOutput.connections.count > 0) {
        AVCaptureConnection* connection = [_videoOutput connections][0];
        if (connection) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            CMFormatDescriptionRef formatDescription =
                connection.supportsVideoMinFrameDuration ? connection.inputPorts[0].formatDescription : nil;
#pragma clang diagnostic pop
            if (formatDescription) {
                CMVideoDimensions dimensions = CMVideoFormatDescriptionGetDimensions(formatDescription);
                if (dimensions.width != _resolution.width || dimensions.height != _resolution.height) {
                    CCAP_NSLOG_I(@"ccap: Actual camera resolution: %dx%d", dimensions.width, dimensions.height);

                    _resolution.width = dimensions.width;
                    _resolution.height = dimensions.height;
                    if (_provider) {
                        auto& prop = _provider->getFrameProperty();
                        prop.width = dimensions.width;
                        prop.height = dimensions.height;
                    }
                }
            }
            return;
        }
    }

    reportError(ErrorCode::InitializationFailed, "No connections available");
}

- (BOOL)start {
    if (_session && _opened && ![_session isRunning]) {
        CGSize targetResolution = _resolution;
        CCAP_NSLOG_V(@"ccap: CameraCaptureObjc start");
        [_session startRunning];
        
        // If the format was changed by session start, restore it
        if (_device && _device.activeFormat && targetResolution.width > 0 && targetResolution.height > 0) {
            CMVideoDimensions afterStart = CMVideoFormatDescriptionGetDimensions(_device.activeFormat.formatDescription);
            if (afterStart.width != targetResolution.width || afterStart.height != targetResolution.height) {
                CCAP_NSLOG_V(@"ccap: Session start changed format from %gx%g, restoring", targetResolution.width, targetResolution.height);
                [self setCameraResolution:targetResolution];
            }
        }
    }
    return [_session isRunning];
}

- (void)setCameraResolution:(CGSize)targetResolution {
    if (!_device) return;
    
    NSError* error = nil;
    if ([_device lockForConfiguration:&error]) {
        AVCaptureDeviceFormat* bestFormat = nil;
        double closestDistance = 1e9;
        
        for (AVCaptureDeviceFormat* format in _device.formats) {
            CMVideoDimensions dimensions = CMVideoFormatDescriptionGetDimensions(format.formatDescription);
            
            /// If we find an exact match, use it immediately
            if (dimensions.width == targetResolution.width && dimensions.height == targetResolution.height) {
                bestFormat = format;
                break;
            }
            
            /// Otherwise, calculate distance for closest match
            double distance = std::abs(dimensions.width - targetResolution.width) + std::abs(dimensions.height - targetResolution.height);
            if (distance < closestDistance) {
                closestDistance = distance;
                bestFormat = format;
            }
        }
        
        if (bestFormat) {
            [_device setActiveFormat:bestFormat];
            CMVideoDimensions actualDimensions = CMVideoFormatDescriptionGetDimensions(bestFormat.formatDescription);
            CCAP_NSLOG_V(@"ccap: Restored device format to: %dx%d", actualDimensions.width, actualDimensions.height);
            
            // Update internal resolution tracking
            _resolution = CGSizeMake(actualDimensions.width, actualDimensions.height);
        }
        
        [_device unlockForConfiguration];
    } else {
        CCAP_NSLOG_W(@"ccap: Failed to lock device for format restoration: %@", error.localizedDescription);
    }
}

- (void)stop {
    if (_session && [_session isRunning]) {
        CCAP_NSLOG_V(@"ccap: CameraCaptureObjc stop");
        [_session stopRunning];
    }
    
    // Notify waiting grab() calls that camera has stopped
    if (_provider) {
        _provider->notifyGrabWaiters();
    }
}

- (BOOL)isRunning {
    return [_session isRunning];
}

- (void)destroy {
    @autoreleasepool {
        if (_session) {
            CCAP_NSLOG_V(@"ccap: CameraCaptureObjc destroy");

            if ([_session isRunning]) {
                [_session stopRunning];
            }

            [_videoOutput setSampleBufferDelegate:nil queue:dispatch_get_main_queue()];

            [_session beginConfiguration];

            if (_videoInput) {
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

- (void)dealloc {
    [self destroy];
}

// Process each frame of data
- (void)captureOutput:(AVCaptureOutput*)output
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
           fromConnection:(AVCaptureConnection*)connection {
    if (!_provider) {
        reportError(ErrorCode::InitializationFailed, "CameraCaptureObjc captureOutput - provider is nil");
        return;
    }

    if (_provider->tooManyNewFrames()) {
        if (!_provider->hasNewFrameCallback()) {
            CCAP_NSLOG_I(@"ccap: VideoFrame dropped to avoid memory leak: grab() called less frequently than camera frame rate.");
            return;
        } else {
            CCAP_NSLOG_I(
                @"ccap: new frame callback returned false, but grab() was not called or is called less frequently than the camera frame rate");
        }
    }

    // Get the image buffer
    CVImageBufferRef imageBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);

    // Lock the image buffer to access its content
    CVPixelBufferLockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);

    auto newFrame = _provider->getFreeFrame();

    CMTime timestamp = CMSampleBufferGetPresentationTimeStamp(sampleBuffer);
    auto internalFormat = _provider->getFrameProperty().cameraPixelFormat;
    auto outputFormat = _provider->getFrameProperty().outputPixelFormat;

    newFrame->timestamp = (uint64_t)(CMTimeGetSeconds(timestamp) * 1e9);
    newFrame->width = (uint32_t)CVPixelBufferGetWidth(imageBuffer);
    newFrame->height = (uint32_t)CVPixelBufferGetHeight(imageBuffer);
    newFrame->pixelFormat = internalFormat;
    newFrame->nativeHandle = imageBuffer;
    newFrame->sizeInBytes = (uint32_t)CVPixelBufferGetDataSize(imageBuffer);

    /// When internalFormat is an RGB color, outputFormat cannot be a YUV color format.
    assert(!((internalFormat & kPixelFormatRGBColorBit) && (outputFormat & kPixelFormatYUVColorBit)));

    if ((internalFormat & kPixelFormatYUVColorBit)) {
        uint32_t yBytesPerRow = (uint32_t)CVPixelBufferGetBytesPerRowOfPlane(imageBuffer, 0);
        uint32_t uvBytesPerRow = (uint32_t)CVPixelBufferGetBytesPerRowOfPlane(imageBuffer, 1);

        newFrame->data[0] = (uint8_t*)CVPixelBufferGetBaseAddressOfPlane(imageBuffer, 0);
        newFrame->data[1] = (uint8_t*)CVPixelBufferGetBaseAddressOfPlane(imageBuffer, 1);
        newFrame->data[2] = nullptr;

        newFrame->stride[0] = yBytesPerRow;
        newFrame->stride[1] = uvBytesPerRow;
        newFrame->stride[2] = 0;
    } else {
        newFrame->data[0] = (uint8_t*)CVPixelBufferGetBaseAddress(imageBuffer);
        newFrame->data[1] = nullptr;
        newFrame->data[2] = nullptr;
        newFrame->stride[0] = (uint32_t)CVPixelBufferGetBytesPerRow(imageBuffer);
        newFrame->stride[1] = 0;
        newFrame->stride[2] = 0;
    }

    /// iOS/macOS does not support i420, and we do not intend to support nv12 to i420 conversion here.
    bool zeroCopy = ((internalFormat & kPixelFormatYUVColorBit) && (outputFormat & kPixelFormatYUVColorBit)) ||
        (internalFormat == outputFormat && _provider->frameOrientation() == kDefaultFrameOrientation);

    if (!zeroCopy) {
        newFrame->orientation = _provider->frameOrientation();

        if (!newFrame->allocator) {
            auto&& f = _provider->getAllocatorFactory();
            newFrame->allocator = f ? f() : std::make_shared<DefaultAllocator>();
        }

        std::chrono::steady_clock::time_point startConvertTime;

        if (verboseLogEnabled()) {
            startConvertTime = std::chrono::steady_clock::now();
        }

        zeroCopy = !inplaceConvertFrame(newFrame.get(), outputFormat, (int)(newFrame->orientation != kDefaultFrameOrientation));

        CVPixelBufferUnlockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);

        if (verboseLogEnabled()) {
#ifdef DEBUG
            constexpr const char* mode = "(Debug)";
#else
            constexpr const char* mode = "(Release)";
#endif

            double durInMs = (std::chrono::steady_clock::now() - startConvertTime).count() / 1.e6;
            static double s_allCostTime = 0;
            static double s_frames = 0;

            if (s_frames > 60) {
                s_allCostTime = 0;
                s_frames = 0;
            }

            s_allCostTime += durInMs;
            ++s_frames;

            CCAP_NSLOG_V(
                @"ccap: inplaceConvertFrame requested pixel format: %s, actual pixel format: %s, flip: %d, cost time %s: (cur %g ms, avg %g ms)",
                pixelFormatToString(_provider->getFrameProperty().outputPixelFormat).data(),
                pixelFormatToString(_provider->getFrameProperty().cameraPixelFormat).data(),
                (int)(newFrame->orientation != kDefaultFrameOrientation), mode, durInMs, s_allCostTime / s_frames);
        }
    }

    if (zeroCopy) {
        newFrame->orientation = kDefaultFrameOrientation;
        CFRetain(imageBuffer);
        auto manager = std::make_shared<FakeFrame>([imageBuffer, newFrame]() mutable {
            CVPixelBufferUnlockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);
            CFRelease(imageBuffer);
            CCAP_NSLOG_V(@"ccap: recycled frame, width: %d, height: %d", (int)newFrame->width, (int)newFrame->height);

            newFrame->nativeHandle = nullptr;
            newFrame = nullptr;
        });

        auto fakeFrame = std::shared_ptr<VideoFrame>(manager, newFrame.get());
        newFrame = fakeFrame;
    }

    newFrame->frameIndex = _provider->frameIndex()++;

    if (verboseLogEnabled()) { /// Generally, camera interfaces are not called in multiple threads, and verbose logs are only for debugging,
                               /// so no lock is needed here.
        static uint64_t s_lastFrameTime;
        static std::deque<uint64_t> s_durations;

        if (s_lastFrameTime != 0) {
            auto dur = newFrame->timestamp - s_lastFrameTime;
            s_durations.emplace_back(dur);
        }

        s_lastFrameTime = newFrame->timestamp;

        /// use a window of 30 frames to calculate the fps
        if (s_durations.size() > 30) {
            s_durations.pop_front();
        }

        double fps = 0.0;

        if (!s_durations.empty()) {
            double sum = 0.0;
            for (auto& d : s_durations) {
                sum += d / 1e9f;
            }
            fps = std::round(s_durations.size() / sum * 10) / 10.0;
        }

        CCAP_NSLOG_V(@"ccap: New frame available: %ux%u, bytes %u, Data address: %p, fps: %g", newFrame->width, newFrame->height,
              newFrame->sizeInBytes, newFrame->data[0], fps);
    }

    _provider->newFrameAvailable(std::move(newFrame));
}

@end

namespace ccap {
ProviderApple::ProviderApple() {
#ifdef CCAP_MACOS
    optimizeLogIfNotSet();
#endif
    m_frameOrientation = kDefaultFrameOrientation;
}

ProviderApple::~ProviderApple() { ProviderApple::close(); }

std::vector<std::string> ProviderApple::findDeviceNames() {
    @autoreleasepool {
        NSArray<AVCaptureDevice*>* devices = findAllDeviceName();
        std::vector<std::string> names;
        if (devices.count != 0) {
            names.reserve(devices.count);
            for (AVCaptureDevice* d in devices) {
                names.emplace_back([d.localizedName UTF8String]);
            }
        }
        return names;
    }
}

bool ProviderApple::open(std::string_view deviceNameOrFilePath) {
    // Check if this looks like a file path
    if (looksLikeFilePath(deviceNameOrFilePath)) {
        return openFile(deviceNameOrFilePath);
    }
    return openCamera(deviceNameOrFilePath);
}

bool ProviderApple::openCamera(std::string_view deviceName) {
    if (m_imp != nil || m_fileReader) {
        reportError(ErrorCode::DeviceOpenFailed, "Camera or file is already opened");
        return false;
    }

    m_isFileMode = false;

    @autoreleasepool {
        m_imp = [[CameraCaptureObjc alloc] initWithProvider:this];
        if (!deviceName.empty()) {
            [m_imp setCameraName:@(deviceName.data())];
        }
        [m_imp setResolution:CGSizeMake(m_frameProp.width, m_frameProp.height)];
        return [m_imp open];
    }
}

bool ProviderApple::openFile(std::string_view filePath) {
    if (m_imp != nil || m_fileReader) {
        reportError(ErrorCode::DeviceOpenFailed, "Camera or file is already opened");
        return false;
    }

    m_isFileMode = true;
    m_fileReader = std::make_unique<FileReaderApple>(this);
    
    if (!m_fileReader->open(filePath)) {
        m_fileReader.reset();
        m_isFileMode = false;
        return false;
    }
    
    return true;
}

bool ProviderApple::isOpened() const {
    if (m_isFileMode) {
        return m_fileReader && m_fileReader->isOpened();
    }
    return m_imp && m_imp.session && m_imp.opened;
}

std::optional<DeviceInfo> ProviderApple::getDeviceInfo() const {
    std::optional<DeviceInfo> deviceInfo;
    
    if (m_isFileMode && m_fileReader) {
        // For file mode, return basic info
        deviceInfo.emplace();
        deviceInfo->deviceName = "Video File";
        deviceInfo->supportedPixelFormats.push_back(PixelFormat::BGRA32);
        deviceInfo->supportedPixelFormats.push_back(PixelFormat::NV12f);
        deviceInfo->supportedResolutions.push_back({(uint32_t)m_fileReader->getWidth(), (uint32_t)m_fileReader->getHeight()});
        return deviceInfo;
    }
    
    if (m_imp && m_imp.videoOutput) {
        @autoreleasepool {
            NSString* deviceName = [m_imp.device localizedName];
            if ([deviceName length] > 0) {
                deviceInfo.emplace();
                deviceInfo->deviceName = [deviceName UTF8String];
                NSArray* supportedFormats = [m_imp.videoOutput availableVideoCVPixelFormatTypes];
                auto& formats = deviceInfo->supportedPixelFormats;
                formats.reserve(supportedFormats.count);
                for (NSNumber* format in supportedFormats) {
                    auto info = getPixelFormatInfo((OSType)[format unsignedIntValue]);
                    if (info.format != PixelFormat::Unknown) {
                        formats.emplace_back(info.format);
                    } else {
                        CCAP_NSLOG_V(@"ccap: The OS native pixel format %@ currently not implemented", info.name);
                    }
                }

                auto allResolutions = allSupportedResolutions(m_imp.session);
                for (auto& info : allResolutions) {
                    deviceInfo->supportedResolutions.emplace_back(info.resolution);
                }
            }
        }
    }

    if (!deviceInfo) {
        reportError(ErrorCode::InitializationFailed, "getDeviceInfo called with no device opened");
    }
    return deviceInfo;
}

void ProviderApple::close() {
    if (m_fileReader) {
        m_fileReader->close();
        m_fileReader.reset();
        m_isFileMode = false;
    }
    if (m_imp) {
        [m_imp destroy];
        m_imp = nil;
    }
    ccap::resetSharedAllocator();
}

bool ProviderApple::start() {
    if (!isOpened()) {
        CCAP_NSLOG_W(@"ccap: start called with no device/file opened");
        reportError(ErrorCode::DeviceStartFailed, "Start called with no device/file opened");
        return false;
    }

    if (m_isFileMode && m_fileReader) {
        return m_fileReader->start();
    }

    @autoreleasepool {
        return [m_imp start];
    }
}

void ProviderApple::stop() {
    if (m_isFileMode && m_fileReader) {
        m_fileReader->stop();
        return;
    }
    if (m_imp) {
        @autoreleasepool {
            [m_imp stop];
        }
    }
}

bool ProviderApple::isStarted() const {
    if (m_isFileMode && m_fileReader) {
        return m_fileReader->isStarted();
    }
    return m_imp && [m_imp isRunning];
}

bool ProviderApple::setFileProperty(PropertyName prop, double value) {
    if (!m_isFileMode || !m_fileReader) {
        return false;
    }
    
    switch (prop) {
    case PropertyName::CurrentTime:
        return m_fileReader->seekToTime(value);
    case PropertyName::PlaybackSpeed:
        return m_fileReader->setPlaybackSpeed(value);
    case PropertyName::CurrentFrameIndex:
        return m_fileReader->seekToFrame(static_cast<int64_t>(value));
    default:
        return false;
    }
}

double ProviderApple::getFileProperty(PropertyName prop) const {
    if (!m_isFileMode || !m_fileReader) {
        return NAN;
    }
    
    switch (prop) {
    case PropertyName::Duration:
        return m_fileReader->getDuration();
    case PropertyName::CurrentTime:
        return m_fileReader->getCurrentTime();
    case PropertyName::PlaybackSpeed:
        return m_fileReader->getPlaybackSpeed();
    case PropertyName::FrameCount:
        return m_fileReader->getFrameCount();
    case PropertyName::CurrentFrameIndex:
        return m_fileReader->getCurrentFrameIndex();
    default:
        return NAN;
    }
}

ProviderImp* createProviderApple() { return new ProviderApple(); }

} // namespace ccap

#endif