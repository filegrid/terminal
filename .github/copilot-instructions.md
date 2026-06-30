## 目标
构建目标是 portable 版本
只构建 portable 项目
目标产物只认 `bin` 目录下的 portable 输出
## 入口

- 后续新增内容入口：
  - 新增文档放 `ext\docs` 目录
  - 新增代码放 `ext\src` 目录
  - 所有变更的功能记录到 ext\READ
- 所有的脚本，备份，需要当前项目的 `microsoft\tmp` 目录下进行 
- portable 构建使用：

  ```powershell
  powershell -NoProfile -ExecutionPolicy Bypass -File .\microsoft\build\scripts\Build-PortableTerminalDistribution.ps1
  ```

## 能做

- 必须对构建产物进行功能性验证


## 不能做

- 不要把 `tools\razzle.cmd`、`bcz`、`bx` 这套通用构建链写进 instruction，避免把 portable 目标带偏。
- 不要直接裸跑 `msbuild`，除非已经确认完整工具链状态并且有明确理由。
- 不要把 `wt.exe`、`OpenConsole.exe`、MSIX 包产物、或 `bin\...` 下普通调试输出当成 portable 正确性的依据。
- 不要拦截 SID-500 内置管理员账号的 portable 或 unpackaged 启动路径；如果 agent/CLI/PowerShell 代理环境里复现启动失败，必须先用最终 `bin` 产物按真实用户手动启动路径验证，确认是产物问题后才能改启动逻辑。
- 不要在 instruction 里写运行时细节、业务行为或验收过程的展开说明；这类内容放文档，不放 instruction。
