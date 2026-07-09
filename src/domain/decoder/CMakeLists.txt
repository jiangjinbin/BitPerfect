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
# ============================================================

# ============================================================
# AudioDecoder 模块源文件
# ============================================================
# 2.2.2 ✅：AudioDecoder.h 类声明（FileInfo + Listener 接口 + AudioDecoder 类）
# 2.2.3 待完成：AudioDecoder.cpp 实现（构造/析构/open/解码循环等）
#
# 源文件路径说明：
#   路径以 domain/decoder/ 为前缀，因为 DECODER_SOURCES 最终被 src/CMakeLists.txt
#   中的 target_sources 消费，路径基准是 src/ 目录。
#   例如 domain/decoder/AudioDecoder.h → src/domain/decoder/AudioDecoder.h

set(DECODER_SOURCES
    domain/decoder/AudioDecoder.h        # 2.2.2 ✅：AudioDecoder 类声明 + FileInfo + Listener 接口
    domain/decoder/AudioDecoder.cpp      # 2.2.4 ✅：open() 方法实现（文件打开、元数据提取、Fifo 分配）
    PARENT_SCOPE
)
