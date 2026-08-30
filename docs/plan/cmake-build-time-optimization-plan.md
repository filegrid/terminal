# CMake 依赖与编译效率优化执行计划

## 成功标准

- 建立覆盖全部实际 CMake target 的依赖清单与 clean/增量耗时基线；
- 每个优化 PR 都附 target 耗时、最慢翻译单元和头文件传播的前后对比；
- 编译期依赖边界收紧且不产生循环依赖、隐藏传递 include 或 ABI 回归；
- CI 持续检测构建时间回归和新增公共头依赖。

## 已完成的初始证据（2026-08-23）

- [x] 识别当前生成器为 Ninja Multi-Config，编译器为 MSVC 14.50.35717。
- [x] 从 `build/.ninja_log` 提取历史全量 `full` wall time：Debug 1140.84s、Release 1149.99s。
- [x] 识别累计编译工作量前四的 target：`TerminalSettingsModelLib`、`TerminalSettingsEditor`、`TerminalAppLib`、`TerminalControlLib`。
- [x] 生成目标图：174 nodes、636 edges，文件为 `build/reports/cmake-target-dependencies.dot`。
- [x] 识别 PCH 为首要热点：`WorkspaceExtension` 177.24s、`TerminalAppLocalTests` 132.30s、`UIHelpers` 126.72s、`TerminalSettingsEditor` 117.50s。

这些是历史日志快照，不能替代可重复基线；阶段 0--2 仍必须完成。

## 第一轮已实施与验证（2026-08-23）

- [x] 精简 `microsoft/src/cascadia/TerminalSettingsModel/pch.h`：移除未在非 PCH 源/头中使用的 `winrt/Windows.Storage.Streams.h` 与 `winrt/Windows.UI.Core.h`。
- [x] 移除 PCH 对 `til/mutex.h` 的隐式供给；在真实使用点 `ApplicationState.h`、`ActionMap.h` 显式 include，修复并防止继续依赖传递包含。
- [x] 重建 Debug `TerminalSettingsModelLib`：47 步，最终链接 `Microsoft.Terminal.Settings.Model.Lib.lib`；重定向构建日志未发现 `error Cxxxx`/`fatal error`，所有编译进程退出。
- [x] Ninja dry-run 返回 0；本次先触发 CMake glob/reconfigure 检查，未显示额外 target 编译边。

测量说明：本轮 PCH 编译步骤在 `.ninja_log` 中为约 50.2s，而历史热点为 81.32s；两者不是同一份受控的 clean-build 样本，**不得宣称 38% 的稳定收益**。下一轮应在独立 build 目录、固定并行度下做三次 PCH 失效重建，才可写入正式收益指标。构建已开启 `/showIncludes`，故详细输出必须重定向至 `build/reports`，否则大量 include 日志会挤占 SSH runner 管道并干扰测量。

## 第二轮已实施与验证（2026-08-23）

- [x] 从 `TerminalSettingsModel/pch.h` 移除 `wil/registry.h`、`Windows.ApplicationModel.AppExtensions.h`、`Windows.Storage.h`、`Windows.System.h`、`Windows.UI.ViewManagement.h` 和 `Microsoft.Terminal.TerminalConnection.h`。
- [x] 在 `CascadiaSettingsSerialization.cpp`、`Theme.cpp`、`WslDistroGenerator.cpp`、`CascadiaSettings.cpp`、`AzureCloudShellGenerator.cpp`、`KeyChordSerialization.cpp` 与 `TerminalSettingsSerializationHelpers.h` 补齐实际使用的显式 include。
- [x] Debug `TerminalSettingsModelLib` 重建完成并成功链接；round-2 日志未包含 `error Cxxxx`、`fatal error` 或 `ninja: build stopped`。

该轮继续减少所有 Settings Model 翻译单元被迫解析的 WinRT/WIL API 面；后续收益仍需在干净、固定环境中以三轮中位数统计。

## 第三轮已实施与验证（2026-08-23）

- [x] 将 `Windows.ApplicationModel.Resources.Core.h` 从 PCH 下沉到 7 个实际使用的 Action/Resource 文件与头。
- [x] Debug `TerminalSettingsModelLib` 重建并成功链接；round-3 日志未发现 C++ 编译错误或 Ninja 停止错误。

## 第四轮已实施与验证（2026-08-23）

- [x] 将 `til/throttled_func.h` 从 `TerminalSettingsModel/pch.h` 下沉到唯一需要其完整定义的 `ApplicationState.h`，继续消除 PCH 的隐式头供给。
- [x] Debug `TerminalSettingsModelLib` 重建并成功链接；round-4 日志未发现 C++ 编译错误或 Ninja 停止错误。
- [x] `TerminalSettingsEditor/pch.h` 移除未使用的 `Windows.UI.Input.h` 与 `Windows.UI.Popups.h`；Debug `TerminalSettingsEditor` 重建并成功链接 `Microsoft.Terminal.Settings.Editor.dll`。
- [x] 尝试移除 `Microsoft.UI.Xaml.XamlTypeInfo.h` 后，生成的 `XamlTypeInfo.g.cpp` 编译失败；已立即恢复该 include。这证明该头由 XAML 生成代码直接需要，不能凭手写源文件的零引用删除。

本轮有效 Editor PCH 记录为 89.77s，对比历史样本 96.19s；这可作为方向性实测，仍须以相同干净配置的三次中位数确认。失败的 84.70s 样本不计入收益统计。

## 第五轮进行中：Terminal App / Control（2026-08-23）

- [x] 从 `TerminalApp/pch.h` 移除手写源中零使用的 Media、Deployment、Store、Storage Provider/Pickers 投影头；本轮 PCH 成功生成，`.ninja_log` 记录 68.90s（历史记录 74.91s）。
- [!] `TerminalAppLib` 的完整目标构建在 Workspace glue 既有缺失文件 `ext/src/glue/chat/TerminalInputHarness.h` 处停止；该错误来自 `TerminalPageWorkspaceIncludes.h`，不属于 PCH 改动。该轮不将完整 target 通过计入验证，也不修改缺失的外部文件。
- [x] `TerminalControlLib`：尝试移除 DataTransfer、Storage.Streams、UI.Text.Core 投影头；`TermControl.cpp` 因 using/别名使用这些 API 而失败，三者均已恢复。失败 PCH 样本 60.28s 不计入收益；恢复后的 Debug 重建正在验证。

## 第六轮已实施与验证：UIHelpers（2026-08-23）

- [x] 从 `UIHelpers/pch.h` 移除 `Windows.ApplicationModel.Resources.Core.h` 与 `Windows.Graphics.Imaging.h`，以降低所有 UIHelpers 翻译单元的投影解析量。
- [x] 首次重建正确暴露三个 PCH 隐式依赖：`ResourceString.cpp`、`TextMenuFlyout.cpp` 显式包含 Resources.Core；`IconPathConverter.cpp` 显式包含 Graphics.Imaging。没有把头放回 PCH。
- [x] 修正后 Debug `UIHelpers` 重建并成功链接 `Microsoft.Terminal.UI.dll`，round-2 日志没有 C++ 编译错误或 Ninja 停止错误。
- [x] 有效 PCH 记录为 72.34s；历史记录为 126.72s。两者来自不同增量构建，作为方向性实测记录，正式回归指标仍需阶段 0 的 clean 三轮中位数。

## 第七轮已实施与验证：UIMarkdown / WorkspaceExtension（2026-08-23）

- [x] `UIMarkdown` 将 `Windows.Foundation.Collections.h` 从 PCH 下沉到 `MarkdownToXaml.cpp`；首次重建暴露 `IVector::Append` 的完整定义依赖，补齐后 Debug DLL 成功链接。有效 PCH 72.48s，历史 111.70s。
- [x] `WorkspaceExtension` 消除了对应用层 `TerminalApp/pch.h` 的直接 PCH 依赖，改用模型层 `TerminalSettingsModel/pch.h`；Debug `WorkspaceExtension.dll` 成功链接。有效 PCH 85.67s，历史 177.24s。

## 第八轮已实施与验证：测试热点（2026-08-23）

- [x] `SettingsModelUnitTests` 从 PCH 移除 `Windows.ApplicationModel.Resources.Core.h`，PCH 从历史 113.80s 到本轮 77.23s；全部编译步骤完成。Debug projection-side `KeyChord` 构造函数此前仅在非 Debug 测试替身中定义，现已在 Debug 提供，`SettingsModel.Unit.Tests.dll` 成功链接。
- [x] `TerminalAppLocalTests` 从 PCH 移除 `Windows.ApplicationModel.Resources.Core.h`；本轮有效 PCH 104.10s，对比历史 132.30s，Debug `TerminalApp.LocalTests.dll` 成功链接。
- [x] Debug `full` 回归构建完成，`workspace-extension-full` exit=0（326s）、`full-repack` exit=0（12s）；日志未发现 C++ 编译错误或 `ninja: build stopped`。输出为 `build/reports/full-debug-regression-20260823-173132.*.log`。
- [x] 第二次 Debug `full` 构建完成（18:01:55--18:17:37，942.00s）；对比历史 Debug `full` 1140.84s，方向性减少 198.84s（17.4%）。全部嵌套阶段 exit=0。由于未在独立 clean build 目录作三次中位数采样，此项不作为正式 CI 基线。
- [x] 最终 Debug `full` 回归完成：native-product-foundation 460s、terminal-settings-adapter 148s、native-product-shims 15s、workspace-extension-full 344s、full-repack 13s，全部 exit=0；未发现 C++ 编译错误或 `ninja: build stopped`。

## 后续阶段：Workspace 模块边界重构（明确延期）

- [ ] 将 `WorkspaceExtension.dll` 收敛为 core-only，移出 `src/glue/workspace/*`、`src/glue/chat/*` 与 UI/TerminalApp 依赖。
- [ ] 拆除 `WorkspaceModelWrapper.cpp → WorkspaceModel.cpp → WorkspaceCore.cpp` 的文本聚合编译链，建立可独立计时的 core target。
- [ ] 评估并验证 DLL ABI、部署、加载和宿主 target 链接关系。

该组工作按决定移至下一阶段，**不阻塞本阶段**的构建依赖/头文件优化验收。

## 第五轮完成状态更新（2026-08-23）

- [x] `TerminalControlLib` 头文件试验已恢复；恢复后的 Debug `TerminalControlLib` 重建成功，链接 `Microsoft.Terminal.ControlLib.lib`。

## 阶段 0：准备与可重复测量

工作项：确定目标 preset、生成器、编译器版本、并行度、机器规格；新建独立 `build/measure`；保证 `CMAKE_EXPORT_COMPILE_COMMANDS=ON`；确定 MSVC/Clang/GCC 对应的耗时采集方式。

检查点 0（必须通过）：

- [ ] configure 可重复成功，未使用历史 build 目录；
- [ ] 能列出全部 build target；
- [ ] 已记录命令行、commit、编译器、并行度、CPU/内存与采样日期；
- [ ] 三次 clean build 和三次 no-change build 的中位数已保存。

产物：`docs/metrics/cmake-build-baseline-<date>.md`、原始构建日志、`compile_commands.json`、`cmake-targets.dot`。

本仓库执行命令（固定为当前生成器和配置）：

```powershell
cmake -S . -B build
cmake --build build --config Debug --target full --clean-first --parallel <N> -- -d stats
cmake --build build --config Release --target full --clean-first --parallel <N> -- -d stats
```

运行三轮后，将 Ninja 日志中的同一 target 步骤按本次 build-id 分组；禁止把跨多次构建的 `.ninja_log` 累计值称为单次 wall time。

## 阶段 1：实际模块依赖审计

工作项：从 CMake 调用与 Graphviz 图提取所有 target，标识库类型、源码/公共头、直接链接边、依赖可见性、子目录归属与循环；补充 CMake 全局命令、目录级 include 路径和手写编译标志。

检查点 1：

- [ ] `docs/metrics` 中每个 target 都有一行，测试/工具/生成代码目标已标注；
- [ ] 所有 `PUBLIC` 与 `INTERFACE` 边都有“出现在公共 API 中”的证据或待修复项；
- [ ] 依赖图无未解释的逆向边和循环；
- [ ] 已按层级标注 app、feature、domain、foundation、third-party。

产物：真实 target 依赖表、更新后的 Graphviz 图、按风险排序的问题清单。

## 阶段 2：编译耗时与头文件传播剖析

工作项：按 target 汇总构建日志；采集 TU 级耗时；通过 `/showIncludes`、`-H`、depfile 或 IWYU 统计公共头传播；模拟修改 Top 公共头、私有实现头、单个 `.cpp` 的三类增量构建。

检查点 2：

- [ ] 每个 target 都有 clean、no-change、公共头修改、私有头修改和 `.cpp` 修改耗时；
- [ ] 已列出 Top 20 target 与 Top 20 翻译单元；
- [ ] 已列出 Top 20 头文件的“受影响 TU 数 × 平均 TU 耗时”；
- [ ] 结果区分编译、链接、代码生成、重配置与缓存时间；
- [ ] 已确定第一批优化项，每项有量化收益假设与回归风险。

产物：`docs/metrics/build-profile-<date>.md`、编译器 trace、头文件传播 CSV。

本仓库的优先采样 target 顺序：`TerminalSettingsModelLib` → `TerminalSettingsEditor` → `TerminalAppLib` → `TerminalControlLib` → `SettingsModelUnitTests` → `UIHelpers` → `WorkspaceExtension` → `TerminalAppLocalTests` → `UIMarkdown`。对 MSVC，在这九个 target 的试验配置中加入 `/Bt+ /showIncludes`；不得一次性全局加入，避免日志规模掩盖结果。

## 阶段 3：依赖边界与头文件整改（迭代执行）

工作项：按收益排序逐批处理：将实现依赖改为 `PRIVATE`、公共 API 改为前向声明或 PImpl、移动实现到 `.cpp`、拆分聚合头、提取窄接口/adapter、消除反向或循环依赖。每批最多覆盖一个头文件簇或一个依赖环，便于归因。

首批批次固定为：

1. `TerminalSettingsModelLib/pch.h` 和模型公共头；
2. `TerminalSettingsEditor` 的 XAML generated/projection 边界；
3. `TerminalAppLib` 的 `TerminalPage.cpp` 与生成头边界；
4. `UIHelpers` → `UIHelpersCore` 的解析/平台隔离；
5. `TerminalAppLocalTests` 的逻辑测试与 UI 集成测试拆分；
6. `WorkspaceExtension` 的配置无关 facade。

每批检查点（每个 PR 必须全部通过）：

- [ ] 公共头能独立编译，调用点显式包含所需头；
- [ ] 已运行受影响 target 的 clean build、相关测试和三类增量测量；
- [ ] `PUBLIC/INTERFACE` 依赖数、受影响 TU 数或耗时有明确下降；
- [ ] 未引入 ABI/序列化/插件接口破坏；
- [ ] 更新依赖图和指标表，保留优化前后原始日志。

退出条件：Top 头文件传播成本已处理或每一项已给出不能再降的约束；不存在无理由的公共实现依赖与 target 循环。

## 阶段 4：针对实测热点的构建机制优化

工作项：仅对已证实解析瓶颈的 target 引入最小 PCH；仅对安全的小 TU target 试验 unity build；为开发/CI 配置编译缓存；对链接热点评估增量链接或目标边界调整。

检查点 4：

- [ ] 每项实验在相同基线上进行，且有启用/禁用对照；
- [ ] PCH 命中率、失效代价和增量影响已记录；
- [ ] unity build 已排除 ODR、注册顺序、宏污染和生成代码风险；
- [ ] 缓存命中率与无缓存时间分开汇报；
- [ ] 对高频编辑路径无超过 5% 的回归。

退出条件：每项机制保留、限制或回退都有数据依据。

## 阶段 5：CI 门禁与长期维护

工作项：在 CI 保留轻量构建基准；定期运行完整 profile；为新增公共依赖、循环依赖和头文件扇出设置审查/检查；发布模块依赖所有权规则。

检查点 5：

- [ ] CI 输出 target 构建耗时与总耗时，并保存可追溯工件；
- [ ] 相对基线的阈值已配置（初始建议关键增量 +5%、总 clean +10%）；
- [ ] 新增 `PUBLIC/INTERFACE` 依赖需明确批准；
- [ ] 每月或每个发布周期重跑完整头文件传播分析；
- [ ] 指标、依赖图和例外清单均有负责人。

## 决策记录模板

| 日期/PR | Target/头文件 | 问题证据 | 改动 | clean 前→后 | 增量前→后 | 受影响 TU 前→后 | 测试 | 结论 |
|---|---|---|---|---|---|---|---|---|
| | | | | | | | | |
