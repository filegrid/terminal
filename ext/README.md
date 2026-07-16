# Portable Build Notes

`ext\README.md` 只保留 portable 构建相关说明；功能说明放仓库根目录 `README.md` / `README-cn.md`。

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
3. `ext\src\portable\PortablePackageTool` 的便携打包

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

## Recent functional changes

1. Workspace SSH 启动回放现在会识别 `ssh -t`。当目标 node 标记为 Windows 时，会先等最终 Windows shell 报出对应工作目录，再回放 `startupDirectory` / `startupAction`，避免输入误打到跳板机那层 SSH。
