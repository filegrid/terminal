# CMake 模块依赖与编译耗时优化方案

## 1. 目标与边界

本方案以工程中实际存在的 CMake target 为分析单位，而不是以目录或静态库文件名猜测模块。执行后必须生成每个 target 的：

- 全量（clean）构建耗时、增量构建耗时和其编译单元耗时 Top N；
- 直接与传递依赖图，以及依赖可见性（`PRIVATE`、`PUBLIC`、`INTERFACE`）；
- 公共头文件的被包含次数、包含深度和修改后受影响 target / 翻译单元数；
- 优化前后同一工具链、同一并行度下的对比数据。

验收目标：在不改变对外行为的前提下，降低关键增量路径的重编译范围；将目标间的链接依赖与编译期头文件依赖分别收紧；以基线数据确定总体 clean build 和高频增量 build 的改善阈值。任何没有基线、没有命令和没有前后数据的“优化”均不算完成。

## 1.1 本仓库实测快照（2026-08-23）

本节不是推测，来自当前 `build` 目录的 Ninja 日志、CMake Graphviz 输出和顶层 `CMakeLists.txt`。构建系统为 Ninja Multi-Config，编译器为 MSVC 14.50.35717；依赖图包含 **174 个 target、636 条边**。Graphviz 文件位于 `build/reports/cmake-target-dependencies.dot`。

`full` target 的最近一次 wall time 为 Debug **1140.84s**、Release **1149.99s**。该值是历史全量构建记录，并非新鲜的三次中位数；后续基线必须按第 2.2 节重测，不能与不同机器、并行度或缓存状态直接比较。

| 优先级 | Target | Ninja 累计编译工作量(s) | 编译步骤数 | 最慢单步(s) | 直接依赖/现象 | 首要动作 |
|---:|---|---:|---:|---:|---|---|
| P0 | `TerminalSettingsModelLib` | 4304.95 | 305 | 81.32 | `ConTypes`、`WinRTUtils`、`ConInt`、`ConProps`；静态库 + PCH | 先做 `pch.h` 包含树与模型头传播审计；将实现型 WinRT/JSON 依赖移到 `.cpp`/PImpl |
| P0 | `TerminalSettingsEditor` | 3283.75 | 184 | 117.50 | 依赖 Settings Model/Adapter、Con*、WinRTUtils 和 XAML 生成 | 分离 XAML 生成、ViewModel 与公共设置 API；缩小 PCH，隔离生成头 |
| P0 | `TerminalAppLib` | 1594.40 | 120 | 94.85 | 依赖 Settings Adapter，且受 `TerminalAppXaml` 约束 | 将页面/投影生成与核心逻辑拆成独立 target；审查 `TerminalPage.cpp` 头扇出 |
| P0 | `TerminalControlLib` | 1243.74 | 83 | 107.00 | 受 `TerminalControlXaml` 约束；Control、Renderer、Input 等层汇合 | 控件运行时 API 与渲染/输入实现分层；缩窄 `pch.h` 和 XAML 生成依赖 |
| P1 | `SettingsModelUnitTests` | 1175.16 | 30 | 113.80 | 测试 target 重新构建重 PCH | 测试公共夹具拆分；避免生产 `pch.h` 与测试 PCH 重复承载所有依赖 |
| P1 | `UIHelpers` | 727.17 | 35 | 126.72 | 链接 `ConTypes`、`WinRTUtils`、fmt、Windows SDK 库 | 以小型 `UIHelpersCore` 替代把 WinRT/窗口实现暴露给所有消费者 |
| P1 | `WorkspaceExtension` | 718.99 | 48 | 177.24 | 对 Debug/Release/RelWithDebInfo/AuditMode/Fuzzing 显式链接多组内部产物 | 将配置选择下沉为 imported/alias target；扩展 glue 仅依赖稳定 facade，禁止直接吞入各配置库 |
| P1 | `TerminalAppLocalTests` | 525.11 | 14 | 132.30 | 直接扇入 App、Settings、Core、Connection、Con* | 测试分层，分离 UI/XAML 集成测试与逻辑测试，减少普通测试的全栈依赖 |
| P2 | `UIMarkdown` | 481.37 | 40 | 111.70 | 依赖 `UIHelpers`、`WinRTUtils`、cmark、XAML 生成 | 将 Markdown 解析 core 与 XAML 渲染 adapter 拆开 |

“累计编译工作量”是 `.ninja_log` 中同一 target 近期编译步骤的累计，反映 CPU 工作量与热点排序；步骤有跨多次构建的记录，不能当作一次构建 wall time 相加。PCH 单步耗时分别可达 `WorkspaceExtension` 177.24s、`TerminalAppLocalTests` 132.30s、`UIHelpers` 126.72s、`TerminalSettingsEditor` 105.29--117.50s、`TerminalSettingsModelLib` 81.32s。这说明 PCH 是主要失效放大器：PCH 不是天然收益，修改其任一高扇出头会让整个 target 首先付出数十秒代价。

### 当前依赖形态与具体风险

```text
WindowsTerminal / TerminalAppLocalTests / WorkspaceExtension
                 ↓
TerminalApp ─────┼── TerminalSettingsEditor ── TerminalSettingsAdapter
                 ├── UIMarkdown ─────────────── UIHelpers
                 ├── TerminalControl ────────── TerminalControlLib
                 └── TerminalSettingsModel ──── TerminalSettingsModelLib
                                                   ↓
                                ConTypes / ConInt / ConProps / WinRTUtils
```

这是目标链接图的简化视图；虚线边在 Graphviz 中主要表示 `PRIVATE` 链接，故不能据此判断头文件可见性。需要用第 2.3 节的实际 include 数据决定何处改为前向声明、PImpl 或单独 facade。

已确认的结构性问题是：

1. Settings Model、Settings Editor、Terminal App 和 Control 同时拥有重量级 PCH、XAML 代码生成和跨层链接，导致任何公共头/PCH 变动会触发多处生成与编译。
2. `WorkspaceExtension` 对五个配置各自出现的 `ConProps`、Control、Settings、UI、Connection、WinRTUtils 产物建立链接边。它应只消费稳定的配置无关 facade；当前做法让配置矩阵进入扩展模块的依赖面。
3. `TerminalAppLocalTests` 直接扇入产品大部分上层库，PCH 单次已达 132.30s。测试不应成为频繁编辑路径的依赖汇聚点。
4. `TerminalSettingsEditor` 和 `TerminalAppLib` 的 XAML 生成节点是明确的 order-only/生成前置约束。生成产物头必须与手写公共 API 分离，否则会把代码生成变化扩散到普通业务翻译单元。

### 首批实施项（按顺序）

1. 为上述 P0 target 输出 `/showIncludes` 与 `/Bt+` 数据；先列出每个 `pch.h` 的 Top 30 传递头和所有使用该 PCH 的 TU。验收是确认能从 PCH 移出的头，而不是先添加新的 PCH。
2. 将 `TerminalSettingsModelLib` 的稳定数据/接口头与实现、WinRT 映射、JSON、命令/动作实现头拆开；指针/引用使用前向声明，`unique_ptr` 的析构移至 `.cpp`。目标是减少 `pch.h` 对内部模型与第三方头的依赖。
3. 在 `TerminalSettingsEditor` 和 `TerminalAppLib` 中把 XAML projection/generated headers 限定在 `Xaml` adapter target；核心 ViewModel/业务 target 不直接包含生成头。先在一个 target 做试点并度量 PCH 与修改一个普通 `.cpp` 的增量时间。
4. 从 `UIHelpers` 提取不含 WinRT/窗口/XAML 的 `UIHelpersCore`（静态库或 OBJECT 库仅在证据充分时采用），让 `UIMarkdown` 的解析路径只依赖 core；平台实现保持 `PRIVATE`。
5. 将 `TerminalAppLocalTests` 拆为纯逻辑单测和 UI/XAML 集成测；前者不链接 `TerminalApp`/Editor/Control 的完整实现。以测试选择性构建代替每次改设置模型都重建全栈测试 PCH。
6. 将 `WorkspaceExtension` 的按配置具体库路径改为配置无关的 imported/alias facade，并把 extension glue 的可见 API 缩至少数运行时入口。每种配置继续链接正确产物，但扩展源码不得看见所有产品模块的实现头。

## 2. 现状盘点方式（必须先执行）

### 2.1 Target 清单与 CMake 依赖图

对每个 configure preset（或当前默认配置）执行：

```powershell
cmake --preset <preset>
cmake --build --preset <build-preset> --target help
cmake --graphviz=build/cmake-targets.dot -S . -B build/<preset>
```

补充审查所有 `target_link_libraries`、`target_include_directories`、`target_sources`、`add_subdirectory` 和 `include()` 调用，产出以下表格。`<target>` 必须替换成项目真实名称，不能遗漏可执行文件、库、测试、工具和代码生成目标。

| Target | 类型 | 源码数 | 对外头文件目录 | 直接链接依赖 | PUBLIC/INTERFACE 泄漏依赖 | 归属层 | 风险 |
|---|---:|---:|---|---|---|---|---|
| `<target>` | | | | | | | |

### 2.2 构建时间基线

固定编译器版本、构建目录、并行度和机器负载。至少重复三次 clean build，采用中位数；所有后续比较使用相同条件。Ninja 推荐打开统计与编译数据库：

```powershell
cmake -S . -B build/measure -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/measure --clean-first --parallel <N> -- -d stats
cmake --build build/measure --parallel <N> -- -d stats
```

MSVC 配置加入 `/Bt+ /time`（或使用 Build Insights）；Clang 配置加入 `-ftime-trace`；GCC 配置加入 `-ftime-report`。将输出按 target 与 `.cpp` 汇总，形成如下真实数据表：

| Target | clean 中位数(s) | 增量中位数(s) | 最慢 TU | 最慢 TU(s) | 编译占比 | 链接占比 | 主要瓶颈 |
|---|---:|---:|---|---:|---:|---:|---|
| `<target>` | | | | | | | |

### 2.3 头文件传播基线

从 `compile_commands.json` 取得每个翻译单元的 `-H`（Clang/GCC）或 MSVC `/showIncludes` 输出；结合编译依赖文件（Ninja `.d`）统计。每个公开头文件必须记录：

| 头文件 | 所属 target | 直接包含 TU 数 | 传递包含 TU 数 | 最大深度 | 修改后需重编 target 数 | 优先级 |
|---|---|---:|---:|---:|---:|---|
| `<header>` | | | | | | |

优先处理“传递包含 TU 数 × 单个 TU 平均耗时”最高的头文件，而不是文件体积最大的头文件。

## 3. 目标依赖的设计规则

依赖按层单向流动：

```text
app / tools / tests
        ↓
feature libraries
        ↓
domain libraries
        ↓
foundation / platform adapters
        ↓
third-party libraries
```

1. 同层 target 不得循环链接；发现循环时提取最小的纯接口 target，或将共享实现下沉到新的低层库。
2. `app`、`tests`、`tools` 只可位于叶子层，不得被生产库链接。
3. 业务库不得直接依赖 UI、命令行、测试框架或具体基础设施；以窄接口反转依赖，适配器在组合根注入。
4. 仅出现在 `.cpp` 的依赖必须是 `PRIVATE`；只有出现在安装/公共头且消费者必须编译时才允许 `PUBLIC` / `INTERFACE`。
5. 每个库都使用 `target_include_directories`，公共暴露目录限定为 `BUILD_INTERFACE` / `INSTALL_INTERFACE`；禁止全局 `include_directories`、`link_libraries`、`add_definitions`。
6. 一个 target 的 public headers 只允许包含其稳定 API、标准库和确有 ABI/模板需要的依赖头；实现类型、第三方大头和不稳定类型一律移出。

## 4. 头文件问题的专项改造

### 4.1 判定与优先级

将下列情况视为缺陷并建立 issue：

- 公共头包含重量级第三方、平台 SDK、日志/格式化、容器实现或其他模块内部头；
- 仅使用指针、引用、智能指针或函数声明，却包含了完整定义；
- 内联函数、模板和宏在公共头中携带大段业务实现；
- 聚合头（`all.h`、`common.h`、`pch.h`）被公共 API 或基础库依赖；
- header-only target 被用作逃避依赖边界，导致大范围重复解析。

### 4.2 处方

| 症状 | 改造 | CMake / ABI 注意事项 | 验证 |
|---|---|---|---|
| 仅需声明却 include 完整类型 | 前向声明；完整 include 下移 `.cpp` | 析构函数、`unique_ptr<T>`、按值成员需在完整类型可见处定义 | 包含树与受影响 TU 数下降 |
| 公共类暴露复杂成员 | PImpl 或窄值对象/抽象接口 | 在 `.cpp` 定义析构和特殊成员；评估 ABI 与分配成本 | 消费者无需重编实现变更 |
| 重实现放在头中 | 移到 `.cpp`；模板显式实例化 | 只对稳定、有限类型集合显式实例化 | `-ftime-trace` 解析时间下降 |
| 第三方头从公共 API 透出 | 建立 adapter / facade | 依赖改为 `PRIVATE`；不得改变必要的序列化/ABI 合约 | 消费 target 不再引用该第三方 include dir |
| 大型通用头 | 拆为最小职责头，调用点按需 include | 不以隐藏 include 维持编译；启用 IWYU 检查 | 直接 include 自洽 |

每次移动 include 后单独编译所有受影响 target；禁止依赖 PCH 或其他头的偶然传递包含。建议在 CI 增加 include-what-you-use（或同等静态检查）并将新增公共 include 作为代码评审项。

## 5. 编译耗时的专项改造

### 5.1 按 target 选择手段

| 测量结果 | 优先优化 | 不应采用 |
|---|---|---|
| 解析公共头占主导 | 4 节的隔离/前向声明/PImpl；最小化 API | 仅提高并行度 |
| 同一稳定、重量级头反复解析 | 仅在同质 target 内建立 PCH | 把项目聚合头作为全局 PCH |
| 少数模板 TU 极慢 | 减少模板实现暴露、显式实例化、拆分算法 | 盲目 unity build |
| 小 TU 极多、进程开销明显 | 谨慎对单个私有 target 使用 unity build | 对含注册宏、ODR 风险或生成代码的 target 启用 |
| 链接时间占主导 | 减少不必要对象/静态库边界；启用增量链接（开发配置） | 将链接问题误判为头文件问题 |
| 外部依赖反复重编 | 使用预编译二进制依赖、缓存和正确依赖边界 | 将第三方源码 public 暴露 |

### 5.2 PCH 准入规则

仅为满足以下条件的 target 启用 `target_precompile_headers`：至少 80% 的该 target 翻译单元稳定使用相同、且几乎不变的重量级头。PCH 内容只可包括标准库和稳定第三方头，禁止项目业务头、聚合头和频繁变化的配置头。每个 PCH 必须记录命中率、冷构建影响、修改 PCH 后的失效成本；收益不足则移除。

### 5.3 并行与缓存

开发与 CI 分别设置合理 `--parallel` 上限并记录 CPU、内存、磁盘占用，防止过度并行造成换页。配置编译缓存（如 sccache/ccache，Windows 可用 sccache），并单独汇报缓存命中率；缓存不替代头文件依赖整改。Ninja 的 depfile 和 CMake/Ninja 的重配置规则必须保持准确，禁止用粗粒度 `add_custom_target` 让每次构建全量失效。

## 6. 实施顺序与验收

1. 先完成第 2 节的目标与耗时基线，并冻结报告；没有该报告不得改造。
2. 先处理 Top 20% 头文件传播成本和 Top 20% target 编译时间，它们通常决定主要收益。
3. 每个小批次只改变一个依赖边界或一个头文件簇，执行 clean + 两类增量测试，记录差异。
4. PCH、unity、缓存作为测量后手段；若使单文件修改的增量时间变差，必须回退或限制范围。
5. 完成后更新 target 依赖图并在 CI 门禁中保留基线、容许回归阈值和 include 审查。

建议初始门槛：关键 target 的增量构建时间不得回归超过 5%；任何新增 `PUBLIC` / `INTERFACE` 依赖必须说明公共 API 必要性；Top 10 公共头的传播成本至少降低 20% 或记录不可降的技术原因。最终百分比以首次实测基线和项目约束校正。

## 7. 已执行轮次的实际效果（2026-08-23）

本节只记录已落盘的 Ninja PCH 步骤和实际目标重建结果。历史记录跨构建累积，当前记录又受增量状态影响，故**不是 clean-build 基准**；百分比仅用于排序和后续受控复测，不能等同全量 wall time 收益。

| 模块 | 头部整改 | 历史 PCH(s) | 当前有效 PCH(s) | 方向性变化 | 真实目标验证 |
|---|---|---:|---:|---:|---|
| `TerminalSettingsModelLib` | 从 PCH 下沉 10 个 WinRT/WIL/TIL 头，并在 14 个真实使用点显式包含 | 81.32 | 47.22--51.19 | -37%--42% | Debug 静态库成功链接（四轮） |
| `TerminalSettingsEditor` | 去除未使用的 Input/Popups；保留生成代码必需的 XamlTypeInfo | 96.19 | 89.77 | -6.7% | Debug DLL 成功链接 |
| `TerminalAppLib` | 去除 Media/Deployment/Store/Provider/Pickers 投影头 | 74.91 | 68.90 | -8.0% | PCH 成功；完整目标被既有缺失 `TerminalInputHarness.h` 阻断 |
| `UIHelpers` | 从 PCH 下沉 Resources.Core/Graphics.Imaging，并补到 3 个 `.cpp` | 126.72 | 72.34 | -42.9% | Debug DLL 成功链接 |
| `UIMarkdown` | 从 PCH 下沉 Foundation.Collections，并补到 `MarkdownToXaml.cpp` | 111.70 | 72.48 | -35.1% | Debug DLL 成功链接 |
| `WorkspaceExtension` | PCH 从应用层 `TerminalApp/pch.h` 收窄到 `TerminalSettingsModel/pch.h` | 177.24 | 85.67 | -51.7% | Debug DLL 成功链接 |
| `TerminalAppLocalTests` | 从 PCH 下沉 Resources.Core | 132.30 | 104.10 | -21.3% | Debug 测试 DLL 成功链接 |
| `SettingsModelUnitTests` | 从 PCH 下沉 Resources.Core；补齐 Debug projection-side KeyChord 构造函数 | 113.80 | 77.23 | -32.1% | Debug 测试 DLL 成功链接 |
| `TerminalControlLib` | 试验 DataTransfer/Streams/Text.Core 后发现隐藏依赖并全部恢复 | 107.00 | 不计入 | — | 恢复后 Debug 静态库成功链接 |

失败试验也保留在 `docs/plan/cmake-build-time-optimization-plan.md`：它们表明仅凭限定名搜索会漏掉 using、别名与 XAML 生成源。后续所有 PCH 移除必须遵循“移除 → 受影响 target 重建 → 如失败则在真实使用点显式 include 或恢复”的闭环，而非理论判定。

### 7.1 全链路 Debug 构建的方向性对比

| 项目 | 耗时 | 差异 |
|---|---:|---:|
| 历史 `full` Debug 记录 | 1140.84s | 基线 |
| 本轮优化后 `full` Debug（18:01:55--18:17:37） | 942.00s | -198.84s（-17.4%） |

本轮全链路构建所有嵌套阶段均 exit=0，且日志没有 C++ 错误或 Ninja 停止错误。该对比使用同一配置和并行度，但历史记录与当前样本不是在隔离目录、固定机器负载下获得的两组 clean-build 三次中位数；因此它是**实际方向性收益**，不是可用于 CI 门禁的正式性能基准。
