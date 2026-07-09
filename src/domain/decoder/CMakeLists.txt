# ============================================================
# BitPerfect 领域层 / AudioDecoder 模块 —— CMakeLists.txt
# ============================================================
# 职责：声明 AudioDecoder 模块的所有源文件，
#       通过 PARENT_SCOPE 将 DECODER_SOURCES 回传到父级 domain/CMakeLists.txt
#
# AudioDecoder 是 P0 核心验证阶段的第一个业务模块，负责：
#   - 将 FLAC/WAV 等压缩音频文件解码为 PCM 浮点数据
#   - 解码运行在独立后台线程，不阻塞音频回调线程
#   - 解码结果写入 AbstractFifo 无锁环形缓冲区，供音频引擎消费
#
# 本模块是 2.2 阶段"基础音频文件解码"的核心交付物，
# 共 14 个子步骤（2.2.1 → 2.2.14），从目录骨架搭建到单元测试覆盖。
#
# 当前完成状态（2.2.10）：
#   2.2.1 ✅ 创建 decoder 模块目录和 CMakeLists.txt
#   2.2.2 ✅ 编写 AudioDecoder 类头文件（FileInfo + Listener 接口 + 类声明，250 行）
#   2.2.3 ✅ 实现构造函数和析构函数（构造空体 + 析构调用 stopDecoding()）
#   2.2.4 ✅ 实现 open() 方法（10 步流程：AudioFormatManager → AudioFormatReader → 元数据提取 → Fifo 分配）
#   2.2.5 ✅ 实现 startDecoding() 方法（3 道 Guard + std::thread 创建）
#   2.2.6 ✅ 实现 stopDecoding() 方法（原子退出标志 + join + Fifo reset）
#   2.2.7 ✅ 实现 decodingLoop() 方法（while 循环 → reader_->read() → memcpy 入 Fifo → EOF 检测）
#   2.2.8 ✅ 实现 seekTo() 方法（越界检查 → stop → 设置 current_position_ → start）
#   2.2.9 ✅ 实现 3 个 getter（提前于 2.2.7 完成，均为单行原子操作）
#   2.2.10 ✅ 实现 addListener() / removeListener()（P0 仅管理列表，不触发回调）
#   ⬜ 2.2.14 编写单元测试（test_audio_decoder.cpp，6 个测试用例）
# ============================================================

# ============================================================
# AudioDecoder 模块源文件
# ============================================================
# AudioDecoder.h   —— 2.2.2 ✅：类声明（FileInfo 结构体 + Listener 内部接口 + 9 个公开方法 + 1 个私有方法 + 10 个成员变量，约 250 行）
# AudioDecoder.cpp —— 2.2.3~2.2.10 ✅：完整实现（构造/析构/open/startDecoding/stopDecoding/seekTo/getFifo/isDecodingComplete/getDecodedPosition/addListener/removeListener/decodingLoop，约 790 行）
#
# 源文件路径说明：
#   路径以 domain/decoder/ 为前缀，因为 DECODER_SOURCES 最终被 src/CMakeLists.txt
#   中的 target_sources 消费，路径基准是 src/ 目录。
#   例如 domain/decoder/AudioDecoder.h → src/domain/decoder/AudioDecoder.h

set(DECODER_SOURCES
    domain/decoder/AudioDecoder.h        # 2.2.2 ✅：AudioDecoder 类声明 + FileInfo + Listener 接口（约 250 行）
    domain/decoder/AudioDecoder.cpp      # 2.2.3~2.2.10 ✅：全部方法完整实现（约 790 行）
    PARENT_SCOPE                         # 将 DECODER_SOURCES 变量回传到父级 domain/CMakeLists.txt
)
