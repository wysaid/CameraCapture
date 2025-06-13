# ccap Examples

## Desktop Examples

Build desktop examples using CMake:

```bash
cd ccap
mkdir build && cd build
cmake .. -DCCAP_BUILD_EXAMPLES=ON
cmake --build .
```

## iOS Example

Build iOS example using CocoaPods:

```bash
cd ccap/examples
pod install
open *.xcworkspace
```

Build and run using Xcode.
