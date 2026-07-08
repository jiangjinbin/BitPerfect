# ============================================================
# BitPerfect 基础设施层 —— CMakeLists.txt
# ============================================================
# 职责：声明基础设施层（infrastructure）的所有源文件，
#       汇总到 BP_INFRASTRUCTURE_SOURCES 变量，传递回父级 CMakeLists.txt
#
# 基础设施层负责为上层提供通用的底层能力，不包含任何业务逻辑：
#   - Logger：统一的日志接口（基于 spdlog 封装）
#   - Config：JSON 配置文件的读写
#   - Database：SQLite 数据库的封装（P2 启用）
#   - Threading：线程池和无锁队列（P1 启用）
#   - Platform：平台特定实现封装（macOS CoreAudio、Windows WASAPI 等）
#
# P0 阶段：所有模块源码为空（代码尚未编写），本文件仅搭建好框架。
#          各模块变量设为空字符串，汇总后 BP_INFRASTRUCTURE_SOURCES 也为空。
# P1+ 阶段：取消各模块的注释，填入实际 .h/.cpp 文件路径即可。
# ============================================================

# ============================================================
# Logger 模块 —— 统一日志接口
# ============================================================
# 对 spdlog 进行二次封装，提供项目级的日志宏：
#   LOG_INFO("消息"), LOG_ERROR("消息"), LOG_WARN("消息")
# 封装的好处：
#   - 统一日志格式（时间戳 + 线程 ID + 文件名 + 行号）
#   - 可以随时切换底层实现（例如从 spdlog 换到别的库）
#   - 音频线程安全（使用无锁队列异步输出，不会阻塞音频回调）
#
# P0 阶段暂无源码，P1 取消下面注释：
# set(LOGGER_SOURCES
#     infrastructure/logger/Logger.h
#     infrastructure/logger/Logger.cpp
# )
set(LOGGER_SOURCES "")

# ============================================================
# Config 模块 —— JSON 配置文件读写
# ============================================================
# 使用 nlohmann/json 读写应用配置，实现：
#   - 开机自启播放最后一个播放列表
#   - UI 主题和语言偏好
#   - 音频设备选择（上次使用的设备 ID、采样率、位深）
#   - 快捷键映射
#
# P0 阶段暂无源码，P1 取消下面注释：
# set(CONFIG_SOURCES
#     infrastructure/config/ConfigStore.h
#     infrastructure/config/ConfigStore.cpp
# )
set(CONFIG_SOURCES "")

# ============================================================
# Database 模块 —— SQLite 数据库封装
# ============================================================
# 对 sqlite3 进行 C++ RAII 风格封装，提供：
#   - 自动事务管理（构造时 BEGIN，析构时 COMMIT/ROLLBACK）
#   - 预编译语句缓存（避免重复 parse SQL）
#   - WAL 模式（Write-Ahead Logging，读写不互斥）
#   - 迁移系统（数据库版本升级时自动执行 SQL 脚本）
#
# P0 阶段暂无源码，P2 阶段启用（音乐库管理需要数据库）：
# set(DATABASE_SOURCES
#     infrastructure/database/Database.h
#     infrastructure/database/Database.cpp
# )
set(DATABASE_SOURCES "")

# ============================================================
# Threading 模块 —— 线程工具
# ============================================================
# 提供跨平台的线程工具集：
#   - 线程池（ThreadPool）：管理后台解码、指纹计算等任务
#   - 无锁队列（LockFreeQueue）：音频线程和生产线程之间的数据传递
#   - 线程断言（AudioThreadAssert）：Debug 模式下验证线程归属
#   - 线程优先级管理：将音频回调线程设为最高实时优先级
#
# P0 阶段暂无源码，P1 启用：
# set(THREADING_SOURCES
#     infrastructure/threading/ThreadPool.h
#     infrastructure/threading/ThreadPool.cpp
# )
set(THREADING_SOURCES "")

# ============================================================
# Platform 模块 —— 平台抽象层
# ============================================================
# 将不同操作系统的 API 差异封装在 platform/<name>/ 目录下：
#   - platform/macos/：macOS CoreAudio 辅助（Hog Mode、采样率切换、整数模式）
#   - platform/windows/：Windows WASAPI（P4 跨平台时实现）
#   - platform/linux/：Linux ALSA 直通（P4 跨平台时实现）
#   - PlatformUtils.h/cpp：跨平台的通用工具函数（路径处理、系统信息）
#
# .mm 扩展名 = Objective-C++ 文件，可以同时写 C++ 和 Objective-C 代码，
# 用于直接调用 macOS 系统框架（CoreAudio 是 C 接口，不需要 ObjC++，但
# 某些系统 API 如 NSApplication 需要用 ObjC++）

# macOS CoreAudio 辅助函数
# P0 阶段暂无源码，P1 取消下面注释：
# set(PLATFORM_MACOS_SOURCES
#     infrastructure/platform/macos/CoreAudioHelper.h
#     infrastructure/platform/macos/CoreAudioHelper.mm
# )
set(PLATFORM_MACOS_SOURCES "")

# 跨平台通用工具函数
# P0 阶段暂无源码，P1 取消下面注释：
# set(PLATFORM_UTILS_SOURCES
#     infrastructure/platform/PlatformUtils.h
#     infrastructure/platform/PlatformUtils.cpp
# )
set(PLATFORM_UTILS_SOURCES "")

# ============================================================
# 汇总：基础设施层所有源文件
# ============================================================
# 将各模块的源文件列表合并到一个变量中
# PARENT_SCOPE：将变量设置到父级 CMakeLists.txt（即 src/CMakeLists.txt）的作用域中
#               不加 PARENT_SCOPE 的话，这个变量只在本文件可见，父级拿不到
#
# P0 阶段：所有子模块变量为空，BP_INFRASTRUCTURE_SOURCES 也被设置为空
# P1+ 阶段：取消上面注释后，此变量自动包含对应文件
set(BP_INFRASTRUCTURE_SOURCES
    ${LOGGER_SOURCES}
    ${CONFIG_SOURCES}
    ${DATABASE_SOURCES}
    ${THREADING_SOURCES}
    ${PLATFORM_MACOS_SOURCES}
    ${PLATFORM_UTILS_SOURCES}
    PARENT_SCOPE                       # 关键：将变量从子作用域提升到父作用域
)
