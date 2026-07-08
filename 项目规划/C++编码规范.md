# BitPerfect C++ 编码规范

> 版本：v1.2 | 日期：2026-07-09
>
> 本文档是 BitPerfect 项目的 C++ 代码编写标准，所有开发者及 AI 编码助手必须遵守。
> 目标：保证代码风格统一、可读性优先、降低维护成本。

---

## 一、文件组织

### 1.1 文件命名

- **头文件**：`snake_case.h`（如 `audio_engine.h`、`config_store.h`）
- **源文件**：`snake_case.cpp`（如 `audio_engine.cpp`、`config_store.cpp`）
- **测试文件**：`test_<模块名>.cpp`（如 `test_audio_engine.cpp`）
- **Objective-C++ 文件**：`snake_case.mm`（如 `core_audio_helper.mm`）

### 1.2 目录结构

```
src/
├── infrastructure/     # 基础设施层
│   ├── logger/         # 子目录一律 snake_case
│   ├── config/
│   ├── database/
│   ├── platform/
│   │   └── macos/      # 平台子目录，macOS 特定代码放这里
│   └── threading/
├── domain/             # 领域层
│   ├── audio_engine/
│   ├── decoder/
│   ├── transport/
│   ├── library/
│   ├── playlist/
│   └── models/
├── application/        # 应用层
│   ├── app_controller/
│   ├── player_controller/
│   ├── library_controller/
│   ├── playlist_controller/
│   └── settings_manager/
└── ui/                 # UI 层
    ├── main_window/
    ├── transport_bar/
    ├── now_playing/
    ├── playlist_panel/
    ├── library_browser/
    ├── settings/
    ├── theme/
    └── components/
```

### 1.3 头文件保护

统一使用 `#pragma once`，不使用传统的 `#ifndef` / `#define` / `#endif` 守卫。

```cpp
// ✅ 正确
#pragma once

// ❌ 禁止
#ifndef BITPERFECT_AUDIO_ENGINE_H
#define BITPERFECT_AUDIO_ENGINE_H
// ...
#endif
```

### 1.4 #include 顺序

每个源文件的 include 按以下顺序分组，组间用空行分隔：

1. **对应的头文件**（.cpp 文件的第一条 include 必须是它自己的 .h）
2. **本项目其他模块的头文件**（按模块层级：infrastructure → domain → application → ui）
3. **第三方库头文件**（JUCE → spdlog → 其他）
4. **C++ 标准库头文件**

```cpp
// ✅ 正确示例（AudioEngine.cpp 的 include 部分）
#include "audio_engine.h"                    // 1. 自己的头文件

#include "config/config_store.h"             // 2. 本项目其他模块
#include "threading/fifo_buffer.h"

#include <juce_audio_devices/juce_audio_devices.h>  // 3. 第三方库

#include <atomic>                             // 4. 标准库
#include <memory>
#include <string>
```

### 1.5 前向声明

头文件中能用前向声明的尽量用前向声明，减少编译依赖，缩短编译时间：

```cpp
// ✅ 正确：头文件中前向声明
#pragma once

class Database;  // 前向声明，不需要 #include "database/database.h"

class MusicLibrary {
public:
    void setDatabase(Database* database);  // 指针/引用参数不需要完整定义
private:
    Database* database_ = nullptr;
};

// ❌ 避免：不必要的头文件包含
#pragma once
#include "database/database.h"  // 拖慢编译，且引入了不必要的依赖
```

---

## 二、命名规范

### 2.1 类与结构体 —— PascalCase

```cpp
// ✅ 正确：类名 PascalCase
class AudioEngine { ... };
class ConfigStore { ... };
class PlayerController { ... };

// ✅ 结构体（纯数据载体）也使用 PascalCase
struct TrackInfo { ... };
struct AlbumInfo { ... };
```

### 2.2 函数与方法 —— camelCase

```cpp
// ✅ 正确
void setCurrentDevice(const juce::String& device_name);
double getCurrentSampleRate() const;
bool isHogModeActive() const;
void audioDeviceIOCallbackWithContext(...) override;

// ❌ 禁止：snake_case 函数名
void set_current_device(const juce::String& device_name);  // 风格不统一
```

### 2.3 变量 —— snake_case

局部变量和函数参数统一使用 `snake_case`（全小写 + 下划线分隔），成员变量在此基础上加尾部 `_`。

```cpp
// ✅ 正确：局部变量，snake_case
int num_channels = 2;
double sample_rate = 44100.0;
juce::String device_name = "Built-in Output";

// ✅ 正确：成员变量，snake_case + 尾部下划线（Google C++ Style Guide 规范）
class AudioEngine {
private:
    juce::AudioDeviceManager device_manager_;     // 尾部 _ 区分成员和局部变量
    std::atomic<bool> is_running_{false};         // 原子变量同样遵循此规范
    std::atomic<double> current_sample_rate_{44100.0};
    int audio_source_channels_ = 2;
};

// ✅ 正确：函数参数同样用 snake_case
void setDeviceName(const juce::String& device_name);
bool loadConfig(const juce::File& config_file);

// ❌ 禁止：匈牙利命名法、无标记的成员变量
bool b_is_running;              // 不要匈牙利命名法
int audio_source_channels;      // 缺少尾部 _，与局部变量无法区分
```

### 2.4 常量与枚举

```cpp
// 全局/静态常量：k 前缀 + PascalCase
constexpr double kDefaultSampleRate = 44100.0;
constexpr int kMaxBufferSize = 4096;
constexpr int kFifoCapacitySeconds = 2;

// 枚举值：PascalCase（强类型枚举）
enum class State { Stopped, Playing, Paused, Seeking };
enum class PlayMode { Sequential, RepeatOne, RepeatAll, Shuffle };
```

### 2.5 宏 —— UPPER_SNAKE_CASE

```cpp
// ✅ 项目级宏以 BP_ 为前缀
#define BP_ASSERT_MESSAGE_THREAD()  jassert(juce::MessageManager::getInstance()->isThisTheMessageThread())
#define BP_LOG_ERROR(msg)           SPDLOG_ERROR(msg)
```

### 2.6 命名空间 —— snake_case

```cpp
// ✅ 正确：命名空间名与目录名对应，snake_case
namespace platform_utils {
    juce::File getAppDataDir();
    juce::File getLogsDir();
}

namespace core_audio_helper {
    bool takeHogMode(juce::AudioDeviceID device_id);
    bool releaseHogMode(juce::AudioDeviceID device_id);
}

// 不要在头文件全局范围内写 using namespace
// ✅ 正确：在 .cpp 文件函数体内部可以使用
void someFunction() {
    using namespace core_audio_helper;  // .cpp 的局部作用域中可以使用
}
```

### 2.7 速查表

| 元素 | 风格 | 示例 |
|------|------|------|
| 类/结构体 | PascalCase | `AudioEngine`, `TrackInfo` |
| 函数/方法 | camelCase | `getCurrentSampleRate()`, `onDeviceChanged()` |
| 局部变量/参数 | snake_case | `num_channels`, `sample_rate` |
| 成员变量 | snake_case + 尾部 `_` | `device_manager_`, `is_running_` |
| 全局常量 | k + PascalCase | `kDefaultSampleRate` |
| 枚举值 | PascalCase | `State::Playing`, `PlayMode::Shuffle` |
| 宏 | UPPER_SNAKE_CASE | `BP_ASSERT_MESSAGE_THREAD()` |
| 命名空间 | snake_case | `platform_utils`, `core_audio_helper` |
| 文件名 | snake_case | `audio_engine.h`, `config_store.cpp` |
| 目录名 | snake_case | `audio_engine/`, `main_window/` |
| 变量初始化 | `=` 优先，默认构造例外 | `int x = 0;` 优于 `int x(0);` |

---

## 三、类型使用

### 3.1 禁用 auto 类型推导

**这是 BitPerfect 项目最重要的编码规则之一。** 所有变量必须显式声明类型。

```cpp
// ❌ 禁止：auto 让代码阅读者必须去推断类型
auto device_name = audioEngine.getCurrentDeviceName();
auto sample_rate = config.getValue<double>("audio.sample_rate", 44100.0);
auto reader = std::make_unique<juce::AudioFormatReader>(...);

// ✅ 正确：显式声明类型，一目了然
juce::String device_name = audioEngine.getCurrentDeviceName();
double sample_rate = config.getValue<double>("audio.sample_rate", 44100.0);
std::unique_ptr<juce::AudioFormatReader> reader =
    std::make_unique<juce::AudioFormatReader>(...);
```

**理由**：
1. 显式类型让代码读者无需跳转到定义就能理解变量类型
2. 与 Java 风格一致，降低 Java 开发者的阅读负担
3. 编译错误信息更清晰（模板推导失败的错误信息极难阅读）

### 3.2 禁止所有类型推导语法

```cpp
// ❌ 禁止
auto x = 42;                    // 禁止
decltype(auto) y = someFunc();  // 禁止
auto lambda = [](int a) { };    // 禁止，lambda 用 std::function 声明类型

// ✅ 正确：显式类型
int x = 42;

// lambda 应声明返回类型和参数类型
std::function<void(int)> lambda = [](int a) {
    // lambda 体
};
```

### 3.3 空指针

统一使用 `nullptr`，禁止使用 `NULL` 或 `0`。

```cpp
// ✅ 正确
Database* database_ = nullptr;
std::shared_ptr<ConfigStore> config_ = nullptr;

// ❌ 禁止
Database* database_ = NULL;  // C 风格
Database* database_ = 0;     // 歧义：这是整数 0 还是空指针？
```

### 3.4 类型别名

使用 `using` 声明类型别名，不使用 `typedef`。

```cpp
// ✅ 正确：using 声明，语法与变量声明一致，更直观
using TrackId = int64;
using SampleRate = double;
using Callback = std::function<void(const juce::String&)>;

// ❌ 禁止：typedef 语法，阅读顺序不自然
typedef int64 TrackId;
typedef double SampleRate;
```

### 3.5 强类型枚举

优先使用 `enum class`（C++11 强类型枚举），避免传统 `enum` 的隐式整型转换。

```cpp
// ✅ 正确：enum class，作用域限定，无隐式转换
enum class State { Stopped, Playing, Paused, Seeking };

State current_state = State::Stopped;  // 必须带作用域前缀

// 与整型互转时必须显式 static_cast
int state_value = static_cast<int>(State::Playing);

// ❌ 禁止：传统 enum，值污染外部作用域
enum State { Stopped, Playing, Paused, Seeking };  // Stopped/Playing 等直接暴露
```

### 3.6 变量初始化风格

**变量初始化优先使用 `=` 语法（拷贝初始化），不使用括号 `()` 语法（直接初始化）。**

```cpp
// ✅ 写法 A（推荐）：= 赋值风格，一目了然"右边创建对象，赋给左边变量"
juce::String device_name = "Built-in Output";
juce::File config_file = juce::File("/path/to/config.json");
std::unique_ptr<AudioDecoder> decoder = std::make_unique<AudioDecoder>();
int num_channels = 2;
double sample_rate = 44100.0;

// ✅ 写法 B：允许但仅用于以下场景 ——
//         场景 1：默认构造（无法用 = 表达）
AudioDecoder decoder;               // 默认构造，没有等号写法
FileInfo info;                       // 同上
std::string text;                    // 同上

//         注意：即使构造函数带 explicit 修饰，也可以通过显式写出类型名来使用 =：
//              std::unique_ptr<T> p = std::unique_ptr<T>(raw_pointer);  // explicit 构造也能用 =，重复类型名即可

// ❌ 禁止写法：能用 = 却用括号
juce::String device_name("Built-in Output");   // 禁止，应改用 =
juce::File config_file("/path/to/config.json"); // 禁止，应改用 =
int num_channels(2);                            // 禁止，应改用 =
	std::unique_ptr<T> reader(raw_pointer);          // 禁止，应改用 = std::unique_ptr<T>(raw_pointer)
```

**理由**：

1. **对 Java 开发者友好**：`=` 语法和 Java 的对象赋值视觉一致，降低新手认知负担
2. **避免 "most vexing parse"**：C++ 的括号初始化 `T x(arg)` 在某些情况下会被编译器误解析为函数声明，而 `T x = arg` 永远不会
3. **零开销**：C++17 起强制拷贝省略（mandatory copy elision），`=` 和 `()` 编译结果完全相同，不存在性能差异
4. **编码规范一致性**：项目中 `=` 语法已在 `std::make_unique` 等场景广泛使用，统一风格减少二义性

> **唯一例外**：默认构造（无参数）无法用 `=` 表达，此时直接写 `T x;` 即可。

---

## 四、函数规范

### 4.1 返回类型前置声明

**必须使用传统的前置返回类型语法，禁止 trailing return type。**

```cpp
// ✅ 正确：返回类型在函数名前面
double getCurrentSampleRate() const;
juce::StringArray getAvailableDevices();
std::unique_ptr<AudioDecoder> createDecoder(const juce::File& file);

// ❌ 禁止：trailing return type（后置返回类型）
auto getCurrentSampleRate() const -> double;              // 禁止
auto createDecoder(const juce::File& file) -> std::unique_ptr<AudioDecoder>;  // 禁止
```

### 4.2 参数顺序与传递方式

1. **输入参数在前，输出参数在后**
2. **非基本类型的输入参数用 `const &` 传递**（避免拷贝）
3. **输出参数用指针传递**（`*`），语义明确表示「可能为空」

```cpp
// ✅ 正确
bool load(const juce::File& config_file);            // 入参：const & 引用
void query(const std::string& sql,                    // 入参：const &
           Callback callback,                         // 入参：值传递（函数对象）
           std::vector<std::string>* out_results);    // 出参：指针

// ❌ 避免
bool load(juce::File config_file);                   // 不必要的拷贝
void query(const std::string& sql, std::vector<std::string>& out_results);  // 出参用引用，调用侧不知道会修改
```

### 4.3 const 正确性

- 不修改对象状态的方法**必须**声明为 `const`
- 不修改的参数用 `const` 修饰
- 局部变量如果不修改，声明为 `const`

```cpp
// ✅ 正确
class Transport {
public:
    State getState() const;             // 方法不修改对象，const
    double getCurrentPosition() const;  // 方法不修改对象，const
    void setPlayMode(PlayMode mode);    // 方法修改对象，不加 const
};

void processFile(const juce::File& file) {  // 参数不修改，const &
    const int buffer_size = file.getSize();   // 局部变量不修改，const
    // ...
}
```

### 4.4 override 关键字

重写虚函数**必须**加 `override`，编译器会检查基类是否有对应虚函数。

```cpp
// ✅ 正确
class AudioEngine : public juce::AudioIODeviceCallback {
public:
    void audioDeviceIOCallbackWithContext(...) override;  // 必须加 override
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
};

// ❌ 禁止：缺少 override
void audioDeviceIOCallbackWithContext(...);  // 如果基类签名变了，这里不会报错
```

### 4.5 [[nodiscard]] 标记

返回值不应被忽略的函数（如返回错误码、状态、需要释放的资源）必须标记 `[[nodiscard]]`。

```cpp
// ✅ 正确：丢弃返回值将导致编译器警告
[[nodiscard]] bool load(const juce::File& config_file);
[[nodiscard]] bool isHogModeSupported() const;
[[nodiscard]] std::unique_ptr<AudioDecoder> createDecoder();

// ❌ 调用侧如果忽略返回值会收到编译器警告
load(file);  // 警告：忽略了 [[nodiscard]] 返回值
```

### 4.6 默认参数

只允许在 **.cpp 源文件或 .h 头文件的声明处** 使用默认参数，不在定义处重复。

```cpp
// ✅ 正确：.h 头文件中
class ConfigStore {
public:
    template<typename T>
    T getValue(const std::string& key_path, const T& default_value = T());
};
```

---

## 五、类设计

### 5.1 访问控制顺序

成员按以下顺序排列：

1. **访问级别**：`public` → `protected` → `private`（用户最关心公开接口，放在最前面）
2. **同一级别内**：类型声明 → 方法 → 数据成员

```cpp
class Example {
public:
    // ========== 1. 类型声明 ==========
    using Callback = std::function<void(int)>;
    enum class State { Idle, Active };

    // ========== 2. 构造/析构 ==========
    Example();
    ~Example();

    // ========== 3. 公开方法 ==========
    void setValue(int value);
    int getValue() const;

protected:
    // ========== 子类可重写的方法 ==========
    virtual void onUpdate();

private:
    // ========== 私有方法 ==========
    void internalHelper();

    // ========== 数据成员（放在最后）==========
    int value_ = 0;
    juce::String name_;
};
```

### 5.2 成员变量

- **全部为 private**，通过 getter/setter 提供受控访问
- 成员变量声明时**必须给默认值**（C++11 类内初始化，`int x = 0` 或 `int x{0}`）
- 构造函数优先用**成员初始化列表**，禁止在函数体内做简单赋值
- 复杂的初始化逻辑（如调用成员方法、注册回调）允许放在构造函数体内

```cpp
// ✅ 正确
class AudioEngine {
public:
    AudioEngine()
        : device_manager_()  // 初始化列表：零参数构造，仅为了显式表达意图
    {
        // 构造函数体：仅放无法在初始化列表中完成的复杂逻辑
        device_manager_.initialise(2, 2, nullptr, true);  // 方法调用，不是简单赋值
    }

    bool isRunning() const { return is_running_.load(); }  // 只读访问器

private:
    // 声明时给默认值（类内初始化），初始化顺序一目了然
    juce::AudioDeviceManager device_manager_;
    std::atomic<bool> is_running_{false};
    std::atomic<bool> hog_mode_active_{false};
    std::atomic<double> current_sample_rate_{44100.0};
    int audio_source_channels_ = 2;
};

// ❌ 禁止：在构造函数体内做简单赋值
class Bad {
public:
    Bad() {
        value_ = 0;        // 警告：简单赋值应放在初始化列表
        name_ = "default"; // 警告：简单赋值应放在初始化列表
    }
private:
    int value_;
    std::string name_;
};
```

### 5.3 struct vs class

| 关键字 | 用途 |
|--------|------|
| `class` | 有行为的对象：封装数据 + 方法，有不变式需要维护 |
| `struct` | 纯数据载体（POD）：公开字段，无私有成员，无行为逻辑 |

```cpp
// ✅ struct：纯数据载体，字段公开
struct TrackInfo {
    int64 db_id = 0;
    juce::String file_path;
    juce::String title;
    juce::String artist;
    double sample_rate = 44100.0;
    int bit_depth = 16;
    int channels = 2;
};

// ✅ class：有行为，有封装
class AudioEngine {
public:
    void initialize();
    void shutdown();
private:
    // 封装的状态
    juce::AudioDeviceManager device_manager_;
    std::atomic<bool> is_running_{false};
};
```

### 5.4 Rule of 5 / Rule of 0

- **优先 Rule of 0**：如果类用智能指针管理资源，不显式声明析构/拷贝/移动函数，编译器自动生成
- 如果需要管理原始资源（如某些 JUCE 对象的包装），显式声明 Rule of 5（析构、拷贝构造、拷贝赋值、移动构造、移动赋值）

```cpp
// ✅ Rule of 0：智能指针自动管理资源
class AudioDecoder {
    std::unique_ptr<juce::AudioFormatReader> reader_;   // unique_ptr 自动管理
    std::unique_ptr<juce::AudioFormatReaderSource> source_;
    // 不需要写 ~AudioDecoder()、拷贝/移动函数
};

// ✅ Rule of 5：当必须手动管理资源时（罕见）
class LegacyWrapper {
public:
    LegacyWrapper();
    ~LegacyWrapper();
    LegacyWrapper(const LegacyWrapper& other);
    LegacyWrapper& operator=(const LegacyWrapper& other);
    LegacyWrapper(LegacyWrapper&& other) noexcept;
    LegacyWrapper& operator=(LegacyWrapper&& other) noexcept;
private:
    void* raw_handle_;  // 手动管理的原始资源
};
```

### 5.5 单一职责

每个类只做一件事。如果发现一个类承担了多个职责，拆分为独立类。

```cpp
// ❌ 避免：AudioEngine 直接处理解码 + 播放 + 设备管理
class AudioEngine {
    // 太多职责混在一起
};

// ✅ 正确：职责分离
class AudioEngine   { ... };  // 只负责设备管理 + 音频回调
class AudioDecoder  { ... };  // 只负责解码
class Transport     { ... };  // 只负责播放状态机
```

---

## 六、指针与内存管理

### 6.1 智能指针优先

- `std::unique_ptr`：所有权独占，默认首选
- `std::shared_ptr`：所有权共享（仅在确实需要共享时使用）
- 裸指针：**仅用于不持有所有权的场景**（观察者 / 依赖注入 / Listener）

```cpp
// ✅ 正确：unique_ptr 持有资源所有权
std::unique_ptr<juce::AudioFormatReader> reader_;
std::unique_ptr<ConfigStore> config_;

// ✅ 正确：裸指针用于依赖注入（不持有所有权）
class AudioEngine {
private:
    juce::AbstractFifo* audio_source_fifo_ = nullptr;  // 指向解码器的 Fifo，AudioEngine 不拥有它
    Database* database_ = nullptr;                     // 由外部注入，AudioEngine 不负责释放
};

// ❌ 禁止：裸指针持有所有权（不知道谁负责释放）
juce::AudioFormatReader* reader_;  // 应该是 unique_ptr
```

### 6.2 禁止裸 new / delete

使用 `std::make_unique` / `std::make_shared` 创建对象，避免裸 `new` 和 `delete`。

```cpp
// ✅ 正确
std::unique_ptr<AudioDecoder> decoder = std::make_unique<AudioDecoder>();
std::shared_ptr<ConfigStore> config = std::make_shared<ConfigStore>();

// ❌ 禁止：裸 new / delete
AudioDecoder* decoder = new AudioDecoder();  // 禁止
delete decoder;                               // 禁止
```

### 6.3 传递方式选择

```cpp
// 传递所有权 → unique_ptr by value
void setDecoder(std::unique_ptr<AudioDecoder> decoder);

// 不修改的入参 → const &（基本类型除外）
void processTrack(const TrackInfo& track);

// 可修改的出参 → 指针（表达"可能为空"的语义）
bool load(const juce::File& file, FileInfo* out_info);

// 不拥有所有权的依赖 → 裸指针
void setDatabase(Database* database);
```

---

## 七、控制流与格式

### 7.1 大括号风格 —— K&R

与 Java 保持一致：开括号**不换行**，闭括号**独占一行**。

```cpp
// ✅ 正确：K&R 风格
void play() {
    if (state_ == State::Playing) {
        return;  // 已在播放，直接返回
    }
    state_ = State::Playing;
}

// ❌ 禁止：Allman 风格（开括号换行）
void play()
{
    if (state_ == State::Playing)
    {
        return;
    }
    state_ = State::Playing;
}
```

### 7.2 必须使用大括号

**即使只有一行语句也必须使用大括号。** 这是防御性编程，防止修改时引入 bug。

```cpp
// ✅ 正确：即使单行也加大括号
if (is_running_) {
    device_manager_.stop();
}

for (int i = 0; i < num_channels; i++) {
    output_channel_data[i][sample] = 0.0f;  // 静音
}

// ❌ 禁止：省略大括号
if (is_running_)
    device_manager_.stop();  // 缺少大括号，后续加代码容易出错
```

### 7.3 缩进与空格

- **缩进**：4 个空格（不使用 Tab 字符）
- **二元运算符两侧**加空格
- **逗号后面**加空格
- **关键字后面**加空格（`if (`, `for (`, `while (`）

```cpp
// ✅ 正确
int result = a + b * c;                 // 运算符左右有空格
for (int i = 0; i < count; i++) {       // 关键字后空格，分号后空格
    processTrack(tracks[i]);            // 4 空格缩进
}

// ❌ 避免
int result=a+b*c;                       // 缺少空格，难读
for(int i=0;i<count;i++){               // 缺少空格
```

### 7.4 if/else 格式

```cpp
// ✅ 正确
if (condition) {
    doSomething();
} else {
    doOtherwise();
}

// 多分支
if (state == State::Playing) {
    pause();
} else if (state == State::Paused) {
    resume();
} else {
    play();
}
```

### 7.5 switch 格式

`case` 中包含局部变量声明时，必须用大括号包裹。

```cpp
// ✅ 正确
switch (state) {
    case State::Stopped: {
        // case 中有局部变量，必须加 {}
        juce::String message = "已停止";
        notifyListeners(message);
        break;
    }
    case State::Playing:
        // 无局部变量，可以不加 {}
        transport.start();
        break;
    default:
        break;
}
```

### 7.6 行宽限制

每行代码不超过 **120 个字符**。超过时适当换行并对齐。

```cpp
// ✅ 正确：参数过长时换行，参数对齐
void audioDeviceIOCallbackWithContext(
    const float* const* input_channel_data,
    int num_input_channels,
    float* const* output_channel_data,
    int num_output_channels,
    int num_samples,
    const juce::AudioIODeviceCallbackContext& context) override;

// 函数调用过长时换行
std::unique_ptr<AudioDecoder> decoder =
    std::make_unique<AudioDecoder>(file, sample_rate, num_channels);
```

---

## 八、注释规范

### 8.1 基本原则

- **注释用中文**，与项目沟通语言保持一致
- 注释要说明**这段代码做了什么**，以及**为什么这么做**（意图 + 行为）
- 每个源文件、类、公开方法都要有注释

### 8.2 文件头注释

每个 `.h` 和 `.cpp` 文件顶部必须有：

```cpp
/**
 * 文件名：AudioEngine.h
 * 职责：音频引擎 —— 管理音频设备生命周期、Hog Mode 独占、采样率切换、
 *       Integer Mode 直通、音频回调分发
 * 所属模块：domain/audio_engine（领域层）
 * 所属线程：初始化/配置由 UI 线程调用，音频回调由系统音频实时线程调用
 */
#pragma once
```

### 8.3 类注释

```cpp
/**
 * AudioEngine —— 音频引擎
 *
 * 负责管理 CoreAudio 音频设备（通过 JUCE AudioDeviceManager），
 * 支持 Hog Mode（独占）和 Integer Mode（整数直通），
 * 在音频实时线程中从 AbstractFifo 读取已解码的 PCM 数据并输出到 DAC。
 *
 * 线程安全：初始化/关闭/切换设备在 UI 线程调用；audioDeviceIOCallback
 *          运行在系统音频实时线程中，此回调中严格禁止锁/分配/I/O/日志。
 */
class AudioEngine : public juce::AudioIODeviceCallback {
    // ...
};
```

### 8.4 方法注释

JavaDoc 风格：说明功能、参数、返回值、线程约束。

```cpp
/**
 * 设置当前播放的音频设备
 *
 * @param device_name  设备友好名称（来自 getAvailableDevices()）
 * @return            切换成功返回 true，设备不存在或占用时返回 false
 *
 * 线程约束：必须在 UI 线程调用。若当前正在播放，内部会先 stop 再切换再 start。
 *          Hog Mode 激活时切换设备会自动先释放 Hog Mode。
 */
bool setCurrentDevice(const juce::String& device_name);
```

### 8.5 行内注释

关键逻辑需要逐行注释，说明「做了什么」以及「为什么这么做」。

```cpp
// ✅ 正确：解释为什么这样做
void AudioEngine::audioDeviceIOCallbackWithContext(...) {
    for (int sample = 0; sample < num_samples; sample++) {
        // 从无锁环形缓冲读取 PCM 数据（AbstractFifo 保证音频线程安全）
        int ready = audio_source_fifo_->read(read_buffer, num_samples);

        if (ready < num_samples) {
            // 缓冲区不足时输出静音，防止爆音
            // （解码线程处理不过来，静音优于输出未初始化内存）
            for (int ch = 0; ch < num_output_channels; ch++) {
                output_channel_data[ch][sample] = 0.0f;
            }
        } else {
            // 正常播放：直接 memcpy 到输出缓冲，零格式转换
            for (int ch = 0; ch < num_output_channels; ch++) {
                output_channel_data[ch][sample] = read_buffer[ch][sample];
            }
        }
    }
}

// ❌ 避免：只重复代码本身，没有说明意图
// 读取 fifo
int ready = audio_source_fifo_->read(read_buffer, num_samples);  // 没说为什么读、读了干什么
```

---

## 九、音频实时线程安全守则

> ⚠️ **这是 BitPerfect 项目最重要的技术约束，违反将导致音频爆音、死锁或内核崩溃。**

### 9.1 音频回调中禁止的行为

在 `audioDeviceIOCallback`（运行在系统音频实时线程）中，以下行为**绝对禁止**：

```cpp
void AudioEngine::audioDeviceIOCallbackWithContext(...) {
    // ========== ✅ 可以做的事 ==========
    // 1. memcpy（复制数据到输出 buffer）
    std::memcpy(output_channel_data[0], buffer_, num_samples * sizeof(float));

    // 2. 原子变量读写（std::atomic）
    current_position_.store(current_position_.load() + num_samples);

    // 3. AbstractFifo 读写（无锁环形缓冲）
    audio_source_fifo_->read(read_buffer, num_samples);

    // 4. 简单算术运算
    float gain = 1.0f;
    for (int i = 0; i < num_samples; i++) {
        output_channel_data[0][i] *= gain;
    }

    // ========== ❌ 绝对不能做的事 ==========
    // 1. 内存分配
    //    new / delete / malloc / free / std::make_unique / std::vector::push_back
    //    原因：内存分配器内部有锁，会导致优先级反转

    // 2. 加锁
    //    std::mutex::lock() / juce::CriticalSection / std::lock_guard
    //    原因：音频线程等待 UI 线程持有的锁 → 死锁或爆音

    // 3. 文件 I/O
    //    fopen / fread / juce::File::loadFileAsString
    //    原因：I/O 系统有锁且延迟不确定

    // 4. 系统 API 调用
    //    CoreAudio setProperty / AudioObjectSetPropertyData
    //    原因：内部可能加锁，导致死锁

    // 5. 日志输出
    //    SPDLOG_INFO / printf / std::cout
    //    原因：spdlog 即使异步模式，内部也可能涉及内存分配

    // 6. 消息发送
    //    juce::MessageManager::callAsync
    //    原因：消息管理器内部有锁
}
```

### 9.2 错误报告模式

音频回调中发生错误时，不能抛异常、不能记日志、不能通知 Listener。只能：

```cpp
// 设置原子错误标志
std::atomic<bool> audio_error_{false};
std::atomic<int> last_audio_error_code_{0};

void audioDeviceIOCallbackWithContext(...) {
    if (buffer_underrun) {
        // 只能设置原子标志，其他线程会检测到
        audio_error_.store(true);
        // 同时输出静音，防止噪音
        std::memset(output_channel_data[0], 0, num_samples * sizeof(float));
    }
}

// UI 线程通过定时器（100ms）检查原子标志
void checkAudioErrorsOnUIThread() {
    if (audio_error_.exchange(false)) {  // 原子读取并重置
        // 现在可以在 UI 线程安全地记日志、弹出错误提示
        SPDLOG_ERROR("音频缓冲区欠载，设备可能过载");
        notifyListeners("音频引擎出现 buffer underrun");
    }
}
```

### 9.3 调试期断言

```cpp
// 开发期启用，Release 编译时通过宏去掉，零运行时开销
#ifdef BP_DEBUG
    // 断言当前在消息线程（UI 线程）
    #define BP_ASSERT_MESSAGE_THREAD() \
        jassert(juce::MessageManager::getInstance()->isThisTheMessageThread())

    // 断言当前不在音频线程（防止在音频回调中调用危险函数）
    #define BP_ASSERT_NOT_AUDIO_THREAD() \
        jassert(!juce::Thread::getCurrentThread()->isAudioThread())
#else
    #define BP_ASSERT_MESSAGE_THREAD()      // Release 下为空，零开销
    #define BP_ASSERT_NOT_AUDIO_THREAD()    // Release 下为空，零开销
#endif
```

---

## 十、JUCE 框架使用规范

### 10.1 优先使用 JUCE 封装

在 JUCE 提供等价实现时，优先使用 JUCE 类：

| 场景 | 使用 JUCE | 不使用标准库 |
|------|-----------|-------------|
| 字符串 | `juce::String` | `std::string`（仅在 JUCE 交互边界用） |
| 动态数组 | `juce::Array<T>` | `std::vector<T>`（优先 vector） |
| 文件操作 | `juce::File` | `std::filesystem` |
| 线程管理 | `juce::ThreadPool` | `std::thread`（裸线程） |
| 无锁通信 | `juce::AbstractFifo` | 自行实现 |
| 锁 | `juce::CriticalSection` | `std::mutex` |
| XML/JSON | JUCE XmlElement / `nlohmann::json` | 自行解析 |

> **注意**：JUCE 字符串和标准库字符串的转换边界要明确。
> - 内部业务逻辑（如数据库操作）用 `std::string`
> - 与 JUCE/UI 交互的接口用 `juce::String`
> - 转换用 `.toStdString()` / `juce::String(str)`

### 10.2 UI 组件规范

```cpp
// ✅ 正确：继承 juce::Component，定制外观继承 LookAndFeel_V4
class TransportBar : public juce::Component,
                     public juce::Button::Listener {
public:
    TransportBar();
    ~TransportBar() override;

    // juce::Component
    void paint(juce::Graphics& g) override;
    void resized() override;

    // juce::Button::Listener
    void buttonClicked(juce::Button* button) override;

private:
    juce::TextButton play_button_{"播放"};    // 中文按钮标签
    juce::TextButton pause_button_{"暂停"};
    juce::Slider seek_slider_;
};

// ✅ 自定义 LookAndFeel
class BitPerfectLookAndFeel : public juce::LookAndFeel_V4 {
public:
    void drawRotarySlider(...) override;  // 只重写需要定制的部分
};
```

### 10.3 Listener 模式

Listener 用于下层→上层通知，BitPerfect 中统一使用 `juce::ListenerList`：

```cpp
// ✅ 正确：使用 juce::ListenerList
class Transport {
public:
    struct Listener {
        virtual ~Listener() = default;
        virtual void onStateChanged(State new_state) {}
        virtual void onPositionChanged(double position_seconds) {}
        virtual void onPlayModeChanged(PlayMode mode) {}
    };

    void addListener(Listener* listener) { listeners_.add(listener); }
    void removeListener(Listener* listener) { listeners_.remove(listener); }

private:
    juce::ListenerList<Listener> listeners_;  // 线程安全的 Listener 列表

    void notifyStateChanged(State new_state) {
        // 自动遍历所有 Listener 并回调
        listeners_.call([new_state](Listener& l) {
            l.onStateChanged(new_state);
        });
    }
};
```

### 10.4 跨线程 UI 更新

从后台线程更新 UI **必须**使用 `juce::MessageManager::callAsync()`：

```cpp
// ✅ 正确：后台线程 → UI 线程
void AudioDecoder::decodingLoop() {
    while (running_) {
        // ... 解码数据，填充 Fifo ...

        // 需要通知 UI 时，必须投递到 UI 线程
        juce::MessageManager::callAsync([this]() {
            // 这里的代码在 UI 线程执行，可以安全操作 UI
            listeners_.call([](Listener& l) {
                l.onDecodingComplete();
            });
        });
    }
}

// ❌ 禁止：后台线程中直接调用 Listener（Listener 可能在操作 UI 控件）
void AudioDecoder::decodingLoop() {
    // 直接 notify → 在后台线程操作 UI → 崩溃
    listeners_.call([](Listener& l) { l.onDecodingComplete(); });  // 禁止！
}
```

### 10.5 浮点比较

TODO: 浮点比较用 std::abs(a - b) < epsilon

### 10.6 错误码用 0 表示成功，非 0 表示失败

TODO: 所有函数返回 int 时，0 成功，非 0 失败（与 Unix/POSIX 惯例一致）

### 10.7 TODO 标签

TODO 统一用小写格式 `TODO:`，便于搜索。若涉及后续阶段功能，加注阶段号：

```cpp
// TODO: 当前占位，P1 阶段实现完整错误处理
int play() {
    return 0;  // TODO: P1 实现 Transport 状态机，返回真实错误码
}

// TODO(P3): 支持 DSD 格式（DSF/DFF）
```

### 10.8 头文件中只能用注释，禁止使用 TODO

TODO 只能写在 .cpp 实现文件中，头文件（.h）中禁止出现 TODO。

```cpp
// ✅ 正确：.cpp 文件中
// TODO: 当前实现仅支持 FLAC/WAV，P1 扩展至全部格式
bool AudioDecoder::open(const juce::File& file, FileInfo& out_info) {
    // ...
}

// ❌ 禁止：.h 头文件中
class AudioDecoder {
public:
    // TODO: P1 添加对 ALAC/AIFF/MP3/AAC 的支持  ← 禁止！头文件中不能有 TODO
    bool open(const juce::File& file, FileInfo& out_info);
};
```

**理由**：头文件是模块的公开接口契约，TODO 是内部实现状态的标记，不应暴露在接口中。

---

## 十一、CMake 规范

### 11.1 模块 CMakeLists.txt

每个模块目录有独立的 `CMakeLists.txt`，父目录通过 `add_subdirectory` 引入。

### 11.2 变量命名

```cmake
# ✅ 正确：项目级变量使用 BP_ 前缀
set(BP_INFRASTRUCTURE_SOURCES
    infrastructure/logger/Logger.cpp
    infrastructure/config/ConfigStore.cpp
)

# 局部变量用小写
set(local_list "")
```

### 11.3 源文件显式列出

**禁止** `file(GLOB ...)`，每个源文件必须显式列出。这保证了 CMake 能精确追踪依赖，避免新增文件时遗漏。

```cmake
# ✅ 正确：显式列出所有源文件
set(LOGGER_SOURCES
    infrastructure/logger/Logger.h
    infrastructure/logger/Logger.cpp
)

# ❌ 禁止：通配符收集
file(GLOB LOGGER_SOURCES "infrastructure/logger/*.cpp")  # 禁止
```

---

## 十二、测试规范

### 12.1 框架与命名

- 使用 **Catch2** 测试框架
- 测试文件命名：`test_<模块名>.cpp`
- 测试用例描述用中文

```cpp
// 文件：tests/domain/test_audio_engine.cpp

#include <catch2/catch_test_macros.hpp>
#include "audio_engine/audio_engine.h"

TEST_CASE("AudioEngine 初始化时枚举所有音频设备", "[AudioEngine]") {
    AudioEngine engine;
    engine.initialize();

    juce::StringArray devices = engine.getAvailableDevices();
    REQUIRE(!devices.isEmpty());  // 至少有一个设备（内置输出）
}

TEST_CASE("获取当前采样率返回默认值 44100", "[AudioEngine]") {
    AudioEngine engine;
    REQUIRE(engine.getCurrentSampleRate() == 44100.0);
}
```

### 12.2 测试原则

- 每个公开接口**至少一个**测试用例
- 测试**正常路径 + 边界条件 + 错误路径**
- 测试之间**互相独立**，不依赖执行顺序

---

## 十三、禁止事项

| 禁止项 | 替代方案 |
|--------|---------|
| C 风格类型转换 `(int)x` | `static_cast<int>(x)` 等 C++ 转换 |
| `using namespace std;`（头文件中） | 全限定名 `std::string`，或仅在 `.cpp` 局部作用域用 |
| 全局变量 | 单例模式或依赖注入 |
| 宏函数 `#define MAX(a,b) ...` | `inline` 函数或 `constexpr` 函数 |
| `printf` / `cout` | `SPDLOG_INFO` / `SPDLOG_ERROR` |
| 原始 C 字符串 `char*` | `std::string` 或 `juce::String` |
| 裸 `new` / `delete` | `std::make_unique` / `std::make_shared` |
| `NULL` / `0` 表示空指针 | `nullptr` |
| `typedef` | `using` 别名声明 |
| 传统 `enum`（非 class） | `enum class` 强类型枚举 |
| `auto` 类型推导 | 显式类型声明 |
| `()` 风格变量初始化（可用 `=` 替代时） | `=` 风格变量初始化 |
| trailing return type | 传统前置返回类型 |
| 省略大括号的单行语句 | 始终使用大括号 |
| `file(GLOB ...)`（CMake 中） | 显式列出源文件 |
| `#ifndef` 头文件守卫 | `#pragma once` |

---

## 附录 A：代码模板

### A.1 头文件模板

```cpp
/**
 * 文件名：Example.h
 * 职责：[一句话描述]
 * 所属模块：[层级/模块名]
 */
#pragma once

#include <memory>     // 标准库
#include <string>

#include <juce_core/juce_core.h>  // 第三方

// 前向声明
class Database;

/**
 * Example —— [类的职责描述]
 *
 * [详细说明，包含线程安全性说明]
 */
class Example {
public:
    // --- 构造 / 析构 ---
    Example();
    ~Example();

    // --- 公开方法 ---
    void doSomething(int param);
    [[nodiscard]] int getValue() const;

private:
    // --- 私有方法 ---
    void internalHelper();

    // --- 成员变量 ---
    int value_ = 0;
    Database* database_ = nullptr;  // 不持所有权，由外部注入
};
```

### A.2 源文件模板

```cpp
/**
 * 文件名：Example.cpp
 * 职责：[一句话描述]
 */
#include "example.h"  // 自己的头文件必须第一个

#include "database/database.h"  // 本项目
#include <spdlog/spdlog.h>      // 第三方

#include <algorithm>            // 标准库

// ============================================================
// 构造 / 析构
// ============================================================

Example::Example() {
    // 初始化逻辑
}

Example::~Example() {
    // 清理逻辑
}

// ============================================================
// 公开方法
// ============================================================

void Example::doSomething(int param) {
    // 实现
}

int Example::getValue() const {
    return value_;
}

// ============================================================
// 私有方法
// ============================================================

void Example::internalHelper() {
    // 内部实现
}
```

---

## 附录 B：Java 开发者常见 C++ 陷阱

| 陷阱 | Java 行为 | C++ 正确做法 |
|------|----------|-------------|
| 对象赋值 | 引用语义：`a = b` 后 a 和 b 指向同一对象 | 值语义：`a = b` 是拷贝，用 `std::move` 转移所有权 |
| 空检查 | `if (obj == null)` | `if (ptr == nullptr)` 或判断 `std::optional<T>` |
| 字符串 | String 是引用类型 | `std::string` 是值类型，注意拷贝成本 |
| 继承覆盖 | 默认就是虚方法 | 必须加 `virtual` + 子类加 `override` |
| GC | 自动回收 | RAII：资源在析构函数中释放，依靠作用域自动管理 |
| 接口 | `interface` | 抽象基类（全纯虚 + 虚析构）或 C++20 `concept` |
| 异常 | 随处可用 | 音频实时线程中禁止抛异常，用错误码 |
| 泛型 | 类型擦除 | 模板是编译期代码生成，注意头文件包含 |

---

> 最后更新：2026-07-09（v1.2：3.6 收紧 —— explicit 构造函数也必须用 = 语法）
