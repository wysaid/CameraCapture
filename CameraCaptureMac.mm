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
#import <Foundation/Foundation.h>

namespace ccap
{
extern LogLevel g_logLevel;
}

@interface CameraCaptureObjc : NSObject<AVCaptureVideoDataOutputSampleBufferDelegate>

@property (nonatomic, strong) AVCaptureSession* session;
@property (nonatomic, strong) AVCaptureDevice* device;
@property (nonatomic, strong) NSString* cameraName;
@property (nonatomic, strong) AVCaptureDeviceInput* videoInput;
@property (nonatomic, strong) AVCaptureVideoDataOutput* videoOutput;
@property (nonatomic, strong) dispatch_queue_t captureQueue;
@property (nonatomic) CGSize resolution;

- (instancetype)init;
- (BOOL)start;
- (void)stop;

@end

@implementation CameraCaptureObjc

- (instancetype)init
{
    self = [super init];
    if (self)
    {
        _session = [[AVCaptureSession alloc] init];
        [_session beginConfiguration];

        if (_resolution.width > 0 && _resolution.height > 0)
        {
            NSArray* resolutions = @[
                @[ @320, @240 ],
                @[ @352, @288 ],
                @[ @640, @480 ],
                @[ @960, @540 ],
                @[ @1280, @720 ],
                @[ @1920, @1080 ],
                @[ @3840, @2160 ],
            ];

            CGSize inputSize = _resolution;

            for (NSArray* res in resolutions)
            {
                if ([res[0] intValue] >= _resolution.width && [res[1] intValue] >= _resolution.height)
                {
                    _resolution.width = [res[0] intValue];
                    _resolution.height = [res[1] intValue];
                    break;
                }
            }

            if (ccap::g_logLevel & ccap::LogLevel::Info)
                NSLog(@"Camera resolution: %gx%g", _resolution.width, _resolution.height);
        }

        if (_cameraName != nil && _cameraName.length > 0)
        {
            NSArray* devices = [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];
            for (AVCaptureDevice* d in devices)
            {
                if ([d.localizedName isEqualToString:_cameraName])
                {
                    _device = d;
                    break;
                }
            }
        }

        if (!_device)
        {
            _device = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];
        }

        if (![_device hasMediaType:AVMediaTypeVideo])
        {
            if (ccap::g_logLevel & ccap::LogLevel::Error)
                NSLog(@"No video device found");
            return nil;
        }

        NSError* error = nil;
        [_device lockForConfiguration:&error];
        if ([_device isFocusModeSupported:AVCaptureFocusModeContinuousAutoFocus])
            [_device setFocusMode:AVCaptureFocusModeContinuousAutoFocus];
        [_device unlockForConfiguration];

        // Create input device
        _videoInput = [[AVCaptureDeviceInput alloc] initWithDevice:_device error:&error];
        if (error)
        {
            [AVCaptureDeviceInput deviceInputWithDevice:self.device error:&error];
            if (error)
            {
                if (ccap::g_logLevel & ccap::LogLevel::Error)
                {
                    NSLog(@"Open camera failed: %@", error);
                }
                return nil;
            }
        }

        // Add input device to session
        if ([_session canAddInput:_videoInput])
        {
            [_session addInput:_videoInput];
        }

        // Create output device
        _videoOutput = [[AVCaptureVideoDataOutput alloc] init];
        [_videoOutput setAlwaysDiscardsLateVideoFrames:YES]; // better performance

        _videoOutput.videoSettings = @{ (id)kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_24BGR) };

        // Set output queue
        _captureQueue = dispatch_queue_create("ccap.queue", DISPATCH_QUEUE_SERIAL);
        [_videoOutput setSampleBufferDelegate:self queue:_captureQueue];

        // Add output device to session
        if ([_session canAddOutput:_videoOutput])
        {
            [_session addOutput:_videoOutput];
        }

        [_session commitConfiguration];

        NSDictionary* videoSettings = [_videoOutput videoSettings];
        _resolution.width = [[videoSettings objectForKey:@"Width"] floatValue];
        _resolution.height = [[videoSettings objectForKey:@"Height"] floatValue];
    }
    return self;
}

// 处理每一帧的数据
- (void)captureOutput:(AVCaptureOutput*)output
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
           fromConnection:(AVCaptureConnection*)connection
{
    // 获取图像缓冲区
    CVImageBufferRef imageBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);

    // 锁定图像缓冲区以访问其内容
    CVPixelBufferLockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);

    // 获取分辨率和数据地址
    size_t width = CVPixelBufferGetWidth(imageBuffer);
    size_t height = CVPixelBufferGetHeight(imageBuffer);
    void* baseAddress = CVPixelBufferGetBaseAddress(imageBuffer);

    // 打印帧信息

    if (ccap::g_logLevel & ccap::LogLevel::Verbose)
    {
        NSLog(@"Frame resolution: %lux%lu, Data address: %p", width, height, baseAddress);
    }

    // 解锁图像缓冲区
    CVPixelBufferUnlockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);
}

- (BOOL)start
{
    return NO;
}

- (void)stop
{
    // Implementation for stopping the camera capture
}

@end

namespace ccap
{
ProviderMac::~ProviderMac() = default;

bool ProviderMac::open(std::string_view deviceName)
{
    // Implementation for opening the camera on macOS
    return true;
}

bool ProviderMac::isOpened() const
{
    // Implementation for checking if the camera is opened on macOS
    return true;
}

void ProviderMac::close()
{
    // Implementation for closing the camera on macOS
}

bool ProviderMac::start()
{
    // Implementation for starting the camera capture on macOS
    return false;
}

void ProviderMac::stop()
{
    // Implementation for stopping the camera capture on macOS
}

bool ProviderMac::isStarted() const
{
    // Implementation for checking if the camera capture is started on macOS
    return false;
}

} // namespace ccap

#endif