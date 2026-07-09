/**
 * 文件名：AudioDecoder.h
 * 职责：AudioDecoder 模块的公开接口 —— 将 FLAC/WAV 等音频文件解码为 PCM 浮点数据，
 *       写入 AbstractFifo 无锁环形缓冲区，供音频引擎消费
 * 所属模块：domain/decoder（领域层）
 * 所属线程：open()/startDecoding()/stopDecoding()/seekTo() 由 CLI/UI 线程调用，
 *          decodingLoop() 运行在独立后台线程，
 *          getFifo()/isDecodingComplete()/getDecodedPosition() 可被任意线程安全调用（原子变量 + 无锁 Fifo）
 *
 * 使用流程：
 *   1. AudioDecoder decoder;
 *   2. decoder.open(audio_file);        // 打开文件，解析元数据
 *   3. decoder.startDecoding();         // 启动后台解码线程
 *   4. decoder.getFifo().read(...);     // 从任意线程无锁读取 PCM 数据
 *   5. decoder.stopDecoding();          // 停止解码（析构函数自动调用）
 */
#pragma once

// ============================================================
// C++ 标准库
// ============================================================
#include <atomic>      // std::atomic —— 线程间无锁共享状态（running_、decoding_complete_、current_position_）
#include <memory>      // std::unique_ptr —— 独占所有权智能指针（reader_、fifo_）
#include <string>      // std::string —— FileInfo::format_name、Listener::onDecodingError 参数
#include <thread>      // std::thread —— 解码后台线程
// （fifo_buffer_ 已改为 juce::AudioBuffer<float>，不再需要 <vector>）

// ============================================================
// JUCE 第三方库
// ============================================================
#include <juce_audio_basics/juce_audio_basics.h>    // juce::AudioBuffer<float> —— 单帧解码临时缓冲
#include <juce_audio_formats/juce_audio_formats.h>  // juce::AudioFormatReader —— 音频文件解析和帧读取
#include <juce_core/juce_core.h>                    // juce::File、juce::AbstractFifo、juce::int64
#include <juce_events/juce_events.h>                // juce::ListenerList<Listener> —— 线程安全的监听器列表


// ============================================================
// FileInfo —— 音频文件元信息（纯数据载体，所有字段公开）
// ============================================================
/**
 * FileInfo 描述一个已打开的音频文件的基本属性。
 * 在 open() 成功后填充，供上层查询文件格式和参数。
 *
 * 这是一个 struct（而非 class），因为它只有数据没有行为，
 * 所有字段公开可读，无需 getter/setter。
 */
struct FileInfo {
    double sample_rate = 44100.0;     // 采样率（Hz），如 44100.0、48000.0、96000.0
    int num_channels = 2;             // 声道数，1 = 单声道，2 = 立体声
    int bit_depth = 16;               // 位深（bits/sample），如 16、24、32
    juce::int64 total_frames = 0;     // 总采样帧数（有多少个采样时刻，不是"采样点数 × 声道数"。一帧 = 同一时刻所有声道的采样点算作 1 帧，如立体声 t=0 时刻 {L,R} 算 1 帧。文件时长 = total_frames / sample_rate）
    double duration_seconds = 0.0;   // 时长（秒），由 total_frames / sample_rate 计算得出
    std::string format_name;          // 格式名称（如 "FLAC"、"WAV"），由 AudioFormatReader 提供
};


// ============================================================
// AudioDecoder —— 音频解码器
// ============================================================
/**
 * AudioDecoder 负责将压缩音频文件（FLAC/WAV）解码为 PCM 浮点数据。
 *
 * 设计要点：
 *   - 解码在独立后台线程（std::thread）中执行，不阻塞调用线程
 *   - 解码结果通过 juce::AbstractFifo（无锁环形缓冲区）输出，
 *     音频实时线程可以直接从中读取，无需加锁
 *   - AbstractFifo 大小根据采样率和声道数动态计算（0.5 秒缓冲），
 *     而非硬编码常量
 *   - P0 阶段 Listener 通知留空（无 UI 层消费者），
 *     addListener/removeListener 仅管理列表，不触发回调
 *
 * 线程安全：
 *   - open() / startDecoding() / stopDecoding() / seekTo()：由 CLI/UI 线程调用
 *   - getFifo()：任意线程安全（返回引用，AbstractFifo 本身是线程安全的）
 *   - isDecodingComplete() / getDecodedPosition()：任意线程安全（原子变量）
 *   - decodingLoop()：运行在独立 std::thread 中，仅操作私有成员
 */
class AudioDecoder {
public:
    // ============================================================
    // Listener —— 解码器事件监听接口（内部类）
    // ============================================================
    /**
     * 解码器状态变化时的回调接口。
     * 观察者模式：Listener 注册到 AudioDecoder，解码线程状态变化时通知所有 Listener。
     *
     * 注意（P0 阶段）：addListener/removeListener 仅管理列表，
     * 不会实际触发任何回调。P1 阶段通过 MessageManager::callAsync 实现跨线程通知。
     */
    struct Listener {
        virtual ~Listener() = default;

        /**
         * 解码开始回调（P0 阶段不会被调用）
         * P1：在 startDecoding() 中通过 MessageManager::callAsync 投递到 UI 线程
         */
        virtual void onDecodingStarted() {}

        /**
         * 解码完成回调（P0 阶段不会被调用）
         * P1：在 decodingLoop() 读到文件末尾时通过 MessageManager::callAsync 投递到 UI 线程
         */
        virtual void onDecodingComplete() {}

        /**
         * 解码错误回调（P0 阶段不会被调用）
         *
         * @param error_message 人类可读的错误描述（英文，来自 JUCE 或系统 API）
         * P1：在解码过程中捕获异常时通过 MessageManager::callAsync 投递到 UI 线程
         */
        virtual void onDecodingError(const std::string& error_message) {}
    };

    // ============================================================
    // 构造 / 析构
    // ============================================================

    /**
     * 构造函数 —— 所有成员初始化为安全默认值
     *
     * 不分配任何资源（reader_、fifo_ 均为 nullptr），
     * 资源分配在 open() 中按需进行。
     */
    AudioDecoder();

    /**
     * 析构函数 —— 确保解码线程已终止
     *
     * 内部调用 stopDecoding() 设置退出标志并 join 线程。
     * unique_ptr 自动释放 reader_ 和 fifo_ 持有的资源（RAII）。
     */
    ~AudioDecoder();

    // ============================================================
    // 公开方法
    // ============================================================

    /**
     * 打开音频文件并提取元数据
     *
     * 创建 juce::AudioFormatReader 解析文件头，提取采样率、声道数、位深、总采样数等信息，
     * 并动态分配 AbstractFifo 无锁环形缓冲区。
     *
     * @param file 要打开的音频文件（juce::File 对象，支持 FLAC/WAV 等格式）
     * @return true  文件成功打开，reader_ 创建完毕，file_info_ 已填充
     * @return false 格式不支持或文件损坏，reader_ 保持 nullptr
     *
     * 线程约束：必须在调用 startDecoding() 之前由同一线程调用。
     *         重复调用 open() 会先释放旧 reader_，再创建新 reader_。
     */
    [[nodiscard]] bool open(const juce::File& file);

    /**
     * 启动后台解码线程
     *
     * 检查 reader_ 已就绪（open() 调用成功），然后：
     *   1. 设置 running_ = true、decoding_complete_ = false、current_position_ = 0
     *   2. 创建 std::thread 执行 decodingLoop()
     *   3. 将线程对象保存到 decoding_thread_
     *
     * 线程约束：必须在 open() 成功后调用。重复调用（解码线程已运行时）会被忽略。
     */
    void startDecoding();

    /**
     * 停止解码并等待线程退出
     *
     * 1. 设置 running_ = false（通知 decodingLoop 退出 while 循环）
     * 2. 若 decoding_thread_ 可 join，调用 join() 等待线程完全结束
     * 3. 重置 AbstractFifo 读写指针，清空缓冲
     *
     * 线程约束：可在任意非音频回调线程调用。析构函数会自动调用本方法。
     *         重复调用安全（幂等操作，join 后的线程不可再 join，通过 joinable() 检查）。
     */
    void stopDecoding();

    /**
     * 跳转到指定采样位置
     *
     * 实现流程：
     *   1. 验证 sample_position 在 [0, total_frames] 范围内，越界则静默返回
     *   2. 调用 stopDecoding() 暂停当前解码（内部包含 join 等待 + Fifo reset）
     *   3. 将目标位置写入 current_position_ 原子变量（seek 机制的核心）
     *   4. 调用 startDecoding() 从新位置恢复解码
     *
     * seek 机制说明：
     *   JUCE AudioFormatReader 基类没有 setPosition() 方法，seek 不通过操作 reader 实现，
     *   而是通过 current_position_ 原子变量在 seekTo() → decodingLoop() 之间传递目标位置。
     *   decodingLoop() 从 current_position_.load() 读取起始位置，作为 read() 的
     *   readerStartSample 参数传入，从而实现在指定位置开始解码。
     *
     * @param sample_position 目标采样帧位置（从文件开头算起，0-based）
     *                        允许等于 total_frames（立即触发 EOF 检测）
     *
     * 线程约束：必须在 open() 成功后调用。越界时无操作（静默忽略）。
     */
    void seekTo(juce::int64 sample_position);

    /**
     * 获取 AbstractFifo 引用 —— 供音频线程无锁读取 PCM 数据
     *
     * AbstractFifo 是无锁环形缓冲区，音频实时线程可以安全地调用其 read() 方法
     * 而不会违反"禁止加锁"的硬实时约束。
     *
     * @return AbstractFifo 的常量引用（调用方通过它读取已解码的 PCM 数据）
     *
     * 线程约束：任意线程安全。但必须在 open() 之后调用（否则 fifo_ 为 nullptr，行为未定义）。
     */
    juce::AbstractFifo& getFifo();

    /**
     * 查询解码是否已全部完成
     *
     * @return true  文件所有帧已解码完毕，Fifo 中可能还有未消费的数据
     * @return false 解码正在进行中，或尚未开始解码
     *
     * 线程约束：任意线程安全（原子变量 load）
     */
    [[nodiscard]] bool isDecodingComplete() const;

    /**
     * 查询当前解码位置
     *
     * @return 已解码到的采样帧位置（0-based，从文件开头算起）
     *         返回 0 表示尚未开始解码或刚调用 startDecoding()
     *
     * 线程约束：任意线程安全（原子变量 load）
     */
    [[nodiscard]] juce::int64 getDecodedPosition() const;

    /**
     * 注册解码事件监听器
     *
     * 将 listener 加入 listeners_ 列表。同一 listener 重复添加会被 ListenerList 忽略。
     * P0 阶段：仅管理列表，不会向 listener 发送任何通知。
     *
     * @param listener 要注册的监听器指针（不能为 nullptr，AudioDecoder 不持有所有权）
     *
     * 线程约束：应在 UI/CLI 线程调用，避免与解码线程产生竞态
     */
    void addListener(Listener* listener);

    /**
     * 移除解码事件监听器
     *
     * 将 listener 从 listeners_ 列表中移除。移除不存在的 listener 是安全操作（无影响）。
     *
     * @param listener 要移除的监听器指针
     *
     * 线程约束：应在 UI/CLI 线程调用，避免与解码线程产生竞态
     */
    void removeListener(Listener* listener);

private:
    // ============================================================
    // 私有方法
    // ============================================================

    /**
     * 解码线程主循环
     *
     * 这是 startDecoding() 创建的 std::thread 的执行体。
     * 循环调用 reader_->read() 逐帧解码，将结果写入 AbstractFifo。
     *
     * 循环逻辑：
     *   while (running_):
     *     1. reader_->read(&decode_buffer_, 0, frames_per_chunk, start_sample, true)
     *        从当前文件位置读取4096帧（4096 frames）
     *     2. 若返回值 <= 0（文件末尾或错误）：设置 decoding_complete_ = true，退出循环
     *     3. 将 decode_buffer_ 中的数据写入 AbstractFifo（fifo_->write() + memcpy）
     *     4. 更新 current_position_ = reader_->getPosition()
     *
     * 异常安全：read() 或 write() 抛出异常时捕获，设置 running_ = false，不崩溃。
     *
     * 线程约束：仅由 decoding_thread_ 执行，不直接在其他线程调用
     */
    void decodingLoop();

    // ============================================================
    // 私有成员变量（声明时给默认值，遵循 Google C++ Style Guide）
    // ============================================================

    // --- 音频格式管理器（持有 AudioFormat 对象）---
    // AudioFormatReader 内部持有 AudioFormat 的裸指针（不持有所有权）。
    // AudioFormat 对象的实际所有者是 AudioFormatManager，
    // 因此 format_manager_ 必须比 reader_ 活得更久。
    //
    // 放在 reader_ 前面声明，利用 C++ 逆序析构规则：
    //   声明顺序：format_manager_（先）→ reader_（后）
    //   析构顺序：reader_（先）→ format_manager_（后）
    // 确保 reader_ 销毁时，它引用的 AudioFormat 对象仍然存活。
    //
    // 在 open() 中通过 std::make_unique 创建并注册基本格式（WAV、FLAC 等）。
    std::unique_ptr<juce::AudioFormatManager> format_manager_ = nullptr;

    // --- 音频文件读取器（持有文件句柄，读取和解码音频帧）---
    // 使用 unique_ptr：reader_ 的生命周期与 AudioDecoder 绑定，
    // AudioDecoder 销毁时 reader_ 自动析构，关闭文件句柄
    std::unique_ptr<juce::AudioFormatReader> reader_ = nullptr;

    // --- 无锁环形缓冲区（解码线程写 → 音频线程读）---
    // AbstractFifo 内部使用原子变量管理读写指针，两端无需加锁
    std::unique_ptr<juce::AbstractFifo> fifo_ = nullptr;

    // --- Fifo 底层存储 ---
    // AbstractFifo 只管理读写指针，实际数据需要一个独立的缓冲区来存放。
    // 改为 juce::AudioBuffer<float> 与 decode_buffer_ 类型一致，
    // 拷贝时无需平面↔交错格式转换，直接逐声道 memcpy。
    // 大小在 open() 中按 num_channels × (sample_rate × 0.5) 秒动态分配
    juce::AudioBuffer<float> fifo_buffer_;

    // --- 每次解码的最大采样帧数 ---
    // open() 分配 decode_buffer_ 和 decodingLoop() 每次读取都使用此值，
    // 作为类静态常量确保两处引用同一数据源，避免字面量分散导致不一致。
    // 4096 是经验值，兼顾内存开销（立体声 32-bit float ≈ 32KB/chunk）和解码效率。
    static constexpr int kFramesPerChunk = 4096;

    // --- 单帧解码临时缓冲 ---
    // reader_->read() 将解码后的 PCM 数据写入此缓冲区。
    // 大小：num_channels × kFramesPerChunk frames（每次解码一个 chunk）
    // 类型：juce::AudioBuffer<float>（JUCE 的多声道音频缓冲封装）
    juce::AudioBuffer<float> decode_buffer_;

    // --- 当前打开文件的元信息 ---
    // 在 open() 中填充，之后只读
    FileInfo file_info_;

    // --- 线程控制原子变量 ---
    // running_：控制解码循环的开关
    //   - startDecoding() 设为 true  → decodingLoop 开始执行
    //   - stopDecoding()  设为 false → decodingLoop 退出 while 循环
    std::atomic<bool> running_{false};

    // decoding_complete_：标记文件是否已全部解码完毕
    //   - startDecoding() 重置为 false
    //   - decodingLoop() 读到文件末尾时设为 true
    //   - isDecodingComplete() 返回此值（任意线程安全读取）
    std::atomic<bool> decoding_complete_{false};

    // current_position_：当前已解码到的采样帧位置
    //   - 由 decodingLoop() 在每帧解码后更新
    //   - seekTo() 调用 reader_->setPosition() 后同步更新此值
    //   - getDecodedPosition() 返回此值（任意线程安全读取）
    std::atomic<juce::int64> current_position_{0};

    // --- 监听器列表（线程安全）---
    // 使用 juce::ListenerList 管理多个 Listener，自动处理遍历中删除的安全性
    juce::ListenerList<Listener> listeners_;

    // --- 解码线程 ---
    // 由 startDecoding() 创建，stopDecoding() join 等待退出
    // 使用 std::thread（而非 juce::ThreadPool）：解码器只管理一个专用线程
    std::thread decoding_thread_;
};
