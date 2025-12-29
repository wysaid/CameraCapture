# Memory Management in Video File Playback

## Overview

ccap uses different frame buffering strategies for **camera mode** and **file mode**:

- **Camera Mode**: Real-time streaming - drops old frames when buffer is full
- **File Mode**: Post-processing - pauses reading when buffer is full (**no frame dropping**)

## Camera vs. File Mode

### Camera Mode (Real-time Streaming)

Optimized for capturing the **latest frame** from a live source:

```cpp
ccap::Provider provider;
provider.open(0, true);  // Camera device

// If grab() is slow, old frames are dropped
// This ensures you always get recent frames
```

**Behavior**:
- ✓ Always reads latest frames
- ✓ Drops old frames automatically
- ✓ Bounded memory usage
- ✓ Low latency

### File Mode (Post-processing)

Optimized for **processing every frame** without loss:

```cpp
ccap::Provider provider;
provider.open("video.mp4", true);  // Video file

// If grab() is slow, reading PAUSES
// This ensures NO frames are dropped
```

**Behavior**:
- ✓ **Never drops frames**
- ✓ Backpressure mechanism pauses reading
- ✓ All frames can be processed
- ✓ Bounded memory usage

## Backpressure Mechanism (File Mode)

When `PlaybackSpeed=0` in file mode:

1. **Fast Reading**: Read loop fetches frames as quickly as possible
2. **Queue Monitoring**: Checks if queue is full before reading next frame
3. **Pause on Full**: When queue reaches `maxAvailableFrameSize`, reading **pauses**
4. **Resume on Consume**: When user calls `grab()`, reading resumes

```cpp
// Simplified backpressure logic in file reader
while (!shouldStop) {
    if (!provider->shouldReadMoreFrames()) {
        // Queue is full, wait for consumer
        sleep(10ms);
        continue;
    }
    
    // Read next frame
    auto frame = readNextFrame();
    provider->newFrameAvailable(frame);
}
```

### Why Backpressure?

**Problem Without Backpressure**:
```cpp
// User processing video frame by frame
for (int i = 0; i < 100; i++) {
    auto frame = provider.grab(1000);
    slowProcessing(frame);  // Takes 100ms
    
    // Without backpressure: frames 1-99 might be dropped!
    // User would miss data
}
```

**Solution With Backpressure**:
```cpp
// Same code, but now ALL frames are preserved
for (int i = 0; i < 100; i++) {
    auto frame = provider.grab(1000);
    slowProcessing(frame);  // Takes 100ms
    
    // With backpressure: reading pauses, no frames dropped
    // User gets every single frame: 0, 1, 2, ..., 99 ✓
}
```

## Frame Queue Limit

ccap automatically limits the number of buffered frames using a **bounded queue**:

```cpp
// Default limits (defined in ccap_core.h)
DEFAULT_MAX_AVAILABLE_FRAME_SIZE = 3    // Max frames waiting for grab()
DEFAULT_MAX_CACHE_FRAME_SIZE = 15       // Max frames in reuse pool
```

### How It Works

**Camera Mode**:
```cpp
// From ccap_imp.cpp - newFrameAvailable()
m_availableFrames.push(std::move(frame));
if (!m_isFileMode && m_availableFrames.size() > m_maxAvailableFrameSize) {
    m_availableFrames.pop();  // Drop oldest frame in camera mode
}
```

**File Mode**:
```cpp
// From ccap_file_reader_*.cpp - readLoop()
while (!shouldStop) {
    // Check backpressure before reading
    if (!provider->shouldReadMoreFrames()) {
        sleep(10ms);  // Wait for consumer to catch up
        continue;
    }
    
    auto frame = readNextFrame();
    provider->newFrameAvailable(frame);  // Never drops in file mode
}
```

## Memory Safety Guarantees

### File Mode (Video Playback)

#### Scenario 1: User Never Calls grab()
- ✅ **Safe**: Queue fills up to 3 frames (default), then reading pauses
- ✅ **No drops**: First 3 frames are preserved
- Memory usage: ~3 frames × frame_size

#### Scenario 2: User Calls grab() Slowly
- ✅ **Safe**: Reading pauses when queue is full
- ✅ **No drops**: Every frame is processed, just slower
- ✅ **Sequential**: Frame indices are consecutive (0, 1, 2, 3, ...)

#### Scenario 3: Long Video Processing
- ✅ **Safe**: Memory usage is constant
- ✅ **No drops**: All frames are processed
- ✅ **Reliable**: Perfect for batch processing

### Camera Mode (Live Streaming)

#### Scenario 1: User Never Calls grab()
- ✅ **Safe**: Only 3 frames (default) in memory
- ✅ **Drops old frames**: Gets latest frames when finally grabbed
- Memory usage: ~3 frames × frame_size

#### Scenario 2: User Calls grab() Slowly  
- ✅ **Safe**: Memory bounded
- ⚠️ **May skip frames**: Gets recent frames, not all frames
- Frame indices may have gaps (e.g., 0, 5, 10, ...)

#### Scenario 3: Long Camera Session
- ✅ **Safe**: Memory usage constant
- ✅ **Low latency**: Always gets fresh frames

## Configuration

### Adjust Buffer Size

You can customize the buffer size based on your needs:

```cpp
ccap::Provider provider;

// For lower latency (more aggressive dropping)
provider.setMaxAvailableFrameSize(1);

// For smoother playback (more buffering)
provider.setMaxAvailableFrameSize(5);
```

### Use Cases

**Video Post-Processing (File Mode)**:
```cpp
ccap::Provider provider;
provider.open("long_video.mp4", true);

int processedFrames = 0;
while (auto frame = provider.grab(1000)) {
    // Process every single frame - none will be dropped
    // Even if processing is slow, backpressure handles it
    analyzeFrame(frame);
    processedFrames++;
}

std::cout << "Processed all " << processedFrames << " frames\n";
```

**Real-time Camera Preview (Camera Mode)**:
```cpp
ccap::Provider provider;
provider.open(0, true);  // Camera

while (auto frame = provider.grab(1000)) {
    // Always get fresh frames
    // Old frames are dropped if preview is slow
    displayFrame(frame);
}
```

**Smooth Real-time Playback (File Mode with rate control)**:
```cpp
ccap::Provider provider;
provider.open("video.mp4", true);
provider.set(ccap::PropertyName::PlaybackSpeed, 1.0);  // Real-time

// Smooth playback at correct speed, minimal buffering
while (auto frame = provider.grab(1000)) {
    display(frame);
}

while (auto frame = provider.grab(1000)) {
    // Process each frame - many will be skipped
    analyze(frame);
}
```

**Smooth Preview with Occasional Drops**:
```cpp
provider.setMaxAvailableFrameSize(5);  // More buffering
provider.set(ccap::PropertyName::PlaybackSpeed, 1.0);  // Real-time speed
```

## Frame Rate Control

### PlaybackSpeed Values

| Value | Behavior | Memory Impact |
|-------|----------|---------------|
| 0.0 (default) | No rate control, read as fast as possible | Bounded by queue limit |
| 1.0 | Normal speed (match video fps) | Lower pressure, smoother |
| 2.0 | 2x speed | Medium pressure |
| 0.5 | Half speed | Very low pressure |

### Example: Memory-Safe Fast Processing

```cpp
ccap::Provider provider;

// Default PlaybackSpeed=0 means fastest reading
provider.open("large_video.mp4", true);

int processedFrames = 0;
while (auto frame = provider.grab(1000)) {
    // Even if this processing is slow,
    // memory won't explode AND no frames are dropped
    slowProcessing(frame);
    processedFrames++;
}

std::cout << "Processed ALL " << processedFrames << " frames\n";
```

### Resource Usage

```text
Memory = maxAvailableFrameSize × frame_size + frame_pool_overhead

For typical HD video (1920×1080, RGBA):
- Frame size: ~8MB
- Default config: 3 frames × 8MB = 24MB base
- Frame pool: ~15 frames × ~100 bytes = minimal
- Total: ~25-30MB (constant, regardless of video length!)
```

### CPU/Disk I/O

- **PlaybackSpeed=0**: Maximum disk/CPU usage for decoding
- **PlaybackSpeed>0**: Throttled by target frame rate
- Video codec and format significantly impact decoding speed

## Comparison with OpenCV

ccap's behavior with `PlaybackSpeed=0` matches OpenCV's `cv::VideoCapture`:

```cpp
// OpenCV
cv::VideoCapture cap("video.mp4");
while (true) {
    cv::Mat frame;
    if (!cap.read(frame)) break;  // Immediate, no rate control
    // Process frame
}

// ccap equivalent
ccap::Provider provider;
provider.open("video.mp4", true);  // PlaybackSpeed=0 by default
while (auto frame = provider.grab(1000)) {
    // Process frame - same behavior!
}
```

## Best Practices

### ✅ DO

- **Use default settings** for most use cases (already optimized)
- **Set PlaybackSpeed=1.0** if you want smooth real-time playback with rate control
- **Check frame->frameIndex** in camera mode to detect skipped frames
- **In file mode**: frameIndex will always be consecutive (0, 1, 2, ...) - no drops
- **Use callback mode** for real-time processing without explicit grab() calls

### ❌ DON'T

- Don't worry about memory explosion - bounded queue handles it
- Don't try to cache all frames in application code - use file mode's backpressure
- Don't assume frame dropping in file mode - it preserves all frames
- Don't expect consecutive indices in camera mode - frame skipping is intentional

## Monitoring and Debugging

### Check Buffer Status

```cpp
// Get current frame to see how far playback has progressed
auto frame = provider.grab(0);  // Non-blocking
if (frame) {
    std::cout << "Current frame index: " << frame->frameIndex << std::endl;
    
    double currentTime = provider.get(ccap::PropertyName::CurrentTime);
    std::cout << "Video time: " << currentTime << "s" << std::endl;
}
```

### Detect Frame Drops (Camera Mode)

```cpp
// Only relevant for camera mode - file mode never drops frames
uint64_t lastIndex = 0;
bool isFileMode = provider.isFileMode();

while (auto frame = provider.grab(1000)) {
    if (!isFileMode && lastIndex > 0 && frame->frameIndex - lastIndex > 1) {
        std::cout << "Camera dropped " << (frame->frameIndex - lastIndex - 1) 
                  << " frames (normal behavior)" << std::endl;
    }
    lastIndex = frame->frameIndex;
}
```

## Summary

ccap's memory management is **safe and mode-aware**:

### Camera Mode
1. ✅ Bounded memory (default: ~25-30MB for HD video)
2. ✅ Drops old frames to keep recent data
3. ✅ Low latency for real-time applications
4. ⚠️ May skip frames if grab() is slow

### File Mode  
1. ✅ Bounded memory (same as camera mode)
2. ✅ **Never drops frames** - uses backpressure
3. ✅ All frames can be processed
4. ✅ Works with videos of any length
5. ✅ Compatible with OpenCV's cv::VideoCapture behavior

**You can safely process long videos with `PlaybackSpeed=0` without worrying about memory exhaustion or frame loss.**
