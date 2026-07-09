# ============================================================
# BitPerfect 领域层 —— CMakeLists.txt
# ============================================================
# 职责：声明领域层（domain）的所有源文件，
#       汇总到 BP_DOMAIN_SOURCES 变量，传递回父级 CMakeLists.txt
#
# 领域层是项目的核心，封装所有业务逻辑，不依赖 UI 和基础设施层的具体实现。
# 按照 DDD（领域驱动设计）思想，领域层定义了：
#   - 实体（Entity）：TrackInfo、AlbumInfo、PlaylistInfo
#   - 领域服务（Domain Service）：AudioEngine、Decoder、Transport
#   - 仓储接口（Repository Interface）：Library、Playlist 的数据访问抽象
#
# P0 阶段：所有模块源码为空（代码尚未编写），本文件仅搭建好框架。
# P1+ 阶段：取消各模块注释即可启用。
# ============================================================

# ============================================================
# AudioEngine 模块 —— 音频引擎
# ============================================================
# 音频引擎是项目的核心组件，负责：
#   - CoreAudio 音频设备打开和管理
#   - 独占模式（Hog Mode）申请和释放
#   - 音频回调函数（实时线程）：从环形缓冲区读取数据并写入设备
#   - 浮点（Float32）和整数（Int16/Int32）双路径支持
#   - 采样率管理和格式转换
#
# ⚠️ 音频引擎运行在实时音频回调线程上，不能：
#   - 分配/释放内存（malloc/new/delete）
#   - 获取互斥锁（mutex.lock()）
#   - 执行文件 I/O（文件读写在解码线程完成）
#   - 调用任何可能阻塞的系统 API
#   （违反上述规则会导致音频 glitch 或 drop-out）
#
# P0 阶段暂无源码，P1 取消下面注释：
# set(AUDIO_ENGINE_SOURCES
#     domain/audio_engine/AudioEngine.h
#     domain/audio_engine/AudioEngine.cpp
# )
set(AUDIO_ENGINE_SOURCES "")

# ============================================================
# AudioDecoder 模块 —— 音频解码器
# ============================================================
# 负责将压缩的音频文件解码为 PCM（脉冲编码调制）原始数据：
#   - 支持格式：FLAC、ALAC、WAV、AIFF、PCM
#   - 解码方式：使用 CoreAudio AudioConverter（macOS 原生）
#   - 输出：Float32 PCM（-1.0 ~ +1.0，样本值归一化为浮点）
#   - 解码运行在后台线程，不阻塞音频回调
#
# decoder 模块有独立子目录和 CMakeLists.txt（2.2.1 创建），
# 源文件在 domain/decoder/CMakeLists.txt 中声明，通过 PARENT_SCOPE
# 将 DECODER_SOURCES 回传到本文件作用域。
add_subdirectory(decoder)

# ============================================================
# Models 模块 —— 数据模型
# ============================================================
# 定义领域中用到的数据结构（纯数据，不含业务逻辑）：
#   - TrackInfo：单首歌曲的元数据（标题、艺术家、专辑、时长、文件路径、格式等）
#   - AlbumInfo：专辑信息（名称、封面路径、发行年份、曲目列表）
#   - PlaylistInfo：播放列表信息（名称、曲目列表、创建时间）
#
# Models 是纯头文件（header-only），不需要 .cpp 文件 ——
# 所有成员都是公开的数据字段，getter/setter 内联在头文件中。
#
# P0 阶段暂无源码，P1 取消下面注释：
# set(MODELS_SOURCES
#     domain/models/TrackInfo.h
# )
set(MODELS_SOURCES "")

# ============================================================
# Transport 模块 —— 播放状态机
# ============================================================
# 管理播放器的播放状态转换：
#   Stopped → Playing → Paused → Stopped（标准播放三态切换）
#   以及：Prev/Next 切歌、Seek 跳转、FF/Rew 快进快退
# 作为播放控制器和音频引擎之间的中间层，负责协调二者
#
# P0 阶段暂无源码，P1 启用：
# set(TRANSPORT_SOURCES
#     domain/transport/Transport.h
#     domain/transport/Transport.cpp
# )
set(TRANSPORT_SOURCES "")

# ============================================================
# Library 模块 —— 音乐库
# ============================================================
# 管理用户的本地音乐文件集合：
#   - 扫描指定目录，发现所有支持的音频文件
#   - 提取元数据（艺术家、专辑、年份、封面等）
#   - 建立数据库索引（通过基础设施层的 Database 模块）
#   - 提供搜索和筛选接口（按艺术家、专辑、流派等）
#
# P0 阶段暂无源码，P2 启用：
# set(LIBRARY_SOURCES
#     domain/library/MusicLibrary.h
#     domain/library/MusicLibrary.cpp
# )
set(LIBRARY_SOURCES "")

# ============================================================
# Playlist 模块 —— 播放列表
# ============================================================
# 管理播放列表的创建、编辑和播放：
#   - 创建/删除播放列表
#   - 向播放列表添加/移除/排序曲目
#   - M3U/M3U8 播放列表文件导入/导出
#   - 播放顺序管理（顺序、随机、单曲循环、列表循环）
#
# P0 阶段暂无源码，P1 启用：
# set(PLAYLIST_SOURCES
#     domain/playlist/Playlist.h
#     domain/playlist/Playlist.cpp
# )
set(PLAYLIST_SOURCES "")

# ============================================================
# 汇总：领域层所有源文件
# ============================================================
# PARENT_SCOPE：将变量提升到父级作用域（src/CMakeLists.txt）
#               不加此关键字则变量仅在本文件可见
#
# P0 阶段：所有子模块变量为空，BP_DOMAIN_SOURCES 为空
# P1+ 阶段：取消上面注释后自动包含对应文件
set(BP_DOMAIN_SOURCES
    ${AUDIO_ENGINE_SOURCES}
    ${DECODER_SOURCES}
    ${MODELS_SOURCES}
    ${TRANSPORT_SOURCES}
    ${LIBRARY_SOURCES}
    ${PLAYLIST_SOURCES}
    PARENT_SCOPE                       # 关键：将变量从子作用域提升到父作用域
)
