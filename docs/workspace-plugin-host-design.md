# Workspace 插件化接入 `TerminalPage` 设计方案

## 背景

当前 workspace/chat 相关逻辑虽然已经尽量收敛到 `ext\src`，但仍然存在一个关键问题：

1. `ext` 里的一部分实现还是通过 `TerminalPage.cpp` 的 include glue 方式接入。
2. 这会让 `ext` 改动继续穿透到 `TerminalPage.cpp` 这个大编译单元。
3. 结构上也还没有形成真正的“宿主 + 插件”边界。

这次的目标不是马上改代码，而是先把后续改造的边界定死，避免继续把 workspace 当成 `TerminalPage` 的内部实现片段。

## 硬约束

后续 workspace 插件化必须满足下面几个约束：

1. **workspace/ext 代码不能直接访问 `TerminalPage` 内部细节。**
2. **workspace/ext 代码只能依赖宿主/基类接口，不能依赖 `TerminalPage` 具体实现类。**
3. **ext 不出现任何原项目实现内容；它只提供 workspace 的业务逻辑、状态和接口。**
4. **`WorkspaceModel` / `WorkspaceApi` 只保留稳定数据结构和接口声明，不承载会频繁变化的核心业务实现。**
5. **`TerminalPage.cpp` 里只保留注册、转发、最薄的桥接入口。**
6. **不再继续扩散 `#include "..\\..\\..\\ext\\src\\...\\*.cpp"` 这种把实现文本并入 `TerminalPage.cpp` 的方式。**

这里的“内部细节”包括但不限于：

- `TerminalPage` 私有成员变量
- `TerminalPage` 私有 helper
- 只能在 `TerminalPage.cpp` 里成立的局部状态约定
- 插件通过 include 方式间接拿到的匿名 namespace 内容

## 当前问题

当前做法的主要问题有两类。

### 1. 编译问题

`ext\src\chat\WorkspaceChatDispatchGlue.cpp` 这类实现目前仍通过 `TerminalPageWorkspaceGlue.cpp` 间接并入 `TerminalPage.cpp`。

结果是：

1. 改的是 `ext`
2. 但实际重编的是 `TerminalPage.cpp`
3. 最终仍然触发大的 app/project 编译链

所以即使逻辑文件“物理上”放进了 `ext`，编译边界仍然没有真正切开。

### 2. 架构问题

如果 workspace 逻辑需要直接读取 `TerminalPage` 私有状态、调用私有流程，或者默认共享它的实现上下文，那它本质上仍然是：

> `TerminalPage` 的一块外置源码片段，而不是插件。

这会带来几个问题：

1. 边界不清晰，后续很容易继续扩散耦合。
2. `TerminalPage` 难以继续向 `main` 靠拢。
3. workspace 代码难以单独演进、测试和替换。

## 目标形态

目标是把 workspace 变成一个**通过宿主接口注册到 `TerminalPage` 的扩展模块**。

### 宿主视角

`TerminalPage` 只负责：

1. 创建 workspace 扩展实例
2. 提供一个受控的宿主接口对象
3. 在生命周期节点把事件转发给扩展
4. 在必要位置承载少量 UI/对象接线

### 插件视角

workspace 扩展只负责：

1. 管理 workspace/chat 自己的业务状态和规则
2. 通过宿主接口请求终端能力
3. 提供 workspace 的持久化、启动解析、运行态同步、合法性校验等业务逻辑
4. 不承载原生项目的页面实现、XAML/UI 组件、页面交互或宿主视觉状态
5. 不假设宿主的私有实现细节

## 2026-07 边界修正

这次排查 `ceshi` 错误加载与崩溃时，已经确认当前结构还有一层关键偏差：

1. `microsoft\src\cascadia\TerminalSettingsModel\Workspace.cpp` 仍然直接 `#include "../../../../ext/src/workspace/WorkspaceModel.cpp"`。
2. 结果是 `WorkspaceModel` 虽然名字上像“模型/基类”，实际上仍然承载了 workspace 的核心实现。
3. 原生侧还会直接调用 `WorkspaceManager::Load()`、`WorkspaceStateManager::Load()`、`BuildStartupActions()` 这类业务逻辑，而不是只拿对象/接口。
4. 这会让 ext 侧业务规则继续通过共享实现或 include glue 穿透回宿主，导致“只改 ext 但宿主仍然吃共享/旧逻辑”的问题反复出现。

因此，后续收敛的明确方向是：

1. **`WorkspaceModel` 退回稳定 API/数据结构层。**
2. **workspace 的核心 load/save/startup/validation/runtime 逻辑收敛到 ext 提供的业务接口。**
3. **原生侧只调用这些业务接口，并负责 UI、页面生命周期和用户交互。**
4. **ext 目录里不再放原项目侧的页面实现、宿主 glue 或兼容性实现内容。**
5. **任何 root-level `.wt` 兼容/迁移规则，都不能再通过宿主内嵌实现偷偷生效。**

## 2026-07 新分层修正

后续 `ext` 不再追求“整个 ext 都与 Terminal 彻底无关”，而是明确拆成两层：

1. **底层接口/核心层**
   - 产物是独立 DLL。
   - 只使用纯 C++。
   - 不依赖 WinRT、XAML、`TerminalPage`、`TerminalSettingsModel`、`TerminalApp` 这套宿主类型。
   - 只承载 workspace 的数据结构、持久化、校验、启动输入拼装、运行态规则等业务逻辑。
2. **胶水层**
   - 负责把底层核心层接到现有 Terminal/Microsoft 宿主接口上。
   - 可以依赖 WinRT 和宿主类型，但只做适配、转换、生命周期转发。
   - 属于 `full` 的一部分，不允许把业务逻辑重新塞回旧的 `main` 分支宿主逻辑里。

这意味着：

1. `WorkspaceModel` / `WorkspaceApi` 上层继续保留对现有宿主的兼容入口，但它们应该逐步退化成 glue facade。
2. 真正可复用、可独立构建、可独立测试的 workspace 规则应下沉到纯 C++ core DLL。
3. 宿主侧和 WinRT 侧看到的只是 core 的结果，不再让 core 反向 include `TerminalApp` / `Settings.Model` / generated winrt 头。

## 总体结构

建议引入下面这组边界。

### 1. 宿主接口

定义一个类似 `IWorkspaceTerminalPageHost` 的宿主接口，放在 `ext\src\workspace` 或独立的共享头里。

它只暴露 workspace 真正需要的能力，而不是把整个 `TerminalPage` 暴露出去。

接口能力建议按类别拆分：

#### A. 终端上下文能力

- 获取当前活动 tab/pane/control
- 按 routing key / tab id 查找目标终端上下文
- 获取当前 workspace key / 当前窗口关联 workspace

#### B. 终端输入输出能力

- 向指定 control 发送输入
- 查询 `BracketedPasteEnabled`
- 订阅/转发终端输入输出事件
- 请求执行“发送文本”“发送提交键”这类标准宿主动作

#### C. UI 宿主能力

- 获取 workspace 输入框 / 容器 / 标题栏挂点
- 请求刷新布局
- 请求切换聊天面板显示状态
- 请求焦点切换

#### D. 生命周期与状态能力

- 窗口创建完成
- tab/pane 创建完成
- 连接状态变化
- 设置重载
- 窗口关闭前确认

#### E. 持久化和诊断能力

- 获取 `.wt` 路径相关能力
- 写 workspace 诊断日志
- 获取当前窗口/标签页/窗格标识

### 2. 插件接口

定义一个类似 `IWorkspaceExtension` 的扩展接口。

`TerminalPage` 只认识这个接口，不认识 workspace 具体实现类。

建议最小接口面包括：

- `Initialize(host)`
- `OnCreateCompleted()`
- `OnTabChanged(...)`
- `OnConnectionStateChanged(...)`
- `OnSettingsReloaded()`
- `OnWindowClosing(...)`

按这次修正后的边界，扩展接口应优先按**业务能力**拆，而不是按 UI 控件拆。例如：

- `IWorkspacePersistenceService`
- `IWorkspaceLaunchService`
- `IWorkspaceRuntimeStateService`
- `IWorkspaceValidationService`

如果第一步不想拆太细，也至少要保证总接口只暴露业务动作，不暴露 XAML/UI 细节。

如果某些接口太大，可以继续细分为：

- `IWorkspaceChatExtension`
- `IWorkspacePersistenceExtension`
- `IWorkspaceRuntimeExtension`

这里不再建议继续保留 `IWorkspaceUiExtension` 这类把 UI 行为塞进 ext 的方向；UI 行为应该留在宿主，ext 只返回状态/决策结果。

### 3. 注册入口

`TerminalPage.cpp` 中只保留一个非常薄的注册点，例如：

1. 创建 `TerminalPageHostAdapter`
2. 调用 `CreateWorkspaceExtension(host)`
3. 保存为 `std::unique_ptr<IWorkspaceExtension>`

后续 `TerminalPage` 只在少数生命周期节点把事件转发过去。

## 关键设计原则

### 原则 1：插件拿到的是能力，不是实现

插件看到的应该是：

- “帮我拿当前活动 control”
- “帮我把这段文本发给某个 control”
- “告诉我当前 tab/pane 的上下文”

而不是：

- 直接读 `TerminalPage::_xxx`
- 直接改 `TerminalPage::_yyy`
- 假设某个 XAML 元素一定保存在某个私有成员里

### 原则 2：接口要收敛成稳定语义

宿主接口暴露的应该是稳定语义，而不是临时搬运现有私有 helper 名字。

例如优先暴露：

- `RequestChatSubmit(...)`
- `ResolveTerminalContext(...)`
- `SetWorkspaceChatCollapsed(...)`

而不是把当前 `TerminalPage` 内部零散 helper 原样抄成接口。

### 原则 3：UI 挂点可以保留，业务逻辑不能内嵌

`TerminalPage` 作为页面宿主，仍然不可避免要承载一些 UI 组件接线，例如：

- XAML 命名元素
- routed event 入口
- 页面生命周期

但这些入口之后的业务逻辑应该尽快转给 workspace 扩展处理。

进一步说：

1. ext 不应该 new / 持有页面级 UI 组件作为自己的长期职责。
2. ext 不应该定义 workspace 管理页面、编辑交互、tab row 交互、焦点/显示等宿主 UI 细节。
3. ext 只应该给出“当前 workspace 是否锁定”“这组 tab/node 是否合法”“应如何生成 startup actions”“应如何持久化/迁移”等业务答案。

### 原则 4：先抽接口，再迁实现

不能先把代码“硬搬”出去，再让插件继续通过 include 或友元式访问内部细节活着。

正确顺序应该是：

1. 先识别 workspace 当前真正依赖的宿主能力
2. 抽出宿主接口
3. 用宿主适配器承接原有能力
4. 再把 ext 实现从 include glue 改成独立编译单元

### 原则 5：先收回 `WorkspaceModel` 的实现职责

在继续拆 UI glue 之前，先把当前这条错误共享实现链收回来：

1. `Workspace.cpp` 不再作为宿主侧核心业务实现入口。
2. `WorkspaceModel` / `WorkspaceApi` 只保留稳定类型和接口声明。
3. 宿主原先直接调用的 `WorkspaceManager::Load/Save/BuildStartupActions`、`WorkspaceStateManager::Load/Save` 等逻辑，逐步改为通过 ext 的业务接口调用。
4. 当这一步完成后，再删掉仍然依赖共享实现的 host-side include glue。

## 建议文件结构

建议目标结构类似：

```text
ext\src\workspace\
  WorkspaceExtension.h
  WorkspaceExtension.cpp
  WorkspaceHostInterfaces.h
  TerminalPageHostAdapter.h
  TerminalPageHostAdapter.cpp
  WorkspaceChatController.cpp
  WorkspacePersistenceController.cpp
  WorkspaceUiController.cpp
```

原始树侧尽量只保留：

```text
microsoft\src\cascadia\TerminalApp\
  TerminalPage.cpp
  TerminalPage.h
```

其中：

1. `TerminalPage.cpp/.h` 只保留注册点、接口适配点和少量 UI 入口
2. workspace 真正实现放在 `ext\src\workspace`
3. 不再通过 `Glue.cpp` 把大段实现 include 回 `TerminalPage.cpp`

## 推荐改造步骤

### 第一步：列出当前 workspace 对 `TerminalPage` 的直接依赖

把现有 workspace/chat 逻辑按“需要宿主能力”的角度重新盘一遍，形成依赖清单，例如：

1. 需要哪些当前 tab/pane/control 查询
2. 需要哪些 UI 元素访问
3. 需要哪些发送输入/聚焦/激活能力
4. 需要哪些窗口生命周期回调

这一步的输出不是改代码，而是明确接口面。

### 第二步：定义宿主接口

把上一步的直接依赖抽成 `IWorkspaceTerminalPageHost` 一类接口。

要求：

1. 只暴露 workspace 真实需要的能力
2. 不暴露 `TerminalPage` 私有实现
3. 不把“为了兼容当前代码”而存在的内部状态直接泄露出去

### 第三步：实现宿主适配器

在 `TerminalPage` 一侧增加一个适配器对象，用它把现有页面能力映射到宿主接口。

这一步之后，workspace 代码对 `TerminalPage` 的依赖应该变成：

> 只依赖接口头，不依赖 `TerminalPage` 类定义细节。

### 第四步：把 include glue 改成正常编译单元

这是这轮最关键的编译边界切断点。

目标是：

1. `WorkspaceChatDispatchGlue.cpp` 之类不再通过 include 方式并入 `TerminalPage.cpp`
2. workspace 扩展实现作为独立 `.cpp` 编译
3. `TerminalPage.cpp` 只调用扩展接口

### 第五步：收缩 `TerminalPage` 中残留的 workspace 状态

把仍然只服务于 workspace/chat 的状态继续收进扩展内部。

`TerminalPage` 中只保留：

1. 宿主适配所必需的对象
2. 页面级 UI 对象引用
3. 极少量页面生命周期桥接状态

## 预期收益

### 1. 编译收益

当 workspace 实现成为独立编译单元后：

1. 改 `ext\src\workspace` 的实现，不再必然重编 `TerminalPage.cpp`
2. 小范围功能迭代会更接近真实增量编译

这不能消灭 portable 全量打包成本，但能先解决“`ext` 小改也总是打穿大 cpp”的问题。

### 2. 架构收益

1. `TerminalPage.cpp` 更容易继续贴近 `main`
2. workspace 真正成为 fork 扩展层
3. 审查边界会更清晰：凡是插件代码直接碰宿主内部细节，都算越界

### 3. 演进收益

后续无论继续拆 chat、workspace save、workspace runtime，还是进一步做多扩展模块，都会更稳。

## 非目标

这份方案当前**不直接解决**下面这些问题：

1. portable 全构建链本身较重的问题
2. workspace chat submit 具体采用哪条发送策略的问题
3. 是否进一步把 `Workspace.cpp` / `AppLogic.cpp` 也做同等级插件化的问题

这些可以在宿主接口边界稳定后再继续推进。

## 验收标准

后续按这个方案开始改造时，至少要满足下面几点，才能算“插件化成立”：

1. `ext` workspace 实现文件不再通过 `#include *.cpp` 并入 `TerminalPage.cpp`
2. workspace 实现不再直接访问 `TerminalPage` 私有成员/私有 helper
3. `TerminalPage` 对 workspace 的认知只剩接口和注册入口
4. `ext` 小改不会默认打穿 `TerminalPage.cpp` 这个大实现单元

## 当前结论

“把 workspace 作为插件注册进 `TerminalPage.cpp`”这个方向是对的，**但前提不是多加一个注册函数，而是先把宿主/插件接口边界做实**。

否则就只是：

> 形式上叫插件，实际上还是 `TerminalPage` 内部实现的外置源码片段。
