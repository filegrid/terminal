# codev: VS Code Web with Docker

This guide uses `codev` as an example script and command name; you may rename it. It starts code-server in Docker for the current project. Project files remain on the host; each project has its own container, while extensions are stored in a shared Docker volume.

## 1. Build the Docker image

The `Dockerfile` in the codev project needs `code-server.tar.gz` in its build context. Building through codev downloads that archive automatically:

```bash
codev rebuild
```

Manual build example:

```bash
VERSION=4.136.2
mkdir -p build-context
cp Dockerfile build-context/
curl -fL -o build-context/code-server.tar.gz \
  "https://github.com/coder/code-server/releases/download/v${VERSION}/code-server-${VERSION}-linux-amd64.tar.gz"
docker build -t codev:latest build-context
```

The current Dockerfile uses a locally available `redis:7.4` (Debian 12) image as its base image. Make sure that image exists before a manual build.

## 2. Start a Docker container manually

Run this from the project root. The current directory is mounted at `/home/coder/project`:

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

Open `http://127.0.0.1:8080` in a browser. To remove this manually created example container:

```bash
docker rm -f codev-demo
```

## 3. Manage the current directory with `codev`

Save the complete example below as `codev` (the filename may be changed), keep it alongside `Dockerfile`, then run `chmod +x codev`:

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

Its essential operation is mounting the directory from which it was run as the project directory:

```bash
-v "$(pwd -P):/home/coder/project"
```

The script derives a container name from the absolute project path, picks a free port starting at 8080, and mounts extensions from a shared volume. Keep the codev project's `codev` script and `Dockerfile` together, then add the script to `PATH`:

```bash
mkdir -p ~/.local/bin
ln -sfn "$(pwd -P)/codev" ~/.local/bin/codev
```

From any project root, run:

```bash
codev          # Downloads/builds on first use, then starts this project
codev stop     # Removes this project's container
codev list     # Shows projects, URLs, and ports
codev rebuild  # Rebuilds the image
```

Running `codev` again in the same directory reuses its running container. Multiple projects use automatically selected ports such as 8080, 8081, and 8082; use the URL printed by the command.

## 4. Configure the codev URL in GeekTerminal

1. Run `codev` and copy its complete printed address, for example `http://127.0.0.1:8080`.
2. Open **Workspace Management** from the top-right drop-down menu, then open the target node.
3. Add a WebView window under **Command windows** and paste the address into its URL field.
4. Save the workspace, then reopen it from the workspace selector at the top left.

codev then appears as a window of that node alongside code terminals and debugging pages. It has no password by default, so use it only locally or on a trusted LAN.
