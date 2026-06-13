# ext 目录说明

## portable 构建规则

这个仓库后续只认 portable 构建链，不要再混入通用构建链。

1. 只构建 portable 项目
2. 目标产物只认 `bin\WindowsTerminalDev_0.0.1.0_x64.exe`
3. 不要把 `tools\razzle.cmd`、`bcz`、`bx` 这套通用构建链写进 instruction，也不要拿它们的产物做 portable 验收

## ext 目录约定

1. 文档放 `ext\docs`
2. 代码放 `ext\src`
