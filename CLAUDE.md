# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

BitPerfect —— 跨平台（macOS / Linux / Windows）本地音乐播放器，核心目标是绕过系统音频重采样，以独占模式（Hog Mode / WASAPI Exclusive / ALSA 直通）直出音频数据到 DAC。当前阶段仅开发 macOS 版本。

## 技术方向

- 语言：音频核心用 C++（C++17 标准）
- 编码规范：**必须**遵守 `项目规划/C++编码规范.md`（含命名、类型使用、函数规范、音频线程安全等 13 章）
- 音频格式：CoreAudio HAL 物理层仅支持 Interleaved（交错）布局，所有 Planar → Interleaved 转换在音频回调中完成，不得在解码线程中做（会增加内存拷贝次数）。详见 `项目规划/CoreAudio-HAL-设备测试报告.md`。

## 项目进度

> 完整规划见 `项目规划/项目进度.md`，修改进度或阶段划分时同步更新该文件及本段。

**当前状态**：第二阶段 — P0 核心验证。

- 第一阶段（1.1→1.7）✅ 全部完成
- 2.1 工程初始化与 CMake 构建 ✅
- 2.2 解码器模块 ✅（14/14 子步骤全部完成：AudioDecoder 类完整实现，支持 WAV/FLAC 多线程解码、FIFO 缓冲、seek 跳转、Listener 管理；CLI 端到端 7 项测试全通过；单元测试 7 用例 41 断言全通过）
- 开发者学习 ✅ 全部完成

**下一步**：2.3 音频设备管理（AudioDeviceManager）—— 设备枚举、Hog Mode 独占、采样率切换、物理格式匹配。

要点（基于 CoreAudio HAL 设备测试报告）：
- CoreAudio HAL 物理层只认 Interleaved，需在音频回调中实现 Planar → Interleaved 转换
- 通过 `kAudioStreamPropertyPhysicalFormats` 查询设备能力，实现格式匹配
- 优先使用 Integer + NonMixable 格式实现 bit-perfect（SMSL DAC 已验证支持）
- 设备操作（Hog Mode/采样率切换/格式设置）在 UI 线程执行，**禁止**在音频实时线程调用 CoreAudio API

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

## 设备与格式参考

> 测试报告：[项目规划/CoreAudio-HAL-设备测试报告.md](项目规划/CoreAudio-HAL-设备测试报告.md)
> 测试工具：`test_coreaudio_format/`（独立于主项目的 CoreAudio HAL 验证工具，可复现所有数据）

### 关键发现

1. **所有输出设备只支持 Interleaved（交错）格式**：4 台设备、73 种物理格式，Planar 数量为 0。CoreAudio HAL 物理层不认 Planar。
2. **JUCE 回调给的是 Planar**：`audioDeviceIOCallbackWithContext` 的 `outputChannelData` 是 `float* const*`（每声道独立数组），与 HAL 要求的 Interleaved 不匹配。
3. **只有内置扬声器使用 Float**：Mac mini 扬声器是唯一的 Float 数据类型设备；所有外置 DAC（SMSL/D5/Studio Display）都是 Integer。
4. **SMSL USB AUDIO 是核心测试 DAC**：支持 44.1k～768kHz、24/32bit Integer、含 NonMixable 直通格式，是 BitPerfect 验证的最佳目标设备。
5. **NonMixable 标志可用于直通**：SMSL 和 Studio Display 部分格式带有 `kAudioFormatFlagIsNonMixable`，设置此标志可绕过系统混音器。

### 对开发的指导

#### JUCE AudioFormatReader 支持两种解码输出

JUCE 的 `AudioFormatReader::read()` 有**两个重载**：

| 重载 | 输出类型 | 数据范围 | 当前 AudioDecoder 使用情况 |
|------|---------|---------|--------------------------|
| `read(float* const*, ...)` | Planar Float | [-1.0, +1.0] | ✅ 当前使用（`decode_buffer_` 是 `AudioBuffer<float>`） |
| `read(int* const*, ...)` | Planar Int32 | [-0x80000000, 0x7fffffff] | ❌ 未使用，但可直接调用 |

`int` 重载的描述（[juce_AudioFormatReader.h:105-110]）：*"If the format is fixed-point, each channel will be written as an array of 32-bit signed integers using the full range -0x80000000 to 0x7fffffff, regardless of the source's bit-depth."*

#### 对 Integer 路径（Path B）的影响

Integer 路径可以在**解码层就直接拿 int 数据**，不需要 float→int 转换：

- **解码阶段**：调用 `reader->read(int**, ...)` 直接获取 Planar Int32 PCM，数据范围始终是 int32 满幅，与源文件位深无关（16bit/24bit 文件会自动扩展到 int32）
- **音频回调阶段**：只需做 **int32 → 目标位深缩减 + Planar → Interleaved** 转换，然后直接写入 DAC 缓冲区

这意味着 Integer 路径的真正数据流是：

```
文件 → JUCE read(int**) → Planar Int32 → 位深匹配(32→24/16) + Interleave → CoreAudio HAL NonMixable → DAC
```

**全程零浮点**，不需要绕过 JUCE。

#### 2.3 独占模式（Hog Mode）必须处理的格式转换

JUCE 回调给的是 **Planar Float**，HAL 要的是 **Interleaved Integer**（外置 DAC）或 **Interleaved Float**（内置扬声器）。实现 Hog Mode 时必须在音频回调中包含以下转换层：

- **Float 路径（Path A）**：Planar Float → Interleave → Interleaved Float（内置扬声器，直接交错排列即可）或 Planar Float → Interleave → Interleaved Float（外置 DAC，CoreAudio 内部自动做 float→int 转换，我们只需 interleave）
- **Integer 路径（Path B）**：解码阶段用 `reader->read(int**, ...)` 获取 Planar Int32 → 音频回调中做位深匹配 + Interleave → Interleaved Int16/Int24/Int32 → 直接写入 DAC 物理格式缓冲区（配合 NonMixable 绕过系统混音器，实现 bit-perfect）

#### 各设备 Integer 格式能力速查表

以下数据来自 `test_coreaudio_format` 工具对各设备的 `kAudioStreamPropertyAvailablePhysicalFormats` 实际查询结果（2026-07-10）：

| 设备 | int16 | int20 | int24 | int32 | NonMixable | Float |
|------|:---:|:---:|:---:|:---:|:---:|:---:|
| **SMSL USB AUDIO** ⭐ | ❌ | ❌ | ✅（20 种） | ✅（20 种） | ✅（20/40） | ❌ |
| D5 | ✅（7 种） | ✅（7 种） | ✅（7 种） | ❌ | ❌ | ❌ |
| Studio Display | ❌ | ❌ | ✅（8 种） | ❌ | ✅（4/8） | ❌ |
| Mac mini 扬声器 | ❌ | ❌ | ❌ | ❌ | ❌ | ✅（4 种） |

> 💡 这意味着：SMSL 不能用 int16（没有对应物理格式）；D5 不能用 int32（最大 24bit）；Mac mini 扬声器根本不能走 Integer 路径。

#### 设备格式匹配策略

播放前按以下流程选择目标物理格式：

1. **读取源文件属性**：采样率 + 位深（16/24/32bit）
2. **由用户 Hog Mode 开关决定 NonMixable**：Hog Mode 开启 → 只选带 NonMixable 标志的物理格式；Hog Mode 关闭 → 只选不带 NonMixable 的格式。NonMixable 是 Hog Mode 在流格式层的体现，不是设备自动选择项。
3. **Integer 路径（Path B）位深匹配**（在已筛选的格式中选，主流 DAC 都有 32bit，直接选即可）：
   - 32bit Integer（JUCE 输出就是 int32，直接零转换喂给 DAC）
   - 若无 Integer 格式 → 回退 Float 路径
4. **Float 路径（Path A）回退**：当设备不支持 Integer 格式时（如 Mac mini 扬声器），使用 Float32 格式，CoreAudio 内部隐式转换

#### 位深匹配转换简表

JUCE `read(int**, ...)` 统一输出 int32。主流 DAC 都支持 32bit Integer 物理格式，选它即可零转换。

| 场景 | 目标物理格式 | 回调中做什么 |
|------|:---:|------|
| 外置 DAC（SMSL 等，有 32bit） | 32bit Integer（Hog Mode 下加 NonMixable） | **零转换**：Planar int32 → Interleave → 写入 |
| 内置扬声器（无 Integer） | — | 无法走 Integer 路径，回退 Float 路径 |

#### 测试验证优先级

1. **SMSL USB AUDIO**（默认设备，格式最全，优先测试 Integer Mode）
2. Mac mini 扬声器（验证 Float 路径回退逻辑）
