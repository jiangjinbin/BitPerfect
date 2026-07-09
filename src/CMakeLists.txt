# ============================================================
# BitPerfect 源代码 —— 根级别 CMakeLists.txt
# ============================================================
# 职责：收集各层架构的源文件，定义构建目标（CLI 程序 / GUI 应用）
#
# 本文件是 src/ 目录下所有源代码的统一管理中心。它负责：
#   1. 通过 configure_file 在编译时生成版本号头文件
#   2. 调用 add_subdirectory 收集各层的源文件列表
#   3. 定义可执行文件目标（P0: CLI 桩程序，P1: GUI 应用）
#   4. 链接第三方依赖库
#   5. 配置 macOS 平台特有选项（框架链接）
# ============================================================

# ============================================================
# 1. 版本号头文件（由 CMake configure_file 生成）
# ============================================================
# configure_file 的作用：读取模板文件 → 替换 @变量@ → 输出到指定路径
#
# 第一个参数：输入路径（模板文件，我们手写的内容）
# 第二个参数：输出路径（生成的文件，放在 build 目录下，不污染源码目录）
#             - ${CMAKE_CURRENT_BINARY_DIR} = build/debug/src/
#             - 这样的路径设计符合"源码和构建产物分离"的最佳实践
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/version.h.in    # 模板文件：src/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/version.h       # 生成文件：build/debug/src/version.h
)

# ============================================================
# 2. 收集各层源文件
# ============================================================
# 通过 add_subdirectory 进入各层的 CMakeLists.txt
# 每个子 CMakeLists.txt 声明自己负责的 .h/.cpp 文件列表
# 文件列表通过 PARENT_SCOPE 关键字回传到本文件的变量中

# --- 基础设施层（infrastructure）---
# 职责：Logger、Config、Database、Threading、Platform（平台抽象）
add_subdirectory(infrastructure)

# --- 领域层（domain）---
# 职责：AudioEngine、Decoder、Transport、Library、Playlist（核心业务逻辑）
add_subdirectory(domain)

# --- 应用层（application）---
# P0 阶段此目录无源代码，暂不调用 add_subdirectory。
# P1 阶段取消下面注释以启用到应用层的导航：
# add_subdirectory(application)

# --- UI 层（ui）---
# P0 阶段此目录无源代码，暂不调用 add_subdirectory。
# P1 阶段取消下面注释以启用到 UI 层的导航：
# add_subdirectory(ui)

# ============================================================
# 3. CLI 测试程序目标（P0 阶段唯一构建目标）
# ============================================================
# BitPerfectCli 是 P0 阶段的命令行测试程序，用于验证：
#   - CMake 构建系统配置正确
#   - 全部 5 个第三方库链接正常
#   - 版本号生成模板工作正常
# P1 阶段此目标将被替换为 BitPerfectApp（基于 JUCE 的 GUI 应用）

# ============================================================
# 为什么使用 add_executable 而不是 juce_add_console_app？
# ============================================================
# JUCE 8.0.14 提供的 juce_add_console_app() 宏内部调用 add_executable(${target})
# 时不传入任何源文件，这在 CMake 3.x 中是合法的（可以后续用 target_sources 补充）。
# 但 CMake 4.x（本项目使用的 4.3.4）不再允许此行为，会直接报错：
#   "No SOURCES given to target: BitPerfectCli"
#
# 解决方案：P0 阶段使用标准 add_executable，手动传入源文件并手动链接库。
#          这种方式完全等同，只是少了 JUCE 的自动代码签名和包路径设置，
#          这些在 P0 CLI 阶段不需要。P1 切换 GUI 时可升级 JUCE 解决此问题。
# ============================================================

add_executable(BitPerfectCli
    cli/main.cpp                        # CLI 入口程序（P0 唯一源码）
)

# ============================================================
# 3.5 将领域层源文件加入构建目标
# ============================================================
# add_subdirectory(domain) 通过 PARENT_SCOPE 将解码器等模块的源文件
# 汇总到 BP_DOMAIN_SOURCES 变量。通过 target_sources 将这些文件
# 加入可执行目标的编译列表，使它们参与编译和链接。
target_sources(BitPerfectCli PRIVATE ${BP_DOMAIN_SOURCES})

# ============================================================
# 4. 链接第三方库
# ============================================================
# target_link_libraries 分为三种链接范围：
#   PRIVATE：仅 BitPerfectCli 自己使用，不传递给依赖它的目标
#   PUBLIC：依赖本目标的其他目标也能使用这些库的头文件/符号
#   INTERFACE：自己不使用，仅传递给依赖方
#
# P0 阶段的 BitPerfectCli 是最终的可执行文件，没有其他目标依赖它，
# 所以全部使用 PRIVATE（最小暴露原则）。

target_link_libraries(BitPerfectCli PRIVATE
    juce_core                           # JUCE 核心模块：提供 juce::String、juce::File、
                                        #               juce::Thread、juce::AbstractFifo 等基础类
    juce_audio_basics                   # JUCE 音频基础模块：提供 juce::AudioBuffer<float>
                                        #               AudioDecoder 用于单帧解码临时缓冲
    juce_audio_formats                  # JUCE 音频格式模块：提供 juce::AudioFormatManager、
                                        #               juce::AudioFormatReader，用于音频文件解析
    juce_events                         # JUCE 事件模块：提供 juce::ListenerList，
                                        #               AudioDecoder 用于管理 Listener 列表
    spdlog::spdlog                      # 日志输出库：比 std::cout 更强大，支持多级日志、
                                        #             彩色输出、线程安全、异步日志
    nlohmann_json                       # JSON 解析库：一行代码即可解析/生成 JSON 字符串
    sqlite3                             # 嵌入式数据库：P2 阶段用于存储音乐元数据索引，
                                        #              P0 提前链接确保集成正常
    Threads::Threads                    # 系统线程库：提供 -pthread 链接标志（Linux 必须），
                                        #              macOS 自动委托给 libSystem（无额外标志）
                                        #              AudioDecoder 在 startDecoding/stopDecoding 中使用 std::thread
)

# ============================================================
# 5. 添加 include 目录
# ============================================================
# CMake 的 include 目录决定了 #include 指令的搜索路径。
# 默认情况下，只有编译目标所在目录会作为 include 路径。
#
# 加上 ${CMAKE_CURRENT_SOURCE_DIR}（即 src/）后，代码中的 include 可以这样写：
#   #include "infrastructure/logger/Logger.h"
#   #include "domain/audio_engine/AudioEngine.h"
#
# 这比相对路径写法清晰得多：
#   #include "../../infrastructure/logger/Logger.h"    ← 不推荐
#
# PRIVATE：这些 include 路径仅 BitPerfectCli 编译时使用

target_include_directories(BitPerfectCli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# 让生成的 version.h（位于 build/debug/src/）能被 #include "version.h" 找到
# ${CMAKE_CURRENT_BINARY_DIR} 是 build 目录中 src/ 子目录的位置
target_include_directories(BitPerfectCli PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# ============================================================
# 6. macOS 平台特定框架链接
# ============================================================
# macOS 的系统 API 被组织为 框架（Framework），每个框架包含一组相关的头文件和动态库。
# 使用 -framework <FrameworkName> 语法让链接器解析框架中的符号。

if(APPLE)
    # CoreAudio：macOS 底层音频 I/O API
    #   - 音频设备发现和枚举（AudioHardwareService）
    #   - 独占模式（Hog Mode）：阻止其他应用使用音频设备
    #   - 采样率/位深切换
    #   - AudioStreamBasicDescription 配置
    #   这是实现 bit-perfect 音频输出的核心依赖
    target_link_libraries(BitPerfectCli PRIVATE
        "-framework CoreAudio"
    )

    # AudioToolbox：音频工具集
    #   - 音频格式转换服务
    #   - 音频文件流解析
    #   - 音频编解码器接口
    target_link_libraries(BitPerfectCli PRIVATE
        "-framework AudioToolbox"
    )

    # CoreFoundation：C 语言级别的 Foundation 对象
    #   - CFString、CFDictionary、CFArray 等 Core Foundation 对象
    #   - 大部分 CoreAudio API 的参数使用 CFString 而非 C 字符串
    #   - RunLoop 管理（音频回调依赖 RunLoop 来投递事件）
    #   JUCE 内部也会调用 CoreFoundation
    target_link_libraries(BitPerfectCli PRIVATE
        "-framework CoreFoundation"
    )

    # ============================================================
    # P1 阶段注意事项（ObjC++ 源文件处理）
    # ============================================================
    # 基础设施层的 macOS 平台代码（.mm 文件）需要用 Objective-C++ 编译器编译。
    # .mm 文件包含 C++ 和 Objective-C 的混合代码，用于直接调用 macOS 系统框架。
    # P1 阶段需要取消下面的注释，让 CMake 知道这些文件需要特殊处理：
    #
    # set_source_files_properties(
    #     ${BP_INFRASTRUCTURE_SOURCES}
    #     PROPERTIES
    #         COMPILE_FLAGS "-x objective-c++"
    # )
endif()
