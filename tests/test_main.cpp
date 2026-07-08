/**
 * 文件名：test_main.cpp
 * 职责：Catch2 测试框架集成验证 —— 确认 Catch2 可以正常编译、链接和运行
 *
 * 本文件包含一个极简的测试用例，目的是验证：
 *   1. Catch2 框架头文件可以正常 include
 *   2. Catch2 提供的 main 函数（Catch2::Catch2WithMain）链接正确
 *   3. TEST_CASE 宏可以正常使用
 *
 * 后续 P1+ 阶段会在此文件所在目录（tests/）下添加各模块的单元测试文件。
 *
 * 为什么不需要写 main 函数？
 *   - Catch2::Catch2WithMain 是 Catch2 提供的 CMake 目标，
 *     它内部已经包含了一个完整的 main 函数。
 *   - 此 main 函数负责解析命令行参数（如 -s 显示成功测试输出、--list-tests 列出测试）、
 *     运行所有 TEST_CASE、输出彩色结果报告。
 *   - 我们只需写 TEST_CASE 定义即可。
 *
 * 运行方式：./build/debug/tests/BitPerfectTests
 * 预期输出：All tests passed (1 assertion in 1 test case)
 *
 * 所属模块：tests
 */

// Catch2 测试宏头文件
// 提供了 TEST_CASE、REQUIRE、CHECK、SECTION 等测试宏
// TEST_CASE：定义一个测试用例
// REQUIRE：断言表达式为真，失败则终止当前测试用例
// CHECK：断言表达式为真，失败但继续执行
#include <catch2/catch_test_macros.hpp>

/**
 * 测试用例：Catch2 框架集成验证
 *
 * TEST_CASE 宏参数说明：
 *   第一个参数 "Catch2 框架集成验证" —— 测试用例名称（人类可读的描述文本）
 *   第二个参数 "[smoke]" —— 标签（tag），用方括号包裹，用于分组过滤
 *      - "smoke" 是烟雾测试（smoke test）的习惯标签，表示最基础的验证
 *      - 运行时可以用 ./BitPerfectTests "[smoke]" 只运行带此标签的测试
 *
 * 这个测试只验证最基础的 C++ 算术运算，目的是确认测试框架本身工作正常。
 * 如果连 1+1=2 都测不过，说明 Catch2 的编译或链接出了问题。
 */
TEST_CASE("Catch2 框架集成验证", "[smoke]") {
    // REQUIRE 宏：断言表达式为 true
    // 如果 1+1 != 2，Catch2 会输出：
    //   FAILED:
    //     REQUIRE(1 + 1 == 2)
    //   with expansion:
    //     3 == 2
    // 这样的输出让你能快速定位失败原因
    REQUIRE(1 + 1 == 2);
}

// 注意：
// 本文件不需要写 int main() 函数。
// Catch2::Catch2WithMain 库已经自动提供了 main 函数。
//
// 如果需要自定义 main 函数（例如想在运行测试前做一些初始化），
// 可以改为链接 Catch2::Catch2（不带 WithMain），然后在自己的 main 中
// 调用 Catch2::Session 来运行测试。
