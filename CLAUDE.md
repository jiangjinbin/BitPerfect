# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

BitPerfect —— 跨平台（macOS / Linux / Windows）本地音乐播放器，核心目标是绕过系统音频重采样，以独占模式（Hog Mode / WASAPI Exclusive / ALSA 直通）直出音频数据到 DAC。当前阶段仅开发 macOS 版本。

## 技术方向

- 语言：音频核心用 C++（C++17 标准）
- 编码规范：**必须**遵守 `项目规划/C++编码规范.md`（含命名、类型使用、函数规范、音频线程安全等 13 章）

## 项目进度

> 完整规划见 `项目规划/项目进度.md`，修改进度或阶段划分时同步更新该文件及本段。

**当前状态**：第二阶段 — P0 核心验证。第一阶段全部完成 ✅（1.1 需求分析 → 1.7 C++ 编码规范）。2.1 项目工程初始化与 CMake 构建 ✅（13 个文件、7 个 CMakeLists.txt、5 个第三方库集成、CLI 桩代码验证通过）。2.2.1 创建 decoder 模块目录和 CMakeLists.txt ✅（`src/domain/decoder/` 目录 + CMakeLists.txt 骨架，`add_subdirectory(decoder)` 构建链路验证通过）。
2.2.2 编写 AudioDecoder 类头文件 ✅（`src/domain/decoder/AudioDecoder.h`，250 行，声明 FileInfo 结构体、Listener 内部接口、AudioDecoder 类含 9 个公开方法 + 1 个私有方法 + 10 个私有成员变量）。
2.2.3 实现构造函数和析构函数 ✅（`src/domain/decoder/AudioDecoder.cpp`，约 315 行，构造空体 + 析构调用 stopDecoding() + 10 个空桩方法，每个标注 TODO 指向对应实现步骤）。
2.2.4 实现 open() 方法 ✅（约 110 行，10 步流程：创建 AudioFormatManager → 注册格式 → 创建 AudioFormatReader → 提取元数据 → 计算时长 → 动态分配 AbstractFifo 和 decode_buffer_。同步修复 4 个编译/运行时兼容性问题：format_manager_ 成员、JUCE 模块链接、target_sources、.cpp 启用）。

**开发者学习**：✅ 全部完成（C++、音频基础、JUCE、CMake、辅助库、Git，共 29 章，目录见 `项目规划/学习手册/00-目录与学习路线.md`）。

**下一步**：2.2.5 实现 `startDecoding()` 方法 —— 检查 reader_ 是否已创建、设置 running_ 等原子标志、创建 std::thread 执行 decodingLoop()、防止重复调用。2.2 阶段共 14 个子步骤（2.2.1 ✅ → 2.2.2 ✅ → 2.2.3 ✅ → 2.2.4 ✅ → 2.2.5 → ... → 2.2.14），详见 `项目规划/项目进度.md`。

**功能优先级**：P0 核心验证（CLI 验证 Float + Integer 双路径，证明 bit-perfect 可行）→ P1 基础播放器（带 UI 的 MVP）→ P2 音乐管理（曲库 + 数据库）→ P3 体验增强与发布 → P4 未来扩展（跨平台 + 高级功能）。详见 `项目规划/需求分析.md` 第二章。

## 沟通偏好

- 使用中文沟通
- 解释原理后再给代码
- 代码和配置文件需逐行添加注释（便于不懂技术的新手学习、理解、快速上手和后期维护）
- 代码优先选择最清晰直白的写法，不追求语法糖和简写

## 上下文排除

以下目录是给人阅读的参考资料，AI 不要主动搜索或读取其中的文件（会浪费 token）：
- `项目规划/学习手册/`
- `项目规划/技术选型参考对比/`

## 测试资源

测试用的音频文件统一放在 `测试资源/` 目录下。开发和验证时优先使用该目录中的文件。

当前可用资源：

| 文件 | 格式 | 采样率 | 位深 | 声道 | 时长 | 说明 |
|------|------|--------|------|------|------|------|
| `测试资源/鸳鸯戏.flac` | FLAC | 48kHz | 24bit | 立体声 | 3分31秒 | 流行歌曲，用于 FLAC 解码验证 |
| `测试资源/渡口.wav` | WAV | 44.1kHz | 16bit | 立体声 | 3分44秒 | 蔡琴经典试音曲，用于 WAV 解码验证 |
