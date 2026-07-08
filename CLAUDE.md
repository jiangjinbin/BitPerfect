# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

BitPerfect —— 跨平台（macOS / Linux / Windows）本地音乐播放器，核心目标是绕过系统音频重采样，以独占模式（Hog Mode / WASAPI Exclusive / ALSA 直通）直出音频数据到 DAC。当前阶段仅开发 macOS 版本。

## 技术方向

- 语言：音频核心用 C++（C++17 标准）
- 编码规范：**必须**遵守 `项目规划/C++编码规范.md`（含命名、类型使用、函数规范、音频线程安全等 13 章）

## 项目进度

> 完整规划见 `项目规划/项目进度.md`，修改进度或阶段划分时同步更新该文件及本段。

**当前状态**：第二阶段 — P0 核心验证。第一阶段全部完成 ✅（1.1 需求分析 → 1.7 C++ 编码规范）。2.1 项目工程初始化与 CMake 构建 ✅（13 个文件、7 个 CMakeLists.txt、5 个第三方库集成、CLI 桩代码验证通过）。

**开发者学习**：✅ 全部完成（C++、音频基础、JUCE、CMake、辅助库、Git，共 29 章，目录见 `项目规划/学习手册/00-目录与学习路线.md`）。

**下一步**：2.2 基础音频文件解码（FLAC/WAV，PCM float）—— 编写 AudioDecoder 模块，验证 JUCE AudioFormatReader 解码正确性。

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
