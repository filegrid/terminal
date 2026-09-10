# 工作区节点命令子 Tab 使用说明

适用于 0.0.4 及以后版本。

一个工作区节点可包含多个命令窗口，但一级标签栏仍只显示该节点。选中节点后，可在内容区的第二级 Tab 或右侧图标之间切换命令窗口；相关的终端、服务和 Web 工具因此不会占满顶层标签栏。

## 前提

- 使用可编辑、未锁定的工作区；锁定的工作区需先解锁并进入编辑模式。
- 节点已选择可用的终端 profile。终端类型命令窗口复用此 profile、节点启动目录和标题策略。
- WebView 窗口使用系统已安装的默认 WebView Runtime；程序不会下载、安装、打包或指定其版本和路径。若网页无法正常显示，请从[官方页面](https://developer.microsoft.com/en-us/microsoft-edge/webview2/)安装或更新运行时，然后重新启动程序。

## 配置命令子 Tab

1. 从左上角的工作区名称进入管理页，选择工作区和节点，再进入编辑模式。
2. 在“命令窗口”区域中，点击 `+` 添加终端命令窗口，填写名称和启动命令。启动命令可留空，此时仅按节点 profile 启动终端。
3. 点击地球图标添加 WebView 窗口，填写名称和完整 URL，例如 `http://127.0.0.1:8080`。
4. 点击每项左侧图标可设置子 Tab 图标；拖动列表项可调整顺序。
5. 有两个或以上命令窗口时，在“多窗口展示”中选择“Tab”。添加 WebView 时会自动选择 Tab 模式；WebView 不支持左右分隔模式。
6. 选择子 Tab 位置：左上（图标加文字）、右上（竖排图标）或右下（底部竖排图标）。保存后重新打开工作区以创建新会话。

每个节点最多 5 个命令窗口，且至少保留 1 个。关闭一个一级节点 Tab 会关闭其中全部命令窗口；切换子 Tab 不会重启已创建的终端会话。

## codev 示例

`/mnt/d/my/codev/` 中的 `codev` 会以 Docker 启动 code-server，把当前项目映射到浏览器编辑器。环境需要 Linux 或 WSL2、正在运行的 Docker daemon、`curl` 和 `bash`。建议将命令软链接到源码目录，让脚本可找到同目录的 `Dockerfile`：

```bash
mkdir -p ~/.local/bin
ln -sfn /mnt/d/my/codev/codev ~/.local/bin/codev
export PATH="$HOME/.local/bin:$PATH"
codev --help
```

首次运行会下载 code-server（默认 `4.136.2`）、构建 `codev:latest`，并创建共享插件卷 `codev-extensions`。也可预先执行：

```bash
codev rebuild
```

在项目目录启动编辑器：

```bash
cd /path/to/your-project
codev
```

它会输出地址，例如 `http://127.0.0.1:8080`。在同一个工作区节点中建议设置如下命令窗口，并将展示方式设为“Tab”：

| 顺序 | 名称 | 类型 | 命令或 URL |
| --- | --- | --- | --- |
| 1 | 开发终端 | 终端 | 留空，或 `wsl` / 项目启动命令 |
| 2 | VS Code Web | WebView | `http://127.0.0.1:8080` |
| 3 | Dev Server | 终端 | `npm run dev` |

多个项目会依次使用 `8080`、`8081` 等端口；为每个节点填写 `codev` 实际输出的 URL。使用以下命令管理实例：

```bash
cd /path/to/your-project
codev stop       # 删除当前项目的容器，不删除项目文件或共享插件
codev list       # 查看已记录实例
codev rebuild    # 重建镜像；之后需 stop 再 codev 才会使用新镜像
```

`codev` 默认以 `--auth none` 绑定 `0.0.0.0`，只应在本机或可信网络使用。在 WSL2 中，Windows 端 WebView 应优先使用 `http://127.0.0.1:端口`。WebView 只显示网页，不会自动启动 `codev`；请先运行 `codev`。
