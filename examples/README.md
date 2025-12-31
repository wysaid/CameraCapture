# ccap Examples

## Desktop Examples

Build desktop examples using CMake:

```bash
cd ccap
mkdir build && cd build
cmake .. -DCCAP_BUILD_EXAMPLES=ON
cmake --build .
```

**Available examples:**

| Example | Description | Platform |
|---------|-------------|----------|
| `0-print_camera` / `0-print_camera_c` | List available cameras | All |
| `1-minimal_example` / `1-minimal_example_c` | Basic frame capture | All |
| `2-capture_grab` / `2-capture_grab_c` | Continuous capture | All |
| `3-capture_callback` / `3-capture_callback_c` | Callback-based capture | All |
| `4-example_with_glfw` / `4-example_with_glfw_c` | OpenGL rendering | All |
| `5-play_video` / `5-play_video_c` | Video file playback | Windows/macOS |

Each example is available in both C++ (`.cpp`) and pure C (`.c`) variants.

## iOS Example

Build iOS example using CocoaPods:

```bash
cd ccap/examples
pod install
open *.xcworkspace
```

Build and run using Xcode.
