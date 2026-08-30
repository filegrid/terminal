# 当前分支改动向 `ext\src` 收敛计划

> 2026-07-14 回退校正：`TerminalPage.cpp` 应尽量贴近 `main`，只把 workspace/chat 这类 fork 新增逻辑留在 `ext`。此前搬到 `ext\src\workspace` 的通用 lifecycle / terminal events / tab drag / new-tab-flyout / interaction / control / window state / settings UI / helpers / teaching tips / window actions / theme runtime / suggestions 逻辑，现已全部移回 `TerminalPage.cpp`，对应 glue 文件也已删除。下面较早的“已移到 ext”条目仅代表历史过程，不再代表当前代码形态。

## 目标

对比 `main` 后，当前分支的定制改动要尽量收敛到 `ext\src` 下维护；后续新增功能也尽量避免继续改动 fork 过来的原始代码树。

这里的核心不是“一次性把所有 fork 差异清零”，而是：

1. **把当前仍在演进的定制逻辑先抽离到 `ext` 自有目录。**
2. **把原始代码树中的改动压缩成最薄的一层接线/入口。**
3. **给后续开发定一条约束：默认改 `ext`，只有缺少挂点时才最小化修改原始树。**

## 当前观察

### 1. 与 `main` 的整体差异

当前分支相对 `main` 的历史差异面很大，不能指望一次重构就把所有历史 fork 差异都搬干净。  
因此本轮应优先处理 **当前仍在维护、且已经明显形成“定制层”的改动面**。

### 2. 当前工作区中的活跃改动

当前未提交改动可粗分为：

| 区域 | 数量 | 说明 |
| --- | ---: | --- |
| `ext\src` | 11 | 已有一部分定制代码沉淀在这里，说明方向是对的 |
| `microsoft\...` | 15 | 仍有较多逻辑直接改在 fork 原始树里 |
| `ext` 其他 | 3 | 文档/记录类 |
| 根目录其他 | 2 | 顶层构建与忽略规则 |

### 3. 当前最需要收敛的原始树改动点

按改动面看，优先级最高的是这几组：

| 领域 | 当前落在原始树的文件 |
| --- | --- |
| workspace chat / UI / 路由 | `TerminalPage.cpp/.h`、`AppLogic.cpp`、`Resources.resw`、`TerminalAppLib.vcxproj*` |
| workspace 持久化 / 模型 | `Workspace.cpp/.h`、`WorkspaceTests.cpp`、`TabTests.cpp` |
| portable 构建 / 打包 | 顶层 `CMakeLists.txt`；旧 PowerShell/WAP 迁移见 `docs/plan/cmake-ninja-msbuild-migration.md` |
| 低层补丁 | `til\mutex.h` |

## 收敛原则

### 原则 1：新增逻辑默认进 `ext`

- 新代码放 `ext\src`
- 新文档放 `ext\docs`
- 功能变更记录继续写 `ext\READ`

### 原则 2：原始树只保留“薄接线”

原始树允许保留的改动，原则上只限于：

1. 注册/包含 `ext` 代码
2. 调用 `ext` 提供的入口
3. 暴露必要 hook / callback / interface
4. 项目文件把 `ext` 源码编进现有目标

不再把完整业务逻辑、状态机、存储规则、诊断流程直接堆在 `microsoft\src\...` 里。

### 原则 3：没有挂点时，先补挂点，再把逻辑外移

如果原始代码当前没有可复用扩展点：

1. 先在原始树加一个**最小** hook
2. 把真实实现放到 `ext\src`
3. 后续继续迭代只改 `ext`

### 原则 4：portable 构建只有一个入口

- 构建入口只保留仓库根 CMake
- portable 定制实现放到 `ext\src\portable`
- 已迁移的旧脚本/工程立即删除，不保留参数透传或兼容壳

### 原则 5：每次改动都必须先做“层级归属判断”

这是强制步骤，不是建议步骤。开始写代码前，必须先判断本次改动属于哪一层：

1. `core`
2. `glue`
3. 原始树薄接线

判定标准如下：

- 业务逻辑、状态规则、持久化规则、校验规则、启动规划、运行态判定，默认归 `ext\src\core`
- Terminal/WinRT/UI 对象适配、事件接线、参数转发、运行时日志落点，归 `ext\src\glue`
- `microsoft\src\...` 只允许保留最小 hook、注册、include、接口暴露、构建挂接

禁止跳过这一步，直接按“哪里改起来快就先改哪里”的方式落代码。

### 原则 6：排障允许临时落在 `glue`，但修复完成后必须回收

为了定位问题，可以临时在 `glue` 增加：

1. 运行时日志
2. 观测性字段
3. 过渡性参数透传

但以下内容不允许以“排障”为理由长期留在 `glue`：

1. 状态机
2. 持久化/共享内存规则
3. 启动目录/启动动作判定
4. 工作区打开状态/进程存活校验

如果某次修复为了提速先把这类逻辑放进了 `glue`，结束前必须补一个回收动作：

1. 下沉到 `ext\src\core`
2. 让 `glue` 只保留调用
3. 在变更说明里明确这次是否还有临时逻辑未回收

### 原则 7：提交前必须做边界自检

在认为“已经修好”之前，至少自检下面三项：

1. 这次新增代码里，是否有业务规则误落在 `glue`
2. 这次新增代码里，是否有完整实现误落在 `microsoft\src\...`
3. 如果用了 `glue` 临时排障，是否已经回收，或者是否明确记录未回收项

如果这三项没有过，不应把改动视为完成。

## 建议的收敛结构

### A. Workspace Chat 相关

**目标：** `TerminalPage` / `AppLogic` 里只保留调用点，把定制行为收敛到 `ext\src\chat`

建议归位：

| 类型 | 目标位置 |
| --- | --- |
| 聊天提交策略 | `ext\src\chat` |
| 诊断日志 | `ext\src\chat` |
| workspace 存储路径规则 | `ext\src\chat` |
| Copilot/CLI 识别与路由 | `ext\src\chat` |
| 模拟/调试辅助工具 | `ext\src\chat` |

原始树保留内容：

- `TerminalPage` 中的 UI 事件接入点
- `AppLogic` 中的应用级初始化/转发
- `Resources.resw` 中不可避免的资源声明
- 工程文件中的编译入口

当前已完成的收敛包括：

- `WorkspaceChatController/Store/StoragePaths/DiagnosticLog/TerminalEventStore/CopilotTerminalSimulator` 已归位到 `ext\src\chat`
- `TerminalPage.cpp` 中一批 workspace chat diagnostics helper 已下沉到 `WorkspaceDiagnosticLog.*`
- workspace chat 文本裁剪、终端输入归一化、输出摘要、交互 CLI 判定等纯规则 helper 已继续下沉到 `WorkspaceChatTextHelpers.*`
- workspace chat 的 workspaceKey / draftKey / stateKey 生成、cwd 捕获同步、两阶段提交偏好判定等状态规则 helper 已继续下沉到 `WorkspaceChatStateHelpers.*`
- `TerminalPage.cpp` 中一整段 workspace chat method body 也已移到 `ext\src\chat\WorkspaceChatTerminalGlue.cpp`，原文件改为通过 include 保留薄桥接
- `TerminalPage.cpp` 中一整段 workspace flyout / open-workspace / lock-state / tab-row method body 也已移到 `ext\src\workspace\WorkspaceTerminalGlue.cpp`，原文件改为通过 include 保留薄桥接
- `TerminalPage.cpp` 中一整段 workspace/chat UI 状态 method body 也已移到 `ext\src\workspace\WorkspaceUiStateGlue.cpp`，原文件改为通过 include 保留薄桥接
- `TerminalPage.cpp` 中一整段 workspace chat dispatch / keyboard submit / draft / input-height / collapse-state method body 也已移到 `ext\src\chat\WorkspaceChatDispatchGlue.cpp`，原文件改为通过 include 保留薄桥接
- `TerminalPage.cpp` 中一整段 workspace save/runtime method body 也已移到 `ext\src\workspace\WorkspaceSaveRuntimeGlue.cpp`，原文件改为通过 include 保留薄桥接
- `TerminalPage.cpp` 中一整段 workspace editor method body 也已移到 `ext\src\workspace\WorkspaceEditorGlue.cpp`，原文件改为通过 include 保留薄桥接
- `TerminalPage.cpp` 中一整段 workspace saver / baseline method body 也已移到 `ext\src\workspace\WorkspaceSaverGlue.cpp`，原文件改为通过 include 保留薄桥接
- `TerminalPage.cpp` 中大块 `BuildWorkspaceManagerContent()` UI 构造也已移到 `ext\src\workspace\WorkspaceManagerContentGlue.cpp`，原文件改为通过 include 保留薄桥接
- `TerminalPage.cpp` 中 workspace confirm-save / saver key handling 也已并入 `ext\src\workspace\WorkspaceSaverGlue.cpp`，workspace window-state / startup-action / runtime metadata 相关 method body 已移到 `ext\src\workspace\WorkspaceLaunchRuntimeGlue.cpp`
- `TerminalPage.h` 里的 workspace chat submit transport、terminal routing/capture state、workspace runtime state 等自定义类型也已抽到 `ext\src\workspace\TerminalPageWorkspaceTypes.h`，头文件内仅保留薄别名维持兼容
- `TerminalPage.cpp` 中 `Create()` 里的 workspace tab-row 接线与 `ProcessStartupActions()` 里的 startup workspace 预处理也已移到 `ext\src\workspace\WorkspaceCreateGlue.cpp`
- `TerminalPage.cpp` 顶部匿名 namespace 里剩余的大块 workspace helper（runtime metadata 推断、SSH startup replay、颜色规则、node 查找、workspace layout 比较）也已移到 `ext\src\workspace\WorkspaceTerminalPageHelpers.cpp`
- `TerminalPage.cpp` 里最后剩下的显式 workspace method `_BuildWorkspaceNodeArgs()` 也已并入 `ext\src\workspace\WorkspaceSaveRuntimeGlue.cpp`
- `TerminalPage.cpp` 里的 `_RegisterTerminalEvents()` / `_RegisterTabEvents()` 也已移到 `ext\src\workspace\WorkspaceTerminalEventsGlue.cpp`
- `TerminalPage.cpp` 顶部剩余的 workspace/chat 顶层 helper（chat submit timing/constants、diagnostic logging、focused-element probes、window-keyboard text/Enter injection、modifier release）也已移到 `ext\src\chat\WorkspaceChatTopLevelHelpers.cpp`
- `TerminalPage::_ConnectionStateChangedHandler()` 里的 workspace startup replay 也已抽成 `ext\src\workspace\WorkspaceLaunchRuntimeGlue.cpp` 中的 `_ReplayPendingWorkspaceStartupInput()`
- `TerminalPage::ProcessStartupActions()` 里剩余的 workspace startup queue 清理与 skip-next-SendInput 分支也已抽到 `ext\src\workspace\WorkspaceCreateGlue.cpp`
- `TerminalPage.cpp` 里整段 tab drag/drop glue（workspace lock 检查、drag-start / drag-over / drop / dropped-outside / send-to-other-window）也已移到 `ext\src\workspace\WorkspaceTabDragGlue.cpp`
- `TerminalPage.cpp` 里剩余几个零散 workspace 判断也继续 helper 化：split 时的 settings/workspace-manager tab 拦截、settings reload 后的 workspace surface refresh 已并入 `ext\src\workspace\WorkspaceTerminalGlue.cpp`，`CurrentWorkspaceId()` getter 已并入 `ext\src\workspace\WorkspaceLaunchRuntimeGlue.cpp`
- `TerminalPage::CloseWindow()` 开头的 workspace save-confirmation 分支也已抽到 `ext\src\workspace\WorkspaceSaverGlue.cpp` 中的 `_ConfirmWorkspaceCloseWindowIfNeeded()`
- `TerminalPage.h` 里的 workspace/chat 成员块和大段 workspace/chat 声明面也已改为 ext 头片段：`TerminalPageWorkspaceMembers.h`、`TerminalPageWorkspaceMethods.h`、`TerminalPageWorkspaceRuntimeMethods.h`
- `TerminalPage.h` 里剩余的 workspace 公共 API 声明、早期 workspace 成员、startup/saver workspace 成员、以及尾部 workspace 声明也继续拆成 ext 头片段：`TerminalPageWorkspacePublicMethods.h`、`TerminalPageWorkspaceEarlyMembers.h`、`TerminalPageWorkspaceStartupMembers.h`、`TerminalPageWorkspaceTailMethods.h`
- workspace runtime 的 startupAction / directory / OS / shell resolve 声明和 workspace node load/color 声明也已并入 `TerminalPageWorkspaceRuntimeMethods.h`；同时 `TerminalPage.cpp` 里 pane 创建时的条件 runtime-state 注册与 workspace middle-click hook 判断也都改为走 ext helper
- `TerminalPage.cpp` 里 `_updateAllTabCloseButtons()` 这一段也已并到 `ext\src\workspace\WorkspaceTerminalGlue.cpp`
- `TerminalPage` 顶部剩余的 workspace/chat include 面也已收成 `ext\src\workspace\TerminalPageWorkspaceIncludes.h`，`TerminalPage.cpp` 里重复的 workspace model/manager/chat include 已去掉
- `TerminalPage.cpp` 里的 workspace/chat cpp bridge stack 也继续聚合成 `ext\src\workspace\TerminalPageWorkspacePrelude.cpp` 与 `TerminalPageWorkspaceGlue.cpp`
- `TerminalPage.h` 里分散的 workspace/chat public alias、private member、private method 片段也继续聚合成 `TerminalPageWorkspacePublicSurface.h`、`TerminalPageWorkspacePrivateMembersSurface.h`、`TerminalPageWorkspacePrivateMethodsSurface.h`
- `TerminalPage.cpp` 顶部残留的 workspace/chat helper header include 也继续聚合成 `TerminalPageWorkspaceCppIncludes.h`
- `TerminalPage.h` 里最后零散留在原处的 `_RegisterTerminalEvents()`、`_RegisterTabEvents()`、`_updateAllTabCloseButtons()` 声明也已并回 `TerminalPageWorkspaceMethods.h`
- `TerminalPage.cpp` 里的 `Create()`、`ProcessStartupActions()`、`CloseWindow()` 整段 method body 也已移到 `ext\src\workspace\WorkspaceLifecycleGlue.cpp`
- `TerminalPage.cpp` 里的 `_SplitPane()`、`_RefreshUIForSettingsReload()`、`_ConnectionStateChangedHandler()` 整段 method body 也已继续并入 `ext\src\workspace\WorkspaceLifecycleGlue.cpp`
- `TerminalPage.cpp` 里的 `_MakeTerminalPane()` 与 `_MakePane()` 也已继续并入 `ext\src\workspace\WorkspaceLifecycleGlue.cpp`
- `TerminalPage.cpp` 里的 `CreateTabFromConnection()` 与 `_CompleteInitialization()` 也已继续并入 `ext\src\workspace\WorkspaceLifecycleGlue.cpp`
- `TerminalPage.cpp` 里的 dialog / new-tab-flyout 相邻方法块（`_ShowAboutDialog()`、`_ShowDialogHelper()`、confirm/paste dialogs、`_CreateNewTabFlyout*`、`_OpenNewTabDropdown()`、`_OpenNewTerminalViaDropdown()`）也已继续并入 `ext\src\workspace\TerminalPageNewTabFlyoutGlue.cpp`
- `TerminalPage.cpp` 里的 `RequestQuit()`、`PersistState()`、`_ShouldWarnOnClose()`、`_ShouldWarnOnCloseTab()`、`Panes()` 也已继续并入 `ext\src\workspace\WorkspaceLifecycleGlue.cpp`
- `TerminalPage.cpp` 里的 `_Scroll()`、`_MovePane()`、`_DetachPaneFromWindow()`、`_DetachTabFromWindow()`、`_MoveContent()`、`_MoveTab()`、`_activePaneChanged()`、`AttachContent()` 也已继续并入 `ext\src\workspace\WorkspaceLifecycleGlue.cpp`
- `TerminalPage.cpp` 里的 `_ToggleSplitOrientation()`、`_ResizePane()`、`_ScrollPage()`、`_ScrollToBufferEdge()` 也已继续并入 `ext\src\workspace\WorkspaceLifecycleGlue.cpp`
- `TerminalPage.cpp` 里的 URI / notice 相邻交互块（`_OpenHyperlinkHandler()`、`_ShowCouldNotOpenDialog()`、`_IsUriSupported()`、`_IsUriConsideredSomewhatSafe()`、`_ControlNoticeRaisedHandler()`、`_ShowControlNoticeDialog()`）也已继续并入 `ext\src\workspace\TerminalPageInteractionGlue.cpp`
- `TerminalPage.cpp` 里的 control/content/background/taskbar 相邻块（`_OnTabCloseRequested()`、`_CreateNewControlAndContent()`、`_AttachControlToContent()`、`_SetupControl()`、`_restartPaneConnection()`、`_SetBackgroundImage()`、`SetStartupConnection()`、`DialogPresenter()`、`TaskbarState()`）也已继续并入 `ext\src\workspace\TerminalPageControlGlue.cpp`
- `TerminalPage.cpp` 里的 titlebar/window-state 相邻块（`TitlebarClicked()`、`WindowVisibilityChanged()`、`_Find()`、`ToggleFocusMode()`、`SetFocusMode()`、`ToggleFullscreen()`、`ToggleAlwaysOnTop()`）也已继续并入 `ext\src\workspace\TerminalPageWindowStateGlue.cpp`
- `TerminalPage.cpp` 里的 fullscreen/maximize/settings-ui 相邻块（`_FocusActiveControl()`、`FocusMode()`、`Fullscreen()`、`AlwaysOnTop()`、`ShowTabsFullscreen()`、`SetShowTabsFullscreen()`、`SetFullscreen()`、`Maximized()`、`RequestSetMaximized()`、`_makeSettingsContent()`、`OpenSettingsUI()`）也已继续并入 `ext\src\workspace\TerminalPageSettingsUiGlue.cpp`
- `TerminalPage.cpp` 里的 helper / teaching-tip 相邻块（`_GetTabImpl()`、`_ComputeScrollDelta()`、`_ReadSystemRowsToScroll()`、`ShowKeyboardServiceWarning()`、`KeyboardServiceDisabledText()`、`_UpdateTeachingTipTheme()`、`IdentifyWindow()`、`ShowTerminalWorkingDirectory()`）也已继续并入 `ext\src\workspace\TerminalPageHelpersGlue.cpp` 与 `ext\src\workspace\TerminalPageTeachingTipGlue.cpp`
- `TerminalPage.cpp` 里的 rename / elevate / infobar-dismiss 相邻块（`_WindowRenamerActionClick()`、`_RequestWindowRename()`、`_WindowRenamerKeyDown()`、`_WindowRenamerKeyUp()`、`GetClosestProfileForDuplicationOfProfile()`、`_OpenElevatedWT()`、`_maybeElevate()`、`_CloseOnExitInfoDismissHandler()`、`_KeyboardServiceWarningInfoDismissHandler()`、`_IsMessageDismissed()`、`_DismissMessage()`）也已继续并入 `ext\src\workspace\TerminalPageWindowActionsGlue.cpp`
- `TerminalPage.cpp` 里的 theme/runtime 相邻块（`_updateThemeColors()`、`_updatePaneResources()`、`_adjustProcessPriority()`、`WindowActivated()`）也已继续并入 `ext\src\workspace\TerminalPageThemeRuntimeGlue.cpp`
- `TerminalPage.cpp` 里的 completions / context-menu / quick-fix 相邻块（`_ControlCompletionsChangedHandler()`、`_OpenSuggestions()`、`_PopulateContextMenu()`、`_PopulateQuickFixMenu()`、`_windowPropertyChanged()`、`_CreateRunAsAdminFlyout()`）也已继续并入 `ext\src\workspace\TerminalPageSuggestionsGlue.cpp`

当前仍未收口的部分主要是：

- `TerminalPage.cpp/.h` 里仍保留较多 workspace chat 状态、事件编排和少量零散接线 glue
- `TerminalPage.cpp/.h` 里仍保留 workspace chat UI 初始化、若干成员状态，以及少量零散 workspace glue
- `TerminalPage.cpp/.h` 目前已形成多个 ext include / ext header 桥接层，但仍残留部分零散 terminal/chat 路由粘合代码与状态容器
- `AppLogic.cpp` 仍未专门做新一轮收缩

### B. Workspace 持久化与模型

**目标：** 规则尽量外置，原始模型层只保留最小桥接

建议：

1. 把 workspace 的目录规则、状态文件规则、命名规则、聊天/草稿/事件文件组织规则继续收敛到 `ext\src`
2. `Workspace.cpp/.h` 只保留必要的数据模型接入
3. 若仍需推理/迁移/序列化增强，优先做成 `ext` helper，再由原始模型调用

这里需要注意：  
如果某些逻辑深度绑定 `TerminalSettingsModel` 内部类型，不一定能完全搬走；这种情况的目标不是“零修改”，而是**把复杂规则搬走，只留下类型适配**。

#### 当前判断：`Workspace.cpp` 适合“整体外移实现”

基于当前调用面，`workspace` 本身就是本分支新增概念，外部真正依赖的主要是：

- `WorkspaceManager`
- `WorkspaceStateManager`
- `SanitizeWorkspaceDirectoryName`
- `ResolveWorkspaceNodeTabColor`
- `EnsureWorkspaceNodeTabColors`

也就是说，外部要的其实是**一组 API 和数据结构**，不是必须把实现继续放在 `microsoft\src\cascadia\TerminalSettingsModel\Workspace.cpp`。

因此这块后续可以按下面方式收敛：

1. `ext\src\workspace` 持有主要实现（加载、保存、YAML 平铺、目录遍历、状态文件处理、startup action 推导等）
2. 原始树里的 `Workspace.h/.cpp` 只保留：
   - 兼容现有 include 路径的声明入口
   - 必要的 project 编译接线
   - 极薄的 forward / wrapper

换句话说，这块不是只能“局部抽 helper”，而是可以继续演进到：

> **`Workspace.cpp` 在原始树里只剩壳，真实实现整体搬到 `ext\src\workspace`。**

当前唯一需要保留注意的点是：

- 现有调用方很多都直接 include `TerminalSettingsModel\Workspace.h`
- 测试、`TerminalPage.cpp`、`AppLogic.cpp`、`TerminalWindow.cpp` 都依赖这套类型名

所以更稳的做法不是直接删 `Workspace.h/.cpp`，而是：

1. **保留原路径文件作为兼容门面**
2. **把实现主体整体搬到 `ext`**
3. **外部调用方暂时不改 include 路径**

当前已完成第一步落地：

- `microsoft\src\cascadia\TerminalSettingsModel\Workspace.cpp` 现在只保留 `pch.h` 和对 `ext\src\workspace\WorkspaceModel.cpp` 的兼容包含
- workspace 主实现体已迁到 `ext\src\workspace`
- 现有工程项与 include 路径不需要同步改动，先维持兼容
- 其中 workspace 元数据/状态文件的 YAML 解析、序列化、目录枚举逻辑也已继续拆到 `ext\src\workspace\WorkspacePersistenceSerialization.cpp`
- workspace 节点配色、SSH/启动命令拼装等规则逻辑也已继续拆到 `ext\src\workspace\WorkspaceStartupColorLogic.cpp`
- `Workspace.h` 也已降为兼容入口，真实 API 声明面改由 `ext\src\workspace\WorkspaceApi.h` 持有
- `WorkspaceModel.cpp` 也已继续压缩为组合层，剩余实现按 utility / persistence serialization / persistence managers / startup+color / facade methods 分拆到独立 ext 文件

### C. Portable 构建与打包

**目标：** portable 的实现层放 `ext\src\portable`，构建入口只保留仓库根 CMake

建议归位：

| 类型 | 目标位置 |
| --- | --- |
| 新的打包实现 | `ext\src\portable\PortablePackageTool` |
| CMake 辅助脚本 | `ext\src\portable\cmake` |
| 后续 portable 专属逻辑 | `ext\src\portable\...` |

原始树/根目录保留内容：

- 顶层 `CMakeLists.txt` 作为唯一构建入口
- pipeline 迁移后删除 `New-UnpackagedTerminalDistribution.ps1`
- `CascadiaPackage.wapproj` 已在 MakePri/MakeAppx 切换时删除

换句话说，portable 这块的理想状态是：

- **入口在原处**
- **实现尽量在 `ext`**

### D. 测试

测试工程仍会留在原始测试项目中，因为它们要接现有测试基础设施。  
但测试覆盖的定制行为，应尽量通过 `ext` 暴露的能力来验证，而不是把实现继续塞回原始树。

## 分阶段执行建议

### Phase 1：先做差异盘点与归属表

输出一张清单，按下面三类标记当前改动：

1. **必须留在原始树**
2. **可抽到 `ext\src`**
3. **可删除/回退/重写为薄接线**

优先处理当前工作区已改动的这些文件，不先追历史长尾。

### Phase 2：先收敛 workspace chat

原因：

- 这块已在 `ext\src\chat` 有基础
- 当前最明显地横跨 UI、存储、诊断、路由
- 也是最容易继续把逻辑堆回 `TerminalPage.cpp` 的地方

目标结果：

- `TerminalPage.cpp/.h` 只剩事件接入、状态透传、少量 UI glue
- 实际提交策略、日志、存储规则、CLI 判断都在 `ext\src\chat`

### Phase 3：再收敛 workspace 持久化规则

重点看：

- 路径/目录布局
- state/workspace/tab 文件组织
- 名称清洗
- 推理与兼容迁移

目标结果：

- `Workspace.cpp/.h` 留最小模型桥接
- 复杂规则尽量进 `ext`

### Phase 4：最后收敛 portable 构建实现

重点：

- 保持现有 script 入口还能用
- 保持新的 CMake portable 入口还能用
- 但新增实现继续集中在 `ext\src\portable`

目标结果：

- 旧脚本/工程不保留兼容壳
- 真实 portable 定制逻辑在 `ext`

### Phase 5：建立后续改动约束

后续新需求默认按下面流程执行：

1. 先判断能否直接做在 `ext\src`
2. 如果不能，再找最小 hook 点
3. 只有 hook 不存在时，才改原始树
4. 原始树改动必须说明“为什么不能只改 `ext`”

## 本轮落地标准

本轮后续实施时，建议以这几个结果作为完成标准：

1. 当前活跃定制逻辑有明确 owner，优先落在 `ext\src`
2. `microsoft\src\...` 中的大块定制逻辑明显减少
3. 原始树剩余改动大多是入口、hook、项目接线、资源声明
4. portable 与 workspace chat 两条线都形成“原始树入口 + ext 实现”的结构
5. 后续需求默认不再直接往 fork 原始文件里堆实现

## Phase 1 差异归属表（当前活跃工作区）

下面这张表只针对**当前工作区还在演进的改动**，不追溯整个分支历史长尾。  
目的不是现在就立刻搬完，而是先明确每个改动点后续应该往哪边收。

### A. 已经在正确位置的 `ext` 改动

这些文件本身已经在目标区域，后续应继续把实现收敛在这里：

| 文件 | 归类 | 说明 |
| --- | --- | --- |
| `ext\src\chat\WorkspaceChatController.*` | 已在目标位置 | workspace chat 提交/路由逻辑 owner |
| `ext\src\chat\WorkspaceChatStore.*` | 已在目标位置 | 聊天持久化 owner |
| `ext\src\chat\WorkspaceStoragePaths.*` | 已在目标位置 | workspace 聊天/草稿/日志路径规则 owner |
| `ext\src\chat\WorkspaceDiagnosticLog.*` | 已在目标位置 | 诊断日志 owner |
| `ext\src\chat\TerminalEventStore.cpp` | 已在目标位置 | 终端事件归档 owner |
| `ext\src\chat\CopilotTerminalSimulator.js` | 已在目标位置 | 调试/复现辅助工具 |
| `ext\src\portable\PortablePackageTool\...` | 已在目标位置 | portable 打包实现层 |
| `ext\src\portable\cmake\...` | 已在目标位置 | portable CMake 辅助实现层 |

### B. 必须留在原始树的文件

这些文件即使后续继续优化，也只能留在原始树，因为它们本质上是工程接线、资源、测试基建或低层补丁位：

| 文件 | 归类 | 原因 | 后续约束 |
| --- | --- | --- | --- |
| `microsoft\src\cascadia\TerminalApp\Resources\en-US\Resources.resw` | 必留原始树 | WinUI 资源文件必须由原项目直接持有 | 只保留资源声明，不放业务逻辑语义扩散 |
| `microsoft\src\cascadia\TerminalApp\Resources\zh-CN\Resources.resw` | 必留原始树 | 同上 | 同上 |
| `microsoft\src\cascadia\TerminalApp\TerminalAppLib.vcxproj` | 必留原始树 | 现有目标的源码编译入口 | 只负责把 `ext\src` 文件编进目标 |
| `microsoft\src\cascadia\TerminalApp\TerminalAppLib.vcxproj.filters` | 必留原始树 | VS 工程过滤器元数据 | 只跟随工程接线调整 |
| `microsoft\src\cascadia\UnitTests_SettingsModel\SettingsModel.UnitTests.vcxproj` | 必留原始树 | 现有测试工程入口 | 只增加/引用测试代码，不承载实现 |
| `microsoft\src\cascadia\LocalTests_TerminalApp\TabTests.cpp` | 必留原始树 | 测试必须接原有测试基础设施 | 测试 `ext` 行为，不把实现塞回测试配套代码 |
| `microsoft\src\cascadia\UnitTests_SettingsModel\WorkspaceTests.cpp` | 必留原始树 | 同上 | 同上 |
| `microsoft\src\inc\til\mutex.h` | 必留原始树 | 低层公共头补丁位，不适合用 `ext` 旁路替代 | 仅在确认无法通过上层结构回避时保留 |
| `microsoft\src\cascadia\CascadiaPackage\CascadiaPackage.wapproj` | 已删除 | manifest/PRI/MSIX 已由根 CMake 唯一路线生成 | 不恢复 WAP 入口 |
| `microsoft\build\scripts\New-UnpackagedTerminalDistribution.ps1` | 待删除 | 当前 pipeline 仍直接调用 | pipeline 切到 PortablePackageTool 后立即删除 |
| `CMakeLists.txt` | 必留原始树 | 顶层构建入口必须在仓库根 | 只做入口编排，portable 实现继续放 `ext\src\portable` |

### C. 应收缩为“薄接线”的原始树文件

这些文件还会留在原始树，但目标是把里面的**定制实现**尽量搬到 `ext\src`，原始树只剩调用/桥接：

| 文件 | 当前问题 | 目标归类 | 具体收缩方向 |
| --- | --- | --- | --- |
| `microsoft\src\cascadia\TerminalApp\TerminalPage.cpp` | 容易继续堆 workspace chat 行为细节 | 薄接线 | 保留 UI 事件接入、状态透传；提交策略/诊断/CLI 判断下沉 `ext\src\chat` |
| `microsoft\src\cascadia\TerminalApp\TerminalPage.h` | 容易继续扩散定制状态与 helper | 薄接线 | 仅保留必要成员、接口声明、ext controller 持有位 |
| `microsoft\src\cascadia\TerminalApp\AppLogic.cpp` | 应用级 workspace/chat 逻辑容易越积越多 | 薄接线 | 保留初始化/调用点，实际逻辑放 `ext` |
| `microsoft\src\cascadia\TerminalSettingsModel\Workspace.cpp` | 目录规则、命名规则、状态落盘规则仍混在模型层 | 薄接线 | 复杂规则提到 `ext` helper，模型层只做类型适配 |
| `microsoft\src\cascadia\TerminalSettingsModel\Workspace.h` | 容易承载过多定制接口 | 薄接线 | 仅保留模型声明与最小桥接 |

### D. 当前建议优先迁出的实现主题

按“先收逻辑、后留桥”的顺序，下一批应优先从原始树抽走的是：

1. `TerminalPage.*` 里的 workspace chat 提交判定、日志事件拼装、diagnostics 细节
2. `Workspace.cpp/.h` 里的路径规则、命名清洗、state/workspace/tab 文件布局规则
3. portable 构建里除入口编排外的专属实现

## 后续改动约束（执行规则）

为了避免后面又把实现塞回 fork 原始树，后续提交默认遵守下面规则：

1. **先找 `ext\src` 落点，再写代码。**
2. 如果改了 `microsoft\...`，提交前必须能回答：  
   **“这里为什么不能只改 `ext`？”**
3. 对 `microsoft\...` 的新增改动，优先只允许三类：
   - 新增 `#include` / 调用 `ext` 入口
   - 暴露 hook / callback / interface
   - 工程接线 / 资源声明 / 测试挂接
4. 若某个原始树文件里新增了成块业务逻辑，应视为收敛失败，下一轮优先外移。

## 建议的下一步

按优先级直接开始做 **Phase 1 差异归属表**，先把当前这些活跃改动文件逐个标成：

- 必留原始树
- 可迁到 `ext\src`
- 可收缩为薄接线

这一步完成后，再按 `workspace chat -> workspace persistence -> portable` 的顺序逐组迁移，会比一次性大搬家更稳。
