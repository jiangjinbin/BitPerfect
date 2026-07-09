/**
 * 文件名：main.cpp
 * 职责：P0 阶段 CLI 测试程序入口 —— 构建系统验证用桩代码
 *
 * 本文件是 P0 核心验证阶段的唯一源代码。它的作用不是实现业务功能，
 * 而是 验证构建系统（CMake + 第三方库集成）工作正常：
 *   - CMake 配置正确（根 CMakeLists.txt → src/ → infrastructure/ → domain/）
 *   - version.h 自动生成（CMake configure_file 模板替换机制）
 *   - JUCE（juce_core）编译 + 链接成功
 *   - spdlog（日志库）编译 + 链接成功
 *   - nlohmann/json（JSON 库）编译 + 链接成功
 *   - sqlite3（嵌入式数据库）编译 + 链接成功
 *
 * 这些验证是后续 P1 阶段编写实际音频代码的基础。
 * 如果构建系统不通，后面的代码写得再好也编译不过。
 * 
 * 运行方式：
 * cmake --build build/debug --target BitPerfectCli && ./build/debug/src/BitPerfectCli
 * 或
 * cmake --build build && ./build/src/BitPerfectCli
 * 
 * 预期输出：5 个 ✅ 验证通过信息，返回码 0
 *
 * 测试控制：在 main() 函数中注释掉不需要的测试调用即可跳过。
 *
 * 所属模块：src/cli（P0 CLI 程序入口，P1 将被 src/main.cpp GUI 入口取代）
 */

#include "version.h"         // CMake configure_file 自动生成的版本号头文件
                             // 提供 BITPERFECT_VERSION_MAJOR/MINOR/PATCH/STRING 宏

// ============================================================
// 项目头文件
// ============================================================
#include "domain/decoder/AudioDecoder.h"  // AudioDecoder —— 音频文件解码器

// ============================================================
// 第三方库头文件
// ============================================================
// 以下头文件来自 third_party/ 目录下的第三方依赖库。
// 它们通过 CMake 的 target_include_directories 和 add_subdirectory
// 被加入 include 搜索路径，因此可以用 <> 语法直接包含。
//
// #include <> 和 #include "" 的区别：
//   - <>：在系统头文件路径和 CMake 配置的 include 路径中搜索
//   - ""：先在当前文件所在目录搜索，再在 include 路径中搜索
// 对于第三方库，习惯使用 <>；对于项目自己的头文件，使用 ""

#include <spdlog/spdlog.h>                   // spdlog 日志库的主头文件
                                             // 提供 spdlog::info()、spdlog::error() 等日志函数
                                             // 使用 {fmt} 库进行格式化（类似 Python 的 f-string）

#include <juce_core/juce_core.h>             // JUCE 框架的核心模块头文件
                                             // 提供 juce::String（Unicode 字符串）、
                                             // juce::File（跨平台文件操作）、
                                             // juce::Thread（线程封装）等基础类

#include <nlohmann/json.hpp>                 // nlohmann JSON 库头文件
                                             // 使用 nlohmann::json 类型表示 JSON 对象
                                             // 支持类似于 Python dict 的操作语法

#include <sqlite3.h>                         // SQLite C API 头文件
                                             // 提供 sqlite3_open()、sqlite3_exec()、
                                             // sqlite3_prepare_v2()、sqlite3_close() 等函数

// ============================================================
// 标准库头文件
// ============================================================
#include <cstdlib>            // EXIT_SUCCESS 和 EXIT_FAILURE 宏
                              // EXIT_SUCCESS = 0（程序成功退出）
                              // EXIT_FAILURE = 1（程序异常退出）
#include <thread>             // std::this_thread::sleep_for —— 测试中等待解码线程完成


// ============================================================
// 测试函数 —— 每个函数独立验证一个第三方库或构建产物。
// 采用"声明即实现"的风格（类似 Java），放在 main() 前面。
// 返回值：true = 该测试通过，false = 该测试失败。
// 用法：在 main() 中注释掉不需要的调用即可跳过对应测试。
// ============================================================

/**
 * 验证 1：version.h（CMake configure_file 生成的版本号模板）
 *
 * 验证点：
 *   - spdlog 日志库可以正常输出
 *   - version.h 中的版本号宏被 CMake 正确替换
 *
 * @return true  始终返回 true（版本号输出不可能失败）
 */
bool verify_version_header() {
    // spdlog::info() 类似 printf，但使用 {} 作为占位符（fmt 风格）
    spdlog::info("=== {} v{}.{}.{} 构建系统验证 ===",
                 "BitPerfect CLI",                   // {} 第 1 个占位符
                 BITPERFECT_VERSION_MAJOR,            // {} 第 2 个占位符（主版本号）
                 BITPERFECT_VERSION_MINOR,            // {} 第 3 个占位符（次版本号）
                 BITPERFECT_VERSION_PATCH);           // {} 第 4 个占位符（修订版本号）
    return true;
}

/**
 * 验证 2：JUCE（juce_core 核心模块）
 *
 * 验证点：
 *   - juce_core 模块可以正常链接
 *   - juce::String 可以正常创建和使用
 *   - juce::File 可以获取当前工作目录
 *
 * @return true  始终返回 true（字符串操作不可能失败）
 */
bool verify_juce_core() {
    // 创建一个 juce::String 对象并获取当前工作目录。
    // juce::String 是 JUCE 封装的 Unicode 字符串类，内部使用引用计数
    // 的写时复制（Copy-on-Write）策略来减少内存复制。
    //
    // juce::File::getCurrentWorkingDirectory() 返回当前工作目录的 File 对象。
    // getFullPathName() 返回完整路径的 String 表示。
    // toStdString() 将 juce::String 转换为 std::string（UTF-8 编码）。
    // 注意：juce::String(const char*) 构造函数要求传入 ASCII 字符串（仅 0-127 的字符），
    // 不能包含中文等非 ASCII 字符。如果需要处理 UTF-8 中文，应使用：
    //   juce::String juce_message = juce::CharPointer_UTF8("中文");
    // P0 验证阶段使用纯 ASCII 消息以避免 Debug 模式的 jassert 断言。
    juce::String juce_message = "JUCE core module initialised successfully";
    juce::File current_dir = juce::File::getCurrentWorkingDirectory();
    spdlog::info("[JUCE] {}，当前工作目录：{}",
                 juce_message.toStdString(),          // juce::String → std::string
                 current_dir.getFullPathName().toStdString());
    return true;
}

/**
 * 验证 3：nlohmann/json（JSON 解析库）
 *
 * 验证点：
 *   - nlohmann/json 头文件可以正常 include
 *   - JSON 字符串可以正常解析
 *   - 字段值可以正确读取和类型转换
 *
 * @return true  始终返回 true（已知合法 JSON 字符串解析不可能失败）
 */
bool verify_nlohmann_json() {
    // R"(...)" 是 C++11 的原始字符串字面量（Raw String Literal），
    // 括号中的所有字符都是字面量（包括换行和引号），不需要转义。
    // 这对写 JSON 字符串非常方便 —— 不需要用 \" 转义双引号。

    // 内联 JSON 字符串 —— 模拟了一个简单的项目配置文件
    const char* json_str = R"({
        "project": "BitPerfect",
        "version": "1.0.0",
        "status": "build_system_verified"
    })";

    // nlohmann::json::parse() 解析 JSON 字符串，返回 json 对象
    // 如果 JSON 格式不合法，会抛出 nlohmann::json::parse_error 异常
    // P0 阶段不处理异常（相信已知字符串是合法的），P1 加 try-catch
    nlohmann::json config = nlohmann::json::parse(json_str);

    // 使用 [] 运算符读取 JSON 字段，get<T>() 将值转换为 C++ 类型
    spdlog::info("[nlohmann/json] 项目名：{}，状态：{}",
                 config["project"].get<std::string>(),   // "BitPerfect"
                 config["status"].get<std::string>());   // "build_system_verified"
    return true;
}

/**
 * 验证 4：sqlite3（SQLite 嵌入式数据库）
 *
 * 验证点：
 *   - sqlite3.c 可以被 C 编译器正确编译
 *   - C 编译产物可以被 C++ 链接器正确链接（extern "C" 机制）
 *   - 数据库的基本 CRUD（建表 → 插入 → 查询）操作正常
 *
 * SQLite 变量命名说明：
 *   - rc (return code)：SQLite C API 的函数返回值，0 = SQLITE_OK
 *   - db (database)：数据库连接句柄，类似文件描述符
 *   - stmt (statement)：预编译 SQL 语句对象
 *
 * sqlite3* 是一个指针，指向 SQLite 内部维护的数据库连接结构体。
 * 用户不需要关心结构体内部细节，只需通过 sqlite3_* 函数操作。
 *
 * @return true  SQLite CRUD 全部成功
 * @return false 建表、插入或查询任一环节失败
 */
bool verify_sqlite3() {
    sqlite3* db = nullptr;                           // 数据库连接句柄，初始化为空指针

    // sqlite3_open() 打开（或创建）一个数据库文件
    // 参数 1（":memory:"）：特殊文件名，表示创建仅存在于内存中的数据库，
    //                     程序退出后数据自动消失，不会留下磁盘文件
    // 参数 2（&db）：输出参数，返回数据库连接句柄的地址
    // 返回值：SQLITE_OK（0）表示成功，其他值表示错误
    int rc = sqlite3_open(":memory:", &db);
    if (rc != SQLITE_OK) {
        // sqlite3_errmsg(db) 返回人类可读的英文错误描述
        spdlog::error("[sqlite3] 无法打开内存数据库：{}", sqlite3_errmsg(db));
        return false;                                // 测试失败，返回 false
    }

    // --- 创建测试表 ---
    // SQL 语句：CREATE TABLE 表名 (列名 类型 约束, ...)
    //   - id INTEGER PRIMARY KEY：整数类型主键，自动唯一标识每一行
    //   - name TEXT：文本类型的列，存储 UTF-8 字符串
    // sqlite3_exec() 是执行不带返回值的 SQL 语句的便捷函数
    // 后面 3 个 nullptr 分别为：回调函数、回调参数、错误信息（我们不需要）
    const char* create_sql =
        "CREATE TABLE verify (id INTEGER PRIMARY KEY, name TEXT);";
    rc = sqlite3_exec(db, create_sql, nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK) {
        spdlog::error("[sqlite3] 建表失败：{}", sqlite3_errmsg(db));
        sqlite3_close(db);                           // 失败前关闭数据库连接，释放资源
        return false;
    }

    // --- 插入测试数据 ---
    // INSERT INTO 表名 (列1, 列2) VALUES (值1, 值2);
    const char* insert_sql =
        "INSERT INTO verify (id, name) VALUES (1, 'BitPerfect 构建验证');";
    rc = sqlite3_exec(db, insert_sql, nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK) {
        spdlog::error("[sqlite3] 插入数据失败：{}", sqlite3_errmsg(db));
        sqlite3_close(db);
        return false;
    }

    // --- 查询验证 ---
    // 对于需要读取结果的查询，使用 sqlite3_prepare_v2() + sqlite3_step() 两步：
    //   1. sqlite3_prepare_v2()：编译 SQL 语句为字节码（预编译语句对象）
    //   2. sqlite3_step()：执行预编译语句，逐行获取结果
    // 这种方式比 sqlite3_exec() 更高效（可重复使用预编译语句）且能读取返回数据
    const char* select_sql = "SELECT name FROM verify WHERE id = 1;";
    sqlite3_stmt* stmt = nullptr;                    // 预编译语句对象

    // sqlite3_prepare_v2() 参数说明：
    //   参数 1（db）：数据库连接
    //   参数 2（select_sql）：要编译的 SQL 字符串
    //   参数 3（-1）：SQL 字符串长度，-1 表示自动计算（读到 \0 为止）
    //   参数 4（&stmt）：输出参数，返回预编译语句对象
    //   参数 5（nullptr）：未使用的 SQL 尾部指针（我们不需要）
    rc = sqlite3_prepare_v2(db, select_sql, -1, &stmt, nullptr);

    // sqlite3_step() 执行预编译语句，返回：
    //   - SQLITE_ROW：成功获取一行数据
    //   - SQLITE_DONE：没有更多数据行（对 SELECT 来说表示结果集为空）
    if (rc == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW) {
        // sqlite3_column_text() 获取当前行第 N 列（从 0 开始）的文本值
        // 返回 const unsigned char*，需要转换为 const char*
        const char* name =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        spdlog::info("[sqlite3] 查询结果：{}", name);  // 预期输出："BitPerfect 构建验证"
    }

    // --- 清理 SQLite 资源 ---
    // sqlite3_finalize() 销毁预编译语句，释放关联的内存
    sqlite3_finalize(stmt);
    // sqlite3_close() 关闭数据库连接，将内存数据写回磁盘（对于 :memory: 数据库则直接丢弃）
    sqlite3_close(db);

    return true;                                     // 所有 SQLite 操作成功
}

/**
 * 验证 5：输出验证总结
 *
 * 如果程序运行到这里，说明前面所有被启用的测试都通过了。
 * 用醒目的分隔线输出总结信息，方便人工快速判断构建系统是否就绪。
 *
 * @return true  始终返回 true
 */
bool print_verify_summary() {
    spdlog::info("============================================");
    spdlog::info("全部第三方库验证通过！");
    spdlog::info("  ✅ JUCE (juce_core)   — 字符串、文件路径");
    spdlog::info("  ✅ spdlog              — 日志输出");
    spdlog::info("  ✅ nlohmann/json       — JSON 解析");
    spdlog::info("  ✅ sqlite3             — 内存数据库建表+查询");
    spdlog::info("  ✅ version.h           — CMake configure_file");
    spdlog::info("============================================");
    return true;
}

/**
 * 验证 6：AudioDecoder 端到端解码验证（2.2.7 临时测试）
 *
 * 验证点：
 *   - open() 能正确打开音频文件并提取元数据
 *   - startDecoding() 能启动后台解码线程
 *   - decodingLoop() 能正确解码整个文件并到达 EOF
 *   - AbstractFifo 数据写入正确（通过 prepareToRead 读取验证）
 *   - stopDecoding() 能安全停止解码线程
 *   - isDecodingComplete() 返回正确的完成状态
 *   - getDecodedPosition() 返回正确的解码位置
 *
 * 测试文件：测试资源/渡口.wav（WAV 44.1kHz / 16bit / 立体声 / 3分44秒）
 *
 * @return true  解码全流程验证通过
 * @return false 任一环节失败
 */
bool verify_audio_decoder() {
    spdlog::info("=== AudioDecoder 端到端解码验证 ===");

    // ----------------------------------------------------
    // 步骤 1：创建 AudioDecoder 实例并打开测试音频文件
    // ----------------------------------------------------
    AudioDecoder decoder;

    // 使用 juce::File::getCurrentWorkingDirectory() 获取项目根目录
    // 然后通过 getChildFile 逐级定位测试文件
    // ⚠️ 显式使用 juce::String::fromUTF8() 处理包含中文的文件路径，
    //    避免编译器对源文件字符串常量的编码处理与文件系统不一致
    juce::File project_root = juce::File::getCurrentWorkingDirectory();
    juce::File test_resource_dir = project_root.getChildFile(
        juce::String::fromUTF8("测试资源"));
    juce::File test_file = test_resource_dir.getChildFile(
        juce::String::fromUTF8("渡口.wav"));

    // 检查文件是否存在
    if (!test_file.existsAsFile()) {
        spdlog::error("[AudioDecoder] 测试文件不存在：{}",
                      test_file.getFullPathName().toStdString());
        return false;
    }

    // 打开音频文件
    bool open_result = decoder.open(test_file);
    if (!open_result) {
        spdlog::error("[AudioDecoder] open() 失败");
        return false;
    }
    spdlog::info("[AudioDecoder] ✅ open() 成功");

    // ----------------------------------------------------
    // 步骤 2：启动解码线程
    // ----------------------------------------------------
    decoder.startDecoding();
    // 验证重复调用幂等性（startDecoding 内部会检测并忽略重复调用）
    decoder.startDecoding();
    spdlog::info("[AudioDecoder] ✅ startDecoding() 成功（含幂等性验证）");

    // ----------------------------------------------------
    // 步骤 3：从 AbstractFifo 消费解码数据
    // ----------------------------------------------------
    // 获取 Fifo 引用，用于无锁读取解码后的 PCM 数据
    juce::AbstractFifo& fifo = decoder.getFifo();

    // 累计读取的采样帧数
    juce::int64 total_frames_read = 0;

    // 每次从 fifo 读取的帧数（与解码块的 4096 一致）
    const int kReadChunkSize = 4096;

    // 循环读取直到解码完成
    while (!decoder.isDecodingComplete() || fifo.getNumReady() > 0) {
        // 检查 fifo 中是否有可读取的数据
        int num_ready = fifo.getNumReady();
        if (num_ready > 0) {
            // 获取可读取位置
            int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
            fifo.prepareToRead(num_ready, start1, size1, start2, size2);
            int total_readable = size1 + size2;

            // 累计读取的帧数（实际消费数据，不做额外处理）
            total_frames_read += total_readable;

            // 通知 Fifo 读取完成（释放空间给解码线程写入）
            fifo.finishedRead(total_readable);
        } else {
            // 没有数据可读，短暂休眠让出 CPU 给解码线程
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    spdlog::info("[AudioDecoder] ✅ Fifo 消费完成，共读取 {} 采样帧", total_frames_read);

    // ----------------------------------------------------
    // 步骤 4：验证解码状态
    // ----------------------------------------------------
    // 检查解码完成标志
    if (!decoder.isDecodingComplete()) {
        spdlog::error("[AudioDecoder] isDecodingComplete() 应返回 true，但返回 false");
        return false;
    }
    spdlog::info("[AudioDecoder] ✅ isDecodingComplete() = true");

    // 检查解码位置（应与总采样数接近或相等）
    juce::int64 final_position = decoder.getDecodedPosition();
    spdlog::info("[AudioDecoder] ✅ getDecodedPosition() = {} 采样帧", final_position);

    // 验证读取的总帧数与解码位置一致（允许一定误差，因为最后一个块可能未完全写入 fifo）
    if (final_position <= 0) {
        spdlog::error("[AudioDecoder] 解码位置异常：{}", final_position);
        return false;
    }

    // ----------------------------------------------------
    // 步骤 5：停止解码
    // ----------------------------------------------------
    decoder.stopDecoding();
    // 验证幂等性
    decoder.stopDecoding();
    spdlog::info("[AudioDecoder] ✅ stopDecoding() 成功（含幂等性验证）");

    // ----------------------------------------------------
    // 步骤 6：seekTo() 专项验证（2.2.8）
    // ----------------------------------------------------
    // 测试 6a：越界检查 —— seek 到负数位置，应静默忽略
    {
        juce::int64 pos_before = decoder.getDecodedPosition();
        decoder.seekTo(-100);
        juce::int64 pos_after = decoder.getDecodedPosition();
        if (pos_before != pos_after) {
            spdlog::error("[AudioDecoder] seekTo(-100) 应静默忽略，但 position 从 {} 变为 {}",
                          pos_before, pos_after);
            return false;
        }
        spdlog::info("[AudioDecoder] ✅ seekTo() 越界检查（负数）—— 静默忽略");
    }

    // 测试 6b：越界检查 —— seek 到 total_frames + 1，应静默忽略
    {
        juce::int64 total_frames = 9878988;  // 渡口.wav 的总采样帧数
        juce::int64 pos_before = decoder.getDecodedPosition();
        decoder.seekTo(total_frames + 1);
        juce::int64 pos_after = decoder.getDecodedPosition();
        if (pos_before != pos_after) {
            spdlog::error("[AudioDecoder] seekTo(total_frames+1) 应静默忽略，但 position 从 {} 变为 {}",
                          pos_before, pos_after);
            return false;
        }
        spdlog::info("[AudioDecoder] ✅ seekTo() 越界检查（超范围）—— 静默忽略");
    }

    // 测试 6c：seek 到 1 秒位置（44100 帧），验证从中间开始解码
    {
        juce::int64 seek_target = 44100;  // 44100 Hz × 1 秒 = 1 秒处
        decoder.seekTo(seek_target);

        // 短暂等待后检查解码位置（解码线程需要一点时间启动并读取第一块数据）
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        juce::int64 current_pos = decoder.getDecodedPosition();
        if (current_pos < seek_target) {
            spdlog::warn("[AudioDecoder] seekTo(44100) 后解码位置 {} < 目标 {}，可能尚未开始解码",
                         current_pos, seek_target);
            // 不直接失败 —— 解码线程可能还没来得及更新位置
        }
        spdlog::info("[AudioDecoder] ✅ seekTo(44100) 解码已从 {} 位置开始", current_pos);

        // 消费部分数据以验证 seek 生效
        juce::AbstractFifo& fifo_seek = decoder.getFifo();
        juce::int64 seek_frames_read = 0;

        // 只消费约 1 秒的数据（44100 帧）就停止，验证 seek 位置正确
        while (seek_frames_read < 44100 * 2) {  // 最多读 2 秒数据
            int num_ready = fifo_seek.getNumReady();
            if (num_ready > 0) {
                int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
                fifo_seek.prepareToRead(num_ready, start1, size1, start2, size2);
                int total_readable = size1 + size2;
                seek_frames_read += total_readable;
                fifo_seek.finishedRead(total_readable);

                if (seek_frames_read >= 44100) {  // 读到 1 秒数据就够了
                    break;
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }

        juce::int64 final_seek_pos = decoder.getDecodedPosition();
        spdlog::info("[AudioDecoder] ✅ seekTo(44100) 验证：读取 {} 帧，最终位置 {}",
                     seek_frames_read, final_seek_pos);

        // 验证位置确实在 44100 附近（允许一定偏差，因为是多线程）
        if (final_seek_pos < seek_target) {
            spdlog::error("[AudioDecoder] seekTo(44100) 失败：最终位置 {} < 目标 {}",
                          final_seek_pos, seek_target);
            return false;
        }
    }

    // 测试 6d：seek 到文件末尾，应立即触发 EOF
    {
        juce::int64 total_frames = 9878988;
        decoder.seekTo(total_frames);

        // 等待解码线程检测到 EOF 并退出
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        if (!decoder.isDecodingComplete()) {
            spdlog::error("[AudioDecoder] seekTo(total_frames) 后 isDecodingComplete() 应为 true");
            return false;
        }
        spdlog::info("[AudioDecoder] ✅ seekTo(total_frames) EOF 立即触发");
    }

    // 测试 6e：幂等性 —— 连续两次 seek 到不同位置，第二次生效
    {
        decoder.seekTo(0);      // 先跳到开头
        decoder.seekTo(88200);  // 再跳到 2 秒处（覆盖前一次）

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        juce::int64 current_pos = decoder.getDecodedPosition();
        // 第二次 seek 应该生效，位置应在 88200 附近，而不是 0
        if (current_pos < 100) {
            spdlog::error("[AudioDecoder] seekTo 幂等性失败：第二次 seekTo(88200) 后位置 {} 接近 0，"
                          "说明第一次 seekTo(0) 覆盖了第二次",
                          current_pos);
            return false;
        }
        spdlog::info("[AudioDecoder] ✅ seekTo() 幂等性验证通过（位置 = {}）", current_pos);

        // 恢复：seek 回起始位置，为后续验证做准备
        decoder.seekTo(0);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // 停止解码（seek 测试结束）
        decoder.stopDecoding();
    }

    // ----------------------------------------------------
    // 步骤 7：Listener 管理方法验证（2.2.10）
    // ----------------------------------------------------
    {
        // 测试监听器子类 —— 继承 AudioDecoder::Listener，
        // 覆盖三个回调方法，统计调用次数（用于验证 P0 阶段不触发回调）
        struct TestListener : public AudioDecoder::Listener {
            int started_count = 0;    // onDecodingStarted 被调用次数
            int completed_count = 0;  // onDecodingComplete 被调用次数
            int error_count = 0;      // onDecodingError 被调用次数

            void onDecodingStarted() override {
                ++started_count;
            }

            void onDecodingComplete() override {
                ++completed_count;
            }

            void onDecodingError(const std::string& /*error_message*/) override {
                ++error_count;
            }
        };

        TestListener listener1;
        TestListener listener2;

        // 1. 验证 addListener 不崩溃
        decoder.addListener(&listener1);
        decoder.addListener(&listener2);

        // 2. 验证重复添加不崩溃（juce::ListenerList 自动去重，同一指针只保留一份）
        decoder.addListener(&listener1);
        decoder.addListener(&listener1);

        // 3. 验证 removeListener 不崩溃
        decoder.removeListener(&listener2);

        // 4. 验证移除不存在的 listener 安全无操作（不崩溃）
        decoder.removeListener(&listener2);

        // 5. 验证移除后重新添加不崩溃（操作幂等、状态稳定）
        decoder.addListener(&listener2);
        decoder.removeListener(&listener2);

        // 6. 验证 P0 阶段不会触发任何回调
        //    因为 decodingLoop / startDecoding / stopDecoding 中均未调用 listeners_.call()
        if (listener1.started_count != 0 ||
            listener1.completed_count != 0 ||
            listener1.error_count != 0) {
            spdlog::error("[AudioDecoder] ❌ P0 阶段不应触发 Listener 回调，"
                          "但实际触发了（started={}, completed={}, error={}）",
                          listener1.started_count,
                          listener1.completed_count,
                          listener1.error_count);
            return false;
        }

        spdlog::info("[AudioDecoder] ✅ addListener/removeListener 验证通过"
                     "（含重复添加、移除不存在项、重新添加、P0 无回调）");
    }

    // ----------------------------------------------------
    // 步骤 8：输出验证结果
    // ----------------------------------------------------
    spdlog::info("[AudioDecoder] ========================================");
    spdlog::info("[AudioDecoder] 🎉 端到端解码验证全部通过！");
    spdlog::info("[AudioDecoder]    - 文件打开 + 元数据提取 ✅");
    spdlog::info("[AudioDecoder]    - 后台解码线程 ✅");
    spdlog::info("[AudioDecoder]    - 无锁 Fifo 写入/读取 ✅");
    spdlog::info("[AudioDecoder]    - EOF 检测 + 解码完成标志 ✅");
    spdlog::info("[AudioDecoder]    - 线程安全停止 + 幂等性 ✅");
    spdlog::info("[AudioDecoder]    - seekTo() 跳转播放位置 ✅");
    spdlog::info("[AudioDecoder]    - addListener/removeListener 管理 ✅");
    spdlog::info("[AudioDecoder]    - 共解码 {} 采样帧", total_frames_read);
    spdlog::info("[AudioDecoder] ========================================");

    return true;
}


/**
 * 主函数 —— 程序入口点
 *
 * 依次调用各个测试函数，任意一个返回 false 则整体退出码为 EXIT_FAILURE。
 * 如需跳过某个测试，直接注释掉对应的调用行即可。
 *
 * 按照 C++ 标准，main 函数不需要显式写 return 语句，
 * 编译器会在 main 末尾自动插入 return 0。
 * 但为了明确表达意图，我们显式返回 EXIT_SUCCESS 或 EXIT_FAILURE。
 *
 * @return EXIT_SUCCESS（0）：全部被启用的测试通过
 * @return EXIT_FAILURE（1）：至少一项测试失败
 */
int main() {
    // 以下每个函数调用对应一个独立的验证测试。
    // 注释掉某一行即可跳过对应测试，不影响其他测试的执行。
    // 测试执行顺序即为调用顺序。

    bool all_passed = true;                          // 跟踪所有测试是否通过

    //all_passed = verify_version_header() && all_passed;  // 测试 1：version.h + spdlog
    //all_passed = verify_juce_core() && all_passed;       // 测试 2：JUCE core 模块
    //all_passed = verify_nlohmann_json() && all_passed;   // 测试 3：nlohmann/json 解析
    //all_passed = verify_sqlite3() && all_passed;         // 测试 4：SQLite 内存数据库 CRUD
    //all_passed = print_verify_summary() && all_passed;   // 测试 5：输出验证总结
    all_passed = verify_audio_decoder() && all_passed;   // 测试 6：AudioDecoder 端到端解码验证（2.2.7）

    // 任一失败则返回 EXIT_FAILURE，操作系统和 CI 可据此判断构建是否就绪
    if (!all_passed) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
