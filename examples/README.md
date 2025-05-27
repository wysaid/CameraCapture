# ccap Examples Build Instructions

## Desktop (Windows/macOS, etc.)

Initialize and build as a standard CMake project to get the desktop demo:

```sh
cd /path/to/ccap_repo
mkdir -p build && cd build
cmake ..
cmake --build .
```

## iOS

The `examples` directory contains a standard iOS project with CocoaPods support.

Simply run `pod install` in the `examples` directory, then open the workspace file:

```sh
brew install cocoapods # Install cocoapods for iOS demo
cd /path/to/ccap_repo
cd examples
pod install
open *.xcworkspace # Then use Xcode to build and run
```

## Android

WIP...
