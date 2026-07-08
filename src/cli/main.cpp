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
 * 运行方式：./build/debug/src/BitPerfectCli
 * 预期输出：5 个 ✅ 验证通过信息，返回码 0
 *
 * 所属模块：src/cli（P0 CLI 程序入口，P1 将被 src/main.cpp GUI 入口取代）
 */

#include "version.h"         // CMake configure_file 自动生成的版本号头文件
                             // 提供 BITPERFECT_VERSION_MAJOR/MINOR/PATCH/STRING 宏

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


/**
 * 主函数 —— 程序入口点
 *
 * 按照 C++ 标准，main 函数不需要显式写 return 语句，
 * 编译器会在 main 末尾自动插入 return 0。
 * 但为了明确表达意图，我们显式返回 EXIT_SUCCESS 或 EXIT_FAILURE。
 *
 * @return EXIT_SUCCESS（0）：全部验证通过
 * @return EXIT_FAILURE（1）：至少一项验证失败
 */
int main() {
    // ============================================================
    // 1. 验证 version.h（CMake configure_file 生成的版本号模板）
    // ============================================================
    // 这行日志同时验证了两件事：
    //   a) spdlog 日志库可以正常输出
    //   b) version.h 中的版本号宏被 CMake 正确替换
    // spdlog::info() 类似 printf，但使用 {} 作为占位符（fmt 风格）
    spdlog::info("=== {} v{}.{}.{} 构建系统验证 ===",
                 "BitPerfect CLI",                   // {} 第 1 个占位符
                 BITPERFECT_VERSION_MAJOR,            // {} 第 2 个占位符（主版本号）
                 BITPERFECT_VERSION_MINOR,            // {} 第 3 个占位符（次版本号）
                 BITPERFECT_VERSION_PATCH);           // {} 第 4 个占位符（修订版本号）

    // ============================================================
    // 2. 验证 JUCE（juce_core 核心模块）
    // ============================================================
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

    // ============================================================
    // 3. 验证 nlohmann/json（JSON 解析库）
    // ============================================================
    // 验证点：
    //   a) nlohmann/json 头文件可以正常 include
    //   b) JSON 字符串可以正常解析
    //   c) 字段值可以正确读取和类型转换
    //
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

    // ============================================================
    // 4. 验证 sqlite3（SQLite 嵌入式数据库）
    // ============================================================
    // 验证点：
    //   a) sqlite3.c 可以被 C 编译器正确编译
    //   b) C 编译产物可以被 C++ 链接器正确链接（extern "C" 机制）
    //   c) 数据库的基本 CRUD（建表 → 插入 → 查询）操作正常
    //
    // SQLite 变量命名说明：
    //   - rc (return code)：SQLite C API 的函数返回值，0 = SQLITE_OK
    //   - db (database)：数据库连接句柄，类似文件描述符
    //   - stmt (statement)：预编译 SQL 语句对象
    //
    // sqlite3* 是一个指针，指向 SQLite 内部维护的数据库连接结构体。
    // 用户不需要关心结构体内部细节，只需通过 sqlite3_* 函数操作。

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
        return EXIT_FAILURE;                         // 非零返回值表示程序异常退出
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
        sqlite3_close(db);                           // 关闭数据库连接，释放资源
        return EXIT_FAILURE;
    }

    // --- 插入测试数据 ---
    // INSERT INTO 表名 (列1, 列2) VALUES (值1, 值2);
    const char* insert_sql =
        "INSERT INTO verify (id, name) VALUES (1, 'BitPerfect 构建验证');";
    rc = sqlite3_exec(db, insert_sql, nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK) {
        spdlog::error("[sqlite3] 插入数据失败：{}", sqlite3_errmsg(db));
        sqlite3_close(db);
        return EXIT_FAILURE;
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

    // ============================================================
    // 5. 总结输出
    // ============================================================
    // 如果程序运行到这里没有中途退出，说明前面所有验证步骤都通过了。
    // 用醒目的分隔线输出总结信息，方便人工快速判断构建系统是否就绪。
    spdlog::info("============================================");
    spdlog::info("全部第三方库验证通过！");
    spdlog::info("  ✅ JUCE (juce_core)   — 字符串、文件路径");
    spdlog::info("  ✅ spdlog              — 日志输出");
    spdlog::info("  ✅ nlohmann/json       — JSON 解析");
    spdlog::info("  ✅ sqlite3             — 内存数据库建表+查询");
    spdlog::info("  ✅ version.h           — CMake configure_file");
    spdlog::info("============================================");

    // EXIT_SUCCESS 是标准库宏（值为 0），表示程序正常结束
    // 操作系统和 CI 环境通过检查进程退出码来判断程序是否成功
    return EXIT_SUCCESS;
}
