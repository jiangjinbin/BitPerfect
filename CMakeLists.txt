# ============================================================
# BitPerfect —— 根 CMakeLists.txt
# ============================================================
# 职责：项目定义、C++ 标准、引入第三方依赖、添加子目录
#
# 这是 CMake 构建系统的入口文件，CMake 会从这里开始读取。
# 它定义了项目的基础信息（项目名、版本号、语言），设置了全局
# 编译选项（C++17 标准、macOS 最低版本、架构），然后引入所有
# 第三方依赖库，最后添加源代码和测试子目录。
# ============================================================

# --- 最低 CMake 版本要求 ---
# CMake 3.22 开始支持更完善的预设（presets）和 arm64 交叉编译，
# 同时与 Xcode 14+（macOS 13 开发环境）兼容
cmake_minimum_required(VERSION 3.22)

# --- 项目定义 ---
# project() 是 CMake 创建项目时必须调用的第一个命令。
# VERSION: 项目的语义版本号，可在 CMakeLists.txt 中用 PROJECT_VERSION 变量引用
# DESCRIPTION: 项目的简要描述（CMake 3.9+ 支持）
# LANGUAGES C CXX:
#   - C：编译 SQLite 的 sqlite3.c 需要 C 编译器
#   - CXX：本项目的主要语言是 C++（这是 CMake 对 C++ 的内部名称）
#   - CMake 4.x 要求在 project() 中显式声明所有需要的语言，否则 C 编译器变量不会被初始化
project(BitPerfect
    VERSION 1.0.0
    DESCRIPTION "跨平台 Bit-Perfect 本地音乐播放器"
    LANGUAGES C CXX
)

# ============================================================
# CMake 4.x 兼容策略
# ============================================================
# CMP0174：CMake 4.0 引入的新策略，控制 add_subdirectory 解析子目录 CMakeLists.txt 的行为
# NEW 表示使用最新行为，确保与 CMake 4.x 完全兼容，避免 CMP0174 警告
if(POLICY CMP0174)
    cmake_policy(SET CMP0174 NEW)
endif()

# ============================================================
# 全局 C++ 编译设置
# ============================================================
# 这三条命令告诉 CMake 如何编译 C++ 代码：

# CMAKE_CXX_STANDARD：使用 C++17 标准（按项目技术选型要求）
# C++17 提供了我们需要的 std::optional、std::filesystem、
# if constexpr、结构化绑定等现代特性
set(CMAKE_CXX_STANDARD 17)

# CMAKE_CXX_STANDARD_REQUIRED：强制要求编译器必须支持 C++17
# 如果编译器不支持 C++17，CMake 会直接报错退出，而不是悄悄降级
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# CMAKE_CXX_EXTENSIONS：禁用编译器专有的 GNU 扩展（如 typeof、语句表达式）
# 禁用后确保代码在不同编译器（Clang、GCC、MSVC）之间可移植
set(CMAKE_CXX_EXTENSIONS OFF)

# 生成 compile_commands.json（编译命令数据库）
# 这个文件记录了每个 .cpp 文件的完整编译命令，供以下工具使用：
#   - clangd（C++ 语言服务器，提供代码补全、跳转、错误检查）
#   - VS Code 的 C/C++ 插件
#   - CLion 等 IDE
# 设置为 ON 后，每次 cmake 配置都会更新此文件
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================================
# 编译选项
# ============================================================

# Debug 模式下的额外安全检测（仅开发期使用）
# -fsanitize=address：启用 AddressSanitizer（ASan），检测内存越界、use-after-free、
#                     双重释放、内存泄漏等内存错误
# -fsanitize=undefined：启用 UndefinedBehaviorSanitizer（UBSan），检测整数溢出、
#                       空指针解引用、非法类型转换等未定义行为
# if(APPLE)：仅在 macOS 上启用（Linux 上 ASan 可能需要额外的库安装步骤）
# Release 模式下这些标志不会被添加（Release 构建不需要 sanitizer，它们会拖慢运行时性能）
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND APPLE)
    add_compile_options(-fsanitize=address -fsanitize=undefined)
    add_link_options(-fsanitize=address -fsanitize=undefined)
endif()

# --- macOS 特定编译设置 ---
if(APPLE)
    # mac OS 最低部署目标（13.0 即 macOS Ventura，2022 年发布）
    # 设置这个后，编译出的二进制文件可以在 macOS 13.0 及更新版本上运行
    # 低于 13.0 的系统会收到错误提示
    # CACHE STRING 表示这个值会持久化到 CMakeCache.txt，后续重配置不会丢失
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "macOS 最低部署版本")

    # 目标 CPU 架构：同时包含 Apple Silicon（arm64）和 Intel（x86_64）
    # 这样编译出的 Mach-O 文件是 通用二进制（Universal Binary），
    # 既可以在 M1/M2/M3/M4 Mac 上原生运行，也可以在 Intel Mac 上运行
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "目标架构（通用二进制）")
endif()

# ============================================================
# Debug 宏定义
# ============================================================
# 使用 CMake 生成器表达式（Generator Expression）有条件地定义宏：
# $<$<CONFIG:Debug>:BP_DEBUG>
#   解读：当构建类型为 Debug 时，向所有编译单元添加 -DBP_DEBUG 宏
#   当构建类型为 Release 时，不添加此宏
# 代码中可以这样使用：
#   #ifdef BP_DEBUG
#       assert_audio_thread();  // 仅在 Debug 模式下检查音频线程断言
#   #endif
# 好处：Release 构建时相关检查代码通过 #ifdef 被预处理器去掉，零运行时开销
add_compile_definitions(
    $<$<CONFIG:Debug>:BP_DEBUG>
)

# ============================================================
# 第三方依赖
# ============================================================
# 本项目使用 5 个第三方库，按目录引入方式分为三类：
#   1. add_subdirectory：有 CMakeLists.txt 的库（JUCE、spdlog、Catch2）
#   2. INTERFACE 库：纯头文件库（nlohmann/json）
#   3. 直接编译源文件：无 CMakeLists.txt 的库（sqlite3）

# --- JUCE 音频应用框架 ---
# JUCE（Jules' Utility Class Extensions）是一个跨平台 C++ 音频应用框架，
# 提供音频 I/O、GUI、文件处理等模块。P0 阶段仅使用 juce_core 核心模块
# （字符串、文件、线程等基础工具），不使用 GUI 相关模块。
#
# 设置 JUCE_MODULE_AVAILABLE_juce_gui_basics ON：
#   告诉 JUCE 的构建系统，这些 GUI 模块是"可用的"但不是必须的。
#   不设置的话，如果模块路径不存在，JUCE 的 CMake 可能会报错。
#   P0 阶段不需要 GUI，所以设为 ON 表示"可接受缺失"。
set(JUCE_MODULE_AVAILABLE_juce_gui_basics ON)
set(JUCE_MODULE_AVAILABLE_juce_gui_extra ON)
set(JUCE_MODULE_AVAILABLE_juce_opengl ON)

# add_subdirectory：引入 JUCE 的 CMake 构建系统
# 这会让 CMake 去解析 third_party/JUCE/CMakeLists.txt 并定义 juce_core 等目标
add_subdirectory(third_party/JUCE)

# --- spdlog 日志库 ---
# spdlog 是一个高性能 C++ 日志库，类似于 Python 的 logging 或 Java 的 log4j。
# 它默认使用 header-only 模式（所有实现都在头文件中），不需要编译 .cpp 文件。
#
# SPDLOG_FMT_EXTERNAL：
#   spdlog 依赖 fmt（另一个 C++ 格式化库）来格式化字符串。
#   设为 OFF 表示使用 spdlog 内嵌的 fmt 副本，不需要额外安装 fmt。
#   （若设为 ON，则需要额外 find_package(fmt)）
# CACHE BOOL "" FORCE：
#   CACHE BOOL：变量类型为布尔，存入 CMake 缓存
#   ""：空描述文本
#   FORCE：强制覆盖 CMakeCache.txt 中可能已有的值
set(SPDLOG_FMT_EXTERNAL OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/spdlog)

# --- nlohmann/json JSON 解析库 ---
# nlohmann/json 是一个"JSON for Modern C++"库，语法简洁。
# 它是纯头文件库（header-only），只需一个 json.hpp 头文件，
# 不需要编译任何 .cpp。
#
# INTERFACE IMPORTED 库：
#   - INTERFACE 表示这是一个纯头文件库，没有编译产物（.a/.so/.dylib）。
#   - 其他目标链接它时，只获取 include 路径，不增加链接产物。
#   - 类似于 CMake 3.x 的 target_include_directories，但更符合现代 CMake 实践。
add_library(nlohmann_json INTERFACE)

# target_include_directories：为 nlohmann_json 库设置头文件搜索路径
# INTERFACE 关键字表示：
#   - 这个路径仅提供给"使用者"（链接了本库的目标），不用于本库自己
#   - 因为 INTERFACE 库本身没有编译过程，只需要传递 include 路径
target_include_directories(nlohmann_json
    INTERFACE
        third_party/nlohmann_json/include  # nlohmann_json 的头文件目录
)

# --- Catch2 测试框架 ---
# Catch2 是一个现代 C++ 单元测试框架，语法类似 BDD 风格的自然语言。
# add_subdirectory 会定义 Catch2::Catch2WithMain 目标（自带 main 函数）。
# 引入后，在 tests/CMakeLists.txt 中可以用 target_link_libraries 链接。
add_subdirectory(third_party/Catch2)

# --- SQLite 嵌入式数据库 ---
# SQLite 是世界最广泛部署的嵌入式关系数据库，零配置，库文件不到 1MB。
# 本项目使用官方提供的 amalgamation（合并版）：
#   把成千上万个 .c 文件合并到一个 sqlite3.c 中，方便编译。
# 官方仓库只提供 Makefile（Unix）和 Makefile.msc（Windows），不含 CMakeLists.txt，
# 所以需要手动用 add_library 直接编译 sqlite3.c。
#
# STATIC 表示编译为静态库（.a 文件），会将 sqlite3 的目标代码嵌入到最终可执行文件中。
# 静态库的好处：发布时不需要附带 sqlite3.dylib 文件，单文件即可运行。
add_library(sqlite3 STATIC
    third_party/sqlite3/sqlite3.c    # SQLite 的合并 C 源文件（全量实现）
    third_party/sqlite3/sqlite3.h    # SQLite 的头文件（声明接口）
)

# 设置 sqlite3 库的头文件搜索路径
# PUBLIC 关键字：使用 sqlite3 的目标也需要这个路径来 #include "sqlite3.h"
# 这样在 main.cpp 中写 #include <sqlite3.h> 就能找到 third_party/sqlite3/sqlite3.h
target_include_directories(sqlite3
    PUBLIC
        third_party/sqlite3          # 包含 sqlite3.h 的目录
)

# 设置 sqlite3 的编译宏定义
# PRIVATE 关键字：这些宏仅影响 sqlite3.c 的编译，不会泄露给链接了 sqlite3 的目标
target_compile_definitions(sqlite3
    PRIVATE
        # SQLITE_THREADSAFE=1：启用 SQLite 的线程安全模式（默认值，这里显式确认）。
        #   - 0：单线程，无互斥锁，最快但不安全
        #   - 1：序列化模式（默认），多线程安全，各线程可任意使用同一个数据库连接
        #   - 2：多线程模式，允许多线程同时读但排他写入
        #   P2 阶段使用 WAL（Write-Ahead Logging）模式需要线程安全支持，因此设为 1。
        SQLITE_THREADSAFE=1

        # SQLITE_ENABLE_FTS5：启用全文搜索引擎（FTS5）
        # FTS5 允许对音乐元数据进行中文/英文全文搜索，例如：
        #   SELECT * FROM tracks WHERE tracks MATCH '周杰伦 晴天';
        # P2 音乐管理阶段将使用此功能。
        SQLITE_ENABLE_FTS5
)

# 禁用 SQLite 的编译警告（第三方代码，我们无法也不应该修改）
# -w 标志会关闭编译器产生的所有警告
# 只在 Apple（Clang）平台上执行（MSVC 的等效标志是 /w）
if(APPLE)
    target_compile_options(sqlite3 PRIVATE -w)
endif()

# ============================================================
# 子目录
# ============================================================
# add_subdirectory() 让 CMake 进入子目录并解析其中的 CMakeLists.txt
# 子目录可以继承父目录的变量，但通过 PARENT_SCOPE 才能向父级回传变量

# src/ 包含了项目的所有源代码（各层模块 + CLI 程序入口）
add_subdirectory(src)

# tests/ 包含了单元测试代码（基于 Catch2 框架）
add_subdirectory(tests)
