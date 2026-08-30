# Workspace 聊天窗口设计文档

## 背景

当前 workspace 已经承担“组织终端会话”的职责，下一步需要在主窗体下方补一个真正可用的聊天窗口，让聊天、终端输入输出、workspace 上下文形成同一套工作流，而不是分散在外部工具里。

这次先落设计，不直接改现有运行逻辑。

## 目标

1. 在终端主内容区下方提供一个可收起、可拉伸的聊天窗口。
2. 聊天窗口按 workspace 维度隔离上下文、草稿、历史记录和检索数据。
3. 聊天记录和终端屏幕输出都落盘到 workspace 对应目录，并按日期切分，后续可直接做检索和时间线。
4. 为后续语音输入、右侧检索面板、聊天和终端互相回填打基础。

## 一期范围

### 1. 交互

- 聊天窗口固定放在主窗体下方，默认跟随 workspace 打开。
- 输入框支持多行编辑：
  - `Enter` 插入换行
  - `Ctrl+Enter` 发送到主窗体的聊天控制器
  - `Ctrl+C` / `Ctrl+V` 走标准剪贴板
- 输入中的未发送内容按 workspace 缓存；窗口关闭、切换 workspace、应用重启后都能恢复。
- 聊天消息列表至少支持：
  - 用户消息
  - 系统消息
  - 后续 AI/助手返回消息

### 2. 持久化

- 所有聊天消息写入 workspace 目录，按日期分文件。
- 所有终端输入输出摘要也写入同一 workspace 目录，按日期分文件。
- 聊天和终端事件都带统一关联字段，后续能从聊天定位到对应终端输入，也能从终端输入回看对应聊天上下文。

### 3. 为后续检索保留的数据

- 每条聊天消息保留时间戳、发送者、正文、窗口/标签页/窗格关联信息。
- 每条终端事件保留时间戳、tab/pane 标识、输入命令、输出片段、关联聊天消息标识。
- 额外生成 5 分钟聚合段，用于右侧时间线和关键词检索。

## 后续范围

### 1. 语音输入

- 语音转文字结果直接进入聊天输入框，而不是绕过输入框直接发送。
- 保留原始语音片段引用，便于后续重放和纠错。

### 2. 右侧检索面板

- 支持按关键词和时间线检索。
- 5 分钟内聊天内容和终端输出按时间段聚合展示。
- 聚合结果支持：
  - 发送到聊天窗口
  - 发送到主窗体
  - 定位到对应的终端输入

## 存储设计

### 1. workspace 数据目录

建议在 `%USERPROFILE%\.wt` 下给每个 workspace 建独立目录：

```text
%USERPROFILE%\.wt\
  workspaces.yaml
  workspaces\
    <workspace-id>\
      <tab-name>\
        chat\
          2026-06-25.jsonl
        terminal\
          2026-06-25.jsonl
        drafts\
          active.json
```

说明：

- `workspaces.yaml` 继续只保存 workspace 定义。
- `workspaces\<workspace-id>\<tab-name>\` 保存该 tab 的聊天、终端事件和草稿数据。
- 日期文件统一使用本地日期命名，便于人工查看和按天清理。
- 窗口级运行态仍沿用现有 `workspace-window-state-<exe>-<hash>.yaml`，聊天历史和检索数据不要混进去。

### 2. 聊天日志格式

`chat\YYYY-MM-DD.jsonl` 建议按 append-only JSONL 写入，每行一条消息事件，例如：

```json
{"ts":"2026-06-25T18:20:31.221+08:00","workspaceId":"ws-dev","windowId":3,"messageId":"chat-000001","role":"user","text":"帮我总结刚才的错误输出","replyTo":"","correlationId":"corr-9f2d"}
```

建议字段：

- `ts`
- `workspaceId`
- `windowId`
- `messageId`
- `role`
- `text`
- `replyTo`
- `correlationId`

### 3. 终端日志格式

`terminal\YYYY-MM-DD.jsonl` 记录与检索相关的终端输入输出事件，不追求逐字符完整录屏，优先记录“可检索、可回链”的结构化内容，例如：

```json
{"ts":"2026-06-25T18:21:02.110+08:00","workspaceId":"ws-dev","tabId":"tab-2","paneId":"pane-1","eventId":"term-000014","kind":"input","text":"git status","cmd":"git status","cwd":"D:\\repo","correlationId":"corr-9f2d"}
{"ts":"2026-06-25T18:21:03.884+08:00","workspaceId":"ws-dev","tabId":"tab-2","paneId":"pane-1","eventId":"term-000015","kind":"output","text":"On branch filegrid ...","cmd":"git status","cwd":"D:\\repo","correlationId":"corr-9f2d"}
```

建议：

- `input` 事件在真正把命令送进终端时按**单行**落盘。
- `output` 事件按块聚合，而不是每次 buffer 微小变化都写文件。
- `cwd/cmd` 可以基于当前已知工作目录和常见 `cd`/`pushd`/`popd`/`Set-Location` 一类命令做 harness 推演，但不要把它当成绝对真实 shell 状态。
- 当一次聊天触发了终端动作时，共用同一个 `correlationId`。

## 与现有架构的结合点

### 1. UI 宿主

- `src\cascadia\TerminalApp\TerminalPage.xaml`
- `src\cascadia\TerminalApp\TerminalPage.cpp`

这里负责把主页面布局改成“终端区 + 底部聊天区”。聊天区建议作为 `TerminalPage` 的一个 dock 区域，而不是新开一个独立窗口。

### 2. workspace 关联

- `src\cascadia\TerminalSettingsModel\Workspace.h`
- `src\cascadia\TerminalSettingsModel\Workspace.cpp`

一期不要求改 workspace 基础定义；先直接用现有 `workspace.Id` 作为聊天与检索目录的归属键。

如果后续允许重命名 workspace id，再补目录迁移逻辑。

### 3. 运行态

- 继续让 app 级运行态只保存“当前窗口打开的是哪个 workspace、当前聊天草稿指针、最后一次选中的检索时间段”这类轻量状态。
- 大体积聊天和终端历史不要写进现有 `workspace-window-state-<exe>-<hash>.yaml`，避免状态文件失控。

### 4. 新增模块落点

按当前约束，新增聊天相关代码优先放在 `ext\src`，主工程只保留桥接和接线：

- `ext\src\chat\WorkspaceChatController.*`
- `ext\src\chat\WorkspaceChatStore.*`
- `ext\src\chat\TerminalEventStore.*`
- `ext\src\chat\TimelineIndexBuilder.*`

`src\cascadia\TerminalApp` 只负责：

- 承载 UI
- 转发快捷键和发送动作
- 把当前 workspace、tab、pane 上下文交给聊天模块

## 一期实现建议

1. 先把底部聊天窗口和多行输入框接进 `TerminalPage`。
2. 先完成 `Enter` / `Ctrl+Enter` / `Ctrl+C` / `Ctrl+V` 和草稿恢复。
3. 同步接入 workspace 目录写盘，先落 `chat` 和 `terminal` 两类原始日志。
4. 在原始日志稳定后，再补 `timeline` 5 分钟聚合。
5. 右侧检索和语音输入放到下一阶段，不和一期耦合。

## 非目标

- 一期不做复杂富文本编辑器。
- 一期不做跨 workspace 的全局检索。
- 一期不做日志云同步。
- 一期不把终端完整屏幕录制成视频或逐帧快照，只保留检索所需结构化输出。
