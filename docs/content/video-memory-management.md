# Memory Management & Video Playback

Understanding ccap's memory management helps you choose the right settings for your application.

## Quick Reference

| Mode | Frame Dropping | Memory Usage | Use Case |
|------|---------------|--------------|----------|
| **Camera Mode** | ✅ Yes (keeps latest) | Bounded (~25-30MB) | Live streaming, real-time preview |
| **File Mode** | ❌ No (backpressure) | Bounded (~25-30MB) | Video processing, frame analysis |

## Core Concepts

### Camera vs File Mode

ccap automatically detects whether you're working with a live camera or a video file and applies different strategies:

**Camera Mode** - Optimized for real-time:
```cpp
ccap::Provider provider;
provider.open(0, true);  // Camera device

// Latest frames are prioritized
// Old frames are dropped if processing is slow
```

**File Mode** - Optimized for completeness:
```cpp
ccap::Provider provider;
provider.open("video.mp4", true);  // Video file

// ALL frames are preserved
// Reading pauses when buffer is full (backpressure)
```

### Backpressure Mechanism (File Mode)

When processing video files with `PlaybackSpeed=0` (default):

1. **Fast Reading**: Decoder reads frames as quickly as possible
2. **Buffer Monitoring**: Checks if frame queue is full
3. **Automatic Pause**: Reading pauses when buffer reaches limit
4. **Resume on Consume**: Resumes when `grab()` is called

**Why This Matters:**

```cpp
// Without backpressure:
for (int i = 0; i < 100; i++) {
    auto frame = provider.grab(1000);
    slowProcessing(frame);  // 100ms each
    // ❌ Risk: frames 1-99 might be dropped
}

// With backpressure (ccap's approach):
for (int i = 0; i < 100; i++) {
    auto frame = provider.grab(1000);
    slowProcessing(frame);  // 100ms each
    // ✅ All frames preserved: 0, 1, 2, ..., 99
}
```

### Memory Limits

Default configuration (from `ccap_core.h`):
- `maxAvailableFrameSize = 3` - Max frames waiting for `grab()`
- `maxCacheFrameSize = 15` - Max frames in reuse pool

**Memory Footprint:**
```
HD Video (1920×1080, RGBA):
├── Frame size: ~8MB
├── Buffer: 3 frames × 8MB = 24MB
├── Frame pool overhead: minimal
└── Total: ~25-30MB (constant regardless of video length!)
```

## Playback Speed Control

| Value | Behavior | Memory Pressure |
|-------|----------|----------------|
| `0.0` (default) | Read as fast as possible | Bounded by queue |
| `1.0` | Match video's frame rate | Lower, smoother |
| `2.0` | Double speed | Medium |
| `0.5` | Half speed | Very low |

### Example: Processing Long Videos

```cpp
ccap::Provider provider;
provider.open("long_video.mp4", true);  // PlaybackSpeed=0 by default

int processedFrames = 0;
while (auto frame = provider.grab(1000)) {
    // Process every frame
    // Even if slow, backpressure prevents:
    // - Frame dropping ✅
    // - Memory explosion ✅
    analyzeFrame(frame);
    processedFrames++;
}

std::cout << "Processed ALL " << processedFrames << " frames\n";
```

## Configuration

### Adjust Buffer Size

```cpp
ccap::Provider provider;

// Lower latency (more aggressive dropping in camera mode)
provider.setMaxAvailableFrameSize(1);

// Smoother playback (more buffering)
provider.setMaxAvailableFrameSize(5);
```

### Control Playback Rate

```cpp
// Fast processing (default)
provider.open("video.mp4", true);
// PlaybackSpeed=0, reads as fast as possible

// Real-time playback
provider.set(ccap::PropertyName::PlaybackSpeed, 1.0);
// Matches video's original frame rate

// Slow motion analysis
provider.set(ccap::PropertyName::PlaybackSpeed, 0.5);
// Half speed playback
```

## Common Scenarios

### Video Frame-by-Frame Analysis

```cpp
ccap::Provider provider;
provider.open("dataset.mp4", true);

// Default settings are perfect:
// - PlaybackSpeed=0: Fast reading
// - Backpressure: No drops
// - Bounded memory: Safe for large files

while (auto frame = provider.grab(1000)) {
    // Every frame is guaranteed to be processed
    processFrame(frame);
}
```

### Real-Time Camera Preview

```cpp
ccap::Provider provider;
provider.open(0, true);  // Camera

// Small buffer for minimal latency
provider.setMaxAvailableFrameSize(1);

while (auto frame = provider.grab(1000)) {
    // Always gets the latest frame
    // Old frames automatically dropped
    displayFrame(frame);
}
```

### Smooth Video Playback

```cpp
ccap::Provider provider;
provider.open("movie.mp4", true);

// Enable rate control for smooth playback
provider.set(ccap::PropertyName::PlaybackSpeed, 1.0);

while (auto frame = provider.grab(1000)) {
    // Frames delivered at correct timing
    renderFrame(frame);
}
```

## Monitoring & Debugging

### Check Current Frame

```cpp
auto frame = provider.grab(0);  // Non-blocking
if (frame) {
    std::cout << "Frame index: " << frame->frameIndex << std::endl;
    
    double currentTime = provider.get(ccap::PropertyName::CurrentTime);
    std::cout << "Video time: " << currentTime << "s" << std::endl;
}
```

### Detect Skipped Frames (Camera Mode)

```cpp
uint64_t lastIndex = 0;
while (auto frame = provider.grab(1000)) {
    if (lastIndex > 0 && frame->frameIndex - lastIndex > 1) {
        std::cout << "Dropped " << (frame->frameIndex - lastIndex - 1) 
                  << " frames (normal in camera mode)" << std::endl;
    }
    lastIndex = frame->frameIndex;
}
```

**Note:** In file mode, `frameIndex` is always consecutive (0, 1, 2, ...) with no gaps.

## Best Practices

### ✅ DO

- Use default settings for most use cases
- Set `PlaybackSpeed=1.0` for smooth real-time playback
- Check `frameIndex` in camera mode to detect skips
- Trust that file mode preserves all frames
- Use callback mode for real-time processing

### ❌ DON'T

- Don't worry about memory explosion - it's bounded
- Don't cache all frames yourself - use file mode's backpressure
- Don't expect consecutive indices in camera mode
- Don't assume frame drops in file mode - none occur

## Comparison with OpenCV

ccap's file mode with `PlaybackSpeed=0` behaves like OpenCV's `cv::VideoCapture`:

```cpp
// OpenCV
cv::VideoCapture cap("video.mp4");
while (true) {
    cv::Mat frame;
    if (!cap.read(frame)) break;
    processFrame(frame);
}

// ccap equivalent
ccap::Provider provider;
provider.open("video.mp4", true);
while (auto frame = provider.grab(1000)) {
    processFrame(frame);
}
```

Both provide immediate frame access without rate limiting by default.

## Summary

ccap's memory management is **safe, predictable, and mode-aware**:

**Camera Mode:**
- ✅ Bounded memory (~25-30MB for HD)
- ✅ Drops old frames automatically
- ✅ Low latency
- ⚠️ May skip frames if processing is slow

**File Mode:**
- ✅ Bounded memory (same as camera)
- ✅ Never drops frames (backpressure prevents it)
- ✅ Works with videos of any length
- ✅ All frames can be processed

You can safely process gigabytes of video with constant memory usage and zero frame loss.
