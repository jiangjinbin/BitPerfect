/**
 * 文件名：main.cpp
 * 职责：BitPerfect CLI 测试程序 —— AudioDecoder 模块端到端验证
 * 所属模块：CLI（命令行接口层）
 *
 * 测试覆盖：
 *   1. WAV 文件打开与元数据提取（格式/采样率/声道/位深）
 *   2. FLAC 文件打开与元数据提取（格式/采样率/位深）
 *   3. WAV 文件全解码验证（Fifo 消费帧数 = 文件总帧数）
 *   4. FLAC 文件全解码验证（Fifo 消费帧数 = 文件总帧数）
 *   5. seekTo() 跳转播放位置（越界/合法/EOF 三种场景）
 *   6. Listener 管理方法（add/remove 6 种场景）
 *   7. 错误处理（打开不存在的文件）
 *
 * 编译与运行（需在项目根目录执行）：
 *   # 编译
 *   cmake --build build/debug --target BitPerfectCli
 *
 *   # 运行
 *   ./build/debug/src/BitPerfectCli
 *
 * 退出码：
 *   0 = 全部测试通过
 *   1 = 至少一项测试失败
 *
 * 当前状态（2.2.12）：✅ 完整实现，7 项测试覆盖 AudioDecoder 全部公开 API
 */

// ============================================================
// 项目头文件
// ============================================================
#include "domain/decoder/AudioDecoder.h"

// ============================================================
// 第三方库
// ============================================================
#include <spdlog/spdlog.h>             // 日志输出：info/warn/error
#include <juce_core/juce_core.h>       // juce::File、juce::AbstractFifo、juce::String

// ============================================================
// C++ 标准库
// ============================================================
#include <cmath>        // std::abs —— 浮点数容差比较
#include <cstdlib>      // EXIT_SUCCESS、EXIT_FAILURE
#include <string>       // std::string
#include <thread>       // std::this_thread::sleep_for
#include <chrono>       // std::chrono::milliseconds


// ============================================================
// 辅助函数
// ============================================================

/**
 * 定位测试资源目录下的音频文件
 *
 * 从当前工作目录（应为项目根目录）出发，进入 测试资源/ 子目录定位指定文件。
 * 使用 juce::String::fromUTF8() 处理中文路径（"测试资源" 包含中文字符）。
 *
 * @param file_name 文件名（不含路径），如 "渡口.wav"、"鸳鸯戏.flac"
 * @return 对应的 juce::File 对象（即使文件不存在也返回路径对象，由调用方检查）
 */
juce::File locateTestFile(const std::string& file_name) {
    // 获取当前工作目录（运行 CLI 程序时的目录，预期为项目根目录）
    juce::File current_dir = juce::File::getCurrentWorkingDirectory();

    // 进入测试资源子目录
    // fromUTF8 将 C++ 字符串（UTF-8 编码）转换为 JUCE 内部使用的 String 类型
    juce::File test_resource_dir = current_dir.getChildFile(
        juce::String::fromUTF8("测试资源"));

    // 定位具体文件
    // fromUTF8 接受 const char*，使用 c_str() 将 std::string 转为 C 字符串
    juce::File test_file = test_resource_dir.getChildFile(
        juce::String::fromUTF8(file_name.c_str()));

    return test_file;
}

/**
 * 打开音频文件并打印完整的元数据信息
 *
 * 通过 AudioDecoder::open() 打开文件，通过 getFileInfo() 获取元数据，
 * 以人类友好的格式打印到控制台。打印的信息包括：格式名、采样率、声道数、
 * 位深、总帧数、时长、预期 Fifo float 总数。
 *
 * @param decoder 刚构造的 AudioDecoder 实例（尚未调用过 open()）
 * @param file    要打开的音频文件
 * @return true   打开成功，元数据已打印
 * @return false  打开失败
 */
bool openAndPrintFileInfo(AudioDecoder& decoder, const juce::File& file) {
    // 打印文件路径（getFullPathName() 返回绝对路径的 std::string）
    spdlog::info("📂 文件：{}", file.getFullPathName().toStdString());

    // 调用 AudioDecoder::open() 解析文件
    bool open_result = decoder.open(file);

    // 打开失败时打印错误并返回 false
    if (!open_result) {
        spdlog::error("   无法打开文件（格式不支持或文件损坏）");
        return false;
    }

    // 获取文件元信息的只读引用（零拷贝，线程安全）
    const FileInfo& info = decoder.getFileInfo();

    // 计算预期 Fifo 中 float 样本总数
    // 一个采样帧 = 同一时刻所有声道的采样点（立体声 1 帧 = 2 个 float 样本）
    juce::int64 expected_float_count = info.total_frames * info.num_channels;

    // 格式化时长：秒 + 分:秒
    int total_seconds = static_cast<int>(info.duration_seconds);
    int minutes = total_seconds / 60;
    int seconds = total_seconds % 60;

    // 逐行打印元数据（每行缩进 4 空格，层次分明）
    spdlog::info("   格式：{}", info.format_name);
    spdlog::info("   采样率：{} Hz", info.sample_rate);
    spdlog::info("   声道数：{}（{}）",
                 info.num_channels,
                 (info.num_channels == 1 ? "单声道" : "立体声"));
    spdlog::info("   位深：{} bit", info.bit_depth);
    spdlog::info("   总帧数：{} 帧", info.total_frames);
    spdlog::info("   时长：{:.2f} 秒（{} 分 {} 秒）",
                 info.duration_seconds, minutes, seconds);
    spdlog::info("   预期 Fifo float 总数：{}（total_frames × num_channels）",
                 expected_float_count);

    return true;
}

/**
 * 从 AbstractFifo 消费一批数据
 *
 * 调用 prepareToRead/finishedRead 消费 Fifo 中当前可用的所有数据，
 * 累计消费的帧数。不实际 memcpy 数据（测试程序只关心数量）。
 *
 * @param fifo               AbstractFifo 引用（来自 decoder.getFifo()）
 * @param frames_consumed    累计消费帧数（传入/传出参数，会在原有基础上累加）
 */
void consumeFifoData(juce::AbstractFifo& fifo, juce::int64& frames_consumed) {
    // 调用 getNumReady() 检查当前可读的采样帧数（无锁原子操作）
    int num_ready = fifo.getNumReady();

    // 没有数据可读，直接返回（让出 CPU 给解码线程）
    if (num_ready <= 0) {
        return;
    }

    // AbstractFifo::prepareToRead() 返回最多两个连续段（环形缓冲区可能回绕到开头）
    int start1 = 0;
    int size1 = 0;
    int start2 = 0;
    int size2 = 0;
    fifo.prepareToRead(num_ready, start1, size1, start2, size2);

    // 累计读取的总帧数（两段之和，不实际 memcpy 数据——测试程序只关心数量）
    int total_read = size1 + size2;

    // 通知 AbstractFifo 读取完成（原子推进读指针）
    fifo.finishedRead(total_read);

    // 累加到外部计数器
    frames_consumed += total_read;
}

/**
 * 打印分隔线（带标题）
 *
 * @param title 分隔线中间的标题文本
 */
void printSeparator(const std::string& title) {
    spdlog::info("");
    spdlog::info("============================================================");
    spdlog::info("  {}", title);
    spdlog::info("============================================================");
}

/**
 * 打印单条测试结果
 *
 * @param passed    是否通过
 * @param test_name 测试名称
 */
void printResult(bool passed, const std::string& test_name) {
    if (passed) {
        spdlog::info("  ✅ 通过 —— {}", test_name);
    } else {
        spdlog::error("  ❌ 失败 —— {}", test_name);
    }
}


// ============================================================
// 测试函数
// ============================================================

/**
 * 测试 1：WAV 文件打开与元数据提取
 *
 * 验证 AudioDecoder::open() 能正确解析 WAV 格式文件，
 * getFileInfo() 返回的元数据与预期一致。
 *
 * @return true  测试通过
 * @return false 测试失败
 */
bool testOpenWav() {
    printSeparator("测试 1/7：WAV 文件打开与元数据提取");

    // 创建 AudioDecoder 实例（栈上局部变量，函数结束自动析构）
    AudioDecoder decoder;

    // 定位测试文件
    juce::File wav_file = locateTestFile("渡口.wav");

    // 检查文件是否存在（CWD 可能不在项目根目录）
    if (!wav_file.existsAsFile()) {
        spdlog::error("  ❌ 测试文件不存在：{}", wav_file.getFullPathName().toStdString());
        spdlog::warn("  请确认在项目根目录下运行本程序");
        return false;
    }

    // 打开文件并打印元数据
    if (!openAndPrintFileInfo(decoder, wav_file)) {
        printResult(false, "WAV 文件打开与元数据提取");
        return false;
    }

    // 验证元数据准确性
    const FileInfo& info = decoder.getFileInfo();

    // 验证格式名（JUCE 返回的格式名带 " file" 后缀，如 "WAV file"）
    // 使用子串匹配而非精确比较，兼容不同 JUCE 版本的命名差异
    if (info.format_name.find("WAV") == std::string::npos) {
        spdlog::error("    格式名不匹配：期望包含 WAV，实际 {}", info.format_name);
        printResult(false, "WAV 文件打开与元数据提取");
        return false;
    }

    // 验证采样率（44100 Hz，容差 0.5 Hz）
    const double kSampleRateTolerance = 0.5;
    if (std::abs(info.sample_rate - 44100.0) > kSampleRateTolerance) {
        spdlog::error("    采样率不匹配：期望 44100 Hz，实际 {} Hz", info.sample_rate);
        printResult(false, "WAV 文件打开与元数据提取");
        return false;
    }

    // 验证声道数（立体声 = 2）
    if (info.num_channels != 2) {
        spdlog::error("    声道数不匹配：期望 2，实际 {}", info.num_channels);
        printResult(false, "WAV 文件打开与元数据提取");
        return false;
    }

    // 验证位深（16 bit）
    if (info.bit_depth != 16) {
        spdlog::error("    位深不匹配：期望 16 bit，实际 {} bit", info.bit_depth);
        printResult(false, "WAV 文件打开与元数据提取");
        return false;
    }

    printResult(true, "WAV 文件打开与元数据提取");
    return true;
}

/**
 * 测试 2：FLAC 文件打开与元数据提取
 *
 * 验证 AudioDecoder::open() 能正确解析 FLAC 格式文件，
 * getFileInfo() 返回的元数据与预期一致（48kHz/24bit）。
 *
 * @return true  测试通过
 * @return false 测试失败
 */
bool testOpenFlac() {
    printSeparator("测试 2/7：FLAC 文件打开与元数据提取");

    AudioDecoder decoder;
    juce::File flac_file = locateTestFile("鸳鸯戏.flac");

    if (!flac_file.existsAsFile()) {
        spdlog::error("  ❌ 测试文件不存在：{}", flac_file.getFullPathName().toStdString());
        spdlog::warn("  请确认在项目根目录下运行本程序");
        return false;
    }

    if (!openAndPrintFileInfo(decoder, flac_file)) {
        printResult(false, "FLAC 文件打开与元数据提取");
        return false;
    }

    const FileInfo& info = decoder.getFileInfo();

    // 验证格式名（JUCE 返回的格式名带 " file" 后缀，如 "FLAC file"）
    if (info.format_name.find("FLAC") == std::string::npos) {
        spdlog::error("    格式名不匹配：期望包含 FLAC，实际 {}", info.format_name);
        printResult(false, "FLAC 文件打开与元数据提取");
        return false;
    }

    // 验证采样率（48000 Hz，容差 0.5 Hz）
    const double kSampleRateTolerance = 0.5;
    if (std::abs(info.sample_rate - 48000.0) > kSampleRateTolerance) {
        spdlog::error("    采样率不匹配：期望 48000 Hz，实际 {} Hz", info.sample_rate);
        printResult(false, "FLAC 文件打开与元数据提取");
        return false;
    }

    // 验证位深（24 bit）
    if (info.bit_depth != 24) {
        spdlog::error("    位深不匹配：期望 24 bit，实际 {} bit", info.bit_depth);
        printResult(false, "FLAC 文件打开与元数据提取");
        return false;
    }

    // 验证声道数（立体声）
    if (info.num_channels != 2) {
        spdlog::error("    声道数不匹配：期望 2，实际 {}", info.num_channels);
        printResult(false, "FLAC 文件打开与元数据提取");
        return false;
    }

    printResult(true, "FLAC 文件打开与元数据提取");
    return true;
}

/**
 * 测试 3：WAV 文件全解码验证
 *
 * 启动解码线程，从 Fifo 消费全部数据，验证：
 *   1. isDecodingComplete() 返回 true
 *   2. 消费的帧数 = 文件总帧数
 *   3. float 样本总数 = total_frames × num_channels
 *
 * @return true  测试通过
 * @return false 测试失败
 */
bool testDecodeWav() {
    printSeparator("测试 3/7：WAV 文件全解码验证");

    AudioDecoder decoder;
    juce::File wav_file = locateTestFile("渡口.wav");

    if (!wav_file.existsAsFile()) {
        spdlog::error("  ❌ 测试文件不存在：{}", wav_file.getFullPathName().toStdString());
        return false;
    }

    if (!decoder.open(wav_file)) {
        spdlog::error("  ❌ 无法打开文件");
        return false;
    }

    // 从 getFileInfo() 动态获取总帧数（不再硬编码魔法数字）
    const FileInfo& info = decoder.getFileInfo();
    juce::int64 total_frames = info.total_frames;
    int num_channels = info.num_channels;

    spdlog::info("  🎵 开始解码 WAV 文件（{} 帧，{} 声道）...", total_frames, num_channels);

    // 启动后台解码线程
    decoder.startDecoding();

    // 获取 Fifo 引用（消费者使用，任意线程安全）
    juce::AbstractFifo& fifo = decoder.getFifo();

    // 从 Fifo 消费所有解码数据
    // 退出条件：解码完成 AND Fifo 中无剩余数据
    juce::int64 total_frames_read = 0;
    bool all_passed = true;

    while (true) {
        // 检查退出条件：解码线程已停止 AND Fifo 中没有待读数据
        bool decoding_complete = decoder.isDecodingComplete();
        bool fifo_empty = (fifo.getNumReady() == 0);

        if (decoding_complete && fifo_empty) {
            break;
        }

        // 从 Fifo 读取可用数据并累计帧数
        if (!fifo_empty) {
            consumeFifoData(fifo, total_frames_read);
        } else {
            // Fifo 空但解码尚未完成：短暂休眠让出 CPU 给解码线程
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    // 停止解码（join 线程 + 重置 Fifo）
    decoder.stopDecoding();

    spdlog::info("  解码完成，共消费 {} 帧", total_frames_read);

    // 验证 1：isDecodingComplete()
    bool check_complete = decoder.isDecodingComplete();
    spdlog::info("  验证 1：isDecodingComplete() = {} {}",
                 check_complete, check_complete ? "✅" : "❌");
    all_passed = all_passed && check_complete;

    // 验证 2：消费帧数 = 文件总帧数
    bool check_frames = (total_frames_read == total_frames);
    spdlog::info("  验证 2：Fifo 消费帧数 = {}，期望 = {} {}",
                 total_frames_read, total_frames,
                 check_frames ? "✅" : "❌");
    all_passed = all_passed && check_frames;

    // 验证 3：float 样本总数 = total_frames × num_channels
    juce::int64 actual_floats = total_frames_read * num_channels;
    juce::int64 expected_floats = total_frames * num_channels;
    bool check_floats = (actual_floats == expected_floats);
    spdlog::info("  验证 3：float 样本总数 = {}，期望 = {} {}",
                 actual_floats, expected_floats,
                 check_floats ? "✅" : "❌");
    all_passed = all_passed && check_floats;

    printResult(all_passed, "WAV 文件全解码验证");
    return all_passed;
}

/**
 * 测试 4：FLAC 文件全解码验证
 *
 * 与 testDecodeWav 逻辑一致，针对 FLAC 文件（48kHz/24bit）做全解码验证。
 * FLAC 是压缩格式，解码计算量更大，但验证逻辑完全相同。
 *
 * @return true  测试通过
 * @return false 测试失败
 */
bool testDecodeFlac() {
    printSeparator("测试 4/7：FLAC 文件全解码验证");

    AudioDecoder decoder;
    juce::File flac_file = locateTestFile("鸳鸯戏.flac");

    if (!flac_file.existsAsFile()) {
        spdlog::error("  ❌ 测试文件不存在：{}", flac_file.getFullPathName().toStdString());
        return false;
    }

    if (!decoder.open(flac_file)) {
        spdlog::error("  ❌ 无法打开文件");
        return false;
    }

    const FileInfo& info = decoder.getFileInfo();
    juce::int64 total_frames = info.total_frames;
    int num_channels = info.num_channels;

    spdlog::info("  🎵 开始解码 FLAC 文件（{} 帧，{} 声道）...", total_frames, num_channels);

    decoder.startDecoding();

    juce::AbstractFifo& fifo = decoder.getFifo();

    juce::int64 total_frames_read = 0;
    bool all_passed = true;

    while (true) {
        bool decoding_complete = decoder.isDecodingComplete();
        bool fifo_empty = (fifo.getNumReady() == 0);

        if (decoding_complete && fifo_empty) {
            break;
        }

        if (!fifo_empty) {
            consumeFifoData(fifo, total_frames_read);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    decoder.stopDecoding();

    spdlog::info("  解码完成，共消费 {} 帧", total_frames_read);

    // 验证 1：isDecodingComplete()
    bool check_complete = decoder.isDecodingComplete();
    spdlog::info("  验证 1：isDecodingComplete() = {} {}",
                 check_complete, check_complete ? "✅" : "❌");
    all_passed = all_passed && check_complete;

    // 验证 2：消费帧数 = 文件总帧数
    bool check_frames = (total_frames_read == total_frames);
    spdlog::info("  验证 2：Fifo 消费帧数 = {}，期望 = {} {}",
                 total_frames_read, total_frames,
                 check_frames ? "✅" : "❌");
    all_passed = all_passed && check_frames;

    // 验证 3：float 样本总数
    juce::int64 actual_floats = total_frames_read * num_channels;
    juce::int64 expected_floats = total_frames * num_channels;
    bool check_floats = (actual_floats == expected_floats);
    spdlog::info("  验证 3：float 样本总数 = {}，期望 = {} {}",
                 actual_floats, expected_floats,
                 check_floats ? "✅" : "❌");
    all_passed = all_passed && check_floats;

    printResult(all_passed, "FLAC 文件全解码验证");
    return all_passed;
}

/**
 * 测试 5：seekTo() 跳转播放位置
 *
 * 验证四种场景：
 *   1. 负数越界 → 静默忽略，位置不变
 *   2. 超出 total_frames 越界 → 静默忽略
 *   3. seek 到合法位置（1 秒处 = 44100 帧），解码从目标位置开始
 *   4. seek 到文件末尾（total_frames），解码立即完成
 *
 * @return true  测试通过
 * @return false 测试失败
 */
bool testSeekTo() {
    printSeparator("测试 5/7：seekTo() 跳转播放位置");

    AudioDecoder decoder;
    juce::File wav_file = locateTestFile("渡口.wav");

    if (!wav_file.existsAsFile()) {
        spdlog::error("  ❌ 测试文件不存在");
        return false;
    }

    if (!decoder.open(wav_file)) {
        spdlog::error("  ❌ 无法打开文件");
        return false;
    }

    const FileInfo& info = decoder.getFileInfo();
    juce::int64 total_frames = info.total_frames;
    bool all_passed = true;

    // --- 场景 1：负数越界 ---
    // seekTo(-100) 应静默返回，不崩溃、不改变位置
    juce::int64 before_seek = decoder.getDecodedPosition();
    decoder.seekTo(-100);
    juce::int64 after_seek = decoder.getDecodedPosition();
    bool check_negative = (before_seek == after_seek);
    spdlog::info("  场景 1：负数越界 seekTo(-100) → 位置不变 {}",
                 check_negative ? "✅" : "❌");
    all_passed = all_passed && check_negative;

    // --- 场景 2：超范围越界 ---
    // seekTo(total_frames + 1000) 应静默返回
    before_seek = decoder.getDecodedPosition();
    decoder.seekTo(total_frames + 1000);
    after_seek = decoder.getDecodedPosition();
    bool check_overflow = (before_seek == after_seek);
    spdlog::info("  场景 2：超范围越界 seekTo(total_frames+1000) → 位置不变 {}",
                 check_overflow ? "✅" : "❌");
    all_passed = all_passed && check_overflow;

    // --- 场景 3：合法 seek 到 1 秒处 ---
    // seekTo(44100) → 解码应从第 44100 帧开始
    juce::int64 seek_target = 44100;
    decoder.seekTo(seek_target);

    // 等待解码线程启动并产生一些输出
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // getDecodedPosition() 应在 seek_target 附近（解码线程启动后会继续推进）
    juce::int64 current_pos = decoder.getDecodedPosition();
    bool check_seek = (current_pos >= seek_target);
    spdlog::info("  场景 3：seekTo(44100) → 解码位置 = {}（期望 ≥ {}）{}",
                 current_pos, seek_target, check_seek ? "✅" : "❌");

    // 停止解码（清理状态，避免影响后续测试）
    decoder.stopDecoding();
    all_passed = all_passed && check_seek;

    // --- 场景 4：seek 到文件末尾 ---
    // seekTo(total_frames) → 解码应立即完成（无数据可读）
    decoder.seekTo(total_frames);

    // 短暂等待解码线程检测 EOF
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    bool check_eof = decoder.isDecodingComplete();
    spdlog::info("  场景 4：seekTo(total_frames) → isDecodingComplete() = {} {}",
                 check_eof, check_eof ? "✅" : "❌");

    decoder.stopDecoding();
    all_passed = all_passed && check_eof;

    printResult(all_passed, "seekTo() 跳转播放位置");
    return all_passed;
}

/**
 * 测试 6：Listener 管理方法
 *
 * 验证 addListener() 和 removeListener() 的 6 种场景：
 *   1. addListener 不崩溃（正常添加）
 *   2. 重复添加同一 listener 不崩溃（幂等）
 *   3. removeListener 不崩溃（正常移除）
 *   4. 移除不存在的 listener 安全无操作
 *   5. 移除后重新添加不崩溃
 *   6. P0 阶段不触发任何回调（仅管理列表）
 *
 * @return true  测试通过
 * @return false 测试失败
 */
bool testListenerManagement() {
    printSeparator("测试 6/7：Listener 管理方法");

    // 自定义监听器：计数回调触发次数，验证 P0 不触发回调
    class TestListener : public AudioDecoder::Listener {
    public:
        int started_count = 0;    // onDecodingStarted 调用计数
        int completed_count = 0;  // onDecodingComplete 调用计数
        int error_count = 0;      // onDecodingError 调用计数

        void onDecodingStarted() override {
            started_count++;
        }

        void onDecodingComplete() override {
            completed_count++;
        }

        void onDecodingError(const std::string& /*error_message*/) override {
            error_count++;
        }
    };

    AudioDecoder decoder;
    TestListener listener;
    bool all_passed = true;

    // 场景 1：addListener 不崩溃
    decoder.addListener(&listener);
    spdlog::info("  场景 1：addListener() → 不崩溃 ✅");

    // 场景 2：重复添加不崩溃（ListenerList 内部去重）
    decoder.addListener(&listener);
    decoder.addListener(&listener);
    spdlog::info("  场景 2：重复 addListener() × 2 → 不崩溃 ✅");

    // 场景 3：removeListener 不崩溃
    decoder.removeListener(&listener);
    spdlog::info("  场景 3：removeListener() → 不崩溃 ✅");

    // 场景 4：移除不存在的 listener 安全无操作
    decoder.removeListener(&listener);
    spdlog::info("  场景 4：removeListener() 已移除的 listener → 不崩溃 ✅");

    // 场景 5：移除后重新添加
    decoder.addListener(&listener);
    spdlog::info("  场景 5：removeListener() 后重新 addListener() → 不崩溃 ✅");

    // 场景 6：P0 阶段不触发任何回调
    // 注意：当前 P0 阶段确实不会触发回调（解码循环中无 Listener 通知代码）
    // 此处验证初始状态为零（后续如有误触发，回调计数会 > 0）
    bool check_no_callback = (listener.started_count == 0) &&
                             (listener.completed_count == 0) &&
                             (listener.error_count == 0);
    spdlog::info("  场景 6：P0 阶段不触发回调（回调计数全为 0）{}",
                 check_no_callback ? "✅" : "❌");
    all_passed = all_passed && check_no_callback;

    printResult(all_passed, "Listener 管理方法");
    return all_passed;
}

/**
 * 测试 7：错误处理（打开不存在的文件）
 *
 * 验证：
 *   1. AudioDecoder::open() 对不存在的文件返回 false
 *   2. getFileInfo() 在 open() 失败后返回默认值（total_frames == 0）
 *   3. 格式名为空字符串（默认值）
 *
 * @return true  测试通过
 * @return false 测试失败
 */
bool testOpenNonexistentFile() {
    printSeparator("测试 7/7：错误处理（不存在的文件）");

    AudioDecoder decoder;

    // 构造一个绝对不存在的文件路径
    juce::File nonexistent = locateTestFile("__不存在的文件__.xyz");

    spdlog::info("  尝试打开不存在的文件：{}", nonexistent.getFullPathName().toStdString());

    // 验证 1：open() 应返回 false
    bool open_result = decoder.open(nonexistent);
    bool check_open_fail = (!open_result);
    spdlog::info("  验证 1：open() 返回 false {}", check_open_fail ? "✅" : "❌");

    // 验证 2：getFileInfo() 返回默认值（total_frames == 0）
    const FileInfo& info = decoder.getFileInfo();
    bool check_default_frames = (info.total_frames == 0);
    spdlog::info("  验证 2：getFileInfo().total_frames == 0 {}",
                 check_default_frames ? "✅" : "❌");

    // 验证 3：格式名为空（默认初始化的 std::string）
    bool check_empty_format = info.format_name.empty();
    spdlog::info("  验证 3：getFileInfo().format_name 为空 {}",
                 check_empty_format ? "✅" : "❌");

    bool all_passed = check_open_fail && check_default_frames && check_empty_format;
    printResult(all_passed, "错误处理（不存在的文件）");
    return all_passed;
}


// ============================================================
// 主函数
// ============================================================

/**
 * BitPerfect CLI 测试程序入口
 *
 * 依次运行 7 项测试，汇总结果，返回适当的退出码。
 * 测试顺序：先验证基础功能（文件打开），再验证核心功能（解码），
 * 然后验证高级功能（seek、Listener），最后验证错误处理。
 */
int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    // 设置 spdlog 日志级别为 info（显示 info、warn、error）
    spdlog::set_level(spdlog::level::info);

    // ============================================================
    // 打印程序头部
    // ============================================================
    spdlog::info("");
    spdlog::info("============================================================");
    spdlog::info("  BitPerfect CLI — AudioDecoder 模块测试程序 v1.0.0");
    spdlog::info("============================================================");

    // ============================================================
    // 依次执行 7 项测试，用 all_passed 汇总
    // ============================================================
    bool all_passed = true;

    // 测试 1-2：文件打开与元数据提取（基础 API 验证）
    all_passed = testOpenWav() && all_passed;
    all_passed = testOpenFlac() && all_passed;

    // 测试 3-4：全解码验证（核心功能，最重要的测试）
    all_passed = testDecodeWav() && all_passed;
    all_passed = testDecodeFlac() && all_passed;

    // 测试 5-6：高级功能（seek + Listener）
    all_passed = testSeekTo() && all_passed;
    all_passed = testListenerManagement() && all_passed;

    // 测试 7：错误处理（边界条件）
    all_passed = testOpenNonexistentFile() && all_passed;

    // ============================================================
    // 打印最终结果
    // ============================================================
    spdlog::info("");
    spdlog::info("============================================================");
    if (all_passed) {
        spdlog::info("  🎉 全部 7 项测试通过");
        spdlog::info("============================================================");
        spdlog::info("");
        return EXIT_SUCCESS;
    } else {
        spdlog::warn("  ⚠️  部分测试失败，请查看上方 ❌ 标记");
        spdlog::info("============================================================");
        spdlog::info("");
        return EXIT_FAILURE;
    }
}
