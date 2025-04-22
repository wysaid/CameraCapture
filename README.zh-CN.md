# CameraCapture - 一个 C++ 轻量相机库

[English](./README.md) | [中文](./README.zh-CN.md)

## Overview

CameraCapture 是一个高效的、轻量级的 C++ 相机捕获库，旨在简化相机图像捕获和处理的过程。支持 Windows、MacOS 和 Linux 平台, 不依赖 OpenCV 或者 FFmpeg 等第三方库。它提供了简单易用的 API，适合需要快速实现相机捕获功能的开发者。

## Dependencies

- C++17 或更高版本
- CMake 3.10 或更高版本
- 系统依赖:
  - Windows: MSMF
  - MacOS: AVFoundation

## Usage

使用非常简单, 本项目提供一个头文件和一个静态库, 直接添加到你的项目中即可。
