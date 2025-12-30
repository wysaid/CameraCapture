# 内存管理与视频播放

理解 ccap 的内存管理机制，帮助您为应用程序选择合适的设置。

## 快速参考

| 模式 | 丢帧行为 | 内存使用 | 适用场景 |
|------|----------|----------|----------|
| **相机模式** | ✅ 是（保留最新帧） | 有界（约25-30MB） | 直播流、实时预览 |
| **文件模式** | ❌ 否（反压机制） | 有界（约25-30MB） | 视频处理、帧分析 |

## 核心概念

### 相机模式 vs 文件模式

ccap 会自动检测您正在使用实时相机还是视频文件，并应用不同的策略：

**相机模式** - 优化实时性：
```cpp
ccap::Provider provider;
provider.open(0, true);  // 相机设备

// 优先处理最新帧
// 如果处理速度慢，旧帧会被丢弃
```

**文件模式** - 优化完整性：
```cpp
ccap::Provider provider;
provider.open("video.mp4", true);  // 视频文件

// 保留所有帧
// 当缓冲区满时读取暂停（反压机制）
```

### 反压机制（文件模式）

当使用 `PlaybackSpeed=0`（默认值）处理视频文件时：

1. **快速读取**：解码器尽可能快地读取帧
2. **缓冲区监控**：检查帧队列是否已满
3. **自动暂停**：缓冲区达到限制时暂停读取
4. **消费后恢复**：调用 `grab()` 后恢复读取

**为什么这很重要：**

```cpp
// 没有反压机制：
for (int i = 0; i < 100; i++) {
    auto frame = provider.grab(1000);
    slowProcessing(frame);  // 每帧 100ms
    // ❌ 风险：帧 1-99 可能被丢弃
}

// 有反压机制（ccap 的方式）：
for (int i = 0; i < 100; i++) {
    auto frame = provider.grab(1000);
    slowProcessing(frame);  // 每帧 100ms
    // ✅ 所有帧都保留：0, 1, 2, ..., 99
}
```

### 内存限制

默认配置（来自 `ccap_core.h`）：
- `maxAvailableFrameSize = 3` - 等待 `grab()` 的最大帧数
- `maxCacheFrameSize = 15` - 重用池中的最大帧数

**内存占用：**
```
高清视频（1920×1080，RGBA）：
├── 单帧大小：约 8MB
├── 缓冲区：3 帧 × 8MB = 24MB
├── 帧池开销：极小
└── 总计：约 25-30MB（无论视频长度，保持恒定！）
```

## 播放速度控制

| 值 | 行为 | 内存压力 |
|----|------|----------|
| `0.0`（默认） | 尽可能快地读取 | 受队列限制 |
| `1.0` | 匹配视频帧率 | 较低，更平滑 |
| `2.0` | 2倍速 | 中等 |
| `0.5` | 半速 | 非常低 |

### 示例：处理长视频

```cpp
ccap::Provider provider;
provider.open("long_video.mp4", true);  // 默认 PlaybackSpeed=0

int processedFrames = 0;
while (auto frame = provider.grab(1000)) {
    // 处理每一帧
    // 即使处理速度慢，反压机制可防止：
    // - 丢帧 ✅
    // - 内存爆炸 ✅
    analyzeFrame(frame);
    processedFrames++;
}

std::cout << "已处理所有 " << processedFrames << " 帧\n";
```

## 配置

### 调整缓冲区大小

```cpp
ccap::Provider provider;

// 更低延迟（相机模式下更积极地丢帧）
provider.setMaxAvailableFrameSize(1);

// 更平滑的播放（更多缓冲）
provider.setMaxAvailableFrameSize(5);
```

### 控制播放速率

```cpp
// 快速处理（默认）
provider.open("video.mp4", true);
// PlaybackSpeed=0，尽可能快地读取

// 实时播放
provider.set(ccap::PropertyName::PlaybackSpeed, 1.0);
// 匹配视频原始帧率

// 慢动作分析
provider.set(ccap::PropertyName::PlaybackSpeed, 0.5);
// 半速播放
```

## 常见场景

### 视频逐帧分析

```cpp
ccap::Provider provider;
provider.open("dataset.mp4", true);

// 默认设置最合适：
// - PlaybackSpeed=0：快速读取
// - 反压机制：不丢帧
// - 内存有界：大文件也安全

while (auto frame = provider.grab(1000)) {
    // 保证处理每一帧
    processFrame(frame);
}
```

### 实时相机预览

```cpp
ccap::Provider provider;
provider.open(0, true);  // 相机

// 小缓冲区实现最低延迟
provider.setMaxAvailableFrameSize(1);

while (auto frame = provider.grab(1000)) {
    // 始终获取最新帧
    // 旧帧自动丢弃
    displayFrame(frame);
}
```

### 平滑视频播放

```cpp
ccap::Provider provider;
provider.open("movie.mp4", true);

// 启用速率控制以实现平滑播放
provider.set(ccap::PropertyName::PlaybackSpeed, 1.0);

while (auto frame = provider.grab(1000)) {
    // 按正确时机传递帧
    renderFrame(frame);
}
```

## 监控与调试

### 检查当前帧

```cpp
auto frame = provider.grab(0);  // 非阻塞
if (frame) {
    std::cout << "帧索引: " << frame->frameIndex << std::endl;
    
    double currentTime = provider.get(ccap::PropertyName::CurrentTime);
    std::cout << "视频时间: " << currentTime << "秒" << std::endl;
}
```

### 检测跳过的帧（相机模式）

```cpp
uint64_t lastIndex = 0;
while (auto frame = provider.grab(1000)) {
    if (lastIndex > 0 && frame->frameIndex - lastIndex > 1) {
        std::cout << "丢弃了 " << (frame->frameIndex - lastIndex - 1) 
                  << " 帧（相机模式下正常）" << std::endl;
    }
    lastIndex = frame->frameIndex;
}
```

**注意：** 在文件模式下，`frameIndex` 始终是连续的（0, 1, 2, ...），没有间隙。

## 最佳实践

### ✅ 推荐做法

- 大多数情况使用默认设置
- 设置 `PlaybackSpeed=1.0` 以实现平滑实时播放
- 在相机模式下检查 `frameIndex` 以检测跳帧
- 相信文件模式会保留所有帧
- 使用回调模式进行实时处理

### ❌ 不推荐做法

- 不必担心内存爆炸 - 内存是有界的
- 不要自己缓存所有帧 - 使用文件模式的反压机制
- 不要期望相机模式下的连续索引
- 不要假设文件模式会丢帧 - 不会发生

## 与 OpenCV 对比

ccap 的文件模式使用 `PlaybackSpeed=0` 时的行为类似于 OpenCV 的 `cv::VideoCapture`：

```cpp
// OpenCV
cv::VideoCapture cap("video.mp4");
while (true) {
    cv::Mat frame;
    if (!cap.read(frame)) break;
    processFrame(frame);
}

// ccap 等效代码
ccap::Provider provider;
provider.open("video.mp4", true);
while (auto frame = provider.grab(1000)) {
    processFrame(frame);
}
```

两者默认都提供立即帧访问，不进行速率限制。

## 总结

ccap 的内存管理是**安全、可预测且模式感知**的：

**相机模式：**
- ✅ 内存有界（高清约 25-30MB）
- ✅ 自动丢弃旧帧
- ✅ 低延迟
- ⚠️ 如果处理速度慢可能跳帧

**文件模式：**
- ✅ 内存有界（与相机模式相同）
- ✅ 永不丢帧（反压机制防止）
- ✅ 适用于任意长度的视频
- ✅ 可以处理所有帧

您可以安全地处理 GB 级视频，内存使用保持恒定且零帧丢失。
