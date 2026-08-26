# 第二阶段：Ext Core 模块边界设计

## 目标

将当前 `WorkspaceExtension.dll` 升级为 `Ext.dll`：它承载 workspace 的 core 与
Settings Model facade，不承载 `TerminalPage`、XAML 控件、页面状态或页面生命周期。
页面功能位于通用 `Glue.dll` 容器，而不是 `TerminalAppLib`；`TerminalApp` 只通过宿主
接口加载 Glue。未来所有新增页面均遵守 `TerminalApp -> Glue.dll -> Ext.dll` 的模式。

本阶段的边界是编译边界，不改变 workspace 文件格式、启动动作和页面交互语义。

## 当前依赖问题

当前 `WorkspaceExtension` 的单一目标混合了三类职责：

| 层 | 代表实现 | 不应依赖 |
| --- | --- | --- |
| Core | `src/core/workspace/WorkspaceCore*.cpp` | TerminalPage、XAML、TerminalControl |
| Model facade | `WorkspaceModel.cpp`、`WorkspaceFacade*.cpp` | 页面状态和页面控件 |
| Page glue | `WorkspaceTerminalPageExtension.cpp`、chat/UI state | 反向装入 core DLL |

因此该 DLL 目前链接 TerminalControl、SettingsEditor、UIHelpers、UIMarkdown 和 XAML
头文件；同时 `TerminalPage.cpp` 在运行时加载这个 DLL。页面微小修改会触发 DLL 的
PCH、编译和链接，core 改动也会被 UI 依赖放大。

此外，`TerminalSettingsModel/Workspace.cpp` 已直接包含 `WorkspaceModel.cpp`。第二阶段
不把这份既有 model 集成强行改成跨 DLL ABI；否则必须先为所有 model 类建立稳定的
导出 ABI。这是下一检查点才允许做的动作，不是本轮安全的机械迁移。

## 目标图

```text
TerminalApp
  └─ 宿主接口与容器挂载点；加载/卸载 Glue.dll

Glue.dll
  └─ 所有页面 glue：workspace 管理 UI、WorkspaceTerminalPageExtension、chat 页面状态
       └─ 加载/校验/卸载 Ext.dll；允许 TerminalPage / XAML / TerminalControl

Ext.dll
  └─ Workspace core + Settings Model facade
       └─ Settings Model、WinRTUtils、ConTypes（允许）

禁止：Ext.dll ──> TerminalApp / TerminalPage / XAML UI / SettingsEditor / UIMarkdown
```

`TerminalPage` 只加载和卸载通用 `Glue.dll`，并只依赖窄宿主接口和通用
`CreateGlueTerminalPageExtension` / `DestroyGlueTerminalPageExtension` 工厂；不再通过
workspace 专属 DLL 名称或 factory 名称绑定容器。`WorkspaceExtLoader`（位于 Glue）负责加载、版本校验和卸载 `Ext.dll`。
页面 implementation 留在 `src/glue/` 并由 Glue 编译；`Ext.dll` 作为 page-free core
产物被打包。

现有 `WorkspaceTerminalPageLegacyGlue.cpp` 中仍有直接定义 `TerminalPage` 私有成员的
历史片段；它们必须逐项收敛为 `TerminalPageBase` 宿主接口后才可完全迁入 Glue。新增
页面不得新增这种 host-private 片段：必须从第一天起只通过 Glue 的窄宿主接口访问
TerminalApp。

## 实施顺序与检查点

1. 建立通用 `Glue.dll` 页面容器，迁移 page extension 与页面状态；TerminalApp 只加载 Glue。
2. 将 core 输出和打包名称迁移为 `Ext.dll`，并从目标源集中删除 page/chat 源。
3. 将 Ext 的链接集合收缩到 core/facade 实际需要的库；每移除一组库都构建验证。
4. 用 include/link 审计证明 Ext 不再包含页面头、页面源、XAML/UI 链。
5. 分别测量 `TerminalAppLib` 和 `Ext` 的 clean / PCH / 增量构建时间，更新本文件的
   实测表。不能以理论估算替代计时。

## 编译耗时优化机制与验收

| 变化 | 消除的重编译扇出 | 验收方式 |
| --- | --- | --- |
| 页面 extension 编入 Glue.dll | 页面改动不重编 Ext 或 TerminalApp | 仅改 page 源后 Ext/TerminalApp 无重编译 |
| Ext 去除 chat/page 源 | chat/XAML 头不再进入 Ext PCH/TU | 编译命令和目标源审计 |
| Ext 链接白名单 | UI/Editor 产物不再是 Ext 链接前置 | `target_link_libraries` 审计 + 链接成功 |
| 输出改名为 Ext | 产品内 core DLL 命名与职责一致 | 完整打包中存在 `Ext.dll` |

计时必须使用同一配置、相同并行度，至少记录 clean build、PCH rebuild、仅 core
修改、仅 page 修改四种情形。第一阶段历史基准只作参照，不能和本阶段的拆分后单目标
时间直接相加比较。

## 已验证的增量效果与缓存前提

在 Debug、`--parallel 1` 下，修复 Ninja 依赖缓存后得到的实测结果为：

| 触发 | Ext | Glue | TerminalApp | 结论 |
| --- | ---: | ---: | ---: | --- |
| 无源码改动 | 1.00 秒 / 0 步 | 0.80 秒 / 0 步 | 0.85 秒 / 0 步 | 三目标稳定 no-op |
| 仅 `ExtCoreApi.cpp` | 4.57 秒 / 2 步 | 不参与 | 不参与 | 只编译 core TU 并链接 Ext |
| 仅页面 extension | 0.81 秒 / 0 步 | 8.02 秒 / 3 步 | 不参与 | 页面不再使 Ext 重编译 |
| Ext PCH 失效 | 约 100 秒 / 5 步 | 不参与 | 不参与 | 只重编 Ext PCH、3 个 Ext TU 与链接 |

此前 781 秒、89 步的“无改动 Ext 构建”已被证明不是模块依赖扇出：Ninja 每次报告
`premature end of file; recovering`，根因为损坏的 `.ninja_log` / `.ninja_deps`。两个文件
均已无损备份后重建；恢复后 no-op 如上表。`VerifyGlobs.cmake_force` 只导致 CMake glob
检查，重复配置得到的 `build.ninja` 内容不变，不是重编译根因。

## 遗留宿主页面迁移切片

当前 `WorkspaceTerminalPageLegacyGlue.cpp` 以文本包含方式引入 12 个直接定义
`TerminalPage::` 私有成员的片段；TerminalApp 还编译 workspace chat 核心与
`WorkspaceManagerPaneContent.cpp`、`WorkspaceIconPickerDialog.cpp`。不能仅改 CMake target：
那会在 Glue 中访问 TerminalPage 私有字段，造成 ABI 与生命周期错误。

后续按以下可验证的顺序迁移：

1. 将 `TerminalPageBase` 拆成稳定的 `TerminalPageHostFacade`：只暴露“获取/设置活动 tab、
   请求打开页面、调度 UI 回调、持久化 workspace、页面宿主控件操作”等行为，不暴露
   `TerminalPage` 数据成员或 WinRT implementation 类型的可写引用。
2. 先迁移无状态的 workspace manager 页面组（manager content、icon picker、editor panels），
   令其成为 Glue extension 成员，并由 facade 提供所需宿主操作；每组完成即从
   `_terminal_app_sources` 删除，验证 TerminalApp 目标不再编译该源。
3. 再迁移 chat dispatch / terminal glue；chat 状态全部归 Glue 所有，TerminalApp 只转发
   control/tab 事件给 Glue。随后从 TerminalApp 删除 chat core 编译项。
4. 最后迁移 startup/save/launch 生命周期片段，并移除 `WorkspaceTerminalPageLegacyGlue.cpp`。

`WorkspaceManagerPaneContent` 不能作为“单 cpp 改 target”的例外：它实现 TerminalApp 的
`IPaneContent`，因此必须经由 Glue extension 的接口传递，不能让宿主保存 Glue implementation
指针。`WorkspaceIconPickerDialog` 的情况不同：真实调用路径已经在 Glue 的
`WorkspaceManagerIconPickerGlue.cpp` 中直接构造通用 `ContentDialog`；TerminalApp 的同名
XAML RuntimeClass 从未在该路径中激活，可以连同其 IDL/XAML/实现注册一并退役，而不应把它
复制到 Glue。

其中 `WorkspaceManagerPaneContent` 已完成第一步迁移：它不是 XAML RuntimeClass，而是程序化
`IPaneContent` 包装器。其实现现由 Glue 编译；TerminalPage 只持有 `IPaneContent`，通过
`IGlueTerminalPageExtension` 的 create/update 操作使用页面。实测仅改该 cpp 时，Glue 编译
70 秒、链接 2 秒，TerminalApp 为 no-op。图标选择器的实际页面同样在 Glue：已退役 TerminalApp
中未使用的 XAML RuntimeClass，并在 WinMD 合并前显式清理其旧生成 WinMD，防止陈旧 activation
factory 重新进入投影。

每个切片必须重新测量“仅该页面源改动”：Ext 与 TerminalApp 必须为 no-op，Glue 只重编
该切片和 Glue 链接。未满足这个检查点的切片不得标记为完成。

### Pane-content 编译热点的已实施优化

`WorkspaceManagerPaneContent.cpp` 位于 TerminalApp 源目录，原先手工包含同目录重型
`TerminalApp/pch.h`，且 CMake 又对它禁用 Glue PCH。这个 69 行的页面包装器因此单源编译
需要 69–70 秒。现已取消该源的 `SKIP_PRECOMPILE_HEADERS`，删除同目录 PCH include，并将
实际使用的 Library/WIL/SettingsModel/XAML 依赖显式列出；它现在使用 Glue target 的 PCH，
不会重新把 TerminalApp PCH 作为页面输入。

同配置 Debug、`--parallel 8` 下的单源重编从 69–70 秒降至 **7 秒**（约 90%）；Glue 链接
为 2 秒、扩展重新打包为 4 秒。Release 的同源实测为 **5 秒**（Glue 链接 1 秒、重打包 4 秒），
恢复测量标记后再构建仍为 5 秒。该路径已通过 Debug、Release Ext、Release TerminalApp 和
Release full 构建验证。

## Ext v2 窗口状态计划 ABI

窗口状态刷新是首个需要跨 Ext/Glue/TerminalApp 的生命周期切片。Ext 持有 workspace-core
计划实现，Glue 持有流程与诊断，TerminalApp 唯一保留 `ApplicationState` 的提交权限。为避免
Glue 链接未导出的 SettingsModel implementation，Ext v2 导出
`GetExtWorkspaceWindowRefreshPlan`：调用方传入窗口 ID、当前 workspace ID，以及自有 UTF-16 输出
缓冲区；返回值为版本化固定布局的计划（skip、是否清理 pending launch、进程 ID、输出字符串长度）。
协议中的布尔值采用 `uint8_t`，并保留 6 个扩展字节；不以 C++ `bool` 的对象布局作为跨 DLL 契约。

Glue 的 loader 在模块加载时校验 API 版本并解析该导出；缺少符号或版本不符会拒绝加载。Glue 不跨
DLL 保存 C++ 对象、也不链接 Ext 静态实现。TerminalApp 仅实现两个窄 host 操作：读取 WindowId，
以及在计划要求时清理 pending launch 并 Flush。这样 `RefreshWorkspaceWindowState` 的决策和诊断已在
Glue，而 SettingsModel 的私有实现与页面容器隔离。

Ext v3 以同一规则导出 `GetExtWorkspaceCurrentIdChangePlan`。Glue 负责比较当前 ID、记录诊断、
更新其 workspace state 和调用该计划；TerminalApp 的 setter 只转发。由于 DispatcherTimer 的回调
必须用 `TerminalPage` 的 `weakThis` 保证销毁安全，宿主保留“启动/停止心跳”动作；另保留一个聚合
UI 刷新动作。二者不接触 workspace 状态或 SettingsModel implementation，因此仍是单向 Glue → host
能力调用，而非把页面逻辑带回 TerminalApp。

## 中立契约头与剩余反向包含

`TerminalApp` 不应包含 `src/glue` 中的 ABI 定义。host interface、页面状态记录和 DLL 导出宏已移到
`src/contracts`，诊断公开头已移到 `src/core/chat`；二者现在由 host 与 Glue 共同依赖。这是物理包含
方向的修正，不引入新的 DLL 链接边。

仍处于迁移中的 `TerminalPageWorkspace*Surface/Includes` 是为 legacy 文本聚合保留的成员声明，不是
合法长期契约；它们必须随相应生命周期切片被删除，不能迁到 `contracts` 后继续让 TerminalApp 使用。

原 `WorkspaceHostBridge` 的两个 host 使用点（workspace 定义路径与窗口状态清理）已替换为 Ext v4
runtime service。`src/contracts/ExtCoreRuntimeClient` 是通用 host-side client：它只加载 Ext、检查版本、
调用 C ABI，完全不包含或链接 Glue。这样 TerminalApp 的文件监控和析构清理仍可在页面容器未加载时
执行，同时不再经由 Glue 的 legacy bridge 访问 workspace 实现。

## 后续边界（不在本阶段混入）

## 共享聊天核心编译边界

聊天存储、输入解析、事件记录与状态 helper 都不依赖 `TerminalPage`、XAML 或 Ext 的页面
contract。它们被同时直接编入 Glue 与 TerminalApp 时，会使每次 chat core 改动各编译一次。
现在这些翻译单元统一由 `WorkspaceChatCore.lib` 编译；Glue 和仍含 legacy host glue 的
TerminalApp 仅链接该静态库。`WorkspaceDiagnosticLog.cpp` 也已归入该共享库；legacy host
仍可通过头文件调用公开函数，但不再拥有该实现的独立编译副本。

实测仅触碰 `WorkspaceChatStateHelpers.cpp`（Release，`--parallel 1`）时，Glue 路径为 **4 步、
约 3 秒**：一个 core TU、`WorkspaceChatCore.lib`、Glue 链接及 glob 检查；之后 TerminalApp
为 **1 个实际工作步骤**，只重链 `TerminalApp.dll`，没有重新编译任一 chat core TU。该共享库
不进入 Ext，因此 Ext 继续保持 page-free core DLL 的边界。由于 `WindowsTerminal.exe` 还直接链接
`TerminalAppLib`，该静态库通过 `TerminalAppLib` 的 PUBLIC link interface 传播；不能只挂到
`TerminalApp.dll`，否则最终产品链接会缺失 chat 符号。

`WorkspaceDiagnosticLog.cpp` 同样归入该共享实现，而不是 Glue 页面代码。它不依赖页面或宿主，
但其历史 `pch.h` 依赖曾让 Windows `min` 宏、宽字符 fmt overload 和 UTF/WIL helpers 隐式可用。
现在这些依赖全部由诊断模块自身显式声明；因此可以安全地从 Glue、TerminalApp、SettingsModel 与
WindowsTerminal 的 source list 删除，只通过共享库链接。

## 后续边界（不在本阶段混入）

将 `WorkspaceModel.cpp` 的唯一实现完全迁入 Ext 并使 SettingsModel 通过稳定 ABI 调用，
需要先设计 C ABI 或 WinRT contract、所有权规则与版本兼容性。该项在本阶段完成页面/core
物理拆分和测量后，再单独立项。

## TerminalPage legacy 声明归属

`TerminalPageWorkspace*Surface/Includes` 不属于 Glue 对外能力：它们只是 `TerminalPage` 私有成员、
方法声明和 legacy 文本聚合所需的宿主头片段。将它们放在 `src/glue/workspace` 会诱导
TerminalApp 反向包含 Glue，且把宿主私有布局伪装成模块契约。

该组 10 个头已迁至 `microsoft/src/cascadia/TerminalApp` 并改名为
`WorkspacePageLegacy*`；`TerminalPage.h` 只包含同目录头、`src/contracts` 和 `src/core` 的公开头，
不再直接包含 `src/glue/workspace/TerminalPageWorkspace*`。这不是把 legacy 实现塞进契约层：
`WorkspaceTerminalPageLegacyGlue.cpp` 仍暂留在 Glue 目录、仍作为 TerminalApp 的聚合实现源，
后续只有在相应成员实现完成迁移或删除时才移动/移除它。

该修正使物理头依赖与运行时边界一致：Ext 提供 core ABI，Glue 提供页面 extension，TerminalApp
保有自己的私有声明与生命周期实现；共享类型只经 `src/contracts`，共享 chat API 只经 `src/core/chat`。

聚合的 include 编排同样属于宿主：它现位于 TerminalApp，使用 `.inc` 后缀并由唯一的 legacy
聚合翻译单元文本包含。不能将它直接作为 `.cpp` 放入自动 glob 的 TerminalApp 源目录，否则 CMake 会
单独编译它且与聚合单元产生重复定义。实际页面实现片段仍在 Glue 路径，直到通过窄 host facade
完成逐片迁移；路径整理不等同于实现解耦。

## Startup 状态的 ABI 前置条件

对 startup workspace 状态的迁移验证表明，`LoadWorkspaceStartupState` 的 Glue facade 并未导出给
`Glue.dll`。让 Glue 直接调用它会在 DLL 链接阶段产生 `LNK2019`；将 Settings facade 或 core 静态实现
额外链接进 Glue 会复制实现并重新引入依赖，不能作为修复。

因此该切片必须先以 Ext C ABI 提供版本化 startup plan：固定布局元数据、调用方提供的
`uint8_t` 可见性缓冲区和 UTF-16 node-ID 缓冲区（长度/容量均显式传递）。Glue 解析并拥有队列，
TerminalApp 只维持调度入口。该 ABI 未实现前保留现有两个窄 host 转发，避免以错误的二进制依赖
换取表面上的源文件迁移。

该前置条件现已由 Ext v5 实现：`GetExtWorkspaceStartupPlan` 返回可见性条目数和 node-ID 字符数；
node ID 以连续的 NUL 终止 UTF-16 字符串编码。Glue 先调用取得容量、再分配自身缓冲区并解析为队列；
Ext/Glue 间没有 STL、WinRT 或 Settings implementation 对象。`OnPreparingStartupActions` 现在由
extension 设置当前 ID、取得 plan、填充队列并清除 startup ID，完成后直接清队列；两条对应的
TerminalPage host 转发已删除。

## 图标选择器的 Glue 页面边界

图标选择器是第一个迁入 Glue 的实际交互式页面切片。`WorkspaceManagerIconPickerDialog.cpp`
在 Glue.dll 内创建 family 切换、图标预览网格和本地图片选择，并通过
`TerminalPageBase::ShowWorkspaceDialog` 请求宿主展示 `ContentDialog`。这个 facade 只传递 dialog
和结果：Glue 不持有 `TerminalPage`、`IDialogPresenter`、窗口句柄或 workspace editor 的可变状态。

选择结果跨 extension interface 以 `winrt::hstring` 返回，而不是 `std::wstring` 作为
`IAsyncOperation` 的泛型实参；前者是可投影、ABI 稳定的 WinRT 类型。宿主入口仅在协程恢复后读取
当前 workspace/node 的初值并提交已选择的字符串，保留节点诊断、空值/no-op 判断和 editor state
修改权限。原先在 `WorkspaceManagerIconPickerGlue.cpp` 的 335 行 XAML 构建实现已经删除，避免同一 UI
逻辑继续进入 TerminalApp 的 legacy 聚合翻译单元。

此模式是后续 Glue 页面迁移的模板：页面构建、局部交互和临时选择 state 属于 Glue；Tab/窗口生命周期
与最终 model 提交留在 host；跨边界只新增最窄的 WinRT 值和受控 host capability。

## 管理页导航的 Ext v7 边界

workspace 管理页的导航编码本身是 core policy，但其历史 facade 位于 SettingsModel，Glue 不能链接。
Ext v7 因此导出 `GetExtWorkspaceManagerNavigationPlan`：调用方传入四个标量（workspace/node index、
workspace count、selected index 和当前 nav value），取得固定布局的 workspace/node/editor selection
以及可选的 resolved workspace/node index。没有 SettingsModel 对象、STL 容器或 C++/WinRT 对象穿过 DLL。

Glue loader 校验 API v7 并动态解析该函数；Glue extension 保留 nav state，TerminalApp 只传递其已有
editor state 的两个标量、按返回索引更新本地选中项并重建页面。这样既不将编码规则复制到 Glue，也不让
Glue 链接未导出的 SettingsModel implementation。

随后的 manager 操作收敛也遵循同一规则：extension 负责调用导航计划并维护 selection，host contract
只提供“选择 workspace”“刷新内容”“保存编辑”“重新加载编辑”动作。重新加载不返回 `WorkspaceManager`，而是
返回 `WorkspaceManagerEditorView { WorkspaceCount, SelectedWorkspaceIndex }` 标量快照。实际验证曾表明，
在 Glue.dll 内直接调用 `WorkspaceManager::Workspaces()` 会产生 LNK2019，因为该 Settings implementation
未导出；这个快照既保留重置后的导航语义，又不重新引入反向二进制依赖。

manager 编辑面板的图标转换也完成过独立链接验证：`IconPathConverter` 的 `IconPathConverter.g.h` 属于
UIHelpers 的生成投影，当前没有作为 Glue target 的公开构建依赖。直接把它包含进 Glue 会在编译期失败，
而不是可接受的“补一个 include”。因此本阶段保留 `_CreateNewTabFlyoutIcon` 这项 host 依赖；要迁走它，
必须先单独提供 UIHelpers 的正式投影/ABI 契约，不能以 TerminalApp 私有生成目录作为 Glue 的反向依赖。

## 编辑器节点移动的 core 规则与构建依赖

节点移动不是页面策略：它必须验证只能在相邻的可见节点间移动，并在交换后同步持久化的 `TabOrder`。
该规则现由 `WorkspaceCoreNavigation.cpp` 的 `MoveWorkspaceManagerVisibleNode` 单点实现，SettingsModel
facade 仅作值转换；管理页回调只保留触发、将 editor 标脏与更新导航。这样 drag/reorder 或将来的 Glue
页面不必复制隐藏节点、边界和 TabOrder 的一致性规则。

`TerminalSettingsModel/Workspace.cpp` 是将这些分割实现文本包含进宿主的包装翻译单元。CMake/Ninja 不会在
第一次构建前可靠发现其嵌套 `.cpp` 包含，因此必须在该 source 的 `OBJECT_DEPENDS` 中列出 workspace core
与 facade 分片。否则改了 Glue/Core 实现却仍链接旧 object，会造成“增量构建成功、行为未更新”。该依赖
只让真正共享该包装单元的变更重编它，既修复正确性，也避免靠全量 clean 作为日常构建机制。

“从节点模板新增”保留刚生成节点的 ID/名称、复制模板其余值，是独立的 editor 规则；它由
`ApplyWorkspaceManagerNodeTemplate` 统一实现。颜色分配仍在 host，因为它依赖活动 profile 与 UI settings；
core 因而不会获得页面或设置依赖。

编辑器删除确认框遵循同一页面边界：Glue 创建 workspace/node 两种 `ContentDialog` 并通过
`TerminalPageBase::ShowWorkspaceDialog` 等待结果，返回单个 `bool`；host 只在确认后删除对应 definition。
这移除了 editor 面板对 `_dialogPresenter` 的直接依赖，同时不把 model 删除权限、页面 weak 生命周期或选中状态
暴露给 Glue。

原生文件/文件夹 picker 需要窗口 owner，故 `TerminalPage` 保留 `PickWorkspacePath(bool)` 的实现并在内部使用
HWND；Glue 的 `WorkspaceManagerPathPicker` 仅发起该窄 capability 并返回 `hstring` 路径。editor 面板不再包含
文件对话框 flags、COM callback 或 `_hostingHwnd` 访问，选择后的 node state 提交仍由 host 控制。

## Profile picker 的中立快照边界

Profile 下拉框的 XAML 项构建、Tag、选中定位与 enabled 状态属于 Glue。host 将 `ActiveProfiles()` 转换为
`WorkspaceProfileOption { Guid, DisplayName }` 的值快照，并把当前 GUID 与编辑权限传给
`CreateWorkspaceManagerProfilePicker`；Glue 不接触 SettingsModel implementation 或 workspace model。

节点若引用不在活动 profile 列表中的旧 GUID，host 仍负责按其既有 settings 查找规则生成一个 fallback 值快照；
Glue 只把它当作普通 option。最终 `SelectionChanged` 写回 model、dirty 标记以及图标/颜色预览仍由 host 完成。
这既迁出了两个实际 picker 的 UI 构建，又不把 Settings 查找或保存权限倒灌到 Glue。

## 颜色选择 dialog 的页面边界

workspace 背景色与 node tab 色现在共用 Glue 的 `WorkspaceManagerColorPickerDialog`。Glue 创建
`ColorPicker`、处理取消/确认并以 `#RRGGBB` 值返回；它只借用 `ShowWorkspaceDialog`，不接触选中
workspace、node index、settings 或 dirty 状态。Host 在协程恢复后重新取得当前 model，只有非空确认结果
才写回并刷新预览。原来的两个 Host `ColorPickupFlyout` 因而不再构建页面 UI。

## 图标选择流程从 legacy 聚合移出

图标 dialog 早已在 Glue；本轮进一步将“读取初值 → await picker → 诊断 → 提交”的两个协程移入
`WorkspaceTerminalPageExtension`。Host 仅提供四个以值传递的 capability：读取当前 workspace/node icon，
以及提交 workspace/node icon。它不暴露 editor manager、选中 workspace 指针或诊断对象给 Glue。

因此 `WorkspaceManagerIconPickerGlue.cpp` 已从 `WorkspaceTerminalPageLegacyGlue.cpp` 的文本聚合集移除，
编辑面板直接调用 extension 的 `IAsyncAction`。这是真正减少 TerminalApp 编译输入的一块页面流程迁移，
而不是仅把 dialog 代码换目录。

## 编辑预览的解析值契约

页面预览不能以“只读”为理由继续访问 host 的对象图。节点图标虽有显式值，还存在 profile 图标回退；节点
tab 色同样需由 Settings 解析自动色与 profile 继承色。若 Glue 自己完成这些解析，它就必须链接
`WorkspaceManager`、`CascadiaSettings` 或 TerminalApp 的 legacy helper，重新形成反向依赖。

因此契约提供两个解析后的标量 capability：`WorkspaceManagerNodeIconPreviewForEditing(nodeIndex)` 和
`WorkspaceManagerNodeTabColorPreview(nodeIndex)`。前者返回可直接渲染的图标路径，后者返回可直接交给 XAML
颜色 parser 的色值；host 负责索引校验、profile/Settings 查找与无效旧 GUID 的降级。Glue 负责 UI 元素、刷新
时机和空值的通用 fallback。该分工使页面未来可整体迁到 Glue，而不让 Glue 知道 workspace 模型的实际布局。

这也说明下一步应使用 DTO 而非更多 model getter：初始 workspace/node 表单还需要多个字段、profile option 和
结构 ID，适合一次传递 `WorkspaceManagerEditorSnapshot` 的纯值版本化快照；不适合向契约逐个泄漏
`Workspace*`、Settings 或容器引用。

## 节点排序的命令边界

排序 UI 的输出不是新的 workspace 模型，而是用户在列表中形成的可见 node-ID 序列。Glue 因而只提交这组
`hstring` 值；host 的 `ReorderWorkspaceManagerVisibleNodes` 验证当前选择、更新 `TabOrder`、重建节点顺序并把
未显示 tab 的节点追加回来。这样隐藏节点保护和 dirty 状态都不依赖页面实现，也避免了将 `WorkspaceNode`
容器定义暴露给 Glue。这个命令模式可复用于将来的通用页面：UI 产生意图与标量标识，host/核心拥有模型变换。
