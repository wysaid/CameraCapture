# CCAP Android Demo

这是一个展示如何使用CCAP库在Android上进行摄像头操作的示例应用。

## 功能特性

- 列出可用的摄像头设备
- 打开指定的摄像头
- 开始/停止视频捕获
- 实时显示帧计数
- 查看操作日志

## 构建要求

- Android SDK API 21+ (Android 5.0+)
- Android NDK r21+
- CMake 3.18.1+
- Java 8+

## 环境设置

1. 确保已安装Android SDK和NDK：
   ```bash
   export ANDROID_SDK_ROOT=/path/to/android-sdk
   export ANDROID_NDK_ROOT=/path/to/android-ndk
   ```

2. 确保PATH中包含必要的工具：
   ```bash
   export PATH=$ANDROID_SDK_ROOT/platform-tools:$PATH
   export PATH=$ANDROID_SDK_ROOT/tools:$PATH
   ```

## 构建步骤

使用VSCode任务（推荐）：
1. 打开VSCode
2. 按 `Cmd+Shift+P`（macOS）或 `Ctrl+Shift+P`（其他系统）
3. 输入 "Tasks: Run Task"
4. 选择以下任务之一：
   - "Build Android Library (arm64-v8a)" - 构建64位ARM库
   - "Build Android Library (armeabi-v7a)" - 构建32位ARM库
   - "Build Android Demo Project" - 构建完整的Demo APK
   - "Install Android Demo APK" - 安装到设备
   - "Clean Android Build" - 清理构建文件

或者手动构建：

### 1. 构建CCAP库

```bash
# 构建 arm64-v8a 版本
mkdir -p build/android
cd build/android
cmake ../../src \\
    -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake \\
    -DANDROID_ABI=arm64-v8a \\
    -DANDROID_PLATFORM=android-21 \\
    -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# 构建 armeabi-v7a 版本
cd ../..
mkdir -p build/android-v7a
cd build/android-v7a
cmake ../../src \\
    -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake \\
    -DANDROID_ABI=armeabi-v7a \\
    -DANDROID_PLATFORM=android-21 \\
    -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### 2. 构建Android应用

```bash
cd examples/android/CcapDemo
./gradlew assembleDebug
```

### 3. 安装到设备

```bash
./gradlew installDebug
```

## 使用说明

1. 启动应用后，首先会请求摄像头权限
2. 授权后，应用会自动扫描可用的摄像头
3. 从下拉菜单中选择要使用的摄像头
4. 点击"Open Camera"打开摄像头
5. 点击"Start Capture"开始捕获视频帧
6. 观察帧计数的实时更新
7. 点击"Stop Capture"停止捕获
8. 点击"Close Camera"关闭摄像头

## 故障排除

### 权限问题
- 确保应用已被授予摄像头权限
- 在Android设置中检查应用权限

### 构建问题
- 确保ANDROID_NDK_ROOT环境变量正确设置
- 检查NDK版本是否兼容（推荐r21+）
- 确保CMake版本足够新（3.18.1+）

### 运行时问题
- 检查logcat输出以获取详细错误信息：
  ```bash
  adb logcat -s CcapDemo
  ```
- 确保设备支持Camera2 API（Android 5.0+）

## 技术细节

### 架构
- **Java层**: MainActivity管理UI和用户交互
- **JNI层**: android_demo_jni.cpp提供Java到C++的桥接
- **C++层**: 使用CCAP库进行实际的摄像头操作

### 支持的像素格式
- YUV420P (默认)
- NV21
- NV16
- RGB565
- RGBA

### 性能特性
- 使用NEON指令集优化（ARM设备）
- 多线程设计确保UI响应性
- 内存高效的帧传递机制

## 开发指南

如果你想基于这个Demo开发自己的应用：

1. 复制这个项目作为起点
2. 修改包名和应用名
3. 根据需要调整摄像头配置
4. 添加你自己的帧处理逻辑
5. 参考`frameCallback`函数来处理视频帧

## 相关文档

- [CCAP主文档](../../../README.md)
- [Android集成指南](../../../docs/android_integration.md)
- [Android实现总结](../../../ANDROID_IMPLEMENTATION_SUMMARY.md)