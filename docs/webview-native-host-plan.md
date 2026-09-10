# 节点 WebView 原生 Composition 宿主设计

## 目的

工作区的网页命令不使用 `Microsoft.UI.Xaml.Controls::WebView2`。正式路径通过 WebView2 原生 COM API 创建 `ICoreWebView2CompositionController`，并把其根 visual 放入布局 surface。

浏览器不是 XAML 控件：XAML 仅提供 composition target 与原始 Pointer 事件；宿主将按下、释放、移动显式交给 WebView2 composition controller，并由 controller 管理浏览器焦点。这样避免 child HWND 覆盖 XAML Island 后造成的输入与关闭控件遮挡。

本设计不下载、安装、打包或指定浏览器 Runtime。environment 的 `browserExecutableFolder` 与 user-data-folder 均传入 `nullptr`，因此使用系统默认 Runtime 和 WebView2 默认用户数据目录。Runtime 更新入口在使用文档中统一指向 WebView2 官方页面。

## 正式场景

下列三处必须使用同一宿主实现 `WorkspaceNativeHwndWebViewHost`：

| 场景 | 入口 | 所有权 |
| --- | --- | --- |
| 单个网页节点命令 | `Tab::SetTerminalContentWebView` | 节点 `Tab` |
| 多命令节点 Tab 内的网页 | `WorkspaceCommandTabHost` | 节点 `Tab`，每个网页命令一个实例 |
| 工作区管理页 | `WorkspaceManagerContentGlue` | `TerminalPage` |

Demo 对照不属于正式路径；正式入口和 composition demo 使用相同的 WebView2 COM controller 类型，但不共享 demo 状态、数据目录或页面逻辑。

## 宿主边界

```text
TerminalPage / Tab
  ├─ XAML：终端 Pane、TabView、节点图标及网页矩形的布局占位
  └─ WorkspaceNativeHwndWebViewHost
       ├─ ICoreWebView2Environment
       ├─ ICoreWebView2CompositionController
       ├─ ICoreWebView2Controller
       ├─ ICoreWebView2
       └─ composition root visual
```

XAML `Grid` 不包含 WebView2 控件。它持有 composition target；宿主将其 Pointer 事件转换为 WebView2 鼠标输入，并通过 `MoveFocus` 把焦点交给 controller。

宿主在 `Loaded`、`Unloaded` 和 `SizeChanged` 时同步 controller 可见性与 bounds；切换命令 Tab 只调用 `Show`/`Hide`，不得销毁或重新创建浏览器。节点 Tab 关闭时才 `Close` controller 并解除 visual target。

右上、右下图标栏属于 XAML。浏览器 visual 与它们处于同一 composition 树，XAML Z-order 可正常生效；内容仍在右侧预留图标栏宽度，图标栏在上/下边缘额外留出一个图标高度。

## 浏览器生命周期与日志

宿主统一记录下列事件，日志不记录 cookie、请求正文或网页输入：

- environment、composition controller、controller、core 创建失败的 HRESULT；
- 导航开始与完成（URL、成功状态、Web 错误码）；
- renderer/process failure（失败类型）；
- 鼠标按下、焦点变化、显示、隐藏、关闭与尺寸变化的宿主状态。

工作区管理页在切换到其他导航项目时关闭其实例。节点网页实例由 `Tab::Shutdown` 关闭；非活动网页命令仅隐藏，保留状态供再次切换。

## 约束与验收

- 正式目录不得出现 `Microsoft::UI::Xaml::Controls::WebView2` 或 `EnsureCoreWebView2Async`。
- 任何正式浏览器创建都必须经过 `WorkspaceNativeHwndWebViewHost`，并使用 `CreateCoreWebView2CompositionController`。
- 单网页命令、多命令网页及工作区管理页均能创建、显示、隐藏和销毁 controller。
- 在右上/右下图标布局中，网页不遮挡节点命令图标。
- 浏览器进程异常只写诊断日志；不得使 Terminal 主进程退出。

## 与 Demo 的区别

| 模式 | controller | 网页是否受 XAML visual/input 管理 | 用途 |
| --- | --- | --- | --- |
| XAML WebView2 Demo | XAML `WebView2` | 是 | 对照 |
| composition Demo | `CoreWebView2CompositionController` | 是 | 对照 |
| 正式宿主 | `CoreWebView2CompositionController` | 浏览器 visual 与输入由原生 COM controller 管理 | 生产路径 |
