/**
 * 文件名：AudioDecoder.cpp
 * 职责：AudioDecoder 类的实现 —— 将 FLAC/WAV 等音频文件解码为 PCM 浮点数据
 * 所属模块：domain/decoder（领域层）
 * 所属线程：open()/startDecoding()/stopDecoding()/seekTo() 由 CLI/UI 线程调用，
 *          decodingLoop() 运行在独立后台线程，
 *          getFifo()/isDecodingComplete()/getDecodedPosition() 可被任意线程安全调用
 *
 * 当前状态（2.2.14）：
 *   - ✅ 全部 12 个方法均已完整实现，无空桩
 *
 * 公开方法（12 个）：
 *   AudioDecoder() / ~AudioDecoder()                          → 构造 + 安全析构
 *   open()                                                    → 文件解析 + 元数据提取 + 缓冲区分配
 *   startDecoding() / stopDecoding()                          → 线程生命周期管理
 *   seekTo()                                                  → 跳转播放位置（含越界检查）
 *   getFifo() / isDecodingComplete() / getDecodedPosition()   → 状态查询（任意线程安全）
 *   getFileInfo()                                             → 文件元信息查询（只读）
 *   addListener() / removeListener()                          → 监听器管理（P0 仅管理列表）
 *
 * 私有方法（1 个）：
 *   decodingLoop()                                            → 解码主循环（后台线程执行，含 EOF 检测 + Fifo 阻塞写入 + 异常安全）
 */

// ============================================================
// 自己的头文件（必须第一个 include，确保头文件自给自足）
// ============================================================
#include "domain/decoder/AudioDecoder.h"

// ============================================================
// 第三方库
// ============================================================
#include <spdlog/spdlog.h>  // 日志输出：spdlog::info()、spdlog::error()

// ============================================================
// C++ 标准库
// ============================================================
#include <cassert>   // assert —— getFifo() 运行时空指针检查（debug 构建中捕获误用）
#include <chrono>    // std::chrono::milliseconds —— 解码线程等待 fifo 空间时的休眠时长
#include <cstring>   // std::memcpy —— 逐声道拷贝 PCM 数据


// ============================================================
// 构造 / 析构
// ============================================================

/**
 * 构造函数 —— 所有成员初始化为安全默认值
 *
 * 为什么函数体为空？
 *   AudioDecoder.h 中已通过 C++11 类内初始化（in-class initializer）
 *   为所有 10 个成员变量赋予了安全默认值，因此构造函数无需再做任何操作：
 *
 *   reader_            = nullptr  → 未打开任何文件，资源在 open() 中分配（2.2.4）
 *   fifo_              = nullptr  → 未分配环形缓冲区，资源在 open() 中分配（2.2.4）
 *   fifo_buffer_       = {}       → 空 AudioBuffer，存储空间在 open() 中分配（2.2.4）
 *   decode_buffer_     = {}       → 空 AudioBuffer，大小在 open() 中分配（2.2.4）
 *   file_info_         = {}       → 默认 FileInfo{44100.0, 2, 16, 0, 0.0, ""}
 *   running_           = false    → 未开始解码
 *   decoding_complete_ = false    → 未完成解码
 *   current_position_  = 0        → 解码位置为起始位置
 *   listeners_         = {}       → 空的监听器列表
 *   decoding_thread_   = {}       → 默认构造的空 std::thread（不可 joinable）
 *
 * 为什么不写成员初始化列表？
 *   编码规范 5.2 节要求"构造函数优先用成员初始化列表"，但这是针对"在
 *   函数体内做简单赋值"的反面约束。由于所有成员已在头文件中通过 = 语法
 *   赋予了默认值（遵循规范 3.6 节），在初始化列表中再写一遍会造成：
 *     1. 信息冗余 —— 两个地方维护同一组默认值
 *     2. 维护风险 —— 修改头文件默认值时容易忘记同步更新 .cpp
 *   因此采用空函数体 + 本注释的方式，让编译器使用类内默认值。
 *
 * 线程约束：无（构造阶段对象尚未被多线程访问）
 */
AudioDecoder::AudioDecoder() {
    // 所有成员已在头文件中通过类内初始化赋予安全默认值
    // 详见上方注释中的逐项说明
}

/**
 * 析构函数 —— 确保解码线程已终止后释放资源
 *
 * 执行流程：
 *   1. 调用 stopDecoding() 设置退出标志并 join 解码线程
 *      - stopDecoding() 已完整实现，此处安全等待线程退出并重置缓冲区
 *   2. 析构函数体执行完毕后，C++ 自动按声明逆序析构所有成员变量：
 *      - decoding_thread_：如果线程仍可 join（异常情况），std::thread 析构会调用 std::terminate()
 *                          因此必须先调用 stopDecoding() 确保线程已 join
 *      - listeners_：ListenerList 析构，自动清理监听器列表
 *      - 原子变量（running_/decoding_complete_/current_position_）：无需特殊清理
 *      - file_info_：FileInfo 析构（string 自动释放内存）
 *      - decode_buffer_：AudioBuffer 析构（自动释放内部浮点数组）
 *      - fifo_buffer_：AudioBuffer 析构（自动释放内部浮点数组）
 *      - fifo_：unique_ptr 析构（自动 delete AbstractFifo 对象）
 *      - reader_：unique_ptr 析构（自动 delete AudioFormatReader，关闭文件句柄）
 *
 * 线程约束：可在任意非音频回调线程调用（通常由对象所有者在线程退出时调用）
 */
AudioDecoder::~AudioDecoder() {
    // 确保解码线程已退出再析构成员变量
    // stopDecoding() 已完整实现，此处安全等待解码线程退出
    stopDecoding();
}


// ============================================================
// ============================================================
// 公开方法（以下所有方法均已完整实现）
// ============================================================

/**
 * 打开音频文件并提取元数据
 *
 * 当前状态（2.2.4）：✅ 完整实现（2026-07-09 修复 repeat-open 悬垂指针 bug）
 *
 * 执行流程：
 *   0. 释放旧 reader_（防止 repeat-open 时旧 reader_ 持有已销毁 AudioFormat 的悬垂指针）
 *   1. 创建 AudioFormatManager 并注册基本格式（WAV、FLAC、AIFF 等）
 *   2. 通过 format_manager_ 创建 AudioFormatReader
 *   3. 提取元数据（采样率、声道数、位深、总采样数、时长）
 *   4. 动态分配 AbstractFifo（容量 = 采样率 × 声道数 × 0.5 秒）
 *   5. 分配 fifo_buffer_ 和 decode_buffer_
 *
 * @param file 要打开的音频文件（juce::File 对象）
 * @return true  文件成功打开，元数据已填充，缓冲区已分配
 * @return false 格式不支持或文件损坏
 */
bool AudioDecoder::open(const juce::File& file) {
    // ============================================================
    // 步骤 -1：停止旧的解码线程（防止 repeat-open 数据竞争）
    // ============================================================
    // 如果旧文件正在解码中（running_ == true），直接释放 reader_ 会导致
    // decodingLoop() 中的 reader_->read() 访问已释放的内存（use-after-free）。
    //
    // 解决：先调用 stopDecoding() 安全停止解码线程（内部包含：
    //   running_.store(false) → join() 等待线程退出 → fifo_->reset()）
    // 然后再安全释放 reader_。
    //
    // 首次调用时 running_ 为 false、decoding_thread_ 不可 joinable，
    // stopDecoding() 是幂等的，不会有副作用。
    stopDecoding();

    // ============================================================
    // 步骤 0：释放旧 reader_（防止 repeat-open 悬垂指针 bug）
    // ============================================================
    // 背景：reader_ 内部持有指向 AudioFormat 对象的裸指针（JUCE 设计），
    //       这些 AudioFormat 对象由 format_manager_ 统一管理生命周期。
    //
    // 问题：如果先销毁 format_manager_（步骤 1 的赋值操作会触发），
    //       AudioFormat 对象也随之销毁，但 reader_ 此时还活着，
    //       其内部的 AudioFormat 裸指针变成悬垂指针。
    //
    // 解决：在创建新 format_manager_ 之前先释放旧 reader_。
    //       此时旧 format_manager_ 仍然存活，旧 reader_ 析构时
    //       访问的 AudioFormat 指针仍然有效，不会触发 use-after-free。
    //
    // 首次调用时 reader_ 为 nullptr，reset(nullptr) 是安全的空操作。
    reader_.reset();

    // ============================================================
    // 步骤 0b：重置所有可变状态为默认值（与 reader_.reset() 对称）
    // ============================================================
    // 背景：reader_ 已释放，旧文件的解码上下文不再有效。为防止后续步骤
    //   （如 createReaderFor）失败导致对象处于"reader_ 为空但 file_info_
    //   保留旧数据"的不一致状态，在此处将所有与旧文件关联的状态统一重置。
    //
    // 如果 open() 最终成功，这些字段将在步骤 5-9b 中被新文件的数据覆盖；
    // 如果 open() 中途失败，调用方通过 getFileInfo() 等查询方法看到的是
    // 干净的默认值（而非旧文件的幽灵数据）。
    //
    // 重置项说明：
    //   - file_info_：元数据（采样率/声道/位深/总帧数/时长/格式名）归零
    //   - current_position_：解码位置归零（新文件从头开始）
    //   - decoding_complete_：标记为未完成（新文件尚未开始解码）
    //   - running_ 已由 stopDecoding() 设为 false，无需重复操作
    file_info_ = FileInfo{};                   // 重置为默认值（44100, 2, 16, 0, 0.0, ""）
    current_position_.store(0);               // 解码位置归零
    decoding_complete_.store(false);          // 标记为未完成

    // ============================================================
    // 步骤 1：创建 AudioFormatManager 并注册基本音频格式
    // ============================================================
    // std::make_unique 遵循编码规范（禁止裸 new）
    // registerBasicFormats() 会注册 WAV、FLAC、AIFF、OGG、MP3 等 JUCE 内置支持的格式
    // 注意：此赋值会先销毁旧的 format_manager_（以及其中注册的 AudioFormat 对象），
    //       但步骤 0 已确保旧 reader_ 在此之前被释放，因此不会有悬垂指针问题
    format_manager_ = std::make_unique<juce::AudioFormatManager>();
    format_manager_->registerBasicFormats();

    // ============================================================
    // 步骤 2：通过 AudioFormatManager 创建 AudioFormatReader
    // ============================================================
    // createReaderFor() 根据文件扩展名和内部魔数自动选择正确的解码器
    // 返回值是裸指针（JUCE API 设计），调用方负责释放
    juce::AudioFormatReader* raw_reader = format_manager_->createReaderFor(file);

    // ============================================================
    // 步骤 3：创建失败处理
    // ============================================================
    // 创建失败的可能原因：
    //   - 文件不存在或无读取权限
    //   - 文件格式不在已注册列表中
    //   - 文件损坏，解码器无法解析文件头
    if (raw_reader == nullptr) {
        // spdlog::error 输出错误日志到控制台
        // getFullPathName() 返回文件的绝对路径，toStdString() 转为 std::string
        spdlog::error("无法打开音频文件：{}", file.getFullPathName().toStdString());
        return false;
    }

    // ============================================================
    // 步骤 4：将裸指针所有权转移到 unique_ptr
    // ============================================================
    // reader_.reset(raw_reader) 的含义：
    //   - 如果 reader_ 之前已持有其他 reader（重复打开场景），先 delete 旧的
    //   - 然后将 raw_reader 的所有权交给 reader_
    //   - 此后 reader_ 的生命周期管理由 unique_ptr 的 RAII 机制保证
    reader_.reset(raw_reader);

    // ============================================================
    // 步骤 5：从 reader_ 提取音频元数据，填充 file_info_
    // ============================================================
    // reader_ 的成员变量是 JUCE 在创建 reader 时通过解析文件头自动填充的
    // static_cast<int> 遵循编码规范（禁止 C 风格转换）
    file_info_.sample_rate = reader_->sampleRate;                              // 采样率（Hz）
    file_info_.num_channels = static_cast<int>(reader_->numChannels);          // 声道数
    file_info_.bit_depth = static_cast<int>(reader_->bitsPerSample);           // 位深（bit）
    file_info_.total_frames = reader_->lengthInSamples;                       // 总采样帧数
    file_info_.format_name = reader_->getFormatName().toStdString();           // 格式名（"FLAC"/"WAV"等）

    // ============================================================
    // 步骤 6：计算时长
    // ============================================================
    // duration_seconds = 总采样帧数 / 采样率
    // 使用 static_cast<double> 确保浮点除法（避免整数截断）
    file_info_.duration_seconds = static_cast<double>(file_info_.total_frames) / file_info_.sample_rate;

    // ============================================================
    // 步骤 7：动态计算 AbstractFifo 容量
    // ============================================================
    // 容量公式：采样率 × 0.5（秒）= 每个声道的采样点数
    // AbstractFifo 管理单个声道的读写进度（所有声道同步，同一索引），
    // fifo_buffer_（AudioBuffer<float>）提供 num_channels 个声道，
    // 每个声道存储 fifo_capacity 个采样点。
    //
    // 总内存：num_channels × fifo_capacity × sizeof(float)
    // 举例：44.1kHz 立体声 → 每声道 44100 × 0.5 = 22050 采样点，
    //       总内存 = 2 × 22050 × 4 = 172 KB（与旧方案完全一致）
    //       192kHz 立体声 → 每声道 192000 × 0.5 = 96000 采样点，
    //       总内存 = 2 × 96000 × 4 = 750 KB
    int fifo_capacity = static_cast<int>(
        file_info_.sample_rate * 0.5);

    // ============================================================
    // 步骤 8：分配无锁环形缓冲区
    // ============================================================
    // AbstractFifo 只管理读写指针（原子变量），不持有实际数据
    // 容量为单声道采样点数，索引表示"第几个采样帧"
    fifo_ = std::make_unique<juce::AbstractFifo>(fifo_capacity);

    // fifo_buffer_ 分配 num_channels 个声道，每声道 fifo_capacity 个采样帧
    // 与 decode_buffer_ 类型一致（均为 AudioBuffer<float>，平面存储），
    // 解码循环中从 decode_buffer_ 拷贝到 fifo_buffer_ 时无需格式转换
    fifo_buffer_.setSize(file_info_.num_channels, fifo_capacity);

    // ============================================================
    // 步骤 9：分配单帧解码临时缓冲
    // ============================================================
    // decode_buffer_ 是 juce::AudioBuffer<float> 类型，用于接收 reader_->read() 的解码输出
    // setSize(num_channels, num_samples) 分配 num_channels 个声道，
    // 每个声道 kFramesPerChunk 个采样帧的存储空间（kFramesPerChunk 是经验值，兼顾内存开销和解码效率）
    //
    // AudioBuffer<float> 内部使用独立的 float 数组存储每个声道的数据（非交错存储），
    // 与 fifo_buffer_ 类型一致，拷贝时直接逐声道 memcpy，无需平面↔交错格式转换
    decode_buffer_.setSize(file_info_.num_channels, kFramesPerChunk);

    // ============================================================
    // 步骤 9b：重置解码状态（新文件 = 全新的解码会话）
    // ============================================================
    // 打开新文件意味着上一轮解码的状态已无意义，必须重置：
    //   - current_position_：旧文件可能解码到任意位置（如 9,878,988），
    //     新文件必须从位置 0 开始解码
    //   - decoding_complete_：旧文件可能已播放完毕（true），
    //     新文件尚未开始解码，应重置为 false
    //
    // 注意：running_ 已在步骤 -1 的 stopDecoding() 中设为 false，
    //       decoding_thread_ 已在 join() 中回收，无需在此处理
    current_position_.store(0);
    decoding_complete_.store(false);

    // ============================================================
    // 步骤 10：输出成功日志
    // ============================================================
    // spdlog::info 输出信息日志，便于开发调试和验证 open() 是否正确工作
    // {:.0f} 格式化：采样率不需要小数位
    // {:.2f} 格式化：时长保留两位小数
    spdlog::info("音频文件已打开：{}", file_info_.format_name);
    spdlog::info("  采样率：{:.0f} Hz，声道：{}，位深：{} bit",
                 file_info_.sample_rate, file_info_.num_channels, file_info_.bit_depth);
    spdlog::info("  总采样数：{}，时长：{:.2f} 秒",
                 file_info_.total_frames, file_info_.duration_seconds);

    return true;
}

/**
 * 启动后台解码线程
 *
 * 当前状态（2.2.5）：✅ 完整实现
 *
 * 执行流程（6 个步骤）：
 *   1. 前置检查：reader_ 是否已创建（open() 是否调用成功），未就绪则打印错误并返回
 *   2. 重复调用检查：running_ 是否已为 true，若已在运行则幂等忽略（防止创建多个线程竞争）
 *   3. 旧线程回收：decoding_thread_ 是否 joinable，若是则先 join（防止移动赋值触发 std::terminate）
 *   4. 状态初始化：running_ = true、decoding_complete_ = false、current_position_ = 0
 *   5. 创建线程：std::thread 执行 decodingLoop()，保存到 decoding_thread_
 *   6. 成功日志：输出采样率、声道数、位深等信息
 *
 * 线程约束：必须在 open() 成功后由 CLI/UI 线程调用。
 *         重复调用安全（幂等操作，已运行时忽略）。
 */
void AudioDecoder::startDecoding() {
    // ============================================================
    // 步骤 1：前置条件检查 —— reader_ 是否已创建（open() 是否调用成功）
    // ============================================================
    // reader_ 在 open() 中通过 format_manager_->createReaderFor() 创建
    // 若 open() 尚未调用或调用失败，reader_ 保持 nullptr（头文件类内初始化的默认值）
    // 此时没有文件可以解码，必须拒绝调用
    if (reader_ == nullptr) {
        // spdlog::error 输出错误日志到控制台
        // 提示调用者：必须先成功调用 open() 才能启动解码线程
        spdlog::error("startDecoding() 调用失败：reader_ 未创建，请先调用 open() 打开音频文件");
        return;
    }

    // ============================================================
    // 步骤 2：重复调用检查 —— 解码线程是否已在运行
    // ============================================================
    // running_ 是原子变量（std::atomic<bool>），由解码线程在 while(running_) 中读取
    // 若已为 true，说明解码循环正在另一个线程中执行
    // 此时如果创建第二个线程，会导致三个严重问题：
    //   1. 两个线程同时调用 reader_->read()，reader_ 不是线程安全的 → 数据错乱
    //   2. 两个线程同时写入 AbstractFifo → 数据相互覆盖
    //   3. decoding_thread_ = std::thread(...) 移动赋值会先析构旧线程 → std::terminate() 崩溃
    //
    // 因此必须幂等返回，静默忽略本次重复调用
    // load() 使用默认 memory_order_seq_cst，与头文件文档中的"任意线程安全"语义一致
    if (running_.load()) {
        // spdlog::warn 输出警告日志（非 error，因为系统仍正常运行，只是调用方做了多余调用）
        spdlog::warn("startDecoding() 被重复调用，解码线程已在运行中，忽略本次调用");
        return;
    }

    // ============================================================
    // 步骤 3：旧线程回收 —— 检测并 join 上一个已结束但未回收的线程
    // ============================================================
    // 在以下场景中可能出现 joinable 但 running_ 为 false 的线程：
    //   - decodingLoop() 内 while(running_) 循环退出，线程函数返回（线程结束）
    //   - 但 decoding_thread_ 对象尚未被 join，处于 joinable 状态
    //   - 此时 running_ 为 false（通过了步骤 2 的检查），但线程尚未回收
    //
    // 如果不先 join 就直接执行 decoding_thread_ = std::thread(...)：
    //   - std::thread 的移动赋值运算符会先析构左侧的旧线程对象
    //   - 若旧线程处于 joinable 状态，std::thread 析构函数调用 std::terminate()
    //   - 导致整个进程崩溃（C++ 标准规定，joinable 的 std::thread 析构时调用 std::terminate）
    //
    // 因此必须在此处检测并安全回收旧线程资源
    if (decoding_thread_.joinable()) {
        // spdlog::info 输出信息日志，记录这个正常的资源回收操作
        spdlog::info("startDecoding()：检测到上一个解码线程未 join，先 join 再创建新线程");
        // join() 会阻塞当前线程（CLI/UI 线程）直到旧解码线程完全结束
        // 由于旧线程函数已经返回（joinable 意味着线程已结束），此处的 join() 会立即返回
        // join() 之后，decoding_thread_ 变为不可 joinable 状态，安全
        decoding_thread_.join();
    }

    // ============================================================
    // 步骤 4：初始化运行状态
    // ============================================================
    // ⚠️ 顺序至关重要：running_ 必须在创建线程之前设为 true
    // 如果先创建线程再设 running_ = true，可能出现竞态：
    //   新线程启动 → 读取 running_（此时还是 false）→ while(running_) 循环立即退出 → 线程结束
    //   结果：解码线程"一闪而过"，什么都没做就退出了
    //
    // store() 使用默认 memory_order_seq_cst（顺序一致性），
    // 与 std::thread 创建的 happens-before 关系共同保证新线程能看到这个值
    running_.store(true);

    // decoding_complete_ 重置为 false，表示解码尚未完成
    // 上一轮解码可能已到达文件末尾（decoding_complete_ == true），必须重置
    decoding_complete_.store(false);

    // current_position_ 保持当前值不变（不在此处重置）：
    //   - 首次调用（open() 后）：构造时初始化为 0 → decodingLoop() 从头开始解码 ✅
    //   - seekTo() → startDecoding()：seekTo() 已写入目标位置 → 从指定位置开始解码 ✅
    //   - stopDecoding() → startDecoding()：保持中断时的位置 → 续播
    //     （如需从头开始，调用方应先行调用 seekTo(0)）
    //
    // ⚠️ 原设计在此处执行 current_position_.store(0) 无条件重置，
    //    但这会导致 seekTo() 设置的目标位置被覆盖。改为依赖构造函数默认值，
    //    由不同调用路径自行管理 current_position_ 的值。

    // ============================================================
    // 步骤 5：创建解码线程
    // ============================================================
    // std::thread 构造参数说明：
    //   第一个参数 &AudioDecoder::decodingLoop —— 成员函数指针（C++ 标准语法）
    //   第二个参数 this                        —— 传给成员函数的隐式 this 指针
    //
    // 线程创建后立即开始执行 decodingLoop()，与当前线程（CLI/UI 线程）并发运行
    // 线程对象通过移动赋值（=）保存到 decoding_thread_ 成员变量中
    // 移动赋值后，右侧的临时 std::thread 对象变为"空"状态（不可 joinable），析构安全
    //
    // 线程创建后会立即开始执行 decodingLoop()，在独立线程中解码音频数据
    decoding_thread_ = std::thread(&AudioDecoder::decodingLoop, this);

    // ============================================================
    // 步骤 6：输出启动成功日志
    // ============================================================
    // spdlog::info 输出信息日志，包含当前文件的音频参数
    // 这些参数来自 file_info_（open() 中填充），在此处为只读访问（无竞态问题）
    // 日志格式与 open() 中的成功日志保持一致（见本文件第 297-301 行）
    // {} 格式符用于 sample_rate（double 类型，spdlog 会格式化为 44100 而非 44100.0）
    spdlog::info("startDecoding()：解码线程已启动（{} Hz / {} 声道 / {} bit）",
                 file_info_.sample_rate, file_info_.num_channels, file_info_.bit_depth);
}

/**
 * 停止解码并等待线程退出
 *
 * 当前状态（2.2.6）：✅ 完整实现
 *
 * 执行流程（3 个步骤）：
 *   1. 设置 running_ = false，通知 decodingLoop() 退出 while 循环
 *   2. 若 decoding_thread_ 可 joinable，调用 join() 阻塞等待线程完全退出
 *   3. 若 fifo_ 已创建，调用 reset() 重置读写指针，清空缓冲区中未消费的数据
 *
 * 线程安全：
 *   - 可在任意（非音频回调）线程调用
 *   - 通过 std::atomic<bool> 与解码线程通信，无锁操作
 *   - 幂等安全：多次调用安全无副作用（join 后的线程 joinable() 返回 false，自动跳过）
 *
 * 生命周期：
 *   - 析构函数会调用本方法，确保 std::thread 在 joinable 状态下不会被析构（避免 std::terminate）
 *   - 用户也可主动调用以提前停止解码
 *
 * ⚠️ 时序要求：join() 必须在 fifo_->reset() 之前执行，
 *   防止解码线程还在写 fifo 时 reset 造成数据竞争。
 */
void AudioDecoder::stopDecoding() {
    // ============================================================
    // 步骤 1：设置退出标志，通知解码线程停止
    // ============================================================
    // running_ 是 std::atomic<bool>，解码线程在 decodingLoop() 的 while(running_) 循环中读取
    // 此处设为 false 后，解码线程会在下一次循环条件检查时退出
    // store() 使用默认 memory_order_seq_cst（顺序一致性），确保解码线程能看到最新值
    // 即使 running_ 已经是 false（startDecoding() 从未调用，或 stopDecoding() 被重复调用），
    // 重复 store 也无副作用，保证了幂等安全性
    running_.store(false);

    // ============================================================
    // 步骤 2：等待解码线程完全退出
    // ============================================================
    // joinable() 检查确保只在线程存在且尚未被 join 时才执行 join
    // 以下场景会通过此检查：
    //   a. startDecoding() 创建了线程，且线程尚未被 join → 执行 join 等待
    //   b. startDecoding() 从未调用 → decoding_thread_ 保持默认构造状态，不可 joinable → 跳过
    //   c. stopDecoding() 已被调用过一次 → 上次的 join 使线程变为不可 joinable → 跳过（幂等）
    //
    // join() 会阻塞当前线程（CLI/UI 线程）直到解码线程的 decodingLoop() 完全退出
    // 如果解码线程正在 reader_->read() 中阻塞（等待磁盘 I/O），
    // join() 会一直等待直到 read() 返回、while 循环退出、线程函数返回为止
    //
    // ⚠️ join() 必须在 fifo_->reset() 之前执行：
    //   join 前解码线程可能还在向 fifo 写入数据，此时 reset 会产生数据竞争
    //   join 后解码线程已完全停止，reset 是安全的
    if (decoding_thread_.joinable()) {
        // 阻塞等待解码线程完全退出
        decoding_thread_.join();

        // 输出停止成功日志（与 startDecoding() 的启动日志对称）
        spdlog::info("stopDecoding()：解码线程已停止");
    }

    // ============================================================
    // 步骤 3：重置无锁环形缓冲区读写指针
    // ============================================================
    // AbstractFifo 只管理读写指针，不管理底层数据缓冲区
    // reset() 将读写指针归零，清空缓冲区中所有未消费的解码数据
    //
    // 判空检查：
    //   - 如果 open() 从未调用，fifo_ 为 nullptr（头文件类内初始化的默认值）
    //   - 此时跳过 reset，避免空指针解引用
    //
    // 注意：此处不释放 fifo_buffer_（AudioBuffer）和 decode_buffer_（解码临时缓冲），
    // 它们由 open() 在下一次调用时重新分配，或由析构函数自动回收（RAII）
    if (fifo_ != nullptr) {
        fifo_->reset();
    }
}

/**
 * 跳转到指定采样位置
 *
 * @param sample_position 目标采样帧位置（0-based，从文件开头算起）
 */
void AudioDecoder::seekTo(juce::int64 sample_position) {
    // ============================================================
    // 步骤 1：验证 sample_position 在 [0, total_frames] 范围内
    // ============================================================
    // total_frames 在 open() 中从 reader_->lengthInSamples 提取并存入 file_info_
    // 越界时静默返回，不输出日志（由调用方决定是否报错）
    // 允许 sample_position == total_frames：表示 seek 到文件末尾，
    //   decodingLoop 启动后会立即检测 EOF 并退出（不产生解码数据）
    if (sample_position < 0 || sample_position > file_info_.total_frames) {
        return;
    }

    // ============================================================
    // 步骤 2：调用 stopDecoding() 暂停当前解码
    // ============================================================
    // stopDecoding() 内部依次执行（详见第 450-500 行）：
    //   a. running_.store(false) —— 通知解码线程退出 while 循环
    //   b. join() —— 阻塞等待解码线程完全退出（防止数据竞争）
    //   c. fifo_->reset() —— 重置环形缓冲区读写指针
    // 因此步骤 3（重置 AbstractFifo）已被 stopDecoding() 内部覆盖，无需重复操作
    stopDecoding();

    // ============================================================
    // 步骤 3：将目标位置写入 current_position_ 原子变量
    // ============================================================
    // 这是 seek 机制的核心：JUCE AudioFormatReader 基类没有 setPosition() 方法，
    // seek 通过以下链条实现：
    //   seekTo() → store 到 current_position_
    //   → startDecoding() 创建新线程 → decodingLoop() 从 current_position_ load 起始位置
    //   → 作为 readerStartSample 参数传给 reader_->read()
    //
    // 写入时机说明：必须在 stopDecoding() 之后（线程已退出）、startDecoding() 之前
    // 此时没有其他线程访问 current_position_，无数据竞争
    current_position_.store(sample_position);

    // ============================================================
    // 步骤 4：调用 startDecoding() 从新位置恢复解码
    // ============================================================
    // startDecoding() 会创建新的解码线程执行 decodingLoop()
    // decodingLoop 从 current_position_（即 sample_position）开始读取
    startDecoding();
}

/**
 * 获取 AbstractFifo 引用 —— 供音频线程无锁读取 PCM 数据
 *
 * @return AbstractFifo 的引用
 *
 * 线程约束：任意线程安全。但必须在 open() 之后调用（否则 fifo_ 为 nullptr，行为未定义）。
 */
juce::AbstractFifo& AudioDecoder::getFifo() {
    // 运行时断言：确保 open() 已被成功调用（fifo_ 不为空）
    // assert 仅在 debug 构建中生效，release 构建中被编译器移除（零性能开销）
    // 这能帮助开发者在 debug 阶段尽早发现"解码前忘记 open()"的误用
    assert(fifo_ != nullptr);

    // 直接返回 fifo_ 指向的 AbstractFifo 对象的引用
    // AbstractFifo 使用原子变量管理读写指针，任意线程安全访问
    // 音频实时线程可以安全调用 prepareToRead() / finishedRead() 无锁读取 PCM 数据
    return *fifo_;
}

/**
 * 查询解码是否已全部完成
 *
 * @return true 表示解码已全部完成（EOF 已到达），false 表示仍在解码中
 */
bool AudioDecoder::isDecodingComplete() const {
    // 原子变量 load 操作，任意线程安全读取解码完成状态
    return decoding_complete_.load();
}

/**
 * 查询当前解码位置
 *
 * @return 已解码到的采样帧位置（0-based，从文件开头算起）
 */
juce::int64 AudioDecoder::getDecodedPosition() const {
    // 原子变量 load 操作，任意线程安全读取当前解码位置
    return current_position_.load();
}

const FileInfo& AudioDecoder::getFileInfo() const {
    // 返回 file_info_ 的常量引用（只读），避免 6 个字段的拷贝开销
    // file_info_ 仅在 open() 中写入（调用方负责在 startDecoding() 之前调用 open()），
    // 之后所有线程只读访问，无需加锁
    return file_info_;
}

/**
 * 注册解码事件监听器
 *
 * P0 阶段说明：addListener/removeListener 仅管理列表，
 * 不会向 listener 发送任何通知。P1 阶段通过 MessageManager::callAsync 实现跨线程通知。
 *
 * @param listener 要注册的监听器指针（AudioDecoder 不持有所有权）
 */
void AudioDecoder::addListener(Listener* listener) {
    // 将 listener 加入监听器列表
    // juce::ListenerList 自动处理重复添加：同一 listener 只保留一份（内部使用链表 + 指针比较）
    listeners_.add(listener);
}

/**
 * 移除解码事件监听器
 *
 * @param listener 要移除的监听器指针
 */
void AudioDecoder::removeListener(Listener* listener) {
    // 将 listener 从监听器列表中移除
    // juce::ListenerList 自动处理移除不存在的 listener：内部检查后安全无操作
    listeners_.remove(listener);
}


// ============================================================
// ============================================================
// 私有方法（以下方法均已完整实现）
// ============================================================

/**
 * 解码线程主循环
 *
 * 将音频文件逐帧解码为 PCM 浮点数据，写入 AbstractFifo 环形缓冲区，
 * 供音频引擎（或测试程序的消费者线程）无锁读取。循环受 running_ 原子变量控制，
 * 可通过 stopDecoding() 安全终止。
 *
 * 线程约束：仅由 decoding_thread_ 执行（2.2.5 创建线程后），不直接在其他线程调用
 */
void AudioDecoder::decodingLoop() {
    // ============================================================
    // 步骤 1：获取音频文件参数（只读常量，线程安全）
    // ============================================================
    // 将成员变量缓存为局部常量，避免每次循环都通过 this 指针间接访问
    //（编译器的优化器通常会自动做这件事，但显式写出更清晰）
    int num_channels = file_info_.num_channels;               // 声道数（1 = 单声道，2 = 立体声）
    juce::int64 total_frames = file_info_.total_frames;     // 文件总采样帧数

    // 每次解码的最大采样帧数，引用类静态常量 kFramesPerChunk（在 AudioDecoder.h 中定义）

    // 当前读取位置（从文件开头算起，0-based）
    // ⚠️ 使用局部变量手动追踪位置，而非 reader_->getPosition()
    //    原因：AudioFormatReader 基类没有 getPosition() 方法（JUCE 8.0.14 确认）
    //    每次成功读取 frames_to_read 帧后递增，精确追踪已读取的采样帧数
    //
    // 起始值从 current_position_ 原子变量读取：
    //   - 正常启动（startDecoding()）：构造函数初始化为 0 → 从头解码
    //   - seek 后启动（seekTo() → startDecoding()）：seekTo 写入的目标位置 → 从指定处解码
    juce::int64 read_position = current_position_.load();

    // ============================================================
    // 步骤 2：解码主循环（受 running_ 原子变量控制）
    // ============================================================
    // running_ 由 startDecoding() 设为 true，stopDecoding() 设为 false
    // 解码线程在每次循环迭代开始时检查，确保能及时响应停止请求
    try {
        while (running_.load()) {
            // ----------------------------------------------------
            // 2a. 检查是否已到达文件末尾（EOF 检测）
            // ----------------------------------------------------
            // ⚠️ 注意：reader_->read() 的返回值是 bool，读到文件尾时只是补零并返回 true，
            //    无法通过返回值判断是否到达 EOF。因此使用手动位置追踪替代原方案中的
            //    "若返回值 <= 0 则退出" 逻辑。
            if (read_position >= total_frames) {
                // 设置解码完成标志（原子写入，任意线程通过 isDecodingComplete() 可见）
                decoding_complete_.store(true);
                spdlog::info("解码完成：已到达文件末尾（共 {} 采样帧）", total_frames);
                break;
            }

            // ----------------------------------------------------
            // 2b. 计算本次要读取的采样帧数
            // ----------------------------------------------------
            // 文件最后一帧可能不足 kFramesPerChunk 帧，需要动态调整读取数量
            // 使用 juce::int64 做减法避免整数溢出（total_frames 可达 2^63）
            juce::int64 remaining = total_frames - read_position;
            int frames_to_read = static_cast<int>(
                (remaining < kFramesPerChunk) ? remaining : kFramesPerChunk);

            // ----------------------------------------------------
            // 2c. 从文件解码一段音频数据到 decode_buffer_
            // ----------------------------------------------------
            // read() 参数说明（JUCE AudioFormatReader，6 个参数）：
            //   &decode_buffer_  —— 目标 AudioBuffer<float>，在 open() 中已分配 kFramesPerChunk 帧
            //   0                —— 目标 buffer 中的起始写入偏移（从头开始写）
            //   frames_to_read   —— 本次要读取的采样帧数
            //   read_position    —— 文件中起始读取位置（0-based 绝对位置）
            //   true             —— useReaderLeftChan（使用文件的左声道）
            //   true             —— useReaderRightChan（使用文件的右声道）
            // ⚠️ 注意：原 TODO 注释中只有 5 个参数，缺少第六个参数 useReaderRightChan，
            //    JUCE 8.0.14 实际 API 需要 6 个参数，此处已修正。
            bool read_success = reader_->read(
                &decode_buffer_,          // 目标 AudioBuffer
                0,                         // 目标起始偏移
                frames_to_read,            // 读取帧数
                read_position,             // 文件中起始位置
                true,                      // 使用左声道
                true                       // 使用右声道
            );

            // 读取失败处理（磁盘错误、文件损坏等，非 EOF 情况）
            if (!read_success) {
                spdlog::error("解码错误：reader_->read() 在位置 {} 处返回 false", read_position);
                running_.store(false);
                break;
            }

            // ----------------------------------------------------
            // 2d. 将 decode_buffer_ 中的数据写入 fifo_buffer_
            // ----------------------------------------------------
            // 数据流：decode_buffer_（临时缓冲）→ fifo_buffer_（环形缓冲区）
            // 两个缓冲区都是 juce::AudioBuffer<float>，内部均为平面存储（planar），
            // 因此可以直接逐声道 memcpy，无需做平面 ↔ 交错格式转换
            {
                // --- 2d-i. 获取环形缓冲区中的可写入位置 ---
                // prepareToWrite 返回最多两个连续段（因为环形缓冲区可能回绕到开头）
                // start1/size1：第一段连续区域的起始位置和大小
                // start2/size2：第二段连续区域的起始位置和大小（回绕部分，可能为 0）
                int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
                fifo_->prepareToWrite(frames_to_read, start1, size1, start2, size2);
                int total_writable = size1 + size2;

                // --- 2d-ii. 若 fifo 已满，等待消费者（音频引擎）读取释放空间 ---
                // 解码线程不是实时线程，可以阻塞等待（使用 sleep 而非忙等，避免 CPU 空转）
                // 等待期间持续检查 running_，确保 stopDecoding() 可以及时中断等待
                while (running_.load() && total_writable < frames_to_read) {
                    // 休眠 5 毫秒，让出 CPU 给消费者线程
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    // 重新尝试获取写入位置
                    fifo_->prepareToWrite(frames_to_read, start1, size1, start2, size2);
                    total_writable = size1 + size2;
                }

                // 如果在等待期间 stopDecoding() 被调用（running_ 变为 false），退出循环
                if (!running_.load()) {
                    break;
                }

                // --- 2d-iii. 逐声道拷贝第一段数据（start1 开始，size1 帧）---
                // 两个 AudioBuffer 均为平面存储（planar），每个声道是独立的 float 数组
                // getReadPointer(ch) 返回解码数据的声道 ch 起始地址（只读）
                // getWritePointer(ch) 返回 fifo 缓冲区的声道 ch 起始地址（可写）
                if (size1 > 0) {
                    for (int ch = 0; ch < num_channels; ++ch) {
                        const float* src = decode_buffer_.getReadPointer(ch);
                        float* dst = fifo_buffer_.getWritePointer(ch);
                        // memcpy 参数：目标地址、源地址、字节数
                        // sizeof(float) * size1 = 拷贝的字节数
                        std::memcpy(dst + start1, src,
                                    static_cast<size_t>(size1) * sizeof(float));
                    }
                }

                // --- 2d-iv. 逐声道拷贝第二段数据（start2 开始，size2 帧，回绕部分）---
                // 当 size2 > 0 时，说明环形缓冲区写入位置回绕到了缓冲区开头
                // 源数据从 decode_buffer_ 的第 size1 帧开始（前 size1 帧已在第一段拷贝）
                if (size2 > 0) {
                    for (int ch = 0; ch < num_channels; ++ch) {
                        const float* src = decode_buffer_.getReadPointer(ch) + size1;
                        float* dst = fifo_buffer_.getWritePointer(ch);
                        std::memcpy(dst + start2, src,
                                    static_cast<size_t>(size2) * sizeof(float));
                    }
                }

                // --- 2d-v. 通知 AbstractFifo 写入完成 ---
                // finishedWrite 原子性地推进写指针，使新数据对消费者（prepareToRead）立即可见
                fifo_->finishedWrite(total_writable);
            }

            // ----------------------------------------------------
            // 2e. 更新解码位置
            // ----------------------------------------------------
            // 读取成功，推进位置
            read_position += frames_to_read;

            // 将当前位置写入原子变量，供 getDecodedPosition() 在任意线程查询
            // 使用默认 memory_order_seq_cst（顺序一致性），与头文件文档一致
            current_position_.store(read_position);
        }
    } catch (const std::exception& e) {
        // ============================================================
        // 异常处理：捕获 std::exception 及其子类
        // ============================================================
        // 可能的异常来源：
        //   - reader_->read()：磁盘 I/O 错误、文件被外部修改/删除
        //   - fifo_->prepareToWrite() / finishedWrite()：通常不抛异常（无锁算法）
        //   - std::memcpy：段错误（理论上不会，因为 buffer 已在 open() 中分配足够空间）
        //
        // 处理方式：记录错误信息，设置 running_ = false 使线程退出
        // 不重新抛出异常（C++ 析构函数不会自动 join 线程，uncaught exception 会导致 std::terminate）
        spdlog::error("解码线程异常（std::exception）：{}", e.what());
        running_.store(false);
        // 设置解码完成标志，通知消费者线程不会再有新数据到来
        // 如果不设置此标志，消费者使用的 isDecodingComplete() && fifo_empty
        // 退出条件将永远无法满足（decoding_complete_ 始终为 false），
        // 导致消费者陷入无限 sleep 循环
        decoding_complete_.store(true);
    } catch (...) {
        // ============================================================
        // 异常处理：捕获非 std::exception 类型的异常
        // ============================================================
        // C++ 允许抛出任意类型（int、const char*、自定义类型等），
        // catch (...) 作为最后的兜底，确保任何异常都不会逃逸出线程函数
        spdlog::error("解码线程发生未知异常（非 std::exception 类型）");
        running_.store(false);
        // 同上：通知消费者不会再有新数据，防止无限等待
        decoding_complete_.store(true);
    }
}
