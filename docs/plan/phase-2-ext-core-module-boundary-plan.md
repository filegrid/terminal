# 第二阶段执行计划与检查点：Ext Core 模块边界

## 完成定义

- [ ] 产物由 `WorkspaceExtension.dll` 更名为 `Ext.dll`，打包流程使用新名称。
- [ ] `Glue.dll` 是所有新增页面的唯一容器；页面实现不编入 TerminalApp。
- [ ] `Ext` 不编译 `WorkspaceTerminalPageExtension.cpp`、页面 chat 状态或 TerminalPage glue。
- [ ] `TerminalPage` 只 `LoadLibrary` / `GetProcAddress` / `FreeLibrary` 通用 `Glue.dll`，并使用通用 Glue factory，不加载 Ext。
- [ ] `Ext` 的 CMake 链接列表不含 TerminalApp、TerminalControl、SettingsEditor、UIHelpers、UIMarkdown。
- [x] Debug 目标构建通过：`TerminalApp`、`Ext`、`full`。
- [ ] 记录 Ext、TerminalAppLib 的 clean/PCH/核心改动/页面改动实测耗时。
- [ ] 用源文件、include 和链接列表复核边界，并将结果填写到本文件。

## 执行检查点

| 检查点 | 操作 | 成功证据 | 状态 |
| --- | --- | --- | --- |
| P2-1 | 建立新设计和计划 | 两份独立文档已写入 docs | 完成 |
| P2-2 | 建立通用 Glue 页面容器 | TerminalApp 加载 Glue；Glue 装载/校验/卸载 Ext；迁移遗留 host-private 页面片段 | 进行中 |
| P2-3 | DLL 改名为 Ext 并调整打包 | `Ext.dll` 进入产品 overlay | 已实现；Debug `Ext.dll` 已构建 |
| P2-4 | 删除 Ext 的 page/chat 源和 UI 链 | CMake 白名单审计 | 已实现；Debug `Ext.dll` 已构建 |
| P2-5 | 分目标构建 | TerminalApp、Ext 均成功 | 完成：2026-08-25 Debug `--parallel 8` 预热后均成功/no-op |
| P2-6 | 完整构建/打包 | `full` 成功且产物存在 | 完成：2026-08-25 Debug `--parallel 8` 通过 |
| P2-7 | 计时及边界复核 | 实测表与审计结论已填写 | 进行中：Ext/Glue/no-op 与残余宿主审计完成；clean、TerminalAppLib PCH 待专门测量 |

## 实测记录

| 配置 | TerminalAppLib | Ext | Glue | 说明 |
| --- | ---: | ---: | ---: | --- |
| Debug clean（共享依赖图，`ext --clean-first --parallel 8`） | 不单独构建 | Ext.dll 于 227.662 秒完成 | Glue.dll 于 468.533 秒完成 | CMake `--clean-first` 清理 900 个共享输出；这是共享图 clean，不是 Ext 专属 clean |
| Debug TerminalAppLib PCH rebuild（当前，`--parallel 8`） | 30.78 秒 | 不参与 | 不参与 | 仅触碰并恢复 `TerminalApp/pch.h` 的无语义标记；完整 PCH 失效路径 |
| Debug core-only edit（当前，`--parallel 8`） | TerminalApp no-op | 10.2 秒 / 3 步 | 不参与 | `ExtCoreApi.cpp`：ExtCoreApi 2 秒、Ext link 2 秒、repack 5 秒 |
| Debug page-only edit（当前，`--parallel 8`） | TerminalApp no-op（恢复后 0.8 秒） | Ext core 不编译 | 12.1 秒 / 3 步 | `WorkspaceManagerColorPickerDialog.cpp`：Glue source 4 秒、Glue link 2 秒、repack 5 秒 |
| Debug 无源码改动（失效基线） | 未测 | 781 秒 | 未测 | 2026-08-24 11:50:15–12:03:16；仍重建 89 步，增量失效，不能作为正常基线 |
| Debug 无源码改动（缓存修复后） | 0.85 秒 / 0 步 | 1.00 秒 / 0 步 | 0.80 秒 / 0 步 | 2026-08-24；均仅 `[0/2]` glob 检查 |
| Debug 无源码改动（当前，`--parallel 8`） | 2.0 秒 / 0 步 | Ext target 含 repack 4 秒 | 已由 TerminalApp 复跑 no-op 验证 | 2026-08-25；首次 Debug 依赖预热不计入该行 |

若既有项目错误阻断 full，必须记录错误、最小修复和重新验证，不得把“未运行”标记为通过。

## P2-5 实际构建证据（2026-08-24）

- `WorkspaceExtension`（输出 `microsoft/bin/x64/Debug/Ext/Ext.dll`）构建通过；最终 DLL 链接 3 秒。
- `Glue`（输出 `microsoft/bin/x64/Debug/Ext/Glue.dll`）构建通过；最终 DLL 链接 1 秒。
- `TerminalApp`（输出 `microsoft/bin/x64/Debug/TerminalApp/TerminalApp.dll`）构建通过。
- `dumpbin /exports` 已确认：`Ext.dll` 导出 `GetExtCoreApiVersion`；`Glue.dll` 导出
  `CreateGlueTerminalPageExtension`、`DestroyGlueTerminalPageExtension`，并保留旧 workspace
  工厂名作为兼容别名。
- 此次是 CMake 重配置后的串行依赖重建，不可当作 clean/PCH/增量四类性能基准；计时矩阵仍待按同一基线独立执行。
- `WorkspaceTerminalPageLegacyGlue.cpp` 仍作为 `TerminalAppLib` 的编译单元，因为它直接定义
  `TerminalPage` 成员函数。必须先将宿主调用收敛到 `TerminalPageBase`，才可从该 target 移入 Glue。

## P2-6 完整构建证据（2026-08-24）

`cmake --build build --config Debug --target full --parallel 1` 成功退出。计时启动器记录：

| full 子阶段 | 实测耗时 |
| --- | ---: |
| native-product-foundation | 448 秒 |
| terminal-settings-adapter | 147 秒 |
| native-product-shims | 16 秒 |
| extension-containers-full | 343 秒 |
| full-repack | 12 秒 |

`Ext.dll` 与 `Glue.dll` 均存在于 `microsoft/bin/x64/Debug/Ext/`。这些是完整构建链耗时，不替代 P2-7 所要求的四类可比增量基准。

## P2-6 回归验证（2026-08-24，Glue 状态所有权切片后）

`full` 再次成功退出。此次有增量输入时的计时为：

| full 子阶段 | 实测耗时 |
| --- | ---: |
| native-product-foundation | 91 秒 |
| terminal-settings-adapter | 0 秒 |
| native-product-shims | 15 秒 |
| extension-containers-full | 1 秒 |
| full-repack | 13 秒 |

`dumpbin /exports` 复核：`Ext.dll` 导出 `GetExtCoreApiVersion`；`Glue.dll` 导出
`CreateGlueTerminalPageExtension` / `DestroyGlueTerminalPageExtension`，同时保留旧 workspace
工厂名作为兼容别名。该回归不是 clean 基准，故不覆盖实测矩阵中的 clean 项。

## P2-7 首个实测问题

在没有修改源码的条件下，连续执行 `WorkspaceExtension` 目标仍触发 89 步重建，耗时 781 秒。
因此当前不能宣称拆分已经带来预期增量收益。下一步必须使用 Ninja explain/depfile 审计定位每次
失效的生成文件、PCH 或时间戳，再重测 normal no-op、core-only 与 page-only 增量。

## P2-7 Ninja 增量修复（2026-08-24）

根因不是 `VerifyGlobs.cmake_force`：该 phony 输入会触发 CMake 的 glob 检查，但两次显式
配置得到的 `build.ninja` 内容哈希不变。Ninja 每次都报告 `premature end of file; recovering`，
且无源码改动仍重建 89 步。将损坏的缓存无损备份后重建，问题消失：

- `.ninja_log` 已备份为 `build/.ninja_log.corrupt-20260824-121806`；
- `.ninja_deps`（159,372 字节）已备份为 `build/.ninja_deps.corrupt-20260824-141555`；
- 重建依赖数据库的一次性构建完成（89 步、无恢复警告）；
- 随后的 `WorkspaceExtension` 无源码改动构建退出码 0，耗时 **0.80 秒**，输出仅
  `[0/2] Re-checking globbed directories...` 与 `ninja: no work to do.`。

因此，781 秒的“无改动耗时”已作废；后续模块数据必须以修复后缓存为基线。

## P2-7 已取得的模块隔离数据（2026-08-24）

- 修改 `src/core/workspace/ExtCoreApi.cpp` 后，`WorkspaceExtension` 为 **4.57 秒**：只编译
  `ExtCoreApi.cpp`（约 1 秒）并链接 `Ext.dll`（约 3 秒）。
- 修改 `src/glue/workspace/WorkspaceTerminalPageExtension.cpp` 后，`WorkspaceExtension` 为
  **0.81 秒、0 个实际工作步骤**；`Glue` 为 **8.02 秒**：只编译该页面源（约 5 秒）并链接
  `Glue.dll`（约 2 秒）。页面变动不会使 Ext 重编译。
- 对 `WorkspaceExtension` 的 PCH 输入做时间戳失效后，约 **100 秒**完成；工作范围严格为 PCH、
  `WorkspaceModelWrapper.cpp`、`ExtCoreApi.cpp`、`ExtActionNameFallback.cpp` 和 Ext 链接，
  没有重建公共 Settings、TerminalControl、SettingsEditor 或 Glue。
- 为重建刚清理的全局 `.ninja_deps`，首次访问 `Glue` 触发了 265 步依赖预热；此数据不计入页面
  增量。预热已成功完成，真实页面增量以上述 8.02 秒为准。

尚未执行的 clean 与 TerminalAppLib PCH 基准不会以推测补全；它们保留为下一次专门的隔离测量。

## P2-7 Debug clean 构建实测（2026-08-25）

- 执行 `cmake --build build --config Debug --target ext --clean-first --parallel 8`。该生成器的
  `--clean-first` 清理了 **900** 个共享输出，随后重建 **412** 个步骤；因此该值必须解释为共享依赖图的
  clean 基线，不能写成“仅 Ext clean”。
- `.ninja_log` 可审计地记录 Ext.dll 于 **227.662 秒**完成、Glue.dll 于 **468.533 秒**完成。
  后续 Debug `ext` 复跑只执行 ext-repack，**3 秒**且无编译步骤。
- TerminalAppLib 的独立 PCH 失效、core-only 与 page-only 的同配置复测仍待执行；这些项目保持未完成。

## P2-7 Debug clean 后 TerminalApp 生成依赖修复（2026-08-25）

- shared clean 后首次 Debug TerminalApp 失败于 `AboutDialog.cpp`：它直接包含生成的
  `src/generated/ExtBuildVersion.h`，但 CMake 仅把 `ext-build-version` 挂在 full packaging，未把它声明为
  `TerminalAppLib` 的输入依赖。此前已有文件时该缺陷被缓存掩盖。
- 已将 `TerminalAppLib -> ext-build-version` 加为显式 target dependency。它只生成版本头，**不**增加
  TerminalApp 到 Ext.dll 的链接或运行时依赖。修复后的构建先生成该头，`AboutDialog.cpp` 成功编译；随后
  Debug TerminalApp no-op 为 **0.8 秒 / 0 步**。
- Debug 页面增量（仅触碰 `WorkspaceManagerColorPickerDialog.cpp`）为 **12.1 秒**：Glue source **4 秒**、
  Glue link **2 秒**、ext repack **5 秒**；Ext core 未编译。shared clean 后 Host 的首次恢复不计入该增量，
  恢复完成后 Host no-op 如上。

## P2-7 Debug core-only 增量实测（2026-08-25）

- 仅触碰 `ExtCoreApi.cpp` 的无语义测量标记后，`ext --parallel 8` 为 **10.2 秒**：ExtCoreApi source
  **2 秒**、Ext.dll link **2 秒**、ext repack **5 秒**。Glue 没有 source 编译。
- 恢复最终源码后再次 Ext 构建成功；紧接的 Debug TerminalApp 输出 `ninja: no work to do.`。这证明 core
  变动仍限定为 Ext 路径，不触发 Host 源码重编译。

## P2-7 宿主残余边界审计（2026-08-24）

`TerminalApp` 的新依赖缓存预热成功后，无源码改动为 **0.85 秒、0 个实际工作步骤**。预热日志
同时确认它仍直接编译以下尚未迁入 Glue 的单元：

- `src/glue/workspace/WorkspaceTerminalPageLegacyGlue.cpp`；其文本包含 12 个 `TerminalPage::`
  成员函数片段，直接访问宿主私有字段与 XAML 控件；
- `src/core/chat/TerminalInputHarness.cpp`、`TerminalEventStore.cpp`、`WorkspaceChatController.cpp`、
  `WorkspaceChatStateHelpers.cpp`、`WorkspaceChatTextHelpers.cpp`、`WorkspaceChatStore.cpp`、
  `WorkspaceStoragePaths.cpp` 和 `src/glue/chat/WorkspaceDiagnosticLog.cpp`；
- 当时另外，`WorkspaceManagerPaneContent.cpp`、`WorkspaceIconPickerDialog.cpp` 仍为 TerminalApp
  页面源；后续切片已分别迁移/退役，见下文。

这些源不能靠 CMake 改 target 机械移动：它们定义/调用 `TerminalPage` 私有成员。下一迁移切片必须先
扩展 `TerminalPageBase` 为仅含稳定宿主操作的 facade，再把页面行为改为 Glue extension 的成员；
在此之前，P2-2 保持“进行中”。

## P2-2 Glue 状态所有权切片（2026-08-24）

- 新增 `src/glue/workspace/WorkspacePageStateTypes.h`；将 chat UI 状态、终端输出捕获、
  chat 发送传输枚举和 workspace node runtime state 定义为 `terminal::workspace` 的 Glue
  所有状态。
- `WorkspaceHostInterfaces.h` 与 `WorkspaceTerminalPageExtension.cpp` 直接使用这些 Glue
  类型；不再把它们声明/存储为 `winrt::TerminalApp::implementation` 类型。
- `TerminalPageWorkspaceTypes.h` 保留兼容 `using` 别名，遗留 `TerminalPage::` 片段仍可
  正确编译，且没有复制状态。
- 验证：Debug `Glue` 成功（重编 extension、链接 Glue）；Debug `TerminalApp` 成功且 no-op。
- 状态抽离后的 Debug `Glue` 无源码改动复测为 **1.01 秒、0 个实际工作步骤**。

这完成了宿主页面迁移前的状态所有权收敛，但没有把 `TerminalPage` 私有成员函数机械移动；
后者仍受 P2-7 审计所列 host facade 缺口约束。

## P2-2 Workspace manager 页面包装器迁移（2026-08-24）

- `WorkspaceManagerPaneContent.cpp` 已从 `_terminal_app_sources` 删除并加入 `Glue` target；
  TerminalApp 不再直接编译该实现。
- TerminalPage 将存储类型收敛为 `TerminalApp::IPaneContent`，通过 Glue extension 的
  `CreateWorkspaceManagerPaneContent`、`UpdateWorkspaceManagerPaneContent`、
  `UpdateWorkspaceManagerPaneSettings` 创建和更新页面，避免跨 DLL 保存 Glue implementation 指针。
- Glue 为此显式链接既有 DllMain-free Settings support，并定义自身 WinRTUtils 资源域；未新增
  Ext 或 TerminalApp 的反向依赖。
- 验证：强制重编 legacy 聚合单元后 Debug `TerminalApp` 成功；Debug `full` 成功。
- 性能实测：仅修改 `WorkspaceManagerPaneContent.cpp`，Glue 编译 **70 秒**、链接 **2 秒**；
  后续 TerminalApp 为 **0 个实际工作步骤**（`ninja: no work to do`）。
- `TerminalPageWorkspaceIncludes.h` 已不再包含 `WorkspaceManagerPaneContent.h`；该实现头在
  业务代码中只由 Glue 使用，TerminalApp 仅保留其自身 XAML type-info 生成所需的引用。
- 头依赖移除后强制重编 `WorkspaceTerminalPageLegacyGlue.cpp`、TerminalAppLib、TerminalApp
  均成功；随后 Debug `full` 成功（native foundation 6 秒、adapter 0 秒、shims 1 秒、
  extension containers 0 秒、repack 13 秒）。

## P2-4 回归审计（2026-08-24，状态所有权切片后）

Ext target 的实际 CMake 编译源仍只有 `WorkspaceModelWrapper.cpp`、`ExtCoreApi.cpp` 与
`ExtActionNameFallback.cpp`。链接集合为最小 Settings support、Types、WinRTUtils、Settings
Model 和系统库；未包含 `TerminalApp`、`TerminalControl`、`SettingsEditor`、`UIHelpers`、
`UIMarkdown`、页面 extension 或 chat 页面源。Ext page-free core 边界未回退。

## P2-2 图标选择器遗留 RuntimeClass 退役（2026-08-24）

- 审计确认真实图标选择器由 Glue 的 `WorkspaceManagerIconPickerGlue.cpp` 直接构造通用
  `ContentDialog`；没有任何调用路径激活 `TerminalApp.WorkspaceIconPickerDialog`。
- 已从 TerminalApp 的 IDL、XAML 页面和 C++ 源列表移除该遗留 RuntimeClass；源文件保留在工作树
  中，未做不可逆删除。
- 修复了 CMake 的陈旧 WinMD 问题：`mdmerge` 会合并 `unmerged` 目录内所有 WinMD，即使 IDL 列表
  已删除，旧 `WorkspaceIconPickerDialog.winmd` 仍会生成 activation factory。合并前现在显式清理
  这个已退役的生成文件。
- 验证：Release `TerminalApp` 完整重生成并成功链接；其生成的 `module.g.cpp` 不再包含
  `WorkspaceIconPickerDialog`。随后 `TerminalApp --parallel 1` 为 **0 个实际工作步骤**，输出
  `ninja: no work to do.`。
- 这消除了 TerminalApp 对该页面的 IDL/XAML/C++ 编译与 activation-factory 依赖；页面实际行为
  仍由 Glue 承担，未改变图标选择交互。

## P2-2 共享聊天核心去重编译（2026-08-24）

- 新建 `WorkspaceChatCore` 静态库，统一编译 `TerminalInputHarness`、`TerminalEventStore`、
  `WorkspaceChatController`、`WorkspaceChatStateHelpers`、`WorkspaceChatTextHelpers`、
  `WorkspaceChatStore` 与 `WorkspaceStoragePaths`。
- 已从 Glue 和 TerminalApp 的直接 source list 中移除上述 7 个源；两者改为链接共享库。
  `WorkspaceDiagnosticLog.cpp` 暂不移动，因为 legacy `TerminalPage` 片段仍直接调用它。
- 验证：Release Glue、Release TerminalApp 均成功。仅触碰
  `WorkspaceChatStateHelpers.cpp` 后，Glue 为 **4 步、约 3 秒**（一次 core 编译、静态库、Glue
  链接）；随后 TerminalApp 为 **1 步**，只链接 `TerminalApp.dll`，没有 chat core 编译。
- 这消除了 TerminalApp/Glue 对同一 chat core 源的双重编译，不向 Ext 增加页面或宿主依赖。
- 首次 full 回归暴露 `WindowsTerminal.exe` 直接链接 `TerminalAppLib` 的消费路径；已将
  `WorkspaceChatCore` 放入 `TerminalAppLib` 的 PUBLIC link interface，不能仅链接
  `TerminalApp.dll`。修复后 Release `full` 成功：foundation **89 秒**、adapter **1 秒**、
  shims **0 秒**、extension containers **1 秒**、repack **13 秒**（`--parallel 8`，仅作回归
  证据，不与串行增量数据混比）。

## P2-2 共享诊断实现与头依赖收敛（2026-08-24）

- `WorkspaceDiagnosticLog.cpp` 已纳入 `WorkspaceChatCore`；已从 Glue、TerminalApp、
  SettingsModel 与 WindowsTerminal 的直接 source list 移除，避免同一诊断实现多目标编译。
- 抽离暴露并修复了此前由 PCH 隐藏的依赖：`NOMINMAX` 防止 Windows `min` 宏污染、`fmt/xchar.h`
  提供宽字符格式化、WIL/WinRT/GSL 头均显式引入；UTF 转换改用 `winrt::to_string` /
  `winrt::to_hstring`，不再依赖 PCH 间接包含的 TIL 转换宏。
- 验证：Release Glue、TerminalApp 均成功；Release full 成功（foundation **7 秒**、adapter
  **1 秒**、shims **1 秒**、extension containers **1 秒**、repack **16 秒**，`--parallel 8`）。
- 仅触碰诊断 cpp（Release，`--parallel 1`）后，Glue 路径只编译共享诊断 TU 并链接
  `WorkspaceChatCore`、SettingsModel、Glue；后续 TerminalApp 不编译诊断源，只生成 adapter
  reference projection 并链接 DLL。

## P2-2 遗留窗口状态迁移边界验证与落实（2026-08-24）

- 尝试将 `TerminalPage::RefreshWorkspaceWindowState` 的窗口状态计划、ApplicationState 更新和
  诊断迁入 Glue；宿主接口仅提供 `WindowId`。
- Glue 独立链接明确失败：`ApplicationState::SharedInstance` 与
  `Settings::Model::implementation::RefreshWorkspaceWindowState` 均未向 Glue 导出。直接引用会令
  `Glue.dll` 出现 LNK2019，不能以宿主 implementation 或额外静态链接绕过该边界。
- 已将失败方案完整撤回后，以稳定 C ABI 落实迁移：Ext API 升级至 v2，并导出
  `GetExtWorkspaceWindowRefreshPlan`。输入和输出只包含固定宽度数值、布尔值与调用方提供的 UTF-16
  缓冲区，Glue 不持有 Ext C++/WinRT/SettingsModel implementation 对象。
- Glue 的 `WorkspaceExtLoader` 在加载 Ext 时同时解析该符号；`WorkspaceTerminalPageExtension`
  负责请求窗口刷新计划、记录诊断。TerminalApp 的 `TerminalPage` 仅通过 host facade 提供窗口 ID，
  并提交清理 pending workspace launch 的 ApplicationState 副作用。
- `TerminalPage::RefreshWorkspaceWindowState` 现在只转发给 Glue extension；窗口计划、日志和流程控制
  已不在 TerminalApp legacy 聚合单元执行。
- 验证：Release `ext` 重编 ExtCoreApi、Glue loader/extension 并全部成功；`link /dump /exports Ext.dll`
  确认两个导出 `GetExtCoreApiVersion`、`GetExtWorkspaceWindowRefreshPlan`；强制包含
  `WorkspaceTerminalPageLegacyGlue.cpp` 后 TerminalApp 成功，随后 no-op 为 `ninja: no work to do`；
  Release `full --parallel 8` 成功。
- ABI 加固复测：布尔字段改为显式 `uint8_t` 并添加保留字节后，Release `ext` 成功重编 ExtCoreApi、
  Glue loader 并链接两个 DLL；ext-only repack 成功。该调整消除了跨 DLL 协议依赖 C++ `bool` 布局的风险。

## P2-2 回退后的完整产品回归基线（2026-08-24）

- Release `full --parallel 8` 连续两次成功。第二次的实际阶段耗时为：native foundation **2 秒**、
  terminal-settings-adapter **0 秒**、native-product-shims **1 秒**、extension containers **0 秒**、
  full repack **14 秒**。
- 即使 C++ 目标为 no-op，`full` 仍会解包 XAML runtime、生成 PRI 并创建产品包；这解释了完整命令的
  约 **23 秒** 墙钟时间，不能归因给 Glue/Ext/TerminalApp 的 C++ 耦合。现有 PRI263 资源警告为既有
  打包警告，未在本次改动中新增。

## P2-2 当前 Workspace ID 切换迁移（2026-08-24）

- Ext API 升级至 v3，新增 `GetExtWorkspaceCurrentIdChangePlan`。它将 previous/next/last/baseline
  workspace ID 转换为固定布局的变更计划（resolved last ID、reset baseline、heartbeat 开关），字符串
  仍由调用方的 UTF-16 缓冲区承载。
- Glue extension 现在拥有 ID 切换的比较、诊断、计划调用和 Glue 状态更新；
  `TerminalPage::CurrentWorkspaceId(value)` 仅转发给 extension。
- TerminalApp 保留两个生命周期安全的宿主动作：配置已有 weak-reference 保护的 DispatcherTimer，及
  聚合 tab row/交互/关闭按钮/chat UI 刷新。它不再计算 workspace ID 计划或写 Glue 状态。
- 验证：Release `ext` 重编 ExtCoreApi、Glue loader/extension 并成功；强制 legacy 聚合单元后的
  TerminalApp 成功、随后 no-op；导出检查确认 v3 的三个符号；Release `full --parallel 8` 成功，
  extension containers 0 秒、最终 repack 13 秒。

## P2-4 物理头依赖方向收敛（2026-08-24）

- `WorkspaceHostInterfaces.h`、`WorkspacePageStateTypes.h`、`WorkspaceDllApi.h` 已从 `src/glue` 迁至
  中立 `src/contracts`，分别命名为 `GluePageHostContract.h`、`GluePageStateTypes.h`、
  `GluePageContractApi.h`。TerminalApp 与 Glue 现在共同包含契约目录，而不是 TerminalApp 包含 Glue
  目录中的 host interface。
- 已将 `WorkspaceDiagnosticLog.h` 迁至 `src/core/chat`；实现仍由 `WorkspaceChatCore` 编译，
  TerminalApp 不再为了诊断 API 包含 `src/glue/chat`。
- 验证：Release ext/Glue 成功；强制 TerminalApp 成功、后续 no-op；Release full 成功。
- 审计仍发现两类未完成物理反向包含：`TerminalPageWorkspace*Surface/Includes` 的 legacy member
  声明片段。它们不是中立契约，不能通过改名掩盖；后续切片须迁移成员实现。

## P2-4 Ext runtime service 消除 WorkspaceHostBridge（2026-08-24）

- Ext API 升级至 v4，导出 `GetExtWorkspaceDefinitionsPath` 与
  `RemoveExtPersistedWorkspaceWindowState`。前者采用调用方 UTF-16 缓冲区，后者只接收 window ID；
  两者不暴露 Glue 或 SettingsModel implementation 类型。
- 新建中立 `src/contracts/ExtCoreRuntimeClient`，由 TerminalApp 编译。它仅动态加载 Ext、校验 API
  版本并解析上述 C ABI；`AppLogic` 的文件监控路径与 `TerminalWindow` 析构时的状态清理改经此客户端。
- TerminalApp 已不再包含 `WorkspaceHostBridge.h` 或 `WorkspaceLegacyApiBridge.h`。编译过程中发现
  ExtCoreApi 对路径解析的间接包含，已补齐 `WorkspacePersistencePaths.h`，使 Ext API 自包含。
- 验证：Release Ext v4、Glue 成功；TerminalApp 重编 runtime client、AppLogic、TerminalWindow 后成功，
  后续 no-op；导出检查确认两个 v4 服务符号；Release `full --parallel 8` 成功，containers 0 秒、
  repack 14 秒。

## P2-4 TerminalPage legacy 声明归属迁移（2026-08-24）

- 已将 10 个 `TerminalPageWorkspace*Surface/Includes` 头从 `src/glue/workspace` 移入
  `microsoft/src/cascadia/TerminalApp`，统一改名 `WorkspacePageLegacy*`。它们描述的都是
  `TerminalPage` 私有成员/方法，不是 Glue 的稳定接口；`TerminalPage.h` 现只包含本模块、
  `src/contracts` 和 `src/core` 头。
- `WorkspaceTerminalPageLegacyGlue.cpp` 仍是 TerminalApp 的 CMake 聚合实现源，保留在 Glue 目录；
  本检查点只修正声明的物理所有权，未虚假宣称 legacy 成员实现已经脱离宿主。实现迁移必须按页面和
  生命周期切片完成后再单独删除该聚合单元。
- 验证：Release `ext` 成功；强制 TerminalApp 后成功，随后 `TerminalApp --parallel 8` 为
  `ninja: no work to do.`；针对 TerminalApp 源的旧 `src/glue/workspace/TerminalPageWorkspace*`
  包含扫描为空；Release `full --parallel 8` 成功（本轮墙钟约 **31 秒**，输出显示 extension
  containers 为 no-op）。
- 完整构建仍以 XAML runtime 解包、PRI/资源合并和 MSIX 创建为主；现有 PRI263 警告为既有包资源
  警告，本轮未新增。下一检查点是逐个拆除 `WorkspaceTerminalPageLegacyGlue.cpp` 内的成员实现，
  每个切片都必须证明 Ext 与非相关宿主目标不重编。

## P2-4 TerminalPage legacy 聚合编排归属（2026-08-24）

- `TerminalPageWorkspaceCppIncludes.h`、`TerminalPageWorkspacePrelude.cpp` 与
  `TerminalPageWorkspaceGlue.cpp` 的编排职责已迁入 TerminalApp，分别改为
  `WorkspacePageLegacyCppIncludes.h`、`WorkspacePageLegacyPrelude.inc`、
  `WorkspacePageLegacyGlue.inc`；唯一的 `WorkspaceTerminalPageLegacyGlue.cpp` 继续文本包含它们。
- 编排文件使用 `.inc`，避免被 TerminalApp 的 CMake source glob 当成独立 `.cpp` 编译；若直接移动为
  普通 cpp，会与聚合翻译单元重复定义。12 个实际实现片段仍在 Glue 路径，尚未宣称完成实现迁移。
- 验证：强制 `WorkspaceTerminalPageLegacyGlue.cpp` 后 Release TerminalApp 成功；随后 no-op。
  TerminalApp 对旧编排文件名的扫描为空；Release `full --parallel 8` 成功，extension containers
  为 no-op。完整构建墙钟约 **31 秒**，仍主要耗在资源/包处理。

## P2-5 Startup 状态迁移边界验证（2026-08-24）

- 尝试将 startup workspace 状态加载、pending 输入可见性队列与 node-ID 队列完全放入 Glue extension。
  编译成功，但 Glue 链接 `LoadWorkspaceStartupState` 时产生 `LNK2019`：该 Settings facade 实现未导出给
  `Glue.dll`。
- 已完整撤回该尝试；Ext 与 TerminalApp 随后成功，TerminalApp 最终为 no-op。没有通过追加
  Settings/core 静态库来掩盖错误，因为那会使 Glue 拥有第二份 core/facade 实现并破坏 DLL 边界。
- 后续实现条件：Ext API v5 增加 startup plan C ABI。协议使用固定宽度字段，输入为 workspace ID，
  输出使用调用方提供的 `uint8_t` visibility buffer 与 UTF-16 NUL-separated node-ID buffer，并先返回所需
  长度。Glue 拥有解析后的队列；TerminalApp 只保留启动调度。完成该 ABI 后再删除
  `PrepareStartupWorkspaceState` / `ClearPendingWorkspaceStartupState` 两个 host 转发。

## P2-5 Startup 状态 Ext v5 ABI 落实（2026-08-25）

- Ext API 已升级至 v5，新增 `GetExtWorkspaceStartupPlan`。计划只包含固定宽度的可见性条目数和
  node-ID UTF-16 字符数；调用方以 `uint8_t` 数组接收可见性，以连续 NUL 终止字符串缓冲区接收 ID。
  所有长度与容量均显式传递，零长度队列不要求缓冲区。
- Glue loader 校验 v5 并解析新导出，通过两段式调用获得并解析 plan。`WorkspaceTerminalPageExtension`
  现拥有 startup ID 的消费、当前 ID 切换、pending 队列填充及完成后的清理；TerminalApp 的
  `PrepareStartupWorkspaceState` / `ClearPendingWorkspaceStartupState` host 接口、实现和 legacy 定义均已删除。
- 验证：Release Ext/Glue 成功（ExtCoreApi、loader、extension 重编）；强制 legacy 聚合后的
  TerminalApp 成功，随后 no-op；`link /dump /exports Ext.dll` 确认 `GetExtCoreApiVersion` 与
  `GetExtWorkspaceStartupPlan`；Release `full --parallel 8` 成功，extension containers **1 秒**、
  full repack **13 秒**。
- 删除 host 虚接口后，`TerminalWindow.cpp` 的陈旧对象曾保留旧虚表引用并出现 LNK2001；强制重编该
  间接包含者后链接成功。这是增量依赖追踪缺口的验证记录，不是新 ABI 的运行时依赖。

## P2-5 Startup 决策纯转发删除（2026-08-25）

- `_ShouldSkipWorkspaceStartupAction` 没有独立策略，只将同一组参数转发给
  `IWorkspaceTerminalPageExtension::ShouldSkipStartupAction`。三个 legacy 调用点已直接使用 extension，
  并删除 TerminalPage 的声明和定义。
- 验证：强制 legacy 聚合后的 Release TerminalApp 成功；后续 `TerminalApp --parallel 8` 为 no-op；
  对 TerminalApp/Glue 源的旧成员名扫描为空。该切片未变更 Ext 或打包输入，未混入无关的 full 构建。

## P2-5 Pending node 队列消费转发删除（2026-08-25）

- 删除 `_HasPendingWorkspaceNodeInputVisibility`、`_ConsumePendingWorkspaceNodeInputVisibility` 与
  `_ConsumePendingWorkspaceNodeId` 三个无状态包装器。Tab 创建及 launch runtime 现在直接消费
  extension 所有的队列。
- 保留 `_PreparePendingWorkspaceNodeInputVisibility` / `_PreparePendingWorkspaceNodeIds`：它们可处理尚未
  持久化的临时 workspace 定义，不能错误改为按 workspace ID 查询的 Ext v5 startup plan。
- 验证：强制 legacy 聚合后的 Release TerminalApp 成功，后续为 no-op，三个旧成员名扫描为空。

## P2-5 未调用 startup-action 包装器删除（2026-08-25）

- `_PreparePendingWorkspaceNodeStartupAction` 经全树审计只剩 legacy 声明与定义，实际启动路径已直接使用
  extension 的 `TrackWorkspaceNodeStartupAction` / `ShouldSkipStartupAction`。已删除这段死包装，避免将
  extension 状态再次绕回 TerminalPage。
- 验证：强制 legacy 聚合后的 Release TerminalApp 成功，后续为 no-op；旧成员名扫描为空。

## P2-5 Dead workspace node icon updater 删除（2026-08-25）

- 全量引用审计发现 `_ApplyWorkspaceNodeIcon` 仅有 legacy 声明和定义，没有事件或业务调用路径。
  它曾按 node 索引查找 tab 并调用 `_UpdateTabIcon`；删除不会改变任何可达行为。
- 验证：强制 legacy 聚合后的 Release TerminalApp 成功，后续为 no-op，旧成员名扫描为空。至此该审计
  范围内没有其余仅“声明+定义”的 `TerminalPage::_*` workspace 成员；下一切片必须是带窄 facade 的
  实际页面/生命周期迁移。

## P2-6 图标选择器 UI facade 前置条件（2026-08-25，进行中）

- 新增 `TerminalPageBase::ShowWorkspaceDialog(ContentDialog)`：Glue 只能请求显示一个给定的 dialog 并等待
  结果，不能取得 `TerminalPage`、窗口句柄或 `IDialogPresenter`。TerminalApp 保留 presenter 的空值处理。
- 该动作是图标选择器实际迁移的唯一对话框宿主能力；SettingsEditor 的 `IconPicker` 是独立 RuntimeClass，
  不复用也不链接，避免把另一套页面/XAML 生命周期带回 Glue。
- 验证：Release Ext/Glue 成功；强制 TerminalApp 成功，随后 no-op。状态为**进行中**：下一步将把当前
  图标选择器的 dialog 构建协程编译进 Glue，TerminalApp 只保留弱生命周期、读取初值和提交选择。

## P2-6 图标选择器 UI 实际迁移（2026-08-25）

- 已新增 Glue 编译单元 `WorkspaceManagerIconPickerDialog.cpp`，承载图标 family、预览网格、本地图片
  选择与 dialog 协程；`IWorkspaceTerminalPageExtension::PickWorkspaceManagerIcon` 以
  `IAsyncOperation<winrt::hstring>` 返回选择结果。使用 `hstring` 是 C++/WinRT async ABI 的必要约束，
  不将 `std::wstring` 作为 WinRT 异步泛型跨模块返回。
- TerminalPage 仅保留工作区/节点初值读取、协程恢复后的 editor model 提交、以及 node 级诊断。它经
  `ShowWorkspaceDialog` 提供展示能力，不向 Glue 暴露 presenter、窗口或 TerminalPage 对象。
- `WorkspaceManagerIconPickerGlue.cpp` 已从 421 行收敛为 89 行宿主入口；旧的 335 行 dialog/XAML
  组装和图标预览逻辑不再被 `WorkspaceTerminalPageLegacyGlue.cpp` 编译。
- 验证：Release `ext --parallel 8` 成功（新 dialog 编译 **5 秒**、extension Glue link **1 秒**、
  ext repack **4 秒**）；强制 legacy 聚合后的 Release TerminalApp 成功，随后 no-op；Release
  `full --parallel 8` 成功，extension containers **1 秒**、full repack **15 秒**。完整构建仍以
  XAML runtime/PRI/MSIX 打包为主，PRI263 为既有资源警告，未新增。

## P2-7 管理页导航 Ext v7 策略迁移（2026-08-25）

- 导航编码与 nav→workspace/node index 解析已由纯 core `WorkspaceCoreNavigation.cpp` 经 Ext v7 的
  `GetExtWorkspaceManagerNavigationPlan` 导出。计划是固定布局，只接受 index/count/nav 标量，返回
  workspace、node、editor selection 与可选 resolved index。
- Glue loader 解析 v7 导出；管理页原有四个 TerminalPage 导航方法现为薄转发，规则不再调用
  SettingsModel facade。Glue extension 不读取 `WorkspaceManager::Workspaces()`；该数量和选中 index
  由 host 作为值传入，避免对未导出 C++ 成员产生链接依赖。
- 管理页内对 `ResolveWorkspaceIndexFromManagerNavSelection` / `ResolveWorkspaceNodeIndexFromManagerNavSelection`
  的直接 SettingsModel facade 调用扫描为空；构建页所需的 index 解析均经 Glue extension → Ext loader。
- 验证：Release `ext --parallel 8` 成功；强制 TerminalApp 后完成，随后 no-op；`link /dump /exports
  Ext.dll` 确认 `GetExtCoreApiVersion` 与 `GetExtWorkspaceManagerNavigationPlan`。随后 Release
  `full --parallel 8` 成功，extension containers **1 秒**；最终完整打包仍由 PRI/MSIX 固定开销主导。

## P2-8 管理页节点移动 core 规则与增量构建图修复（2026-08-25）

- 新增纯 core `MoveWorkspaceManagerVisibleNode`：只允许相邻可见节点交换，并在一次操作中同步
  `Workspace::Nodes` 与 `TabOrder`。SettingsModel facade 只进行 core/WinRT 值转换；管理页回调不再自己
  校验、`swap` 和重建 TabOrder，只负责标记 editor dirty 与选中目标节点。
- 排除一次实际链接风险：`TerminalSettingsModel/Workspace.cpp` 通过文本包含 `WorkspaceModel.cpp`，但此前
  未将嵌套 workspace 分片声明为 CMake `OBJECT_DEPENDS`。因此新增 facade 时宿主错误复用了旧 object 并报
  `LNK2019`。现已为该 wrapper 列出 core 与 facade 分片；此后相关变更会准确重编这个唯一 wrapper，而不需要
  clean build，也不会保留陈旧实现。
- 验证：Release `ext --parallel 8` 成功（ext repack **4 秒**）；强制 Release TerminalApp 成功，后续
  `TerminalApp --parallel 8` 为 `ninja: no work to do.`；Release `full --parallel 8` 成功，extension
  containers **1 秒**。PRI263 是既有资源警告，本轮未新增。

## P2-9 节点模板复制规则与 legacy 聚合依赖（2026-08-25）

- 新增 core `ApplyWorkspaceManagerNodeTemplate`：模板复制保留刚生成节点的 ID 与名称，其他 node 定义从模板
  取得。回调仅调用既有新增节点动作、该规则、host settings 颜色处理和导航更新；profile/UI settings 未进入 core。
- `WorkspaceTerminalPageLegacyGlue.cpp` 的 `.inc` 编排和 12 个 Glue 分片现已列入 `OBJECT_DEPENDS`。此前编辑
  `WorkspaceManagerContentGlue.cpp` 会被 Ninja 错误判成 no-op；现已保证改动必定触发该聚合单元重编。
- 验证：Release TerminalApp 在 CMake 重生成后成功且后续 no-op；Release `ext --parallel 8` 成功（Glue
  extension 编译 **6 秒**、Ext wrapper 编译 **8 秒**、ext repack **4 秒**）。下一轮做完整打包回归。

## P2-10 管理页删除确认 Glue UI 迁移（2026-08-25）

- 新增 `WorkspaceManagerDeleteConfirmationDialog.cpp` 到 Glue.dll。它统一构建“删除工作区/删除节点”确认框，
  仅通过已有 `ShowWorkspaceDialog` host capability 返回 `bool`。新增 extension 异步方法
  `ConfirmWorkspaceManagerDeletion(bool)`；没有把 presenter、窗口或 TerminalPage 指针交给 Glue。
- `WorkspaceManagerEditorPanels.cpp` 的两处 coroutine 已删除直接 `_dialogPresenter` 使用。host 仍拥有 weak
  生命周期检查，并只在确认后调用既有 definition/node 删除动作，因此 model 权限与选中状态不跨 DLL。
- 验证：Release `ext --parallel 8` 成功（新 dialog **3 秒**、extension **7 秒**、repack **4 秒**）；
  Release TerminalApp 成功并后续 no-op；Release `full --parallel 8` 成功，extension containers **1 秒**、
  full repack **13 秒**。PRI263 为既有资源警告。

## P2-11 管理页路径选择 capability 收敛（2026-08-25）

- 新增 Glue `WorkspaceManagerPathPicker` 与 extension async 方法；node editor 的文件/文件夹选择现在通过
  Glue 请求 `TerminalPageBase::PickWorkspacePath(bool)`，只返回 `hstring` 路径。editor 面板不再直接读
  `_hostingHwnd`、调用 `OpenFilePicker` 或配置 `FOS_PICKFOLDERS`。
- HWND 与原生 COM picker 仍封装在 `TerminalPage::PickWorkspacePath`，因为其 owner 是宿主生命周期能力；
  Glue 不取得窗口句柄，仍仅拥有请求和结果。节点 model 提交、dirty 标记与 weak 生命周期检查保留在 host。
- 验证：Release Ext/Glue 成功（新 path picker **1 秒**、extension **7 秒**、repack **4 秒**）；Release
  TerminalApp 成功且后续 no-op；Release `full --parallel 8` 成功，extension containers **1 秒**、full repack
  **14 秒**。PRI263 为既有资源警告。

## P2-12 管理页 Profile picker UI 迁移（2026-08-25）

- 新增契约值类型 `WorkspaceProfileOption` 与 Glue `WorkspaceManagerProfilePicker`。它在 Glue.dll 内构建
  ComboBoxItem、Tag、选中项与 enabled 状态；默认节点和节点编辑两个实际下拉框均改为调用 extension。
- Host 仅将活动 profiles 变为 `Guid/DisplayName` 快照。节点的非活动旧 profile fallback 仍在 host 解析后作为
  普通 snapshot option 传入；选择变化后的 model 写入、dirty、图标和颜色预览仍留在 host。
- 验证：Release `ext --parallel 8` 成功（新 picker **2 秒**、Glue link **1 秒**、repack **3 秒**）；Release
  TerminalApp 成功且复跑 no-op；Release `full --parallel 8` 成功，extension containers **0 秒**、full repack
  **14 秒**。PRI263 为既有资源警告。

## P2-13 管理页颜色选择 dialog 迁移（2026-08-25）

- 新增 Glue `WorkspaceManagerColorPickerDialog` 与异步 extension 方法。它在 Glue.dll 内创建 `ColorPicker`
  和确认/取消 dialog，确认时返回 `#RRGGBB`；workspace 背景色和 node tab 色两处均改由它发起。
- Host 保留 palette 自动分配、颜色预览、node 色解析、model 写回和 dirty 权限；协程恢复后重新验证所选
  workspace/node 仍存在，取消不会写入。这避免把 SettingsModel 或 editor 可变状态带入 Glue。
- 验证：Release `ext --parallel 8` 成功（新 dialog **3 秒**、Glue link **1 秒**、repack **2 秒**）；Release
  TerminalApp 成功且复跑 no-op；Release `full --parallel 8` 成功，extension containers **1 秒**、full repack
  **14 秒**。PRI263 为既有资源警告。

## P2-14 图标选择流程从 legacy Host 聚合移入 Glue（2026-08-25）

- `WorkspaceTerminalPageExtension` 新增 workspace/node 图标选择 `IAsyncAction`：它拥有读取初值、await
  Glue picker、节点诊断及确认后提交的完整流程。编辑面板不再调用 `TerminalPage::_ShowWorkspaceManager*IconPicker`。
- `TerminalPageBase` 只新增四个字符串值 capability（workspace/node 当前 icon、workspace/node 应用 icon）；
  Host 不将 editor manager、model 指针、窗口或诊断实现跨 DLL 暴露。旧 89 行
  `WorkspaceManagerIconPickerGlue.cpp` 已从 legacy `.inc` 聚合和 `TerminalPage` 私有声明移除。
- 验证：Release Ext/Glue 在共享 clean 后恢复成功，随后 `ext` 仅 repack **2 秒**；Release TerminalApp
  恢复构建成功且复跑 `ninja: no work to do.`；Release `full --parallel 8` 成功，extension containers
  **1 秒**、full repack **13 秒**；legacy include/声明扫描旧图标 coroutine 为零。
- Debug `ext --parallel 8` 与 `TerminalApp --parallel 8` 亦已在本切片后成功退出；前者完成 Glue.dll
  链接 **2 秒**、ext repack **13 秒**，后者完成 `TerminalAppLib` 与 `TerminalApp.dll` 链接。该轮因共享
  clean 后恢复上游 WinMD/XAML 而非正常增量，故不写入页面增量性能基线。

## P2-15 无调用 WorkspaceSaver 入口删除（2026-08-25）

- 引用审计确认 `_OpenWorkspaceSaver` 仅有 `WorkspacePageLegacyMethods.h` 声明和一个空实现；没有命令、
  事件或业务调用方。已删除这对无效 API。仍由 XAML 绑定的键盘/关闭确认处理保持不变。
- Debug `TerminalApp --parallel 8` 成功：XAML 重新生成后，`WorkspaceTerminalPageLegacyGlue.cpp`、
  `TerminalAppLib` 和 `TerminalApp.dll` 全部链接通过。该构建显示 Host 聚合头变动会触发整个页面路径，
  因而不作为普通页面增量指标；后续切片继续优先迁出可稳定封装的行为。
- 随后的同命令无改动复跑为 **0.76 秒**，无实际 Ninja 工作步骤；本切片未遗留增量失效。
- Release `full --parallel 8` 亦成功：`WorkspaceTerminalPageLegacyGlue.cpp` 重新编译并完成产品包，
  extension containers **1 秒**、full repack **13 秒**。PRI263 为既有资源警告，未出现新增错误。

## P2-16 工作区/节点删除事务迁入 Glue（2026-08-25）

- `TerminalPageBase` 新增仅按稳定 workspace/node ID 执行删除的两个 capability；Host 继续拥有窗口关闭、
  持久化、选中状态和页面刷新等本地副作用，未暴露 `TerminalPage`、窗口或 editor model。
- `WorkspaceTerminalPageExtension` 新增两个 `IAsyncAction`，在 Glue 内执行确认 dialog 和确认后的 Host
  提交。`WorkspaceManagerEditorPanels.cpp` 的两个删除按钮只启动 extension action，不再直接调用
  `_RemoveWorkspaceDefinitionById` 或 `_RemoveWorkspaceNodeById`。
- Debug Ext/Glue 与 TerminalApp 均成功链接；面板私有删除调用扫描为 **0**。本次 contract 头变动触发的
  WinMD/XAML 共享恢复不计入页面增量基线。
- Release `TerminalApp --parallel 8` 亦成功：legacy 聚合、`TerminalAppLib` 和 `TerminalApp.dll` 完成
  链接；未出现新增编译或资源错误。

## P2-7 TerminalAppLib PCH 实测（2026-08-25）

- 在 `TerminalApp/pch.h` 加入并随后删除一行无语义标记，分别执行 Debug
  `TerminalApp --parallel 8`。PCH 失效构建为 **30.78 秒**；删除标记后的恢复构建为 **9.31 秒**。
- 这项测量不触发 Ext/Glue 编译，证明页面代码继续进入 `TerminalPage.h`/PCH 会直接扩大 Host 编辑路径。
  后续页面迁移优先使用既有 `GluePageHostContract.h` 的窄 capability，而不在 `TerminalPage.h` 暴露模型或 UI 类型。

## P2-6 Debug full 阻塞修复与页面热点复测（2026-08-25）

- Debug `full` 首次在 `WindowsTerminal/AppHost.cpp` 失败：代码包含已不存在的
  `src/glue/chat/WorkspaceDiagnosticLog.h`，实际诊断 API 已归属 `src/core/chat`。已修为 Core 路径；
  单独 `WindowsTerminal` 随后成功编译链接，未增加页面依赖。
- 修复后 Debug `full --parallel 8` 成功。实际容器阶段重编 `WorkspaceManagerPaneContent.cpp`：Glue source
  **70 秒**、Glue.dll link **3 秒**、extension containers **74 秒**、full repack **15 秒**。
  该 source 是当前下一优先级编译热点；后续将把其独立于 manager 编辑逻辑的部分进一步切出，避免一般页面改动
  反复触发 70 秒编译。

## P2-7 Glue pane-content PCH 根因审计（2026-08-25）

- `WorkspaceManagerPaneContent.cpp` 本身只有 **69 行**，且已设置 `SKIP_PRECOMPILE_HEADERS`，但它在
  `TerminalApp` 源目录内手工 `#include "pch.h"`；按同目录规则这解析为 TerminalApp 的重型 PCH，解释了
  69–70 秒的单文件编译时间。
- 临时移除该 include 后暴露了隐式的 XAML、SettingsModel projection、WIL、`LibraryIncludes`/资源宏依赖。
  这证明不能在该单文件拼凑大量隐式头；已恢复原 include 并复建成功（Glue source **69 秒**、link **2 秒**、
  repack **6 秒**），没有留下失败产物。
- 后续优化项明确为：为 Glue pane-content 建立**专用轻量 PCH**，以显式、稳定的公共投影头替换同目录
  TerminalApp PCH 解析；该项仍未完成，不能把本次审计称为性能优化完成。

## P2-7 Glue pane-content PCH 优化落地（2026-08-25）

- 上述“仍未完成”已由本次实现取代：删除该源的 `SKIP_PRECOMPILE_HEADERS`，移除同目录
  `#include "pch.h"`，使其使用 Glue target 的 PCH；源文件只显式包含实际所需的
  `LibraryIncludes`、`LibraryResources`、WIL、SettingsModel 和 XAML 投影头。
- Debug `ext --parallel 8` 的同一源重编实测为：Glue source **7 秒**、Glue.dll link
  **2 秒**、ext repack **4 秒**。相对原始 **69–70 秒** source 编译，单文件编译时间减少约
  **90%**；Ext core 与 TerminalApp 均未参与源码重编译。
- Release 同一单源无语义改动实测：Glue source **5 秒**、Glue.dll link **1 秒**、ext repack
  **4 秒**；恢复测量标记后又成功构建（source **5 秒**、link **0 秒**、repack **5 秒**）。因此
  Release 也从原先 Debug 基线可见的 69–70 秒级热点降至个位数秒；Release `TerminalApp` 与
  `full` 也已通过。
- 仍有 11 个 workspace/chat 源片段虽在 `src/glue`，却以文本 include 方式定义 `TerminalPage::`
  成员并由 TerminalAppLib 编译。manager content 的页面构建及编辑保存属于该遗留组；下一切片须将
  “构建页面 + 用户动作”改为 Glue extension API，不能仅移动 cpp 文件。

## P2-2 manager 导航宿主 API 收敛（2026-08-25）

- 删除仅供 manager 页面内部使用的三个 `TerminalPage` 成员：workspace、workspace-node、editor
  navigation selection。页面改为直接调用已有的 `IWorkspaceTerminalPageExtension` 导航计算 API。
- 同步删除 `WorkspacePageLegacyMethods.h` 声明；全树调用点审计未发现旧成员名。Debug `TerminalApp`
  重新编译 `WorkspaceTerminalPageLegacyGlue.cpp` 后成功链接。该切片减少了宿主公开的页面计算 API，
  但 manager 内容构建本体仍未迁出，不能把 P2-2 标为完成。
- manager 内容构建器的 editor manager、selection、edit-mode 与 dirty 状态读取已全部改经
  `IWorkspaceTerminalPageExtension` 访问；扫描无遗留宿主字段名，Debug `TerminalApp` 重新编译并
  成功链接。另移除了仅返回 `Workspace.Name` 的 `_WorkspaceDisplayName` 宿主成员，三个调用点改为
  直接使用模型数据；Debug `TerminalApp` 再次成功。
- 新增 `TerminalPageBase::WorkspaceSettings()` 的**值返回**只读 capability，并由 Glue extension
  透传；manager 内容构建器已用它替代两处 `_settings` 直接读取，且 current-workspace-id 读取也已
  切到 extension state。Debug `ext`（Glue.dll 链接）与 Debug `TerminalApp` 均成功，未将宿主字段
  或实现类型跨 DLL 暴露。
- 导航应用已进一步收敛为 `IWorkspaceTerminalPageExtension::NavigateWorkspaceManager`：Glue 计算/保存
  selection，宿主只执行“选择 workspace”和“重建内容”两个动作。首次尝试让 Glue 直接调用
  `WorkspaceManager::Workspaces()` 曾在 `Glue.dll` 链接时触发 LNK2019，已改为由遗留页面传入标量
  `workspaceCount`/`selectedWorkspaceIndex`，避免把未导出的 Settings implementation 依赖反向带入 Glue。
- 保存和重置编辑器也收敛为 extension 编排：宿主 contract 仅保留 `SaveWorkspaceManagerEdits` 与
  `ReloadWorkspaceManagerEdits` 两个动作；manager 页面不再直接调用 `_SaveWorkspaceEditorState` 或
  `_LoadWorkspaceEditorState`。重载后的 workspace 数量和选择项以 `WorkspaceManagerEditorView` 标量快照
  返回，避免 Glue 直接链接 Settings implementation。Debug 与 Release 的 `ext`、`TerminalApp` 都已通过；
  Release 最终复核中 TerminalApp 为 no-op，Ext 只执行 **3 秒** artifact repack。
  页面构建本体和 XAML 事件入口仍在 TerminalApp 的遗留聚合单元，P2-2 继续保持进行中。

## P2-6 本轮 Release full 回归（2026-08-25）

- 在保存/重置 capability 和 `WorkspaceManagerEditorView` 标量快照落地后，`Release full --parallel 8`
  成功退出。第二次增量复核的实际阶段耗时为：native-product-foundation **2 秒**、
  terminal-settings-adapter **0 秒**、native-product-shims **1 秒**、extension-containers **1 秒**、
  full-repack **14 秒**。
- extension containers 为 no-op，说明本轮 contract/legacy manager 变更没有诱发 Ext/Glue 的额外源码重编；
  `full` 的主要墙钟时间仍来自 XAML runtime 解包、PRI 和产品包重建。PRI263 为既有警告，退出码为 0。

## P2-2 manager 新增动作收敛（2026-08-25）

- manager 内容的四条新增路径（空白/模板 workspace、空白/模板 node）不再直接调用
  `_SetSelectedWorkspaceIndex`、`_AddWorkspaceDefinition`、`_AddWorkspaceNode`。它们经 extension 转发到
  host 的两个受控动作，并只接收 `WorkspaceManagerMutationView { Changed, WorkspaceCount,
  SelectedWorkspaceIndex, SelectedWorkspaceNodeCount }`。
- 这使 Glue 负责后续 navigation/template orchestration，host 保留依赖资源字符串、活动 profile、颜色分配和
  Settings implementation 的创建行为。传回的全是标量，未把 workspace model 引入 DLL ABI。
- 首次 Debug 编译发现迁移后遗留未使用 `nodes` 局部变量；项目将 C4189 升级为错误。已删除该变量，Debug
  `TerminalApp` 重新编译 legacy aggregation、链接成功，最终复跑 `ninja: no work to do`。Release 与 full
  回归留在此切片的下一次变更合并后执行；P2-2 仍进行中。

## P2-2 editor 图标工厂边界验证（2026-08-25）

- 尝试将 manager 内容和 editor panel 的 `_CreateNewTabFlyoutIcon` 替换为 Glue 编译的
  `CreateWorkspaceIconElement`。独立 `ext` 构建在 `IconPathConverter.g.h` 失败：UIHelpers 的生成投影未向
  Glue target 提供 include/ABI，不能将 TerminalApp 的私有投影目录倒灌给 Glue。
- 已完整撤回该 helper、CMake source 和调用改动；Debug `ext` 成功（repack **3 秒**），Debug `TerminalApp`
  最终为 no-op。该依赖被列为物理页面迁移的前置项，不计作已完成迁移。

## P2-2 workspace 通用表单 mutation 收敛（2026-08-25）

- workspace manager 的名称、描述、默认启动目录现在经 `WorkspaceManagerWorkspaceTextField`；默认输入框、
  默认标题和默认显示 tab 经 `WorkspaceManagerWorkspaceBoolField`；默认 profile 的 GUID/显示名以单个原子
  action 更新。上述回调不再从页面取得 `Workspace*` 并直接写入。
- 背景色 picker 和“换一个”调色板分别收敛为字符串 get/apply/rotate capability。调色板去重仍在 host，
  因为它需遍历 Settings implementation；Glue 只拥有 color dialog、等待结果和预览刷新。
- Debug 每次 extension 编译为 **8–9 秒**、Glue link **2–3 秒**、repack **2–4 秒**；本切片最终
  Debug `TerminalApp` 为 no-op。颜色 action 首次因 `TerminalPage.cpp` 缺少未限定 facade 名而失败，已改为
  `Settings::Model::implementation::PickWorkspacePaletteColor` 的正式 wrapper 并通过。node 表单、结构重排和
  图标 factory 仍未迁出，P2-2 保持进行中。

## P2-2 node 文本字段 mutation 收敛（2026-08-25）

- node 名称、启动目录和启动命令均通过 `UpdateWorkspaceManagerNodeText(nodeIndex, field, value)` 写入。
  host 执行选中 workspace/node index 校验与 dirty 标记；路径 picker 在 await 返回后重新取得 weak page 的
  strong 引用，只有 mutation 成功才更新 TextBox。
- 首次编译发现协程中的 `strong` 重取在替换时遗漏，已恢复。Debug Ext：extension **8 秒**、link **2 秒**、
  repack **4 秒**；Debug TerminalApp 重编 legacy aggregation/链接后成功，最终复跑 no-op。
- 尚余 node 的 profile、三个布尔值、tab color、图标预览以及 workspace/node 结构重排；这些仍是物理页面
  迁移前需要收敛的 host-private 访问。

## P2-6 Release 完整构建回归（2026-08-25）

- `cmake --build build --config Release --target full --parallel 8` 成功退出。该次增量打包的
  `native-product-foundation` 为 **26 秒**（包含 `AppHost.cpp` 对 Core 诊断头修复的编译/链接），
  `terminal-settings-adapter` **1 秒**、`native-product-shims` **0 秒**、
  `extension-containers-full` **1 秒**、`full-repack` **13 秒**。
- 输出中的 PRI263 是已有资源限定符告警；命令退出码为 0，未出现 Ext/Glue/TerminalApp 的编译或链接错误。

## P2-2 node 选项与预览只读边界收敛（2026-08-25）

- node 的 `ShowTab`、`ShowInputPanel`、`UseNodeNameAsTabTitle` 以及 profile 选择均已改为 extension 的
  原子 mutation；Glue 回调不再持有并写入 `WorkspaceNode`。tab 色读取、应用和调色板轮换同样经受控
  capability 完成，host 保留 Settings 相关的自动颜色分配与 dirty 标记。
- 本轮进一步将两个容易被忽略的只读反向依赖迁出：节点图标预览由
  `WorkspaceManagerNodeIconPreviewForEditing` 返回“显式图标或 profile resolved icon”；标签色预览由
  `WorkspaceManagerNodeTabColorPreview` 返回已按 Settings 解析的颜色文本。Glue 只创建 `IconElement` 和
  `Brush`，不再读取 `_SelectedWorkspaceForEditing()`、`_settings` 或 legacy 色彩 helper，且保留原有回退语义。
- Debug 验证：Ext 的 extension 编译 **9 秒**、Glue link **2 秒**、artifact repack **2 秒**；随后
  `TerminalApp` 完整重编/链接完成，复跑为 `ninja: no work to do`。首次尝试在普通 host cpp 使用
  legacy-local `_tryParseGuid` 失败，改为独立的 `winrt::guid` 解析并捕获无效旧 GUID；颜色解析则改用
  Settings Model 的正式 `ResolveWorkspaceNodeTabColor` API。两项修复没有把 legacy helper 或私有投影引入新边界。
- 该文件中剩余的 `_SelectedWorkspaceForEditing` / `_settings` 读取仅用于一次性初始表单快照、结构重排的
  workspace/node ID 捕获、以及将 ActiveProfiles 转为 picker snapshot。下一切片应将初始 workspace/node
  editor snapshot 设计为纯值 DTO；在 DTO 到位前，不把物理 legacy 聚合页迁移误标为完成。

## P2-2 可见节点重排 action 收敛（2026-08-25）

- `DragItemsCompleted` 不再读取/重排 `Workspace::Nodes` 或设置 dirty 标记。Glue 仅按 ListView Tag 提交
  `vector<hstring>` 的可见 node-ID 顺序；host action `ReorderWorkspaceManagerVisibleNodes` 负责写 TabOrder、
  按该顺序重建可见节点、追加隐藏节点并标脏。
- Debug 实测先暴露一个实际类型问题：独立 host cpp 中未限定 `WorkspaceNode` 没有 legacy aggregation 的别名，
  改为从 `workspace->Nodes` 推导容器后消除错误。最终 `TerminalApp` 编译、链接成功，复跑为 no-op；Ext 复跑
  仅 artifact repack **12 秒**。当前面板的直接 host 读取降为两处初始 workspace/node 快照以及四处
  ActiveProfiles/旧 profile fallback，DTO 仍是下一主切片。

## P2-2 profile 值快照与排序图标回退收敛（2026-08-25）

- `WorkspaceManagerProfileOptionsForEditing(currentGuid, currentName)` 现在在 host 将 ActiveProfiles 和不在
  列表内的旧 GUID fallback 统一转换为 `WorkspaceProfileOption` 值列表；Glue 仅构建 picker。排序列表的图标
  也改用既有 `WorkspaceManagerNodeIconPreviewForEditing`，从而复用显式图标/profile 图标回退，面板内已无
  `_settings` 读取。
- Debug Ext 实测 extension compile **10 秒**、Glue link **4 秒**、repack **4 秒**；Debug TerminalApp
  编译/链接成功，最终复跑为 no-op。扫描仅剩两个 `_SelectedWorkspaceForEditing`：workspace/node 初始表单
  数据源。这两个读取将以一个版本化纯值 editor snapshot DTO 一并替换，不继续添加单字段 getter。
- Release Ext 成功（最终 repack **2 秒**）。Release TerminalApp 已完成所有编译阶段，但最后写入
  `microsoft/bin/x64/Release/TerminalApp/TerminalApp.dll` 时 LNK1104；检测到多个既有 `OpenConsole` 进程，
  输出 DLL 被占用。未终止用户进程，故 Release 包装回归待释放该 DLL 后复跑；这是输出文件锁而非源码/链接符号错误。
