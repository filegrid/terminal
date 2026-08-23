# Workspace Path/Cmd/OS/Shell 推理独立设计

## 1. 目标

本设计单独定义 workspace 保存时，每个 tab 的 `path`、`cmd`、`os` 与 `shell` 如何推理、维护、保存。

本设计解决：

1. 每个 tab 独立保存自己的 `path`
2. 每个 tab 独立保存自己的 `cmd`
3. 每个 tab 独立保存自己的 `os`
4. 每个 tab 独立保存自己的 `shell`
5. `path`、`cmd`、`os`、`shell` 都通过运行态推理获得
6. `path` 与 `cmd` 使用两条独立推理链路，`os/shell` 使用独立识别链路

本设计不解决：

1. 所有 shell 方言的完整解释执行
2. 远端 ssh 会话 100% 真实 cwd 反查
3. 任意脚本内部 `chdir` 的精确追踪

## 1A. 用户使用指南

为了让未保存 tab 的 `path/cmd/os/shell` 推理更稳定，建议用户：

1. 尽量直接输入完整命令再发送
2. 尽量直接输入完整路径或完整目录切换命令
3. **尽量不要依赖 `Tab`、方向键、候选补全、历史补全等交互来完成关键命令**

原因：

1. 这类补全/历史导航交互本身不稳定
2. 它们更像编辑辅助动作，不一定能稳定表达最终命令语义
3. 频繁依赖这些交互时，更容易让未保存 tab 的推理结果变差

这条指南只针对：

1. **未保存 tab 的首次保存推理**

不影响：

1. 已保存 workspace tab 的既有 node 配置

## 2. 核心口径

### 2.1 path、cmd、os、shell 都要推理

workspace 保存时：

1. `path` 不是直接抄某个静态字段
2. `cmd` 也不是直接抄 profile 的底层 launcher
3. `os` 不能只靠用户猜测或固定默认值
4. `shell` 不能只靠 profile 名字硬编码
5. 四者都必须根据 tab 的运行态行为推理

### 2.2 path 和 cmd 是两条独立链路

必须明确：

1. **不能**拿 `cmd` 去反推 `path`
2. **不能**因为 `cmd` 里带路径，就把那段路径写成 `path`
3. **不能**为了修 `cmd` 让 `path` 回退到启动目录
4. **不能**为了修 `path` 把 `cmd` 改成路径推理结果

一句话：

- `cmd` 保存的是命令语义
- `path` 保存的是工作路径状态语义

### 2.3 已保存 tab 与未保存 tab 的边界

这条规则必须单独写清楚：

1. **未保存窗口里的 tab**
   - 允许使用运行态推理结果生成 `path/cmd/os/shell`
2. **已保存 workspace 打开的 tab**
   - 直接保留 workspace node 里已有的 `startupDirectory/startupAction`
   - **不需要再做任何运行态推理、覆盖、回填、校准**

也就是说：

1. 推理链路主要服务于**未保存窗口 -> 首次保存 workspace**
2. 对于**已保存 workspace -> 再次保存**
   - 直接使用已保存 node 配置
   - 不允许运行态 heuristic 把 node 配置冲掉

这样做的原因：

1. 已保存 workspace 的 node 配置是显式管理数据
2. 运行态推理是近似值，不应参与覆盖显式配置
3. 否则会造成反复“打开 -> 保存 -> 再打开”后配置漂移

## 3. path 的定义

`path` 指：

- 当前 tab 在 workspace 语义下应保存的**工作路径状态**

它不是：

1. profile 默认目录
2. 创建 tab 时的初始目录快照
3. `WorkingDirectory()` 的一次性缓存值
4. 命令文本里出现的某个路径字符串
5. 可执行文件所在路径

## 4. cmd 的定义

`cmd` 指：

- 当前 tab 在 workspace 语义下应保存的**命令文本结果**

它不是：

1. profile 自带 shell launcher
2. 当前前台进程完整命令行真值
3. 任意时候屏幕上看到的 prompt 文本拼接结果

### 4.1 cmd 的可信来源

优先级应是：

1. workspace node 原始 `startupAction`
2. 显式 commandline override
3. 运行态输入链路推理出的最近有效命令文本
4. 否则为空

### 4.2 cmd 的原则

1. 可以是普通命令
2. 可以带路径参数
3. 可以是 Windows 直接路径命令
4. 可以是 `D:\tools\run.exe`
5. 可以是 `.\scripts\build.ps1`
6. 可以是 `python D:\repo\tool.py`

但这些都只是 **cmd**，不能直接变成 **path**

## 4A. os 与 shell 的定义

### 4A.1 os 的定义

`os` 指：

- 当前 tab 在 workspace 语义下应保存的目标操作系统类型

当前需求范围内只区分：

1. `windows`
2. `linux`

### 4A.2 shell 的定义

`shell` 指：

- 当前 tab 在 workspace 语义下应保存的交互 shell 类型

当前需求范围内只区分：

1. `cmd`
2. `powershell`
3. `ssh`
4. `wsl`

### 4A.3 os 与 shell 的关系

1. `os` 与 `shell` 相关，但不是同一个字段
2. `shell=ssh` 时，`os` 仍需要独立推理
3. `shell=powershell` 不自动等于 `os=windows`
4. `shell=cmd` 当前只应落在 `os=windows`

## 4B. os/shell 推理目标

对于**未保存 tab**，保存时除 `path/cmd` 外，还要推理出：

1. `os`
2. `shell`

用途：

1. 决定 path 推理使用 Windows 还是 POSIX 规则
2. 决定目录命令集合
3. 决定保存后的 node 展示和后续恢复策略

## 5. 为什么 path 不能直接读现成值

当前可见来源：

1. `TermControl::WorkingDirectory()`
2. `ControlCore::WorkingDirectory()`
3. `Terminal::GetWorkingDirectory()`
4. root process cwd
5. `startingDirectory`

这些都不稳定：

1. `WorkingDirectory()` 依赖 shell integration，上报不到时经常只是启动目录或旧缓存
2. root process cwd 不是 shell 当前运行期 cwd
3. `startingDirectory` 只是锚点，不是当前状态

所以结论：

- `path` 必须靠运行态推理维护

## 6. 当前已实现的 harness 能力

当前 harness 在：

- `ext\src\chat\TerminalInputHarness.cpp`

当前已有能力：

1. 输入拆行：`SplitTerminalInputLines`
2. 命令分段：支持 `&&`、`;`
3. 基础引号处理：单引号、双引号
4. 目录命令识别：
   - `cd`
   - `chdir`
   - `set-location`
   - `sl`
   - `pushd`
   - `push-location`
   - `popd`
   - `pop-location`
5. Windows 路径识别与归一化
6. POSIX 路径识别与归一化
7. 目录参数提取：
   - `/d`
   - `-Path`
   - `-LiteralPath`
8. 目录栈维护
9. 运行态 `TrackTerminalInput(...)` 推理

当前**还没有**独立完成：

1. `os` 推理链路
2. `shell` 推理链路

当前已有测试在：

- `microsoft\src\cascadia\LocalTests_TerminalApp\TerminalInputHarnessTests.cpp`

当前已有测试覆盖：

1. `SplitInputIntoSingleLines`
2. `TracksWindowsCdChain`
3. `TracksPowerShellSetLocationChain`
4. `TracksPosixPushdPopd`
5. `UsesReportedWorkingDirectoryAsAnchor`

## 7. 当前尚未补齐的关键缺口

当前还没完整覆盖的重点：

1. **Windows 单独盘符切换**：`D:`
2. **路径命令 vs 目录切换严格区分**
   - `D:` 应该改 path
   - `D:\tools\run.exe` 不应该直接改 path
3. 更复杂 PowerShell 表达式
4. **Linux 本地 shell 兼容细则还不够完整**
5. ssh 远端真实 cwd 场景
6. drive-local current directory 语义
7. `os` / `shell` 的独立推理与落盘

## 8. 支持哪些系统/壳

### 8.1 当前设计必须支持

1. **Windows cmd**
2. **Windows PowerShell / PowerShell**
3. **Linux 本地 POSIX shell**
4. **ssh 场景下的输入语义推理**

### 8.1A 未保存 tab 必须额外推理的元信息

1. `os`：`windows` / `linux`
2. `shell`：`cmd` / `powershell` / `ssh` / `wsl`

这两个值与 `path/cmd` 一起构成未保存 tab 首次保存时的推理结果。

### 8.1B 已保存 tab 的处理

1. 已保存 tab 不再做运行态覆盖
2. 若 node 已有 `os/shell/path/cmd` 配置，则直接保留
3. 未保存 tab 才走 `os/shell/path/cmd` 推理链路

### 8.2 Windows cmd 必须支持

1. `cd foo`
2. `cd /d D:\repo`
3. `chdir foo`
4. `pushd foo`
5. `popd`
6. `D:`

### 8.3 PowerShell 必须支持

1. `Set-Location foo`
2. `Set-Location -Path foo`
3. `Set-Location -LiteralPath foo`
4. `sl foo`
5. `push-location foo`
6. `pop-location`

### 8.4 POSIX 基础语义必须支持

1. `/home/dev`
2. `./sub`
3. `../sub`
4. `pushd project`
5. `popd`

### 8.5 Linux 本地 shell 必须支持

本节专门定义 Linux 本地 shell，不和 ssh 混写。

#### 8.5.1 目标 shell

首版按 POSIX 风格输入语义兼容：

1. `bash`
2. `zsh`
3. `sh`

尽量兼容，但不承诺完整支持：

1. `fish`
2. 其他非 POSIX shell

#### 8.5.2 必须支持的目录变更输入

1. `cd project`
2. `cd ./project`
3. `cd ../project`
4. `cd /srv/app`
5. `cd ~`
6. `cd -`
7. `pushd project`
8. `popd`

#### 8.5.3 必须识别为 cmd、不能直接改 path 的输入

1. `/usr/bin/python3`
2. `./build.sh`
3. `../tools/run`
4. `python /srv/app/tool.py`
5. `git -C /srv/app status`
6. `env VAR=1 ./run.sh`

规则：

1. 长得像路径，不代表目录切换
2. 只有目录变更语义才允许修改 path

#### 8.5.4 Linux 本地 shell 的组合命令

必须支持：

1. `cd /srv/app && git status`
2. `pushd project; ls`
3. `cd ../repo && ./build.sh`

规则：

1. 目录变更段更新 path
2. 普通执行段只更新 cmd，不直接改 path
3. 最终保存整行结束后的 path 状态

### 8.6 ssh 场景必须明确

ssh 下只能维护：

1. 用户输入语义上的 path 状态
2. 基于锚点和命令的近似推理

ssh 下不能承诺：

1. 已拿到远端真实 cwd
2. 已精确知道远端脚本内部目录变化

### 8.7 os/shell 推理规则

#### 8.7.1 shell 推理优先级

建议优先级：

1. 连接/配置显式类型
2. 创建 tab 时的 commandline / profile 信息
3. 运行态输入特征

#### 8.7.2 shell 识别规则

1. 命中 `cmd.exe`、`ComSpec`、典型 `cmd` 语法时，判为 `cmd`
2. 命中 `powershell.exe`、`pwsh.exe`、`Set-Location`、`push-location`、`pop-location`、`sl` 等语法时，判为 `powershell`
3. 命中 `ssh` 连接、ssh profile、ssh launcher 时，判为 `ssh`
4. 命中 `Microsoft.WSL` profile、`wsl.exe` 或遗留 `bash.exe` launcher 时，判为 `wsl`

#### 8.7.3 os 识别规则

1. `shell=cmd` 时，`os=windows`
2. `shell=powershell` 时：
   - 若 profile / commandline / 路径语法明显是 Windows，优先 `windows`
   - 若运行态路径和命令长期表现为 POSIX 远端语义，允许判为 `linux`
3. `shell=ssh` 时：
   - 优先根据 ssh 目标会话的已知配置/上下文判断
   - 判断不出来时允许保留 unknown-like internal state，但落盘前按当前需求应收敛到 `linux` 或 `windows`

#### 8.7.4 输入特征辅助判断

可用于辅助但不能单独决定：

1. Windows 路径：`D:\repo`
2. Windows 切盘：`D:`
3. PowerShell 命令：`Set-Location`
4. POSIX 路径：`/srv/app`
5. POSIX 家目录：`~`

规则：

1. 这些特征只能作为辅助信号
2. 不能让单次偶然输入把 shell/os 误判死

## 9. path 推理状态机

每个 tab 或 contentId 需要独立状态：

```text
WorkspacePathState
- ContentId
- OperatingSystem
- ShellKind
- CurrentPath
- PreviousPath
- DirectoryStack[]
- AnchorPath
- LastCalibratedPath
- LastInputLine
- LastMutationKind
- HasReliableAnchor
- SourceKind
- LastPathPerDrive
```

### 9.1 字段说明

1. `CurrentPath`
   - 当前 path 推理结果
2. `OperatingSystem`
   - 推理得到的 `windows` / `linux`
3. `ShellKind`
   - 推理得到的 `cmd` / `powershell` / `ssh` / `wsl`
4. `PreviousPath`
   - 前一个 path，用于 `cd -` 或类似语义
5. `DirectoryStack`
   - `pushd/popd`、`push-location/pop-location`
6. `AnchorPath`
   - 初始锚点
7. `LastCalibratedPath`
   - 最近一次外部校准值
8. `LastInputLine`
   - 最近一次参与 path 推理的输入
9. `LastMutationKind`
   - 最近一次 path 变化原因
10. `HasReliableAnchor`
   - 是否已有有效锚点
11. `SourceKind`
   - 锚点来源：workspace node / startingDirectory / shell-report / unknown
12. `LastPathPerDrive`
   - 每个盘符最近路径，用于 `D:` 这种切盘语义

## 10. path 初始化 anchor

优先级：

1. workspace node 的 `startupDirectory`
2. 创建 tab 时的 `startingDirectory`
3. 可信 `WorkingDirectory()` 上报
4. 否则为空

约束：

1. 没有 anchor 时不能伪造 `C:\`
2. 不能直接退回 profile 默认目录充当当前 path
3. `WorkingDirectory()` 只能辅助校准，不能覆盖整套状态机

## 11. 输入驱动推理流程

每次收到一行输入：

1. 规范化文本
2. 拆分为 line
3. 按 `&&` / `;` 分段
4. 每段独立判断：
   - 是否目录变更语义
   - 是否普通命令语义
5. 若是目录变更语义，则更新 `CurrentPath`
6. 若不是，则保持 `CurrentPath`
7. 全部段处理完成后，`CurrentPath` 即该行后的 path 状态

## 12. 哪些输入应改变 path

### 12.1 必须改变 path 的输入

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
12. `D:`

### 12.2 不应直接改变 path 的输入

1. `D:\tools\run.exe`
2. `.\scripts\build.ps1`
3. `python D:\repo\tool.py`
4. `git -C D:\repo status`
5. `code D:\repo`
6. `C:\Windows\System32\cmd.exe /c dir`

原则：

- 看起来像路径，不等于目录切换

## 13. Windows 路径语义

### 13.1 必须支持的路径形态

1. `D:\repo`
2. `D:/repo`
3. `\\server\share\repo`
4. `.\sub`
5. `..\sub`
6. `\relative-on-current-drive`
7. `"D:\Program Files\repo"`
8. `D:`

### 13.2 归一化要求

1. `/` 统一成 `\`
2. 去掉冗余 `.` 段
3. 正确处理 `..`
4. 保留 UNC 前缀
5. 不依赖文件系统存在性检查

### 13.3 `D:` 的规则

`D:` 不是普通路径字符串，而是 Windows 的切盘语义。

处理规则：

1. 当整段输入仅为 `D:` 时，视为切换当前工作盘
2. 若已记录 `D` 盘最近路径，则切到该路径
3. 若未记录，则退化到 `D:\`
4. 这次输入会影响 `path`
5. 但 `D:` 仍可作为 `cmd` 文本保留，不能让 `cmd` 反过来决定 `path`

## 13A. Linux / POSIX 路径语义

### 13A.1 必须支持的路径形态

1. `/srv/app`
2. `./project`
3. `../project`
4. `../../repo`
5. `~`
6. `~/project`

### 13A.2 归一化要求

1. 保留 `/` 分隔符
2. 去掉冗余 `.` 段
3. 正确处理 `..`
4. 根路径 `/` 不被弹空
5. `~` 若不能可靠展开，允许按字面保留

### 13A.3 `cd -` 规则

1. `cd -` 使用 `PreviousPath`
2. `PreviousPath` 不存在时不改 path

### 13A.4 `~` 与 `~/repo` 规则

1. 有 home 锚点时可展开为绝对路径
2. 没有 home 锚点时允许保留 `~` / `~/repo`
3. 不能随便把 `~` 伪造展开为某个固定目录

### 13A.5 Linux 下路径命令与目录切换的区分

这些是命令，不直接改 path：

1. `./build.sh`
2. `/usr/bin/python3`
3. `../tools/run`
4. `python ./tool.py`
5. `git -C /srv/app status`

这些会直接改 path：

1. `cd ...`
2. `pushd ...`
3. `popd`
4. `cd -`

## 14. PowerShell 细节

首版静态支持：

1. `Set-Location foo`
2. `Set-Location -Path "D:\Program Files\repo"`
3. `Set-Location -LiteralPath \\server\share\repo`
4. `sl ..`
5. `push-location foo`
6. `pop-location`

首版不保证：

1. 变量展开后的真实路径
2. `$(...)`
3. 脚本块内部动态表达式
4. 任意复杂 pipeline 执行语义

策略：

1. 复杂表达式识别失败时，不乱改 path

## 15. 多段命令规则

必须支持：

1. `cd /d D:\repo && git status`
2. `Set-Location .\sub; dir`
3. `pushd project && ls`
4. `D: && cd repo && build.cmd`

处理规则：

1. 顺序切段
2. 每段独立判断是否改 path
3. 改 path 的段立即生效
4. 后续段看到更新后的路径状态
5. 最终保存整行结束后的 `CurrentPath`

## 15A. Linux 本地 shell 细节

### 15A.1 首版必须支持

1. `cd foo`
2. `cd ./foo`
3. `cd ../foo`
4. `cd /srv/app`
5. `cd ~`
6. `cd -`
7. `pushd foo`
8. `popd`

### 15A.2 首版不保证

1. `CDPATH`
2. shell alias 对 `cd` 的重写
3. 复杂 shell function
4. 子 shell 中的目录变化
5. 命令替换或 process substitution

### 15A.3 策略

1. 先支持静态文本参数
2. 不能可靠解析时，不乱改 path
3. Linux 本地 shell 与 ssh 远端 shell 复用 POSIX 语义解析核心

## 16. cmd 推理链路

`cmd` 也要推理，但链路独立于 `path`：

1. workspace node 原始 `startupAction`
2. 显式 commandline override
3. 运行态输入链路中的最近有效命令文本
4. 需要时可结合 `TrackTerminalInput(...).LastCommand`

约束：

1. cmd 推理失败，不得破坏 path 状态
2. path 推理失败，不得篡改 cmd

## 16A. os/shell 推理链路

### 16A.1 目标

对**未保存 tab**推理出：

1. `os`
2. `shell`

### 16A.2 推理输入

可用输入：

1. profile / connection 类型
2. 创建 tab 时的 commandline
3. 运行态输入语法
4. 路径风格

### 16A.3 推理输出约束

1. 输出必须独立于 `path/cmd` 字段落盘
2. 输出结果用于指导 path 规则选择
3. 若 shell/os 推理失败，不得破坏已有 `path/cmd`

## 17. 保存策略

### 17.1 保存 path

优先级：

1. `PathState.CurrentPath`
2. `PathState.AnchorPath`
3. 接口返回的默认路径值（例如 `WorkingDirectory()` 这类当前可取到的默认目录接口值）
4. 空

明确禁止：

1. 从 `cmd` 再反推 path
2. 直接退回 profile 默认目录
3. 直接把 `WorkingDirectory()` 当稳定真值覆盖状态机结果

补充规则：

1. 优先使用推理结果
2. 推理不出来时，再退回接口返回的默认路径值
3. 这个接口值只作为兜底，不参与覆盖已推理出的 path 状态

### 17.2 保存 cmd

优先级：

1. workspace/runtime 原始 `startupAction`
2. 显式 override commandline
3. 最近有效命令文本推理结果
4. 否则为空

### 17.3 保存 os / shell

仅对未保存 tab：

1. `OperatingSystem` 读取 os 推理链路结果
2. `ShellKind` 读取 shell 推理链路结果
3. 推理不出来时允许保留空内部状态继续尝试，但最终落盘应尽量收敛为需求范围内的值

对已保存 tab：

1. 直接保留 node 已保存的 `os/shell`
2. 不做运行态覆盖

## 18. 失败与降级

### 18.1 path 推理失败

1. 保留旧状态
2. 不乱写新路径
3. 记录诊断信息

### 18.2 无 anchor

1. 保持空状态
2. 等待后续绝对路径或明确切盘/切目录语义建立状态

### 18.3 校准冲突

若 `WorkingDirectory()` 与状态机冲突：

1. 当前输入若是明确目录变更命令，优先状态机
2. 长时间无输入且 shell 主动上报新值时，可接受校准
3. 不能静默把状态机完整覆盖掉

## 19. 实现步骤

### 19.1 抽独立 path 状态机

建议新增：

1. `ext\src\chat\WorkspacePathInference.h`
2. `ext\src\chat\WorkspacePathInference.cpp`

职责：

1. 维护 `WorkspacePathState`
2. 解析输入行
3. 更新 path 状态
4. 推理 `os/shell`
5. 提供 `CurrentPath`

### 19.2 接入 TerminalPage

1. 为每个 `contentId` 持有 path 状态
2. 在 `CharSent` / `StringSent` flush 后喂入 path 状态机
3. 在 workspace 打开链路初始化 anchor 与已知 `os/shell`
4. tab 关闭时释放状态

### 19.3 保存切换

`_TryCaptureCurrentWorkspace(...)`：

1. `StartupDirectory` 读取 path 状态链路
2. `StartupAction` 读取 cmd 推理链路
3. `OperatingSystem` / `ShellKind` 读取 os/shell 推理链路
4. 几类值不能互相回退或串写

## 20. 测试矩阵

### 20.1 当前已有 harness 测试

1. `TracksWindowsCdChain`
2. `TracksPowerShellSetLocationChain`
3. `TracksPosixPushdPopd`
4. `UsesReportedWorkingDirectoryAsAnchor`

### 20.2 必须补充的测试

1. `TracksDriveLetterSwitch`
   - `D:`
2. `DistinguishesExecutablePathFromDirectorySwitch`
   - `D:\tools\run.exe`
3. `TracksCdThenCommandAcrossSegments`
   - `cd /d D:\repo && git status`
4. `TracksPowerShellLiteralPath`
5. `TracksDriveScopedPathHistory`
6. `DoesNotCorruptPathWhenCmdContainsPathArgument`
   - `git -C D:\repo status`
7. `TracksLinuxCdDash`
   - `cd -`
8. `TracksLinuxHomePath`
   - `cd ~`
9. `DistinguishesLinuxExecutablePathFromDirectorySwitch`
   - `./build.sh`
10. `TracksLinuxCdThenCommandAcrossSegments`
   - `cd /srv/app && ./build.sh`
11. `DoesNotCorruptLinuxPathWhenCmdContainsPathArgument`
   - `git -C /srv/app status`
12. `InfersWindowsCmdShell`
13. `InfersWindowsPowerShellShell`
14. `InfersSshShell`
15. `InfersLinuxOperatingSystem`
16. `DoesNotLetSinglePathTokenMisclassifyShell`

## 21. 验收标准

满足以下条件才算正确：

1. 每个 tab 的 path 独立维护
2. 每个 tab 的 cmd 独立维护
3. path 与 cmd 都通过推理得到
4. `D:` 能正确改变 path
5. `D:\tools\run.exe` 不会被误当成目录切换
6. `pushd/popd` 栈行为正确
7. ssh 场景不伪装成“已拿到远端真实 cwd”
8. 保存 workspace 时，`path` 只来自 path 状态链路
9. 保存 workspace 时，`cmd` 只来自 cmd 推理链路
10. Linux 本地 shell 下 `cd -`、`cd ~`、`./script`、`git -C` 等场景语义正确
11. 未保存 tab 能额外推理出 `os`
12. 未保存 tab 能额外推理出 `shell`
13. 已保存 tab 的 `os/shell/path/cmd` 都不被运行态覆盖

## 22. 一句话结论

workspace 保存里的 `path`、`cmd`、`os`、`shell` 都要推理，但必须分链路推理；未保存 tab 才参与推理，已保存 tab 不参与任何运行态覆盖。
