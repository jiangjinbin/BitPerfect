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
2.2.2 编写 AudioDecoder 类头文件 ✅（`src/domain/decoder/AudioDecoder.h`，250 行，声明 FileInfo 结构体、Listener 内部接口、AudioDecoder 类含 9 个公开方法 + 1 个私有方法 + 10 个私有成员变量。2026-07-09 重构：fifo_buffer_ 类型从 `std::vector<float>` 改为 `juce::AudioBuffer<float>`，与 `decode_buffer_` 类型统一，平面存储逐声道拷贝无需格式转换）。
2.2.3 实现构造函数和析构函数 ✅（`src/domain/decoder/AudioDecoder.cpp`，约 315 行，构造空体 + 析构调用 stopDecoding() + 10 个空桩方法，每个标注 TODO 指向对应实现步骤）。
2.2.4 实现 open() 方法 ✅（约 110 行，10 步流程：创建 AudioFormatManager → 注册格式 → 创建 AudioFormatReader → 提取元数据 → 计算时长 → 动态分配 AbstractFifo、fifo_buffer_（AudioBuffer<float>，num_channels × (sample_rate×0.5) samples）和 decode_buffer_。同步修复 4 个编译/运行时兼容性问题：format_manager_ 成员、JUCE 模块链接、target_sources、.cpp 启用。2026-07-09 修复 repeat-open 悬垂指针 bug：新增步骤 0 提前释放 reader_，防止旧 reader_ 中的 AudioFormat 裸指针在旧 format_manager_ 被销毁后悬垂）。
2.2.5 实现 startDecoding() 方法 ✅（约 120 行，6 步流程：reader_ 前置检查 → running_ 重复调用守卫 → 旧线程 joinable 回收 → 原子标志初始化 → std::thread 创建 → 成功日志。3 道 Guard 确保线程安全，编译零错误零警告）。
2.2.6 实现 stopDecoding() 方法 ✅（约 45 行，3 步流程：running_.store(false) 通知退出 → joinable() + join() 等待线程结束 → fifo_->reset() 重置缓冲区。join 在 fifo reset 之前执行避免数据竞争，幂等安全。编译零错误零警告）。
2.2.7 实现 decodingLoop() 方法 ✅（约 130 行，6 步流程：获取常量 → while(running_) 循环读取 → reader_->read() 解码一帧 → prepareToWrite + 逐声道 memcpy 写入 fifo_buffer_ → finishedWrite 通知 → 更新位置。修正原 TODO 三处 API 差异：read() 需 6 个参数补全 useReaderRightChan、返回值是 bool 不能用 <=0 判断 EOF（改用 read_position >= total_frames 手动追踪）、reader_ 基类无 getPosition() 方法（改用局部变量）。Fifo 满时阻塞等待 + running_ 检查。两级 try/catch 异常保护。同步提前实现 getFifo()/isDecodingComplete()/getDecodedPosition() 三个 getter（原属 2.2.9，均为单行原子操作，端到端测试必需）。CLI 端到端验证通过：渡口.wav 9,878,988 采样帧全部解码，Fifo 消费帧数一致）。
2.2.8 实现 seekTo() 方法 ✅（约 40 行，4 步流程：越界检查 → stopDecoding() → current_position_.store() → startDecoding()。发现并修正了原始 TODO 中 3 处冲突：① reader_->setPosition() 不存在（JUCE 基类无此方法，改为通过 current_position_ 原子变量传递目标位置到 decodingLoop()）、② startDecoding() 无条件重置 current_position_ 为 0（改为保持当前值不变）、③ decodingLoop() 硬编码 read_position = 0（改为 current_position_.load() 读取起始位置）。CLI 端到端 6 项验证通过：越界检查（负数/超范围静默忽略）、seek 到 1 秒处、seek 到文件末尾 EOF 立即触发、幂等性）。
2.2.10 实现 Listener 管理方法 ✅（约 10 行，addListener() 调用 listeners_.add() / removeListener() 调用 listeners_.remove()。P0 阶段仅管理列表，不触发回调。CLI 验证 6 项通过：添加 / 重复添加 / 移除 / 移除不存在项 / 重新添加 / P0 无回调。编译零错误零警告）。

**开发者学习**：✅ 全部完成（C++、音频基础、JUCE、CMake、辅助库、Git，共 29 章，目录见 `项目规划/学习手册/00-目录与学习路线.md`）。

**下一步**：2.2.11 更新 CMake 构建系统（完善 decoder/CMakeLists.txt + 添加库链接）。2.2.9 三个 getter 已提前完成（2.2.7 端到端测试需要）。2.2 阶段共 14 个子步骤（2.2.1 ✅ → 2.2.2 ✅ → 2.2.3 ✅ → 2.2.4 ✅ → 2.2.5 ✅ → 2.2.6 ✅ → 2.2.7 ✅ → 2.2.8 ✅ → 2.2.10 ✅ → ... → 2.2.14），详见 `项目规划/项目进度.md`。

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
