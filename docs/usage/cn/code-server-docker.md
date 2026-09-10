# codev：Docker 版 VS Code Web

本文以 `codev` 作为示例脚本和命令名；它可以改成任意名称。它用 Docker 为当前项目启动 code-server。项目文件保留在宿主机；每个项目有独立容器，扩展由共享 Docker 卷保存。

## 1. 构建 Docker 镜像

codev 项目中的 `Dockerfile` 需要与 `code-server.tar.gz` 一起作为构建上下文。通过 codev 构建时会自动下载该压缩包：

```bash
codev rebuild
```

手动构建示例：

```bash
VERSION=4.136.2
mkdir -p build-context
cp Dockerfile build-context/
curl -fL -o build-context/code-server.tar.gz \
  "https://github.com/coder/code-server/releases/download/v${VERSION}/code-server-${VERSION}-linux-amd64.tar.gz"
docker build -t codev:latest build-context
```

当前 Dockerfile 使用本机已有的 `redis:7.4`（Debian 12）作为基础镜像；手动构建前需确保该镜像存在。

## 2. 手动启动一个 Docker 容器

在项目根目录执行。当前目录会挂载到 `/home/coder/project`：

```bash
docker volume create codev-extensions
docker run -d --name codev-demo --restart unless-stopped \
  -p 127.0.0.1:8080:8080 \
  -v "$(pwd -P):/home/coder/project" \
  -v codev-extensions:/home/coder/.local/share/code-server/extensions \
  codev:latest \
  --auth none --bind-addr 0.0.0.0:8080 --disable-telemetry \
  --ignore-last-opened /home/coder/project
```

浏览器地址是 `http://127.0.0.1:8080`。清理这个手动创建的容器：

```bash
docker rm -f codev-demo
```

## 3. 使用 `codev` 管理当前目录

将以下完整脚本保存为 `codev`（文件名可改），与 `Dockerfile` 放在同一目录，再执行 `chmod +x codev`：

```bash
#!/usr/bin/env bash
set -euo pipefail

IMAGE="${CODEV_IMAGE:-codev:latest}"
VOLUME="${CODEV_EXTENSIONS_VOLUME:-codev-extensions}"
DATA_DIR="${CODEV_DATA_DIR:-${XDG_DATA_HOME:-$HOME/.local/share}/codev}"
BUILD_DIR="$DATA_DIR/image"
VERSION="${CODEV_CODE_SERVER_VERSION:-4.136.2}"
PROJECT="$(pwd -P)"
KEY="$(printf '%s' "$PROJECT" | sha256sum | cut -c1-12)"
NAME="codev-$KEY"
START_PORT="${CODEV_PORT:-8080}"
SCRIPT_DIR="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"

prepare_build() {
  mkdir -p "$BUILD_DIR"
  cp "$SCRIPT_DIR/Dockerfile" "$BUILD_DIR/Dockerfile"
  if [[ ! -s "$BUILD_DIR/code-server.tar.gz" ]]; then
    curl -fL --retry 3 -o "$BUILD_DIR/code-server.tar.gz" \
      "https://github.com/coder/code-server/releases/download/v${VERSION}/code-server-${VERSION}-linux-amd64.tar.gz"
  fi
}

build_image() {
  prepare_build
  docker build -t "$IMAGE" "$BUILD_DIR"
}

pick_port() {
  local port
  for port in $(seq "$START_PORT" $((START_PORT + 200))); do
    if ! ss -ltn 2>/dev/null | grep -qE ":${port}[[:space:]]"; then
      echo "$port"
      return
    fi
  done
  echo "No free port" >&2
  exit 1
}

start() {
  docker image inspect "$IMAGE" >/dev/null 2>&1 || build_image
  docker volume inspect "$VOLUME" >/dev/null 2>&1 || docker volume create "$VOLUME" >/dev/null
  if docker ps --format '{{.Names}}' | grep -Fxq "$NAME"; then
    echo "http://127.0.0.1:$(docker port "$NAME" 8080/tcp | sed 's/.*://')"
    return
  fi
  docker rm -f "$NAME" >/dev/null 2>&1 || true
  local port
  port="$(pick_port)"
  docker run -d --name "$NAME" --label workspace-web=1 \
    -p "127.0.0.1:${port}:8080" \
    -v "${PROJECT}:/home/coder/project" \
    -v "${VOLUME}:/home/coder/.local/share/code-server/extensions" \
    "$IMAGE" --auth none --bind-addr 0.0.0.0:8080 \
    --disable-telemetry --ignore-last-opened /home/coder/project >/dev/null
  echo "http://127.0.0.1:${port}"
}

case "${1:-start}" in
  start) start ;;
  stop) docker rm -f "$NAME" ;;
  list) docker ps -a --filter label=workspace-web=1 \
          --format 'table {{.Names}}\t{{.Status}}\t{{.Ports}}' ;;
  rebuild) docker image rm "$IMAGE" 2>/dev/null || true; build_image ;;
  *) echo "Usage: ${0##*/} [start|stop|list|rebuild]" >&2; exit 2 ;;
esac
```

它的关键挂载操作是：

```bash
-v "$(pwd -P):/home/coder/project"
```

脚本以项目绝对路径生成容器名、自动选择从 8080 起的空闲端口，并将扩展挂载到共享卷。将 codev 项目的 `codev` 脚本和 `Dockerfile` 放在同一目录后，可把脚本链接进 PATH：

```bash
mkdir -p ~/.local/bin
ln -sfn "$(pwd -P)/codev" ~/.local/bin/codev
```

在任意项目根目录执行：

```bash
codev          # 首次会下载并构建，然后启动当前项目
codev stop     # 删除当前项目的容器
codev list     # 查看项目、URL 和端口
codev rebuild  # 重建镜像
```

同一目录再次执行 `codev` 会复用运行中的容器。多项目时端口会自动变为 8080、8081、8082 等，以命令实际输出的 URL 为准。

## 4. 在极客终端配置 codev URL

1. 运行 `codev`，复制输出的完整地址，例如 `http://127.0.0.1:8080`。
2. 从顶栏右侧下拉菜单打开“工作区管理”，进入目标节点。
3. 在“命令窗口”中添加 WebView 窗口，并把该地址填入 URL 字段。
4. 保存工作区；从顶栏左侧工作区选择器重新打开工作区。

codev 会作为该节点的一个窗口出现，可与代码终端、调试页面一起使用。默认没有登录密码，仅限本机或可信内网。
