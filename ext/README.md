# Portable Build Notes

`ext\README.md` 只保留 portable 构建相关说明；功能说明放仓库根目录 `README.md` / `README-cn.md`。

## Repository entry rules

1. 新增文档放 `ext\docs` 目录。
2. 新增代码放 `ext\src` 目录。
3. 需要保留的脚本、备份放 `microsoft\tmp` 目录。

## Build target

1. 只构建 portable 项目。
2. 最终只认 `bin\` 目录下的 portable 输出。

## Build command

```powershell
cmake -S . -B .\build
cmake --build .\build
```

默认无参构建就是 `ext` 增量路径；如果需要全链路重编，使用：

```powershell
cmake --build .\build --target full
```

## CMake entrypoint

默认 portable 构建入口就是下面这个 **真正走 `build` 目录** 的 CMake 入口。这个入口直接驱动：

1. `build` 目录下的 CMake 构建树
2. 现有 VC/MSBuild 工程的编译
3. `ext\src\glue\portable\PortablePackageTool` 的便携打包

不会在 CMake 路径里再回调 `Build-PortableTerminalDistribution.ps1`。

可通过 CMake cache 变量覆盖常用参数，例如平台：

```powershell
cmake -S . -B .\build -DPORTABLE_PLATFORM=arm64
```

## Legacy compatibility entrypoint

旧 PowerShell portable 脚本入口仍然保留，但它不再是默认构建方式，只用于兼容旧调用面或对比排障：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\microsoft\build\scripts\Build-PortableTerminalDistribution.ps1
```

## Output expectation

1. 便携版构建产物应位于 `bin\` 目录。
2. 发布产物名需要带 `GeekEdition`，当前只生成 `WindowsTerminalPortableGeekEdition_System_<version>_<arch>.exe`，并跟随系统语言。
3. `wt.exe`、`OpenConsole.exe`、MSIX 包产物以及普通调试输出都不能作为 portable 正确性的依据。
4. 需要基于最终 `bin\` 产物做功能性验证。
5. 如果 agent/CLI/PowerShell 代理环境里复现启动失败，必须先用最终 `bin\` 产物按真实用户路径手动验证，确认是产物问题后再改启动逻辑。

## Recent functional changes

1. Workspace SSH 启动回放现在会识别 `ssh -t`。当目标 node 标记为 Windows 时，会先等最终 Windows shell 报出对应工作目录，再回放 `startupDirectory` / `startupAction`，避免输入误打到跳板机那层 SSH。

## ext src layout

`ext\src` 现在按两层目录组织：

1. `ext\src\core`：纯 C++ core / interface 代码，不依赖 Terminal 宿主实现。
2. `ext\src\glue`：和 Terminal / WinRT / packaging 接口耦合的 glue 代码。

## Layer rules

这是入口约束，默认按这个执行：

1. 新增逻辑默认进 `ext`
2. 业务逻辑默认进 `ext\src\core`
3. `glue` 越薄越好，只做接口适配和接线
4. `microsoft\src\...` 只保留薄接线

具体要求：

1. 持久化规则、状态规则、校验规则、启动规划、运行态判定，默认放 `ext\src\core`
2. Terminal/WinRT/UI 对象适配、事件接线、参数转发、运行时日志落点，放 `ext\src\glue`
3. 原始树只允许保留最小 hook、注册、include、接口暴露、构建挂接

禁止直接按“哪里改起来快就改哪里”的方式落代码。
新增 `glue` 层业务逻辑、状态逻辑、定时任务、共享内存读写、持久化分支前，必须先人工确认。

## Required check

开始改代码前，必须先判断这次改动属于哪一层：

1. `core`
2. `glue`
3. 原始树薄接线

如果只是为了排障，允许临时把日志放进 `glue`；但业务规则、状态机、启动/状态判定不应长期留在 `glue`，修复完成后必须回收到 `core`。

## Glue approval

下面几类改动，默认不允许直接落到 `glue`：

1. 业务规则
2. 运行态状态判断
3. 定时任务调度策略
4. 共享内存读写
5. 持久化/兼容分支

确实只能先落 `glue` 时，必须先得到人工确认，并补一份后续回收计划。

## Instruction guardrails

1. 不要把 `tools\razzle.cmd`、`bcz`、`bx` 这套通用构建链写进 instruction，避免把 portable 目标带偏。
2. 不要直接裸跑 `msbuild`，除非已经确认完整工具链状态并且有明确理由。
3. 不要在 instruction 里写运行时细节、业务行为或验收过程的展开说明；这类内容放文档，不放 instruction。
