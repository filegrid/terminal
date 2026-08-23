# Workspace 设计文档（结合现有 Terminal 架构）

对应需求文档见：`ext\docs\workspace-requirements.md`

## 设计输入

在现有终端逻辑和现有 `settings.json` 体系不拆分的前提下，新增 **workspace** 能力：

- 一个 workspace 包含多个连接实例。
- workspace 定义存储在用户目录下的 `.wt\workspaces.yaml`。
- 这里的 settings，明确是指 Windows Terminal 当前使用的 `settings.json`。
- 讨论里提到的 `peer / server / resource` 只是帮助理解的概念占位，实际实现仍然要对齐当前终端 `settings.json` 的真实结构、命名和解析逻辑。
- workspace 内的每个实例支持类似 SecureCRT session 的会话级配置，至少包含：
  - 启动目录
  - 启动命令/脚本
- workspace node 还需要有一个字段对齐 `settings.json` 里 `profiles.list` 中某个 profile/command 的 `guid`，以便复用该项已有的颜色、icon 等风格信息。
- workspace 节点默认是浏览态，**只有进入编辑模式后**才能增删节点；平时不显示 `+` 和 `x`。
- 第一个启动的终端窗口默认恢复“最近打开的 workspace”。
- 后续新开窗口默认打开“空 workspace”（即当前模式），用户可以再将其保存为 workspace。

## 与现有系统结合的设计目标

这次不是给 Terminal 再造一套新的配置系统，而是在现有架构上插入一个新的“工作区编排层”。

要复用的现有系统主要有三块：

1. `settings.json` / `CascadiaSettings`：现有用户可编辑配置与 profile 定义
2. `TerminalSettings::CreateWithNewTerminalArgs(...)`：现有 profile 解析与终端样式生成链路
3. `TerminalPage::ProcessStartupActions(...)`：现有窗口/tab/pane 启动动作执行链路
workspace 的设计目标是：

- **定义层**放在 `.wt\workspaces.yaml`
- **样式层**继续复用 `settings.json` 的 `profiles.list`
- **启动层**继续复用 `NewTerminalArgs + ActionAndArgs`
- **workspace 运行态**单独放在 `.wt\workspace-state.yaml`

## 总体原则

### 1. 继续使用同一个 settings.json

workspace 不是替换当前 `settings.json`，也不是再做一套新的连接体系。

现有 `settings.json` 中承载连接、服务端、资源等职责的配置，继续作为**基础配置源**存在，并继续走当前终端的连接、参数拼装、资源解析、启动流程。

workspace 只负责两件事：

1. 组织多个连接实例
2. 在实例层做少量会话级覆盖

这样可以保证：

- 当前终端打开单个连接的逻辑不变
- 现有 `settings.json` 中的相关配置可直接复用
- workspace 只是“编排层”，不是“第二套 settings.json”

### 2. workspace 配置与 settings.json / workspace-state.yaml 分层

- **全局配置**：继续放在当前 `settings.json` 中，管理现有连接相关配置和 profile 风格配置。
- **workspace 定义**：放在 `%USERPROFILE%\.wt\workspaces.yaml`。
- **workspace 运行态状态**：放在 `%USERPROFILE%\.wt\workspace-state.yaml`。

两者关系：

- `workspaces.yaml` 引用 `settings.json` 中已经存在的连接定义
- `workspaces.yaml` 也引用 `settings.json` 中 `profiles.list` 的 `guid`
- workspace 实例只保存“编排信息 + 会话级覆盖项”
- 最近打开的 workspace、窗口与 workspace 绑定关系、“新窗口打开 workspace” 选项等运行态状态不写回 `workspaces.yaml`，而是走独立的 `workspace-state.yaml`

## 数据模型

## 文件位置

- 全局配置：保持现有 `settings.json`
- workspace 文件：`%USERPROFILE%\.wt\workspaces.yaml`
- 运行态状态文件：`%USERPROFILE%\.wt\workspace-state.yaml`

其中：

- `workspaces.yaml` 只保存 workspace 定义
- `workspace-state.yaml` 保存最近打开的 workspace、窗口/workspace 关联、待消费的新窗口 workspace 打开请求，以及“新窗口打开 workspace”这类 workspace 用户配置

## 现有系统结合点

### 1. 设置模型层：不塞进 CascadiaSettings JSON 解析器

`settings.json` 当前走的是 `CascadiaSettings` / `TerminalSettingsModel` 的 JSON 解析链路。

workspace 定义文件是：

- 单独文件
- 单独格式（当前方案是 YAML）
- 单独职责（编排层，不是 profile 基础设置）

因此不建议把 `workspaces.yaml` 强行塞进 `CascadiaSettings` 本体里一起解析。

更合适的做法是：

- 在 `TerminalSettingsModel` 旁边新增一组 workspace 模型与加载器
- 例如 `WorkspaceSet` / `WorkspaceNode` / `WorkspaceManager`
- 它们和 `CascadiaSettings` 并列，而不是嵌进 `CascadiaSettings` 的 JSON schema

这样分层更清楚：

- `CascadiaSettings`：继续负责 `settings.json`
- `WorkspaceManager`：负责 `%USERPROFILE%\.wt\workspaces.yaml`
- `WorkspaceStateManager`：负责 `%USERPROFILE%\.wt\workspace-state.yaml`

### 2. 状态层：在 `.wt` 下单独维护 workspace 运行态

- `workspaces.yaml` 继续只保存 workspace 定义。
- 在 `%USERPROFILE%\.wt` 下新增独立的 `workspace-state.yaml` 保存 workspace 运行态和用户配置。
- `state.json` 继续只承担 Windows Terminal 原本的通用运行态，不混入 workspace 特有状态。

`workspace-state.yaml` 至少保存：

- `lastOpenedWorkspaceId`
- `openInNewWindow`
- 当前打开的 terminal 窗口与其 workspace 关联
- 新窗口打开 workspace 时的待消费运行态队列

### 3. 监听层：给 workspaces.yaml 增加独立 watcher

现有 `AppLogic::_RegisterSettingsChange()` 只监听 `settings.json` 所在目录。

workspace 接入后，需要再补一条独立监听链路：

- 监听 `%USERPROFILE%\.wt\workspaces.yaml`
- 当文件改动时，只 reload workspace 定义
- 不触发整份 `settings.json` 的 reload

也就是说运行时会有两类 reload：

1. `settings.json` reload：影响 profile、theme、actions、global settings
2. `workspaces.yaml` reload：影响 workspace 列表、节点结构、最近选择可见性校验

这两个 reload 源要分开，否则 workspace 改动会被错误地纳入 settings 保存/刷新路径。

### 4. 启动层：复用 NewTerminalArgs 和 startup actions

现有终端打开 tab / pane 的主链路已经很明确：

1. 用 `NewTerminalArgs` 描述一个待启动终端实例
2. 调用 `TerminalSettings::CreateWithNewTerminalArgs(_settings, args)` 解析 profile 和终端样式
3. 在 `TerminalPage::_MakeTerminalPane(...)` 中创建连接与控件
4. 用 `TerminalPage::ProcessStartupActions(...)` 执行动作序列

workspace 不应该绕开这条链路，而应该把每个 node **编译成现有启动对象**。

具体映射建议如下：

- `node.profileGuid` -> `NewTerminalArgs.Profile(...)`
- `node.startupDirectory` -> `NewTerminalArgs.StartingDirectory(...)`
- `node.startupAction` -> 一个后续执行的 `ActionAndArgs`，而不是直接覆盖 profile 的 `commandline`

这里最关键的一点是：

**`startupAction` 不建议直接映射到 `NewTerminalArgs.Commandline`。**

因为 `NewTerminalArgs.Commandline` 在现有系统里表示“覆盖 profile 的启动命令”，会改变该 profile 的基础身份；而 workspace 里的启动动作更像是“连接建好后要额外执行的一段命令/脚本”。

因此更合适的方式是：

1. 先按 `profileGuid` 创建 tab
2. 然后把 `startupAction` 作为后续 startup action 注入
3. 需要时用现有 `SendInput` 风格动作把命令/脚本文本送入终端

这样才能同时满足：

- profile 样式保持和 `settings.json` 一致
- profile 的基础 commandline 不被 workspace 覆盖
- workspace 仍然可以做“切目录 + 执行启动命令/脚本”

### 5. 首窗口恢复逻辑：复用现有 first-window / startup 管线

现有 `WindowEmperor` 和 `TerminalWindow` 已经有“首窗口如何初始化内容”的现成逻辑：

- persisted layout 走 `ApplicationState::PersistedWindowLayouts`
- command line 走 `AppCommandlineArgs`
- settings startup actions 走 `GlobalSettings().StartupActions()`

workspace 第一版不应该自己再造一套窗口初始化通道。

更合适的接法是：

#### 首个窗口

当满足下面条件时：

- 没有 persisted layout 需要恢复
- 没有显式命令行参数
- 这是当前应用会话的第一个终端窗口

则：

1. 从 `workspace-state.yaml` 取 `lastOpenedWorkspaceId`
2. 从 `WorkspaceManager` 取对应 workspace
3. 把该 workspace 编译成一组 `ActionAndArgs`
4. 填给 `TerminalWindow` 现有的 startup actions 入口

#### 后续窗口

后续新窗口不要自动套用最近 workspace，直接走当前默认空白启动流程。

这正好符合你要的：

- 第一个窗口恢复最近 workspace
- 其他窗口打开空 workspace

而且不会破坏现有命令行显式启动、persisted layout 恢复等既有优先级。

### 6. UI 层：不要直接塞进现有 Settings Save 管线

现有 `TerminalSettingsEditor::MainPage` 的核心模型是：

- 加载 `CascadiaSettings`
- clone 一份 `_settingsClone`
- 用户在 UI 中修改 clone
- 点 Save 时写回 `settings.json`

这个管线天然是为 `settings.json` 设计的。

workspace 如果直接作为一个新的 settings 导航页塞进去，会立刻遇到两个问题：

1. workspace 持久化目标其实是 `workspaces.yaml`，不是 `settings.json`
2. workspace 的编辑模式、浏览模式、节点增删交互，和现有 profile/settings 页的交互模型不一样

所以第一版更合理的做法是：

- workspace 编辑器作为**独立页面/独立面板**
- 可以从主页或侧栏进入
- 有自己的编辑态与保存逻辑
- 保存时只写 `workspaces.yaml`

而不是挂到当前 Settings UI 的统一 Save 按钮下面。

这样改动最小，也最不容易把现有 Settings UI 搅乱。

## workspaces.yaml 建议结构

```yaml
version: 1

workspaces:
  - id: ws-dev
    name: Dev Workspace
    description: daily development
    nodes:
      - id: peer-app-1
        name: App 1
        connectionRef: peer-app
        profileGuid: "{00000000-0000-0000-0000-000000000101}"
        startupDirectory: D:\work\app
        startupAction: .\scripts\bootstrap.ps1
        env:
          PROJECT_ENV: dev

      - id: peer-db-1
        name: DB
        connectionRef: peer-db
        profileGuid: "{00000000-0000-0000-0000-000000000102}"
        startupDirectory: D:\work\db
        startupAction: ""

  - id: ws-ops
    name: Ops Workspace
    nodes:
      - id: peer-prod-1
        name: Prod
        connectionRef: peer-prod
        profileGuid: "{00000000-0000-0000-0000-000000000103}"
```

这里的 `connectionRef` 只是方案里的中性名字，用来表达“引用现有 `settings.json` 中的连接定义”。最终字段名和引用对象应以当前终端 `settings.json` 的真实模型为准，不要求字面使用 `peer`。

`profileGuid` 用来对齐 `settings.json` 的 `profiles.list[*].guid`。workspace node 启动后，优先复用该 profile 已有的视觉属性，例如：

- 颜色
- icon
- tab/title 风格相关展示项

## 字段说明

### workspace

- `id`: 稳定标识
- `name`: 展示名称
- `description`: 可选描述
- `nodes`: 节点列表

### node

- `id`: workspace 内节点唯一标识
- `name`: 节点显示名
- `connectionRef`: 引用现有 `settings.json` 中的连接定义
- `profileGuid`: 对应 `settings.json` 中 `profiles.list[*].guid`，用于复用该 profile 的风格
- `startupDirectory`: 节点启动目录
- `startupAction`: 节点启动命令/脚本
- `env`: 可选环境变量

## 节点实例与现有连接定义的关系

workspace 中的 node 是**现有连接定义的实例化引用**，不是新的底层配置类型。

推荐关系如下：

- **现有连接定义**：描述连接目标、协议、认证方式，以及它关联的其他配置
- **profile**：描述该连接实例启动后的默认视觉风格
- **workspace node**：描述“在某个 workspace 中如何启动这条连接，以及套用哪一个 profile 风格”

因此同一个连接定义可以在多个 workspace 中被重复引用，也可以在同一个 workspace 中出现多个实例，只是它们的：

- 启动目录
- 启动命令/脚本
- 环境变量
- 显示名称
- 绑定的 profile 风格

可以不同。

这和 SecureCRT 的 session 使用习惯基本一致：底层连接能力保持统一，上层允许面向场景做实例化配置。

## 启动与执行逻辑

## 节点启动顺序

单个 node 启动时，按下面顺序组装：

1. 从当前 `settings.json` 解析 `connectionRef`
2. 从当前 `settings.json` 解析 `profileGuid`
3. 构造 `NewTerminalArgs`
   - `Profile = profileGuid`
   - `StartingDirectory = startupDirectory`
4. 调用现有 `TerminalSettings::CreateWithNewTerminalArgs(...)`
5. 按现有 `TerminalPage::_MakeTerminalPane(...)` 链路创建 tab / pane
6. 在 tab 建立后追加 `startupAction` 对应的 startup action
7. 通过现有 `TerminalPage::ProcessStartupActions(...)` 执行后续动作

## 启动目录 / 启动动作规则

建议规则：

- `startupDirectory`：如果配置了，则覆盖默认工作目录
- `startupAction`：作为连接成功后的首个启动动作执行；它可以是命令，也可以是脚本路径，UI 上按一个概念展示，底层再按内容判断执行方式

第一版建议只保留一个字段，避免把“启动命令”和“启动脚本”拆成两个配置后又要处理优先级冲突。

## UI 方案

## 设置页 / 工作区页的关系

第一版 **不把 workspace 塞进现有 settings 分类树里做成新的底层配置源，也不强行复用当前 Settings UI 的统一 Save 管线**。

workspace 更适合做成：

- 独立的 workspace 侧栏 / workspace 面板
- 或者主页上的 workspace 管理入口

但其底层引用的现有连接相关配置和 profile 风格，仍来自现有 `settings.json` / 现有 settings 页面。

这样拆开后职责会很清楚：

- Settings UI：编辑 `settings.json`
- Workspace UI：编辑 `workspaces.yaml`
- WorkspaceStateManager：记录最近打开状态和窗口/workspace 运行态

## workspace 视图

workspace 视图显示：

- workspace 列表
- 当前 workspace 的节点树 / 节点列表

默认态是**浏览态**：

- 不显示 `+`
- 不显示 `x`
- 不显示拖拽把手

只有用户点击“编辑 workspace”后进入**编辑态**，才显示：

- 新增节点
- 删除节点
- 重命名节点
- 调整顺序

这样可以避免平时误操作，也符合你要求的“平时没有 x 和加号”。

## 空 workspace

空 workspace 是一个**临时工作区**，用于承接当前模式：

- 用户新开一个普通终端窗口时，默认进入空 workspace
- 空 workspace 不强制立即落盘
- 当用户执行“保存为 workspace”时，才写入 `workspaces.yaml`

这可以兼容现在的打开逻辑，不强迫所有窗口都绑定到命名 workspace。

## 启动行为

## 首个窗口

应用启动后的**第一个终端窗口**：

- 读取 `workspace-state.yaml`
- 如果存在最近打开的 workspace，则把它编译成 startup actions 后自动打开
- 如果不存在，则打开空 workspace

## 后续窗口

同一应用会话中的**后续新窗口**：

- 默认打开空 workspace
- 不自动复用最近 workspace

这样可以满足：

- 首屏快速恢复上次工作现场
- 新窗口仍保留现在“开一个干净终端”的习惯

## 保存与状态管理

## 保存时机

- 编辑 workspace 后，显式保存
- “保存为 workspace”时创建新定义
- 节点增删改顺序只在编辑模式下允许提交

不建议默认每次点一下就自动保存，避免 workspace 被误改。

## 最近打开记录

建议记录：

- `lastOpenedWorkspaceId`
- `lastOpenedAt`

必要时可再加：

- `lastSelectedNodeId`
- `lastWindowMode`

## 兼容性与迁移

- 没有 `workspaces.yaml` 时，不影响当前功能，直接走现有模式
- 现有 `settings.json` 中的连接相关配置和 profile 配置无需迁移
- workspace 是增量能力，可逐步接入

## 第一阶段建议的代码落点

如果后续开始实现，建议按现有仓库职责分布拆成下面几层：

### TerminalSettingsModel

- 新增 workspace 模型对象
- 新增 workspace 定义加载/保存器
- 新增 workspace 运行态加载/保存器

原因：

- 这里本来就负责 settings 和周边模型/序列化
- `WorkspaceManager` / `WorkspaceStateManager` 都适合留在这一层
- 不会把 `TerminalApp` 变成“既管 UI 又管文件格式”的大杂烩

### TerminalApp

- 新增“打开 workspace”动作
- 新增“把 workspace 编译成 startup actions”的适配层
- 在窗口首次启动时决定是否恢复最近 workspace

原因：

- `TerminalPage` / `TerminalWindow` 已经拥有 startup actions 执行权
- `NewTerminalArgs`、`ActionAndArgs`、tab/pane 创建都在这一层

### TerminalSettingsEditor 或独立 Workspace 编辑页

- 新增 workspace 浏览/编辑 UI
- 维护浏览态/编辑态切换
- 保存时只写 `workspaces.yaml`

原因：

- 这里负责 XAML 编辑器体验
- 但不应该再借用现有 Settings Save 管线去写 `settings.json`

## 第一阶段落地范围

第一阶段建议只做这些能力：

1. 在模型层增加 `workspaces.yaml` 读写能力
2. 在 `.wt\workspace-state.yaml` 增加 workspace 运行态字段
3. workspace 引用现有连接定义
4. 节点支持 `profileGuid`，并复用 `settings.json` 中对应 profile 的颜色、icon 等风格
5. 节点支持 `startupDirectory`、`startupAction`
6. workspace node 编译成现有 `NewTerminalArgs + ActionAndArgs`
7. workspace 浏览态 / 编辑态切换
8. 首窗口恢复最近 workspace
9. 后续窗口默认空 workspace
10. 空 workspace 支持“保存为 workspace”

## 后续可扩展项

这些不放进第一阶段，但结构上要预留：

- 节点分组 / 文件夹
- 批量启动 workspace
- workspace 级环境变量
- workspace 级默认目录
- 最近连接历史
- 节点图标、颜色、标签
- 导入 SecureCRT session

## 结论

这个设计的核心不是“新增一个 workspace 文件”本身，而是：

- **连接能力和风格能力继续归现有 `settings.json` 管**
- **workspace 定义落在 `%USERPROFILE%\.wt\workspaces.yaml`**
- **最近工作区状态、窗口/workspace 绑定和 workspace 用户态配置归 `%USERPROFILE%\.wt\workspace-state.yaml` 管**
- **workspace node 启动时，复用现有 `NewTerminalArgs + startup actions` 管线**
- **UI 默认浏览态，编辑态才允许增删**
- **首窗口恢复最近 workspace，后续窗口打开空 workspace**

这样改动边界清晰，对现有终端逻辑冲击最小，也满足你现在提的使用方式。
