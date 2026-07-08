# ============================================================
# BitPerfect 测试 —— CMakeLists.txt
# ============================================================
# 职责：定义测试可执行文件目标（BitPerfectTests），链接 Catch2 测试框架
#
# 本文件管理项目中所有单元测试的编译和运行：
#   1. 声明各层测试源文件列表（按模块组织）
#   2. 汇总并创建测试可执行文件目标
#   3. 注册 CTest 测试，支持 ctest 命令一键运行全部测试
# ============================================================

# ============================================================
# 1. 测试源文件列表
# ============================================================
# 按架构分层组织测试文件，与源代码目录结构一一对应。
# 每层实现后取消对应注释，测试文件即可自动编译。

# --- 测试入口文件 ---
# 包含 Catch2 的配置和 main 函数初始化
# Catch2::Catch2WithMain 已经提供了 main 函数，
# 此文件只需写 TEST_CASE 宏定义的测试用例即可
set(BP_TEST_MAIN
    test_main.cpp
)

# --- 基础设施层测试 ---
# 测试 Logger、Config、Database、Threading、Platform 模块
# P0 阶段暂无源码，取消注释以启用：
# set(BP_TEST_INFRASTRUCTURE
#     tests/infrastructure/test_config_store.cpp       # ConfigStore 配置读写测试
#     tests/infrastructure/test_database.cpp           # Database 数据库封装测试
#     tests/infrastructure/test_fifo_buffer.cpp        # 无锁环形缓冲区测试
# )
set(BP_TEST_INFRASTRUCTURE "")

# --- 领域层测试 ---
# 测试 AudioEngine、Decoder、Transport、Library、Playlist、Models 模块
# 这是项目测试的核心部分，P1+ 阶段会写最多测试
# set(BP_TEST_DOMAIN
#     tests/domain/test_audio_engine.cpp               # 音频引擎测试（最重要）
#     tests/domain/test_audio_decoder.cpp              # 音频解码器测试
#     tests/domain/test_transport.cpp                  # 播放状态机测试
#     tests/domain/test_music_library.cpp              # 音乐库扫描测试
#     tests/domain/test_playlist.cpp                   # 播放列表管理测试
#     tests/domain/test_metadata_parser.cpp            # 元数据解析测试
# )
set(BP_TEST_DOMAIN "")

# --- 应用层测试 ---
# 测试 Application Controller 和 Player Controller 的集成逻辑
# set(BP_TEST_APPLICATION
#     tests/application/test_player_controller.cpp
#     tests/application/test_library_controller.cpp
# )
set(BP_TEST_APPLICATION "")

# ============================================================
# 2. 汇总所有测试源文件
# ============================================================
set(BP_TEST_SOURCES
    ${BP_TEST_MAIN}
    ${BP_TEST_INFRASTRUCTURE}
    ${BP_TEST_DOMAIN}
    ${BP_TEST_APPLICATION}
)

# ============================================================
# 3. 测试可执行文件
# ============================================================
# CMake 4.x 规则：add_executable 必须提供至少一个源文件。
# 如果 BP_TEST_SOURCES 为空（只有 test_main.cpp 被注释掉），
# 直接调用 add_executable 会报错。因此需要先检查列表是否非空。

list(LENGTH BP_TEST_SOURCES BP_TEST_COUNT)
# list(LENGTH ...) 获取列表的长度，赋值给 BP_TEST_COUNT

if(BP_TEST_COUNT GREATER 0)
    # 创建测试可执行文件目标
    # Catch2::Catch2WithMain 自带 main 函数，我们不需要自己写 main
    add_executable(BitPerfectTests ${BP_TEST_SOURCES})

    # 链接测试框架和第三方库
    # Catch2::Catch2WithMain：Catch2 测试框架 + 自动生成的 main 函数
    # spdlog::spdlog：测试中可以用日志输出调试信息
    # nlohmann_json：测试 JSON 配置解析时使用
    # sqlite3：测试数据库封装时使用
    target_link_libraries(BitPerfectTests PRIVATE
        Catch2::Catch2WithMain
        spdlog::spdlog
        nlohmann_json
        sqlite3
    )

    # 添加 src/ 到 include 路径，让测试代码可以这样引用：
    #   #include "domain/audio_engine/AudioEngine.h"
    target_include_directories(BitPerfectTests PRIVATE ${CMAKE_SOURCE_DIR}/src)

    # macOS 平台：链接 CoreAudio 框架（音频设备测试需要使用系统音频 API）
    if(APPLE)
        target_link_libraries(BitPerfectTests PRIVATE
            "-framework CoreAudio"
            "-framework AudioToolbox"
            "-framework CoreFoundation"
        )
    endif()

    # ============================================================
    # 4. 注册测试（CTest 集成）
    # ============================================================
    # enable_testing() 启用 CTest（CMake 内置的测试运行器）
    enable_testing()

    # include(CTest) 加载 CTest 模块，提供 Dart 仪表盘等高级功能
    include(CTest)

    # include(Catch) 加载 Catch2 的 CTest 集成模块
    # 这个模块由 Catch2 库提供（third_party/Catch2/contrib/Catch.cmake）
    include(Catch)

    # catch_discover_tests 自动发现所有 TEST_CASE 并注册到 CTest
    # 执行 ctest 命令时，每个 TEST_CASE 会作为独立的测试项运行，
    # 获得独立的通过/失败报告和时间统计
    catch_discover_tests(BitPerfectTests)
else()
    # P0 阶段：如果不取消注释任何测试文件，输出一条信息说明这是预期行为
    message(STATUS "测试源文件为空，跳过创建测试目标（P0 阶段预期行为）")
endif()
