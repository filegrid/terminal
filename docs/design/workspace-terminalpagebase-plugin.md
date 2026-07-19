# Workspace `TerminalPageBase` Plugin Host Design

## 目标

- `WorkspaceExtension.dll` 由宿主在运行时动态加载，而不是让 `TerminalApp` 静态链接 `WorkspaceExtension.lib`。
- 插件边界只暴露 `TerminalPageBase` 这类稳定宿主接口和实例指针，不再把 `winrt::TerminalApp::implementation::TerminalPage` 直接暴露给插件。
- `TerminalPage` 私有 workspace glue 留在宿主侧；插件只保留 workspace/chat 的状态与策略。
- portable / full 两条链路都继续产出同一个 `bin` 下可运行的 portable 结果。

## 现状问题

之前的 `WorkspaceExtension.dll` 只做到了“单独编译”，没有做到“真正解耦”：

1. DLL 侧直接 `#include "TerminalPage.h"`。
2. 工厂函数直接接收 `TerminalPage&`。
3. `TerminalApp` 静态链接 `WorkspaceExtension.lib`，运行时没有插件装载边界。
4. 一部分本应属于宿主的 `TerminalPageWorkspaceGlue.cpp` 代码还在 DLL 侧编译。

这样即使二进制名字变成了 DLL，本质上仍然是主工程实现的一部分。

## 目标边界

### 宿主侧

- `TerminalPage` 实现 `terminal::workspace::TerminalPageBase`。
- `TerminalPageBase` 只暴露 workspace 插件确实需要的宿主能力：
  - UI 初始化/刷新
  - 启动输入回放
  - 当前 workspace 标识读取
  - close / split / pane-create 这些宿主决策钩子
- `TerminalPage` 负责：
  - `LoadLibraryExW("WorkspaceExtension.dll")`
  - `GetProcAddress(CreateWorkspaceTerminalPageExtension)`
  - `GetProcAddress(DestroyWorkspaceTerminalPageExtension)`
  - 生命周期释放

### 插件侧

- `WorkspaceExtension.dll` 只实现 `IWorkspaceTerminalPageExtension`。
- 插件构造只接收 `TerminalPageBase*`。
- 插件内部不再引用 `TerminalPage.h`，也不直接触碰 `TerminalPage` 私有成员。

### Host-only glue

- `TerminalPageWorkspaceGlue.cpp` 仍然定义 `TerminalPage` 的成员函数，所以必须回到宿主编译单元。
- 这部分代码仍可通过 `_workspaceExtension` 访问插件状态，但不再被编进插件 DLL。

## ABI 约定

导出统一改成 C 风格符号，避免依赖 C++ 名字修饰：

```cpp
extern "C" WT_WORKSPACE_EXT_API IWorkspaceTerminalPageExtension* WINAPI CreateWorkspaceTerminalPageExtension(TerminalPageBase* host);
extern "C" WT_WORKSPACE_EXT_API void WINAPI DestroyWorkspaceTerminalPageExtension(IWorkspaceTerminalPageExtension* extension);
```

宿主只缓存这两个函数指针和模块句柄。

## 本次落地范围

这次调整先完成真正的宿主/插件装载边界：

1. 引入 `TerminalPageBase` 接口，替换 `TerminalPage&` 过边界。
2. `TerminalPage` 动态加载 `WorkspaceExtension.dll`。
3. 去掉 `TerminalApp` 对 `WorkspaceExtension.lib` 的静态链接。
4. 把 `TerminalPageWorkspaceGlue.cpp` 从 DLL 编译单元移回宿主。
5. 把 DLL 侧对 `TerminalPage.h` 的直接包含去掉。

## 后续继续收敛

这次之后，插件已经不再直接依赖 `TerminalPage` 实现类和静态 import lib。后续还可以继续收敛：

1. 继续缩小 `TerminalPageBase` 的方法面，按 capability 拆分。
2. 把 workspace 的 load/save/startup/runtime 校验等**业务逻辑**收敛为 ext 提供的接口，原生侧只负责 UI 和生命周期接线。
3. 让 `WorkspaceModel` / `WorkspaceApi` 退回稳定数据结构与接口层，不再继续承载宿主侧共享核心实现。
4. 让 ext 不再出现任何原项目实现内容，只保留业务逻辑、状态与接口。
5. 清理 DLL 侧不再需要的 `TerminalApp` include / link 依赖。
6. 在 ext-only build 中进一步剥离不必要的大库链接，继续压缩 compile/link 时间。

## 2026-07 新分层修正

`ext` 现在进一步明确拆成两层，而不是要求整个 `ext` 都完全脱离宿主：

1. **底层接口/核心层**
   - 独立 DLL。
   - 纯 C++。
   - 不依赖 WinRT、XAML、`TerminalPage`、`TerminalSettingsModel`、`TerminalApp`。
   - 负责 workspace 的数据结构、持久化、校验、启动规划、运行态规则等业务逻辑。
2. **胶水层**
   - 负责把 core DLL 接到现有 Terminal/Microsoft 宿主接口上。
   - 可以依赖 WinRT 和现有宿主类型。
   - 属于 `full` 的一部分。
   - 只做适配、转换、生命周期转发，不再承载业务决策。

这样拆的目标是：**业务逻辑不回流到旧的 host/main 分支实现里，但 glue 也不必假装没有宿主依赖。**

## 额外修正：不要把 UI 放进 ext

最近的 workspace 边界修正再补一个明确约束：

1. ext 可以提供 workspace 相关的业务逻辑接口。
2. ext 不应该继续持有或扩展页面级 UI 语义。
3. ext 也不应该保留任何原项目侧页面实现、宿主 glue、兼容壳或原生工程实现细节。
4. 工作区管理页、编辑交互、tab row 交互、焦点/显示等页面行为应留在原生宿主。
5. ext 应该返回业务状态、校验结果、启动动作、持久化结果，而不是定义 UI 组件行为本身。
