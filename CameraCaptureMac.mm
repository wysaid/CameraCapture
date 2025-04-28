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

@interface CameraCaptureObjc : NSObject<AVCaptureVideoDataOutputSampleBufferDelegate>
{
    ccap::ProviderMac* _provider;

    /// for verbose log
    uint64_t _frameCounter;
    uint64_t _lastFrameTime;
    double _duration;
    double _fps;
}

@property (nonatomic, strong) AVCaptureSession* session;
@property (nonatomic, strong) AVCaptureDevice* device;
@property (nonatomic, strong) NSString* cameraName;
@property (nonatomic, strong) AVCaptureDeviceInput* videoInput;
@property (nonatomic, strong) AVCaptureVideoDataOutput* videoOutput;
@property (nonatomic, strong) dispatch_queue_t captureQueue;
@property (nonatomic) CGSize resolution;

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
        AVCaptureSessionPreset preset = AVCaptureSessionPresetHigh;

        if (_resolution.width > 0 && _resolution.height > 0)
        {
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
                    NSLog(@"CameraCaptureObjc init - session preset not supported, using AVCaptureSessionPresetHigh");
                }
                preset = AVCaptureSessionPresetHigh;
            }

            [_session setSessionPreset:preset];

            if (ccap::infoLogEnabled())
            {
                NSLog(@"Expected camera resolution: (%gx%g), actual matched camera resolution: (%gx%g)",
                      inputSize.width, inputSize.height, _resolution.width, _resolution.height);
            }
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
            if (ccap::errorLogEnabled())
                NSLog(@"No video device found");
            return nil;
        }

        NSError* error = nil;
        [_device lockForConfiguration:&error];
        if ([_device isFocusModeSupported:AVCaptureFocusModeContinuousAutoFocus])
        {
            [_device setFocusMode:AVCaptureFocusModeContinuousAutoFocus];
            if (ccap::verboseLogEnabled())
            {
                NSLog(@"Set focus mode to continuous auto focus");
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
                    NSLog(@"Open camera failed: %@", error);
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
                NSLog(@"Add input to session");
            }
        }

        // Create output device
        _videoOutput = [[AVCaptureVideoDataOutput alloc] init];
        [_videoOutput setAlwaysDiscardsLateVideoFrames:YES]; // better performance

        _videoOutput.videoSettings = @{ (id)kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_32BGRA) };

        // Set output queue
        _captureQueue = dispatch_queue_create("ccap.queue", DISPATCH_QUEUE_SERIAL);
        [_videoOutput setSampleBufferDelegate:self queue:_captureQueue];

        // Add output device to session
        if ([_session canAddOutput:_videoOutput])
        {
            [_session addOutput:_videoOutput];
            if (ccap::verboseLogEnabled())
            {
                NSLog(@"Add output to session");
            }
        }

        [_session commitConfiguration];
        [self flushResolution];
    }
    return self;
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
                    if (ccap::warningLogEnabled())
                    {
                        NSLog(@"Actual camera resolution: %dx%d", dimensions.width, dimensions.height);
                    }

                    _resolution.width = dimensions.width;
                    _resolution.height = dimensions.height;
                }
            }
        }
    }
}

- (BOOL)start
{
    if (_session && ![_session isRunning])
    {
        if (ccap::verboseLogEnabled())
        {
            NSLog(@"CameraCaptureObjc start");
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
            NSLog(@"CameraCaptureObjc stop");
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
            NSLog(@"CameraCaptureObjc destroy");
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
        NSLog(@"CameraCaptureObjc dealloc - begin");
    }

    [self destroy];

    if (ccap::verboseLogEnabled())
    {
        NSLog(@"CameraCaptureObjc dealloc - end");
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
            NSLog(@"CameraCaptureObjc captureOutput - provider is nil");
        return;
    }

    // 获取图像缓冲区
    CVImageBufferRef imageBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);

    // 锁定图像缓冲区以访问其内容
    CVPixelBufferLockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);

    // 获取分辨率和数据地址
    size_t width = CVPixelBufferGetWidth(imageBuffer);
    size_t height = CVPixelBufferGetHeight(imageBuffer);
    size_t bytesPerRow = CVPixelBufferGetBytesPerRow(imageBuffer);
    size_t bytes = CVPixelBufferGetDataSize(imageBuffer);
    void* baseAddress = CVPixelBufferGetBaseAddress(imageBuffer);

    auto newFrame = _provider->getFreeFrame();
    CMTime timestamp = CMSampleBufferGetPresentationTimeStamp(sampleBuffer);
    newFrame->timestamp = (uint64_t)(CMTimeGetSeconds(timestamp) * 1e9);
    newFrame->width = width;
    newFrame->height = height;
    newFrame->pixelFormat = ccap::PixelFormat::BGRA8888;

    if (newFrame->sizeInBytes != bytes)
    {
        if (!newFrame->allocator)
            newFrame->allocator = _provider->allocator();
        newFrame->allocator->resize(bytes);
        newFrame->sizeInBytes = bytes;
    }

    newFrame->data[0] = newFrame->allocator->data();
    newFrame->data[1] = nullptr;
    newFrame->data[2] = nullptr;
    newFrame->stride[0] = bytesPerRow;
    newFrame->stride[1] = 0;
    newFrame->stride[2] = 0;

    memcpy(newFrame->data[0], baseAddress, bytes);

    CVPixelBufferUnlockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);

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
            constexpr double alpha = 0.4; // Smoothing factor, smaller value means smoother
            _fps = alpha * newFps + (1.0 - alpha) * _fps;
            _frameCounter = 0;
            _duration = 0;
        }

        double roundedFps = std::round(_fps * 10.0) / 10.0;
        NSLog(@"New frame available: %lux%lu, bytes %lu, Data address: %p, fps: %g", width, height, bytesPerRow * height, baseAddress, roundedFps);
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

bool ProviderMac::open(std::string_view deviceName)
{
    if (m_imp != nil)
    {
        if (ccap::errorLogEnabled())
        {
            NSLog(@"Camera is already opened");
        }
        return false;
    }

    m_imp = [[CameraCaptureObjc alloc] initWithProvider:this captureSize:CGSizeMake(m_frameProp.width, m_frameProp.height)];
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
    return m_imp && [m_imp start];
}

void ProviderMac::stop()
{
    if (m_imp)
    {
        [m_imp stop];
    }
}

bool ProviderMac::isStarted() const
{
    return m_imp && [m_imp isRunning];
}

} // namespace ccap

#endif