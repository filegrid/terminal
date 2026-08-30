# Workspace 保存与 Path/Cmd 推理方案

## 背景

当前 workspace 保存逻辑在保存 node 启动信息时，取的是 `NewTerminalArgs` 里的：

- `StartingDirectory`
- `Commandline`

对应代码在：

- `src\cascadia\TerminalApp\TerminalPage.cpp:1572-1600`
- `src\cascadia\TerminalApp\TerminalPage.cpp:7095-7120`
- `src\cascadia\TerminalApp\TerminalPaneContent.cpp:87-150`

其中 `TerminalPaneContent::GetNewTerminalArgs(BuildStartupKind::Persist)` 返回的 `Commandline()` 来自当前 control settings，而不是“当前 workspace node 实际需要回放的启动命令”。

## 当前问题

现在保存时：

1. `StartupDirectory` 基本是对的，已经优先取当前工作目录。
2. `StartupAction` 被写成了 `terminalArgs->Commandline()`。

这会导致多保存一级壳层：

- profile 本身已经定义了 shell/启动器；
- 保存时又把这个 `Commandline` 当成 node 的“启动命令”；
- workspace 重新打开时，会先按 profile 起终端，再把 `StartupAction` 作为 `SendInput` 发进去；
- 结果就变成“在已经启动的 shell 里再执行一层启动器/壳命令”。

这就是“多保持了一级”的根因。

## 正确口径

workspace node 保存时，要保留四类**彼此独立**的运行信息：

1. **path**：当前 tab 在 workspace 语义下应保存的工作路径状态
2. **cmd**：当前 tab 在 workspace 语义下应保存的启动/命令文本
3. **os**：当前 tab 在 workspace 语义下应保存的操作系统类型（windows / linux）
4. **shell**：当前 tab 在 workspace 语义下应保存的 shell 类型（cmd / ssh / powershell）

这里最重要的要求是：

1. **`cmd` 和 `path` 没关系**
2. **`path` 需要单独推理**
3. **不能拿保存出来的 `cmd` 去倒推 `path`**
4. **也不能因为 `cmd` 里带路径，就把那段路径当成 `path` 真值**

换句话说：

- `cmd` 保存的是“要回放的命令文本”
- `path` 保存的是“会话当前所在路径状态”
- `os` 保存的是目标操作系统识别结果
- `shell` 保存的是交互 shell 识别结果
- 它们可以由同一次用户操作共同变化，但保存时必须走**独立取值链路**

### 1. `cmd` 的语义

这里的“当前启动命令”不是 profile 的底层 `commandline`，而是这个 node 语义上需要回放的那条命令：

- 如果 node 原本就是 workspace 打开的，并且有 `startupAction`，保存时应继续保留这条 `startupAction`。
- 如果 tab/pane 是用户显式用自定义命令创建的，那么才应该把该次显式覆盖的命令当成 node 的启动命令。
- 如果当前会话只是普通 profile 启动，没有额外业务命令，则 `startupAction` 应为空，不能把 profile shell 本身再塞进去。

### 2. `path` 的语义

这里的“path”不是“某个 API 直接读出来的当前真实 cwd 真值”，而是：

- 结合**会话输入行为**
- 按 shell 语义持续维护的
- 当前 tab 的**工作路径状态**

也就是说：

- `cd foo`
- `Set-Location foo`
- `pushd foo`
- `popd`
- Windows 下直接输入盘符切换，如 `D:`
- 其他会导致 shell 当前工作路径变化的交互

都应该影响保存出来的 `path`。

但是：

- 这些输入只用于**更新 path 状态**
- **不是**说把保存出来的 `cmd` 再拿去倒推一次 `path`
- 也**不是**去从任意命令行里抽一个“像路径的字符串”硬猜

## 这两个值到底怎么拿

这件事必须拆开讲，不能混成一个来源。

### 1. `cmd` 怎么拿

`cmd` 的目标是保存要回放的命令文本，不是当前 shell 的底层 launcher。

#### 1.1 profile/新建 tab 时的底层启动命令

这类命令是：

- `cmd.exe /k ...`
- `pwsh.exe -NoLogo ...`
- `ssh user@host -p 22`

它来自 `NewTerminalArgs.Commandline()` / `control.Settings().Commandline()`，本质是“这个终端最初怎么被拉起来的”。

这个值**能拿到**，但它只代表底层 launcher，不代表用户当前在交互里执行的命令。

#### 1.2 当前/最近一次需要保存的命令文本

仓库里已有输入采集链路：

- `CharSent`
- `StringSent`
- `PendingInput`
- `TrackTerminalInput(...).LastCommand`

这些值适合用于维护“最近一次命令文本”，因为这是 `cmd` 语义本身。

对于 `cmd` 的原则是：

1. 可以从输入采集链路得到；
2. 可以是普通命令；
3. 可以带路径参数；
4. 可以是 Windows 里的盘符/路径式写法；
5. 只要它本来是命令文本，就按命令文本保存；
6. **不要**因为它看起来像路径，就把它改造成 `path` 来源。

### 2. `path` 怎么拿

`path` 不能简单依赖 `control.WorkingDirectory()`。

仓库里已有这些入口：

- `TermControl::WorkingDirectory()`
- `ControlCore::WorkingDirectory()`
- `Terminal::GetWorkingDirectory()`

但这套值本质上依赖 shell integration/客户端上报，Terminal 内核只是保存“客户端最近一次告诉我的工作目录”。

对 `cmd` / `powershell` / `ssh` / 远端 shell 来说，这个值经常只是：

- 启动时目录
- 上一次成功上报的目录
- 某个旧缓存

它**不是**当前工作路径的稳定真值来源。

因此，`path` 的正确方案不是“直接读一个 API”，而是：

1. 建立 **tab 级 path 状态机**
2. 以会话输入行为持续推理这个 tab 当前的路径状态
3. 在保存 workspace 时读取这份路径状态

### 3. `path` 推理的规则

`path` 推理必须以“会导致 shell 工作路径改变的输入行为”为核心，而不是以“保存出来的 cmd 内容”为核心。

#### 3.1 必须支持的目录变更行为

至少要覆盖：

1. `cd foo`
2. `cd /d D:\repo`
3. `chdir foo`
4. `Set-Location foo`
5. `Set-Location -Path foo`
6. `Set-Location -LiteralPath foo`
7. `sl foo`
8. `pushd foo`
9. `push-location foo`
10. `popd`
11. `pop-location`
12. Windows 直接切盘符：`D:`

#### 3.2 Windows 下必须兼容的路径风格

必须支持：

1. 绝对路径：`D:\repo`
2. UNC 路径：`\\server\share\repo`
3. 相对路径：`.\sub`、`..\sub`
4. 仅盘符切换：`D:`
5. 带引号路径：`"D:\Program Files\app"`
6. PowerShell 风格参数：`-Path`、`-LiteralPath`

其中要特别区分两类场景：

1. **目录切换语义**
   - 例如 `cd D:\repo`
   - 例如 `Set-Location .\sub`
   - 例如单独输入 `D:`
   - 这些应更新 path 状态
2. **命令文本语义**
   - 例如 `D:\tools\run.exe`
   - 例如 `.\scripts\build.ps1`
   - 例如 `C:\Windows\System32\cmd.exe /c dir`
   - 这些首先是命令，不应自动把其可执行文件路径当成 path

也就是说：

- **单独盘符输入 `D:`** 在 Windows shell 里有明确的工作盘切换语义，应影响 path
- **带文件名的路径命令** 是命令文本，不应直接改 path

#### 3.3 组合命令

必须支持单行多段命令，例如：

- `cd /d D:\repo && git status`
- `Set-Location .\sub; dir`
- `pushd project && ls`

这里的规则是：

1. 逐段执行语义推理；
2. 目录切换段更新 path 状态；
3. 其他段只更新最近命令文本，不直接更新 path；
4. 最终保存的 `path` 取该行执行完后的路径状态；
5. 最终保存的 `cmd` 仍按命令文本链路维护，不能因为 path 更新了就篡改 cmd。

#### 3.4 目录栈

对于 `pushd/popd`、`push-location/pop-location`，必须维护目录栈：

1. `pushd` 前把当前路径压栈；
2. 切到目标路径；
3. `popd` 时恢复栈顶；
4. 栈空时 `popd` 不应乱改 path。

#### 3.5 锚点与初始化

因为 Terminal 拿不到稳定的当前真实 cwd 真值，path 推理必须有锚点：

1. 优先用创建 tab 时的 `startingDirectory` 作为初始锚点；
2. 若某次 shell integration 上报了 `WorkingDirectory()`，可把它作为校准值；
3. 若会话恢复自已有 workspace node，则优先用 node 的 `startupDirectory` 作为初始锚点；
4. 若完全没有锚点，path 状态允许为空，但不能编造成看似真实的目录。

注意：

- `WorkingDirectory()` 只能作为**可选校准输入**
- 不能把它当稳定真值覆盖整套推理链路

### 4. 对 cmd / powershell / ssh 的实际含义

按 workspace 保存语义，建议这样理解：

| 场景 | path 来源 | cmd 来源 | 备注 |
| --- | --- | --- | --- |
| 普通 cmd / powershell profile | 运行态 path 推理状态 | 默认空 | path 允许被 `cd`、盘符切换等输入持续更新 |
| 显式 commandline override 新建 tab | 运行态 path 推理状态 | 创建时的 `NewTerminalArgs.Commandline()` | 这是用户明确指定的启动命令 |
| workspace node 自己带 `startupAction` 打开 | 运行态 path 推理状态（初始锚点可来自 node） | node 运行态里原始 `startupAction` | 不能回退成 profile commandline |
| ssh profile | 本地只能维护本地可见输入语义推理；远端 cwd 若无额外上报则不承诺绝对真实 | 默认空，除非本次是显式 override 或 node 原本就有 `startupAction` | ssh 的 path 推理只能做到语义近似，不能伪装成已拿到远端真实 cwd |

## 关键结论

这件事的核心不是“统一反查所有 shell 的真实 cwd 和完整运行命令”，因为那条路本身就不成立。

正确做法是：

1. **cmd 与 path 完全解耦**
2. **cmd**：走命令文本链路
3. **path**：走独立的路径状态机推理链路
4. `WorkingDirectory()` 只能做辅助校准，不能当稳定真值
5. 对于**未保存窗口**，按 tab 维度分别维护 path 状态与 cmd 状态
6. 对于**已保存 workspace** 的 tab，直接保留 node 已保存配置，不再做任何运行态推理、覆盖、回填或校准

## 方案

### 1. 拆开“底层创建命令”和“node 启动命令”

不要再直接用 `terminalArgs->Commandline()` 回填 `WorkspaceNode.StartupAction`。

`WorkspaceNode.StartupAction` 的来源改为一条独立的 node 级运行态字段，专门表示：

- 该 tab/pane 在 workspace 语义下要回放的启动命令。

建议新增一份 page 级映射，按 tab 或按 contentId 维护，例如：

- `contentId -> startupAction`
  或
- `tab runtime id -> startupAction`

### 2. 在创建 workspace tab 时把 startupAction 记住

workspace 恢复时，`WorkspaceManager::BuildStartupActions(...)` 会先创建 tab，再追加 `SendInput`。

在这条链路里，需要在 tab/pane 运行态上同步记住当前 node 的 `startupAction`，这样后续“再次保存当前窗口为 workspace”时，能取回原始 node 命令，而不是退回到底层 profile commandline。

要求：

1. `startupAction` 为空时，不记录伪值。
2. `startupAction` 非空时，按 node 原值记录，不做二次包装。

### 3. 对显式命令创建的 tab，单独记录覆盖命令

对于不是从 workspace 打开的 tab，如果它是通过显式 `newTab/splitPane` 命令覆盖了 `commandline` 创建的，也需要把这条“用户显式覆盖命令”记成 node 启动命令。

建议在创建 `TermControl` / `TerminalPaneContent` 时，把用于创建当前内容的原始 `NewTerminalArgs` 中这两个字段单独保存下来：

- evaluated starting directory
- explicit commandline override

保存 workspace 时优先读取这份“创建时显式覆盖值”，而不是从当前 `control.Settings()` 反推。

### 4. 保存时的取值优先级

保存 node 时建议改成下面的优先级：

1. `StartupDirectory`
   - 优先 tab 级 path 状态机当前值；
   - 没有状态值时回退创建时的 `startingDirectory`；
   - 仍推理不出来时，再回退接口返回的默认路径值；
   - 再不行才允许为空；
   - **不要**直接把 `WorkingDirectory()` 当成稳定真值覆盖状态机；
   - **不要**从保存出来的 `cmd` 字符串再反推一次目录。
2. `StartupAction`
   - 优先 workspace/runtime 里记录的 node 启动命令；
   - 其次是创建时显式覆盖的 commandline；
   - 如果两者都没有，则置空；
   - **不要**再直接回退到 profile/control settings 的 `Commandline()`。

### 5. 打开 workspace 时继续沿用现有执行方式

当前“新建 tab + 可选 SendInput”的执行模型可以保留，不需要改打开协议：

1. `StartupDirectory` 继续放进 `NewTerminalArgs.StartingDirectory`
2. `StartupAction` 继续在 tab 创建后用 `SendInput` 下发

这次只修正“保存时如何提取 node 信息”，不改“打开时如何回放 node 信息”。

## 改造点

### 1. `TerminalPage::_TryCaptureCurrentWorkspace`

当前：

- `node.StartupDirectory = terminalArgs->StartingDirectory()`
- `node.StartupAction = terminalArgs->Commandline()`

需要改成：

- `node.StartupDirectory = 当前会话有效工作目录`
- `node.StartupAction = 运行态记录的 node 启动命令`

### 2. `TerminalPaneContent` / `TerminalPage` 运行态

新增一份只用于 workspace 保存的运行态结构，至少包含：

- `ContentId`
- `PathState`
- `PreviousPathState`
- `DirectoryStack`
- `ResolvedStartingDirectory`
- `StartupAction`
- `LastCommand`
- `Source`（workspace / explicit override / none，可选）

这里要明确：

- `PathState` 和 `LastCommand` 是两个字段
- 更新 `LastCommand` 不代表更新 `PathState`
- 更新 `PathState` 也不代表篡改 `LastCommand`

### 3. workspace 打开链路

在根据 `WorkspaceNode` 创建 tab 时，把 node 的 `startupAction` 写入上述运行态缓存。

### 4. 普通新建 tab / split pane 链路

如果 `NewTerminalArgs.Commandline()` 是调用方显式传入的覆盖命令，则同步记录到运行态缓存；
如果只是 profile 自带 shell，则不要记成 node 启动命令。

同时，path 状态机需要在此时初始化锚点：

1. 有 `startingDirectory` 时，用它初始化；
2. 没有时保持空状态；
3. 后续由目录切换类输入持续更新。

## 兼容与迁移

这次不需要改 `workspaces.yaml` 结构：

- `startupDirectory`
- `startupAction`

字段继续沿用，只修正保存时的填充值。

对已有错误保存的数据，不做自动迁移。用户下一次重新保存 workspace 后，新值自然覆盖旧值。

## 验收

至少验证下面几类场景：

1. 普通 profile 打开 tab，执行 `cd foo` / `Set-Location foo` / `D:` / `cd /d D:\repo` 后保存 workspace  
   结果：`startupDirectory` 保存推理后的当前路径状态，`startupAction` 为空。

2. workspace node 配了启动命令，例如 `ssh xxx`，打开后再次保存  
   结果：仍保存 `ssh xxx`，不会变成 profile shell 命令。

3. 使用显式 commandline override 新开 tab，再保存 workspace  
   结果：保存这条 override 命令，而不是 profile 默认 shell。

4. 在 Windows 下直接输入盘符 `D:` 再执行 `git status` 后保存 workspace  
   结果：`startupDirectory` 反映切盘后的路径状态；`startupAction` 不因盘符切换而被污染。

5. 在 Windows 下执行 `D:\tools\run.exe` 或 `.\scripts\build.ps1` 后保存 workspace  
   结果：这些文本保留为命令语义；不能因为它们看起来像路径，就把其文件路径写成 `startupDirectory`。

6. 从已保存 workspace 反复“打开 -> 保存 -> 再打开”  
   结果：启动命令不逐轮套娃，不多出一层 shell。

## 实施顺序

1. 增加 node 启动命令运行态缓存。
2. 增加 tab 级 path 状态机运行态缓存。
3. 在 workspace 打开链路写入 `startupAction` 与 path anchor 运行态。
4. 在普通显式命令建 tab 链路写入 override 命令运行态，并初始化 path anchor。
5. 调整 `_TryCaptureCurrentWorkspace(...)` 的保存取值逻辑。
6. 用 portable 产物做回归验证。

## Path 推理详细设计

### 1. 目标

本设计只解决一件事：

- **保存 workspace 时，如何为每个 tab 推理并持久化正确的 `path`**

这里的 `path` 指的是：

- 当前 tab 在 workspace 语义下应保存的**工作路径状态**

不是：

- profile 默认启动目录
- 某个 API 偶尔返回的 cwd 缓存
- 当前命令文本里的某段路径字符串
- 当前前台进程 exe 的文件路径

### 2. 核心原则

#### 2.1 `cmd` 和 `path` 完全解耦

必须把下面两件事完全拆开：

1. `cmd`
   - 保存“要回放的命令文本”
2. `path`
   - 保存“当前 tab 的工作路径状态”

规则：

1. **不能**拿 `cmd` 去反推 `path`
2. **不能**因为 `cmd` 里带路径，就把那段路径当成 `path`
3. **不能**为了修 `cmd`，把 `path` 退回启动目录
4. `cmd` 出错和 `path` 出错是两套问题，必须分别处理

#### 2.2 `path` 依赖推理，不依赖单一真值接口

当前 Terminal 在 `cmd` / `powershell` / `ssh` 等场景下，没有稳定、统一、随时可读的“当前真实 cwd 真值”接口。

现有可见来源：

1. `TermControl::WorkingDirectory()`
2. `ControlCore::WorkingDirectory()`
3. `Terminal::GetWorkingDirectory()`
4. root process cwd
5. `startingDirectory`

这些都不能直接当成最终真值：

1. `WorkingDirectory()` 依赖 shell integration 上报，很多时候只是启动目录或上一次成功上报值
2. root process cwd 只是根进程视角，不等于 shell 运行期当前目录
3. `startingDirectory` 只是初始锚点，不代表用户后来没有切目录

所以结论只有一个：

- **`path` 必须靠 tab 级运行态推理**

### 3. 设计范围

#### 3.1 本设计解决

1. workspace 保存时每个 tab 的 `path` 推理
2. Windows 本地 shell 下的目录切换语义
3. PowerShell 目录命令语义
4. 目录栈语义
5. 盘符切换语义
6. 多段命令同一行执行后的路径收敛
7. 保存时的路径状态读取

#### 3.2 本设计不解决

1. 远端 ssh 会话 100% 真实 cwd 反查
2. 所有 shell 方言的完整语义执行
3. 任意脚本语言内部的 `chdir` 行为解析
4. 任意可执行程序运行过程中隐式修改 cwd 的精确追踪
5. 从屏幕缓冲区回溯整段历史来推理目录

### 4. 术语

#### 4.1 PathState

当前 tab 推理得到的工作路径状态。

#### 4.2 Anchor

初始化推理状态时的起始路径锚点。

#### 4.3 Directory Mutation Command

会改变 shell 当前工作路径状态的命令或输入行为。

#### 4.4 Command Text

用户输入并发送给终端的命令文本。它可以包含路径，但它本身不等于 `path`。

### 5. 运行态模型

每个 tab 或 contentId 需要维护一份独立状态，建议字段如下：

```text
WorkspacePathState
- ContentId
- CurrentPath
- PreviousPath
- DirectoryStack[]
- AnchorPath
- LastCalibratedPath
- LastInputLine
- LastMutationKind
- HasReliableAnchor
- SourceKind
- LastPathPerDrive['C'..'Z']
```

#### 5.1 字段说明

1. `CurrentPath`
   - 当前推理结果
2. `PreviousPath`
   - 上一次 path，用于 `cd -` 或等价场景
3. `DirectoryStack[]`
   - `pushd/popd`、`push-location/pop-location` 所需目录栈
4. `AnchorPath`
   - 初始化锚点
5. `LastCalibratedPath`
   - 最近一次由 `WorkingDirectory()` 或其他可信输入校准的值
6. `LastInputLine`
   - 最近一次参与路径推理的输入行，仅用于调试或日志
7. `LastMutationKind`
   - 最近一次路径变化类型，便于诊断
8. `HasReliableAnchor`
   - 当前是否已有可用锚点
9. `SourceKind`
   - 初始化来源：workspace node / startingDirectory / shell-report / unknown
10. `LastPathPerDrive`
   - 记录每个 Windows 盘符最近一次已知路径，用于 `D:` 这类切盘语义

#### 5.2 隔离要求

必须满足：

1. 每个 tab 独立一份状态
2. pane 若共享同一 contentId，按当前保存语义确认是否共享；若不共享，需下沉到 pane 粒度
3. 切 tab、切 workspace、打开新窗口都不能串状态
4. 关闭 tab 时清理对应状态

### 6. Anchor 初始化

#### 6.1 优先级

初始化锚点建议按下面顺序：

1. 当前 tab 若来自已保存 workspace node，优先用 node 的 `startupDirectory`
2. 若创建 tab 时显式提供 `startingDirectory`，用它
3. 若 `WorkingDirectory()` 在创建后立刻有可信上报，可用作锚点
4. 若以上都没有，状态保持空

#### 6.2 约束

1. 没有锚点时，不能伪造 `C:\`、用户目录或 profile 默认目录
2. 空锚点允许存在，但后续只有遇到可确定的绝对路径行为时才能进入非空状态
3. 校准只能更新 path 状态，不能回写为 `cmd`

### 7. 输入采集

`path` 推理只依赖**发送给终端的输入流**，不依赖保存时回看 `cmd` 字段。

建议继续使用现有输入采集入口：

1. `CharSent`
2. `StringSent`
3. `PendingInput`

但要明确：

1. 采集输入是为了**实时更新 path 状态**
2. 不是为了保存时再从 `cmd` 字符串里做二次推演

### 8. 解析总流程

每次拿到一行已发送输入后，执行以下流程：

1. 标准化输入
2. 按命令分隔符切段
3. 对每个 segment 做 shell 语义判断
4. 如果是目录变更语义，则更新 `CurrentPath`
5. 如果不是目录变更语义，则不改 `CurrentPath`
6. 所有 segment 处理完后，当前 `CurrentPath` 即该行结束后的路径状态

### 9. 支持的命令分隔符

#### 9.1 首版必须支持

1. `&&`
2. `;`

#### 9.2 可选支持

1. `||`
2. PowerShell 管道前后的特殊语义

首版处理原则：

1. `&&` 视作顺序段分隔
2. `;` 视作顺序段分隔
3. 即使不模拟命令成功失败，也应保持顺序更新语义
4. 不在首版模拟复杂条件执行短路

### 10. 目录变更语义

#### 10.1 必须支持的目录命令

##### cmd / 通用

1. `cd foo`
2. `cd /d D:\repo`
3. `chdir foo`
4. `pushd foo`
5. `popd`

##### PowerShell

1. `Set-Location foo`
2. `Set-Location -Path foo`
3. `Set-Location -LiteralPath foo`
4. `sl foo`
5. `push-location foo`
6. `pop-location`

#### 10.2 Windows 特有输入

1. 单独盘符切换：`D:`
2. 单独 UNC 切换：`\\server\share`
3. 带引号目录参数：`cd "D:\Program Files\repo"`

#### 10.3 目录命令参数提取规则

需要支持：

1. 跳过 cmd 的 `/d`
2. 识别 PowerShell 的 `-Path`
3. 识别 PowerShell 的 `-LiteralPath`
4. 保留被引号包裹的路径原义
5. 支持包含空格的路径

#### 10.4 特殊语义

1. `cd -`
   - 若后续支持，则切回 `PreviousPath`
2. `popd`
   - 仅从目录栈恢复
3. 栈为空时 `popd`
   - 不改当前路径
4. `pushd`
   - 先压栈再切换

### 11. Windows 路径识别

#### 11.1 必须识别的路径形式

1. `D:\repo`
2. `D:/repo`
3. `\\server\share\repo`
4. `\relative-on-current-drive`
5. `.\sub`
6. `..\sub`
7. `D:`
8. `"D:\Program Files\repo"`

#### 11.2 规范化要求

1. `/` 转成 `\`
2. 保留盘符大小写统一策略，建议大写盘符
3. 去掉无意义的 `.` 段
4. 正确处理 `..`
5. UNC 路径保留 `\\server\share` 前缀
6. 不随意调用文件系统确认路径存在性

#### 11.3 `D:` 的特殊规则

`D:` 不能简单按“像路径的命令文本”处理，必须按 Windows shell 语义处理。

规则：

1. 当整段输入仅为 `D:` 时，视为工作盘切换
2. 若当前状态里已记录过 `D` 盘最近路径，则切到该盘最近路径
3. 若没有记录过 `D` 盘最近路径，则退化为 `D:\`
4. 切盘后不应把 `D:` 写成 `cmd` 触发的路径字符串回填逻辑

### 12. 命令文本与路径的区分

这是最容易出错的地方。

#### 12.1 这些属于路径变更语义

1. `cd D:\repo`
2. `Set-Location .\sub`
3. `pushd \\server\share`
4. `D:`

#### 12.2 这些属于命令文本语义，不直接改 path

1. `D:\tools\run.exe`
2. `.\scripts\build.ps1`
3. `C:\Windows\System32\cmd.exe /c dir`
4. `python D:\repo\tool.py`
5. `git -C D:\repo status`
6. `code D:\repo`

#### 12.3 判定原则

不要用“看起来像路径”来判断是否改 path。

只能在以下情况下更新 path：

1. 命令名本身是目录变更命令
2. 或整段输入匹配 Windows 的单独盘符切换语义
3. 或未来明确支持的其他目录变更语义

除此之外，即使命令里出现路径，也只当命令参数。

### 13. 多段命令

#### 13.1 例子

1. `cd /d D:\repo && git status`
2. `Set-Location .\sub; dir`
3. `pushd project && ls`
4. `D: && cd repo && build.cmd`

#### 13.2 处理规则

1. 按顺序切段
2. 每段独立判断是否改 path
3. 改 path 的段立即更新状态
4. 其后段看到的是更新后的路径
5. 最终保存的是整行执行完成后的 `CurrentPath`

### 14. PowerShell 细节

#### 14.1 首版必须支持

1. `Set-Location foo`
2. `Set-Location -Path "D:\Program Files\repo"`
3. `Set-Location -LiteralPath \\server\share\repo`
4. `sl ..`
5. `push-location foo`
6. `pop-location`

#### 14.2 暂不保证

1. `Set-Location` 的复杂脚本表达式求值
2. 变量展开后的真实路径
3. 子表达式 `$(...)`
4. 脚本块内多层动态目录切换

策略：

1. 首版只支持静态文本参数
2. 复杂表达式识别失败时，不乱改 path

### 15. cmd 细节

#### 15.1 首版必须支持

1. `cd foo`
2. `cd /d D:\repo`
3. `chdir foo`
4. `pushd foo`
5. `popd`
6. `D:`

#### 15.2 注意

1. `cd` 不带参数时，不改 path
2. `cd /d` 需要同时允许切盘符和切目录
3. `pushd` 对 UNC 路径要入栈并切换

### 16. ssh 场景

ssh 场景必须单独写清楚，不能假装“已获取远端真实 cwd”。

#### 16.1 本地可做的事

1. 维护用户输入语义上的路径状态
2. 识别远端 shell 中显式输入的 `cd` / `pwd` 类切换语义
3. 基于锚点和输入继续推理

#### 16.2 本地做不到的事

1. 直接读到远端真实 cwd
2. 确认远端脚本内部是否改目录
3. 确认远端命令失败后目录是否真的变化

#### 16.3 结论

1. ssh 下的 `path` 是**语义推理值**
2. 不是远端真实 cwd 保证值
3. 文档和实现里都要明确这一点

### 17. 校准机制

虽然 `WorkingDirectory()` 不能当真值，但仍有价值。

#### 17.1 可以做什么

1. 当 shell integration 可靠上报时，用它校准 `CurrentPath`
2. 当推理状态为空而上报值存在时，用它补锚点
3. 当推理状态明显失真时，用它矫正

#### 17.2 不能做什么

1. 不能把它当唯一来源
2. 不能每次保存时直接覆盖状态机结果
3. 不能因为它有值，就丢掉已推理出的路径状态

#### 17.3 建议规则

1. `CurrentPath` 为空时，可接受 `WorkingDirectory()` 作为锚点
2. `CurrentPath` 非空且 `WorkingDirectory()` 为空时，保留状态机结果
3. `CurrentPath` 非空且 `WorkingDirectory()` 非空但冲突时：
   - 若当前输入刚好是目录变更命令，以状态机结果优先
   - 若长时间无输入且 shell 主动上报新值，可接受校准

### 18. 保存策略

#### 18.1 `StartupDirectory`

优先级：

1. `PathState.CurrentPath`
2. `AnchorPath`
3. 空

明确禁止：

1. 直接用 `cmd` 文本反推
2. 直接退回 profile 默认目录伪装成当前路径

#### 18.2 `StartupAction`

独立处理：

1. workspace node 原始 `startupAction`
2. 显式 commandline override
3. 否则为空

`StartupAction` 的生成不能影响 `StartupDirectory`。

### 19. 失败与降级策略

#### 19.1 推理失败

如果某条输入无法可靠解析：

1. 不乱改 `CurrentPath`
2. 保留原状态
3. 可记录诊断信息

#### 19.2 无锚点

若没有任何锚点：

1. 保持空状态
2. 等待后续出现绝对路径切换语义
3. 不伪造默认目录

#### 19.3 冲突

若状态机推理与外部校准冲突：

1. 保留冲突记录
2. 按上文校准策略处理
3. 不静默覆盖到用户无法理解

### 20. 实现步骤

#### 20.1 第一阶段：抽独立状态机

新增独立模块，例如：

- `ext\src\chat\WorkspacePathInference.h`
- `ext\src\chat\WorkspacePathInference.cpp`

职责：

1. 维护 `WorkspacePathState`
2. 解析输入行
3. 应用目录变更语义
4. 提供 `CurrentPath()`

#### 20.2 第二阶段：接入 TerminalPage

在 `TerminalPage` 里：

1. 为每个 `contentId` 持有一份 path 状态
2. 在 `CharSent` / `StringSent` flush 后喂入状态机
3. 在 workspace 打开链路设置 anchor
4. 在 tab 关闭时释放状态

#### 20.3 第三阶段：保存逻辑切换

`_TryCaptureCurrentWorkspace(...)` 保存 path 时：

1. 先读 path 状态机
2. 再读 anchor
3. 最后允许为空

不能：

1. 再从 `LastCommand`
2. 再从 `CommandHistory().CurrentCommandline()`
3. 再从“看起来像路径的命令文本”

去反推 `StartupDirectory`

#### 20.4 第四阶段：校准接入

把 `WorkingDirectory()` 作为可选校准输入接入：

1. 初始化补锚点
2. 长时间无输入时做被动校准
3. 不覆盖明确的目录变更推理结果

### 21. 测试矩阵

#### 21.1 cmd

1. `cd test`
2. `cd /d D:\repo`
3. `pushd foo && popd`
4. `D:`
5. `D: && cd repo`

#### 21.2 PowerShell

1. `Set-Location .\sub`
2. `Set-Location -Path "D:\Program Files\repo"`
3. `sl ..`
4. `push-location foo; pop-location`

#### 21.3 命令与路径分离

1. `D:\tools\run.exe`
2. `.\scripts\build.ps1`
3. `python D:\repo\tool.py`
4. `git -C D:\repo status`

要求：

1. `cmd` 可保留文本
2. `path` 不被这些命令文本直接污染

#### 21.4 组合命令

1. `cd /d D:\repo && git status`
2. `Set-Location .\sub; dir`
3. `D: && build.cmd`

#### 21.5 ssh

1. `cd /srv/app && ls`
2. `pushd project; popd`
3. 无 shell integration 上报场景

要求：

1. 结果按语义推理
2. 文档上明确不承诺远端真实 cwd

### 22. 验收标准

满足以下条件才算方案正确：

1. 每个 tab 的 path 独立维护
2. path 不会因为修 cmd 而回退到启动目录
3. `D:` 能正确影响 path
4. `D:\tools\run.exe` 不会被误当成目录切换
5. `pushd/popd` 目录栈行为正确
6. ssh 场景不伪装成“已拿到远端真实 cwd”
7. 保存 workspace 时，`StartupDirectory` 只来自 path 状态链路

### 23. 一句话结论

workspace 保存里的 `path` 不是“读出来”的，也不是从 `cmd` 里“猜出来”的；它必须是**每个 tab 独立维护的一份路径状态机推理结果**。
