# Windows Terminal WebSocket 终端镜像方案
> 核心原则：**原终端逻辑一行不改，只在 IO 入口做旁路镜像；WebSocket、终端渲染、PTY 管理全部复用成熟开源组件，零自研底层能力**

## 一、整体架构与复用清单
### 复用组件（无需自研）
1. **终端核心**：完全复用 Windows Terminal 原生 `ConptyConnection`，包含 ConPTY 创建、管道读写、窗口 Resize、信号处理全链路
2. **WebSocket 服务**：复用 `websocketpp`（C++ 头文件库，与 Terminal 原生 C++ 栈无缝对接，协议层零开发）
3. **网页终端渲染**：复用 `xterm.js` 官方完整实现，含 VT 序列解析、键盘输入、自适应大小
4. **构建体系**：复用 Terminal 现有 CMake 工程，仅新增依赖引用

### 工作原理
在 `ConptyConnection` 的输入输出管道处新增旁路：
- PTY 输出 → 原 Terminal UI 正常显示 + 旁路广播给所有 WebSocket 客户端
- 网页输入 → WebSocket 接收 → 直接调用原生 `WriteInput` 注入 PTY
- 两边共享同一个 ConPTY 会话，双向完全同步，原生终端的所有功能（PSReadLine、TUI、Ctrl+C）100% 保留

## 二、第一步：引入 WebSocket 依赖
选用 `websocketpp`（header‑only，无编译负担，MIT 协议），完全复用 Terminal 现有 CMake 构建体系：
1. 将 `websocketpp` 源码放入 `dep/websocketpp` 目录
2. 在 `src/cascadia/TerminalConnection/CMakeLists.txt` 中新增头文件路径：

```cmake
include_directories(${CMAKE_SOURCE_DIR}/dep/websocketpp)
```

> 依赖 ASIO，Terminal 工程已内置 WIL + Windows 异步 IO 环境，无需额外引入 Boost。

## 三、第二步：修改 Terminal 源码（仅新增，不改原逻辑）
### 1. 修改 `ConptyConnection.h`
新增成员变量与方法，原有成员完全保留：

```cpp
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <set>

typedef websocketpp::server<websocketpp::config::asio> ws_server;
typedef websocketpp::connection_hdl ws_conn_hdl;

class ConptyConnection : public ITerminalConnection
{
    // ... 原有所有成员保持不变 ...

private:
    // 新增：WebSocket 服务与客户端管理
    std::unique_ptr<ws_server> _wsServer;
    std::set<ws_conn_hdl, std::owner_less<ws_conn_hdl>> _wsClients;
    std::mutex _wsMutex;
    uint16_t _wsPort = 8080; // 可配置到 settings.json

    void _StartWebSocketServer();
    void _BroadcastToWeb(const std::string& data);
    void _OnWebMessage(ws_conn_hdl hdl, ws_server::message_ptr msg);
};
```

### 2. 修改 `ConptyConnection.cpp`
#### （1）在 `Start()` 方法末尾，PTY 启动成功后新增一行
```cpp
// 原有逻辑：启动 PTY、创建读写线程 ...
// 新增：启动 WebSocket 镜像服务
_StartWebSocketServer();
```

#### （2）在 `_readOutput` 读取回调中，新增旁路广播
原有输出到 Terminal UI 的逻辑完全不动，只加一行分流：
```cpp
// 原有逻辑：把 data 发给终端渲染层 ...
// 新增：同步广播到网页端
_BroadcastToWeb(std::string(data.begin(), data.end()));
```

#### （3）实现 WebSocket 核心方法
```cpp
void ConptyConnection::_StartWebSocketServer()
{
    _wsServer = std::make_unique<ws_server>();
    _wsServer->init_asio();
    _wsServer->set_reuse_addr(true);
    
    // 新客户端连接
    _wsServer->set_open_handler([this](ws_conn_hdl hdl) {
        std::lock_guard<std::mutex> lock(_wsMutex);
        _wsClients.insert(hdl);
    });
    
    // 客户端断开
    _wsServer->set_close_handler([this](ws_conn_hdl hdl) {
        std::lock_guard<std::mutex> lock(_wsMutex);
        _wsClients.erase(hdl);
    });
    
    // 收到网页输入，直接注入 PTY
    _wsServer->set_message_handler([this](ws_conn_hdl hdl, ws_server::message_ptr msg) {
        std::string input = msg->get_payload();
        // 复用原生 WriteInput 方法，零自研输入逻辑
        this->WriteInput(input);
    });
    
    _wsServer->listen(_wsPort);
    _wsServer->start_accept();
    
    // 后台线程跑 IO 事件循环，不阻塞终端 UI
    std::thread([this]() { _wsServer->run(); }).detach();
}

void ConptyConnection::_BroadcastToWeb(const std::string& data)
{
    std::lock_guard<std::mutex> lock(_wsMutex);
    for (auto& hdl : _wsClients) {
        _wsServer->send(hdl, data, websocketpp::frame::opcode::binary);
    }
}
```

#### （4）析构函数中补充资源释放
```cpp
ConptyConnection::~ConptyConnection()
{
    // 原有逻辑：关闭 PTY、清理管道 ...
    // 新增：停止 WebSocket 服务
    if (_wsServer) {
        _wsServer->stop();
    }
}
```

### 3. 可选：窗口大小同步
新增 WebSocket 接收 resize 指令的逻辑，调用原生 `Resize` 方法同步 PTY 尺寸，避免网页终端排版错乱：
```cpp
// 网页发送 {"type":"resize","cols":120,"rows":30}
// 解析后直接调用：
this->Resize(cols, rows);
```

## 四、第三步：网页前端（完全复用 xterm.js，单文件零部署）
直接复用 xterm.js 官方 CDN 资源，保存为单个 HTML 文件，浏览器打开即用，无需构建。

```html
<!DOCTYPE html>
<html>
<head>
  <link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/xterm@5.3.0/css/xterm.css">
  <script src="https://cdn.jsdelivr.net/npm/xterm@5.3.0/lib/xterm.js"></script>
  <script src="https://cdn.jsdelivr.net/npm/xterm-addon-fit@0.8.0/lib/xterm-addon-fit.js"></script>
</head>
<body style="margin:0;height:100vh">
  <div id="terminal" style="height:100%"></div>
  <script>
    const term = new Terminal();
    const fitAddon = new FitAddon.FitAddon();
    term.loadAddon(fitAddon);
    term.open(document.getElementById('terminal'));
    fitAddon.fit();

    const ws = new WebSocket('ws://127.0.0.1:8080');
    ws.binaryType = 'arraybuffer';

    // 接收 PTY 输出，渲染到网页
    ws.onmessage = e => term.write(new Uint8Array(e.data));
    
    // 网页键盘输入，发给 PTY
    term.onData(data => ws.send(data));
    
    // 窗口大小变化同步
    window.onresize = () => {
      fitAddon.fit();
      ws.send(JSON.stringify({
        type: 'resize',
        cols: term.cols,
        rows: term.rows
      }));
    };
  </script>
</body>
</html>
```

## 五、编译与使用
1. 按官方文档编译 Windows Terminal 源码
2. 启动编译后的 Terminal，打开任意 PowerShell / Cmd / WSL 标签页
3. 浏览器打开上述 HTML 文件，自动连接对应标签的终端会话
4. 两边输入输出完全同步，原生终端的所有交互能力完整保留

## 六、可选增强（全部复用现有能力）
1. **鉴权**：复用 websocketpp 内置的 HTTP 握手校验，加 Token 或账号密码，禁止裸奔公网
2. **多标签区分**：每个标签分配独立端口/路径，复用 Terminal 标签 ID 管理
3. **断线重连**：前端加重连逻辑，后端保留最近 N 行输出缓存，重连后自动补发
4. **配置化**：把端口、开关写入 Terminal 的 `settings.json`，复用原生配置体系

> 该方案侵入性极低，所有原生终端逻辑完全保留，仅新增约 100 行代码即可实现完整的双向 Web 镜像。
