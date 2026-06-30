# Portable Build Notes

`ext\README.md` 只保留 portable 构建相关说明；功能说明放仓库根目录 `README.md` / `README-cn.md`。

## Build target

1. 只构建 portable 项目。
2. 最终只认 `bin\` 目录下的 portable 输出。

## Build command

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\microsoft\build\scripts\Build-PortableTerminalDistribution.ps1
```

## Output expectation

1. 便携版构建产物应位于 `bin\` 目录。
2. 发布产物名需要带 `GeekEdition`，并同时生成两份：`WindowsTerminalPortableGeekEdition_System_<version>_<arch>.exe` 跟随系统语言，`WindowsTerminalPortableGeekEdition_English_<version>_<arch>.exe` 固定英文界面。
3. `wt.exe`、`OpenConsole.exe`、MSIX 包产物以及普通调试输出都不能作为 portable 正确性的依据。
4. 需要基于最终 `bin\` 产物做功能性验证。
