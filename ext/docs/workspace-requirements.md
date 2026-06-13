# Workspace 需求文档

## 背景

需要在当前 Windows Terminal 基础上增加 workspace 能力，用来组织一组可重复打开的连接实例，同时尽量保持现有终端逻辑不变。

这里提到的 settings，明确是指当前 Windows Terminal 使用的 `settings.json`。

## 核心需求

### 1. workspace 定义

- 支持 workspace
- 一个 workspace 包含多个连接实例
- workspace 定义存储在用户目录下的 `.wt\workspaces.yaml`

### 2. 与现有 settings 的关系

- 继续使用当前 Windows Terminal 的 `settings.json`
- workspace 不是替换 `settings.json`，也不是第二套基础配置
- 讨论中提到的 `peer / server / resource` 只是帮助理解的概念，不要求按这个字面结构实现

### 3. 节点能力

每个 workspace node 至少需要支持：

- 启动目录
- 启动命令/脚本（这是一个概念，不拆成两个字段）

### 4. profile 风格复用

- node 里需要有一个字段能对应 `settings.json` 的 `profiles.list[*].guid`
- 这样 node 可以复用该 profile 的视觉风格
- 至少包括：
  - 颜色
  - icon
  - 其他 tab / title 相关风格

### 5. 编辑交互

- workspace 节点默认是浏览态
- 只有进入编辑模式后才能增删节点
- 平时不显示 `+`
- 平时不显示 `x`

### 6. 启动行为

- 第一个启动的终端窗口默认恢复最近打开的 workspace
- 后续新开的窗口默认打开空 workspace
- 空 workspace 就是现在的默认模式
- 空 workspace 可以保存为正式 workspace
- workspace 菜单里提供“新窗口打开 workspace”选项，默认选中；选中时在新的 workspace 窗口中打开，取消选中时替换当前窗口内容

## 约束

### 1. 保持现有终端逻辑一致

- workspace 需要尽量保持和当前终端逻辑一致
- 要继续复用现有设置、现有 profile 风格、现有启动方式

### 2. 配置职责分离

- `settings.json` 继续承担现有终端配置职责
- `workspaces.yaml` 只承担 workspace 编排职责

## 第一阶段目标

第一阶段至少覆盖：

1. 定义 `workspaces.yaml` 的基本结构
2. 支持 workspace -> node 的组织关系
3. node 支持启动目录
4. node 支持启动命令/脚本
5. node 支持 profile guid 映射
6. 支持浏览态 / 编辑态切换
7. 支持首窗口恢复最近 workspace
8. 支持后续窗口默认空 workspace
9. 支持空 workspace 保存为正式 workspace
