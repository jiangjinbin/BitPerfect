# BitPerfect UI/UX 原型设计文档

> 版本：v1.0 | 日期：2026-07-08 | 阶段：第一阶段 — 1.5 UI/UX 原型设计
>
> 本文档是 BitPerfect 用户界面的设计规格书，作为步骤 2.8「UI 基础界面开发」的编码依据。
> 由于 JUCE 无可视化设计器，所有布局以 ASCII 图 + 精确数值描述，可直接映射到 JUCE C++ API。

---

## 一、设计目标与约束

### 1.1 设计原则

基于需求分析（项目定位、目标用户）和竞品调研（市场空白、差异化方向），确立以下设计原则：

| # | 原则 | 说明 |
|---|------|------|
| 1 | **跨平台统一风格** | 一套 UI 在 macOS/Windows/Linux 外观一致，不追求 macOS 原生控件 |
| 2 | **默认简洁，按需深入** | 新手开箱即用，高级功能（Hog Mode、Integer Mode、音频链路）通过设置面板暴露 |
| 3 | **深色主题先行** | 与 Hi-Fi 专业定位一致，P0 固定深色，P2 支持浅色切换 |
| 4 | **音频链路可视可验证** | 实时显示文件→DAC 数据路径，让 bit-perfect 可被用户确认 |
| 5 | **无 DSP 面板** | 界面不堆砌均衡器/音效控件，保持干净，与"bit-perfect 纯粹"理念一致 |
| 6 | **菜单栏常驻** | 迷你控制器作为竞品差异化的重点，P0 完成设计，P2 实现 |
| 7 | **中文优先** | 界面文字和操作提示以简体中文显示（按钮标签、菜单项、提示信息、设置选项等） |

### 1.2 技术约束

| 约束 | 说明 |
|------|------|
| UI 框架 | JUCE GUI（`juce_gui_basics`），C++ 自绘控件，非原生 AppKit |
| 布局系统 | `juce::FlexBox` + `juce::Grid`，手写 C++ 代码控制 |
| 样式系统 | 继承 `juce::LookAndFeel_V4`，通过 C++ 代码定制颜色/字体/圆角 |
| 渲染后端 | macOS 上底层 CoreGraphics 自绘 |
| 线程模型 | UI 主线程通过 `juce::MessageManager::callAsync()` 接收下层通知 |
| 布局管理器 | `juce::StretchableLayoutManager` 管理可拖拽分隔条 |
| 列表控件 | `juce::TableListBox`（播放列表）+ `juce::TreeView`（导航树） |

### 1.3 目标用户与使用场景

| 用户类型 | 特征 | 核心需求 |
|----------|------|---------|
| Hi-Fi 发烧友 | 有外置 DAC，关心采样率/位深度/Integer Mode | 验证 bit-perfect 数据链路，查看音频状态 |
| 普通音乐爱好者 | 有本地音乐文件（FLAC/MP3 等） | 界面好看、操作简单、默认设置即最优音质 |

**设计策略**：默认界面仅展示核心功能（封面、曲目信息、播放控制、播放列表），高级设置通过「设置 → 音频」面板展开，两种用户都能满足。

---

## 二、设计语言

### 2.1 配色方案（深色主题）

所有颜色定义为 Token，组件设计时引用 Token 名称而非硬编码色值。P2 添加浅色主题时，只需替换 Token 对应的色值。

#### 背景层级（4 级）

```
Token              色值         用途
──────────────────────────────────────────────────
bg-primary         #1A1A1E     窗口背景、主内容区
bg-secondary       #242429     侧边栏、面板背景
bg-tertiary        #2C2C32     卡片、列表行悬停
bg-elevated        #32323A     弹出面板、对话框、悬浮卡片
```

#### 文字层级（4 级）

```
Token              色值         用途
──────────────────────────────────────────────────
text-primary       #F5F5F7     标题、正文、重要信息
text-secondary     #9898A0     副标题、辅助信息、列表时长
text-tertiary      #6C6C76     占位文字、禁用文字
text-inverse       #1A1A1E     深色背景上的反色文字（亮色按钮）
```

#### 强调色（Accent）

```
Token              色值         用途
──────────────────────────────────────────────────
accent-primary     #4A90D9     主按钮、播放进度、选中态、开关开启
accent-hover       #5BA0E9     控件悬停状态
accent-pressed     #3A80C9     控件按下状态
accent-muted       #2A5079     选中背景（列表行选中时）
```

#### 功能色（语义色）

```
Token              色值         用途
──────────────────────────────────────────────────
success            #34C759     成功状态、Integer Mode 激活、Bit-Perfect 确认
warning            #FF9F0A     警告状态、采样率不匹配
error              #FF453A     错误状态、DAC 断开
info               #5E5CE6     信息提示
```

#### 边框与分割

```
Token              色值         用途
──────────────────────────────────────────────────
border-default     #38383F     默认边框
border-subtle      #2C2C32     弱边框、分割线
border-focus       #4A90D9     输入框聚焦边框
```

#### 播放/音量/按钮专用色

```
Token              色值         用途
──────────────────────────────────────────────────
progress-track     #3A3A42     进度条背景轨道
progress-fill      #4A90D9     进度条已播放部分（同 accent-primary）
progress-buffer    #5C5C66     缓冲进度（灰色段）
volume-track       #3A3A42     音量条背景轨道
volume-fill        #9898A0     音量条填充部分
btn-bg             #32323A     默认按钮背景
btn-bg-hover       #3A3A42     按钮悬停背景
btn-bg-pressed     #2A2A32     按钮按下背景
btn-bg-disabled    #242429     按钮禁用背景
```

#### 阴影

```
Token              色值                                    用途
────────────────────────────────────────────────────────────────
shadow-sm          0px 1px 4px  rgba(0,0,0,0.30)          小控件（滑块、按钮）
shadow-md          0px 4px 12px rgba(0,0,0,0.40)          面板、卡片
shadow-lg          0px 8px 24px rgba(0,0,0,0.50)          对话框、弹出菜单
shadow-cover       0px 4px 20px rgba(0,0,0,0.60)          封面专属
```

### 2.2 字体规格

#### 字体选择

直接使用各平台系统默认 sans-serif 字体，零配置，无需捆绑字体文件。

```
平台        UI 字体                      等宽字体（用于音频链路状态栏参数）
────────────────────────────────────────────────
macOS       SF Pro Text / SF Pro Display   SF Mono
Windows     Segoe UI                       Consolas
Linux       Noto Sans                      Noto Mono
```

> 决策理由：JUCE 默认即使用系统 sans-serif 字体，无需额外代码。各平台用户看到的是自己熟悉的系统 UI 字体，体验最自然。
> 唯一需要显式指定的场景：音频链路状态栏中的技术参数（如 `FLAC 24bit/96kHz`），指定等宽字体确保数字对齐。

#### 字号阶梯（7 级）

```
Token          字号     字重        行高      用途
────────────────────────────────────────────────────────
display-lg     24pt     Bold        32px     向导标题
display-md     20pt     SemiBold    28px     播放页曲目标题
headline       18pt     SemiBold    24px     面板标题、对话框标题
title          16pt     Medium      22px     列表标题、卡片标题
body           14pt     Regular     20px     列表项主文字、正文
body-sm        13pt     Regular     18px     播放列表行、树节点文字
caption        12pt     Regular     16px     辅助信息、时长、元数据标签
caption-sm     11pt     Regular     14px     音频链路状态栏
overline       10pt     SemiBold    14px     分类标签（全大写、字母间距 +0.5px）
```

### 2.3 间距系统（4px 基准网格）

```
Token          值       用途
────────────────────────────────────────
space-xs       4px      图标与文字间距、紧凑列表内边距
space-sm       8px      列表行内间距、按钮内边距（上下）
space-md       16px     面板内边距、段落间距、按钮内边距（左右）
space-lg       24px     组件间距、卡片间距
space-xl       32px     页面边距、大标题上下空间
space-xxl      48px     章节间距、欢迎页留白

组件内边距：
panel-padding      16px     面板/卡片内边距
list-row-padding   8px 16px 列表行上下 8px / 左右 16px
button-padding     6px 20px 按钮上下 6px / 左右 20px
input-padding      8px 12px 输入框内边距
```

### 2.4 图标尺寸

```
Token          尺寸      用途
────────────────────────────────────────
icon-sm        14px     状态图标、表格内图标
icon-md        20px     运输控件图标（播放/暂停/切歌）
icon-lg        24px     导航栏图标、工具栏图标
icon-xl        32px     功能入口大图标、空状态图标
```

### 2.5 图标清单与风格

#### 风格定义

- **线性图标**（stroke 风格），填充图标仅在选中态使用
- 统一描边宽度：1.5px
- 颜色继承父级文字颜色，通过 `juce::Graphics::setColour()` 动态设置
- 实现方式：纯代码 `juce::Path` 绘制，零资源依赖，完美支持主题切换

#### P0 需要的图标（共 14 个）

```
图标名称          用途                  Path 描述
────────────────────────────────────────────────────
play              播放按钮             三角形（向右）
pause             暂停按钮             两条竖线
previous          上一首               双三角形 + 竖线（向左）
next              下一首               双三角形 + 竖线（向右）
volume            音量（有声音）        喇叭 + 声波弧线
volume-muted      音量（静音）          喇叭 + ✕
play-mode-seq     顺序播放             单箭头向右
play-mode-one     单曲循环             数字 1 + 循环箭头
play-mode-all     列表循环             循环箭头
play-mode-shuffle 随机播放             交叉箭头
search            搜索                 放大镜
folder            文件夹               文件夹形状
music-note        音符（封面占位）       音符
alert             警告/错误            三角形 + 感叹号
close             关闭                 ✕
add               添加                 +（加号）
```

### 2.6 圆角规范

```
控件                 圆角值     说明
────────────────────────────────────────
进度条轨道            2px       细长轨道，圆角较小
滚动条滑块            4px       ScrollBar thumb
按钮                 6px       TextButton / ShapeButton
搜索框/输入框         6px       输入控件
卡片/面板            8px       设置分组卡片、列表行高亮
开关                 10px      完全圆形（高度 20px → 10px 半径）
弹出菜单             8px       PopupMenu 背景
专辑封面             12px      CoverArtComponent 外层
对话框               12px      DialogWindow / AlertWindow
```

### 2.7 动效规范

| 动效 | 时长 | 缓动函数 | 说明 |
|------|------|---------|------|
| 侧边栏展开/折叠 | 250ms | ease-out | `ComponentAnimator` 驱动 resize |
| 按钮 hover | 150ms | ease | 颜色渐变 |
| 按钮 press | 50ms | ease-in | scale 0.97 |
| 进度条缓冲增长 | 300ms | linear | 缓冲段前进 |
| 封面切换（切歌） | 300ms | ease-in-out | 淡入淡出 |
| Toast 出现/消失 | 200ms | ease-out / ease-in | 底部滑入，3 秒后滑出 |

> JUCE 实现：使用 `juce::ComponentAnimator` + `juce::Timer`（60fps）驱动颜色/透明度过渡。

---

## 三、窗口与布局

### 3.1 主窗口布局 —— 方案 A：三栏式（默认）

```
┌──────────────────────────────────────────────────────────────────────────┐
│  ● ● ●  BitPerfect — 曲目名                                [—] [□] [✕]  │  ← 标题栏（原生）
├──────────┬───────────────────────────────────┬────────────────────────────┤
│          │                                   │                            │
│  音乐库   │        当前播放                   │       播放列表              │
│  浏览     │                                   │                            │
│          │    ┌───────────────────────┐      │  ┌───┬──────────────────┐  │
│ ┌──────┐ │    │                       │      │  │ # │ 标题   艺人  时长 │  │
│ │ 艺术家│ │    │                       │      │  ├───┼──────────────────┤  │
│ │ 专辑  │ │    │      专辑封面           │      │  │ 1 │ Song A  …  3:21 │  │
│ │ 文件夹│ │    │     400×400 px        │      │  │ 2 │ Song B  …  4:05 │  │
│ │      │ │    │                       │      │  │ 3 │ Song C  …  2:58 │  │
│ ├──────┤ │    │                       │      │  │ … │ …               │  │
│ │播放列表│ │    └───────────────────────┘      │  └───┴──────────────────┘  │
│ │ 最爱  │ │                                   │                            │
│ │ 古典  │ │   曲目标题 (display-md)             │  共 12 首 · 58 分钟        │
│ │ + 新建│ │   艺术家 · 专辑 (body)              │                            │
│ └──────┘ │   2023 · FLAC · 96kHz/24bit        │                            │
│          │                                   │                            │
│   🔍 搜索 │   ═══════════════════════════      │                            │
│          │   ▲ 1:23 ────○──────── 4:56       │                            │
│          │   ═══════════════════════════      │                            │
│          │                                   │                            │
│          │   ⏮    ⏯    ⏭     🔀   🔊         │                            │
│          │                                   │                            │
│  240px   │              flex(1)              │           320px             │
│          │             (min 380)             │                            │
├──────────┴───────────────────────────────────┴────────────────────────────┤
│  FLAC 24bit/96kHz  ──→  DAC 24bit/96kHz  ──→  独占模式  ──→  Int32   ✅   │  ← 状态栏 32px
└──────────────────────────────────────────────────────────────────────────┘

窗口默认尺寸：1040 × 680 px
窗口最小尺寸：800 × 520 px
```

**列宽可调**：

| 区域 | 默认宽度 | 最小宽度 | 最大宽度 |
|------|:------:|:------:|:------:|
| 左侧导航（LibraryBrowser） | 240px | 180px | 320px |
| 中间区域（NowPlayingPanel） | flex(1) | 380px | — |
| 右侧列表（PlaylistPanel） | 320px | 0px（可折叠） | 400px |
| 底部状态栏（AudioChainDisplay） | 固定 32px | — | — |

分隔条：左右各一条 `juce::StretchableLayoutResizerBar`，宽度 4px，hover 时变宽到 6px 并改变光标。

### 3.2 主窗口布局 —— 方案 B：两栏式（紧凑布局 / 小窗口）

```
┌───────────────────────────────────────────────────────────────┐
│  ● ● ●  BitPerfect — 曲目名                      [—] [□] [✕]  │
├───────────────────────┬───────────────────────────────────────┤
│                       │                                       │
│  当前播放             │      内容面板（Tab 切换）               │
│                       │                                       │
│   ┌───────────────┐   │  ┌───┬────────────────────────────┐   │
│   │               │   │  │ # │ 标题          艺人     时长  │   │
│   │  专辑封面      │   │  ├───┼────────────────────────────┤   │
│   │  320×320 px   │   │  │ 1 │ Song A       Artist 3:21  │   │
│   │               │   │  │ 2 │ Song B       Artist 4:05  │   │
│   └───────────────┘   │  └───┴────────────────────────────┘   │
│                       │                                       │
│   曲目标题             │  [音乐库]  [播放列表]  ← Tab 切换标签   │
│   艺术家 · 专辑        │                                       │
│                       │                                       │
│   ═══════════════     │                                       │
│   ▲ 1:23 ──○── 4:56  │                                       │
│   ═══════════════     │                                       │
│   ⏮  ⏯  ⏭  🔀  🔊    │                                       │
│                       │                                       │
│       380px           │               flex(1)                 │
│                       │                                       │
├───────────────────────┴───────────────────────────────────────┤
│  FLAC 24/96 → DAC 24/96 → 独占模式 → Int32               ✅  │
└───────────────────────────────────────────────────────────────┘

窗口默认尺寸：800 × 560 px
窗口最小尺寸：640 × 440 px
```

**特点**：左侧封面+控制 380px，右侧通过 Tab 切换「音乐库」和「播放列表」视图。适合小窗口或笔记本屏幕。

### 3.3 迷你窗口布局 —— 方案 C：上下分区式

> P2 实现，P0 完成布局设计。

```
┌──────────────────────────────────────────────────┐
│  ┌──────────┐                                    │
│  │          │  Song Title                        │
│  │  封面     │  Artist · Album                    │
│  │ 96×96 px │                                    │
│  │          │  ═══════════════════════            │
│  └──────────┘  ▲ 1:23 ────○──────── 4:56         │
│                ═══════════════════════            │
│                ⏮     ⏯     ⏭      🔊             │
└──────────────────────────────────────────────────┘

窗口尺寸：480 × 140 px（固定高度）
始终置顶：setAlwaysOnTop(true)
无原生标题栏：setUsingNativeTitleBar(false)，自定义关闭按钮
双击封面 → 恢复主窗口
```

### 3.4 窗口尺寸规范汇总

| 窗口类型 | 默认尺寸 | 最小尺寸 | 备注 |
|----------|----------|----------|------|
| 主窗口（三栏式） | 1040 × 680 | 800 × 520 | 默认布局 |
| 主窗口（两栏式） | 800 × 560 | 640 × 440 | 紧凑布局 |
| 迷你窗口 | 480 × 140 | 固定 | P2 实现 |
| 设置对话框 | 680 × 480 | 固定 | 模态对话框 |
| 首次启动向导 | 560 × 420 | 固定 | 模态对话框 |
| Toast 通知条 | 全宽 × 40 | — | 底部滑入 |

### 3.5 主窗口关闭行为

- 点击关闭按钮（✕）：根据用户设置 —「退出应用」或「隐藏到菜单栏」（仅隐藏窗口，后台继续播放）
- `Cmd+W`：关闭窗口（同关闭按钮行为）
- `Cmd+Q`：退出应用
- 此行为可在「设置 → 界面」中配置

### 3.6 布局切换

- 方案 A ↔ 方案 B：通过菜单「视图 → 紧凑布局」切换，状态持久化到 `ConfigStore`
- 方案 C：独立迷你窗口，通过菜单「视图 → 迷你播放器」或 `Cmd+Shift+M` 打开
- 从迷你窗口回到主窗口：双击封面区域 或 `Cmd+Shift+M` 再次切换

---

## 四、组件规格

> 按依赖顺序排列：先底层可复用组件，再组合组件。

### 4.1 CoverArtComponent（封面显示组件）

```
类名：CoverArtComponent
父类：juce::Component
文件：ui/components/CoverArtComponent.h

┌─────────────────────────────┐
│                             │
│                             │
│      专辑封面图片             │
│      (保持正方形)             │
│                             │
│                             │
└─────────────────────────────┘

尺寸：由父容器决定（保持正方形）
圆角：12px
阴影：shadow-cover（0px 4px 20px rgba(0,0,0,0.60)）

绘制层级（从下到上）：
  1. 阴影层（juce::DropShadow 或手动绘制）
  2. 圆角矩形背景（bg-tertiary，封面加载前显示）
  3. 封面图片（Graphics::drawImage()，使用圆角矩形 clip 区域）
  4. 占位图标（仅当无封面时，音符图标居中，text-tertiary 颜色）

状态：
  加载中     — bg-tertiary + 微弱脉冲动画（Timer 50ms 更新 alpha）
  已加载     — 封面图片 + 柔光阴影
  无封面     — bg-tertiary + 音符图标（icon-xl, 32×32px）
  加载失败   — 同"无封面"

API：
  void setCoverImage(const juce::Image& image);
  void setPlaceholder();
  void setLoading();
  void clear();
```

### 4.2 SeekSlider（进度条组件）

```
类名：SeekSlider
父类：juce::Slider（LinearBar 风格）+ 自定义 LookAndFeel
文件：ui/components/SeekSlider.h

┌──────────────────────────────────────────────────────┐
│                                                      │
│  1:23    ═══════════●════════════════════    4:56    │
│          ▲ 已播放    滑块            未播 ▲           │
│                                                      │
└──────────────────────────────────────────────────────┘

高度：32px
轨道高度：4px（normal）/ 6px（hover）
滑块尺寸：12×12 px 圆形（normal）/ 14×14 px（hover）
圆角：轨道 2px，滑块 6px
时间标签：caption（12pt），text-secondary。左：当前时间，右：总时长

颜色：
  - 轨道（未播放部分）：progress-track（#3A3A42）
  - 轨道（已播放部分）：progress-fill（#4A90D9）
  - 缓冲段：progress-buffer（#5C5C66）
  - 滑块：progress-fill（#4A90D9）

交互：
  - 点击轨道任意位置 → 直接跳转到对应时间
  - 拖拽滑块 → 实时更新左侧时间标签，松手后执行 seekTo()
  - 拖拽过程中播放状态不变（静默 seek）
  - 缓冲段数据来自 AudioDecoder 回调
```

### 4.3 VolumeSlider（音量滑块组件）

```
类名：VolumeSlider
父类：juce::Slider（LinearHorizontal）+ 自定义 LookAndFeel
文件：ui/components/VolumeSlider.h

┌─────────────────────────────────────────────────┐
│                                                 │
│  🔊   ═══════════●══════════════════            │
│  图标  轨道       滑块                           │
│                                                 │
└─────────────────────────────────────────────────┘

尺寸：140 × 36 px
轨道高度：4px
滑块尺寸：10×10 px 圆形

颜色：
  - 轨道（剩余部分）：volume-track（#3A3A42）
  - 轨道（已填充部分）：volume-fill（#9898A0）
  - 滑块：同 volume-fill

音量范围：0.0 ~ 1.0（PCM 增益），步长 0.01，默认值 0.8（80%）

音量图标（14×14px，位于滑块左侧 8px）：
  - 点击图标 → 切换静音
  - 正常：🔊 图标（volume 图标）
  - 静音：🔇 图标（volume-muted 图标），滑块归零但记忆原始值
  - 取消静音：恢复原始音量
  - 音量 = 0 时自动显示静音图标

软件音量提示（P2）：
  - 设置中禁用软件音量后，此滑块变灰，显示 tooltip"已禁用 — 请使用 DAC 音量控制"
```

### 4.4 PlayPauseButton（播放/暂停按钮）

```
类名：PlayPauseButton
父类：juce::ShapeButton 或自定义 juce::Component
文件：ui/components/PlayPauseButton.h（或内嵌在 TransportBar 中）

尺寸：56 × 56 px（比上下首按钮略大，突出主要操作）
圆角：28px（圆形）
背景：透明（normal）/ accent-primary 15% 透明度（hover）/ accent-primary 25%（pressed）

状态：
  Normal    — 圆形透明背景，图标 text-primary
  Hover     — 150ms 过渡，浅色背景 + 图标 accent-primary + 手型光标
  Pressed   — 背景加深，图标 scale 0.95
  Disabled  — 图标 text-tertiary，无背景，正常光标

图标：
  - Transport 停止/暂停 → 显示 ▶（play Path）
  - Transport 播放中 → 显示 ⏸（pause Path）

点击行为：调用 PlayerController::togglePlayPause()

JUCE 实现提示：
  - 在 paint() 中绘制圆形背景 + 图标 Path
  - mouseEnter / mouseExit 触发 repaint()
  - 使用 ComponentAnimator 做 scale 动画
```

### 4.5 上下首按钮

```
尺寸：48 × 48 px
图标：text-secondary（normal）/ accent-primary（hover）
其余规格与 PlayPauseButton 相同，仅尺寸略小。

左按钮（⏮ previous Path）：点击 → PlayerController::previousTrack()
右按钮（⏭ next Path）：    点击 → PlayerController::nextTrack()
```

### 4.6 播放模式按钮

```
尺寸：36 × 36 px 圆形按钮

四种模式循环切换：
  Sequential  →（图标：→） 列表播完后停止
  RepeatOne   →（图标：🔂） 单曲循环
  RepeatAll   →（图标：🔁） 列表播完后从头循环
  Shuffle     →（图标：🔀） 随机打乱

当前模式图标：accent-primary，其余状态：text-secondary
点击行为：Transport::cyclePlayMode()
```

### 4.7 TransportBar（播放控制条）

```
类名：TransportBar
父类：juce::Component
文件：ui/transport_bar/TransportBar.h
高度：72px（固定）
布局：FlexBox（水平排列，居中对齐）

┌──────────────────────────────────────────────────────────────────────────┐
│                                                                          │
│   [⏮]    [⏯]    [⏭]     1:23 ═══●════ 4:56     [🔀]      🔊 [════●══]  │
│  48×48  56×56  48×48   40×18   flex(1)  40×18   36×36   14×14  140×36   │
│  上一首  播放   下一首   当前时间  进度条   总时间   播放模式  图标  音量滑块  │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘

子组件间距：space-md（16px）
左右边距：space-lg（24px）
```

### 4.8 NowPlayingPanel（当前播放面板）

```
类名：NowPlayingPanel
父类：juce::Component
文件：ui/now_playing/NowPlayingPanel.h
布局：FlexBox（垂直排列，居中对齐，justifyContent: center）

┌──────────────────────────────────────┐
│                                      │
│          ┌──────────────────┐        │
│          │                  │        │
│          │   CoverArt       │        │
│          │   Component      │        │
│          │   400×400 px     │        │
│          │   (随窗口缩放)    │        │
│          │                  │        │
│          └──────────────────┘        │
│                                      │
│          曲目标题 (display-md)        │  ← 居中，最多两行省略
│          艺术家 · 专辑 (body)         │  ← 居中，单行省略
│          2023 · FLAC · 96kHz/24bit   │  ← caption, text-secondary, 居中
│                                      │
└──────────────────────────────────────┘

封面尺寸策略（保持正方形）：
  coverSize = min(panelWidth - 64px, panelHeight - 200px, 400px)

曲目信息字段来源（domain::TrackInfo）：
  - 标题：    TrackInfo::title
  - 艺术家：  TrackInfo::artist
  - 专辑：    TrackInfo::album
  - 元数据行：TrackInfo::year · TrackInfo::format · TrackInfo::sampleRate/TrackInfo::bitDepth

无曲目状态：
  - 封面区域显示占位图（BitPerfect Logo 或音符图标）
  - 曲目标题显示 "未选择曲目"（text-tertiary）
  - 艺术家/专辑行隐藏
  - 元数据行隐藏
```

### 4.9 PlaylistPanel（播放列表面板）

```
类名：PlaylistPanel
父类：juce::Component（内部使用 juce::TableListBox）
文件：ui/playlist_panel/PlaylistPanel.h

┌─────────────────────────────────────────────────┐
│  播放列表                          [12 首 · 58分钟] │  ← 标题栏 40px
├───┬─────────────────────────────────────────────┤
│ # │ 标题                   艺人            时长   │  ← 表头 32px
├───┼─────────────────────────────────────────────┤
│ 1 │ Speak to Me          Pink Floyd      1:08  │  ← 行 36px
│ 2 │ Breathe              Pink Floyd      2:48  │
│ 3 │ On the Run           Pink Floyd      3:36  │
│…  │ …                                     …    │
│ ▶ │ The Great Gig...     Pink Floyd      4:44  │  ← 正在播放行
│…  │ …                                     …    │
└───┴─────────────────────────────────────────────┘

列配置：
  #       宽度 40px    居中    序号（正在播放行显示 ▶）
  标题    宽度 flex(3)  左对齐  单行省略
  艺人    宽度 flex(2)  左对齐  单行省略
  时长    宽度 52px    右对齐  mm:ss 格式

行状态（背景色）：
  正常行      bg-primary
  悬停行      bg-tertiary（#2C2C32）
  选中行      accent-muted 背景 + accent-primary 左边框（2px）
  正在播放    accent-muted 背景 + ▶ 图标 + 文字加粗（text-primary）
  文件丢失    text-tertiary + ⚠ 图标 + 斜体

表头：
  高度 32px，背景 bg-secondary
  文字：overline 风格（10pt, SemiBold, text-secondary, 全大写）
  可点击排序（P1）：点击列头 → 按该列排序

右键菜单：
  - 播放选中曲目
  - 从播放列表中删除
  - 查看曲目信息
  - 在 Finder 中显示

多选：Cmd+点击（多选）、Shift+点击（范围选择）
键盘：Delete 键删除选中行
空状态：显示 "拖拽歌曲到此处添加" 引导文字

JUCE 实现：
  - juce::TableListBox，重写 getNumRows() / paintRowBackground() / paintCell()
  - setDragEnabled(true) 启用拖拽排序
  - 虚拟滚动：仅渲染可见行（TableListBox 默认行为）
```

### 4.10 LibraryBrowser（音乐库导航）

```
类名：LibraryBrowser
父类：juce::Component（内部使用 juce::TreeView）
文件：ui/library_browser/LibraryBrowser.h

┌──────────────────────────────────┐
│  🔍 搜索音乐库...                 │  ← 搜索框 36px
│                                  │
│  ┌ 音乐库 ▸                      │  ← 分类标题（粗体，可折叠）
│  │ ├─ 艺术家                     │
│  │ │  ├─ Pink Floyd (3)         │  ← 括号内数 = 专辑数
│  │ │  │  ├─ The Dark Side...    │
│  │ │  │  │  ├─ 1. Speak to Me  │
│  │ │  │  │  ├─ 2. Breathe      │
│  │ │  │  │  └─ ...             │
│  │ │  │  └─ The Wall           │
│  │ │  ├─ Daft Punk (2)         │
│  │ │  └─ ...                    │
│  │ ├─ 专辑                      │
│  │ ├─ 文件夹                    │
│  │ └─ 流派 (P2)                 │
│  │                              │
│  └ 播放列表                      │
│    ├─ 我的最爱 (42)              │  ← 括号内数 = 曲目数
│    ├─ 古典精选 (18)             │
│    └─ + 新建播放列表...          │
│                                  │
└──────────────────────────────────┘

树节点类型与行为：
  分类节点（"音乐库"/"播放列表"）    — overline 风格，不能选中/拖拽，点击折叠/展开
  维度节点（"艺术家"/"专辑"/"文件夹"）— body-sm，单选（同一时间一种维度展开）
  数据节点（艺术家/专辑/曲目名）      — body-sm，双击播放（曲目）/展开（专辑）
  播放列表节点                       — body-sm，双击加载到 PlaylistPanel

选中态：
  - 当前选中节点：accent-muted 背景 + text-primary 文字颜色

搜索框：
  - 高度 36px，占 LibraryBrowser 全宽
  - 输入时 300ms 防抖，实时过滤树节点
  - 输入时切换为"搜索结果"视图（覆盖树节点）
  - 搜索无结果：显示 "未找到匹配 'xxx' 的结果"（caption, text-tertiary）
  - 清空 → 恢复浏览模式

JUCE 实现：
  - juce::TreeView + 自定义 TreeViewItem 子类
  - 搜索时替换 RootItem 的子节点
  - Timer 实现 300ms 防抖搜索
```

### 4.11 AudioChainDisplay（音频链路状态显示）

```
类名：AudioChainDisplay
父类：juce::Component
文件：ui/components/AudioChainDisplay.h
高度：32px（固定）
背景：bg-secondary

┌──────────────────────────────────────────────────────────────────────────┐
│  FLAC 24bit/96kHz  ──→  DAC 24bit/96kHz  ──→  Exclusive  ──→  Int32  ✅  │
│  源文件格式      箭头   当前DAC采样率    箭头  输出模式   箭头  数据格式  状态│
└──────────────────────────────────────────────────────────────────────────┘

四个区块（卡片样式）：
  ┌──────────────────┐    ┌──────────────────┐
  │ FLAC 24bit/96kHz │ → │ DAC 24bit/96kHz  │ …
  └──────────────────┘    └──────────────────┘
  背景：bg-elevated       圆角：4px        内边距：4px 8px
  字体：caption-sm（11pt），等宽字体

颜色编码（根据状态）：
  ✅ Bit-Perfect 已确认  — 所有区块正常，右侧显示绿色 success 文字
  ⚠ 采样率不匹配         — DAC 区块边框变 warning 色
  ❌ 共享模式             — 输出模式区块边框变 error 色
  ⚠ Float32 转换中        — 数据格式区块显示 Float32，边框 warning 色

交互：
  - 悬停任意区块 → tooltip 显示详细技术参数
  - 点击状态栏 → 打开设置 → 音频链路标签页
```

### 4.12 SettingsDialog（设置对话框）

```
类名：SettingsDialog
父类：juce::DialogWindow
文件：ui/settings/SettingsDialog.h
尺寸：680 × 480 px（模态）

┌───────────────────────────────────────────────────────────┐
│  ● ● ●  设置                                        [✕]   │
├──────────────┬────────────────────────────────────────────┤
│              │                                            │
│  音频设备     │  选择音频输出设备:                           │
│              │  ┌────────────────────────────────────┐    │
│  音频链路     │  │ ● SMSL SU-9 (USB Audio)           │    │
│              │  │   最大: 768kHz / 32bit             │    │
│  界面         │  │ ○ MacBook Pro 扬声器               │    │
│              │  │   最大: 96kHz / 32bit               │    │
│  快捷键       │  │ ○ Topping DX7 Pro                 │    │
│              │  │   最大: 768kHz / 32bit / DSD1024    │    │
│  关于         │  └────────────────────────────────────┘    │
│              │                                            │
│   150px      │  [✓] Hog Mode (独占模式)                    │
│              │      独占音频设备，防止系统声音混入             │
│              │                                            │
│              │  [✓] Integer Mode                          │
│              │      绕过 Float32 转换，整数直出到 DAC        │
│              │                                            │
│              │  缓冲区大小:  [ 512 samples (11.6ms) ▼]     │
│              │                                            │
│              │                     flex(1)                 │
└──────────────┴────────────────────────────────────────────┘

左侧导航：
  - 宽度 150px，不可调
  - 选中项：accent-muted 背景 + accent-primary 左边框 2px
  - 悬停项：bg-tertiary 背景

控件规格：

  设备选择列表（单选）：
    - 每行：○/● 选中指示器 + 设备名称（body）+ 次要信息（caption, text-secondary）
    - 选中指示器颜色：accent-primary

  开关（Toggle Switch）：
    - 尺寸：40 × 22 px
    - 关闭态：bg track（bg-tertiary），圆形滑块在左
    - 开启态：accent-primary track，圆形滑块在右（白色）
    - 动画：滑块滑动 150ms ease-out
    - 右侧：设置名称（body）+ 下方说明文字（caption, text-secondary）

  下拉选择（ComboBox）：
    - 高度：32px
    - 背景：bg-elevated，边框：border-default（1px），圆角：6px
    - 聚焦：边框变 border-focus

"音频链路"子页内容：
  ┌──────────────────────────────────────────────┐
  │  文件格式 ──→ DAC采样率 ──→ 输出模式 ──→ 数据格式 │
  │  ┌────────┐  ┌─────────┐  ┌────────┐ ┌─────┐ │
  │  │ FLAC   │  │ 96 kHz  │  │ 独占   │ │Int32│ │
  │  │ 24bit  │→ │         │→ │ Hog    │→│     │ │
  │  │ 96kHz  │  │         │  │ Mode   │ │     │ │
  │  └────────┘  └─────────┘  └────────┘ └─────┘ │
  │          ✅ Bit-Perfect 已确认                 │
  ├──────────────────────────────────────────────┤
  │  源文件路径: /Volumes/Music/track.flac         │
  │  解码器: JUCE FLAC Reader                     │
  │  输出通道: 2 (立体声)                           │
  │  当前延迟: 11.6ms (512 samples @96kHz)         │
  │  CoreAudio 设备 ID: 87                        │
  └──────────────────────────────────────────────┘
```

### 4.13 MainWindow（主窗口）

```
类名：MainWindow
父类：juce::DocumentWindow
文件：ui/main_window/MainWindow.h

子组件结构（三栏式）：
  MainWindow
  ├── juce::MenuBarComponent（菜单栏，跨平台自绘）
  ├── StretchableLayoutManager（三栏布局管理器）
  │   ├── 左栏：LibraryBrowser（宽度可调 180–320px）
  │   ├── 中栏：NowPlayingPanel（弹性宽度，min 380px）
  │   └── 右栏：PlaylistPanel（宽度可调 0–400px，可完全折叠）
  └── AudioChainDisplay（底部状态栏，固定 32px）

窗口标题格式：
  - 无曲目播放：  "BitPerfect"
  - 曲目播放中：  "曲目名 — 艺术家 — BitPerfect"

构造参数：
  - PlayerController*    —— 绑定 NowPlayingPanel + TransportBar
  - LibraryController*   —— 绑定 LibraryBrowser
  - PlaylistController*  —— 绑定 PlaylistPanel

菜单栏结构：
  BitPerfect  │  文件     │  编辑    │  播放控制     │  视图          │  帮助
  ────────────┼──────────┼─────────┼──────────────┼───────────────┼─────────
  关于…       │  添加文件 │  撤销   │  播放/暂停    │  紧凑布局      │  帮助文档
  设置… Cmd+, │  添加文件夹│  重做   │  上一首      │  迷你播放器     │  报告问题
  退出 Cmd+Q  │  导入M3U  │  全选   │  下一首      │  显示导航栏    │  关于…
              │  导出M3U  │  删除   │  音量增/减   │  显示播放列表   │
              │          │         │  播放模式切换  │  显示状态栏    │

窗口关闭事件：
  - closeButtonPressed() → 根据设置决定退出或隐藏到菜单栏
  - 如果"隐藏到菜单栏"→ setVisible(false)，播放继续
```

---

## 五、每屏状态设计

### 5.1 主窗口状态一览

```
状态              触发条件                    设计要点
──────────────────────────────────────────────────────────────────
播放中            用户双击曲目/点击播放        封面显示、进度条走动、⏯ 图标
暂停中            用户点击暂停                 封面保持、进度条停止、图标变 ▶
停止/无播放       刚启动无历史                  封面占位图、信息空白、进度归零（不可拖拽）
播放结束          列表最后一首播完              根据模式：停止或循环，进度条归零
曲目切换中         切换曲目解码中               封面脉冲动画、进度条显示缓冲段
侧边栏折叠         用户拖拽/点击折叠按钮         动画 250ms，对应区域宽度 → 0
窗口缩放           用户拖拽窗口边缘              封面等比缩放，列表列宽按比例调整
```

### 5.2 空状态设计

#### 5.2.1 无音乐库（首次启动后未扫描）

```
┌──────────────────────────────────────────────────────────────┐
│  ● ● ●  BitPerfect                            [—] [□] [✕]   │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│                                                              │
│                   ┌──────────────────┐                       │
│                   │                  │                       │
│                   │   🎵 BitPerfect  │                       │
│                   │   Logo 图标      │                       │
│                   │   120×120 px    │                       │
│                   │                  │                       │
│                   └──────────────────┘                       │
│                                                              │
│                    欢迎使用 BitPerfect                        │
│               高品质本地音乐播放器，bit-perfect 音质保证         │
│                                                              │
│                     [ 选择音乐文件夹 ]                         │
│                                                              │
│              支持 FLAC / WAV / ALAC / AIFF / MP3 / AAC        │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

#### 5.2.2 播放列表为空

PlaylistPanel 区域显示：
```
┌──────────────────────────────────┐
│                                  │
│      拖拽歌曲到此处添加            │  ← caption, text-tertiary, 居中
│      或从音乐库双击曲目播放        │
│                                  │
└──────────────────────────────────┘
```

#### 5.2.3 搜索无结果

LibraryBrowser 中显示：
```
  🔍 不存在的歌名 ✕
  ─────────────────────
  未找到匹配 "不存在的歌名" 的结果
  请尝试其他关键词
```

### 5.3 错误状态设计

#### 5.3.1 DAC 断开（非模态覆盖层）

```
正常 UI 内容（变暗，加半透明黑色 overlay 50%）

┌─────────────────────────────────────┐
│  ⚠ 音频设备已断开                     │
│                                     │
│  DAC "SMSL SU-9" 已断开连接。         │
│  播放已自动暂停。                     │
│                                     │
│  [ 重试连接 ]    [ 切换设备 ]          │
└─────────────────────────────────────┘

overlay 背景：rgba(0, 0, 0, 0.5)
卡片背景：bg-elevated，圆角 12px，shadow-lg
居中显示
```

#### 5.3.2 文件格式不支持（Toast 通知条）

```
┌──────────────────────────────────────────────────────────────┐
│  ⚠ 不支持的文件格式: track.ape  (3 秒后自动消失)                │  ← Toast 40px
└──────────────────────────────────────────────────────────────┘

位置：窗口底部，紧贴状态栏上方
背景：warning（#FF9F0A）15% 透明度 + border warning 色
动画：从底部滑入 200ms → 停留 3 秒 → 滑出 200ms
```

#### 5.3.3 文件丢失（播放列表行标记）

播放列表中对应行：
```
  12 │ ⚠ │ Yesterday (丢失)  │ The Beatles  │ --:--  │
```
文字 text-tertiary + 斜体，最左侧显示警告图标，时长显示 `--:--`。

不弹出模态对话框，仅在状态栏显示一次 Toast。

### 5.4 首次启动向导

#### 步骤 1/3：欢迎页

```
┌──────────────────────────────────────────────┐
│  ● ● ●  BitPerfect 设置                      │
├──────────────────────────────────────────────┤
│                                              │
│           ┌──────────────────┐               │
│           │   BitPerfect     │               │
│           │   Logo           │               │
│           └──────────────────┘               │
│                                              │
│           欢迎使用 BitPerfect                  │
│                                              │
│    我们将帮你完成初始设置：                     │
│    · 选择音乐文件夹                            │
│    · 扫描音乐库                               │
│    · 配置音频输出设备                          │
│                                              │
│              [ 开始设置 ]                      │
│                                              │
└──────────────────────────────────────────────┘

对话框尺寸：560 × 420 px（模态）
标题：display-lg（24pt Bold）
说明文字：body（14pt, text-secondary）
按钮：accent-primary 背景，圆角 6px，宽度 160px
```

#### 步骤 2/3：选择音乐文件夹

```
┌──────────────────────────────────────────────┐
│  ● ● ●  BitPerfect 设置                      │
├──────────────────────────────────────────────┤
│                                              │
│   选择音乐文件夹         步骤 2/3              │
│                                              │
│   选择你存放本地音乐文件的文件夹：               │
│                                              │
│   ┌──────────────────────────────────────┐   │
│   │ 📁 /Users/xxx/Music            [✕]   │   │
│   │ 📁 /Volumes/NAS/Lossless        [✕]   │   │
│   │                                      │   │
│   │ + 添加文件夹                          │   │
│   └──────────────────────────────────────┘   │
│                                              │
│                [ 上一步 ]  [ 开始扫描 ]        │
│                                              │
└──────────────────────────────────────────────┘

已添加文件夹列表：bg-elevated 背景，圆角 8px
每行：文件夹图标 + 路径 + 删除按钮
"添加文件夹"：juce::FileChooser 弹出系统文件选择对话框（原生）
```

#### 步骤 3/3：扫描进度

```
┌──────────────────────────────────────────────┐
│  ● ● ●  BitPerfect 设置                      │
├──────────────────────────────────────────────┤
│                                              │
│   正在扫描你的音乐库...     步骤 3/3              │
│                                              │
│   ┌──────────────────────────────────────┐   │
│   │  ████████████░░░░░░░░░░  65%         │   │  ← 进度条
│   └──────────────────────────────────────┘   │
│                                              │
│   已处理：650 / 1000 个文件                    │
│   已添加：623 首曲目                           │
│   跳过：27 个（不支持的格式）                    │
│                                              │
│   正在扫描：The Dark Side of the Moon         │
│                                              │
│               [ 进入 BitPerfect ]             │   ← 扫描完成前灰色不可点击
│               [ 在后台继续 ]                   │   ← 始终可用
│                                              │
└──────────────────────────────────────────────┘

进度条：同 SeekSlider 规格，高度 6px，圆角 3px
"跳过"行：warning 色高亮，点击展开被跳过的文件列表
"进入 BitPerfect"：扫描完成后 accent-primary，否则灰色禁用
"在后台继续"：关闭向导，扫描在后台继续，用户可立即使用播放器
```

### 5.5 设置对话框所有子页

| 子页 | 内容 |
|------|------|
| **音频设备** | 设备单选列表、Hog Mode 开关、Integer Mode 开关、缓冲区大小下拉 |
| **音频链路** | 实时链路流程图、技术细节展开面板（源路径/解码器/通道数/延迟/设备 ID） |
| **界面** | 主题选择（P2）、关闭按钮行为（退出/隐藏到菜单栏）、语言选择（P2） |
| **快捷键** | 快捷键列表（只读显示，P2 支持自定义） |
| **关于** | 版本号、Git Commit、开源协议链接、致谢列表 |

---

## 六、交互流程

### 6.1 首次启动 → 播放音乐

```
应用启动
  │
  ├──→ 检查 ConfigStore：是否有已配置的音乐文件夹？
  │      │
  │      NO → 显示首次启动向导（模态，三步流程）
  │      │      ├── 步骤 1: 欢迎页 → 点击"开始设置"
  │      │      ├── 步骤 2: 选择音乐文件夹 → FileChooser 选目录 → 添加
  │      │      └── 步骤 3: 扫描进度 → 完成 → 点击"进入 BitPerfect"
  │      │
  │      YES → 直接进入主窗口，后台增量扫描
  │
  └──→ 显示主窗口（三栏式）
       │
       ├── 左侧 LibraryBrowser：显示音乐库目录树
       ├── 中间 NowPlayingPanel：显示空状态引导
       └── 右侧 PlaylistPanel：空列表

用户操作路径 A（从音乐库双击）：
  浏览艺术家 → 展开专辑 → 双击曲目
  → 该专辑所有曲目加载到播放列表 → 从所选曲目开始播放

用户操作路径 B（右键添加到列表）：
  浏览艺术家 → 右键专辑 → "添加到播放列表"
  → 专辑曲目追加到当前列表 → 用户双击列表中曲目播放

用户操作路径 C（拖拽文件）：
  从 Finder 拖拽文件/文件夹到 PlaylistPanel
  → 直接添加并开始扫描（添加文件夹时）

播放状态更新链：
  PlayerController::playTrack(trackId)
  → Transport 状态变为 Playing
  → NowPlayingPanel 更新封面 + 曲目信息
  → TransportBar 按钮状态更新
  → 状态栏更新音频链路信息
```

### 6.2 浏览 → 播放 → 切歌

```
浏览方式：
  方式 1 — 按艺术家 → 专辑：TreeView 展开艺术家 → 展开专辑 → 展开曲目
  方式 2 — 按专辑：TreeView 中按专辑字母排序
  方式 3 — 按文件夹：TreeView 显示文件系统目录结构
  方式 4 — 搜索：输入关键字 → 300ms 防抖 → 结果直接显示在 LibraryBrowser

切歌触发：
  - 自然结束（播放到曲目结尾）
  - 用户点击上一首/下一首按钮
  - 用户双击播放列表中的曲目
  - 快捷键（Cmd + ← / →）
  - 播放列表播完，根据模式（顺序 → 停止 / 循环 → 重头 / 随机 → 下一随机曲目）

切歌过渡：
  1. 当前曲目渐出（300ms 淡出音频）+ 封面 300ms 淡入淡出
  2. 下一曲目开始解码（后台线程）
  3. 状态栏更新音频链路信息（格式/采样率可能变化）
```

### 6.3 播放列表管理流程

```
创建播放列表：
  右键左侧"播放列表"分类 → "新建播放列表"
  → 输入名称 → 确认 → 新建空列表显示在树中

添加歌曲：
  方式 1：从 LibraryBrowser 拖拽曲目/专辑到 PlaylistPanel
  方式 2：从 Finder 拖拽音频文件/文件夹到 PlaylistPanel
  方式 3：右键 LibraryBrowser 中曲目 → "添加到播放列表" → 选择目标
  方式 4：双击曲目/专辑（替换当前列表并播放）

删除歌曲：
  - 选中行 + Delete 键
  - 右键 → "从播放列表中删除"

重新排序：
  - 拖拽行到目标位置（显示蓝色插入指示线）

导入/导出（P1）：
  - 菜单：文件 → 导入 M3U / 导出 M3U
  - FileChooser 选择目标文件
```

### 6.4 拖拽交互规范

```
拖拽源            →  目标              行为
────────────────────────────────────────────────────
LibraryBrowser  →  PlaylistPanel      插入到目标位置（显示插入指示线）
Finder          →  PlaylistPanel      添加文件（单文件）/ 递归扫描（文件夹）
PlaylistPanel   →  PlaylistPanel      重新排序（行拖拽）
LibraryBrowser  →  播放列表树节点       添加到指定播放列表（P1）

视觉反馈：
  - 拖拽源：半透明（alpha 50%）
  - 有效目标：PlaylistPanel 边框高亮为 accent-primary
  - 无效目标：PlaylistPanel 边框不变
  - 插入位置：在目标行上方/下方显示蓝色指示线（2px, accent-primary）
```

### 6.5 快捷键映射表

```
全局快捷键：
  Space              播放 / 暂停
  Cmd + ←            上一首
  Cmd + →            下一首
  Cmd + ↑            音量 +5%
  Cmd + ↓            音量 -5%
  Cmd + F            聚焦搜索框
  Cmd + ,            打开设置
  Cmd + N            新建播放列表
  Cmd + Shift + M    切换迷你窗口
  Cmd + I            显示曲目信息
  Delete             删除播放列表中选中行（PlaylistPanel 聚焦时）
  Enter              播放选中曲目
  Esc                退出搜索 / 关闭设置对话框

媒体键（P2）：
  F7 / Media Previous    上一首
  F8 / Media Play/Pause  播放/暂停
  F9 / Media Next        下一首
```

---

## 七、JUCE 实现指南

### 7.1 组件 → JUCE 类映射表

| 设计组件 | JUCE 类 | 关键配置 |
|----------|---------|---------|
| MainWindow | `juce::DocumentWindow` | `setResizable(true, true)`, `setUsingNativeTitleBar(true)` |
| TransportBar | `juce::Component` + `juce::FlexBox` | 固定高度 72px |
| PlayPauseButton | `juce::ShapeButton` 或自定义 `Component` | 通过 `juce::Path` 绘制图标 |
| SeekSlider | `juce::Slider`（LinearBar） | 自定义 `LookAndFeel::drawLinearSlider()` |
| VolumeSlider | `juce::Slider`（LinearHorizontal） | 同上 |
| NowPlayingPanel | `juce::Component` + `juce::FlexBox` | 垂直居中布局 |
| CoverArtComponent | 自定义 `juce::Component` | `paint()` 中 `Graphics::drawImage()` |
| PlaylistPanel | `juce::Component` 内嵌 `juce::TableListBox` | `setDragEnabled(true)` |
| LibraryBrowser | `juce::Component` 内嵌 `juce::TreeView` | 自定义 `TreeViewItem` 子类 |
| AudioChainDisplay | 自定义 `juce::Component` | 固定高度 32px |
| SettingsDialog | `juce::DialogWindow` | `showDialog()` 模态显示 |
| WelcomeWizard | `juce::DialogWindow` + 自定义 steps | 模态，三步流程 |
| 分隔条 | `juce::StretchableLayoutResizerBar` | 宽度 4px，hover 变 6px |
| 三栏布局 | `juce::StretchableLayoutManager` | `StretchableLayoutManager::setItemLayout()` |
| 搜索框 | `juce::TextEditor` | 高度 36px，圆角 6px，300ms 防抖 Timer |

### 7.2 LookAndFeel 定制清单

需继承 `juce::LookAndFeel_V4` 并重写以下方法：

```
需要重写的方法                          对应的设计规范
────────────────────────────────────────────────────────────
drawButtonBackground()              按钮背景色、圆角 6px、各状态颜色
drawButtonText()                    按钮文字颜色、字体
drawLinearSlider()                  进度条轨道颜色/高度、滑块大小/颜色
drawLinearSliderBackground()        音量条外观
drawToggleButton()                  开关外观（40×22px，圆角 11px，滑动动画）
drawTableHeaderBackground()         表头背景色 bg-secondary
drawTableHeaderColumn()             表头文字样式（overline）
paintCell() / paintRowBackground()  播放列表行状态颜色
drawTreeviewPlusMinusBox()          树展开/折叠箭头颜色
drawPopupMenuBackground()           弹出菜单背景 + 圆角
drawScrollbar()                     滚动条颜色 + 圆角
drawTextEditorOutline()             输入框边框颜色
createDefaultTypefaceForText()      默认字体 = 系统 sans-serif（无需重写，JUCE 默认行为）

建议策略：
  - 所有颜色从 BitPerfectColours.h 中读取 Token 常量
  - 不硬编码色值在 LookAndFeel 中
  - P2 浅色主题只需替换 Token 常量指向的色值
```

### 7.3 布局实现建议

| 场景 | 推荐方式 | 说明 |
|------|---------|------|
| 主窗口三栏 | `StretchableLayoutManager` | 支持拖拽分隔条调整列宽 |
| 每栏内部 | `FlexBox` | 简洁的 flex 布局，CSS Flexbox 映射 |
| 设置对话框 | `FlexBox` + `Grid` | 左侧固定宽导航，右侧 flex 内容 |
| 播放列表 | `TableListBox` | 虚拟滚动，大数据量性能好 |
| 导航树 | `TreeView` | 层级数据天然映射 |
| TransportBar | `FlexBox` 水平 + 居中 | 按钮 + 进度条 + 滑块水平排列 |

`StretchableLayoutManager` 使用示例策略：
```cpp
// 伪代码 —— 三栏布局
layoutManager.setItemLayout(0, 180, 320, 240);  // 左栏：最小/最大/默认
layoutManager.setItemLayout(1, 380, -1,  480);  // 中栏：-1 = 无限制
layoutManager.setItemLayout(2, 0,   400, 320);  // 右栏：0 = 可完全折叠
```

### 7.4 性能注意事项

| 关注点 | 建议 |
|--------|------|
| 播放列表虚拟滚动 | `TableListBox` 默认仅渲染可见行，不需要额外优化 |
| 封面图片缓存 | 维护 LRU 缓存（最大 50 张），缩放后缓存 `juce::Image` |
| 封面缩放 | 原图保持原始分辨率，缩放仅在 `paint()` 中用 `Graphics::drawImage(Image, Rectangle)` 硬件加速 |
| UI 线程不阻塞 | 所有 I/O（文件读取、数据库查询）在后台线程，UI 仅通过 Listener 回调更新 |
| 波形渲染 | 静态波形预渲染为 `juce::Image`，`paint()` 中直接 `drawImage()` |
| 动画 | 使用 `juce::ComponentAnimator`，避免裸 `Timer` 驱动动画 |
| LookAndFeel | 不每帧创建临时对象，颜色/字体在构造时预计算并缓存 |

### 7.5 主题系统预留扩展点

```
P0 实现（深色固定）：
  - 所有颜色定义在 BitPerfectColours.h 中作为 const juce::Colour
  - BitPerfectLookAndFeel 引用这些常量
  - ThemeManager 存在，但目前仅管理深色一种主题

P2 扩展点：
  - ThemeManager 维护 currentTheme 枚举（Light / Dark / System）
  - System 模式：监听 macOS 系统外观变化事件
  - applyTheme() 调用 → BitPerfectLookAndFeel::setColourScheme(Theme&)
  - 同时加载 resources/themes/light.json 和 dark.json
  - 遍历所有 Visible Component，调用 repaint()
  - 窗口装饰色（标题栏）通过 native API 设置（macOS: NSAppearance）
```

---

## 八、附录

### 附录 A：颜色值速查表

```
Token              色值        预览
──────────────────────────────────────
bg-primary         #1A1A1E     ■ 窗口背景
bg-secondary       #242429     ■ 侧边栏面板
bg-tertiary        #2C2C32     ■ 列表悬停
bg-elevated        #32323A     ■ 对话框卡片
text-primary       #F5F5F7     ■ 标题正文
text-secondary     #9898A0     ■ 辅助信息
text-tertiary      #6C6C76     ■ 占位禁用
accent-primary     #4A90D9     ■ 主强调色
accent-hover       #5BA0E9     ■ 悬停
accent-pressed     #3A80C9     ■ 按下
accent-muted       #2A5079     ■ 选中背景
success            #34C759     ■ 成功
warning            #FF9F0A     ■ 警告
error              #FF453A     ■ 错误
info               #5E5CE6     ■ 信息
border-default     #38383F     ■ 默认边框
border-subtle      #2C2C32     ■ 弱边框
border-focus       #4A90D9     ■ 聚焦边框
progress-track     #3A3A42     ■ 进度轨道
progress-fill      #4A90D9     ■ 进度填充
progress-buffer    #5C5C66     ■ 缓冲进度
volume-track       #3A3A42     ■ 音量轨道
volume-fill        #9898A0     ■ 音量填充
btn-bg             #32323A     ■ 按钮背景
btn-bg-hover       #3A3A42     ■ 按钮悬停
btn-bg-pressed     #2A2A32     ■ 按钮按下
btn-bg-disabled    #242429     ■ 按钮禁用
```

### 附录 B：窗口尺寸断点

```
窗口类型          默认尺寸      最小尺寸      备注
──────────────────────────────────────────────────────
主窗口（三栏式）  1040 × 680   800 × 520    P0 默认
主窗口（两栏式）  800 × 560    640 × 440    视图切换
迷你窗口          480 × 140    固定         P2 实现
设置对话框        680 × 480    固定         模态
首次启动向导      560 × 420    固定         模态
Toast 通知条      全宽 × 40    固定         自动消失

列宽范围（三栏式）：
  左栏（LibraryBrowser）  180–240–320
  中栏（NowPlayingPanel） 380–flex–∞
  右栏（PlaylistPanel）   0–320–400
  底栏（AudioChain）      32（固定）
```

### 附录 C：参考竞品方向

| 竞品 | 参考方向 | 具体要点 |
|------|---------|---------|
| **Audirvāna v3 Allegro** | 现代 UI 设计方向 | 暗色主题、大封面、清晰的音频链路展示 |
| **Colibri** | 极简布局 | 迷你窗口设计、菜单栏控制器、无冗余控件 |
| **Roon** | 元数据浏览 | 艺术家→专辑→曲目层级浏览、丰富的元数据展示、信号路径可视化 |

### 附录 D：与架构设计的一致性检查

| 架构设计中的 UI 模块 | 本文档对应章节 |
|---------------------|---------------|
| MainWindow | 4.13 MainWindow、3.1 三栏式布局 |
| TransportBar | 4.7 TransportBar |
| NowPlayingPanel | 4.8 NowPlayingPanel |
| PlaylistPanel | 4.9 PlaylistPanel |
| LibraryBrowser | 4.10 LibraryBrowser |
| SettingsDialog | 4.12 SettingsDialog |
| ThemeManager | 7.5 主题系统预留扩展点 |
| CoverArtComponent | 4.1 CoverArtComponent |
| SeekSlider | 4.2 SeekSlider |
| VolumeSlider | 4.3 VolumeSlider |
| AudioChainDisplay | 4.11 AudioChainDisplay |

### 附录 E：P0 UI 文件清单（对应编码阶段 2.8）

```
src/ui/
├── main_window/
│   └── MainWindow.h / .cpp
├── transport_bar/
│   └── TransportBar.h / .cpp
├── now_playing/
│   └── NowPlayingPanel.h / .cpp
├── components/
│   ├── CoverArtComponent.h / .cpp
│   ├── SeekSlider.h / .cpp
│   └── VolumeSlider.h / .cpp
└── theme/
    ├── BitPerfectLookAndFeel.h / .cpp
    └── BitPerfectColours.h（颜色 Token 常量定义）
```

---

> 下一步：步骤 1.6 — 项目工程初始化（CMake + JUCE + VS Code 配置）
