# 节点多窗口设计

## 1. 目标与范围

一个工作区节点可配置并启动 **1～3 个终端命令**。同一节点的已打开终端以一个“多窗口会话”呈现，用户可选择：

- **左右分隔模式**：所有窗口同时可见；支持 `n - 1` 条可拖动分隔线。
- **Tab 模式**：同一时刻显示一个窗口；支持左上文字 Tab、右上图标 Tab、右下图标 Tab 三种子模式。

本期仅支持左右分隔；不包含上下分隔、超过三个命令、跨节点合并、命令编辑器重构以外的工作区管理能力。

## 2. 关键体验与规则

### 2.1 命令配置

- 节点的启动命令为有序列表，数量为 1～3。
- 每项配置：`icon`、`name`、`command`；三个字段都可为空。Host Demo 对空值只展示占位状态，不做命令校验或启动行为。
- 配置区使用与工作区管理节点一致的多拖拽交互：整项拖拽排序、明确的插入位置、键盘/触屏可访问的替代排序操作。
- 删除前若会使列表为 0 项，禁止删除并说明至少保留一个命令。
- 排序定义启动顺序、分隔模式中的从左到右顺序，以及 Tab 模式中的从前到后顺序。

### 2.2 展示模式

模式偏好属于“已打开的多窗口会话”，并在当前节点再次打开时恢复；默认值为左右分隔。模式切换不重启进程，也不改变窗口顺序。

| 模式 | 布局 | 交互 |
| --- | --- | --- |
| Split | 从左到右的窗口列 | 拖动分隔线立即调整占比并持久化 |
| Tab / 左上 | 内容区左上方图标加文字的二级 Tab | 图标使用命令 icon；文字使用窗口当前标题 |
| Tab / 右上 | 内容区右上方竖排图标条 | 图标默认半透明；悬停不透明；选中态明显 |
| Tab / 右下 | 内容区右下方竖排图标条 | 行为同右上 |

真实接线后，当 TUI Agent（例如 Codex、Claude Code）更新终端标题时，左上图标加文字 Tab 立即同步文字标题；右上/右下仍以命令配置的 icon 为主，辅助文本使用配置名或最新标题。Host Demo 用固定标题模拟这一视觉效果。

### 2.3 Split 尺寸

- 有 `n` 个窗口时保存 `n` 个正数权重，权重和恒为 1；管理页显示为总和 100% 的可拖动分配线。
- 分隔条数固定为 `n - 1`（三窗口必为两条）；拖动第 `i` 条时，仅改变相邻两个窗口的权重，其它窗口不动，并在拖动期间显示这两个区域的百分比。
- 运行态直接拖动分隔线时，按节流策略保存；拖动结束强制落盘。管理页拖动结束同样保存。
- 需要最小可见宽度；若容器过窄，按最小宽度约束，无法满足时允许横向滚动而不是生成负值或不可操作的面板。
- 命令增删或重排后，按当前顺序迁移既有权重并重新归一化；新增窗口先取默认权重，删除窗口的权重按比例分配给剩余窗口。

## 3. 信息架构（Host 页面 Demo）

```text
Workspace / 节点编辑
├─ 启动命令（1–3）
│  ├─ 拖拽排序列表
│  ├─ 命令项：icon | name | command | 删除
│  └─ 添加命令（不足 3 个时可用）
└─ 多窗口显示（仅命令数 ≥ 2）
   ├─ 展示：Split / Tab
   ├─ Tab 位置：左上 / 右上 / 右下（仅 Tab）
   └─ Split 比例：100% 分配线 + n-1 个拖拽点（仅 Split）

运行中的节点
└─ Multi-window container
   ├─ Split：Terminal A | divider | Terminal B | [divider | Terminal C]
   └─ Tab：tab bar（按位置） + active terminal
```

### 3.1 Demo 原则

第一阶段只实现一个**写死的 Demo workspace**：预置单/双/三窗口节点，以及模拟标题、图标和终端内容。它是可点击、可拖拽的原生 Host 页面 Demo；配置列表、数量限制、展示模式切换、三种 Tab 位置、比例编辑与视觉状态均可验证。所有状态只存于页面内存，关闭即重置；不得接入 Core、进程管理、持久化、真实终端或业务逻辑。

## 4. 三步实施与验收门禁

严格顺序：**Host → Core → Glue**。任一步测试未通过，不进入下一步。

### Step 1 — Host：页面 Demo（不联调）

**交付**：节点编辑页与运行态容器的静态/本地状态 Demo；复用现有节点排序组件或抽取可复用拖拽列表。

**验收测试**：

1. 可新增至 3 条命令，达到上限后添加按钮禁用；可删除但至少留 1 条；command 允许留空并显示空状态。
2. 每项可编辑 icon/name/command，拖拽后顺序、Split 模拟顺序和 Tab 模拟顺序一致。
3. Split、Tab 与三个 Tab 位置都能切换；左上 Tab 同时显示图标和文字，右上/右下图标悬停透明度与选中态符合设计。
4. 2/3 窗口时，比例线分别出现 1/2 个分隔拖拽点，总和始终为 100%。
5. 运行态模拟分隔拖动能即时更新管理页模拟值（仅本地 mock state）。

**门禁**：组件/交互测试、视觉快照（如项目已有）和现有 Host 测试全部通过。

### Step 2 — Core：业务逻辑、架构与接口

**交付**：领域模型、校验、布局计算、状态迁移、持久化抽象及单元测试；Core 不依赖 Host UI 或平台终端实现。

#### 4.2.1 整体架构

```text
Host UI
  │  DTO / user intent
  ▼
Glue adapters ── process/window runtime
  │
  ▼
Workspace Core
  ├─ NodeCommandPolicy（1–3、字段校验、排序）
  ├─ MultiWindowLayoutPolicy（Split/Tab、权重、最小尺寸）
  ├─ SessionState（active tab、标题、运行状态）
  └─ WorkspaceRepository port（配置与布局偏好）
```

#### 4.2.2 建议领域模型

```ts
type CommandId = string;
type WindowDisplayMode = "split" | "tab";
type TabPlacement = "top-left" | "top-right" | "bottom-right";

interface NodeCommand {
  id: CommandId;
  icon?: string;
  name?: string;
  command?: string;
}

interface MultiWindowPreference {
  mode: WindowDisplayMode;
  tabPlacement: TabPlacement;
  // 顺序与 commands 一一对应；仅 split 使用；总和为 1
  splitWeights: number[];
}

interface NodeMultiWindowConfig {
  commands: NodeCommand[]; // 1..3，id 唯一；command 可为空
  preference: MultiWindowPreference;
}
```

运行态标题不写入 `NodeCommand`：它是会话状态 `WindowRuntimeState.title`，用 `name ?? title ?? command` 决定显示文本，避免终端标题刷新污染配置。

#### 4.2.3 对外接口（Core port）

```ts
interface WorkspaceMultiWindowService {
  validate(config: NodeMultiWindowConfig): ValidationResult;
  reorderCommand(nodeId: string, commandId: CommandId, targetIndex: number): NodeMultiWindowConfig;
  addCommand(nodeId: string, command: NewNodeCommand): NodeMultiWindowConfig;
  updateCommand(nodeId: string, commandId: CommandId, patch: CommandPatch): NodeMultiWindowConfig;
  removeCommand(nodeId: string, commandId: CommandId): NodeMultiWindowConfig;
  setDisplayMode(nodeId: string, mode: WindowDisplayMode): MultiWindowPreference;
  setTabPlacement(nodeId: string, placement: TabPlacement): MultiWindowPreference;
  resizeSplit(nodeId: string, dividerIndex: number, ratio: number): MultiWindowPreference;
  setSplitWeights(nodeId: string, weights: number[]): MultiWindowPreference;
}

interface WindowSessionEvents {
  windowTitleChanged(nodeId: string, commandId: CommandId, title: string): void;
  activeWindowChanged(nodeId: string, commandId: CommandId): void;
}
```

`resizeSplit` 的 `ratio` 是相邻二项在其合计宽度中的比例，取值经 Core 裁剪到最小尺寸允许范围；调用成功后返回已归一化权重。

#### 4.2.4 当前 C++ Core 接口

本阶段的实际接口位于 `src/core/workspace/WorkspaceCore.h`，且不依赖 Host、XAML 或终端运行时：

```cpp
std::vector<WorkspaceNodeCommand> EffectiveWorkspaceNodeCommands(const WorkspaceNode&);
WorkspaceMultiWindowValidationResult ValidateWorkspaceNodeMultiWindowConfig(const WorkspaceNode&);
bool SetWorkspaceNodeCommands(WorkspaceNode&, std::vector<WorkspaceNodeCommand>);
bool ReorderWorkspaceNodeCommands(WorkspaceNode&, const std::vector<std::wstring>& orderedIds);
bool SetWorkspaceNodeSplitWeights(WorkspaceNode&, const std::vector<double>&);
bool ResizeWorkspaceNodeSplit(WorkspaceNode&, size_t dividerIndex, double leftRatio);
WorkspaceSplitLayoutResult CalculateWorkspaceNodeSplitLayout(const WorkspaceNode&, double availableWidth, double minimumWindowWidth);
bool SetWorkspaceCommandRuntimeTitle(const WorkspaceNode&, WorkspaceNodeSessionState&, std::wstring_view commandId, std::wstring title);
bool SetWorkspaceNodeActiveCommand(const WorkspaceNode&, WorkspaceNodeSessionState&, std::wstring_view commandId);
std::wstring ResolveWorkspaceCommandDisplayName(const WorkspaceNodeCommand&, const WorkspaceNodeSessionState&);
```

`SetWorkspaceNodeCommands` 拒绝空列表、超过 3 项及重复/空 ID；`command` 字段本身允许为空。权重统一量化为 5% 单位，拖动仅修改相邻两个窗口。读取旧 `startupAction` 时 `EffectiveWorkspaceNodeCommands` 会提供一条合成命令；写回时统一输出 `commands`、`multiWindowMode`、`tabPlacement` 与 `splitWeights`。
`CalculateWorkspaceNodeSplitLayout` 在容器小于窗口数乘以最小宽度时返回 `RequiresHorizontalScroll`，并保持每个窗口最小可见宽度；否则按已归一化权重计算宽度。
`WorkspaceNodeSessionState` 仅保存当前会话的活动命令和运行时标题；显示文本优先配置名、再取运行时标题、最后取命令行，因此 TUI 标题刷新不会改写命令配置。

**验收测试**：字段/数量/ID 校验、重排与增删权重迁移、归一化、相邻分隔拖动、最小尺寸约束、序列化迁移、标题与配置分离，均为确定性单元测试。

**门禁**：Core 单元测试和既有 Core 测试全绿，且没有 UI 或平台依赖倒灌。

### Step 3 — Glue：真实接线与端到端验证

**交付**：Host 调用 Core；配置与会话偏好持久化；一个命令对应一个终端窗口运行实例；终端标题事件回流；真实分隔线拖动保存。

**关键接线**：

1. 节点启动按 `commands` 顺序创建最多三个终端运行实例，并以 `command.id` 作为稳定关联键。
2. Host 的编辑、排序、模式和尺寸 intent 统一经 Core 校验后写入 repository。
3. 运行态拖动通过 `resizeSplit` 更新内存布局，节流写入；pointer-up/drag-end 强制写入。
4. 终端标题变更发到 `windowTitleChanged`；Host 仅订阅会话状态渲染文字 Tab。
5. 旧节点单命令配置迁移为 `commands: [legacyCommand]`，默认 `split/top-left/[1]`，确保兼容。

**验收测试**：

1. 重启应用后命令顺序、模式、Tab 位置与 Split 比例恢复。
2. 同节点 2/3 个真实命令可分别运行；切换 Tab 不中断后台窗口。
3. 运行态拖动分隔线后刷新仍保持比例。
4. Codex/Claude Code 等终端标题更新时，左上 Tab 同步更新；图标 Tab 保持 icon 与正确选中态。
5. 单命令旧节点正常打开且不显示不必要的多窗口控制。

**门禁**：端到端测试、持久化回归、手工多平台终端验证与全量相关测试通过。

## 5. 持久化与迁移

- 节点配置新增 `commands` 与 `multiWindowPreference`。读取时同时兼容旧的单命令 `startupAction`：若 `commands` 缺失，则将其映射为一条命令。
- 使用显式 schema version；节点文件新结构写为 `version: 2`。旧格式在下一次保存时完成单向迁移，不再双写 `startupAction`。
- 偏好按节点 ID 保存；运行时标题、当前运行状态按会话保存，不作为工作区配置同步。
- 无效/损坏权重回退为均分；缺失或无效展示模式回退为 Split；迁移错误必须可观测且不阻断节点启动。

## 6. 非功能约束与待确认项

- icon 的存储格式应沿用项目现有图标系统（标识符而非任意二进制数据）；Host Demo 可用已支持的图标集。
- 拖拽必须可通过键盘替代操作完成，且为图标 Tab 提供可访问名称/tooltip。
- 需要确认“窗口”在现有代码中对应 PTY、TerminalView 还是更上层会话；Glue 以稳定 `command.id` 映射，避免依赖数组索引。
- 若已有工作区同步，配置可同步；会话标题与 active tab 是否跨设备同步需另行产品决策，本设计默认不同步。
