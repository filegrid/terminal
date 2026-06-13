## 目标
构建目标是 portable 版本
## 入口

- 后续新增内容入口：
  - 新增文档放 `ext\docs` 目录
  - 新增代码放 `ext\src` 目录
- 所有的脚本，备份，需要当前项目的tmp目录下进行 
- 优先使用仓库入口命令：

  ```cmd
  tools\razzle.cmd
  bcz
  ```

- 单项目构建使用：

  ```cmd
  tools\razzle.cmd
  bx
  ```

- portable 构建使用：

  ```powershell
  powershell -NoProfile -ExecutionPolicy Bypass -File .\build\scripts\Build-PortableTerminalDistribution.ps1
  ```

## 能做

- 必须对构建产物进行功能性验证


## 不能做

- 不要直接裸跑 `msbuild`，除非已经确认完整工具链状态并且有明确理由。
- 不要把 `wt.exe`、`OpenConsole.exe`、或 `bin\...` 下普通调试输出当成 portable 正确性的依据。
- 不要在 instruction 里写运行时细节、业务行为或验收过程的展开说明；这类内容放文档，不放 instruction。
