/**
 * @file ccap_file_reader_apple.mm
 * @author wysaid (this@wysaid.org)
 * @brief Video file reader implementation for macOS/iOS using AVFoundation
 * @date 2025-12
 *
 */

#if __APPLE__

#include "ccap_file_reader_apple.h"

#include "ccap_imp_apple.h"
#include "ccap_convert.h"
#include "ccap_convert_frame.h"

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>

#include <chrono>
#include <cmath>
#include <thread>

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

@interface CcapFileReaderObjc : NSObject {
    ProviderApple* _provider;
    dispatch_queue_t _readQueue;
    std::atomic<bool> _isReading;
    std::atomic<bool> _shouldStop;
    std::chrono::steady_clock::time_point _startTime;
    double _playbackSpeed;
    int64_t _currentFrameIndex;
    double _currentTime;
}

@property (nonatomic, strong) AVAsset* asset;
@property (nonatomic, strong) AVAssetReader* assetReader;
@property (nonatomic, strong) AVAssetReaderTrackOutput* trackOutput;
@property (nonatomic, strong) NSURL* fileURL;
@property (nonatomic) CMTime frameDuration;
@property (nonatomic) double videoDuration;
@property (nonatomic) double frameRate;
@property (nonatomic) int64_t totalFrameCount;
@property (nonatomic) int videoWidth;
@property (nonatomic) int videoHeight;
@property (nonatomic) BOOL opened;
@property (nonatomic) BOOL started;

- (instancetype)initWithProvider:(ProviderApple*)provider;
- (BOOL)openFile:(NSString*)filePath;
- (void)close;
- (BOOL)start;
- (void)stop;
- (double)getCurrentTime;
- (BOOL)seekToTime:(double)timeInSeconds;
- (double)getCurrentFrameIndex;
- (BOOL)seekToFrame:(int64_t)frameIndex;
- (double)getPlaybackSpeed;
- (BOOL)setPlaybackSpeed:(double)speed;

@end

@implementation CcapFileReaderObjc

- (instancetype)initWithProvider:(ProviderApple*)provider {
    self = [super init];
    if (self) {
        _provider = provider;
        _isReading = false;
        _shouldStop = false;
        _playbackSpeed = 0.0;
        _currentFrameIndex = 0;
        _currentTime = 0.0;
        _opened = NO;
        _started = NO;
    }
    return self;
}

- (void)dealloc {
    [self close];
}

- (BOOL)openFile:(NSString*)filePath {
    if (_opened) {
        [self close];
    }
    
    _fileURL = [NSURL fileURLWithPath:filePath];
    if (![[NSFileManager defaultManager] fileExistsAtPath:filePath]) {
        reportError(ErrorCode::FileOpenFailed, "File does not exist: " + std::string([filePath UTF8String]));
        return NO;
    }
    
    _asset = [AVAsset assetWithURL:_fileURL];
    if (!_asset) {
        reportError(ErrorCode::FileOpenFailed, "Failed to create AVAsset");
        return NO;
    }
    
    // Get video track
    NSArray<AVAssetTrack*>* videoTracks = [_asset tracksWithMediaType:AVMediaTypeVideo];
    if (videoTracks.count == 0) {
        reportError(ErrorCode::UnsupportedVideoFormat, "No video track found");
        return NO;
    }
    
    AVAssetTrack* videoTrack = videoTracks.firstObject;
    
    // Get video properties
    CGSize naturalSize = videoTrack.naturalSize;
    _videoWidth = (int)naturalSize.width;
    _videoHeight = (int)naturalSize.height;
    
    // Apply transform if needed (for rotated videos)
    CGAffineTransform transform = videoTrack.preferredTransform;
    if (transform.b == 1.0 && transform.c == -1.0) {
        // 90 degrees rotation
        std::swap(_videoWidth, _videoHeight);
    } else if (transform.b == -1.0 && transform.c == 1.0) {
        // -90 degrees rotation
        std::swap(_videoWidth, _videoHeight);
    }
    
    _frameRate = videoTrack.nominalFrameRate;
    if (_frameRate <= 0) {
        _frameRate = 30.0; // Default to 30 fps
    }
    
    _frameDuration = CMTimeMake(1, (int32_t)_frameRate);
    _videoDuration = CMTimeGetSeconds(_asset.duration);
    _totalFrameCount = (int64_t)(_videoDuration * _frameRate);
    
    // Update provider frame properties
    if (_provider) {
        auto& prop = _provider->getFrameProperty();
        prop.width = _videoWidth;
        prop.height = _videoHeight;
        prop.fps = _frameRate;
    }
    
    CCAP_NSLOG_I(@"ccap: Opened video file: %@ (%dx%d, %.2f fps, %.2f seconds, %lld frames)",
                 filePath, _videoWidth, _videoHeight, _frameRate, _videoDuration, _totalFrameCount);
    
    _opened = YES;
    _currentFrameIndex = 0;
    _currentTime = 0.0;
    
    return YES;
}

- (BOOL)setupAssetReader {
    if (_assetReader) {
        [_assetReader cancelReading];
        _assetReader = nil;
        _trackOutput = nil;
    }
    
    NSError* error = nil;
    _assetReader = [[AVAssetReader alloc] initWithAsset:_asset error:&error];
    if (error) {
        reportError(ErrorCode::FileOpenFailed, "Failed to create AVAssetReader: " + std::string([error.localizedDescription UTF8String]));
        return NO;
    }
    
    NSArray<AVAssetTrack*>* videoTracks = [_asset tracksWithMediaType:AVMediaTypeVideo];
    AVAssetTrack* videoTrack = videoTracks.firstObject;
    
    // Configure output settings - prefer NV12/BGRA based on provider settings
    OSType pixelFormat = kCVPixelFormatType_32BGRA;
    if (_provider) {
        auto& prop = _provider->getFrameProperty();
        if (prop.outputPixelFormat & kPixelFormatYUVColorBit) {
            pixelFormat = kCVPixelFormatType_420YpCbCr8BiPlanarFullRange;
        }
    }
    
    NSDictionary* outputSettings = @{
        (id)kCVPixelBufferPixelFormatTypeKey: @(pixelFormat)
    };
    
    _trackOutput = [[AVAssetReaderTrackOutput alloc] initWithTrack:videoTrack outputSettings:outputSettings];
    _trackOutput.alwaysCopiesSampleData = NO;
    
    if ([_assetReader canAddOutput:_trackOutput]) {
        [_assetReader addOutput:_trackOutput];
    } else {
        reportError(ErrorCode::UnsupportedVideoFormat, "Cannot add track output to asset reader");
        return NO;
    }
    
    // Set time range starting from current position
    CMTime startTime = CMTimeMakeWithSeconds(_currentTime, 600);
    CMTime duration = CMTimeSubtract(_asset.duration, startTime);
    _assetReader.timeRange = CMTimeRangeMake(startTime, duration);
    
    if (![_assetReader startReading]) {
        reportError(ErrorCode::FileOpenFailed, "Failed to start reading");
        return NO;
    }
    
    return YES;
}

- (void)close {
    [self stop];
    
    if (_assetReader) {
        [_assetReader cancelReading];
        _assetReader = nil;
    }
    _trackOutput = nil;
    _asset = nil;
    _fileURL = nil;
    _opened = NO;
    _currentFrameIndex = 0;
    _currentTime = 0.0;
    
    // Clean up queue only when fully closing (ARC will handle release)
    _readQueue = nil;
}

- (BOOL)start {
    if (!_opened || _started) {
        return _started;
    }
    
    if (![self setupAssetReader]) {
        return NO;
    }
    
    _shouldStop = false;
    _started = YES;
    _startTime = std::chrono::steady_clock::now();
    
    // Reuse existing queue or create new one
    if (!_readQueue) {
        _readQueue = dispatch_queue_create("ccap.file_reader", DISPATCH_QUEUE_SERIAL);
    }
    
    dispatch_async(_readQueue, ^{
        [self readLoop];
    });
    
    return YES;
}

- (void)stop {
    _shouldStop = true;
    _started = NO;
    
    // Wait for reading to finish
    int waitCount = 0;
    while (_isReading && waitCount++ < 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    if (_assetReader) {
        [_assetReader cancelReading];
    }
    
    // Queue will be reused on next start(), cleaned up only in close()
}

- (void)readLoop {
    _isReading = true;
    
    auto lastFrameTime = std::chrono::steady_clock::now();
    double targetFrameInterval = (_playbackSpeed > 0.0) ? (1.0 / (_frameRate * _playbackSpeed)) : 0.0;
    
    while (!_shouldStop && _assetReader.status == AVAssetReaderStatusReading) {
        @autoreleasepool {
            // Backpressure: if queue is full, wait for consumer to catch up
            // This prevents dropping frames in file mode
            if (!_provider->shouldReadMoreFrames()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            CMSampleBufferRef sampleBuffer = [_trackOutput copyNextSampleBuffer];
            
            if (!sampleBuffer) {
                if (_assetReader.status == AVAssetReaderStatusCompleted) {
                    CCAP_NSLOG_I(@"ccap: Video playback completed");
                }
                break;
            }
            
            // Frame rate control (only when playback speed > 0)
            if (targetFrameInterval > 0.0) {
                auto now = std::chrono::steady_clock::now();
                double elapsedSeconds = std::chrono::duration<double>(now - lastFrameTime).count();
                double sleepTime = targetFrameInterval - elapsedSeconds;
                
                if (sleepTime > 0.001) {
                    std::this_thread::sleep_for(std::chrono::duration<double>(sleepTime));
                }
                
                lastFrameTime = std::chrono::steady_clock::now();
            }
            
            // Process the frame
            [self processFrame:sampleBuffer];
            
            CFRelease(sampleBuffer);
            
            _currentFrameIndex++;
            _currentTime = (double)_currentFrameIndex / _frameRate;
            
            // Update target interval in case playback speed changed
            targetFrameInterval = (_playbackSpeed > 0.0) ? (1.0 / (_frameRate * _playbackSpeed)) : 0.0;
        }
    }
    
    _isReading = false;
    _started = NO;
    
    // Notify waiting grab() calls that playback has finished
    if (_provider) {
        _provider->notifyGrabWaiters();
    }
}

- (void)processFrame:(CMSampleBufferRef)sampleBuffer {
    if (!_provider) return;
    
    CVImageBufferRef imageBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
    if (!imageBuffer) return;
    
    CVPixelBufferLockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);
    
    auto newFrame = _provider->getFreeFrame();
    
    CMTime pts = CMSampleBufferGetPresentationTimeStamp(sampleBuffer);
    newFrame->timestamp = (uint64_t)(CMTimeGetSeconds(pts) * 1e9);
    newFrame->width = (uint32_t)CVPixelBufferGetWidth(imageBuffer);
    newFrame->height = (uint32_t)CVPixelBufferGetHeight(imageBuffer);
    newFrame->nativeHandle = imageBuffer;
    newFrame->sizeInBytes = (uint32_t)CVPixelBufferGetDataSize(imageBuffer);
    
    OSType pixelFormatType = CVPixelBufferGetPixelFormatType(imageBuffer);
    
    bool isYUV = (pixelFormatType == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange ||
                  pixelFormatType == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange);
    
    if (isYUV) {
        newFrame->pixelFormat = (pixelFormatType == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange) 
                                ? PixelFormat::NV12f : PixelFormat::NV12;
        newFrame->data[0] = (uint8_t*)CVPixelBufferGetBaseAddressOfPlane(imageBuffer, 0);
        newFrame->data[1] = (uint8_t*)CVPixelBufferGetBaseAddressOfPlane(imageBuffer, 1);
        newFrame->data[2] = nullptr;
        newFrame->stride[0] = (uint32_t)CVPixelBufferGetBytesPerRowOfPlane(imageBuffer, 0);
        newFrame->stride[1] = (uint32_t)CVPixelBufferGetBytesPerRowOfPlane(imageBuffer, 1);
        newFrame->stride[2] = 0;
    } else {
        newFrame->pixelFormat = PixelFormat::BGRA32;
        newFrame->data[0] = (uint8_t*)CVPixelBufferGetBaseAddress(imageBuffer);
        newFrame->data[1] = nullptr;
        newFrame->data[2] = nullptr;
        newFrame->stride[0] = (uint32_t)CVPixelBufferGetBytesPerRow(imageBuffer);
        newFrame->stride[1] = 0;
        newFrame->stride[2] = 0;
    }
    
    // Note: AVFoundation (AVAssetReader) returns video frames in TopToBottom order
    // for all formats (NV12, BGRA32), similar to Media Foundation on Windows.
    constexpr FrameOrientation inputOrientation = FrameOrientation::TopToBottom;
    
    // Check if conversion or flip is needed
    auto& prop = _provider->getFrameProperty();
    bool isOutputYUV = (newFrame->pixelFormat & kPixelFormatYUVColorBit) != 0;
    FrameOrientation targetOrientation = isOutputYUV ? FrameOrientation::TopToBottom : _provider->frameOrientation();
    bool shouldFlip = !isOutputYUV && (inputOrientation != targetOrientation);
    bool shouldConvert = newFrame->pixelFormat != prop.outputPixelFormat;
    
    newFrame->orientation = targetOrientation;
    
    bool zeroCopy = !shouldConvert && !shouldFlip;
    
    if (!zeroCopy) {
        if (!newFrame->allocator) {
            auto&& f = _provider->getAllocatorFactory();
            newFrame->allocator = f ? f() : std::make_shared<DefaultAllocator>();
        }
        
        zeroCopy = !inplaceConvertFrame(newFrame.get(), prop.outputPixelFormat, shouldFlip);
        CVPixelBufferUnlockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);
    }
    
    if (zeroCopy) {
        CFRetain(imageBuffer);
        auto manager = std::make_shared<FakeFrame>([imageBuffer, newFrame]() mutable {
            CVPixelBufferUnlockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);
            CFRelease(imageBuffer);
            newFrame->nativeHandle = nullptr;
            newFrame = nullptr;
        });
        
        auto fakeFrame = std::shared_ptr<VideoFrame>(manager, newFrame.get());
        newFrame = fakeFrame;
    }
    
    newFrame->frameIndex = _currentFrameIndex;
    
    _provider->newFrameAvailable(std::move(newFrame));
}

- (double)getCurrentTime {
    return _currentTime;
}

- (BOOL)seekToTime:(double)timeInSeconds {
    if (!_opened) return NO;
    
    timeInSeconds = std::clamp(timeInSeconds, 0.0, _videoDuration);
    _currentTime = timeInSeconds;
    _currentFrameIndex = (int64_t)(timeInSeconds * _frameRate);
    
    // If playing, restart from new position
    if (_started) {
        [self stop];
        return [self start];
    }
    
    return YES;
}

- (double)getCurrentFrameIndex {
    return (double)_currentFrameIndex;
}

- (BOOL)seekToFrame:(int64_t)frameIndex {
    if (!_opened) return NO;
    
    frameIndex = std::clamp(frameIndex, (int64_t)0, _totalFrameCount);
    _currentFrameIndex = frameIndex;
    _currentTime = (double)frameIndex / _frameRate;
    
    // If playing, restart from new position
    if (_started) {
        [self stop];
        return [self start];
    }
    
    return YES;
}

- (double)getPlaybackSpeed {
    return _playbackSpeed;
}

- (BOOL)setPlaybackSpeed:(double)speed {
    if (speed < 0) return NO;
    _playbackSpeed = speed;
    return YES;
}

@end

namespace ccap {

FileReaderApple::FileReaderApple(ProviderApple* provider) :
    m_provider(provider) {
    m_objc = [[CcapFileReaderObjc alloc] initWithProvider:provider];
}

FileReaderApple::~FileReaderApple() {
    close();
    m_objc = nil;
}

bool FileReaderApple::open(std::string_view filePath) {
    // Convert string_view to NSString safely (string_view may not be null-terminated)
    NSString* path = [[NSString alloc] initWithBytes:filePath.data()
                                              length:filePath.size()
                                            encoding:NSUTF8StringEncoding];
    return [m_objc openFile:path];
}

void FileReaderApple::close() {
    [m_objc close];
}

bool FileReaderApple::isOpened() const {
    return m_objc.opened;
}

bool FileReaderApple::start() {
    return [m_objc start];
}

void FileReaderApple::stop() {
    [m_objc stop];
}

bool FileReaderApple::isStarted() const {
    return m_objc.started;
}

double FileReaderApple::getDuration() const {
    return m_objc.videoDuration;
}

double FileReaderApple::getFrameCount() const {
    return (double)m_objc.totalFrameCount;
}

double FileReaderApple::getCurrentTime() const {
    return [m_objc getCurrentTime];
}

bool FileReaderApple::seekToTime(double timeInSeconds) {
    return [m_objc seekToTime:timeInSeconds];
}

double FileReaderApple::getCurrentFrameIndex() const {
    return [m_objc getCurrentFrameIndex];
}

bool FileReaderApple::seekToFrame(int64_t frameIndex) {
    return [m_objc seekToFrame:frameIndex];
}

double FileReaderApple::getPlaybackSpeed() const {
    return [m_objc getPlaybackSpeed];
}

bool FileReaderApple::setPlaybackSpeed(double speed) {
    return [m_objc setPlaybackSpeed:speed];
}

double FileReaderApple::getFrameRate() const {
    return m_objc.frameRate;
}

int FileReaderApple::getWidth() const {
    return m_objc.videoWidth;
}

int FileReaderApple::getHeight() const {
    return m_objc.videoHeight;
}

} // namespace ccap

#endif // __APPLE__
