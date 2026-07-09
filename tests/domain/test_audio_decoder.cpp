/**
 * 文件名：test_audio_decoder.cpp
 * 职责：AudioDecoder 模块的单元测试 —— 验证文件打开、元数据提取、全量解码、错误处理、seek 跳转
 * 所属模块：tests/domain（测试层-领域层）
 *
 * 测试覆盖（6 个测试用例）：
 *   1. open() 成功读取 WAV 文件信息（采样率/声道/位深/总帧数/时长/格式名）
 *   2. open() 成功读取 FLAC 文件信息（同上，覆盖不同格式/采样率/位深）
 *   3. 解码 WAV 到完成 —— 全量解码 + Fifo 消费，验证帧数一致性
 *   4. 解码 FLAC 到完成 —— 同上，覆盖压缩格式解码路径
 *   5. open() 不存在的文件返回 false —— 错误处理 + 默认状态验证
 *   6. seekTo() 跳转到中间位置后解码位置正确 —— 越界保护 + 合法跳转
 *
 * 运行方式：
 *   # 只运行本文件的测试
 *   ./build/debug/tests/BitPerfectTests "[AudioDecoder]" -s
 *
 *   # 运行全部测试
 *   ./build/debug/tests/BitPerfectTests -s
 *
 *   # 通过 CTest 运行
 *   ctest --test-dir build/debug -R AudioDecoder -V
 *
 * 当前状态（2.2.14）：✅ 6 个测试用例全部实现
 */

// ============================================================
// 测试框架
// ============================================================
// Catch2 测试宏头文件：提供 TEST_CASE、REQUIRE、CHECK、SECTION 等宏
// Catch2::Catch2WithMain 自动提供 main 函数，测试文件无需自己写 main
#include <catch2/catch_test_macros.hpp>

// ============================================================
// 被测试的模块
// ============================================================
#include "domain/decoder/AudioDecoder.h"  // AudioDecoder 类 + FileInfo 结构体 + Listener 接口

// ============================================================
// JUCE 库（测试文件直接使用 JUCE 类型，与 CLI 测试一致）
// ============================================================
#include <juce_core/juce_core.h>                     // juce::File、juce::AbstractFifo、juce::String
#include <juce_audio_formats/juce_audio_formats.h>    // juce::AudioFormatReader（AudioDecoder 内部使用）

// ============================================================
// C++ 标准库
// ============================================================
#include <cmath>    // std::abs —— 浮点数容差比较
#include <chrono>   // std::chrono::milliseconds —— 等待解码线程产出数据
#include <thread>   // std::this_thread::sleep_for —— 消费者循环中的等待
#include <string>   // std::string —— 文件名和格式名比较


// ============================================================
// 辅助函数
// ============================================================

/**
 * 定位测试资源目录下的音频文件（增强版，兼容多种运行方式）
 *
 * 与 CLI 测试中的 locateTestFile() 不同，本版本会向上搜索"测试资源/"目录，
 * 以兼容 CTest 将工作目录设为 build/debug/ 而非项目根目录的情况。
 *
 * 搜索逻辑：
 *   从当前工作目录（juce::File::getCurrentWorkingDirectory()）开始，
 *   向上查找最多 5 层父目录，寻找名为"测试资源"的子目录。
 *   找到后，在测试资源目录下定位指定文件名的文件。
 *
 * 使用 juce::String::fromUTF8() 处理包含中文的目录名（"测试资源" 含中文字符）。
 *
 * @param file_name 文件名（不含路径），如 "渡口.wav"、"鸳鸯戏.flac"
 * @return 对应的 juce::File 对象（即使文件不存在也返回路径对象，由调用方检查）
 */
juce::File locateTestFile(const std::string& file_name) {
    // 从当前工作目录开始搜索
    juce::File dir = juce::File::getCurrentWorkingDirectory();

    // 向上最多搜索 5 层父目录
    // 覆盖场景：
    //   - CWD = 项目根目录：立即找到（第 0 层）
    //   - CWD = build/debug/：向上一层到项目根目录，找到（第 1 层）
    //   - CWD = build/debug/tests/：向上两层，找到（第 2 层）
    for (int i = 0; i < 5; ++i) {
        // 在当前搜索目录下查找"测试资源"子目录
        juce::File test_dir = dir.getChildFile(
            juce::String::fromUTF8("测试资源"));

        // 如果找到了测试资源目录，直接返回指定文件
        if (test_dir.isDirectory()) {
            return test_dir.getChildFile(
                juce::String::fromUTF8(file_name.c_str()));
        }

        // 没找到，向上一层继续搜索
        dir = dir.getParentDirectory();
    }

    // 兜底：向上 5 层都没找到，返回基于 CWD 的相对路径
    // 调用方会通过 existsAsFile() 检查并给出明确的失败信息
    return juce::File::getCurrentWorkingDirectory()
        .getChildFile(juce::String::fromUTF8("测试资源"))
        .getChildFile(juce::String::fromUTF8(file_name.c_str()));
}

/**
 * 从 AbstractFifo 中消费一次数据（非阻塞）
 *
 * 调用 AbstractFifo 的 prepareToRead / finishedRead 方法从环形缓冲区中
 * 取走当前所有就绪的数据帧。AbstractFifo 内部使用原子变量管理读写指针，
 * 因此本函数可以被任意线程安全调用。
 *
 * 消费模式说明：
 *   AbstractFifo 的 prepareToRead / finishedRead 方法可能返回两段不连续的数据（因为环形缓冲区
 *   在末尾会回绕），本函数处理了这种两段场景。
 *
 * @param fifo            要消费的 AbstractFifo 引用
 * @param frames_consumed 累计消费帧数（传出参数，本函数会在此值上累加）
 */
void consumeFifoData(juce::AbstractFifo& fifo, juce::int64& frames_consumed) {
    // 查询当前 Fifo 中有多少帧数据可供读取
    int num_ready = fifo.getNumReady();

    // 没有数据可读，直接返回
    if (num_ready <= 0) {
        return;
    }

    // prepareToRead 返回两个连续块的信息：
    //   start1, size1 —— 第一个连续块（从当前读指针位置到缓冲区末尾或写指针位置）
    //   start2, size2 —— 第二个连续块（仅在环形缓冲区回绕时非零）
    // 我们需要准备好接收这两段数据，以便完整消费所有就绪帧
    int start1 = 0;
    int size1 = 0;
    int start2 = 0;
    int size2 = 0;
    fifo.prepareToRead(num_ready, start1, size1, start2, size2);

    // 计算本次实际消费的总帧数（size2 在无回绕时为 0）
    int total_read = size1 + size2;

    // 通知 Fifo 这些帧已被消费，读指针前进
    // 注意：我们不需要拷贝实际数据（测试只关心帧计数），
    // 但如果未来需要验证 PCM 数据正确性，可以在这里读取 fifo_buffer_
    fifo.finishedRead(total_read);

    // 累加到外部计数器
    frames_consumed += total_read;
}

/**
 * 消费所有已解码的帧，直到解码完成且 Fifo 为空
 *
 * 这是一个阻塞函数：它会持续轮询 isDecodingComplete() 和 Fifo 状态，
 * 直到所有帧都被解码并消费完毕。
 *
 * 循环逻辑：
 *   while (true):
 *     - 如果解码完成 且 Fifo 为空 → 退出（所有数据已消费完毕）
 *     - 如果 Fifo 中有数据 → 立即消费
 *     - 如果 Fifo 为空但解码未完成 → 休眠 5ms 等待解码线程产出新数据
 *
 * 线程安全说明：
 *   本函数运行在测试线程（主线程），消费 Fifo 中的数据；
 *   AudioDecoder 的解码线程（后台线程）同时向 Fifo 写入数据。
 *   AbstractFifo 的无锁设计保证两线程无需互斥锁即可安全并发。
 *
 * @param decoder 已调用 open() 和 startDecoding() 的 AudioDecoder 实例
 * @return 总共消费的采样帧数
 */
juce::int64 consumeAllDecodedFrames(AudioDecoder& decoder) {
    // 获取 Fifo 引用（必须在 open() 之后调用，否则 fifo_ 为 nullptr）
    juce::AbstractFifo& fifo = decoder.getFifo();

    // 累计消费的帧数
    juce::int64 frames_consumed = 0;

    // 循环消费直到所有帧都处理完毕
    while (true) {
        // 检查退出条件：解码完成 + Fifo 已清空
        bool complete = decoder.isDecodingComplete();
        bool empty   = (fifo.getNumReady() == 0);

        if (complete && empty) {
            break;  // 全部解码并消费完毕
        }

        // Fifo 中有数据，立即消费
        if (!empty) {
            consumeFifoData(fifo, frames_consumed);
        } else {
            // Fifo 为空但解码还在进行中，短暂休眠等待解码线程写入新数据
            // 5ms 是经验值：足够短不会明显延迟，足够长不会浪费 CPU
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    return frames_consumed;
}


// ============================================================
// 测试用例 1：open() 成功读取 WAV 文件信息
// ============================================================
/**
 * 验证 open() 能正确解析 WAV 文件的元数据并填充 FileInfo 结构体。
 *
 * 前置条件：测试资源目录中存在"渡口.wav"（44.1kHz/16bit/立体声）
 * 预期行为：open() 返回 true，FileInfo 各字段值与已知文件属性一致
 */
TEST_CASE("open() 成功读取 WAV 文件信息", "[AudioDecoder]") {
    // --- 准备：构造解码器并定位测试文件 ---
    AudioDecoder decoder;
    juce::File file = locateTestFile("渡口.wav");

    // 如果测试文件不存在（例如测试资源未下载），测试应该失败并给出明确信息
    REQUIRE(file.existsAsFile());

    // --- 执行：打开文件 ---
    bool result = decoder.open(file);

    // --- 验证：open() 必须成功 ---
    REQUIRE(result == true);

    // --- 验证：FileInfo 各字段值 ---
    // 使用 CHECK（而非 REQUIRE）逐字段验证，
    // 这样即使某个字段不匹配，其他字段的检查也会继续执行，
    // 一次测试运行就能看到所有不匹配的字段，方便定位问题
    const FileInfo& info = decoder.getFileInfo();

    // 格式名应包含 "WAV"（JUCE 可能返回 "WAV" 或 "WAV file"，子串匹配更鲁棒）
    CHECK(info.format_name.find("WAV") != std::string::npos);

    // 采样率 44100.0 Hz，浮点容差 0.5（避免浮点表示误差导致误报）
    CHECK(std::abs(info.sample_rate - 44100.0) < 0.5);

    // 声道数：立体声 = 2
    CHECK(info.num_channels == 2);

    // 位深：16 bit
    CHECK(info.bit_depth == 16);

    // 总帧数：9,878,988（已知值，渡口.wav 的实际帧数）
    CHECK(info.total_frames == 9878988);

    // 时长 = 总帧数 / 采样率，容差 0.01 秒
    CHECK(std::abs(info.duration_seconds - 9878988.0 / 44100.0) < 0.01);

    // 格式名不应为空（基础健壮性检查）
    CHECK(!info.format_name.empty());
}


// ============================================================
// 测试用例 2：open() 成功读取 FLAC 文件信息
// ============================================================
/**
 * 验证 open() 能正确解析 FLAC 文件的元数据。
 *
 * FLAC 与 WAV 的区别：
 *   - FLAC 是压缩格式，需要解压才能读取 PCM 数据
 *   - 不同的采样率（48kHz vs 44.1kHz）和位深（24bit vs 16bit）
 *   - 48kHz 采样率意味着更大的 fifo_buffer_（48k × 0.5 = 24k frames vs 22.05k frames）
 *   - 这验证了 AudioDecoder 的格式无关设计
 *
 * 前置条件：测试资源目录中存在"鸳鸯戏.flac"（48kHz/24bit/立体声）
 */
TEST_CASE("open() 成功读取 FLAC 文件信息", "[AudioDecoder]") {
    // --- 准备 ---
    AudioDecoder decoder;
    juce::File file = locateTestFile("鸳鸯戏.flac");

    // 测试文件必须存在
    REQUIRE(file.existsAsFile());

    // --- 执行 ---
    bool result = decoder.open(file);

    // --- 验证 ---
    REQUIRE(result == true);

    const FileInfo& info = decoder.getFileInfo();

    // 格式名应包含 "FLAC"
    CHECK(info.format_name.find("FLAC") != std::string::npos);

    // 采样率 48000.0 Hz
    CHECK(std::abs(info.sample_rate - 48000.0) < 0.5);

    // 声道数：立体声
    CHECK(info.num_channels == 2);

    // 位深：24 bit（FLAC 的高解析度特性）
    CHECK(info.bit_depth == 24);

    // 总帧数：10,133,504（已知值，鸳鸯戏.flac 的实际帧数）
    CHECK(info.total_frames == 10133504);

    // 时长 = 总帧数 / 采样率
    CHECK(std::abs(info.duration_seconds - 10133504.0 / 48000.0) < 0.01);

    // 格式名不应为空
    CHECK(!info.format_name.empty());
}


// ============================================================
// 测试用例 3：解码 WAV 到完成
// ============================================================
/**
 * 端到端验证：后台解码线程将 WAV 文件的所有帧正确解码并写入 AbstractFifo。
 *
 * 测试流程：
 *   1. 打开 WAV 文件
 *   2. 启动后台解码线程
 *   3. 在主线程中循环消费 Fifo 中的数据
 *   4. 验证 isDecodingComplete() 返回 true
 *   5. 验证消费帧数 = 文件总帧数
 *
 * 这验证了"生产者-消费者"模型的核心正确性：
 *   生产者 = 解码线程（decodingLoop）
 *   消费者 = 测试线程（本函数中的消费循环）
 *   缓冲区 = AbstractFifo（无锁环形缓冲区）
 */
TEST_CASE("解码 WAV 到完成（isDecodingComplete() == true）", "[AudioDecoder]") {
    // --- 准备：打开文件 ---
    AudioDecoder decoder;
    juce::File file = locateTestFile("渡口.wav");
    REQUIRE(file.existsAsFile());
    REQUIRE(decoder.open(file) == true);

    // 从 FileInfo 获取总帧数（动态获取，不硬编码，这样更换测试文件时只改测试 1）
    juce::int64 total_frames = decoder.getFileInfo().total_frames;

    // --- 执行：启动解码 + 消费所有帧 ---
    decoder.startDecoding();
    juce::int64 frames_consumed = consumeAllDecodedFrames(decoder);

    // --- 清理：停止解码线程 ---
    decoder.stopDecoding();

    // --- 验证 ---
    // 解码完成标志应为 true
    CHECK(decoder.isDecodingComplete() == true);

    // 消费的帧数应等于文件总帧数（逐帧不丢）
    CHECK(frames_consumed == total_frames);
}


// ============================================================
// 测试用例 4：解码 FLAC 到完成
// ============================================================
/**
 * 端到端验证：后台解码线程将 FLAC 文件的所有帧正确解码并写入 AbstractFifo。
 *
 * 与测试用例 3 的区别：
 *   - FLAC 是真正需要解压的格式（WAV 本质是 PCM + 头），
 *     这验证了 JUCE AudioFormatReader 的 FLAC 解码路径
 *   - 48kHz 采样率意味着更大的 fifo_buffer_（48k × 0.5 = 24k frames vs 22.05k frames）
 *   - 24bit 位深意味着不同的内部数据转换路径
 */
TEST_CASE("解码 FLAC 到完成", "[AudioDecoder]") {
    // --- 准备 ---
    AudioDecoder decoder;
    juce::File file = locateTestFile("鸳鸯戏.flac");
    REQUIRE(file.existsAsFile());
    REQUIRE(decoder.open(file) == true);

    juce::int64 total_frames = decoder.getFileInfo().total_frames;

    // --- 执行 ---
    decoder.startDecoding();
    juce::int64 frames_consumed = consumeAllDecodedFrames(decoder);

    // --- 清理 ---
    decoder.stopDecoding();

    // --- 验证 ---
    CHECK(decoder.isDecodingComplete() == true);
    CHECK(frames_consumed == total_frames);
}


// ============================================================
// 测试用例 5：打开不存在的文件返回 false
// ============================================================
/**
 * 验证错误处理：当 open() 被传入一个不存在的文件路径时，
 * 应返回 false 且 FileInfo 保持默认初始值。
 *
 * 这是防御性编程的基础验证：调用方可以安全地通过返回值判断 open() 是否成功，
 * 并在失败后读取 getFileInfo() 而不获得脏数据。
 */
TEST_CASE("open() 不存在的文件返回 false", "[AudioDecoder]") {
    // --- 准备：构造一个指向不存在文件的路径 ---
    AudioDecoder decoder;

    // locateTestFile 会返回"测试资源/__nonexistent__.xyz"路径，
    // 但该文件在测试资源目录中不存在，因此 open() 应该失败
    juce::File nonexistent = locateTestFile("__nonexistent__.xyz");

    // 确认这个文件确实不存在（双重保险）
    REQUIRE(!nonexistent.existsAsFile());

    // --- 执行：尝试打开不存在的文件 ---
    bool result = decoder.open(nonexistent);

    // --- 验证：返回值 ---
    CHECK(result == false);

    // --- 验证：FileInfo 保持默认值 ---
    const FileInfo& info = decoder.getFileInfo();

    // total_frames 默认为 0（文件未打开，没有解析出任何帧数）
    CHECK(info.total_frames == 0);

    // format_name 默认为空字符串（没有识别到任何格式）
    CHECK(info.format_name.empty());

    // 补充验证：open() 失败后调用 startDecoding() 不应崩溃
    // AudioDecoder::startDecoding() 有 reader_ == nullptr 守卫，
    // 检测到无 reader 时会记录错误并立即返回
    REQUIRE_NOTHROW(decoder.startDecoding());
}


// ============================================================
// 测试用例 6：seekTo() 跳转到中间位置后解码位置正确
// ============================================================
/**
 * 验证 seekTo() 的三类场景：
 *   1. 负数越界 → 静默忽略，位置不变
 *   2. 超出 EOF 越界 → 静默忽略，位置不变
 *   3. 合法跳转 → 解码从目标位置恢复，getDecodedPosition() >= 目标位置
 *
 * 三个场景使用 Catch2 SECTION 拆分为独立子测试，确保一个失败不影响其他。
 *
 * seek 机制回顾：
 *   AudioDecoder 通过 current_position_ 原子变量将目标位置从 seekTo()
 *   传递给 decodingLoop()，而非直接操作 AudioFormatReader（JUCE 基类
 *   没有 setPosition() 方法）。decodingLoop 启动时从 current_position_
 *   读取起始位置，传给 reader_->read() 的 readerStartSample 参数。
 */
TEST_CASE("seekTo() 跳转到中间位置后解码位置正确", "[AudioDecoder]") {
    // --- 准备：打开 WAV 文件（每个 SECTION 都会重新执行此处，确保状态干净）---
    AudioDecoder decoder;
    juce::File file = locateTestFile("渡口.wav");
    REQUIRE(file.existsAsFile());
    REQUIRE(decoder.open(file) == true);

    juce::int64 total_frames = decoder.getFileInfo().total_frames;

    // ================================================================
    // SECTION 1：负数越界被静默忽略
    // ================================================================
    SECTION("负数越界被静默忽略") {
        // seekTo(-100) 应该在边界检查中直接返回，不改变任何状态
        decoder.seekTo(-100);

        // 由于没有启动解码（seekTo 在越界检查后直接 return），
        // 位置保持为初始值 0
        CHECK(decoder.getDecodedPosition() == 0);
    }

    // ================================================================
    // SECTION 2：超出 total_frames 越界被静默忽略
    // ================================================================
    SECTION("超出 total_frames 越界被静默忽略") {
        // seekTo(total_frames + 1000) 超出文件末尾，应被忽略
        decoder.seekTo(total_frames + 1000);

        // 位置应保持初始值 0（越界被忽略，解码未启动）
        CHECK(decoder.getDecodedPosition() == 0);
    }

    // ================================================================
    // SECTION 3：合法 seek 后解码从目标位置恢复
    // ================================================================
    SECTION("seekTo 到合法位置后解码从目标位置开始") {
        // 跳转到 5 秒处（采样率 44100 × 5 = 220500 帧）
        juce::int64 seek_target = 44100 * 5;
        decoder.seekTo(seek_target);

        // seekTo() 内部调用了 startDecoding()，解码线程已经在后台运行。
        // 短暂休眠等待解码线程产出第一个 chunk（4096 帧），
        // 通常只需几个毫秒，100ms 是保守的安全值。
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // 验证解码位置已经超过 seek 目标（说明解码确实从新位置开始了）
        juce::int64 current_pos = decoder.getDecodedPosition();
        CHECK(current_pos >= seek_target);

        // 清理：停止解码线程
        decoder.stopDecoding();
    }
}
